#!/usr/bin/env python3
"""Generate the original data image of cod2_lnxded 1.3 as a linkable assembler file.

Writes (deterministically):
  $COD2_LOCAL/globals/cod2_data.S (.local/cod2/globals)      .rodata/.data blobs, .bss zero blob,
                                                      one global alias per original global,
                                                      .init_array entries for the 4 .ctors
  $COD2_LOCAL/globals/cod2_aliases.ld  linker assignments for data-declared
                                                      names that denote reconstructed functions
  $COD2_LOCAL/globals/symbol_map.json  {addr: {name, aliases, size, section, ...}}
  .local/cod2/build/globals-report.json               pointer-rewrite statistics, unmapped
                                                      words, ambiguities, conflicts

The original image has no relocations, so every 32-bit word in .rodata/.data whose value
falls in the image is classified (function start, switch-table target, data pointer,
false positive) and pointers are re-expressed symbolically. See
analysis/global-data-image.md.

Inputs: the original binary (.local/cod2/lnxded-1.3/cod2_lnxded, SHA-256 checked), the
chunk sources/headers, analysis/symbols/*.json, and the chunk objects compiled by
link_test.py's flags (compiled here when missing or stale). Standard library + the GNU
toolchain (g++ -m32) only; the ELF files are parsed here.

  python3 games/call-of-duty-2/tools/gen_globals.py [--jobs 6] [--no-probe]
"""

from __future__ import annotations

import argparse
import bisect
import concurrent.futures as cf
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GAME = HERE.parent
REPO = GAME.parents[1]
RECON = GAME / "recon"
CHUNKS = RECON / "chunks"
INCLUDE = RECON / "include"
SYMBOLS = GAME / "analysis" / "symbols"
LOCAL = Path(os.environ.get("COD2_LOCAL", REPO / ".local" / "cod2"))
# The data image holds byte copies of the original .rodata/.data, so it is generated output kept out of tracked
# source (repository rule: converted original data lives under .local/).
OUTDIR = Path(os.environ.get("COD2_GLOBALS", LOCAL / "globals"))
ORIGINAL = LOCAL / "lnxded-1.3" / "cod2_lnxded"
BUILD = Path(os.environ.get("COD2_BUILD", LOCAL / "build"))
OBJDIR = BUILD / "obj"
PROBEDIR = BUILD / "probe"
# compiler/linker temporaries go to the build tree (the host /tmp may be a small tmpfs)
os.environ.setdefault("COD2_BUILD_TMP", str(BUILD / "tmp"))
Path(os.environ["COD2_BUILD_TMP"]).mkdir(parents=True, exist_ok=True)
os.environ["TMPDIR"] = os.environ["COD2_BUILD_TMP"]
EXPECT_SHA = "05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7"

CXXFLAGS = ["-m32", "-std=gnu++98", "-w", "-fno-builtin", "-O0", "-fno-pie"]
LIB_DIRS = ["/usr/lib32", "/usr/lib/i386-linux-gnu", "/lib32", "/lib/i386-linux-gnu"]
SYSTEM_LIBS = ["libc.so.6", "libm.so.6", "libdl.so.2", "libpthread.so.0", "libstdc++.so.6", "libgcc_s.so.1"]

# Sections whose contents are emitted (name -> blob label).
BLOBS = {".rodata": "cod2_rodata", ".data": "cod2_data", ".bss": "cod2_bss"}
ADDR_RE = re.compile(r"(?:^|_)(08[0-9a-f]{6})(?=_|$)")


