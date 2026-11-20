"""Lane wpn_mantle_turret: mantling, ladders and stationary MG42 turrets on mp_toujane (dm).

Cases
  wpn_mantle_ladder_toujane  A (god mode, so falls and the out-of-world trigger_hurt do not kill) is teleported with
                             `setviewpos` in front of two ladder brushes and twelve mantle_on / mantle_over brushes and
                             walks / jumps into each. The table was found by exploration (see
                             analysis/gameplay/wpn_mantle_turret.md); every step is keyed to server time.
  wpn_turret_mg42_toujane    A mounts misc_mg42 308 with the use button, sweeps and fires, shoots B (killed by the
                             turret), dismounts and mounts misc_mg42 311; B respawns, mounts misc_mg42 517, fires and
                             dismounts.

Setviewpos z is the eye height: the origin is placed at z - 59 and the player then falls to the floor.
"""
import math
import gameplay as G

B_JUMP, B_USE, B_ATTACK, B_CROUCH = G.B_JUMP, G.B_USE, G.B_ATTACK, G.B_CROUCH
COMMON = ["+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1", "+devmap", "mp_toujane"]


class Base(G.Duel):
    """Both clients pick team and weapon through the stock menus (as Duel); subclasses add the timeline."""

    def menus(self, p, t, once, out):
        gs = p.c.gamestate
        sid = G.full_server_id(gs)
        if t >= 0:
            once("team", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1000:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))

    def actions(self, p, t):
        out = []

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                r = fn()
                if r:
                    out.append(r)
        self.menus(p, t, once, out)
        self.more_actions(p, t, once, out)
        return out

    def more_actions(self, p, t, once, out):
        pass

    @staticmethod
    def blank(p):
        return {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
                "offhand": 0}

    actions2 = lambda self, p, t: []


# ---------------------------------------------------------------------------------------------------------
# mantle + ladder
# ---------------------------------------------------------------------------------------------------------
# (brush, setviewpos x y z yaw): the player falls onto the floor in front of the brush, then walks forward with the
# jump button held. Brush numbers are indices into the d3dbsp brush lump of maps/mp/mp_toujane.d3dbsp (the mantle_on
# and mantle_over materials).
LADDERS = [(1739.0, 474.5, 65.0, 90.0),    # ladder brush 4510 (z -1..160) from the south: climbs 6 -> 160
           (2356.0, 1237.0, 143.0, 180.0)]  # ladder brush 4502 (z 77..257) from the east: climbs 84 -> 256
MANTLES = [(4364, (779.0, 482.0, 228.0, 0.0)), (4365, (28.0, 1936.0, 238.0, 0.0)),
           (4366, (1265.2738647460938, 1765.052734375, 122.0, 90.0)),
           (4366, (1265.2738647460938, 1875.1234130859375, 122.0, 270.0)),
           (4381, (1400.0, 1541.0, 102.0, 0.0)), (4381, (1504.0, 1541.0, 102.0, 180.0)),
           (4383, (1144.1018676757812, 1765.9814453125, 122.0, 90.0)), (4386, (1018.0, 1576.0, 202.0, 270.0)),
           (4390, (485.0, 1510.0, 56.0, 90.0)), (4390, (485.0, 1578.0, 56.0, 270.0)),
           (4392, (-298.0, 1026.0, 88.0, 0.0)), (4392, (-228.0, 1026.0, 88.0, 180.0))]


class MantleLadder(Base):
    T_START = 3000
    LADDER_DUR = 5000
    MANTLE_DUR = 3000

    def schedule(self):
        """[(t_start, kind, pos)]"""
        out, t = [], self.T_START
        for pos in LADDERS:
            out.append((t, "ladder", pos))
            t += self.LADDER_DUR
        for _, pos in MANTLES:
            out.append((t, "mantle", pos))
            t += self.MANTLE_DUR
        return out

    def more_actions(self, p, t, once, out):
        if p is not self.A:
            return
        if t >= 1500:
            once("god", lambda: "god")
        for i, (ts, kind, pos) in enumerate(self.schedule()):
            if t >= ts:
                once("tp%d" % i, lambda pos=pos: "setviewpos %.1f %.1f %.1f %.1f" % pos)
        if t >= self.schedule()[-1][0] + self.MANTLE_DUR + 500:
            once("score", lambda: "score")

    def usercmd(self, p, t):
        cmd = self.blank(p)
        if p is self.A:
            for ts, kind, pos in self.schedule():
                u = t - ts
                if kind == "ladder" and 1400 <= u < 4400:
                    cmd["forwardmove"] = 127
                if kind == "mantle" and 1100 <= u < 2700:
                    cmd["forwardmove"] = 127
                    if u >= 1300:
                        cmd["buttons"] |= B_JUMP
        p.cmd = cmd
        return cmd

    usercmd2 = usercmd


