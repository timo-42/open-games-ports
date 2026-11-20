"""Lane wpn_grenade: frag and smoke offhand grenades (buttons 0x10000 = offhand class 1, 0x20000 = class 2 as read
by PM_Weapon_HandleOffhandButtons 0x080efce2), cooking, flight, bounce, explosion damage, grenade kills, ammo counts."""
import math
import gameplay as G
import cod2proto as P

B_FRAG, B_SMOKE = 0x10000, 0x20000


class GrenadeScen(G.Duel):
    TEAM_MENU = "team_britishgerman"
    WEAPON_MENU = {"allies": "weapon_british", "axis": "weapon_german"}
    """A (thompson: 1 frag + 1 smoke) and B (kar98k: 3 frags) face each other ~200 units apart."""
    T_TELE = 5000
    A_AT, B_AT = "p0", "p1"
    # (player tag, button, start, end, pitch, offhand-name-or-None)
    PLAN = []

    def actions(self, p, t):
        gs = p.c.gamestate
        out = []

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                r = fn()
                if r:
                    out.append(r)
        sid = G.full_server_id(gs)
        if t >= 0:
            once("team", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, self.TEAM_MENU), p.team))
        if t >= 1000:
            menu = self.WEAPON_MENU[p.team]
            once("weapon", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))
        if p is self.A:
            if t >= 1500:
                once("mark_p0", lambda: self.mark(p, "p0"))
            if t >= 2600:
                once("mark_p1", lambda: self.mark(p, "p1"))
            if t >= self.T_TELE:
                once("teleport", lambda: self.teleport(p, self.A_AT, self.B_AT))
            if t >= self.T_SCORE:
                once("score", lambda: "score")
        else:
            if t >= self.T_TELE:
                once("teleport", lambda: self.teleport(p, self.B_AT, self.A_AT))
            if t >= self.T_SCORE:
                once("score", lambda: "score")
        return out

    T_SCORE = 30000

    def offidx(self, p, name):
        return G.weapon_index(p.c.gamestate, name) or 0

    def usercmd(self, p, t):
        ps = p.psn()
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
               "offhand": 0}
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]

        def look(yaw, pitch):
            cmd["angles"][0] = (G.angle2short(pitch) - da[0]) & 0xFFFF
            cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF
        if p is self.A and 1500 <= t < 2600:
            cmd["forwardmove"] = 127
        yaw = ps.get("viewangles[1]", 0.0)
        self.plan(p, t, cmd, look, yaw)
        p.cmd = cmd
        return cmd

    def plan(self, p, t, cmd, look, yaw):
        raise NotImplementedError


class FragSmoke(GrenadeScen):
    def plan(self, p, t, cmd, look, yaw):
        fa = "frag_grenade_british_mp"
        fb = "frag_grenade_german_mp"
        if p is self.A:
            if 5500 <= t < 5700:
                look(yaw, -25.0)
            if 6000 <= t < 6100:             # tap: throw the only frag (fuse 3.5 s)
                cmd["buttons"] |= B_FRAG
                cmd["offhand"] = self.offidx(p, fa)
            if 6100 <= t < 8000:
                look(yaw, -25.0)
            if 9000 <= t < 9100:             # smoke
                cmd["buttons"] |= B_SMOKE
            if 12000 <= t < 12100:           # second frag: no ammo left -> denied
                cmd["buttons"] |= B_FRAG
            if 13000 <= t < 13100:           # second smoke: denied
                cmd["buttons"] |= B_SMOKE
        else:
            if 5500 <= t < 11000:
                look(yaw, -25.0)
            if 11000 <= t:
                look(yaw, 0.0)
            if 6500 <= t < 8000:             # cook 1.5 s, then throw
                cmd["buttons"] |= B_FRAG
                cmd["offhand"] = self.offidx(p, fb)
            if 10000 <= t < 10100:           # tap throw
                cmd["buttons"] |= B_FRAG
            if 14000 <= t < 18000:           # hold past the fuse: explodes in the hand
                cmd["buttons"] |= B_FRAG
            if 20000 <= t < 20100:           # no frags left
                cmd["buttons"] |= B_FRAG
        if t >= 24000 and p is self.B:
            cmd["buttons"] |= G.B_USE


class KillRespawn(GrenadeScen):
    """tdm on carentan (american/german): A (m1garand, 2 frags) throws at B (mp40, 1 frag + 1 smoke) and runs away,
    B is killed by the grenade, respawns with use (killcam), throws again and cooks a frag to death."""
    TEAM_MENU = "team_americangerman"
    WEAPON_MENU = {"allies": "weapon_american", "axis": "weapon_german"}
    T_SCORE = 36000
    A_AT, B_AT = "p1", "p0"   # A on the open (west) side so that it can run away from the blast, B at the wall

    def plan(self, p, t, cmd, look, yaw):
        fa = "frag_grenade_american_mp"
        if p is self.A:
            if 5500 <= t < 12000:
                look(yaw, 4.0)                 # flat-ish throw at B
            if 6000 <= t < 6100:
                cmd["buttons"] |= B_FRAG
                cmd["offhand"] = self.offidx(p, fa)
            if 6300 <= t < 9300:
                cmd["forwardmove"] = -127      # run away from the blast
            if 12000 <= t < 13500:             # second frag: cook 1.5 s, lob
                look(yaw, -35.0)
                cmd["buttons"] |= B_FRAG
            if 13500 <= t < 14000:
                look(yaw, -35.0)
            if 16000 <= t < 16100:             # none left
                cmd["buttons"] |= B_FRAG
        else:
            if 6500 <= t < 6600:               # smoke
                cmd["buttons"] |= B_SMOKE
            if 8500 <= t < 8600:               # frag, tap
                cmd["buttons"] |= B_FRAG
            if 14000 <= t < 40000:             # killed around 9.8 s: respawn with use, then cook it out
                cmd["buttons"] |= G.B_USE
            if 26000 <= t < 31000 and t >= self.B_respawn_t():
                cmd["buttons"] |= B_FRAG

    def B_respawn_t(self):
        return 0


CASES = {
    "wpn_grenade_fragsmoke_dm_toujane": {
        "args": ["+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
                 "+devmap", "mp_toujane"],
        "scenario": FragSmoke,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "kar98k_mp")],
        "t0": 20000, "length": 34000,
    },
    "wpn_grenade_kill_tdm_carentan": {
        "args": ["+set", "g_gametype", "tdm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
                 "+devmap", "mp_carentan"],
        "scenario": KillRespawn,
        "players": [("A", "allies", "m1garand_mp"), ("B", "axis", "mp40_mp")],
        "t0": 20000, "length": 40000,
    },
}
