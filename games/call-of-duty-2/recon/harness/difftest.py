#!/usr/bin/env python3
"""Function-level differential tests: original cod2_lnxded vs one reconstructed chunk.

Builds hx_runner for a chunk (original image + reconstructed chunk linked against the
original's addresses), generates inputs from the recorded signature, runs both sides from
identical forked state and prints a JSON verdict.

  difftest.py --chunk 030 --func 0x080b7ae2 --cases 2000
  difftest.py --chunk 030 --all                 # batch; writes .local/cod2/harness/results/chunk_030.json
  difftest.py --selftest                        # mutated reconstruction must be detected

Standard library only (Python 3.11+). See analysis/differential-harness.md.
"""

from __future__ import annotations

import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import random
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GAME = HERE.parents[1]
REPO = GAME.parents[1]
RECON = GAME / "recon"
CHUNKS = RECON / "chunks"
INCLUDE = RECON / "include"
SYMBOLS = GAME / "analysis" / "symbols"
LOCAL = Path(os.environ.get("COD2_LOCAL", REPO / ".local" / "cod2"))
ORIGINAL = LOCAL / "lnxded-1.3" / "cod2_lnxded"
STDCXX5 = LOCAL / "runtime-i386" / "usr" / "lib" / "i386-linux-gnu" / "libstdc++.so.5"
WORK = LOCAL / "harness"
EXPECT_SHA = "05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7"

sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE))
import specs  # noqa: E402  (per-function overrides)

# Reconstruction compile flags. x87 math, no SSE, i686: GCC 3.3.6 targeted the x87 FPU.
# -fno-builtin keeps library calls as calls (the original's string helpers are calls);
# -fwrapv/-fno-strict-aliasing keep 2003-era integer/aliasing assumptions defined.
RECON_CFLAGS = [
    "-m32", "-std=gnu++98", "-O1", "-march=i686", "-mfpmath=387", "-mno-sse", "-fno-pic", "-fno-pie",
    "-fno-builtin", "-fno-strict-aliasing", "-fwrapv", "-fno-stack-protector", "-U_FORTIFY_SOURCE",
    "-D_FORTIFY_SOURCE=0", "-fno-omit-frame-pointer", "-w",
]
RUNNER_CFLAGS = ["-m32", "-O1", "-g", "-no-pie", "-fno-pie", "-Wl,-Ttext-segment=0x60000000", "-mno-sse",
                 "-fno-stack-protector"]
# glibc 2.38+ headers redirect some calls; route them to the original's equivalent import.
LIBC_ALIASES = {"__isoc99_sscanf": "sscanf", "__isoc23_sscanf": "sscanf"}


def log(*a):
    print(*a, file=sys.stderr)


# ----------------------------------------------------------------------------- ELF facts
def original_imports() -> tuple[dict[str, int], dict[str, int]]:
    """Return (import name -> PLT entry address, defined dynsym object name -> address)."""
    data = ORIGINAL.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECT_SHA:
        raise SystemExit(f"{ORIGINAL}: SHA-256 mismatch")
    shoff, = struct.unpack_from("<I", data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x2E)
    secs = []
    for i in range(shnum):
        secs.append(struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize))
    shstr = secs[shstrndx][4]

    def name(off, base):
        end = data.index(b"\0", base + off)
        return data[base + off:end].decode()

    by = {name(s[0], shstr): s for s in secs}
    dynsym, dynstr, relplt, plt = by[".dynsym"], by[".dynstr"], by[".rel.plt"], by[".plt"]
    syms = []
    for off in range(0, dynsym[5], 16):
        st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from("<IIIBBH", data, dynsym[4] + off)
        syms.append((name(st_name, dynstr[4]), st_value, st_shndx))
    imports = {}
    for i, off in enumerate(range(0, relplt[5], 8)):
        r_offset, r_info = struct.unpack_from("<II", data, relplt[4] + off)
        imports[syms[r_info >> 8][0]] = plt[3] + 16 * (i + 1)
    objects = {n: v for n, v, shndx in syms if shndx != 0 and v}
    return imports, objects


# ----------------------------------------------------------------------------- symbols
def load_symbol_records():
    funcs: dict[str, list[int]] = {}
    globs: dict[str, list[int]] = {}
    per_chunk: dict[int, dict] = {}
    merged = SYMBOLS / "symbols.json"
    paths = sorted(SYMBOLS.glob("chunk_[0-9][0-9][0-9].json"))
    for path in paths:
        try:
            d = json.loads(path.read_text())
        except (json.JSONDecodeError, OSError):
            continue
        per_chunk[int(path.stem.split("_")[1])] = d
        for f in d.get("functions", []):
            funcs.setdefault(f.get("name", ""), []).append(int(f["addr"], 16))
        for g in d.get("globals", []):
            globs.setdefault(g.get("name", ""), []).append(int(g["addr"], 16))
    if merged.exists():
        try:
            m = json.loads(merged.read_text())
            items = m.get("functions", m) if isinstance(m, dict) else m
            if isinstance(items, dict):
                items = [dict(v, addr=k) if isinstance(v, dict) else {"addr": k, "name": v} for k, v in items.items()]
            for f in items if isinstance(items, list) else []:
                if isinstance(f, dict) and f.get("name") and f.get("addr"):
                    funcs[f["name"]] = [int(str(f["addr"]), 16)]  # merged names are unique
        except (json.JSONDecodeError, ValueError, OSError):
            pass
    return funcs, globs, per_chunk


def provenance_names(cpp: Path) -> dict[int, str]:
    """addr -> declarator name from '// 0x080abcde  size=...' comments in a chunk source."""
    out: dict[int, str] = {}
    lines = cpp.read_text(errors="replace").splitlines()
    pend = None
    for line in lines:
        m = re.match(r"\s*//\s*0x([0-9a-fA-F]{8})\s+size=", line)
        if m:
            pend = int(m.group(1), 16)
            continue
        if pend is None or line.strip().startswith("//") or not line.strip():
            continue
        m = re.search(r"([A-Za-z_][\w:~]*)\s*\(", line)
        if m:
            out[pend] = m.group(1)
            pend = None
    return out


# ----------------------------------------------------------------------------- build
def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        raise RuntimeError(f"{' '.join(map(str, cmd))}\n{r.stdout}{r.stderr}")
    return r.stdout


def demangle(names: list[str]) -> list[str]:
    if not names:
        return []
    out = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout
    return out.splitlines()


