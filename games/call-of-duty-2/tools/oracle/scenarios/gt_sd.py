"""Search & Destroy lane (gt_sd): rounds, bomb plant/explode/defuse, timeout, elimination, score limit.

Cases (all g_gametype sd, devmap so setviewpos works; sv_cheats comes from devmap):
  gt_sd_rounds_mp_toujane   join -> 'reset' round -> map_restart; R1 plant at site A + explosion kills the planter;
                            R2 plant at site B + defuse by the defender; R3 attacker shoots the defender (elimination)
                            -> scorelimit 2 -> endMap (intermission); the session ends during the intermission.
  gt_sd_rounds_mp_carentan  join -> reset; R1 two interrupted plants, round time (30 s) expires; R2 plant, defender
                            stands in the blast, both die (planting team wins); R3 defender suicides -> scorelimit
                            -> endMap (intermission); the session ends during the intermission.

Determinism: every decision is a function of the deciding client's own snapshot server time relative to the level
start time (configstring 13; `map_restart` rewrites it with a `d 13 <time>` server command) and of the number of such
restarts it has seen since its current gamestate. Positions are set with setviewpos from fixed coordinates (z is the eye
height; the player falls to the ground), bomb sites were located from the map's entity lump and brush-model bounds.
"""
import math
import re
import gameplay as G
import cod2proto as P

B_USE, B_ATTACK = G.B_USE, G.B_ATTACK


def menu_names(gs):
    return {int(k) - 0x4DE: v for k, v in gs["configstrings"].items() if 0x4DE <= int(k) < 0x4DE + 0x20}


def team_menu(gs):
    return sorted(i for i, v in menu_names(gs).items() if v.startswith("team_"))[0]


def weapon_menu(gs, team):
    ws = sorted((i, v) for i, v in menu_names(gs).items() if v.startswith("weapon_"))
    if team == "axis":
        return [i for i, v in ws if v == "weapon_german"][0]
    return [i for i, v in ws if v != "weapon_german"][0]


def level_rounds(p):
    """(number of 'd 13' restarts since the current gamestate, level start time) from the client's own data."""
    gs = p.c.gamestate
    base = int(gs["configstrings"].get(13, "0") or 0)
    n = 0
    for seq, txt in sorted(p.c.server_cmds.items()):
        if seq <= gs["reliableSequence"]:
            continue
        m = re.match(rb"d 13 (\d+)$", txt.strip())
        if m:
            n += 1
            base = int(m.group(1))
    return n, base


def tp(x, y, z, yaw):
    return "setviewpos %.1f %.1f %.1f %.1f" % (x, y, z, yaw)


