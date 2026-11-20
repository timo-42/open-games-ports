# cod2_lnxded reconstruction: gameplay-coverage lane brief

Goal: extend the gameplay oracle to a feature that is not covered yet, prove it deterministic on the
original `cod2_lnxded` 1.3 (SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`), then drive
the reconstruction to **0 mismatches on deterministic items** for it. Paths are relative to the repository root;
`G = games/call-of-duty-2`.

Read first:
- `G/analysis/gameplay-parity.md`: method, determinism controls, protocol facts and earlier fixes.
- `G/tools/oracle/gameplay.py`: the `Duel` scenario and the `load_plugins()` docstring.
- `G/recon/WORKER_BRIEF.md`: translation conventions.

## You may edit ONLY

- `G/tools/oracle/scenarios/<lane>.py`: your plugin. Cases are named `<lane>_<what>_<map>`.
- `G/analysis/gameplay/<lane>.md`: your findings.
- `G/recon/chunks/chunk_NNN.cpp` / `_types.h`: minimal, evidence-backed fixes to reconstruction defects. Other lanes
  edit chunks concurrently:
  - Re-read the region right before each Edit.
  - Change only the lines you must.
  - Never revert or reformat code you did not write.
  - Add a comment `/* gameplay-fix(<lane>): <one line, with original address> */`.
- `.local/cod2/lanes/<lane>/` (scratch, private build) and `.local/cod2/lanes/<lane>/done.json` (written LAST).

Never edit `gameplay.py`, `fakeclient.py`, `cod2proto.py`, other plugins, `recon/include/*`, `recon/globals/*`, or
the tools. Never git commit, stash, checkout or reset. If you need a harness feature:
- implement it inside your plugin, either as class methods or by subclassing `Duel`;
- do not monkeypatch at module level, because every plugin is imported for every case;
- describe it in `done.json` under `harness_requests`.

## Plugin

```python
import gameplay as G           # tools/oracle/gameplay.py; load_plugins aliases it
class MyScen(G.Duel): ...      # actions(p, t) -> [reliable cmds]; usercmd(p, t) sets p.cmd; actions2/usercmd2 after map change
CASES = {"<lane>_x_mp_toujane": {"args": ["+set", "g_gametype", "sd", ..., "+devmap", "mp_toujane"],
          "scenario": MyScen, "players": [("A","allies","thompson_mp"), ("B","axis","mp40_mp"), ...],
          "t0": 20000, "length": 40000, "rotate": 0, "bot_rcon": None, "mod": None}}
```

- **Packet loss:** define `drop_c2s(self, p, raw)` and/or `drop_s2c(self, p, raw)` and return True to drop. `raw[:4]` is the little-endian sequence number, and bit 31 marks a fragment. Drop decisions must depend only on sequence numbers and server time, never on the wall clock.
- **GSC overlays:** a `mod` is `fn(home, data_dir) -> str`. It writes loose files under `<home>/mods/<x>` from game data and returns a line for `harness.log`. The overlay must never be committed. Pass `+set fs_game mods/<x>` in the args.
- **Timing:** every decision must be a function of the deciding client's own latest snapshot and its server time.
- **Team and weapon menus:** in other gametypes the stock menus and their responses may differ. Read the `.gsc` files and menu configstrings from the `.iwd` (ZIP) files in the user-supplied `COD2_DATA_MAIN` directory (read-only).

## Run and compare

```sh
O=G/tools/oracle; L=.local/cod2
python3 $O/gameplay.py run <case> 1 --server original      # repeat for runs 2 and 3
python3 $O/gameplay.py compare $L/oracle/captures/gameplay_<case>   # determinism across originals: aim for >95% identical
export COD2_BUILD=$PWD/.local/cod2/lanes/<lane>/build; mkdir -p $COD2_BUILD
cp $L/build/globals-report.json $L/build/static_aliases.json $COD2_BUILD/
nice python3 G/tools/link_test.py --no-gen --skip-before --jobs 4   # builds $COD2_BUILD/cod2_lnxded_recon
RECON_BIN=$COD2_BUILD/cod2_lnxded_recon python3 $O/gameplay.py run <case> 1 --server recon
python3 $O/gameplay.py compare $L/oracle/captures/gameplay_<case> $L/recon-run/captures/gameplay_<case>
```

- **Concurrency:** runs execute inside their own `bwrap --unshare-net` namespace, so lanes can run concurrently on the same port.
- **Variable items:** if an item varies across original runs, find why. Change the scenario to make it deterministic: decide by server time, pin the seed (default), or avoid wall-clock effects. Do not filter items away. Masks are allowed only for proven timing families, documented in your notes.
- **Debugging a mismatch:**
  - Find the first divergent server time and field.
  - Use `G/tools/oracle/statediff.py` / `calltrace.py` (gdb, both servers, pinned seed) to find the first diverging function.
  - Read the disassembly (`objdump -d --start-address=...` on `.local/cod2/lnxded-1.3/cod2_lnxded`) and fix the translation.
  - Re-run `G/recon/harness/difftest.py --chunk NNN --func 0x...` if the function is testable.
  - Earlier defect classes to look for first: decompiler-split arrays/matrices, int-vs-float reinterpretation, inverted branches, x87 extended-precision differences (fstp/fld spills, return in st0), and swapped struct fields.
- **Final acceptance:**
  - Each case: 3 original runs and at least 2 recon runs, with `mismatches vs other on deterministic items: 0`.
  - After your fixes, also re-run `duel_dm_toujane` once against the recon (`--tag=-<lane>`). It must stay at 0 against `$L/oracle/captures/gameplay_duel_dm_toujane`.

## Notes and done marker

- **Notes:** `G/analysis/gameplay/<lane>.md`, matching the style of `gameplay-parity.md`.
  - Tag findings [O] observed, [I] inferred, [U] unknown.
  - Cover: cases and what they exercise, determinism controls, item counts, each fix with its original address and evidence, and what is still uncovered and why.
- **`done.json`:** `{lane, cases: {case: {original_runs, recon_runs, identical_items, mismatches}}, fixes: [{chunk, func_addr, summary}], harness_requests, uncovered, notes}`.
