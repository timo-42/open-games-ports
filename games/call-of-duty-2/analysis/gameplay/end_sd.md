# Gameplay lane end_sd: Search & Destroy match-end conditions

Plugin: `tools/oracle/scenarios/end_sd.py` (reuses `SdPlan` from `gt_sd.py` as an unmodified base class). Original `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a7`). Tags: [O] observed, [I] inferred, [U] unknown.

## Cases

| Case | Exercises | Items | Original runs | Recon runs | Mismatches |
|---|---|---|---|---|---|
| `end_sd_roundlimit_mp_toujane` | `scr_sd_roundlimit 2`: R1 elimination (A shoots B), R2 plant at site A, bomb explodes and kills the planter (last round won by the bomb); `checkRoundLimit` -> `endMap`, intermission, `ExitLevel` and the reload of the map, then both clients quit | 158823 | 3 | 2 | 0 |
| `end_sd_timelimit_mp_carentan` | `scr_sd_timelimit 1.1` + `scr_sd_roundlength 0.5`: R1 defender suicide (elimination), R2 nobody acts, the 30 s round expires (`MP_TIMEHASEXPIRED`, defenders win); `checkTimeLimit` -> `endMap`; `ExitLevel`, reload | 138478 | 3 | 2 | 0 |
| `end_sd_roundlimit3_mp_toujane` | `scr_sd_roundlimit 3`, roundlength 0.5: R1 plant at B + defuse (defenders), R2 timeout (defenders), R3 elimination (attackers, last round); `endMap`, `ExitLevel`, reload | 199513 | 3 | 2 | 0 |
| Regression `duel_dm_toujane` (`--tag=-end_sd`) | | 84560 | existing | 1 | 0 |

## Design (from stock `sd.gsc`)

- [O] `endRound` waits 7 s, then (match started) `checkScoreLimit`, `game["roundsplayed"]++`, `checkRoundLimit`; then `game["timepassed"] += (getTime-level.starttime)/60000` (the "reset" round counts) and `checkTimeLimit`; each of them can set `level.mapended` and thread `endMap`. `endMap` -> `spawnIntermission` for all, `wait 10`, `exitLevel(false)`.
- [O] Round limit 2 is reached after the 2nd real round (the reset round does not count: `roundsplayed` is zeroed after it). The game log shows `ExitLevel: executed` 17 s after the last `W;` line (7 s + 10 s).
- [O] Timelimit 1.1 min: reset round ends ~28 s, R1 (3 s + 7 s) brings it to ~0.6 min, R2 (30 s + 7 s) to ~1.25 min >= 1.1, so the limit triggers at the end of R2 and not earlier (no `map_restart` after R2: games log shows `ExitLevel` and no further round).
- [O] Round expiry (`scr_sd_roundlength 0.5`, the stock minimum is the grace period): `W;axis` (defenders) with no kill line.
- [O] Last-round win kinds covered: bomb explosion (toujane/2), elimination (toujane3/R3), time-out (carentan/R2) and defuse as non-final (toujane3/R1).
- Unlike the earlier `gt_sd` cases the sessions are not cut during the intermission: the original calls `exitLevel`, which restarts the same map (no rotation configured), the clients receive a second gamestate with `sv_cheats` 0 (no scenario action is needed after that) and quit at the end.

## Determinism

- [O] Original: 3 runs per case, `compare`: 100 % identical (`{'identical': N}`) with no mask. The map-load timing family documented in `gt_sd.md` did not appear because no action depends on the post-reload frames and the reload timing was equal in all 3 runs.
- [O] Recon vs original: 2 runs each, 0 mismatches, 0 missing, 0 extra. Games logs differ only in the wall-clock minute stamp (`0:05` vs `0:06` for the first join line).

## Fixes

None needed: all three cases matched at first run (earlier lane fixes, notably the intermission flush at 0x08109616, already cover the endMap path; both end paths here send the final scoreboard through it). [O]

## Not covered / limits

- [U] Map rotation after the match end (`sv_mapRotation` into another map or gametype) and scores carried across it; a limit of exactly equal time (`timepassed == timelimit`) is not constructible with stable results because it depends on frame times.
- [U] Tie (`MP_THE_GAME_IS_A_TIE`) as a final result: needs equal scores at the limit; not exercised (all final scores were unequal).
- [U] `updateGametypeCvars` changes at runtime (changing the limits via rcon mid-match) not exercised.
