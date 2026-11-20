"""Lane social_vote: voting (callvote/vote, g_allowVote), spectating and follow (usercmd attack/ADS edges and the
follownext/followprev commands), quick messages (mr quickcommands/quickstatements/quickresponses), say / say_team /
tell, and team changes mid-game, all on a tdm mp_toujane server with five fake clients.

Everything is keyed to absolute server time through the deciding client's own snapshot (Duel contract).
"""
import gameplay as G

ARGS = ["+set", "g_gametype", "tdm", "+set", "g_allowVote", "1", "+set", "g_log", "games_mp.log", "+set", "g_logSync",
        "1", "+map", "mp_toujane"]
PLAYERS = [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp"), ("C", "allies", "m1carbine_mp"),
           ("D", "axis", "kar98k_mp"), ("E", "spectator", "")]


class Social(G.Duel):
    """Common machinery: menu responses by menu name, a timed script table per tag."""

    def script(self, tag):
        return []

    def usercmd_for(self, tag, t):
        return {}

    def actions(self, p, t):
        gs = p.c.gamestate
        out = []
        sid = G.full_server_id(gs)

        def mr(menu, resp):
            i = G.menu_index(gs, menu)
            return "mr %d %d %s" % (sid, i, resp) if i is not None else "say menu-missing-" + menu

        for i, (at, kind, arg) in enumerate(self.script(p.tag)):
            if t < at or ("s%d" % i) in p.done:
                continue
            p.done.add("s%d" % i)
            if kind == "mr":
                out.append(mr(*arg))
            elif kind == "rcon":
                p.c.send_raw(G.OOB + ("rcon %s %s" % (G.RCON, arg)).encode(), "oob")
            else:
                out.append(arg)
        return out

    def usercmd(self, p, t):
        cmd = {"buttons": 0, "angles": list(p.cmd["angles"]), "forwardmove": 0, "rightmove": 0, "weapon": 0,
               "offhand": 0}
        spec = self.usercmd_for(p.tag, t)
        for k in ("buttons", "forwardmove", "rightmove"):
            cmd[k] = spec.get(k, 0)
        if "yaw" in spec:
            da = p.psn().get("delta_angles[1]", 0)
            cmd["angles"][1] = (G.angle2short(spec["yaw"]) - da) & 0xFFFF
        p.cmd = cmd
        return cmd

    def actions2(self, p, t):
        return []

    usercmd2 = usercmd


def team_weapon(tag, team, weapon):
    menu = "weapon_british" if team == "allies" else "weapon_german"
    return [(0, "mr", ("team_britishgerman", team)), (1000, "mr", (menu, weapon))]


def pre(tag):
    d = {t: (team, w) for t, team, w in PLAYERS}
    team, w = d[tag]
    if team == "spectator":
        return [(0, "mr", ("team_britishgerman", "spectator"))]
    return team_weapon(tag, team, w)


class Vote(Social):
    """The server drops a client's reliable commands that arrive within ~800 ms of the previous one (flood protection,
    nextReliableTime), so every client's commands are at least 1000 ms apart."""

    def script(self, tag):
        s = pre(tag)
        if tag == "A":
            s += [(1000, "rcon", "set g_allowVote 0"), (1500, "cmd", "callvote kick fakeB"),
                  (2100, "rcon", "set g_allowVote 1"),
                  (3200, "cmd", "callvote bogus"), (4200, "cmd", "callvote kick nobody"),
                  (5200, "cmd", "callvote map nomap"), (6200, "cmd", "callvote typemap xyz mp_toujane"),
                  (7200, "cmd", "vote yes"), (8200, "cmd", "callvote kick fakeE"),
                  (9200, "cmd", "callvote map_restart"),
                  (14500, "cmd", "callvote typemap tdm mp_toujane"), (15500, "cmd", "callvote clientkick 99"),
                  (16500, "cmd", "callvote g_gametype nogt"), (17500, "cmd", "callvote a;b"),
                  (20500, "cmd", "vote no"), (30000, "cmd", "say gg")]
        if tag == "B":
            s += [(9200, "cmd", "vote yes"), (10200, "cmd", "vote yes"), (18500, "cmd", "callvote map_restart"),
                  (22500, "cmd", "vote no")]
        if tag == "C":
            s += [(10500, "cmd", "vote y"), (11500, "cmd", "vote no"), (21500, "cmd", "callvote g_gametype dm"),
                  (27000, "cmd", "callvote clientkick 3")]
        if tag == "D":
            s += [(9700, "cmd", "vote no"), (19500, "cmd", "vote no"), (23000, "cmd", "vote no"),
                  (24500, "cmd", "callvote map_restart")]
        if tag == "E":
            s += [(2000, "cmd", "callvote kick fakeB"), (9500, "cmd", "vote yes")]
        return s

    def usercmd_for(self, tag, t):
        if tag == "B" and 1500 <= t < 2500:
            return {"rightmove": 127}
        if tag == "C" and 2000 <= t < 3000:
            return {"forwardmove": 127, "yaw": 45.0}
        return {}