def base_name(dem: str) -> str:
    return dem.split("(")[0].strip()


def build(chunk: int, source: Path | None = None, tag: str = "", extra_cflags: list[str] | None = None) -> dict:
    """Compile the chunk, alias its unresolved symbols to the original image, link hx_runner."""
    src = source or CHUNKS / f"chunk_{chunk:03d}.cpp"
    if not src.exists():
        raise SystemExit(f"no reconstruction source {src}")
    cflags = RECON_CFLAGS + (extra_cflags or [])
    inputs = [src, CHUNKS / f"chunk_{chunk:03d}_types.h", HERE / "hx_runner.c", HERE / "hx_callstub.S",
              Path(__file__), *sorted(INCLUDE.glob("*.h")), *sorted(SYMBOLS.glob("*.json"))]
    h = hashlib.sha256(" ".join(cflags).encode())
    for p in inputs:
        if p.exists():
            h.update(p.name.encode() + p.read_bytes())
    key = h.hexdigest()[:16]
    bdir = WORK / "build" / f"chunk_{chunk:03d}{tag}"
    meta_path = bdir / "build.json"
    if meta_path.exists():
        meta = json.loads(meta_path.read_text())
        if meta.get("key") == key and Path(meta["runner"]).exists():
            return meta
    if bdir.exists():
        shutil.rmtree(bdir)
    bdir.mkdir(parents=True)
    obj = bdir / "chunk.o"
    run(["g++", *cflags, "-I", str(src.parent), "-I", str(INCLUDE), "-I", str(CHUNKS), "-c", str(src), "-o", str(obj)])

    imports, dynobjs = original_imports()
    funcs, globs, per_chunk = load_symbol_records()
    rec = per_chunk.get(chunk, {})
    chunk_funcs = {int(f["addr"], 16): f for f in rec.get("functions", [])}
    prov = provenance_names(src)

    nm = run(["nm", "-P", str(obj)]).splitlines()
    undefined, defined = [], []
    for line in nm:
        parts = line.split()
        if len(parts) >= 2:
            (undefined if parts[1] == "U" else defined).append((parts[0], parts[1]))
    dem_u = demangle([s for s, _ in undefined])
    dem_d = demangle([s for s, _ in defined])

    # in-chunk definitions -> original addresses
    name_to_addr: dict[str, list[int]] = {}
    for addr, f in chunk_funcs.items():
        for n in {f.get("name", ""), prov.get(addr, "")}:
            if n:
                name_to_addr.setdefault(n, []).append(addr)
                name_to_addr.setdefault(n.split("::")[-1] if "::" not in n else n, []).append(addr)
    for addr, n in prov.items():
        name_to_addr.setdefault(n, []).append(addr)
    table = []
    warnings = []
    seen_addr = set()
    localize = []
    for (sym, typ), dem in zip(defined, dem_d):
        if typ not in "Tt":
            if typ in "DdBbRr" and re.fullmatch(r"g_[0-9a-f]{8}", base_name(dem)):
                warnings.append(f"chunk defines placeholder global {dem}; it is NOT shared with the original")
            continue
        b = base_name(dem)
        addrs = sorted(set(name_to_addr.get(b, [])))
        if len(addrs) == 1 and addrs[0] not in seen_addr:
            table.append((addrs[0], "hxr_" + sym, b))
            seen_addr.add(addrs[0])
            if typ == "t":
                localize.append(sym)
        elif len(addrs) > 1:
            warnings.append(f"ambiguous in-chunk name {b}: {[hex(a) for a in addrs]}")

    # out-of-chunk / unresolved references -> original addresses
    defsyms: dict[str, int] = {}
    renames: dict[str, str] = {}
    traps: list[tuple[str, str]] = []
    aliases = []
    for i, ((sym, _), dem) in enumerate(zip(undefined, dem_u)):
        b = base_name(dem)
        addr = None
        how = ""
        m = re.fullmatch(r"(?:fn|g)_([0-9a-f]{8})", b)
        if m:
            addr, how = int(m.group(1), 16), "placeholder"
        elif sym in imports or LIBC_ALIASES.get(sym) in imports:
            addr, how = imports[sym if sym in imports else LIBC_ALIASES[sym]], "original PLT"
        elif sym in dynobjs:
            addr, how = dynobjs[sym], "original dynsym object"
        else:
            cands = sorted(set(name_to_addr.get(b, [])))
            if len(cands) == 1:
                addr, how = cands[0], "in-chunk name (original)"
            elif b in globs and len(set(globs[b])) == 1:
                addr, how = globs[b][0], "named global"
            elif b in funcs and len(set(funcs[b])) == 1:
                addr, how = funcs[b][0], "named function"
            elif b in funcs or b in globs:
                warnings.append(f"ambiguous name {b}: {[hex(a) for a in sorted(set(funcs.get(b, []) + globs.get(b, [])))]}")
        if addr is not None:
            new = f"__hx_sym_{i}"
            renames[sym] = new
            defsyms[new] = addr
            aliases.append({"symbol": dem, "addr": f"0x{addr:08x}", "via": how})
        elif sym != dem or not re.fullmatch(r"[A-Za-z_]\w*", sym):
            traps.append((sym, dem))  # unknown C++ symbol
        # else: plain C name, left for the runner's libc (checked at link time)

    def emit_and_link(traps):
        for sym, dem in traps:
            renames[sym] = f"__hx_trap_{len(renames)}"
        robj = bdir / "chunk_linked.o"
        args = ["objcopy"]
        for old, new in renames.items():
            args += ["--redefine-sym", f"{old}={new}"]
        # every definition gets an hxr_ prefix so reconstructed _start/_init/strlen/... can
        # never collide with the runner, crt or libc
        for sym, typ in defined:
            if typ != "U" and not sym.startswith("."):
                args += ["--redefine-sym", f"{sym}=hxr_{sym}"]
        run([*args, str(obj), str(bdir / "chunk_renamed.o")])
        run(["objcopy", *[a for s in localize for a in ("--globalize-symbol", "hxr_" + s)],
             str(bdir / "chunk_renamed.o"), str(robj)])
        with open(bdir / "aliases.ld", "w") as f:
            for n, a in sorted(defsyms.items()):
                f.write(f"{n} = 0x{a:08x};\n")
        with open(bdir / "recon_table.c", "w") as f:
            f.write("#include <stdint.h>\n")
            f.write("struct hx_recon_entry { uint32_t orig; void *recon; const char *name; };\n")
            f.write("__attribute__((noreturn)) void hx_trap_recon(const char *name);\n")
            for i, (addr, sym, b) in enumerate(table):
                f.write(f'extern char hx_r{i}[] __asm__("{sym}");\n')
            f.write("const struct hx_recon_entry hx_recon_table[] = {\n")
            for i, (addr, sym, b) in enumerate(table):
                f.write(f'  {{0x{addr:08x}u, hx_r{i}, "{b}"}},\n')
            f.write("  {0, 0, 0}\n};\n")
            f.write(f"const int hx_recon_count = {len(table)};\n")
            for sym, dem in traps:
                f.write(f'void {renames[sym]}(void) {{ hx_trap_recon("{dem}"); }}\n')
        runner = bdir / "hx_runner"
        r = subprocess.run(["gcc", *RUNNER_CFLAGS, "-o", str(runner), str(HERE / "hx_runner.c"),
                            str(HERE / "hx_callstub.S"), str(bdir / "recon_table.c"), str(robj),
                            str(bdir / "aliases.ld"), "-ldl", "-lm", "-lpthread"],
                           capture_output=True, text=True)
        return r, runner

    r, runner = emit_and_link(traps)
    if r.returncode:
        missing = sorted(set(re.findall(r"undefined reference to `([^']+)'", r.stderr)))
        if not missing:
            raise RuntimeError(r.stderr)
        rev = {v: k for k, v in renames.items()}
        more = []
        for mname in missing:
            raw = rev.get(mname, mname)
            if raw in [s for s, _ in undefined] and raw not in renames:
                more.append((raw, demangle([raw])[0]))
        if not more:
            raise RuntimeError(r.stderr)
        traps += more
        r, runner = emit_and_link(more)
        if r.returncode:
            raise RuntimeError(r.stderr)
    sizes = probe_struct_sizes(bdir, src, chunk, [f.get("signature", "") for f in chunk_funcs.values()], cflags)
    meta = {
        "struct_sizes": sizes,
        "key": key, "chunk": chunk, "source": str(src), "runner": str(runner), "cflags": cflags,
        "recon_functions": [{"addr": f"0x{a:08x}", "symbol": s, "name": b} for a, s, b in table],
        "aliases": aliases, "traps": [d for _, d in traps], "warnings": warnings,
    }
    meta_path.write_text(json.dumps(meta, indent=1))
    return meta


