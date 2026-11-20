# CoD2 1.3 gameplay parity: Capture The Flag (lane gt_ctf)

Tags: [O] observed in a run on this host, [I] inferred, [U] unknown. Plugin: `tools/oracle/scenarios/ctf.py`.

## Case

`ctf_flag_cycle_mp_toujane` (`g_gametype ctf`, `+devmap mp_toujane`, T0 20000, 52000 ms, pinned seed 1234567). Players A (allies, thompson_mp) and B (axis, mp40_mp) pick team/weapon through the stock menus, then act on server-time-keyed `setviewpos` teleports (cheats via devmap). Flag positions come from the `mp_toujane.d3dbsp` entity lump [O]: `allied_flag` trigger_radius origin (4 805 -13), `axis_flag` (2263 2480 72), radius 48, height 128.

| T0+ ms | Action | Observed (original) [O] |
|---|---|---|
| 3000 | A onto the axis flag | enemy-touch pickup: `ctf_touchenemy` sounds, `MP_ENEMY_FLAG_TAKEN`/`MP_YOUR_FLAG_WAS_TAKEN`, `j_spine4` attach, statusicon |
| 7000 | A (carrying) onto the allied flag at base | capture: `ctf_touchcapture`, `MP_ENEMY_FLAG_CAPTURED`, score +10, team score 1 (`H`/`G` commands), flag returned home |
| 9000 | B onto the allied flag | pickup by axis |
| 11000-15500 | A teleports 120 units east of B and fires at B from snapshot data | 3 hits (35 dmg each), B killed carrying the flag; flag dropped, returned immediately; B killcam, respawn by holding use |
| 26000, 27500 | B takes the allied flag again, then goes to the axis flag | axis capture, axis team score |
| 29000, 30500, 32000 | B takes it a third time, moves to an axis spawn point and suicides (`kill`) | `MOD_SUICIDE` drop, flag returned immediately |
| 37000 / 46500 | A takes the axis flag again, B (after respawn) takes the allied flag a 4th time | both still carrying at the end |
| end | B then A disconnect | `dropFlag` from `Callback_PlayerDisconnect` for both carriers |

Everything is a function of the deciding client's own snapshot time. No wall-clock decisions.

## Determinism and result

- Original: 3 runs, **97359 items identical, 0 variable** [O].
- Reconstruction (`.local/cod2/lanes/gt_ctf/build/cod2_lnxded_recon`): 2 runs, **0 mismatches on deterministic items** (0 missing, 0 extra) [O].
- Regression: `duel_dm_toujane` on the same recon build (`--tag=-gt_ctf`): 84560 identical, 0 mismatches [O].
- Items: full per-snapshot playerState and entity fields (bit exact), server commands, `server.log`, `games_mp.log`, rcon replies, as in `gameplay-parity.md`.

## Fixes

None needed: the first reconstruction run already matched. [I] The CTF paths (`trigger_radius` touch via `G_TouchTriggers`, script_model show/hide and attach/detach, `objective_*`, `bulletTrace`, `istouching`, team scores) were already covered by the earlier chunk fixes.

## Findings about the harness and the map [O]

- A `setviewpos` into solid or below the floor kills the player at once (`K;...;world;none;1000;MOD_TRIGGER_HURT`). `(80,840)`, `(-13,861)` and `(1353,618)` at low z were such positions. Teleporting at floor+60 to spots where another player already stood worked. The plugin uses positions verified on the original.
- Players that overlap or stand inside the flag base are pushed out at a constant ~25 units/s (a flag-area effect; deterministic).
- Dropping a flag on this map always returned it at once (`dropFlag` -> `istouching(level.flag_returners[i])`) [O] at every tested position: two death sites near the allied flag and a suicide at an axis spawn. [I] The `trigger_hurt` brush (`*21`) has a large bounding box, so `istouching` is true everywhere.

## Not covered

- The "touch an own flag that is not at base" return (`RETURNED THE FLAG`, +2 score) and the 120 s `autoReturn`: unreachable on mp_toujane because every drop returns at once [I]. A map without such a `trigger_hurt` (mp_carentan has one too) or a GSC overlay would be needed.
- Score limit / time limit end of a CTF map (`checkScoreLimit`, `endMap`): the dm rotation case covers `endMap`, but the CTF branch is not run here.
- Flag waypoints/HUD icons are only visible as configstring and server-command effects; client rendering is out of scope.
