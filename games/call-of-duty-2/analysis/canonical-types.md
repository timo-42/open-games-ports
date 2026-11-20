# Canonical cross-chunk types (type-architect lane)

Target: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`
(ELF32 i386, GCC 3.3.6). Evidence is disassembly (`objdump -d -M intel` of
`.local/cod2/lnxded-1.3/cod2_lnxded`), the subsystem proposals in `analysis/subsystems/*.structs.json`,
and clang record layouts of every `recon/chunks/chunk_NNN_types.h`. No REA/Ghidra session was opened by
this lane; addresses quoted below are instruction addresses in the binary. Claim tags: **[O]** observed in
the disassembly / a rodata table / an allocation or memset size, **[I]** inferred (lineage or single-chunk
use), **[U]** unknown.

Outputs:

| File | Content |
| --- | --- |
| `recon/include/cod2_canon.h` | 62 canonical records (structs, unions, helper views); every field carries an evidence comment; 1,178 `COD2_ASSERT_SIZE`/`COD2_ASSERT_OFFSET` checks. Unrecovered bytes are `byte unk_0xOFF[N]`. |
| `recon/include/cod2_globals.h` | `extern` declarations for all 986 `symbols.json` globals, address/section/gap/declared size in a trailing comment. |
| `analysis/canonical-migration.json` | Machine-readable per-chunk migration (`replace_types`, `field_renames`, `delete_local_definitions`, `drop_extern_globals`, `notes`), tag maps, global containers and size violations. |

Both headers are generated (generator: `.local/cod2/canon-work/`, ignored; `canon_spec.py` is the
layout source, `gen_canon.py`, `gen_globals_h.py`, `migrate.py`). Edit the headers directly if the
generator is not available; keep the asserts.

Verification (passes, 2026-11-17):

```sh
printf '#include "cod2_types.h"\n#include "cod2_canon.h"\n#include "cod2_globals.h"\n' > t.cpp
g++ -m32 -std=gnu++98 -fsyntax-only -w -I games/call-of-duty-2/recon/include t.cpp      # all static asserts hold
clang++ -m32 -std=gnu++98 -fsyntax-only -w -I games/call-of-duty-2/recon/include t.cpp  # also passes
# adding #include "cod2_prototypes.h" after the three headers also compiles (0 errors)
```

Include order for chunks after migration: `cod2_types.h`, `cod2_canon.h`, `cod2_globals.h`,
`cod2_prototypes.h`, then `chunk_NNN_types.h`.

## 1. Why the link fails, and what fixes it

* C++ mangling encodes the struct **tag**: `void f(struct cvar_s *)` and `void f(struct dvar_s *)` are
  different symbols. Unifying tags (`replace_types`) and rewriting `symbols.json` signatures with the
  owning chunk's map removes most of the 1,199 signature mismatches; layouts matter only for correctness.
* Every chunk declared its own `extern` globals with local types; after migration they include
  `cod2_globals.h` instead (`drop_extern_globals`, 1,611 re-declarations across the chunks).
* Layout-level: a chunk that keeps a private copy of a canonical struct would compile but silently
  disagree; `delete_local_definitions` lists the 255 local records that must go.

## 2. Decisions per canonical struct

| Canonical | Size | Decisive evidence | Wrong or partial chunk layouts |
| --- | --- | --- | --- |
| `netadr_s` / `netadr_t` | 0x14 | `NET_AdrToString` reads type `[ebp+8]`, ip bytes `[ebp+0xc]`, port `movsx WORD [ebp+0x10]` (0x0806b207), ipx bytes +0xc..+0x12 [O]; passed by value as 5 dwords | 101 used the Q3 layout (port at +0x12): wrong. 20 (`ip` as uint32 + w3/w4) and 23 (`word1..4`) are untyped views. |
| `msg_s` | 0x18 | `MSG_Init`, bit cursor at +0x14 (chunks 8/22/38) [O] | 6/21 (0x10), 9 (0x14) truncated; 20 (0x20) and 23 (0x28) over-sized stack views. |
| `netchan_s` | 0x40040 | `Netchan_Setup` memset 0x40040 (0x0806b9d6) [O]; profile pointer last (= client+0xae700) | 22 (0x4003c, profile as a separate client field), 23 (0x2003c) truncated. |
| `netProfile_s` (+`netProfileStream_s`, `netProfilePacket_s`) | 0x5e0 | allocation 0x5e0 in NetProfile_Sync; 60 x 12-byte samples [O] | names only (9 vs 22). |
| `usercmd_s` | 0x1c | byte stores at +0x18/+0x19 (0x08096830/0x08096862); weapon/offhand bytes patched from ps+0xd4/+0xd0 [O] | 20 (`w0c[4]`), 46 (`int field_08`, `int field_18`) merged byte fields; 40 `UserCmd040` is `pml_s`. |
| `entityState_s` | 0xf0 | 59-entry netField table 0x081417a0 (names are rodata strings) [O]; `rep movs` 0xf0 (0x0809b973) | 39 (0xd0) truncated; 40 misnamed it `PlayerState`; game chunks named +0x74/+0x7c/+0x8c/+0x90 by local guesses. |
| `entityShared_s` (gentity+0xf0) | 0x68 | `SV_ArchiveSnapshot` copies svFlags byte +0xf2, clientMask +0xf4/+0xf8, tests broadcastTime +0x100, absmin/absmax +0x120/+0x12c (0x0809b989..0x0809ba18) [O] | 20 `inUse` (+0xf0) is `linked`; 21 calls absmin/absmax "currentOrigin/currentAngles"; 23 `field_f8` is an int mask, not float. |
| `archivedEntity_s` (+`archivedEntityShared_s`) | 0x114 | same function writes the archived record (+0xf0 svFlags|8, +0xf4/+0xf8 mask, +0xfc/+0x108 bounds) and indexes it with `imul 0x114` (0x0809b954) [O] | 22/23 called it `cachedEntity_s` (layouts agree). |
| `trace_s` / `trace_t` | 0x24 | `mov BYTE [eax+0x23],1` unconditionally (startsolid, 0x0805a15c) and `mov BYTE [eax+0x22],1` + fraction=0 only when never leaving the solid (allsolid, 0x0805a169); +0x10 = `materials[n]+0x40` (0x0805a1b8); +0x18 = `&materials[n]` (0x0805a1d4) [O] | 003/041/045/050/106 swapped allsolid/startsolid; 005/023 called +0x10 a plane distance (it is surfaceFlags); 042 (0x28), 047 (0x30), 043 (0x34), 053 (0x20), 105 (0x30) sizes are stack-frame guesses. |
| `hudelem_s` | 0x80 | 32-entry netField table 0x08142b80 [O] | 9 knew it as `recordFields32`. |
| `game_hudelem_s` | 0x8c | pool of 0x400 at 0x08628f80 ends exactly at the next global 0x0864bf80; owner/team/archived at +0x80/+0x84/+0x88 (HudElem_Alloc/Init) [O] | 47/48/104 called this `hudelem_s` (that tag is now the 0x80 network element). |
| `playerState_s` | 0x26a8 | 105-entry netField table 0x08142520 [O]; memcpy 0x26a8 [O] | 8/39..44 truncated prefixes; 8/41/44 `buttonBits`@0xa0 is `eFlags`; 41 `linkedEntityNum`@0xd4 is `weapon`; 49 `usingTurretIndex`@0x60 is `groundEntityNum`; 40 `pmFlags`@0x8 is an entityState view. |
| `clientState_s` | 0x5c | 22-entry table 0x08141f60 [O] | 22/23 knew only clientNum. |
| `pmove_s` | 0xf8 | memset 0xf8 in ClientThink_real (46); usercmd embedded at +4/+0x20 [O] | 43/44 flattened the usercmds; 40 (`PmoveCtx`) and 46 named +0xe5..+0xf7 differently (same bytes). |
| `pml_s` | 0x7c | memset 0x7c in PM_SingleStep; groundTrace 9 dwords at +0x38 (0x080e34c6) [O] | 40 `JumpMoveOut`/`UserCmd040` are partial pml views; 44 (0x2c) truncated. |
| `gentity_s` | 0x230 | 165 x `imul reg,0x230` (e.g. 0x080f4b3a), memset 0x8c000 = 1024 x 0x230 (0x0810935b) [O] | **23 (0x274) refuted**: no 0x274 stride exists. Server chunks 5/20/21/22 used truncated heads (0x138/0x150). Head = `entityState_s s` + `entityShared_t r`. |
| `gclient_s` | 0x28a4 | 41 immediates 0x28a4, memset 0xa2900 = 64 x 0x28a4 (0x0810938e) [O]; clientState embedded at +0x2748 (ClientDisconnect clears 0x5c bytes) | **51/102 named it `client_s`** (collides with the server client); 50 (0x2788), 104 (0x2744), 106 (0x27f0) truncated; 49 `sortState`@0x26c4 is `connected`. 0x2844/0x2848 are floats (`fstp`), 46 typed them int. |
| `level_locals_s` | 0x3624 | G_InitGame memset 0x3624 at 0x0864c380 (0x081090d8) [O]; members cross-checked with the 40 `symbols.json` globals inside the block | 46 asserted only 0x31c; 47/52 used `level` as the client-array pointer (it is `level.clients`). `sortedClients` is 64 ints (symbols.json said 1024). |
| `weaponDef_s` | 0x604 | `mov [esp],0x604; call Hunk_Alloc` (0x080f33a9) [O]; names from the 366-entry field table 0x08186a60 decoded by chunk 45 [O] | 47 (0x5fc) lower bound; 102/103 put maxAmmo at 0x1c0 (it is startAmmo; maxAmmo is 0x1d4); 43/44/49/52/53 partial views. Hit-location multipliers are an array (`[eax+edx*4+0x5b0]`, 0x08101b31). |
| `reliableCommand_s` | 0x408 | stride 0x408 (chunks 20/21/22) | names only. |
| `clientSnapshot_s` | 0x26c4 | `lea edx,[eax+0x234f0]` = client+0x20e34+0x26bc (0x08091038) [O] | 20 macro base 0x236f0 wrong. |
| `voicepacket_s` (packed) | 0x105 | 40 records between 0xae714 and 0xb0fdc [O] | 23 declared an unpacked 0x108 record, shifting every later client field by 0x78. |
| `client_s` / `client_t` | 0xb1064 | 83 immediates (stride, memset in SV_DirectConnect) [O] | 23 (0xb10dc) see voicepacket; 23 `reliableAcknowledge`@0x20840 is `lastClientCommand` (0x0809ae4c); 20 `pendingDrop` int is a `const char *`; 19 `name[64]`/22 `name[0x1d0]` overlap `downloadName`. |
| `challenge_s`, `tempban_s`, `archivedSnapshot_s`, `cachedSnapshot_s`, `cachedClient_s` | 0x74, 8, 8, 0x1c, 0x2708 | chunk 21 allocation sizes, `imul 0x2708` (0x08099480) [O] | names only. |
| `svEntity_s` | 0x174 | 30 x 0x174; baseline at +4 (`add eax,0x844261c` after `imul 0x174`, 0x0809ba3c) [O] | 21 `archivedEntity_s` starts 4 bytes late (baseline view); 23 typed +0 as int (two uint16 links); 05 base 0x084424a4 is `&svEntities[-1]` (1-based). |
| `serverStatic_s` | 0x1d10c | memset 0x1d10c (0x08094374) [O]; members anchored by the globals at 0x08423084..0x0844010c | 21 (0x54) prefix only. |
| `server_s` | 0x5f534 | clear size 0x5f534 (0x080932f4) [O]; tail fields = globals 0x0849f618..0x0849f6f4 (gentities, gameClients, skel counters, gametype[64] ending exactly at 0x5f534) | 21/22 called sv.timeResidual "svs". |
| `dvar_s` (+`DvarValue`, `DvarLimits`) | 0x24 | pool 0x085abe20 + 0x500 x 0x24 = hash table 0x085b7220 [O]; Dvar_SetLatched +0xc, ResetToDefault +0x10 | 6/9/19/20/22/23/40/42/43/47/49/50/51/103/107 views of 0xc bytes; 9 put `string` at +4 (Q3 cvar layout: wrong, +4 is flags). |
| `cmd_function_s` | 0x14 | Com_MallocZeroed(0x14) | none. |
| `fileHandleData_s`, `searchpath_s`, `iwd_s`, `directory_s` | 0x11c, 0x14, 0x320, 0x200 | FS_HandleForFile `cmp [slot+0x849fd80]` (0x0809e7d4); `cmp [eax+0xc]` localized flag (0x08065935) [O] | 25 `cod2_fsh_s` based 8 bytes late (fsh_table = fsh+8); 7 `searchpath_s.dir`@0xc is `bLocalized`. |
| `VariableValue`, `VariableUnion` | 8, 4 | OP handlers write value then type (16/17) [O] | names only (14/15/17/101). |
| `ScrVarEntry` | 0x10 | pool 0x08297500, 0x10000 x 0x10 [O] | kept the subsystem's hash-slot model (field names differ from IW `VariableValueInternal`). |
| `ScrVmFrame`, `scrVmPub_t` | 0x18, 0x320 | 0x20 + 32 x 0x18; globals 0x083d8a84..0x083d8aa0 [O] | 101 (0x20) header only; 13/17 kept members as separate globals. |
| `scrVarPub_t` | 0x50 | 22 globals 0x08397500..0x0839754c; +0x08 is `movzx word` (0x0807c428) [O] | 14 typed 0x08397508 as uint32. |
| `scrVmGlob_t` | 0x28 | globals 0x083dce80..0x083dce9c [I, low] | proposal only. |
| `ScrClassInfo`, `ScrStackBuffer_s`/`ScrStackRecord_s` (packed) | 0xc, 0xb, 5 | table 0x0817d920; packed record reads [O] | 16 `classnum`@2 is the entity-array id. clang ignores the chunks' packing pragmas (15/17 asserts fail under clang only). |

Opaque (forward-declared only, owned by their subsystem): `XModel`, `DObj_s`, `XAnimTree_s`, `turret_s`,
`game_attach_node_s`, `unz_s`, `fileInIwd_s`, `snd_alias_list_t`, `clientInfo_s`, `huff_s`, `FxPool`,
`dMaterial_s`, `accuracy_knot_s`.

## 3. Unresolved conflicts

1. `gentity_s` +0x160..+0x167: one byte each, 3-4 competing names per byte (occupied/isPlayer/
   physicsObject, touchPending/useState/turretPending). Canonical names are placeholders [U].
2. `gentity_s` per-class region 0x1a8..0x207: `gentity_u` union with `item`, `mover`, `trigger`,
   `turretRateMs` and `raw[0x60]` views, each from a single chunk; 045 `box_a/box_b` (+0x1ec/+0x1f8),
   052 `limit1a8/count1ac`, 053 `ownerNum`@0x1b4 have no view yet (they land in `u.raw`).
3. `gclient_s` 0x27e0..0x27ff (103 damage feedback vs 045 recoil samples) and 0x284c..0x28a3 (046 view
   spring vectors; 046 `field_2898 int[3]` overlaps 045 `last_activity_time`@0x289c and 053
   `nextUseTime`@0x28a0, which the binary accesses as plain dwords). Neutral names `viewVec*`.
4. `playerState_s`: `hudCurrent`@0x7a8 / `hudArchival`@0x1728 assignment follows game.md (archived flag
   +0x88 selects 0x1728) [I]; non-networked 0xc8, 0x5a0, 0x5cc names are provisional.
5. `level_locals_s` 0x0b2c..0x1347 and `server_s` 0x14..0x417 unrecovered; `cs12Bound` meaning unknown.
6. `DvarLimits` is a union: chunks that named +0x14/+0x18 `domain_min/max` must pick
   `domain.integer`, `domain.decimal` or `domain.enumeration` per dvar type; the automatic rename picks
   `enumeration`.
7. `clientInfo_s` (0x4b8) role of +0x380..+0x3e8 (046 vs 049 `turret_user_s`) - left opaque.
8. Subsystem tag unification without layout agreement (tags only; layouts stay in the owning chunks):
   `XModel` (031: 0x8c vs 034/035: 0x90), `FxSystem` (054 0x84 vs 058 0x20), `ParseNode` /
   `ConfigBlock` / `fx_parse_node_s` (0x14/0x30/0x48), `fx_primitive_s` family (0x220..0x2a4),
   `ScriptNode` family (variable length), `xanim_track_s` family. The subsystem owners must reconcile
   these layouts.
9. `scrVmGlob_t` is low confidence (0x00..0x07 and 0x10..0x17 never touched).
10. `g_0818f016` lies inside the last element of `g_0818901c` (`cod2_area_node_s[1024]`, +0x5ffa): either
    the count is 1023+ or it is a field of node 1023 [U].

## 4. Globals (`cod2_globals.h`)

* 986 declarations, names exactly as in `symbols.json` (the link contract with the global-image lane);
  623 primitive, 210 canonical-typed, 133 subsystem-owned incomplete tags (`struct tag` arrays/objects
  are valid C++ declarations), 11 function pointers, 5 `cod2_types.h` types, 4 system types as byte
  arrays (`jmp_buf` 0x9c, `pthread_mutex_t` 0x18, `pthread_t` 4, `termios` 0x3c, to keep system headers
  out of the shared header).
* Type corrections against `symbols.json`: `g_08188dd0` is `struct dMaterial_s *` (cm.materials,
  0x48-byte records read at +0x40), `g_08397508` `uint16`, `g_0864c59c` `int[64]`
  (level.sortedClients), `g_0864c3a4` `objective_t[16]`; every `cvar_s *`/`cod2_dvar_s *`/... is
  `dvar_t *`.
* `__gmon_start__` is both a function and a global in `symbols.json`; it is the `.got` slot of the weak
  crt function and is declared as the function. **The global-image lane must not define it**; likewise
  `g_0817d00c` is the `.ctors` list.
* Gap check: 864 declarations have a known size, 121 are open arrays (`[]`), 1 unchecked (`.ctors`).
  18 declarations exceed the gap to the next global; none is a plain overrun:

| Class | Symbols | Meaning |
| --- | --- | --- |
| container (5) | `svs`, `scrVmPub`, `g_08188dc0_clipmap`, `g_081e9c60` (huff_s = 2 x huffman 0x701c), `yylval` | struct whose members also exist as globals (interior aliases). |
| array-alias (11) | `_dist_code` (+0x100 alias), `g_0818901c`, `scrAnimTreeObjects` ([1] row alias), `scrAnimTreeCount`, `scrFuncTable` ([9] alias), `g_08287080`, `g_083deec0`, `g_0864f9c0`, `g_clientInfos` (field-base alias +0x4a4), `g_08716400` (g_entities, alias = entity 1022), `g_0884e7b8` | array with a named element/field inside it. |
| phantom-alias (2) | `g_08440618` (contains `g_084424a4` = `&sv.svEntities[-1]`), `g_08703a60` (field-base view `&g_clientInfos[0].poseHandle`, stride 0x4b8, runs into g_entities) | the inner/outer symbol is not a real object; recommend removing it from symbols.json and indexing the owner. |

  Overlapping symbols are harmless for assembler aliases at fixed addresses. The JSON
  (`_globals.containers`) lists every interior alias with its member path, e.g.
  `g_svs_time -> svs.time`, `g_08440210 -> sv.timeResidual`, `g_0864c56c -> level.time`,
  `scr_call_depth -> scrVmPub.function_count`; proposed new container names are `sv` (0x08440200),
  `level` (0x0864c380), `scrVarPub` (0x08397500), `scrVmGlob` (0x083dce78), `g_entities` (0x08716400),
  `g_clients` (0x087a2500). Renaming is left to the merge lane because it changes the shared symbol set.

## 5. Migration procedure for chunk lanes

For chunk NNN, read `canonical-migration.json["NNN"]`:

1. Delete every record in `delete_local_definitions` from `chunk_NNN_types.h` (with its asserts and
   typedefs); delete every `extern` in `drop_extern_globals`.
2. Apply `replace_types` to the header and the `.cpp` (both `struct tag` and typedef spellings). For
   targets in `cod2_canon.h` use the canonical typedef (`gentity_t`, `dvar_t`, ...); for subsystem
   targets rename the local definition itself.
3. Apply `field_renames` (`"LocalType.old": "new.path"`): member accesses on the old type become
   `->new.path`. Paths descend into embedded records (`ent->s.eType`, `ent->r.currentOrigin`,
   `cl->cs.team`, `client->netchan.qport`) or index arrays (`ps->stats[0]`). Renames of the form
   `x[N]` on a byte array (for example `netadr_s.w3 -> ipx[2]`) mean the old field started at byte N
   of a canonical array: rewrite the access instead of keeping a pointer cast.
4. Work through `notes`: truncated local views (size mismatch) are fine once deleted; "falls in
   canonical padding" means the canon lacks a field the chunk uses (2 cases: chunk 015 frame-header
   records at +0xc, a clang artefact of its packing pragma, and chunk 024 `referenced_pad`); "byte blob"
   / "does not start on a canonical field" (20 cases) mark untyped views that must be split; "type ... check
   every access" (99 cases) marks local int/float/pointer guesses that disagree with the canonical
   member type.
5. Put the shared includes at the top of `chunk_NNN.cpp` in the order given above and re-run the chunk
   compile command from `WORKER_BRIEF.md`.
6. The merge lane rewrites `symbols.json` signatures per owning chunk with the same `replace_types`
   map and regenerates `cod2_prototypes.h`.

## 6. Riskiest decisions

1. **gentity_s = entityState_s s + entityShared_t r + game part.** Layout is fully observed, but every
   game/server chunk access in 0x000..0x157 becomes `s.`/`r.` (most of the 1,489 renames).
2. **gentity_u per-class union (0x1a8..0x207)** built from single-chunk views (items 048, movers 051,
   triggers 107, turrets 049); other classes still use raw bytes.
3. **Subsystem tag choices by signature frequency** (`fx_primitive_s`, `ScriptNode`, `ScriptScope`,
   `ParseNode`, `xanim_track_s`, `XAnimSlot`, `entity_handler_s`, `gitem_s`, `clientInfo_s`) where the
   chunk layouts disagree in size; only the tag is unified.
4. **Inferred names in observed slots:** `hudCurrent`/`hudArchival`, ps 0xc8/0x5a0/0x5cc, gclient
   0x26b0/0x27d8/0x2810, level `teamScores`/`cs12Bound`, scrVarPub `canonicalStrCount`/`pauseArrayId`,
   the whole `scrVmGlob_t`.
5. **Globals kept as overlapping aliases plus four type overrides** (`g_08188dd0`, `g_08397508`,
   `g_0864c59c`, `g_0864c3a4`) and system types as byte arrays (`g_script_error` needs a cast for
   `setjmp`). The global-image lane must accept overlapping symbols and skip `__gmon_start__`.

## 7. Per-chunk migration list

Columns: replace_types / field_renames / delete_local_definitions / drop_extern_globals / notes counts.
Totals: 221 tag replacements, 1,489 field renames, 255 local definitions to delete, 1,611 extern
re-declarations to drop. The JSON is authoritative; the lists below repeat it for review.


| chunk | replace | renames | delete | drop externs | notes |
| --- | --- | --- | --- | --- | --- |
| 000 | 0 | 0 | 0 | 0 | 0 |
| 001 | 0 | 0 | 0 | 4 | 0 |
| 002 | 1 | 0 | 0 | 13 | 0 |
| 003 | 0 | 2 | 1 | 4 | 1 |
| 004 | 13 | 5 | 2 | 24 | 1 |
| 005 | 13 | 7 | 4 | 15 | 2 |
| 006 | 2 | 6 | 5 | 36 | 2 |
| 007 | 2 | 10 | 6 | 45 | 4 |
| 008 | 0 | 11 | 4 | 16 | 3 |
| 009 | 4 | 17 | 8 | 34 | 3 |
| 010 | 6 | 0 | 1 | 29 | 0 |
| 011 | 2 | 2 | 1 | 24 | 0 |
| 012 | 6 | 0 | 0 | 63 | 0 |
| 013 | 3 | 2 | 1 | 31 | 0 |
| 014 | 2 | 8 | 3 | 11 | 1 |
| 015 | 4 | 12 | 6 | 17 | 5 |
| 016 | 1 | 4 | 5 | 21 | 1 |
| 017 | 5 | 16 | 5 | 30 | 8 |
| 018 | 0 | 0 | 0 | 17 | 0 |
| 019 | 0 | 5 | 2 | 39 | 1 |
| 020 | 3 | 62 | 10 | 41 | 14 |
| 021 | 4 | 31 | 9 | 59 | 8 |
| 022 | 3 | 37 | 19 | 31 | 6 |
| 023 | 11 | 55 | 20 | 26 | 22 |
| 024 | 6 | 11 | 5 | 16 | 2 |
| 025 | 4 | 5 | 4 | 13 | 3 |
| 026 | 0 | 0 | 0 | 2 | 0 |
| 027 | 1 | 1 | 1 | 24 | 1 |
| 028 | 6 | 4 | 2 | 16 | 1 |
| 029 | 2 | 5 | 1 | 9 | 1 |
| 030 | 0 | 0 | 0 | 13 | 0 |
| 031 | 1 | 0 | 0 | 3 | 0 |
| 032 | 6 | 0 | 0 | 7 | 0 |
| 033 | 2 | 0 | 0 | 3 | 0 |
| 034 | 2 | 0 | 0 | 8 | 0 |
| 035 | 4 | 0 | 1 | 8 | 0 |
| 036 | 0 | 0 | 0 | 13 | 0 |
| 037 | 0 | 0 | 0 | 15 | 0 |
| 038 | 0 | 0 | 2 | 33 | 0 |
| 039 | 2 | 5 | 5 | 23 | 5 |
| 040 | 11 | 52 | 10 | 60 | 12 |
| 041 | 0 | 46 | 6 | 20 | 6 |
| 042 | 2 | 33 | 7 | 18 | 6 |
| 043 | 8 | 57 | 7 | 16 | 14 |
| 044 | 2 | 108 | 5 | 6 | 5 |
| 045 | 3 | 64 | 5 | 26 | 8 |
| 046 | 0 | 138 | 8 | 17 | 16 |
| 047 | 2 | 71 | 6 | 18 | 13 |
| 048 | 5 | 49 | 6 | 47 | 2 |
| 049 | 3 | 77 | 5 | 55 | 8 |
| 050 | 4 | 25 | 6 | 26 | 5 |
| 051 | 3 | 33 | 4 | 20 | 3 |
| 052 | 6 | 69 | 5 | 36 | 3 |
| 053 | 3 | 63 | 5 | 108 | 5 |
| 054 | 2 | 0 | 0 | 16 | 0 |
| 055 | 3 | 0 | 0 | 14 | 0 |
| 056 | 8 | 0 | 0 | 6 | 0 |
| 057 | 2 | 0 | 0 | 8 | 0 |
| 058 | 4 | 0 | 0 | 16 | 0 |
| 059 | 4 | 0 | 0 | 22 | 0 |
| 060 | 0 | 0 | 0 | 8 | 0 |
| 061 | 0 | 0 | 0 | 4 | 0 |
| 062 | 0 | 0 | 0 | 0 | 0 |
| 100 | 2 | 6 | 5 | 18 | 4 |
| 101 | 2 | 17 | 9 | 24 | 9 |
| 102 | 1 | 28 | 4 | 42 | 5 |
| 103 | 4 | 62 | 4 | 11 | 3 |
| 104 | 4 | 87 | 5 | 26 | 5 |
| 105 | 5 | 24 | 4 | 19 | 1 |
| 106 | 4 | 38 | 4 | 36 | 3 |
| 107 | 3 | 19 | 2 | 32 | 3 |

### chunk 001

* drop externs (4): g_0817d3a8, g_08188c68, g_08188c80, g_08188c84

### chunk 002

* replace: `cm_map_s` -> `clipMap_s`
* drop externs (13): g_0817d3c0_directives, g_0817d460_dollardirectives, g_0817d500_default_punctuations, g_08188ca0_sourceFiles, g_08188da8_bspData, g_08188dac_bspLength, g_08188db0_bspChecksum, g_08188dc0_clipmap, g_08188de8, g_08188e34, g_08188e58, g_085b7660_slots, g_08a6f440

### chunk 003

* delete: `trace_s`
* rename `trace_s`: allsolid -> startsolid, startsolid -> allsolid
* drop externs (4): cm_numPlanes, cm_planes, cmod_base, cmod_lumpBuffer
* note: trace_s: byte 0x22 is allsolid and 0x23 startsolid; check local names.

### chunk 004

* delete: `cm_hit_s`, `cm_plane_s`
* replace: `cm_brush_s` -> `cbrush_s`, `cm_brush_side_s` -> `cbrushside_s`, `cm_edge_s` -> `CollisionEdge_s`, `cm_hit_s` -> `trace_s`, `cm_leaf_s` -> `cLeaf_s`, `cm_model_s` -> `cmodel_s`, `cm_node_s` -> `cbrushnode_s`, `cm_plane_node_s` -> `cnode_s`, `cm_plane_s` -> `cplane_s`, `cm_surface_s` -> `CollisionAabbTree_s`, `cm_trace_s` -> `traceWork_s`, `cm_wall_s` -> `CollisionBorder_s`, `cm_wedge_s` -> `CollisionTriangle_s`
* rename `cm_hit_s`: all_solid -> allsolid, flags -> entityNum, start_solid -> startsolid, surface -> material, surface_value -> contents
* drop externs (24): g_08140020, g_08140120, g_08140140, g_08140180, g_081401c0, g_08188dc4, g_08188dc8, g_08188dd0, g_08188de0, g_08188de8, g_08188df0, g_08188e08, g_08188e10, g_08188e30, g_08188e34, g_08188e38, g_08188e40, g_08188e44, g_08188e48, g_08188e4c, g_08188e50, g_08188e58, g_08188eec, g_08188ef0
* note: cm_hit_s.surface_value (+0x14) local type float vs canonical contents int: check every access

### chunk 005

* delete: `cod2_area_link_s`, `cod2_gentity_s`, `cod2_plane_s`, `cod2_trace_result_s`
* replace: `cod2_area_box_s` -> `cm_static_model_s`, `cod2_area_link_s` -> `svEntity_s`, `cod2_brush_s` -> `cbrush_s`, `cod2_brush_side_s` -> `cbrushside_s`, `cod2_bsp_node_s` -> `cnode_s`, `cod2_gentity_s` -> `gentity_s`, `cod2_kdnode_s` -> `cbrushnode_s`, `cod2_leaf_item_s` -> `CollisionAabbTree_s`, `cod2_leaf_s` -> `cLeaf_s`, `cod2_plane_s` -> `cplane_s`, `cod2_query_s` -> `SegQueryB`, `cod2_trace_result_s` -> `trace_s`, `cod2_trace_s` -> `traceWork_s`
* rename `cod2_area_link_s`: next -> nextEntityInWorldSector, node -> worldSector
* rename `cod2_gentity_s`: absmax -> r.absmax, absmin -> r.absmin, contents -> r.contents
* rename `cod2_trace_result_s`: flag_22 -> allsolid, plane_dist -> surfaceFlags
* drop externs (15): g_08188dc4, g_08188dc8, g_08188de0, g_08188de8, g_08188df0, g_08188e30, g_08188e3c, g_08188e40, g_08189000, g_0818900c, g_08189018, g_0818901a, g_0818901c, g_0818f016, g_084424a4
* note: cod2_gentity_s: local size 0x138 vs canonical gentity_s 0x230
* note: cod2_trace_result_s.plane_dist (+0x10) local type float vs canonical surfaceFlags int: check every access

### chunk 006

* delete: `SpatialEntry`, `cmd_function_s`, `dvar_s`, `msg_s`, `netadr_s`
* replace: `SpatialEntry` -> `svEntity_s`, `SpatialNode` -> `cod2_area_node_s`
* rename `SpatialEntry`: contentsMask -> linkcontents, next -> nextEntityInWorldSector
* rename `cmd_function_s`: aux8 -> autoCompleteDir, auxC -> autoCompleteExt
* rename `dvar_s`: value -> current
* rename `netadr_s`: words -> type
* drop externs (36): cmd_argc, cmd_args, cmd_argv, cmd_functions, cmd_tokenized, com_consoleLines, com_developer, com_errorCode, com_errorCount, com_errorEntered, com_errorMessage, com_knownErrorStrings, com_lastErrorTime, com_logfile, com_logfileHandle, com_numConsoleLines, com_openingLogfile, com_pushEventOverflowed, com_pushedEvents, com_pushedEventsHead, com_pushedEventsTail, com_safemode, com_sv_running, g_0818901c, g_081a21c4, g_081a21c8, g_081a21cc, g_081a2270, g_084424a4, g_cmd_text, g_cmd_text_buffers, g_cmd_text_storage, g_cmd_wait, rd_buffer, rd_buffersize, rd_flush
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: msg_s: local size 0x10 vs canonical msg_s 0x18

### chunk 007

* delete: `comStringBuffer_s`, `dvarValue_u`, `dvar_s`, `fileHandleData_s`, `iwd_s`, `searchpath_s`
* replace: `comStringBuffer_s` -> `field_s`, `dvarValue_u` -> `DvarValue`
* rename `comStringBuffer_s`: capacity -> widthInChars, data -> buffer
* rename `dvar_s`: max -> domain.enumeration.strings, min -> domain.enumeration.stringCount
* rename `fileHandleData_s`: file -> handleFiles
* rename `iwd_s`: basename -> iwdBasename, filename -> iwdFilename, gamename -> iwdGamename, pureChecksum -> pure_checksum
* rename `searchpath_s`: dir -> bLocalized
* drop externs (45): com_animCheck, com_cl_paused, com_dedicated, com_developer, com_developer_script, com_errorEntered, com_errorMessage, com_fixedtime, com_frameCount, com_frameTime, com_frameTimeScale, com_fullyInitialized, com_introPlayed, com_lastFrameTime, com_logfile, com_logfileHandle, com_maxfps, com_shortversion, com_sv_paused, com_sv_running, com_timescale, com_version, com_viewlog, dvar_indexCount, dvar_modifiedFlags, dvar_vars, fs_basepath, fs_cdpath, fs_checksumFeed, fs_debug, fs_defaultConfigName, fs_homepath, fs_numServerIwds, fs_restrict, fs_searchpaths, fs_serverIwdNames, fs_serverIwds, fsh, g_0817d838, g_081a21c4, g_081a2270, g_081a5080, g_081d7900, g_081d8200, g_0849fd70
* note: dvar_s: local size 0x20 vs canonical dvar_s 0x24
* note: iwd_s: local size 0x314 vs canonical iwd_s 0x320
* note: searchpath_s: +0x0c is bLocalized (cmp [eax+0xc] at 0x08065935), not dir; the dir pointer is +0x08.
* note: searchpath_s: local size 0x10 vs canonical searchpath_s 0x14

### chunk 008

* delete: `msg_s`, `netField_s`, `playerState_s`, `usercmd_s`
* rename `playerState_s`: buttonBits -> eFlags, deltaAngles -> delta_angles, fieldDc -> fWeaponPosFrac, field_d0 -> offHandIndex, field_d4 -> weapon, moveAxis -> leanf, pmFlags -> pm_flags, pmType -> pm_type, viewAngles -> viewangles
* rename `usercmd_s`: forwardMove -> forwardmove, rightMove -> rightmove
* drop externs (16): g_081413e2, g_081417a0, g_08141ae0, g_08141f60, g_0817d840, g_0817d880, g_081e9500, g_081e9544, g_081e9c60, g_081f7c98, g_081f7ca0, g_081f80a0, g_081fa0a0, g_084a64a0, g_084a64c0, g_084a74c0
* note: playerState_s: local size 0xf4 vs canonical playerState_s 0x26a8
* note: playerState_s.field_d0 (+0xd0) local type uint8 vs canonical offHandIndex int: check every access
* note: playerState_s.field_d4 (+0xd4) local type uint8 vs canonical weapon int: check every access

### chunk 009

* delete: `cvar_s`, `msg_s`, `netField_s`, `netadr_s`, `netchan_s`, `netprof_half_s`, `netprof_s`, `netprof_sample_s`
* replace: `cvar_s` -> `dvar_s`, `netprof_half_s` -> `netProfileStream_s`, `netprof_s` -> `netProfile_s`, `netprof_sample_s` -> `netProfilePacket_s`
* rename `cvar_s`: integer -> current, string -> flags
* rename `netchan_s`: profile -> prof
* rename `netprof_half_s`: bytesPerSecond -> iBytesPerSecond, lastUpdateTime -> iLastBPSCalcTime, maxPacketSize -> iLargestPacket, minPacketSize -> iSmallestPacket, numFragmented -> iCountedFragments, numPackets -> iCountedPackets, percentFragmented -> iFragmentPercentage, sampleIndex -> iCurrPacket, samples -> packets
* rename `netprof_s`: incoming -> recieve, outgoing -> send
* rename `netprof_sample_s`: bytes -> iSize, fragmented -> bFragment, time -> iTime
* drop externs (34): anim_property_words, com_sv_running, cvar_net_lanauthorize, cvar_net_profile, cvar_net_showprofile, cvar_packetDebug, cvar_showdrop, cvar_showpackets, g_081417a0, g_08141ae0, g_08141f60, g_0817d838, g_081e9c60, g_081f0c7c, g_081f7c98, g_082055a0, g_08205600, g_08205638, g_08205648, g_0820584c, g_08205a50, g_08205a60, g_08205a64, g_08205c68, g_0839753c, huff_symbol_counts, net_loopbacks, net_profile_mode, net_profile_names, net_qport, net_sock_names, originFields, playerStateFields, recordFields32
* note: cvar_s: local size 0xc vs canonical dvar_s 0x24
* note: cvar_s.string (+0x4) local type const char * vs canonical flags uint16: check every access
* note: msg_s: local size 0x14 vs canonical msg_s 0x18

### chunk 010

* delete: `ScrVariableValue`
* replace: `ScrDeferredValue` -> `ScriptValue`, `ScrLocalBlock` -> `ScriptScope`, `ScrNode` -> `ScriptNode`, `ScrNodeList` -> `ScriptList`, `ScrNodeListLink` -> `ScriptListItem`, `ScrVariableValue` -> `VariableValue`
* drop externs (29): g_08205c84, g_08205c88, g_08205c8c, g_08206cc8, g_0828733c, scrAnimTreeCount, scrAnimTreeNames, scrAnimTreeObjects, scrAnimTreeRoot, scrAnimTreeScratchObject, scrCompileDebuggerMode, scrCompileKeepStringRefs, scrCompileMaxCallStackDepth, scrCompileMaxStackDepth, scrCompileMode, scrCompileStackDepth, scrCompileUninitIsError, scrFuncRecordPtr, scrFuncTable, scrFuncTableCount, scrLastEmitPtr, scrLastOpcodePtr, scrOpcodeAtStackBase, scrPrevOpcodePtr, scrUsingAnimTreeIndex, scrValueStack, scrValueStackCount, scr_debug_enabled, scr_last_source_text

### chunk 011

* delete: `ScriptTableValue`
* replace: `ScriptFileEntry` -> `scr_frame_s`, `ScriptTableValue` -> `VariableValue`
* rename `ScriptTableValue`: tag -> type, value -> u
* drop externs (24): g_08205c90, g_08205c94, g_08206cc8, g_08206cec, g_08206ced, g_08206cf0, g_08206cf4, g_08206cf5, g_08206cf8, g_08206cfc, g_08206d00, g_08206d04, g_08206d08, g_08287334, g_0839750b, g_08397510, g_0839753c, g_scrTypeNames, scrCompileDebuggerMode, scrCompileKeepStringRefs, scrCompileMode, scrLastEmitPtr, scrValueStack, scrValueStackCount

### chunk 012

* replace: `cod2_fn_ctx` -> `ScriptScope`, `cod2_include_rec` -> `ScrFuncRecord`, `cod2_loop_jump` -> `ScriptPatchEntry`, `cod2_scr_list` -> `ScriptListItem`, `cod2_scr_node` -> `ScriptNode`, `cod2_scr_pair` -> `ScriptNodeRef`
* drop externs (63): g_0814445f, g_08205648, g_08205a58, g_08205c84, g_08205c88, g_08205c8c, g_08205c90, g_08205c94, g_08205c98, g_08205c9c, g_08205ca0, g_08205cb0, g_08205cdc, g_08206cc8, g_08206ccc, g_08206cdd, g_08206ce0, g_08206ce4, g_08206ce6, g_08206ce8, g_08206cec, g_08206ced, g_08206cf0, g_08206cf4, g_08206cf5, g_08206cf8, g_08206cfc, g_08206d00, g_08206d04, g_08206d08, g_08206d18, g_08206f80, g_08286f80, g_08287080, g_08287180, g_08287280, g_082872a4, g_082872a8, g_0828733c, g_08397500, g_08397504, g_08397508, g_0839750b, g_0839753c, g_08397548, g_0839754c, scrAnimTreeCount, scrAnimTreeObjects, scrAnimTreeRoot, scrAnimTreeScratchObject, scrCompileDebuggerMode, scrCompileKeepStringRefs, scrCompileMaxCallStackDepth, scrCompileMaxStackDepth, scrCompileMode, scrCompileStackDepth, scrCompileUninitIsError, scrFuncRecordPtr, scrFuncTableCount, scrLastEmitPtr, scrUsingAnimTreeIndex, scrValueStackCount, scr_last_source_text

### chunk 013

* delete: `scr_call_entry_s`
* replace: `scr_call_entry_s` -> `ScrVmFrame`, `scr_list_node_s` -> `ScriptListItem`, `scr_list_s` -> `ScriptList`
* rename `scr_call_entry_s`: flags -> localId, pc -> pos
* drop externs (31): g_0828717f, g_08287180, scrCompileMode, scrLastOpcodePtr, scrOpcodeAtStackBase, scr_call_depth, scr_call_stack, scr_compile_error_deferred, scr_cur_file_line_count, scr_debug_enabled, scr_error_fatal, scr_error_plain, scr_error_recoverable, scr_file_table, scr_file_table_cap, scr_file_table_count, scr_frame_count, scr_frame_table_cap, scr_last_file_key, scr_last_source_text, scr_line_table, scr_line_table_cap, scr_line_table_count, scr_pending_a, scr_pending_b, scr_saved_src, scr_saved_src_count, sl_hash, sl_hash_ready, sl_last_link_ptr, str_pool_base

### chunk 014

* delete: `ScrClassInfo`, `ScrVarEntry`, `ScrVarValue`
* replace: `ScrStrSlot` -> `sl_hash_node_s`, `ScrVarValue` -> `VariableValue`
* rename `ScrClassInfo`: classChar -> charId, entityList -> entArrayId, fieldObject -> id
* rename `ScrVarEntry`: link12 -> nextSibling, link14 -> prevSibling, link2 -> hashPrev, var -> hashId
* rename `ScrVarValue`: value -> u
* drop externs (11): g_08297484, g_08397508, g_08397520, g_08397540, g_08397544, g_scrClassInfo, g_scrTypeNames, g_scrVar, scrVarPub_gameId, scrVarPub_levelId, sl_last_link_ptr
* note: ScrClassInfo.classChar (+0x4) local type int vs canonical charId char: check every access

### chunk 015

* delete: `ScrClassDef`, `ScrFrameHeader`, `ScrFrameRecord`, `ScrValueUnion`, `ScrVarEntry`, `VariableValue`
* replace: `ScrClassDef` -> `ScrClassInfo`, `ScrFrameHeader` -> `ScrStackBuffer_s`, `ScrFrameRecord` -> `ScrStackRecord_s`, `ScrValueUnion` -> `VariableUnion`
* rename `ScrClassDef`: classChar -> charId, fieldMapId -> id
* rename `ScrFrameHeader`: recordCount -> size, varId -> localId
* rename `ScrFrameRecord`: kind -> type, value -> value+3
* rename `ScrValueUnion`: uintValue -> intValue
* rename `ScrVarEntry`: flags -> info, link_a -> hashId, link_b -> hashPrev, link_slot -> prevSibling, value -> u
* drop externs (17): g_08205cb0, g_08397500, g_08397510, g_08397520, g_08397530, g_0839753c, g_08397540, g_08397544, g_08397548, g_0839754c, g_scrClassInfo, g_scrTypeNames, g_scrVar, scrVarPub_error_index, scrVarPub_gameId, scrVarPub_inited, scr_compile_error_deferred
* note: ScrClassDef.classChar (+0x4) local type int vs canonical charId char: check every access
* note: ScrFrameHeader: local size 0x14 vs canonical ScrStackBuffer_s 0xb
* note: ScrFrameHeader.records (+0xc) falls in canonical ScrStackBuffer_s padding: add the field to the canon or access via unk bytes
* note: ScrFrameRecord: local size 0x8 vs canonical ScrStackRecord_s 0x5
* note: ScrFrameRecord.value (+0x4) does not start on a canonical field (value+3): split/retype access

### chunk 016

* delete: `ScrClassMapEntry`, `ScrVmFrame`, `VariableUnion`, `VariableValue`, `scrVmPub_t`
* replace: `ScrClassMapEntry` -> `ScrClassInfo`
* rename `ScrClassMapEntry`: classnum -> entArrayId, field0 -> id, id -> charId
* rename `ScrVmFrame`: callerStartTop -> startTop
* drop externs (21): g_08205a58, g_08205c88, g_08205c8c, g_08205c90, g_08205c94, g_script_error, g_script_error_level, scrAnimTreeRoot, scrVarPub_animId, scrVarPub_error_index, scrVarPub_gameId, scrVarPub_inited, scrVarPub_levelId, scrVarPub_tempVariable, scrVarPub_time, scrVarPub_timeArrayId, scrVmGlob_dialog_error_message, scrVmGlob_loading, scrVmGlob_starttime, scrVmPub, scrVmStack
* note: ScrClassMapEntry.id (+0x4) local type int vs canonical charId char: check every access

### chunk 017

* delete: `cod2_scr_entry_s`, `cod2_scr_frame_s`, `cod2_scr_thread_s`, `cod2_scr_varlist_s`, `cod2_scr_varrec_s`
* replace: `cod2_scr_entry_s` -> `VariableValue`, `cod2_scr_frame_s` -> `ScrVmFrame`, `cod2_scr_thread_s` -> `ScrVmFrame`, `cod2_scr_varlist_s` -> `ScrStackBuffer_s`, `cod2_scr_varrec_s` -> `ScrStackRecord_s`
* rename `cod2_scr_entry_s`: value -> u
* rename `cod2_scr_frame_s`: owner -> pos, value -> localId, word2 -> localVarCount, word3 -> callMarker, word4 -> startTop, word5 -> callMarkerType
* rename `cod2_scr_thread_s`: count -> localVarCount, id -> localId, owner -> pos
* rename `cod2_scr_varlist_s`: count -> size, head -> localId, owner -> pos, size -> bufLen, tag -> time
* rename `cod2_scr_varrec_s`: value -> value+3
* drop externs (30): g_08145aa0, g_0820584c, g_08205a50, g_08397510, g_08397520, g_08397548, g_083d7600, g_083d8a84, g_083d8a8c, g_083d8a90, g_083d8a98, g_083d8a9c, g_083dce78, g_083dce98, g_scrTypeNames, g_script_error_level, scrVarPub_animId, scrVarPub_error_index, scrVarPub_levelId, scrVarPub_tempVariable, scrVarPub_time, scrVarPub_timeArrayId, scrVmGlob_dialog_error_message, scrVmGlob_starttime, scrVmPub, scrVmStack, scr_call_depth, scr_call_stack, scr_compile_error_deferred, scr_error_fatal
* note: cod2_scr_frame_s.owner (+0x0) local type uint32 vs canonical pos const char *: check every access
* note: cod2_scr_frame_s.word3 (+0xc) local type uint32 vs canonical callMarker VariableValue *: check every access
* note: cod2_scr_frame_s.word4 (+0x10) local type uint32 vs canonical startTop VariableValue *: check every access
* note: cod2_scr_thread_s.owner (+0x0) local type uint32 vs canonical pos const char *: check every access
* note: cod2_scr_varlist_s: local size 0xc vs canonical ScrStackBuffer_s 0xb
* note: cod2_scr_varlist_s.owner (+0x0) local type uint32 vs canonical pos const char *: check every access
* note: cod2_scr_varrec_s: local size 0x8 vs canonical ScrStackRecord_s 0x5
* note: cod2_scr_varrec_s.value (+0x4) does not start on a canonical field (value+3): split/retype access

### chunk 018

* drop externs (17): g_08205c84, g_083deec0, g_083deecc, scrVmPub, yychar, yycheck, yydefact, yydefgoto, yylval, yynerrs, yypact, yypgoto, yyr1, yyr2, yystos, yytable, yytranslate

### chunk 019

* delete: `client_s`, `dvar_s`
* rename `client_s`: clientDeniedCheckArg -> scriptId, kickResultValue -> guid, lastUsercmd -> lastUsercmd.serverTime, remoteAddressType -> netchan.remoteAddress.type
* rename `dvar_s`: value -> current
* drop externs (39): com_frameTime, g_08423008, g_08423088, g_08440200, g_08440204, g_08440208, g_lex_char_buf, g_lex_column, g_lex_error_pos, g_lex_reserved, g_lex_token_pos, g_server_commands_registered, g_svs_clients, g_svs_nextHeartbeatTime, g_svs_time, g_yy_accept, g_yy_base, g_yy_buffer_stack, g_yy_buffer_stack_max, g_yy_buffer_stack_top, g_yy_c_buf_p, g_yy_chk, g_yy_def, g_yy_did_buffer_switch_on_eof, g_yy_ec, g_yy_flex_debug, g_yy_hold_char, g_yy_init, g_yy_last_accepting_cpos, g_yy_last_accepting_state, g_yy_meta, g_yy_n_chars, g_yy_nxt, g_yy_start, g_yyin, g_yyleng, g_yylineno, g_yyout, g_yytext
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24

### chunk 020

* delete: `challenge_s`, `client_s`, `cvar_s`, `gentity_s`, `msg_s`, `netadr_s`, `reliable_cmd_s`, `sv_entity_s`, `tempban_s`, `usercmd_s`
* replace: `cvar_s` -> `dvar_s`, `reliable_cmd_s` -> `reliableCommand_s`, `sv_entity_s` -> `svEntity_s`
* rename `challenge_s`: connecting -> connected, createdTime -> firstTime, firstTime -> time, lastTime -> pingTime, pingCache -> firstPing
* rename `client_s`: clientRandomId -> scriptId, downloadAckBlock -> downloadClientBlock, downloadBlockLen -> downloadBlockSize, downloadGate -> clientDownloadingWWW, downloadHandle -> download, downloadNextBlock -> downloadCurrentBlock, downloadPos -> downloadCount, downloadQueuedEof -> downloadEOF, downloadRetry -> wwwFallback, downloadSendBlock -> downloadXmitBlock, downloadTime -> downloadSendTime, enterWorldTime -> nextSnapshotTime, gamestateSeq -> gamestateMessageNum, isBot -> bIsTestClient, lastActivityTime -> lastPacketTime, lastClientCommand -> lastClientCommandString, lastClientCommandSeq -> lastClientCommand, lastCmdSeq -> serverId, messageSeq -> messageAcknowledge, pendingDrop -> delayDropMsg, pureStatus -> pureAuthentic, qport -> netchan.qport, redirectSent -> downloadingWWW, redirectUrl -> downloadURL, reliableCmds -> reliableCommands, remoteAddress -> netchan.remoteAddress, snapMsec -> snapshotMsec, textFloodTime -> nextReliableTime, unsentFragments -> netchan.unsentFragments, usercmdAckSeq -> deltaMessage, voiceEnabled -> sendVoice, wwwDownload -> wwwOk
* rename `cvar_s`: v -> current
* rename `gentity_s`: field100 -> r.broadcastTime, field8c -> s.index, flags -> r.svFlags, inUse -> r.linked, linkCount11c -> r.contents, linkedFlag -> r.bmodel, number -> s.number, origin104 -> r.mins, origin110 -> r.maxs, vec138 -> r.currentOrigin, vec144 -> r.currentAngles
* rename `msg_s`: w0 -> overflowed, w2 -> maxsize
* rename `netadr_s`: portPad -> ipx[0], w3 -> ipx[2], w4 -> ipx[6]
* rename `sv_entity_s`: clusters -> clusternums, linked -> baseline.s.number, w00 -> worldSector
* rename `tempban_s`: time -> banTime
* rename `usercmd_s`: b08 -> weapon, b09 -> offHandIndex, w0c -> angles, w1 -> buttons
* drop externs (41): com_dedicated, com_developer, cv_sv_allowDownload, cv_sv_floodProtect, cv_sv_kickBanTime, cv_sv_maxPing, cv_sv_maxRate, cv_sv_maxclients, cv_sv_minPing, cv_sv_privateClients, cv_sv_privatePassword, cv_sv_pure, cv_sv_reconnectlimit, cv_sv_showCommands, cv_sv_wwwBaseURL, cv_sv_wwwDlDisconnected, cv_sv_wwwDownload, cvar_net_lanauthorize, g_0817d838, g_0817d9e0, g_083e2f0c, g_083e3000, g_08423000, g_08423004, g_08423008, g_084230dc, g_084400f0, g_08440104, g_0844010c, g_0844020c, g_08440618, g_08442618, g_0849f61c, g_0849f620, g_0849f624, g_0849f628, g_0849f62c, g_0849f630, g_0849f634, g_svs_clients, g_svs_time
* note: challenge_s.keyA (+0x30, char[33]) is a byte blob spanning canonical fields starting at clientPBguid: replace with field accesses
* note: challenge_s.keyB (+0x51, char[33]) is a byte blob spanning canonical fields starting at clientPBguid2: replace with field accesses
* note: client_s.pendingDrop (+0x8) local type int32 vs canonical delayDropMsg const char *: check every access
* note: client_s.netchanHead (+0x6e6c4, uint8[16]) is a byte blob spanning canonical fields starting at netchan.outgoingSequence: replace with field accesses
* note: cvar_s: local size 0xc vs canonical dvar_s 0x24
* note: gentity_s: gentity +0xf0 is r.linked (was inUse); +0x104 r.mins, +0x110 r.maxs, +0x11c r.contents, +0x138/+0x144 r.currentOrigin/currentAngles.
* note: gentity_s: local size 0x150 vs canonical gentity_s 0x230
* note: msg_s: local size 0x20 vs canonical msg_s 0x18
* note: msg_s.tail (+0x14, uint8[12]) is a byte blob spanning canonical fields starting at bit: replace with field accesses
* note: netadr_s.portPad (+0xa) local type uint16 vs canonical ipx[0] byte: check every access
* note: netadr_s.w3 (+0xc) local type uint32 vs canonical ipx[2] byte: check every access
* note: netadr_s.w4 (+0x10) local type uint32 vs canonical ipx[6] byte: check every access
* note: reliable_cmd_s.text (+0x0, char[1024]) is a byte blob spanning canonical fields starting at command: replace with field accesses
* note: sv_entity_s.w00 (+0x0) local type int32 vs canonical worldSector uint16: check every access

### chunk 021

* delete: `archivedEntity_s`, `client_s`, `cvar_s`, `msg_s`, `netadr_s`, `serverCommand_s`, `serverStatic_s`, `server_s`, `sharedEntity_s`
* replace: `archivedEntity_s` -> `svEntity_s`, `cvar_s` -> `dvar_s`, `serverCommand_s` -> `reliableCommand_s`, `sharedEntity_s` -> `gentity_s`
* rename `archivedEntity_s`: angles -> baseline.r.absmax, f_f0 -> baseline.r.svFlags, f_f4 -> baseline.r.clientMask[0], f_f8 -> baseline.r.clientMask[1], head -> baseline.s, origin -> baseline.r.absmin
* rename `client_s`: info_b1021 -> authKeyA, qport -> netchan.qport, reliableSearchCursor -> reliableSent, remoteAddress -> netchan.remoteAddress
* rename `cvar_s`: v -> current
* rename `msg_s`: allowoverflow -> overflowed
* rename `serverCommand_s`: flags -> cmdType, time -> cmdTime
* rename `serverStatic_s`: buffersEnabled -> archiveEnabled, nextSnapshotClientEntities -> nextSnapshotClients, numSnapshotClientEntities -> numSnapshotClients, pool_30 -> archivedSnapshotFrames, pool_34 -> archivedSnapshotBuffer, pool_48 -> cachedSnapshotEntities, pool_4c -> cachedSnapshotClients, pool_50 -> cachedSnapshotFrames, snapshotClientEntities -> snapshotClients
* rename `server_s`: archivedEntities -> svEntities[0].baseline
* rename `sharedEntity_s`: currentAngles -> r.absmax, currentOrigin -> r.absmin, f_f2 -> r.svFlags, f_f4 -> r.clientMask[0], f_f8 -> r.clientMask[1], linked -> r.linked, number -> s.number
* drop externs (59): com_frameTime, cv_com_expectedHunkUsage, cv_g_gametype, cv_mapname, cv_nextmap, cv_rcon_password, cv_sv_allowAnonymous, cv_sv_allowDownload, cv_sv_allowedClan1, cv_sv_allowedClan2, cv_sv_cheats, cv_sv_debugRate, cv_sv_debugReliableCmds, cv_sv_disableClientConsole, cv_sv_floodProtect, cv_sv_fps, cv_sv_hostname, cv_sv_iwdNames, cv_sv_iwds, cv_sv_kickBanTime, cv_sv_mapRotation, cv_sv_mapRotationCurrent, cv_sv_maxPing, cv_sv_maxRate, cv_sv_maxclients, cv_sv_minPing, cv_sv_packet_info, cv_sv_padPackets, cv_sv_privateClients, cv_sv_privatePassword, cv_sv_punkbuster, cv_sv_pure, cv_sv_reconnectlimit, cv_sv_referencedIwdNames, cv_sv_referencedIwds, cv_sv_serverid, cv_sv_showAverageBPS, cv_sv_showCommands, cv_sv_timeout, cv_sv_voice, cv_sv_voiceQuality, cv_sv_wwwBaseURL, cv_sv_wwwDlDisconnected, cv_sv_wwwDownload, cv_sv_zombietime, dvar_modifiedFlags, fs_numServerIwds, g_0817d838, g_081a21c4, g_083e2f80, g_08423008, g_08440104, g_0849f618, g_0849f624, g_0849f630, g_0849f7e0, g_0860b420, sv_gametypeName, svs
* note: archivedEntity_s: chunk 21 archivedEntities[i] (stride 0x174) start at svEntity+4: use &sv.svEntities[i].baseline (archivedEntity_t); offsets below are svEntity-relative (+4 applied).
* note: cvar_s: local size 0xc vs canonical dvar_s 0x24
* note: msg_s: local size 0x10 vs canonical msg_s 0x18
* note: serverCommand_s.text (+0x0, char[1024]) is a byte blob spanning canonical fields starting at command: replace with field accesses
* note: serverStatic_s: local size 0x54 vs canonical serverStatic_s 0x1d10c
* note: sharedEntity_s: local currentOrigin/currentAngles at 0x120/0x12c are r.absmin/r.absmax.
* note: sharedEntity_s: local size 0x138 vs canonical gentity_s 0x230
* note: sharedEntity_s.head (+0x4, unsigned char[236]) is a byte blob spanning canonical fields starting at s.eType: replace with field accesses

### chunk 022

* delete: `archivedFrame_s`, `cachedClient_s`, `cachedEntity_s`, `cachedSnapshot_s`, `clientSnapshot_s`, `clientState_s`, `client_s`, `dvar_s`, `entityState_s`, `msg_s`, `netProfileStream_s`, `netProfile_s`, `netadr_s`, `netchan_s`, `playerState_s`, `reliableCommand_s`, `sharedEntity_s`, `svEntity_s`, `usercmd_s`
* replace: `archivedFrame_s` -> `archivedSnapshot_s`, `cachedEntity_s` -> `archivedEntity_s`, `sharedEntity_s` -> `gentity_s`
* rename `archivedFrame_s`: offset -> start
* rename `cachedClient_s`: hasPlayerState -> playerStateExists
* rename `cachedEntity_s`: absmax -> r.absmax, absmin -> r.absmin, clientMask -> r.clientMask, es -> s, svFlags -> r.svFlags
* rename `cachedSnapshot_s`: firstClient -> first_client, firstEntity -> first_entity, isDelta -> usesDelta, numClients -> num_clients, numEntities -> num_entities, serverTime -> time
* rename `clientSnapshot_s`: firstClient -> first_client, firstEntity -> first_entity, numClients -> num_clients, numEntities -> num_entities
* rename `client_s`: netProfile -> netchan.prof, noTimeout -> bIsTestClient, slotHandle -> scriptId, snapshotActiveFlag -> sendAsActive
* rename `netProfileStream_s`: bytesPerSec -> iBytesPerSecond, fragDenominator -> iCountedPackets, fragNumerator -> iCountedFragments, fragPercent -> iFragmentPercentage, maxBytes -> iLargestPacket, minBytes -> iSmallestPacket
* rename `netProfile_s`: recv -> recieve, sent -> send
* rename `playerState_s`: word -> commandTime
* rename `reliableCommand_s`: commandTime -> cmdTime
* rename `sharedEntity_s`: absmax -> r.absmax, absmin -> r.absmin, broadcastTime -> r.broadcastTime, clientMask -> r.clientMask, linked -> r.linked, svFlags -> r.svFlags
* drop externs (31): com_cl_paused, com_dedicated, com_sv_paused, com_sv_running, cv_mapname, cv_rcon_password, cv_sv_debugReliableCmds, cv_sv_fps, cv_sv_maxclients, cv_sv_padPackets, cv_sv_timeout, cv_sv_zombietime, cvar_net_profile, dvar_modifiedFlags, g_0817d838, g_08423088, g_08423098, g_0842309c, g_084230ac, g_084230b8, g_084230bc, g_084230c0, g_084230c4, g_08440108, g_08440210, g_0849f624, g_0849fbe0, g_0860b420, g_svs_clients, g_svs_nextHeartbeatTime, g_svs_time
* note: clientState_s.data (+0x4, byte[88]) is a byte blob spanning canonical fields starting at team: replace with field accesses
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: entityState_s.data (+0x4, byte[236]) is a byte blob spanning canonical fields starting at eType: replace with field accesses
* note: netchan_s: local size 0x4003c vs canonical netchan_s 0x40040
* note: sharedEntity_s: local size 0x138 vs canonical gentity_s 0x230
* note: svEntity_s: local size 0x160 vs canonical svEntity_s 0x174

### chunk 023

* delete: `archivedFrame_s`, `cachedClient_s`, `cachedEntity_s`, `cachedFrame_s`, `clientState_s`, `client_frame_s`, `client_s`, `dvar_s`, `entityState_s`, `fileHandleData_s`, `fsEntry_s`, `gentity_s`, `msg_t`, `netadr_t`, `netchan_s`, `playerState_s`, `searchpath_s`, `svEntity_s`, `trace_s`, `voicepacket_s`
* replace: `archivedFrame_s` -> `archivedSnapshot_s`, `boxclip_a_s` -> `BoxHitTrace`, `boxclip_b_s` -> `BoxQueryTrace`, `cachedEntity_s` -> `archivedEntity_s`, `cachedFrame_s` -> `cachedSnapshot_s`, `client_frame_s` -> `clientSnapshot_s`, `fsEntry_s` -> `iwd_s`, `lineclip_a_s` -> `SegHitTrace`, `lineclip_b_s` -> `SegQueryA`, `msg_t` -> `msg_s`, `netadr_t` -> `netadr_s`
* rename `archivedFrame_s`: bufferSize -> size, bufferStart -> start
* rename `cachedClient_s`: hasPlayerState -> playerStateExists
* rename `cachedEntity_s`: absmax -> r.absmax, absmin -> r.absmin, field_f4 -> r.clientMask[0], field_f8 -> r.clientMask[1], svFlags -> r.svFlags
* rename `cachedFrame_s`: firstClient -> first_client, firstEntity -> first_entity, flag18 -> usesDelta, numClients -> num_clients, numEntities -> num_entities
* rename `client_frame_s`: firstClient -> first_client, firstEntity -> first_entity, numClients -> num_clients, numEntities -> num_entities
* rename `client_s`: dropReason -> delayDropMsg, flag20c68 -> downloadName[0], reliableAcknowledge -> lastClientCommand, voiceFlag -> sendVoice
* rename `dvar_s`: v -> current
* rename `entityState_s`: modelindex -> index
* rename `fileHandleData_s`: file -> handleFiles, zipFile -> iwd
* rename `gentity_s`: absmax -> r.absmax, absmin -> r.absmin, bmodel -> r.bmodel, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, field_100 -> r.broadcastTime, field_f4 -> r.clientMask[0], field_f8 -> r.clientMask[1], linked -> r.linked, maxs -> r.maxs, mins -> r.mins, ownerNum -> r.ownerNum, svFlags -> r.svFlags
* rename `netadr_t`: word1 -> ip, word2 -> port, word3 -> ipx[2], word4 -> ipx[6]
* rename `playerState_s`: field_4c -> leanf, field_ec -> viewangles[1], viewheight -> viewHeightCurrent
* rename `searchpath_s`: field_10 -> language, field_c -> bLocalized
* rename `svEntity_s`: baseline -> baseline.s, contents160 -> linkcontents
* rename `trace_s`: dist -> surfaceFlags, field_18 -> material, field_1e -> partName, field_20 -> partGroup
* rename `voicepacket_s`: len -> dataSize+3
* drop externs (26): cv_sv_debugRate, cv_sv_fps, cv_sv_maxRate, cv_sv_maxclients, cv_sv_showAverageBPS, cv_sv_voice, fs_gamedir, fs_ignoreLocalized, fs_loadStack, fs_numServerIwds, fs_searchpaths, fs_serverIwds, fsh, g_0814cfc8, g_08423098, g_0842309c, g_084230ac, g_084230b8, g_084230bc, g_084230c0, g_084230c4, g_08440200, g_08442618, g_0849f624, g_svs_clients, g_svs_time
* note: cachedEntity_s.field_f8 (+0xf8) local type float vs canonical r.clientMask[1] int: check every access
* note: client_s: local size 0xb10dc vs canonical client_s 0xb1064
* note: client_s.muteFlags (+0xb0fe0, byte[64]) is a byte blob spanning canonical fields starting at muteList: replace with field accesses
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: fileHandleData_s.zipFile (+0x14) local type int vs canonical iwd struct iwd_s *: check every access
* note: fsEntry_s: local size 0x308 vs canonical iwd_s 0x320
* note: gentity_s: local 0x274-byte layout is refuted (gentity stride 0x230).
* note: gentity_s: local size 0x274 vs canonical gentity_s 0x230
* note: gentity_s.field_f8 (+0xf8) local type float vs canonical r.clientMask[1] int: check every access
* note: msg_t: local size 0x28 vs canonical msg_s 0x18
* note: netadr_t.word2 (+0x8) local type int vs canonical port uint16: check every access
* note: netadr_t.word3 (+0xc) local type int vs canonical ipx[2] byte: check every access
* note: netadr_t.word4 (+0x10) local type int vs canonical ipx[6] byte: check every access
* note: netchan_s: local size 0x2003c vs canonical netchan_s 0x40040
* note: playerState_s.field_4c (+0x4c) local type int vs canonical leanf float: check every access
* note: playerState_s.field_ec (+0xec) local type int vs canonical viewangles[1] float: check every access
* note: svEntity_s.worldSector (+0x0) local type int vs canonical worldSector uint16: check every access
* note: trace_s.dist (+0x10) local type float vs canonical surfaceFlags int: check every access
* note: trace_s.field_18 (+0x18) local type int vs canonical material struct dMaterial_s *: check every access
* note: voicepacket_s: local struct is unpacked (0x108); canonical voicepacket_s is packed 0x105.
* note: voicepacket_s: local size 0x108 vs canonical voicepacket_s 0x105
* note: voicepacket_s.len (+0x104) does not start on a canonical field (dataSize+3): split/retype access

### chunk 024

* delete: `cvar_s`, `fs_directory_s`, `fs_handle_s`, `fs_pack_s`, `fs_searchpath_s`
* replace: `cvar_s` -> `dvar_s`, `fs_directory_s` -> `directory_s`, `fs_handle_s` -> `fileHandleData_s`, `fs_pack_s` -> `iwd_s`, `fs_packfile_state_s` -> `unz_s`, `fs_searchpath_s` -> `searchpath_s`
* rename `cvar_s`: value -> current
* rename `fs_handle_s`: handle -> handleFiles, pack -> iwd, streamOpen -> streamed, uniqueFile -> zipFile
* rename `fs_pack_s`: checksumPure -> pure_checksum, pakBasename -> iwdBasename, pakFilename -> iwdFilename, pakGamename -> iwdGamename
* rename `fs_searchpath_s`: localized -> bLocalized, pack -> iwd
* drop externs (16): fs_basepath, fs_cdpath, fs_checksumFeed, fs_copyfiles, fs_debug, fs_gamedir, fs_homepath, fs_ignoreLocalized, fs_loadStack, fs_restrict, fs_searchpaths, g_0849fc20, g_0849fd70, g_084a86e0, g_084a8760, g_084a8764
* note: cvar_s: local size 0xc vs canonical dvar_s 0x24
* note: fs_pack_s.referenced_pad (+0x311) falls in canonical iwd_s padding: add the field to the canon or access via unk bytes

### chunk 025

* delete: `cod2_dvar_s`, `cod2_fsh_s`, `cod2_iwd_s`, `cod2_pak_node_s`
* replace: `cod2_dvar_s` -> `dvar_s`, `cod2_fsh_s` -> `fileHandleData_s`, `cod2_iwd_s` -> `iwd_s`, `cod2_pak_node_s` -> `searchpath_s`
* rename `cod2_dvar_s`: value -> current
* rename `cod2_iwd_s`: file_handle -> handle, index_buffer -> buildBuffer, reference_flag -> referenced
* rename `cod2_pak_node_s`: owned_name -> dir
* drop externs (13): com_sv_running, fs_basegameDvar, fs_checksumFeed, fs_defaultConfigName, fs_gameDvar, fs_iwdFileCount, fs_savedBasePath, fs_savedGame, fs_useOldAssetsDvar, fsh_table, g_0814cfc8, g_0814d100, g_0817dba0
* note: cod2_dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: cod2_fsh_s: fsh_table (0x0849fd88) is fsh (0x0849fd80)+8: index fsh[] and add 8 to every local offset (applied below).
* note: cod2_pak_node_s: local size 0xc vs canonical searchpath_s 0x14

### chunk 026

* drop externs (2): g_0814cfc8, g_0817e338

### chunk 027

* delete: `dvar_value8_t`
* replace: `dvar_value8_t` -> `dvar_s`
* rename `dvar_value8_t`: value -> current
* drop externs (24): com_sv_running, g_0817e3a0, g_084a87a0, g_084a97a4, g_084a9800, g_085a9804, g_085a9808, g_085a980c, g_085a9810, g_085a9814, g_085a9818, g_085a981c, g_085a9840, g_085a9844, g_085a9848, g_085aa848, g_085aa878, g_085aa8d0, g_085aa8d4, g_085aad94, g_085ab160, g_085ab164, g_085ab168, g_sndAliasColumnNames
* note: dvar_value8_t: local size 0xc vs canonical dvar_s 0x24

### chunk 028

* delete: `dvar_enum_desc_s`, `dvar_s`
* replace: `dvar_enum_desc_s` -> `DvarLimitsEnum`, `snd_alias_entry_s` -> `snd_alias_build_t`, `snd_alias_list_s` -> `snd_alias_list_t`, `snd_alias_s` -> `snd_alias_t`, `snd_alias_set_s` -> `snd_alias_bank_t`, `snd_curve_s` -> `snd_curve_t`
* rename `dvar_enum_desc_s`: count -> stringCount, names -> strings
* rename `dvar_s`: enumCount -> domain.enumeration.stringCount, enumNames -> domain.enumeration.strings
* drop externs (16): fs_basepath, fs_gamedir, g_085ab168, g_085b7654, g_085b7655, g_canonicalOff, g_canonicalOn, g_digitStringTable, g_dvarScratch, g_dvarScratchIndex, g_emptyString, g_sndAliasColumnNames, g_sndEmptyString, g_sndReferenceNameBuf, g_sndVolModCmdRegistered, g_sndVolModGroupsDirty
* note: dvar_s: local size 0x1c vs canonical dvar_s 0x24

### chunk 029

* delete: `dvar_s`
* replace: `parse_session_s` -> `ComParseState`, `parse_slot_s` -> `ParseSession`
* rename `dvar_s`: domain_max -> domain.enumeration.strings, domain_min -> domain.enumeration.stringCount, hash_next -> hashNext, sorted_next -> next, value -> current
* drop externs (9): dvar_indexCount, dvar_modifiedFlags, dvar_vars, g_0817e420, g_085abe20, g_085b7220, g_085b7654, g_085b7655, g_dvar_sv_cheats
* note: dvar_s.domain_max (+0x18) local type uint32 vs canonical domain.enumeration.strings const char **: check every access

### chunk 030

* drop externs (13): g_0817e3e0, g_085b7660_slots, g_085b76a0, g_085b86c0, g_085b8800, g_085bc800, g_085bc804, g_085bc808, g_085bc80c, g_085bc810, g_085bc814, g_085bc818, g_085bc81c

### chunk 031

* replace: `XModelHitBox` -> `XBoneInfo`
* drop externs (3): g_0814cfc8, g_08182e00, g_085bc828

### chunk 032

* replace: `xanim_asset_s` -> `XAnimParts`, `xanim_blend_slot_s` -> `XAnimSlot`, `xanim_key_s` -> `XAnimNotify`, `xanim_node_s` -> `XAnimNode`, `xanim_node_table_s` -> `XAnimNodeTable`, `xanim_tree_s` -> `XAnimTree`
* drop externs (7): com_developer, g_085bc900, g_085e4900, g_085e4920, g_085e4f20, g_085e4f24, g_085e4f25

### chunk 033

* replace: `XAnimClip` -> `XAnimParts`, `XAnimPartRec` -> `xanim_track_s`
* drop externs (3): g_085bc900, g_085e4920, g_085e4f20

### chunk 034

* replace: `DObjAnimMat` -> `DObjBone`, `XAnimKeyHeader` -> `xanim_track_s`
* drop externs (8): g_085e4f40, g_085e4f44, g_085e4f4c, g_085e4fb0, g_085e4fc4, g_085e4fec, g_085e4fee, g_085e4ff8

### chunk 035

* delete: `z_stream`
* replace: `XModelLodInfo` -> `XModelLod`, `deflate_state` -> `deflate_state_s`, `inflate_blocks_state` -> `inflate_blocks_state_s`, `z_stream` -> `z_stream_s`
* drop externs (8): _dist_code, _length_code, configuration_table, g_085e5020, inflate_border, inflate_mask, z_errmsg, zlib_version

### chunk 036

* drop externs (13): g_08150b9c, g_08150be0, g_08150c60, g_08150ce0, g_08150d60, g_08183240, g_08183244, g_08183260, g_08184260, g_081843a4, g_081843b8, g_081843cc, inflate_mask

### chunk 037

* drop externs (15): _length_code, g_08150f40, g_08150fc0, g_0815108c, g_081510a0, g_08151520, g_0815159f, g_081516a0, g_081518a0, g_08151920, g_08606888, tty_con, tty_tc, ttycon_on, z_errmsg

### chunk 038

* delete: `msg_s`, `netadr_s`
* drop externs (33): eventHead, eventQue, eventTail, ip_socket, ipx_socket, localIP, net_noudp_cvar, numIP, stdin_active, sys_arch_cvar, sys_cdpath, sys_console_text, sys_curtime, sys_cwd, sys_exit_cmdline, sys_homepath, sys_installpath, sys_ipstring, sys_mainthread, sys_mutexes, sys_packetReceived, sys_timeBase, sys_username_cvar, sys_values, tty_con, tty_eof, tty_erase, tty_hist_current, tty_history, tty_history_count, tty_tc, ttycon_cvar, ttycon_on

### chunk 039

* delete: `dvarFloatView_s`, `entityState_s`, `playerState_s`, `pmove_s`, `weaponDef_s`
* replace: `clientAnimData_t` -> `clientInfo_s`, `dvarFloatView_s` -> `dvar_s`
* rename `dvarFloatView_s`: value -> current
* rename `entityState_s`: trBase -> pos.trBase
* rename `playerState_s`: f0xe8 -> viewangles[0]
* rename `pmove_s`: cmdButtons -> cmd.buttons, cmdField4 -> cmd.serverTime
* drop externs (23): dvar_bg_swingSpeed, g_08184860, g_08184880, g_081848c0, g_08184a20, g_08184ac0, g_08184c00, g_08184c60, g_08184cc0, g_0860b420, g_0860b424, g_0860b440, g_0860b480, g_0860db90, g_0860dba0, g_0860e020, g_0860e4a0, g_0860e4a4, g_0860e4a8, g_0860e4ac, g_0860e4c0, g_08626b60, g_08626b80
* note: dvarFloatView_s: local size 0xc vs canonical dvar_s 0x24
* note: entityState_s: local size 0xd0 vs canonical entityState_s 0xf0
* note: playerState_s: local size 0xec vs canonical playerState_s 0x26a8
* note: pmove_s: local size 0xc vs canonical pmove_s 0xf8
* note: weaponDef_s: local size 0x80 vs canonical weaponDef_s 0x604

### chunk 040

* delete: `BGEntity`, `JumpMoveOut`, `MantleState`, `PlayerState`, `PmoveCtx`, `Trace040`, `Trajectory`, `UserCmd040`, `WeaponDef040`, `cvar_s`
* replace: `BGEntity` -> `entityState_s`, `BGItem` -> `gitem_s`, `JumpMoveOut` -> `pml_s`, `PlayerState` -> `playerState_s`, `PlayerViewState` -> `clientInfo_s`, `PmoveCtx` -> `pmove_s`, `Trace040` -> `trace_s`, `Trajectory` -> `trajectory_s`, `UserCmd040` -> `pml_s`, `WeaponDef040` -> `weaponDef_s`, `cvar_s` -> `dvar_s`
* rename `BGEntity`: itemIndex -> index, ownerClient -> clientNum
* rename `JumpMoveOut`: dir -> forward, f2c -> walking, f30 -> groundPlane, f34 -> almostGroundPlane
* rename `MantleState`: stage -> transIndex, time -> timer
* rename `PlayerState`: ammo12c -> stats[0], anim90 -> damageTimer, bits544 -> weapons, clientHandle -> commandTime, deltaYaw58 -> delta_angles[1], field134 -> stats[2], field_d0 -> offHandIndex, gravity48 -> gravity, jumpBaseZ -> jumpOriginZ, jumpBits60 -> groundEntityNum, jumpPhase -> pm_time, lastJumpTime -> jumpTime, mantleFlags -> mantleState.flags, mantleMaxs -> maxs, mantleMins -> mins, mantleStage -> mantleState.transIndex, mantleTime -> mantleState.timer, mantleYaw -> mantleState.yaw, pmFlags -> bobCycle, pmFlags2 -> pm_flags, pmFlags3 -> eFlags, pmType -> pm_type, spread -> aimSpreadScale, viewPitchOffset -> viewangles[0], viewYaw -> viewangles[1], viewZ -> viewmodelIndex, wallNormal -> vLadderVec, weaponIndex -> oldEventSequence, weaponState -> weaponstate
* rename `PmoveCtx`: buttons -> cmd.buttons, forwardMove -> cmd.forwardmove, mantleActive -> mantleStarted, mantleEndHeight -> mantleDuration, mantleTarget -> mantleEndPos, oldButtons -> oldcmd.buttons, serverTime -> cmd.serverTime, traceMask -> tracemask
* rename `Trace040`: allSolid -> allsolid, startSolid -> startsolid
* rename `UserCmd040`: deltaMsec -> msec
* rename `WeaponDef040`: animTrack74 -> playerAnimType, animTrack7c -> weaponClass
* rename `cvar_s`: raw_00 -> name, val -> current
* drop externs (60): dvar_bg_aimSpreadMoveSpeedThreshold, dvar_bg_bobAmplitudeDucked, dvar_bg_bobAmplitudeProne, dvar_bg_bobAmplitudeStanding, dvar_bg_bobMax, dvar_bg_fallDamageMaxHeight, dvar_bg_fallDamageMinHeight, dvar_bg_foliagesnd_fastinterval, dvar_bg_foliagesnd_maxspeed, dvar_bg_foliagesnd_minspeed, dvar_bg_foliagesnd_resetinterval, dvar_bg_foliagesnd_slowinterval, dvar_bg_ladder_yawcap, dvar_bg_prone_yawcap, dvar_bg_swingSpeed, dvar_friction, dvar_inertiaAngle, dvar_inertiaDebug, dvar_inertiaMax, dvar_jump_height, dvar_jump_ladderPushVel, dvar_jump_slowdownEnable, dvar_jump_spreadAdd, dvar_jump_stepSize, dvar_mantle_check_angle, dvar_mantle_check_radius, dvar_mantle_check_range, dvar_mantle_debug, dvar_mantle_enable, dvar_mantle_view_yawcap, dvar_player_adsExitDelay, dvar_player_backSpeedScale, dvar_player_breath_fire_delay, dvar_player_breath_gasp_lerp, dvar_player_breath_gasp_scale, dvar_player_breath_gasp_time, dvar_player_breath_hold_lerp, dvar_player_breath_hold_time, dvar_player_breath_snd_delay, dvar_player_breath_snd_lerp, dvar_player_dmgtimer_flinchTime, dvar_player_dmgtimer_maxTime, dvar_player_dmgtimer_minScale, dvar_player_dmgtimer_stumbleTime, dvar_player_dmgtimer_timePerPoint, dvar_player_footstepsThreshhold, dvar_player_moveThreshhold, dvar_player_scopeExitOnDamage, dvar_player_spectateSpeedScale, dvar_player_strafeSpeedScale, dvar_player_toggleBinoculars, dvar_player_turnAnims, dvar_player_view_pitch_down, dvar_player_view_pitch_up, dvar_stopspeed, g_08184d00, g_08184d60, g_081850c0, g_0860b420, g_08626fb0
* note: BGEntity: BGEntity is the entityState head of a gentity (eType@4, index@0x8c, clientNum@0x90).
* note: BGEntity: local size 0x94 vs canonical entityState_s 0xf0
* note: JumpMoveOut: local size 0x38 vs canonical pml_s 0x7c
* note: JumpMoveOut.f2c (+0x2c) local type float vs canonical walking int: check every access
* note: JumpMoveOut.f30 (+0x30) local type float vs canonical groundPlane int: check every access
* note: JumpMoveOut.f34 (+0x34) local type float vs canonical almostGroundPlane int: check every access
* note: PlayerState: PlayerAnim_*/PlayerView_* functions receive an entityState_s (pmove.md): retype those parameters to entityState_s, not playerState_s.
* note: PlayerState: local size 0x5d8 vs canonical playerState_s 0x26a8
* note: PlayerState.viewZ (+0xe4) local type float vs canonical viewmodelIndex int: check every access
* note: UserCmd040: local size 0x2c vs canonical pml_s 0x7c
* note: WeaponDef040: local size 0x80 vs canonical weaponDef_s 0x604
* note: cvar_s: local size 0xc vs canonical dvar_s 0x24

