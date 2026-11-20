#!/usr/bin/env python3
"""Out-of-band UDP probe for the original CoD2 1.3 dedicated server (stdlib only).

  probe.py capture --port P --rcon PW --out DIR --case NAME --server-log FILE
  probe.py compare CASE_DIR [--json OUT]     # classify fields across run*/ as deterministic vs variable

All server output is treated as data. Packets are stored raw (.bin), as hex and as lossy text.
"""
import argparse, glob, hashlib, json, os, re, socket, sys, time

OOB = b"\xff\xff\xff\xff"


def exchange(sock, addr, payload, idle=0.6, total=6.0):
    sock.sendto(OOB + payload, addr)
    packets, start, last = [], time.time(), time.time()
    sock.settimeout(0.1)
    while time.time() - start < total:
        try:
            data, _ = sock.recvfrom(65535)
            packets.append(data)
            last = time.time()
        except socket.timeout:
            if packets and time.time() - last > idle:
                break
    return packets


class Capture:
    def __init__(self, out, addr):
        self.out, self.addr, self.n, self.index = out, addr, 0, []
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 28970))  # fixed client port: removes ephemeral-port variability from server logs
        os.makedirs(out, exist_ok=True)

    def do(self, name, payload, **kw):
        self.n += 1
        tag = "%02d-%s" % (self.n, re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_"))
        pk = exchange(self.sock, self.addr, payload, **kw)
        rec = {"tag": tag, "request": payload.decode("latin1"), "packets": []}
        # never store the real rcon password differently: it is a fixed test value
        for i, p in enumerate(pk):
            base = os.path.join(self.out, "%s.resp%d" % (tag, i))
            open(base + ".bin", "wb").write(p)
            open(base + ".hex", "w").write(p.hex() + "\n")
            open(base + ".txt", "w", encoding="utf-8", errors="replace").write(p[4:].decode("latin1") if p[:4] == OOB else p.decode("latin1"))
            rec["packets"].append({"len": len(p), "sha256": hashlib.sha256(p).hexdigest()})
        open(os.path.join(self.out, tag + ".request.hex"), "w").write((OOB + payload).hex() + "\n")
        self.index.append(rec)
        return pk

    def text(self, pk):
        return "".join(p[4:].decode("latin1") if p[:4] == OOB else "" for p in pk)


def wait_ready(cap, log, limit=120):
    t0 = time.time()
    while time.time() - t0 < limit:
        pk = exchange(cap.sock, cap.addr, b"getinfo ready", idle=0.3, total=1.0)
        if pk and b"infoResponse" in pk[0]:
            return time.time() - t0
        time.sleep(0.5)
    return None


def capture(a):
    cap = Capture(a.out, ("127.0.0.1", a.port))
    t = wait_ready(cap, a.server_log)
    if t is None:
        print("server never answered getinfo", file=sys.stderr)
        return 2
    time.sleep(1.5)
    gap = 1.1  # stay clear of the server's per-address flood protection
    steps = [
        ("getinfo", b"getinfo abc123"),
        ("getinfo_repeat", b"getinfo abc123"),
        ("getstatus", b"getstatus abc123"),
        ("getchallenge", b"getchallenge"),
        ("getchallenge_repeat", b"getchallenge"),
        ("unknown_oob", b"bogusoob hello"),
        ("rcon_badpw", b"rcon wrongpw status"),
        ("rcon_status", b"rcon %s status" % a.rcon.encode()),
        ("rcon_serverinfo", b"rcon %s serverinfo" % a.rcon.encode()),
        ("rcon_dvarlist", b"rcon %s dvarlist" % a.rcon.encode()),
        ("rcon_cvarlist", b"rcon %s cvarlist" % a.rcon.encode()),
        ("rcon_map_restart", b"rcon %s map_restart" % a.rcon.encode()),
    ]
    for name, payload in steps:
        time.sleep(gap)
        kw = {"idle": 1.0, "total": 10.0} if "list" in name else {}
        cap.do(name, payload, **kw)
        if name == "rcon_map_restart":
            time.sleep(4)
            wait_ready(cap, a.server_log, 30)
            time.sleep(gap)
            cap.do("getstatus_after_restart", b"getstatus abc123")
            time.sleep(gap)
            cap.do("rcon_status_after_restart", b"rcon %s status" % a.rcon.encode())
    time.sleep(gap)
    cap.do("rcon_quit", b"rcon %s quit" % a.rcon.encode(), idle=0.5, total=2.0)
    json.dump({"case": a.case, "ready_after_s": t, "steps": cap.index}, open(os.path.join(a.out, "index.json"), "w"), indent=1)
    return 0


# ---------- comparison / classification ----------
def load_run(d):
    r = {}
    for f in sorted(glob.glob(os.path.join(d, "[0-9][0-9]-*.resp*.txt"))):
        r[os.path.basename(f)[:-4]] = open(f, encoding="utf-8", errors="replace").read()
    return r


def split_fields(text):
    """Split into comparable items: info-string key/values or lines."""
    items = {}
    lines = text.split("\n")
    if len(lines) > 1 and lines[1].startswith("\\"):
        kv = lines[1].split("\\")[1:]
        for i in range(0, len(kv) - 1, 2):
            items["key:" + kv[i]] = kv[i + 1]
        for j, l in enumerate(lines[2:]):
            items["line%d" % j] = l
        items["head"] = lines[0]
    else:
        for j, l in enumerate(lines):
            items["line%d" % j] = l
    return items


def compare(a):
    runs = sorted(glob.glob(os.path.join(a.case_dir, "run*")))
    data = [load_run(r) for r in runs]
    names = sorted(set().union(*[d.keys() for d in data]))
    report = {"runs": [os.path.basename(r) for r in runs], "packets": {}}
    for n in names:
        texts = [d.get(n) for d in data]
        entry = {"present_in": sum(t is not None for t in texts)}
        if None in texts:
            entry["status"] = "missing_in_some_runs"
        elif len(set(texts)) == 1:
            entry["status"] = "identical"
        else:
            keys = sorted(set().union(*[split_fields(t).keys() for t in texts]))
            var = {}
            for k in keys:
                vals = [split_fields(t).get(k) for t in texts]
                if len(set(vals)) > 1:
                    var[k] = vals
            entry["status"] = "variable"
            entry["variable_fields"] = var
        report["packets"][n] = entry
    if a.json:
        json.dump(report, open(a.json, "w"), indent=1)
    for n, e in report["packets"].items():
        print("%-45s %s" % (n, e["status"]))
        for k, v in e.get("variable_fields", {}).items():
            print("    %-30s %s" % (k, [x[:50] if x else x for x in v]))
    return 0


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    c = sp.add_parser("capture")
    c.add_argument("--port", type=int, default=28960)
    c.add_argument("--rcon", required=True)
    c.add_argument("--out", required=True)
    c.add_argument("--case", required=True)
    c.add_argument("--server-log", required=True)
    c.set_defaults(fn=capture)
    d = sp.add_parser("compare")
    d.add_argument("case_dir")
    d.add_argument("--json")
    d.set_defaults(fn=compare)
    a = ap.parse_args()
    sys.exit(a.fn(a))


if __name__ == "__main__":
    main()
