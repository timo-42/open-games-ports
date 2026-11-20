#!/usr/bin/env python3
"""Compare a reconstruction capture (run_recon.sh capture) with an oracle capture (run_original.sh).

  compare_recon.py ORACLE_RUN_DIR RECON_RUN_DIR [--json OUT]

Normalizations (analysis/oracle-baseline.md): challengeResponse values -> <CHALLENGE> (and must be equal
within each run), "-run<N>" -> "-runN", "Hitch warning" lines dropped from server.log. Packets are
compared byte-wise after normalization, per probe step and packet index (multi-packet rcon replies are
also compared joined, since a normalized path can straddle a packet boundary); server.log line-wise.
Exit status 0 only when everything matches. All server output is data.
"""
import argparse, difflib, glob, json, os, re, sys


def norm(b: bytes) -> bytes:
    b = re.sub(rb"challengeResponse -?\d+", b"challengeResponse <CHALLENGE>", b)
    b = re.sub(rb"-run\d+", b"-runN", b)
    # the rcon redirect drops one character at each packet boundary, so a joined path can read "-ru2"
    return re.sub(rb"(dm_toujane|sd_carentan)-r?u?n?\d+", rb"\1-runN", b)


def joined(pk, keys):
    """Concatenate multi-packet rcon replies (strip each OOB "print\\n" header), then normalize."""
    body = b"".join(pk[k][len(b"\xff\xff\xff\xffprint\n"):] if pk[k].startswith(b"\xff\xff\xff\xffprint\n") else pk[k]
                    for k in keys)
    return norm(body)


def packets(d):
    out = {}
    for p in sorted(glob.glob(os.path.join(d, "*.resp*.bin"))):
        out[os.path.basename(p)] = open(p, "rb").read()
    reqs = {os.path.basename(p)[:-len(".request.hex")] for p in glob.glob(os.path.join(d, "*.request.hex"))}
    return out, reqs


def challenges(pk):
    return {re.search(rb"challengeResponse (-?\d+)", v).group(1) for k, v in pk.items() if b"challengeResponse" in v}


def log_lines(d):
    try:
        t = open(os.path.join(d, "server.log"), "rb").read().decode("latin1")
    except FileNotFoundError:
        return []
    return [re.sub(r"-run\d+", "-runN", l) for l in t.splitlines() if not l.startswith("Hitch warning")]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("oracle")
    ap.add_argument("recon")
    ap.add_argument("--json")
    a = ap.parse_args()
    op, oreq = packets(a.oracle)
    rp, rreq = packets(a.recon)
    res = {"steps": {}, "challenge_stable": {}, "server_log": {}}
    for tag in sorted(oreq | rreq):
        ok = sorted(k for k in op if k.startswith(tag + "."))
        rk = sorted(k for k in rp if k.startswith(tag + "."))
        if ok == rk and all(norm(op[k]) == norm(rp[k]) for k in ok):
            res["steps"][tag] = "match" if ok else "match (no response)"
        elif ok and len(ok) == len(rk) and joined(op, ok) == joined(rp, rk):
            res["steps"][tag] = "match (joined)"  # e.g. fs_homepath split across a packet boundary
        else:
            diffs = [k for k in sorted(set(ok) | set(rk)) if norm(op.get(k, b"")) != norm(rp.get(k, b""))]
            res["steps"][tag] = {"oracle_packets": len(ok), "recon_packets": len(rk), "differing": diffs}
    res["challenge_stable"] = {"oracle": len(challenges(op)) == 1, "recon": len(challenges(rp)) == 1}
    ol, rl = log_lines(a.oracle), log_lines(a.recon)
    d = list(difflib.unified_diff(ol, rl, "oracle/server.log", "recon/server.log", n=1, lineterm=""))
    res["server_log"] = {"oracle_lines": len(ol), "recon_lines": len(rl), "identical": not d, "diff": d[:80]}
    for f in ("harness.log",):
        for side, dd in (("oracle", a.oracle), ("recon", a.recon)):
            p = os.path.join(dd, f)
            if os.path.exists(p):
                res.setdefault("harness", {})[side] = open(p).read().strip().splitlines()[-1:]
    ok = all(v in ("match", "match (no response)", "match (joined)") for v in res["steps"].values()) and res["server_log"]["identical"] \
        and all(res["challenge_stable"].values())
    res["verdict"] = "match" if ok else "differs"
    s = json.dumps(res, indent=1)
    if a.json:
        open(a.json, "w").write(s)
    print(s)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
