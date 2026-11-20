"""Lane net_loss: deterministic packet loss, duplication, reordering and silence on the netchan of a running dm game.

Case `netloss_play_dm_toujane` (A clean, B lossy, C goes silent; sv_timeout 3):
  * B sends every usercmd packet with cl_packetdup-like resends of its last two usercmds (original serverTimes).
  * B's client->server packets are dropped in single / burst patterns, and some are reordered (held back and sent after
    the next packet, so the server sees seq N+1 before N and discards N as out of order).
  * B's reliable client commands (`say`) are queued inside those windows, so they are retransmitted until the server's
    next message acks them.
  * B's server->client packets are dropped (single losses, then a 35-message outage > PACKET_BACKUP-3, so the server must
    fall back to a full snapshot) while A talks, so B's reliable server commands are retransmitted too.
  * C stops sending anything after rel 16000: the server drops it with EXE_TIMEDOUT after sv_timeout seconds.
Case `netloss_gamestate_dm_toujane` (A clean, B loses gamestate fragments during connect): the 7-fragment gamestate
loses a middle fragment, then (after the server retransmits it) its last fragment, then arrives.

Determinism: every drop / duplication decision depends only on the deciding client's own latest snapshot server time
or on the server's message sequence relative to the first message after a snapshot-time trigger. The harness's wall-clock
keep-alive (usercmds=1, resend=True) is swallowed for the lossy clients (it would race with the 100 ms gaps that loss
creates); it is not part of the observable server behaviour being compared.
"""
import struct

import gameplay as G


def swallow_keepalive(p):
    """Replace the wall-clock keep-alive of p's client by a no-op (instance level)."""
    orig = p.c.send_packet

    def send_packet(usercmds=0, note="", cmd=None, resend=False):
        if usercmds and resend:
            return {}
        return orig(usercmds=usercmds, note=note, cmd=cmd, resend=resend)
    p.c.send_packet = send_packet


def install_packetdup(p, dup, hist):
    """cl_packetdup: every usermove packet carries the current usercmd plus the last `dup` ones, with their original
    serverTimes (the server skips the ones it already ran). The packet is otherwise identical to
    fakeclient.Client.send_packet (commands, ack fields, Huffman, XOR)."""
    c = p.c
    orig = c.send_packet
    P = G.P

    def send_packet(usercmds=0, note="", cmd=None, resend=False):
        if usercmds and resend:
            return {}
        if not usercmds or cmd is None or note == "disconnect":
            return orig(usercmds=usercmds, note=note, cmd=cmd, resend=resend)
        m = P.Msg()
        for s, txt in c.client_cmds:
            if s > c.client_cmd_acked:
                m.write_bits(2, 3)
                m.write_long(s)
                m.write_string(txt)
        msg_ack, rel_ack = c.in_seq, c.server_cmd_seq
        ack_text = c.server_cmds.get(rel_ack, b"") if rel_ack > 0 else b""
        key = (c.checksum_feed ^ msg_ack ^ P.com_hashkey(ack_text, 32)) & 0xFFFFFFFF
        base = c.latest_server_time or 0
        t = max(base, (c.last_cmd_time or 0) + 1)
        c.last_cmd_time = t
        hist.append((t, dict(cmd)))
        del hist[:-(dup + 1)]
        m.write_bits(0 if c.last_msg_had_snapshot else 1, 3)
        m.write_byte(len(hist))
        for tt, cc in hist:
            P.write_usercmd_full(m, key, dict(cc, serverTime=tt))
        body = c.huff.compress(m.bytes())
        k2 = (c.challenge ^ c.server_id ^ msg_ack) & 0xFF
        enc = P.xor_stream(body, k2, ack_text)
        pkt = struct.pack("<iHBii", c.out_seq, c.qport, c.server_id, msg_ack, rel_ack) + enc
        c.out_seq += 1
        c.send_raw(pkt, note or "usermove dup x%d" % len(hist))
        return {"msgAck": msg_ack, "relAck": rel_ack, "cmd_times": [h[0] for h in hist]}
    c.send_packet = send_packet


