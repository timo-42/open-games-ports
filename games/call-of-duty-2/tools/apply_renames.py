#!/usr/bin/env python3
"""Apply an explicit batch of function renames to every reconstructed chunk.

Each finished chunk record (analysis/symbols/chunk_NNN.json) may carry
"name_before_rename" next to "name". For every such pair whose old name was
unique in the given previous symbols.json, rewrite whole-word occurrences of the
old name to the new one in all recon/chunks/*.cpp|*.h files in one simultaneous
pass (so swaps A<->B are safe). Then drop the marker. Ambiguous old names are
reported and left for manual handling. Run tools/recon_merge.py afterwards.

Usage: apply_renames.py PREVIOUS_SYMBOLS_JSON
"""

from __future__ import annotations

import collections
import json
import re
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]


def main() -> int:
    previous = json.loads(Path(sys.argv[1]).read_text())["functions"]
    counts = collections.Counter(v["name"] for v in previous.values())
    renames: dict[str, str] = {}
    ambiguous = []
    records = sorted((GAME / "analysis" / "symbols").glob("chunk_[0-9][0-9][0-9].json"))
    for path in records:
        for f in json.loads(path.read_text()).get("functions", []):
            old = f.get("name_before_rename")
            if not old or old == f["name"]:
                continue
            addr = f"0x{int(f['addr'], 16):08x}"
            if counts.get(old, 0) == 1 and previous.get(addr, {}).get("name") == old:
                renames[old] = f["name"]
            else:
                ambiguous.append((f["addr"], old, f["name"]))
    changed = 0
    if renames:
        pattern = re.compile(r"\b(" + "|".join(map(re.escape, sorted(renames, key=len, reverse=True))) + r")\b")
        for src in sorted((GAME / "recon" / "chunks").glob("chunk_*")):
            if src.suffix not in {".cpp", ".h"}:
                continue
            text = src.read_text()
            new = pattern.sub(lambda m: renames[m.group(1)], text)
            if new != text:
                src.write_text(new)
                changed += 1
    for path in records:
        data = json.loads(path.read_text())
        touched = False
        for f in data.get("functions", []):
            if f.get("name_before_rename") in renames:
                del f["name_before_rename"]
                touched = True
        if touched:
            path.write_text(json.dumps(data, indent=1) + "\n")
    print(json.dumps({"renamed": len(renames), "files_changed": changed, "ambiguous": ambiguous}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
