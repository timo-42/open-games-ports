# Gameplay lane wpn_grenade: frag and smoke offhand grenades

Plugin: `tools/oracle/scenarios/wpn_grenade.py`. Tags: [O] observed, [I] inferred, [U] unknown. Original: `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a3a7`).

## Result

| Case | Original runs: identical / variable items | Recon runs | Mismatches |
|---|---|---|---|
| `wpn_grenade_fragsmoke_dm_toujane` | 70027 / 0 | 2 | 0 |
| `wpn_grenade_kill_tdm_carentan` | 79972 / 0 | 2 | 0 |
| regression `duel_dm_toujane` (`--tag=-wpn_grenade`) | 84560 / 0 | 1 | 0 |

## Protocol facts

- [O] Offhand buttons in `usercmd.buttons`: `0x10000` = offhand class 1 (frag), `0x20000` = class 2 (smoke), read by `PM_Weapon_HandleOffhandButtons` 0x080efce2 (`BG_FindUsableWeaponOfClass(ps, hand)`). They fit the 18-bit `buttons>>1` wire field. `usercmd.offHandIndex` is honoured only if the weapon is owned. `giveGrenades()` (`_weapons.gsc`) gives 1/2/3 frags by primary weapon (thompson/mp40 1, m1garand 2, kar98k/enfield 3) and a smoke grenade only to SMGs/shotgun.
- [O] State machine seen in playerState: weaponstate 12 (offhand start), 13 (pin, weaponTime 600), 14 (hold, `grenadeTimeLeft` 3500), 15 (throw, weaponTime 300), 16. Smoke starts with `grenadeTimeLeft` 1000. The grenade entity (eType 4, `weapon` = grenade index, `pos.trType` 5 gravity, `apos.trType` 2) appears about 750 ms after the press for a tap and at release for a cook. A frag with no ammo left (A at 12000, second smoke at 13000) is denied: no state change, no entity.
- [O] Holding past the fuse (B at 14000..18000) throws at the end of the hold; the grenade explodes about 3.5 s after the throw.
- [O] Game log: `K;...;frag_grenade_american_mp;187;MOD_GRENADE_SPLASH;none` (kill by another player), the thrower's own grenade kills both players when it lands within about 140 units, killcam replays the grenade entity.

## Cases

1. `wpn_grenade_fragsmoke_dm_toujane`: A thompson (1 frag, 1 smoke) and B kar98k (3 frags). `setviewpos` puts them about 100 units apart facing each other. A: tap frag (overshoot, lob pitch -25), smoke, then denied frag and denied smoke. B: cooked 1.5 s frag, tap frag, hold-to-fuse frag (with bounces off a wall, rest, explosion kills both), no ammo, use to respawn.
2. `wpn_grenade_kill_tdm_carentan` (american/german, tdm): A m1garand (2 frags) throws flat at B (mp40, 1 frag, 1 smoke) and runs backwards; B throws smoke and frag; B dies to A's grenade (attribution, kill feed, killcam, `score`), A dies to B's grenade, respawn with use, second frag cooked 1.5 s and lobbed, denied press when empty.

## Determinism

[O] Both cases are bit-identical across 3 original runs with the default pinned seed. All decisions are functions of server time; the grenade code consumes `Rand_Fraction` (spin) which is pinned.

## Fixes (all in chunk_049/040/050, found by diffing grenade entity fields; the first divergence was always a bounce)

1. `BG_EvaluateTrajectory` 0x080df5d4 and `BG_EvaluateTrajectoryDelta` 0x080df948 (chunk_040): the original multiplies the int time delta by the float constant `0.001f` (0x8154d2c / 0x8154dbc) on the x87 stack and rounds once; the reconstruction used the double `0.001`. Gravity: `z - dt*400*dt` and `z - dt*800` evaluated in extended precision; case 7/8 `ratio*0.5*dt*dt` likewise. Effect: 1-ulp differences in the position of resting grenades.
2. `Turret_UpdateFromInput` 0x0810d1a8 and `Turret_UpdateTrajectoryAim` 0x0810cf4c (chunk_049; these are the grenade bounce/rest functions despite the name): the bounce time is `fild * float` followed by `fistp` with the control word forced to truncate; the reconstruction used `lrintf` (round to nearest), shifting the bounce velocity by one millisecond of gravity (0.25 units/s).
3. `Turret_UpdateTrajectoryAim` 0x0810cf4c: the spin jitter `0.85 + 0.3*Rand_Fraction()` uses the float constants 0x815ae18 / 0x815ae14, not doubles.
4. `Vec3_Dot_0810ecf2` (chunk_050): x, y, z summation order (the reconstruction summed z, y, x), same class as the earlier DotProduct fix. `Vec3_Length_0810ed26`: x,y,z sum in extended precision, rounded once when passed to the sqrt wrapper.
5. `Turret_ProcessInput` 0x0810d512 (chunk_049): the `0xbb` bounce event parameter and the final `ent->s.surfType` are re-read from `in->flags` after the `0x700000` default was stored for a client target (0x0810d752..0x0810d775, 0x0810d858); the reconstruction used the stale `seat` local (eventParm 0 instead of 7 when a grenade hits a player).

## Not covered

Grenade pickup of dropped grenades, grenade damage to entities other than players (script_model/turrets), `panzerfaust`/`panzerschreck` rockets, russian/british smoke difference, `scr_allow_*grenades 0`, throws while prone/crouching or on ladders, mantle, more than two players, packet loss during throws. [U] The recon is not compared in crowded explosions (several victims in one `G_RadiusDamage`).
