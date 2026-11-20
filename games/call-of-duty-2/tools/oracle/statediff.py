#!/usr/bin/env python3
"""State differential: original cod2_lnxded vs the reconstruction at the same breakpoint.

Runs both servers (same sandbox/arguments as run_original.sh / run_recon.sh, under batch gdb inside
`bwrap --unshare-net`), stops each at the N-th hit of a function, dumps the .data and .bss images and
compares them word by word. The reconstruction's data image keeps the original layout inside the blobs
cod2_data / cod2_bss / cod2_rodata (gen_globals.py), so offsets correspond 1:1; a word that is a pointer
into a blob is considered equal when it points to the same blob offset on both sides. Heap pointers are
reported separately (they can legitimately differ). Differences are grouped by the data-image symbol that
contains them (.local/cod2/globals/symbol_map.json, written by tools/gen_globals.py).

  statediff.py --orig 0x08076296 --recon Scr_LoadScriptFile --hit 1 [--case dm_toujane] [--limit 40]

All server output is data; nothing is executed outside the sandbox.
"""
from __future__ import annotations

import argparse, bisect, json, os, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
COD2 = Path(os.environ.get("COD2_LOCAL", REPO / ".local/cod2"))
ORIG = COD2 / "lnxded-1.3/cod2_lnxded"
RECON = Path(os.environ.get("RECON_BIN", COD2 / "build/cod2_lnxded_recon"))
SECS = {".data": (0x0817D3A0, 0xB800, "cod2_data"), ".bss": (0x08188C00, 0x8E68D8, "cod2_bss"),
        ".rodata": (0x0813E340, 0x22ADC, "cod2_rodata")}
CASES = {"dm_toujane": "+set g_gametype dm +map mp_toujane", "sd_carentan": "+set g_gametype sd +map mp_carentan",
         "none": ""}


