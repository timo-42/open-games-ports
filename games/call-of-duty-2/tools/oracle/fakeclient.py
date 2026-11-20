#!/usr/bin/env python3
"""Scripted CoD2 1.3 (protocol 118) fake client: connection-level oracle for the Linux dedicated server.

  fakeclient.py run <case> [run-index] [--server original|recon] [--seconds S] [-- extra server args]
        Launch the server AND this client inside one `bwrap --unshare-net` namespace (loopback only;
        never exposes a server to the LAN), capture into
          original: .local/cod2/oracle/captures/client_<case>/run<N>/
          recon:    .local/cod2/recon-run/captures/client_<case>/run<N>/
        cases: dm_toujane | sd_carentan | lanauth_dm_toujane (dm_toujane + net_lanauthorize 1)
  fakeclient.py session --port P --out DIR --case NAME --server-log FILE [--seconds S] [--rcon PW]
        (inner step, runs inside the namespace) the scripted session itself.
  fakeclient.py compare CASE_DIR [OTHER_CASE_DIR] [--json OUT]
        Same CLI as probe.py compare. One dir: classify every summary item across run*/ as
        identical / variable / missing. Two dirs: additionally compare OTHER (e.g. the reconstruction)
        against the items that are deterministic in CASE_DIR (the original).

Session script (see analysis/oracle-client.md): getchallenge -> connect with wrong protocol -> connect with
wrong challenge -> getchallenge -> connect (userinfo incl. protocol 118) -> in-band netchan: receive
gamestate (configstrings, baselines), send "cp" pure checksums + usercmds, receive snapshots (playerState /
entity / clientState deltas decoded), rcon status while connected, clientCommand "say hi" -> disconnect ->
rcon status -> rcon quit. All server bytes are data; every datagram is logged raw (packets.jsonl).
"""
import argparse
import glob
import hashlib
import json
import os
import re
import select
import socket
import struct
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import cod2proto as P  # noqa: E402

OOB = b"\xff\xff\xff\xff"
CLIENT_PORT = 28971
QPORT = 0x1D2B  # fixed (a real client randomises it): removes qport variability from the server's view
NAME = "fakeclient"
RCON = "oracle-test-pw"


# =============================================================================================
# capture helpers
# =============================================================================================
class Log:
    def __init__(self, out):
        self.t0 = time.time()
        self.pk = open(os.path.join(out, "packets.jsonl"), "w")
        self.ev = open(os.path.join(out, "events.jsonl"), "w")
        self.dec = open(os.path.join(out, "decoded.jsonl"), "w")  # per server message: 4 raw bytes + Huffman-decoded body
        self.n = 0

    def decoded(self, seq, body):
        self.dec.write(json.dumps({"seq": seq, "t": self.t(), "len": len(body), "hex": body.hex()}) + "\n")
        self.dec.flush()

    def t(self):
        return round(time.time() - self.t0, 4)

    def packet(self, direction, data, note=""):
        self.n += 1
        self.pk.write(json.dumps({"n": self.n, "t": self.t(), "dir": direction, "len": len(data),
                                  "note": note, "hex": data.hex()}) + "\n")
        self.pk.flush()

    def event(self, kind, **kw):
        kw = dict(kind=kind, t=self.t(), **kw)
        self.ev.write(json.dumps(kw, sort_keys=True) + "\n")
        self.ev.flush()
        return kw


def oob_text(p):
    return p[4:].decode("latin1") if p[:4] == OOB else None