def probe_struct_sizes(bdir: Path, src: Path, chunk: int, sigs: list[str], cflags: list[str]) -> dict[str, int]:
    """sizeof() of every struct named in the chunk's signatures, as the reconstruction headers
    define it (incomplete types are dropped)."""
    names = sorted({m for sig in sigs for m in re.findall(r"\b(?:struct|union)\s+(\w+)", sig)})
    if not names:
        return {}
    heads = [ln for ln in src.read_text(errors="replace").splitlines() if ln.startswith("#include")]
    probe = bdir / "sizes.cpp"
    for _ in range(4):
        body = "\n".join(f"char hx_sizeof_{n}[sizeof(struct {n})];" for n in names)
        probe.write_text("\n".join(heads) + "\n" + body + "\n")
        r = subprocess.run(["g++", *cflags, "-fno-common", "-I", str(src.parent), "-I", str(INCLUDE), "-I", str(CHUNKS),
                            "-c", str(probe),
                            "-o", str(bdir / "sizes.o")], capture_output=True, text=True)
        if r.returncode == 0:
            break
        bad = set(re.findall(r"hx_sizeof_(\w+)", r.stderr)) | set(re.findall(r"struct (\w+)'", r.stderr))
        if not bad & set(names):
            return {}
        names = [n for n in names if n not in bad]
        if not names:
            return {}
    else:
        return {}
    out = {}
    for line in run(["nm", "-S", str(bdir / "sizes.o")]).splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3].startswith("hx_sizeof_"):
            out[parts[3][len("hx_sizeof_"):]] = int(parts[1], 16)
    return out


# ----------------------------------------------------------------------------- signatures
INT_TYPES = {
    "char": (8, True), "signed char": (8, True), "unsigned char": (8, False), "byte": (8, False),
    "uint8": (8, False), "int8": (8, True), "bool": (8, False), "short": (16, True), "short int": (16, True),
    "int16": (16, True), "unsigned short": (16, False), "uint16": (16, False), "int": (32, True),
    "int32": (32, True), "long": (32, True), "long int": (32, True), "signed": (32, True), "signed int": (32, True),
    "unsigned": (32, False), "unsigned int": (32, False), "uint32": (32, False), "unsigned long": (32, False),
    "size_t": (32, False), "qboolean": (32, True),
}
I64_TYPES = {"int64", "uint64", "long long", "unsigned long long", "long long int"}
FLOATS_N = {"vec2_t": 2, "vec3_t": 3, "vec4_t": 4}
KEYWORDS = {"const", "volatile", "struct", "union", "enum", "unsigned", "signed", "short", "long", "int", "char"}


def struct_sizes() -> dict[str, int]:
    """Struct sizes asserted with COD2_ASSERT_SIZE in the shared and chunk headers."""
    out: dict[str, int] = {}
    for h in [*INCLUDE.glob("*.h"), *CHUNKS.glob("*_types.h")]:
        for m in re.finditer(r"COD2_ASSERT_SIZE\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\)", h.read_text(errors="replace")):
            out.setdefault(m.group(1), int(m.group(2), 0))
    return out


STRUCT_SIZES = struct_sizes()


class Unsupported(Exception):
    pass


def norm_type(t: str) -> str:
    t = re.sub(r"\b(const|volatile|struct|union|enum)\b", " ", t)
    t = re.sub(r"\s*\*\s*", "*", t)
    return re.sub(r"\s+", " ", t).strip()


def parse_param(p: str) -> dict:
    p = p.strip()
    if p == "...":
        return {"kind": "varargs"}
    if "(" in p:
        raise Unsupported(f"function pointer parameter '{p}'")
    arr = None
    m = re.search(r"\[(\w*)\]\s*$", p)
    if m:
        arr = m.group(1)
        p = p[: m.start()].strip()
    toks = re.findall(r"[A-Za-z_]\w*|\*", p)
    name = ""
    if len(toks) >= 2 and toks[-1] != "*" and toks[-1] not in KEYWORDS and not (
            len(toks) == 2 and toks[0] in ("unsigned", "signed") and toks[-1] in INT_TYPES):
        name = toks[-1]
        p = p[: p.rfind(name)].strip()
    const = bool(re.search(r"\bconst\b", p))
    t = norm_type(p)
    if arr is not None:
        t += "*"
    return {"type": t, "name": name, "const": const, "raw": p}


