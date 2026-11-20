#!/usr/bin/env python3
"""Select the applicable proposals of analysis/subsystems/*.names2.json.

Writes a plan (JSON) with the proposals to apply and the skipped ones with reasons.
Rules: `old` must equal the current name at that address (symbols.json); the new name is a
C++ identifier, not a keyword, unique among all function and global names after the pass,
not already used as a word in the recon sources/headers (comments ignored), and not a
libc/libstdc++ symbol or a system-header identifier. Disagreeing proposals for one
address: the better-evidenced one wins (confidence rank, then evidence length); a candidate
that fails a check falls back to the next one.

Usage: plan_naming_pass.py OUT_PLAN_JSON
"""

from __future__ import annotations

import collections
import glob
import json
import re
import subprocess
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]
IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
RANK = {"observed": 3, "inferred": 2, "descriptive": 1}
KEYWORDS = set("""alignas alignof and and_eq asm auto bitand bitor bool break case catch char class compl const
const_cast continue default delete do double dynamic_cast else enum explicit export extern false float for friend
goto if inline int long mutable namespace new not not_eq operator or or_eq private protected public register
reinterpret_cast return short signed sizeof static static_cast struct switch template this throw true try typedef
typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq main""".split())
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def system_names() -> set[str]:
    names: set[str] = set()
    for p in ("/lib/x86_64-linux-gnu/libc.so.6", "/lib/x86_64-linux-gnu/libm.so.6",
              "/usr/lib/x86_64-linux-gnu/libstdc++.so.6", "/lib/x86_64-linux-gnu/libdl.so.2",
              "/lib/x86_64-linux-gnu/libpthread.so.0"):
        if Path(p).exists():
            out = subprocess.run(["nm", "-D", "--defined-only", p], capture_output=True, text=True).stdout
            names.update(line.split()[-1].split("@")[0] for line in out.splitlines() if line.strip())
    roots = ["/usr/include"] + glob.glob("/usr/lib/gcc/*/*/include")
    for root in roots:
        for p in Path(root).rglob("*"):
            if p.is_file():
                try:
                    names.update(WORD.findall(p.read_text(errors="ignore")))
                except OSError:
                    pass
    return names


def source_words() -> set[str]:
    words: set[str] = set()
    files = list((GAME / "recon" / "chunks").glob("chunk_*")) + list((GAME / "recon" / "include").glob("*.h"))
    for f in files:
        if f.suffix in {".cpp", ".h"}:
            words.update(WORD.findall(COMMENTS.sub(" ", f.read_text())))
    return words


def main() -> int:
    sym = json.loads((GAME / "analysis/symbols/symbols.json").read_text())
    cur = {
        "f": {int(a, 16): v["name"] for a, v in sym["functions"].items()},
        "g": {int(a, 16): v["name"] for a, v in sym["globals"].items()},
    }
    props: dict[str, list[dict]] = {"f": [], "g": []}
    for f in sorted(glob.glob(str(GAME / "analysis/subsystems/*.names2.json"))):
        d = json.loads(Path(f).read_text())
        for key, short in (("functions", "f"), ("globals", "g")):
            for x in d[key]:
                x["src"] = Path(f).name
                props[short].append(x)

    sysn = system_names()
    words = source_words()
    counts = collections.Counter(list(cur["f"].values()) + list(cur["g"].values()))
    skipped: list[dict] = []
    groups: list[tuple[str, int, list[dict]]] = []
    for kind in ("f", "g"):
        by = collections.defaultdict(list)
        for p in props[kind]:
            by[int(p["addr"], 16)].append(p)
        for a, ps in by.items():
            if a not in cur[kind]:
                for p in ps:
                    skipped.append({"kind": kind, "addr": p["addr"], "old": p["old"], "new": p["new"], "reason": "address not in symbols.json"})
                continue
            groups.append((kind, a, ps))

    def pref(p):
        return (-RANK.get(p["confidence"], 0), -len(p.get("evidence", "")), p["src"])

    # Names that go away (old names of addresses with at least one valid proposal) may be reused (swaps).
    vacating = {cur[k][a] for k, a, ps in groups if any(p["old"] == cur[k][a] for p in ps)}
    claimed: dict[str, tuple[str, int]] = {}
    chosen: list[dict] = []
    # Process the best-evidenced groups first so that collisions are resolved in their favour.
    groups.sort(key=lambda g: pref(sorted(g[2], key=pref)[0]))
    for kind, a, ps in groups:
        reasons = []
        picked = None
        seen_new = set()
        for p in sorted(ps, key=pref):
            new = p["new"]
            if new in seen_new:
                continue
            seen_new.add(new)
            why = None
            if p["old"] != cur[kind][a]:
                why = f"old '{p['old']}' != current '{cur[kind][a]}'"
            elif not IDENT.match(new) or new in KEYWORDS:
                why = "not a valid identifier"
            elif new in sysn:
                why = "libc/libstdc++/system-header name"
            elif new in claimed:
                why = f"collides with another selected proposal at 0x{claimed[new][1]:08x}"
            elif counts.get(new, 0) and new not in vacating:
                why = "name already used by an existing function/global"
            elif new in words and new not in vacating:
                why = "identifier already used in recon sources/headers"
            elif kind == "f" and counts.get(p["old"], 0) != 1:
                why = "old function name is ambiguous in symbols.json"
            if why:
                reasons.append(f"{new}: {why}")
                continue
            picked = p
            break
        if picked is None:
            for p in ps:
                pass
            skipped.append({"kind": kind, "addr": f"0x{a:08x}", "old": cur[kind][a], "new": sorted(ps, key=pref)[0]["new"],
                            "reason": "; ".join(reasons)})
            continue
        claimed[picked["new"]] = (kind, a)
        alts = sorted({p["new"] for p in ps} - {picked["new"]})
        chosen.append({**picked, "kind": kind, "dissent": alts})
    # A new name must not equal a vacating old name that is not actually renamed away.
    renamed_old = {c["old"] for c in chosen}
    final = []
    for c in chosen:
        if counts.get(c["new"], 0) and c["new"] not in renamed_old:
            skipped.append({"kind": c["kind"], "addr": c["addr"], "old": c["old"], "new": c["new"],
                            "reason": "target name's current holder was not renamed"})
        else:
            final.append(c)
    final.sort(key=lambda c: (c["kind"], int(c["addr"], 16)))
    Path(sys.argv[1]).write_text(json.dumps({"apply": final, "skipped": skipped}, indent=1) + "\n")
    print(json.dumps({"functions": sum(c["kind"] == "f" for c in final), "globals": sum(c["kind"] == "g" for c in final),
                      "skipped": len(skipped)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
