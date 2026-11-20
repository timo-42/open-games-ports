# Gameplay lane wpn_rockets: rocket launchers, scoped rifles in ADS, dropped item pickup, non-lethal melee

Tags: [O] observed, [I] inferred, [U] unknown. Method, harness and determinism controls are in `../gameplay-parity.md`. Plugin: `tools/oracle/scenarios/wpn_rockets.py`. Original: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`.

## Result

| Case | Original runs: identical / variable items | Recon runs | Mismatches |
|---|---|---|---|
| `wpn_rockets_rocket_dm_toujane` (dm, t0 20000, length 48000) | 3: 102924 / 0 | 2 | 0 |
| `wpn_rockets_snipe_tdm_carentan` (tdm, t0 20000, length 40000) | 3: 86370 / 0 | 2 | 0 |
| regression `duel_dm_toujane` (`--tag=-wpn_rockets`) | existing 3: 84560 / 0 | 1 | 0 |

Recon binary: `.local/cod2/lanes/wpn_rockets/build/cod2_lnxded_recon` (`link_test.py --no-gen --skip-before`). No masks, no filtered items.

## Why a GSC overlay is needed [O]
- [O] The stock MP scripts never allow the launchers: `maps/mp/gametypes/_weapons.gsc` has `//precacheItem("panzerfaust_mp"); //precacheItem("panzerschreck_mp");` commented out, `restrictWeaponByServerCvars` returns `"restricted"` in its `default:` case, and no weapon menu lists them (`weapon_british/american/german.menu`). Without precache the weapon is not in configstring 7 and cannot be given.
- The plugin's `mod` (`make_mod`) writes one loose file, `<fs_homepath>/mods/wpnrk/maps/mp/gametypes/_weapons.gsc`, derived at run time from `iw_07.iwd` (CRLF preserved): the two `precacheItem` lines enabled and a `case "panzerschreck_mp": case "panzerfaust_mp": break;` before the `fraggrenade` case. Its SHA-256 and the stock one are written to `harness.log`. It is never committed. Everything else is stock GSC and engine behaviour: team and weapon menu responses (`mr <sid> <menu> panzerschreck_mp`), spawn, damage, kill, `dropWeapon`/`dropOffhand`, pickup.
- [O] Weapon indices (configstring 7) are resolved from the gamestate by name, so they do not depend on the overlay's precache order.

## Case 1: `wpn_rockets_rocket_dm_toujane` (dm, mp_toujane, A panzerschreck / B kar98k_sniper)
A and B are moved by `setviewpos` to two spawn-point floors 522 units apart (A (858,742), B (1363,609); eye z = floor + 60, they settle at z about -2). Every decision uses server time and the deciding client's own snapshot.