def classify(par: dict, ret: bool = False) -> dict:
    if par.get("kind") == "varargs":
        return par
    t = par["type"]
    stars = t.count("*")
    base = t.replace("*", "").strip()
    c = dict(par)
    if stars == 0:
        if base == "void":
            c["kind"] = "void"
        elif base in INT_TYPES:
            c["kind"], (c["bits"], c["signed"]) = "int", INT_TYPES[base]
        elif base in I64_TYPES:
            c["kind"] = "i64"
        elif base in ("float", "vec_t"):
            c["kind"] = "f32"
        elif base == "double":
            c["kind"] = "f64"
        elif base in FLOATS_N and not ret:
            c["kind"], c["n"] = "fbuf", FLOATS_N[base]
        else:
            raise Unsupported(f"by-value type '{par['raw']}'")
        return c
    if stars == 1 and base in ("char", "signed char", "unsigned char", "byte", "uint8", "int8"):
        c["kind"] = "str" if par["const"] else "buf"
    elif stars == 2 and base == "char":
        c["kind"] = "strpp"
    elif stars == 1 and base in ("float", "vec_t") or stars == 1 and base in FLOATS_N:
        c["kind"], c["n"] = "fbuf", 4 * FLOATS_N.get(base, 4)
    elif stars == 1 and base == "double":
        c["kind"] = "dbuf"
    elif stars == 1 and base in INT_TYPES or stars == 1 and base in I64_TYPES:
        c["kind"] = "ibuf"
    else:
        c["kind"] = "opaque"
    if ret:
        c["kind"] = "retstr" if c["kind"] in ("str", "buf") else "ptr"
    return c


def parse_signature(sig: str) -> dict:
    sig = sig.strip().rstrip(";").strip()
    m = re.match(r"^(.*?)([A-Za-z_][\w:]*)\s*\((.*)\)\s*(const)?$", sig, re.S)
    if not m:
        raise Unsupported(f"cannot parse signature '{sig}'")
    rett, name, params = m.group(1).strip(), m.group(2), m.group(3).strip()
    if "::" in name and not re.search(r"\bstatic\b", rett):
        raise Unsupported("C++ member function (implicit this) - add a spec")
    rett = re.sub(r"\b(static|inline|extern|virtual)\b", "", rett).strip()
    ret = classify({"type": norm_type(rett), "name": "", "const": "const" in rett, "raw": rett}, ret=True)
    if ret["kind"] in ("fbuf",) or ret.get("kind") not in ("void", "int", "i64", "f32", "f64", "ptr", "retstr"):
        raise Unsupported(f"return type '{rett}'")
    plist = [] if params in ("", "void") else split_params(params)
    ps = [classify(parse_param(p)) for p in plist]
    return {"name": name, "ret": ret, "params": ps}


def split_params(s: str) -> list[str]:
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return out


# ----------------------------------------------------------------------------- input generation
SIZE_NAME = re.compile(r"(size|len|count|max|limit|^n$|^num|bufferlength|destsize)", re.I)
CHAR_NAME = re.compile(r"^(c|ch|chr|character|c1|c2)$", re.I)
INDEX_NAME = re.compile(r"(index|slot|thread|^i$|^idx|handle|num$|^x$|^y$)", re.I)
INT_BOUNDS = [0, 1, -1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 100, 127, 128, 255, 256, 1023, 1024, 0x3FF, 0x400,
              0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, -0x80000000, -2, -128, -129]
FLOAT_SPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 0.25, 1e-7, -1e-7, 1e30, -1e30, 3.4028234663852886e38,
                 1.1754943508222875e-38, 1e-45, float("inf"), float("-inf"), float("nan"), 3.14159265, 90.0,
                 180.0, 360.0, -180.0, 1000.0, 0.999999, 1.000001]
ALNUM = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
INFO_KEYS = ["name", "sv_hostname", "g_gametype", "mapname", "protocol", "rate", "snaps", "cl_punkbuster", "fs_game",
             "Name", "NAME", "k", "", "key with space", "x" * 40]


def rand_text(rng: random.Random, n: int, alphabet: str | None = None) -> bytes:
    if alphabet is None:
        pool = rng.choice([
            ALNUM, ALNUM + " _-.", "abcABC ", ALNUM + "\\;\"'%^/*?.,:{}()[]<>=+-!&|\t\n\r ",
            "".join(chr(c) for c in range(1, 128)), None,
        ])
        if pool is None:
            return bytes(rng.randint(1, 255) for _ in range(n))
        alphabet = pool
    return "".join(rng.choice(alphabet) for _ in range(n)).encode("latin-1")


def gen_string(rng: random.Random, name: str, func: str, prev: list[bytes]) -> bytes:
    lname, lfunc = name.lower(), func.lower()
    if prev and rng.random() < 0.4:
        p = rng.choice(prev)
        op = rng.randrange(6)
        if op == 0:
            return p
        if op == 1:
            return p.swapcase()
        if op == 2:
            return p[: rng.randint(0, len(p))]
        if op == 3 and p:
            i = rng.randrange(len(p))
            return p[:i] + bytes([rng.randint(1, 255)]) + p[i + 1:]
        if op == 4:
            return p + rand_text(rng, rng.randint(1, 5))
        return p.upper()
    if lfunc.startswith("info_") or "info" in lname:
        if lname in ("key",):
            return rng.choice(INFO_KEYS).encode() if rng.random() < 0.8 else rand_text(rng, rng.randint(0, 12))
        if lname in ("value",):
            return rand_text(rng, rng.choice([0, 1, 5, 20, 300, 1100]))
        pairs = []
        for _ in range(rng.choice([0, 1, 2, 3, 5, 10, 60, 200])):
            k = rng.choice(INFO_KEYS) if rng.random() < 0.6 else rand_text(rng, rng.randint(1, 10), ALNUM)
            v = rand_text(rng, rng.randint(0, 20), ALNUM + " _.")
            pairs.append(b"\\" + k.encode() if isinstance(k, str) else b"\\" + k)
            pairs.append(b"\\" + v)
        s = b"".join(pairs)
        if rng.random() < 0.1:
            s = s[1:]  # missing leading backslash
        return s[: rng.choice([len(s), len(s), 1023, 1100, 8191, 9000])]
    if "fmt" in lname or "format" in lname:
        return rand_text(rng, rng.randint(0, 30), ALNUM + " ")
    if any(k in lname for k in ("path", "file", "ext", "dir")):
        parts = [rand_text(rng, rng.randint(0, 8), ALNUM) for _ in range(rng.randint(0, 4))]
        sep = rng.choice([b"/", b"\\", b"/"])
        s = sep.join(parts)
        if rng.random() < 0.6:
            s += b"." + rand_text(rng, rng.randint(0, 4), ALNUM)
        return s
    if "pattern" in lname or "wild" in lfunc:
        return rand_text(rng, rng.randint(0, 12), "ab*?AB.c")
    r = rng.random()
    if r < 0.08:
        return b""
    if r < 0.5:
        return rand_text(rng, rng.randint(1, 16))
    if r < 0.7:  # parser-ish text
        toks = [b"word", b"\"quoted str\"", b"//comment\n", b"/* block */", b"123", b"-5", b"1.5", b",", b"\n",
                b" ", b"\t", b"{", b"}", b"+=", b"&&", b"\"unterminated", b"a,b,,c", b"\\", b"^1red"]
        return b"".join(rng.choice(toks) for _ in range(rng.randint(1, 20)))
    if r < 0.9:
        return rand_text(rng, rng.randint(17, 300))
    return rand_text(rng, rng.choice([1023, 1024, 1500, 4000, 8192, 9000]))


