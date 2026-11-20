"""Lane end_teams: end conditions (score limit, time limit, intermission/final scoreboard) in tdm, ctf and hq, plus
CTF dropped-flag touch return / autoReturn and the HQ radio hold cap.

end_teams_tdm_ctf_mp_toujane   tdm: scr_tdm_scorelimit 2, A kills B twice -> checkScoreLimit/endMap, intermission;
                               rotation "gametype ctf map mp_downtown": scr_ctf_timelimit 0.5 ends epoch 2 (time limit).
end_teams_ctf_hq_mp_downtown   ctf on mp_downtown (the only mp map without trigger_hurt/minefield, so dropFlag never
                               returns a flag at once): A drops the axis flag, B (owning team) touches it ("returned
                               the flag"), A re-picks a dropped flag (end_autoreturn), the last drop returns by the
                               120 s autoReturn, then B captures twice: scr_ctf_scorelimit 2 -> endMap. Rotation
                               "gametype hq map mp_toujane": scr_hq_timelimit 0.5 ends epoch 2.
end_teams_hq_hold_mp_toujane   hq: A holds the radio until the 120 s RadioMaxHoldSeconds reset, takes the next radio
                               and reaches scr_hq_scorelimit.
Every decision is a function of the deciding client's own snapshot and server time."""
import math

import gameplay as G
import cod2proto as P

WEAPON_BY_MENU = {"weapon_british": "thompson_mp", "weapon_german": "mp40_mp", "weapon_american": "thompson_mp",
                  "weapon_russian": "ppsh_mp"}
LOG = ["+set", "g_log", "games_mp.log", "+set", "g_logSync", "1"]


def tele(pos, yaw=0.0, dz=20.0):
    return "setviewpos %.1f %.1f %.1f %.1f" % (pos[0], pos[1], pos[2] + dz, yaw)


class Base(G.Duel):
    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.im = {}

    def menus(self, p):
        """(team menu index, {team: weapon menu index}) from this epoch's gamestate: menu configstrings are
        registered in order ingame, team_<a><b>, weapon_<a>, weapon_<b> (_menus.gsc), so the names differ per map."""
        gs = p.c.gamestate
        names = sorted((int(k), v) for k, v in gs["configstrings"].items() if 0x4DE <= int(k) < 0x4DE + 0x20)
        self.notes["menus_e%d" % p.epoch] = [list(x) for x in names]
        team = [i - 0x4DE for i, v in names if v.startswith("team_")][0]
        weap = [(i - 0x4DE, v) for i, v in names if v.startswith("weapon_")]
        return team, {"allies": weap[0], "axis": weap[1]}

    join1 = None   # (team, weapon) menu response times; None = weapon 1000 ms after the team response was sent
    join2 = None

    def join(self, p, t, out, key="", times=None):
        """Team then weapon menu response (fixed times relative to the level start, or relative to each other)."""
        sid = G.full_server_id(p.c.gamestate)
        if t >= (times[0] if times else 0) and "team" + key not in p.done:
            p.done.add("team" + key)
            p.done.add(("team_t", key, t))
            out.append("mr %d %d %s" % (sid, self.menus(p)[0], p.team))
        sent = [d[2] for d in p.done if isinstance(d, tuple) and d[1] == key]
        due = times[1] if times else (sent[0] + 1000 if sent else None)
        if sent and t >= due and "weapon" + key not in p.done:
            p.done.add("weapon" + key)
            idx, name = self.menus(p)[1][p.team]
            out.append("mr %d %d %s" % (sid, idx, WEAPON_BY_MENU[name]))

    def at(self, p, t, out, key, t_min, fn):
        if t >= t_min and key not in p.done:
            p.done.add(key)
            r = fn()
            if r:
                out.append(r)

    def aim(self, p, target, cmd, ps, fire):
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]
        me = [ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0),
              ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0)]
        st = p.ents().get(target.c.client_num)
        if st is not None:
            e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
            dx, dy, dz = e["pos.trBase[0]"] - me[0], e["pos.trBase[1]"] - me[1], e["pos.trBase[2]"] + 40.0 - me[2]
            cmd["angles"][0] = (G.angle2short(-math.degrees(math.atan2(dz, math.hypot(dx, dy)))) - da[0]) & 0xFFFF
            cmd["angles"][1] = (G.angle2short(math.degrees(math.atan2(dy, dx))) - da[1]) & 0xFFFF
        if fire:
            cmd["buttons"] |= G.B_ATTACK

    def blank(self, p):
        return {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
                "offhand": 0}

    def intermission(self, p):
        return p.psn().get("pm_type", 0) == 5

    def score_in_intermission(self, p, t, out, delay=2000):
        """`score` (the scoreboard request) `delay` ms after this client first sees pm_type == PM_INTERMISSION."""
        if self.intermission(p):
            key = (p.tag, p.epoch)
            first = self.im.setdefault(key, t)
            self.notes["intermission_%s_%d" % key] = first
            self.at(p, t, out, "im_score%d" % p.epoch, first + delay, lambda: "score")

    def actions2(self, p, t):
        """Epoch 2: players are spectators again after the map change; join through the stock menus."""
        out = []
        self.join(p, t, out, key="e2", times=self.join2)
        self.actions2_extra(p, t, out)
        self.score_in_intermission(p, t, out)
        return out

    def actions2_extra(self, p, t, out):
        pass


