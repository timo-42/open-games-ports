# Gameplay lane wpn_mantle_turret: mantling, ladders and stationary MG42 turrets

Tags: [O] observed on this host, [I] inferred, [U] unknown. Method, harness and determinism controls are in `../gameplay-parity.md`. Plugin: `tools/oracle/scenarios/wpn_mantle_turret.py`. Original: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`.

## Result

| Case | What it exercises | Original runs: identical / variable items | Recon runs | Mismatches |
|---|---|---|---|---|
| `wpn_mantle_ladder_toujane` | Two ladder climbs and twelve mantle trials on `mp_toujane` (dm), A in `god` mode | 113918 / 0 (3 runs) | 3 | 0 |
| `wpn_turret_mg42_toujane` | Mount, sweep, fire, kill with and dismount of three `misc_mg42` | 70087 / 0 (3 runs) | 3 | 0 |
| `duel_dm_toujane` (regression) | unchanged | 84560 / 0 | 1 (`--tag=-wpn_mantle_turret`) | 0 |

Recon binary: `.local/cod2/lanes/wpn_mantle_turret/build/cod2_lnxded_recon`, built with `link_test.py --no-gen --skip-before`.

## Design

### Finding the features [O]
- [O] The `.d3dbsp` source maps inside `iw_03/04/15.iwd` carry entity strings and the brush lump. `mp_toujane` has three `misc_mg42` in the `dm` gametype (entities 308 at 662,24,197; 311 at -80,1362,33; 517 at 2165,2397,89, weapon `mg42_bipod_stand_mp`). It also has 47 brushes that use the `mantle_on`, `mantle_over` and `ladder` materials.
- [O] Brush parse used for the table: header `IBSP` v4, 39 lumps of (length, offset); lump 0 = materials (72 bytes: name[64], surfaceFlags, contentFlags), lump 5 = brush sides (8 bytes: float dist for the first six axial sides or plane number, then material), lump 6 = brushes (4 bytes: numSides, material). The first six sides give `xmin,xmax,ymin,ymax,zmin,zmax`. The floor is a terrain soup and is not in the brush lump, so floor heights were found by dropping the player (below).
- [O] `setviewpos x y z yaw` takes the **eye** height: the origin becomes `z - 59` and the player then falls. Landing on a floor gives origin z = floor + 0.125. Teleporting below the floor gives a stuck player (no ground, no fall) and teleporting into the void is a `MOD_TRIGGER_HURT` death at z -64..-56. The mantle case therefore runs with `god`.
- [O] Mantle needs `pm->cmd.buttons & 0x400` (jump) while a `mantle_on`/`mantle_over` brush is inside `mantle_check_range` 20 in front of the player (`Mantle_FindSurface` traces with content mask 0x1000000; `Mantle_CheckLedge` probes at +60/+40/+20). `ps.mantleState.flags` bit 8 means "mantle available" even without jump.
- [O] Turret use needs the user on the ground (`Turret_CanBeUsed` rejects `groundEntityNum == 0x3ff`) and **behind** the gun: the horizontal direction user->turret must be inside the turret's yaw arc (`Turret_IsAimingAtUser`, arcs -45..+45 for the MG42). `cursorHint` becomes 6. Holding use (0x8) for 300 ms mounts: `viewlocked_entNum` = turret entity, `eFlags` 0x300 (mounted) and 0x40 (firing), and `viewAngleClampBase/Range` follow the turret arcs. Pressing use again dismounts.
- [O] There is no overheat state in the playerstate: 4 s of continuous fire on an MG42 changes only `eFlags` 0x40. [U] Whether the weapon def has a heat model that is not networked.

### Cases
`wpn_mantle_ladder_toujane` (t0 20000, length 56000, absolute server time):
- Both clients take the stock team/weapon menus; A sends `god` at +1500.
- A is teleported (+3000, +8000) in front of two ladder brushes (4510, 4502) and walks forward for 3 s: 158 snapshots with `pm_flags & 0x20`, `vLadderVec` set, climbs z 6 -> 160 and 84 -> 256.
- A is then teleported (every 3 s from +13000) in front of twelve mantle brushes (4364, 4365, 4366 x2, 4381 x2, 4383, 4386, 4390 x2, 4392 x2; both sides where the table says so), walks forward and holds jump from +1300.
- [O] 14 mantle starts (two brushes re-trigger after landing), 245 snapshots with `pm_flags & 4`, all seven stage rows `mantleState.transIndex` 0..6 and `mantleState.flags` 2, 4, 5, 7.

`wpn_turret_mg42_toujane` (t0 20000, length 34000):
- A teleports behind mg 308, mounts at +4200, sweeps the barrel +25 degrees over 1.5 s, then aims at B (standing at the bait point 700,300 on the plateau in front, aiming from A's own snapshot of B's entity) and fires for 2 s. B dies (`D;`/`K;` lines, K weapon `mg42_bipod_stand_mp`, D weapon `thompson_mp` [O], an original quirk).
- A dismounts, teleports behind mg 311, mounts, sweeps +-30 degrees and fires 4 s, dismounts.
- B holds use in the killcam, respawns, teleports behind mg 517 (the teleport is keyed to B's own `pm_type == 0` after t >= 14000), mounts, sweeps and fires 3 s, dismounts. A requests `score` at the end.

### Determinism
- [O] Both cases are bit-identical across 3 original runs (113918 and 70087 items, 0 variable) with the default pinned seed and no masks. Every decision uses server time and the deciding client's own snapshot.
- [O] The harness's fixed `rcon status` and game-log normalisation (J-line time, ping fields) are the only normalisations in play.

## Fixes (original address, evidence)

Each was found by comparing a PM trace dump (gdb Python `FinishBreakpoint` on `PM_TraceRaw` 0x080e1308, mask 0x1000000 and all masks) or `calltrace.py` argument diffs, then reading the original disassembly.

1. `Mantle_CheckLedge` 0x080dd65a (chunk_040). The first probe matched; the second trace was `start=end` in the recon. The original stores `ledge->start[2] + 18.0` into `end[2]` (`0x080dd7b8..0x080dd7c3`) and sweeps down from the old end. The original also tests the trace's `startsolid` byte (`[ebp-0x25]`, trace + 0x23) at 0x080dd774, 0x080dd811 and 0x080dd8f1, not `allsolid`, and compares `normal[2]` against the **float** 0.7 (0x81535a8, 0x3f333333). [O] Before the fix `mantleState.flags` 8 was never set (24k mismatches in the first case).
2. `Mantle_ComputeLedgeTarget` 0x080dd3dc (chunk_040). The probe start is `ledgePos` (+0x18) + 31 * dir (`0x080dd45f..0x080dd481`), not `ledge->start` (+0x0c); `startsolid` again. Effect: wrong `ledge->end`, wrong mantle origin (x off by 0.23) and the surface bit 1 of `mantleState.flags` lost.
3. `Mantle_UpdateMove` 0x080de260 (chunk_040). The velocity scale is `1 / ((newTime - oldTime) * 0.001f)` in x87 extended with the float constant at 0x8153928 (`fild; fld c; fmulp; fld1; fdivrp`). The recon used the double 0.001. Effect: 1 ulp in `velocity` during the landing frames.
4. `XAnim_NodeDurationMs` 0x080c0d6c (chunk_033). `fistp` runs under control word `| 0xc00` (truncate), the recon rounded to nearest (`rintl`). [O] The up animation of the first stage is 666 ms in the original and 667 ms in the recon, so the blend fraction differed (0.075075 vs 0.074963) and the mantle path was off by 0.0004. [I] The unrounded duration lies between 666.0 and 667.0.
5. `Turret_IsAimingAtUser` 0x0810c1be (chunk_049). `dir[2] = 0` before `Vec3_Normalize` (`0x0810c26c..0x0810c271`, `[ebp-0x20]`). Without it the 37-unit height offset pushed the user outside the 45 degree cone, so `cursorHint` 6 never appeared and a turret could not be used.
6. `Turret_Occupy` 0x0810c3de (chunk_104). The yaw uses `currentAngles[1]` (+0x148) in `angles2[1]` and `viewAngleClampBase[1]` (`0x0810c593`, `0x0810c67a`); the recon read `[2]`. Effect: `viewAngleClampBase[1]` missing, the view clamped to 45 degrees, `Turret_SetInitialAngles` clamped against a wrong delta and the user was teleported out of the map and killed.
7. `Turret_PositionUser` 0x0810aa50 (chunk_049), three defects:
   - `idx = (int)lrintf(frac)` -> truncation (`fistp` under `| 0xc00`, `0x0810ad98..0x0810adb9`).
   - Decompiler-split matrices (defect class 1 of `../gameplay-parity.md`): `local_9c[9]` + `auStack_78[3]` and `local_dc[9]` + `auStack_b8[3]` are two 4x3 matrices that `Mat34_ConcatRowVector` reads as 12 floats (`[ebp-0x98]`/`[ebp-0xd8]`, origin rows at +0x24).
   - `XAnim_BlendRootAll` 0x080c15ba writes a vec3 at `[ebp-0x68]`; the loop exit test, `prevDist` and the segment blend read its `[2]` (`[ebp-0x60]`, 0x0810aeee/0x0810b159), not `[1]`, and the array was 2 floats long. The first mount (mg 308) matched by luck, the second (mg 311) took the multi-segment path.

## Still uncovered
- [U] Turret overheat / heat state: no networked field changes. The `lrintf(ent->u.turretRateMs)` in `Turret_Advance` (chunk_049, near the original 0x0810d1a8; compare 0x0810cf4c..) is not proven to be a truncation; the cases fire the turret but the path where `trTime + rate` is reached was not diffed beyond zero mismatches.
- [U] Turrets in other gametypes (`tdm`, `sd`, `hq` entities of the same maps), `misc_turret` (not on `mp_toujane`), and MG42 use by two players at once or by a player who is pushed off ground.
- [U] Ladders: only two brushes are in the case (in the exploration run the other eight start positions either landed away from the ladder or only touched its top). Dismount at the top and jumping off a ladder are not covered; the harness has no way to aim at a ladder from its own snapshot beyond the fixed teleport.
- [U] Mantle with `mantle_debug`, crouched mantles (`eFlags & 4`), mantles that fail the "can't fit" probe, the 20/40 unit probes (all twelve trials mantle at 60 or 40 [I]).
- [U] Killcam of a player killed by a turret (not run; B respawns through the stock killcam as in `duel_dm_toujane`).

## Harness notes
- No harness change was needed. The plugin keeps one `Base(Duel)` with `more_actions`; `actions2/usercmd2` after a map change are unused.
- The exploration cases used to find the brush/floor/turret geometry were removed from the plugin. They are reproducible from the parse described above.