### chunk 041

* delete: `entityState_s`, `playerState_s`, `pml_s`, `pmove_s`, `trace_s`, `usercmd_s`
* rename `entityState_s`: apos -> apos.trBase, event -> eventParm, field_30 -> apos.trType, field_74 -> otherEntityNum, field_78 -> attackerEntityNum, field_80 -> constantLight, field_cc -> legsAnim, field_d0 -> torsoAnim, field_e4 -> fTorsoHeight, field_e8 -> fTorsoPitch, field_ec -> fWaistPitch, moveAxisBits -> leanf, movementDirF -> angles2[1], trBase -> pos.trBase, trType -> pos.trType
* rename `playerState_s`: buttonBits -> eFlags, eventReadCursor -> entityEventSequence, eventSequenceSent -> oldEventSequence, field_10 -> pm_time, field_584 -> proneDirection, field_594 -> viewlocked_entNum, field_5a8 -> fTorsoHeight, field_5ac -> fTorsoPitch, field_5b0 -> fWaistPitch, field_64 -> vLadderVec[0], field_7c -> legsAnim, field_84 -> torsoAnim, field_d0 -> offHandIndex, linkedEntityNum -> weapon, moveAxis -> leanf, pmFlags -> pm_flags, pmTime -> damageTimer, pmType -> pm_type, stanceFloat -> viewHeightLerpPosAdj, stanceTarget -> viewHeightLerpTarget, stanceToggled -> viewHeightLerpDown, stanceTransitionStart -> viewHeightLerpTime, viewAngles -> viewangles, viewheight -> viewHeightCurrent, viewheightTarget -> viewHeightTarget
* rename `pml_s`: groundTraceHit -> almostGroundPlane
* rename `pmove_s`: traceMode -> handler
* rename `trace_s`: allsolid -> startsolid, startsolid -> allsolid
* rename `usercmd_s`: forwardMove -> forwardmove, rightMove -> rightmove
* drop externs (20): dvar_bg_fallDamageMaxHeight, dvar_bg_fallDamageMinHeight, dvar_friction, dvar_inertiaAngle, dvar_inertiaDebug, dvar_inertiaMax, dvar_player_backSpeedScale, dvar_player_dmgtimer_maxTime, dvar_player_dmgtimer_minScale, dvar_player_spectateSpeedScale, dvar_player_strafeSpeedScale, dvar_stopspeed, g_0814cfc8, g_0815486c, g_081551e0, g_08186780, g_08186800, g_08186880, g_08186960, g_081869c0
* note: entityState_s.moveAxisBits (+0xd4) local type int32 vs canonical leanf float: check every access
* note: playerState_s: local size 0x5d0 vs canonical playerState_s 0x26a8
* note: playerState_s.field_5a8 (+0x5a8) local type int32 vs canonical fTorsoHeight float: check every access
* note: pml_s: local size 0x78 vs canonical pml_s 0x7c
* note: pmove_s: local size 0xe8 vs canonical pmove_s 0xf8
* note: trace_s: byte 0x22 is allsolid and 0x23 startsolid (disasm 0x0805a169/0x0805a15c); local names were swapped.