# ------------------------------------------------------------------------------------------------------------
class TdmCtf(Base):
    """Epoch 1 (tdm): A kills B at T+16.5 s and again after B respawned: team score 2 = scr_tdm_scorelimit.
    Epoch 2 (ctf): both join, move (Duel.usercmd2) and the time limit (scr_ctf_timelimit 0.5) ends the map."""

    def actions(self, p, t):
        out = []
        self.join(p, t, out, times=self.join1)
        if p is self.A:
            self.at(p, t, out, "p0", 1500, lambda: self.mark(p, "p0"))
            self.at(p, t, out, "p1", 2600, lambda: self.mark(p, "p1"))
            self.at(p, t, out, "tele", 15000, lambda: self.teleport(p, "p0", "p1"))
        else:
            self.at(p, t, out, "tele", 15000, lambda: self.teleport(p, "p1", "p0"))
            self.at(p, t, out, "tele2", 31000, lambda: self.teleport(p, "p1", "p0"))
        self.score_in_intermission(p, t, out)
        return out

    def usercmd(self, p, t):
        ps = p.psn()
        cmd = self.blank(p)
        if p is self.A:
            if 1500 <= t < 2600:
                cmd["forwardmove"] = 127
            if 16000 <= t < 19500 or 32000 <= t < 36000:
                self.aim(p, self.B, cmd, ps, t >= 16500 and (t < 19500 or t >= 32500))
        else:
            if 19500 <= t < 30500:
                cmd["buttons"] |= G.B_USE
        p.cmd = cmd
        return cmd

    def actions2_extra(self, p, t, out):
        # no setviewpos here: the rotation loads the map with `map`, so cheats are off in epoch 2
        if p is self.B:
            self.at(p, t, out, "e2say", 4000, lambda: "say map2")


# mp_downtown (entity lump): trigger_radius flags, ctf spawn points (floor-verified by the game's own spawns)
AXIS_FLAG_DT = (1781.0, 444.0, 46.0)
ALLIED_FLAG_DT = (1533.0, -2715.0, 35.0)
S1 = (1769.0, -1293.0, 67.0)   # mp_ctf_spawn_axis
S2 = (2556.0, -2771.0, 87.0)   # mp_ctf_spawn_allied
S3 = (3104.0, -1738.0, 38.0)   # mp_ctf_spawn_allied


class CtfHq(Base):
    # Epoch 2: A's first active snapshot is T+2100 or T+2150 depending on a wall-clock race at the map change, so the
    # menu responses are keyed to fixed times that both snapshots reach in the same frame (t = 2500, 3500).
    join1 = (0, 1000)
    join2 = (2500, 3500)

    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.alive_at = None
        self.deaths = []

    def track(self, p):
        """A's last alive origin before each death: where its carried flag drops (own snapshot data)."""
        ps = p.psn()
        o = (ps.get("origin[0]", 0.0), ps.get("origin[1]", 0.0), ps.get("origin[2]", 0.0))
        if ps.get("pm_type", 0) == 0:
            self.alive_at = o
        elif self.alive_at is not None:
            self.deaths.append(self.alive_at)
            self.notes["deaths_A"] = [list(d) for d in self.deaths]
            self.alive_at = None

    def actions(self, p, t):
        out = []
        self.join(p, t, out, times=self.join1)
        if p is self.A:
            self.track(p)
            self.at(p, t, out, "take1", 3000, lambda: tele(AXIS_FLAG_DT, 90.0))
            self.at(p, t, out, "to_s1", 5000, lambda: tele(S1, 0.0))
            self.at(p, t, out, "kill1", 6500, lambda: "kill")
            self.at(p, t, out, "take2", 22000, lambda: tele(AXIS_FLAG_DT, 90.0))
            self.at(p, t, out, "to_s2", 24000, lambda: tele(S2, 0.0))
            self.at(p, t, out, "kill2", 25000, lambda: "kill")
            if len(self.deaths) >= 2:   # enemy touches the flag dropped at A's 2nd death site again
                self.at(p, t, out, "repick", 40000, lambda: tele(self.deaths[1], 0.0, 25.0))
            self.at(p, t, out, "to_s3", 41500, lambda: tele(S3, 0.0))
            self.at(p, t, out, "kill3", 42500, lambda: "kill")           # last drop: autoReturn 120 s later
        else:
            self.at(p, t, out, "return1", 9000, lambda: tele(S1, 0.0))   # owning team touches the dropped flag
            self.at(p, t, out, "take_a", 165000, lambda: tele(ALLIED_FLAG_DT, 0.0))
            self.at(p, t, out, "cap1", 166500, lambda: tele((AXIS_FLAG_DT[0] - 30, AXIS_FLAG_DT[1], AXIS_FLAG_DT[2]), 0.0))
            self.at(p, t, out, "take_b", 168000, lambda: tele(ALLIED_FLAG_DT, 0.0))
            self.at(p, t, out, "cap2", 169500, lambda: tele((AXIS_FLAG_DT[0] - 30, AXIS_FLAG_DT[1], AXIS_FLAG_DT[2]), 0.0))
        self.score_in_intermission(p, t, out)
        return out

    def usercmd(self, p, t):
        cmd = self.blank(p)
        if p is self.A and (8000 <= t < 21500 or 26000 <= t < 39500 or 44000 <= t < 60000):
            cmd["buttons"] |= G.B_USE
        p.cmd = cmd
        return cmd


