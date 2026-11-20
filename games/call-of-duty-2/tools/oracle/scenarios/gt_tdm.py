"""Lane gt_tdm: Team Deathmatch (g_gametype tdm).

gt_tdm_teams_mp_toujane   four clients: explicit team join (A allies), autoassign (B, C, D; C hits the randomInt
                          tie-break, D the "smaller team" branch), team kill with scr_friendlyfire 1 (A kills its own
                          team mate, friendly damage + score--), enemy kill (A kills B: score++, team score++), an
                          axis player kills A, a suicide, a mid-game team change (B -> allies, switching_teams
                          suicide), say_team and score.
gt_tdm_rotate_dm_mp_toujane   two clients, one enemy kill, the time limit ends the map (W/L lines for the winning
                          team) and sv_mapRotation "gametype dm map mp_carentan" changes the gametype.
Every decision is a function of the deciding client's own snapshot and server time (see gameplay.py)."""
import math
import re

import gameplay as G

P = G.P

WEAPON_BY_MENU = {"weapon_british": "thompson_mp", "weapon_german": "mp40_mp",
                  "weapon_american": "thompson_mp", "weapon_russian": "ppsh_mp"}


def last_script_menu(p):
    """Newest `v g_scriptMainMenu "<menu>"` server command this client has received (the weapon menu that the
    server opened for the team the script assigned)."""
    best = None
    for seq, raw in sorted(p.c.server_cmds.items()):
        m = re.match(r'v g_scriptMainMenu "(\w+)"', raw.decode("latin1"))
        if m:
            best = m.group(1)
    return best


