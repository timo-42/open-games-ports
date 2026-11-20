# Gameplay lane gt_tdm: Team Deathmatch (g_gametype tdm)

Original: `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a3a7`). Plugin: `tools/oracle/scenarios/gt_tdm.py`. Tags: [O] observed, [I] inferred, [U] unknown.

## Cases

| Case | What it exercises | Original runs: identical items | Recon runs | Mismatches |
|---|---|---|---|---|
| `gt_tdm_teams_mp_toujane` | 4 clients, `scr_friendlyfire 1`, tdm stock GSC | 3: 221204 (0 variable) | 3 | 0 |
| `gt_tdm_rotate_dm_mp_toujane` | 2 clients, enemy kill, time limit, `sv_mapRotation "gametype dm map mp_carentan"` | 3 (padded, see below): 123909 identical, 24 missing in some runs, 3 variable | 3 (padded) | 0 |
| `duel_dm_toujane` (regression, `--tag=-gt_tdm`) | existing | 1 | 1 | 0 (84560 items) |

### gt_tdm_teams_mp_toujane (T0 = server time 20000, 52 s)
- [O] Team join: A `mr allies` (explicit team menu response, `menuAllies` incl. `getJoinTeamPermissions`), B/C/D `mr ... autoassign` at +300/+600/+900 ms (`menuAutoAssign`): B goes to the smaller team (axis), C hits the tie branch (`randomInt(2)`, pinned seed), D the "smaller team" branch, so teams are 2v2 for any seed. The weapon is chosen from the `v g_scriptMainMenu "weapon_<team>"` command the server sent that client (client-side data only).
- Team kill (+16 s): A and its team mate X are teleported (`setviewpos`, devmap cheats) to the marks p0/p1, A aims from its own snapshot and fires: friendly damage lines `D;...;allies;X;...;allies;A;thompson_mp;35;...`, `K;` with the same team, A `score--`.
- Enemy kill (+25 s): B teleported to p1, A kills B: `score++`, `setTeamScore(allies)`, `checkScoreLimit`, killcam for B.
- Axis kill (+32 s): enemy Y kills A (axis team score).
- Misc: `say` (Y), `say_team` (A), `score` (A), suicide `kill` (X), mid-game team change B `mr allies` (`switching_teams` suicide, `K ... MOD_SUICIDE`), respawn by holding use, 4 disconnects.
- Roles X/Y are resolved from A's own clientState `team` fields (recorded in `scenario_notes.roles`).
- [O] Original games_mp.log: `D`/`K` for fakeD by fakeA (allies vs allies), `K` fakeB by fakeA, `K` fakeA by fakeC (axis), `say`, `sayteam`, two `MOD_SUICIDE`.

### gt_tdm_rotate_dm_mp_toujane
- `scr_tdm_timelimit 1.25`: A kills B (team score allies 1:0), then `endMap`: `W;allies;...` / `L;axis;...`, intermission, `ExitLevel`, `map_rotate` loads mp_carentan with `g_gametype dm` (second InitGame line shows `g_gametype\dm`), second gamestate, players respawned by the dm script, `score`, `say map2`.

## Determinism controls
- Pinned `G_InitGame` seed (default): `randomInt` (autoassign tie, spawn points) is reproducible.
- All decisions are a function of the deciding client's own snapshot and server time; `once` keys; roles from A's snapshot.
- [O] Teams case: 3 original runs fully identical (221204 items).

### Timing family: wall-clock map-load duration after `map_rotate` (rotation case only)
- [O] After the rotation the first second-level snapshot time is the wall-clock duration of `SV_SpawnServer` rounded to the 50 ms grid (the level start is configstring 13 = 85200; `Hitch warning` on the second load). The original's own hitch varied between runs: 1030, 1031, 1033 ms (runs 1-3) and 916, 954, 930 ms (runs 4-6, other host load), giving first snapshots at +1950 / +1850 / +1900. Spawn point choice and falls then shift with the frame count, so every epoch-2 snapshot (A origin differs for the whole level) differs. All other items (gametype switch, W/L lines, configstrings, baselines, logs) matched.
- [O] Proof it is wall clock and not a recon defect: the reconstruction ran at a hitch of ~905 ms and mismatched (910 items, only `A:e2:*`); when the second `SV_SpawnServer` was padded by gdb to the original's 1030 ms hitch the 124080 original items matched with 0 mismatches (run `-stall`).
- Control used for the final result: a gdb python script pads the second `SV_SpawnServer` to a fixed 1.15 s wall time on both servers (breakpoint at its entry, finish breakpoint at its return, `time.sleep(rest)`), so the hitch is 1157 +- 1 ms on both (`PADDED rem=0.2x`). Entry addresses: original 0x08093520, reconstruction 0x08091935 (`nm`). The scripts are `.local/cod2/lanes/gt_tdm/pad_orig.gdb` and `pad_recon.gdb`; run with
  `gameplay.py run gt_tdm_rotate_dm_mp_toujane N --server original|recon --gdb-extra <script>`. The unpadded captures are kept as `gameplay_gt_tdm_rotate_dm_mp_toujane-nopad`.
- [O] Remaining variable items in the padded originals: one snapshot at `A:e2:t+02250` is missing in run 2 (and `commandTime -100` at +2300), and the `lastmsg` column of the ZMBI row in `rcon status` (1950 / 2000). Both are wall-clock (snapshot rate limit after a large first snapshot, last packet time) and are excluded by the classifier as variable.

## Fixes
1. `Scr_AddEntityFieldValue` 0x0811af3c (chunk_051): types 3 (string id) and 7 (object handle, e.g. the client's `pers`) read the field as signed `int16`; the original uses `movzx word` (0x0811af60, 0x0811b024). Object ids >= 0x8000 were sign-extended to 0xFFFFxxxx, so `self.pers` evaluated to an invalid object and scripts failed with `undefined is not an array` (`_teams.gsc` line 414, thread `onPlayerConnecting`, and later `self.pers["team"]` readers). [O] Found because `ClientConnect` got `pers=48811` (`PROBE` via gdb) in ~30% of runs: the id comes from the script variable free list and depends on the allocation history. Effect: some clients stayed spectators, `ui_allow_join*` commands and team assignment differed, teams swapped. Before the fix 2 of 5 solo runs diverged; after it 3/3 teams runs and 3/3 padded rotation runs match.

## Not covered / limitations
- scr_friendlyfire 2 and 3 (reflected / shared damage), `scr_tdm_scorelimit` ending the map, team balance (`scr_teambalance`), spectator team join, hit sounds are not driven.
- Rotation determinism needs the `SV_SpawnServer` pad above (harness request).
- The pad changes nothing but wall time; it was applied to the original and the reconstruction alike.
