"""Lane net_dl_voice: client downloads (download / nextdl / stopdl / donedl / retransdl, svc_download blocks, the
wwwDownload redirect and refusal paths) and voice (sv_voice / cl_voice OOB "v" packets and their relay).

Cases (see analysis/gameplay/net_dl_voice.md):
  net_dl_dl_mp_toujane     sv_allowDownload 1: complete / aborted / bad-ack downloads of a generated mod iwd
  net_dl_www_mp_toujane    sv_wwwDownload 1 + sv_allowDownload 0: redirects, fallback, refusal and wwwdl errors
  net_dl_voice_tdm_toujane sv_voice 1: voice packets in the active (local) and the pre-active (relay) paths

Players tagged "E"/"F" are *connecting-state* clients: they join, receive the gamestate and never send a usermove, so
the server keeps them in CS_PRIMED, exactly the state a real client is in while it downloads. Their requests are
fired from player A's snapshot times (server time), and their block acks are generated while the server message is
parsed, so every decision is still a function of server time.
"""
import hashlib
import struct
import types
import zipfile
import os
import re
import zlib

import gameplay as G
import cod2proto as P
import fakeclient as F

OOB = F.OOB
MODNAME = "dl"
IWDNAME = "zz_dl"
IWD_REL = "mods/%s/%s.iwd" % (MODNAME, IWDNAME)


# --------------------------------------------------------------------------------------------------
# the generated mod iwd (deterministic bytes; never committed, built under the per-run home from game data)
# --------------------------------------------------------------------------------------------------
def build_iwd_bytes(game_iwd=None):
    """A stored (uncompressed) ZIP with fixed timestamps: a text file, a pseudo-random 20000-byte blob (a multiple of
    nothing, so the last block is partial) and a copy of one small stock text asset when the game data is available."""
    import io
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_STORED) as z:
        def add(name, data):
            zi = zipfile.ZipInfo(name, date_time=(2005, 1, 1, 0, 0, 0))
            zi.compress_type = zipfile.ZIP_STORED
            zi.external_attr = 0o644 << 16
            z.writestr(zi, data)
        add("dl/readme.txt", b"open-games net_dl_voice lane download test iwd\n")
        blob = bytearray()
        h = b"net_dl_voice"
        while len(blob) < 20000:
            h = hashlib.sha256(h).digest()
            blob += h
        add("dl/blob.bin", bytes(blob[:20000]))
    return buf.getvalue()


def make_mod(home, data):
    d = os.path.join(home, "mods", MODNAME)
    os.makedirs(d, exist_ok=True)
    raw = build_iwd_bytes()
    with open(os.path.join(d, IWDNAME + ".iwd"), "wb") as fh:
        fh.write(raw)
    return "net_dl_voice overlay %s sha256 %s size %d" % (IWD_REL, hashlib.sha256(raw).hexdigest(), len(raw))


# --------------------------------------------------------------------------------------------------
# client message parsing with svc_download (op 5), a copy of fakeclient.Client.parse_server_message
# --------------------------------------------------------------------------------------------------
def parse_server_message(self, seq, payload, nfrag, dropped):
    info = {"seq": seq, "fragments": nfrag, "dropped_before": dropped, "payload_len": len(payload)}
    if len(payload) < 4:
        info["error"] = "short"
        self.messages.append(info)
        return info
    import struct as _s
    ack_cmd = _s.unpack_from("<i", payload, 0)[0]
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
                    if m_ and new:
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
                self.nl_download(msg, ops, info)
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