def int_value(rng: random.Random, p: dict, ctx: dict) -> int:
    name = p.get("name", "")
    bits, signed = p.get("bits", 32), p.get("signed", True)
    if p["type"] == "bool":
        v = rng.choice([0, 1, 1, 0, 2, 255])
    elif SIZE_NAME.search(name):
        cap = ctx.get("last_cap", 64)
        ln = ctx.get("last_len", 8)
        v = rng.choice([0, 1, 2, 3, ln, ln + 1, max(ln - 1, 0), cap, cap - 1, rng.randint(0, max(cap, 1)),
                        rng.randint(0, 64), -1 if rng.random() < 0.1 else 4])
    elif CHAR_NAME.search(name) or bits == 8:
        v = rng.choice([rng.randint(-128, 255), rng.randint(0, 127), 0, -1, 0x7F, 0x80, 0xFF, 0x20, ord("a"),
                        ord("Z"), ord("_"), ord("\\")])
    elif INDEX_NAME.search(name):
        v = rng.choice([0, 0, 1, 2, 3, rng.randint(0, 15), rng.randint(0, 63)])
    else:
        r = rng.random()
        v = rng.choice(INT_BOUNDS) if r < 0.4 else rng.randint(-100, 100) if r < 0.7 else rng.getrandbits(32)
    v &= (1 << bits) - 1
    if signed and v >= 1 << (bits - 1):
        v -= 1 << bits
    return v & 0xFFFFFFFF  # caller extends to 32 bits


def float_value(rng: random.Random) -> float:
    r = rng.random()
    if r < 0.3:
        return rng.choice(FLOAT_SPECIAL)
    if r < 0.6:
        return rng.uniform(-1.0, 1.0)
    if r < 0.85:
        return rng.uniform(-10000.0, 10000.0)
    return struct.unpack("<f", struct.pack("<I", rng.getrandbits(32)))[0]


def f32_bits(v: float) -> int:
    try:
        return struct.unpack("<I", struct.pack("<f", v))[0]
    except OverflowError:
        return 0x7F800000 if v > 0 else 0xFF800000


def gen_varargs(rng: random.Random, objs: list, words: list) -> bytes:
    """Format string with matching arguments for printf-like varargs functions."""
    convs = ["%d", "%i", "%u", "%x", "%X", "%s", "%c", "%%", "%5d", "%-8s", "%.3s", "%f", "%.2f", "%g", "%08x",
             "%s", "%s", "%d"]
    fmt = b""
    for _ in range(rng.randint(0, 5)):
        fmt += rand_text(rng, rng.randint(0, 6), ALNUM + " :=-")
        c = rng.choice(convs)
        fmt += c.encode()
        if c == "%%":
            continue
        if c[-1] in "diuxXc":
            words.append(("w", int_value(rng, {"type": "int", "name": "v"}, {})))
        elif c[-1] in "fg":
            d = struct.unpack("<Q", struct.pack("<d", rng.choice([0.0, 1.5, -2.25, 1e10, 123.456, 1e300])))[0]
            words += [("w", d & 0xFFFFFFFF), ("w", d >> 32)]
        else:
            s = rng.choice([b"", b"abc", rand_text(rng, rng.randint(0, 40)), rand_text(rng, rng.choice([600, 1200]))])
            objs.append((len(s) + 1, s))
            words.append(("o", len(objs) - 1))
    if rng.random() < 0.15:
        fmt += rand_text(rng, rng.choice([500, 1100]), ALNUM)
    return fmt