### chunk 042

* delete: `dvarValue_u`, `dvar_s`, `playerState_s`, `pml_s`, `pmove_s`, `trace_s`, `usercmd_s`
* replace: `clientAnimCtrl_s` -> `clientInfo_s`, `dvarValue_u` -> `DvarValue`
* rename `dvarValue_u`: b -> enabled, f -> enabled, i -> enabled
* rename `dvar_s`: value -> current
* rename `playerState_s`: field_130 -> stats[1], field_44 -> foliageSoundTime, field_48 -> gravity, field_584 -> proneDirection, field_588 -> proneDirectionPitch, field_58c -> proneTorsoPitch, field_5a8 -> fTorsoHeight, field_5ac -> fTorsoPitch, field_5b0 -> fWaistPitch, field_70 -> jumpTime, field_78 -> legsTimer, field_80 -> torsoTimer, field_88 -> legsAnimDuration, field_90 -> damageTimer, field_94 -> damageDuration, field_98 -> flinchYaw, field_f8 -> viewHeightCurrent, field_fc -> viewHeightLerpTime, ladderNormal -> vLadderVec, limitCenter -> viewAngleClampBase, limitRange -> viewAngleClampRange, moveAngleDelta -> movementDir, smoothedVelocity -> oldVelocity, stance -> viewHeightTarget, weaponState -> weaponstate
* rename `pml_s`: field_34 -> almostGroundPlane
* rename `pmove_s`: clipType -> handler, field_40 -> numtouch, stanceChanged -> proneChange
* drop externs (18): dvar_bg_foliagesnd_fastinterval, dvar_bg_foliagesnd_maxspeed, dvar_bg_foliagesnd_minspeed, dvar_bg_foliagesnd_resetinterval, dvar_bg_foliagesnd_slowinterval, dvar_bg_ladder_yawcap, dvar_bg_prone_yawcap, dvar_player_backSpeedScale, dvar_player_dmgtimer_flinchTime, dvar_player_dmgtimer_stumbleTime, dvar_player_footstepsThreshhold, dvar_player_moveThreshhold, dvar_player_strafeSpeedScale, dvar_player_turnAnims, dvar_player_view_pitch_down, dvar_player_view_pitch_up, g_081869c0, g_0860b420
* note: dvarValue_u.f (+0x0) local type float vs canonical enabled byte: check every access
* note: dvarValue_u.i (+0x0) local type int vs canonical enabled byte: check every access
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: playerState_s: local size 0x5b4 vs canonical playerState_s 0x26a8
* note: pmove_s: local size 0xe8 vs canonical pmove_s 0xf8
* note: trace_s: local size 0x28 vs canonical trace_s 0x24

