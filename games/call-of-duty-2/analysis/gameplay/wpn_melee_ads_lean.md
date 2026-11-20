# Gameplay lane `wpn_melee_ads_lean`: aim down sights, lean, melee

Binary: original `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a3b0451a`, see `gameplay-parity.md`). Method, determinism controls and protocol facts are as in `../gameplay-parity.md`; this note covers only what this lane added. Tags: [O] observed, [I] inferred, [U] unknown.

## Cases (plugin `tools/oracle/scenarios/wpn_melee_ads_lean.py`)

Both cases are dm on mp_toujane (`+devmap`, seed pinned to 1234567 by the harness), two clients (thompson / mp40), T0 = 20000.

| Case | Original runs | Items (identical / variable) | Recon runs | Mismatches |
|---|---|---|---|---|
| `wpn_melee_ads_lean_ads_lean_dm_toujane` (length 40000) | 3 | 89443 / 0 | 2 | 0 |
| `wpn_melee_ads_lean_melee_dm_toujane` (length 38000) | 3 | 76768 / 1 | 2 | 0 |
| regression `duel_dm_toujane` (tag `-wpn_melee_ads_lean`) | existing 3 | 84560 | 1 | 0 |

### `ads_lean`: stance x ADS x lean matrix, ADS fire, lean at a wall
Both players run the same table of button phases (`PHASES`, relative to T0):
- stand: ADS; ADS + forward walk; lean left; lean right; both lean bits (cancel); ADS + lean left; ADS + lean right + strafe;
- crouch + lean left; crouch + ADS + lean right + forward;
- prone; prone + ADS; prone + ADS + forward; prone + lean left; prone + lean right + ADS + strafe;
- ADS + attack for 1.5 s (spread, view kick, impacts) and hip fire for comparison.

Then the wall test (from 20500): each client walks forward along its own yaw until its own snapshot velocity is below 5 for the first time (after 600 ms; at most 5000 ms), then turns +/-90 degrees relative to that yaw, so that the wall is on its right or left, and leans toward and away from it, with ADS, crouched, prone.
- [O] A is blocked at server time 21100 at (369.125, 1058.396, 119.125); B is not blocked within the 5000 ms window and the decision falls back to 25500 (both servers).
- [O] Leaning toward the wall standing is not reduced (leanf 0.5); crouched/prone toward the wall gives a clipped lean (leanf 0.2026), so the lean trace against world geometry is exercised.
- Determinism [O]: the three original runs agree on all 89443 items. Every decision depends only on the deciding client's own snapshot and server time.

### `melee`: miss, hit/kill, killcam/respawn, kill in the other direction
`setviewpos` puts A at (387, 992) facing +y (a spot A walked to in the ADS case, known ground) and B 65 units away (out of range) facing -y; B is moved to 38 units at 4500. Timeline:
- A presses B_MELEE (0x4) at 3500 (miss at 65 units); at 5500 (hit, kills B: `K;...;thompson_mp;459;MOD_MELEE;head`), and repeats at 6500-9500 against the dead player;
- ADS then melee (ADS is cancelled) at 10500-11500;
- B holds use from 12000 (killcam end, respawn), is teleported next to A at 20000 and bashes A at 21500 (kills A: `mp40_mp;453;MOD_MELEE;head`), then further bashes at 22500-27500 with A in ADS, crouch and leaning (A is in killcam; B bashes air).
- [O] melee damage is 459/453 with head hit location, i.e. a single bash always kills at 100 health. A non-lethal melee hit is therefore not reachable in dm without lowering health first.
- [O] melee weaponTime 220 (weapAnim 520) per bash with a thompson, 150 for the later bashes.

## Determinism notes
- Variable item [O]: `oob:rcon_status_after`. After A's disconnect the status row is a `ZMBI` entry whose `lastmsg` column is 1950 or 2000 depending on the wall-clock phase of the last packet (50 ms frames). This is a timing family: the harness regex masks lastmsg only for numeric ping rows. It appears only in the `melee` case (the ADS case always gave 2000); it is not a mismatch (variable items are excluded) and it was not filtered.
- No masks were added. Seed pinning is the harness default.

## Fixes (every one found by call-trace/probe diff on the same pinned scenario, then checked in the original disassembly)