class SdPlan(G.Duel):
    """PLAN[epoch] = {"acts": [(round n, tag, tr_ms, text)], "hold": [(n, tag, tr0, tr1, buttons)],
    "aim": [(n, tag, tr0, tr1, fire_from)]}; tr is the time since the level (re)start. Round 0 is the join phase
    (team menu at 2000 ms/1000 ms after the level start of epoch 2/1, weapon menu 1000 ms later)."""
    PLAN = {}
    JOIN = {1: (0, 1000), 2: (2000, 3000)}

    def _acts(self, p, t, epoch):
        gs = p.c.gamestate
        out = []
        n, base = level_rounds(p)
        tr = t + p.anchor - base
        if n == 0:
            jt, jw = self.JOIN[epoch]
            sid = G.full_server_id(gs)
            for key, at, fn in (("team", jt, lambda: "mr %d %d %s" % (sid, team_menu(gs), p.team)),
                                ("weapon", jw, lambda: "mr %d %d %s" % (sid, weapon_menu(gs, p.team), p.weapon_choice))):
                k = "%d:%s" % (epoch, key)
                if t >= at and k not in p.done:
                    p.done.add(k)
                    out.append(fn())
        for i, (rn, tag, at, text) in enumerate(self.PLAN.get(epoch, {}).get("acts", [])):
            k = "%d:a%d" % (epoch, i)
            if rn == n and tag == p.tag and tr >= at and k not in p.done:
                p.done.add(k)
                out.append(text)
        return out

    def _cmd(self, p, t, epoch):
        ps = p.psn()
        n, base = level_rounds(p)
        tr = t + p.anchor - base
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        plan = self.PLAN.get(epoch, {})
        for rn, tag, t0, t1, btn in plan.get("hold", []):
            if rn == n and tag == p.tag and t0 <= tr < t1:
                cmd["buttons"] |= btn
        for rn, tag, t0, t1, fire in plan.get("aim", []):
            if rn == n and tag == p.tag and t0 <= tr < t1:
                other = self.B if p is self.A else self.A
                me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0),
                      ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0)]
                st = p.ents().get(other.c.client_num)
                if st is not None:
                    e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
                    tgt = [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"] + 40.0]
                    dx, dy, dz = tgt[0] - me[0], tgt[1] - me[1], tgt[2] - me[2]
                    yaw = math.degrees(math.atan2(dy, dx))
                    pitch = -math.degrees(math.atan2(dz, math.hypot(dx, dy)))
                    cmd["angles"][0] = (G.angle2short(pitch) - da[0]) & 0xFFFF
                    cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF
                if tr >= fire:
                    cmd["buttons"] |= B_ATTACK
        p.cmd = cmd
        return cmd

    def actions(self, p, t):
        return self._acts(p, t, 1)

    def actions2(self, p, t):
        return self._acts(p, t, 2)

    def usercmd(self, p, t):
        return self._cmd(p, t, 1)

    def usercmd2(self, p, t):
        return self._cmd(p, t, 2)


# --- mp_toujane: bomb zone A origin (1047,2750,91) box x 909..1183 y 2590..2917 z 41..143 (wreck top at z 124);
#     zone B origin (2834,1634,92) box x 2692..2972 y 1514..1754 z 44..132; bomb trigger box +-40 x/y, 0..56 above the
#     plant point. Attacker spawns near (-100,1100), defender spawns near (2200,2500).
class SdToujane(SdPlan):
    PLAN = {
        1: {"acts": [
            (1, "A", 1000, tp(1047, 2650, 300, 90)),          # R1: A on the wreck inside zone A
            (1, "B", 30000, "score"),
            (2, "A", 1000, tp(2834, 1560, 300, 90)),          # R2: A on the ground inside zone B
            (2, "B", 7500, tp(2806, 1565, 300, 0)),           # B next to the planted bomb
            (2, "A", 17000, "say planted"),
            (3, "A", 1000, tp(-65, 1160, 100, 180)),          # R3: face to face in the attacker spawn area
            (3, "B", 1000, tp(-185, 1160, 100, 0)),
            (3, "A", 12000, "score"),
        ],
            "hold": [(1, "A", 2000, 16000, B_USE),
                     (2, "A", 2000, 8000, B_USE),             # A releases use before B starts (one user at a time)
                     (2, "B", 9000, 22000, B_USE)],
            "aim": [(3, "A", 3000, 9000, 3500)]},
    }


# --- mp_carentan: zone A origin (-108,2540,18) box x -351..122 y 2293..2769 z -30..66 (ground z -32 at
#     (-108,2400)); (-108,2540) is a prop at z 76, zone B's center is a kill zone for players.
class SdCarentan(SdPlan):
    PLAN = {
        1: {"acts": [
            (1, "A", 1000, tp(-108, 2400, 100, 90)),          # R1: A starts two plants and releases early
            (1, "B", 3000, "say hold"),
            (2, "A", 1000, tp(-108, 2400, 100, 90)),          # R2: plant; B stands in the blast radius
            (2, "B", 9000, tp(-40, 2400, 100, 90)),
            (3, "B", 3000, "kill"),                           # R3: defender suicide -> elimination
            (3, "A", 8000, "score"),
        ],
            "hold": [(1, "A", 2000, 4000, B_USE), (1, "A", 6000, 8500, B_USE),
                     (2, "A", 2000, 8000, B_USE)],
            "aim": []},
    }


ARGS = ["+set", "g_gametype", "sd", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
        "+set", "scr_sd_bombtimer", "30", "+set", "scr_sd_scorelimit", "2"]
PL = [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")]
CASES = {
    "gt_sd_rounds_mp_toujane": {
        "args": ARGS + ["+devmap", "mp_toujane"],
        "scenario": SdToujane, "players": PL, "t0": 20000, "length": 93000},
    "gt_sd_rounds_mp_carentan": {
        "args": ARGS + ["+set", "scr_sd_roundlength", "0.5", "+devmap", "mp_carentan"],
        "scenario": SdCarentan, "players": PL, "t0": 20000, "length": 104000},
}
