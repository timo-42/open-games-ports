# Link wave deletion audit

## Result

**Confirmed functions with losses still present in HEAD: 0.** In particular, the large apparent reductions in chunks 059–061 at `3c7ef94b4` are absent from HEAD: their current bodies are materially larger, consistent with the restoration by `70e81e6bf`. No other candidate was confirmed as a semantic deletion in HEAD from this audit. This is a conservative confirmation count, not a proof that every function is equivalent: the token comparison is a triage heuristic and the original disassembly was not manually translated for all 268 candidates.

## Scope and method

Compared functions identified by `// 0x08...` provenance comments in matching `chunk_NNN.cpp` files between `4db502b80` and `3c7ef94b4`, then checked current HEAD. There are 71 chunk source files in the wave and 268 functions with a nonempty heuristic deletion signal, across 51 chunks. The reproducible scanner is [audit_deletions.py](../../tools/audit_deletions.py); full candidate records and top-candidate disassemblies are under `.local/cod2/audit/` (untracked).

Candidate list entries are not equivalent to confirmed losses. The normalizer removes comments, literals, and common C-style casts and compares calls, assignment operators, and control keywords. It does not fully parse C++ types, templates, macros, overloaded operators, or AST structure. As a result, helper/intrinsic substitutions, initializer/declaration rewrites, and expression restructuring can appear as deleted calls or assignments.

## Per chunk (lane) scanner counts

Counts below are functions with at least one removed/replaced structural token, not confirmed deletion counts. Unlisted chunks had zero such signals.

| Chunk | Candidates | Chunk | Candidates | Chunk | Candidates |
|---|---:|---|---:|---|---:|
| 004 | 6 | 005 | 3 | 006 | 5 |
| 007 | 8 | 008 | 1 | 009 | 3 |
| 010 | 2 | 011 | 1 | 012 | 1 |
| 015 | 11 | 016 | 1 | 017 | 6 |
| 018 | 2 | 020 | 1 | 022 | 3 |
| 024 | 3 | 025 | 7 | 026 | 7 |
| 027 | 1 | 028 | 5 | 029 | 7 |
| 030 | 1 | 031 | 1 | 034 | 1 |
| 035 | 1 | 038 | 6 | 039 | 2 |
| 040 | 16 | 042 | 2 | 043 | 1 |
| 045 | 6 | 046 | 10 | 047 | 2 |
| 048 | 8 | 049 | 4 | 050 | 1 |
| 051 | 1 | 052 | 2 | 053 | 1 |
| 054 | 2 | 055 | 6 | 056 | 2 |
| 058 | 5 | 059 | 35 | 060 | 40 |
| 061 | 16 | 100 | 1 | 104 | 8 |
| 105 | 1 | 107 | 1 | | |

## Highest-priority review findings

| Address | Name | Chunk | Wave signal | HEAD check / evidence |
|---|---|---:|---|---|
| `0x0813638c` | `Vec3_ToAngles` | 059 | 23 removed call tokens; wave body 1,153 chars | HEAD body is 1,647 chars and restores the missing effect construction, randomization, elapsed-time, vector, and object-setting calls. Not a current loss. |
| `0x08135d72` | `Fx_SpawnA` | 059 | 21 removed call tokens; wave body 831 chars | HEAD body is 1,425 chars and restores the spawner construction/origin/trace/pick and object setup calls. Not a current loss. |
| `0x08139c8e` | `LocLine_Parse` | 060 | 10 calls and one assignment signal | HEAD body is 2,732 chars versus 2,293 at wave; parser/value/table and string handling are present in the expanded body. No confirmed current loss. |
| `0x081361e6` | `Fx_SpawnC` | 059 | 10 removed call tokens | HEAD body is 895 chars versus 535 at wave and includes construction, sequence, span, vector, and object operations. |
| `0x0813ca88` | `it` | 061 | 7 removed call tokens | HEAD body expands from 1,497 to 1,561 chars; signal consists of PunkBuster file/process helpers and was not established as a deleted behavior. |
| `0x080de830` | `tunables` | 040 | 68 apparent calls | False positive pattern: `cod2_f32_bits` conversion helpers emitted by the rewrite of float constants. These are representation/conversion changes, not 68 lost behavioral calls. |
| `0x08056490` | `Sphere` | 004 | 14 apparent calls, 15 assignments, 2 returns | Large function/expression rewrite; sequence matching reports shifted tokens. Not promoted to a confirmed loss without equivalent source/binary trace evidence. |

For the top 40 scores the script saved `objdump -d -M intel` windows starting at each provenance address (512 bytes). These raw windows establish the original machine instructions available for follow-up, but the scanner does not infer source-level behavior from them. The high-confidence chunk 059–061 recovery check is also directly evidenced by comparison of the wave and HEAD bodies above; original-binary validation of individual branches was not needed to label these as *not present in HEAD*.

