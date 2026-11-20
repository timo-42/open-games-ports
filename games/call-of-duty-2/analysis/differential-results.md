# Differential harness results: all chunks, host GCC vs GCC 3.3.6 -O0

Lane: measurement only. No chunk source, header, tool or harness file was edited. Raw JSON: `.local/cod2/harness/results-host-2026-11-17/` (run A), `.local/cod2/harness/results-gcc33-O0-2026-11-17/` (run B), logs `.local/cod2/harness/logs/diffrun/`. Pre-run results were kept in `results-prev-before-0-2026-11-17/`.

## Run identity

- Git HEAD at start: `6ec093419ee51b77d989446eb77ff766b1bb326b`; HEAD at end: `f596dcaf6f06c4a8c77146c05a78af719fed06bc` (a concurrent boot lane committed during the run; working tree also had uncommitted `chunk_020.cpp` edits at the end).
- Command A: `difftest.py --chunk NNN --all --cases 300 --jobs 1` (host `g++`, harness flags `-O1 -mfpmath=387 ...`). Command B: same with `GCC33_OPT=-O0 PATH=$T/bin:$PATH COD2_LOCAL=$T/local` (the JSON `cflags` field still says `-O1`; the wrapper rewrites it to `-O0`). 6 parallel jobs, `nice 10`, seed 1 (harness default).
- Original binary SHA-256 `05e774a3...a3a7` (harness checks it). 71 chunks, 5129 functions per run, 142 batch jobs, all exit 0.
- Per-chunk source hash tested = `source_sha256` in each result JSON (table below). Because the boot lane edits chunks concurrently, A and B of a chunk may have seen different file contents: chunks where the A hash, B hash and the pre-run snapshot of `chunk_NNN.cpp` do not all agree are marked `*`. Headers were not hashed per run (only the `.cpp`); pre-run hashes of all `recon/chunks/*` files are in `logs/diffrun/chunk-sha256.txt`.

## Per-chunk table

