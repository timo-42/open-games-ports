# cod2_lnxded reconstruction — link-wave lane brief

Goal: all 71 chunks link into one executable (`tools/link_test.py` exit 0) while staying
faithful to `cod2_lnxded` 1.3 (SHA-256 `05e774a3…a3a7`). Also fix differential-harness
failures in your chunks. Conventions from `recon/WORKER_BRIEF.md` still apply (faithful
translation, provenance comments, observed/inferred/unknown naming, no GPL code).

Paths below are relative to the repository root. Your lane owns a fixed list of chunks.

## You may edit ONLY

- `games/call-of-duty-2/recon/chunks/chunk_NNN.cpp`, `chunk_NNN_types.h` and
  `games/call-of-duty-2/analysis/symbols/chunk_NNN.json` for YOUR chunks;
- `.local/cod2/migrate/requests/<lane>-<n>.json` (requests to other owners);
- `.local/cod2/migrate/done/<lane>.json` (your completion marker, written last);
- your private build dir `.local/cod2/build-<lane>/`.

Never edit other chunks, `recon/include/*`, `recon/globals/*`, tools, or the harness.
Never commit, stash, or run `recon_merge.py` / `gen_globals.py`; the merge-queue lane does that.

## Shared inputs

- `recon/include/cod2_types.h` (base), `cod2_canon.h` (62 canonical structs),
  `cod2_globals.h` (typed externs for every global; the data image defines them at their
  original addresses), `cod2_prototypes.h` (generated from every chunk JSON signature;
  the merge lane regenerates it as lanes finish).
- `analysis/canonical-types.md`, `analysis/canonical-migration.json` (per chunk:
  `replace_types`, `field_renames`, local definitions to delete, duplicate externs to drop).
- `analysis/subsystems/*.md` for semantics.
- Binary `.local/cod2/lnxded-1.3/cod2_lnxded` (objdump only), pseudocode in
  `.local/cod2/decomp/functions*/`.

## Steps

1. Include order in each chunk: `cod2_types.h`, `cod2_canon.h`, `cod2_globals.h`,
   `cod2_prototypes.h`, then `chunk_NNN_types.h`. Apply your chunks' entries in
   `canonical-migration.json`: delete local copies of canonical structs, apply field
   renames, delete local `extern` declarations of globals (use `cod2_globals.h` names/types),
   keep chunk-private types in the chunk header (rename a tag if it collides with a canonical
   one). Do not define any global in a chunk (globals live in the data image); a chunk-local
   `static` constant that is not an original global is fine.
2. Signatures. For every function your chunks DEFINE, the signature in your chunk JSON is the
   contract that `cod2_prototypes.h` is generated from: rewrite it with canonical types and
   make the definition match it exactly. Verify return/argument types against the
   disassembly (callers' use of eax/st0, argument stack slots, by-value structs).
   For every function your chunks CALL in another chunk: delete your local re-declaration and
   use the shared prototype; the defining chunk is authoritative. If the shared prototype is
   verifiably wrong, keep your call compiling against it with a minimal cast marked
   `/* SIGFIX-REQUEST */` and write a request JSON
   `{addr, current, proposed, evidence}` for the owner; the merge lane routes it.
3. Linkage: libc/libm/libdl/pthread functions come from system headers (`extern "C"`);
   never redeclare them. Functions called from data tables keep their recorded names.
4. Use the link report: run
   ```sh
   export COD2_BUILD=$PWD/.local/cod2/build-<lane>
   mkdir -p "$COD2_BUILD" && cp .local/cod2/build/globals-report.json .local/cod2/build/static_aliases.json "$COD2_BUILD"/
   nice python3 games/call-of-duty-2/tools/link_test.py --no-gen --skip-before --jobs 2
   ```
   `$COD2_BUILD/link-report.json` lists every undefined symbol with `referenced_by` (chunks)
   and `defined_as` (actual definitions with address/chunk), plus duplicates. Drive to zero
   every entry where one of YOUR chunks is the referencer and the definer has finished
   migrating, and every duplicate involving your chunks (keep the definition at the address
   your chunk owns; if two chunks define the same address, the lower chunk number keeps it
   and you report it).
5. Harness: `.local/cod2/harness/results/chunk_NNN.json` lists `fail` verdicts with the first
   mismatch. Fix those translations from the disassembly. If the harness still builds with
   the migrated chunk, re-run `python3 games/call-of-duty-2/recon/harness/difftest.py --chunk NNN --func 0x...`;
   if it does not (it predates the canonical headers), say so — do not edit the harness.
6. Every chunk you own must still compile:
   `g++ -m32 -std=gnu++98 -fsyntax-only -w -fno-builtin -I games/call-of-duty-2/recon/include -I games/call-of-duty-2/recon/chunks <file>`.
7. Write `.local/cod2/migrate/done/<lane>.json` LAST:
   `{lane, chunks, compile_ok, undefined_refs_from_my_chunks, duplicates_left, harness_fixed, harness_still_failing, requests_filed, notes}`.
