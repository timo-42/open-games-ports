"""Lane vote_pass: votes that pass and execute (g_oldVoting 1) through G_CheckVote -> Cbuf: fast_restart, tempBanUser
(-> tempBanClient), map, g_gametype (-> map_restart), typemap (map change, gametype+map change, gametype only),
map_rotate; plus script voting (g_oldVoting 0). Every decision is keyed to the deciding client's own snapshot time
(epoch, time since that epoch's level start). The server drops a client's commands that arrive within ~800 ms of the
previous one (sv_floodProtect), so each client's commands are >= 1000 ms apart.
"""
import gameplay as G

LOG = ["+set", "g_log", "games_mp.log", "+set", "g_logSync", "1", "+set", "g_allowVote", "1"]
P4 = [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("C", "allies", "m1carbine_mp"),
      ("D", "axis", "kar98k_mp")]
P3 = P4[:3]


class Scripted(G.Duel):
    SCRIPT = {}     # tag -> [(epoch, rel ms, kind, arg)]   kind: cmd | mr (menu, response)
    MOVE = {}       # tag -> [(epoch, from, to, {usercmd fields})]

    @staticmethod
    def server_id(p, epoch):
        """sv_serverid: from the gamestate, updated by the `d 1 <systeminfo>` configstring command that fast_restart
        sends (the id changes and `mr` carries it)."""
        sid = G.full_server_id(p.c.gamestate)
        if getattr(p, "_vp_epoch", 0) != epoch:   # first call in this epoch: older `d 1` commands belong to the old level
            p._vp_epoch, p._vp_seq = epoch, p.c.server_cmd_seq
        for seq in sorted(k for k in p.c.server_cmds if k > p._vp_seq):
            txt = p.c.server_cmds[seq].decode("latin1")
            if txt.startswith("d 1 ") and "sv_serverid" in txt:
                m = G.re.search(r"\\sv_serverid\\(-?\d+)", txt)
                if m:
                    sid = int(m.group(1))
        return sid

    def _run(self, p, t, epoch):
        gs = p.c.gamestate
        sid = self.server_id(p, epoch)
        out = []
        for i, (ep, at, kind, arg) in enumerate(self.SCRIPT.get(p.tag, [])):
            key = "s%d" % i
            if ep != epoch or t < at or key in p.done:
                continue
            p.done.add(key)
            if kind == "mr":
                alias = {"team": ("team_britishgerman", "team_americangerman"),
                         "weapon_allies": ("weapon_british", "weapon_american")}.get(arg[0], (arg[0],))
                idx = None
                for nm in alias:   # british menus on toujane, american on carentan
                    idx = G.menu_index(gs, nm) if idx is None else idx
                out.append("mr %d %d %s" % (sid, idx, arg[1]) if idx is not None else "say menu-missing-" + arg[0])
            else:
                out.append(arg)
        return out

    def _cmd(self, p, t, epoch):
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
               "offhand": 0}
        for ep, a, b, spec in self.MOVE.get(p.tag, []):
            if ep == epoch and a <= t < b:
                for k in ("buttons", "forwardmove", "rightmove"):
                    cmd[k] = spec.get(k, 0)
                if "yaw" in spec:
                    da = p.psn().get("delta_angles[1]", 0)
                    cmd["angles"][1] = (G.angle2short(spec["yaw"]) - da) & 0xFFFF
        p.cmd = cmd
        return cmd

    def actions(self, p, t):
        return self._run(p, t, 1)

    def usercmd(self, p, t):
        return self._cmd(p, t, 1)

    def actions2(self, p, t):
        return self._run(p, t, p.epoch)

    def usercmd2(self, p, t):
        return self._cmd(p, t, p.epoch)


def team(tag, ep, base=0):
    d = {t: (tm, w) for t, tm, w in P4}
    tm, w = d[tag]
    menu = "weapon_allies" if tm == "allies" else "weapon_german"
    return [(ep, base, "mr", ("team", tm)), (ep, base + 1000, "mr", (menu, w))]