class NetScen(G.Duel):
    """Common machinery: svc_download decoding with scripted block acks, OOB capture, connecting-state clients."""

    PRIMED = ()      # tags of connecting-state clients (never send a usermove)
    VOICE_USERS = ()  # tags that switch cl_voice on through a userinfo command

    def __init__(self, t0, players, summary):
        G.Duel.__init__(self, t0, players, summary)
        self.by = {p.tag: p for p in players}
        self.oob_log = {p.tag: [] for p in players}
        self.notes["oob"] = {}
        for p in players:
            p.dl_log = []       # one string per decoded svc_download record
            p.dl_next = 0       # next expected block number (what a real client acknowledges)
            p.dl_ack_max = 0    # auto-ack blocks below this number (set by the script per download)
            p.dl_auto_done = False  # send "donedl" after the terminating (empty) block
            p.dl_crc = 0
            p.dl_bytes = 0
            p.dl_done_cmds = 0
            c = p.c
            c.nl_download = types.MethodType(self._make_nl_download(p), c)
            c.parse_server_message = types.MethodType(parse_server_message, c)
            if p.tag in self.PRIMED:
                c.send_packet = self._no_usermove(c.send_packet)
                c.parse_gamestate = types.MethodType(self._hide_iwds(c.parse_gamestate), c)
            self._wrap_log(p)
            self.notes["dl_" + p.tag] = p.dl_log

    # ----- helpers -----
    @staticmethod
    def _no_usermove(orig):
        def send_packet(usercmds=0, note="", cmd=None, resend=False):
            return orig(0, note=note)
        return send_packet

    @staticmethod
    def _hide_iwds(orig):
        """gameplay.send_cp reads every referenced iwd's central directory in pure Python (0.1-0.3 s of client CPU per
        gamestate); one Python thread serves all fake clients, so that stall delays the active clients' usercmds and
        shifts their commandTime by wall-clock-dependent amounts. The connecting-state clients therefore answer with a
        `cp` that lists no iwds: the working copy of the gamestate (c.gamestate) lacks sv_referencedIwdNames, while the
        recorded gamestates (c.gamestates, the summary) keep the server's bytes."""
        def parse_gamestate(self, seq, msg, info):
            orig(seq, msg, info)
            gs = self.gamestate
            cs = dict(gs["configstrings"])
            cs[1] = re.sub(r"\\sv_referencedIwdNames\\[^\\]*", lambda m_: "\\sv_referencedIwdNames\\", cs.get(1, ""))
            self.gamestate = dict(gs, configstrings=cs)
        return parse_gamestate

    def _wrap_log(self, p):
        orig = p.log.event

        def event(kind, **kw):
            if kind == "oob_inband":
                txt = kw.get("text", "")
                raw = txt.encode("latin1")
                self.oob_log[p.tag].append({"hex": raw.hex(), "t": (p.c.latest_server_time or 0) - self.t0})
                self.notes["oob"][p.tag] = self.oob_log[p.tag]
            return orig(kind, **kw)
        p.log.event = event

    def _make_nl_download(self, p):
        def nl_download(client, msg, ops, info):
            blk = msg.read_short()
            now = ("t%+d" % (client.latest_server_time - self.t0)) if client.latest_server_time else "t?"
            if blk == -1:
                url = msg.read_string_raw(0x400).decode("latin1")
                size = msg.read_long()
                flag = msg.read_long()
                p.dl_log.append("www redirect %s size %d disconnected %d" % (url, size, flag))
                ops.append("download-www")
                return
            if blk == 0:
                size = msg.read_long()
                if size == -1:
                    txt = msg.read_string_raw(0x400).decode("latin1")
                    p.dl_log.append("denied %r" % txt)
                    ops.append("download-denied")
                    p.dl_next = 0
                    return
                p.dl_total = size
            n = msg.read_short()
            data = bytes(msg.data[msg.readcount:msg.readcount + n]) if n > 0 else b""
            msg.readcount += max(n, 0)
            line = "blk %d len %d crc %08x%s" % (blk, n, zlib.crc32(data) & 0xFFFFFFFF,
                                                  (" total %d" % size) if blk == 0 else "")
            ops.append("download%d" % blk)
            if blk == p.dl_next and blk < p.dl_ack_max:
                p.dl_crc = zlib.crc32(data, p.dl_crc)
                p.dl_bytes += n
                client.add_command("nextdl %d" % blk)
                p.dl_next = blk + 1
                line += " ack"
                if n == 0:
                    line += " end bytes %d crc %08x" % (p.dl_bytes, p.dl_crc & 0xFFFFFFFF)
                    p.dl_next, p.dl_bytes, p.dl_crc = 0, 0, 0
                    p.dl_ack_max = 0
                    if p.dl_auto_done:
                        client.add_command("donedl")
                        line += " +donedl"
            else:
                line += " ignored"
            p.dl_log.append(line)
        return nl_download

    def ui(self, p, **over):
        """The connect userinfo with overrides, as a `userinfo` client command."""
        kv = [("cg_predictItems", "1"), ("cl_anonymous", "0"), ("cl_punkbuster", "0"), ("cl_voice", "0"),
              ("cl_wwwDownload", "0"), ("rate", "25000"), ("snaps", "20"), ("name", p.name),
              ("protocol", "118"), ("challenge", "0"), ("qport", str(p.c.qport))]  # constant: the connect challenge is a libc rand() value
        kv = [(k, str(over.get(k, v))) for k, v in kv]
        return 'userinfo "%s"' % "".join("\\%s\\%s" % x for x in kv)

    def menus(self, p, t):
        gs = p.c.gamestate
        out = []
        sid = G.full_server_id(gs)
        # staggered by 200 ms of server time per player: the order in which the server sees the clients' menu
        # responses (and so the spawn choices) must not depend on which socket Python reads first
        off = 200 * self.players.index(p)

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                out.append(fn())
        if t >= off:
            once("team", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1000 + off:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))
        return out

    def idle_usercmd(self, p, t):
        p.cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
                 "offhand": 0}
        return p.cmd

    usercmd2 = idle_usercmd

    def usercmd(self, p, t):
        cmd = self.idle_usercmd(p, t)
        # a little movement so the snapshots carry changing playerstate/entities
        if p is self.A and 1500 <= t < 2300:
            cmd["forwardmove"] = 127
        if p is self.B and 1500 <= t < 2300:
            cmd["rightmove"] = 127
        return cmd

    def fire(self, p, t, plan, tag="p"):
        """plan: [(time, fn)] fired once each by player p's own snapshot time (tag keeps several plans apart)."""
        out = []
        for i, (at, fn) in enumerate(plan):
            if t >= at and ("%s%d" % (tag, i)) not in p.done:
                p.done.add("%s%d" % (tag, i))
                r = fn()
                if r:
                    out.extend(r if isinstance(r, list) else [r])
        return out

    def req(self, p, name, ack_max, done=False):
        """A new download request, as a real client starts one: next expected block 0, acks below ack_max."""
        p.dl_next, p.dl_bytes, p.dl_crc = 0, 0, 0
        p.dl_ack_max = ack_max
        p.dl_auto_done = done
        p.dl_log.append("request %s" % name)
        return "download " + name

    def send_now(self, q, *cmds):
        """Queue commands on connecting-state client q and flush them immediately (a packet at this server time)."""
        for c in cmds:
            q.c.add_command(c)
        q.c.send_packet(0, note="primed cmds")

    def actions2(self, p, t):
        return []

    def rcon(self, cmd):
        self.A.c.send_raw(OOB + ("rcon %s %s" % (F.RCON, cmd)).encode(), "oob")