| chunk | funcs | A pass | A fail | A unsup | B pass | B fail | B unsup | source sha256 (A) | note |
|---|---|---|---|---|---|---|---|---|---|
| 000 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | `0df3d5b4898e` | no functions (summary empty) |
| 001 | 40 | 28 | 0 | 12 | 28 | 0 | 12 | `ce620254d8f5` |  |
| 002 | 68 | 37 | 0 | 31 | 37 | 0 | 31 | `e04cd225aede` |  |
| 003 | 69 | 53 | 0 | 16 | 53 | 0 | 16 | `4a801a16a301` |  |
| 004 | 65 | 49 | 0 | 16 | 49 | 0 | 16 | `c4f36e195713` |  |
| 005 | 53 | 38 | 0 | 15 | 38 | 0 | 15 | `72be0f1acf6c` |  |
| 006 | 83 | 65 | 0 | 18 | 65 | 0 | 18 | `2008f6caf77c` |  |
| 007 | 80 | 57 | 0 | 23 | 56 | 0 | 24 | `26affd3e5632` |  |
| 008 | 77 | 63 | 0 | 14 | 63 | 0 | 14 | `1dbf5d73aa97` |  |
| 009 | 59 | 31 | 4 | 24 | 32 | 3 | 24 | `27ca75823152` | build warning: chunk defines placeholder global g_082055a0; it is NOT share |
| 010 | 97 | 31 | 0 | 66 | 31 | 0 | 66 | `98ea90d25f2c` |  |
| 011 | 61 | 34 | 0 | 27 | 12 | 0 | 49 | `65e3e3ed3132` |  |
| 012 | 73 | 32 | 0 | 41 | 31 | 1 | 41 | `c426dab9a9c2` |  |
| 013 | 88 | 45 | 0 | 43 | 45 | 0 | 43 | `7a8296429f39` |  |
| 014 | 105 | 82 | 2 | 21 | 82 | 2 | 21 | `7d1f08c441f5` |  |
| 015 | 73 | 34 | 0 | 39 | 34 | 0 | 39 | `db47e6d52017` |  |
| 016 | 17 | 12 | 0 | 5 | 12 | 0 | 5 | `34ecabb2e050` |  |
| 017 | 81 | 22 | 2 | 57 | 22 | 2 | 57 | `cf2b74b031fe` |  |
| 018 | 17 | 11 | 1 | 5 | 11 | 1 | 5 | `686fc234ee9a` |  |
| 019 | 55 | 24 | 0 | 31 | 24 | 0 | 31 | `d18266283fc4` |  |
| 020 | 55 | 28 | 0 | 27 | 14 | 0 | 41 | `95fc8a73ddac` |  |
| 021 | 71 | 39 | 0 | 32 | 38 | 1 | 32 | `b2223e91fa1a` | *src changed during run |
| 022 | 41 | 21 | 0 | 20 | 21 | 0 | 20 | `2ea99151ec72` |  |
| 023 | 51 | 32 | 0 | 19 | 24 | 0 | 27 | `957697e22d84` |  |
| 024 | 54 | 34 | 0 | 20 | 34 | 0 | 20 | `a716417a719c` |  |
| 025 | 86 | 77 | 0 | 9 | 77 | 0 | 9 | `85b8b6136521` | *src changed during run |
| 026 | 105 | 101 | 0 | 4 | 100 | 1 | 4 | `08cc9ae1e31e` |  |
| 027 | 85 | 65 | 0 | 20 | 65 | 0 | 20 | `f48bfaad1fa3` | *src changed during run |
| 028 | 58 | 50 | 0 | 8 | 33 | 0 | 25 | `59bfd88ffb2b` | *src changed during run |
| 029 | 104 | 81 | 15 | 8 | 57 | 1 | 46 | `434fd61c44b8` | *src changed during run |
| 030 | 86 | 82 | 2 | 2 | 83 | 1 | 2 | `96503652e4b4` | *src changed during run |
| 031 | 84 | 67 | 0 | 17 | 67 | 0 | 17 | `e2fd5db41ec7` | *src changed during run |
| 032 | 65 | 31 | 0 | 34 | 31 | 0 | 34 | `a6b07c9b383f` |  |
| 033 | 88 | 53 | 0 | 35 | 53 | 0 | 35 | `97aac8c2f4c9` |  |
| 034 | 75 | 44 | 0 | 31 | 44 | 0 | 31 | `c044f949c2d9` |  |
| 035 | 35 | 24 | 0 | 11 | 24 | 0 | 11 | `7c743e61043a` |  |
| 036 | 26 | 18 | 1 | 7 | 17 | 2 | 7 | `afb92a0ea4a6` |  |
| 037 | 53 | 33 | 0 | 20 | 33 | 0 | 20 | `8351cce38b3d` | *src changed during run |
| 038 | 94 | 68 | 3 | 23 | 68 | 3 | 23 | `90ce12e2442c` | *src changed during run |
| 039 | 55 | 37 | 0 | 18 | 37 | 0 | 18 | `ba3a410b701a` | *src changed during run |
| 040 | 80 | 38 | 2 | 40 | 40 | 0 | 40 | `590b30c381dc` | *src changed during run |
| 041 | 53 | 35 | 0 | 18 | 35 | 0 | 18 | `92b93edf1eaf` |  |
| 042 | 38 | 27 | 0 | 11 | 27 | 0 | 11 | `9edcd48f91ac` | *src changed during run |
| 043 | 86 | 48 | 1 | 37 | 48 | 1 | 37 | `43663db15c66` |  |
| 044 | 59 | 17 | 0 | 42 | 17 | 0 | 42 | `34834262ab1e` |  |
| 045 | 70 | 45 | 0 | 25 | 45 | 0 | 25 | `5813be213f87` | *src changed during run |
| 046 | 56 | 31 | 0 | 25 | 31 | 0 | 25 | `478294b50e65` | *src changed during run |
| 047 | 62 | 41 | 0 | 21 | 42 | 0 | 20 | `794640a9d783` | *src changed during run |
| 048 | 65 | 33 | 1 | 31 | 33 | 1 | 31 | `f7c6dbcb5411` | *src changed during run |
| 049 | 59 | 29 | 0 | 30 | 29 | 0 | 30 | `e2ada634e82b` | *src changed during run |
| 050 | 83 | 49 | 0 | 34 | 49 | 0 | 34 | `89c03b7bb977` | *src changed during run |
| 051 | 92 | 46 | 1 | 45 | 46 | 1 | 45 | `1b5f9e5f9bd3` | *src changed during run |
| 052 | 92 | 65 | 0 | 27 | 64 | 0 | 28 | `1f27e6da3e0d` | *src changed during run |
| 053 | 82 | 34 | 2 | 46 | 36 | 0 | 46 | `c82ea211a5b5` | *src changed during run |
| 054 | 105 | 37 | 0 | 68 | 35 | 0 | 70 | `1f6e1e5f52d2` |  |
| 055 | 131 | 76 | 1 | 54 | 77 | 0 | 54 | `b58107c6be8f` |  |
| 056 | 100 | 68 | 11 | 21 | 78 | 1 | 21 | `68c6a4dde484` |  |
| 057 | 75 | 34 | 1 | 40 | 34 | 1 | 40 | `a7b84d831f42` |  |
| 058 | 83 | 58 | 0 | 25 | 58 | 0 | 25 | `8d582117b652` | *src changed during run |
| 059 | 115 | 97 | 1 | 17 | 98 | 0 | 17 | `3c80802a5825` | *src changed during run |
| 060 | 100 | 43 | 0 | 57 | 43 | 0 | 57 | `dfffd069be21` | *src changed during run |
| 061 | 116 | 38 | 4 | 74 | 38 | 4 | 74 | `5ce0e936eb69` | *src changed during run |
| 062 | 89 | 0 | 0 | 89 | 0 | 0 | 89 | `1eff3d243886` | *src changed during run |
| 100 | 67 | 41 | 1 | 25 | 31 | 1 | 35 | `0ae2843813d3` | *src changed during run |
| 101 | 77 | 42 | 4 | 31 | 38 | 1 | 38 | `8ec8d6b68b38` | *src changed during run |
| 102 | 80 | 38 | 0 | 42 | 31 | 0 | 49 | `3c4ea4364047` | *src changed during run |
| 103 | 48 | 0 | 0 | 48 | 0 | 0 | 48 | `e03e60128f68` | *src changed during run |
| 104 | 89 | 8 | 0 | 81 | 3 | 0 | 86 | `b5f2d6e904fb` | *src changed during run |
| 105 | 86 | 12 | 0 | 74 | 12 | 0 | 74 | `2fa179ae3d03` | *src changed during run |
| 106 | 72 | 15 | 0 | 57 | 15 | 0 | 57 | `87c7adcacc12` | *src changed during run |
| 107 | 87 | 27 | 1 | 59 | 25 | 0 | 62 | `481d718c1f4c` | *src changed during run |
| **total** | 5129 | **2905** | **61** | **2163** | **2803** | **29** | **2297** | | |

## A x B verdict matrix (rows A, columns B)

| A \ B | pass | fail | unsupported |
|---|---|---|---|
| pass | 2778 | 4 | 123 |
| fail | 23 | 25 | 13 |
| unsupported | 2 | 0 | 2161 |

## Fail in A, pass in B (compiler-generation artefacts), 23

