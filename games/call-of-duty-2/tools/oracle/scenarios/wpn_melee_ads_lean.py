"""Lane wpn_melee_ads_lean: aim down sights (B_ADS), lean left/right (B_LEANLEFT/B_LEANRIGHT), melee (B_MELEE).

Cases (all dm on mp_toujane, devmap so that setviewpos works):
  wpn_melee_ads_lean_ads_lean_dm_toujane  stand/crouch/prone x ADS x lean x movement, ADS fire (spread), lean at a wall
  wpn_melee_ads_lean_melee_dm_toujane     melee miss, melee hit, melee kill, killcam/respawn, melee kill the other way

Every decision is a function of the deciding client's own latest snapshot (own velocity/health/position, the other
player's entity position) and its server time, as for Duel.
"""
import math

import gameplay as G

P = G.P
B_ATTACK, B_MELEE, B_USE = G.B_ATTACK, G.B_MELEE, G.B_USE
B_LL, B_LR, B_PRONE, B_CROUCH, B_JUMP, B_ADS = G.B_LEANLEFT, G.B_LEANRIGHT, G.B_PRONE, G.B_CROUCH, G.B_JUMP, G.B_ADS


def _base_cmd(p):
    return {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}


def _look(p, cmd, yaw, pitch):
    ps = p.psn()
    cmd["angles"][0] = (G.angle2short(pitch) - ps.get("delta_angles[0]", 0)) & 0xFFFF
    cmd["angles"][1] = (G.angle2short(yaw) - ps.get("delta_angles[1]", 0)) & 0xFFFF


class Base(G.Duel):
    def actions2(self, p, t):
        return []

    def usercmd2(self, p, t):
        p.cmd = _base_cmd(p)
        return p.cmd

    def other(self, p):
        return self.B if p is self.A else self.A

    def other_pos(self, p):
        """(x, y, z) of the other player's entity in p's own snapshot, or None."""
        st = p.ents().get(self.other(p).c.client_num)
        if st is None:
            return None
        e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
        return [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"]]

    def tele_to_other(self, p, dist, z_add=8.0):
        """setviewpos `dist` units in front of the other player, facing it (from p's own snapshot)."""
        o = self.other_pos(p)
        ps = p.psn()
        me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0)]
        if not o:
            return None
        dx, dy = me[0] - o[0], me[1] - o[1]
        n = math.hypot(dx, dy) or 1.0
        x, y = o[0] + dx / n * dist, o[1] + dy / n * dist
        yaw = math.degrees(math.atan2(o[1] - y, o[0] - x))
        text = "setviewpos %.1f %.1f %.1f %.1f" % (x, y, o[2] + z_add, yaw)
        self.notes.setdefault("teleports", {}).setdefault(p.tag, []).append(text)
        return text


# =============================================================================================
# case 1: ADS and lean
# =============================================================================================
class AdsLean(Base):
    # (start, end, buttons, forward, right) relative to T0; phases for the pure ADS/lean matrix
    PHASES = [
        (1500, 2500, B_ADS, 0, 0),                          # stand ADS
        (2500, 3500, B_ADS, 127, 0),                        # stand ADS walk forward
        (3500, 4000, 0, 0, 0),
        (4000, 5000, B_LL, 0, 0),                           # lean left
        (5000, 6000, B_LR, 0, 0),                           # lean right
        (6000, 6500, B_LL | B_LR, 0, 0),                    # both
        (6500, 7500, B_ADS | B_LL, 0, 0),                   # ADS + lean left
        (7500, 8500, B_ADS | B_LR, 0, 127),                 # ADS + lean right + strafe
        (8500, 9500, B_CROUCH | B_LL, 0, 0),                # crouch lean left
        (9500, 10500, B_CROUCH | B_ADS | B_LR, 127, 0),     # crouch ADS lean right + forward
        (10500, 11500, B_PRONE, 0, 0),                      # prone
        (11500, 12500, B_PRONE | B_ADS, 0, 0),              # prone ADS
        (12500, 13500, B_PRONE | B_ADS, 127, 0),            # prone ADS forward
        (13500, 14500, B_PRONE | B_LL, 0, 0),               # prone lean left
        (14500, 15500, B_PRONE | B_LR | B_ADS, 0, 127),     # prone lean right ADS strafe
        (15500, 16500, B_CROUCH, 0, 0),
        (16500, 17000, 0, 0, 0),
        (17000, 18500, B_ADS | B_ATTACK, 0, 0),             # ADS fire (spread)
        (18500, 19000, 0, 0, 0),
        (19000, 20000, B_ATTACK, 0, 0),                     # hip fire for comparison
    ]
    WALL0 = 20500        # walk into a wall, then lean against it
    WALL_MAX = 5000

    def actions(self, p, t):
        out = []
        sid = G.full_server_id(p.c.gamestate)
        gs = p.c.gamestate

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
            if t >= 33000:
                once("score", lambda: "score")
        return out

    def usercmd(self, p, t):
        cmd = _base_cmd(p)
        ps = p.psn()
        for (a, b, btn, fwd, rgt) in self.PHASES:
            if a <= t < b:
                cmd["buttons"] |= btn
                cmd["forwardmove"], cmd["rightmove"] = fwd, rgt
        w0 = self.WALL0
        if t >= w0:
            m = p.marks
            if "wall_yaw" not in m:
                m["wall_yaw"] = ps.get("viewangles[1]", 0.0)
            yaw = m["wall_yaw"]
            speed = math.hypot(ps.get("velocity[0]", 0.0), ps.get("velocity[1]", 0.0))
            if "wall_tb" not in m:
                if t >= w0 + 600 and speed < 5.0:
                    m["wall_tb"] = t
                    self.notes["wall_blocked_" + p.tag] = [t, ps.get("origin[0]"), ps.get("origin[1]"), ps.get("origin[2]")]
                elif t >= w0 + self.WALL_MAX:
                    m["wall_tb"] = t
                    self.notes["wall_notblocked_" + p.tag] = t
            if "wall_tb" not in m:
                _look(p, cmd, yaw, 0.0)
                cmd["forwardmove"] = 127
            else:
                d = t - m["wall_tb"]
                # facing yaw+90: the wall is on the right; then yaw-90: the wall is on the left
                if d < 1000:
                    _look(p, cmd, yaw + 90.0, 0.0)
                    cmd["buttons"] |= B_LR           # toward the wall
                elif d < 2000:
                    _look(p, cmd, yaw + 90.0, 0.0)
                    cmd["buttons"] |= B_LL           # away from the wall
                elif d < 3000:
                    _look(p, cmd, yaw - 90.0, 0.0)
                    cmd["buttons"] |= B_LL           # toward the wall
                elif d < 4000:
                    _look(p, cmd, yaw - 90.0, 0.0)
                    cmd["buttons"] |= B_LR | B_ADS   # away, with ADS
                elif d < 5000:
                    _look(p, cmd, yaw + 90.0, 0.0)
                    cmd["buttons"] |= B_LR | B_CROUCH
                elif d < 6000:
                    _look(p, cmd, yaw + 90.0, 0.0)
                    cmd["buttons"] |= B_LR | B_PRONE
                else:
                    _look(p, cmd, yaw, 0.0)
                    cmd["buttons"] |= B_LL
        p.cmd = cmd
        return cmd


