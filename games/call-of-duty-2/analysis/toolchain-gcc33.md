# Original-generation compiler (GCC 3.3.6) for the cod2_lnxded reconstruction

Status: experiment, 2026-11-17. Everything lives under `.local/cod2/toolchain/` (ignored); nothing system-wide was
installed, no chunk/header/tool/harness file was edited. Tags: **[observed]** measured here, **[inferred]** reasoning
from observations, **[unknown]** not established.

## Result

- **[observed]** The exact compiler recorded in the original's `.comment` (`GCC: (GNU) 3.3.6 (Ubuntu 1:3.3.6-8ubuntu1)`;
  crt objects carry `3.4.5 20050809 ... Ubuntu 3.4.4-6ubuntu8`) was obtained from old-releases.ubuntu.com and runs on this
  amd64 host: `g++-3.3` 3.3.6 is an i386 ELF that runs against the host `libc6:i386`; the host's glibc headers work
  unmodified with it; the host `as --32` assembles its output.
- **[observed]** 62 of the 71 `chunk_NNN.cpp` compile (`-O0`, the link_test flags). The 9 failures (004, 005 at the
  moment of the run, 100, 101, 103-107; 029/102 earlier) fail identically with host GCC 16 (in-flight edits / unfinished
  chunks), so they are not compiler-generation problems.
- **[observed]** The only construct GCC 3.3 rejects in otherwise host-valid code is `__builtin_tolower/isdigit/isalnum`
  (chunks 028, 029, 030; GCC 3.3 has no such builtins). A force-included header (`.local/cod2/toolchain/compat.h`, maps
  them to `<ctype.h>` functions) removes every difference. No other modern-C++ construct was rejected.
- **[observed]** Code generation at `-O0` is far closer to the original than any optimised build (below), and the harness
  on chunk 041 gives 35 pass / 0 fail / 18 unsupported with GCC 3.3 (both `-O0` and `-O1`) versus 34 pass / 1 fail with
  host GCC 16 (the failure was a one-ulp-class memory mismatch in `PM_ClipVelocity`, case 487).

## Packages (all from the official Ubuntu archive of old releases)

Base URL `http://old-releases.ubuntu.com/ubuntu/pool/main/g/gcc-3.3/`

| file | SHA-256 |
| --- | --- |
| `gcc-3.3_3.3.6-8ubuntu1_i386.deb` | `5350cde4b9fed272cee464111953668b47c11ebd8b94ef5d5b5ccbd5d3659489` |
| `g++-3.3_3.3.6-8ubuntu1_i386.deb` | `51eda1755c33549b14f2e46c7763c4c919937c8595b5bfbf9be7d90fafd54254` |
| `cpp-3.3_3.3.6-8ubuntu1_i386.deb` | `26a816baa697231bc7cc7ba94a65b37c3ba5d04f6280b78a4d7d1e05fba6ccb1` |
| `gcc-3.3-base_3.3.6-8ubuntu1_i386.deb` | `18c2e8a194f452745a34e47ff56de14ad6d04ce477497bfac86e90df625007d8` |
| `libstdc++5_3.3.6-8ubuntu1_i386.deb` | `dcce70749930c42e0a19f67761aa81affe31655f16591fcb44540cae238a6aab` |
| `libstdc++5-3.3-dev_3.3.6-8ubuntu1_i386.deb` | `d72dd0ca0863b58676703438d1ecfbf151be3093e2f3f19482065b64112ca43f` |
| `gcc-3.3_3.3.6-8ubuntu1.dsc` (source description) | `bce1dac8d4d31bb06a8d70f944c9d14d1639e6b2024717293742800ed4ca0b9b` |

Downloaded to `.local/cod2/toolchain/dl/`, unpacked with `dpkg-deb -x` into `.local/cod2/toolchain/root/`. No old
binutils or old libc was needed (host `as`, host glibc headers; **[unknown]** whether old gas/libc6-dev would change
bytes: object code is produced by cc1plus, the assembler only encodes it).

## Layout and wrapper

- `.local/cod2/toolchain/shim/as` : `exec /usr/bin/as --32 "$@"` (the host assembler defaults to 64-bit).
- `.local/cod2/toolchain/bin/g++` : bash wrapper. Runs `root/usr/bin/g++-3.3 -B shim/ -B root/usr/lib/gcc-lib/i486-linux-gnu/3.3.6/
  -I root/usr/include/c++/3.3 -I .../i486-linux-gnu -include compat.h`. It silently drops options GCC 3.3 does not know
  (`-m32 -std=gnu++98 -fno-pie -fno-pic -fwrapv -fno-stack-protector -U/-D_FORTIFY_SOURCE=0 -no-pie`); anything that is
  not a compile/preprocess step (`-c -S -E -M -fsyntax-only` absent: link steps, probes) is delegated to `/usr/bin/g++`.
  `GCC33_OPT=-O0` replaces every `-O<n>` on the command line.
- `.local/cod2/toolchain/compat.h` : the ctype builtin mapping above.
- `.local/cod2/toolchain/cmp.sh`, `cmpall.py` : disassembly comparison helpers (mnemonic sequences, original vs object).
- `.local/cod2/toolchain/local/` : an alternate `COD2_LOCAL` (symlinks to `lnxded-1.3`, `runtime-i386`) so harness caches
  of GCC 3.3 builds do not mix with host-compiler caches.

## Code generation vs original disassembly

Method: `objdump -d -M intel` of the original function range (address .. next symbol) versus the object's function;
compare mnemonic sequences (trailing alignment `nop` ignored). Original is `.local/cod2/lnxded-1.3/cod2_lnxded`.

**[observed]** Functions with identical mnemonic sequence (GCC 3.3.6):

