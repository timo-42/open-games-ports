#!/usr/bin/env python3
"""Generate matching gdb call-trace probes for the original cod2_lnxded and the reconstruction.

Every reconstructed function carries a header comment `// 0xADDR size=N ...` (or `/* 0xADDR ...`) followed by
its definition; this maps original entry addresses to reconstruction symbols (via `nm -C`). The generated
probe files break at the entry of each selected function while a window is open and print the function's
original address plus its first stack argument words; the window opens at the entry of a trigger function
(optionally only when a condition holds, e.g. a level time) and closes at the entry of a stop function.
Bss/data addresses in conditions are written as original addresses and shifted for the reconstruction
(its data image keeps the original layout at +0x2B880 for 0x08400000..0x08a00000, see analysis/global-data-image.md).

  calltrace.py --chunks 032 033 --trigger 0x080c1260 --stop 0x080b9dfa \\
      --when '*(int*)0x0864c56c == 35100' --args 4 --out-dir DIR
  -> DIR/trace_orig.gdb, DIR/trace_recon.gdb (append with gameplay.py run --gdb-extra)
  calltrace.py diff ORIG_GDB_LOG RECON_GDB_LOG   # first divergence, pointer-like words masked

Evidence only: the probe prints raw words; floats are shown as hex and decoded in `diff`.
"""
import argparse
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.abspath(os.path.join(HERE, "../.."))
REPO = os.path.abspath(os.path.join(GAME, "../.."))
RECON = os.environ.get("RECON_BIN", os.path.join(REPO, ".local/cod2/build/cod2_lnxded_recon"))
SHIFT = 0x2B880


def chunk_functions(chunk):
    """[(orig_addr, name)] from recon/chunks/chunk_NNN.cpp header comments."""
    src = open(os.path.join(GAME, "recon/chunks/chunk_%s.cpp" % chunk), encoding="latin1").read().split("\n")
    out = []
    pending = None
    for line in src:
        m = re.match(r"\s*(?://|/\*|\*)\s*0x([0-9a-fA-F]{8})\s+size=", line)
        if m:
            pending = int(m.group(1), 16)
            continue
        if pending is not None:
            m = re.match(r"^(?:static\s+)?[A-Za-z_][\w\s\*&:<>,]*?\b([A-Za-z_]\w*)\s*\(", line)
            if m and not line.rstrip().endswith(";") and not line.startswith((" ", "\t", "//", "/*")):
                out.append((pending, m.group(1)))
                pending = None
    return out


ARITY = {}


def _arity(sig):
    """Stack words of a demangled C++ parameter list ('(float, double, int*)' -> 4)."""
    inner = sig[sig.index("(") + 1: sig.rindex(")")] if "(" in sig else ""
    if inner.strip() in ("", "void"):
        return 0
    depth, parts, cur = 0, [], ""
    for ch in inner:
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    parts.append(cur)
    return sum(2 if p.strip() in ("double", "long long", "unsigned long long") else 1 for p in parts)


def recon_symbols():
    nm = subprocess.run(["nm", "-C", RECON], capture_output=True, text=True).stdout
    syms = {}
    for l in nm.splitlines():
        p = l.split(" ", 2)
        if len(p) == 3 and p[0] and p[1] in ("T", "t"):
            name = p[2].split("(")[0]
            syms.setdefault(name, int(p[0], 16))
            ARITY.setdefault(name, _arity(p[2]))
    return syms


def shift_cond(cond):
    return re.sub(r"0x(08[0-9a-fA-F]{6})\b",
                  lambda m: "0x%08x" % (int(m.group(1), 16) + SHIFT) if 0x08400000 <= int(m.group(1), 16) < 0x08A00000
                  else m.group(0), cond)


def gen(a):
    syms = recon_symbols()
    funcs = []
    for ch in a.chunks:
        funcs += chunk_functions(ch)
    for extra in a.func:
        addr, name = extra.split("=")
        funcs.append((int(addr, 16), name))
    os.makedirs(a.out_dir, exist_ok=True)
    missing = []
    for side in ("orig", "recon"):
        lines = ["set $tr = 0", "set $trn = 0"]

        def addr_of(orig, name):
            if side == "orig":
                return orig
            return syms.get(name)

        def name_for(addr):
            for o, n in funcs:
                if o == addr:
                    return n
            return None

        trig = addr_of(int(a.trigger, 16), name_for(int(a.trigger, 16)) or a.trigger_name)
        stop = addr_of(int(a.stop, 16), name_for(int(a.stop, 16)) or a.stop_name)
        when = a.when if side == "orig" else shift_cond(a.when)
        # breakpoint numbers: a.bp_base breakpoints exist before this file (gameplay.py's seed pin = 1); the trigger is
        # base+1, the stop base+2, the traced functions base+3..last. Traced functions stay disabled outside the window
        # (conditional breakpoints in hot functions would slow the server to a crawl).
        first, last = a.bp_base + 2, a.bp_base + 2 + len([f for f in funcs if addr_of(*f) is not None])
        lines += ["set $ctn = 0",
                  "break *0x%x if $trn == 0 && (%s)" % (trig, when), "commands", "silent", "set $ctn = $ctn + 1",
                  "if $ctn == %d" % a.nth, "set $tr = 1",
                  "set $trn = 1", "enable %d-%d" % (first, last), 'printf "CT open\\n"', "end", "continue", "end",
                  "break *0x%x" % stop, "commands", "silent", "set $tr = 0", "disable %d-%d" % (first - 1, last),
                  'printf "CT close\\n"', "continue", "end"]
        for orig, name in funcs:
            ad = addr_of(orig, name)
            if ad is None:
                if side == "recon":
                    missing.append(name)
                continue
            n = min(ARITY.get(name, a.args), a.args)  # arity from the reconstruction's signature
            fmt = " ".join(["%08x"] * n)
            argv = "".join(", *(unsigned*)($esp+%d)" % (4 + 4 * i) for i in range(n))
            for spec in a.deref:  # ORIGADDR:ARGINDEX:COUNT -> also print COUNT dwords the argument points to
                da, di, dc = spec.split(":")
                if int(da, 16) == orig:
                    fmt += " [" + " ".join(["%08x"] * int(dc)) + "]"
                    argv += "".join(", *(unsigned*)(*(unsigned*)($esp+%d)+%d)" % (4 + 4 * int(di), 4 * k)
                                    for k in range(int(dc)))
            lines += ["break *0x%x" % ad, "commands", "silent",
                      'printf "CT %08x %s %s ret=%%08x\\n"%s, *(unsigned*)$esp' % (orig, name, fmt, argv), "continue", "end"]
        lines.append("disable %d-%d" % (first, last))
        open(os.path.join(a.out_dir, "trace_%s.gdb" % side), "w").write("\n".join(lines) + "\n")
    print("functions: %d, missing in recon: %s" % (len(funcs), missing))


