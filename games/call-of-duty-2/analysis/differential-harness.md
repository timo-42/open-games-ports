# Differential harness: original `cod2_lnxded` vs reconstructed chunks

Lane: differential harness. Target: `cod2_lnxded` (Linux dedicated server 1.3), ELF32 i386
EXEC, stripped, GCC 3.3.6, SHA-256
`05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7` (checked by
`difftest.py` before every build).

Code: `games/call-of-duty-2/recon/harness/`

| file | role |
|---|---|
| `hx_runner.c` | loader, import binding, per-case fork, comparison, JSON lines |
| `hx_callstub.S` | cdecl trampolines (`hx_call_cdecl`, `hx_call_cdecl_fp` keeps ST(0)) |
| `difftest.py` | build per chunk, symbol aliasing, signature parsing, input generation, verdicts, batch, self-test, report |
| `specs.py` | default startup state and per-function input/state overrides |

Scratch and results stay outside tracked source: `.local/cod2/harness/{build,runs,results,selftest,sandbox,logs}`.

Tags used below: **observed** = seen in a run of this harness or in the binary;
**inferred** = derived from observed facts but not proven by them.

## How to run

```sh
# one or more functions (exit status 0 only if every verdict is pass)
python3 games/call-of-duty-2/recon/harness/difftest.py --chunk 030 --func 0x080b7ae2 --cases 2000
# every function of a chunk with a supported signature -> .local/cod2/harness/results/chunk_030.json
python3 games/call-of-duty-2/recon/harness/difftest.py --chunk 030 --all --cases 500 --jobs 4
# test an edited copy of a chunk source, or other compiler flags
python3 games/call-of-duty-2/recon/harness/difftest.py --chunk 030 --func 0x080b7ae2 --source /path/chunk_030.cpp --cflags "-O0"
# mutation self-test (must print "selftest": "pass")
python3 games/call-of-duty-2/recon/harness/difftest.py --selftest --cases 400
# markdown table of all batch results
python3 games/call-of-duty-2/recon/harness/difftest.py --report
```

Requirements: `gcc`/`g++` with `-m32` multilib, i386 libc (`libc6:i386`), binutils
(`nm`, `objcopy`, `c++filt`), Python 3.11+. The original binary is read from
`.local/cod2/lnxded-1.3/cod2_lnxded`; `libstdc++.so.5` from
`.local/cod2/runtime-i386/usr/lib/i386-linux-gnu/` if present (`COD2_LOCAL` overrides
`.local/cod2`). Runners are run under `nice -n 10`; `--jobs` is capped at 4.

Verdict JSON (one object per function): `verdict` is `pass`, `fail`, `unsupported`
(signature not drivable, or every case discarded) or `error` (build/runner problem);
`counts` per case status, `discard_reasons`, `first_mismatch` (what differed and where) and
`first_mismatch_input` (the decoded case: objects, memory writes, argument words) so a
failing case can be reproduced; `notes` lists tolerated differences (below).

## Design

### Original side (loader) — observed working

* All `PT_LOAD` segments are mapped at their link addresses with `MAP_FIXED_NOREPLACE`
  (text `0x08048000..0x0817c00c` R+X, data `0x0817d00c..0x08a6f4d8` R+W); file bytes are
  copied, the rest of the RW segment (`.bss`, 9.3 MB) stays zero. `_start`, `main`,
  `.init` and `.ctors` are never run.
* `.rel.plt` (150 `R_386_JUMP_SLOT`) and `.rel.dyn` (`GLOB_DAT __gmon_start__` = 0, five
  `R_386_COPY`: `stdin`/`stdout`/`stderr` and two `__cxxabiv1` vtables) are applied by the
  runner itself. Symbols are resolved with `dlvsym` using the versions from `.gnu.version_r`
  (`GLIBC_2.0`, `GLIBC_2.1`, ...); `GLIBCPP_3.2`/`CXXABI_1.2` names are taken from a
  `dlopen`ed i386 `libstdc++.so.5` (RTLD_LOCAL) when available. Observed: all 150 imports
  resolve on this host; any unresolved import would get a stub that exits with its name.
* Every import slot points to a generated thunk that appends the import index to the
  current side's trace (only while the function under test runs) and jumps to the real
  function. The reconstruction's libc calls are linked to the original's **PLT entries**,
  so both sides use the same binding and the same trace.
* Sandbox: imports with host side effects (`system`, `execl`, `fork`, `unlink`, `remove`,
  `rmdir`, `rename`, `mkdir`, `chmod`, `seteuid`, sockets, `select`, `ioctl`,
  `tcsetattr`/`tcgetattr`, `fcntl`, `dlopen`/`dlsym`/`dlclose`, `sleep`/`usleep`,
  `signal`) stop the side with status 96; `fopen` is allowed for reading only; cwd is
  `.local/cod2/harness/sandbox`, stdin is `/dev/null`, all descriptors above 2 are closed
  in the side processes.

### Reconstruction side (aliasing)

The runner is linked **non-PIE at `0x60000000`** (`-no-pie -Wl,-Ttext-segment=0x60000000`)
rather than as PIE. Reason: chunk code refers to original globals and functions through
absolute addresses; in an i386 PIE those references become GOT/GOTOFF or text
relocations against `SHN_ABS` symbols, which the linker either rejects or relocates by the
load bias. A fixed high base gives the same non-overlap guarantee (the original occupies
`0x08048000..0x08a70000`, the argument arena `0x30000000`, the runner `0x60000000+`, shared
libraries the top of the address space) with plain absolute relocations.

Build steps (`difftest.py build`), cached under `.local/cod2/harness/build/chunk_NNN/`
keyed by the hash of the chunk source, headers, symbol records and harness sources:

1. `g++ -m32 -std=gnu++98 -O1 -march=i686 -mfpmath=387 -mno-sse -fno-pic -fno-pie
   -fno-builtin -fno-strict-aliasing -fwrapv -fno-stack-protector -D_FORTIFY_SOURCE=0`
   compiles `chunk_NNN.cpp` (the chunk keeps C++ linkage, so names are mangled).
2. `nm` + `c++filt`: each **undefined** symbol is demangled and resolved by its base name:
   `fn_08xxxxxx`/`g_08xxxxxx` placeholders → that address; an import name of the original
   (`strlen`, `vsnprintf`, ...) → the original's PLT entry (`.plt + 16*(i+1)`); a dynsym
   object of the original → its address; a name recorded once in
   `analysis/symbols/chunk_*.json` (functions or globals; `symbols.json` when present) →
   that address. Ambiguous names are listed in `build_warnings`; unknown C++ symbols are
   bound to trap stubs that exit with the symbol name (status 98).
3. `objcopy --redefine-sym` renames each resolved reference to `__hx_sym_N` and every
   definition of the chunk to `hxr_<mangled>` (so a reconstructed `_start`, `_init` or
   `strlen` cannot collide with crt/libc); a generated linker script defines
   `__hx_sym_N = 0x0806...;` (absolute symbols). This is the "linker defsym" approach on
   mangled names; no `extern "C"` wrappers are needed.
4. `recon_table.c` maps original address → reconstructed entry (asm labels on the renamed
   mangled names), from the chunk JSON names and the `// 0x080abcde size=...` provenance
   comments. Functions defined in the chunk stay reconstructed (a reconstructed function
   calls reconstructed in-chunk callees and original out-of-chunk callees); globals are
   shared, so both sides use one memory layout.
5. Struct sizes for pointer parameters are probed by compiling `sizeof(struct X)` against
   the chunk's own includes.

### Per case