class PassTdm(Scripted):
    """tdm toujane, A..D (quorum 3 of 4, then 2 of 3).
    1: A `map_restart` passes -> `fast_restart` 3 s later (players become spectators and re-pick teams).
    2: B `tempBanUser fakeD` passes -> `tempBanClient "3"` (D banned and dropped).
    3: C `map mp_carentan` passes -> `map mp_carentan`: new gamestate.
    4 (carentan): B `g_gametype dm` passes -> `g_gametype dm; map_restart`."""
    SCRIPT = {
        "A": team("A", 1) + [(1, 3000, "cmd", "callvote map_restart"), (1, 14000, "cmd", "vote yes"),
                             (1, 22000, "cmd", "vote yes")] + team("A", 1, 10000) +
             team("A", 2, 3000) + [(2, 7000, "cmd", "vote yes"), (3, 4000, "cmd", "score")],
        "B": team("B", 1) + [(1, 4000, "cmd", "vote yes"), (1, 13000, "cmd", "callvote tempBanUser fakeD")] +
             team("B", 1, 10000) + team("B", 2, 3000) + [(2, 6000, "cmd", "callvote g_gametype dm"), (3, 5500, "cmd", "say dm")],
        "C": team("C", 1) + [(1, 5000, "cmd", "vote y"), (1, 15000, "cmd", "vote yes"),
                             (1, 21000, "cmd", "callvote map mp_carentan")] + team("C", 1, 10000) +
             team("C", 2, 3000) + [(2, 8000, "cmd", "vote yes")],
        "D": team("D", 1) + [(1, 6000, "cmd", "vote yes"), (1, 12500, "cmd", "vote no"),
                             (1, 14500, "cmd", "vote no")] + team("D", 1, 10000),
    }
    MOVE = {"B": [(1, 1500, 2500, {"rightmove": 127})], "C": [(1, 2000, 3000, {"forwardmove": 127, "yaw": 45.0})]}


class PassTypemap(Scripted):
    """tdm toujane, A..C (quorum 2 of 3). 1: A typemap dm mp_carentan (-> g_gametype dm; map mp_carentan).
    2 (dm carentan): B typemap tdm mp_carentan (-> g_gametype tdm; map_restart, gametype only).
    3 (tdm carentan, same level load): A map_rotate (sv_mapRotation "gametype dm map mp_toujane")."""
    SCRIPT = {
        "A": team("A", 1) + [(1, 3000, "cmd", "callvote typemap dm mp_carentan")] + team("A", 2, 3000) +
             team("A", 3, 3000) + [(3, 6000, "cmd", "callvote map_rotate")],
        "B": team("B", 1) + [(1, 4000, "cmd", "vote yes")] + team("B", 2, 3000) +
             [(2, 6000, "cmd", "callvote typemap tdm mp_carentan")] + team("B", 3, 3000) +
             [(3, 7000, "cmd", "vote yes")],
        "C": team("C", 1) + team("C", 2, 3000) + [(2, 7000, "cmd", "vote yes")] + team("C", 3, 3000),
    }


class ScriptVote(Scripted):
    """g_oldVoting 0: callvote/vote are forwarded to the scripts (call_vote / vote notify); no stock script listens."""
    SCRIPT = {
        "A": team("A", 1) + [(1, 3000, "cmd", "callvote map_restart"), (1, 5000, "cmd", "callvote kick fakeB"),
                             (1, 7000, "cmd", "callvote a;b"), (1, 8000, "cmd", "vote yes")],
        "B": team("B", 1) + [(1, 4000, "cmd", "vote no"), (1, 9000, "cmd", "callvote map mp_carentan")],
        "C": team("C", 1) + [(1, 6000, "cmd", "vote y")],
    }


CASES = {
    "vote_pass_tdm_toujane": {"args": ["+set", "g_gametype", "tdm"] + LOG + ["+map", "mp_toujane"],
                              "scenario": PassTdm, "players": P4, "t0": 20000, "length": 40000, "rotate": 14000},
    "vote_pass_typemap_toujane": {"args": ["+set", "g_gametype", "tdm", "+set", "sv_mapRotation",
                                           "gametype dm map mp_toujane"] + LOG + ["+map", "mp_toujane"],
                                  "scenario": PassTypemap, "players": P3, "t0": 20000, "length": 40000,
                                  "rotate": 12000},
    "vote_pass_script_toujane": {"args": ["+set", "g_gametype", "tdm", "+set", "g_oldVoting", "0"] + LOG +
                                 ["+map", "mp_toujane"],
                                 "scenario": ScriptVote, "players": P3, "t0": 20000, "length": 14000},
}
