#!/usr/bin/env python3
"""Link test for the cod2_lnxded 1.3 reconstruction: all chunks + the original data image.

Steps
  1. compile every recon/chunks/chunk_NNN.cpp (g++ -m32 -std=gnu++98 -w -fno-builtin -O0
     -fno-pie, parallel, at most 6 jobs, nice) into .local/cod2/build/obj/
  2. regenerate .local/cod2/globals/cod2_data.S (+ cod2_aliases.ld, symbol_map.json) with
     gen_globals.py unless --no-gen, and assemble it (as --32)
  3. link everything with g++ -m32 -no-pie ... -ldl -lpthread -lm
  4. classify: undefined globals before (chunks only) and after (with the data image),
     undefined functions (signature mismatch / renamed-by-address / truly missing),
     duplicate definitions

Prints a JSON summary; the full lists go to .local/cod2/build/link-report.json.
Exit status 0 only when the link succeeded.

  python3 games/call-of-duty-2/tools/link_test.py [--jobs 6] [--no-gen] [--no-probe]
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_globals as gg  # noqa: E402

BUILD = gg.BUILD
REPORT = BUILD / "link-report.json"
EXE = BUILD / "cod2_lnxded_recon"
LDFLAGS = ["-m32", "-no-pie", "-Wl,--no-demangle"]
LIBS = ["-ldl", "-lpthread", "-lm"]
CALL_RELOCS = {2, 4}  # R_386_PC32, R_386_PLT32


def demangle(names: list[str]) -> dict[str, str]:
    if not names:
        return {}
    try:
        out = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.splitlines()
    except OSError:
        return {n: n for n in names}
    return dict(zip(names, out)) if len(out) == len(names) else {n: n for n in names}


def link(objs: list[Path], extra: list[Path], out: Path = EXE, flags: list[str] | None = None) -> tuple[int, str]:
    cmd = ["nice", "-n", "10", "g++", *LDFLAGS, *(flags or []), "-o", str(out), *map(str, objs), *map(str, extra), *LIBS]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def parse_link_errors(text: str) -> tuple[set[str], set[str]]:
    und = set(re.findall(r"undefined reference to `([^']+)'", text))
    dup = set(re.findall(r"multiple definition of `([^']+)'", text))
    return und, dup


def linkable_objects(objs: list[Path]) -> list[Path]:
    """Objects with global aliases for static functions named by the data image."""
    aliases = json.loads((BUILD / "static_aliases.json").read_text()) if (BUILD / "static_aliases.json").exists() else {}
    out = []
    ldir = BUILD / "obj-link"
    ldir.mkdir(parents=True, exist_ok=True)
    for o in objs:
        m = aliases.get(o.stem)
        if not m:
            out.append(o)
            continue
        dst = ldir / o.name
        args = ["objcopy"]
        for a, v in sorted(m.items()):
            args += ["--add-symbol", f"{a}={v['section']}:{v['value']},global,function"]
        r = subprocess.run([*args, str(o), str(dst)], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(f"objcopy failed for {o}: {r.stderr}")
        out.append(dst)
    return out


def verify_image(objs: list[Path], data_obj: Path, smap: dict) -> dict:
    """Link a relaxed executable (undefined/multiple definitions tolerated) and check the image.

    Every alias must sit at (blob base + original offset) and its bytes must equal the
    original; every rewritten word must hold the relocated address of its target.
    """
    exe = BUILD / "cod2_lnxded_layoutcheck"
    rc, txt = link(objs, [data_obj, gg.OUTDIR / "cod2_aliases.ld"], exe,
                   ["-Wl,--unresolved-symbols=ignore-all", "-Wl,--allow-multiple-definition", "-Wl,--noinhibit-exec"])
    if not exe.exists():
        return {"ok": False, "error": txt[-2000:]}
    orig = gg.Elf32(gg.ORIGINAL)
    new = gg.Elf32(exe)
    syms = {s["name"].split("@")[0]: s["value"] for s in new.symbols(".symtab") if s["name"] and s["shndx"] != 0}

    def read(elf: gg.Elf32, a: int, n: int) -> bytes | None:
        for s in elf.sections:
            if s["flags"] & 2 and s["addr"] <= a and a + n <= s["addr"] + s["size"]:
                return bytes(n) if s["type"] == 8 else elf.data[s["offset"] + a - s["addr"]:s["offset"] + a - s["addr"] + n]
        return None

    bases = {sec: (orig.by_name[sec]["addr"], syms.get(label)) for sec, label in gg.BLOBS.items()}
    crt = (orig.by_name[".ctors"]["addr"], syms.get("cod2_crt_tables"))
    rewritten = {}
    for ln in (gg.OUTDIR / "cod2_data.S").read_text().splitlines():
        m = re.match(r"\t\.long\t(\S+)\t/\* 0x([0-9a-f]{8}): was 0x([0-9a-f]{8}) \*/", ln)
        if m:
            rewritten[int(m.group(2), 16)] = (m.group(1), int(m.group(3), 16))

    def moved(a: int) -> int | None:
        for o, n in bases.values():
            if n is not None and o <= a <= o + 0x1000000 and gg.section_of(orig, a) in (None, *gg.BLOBS) and \
                    any(orig.by_name[sec]["addr"] <= a <= orig.by_name[sec]["addr"] + orig.by_name[sec]["size"]
                        and bases[sec][0] == o for sec in gg.BLOBS):
                return n + a - o
        return None

    bad_addr, bad_bytes, checked, bad_ptr, ptr_checked = [], [], 0, [], 0
    for a_s, e in smap.items():
        if e.get("size_source") == "function":
            continue
        a = int(a_s, 16)
        exp = moved(a) if e["section"] in gg.BLOBS else (crt[1] + a - crt[0] if crt[1] else None)
        for n in [e["name"], *e["aliases"]]:
            checked += 1
            if syms.get(n) != exp:
                bad_addr.append({"symbol": n, "addr": a_s, "expected": exp, "got": syms.get(n)})
        if e["section"] in (".rodata", ".data") and exp is not None:
            size = min(e["size"], 0x10000)
            ob, nb = read(orig, a, size), read(new, exp, size)
            if ob is None or nb is None:
                continue
            for i in range(size):
                if ob[i] != nb[i] and not any(w <= a + i < w + 4 for w in range(a + i - 3, a + i + 1) if w in rewritten):
                    bad_bytes.append({"symbol": e["name"], "offset": i})
                    break
    for w, (expr, v) in sorted(rewritten.items()):
        nw = moved(w) if gg.section_of(orig, w) in gg.BLOBS else (crt[1] + w - crt[0] if crt[1] else None)
        got = int.from_bytes(read(new, nw, 4) or b"\0\0\0\0", "little") if nw else None
        tgt = moved(v) if expr.startswith("cod2_rodata") or expr.startswith("cod2_data") or expr.startswith("cod2_bss") \
            else None
        if tgt is None:
            m = re.match(r"([A-Za-z_$.][\w$.]*)(?:\+(\d+))?$", expr)
            tgt = (syms.get(m.group(1)) + int(m.group(2) or 0)) if m and syms.get(m.group(1)) is not None else None
        if tgt is None:
            continue  # import resolved through the PLT / COPY: not checked here
        ptr_checked += 1
        if got != tgt:
            bad_ptr.append({"word": f"0x{w:08x}", "expr": expr, "expected": tgt, "got": got})
    return {"ok": not (bad_addr or bad_bytes or bad_ptr), "aliases_checked": checked, "bad_addresses": bad_addr[:50],
            "objects_with_byte_differences": bad_bytes[:50], "pointers_checked": ptr_checked,
            "bad_pointers": bad_ptr[:50], "relaxed_link_returncode": rc}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--no-gen", action="store_true", help="use the existing .local/cod2/globals/cod2_data.S")
    ap.add_argument("--no-probe", action="store_true", help="gen_globals without sizeof() probes")
    ap.add_argument("--skip-before", action="store_true", help="skip the link without the data image")
    ap.add_argument("--no-verify", action="store_true", help="skip the relaxed-link layout verification")
    args = ap.parse_args()
    t0 = time.time()

    comp = gg.compile_chunks(gg.OBJDIR, args.jobs)
    objs = sorted(gg.OBJDIR.glob("chunk_*.o"))
    if not args.no_gen:
        gg.generate(gg.OBJDIR, args.jobs, probe=not args.no_probe)
    gen = json.loads((BUILD / "globals-report.json").read_text())
    smap = json.loads((gg.OUTDIR / "symbol_map.json").read_text())
    data_names = {n for e in smap.values() if e.get("size_source") != "function" for n in [e["name"], *e["aliases"]]}
    objs = linkable_objects(objs)
    data_obj = BUILD / "cod2_data.o"
    r = subprocess.run(["as", "--32", "-o", str(data_obj), str(gg.OUTDIR / "cod2_data.S")], capture_output=True,
                       text=True)
    if r.returncode:
        print(json.dumps({"error": "assembling cod2_data.S failed", "stderr": r.stderr[-4000:]}, indent=1))
        return 2

    scan = gg.scan_objects(gg.OBJDIR)
    sysnames, _ = gg.system_symbols()
    called = {n for n, k in scan["reloc_kinds"].items() if k & CALL_RELOCS}
    defined_any = set(scan["defined"])
    referenced = set(scan["undefined"])
    recs = gg.load_symbol_records()
    skipped = gen.get("skipped", {})

    fname_addr: dict[str, list[int]] = {}
    for a, v in recs["funcs"].items():
        fname_addr.setdefault(v.get("name", ""), []).append(a)
    crt_entry = {"main"}  # referenced by crt1.o

    def is_global(n: str) -> bool:
        """Data symbol? (functions: called, mangled, a recorded function name, or main)."""
        if n in data_names:
            return True
        if n in skipped:
            return n not in called
        if n in fname_addr or n in crt_entry:
            return False
        return not n.startswith("_Z") and n not in called and n not in sysnames

    # ---- before: chunks only
    before_und_nm = {n for n in referenced if n not in defined_any and n not in sysnames}
    before_globals = sorted(n for n in before_und_nm if is_global(n))
    link_before = None
    if not args.skip_before:
        rc, txt = link(objs, [])
        u, d = parse_link_errors(txt)
        link_before = {"returncode": rc, "undefined": len(u), "undefined_globals": len([n for n in u if is_global(n)]),
                       "multiple_definitions": len(d)}

    # ---- after: chunks + data image + function aliases
    rc, txt = link(objs, [data_obj, gg.OUTDIR / "cod2_aliases.ld"])
    und, dup = parse_link_errors(txt)
    other_errors = [ln for ln in txt.splitlines() if "error" in ln and "undefined reference" not in ln
                    and "multiple definition" not in ln][:40]
    after_globals = sorted(n for n in und if is_global(n))
    after_funcs = sorted(n for n in und if not is_global(n))

    verify = None if args.no_verify else verify_image(objs, data_obj, smap)

    # ---- classify undefined functions
    rsym = {int(a, 16): s for a, s in gen["recon_function_map"]["symbols"].items()}
    sym_addr = {s: a for a, s in rsym.items()}
    def_funcs: dict[str, list[str]] = {}
    for n, ds in scan["defined"].items():
        if any(k.startswith("func") for _c, k in ds):
            def_funcs.setdefault(gg.mangled_base(n), []).append(n)
    dem = demangle(after_funcs)
    mismatch, linkage, renamed, missing_known, missing_unknown, libc_like, sys_cpp = [], [], [], [], [], [], []
    for n in after_funcs:
        b = gg.mangled_base(n)
        users = sorted(scan["undefined"].get(n, ()))
        ent = {"symbol": n, "demangled": dem.get(n, n), "referenced_by": users}
        others = [s for s in def_funcs.get(b, []) if s != n]
        if others:
            ent["defined_as"] = [{"symbol": s, "demangled": d, "address": f"0x{sym_addr[s]:08x}" if s in sym_addr else None,
                                  "chunks": sorted({c for c, _ in scan["defined"][s]})}
                                 for s, d in demangle(others).items()]
            # caller and definition disagree only in language linkage (extern "C" vs C++)
            if not n.startswith("_Z") or all(not o.startswith("_Z") for o in others):
                linkage.append(ent)
            else:
                mismatch.append(ent)
            continue
        if n.startswith("_Z") and b in sysnames:
            sys_cpp.append(ent)  # a libc/libm function declared without extern "C"
            continue
        m = gg.ADDR_RE.search(b)
        addrs = [int(m.group(1), 16)] if m else sorted(set(fname_addr.get(b, [])))
        if addrs:
            a = addrs[0]
            ent["address"] = f"0x{a:08x}"
            ent["original_name"] = recs["funcs"].get(a, {}).get("name")
            if a in rsym:
                ent["reconstructed_as"] = rsym[a]
                renamed.append(ent)
            else:
                missing_known.append(ent)
            continue
        if not n.startswith("_Z"):
            libc_like.append(ent)
        else:
            missing_unknown.append(ent)

    # ---- duplicates (nm view + linker view)
    dups_nm = {}
    for n, ds in scan["defined"].items():
        strong = [(c, k) for c, k in ds if "-local" not in k and "-weak" not in k and ":.text." not in k
                  and not n.startswith("DW.ref.")]
        if len({c for c, _ in strong}) > 1:
            dups_nm[n] = sorted({c for c, _ in strong})
    crt_dups = sorted(d for d in dup if d not in dups_nm)
    dupl = {"between_chunks": {n: {"demangled": demangle([n]).get(n, n), "chunks": c} for n, c in sorted(dups_nm.items())},
            "with_crt_or_libs": {n: sorted({c for c, _ in scan["defined"].get(n, [])}) for n in crt_dups}}

    report = {
        "link_returncode": rc,
        "executable": str(EXE) if rc == 0 else None,
        "objects": len(objs),
        "compile_failures": comp["failed"],
        "undefined_globals_before": before_globals,
        "link_before": link_before,
        "undefined_globals_after": [{"symbol": n, "referenced_by": sorted(scan["undefined"].get(n, ())),
                                     "skipped_reason": skipped.get(n)} for n in after_globals],
        "undefined_functions": {
            "signature_mismatch": mismatch,
            "linkage_mismatch_c_vs_cpp": linkage,
            "system_function_declared_cpp": sys_cpp,
            "renamed_known_address": renamed,
            "missing_known_address": missing_known,
            "missing_unknown_cpp": missing_unknown,
            "missing_unmangled": libc_like,
        },
        "duplicate_definitions": dupl,
        "other_linker_errors": other_errors,
        "image_verification": verify,
        "data_image": {k: gen[k] for k in ("data_symbols_emitted", "data_addresses", "referenced_data_symbols_defined")},
        "pointer_rewrites": gen["pointer_scan"],
        "seconds": round(time.time() - t0, 1),
    }
    BUILD.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=1) + "\n")
    summary = {
        "link_ok": rc == 0,
        "objects": len(objs),
        "compile_failures": sorted(comp["failed"]),
        "undefined_globals_before": len(before_globals),
        "undefined_globals_after": len(after_globals),
        "undefined_symbols_after": len(und),
        "undefined_functions": {k: len(v) for k, v in report["undefined_functions"].items()},
        "duplicate_definitions": {k: len(v) for k, v in dupl.items()},
        "link_before": link_before,
        "pointer_rewrites": gen["pointer_scan"]["rewritten"],
        "image_verification": None if verify is None else {k: (len(v) if isinstance(v, list) else v)
                                                           for k, v in verify.items() if k != "error"},
        "report": str(REPORT),
        "seconds": report["seconds"],
    }
    print(json.dumps(summary, indent=1))
    return 0 if rc == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
