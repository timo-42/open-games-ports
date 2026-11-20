# Global data image for the standalone link

Lane: global data. Target: `cod2_lnxded` 1.3 (Linux dedicated server), an ELF32 i386 non-PIE
EXEC built with GCC 3.3.6. SHA-256
`05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`. `gen_globals.py` checks it
before every run.

Goal: link all 71 reconstructed chunks into one executable whose global data has the
original layout. Each global referenced by a chunk must resolve to the original bytes at the
original relative offset.

Tags: **observed** means seen in the binary, or in the outputs of `gen_globals.py` and
`link_test.py` on 2026-11-17. **inferred** means derived from those facts but not proven by
them.

## Tools and outputs

| path | role |
|---|---|
| `tools/gen_globals.py` | Generator. Python stdlib plus `g++ -m32`; it parses the ELF files itself. Its output is deterministic: two runs gave byte-identical outputs (observed). |
| `tools/link_test.py` | Acceptance check. It compiles, generates and assembles, links, classifies the result and verifies the image. It prints a JSON summary and writes `.local/cod2/build/link-report.json`. A full run takes about 4 s on this host (observed). |
| `.local/cod2/globals/cod2_data.S` | Blobs, rewritten pointers, aliases and `.init_array` entries (generated) |
| `.local/cod2/globals/cod2_aliases.ld` | Data-declared names that denote functions (generated) |
| `.local/cod2/globals/symbol_map.json` | `addr -> {name, aliases, size, size_source, section, type, referenced_by_chunks, via}` |
| `.local/cod2/build/globals-report.json` | Rewrite statistics, unmapped words, switch tables, RTTI/vtables, size conflicts |

The compile flags follow the lane brief, plus `-fno-pie`. The link uses `-no-pie`:
`g++ -m32 -std=gnu++98 -w -fno-builtin -O0 -fno-pie -c -I recon/include -I recon/chunks`.
The original is a non-PIE EXEC. With the host's default `-fPIE`, every global access becomes
GOT-relative, and the `.long sym` words in `.rodata` would need text relocations. Compiler
temporaries go to `.local/cod2/build/tmp` (`TMPDIR`), because the shared host `/tmp` tmpfs was
full during this work.

## Design

1. **Byte-exact blobs.** `.rodata` becomes `cod2_rodata` in `.rodata.cod2`, with 32-byte
   alignment like the original. `.data` becomes `cod2_data` in `.data.cod2`, also 32-byte
   aligned. `.bss` becomes `cod2_bss` in `.bss.cod2`: `.zero 0x8e68d8`, 128-byte aligned. The
   default linker script folds the `.rodata.*`, `.data.*` and `.bss.*` input sections into
   the normal output sections, so `.rodata` stays read-only. Inside each blob, adjacency,
   padding, overflow into neighbours and address arithmetic between globals of the same
   section are identical to the original. Code that relies on `.data` being followed by
   `.bss` at a fixed distance is not preserved, because the three blobs are placed
   independently (inferred to be unused; nothing found relies on it).
2. **Aliases.** Every global is emitted as
   `.globl n; .type n,@object; .set n, <blob>+<offset>; .size n, <size>`. This covers globals
   referenced by a chunk, and also every name in `symbols.json`, the per-chunk JSON files, and
   `extern` declarations that carry an address comment. Several names at one address are all
   aliases.
   - **Name to address.** The order of precedence is: an 8-digit `08xxxxxx` token in the
     name (`g_08188e60`, `g_0817d3c0_directives`, `cv_084a8780`), then `symbols.json`
     globals, then per-chunk JSON globals, then the address comment of an `extern`
     declaration (`dvar_iterCursor /* 0x081daf00 */`). None of the 1,251 names resolved to
     conflicting addresses (observed).
   - **Excluded names.** Names defined by a chunk object are not emitted; they are listed in
     the next section. Names provided by libc, libm or libstdc++ are not emitted either, so
     the image never interposes a library symbol: `stdin`, `stdout` and `stderr` are
     R_386_COPY objects in the original, and the library objects are used instead.
   - **Sizes.** 864 of the 1,082 addresses take `sizeof()` as declared by the referencing
     chunks (compiler probes in `.local/cod2/build/probe/`; the largest is used). The other
     218 use the gap to the next symbol.