| Rel. time | What |
|---|---|
| 6000 | A aims at B's chest (from its own snapshot of B's entity) and fires a rocket: direct hit, `K;...;panzerschreck_mp;1000;MOD_PROJECTILE;none`. Missile entity eType 4 (eFlags 0x400, `pos.trType` gravity-less linear, trDelta about 1930 u/s), then the explode event entity (eType 208), debris and B's corpse. |
| 7000..12000 | B is dead and drops kar98k_sniper_mp and 3 frags (eType 3 item entities; `dropWeapon`/`dropOffhand`). A walks toward the nearest item entity in its own snapshot with use pressed: `Weapon;...;frag_grenade_german_mp` (touch pickup of a dropped grenade, offhand index and `G_SendCharIndexC`). |
| 11000 | B holds use (killcam ended) and respawns; moved back to its mark once its own `pm_type == 0` plus 500 ms. A is moved back too. |
| 15000..17000 | B in ADS with the sniper rifle (`fWeaponPosFrac` 0 -> 1, `adsOverlayReticle` FG42 / `scope_overlay_german`, zoom, overlay sway). |
| 18000 | A fires a second rocket at the floor 285 units to the side of B: no direct hit, the splash hurts A itself (`D;...;panzerschreck_mp;69;MOD_PROJECTILE_SPLASH`), B is not hit. |
| 20500..24000 | A selects `webley_mp` and teleports 40 units in front of B (eye z = B's floor + 60). Bash at 21500: `D;...;webley_mp;54;MOD_MELEE;left_arm_lower`, **non-lethal melee hit** (pistol `meleeDamage` 50). |
| 24000..25000 | B in ADS, fires at A at 24600: `K;...;kar98k_sniper_mp;135;MOD_RIFLE_BULLET;torso_lower` (ADS shot: damage 90 x 1.5 torso_lower). |
| 28000 | A holds use (respawn), `score` at 40000. |

Findings [O]:
- [O] Melee damage depends on the weapon: `meleeDamage` is 150 for rifles and SMGs (a bash kills at full health, see `wpn_melee_ads_lean.md`), 50 for the pistols (`colt/luger/webley/tt30`), 40 for frag grenades, 50 for smoke grenades. A pistol bash is the non-lethal melee that needs no overlay and no health change; the hit location multiplier (here left_arm_lower: 54) scales it.
- [O] The first splash test hit its own thrower (69) rather than B because the lateral target point was too near geometry; a splash on a second victim is in case 2. Splash falloff measured: 179 at 229 units from the impact, 678 at about 20 units [O]; the in-game `explosionInnerDamage` 800 / `explosionOuterDamage` 10 / radius 256 do not give 179 at 229 with a linear falloff, so the real falloff is not linear in the radius [I].
- [O] The launcher fires without any random spread (`weaponType` 1 projectile goes through `G_LaunchWeaponMissile`, no `Rand`); the rifle shot uses `Bullet_ApplySpread` (angle 0 at full ADS).

## Case 2: `wpn_rockets_snipe_tdm_carentan` (tdm, mp_carentan, A springfield_mp / B panzerschreck_mp)
A (allies, American menu) and B (axis, German menu answered with `panzerschreck_mp`) are moved to the marks A walks to (about 100 units apart).

| Rel. time | What |
|---|---|
| 6000..7000 | A in ADS with the springfield (scope overlay `scope_overlay_american`), shot at 7000: `K;...;springfield_mp;135;MOD_RIFLE_BULLET;torso_lower`, B dies and drops the launcher and its frag. |
| 8000..11000 | A walks onto the dropped items with use pulsed (a fresh press is required; held-down use never fires the pickup, cursorHint 27 is shown while on the item). `Weapon;...;panzerschreck_mp` (pickup of a dropped launcher, which makes A drop the current primary: weapon swap through `Item_PickupWeapon` -> `Player_DropWeapon`), then `Weapon;...;springfield_mp` (A picks its own dropped rifle back up). |
| 10000..13000 | B holds use, respawns at a random spawn, is moved onto the marks and picks up its own old items: `Weapon;...;frag_grenade_german_mp`, `Weapon;...;panzerschreck_mp`. |
| 14000..15100 | B in ADS with the launcher (no overlay, zoom fov 80) fires at the floor next to A: splash, `D;...;panzerschreck_mp;84;MOD_PROJECTILE_SPLASH` (B itself) and `K;...;panzerschreck_mp;602;MOD_PROJECTILE_SPLASH` (A killed by the splash: two victims of one `G_RadiusDamage`, one of them the shooter). |
| 16500 | A's launcher shot comes after A died, so it is not fired (the log shows no further K). |

[O] Pickup facts: dropped items are eType 3 entities with `index` = weapon index (23 launcher, 14 frag in the log); the use press must be a new press; pickup of a weapon with full slots drops the current primary with a random throw (`Item_ThrowFromPlayer`, one `Rand_Signed`) and no angular spread.

## Determinism
- [O] Both cases are bit-identical across 3 original runs (102924 and 86370 items, 0 variable) with the default pinned seed and no masks.
- Controls: all decisions are functions of server time and the deciding client's own snapshot (the `aim_at`, `nearest_item` and respawn-detection helpers read only that client's own `ents()`/`psn()`); the respawn teleport waits for the client's own `pm_type == 0` plus 500 ms; pickup use presses are keyed to `t // 100`.
- [O] First attempts that were not deterministic by design were removed: the case 1 teleport originally used `z + 8` as the eye height, which put A below the floor (z -52) and gave a leg hit; it now uses `z + 60`.

