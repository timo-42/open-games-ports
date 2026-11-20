# Gameplay lane gt_hq: Headquarters (`g_gametype hq`)

Tags: [O] observed, [I] inferred, [U] unknown. Plugin: `tools/oracle/scenarios/gt_hq.py`. Original: `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a7`), seed pinned (1234567).

## Cases

| Case | Exercises | Original runs: identical / variable items | Recon runs | Mismatches |
|---|---|---|---|---|
| `gt_hq_capture_mp_toujane` | Both clients join through the stock menus. The script waits for both teams, `restartRound`, 15 s radio delay, then the radio (eType 6 script_model) appears. A walks into it (`setviewpos`): 9.9 s capture, `MP_SETUP_HQ_ALLIED`, 1 point/s hold ticks, `score`. B walks in and destroys the HQ (axis +10 points, `MP_SHUTDOWN_ALLIED_HQ`, radioexplosion fx). Radio relocates (15 s later, a different entity/position). A and B contest it (no progress), A leaves, B captures (`MP_SETUP_HQ_AXIS`). | 163354 / 1 | 2 | 0 |
| `gt_hq_fight_mp_toujane` | A captures. B teleports next to the radio, A aims from its own snapshot and kills B (3 thompson hits, `D`/`K` log lines, corpse via `cloneplayer`, killcam). B holds use, respawns after the attacker respawn timer, destroys the HQ, radio relocates and is contested. | 167735 / 0 | 2 | 0 |

Regression: `duel_dm_toujane` (`--tag=-gt_hq`) against the recon: 0 mismatches of 84560 items.

## Determinism controls [O]
- Radio choice uses `randomint` in `hq_randomize_radioarray`/`hq_obj_think`; the pinned G_InitGame seed makes it fixed (radio index 2 at (1282.5, 731.8, -2.0), then index 3 at (2206.3, 411.6, 54.7)).
- Every decision is keyed to the first snapshot of the deciding client that contains a radio entity (eType 6 with an origin from the map script's `level.radio[]` list; the temp-entity fx at the same origin are ignored). The first radio appears at T0+20250 in all runs.
- Teleports use `setviewpos` (devmap, cheats). Clients are 100 units apart in the fight case (B at radio x-70, A at x+30) so the aim vector is defined.
- One variable item per capture-case run: `oob:rcon_status_after`, the `lastmsg` column of a zombie slot (1950/2000 ms). It is a wall-clock family the generic masks in `gameplay.py` only cover for ping rows; not filtered, not compared (variable on the original).

## Fix
- `G_UpdateCorpseSlot` 0x08107846 (chunk_048): the original's lifted retrace (when the first corpse trace is allsolid, start+64 down to currentOrigin) does `if (!tr.allsolid) { VectorLerp(start, currentOrigin, tr.fraction, out6c); Vec3Copy(out6c, currentOrigin); }` at 0x08107ccb..0x08107d08 before `G_CorpseApplyTrace`. The reconstruction omitted it, so the corpse stayed at the player's (under-floor) origin z -36 instead of settling at -1.875, and bullets hit different places. [O] Located by `calltrace.py` (chunk 048, window = first `G_RunCorpse`): identical call flow, `G_CorpseApplyTrace` arg 3 (`vel`) z = 0xbff00000 (original) vs 0xc2100000 (recon); disassembly showed the missing lerp. Not reachable by difftest (opaque entity/trace structs); parity evidence is the oracle.

## Not covered
- Kill of the defenders while the radio is held (WaitingOnNeutralize staydead path), score limit / time limit end in hq, more than 2 players, mp_carentan radio list, radio hold-time cap (`RadioMaxHoldSeconds` 120 s).
- `radios`/`sound` messages beyond what the clients receive as reliable commands.