### chunk 043

* delete: `WeaponDef043_s`, `dvar043_s`, `dvarValue043_u`, `playerState043_s`, `pmTrace043_s`, `pml043_s`, `pmove043_s`
* replace: `WeaponDef043_s` -> `weaponDef_s`, `bgItem043_s` -> `gitem_s`, `dvar043_s` -> `dvar_s`, `dvarValue043_u` -> `DvarValue`, `playerState043_s` -> `playerState_s`, `pmTrace043_s` -> `trace_s`, `pml043_s` -> `pml_s`, `pmove043_s` -> `pmove_s`
* rename `WeaponDef043_s`: altWeaponName -> altWeapon, ammoIndex -> iAmmoIndex, clipIndex -> iClipIndex, field04Name -> displayName, field1b4 -> worldModel, field1b8 -> hudIcon, field1c0 -> startAmmo, field278 -> adsOverlayReticle, field28c -> hipSpreadStandMin, field290 -> hipSpreadDuckedMin, field294 -> hipSpreadProneMin, field298 -> hipSpreadMax, field29c -> hipSpreadDuckedMax, field2a0 -> hipSpreadProneMax, field32c -> aimDownSight, field330 -> rechamberWhileAds, field340 -> clipOnly, field348 -> adsFire, field35c -> segmentedReload, field360 -> reloadAmmoAdd, field364 -> reloadStartAdd, field524 -> adsReloadTransTime, field588 -> adsTransInTimeInv, field58c -> adsTransOutTimeInv, field78 -> weaponType, field7c -> weaponClass, field84 -> offhandClass, field98 -> pickupSound, internalName -> szInternalName, sharedAmmoCapIndex -> iSharedAmmoCapIndex
* rename `dvar043_s`: field00 -> name, field04 -> flags
* rename `playerState043_s`: field00 -> commandTime, field34 -> weaponTime, field38 -> weaponDelay, field_128 -> damageCount, field_578 -> maxs[0], field_584 -> proneDirection, field_5a8 -> fTorsoHeight, field_5b4 -> holdBreathScale, field_5b8 -> holdBreathTimer, field_5d0 -> weapAnim, field_e0 -> adsDelayTime, heldSlot -> weaponslots[0], viewHeight -> viewHeightCurrent, weaponPosFrac -> fWeaponPosFrac
* rename `pmTrace043_s`: allSolid -> allsolid
* rename `pml043_s`: field2c -> walking, field34 -> almostGroundPlane, field38 -> groundTrace.fraction, groundNormal -> groundTrace.normal
* rename `pmove043_s`: cmdButtons -> cmd.buttons, cmdByte1c -> cmd.forwardmove, cmdByte1d -> cmd.rightmove, cmdServerTime -> cmd.serverTime, fieldE4 -> handler, oldCmdButtons -> oldcmd.buttons
* drop externs (16): bg_ammoTypes, bg_clipTypes, bg_numAmmoTypes, bg_numClipTypes, bg_numSharedAmmoCaps, bg_numWeapons, bg_sharedAmmoCaps, bg_weaponDefs, dvar_player_adsExitDelay, dvar_player_breath_fire_delay, dvar_player_breath_gasp_lerp, dvar_player_breath_gasp_scale, dvar_player_breath_gasp_time, dvar_player_breath_hold_lerp, dvar_player_breath_hold_time, dvar_player_scopeExitOnDamage
* note: WeaponDef043_s: local size 0x590 vs canonical weaponDef_s 0x604
* note: WeaponDef043_s.field98 (+0x98) local type int vs canonical pickupSound const char *: check every access
* note: WeaponDef043_s.field1b4 (+0x1b4) local type int vs canonical worldModel const char *: check every access
* note: WeaponDef043_s.field1b8 (+0x1b8) local type int vs canonical hudIcon const char *: check every access
* note: dvar043_s: local size 0xc vs canonical dvar_s 0x24
* note: dvar043_s.field00 (+0x0) local type int vs canonical name const char *: check every access
* note: dvar043_s.field04 (+0x4) local type int vs canonical flags uint16: check every access
* note: playerState043_s: local size 0x5d4 vs canonical playerState_s 0x26a8
* note: playerState043_s.field_578 (+0x578) local type int vs canonical maxs[0] float: check every access
* note: playerState043_s.field_584 (+0x584) local type int vs canonical proneDirection float: check every access
* note: pmTrace043_s: local size 0x34 vs canonical trace_s 0x24
* note: pml043_s: local size 0x60 vs canonical pml_s 0x7c
* note: pml043_s.field38 (+0x38) local type int vs canonical groundTrace.fraction float: check every access
* note: pmove043_s: local size 0xe8 vs canonical pmove_s 0xf8