class Spec(Social):
    def script(self, tag):
        s = pre(tag)
        if tag == "A":
            s += [(2000, "cmd", "say hi all"), (6000, "mr", ("quickcommands", "1")),
                  (7000, "mr", ("quickcommands", "2")), (9000, "mr", ("quickcommands", "3")),
                  (10000, "mr", ("team_britishgerman", "axis")), (11000, "mr", ("weapon_german", "mp40_mp")),
                  (16000, "cmd", "kill"), (17000, "cmd", "say_team axis A")]
        if tag == "B":
            s += [(2100, "cmd", "say_team axis only"), (6100, "mr", ("quickstatements", "2")),
                  (12000, "mr", ("team_britishgerman", "spectator")),
                  (14000, "mr", ("team_britishgerman", "allies")), (15000, "mr", ("weapon_british", "thompson_mp")),
                  (17500, "cmd", "say_team allies B")]
        if tag == "C":
            s += [(2200, "cmd", "say_team allies only"), (6200, "mr", ("quickresponses", "3")),
                  (9500, "mr", ("quickstatements", "1")), (13000, "mr", ("quickresponses", "1")),
                  (18000, "cmd", "tell 0 hello")]
        if tag == "D":
            s += [(2300, "cmd", "tell 1 hello"), (6300, "mr", ("quickcommands", "5")),
                  (15500, "mr", ("team_britishgerman", "autoassign")), (16600, "mr", ("weapon_british", "thompson_mp"))]
        if tag == "E":
            s += [(2400, "cmd", "say_team spec"), (3500, "cmd", "say spec all"), (5000, "cmd", "follownext"),
                  (6200, "cmd", "followprev"), (7400, "cmd", "follownext"), (8500, "mr", ("quickcommands", "1")),
                  (19000, "cmd", "follownext"), (20200, "cmd", "follownext"), (21400, "cmd", "followprev")]
        return s

    def usercmd_for(self, tag, t):
        if tag == "A":
            if 1500 <= t < 2600:
                return {"forwardmove": 127}
            if 3000 <= t < 5000:
                return {"forwardmove": 127, "rightmove": 127, "yaw": 20.0}
        if tag == "B" and 1500 <= t < 2500:
            return {"rightmove": 127}
        if tag == "C" and 3000 <= t < 4500:
            return {"forwardmove": -127, "yaw": 90.0}
        if tag == "D" and 2000 <= t < 4000:
            return {"forwardmove": 127, "rightmove": -127}
        if tag == "E":
            # attack edge = follow next, ADS edge = follow previous
            if t in range(3000, 3100) or 3300 <= t < 3400 or 3600 <= t < 3700 or 7000 <= t < 7100 or \
                    12500 <= t < 12600 or 13500 <= t < 13600 or 18000 <= t < 18100:
                return {"buttons": G.B_ATTACK}
            if 4000 <= t < 4100 or 4300 <= t < 4400 or 8000 <= t < 8100 or 14500 <= t < 14600:
                return {"buttons": G.B_ADS}
            if 9000 <= t < 9200:
                return {"buttons": G.B_MELEE}
        return {}


CASES = {
    "social_vote_tdm_toujane": {"args": ARGS, "scenario": Vote, "players": PLAYERS, "t0": 20000, "length": 57000},
    "social_spec_tdm_toujane": {"args": ARGS, "scenario": Spec, "players": PLAYERS, "t0": 20000, "length": 24000},
}