# ==================================================================================================
# case 1: sv_allowDownload 1
# ==================================================================================================
class DlAllow(NetScen):
    PRIMED = ("E",)
    NAME = IWD_REL

    def actions(self, p, t):
        out = self.menus(p, t)
        E = self.by["E"]
        N = self.NAME
        if p is self.A:
            out += self.fire(p, t, [
                (3000, lambda: [self.req(p, N, 99)]),                        # complete download while active
                (8000, lambda: [self.req(p, N, 2)]),                         # acks blocks 0 and 1, then stalls
                (8600, lambda: ["retransdl 2", "retransdl 5"]),              # retransmit request (matching / not)
                (9500, lambda: ["stopdl"]),                                  # abort
                (10500, lambda: ["stopdl"]),                                 # stopdl with nothing running
                (10800, lambda: [self.ui(p, rate=90000)]),                   # more blocks per snapshot
                (11000, lambda: [self.req(p, N, 99)]),                       # restart: blocks from 0 again
                (16000, lambda: [self.req(p, "mods/%s/nothere.iwd" % MODNAME, 99)]),   # file not found
                (17000, lambda: [self.req(p, "main/iw_00.iwd", 0)]),         # stock IW iwd, never acknowledged
                (18000, lambda: ["download " + "x" * 90]),                    # name longer than the 64-byte field
                (18500, lambda: ["download"]),                                # no argument
                (33000, lambda: ["score"]),
            ])
            # connecting-state client: complete download, donedl -> second gamestate, refused names, ...
            self.fire(p, t, tag="e", plan=[
                (4000, lambda: self.send_now(E, self.req(E, N, 99, done=True))),
                (9000, lambda: self.send_now(E, self.req(E, "mods/%s/missing.iwd" % MODNAME, 99))),
                (10000, lambda: self.send_now(E, self.req(E, "main/iw_01.iwd", 0))),
                (12000, lambda: self.send_now(E, self.req(E, N, 99))),
                (15000, lambda: self.rcon("set sv_maxRate 500")),            # clamped to 1000 by the download writer
                (15500, lambda: self.send_now(E, self.ui(E, rate=2000))),
                (20000, lambda: self.send_now(E, self.req(E, N, 4))),        # stalls at block 4; server retransmits
                (24500, lambda: self.send_now(E, "stopdl")),
                (25000, lambda: self.send_now(E, "userinfo")),               # malformed userinfo (no argument)
                (26000, lambda: self.send_now(E, self.req(E, N, 99), "retransdl 0")),
                (27500, lambda: self.send_now(E, self.req(E, "../main/iw_05.iwd", 0))),
                (28000, lambda: self.send_now(E, self.req(E, "mods/%s/../%s/%s.iwd" % (MODNAME, MODNAME, IWDNAME), 99))),
                (30000, lambda: self.send_now(E, "nextdl 7")),               # wrong block: broken download
            ])
        else:  # B
            out += self.fire(p, t, [
                (6000, lambda: [self.req(p, N, 3)]),
                (14000, lambda: ["nextdl 7"]),    # not the expected block: "broken download"
            ])
        return out