1. `G_UpdateWeaponTargetLock` 0x08120a70 (chunk_053) [melee]: one 4x3 matrix at `[ebp-0x48]` (axis rows at +0, +0xc, +0x18, origin at +0x24), filled by `G_GetPlayerViewAxis(ent, m, m+0xc, m+0x18)` and `G_GetPlayerViewOrigin(ent, m+0x24)`, then `Bullet_FireFromEntity(ent, m, range, width, height)` (the dvar words at `[+8]` are passed raw). The decompile split it into `eye[6]`, `fwd[3]`, `right[3]`, `up[3]` and passed only `fwd`, so the melee trace used a garbage origin and never hit. Effect before the fix: A's melee never damaged B (A: no kill, 24323 mismatches). Same class as "locals the original keeps contiguous" in `gameplay-parity.md`.
2. `WeaponView_ComputeIdleSway` 0x080f11b0 and `WeaponView_UpdateMoveRotation` 0x080f0d74 (chunk_044, `view_motion_s` in `chunk_044_types.h`) [ads]: the view-motion slots at +4 (speed), +8 (frame seconds), +0xc..+0x14 (offset vector), +0x18 (idle factor) are floats in the original (`fld/fstp [vm+0x18]`, `fld [vm+8]` times 0.5), mirrored from gclient 0x2870/0x287c as raw dwords. They were `int32` with numeric `(float)` casts and `(int32)` stores, so the idle factor was truncated to an integer (0 or 1) and delta/speed were bit patterns read as integers. Evidence: probe on `gclient+0x287c` at ClientThink_real: original 0.974999785 / 0.999999762 after the first frames, reconstruction int 1 (1.4e-45). Effect: gun angles (`fGunPitch`/`fGunYaw`, which feed `G_GetViewFrame`) lacked the idle sway, so the second ADS shot left along a different ray (world impact missing; 112 mismatches). The sin argument is now `fild phase * float const` in extended precision, as at 0x080f1401.
3. `BG_BobCyclePhase` 0x080f0bde (chunk_044) and `WeaponView_ApplyYawBob` 0x080f14aa (chunk_045) [ads]: x87 chain. The original computes `t = byte/255.0f`, `t*pi + t*pi + 2*pi` in extended precision and rounds once to float (`fstp [ebp-4]` at 0x080f0c2c; `fstp [ebp-0x2c]` at 0x080f14ff, then `+ pi/4 + 2*pi` rounded once to `[ebp-0x10]`, then `- 0.4712389167638204` (double at 0x081558c0)). The reconstruction rounded `t` to double, used float constants (3.1415927f, 0.7853982f, 0.4712389f) and returned an unrounded long double. First difference seen: `BG_BobSinePair` phase argument 0x4155a0d8 (13.3518) vs 0x4155a0d9.

### Verified-equal functions that were suspected
`BG_WeaponRandomFireKick` 0x080f095a (chunk_044) and `G_FireWeaponByMode` 0x08120870: probes of ps blend, restrict time, angle in/out and the 6-float output are bit-identical (24 calls); the divergence was downstream, in the view-motion state that turns the kick into `fGunPitch`/`fGunYaw`.

## Not covered / limits
- [U] `WeaponView_ApplyBobB`-style helpers in chunk_045 at lines ~432/465 use the same `(double)((float)byte/255.0)*3.1415927` idiom (functions 0x080f21ba and a neighbour) as `WeaponView_ApplyYawBob` had before fix 3. They were not on any traced path of these cases and were not changed (no evidence); the likely same defect is listed in `done.json` as uncovered.
- [U] The mantle, sprint, turret and ADS-with-overlay-reticle (sniper) paths: the stock thompson/mp40 have no reticle overlay, so `adsOverlayReticle` branches of the idle sway are not executed.
- [U] A non-lethal melee hit and `meleeDamage` scaling by hit location: a bash on a player at full health always kills in dm; other gametypes with more health were not used.
- [U] Lean/ADS in other gametypes' menus and on other maps; leaning blocked by a prop or a moving entity (only static world geometry).
- Lean against a wall while ADS, crouched and prone is exercised (A blocked); the second client is not blocked within the window. The leanf range reported is 0.2026 to 0.5.