## Fixes (original address, evidence)

Found with gdb probes on both servers under the pinned seed (`gameplay.py run --gdb-extra`): a break on `Bullet_ApplySpread`/`Bullet_Fire_Extended` (first difference: the shot direction of B's first ADS sniper shot), on `G_GetViewFrame` (client `fGunPitch/fGunYaw`: original 29.42 / 166.95, recon -109022648 / -102858328), on `WeaponView_Accumulate` (output and the vm words), and on `rand@plt` with a level-time key and backtrace.

1. `WeaponView_ApplyIdleSway` 0x080f1ede (chunk_045) [scoped ADS]. Slots +0x18 (frame seconds) and +0x1c (idle factor) of the view struct are floats (`fld/fstp [vm+0x18]`, `[vm+0x1c]` at 0x080f2039..0x080f20d2), mirrored from `gclient+0x287c` as raw dwords. The reconstruction used `(int32)` casts on them, so the factor was stored as 0x20d99999 and the sway output was about -1e8 for any weapon with `adsOverlayReticle != 0` (the ADS block of `ClientThink_real` 0x080f5d2e skips the rotate step for those weapons, so `fGunPitch/fGunYaw` became garbage and the shot went the wrong way). Also corrected from the disassembly: `hb * frameSeconds * 1000 * speed` in extended precision with `fistp` under `| 0xc00`, `amount * factor` and `* fWeaponPosFrac` and `* ps[0x5b4]` each rounded to float, `sin(fild phase * float const)` as a double with the float constants 0.0007 / 0.001 / 0.01 (`0x081558fc..0x08155904`), out += amount * sin * 0.01 once in extended. [O] Same defect class as fix 2 of `wpn_melee_ads_lean.md`.
2. `Player_DropWeapon` 0x081064de (chunk_048) [pickup]. `cmp [ebp+0x10],0; je 0x08106a7e` (0x08106890) skips the whole `withAim` block, including the three `Rand_Signed()` multiplications by 60/40/50 (0x081069fe..0x08106a45) that build the angular spread of the dropped item. The reconstruction ran the spread unconditionally, burning three `rand()` calls per pickup swap; the next consumer of the libc `rand()` stream (the player collision push `G_PlayerCollisionPush` 0x080f6e9e, which adds `Rand_Signed()` to the push direction) then pushed B in another direction (velocity (11.9,-22.0) vs (10.6,-22.6)). Evidence: `rand@plt` call counts per level time (162 original vs 168 recon, the extra six at the two swaps) with backtrace `Item_PickupWeapon -> Player_DropWeapon`.

## Not covered / limits
- [U] `panzerfaust_mp` (same weapon class and path as the schreck but a different `projectileSpeed` 2800 and no reload); it is precached by the overlay but not used.
- [U] Hold-breath on scoped rifles (`holdBreathTimer`, `holdBreathScale` < 1): no stock input bit was identified, the cases never press one; `ApplyIdleSway` is covered with scale 1.
- [U] The other overlay-gated view helpers (`WeaponView_ApplyBobB` 0x080f21ba converts `slot_14` with an int cast; `WeaponView_ApplyCrouchSway` 0x080f237e) were not executed on a diffed path and were not changed. [I] `ApplyBobB` has the same int-vs-float slot defect as fix 1 but is not reached from `WeaponView_ComputeOffset`.
- [U] Rocket impacts on non-player entities (script models, turrets, vehicles), rockets in other gametypes (sd/ctf/hq), missile entity lifetime (fuse timeout in the air), a launcher kill at long range (the missile flight is 522 units), a second launcher shot after the reload (`reloadTime` 4 s: the cases do not wait that long), more than two victims in one blast.
- [U] A melee kill by the pistol (`meleeDamage` 50) with a head hit, and `meleeDamage` scaling for other hit locations.
- The `rocket` case's second splash hit A and not B (see above); the splash falloff shape is only sampled at two distances.
