"""Lane wpn_rockets: rocket launchers (panzerschreck_mp), scoped rifles with ADS (kar98k_sniper_mp / springfield_mp),
dropped grenade / weapon pickup and a non-lethal melee hit.

The stock MP scripts never precache or allow the launchers (`//precacheItem("panzerschreck_mp")` and the weapon
restriction switch's default case `restricted` in maps/mp/gametypes/_weapons.gsc). The `mod` overlay therefore writes a
loose copy of that one file (derived from the game's own iw_07.iwd at run time, never committed) under
<fs_homepath>/mods/wpnrk with the two precache lines enabled and the two weapons accepted by restrictWeaponByServerCvars.
Everything else (menus, spawn, damage, kill, drop, pickup) is stock GSC and stock engine behaviour.

Every decision is a function of the deciding client's own latest snapshot and its server time (as for Duel).
"""
import hashlib
import math
import os
import zipfile

import gameplay as G

P = G.P
B_ATTACK, B_MELEE, B_USE, B_RELOAD = G.B_ATTACK, G.B_MELEE, G.B_USE, G.B_RELOAD
B_ADS, B_CROUCH, B_PRONE = G.B_ADS, G.B_CROUCH, G.B_PRONE
B_FRAG = 0x10000
MODNAME = "wpnrk"
ET_ITEM = 3


def make_mod(home, data):
    iwd = os.path.join(data, "iw_07.iwd")
    src = zipfile.ZipFile(iwd).read("maps/mp/gametypes/_weapons.gsc").decode("latin1")
    new = src
    for w in ("panzerfaust_mp", "panzerschreck_mp"):
        line = '//precacheItem("%s");' % w
        if line not in new:
            raise SystemExit("_weapons.gsc: %s not found" % line)
        new = new.replace(line, line[2:])
    anchor = "\tcase \"fraggrenade\":\r\n\t\tif(!getcvarint(\"scr_allow_fraggrenades\"))"
    if anchor not in new:
        raise SystemExit("_weapons.gsc: restrict anchor not found")
    new = new.replace(anchor, '\tcase "panzerschreck_mp":\r\n\tcase "panzerfaust_mp":\r\n\t\tbreak;\r\n\r\n' + anchor, 1)
    out = os.path.join(home, "mods", MODNAME, "maps/mp/gametypes")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "_weapons.gsc"), "w", encoding="latin1", newline="") as fh:
        fh.write(new)
    return "wpn_rockets overlay _weapons.gsc sha256 %s (stock %s)" % (
        hashlib.sha256(new.encode("latin1")).hexdigest(), hashlib.sha256(src.encode("latin1")).hexdigest())


def _base_cmd(p):
    return {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}