### chunk 044

* delete: `cvar_value8_s`, `playerState_s`, `pml_s`, `pmove_s`, `weapon_def_s`
* replace: `cvar_value8_s` -> `dvar_s`, `weapon_def_s` -> `weaponDef_s`
* rename `cvar_value8_s`: value -> current
* rename `playerState_s`: buttonBits -> eFlags, clipAmmo -> ammoclip, field_128 -> damageCount, field_50 -> speed, field_f4 -> viewHeightTarget, offhandTimer -> grenadeTimeLeft, offhandWeapon -> offHandIndex, pmFlags -> pm_flags, pmType -> pm_type, reserveAmmo -> ammo, shotDelay -> weaponRestrictKickTime, viewheight -> viewHeightCurrent, weaponBits2 -> weaponrechamber, weaponMeter -> aimSpreadScale, weaponState -> weaponstate, weaponTimer -> weaponTime, weaponsHeld -> weapons
* rename `pmove_s`: cmdAngles -> cmd.angles, cmdButtons -> cmd.buttons, cmdForwardMove -> cmd.forwardmove, cmdOffhand -> cmd.offHandIndex, cmdRightMove -> cmd.rightmove, cmdServerTime -> cmd.serverTime, cmdWeapon -> cmd.weapon, field_e0 -> proneChange, oldAngles -> oldcmd.angles, oldButtons -> oldcmd.buttons, oldOffhand -> oldcmd.offHandIndex, oldServerTime -> oldcmd.serverTime, oldWeapon -> oldcmd.weapon
* rename `weapon_def_s`: altDelayCount -> meleeDelay, altFireEnabled -> meleeDamage, altWeapon36c -> altWeaponIndex, fField1a0 -> posRotRate, fField1a4 -> posProneRotRate, fField1a8 -> standRotMinSpeed, fField1ac -> duckedRotMinSpeed, fField1b0 -> proneRotMinSpeed, fField2a4 -> hipSpreadDecayRate, fField2a8 -> hipSpreadFireAdd, fField2ac -> hipSpreadTurnAdd, fField2b0 -> hipSpreadMoveAdd, fField2b4 -> hipSpreadDuckedDecay, fField2b8 -> hipSpreadProneDecay, fField2c8 -> adsIdleAmount, fField2cc -> hipIdleAmount, fField2d0 -> adsIdleSpeed, fField2d4 -> hipIdleSpeed, fField2d8 -> idleCrouchFactor, fField2dc -> idleProneFactor, fField46c -> adsAimPitch, fField47c -> adsGunKickReducedKickPercent, fField480 -> adsGunKickPitchMin, fField484 -> adsGunKickPitchMax, fField488 -> adsGunKickYawMin, fField48c -> adsGunKickYawMax, fField4a0 -> adsViewKickPitchMin, fField4a4 -> adsViewKickPitchMax, fField4a8 -> adsViewKickYawMin, fField4ac -> adsViewKickYawMax, fField4c4 -> hipGunKickReducedKickPercent, fField4c8 -> hipGunKickPitchMin, fField4cc -> hipGunKickPitchMax, fField4d0 -> hipGunKickYawMin, fField4d4 -> hipGunKickYawMax, fField4e8 -> hipViewKickPitchMin, fField4ec -> hipViewKickPitchMax, fField4f0 -> hipViewKickYawMin, fField4f4 -> hipViewKickYawMax, fField588 -> adsTransInTimeInv, fView138 -> standRotP, fView15c -> duckedRotP, fView180 -> proneRotP, field_278 -> adsOverlayReticle, field_324 -> semiAuto, field_33c -> cookOffHold, field_348 -> adsFire, field_358 -> noPartialReload, field_35c -> segmentedReload, field_360 -> reloadAmmoAdd, field_478 -> adsGunKickReducedKickBullets, field_4c0 -> hipGunKickReducedKickBullets, field_74 -> playerAnimType, field_7c -> weaponClass, name -> szInternalName, offhandTime248 -> fuseTime, shotDelayBase -> fireDelay, shotInterval -> fireTime, shotsPerStepMax -> rechamberBoltTime, stateCount228 -> reloadStartAddTime, stateTime208 -> rechamberTime, stateTime210 -> holdFireTime, stateTime214 -> meleeTime, stateTime218 -> reloadTime, stateTime21c -> reloadEmptyTime, stateTime224 -> reloadStartTime, stateTime22c -> reloadEndTime, stateTime230 -> dropTime, stateTime234 -> raiseTime, stateTime238 -> altDropTime, stateTime23c -> altRaiseTime, stateTime240 -> quickDropTime, stateTime244 -> quickRaiseTime, stateTime2c0 -> adsTransInTime, stateTime2c4 -> adsTransOutTime, stepLimit220 -> reloadAddTime, usesPendingBit -> boltAction
* drop externs (6): dvar_bg_aimSpreadMoveSpeedThreshold, dvar_bg_bobAmplitudeDucked, dvar_bg_bobAmplitudeProne, dvar_bg_bobAmplitudeStanding, dvar_player_adsExitDelay, dvar_player_toggleBinoculars
* note: cvar_value8_s: local size 0xc vs canonical dvar_s 0x24
* note: playerState_s: local size 0x5d8 vs canonical playerState_s 0x26a8
* note: pml_s: local size 0x2c vs canonical pml_s 0x7c
* note: pmove_s: local size 0xe4 vs canonical pmove_s 0xf8
* note: weapon_def_s: local size 0x58c vs canonical weaponDef_s 0x604

