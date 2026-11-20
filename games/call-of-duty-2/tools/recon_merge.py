#!/usr/bin/env python3
"""Merge-queue step for the cod2_lnxded C++ reconstruction.

Reads every finished chunk record (analysis/symbols/chunk_NNN.json, written last
by a worker as its completion marker), then:

1. builds analysis/symbols/symbols.json (address -> name/signature/evidence),
   resolving duplicate names deterministically and listing them in conflicts.json;
2. renames placeholders (fn_08xxxxxx / g_08xxxxxx) and superseded names in the
   finished chunk sources to the current names;
3. regenerates recon/include/cod2_prototypes.h from the recorded signatures,
   dropping (and reporting) signatures that do not compile;
4. syntax-checks every finished chunk with g++ -m32 and writes
   analysis/symbols/merge-report.json.

Standard library only. Run from any directory.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]
SYMBOLS = GAME / "analysis" / "symbols"
RECON = GAME / "recon"
CHUNKS = RECON / "chunks"
INCLUDE = RECON / "include"
CONF_RANK = {"observed": 3, "inferred": 2, "unknown": 1}
IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
QUALIFIED = re.compile(r"^[A-Za-z_]\w*::~?[A-Za-z_]\w*$")  # Class::method members, declared in class definitions
CXX = ["g++", "-m32", "-std=gnu++98", "-fsyntax-only", "-w", "-fno-builtin"]


def norm_addr(value: str) -> str:
    return f"0x{int(str(value), 16):08x}"


def load_chunks() -> list[dict]:
    records = []
    for path in sorted(SYMBOLS.glob("chunk_[0-9][0-9][0-9].json")):
        try:
            data = json.loads(path.read_text())
        except json.JSONDecodeError as exc:
            print(f"warning: {path.name}: {exc}", file=sys.stderr)
            continue
        data["_path"] = path
        data["_chunk"] = int(path.stem.split("_")[1])
        records.append(data)
    return records


def resolve(entries: list[dict], kind: str, conflicts: list[dict]) -> dict[str, dict]:
    """Return addr -> entry with unique, valid identifiers."""
    by_addr: dict[str, dict] = {}
    for entry in entries:
        addr = norm_addr(entry["addr"])
        entry["addr"] = addr
        prev = by_addr.get(addr)
        if prev is None or CONF_RANK.get(entry.get("confidence"), 0) > CONF_RANK.get(prev.get("confidence"), 0):
            by_addr[addr] = entry
    prefix = "fn_" if kind == "function" else "g_"
    by_name: dict[str, list[str]] = {}
    for addr, entry in by_addr.items():
        name = str(entry.get("name") or "")
        if not (IDENT.match(name) or (kind == "function" and QUALIFIED.match(name))):
            conflicts.append({"kind": kind, "addr": addr, "issue": "invalid identifier", "name": name})
            name = f"{prefix}{addr[2:]}"
            entry["name"] = name
        by_name.setdefault(name, []).append(addr)
    for name, addrs in by_name.items():
        if len(addrs) < 2:
            continue
        addrs.sort(key=lambda a: (-CONF_RANK.get(by_addr[a].get("confidence"), 0), int(a, 16)))
        for addr in addrs[1:]:
            new = f"{name}_{addr[2:]}"
            conflicts.append({"kind": kind, "name": name, "kept": addrs[0], "renamed": addr, "to": new})
            by_addr[addr]["name"] = new
            sig = str(by_addr[addr].get("signature") or "")
            by_addr[addr]["signature"] = re.sub(rf"\b{re.escape(name)}\b", new, sig, count=1)
    return by_addr


def rename_sources(done: set[int], renames: dict[str, str]) -> int:
    if not renames:
        return 0
    pattern = re.compile(r"\b(" + "|".join(map(re.escape, sorted(renames, key=len, reverse=True))) + r")\b")
    changed = 0
    for chunk in sorted(done):
        for path in CHUNKS.glob(f"chunk_{chunk:03d}*"):
            if path.suffix not in {".cpp", ".h"}:
                continue
            text = path.read_text()
            new = pattern.sub(lambda m: renames[m.group(1)], text)
            if new != text:
                path.write_text(new)
                changed += 1
    return changed


PROV = re.compile(r"^(?://|/\*) (0x[0-9a-fA-F]{8})\s+size=", re.M)
DEFNAME = re.compile(r"([A-Za-z_]\w*)\s*\(")


def sync_definitions(done: set[int], functions: dict[str, dict]) -> int:
    """Rename each chunk's own definitions (found via provenance comments) to the
    resolved symbol name, within that chunk's .cpp/.h only."""
    changed = 0
    for chunk in sorted(done):
        cpp = CHUNKS / f"chunk_{chunk:03d}.cpp"
        if not cpp.exists():
            continue
        text = cpp.read_text()
        mapping: dict[str, str] = {}
        for m in PROV.finditer(text):
            entry = functions.get(norm_addr(m.group(1)))
            if not entry or not IDENT.match(entry["name"]) or entry["name"].startswith("fn_"):
                continue
            rest = text[m.end():]
            body = []
            for line in rest.split("\n")[1:]:
                st = line.strip()
                if not st or st.startswith(("//", "/*", "*")):
                    continue
                if st.startswith("#"):
                    body = ["#"]
                    break
                body.append(line)
                if "{" in line:
                    break
                if ";" in line:
                    body = []  # prototype: keep looking for the definition
            head = " ".join(body)
            if "{" not in head:
                continue
            dm = DEFNAME.search(head.split("{")[0])
            old, new = dm.group(1) if dm else "", entry["name"]
            base = lambda n: re.sub(r"_[0-9a-f]{8}$", "", n)
            # only repair duplicate-name suffix churn; never touch helpers or macros
            if dm and not head.lstrip().startswith("#") and old != new and base(old) == base(new):
                mapping[dm.group(1)] = entry["name"]
        if not mapping:
            continue
        pattern = re.compile(r"\b(" + "|".join(map(re.escape, sorted(mapping, key=len, reverse=True))) + r")\b")
        for path in CHUNKS.glob(f"chunk_{chunk:03d}*"):
            if path.suffix not in {".cpp", ".h"}:
                continue
            t = path.read_text()
            n = pattern.sub(lambda mm: mapping[mm.group(1)], t)
            if n != t:
                path.write_text(n)
                changed += 1
    return changed