def gen_case(rng: random.Random, sig: dict, spec: dict) -> dict:
    """Return {'objs': [(size, bytes)], 'fix': [...], 'args': [('w', v) | ('o', id)]}."""
    objs: list[tuple[int, bytes]] = []
    fixes: list[tuple[int, int, int, int]] = []
    words: list[tuple[str, int]] = []
    ctx: dict = {}
    prev: list[bytes] = []
    gens = spec.get("args", {})
    params = sig["params"]
    for idx, p in enumerate(params):
        k = p["kind"]
        name = p.get("name", "")
        custom = gens.get(name) or gens.get(idx)
        if custom:
            custom(rng, objs, fixes, words, ctx, prev)
            continue
        if k == "int":
            words.append(("w", int_value(rng, p, ctx)))
        elif k == "i64":
            v = rng.choice([0, 1, -1 & (2**64 - 1), 2**63, 2**32, 0x0123456789ABCDEF, rng.getrandbits(64)])
            words += [("w", v & 0xFFFFFFFF), ("w", v >> 32)]
        elif k == "f32":
            words.append(("w", f32_bits(float_value(rng))))
        elif k == "f64":
            d = struct.unpack("<Q", struct.pack("<d", float_value(rng)))[0]
            words += [("w", d & 0xFFFFFFFF), ("w", d >> 32)]
        elif k in ("str", "buf"):
            if p.get("const") or k == "str":
                if rng.random() < spec.get("null_prob", 0.02):
                    words.append(("w", 0))
                    continue
            s = gen_string(rng, name, sig["name"], prev)
            prev.append(s)
            if k == "str":
                cap = len(s) + 1
            else:
                cap = max(len(s) + 1, rng.choice(spec.get("buf_caps", [0x4400, 0x4400, 0x400, 64, len(s) + 1])))
            objs.append((cap, s))
            ctx["last_cap"], ctx["last_len"] = cap, len(s)
            words.append(("o", len(objs) - 1))
        elif k == "strpp":
            s = gen_string(rng, name, sig["name"], prev)
            prev.append(s)
            objs.append((len(s) + 1, s))
            objs.append((4, b""))
            fixes.append((len(objs) - 1, 0, len(objs) - 2, 0))
            words.append(("o", len(objs) - 1))
        elif k == "fbuf":
            n = max(p.get("n", 16), 16)
            data = b"".join(struct.pack("<I", f32_bits(float_value(rng))) for _ in range(n))
            objs.append((4 * n, data))
            words.append(("o", len(objs) - 1))
        elif k == "dbuf":
            data = b"".join(struct.pack("<d", float_value(rng)) for _ in range(16))
            objs.append((128, data))
            words.append(("o", len(objs) - 1))
        elif k == "ibuf":
            data = b"".join(struct.pack("<I", int_value(rng, {"type": "int", "name": ""}, {})) for _ in range(32))
            objs.append((128, data))
            words.append(("o", len(objs) - 1))
        elif k == "varargs":
            # the preceding const char * parameter is the format: replace its content
            fmt = gen_varargs(rng, objs, words)
            fobj = ctx.get("fmt_obj")
            if fobj is not None:
                objs[fobj] = (len(fmt) + 1, fmt)
        elif k == "opaque":
            base = p["type"].replace("*", "").strip()
            if p["type"].count("*") == 2:
                # pointer to an out-pointer slot (e.g. void **outData): one zeroed word
                objs.append((4, b""))
                words.append(("o", len(objs) - 1))
                continue
            size = STRUCT_SIZES.get(base) if p["type"].count("*") == 1 else None
            if not size:
                raise Unsupported(f"opaque pointer parameter '{p['raw']} {name}' needs a spec")
            # struct with a recorded COD2_ASSERT_SIZE: zero-filled or random bytes; random
            # embedded pointers usually crash the original and the case is discarded
            data = bytes(size) if rng.random() < 0.5 else bytes(rng.getrandbits(8) for _ in range(size))
            objs.append((size, data))
            words.append(("o", len(objs) - 1))
        else:
            raise Unsupported(f"parameter kind {k}")
        if k in ("str", "buf") and idx + 1 < len(params) and params[idx + 1].get("kind") == "varargs":
            ctx["fmt_obj"] = len(objs) - 1
    return {"objs": objs, "fix": fixes, "args": words}


def write_cases(path: Path, addr: int, sig: dict, spec: dict, n: int, seed: int) -> None:
    rng = random.Random(seed)
    ret = sig["ret"]
    rk = {"void": "v 0", "int": f"i {ret.get('bits', 32)}", "i64": "q 64", "f32": "f 32", "f64": "d 64",
          "ptr": "p 32", "retstr": "s 32"}[ret["kind"]]
    with open(path, "w") as f:
        f.write(f"target {addr:08x}\nret {rk}\ntimeout {spec.get('timeout', 2)}\n")
        if spec.get("strict_imports"):
            f.write("strict_imports\n")
        for a, ws in specs.DEFAULT_INIT + spec.get("init", []):
            f.write(f"init {a:08x} {' '.join(f'{w & 0xFFFFFFFF:08x}' for w in ws)}\n")
        for i in range(n):
            c = gen_case(rng, sig, spec)
            f.write(f"case {i}\n")
            for a, ws in spec.get("pre", []):
                f.write(f"pre {a:08x} {' '.join(f'{w & 0xFFFFFFFF:08x}' for w in ws)}\n")
            for a, data in (spec["mem"](rng) if callable(spec.get("mem")) else spec.get("mem", [])):
                f.write(f"mem {a:08x} {data.hex() or '-'}\n")
            for j, (size, data) in enumerate(c["objs"]):
                f.write(f"obj {j} {size:x} {data.hex() or '-'}\n")
            for o, off, t, toff in c["fix"]:
                f.write(f"fix {o} {off:x} {t} {toff:x}\n")
            for t, v in c["args"]:
                f.write(f"arg w {v & 0xFFFFFFFF:08x}\n" if t == "w" else f"arg o {v} 0\n")
            f.write("go\n")