## False-positive patterns filtered or observed

- Comments and string/character literals are removed before token comparison; common C scalar and pointer casts are stripped.
- Identifier spelling changes are intentionally ignored at the token level, so local variable and field renames do not by themselves count as losses.
- Type/helper expansion (notably `cod2_f32_bits`), initialization lowering, expression regrouping, and changes in helper-call boundaries still trigger signals. The scanner cannot reliably filter these without parsing types and expressions.
- Repeated template/container operations can shift the alignment and produce spurious deleted calls; candidates in chunks 060–061 include such standard-library map/tree wrappers.
- A replaced/shortened body may be a legitimate rewrite; a nonempty score is only a review lead. `(void)` casts, `#if 0`, newly added early returns, and commented-out code require contextual inspection and are not all independently classified by the present heuristic.

## Limitations

The scanner matches on address and uses function brace extents, but source-declared names may differ from binary symbols. The disassembly windows are fixed-size rather than exact function ranges and can include adjacent code. This report makes no claim of complete behavioral parity for all migrated functions; it reports zero specifically **confirmed deletion losses still present in HEAD** under this audit's evidence threshold.

## Follow-up verification (12 Sonnet verifier lanes, 2026-11-18)

All 268 candidates were checked against 4db502b80, HEAD and the original disassembly: 258 false positives; 10 confirmed losses still present in HEAD (9 in chunk 059 Fx_Spawn*/Fx_QueueEventB, 1 in chunk 061 PB_LoadServerDll). The initial '0 confirmed' result reflected unanalysed candidates, not their absence.

## Restored

Restored from 4db502b80 text, adapted to current names and types, and checked call by call against `objdump -d -M intel` of `cod2_lnxded` 1.3. Evidence level: observed (disassembly); no behavioural parity claim beyond that.

chunk_059.cpp:
- Fx_SpawnA 0x08135d72: `Line_ConstructBase` (0x8135d94), `FxSpawner_ComputeOrigin` (0x8135e47), valid-ref `Orient_PointWorldToLocal` + `FxObj_SetVec24c` (0x8135f0f/0x8135f21).
- Fx_SpawnB 0x0813604c: `Tail_ConstructBase` (0x813606c).
- Fx_SpawnC 0x081361e6: `Cylinder_ConstructBase` (0x8136208), valid-ref `Orient_VecWorldToLocal` + `FxObj_SetVec48` (0x813634c/0x813635e).
- Fx_SpawnD 0x0813638c: `Emitter_ConstructBase` (0x81363b9), valid-ref `FxObj_SetVec264OrClear(orient)` (0x8136651), and the store tail 0x81366a3..0x8136755 (SetOrigin, SetVec24cOrClear, SetVec258OrClear, SetVec278OrClear, SetVec284OrClear, SetFieldB4, SetField290/294/298, SetField270(g_08188ac0->time), FxObj29c_RandomizeValue).
- Fx_SpawnE 0x08136936: `Light_ConstructBase` (0x8136955).
- Fx_SpawnF 0x08136a8a: `OrientedParticle_ConstructBase` (0x8136aac), Randomize / GetSpan+SetupSequence / ApplyElapsed / Vec3_Copy (0x8136b5b..0x8136bc0), valid-ref `Orient_VecWorldToLocal` + `FxObj_SetVec24cB` (0x8136c04/0x8136c16).
- Fx_SpawnG 0x08136c44: `Flash_Construct` (0x8136c63).
- Fx_SpawnH 0x08136d84: `Cloud_Construct` (0x8136da4).
- Fx_QueueEventB 0x081367b0: valid-ref `FxPickList_Pick` + `FxScheduler_PlayEffect(..., 0, &target->+0x3c)` (0x8136846/0x813686f). The bolt argument is `lea ebx,[eax+0x3c]`, i.e. the address of the field.

chunk_061.cpp:
- PB_LoadServerDll 0x0813ca88: `fclose(probe)` (0x813cafe), `PB_MakeExecutable(PB_BuildPath("pbsvold.so"))` (0x813cb23) and `PB_MakeExecutable(PB_BuildPath("pbsv.so"))` (0x813cbb5), each before the following `remove`.

Scan: a per-function call-count comparison of chunks 059, 060 and 061 against 4db502b80 found no further functions with fewer calls than the original text.

Verification:
- `link_test.py --no-gen --skip-before` (private COD2_BUILD): link_ok, 0 compile failures, 0 undefined, exit 0.
- `difftest.py --chunk 059` for each restored Fx_Spawn*/Fx_QueueEventB: verdict "unsupported" on all 50 cases, because the original SIGSEGVs without engine state (pools, g_08a53804, scheduler). Not testable by the harness; the disassembly comparison above is the only evidence.
- `difftest.py --chunk 061 --func 0x0813ca88`: "pass", but every case was discarded as sandbox_denied:dlopen, so it does not exercise the restored calls.
