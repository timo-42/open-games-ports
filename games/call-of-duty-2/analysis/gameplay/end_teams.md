# CoD2 1.3 gameplay parity: end conditions in tdm, ctf and hq (lane end_teams)

Tags: [O] observed in a run on this host, [I] inferred, [U] unknown. Plugin: `tools/oracle/scenarios/end_teams.py`. Original `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a7`), pinned seed 1234567, rotation cases use the 1.150 s spawn pad of `gameplay.py`. Reconstruction build: `.local/cod2/lanes/end_teams/build/cod2_lnxded_recon`.

## Cases

| Case | Exercises | Original runs: identical / variable / missing | Recon runs | Mismatches |
|---|---|---|---|---|
| `end_teams_tdm_ctf_mp_toujane` | Epoch 1, tdm with `scr_tdm_scorelimit 2`: A kills B (3 hits, `K`), B respawns, A kills B again; the second kill reaches team score 2: `MP_SCORE_LIMIT_REACHED`, `W`/`L` log lines, `pm_type` 5 (intermission) for both clients at T+32750, `score` request 2 s into the intermission, `wait 10`, `exitLevel`. Rotation `gametype ctf map mp_downtown`. Epoch 2, ctf with `scr_ctf_timelimit 0.5`: both join through the menus (the team and weapon menu names are `team_russiangerman` / `weapon_russian`), the time limit ends the map at level time 30 s: `MP_TIME_LIMIT_REACHED`, tie (`MP_THE_GAME_IS_A_TIE`), intermission at T+30200. | 124946 / 2 / 1 | 2 | 0 |
| `end_teams_ctf_hq_mp_downtown` | Epoch 1, ctf on mp_downtown with `scr_ctf_scorelimit 2` (see "CTF map choice"): A takes the axis flag and suicides at a spawn point (`dropFlag`, flag stays: no `flag_returners`); B (axis, the owning team) touches the dropped flag: `MP_YOUR_FLAG_WAS_RETURNED`, +2. A takes the flag again, drops it, re-picks it at its death site (enemy touch of a dropped flag, `end_autoreturn`), carries it to another spawn and drops it a third time at T+42.5 s. The last drop returns by `autoReturn` (`wait 120`) at about T+162.5 s with no player involved. B then captures twice (allied flag to the axis base, which needs the axis flag `atbase`, which only the autoReturn made true): score 2, `MP_SCORE_LIMIT_REACHED`, `W axis`, intermission at T+169550, `score` 2 s later. Rotation `gametype hq map mp_toujane`. Epoch 2, hq with `scr_hq_timelimit 0.5`: time limit ends the map, intermission at T+30250. | 352073 / 4 / 149 | 2 | 0 |
| `end_teams_hq_hold_mp_toujane` | hq with `scr_hq_scorelimit 125`: A captures the first radio at T+20.25 s (radio 2 at (1282.5, 731.8)), holds it (1 point/s); at the 120 s `RadioMaxHoldSeconds` cap `hq_maxholdtime_think` -> `hq_radio_resetall` (`MP_MAXHOLDTIME_MINUTES` with `MP_UPTEAM`, radio explosion, radio hidden, `finish_staydead`), 15 s radio delay, the next radio appears at T+165.1 s (radio 3 at (2206.3, 411.6)), A captures it and holds until the score reaches 125: `MP_SCORE_LIMIT_REACHED`, `W allies`, intermission at T+179250, `score` 2 s later, disconnects inside the intermission. | 313064 / 0 / 0 | 2 | 0 |

Regression: `duel_dm_toujane` (`--tag=-end_teams`) against the recon: 84560 identical, 0 mismatches.