# ----------------------------------------------------------------------------- run
def test_function(meta: dict, chunk: int, addr: int, cases: int, seed: int, keep: bool = False) -> dict:
    _, _, per_chunk = load_symbol_records()
    rec = {int(f["addr"], 16): f for f in per_chunk.get(chunk, {}).get("functions", [])}
    f = rec.get(addr)
    verdict = {"chunk": f"{chunk:03d}", "addr": f"0x{addr:08x}", "name": f.get("name") if f else None,
               "signature": f.get("signature") if f else None, "status": None}
    recon_addrs = {int(r["addr"], 16) for r in meta["recon_functions"]}
    spec = specs.SPECS.get(addr, {})
    if spec.get("skip"):
        return {**verdict, "verdict": "unsupported", "reason": spec["skip"]}
    if f is None and "signature" not in spec:
        return {**verdict, "verdict": "unsupported", "reason": "no symbol record"}
    verdict["status"] = f.get("status") if f else None
    if addr not in recon_addrs:
        return {**verdict, "verdict": "unsupported", "reason": "function not found in compiled reconstruction"}
    STRUCT_SIZES.update(meta.get("struct_sizes", {}))
    try:
        sig = parse_signature(spec.get("signature") or f["signature"])
        gen_case(random.Random(0), sig, spec)  # probe for unsupported parameter kinds
    except Unsupported as e:
        return {**verdict, "verdict": "unsupported", "reason": str(e)}
    rdir = WORK / "runs" / f"chunk_{chunk:03d}"
    rdir.mkdir(parents=True, exist_ok=True)
    cpath, opath = rdir / f"{addr:08x}.cases", rdir / f"{addr:08x}.results"
    write_cases(cpath, addr, sig, spec, cases, seed)
    cmd = ["nice", "-n", "10", meta["runner"], str(ORIGINAL), str(cpath), str(opath)]
    if STDCXX5.exists():
        cmd.append(str(STDCXX5))
    sandbox = WORK / "sandbox"
    sandbox.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, HX_SANDBOX=str(sandbox))
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=max(900, cases), env=env,
                       stdin=subprocess.DEVNULL)
    lines = [json.loads(x) for x in opath.read_text().splitlines() if x.strip()] if opath.exists() else []
    header = lines[0] if lines and lines[0].get("header") else {}
    results = [x for x in lines if "case" in x]
    errors = [x for x in lines if "error" in x]
    aborted = next((x["aborted"] for x in lines if "aborted" in x), None)
    counts: dict[str, int] = {}
    for r in results:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    fails = [r for r in results if r["status"] in ("fail", "error")]
    notes = [r for r in results if r["status"] == "pass" and ("what" in r or "note" in r)]
    discards: dict[str, int] = {}
    for r in results:
        if r["status"] == "discard":
            discards[r["why"]] = discards.get(r["why"], 0) + 1
    out = {**verdict, "cases": len(results), "counts": counts, "discard_reasons": discards,
           "unresolved_imports": header.get("unresolved", [])}
    if errors or p.returncode or not results:
        out["verdict"] = "error"
        out["error"] = (errors[0]["error"] if errors else p.stderr[-2000:]) or f"runner exit {p.returncode}"
        return out
    compared = counts.get("pass", 0) + counts.get("fail", 0)
    if fails:
        out["verdict"] = "fail"
        first = fails[0]
        out["first_mismatch"] = first
        out["first_mismatch_input"] = case_input(cpath, first["case"])
    elif compared == 0:
        out["verdict"] = "unsupported"
        out["reason"] = "every case was discarded (original crashed/exited abnormally)" + (f"; {aborted}" if aborted else "")
    else:
        out["verdict"] = "pass"
    notes += [r for r in results if r["status"] == "pass" and "literal" in r and r not in notes]
    if notes:
        out["notes"] = {"import_trace_differs": sum(1 for r in notes if r.get("what") == "imports"),
                        "x87_excess_precision": sum(1 for r in notes if "note" in r),
                        "constant_literal_pointer": sum(1 for r in notes if "literal" in r),
                        "example": next((r for r in notes if r.get("what") == "imports"), notes[0])}
    if not keep:
        cpath.unlink(missing_ok=True)
    return out


def case_input(cpath: Path, case: int) -> list[str]:
    out, on = [], False
    with open(cpath) as f:
        for line in f:
            if line.startswith("case "):
                on = int(line.split()[1]) == case
                continue
            if on:
                line = line.rstrip("\n")
                if line == "go":
                    break
                parts = line.split()
                if parts[0] == "obj" and parts[3] != "-":
                    data = bytes.fromhex(parts[3])
                    txt = data[:160].decode("latin-1").encode("unicode_escape").decode()
                    line = f"obj {parts[1]} size=0x{parts[2]} len={len(data)} \"{txt}{'...' if len(data) > 160 else ''}\""
                out.append(line)
    return out


def chunk_functions(chunk: int) -> list[int]:
    _, _, per_chunk = load_symbol_records()
    return [int(f["addr"], 16) for f in per_chunk.get(chunk, {}).get("functions", [])]


def batch(chunk: int, cases: int, seed: int, jobs: int, funcs: list[int] | None = None) -> dict:
    meta = build(chunk)
    addrs = funcs or chunk_functions(chunk)
    results = []
    with cf.ThreadPoolExecutor(max_workers=min(jobs, 4)) as ex:
        futs = {ex.submit(test_function, meta, chunk, a, cases, seed + a): a for a in addrs}
        for fut in cf.as_completed(futs):
            try:
                results.append(fut.result())
            except Exception as e:  # noqa: BLE001
                results.append({"addr": f"0x{futs[fut]:08x}", "verdict": "error", "error": str(e)[-2000:]})
    results.sort(key=lambda r: r["addr"])
    summary: dict[str, int] = {}
    for r in results:
        summary[r["verdict"]] = summary.get(r["verdict"], 0) + 1
    report = {"chunk": f"{chunk:03d}", "source_sha256": hashlib.sha256(Path(meta["source"]).read_bytes()).hexdigest(),
              "original_sha256": EXPECT_SHA, "cases_per_function": cases, "seed": seed, "cflags": meta["cflags"],
              "summary": summary, "build_warnings": meta["warnings"], "traps": meta["traps"], "functions": results}
    out = WORK / "results" / f"chunk_{chunk:03d}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=1))
    report["written"] = str(out)
    return report


# ----------------------------------------------------------------------------- self-test
SELFTEST_MUTATIONS = [
    # (chunk, function addr, old text, new text, description)
    (30, 0x080B7AE2, "c1 = c1 - 0x20;", "c1 = c1 - 0x21;", "Q_stricmpn: wrong case fold"),
    (30, 0x080B8318, None, None, "Info_RemoveKey: unchanged control (must pass)"),
    (30, 0x080B85CE, "if (curLen + newLen < 0x401)", "if (curLen + newLen < 0x400)",
     "Info_SetValueForKey: off-by-one length limit"),
    (30, 0x080B7AB6, None, None, "Q_strncpyz: unchanged control (must pass)"),
    (30, 0x080B7FA6, "next + *(int *)(block + 0x800) * -2", "next + *(int *)(block + 0x800) * -2 + 0",
     "va: no-op source change (must pass)"),
    (30, 0x080B7FA6, "buf = block + *(int *)(block + 0x800) * 0x400;", "buf = block + *(int *)(block + 0x800) * 0x3ff;",
     "va: wrong buffer stride (global .bss write)"),
    (30, 0x080B85CE, "Info string length exceeded\\n", "Info string length exceeded!\\n",
     "Info_SetValueForKey: changed console message (stdout)"),
    (30, 0x080B605C, "CurrentSession(st)->spaceDelimited = (enable != 0);",
     "CurrentSession(st)->spaceDelimited = (unsigned char)enable;", "Com_SetSpaceDelimited: bool normalisation dropped"),
]