class NetLoss(G.Duel):
    DUP = 2

    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.A, self.B = players[0], players[1]
        self.C = players[2] if len(players) > 2 else None
        self.held = {}      # tag -> raw c2s packet held back for reordering
        self.s2c_base = {}  # tag -> (window name -> first server seq after the trigger)
        self.log_drops = {"c2s": [], "s2c": [], "held": []}
        summary.setdefault("scenario_notes", {})["drops"] = self.log_drops
        swallow_keepalive(self.A)
        install_packetdup(self.B, self.DUP, [])
        if self.C:
            swallow_keepalive(self.C)

    # ---- helpers -------------------------------------------------------------------------------------
    @staticmethod
    def rel(p):
        ts = p.c.latest_server_time
        return None if ts is None or p.anchor is None else ts - p.anchor

    def look(self, p, cmd, yaw, pitch=0.0):
        ps = p.psn()
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        cmd["angles"][0] = (G.angle2short(pitch) - da[0]) & 0xFFFF
        cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF

    # ---- packet loss hooks (called by gameplay.session) ----------------------------------------------
    def drop_c2s(self, p, raw):
        r = self.rel(p)
        held = self.held.setdefault(p.tag, [])
        if held and len(held) == 2:  # release the two held packets in reverse order, before the current one
            for h in reversed(held):
                self._release(p, h)
            del held[:]
        out = False
        if p.tag == "B" and r is not None and p.epoch == 1 and p.phase == "active":
            k = r // 50
            if 3000 <= r < 6000 and k % 3 == 0:
                out = True
                self.log_drops["c2s"].append([p.tag, r])
            elif 6000 <= r < 6200:  # burst of 4 consecutive packets (longer than the dup window of 2)
                out = True
                self.log_drops["c2s"].append([p.tag, r])
            elif 6500 <= r < 8500 and (held or k % 5 == 0):
                # reorder: hold two consecutive packets, then send them swapped after the next one's turn
                held.append(raw)
                self.log_drops["held"].append([p.tag, r])
                return True
        if p.tag == "C" and r is not None and r >= 16000:
            out = True
            self.log_drops["c2s"].append([p.tag, r])
        return out

    def _release(self, p, raw):
        p.log.packet("c2s", raw, "reordered")
        p.c.sock.sendto(raw, p.c.addr)

    def drop_s2c(self, p, raw):
        if p.tag == "B":
            seq = int.from_bytes(raw[:4], "little") & 0x7FFFFFFF
            r = self.rel(p)
            bases = self.s2c_base.setdefault(p.tag, {})
            for name, start in (("singles", 9000), ("outage", 13000)):
                if name not in bases and r is not None and p.epoch == 1 and r >= start:
                    bases[name] = seq
            k = seq - bases["singles"] if "singles" in bases else -1
            if 0 <= k < 40 and k % 4 == 1:
                self.log_drops["s2c"].append([p.tag, "singles", k])
                return True
            o = seq - bases["outage"] if "outage" in bases else -1
            if 0 <= o < 35:
                self.log_drops["s2c"].append([p.tag, "outage", o])
                return True
        return False

    # ---- script ----------------------------------------------------------------------------------------
    def actions(self, p, t):
        gs = p.c.gamestate
        out = []
        sid = G.full_server_id(gs)

        def once(key, text):
            if key not in p.done:
                p.done.add(key)
                out.append(text)

        if t >= 0:
            once("team", "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1000:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))
        if p is self.B:
            for i, at in enumerate((3500, 4500, 6050, 7000, 9100, 13050)):
                if t >= at:
                    once("bsay%d" % i, "say b%d" % i)
        elif p is self.A:
            for i, at in enumerate((9200, 13500, 13900, 14500)):
                if t >= at:
                    once("asay%d" % i, "say a%d" % i)
            if t >= 22000:
                once("score", "score")
        return out

    def usercmd(self, p, t):
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}
        if p is self.A:
            if 1500 <= t < 4000:
                cmd["forwardmove"] = 127
            self.look(p, cmd, 20.0 * (t // 1000), 0.0)
            if 5000 <= t < 5100 or 9000 <= t < 9100:
                cmd["buttons"] |= G.B_JUMP
            if 6000 <= t < 7000 or 14000 <= t < 14500:
                cmd["buttons"] |= G.B_ATTACK
            if 10000 <= t < 10100:
                cmd["buttons"] |= G.B_RELOAD
        elif p is self.B:
            # walks and turns in steps keyed to its own snapshot time, so lost usercmds change the trajectory
            if 1500 <= t < 15000:
                cmd["forwardmove"] = 127 if (t // 700) % 3 else -127
                cmd["rightmove"] = 127 if (t // 900) % 2 else 0
            self.look(p, cmd, 45.0 * (t // 600), 0.0)
            if t // 1100 % 4 == 3:
                cmd["buttons"] |= G.B_JUMP
            if 4000 <= t < 4400 or 7000 <= t < 7600 or 10000 <= t < 11000:
                cmd["buttons"] |= G.B_ATTACK
            if 12000 <= t < 12100:
                cmd["buttons"] |= G.B_RELOAD
            if 15000 <= t < 15500:
                cmd["buttons"] |= G.B_CROUCH
        else:
            if 1500 <= t < 15000:
                cmd["forwardmove"] = 127 if (t // 800) % 2 else -127
            self.look(p, cmd, 90.0 * (t // 1500), 0.0)
        p.cmd = cmd
        return cmd


class GamestateLoss(NetLoss):
    """B loses fragments of its connect-time gamestate; no other loss."""

    DUP = 0

    def __init__(self, t0, players, summary):
        G.Duel.__init__(self, t0, players, summary)
        self.A, self.B = players[0], players[1]
        self.C = None
        self.held = {}
        self.s2c_base = {}
        self.log_drops = {"c2s": [], "s2c": [], "held": []}
        self.frag_seqs = []
        summary.setdefault("scenario_notes", {})["drops"] = self.log_drops

    def drop_c2s(self, p, raw):
        return False

    def drop_s2c(self, p, raw):
        if p.tag != "B" or not (int.from_bytes(raw[:4], "little") & 0x80000000):
            return False
        seq = int.from_bytes(raw[:4], "little") & 0x7FFFFFFF
        fstart, flen = struct.unpack_from("<ih", raw, 4)
        if seq not in self.frag_seqs:
            self.frag_seqs.append(seq)
        n = self.frag_seqs.index(seq)
        if n == 0 and fstart == 2600:  # first transmission: lose the third fragment
            self.log_drops["s2c"].append([p.tag, "gs0", fstart])
            return True
        if n == 1 and flen != 1300:    # retransmission: lose the last fragment
            self.log_drops["s2c"].append([p.tag, "gs1-last", fstart])
            return True
        return False

    def usercmd(self, p, t):
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}
        if 1500 <= t < 4000:
            cmd["forwardmove"] = 127
        if p is self.B and 2000 <= t < 2100:
            cmd["buttons"] |= G.B_JUMP
        p.cmd = cmd
        return cmd


_COMMON = ["+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1"]
CASES = {
    "netloss_play_dm_toujane": {
        "args": _COMMON + ["+set", "sv_timeout", "3", "+set", "sv_zombietime", "1", "+devmap", "mp_toujane"],
        "scenario": NetLoss,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("C", "allies", "m1carbine_mp")],
        "t0": 20000, "length": 26000,
    },
    "netloss_gamestate_dm_toujane": {
        "args": _COMMON + ["+devmap", "mp_toujane"],
        "scenario": GamestateLoss,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")],
        "t0": 20000, "length": 8000,
    },
}
