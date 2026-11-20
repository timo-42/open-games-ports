"""Lane gt_hq: Headquarters (g_gametype hq) on mp_toujane.

Timeline is driven by the first snapshot (of the deciding client) that contains a radio entity, never by the wall
clock. Radio positions are the ten level.radio[] origins of iw_15 maps/mp/mp_toujane.gsc; which one appears (and
when) depends on randomint(), which gameplay.py pins through G_InitGame's seed.
"""
import math
import gameplay as G
import cod2proto as P

TOUJANE_RADIOS = [(285.332, 471.947, 12.1747), (-7.80745, 1100.36, -11.0072), (1282.54, 731.845, -1.99999),
                  (2206.32, 411.609, 54.6547), (758.63, 1725.03, 148), (1558.4, 1569.03, 96.0133),
                  (986.298, 3040.61, 58.7701), (2295.88, 2483.53, 70.7077), (2971.51, 1570.41, 45.4606),
                  (1758, 653.443, 176)]


def find_radios(p):
    """{entnum: (x,y,z)} of entities in p's own latest snapshot whose trBase matches a map radio origin."""
    out = {}
    for n, st in p.ents().items():
        e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
        if e.get("eType") != 6:   # ET_MODEL script_model; temp-entity fx at the radio origin are not radios
            continue
        o = (e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"])
        for r in TOUJANE_RADIOS:
            if abs(o[0] - r[0]) < 1.0 and abs(o[1] - r[1]) < 1.0 and abs(o[2] - r[2]) < 1.0:
                out[n] = r
    return out


class HQ(G.Duel):
    """A (allies) captures the first radio, holds it; B (axis) destroys it; the radio relocates; A and B contest
    the second radio, A leaves, B captures it."""

    b_dx = 30.0   # B's x offset from the radio origin

    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.st = {p.tag: {"seen": {}, "t": {}} for p in players}

    def radio_state(self, p, t):
        """Track radios seen in p's own snapshots: first-seen time (relative) per entity number, in order."""
        s = self.st[p.tag]
        rs = find_radios(p)
        for n, pos in rs.items():
            if n not in s["seen"]:
                s["seen"][n] = (len(s["seen"]) + 1, t, pos)
        return s, rs

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
            once("team", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1000:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))
        s, rs = self.radio_state(p, t)
        order = sorted(s["seen"].items(), key=lambda kv: kv[1][0])
        r1 = order[0][1] if order else None
        r2 = order[1][1] if len(order) > 1 else None
        self.notes["radios_" + p.tag] = {str(k): list(v[2]) + [v[1]] for k, v in order}

        dx = self.b_dx if p is self.B else 30.0

        def tp(pos, yaw=0.0):
            return "setviewpos %.1f %.1f %.1f %.1f" % (pos[0] + dx, pos[1], pos[2] + 25.0, yaw)

        if r1:
            d = t - r1[1]
            if p is self.A:
                if d >= 500:
                    once("tp_r1", lambda: tp(r1[2], 180.0))
                if d >= 16500:
                    once("score1", lambda: "score")   # after the capture and some score ticks
            else:
                if d >= 18500:
                    once("tp_r1b", lambda: tp(r1[2], 180.0))   # destroy the allied HQ
        if r2:
            d = t - r2[1]
            if d >= 500:
                once("tp_r2", lambda: tp(r2[2], 180.0))
            if p is self.A and d >= 6500:
                once("leave", lambda: tp(r1[2], 0.0))
            if p is self.B and d >= 19000:
                once("score2", lambda: "score")
            if p is self.A and d >= 21000:
                once("say", lambda: "say hq")
        return out

    def usercmd(self, p, t):
        p.cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
                 "offhand": 0}
        return p.cmd

    def actions2(self, p, t):
        return []

    usercmd2 = usercmd



class HQFight(HQ):
    """A captures the first radio. B walks into the radio area, A shoots B dead (killcam, attacker respawn timer, the
    defenders-do-not-respawn rule), B respawns and destroys the HQ; the radio relocates. The aim comes from A's own
    snapshot as in Duel."""

    b_dx = -70.0

    def actions(self, p, t):
        out = super().actions(p, t)
        s = self.st[p.tag]
        order = sorted(s["seen"].items(), key=lambda kv: kv[1][0])
        r1 = order[0][1] if order else None
        if r1 and p is self.B:
            ps = p.psn()
            if t >= r1[1] + 33000 and ps.get("pm_type", 0) == 0 and "tp_r1c" not in p.done:
                p.done.add("tp_r1c")
                out.append("setviewpos %.1f %.1f %.1f %.1f" % (r1[2][0] + self.b_dx, r1[2][1], r1[2][2] + 25.0, 0.0))
        return out

    def usercmd(self, p, t):
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
               "offhand": 0}
        order = sorted(self.st[p.tag]["seen"].items(), key=lambda kv: kv[1][0])
        r1 = order[0][1] if order else None
        if r1:
            d = t - r1[1]
            ps = p.psn()
            if p is self.A and 19000 <= d < 24000:
                da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
                me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0),
                      ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0)]
                st = p.ents().get(self.B.c.client_num)
                if st is not None:
                    e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
                    tgt = [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"] + 40.0]
                    dx, dy, dz = tgt[0] - me[0], tgt[1] - me[1], tgt[2] - me[2]
                    yaw = math.degrees(math.atan2(dy, dx))
                    pitch = -math.degrees(math.atan2(dz, math.hypot(dx, dy)))
                    cmd["angles"][0] = (G.angle2short(pitch) - da[0]) & 0xFFFF
                    cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF
                if d >= 19500:
                    cmd["buttons"] |= G.B_ATTACK
            if p is self.B and 26000 <= d < 80000:
                cmd["buttons"] |= G.B_USE      # respawn as soon as the script polls useButtonPressed
        p.cmd = cmd
        return cmd

CASES = {
    "gt_hq_capture_mp_toujane": {
        "args": ["+set", "g_gametype", "hq", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
                 "+devmap", "mp_toujane"],
        "scenario": HQ,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")],
        "t0": 20000, "length": 90000, "rotate": 0, "bot_rcon": None, "mod": None,
    },
    "gt_hq_fight_mp_toujane": {
        "args": ["+set", "g_gametype", "hq", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
                 "+devmap", "mp_toujane"],
        "scenario": HQFight,
        "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")],
        "t0": 20000, "length": 90000, "rotate": 0, "bot_rcon": None, "mod": None,
    },
}