def selftest(cases: int, seed: int) -> dict:
    src = CHUNKS / "chunk_030.cpp"
    text = src.read_text()
    mdir = WORK / "selftest"
    mdir.mkdir(parents=True, exist_ok=True)
    outcome = []
    ok = True
    for chunk, addr, old, new, desc in SELFTEST_MUTATIONS:
        expect = "pass" if "must pass" in desc else "fail"
        meta = build(chunk) if old is None else None
        if meta is None or expect == "fail":
            if old not in text:
                outcome.append({"mutation": desc, "result": "skipped: mutation anchor not found in source"})
                continue
            mut = mdir / "chunk_030.cpp"
            mut.write_text(text.replace(old, new, 1))
            meta = build(chunk, source=mut, tag="_mut")
        r = test_function(meta, chunk, addr, cases, seed)
        good = r["verdict"] == expect
        ok &= good
        outcome.append({"mutation": desc, "expected": expect, "verdict": r["verdict"], "ok": good,
                        "first_mismatch": r.get("first_mismatch"), "counts": r.get("counts")})
    return {"selftest": "pass" if ok else "fail", "results": outcome}


NONDETERMINISTIC = {"Sys_Milliseconds", "Sys_MillisecondsRaw", "Sys_EnterCriticalSection", "Sys_LeaveCriticalSection"}


def classify_failure(f: dict) -> str:
    """Heuristic triage of a first mismatch (inferred, not proof)."""
    fm = f.get("first_mismatch") or {}
    what = fm.get("what")
    if f.get("name") in NONDETERMINISTIC:
        return "nondeterministic (time/thread id)"
    if what == "outcome" and "unresolved" in str(fm.get("recon")):
        return "build: unresolved reconstruction symbol"
    if what == "outcome":
        return "recon crash/exit"
    if what == "memory" and fm.get("orig_byte") and fm.get("recon_byte"):
        a, b = int(fm["orig_byte"], 16), int(fm["recon_byte"], 16)
        if a ^ b == 0x40 and fm.get("diff_bytes", 0) <= 2:
            return "float: signalling NaN quieted by x87 load/store"
        if abs(a - b) == 1 and fm.get("diff_bytes", 0) <= 8:
            return "float: last-bit rounding (x87 precision/spill)"
    if what == "return" and f.get("signature", "").split()[0] in ("float", "double"):
        try:
            if abs(int(fm["orig"], 16) - int(fm["recon"], 16)) <= 2:
                return "float: last-bit rounding (x87 precision/spill)"
        except (KeyError, ValueError):
            pass
    return "logic/layout (inspect)"


def report() -> str:
    """Markdown summary of every .local/cod2/harness/results/chunk_NNN.json."""
    rows, fails = [], []
    tot: dict[str, int] = {}
    for path in sorted((WORK / "results").glob("chunk_[0-9][0-9][0-9].json")):
        d = json.loads(path.read_text())
        sm = d["summary"]
        for k, v in sm.items():
            tot[k] = tot.get(k, 0) + v
        rows.append(f"| {d['chunk']} | {sm.get('pass', 0)} | {sm.get('fail', 0)} | {sm.get('unsupported', 0)} | "
                    f"{sm.get('error', 0)} | {d['source_sha256'][:12]} |")
        for f in d["functions"]:
            if f["verdict"] in ("fail", "error"):
                fm = f.get("first_mismatch") or {}
                detail = {k: v for k, v in fm.items() if k in ("what", "orig", "recon", "addr", "region", "orig_byte",
                                                               "recon_byte", "orig_str", "recon_str", "diff_bytes")}
                c = f.get("counts", {})
                fails.append(f"| {d['chunk']} | `{f['addr']}` | {f.get('name')} | {classify_failure(f)} | "
                             f"{c.get('fail', 0)}/{c.get('pass', 0) + c.get('fail', 0)} | "
                             f"`{json.dumps(detail or f.get('error', ''))[:220].replace('|', '/')}` |")
    out = ["| chunk | pass | fail | unsupported | error | source sha256 |", "|---|---|---|---|---|---|", *rows,
           f"| **total** | {tot.get('pass', 0)} | {tot.get('fail', 0)} | {tot.get('unsupported', 0)} | "
           f"{tot.get('error', 0)} | |", "",
           "| chunk | addr | name | triage (inferred) | failing/compared | first mismatch |", "|---|---|---|---|---|---|", *fails]
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--chunk", type=lambda s: int(s, 10))
    ap.add_argument("--func", action="append", help="function address (repeatable)")
    ap.add_argument("--all", action="store_true", help="batch over every function of the chunk")
    ap.add_argument("--cases", type=int, default=500)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--source", type=Path, help="test this .cpp instead of recon/chunks/chunk_NNN.cpp")
    ap.add_argument("--keep", action="store_true", help="keep generated case files")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--build-only", action="store_true")
    ap.add_argument("--cflags", default="", help="extra reconstruction compile flags (e.g. '-O0 -ffloat-store')")
    ap.add_argument("--report", action="store_true", help="markdown table of all batch results")
    a = ap.parse_args()
    if a.report:
        print(report())
        return 0
    if a.selftest:
        r = selftest(a.cases, a.seed)
        print(json.dumps(r, indent=1))
        return 0 if r["selftest"] == "pass" else 1
    if a.chunk is None:
        ap.error("--chunk is required")
    if a.build_only:
        print(json.dumps(build(a.chunk, source=a.source, tag="_src" if a.source else ""), indent=1))
        return 0
    if a.all:
        funcs = [int(x, 16) for x in a.func] if a.func else None
        r = batch(a.chunk, a.cases, a.seed, a.jobs, funcs)
        print(json.dumps({k: r[k] for k in ("chunk", "summary", "written")}, indent=1))
        for f in r["functions"]:
            line = f"{f['addr']} {str(f.get('name')):32} {f['verdict']}"
            if f["verdict"] == "fail":
                fm = f["first_mismatch"]
                line += f"  [{fm.get('what')}] " + json.dumps({k: v for k, v in fm.items() if k not in ('case', 'status')})[:200]
            elif f["verdict"] in ("unsupported", "error"):
                line += "  " + str(f.get("reason") or f.get("error"))[:160]
            print(line)
        return 0
    if not a.func:
        ap.error("--func or --all is required")
    extra = a.cflags.split()
    tag = ("_src" if a.source else "") + ("_cf" + hashlib.sha1(a.cflags.encode()).hexdigest()[:6] if extra else "")
    meta = build(a.chunk, source=a.source, tag=tag, extra_cflags=extra)
    rc = 0
    for fa in a.func:
        r = test_function(meta, a.chunk, int(fa, 16), a.cases, a.seed, a.keep)
        print(json.dumps(r, indent=1))
        rc |= r["verdict"] not in ("pass",)
    return rc


if __name__ == "__main__":
    sys.exit(main())