class Rockets(G.Duel):
    TEAM_MENU = "team_britishgerman"
    WEAPON_MENU = {"allies": "weapon_british", "axis": "weapon_german"}
    T_SCORE = 40000

    # ---- helpers -------------------------------------------------------------------------------------
    def other(self, p):
        return self.B if p is self.A else self.A

    def look(self, p, cmd, yaw, pitch):
        ps = p.psn()
        cmd["angles"][0] = (G.angle2short(pitch) - ps.get("delta_angles[0]", 0)) & 0xFFFF
        cmd["angles"][1] = (G.angle2short(yaw) - ps.get("delta_angles[1]", 0)) & 0xFFFF

    def me(self, p):
        ps = p.psn()
        return [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0), ps.get("origin[2]", 0.0)], ps.get("viewHeightCurrent", 0.0)

    def other_pos(self, p):
        st = p.ents().get(self.other(p).c.client_num)
        if st is None:
            return None
        e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
        return [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"]]

    def aim_at(self, p, cmd, dz, ground_back=0.0):
        """Look at the other player's entity (+dz above its origin) from p's own snapshot; with ground_back > 0, at the
        floor point `ground_back` units to the side of the target (a splash shot next to the target)."""
        o = self.other_pos(p)
        if o is None:
            return None
        me, vh = self.me(p)
        eye = [me[0], me[1], me[2] + vh]
        tgt = [o[0], o[1], o[2] + dz]
        if ground_back:
            dx, dy = eye[0] - o[0], eye[1] - o[1]
            n = math.hypot(dx, dy) or 1.0
            tgt = [o[0] - dy / n * ground_back, o[1] + dx / n * ground_back, o[2]]  # perpendicular to the line to p
        dx, dy, dz_ = tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]
        self.look(p, cmd, math.degrees(math.atan2(dy, dx)), -math.degrees(math.atan2(dz_, math.hypot(dx, dy))))
        return math.hypot(dx, dy)

    def nearest_item(self, p):
        """(x, y, z, weapon) of the nearest ET_ITEM entity in p's own snapshot, or None."""
        me, _ = self.me(p)
        best = None
        for n, st in sorted(p.ents().items()):
            e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
            if e["eType"] != ET_ITEM:
                continue
            pos = [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"]]
            d = math.hypot(pos[0] - me[0], pos[1] - me[1])
            if best is None or d < best[0]:
                best = (d, pos, e["index"], n)
        return best

    def widx(self, p, name):
        return G.weapon_index(p.c.gamestate, name) or 0

    # ---- reliable commands ----------------------------------------------------------------------------
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
        out.extend(self.script_actions(p, t, once))
        return out

    def script_actions(self, p, t, once):
        return []

    def usercmd(self, p, t):
        cmd = _base_cmd(p)
        if p is self.A and 1500 <= t < 2600:
            cmd["forwardmove"] = 127
        self.plan(p, t, cmd)
        p.cmd = cmd
        return cmd

    def actions2(self, p, t):
        return []

    def usercmd2(self, p, t):
        p.cmd = _base_cmd(p)
        return p.cmd

    def plan(self, p, t, cmd):
        raise NotImplementedError


class RocketSniper(Rockets):
    """dm/toujane, A (allies, panzerschreck + webley + 1 frag) at X1, B (axis, kar98k_sniper + luger + 3 frags) at X2,
    522 units apart on spawn-point floors (setviewpos; eye z = floor + 60).
       6000  A fires a rocket at B: direct hit kills B (MOD_PROJECTILE); B's weapon and frags are dropped
       7000  A walks to the nearest dropped item and holds use (pickup)
       13000 A is moved back to X1; B (killcam ended: use held) respawns and is moved to X2
       15000 B ADS with the sniper rifle at A and fires; B releases ADS
       18000 A fires a rocket at the floor 245 units from B towards A: splash only, B survives
       20000 A teleports 40 units in front of B, selects the pistol and bashes (non-lethal pistol melee)
       B bashes A back, ADS/overlay on/off, second shot
    """
    X1 = (858.0, 742.0, 39.0)
    X2 = (1363.0, 609.0, 29.0)
    T_SCORE = 44000
    T_RESP = 11000

    def tele(self, pos, other, dist=None):
        yaw = math.degrees(math.atan2(other[1] - pos[1], other[0] - pos[0]))
        return "setviewpos %.1f %.1f %.1f %.1f" % (pos[0], pos[1], pos[2] + 60.0, yaw)

    def script_actions(self, p, t, once):
        out = []
        mine, theirs = (self.X1, self.X2) if p is self.A else (self.X2, self.X1)
        if t >= 5000:
            once("tele", lambda: self.tele(mine, theirs))
        if p is self.B and t >= self.T_RESP and "tele2" not in p.done:
            ps = p.psn()
            if ps.get("pm_type", 1) == 0 and ps.get("commandTime", 0):
                p.marks.setdefault("resp_seen", t)
                if t >= p.marks["resp_seen"] + 500:
                    p.done.add("tele2")
                    out.append(self.tele(mine, theirs))
        if p is self.A and t >= 13000:
            once("tele2", lambda: self.tele(mine, theirs))
        if p is self.A and t >= self.T_BASH:
            once("tele_bash", lambda: self.tele_bash(p))
        if t >= self.T_SCORE:
            once("score", lambda: "score")
        return out

    T_BASH = 20000

    def tele_bash(self, p):
        o = self.other_pos(p)
        if o is None:
            return None
        dx, dy = self.X1[0] - o[0], self.X1[1] - o[1]
        n = math.hypot(dx, dy)
        x, y = o[0] + dx / n * 40.0, o[1] + dy / n * 40.0
        yaw = math.degrees(math.atan2(o[1] - y, o[0] - x))
        text = "setviewpos %.1f %.1f %.1f %.1f" % (x, y, o[2] + 60.0, yaw)
        self.notes["tele_bash"] = text
        return text

    def plan(self, p, t, cmd):
        if p is self.A:
            if 5500 <= t < 6200:
                self.aim_at(p, cmd, 35.0)
            if 6000 <= t < 6100:
                cmd["buttons"] |= B_ATTACK
            if 7000 <= t < 12000:
                it = self.nearest_item(p)
                if it:
                    d, pos, idx, n = it
                    me, vh = self.me(p)
                    self.look(p, cmd, math.degrees(math.atan2(pos[1] - me[1], pos[0] - me[0])), 0.0)
                    if d > 8.0:
                        cmd["forwardmove"] = 127
                cmd["buttons"] |= B_USE
            if 13500 <= t < 18400:
                self.aim_at(p, cmd, 35.0, ground_back=-285.0) if t >= 17500 else self.aim_at(p, cmd, 35.0)
            if 18000 <= t < 18100:
                cmd["buttons"] |= B_ATTACK
            if 28000 <= t < 40000:
                cmd["buttons"] |= B_USE
            if 20500 <= t < 24000:
                w = self.widx(p, "webley_mp")
                cmd["weapon"] = w
                self.aim_at(p, cmd, 30.0)
            if any(a <= t < a + 100 for a in (21500,)):
                cmd["buttons"] |= B_MELEE
        else:
            if 5500 <= t < 8000:
                self.aim_at(p, cmd, 35.0)
            if 11000 <= t < 13000:
                cmd["buttons"] |= B_USE
            if 14000 <= t < 30000:
                self.aim_at(p, cmd, 35.0)
            if 15000 <= t < 17000 or 24000 <= t < 25000:
                cmd["buttons"] |= B_ADS
            if 24600 <= t < 24700:
                cmd["buttons"] |= B_ATTACK
            if 24000 <= t < 25000:
                pass
            if 26500 <= t < 26600:
                cmd["buttons"] |= B_ATTACK


class SnipePickup(Rockets):
    """tdm/carentan (american vs german): A springfield_mp, B panzerschreck_mp (axis menu answered with the launcher).
       5000  both moved to the marks (about 100 units apart, facing)
       6000  A ADS (scope overlay), 7000 shot: B dies (rifle kill), drops the launcher (and a frag)
       8000  A walks onto the dropped items with use held (pickup of a dropped launcher and a grenade)
       11000 B (use held) respawns, is moved in front of A
       14500 A selects the picked-up launcher (if any), aims, fires: rocket kill of B (direct hit)
       B fires its own launcher in ADS (no overlay) at the floor next to A before that: splash, A survives
    """
    TEAM_MENU = "team_americangerman"
    WEAPON_MENU = {"allies": "weapon_american", "axis": "weapon_german"}
    T_SCORE = 36000

    def script_actions(self, p, t, once):
        out = []
        if p is self.A:
            if t >= 5000:
                once("tele", lambda: self.teleport(p, "p1", "p0"))
            if t >= 13000:
                once("tele2", lambda: self.teleport(p, "p1", "p0"))
        else:
            if t >= 5000:
                once("tele", lambda: self.teleport(p, "p0", "p1"))
            if t >= 10500 and "tele2" not in p.done:
                ps = p.psn()
                if ps.get("pm_type", 1) == 0 and ps.get("commandTime", 0):
                    p.marks.setdefault("resp_seen", t)
                    if t >= p.marks["resp_seen"] + 500:
                        p.done.add("tele2")
                        out.append(self.teleport(p, "p0", "p1"))
        if t >= self.T_SCORE:
            once("score", lambda: "score")
        return out

    def plan(self, p, t, cmd):
        if p is self.A:
            if 5500 <= t < 8000:
                self.aim_at(p, cmd, 45.0)
            if 6000 <= t < 7500:
                cmd["buttons"] |= B_ADS
            if 7000 <= t < 7100:
                cmd["buttons"] |= B_ATTACK
            if 8000 <= t < 11000:
                it = self.nearest_item(p)
                if it:
                    d, pos, idx, n = it
                    me, vh = self.me(p)
                    self.look(p, cmd, math.degrees(math.atan2(pos[1] - me[1], pos[0] - me[0])), 0.0)
                    if d > 8.0:
                        cmd["forwardmove"] = 127
                if (t // 100) % 4 == 0:             # pickup needs a fresh use press: pulse it
                    cmd["buttons"] |= B_USE
            if 14000 <= t < 20000:
                w = self.widx(p, "panzerschreck_mp")
                if w and self.has_weapon(p, w):
                    cmd["weapon"] = w
                self.aim_at(p, cmd, 30.0)
            if 16500 <= t < 16600:
                cmd["buttons"] |= B_ATTACK
            if 20000 <= t < 21000:
                cmd["buttons"] |= B_ADS
        else:
            if 10000 <= t < 13000:
                cmd["buttons"] |= B_USE
            if 13500 <= t < 16000:
                self.aim_at(p, cmd, 0.0, ground_back=60.0)
            if 14000 <= t < 15500:
                cmd["buttons"] |= B_ADS
            if 15000 <= t < 15100:
                cmd["buttons"] |= B_ATTACK

    def has_weapon(self, p, widx):
        ps = p.ps()
        if not ps:
            return False
        return bool(ps[(0x544 >> 2) + widx // 32] >> (widx % 32) & 1)  # ps.weapons[] bit array (cod2proto PLAYER_FIELDS)


COMMON = ["+set", "g_gametype", "dm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
          "+set", "fs_game", "mods/" + MODNAME]
CARENTAN = ["+set", "g_gametype", "tdm", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
            "+set", "fs_game", "mods/" + MODNAME]
CASES = {
    "wpn_rockets_snipe_tdm_carentan": {
        "args": CARENTAN + ["+devmap", "mp_carentan"], "scenario": SnipePickup, "mod": make_mod,
        "players": [("A", "allies", "springfield_mp"), ("B", "axis", "panzerschreck_mp")],
        "t0": 20000, "length": 40000},
    "wpn_rockets_rocket_dm_toujane": {
        "args": COMMON + ["+devmap", "mp_toujane"], "scenario": RocketSniper, "mod": make_mod,
        "players": [("A", "allies", "panzerschreck_mp"), ("B", "axis", "kar98k_sniper_mp")],
        "t0": 20000, "length": 48000},
}
