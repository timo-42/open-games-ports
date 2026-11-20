#!/usr/bin/env python3
"""Apply the global renames of a naming-pass plan (see plan_naming_pass.py).

Renames whole-word, in one simultaneous pass (swaps are safe), in recon/chunks/*.cpp|*.h,
recon/include/cod2_globals.h and the "name" of the matching "globals" entries of every
analysis/symbols/chunk_NNN.json. Entries are matched by address and old name.

Types: the declaration type in cod2_globals.h is only replaced when the proposal's type_hint is a
supported scalar type of the same size as the declared one; every other hint is kept as a
comment-free no-op and counted in the summary (a type change is not a mechanical rename).
Currently no scalar rewrite is enabled (SUPPORTED_RETYPE empty) because signedness/float changes alter
generated code; the machinery is kept so a reviewed list can be enabled.

Usage: apply_global_renames.py PLAN_JSON [--dry-run]
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]
SUPPORTED_RETYPE: dict[tuple[str, str], str] = {}  # (declared type, hint) -> new declared type


def main() -> int:
    plan = json.loads(Path(sys.argv[1]).read_text())["apply"]
    dry = "--dry-run" in sys.argv
    ren = {c["old"]: c for c in plan if c["kind"] == "g"}
    if not ren:
        return 0
    pat = re.compile(r"\b(" + "|".join(map(re.escape, sorted(ren, key=len, reverse=True))) + r")\b")
    files = [p for p in sorted((GAME / "recon" / "chunks").glob("chunk_*")) if p.suffix in {".cpp", ".h"}]
    files.append(GAME / "recon" / "include" / "cod2_globals.h")
    changed = 0
    retyped = 0
    for src in files:
        text = src.read_text()
        new = pat.sub(lambda m: ren[m.group(1)]["new"], text)
        if src.name == "cod2_globals.h":
            lines = []
            for line in new.split("\n"):
                m = re.match(r"extern (.*?)\b(\w+)(\[[^\]]*\])*;", line)
                if m:
                    entry = next((c for c in ren.values() if c["new"] == m.group(2)), None)
                    key = (m.group(1).strip(), (entry or {}).get("type_hint", ""))
                    if entry and key in SUPPORTED_RETYPE:
                        line = line.replace(m.group(1), SUPPORTED_RETYPE[key] + " ", 1)
                        retyped += 1
                lines.append(line)
            new = "\n".join(lines)
        if new != text:
            changed += 1
            if not dry:
                src.write_text(new)
    json_changed = 0
    entries = 0
    for path in sorted((GAME / "analysis" / "symbols").glob("chunk_[0-9][0-9][0-9].json")):
        data = json.loads(path.read_text())
        touched = False
        for key in ("globals", "documentation_only_globals"):
            for g in data.get(key, []):
                c = ren.get(g.get("name"))
                if c and int(g["addr"], 16) == int(c["addr"], 16):
                    g["name"] = c["new"]
                    entries += 1
                    touched = True
        if touched:
            json_changed += 1
            if not dry:
                path.write_text(json.dumps(data, indent=1) + "\n")
    print(json.dumps({"renames": len(ren), "source_files_changed": changed, "chunk_json_changed": json_changed,
                      "json_entries_renamed": entries, "retyped": retyped}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
