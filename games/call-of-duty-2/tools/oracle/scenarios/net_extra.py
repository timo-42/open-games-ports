"""Lane net_extra: network edge cases of the running dm game.

Cases (all dm on mp_toujane, pinned G_InitGame seed; every decision is a function of the deciding client's own latest
snapshot server time or of the server message sequence relative to such a trigger, never of the wall clock):

  netx_edge_dm_toujane    A clean, B suffers server->client delay bursts and reordering
                          (generic scenario hook `hold_s2c` of gameplay.py), C's client->server packets all vanish
                          while a GSC overlay (mods/netxflood: setClientCvar / iprintln to C every server frame)
                          fills the server's reliable-command queue for C until it overflows and the server drops
                          C with EXE_SERVERCOMMANDOVERFLOW. (rcon is no usable flood source: SVC_RemoteCommand
                          ignores a second rcon within 500 ms of wall-clock time.)
  netx_rotloss_dm_toujane the time limit ends the map (rotation to mp_carentan, spawn pad as in gt_tdm); B loses the
                          second gamestate's fragments and c2s packets around the map change, A is clean.
  netx_rate_dm_toujane    clients with a NON-LAN source address (198.18.0.x, added to lo inside the namespace; the
                          server treats 127.0.0.1 and private ranges as LAN, Sys_IsLANAddress): rate 2500 / 5000 and
                          snaps 10 / 20 delay snapshots (SV_RateMsec, rateDelayed), changed at run time by `userinfo`.
"""
import struct

import gameplay as G

P = G.P


def swallow_keepalive(p):
    """Replace the wall-clock keep-alive of p's client by a no-op (instance level)."""
    orig = p.c.send_packet

    def send_packet(usercmds=0, note="", cmd=None, resend=False):
        if usercmds and resend:
            return {}
        return orig(usercmds=usercmds, note=note, cmd=cmd, resend=resend)
    p.c.send_packet = send_packet


