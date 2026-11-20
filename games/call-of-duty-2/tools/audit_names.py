#!/usr/bin/env python3
"""Mechanically audit reconstructed function names against the original ELF."""
import json
import re
import struct
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GAME = ROOT / "games/call-of-duty-2"
BIN = ROOT / ".local/cod2/lnxded-1.3/cod2_lnxded"
SYMBOLS = GAME / "analysis/symbols"
DECOMP = ROOT / ".local/cod2/decomp/functions"
OUT_JSON = ROOT / ".local/cod2/audit/naming-audit.json"
OUT_MD = GAME / "analysis/verification/naming-audit.md"


DYNSYMS = set()


def elf_strings(path):
    data = path.read_bytes()
    if data[:4] != b"\x7fELF":
        raise ValueError("not an ELF file")
    cls, enc = data[4], data[5]
    endian = "<" if enc == 1 else ">"
    if cls == 1:
        shoff = struct.unpack_from(endian + "I", data, 32)[0]
        shentsize, shnum, shstrndx = struct.unpack_from(endian + "HHH", data, 46)
        fmt = endian + "IIIIIIIIII"
    elif cls == 2:
        shoff = struct.unpack_from(endian + "Q", data, 40)[0]
        shentsize, shnum, shstrndx = struct.unpack_from(endian + "HHH", data, 58)
        fmt = endian + "IIQQQQIIQQ"
    else:
        raise ValueError("unsupported ELF class")
    headers = [struct.unpack_from(fmt, data, shoff + i * shentsize) for i in range(shnum)]
    names_hdr = headers[shstrndx]
    name_data = data[names_hdr[4]:names_hdr[4] + names_hdr[5]]
    def cstr(buf, pos):
        end = buf.find(b"\0", pos)
        return buf[pos:end if end >= 0 else len(buf)].decode("ascii", "replace")
    found = []
    for h in headers:
        name = cstr(name_data, h[0])
        if name == ".dynstr":
            dyn = data[h[4]:h[4] + h[5]]
            DYNSYMS.update(x.decode("ascii", "replace") for x in dyn.split(b"\0") if x)
            syms = subprocess.run(["readelf", "-W", "--dyn-syms", str(path)], text=True, capture_output=True, check=False).stdout
            names = {ln.split()[7].split("@")[0] for ln in syms.splitlines() if len(ln.split()) >= 8 and ln.split()[0].rstrip(":").isdigit()}
            DYNSYMS.update(names)
            mangled = [x for x in names if x.startswith("_Z")]
            try:
                out = subprocess.run(["c++filt"], input="\n".join(mangled), text=True, capture_output=True, check=False).stdout
                for dem in out.splitlines():
                    DYNSYMS.add(dem)
                    DYNSYMS.add(re.sub(r"\(.*$", "", dem))
            except OSError:
                pass
            continue
        if name not in (".rodata", ".data"):
            continue
        off, size, addr = h[4], h[5], h[3]
        section = data[off:off + size]
        for m in re.finditer(rb"[\x09\x0a\x0d\x20-\x7e]+\x00", section):
            raw = m.group()[:-1]
            if raw:
                found.append({"address": addr + m.start(), "text": raw.decode("ascii"), "section": name})
    return found


def normalize_name(name):
    # Strip an optional trailing parenthesized descriptor, retaining the symbol itself.
    return re.sub(r"\s*\([^()]*\)\s*$", "", name).strip()


def parse_range(pseudo, addr):
    m = re.search(r"^// range:\s*(0x[0-9a-fA-F]+)-(0x[0-9a-fA-F]+)", pseudo, re.M)
    if m:
        return int(m.group(1), 16), int(m.group(2), 16) + 1
    # Imports/no-body records may lack a range; permit a small address window for objdump.
    a = int(addr, 16)
    return a, a + 0x100


