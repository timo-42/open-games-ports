"""Lane gt_ctf: Capture The Flag (g_gametype ctf) on mp_toujane.

Flag entities (trigger_radius, radius 48, height 128) from the mp_toujane entity lump [O]:
  allied_flag origin (4, 805, -13), axis_flag origin (2263, 2480, 72).
Every action is keyed to the deciding client's own snapshot time (relative to T0), like the Duel scenario.
"""
import math

import gameplay as G

ALLIED_FLAG = (4.0, 805.0, -13.0)
AXIS_FLAG = (2263.0, 2480.0, 72.0)
# duel positions east of the allied flag (open floor, line of sight, verified on the original)
AXIS_SPAWN = (2383.0, 2147.0, 84.0)  # mp_ctf_spawn_axis
POS_A = (124.0, 840.0, -52.0)  # known free: A stood here in an earlier run


def tele(pos, yaw=0.0, dz=20.0):
    return "setviewpos %.1f %.1f %.1f %.1f" % (pos[0], pos[1], pos[2] + dz, yaw)


class CtfScen(G.Duel):
    """Timeline (relative to T0), all positions and times fixed:
      3000  A teleports onto the axis flag: pickup (enemy touch).
      7000  A teleports onto the allied flag (at its base) while carrying: capture.
      9000  B teleports onto the allied flag: pickup.
      12500 A (east, 120 units away) shoots B; B dies carrying the flag and the drop lands in a trigger_hurt: the flag
            returns at once (level.flag_returners).
      15000+ B holds use and respawns.
      26000 B takes the allied flag again; 29000 B suicides (`kill`): the flag drops at B's position, atbase false.
      32000 A touches the dropped flag: return (+2).
      44000 B takes the flag a third time; 36000 A takes the axis flag; both disconnect carrying (dropFlag)."""

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
        if p is self.A:
            if t >= 3000:
                once("take_axis_flag", lambda: tele(AXIS_FLAG, 90.0))
            if t >= 7000:  # carry it home: the allied flag is at its base, so this captures
                once("capture", lambda: tele((ALLIED_FLAG[0] + 30.0, ALLIED_FLAG[1], ALLIED_FLAG[2]), 0.0))
            if t >= 8500:
                once("score1", lambda: "score")
            if t >= 11000:  # A east of B (B stands at the flag's north side)
                once("face_b", lambda: tele(POS_A, 180.0, 60.0))
            if t >= 36000:
                once("score2", lambda: "score")
            if t >= 37000:  # take the axis flag again and keep it until disconnect
                once("take_axis_again", lambda: tele(AXIS_FLAG, 90.0))
        else:
            if t >= 9000:
                once("take_allied_flag", lambda: tele((ALLIED_FLAG[0], ALLIED_FLAG[1] + 35.0, ALLIED_FLAG[2]), 0.0))
            if t >= 22000:
                once("score2", lambda: "score")
            if t >= 26000:  # second pickup, after the respawn
                once("take_again", lambda: tele((ALLIED_FLAG[0], ALLIED_FLAG[1] + 35.0, ALLIED_FLAG[2]), 0.0))
            if t >= 27500:  # axis capture at the axis flag's base
                once("axis_capture", lambda: tele((AXIS_FLAG[0] - 30.0, AXIS_FLAG[1], AXIS_FLAG[2]), 0.0))
            if t >= 29000:  # third pickup
                once("take_third", lambda: tele((ALLIED_FLAG[0], ALLIED_FLAG[1] + 35.0, ALLIED_FLAG[2]), 0.0))
            if t >= 30500:  # carry it to an axis spawn point and suicide there
                once("to_spawn", lambda: tele(AXIS_SPAWN, 0.0, 10.0))
            if t >= 32000:
                once("suicide", lambda: "kill")
            if t >= 46500:  # fourth pickup: still carrying at disconnect
                once("take_fourth", lambda: tele((ALLIED_FLAG[0], ALLIED_FLAG[1] + 35.0, ALLIED_FLAG[2]), 0.0))
        return out

    def usercmd(self, p, t):
        ps = p.psn()
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]

        def look(yaw, pitch):
            cmd["angles"][0] = (G.angle2short(pitch) - da[0]) & 0xFFFF
            cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF

        if p is self.A:
            st = p.ents().get(self.B.c.client_num)
            if 12000 <= t < 15500:  # aim at B's entity in A's own snapshot and fire
                me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0),
                      ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0)]
                if st is not None:
                    e = G.P.named_fields(st, G.P.ENTITY_FIELDS, nonzero_only=False)
                    tgt = [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"] + 40.0]
                    dx, dy, dz = tgt[0] - me[0], tgt[1] - me[1], tgt[2] - me[2]
                    look(math.degrees(math.atan2(dy, dx)), -math.degrees(math.atan2(dz, math.hypot(dx, dy))))
                if t >= 12500:
                    cmd["buttons"] |= G.B_ATTACK
        else:
            if 15000 <= t < 25500 or 32500 <= t < 46000:  # hold use: respawn as soon as the script polls it
                cmd["buttons"] |= G.B_USE
        p.cmd = cmd
        return cmd


CASES = {
    "ctf_flag_cycle_mp_toujane": {
        "args": ["+set", "g_gametype", "ctf", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
                 "+devmap", "mp_toujane"],
        "scenario": CtfScen,
        "t0": 20000, "length": 52000,
    },
}