# ==================================================================================================
# case 2: wwwDownload redirect and refusal
# ==================================================================================================
class DlWww(NetScen):
    PRIMED = ("E", "F")
    NAME = IWD_REL

    def actions(self, p, t):
        out = self.menus(p, t)
        E, F_ = self.by["E"], self.by["F"]
        N = self.NAME
        if p is self.A:
            out += self.fire(p, t, [
                # A is not www capable: sv_allowDownload 0 refuses the plain download
                (3000, lambda: ["download " + N]),
                (4000, lambda: ["download mods/%s/%s.iwd" % (MODNAME, "nothere")]),
                (30000, lambda: ["score"]),
            ])

            # E: www redirect, dupe ack, checksum failure -> gamestate resend -> fallback refused -> redirect again
            def e_ui():
                self.send_now(E, self.ui(E, cl_wwwDownload=1))

            def e_dl():
                self.send_now(E, "download " + N)
            self.fire(p, t, tag="e", plan=[
                (5000, e_ui),
                (5200, lambda: self.send_now(E, "download mods/%s/missing.iwd" % MODNAME)),   # redirect: no file size
                (5500, e_dl),
                (6000, lambda: self.send_now(E, "wwwdl ack")),
                (6500, lambda: self.send_now(E, "wwwdl ack")),             # duplicate ack warning
                (7000, lambda: self.send_now(E, "wwwdl chkfail")),         # -> wwwFallback + SV_SendClientGameState
                (8000, e_dl),                                               # www skipped once, plain path refuses
                (9000, e_dl),                                               # redirected again
                (9500, lambda: self.send_now(E, "wwwdl ack")),
                (10000, lambda: self.send_now(E, "wwwdl fail")),           # fail -> fallback + gamestate
                (11000, lambda: self.send_now(E, "wwwdl done")),           # not downloading any more: dropped
            ])

            # F: redirect, ack, done, redirect again, unknown subcommand
            def f_ui():
                self.send_now(F_, self.ui(F_, cl_wwwDownload=1))

            def f_dl():
                self.send_now(F_, "download " + N)
            self.fire(p, t, tag="f", plan=[
                (12000, f_ui),
                (12500, f_dl),
                (13000, lambda: self.send_now(F_, "wwwdl ack")),
                (13500, lambda: self.send_now(F_, "wwwdl done")),
                (14000, f_dl),
                (14500, lambda: self.send_now(F_, "wwwdl ack")),
                (15000, lambda: self.send_now(F_, "wwwdl nonsense")),      # unknown subcommand: dropped
            ])
        else:  # B: wwwdl without any redirect, then bbl8r
            out += self.fire(p, t, [
                (17000, lambda: ["wwwdl ack"]),    # unexpected: dropped
            ])
        return out