def parse(path):
    out = []
    for l in open(path, errors="replace"):
        if l.startswith("CT ") and not l.startswith(("CT open", "CT close")):
            p = l.replace("[", "").replace("]", "").split()
            ret = [w for w in p[3:] if w.startswith("ret=")]
            out.append((p[1], p[2] + ("<-" + ret[0][4:] if ret else ""), [w for w in p[3:] if not w.startswith("ret=")]))
    return out


def norm(w):
    v = int(w, 16)
    if 0x08000000 <= v < 0x0a000000 or 0xe0000000 <= v < 0xffffff00:
        return "PTR"
    return w


def fdec(w):
    v = int(w, 16)
    f = struct.unpack("<f", struct.pack("<I", v))[0]
    return "%s(%g)" % (w, f) if 1e-6 < abs(f) < 1e7 else w


def diff(a):
    """Align the two call sequences by function (difflib), list structural differences (calls present on one side
    only, e.g. a helper the original inlines), then report the first matched call whose first --nargs argument
    words differ (pointer-like words masked)."""
    import difflib
    o, r = parse(a.orig_log), parse(a.recon_log)
    print("calls: orig %d, recon %d" % (len(o), len(r)))
    sm = difflib.SequenceMatcher(None, [x[0] for x in o], [x[0] for x in r], autojunk=False)  # by function address
    pairs = []
    structural = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            pairs += list(zip(range(i1, i2), range(j1, j2)))
        else:
            structural.append((tag, [o[i][1] for i in range(i1, i2)], [r[j][1] for j in range(j1, j2)]))
    print("structural differences: %d" % len(structural))
    for st in structural[:a.context]:
        print("  %s orig=%s recon=%s" % (st[0], st[1][:6], st[2][:6]))
    for n, (i, j) in enumerate(pairs):
        if [norm(w) for w in o[i][2][:a.nargs]] != [norm(w) for w in r[j][2][:a.nargs]]:
            print("first argument divergence at matched call #%d (orig #%d, recon #%d)" % (n, i, j))
            for m in range(max(0, n - a.context), min(len(pairs), n + a.context)):
                oi, rj = pairs[m]
                mark = ">>" if m == n else "  "
                print("%s orig  %s %-34s %s" % (mark, o[oi][0], o[oi][1], " ".join(fdec(w) for w in o[oi][2])))
                print("%s recon %s %-34s %s" % (mark, r[rj][0], r[rj][1], " ".join(fdec(w) for w in r[rj][2])))
            return 1
    print("no argument divergence in %d matched calls" % len(pairs))
    return 1 if structural else 0


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd")
    g = sp.add_parser("gen")
    g.add_argument("--chunks", nargs="*", default=[])
    g.add_argument("--func", nargs="*", default=[], help="extra ORIGADDR=reconName")
    g.add_argument("--trigger", required=True)
    g.add_argument("--trigger-name", default="")
    g.add_argument("--stop", required=True)
    g.add_argument("--stop-name", default="")
    g.add_argument("--when", default="1")
    g.add_argument("--args", type=int, default=4)
    g.add_argument("--bp-base", type=int, default=1, help="breakpoints defined before this file (seed pin)")
    g.add_argument("--deref", nargs="*", default=[], help="ORIGADDR:ARGINDEX:COUNT pointer dumps")
    g.add_argument("--nth", type=int, default=1, help="open the window at the n-th trigger hit that satisfies --when")
    g.add_argument("--out-dir", required=True)
    g.set_defaults(fn=gen)
    d = sp.add_parser("diff")
    d.add_argument("orig_log")
    d.add_argument("recon_log")
    d.add_argument("--context", type=int, default=6)
    d.add_argument("--nargs", type=int, default=99, help="compare only the first N argument words (arity unknown)")
    d.set_defaults(fn=diff)
    a = ap.parse_args()
    if not a.cmd:
        ap.print_help()
        return 2
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