## Determinism controls [O]
- Every decision is keyed to the deciding client's own snapshot time (relative to T0 or, after the rotation, to configstring 13). Positions are floor-verified ctf spawn points of mp_downtown (`1769 -1293 67`, `2556 -2771 87`, `3104 -1738 38`) and the flag trigger origins from the entity lump (axis `1781 444 46`, allied `1533 -2715 35`). The `repick` teleport uses A's own last alive origin before its death, taken from A's snapshots (the death site differs from the teleport target by about 90 units because the player is pushed).
- Intermission is detected as `pm_type == 5` in the client's own playerState; the `score` request is sent 2000 ms later.
- Epoch 2 is entered with `map`, so cheats are off there: `setviewpos` does nothing [O] (A stayed at its spawn). Epoch 2 therefore only joins through the menus, moves with `Duel.usercmd2` and waits for the time limit.
- After a rotation the players are spectators again (`pm_type` 4) and must join through the menus. The team menu name depends on the map (`game["allies"]`): `team_britishgerman`, then `team_russiangerman`; the plugin reads the menu configstrings (0x4DE...) of the current gamestate and sends the matching weapon (`thompson_mp`, `ppsh_mp`, `mp40_mp`).
- The disconnects are placed inside the 10 s intermission (case lengths 184000 / 175000 ms, rotation 34000 ms after the second level start); otherwise `exitLevel` would start a third level.
- Timing families that remain (all on the original, not filtered by us; the compare treats them as variable / missing):
  - `end_teams_tdm_ctf`: B `commandTime` at epoch-2 T+26700 (-50 vs -100) in 1 of 3 runs, plus A entity 1 `trTime` (3 items): one late usercmd packet (wall clock).
  - `end_teams_ctf_hq`: the first epoch-2 records of the clients (A T+2300/2350, B T+2250..2400): A's or B's first active snapshot of the new level arrives at +50 or +100 ms depending on a wall-clock race at the map change (5 original runs of an earlier version of the case: A first record at 2100 or 2150 in a 3:2 split). The menu responses of this case are keyed to fixed times (2500 / 3500 ms) so that the race no longer changes server state; only the first records and the first `commandTime` differ. 153 of 352226 items.
  - The `lastmsg` column of zombie rows in `rcon status` is masked by `gameplay.py`.

## CTF map choice [O]
`level.flag_returners` holds every `trigger_hurt` and `minefield`. The entity lumps of the 15 stock mp maps (extracted from the `.iwd` files) give: mp_downtown has neither, every other map has at least one `trigger_hurt` (toujane 1, railyard 3, harbor 2, brecourt/decoy also minefields). `istouching` on such a brush is true for any flag position on toujane (`gt_ctf.md`), so only mp_downtown keeps a dropped flag dropped. mp_downtown has `trigger_radius` `allied_flag` / `axis_flag` and 16 + 16 `mp_ctf_spawn_*` points. [I] This is the only stock map where the return-by-touch and autoReturn paths are reachable without a script overlay.

## Fixes
None: the first reconstruction run of every case already matched. [I] The paths involved (`checkScoreLimit`, `endMap`, `getTeamScore`/`setTeamScore`, `istouching`, trigger_radius touches of a moved flag, `wait 120` timers, `hq_maxholdtime_think`, `hq_radio_resetall`, `exitLevel`/`map_rotate` with a gametype change, menu configstrings per gametype) are covered by earlier chunk fixes (intermission scoreboard 0x08109616, corpse settle 0x08107846 and others in `gameplay-parity.md`).

## Not covered
- Time limit in tdm: the same `checkTimeLimit` shape as in the other gametypes, covered by `gt_tdm_rotate_dm_mp_toujane` (`W`/`L` lines, rotation tdm -> dm). Not repeated here because `scr_tdm_timelimit` and `scr_tdm_scorelimit` would end the one map each.
- CTF with a flag carrier at the end of the map and the drop of a flag at `endMap` (no `dropFlag` call in `endMap`): carriers are not exercised at the end.
- HQ "radio timeout": hq has no other radio timeout than `RadioMaxHoldSeconds` (no other `wait` that removes a radio, `hq_obj_think` only waits `RadioSpawnDelay`); covered. Contest/destroy paths are in `gt_hq.md`.
- Epoch 2 gameplay with flags/radios (cheats are off after a rotation, no teleports; walking to a flag would need a recorded route).