### chunk 045

* delete: `gclient_s`, `gentity_s`, `playerstate_s`, `trace_s`, `weapon_def_s`
* replace: `entity_class_s` -> `entity_handler_s`, `playerstate_s` -> `playerState_s`, `weapon_def_s` -> `weaponDef_s`
* rename `gclient_s`: ammo_max_sample -> maxHealth, buttons -> cmd.buttons, buttons_edges -> buttonsSinceLastFrame, buttons_prev -> buttons, buttons_prev2 -> oldbuttons, event_count -> ps.eventSequence, event_id -> ps.events, event_parm -> ps.eventParms, flags_0c -> ps.pm_flags, inactivity_deadline -> inactivityTime, inactivity_warned -> inactivityWarning, last_activity_time -> lastActivityTime, recoil_angles -> damageDir, recoil_count -> ps.damageEvent, recoil_level -> ps.damageCount, recoil_pitch_neg -> recoilPitchNeg, recoil_pitch_out -> recoilPitchOut, recoil_samples -> damageTaken, recoil_stamp -> recoilStamp, speed_scale_q -> ps.stats[2], state -> ps.pm_type, view_angles -> ps.viewangles, view_pitch_q8 -> ps.damagePitch, view_roll_q8 -> ps.damageYaw
* rename `gentity_s`: box_a -> u.item+68, box_b -> u.item+80, box_mode -> parent, contents -> r.contents, entity_class -> handler, flags_174 -> flags, kind_f4 -> r.clientMask[0], model_handle -> freeAfterEvent, owner_num -> r.linked
* rename `playerstate_s`: lean_offset -> leanf, pm_flags -> eFlags, speed_scale -> holdBreathScale, stance_blend -> fWeaponPosFrac, view_angles_e8 -> viewangles, yaw_byte -> bobCycle
* rename `trace_s`: allsolid -> startsolid, endpos -> normal, normal -> surfaceFlags, plane_dist_or_entity -> entityNum, startsolid -> allsolid
* rename `weapon_def_s`: locGun -> locationDamageMultipliers[18], locHead -> locationDamageMultipliers[2], locHelmet -> locationDamageMultipliers[1], locLeftArmLower -> locationDamageMultipliers[9], locLeftArmUpper -> locationDamageMultipliers[7], locLeftFoot -> locationDamageMultipliers[17], locLeftHand -> locationDamageMultipliers[11], locLeftLegLower -> locationDamageMultipliers[15], locLeftLegUpper -> locationDamageMultipliers[13], locNeck -> locationDamageMultipliers[3], locNone -> locationDamageMultipliers[0], locRightArmLower -> locationDamageMultipliers[8], locRightArmUpper -> locationDamageMultipliers[6], locRightFoot -> locationDamageMultipliers[16], locRightHand -> locationDamageMultipliers[10], locRightLegLower -> locationDamageMultipliers[14], locRightLegUpper -> locationDamageMultipliers[12], locTorsoLower -> locationDamageMultipliers[5], locTorsoUpper -> locationDamageMultipliers[4], weapon_name -> szInternalName
* drop externs (26): g_080f2de8, g_081869e0, g_081869f0, g_08186a60, g_08187b88, g_08187b94, g_08187d0c, g_08627940, g_08627f44, g_08627f60, g_08628178, g_08628180, g_08628580, g_086289a4, g_086289c0, g_0864c380, g_0864c56c, g_08716400, g_087a2428, g_087a24cc, g_08853272, g_08853282, g_08853284, g_08853286, g_08853288, g_0885328a
* note: gentity_s.owner_num (+0xf0) local type int32 vs canonical r.linked byte: check every access
* note: gentity_s.box_mode (+0x18c) local type int32 vs canonical parent gentity_s *: check every access
* note: gentity_s.box_a (+0x1ec) does not start on a canonical field (u.item+68): split/retype access
* note: gentity_s.box_b (+0x1f8) does not start on a canonical field (u.item+80): split/retype access
* note: playerstate_s: local size 0x5b8 vs canonical playerState_s 0x26a8
* note: playerstate_s.yaw_byte (+0x8) local type byte vs canonical bobCycle int: check every access
* note: trace_s: byte 0x22 is allsolid and 0x23 startsolid; +0x04 is the plane normal, not endpos.
* note: trace_s.plane_dist_or_entity (+0x1c) local type float vs canonical entityNum uint16: check every access

### chunk 046

* delete: `dvar_s`, `gclient_s`, `gentity_s`, `level_locals_s`, `playerState_s`, `pmove_s`, `usercmd_s`, `weaponDef_s`
* rename `dvar_s`: u -> current
* rename `gclient_s`: attachModelIndex -> cs.attachModelIndex, attachTagIndex -> cs.attachTagIndex, buttonsLatched -> buttonsSinceLastFrame, buttonsPressed -> latched_buttons, cleanName -> cs.name, clientNum -> cs.clientNum, field_26b0 -> killCamEntity, field_273c -> viewmodelIndex, field_27ac -> noclip, field_27b0 -> ufo, field_27b4 -> bFrozen, field_27d8 -> fGunPitch, field_27dc -> fGunYaw, field_2800 -> inactivityTime, field_2810 -> currentAimSpreadScale, field_281c -> unk_0x281c, field_2830 -> useHoldEntity, field_283c -> pingTime, field_2840 -> recoilStamp, field_2844 -> recoilPitchOut, field_2848 -> recoilPitchNeg, field_284c -> viewVec284c, field_2858 -> viewVec2858, field_2864 -> viewVec2864, field_2870 -> viewVec2870, field_287c -> viewInt287c, field_2880 -> viewVec2880, field_288c -> viewVec288c, field_2898 -> viewInt2898, followClient -> lastFollowedClient, isBot -> localClient, lastSayTime -> lastVoiceTime, lastThinkTime -> lastCmdTime, maxHealthSetting -> maxHealth, modelIndex -> cs.modelindex, name -> newnetname, oldButtons -> oldbuttons, predictItems -> predictItemPickup, spectateMask -> noSpectate, targetIndex -> useButtonDone, team -> cs.team
* rename `gentity_s`: absmax -> r.absmax, absmin -> r.absmin, aposBase -> s.apos.trBase, aposType -> s.apos.trType, attachModels -> attachModelNames[0], attachTags -> attachTagNames[0], clientNum -> s.clientNum, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, eFlags -> s.eFlags, eType -> s.eType, event -> s.eventParm, eventParms -> s.eventParms, eventSequence -> s.eventSequence, events -> s.events, field_154 -> r.eventTime, field_161 -> takedamage, field_162 -> active, field_164 -> model, field_165 -> attachIgnoreCollision, field_166 -> handler, field_168 -> classname, field_174 -> flags, field_178 -> eventTime, field_18c -> parent, field_190 -> nextthink, field_208 -> tagInfo, field_6c -> s.angles2[1], field_74 -> s.otherEntityNum, field_7c -> s.groundEntityNum, field_c8 -> s.weapon, field_cc -> s.legsAnim, field_d0 -> s.torsoAnim, field_d4 -> s.leanf, field_e0 -> s.animMovetype, field_e4 -> s.fTorsoHeight, field_e8 -> s.fTorsoPitch, field_ec -> s.fWaistPitch, inuse -> r.inuse, linked -> r.linked, maxs -> r.maxs, mins -> r.mins, number -> s.number, ownerNum -> r.ownerNum, posBase -> s.pos.trBase, posDelta -> s.pos.trDelta, posDuration -> s.pos.trDuration, posTime -> s.pos.trTime, posType -> s.pos.trType, svFlags -> r.svFlags
* rename `level_locals_s`: field_1c -> initializing, field_20 -> clientIsSpawning, field_200 -> teamScores[0], field_204 -> teamScores[1], field_214 -> manualNameChange, numSortedClients -> numConnectedClients
* rename `playerState_s`: eventConsumedSeq -> entityEventSequence, eventSequenceSent -> oldEventSequence, field_100 -> viewHeightLerpTarget, field_104 -> viewHeightLerpDown, field_108 -> viewHeightLerpPosAdj, field_4c -> leanf, field_5a8 -> fTorsoHeight, field_5ac -> fTorsoPitch, field_5b0 -> fWaistPitch, field_5d4 -> aimSpreadScale, field_5dc -> shellshockTime, field_5e0 -> shellshockDuration, field_60 -> groundEntityNum, field_7a4 -> deltaTime, field_7c -> legsAnim, field_84 -> torsoAnim, field_9c -> movementDir, field_dc -> fWeaponPosFrac, field_e4 -> viewmodelIndex, field_f4 -> viewHeightTarget, field_fc -> viewHeightLerpTime, health -> stats[0], linkedEntityNum -> viewlocked_entNum, maxHealth -> stats[2], spawnCount -> stats[5], targetInfo -> iCompassFriendInfo, viewAnglePitch -> proneTorsoPitch, viewAngleYaw -> proneDirection, viewHeight -> viewHeightCurrent, weaponSlotA -> weaponslots[1], weaponSlotB -> weaponslots[2]
* rename `pmove_s`: eventOrigin -> mantleEndPos, eventTime -> mantleDuration, field_e4 -> handler, spawnEvent -> mantleStarted
* rename `usercmd_s`: field_08 -> weapon, field_18 -> forwardmove
* rename `weaponDef_s`: field_278 -> adsOverlayReticle, field_300 -> swayShellShockScale
* drop externs (17): dvar_bg_bobMax, g_0815486c, g_08187ba0, g_08187bac, g_0860b420, g_08628ae0, g_0864e158, g_0864e15c, g_0864f9c0, g_087a2410, g_087a2428, g_08853252, g_0885328c, g_088532ba, g_clientInfos, g_playerFields, g_playerMethods
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: gclient_s.archiveTime (+0x26b4) local type int vs canonical archiveTime float: check every access
* note: gclient_s.field_2844 (+0x2844) local type int vs canonical recoilPitchOut float: check every access
* note: gclient_s.field_2848 (+0x2848) local type int vs canonical recoilPitchNeg float: check every access
* note: gentity_s.field_d4 (+0xd4) local type int vs canonical s.leanf float: check every access
* note: gentity_s.field_168 (+0x168) local type int vs canonical classname uint16: check every access
* note: gentity_s.field_208 (+0x208) local type int vs canonical tagInfo struct game_attach_node_s *: check every access
* note: level_locals_s: local size 0x31c vs canonical level_locals_s 0x3624
* note: playerState_s.field_4c (+0x4c) local type int vs canonical leanf float: check every access
* note: playerState_s.field_60 (+0x60) local type uint16 vs canonical groundEntityNum int: check every access
* note: playerState_s.field_e4 (+0xe4) local type float vs canonical viewmodelIndex int: check every access
* note: playerState_s.field_108 (+0x108) local type int vs canonical viewHeightLerpPosAdj float: check every access
* note: playerState_s.field_5d4 (+0x5d4) local type int vs canonical aimSpreadScale float: check every access
* note: usercmd_s.field_08 (+0x8) local type int vs canonical weapon byte: check every access
* note: usercmd_s.field_18 (+0x18) local type int vs canonical forwardmove signed char: check every access
* note: weaponDef_s: local size 0x304 vs canonical weaponDef_s 0x604

### chunk 047

* delete: `dvar_s`, `gclient_s`, `gentity_s`, `hudelem_s`, `trace_result_s`, `weaponDef_s`
* replace: `hudelem_s` -> `game_hudelem_s`, `trace_result_s` -> `trace_s`
* rename `gclient_s`: ammo -> ps.ammo, ammoClip -> ps.ammoclip, clientNum -> ps.clientNum, currentWeapon -> ps.weapon, eFlags -> ps.eFlags, field_12c -> ps.stats[0], field_26b4 -> archiveTime, field_3c -> ps.grenadeTimeLeft, field_590 -> ps.viewlocked, field_594 -> ps.viewlocked_entNum, field_5d8 -> ps.shellshockIndex, field_d0 -> ps.offHandIndex, field_dc -> ps.fWeaponPosFrac, killerYaw -> ps.stats[1], name -> cs.name, origin -> ps.origin, pm_flags -> ps.pm_flags, pm_type -> ps.pm_type, team -> cs.team, viewHeight -> ps.viewHeightCurrent, viewangles -> ps.viewangles
* rename `gentity_s`: absmax -> r.absmax, absmin -> r.absmin, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, eType -> s.eType, field_118 -> r.maxs[2], field_74 -> s.otherEntityNum, field_84 -> s.loopSound, field_c8 -> s.weapon, field_f1 -> r.bmodel, field_fc -> r.inuse, handlerIndex -> handler, number -> s.number, ownerNum -> r.ownerNum
* rename `hudelem_s`: field_04 -> elem.x, field_08 -> elem.y, field_0c -> elem.z, field_10 -> elem.fontScale, field_14 -> elem.font, field_18 -> elem.alignOrg, field_1c -> elem.alignScreen, field_20 -> elem.color, field_24 -> elem.fromColor, field_28 -> elem.fadeStartTime, field_2c -> elem.fadeTime, field_30 -> elem.label, field_34 -> elem.width, field_38 -> elem.height, field_3c -> elem.materialIndex, field_40 -> elem.fromWidth, field_44 -> elem.fromHeight, field_48 -> elem.scaleStartTime, field_4c -> elem.scaleTime, field_50 -> elem.fromX, field_54 -> elem.fromY, field_58 -> elem.fromAlignOrg, field_5c -> elem.fromAlignScreen, field_68 -> elem.time, field_6c -> elem.duration, field_70 -> elem.value, field_74 -> elem.text, field_78 -> elem.sort, field_7c -> elem.foreground, field_84 -> team, field_88 -> archived, inUse -> elem.type, ownerKey -> clientNum
* rename `weaponDef_s`: field_78 -> weaponType, hitLocMultiplier -> locationDamageMultipliers
* drop externs (18): dvar_g_allowVote, g_0814cfc8, g_0860b420, g_0864f974, g_0864f9c0, g_087a240c, g_entityHandlers, g_hitLocIds, g_hitLocMultiplier, g_hitLocNames, g_hudelems, g_modNames, level_maxclients, level_pendingVoteExecute, level_voteDisplayString, level_voteNo, level_voteTime, level_voteYes
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: gclient_s.field_dc (+0xdc) local type int vs canonical ps.fWeaponPosFrac float: check every access
* note: gclient_s.field_26b4 (+0x26b4) local type int vs canonical archiveTime float: check every access
* note: hudelem_s.field_04 (+0x4) local type int vs canonical elem.x float: check every access
* note: hudelem_s.field_08 (+0x8) local type int vs canonical elem.y float: check every access
* note: hudelem_s.field_0c (+0xc) local type int vs canonical elem.z float: check every access
* note: hudelem_s.field_50 (+0x50) local type int vs canonical elem.fromX float: check every access
* note: hudelem_s.field_54 (+0x54) local type int vs canonical elem.fromY float: check every access
* note: hudelem_s.field_70 (+0x70) local type int vs canonical elem.value float: check every access
* note: hudelem_s.field_78 (+0x78) local type int vs canonical elem.sort float: check every access
* note: trace_result_s: local size 0x30 vs canonical trace_s 0x24
* note: weaponDef_s: local size 0x5fc is a lower bound; canonical size 0x604 (Hunk_Alloc 0x604 at 0x080f33a9).
* note: weaponDef_s: local size 0x5fc vs canonical weaponDef_s 0x604

### chunk 048

* delete: `gclient_s`, `gentity_s`, `hudelem_s`, `hudelem_snapshot_s`, `trace_result_s`, `weapon_info_s`
* replace: `hudelem_s` -> `game_hudelem_s`, `hudelem_snapshot_s` -> `hudelem_s`, `itemdef_s` -> `gitem_s`, `trace_result_s` -> `trace_s`, `weapon_info_s` -> `weaponDef_s`
* rename `gclient_s`: ammoClip -> ps.ammo, ammoReserve -> ps.ammoclip, curWeapon -> ps.weapon, hudListFlag -> ps.hudArchival, hudListNoFlag -> ps.hudCurrent, maxHealth -> ps.stats[2], name -> cs.name, syncedHealth -> ps.stats[0], weaponSlot -> ps.weaponslots[0]
* rename `gentity_s`: angles -> r.currentAngles, clipAmmo -> count, flags11c -> r.contents, flags174 -> flags, inuse -> r.inuse, itemIndex -> s.index, kind -> s.eType, maxs -> r.maxs, mins -> r.mins, nextThink -> nextthink, number_00 -> s.number, origin -> r.currentOrigin, ownerNum -> s.clientNum, reserveAmmo -> u.item.ammoCount, spawnTime -> s.pos.trTime, state -> s.pos.trType, targetClient -> s.groundEntityNum, time_34 -> s.apos.trTime, touchPending -> active, traceMask -> clipmask, vec_18 -> s.pos.trBase, vec_24 -> s.pos.trDelta, vec_48 -> s.apos.trDelta, weaponDef -> u.item.index
* rename `hudelem_s`: duration -> elem.duration, endTime -> elem.time, height -> elem.height, listSelector -> archived, shader -> elem.materialIndex, targetClient -> clientNum, type -> elem.type, viewerClient -> team, width -> elem.width
* rename `hudelem_snapshot_s`: words -> type
* rename `trace_result_s`: endpos -> normal
* rename `weapon_info_s`: altMode -> offhandClass, ammoRangeA -> dropAmmoMin, ammoRangeB -> dropAmmoMax, name -> szInternalName, slotGroup -> weaponSlot
* drop externs (47): dvar_g_allowVote, g_08159ee0, g_0815a180, g_081850c0, g_0864bf80, g_0864c384, g_0864c390, g_0864c394, g_0864c39c, g_0864d6c8, g_0864e0d4, g_0864e0d8, g_0864e15c, g_0864f97c, g_0864f980, g_08703588, g_08703a60, g_08716400, g_087a2400, g_087a2408, g_087a240c, g_087a2410, g_087a2418, g_087a2420, g_087a2424, g_087a2428, g_087a2434, g_087a2438, g_087a2448, g_087a244c, g_087a2450, g_087a246c, g_087a2470, g_087a2480, g_087a24b0, g_087a24b8, g_087a24bc, g_087a24c0, g_087a24c4, g_087a24c8, g_087a24cc, g_087a24d4, g_087a24d8, g_0884e7b8, g_08853274, g_hudelems, level_maxclients
* note: gclient_s.weaponOwned (+0x544, unsigned char[16]) is a byte blob spanning canonical fields starting at ps.weapons: replace with field accesses
* note: weapon_info_s: local size 0x378 vs canonical weaponDef_s 0x604

### chunk 049