The runner (`hx_runner`) reads a case file. Global `init` lines call ORIGINAL functions
once (default: `Com_SetThreadBlocks(0)` 0x080b805e and `Swap_Init()` 0x080b7956 — inferred
startup state; see `specs.py`). For every case it forks **P**, which lays out argument
objects in the arena (`0x30000000`, 16-byte aligned, 64-byte `0xA5` gaps, unused arena
`0xA5`), writes `mem` lines into the image, runs per-case `pre` calls, hashes every page of
the RW segment and the used arena, then forks **A** (original) and **B** (reconstruction)
from that identical state. Each side: `alarm(2 s)`, stdout/stderr into a memfd, call
through the cdecl trampoline with a 16-byte aligned stack, record EAX/EDX/ST(0), hash all
pages again, copy changed pages to shared memory.

Compared, in this order (first difference reported):

1. outcome: returned / signal / `exit(n)` / sandbox denial. A crash, timeout, unresolved
   import or sandbox denial **in the original discards the case** (never counted as pass).
2. return value by declared type: integers masked to their width, 64-bit EDX:EAX, `float`
   and `double` bitwise after rounding ST(0) to the declared type, pointers by value.
   For `char *` returns the string at the pointer is compared as well.
3. every byte of `.data`/`.bss` (`0x0817d000..0x08a70000`) and of the arena (caller
   buffers and their gaps). Detail: address, region, both bytes, 32-byte windows, number of
   differing bytes.
4. stdout/stderr bytes.
5. ordered import trace (index of first difference, first 12 calls of each side).

Tolerated differences (pass with a note, counted in `notes`):

* function-pointer equivalence: a word (memory or return) that holds original address X on
  side A and the reconstructed entry of X on side B;
* constant-literal equivalence: a pointer into the original `.rodata`
  (`0x0813e340..0x08160e1c`) on side A and into the runner's own image on side B whose
  strings are equal (two builds cannot share literal addresses);
* x87 80-bit return value differs only before rounding to the declared type;
* import-trace differences are notes by default (GCC 3.3 vs GCC 16 differ in which libc
  calls are inlined; glibc 2.38 headers turn `strtol` into `__isoc23_strtol`, which is
  not an original import and is called directly). `strict_imports` in a spec turns them
  into failures.

### Input generation

From the recorded `signature` (C++ declaration). Supported: integer types of 8/16/32 bits
(sign/zero extended), 64-bit integers, `float`, `double`, `char *`/`const char *`
(strings: empty, short, mixed case, parser text, colour codes, non-ASCII, 1023..9000
chars, case-folded/prefix/mutated variants of earlier string arguments; ~2 % NULL),
`char **`, `float *`/`vec3_t` (special values incl. ±0, ±inf, NaN, denormals, random
bits), `int *`/`double *` buffers, pointers to structs whose size the chunk headers define
(zeroed or random bytes), `void **` out-slots, and printf-style `...` (format and matching
int/double/string arguments). Size-like parameters (`size`, `len`, `count`, `max`, `n`)
are drawn around the preceding buffer's capacity and length, character parameters from
-1..255, index-like ones from small ranges. A runner gives up on a function after 5
consecutive original timeouts or 60 cases without a comparable one.

Not supported without a spec: `void *`, function pointers, C++ member functions (implicit
`this`), struct/`long double` by value, `va_list`. `specs.py` adds generators and state
(e.g. Info strings sized to the `0x400`/`0x2000` limits, matrix text for
`Com_Parse1DMatrix`, `sessionIndex`/`va` rotation state written via `mem`).

### Floating point

GCC 3.3.6 on i386 generated x87 code (observed in disassembly, e.g. `fld`/`fstp` in the
vector helpers; `PC_NameHash` shows unoptimised `-O0`-style code with all locals in
memory). The reconstruction is compiled with `-march=i686 -mfpmath=387 -mno-sse` so both
sides use the x87 unit with the same control word (inherited, Linux default 0x037f,
extended precision). C++ in GCC 16 only supports `-fexcess-precision=fast`, so spills and
rounding points may differ from GCC 3.3; `--cflags "-O0"` or `--cflags "-ffloat-store"`
can be used to check whether a float mismatch depends on spilling (inferred to be the main
residual risk for float code).

## Self-test (observed, `--selftest --cases 400`, seed 1)

| mutation | expected | verdict | first mismatch |
|---|---|---|---|
| Q_stricmpn 0x080b7ae2 `c1 - 0x20` → `c1 - 0x21` | fail | fail | return 0x0 vs 0xffffffff |
| Info_RemoveKey 0x080b8318 unchanged | pass | pass | — |
| Info_SetValueForKey 0x080b85ce `< 0x401` → `< 0x400` | fail | fail | arena 0x3000042f: '\\' vs NUL |
| Q_strncpyz 0x080b7ab6 unchanged | pass | pass | — |
| va 0x080b7fa6 no-op source edit | pass | pass | — |
| va 0x080b7fa6 buffer stride 0x400 → 0x3ff | fail | fail | return 0x085b7aa0 vs 0x085b7a9f |
| Info_SetValueForKey changed console text | fail | fail | stdout differs |
| Com_SetSpaceDelimited drops `!= 0` | fail | fail | .bss 0x0817e825: 0x01 vs 0x00 |

Lessons from building it (observed): a single call from fresh state never exercised `va`'s
second buffer and random info strings almost never hit the exact 0x400 boundary; both
needed `specs.py` state/input seeding before the mutations were caught.

## Results (observed, 2026-11-17)

Batch: `difftest.py --chunk NNN --all --cases 300 --jobs 4` over every chunk that had both
`recon/chunks/chunk_NNN.cpp` and `analysis/symbols/chunk_NNN.json`, seed 1. Other lanes were
editing chunk sources and the shared prototypes while this ran, so each row is valid only
for the source snapshot whose SHA-256 prefix is listed; a few chunks (e.g. 001, 029) did
not compile at a later snapshot, and some failures seen in the batch (e.g.
`Vec3_PackDir10`) pass against the current source. Re-run a chunk before acting on a row.
Chunk 000 (PLT stubs/crt) has no testable record.

Totals: **2252 pass, 148 fail, 1556 unsupported** (unsupported = signature not drivable
without a spec, or every generated case crashed/timed out in the original because the
function needs engine state). A `pass` means no difference in the cases that the original
completed; check `counts` for how many cases that was.

### Required demonstration functions (chunk 030, 2000 cases each)

| addr | function | verdict | pass | discarded (original crashed: NULL/overlong input, `Com_Error`) |
|---|---|---|---|---|
| 0x080b7c3c | Q_WildcardCompare | pass | 1931 | 69 |
| 0x080b85ce | Info_SetValueForKey | pass | 2000 | 0 |
| 0x080b8318 | Info_RemoveKey | pass | 1967 | 33 |
| 0x080b7fa6 | va | pass | 1711 | 289 (output > 0x3ff chars -> `Com_Error`) |
| 0x080b6664 | Com_ParseCSV | pass | 1906 | 94 |
| 0x080b8108 | Info_ValueForKey | pass | 2000 | 0 (1675 with literal-pointer note) |
| 0x080b7bf8 | Q_stricmp | pass | 1941 | 59 |
| 0x080b7ab6 | Q_strncpyz | pass | 1731 | 269 |

(The task brief listed 0x080b7c3c as Q_stricmp and 0x080b8108 as va; in the chunk 030
record those addresses are Q_WildcardCompare and Info_ValueForKey; Q_stricmp is
0x080b7bf8 and va 0x080b7fa6. All are tested above.) Whole chunk 030: 83 pass, 0 fail,
3 unsupported (`Com_ParseError` always reaches `Com_Error`; `Com_ParseInfoFields` had a
malformed recorded signature at batch time (`bool Com_Parsestruct InfoFields(...`, since
corrected by the merge lane) and takes function-pointer parameters; `Rotate_Eased` takes
`void *`).

