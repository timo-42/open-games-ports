"""Lane wpn_turret2 (round 2): turret contest / kill + killcam / death on the turret in tdm, ladder variants, crouched and
failed mantles. All on mp_toujane; every decision is keyed to server time and the deciding client's own snapshot.

Cases
  wpn_turret2_tdm_toujane     tdm, A and C (allies) and B (axis) around misc_mg42 308: C mounts first and A fails (twice), C kills
                              B from the turret (B follows C in the killcam), A mounts and suicides on it, A and C press use in
                              the same frame, B shoots A dead on the turret, C mounts the freed turret and kills B again.
  wpn_turret2_ladder_toujane  seven ladder trials: jump off, top-out, hang + descend, jump at the top, crouch + strafe off, tap jump,
                              reverse down.
  wpn_turret2_mantle_toujane  nineteen mantle trials: failed ledge probes (too far / too thick / not facing / can't find ledge /
                              too steep), crouched, prone, crouch-then-stand and a standing mantle.
Details: analysis/gameplay/wpn_turret2.md.
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
# turret: contest, kill + killcam, death on the turret, tdm
# ---------------------------------------------------------------------------------------------------------
MG1 = (658.3, -5.8, 220.0, 82.9)      # behind misc_mg42 308 (662 24 197), same entity string for tdm and dm
MG1C = (690.0, -22.0, 224.0, 82.9)    # second user spot behind the same turret, 32 units from MG1
BAIT = (700.0, 300.0, 233.0, 270.0)   # in front of mg 308


class TurretTdm(Base):
    """A and C (allies) fight over misc_mg42 308 while B (axis) stands at BAIT in front of the gun.
    P1: C mounts first, A tries (turret occupied), C kills B from the turret (B follows C in the killcam), C dismounts.
    P2: A mounts, C tries, A suicides on the turret (`kill`), respawns.
    P3: B has respawned and stands at BAIT; A and C press use in the same frame (A first), A mounts, B shoots A dead on
        the turret (A killcam) while A fires back.
    P4: C mounts the freed turret and kills B (second turret kill, second killcam)."""

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

    def aim_ent(self, p, other, cmd, ps, dz=40.0):
        st = p.ents().get(other.c.client_num)
        if st is not None:
            e = G.P.named_fields(st, G.P.ENTITY_FIELDS, nonzero_only=False)
            self.look_at(cmd, ps, [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"] + dz])

    @staticmethod
    def watch_death(p, t):
        """Server-time keyed life cycle from the player's own snapshots. marks: life (number of deaths seen), dead
        (time of the last death), alive (time the player is live again, None while dead). After the killcam the
        snapshot's ps is the followed player's (clientNum differs), so liveness is pm_type 0 AND own clientNum."""
        ps = p.psn()
        pt = ps.get("pm_type", 4)
        m = p.marks
        m.setdefault("life", 0)
        if pt == 6 and m.get("alive", "x") != None and (m["life"] == 0 or m.get("alive") is not None):
            m["life"] += 1
            m["dead"] = t
            m["alive"] = None
        if pt == 0 and m["life"] > 0 and m.get("alive", 0) is None and ps.get("clientNum") == p.c.client_num:
            m["alive"] = t

    @staticmethod
    def dead(p):
        return p.marks.get("life", 0) > 0 and p.marks.get("alive", 0) is None

    def more_actions(self, p, t, once, out):
        self.watch_death(p, t)
        m = p.marks
        if p is self.A:
            if t >= 3000:
                once("tp1", lambda: "setviewpos %.1f %.1f %.1f %.1f" % MG1)
            if t >= 12500:
                once("suicide", lambda: "kill")
            if m.get("life") == 1 and m.get("alive") is not None and t >= max(m["alive"], 16000):
                once("tp3", lambda: "setviewpos %.1f %.1f %.1f %.1f" % MG1)
            if t >= 28000:
                once("score", lambda: "score")
        elif p is self.B:
            if t >= 3000:
                once("tp1", lambda: "setviewpos %.1f %.1f %.1f %.1f" % BAIT)
            if m.get("life", 0) >= 1 and m.get("alive") is not None and t >= m["alive"] + 100:
                once("tp2_%d" % m["life"], lambda: "setviewpos %.1f %.1f %.1f %.1f" % BAIT)
        else:
            if t >= 3000:
                once("tp1", lambda: "setviewpos %.1f %.1f %.1f %.1f" % MG1C)
            if t >= 16000:
                once("tp3", lambda: "setviewpos %.1f %.1f %.1f %.1f" % MG1C)

    def usercmd(self, p, t):
        ps = p.psn()
        cmd = self.blank(p)
        m = p.marks
        if self.dead(p):
            cmd["buttons"] |= B_USE                          # killcam / dead: hold use to respawn
        elif p is self.A:
            if 4200 <= t < 4500 or 5000 <= t < 5300 or 7000 <= t < 7300:
                cmd["buttons"] |= B_USE                      # turret occupied by C: nothing happens
            if 10000 <= t < 10300:
                cmd["buttons"] |= B_USE                      # mount (C dismounted at 9500)
            if 10600 <= t < 12400:
                self.sweep(cmd, ps, 82.9, 10.0, 1400.0, t - 10600)
            if m.get("life") == 1 and 19000 <= t < 19300:
                cmd["buttons"] |= B_USE                      # same frame as C: A (client 0) is processed first
            if m.get("life") == 1 and 19300 <= t < 21000:
                self.sweep(cmd, ps, 82.9, 20.0, 700.0, t - 19300)
            if m.get("life") == 1 and 21000 <= t < 24000:
                self.aim_ent(p, self.B, cmd, ps)
                cmd["buttons"] |= B_ATTACK
        elif p is self.B:
            if m.get("life") == 1 and m.get("alive") is not None and 19000 <= t < 21500:
                self.aim_ent(p, self.A, cmd, ps)
                cmd["buttons"] |= B_ATTACK                   # shoot A on the turret
        else:
            if 4000 <= t < 4300:
                cmd["buttons"] |= B_USE                      # mount first (A is 200 ms later)
            if 5000 <= t < 6300:
                self.sweep(cmd, ps, 82.9, 25.0, 1300.0, t - 5000)
            if 6500 <= t < 8500:
                self.aim_ent(p, self.B, cmd, ps)
                cmd["buttons"] |= B_ATTACK                   # kill B from the turret
            if 9500 <= t < 9800:
                cmd["buttons"] |= B_USE                      # dismount
            if 10800 <= t < 11100 or 11500 <= t < 11800:
                cmd["buttons"] |= B_USE                      # A is on it: nothing happens
            if 19000 <= t < 19300:
                cmd["buttons"] |= B_USE                      # lost the same-frame race against A
            if 22500 <= t < 22800:
                cmd["buttons"] |= B_USE                      # mount the freed turret
            if 23000 <= t < 23600:
                self.sweep(cmd, ps, 82.9, 25.0, 900.0, t - 23000)
            if 23600 <= t < 26500:
                self.aim_ent(p, self.B, cmd, ps)
                cmd["buttons"] |= B_ATTACK
            if 27000 <= t < 27300:
                cmd["buttons"] |= B_USE
        p.cmd = cmd
        return cmd

    usercmd2 = usercmd


# ---------------------------------------------------------------------------------------------------------
# ladders: mount from the ground / from the air, climb, top-out, jump off, descend, strafe off, crouch
# ---------------------------------------------------------------------------------------------------------
# (name, setviewpos x y eye_z yaw). Brush numbers are the d3dbsp brush lump indices (ladder material); the player is
# dropped 26 units in front of the ladder face (explored with setviewpos + god, see analysis/gameplay/wpn_turret2.md).
LADDER_TRIALS = [
    ("jumpoff_4510", (1739.0, 474.5, 65.0, 90.0)),     # climb ~70 units, jump backwards off the ladder
    ("topout_4498", (1152.0, 1747.0, 183.0, 180.0)),   # grab from the air, climb 30 units, step over the top
    ("descend_4503", (313.0, 929.0, 130.0, 0.0)),      # mid-ladder: climb a little, hang, look down and climb down
    ("topjump_4313", (850.0, 789.0, 181.0, 180.0)),    # climb to the top with the jump button held
    ("strafe_4501", (1178.0, 2325.0, 262.0, 270.0)),   # climb, crouch, strafe off sideways, walk back into it
    ("tapjump_4502", (2356.0, 1237.0, 143.0, 180.0)),  # a short jump while on the ladder, then climb on
    ("back_4510", (1739.0, 474.5, 65.0, 90.0)),        # climb, then reverse (backward move) down the ladder
]


class Ladders(Base):
    T_START = 3000
    SLOT = 6500

    def more_actions(self, p, t, once, out):
        if p is not self.A:
            return
        if t >= 1500:
            once("god", lambda: "god")
        for i, (nm, pos) in enumerate(LADDER_TRIALS):
            if t >= self.T_START + self.SLOT * i:
                once("tp%d" % i, lambda pos=pos: "setviewpos %.1f %.1f %.1f %.1f" % pos)
        if t >= self.T_START + self.SLOT * len(LADDER_TRIALS) + 500:
            once("score", lambda: "score")

    @staticmethod
    def program(i, u, cmd):
        """Fill cmd for trial i at u ms after the teleport. Returns the pitch to look at (default 0)."""
        pitch = 0.0
        if i == 0:       # jump off backwards in mid-ladder
            if 1400 <= u < 2700:
                cmd["forwardmove"] = 127
            if 2700 <= u < 3000:
                cmd["forwardmove"] = -127
                cmd["buttons"] |= B_JUMP
        elif i == 1:     # top-out
            if 1200 <= u < 5200:
                cmd["forwardmove"] = 127
        elif i == 2:     # climb, hang, look down and descend
            if 1200 <= u < 1800:
                cmd["forwardmove"] = 127
            if 3200 <= u < 6000:
                pitch = 60.0
                cmd["forwardmove"] = 127
        elif i == 3:     # jump held at the top
            if 1200 <= u < 5000:
                cmd["forwardmove"] = 127
            if 3000 <= u < 5000:
                cmd["buttons"] |= B_JUMP
        elif i == 4:     # crouch, strafe off, walk back in
            if 1200 <= u < 2200:
                cmd["forwardmove"] = 127
            if 2200 <= u < 2600:
                cmd["forwardmove"] = 127
                cmd["buttons"] |= B_CROUCH
            if 2600 <= u < 3200:
                cmd["rightmove"] = 127
            if 3200 <= u < 3800:
                cmd["rightmove"] = -127
            if 3800 <= u < 5600:
                cmd["forwardmove"] = 127
        elif i == 5:     # tap jump
            if 1200 <= u < 5000:
                cmd["forwardmove"] = 127
            if 2000 <= u < 2100:
                cmd["buttons"] |= B_JUMP
        elif i == 6:     # reverse down
            if 1200 <= u < 2400:
                cmd["forwardmove"] = 127
            if 2400 <= u < 5000:
                cmd["forwardmove"] = -127
        return pitch

    def usercmd(self, p, t):
        cmd = self.blank(p)
        if p is self.A:
            ps = p.psn()
            da0 = ps.get("delta_angles[0]", 0)
            pitch = 0.0
            for i in range(len(LADDER_TRIALS)):
                u = t - (self.T_START + self.SLOT * i)
                if 0 <= u < self.SLOT:
                    pitch = self.program(i, u, cmd)
            cmd["angles"][0] = (G.angle2short(pitch) - da0) & 0xFFFF
        p.cmd = cmd
        return cmd

    usercmd2 = usercmd


# ---------------------------------------------------------------------------------------------------------
# mantle: failed ledge probes and crouched / prone attempts
# ---------------------------------------------------------------------------------------------------------
# (brush+side, mode, setviewpos x y eye_z yaw). The positions come from the exploration table (26 units from the brush
# face, eye 40 above its top; brush indices of the d3dbsp brush lump). The outcome of every trial was read from
# Mantle_DebugPrint (0x080dd0f8) under gdb on the original: see analysis/gameplay/wpn_turret2.md.
MANTLE_TRIALS = [
    ("4365n", "jump", (61.0, 1982.0, 266.0, 270.0)),      # Ledge is too far away (all three probe heights)
    ("4386w", "jump", (954.0, 1546.0, 230.0, 0.0)),       # too far away
    ("4392s", "jump", (-263.0, 958.0, 116.0, 90.0)),      # too far away
    ("4375w", "jump", (588.0, 23.5, 241.0, 0.0)),         # too far away
    ("4390w", "jump", (438.0, 1544.0, 84.0, 0.0)),        # too far away
    ("4368n", "jump", (761.5, 2494.0, 270.0, 270.0)),     # too far away
    ("4381s", "jump", (1452.0, 1476.0, 130.0, 90.0)),     # Player is not facing mantle surface
    ("4383e", "jump", (1197.2037353515625, 1797.99072265625, 150.0, 180.0)),   # not facing
    ("4365s", "jump", (61.0, 1890.0, 266.0, 90.0)),       # Mantle brush is too thick
    ("4370n", "jump", (2651.5, 986.0, 255.0, 270.0)),     # too thick
    ("4374n", "jump", (797.0, 56.0, 202.0, 270.0)),       # Can't find ledge
    ("4375n", "jump", (661.5, 56.0, 241.0, 270.0)),       # Can't find ledge
    ("4383n", "jump", (1144.1018676757812, 1830.0, 150.0, 270.0)),   # Can't find ledge / too steep
    ("4366s", "jump", (1265.2738647460938, 1765.052734375, 150.0, 90.0)),   # steep, far and available mixed
    ("4376w", "jump", (1397.0, 1589.5, 130.0, 0.0)),      # Can't find ledge, too steep
    ("4383w", "crouch", (1091.0, 1797.99072265625, 150.0, 0.0)),    # crouched all the time: Player not standing
    ("4364w", "prone", (779.0, 482.0, 256.0, 0.0)),       # prone: Player not standing
    ("4392w", "uncrouch", (-298.0, 1026.0, 116.0, 0.0)),  # crouched, released, then jump: mantles once standing
    ("4383w", "jump", (1091.0, 1797.99072265625, 150.0, 0.0)),      # the standing mantle for comparison
]


class MantleFail(Base):
    T_START = 3000
    SLOT = 2800

    def more_actions(self, p, t, once, out):
        if p is not self.A:
            return
        if t >= 1500:
            once("god", lambda: "god")
        for i, (nm, mode, pos) in enumerate(MANTLE_TRIALS):
            if t >= self.T_START + self.SLOT * i:
                once("tp%d" % i, lambda pos=pos: "setviewpos %.1f %.1f %.1f %.1f" % pos)
        if t >= self.T_START + self.SLOT * len(MANTLE_TRIALS) + 500:
            once("score", lambda: "score")

    def usercmd(self, p, t):
        cmd = self.blank(p)
        if p is self.A:
            for i, (nm, mode, pos) in enumerate(MANTLE_TRIALS):
                u = t - (self.T_START + self.SLOT * i)
                if not 0 <= u < self.SLOT:
                    continue
                walk = 1000 <= u < 2400
                if mode == "crouch" and u < 2400:
                    cmd["buttons"] |= B_CROUCH
                if mode == "prone" and u < 2400:
                    cmd["buttons"] |= G.B_PRONE
                if mode == "uncrouch" and u < 1100:
                    cmd["buttons"] |= B_CROUCH
                if walk:
                    cmd["forwardmove"] = 127
                    if u >= 1200:
                        cmd["buttons"] |= B_JUMP
        p.cmd = cmd
        return cmd

    usercmd2 = usercmd


CASES = {
    "wpn_turret2_tdm_toujane": {"args": ["+set", "g_gametype", "tdm"] + COMMON[3:], "scenario": TurretTdm, "t0": 20000,
                                "length": 30000, "players": [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"),
                                                             ("C", "allies", "m1carbine_mp")]},
    "wpn_turret2_ladder_toujane": {"args": COMMON, "scenario": Ladders, "t0": 20000,
                                   "length": 3000 + 6500 * len(LADDER_TRIALS) + 2000},
    "wpn_turret2_mantle_toujane": {"args": COMMON, "scenario": MantleFail, "t0": 20000,
                                   "length": 3000 + 2800 * len(MANTLE_TRIALS) + 2000},
}
