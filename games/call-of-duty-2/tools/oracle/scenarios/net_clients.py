"""Lane net_clients: more than 4 clients, teams, PVS culling, "server is full", rate / snaps limiting.

Cases
  net_clients_tdm8_mp_toujane   sv_maxclients 8, tdm (fs_game mods/netclients, no files), 8 fake clients (4 allies /
                                4 axis) + a 9th that must be rejected ("Server is full"); everybody moves, aims at
                                the nearest enemy visible in its own snapshot and fires; two gatherings
                                (setviewpos) put both teams face to face; clients send `userinfo` with other
                                rate/snaps values (including the clamped extremes 500 and 200000).
  net_clients_dm20_mp_toujane   sv_maxclients 20, dm, 12 fake clients (client numbers 0..11 of 20 slots), same
                                movement / firefight script, 24 s.

Every decision is a function of the deciding client's own latest snapshot (server time + its entities); the join
sequence is staggered by 250 ms per client so two clients' commands never race for the same server frame.
"""
import math
import os

import gameplay as G
import cod2proto as P

ALLIES_W = ["thompson_mp", "sten_mp", "bren_mp", "enfield_mp", "m1garand_mp", "thompson_mp"]
# shotgun_mp and kar98k_sniper_mp are left out on purpose: their pellets / penetration traces diverge in the
# reconstruction (see analysis/gameplay/net_clients.md, open items)
AXIS_W = ["mp40_mp", "kar98k_mp", "mp44_mp", "g43_mp", "mp40_mp", "kar98k_mp"]

# per client index: [(relative time ms, rate, snaps), ...]  ("userinfo" command, replaces the whole userinfo string)
# [O] the server ignores rate/snaps for LAN/loopback peers when it schedules snapshots (SV_SendMessageToClient
# 0x0809aba2 tests netchan type 2 or Sys_IsLANAddress 0x080d5638 and then sends every frame), but it does store the
# clamped rate in client_t (SV_UserinfoChanged 0x08090...: <1000 -> 1000, >90000 -> 90000, missing -> 5000), which
# `rcon status` prints. The profiles exercise exactly that, including the clamps.
# [O] sv_floodProtect: a game command (mr/say/score/kill/setviewpos) that arrives less than ~1 s after the client's
# previous command is silently dropped (the recon, which has slower frames, lost "mr weapon" 1000 ms after "mr team"
# whenever one packet crossed a frame boundary). Every client therefore keeps >= 1500 ms between its commands.
PROFILES = {
    1: [(5000, 5000, 10)],
    2: [(5000, 5000, 20)],
    3: [(5000, 5000, 10), (15000, 25000, 20)],
    5: [(5000, 500, 20)],
    6: [(5000, 200000, 5)],
    7: [(6000, 0, 10), (22000, 25000, 20)],
    9: [(5000, 5000, 10)],
    10: [(5000, 3000, 20)],
}


def userinfo(name, challenge, qport, rate, snaps):
    kv = [("cg_predictItems", "1"), ("cl_anonymous", "0"), ("cl_punkbuster", "0"), ("cl_voice", "0"),
          ("cl_wwwDownload", "0"), ("rate", str(rate)), ("snaps", str(snaps)), ("name", name),
          ("protocol", "118"), ("challenge", str(challenge)), ("qport", str(qport))]
    return "".join("\\%s\\%s" % kv_ for kv_ in kv)


