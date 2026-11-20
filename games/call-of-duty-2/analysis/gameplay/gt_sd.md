# Gameplay lane gt_sd: Search & Destroy (`g_gametype sd`)

Plugin: `tools/oracle/scenarios/gt_sd.py`. Original `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a7`). Tags: [O] observed, [I] inferred, [U] unknown.

## Cases

| Case | Exercises | Items (identical on original) | Original runs | Recon runs | Mismatches |
|---|---|---|---|---|---|
| `gt_sd_rounds_mp_toujane` | Join via the stock team/weapon menus, the "reset" round, `map_restart`; R1 plant at site A and bomb explosion that kills the planter; R2 plant at site B, defuse by the defender; R3 attacker shoots the defender (elimination); score limit 2, `endMap`, intermission | 164579 | 3 | 3 | 0 |
| `gt_sd_rounds_mp_carentan` | Same join and reset; R1 two interrupted plants (progress < 5 s), 30 s round timer expires (defenders win); R2 plant, defender stands in the blast radius, both die in one frame (planting team wins); R3 defender `kill` (MOD_SUICIDE), elimination; score limit; `endMap`, intermission | 181824 | 3 | 3 | 0 |
| Regression `duel_dm_toujane` (`--tag=-gt_sd`) | | 84560 | 3 (existing) | 1 | 0 |

## Design (from stock GSC `iw_15.iwd` `maps/mp/gametypes/sd.gsc` and the map entity lumps)

- [O] Both players join through `mr <serverid> <menu> <response>` (team menu, then weapon menu). When the second player spawns, `checkMatchStart` runs `endRound("reset")`, waits 7 s and calls `map_restart(true)` (`InitGame` is logged again; `d 13 <time>` rewrites the level start time). Every later round also ends with `wait 7` and `map_restart(true)`.
- [O] Timeline keys: a decision is `(number of "d 13" restarts seen since the current gamestate, snapshot server time minus the latest configstring 13)`. All of it comes from the deciding client's own received data.
- Bomb sites were taken from the BSP entity lump and the model bounds (lump 35):
  - toujane zone A origin (1047,2750,91), box x 909..1183, y 2590..2917, z 41..143, with a wreck whose top is at z 124 (the wreck collides, so a player teleported to z 60 gets stuck inside it); zone B origin (2834,1634,92), box z 44..132.
  - carentan zone A origin (-108,2540,18), ground z -32 at (-108,2400); zone B's centre is a player kill zone (`MOD_SUICIDE` 100000 within 0.7 s).
- [O] `setviewpos x y z yaw` treats z as eye height (feet = z - 59). The scenario passes z=300 (or 100) and lets the player fall onto the ground or prop.
- [O] A planting player needs `useButtonPressed` for about 5 s (0.05 s steps; the log line `A;guid;num;team;name;bomb_plant` follows). The defuse trigger serves one player at a time: a second player that is still holding use (the planter) starves the defuser, so the planter releases use before the defender presses it. A defender who lands on a different surface (toujane site B car roof, z 105) is outside the 0..56 trigger box and the defuse never starts.
- [O] Bomb timer: `scr_sd_bombtimer 30` (stock minimum). `radiusDamage(range 500, 2000..1000)` kills players in range (`MOD_EXPLOSIVE`, damages 1251..1978 logged).
- `scr_sd_scorelimit 2` ends the map in R3 (allies 2 / axis 1 on toujane; axis 1, allies 2 on carentan). `scr_sd_roundlength 0.5` (carentan) gives a 30 s round that expires with `MP_TIMEHASEXPIRED`.

## Determinism

- [O] Original: 3 runs per case, run concurrently (3 processes), `compare`: 100 % identical (`counts: {'identical': 164579}` / `{'identical': 181824}`), no mask. The seed is pinned as in the other cases (`G_InitGame` breakpoint).
- [O] The `map_restart` hitch (a 1.2..2.1 s frame logged as `Hitch warning`) did not change any recorded server time on the original or on the reconstruction in these cases. The `d 13` values are equal in all runs: toujane 28050, 72100, 98150; carentan 28050, 65250, 109300.
- Deliberately avoided: a map rotation inside a case. I built it first (`sv_mapRotation "gametype sd map mp_carentan"`; after `map_rotate` the clients get a second gamestate, `sv_cheats` is 0 so `setviewpos` no longer works) and found a timing family, see below. The cases end during the intermission instead (the end falls between `pm_type 5` and `exitLevel`), so no gamestate change happens.

### Timing family seen and not hidden: first snapshots after a map change [O]

- With rotation, the first 3-4 snapshots after the second gamestate arrive at `cs13 + 1950, 2050, 2150` on the original in 6/6 runs (carentan `+1750`), and at `+1900/+1950` (carentan `+1700/+1750`) on the reconstruction, varying between its own runs (toujane recon: 1900, 1950, 1900). The last snapshot of the old level and the new `cs13` are equal (129350), so the offset is the wall-clock duration of the map load (about 1.61 s on the recon versus 1.68 s on the original between the last old snapshot and the first new one).
- A scenario action keyed to `rel >= 2000` therefore fired on different server frames, and in one recon run that moved the second level's joins by a frame and changed ~10000 items. Actions must be keyed at least 500 ms after the first snapshots (`rel >= 3000`).
- Records at `e2:t+1900 .. +2200` (about 330 items) then still differ between runs of the same binary. This is a map-load-time family like the known `map_restart` step in `gameplay-parity.md`; no code defect was found. It is the reason rotation is not in these cases.

## Fix

| Chunk | Original | Summary | Evidence |
|---|---|---|---|
| `chunk_049.cpp` `G_FlushPendingTurretUpdates` (misnamed; it is the "send scoreboard to clients in intermission after a score change" flush) | 0x08109616 | The test was `client->ps.commandTime == 5`; the original compares `[client+0x4] == 5`, i.e. `ps.pm_type == PM_INTERMISSION`. | Disassembly: `cmp DWORD PTR [eax+0x4],0x5` with client stride 0x28a4 and `connected` at +0x26c4. Gameplay: during the intermission the original sent `b 1 1 2 0 0 0 1 0` to A in the same message as B's `MP_DISCONNECTED`; the reconstruction sent nothing (`A:server_command:0252` / `:0243` missing in 3/3 runs of both cases). After the fix 0 mismatches in 6/6 runs. |

The commandTime field never equals 5 during normal play, so the flush never ran; the effect appears whenever a client count or score changes while a client is in the intermission (dm already uses this path, but the dm cases end before any change).

## Not covered / limits

- [U] Map rotation out of sd and the round after it: needs a cheat-independent plant (no `setviewpos` after `map_rotate`) and the map-load timing family above.
- [U] `scr_sd_roundlimit` and `scr_sd_timelimit` endings, dual/single bomb modes (`level.bombmode` is hard-wired 0 in the stock script), friendly fire, team switching and `menuAutoAssign` randomness, killcam after a mid-round kill (the R3 victim is the last player of its team, so the script skips the killcam), spectator permission changes.
- Plants are made by teleporting (cheats), not by walking; pathing between spawn and site was not exercised.
- Items cover server-visible state of the two scripted clients; HUD elements (clock, progress bar) are visible only through configstrings/hud-element reliable commands that the existing decoder records as server commands.