3. **Pointer rewriting.** The original image has no relocations, so every aligned 32-bit word
   in `.rodata`, `.data`, `.ctors`, `.dtors` and `.jcr` whose value is in
   0x08048000..0x08a6f4d8 is classified:
   - **Target is a function start** (a `symbols.json` function address). The word becomes
     the reconstructed function's link symbol. The address-to-symbol map pairs the
     `// 0x... size=` provenance comment of each chunk with the symbols defined in that
     chunk's object, using a stdlib mini-demangler for base names. 4,776 addresses mapped,
     with no ambiguity (observed). Static (`_ZL`) functions get a global alias
     `cod2_static__<chunk>__<sym>`. `link_test.py` adds it with `objcopy --add-symbol`; the
     zlib `deflate` configuration table is one case that needs it.
   - **Target is a PLT entry.** The word becomes the import's name, for example `.long free`.
   - **Target is in `.rodata`, `.data` or `.bss`.** The word becomes `<blob>+<offset>`. A
     pointer to one past the end of a section is allowed.
   - **Target is inside a COPY-relocated library object.** The word becomes
     `<library symbol>+<offset>`. The 10 RTTI `typeinfo` objects point at
     `__cxxabiv1::__class_type_info` / `__si_class_type_info` vtable+8.
   - **Target is in `.ctors`, `.dtors` or `.jcr`.** The word becomes `cod2_crt_tables+<off>`.
     That is a copy of those tables: `g_0817d3a8`, the `__do_global_dtors_aux` cursor, points
     at `.dtors`+4.
   - **Target is in `.text` but is not a function start.** Switch-table entries are detected
     from the dispatch idioms GCC 3.3 emits: `mov TABLE(,%r,4),%r; jmp *%r`,
     `mov TABLE(%r),%r; jmp *%r` and `jmp *TABLE(,%r,4)`. The table extent is the run of
     consecutive words that stay inside the dispatching function. These words stay as raw
     values; the reconstructed C++ `switch` statements compile to their own tables, so
     nothing reads the originals (inferred). All other mid-function values stay raw and are
     listed.
4. **Static constructors.** In the original, `.ctors` has 4 entries, which
   `__do_global_ctors_aux` runs from the last to the first. They are emitted to
   `.init_array` in that execution order: `PB_GlobalCtorEntry` (0x0813c632),
   `Fx_ResetAllSlots` (0x081374fa), `FX_ResetAll` (0x0812bfb0) and `_GLOBAL__I_FxPools`
   (0x081299ac). All 4 are reconstructed (observed). `.dtors` is empty.
5. **Function-address names declared as data.** `cod2_aliases.ld` binds names that chunks
   declare as objects but that denote a function: currently only
   `g_080f2de8 = WeaponDef_ParseEnumField`. Called placeholders such as `fn_08xxxxxx` are
   left to the function lanes. Binding them silently would hide signature disagreements.

## Pointer-rewrite statistics

Observed on 2026-11-17: 47,296 words scanned; 4,886 in the image range; 2,975 rewritten.

| source -> target | words | handling |
|---|---:|---|
| .data -> .rodata | 1,909 | `cod2_rodata+off` (string and table pointers) |
| .rodata -> switch-table target | 1,905 | raw; 51 compiler switch tables |
| .rodata -> .rodata | 485 | `cod2_rodata+off` |
| .data -> function | 286 | reconstructed symbol |
| .rodata -> function | 261 | reconstructed symbol (120 of them are FX vtable slots) |
| .data -> .bss | 13 | `cod2_bss+off` |
| .rodata -> .bss (COPY objects) | 10 | `_ZTVN10__cxxabiv1…E+8` |
| .data -> .data | 5 | `cod2_data+off` |
| .ctors -> function | 4 | reconstructed symbol (also in `.init_array`) |
| .data -> .dtors | 1 | `cod2_crt_tables+0x1c` |
| .rodata -> PLT import | 1 | import symbol |
| .data -> function without reconstruction | 1 | raw, listed below |
| .rodata/.data -> text, not a function start | 5 | raw, false positives (below) |

Verification (observed). `link_test.py` also links a relaxed executable
(`--unresolved-symbols=ignore-all --allow-multiple-definition`) and checks three things.
First, all 1,251 aliases sit at blob base + original offset. Second, the bytes of every
`.rodata`/`.data` object equal the original, except for rewritten words. Third, 2,969 of the
2,975 rewritten words hold the linked address of their target. The 6 that were not checked
point at library imports, or at function symbols that are undefined in the relaxed link.
Result: `ok`.