* delete: `dvar_s`, `gclient_s`, `gentity_s`, `trace_s`, `weaponinfo_s`
* replace: `turret_mode_s` -> `entity_handler_s`, `turret_user_s` -> `clientInfo_s`, `weaponinfo_s` -> `weaponDef_s`
* rename `dvar_s`: value -> current
* rename `gclient_s`: eFlags -> ps.eFlags, field004 -> ps.commandTime, field03c -> ps.grenadeTimeLeft, flags27bc -> buttons, origin -> ps.origin, recoil -> ps.viewHeightCurrent, secondaryScore -> deaths, sessionTeam -> cs.team, sortState -> connected, turretAimState -> ps.viewlocked, turretEntityNum -> ps.viewlocked_entNum, turretEntries -> ps.objective, usingTurretIndex -> ps.groundEntityNum, viewAngles -> ps.viewangles[0]
* rename `gentity_s`: aBase -> s.apos.trBase, aTrDuration -> s.apos.trDuration, aTrTime -> s.apos.trTime, aTrType -> s.apos.trType, aimAngles -> s.angles2[0], aimDelta -> s.angles2[2], aimTarget -> r.ownerNum, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, driverRef -> tagInfo, eFlags -> s.eFlags, eType -> s.eType, field058 -> s.time2, field074 -> s.otherEntityNum, field078 -> s.attackerEntityNum, field07c -> s.groundEntityNum, field080 -> s.constantLight, field154 -> r.eventTime, field178 -> eventTime, field17c -> freeAfterEvent, field180 -> unlinkAfterEvent, field184 -> clipmask, field18c -> parent, field198 -> maxHealth, inUse -> r.inuse, lastFrame -> framenum, linked -> r.linked, maxs -> r.maxs, mins -> r.mins, modelIndex -> classname, nextThink -> nextthink, number -> s.number, occupied -> physicsObject, pos -> s.pos, rateMs -> u.turretRateMs, spawnFlags -> flags, svFlags -> r.svFlags, turret -> pTurretInfo, turretAi -> takedamage, turretOutput84 -> s.loopSound, turretPending -> active, turretSeat88 -> s.surfType, turretSlot -> s.clientNum, turretType -> handler, weapon -> s.weapon
* rename `trace_s`: endpos -> normal, flagAt23 -> startsolid
* rename `weaponinfo_s`: fireInterval -> fireTime, fireModeFlag -> weaponType, pitchLimit -> maxHorTurnSpeed, projSpeed -> projectileSpeed, speedScale -> animHorRotateInc, spreadDeg -> destabilizationAngleMax, tagName0 -> loopFireSound, tagName1 -> loopFireSoundPlayer, tagName2 -> stopFireSound, tagName3 -> stopFireSoundPlayer, timeScale -> destabilizationTimeReductionRatio, timeScaleMs -> destabilizationBaseTime, turretAimParam -> stance, yawLimit -> maxVertTurnSpeed
* drop externs (55): g_0814cfc8, g_08187c0c, g_08187d00, g_0860b420, g_0864c380, g_0864c384, g_0864c38c, g_0864c390, g_0864c394, g_0864c398, g_0864c39c, g_0864c3a4, g_0864c568, g_0864c56c, g_0864c570, g_0864c574, g_0864c578, g_0864c580, g_0864c584, g_0864c590, g_0864c59c, g_0864c69c, g_0864d6c8, g_0864e0d4, g_0864e168, g_0864ed68, g_0864f968, g_0864f97c, g_0864f980, g_0864f984, g_0864f988, g_0864f98c, g_0864f990, g_0864f9c0, g_08703574, g_08716400, g_087a2408, g_087a246c, g_087a2470, g_087a2480, g_087a24d0, g_087a24d4, g_087a24d8, g_087a2500, g_08844e00, g_0884e7bc, g_08853274, g_088532ac, g_088532b8, g_clientInfos, level_maxclients, level_pendingVoteExecute, level_voteNo, level_voteTime, level_voteYes
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: gentity_s.field18c (+0x18c) local type int vs canonical parent gentity_s *: check every access
* note: weaponinfo_s: local size 0x5b0 vs canonical weaponDef_s 0x604
* note: weaponinfo_s.leftArc (+0x528) local type int vs canonical leftArc float: check every access
* note: weaponinfo_s.rightArc (+0x52c) local type int vs canonical rightArc float: check every access
* note: weaponinfo_s.topArc (+0x530) local type int vs canonical topArc float: check every access
* note: weaponinfo_s.bottomArc (+0x534) local type int vs canonical bottomArc float: check every access
* note: weaponinfo_s.playerSpread (+0x540) local type int vs canonical playerSpread float: check every access

### chunk 050

* delete: `cod2_gclient_s`, `cod2_gentity_s`, `cod2_objective_s`, `dvar_s`, `trace_s`, `weaponDef_s`
* replace: `cod2_entclass_s` -> `entity_handler_s`, `cod2_gclient_s` -> `gclient_s`, `cod2_gentity_s` -> `gentity_s`, `cod2_objective_s` -> `objective_s`
* rename `cod2_gclient_s`: deltaAnglesYaw -> ps.delta_angles[1], name -> cs.name[0], origin -> ps.origin
* rename `cod2_gentity_s`: absmax -> r.absmax, absmin -> r.absmin, apos -> s.apos, clipMask -> clipmask, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, eFlags -> s.eFlags, eType -> s.eType, entClass -> handler, eventParm -> s.eventParm, expireTime -> nextthink, groundEntityNum -> s.groundEntityNum, maxs -> r.maxs, mins -> r.mins, number -> s.number, ownerNum -> r.ownerNum, pos -> s.pos, weaponIndex -> s.weapon
* rename `dvar_s`: value -> current
* rename `trace_s`: allsolid -> startsolid, startsolid -> allsolid
* drop externs (26): g_08187d00, g_08188020, g_08188040, g_08188720, g_0864c3a4, g_0864c570, g_087a24b0, g_08845680, g_0884d680, g_0884d700, g_0884d708, g_0884d728, g_0884d72c, g_0884e7ac, g_08853226, g_08853230, g_08853232, g_08853236, g_0885323a, g_0885323e, g_0885324c, g_08853254, g_0885325e, g_0885326c, g_08853294, g_loc_warningsAsErrors
* note: cod2_gclient_s: local size 0x2788 vs canonical gclient_s 0x28a4
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: trace_s: byte 0x22 is allsolid and 0x23 startsolid.
* note: trace_s.material (+0x18) local type int vs canonical material struct dMaterial_s *: check every access
* note: weaponDef_s: local size 0x5b0 vs canonical weaponDef_s 0x604

### chunk 051

* delete: `WeaponDefHead_s`, `client_s`, `dvar_s`, `gentity_s`
* replace: `WeaponDefHead_s` -> `weaponDef_s`, `client_s` -> `gclient_s`, `namedEntry_s` -> `cod2_gametype_s`
* rename `WeaponDefHead_s`: name -> szInternalName
* rename `client_s`: name -> cs.name, state -> connected
* rename `dvar_s`: u -> current
* rename `gentity_s`: apos -> s.apos, clientData -> client, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, dmg -> damage, eType -> s.eType, inuse -> r.inuse, modelIndex -> s.index, modelName -> model, moveAccelEnd -> u.mover.moveAccelEnd, moveCruiseEnd -> u.mover.moveCruiseEnd, moveCruiseTime -> u.mover.moveCruiseTime, moveDecelTime -> u.mover.moveDecelTime, moveDest -> u.mover.moveDest, moveSpeed -> u.mover.moveSpeed, moverFlag166 -> handler, number -> s.number, packedLight -> s.constantLight, pos -> s.pos, rotAccelEnd -> u.mover.rotAccelEnd, rotCruiseEnd -> u.mover.rotCruiseEnd, rotCruiseTime -> u.mover.rotCruiseTime, rotDecelTime -> u.mover.rotDecelTime, rotDest -> u.mover.rotDest, rotSpeed -> u.mover.rotSpeed, scrBytes -> attachModelNames, scrStrings -> attachTagNames, svFlags -> r.svFlags
* drop externs (20): g_0815e3a0, g_0815e820, g_08188a00, g_0864c578, g_0864d6c8, g_087a240c, g_0884d708, g_0884d728, g_0884d72c, g_08850e00, g_08853248, g_08853260, g_08853262, g_08853264, g_08853266, g_08853280, g_0885329e, g_088532a0, g_modNames, level_maxclients
* note: WeaponDefHead_s: local size 0x4 vs canonical weaponDef_s 0x604
* note: client_s: game-side client: rename to gclient_s; the server client_s is 0xb1064 bytes.
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24

### chunk 052

* delete: `game_client_s`, `game_entity_s`, `game_trace_result_s`, `game_traj_s`, `game_weapon_s`
* replace: `game_client_s` -> `gclient_s`, `game_entity_s` -> `gentity_s`, `game_orient_s` -> `Orient`, `game_trace_result_s` -> `trace_s`, `game_traj_s` -> `trajectory_s`, `game_weapon_s` -> `weaponDef_s`
* rename `game_client_s`: clientNum -> ps.clientNum, eventParms -> ps.eventParms, eventSequence -> ps.eventSequence, events -> ps.events, flags_0c -> ps.pm_flags, hitEntityNum -> ps.stats[3], hitValue -> ps.stats[4], lastTarget_2830 -> useHoldEntity, lastTarget_5a0 -> ps.cursorHintEntIndex, origin -> ps.viewangles, ptr_282c -> pLookatEnt, sessionState -> connected, team -> cs.team, viewheight -> ps.viewHeightCurrent
* rename `game_entity_s`: active17c -> freeAfterEvent, angles -> r.currentAngles, apos -> s.apos, attachChildren -> tagChildren, attachFlags -> attachIgnoreCollision, attachLink -> tagInfo, attachModel -> attachModelNames, attachName -> attachTagNames, contents -> r.contents, cooldown -> health, count1ac -> u.item.index, effectFlags -> spawnflags, eventParms -> s.eventParms, eventSequence -> s.eventSequence, events -> s.events, field88 -> s.surfType, flag162 -> active, flags -> s.eFlags, flags174 -> flags, freeNext -> nextFree, generation -> spawnSerial, inUse -> r.inuse, kind -> handler, limit1a8 -> u.item.ammoCount, link_f2 -> r.svFlags, lockedBy -> parent, maxs -> r.maxs, mins -> r.mins, modelCopy -> s.index, modelIndex -> model, number -> s.number, origin -> r.currentOrigin, ownerSlot -> s.weapon, parentNum -> r.ownerNum, pos -> s.pos, ptr15c -> pTurretInfo, rnd_a0 -> s.eventParm, rnd_d8 -> s.scale, scrName -> classname, srcEntNum -> s.otherEntityNum, takeDamage -> takedamage, targetNum -> s.groundEntityNum, time154 -> r.eventTime, time178 -> eventTime, type -> s.eType
* rename `game_trace_result_s`: flags_10 -> surfaceFlags, flags_14 -> contents, hitType -> partGroup
* rename `game_weapon_s`: altMode_31c -> rifleBullet, class_7c -> weaponClass, damageFar_590 -> minDamage, damage_1ec -> damage, falloffFar_59c -> minDamageRange, falloffNear_598 -> maxDamageRange, friendlyFire_320 -> armorPiercing
* drop externs (36): cv_sv_fps, g_0814cfc8, g_0815eae0, g_0815eae1, g_0815ec20, g_0815ec7f, g_0815ece3, g_0815ed01, g_0815ed20, g_0815ed24, g_08187c0c, g_08187c1f, g_08187d1c, g_0864c384, g_0864c390, g_0864c394, g_0864c39c, g_0864c574, g_0864c58c, g_0864d6c8, g_0864e0d8, g_0864e164, g_0864e168, g_0864f968, g_08716400, g_087a2408, g_087a24cc, g_0884e7ac, g_0884e7bc, g_08852e20, g_0885324a, g_08853252, g_08853270, g_08853274, g_0885327a, level_maxclients
* note: game_entity_s.count1ac (+0x1ac) local type int32 vs canonical u.item.index uint16: check every access
* note: game_entity_s.freeNext (+0x22c) local type int32 vs canonical nextFree gentity_s *: check every access
* note: game_weapon_s: local size 0x5a0 vs canonical weaponDef_s 0x604

### chunk 053

* delete: `cvar_ref_s`, `gameobj_s`, `gclient_s`, `gentity_s`, `trace_s`
* replace: `cvar_ref_s` -> `dvar_s`, `gameobj_s` -> `weaponDef_s`, `impact_entry_s` -> `entity_handler_s`
* rename `cvar_ref_s`: value -> current
* rename `gameobj_s`: aimBaseAngle -> adsSpread, field31c -> rifleBullet, field574 -> unk_0x574, field578 -> unk_0x578, fireType -> weaponType, launchLiftInt -> projectileSpeedUp, launchSpeedInt -> projectileSpeed, linkChildren -> offhandClass, missileKind -> fuseTime, name -> szInternalName, nextLink -> altWeaponIndex, objSubType -> weaponSlot, objType -> weaponClass, pelletCount -> shotCount, pelletSpreadBits -> minDamageRange, rangeAtFire -> enemyCrosshairRange, str56c -> useHintString, str570 -> dropHintString
* rename `gclient_s`: aimPitch -> fGunPitch, aimYaw -> fGunYaw, altHandle -> ps.weapon, buttonsEvents -> latched_buttons, buttonsLast -> buttons, buttonsNow -> oldbuttons, buttonsPrev -> ps.eFlags, clientState -> ps.weaponstate, curHandle -> ps.offHandIndex, flags -> ps.pm_flags, lerpFraction -> currentAimSpreadScale, lerpOverride -> ps.fWeaponPosFrac, lookAtEnt -> pLookatEnt, msgChannel -> ps.clientNum, origin -> ps.origin, sessionType -> ps.pm_type, slotA -> ps.weaponslots[1], slotB -> ps.weaponslots[2], teamNum -> cs.team, useBoundsMax -> ps.maxs, useBoundsMin -> ps.mins, useEntityNum -> useHoldEntity, useStartTime -> useHoldTime, useTargetHandle -> ps.cursorHint, useTargetNum -> ps.cursorHintEntIndex, useTargetSlot -> ps.cursorHintString, velocity -> ps.velocity, viewBase -> ps.viewangles, weaponEntityNum -> ps.viewlocked_entNum
* rename `gentity_s`: absmax -> r.absmax, absmin -> r.absmin, contents -> r.contents, currentOrigin -> r.currentOrigin, eType -> s.eType, impactType -> handler, isPlayer -> takedamage, number -> s.number, ownerNum -> u.item+12, teamNum -> team, trBase -> s.pos.trDelta, useSlotCount -> health, useState -> active, weaponIndex -> u.item.index
* rename `trace_s`: endpos -> normal
* drop externs (108): bg_numWeapons, g_0814cfc8, g_081850dc, g_08187c0c, g_08187c1f, g_08187d0c, g_08188ac0, g_0864c384, g_0864c39c, g_0864c56c, g_08716400, g_087a2420, g_087a2424, g_087a24b8, g_087a24bc, g_087a24c0, g_087a24c4, g_087a24c8, g_08853220, g_08853222, g_08853224, g_08853226, g_08853228, g_0885322a, g_0885322c, g_0885322e, g_08853230, g_08853232, g_08853234, g_08853236, g_08853238, g_0885323a, g_0885323c, g_0885323e, g_08853240, g_08853242, g_08853244, g_08853246, g_08853248, g_0885324a, g_0885324c, g_0885324e, g_08853250, g_08853252, g_08853254, g_08853256, g_08853258, g_0885325a, g_0885325c, g_0885325e, g_08853260, g_08853262, g_08853264, g_08853266, g_08853268, g_0885326a, g_0885326c, g_0885326e, g_08853270, g_08853272, g_08853274, g_08853276, g_08853278, g_0885327a, g_0885327c, g_0885327e, g_08853280, g_08853282, g_08853284, g_08853286, g_08853288, g_0885328a, g_0885328c, g_0885328e, g_08853290, g_08853292, g_08853294, g_08853296, g_08853298, g_0885329a, g_0885329c, g_0885329e, g_088532a0, g_088532a2, g_088532a4, g_088532a6, g_088532a8, g_088532aa, g_088532ac, g_088532ae, g_088532b0, g_088532b2, g_088532b4, g_088532b6, g_088532b8, g_088532ba, g_088532bc, g_088532be, g_088532c0, g_088532c2, g_088532c4, g_088532c6, g_088532c8, g_088532ca, g_088532cc, g_088532ce, g_088532d0, g_088532d2
* note: cvar_ref_s: local size 0xc vs canonical dvar_s 0x24
* note: gameobj_s: local size 0x5a0 vs canonical weaponDef_s 0x604
* note: gameobj_s.pelletSpreadBits (+0x59c) local type uint32 vs canonical minDamageRange float: check every access
* note: gentity_s.ownerNum (+0x1b4) does not start on a canonical field (u.item+12): split/retype access
* note: trace_s: local size 0x20 vs canonical trace_s 0x24

### chunk 054

* replace: `FxDrawEntry` -> `FxBlocker`, `FxParticleTemplate` -> `fx_primitive_s`
* drop externs (16): g_0814cfc8, g_08188ac0, g_08853380, g_08a53380, g_08a53384, g_08a53804, g_08a53cb0, g_08a53d00, vtbl_Cloud, vtbl_Cylinder, vtbl_Effect, vtbl_Emitter, vtbl_Line, vtbl_OrientedParticle, vtbl_Particle, vtbl_Tail

### chunk 055

* replace: `FxObj29c` -> `Emitter`, `FxObjFc` -> `Light`, `FxPrimTemplate` -> `fx_primitive_s`
* drop externs (14): g_08188ac0, g_08a53388, g_08a53390, g_08a53398, g_08a533a0, g_08a533a8, g_08a533b0, g_08a533b8, g_08a533c0, g_08a533c8, g_08a533d0, g_08a53c80, g_08a53c98, vtbl_0815f348

### chunk 056

* replace: `fx_channel_s` -> `FxChannelInit`, `fx_curve_s` -> `FxCurveSrc`, `fx_parse_node_s` -> `ParseNode`, `fx_playback_list_s` -> `FxScheduler`, `fx_playback_s` -> `FxScheduledEffect`, `fx_pool_block_s` -> `FxPoolBlock`, `fx_pool_s` -> `FxPool`, `fx_template_s` -> `FxEffectTemplate`
* drop externs (6): g_fxMissingTemplate, g_fxPoolLarge, g_fxPoolSmall, g_fxRequireMaterials, g_fxTemplateCount, g_fxTemplates

### chunk 057

* replace: `FxChannelSlot` -> `FxCurveSrc`, `PrimitiveTemplate` -> `fx_primitive_s`
* drop externs (8): g_0815fae0, g_0815fcc0, g_08188ac0, g_08a533f0, g_08a53804, g_08a53818, g_08a53840, g_fxRequireMaterials

### chunk 058

* replace: `ConfigBlock` -> `ParseNode`, `FxEffect` -> `Effect`, `FxElemDef` -> `fx_primitive_s`, `FxPtrTable` -> `FxEffectTemplate`
* drop externs (16): g_0814cfc8, g_08188ab4, g_08188ab8, g_08188abc, g_08188ac0, g_08a533f0, g_08a53804, g_08a53818, g_08a53ca0, g_08a53ca8, g_08a53cb0, g_08a53d00, g_08a5c9ac, g_08a5c9b0, g_08a5c9c4, g_fxRequireMaterials

### chunk 059

* replace: `fx_def_s` -> `fx_primitive_s`, `fx_pool_block_s` -> `FxPoolBlock`, `fx_pool_s` -> `FxPool`, `loc_lang_s` -> `cod2_lang_entry_s`
* drop externs (22): fs_ignoreLocalized, g_08188ab8, g_08188abc, g_08188ac0, g_08a53398, g_08a533a0, g_08a533a8, g_08a533b0, g_08a533b8, g_08a533c0, g_08a533c8, g_08a533d0, g_08a53804, g_08a53c80, g_loc_forceEnglish, g_loc_isDoubleByteLanguage, g_loc_language, g_loc_translate, g_loc_unlocalizedBuffer, g_loc_warnings, g_loc_warningsAsErrors, vtbl_Flash

