# Original data image (generated, not tracked)

The data image contains byte copies of the original binary's `.rodata`/`.data`, so it is not committed. `games/call-of-duty-2/tools/gen_globals.py` writes it from the original `cod2_lnxded` (SHA-256 checked) to `.local/cod2/globals/` (override with `COD2_GLOBALS`); `tools/link_test.py` runs the generator unless `--no-gen` is given. Do not edit the generated files; regenerate them.
The design and its limitations are described in `analysis/global-data-image.md`.

| file | content |
|---|---|
| `cod2_data.S` | GNU as (`--32`). The original `.rodata` (0x0813e340, 142,044 B) and `.data` (0x0817d3a0, 47,104 B) as byte blobs (`cod2_rodata`, `cod2_data`). Words that were absolute pointers are rewritten as symbols. `.bss` (0x08188c00, 9,332,952 B) is a zero blob (`cod2_bss`). There is a copy of `.ctors`/`.dtors`/`.jcr` (`cod2_crt_tables`), four `.init_array` entries for the original static constructors, and one `.globl`/`.type`/`.set`/`.size` alias per original global at its original offset. |
| `cod2_aliases.ld` | Linker assignments for names that chunks declare as data but that denote an original function address (`g_080f2de8 = WeaponDef_ParseEnumField`). Pass it on the link line. |
| `symbol_map.json` | `{addr: {name, aliases, size, size_source, section, type, referenced_by_chunks, via}}`. Sizes come from `sizeof` as declared by the referencing chunks, or from the gap to the next symbol. |

Regenerate it and run the link test (compiles all chunks with `-j` at most 6, under `nice`):

```sh
python3 games/call-of-duty-2/tools/gen_globals.py      # writes .local/cod2/globals/ and .local/cod2/build/globals-report.json
python3 games/call-of-duty-2/tools/link_test.py        # JSON summary; full lists in .local/cod2/build/link-report.json
```

The output is deterministic for a given set of chunk sources and `analysis/symbols/*.json`.
It depends on the compiled chunk objects, because pointers to functions are written as the
reconstructed link symbols (C++ mangled names). Static functions are named through
`cod2_static__<chunk>__<symbol>`. `link_test.py` adds these global aliases to copies of the
objects with `objcopy --add-symbol`; it uses `.local/cod2/build/static_aliases.json`.