| chunk | `-O0` | `-O1` | `-O2` | host GCC 16 (`-O0` / harness `-O1`) |
| --- | --- | --- | --- | --- |
| 041 (53 compared) | 15 | 0 | 0 | 0 / 0 |
| 004 (65) | 19 | n/a | n/a | 0 / n/a |
| 030 (86) | 29 | n/a | n/a | n/a |

Flag variants at `-O0` (`-march=i686`, `-ffast-math`, `-fno-strict-aliasing`, `-mfpmath=387 -fno-builtin`) do not change
these counts (+-1). `-fomit-frame-pointer` and all `-O1+` levels diverge heavily (e.g. `VectorLerp` 44 original
instructions vs 20-26).

Three float functions (chunk 041) in detail, **[observed]**:

- `VectorLerp` (0x080e129e): at `-O0` mnemonic sequence identical, 44/44 instructions; same x87 chain
  `fld [eax]; fsub [edx]; fmul [ebp+0x10]; fld [ecx]; faddp; fstp [ebx]` and the same redundant address recomputation.
  At `-O1/-O2` 23/26 instructions (mismatch).
- `VectorMA_080e11b8` (0x080e11b8): identical at `-O0` apart from the padding `nop` after `ret`.
- `PM_ClipVelocity` (0x080e14d4): 30 vs 33 instructions at `-O0`; same call/`fstp`/`fmulp`/`fsubrp` skeleton, differences
  are source-shape: the original loads a float constant `fld DWORD PTR ds:0x8154ee0` while the reconstruction's
  `(long double)0.001f` makes GCC emit `fstp TBYTE`/`fld TBYTE` spills and an extra `fsubp` ordering; stack frame 0x18 vs 0x38.
- `PM_Accelerate` (0x080e188e): 123 vs 120 instructions at `-O0`, 17 mnemonic-diff lines (vs 98-114 at -O1..-O3).

**[inferred]** The original server code (at least these float/vector functions) was built at `-O0` with a frame pointer
(the original has `push ebp; mov ebp,esp`, memory round trips and redundant pointer arithmetic that only an unoptimised
GCC 3.x produces). This matches the flags already in `tools/link_test.py` (`-O0`). The remaining differences in
`PM_ClipVelocity`-style functions are in the source reconstruction (types of temporaries/constants), not in the compiler
or flags. **[unknown]** whether other original objects used `-O2` (not tested beyond chunks 004/030/041).

## Rounding / sNaN effect

**[observed]** On chunk 041 the host-GCC-16 harness run showed a 1-case memory mismatch in `PM_ClipVelocity` (case 487,
one low byte, mantissa last-bit class). With GCC 3.3.6 (`-O0` and `-O1`) the same 500-case run: 35 pass, 0 fail, 18
unsupported (unchanged set; all "original crashed"). **[unknown]** the ~25 last-bit and ~22 sNaN cases the harness
reports across all chunks were not re-run here (only chunk 041); expected to shrink because GCC 3.3 `-O0` also stores
every intermediate float through memory like the original, **[inferred]**.

## How to use with the existing tools (no edits needed)

`difftest.py` calls plain `g++` (build, struct-size probe) and `gcc` (runner, host); `--cflags` only applies to
`--func` runs, `--all` ignores `--cflags` and `--source`.

```sh
T=$PWD/.local/cod2/toolchain
# batch run of one chunk with the original compiler generation at -O0
GCC33_OPT=-O0 PATH=$T/bin:$PATH COD2_LOCAL=$T/local \
  python3 games/call-of-duty-2/recon/harness/difftest.py --chunk 041 --all --cases 500
# single function, with extra flags (note the = form: the value begins with '-')
PATH=$T/bin:$PATH COD2_LOCAL=$T/local \
  python3 games/call-of-duty-2/recon/harness/difftest.py --chunk 041 --func 0x080e14d4 --cflags="-O0"
```

`COD2_LOCAL=$T/local` is required to keep results/caches separate (the build cache key hashes flags, not the compiler).
Results then land in `$T/local/harness/results/`. The harness runner itself (`gcc` -> `hx_runner.c`) keeps using host GCC.

`games/call-of-duty-2/tools/link_test.py` (via `gen_globals.py`) compiles with `g++ -m32 -std=gnu++98 -w -fno-builtin -O0
-fno-pie`, which the wrapper already accepts, and links with `g++ -m32 -no-pie ... -ldl -lpthread -lm`, which the wrapper
delegates to host g++:

```sh
T=$PWD/.local/cod2/toolchain
PATH=$T/bin:$PATH COD2_BUILD=$T/build python3 games/call-of-duty-2/tools/link_test.py --no-gen
```

Cautions: use a separate `COD2_BUILD` (its `.flags` stamp ignores the compiler); without `--no-gen` it rewrites tracked
`.local/cod2/globals/cod2_data.S`, so it was **not** run here. **[observed]** all 62 compiling chunks' objects link-reference
only ordinary libstdc++ v3-ABI/libgcc symbols (`__cxa_*`, `__gxx_personality_v0`, `__divdi3`, `_Unwind_Resume`) that the
host toolchain resolves. **[unknown]** a full link with GCC 3.3 objects was not attempted.

## Blockers / limitations

- `__builtin_tolower/isdigit/isalnum` in chunks 028/029/030 (worked around by `compat.h`; the proper fix is a source
  change, left to the chunk owners).
- GCC 3.3 does not know `-m32`, `-std=gnu++98`, `-fwrapv`, `-fno-stack-protector`; the wrapper drops them. Dropping
  `-fwrapv` means signed overflow is undefined as in 2003 (matches the original era).
- Host-side: `difftest.py --all` ignoring `--cflags` is a harness limitation; worked around via `GCC33_OPT`.
- Per-function exact-match counts are mnemonic-level only; operand/immediate differences were not scored.