### chunk 060

* drop externs (8): g_08188ae0, g_08a6f180, g_08a6f1a0, g_08a6f1c0, g_08a6f200, g_08a6f240, g_08a6f280, g_loc_isDoubleByteLanguage

### chunk 061

* drop externs (4): g_0817d00c, g_0817d3a4, g_08188b60, g_08a6f2c0

### chunk 100

* delete: `cmd_function_s`, `dvar_s`, `dvar_value_u`, `msg_s`, `searchpath_s`
* replace: `cm_map_s` -> `clipMap_s`, `dvar_value_u` -> `DvarValue`
* rename `cmd_function_s`: aux8 -> autoCompleteDir, auxC -> autoCompleteExt
* rename `dvar_s`: domainHi -> domain.enumeration.strings, domainLo -> domain.enumeration.stringCount
* rename `dvar_value_u`: bits -> enabled
* rename `searchpath_s`: pack -> iwd
* drop externs (18): cmd_functions, com_lastFrameTime, dvar_indexCount, dvar_vars, fs_searchpaths, g_0814cfc8, g_08188c84, g_08188ca0_sourceFiles, g_08188dc0_clipmap, g_08188ef8, g_081a5080, g_081d7900, g_081e9544, g_0849fd98, g_084a64a0, g_084a64c0, g_084a74c0, g_cmd_wait
* note: dvar_s: local size 0x20 vs canonical dvar_s 0x24
* note: dvar_s.domainHi (+0x18) local type uint32 vs canonical domain.enumeration.strings const char **: check every access
* note: dvar_value_u.bits (+0x0) local type uint32 vs canonical enabled byte: check every access
* note: searchpath_s: local size 0x8 vs canonical searchpath_s 0x14

### chunk 101

* delete: `ScrVarValue`, `ScrVmFrame`, `VariableValue`, `client_s`, `dvar_s`, `fileHandleData_s`, `netadr_s`, `scrVmPub_t`, `trace_s`
* replace: `ScrVarValue` -> `VariableValue`, `lineclip_a_s` -> `SegHitTrace`
* rename `ScrVarValue`: value -> u
* rename `ScrVmFrame`: word0 -> pos, word10 -> startTop, word14 -> callMarkerType, word8 -> localVarCount, wordC -> callMarker
* rename `VariableValue`: value -> u
* rename `client_s`: bFallback -> wwwFallback, bWWWDl -> downloadingWWW, bWWWing -> clientDownloadingWWW, qport -> netchan.qport, remoteAddress -> netchan.remoteAddress
* rename `fileHandleData_s`: baseOffset -> handleSync, handle -> handleFiles, handleSync -> zipFile, zipFile -> iwd
* rename `netadr_s`: port -> ipx[8]
* drop externs (24): com_sv_running, cv_mapname, cv_sv_cheats, cv_sv_maxclients, fs_basepath, fs_gamedir, fs_homepath, fsh, g_08397510, g_08440108, g_08440200, g_0844020c, g_085a9804, g_085a9808, g_085a980c, g_085a9810, g_085a9814, g_085a981c, g_svs_clients, g_svs_time, scrVarPub_levelId, scrVarPub_timeArrayId, scrVmPub, scr_debug_enabled
* note: ScrVmFrame.word0 (+0x0) local type unsigned int vs canonical pos const char *: check every access
* note: ScrVmFrame.wordC (+0xc) local type unsigned int vs canonical callMarker VariableValue *: check every access
* note: ScrVmFrame.word10 (+0x10) local type unsigned int vs canonical startTop VariableValue *: check every access
* note: client_s.muted (+0xb0fe0, unsigned char[65]) is a byte blob spanning canonical fields starting at muteList: replace with field accesses
* note: dvar_s: local size 0xc vs canonical dvar_s 0x24
* note: fileHandleData_s.zipFile (+0x14) local type int vs canonical iwd struct iwd_s *: check every access
* note: netadr_s.ipx (+0x8, unsigned char[10]) is a byte blob spanning canonical fields starting at port: replace with field accesses
* note: netadr_s.port (+0x12) local type unsigned short vs canonical ipx[8] byte: check every access
* note: scrVmPub_t: local size 0x20 vs canonical scrVmPub_t 0x320

### chunk 102

* delete: `client_s`, `dvar_s`, `gentity_s`, `weaponDef_s`
* replace: `client_s` -> `gclient_s`
* rename `client_s`: ammo -> ps.ammo, clip -> ps.ammoclip, configIndex -> killCamEntity, field26b8 -> score, field27a4 -> psOffsetTime, health -> ps.stats[2], maxHealth -> ps.stats[0], name -> cs.name, requestedHealth -> maxHealth, scaledValue -> archiveTime, sessionTeam -> cs.team, stateFlags -> ps.eFlags, weaponBits -> ps.weapons, weaponD0 -> ps.offHandIndex, weaponD4 -> ps.weapon, weaponSlot -> ps.weaponslots[1]
* rename `dvar_s`: enumCount -> domain.enumeration.stringCount, enumNames -> domain.enumeration.strings, value -> current
* rename `gentity_s`: headIconIndex -> s.iHeadIcon, headIconTeam -> s.iHeadIconTeam
* rename `weaponDef_s`: ammoIndex -> iAmmoIndex, field0c -> gunModel, field10 -> handModel, field14 -> unk_0x14, maxAmmo -> startAmmo, name -> szInternalName, weaponClass -> iSharedAmmoCapIndex
* drop externs (42): bg_ammoTypes, bg_clipTypes, bg_numAmmoTypes, bg_numClipTypes, bg_numWeapons, bg_weaponDefs, dvar_vars, eventQue, g_081869e0, g_081869f0, g_081869fc, g_08186a08, g_08186a20, g_08186a48, g_08186a54, g_085b7220, g_085bc814, g_085bc818, g_085bc828, g_085bc900, g_085e4f20, g_085e5044, g_0860b3ec, g_0860b420, g_08627f44, g_08627f60, g_0864c380, g_0864c574, g_0864f9c0, g_08716400, g_08853222, g_08853224, g_08853268, g_0885328e, g_08853292, g_08853294, g_08853296, g_clientInfos, sys_homepath, sys_installpath, sys_packetReceived, ttycon_on
* note: client_s: game-side client: rename to gclient_s.
* note: client_s.scaledValue (+0x26b4) local type int32 vs canonical archiveTime float: check every access
* note: weaponDef_s: local size 0x1e8 vs canonical weaponDef_s 0x604
* note: weaponDef_s.field0c (+0xc) local type int32 vs canonical gunModel const char *: check every access
* note: weaponDef_s.field10 (+0x10) local type int32 vs canonical handModel const char *: check every access

### chunk 103

* delete: `dvar103_s`, `gclient_s`, `gentity_s`, `weapondef103_s`
* replace: `corpse103_s` -> `ent_ext_s`, `dvar103_s` -> `dvar_s`, `entClass103_s` -> `entity_handler_s`, `weapondef103_s` -> `weaponDef_s`
* rename `dvar103_s`: value -> current
* rename `gclient_s`: adsFraction -> ps.fWeaponPosFrac, ammo -> ps.ammo, clientNum -> ps.clientNum, clip -> ps.ammoclip, damageCount -> ps.damageTimer, damageCountCopy -> ps.damageDuration, damageYaw -> ps.flinchYaw, eFlags -> ps.eFlags, field26c4 -> connected, field7c -> ps.legsAnim, field84 -> ps.torsoAnim, freezeControls -> bFrozen, groundEntityNum -> ps.groundEntityNum, health -> ps.stats[0], kickTime -> ps.pm_time, lastTalkTime -> lastVoiceTime, lookAtEnt -> pLookatEnt, oldButtons -> buttonsSinceLastFrame, origin -> ps.origin, spawnWeapon -> ps.weapon, spawnWeaponTime -> ps.weaponstate, stateFlags -> ps.pm_flags, velocity -> ps.velocity, viewAngles -> ps.viewangles[0], viewModelIndex -> viewmodelIndex, weaponSlot -> ps.weaponslots[0]
* rename `gentity_s`: attackerNum -> s.otherEntityNum, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, eFlags -> s.eFlags, eType -> s.eType, entClass -> handler, field160 -> physicsObject, field184 -> clipmask, field1a8 -> u.item.ammoCount, field7c -> s.groundEntityNum, field88 -> s.surfType, field90 -> s.clientNum, field_a0 -> s.eventParm, field_cc -> s.legsAnim, field_d0 -> s.torsoAnim, field_d8 -> s.scale, inUse -> r.inuse, maxs -> r.maxs, mins -> r.mins, nextThink -> nextthink, number -> s.number, spawnFlags -> flags, svFlags -> r.svFlags, trDelta -> s.pos.trDelta, trTime -> s.pos.trTime, trType -> s.pos.trType, vec120 -> r.absmin, vec12c -> r.absmax, visibleMask -> r.clientMask
* rename `weapondef103_s`: ammoIndex -> iAmmoIndex, field31c -> rifleBullet, field78 -> weaponType, name -> szInternalName, slotType -> weaponSlot
* drop externs (11): dvar_player_dmgtimer_maxTime, dvar_player_dmgtimer_timePerPoint, g_08187d00, g_08853294, g_088532a2, g_088532a4, g_088532a6, g_088532a8, g_088532aa, g_088532c6, g_clientInfos
* note: dvar103_s: local size 0xc vs canonical dvar_s 0x24
* note: gclient_s.weaponsOwned (+0x544, uint8[16]) is a byte blob spanning canonical fields starting at ps.weapons: replace with field accesses
* note: weapondef103_s: local size 0x340 vs canonical weaponDef_s 0x604

### chunk 104

* delete: `gclient_s`, `gentity_s`, `hudelem_s`, `trace104_s`, `trajectory104_s`
* replace: `hudelem_s` -> `game_hudelem_s`, `hudfield104_s` -> `hudelem_field_s`, `trace104_s` -> `trace_s`, `trajectory104_s` -> `trajectory_s`
* rename `gclient_s`: aimLimitA -> ps.viewAngleClampBase[0], aimLimitB -> ps.viewAngleClampBase[1], aimLimitC -> ps.viewAngleClampRange[0], aimLimitD -> ps.viewAngleClampRange[1], allowSpectateTeams -> noSpectate, occupantActive -> ps.viewlocked, occupantNum -> ps.viewlocked_entNum, stateFlags -> ps.pm_flags, turretFlags -> ps.eFlags, turretPitch -> ps.viewangles[1], turretYaw -> ps.viewangles[0]
* rename `gentity_s`: aimTarget -> r.ownerNum, attachModel -> attachModelNames, attachTag -> attachTagNames, contents -> r.contents, controlAngles -> s.angles2, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, driverRef -> tagInfo, eFlags -> s.eType, entClass -> handler, field154 -> r.eventTime, field178 -> eventTime, field17c -> freeAfterEvent, field180 -> unlinkAfterEvent, field184 -> clipmask, field188 -> framenum, field18c -> parent, field198 -> maxHealth, field19c -> damage, field_054 -> s.time, field_058 -> s.time2, field_074 -> s.otherEntityNum, field_07c -> s.groundEntityNum, field_088 -> s.surfType, flags08 -> s.eFlags, flags170 -> spawnflags, inUse -> r.inuse, itemIndex -> u.item.index, kindIndex -> s.index, linked -> r.linked, maxs -> r.maxs, mins -> r.mins, nextThink -> nextthink, number -> s.number, occupied -> physicsObject, rateMs -> u.item.ammoCount, spawnFlags -> flags, svFlags -> r.svFlags, trajectory -> s.pos, turret -> pTurretInfo, turretAi -> takedamage, turretPending -> active, vec120 -> r.absmin, vec12c -> r.absmax, weapon -> s.weapon
* rename `hudelem_s`: alignX -> elem.alignOrg, color -> elem.color, fadeDuration -> elem.fadeTime, fadeStart -> elem.fadeStartTime, field30 -> elem.label, field68 -> elem.time, field6c -> elem.duration, font -> elem.font, fontScale -> elem.fontScale, height -> elem.height, horzAlign -> elem.alignScreen, moveDuration -> elem.moveTime, moveOldAlignX -> elem.fromAlignOrg, moveOldHorzAlign -> elem.fromAlignScreen, moveOldX -> elem.fromX, moveOldY -> elem.fromY, moveStart -> elem.moveStartTime, oldHeight -> elem.fromHeight, oldWidth -> elem.fromWidth, prevColor -> elem.fromColor, scaleDuration -> elem.scaleTime, scaleStart -> elem.scaleStartTime, shader -> elem.materialIndex, textHandle -> elem.text, type -> elem.type, value -> elem.value, width -> elem.width, x -> elem.x, y -> elem.y, z -> elem.z
* rename `trace104_s`: startSolid -> startsolid
* drop externs (26): g_0814cfc8, g_08159ee0, g_0815b06e, g_0815b381, g_08187c0c, g_08187c80, g_08187c8c, g_08187c98, g_08187cc0, g_08187ce0, g_08187d00, g_0864bf80, g_0864c39c, g_0864c56c, g_0864f97c, g_0864f980, g_08853220, g_08853222, g_08853224, g_08853268, g_08853294, g_0885329c, g_088532ac, g_088532be, g_088532c0, g_088532c4
* note: gclient_s: local size 0x2744 vs canonical gclient_s 0x28a4
* note: gentity_s.field18c (+0x18c) local type int vs canonical parent gentity_s *: check every access
* note: gentity_s.rateMs (+0x1a8) local type float vs canonical u.item.ammoCount int: check every access
* note: hudelem_s.moveOldX (+0x50) local type int vs canonical elem.fromX float: check every access
* note: hudelem_s.moveOldY (+0x54) local type int vs canonical elem.fromY float: check every access

### chunk 105

* delete: `client105_s`, `ent105_s`, `objective105_s`, `trace105_s`
* replace: `client105_s` -> `gclient_s`, `ent105_s` -> `gentity_s`, `entHandler105_s` -> `entity_handler_s`, `objective105_s` -> `objective_s`, `trace105_s` -> `trace_s`
* rename `client105_s`: sessionState -> ps.pm_type
* rename `ent105_s`: attachModel -> attachModelNames, attachTag -> attachTagNames, bmodel -> r.bmodel, contents -> r.contents, eFlags -> s.eFlags, eType -> s.eType, eventParm -> s.eventParm, field15c -> pTurretInfo, field160 -> physicsObject, field74 -> s.otherEntityNum, field78 -> s.attackerEntityNum, flagsF2 -> r.svFlags, handlerIndex -> handler, hintString -> s.scale, hintType -> s.dmgFlags, loopSound -> s.loopSound, loopSoundEnd -> r.broadcastTime, maxs -> r.maxs, mins -> r.mins, number -> s.number, origin -> r.currentOrigin, visibleMask -> r.clientMask
* rename `objective105_s`: team -> teamNum
* drop externs (19): g_0814cfc8, g_081850c0, g_08187ba0, g_08187bac, g_08187d10, g_08188020, g_0864c39c, g_0864c3a4, g_0864c56c, g_0864c578, g_0864f984, g_08716400, g_08853222, g_08853224, g_08853274, g_08853276, g_08853278, g_0885327a, g_08853294
* note: trace105_s: local size 0x30 vs canonical trace_s 0x24

### chunk 106

* delete: `cod2_client_slot_s`, `cod2_gclient_s`, `cod2_gentity_s`, `trace_s`
* replace: `cod2_client_slot_s` -> `gclient_s`, `cod2_gclient_s` -> `gclient_s`, `cod2_gentity_s` -> `gentity_s`, `cod2_turret_s` -> `turret_s`
* rename `cod2_client_slot_s`: name -> cs.name, pendingName -> newnetname, state -> connected
* rename `cod2_gclient_s`: deltaAnglesYaw -> ps.delta_angles[1], origin -> ps.origin, shellshockDuration -> ps.shellshockDuration, shellshockIndex -> ps.shellshockIndex, shellshockStartTime -> ps.shellshockTime, team -> cs.team, viewKickDamage -> damageTaken, viewKickDir -> damageDir
* rename `cod2_gentity_s`: absmax -> r.absmax, absmin -> r.absmin, apos -> s.apos, clipMask -> clipmask, contents -> r.contents, currentAngles -> r.currentAngles, currentOrigin -> r.currentOrigin, eFlags -> s.eFlags, eType -> s.eType, eventParm -> s.eventParm, expireTime -> nextthink, fxAxis -> s.origin2, groundEntityNum -> s.groundEntityNum, inUse -> r.inuse, maxs -> r.maxs, mins -> r.mins, modelIndex -> model, number -> s.number, ownerNum -> r.ownerNum, pos -> s.pos, tempFloat68 -> s.angles2[0], tempFloat6c -> s.angles2[1], triggerTeam -> team, turret -> pTurretInfo, weaponIndex -> s.weapon
* rename `trace_s`: allsolid -> startsolid, startsolid -> allsolid
* drop externs (36): g_0814cfc8, g_08187ba0, g_08187bac, g_0860b420, g_0864c380, g_0864c39c, g_0864c56c, g_0864c580, g_0864c584, g_0864c590, g_0864c594, g_0864e0d4, g_0864f970, g_0864f974, g_0864f978, g_0864f988, g_0864f98c, g_0864f990, g_0864f9c0, g_08716400, g_087a1fa0, g_087a2408, g_08853222, g_08853224, g_08853262, g_08853264, g_08853266, g_08853276, g_08853278, g_08853294, g_08853298, g_0885329a, level_maxclients, level_voteNo, level_voteTime, level_voteYes
* note: cod2_gclient_s: local size 0x27f0 vs canonical gclient_s 0x28a4
* note: trace_s: byte 0x22 is allsolid and 0x23 startsolid.
* note: trace_s.material (+0x18) local type int vs canonical material struct dMaterial_s *: check every access

### chunk 107

* delete: `dvar107_s`, `gentity107_s`
* replace: `FxParticleTemplate` -> `fx_primitive_s`, `dvar107_s` -> `dvar_s`, `gentity107_s` -> `gentity_s`
* rename `dvar107_s`: value -> current, word0 -> name, word4 -> flags
* rename `gentity107_s`: accumulate -> u.trigger.accumulate, attachLink -> tagInfo, contents -> r.contents, currentOrigin -> r.currentOrigin, dmg -> damage, eFlags08 -> s.eFlags, entClass -> handler, inuse -> r.inuse, linked -> r.linked, maxs -> r.maxs, mins -> r.mins, nextHurtTime -> u.trigger.timestamp, number -> s.number, svFlags -> r.svFlags, takeDamage -> takedamage, threshold -> u.trigger.threshold
* drop externs (32): g_0815e820, g_08188ab8, g_08188abc, g_08188ac0, g_0864c38c, g_0864c56c, g_0864d6c8, g_08716400, g_08853262, g_08853264, g_08853266, g_08a53804, g_08a53818, g_08a53c80, g_08a53ca0, g_08a5c9ac, g_08a5c9b0, g_08a5c9c4, g_08a6e960, g_08a6e980, g_08a6f2c0, g_hudelems, g_loc_warnings, g_loc_warningsAsErrors, vtbl_Cloud, vtbl_Cylinder, vtbl_Emitter, vtbl_Flash, vtbl_Line, vtbl_OrientedParticle, vtbl_Particle, vtbl_Tail
* note: dvar107_s: local size 0xc vs canonical dvar_s 0x24
* note: dvar107_s.word0 (+0x0) local type int vs canonical name const char *: check every access
* note: dvar107_s.word4 (+0x4) local type int vs canonical flags uint16: check every access