- 009 `0x0806b8be` NET_InitCvars: A {'fail': 300}; first mismatch memory at 0x085abe29 (image_rw) orig 0x88 recon 0x00; class: harness limitation (global aliasing suspected)
- 029 `0x080b4ed0` Dvar_SetOrCreateColor: A {'fail': 294, 'discard': 6}; first mismatch memory at 0x085b76ba (image_rw) orig 0x2d recon 0x32; class: harness limitation (global aliasing suspected)
- 030 `0x080b8c98` Rotate_Eased: A {'pass': 277, 'fail': 23}; first mismatch memory at 0x30000044 (arena) orig 0xdc recon 0x32; class: float last-bit
- 040 `0x080dcf8c` Mantle_RegisterDvars: A {'fail': 300}; first mismatch memory at 0x085abe29 (image_rw) orig 0x08 recon 0x00; class: harness limitation (global aliasing suspected)
- 040 `0x080de7a6` Vec3_MulAddAlt: A {'pass': 299, 'fail': 1}; first mismatch memory at 0x30000148 (arena) orig 0xcf recon 0x00; class: sNaN/NaN payload
- 053 `0x08120ec2` Vec3_Copy: A {'pass': 299, 'fail': 1}; first mismatch memory at 0x300000c6 (arena) orig 0xbc recon 0xfc; class: sNaN/NaN payload
- 053 `0x0812356a` MsgStream_ReadRawByte: A {'fail': 300}; first mismatch return orig 0xff recon 0x0; class: host-codegen-sensitive (see classification)
- 055 `0x08129062` FxObjFc_ApplyViewFalloff: A {'pass': 296, 'fail': 4}; first mismatch memory at 0x30000114 (arena) orig 0x8c recon 0x8b; class: sNaN/NaN payload
- 056 `0x0812bf46` Vec3_Lerp: A {'pass': 299, 'fail': 1}; first mismatch memory at 0x30000148 (arena) orig 0x50 recon 0x00; class: sNaN/NaN payload
- 056 `0x0812df42` FX_PrimParseLife: A {'fail': 22, 'pass': 273, 'discard': 5}; first mismatch memory at 0x30000098 (arena) orig 0xff recon 0x34; class: host-codegen-sensitive (see classification)
- 056 `0x0812df9e` FX_PrimParseSpawnRange: A {'pass': 267, 'fail': 31, 'discard': 2}; first mismatch return orig 0x1 recon 0x0; class: host-codegen-sensitive (see classification)
- 056 `0x0812e014` FX_PrimParseDelay: A {'pass': 270, 'fail': 27, 'discard': 3}; first mismatch memory at 0x30000088 (arena) orig 0xff recon 0x34; class: host-codegen-sensitive (see classification)
- 056 `0x0812e070` FX_PrimParseCount: A {'pass': 270, 'fail': 26, 'discard': 4}; first mismatch memory at 0x30000090 (arena) orig 0xff recon 0x34; class: host-codegen-sensitive (see classification)
- 056 `0x0812e0cc` FX_PrimParseIntensity: A {'fail': 31, 'pass': 263, 'discard': 6}; first mismatch return orig 0x1 recon 0x0; class: host-codegen-sensitive (see classification)
- 056 `0x0812e2fe` FX_PrimParseRadius: A {'pass': 275, 'fail': 22, 'discard': 3}; first mismatch memory at 0x30000128 (arena) orig 0xff recon 0x34; class: host-codegen-sensitive (see classification)
- 056 `0x0812e35c` FX_PrimParseHeight: A {'pass': 257, 'fail': 36, 'discard': 7}; first mismatch memory at 0x30000130 (arena) orig 0xff recon 0x34; class: host-codegen-sensitive (see classification)
- 056 `0x0812e3ba` FX_PrimParseWind: A {'pass': 260, 'fail': 34, 'discard': 6}; first mismatch memory at 0x30000138 (arena) orig 0xff recon 0x34; class: host-codegen-sensitive (see classification)
- 056 `0x0812e418` FX_PrimParseRotation: A {'pass': 263, 'discard': 14, 'fail': 23}; first mismatch memory at 0x30000260 (arena) orig 0xff recon 0x34; class: host-codegen-sensitive (see classification)
- 059 `0x0813777e` FxObj_SetField120: A {'pass': 299, 'fail': 1}; first mismatch memory at 0x30000162 (arena) orig 0xb7 recon 0xf7; class: sNaN/NaN payload
- 101 `0x080a387a` Vec3_SpherePoint: A {'pass': 282, 'fail': 18}; first mismatch memory at 0x30000040 (arena) orig 0x00 recon 0x6f; class: sNaN/NaN payload
- 101 `0x080a3aec` Vec3_SphereSpiral: A {'pass': 158, 'fail': 37, 'discard': 105}; first mismatch memory at 0x30000040 (arena) orig 0x92 recon 0x93; class: sNaN/NaN payload
- 101 `0x080aacd6` SinCos_Degrees: A {'pass': 282, 'fail': 18}; first mismatch memory at 0x30000041 (arena) orig 0xa6 recon 0xa5; class: float last-bit
- 107 `0x081236cc` FxCurveSrc_Copy: A {'pass': 299, 'fail': 1}; first mismatch memory at 0x3000009a (arena) orig 0xb2 recon 0xf2; class: sNaN/NaN payload

## Pass in A, fail in B, 4

- 012 `0x080768aa` Mem_BucketTree_Insert: B {'pass': 164, 'discard': 135, 'fail': 1}; first mismatch memory at 0x08286f7a (image_rw) orig 0x00 recon 0xff
- 021 `0x08094530` SV_FindReliableCommand: B {'pass': 265, 'discard': 34, 'fail': 1}; first mismatch outcome orig returned recon timeout(14)
- 026 `0x080aab3c` Mat3_ToQuat: B {'fail': 22, 'pass': 278}; first mismatch memory at 0x300000c0 (arena) orig 0x93 recon 0x94
- 036 `0x080cf6a4` send_tree: B {'fail': 202, 'discard': 96, 'pass': 2}; first mismatch memory at 0x300016f0 (arena) orig 0x00 recon 0x40

## Pass/fail changes caused by unsupported status

- A pass, B unsupported: 123; A fail, B unsupported: 13; A unsupported, B pass: 2.
- Reasons for A-supported/B-unsupported: {'opaque pointer parameter ': 134, 'every case was discarded (original crashed/exited ': 2}.
- **Harness limitation, B only**: `probe_struct_sizes` in `difftest.py` invokes `g++` (resolved to the GCC 3.3 wrapper in run B) and recovers from errors by parsing `struct NAME'` in stderr. GCC 3.3 phrases its errors differently (`invalid application of sizeof to an incomplete type`, no name), so the probe returns `{}` and every `struct X *` parameter becomes "opaque pointer parameter needs a spec". Chunks with empty `struct_sizes` in B: 000, 007, 011, 016, 017, 020, 023, 028, 029, 036, 054, 056, 057, 058, 062, 100, 101, 102, 103, 104, 105, 107 (in A: 000, 016, 036, 062, 103, 105). This removes about 123 functions from B coverage and is not a translation defect. Chunk 011 additionally shows a header error visible only to GCC 3.3: `struct tag used in naming union ScriptArg` (a struct/union tag mismatch in the migrated headers; host g++ accepts it).