def compile_ok(path: Path) -> tuple[bool, str]:
    proc = subprocess.run(CXX + ["-I", str(INCLUDE), "-I", str(CHUNKS), str(path)], capture_output=True, text=True)
    return proc.returncode == 0, proc.stderr


def write_prototypes(functions: dict[str, dict]) -> list[dict]:
    header = INCLUDE / "cod2_prototypes.h"
    lines = {}
    tags: set[str] = set()
    for addr, entry in sorted(functions.items()):
        sig = str(entry.get("signature") or "").strip().rstrip(";")
        if not sig or entry["name"] not in sig or QUALIFIED.match(entry["name"]):
            continue
        if int(addr, 16) >= 0x08a70000:  # libc/libstdc++ import stubs: use system headers (request L00-3)
            continue
        tags.update(re.findall(r"\b(struct|union|enum)\s+([A-Za-z_]\w*)", sig))
        lines[addr] = f"{sig}; /* {addr} */"
    dropped: list[dict] = []
    for _ in range(50):
        fwd = [f"{kind} {name};" for kind, name in sorted(tags) if kind != "enum"]
        body = "\n".join(lines[a] for a in sorted(lines))
        text = (
            "/* Generated by tools/recon_merge.py from analysis/symbols/chunk_*.json. Do not edit. */\n"
            "#ifndef COD2_PROTOTYPES_H\n#define COD2_PROTOTYPES_H\n#include \"cod2_types.h\"\n"
            + "\n".join(fwd) + "\n" + body + "\n#endif\n"
        )
        header.write_text(text)
        ok, err = compile_ok(header)
        if ok:
            return dropped
        bad = set()
        for match in re.finditer(r"cod2_prototypes\.h:(\d+):\d+: error: (.*)", err):
            src = text.splitlines()[int(match.group(1)) - 1]
            m = re.search(r"/\* (0x[0-9a-f]+) \*/", src)
            if m:
                bad.add((m.group(1), match.group(2)))
        if not bad:
            dropped.append({"addr": None, "error": err[:2000]})
            header.write_text(text.replace(body, ""))
            return dropped
        for addr, msg in bad:
            if lines.pop(addr, None) is not None:
                dropped.append({"addr": addr, "error": msg})
    return dropped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-compile", action="store_true")
    parser.add_argument("--no-rewrite", action="store_true", help="do not modify chunk sources (safe while lanes edit them)")
    args = parser.parse_args()

    records = load_chunks()
    done = {r["_chunk"] for r in records}
    previous = {}
    sym_path = SYMBOLS / "symbols.json"
    if sym_path.exists():
        previous = json.loads(sym_path.read_text())

    conflicts: list[dict] = []
    fn_entries, gl_entries = [], []
    for r in records:
        for f in r.get("functions", []):
            fn_entries.append(dict(f, chunk=r["_chunk"]))
        for g in r.get("globals", []):
            gl_entries.append(dict(g, chunk=r["_chunk"]))
    functions = resolve(fn_entries, "function", conflicts)
    globals_ = resolve(gl_entries, "global", conflicts)

    renames: dict[str, str] = {}
    for kind, table, prefix in (("functions", functions, "fn_"), ("globals", globals_, "g_")):
        old = previous.get(kind, {})
        for addr, entry in table.items():
            placeholder = f"{prefix}{addr[2:]}"
            if entry["name"] != placeholder:
                renames[placeholder] = entry["name"]
            prior = old.get(addr, {}).get("name")
            # superseded names are NOT renamed globally (duplicate names make that ambiguous);
            # definitions are synchronised per chunk by sync_definitions() instead.
    changed = 0
    if not args.no_rewrite:
        changed = rename_sources(done, renames)
        changed += sync_definitions(done, functions)

    keep = ("name", "signature", "subsystem", "confidence", "evidence", "chunk", "type")
    out = {
        "target_sha256": "05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7",
        "chunks_done": sorted(done),
        "functions": {a: {k: e[k] for k in keep if k in e} for a, e in sorted(functions.items())},
        "globals": {a: {k: e[k] for k in keep if k in e} for a, e in sorted(globals_.items())},
    }
    sym_path.write_text(json.dumps(out, indent=1, sort_keys=False) + "\n")
    (SYMBOLS / "conflicts.json").write_text(json.dumps(conflicts, indent=1) + "\n")

    report = {"chunks_done": len(done), "functions": len(functions), "globals": len(globals_),
              "conflicts": len(conflicts), "files_renamed": changed}
    if not args.no_compile:
        report["prototypes_dropped"] = write_prototypes(functions)
        failures = {}
        for chunk in sorted(done):
            src = CHUNKS / f"chunk_{chunk:03d}.cpp"
            if not src.exists():
                failures[chunk] = "missing source"
                continue
            ok, err = compile_ok(src)
            if not ok:
                failures[chunk] = err[-3000:]
        report["compile_failures"] = failures
        report["compile_pass"] = len(done) - len(failures)
    (SYMBOLS / "merge-report.json").write_text(json.dumps(report, indent=1) + "\n")
    summary = {k: (len(v) if isinstance(v, (list, dict)) else v) for k, v in report.items()}
    print(json.dumps(summary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