class TdmTeams(G.Duel):
    def __init__(self, t0, players, summary):
        G.Duel.__init__(self, t0, players, summary)
        self.by = {p.tag: p for p in players}
        self.roles_ = None

    # -- helpers ---------------------------------------------------------------------------------------------
    def team_of(self, n):
        fr = self.A.c.frames.get(self.A.c.in_seq)
        st = fr["clients"].get(n) if fr else None
        return P.named_fields(st, P.CLIENT_FIELDS)["team"] if st else None

    def roles(self):
        """X = A's team mate among C/D, Y = the enemy among C/D (from A's own clientState view)."""
        if self.roles_ is None:
            ta = self.team_of(self.A.c.client_num)
            same = [q for q in (self.by["C"], self.by["D"]) if self.team_of(q.c.client_num) == ta]
            other = [q for q in (self.by["C"], self.by["D"]) if self.team_of(q.c.client_num) != ta]
            if len(same) != 1 or len(other) != 1 or ta is None:
                return None
            self.roles_ = (same[0], other[0])
            self.notes["roles"] = {"X(team mate of A)": same[0].tag, "Y(enemy)": other[0].tag}
        return self.roles_

    def tele_to(self, p, at, facing, key):
        if key in p.done:
            return None
        a, f = self.A.marks.get(at), self.A.marks.get(facing)
        if not a or not f:
            return None
        p.done.add(key)
        return self.teleport(p, at, facing)

    def join_weapon(self, p, key, t_min, sid, gs):
        """Weapon choice for a player the script placed on a team: take the menu the server opened for it."""
        if key in p.done or t_min is None:
            return None
        menu = last_script_menu(p)
        if not menu or not menu.startswith("weapon_"):
            return None
        p.done.add(key)
        return "mr %d %d %s" % (sid, G.menu_index(gs, menu), WEAPON_BY_MENU[menu])

    def aim(self, p, t, target, t_aim, t_fire, t_end, cmd, da, ps):
        if t_aim <= t < t_end and target is not None:
            me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0),
                  ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0)]
            st = p.ents().get(target.c.client_num)
            if st is not None:
                e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
                tgt = [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"] + 40.0]
                dx, dy, dz = tgt[0] - me[0], tgt[1] - me[1], tgt[2] - me[2]
                cmd["angles"][0] = (G.angle2short(-math.degrees(math.atan2(dz, math.hypot(dx, dy)))) - da[0]) & 0xFFFF
                cmd["angles"][1] = (G.angle2short(math.degrees(math.atan2(dy, dx))) - da[1]) & 0xFFFF
            if t >= t_fire:
                cmd["buttons"] |= G.B_ATTACK

    # -- reliable commands -----------------------------------------------------------------------------------
    def actions(self, p, t):
        gs = p.c.gamestate
        sid = G.full_server_id(gs)
        out = []

        def add(x):
            if x:
                out.append(x)

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                add(fn())

        team_menu = G.menu_index(gs, "team_britishgerman")
        tag = p.tag
        if tag == "A":
            if t >= 0:
                once("team", lambda: "mr %d %d allies" % (sid, team_menu))
            if t >= 1000:
                once("weapon", lambda: "mr %d %d thompson_mp" % (sid, G.menu_index(gs, "weapon_british")))
            if t >= 1500:
                once("mark_p0", lambda: self.mark(p, "p0"))
            if t >= 2600:
                once("mark_p1", lambda: self.mark(p, "p1"))
            if t >= 15000:
                add(self.tele_to(p, "p0", "p1", "tele1"))
            if t >= 23000:
                add(self.tele_to(p, "p0", "p1", "tele2"))
            if t >= 37000:
                once("score", lambda: "score")
            if t >= 38000:
                once("say_team", lambda: "say_team tk")
        elif tag == "B":
            if t >= 300:
                once("team", lambda: "mr %d %d autoassign" % (sid, team_menu))   # 1 allies / 0 axis -> axis
            if t >= 1300:
                add(self.join_weapon(p, "weapon", t, sid, gs))
            if t >= 23000:
                add(self.tele_to(p, "p1", "p0", "tele1"))
            if t >= 40000:
                once("change", lambda: "mr %d %d allies" % (sid, team_menu))     # switching_teams suicide
            if t >= 41500:
                add(self.join_weapon(p, "weapon2", t, sid, gs))
        elif tag == "C":
            if t >= 600:
                once("team", lambda: "mr %d %d autoassign" % (sid, team_menu))   # 1/1: randomInt tie-break
            if t >= 1600:
                add(self.join_weapon(p, "weapon", t, sid, gs))
        elif tag == "D":
            if t >= 900:
                once("team", lambda: "mr %d %d autoassign" % (sid, team_menu))   # smaller team
            if t >= 1900:
                add(self.join_weapon(p, "weapon", t, sid, gs))
        # roles are known once everyone spawned
        if t >= 14000 and tag in "CD" and self.roles():
            x, y = self.roles()
            if p is x and t >= 15000:
                add(self.tele_to(p, "p1", "p0", "tele1"))
            if p is y and t >= 30000:
                add(self.tele_to(p, "p1", "p0", "tele1"))
            if p is x and t >= 41000:
                once("kill", lambda: "kill")                                     # suicide: score--
            if p is y and t >= 36500:
                once("say", lambda: "say wp")
        return out

    # -- usercmds --------------------------------------------------------------------------------------------
    def usercmd(self, p, t):
        ps = p.psn()
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        roles = self.roles() if t >= 14000 else None
        x, y = roles if roles else (None, None)
        tag = p.tag
        if tag == "A":
            if 1500 <= t < 2600:
                cmd["forwardmove"] = 127
            if 3500 <= t < 3600:
                cmd["buttons"] |= G.B_JUMP
            if 4500 <= t < 5500:
                cmd["buttons"] |= G.B_CROUCH
            # phase 1: team kill of X, phase 2: enemy kill of B
            self.aim(p, t, x, 16000, 16500, 19500, cmd, da, ps)
            self.aim(p, t, self.by["B"], 25000, 25500, 28500, cmd, da, ps)
            if 37500 <= t < 45000:
                cmd["buttons"] |= G.B_USE       # A was killed by Y: respawn
        elif tag == "B":
            if 1500 <= t < 2500:
                cmd["rightmove"] = 127
            if 3000 <= t < 3500:
                cmd["buttons"] |= G.B_CROUCH
            if 28500 <= t < 38000:
                cmd["buttons"] |= G.B_USE
        elif tag == "C" or tag == "D":
            if 2000 <= t < 3000:
                cmd["forwardmove"] = 127
            if 4000 <= t < 4100:
                cmd["buttons"] |= G.B_JUMP
            if p is x and 19500 <= t < 30000:
                cmd["buttons"] |= G.B_USE
            if p is y:
                self.aim(p, t, self.A, 32000, 32500, 36000, cmd, da, ps)
        p.cmd = cmd
        return cmd

    def actions2(self, p, t):
        return []

    def usercmd2(self, p, t):
        return G.Duel.usercmd2(self, p, t)


class TdmRotate(G.Duel):
    """Two clients: A allies, B axis; A kills B (team score for allies); the time limit then rotates to dm."""

    def actions(self, p, t):
        gs = p.c.gamestate
        sid = G.full_server_id(gs)
        out = []

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                r = fn()
                if r:
                    out.append(r)
        if t >= 0:
            once("team", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1000:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))
        if p is self.A:
            if t >= 1500:
                once("mark_p0", lambda: self.mark(p, "p0"))
            if t >= 2600:
                once("mark_p1", lambda: self.mark(p, "p1"))
            if t >= 15000:
                once("teleport", lambda: self.teleport(p, "p0", "p1"))
            if t >= 33000:
                once("score", lambda: "score")
        else:
            if t >= 15000:
                once("teleport", lambda: self.teleport(p, "p1", "p0"))
        return out

    def usercmd(self, p, t):
        ps = p.psn()
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        if p is self.A:
            if 1500 <= t < 2600:
                cmd["forwardmove"] = 127
            if 16000 <= t < 19500:
                me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0),
                      ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0)]
                st = p.ents().get(self.B.c.client_num)
                if st is not None:
                    e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
                    dx, dy, dz = e["pos.trBase[0]"] - me[0], e["pos.trBase[1]"] - me[1], e["pos.trBase[2]"] + 40.0 - me[2]
                    cmd["angles"][0] = (G.angle2short(-math.degrees(math.atan2(dz, math.hypot(dx, dy)))) - da[0]) & 0xFFFF
                    cmd["angles"][1] = (G.angle2short(math.degrees(math.atan2(dy, dx))) - da[1]) & 0xFFFF
                if t >= 16500:
                    cmd["buttons"] |= G.B_ATTACK
        else:
            if 19500 <= t < 30000:
                cmd["buttons"] |= G.B_USE
        p.cmd = cmd
        return cmd

    def actions2(self, p, t):
        out = []
        if p is self.A and t >= 3000 and "e2score" not in p.done:
            p.done.add("e2score")
            out.append("score")
        if p is self.B and t >= 4000 and "e2say" not in p.done:
            p.done.add("e2say")
            out.append("say map2")
        return out

    def usercmd2(self, p, t):
        return G.Duel.usercmd2(self, p, t)


LOG = ["+set", "g_log", "games_mp.log", "+set", "g_logSync", "1"]
CASES = {
    "gt_tdm_teams_mp_toujane": {
        "args": ["+set", "g_gametype", "tdm", "+set", "scr_friendlyfire", "1"] + LOG + ["+devmap", "mp_toujane"],
        "scenario": TdmTeams,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("C", "axis", "mp40_mp"),
                    ("D", "axis", "mp40_mp")],
        "t0": 20000, "length": 52000},
    "gt_tdm_rotate_dm_mp_toujane": {
        "args": ["+set", "g_gametype", "tdm", "+set", "scr_tdm_timelimit", "1.25", "+set", "sv_mapRotation",
                 "gametype dm map mp_carentan"] + LOG + ["+devmap", "mp_toujane"],
        "scenario": TdmRotate,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")],
        "t0": 20000, "length": 40000, "rotate": 10000},
}