class NetX(G.Duel):
    """Common script: join through the stock menus, walk / turn / fire keyed to the client's own snapshot time."""

    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.by = {p.tag: p for p in players}
        self.log_drops = {"c2s": [], "s2c": [], "held": [], "rcon": []}
        summary.setdefault("scenario_notes", {})["drops"] = self.log_drops
        self.s2c_base = {}

    @staticmethod
    def rel(p):
        ts = p.c.latest_server_time
        return None if ts is None or p.anchor is None else ts - p.anchor

    def look(self, p, cmd, yaw, pitch=0.0):
        ps = p.psn()
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        cmd["angles"][0] = (G.angle2short(pitch) - da[0]) & 0xFFFF
        cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF

    def join(self, p, t, out):
        gs = p.c.gamestate
        sid = G.full_server_id(gs)

        def once(key, text):
            if key not in p.done:
                p.done.add(key)
                out.append(text)
        if t >= 0:
            once("team", "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1500:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))

    def once(self, p, key, out, text):
        if key not in p.done:
            p.done.add(key)
            out.append(text)

    def base_cmd(self, p):
        return {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}


# =====================================================================================================
# netx_edge: s2c delay / reordering, reliable-command overflow
# =====================================================================================================
class Edge(NetX):
    DEAF_FROM = 6000    # C's client->server packets all vanish from here (by C's own snapshot time)

    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.A, self.B, self.C = players[0], players[1], players[2]
        swallow_keepalive(self.B)
        swallow_keepalive(self.C)

    # ---- loss / hold hooks -----------------------------------------------------------------------------
    def drop_c2s(self, p, raw):
        r = self.rel(p)
        if p is self.C and r is not None and p.epoch == 1 and r >= self.DEAF_FROM:
            self.log_drops["c2s"].append([p.tag, r])
            return True
        return False

    def hold_s2c(self, p, raw):
        if p is not self.B:
            return None
        seq = int.from_bytes(raw[:4], "little") & 0x7FFFFFFF
        r = self.rel(p)
        if p.epoch != 1:
            return None
        bases = self.s2c_base.setdefault(p.tag, {})
        for name, start in (("lag", 5000), ("reorder", 9000), ("deep", 13000)):
            if name not in bases and r is not None and r >= start:
                bases[name] = seq
        k = seq - bases["lag"] if "lag" in bases else -1
        # lag bursts: 6 server messages held, delivered in order together when message base+6 (+8 for the 2nd) arrives
        if 0 <= k < 6:
            self.log_drops["held"].append([p.tag, "lag", k])
            return ("seq", bases["lag"] + 6)
        if 10 <= k < 18:
            self.log_drops["held"].append([p.tag, "lag2", k])
            return ("seq", bases["lag"] + 18)
        k = seq - bases["reorder"] if "reorder" in bases else -1
        if 0 <= k < 30 and k % 3 == 1:     # each 3rd message arrives after its successor
            self.log_drops["held"].append([p.tag, "reorder", k])
            return ("seq", seq + 1, "after")
        k = seq - bases["deep"] if "deep" in bases else -1
        if 0 <= k < 30 and k % 10 == 2:    # delivered 3 messages late (after them)
            self.log_drops["held"].append([p.tag, "deep", k])
            return ("seq", seq + 3, "after")
        return None

    # ---- script ----------------------------------------------------------------------------------------
    def actions(self, p, t):
        out = []
        self.join(p, t, out)
        if p is self.B:
            for i, at in enumerate((4000, 5200, 7000, 9500, 11000, 13500, 16000)):
                if t >= at:
                    self.once(p, "bsay%d" % i, out, "say b%d" % i)
        if p is self.A:
            for i, at in enumerate((3000, 6500)):
                if t >= at:
                    self.once(p, "asay%d" % i, out, "say a%d" % i)
        return out

    def usercmd(self, p, t):
        cmd = self.base_cmd(p)
        if p is self.A:
            if 1500 <= t < 4000:
                cmd["forwardmove"] = 127
            self.look(p, cmd, 20.0 * (t // 1000))
            if 5000 <= t < 5100:
                cmd["buttons"] |= G.B_JUMP
            if 6000 <= t < 7000:
                cmd["buttons"] |= G.B_ATTACK
        elif p is self.B:
            if 1500 <= t < 18000:
                cmd["forwardmove"] = 127 if (t // 700) % 3 else -127
                cmd["rightmove"] = 127 if (t // 900) % 2 else 0
            self.look(p, cmd, 45.0 * (t // 600))
            if t // 1100 % 4 == 3:
                cmd["buttons"] |= G.B_JUMP
            if 4000 <= t < 4400 or 9000 <= t < 10000:
                cmd["buttons"] |= G.B_ATTACK
        else:
            if 1500 <= t < 6000:
                cmd["forwardmove"] = 127 if (t // 800) % 2 else -127
            self.look(p, cmd, 90.0 * (t // 1500))
        p.cmd = cmd
        return cmd


# =====================================================================================================
# netx_rotloss: loss during a map change
# =====================================================================================================
class RotLoss(NetX):
    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.A, self.B = players[0], players[1]
        swallow_keepalive(self.B)
        self.frag_seqs = []

    def drop_c2s(self, p, raw):
        if p is self.B and p.epoch >= 2:
            r = self.rel(p)
            if r is not None and p.phase == "active" and 3900 <= r < 5300 and (r // 50) % 2 == 0:
                self.log_drops["c2s"].append([p.tag, r])
                return True
        return False

    def drop_s2c(self, p, raw):
        if p is not self.B:
            return False
        seq = int.from_bytes(raw[:4], "little") & 0x7FFFFFFF
        if not (int.from_bytes(raw[:4], "little") & 0x80000000):
            return False
        fstart, flen = struct.unpack_from("<ih", raw, 4)
        if seq not in self.frag_seqs:
            self.frag_seqs.append(seq)
        n = self.frag_seqs.index(seq)
        # n == 0: first gamestate; n >= 1: the second level's gamestate and its retransmissions
        if n == 1 and fstart == 1300:
            self.log_drops["s2c"].append([p.tag, "gs2-frag1", fstart])
            return True
        if n == 2 and flen != 1300:
            self.log_drops["s2c"].append([p.tag, "gs2-retx-last", fstart])
            return True
        return False

    def actions(self, p, t):
        out = []
        self.join(p, t, out)
        if t >= 30000 and p is self.A:
            self.once(p, "say1", out, "say pre")
        return out

    def usercmd(self, p, t):
        cmd = self.base_cmd(p)
        if 1500 <= t < 3000:
            cmd["forwardmove"] = 127
        self.look(p, cmd, 30.0 * (t // 1000))
        p.cmd = cmd
        return cmd

    def actions2(self, p, t):
        out = []
        if p is self.A and t >= 3500:
            self.once(p, "e2say", out, "say map2a")
        if p is self.B and t >= 6500:
            self.once(p, "e2sayb", out, "say map2b")
        return out

    def usercmd2(self, p, t):
        cmd = self.base_cmd(p)
        if 1000 <= t < 2500 or (p is self.B and 3700 <= t < 5500):
            cmd["forwardmove"] = 127
        if p is self.B and 4200 <= t < 4300:
            cmd["buttons"] |= G.B_JUMP
        self.look(p, cmd, 50.0 * (t // 700))
        p.cmd = cmd
        return cmd


# =====================================================================================================
# netx_rate: non-LAN peers, rate and snaps limiting
# =====================================================================================================
class Rate(NetX):
    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.A = players[0]
        for p in players:
            swallow_keepalive(p)

    def actions(self, p, t):
        out = []
        self.join(p, t, out)
        i = self.players.index(p)
        # gather everybody around client A's first mark to enlarge the snapshots
        if p is self.A and t >= 3000 and "mark_p0" not in p.done:
            p.done.add("mark_p0")
            self.mark(p, "p0")
        if p is not self.A and t >= 4500 + 300 * i:
            if "gather" not in p.done:
                p.done.add("gather")
                a = self.A.marks.get("p0")
                if a:
                    out.append("setviewpos %.1f %.1f %.1f %.1f" % (a[0] + 30.0 * i, a[1] + 15.0 * (i & 1), a[2] + 60.0,
                                                                   45.0 * i))
        # rate / snaps changes at run time (the userinfo string replaces the whole userinfo, as in net_clients)
        for n, (at, rate, snaps) in enumerate(self.PROFILE.get(p.tag, [])):
            if t >= at:
                if ("ui%d" % n) not in p.done:
                    p.done.add("ui%d" % n)
                    out.append('userinfo "%s"' % G.userinfo(p.name, 1, p.c.qport, {"rate": rate, "snaps": snaps}))
        return out

    # per tag: (relative time, rate, snaps) changes after the connect-time values of the plugin spec
    PROFILE = {"B": [(9000, 1000, 30), (14000, 5000, 10), (20000, 25000, 20)],
               "C": [(12000, 2500, 20)],
               "D": [(16000, 2500, 10), (22000, 200000, 5)]}

    def usercmd(self, p, t):
        cmd = self.base_cmd(p)
        if 1500 <= t < 4500:
            cmd["forwardmove"] = 127
        self.look(p, cmd, 20.0 * (t // 1000))
        if t >= 6000 and (t // 1500) % 2 == 0:
            cmd["buttons"] |= G.B_ATTACK
        if t >= 6000 and t // 700 % 5 == 0:
            cmd["buttons"] |= G.B_JUMP
        p.cmd = cmd
        return cmd


FLOOD_GSC = """
netxFlood()
{
	// T0 + 8000 ms (T0 = server time 20000): flood the reliable-command queue of fakeC with one flag-0 command
	// (iprintln, "e"/"h" style, dropped once 64 commands are pending) and one flag-1 command (setClientCvar, "v")
	// per server frame (50 ms); fakeC's acks never reach the server
	while(gettime() < 28000)
		wait 0.05;
	for(i = 0; i < 160; i++)
	{
		victim = undefined;
		players = getentarray("player", "classname");
		for(j = 0; j < players.size; j++)
			if(isDefined(players[j].name) && players[j].name == "fakeC")
				victim = players[j];
		if(!isDefined(victim))
			return;
		victim iprintln("netx " + i);
		victim setClientCvar("nx" + i, "v" + i);
		wait 0.05;
	}
}
"""


def make_flood_mod(home, iwd):
    """Loose-file overlay (never committed; derived from game data): the stock dm.gsc plus `thread netxFlood();` at the
    end of main() and the function above, under <fs_homepath>/mods/netxflood."""
    import hashlib
    import os
    import zipfile
    src = zipfile.ZipFile(iwd).read("maps/mp/gametypes/dm.gsc").decode("latin1")
    anchor = "\tlevel.endgameconfirmed = ::endMap;\r\n}"
    if anchor not in src:
        raise SystemExit("dm.gsc: main() anchor not found")
    out = src.replace(anchor, "\tlevel.endgameconfirmed = ::endMap;\r\n\tthread netxFlood();\r\n}", 1) + FLOOD_GSC.replace("\n", "\r\n")
    d = os.path.join(home, "mods/netxflood/maps/mp/gametypes")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "dm.gsc"), "w", encoding="latin1", newline="") as fh:
        fh.write(out)
    return "netx flood overlay dm.gsc sha256 %s" % hashlib.sha256(out.encode("latin1")).hexdigest()


_COMMON = ["+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1", "+set", "sv_zombietime", "1"]
CASES = {
    "netx_edge_dm_toujane": {
        "args": _COMMON + ["+set", "fs_game", "mods/netxflood", "+devmap", "mp_toujane"],
        "mod": lambda home, data: make_flood_mod(home, data + "/iw_15.iwd"),
        "scenario": Edge,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("C", "allies", "m1carbine_mp")],
        "t0": 20000, "length": 24000,
    },
    "netx_rotloss_dm_toujane": {
        "args": _COMMON + ["+set", "scr_dm_timelimit", "1.25", "+set", "sv_mapRotation", "gametype dm map mp_carentan",
                           "+devmap", "mp_toujane"],
        "scenario": RotLoss,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")],
        "t0": 20000, "length": 40000, "rotate": 9000,
    },
    "netx_rate_dm_toujane": {
        "args": _COMMON + ["+set", "sv_maxRate", "6000", "+devmap", "mp_toujane"],
        "scenario": Rate,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("C", "allies", "m1carbine_mp"),
                    ("D", "axis", "mp40_mp")],
        "client_addrs": {"B": "198.18.0.2", "C": "198.18.0.3", "D": "198.18.0.4"},
        "late_connect": {"B": 9000, "C": 9500, "D": 10000},
        "userinfo": {"B": {"rate": 2500, "snaps": 10}, "C": {"rate": 5000, "snaps": 20},
                     "D": {"rate": 5000, "snaps": 10}},
        "t0": 20000, "length": 28000,
    },
}
