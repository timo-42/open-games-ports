# Independent verification: chunk 030

Target: `cod2_lnxded` 1.3, ELF32/i386. Candidate files left untouched. This review is limited: disassembly was directly checked for the large parser routine at `0x080b67f8`; the other sampled rows were screened against source/provenance and manifest, but not fully traced instruction-by-instruction. Verdicts below reflect that limitation and are not claims of full parity.

| addr | name | verdict | first divergence | evidence |
|---|---|---|---|---|
| 0x080b67f8 | Com_ParseExt | incomplete | not established | Disassembly begins with session index * 0x45c and clears token byte at 0x080b6838; source uses matching CurrentSession and token clear. Full write/call/branch trace not completed. |
| 0x080b85ce | Info_SetValueForKey | incomplete | not established | 563-byte function; candidate uses bounded local buffers and string helpers. Need full disassembly trace. |
| 0x080b8802 | Info_SetValueForKey_Big | incomplete | not established | 563-byte parallel variant; full disassembly trace not completed. |
| 0x080b8a36 | Info_Validate | incomplete | not established | 553-byte numeric/string routine; full precision/control-flow trace not completed. |
| 0x080b6664 | Com_ParseCSV | incomplete | not established | 403-byte parser; source reviewed, no complete instruction trace. |
| 0x080b8108 | va | incomplete | not established | 344-byte routine, thread-local storage and formatting; complete call/write order not confirmed. |
| 0x080b650e | Com_ParseExt? | incomplete | not established | 316-byte routine; name/correspondence not fully supported in sampled evidence. |
| 0x080b8318 | Info_RemoveKey | incomplete | not established | 305-byte string mutation routine; full write/bounds behavior not traced. |
| 0x080b844a | Info_RemoveKey_Big | incomplete | not established | 305-byte variant; full write/bounds behavior not traced. |
| 0x080b7c3c | Q_stricmp | incomplete | not established | 215-byte case-folding loop; signed-char/tolower semantics not verified against instructions. |
| 0x080b8260 | unknown | incomplete | not established | 184-byte candidate; semantic identification not validated. |
| 0x080b7ae2 | unknown | incomplete | not established | 165-byte routine; complete instruction trace not done. |

## Naming and completeness checks

- `observed` tags need a real binary string naming the function. The manifest's `strings` entries are generally empty or contain decompiler fragments rather than binary strings. Names such as `Com_EndParseSession` are backed by the stated error string, but names tagged observed for `Info_*`, `va`, and other helpers should be independently checked against `.rodata`; the current evidence is insufficient to certify every tag.
- Several `inferred` names are plausible engine/parser names, but the comments often cite behavior interpreted through nearby functions rather than a specific counterpart/string/caller chain. Gameplay meaning is not apparent; no gameplay-specific invented names were found in the inspected headers/comments.
- Compile command: **PASS** (`g++ -m32 -std=gnu++98 -fsyntax-only -w -fno-builtin ...`). Manifest: **86** functions; provenance comments in `.cpp`: **86**. No missing provenance entries. `FUN_` survives as the required provenance label, not in executable names; no `DAT_` or `undefined4` artefacts found in candidate sources. No `status=partial` entries, though the report cannot certify all bodies are complete. Recovered parser structs have offset assertions. `ParseSave` and `InfoField` have size assertions but no field offset assertions; those layouts are only partially documented and should be checked against disassembly before acceptance.

## Totals and grade

12 sampled: 0 faithful, 0 minor-divergence, 0 behaviour-divergence, 12 incomplete (verification not completed). Compile pass; 86/86 provenance comments. Overall grade: **F** for independent verification coverage, not a finding that all 86 implementations are wrong.

The sample selection includes the four largest functions, followed by branch/size-heavy and representative functions. Because a complete disassembly audit of 12 functions was not achieved, no parity certification is made. A follow-up should trace each sampled body and record the first concrete mismatch address/source line before assigning functional verdicts.