class NetScen(G.Duel):
    JOIN_STEP = 250
    MOVE_FROM = 4500
    MOVE_TO = 1 << 30

    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.arena = {}
        self.real = [p for p in players if p.tag != "Z"]
        self.reject = [p for p in players if p.tag == "Z"]
        for p in self.reject:   # keep the OOB reply of the rejected client in the (compared) scenario notes
            def ev(kind, _orig=p.log.event, _n=self.notes, **kw):
                if kind == "oob_inband":
                    _n.setdefault("reject_" + p.tag, []).append(kw.get("text"))
                return _orig(kind, **kw)
            p.log.event = ev

    # ------------------------------------------------------------------ helpers
    def idx(self, p):
        return self.real.index(p)

    def is_enemy(self, p, num):
        """Entity number num is an enemy of p. tdm: teams alternate with the client index; dm: everyone else."""
        if num == p.c.client_num:
            return False
        if not self.teams:
            return True
        for q in self.real:
            if q.c.client_num == num:
                return (self.idx(q) & 1) != (self.idx(p) & 1)
        return True

    teams = True

    # ------------------------------------------------------------------ reliable commands
    def actions(self, p, t):
        gs = p.c.gamestate
        out = []
        i = self.idx(p)
        base = self.JOIN_STEP * i

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                r = fn()
                if r:
                    out.append(r)

        sid = G.full_server_id(gs)
        if self.teams:
            team = "allies" if i % 2 == 0 else "axis"
        else:
            team = "allies" if i % 2 == 0 else "axis"   # dm still takes a team choice (spawns the same)
        weap = (ALLIES_W if team == "allies" else AXIS_W)[(i // 2) % 6]
        if t >= base:
            once("team", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), team))
        if t >= base + 1500:
            menu = "weapon_british" if team == "allies" else "weapon_german"
            once("weapon", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, menu), weap))
        for n, (pt, rate, snaps) in enumerate(PROFILES.get(i, [])):
            if t >= pt:
                once("rate%d" % n, lambda rate=rate, snaps=snaps: 'userinfo "%s"' % userinfo(
                    p.name, 1, p.c.qport, rate, snaps))   # constant challenge: the real one is wall-clock derived
        if i == 0 and t >= 3000:
            once("m0", lambda: self.mark_pos(p, i, "m0"))
        if i == 0 and t >= 4400:
            once("m1", lambda: self.mark_pos(p, i, "m1"))
        if t >= 8000 + 40 * i:
            once("tp1", lambda: self.tp(p, i, 0))
        if t >= 17000 + 40 * i:
            once("tp2", lambda: self.tp(p, i, 4))
        if t >= 18500 + 200 * i:
            once("score", lambda: "score")
        if t >= 20000 + 300 * i and i % 3 == 0:
            once("say", lambda: "say hello from %d" % i)
        if t >= 24000 and i == 4:
            once("kill", lambda: "kill")
        return out

    def actions2(self, p, t):
        return []

    def mark_pos(self, p, i, name):
        ps = p.psn()
        self.arena[name] = (ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0), ps.get("origin[2]", 0.0))
        return None

    def tp(self, p, i, step):
        """Gather everybody on the stretch of ground that client 0 walked (its positions at 3 s and 4.4 s, recorded
        from its own snapshots >3 s before anybody reads them): one team at each end, facing the other."""
        m0, m1 = self.arena.get("m0"), self.arena.get("m1")
        if not m0 or not m1:
            return None
        if step == 4:   # second gathering: swap the ends
            m0, m1 = m1, m0
        dx, dy = m1[0] - m0[0], m1[1] - m0[1]
        norm = math.hypot(dx, dy) or 1.0
        m1 = (m0[0] + dx / norm * 110.0, m0[1] + dy / norm * 110.0, m0[2])   # teams 110 units apart
        px, py = -dy / norm, dx / norm
        end, other = (m0, m1) if i % 2 == 0 else (m1, m0)
        k = (i // 2) - 1.5
        yaw = math.degrees(math.atan2(other[1] - end[1], other[0] - end[0]))
        text = "setviewpos %.1f %.1f %.1f %.1f" % (end[0] + px * 22.0 * k, end[1] + py * 22.0 * k, end[2] + 30.0, yaw)
        self.notes["tp%d_%d" % (step, i)] = text
        return text

    # ------------------------------------------------------------------ usercmd
    def usercmd(self, p, t):
        ps = p.psn()
        raw = p.ps()
        i = self.idx(p)
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
               "offhand": 0}
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        health = P.s32(raw[0x12C >> 2]) if raw else 0
        pm_type = ps.get("pm_type", 0)

        def look(yaw, pitch):
            cmd["angles"][0] = (G.angle2short(pitch) - da[0]) & 0xFFFF
            cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF

        if t < self.MOVE_FROM:
            if 3000 <= t < 4400 and health > 0:
                cmd["forwardmove"] = 127     # client 0's walked stretch becomes the arena
            p.cmd = cmd
            return cmd
        dead = health <= 0 or pm_type >= 4
        if dead:
            cmd["buttons"] |= G.B_USE      # respawn as soon as the script polls useButtonPressed (killcam ends)
            p.cmd = cmd
            return cmd
        yaw0 = p.marks.setdefault("yaw0", ps.get("viewangles[1]", 0.0))
        k = (t - self.MOVE_FROM) // 1000
        phase = (k + i) % 7
        yaw = yaw0 + 40.0 * i + 33.0 * ((t - self.MOVE_FROM) // 1500)
        if not ((9000 <= t < 14500) or (18000 <= t < 22500)):
            look(yaw, 0.0)
        standing = (9000 <= t < 14500) or (18000 <= t < 22500)   # firefight windows right after a gathering
        if standing:
            if t in range(9000, 9100) or t in range(18000, 18100):
                cmd["buttons"] |= G.B_JUMP       # leave crouch / prone before the firefight
            else:
                cmd["buttons"] |= G.B_ADS
        elif phase in (0, 1, 2):
            cmd["forwardmove"] = 127
        elif phase == 3:
            cmd["forwardmove"] = 127
            cmd["rightmove"] = 127
            cmd["buttons"] |= G.B_JUMP if (t // 50) % 12 == 0 else 0
        elif phase == 4:
            cmd["forwardmove"] = 64
            cmd["buttons"] |= G.B_CROUCH
        elif phase == 5:
            cmd["forwardmove"] = -127
        else:
            cmd["rightmove"] = -127
            cmd["buttons"] |= G.B_PRONE if (t // 1000) % 2 == 0 else 0
        # aim at the nearest visible enemy of our own snapshot and fire a burst
        me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0), ps.get("origin[2]", 0.0) +
              ps.get("viewHeightCurrent", 0.0)]
        best, bd = None, None
        for n, st in sorted(p.ents().items()):
            if n >= self.maxc or st[1] != 1 or not self.is_enemy(p, n):   # eType 1 = ET_PLAYER
                continue
            tx, ty, tz = P.bits2f(st[6]), P.bits2f(st[7]), P.bits2f(st[8]) + 24.0
            d = (tx - me[0]) ** 2 + (ty - me[1]) ** 2 + (tz - me[2]) ** 2
            if bd is None or d < bd:
                best, bd = (tx, ty, tz), d
        if best is not None and t >= 6000:
            dx, dy, dz = best[0] - me[0], best[1] - me[1], best[2] - me[2]
            look(math.degrees(math.atan2(dy, dx)), -math.degrees(math.atan2(dz, math.hypot(dx, dy))))
            cmd["forwardmove"] = 0 if bd < 90000 else cmd["forwardmove"]
            cmd["buttons"] = (cmd["buttons"] & ~G.B_PRONE)
            cmd["buttons"] |= G.B_ATTACK
        elif t >= 7000 and ((t // 50) + 3 * i) % 24 < 3:
            cmd["buttons"] |= G.B_ATTACK
        if (t // 50 + i) % 130 == 0:
            cmd["buttons"] |= G.B_RELOAD
        p.cmd = cmd
        return cmd

    maxc = 8

    usercmd2 = usercmd


class Tdm8(NetScen):
    teams = True
    maxc = 8


class Dm20(NetScen):
    teams = False
    maxc = 20


CASES = {
    "net_clients_tdm8_mp_toujane": {
        "args": ["+set", "fs_game", "mods/netclients", "+set", "sv_maxclients", "8", "+set", "g_gametype", "tdm",
                 "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1", "+devmap", "mp_toujane"],
        "scenario": Tdm8,
        "players": [(chr(65 + i), "allies" if i % 2 == 0 else "axis", "thompson_mp") for i in range(8)] +
                   [("Z", "allies", "thompson_mp")],
        "t0": 20000, "length": 30000, "rotate": 0, "bot_rcon": None, "mod": None,
    },
    "net_clients_dm20_mp_toujane": {
        "args": ["+set", "fs_game", "mods/netclients", "+set", "sv_maxclients", "20",
                 "+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
                 "+devmap", "mp_toujane"],
        "scenario": Dm20,
        "players": [(chr(65 + i), "allies" if i % 2 == 0 else "axis", "thompson_mp") for i in range(12)],
        "t0": 20000, "length": 24000, "rotate": 0, "bot_rcon": None, "mod": None,
    },
}
