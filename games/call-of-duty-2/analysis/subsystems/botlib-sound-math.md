# Subsystem analysis: botlib remnants, sound aliases, math library (cod2_lnxded 1.3)

Target: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7` (ELF32 i386, stripped, GCC 3.3.x).
Method: read-only static analysis of `analysis/symbols/*.json` (4,590 rows at start, 5,130 at finish; chunks 000-062, 100-105 present), `recon/chunks/chunk_NNN.cpp|_types.h`, Ghidra pseudocode in `.local/cod2/decomp/functions{,_eh}/`, and `objdump -d -M intel` of the binary (no REA/Ghidra session opened). Section map used for address arithmetic: `.text` 0x0804a6c0, `.rodata` 0x0813e340 (file 0xf6340), `.data` 0x0817d3a0 (file 0x1343a0), `.bss` 0x08188c00.

Evidence tags: **[O]** observed in disassembly/strings/data of this binary; **[I]** inferred (naming, intent, lineage); **[U]** unknown / not established. No runtime tracing was done (the oracle baseline notes gdb is not installed), so every behavioural statement is static. Call-graph statements ("no callers") come from a scan of all direct `call`/`jmp` rel32 instructions in `.text` plus a scan of `.rodata`/`.data` for 32-bit pointers; indirect calls through computed pointers cannot be excluded.

Tags in `symbols.json` are noisy. Selected by tag (botlib, sound, math, vector and sub-tags such as math/vec3, math/matrix, common/rand, snd/...): 832 rows. Of those, 8 (`entity/trigger`) are unrelated (matched on "trig"), `math/words` (chunk 029, 20 rows) are 32-bit word/vec copy helpers, `sound/state` (2 rows) are not sound at all (see Naming), and `math/bitset` (3) are bit-array helpers. Related functions carrying other tags that I included: GSC math/sound builtins (chunk 105, tagged game/script-*), `G_*Index` configstring helpers (chunk 052), SV bot functions (chunk 020/022), `Float_RoundHalfUp` and Fx math (chunks 051/055), `Q_ftol`/`Sys_SnapVector` (chunk 038), `PM_*Sound*` (chunk 041), player sound methods (chunk 103).

## 1. Key findings (read this first)

1. **[O] The "botlib" is only the id Tech 3 `l_script`/`l_precomp` text preprocessor** (chunks 001, 002, 100; 0x0804a710-0x08052100, 88 tagged rows). There is no AAS, no bot AI, no `be_*`, no `botlib` string. **[O] Nothing outside that address range calls into it** (no external call/jmp rel32; the only `.data` pointers into it are the directive tables at 0x817d3c0.. / 0x1343cc-0x13446c file offsets and `.eh_frame` FDEs). It is statically linked, effectively dead code on the server. A native port can omit it.
2. **[O] "Bots" are server-side test clients**: GSC `addtestclient()` -> `SV_AddBot` (0x080913da) and `SV_RunBotClients`/`SV_BotRandomUserCmd` (0x08096952/0x0809676c) drive random usercmds with libc `rand()`. They connect through `SV_DirectConnect` with a fake `netadr` of type 0.
3. **[O] The server never mixes audio.** Sound on the dedicated server is (a) a configstring table of alias *names* (CS base 0x24e, 256 entries), (b) event entities / server commands that carry a one-byte configstring index, (c) a full csv sound-alias loader (`Com_LoadSoundAliases`) used only to answer GSC `soundexists("alias")` and to emit load-time diagnostics. Weighted alias selection (`Com_PickSoundAlias`) has no live caller.
4. **[O] Naming bug with gameplay impact**: symbols.json (and chunk 052) name 0x0811d634 `G_SoundIndex` and 0x0811d668 `G_AnimIndex`. The binary says the opposite: 0x0811d668 (CS base 0x24e, max 0x100, returns a byte) is what `playsound`, `playsoundasmaster`, `playloopsound`, `playlocalsound`, item pickup sounds call; 0x0811d634 (CS base 0x48e, max 0x10) is called by `precacheshellshock` (0x081148e6) and by `setchannelvolumes` (chunk 103) with a *shock name*. So 0x0811d634 is a shellshock index lookup and 0x0811d668 is the sound index lookup.
5. **[O] All float math is x87 with 80-bit intermediates; rounding to 24 bits happens only at `fstp dword` stores and at float-typed call boundaries.** Reconstructed C that writes `float` expressions loses this. Concrete, verified differences are in section 9.
6. **[O] Systematic summation-order discrepancy**: chunks 025/026/030/031 (verified samples; the same reversed pattern also appears once or twice in 003/004/034/040/041/047/050/051/055, not individually verified) write dot-product-style sums high-index-first (`v[2]*m[2] + v[1]*m[1] + v[0]*m[0]`); the binary accumulates low-index-first (`((v0*m0 + v1*m1) + v2*m2)` in st(0)). Chunks 053/059 state the correct order. For an extended-precision accumulator the two orders can differ in the last bit of the stored float.
7. **[O] Rounding bugs in reconstructed sources**: Ghidra `ROUND()` in this binary is always a *truncating* `fistp` (control word or'ed with 0xc00), i.e. `(int)x`. Chunk 045 implements it as round-half-away (`Round_To_Int`), chunk 049 as `lrintf` (nearest-even). Chunks 026/043/048/051/055 implement `floor(x+0.5)` as `floor((double)(x + 0.5f))` (float add) while the binary does the add in x87 extended (`fld x; fld 0.5f; faddp; fstp qword`), which differs for e.g. x = 0.49999997f.
8. **[O] Three independent RNGs** (section 4.5): glibc `rand()`, a Quake-style LCG at 0x0817e338 (init 0x89abcdef), and a second LCG at 0x085a9844 used only by `Com_PickSoundAlias`. GSC `randomint/randomfloat/*range` use the 0x817e338 LCG; bullet-spread direction, bot input, weapon-drop spin and sway use libc `rand()` through `Rand_Fraction`.
9. **[O] `Com_PickSoundAlias` weighted pick is degenerate as compiled**: the test is `rand*accum < probability * -2147483648.0f` (constant 0xcf000000 at 0x0814d7c0) and `Com_Rand()` is in 0..32767, so the condition is never true; the function always returns the list head and writes `sequence = maxSequence+1` into it. Dead on the server, but the evidence is recorded for a future client.
10. **[O] Duplicated leaf helpers**: 391 tagged leaf functions collapse to 89 distinct byte sequences (e.g. 46 byte-identical `Vec3_Copy` integer-move copies, 28 `Vec3_Subtract`, 27 `Vec3_Scale`, 26 `Vec3_MA`, 20+ `DotProduct`). They are per-translation-unit copies of header inlines, not different algorithms. One canonical implementation per family suffices if the numeric contract in section 8 is honoured.
11. **[O] Several "void" helpers return values in st(0)** and are mis-typed/mis-named in chunks (`Vec3_DeltaProcess` = Vec3_Distance, `Vec3_LengthDiscard` = Vec3_Length, `Float_SelectChainDiscard` = Float_Clamp, `Snd_SetFlagByte/GetFlagByte` are not sound). Section 7.
12. **[O] Orient layout contradiction**: two different 0x30-byte frames exist (origin-first `Orient`, axis-first `game_orient_s`/Mat34). See structs.json.

## 2. Architecture as implemented in this binary

### 2.1 Botlib remnant: precompiler and script lexer [O/I]

- Address range 0x0804a710-0x08052100 (chunks 001, 002, 100). Name evidence is lineage only ([I]): error strings match id botlib `l_precomp.c`/`l_script.c` ("misplaced #elif", "found # without name", "unknown precompiler directive %s", "expected float value, found %s", "^1Error: file %s, line %d: %s\n", "^1Error: file %s still open in precompiler").
- Allocator: `GetMemory` (0x0804a784) = `Com_MallocZeroed(size+4)` + id word 0x12345678; `FreeMemory` (0x0804a7fc) frees only blocks with that word. `GetClearedMemory` does not NULL-check (observed).
- Layers: `PS_*`/`LoadScriptFile`/`LoadScriptMemory` (script_t, 0x4a4 bytes) lex one buffer; `PC_*`/`LoadSourceFile` (source_t, 0x4c8) add `#define/#if/#include/#evalint/$evalint`, built-in `__LINE__/__FILE__/__DATE__/__TIME__` (kinds 1-4), global defines (head 0x08188c84), and a 63-slot handle table `g_08188ca0[64]` (index 0 unused) used by `PC_LoadSourceHandle` (0x0804f9e6), `PC_FreeSourceHandle`, `PC_ReadTokenHandle`, `PC_SourceFileAndLine` (0x0804fc42), `PC_CheckOpenSourceHandles` (0x0804fcdc).
- Token numeric values use x87: `token_t.floatvalue` is a 12-byte `long double` at +0x40c; `NumberValue` (0x0805068e) builds floats as `v*10+digit` / `digit/10^k` in extended precision; `pc_value_t.floatvalue` in the expression evaluator is a `double`; `#evalfloat` prints `%1.2f` of `Float_Abs(value)` (+ explicit "-" token for negatives).
- Directive tables: handlers at `.data` 0x817d3c0 (name, func) and the `$` table; punctuation table at 0x0817d500 (terminated by p==NULL); includes search `source->includepath` (64 bytes at +0x40).
- Entry points: none from the rest of the binary. Intra-library only: `LoadSourceFile` (0x0804f688) <- `PC_LoadSourceHandle`; `LoadScriptFile` (0x08051bd8) <- `LoadSourceFile`; `LoadScriptMemory` (0x08051d20) <- `LoadSourceFile`, `PC_DefineFromString`. [O] zero external callers; liveness through function pointers [U].
- Related but separate: the GSC compiler/lexer (`script/lexer`, `script/compiler`) is a different, hand-written implementation and does not use this library [I].

### 2.2 Server test clients ("bots") [O]

- GSC builtin `addtestclient` (table entry 0x081885bc -> 0x08116bd8) calls `SV_AddBot` (0x080913da) then the client-begin path (0x0811b150).
- `SV_AddBot`: finds a free slot (`state == 0`), formats `connect "\cg_predictItems\1\cl_punkbuster\0\cl_anonymous\0\color\4\head\default\model\multi\snaps\20\rate\5000\name\bot%d\protocol\%d"` with counter `g_083e2f0c` and protocol 0x76 (118), tokenizes it, builds a zeroed `netadr` with `port = counter`, calls `SV_DirectConnect`, finds the client by address, sets `isBot`, `SV_SendClientGameState`, `SV_ClientEnterWorld(cl, &zero_usercmd(0x1c))`.
- `SV_RunBotClients` (0x08096952, called from `SV_Frame` before `SV_CalcPings` when the frame is due): for every client with `state != 0` and `netchan.remoteAddress.type == 0` calls `SV_BotRandomUserCmd` (0x0809676c).
- `SV_BotRandomUserCmd`: zeroed 0x1c usercmd; `weapon` byte (+8) copied from game-client byte +0xd4; if `Client_GetUnk26b4(clientNum) == 0`: `buttons |= 1` with p<0.5, `|= 0x28` with p<0.5, forwardmove = 0x7f (p<0.33) else 0x81 (p<0.5 of the remainder), rightmove likewise (+0x18/+0x19), each of 3 view angles (int, usercmd +0xc/+0x10/+0x14) gets `(int)(360.0f * Rand_Signed())` with p<0.33. Every probability test consumes a separate `Rand_Fraction()` (libc `rand()/2^31`). Then `cl->deltaMessage = outgoingSequence - 1` and `SV_UpdateClientUsercmd`. The draw order is observable only with a controlled libc seed.

### 2.3 Sound on the server [O unless marked]

Pipeline:

1. **Map load**: `SV_SpawnServer` (0x08093520) calls `Com_LoadSoundAliases("maps/mp/<map>.<bspvariant>", "all_mp", bank 2)` (call at 0x080938a2; "all_mp" at 0x814b04b); `SV_ClearServerState` (0x080924b6) calls `Com_UnloadSoundAliases(2)`.
2. `Com_LoadSoundAliases` (0x080ad0aa): strips `maps/mp/` or `maps/` and the extension, lower-cases the name; bank 1 + `com_sv_running` copies bank 2 over bank 1 (listen-server case; never taken on a dedicated server [I]); otherwise `FS_ListFiles("soundaliases","csv")`; zero files -> prints `WARNING: can't find any sound alias files (soundaliases/*.csv)` and returns *without* setting the bank flag; else `Hunk_PushLowPermanent`, `Com_ClearBuildSoundAliases`, `Com_LoadSoundAliasFile` per file, `Com_MakeSoundAliasesPermanent`, `Hunk_ClearTempMemory`, `Hunk_SetLowPermanent`, sets `g_085a9840[bank] = 1`.
3. `Com_LoadSoundAliasFile` (0x080ae90c): reads `soundaliases/<file>`; first use loads `soundaliases/volumemodgroups.def` (header "VOLUMEMODGROUPS", max 32 groups of name+float, file < 0x2000 bytes) and lazily registers console command `snd_refreshVolumeModGroups` (sets the dirty flag 0 so the file is re-read on the next load). CSV via `Com_BeginParseSession`/`Com_SetCSVMode(1)`/`Com_Parse`; blank rows and rows starting with `#` skipped; header row maps cells to column ids by `Q_stricmp` against the column table at 0x817e340; header must contain `name` and `file` or "missing 'name' and/or 'file' columns".
4. Column semantics (`Com_ParseAliasColumn` 0x080adcc8; table index = column id; [O] unless noted): 1 name (valid-name check, < 0x3f chars), 2 sequence (atoi), 3 file, 4 subtitle (no bytes >= 0x80), 5 vol_min / 6 vol_max (0..1, vol_max defaults to vol_min when absent), 7 vol_mod (volume mod group lookup, fatal `Com_Error` if missing), 8 pitch_min / 9 pitch_max, 10 dist_min / 11 dist_max, 12 channel, 13 type (streamed=2, primed=2, loaded=1), 14 loop (looping/nonlooping), 15 probability, 16 loadspec, 17 masterslave ("master" or a 0..1 slave percentage), 18 secondaryaliasname, 19 volumefalloffcurve (`Com_FindFalloffCurve` is a stub returning NULL), 20 startdelay (atoi), 21 speakermap (accepted, ignored), 22 reverb, 23 "lfe percentage" (clamped 0..1 with `Float_SelectChainDiscard`). Duplicate column in a row is an error. Channels accepted (index 0-10): auto, auto2d, menu, weapon, voice, item, body, local, music, announcer, shellshock (table at 0x817e3a0 also holds "off" at index 11, not accepted).
5. `Com_ValidateSoundAlias` (0x080ae282): swaps pitch/vol min/max if reversed; errors for pitch_min<=0, vol_min<0, dist_min<=0, dist_max<dist_min; `dist_max==0` becomes `dist_min*5`; if `vol_mod != 1` multiplies and clamps both vols to 0..1. Rows with errors are reported with `^1ERROR:` and dropped.
6. `Com_MakeSoundAliasesPermanent` (0x080af224): merge-sorts the build list by name, then by (file, type, name) to dedupe sound-file records (warning "sound file '%s' used as %s in alias '%s' and %s in alias '%s'" for same file with different types), sizes and hunk-allocates `aliases[count*0x44]`, `soundFiles[unique<<4]`, a string pool, then a `snd_alias_list` node per distinct name inserted with `SND_InsertAliasList` (duplicate name -> `^1ERROR: alias '%s' already added - ignoring`).
7. Lookup: `SND_FindAliasList` (0x080acc42): `tolower` hash (`h = c + h*0x7a69`, `& 0x3ff`, 0x7a69 = 31337) into `g_085a9848[1024]`, `Q_stricmp` chain. **GSC `soundexists(name)`** (0x08111cfa) = `SND_FindAliasList(name) != NULL` pushed as an int.
8. **Playback requests** (no alias resolution on the server):
   - `ent playsound(alias)` 0x08111d2c: `idx = (byte)soundIndex(alias)` (0x0811d668, creates the configstring `0x24e+i` = alias name if absent; fatal `Com_Error` when full), `e = G_TempEntity(ent.origin(+0x138), 0xb3)`, `e.svFlags(+0xf2) |= 8`, `e.eventParm(+0xa0) = idx`. `playsoundasmaster` identical with event 0xb4. `G_TempEntity` (0x0811efc4) creates an entity of type `event+10`, truncates the origin to integers via truncating `fistp`, `time = level time`.
   - `playloopsound(alias)` 0x08111e0c: `ent+0x100 = -1; ent+0x84 = idx`. `stoploopsound` 0x08111e5a: `ent+0x100 = levelTime+300; ent+0x84 = 0` (offset 0x100 is `broadcastTime` in chunk 022's sharedEntity) [I: keeps the entity broadcast for 300 ms after the loop stops].
   - `player playlocalsound(alias)` (0x080fd990): `SV_SendServerCommandNum(clientNum, reliable=0, "s %i" idx)`.
   - Global commands (reliable, clientNum -1): `musicplay(alias)` -> `"o <alias>"`; `musicstop([sec])` -> `"p <ms>"` where ms = `Float_RoundHalfUp(1000*sec)`; `soundfade(vol[,sec])` -> `"q %f %i\n"` with `ms = (int)(1000.0*sec)` (truncating, unlike musicstop); `musicstop` errors "fade time must be >= 0".
   - Ambience through configstring 3: `ambientplay(alias[,sec])` -> `"n\<alias>\t\<fadeMs + levelTime>"`; `ambientstop([sec])` -> `"t\<fadeMs + levelTime>"`; empty alias and negative fade are script errors. `fadeMs = Float_RoundHalfUp(1000*sec)`; `levelTime` is `g_0864c56c`.
   - Player DSP methods (chunk 103): `setreverb` -> `"r %i \"%s\" %g %g %g"` (priority 1/2, room, dry default 1.0, wet default 0.5, fade); `deactivatereverb` -> `"D ..."`, `setchannelvolumes` -> `"E %i %i %g"` (priority 1-3, **shellshock configstring index** via `G_FindConfigstringIndex(name,0x48e,0x10,create=0)`, fade), `deactivatechannelvolumes` -> `"F ..."`. Priorities are compared against interned script strings `snd_enveffectsprio_level/_shellshock` and `snd_channelvolprio_holdbreath/_pain/_shellshock` (globals 0x088532a2..0x088532aa). **[O quirk]** `deactivatereverb`/`deactivatechannelvolumes` reuse the 6-conversion format `"%c %i \"%s\" %g %g %g"` but pass only (cmd, priority, fade-as-double); `%s` then consumes the low word of the fade double (0 -> glibc prints "(null)" for the default fade 0) and the `%g`s read whatever follows on the stack. The exact text on the wire is therefore stack-dependent [U].
9. **Dead sound code [O: no callers]**: `Com_PickSoundAlias` (only via the unreferenced wrapper `SND_PickAliasByName` 0x080ad47a), `SND_GetAliasIndex` 0x080accb2, `SND_GetAliasWithOffset` 0x080acd0e (Com_Error text "SND_GetAliasWithOffset: could not find sound alias '%s' with offset %d"), `SND_GetAliasListName` 0x080ad2e2/0x080acf36/0x080acf44, `SND_InitAliasCurves` 0x080acf52 (`.vfcurve` loader, max 15 curves), `Com_DuplicateSoundAlias` 0x080ace1c, `Com_LocalizeSoundAliasSubtitles` 0x080b0fd8 and the StringEd subtitle writers, `Snd_RegisterErrorOnMissing` 0x080d6c7c (dvar `snd_errorOnMissing`, bool, flags 0x1001, global 0x0860b3ec, never read).

### 2.4 Math library [O]

- Vector/matrix/angle/plane/bounds/quaternion code lives mainly at 0x080a3748-0x080ab3fc (chunks 025/026/101), with ~380 duplicated leaf inlines elsewhere (family table in appendix B).
- libm is used through the PLT in **double** precision: `sin`, `cos`, `tan`, `atan`, `atan2`, `asin`, `acos`, `sqrt`, `floor`, `ceil`, `rint`, `log` (PLT 0x08049d74 cos, 0x08049e14 atan, 0x08049eb4 asin, 0x0804a1e4 atan2, 0x0804a214 floor, 0x0804a354 sqrt, 0x0804a604 sin, 0x0804a2e4 tan, 0x0804a0e4 rint, 0x0804a644 log). The whole `.text` contains zero `fsqrt/fsin/fcos/fpatan/fptan/fyl2x/f2xm1` instructions (grep over the full disassembly: 0 hits), so every transcendental and square root goes through libm; `sqrt` of a float is `sqrt((double)x)` stored to float (`Float_Sqrt` 0x080aaebe, 0x080553b2, ...). Results therefore depend on the glibc `libm` behaviour (the oracle uses the host i386 glibc).
- Degree/radian constants: 0.017453292519943295 (double) at 0x814d0e8/0x815c078/0x815ae48/0x815edc8, 180.0 (0x814d0b0), pi 3.141592653589793 (0x814d0b8), 57.29577951308232 (0x815c0b8), 182.04444885253906f / 0.0054931640625f for AngleMod (0x814d1e0/e4), 511.0f for 10-bit direction packing (0x814d050).
- 162-entry unit-normal table `g_0817dba0` (3 floats each, "bytedirs") used by `Normal_DirToIndex` (0x080a3f8e, best dot > 0, returns 0 for NULL) and `Normal_IndexToDir` (0x080a4010, out-of-range -> zero vector `g_0814cfc8`); used for impact events (`temp->rnd_a0 = Normal_DirToIndex(trace.normal)`) [O].
- `Vec3_ToAngles` (0x080a4fe2, used by GSC `vectortoangles`, playFx, mover/projectile code): zero xy -> yaw 0, pitch 270 if z>0 else 90; otherwise `yaw = 180*atan2(y,x)/pi` (+360 if <0) and `pitch = -180*atan2(z, hypot_float)/pi` (+360 if <0) computed as double `atan2` * double 180 / double pi in extended, stored to float; angles[2] = 0. The "raw" variant (0x080a5122) skips the wrap (pitch -90/90).
- `Angles_ToVectors` (0x080a5226): angle floats are first multiplied by the double 0.017453292519943295 and **rounded to float**, then `SinCos_Float` (0x080aaf80: `sin((double)f)`, `cos((double)f)` each stored to float). Right vector uses `right = ( cr*sy - sr*sp*cy, -cr*cy - sr*sp*sy, -sr*cp )`, up = `( sr*sy + cr*sp*cy, -sr*cy + cr*sp*sy, cr*cp )`; forward = `(cp*cy, cp*sy, -sp)`.
- `Vec3_Normalize` (0x080a4424): `len = (float)sqrt((double)(float)(x*x+y*y+z*z))`, zero is left unchanged, **NaN length is not treated as zero** (`jne/jp` falls into the scaling path); `inv = (float)(1.0/len)` (extended divide, float store) and each component is `float(v*inv)`. Returns the original length. Same shape for Vec2/Vec4 and the copy variant (zero input -> zero output).
- Angle helpers (chunk 026): `AngleMod` 0x080a8432 `= 0.0054931640625f * (float)(((int)(a*182.04444885253906f)) & 0xffff)` with a truncating `fistp` and the product computed in extended; `AngleSubtract`/`LerpAngle`/loop-normalizers use +-360 loops (a NaN or huge value is a hang risk in the loop variants [I]).
- Round/convert helpers: `Float_RoundToInt` family (0x080aaf4a, 0x08062b2e, 0x081185e6, 0x08085744, 0x08104a34, 0x081081ee, 0x080b570e, 0x0809e470, 0x081294a6): `fld x; fld 0.5f; faddp; fstp qword; call floor; fistp(trunc)`. `Q_ftol` 0x080d5e6e = truncating `fistp`. `Sys_SnapVector` 0x080d5e94 = libm `rint` per component (current rounding mode = nearest-even), used by `PM_SingleStep`.
- Select/fsel emulation: `Float_Select(test,a,b)` = `test >= 0 ? a : b` via `fucompp/sahf/jae` (NaN -> b, -0.0 -> a); `Float_Max(a,b) = select(a-b, a, b)`, `Float_Min(a,b) = select(b-a, a, b)` (ties return `a`, any NaN returns `b`; not IEEE max/min); `Float_Abs` = `fabs` stored to float; `Float_Clamp(x, lo, hi)` (0x080b11e8, misnamed) = `select(lo - x, lo, select(x - hi, hi, x))`, i.e. `(lo >= x) ? lo : min(x, hi)`.
- Distance/length: `Vec3_Length` = float `sqrt` of the x87 sum x²+y²+z² (x first, stored to float before sqrt); `Vec3_DistanceSquared` (0x080a4116) returns the extended-precision sum **unrounded** (declared `float` in some chunks); `closer(o,a,b)` compares two such values after float stores.
- Dot product: 20+ byte-identical copies compute `((a0*b0 + a1*b1) + a2*b2)` and return it in st(0) without rounding to float (declared `float` in chunks 003/059/051, `long double` in 005/034/040/041/052). Machine code identical, only the declared C type differs. For ports: return `double`/extended, not `float`.
- `VectorMA`/`Vec3_MA` copies: `out = a + s*dir` per component (product exact in extended, one extended add, one float store); `Vec3_Scale` = `v*s` stored; `Vec3_Add/Sub` stored per component; `Vec3_Copy`/`Vec3_Set*_Bits` move 32-bit integers (NaN payload/sign preserved).
- Matrix helpers (chunk 025): `Mat3_*`, `Mat34_Concat` (3x3 block rows 0-2 + translation elements 9-11), `Mat4_Multiply` (row-major 4x4, k ascending in the binary), `Mat3_ToQuat`, `Quat_*`, perspective/orthographic builders. Quaternion layout (x,y,z,w) with `quatScale = 2/|q|^2` in the 8th float of a bone (chunk 031 `DObjBone`).
- Spherical/disk sampling, golden-spiral distributions, `Rand_GaussianPair` (Marsaglia polar with `log`), `Vec2_DiskPoint`, `Vec3_SpherePoint/HemispherePoint`, `Plane_FromPoints`, `Point_InArcSector` (chunk 101): [O] no direct callers (client/FX-side math linked into the server image).

### 2.5 Random number generators [O]

| RNG | State | Algorithm | Seeding | Consumers |
|---|---|---|---|---|
| glibc `rand()` (PLT 0x0804a634) | libc | glibc TYPE_3 additive feedback | `srand(randomSeed)` in `G_InitGame` (0x08109096, seed = `Sys_MillisecondsRaw()` passed by `SV_InitMapState` 0x080925e6), `srand(arg)` in `Pool_ItemCallback` (0x08109082, same value is also given to `Rand_SetSeed`), `srand(Sys_MillisecondsRaw())` in `SV_SpawnServer` | `Rand_Fraction` 0x080a3774 (`(long double)rand()/2147483648.0f`, unrounded) and `Rand_Signed` 0x080a3792 (`2f-1` unrounded) -> bot usercmds, `Vec2_RandomDirection` (bullet spread, `G_ComputeFovViewOffset`), `PM_Weapon_ComputeSwayOffset`, `player_die`, `Item_PickupWeapon`, `Player_DropWeapon`, `Item_ThrowFromPlayer`, `G_PlayerCollisionPush`, `FxObj29c_RandomizeValue`; direct `rand()`: `Bullet_FireFromEntity` (`rand() % 5 + range` damage), `Bullet_Fire_Extended`, `G_FindEntityByStringField`, `BG_AnimScript*` (3 sites), `SV_RequestIpAuthorize`, `FS_FOpenFileReadInternal` |
| LCG A at 0x0817e338 | u32, initial 0x89abcdef | `s = s*0x343fd + 0x269ec3`; `r = s >> 17` (15 bits) | `Rand_SetSeed` 0x080aaa76 from `G_InitGame`, `Pool_ItemCallback`, **and the FX scheduler** (`FxScheduler::PlayEffect`, `Fx_ProcessPendingEvents`) | `Rand_Float(lo,hi)` 0x080aaa84 = `((hi-lo)*(float)r)/32768.0f + lo` (extended chain, one float store); `Rand_Int(lo,hi)` 0x080aaada = `(((hi-lo)*r) >> 15) + lo` with 32-bit signed `imul` (overflows when `hi-lo > 65536`); GSC `randomint/randomfloat/randomintrange/randomfloatrange` (an xref scan attributes their call sites 0x08113cee/0x08113d22 to `GScr_bulletTrace` only because the builtins lacked symbol entries), `Pool_ItemCallback`, `G_SpawnGrenade`, `Bullet_Fire_Extended`, FX code (`FxSpawner_*`, `FxRange_Random`, `Cloud::InitRandomAxis`, `FX_SpinAxes`, `FxPickList_PickOut`) |
| LCG B at 0x085a9844 | u32, initial 0 (bss) | `s = s*0x343fd + 0x269ec3`; `r = (s >> 16) & 0x7fff` | `Com_SetRandSeed` 0x080ad31c, no callers | `Com_PickSoundAlias` only |

GSC `randomint(n)`: `n < 1` -> prints `RandomInt parm: %d` and `Scr_Error("RandomInt parm must be positive integer.\n")`; else `Rand_Int(0, n)`. `randomfloat(max)` = `Rand_Float(0, max)` (no range check). `randomintrange(lo,hi)` requires `hi > lo`; result in `[lo, hi)`. `randomfloatrange(lo,hi)` requires `hi > lo`, error text prints two floats with `%d` (format bug, observed). [O]

## 3. Entry points and call flow

| Entry | Address | Calls / effect |
|---|---|---|
| `SV_SpawnServer` | 0x08093520 | srand(ms); checksumFeed; `Com_LoadSoundAliases(map, "all_mp", 2)` @0x080938a2 |
| `SV_InitMapState` | 0x080925e6 | `G_InitGame(svs.time, Sys_MillisecondsRaw(), ...)` -> `srand`, `Rand_SetSeed` |
| `SV_ClearServerState` | 0x080924b6 | `Com_UnloadSoundAliases(2)` |
| `Com_LoadSoundAliases` | 0x080ad0aa | `Com_ClearBuildSoundAliases` 0x080ae5b6, `Com_LoadSoundAliasFile` 0x080ae90c, `Com_MakeSoundAliasesPermanent` 0x080af224 |
| GSC `soundexists` | 0x08111cfa (table 0x081886f4) | `SND_FindAliasList` 0x080acc42 |
| GSC entity methods playsound/playsoundasmaster/playloopsound/stoploopsound | 0x08111d2c/0x08111d9c/0x08111e0c/0x08111e5a (table 0x08188820..) | sound index 0x0811d668, `G_TempEntity` 0x0811efc4 |
| GSC `musicplay/musicstop/soundfade/ambientplay/ambientstop` | 0x08114734/0x08114778/0x08114824/0x08114a3c/0x08114b14 | `SV_SendServerCommandNum` 0x080917aa, `SV_SetConfigstring(3, ..)` 0x08092780 |
| GSC math builtins | randomint 0x08113ca4, randomfloat 0x08113cfe, randomintrange 0x08113d32, randomfloatrange 0x08113da0, sin 0x08113e16, cos 0x08113e48, tan 0x08113e7a, asin 0x08113ede, acos 0x08113f58, atan 0x08113fd2, int 0x0811400a, distance 0x081140ce, distancesquared 0x08114116, length 0x0811415e, lengthsquared 0x0811418c, closer 0x081141ba, vectordot 0x08114240, vectornormalize 0x08114288, vectortoangles 0x081142ce, anglestoup 0x08114306, anglestoright 0x0811434e, anglestoforward 0x08114396 | table 0x08188238-0x08188334 |
| `addtestclient` | 0x08116bd8 | `SV_AddBot` 0x080913da |
| `SV_Frame` | 0x080969b0 | `SV_RunBotClients` 0x08096952 -> `SV_BotRandomUserCmd` 0x0809676c |

GSC builtin dispatch tables (`name, handler, type` records, stride 12): functions at 0x08188040 (global) and entity methods at 0x08188720 (e.g. `playsound` entry at 0x0818881c).

GSC math builtin details [O]:
- `sin/cos(deg)`: `sin(0.017453292519943295 * (double)deg)`, argument product in extended rounded to double, result `(float)`. `tan(deg)`: `Float_SinCos((float)(const*deg))` then `sin/cos` divided in float; `cos == 0.0f` exactly -> script error "divide by 0". `asin/acos(x)`: range-check `-1 <= x <= 1` else `"%g out of range"`, result `(float)(57.29577951308232 * (float)asin(x))` (note the intermediate `(float)` of the libm result). `atan(x)`: same with `atan`.
- `int(x)`: param type 5 (float) -> truncating `fistp`; 2 (string) -> `atoi`; 6 (int) -> identity; otherwise `Scr_ErrorAtParam(0, "cannot cast %s to int")`.
- `distance(a,b)` = `Vec3_Length(b-a)`; `closer(o,a,b)` = `distSq(a,o) < distSq(b,o)`; `vectordot` = Vec3 dot; `vectornormalize` copies then `Vec3_Normalize` (zero vector stays zero); `anglestoforward/right/up` = `Angles_ToVectors` with one output.
- `wait` (VM, chunk 016): `ticks = floor((double)(wait*20.0f) + 0.5)` truncated to int (0x08085744), forced to 1 when the float is non-zero but rounds to 0 [I for the multiply type, O for the rounding function].

## 4. Global state (addresses, layouts)

| Address | Meaning | Evidence |
|---|---|---|
| 0x085a9848 | `snd_alias_list*[0x400]` hash buckets (memset 0x1000 on unload) | [O] 0x080acbf8/0x080ad274 |
| 0x085a9840 | per-bank "aliases loaded" bytes [4] | [O] |
| 0x085aa848 / 0x085aa878 | alias banks [3] stride 0x10 {_, aliases*, count, _} / aux [3] stride 8 {fileCount, files*} | [O] `Com_MakeSoundAliasesPermanent(&bank, &aux)` |
| 0x085aa8d0 / 0x085aa8d4 / 0x085aad94 | curves-initialised flag / 16 curve slots x 0x48 (slot 0 default) / 15 curve-name slots x 0x40 | [O], never initialised on the server (no caller of `SND_InitAliasCurves`) |
| 0x085ab160 / 0x085ab164 | build-list head / count | [O] |
| 0x085ab168 | volume-mod groups [32] x 0x44 (name[0x40] + float) | [O] |
| 0x0817e340 | csv column-name pointer table (idx 0 NULL, 1..23: name, sequence, file, subtitle, vol_min, vol_max, vol_mod, pitch_min, pitch_max, dist_min, dist_max, channel, type, loop, probability, loadspec, masterslave, secondaryaliasname, volumefalloffcurve, startdelay, speakermap, reverb, "lfe percentage") | [O] |
| 0x0817e3a0 | channel-name pointer table (auto, auto2d, menu, weapon, voice, item, body, local, music, announcer, shellshock, off) | [O] |
| 0x0817e338 / 0x085a9844 | LCG A (init 0x89abcdef) / LCG B (init 0) | [O] |
| 0x0817dba0 | 162 unit normals (float[162][3]) | [O] `Normal_DirToIndex` loop `i < 0xa2` |
| 0x0814cfc8 | float[3] zero vector | [O] |
| 0x0864c56c | game level time (set by `G_InitGame`) used for ambient/loop timestamps | [O] |
| 0x0864c39c | "allow create/level-load" gate used by `precache*` and `G_FindConfigstringIndex` callers | [O] 0x081148ec |
| 0x083e2f0c | test-client counter (name suffix and fake port) | [O] |
| 0x08188ca0 | botlib source-handle table [64] | [O] |
| 0x08188c84 / 0x08188c80 | global defines head / live token count | [O] |
| 0x088532a2..0x088532aa | interned script strings for reverb/channel-volume priority names | [O] |

Configstring bases seen through `G_FindConfigstringIndex` (0x0811d300): items 0x6e/0x20, models 0x14e/0x100 (`G_ModelIndex`), **sounds 0x24e/0x100** (0x0811d668), effects 0x34e/0x40, **shellshock 0x48e/0x10** (0x0811d634), localized strings 0x51e/0x100, shaders 0x61e/0x80; ambient stream uses fixed index 3; index 1-based scan, case-insensitive (`strcasecmp`), index 0 reserved ("" returns 0).

## 5. Data formats and protocols

- **soundaliases/*.csv** (read through the FS layer, so iwd + disk): header row + data rows, comma separated with the engine tokenizer in CSV mode; columns as in 2.3. Output records: `snd_alias_t` (0x44 each), `snd_alias_list_t` (0x10), `snd_file_t` (0x10: name, useCount, reserved, loadType). See structs.json for the reconciled field layout.
- **soundaliases/volumemodgroups.def**: text `VOLUMEMODGROUPS` then `{ name value ... }`, < 0x2000 bytes, <= 32 groups; errors are fatal (`Com_Error(1, ...)`).
- **soundaliases/*.vfcurve** (client path only on this binary, unused): first token = knot count (2..8), then x y pairs in 0..1; first/last knots are forced to (0,1) and (1,0) with a `^3WARNING`.
- **Server -> client sound traffic**: server command letters `o p q s r D E F` (above), event entities 0xb3/0xb4 with `eventParm` = sound configstring index (byte), `loopSound` (+0x84) index, configstring 3 for ambience, configstrings 0x24e.. for alias names.
- **Direction quantization**: 162-normal index (byte) in impact events; 10-bit-per-axis packed direction (`Vec3_PackDir10` 0x080a472c/`Vec3_UnpackDir10` 0x080a47a2, x*511 rounded by `Float_RoundToInt`; no callers on the server [O]).

## 6. Dvars and commands registered by this subsystem

- Console command `snd_refreshVolumeModGroups` (handler 0x080ae900, registered lazily by 0x080ae90c at first alias file load). [O]
- Dvar `snd_errorOnMissing` (bool, default 0, flags 0x1001) at 0x080d6c7c, never registered at runtime (no caller) and never read. [O]
- GSC builtins listed in section 3 (not dvars). No botlib dvars (`bot_*`, `botlib`) exist in the binary. [O: no matching strings]
- Dvars consumed: `com_sv_running` (0x080ad0aa), `sv_maxclients` (bots), `gfx_driver` (BSP variant suffix of the alias map path, via `Gfx_GetBspVariant`). [O/I]

## 7. Naming problems found (evidence)

| Where | Current name | Problem | Evidence | Proposed |
|---|---|---|---|---|
| 0x0811d634 (symbols.json chunk 052) | `G_SoundIndex` | actually shellshock configstring index | base 0x48e max 0x10; caller `precacheshellshock` 0x081148e6 (`call 0x811d634` @0x8114910); chunk 103 `setchannelvolumes` passes a shock name to the same range | `G_ShellShockIndex` |
| 0x0811d668 | `G_AnimIndex` | actually the sound index | base 0x24e max 0x100 returns byte; callers `playsound`, `playsoundasmaster`, `playloopsound`, `playlocalsound`, `Pool_ItemCallback` (x4), `Ent_AddEventB4IfNonzero`; no caller passes an animation name; chunk 105 already notes "used as a sound alias index" | `G_SoundIndex` |
| 0x080a40ea / 0x080a41e0 (chunk 025) | `Vec3_DeltaProcess` / `Vec2_DeltaProcess` (void) | return the length of the difference in st(0) | `call Vec3_Sub; call Vec3_Length; leave; ret` (0x80a40ea..); `GScr_distance` uses the return value (chunk 105 casts the pointer to float-returning) | `Vec3_Distance` / `Vec2_Distance`, return float |
| 0x080dc54a (chunk 040), 0x08120ff6 (chunk 053) | `Vec3_LengthDiscard` | is `Vec3_Length` (sqrt result left in st0) | disassembly ends `fstp [esp]; call sqrt-wrapper; leave; ret` | `Vec3_Length` |
| 0x080b11e8 (chunk 028) | `Float_SelectChainDiscard` (void) | is `Float_Clamp(x, lo, hi)`; chunk 027 already uses the return as a clamp | two nested `Float_Select` calls: inner `select(x-hi, hi, x)` (args `a-c`, `c`, `a`), outer `select(lo-x, lo, inner)`; result `(lo >= x) ? lo : min(x, hi)` | `Float_Clamp` |
| 0x080b1250 / 0x080b1268 (chunk 028) | `Snd_SetFlagByte` / `Snd_GetFlagByte` (sound/state) | not sound: setter writes byte 0x085b7655 = "dvar set-from-config gate" read by `Dvar_SetFromUserInput` (0x080b5045); getter reads byte 0x085b7654 = "sv_cheats registered" set by `Dvar_InitSvCheats` (0x080b513e); the setter is wrapped around `Cbuf_Execute` (`Cbuf_ExecuteWithLoadFlag` 0x08062010) and `Com_ErrorRecover` | xrefs to 0x85b7654/5 | `Dvar_SetSetterGate` / `Dvar_GetSvCheatsReady` |
| 0x08084bb2 `Scr_ReleaseAnyValue` | misleading | is `Scr_GetString` (returns `char*`: `ScrStr_TextFromHandle(Scr_GetAny(i))`); used as the string argument getter by every sound builtin | chunk 017 body | `Scr_GetString` |
| 0x08085164 `Scr_AddIndex` | misleading | byte-identical to `Scr_AddInt` (0x08085140); used to push ints and bools (`closer`, `int()`, `randomint`) | chunk 017 comment | alias of `Scr_AddInt` |
| 0x080ad47a (chunk 101) `SND_PickAliasByName` | ok | unreferenced; `SND_GetAliasListName` appears three times (0x080acf36 holder form, 0x080ad2e2 name form) with different signatures | | disambiguate |
| 0x080ad49c `Com_FindFalloffCurve` | name implies lookup | stub, returns 0 for every curve name (so every runtime alias gets a NULL curve) | disassembly `mov eax,0; leave; ret` at 0x080ad49c; chunk 027 | `Com_FindFalloffCurve_Stub` |
| 0x08051928 etc. | `PS_*`/`PC_*` | fine (lineage) | strings | keep |
| `Float_RoundToInt` x8, `Float_Max` x15, `Vec3_Copy` x46 ... | duplicate names for the same body in different chunks | each chunk defines static copies; linkers of the reconstruction would collide | clustering by bytes (appendix B) | one canonical symbol per family |

## 8. Behaviours a native port must reproduce

1. **Extended-precision evaluation.** Emulate x87 `float` semantics: keep expression temporaries in 80-bit (or at least double with care), round to float only at stores and float-typed call/return boundaries. On macOS arm64 `long double` is 64-bit, so a faithful port needs either `double` plus targeted fixes or a small software-extended type for the few chains where it matters (sum of products, `1.0/len`, `Rand_Float`, `AngleMod`, spread math). I have not quantified how often double vs extended differs; the rounding-sensitive spots are listed in section 9.
2. **libm and libc parity.** `sin/cos/tan/atan/atan2/asin/acos/sqrt/log` double precision (glibc), `rint` nearest-even, `floor`, `rand()/srand()` = glibc TYPE_3 generator. Replicating glibc's `rand_r`-style algorithm explicitly is required for deterministic bot input/spread against the oracle (seed must then be injected; in the original it is wall-clock milliseconds).
3. **Float -> int conversions**: C casts are truncation (`fistp` with RC=chop around the call); `Sys_SnapVector` is the only nearest-even (`rint`). "Round" = `floor(x + 0.5)` in extended without an intermediate float rounding.
4. **RNG streams**: keep the three generators separate, seed both libc and LCG A from the same value at `G_InitGame`, let FX-driven `Rand_SetSeed` calls remain if FX code is ported to the server (open question 1). `Rand_Int` must use 32-bit signed multiply with `>> 15` arithmetic shift.
5. **GSC math/sound builtin surface**: argument validation texts, result types (float vs int), `soundexists` semantics, ms conversion differences (`soundfade` truncates, `musicstop/ambient*` round-half-up), and the exact server-command strings including the trailing `\n` in `soundfade`.
6. **Sound configstrings**: case-insensitive 1-based scan, creation in place, `Com_Error` when the 256-entry range (16 for shellshock) is full, index 0 for empty name; the sound index is truncated to a byte for events and commands.
7. **Entity sound state**: `loopSound` (+0x84), broadcast time (+0x100: -1 while looping, `levelTime+300` after stop), `eventParm` (+0xa0), svFlags bit 3 on playsound events; `G_TempEntity` truncates the origin to integers (event type = event+10).
8. **Alias loader** (needed for `soundexists` and for load-time validation messages): column set, defaulting (`dist_max==0 -> 5*dist_min`, vol/pitch swap, `vol_mod` scaling and clamps), error texts, merge/dedupe rules, name hash and case-insensitive lookup, volumemodgroups parsing limits, loadspec filter (`Com_ParseAliasLoadSpec` 0x080ada4c: the value is a list of map names; a leading `!` makes it an exclude list; a whole-token match of the lower-cased map name returns the list's polarity; otherwise the result is flipped unless the value equals the server loadspec `all_mp`; rows that evaluate to 0 are dropped) [O structure, I intent].
9. **Bot clients** (if test clients are to be supported): fake connect string, fixed protocol 118, random usercmd generator exactly as in 2.2, `isBot`/`netadr.type == 0` handling in `SV_RunBotClients`.
10. **Precompiler**: no behaviour required unless a future GSC/menu path calls it; keep out of the port ([U] whether any data path uses it).

## 9. Reconstruction-source discrepancies (verified against disassembly)

| Chunk / function | Source says | Binary does |
|---|---|---|
| 025 `Vec3_Normalize` 0x080a4424, `Vec2_/Vec4_Normalize`, `Vec3_NormalizeCopy` | `z*z + y*y + x*x`; comment "original order" | `x*x + y*y + z*z` (loads +0, +4, +8 in order) |
| 025 `Vec3_DistanceSquared` 0x080a4116 | `delta[2]^2 + delta[1]^2 + delta[0]^2` "in the original order" | x, y, z order; result returned unrounded |
| 025 `Mat34_ConcatRowVector` 0x080a5ff2, `Mat4_Multiply` 0x080a62c8 ("k = 3,2,1,0"), `Vec3_MulMat3Rows/Cols` 0x080a4882/0x080a492c | highest index first | lowest index first |
| 031 `Orient_PointWorldToLocal` 0x080b8ec6 and siblings; 030 `Frame_TransformPoint` 0x080b8d30 | reversed sums | ascending sums |
| 025/026/040/049/052/053/056 | degree->radian literal `0.017453292` (float-precision text) in `Angles_ToVectors`, `Float_TanDeg` 0x0810de20, `Bullet_ApplySpread` 0x0811fac8, `Vec2_RandomDirection` 0x0811fa70, `G_ComputeFovViewOffset` 0x0812061a, ... | double `0.017453292519943295` (0x814d0e8 etc.), difference ~3e-8 relative |
| 026 `Float_RoundToInt` 0x080aaf4a, 051 0x081185e6, 048 0x08104a34/0x081081ee, 043 0x080eae0c, 055 `FxRoundToInt` 0x081294a6 | `floor((double)(x + 0.5f))` | `x + 0.5` in x87 then spilled as double (no float rounding); chunks 007/018/023/029 write it correctly |
| 045 `Round_To_Int` (used for damage scale, view pitch/roll quantization, "(%i %i %i)" text at 0x080f42da, 0x080f1ede) | half away from zero | truncating `fistp` (0xc00 control word at 0x80f430a, 0x80f2125) |
| 049 (`lrintf` at 0x0810aa50, 0x0810cf4c, 0x0810d1a8, 0x0810de4e) | nearest-even | truncating `fistp` (0xc00 at 0x810cf85, 0x810d21e, 0x810de68, 0x810ada5) |
| 026 `AngleMod` 0x080a8432 | `0.0054931640625f * (float)(((int)(a * 182.04...f)) & 0xffff)` | same math but `a*182.04..` is formed in extended before the truncating `fistp`, and the result is returned unrounded in st(0) |
| 105 `GScr_sin/cos` | `sin(0.01745.. * (double)x)` | product formed in extended then rounded to double (double rounding vs single rounding in C double) |
| 027 `Com_PickSoundAlias` | `rand*accum < prob * -2147483648.0f` | confirmed as printed (never true); documented, not a transcription error |
| 025/059/051/052 `Vec3_Dot`, `DotProduct` | `float` or `long double` returns inconsistently | identical code, unrounded st(0) result |

## 10. Cross-chunk struct conflicts (details in structs.json)

- `snd_alias_t`/`snd_alias_s` and `snd_alias_build_t`/`snd_alias_entry_s`: chunks 027 and 028 disagree on the meaning of offsets 0x30-0x40 (runtime) and 0x130-0x14c (build); 027 is self-consistent and confirmed by the disassembly of `Com_AddSoundAlias` (0x080aefd8: build+0x134 -> alias+0x30, +0x138 -> +0x34, +0x13c -> +0x38, +0x140 -> +0x3c, +0x130 -> +0x40).
- `snd_alias_list`: +0x0c is the hash-chain `next` (027/101), not "reserved" (028).
- `type` values: 028 treats 3 as `primed`; `Com_ParseAliasType` writes 2 for both streamed and primed, 1 for loaded.
- `Orient` (031: origin@0, axis@0xc) vs `game_orient_s` (052: axis@0, origin@0x24): both size 0x30, both verified (0x080b8ec6 vs 0x0811fac8). They are different types.
- 0x20-byte pose/bone record: `QuatTransform_s` (026), `DObjBone` (031) and `XAnimPoseBone` (033) share offsets 0x00 quat[4], 0x10 origin[3]; the float at 0x1c is `quatScale` (026/031) vs `timer` (033).
- `usercmd_t` (022 vs 020): 022 names `weapon`@8, `angles[3]`@0xc, `forwardmove`@0x18, `rightmove`@0x19, confirmed by the random-cmd stores; 020 labels 0x09 and 0x0c..0x1b opaquely.

## 11. Open questions

1. **[U]** Is any FX scheduler path executed on the dedicated server? `FxScheduler::PlayEffect` and `Fx_ProcessPendingEvents` call `Rand_SetSeed` on the same LCG as `randomint`; `GScr_playFx` only spawns an event entity (type 0xc2) and calls no FX code, and no server-path caller of the FX scheduler was found, but vtable/indirect calls are not excluded.
2. **[U]** The exact bytes of `deactivatereverb`/`deactivatechannelvolumes` commands on the wire (stack-dependent varargs).
3. **[U]** Whether `Com_FindFalloffCurve` returning 0 is intentional in the server build or a link-time stub; whether `speakermap`/`reverb` columns ever matter to the server (reverb parsed by `Com_ParseAliasReverb` 0x080adc7e).
4. **[U]** Which clients consume server command letters `o p q r s D E F` (client binary not analysed); letter meanings are inferred from the GSC functions that emit them.
5. **[U]** Intent of the `Com_ParseAliasLoadSpec` post-loop `strcmp(searchFrom, loadSpec)` (compares the unconsumed tail pointer with "all_mp"); behaviour is documented as compiled.
6. **[U]** Whether scripts rely on `randomfloat(max)` with negative or zero `max`: it has no range check [O]; the result is then in `(max, 0]` [I].
7. **[U]** Whether sums in the precompiler/evaluator (`pc_value_t.floatvalue` double vs token long double) matter: no live caller.

## Appendix A: tag census (symbols.json at completion)

math 574 (math/vector 250+, math/vec*, math/matrix*, math/angles, math/bounds, math/float, math/scalar, math/util, math/words...), botlib 88 (precomp 37+18+..., script 33+...), common 51 (common/math 26+, common/rand/random 6+...), sound 50 (sound/alias 39, sndcurve 3, subtitle 6, state 2), fx 25 (fx/math), collision 21 (collision/math), vector 10, sys 5 (sys/time-math), server 4 (script_vectors), util 3, snd 1. Also tagged by noise: entity/trigger 8.

## Appendix B: duplicated leaf families (identical machine code, clustered by bytes)

| bytes | copies | canonical meaning | example addresses |
|---|---|---|---|
| 47 | 46 | `Vec3_Copy` (3 integer moves) | 0x0805d662, 0x0805fbec, 0x0809bf90 |
| 68 | 28 | `Vec3_Subtract` (float per component) | 0x0805d714, 0x0809e546, 0x081295cc |
| 56 | 27 | `Vec3_Scale` | 0x0809e58a, 0x08129610, 0x080c5fae |
| 68 | 26 | `Vec3_Add` | 0x0805d6d0, 0x0809e502, 0x08129588 |
| 83 | 26 | `Vec3_MA` (`a + s*dir`) | 0x08129648, 0x080c5efc, 0x08137300 |
| 41 | 23 | `Vec3_Clear` | 0x0805d638, 0x0809bf66 |
| 51 | 23 | `Vec3_Dot` / `Vec3_LengthSquared` (unrounded) | 0x0805d758, 0x0812969c |
| 35 | 21 | `Vec3_Set` | 0x08129506, 0x080c7816 |
| 25 | 19 | `Float_Abs` (`fabs` + float store) | 0x0805d57c, 0x0804fd28 |
| 44 | 7 | `Vec2_Subtract` | 0x0805fbc0, 0x080e96b8 |
| 31 | 4 | integer select (`x >= 0 ? a : b`) | 0x081298fc, 0x080f2b14 |

Total: 391 leaf functions -> 89 distinct bodies. Remaining distinct math code (non-leaf, call libm or other helpers) is the algorithmic part in chunks 025/026/101.
