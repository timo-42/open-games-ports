"""Search & Destroy match-end lane (end_sd): scr_sd_roundlimit / scr_sd_timelimit endings, last-round win kinds,
the endMap intermission and scr_sd_roundlength expiry. Notes: analysis/gameplay/end_sd.md.

Cases (g_gametype sd, devmap; the session ends during the final intermission, no rotation, no second gamestate):
  end_sd_roundlimit_mp_toujane   scr_sd_roundlimit 2: R1 elimination (A shoots B), R2 plant at site A + explosion
                                 (last round won by the bomb) -> checkRoundLimit -> endMap.
  end_sd_timelimit_mp_carentan   scr_sd_timelimit 1.1 (minutes), scr_sd_roundlength 0.5: R1 defender suicide
                                 (elimination), R2 round time expires (defenders win) -> checkTimeLimit -> endMap.
  end_sd_roundlimit3_mp_toujane  scr_sd_roundlimit 3, roundlength 0.5: R1 plant at B + defuse, R2 timeout, R3
                                 elimination (last round) -> endMap.
Decisions use the same level-relative keys as scenarios/gt_sd.py (SdPlan is reused as a base class, unmodified).
"""
import gameplay as G
from scenarios.gt_sd import SdPlan, tp

B_USE = G.B_USE


class EndRoundLimit(SdPlan):
    PLAN = {
        1: {"acts": [
            (1, "A", 1000, tp(-65, 1160, 100, 180)),          # R1: face to face in the attacker spawn area
            (1, "B", 1000, tp(-185, 1160, 100, 0)),
            (1, "A", 12000, "score"),
            (2, "A", 1000, tp(1047, 2650, 300, 90)),          # R2: plant on the wreck inside zone A
            (2, "B", 28000, "score"),
        ],
            "hold": [(2, "A", 2000, 12000, B_USE)],
            "aim": [(1, "A", 3000, 9000, 3500)]},
    }


class EndTimeLimit(SdPlan):
    PLAN = {
        1: {"acts": [
            (1, "B", 3000, "kill"),                           # R1: defender suicide -> elimination
            (1, "A", 8000, "score"),
            (2, "A", 5000, "say stay"),                       # R2: nobody acts, the 30 s round expires
            (2, "B", 20000, "score"),
        ],
            "hold": [], "aim": []},
    }


class EndRoundLimit3(SdPlan):
    PLAN = {
        1: {"acts": [
            (1, "A", 1000, tp(2834, 1560, 300, 90)),          # R1: plant in zone B, defender defuses
            (1, "B", 7500, tp(2806, 1565, 300, 0)),
            (1, "A", 17000, "say planted"),
            (2, "A", 5000, "say stay"),                       # R2: round time expires
            (2, "B", 20000, "score"),
            (3, "A", 1000, tp(-65, 1160, 100, 180)),          # R3: elimination, last round
            (3, "B", 1000, tp(-185, 1160, 100, 0)),
            (3, "A", 12000, "score"),
        ],
            "hold": [(1, "A", 2000, 8000, B_USE), (1, "B", 9000, 22000, B_USE)],
            "aim": [(3, "A", 3000, 9000, 3500)]},
    }


BASE = ["+set", "g_gametype", "sd", "+set", "g_log", "games_mp.log", "+set", "g_logSync", "1",
        "+set", "scr_sd_bombtimer", "30"]
PL = [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")]
CASES = {
    "end_sd_roundlimit_mp_toujane": {
        "args": BASE + ["+set", "scr_sd_roundlimit", "2", "+devmap", "mp_toujane"],
        "scenario": EndRoundLimit, "players": PL, "t0": 20000, "length": 110000},
    "end_sd_timelimit_mp_carentan": {
        "args": BASE + ["+set", "scr_sd_timelimit", "1.1", "+set", "scr_sd_roundlength", "0.5", "+devmap", "mp_carentan"],
        "scenario": EndTimeLimit, "players": PL, "t0": 20000, "length": 100000},
    "end_sd_roundlimit3_mp_toujane": {
        "args": BASE + ["+set", "scr_sd_roundlimit", "3", "+set", "scr_sd_roundlength", "0.5", "+devmap", "mp_toujane"],
        "scenario": EndRoundLimit3, "players": PL, "t0": 20000, "length": 140000},
}