def main():
    strings = elf_strings(BIN)
    chunks = sorted(SYMBOLS.glob("chunk_*.json"))
    records, names = [], defaultdict(list)
    for p in chunks:
        chunk = json.loads(p.read_text())
        cid, model = chunk.get("chunk", p.stem), chunk.get("model", "unknown")
        for f in chunk.get("functions", []):
            addr, name, conf = f.get("addr", ""), f.get("name", ""), f.get("confidence", "unknown")
            names[name].append({"chunk": cid, "addr": addr})
            needle = normalize_name(name)
            matches = [s for s in strings if needle and needle in s["text"]]
            pseudo_path = DECOMP / (addr.lower() + ".c")
            pseudo = pseudo_path.read_text(errors="replace") if pseudo_path.exists() else ""
            start, end = parse_range(pseudo, addr) if pseudo else (int(addr, 16), int(addr, 16) + 0x100)
            references = []
            for s in matches:
                h = f"0x{s['address']:08x}"
                addr_forms = {h, h[2:], h.upper(), f"0x{s['address']:x}"}
                literal = s["text"].replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t").replace('"', '\\"')
                has_ref = any(x in pseudo for x in addr_forms) or ('"' + literal) in pseudo
                if not has_ref:
                    try:
                        dis = subprocess.run(["objdump", "-d", f"--start-address={start:#x}", f"--stop-address={end:#x}", str(BIN)], text=True, capture_output=True, check=False).stdout
                        has_ref = any(x in dis for x in addr_forms)
                    except OSError:
                        dis = ""
                # Ghidra string labels commonly encode the string and its address.
                label = "s_" + re.sub(r"[^A-Za-z0-9]", "_", s["text"])
                if label in pseudo:
                    has_ref = True
                if has_ref:
                    references.append(h)
            if conf == "observed" and (name in DYNSYMS or name.replace("operator.", "operator ") in DYNSYMS):
                status = "PASS-import"
            elif conf == "observed":
                status = "PASS" if references else ("WEAK" if matches else "FAIL")
            else:
                status = "under-claimed" if matches else conf
            records.append({"chunk": cid, "chunk_file": p.name, "model": model, "addr": addr, "name": name,
                            "confidence": conf, "status": status, "matches": matches,
                            "references": references, "evidence": f.get("evidence", ""),
                            "placeholder": bool(re.fullmatch(r"fn_" + re.escape(addr.replace("0x", "")), name, re.I)),
                            "generic": bool(re.match(r"(?:sub_|func_|FUN_)", name))})

    duplicate_names = {n: places for n, places in names.items() if len(places) > 1}
    def summary(rows):
        counts = Counter(r["status"] for r in rows)
        counts["PASS"] += counts.pop("PASS-import", 0)
        obs = counts["PASS"] + counts["WEAK"] + counts["FAIL"]
        total = len(rows)
        return {"functions": len(rows), "observed_pass": counts["PASS"], "observed_weak": counts["WEAK"],
                "observed_fail": counts["FAIL"], "observed_rate": counts["PASS"] / obs if obs else None,
                "inferred": sum(r["confidence"] == "inferred" for r in rows),
                "unknown": sum(r["confidence"] == "unknown" for r in rows),
                "under_claimed": counts["under-claimed"], "placeholders": sum(r["placeholder"] for r in rows),
                "generic": sum(r["generic"] for r in rows),
                "rates_of_total": {"observed_pass": counts["PASS"] / total if total else 0,
                                   "observed_weak": counts["WEAK"] / total if total else 0,
                                   "observed_fail": counts["FAIL"] / total if total else 0,
                                   "inferred": sum(r["confidence"] == "inferred" for r in rows) / total if total else 0,
                                   "unknown": sum(r["confidence"] == "unknown" for r in rows) / total if total else 0,
                                   "placeholders": sum(r["placeholder"] for r in rows) / total if total else 0}}
    by_model = {m: summary([r for r in records if r["model"] == m]) for m in sorted({r["model"] for r in records})}
    by_chunk = {str(c): summary([r for r in records if str(r["chunk"]) == str(c)]) for c in sorted({r["chunk"] for r in records}, key=lambda x: int(x))}
    result = {"method": "ELF .rodata/.data printable NUL-terminated ASCII; observed name substring match, direct reference in pseudocode or bounded objdump disassembly; all checks are literal and heuristic.",
              "binary": str(BIN), "string_count": len(strings), "chunks": [p.name for p in chunks],
              "by_model": by_model, "by_chunk": by_chunk, "duplicate_names": duplicate_names,
              "placeholder_items": [r for r in records if r["placeholder"]],
              "generic_items": [r for r in records if r["generic"]], "items": records}
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_MD.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(result, indent=2) + "\n")
    lines = ["# Naming audit", "", "## Method", "", result["method"],
             "", "The script parses the ELF section table directly. A direct address or Ghidra-style `s_...` label in pseudocode counts as a reference; otherwise the function range from the pseudocode header is searched with `objdump -d`. Missing pseudocode uses a 0x100-byte fallback window. Each count column shows its rate of all functions in that group; PASS rate is also shown among observed functions.", "",
             "## Per-model", "", "| Model | Functions | PASS (n, total rate) | WEAK (n, total rate) | FAIL (n, total rate) | PASS rate of observed | Inferred (n, rate) | Unknown (n, rate) | Under-claimed | Placeholders (n, rate) |", "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for model, s in by_model.items():
        rate = "n/a" if s["observed_rate"] is None else f"{s['observed_rate']:.1%}"
        q=s["rates_of_total"]
        lines.append(f"| {model} | {s['functions']} | {s['observed_pass']} ({q['observed_pass']:.1%}) | {s['observed_weak']} ({q['observed_weak']:.1%}) | {s['observed_fail']} ({q['observed_fail']:.1%}) | {rate} | {s['inferred']} ({q['inferred']:.1%}) | {s['unknown']} ({q['unknown']:.1%}) | {s['under_claimed']} | {s['placeholders']} ({q['placeholders']:.1%}) |")
    lines += ["", "## Per chunk", "", "| Chunk | Model | Functions | PASS (n, rate) | WEAK (n, rate) | FAIL (n, rate) | PASS rate observed | Inferred (n, rate) | Unknown (n, rate) | Under-claimed | Placeholders (n, rate) |", "|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for c in sorted({r["chunk"] for r in records}, key=lambda x: int(x)):
        rows = [r for r in records if str(r["chunk"]) == str(c)]; s = summary(rows); rate = "n/a" if s["observed_rate"] is None else f"{s['observed_rate']:.1%}"
        q=s["rates_of_total"]
        lines.append(f"| {c} | {rows[0]['model']} | {s['functions']} | {s['observed_pass']} ({q['observed_pass']:.1%}) | {s['observed_weak']} ({q['observed_weak']:.1%}) | {s['observed_fail']} ({q['observed_fail']:.1%}) | {rate} | {s['inferred']} ({q['inferred']:.1%}) | {s['unknown']} ({q['unknown']:.1%}) | {s['under_claimed']} | {s['placeholders']} ({q['placeholders']:.1%}) |")
    for status in ("FAIL", "WEAK"):
        lines += ["", f"## {status} items", ""]
        selected = [r for r in records if r["status"] == status]
        if not selected: lines.append("None.")
        for r in selected:
            ms = ", ".join(f"`{m['text']}` @ `0x{m['address']:08x}`" for m in r["matches"])
            lines.append(f"- `{r['addr']}` `{r['name']}` (chunk {r['chunk']}): {r['evidence']}" + (f"; matched string(s): {ms}" if ms else "; no matching binary string."))
    lines += ["", "## Naming hygiene", "", f"Duplicate names: {len(duplicate_names)}; placeholders: {sum(r['placeholder'] for r in records)}; generic decompiler-style names: {sum(r['generic'] for r in records)}.", ""]
    OUT_MD.write_text("\n".join(lines))
    print(json.dumps({"by_model": by_model, "fail_items": sum(r["status"] == "FAIL" for r in records), "weak_items": sum(r["status"] == "WEAK" for r in records), "chunks": len(chunks)}, indent=2))


if __name__ == "__main__":
    main()