# ----------------------------------------------------------------------------- ELF32
class Elf32:
    """Minimal little-endian ELF32 reader (sections, symbols, relocations)."""

    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        d = self.data
        if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
            raise ValueError(f"{path}: not an ELF32 little-endian file")
        (self.e_type, _, _, self.entry, _, shoff, _, _, _, _, shentsize, shnum, shstrndx) = struct.unpack_from(
            "<HHIIIIIHHHHHH", d, 16)
        raw = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
        strtab = raw[shstrndx]
        self.sections = []
        for i, (name, typ, flags, addr, off, size, link, info, align, entsize) in enumerate(raw):
            self.sections.append({
                "index": i, "name": self._str(strtab[4], name), "type": typ, "flags": flags, "addr": addr,
                "offset": off, "size": size, "link": link, "info": info, "align": align, "entsize": entsize})
        self.by_name = {s["name"]: s for s in self.sections}

    def _str(self, off: int, idx: int) -> str:
        end = self.data.index(b"\0", off + idx)
        return self.data[off + idx:end].decode("latin-1")

    def section_bytes(self, name: str) -> bytes:
        s = self.by_name[name]
        return self.data[s["offset"]:s["offset"] + s["size"]]

    def symbols(self, secname: str) -> list[dict]:
        s = self.by_name.get(secname)
        if not s:
            return []
        strsec = self.sections[s["link"]]
        out = []
        for i in range(s["size"] // 16):
            name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", self.data, s["offset"] + 16 * i)
            out.append({"name": self._str(strsec["offset"], name), "value": value, "size": size,
                        "bind": info >> 4, "type": info & 15, "shndx": shndx, "index": i})
        return out

    def relocations(self) -> list[tuple[str, int, int, int]]:
        """(target section name, offset, type, symbol index) for every SHT_REL section."""
        out = []
        for s in self.sections:
            if s["type"] != 9:
                continue
            target = self.sections[s["info"]]["name"] if s["info"] else ""
            for i in range(s["size"] // 8):
                off, info = struct.unpack_from("<II", self.data, s["offset"] + 8 * i)
                out.append((target, off, info & 0xFF, info >> 8))
        return out


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


# ----------------------------------------------------------------------------- names
def mangled_base(sym: str) -> str:
    """Base name of a symbol: 'Foo' for Foo/_Z3Foov/_ZL3Foov, 'A::B' for _ZN1A1BEv (no c++filt)."""
    if not sym.startswith("_Z"):
        return sym
    s = sym[2:]
    if s.startswith("L"):
        s = s[1:]
    parts: list[str] = []

    def ident(t: str) -> tuple[str, str] | None:
        m = re.match(r"(\d+)", t)
        if not m:
            return None
        n = int(m.group(1))
        start = len(m.group(1))
        return t[start:start + n], t[start + n:]

    if s.startswith("N"):
        s = s[1:]
        while s and s[0] in "rVK":
            s = s[1:]
        while s and s[0] != "E":
            if s[0].isdigit():
                r = ident(s)
                if not r:
                    break
                parts.append(r[0])
                s = r[1]
            elif s[:2] in ("C1", "C2", "C3", "D0", "D1", "D2"):
                parts.append(("~" if s[0] == "D" else "") + (parts[-1] if parts else "?"))
                s = s[2:]
            elif s.startswith("St"):
                parts.append("std")
                s = s[2:]
            else:
                return sym  # operators, templates, substitutions: leave as is
        return "::".join(parts)
    r = ident(s)
    return r[0] if r else sym


def provenance(cpp: Path) -> list[tuple[int, set[str]]]:
    """[(addr, {candidate names})] from '// 0x080abcde  size=N  Name ...' comments."""
    out = []
    pend = None
    for line in cpp.read_text(errors="replace").splitlines():
        m = re.match(r"\s*//\s*0x([0-9a-fA-F]{8})\s+size=\S+\s+(\S+)", line)
        if m:
            if pend:
                out.append(pend)
            pend = (int(m.group(1), 16), {m.group(2)})
            continue
        if pend is None or line.strip().startswith("//") or not line.strip():
            continue
        m = re.search(r"([A-Za-z_][\w:~]*)\s*\(", line)
        if m and m.group(1) not in ("__asm__", "asm", "COD2_ASSERT_SIZE", "COD2_ASSERT_OFFSET"):
            pend[1].add(m.group(1))
        out.append(pend)
        pend = None
    if pend:
        out.append(pend)
    return out


def extern_comment_addrs(paths: list[Path]) -> dict[str, set[int]]:
    """name -> {addr} from 'extern T name[..];  /* 0x08xxxxxx ... */' declarations."""
    out: dict[str, set[int]] = {}
    pat = re.compile(r"^\s*extern\b(?!\s*\"C\"\s*\{)[^;(]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*;\s*(?://|/\*)\s*(?:@\s*)?0x(08[0-9a-fA-F]{6})\b")
    for p in paths:
        for line in p.read_text(errors="replace").splitlines():
            m = pat.match(line)
            if m:
                out.setdefault(m.group(1), set()).add(int(m.group(2), 16))
    return out


# ----------------------------------------------------------------------------- objects
def chunk_sources() -> list[Path]:
    return sorted(CHUNKS.glob("chunk_[0-9][0-9][0-9].cpp"))


def headers() -> list[Path]:
    return sorted(INCLUDE.glob("*.h")) + sorted(CHUNKS.glob("*.h"))


def compile_one(src: Path, objdir: Path) -> tuple[str, bool, str]:
    obj = objdir / (src.stem + ".o")
    cmd = ["nice", "-n", "10", "g++", *CXXFLAGS, "-c", "-I", str(INCLUDE), "-I", str(CHUNKS), str(src), "-o", str(obj)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return src.stem, r.returncode == 0, r.stderr[-4000:]


def compile_chunks(objdir: Path = OBJDIR, jobs: int = 6, force: bool = False) -> dict:
    """Compile every chunk (parallel, nice). Skips objects newer than their inputs."""
    objdir.mkdir(parents=True, exist_ok=True)
    hdr_m = max((p.stat().st_mtime for p in headers()), default=0)
    stamp = objdir / ".flags"
    flags = " ".join(CXXFLAGS)
    if not stamp.exists() or stamp.read_text() != flags:
        force = True
    todo = []
    for src in chunk_sources():
        obj = objdir / (src.stem + ".o")
        if force or not obj.exists() or obj.stat().st_mtime < max(src.stat().st_mtime, hdr_m):
            todo.append(src)
    results = {"compiled": [], "failed": {}}
    with cf.ThreadPoolExecutor(max_workers=max(1, min(jobs, 6))) as ex:
        for stem, ok, err in ex.map(lambda s: compile_one(s, objdir), todo):
            if ok:
                results["compiled"].append(stem)
            else:
                results["failed"][stem] = err
                (objdir / (stem + ".o")).unlink(missing_ok=True)
    stamp.write_text(flags)
    return results


def scan_objects(objdir: Path) -> dict:
    """Defined/undefined symbols and relocation kinds of every chunk object."""
    defined: dict[str, list[tuple[str, str]]] = {}   # name -> [(chunk, kind)]
    undefined: dict[str, set[str]] = {}               # name -> {chunk}
    reloc_kinds: dict[str, set[int]] = {}             # name -> {R_386_*}
    text_syms: dict[str, list[str]] = {}              # chunk -> [defined function symbols]
    local_funcs: dict[str, dict] = {}                 # chunk -> {static function: (section, value)}
    for obj in sorted(objdir.glob("chunk_*.o")):
        chunk = obj.stem
        e = Elf32(obj)
        syms = e.symbols(".symtab")
        for s in syms:
            if not s["name"] or s["type"] in (3, 4):  # SECTION, FILE
                continue
            if s["shndx"] == 0:
                undefined.setdefault(s["name"], set()).add(chunk)
                continue
            sec = e.sections[s["shndx"]]["name"] if s["shndx"] < len(e.sections) else "*ABS*"
            local = s["bind"] == 0
            kind = ("func" if s["type"] == 2 else "object") + ("-local" if local else "") + ("-weak" if s["bind"] == 2 else "")
            if s["type"] == 2:
                text_syms.setdefault(chunk, []).append(s["name"])
                if local:
                    local_funcs.setdefault(chunk, {})[s["name"]] = (sec, s["value"])
            if not local:
                defined.setdefault(s["name"], []).append((chunk, kind + ":" + sec))
            elif s["type"] == 1:
                defined.setdefault(s["name"], []).append((chunk, kind + ":" + sec))
        for _sec, _off, typ, idx in e.relocations():
            if 0 < idx < len(syms) and syms[idx]["shndx"] == 0:
                reloc_kinds.setdefault(syms[idx]["name"], set()).add(typ)
    return {"defined": defined, "undefined": undefined, "reloc_kinds": reloc_kinds, "text_syms": text_syms,
            "local_funcs": local_funcs}


def find_lib(name: str) -> Path | None:
    for d in LIB_DIRS:
        p = Path(d) / name
        if p.exists():
            return p.resolve()
    return None


def system_symbols() -> tuple[set[str], list[str]]:
    names: set[str] = set()
    missing = []
    for lib in SYSTEM_LIBS:
        p = find_lib(lib)
        if not p:
            missing.append(lib)
            continue
        e = Elf32(p)
        for s in e.symbols(".dynsym"):
            if s["shndx"] != 0 and s["name"]:
                names.add(s["name"])
    return names, missing


# ----------------------------------------------------------------------------- size probes
def probe_chunk(chunk: str, names: list[str]) -> dict[str, int]:
    """sizeof(name) as declared in the chunk (complete types only)."""
    PROBEDIR.mkdir(parents=True, exist_ok=True)
    src = PROBEDIR / f"{chunk}_probe.cpp"
    active = dict(enumerate(names))
    sizes: dict[str, int] = {}
    for _attempt in range(4):
        if not active:
            break
        lines = [f'#include "{CHUNKS / (chunk + ".cpp")}"', "#line 1 \"cod2_probe\""]
        order = []
        for i, n in sorted(active.items()):
            order.append(i)
            lines.append(f'extern "C" const unsigned cod2probe_{i} = sizeof({n});')
        src.write_text("\n".join(lines) + "\n")
        r = subprocess.run(["nice", "-n", "10", "g++", *CXXFLAGS, "-S", "-o", "-", "-I", str(INCLUDE), "-I",
                            str(CHUNKS), str(src)], capture_output=True, text=True)
        if r.returncode == 0:
            for m in re.finditer(r"cod2probe_(\d+):\s*\n\s*\.long\s+(\d+)", r.stdout):
                sizes[active[int(m.group(1))]] = int(m.group(2))
            break
        bad = {int(m.group(1)) for m in re.finditer(r"^cod2_probe:(\d+):", r.stderr, re.M)}
        if not bad:
            break
        for line_no in bad:
            if 1 <= line_no <= len(order):
                active.pop(order[line_no - 1], None)
    return sizes


# ----------------------------------------------------------------------------- main analysis
def load_symbol_records() -> dict:
    merged = json.loads((SYMBOLS / "symbols.json").read_text())
    glob_names: dict[str, set[int]] = {}
    glob_info: dict[int, dict] = {}
    for a, v in merged.get("globals", {}).items():
        glob_names.setdefault(v["name"], set()).add(int(a, 16))
        glob_info[int(a, 16)] = v
    chunk_globs: dict[str, set[int]] = {}
    for p in sorted(SYMBOLS.glob("chunk_[0-9][0-9][0-9].json")):
        try:
            d = json.loads(p.read_text())
        except (OSError, json.JSONDecodeError):
            continue
        for g in d.get("globals", []):
            try:
                chunk_globs.setdefault(g["name"], set()).add(int(g["addr"], 16))
            except (KeyError, ValueError):
                pass
    funcs = {int(a, 16): v for a, v in merged.get("functions", {}).items()}
    return {"glob_names": glob_names, "glob_info": glob_info, "chunk_globs": chunk_globs, "funcs": funcs}


def section_of(elf: Elf32, addr: int) -> str | None:
    for s in elf.sections:
        if s["flags"] & 2 and s["addr"] <= addr < s["addr"] + s["size"]:
            return s["name"]
    return None


def jump_tables(elf: Elf32, fstarts: list[int]) -> dict[int, tuple[int, int]]:
    """table base -> (jumping function start, function end).

    GCC 3.3 dispatches switches as `mov TABLE(,%reg,4),%reg; jmp *%reg`, `mov TABLE(%reg),%reg;
    jmp *%reg` or `jmp *TABLE(,%reg,4)`. Bases are the .rodata disp32 operands found within the
    12 bytes before an indirect `jmp *%reg`, or of a `jmp *disp32(,%reg,4)`.
    """
    t = elf.by_name[".text"]
    ro = elf.by_name[".rodata"]
    code = elf.section_bytes(".text")
    out = {}

    def add(base: int, site: int) -> None:
        if not ro["addr"] <= base < ro["addr"] + ro["size"]:
            return
        i = bisect.bisect_right(fstarts, site) - 1
        if i < 0:
            return
        fend = fstarts[i + 1] if i + 1 < len(fstarts) else t["addr"] + t["size"]
        out.setdefault(base, (fstarts[i], fend))

    for m in re.finditer(rb"\xff\x24[\x85\x8d\x95\x9d\xad\xb5\xbd]", code):
        if m.end() + 4 <= len(code):
            add(struct.unpack_from("<I", code, m.end())[0], t["addr"] + m.start())
    for m in re.finditer(rb"\xff[\xe0-\xe7]", code):
        p = m.start()
        for k in range(4, 13):
            if p - k >= 0:
                add(struct.unpack_from("<I", code, p - k)[0], t["addr"] + p)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--objdir", type=Path, default=OBJDIR)
    ap.add_argument("--no-probe", action="store_true", help="skip sizeof() probes (sizes from layout only)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    summary = generate(args.objdir, args.jobs, probe=not args.no_probe)
    if not args.quiet:
        print(json.dumps(summary, indent=1))
    return 0


def generate(objdir: Path = OBJDIR, jobs: int = 6, probe: bool = True) -> dict:
    if sha256(ORIGINAL) != EXPECT_SHA:
        raise SystemExit(f"{ORIGINAL}: SHA-256 mismatch")
    elf = Elf32(ORIGINAL)
    comp = compile_chunks(objdir, jobs)
    objs = scan_objects(objdir)
    recs = load_symbol_records()
    sysnames, missing_libs = system_symbols()
    secs = {n: elf.by_name[n] for n in (".text", ".plt", ".rodata", ".data", ".bss", ".ctors", ".dtors", ".jcr",
                                         ".got", ".got.plt", ".dynamic", ".init", ".fini")}

    def in_sec(name: str, a: int, inclusive_end: bool = False) -> bool:
        s = secs[name]
        return s["addr"] <= a < s["addr"] + s["size"] + (1 if inclusive_end else 0)

    # ---- original imports: PLT entries and COPY-relocated objects
    dynsym = elf.symbols(".dynsym")
    plt_names: dict[int, str] = {}
    copy_objs: list[tuple[int, int, str]] = []
    for target, off, typ, idx in elf.relocations():
        pass
    relplt = elf.by_name[".rel.plt"]
    for i in range(relplt["size"] // 8):
        _off, info = struct.unpack_from("<II", elf.data, relplt["offset"] + 8 * i)
        plt_names[secs[".plt"]["addr"] + 16 * (i + 1)] = dynsym[info >> 8]["name"]
    reldyn = elf.by_name[".rel.dyn"]
    for i in range(reldyn["size"] // 8):
        off, info = struct.unpack_from("<II", elf.data, reldyn["offset"] + 8 * i)
        if info & 0xFF == 5:  # R_386_COPY
            s = dynsym[info >> 8]
            copy_objs.append((off, s["size"], s["name"]))

    # ---- reconstructed functions: original address -> link symbol
    fstarts = sorted(set(recs["funcs"]) | set(plt_names))
    recon_sym: dict[int, str] = {}
    recon_ambiguous: dict[str, list] = {}
    static_aliases: dict[str, dict] = {}
    for src in chunk_sources():
        chunk = src.stem
        by_base: dict[str, list[str]] = {}
        for sym in objs["text_syms"].get(chunk, []):
            b = mangled_base(sym)
            by_base.setdefault(b, []).append(sym)
            by_base.setdefault(b.split("::")[-1], []).append(sym) if "::" in b else None
        for addr, cands in provenance(src):
            hits = sorted({s for c in cands for s in by_base.get(c, []) + by_base.get(c.split("::")[-1], [])})
            # prefer exact base-name hits over last-component hits
            exact = sorted({s for c in cands for s in by_base.get(c, [])})
            hits = exact or hits
            # constructors/destructors: C1/D1 (complete object) are the callable entry points
            if len(hits) > 1:
                pref = [h for h in hits if re.search(r"(C1|D1)E", h)]
                hits = pref if len(pref) == 1 else hits
            if len(hits) == 1 and hits[0] in objs["local_funcs"].get(chunk, {}):
                # static function: give it a global alias the data image can name; link_test.py
                # adds it with objcopy --add-symbol (see static_aliases.json)
                alias = f"cod2_static__{chunk}__{hits[0]}"
                sec, val = objs["local_funcs"][chunk][hits[0]]
                static_aliases.setdefault(chunk, {})[alias] = {"symbol": hits[0], "section": sec, "value": val}
                recon_sym.setdefault(addr, alias)
            elif len(hits) == 1:
                recon_sym.setdefault(addr, hits[0])
            elif hits:
                recon_ambiguous[f"0x{addr:08x}"] = hits
    defined_globally = {n for n, ds in objs["defined"].items() if any("-local" not in k for _c, k in ds)}

    # ---- references: undefined names in objects, classified by relocation kind
    undefined = {n: c for n, c in objs["undefined"].items() if n not in defined_globally}
    called = {n for n, k in objs["reloc_kinds"].items() if 2 in k or 4 in k}  # R_386_PC32 / PLT32

    # ---- name -> address
    hdr_addrs = extern_comment_addrs(headers() + chunk_sources())
    name_addr: dict[str, int] = {}
    name_via: dict[str, str] = {}
    conflicts: list[dict] = []

    def resolve(n: str) -> tuple[int | None, str]:
        cands = []
        m = ADDR_RE.search(n)
        if m and not n.startswith("_Z"):
            cands.append((int(m.group(1), 16), "address in name"))
        for a in sorted(recs["glob_names"].get(n, ())):
            cands.append((a, "symbols.json global"))
        for a in sorted(recs["chunk_globs"].get(n, ())):
            cands.append((a, "chunk json global"))
        for a in sorted(hdr_addrs.get(n, ())):
            cands.append((a, "extern declaration comment"))
        if not cands:
            return None, ""
        addrs = sorted({a for a, _ in cands})
        if len(addrs) > 1:
            conflicts.append({"name": n, "candidates": [{"addr": f"0x{a:08x}", "via": v} for a, v in cands],
                              "chosen": f"0x{cands[0][0]:08x}"})
        return cands[0]

    all_names = set(recs["glob_names"]) | set(recs["chunk_globs"]) | set(hdr_addrs) | set(undefined)
    for n in sorted(all_names):
        if n in sysnames and n not in ("stdin", "stdout", "stderr"):
            # never interpose a libc/libstdc++ definition from the data image
            if n not in undefined:
                continue
        a, via = resolve(n)
        if a is not None:
            name_addr[n] = a
            name_via[n] = via

    # ---- classify every name
    data_aliases: dict[str, int] = {}   # emitted in the .S
    func_aliases: dict[str, str] = {}   # emitted in the linker script
    skipped: dict[str, str] = {}
    for n, a in sorted(name_addr.items()):
        sec = section_of(elf, a)
        if n in defined_globally:
            if sec in BLOBS:
                skipped[n] = "defined by a chunk object (not shared with the original layout): " + \
                    ",".join(sorted({c for c, _ in objs["defined"][n]}))
            continue
        if any(o <= a < o + sz for o, sz, _ in copy_objs):
            skipped[n] = "COPY-relocated library object; the library definition is used"
            continue
        if n in sysnames:
            skipped[n] = "provided by a system library"
            continue
        if sec in BLOBS:
            data_aliases[n] = a
        elif sec in (".text", ".plt", ".init", ".fini"):
            if n.startswith("_Z"):
                continue
            tgt = recon_sym.get(a) or plt_names.get(a)
            if n in called:
                continue  # a called function: resolved (or reported) by the function lanes
            if tgt and tgt != n:
                func_aliases[n] = tgt
            elif n in undefined:
                skipped[n] = f"function address 0x{a:08x} has no reconstructed symbol"
        elif sec in (".ctors", ".dtors", ".jcr"):
            data_aliases[n] = a  # served by the CRT-table image below
        else:
            skipped[n] = f"address 0x{a:08x} in {sec}: not part of the data image"

    # ---- pointer scan of .rodata/.data (and the .ctors/.dtors/.jcr image)
    jt = jump_tables(elf, fstarts)
    lo, hi = 0x08048000, secs[".bss"]["addr"] + secs[".bss"]["size"]
    func_set = set(recs["funcs"])
    stats = {"words_scanned": 0, "candidates": 0}
    from collections import Counter
    kinds: Counter = Counter()
    rewrites: dict[int, str] = {}       # word address -> assembler expression
    unmapped: list[dict] = []
    ambiguous: list[dict] = []
    sw_tables: dict[int, dict] = {}

    def blob_expr(v: int) -> str | None:
        for o, sz, name in copy_objs:
            if o <= v < o + sz:
                return name + (f"+{v - o}" if v != o else "")
        for name, label in BLOBS.items():
            s = secs[name]
            if s["addr"] <= v <= s["addr"] + s["size"]:
                return f"{label}+0x{v - s['addr']:x}"
        if secs[".ctors"]["addr"] <= v <= secs[".jcr"]["addr"] + secs[".jcr"]["size"]:
            return f"cod2_crt_tables+0x{v - secs['.ctors']['addr']:x}"
        return None

    def classify_word(waddr: int, v: int, sec: str, prev_ptr: bool, next_ptr: bool) -> None:
        stats["candidates"] += 1
        if in_sec(".plt", v):
            if v in plt_names:
                rewrites[waddr] = plt_names[v]
                kinds[f"{sec}->import (PLT)"] += 1
            else:
                unmapped.append({"at": f"0x{waddr:08x}", "value": f"0x{v:08x}", "why": "inside .plt, not an entry"})
            return
        if in_sec(".text", v) or in_sec(".init", v) or in_sec(".fini", v):
            if v in func_set or v in recon_sym:
                if v in recon_sym:
                    rewrites[waddr] = recon_sym[v]
                    kinds[f"{sec}->function"] += 1
                else:
                    kinds[f"{sec}->function (no reconstructed symbol)"] += 1
                    unmapped.append({"at": f"0x{waddr:08x}", "value": f"0x{v:08x}",
                                     "why": "function start without a reconstructed symbol",
                                     "function": recs["funcs"].get(v, {}).get("name")})
                return
            # switch table entry?
            for base, (fs, fe) in jt.items():
                if base <= waddr and fs <= v < fe and (waddr - base) % 4 == 0:
                    ok = all(fs <= struct.unpack_from("<I", elf.data, off_of(a2))[0] < fe
                             for a2 in range(base, waddr + 4, 4))
                    if ok:
                        t = sw_tables.setdefault(base, {"table": f"0x{base:08x}", "function": f"0x{fs:08x}",
                                                        "function_name": recs["funcs"].get(fs, {}).get("name"),
                                                        "entries": 0, "section": sec})
                        t["entries"] += 1
                        kinds[f"{sec}->switch-table target (raw)"] += 1
                        return
            i = bisect.bisect_right(fstarts, v) - 1
            fn = fstarts[i] if i >= 0 else None
            kind = "mid-function text address"
            if not prev_ptr and not next_ptr:
                kind += " (isolated; likely integer/byte data)"
            kinds[f"{sec}->text, not a function start (raw)"] += 1
            unmapped.append({"at": f"0x{waddr:08x}", "value": f"0x{v:08x}", "why": kind,
                             "containing_function": f"0x{fn:08x}" if fn else None,
                             "context": ctx(waddr)})
            return
        e = blob_expr(v)
        if e is None:
            kinds[f"{sec}->other section (raw)"] += 1
            unmapped.append({"at": f"0x{waddr:08x}", "value": f"0x{v:08x}", "why": f"points into {section_of(elf, v)}",
                             "context": ctx(waddr)})
            return
        rewrites[waddr] = e
        tsec = section_of(elf, v) or "end-of-section"
        kinds[f"{sec}->{tsec}"] += 1
        if not plausible_target(v):
            ambiguous.append({"at": f"0x{waddr:08x}", "value": f"0x{v:08x}", "target": e,
                              "why": "target is not a known symbol, not 4-aligned and not the start of a string", "context": ctx(waddr)})

    known_addrs = set(name_addr.values())

    def plausible_target(v: int) -> bool:
        """Known symbol, 4-aligned, end of a section, or start of a NUL-terminated string."""
        if v in known_addrs or v % 4 == 0 or blob_expr(v) is None or section_of(elf, v) in (None, ".bss"):
            return True
        try:
            o = off_of(v)
        except KeyError:
            return True
        if elf.data[o - 1] != 0:
            return False
        end = elf.data.find(b"\0", o)
        txt = elf.data[o:end]
        return len(txt) < 4096 and all(32 <= c < 127 or c in (9, 10, 13) for c in txt)

    def off_of(a: int) -> int:
        for s in elf.sections:
            if s["type"] == 1 and s["addr"] <= a < s["addr"] + s["size"]:
                return s["offset"] + a - s["addr"]
        raise KeyError(hex(a))

    def ctx(a: int) -> str:
        o = off_of(a)
        return elf.data[max(o - 8, 0):o + 12].hex()

    def is_ptr(v: int) -> bool:
        return lo <= v < hi

    for name in (".rodata", ".data", ".ctors", ".dtors", ".jcr"):
        s = secs[name]
        raw = elf.section_bytes(name)
        words = [struct.unpack_from("<I", raw, i)[0] for i in range(0, s["size"] - 3, 4)]
        for k, v in enumerate(words):
            stats["words_scanned"] += 1
            if not is_ptr(v):
                continue
            if name in (".ctors", ".dtors") and v in (0, 0xFFFFFFFF):
                continue
            classify_word(s["addr"] + 4 * k, v, name, k > 0 and is_ptr(words[k - 1]),
                          k + 1 < len(words) and is_ptr(words[k + 1]))

    # ---- C++ RTTI and vtables in .rodata (FX classes): typeinfo objects start with a pointer
    # into a COPY-relocated __cxxabiv1 vtable; a vtable is [offset-to-top][typeinfo][slots...]
    ro = secs[".rodata"]
    ro_raw = elf.section_bytes(".rodata")
    ro_words = {ro["addr"] + i: struct.unpack_from("<I", ro_raw, i)[0] for i in range(0, ro["size"] - 3, 4)}
    copy_vt = {o + 8: n for o, _sz, n in copy_objs if n.startswith("_ZTV")}
    typeinfos = {}
    for a, v in ro_words.items():
        if v in copy_vt:
            nm_ptr = ro_words.get(a + 4, 0)
            try:
                o = off_of(nm_ptr)
                tname = elf.data[o:elf.data.index(b"\0", o)].decode("latin-1")
            except (KeyError, ValueError):
                tname = "?"
            base = ro_words.get(a + 8) if copy_vt[v].endswith("20__si_class_type_infoE") else None
            typeinfos[a] = {"name": tname, "kind": copy_vt[v], "base_typeinfo": f"0x{base:08x}" if base else None}
    vtables = []
    for a, v in sorted(ro_words.items()):
        if v in typeinfos and ro_words.get(a - 4) == 0 and a not in typeinfos and (a - 8) not in typeinfos:
            slots = []
            k = a + 4
            while k in ro_words and (in_sec(".text", ro_words[k]) or in_sec(".plt", ro_words[k])):
                f = ro_words[k]
                slots.append({"slot": len(slots), "target": f"0x{f:08x}",
                              "original_name": recs["funcs"].get(f, {}).get("name") or plt_names.get(f),
                              "link_symbol": recon_sym.get(f) or plt_names.get(f)})
                k += 4
            names = [n for n, x in name_addr.items() if x == a + 4]
            vtables.append({"vtable": f"0x{a - 4:08x}", "address_point": f"0x{a + 4:08x}", "names": sorted(names),
                            "class": typeinfos[v]["name"], "typeinfo": f"0x{v:08x}", "slots": slots,
                            "unmapped_slots": sum(1 for x in slots if not x["link_symbol"])})

    # ---- sizes: declared sizeof() per referencing chunk, else distance to the next symbol
    by_addr: dict[int, list[str]] = {}
    for n, a in data_aliases.items():
        by_addr.setdefault(a, []).append(n)
    starts = sorted(by_addr)
    declared: dict[str, dict[str, int]] = {}
    if probe:
        per_chunk: dict[str, list[str]] = {}
        for n in sorted(data_aliases):
            for c in sorted(undefined.get(n, ())):
                per_chunk.setdefault(c, []).append(n)
        with cf.ThreadPoolExecutor(max_workers=max(1, min(jobs, 6))) as ex:
            futs = {c: ex.submit(probe_chunk, c, ns) for c, ns in sorted(per_chunk.items())}
            for c, f in sorted(futs.items()):
                for n, sz in f.result().items():
                    declared.setdefault(n, {})[c] = sz

    def sec_end(a: int) -> int:
        s = secs[section_of(elf, a)]
        return s["addr"] + s["size"]

    symbol_map: dict[str, dict] = {}
    size_conflicts = []
    overlaps = []
    for i, a in enumerate(starts):
        names = sorted(by_addr[a], key=lambda n: (n not in undefined, not ADDR_RE.search(n) is None, n))
        sec = section_of(elf, a)
        nxt = starts[i + 1] if i + 1 < len(starts) and section_of(elf, starts[i + 1]) == sec else sec_end(a)
        gap = nxt - a
        dsizes = sorted({sz for n in names for sz in declared.get(n, {}).values()})
        if dsizes:
            size, how = max(dsizes), "declared"
            if len(dsizes) > 1:
                size_conflicts.append({"addr": f"0x{a:08x}", "names": names,
                                       "declared": {n: declared.get(n, {}) for n in names if n in declared}})
            if size > gap and nxt != sec_end(a):
                overlaps.append({"addr": f"0x{a:08x}", "name": names[0], "size": size, "next_symbol": f"0x{nxt:08x}",
                                 "next_names": sorted(by_addr.get(nxt, []))})
            if a + size > sec_end(a):
                size, how = sec_end(a) - a, "declared, clipped to section end"
        else:
            size, how = gap, "gap to next symbol"
        info = recs["glob_info"].get(a, {})
        symbol_map[f"0x{a:08x}"] = {
            "name": names[0], "aliases": names[1:], "size": size, "size_source": how, "section": sec,
            "type": info.get("type"),
            "referenced_by_chunks": sorted({c for n in names for c in undefined.get(n, ())}),
            "via": sorted({name_via[n] for n in names}),
        }
    for n, tgt in sorted(func_aliases.items()):
        a = name_addr[n]
        symbol_map.setdefault(f"0x{a:08x}", {"name": n, "aliases": [], "size": 0, "size_source": "function",
                                             "section": section_of(elf, a), "type": "function address",
                                             "referenced_by_chunks": sorted(undefined.get(n, ())),
                                             "link_symbol": tgt, "via": [name_via[n]]})

    # ---- .ctors -> .init_array (original runs .ctors from the end towards the start)
    ctors = []
    raw = elf.section_bytes(".ctors")
    for k in range(1, len(raw) // 4 - 1):
        ctors.append(struct.unpack_from("<I", raw, 4 * k)[0])
    ctor_syms = []
    ctor_missing = []
    for v in reversed(ctors):
        if v in recon_sym:
            ctor_syms.append((v, recon_sym[v]))
        else:
            ctor_missing.append(f"0x{v:08x}")

    # ---- emit
    OUTDIR.mkdir(parents=True, exist_ok=True)
    out = []
    w = out.append
    w("/* Generated by games/call-of-duty-2/tools/gen_globals.py -- do not edit.")
    w(f" * Original data image of cod2_lnxded 1.3 (SHA-256 {EXPECT_SHA}).")
    w(" * .rodata/.data are byte-exact copies of the original sections; 32-bit words that were")
    w(" * absolute pointers are re-expressed as symbols (see analysis/global-data-image.md).")
    w(" * .bss is a zero blob of the original size. Every original global is an alias at its")
    w(" * original offset from the blob base. */")
    w("\t.file\t\"cod2_data.S\"")

    def emit_blob(name: str, label: str, section_directive: str, align: int) -> None:
        s = secs[name]
        w("")
        w(f"\t{section_directive}")
        w(f"\t.balign\t{align}")
        w(f"\t.globl\t{label}")
        w(f"\t.type\t{label}, @object")
        w(f"{label}:\t/* original 0x{s['addr']:08x}..0x{s['addr'] + s['size']:08x} */")
        if s["type"] == 8:  # NOBITS
            w(f"\t.zero\t0x{s['size']:x}")
        else:
            raw = elf.section_bytes(name)
            i = 0
            pending: list[int] = []

            def flush() -> None:
                for j in range(0, len(pending), 16):
                    w("\t.byte\t" + ",".join(f"0x{b:02x}" for b in pending[j:j + 16]))
                pending.clear()

            while i < len(raw):
                a = s["addr"] + i
                if a in rewrites and i + 4 <= len(raw):
                    flush()
                    w(f"\t.long\t{rewrites[a]}\t/* 0x{a:08x}: was 0x{struct.unpack_from('<I', raw, i)[0]:08x} */")
                    i += 4
                    continue
                if i % 0x100 == 0 and pending:
                    flush()
                if i % 0x100 == 0:
                    w(f"/* 0x{a:08x} */")
                pending.append(raw[i])
                i += 1
            flush()
        w(f"\t.size\t{label}, 0x{s['size']:x}")

    emit_blob(".rodata", "cod2_rodata", ".section\t.rodata.cod2,\"a\",@progbits", 32)
    emit_blob(".data", "cod2_data", ".section\t.data.cod2,\"aw\",@progbits", 32)
    emit_blob(".bss", "cod2_bss", ".section\t.bss.cod2,\"aw\",@nobits", 128)

    # CRT tables image (.ctors/.dtors/.jcr), referenced by data pointers and chunk 000.
    w("")
    w("\t.section\t.data.cod2.crt,\"aw\",@progbits")
    w("\t.balign\t4")
    w("\t.globl\tcod2_crt_tables")
    w("\t.type\tcod2_crt_tables, @object")
    w("/* Copy of the original .ctors/.dtors/.jcr (0x0817d00c..0x0817d030). It is NOT the live CRT")
    w(" * constructor list: constructors are registered through .init_array below. */")
    w("cod2_crt_tables:")
    a0 = secs[".ctors"]["addr"]
    a1 = secs[".jcr"]["addr"] + secs[".jcr"]["size"]
    for a in range(a0, a1, 4):
        v = struct.unpack_from("<I", elf.data, off_of(a))[0]
        w(f"\t.long\t{rewrites.get(a, f'0x{v:08x}')}\t/* 0x{a:08x} */")
    w(f"\t.size\tcod2_crt_tables, 0x{a1 - a0:x}")

    w("")
    w("/* Original .ctors run by __do_global_ctors_aux from the last entry to the first;")
    w(" * .init_array runs forwards, so the entries are listed in reverse. */")
    w("\t.section\t.init_array,\"aw\",@init_array")
    w("\t.balign\t4")
    for v, sym in ctor_syms:
        w(f"\t.long\t{sym}\t/* original .ctors entry 0x{v:08x} */")
    for v in ctor_missing:
        w(f"\t/* original .ctors entry {v}: no reconstructed function, not registered */")

    w("")
    w("/* ---- global aliases at their original offsets ---- */")
    w("\t.text")
    for a in starts:
        ent = symbol_map[f"0x{a:08x}"]
        sec = ent["section"]
        if sec in BLOBS:
            base = BLOBS[sec]
            expr = f"{base}+0x{a - secs[sec]['addr']:x}"
        else:
            expr = f"cod2_crt_tables+0x{a - a0:x}"
        for n in [ent["name"], *ent["aliases"]]:
            w(f"\t.globl\t{n}\n\t.type\t{n}, @object\n\t.set\t{n}, {expr}\n\t.size\t{n}, {ent['size']}")
    (OUTDIR / "cod2_data.S").write_text("\n".join(out) + "\n")

    ld = ["/* Generated by gen_globals.py: data-declared names that denote original function",
          " * addresses, bound to the reconstructed function (or library import) at that address. */"]
    for n, tgt in sorted(func_aliases.items()):
        ld.append(f"{n} = {tgt}; /* 0x{name_addr[n]:08x} */")
    (OUTDIR / "cod2_aliases.ld").write_text("\n".join(ld) + "\n")
    used = set(rewrites.values()) | set(func_aliases.values()) | {sym for _v, sym in ctor_syms}
    BUILD.mkdir(parents=True, exist_ok=True)
    (BUILD / "static_aliases.json").write_text(json.dumps(
        {c: {a: v for a, v in sorted(m.items()) if a in used} for c, m in sorted(static_aliases.items())
         if any(a in used for a in m)}, indent=1, sort_keys=True) + "\n")
    (OUTDIR / "symbol_map.json").write_text(json.dumps(symbol_map, indent=1, sort_keys=True) + "\n")

    referenced_data = sorted(n for n in undefined if n in data_aliases)
    unresolved_refs = sorted(n for n in undefined if n not in name_addr and n not in sysnames
                             and not n.startswith("_Z") and n not in called)
    summary = {
        "original_sha256": EXPECT_SHA,
        "compile_failures": sorted(comp["failed"]),
        "missing_system_libs": missing_libs,
        "data_symbols_emitted": len(data_aliases),
        "data_addresses": len(starts),
        "referenced_data_symbols_defined": len(referenced_data),
        "unresolved_data_names": unresolved_refs,
        "skipped": skipped,
        "name_conflicts": conflicts,
        "pointer_scan": {**stats, "rewritten": len(rewrites), "kinds": dict(sorted(kinds.items()))},
        "switch_tables": sorted(sw_tables.values(), key=lambda t: t["table"]),
        "unmapped": unmapped,
        "ambiguous": ambiguous,
        "ctors": {"registered": [f"0x{v:08x} {s}" for v, s in ctor_syms], "missing": ctor_missing},
        "recon_function_map": {"mapped": len(recon_sym), "ambiguous": recon_ambiguous,
                               "symbols": {f"0x{a:08x}": s for a, s in sorted(recon_sym.items())}},
        "function_aliases": {n: t for n, t in sorted(func_aliases.items())},
        "size_conflicts": size_conflicts,
        "cxx_typeinfo": {f"0x{a:08x}": t for a, t in sorted(typeinfos.items())},
        "cxx_vtables": vtables,
        "declared_size_overlaps": overlaps,
    }
    BUILD.mkdir(parents=True, exist_ok=True)
    (BUILD / "globals-report.json").write_text(json.dumps(summary, indent=1) + "\n")
    short = {k: v for k, v in summary.items() if not isinstance(v, (list, dict)) or k in ("compile_failures",)}
    short["function_aliases"] = len(func_aliases)
    short["pointer_scan"] = summary["pointer_scan"]
    short["counts"] = {k: len(v) for k, v in summary.items() if isinstance(v, (list, dict)) and k != "pointer_scan"}
    return short


if __name__ == "__main__":
    sys.exit(main())