`Info_ValueForKey` returns a pointer to the constant `""` when the key is missing; the
original's literal (0x0814f8ad) and the reconstruction's literal differ in address, which
the constant-literal rule tolerates (counted under `notes.constant_literal_pointer`).

### Failure triage (inferred)

The triage column is a heuristic (`classify_failure` in `difftest.py`):

* **float: last-bit rounding** (25) — one unit in the last place in a float result. GCC 16
  keeps intermediates in 80-bit registers in different places than GCC 3.3.6 (which
  stored most intermediates to `float` stack slots). Changing `-O1` to `-O0` or adding
  `-ffloat-store` moves but does not remove these differences (observed on
  `Quat_ToMat3`, `Vec3_ExtentDistanceSq`). Exact parity needs explicit `(float)`
  rounding points (or `volatile float` temporaries) where the original stores — they are
  real observable differences, but small.
* **float: signalling NaN quieted** (22) — byte differs by 0x40 in the top mantissa
  byte: the original copies vectors with integer moves, the reconstruction loads/stores
  through the x87 unit, which turns an sNaN into a qNaN. Only reachable with sNaN inputs.
* **nondeterministic** (3) — `Sys_Milliseconds`, `Sys_MillisecondsRaw` (wall clock between
  the two side runs) and `Sys_EnterCriticalSection` (mutex owner thread id differs
  between the two side processes). Not reconstruction bugs.
* **build: unresolved reconstruction symbol** (3) — e.g. `GScr_LoadGametypeList` calls
  `FS_GetModList_080a1d4a`, a name that is neither a placeholder nor recorded in any
  symbol JSON (trap stub fired).
* **recon crash** (8) and **logic/layout** (87) — candidates for real reconstruction bugs.
  Confirmed by disassembly (observed):
  * `PC_NameHash` 0x0804b1a8 (chunk 001): the original multiplies by `(i + 0x77)`
    (`add eax,0x77` at 0x0804b1d7); the reconstruction uses decimal `77`. 264/294 cases
    return a different hash.
  Further examples to inspect: `NET_BaseAdrToString` 0x080d520a returns `"1.0.0.0"` for
  an all-zero address (original `"0.0.0.0"`); `inflate_blocks_reset` 0x080ca620 returns 0
  where the original returns its first argument; `Color3_ToRGBA8`/`Color4_ToRGBA8` return
  different packed words; `CM_PositionTestCapsuleInTriangle` 0x080555b6 and the
  `Line_*`/`Vec*_MaxAbsLength` helpers differ only on NaN/inf/zero-plane inputs
  (unordered x87 compares; 20 of 22 failing `CM_PositionTestCapsuleInTriangle` cases had a
  zero or non-finite triangle plane). Dvar registration functions (`*_RegisterDvars`,
  `Dvar_InitSvCheats`) differ in the upper three bytes of boolean dvar value words: the
  original passes registers with stale upper bits (e.g. `0xf7bea801`) and the stored word
  keeps them, the reconstruction passes a clean `1` (inferred from the windows; impact
  probably nil, but byte-observable).

Full per-chunk table and every failing function with its first mismatch (generated by
`difftest.py --report`; `failing/compared` = failing cases / cases the original completed):

| chunk | pass | fail | unsupported | error | source sha256 |
|---|---|---|---|---|---|
| 001 | 29 | 1 | 12 | 0 | 41c9f4eff825 |
| 002 | 37 | 0 | 31 | 0 | 3b7ce6f21627 |
| 003 | 52 | 1 | 16 | 0 | 497343aeba4c |
| 004 | 44 | 5 | 16 | 0 | 58f974b5b857 |
| 005 | 24 | 3 | 26 | 0 | 0fc3f037e921 |
| 006 | 63 | 2 | 18 | 0 | f8a39699b1b5 |
| 007 | 57 | 0 | 23 | 0 | 3a8352d0f2ca |
| 008 | 63 | 0 | 14 | 0 | 1dbf5d73aa97 |
| 009 | 31 | 4 | 24 | 0 | 61b82337755f |
| 010 | 31 | 0 | 66 | 0 | 98ea90d25f2c |
| 011 | 34 | 0 | 27 | 0 | 2b44483c943c |
| 012 | 34 | 0 | 39 | 0 | 8330b5a26fd0 |
| 013 | 45 | 0 | 43 | 0 | b0ea9aa9a4eb |
| 014 | 82 | 2 | 21 | 0 | cd2fed501103 |
| 015 | 34 | 0 | 39 | 0 | 17f3df048aea |
| 016 | 12 | 0 | 5 | 0 | 2ea52535b629 |
| 017 | 23 | 2 | 56 | 0 | 4b5cfc0315e2 |
| 018 | 11 | 1 | 5 | 0 | ac2011ef23fb |
| 019 | 24 | 0 | 31 | 0 | e6eaf1d73c08 |
| 020 | 27 | 1 | 27 | 0 | 018b6d28881e |
| 021 | 36 | 0 | 35 | 0 | 053d411e4739 |
| 022 | 21 | 0 | 20 | 0 | 9044d7978d85 |
| 023 | 25 | 0 | 26 | 0 | 42be3c2bdee6 |
| 024 | 34 | 0 | 20 | 0 | 84e2c2ae9969 |
| 025 | 49 | 28 | 9 | 0 | 885637331390 |
| 026 | 80 | 21 | 4 | 0 | 9d91c39efc2a |
| 027 | 69 | 0 | 16 | 0 | 02c919cc53ef |
| 028 | 52 | 0 | 6 | 0 | 0cde2229709e |
| 029 | 74 | 22 | 8 | 0 | d2215248f740 |
| 030 | 83 | 0 | 3 | 0 | 5e5a2a139657 |
| 031 | 66 | 1 | 17 | 0 | f6ec2509a8ae |
| 032 | 27 | 4 | 34 | 0 | 1480c0b09f19 |
| 033 | 52 | 1 | 35 | 0 | 4e08b25b6210 |
| 034 | 43 | 1 | 31 | 0 | c7c8b88aa162 |
| 035 | 21 | 2 | 12 | 0 | 170ed49154a8 |
| 036 | 13 | 1 | 12 | 0 | eadfa162da75 |
| 037 | 32 | 0 | 21 | 0 | 24e1dd06d111 |
| 038 | 69 | 4 | 21 | 0 | 2f2f79e12049 |
| 039 | 37 | 0 | 18 | 0 | fed80295ca8c |
| 040 | 32 | 9 | 39 | 0 | d851ca61ba3c |
| 041 | 31 | 4 | 18 | 0 | f79cf8feff34 |
| 042 | 25 | 1 | 12 | 0 | d9e4c37ce0d3 |
| 043 | 47 | 2 | 37 | 0 | 00e453bd63ff |
| 044 | 18 | 0 | 41 | 0 | 9b68d85e2653 |
| 046 | 29 | 2 | 25 | 0 | 9dcf81041438 |
| 047 | 39 | 1 | 22 | 0 | bd5463170e48 |
| 050 | 47 | 2 | 34 | 0 | b29b4b19b8f7 |
| 051 | 47 | 2 | 43 | 0 | 5653071e4c7e |
| 053 | 35 | 1 | 46 | 0 | f7eb747f88f7 |
| 054 | 37 | 0 | 68 | 0 | 1f6e1e5f52d2 |
| 055 | 77 | 1 | 53 | 0 | 75e063c9b8d4 |
| 056 | 64 | 15 | 21 | 0 | 475cdea886e7 |
| 057 | 32 | 1 | 42 | 0 | 0772af135f7c |
| 058 | 52 | 0 | 31 | 0 | afbf03be4223 |
| 062 | 0 | 0 | 89 | 0 | 9d9cc5990a50 |
| 103 | 0 | 0 | 48 | 0 | f1405fe03657 |
| **total** | 2252 | 148 | 1556 | 0 | |

