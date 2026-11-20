#!/usr/bin/env python3
"""Record the function renames of a naming-pass plan in the chunk records.

For each planned function rename sets name_before_rename/name/signature/confidence/evidence of the
matching entry in analysis/symbols/chunk_NNN.json. Then run apply_renames.py with the previous
symbols.json and recon_merge.py. Proposal confidence "descriptive" (role/position labels) is
recorded as "unknown" because the record vocabulary has only observed/inferred/unknown.

Usage: apply_function_proposals.py PLAN_JSON
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]
CONF = {"observed": "observed", "inferred": "inferred", "descriptive": "unknown"}


def main() -> int:
    ren = {int(c["addr"], 16): c for c in json.loads(Path(sys.argv[1]).read_text())["apply"] if c["kind"] == "f"}
    done = set()
    for path in sorted((GAME / "analysis" / "symbols").glob("chunk_[0-9][0-9][0-9].json")):
        data = json.loads(path.read_text())
        touched = False
        for f in data.get("functions", []):
            a = int(f["addr"], 16)
            c = ren.get(a)
            if not c or (f["name"] != c["old"] and not c["old"].startswith(f["name"] + "_")) or a in done:
                continue
            chunk_name = f["name"]  # equals c["old"], or c["old"] minus a "_<addr>" disambiguation suffix
            f["name_before_rename"] = c["old"]
            f["name"] = c["new"]
            if f.get("signature"):
                f["signature"] = re.sub(r"\b" + re.escape(chunk_name) + r"\b", lambda m: c["new"], f["signature"])
            f["confidence"] = CONF[c["confidence"]]
            f["evidence"] = ("naming pass 2: " if c["confidence"] != "descriptive" else "naming pass 2 (descriptive name): ") + c["evidence"]
            done.add(a)
            touched = True
        if touched:
            path.write_text(json.dumps(data, indent=1) + "\n")
    missing = sorted(set(ren) - done)
    print(json.dumps({"recorded": len(done), "not_found_in_chunk_records": [f"0x{a:08x}" for a in missing]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
