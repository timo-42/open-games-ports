# Gameplay lane wpn_turret2: turret contest, kill + killcam, death on the turret (tdm), ladders, crouched and failed mantles

Tags: [O] observed on this host, [I] inferred, [U] unknown. Method and determinism controls: `../gameplay-parity.md`. Round 1 of this feature: `wpn_mantle_turret.md`. Plugin: `tools/oracle/scenarios/wpn_turret2.py`. Original: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`. Map `mp_toujane`, pinned seed 1234567, T0 20000.

## Result

| Case | What it exercises | Original runs: identical / variable items | Recon runs | Mismatches |
|---|---|---|---|---|
| `wpn_turret2_tdm_toujane` | tdm, three clients: two users on one `misc_mg42`, two turret kills with the victim killcam, suicide on the turret, death on the turret | 93667 / 0 (3 runs) | 2 | 0 |
| `wpn_turret2_ladder_toujane` | seven ladder trials | 100096 / 0 (3 runs) | 2 | 0 |
| `wpn_turret2_mantle_toujane` | nineteen mantle trials (failed, crouched, prone, standing) | 113606 / 0 (3 runs) | 2 | 0 |
| `duel_dm_toujane` (regression) | unchanged | 84560 / 0 | 1 (`--tag=-wpn_turret2`) | 0 |

Recon binary: `.local/cod2/lanes/wpn_turret2/build/cod2_lnxded_recon` (`link_test.py --no-gen --skip-before`). No masks, no filtered items. One recon defect found and fixed (below); ladders and mantles matched on the first recon run.

## Overheat: not a feature of this binary [O]
- [O] `strings` of `cod2_lnxded` has no "overheat"/"heat" string (the only `heat` hits are `cheat`). The weapon file of `mg42_bipod_stand_mp` (`weapons/mp/mg42_bipod_stand_mp` in `iw_13.iwd`) has no heat key; its turret fields are `leftArc/rightArc/topArc/bottomArc 45/45/15/15`, `fireTime 0.05`, `horTurnSpeed`, `vertTurnSpeed`, ... [O] Round 1 also saw no networked change over 4 s of continuous fire (only `eFlags` 0x40).
- [O] `Turret_Advance` (0x0810de4e, chunk_049, the item left open in round 1) is **not** a turret function. It is called from `G_RunMissile` (0x0810e0ce, chunk_050) only when `weapon->weaponType == 2` (projectile) and the entity is not flagged 0x20000; it steers a projectile's trajectory by `destabilizationAngleMax/BaseTime` and re-arms `turretRateMs`. The `Turret_*` names of `Turret_Advance`, `Turret_UpdateFromInput` (bounce reflection with `parallel/perpendicularDefaultBounce`), `Turret_ProcessInput` (impact), `Turret_TraceAimWithFlags` (missile trace) in chunk_049/050 are decompiler guesses for missile code. [I] The real turret functions are `Turret_CanBeUsed/Occupy/Think/UpdateUser/ReleaseUser/PositionUser` (chunk_049/104).
- [O] gdb breakpoints on `Turret_Advance` and `G_RunMissile` (entry, both servers, whole tdm case, `--tag=-adv`): **0 hits on the original and 0 on the reconstruction**. So an MG42 never reaches the timing code that round 1 worried about; there is nothing to match there. Overheat is therefore unreachable and not observable with stock data (no heat model exists), not merely unnetworked.

## Cases

### `wpn_turret2_tdm_toujane` (tdm, 30 s; A allies thompson, B axis mp40, C allies m1carbine)
`mp_toujane` has the same `misc_mg42` entities for `tdm` as for `dm` (`script_gameobjectname "tdm"`, 308 at 662 24 197) [O]. All teleports are `setviewpos` (devmap cheats); every action is keyed to the deciding client's own snapshot time.
- P1: A and C stand 32 units apart behind mg 308 (`MG1`, `MG1C`). C presses use at +4000 and mounts at +4050 (`viewlocked_entNum` 307, `eFlags` 0x300). A presses use at +4200, +5000 and +7000 while C is on it: nothing happens (`Turret_CanBeUsed`: `ent->active != 0`) [O]. C sweeps the barrel and fires at B (standing at `BAIT`, aimed from C's own snapshot of B): K line `K;..;axis;fakeB;..;allies;fakeC;none;28;MOD_RIFLE_BULLET` [O] (weapon `none`, damage 28 in tdm; round 1's dm run logged `mg42_bipod_stand_mp`/35 [O], an original quirk of the tdm script path, not investigated).
- Killcam [O]: after B's death (`pm_type 6` at +6700) B's snapshot ps becomes the **followed player's** (`clientNum` 2 = the killer C, from +8750 to B's respawn, `viewlocked_entNum` 307 while C is on the turret). `eFlags` bit 2 is the teleport toggle, not a liveness flag; liveness for the script is `pm_type == 0` and `clientNum` = own. B must hold use to respawn (stock `waitRespawnButton`); its respawn lands at +16450 at a tdm spawn point.
- P2: C dismounts at +9500 (the release teleports C back to the pre-mount spot). A mounts at +10000 (mounted at +10050); C tries twice (+10800, +11500): nothing. A sends `kill` at +12500 on the turret: `MOD_SUICIDE` K line, A dies mounted (`pm_type 6`), the turret is released (`Turret_Think` else branch), dropped weapon and corpse appear.
- P3: A (respawned) and C teleport behind the gun again. A and C press use in the **same** frame at +19000: A (client 0) mounts, C does not. B (respawned, teleported to `BAIT`) shoots A on the turret from +19000: two `D;..;mp40_mp;45` hits and A dies on the turret (`K;..;fakeA;..;fakeB;mp40_mp`), A's own killcam follows.
- P4: C mounts the freed turret at +22500, sweeps, fires at B: second turret kill and second B killcam. Round-1-style `score` at the end.
- 93667 items, bit-identical across 3 originals [O]; 0 mismatches over 2 recon runs.

### `wpn_turret2_ladder_toujane` (dm, 52 s, A in `god`)
Trials are 6.5 s apart; the start points are 26 units in front of the ladder face (brush indices of the d3dbsp brush lump of `maps/mp/mp_toujane.d3dbsp`, material `ladder`, parsed as in round 1: header `IBSP` v4, 39 lumps, lump 5 sides, lump 6 brushes):
1. 4510 from the south: climbs ~70 units, then jump + backward move: jump-off, fall to z -16 (`pm_flags` leaves 0x20) [O].
2. 4498 from the east, set in mid-air next to the ladder: grabs it (0x20), climbs 30 units and steps over the top (top-out dismount at z 153, walks on over the plateau) [O].
3. 4503 from the west (eye 130, mid-ladder): climbs a little, hangs (`forwardmove 0`), looks down 60 degrees and moves forward (descend) [O].
4. 4313 from the east: climbs with the jump button held from +3000, top-out with jump [O].
5. 4501 from the north: climb, crouch on the ladder (+2200), strafe right and left off it, walk back in [O].
6. 4502 from the east: 100 ms jump tap on the ladder, then climb on [O].
7. 4510 again: climb, then move backward (reverse down) [O].
- [O] 100096 identical items over 3 originals; 0 mismatches over 2 recon runs. Ladder code (`PM_LadderMove` chunk_042) needed no change.

### `wpn_turret2_mantle_toujane` (dm, 60 s, A in `god`)
Same program as the round-1 exploration (2.8 s per trial: teleport 26 units from a `mantle_on`/`mantle_over` face and 40 above its top, walk forward from +1000, jump from +1200) with five stance modes: `jump`, `crouch` (crouch held all trial), `prone`, `uncrouch` (crouch for 1.1 s, then release and jump) and a final standing mantle.
- [O] Outcomes were read from `Mantle_DebugPrint` (0x080dd0f8) under gdb, time-stamped with the level time, on the original **and** the reconstruction (`--tag=-md`, `mantle_debug` is not needed because the print function is entered anyway): **6748 messages, identical sequences** on both servers. Counts: "No mantle surface found" 1481, "Ledge is too far away" 1026 (all three probe heights 60/40/20 fail on six brushes: 4365n, 4386w, 4392s, 4375w, 4390w, 4368n), "Can't find ledge" 174, "Player not standing" 134 (crouch and prone trials), "Mantle brush is too thick" 122 (4365s, 4370n), "Player already mantling" 104, "Player is not facing mantle surface" 62 (4381s, 4383e), "Mantle available!" 45, "Ledge is too steep" 3.
- [O] Crouched / prone mantle: never starts (`Mantle_TryStart` rejects `eFlags & 0xc`, message "Player not standing"). The crouch-then-stand trial mantles once the stand-up completes.
- [O] 113606 identical items over 3 originals; 0 mismatches over 2 recon runs; no mantle code change was needed.

## Fix (original address, evidence)

`XAnim_SetSlotWeight` 0x080c20de (chunk_033): `slot->oldWeight = weight * 0.001` used the **double** 0.001. The original (`0x080c21c7..0x080c21d2`: `fld [ebp+0x10]; fld [0x8150274]; fmulp; fstp [slot+0x14]`) multiplies by the float constant 0.001f (bytes `6f 12 83 3a` = 0x3a83126f) in x87 extended and rounds once. Fix: `(float)((long double)weight * (long double)0.0010000000474974513f)`. Comment `gameplay-fix(wpn_turret2)` in the source.
- Symptom [O]: the dropped weapon of a player who died on the turret (`dropWeapon` -> `Player_DropWeapon` places the item at the `tag_weapon_right` world position) differed in `pos.trBase` by 1e-6 (13 mismatches in 93667 items before the fix, entities 125/140/141 for A, B, C, e.g. trBase[1] -0.468784064 vs -0.468789726).
- Localisation: `calltrace.py` (window `Player_DropWeapon` -> `G_SetAnglesTrajectory`, chunks 052/048/032/033/031) showed `XAnim_EvalNodeParts` called with weights 0x392e67e0 (original) vs 0x392e67df (recon); gdb `Vec3_TransformAffine` inputs (slot origin) differed with identical matrices; dumps of `g_xanimSlotRing` at level times 24050/24100 (`.local/cod2/lanes/wpn_turret2/`) put the first diverging word in slot 16 at the first update after C's mount: `weight` 0x3a031e5b vs 0x3a031e5a with identical target 0x3f000ba4 (0.50017762). The slot's blend start weight is `target * 0.001f` (0.0005001776456 rounds to ...5b with the float constant, to ...5a with the double).
- [O] Only mounted-user poses reach this (the turret mount animation crossfade). `XAnim_UpdateBlendRecursive` (0x080bf112) was compared by disassembly: both sides evaluate `(target - weight) * dt / remaining + weight` on the x87 stack with one rounding, so it needs no change.

## Still uncovered
- [U] Overheat: does not exist (see above); not reachable.
- [U] `misc_turret` (not placed on `mp_toujane`); turrets on other maps; `sd`/`hq` turrets (the maps' 517 turret has `sd`).
- [U] Mantle messages not reached: "Player can't fit crouched on ledge", "Mantle surface has vertical normal", "Player is dead", "Player using binoculars", "Not enabled" (mantle_enable 0). [I] The `ps->eFlags & 4` branch inside `Mantle_CheckLedge` is unreachable from `Mantle_TryStart` because that function returns early when `eFlags & 0xc`.
- [U] Ladders on other maps; a ladder entered with a weapon swap or while firing.
- [U] The tdm `K;` line for a turret kill carries weapon `none`/damage 28 in the original (dm: `mg42_bipod_stand_mp`/35); this was matched but the script path that causes it was not read.
- Exploration cases (`mp0/mp1/probe`) used to find the brush/floor positions were removed from the plugin; they are reproducible from the brush parse above (lump 5/6, 26 units from each face, eye 40 above the brush top).

## Harness notes
- No harness change was needed. The plugin carries its own `Base(Duel)` with `more_actions`, and a small `watch_death` helper (per-client life counter from its own snapshots).
- Harness fact worth keeping: `setviewpos` while the player is dead/killcam-following is ignored; the plugin teleports only after the client sees its own live snapshot (`pm_type 0` and own `clientNum`).