# =============================================================================================
# case 2: melee
# =============================================================================================
class Melee(Base):
    """A stands at (387, 992) facing +y (a spot A walked to in the ADS/lean case, so the floor is known), B at
    (387, 1057) facing -y: 65 units apart (out of melee range), then 38 units (in range)."""
    SPOT_A = (387.0, 992.0, 179.1)

    def actions(self, p, t):
        out = []
        sid = G.full_server_id(p.c.gamestate)
        gs = p.c.gamestate

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                r = fn()
                if r:
                    out.append(r)

        def tele(y, yaw):
            return "setviewpos %.1f %.1f %.1f %.1f" % (self.SPOT_A[0], y, self.SPOT_A[2], yaw)

        if t >= 0:
            once("team", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1000:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", lambda: "mr %d %d %s" % (sid, G.menu_index(gs, menu), p.weapon_choice))
        if p is self.A:
            if t >= 2000:
                once("tele1", lambda: tele(992.0, 90.0))
            if t >= 20000:
                once("tele2", lambda: tele(992.0, 90.0))
            if t >= 33000:
                once("score", lambda: "score")
        else:
            if t >= 2000:
                once("tele1", lambda: tele(1057.0, -90.0))     # 65 units from A: out of range
            if t >= 4500:
                once("tele_near", lambda: tele(1030.0, -90.0))   # 38 units: in range
            if t >= 20000:
                once("tele2", lambda: tele(1030.0, -90.0))
            if t >= 33000:
                once("score", lambda: "score")
        return out

    def usercmd(self, p, t):
        cmd = _base_cmd(p)
        if t >= 2500:
            _look(p, cmd, 90.0 if p is self.A else -90.0, 0.0)

        def press(a, dur=100):
            return a <= t < a + dur

        if p is self.A:
            if any(press(x) for x in (3500, 5500, 6500, 7500, 8500, 9500)):
                cmd["buttons"] |= B_MELEE
            if 10500 <= t < 11500:
                cmd["buttons"] |= B_ADS             # ADS, then melee cancels it
            if press(11000):
                cmd["buttons"] |= B_MELEE
            if 22000 <= t < 23000:
                cmd["buttons"] |= B_ADS             # ADS while B bashes
            if 24000 <= t < 25000:
                cmd["buttons"] |= B_CROUCH
            if 26000 <= t < 27000:
                cmd["buttons"] |= B_LL
        else:
            if 12000 <= t < 30000:
                cmd["buttons"] |= B_USE             # respawn once the killcam ended
            if any(press(x) for x in (21500, 22500, 23500, 24500, 25500, 26500, 27500)):
                cmd["buttons"] |= B_MELEE
        p.cmd = cmd
        return cmd


COMMON = ["+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1"]
CASES = {
    "wpn_melee_ads_lean_ads_lean_dm_toujane": {
        "args": COMMON + ["+devmap", "mp_toujane"], "scenario": AdsLean, "t0": 20000, "length": 40000},
    "wpn_melee_ads_lean_melee_dm_toujane": {
        "args": COMMON + ["+devmap", "mp_toujane"], "scenario": Melee, "t0": 20000, "length": 38000},
}