| chunk | addr | name | triage (inferred) | failing/compared | first mismatch |
|---|---|---|---|---|---|
| 001 | `0x0804b1a8` | PC_NameHash | logic/layout (inspect) | 264/294 | `{"what": "return", "orig": "0x1a", "recon": "0x184"}` |
| 003 | `0x080555b6` | CM_PositionTestCapsuleInTriangle | float: last-bit rounding (x87 precision/spill) | 1/295 | `{"what": "memory", "addr": "0x300001e2", "region": "arena", "orig_byte": "0x01", "recon_byte": "0x00", "diff_bytes": 2}` |
| 004 | `0x0805631a` | cm_wedge_edge_sweep | logic/layout (inspect) | 2/300 | `{"what": "memory", "addr": "0x300001d0", "region": "arena", "orig_byte": "0x00", "recon_byte": "0xe9", "diff_bytes": 16}` |
| 004 | `0x08056490` | cm_wedge_sphere_sweep | logic/layout (inspect) | 18/299 | `{"what": "memory", "addr": "0x300001d4", "region": "arena", "orig_byte": "0x69", "recon_byte": "0x00", "diff_bytes": 13}` |
| 004 | `0x08057046` | cm_wall_circle_sweep | logic/layout (inspect) | 12/300 | `{"what": "memory", "addr": "0x300001a2", "region": "arena", "orig_byte": "0xc0", "recon_byte": "0x00", "diff_bytes": 10}` |
| 004 | `0x08057654` | cm_vec3_set | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x30000042", "region": "arena", "orig_byte": "0xbb", "recon_byte": "0xfb", "diff_bytes": 1}` |
| 004 | `0x0805a95e` | cm_trace_vertical_capsule | recon crash/exit | 29/268 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 005 | `0x0805d662` | Vec3_Copy | float: signalling NaN quieted by x87 load/store | 2/300 | `{"what": "memory", "addr": "0x300000c2", "region": "arena", "orig_byte": "0x92", "recon_byte": "0xd2", "diff_bytes": 1}` |
| 005 | `0x0805d82a` | Vec4_Copy | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x300000c6", "region": "arena", "orig_byte": "0x8f", "recon_byte": "0xcf", "diff_bytes": 1}` |
| 005 | `0x0805d86c` | Box_ComputeInverseExtents | float: last-bit rounding (x87 precision/spill) | 88/300 | `{"what": "memory", "addr": "0x30000060", "region": "arena", "orig_byte": "0xf6", "recon_byte": "0xf5", "diff_bytes": 1}` |
| 006 | `0x080608a6` | Cmd_ExecuteString | build: unresolved reconstruction symbol | 277/300 | `{"what": "outcome", "orig": "returned", "recon": "unresolved_recon_symbol"}` |
| 006 | `0x080609cc` | Cmd_ExecuteString_alt | build: unresolved reconstruction symbol | 267/300 | `{"what": "outcome", "orig": "returned", "recon": "unresolved_recon_symbol"}` |
| 009 | `0x0806aaca` | MSG_ReadDeltaPlayerState | logic/layout (inspect) | 125/177 | `{"what": "memory", "addr": "0x30000178", "region": "arena", "orig_byte": "0xa5", "recon_byte": "0xd8", "diff_bytes": 740}` |
| 009 | `0x0806b3ca` | NetProfile_AddSample | logic/layout (inspect) | 237/237 | `{"what": "memory", "addr": "0x300000a0", "region": "arena", "orig_byte": "0x75", "recon_byte": "0x78", "diff_bytes": 1}` |
| 009 | `0x0806b55a` | NetProfile_ComputeStats | logic/layout (inspect) | 138/300 | `{"what": "memory", "addr": "0x0860af20", "region": "image_rw", "orig_byte": "0xc8", "recon_byte": "0xca", "diff_bytes": 1}` |
| 009 | `0x0806b8be` | NET_InitCvars | logic/layout (inspect) | 300/300 | `{"what": "memory", "addr": "0x085abe29", "region": "image_rw", "orig_byte": "0xd8", "recon_byte": "0x00", "diff_bytes": 27}` |
| 014 | `0x0807c2cc` | ScrVar_ReadFieldValue | logic/layout (inspect) | 208/208 | `{"what": "memory", "addr": "0x30000040", "region": "arena", "orig_byte": "0x05", "recon_byte": "0x00", "diff_bytes": 1}` |
| 014 | `0x0807c6b0` | ScrVar_GetOrNewVarValueId24 | logic/layout (inspect) | 1/202 | `{"what": "memory", "addr": "0x08297502", "region": "image_rw", "orig_byte": "0x9a", "recon_byte": "0x00", "diff_bytes": 4}` |
| 017 | `0x08083d12` | Scr_ShutdownThreads | logic/layout (inspect) | 211/211 | `{"what": "memory", "addr": "0x083dce98", "region": "image_rw", "orig_byte": "0x28", "recon_byte": "0x9f", "diff_bytes": 2}` |
| 017 | `0x08085628` | Scr_SampleTimers | logic/layout (inspect) | 300/300 | `{"what": "memory", "addr": "0x083dce98", "region": "image_rw", "orig_byte": "0x3b", "recon_byte": "0xb5", "diff_bytes": 2}` |
| 018 | `0x080857aa` | Sys_ReadTimestampLow | logic/layout (inspect) | 300/300 | `{"what": "return", "orig": "0x2a732163", "recon": "0x2ad75990"}` |
| 020 | `0x08091ad6` | SV_EntityRelevantToView | recon crash/exit | 51/113 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 025 | `0x080a3750` | FS_StatPath | logic/layout (inspect) | 6/300 | `{"what": "memory", "addr": "0x3000009a", "region": "arena", "orig_byte": "0x4b", "recon_byte": "0x00", "diff_bytes": 4}` |
| 025 | `0x080a3d66` | Angle_ApproachProportionalWrapped | recon crash/exit | 246/246 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 025 | `0x080a3dea` | Curve_EvalPiecewise | logic/layout (inspect) | 20/300 | `{"what": "return", "orig": "0x0", "recon": "0xc3d0ae28"}` |
| 025 | `0x080a3f8e` | Normal_DirToIndex | logic/layout (inspect) | 1/300 | `{"what": "return", "orig": "0x4", "recon": "0xe"}` |
| 025 | `0x080a4306` | Vec3_DominantAxisOrder | float: last-bit rounding (x87 precision/spill) | 6/300 | `{"what": "memory", "addr": "0x300000c0", "region": "arena", "orig_byte": "0x01", "recon_byte": "0x00", "diff_bytes": 2}` |
| 025 | `0x080a4424` | Vec3_Normalize | float: last-bit rounding (x87 precision/spill) | 107/300 | `{"what": "memory", "addr": "0x30000040", "region": "arena", "orig_byte": "0x05", "recon_byte": "0x04", "diff_bytes": 2}` |
| 025 | `0x080a44c2` | Vec2_Normalize | float: last-bit rounding (x87 precision/spill) | 75/300 | `{"what": "memory", "addr": "0x30000044", "region": "arena", "orig_byte": "0x2e", "recon_byte": "0x2f", "diff_bytes": 1}` |
| 025 | `0x080a453a` | Vec4_Normalize | float: last-bit rounding (x87 precision/spill) | 112/300 | `{"what": "memory", "addr": "0x30000040", "region": "arena", "orig_byte": "0xa2", "recon_byte": "0xa3", "diff_bytes": 4}` |
| 025 | `0x080a45fc` | Vec3_NormalizeCopy | float: last-bit rounding (x87 precision/spill) | 97/300 | `{"what": "memory", "addr": "0x300000c0", "region": "arena", "orig_byte": "0x83", "recon_byte": "0x82", "diff_bytes": 1}` |
| 025 | `0x080a46a6` | Vec2_NormalizeCopy | logic/layout (inspect) | 73/300 | `{"what": "memory", "addr": "0x300000c4", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x00", "diff_bytes": 3}` |
| 025 | `0x080a472c` | Vec3_PackDir10 | logic/layout (inspect) | 295/300 | `{"what": "return", "orig": "0x3fb06281", "recon": "0x0"}` |
| 025 | `0x080a47a2` | Vec3_UnpackDir10 | float: last-bit rounding (x87 precision/spill) | 70/300 | `{"what": "memory", "addr": "0x30000040", "region": "arena", "orig_byte": "0xbe", "recon_byte": "0xbd", "diff_bytes": 3}` |
| 025 | `0x080a49d6` | Vec3_RotateAroundAxis | logic/layout (inspect) | 53/300 | `{"what": "memory", "addr": "0x30000040", "region": "arena", "orig_byte": "0x91", "recon_byte": "0x50", "diff_bytes": 1}` |
| 025 | `0x080a4cc2` | Vec3_PerpendicularBasis | float: last-bit rounding (x87 precision/spill) | 114/300 | `{"what": "memory", "addr": "0x300000c8", "region": "arena", "orig_byte": "0xee", "recon_byte": "0xef", "diff_bytes": 5}` |
| 025 | `0x080a5226` | Angles_ToVectors | logic/layout (inspect) | 203/300 | `{"what": "memory", "addr": "0x300000c0", "region": "arena", "orig_byte": "0xeb", "recon_byte": "0xbe", "diff_bytes": 12}` |
| 025 | `0x080a539e` | Angle_YawToForwardRight | logic/layout (inspect) | 114/300 | `{"what": "memory", "addr": "0x30000044", "region": "arena", "orig_byte": "0x2e", "recon_byte": "0x69", "diff_bytes": 8}` |
| 025 | `0x080a541e` | Vec3_PerpendicularVector | float: last-bit rounding (x87 precision/spill) | 71/300 | `{"what": "memory", "addr": "0x300000c0", "region": "arena", "orig_byte": "0x99", "recon_byte": "0x98", "diff_bytes": 2}` |
| 025 | `0x080a54f0` | Triangle_Normal | float: last-bit rounding (x87 precision/spill) | 91/300 | `{"what": "memory", "addr": "0x300001c4", "region": "arena", "orig_byte": "0x58", "recon_byte": "0x59", "diff_bytes": 2}` |
| 025 | `0x080a556a` | Line_ProjectPoint | logic/layout (inspect) | 117/300 | `{"what": "memory", "addr": "0x300001c2", "region": "arena", "orig_byte": "0xc0", "recon_byte": "0x80", "diff_bytes": 4}` |
| 025 | `0x080a5618` | Line_ResidualLengthSq | logic/layout (inspect) | 100/300 | `{"what": "return", "orig": "0x7fc00000", "recon": "0x7f800000"}` |
| 025 | `0x080a56bc` | Vec3_ExtentDistanceSq | float: last-bit rounding (x87 precision/spill) | 48/300 | `{"what": "return", "orig": "0x3ed50795", "recon": "0x3ed50796"}` |
| 025 | `0x080a6792` | Mat3_Transpose | float: signalling NaN quieted by x87 load/store | 4/300 | `{"what": "memory", "addr": "0x300000da", "region": "arena", "orig_byte": "0xa5", "recon_byte": "0xe5", "diff_bytes": 1}` |
| 025 | `0x080a6922` | Mat3_Invert | float: last-bit rounding (x87 precision/spill) | 184/300 | `{"what": "memory", "addr": "0x300000c0", "region": "arena", "orig_byte": "0xd6", "recon_byte": "0xd5", "diff_bytes": 5}` |
| 025 | `0x080a6b42` | Rigid_Invert | float: signalling NaN quieted by x87 load/store | 2/300 | `{"what": "memory", "addr": "0x300000d6", "region": "arena", "orig_byte": "0x8c", "recon_byte": "0xcc", "diff_bytes": 1}` |
| 025 | `0x080a6b96` | Mat4_Invert | logic/layout (inspect) | 125/300 | `{"what": "memory", "addr": "0x300000c0", "region": "arena", "orig_byte": "0x00", "recon_byte": "0xb2", "diff_bytes": 64}` |
| 025 | `0x080a7670` | Vec2_RotateDegrees | logic/layout (inspect) | 62/300 | `{"what": "memory", "addr": "0x30000040", "region": "arena", "orig_byte": "0x77", "recon_byte": "0x61", "diff_bytes": 2}` |
| 025 | `0x080a77f8` | Quat_Conjugate | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x300000c6", "region": "arena", "orig_byte": "0xa1", "recon_byte": "0xe1", "diff_bytes": 1}` |
| 025 | `0x080a7846` | Quat_ToMat3 | float: last-bit rounding (x87 precision/spill) | 244/300 | `{"what": "memory", "addr": "0x300000c8", "region": "arena", "orig_byte": "0xce", "recon_byte": "0xcd", "diff_bytes": 4}` |
| 026 | `0x080a79c6` | Quat_VectorNormSqRatio | float: last-bit rounding (x87 precision/spill) | 120/300 | `{"what": "return", "orig": "0x3f7f87b5", "recon": "0x3f7f87b6"}` |
| 026 | `0x080a7a9a` | Quat_RelativeVectorNormSq | float: last-bit rounding (x87 precision/spill) | 133/300 | `{"what": "return", "orig": "0x3eecd9bd", "recon": "0x3eecd9bc"}` |
| 026 | `0x080a7ad8` | Quat_PlanarAngleDeg | float: last-bit rounding (x87 precision/spill) | 81/300 | `{"what": "return", "orig": "0x3c48251e", "recon": "0x3c48251f"}` |
| 026 | `0x080a810c` | Color3_ToRGBA8 | logic/layout (inspect) | 199/300 | `{"what": "return", "orig": "0xff000000", "recon": "0xfff0b94c"}` |
| 026 | `0x080a817c` | Color4_ToRGBA8 | logic/layout (inspect) | 214/300 | `{"what": "return", "orig": "0xca00", "recon": "0xdbb4ca00"}` |
| 026 | `0x080a82f4` | LerpAngle | logic/layout (inspect) | 3/300 | `{"what": "return", "orig": "0x46c314bc", "recon": "0x46c31217"}` |
| 026 | `0x080a8364` | AngleSubtract | logic/layout (inspect) | 58/248 | `{"what": "return", "orig": "0xc2616d80", "recon": "0xc2616daf"}` |
| 026 | `0x080a83c6` | AnglesSubtract | logic/layout (inspect) | 73/101 | `{"what": "memory", "addr": "0x30000140", "region": "arena", "orig_byte": "0x00", "recon_byte": "0x71", "diff_bytes": 2}` |
| 026 | `0x080a84b8` | AngleNormalize360_Loop | logic/layout (inspect) | 1/269 | `{"what": "return", "orig": "0x43940000", "recon": "0x42f00000"}` |
| 026 | `0x080a8536` | AngleNormalize180_Loop | logic/layout (inspect) | 2/269 | `{"what": "return", "orig": "0x42800000", "recon": "0xc2800000"}` |
| 026 | `0x080a85d2` | Vec3_MaxAbsLength | logic/layout (inspect) | 15/300 | `{"what": "return", "orig": "0x7f800000", "recon": "0x7fc00000"}` |
| 026 | `0x080a8660` | Vec2_MaxAbsLength | logic/layout (inspect) | 9/300 | `{"what": "return", "orig": "0x7fc00000", "recon": "0x3ebf6fa3"}` |
| 026 | `0x080a87b4` | Vec3Pair_PushZApart | float: last-bit rounding (x87 precision/spill) | 28/300 | `{"what": "memory", "addr": "0x300000c8", "region": "arena", "orig_byte": "0x9e", "recon_byte": "0x9d", "diff_bytes": 1}` |
| 026 | `0x080a8832` | Vec3Pair_PullXYToZ | logic/layout (inspect) | 35/300 | `{"what": "memory", "addr": "0x30000040", "region": "arena", "orig_byte": "0x40", "recon_byte": "0x60", "diff_bytes": 1}` |
| 026 | `0x080a9f00` | Polygon_ContainsPoint | logic/layout (inspect) | 1/300 | `{"what": "return", "orig": "0x1", "recon": "0x0"}` |
| 026 | `0x080aa680` | Vec3_SharedSideMinSqExceeds | logic/layout (inspect) | 14/300 | `{"what": "return", "orig": "0x0", "recon": "0x1"}` |
| 026 | `0x080aab1a` | Vec3_TransformByQuatTransform_Wrap | float: signalling NaN quieted by x87 load/store | 2/300 | `{"what": "memory", "addr": "0x30000122", "region": "arena", "orig_byte": "0xc0", "recon_byte": "0x80", "diff_bytes": 2}` |
| 026 | `0x080aab3c` | Mat3_ToQuat | logic/layout (inspect) | 44/300 | `{"what": "memory", "addr": "0x300000c2", "region": "arena", "orig_byte": "0xc0", "recon_byte": "0x80", "diff_bytes": 4}` |
| 026 | `0x080aae30` | Float_SnapToStep | logic/layout (inspect) | 20/300 | `{"what": "return", "orig": "0x25be41ec", "recon": "0xffc00000"}` |
| 026 | `0x080ab3f8` | Vec3_TransformByQuatTransform | float: signalling NaN quieted by x87 load/store | 3/300 | `{"what": "memory", "addr": "0x30000126", "region": "arena", "orig_byte": "0xc0", "recon_byte": "0x80", "diff_bytes": 2}` |
| 026 | `0x080ab4a0` | QuatTransform_ToMat3 | float: signalling NaN quieted by x87 load/store | 29/300 | `{"what": "memory", "addr": "0x300000be", "region": "arena", "orig_byte": "0xc0", "recon_byte": "0x80", "diff_bytes": 1}` |
| 029 | `0x080b3740` | Dvar_Redefine | recon crash/exit | 68/142 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 029 | `0x080b443a` | Dvar_SetBoolEx | logic/layout (inspect) | 53/152 | `{"what": "memory", "addr": "0x30000049", "region": "arena", "orig_byte": "0x00", "recon_byte": "0xff", "diff_bytes": 6}` |
| 029 | `0x080b44a8` | Dvar_SetIntEx | logic/layout (inspect) | 158/159 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0xb0", "recon_byte": "0xb4", "diff_bytes": 2}` |
| 029 | `0x080b4526` | Dvar_SetFloatEx | logic/layout (inspect) | 153/157 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0x80", "recon_byte": "0x84", "diff_bytes": 2}` |
| 029 | `0x080b4598` | Dvar_SetVec2Ex | logic/layout (inspect) | 129/131 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0x40", "recon_byte": "0x50", "diff_bytes": 2}` |
| 029 | `0x080b462a` | Dvar_SetVec3Ex | logic/layout (inspect) | 151/152 | `{"what": "memory", "addr": "0x085b76b6", "region": "image_rw", "orig_byte": "0x30", "recon_byte": "0x31", "diff_bytes": 10}` |
| 029 | `0x080b46d4` | Dvar_SetVec4Ex | logic/layout (inspect) | 160/166 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0x30", "recon_byte": "0x50", "diff_bytes": 2}` |
| 029 | `0x080b4812` | Dvar_SetVec4OrColorEx | logic/layout (inspect) | 157/159 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0x80", "recon_byte": "0x84", "diff_bytes": 2}` |
| 029 | `0x080b4980` | Dvar_SetBool | logic/layout (inspect) | 113/124 | `{"what": "memory", "addr": "0x30000049", "region": "arena", "orig_byte": "0xa8", "recon_byte": "0x00", "diff_bytes": 6}` |
| 029 | `0x080b49aa` | Dvar_SetInt | logic/layout (inspect) | 152/155 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0xd0", "recon_byte": "0xd4", "diff_bytes": 2}` |
| 029 | `0x080b49cc` | Dvar_SetFloat | logic/layout (inspect) | 148/151 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0x90", "recon_byte": "0x94", "diff_bytes": 2}` |
| 029 | `0x080b49ee` | Dvar_SetVec2 | logic/layout (inspect) | 162/163 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0xa0", "recon_byte": "0xb0", "diff_bytes": 2}` |
| 029 | `0x080b4a18` | Dvar_SetVec3 | logic/layout (inspect) | 150/152 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0x10", "recon_byte": "0x20", "diff_bytes": 2}` |
| 029 | `0x080b4a48` | Dvar_SetVec4 | logic/layout (inspect) | 149/151 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0x20", "recon_byte": "0x30", "diff_bytes": 2}` |
| 029 | `0x080b4aa2` | Dvar_SetVec4OrColor | logic/layout (inspect) | 161/161 | `{"what": "memory", "addr": "0x30000048", "region": "arena", "orig_byte": "0xd0", "recon_byte": "0xc4", "diff_bytes": 2}` |
| 029 | `0x080b4ada` | Dvar_SetFromStringEx | recon crash/exit | 130/130 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 029 | `0x080b4bd4` | Dvar_SetFromString | recon crash/exit | 139/139 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 029 | `0x080b4ed0` | Dvar_SetOrCreateColor | logic/layout (inspect) | 294/294 | `{"what": "memory", "addr": "0x085b76ba", "region": "image_rw", "orig_byte": "0x2d", "recon_byte": "0x32", "diff_bytes": 24}` |
| 029 | `0x080b5138` | Dvar_InitSvCheats | logic/layout (inspect) | 300/300 | `{"what": "memory", "addr": "0x085abe29", "region": "image_rw", "orig_byte": "0x68", "recon_byte": "0x00", "diff_bytes": 9}` |
| 029 | `0x080b5760` | Vec2_Set_080b5760 | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x30000046", "region": "arena", "orig_byte": "0xa6", "recon_byte": "0xe6", "diff_bytes": 1}` |
| 029 | `0x080b5eba` | ParseSlot_Init | logic/layout (inspect) | 300/300 | `{"what": "memory", "addr": "0x3000044c", "region": "arena", "orig_byte": "0xca", "recon_byte": "0x20", "diff_bytes": 8}` |
| 029 | `0x080b5f32` | Com_BeginParseSession | logic/layout (inspect) | 296/296 | `{"what": "memory", "addr": "0x0817ec88", "region": "image_rw", "orig_byte": "0xca", "recon_byte": "0x20", "diff_bytes": 8}` |
| 031 | `0x080b934c` | Vec3_Set_b | float: signalling NaN quieted by x87 load/store | 2/500 | `{"what": "memory", "addr": "0x30000042", "region": "arena", "orig_byte": "0xa0", "recon_byte": "0xe0", "diff_bytes": 1}` |
| 032 | `0x080bd0a6` | XAnim_MakeFrameKey | logic/layout (inspect) | 46/300 | `{"what": "memory", "addr": "0x3000004c", "region": "arena", "orig_byte": "0xe0", "recon_byte": "0xe7", "diff_bytes": 1}` |
| 032 | `0x080bd34e` | XAnim_ResetUnmaskedParts | logic/layout (inspect) | 152/300 | `{"what": "memory", "addr": "0x300000c0", "region": "arena", "orig_byte": "0x00", "recon_byte": "0xa5", "diff_bytes": 787}` |
| 032 | `0x080bdd54` | XAnim_ReflectPair | float: last-bit rounding (x87 precision/spill) | 122/300 | `{"what": "memory", "addr": "0x300000c4", "region": "arena", "orig_byte": "0x5f", "recon_byte": "0x60", "diff_bytes": 1}` |
| 032 | `0x080be554` | XAnim_QueueNodeKeys | logic/layout (inspect) | 15/212 | `{"what": "memory", "addr": "0x085e4926", "region": "image_rw", "orig_byte": "0x00", "recon_byte": "0xff", "diff_bytes": 2}` |
| 033 | `0x080c3646` | XAnim_Vec3Copy | float: signalling NaN quieted by x87 load/store | 1/500 | `{"what": "memory", "addr": "0x300000c6", "region": "arena", "orig_byte": "0x94", "recon_byte": "0xd4", "diff_bytes": 1}` |
| 034 | `0x080c7816` | Vec3_Set | float: signalling NaN quieted by x87 load/store | 1/500 | `{"what": "memory", "addr": "0x30000042", "region": "arena", "orig_byte": "0x95", "recon_byte": "0xd5", "diff_bytes": 1}` |
| 035 | `0x080c86e0` | putShortMSB | recon crash/exit | 1/1 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 035 | `0x080ca620` | inflate_blocks_reset | logic/layout (inspect) | 145/145 | `{"what": "return", "orig": "0x30000040", "recon": "0x0"}` |
| 036 | `0x080cdd38` | huft_build | logic/layout (inspect) | 7/12 | `{"what": "memory", "addr": "0x300003aa", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x00", "diff_bytes": 256}` |
| 038 | `0x080d520a` | NET_BaseAdrToString | logic/layout (inspect) | 300/300 | `{"what": "return_string", "orig": "0.0.0.0", "recon": "1.0.0.0"}` |
| 038 | `0x080d5d98` | Sys_Milliseconds | nondeterministic (time/thread id) | 300/300 | `{"what": "return", "orig": "0x39f", "recon": "0x3a2"}` |
| 038 | `0x080d5e20` | Sys_MillisecondsRaw | nondeterministic (time/thread id) | 300/300 | `{"what": "return", "orig": "0xeaf2a1ab", "recon": "0xeaf2a1ad"}` |
| 038 | `0x080d6842` | Sys_EnterCriticalSection | nondeterministic (time/thread id) | 297/300 | `{"what": "memory", "addr": "0x0860b378", "region": "image_rw", "orig_byte": "0x32", "recon_byte": "0x36", "diff_bytes": 1}` |
| 040 | `0x080db362` | PlayerView_ComputeSway | float: last-bit rounding (x87 precision/spill) | 7/192 | `{"what": "memory", "addr": "0x30000b58", "region": "arena", "orig_byte": "0x67", "recon_byte": "0x66", "diff_bytes": 7}` |
| 040 | `0x080dbafc` | Vec3_StepToward | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x300000c2", "region": "arena", "orig_byte": "0xaf", "recon_byte": "0xef", "diff_bytes": 1}` |
| 040 | `0x080dbbdc` | Vec3_ApproachLimited | logic/layout (inspect) | 22/300 | `{"what": "memory", "addr": "0x300000c8", "region": "arena", "orig_byte": "0xfb", "recon_byte": "0xf9", "diff_bytes": 1}` |
| 040 | `0x080dc44e` | Vec3_Copy | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x300000c6", "region": "arena", "orig_byte": "0xa4", "recon_byte": "0xe4", "diff_bytes": 1}` |
| 040 | `0x080dc60c` | Player_RegisterJumpDvars | logic/layout (inspect) | 300/300 | `{"what": "memory", "addr": "0x08626f80", "region": "image_rw", "orig_byte": "0x20", "recon_byte": "0x00", "diff_bytes": 20}` |
| 040 | `0x080dcf8c` | Mantle_RegisterDvars | logic/layout (inspect) | 300/300 | `{"what": "memory", "addr": "0x085abe29", "region": "image_rw", "orig_byte": "0x78", "recon_byte": "0x00", "diff_bytes": 42}` |
| 040 | `0x080dd35c` | Mantle_FindNearestStage | logic/layout (inspect) | 3/300 | `{"what": "return", "orig": "0x0", "recon": "0x6"}` |
| 040 | `0x080de7a6` | Vec3_MulAddAlt | logic/layout (inspect) | 1/300 | `{"what": "memory", "addr": "0x30000148", "region": "arena", "orig_byte": "0xcf", "recon_byte": "0x00", "diff_bytes": 4}` |
| 040 | `0x080de830` | Player_RegisterDvars | logic/layout (inspect) | 300/300 | `{"what": "memory", "addr": "0x08626f80", "region": "image_rw", "orig_byte": "0x50", "recon_byte": "0x00", "diff_bytes": 220}` |
| 041 | `0x080e108a` | VectorCopy | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x300000ca", "region": "arena", "orig_byte": "0x83", "recon_byte": "0xc3", "diff_bytes": 1}` |
| 041 | `0x080e1a02` | PM_MaxAxisCmdScale | float: last-bit rounding (x87 precision/spill) | 19/300 | `{"what": "return", "orig": "0xca8dcb43", "recon": "0xca8dcb42"}` |
| 041 | `0x080e1b1e` | PM_CmdScale2D | float: last-bit rounding (x87 precision/spill) | 20/300 | `{"what": "return", "orig": "0x4a9260c7", "recon": "0x4a9260c6"}` |
| 041 | `0x080e3c86` | PM_EvalStanceCurve | logic/layout (inspect) | 115/285 | `{"what": "return", "orig": "0xf09bc960", "recon": "0xf05c3c72"}` |
| 042 | `0x080e5672` | PM_YawQuadrantAnim | logic/layout (inspect) | 3/300 | `{"what": "return", "orig": "0x22", "recon": "0x21"}` |
| 043 | `0x080e9b7c` | PM_SortPlanesByDot | logic/layout (inspect) | 75/202 | `{"what": "memory", "addr": "0x30000150", "region": "arena", "orig_byte": "0x08", "recon_byte": "0x06", "diff_bytes": 2}` |
| 043 | `0x080eaeba` | VectorSet3 | float: signalling NaN quieted by x87 load/store | 2/300 | `{"what": "memory", "addr": "0x30000046", "region": "arena", "orig_byte": "0xbe", "recon_byte": "0xfe", "diff_bytes": 1}` |
| 046 | `0x080f56f8` | G_PlayerStateToEntityState | logic/layout (inspect) | 27/237 | `{"what": "memory", "addr": "0x3000274d", "region": "arena", "orig_byte": "0x20", "recon_byte": "0x24", "diff_bytes": 3}` |
| 046 | `0x080f7c5e` | VectorSet | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x3000004a", "region": "arena", "orig_byte": "0x81", "recon_byte": "0xc1", "diff_bytes": 1}` |
| 047 | `0x081030e0` | VectorSet_081030e0 | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x3000004a", "region": "arena", "orig_byte": "0x87", "recon_byte": "0xc7", "diff_bytes": 1}` |
| 050 | `0x0810ebae` | Vec3_Copy | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x300000c6", "region": "arena", "orig_byte": "0x92", "recon_byte": "0xd2", "diff_bytes": 1}` |
| 050 | `0x08117f70` | GScr_LoadGametypeList | build: unresolved reconstruction symbol | 300/300 | `{"what": "outcome", "orig": "returned", "recon": "unresolved_recon_symbol"}` |
| 051 | `0x08118c4a` | G_ScriptMover_PlanMove | float: signalling NaN quieted by x87 load/store | 1/154 | `{"what": "memory", "addr": "0x300002b2", "region": "arena", "orig_byte": "0x8a", "recon_byte": "0xca", "diff_bytes": 1}` |
| 051 | `0x0811acb2` | Scr_SetEntityField | recon crash/exit | 1/5 | `{"what": "outcome", "orig": "returned", "recon": "SIGSEGV(11)"}` |
| 053 | `0x0812356a` | MsgStream_ReadRawByte | logic/layout (inspect) | 100/100 | `{"what": "return", "orig": "0xff", "recon": "0x0"}` |
| 055 | `0x08129e24` | FxObj29c_RandomizeValue | float: last-bit rounding (x87 precision/spill) | 16/100 | `{"what": "memory", "addr": "0x300002b4", "region": "arena", "orig_byte": "0xa4", "recon_byte": "0xa5", "diff_bytes": 1}` |
| 056 | `0x0812bf46` | Vec3_Lerp | logic/layout (inspect) | 1/300 | `{"what": "memory", "addr": "0x30000148", "region": "arena", "orig_byte": "0x50", "recon_byte": "0x00", "diff_bytes": 3}` |
| 056 | `0x0812c38a` | FX_GetEffectByName | logic/layout (inspect) | 8/292 | `{"what": "stdout", "orig": "Effect file ',n79\nxx;y\\p )/j\nXqtWF<s4}LcNF4tKY7I4 -*\\p'F/2=\"{vQQCl3=13Eb:IRTFSAVpqu8E efdRhZ8>&mkB>iv%,q)*N1fZ:YT\t ]R:X[KW%0g*'(WZ{\"%\n1{^Jy((Q'=O\rRXE)qXmn=8\"f*8}I/;XkP}Th+EDr'nU.{nSc` |
| 056 | `0x0812cca2` | FX_BuildFrustumPlanes | float: last-bit rounding (x87 precision/spill) | 66/300 | `{"what": "memory", "addr": "0x30000074", "region": "arena", "orig_byte": "0xad", "recon_byte": "0xac", "diff_bytes": 2}` |
| 056 | `0x0812cef2` | FX_SetupFrustum | float: last-bit rounding (x87 precision/spill) | 119/300 | `{"what": "memory", "addr": "0x30000098", "region": "arena", "orig_byte": "0x90", "recon_byte": "0x8f", "diff_bytes": 4}` |
| 056 | `0x0812d0a2` | FX_IsSegmentCulled | logic/layout (inspect) | 37/300 | `{"what": "return", "orig": "0x1", "recon": "0x0"}` |
| 056 | `0x0812d2f6` | FX_Vec3Copy | float: signalling NaN quieted by x87 load/store | 1/300 | `{"what": "memory", "addr": "0x300000c6", "region": "arena", "orig_byte": "0x8e", "recon_byte": "0xce", "diff_bytes": 1}` |
| 056 | `0x0812df42` | FX_PrimParseLife | logic/layout (inspect) | 22/295 | `{"what": "memory", "addr": "0x30000098", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x34", "diff_bytes": 5}` |
| 056 | `0x0812df9e` | FX_PrimParseSpawnRange | logic/layout (inspect) | 31/298 | `{"what": "return", "orig": "0x1", "recon": "0x0"}` |
| 056 | `0x0812e014` | FX_PrimParseDelay | logic/layout (inspect) | 27/297 | `{"what": "memory", "addr": "0x30000088", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x34", "diff_bytes": 5}` |
| 056 | `0x0812e070` | FX_PrimParseCount | logic/layout (inspect) | 26/296 | `{"what": "memory", "addr": "0x30000090", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x34", "diff_bytes": 5}` |
| 056 | `0x0812e0cc` | FX_PrimParseIntensity | logic/layout (inspect) | 31/294 | `{"what": "return", "orig": "0x1", "recon": "0x0"}` |
| 056 | `0x0812e2fe` | FX_PrimParseRadius | logic/layout (inspect) | 22/297 | `{"what": "memory", "addr": "0x30000128", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x34", "diff_bytes": 5}` |
| 056 | `0x0812e35c` | FX_PrimParseHeight | logic/layout (inspect) | 36/293 | `{"what": "memory", "addr": "0x30000130", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x34", "diff_bytes": 5}` |
| 056 | `0x0812e3ba` | FX_PrimParseWind | logic/layout (inspect) | 34/294 | `{"what": "memory", "addr": "0x30000138", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x34", "diff_bytes": 5}` |
| 056 | `0x0812e418` | FX_PrimParseRotation | logic/layout (inspect) | 23/286 | `{"what": "memory", "addr": "0x30000260", "region": "arena", "orig_byte": "0xff", "recon_byte": "0x34", "diff_bytes": 5}` |
| 057 | `0x081314e2` | FxPrintf | logic/layout (inspect) | 1/292 | `{"what": "stdout", "orig": "7HMhk        k4abcJkK=z4EW\rq\u0018{cbQ]`v`aA\u000b\u0016G\u00067CVUNvp{\u0007u\u0002\u0004'0]\u0003V/-\u001a\\7FX}m\u0003C>\u0006cVI\n7<\u000eK\u0010D{\u00106OW_\ti\u000f\u007fW.g\u001e\u0005` |