## Unsupported breakdown (A)

- 1538: every case was discarded (original crashed/exited abnormally); no comparable case after 60
- 228: opaque pointer parameter '..' needs a spec
- 151: function not found in compiled reconstruction
- 126: C++ member function (implicit this) - add a spec
- 95: by-value type '..'
- 15: function pointer parameter '..'
- 10: every case was discarded (original crashed/exited abnormally); 5 consecutive original time
- Chunks 061 and 062 have 62 and 89 functions with "function not found in compiled reconstruction" (unchanged from the pre-migration results; these are not harness build failures).

## Harness build failures

None. Every chunk built a `hx_runner` in both runs (71/71, exit 0, empty stderr). Only warning: chunk 009 `chunk defines placeholder global g_082055a0; it is NOT shared with the original` (both runs). Chunk 000 contains no functions, so its summary is empty. The only compiler-visible header problem is the B-side `ScriptArg` struct/union tag error in chunk 011 above, which GCC 3.3 reports only in the struct-size probe.

## Remaining failures, classified

Classification is by first-mismatch evidence only (sNaN and fp labels for the single-byte Fx*/Vec3_* rows are inferred from the byte pattern, not confirmed) (no per-function root cause analysis): **clock** = depends on time/TSC; **sNaN** = memory diff is the x87 quiet-bit (0xffbc.. vs 0xfffc.., or NaN payload) from float load/store copying, harmless; **fp** = 1-6 byte float last-bit differences; **harness** = writes to original global addresses missing in recon (global/dvar pointer aliasing, chunk-owned global copies); **bug** = likely translation bug.