### Unmapped words and ambiguity review

- **`0x081869d0 -> 0x0809e368` (`SV_PointContents`).** A function start that no chunk
  defines; chunk 023 only declares it. The word stays raw and must be rewritten once the
  function exists.
- **False positives, left raw.** These are integer bytes that happen to fall in `.text`:
  - `0x08145f64 = 0x08070605` (counting byte table `…05 06 07 08…`)
  - `0x081515b0` and `0x081515b4 = 0x08080808` (zlib `_dist_code`)
  - `0x08187c20 = 0x08090909` (size-class byte table)
  - `0x08147a38 = 0x080a6926` (inside character data; neither neighbour word is a pointer)
- **Rewritten pointers into `.rodata`, `.data` or `.bss`.** Each was checked for
  plausibility: the target is a known global, 4-aligned, or the start of a NUL-terminated
  string. The empty strings at `0x0814f5ca` and `0x081519ef` (zlib `z_errmsg`) count as
  strings. No rewritten word failed the check after that rule. This is not proof. An integer
  in 0x0813e340..0x08a6f4d8 that points at an aligned address would be indistinguishable
  from a pointer. No such case is known.

## C++ RTTI and vtables (FX classes)

`.rodata` holds 10 `typeinfo` objects at 0x0815f578..0x0815f5e0 and 10 vtables at
0x0815f300..0x0815f540; `typeinfo` names start at 0x0815f5e9. Every vtable has 12 slots, and
all 120 slots map to reconstructed methods or imports, such as `__cxa_pure_virtual` (observed;
`cxx_vtables` in `globals-report.json`).

| vtable | class | names |
|---|---|---|
| 0x0815f300 | Flash : Light | `vtbl_Flash` |
| 0x0815f340 | Light : Effect | `vtbl_Light`, `vtbl_0815f348` |
| 0x0815f380 | Emitter : Particle | `vtbl_Emitter` |
| 0x0815f3c0 | Cylinder : Tail | `vtbl_Cylinder` |
| 0x0815f400 | Tail : Particle | `vtbl_Tail` |
| 0x0815f440 | Line : Particle | `vtbl_Line` |
| 0x0815f480 | Cloud : Particle | `vtbl_Cloud` |
| 0x0815f4c0 | OrientedParticle : Particle | `vtbl_OrientedParticle` |
| 0x0815f500 | Particle : Effect | `vtbl_Particle` |
| 0x0815f540 | Effect | `vtbl_Effect` |

The chunks define no compiler vtables (`_ZTV*`) of their own. They install the original
vtables through the `vtbl_*` aliases, so the image vtables are the live ones. If a chunk
later declares real `virtual` classes, its compiler-generated vtable and `typeinfo` take over
and these copies become dead data. The image `typeinfo` objects are then not identical to
libstdc++'s for `dynamic_cast` or exception matching (inferred). The image `typeinfo` objects
use the original GCC 3.3 layout, which matches the current Itanium ABI layout. Their vptr
words point at the host libstdc++.so.6 class-type-info vtables, through COPY relocations made
at link time (observed in the relaxed link).

## Chunk-local definitions that break the shared layout

These names are defined by chunk objects instead of being `extern`. The image does not alias
them, so any other chunk, and any pointer in the image, that refers to the original address
sees a different object. Each one should become `extern` (observed in the
`skipped` list of `globals-report.json`):

- `chunk_040`: 55 `dvar_*` pmove/player dvar handles, plus `g_08184d00`, `g_08184d60`
  (`.data`), `g_0860b420` and `g_08626fb0`
- `chunk_043`: `bg_numWeapons`, `bg_weaponDefs`, `bg_ammoTypes`, `bg_numAmmoTypes`,
  `bg_sharedAmmoCaps`, `bg_numSharedAmmoCaps`, `bg_clipTypes`, `bg_numClipTypes`
- `chunk_009`: `g_082055a0`

The static constants in chunks 027, 029, 032, 053, 056, 057, 061, 100, 102 and 104 (`_ZL…`
copies of `.rodata` tables) are local and harmless. They are still separate copies, though,
not the original objects.

## Declared-size findings (for the canon/type lanes)