# ==================================================================================================
# case 3: voice
# ==================================================================================================
def voice_packets(sizes, seed, short=False):
    """Body of a voice packet list: count byte, then per packet <len: byte|short><data>."""
    out = bytearray([len(sizes)])
    for i, n in enumerate(sizes):
        out += struct.pack("<H", n) if short else bytes([n & 0xFF])
        out += bytes(((seed * 31 + i * 17 + k * 7) & 0xFF) for k in range(max(n, 0) if n < 0x400 else 0))
    return bytes(out)


class Voice(NetScen):
    PRIMED = ("E",)
    VOICE_USERS = ("A", "B", "C", "E")   # D keeps cl_voice 0

    def vsend(self, q, sizes, seed, short=False):
        pkt = OOB + b"v\0" + struct.pack("<H", q.c.qport) + voice_packets(sizes, seed, short)
        q.c.send_raw(pkt, "voice")
        q.log.event("voice_sent", sizes=list(sizes), seed=seed)

    def actions(self, p, t):
        out = self.menus(p, t)
        A, B, C, D, E = (self.by[k] for k in "ABCDE")
        if p.tag in self.VOICE_USERS and p.tag != "E":
            out += self.fire(p, t, [(1500, lambda: [self.ui(p, cl_voice=1)])])
        if p is A:
            self.fire(p, t, tag="v", plan=[
                (1500, lambda: self.send_now(E, self.ui(E, cl_voice=1))),
                (3000, lambda: self.vsend(A, [20, 33], 1)),                 # team mate C hears it
                (4000, lambda: self.vsend(B, [17], 2)),                     # D (cl_voice 0) does not
                (5000, lambda: self.rcon("set voice_global 1")),
                (6000, lambda: self.vsend(A, [25], 3)),                     # now everybody with cl_voice 1
                (7000, lambda: self.rcon("set voice_localEcho 1")),
                (8000, lambda: self.vsend(A, [12], 4)),                     # A hears itself
                (9000, lambda: self.rcon("set voice_localEcho 0")),
                (11000, lambda: self.vsend(C, [9, 9, 9], 5)),               # three packets in one datagram
                (13000, lambda: self.vsend(A, [0], 6)),                     # invalid size 0
                (13500, lambda: self.vsend(A, [255, 4], 7)),                # max byte length
                (14500, lambda: self.rcon("set voice_global 0")),
                (15500, lambda: self.vsend(A, [30], 8)),                    # back to team only
                (22000, lambda: self.rcon("set voice_deadChat 1")),
                (23000, lambda: self.vsend(A, [14], 9)),                    # dead talker, deadChat on
                (24000, lambda: self.rcon("set voice_deadChat 0")),
                (25000, lambda: self.vsend(A, [14], 10)),                   # dead talker, deadChat off
                (26500, lambda: self.vsend(C, [11], 11)),                   # living talker, A dead
                # connecting-state talker: relay path with 16-bit lengths to every client with cl_voice 1
                (28000, lambda: self.vsend(E, [21, 22], 12, short=True)),
                (29000, lambda: self.vsend(E, [0], 13, short=True)),        # invalid
                (29500, lambda: self.vsend(E, [300], 14, short=True)),      # invalid (> 0x100)
                (30500, lambda: self.vsend(E, [256], 15, short=True)),      # boundary
                (36000, lambda: self.vsend(C, [5], 16)),
                (36500, lambda: self.send_now(E, self.ui(E, cl_voice=1, rate=2000))),
                (37000, lambda: self.rcon("set sv_voice 0")),               # voice off: packets ignored
                (37500, lambda: self.vsend(A, [8], 18)),
                (38000, lambda: self.rcon("set sv_voice 1")),
                (38500, lambda: self.vsend(A, [8], 19)),
            ])
            out += self.fire(p, t, [
                (9500, lambda: ["muteplayer %d" % C.c.client_num]),         # A mutes C (A's own list)
                (10500, lambda: ["muteplayer 99"]),                         # out of range
                (12000, lambda: ["unmuteplayer %d" % C.c.client_num]),
                (16500, lambda: ["muteplayer"]),                            # missing argument
                (20000, lambda: ["kill"]),
                (33000, lambda: ["score"]),
            ])
        elif p is B:
            out += self.fire(p, t, [
                (18000, lambda: ["muteplayer %d" % A.c.client_num]),        # B mutes A (global voice off again)
                (19000, lambda: []),
                (31000, lambda: ["unmuteplayer %d" % A.c.client_num]),
                (34000, lambda: self.vsend(B, [10], 17) or []),
            ])
        elif p is C:
            out += self.fire(p, t, [(34500, lambda: [self.ui(p, cl_voice=1, rate=4000)])])  # rate < 5000: no voice
        return out