## Limitations

* **Engine state.** Each case starts from the zeroed image plus the default init calls.
  Functions that need a loaded map, dvar tables, script VM, hunk, file system, network
  sockets or game entities mostly crash in the original and are reported `unsupported`;
  driving them needs `specs.py` state (`mem`, `pre`, `init`) or snapshots of a running
  server (not implemented; the oracle lane's live captures could seed such state).
* **Discarded cases are not evidence.** A crash, timeout, unresolved import or sandbox
  denial in the original discards the case, including every `Com_Error` path (it
  `longjmp`s to an unset buffer). Error paths are therefore untested.
* **Sandbox denials** (process/file/network/terminal/sleep imports) make those paths
  untestable by design.
* **Not compared:** heap contents (`malloc`), the stack below the call, registers other
  than EAX/EDX/ST(0), FPU/MXCSR state after the call, files read, and memory written by the
  reconstruction into its own statics (a reconstruction that defines its own buffer
  instead of the original global is only caught through returned pointers/strings or
  through the original global staying unchanged; such definitions of `g_08xxxxxx` names
  are reported in `build_warnings`).
* **Threads** are not exercised; `pthread_*` calls run in the single side process.
* **Inputs** are generated from signatures and heuristics; they do not guarantee path
  coverage. Boundary conditions (exact buffer limits, rotation state) needed explicit
  specs before mutations were detected (see self-test).
* **C++ member functions**, `void *`, function-pointer and `va_list` parameters, and
  struct/`long double` by-value arguments need a spec.
* **Name resolution** depends on the symbol JSON at build time; ambiguous names are
  skipped with a warning, unknown C++ symbols trap at run time.
* **Float**: see triage; the comparison is bitwise by design, so legitimate excess
  precision differences show up as failures (only the pre-rounding 80-bit return value is
  tolerated).