# ---------------------------------------------------------------------------------------------------------
# turrets
# ---------------------------------------------------------------------------------------------------------
MG1 = (658.3, -5.8, 220.0, 82.9)      # behind misc_mg42 308 (662 24 197); floor 158.125
MG2 = (-104.4, 1375.6, 68.0, 330.7)   # behind misc_mg42 311 (-80 1362 33); floor ~6
MG3 = (2188.0, 2413.0, 130.0, 214.5)  # behind misc_mg42 517 (2165 2397 89); floor 62.58
BAIT = (700.0, 300.0, 233.0, 270.0)   # in front of mg 308, floor 171.125


class Turret(Base):
    """All decisions are keyed to server time and the deciding client's own snapshot."""

    def more_actions(self, p, t, once, out):
        if p is self.A:
            if t >= 3000:
                once("tp1", lambda: "setviewpos %.1f %.1f %.1f %.1f" % MG1)
            if t >= 11000:
                once("tp2", lambda: "setviewpos %.1f %.1f %.1f %.1f" % MG2)
            if t >= 30000:
                once("score", lambda: "score")
        else:
            if t >= 3000:
                once("tp1", lambda: "setviewpos %.1f %.1f %.1f %.1f" % BAIT)
            if t >= 14000 and p.psn().get("pm_type", 4) == 0 and "tp_mg3" not in p.done:
                p.done.add("tp_mg3")   # first snapshot with B alive again (after the killcam)
                p.marks["tp_t"] = t
                out.append("setviewpos %.1f %.1f %.1f %.1f" % MG3)

    def look_at(self, cmd, ps, target):
        me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0), ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0)]
        dx, dy, dz = target[0] - me[0], target[1] - me[1], target[2] - me[2]
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        cmd["angles"][0] = (G.angle2short(-math.degrees(math.atan2(dz, math.hypot(dx, dy)))) - da[0]) & 0xFFFF
        cmd["angles"][1] = (G.angle2short(math.degrees(math.atan2(dy, dx))) - da[1]) & 0xFFFF

    def sweep(self, cmd, ps, yaw0, amp, period, t):
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        yaw = yaw0 + amp * math.sin(2 * math.pi * t / period)
        cmd["angles"][0] = (G.angle2short(0.0) - da[0]) & 0xFFFF
        cmd["angles"][1] = (G.angle2short(yaw) - da[1]) & 0xFFFF

    def usercmd(self, p, t):
        ps = p.psn()
        cmd = self.blank(p)
        if p is self.A:
            if 4200 <= t < 4500:
                cmd["buttons"] |= B_USE                      # mount mg 308
            if 5000 <= t < 6500:                             # sweep the barrel
                self.sweep(cmd, ps, 82.9, 25.0, 1500.0, t - 5000)
            if 6500 <= t < 8500:                             # aim at B in our own snapshot and fire
                st = p.ents().get(self.B.c.client_num)
                if st is not None:
                    e = G.P.named_fields(st, G.P.ENTITY_FIELDS, nonzero_only=False)
                    self.look_at(cmd, ps, [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"] + 40.0])
                cmd["buttons"] |= B_ATTACK
            if 10200 <= t < 10500:
                cmd["buttons"] |= B_USE                      # dismount
            if 12200 <= t < 12500:
                cmd["buttons"] |= B_USE                      # mount mg 311
            if 13500 <= t < 17500:
                self.sweep(cmd, ps, 330.7, 30.0, 2000.0, t - 13500)
                if t >= 14000:
                    cmd["buttons"] |= B_ATTACK
            if 18500 <= t < 18800:
                cmd["buttons"] |= B_USE                      # dismount
        else:
            tp = p.marks.get("tp_t")
            if t >= 9000 and tp is None:
                cmd["buttons"] |= B_USE                      # killcam: hold use to respawn
            if tp is not None:
                u = t - tp
                if 1200 <= u < 1500:
                    cmd["buttons"] |= B_USE                  # mount mg 517
                if 2500 <= u < 5500:
                    self.sweep(cmd, ps, 214.5, 20.0, 1000.0, u - 2500)
                    if u >= 3000:
                        cmd["buttons"] |= B_ATTACK
                if 6000 <= u < 6300:
                    cmd["buttons"] |= B_USE                  # dismount
        p.cmd = cmd
        return cmd

    usercmd2 = usercmd


CASES = {
    "wpn_mantle_ladder_toujane": {"args": COMMON, "scenario": MantleLadder, "t0": 20000, "length": 56000},
    "wpn_turret_mg42_toujane": {"args": COMMON, "scenario": Turret, "t0": 20000, "length": 34000},
}