# ------------------------------------------------------------------------------------------------------------
TOUJANE_RADIOS = [(285.332, 471.947, 12.1747), (-7.80745, 1100.36, -11.0072), (1282.54, 731.845, -1.99999),
                  (2206.32, 411.609, 54.6547), (758.63, 1725.03, 148), (1558.4, 1569.03, 96.0133),
                  (986.298, 3040.61, 58.7701), (2295.88, 2483.53, 70.7077), (2971.51, 1570.41, 45.4606),
                  (1758, 653.443, 176)]


def find_radios(p):
    out = {}
    for n, st in p.ents().items():
        e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
        if e.get("eType") != 6:
            continue
        o = (e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"])
        for r in TOUJANE_RADIOS:
            if abs(o[0] - r[0]) < 1.0 and abs(o[1] - r[1]) < 1.0 and abs(o[2] - r[2]) < 1.0:
                out[n] = r
    return out


class HqHold(Base):
    """A takes the first radio (T = its first appearance in A's snapshot), holds it until the 120 s cap resets it,
    then takes the next radio that appears and holds it until the score limit ends the map. B stays at its spawn."""

    def __init__(self, t0, players, summary):
        super().__init__(t0, players, summary)
        self.seen = {}

    def actions(self, p, t):
        out = []
        self.join(p, t, out, times=self.join1)
        if p is self.A:
            rs = find_radios(p)
            for n, pos in rs.items():
                if n not in self.seen:
                    self.seen[n] = (len(self.seen) + 1, t, pos)
            self.notes["radios_A"] = {str(k): list(v[2]) + [v[1]] for k, v in self.seen.items()}
            order = sorted(self.seen.items(), key=lambda kv: kv[1][0])
            for i, (n, (idx, t1, pos)) in enumerate(order):
                self.at(p, t, out, "go%d" % idx, t1 + 500 if t >= t1 + 500 else 1 << 60,
                        lambda pos=pos: "setviewpos %.1f %.1f %.1f 0" % (pos[0] + 30.0, pos[1], pos[2] + 25.0))
        self.score_in_intermission(p, t, out)
        return out

    def usercmd(self, p, t):
        cmd = self.blank(p)
        p.cmd = cmd
        return cmd


CASES = {
    "end_teams_tdm_ctf_mp_toujane": {
        "args": ["+set", "g_gametype", "tdm", "+set", "scr_tdm_scorelimit", "2", "+set", "scr_ctf_timelimit", "0.5",
                 "+set", "sv_mapRotation", "gametype ctf map mp_downtown"] + LOG + ["+devmap", "mp_toujane"],
        "scenario": TdmCtf, "t0": 20000, "length": 60000, "rotate": 34000},
    "end_teams_ctf_hq_mp_downtown": {
        "args": ["+set", "g_gametype", "ctf", "+set", "scr_ctf_scorelimit", "2", "+set", "scr_hq_timelimit", "0.5",
                 "+set", "sv_mapRotation", "gametype hq map mp_toujane"] + LOG + ["+devmap", "mp_downtown"],
        "scenario": CtfHq, "t0": 20000, "length": 175000, "rotate": 34000},
    "end_teams_hq_hold_mp_toujane": {
        "args": ["+set", "g_gametype", "hq", "+set", "scr_hq_scorelimit", "125"] + LOG + ["+devmap", "mp_toujane"],
        "scenario": HqHold, "t0": 20000, "length": 184000},
}