def recon_bases() -> dict[str, int]:
    out = subprocess.run(["nm", str(RECON)], capture_output=True, text=True).stdout
    syms = {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
    return {sec: syms[label] for sec, (_, _, label) in SECS.items()}


def run(binary: Path, bp: str, hit: int, case: str, out: Path, rbases: dict[str, int] | None, tlimit: int) -> Path:
    oracle = COD2 / "oracle"
    home = oracle / "run/home" / "statediff"  # same path on both sides: identical command lines and dvar strings
    subprocess.run(["rm", "-rf", str(home)])
    home.mkdir(parents=True)
    out.mkdir(parents=True, exist_ok=True)
    bases = rbases or {s: v[0] for s, v in SECS.items()}
    dumps = "\n".join(f"dump binary memory {out}/{s[1:]}.bin {bases[s]:#x} {bases[s] + SECS[s][1]:#x}" for s in (".data", ".bss"))
    if not bp.startswith("0x"):  # demangled base name -> address
        nmo = subprocess.run(["nm", "-C", str(binary)], capture_output=True, text=True).stdout
        hits = [l.split()[0] for l in nmo.splitlines() if len(l.split(None, 2)) == 3 and l.split(None, 2)[1] in "Tt"
                and l.split(None, 2)[2].split("(")[0] == bp]
        if len(hits) != 1:
            raise SystemExit(f"cannot resolve {bp}: {hits}")
        bp = "0x" + hits[0]
    bpexpr = f"*{bp}"
    env = f"set environment LD_LIBRARY_PATH {COD2}/runtime-i386/usr/lib/i386-linux-gnu\n" if not rbases else ""
    (out / "cmds.gdb").write_text(
        f"set pagination off\nset confirm off\nhandle SIGPIPE nostop noprint\n{env}"
        f"set environment LD_PRELOAD {oracle}/lib/stat32_shim.so\nbreak {bpexpr}\n"
        f"ignore 1 {hit - 1}\nrun\nbt 30\n{dumps}\nkill\nquit\n")
    args = (f"+set dedicated 1 +set net_ip 127.0.0.1 +set net_port 28960 +set sv_maxclients 8 +set rcon_password "
            f"oracle-test-pw +set sv_punkbuster 0 +set sv_pure 1 +set fs_basepath {oracle}/run/base "
            f"+set fs_homepath {home} {CASES[case]}")
    inner = (f"cd {COD2}/lnxded-1.3; (sleep {tlimit}; pkill -INT -f '^{binary}') & "
             f"sleep {tlimit + 30} | gdb -q -batch -x {out}/cmds.gdb --args {binary} {args} > {out}/gdb.log 2>&1")
    subprocess.run(["bwrap", "--ro-bind", "/", "/", "--dev", "/dev", "--proc", "/proc", "--bind", str(oracle / "run"),
                    str(oracle / "run"), "--bind", str(out), str(out), "--unshare-net", "--unshare-pid", "--unshare-ipc",
                    "--die-with-parent", "bash", "-c", inner], timeout=tlimit + 120)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--orig", required=True, help="original breakpoint address (0x...)")
    ap.add_argument("--recon", required=True, help="reconstruction breakpoint (symbol or 0x...)")
    ap.add_argument("--hit", type=int, default=1)
    ap.add_argument("--case", default="dm_toujane", choices=CASES)
    ap.add_argument("--limit", type=int, default=60)
    ap.add_argument("--tlimit", type=int, default=90)
    ap.add_argument("--out", default=str(COD2 / "recon-run/statediff"))
    ap.add_argument("--compare-only", action="store_true", help="reuse existing dumps in --out")
    ap.add_argument("--show", help="print every differing word of this symbol")
    a = ap.parse_args()
    rb = recon_bases()
    out = Path(a.out).resolve()
    if not a.compare_only:
        run(ORIG, a.orig, a.hit, a.case, out / "orig", None, a.tlimit)
        run(RECON, a.recon, a.hit, a.case, out / "recon", rb, a.tlimit)
    for side in ("orig", "recon"):
        log = (out / side / "gdb.log").read_text(errors="replace")
        bt = [l for l in log.splitlines() if l.startswith("#")][:6]
        print(f"[{side}] stop:", *bt, sep="\n  ")
    smap = json.loads((Path(os.environ.get("COD2_GLOBALS", REPO / ".local/cod2/globals")) / "symbol_map.json").read_text())
    starts = sorted((int(k, 16), v["name"], v.get("size", 0)) for k, v in smap.items() if v.get("section") in (".data", ".bss"))
    keys = [s[0] for s in starts]

    def owner(addr: int) -> str:
        i = bisect.bisect_right(keys, addr) - 1
        if i < 0:
            return "?"
        a0, n, sz = starts[i]
        return f"{n}+{addr - a0:#x}" if addr - a0 < max(sz, 1) else f"(after {n}+{addr - a0:#x})"

    def classify(v: int, bases: dict[str, int]) -> tuple[str, int] | None:
        for s, (o, sz, _) in SECS.items():
            if bases[s] <= v < bases[s] + sz:
                return s, v - bases[s]
        return None

    obases = {s: v[0] for s, v in SECS.items()}
    sys.path.insert(0, str(REPO / "games/call-of-duty-2/tools"))
    import gen_globals as gg  # noqa: E402
    oelf, nelf = gg.Elf32(ORIG), gg.Elf32(RECON)

    def cstr(elf, v: int) -> bytes | None:
        for sct in elf.sections:
            if sct["flags"] & 2 and sct["type"] != 8 and sct["addr"] <= v < sct["addr"] + sct["size"] and not sct["flags"] & 4:
                o = sct["offset"] + v - sct["addr"]
                e = elf.data.find(b"\0", o, o + 4096)
                return elf.data[o:e] if e >= o else None
        return None
    # original function address -> reconstruction function address (symbols.json names, unique demangled base)
    fsyms = json.loads((REPO / "games/call-of-duty-2/analysis/symbols/symbols.json").read_text())["functions"]
    nmo = subprocess.run(["nm", "-C", str(RECON)], capture_output=True, text=True).stdout
    rby: dict[str, list[int]] = {}
    for l in nmo.splitlines():
        q = l.split(None, 2)
        if len(q) == 3 and q[1] in "Tt":
            rby.setdefault(q[2].split("(")[0], []).append(int(q[0], 16))
    fmap = {int(k, 16): rby[v["name"]][0] for k, v in fsyms.items() if len(rby.get(v["name"], [])) == 1}

    def heapish(v: int) -> bool:
        return 0x08a00000 <= v < 0x20000000 or 0xd0000000 <= v < 0xf8000000
    raw: list[tuple[str, int, int, int]] = []
    groups: dict[str, list[str]] = {}
    total = 0
    for sec in (".data", ".bss"):
        try:
            ob = (out / "orig" / f"{sec[1:]}.bin").read_bytes()
            nb = (out / "recon" / f"{sec[1:]}.bin").read_bytes()
        except FileNotFoundError:
            print("dump missing (breakpoint not reached?)")
            return 2
        for off in range(0, min(len(ob), len(nb)) - 3, 4):
            ow, nw = ob[off:off + 4], nb[off:off + 4]
            if ow == nw:
                continue
            ov, nv = int.from_bytes(ow, "little"), int.from_bytes(nw, "little")
            co, cn = classify(ov, obases), classify(nv, rb)
            if co and cn and co == cn:
                continue
            if fmap.get(ov) == nv:
                continue
            so = cstr(oelf, ov) if 0x08048000 <= ov < 0x08188c00 else None
            if so is not None and so == (cstr(nelf, nv) if 0x08048000 <= nv < 0x08188c00 + 0x40000 else None):
                continue
            raw.append((sec, off, ov, nv))
    # heap/mmap pointers: drop words whose difference is one of the dominant deltas (allocation streams shifted)
    from collections import Counter
    deltas = Counter((nv - ov) & 0xffffffff for _, _, ov, nv in raw if heapish(ov) and heapish(nv))
    common = {d for d, c in deltas.most_common(8) if c >= 8}
    for sec, off, ov, nv in raw:
        if heapish(ov) and heapish(nv) and ((nv - ov) & 0xffffffff) in common:
            continue
        total += 1
        addr = SECS[sec][0] + off
        groups.setdefault(owner(addr).split("+")[0], []).append(f"{addr:#010x} {owner(addr)}: orig {ov:#010x} recon {nv:#010x}")
    print(f"differing words: {total} in {len(groups)} symbols")
    for n, (g, lines) in enumerate(sorted(groups.items(), key=lambda kv: kv[1][0])):
        if n >= a.limit:
            break
        print(f"{g}: {len(lines)} words; first: {lines[0]}")
    if a.show:
        print(*groups.get(a.show, []), sep="\n")
    (out / "diff.json").write_text(json.dumps(groups, indent=0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