| chunk | addr | name | A | B | class | first mismatch (A) |
|---|---|---|---|---|---|---|
| 009 | `0x0806aaca` | MSG_ReadDeltaPlayerState | fail (125/300) | fail | likely translation bug | memory 0x30000178 arena orig 0xa5 recon 0xd8 (740 bytes) |
| 009 | `0x0806b3ca` | NetProfile_AddSample | fail (237/300) | fail | nondeterministic clock/thread | memory 0x300000a0 arena orig 0x4f recon 0x52 (1 bytes) |
| 009 | `0x0806b55a` | NetProfile_ComputeStats | fail (137/300) | fail | nondeterministic clock/thread | memory 0x0860af20 image_rw orig 0x8e recon 0x90 (1 bytes) |
| 009 | `0x0806b8be` | NET_InitCvars | fail (300/300) | pass | harness limitation (global aliasing suspected) | memory 0x085abe29 image_rw orig 0x88 recon 0x00 (27 bytes) |
| 014 | `0x0807c2cc` | ScrVar_ReadFieldValue | fail (208/300) | fail | likely translation bug | memory 0x30000040 arena orig 0x05 recon 0x00 (1 bytes) |
| 014 | `0x0807c6b0` | ScrVar_GetOrNewVarValueId24 | fail (1/300) | fail | likely translation bug | memory 0x08297502 image_rw orig 0x9a recon 0x00 (4 bytes) |
| 017 | `0x08083d12` | Scr_ResumeSavedStacks | fail (212/300) | fail | nondeterministic clock/thread | memory 0x083dce98 image_rw orig 0xba recon 0x36 (3 bytes) |
| 017 | `0x08085628` | Scr_SampleTimers | fail (300/300) | fail | nondeterministic clock/thread | memory 0x083dce98 image_rw orig 0x78 recon 0xfd (2 bytes) |
| 018 | `0x080857aa` | Sys_ReadTimestampLow | fail (300/300) | fail | nondeterministic clock/thread | return orig '0x4a9042de' recon '0x4b030f21' |
| 029 | `0x080b44a8` | Dvar_SetIntEx | fail (158/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x40 recon 0x44 (2 bytes) |
| 029 | `0x080b4526` | Dvar_SetFloatEx | fail (153/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x50 recon 0x54 (2 bytes) |
| 029 | `0x080b4598` | Dvar_SetVec2Ex | fail (129/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x70 recon 0x80 (2 bytes) |
| 029 | `0x080b462a` | Dvar_SetVec3Ex | fail (151/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0xa0 recon 0xc0 (2 bytes) |
| 029 | `0x080b46d4` | Dvar_SetVec4Ex | fail (160/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x70 recon 0x90 (2 bytes) |
| 029 | `0x080b4812` | Dvar_SetVec4OrColorEx | fail (157/300) | unsupported | harness limitation (global aliasing suspected) | memory 0x085b76b6 image_rw orig 0x30 recon 0x31 (10 bytes) |
| 029 | `0x080b4980` | Dvar_SetBool | fail (113/300) | unsupported | likely translation bug | memory 0x30000049 arena orig 0xb8 recon 0x00 (6 bytes) |
| 029 | `0x080b49aa` | Dvar_SetInt | fail (152/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x30 recon 0x34 (2 bytes) |
| 029 | `0x080b49cc` | Dvar_SetFloat | fail (148/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x70 recon 0x74 (2 bytes) |
| 029 | `0x080b49ee` | Dvar_SetVec2 | fail (162/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x70 recon 0x80 (2 bytes) |
| 029 | `0x080b4a18` | Dvar_SetVec3 | fail (150/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x70 recon 0x80 (2 bytes) |
| 029 | `0x080b4a48` | Dvar_SetVec4 | fail (149/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x90 recon 0xa0 (2 bytes) |
| 029 | `0x080b4aa2` | Dvar_SetVec4OrColor | fail (161/300) | unsupported | likely translation bug | memory 0x30000048 arena orig 0x60 recon 0x54 (2 bytes) |
| 029 | `0x080b4ed0` | Dvar_SetOrCreateColor | fail (294/300) | pass | harness limitation (global aliasing suspected) | memory 0x085b76ba image_rw orig 0x2d recon 0x32 (27 bytes) |
| 029 | `0x080b5138` | Dvar_InitSvCheats | fail (300/300) | fail | harness limitation (global aliasing suspected) | memory 0x085abe29 image_rw orig 0x88 recon 0x00 (9 bytes) |
| 030 | `0x080b7868` | FloatNoSwap | fail (296/300) | fail | likely translation bug | return orig '0x3a' recon '0x42680000' |
| 030 | `0x080b8c98` | Rotate_Eased | fail (23/300) | pass | float last-bit | memory 0x30000044 arena orig 0xdc recon 0x32 (6 bytes) |
| 036 | `0x080cdd38` | huft_build | fail (7/300) | fail | likely translation bug | memory 0x300003aa arena orig 0xff recon 0x00 (256 bytes) |
| 038 | `0x080d5d98` | Sys_Milliseconds | fail (300/300) | fail | nondeterministic clock/thread | return orig '0xf4' recon '0xf6' |
| 038 | `0x080d5e20` | Sys_MillisecondsRaw | fail (300/300) | fail | nondeterministic clock/thread | return orig '0xeb9ad484' recon '0xeb9ad487' |
| 038 | `0x080d6842` | Sys_EnterCriticalSection | fail (297/300) | fail | nondeterministic clock/thread | memory 0x0860b378 image_rw orig 0x8d recon 0x91 (1 bytes) |
| 040 | `0x080dcf8c` | Mantle_RegisterDvars | fail (300/300) | pass | harness limitation (global aliasing suspected) | memory 0x085abe29 image_rw orig 0x08 recon 0x00 (18 bytes) |
| 040 | `0x080de7a6` | Vec3_MulAddAlt | fail (1/300) | pass | sNaN/NaN payload | memory 0x30000148 arena orig 0xcf recon 0x00 (4 bytes) |
| 043 | `0x080e9b7c` | PM_SortPlanesByDot | fail (16/300) | fail | likely translation bug | return orig '0xf7ec5b00' recon '0xf7f05fd4' |
| 048 | `0x081084a4` | G_RegisterDvars | fail (300/300) | fail | harness limitation (global aliasing suspected) | memory 0x085abe29 image_rw orig 0x98 recon 0x01 (18 bytes) |
| 051 | `0x0811acb2` | Scr_SetEntityField | fail (1/300) | fail | likely translation bug | outcome orig 'returned' recon 'SIGSEGV(11)' |
| 053 | `0x08120ec2` | Vec3_Copy | fail (1/300) | pass | sNaN/NaN payload | memory 0x300000c6 arena orig 0xbc recon 0xfc (1 bytes) |
| 053 | `0x0812356a` | MsgStream_ReadRawByte | fail (300/300) | pass | likely translation bug | return orig '0xff' recon '0x0' |
| 055 | `0x08129062` | FxObjFc_ApplyViewFalloff | fail (4/300) | pass | sNaN/NaN payload | memory 0x30000114 arena orig 0x8c recon 0x8b (1 bytes) |
| 056 | `0x0812bf46` | Vec3_Lerp | fail (1/300) | pass | sNaN/NaN payload | memory 0x30000148 arena orig 0x50 recon 0x00 (3 bytes) |
| 056 | `0x0812c38a` | FX_GetEffectByName | fail (8/300) | fail | likely translation bug | stdout orig "Effect file ',n79\nxx;y\\p" recon "Effect file ',n79\nxx;y\\p" |
| 056 | `0x0812df42` | FX_PrimParseLife | fail (22/300) | pass | likely translation bug | memory 0x30000098 arena orig 0xff recon 0x34 (5 bytes) |
| 056 | `0x0812df9e` | FX_PrimParseSpawnRange | fail (31/300) | pass | likely translation bug | return orig '0x1' recon '0x0' |
| 056 | `0x0812e014` | FX_PrimParseDelay | fail (27/300) | pass | likely translation bug | memory 0x30000088 arena orig 0xff recon 0x34 (5 bytes) |
| 056 | `0x0812e070` | FX_PrimParseCount | fail (26/300) | pass | likely translation bug | memory 0x30000090 arena orig 0xff recon 0x34 (5 bytes) |
| 056 | `0x0812e0cc` | FX_PrimParseIntensity | fail (31/300) | pass | likely translation bug | return orig '0x1' recon '0x0' |
| 056 | `0x0812e2fe` | FX_PrimParseRadius | fail (22/300) | pass | likely translation bug | memory 0x30000128 arena orig 0xff recon 0x34 (5 bytes) |
| 056 | `0x0812e35c` | FX_PrimParseHeight | fail (36/300) | pass | likely translation bug | memory 0x30000130 arena orig 0xff recon 0x34 (5 bytes) |
| 056 | `0x0812e3ba` | FX_PrimParseWind | fail (34/300) | pass | likely translation bug | memory 0x30000138 arena orig 0xff recon 0x34 (5 bytes) |
| 056 | `0x0812e418` | FX_PrimParseRotation | fail (23/300) | pass | likely translation bug | memory 0x30000260 arena orig 0xff recon 0x34 (5 bytes) |
| 057 | `0x081314e2` | FxPrintf | fail (1/300) | fail | likely translation bug | stdout orig '7HMhk        k4abcJkK=z4' recon '7HMhk        k4abcJkK=z4' |
| 059 | `0x0813777e` | FxObj_SetField120 | fail (1/300) | pass | sNaN/NaN payload | memory 0x30000162 arena orig 0xb7 recon 0xf7 (1 bytes) |
| 061 | `0x0813cf38` | PB_CopyDllIfMissing | fail (7/300) | fail | likely translation bug | outcome orig 'returned' recon 'SIGSEGV(11)' |
| 061 | `0x0813e230` | Crt_CallInit | fail (300/300) | fail | harness limitation (global aliasing suspected) | memory 0x08a53388 image_rw orig 0xe1 recon 0x00 (36 bytes) |
| 061 | `0x0813e290` | Crt_RunFini | fail (300/300) | fail | harness limitation (global aliasing suspected) | memory 0x08188c68 image_rw orig 0x01 recon 0x00 (1 bytes) |
| 061 | `0x0813e310` | Crt_Fini | fail (300/300) | fail | harness limitation (global aliasing suspected) | memory 0x08188c68 image_rw orig 0x01 recon 0x00 (1 bytes) |
| 100 | `0x080627dc` | Com_SetLastFrameTime | fail (300/300) | fail | nondeterministic clock/thread | memory 0x081a226c image_rw orig 0xc9 recon 0xcb (1 bytes) |
| 101 | `0x080a387a` | Vec3_SpherePoint | fail (18/300) | pass | sNaN/NaN payload | memory 0x30000040 arena orig 0x00 recon 0x6f (4 bytes) |
| 101 | `0x080a3aec` | Vec3_SphereSpiral | fail (37/300) | pass | sNaN/NaN payload | memory 0x30000040 arena orig 0x92 recon 0x93 (19 bytes) |
| 101 | `0x080aacd6` | SinCos_Degrees | fail (18/300) | pass | float last-bit | memory 0x30000041 arena orig 0xa6 recon 0xa5 (1 bytes) |
| 101 | `0x080ab8c0` | Com_TouchMemory | fail (300/300) | fail | nondeterministic clock/thread | memory 0x0860af20 image_rw orig 0x3a recon 0x3d (1 bytes) |
| 107 | `0x081236cc` | FxCurveSrc_Copy | fail (1/300) | pass | sNaN/NaN payload | memory 0x3000009a arena orig 0xb2 recon 0xf2 (1 bytes) |
| 012 | `0x080768aa` | Mem_BucketTree_Insert | pass | fail (1/300) | likely translation bug (B only) | memory 0x08286f7a image_rw orig 0x00 recon 0xff |
| 021 | `0x08094530` | SV_FindReliableCommand | pass | fail (1/300) | likely translation bug (B only) | outcome orig returned recon timeout(14) |
| 026 | `0x080aab3c` | Mat3_ToQuat | pass | fail (22/300) | float last-bit (B only) | memory 0x300000c0 arena orig 0x93 recon 0x94 |
| 036 | `0x080cf6a4` | send_tree | pass | fail (202/300) | likely translation bug (B only) | memory 0x300016f0 arena orig 0x00 recon 0x40 |

A-fail class counts: likely translation bug 32, nondeterministic clock/thread 10, harness limitation (global aliasing suspected) 9, float last-bit 2, sNaN/NaN payload 8.

FX notes: the nine `FX_PrimParse*` functions, A fails, B passes for the FX_PrimParse family (compiler-generation artefact, probably host GCC 16 `-O1` codegen sensitivity on parse code, root cause not analysed); `FxPrintf`/`FX_GetEffectByName` fail in both with stdout that is identical in the visible prefix (difference lies in the truncated tail; not investigated).

## Top 20 likely translation bugs (fail in both A and B unless noted)

1. 030 `0x080b7868` FloatNoSwap (A fail, B fail): return orig '0x3a' recon '0x42680000'
2. 053 `0x0812356a` MsgStream_ReadRawByte (A fail, B pass): return orig '0xff' recon '0x0'
3. 029 `0x080b49aa` Dvar_SetInt (A fail, B unsupported): memory 0x30000048 arena orig 0x30 recon 0x34
4. 029 `0x080b49cc` Dvar_SetFloat (A fail, B unsupported): memory 0x30000048 arena orig 0x70 recon 0x74
5. 029 `0x080b4980` Dvar_SetBool (A fail, B unsupported): memory 0x30000049 arena orig 0xb8 recon 0x00
6. 029 `0x080b46d4` Dvar_SetVec4Ex (A fail, B unsupported): memory 0x30000048 arena orig 0x70 recon 0x90
7. 036 `0x080cdd38` huft_build (A fail, B fail): memory 0x300003aa arena orig 0xff recon 0x00
8. 009 `0x0806aaca` MSG_ReadDeltaPlayerState (A fail, B fail): memory 0x30000178 arena orig 0xa5 recon 0xd8
9. 014 `0x0807c2cc` ScrVar_ReadFieldValue (A fail, B fail): memory 0x30000040 arena orig 0x05 recon 0x00
10. 014 `0x0807c6b0` ScrVar_GetOrNewVarValueId24 (A fail, B fail): memory 0x08297502 image_rw orig 0x9a recon 0x00
11. 051 `0x0811acb2` Scr_SetEntityField (A fail, B fail): outcome orig 'returned' recon 'SIGSEGV(11)'
12. 061 `0x0813cf38` PB_CopyDllIfMissing (A fail, B fail): outcome orig 'returned' recon 'SIGSEGV(11)'
13. 043 `0x080e9b7c` PM_SortPlanesByDot (A fail, B fail): return orig '0xf7ec5b00' recon '0xf7f05fd4'
14. 056 `0x0812c38a` FX_GetEffectByName (A fail, B fail): stdout orig "Effect file ',n79\nxx;y\\p )|j\nX" recon "Effect file ',n79\nxx;y\\p )|j\nX"
15. 057 `0x081314e2` FxPrintf (A fail, B fail): stdout orig '7HMhk        k4abcJkK=z4EW\rq\x18{' recon '7HMhk        k4abcJkK=z4EW\rq\x18{'
16. 036 `0x080cf6a4` send_tree (A pass, B fail): memory 0x300016f0 arena orig 0x00 recon 0x40
17. 012 `0x080768aa` Mem_BucketTree_Insert (A pass, B fail): memory 0x08286f7a image_rw orig 0x00 recon 0xff
18. 021 `0x08094530` SV_FindReliableCommand (A pass, B fail): outcome orig 'returned' recon 'timeout(14)'
19. 029 `0x080b4a18` Dvar_SetVec3 (A fail, B unsupported): memory 0x30000048 arena orig 0x70 recon 0x80
20. 056 `0x0812df9e` FX_PrimParseSpawnRange (A fail, B pass): return orig '0x1' recon '0x0'

Notes on the ranking: FloatNoSwap returns `(float)bits` (recon 0x42680000 for bits 0x3a) where the original returns the bits unchanged (type-punning bug). MsgStream_ReadRawByte returns 0 where the original returns 0xff (A fails 300/300; B passes, so verify with the B build before fixing). The Dvar_Set* family (14 functions in chunk 029, 150/300 cases each; B unsupported because of the probe limitation) stores the value unclamped (e.g. arg 0x34 gives 0x...34 versus the original 0x...30), one shared root cause is likely. Items 17-20 fail in 1 of 300 cases and may be harness artefacts; only Mem_BucketTree_Insert, SV_FindReliableCommand and send_tree fail in B only (send_tree 202/300).


## Fixes (bug-fix lane, 2026-11-17/18)

Method: for every "likely translation bug" the original disassembly was compared with the reconstruction and `difftest.py --chunk N --func 0x... --cases 1000..3000` was re-run (host `g++ -O1`, harness flags). Where a mismatch came from the harness (unconstrained random inputs, uninitialised stack, frame layout) the input was constrained with a per-function spec in `recon/harness/specs.py` or the function was re-run with `--cflags=-O0` (same frame layout and no x87 excess precision as GCC 3.3.6 -O0) and the evidence is stated. Note: `--all` ignores `--cflags`; `--cflags` is only honoured with `--func`. Link check: `COD2_LOCAL=... COD2_BUILD=<worktree>/.local-build python3 games/call-of-duty-2/tools/link_test.py --no-gen --skip-before` exits 0 (aliases 1283 ok, pointers 2969 ok).

### Real translation bugs fixed (4 root causes, 11 functions)

| function | root cause class | fix | verdict after |
|---|---|---|---|
| chunk 030 `0x080b7868` FloatNoSwap | type-pun lost: `Float_Identity(float)` called with a `uint32` argument converts int -> float (`0x3a` became `0x42680000`); the original moves the raw dword into the float argument slot | reinterpret the bits through a union before the call | pass 1000/1000 |
| chunk 053 `0x08120ec2` Vec3_Copy | float copy where the original uses three integer `mov`s (x87 `fld`/`fstp` quiets sNaN payloads) | copy as three volatile dwords | pass 3000/3000 (was 1/300 fail) |
| chunk 107 `0x081236cc` FxCurveSrc_Copy | same: `scaleMin`/`scaleMax` copied as floats, original copies two dwords | two volatile dword copies | pass 3000/3000 (was 1/300 fail) |
| chunk 059 `0x0813777e` FxObj_SetField120 (and 7 siblings: C4, C8, 118, 11c, 124, 294, 298) | source already punned the argument through `unsigned int *`, but host GCC still emitted `flds`/`fstps` (verified in the generated asm), so sNaN payloads were quieted | read the argument through `volatile unsigned int *` | pass 3000/3000 (was 1/300 fail) |

Compiler-artefact class "float copy that must be an integer move" is now fixed everywhere the harness found it. The other sNaN rows (Vec3_MulAddAlt, Vec3_Lerp, FxObjFc_ApplyViewFalloff, Vec3_SpherePoint, Vec3_SphereSpiral) are arithmetic, not copies; see below.

### Reclassified: harness / compiler artefacts (no source change)

Verified by constraining the input (spec added to `specs.py`) unless noted. The reconstruction is not changed.

| function | what really differs | evidence | verdict after |
|---|---|---|---|
| 029 Dvar_SetIntEx/SetFloatEx, SetVec4OrColorEx, Dvar_SetInt, SetFloat, SetVec2/3/4(+Ex), SetVec4OrColor | NOT a clamp bug. `Dvar_SetVariant` (called by all of them) passes 173/173. The mismatching bytes are a pointer into the callee's own stack buffer (`buf`/`vec`) that the dvar stores; stack addresses differ because the recon frame differs from the original's (`sub esp,0x48/0x78`, ...). | window shows `f0 a8 a2 ff` vs `f4 a8 a2 ff` (same pointer, 4 bytes lower). With `--cflags=-O0` SetIntEx, SetInt, SetFloatEx, SetFloat and SetVec4OrColorEx pass (146/146, 146/146, 131/131, 149/149, 161/161); the others differ only by frame size at -O0 (e.g. 0x60 vs 0x6c). | artefact (stack-address in compared memory); not a bug |
| 029 Dvar_SetBool | upper three bytes of the bool value dword are uninitialised stack in the original (`mov [ebp-8],al` then the whole dword is passed). The reconstruction zero-extends on purpose (documented in the source). | orig value `0xf7c9f841`, recon `0x41`. | artefact (indeterminate bytes) |
| 009 NET_InitCvars, 040 Mantle_RegisterDvars, 048 G_RegisterDvars, 029 Dvar_InitSvCheats | same: `Dvar_RegisterBool` leaves the upper bytes of the value dword indeterminate; the dvar pool at `0x085abe20` records them. The name pointer word also differs (literal in the recon image vs original rodata; tolerated by the harness elsewhere). | windows: orig `01 28 c1 f7`, recon `01 00 00 00` (value byte equal) | artefact |
| 029 Dvar_SetOrCreateColor | `va("%g %g %g %g", int, int, int, int)` is a varargs/format mismatch that exists in the original (documented in the source); the third and fourth `%g` read stack words beyond the passed arguments. | text identical up to the second double, then stack residue | artefact |
| 061 Crt_CallInit, Crt_RunFini, Crt_Fini | by design: the linked build uses the system C runtime (`_init`, `__do_global_dtors_aux`), so the original's `completed` flag at `0x08188c68` and similar CRT state are not written. | source comments; chunk 001 notes | not a bug (intentional CRT substitution) |

All 9 "global-aliasing (suspected)" cases (009 NET_InitCvars, 029 SetVec4OrColorEx, SetOrCreateColor, InitSvCheats, 040 Mantle_RegisterDvars, 048 G_RegisterDvars, 061 Crt_CallInit/RunFini/Fini): none is a real bug; the rows above give the cause for each.

| function | what really differs | spec / evidence | verdict after |
|---|---|---|---|
| 036 huft_build | random `b[]` words are used as an index into `c[17]` (zlib guarantees 0..15), so both sides corrupt their own frame differently; after constraining the input the only difference is the 2 pad bytes of each `inflate_huft` entry (`r` is uninitialised stack in both, original holds 0xffff). | new spec; with `r` pre-filled with 0xff in a scratch copy: 1000/1000 pass | artefact (uninitialised struct padding) |
| 036 send_tree | passes 2000/2000 on the host build; the B-only failure was GCC 3.3 `-O0` frame/padding dependence | re-run | pass |
| 009 MSG_ReadDeltaPlayerState | the harness gave `from`/`to` 7- and 2-byte objects, so the 0x26a8-byte `memcpy` overlaps neighbouring arena objects (undefined for `memcpy`, differs from the original's `rep movs`) | new spec (full-size from/to) | pass 265/265 (235 discarded, original crashes on the random message) |
| 014 ScrVar_ReadFieldValue | `out->u.intValue` receives an uninitialised stack local (`unsetValue`) in both (source says so) | orig 0x05 vs recon 0x00 | artefact |
| 014 ScrVar_GetOrNewVarValueId24 | parent id is a 16-bit table index; random 32-bit parents address memory far outside `g_scrVar` (ASLR-dependent) | spec parent 0..0xfffd: 3000/3000 pass | pass |
| 051 Scr_SetEntityField | field index beyond the 10-entry `g_0815e820` table reads garbage rodata (both sides then call garbage) | spec index 0..9 (+ 0xc000 forms): 255/255 compared pass | pass |
| 061 PB_CopyDllIfMissing | `pb_dir` random 316 bytes without NUL overflows the 512-byte stack buffer (different layouts) | spec with terminated `pb_dir`: 858/858 pass | pass |
| 043 PM_SortPlanesByDot | `numPlanes == 0` (or > 11) returns/uses uninitialised `dots[]` | spec numPlanes 1..11: 1000/1000 pass | pass |
| 053 MsgStream_ReadRawByte | zeroed 12-byte msg overflows on the read, so the returned byte is an uninitialised stack local (orig 0xff residue, recon 0x00); source already says "deliberately uninitialised" | verified in disassembly | artefact |
| 056 FX_PrimParseLife/SpawnRange/Delay/Count/Intensity/Radius/Height/Wind/Rotation | `FX_ParseVec2` returns 1 on `sscanf == EOF` (-1) with both outputs untouched (verified in the original: only `== 0` and `== 1` are tested), so callers store uninitialised stack floats | spec: text always contains a number: 9 x 1000/1000 pass | artefact (uninitialised stack), not codegen |
| 056 FX_GetEffectByName, 057 FxPrintf | `FxPrintf` formats into a buffer and passes it to `Com_Printf` as a format string (faithful quirk); random `%` sequences in the text consume stack garbage | FX_GetEffectByName spec without `%`: 484/484 pass. FxPrintf (varargs) not re-driven; same cause | artefact |
| 012 Mem_BucketTree_Insert | passes 3000/3000 on the host build (1496 compared, 1504 original crashes); B-only mismatch was GCC 3.3 `-O0` | re-run | pass |
| 021 SV_FindReliableCommand | loop count is `reliableSequence - reliableSent` (random 32-bit: billions of iterations), so B hit the per-case timeout | spec bounding the distance to 140: 500/500 pass | pass |
| float arithmetic: 026 Mat3_ToQuat, 030 Rotate_Eased, 055 FxObjFc_ApplyViewFalloff, 101 SinCos_Degrees, Vec3_SpherePoint, Vec3_SphereSpiral, 040 Vec3_MulAddAlt, 056 Vec3_Lerp | host `-O1` keeps intermediates in x87 registers (excess precision) where GCC 3.3.6 `-O0` stores each float; last-bit / NaN-vs-finite differences | `--cflags=-O0`: ApplyViewFalloff, SpherePoint, SphereSpiral, Rotate_Eased, SinCos_Degrees all pass 2000..3000/N; Vec3_MulAddAlt, Vec3_Lerp, Mat3_ToQuat pass 3000/3000 at the default flags now | compiler-generation artefact |

### Counts

- Of the 32 "likely translation bug" rows (plus 4 B-only): 4 were real and are fixed in source (FloatNoSwap, Vec3_Copy, FxCurveSrc_Copy, FxObj_SetField120; the last fix also covers 7 sibling `FxObj_SetField*` functions with the same host-codegen shape).
- Reclassified with evidence as harness, input or compiler artefacts (no source change): the other 28 listed rows and the 4 B-only rows; the 9 aliasing cases are none a real bug.
- Specs added to `recon/harness/specs.py`: huft_build, MSG_ReadDeltaPlayerState, Scr_SetEntityField, ScrVar_GetOrNewVarValueId24, PB_CopyDllIfMissing, PM_SortPlanesByDot, FX_GetEffectByName, SV_FindReliableCommand, FX_PrimParse* (x9).

### Remaining (not passing at default host flags)

| function | reason |
|---|---|
| 029 Dvar_Set* (14), 009/040/048/029 bool-register functions, 029 SetOrCreateColor, 061 Crt_* | uninitialised stack bytes / stack pointers / intentional CRT substitution (above); the harness compares them byte-for-byte |
| 036 huft_build | uninitialised `inflate_huft` padding (2 bytes per entry) |
| 014 ScrVar_ReadFieldValue, 053 MsgStream_ReadRawByte | uninitialised stack local returned/stored, as in the original |
| 057 FxPrintf | format-string re-interpretation with stack garbage |
| 055 FxObjFc_ApplyViewFalloff, 101 Vec3_SpherePoint/SphereSpiral/SinCos_Degrees, 030 Rotate_Eased | x87 excess precision at host `-O1` (pass at `-O0`) |
| 10 clock/thread functions | nondeterministic by design (unchanged) |