# ==================================================================================================
TOUJANE_DM = ["+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
              "+set", "fs_game", "mods/%s" % MODNAME]

CASES = {
    "net_dl_dl_mp_toujane": {
        "args": TOUJANE_DM + ["+set", "sv_allowDownload", "1", "+set", "sv_wwwDownload", "0",
                              "+devmap", "mp_toujane"],
        "scenario": DlAllow,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("E", "", "")],
        "mod": make_mod, "t0": 20000, "length": 36000,
    },
    "net_dl_www_mp_toujane": {
        "args": TOUJANE_DM + ["+set", "sv_allowDownload", "0", "+set", "sv_wwwDownload", "1",
                              "+set", "sv_wwwBaseURL", "dl.example.invalid/cod2",   # "//" would start a comment
                              "+set", "sv_wwwDlDisconnected", "1", "+devmap", "mp_toujane"],
        "scenario": DlWww,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("E", "", ""), ("F", "", "")],
        "mod": make_mod, "t0": 20000, "length": 34000,
    },
    "net_dl_voice_tdm_toujane": {
        "args": ["+set", "g_gametype", "tdm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
                 "+set", "sv_voice", "1", "+set", "sv_maxclients", "64", "+devmap", "mp_toujane"],
        "scenario": Voice,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("C", "allies", "thompson_mp"),
                    ("D", "axis", "mp40_mp"), ("E", "", "")],
        "t0": 20000, "length": 40000,
    },
}