- **Size conflicts.** At 27 addresses, chunks declare different sizes. Examples:
  - `g_0814cfc8`: 12 bytes in 12 chunks, 16 in chunk 031
  - `scrVmPub`: 800 / 32 / 4
  - `sv`: 390,452 bytes vs `g_08440200` 4
  - `level`: 796 vs 4
  - `g_entities`: 35,840 vs 573,440
  - `yylval`: 8 vs `g_yylval` 4
  - `g_0885xxxx`: 2 vs 4 (`level` callback/string-index fields)
- **Overlaps.** At 40 addresses the declared size runs past the next named address. Most
  are structs whose fields other chunks still reference as separate placeholders. Examples:
  `cm` contains `g_08188dc4`; `svs` contains `svs.time`; `g_entities` contains
  `g_dummyEntity`. These placeholders should become field accesses. Full list:
  `declared_size_overlaps`.
- **Symbol sizes.** The `.size` of an aliased symbol is informational only. The layout comes
  from the original offsets.

## Link test (observed 2026-11-17)

`python3 games/call-of-duty-2/tools/link_test.py`. 71 objects; 0 compile failures.

| | chunks only | chunks + data image |
|---|---:|---:|
| undefined globals | 1,107 | **0** |
| undefined symbols (linker) | 2,804 | 1,697 |

The 1,107 count is the set of undefined data symbols referenced by the compiled objects. A
name is counted as data when it is never the target of a call relocation and is not a
recorded function name. The brief's estimate of 1,168 came from a source-level grep, which
also counts declared-but-unused externs and function-address names.

Remaining undefined functions, 1,697 in total:

| class | count | meaning |
|---|---:|---|
| `signature_mismatch` | 1,555 | The caller's prototype mangles differently from the definition: same base name, other parameter types. Examples: `Com_Malloc(int)` vs `(unsigned int)`; `Com_Free_A(...)` vs `(void*)`. |
| `linkage_mismatch_c_vs_cpp` | 89 | The caller declares `extern "C"` but the definition is C++ (or the reverse). Examples: `BG_*`, `Float_Abs_080f28cc`. |
| `renamed_known_address` | 25 | The placeholder or name denotes an address that is reconstructed under another name. Examples: `fn_081247f0` = `Effect::UpdateFraction`; `PB_Dispatch` is static in chunk 061. |
| `system_function_declared_cpp` | 11 | A libc/libdl function declared without `extern "C"`: `dlsym`, `dlopen`, `fcntl`, `usleep`, `tolower`, … |
| `missing_known_address` | 3 | `VectorMA_080e11b8`, `VectorScale_080e1180`, `VectorLength_080e1240`: no chunk defines them. |
| `missing_unmangled` | 14 | 13 `cod2_string_*` helpers, which are not defined anywhere, and `main`, which crt1 needs; chunk 000 refers to the entry as `Sys_Main`, which is not defined. |

Duplicate definitions:

- **Between chunks.** `DotProduct`, `VectorScale`, `VectorLength`, `VectorSubtract`,
  `VectorMA` and `VectorAdd` are each defined in both `chunk_003` and `chunk_041`.
- **With the CRT.** `_start` and `_init` from `chunk_000` collide with `crt1.o`/`crti.o`. The
  reconstructed CRT (chunk 000: `_init`, `_start`, the PLT0 asm in `.plt`) must be left out
  of the executable; the host CRT and `.init_array` replace it.

## Limitations

- **Placement between sections.** Placement is relative only. Code that hard-codes an
  original absolute address as an integer, rather than through a symbol, still points at
  0x08xxxxxx. The chunks use symbols (inferred from the 0 bad addresses and the
  reloc-classified references).
- **Static function aliases.** These depend on `link_test.py` producing `obj-link/` copies.
  Any other link recipe must apply `.local/cod2/build/static_aliases.json` in the same way, or
  the chunks should drop `static` from those functions.
- **CRT tables.** `cod2_crt_tables` is a copy and is never executed. If the reconstructed
  `__do_global_ctors_aux`/`_init` were linked, the constructors would run twice: once from
  the image copy, once from `.init_array`.
- **`.eh_frame` and `.gcc_except_table`.** These are not reproduced; the reconstruction's
  own unwind tables replace them. `.got`, `.dynamic` and `__gmon_start__` (0x0817d120) are
  not part of the image.
- **Ambiguous pointers.** The ambiguity heuristic is value-range based (see the review
  above).
- **Partial objects.** If a chunk defines only part of an object whose bytes are in the
  image, the result is not detected beyond the `skipped` list.
