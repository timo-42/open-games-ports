# cod2_lnxded 1.3 reconstruction: boot-to-parity log

Lane goal: run the linked reconstruction (`.local/cod2/build/cod2_lnxded_recon`, built by
`python3 games/call-of-duty-2/tools/link_test.py`) exactly like the oracle (original
`cod2_lnxded`, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`) and remove
divergences one by one. Tags: [O] observed, [I] inferred, [U] unknown. Server output is data.

## How it is run

`games/call-of-duty-2/tools/oracle/run_recon.sh <capture|gdb|plain> <dm_toujane|sd_carentan|none> [run]`
mirrors `run_original.sh`: same `bwrap --unshare-net --unshare-pid --unshare-ipc` sandbox (loopback only),
same server arguments, same `fs_basepath` (`.local/cod2/oracle/run/base`) and per-run `fs_homepath`
(`.local/cod2/oracle/run/home/<case>-run<N>`, the same path the oracle captures use, so `server.log` diffs
directly without path normalization), same `stat32_shim.so` preload. `gdb` mode runs the server under
batch gdb inside the sandbox and interrupts it after `TLIMIT` seconds (default 60) so hangs also yield a
backtrace. Outputs go to `.local/cod2/recon-run/<mode>/<case>/run<N>/`. The reconstruction is never run
outside the sandbox.

Comparison: `diff <(grep -v "Hitch warning" .local/cod2/oracle/captures/dm_toujane/run1/server.log)
<recon log>` (oracle normalization from `oracle-baseline.md`: Hitch lines dropped; paths identical by
construction).

## Static checks added while booting

* Call-sequence comparison (scratch tool, not committed): for every function named in
  `analysis/symbols/symbols.json` that maps to one reconstruction symbol, the multiset of direct call targets
  in the original (`objdump` of `[addr, addr+size)`, targets named through symbols.json / PLT) is compared with
  the reconstruction's. 635 functions differed at the start. Most differences are benign (inlined helpers,
  recon-only static helpers, libc direct calls instead of `Com_Fopen`-style wrappers), but the check found
  the link-wave losses below.
* `-Wreturn-type` over all chunks: after the fixes below, only `fn_08085902` (chunk 018, intentional and
  documented: the binary returns the callee's eax) falls off the end of a non-void function.

## Crash / divergence log

| # | Symptom | Function (original addr) | Error class | Fix | Evidence |
|---|---|---|---|---|---|
| 1 | SIGSEGV in FS_Startup, `mov 0x314(%eax)` with eax = 0x7d5 (2005) | `FS_LoadPackFile` 0x080a0d5e | wrong layout (chunk-local duplicate type) | `chunk_024_types.h`: `unz_file_info_s` was 0x38 bytes (14 words), the original is 0x50 (+ `tm_unz` date at +0x38). `unzGetCurrentFileInfo` (chunk 037, 0x50-byte layout) wrote the year over the next local (`pack`). | original frame reserves `[ebp-0x178]..[ebp-0x128]` (0x50) for the info; faulting value 2005 = tm_year of the zip entry |
| 2 | `execing default_mp.cfg` instead of `default_localize_mp.cfg`; language stays english, localized iwds missing from the second search path | `Loc_InitLanguages` 0x08138224, `Loc_LoadLanguage` 0x0813840a, `Loc_RegisterDvars` 0x08138162, `Loc_TranslateString` 0x081384cc, `Loc_TranslateExeString` 0x08138522 | statements lost in the link wave (lane L19, commit 6ed96a78f) + mistranslation | Re-translated from disassembly: the dvar re-registration, the error print, `Dvar_SetInt` fallback loop, the `LocTable_Lookup` return, the UNLOCALIZED wrapper and all warning/error prints were missing; `Loc_UpdateDoubleByteFlag` call in `Loc_RegisterDvars` was missing | objdump 0x08138128..0x081385fe; pre-L19 text in `git show 6ed96a78f^` |
| 3 | (same symptom, deeper) localized string table never loaded | chunk 060/061: `LocFile_Free`, `FS_CollectFilesRecursive`, `FS_CollectLocalizedFiles`, `LocTable_Reset`, `LocPath_*`, `LocTable_SetFileBaseName`, `LocLine_CheckAmpersandSlots`, `LocLine_Parse`, `LocList_NextEntry`, `LocTable_LoadFile`, `LocTable_ResetCurrent`, `LocTable_Create`, `LocTable_LoadAll`, the std::map wrappers (`LocMap_Find/End/InsertAt/Clear/...`, `LocRb_*`, `RbTree_*`), `PB_*` helpers | statements lost in the link wave (L19) | Re-inserted every pure-deletion hunk of L19 that contained a call and whose code does not survive elsewhere in the function (53 hunks in 060, 27 in 061, 50 in 059 incl. the Fx spawners and pool allocators); adapted to the current prototypes with pointer casts only. L19 had also deleted `return` statements (functions fell off the end) | `git diff 6ed96a78f^ 6ed96a78f`; per-function call-sequence comparison |
| 4 | SIGSEGV in `LocRb_Decrement` on the first map insert | `LocMap_InsertUnique` 0x0813b52c, `LocMap_Clear` 0x0813acd8, `RbTree_CreateNode` 0x0813bcde | mistranslated branch / lost statements | `j == begin()` test had been replaced by `NULL == j` and the insert call dropped; `LocMap_Clear` lost `LocMap_EraseSubtree`; `RbTree_CreateNode` lost `Rb_ConstructValue` (node value never constructed) | objdump 0x0813b52c (calls 0x0813bab4 begin, 0x0813a8c2 equal, 0x0813b90a insert) |
| 5 | `Could not find script 'codescripts/delete'` | `Scr_LoadScriptFile` 0x08076296 | wrong call arguments | the loader (0x08077fde) gets `(name, "<name>.gsc", codePos, 1)`; the translation passed `(0, name, ...)`, so the file without extension was opened | objdump 0x0807637f..0x08076395 |
| 6 | script compile hangs in `ScrVar_FindInBucket` (corrupt hash ring) | `StrPool_FreeBlock` 0x08077090 | hard-coded original address | handle = `(ptr - 0x08206f80) >> 3` used the original pool address literally; the reconstruction's pool lives elsewhere, so frees went to wrong handles (27008 instead of 3696) and the buddy free tree was corrupted. Now `ptr - g_08206f80`. Same class fixed at `ScrDbg` removed-thread sentinel (chunk 013, `0x083d7600`), `G_FreeClassObject1`/`Scr_AddClassObject1` (`0x08628f80`, chunk 051), `G_InitEntityRecord` (`0x08716400`, chunk 052), `PlayerView_ApplySwayBlend` (bone table `0x08184ca8` and word `0x088532c2` read as addresses, chunk 040). A scan of all chunk code for numeric literals inside the original image range is now empty | `statediff.py` bisection over 276 script loads (first divergence between load 189 and 190), gdb watchpoints on the bucket node in both binaries, MT_AllocIndex/MT_FreeIndex trace |
| 7 | `SV_SvEntityForGentity: bad gEnt` | `G_InitEntityRecord` 0x0811e85c | hard-coded original address | `s.number` derived from `e - 0x08716400` | see 6 |
| 8 | `radius not specified for trigger_radius` | `G_SpawnString` 0x0811a3dc | dropped return value | the tail call's result (`KeyValue_Lookup`) is returned; the translation returned 0 | objdump 0x0811a3fe..0x0811a404 |
| 9 | SIGSEGV in `CM_TraceThroughAabbTree_r` (partitionChecks NULL) | `CM_BoxTraceBody` 0x0805b414 (status was `partial`), `Sweep_TraceEntityDispatch` 0x0805cefe | mistranslation (partial function) | re-translated: `cm_scratch_acquire` fills trace work +0xa0..+0xb7 (check count, stamp arrays, partition checks, temp box model, bounds), the translation passed a local array; contents mask is set before `Sweep_UpdateCullFlag`; the z bounds no longer write into `end[2]`; the type-0x3ff branch reads the box model from +0xb0 instead of a NULL placeholder; dispatcher order and the local trace initialisation (fraction=1, two flag bytes) follow the binary | objdump 0x0805b414..0x0805b893 and 0x0805cefe..0x0805d327 |
| 10 | SIGSEGV in `XAnim_BindingRebuild` (bone bit index 10716) | `XAnim_BindingRebuild` 0x080c1260 | missing dereference | part record = `*(binding->models[i])` (`mov eax,[edx+eax*4+0x1c]; mov eax,[eax]`) | objdump 0x080c1395..0x080c13a1 |
| 11 | `Info string length exceeded` for systeminfo | `FS_LoadedIwdChecksums/Names/PureChecksums`, `FS_ReferencedIwdPureChecksums` (0x0806590a..0x08065c36) | wrong field | tests `searchpath+0xc` (bLocalized), not `dir`; localized iwds were listed in `sv_iwds`/`sv_iwdNames` | objdump `cmp DWORD PTR [eax+0xc],0x0`; original systeminfo build traced with gdb (length 637 vs 1019) |
| 12 | `rcon map_restart` never answers (hang in `Scr_ShutdownVariableRoots`) | `VM_ExecuteInternal` waittill/endon (0x08081b2c, 0x08081d36) | wrong constant | object notify-list head name is `0x1fffe`, the translation used `0x20000`, so `Scr_ClearObjectChains` never found the list and the drain loop spun | gdbprobe trace: same owner 2073, `FindVarValueId(2073,0x1fffe)` = 4916 (original) vs 0 |
| 13 | fake client: no `connectResponse` (coordinator report) | `SV_DirectConnect` 0x0808eef3..0x0808ef25 | dropped call | restored `NET_OutOfBandPrint(1, from, "connectResponse")` | objdump |
| 14 | fake client: first snapshot undecodable (Huffman) | `SV_SendClientSnapshot` 0x0809adea | wrong field | message header is `lastClientCommand` (+0x20840), the translation wrote `reliableAcknowledge` (+0x20810), so the client keyed its XOR stream with the wrong command | objdump 0x0809ae4c, 0x0809af20 |
| 15 | fake client: integer entity/client fields arrive as 0 / 0x80000000 | `MSG_WriteDeltaField` 0x080692dc | type punning | re-translated: change/zero tests compare raw words; only bits 0/-99 convert floats (truncating fistp); the long fallback writes the raw word; the bit-field path shifts the raw integer (the translation converted the integer bit pattern through float) | objdump 0x080692dc..0x08069601; deterministic-item mismatches 524 -> 169 |

## Fix count by error class

| class | count |
|---|---|
| statements/returns deleted by link-wave lane L19 (chunks 059/060/061; Loc*, LocTable/std::map glue, Fx spawners and pools, PB helpers) | 130 hunks restored in ~90 functions + 8 Loc functions re-translated |
| mistranslated branch / partial function re-translated (`LocMap_InsertUnique`, `CM_BoxTraceBody`, `Sweep_TraceEntityDispatch` + shared setup, `MSG_WriteDeltaField`) | 5 |
| hard-coded original image addresses | 7 sites in 6 functions |
| wrong struct field / layout (`unz_file_info_s` size, bLocalized vs dir x4, lastClientCommand x2 sites) | 4 |
| wrong constant (notify-list head name, 2 sites) | 1 |
| wrong call arguments (`Scr_LoadScriptFile`) | 1 |
| dropped return value (`G_SpawnString`) / dropped call (`connectResponse`) / missing dereference (`XAnim_BindingRebuild`) | 3 |

## Tools

* `tools/oracle/run_recon.sh` (sandboxed runner, capture/gdb/plain modes, hang watchdog).
* `tools/oracle/compare_recon.py` (normalized OOB packet + server.log comparison; multi-packet rcon replies
  are also compared joined because the rcon redirect drops one character at each packet boundary, which can
  split the run index of `fs_homepath`; the reconstruction reproduces that quirk byte for byte).
* `tools/oracle/statediff.py` (data-image differential between the two binaries at a breakpoint; pointers into
  the image blobs, function pointers and string literals are matched semantically; heap/mmap pointers with a
  dominant shift are ignored). Note: nine globals are still defined inside chunk objects instead of the data
  image (`bg_numWeapons`, `bg_weaponDefs`, `bg_ammoTypes`, `bg_clipTypes`, their counts, `bg_sharedAmmoCaps`,
  `bg_numSharedAmmoCaps` (chunk 043), `g_082055a0` (chunk 009)); statediff shows them as zero on the
  reconstruction side (data image issue, not a behaviour difference).
* Scratch helpers (not committed): per-function call-multiset comparison, large-displacement comparison
  (flags field-offset mistakes such as `SV_UserMove` 0x234f0 vs 0x236f0, `SV_ClientResetDownload`
  0x20cc0 vs 0x20c68, `SV_QueueVoicePacket` 0xae810 vs 0xae714 that are still open), gdb-in-sandbox runner
  with rcon injection.

## Current state (2026-11-18)

* **Milestones 1-5 reached** [O] for both oracle cases:
  `compare_recon.py .local/cod2/oracle/captures/dm_toujane/run{1,2,3} .local/cod2/recon-run/capture/dm_toujane/run1`
  -> `match` for all three oracle runs; `sd_carentan/run1` -> `match`. That is: server.log (132 lines) identical
  after the documented normalization (FS_Startup x3 incl. map_restart, german localized iwds, socket, Common
  Initialization, Server/Game Initialization, master resolve, rcon log lines); every probe step byte-identical
  after normalization (getinfo, getinfo repeat, getstatus, getchallenge x2 = `<CHALLENGE>` and stable within the
  run, unknown OOB = no reply, bad rcon, rcon status, serverinfo, dvarlist (7 packets), cvarlist, map_restart
  (5 packets of re-initialisation text), getstatus/status after restart, quit = no reply); server exit status 0
  after `rcon quit` on both sides.
* **In-band (fake client, coordinator's oracle)** [O]: the full session now completes against the
  reconstruction (challenge, connect, gamestate, 161 snapshots, 40 server commands, say, disconnect, exit 0).
  Gamestate (configstrings, all baselines), server commands 1..40, clientState and the OOB items match.
  Remaining deterministic-item mismatches at that point: 169 (`fakeclient.py compare`; all resolved, see
  "In-game parity" below):
  * 3 turret entities (308, 311, 517): `angles2[0]` (rest pitch) -90 vs -81/-57 -> `ents_hash` of every snapshot
    (121 items) and first_active_entity. Suspect: turret rest/clamp state (`Turret_ClampAim` 0x0810bb76 flags
    0x200/0x400 and `unk3c`, or turret spawn fields); not yet resolved.
  * 13 `ps_rel`/4 `deltaNum`/14 `wire_len`: playerState commandTime relative to serverTime is -49/-99/-100
    where the original has -50 (+ delta base 2 vs 1). Timing-sensitive or a usercmd/serverTime handling
    difference: [U].
  * `oob:rcon_status_connected` differs in the client row (not inspected yet).
* Note for the fake-client lane: `fakeclient.py compare` reported "0 mismatches" when the reconstruction
  capture had crashed before writing `summary.json`; missing items should count as mismatches.

## In-game parity (fake client), 2026-11-18

Lane goal: every item that is deterministic across 3 original fake-client runs (`fakeclient.py compare`) is
identical in the reconstruction. Method as above: sandboxed runs of both servers, disassembly of the original,
faithful re-translation, `difftest.py` for touched functions, `link_test.py` rebuild. Build: HEAD after the
naming-pass-2 merge (2ac955ff2) plus the commits below.

### Divergence log (continues the table above)

| # | Symptom | Function (original addr) | Error class | Fix | Evidence |
|---|---|---|---|---|---|
| 16 | `rcon status` ping 999 while connected (original 33-42) | `SV_UserMove` 0x08090e34 | wrong field offset | ack time written at `client+0x236f0+k*0x26c4` (a macro in `chunk_020_types.h`), the original writes `frames[k].messageAcked` = `+0x234f0+k*0x26c4`; `SV_CalcPings` (0x080963c8, correct) never found an acked frame. Macro removed, canonical field used | objdump 0x08091021..0x08091043; SV_CalcPings 0x08096476 |
| 17 | (latent) usercmd delta base shifted by one dword | `SV_UserMove` 0x08090e34 | wrong local layout | `CL_CreateCmd` filled `&baseline[1]` while the first `MSG_ReadDeltaUsercmdKey` used `&baseline[0]` (uninitialised serverTime, shifted weapon bytes); the original uses one usercmd at `[ebp-0x38]` for both | objdump 0x08090f2b..0x08090f43 |
| 18 | turrets 308/311/517 (mp_toujane) rest at pitch -90 instead of -81/-57 (`angles2[0]`; 121 `ents_hash` items + 3 first-snapshot entities) | `Turret_FindAimOffset` 0x0810bf00 | mistranslated local layout (dropped matrix row) + undersized trace buffer | the original builds a 4x3 affine matrix at `[ebp-0x48]` (AnglesToAxis + `currentOrigin` copied to row 3, `0x0810bf96..0x0810bfab`) for `Vec3_TransformAffine`; the translation copied the origin into a separate unused array, so the drop-pitch traces (31 steps of -3 deg, first hit stored at `pTurretInfo+0x1c`) ran near the world origin and never hit. The `G_TracePointEx` result went into `float[3]` instead of the 0x24-byte `trace_t` (stack overwrite) | objdump 0x0810bf00..0x0810c10d; Turret_RestAim 0x0810be37 reads `+0x1c` |

`difftest.py` after the fixes: `SV_UserMove` 235 pass / 65 discarded (original SIGSEGV on random input),
`Turret_FindAimOffset` 155 pass / 145 discarded; verdict pass for both. The random inputs do not reach the
fixed paths deeply; the end-to-end oracle is the evidence.

### Decided as timing, not behaviour [O]

* **playerState commandTime -49/-99/-100 (+ deltaNum 2)**: the old client sent usercmds on a free-running
  50 ms timer. Server message minus last client command (`packets.jsonl`): the original runs stayed at
  4-25 ms with a slow drift; the reconstruction run drifted through 0 ms (its client ran at 50.2-50.4 ms per
  packet vs 50.1 ms), and exactly at the crossing commandTime became -100 (command arrived after the frame) or
  -49 (two commands with serverTime+1 before one frame). Both servers run frames on the same 50 ms grid
  (`Com_Frame_Try_Block_Function` 0x080626f4 matches: minMsec 1 on a dedicated server, `NET_Sleep(0)` busy loop).
  The client now answers each server message immediately (`--pace snapshot`), which removes the phase
  dependence; with it, ps_rel/deltaNum are deterministic in the original and identical in the reconstruction.
* `snapNNN:wire_len` (43 of 161 indices vary across the original runs: Huffman length of the absolute
  serverTime) and `gamestate:payload_len` (random checksumFeed) are excluded as families by `compare`.
* `Client 0 connecting with 1700|1750 challenge ping` is normalised (client timing, documented in
  `oracle-client.md`).

### Field-offset scan (item 4 of the lane brief)

A displacement comparison (scratch, not committed: every `[reg+disp]`/`lea`/`add` constant >= 0x1000 per
function, original vs reconstruction objdump) flagged 152 functions; reviewed for client_t:
* `SV_UserMove` 0x234f0 vs 0x236f0: **real** (fix 16).
* `SV_ClientResetDownload`/`SV_NextDownload_f`/`SV_WriteDownloadToClient` 0x20cc0/0x20ce0 vs 0x8330/0x8338:
  benign codegen (`[cl+(i+0x8330)*4]` = `[cl+i*4+0x20cc0]`).
* `SV_QueueVoicePacket`/`SV_WriteVoicePackets` 0xae715 vs 0xae714: benign (`voicePackets[n].data` =
  `+0xae714+1`; talker `+0xae714`, dataSize `+0xae815` agree with the original).
* `SV_WriteSnapshotToClient`/`SV_BuildClientSnapshot`/`SV_SendMessageToClient` (0x20e30 vs 0x20e34, frame-relative
  offsets): address arithmetic split differently; snapshot contents match the oracle end to end.
The other flagged functions differ in stack-frame displacements (locals) and were not reviewed one by one [U].

### Oracle extension and current state [O]

Captures (original 3x, reconstruction 2x, snapshot pacing, same `fs_homepath`
`.local/cod2/oracle/run/home/client_<case>-run<N>` for both servers):

| case | original identical / variable items | reconstruction mismatches on deterministic items |
|---|---|---|
| `dm_toujane` | 2759 / 223 | **0** |
| `sd_carentan` | 2814 / 250 | **0** |
| `restart_dm_toujane` (rcon `map_restart` while active) | 3079 / 368 | **0** |

```
python3 games/call-of-duty-2/tools/oracle/fakeclient.py run <case> <N> [--server recon]
python3 -I games/call-of-duty-2/tools/oracle/fakeclient.py compare .local/cod2/oracle/captures/client_<case> .local/cod2/recon-run/captures/client_<case>
```
Timer-paced original captures are kept in `.local/cod2/oracle/captures/client_{dm_toujane,sd_carentan}-timerpace/`.
`compare` now counts a candidate run without `summary.json` as all-missing (it previously skipped it and reported
0 mismatches after a crash), reports items present only in the candidate, and exits 1 on any mismatch.

### Fix count by error class (in-game lane)

| class | count |
|---|---|
| wrong struct field offset (`SV_UserMove` messageAcked) | 1 |
| wrong local layout (usercmd baseline; turret affine matrix + trace buffer) | 2 functions |
| oracle tooling (compare missing runs; timing families; snapshot pacing; homepath) | 4 |

Not covered yet: spawning/movement/weapons/damage (the client stays a spectator), several clients, downloads,
voice, map change (`map`), packet loss.