# =============================================================================================
# the client
# =============================================================================================
class Client:
    def __init__(self, port, out, log, client_port=CLIENT_PORT, qport=QPORT):
        self.addr = ("127.0.0.1", port)
        self.out, self.log = out, log
        self.qport = qport
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", client_port))
        self.huff = P.huffman()
        self.challenge = None
        # netchan
        self.out_seq = 1
        self.in_seq = 0
        self.frag_seq, self.frag_buf, self.frag_count = None, bytearray(), 0
        # protocol state
        self.connected = False
        self.server_id = 0
        self.gamestate = None
        self.gamestates = []
        self.checksum_feed = 0
        self.client_num = None
        self.server_cmds = {}  # seq -> bytes (as received)
        self.server_cmd_seq = 0  # highest received
        self.client_cmds = []  # (seq, text)
        self.client_cmd_seq = 0
        self.client_cmd_acked = 0  # leading long of server messages
        self.baselines = {}
        self.frames = {}  # server message seq -> snapshot dict (ps, ents, clients)
        self.last_msg_had_snapshot = False
        self.latest_server_time = None
        self.last_cmd_time = None
        self.snapshots = []
        self.messages = []
        self.oob_inband = []
        self.first_active_snap = None

    # ---------------- raw io ----------------
    def send_raw(self, data, note=""):
        self.log.packet("c2s", data, note)
        self.sock.sendto(data, self.addr)

    def oob(self, text, idle=0.6, total=4.0):
        """send OOB text, collect replies until idle. In-band packets that arrive meanwhile are processed."""
        payload = text.encode("latin1") if isinstance(text, str) else text
        self.send_raw(OOB + payload, "oob")
        got, start, last = [], time.time(), time.time()
        while time.time() - start < total:
            r, _, _ = select.select([self.sock], [], [], 0.05)
            if r:
                data, _ = self.sock.recvfrom(0x20000)
                self.log.packet("s2c", data)
                if data[:4] == OOB:
                    got.append(oob_text(data))
                    last = time.time()
                else:
                    info = self.handle_inband(data)
                    if info is not None:
                        self.log.event("server_message", **{k: v for k, v in info.items() if k != "snapshot"},
                                       snapshot=info.get("snapshot"))
            elif got and time.time() - last > idle:
                break
        return got

    # ---------------- netchan in ----------------
    def handle_inband(self, data):
        if len(data) < 4:
            return None
        seq = struct.unpack_from("<i", data, 0)[0] & 0xFFFFFFFF
        frag = bool(seq & 0x80000000)
        seq &= 0x7FFFFFFF
        pos = 4
        if frag:
            fstart, flen = struct.unpack_from("<ih", data, pos)
            pos += 6
        if seq <= self.in_seq:
            self.log.event("out_of_order", seq=seq, last=self.in_seq)
            return None
        if frag:
            if seq != self.frag_seq:
                self.frag_seq, self.frag_buf, self.frag_count = seq, bytearray(), 0
            if fstart != len(self.frag_buf):
                self.log.event("fragment_dropped", seq=seq, start=fstart, have=len(self.frag_buf))
                return None
            self.frag_buf += data[pos:pos + flen]
            self.frag_count += 1
            if flen == 1300:
                return None
            payload = bytes(self.frag_buf)
            nfrag = self.frag_count
            self.frag_buf, self.frag_count = bytearray(), 0
        else:
            payload = data[pos:]
            nfrag = 0
        dropped = seq - self.in_seq - 1
        self.in_seq = seq
        self.connected = True
        return self.parse_server_message(seq, payload, nfrag, dropped)

    def parse_server_message(self, seq, payload, nfrag, dropped):
        info = {"seq": seq, "fragments": nfrag, "dropped_before": dropped, "payload_len": len(payload)}
        if len(payload) < 4:
            info["error"] = "short"
            self.messages.append(info)
            return info
        ack_cmd = struct.unpack_from("<i", payload, 0)[0]
        key_string = b""
        for s, txt in self.client_cmds:
            if s == ack_cmd:
                key_string = txt.encode("latin1")
        dec = P.xor_stream(payload[4:], (seq ^ (self.challenge or 0)) & 0xFF, key_string)
        body, overrun = self.huff.decompress(dec)
        info.update(ack_client_cmd=ack_cmd, decompressed_len=len(body), huff_overrun_bits=overrun)
        self.client_cmd_acked = max(self.client_cmd_acked, ack_cmd)
        msg = P.Msg(payload[:4] + body)
        self.log.decoded(seq, payload[:4] + body)
        msg.read_long()
        ops = []
        had_snap = False
        try:
            while True:
                op = msg.read_byte()
                if op == 7:
                    ops.append("eof")
                    break
                if op == -1:
                    ops.append("eom")
                    break
                if op == 4:
                    cseq = msg.read_long()
                    text = msg.read_string_raw(0x400)
                    new = cseq > self.server_cmd_seq
                    if cseq not in self.server_cmds:
                        self.log.event("server_command", seq=cseq, text=text.decode("latin1"), msg=seq)
                        m_ = re.search(rb"\\sv_serverid\\(-?\d+)", text)
                        if m_ and new:  # systeminfo configstring update (e.g. map_restart)
                            self.server_id = int(m_.group(1)) & 0xFF
                            self.log.event("server_id_update", server_id=self.server_id, cmd=cseq)
                    self.server_cmds[cseq] = text
                    self.server_cmd_seq = max(self.server_cmd_seq, cseq)
                    ops.append("cmd%d%s" % (cseq, "" if new else "(dup)"))
                elif op == 1:
                    self.parse_gamestate(seq, msg, info)
                    ops.append("gamestate")
                elif op == 6:
                    self.parse_snapshot(seq, msg, info)
                    had_snap = True
                    ops.append("snapshot")
                elif op == 5:
                    ops.append("download")
                    info["error"] = "download op not decoded"
                    break
                else:
                    ops.append("bad%d" % op)
                    info["error"] = "unknown op %d at %d" % (op, msg.readcount)
                    break
        except Exception as e:  # decoding failure is data, not a crash
            info["error"] = "exception %r" % (e,)
        info["ops"] = ops
        info["overflowed"] = msg.overflowed
        info["unread_bytes"] = msg.cursize - msg.readcount
        self.last_msg_had_snapshot = had_snap and "error" not in info and not msg.overflowed
        self.messages.append(info)
        return info

    def parse_gamestate(self, seq, msg, info):
        rel_seq = msg.read_long()
        cs = {}
        baselines = {}
        while True:
            op = msg.read_byte()
            if op == 2:
                idx = msg.read_short()
                cs[idx] = msg.read_string_raw(0x2000).decode("latin1")
            elif op == 3:
                num = msg.read_bits(10)
                removed, st, cnt = P.read_delta_struct(msg, [0] * (P.ENTITY_SIZE // 4), num, P.ENTITY_FIELDS)
                if st is not None:
                    st[0] = num
                    baselines[num] = st
            elif op == 7:
                break
            else:
                raise ValueError("gamestate op %d" % op)
        self.client_num = msg.read_long()
        self.checksum_feed = msg.read_long() & 0xFFFFFFFF
        self.baselines = baselines
        self.server_cmd_seq = max(self.server_cmd_seq, rel_seq)
        sysinfo = cs.get(1, "")
        m = re.search(r"\\sv_serverid\\(-?\d+)", "\\" + sysinfo.lstrip("\\"))
        if m is None:
            for v in cs.values():
                m = re.search(r"\\sv_serverid\\(-?\d+)", v)
                if m:
                    break
        self.server_id = int(m.group(1)) & 0xFF if m else 0
        self.frames = {}  # snapshots of the previous gamestate are no delta base any more
        self.gamestate = {"msg_seq": seq, "reliableSequence": rel_seq, "configstrings": cs,
                          "baselines": {str(k): v for k, v in sorted(baselines.items())},
                          "clientNum": self.client_num, "checksumFeed": P.s32(self.checksum_feed),
                          "serverId": self.server_id, "fragments": info["fragments"],
                          "payload_len": info["payload_len"]}
        self.gamestates.append(self.gamestate)
        self.log.event("gamestate", msg=seq, reliableSequence=rel_seq, configstrings=len(cs),
                       baselines=len(baselines), clientNum=self.client_num,
                       checksumFeed=P.s32(self.checksum_feed), serverId=self.server_id)

    def parse_snapshot(self, seq, msg, info):
        start_bits = msg.readcount
        server_time = msg.read_long()
        delta_num = msg.read_byte()
        flags = msg.read_byte()
        old = None
        if delta_num:
            old = self.frames.get(seq - delta_num)
        ps, ps_changed = P.read_delta_playerstate(msg, old["ps"] if old else None)
        # entities
        old_ents = sorted(old["ents"].items()) if old else []
        ents, oi = {}, 0
        n_ent_msgs = n_removed = 0
        while True:
            num = msg.read_bits(10)
            if num == 0x3FF or num < 0:
                break
            while oi < len(old_ents) and old_ents[oi][0] < num:
                ents[old_ents[oi][0]] = old_ents[oi][1]
                oi += 1
            if oi < len(old_ents) and old_ents[oi][0] == num:
                frm = old_ents[oi][1]
                oi += 1
            else:
                frm = self.baselines.get(num) or ([num] + [0] * (P.ENTITY_SIZE // 4 - 1))
            removed, st, cnt = P.read_delta_struct(msg, frm, num, P.ENTITY_FIELDS)
            n_ent_msgs += 1
            if removed:
                n_removed += 1
            else:
                st[0] = num
                ents[num] = st
        while oi < len(old_ents):
            ents[old_ents[oi][0]] = old_ents[oi][1]
            oi += 1
        # clients
        old_cl = sorted(old["clients"].items()) if old else []
        clients, oi = {}, 0
        n_cl_msgs = 0
        while True:
            more = msg.read_bit()
            if more != 1:
                break
            num = msg.read_bits(6)
            while oi < len(old_cl) and old_cl[oi][0] < num:
                clients[old_cl[oi][0]] = old_cl[oi][1]
                oi += 1
            if oi < len(old_cl) and old_cl[oi][0] == num:
                frm = old_cl[oi][1]
                oi += 1
            else:
                frm = [0] * (P.CLIENT_SIZE // 4)
            removed, st, cnt = P.read_delta_struct(msg, frm, num, P.CLIENT_FIELDS)
            n_cl_msgs += 1
            if not removed:
                st[0] = num
                clients[num] = st
        while oi < len(old_cl):
            clients[old_cl[oi][0]] = old_cl[oi][1]
            oi += 1
        valid = (delta_num == 0 or old is not None) and not msg.overflowed
        self.frames[seq] = {"ps": ps, "ents": ents, "clients": clients}
        for k in [k for k in self.frames if k < seq - 40]:
            del self.frames[k]
        self.latest_server_time = server_time
        pnamed = P.named_fields(ps, P.PLAYER_FIELDS)
        snap = {"msg": seq, "serverTime": server_time, "deltaNum": delta_num, "snapFlags": flags,
                "valid": valid, "ps_changed_fields": ps_changed, "entity_deltas": n_ent_msgs,
                "entities_removed": n_removed, "num_entities": len(ents), "client_deltas": n_cl_msgs,
                "num_clients": len(clients), "snapshot_bytes": msg.readcount - start_bits,
                "ps_hash": P.state_hash(ps),
                # same hash with commandTime made relative to the snapshot's serverTime (connect-time independent)
                "ps_hash_rel": P.state_hash([ps[0] - server_time & 0xFFFFFFFF] + ps[1:]),
                "ents_hash": P.state_hash([w for _, st in sorted(ents.items()) for w in st]),
                "clients_hash": P.state_hash([w for _, st in sorted(clients.items()) for w in st]),
                "entity_numbers": sorted(ents), "client_numbers": sorted(clients),
                "ps": {k: pnamed[k] for k in ("commandTime", "pm_type", "clientNum", "origin[0]", "origin[1]",
                                              "origin[2]", "viewangles[0]", "viewangles[1]", "weapon",
                                              "pm_flags", "eFlags", "viewHeightCurrent") if k in pnamed},
                "wire_len": info.get("payload_len")}
        if self.first_active_snap is None and not (flags & 2):
            self.first_active_snap = seq
            snap["first_active"] = True
            snap["ps_full"] = pnamed
            snap["entities_full"] = {str(n): P.named_fields(st, P.ENTITY_FIELDS) for n, st in sorted(ents.items())}
            snap["clients_full"] = {str(n): P.named_fields(st, P.CLIENT_FIELDS) for n, st in sorted(clients.items())}
        self.snapshots.append(snap)
        info["snapshot"] = {k: snap[k] for k in ("serverTime", "deltaNum", "snapFlags", "num_entities", "valid")}

    # ---------------- netchan out ----------------
    def add_command(self, text):
        self.client_cmd_seq += 1
        self.client_cmds.append((self.client_cmd_seq, text))
        self.log.event("client_command_queued", seq=self.client_cmd_seq, text=text)

    def send_packet(self, usercmds=0, note="", cmd=None, resend=False):
        """Build [seq][qport][serverId][msgAck][relAck] XOR(Huff(clientCommands + usermove|EOF)).
        cmd: optional explicit usercmd (dict for cod2proto.write_usercmd_full, serverTime filled in here), sent as
        the only usercmd of the packet. resend: reuse the last usercmd serverTime when no newer snapshot arrived (the
        server ignores a usercmd that is not newer than cl->lastUsercmd, so a keep-alive does not shift commandTime)."""
        m = P.Msg()
        for s, txt in self.client_cmds:
            if s > self.client_cmd_acked:
                m.write_bits(2, 3)
                m.write_long(s)
                m.write_string(txt)
        msg_ack = self.in_seq
        rel_ack = self.server_cmd_seq
        ack_text = self.server_cmds.get(rel_ack, b"") if rel_ack > 0 else b""
        times = []
        if usercmds:
            key = (self.checksum_feed ^ msg_ack ^ P.com_hashkey(ack_text, 32)) & 0xFFFFFFFF
            delta = self.last_msg_had_snapshot
            m.write_bits(0 if delta else 1, 3)
            m.write_byte(usercmds)
            base = self.latest_server_time or 0
            prev = None
            for i in range(usercmds):
                if resend and self.last_cmd_time is not None and self.last_cmd_time >= base:
                    t = self.last_cmd_time
                else:
                    t = max(base, (self.last_cmd_time or 0) + 1)
                self.last_cmd_time = t
                if cmd is not None:
                    P.write_usercmd_full(m, key, dict(cmd, serverTime=t))
                else:
                    P.write_usercmd_nochange(m, key, t, prev)
                prev = t
                times.append(t)
        else:
            m.write_bits(3, 3)
        body = self.huff.compress(m.bytes())
        key = (self.challenge ^ self.server_id ^ msg_ack) & 0xFF
        enc = P.xor_stream(body, key, ack_text)
        pkt = struct.pack("<iHBii", self.out_seq, self.qport, self.server_id, msg_ack, rel_ack) + enc
        self.out_seq += 1
        self.send_raw(pkt, note or ("usermove x%d" % usercmds if usercmds else "eof"))
        return {"msgAck": msg_ack, "relAck": rel_ack, "cmd_times": times}


# =============================================================================================
# session script
# =============================================================================================
def wait_ready(c, limit=120, pid=None):
    t0 = time.time()
    while time.time() - t0 < limit:
        if pid:
            try:
                os.kill(pid, 0)
            except OSError:
                return None  # server process is gone
        r = c.oob("getinfo ready", idle=0.3, total=1.0)
        if r and r[0].startswith("infoResponse"):
            return time.time() - t0
        time.sleep(0.5)
    return None


def userinfo(challenge, protocol=118):
    # [I] key set modelled on a CoD2 1.3 client userinfo; the server reads protocol/challenge/qport/name/
    # rate/snaps/cl_voice/cl_wwwDownload/cl_punkbuster/password (SV_DirectConnect, SV_UserinfoChanged).
    kv = [("cg_predictItems", "1"), ("cl_anonymous", "0"), ("cl_punkbuster", "0"), ("cl_voice", "0"),
          ("cl_wwwDownload", "0"), ("rate", "25000"), ("snaps", "20"), ("name", NAME),
          ("protocol", str(protocol)), ("challenge", str(challenge)), ("qport", str(QPORT))]
    return "".join("\\%s\\%s" % kv_ for kv_ in kv)


def session(a):
    os.makedirs(a.out, exist_ok=True)
    log = Log(a.out)
    c = Client(a.port, a.out, log)
    summary = {"case": a.case, "huffman_code_sha256": c.huff.fingerprint, "steps": {}}
    st = summary["steps"]
    ready = wait_ready(c, pid=a.server_pid)
    if ready is None:
        print("server never answered getinfo", file=sys.stderr)
        summary["result"] = "server never answered getinfo"
        json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=1)
        return 2
    summary["ready_after_s"] = round(ready, 2)
    time.sleep(1.5)
    gap = 1.1

    def step(name, text, **kw):
        time.sleep(gap)
        r = c.oob(text, **kw)
        log.event("oob", name=name, request=text, replies=r)
        st[name] = r
        return r

    r = step("getchallenge", "getchallenge")
    m = re.match(r"challengeResponse (-?\d+)", r[0]) if r else None
    if not m:
        # LAN authorisation enabled (net_lanauthorize 1) or non-LAN client: the server forwards to the
        # (unreachable) authorize server. SV_RequestIpAuthorize answers anyway once svs.time - lastAuthReply >
        # 1200000 ms and the challenge slot is > 7000 ms old; --auth-wait polls getchallenge to observe that.
        summary["result"] = "no challengeResponse"
        polls = []
        t_first = time.time()
        while time.time() - t_first < a.auth_wait:
            time.sleep(30.0)
            r = c.oob("getchallenge", idle=0.3, total=2.0)
            polls.append({"after_s": round(time.time() - t_first, 1), "replies": r})
            log.event("oob", name="getchallenge_poll", replies=r)
            m = re.match(r"challengeResponse (-?\d+)", r[0]) if r else None
            if m:
                break
        st["getchallenge_polls"] = polls
        if not m:
            json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=1, sort_keys=True)
            c.oob("rcon %s quit" % a.rcon, total=1.0)
            return 0
        summary["result"] = "challengeResponse after %.1f s of polling" % polls[-1]["after_s"]
    c.challenge = int(m.group(1))
    step("connect_wrong_protocol", 'connect "%s"' % userinfo(c.challenge, 117))
    step("connect_bad_challenge", 'connect "%s"' % userinfo(c.challenge ^ 0x5A5A, 118))
    r = step("getchallenge_again", "getchallenge")
    m2 = re.match(r"challengeResponse (-?\d+)", r[0]) if r else None
    summary["challenge_stable_within_run"] = bool(m2 and int(m2.group(1)) == c.challenge)
    time.sleep(gap)
    # ---- real connect ----
    log.event("connect", userinfo=userinfo(c.challenge))
    c.send_raw(OOB + ('connect "%s"' % userinfo(c.challenge)).encode(), "connect")
    t_connect = time.time()
    oob_after_connect = []
    phase = "connecting"
    scenario = "restart" if a.case.startswith("restart_") else "basic"
    summary["scenario"] = scenario
    summary["pace"] = a.pace
    # pace "snapshot": answer every in-band server message at once (usercmd serverTime = the snapshot's), so the
    # command always reaches the server well inside the next 50 ms frame and commandTime = serverTime - 50 does
    # not depend on the phase between the client's and the server's clocks. A 100 ms timer is the fallback.
    # pace "timer": free-running 50 ms send timer (the first captures; commandTime then depends on the phase).
    period = 0.1 if a.pace == "snapshot" else 0.05
    t_gamestate = None
    t_active = []  # one entry per activation (several with map_restart)
    gs_seen = 0
    pending_rcon = None  # name of the step whose rcon reply (OOB print) is outstanding
    said = disc_sent = False
    t_disc = t_restart = None
    rcon_status_sent = set()
    next_send = time.time() + 0.05
    sends = []
    deadline = t_connect + a.seconds + 25 + (20 if scenario == "restart" else 0)

    def send_cp():
        # pure checksums: "cp @ <pure sums...> <feed ^ sums ^ n>" (SV_VerifyPaks_f 0x08090534). Like a real client we
        # send the pure checksums (MD4 keyed with checksumFeed) of the iwds the server lists as referenced, computed
        # from our own copy of those iwds (same files as the server's). Sent again after every gamestate.
        sysinfo = c.gamestate["configstrings"].get(1, "")
        names = re.search(r"\\sv_referencedIwdNames\\([^\\]*)", sysinfo)
        plain = re.search(r"\\sv_referencedIwds\\([^\\]*)", sysinfo)
        names = names.group(1).split() if names else []
        plain = [int(x) for x in plain.group(1).split()] if plain else []
        sums, iwdinfo = [], []
        for i_, nm in enumerate(names):
            path = os.path.join(a.basepath, nm + ".iwd")
            try:
                ck, pure = P.iwd_checksums(path, c.checksum_feed)
            except OSError as e:
                iwdinfo.append({"name": nm, "error": str(e)})
                continue
            iwdinfo.append({"name": nm, "checksum": ck, "matches_sv_referencedIwds": i_ < len(plain) and plain[i_] == ck})
            sums.append(pure)
        if "cp_iwds" not in summary:
            summary["cp_iwds"] = iwdinfo
            summary["cp_pak_count"] = len(sums)
        chk = c.checksum_feed
        for s_ in sums:
            chk ^= s_ & 0xFFFFFFFF
        chk ^= len(sums)
        c.add_command("cp @ " + "".join("%d " % s_ for s_ in sums) + "%d" % P.s32(chk))

    def rcon(name, cmd):
        nonlocal pending_rcon
        pending_rcon = name  # non-blocking: the reply is collected by the receive branch
        c.send_raw(OOB + ("rcon %s %s" % (a.rcon, cmd)).encode(), "oob")
        log.event("rcon_sent", name=name, cmd=cmd)

    while time.time() < deadline:
        r_, _, _ = select.select([c.sock], [], [], max(0.0, next_send - time.time()))
        if r_:
            data, _ = c.sock.recvfrom(0x20000)
            log.packet("s2c", data)
            if data[:4] == OOB:
                txt = oob_text(data)
                if txt.startswith("print\n") and pending_rcon:
                    st.setdefault(pending_rcon, []).append(txt)
                    log.event("oob", name=pending_rcon, replies=[txt])
                    continue
                oob_after_connect.append({"t": round(time.time() - t_connect, 3), "phase": phase, "text": txt})
                log.event("oob_inband_phase", phase=phase, text=txt)
                continue
            info = c.handle_inband(data)
            if info is None:
                continue
            log.event("server_message", **{k: v for k, v in info.items() if k != "snapshot"},
                      snapshot=info.get("snapshot"))
            if len(c.gamestates) > gs_seen and phase != "disconnected_wait":
                gs_seen = len(c.gamestates)
                if t_gamestate is None:
                    t_gamestate = time.time()
                phase = "primed"
                send_cp()
                sends.append(c.send_packet(usercmds=1, note="cp+enter"))
                next_send = time.time() + period
                continue
            if info.get("snapshot") and not (info["snapshot"]["snapFlags"] & 2) and phase == "primed":
                t_active.append(time.time())
                phase = "active"
            if a.pace == "snapshot" and phase in ("primed", "active"):
                next_send = time.time()
            continue
        # client frame
        now = time.time()
        next_send = now + (0.05 if phase == "connecting" else period)
        if phase == "connecting":
            if c.connected:
                sends.append(c.send_packet(0))
            if now - t_connect > 10:
                break
        elif phase in ("primed", "active"):
            if phase == "active":
                el = now - t_active[-1]
                act = len(t_active)
                if el > 1.0 and act not in rcon_status_sent:
                    rcon_status_sent.add(act)
                    rcon("rcon_status_connected" if act == 1 else "rcon_status_connected_%d" % act, "status")
                if el > 2.0 and not said:
                    said = True
                    c.add_command("say hi")
                if scenario == "restart" and act == 1 and el > 3.0 and t_restart is None:
                    t_restart = now
                    rcon("rcon_map_restart_connected", "map_restart")
                disconnect_due = (el > a.seconds) if scenario == "basic" else (act >= 2 and el > 3.0)
                if disconnect_due and not disc_sent:
                    disc_sent = True
                    c.add_command("disconnect")
                    for _ in range(3):
                        sends.append(c.send_packet(1, note="disconnect"))
                    t_disc = now
                    phase = "disconnected_wait"
                    continue
            if scenario == "restart" and t_restart is not None and now - t_restart > 20 and not disc_sent:
                log.event("restart_never_reactivated")
                break
            sends.append(c.send_packet(1))
        elif phase == "disconnected_wait":
            if now - t_disc > 2.0:
                break
    summary["phase_reached"] = phase
    summary["activations"] = len(t_active)
    summary["oob_after_connect"] = oob_after_connect
    summary["timing"] = {"gamestate_after_connect_s": round(t_gamestate - t_connect, 3) if t_gamestate else None,
                         "active_after_connect_s": [round(t - t_connect, 3) for t in t_active]}
    pending_rcon = None
    time.sleep(gap)
    st["rcon_status_after_disconnect"] = c.oob("rcon %s status" % a.rcon, idle=0.3, total=1.5)
    time.sleep(gap)
    st["rcon_quit"] = c.oob("rcon %s quit" % a.rcon, idle=0.5, total=2.0)
    # ---- summary ----
    summary["challenge"] = c.challenge
    summary["client_sends"] = len(sends)
    summary["gamestate"] = c.gamestates[0] if c.gamestates else None
    summary["gamestates_later"] = c.gamestates[1:]
    summary["server_commands"] = {str(k): v.decode("latin1") for k, v in sorted(c.server_cmds.items())}
    summary["client_commands"] = [{"seq": s, "text": t} for s, t in c.client_cmds]
    summary["client_cmd_acked_by_server"] = c.client_cmd_acked
    summary["messages"] = c.messages
    summary["snapshots"] = c.snapshots
    json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=1, sort_keys=True)
    print("phase=%s messages=%d snapshots=%d server_cmds=%d" % (phase, len(c.messages), len(c.snapshots),
                                                                len(c.server_cmds)))
    return 0


# =============================================================================================
# sandbox runner (mirrors run_original.sh / run_recon.sh: bwrap --unshare-net, loopback only)
# =============================================================================================
def run(a):
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.abspath(os.path.join(here, "../../../.."))
    cod2 = os.environ.get("COD2_LOCAL", os.path.join(repo, ".local/cod2"))
    oracle = os.path.join(cod2, "oracle")
    data = os.environ.get("COD2_DATA_MAIN", "")
    cases = {"dm_toujane": ["+set", "g_gametype", "dm", "+map", "mp_toujane"],
             "sd_carentan": ["+set", "g_gametype", "sd", "+map", "mp_carentan"],
             "lanauth_dm_toujane": ["+set", "net_lanauthorize", "1", "+set", "g_gametype", "dm", "+map", "mp_toujane"],
             # restart_*: rcon map_restart while the client is active, then a second activation (session())
             "restart_dm_toujane": ["+set", "g_gametype", "dm", "+map", "mp_toujane"],
             "restart_sd_carentan": ["+set", "g_gametype", "sd", "+map", "mp_carentan"]}
    if a.case not in cases:
        sys.exit("unknown case %s" % a.case)
    if a.server == "original":
        binp = os.path.join(cod2, "lnxded-1.3/cod2_lnxded")
        want = "05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7"
        if hashlib.sha256(open(binp, "rb").read()).hexdigest() != want:
            sys.exit("server binary hash mismatch")
        out = os.path.join(oracle, "captures", "client_" + a.case, "run%d" % a.run)
        ldpath = "export LD_LIBRARY_PATH=%s/runtime-i386/usr/lib/i386-linux-gnu" % cod2
    else:
        binp = os.environ.get("RECON_BIN", os.path.join(cod2, "build/cod2_lnxded_recon"))
        out = os.path.join(cod2, "recon-run", "captures", "client_" + a.case, "run%d" % a.run)
        ldpath = ":"
    if not os.access(binp, os.X_OK):
        sys.exit("missing server binary %s" % binp)
    if not data or not os.path.isdir(data):
        sys.exit("set COD2_DATA_MAIN to the original game's main directory")
    shim = os.path.join(oracle, "lib/stat32_shim.so")
    for d in ("lib", "run/base/main", "run/home"):
        os.makedirs(os.path.join(oracle, d), exist_ok=True)
    if not os.path.exists(shim) or os.path.getmtime(os.path.join(here, "stat32_shim.c")) > os.path.getmtime(shim):
        subprocess.check_call(["gcc", "-m32", "-shared", "-fPIC", "-O2", os.path.join(here, "stat32_shim.c"), "-o", shim])
    for f in glob.glob(os.path.join(data, "*.iwd")):
        dst = os.path.join(oracle, "run/base/main", os.path.basename(f))
        if not os.path.lexists(dst):
            os.symlink(f, dst)
    # same fs_homepath for both servers (as run_recon.sh): rcon map_restart echoes it in packets whose boundaries
    # drop one character each, so a path-length difference would shift the drops. Do not run the original and the
    # reconstruction concurrently with the same case and run index.
    home = os.path.join(oracle, "run/home", "client_%s-run%d" % (a.case, a.run))
    subprocess.call(["rm", "-rf", out, home])
    os.makedirs(out)
    os.makedirs(home)
    srv = ["+set", "dedicated", "1", "+set", "net_ip", "127.0.0.1", "+set", "net_port", "28960",
           "+set", "sv_maxclients", "8", "+set", "rcon_password", RCON, "+set", "sv_punkbuster", "0",
           "+set", "sv_pure", "1", "+set", "fs_basepath", os.path.join(oracle, "run/base"),
           "+set", "fs_homepath", home] + cases[a.case] + list(a.extra)
    q = lambda s: "'" + s.replace("'", "'\\''") + "'"  # noqa: E731
    inner = """#!/usr/bin/env bash
set -u
%(ld)s
cd %(bindir)s
{ echo "# /proc/net/dev"; tail -n +3 /proc/net/dev | cut -d: -f1 | tr -d ' '; echo "# routes"; tail -n +2 /proc/net/route; } > %(out)s/isolation.txt
mkfifo %(out)s/stdin.fifo
sleep %(hold)d > %(out)s/stdin.fifo &
KEEP=$!
LD_PRELOAD=%(shim)s %(bin)s %(args)s < %(out)s/stdin.fifo > %(out)s/server.log 2>&1 &
SRV=$!
python3 -I %(me)s session --port 28960 --out %(out)s --case %(case)s --server-log %(out)s/server.log --seconds %(secs)s --rcon %(rcon)s --basepath %(base)s --auth-wait %(aw)s --pace %(pace)s --server-pid $SRV > %(out)s/client.log 2>&1
RC=$?
for i in $(seq 1 50); do kill -0 $SRV 2>/dev/null || break; sleep 0.2; done
if kill -0 $SRV 2>/dev/null; then echo "server did not exit after rcon quit; killing" >> %(out)s/harness.log; kill -9 $SRV; fi
wait $SRV 2>/dev/null; echo "server_exit_status=$?" >> %(out)s/harness.log
kill $KEEP 2>/dev/null; rm -f %(out)s/stdin.fifo
exit $RC
""" % {"ld": ldpath, "bindir": q(os.path.dirname(os.path.join(cod2, "lnxded-1.3/cod2_lnxded"))), "out": q(out),
       "shim": q(shim), "bin": q(binp), "args": " ".join(q(x) for x in srv), "me": q(os.path.abspath(__file__)),
       "case": q(a.case), "secs": a.seconds, "base": q(os.path.join(oracle, "run/base")), "aw": a.auth_wait, "hold": int(900 + a.auth_wait), "rcon": RCON, "pace": a.pace}
    fd, path = tempfile.mkstemp(prefix="fc-inner.", suffix=".sh", dir=os.path.join(oracle, "run"))
    os.write(fd, inner.encode())
    os.close(fd)
    try:
        bw = ["bwrap", "--ro-bind", "/", "/", "--dev", "/dev", "--proc", "/proc",
              "--bind", os.path.join(oracle, "run"), os.path.join(oracle, "run"), "--bind", out, out,
              "--unshare-net", "--unshare-pid", "--unshare-ipc", "--die-with-parent", "bash", path]
        rc = subprocess.call(bw)
        if rc:
            open(os.path.join(out, "harness.log"), "a").write("inner exit status %d\n" % rc)
    finally:
        os.unlink(path)
    with open(os.path.join(out, "SHA256SUMS"), "w") as fh:
        for f in sorted(glob.glob(os.path.join(out, "*"))):
            if os.path.basename(f) != "SHA256SUMS" and os.path.isfile(f):
                fh.write("%s  %s\n" % (hashlib.sha256(open(f, "rb").read()).hexdigest(), os.path.basename(f)))
    print("done: %s" % out)
    print(open(os.path.join(out, "client.log")).read().strip())
    return 0


# =============================================================================================
# comparison
# =============================================================================================
def normalize_log_line(line, root):
    line = line.replace(root, "<ROOT>")
    line = re.sub(r"run\d+", "runN", line)
    line = re.sub(r"-(original|recon)-", "-<SERVER>-", line)
    # older captures named the home dir client_<case>-<server>-runN, current runs client_<case>-runN
    line = re.sub(r"(/run/home/client_[A-Za-z0-9_]+?)(-<SERVER>)?-runN", r"\1-<HOME>", line)
    # quantised to server frames and measured from the client's getchallenge: client timing (1700/1750)
    line = re.sub(r"connecting with \d+ challenge ping", "connecting with <N> challenge ping", line)
    return line


def items_for_run(d):
    """Flatten a run's summary + server.log into comparable items {key: value}."""
    s = json.load(open(os.path.join(d, "summary.json")))
    it = {}

    def put(k, v):
        it[k] = json.dumps(v, sort_keys=True) if not isinstance(v, str) else v

    ch = s.get("challenge")
    root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../.."))
    for k, v in sorted(s.get("steps", {}).items()):
        if k.startswith("rcon_map_restart"):
            # multi-packet re-initialisation text with fs_homepath: joined and normalised like server.log (the rcon
            # redirect drops one character per packet boundary, so the joined text is the comparable unit)
            joined = "".join(x[len("print\n"):] if x.startswith("print\n") else x for x in v)
            put("oob:%s:packets" % k, len(v))
            for i_, line in enumerate(joined.split("\n")):
                put("oob:%s:%03d" % (k, i_), normalize_log_line(line, root))
            continue
        txt = json.dumps(v)
        if ch is not None:
            txt = txt.replace(str(ch), "<CHALLENGE>")
        if k.startswith("rcon_status_connected"):  # ping column depends on client timing (%4i, padding kept)
            txt = re.sub(r"(\s+0\s+0\s+)\d+(\s+0 fakeclient)", r"\1<PING>\2", txt)
        put("oob:" + k, txt)
    for k in ("phase_reached", "challenge_stable_within_run", "cp_pak_count", "client_cmd_acked_by_server",
              "huffman_code_sha256", "challenge", "scenario", "pace", "activations"):
        if k in s:
            put(k, s[k])
    put("timing", s.get("timing"))
    put("oob_after_connect", [(o["phase"], o["text"]) for o in s.get("oob_after_connect", [])])
    gs = s.get("gamestate") or {}
    for k in ("reliableSequence", "clientNum", "checksumFeed", "serverId", "fragments", "payload_len"):
        put("gamestate:" + k, gs.get(k))
    for idx, v in sorted((gs.get("configstrings") or {}).items(), key=lambda kv: int(kv[0])):
        put("configstring:%04d" % int(idx), v)
    put("gamestate:configstring_indices", sorted(int(i) for i in (gs.get("configstrings") or {})))
    put("gamestate:baseline_numbers", sorted(int(i) for i in (gs.get("baselines") or {})))
    for n, words in (gs.get("baselines") or {}).items():
        put("baseline:%04d" % int(n), P.state_hash(words))
    for g, gl in enumerate(s.get("gamestates_later") or [], 2):
        for k in ("reliableSequence", "clientNum", "serverId", "fragments", "payload_len"):
            put("gamestate%d:%s" % (g, k), gl.get(k))
        for idx, v in sorted((gl.get("configstrings") or {}).items(), key=lambda kv: int(kv[0])):
            put("gamestate%d:configstring:%04d" % (g, int(idx)), v)
        put("gamestate%d:configstring_indices" % g, sorted(int(i) for i in (gl.get("configstrings") or {})))
        for n, words in (gl.get("baselines") or {}).items():
            put("gamestate%d:baseline:%04d" % (g, int(n)), P.state_hash(words))
    for k, v in (s.get("server_commands") or {}).items():
        put("server_command:%04d" % int(k), v)
    put("client_commands_norm", [re.sub(r"-?\d+", "<N>", c_["text"]) if c_["text"].startswith("cp ")
                                 else c_["text"] for c_ in s.get("client_commands", [])])
    put("message_count", len(s.get("messages", [])))
    put("snapshot_count", len(s.get("snapshots", [])))
    msgs = s.get("messages", [])
    ops = [m.get("ops") for m in msgs]
    while len(ops) > 1 and ops[0] == ["eof"] and ops[1] == ["eof"]:
        ops = ops[1:]  # number of empty CS_CONNECTED messages before the gamestate depends on timing
    put("messages:ops_sequence_norm", ops)
    structure = []
    for o in ops:  # consecutive identical messages collapsed: repeat counts follow client/server timing
        if not structure or structure[-1] != o:
            structure.append(o)
    put("messages:ops_structure", structure)
    put("messages:errors", [m.get("error") for m in msgs if m.get("error")])
    snaps = s.get("snapshots", [])
    for i, sn in enumerate(snaps):
        for k in ("serverTime", "deltaNum", "snapFlags", "valid", "ps_changed_fields", "num_entities",
                  "num_clients", "entity_deltas", "client_deltas", "ents_hash", "clients_hash",
                  "snapshot_bytes", "wire_len"):
            put("snap%03d:%s" % (i, k), sn.get(k))
        ps_rel = dict(sn.get("ps") or {})
        if "commandTime" in ps_rel:
            ps_rel["commandTime"] -= sn["serverTime"]
        put("snap%03d:ps_rel" % i, ps_rel)
        put("snap%03d:ps_hash_rel" % i, sn.get("ps_hash_rel"))
    if snaps:
        t0 = snaps[0]["serverTime"]
        put("snaps:serverTime_rel", [sn["serverTime"] - t0 for sn in snaps])
        put("snaps:serverTime_steps", sorted(set(b["serverTime"] - a_["serverTime"] for a_, b in zip(snaps, snaps[1:]))))
        put("snaps:num_entities_set", sorted(set(sn["num_entities"] for sn in snaps)))
        put("snaps:entity_numbers_first_active", next((sn["entity_numbers"] for sn in snaps if sn.get("first_active")), None))
        fa = next((sn for sn in snaps if sn.get("first_active")), None)
        if fa:
            for k, v in sorted(fa.get("ps_full", {}).items()):
                put("first_active_ps:" + k, v)
            for n, f in sorted(fa.get("entities_full", {}).items(), key=lambda kv: int(kv[0])):
                put("first_active_entity:%04d" % int(n), f)
            for n, f in sorted(fa.get("clients_full", {}).items(), key=lambda kv: int(kv[0])):
                put("first_active_client:%02d" % int(n), f)
    logf = os.path.join(d, "server.log")
    if os.path.exists(logf):
        lines = [normalize_log_line(l.rstrip("\n"), root) for l in open(logf, encoding="latin1")]
        lines = [l for l in lines if not l.startswith("Hitch warning")]
        for i, l in enumerate(lines):
            put("server_log:%04d" % i, l)
    return it


def classify(runs_items):
    keys = sorted(set().union(*[r.keys() for r in runs_items]))
    rep = {}
    for k in keys:
        vals = [r.get(k) for r in runs_items]
        if None in vals:
            rep[k] = {"status": "missing_in_some_runs", "values": [v[:80] if v else v for v in vals]}
        elif len(set(vals)) == 1:
            rep[k] = {"status": "identical", "value": vals[0][:200]}
        else:
            rep[k] = {"status": "variable", "values": [v[:80] for v in vals]}
    return rep


# Item families whose per-index values are timing-dependent in the original: once any member of the family
# varies across original runs, a different value at another index is timing noise, not a behaviour difference.
# wire_len = Huffman-compressed message length, which includes the absolute serverTime long (43 of 161
# dm_toujane indices vary across 3 original runs); gamestate payload_len includes the random checksumFeed.
# snaps:serverTime_steps across an rcon map_restart: the gap is the wall-clock restart duration rounded to 50 ms
# server frames (3 original runs: 1600; reconstruction 1550/1600 from 744-777 ms restarts), so it is timing in
# every *restart* case (analysis/gameplay-parity.md, "Remaining gaps").
TIMING_FAMILIES = (r"snap\d+:wire_len", r"gamestate\d*:payload_len", r"snaps:serverTime_steps")


def family_of(key):
    for pat in TIMING_FAMILIES:
        if re.fullmatch(pat, key):
            return pat
    return None


def compare(a):
    runs = sorted(glob.glob(os.path.join(a.case_dir, "run*")))
    data = [items_for_run(r) for r in runs if os.path.exists(os.path.join(r, "summary.json"))]
    rep = {"runs": [os.path.basename(r) for r in runs], "items": classify(data)}
    counts = {}
    for v in rep["items"].values():
        counts[v["status"]] = counts.get(v["status"], 0) + 1
    rep["counts"] = counts
    rc = 0
    if a.other:
        oruns = sorted(glob.glob(os.path.join(a.other, "run*")))
        # a run without summary.json (server crashed / never ready) still counts: all its items are missing
        odata = [items_for_run(r) if os.path.exists(os.path.join(r, "summary.json")) else {} for r in oruns]
        rep["other_runs"] = [os.path.basename(r) for r in oruns]
        rep["other_runs_without_summary"] = [os.path.basename(r) for r in oruns
                                             if not os.path.exists(os.path.join(r, "summary.json"))]
        varying = {family_of(k) for k, v in rep["items"].items() if v["status"] != "identical"} - {None}
        varying.add(r"gamestate\d*:payload_len")  # always: Huffman length of the random checksumFeed long
        if "restart" in os.path.basename(os.path.normpath(a.case_dir)):
            varying.add(r"snaps:serverTime_steps")  # map_restart hitch, see TIMING_FAMILIES
        rep["timing_families_excluded"] = sorted(varying)
        diff = {}
        for k, v in rep["items"].items():
            if v["status"] != "identical" or family_of(k) in varying:
                continue
            ovals = [o.get(k) for o in odata] or [None]
            bad = [ov for ov in ovals if ov != v["value"] and not (ov and len(v["value"]) == 200 and ov[:200] == v["value"])]
            if bad:
                diff[k] = {"original": v["value"][:120], "other": [ov[:120] if ov else ov for ov in ovals],
                           "missing": any(ov is None for ov in ovals)}
        # items the other server produced that never occur in the original (extra snapshots, log lines, ...)
        extra = sorted(set().union(*[o.keys() for o in odata]) - set(rep["items"])) if odata else []
        for k in extra:
            if family_of(k) not in varying:
                diff[k] = {"original": None, "other": [(o.get(k) or "")[:120] or None for o in odata], "missing": False}
        rep["other_dir"] = a.other
        rep["other_mismatches_on_deterministic_items"] = diff
        rc = 1 if diff else 0
    if a.json:
        json.dump(rep, open(a.json, "w"), indent=1, sort_keys=True)
    for k, v in rep["items"].items():
        if v["status"] != "identical":
            print("%-48s %s %s" % (k, v["status"], v.get("values")))
    print("counts:", counts)
    if a.other:
        d = rep["other_mismatches_on_deterministic_items"]
        print("other runs: %s (without summary.json: %s)" % (rep["other_runs"], rep["other_runs_without_summary"]))
        print("mismatches vs other on deterministic items: %d (missing in other: %d, extra in other: %d)" % (
            len(d), sum(1 for v in d.values() if v["missing"]), sum(1 for v in d.values() if v["original"] is None)))
        for k, v in list(d.items())[:60]:
            print("  %-46s orig=%r other=%r" % (k, (v["original"] or "")[:50], [x[:50] if x else x for x in v["other"]]))
    return rc


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    r = sp.add_parser("run")
    r.add_argument("case")
    r.add_argument("run", type=int, nargs="?", default=1)
    r.add_argument("--server", choices=("original", "recon"), default="original")
    r.add_argument("--seconds", type=float, default=6.0)
    r.add_argument("--auth-wait", type=float, default=0.0)
    r.add_argument("--pace", choices=("snapshot", "timer"), default="snapshot")
    r.add_argument("extra", nargs="*")
    r.set_defaults(fn=run)
    s = sp.add_parser("session")
    s.add_argument("--port", type=int, default=28960)
    s.add_argument("--out", required=True)
    s.add_argument("--case", required=True)
    s.add_argument("--server-log", required=True)
    s.add_argument("--seconds", type=float, default=6.0)
    s.add_argument("--rcon", default=RCON)
    s.add_argument("--server-pid", type=int, default=0)
    s.add_argument("--auth-wait", type=float, default=0.0, help="poll getchallenge this long if unanswered")
    s.add_argument("--pace", choices=("snapshot", "timer"), default="snapshot")
    s.add_argument("--basepath", required=True, help="fs_basepath of the server (iwds for pure checksums)")
    s.set_defaults(fn=session)
    c = sp.add_parser("compare")
    c.add_argument("case_dir")
    c.add_argument("other", nargs="?")
    c.add_argument("--json")
    c.set_defaults(fn=compare)
    a = ap.parse_args()
    sys.exit(a.fn(a))


if __name__ == "__main__":
    main()
