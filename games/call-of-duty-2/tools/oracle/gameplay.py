#!/usr/bin/env python3
"""Gameplay oracle for the CoD2 1.3 Linux dedicated server: two scripted fake clients that pick a team and a
weapon through the stock GSC menus, spawn, move, fire, kill, die and respawn, keyed to absolute server time.

  gameplay.py run <case> [run-index] [--server original|recon|rust] [--seed N | --seed none] [-- extra server args]
        Server (optionally under gdb, which pins G_InitGame's randomSeed argument) and both clients inside one
        `bwrap --unshare-net` namespace (loopback only). Captures go to
          original: .local/cod2/oracle/captures/gameplay_<case>/run<N>/
          recon:    .local/cod2/recon-run/captures/gameplay_<case>/run<N>/
          rust:     .local/cod2/rust-run/captures/gameplay_<case>/run<N>/ (COD2_RUST_BIN, native binary)
  gameplay.py session ...          (inner step inside the namespace)
  gameplay.py compare CASE_DIR [OTHER_CASE_DIR] [--json OUT]
        Same semantics as fakeclient.py compare: classify items across the original runs, then list every item that
        is deterministic in the original but differs in OTHER.

Determinism controls (analysis/gameplay-parity.md):
  * G_InitGame(levelTime, randomSeed, ...) gets randomSeed = Sys_MillisecondsRaw() from SV_InitMapState (0x080925e6 ->
    0x08109096); it seeds both libc srand() and the game LCG (Rand_SetSeed 0x080aaa76). No dvar controls it. With
    --seed N (default 1234567) the server runs under gdb with a breakpoint at G_InitGame's entry that overwrites the
    argument ([esp+8]) before the first instruction, for the original and the reconstruction alike.
  * Every client action and every usercmd is a function of the server time of the snapshot it answers (one usercmd
    per received snapshot, usercmd serverTime = that snapshot's serverTime), never of the wall clock.
  * Usercmds are sent fully explicit (cod2proto.write_usercmd_full) so the server's CL_CreateCmd(ps) delta base
    does not matter.
"""

import argparse
import glob
import hashlib
import json
import math
import os
import re
import select
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import cod2proto as P  # noqa: E402
import fakeclient as F  # noqa: E402

OOB = F.OOB
RCON = F.RCON
G_INITGAME = {"original": "0x08109096"}
SV_SPAWNSERVER = {"original": "0x08093520"}  # recon: from nm (_Z14SV_SpawnServerPc)
# Rotation cases: the second SV_SpawnServer's wall-clock duration (original 916-1033 ms) decides in which 50 ms server
# frame the new level's first snapshot falls, which moves spawn choices and every epoch-2 record. Both servers are
# padded under gdb to this fixed duration so the frame quantisation is identical (analysis/gameplay/gt_tdm.md).
SPAWN_PAD_S = 1.150
PAD_GDB = """python
import gdb, time
class _PadFin(gdb.FinishBreakpoint):
    def __init__(self, t0):
        gdb.FinishBreakpoint.__init__(self, gdb.newest_frame(), internal=True)
        self.t0 = t0
    def stop(self):
        rem = %(pad)r - (time.monotonic() - self.t0)
        if rem > 0:
            time.sleep(rem)
        gdb.write("SPAWNPAD rem=%%.3f\\n" %% rem)
        return False
class _PadSpawn(gdb.Breakpoint):
    n = 0
    def stop(self):
        _PadSpawn.n += 1
        if _PadSpawn.n >= 2:
            _PadFin(time.monotonic())
        return False
_PadSpawn("*%(addr)s", internal=True)
end
"""  # entry of G_InitGame in cod2_lnxded 1.3; the recon address comes from nm
DEFAULT_SEED = 1234567

# buttons (usercmd_t.buttons), inferred from the pmove/weapon readers of the original:
B_ATTACK, B_MELEE, B_USE, B_RELOAD = 0x1, 0x4, 0x8, 0x10  # useButtonPressed tests 0x28 (0x08 | 0x20)
B_LEANLEFT, B_LEANRIGHT, B_PRONE, B_CROUCH, B_JUMP, B_ADS = 0x40, 0x80, 0x100, 0x200, 0x400, 0x1000


def angle2short(a):
    return int(a * 65536.0 / 360.0) & 0xFFFF


# =============================================================================================
# per-snapshot gameplay record
# =============================================================================================
def exact_fields(words, fields, nonzero_only=True):
    """Like cod2proto.named_fields, but float fields keep every bit ('%.9g' round-trips a float32), so 1-ulp
    differences between servers are visible."""
    out = {}
    for name, off, bits in fields:
        v = words[off >> 2]
        if nonzero_only and v == 0:
            continue
        if bits in (0, -99, -100):
            out[name] = float("%.9g" % P.bits2f(v))
        else:
            out[name] = P.s32(v) if bits < 0 else v
    return out


def ps_record(ps):
    rec = exact_fields(ps, P.PLAYER_FIELDS)
    for i in range(6):
        v = ps[(0x12C >> 2) + i]
        if v:
            rec["stats[%d]" % i] = P.s32(v)
    for name, base in (("ammo", 0x144), ("ammoclip", 0x344)):
        for i in range(64):
            v = ps[(base >> 2) + i]
            if v:
                rec["%s[%d]" % (name, i)] = P.s32(v)
    return rec


def ent_diff(old, new):
    """{num: {field: value}} for entities that are new or changed; removed entity numbers listed separately."""
    ch = {}
    for n, st in new.items():
        o = old.get(n)
        if o == st:
            continue
        f = exact_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
        if o is None:
            ch[str(n)] = {k: v for k, v in f.items() if v != 0}
            ch[str(n)]["_new"] = 1
        else:
            fo = exact_fields(o, P.ENTITY_FIELDS, nonzero_only=False)
            ch[str(n)] = {k: v for k, v in f.items() if fo.get(k) != v}
    removed = sorted(n for n in old if n not in new)
    return ch, removed


class Player:
    """One scripted client: the fakeclient.Client plus a timeline keyed to absolute server time."""

    def __init__(self, tag, idx, port, out, team, weapon):
        self.tag, self.team, self.weapon_choice = tag, team, weapon
        self.dir = os.path.join(out, "client" + tag)
        os.makedirs(self.dir, exist_ok=True)
        self.log = F.Log(self.dir)
        self.c = F.Client(port, self.dir, self.log, client_port=28971 + idx, qport=0x1D2B + idx)
        self.name = "fake" + tag
        self.phase = "idle"
        self.gs_seen = 0
        self.records = []
        self.prev_ents = {}
        self.prev_clients = {}
        self.done = set()
        self.cmd = {"buttons": 0, "angles": [0, 0, 0], "forwardmove": 0, "rightmove": 0, "weapon": 0, "offhand": 0}
        self.cmd_log = []
        self.held_s2c = []  # (raw, release) pairs of the optional scenario hook hold_s2c
        self.marks = {}
        self.active_at = None
        self.epoch = 0  # number of gamestates received (2+ after a map change)
        self.anchor = None  # absolute server time that relative times refer to (T0, then each new level's start)

    def ps(self):
        f = self.c.frames.get(self.c.in_seq)
        return f["ps"] if f else None

    def ents(self):
        f = self.c.frames.get(self.c.in_seq)
        return f["ents"] if f else {}

    def psn(self):
        ps = self.ps()
        return P.named_fields(ps, P.PLAYER_FIELDS, nonzero_only=False) if ps else {}

    def record(self, seq):
        fr = self.c.frames.get(seq)
        sn = self.c.snapshots[-1] if self.c.snapshots else None
        if not fr or not sn or sn["msg"] != seq:
            return
        ch, removed = ent_diff(self.prev_ents, fr["ents"])
        cch, cremoved = ent_diff(self.prev_clients, fr["clients"]) if False else ({}, [])
        if fr["clients"] != self.prev_clients:
            cch = {
                str(n): P.named_fields(st, P.CLIENT_FIELDS)
                for n, st in sorted(fr["clients"].items())
                if self.prev_clients.get(n) != st
            }
            cremoved = sorted(n for n in self.prev_clients if n not in fr["clients"])
        self.prev_ents, self.prev_clients = fr["ents"], fr["clients"]
        self.records.append(
            {
                "t": sn["serverTime"],
                "epoch": self.epoch,
                "rel": sn["serverTime"] - (self.anchor or 0),
                "msg": seq,
                "deltaNum": sn["deltaNum"],
                "snapFlags": sn["snapFlags"],
                "ps": ps_record(fr["ps"]),
                "ents": ch,
                "ents_removed": removed,
                "ps_hash_rel": sn["ps_hash_rel"],
                "clients": cch,
                "clients_removed": cremoved,
                "ps_hash": sn["ps_hash"],
                "ents_hash": sn["ents_hash"],
                "num_entities": sn["num_entities"],
            }
        )


# =============================================================================================
# scenario: duel (two clients, cheats on for setviewpos)
# =============================================================================================
def menu_index(gs, name):
    for k, v in gs["configstrings"].items():
        if 0x4DE <= int(k) < 0x4DE + 0x20 and v == name:
            return int(k) - 0x4DE
    return None


def full_server_id(gs):
    m = re.search(r"\\sv_serverid\\(-?\d+)", "\\" + gs["configstrings"].get(1, "").lstrip("\\"))
    return int(m.group(1)) if m else 0


def weapon_index(gs, name):
    names = gs["configstrings"].get(7, "").split()  # [O] configstring 7 lists the precached weapons, index 0 first
    return names.index(name) + 1 if name in names else None


class Duel:
    """Timeline relative to T0 (absolute server time, ms). Every decision uses only data from the deciding client's
    own latest snapshot, so it is a function of server state at a fixed server time."""

    def __init__(self, t0, players, summary):
        self.t0 = t0
        self.players = players
        self.A, self.B = players[0], players[1]
        self.summary = summary
        self.notes = summary.setdefault("scenario_notes", {})

    def actions(self, p, t):
        """Reliable client commands due at relative time t (called once per snapshot; each key fires once)."""
        gs = p.c.gamestate
        out = []

        def once(key, fn):
            if key not in p.done:
                p.done.add(key)
                r = fn()
                if r:
                    out.append(r)

        sid = full_server_id(gs)
        if t >= 0:
            once("team", lambda: "mr %d %d %s" % (sid, menu_index(gs, "team_britishgerman"), p.team))
        if t >= 1000:
            menu = "weapon_british" if p.team == "allies" else "weapon_german"
            once("weapon", lambda: "mr %d %d %s" % (sid, menu_index(gs, menu), p.weapon_choice))
        if p is self.A:
            if t >= 1500:
                once("mark_p0", lambda: self.mark(p, "p0"))
            if t >= 2600:
                once("mark_p1", lambda: self.mark(p, "p1"))
            if t >= 15000:
                once("teleport", lambda: self.teleport(p, "p1", "p0"))
            if t >= 33000:
                once("score", lambda: "score")
            if t >= 34000:
                once("say", lambda: "say gg")
            if t >= 36000:
                once("kill", lambda: "kill")
        else:
            if t >= 15000:
                once("teleport", lambda: self.teleport(p, "p0", "p1"))
            if t >= 33000:
                once("score", lambda: "score")
        return out

    def actions2(self, p, t):
        """After the map rotation (relative to the new level's start time, configstring 13)."""
        out = []
        if p is self.A and t >= 3000 and "e2score" not in p.done:
            p.done.add("e2score")
            out.append("score")
        if p is self.B and t >= 4000 and "e2say" not in p.done:
            p.done.add("e2say")
            out.append("say map2")
        return out

    def usercmd2(self, p, t):
        cmd = {
            "buttons": 0,
            "angles": list(p.cmd["angles"]),
            "forwardmove": 0,
            "rightmove": 0,
            "weapon": 0,
            "offhand": 0,
        }
        if p is self.A and 1000 <= t < 2000:
            cmd["forwardmove"] = 127
        if p is self.B and 1000 <= t < 1500:
            cmd["rightmove"] = -127
        if p is self.B and 2500 <= t < 2600:
            cmd["buttons"] |= B_JUMP
        p.cmd = cmd
        return cmd

    def mark(self, p, name):
        ps = p.psn()
        self.A.marks[name] = [
            ps.get("origin[0]", 0.0),
            ps.get("origin[1]", 0.0),
            ps.get("origin[2]", 0.0),
            ps.get("viewHeightCurrent", 0.0),
        ]
        self.notes["mark_" + name] = self.A.marks[name]
        return None

    def teleport(self, p, at, facing):
        a, f = self.A.marks.get(at), self.A.marks.get(facing)
        if not a or not f:
            return None
        yaw = math.degrees(math.atan2(f[1] - a[1], f[0] - a[0]))
        text = "setviewpos %.1f %.1f %.1f %.1f" % (a[0], a[1], a[2] + 60.0, yaw)
        self.notes["teleport_" + p.tag] = text
        return text

    def usercmd(self, p, t):
        ps = p.psn()
        cmd = {
            "buttons": 0,
            "angles": list(p.cmd["angles"]),
            "forwardmove": 0,
            "rightmove": 0,
            "weapon": 0,
            "offhand": 0,
        }
        da = [ps.get("delta_angles[0]", 0), ps.get("delta_angles[1]", 0)]

        def look(yaw, pitch):
            cmd["angles"][0] = (angle2short(pitch) - da[0]) & 0xFFFF
            cmd["angles"][1] = (angle2short(yaw) - da[1]) & 0xFFFF

        if p is self.A:
            if 1500 <= t < 2600:
                cmd["forwardmove"] = 127
            if 2600 <= t < 3100:  # turn 90 degrees right in 10 steps relative to the view at 2600
                base = p.marks.setdefault("yaw2600", ps.get("viewangles[1]", 0.0))
                look(base - 90.0 * min(1.0, (t - 2600 + 50) / 500.0), 0.0)
            if 3500 <= t < 3600:
                cmd["buttons"] |= B_JUMP
            if 4500 <= t < 5500:
                cmd["buttons"] |= B_CROUCH
            if 5500 <= t < 7500:
                cmd["buttons"] |= B_PRONE
            if 9000 <= t < 9500:
                cmd["buttons"] |= B_ATTACK
            if 10000 <= t < 10100:
                cmd["buttons"] |= B_RELOAD
            if 12000 <= t < 13500:
                w = weapon_index(p.c.gamestate, "webley_mp")
                cmd["weapon"] = w or 0
            if 13500 <= t < 14500:
                w = weapon_index(p.c.gamestate, p.weapon_choice)
                cmd["weapon"] = w or 0
            if 16000 <= t < 19000:  # aim at B's entity in our own snapshot and fire
                me = [
                    ps.get("origin[0]", 0.0),
                    ps.get("origin[1]", 0.0),
                    ps.get("origin[2]", 0.0) + ps.get("viewHeightCurrent", 0.0),
                ]
                bnum = self.B.c.client_num
                st = p.ents().get(bnum)
                if st is not None:
                    e = P.named_fields(st, P.ENTITY_FIELDS, nonzero_only=False)
                    tgt = [e["pos.trBase[0]"], e["pos.trBase[1]"], e["pos.trBase[2]"] + 40.0]
                    dx, dy, dz = tgt[0] - me[0], tgt[1] - me[1], tgt[2] - me[2]
                    look(math.degrees(math.atan2(dy, dx)), -math.degrees(math.atan2(dz, math.hypot(dx, dy))))
                if t >= 16500:
                    cmd["buttons"] |= B_ATTACK
        else:
            if 1500 <= t < 2500:
                cmd["rightmove"] = 127
            if 3000 <= t < 3500:
                cmd["buttons"] |= B_CROUCH
            if 19500 <= t < 30000:  # hold use: respawn as soon as the script polls useButtonPressed
                cmd["buttons"] |= B_USE
        p.cmd = cmd
        return cmd


class Watch(Duel):
    """Both fake clients stay connected spectators (no menu responses); used with server-side test clients."""

    def actions(self, p, t):
        return []

    def actions2(self, p, t):
        return []

    def usercmd(self, p, t):
        p.cmd = {
            "buttons": 0,
            "angles": list(p.cmd["angles"]),
            "forwardmove": 0,
            "rightmove": 0,
            "weapon": 0,
            "offhand": 0,
        }
        return p.cmd

    usercmd2 = usercmd


SCENARIOS = {"bots_dm_toujane": Watch}
DEFAULT_PLAYERS = [("A", "allies", "thompson_mp"), ("B", "axis", "mp40_mp")]
PLAYERS = {}  # case -> [(tag, team, weapon), ...]; default DEFAULT_PLAYERS
MODS = {}  # case -> fn(home, data_dir) -> str written to harness.log (loose-file overlay under fs_homepath)
DEFAULTS = {"bots_dm_toujane": {"length": 25000}}  # case -> {"t0": ms, "length": ms}; bots captures use 25 s


def load_plugins():
    """Merge every tools/oracle/scenarios/<name>.py into the case tables. A plugin defines
        CASES = {"<case>": {"args": [server args...], "scenario": <Duel subclass>, optional "players": [(tag, team,
                 weapon), ...], "mod": fn(home, data_dir) -> str, "bot_rcon": ms rel. T0, "rotate": ms, "t0": ms,
                 "length": ms}}
    and imports this module as `gameplay` (aliased to __main__ here, so classes are shared)."""
    import importlib

    sys.modules.setdefault("gameplay", sys.modules[__name__])
    sdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "scenarios")
    if sdir not in sys.path:
        sys.path.insert(0, os.path.dirname(sdir))
    for f in sorted(glob.glob(os.path.join(sdir, "*.py"))):
        name = os.path.basename(f)[:-3]
        if name.startswith("_"):
            continue
        mod = importlib.import_module("scenarios." + name)
        for case, spec in getattr(mod, "CASES", {}).items():
            if case in CASES:
                sys.exit("scenario plugin %s redefines case %s" % (name, case))
            CASES[case] = list(spec["args"])
            SCENARIOS[case] = spec.get("scenario", Duel)
            if spec.get("players"):
                PLAYERS[case] = list(spec["players"])
            if spec.get("mod"):
                MODS[case] = spec["mod"]
            if spec.get("bot_rcon") is not None:
                BOT_RCON[case] = spec["bot_rcon"]
            if spec.get("rotate"):
                ROTATE[case] = spec["rotate"]
            if spec.get("late_connect"):
                LATE_CONNECT[case] = dict(spec["late_connect"])
            if spec.get("client_addrs"):
                CLIENT_ADDRS[case] = dict(spec["client_addrs"])
            if spec.get("userinfo"):
                USERINFO[case] = {k: dict(v) for k, v in spec["userinfo"].items()}
            DEFAULTS[case] = {k: spec[k] for k in ("t0", "length") if k in spec}


def make_test_client_mod(home, iwd):
    """Option B overlay (never committed: derived from game data): the stock dm.gsc with its commented-out
    `thread maps\\mp\\gametypes\\_teams::addTestClients();` enabled, as a loose file under
    <fs_homepath>/mods/gptest. _teams.gsc then adds scr_testclients bots (addtestclient) that join alternating
    teams with m1carbine_mp / mp40_mp; their usercmds come from SV_BotRandomUserCmd (Rand_*, pinned seed)."""
    import zipfile

    src = zipfile.ZipFile(iwd).read("maps/mp/gametypes/dm.gsc").decode("latin1")
    line = "//thread maps\\mp\\gametypes\\_teams::addTestClients();"
    if line not in src:
        sys.exit("dm.gsc: addTestClients line not found")
    out = os.path.join(home, "mods/gptest/maps/mp/gametypes")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "dm.gsc"), "w", encoding="latin1") as fh:
        fh.write(src.replace(line, line[2:]))
    return hashlib.sha256(src.replace(line, line[2:]).encode("latin1")).hexdigest()


# =============================================================================================
# session (runs inside the namespace)
# =============================================================================================
def userinfo(name, challenge, qport, extra=None):
    kv = [
        ("cg_predictItems", "1"),
        ("cl_anonymous", "0"),
        ("cl_punkbuster", "0"),
        ("cl_voice", "0"),
        ("cl_wwwDownload", "0"),
        ("rate", "25000"),
        ("snaps", "20"),
        ("name", name),
        ("protocol", "118"),
        ("challenge", str(challenge)),
        ("qport", str(qport)),
    ]
    if extra:  # per-case overrides (plugin spec "userinfo": {tag: {key: value}}), e.g. rate / snaps
        kv = [(k, str(extra[k]) if k in extra else v) for k, v in kv]
    return "".join("\\%s\\%s" % kv_ for kv_ in kv)


AUTH_HOST = "cod2master.activision.com"
AUTH_IP = "198.18.0.100"  # what AUTH_HOST resolves to inside the namespace of cases with non-LAN clients
AUTH_PORT = 20700  # SV_RequestIpAuthorize 0x0808d18e: port 0x50dc


def start_fake_authorize_server():
    """Non-LAN clients get their challenge only after the authorize server accepted them (SV_RequestIpAuthorize 0x0808d18e;
    without an answer the original refuses challenges for the first 20 minutes of uptime). The sandbox has no network, so
    AUTH_HOST is mapped to AUTH_IP in a private /etc/hosts (run()) and this thread answers `getIpAuthorize` like the
    master: `ipAuthorize <challenge> accept <key> <guid> <pbguid>`, guid derived from the client's IP."""
    import threading

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((AUTH_IP, AUTH_PORT))

    def loop():
        while True:
            try:
                data, frm = sock.recvfrom(4096)
            except OSError:
                return
            m = re.match(rb"\xff\xff\xff\xffgetIpAuthorize (-?\d+) (\d+)\.(\d+)\.(\d+)\.(\d+)", data)
            if m:
                guid = 1000 + int(m.group(5))
                sock.sendto(OOB + b"ipAuthorize %s accept 0 %d %032x" % (m.group(1), guid, guid), frm)

    threading.Thread(target=loop, daemon=True).start()


def session(a):
    os.makedirs(a.out, exist_ok=True)
    summary = {"case": a.case, "t0": a.t0, "steps": {}, "seed": a.seed}
    st = summary["steps"]
    players = [
        Player(tag, i, a.port, a.out, team, weapon)
        for i, (tag, team, weapon) in enumerate(PLAYERS.get(a.case, DEFAULT_PLAYERS))
    ]
    for p in players:  # opt-in non-LAN source addresses (plugin spec "client_addrs"; added to lo by run())
        ip = CLIENT_ADDRS.get(a.case, {}).get(p.tag)
        if ip:
            port_ = p.c.sock.getsockname()[1]
            p.c.sock.close()
            p.c.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            p.c.sock.bind((ip, port_))
    if CLIENT_ADDRS.get(a.case):
        start_fake_authorize_server()
    A = players[0]
    for f in sorted(glob.glob(os.path.join(a.basepath, "main", "*.iwd"))):  # warm the checksum cache before T0
        try:
            P.iwd_crc_blob(f)
        except (OSError, ValueError):
            pass
    if F.wait_ready(A.c, pid=a.server_pid) is None:
        summary["result"] = "server never answered getinfo"
        json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=1)
        return 2
    time.sleep(1.0)
    late = {p.tag: LATE_CONNECT.get(a.case, {})[p.tag] for p in players if p.tag in LATE_CONNECT.get(a.case, {})}
    for p in players:
        if p.tag in late:  # connects from the main loop, phase-locked to A's snapshot times (see LATE_CONNECT)
            continue
        r = p.c.oob("getchallenge", idle=0.3, total=2.0)
        m = re.match(r"challengeResponse (-?\d+)", r[0]) if r else None
        if not m:
            summary["result"] = "no challengeResponse for %s" % p.tag
            json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=1)
            return 2
        p.c.challenge = int(m.group(1))
        p.c.send_raw(
            OOB
            + (
                'connect "%s"' % userinfo(p.name, p.c.challenge, p.c.qport, USERINFO.get(a.case, {}).get(p.tag))
            ).encode(),
            "connect",
        )
        p.phase = "connecting"
        time.sleep(0.3)
    scen = SCENARIOS.get(a.case, Duel)(a.t0, players, summary)
    # optional deterministic packet loss: scen.drop_c2s(p, raw) / scen.drop_s2c(p, raw) -> True drops the in-band
    # packet (raw[:4] is the little-endian sequence, bit 31 = fragment)
    if hasattr(scen, "drop_c2s"):
        for p in players:

            def _send_raw(data, note="", _p=p, _orig=p.c.send_raw):
                if data[:4] != OOB and scen.drop_c2s(_p, data):
                    _p.log.event("dropped_c2s", seq=int.from_bytes(data[:4], "little"), note=note)
                    return
                return _orig(data, note)

            p.c.send_raw = _send_raw
    socks = {p.c.sock: p for p in players}
    pending_rcon = None
    end_t = a.t0 + a.length
    deadline = time.time() + 30 + (a.t0 + a.length) / 1000.0 + (90 + a.rotate_after / 1000.0 if a.rotate_after else 0)
    next_tick = time.time() + 0.1
    disconnected = set()
    finish_wall = None

    def send(p, note=""):
        nonlocal next_tick
        next_tick = time.time() + 0.1  # keep-alive only after 100 ms without any server message
        if p.phase in ("primed", "active"):
            p.c.send_packet(usercmds=1, cmd=p.cmd, note=note, resend=(note == "timer"))
        elif p.c.connected and p.phase == "connecting":
            p.c.send_packet(0)

    def rcon(name, cmd):
        nonlocal pending_rcon
        pending_rcon = name
        A.c.send_raw(OOB + ("rcon %s %s" % (a.rcon, cmd)).encode(), "oob")

    waiting_since = {}
    last_enter = {}

    def server_now():
        return max((q.c.latest_server_time or 0) for q in players)

    def flush_enters():
        """Staggered (re)join: once every connecting client has the new gamestate (or after 3 s), the
        clients enter one at a time in player order, each after the previous one has become active, so the order of
        ClientBegin (J lines, MP_CONNECTED, client numbers) never depends on packet timing."""
        for ep in sorted(waiting_since):
            # clients that are connecting (or already have any gamestate) take part; late connectors that have not
            # started yet do not hold the others back
            live = [q for q in players if q.tag not in disconnected and (q.gs_seen >= 1 or q.phase == "connecting")]
            ready = all(q.gs_seen >= ep for q in live)
            if not ready and time.time() - waiting_since[ep] < 3.0:
                continue
            for q in players:  # player order
                if q.epoch != ep or q.tag in disconnected:
                    continue
                if q.phase == "primed":
                    srv, wall = last_enter.get(ep, (0, 0.0))
                    valve = (srv > 0 and server_now() >= srv + 2000) or time.time() >= wall + 10.0
                    if not valve:
                        break  # previous client entered but is not active yet (valve: 2000 ms server time, 10 s wall)
                if q.phase == "awaiting_enter":
                    q.phase = "primed"
                    send_cp(q, a.basepath)
                    q.c.send_packet(usercmds=1, note="cp+enter")
                    q.log.event("enter_flushed", epoch=ep, all_ready=ready)
                    last_enter[ep] = (server_now(), time.time())
                    break
            if not any(q.phase == "awaiting_enter" and q.epoch == ep for q in players):
                del waiting_since[ep]

    def inband(p, data):
        """Process one in-band server packet for player p (after loss/hold decisions)."""
        nonlocal finish_wall
        info = p.c.handle_inband(data)
        if info is None:
            return
        p.log.event(
            "server_message", **{k: v for k, v in info.items() if k != "snapshot"}, snapshot=info.get("snapshot")
        )
        if len(p.c.gamestates) > p.gs_seen:
            p.gs_seen = len(p.c.gamestates)
            p.epoch = p.gs_seen
            if p.epoch == 1:
                p.anchor = a.t0
            else:  # map change: re-anchor on the new level's start time (configstring 13)
                try:
                    p.anchor = int(p.c.gamestate["configstrings"].get(13, "0") or 0)
                except ValueError:
                    p.anchor = 0
                p.log.event("rotation", epoch=p.epoch, anchor=p.anchor)
            # initial connect and map change alike: hold the enter until every connected client has this gamestate,
            # then they enter one at a time in player order (flush_enters), so ClientBegin order never depends on
            # socket read order or harness speed
            p.phase = "awaiting_enter"
            waiting_since.setdefault(p.epoch, time.time())
            return
        snap = info.get("snapshot")
        if not snap:
            send(p)
            return
        p.record(info["seq"])
        if not (snap["snapFlags"] & 2) and p.phase == "primed":
            p.phase = "active"
            p.active_at = snap["serverTime"]
        t = snap["serverTime"] - p.anchor
        if p.phase == "active":
            acts = scen.actions(p, t) if p.epoch == 1 else scen.actions2(p, t)
            for text in acts:
                p.c.add_command(text)
                p.cmd_log.append({"t": snap["serverTime"], "epoch": p.epoch, "rel": t, "cmd": text})
            if p.epoch == 1:
                scen.usercmd(p, t)
            else:
                scen.usercmd2(p, t)
            # test-client case: add the bots at a fixed server time, after both fake clients are connected, so
            # the clients' connect-time rand() calls (getchallenge) never interleave with the bots' usercmds
            if a.case in BOT_RCON and p is A and p.epoch == 1 and t >= BOT_RCON[a.case] and "bots" not in A.done:
                A.done.add("bots")
                rcon("rcon_set_testclients", "set scr_testclients 2")
        send(p)
        # end of the scripted part: B then A disconnect, both driven by A's server time (rotation cases: by A's
        # time relative to the second level's start)
        if a.rotate_after:
            at = t if (p is A and p.epoch >= 2) else None
            end_rel = a.rotate_after
        else:
            at = snap["serverTime"] if p is A else None
            end_rel = end_t
        if at is not None and at >= end_rel and "end" not in A.done:
            A.done.add("end")
            rcon("rcon_status_end", "status")
        if at is not None and at >= end_rel + 1000 and "discB" not in A.done:
            A.done.add("discB")
            for q_ in players[1:]:
                q_.c.add_command("disconnect")
                for _ in range(3):
                    q_.c.send_packet(1, cmd=q_.cmd, note="disconnect")
                disconnected.add(q_.tag)
                q_.frozen_cmds = dict(q_.c.server_cmds)
        if at is not None and at >= end_rel + 2000 and "discA" not in A.done:
            A.done.add("discA")
            A.c.add_command("disconnect")
            for _ in range(3):
                A.c.send_packet(1, cmd=A.cmd, note="disconnect")
            disconnected.add("A")
            A.frozen_cmds = dict(A.c.server_cmds)
            finish_wall = time.time() + 2.0

    while time.time() < deadline:
        for p in players:  # late connects: getchallenge right after A's snapshot of server time >= the case's threshold
            if (
                p.tag in late
                and p.phase == "idle"
                and "gc" not in p.done
                and (A.c.latest_server_time or 0) >= late[p.tag]
            ):
                p.done.add("gc")
                p.c.send_raw(OOB + b"getchallenge", "oob")
        r_, _, _ = select.select(list(socks), [], [], max(0.0, next_tick - time.time()))
        if not r_:
            next_tick = time.time() + 0.1
            if a.server_pid:
                try:
                    os.kill(a.server_pid, 0)
                except ProcessLookupError:
                    summary["result"] = "server exited during gameplay"
                    break
            for p in players:
                if p.tag not in disconnected:
                    send(p, "timer")
            flush_enters()
            if finish_wall and time.time() > finish_wall:
                break
            continue
        for s in r_:
            p = socks[s]
            data, _ = s.recvfrom(0x20000)
            p.log.packet("s2c", data)
            if data[:4] == OOB:
                txt = F.oob_text(data)
                m_ = re.match(r"challengeResponse (-?\d+)", txt)
                if p.tag in late and p.phase == "idle" and m_:
                    p.c.challenge = int(m_.group(1))
                    p.c.send_raw(
                        OOB
                        + (
                            'connect "%s"'
                            % userinfo(p.name, p.c.challenge, p.c.qport, USERINFO.get(a.case, {}).get(p.tag))
                        ).encode(),
                        "connect",
                    )
                    p.phase = "connecting"
                    continue
                if txt.startswith("print\n") and pending_rcon and p is A:
                    st.setdefault(pending_rcon, []).append(txt)
                else:
                    p.log.event("oob_inband", text=txt)
                # Every scenario is clocked by A's snapshots. A rejected initial connection cannot advance.
                if p is A and p.phase == "connecting" and not p.gs_seen and txt.startswith("error\n"):
                    summary["result"] = txt
                    deadline = time.time()
                continue
            if p.tag in disconnected:
                continue
            if hasattr(scen, "drop_s2c") and scen.drop_s2c(p, data):
                p.log.event("dropped_s2c", seq=int.from_bytes(data[:4], "little"))
                continue
            # optional deterministic delay/reordering: scen.hold_s2c(p, raw) -> None, or (kind, value[, "after"]) with
            # kind "seq" (release when a later packet with sequence >= value arrives) or "time" (when the player's
            # latest snapshot server time >= value, checked on every later packet). Held packets are delivered
            # before the releasing packet, or after it (= reordered) with "after".
            if hasattr(scen, "hold_s2c"):
                seq_now = int.from_bytes(data[:4], "little") & 0x7FFFFFFF
                hold_now = scen.hold_s2c(p, data)
                held_s2c = p.held_s2c
                if hold_now:
                    held_s2c.append((data, hold_now))
                    p.log.event("held_s2c", seq=seq_now, release=list(hold_now))
                    continue

                def _due(h):
                    kind, val = h[1][0], h[1][1]
                    return seq_now >= val if kind == "seq" else (p.c.latest_server_time or 0) >= val

                for h in [h for h in held_s2c if _due(h) and not (len(h[1]) > 2 and h[1][2] == "after")]:
                    held_s2c.remove(h)
                    p.log.event("released_s2c", seq=int.from_bytes(h[0][:4], "little") & 0x7FFFFFFF, before=seq_now)
                    inband(p, h[0])
                inband(p, data)
                for h in [h for h in held_s2c if _due(h) and len(h[1]) > 2 and h[1][2] == "after"]:
                    held_s2c.remove(h)
                    p.log.event("released_s2c", seq=int.from_bytes(h[0][:4], "little") & 0x7FFFFFFF, after=seq_now)
                    inband(p, h[0])
                continue
            inband(p, data)
        flush_enters()
        if finish_wall and time.time() > finish_wall:
            break
    pending_rcon = None
    st["rcon_status_after"] = A.c.oob("rcon %s status" % a.rcon, idle=0.3, total=1.5)
    time.sleep(0.5)
    st["rcon_quit"] = A.c.oob("rcon %s quit" % a.rcon, idle=0.5, total=2.0)
    for p in players:
        c = p.c
        summary["client" + p.tag] = {
            "name": p.name,
            "phase": p.phase,
            "active_at": p.active_at,
            "clientNum": c.client_num,
            "challenge": c.challenge,
            "gamestate": {k: v for k, v in (c.gamestates[0] if c.gamestates else {}).items()},
            "gamestates_later": [{k: v for k, v in g.items()} for g in c.gamestates[1:]],
            # frozen at the client's own disconnect: later in-band packets (read by the final rcon OOB exchange)
            # arrive at wall-clock-dependent times
            "server_commands": {
                str(k): v.decode("latin1") for k, v in sorted(getattr(p, "frozen_cmds", c.server_cmds).items())
            },
            "client_commands": [{"seq": s_, "text": t_} for s_, t_ in c.client_cmds],
            "cmd_log": p.cmd_log,
            "message_errors": [m.get("error") for m in c.messages if m.get("error")],
            "records": p.records,
        }
    json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"), indent=0, sort_keys=True)
    print(
        " | ".join(
            "%s: phase=%s records=%d cmds=%d" % (p.tag, p.phase, len(p.records), len(p.c.server_cmds)) for p in players
        )
    )
    return 2 if "result" in summary else 0


def send_cp(p, basepath):
    c = p.c
    sysinfo = c.gamestate["configstrings"].get(1, "")
    names = re.search(r"\\sv_referencedIwdNames\\([^\\]*)", sysinfo)
    names = names.group(1).split() if names else []
    sums = []
    for nm in names:
        try:
            # pure checksum only (keyed MD4 over the cached CRC blob, about 2 ms per iwd)
            sums.append(P.block_checksum(P.iwd_crc_blob(os.path.join(basepath, nm + ".iwd")), c.checksum_feed))
        except OSError:
            pass
    chk = c.checksum_feed
    for s_ in sums:
        chk ^= s_ & 0xFFFFFFFF
    chk ^= len(sums)
    c.add_command("cp @ " + "".join("%d " % s_ for s_ in sums) + "%d" % P.s32(chk))


# =============================================================================================
# sandbox runner
# =============================================================================================
ROTATE = {"rotate_dm_toujane": 10000}
CLIENT_ADDRS = {}  # case -> {player tag: source IP}; non-loopback addresses are added to lo inside the namespace
LATE_CONNECT = {}  # case -> {player tag: A's server time (ms) after which that player starts its connect}. Needed for
# clients whose snapshot interval is a multiple of the 50 ms frame (non-LAN, snaps < 20): their snapshot
# phase is the parity of the server tick that handled the connect, which is wall-clock luck otherwise.
USERINFO = {}  # case -> {player tag: {userinfo key: value}} overrides of the connect userinfo
MODS["bots_dm_toujane"] = lambda home, data: (
    "test overlay dm.gsc sha256 %s" % make_test_client_mod(home, os.path.join(data, "iw_15.iwd"))
)
BOT_RCON = {
    "bots_dm_toujane": -5000
}  # relative to T0  # cases with a map rotation: end 10 s after the second level starts
CASES = {
    # devmap: sv_cheats 1 (setviewpos is cheat-protected); g_log for the J/D/K/Q game log
    "duel_dm_toujane": [
        "+set",
        "g_gametype",
        "dm",
        "+set",
        "g_log",
        "games_mp.log",
        "+set",
        "g_logSync",
        "1",
        "+devmap",
        "mp_toujane",
    ],
    # the duel, then the 75 s time limit ends the map: intermission (wait 10), exitLevel -> map_rotate -> mp_carentan
    "rotate_dm_toujane": [
        "+set",
        "g_gametype",
        "dm",
        "+set",
        "g_log",
        "games_mp.log",
        "+set",
        "g_logSync",
        "1",
        "+set",
        "scr_dm_timelimit",
        "1.25",
        "+set",
        "sv_mapRotation",
        "gametype dm map mp_carentan",
        "+devmap",
        "mp_toujane",
    ],
    # option B: server-side test clients through a loose-file test overlay of dm.gsc (make_test_client_mod)
    "bots_dm_toujane": [
        "+set",
        "fs_game",
        "mods/gptest",
        "+set",
        "scr_testclients",
        "0",
        "+set",
        "g_gametype",
        "dm",
        "+set",
        "g_log",
        "games_mp.log",
        "+set",
        "g_logSync",
        "1",
        "+map",
        "mp_toujane",
    ],
}


def run(a):
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.abspath(os.path.join(here, "../../../.."))
    cod2 = os.path.abspath(os.environ.get("COD2_LOCAL", os.path.join(repo, ".local/cod2")))
    oracle = os.path.join(cod2, "oracle")
    data_main = os.environ.get("COD2_DATA_MAIN")
    data = os.path.abspath(data_main) if data_main else ""
    if a.case not in CASES:
        sys.exit("unknown case %s" % a.case)
    if a.server == "rust":
        if a.gdb_extra:
            sys.exit("--gdb-extra is only supported for i386 servers")
        binp = os.path.abspath(os.environ.get("COD2_RUST_BIN", os.path.join(repo, "target/release/cod2-server")))
        if not os.access(binp, os.X_OK):
            sys.exit("missing Rust server executable: %s" % binp)
        out = os.path.join(cod2, "rust-run", "captures", "gameplay_" + a.case + a.tag, "run%d" % a.run)
        ldpath = ""
    elif a.server == "original":
        binp = os.path.join(cod2, "lnxded-1.3/cod2_lnxded")
        if (
            hashlib.sha256(open(binp, "rb").read()).hexdigest()
            != "05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7"
        ):
            sys.exit("server binary hash mismatch")
        out = os.path.join(oracle, "captures", "gameplay_" + a.case + a.tag, "run%d" % a.run)
        ldpath = os.path.join(cod2, "runtime-i386/usr/lib/i386-linux-gnu")
        bp = G_INITGAME["original"]
        spawn_bp = SV_SPAWNSERVER["original"]
    else:
        binp = os.environ.get("RECON_BIN", os.path.join(cod2, "build/cod2_lnxded_recon"))
        out = os.path.join(cod2, "recon-run", "captures", "gameplay_" + a.case + a.tag, "run%d" % a.run)
        ldpath = ""
        nm = subprocess.run(["nm", binp], capture_output=True, text=True).stdout
        m = re.search(r"^([0-9a-f]+) T _Z10G_InitGameijii$", nm, re.M)
        bp = "0x" + m.group(1) if m else None
        m = re.search(r"^([0-9a-f]+) T _Z14SV_SpawnServerPc$", nm, re.M)
        spawn_bp = "0x" + m.group(1) if m else None
    if not data or not os.path.isdir(data):
        sys.exit("set COD2_DATA_MAIN to the original game's main directory")
    shim = os.path.join(oracle, "lib/stat32_shim.so")
    for d in ("lib", "run/base/main", "run/home"):
        os.makedirs(os.path.join(oracle, d), exist_ok=True)
    if a.server != "rust":
        if not os.path.exists(shim) or os.path.getmtime(os.path.join(here, "stat32_shim.c")) > os.path.getmtime(shim):
            subprocess.check_call(
                ["gcc", "-m32", "-shared", "-fPIC", "-O2", os.path.join(here, "stat32_shim.c"), "-o", shim]
            )
        for f in glob.glob(os.path.join(data, "*.iwd")):
            dst = os.path.join(oracle, "run/base/main", os.path.basename(f))
            if not os.path.lexists(dst):
                os.symlink(f, dst)
    # unique per server and tag: g_log appends to <home>/main/games_mp.log, so concurrent runs must not share it
    home = os.path.join(oracle, "run/home", "gameplay_%s%s-%s-run%d" % (a.case, a.tag, a.server, a.run))
    subprocess.call(["rm", "-rf", out, home])
    os.makedirs(out)
    os.makedirs(home)
    if a.case in MODS:
        open(os.path.join(out, "harness.log"), "a").write("%s\n" % MODS[a.case](home, data))
    for k, v in DEFAULTS.get(a.case, {}).items():
        if getattr(a, k) is None:
            setattr(a, k, v)
    a.t0 = 20000 if a.t0 is None else a.t0
    a.length = 40000 if a.length is None else a.length
    # A Rust loopback-only listener disables master services. A non-LAN case reaches its
    # simulated authorization peer on a second lo address inside the isolated namespace.
    bind_ip = "0.0.0.0" if a.server == "rust" and CLIENT_ADDRS.get(a.case) else "127.0.0.1"
    srv = (
        [
            "+set",
            "dedicated",
            "1",
            "+set",
            "net_ip",
            bind_ip,
            "+set",
            "net_port",
            "28960",
            "+set",
            "sv_maxclients",
            "8",
            "+set",
            "rcon_password",
            RCON,
            "+set",
            "sv_punkbuster",
            "0",
            "+set",
            "sv_pure",
            "1",
            "+set",
            "fs_basepath",
            os.path.join(oracle, "run/base"),
            "+set",
            "fs_homepath",
            home,
        ]
        + CASES[a.case]
        + list(a.extra)
    )
    q = lambda s: "'" + s.replace("'", "'\\''") + "'"  # noqa: E731
    env = (
        "" if a.server == "rust" else "LD_PRELOAD=%s" % q(shim) + (" LD_LIBRARY_PATH=%s" % q(ldpath) if ldpath else "")
    )
    seed = None if a.seed == "none" else int(a.seed)
    rot = a.rotate_after if a.rotate_after is not None else ROTATE.get(a.case, 0)
    if a.server == "rust":
        if seed is not None:
            srv[:0] = ["--game-seed", str(seed)]
            if rot and not a.no_spawn_pad:
                srv[:0] = ["--map-load-ms", str(round(SPAWN_PAD_S * 1000))]
        launch = "%s %s < %s/stdin.fifo > %s/server.log 2>&1 &" % (q(binp), " ".join(q(x) for x in srv), q(out), q(out))
    elif seed is not None:
        if not bp:
            sys.exit("no G_InitGame address for %s" % binp)
        gdbx = os.path.join(out, "seed.gdb")
        with open(gdbx, "w") as fh:
            fh.write(
                "set pagination off\nset confirm off\nhandle SIGPIPE nostop noprint pass\n"
                "set exec-wrapper env %s\n"
                % env
                + "break *%s\ncommands\nsilent\n" % bp
                + 'printf "SEEDPIN levelTime=%%d seed=%%u -> %d restart=%%d\\n", *(int*)($esp+4), '
                "*(unsigned*)($esp+8), *(int*)($esp+12)\n"
                % seed
                + "set var *(unsigned*)($esp+8) = %d\ncontinue\nend\n" % seed
                + (PAD_GDB % {"pad": SPAWN_PAD_S, "addr": spawn_bp} if rot and not a.no_spawn_pad else "")
                + (open(a.gdb_extra).read() if a.gdb_extra else "")
                + "run %s < %s/stdin.fifo > %s/server.log 2>&1\n" % (" ".join(q(x) for x in srv), q(out), q(out))
                +
                # reached only when the inferior stops (signal, or the harness's SIGINT on a hang)
                'if $_isvoid($_exitcode)\nprintf "HANG-OR-SIGNAL backtrace\\n"\nbt 40\ninfo registers\n'
                "x/12i $pc-16\nkill\nend\n"
            )
        launch = "gdb -q -batch -x %s %s > %s/gdb.log 2>&1 &" % (q(gdbx), q(binp), q(out))
    else:
        launch = "env %s %s %s < %s/stdin.fifo > %s/server.log 2>&1 &" % (
            env,
            q(binp),
            " ".join(q(x) for x in srv),
            q(out),
            q(out),
        )
    inner = """#!/usr/bin/env bash
set -u
cd %(bindir)s
{ echo "# /proc/net/dev"; tail -n +3 /proc/net/dev | cut -d: -f1 | tr -d ' '; echo "# routes"; tail -n +2 /proc/net/route; } > %(out)s/isolation.txt
%(addrs)smkfifo %(out)s/stdin.fifo
sleep 900 > %(out)s/stdin.fifo &
KEEP=$!
%(launch)s
SRV=$!
python3 -I %(me)s session --port 28960 --out %(out)s --case %(case)s --rcon %(rcon)s --basepath %(base)s --t0 %(t0)d --length %(len)d --seed %(seed)s --rotate-after %(rot)d --server-pid $SRV > %(out)s/client.log 2>&1
RC=$?
for i in $(seq 1 50); do kill -0 $SRV 2>/dev/null || break; sleep 0.2; done
if kill -0 $SRV 2>/dev/null; then echo "server did not exit after rcon quit; interrupting for a backtrace, then killing" >> %(out)s/harness.log; pkill -INT -f -- %(bin_regex)s; for i in $(seq 1 25); do kill -0 $SRV 2>/dev/null || break; sleep 0.2; done; pkill -9 -f -- %(bin_regex)s; kill -9 $SRV 2>/dev/null; fi
wait $SRV 2>/dev/null; echo "server_exit_status=$?" >> %(out)s/harness.log
kill $KEEP 2>/dev/null; rm -f %(out)s/stdin.fifo
cp %(home)s/main/games_mp.log %(out)s/games_mp.log 2>/dev/null || cp "$(find %(home)s -name games_mp.log | head -1)" %(out)s/games_mp.log 2>/dev/null
exit $RC
""" % {
        "bindir": q(os.path.dirname(binp) if a.server == "rust" else os.path.join(cod2, "lnxded-1.3")),
        "out": q(out),
        "launch": launch,
        "me": q(os.path.abspath(__file__)),
        "case": q(a.case),
        "base": q(os.path.join(oracle, "run/base")),
        "rcon": RCON,
        "t0": a.t0,
        "len": a.length,
        "seed": str(seed) if seed is not None else "none",
        "bin_regex": q("^" + re.escape(binp) + "( |$)"),
        "home": q(home),
        "rot": rot,
        "addrs": "".join(
            "ip addr add %s/32 dev lo\n" % ip
            for ip in sorted(
                set(CLIENT_ADDRS.get(a.case, {}).values()) | ({AUTH_IP} if CLIENT_ADDRS.get(a.case) else set())
            )
        ),
    }
    fd, path = tempfile.mkstemp(prefix="gp-inner.", suffix=".sh", dir=os.path.join(oracle, "run"))
    os.write(fd, inner.encode())
    os.close(fd)
    try:
        bw = [
            "bwrap",
            "--ro-bind",
            "/",
            "/",
            "--dev",
            "/dev",
            "--proc",
            "/proc",
            "--bind",
            os.path.join(oracle, "run"),
            os.path.join(oracle, "run"),
            "--bind",
            out,
            out,
            "--unshare-net",
            "--unshare-pid",
            "--unshare-ipc",
            "--die-with-parent",
            "bash",
            path,
        ]
        if a.server == "rust":
            # Expose the same IWD-only base as the original. The native VFS rejects
            # loose-file symlinks, so mount each archive read-only in an empty directory.
            main_dir = os.path.join(oracle, "run/base/main")
            mounts = ["--tmpfs", main_dir]
            for path_ in sorted(glob.glob(os.path.join(data, "*.iwd"))):
                mounts.extend(["--ro-bind", path_, os.path.join(main_dir, os.path.basename(path_))])
            mounts.extend(["--remount-ro", main_dir])
            i = bw.index("--unshare-net")
            bw[i:i] = mounts
        if CLIENT_ADDRS.get(a.case):  # AUTH_HOST resolves to the fake authorize server (start_fake_authorize_server)
            hosts = os.path.join(out, "hosts")
            with open(hosts, "w") as fh:
                fh.write("127.0.0.1 localhost\n%s %s\n" % (AUTH_IP, AUTH_HOST))
            bw[bw.index("--unshare-net") : bw.index("--unshare-net")] = ["--ro-bind", hosts, "/etc/hosts"]
        if CLIENT_ADDRS.get(
            a.case
        ):  # `ip addr add` on lo needs CAP_NET_ADMIN over the namespace: map to a user ns root
            bw[bw.index("--unshare-net") : bw.index("--unshare-net")] = [
                "--unshare-user",
                "--uid",
                "0",
                "--gid",
                "0",
                "--cap-add",
                "CAP_NET_ADMIN",
            ]
        rc = subprocess.call(bw)
        if rc:
            open(os.path.join(out, "harness.log"), "a").write("inner exit status %d\n" % rc)
    finally:
        os.unlink(path)
    with open(os.path.join(out, "SHA256SUMS"), "w") as fh:
        for f in sorted(glob.glob(os.path.join(out, "**/*"), recursive=True)):
            if os.path.basename(f) != "SHA256SUMS" and os.path.isfile(f):
                fh.write("%s  %s\n" % (hashlib.sha256(open(f, "rb").read()).hexdigest(), os.path.relpath(f, out)))
    print("done: %s" % out)
    client_log = os.path.join(out, "client.log")
    if os.path.exists(client_log):
        print(open(client_log).read().strip())
    return rc


# =============================================================================================
# comparison
# =============================================================================================
def items_for_run(d):
    s = json.load(open(os.path.join(d, "summary.json")))
    root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../.."))
    it = {}

    def put(k, v):
        it[k] = v if isinstance(v, str) else json.dumps(v, sort_keys=True)

    t0 = s.get("t0", 0)
    for tag in sorted(k[6:] for k in s if k.startswith("client")):
        c = s.get("client" + tag)
        if not c:
            continue
        put("%s:phase" % tag, c["phase"])
        put("%s:clientNum" % tag, c["clientNum"])
        put("%s:message_errors" % tag, c["message_errors"])
        for k, v in c["server_commands"].items():
            if v.startswith("b "):  # scoreboard: " <client> <score> <ping> <deaths> <killcam>" per client
                w = v.split()
                for i in range(4 + 2, len(w), 5):
                    w[i] = "<PING>"
                v = " ".join(w)
            put("%s:server_command:%04d" % (tag, int(k)), v)
        put("%s:cmd_log" % tag, [(x.get("epoch", 1), x.get("rel", x["t"] - t0), x["cmd"]) for x in c["cmd_log"]])
        for g, gl in enumerate(c.get("gamestates_later") or [], 2):
            if not isinstance(gl, dict):
                continue
            for k in ("reliableSequence", "clientNum", "serverId", "fragments"):
                put("%s:gamestate%d:%s" % (tag, g, k), gl.get(k))
            for idx, v in sorted((gl.get("configstrings") or {}).items(), key=lambda kv: int(kv[0])):
                put("%s:gamestate%d:configstring:%04d" % (tag, g, int(idx)), v)
            for n, words in (gl.get("baselines") or {}).items():
                put("%s:gamestate%d:baseline:%04d" % (tag, g, int(n)), P.state_hash(words))
        for r in c["records"]:
            ep = r.get("epoch", 1)
            rel = r.get("rel", r["t"] - t0) if ep > 1 else r["t"] - t0
            if ep == 1 and rel < -2000:  # connect phase: timing-dependent by construction
                continue
            key = "%s:t%+06d" % (tag, rel) if ep == 1 else "%s:e%d:t%+06d" % (tag, ep, rel)
            ps = dict(r["ps"])
            if "commandTime" in ps:
                ps["commandTime"] -= r["t"]
            for f in sorted(ps):
                put(key + ":ps:" + f, ps[f])
            put(key + ":ps_fields", sorted(ps))
            put(key + ":ps_hash_rel", r.get("ps_hash_rel"))
            for n, f in sorted(r["ents"].items(), key=lambda kv: int(kv[0])):
                put(key + ":ent%04d" % int(n), f)
            put(key + ":ents_removed", r["ents_removed"])
            put(key + ":num_entities", r["num_entities"])
            put(key + ":clients", r["clients"])
    for k, v in sorted(s.get("steps", {}).items()):
        txt = json.dumps(v)
        # status rows: num score ping guid name lastmsg address qport rate -> ping and lastmsg are client timing
        txt = re.sub(r"(\\n\s*\d+\s+-?\d+\s+)\d+(\s+\d+ \S+\s+)\d+( 127)", r"\1<PING>\2<LASTMSG>\3", txt)
        # zombie rows (slot freed after a disconnect, ping column "ZMBI"): lastmsg is wall-clock (1950/2000)
        txt = re.sub(r"(\\n\s*\d+\s+-?\d+\s+ZMBI\s+\d+ \S*\s+)\d+( 127)", r"\1<LASTMSG>\2", txt)
        put("oob:" + k, txt)
    put("scenario_notes", s.get("scenario_notes"))
    for name in ("server.log", "games_mp.log"):
        f = os.path.join(d, name)
        if os.path.exists(f):
            with open(f, encoding="latin1") as stream:
                lines = [line.rstrip("\n") for line in stream]
            # The stock search-path listing records the absolute scratch directory. Captures can
            # move between CI workspaces, so use the path recorded at capture time for each log.
            scratch = next(
                (
                    line.split("/oracle/run/", 1)[0]
                    for line in lines
                    if line.startswith("/") and "/oracle/run/home/gameplay_" in line
                ),
                None,
            )
            if scratch:
                lines = [line.replace(scratch + "/oracle/run/", "<ORACLE>/run/") for line in lines]
            lines = [
                re.sub(r"/run/home/gameplay_[^/\s]*", "/run/home/<HOME>", F.normalize_log_line(line, root))
                for line in lines
            ]
            lines = [line for line in lines if not line.startswith("Hitch warning")]
            if name == "games_mp.log":
                # J (join) is logged at the client's 'begin', i.e. at the wall-clock connect time before T0
                lines = [re.sub(r"^\s*\d+:\d\d (J;)", r"<CONNECT-TIME> \1", line) for line in lines]
            for i, line in enumerate(lines):
                put("%s:%04d" % (name, i), line)
    return it


EPOCH_KEY = re.compile(r"^([A-Z]):e(\d+):t([+-]\d+):")


def map_change_cutoffs(runs_items):
    """Map-load boundary (timing family). After a map change each client becomes active again at a wall-clock-
    dependent server frame (original runs differ by several 50 ms frames), and its first record there diffs against the
    previous epoch. Per client and epoch >= 2 the comparison therefore starts one record after the latest first record
    over ALL runs (original and other): from then on every run has the client active and recording each snapshot."""
    cut = {}
    for items in runs_items:
        first = {}
        for k in items:
            m = EPOCH_KEY.match(k)
            if m:
                key = (m.group(1), int(m.group(2)))
                first[key] = min(first.get(key, 10**9), int(m.group(3)))
        for key, v in first.items():
            cut[key] = max(cut.get(key, -(10**9)), v)
    return cut


def drop_before_cutoff(items, cut):
    out = {}
    for k, v in items.items():
        m = EPOCH_KEY.match(k)
        if m and int(m.group(3)) <= cut.get((m.group(1), int(m.group(2))), -(10**9)):
            continue
        out[k] = v
    return out


def compare(a):
    runs = sorted(glob.glob(os.path.join(a.case_dir, "run*")))
    data = [items_for_run(r) for r in runs if os.path.exists(os.path.join(r, "summary.json"))]
    odata = []
    if a.other:
        oruns = sorted(glob.glob(os.path.join(a.other, "run*")))
        odata = [items_for_run(r) if os.path.exists(os.path.join(r, "summary.json")) else {} for r in oruns]
    cut = map_change_cutoffs(data + [o for o in odata if o])
    data = [drop_before_cutoff(d, cut) for d in data]
    odata = [drop_before_cutoff(o, cut) for o in odata]
    rep = {
        "runs": [os.path.basename(r) for r in runs],
        "items": F.classify(data),
        "map_change_cutoffs": {"%s:e%d" % k: v for k, v in sorted(cut.items())},
    }
    counts = {}
    for v in rep["items"].values():
        counts[v["status"]] = counts.get(v["status"], 0) + 1
    rep["counts"] = counts
    rc = 0
    if a.other:
        diff = {}
        for k, v in rep["items"].items():
            if v["status"] != "identical":
                continue
            ovals = [o.get(k) for o in odata] or [None]
            # classify truncates its printable sample to 200 characters. Use the complete reference item for equality.
            bad = [ov for ov in ovals if ov != data[0][k]]
            if bad:
                diff[k] = {
                    "original": v["value"][:160],
                    "other": [ov[:160] if ov else ov for ov in ovals],
                    "missing": any(ov is None for ov in ovals),
                }
                reference = data[0][k]
                if all(ov is not None and ov[:60] == reference[:60] for ov in bad):
                    first_difference = min(len(os.path.commonprefix([reference, ov])) for ov in bad)
                    start = max(0, first_difference - 24)
                    diff[k]["context"] = {
                        "offset": start,
                        "original": reference[start : start + 60],
                        "other": [ov[start : start + 60] for ov in ovals],
                    }
        extra = sorted(set().union(*[o.keys() for o in odata]) - set(rep["items"])) if odata else []
        for k in extra:
            diff[k] = {"original": None, "other": [(o.get(k) or "")[:160] or None for o in odata], "missing": False}
        rep["other_dir"] = a.other
        rep["other_mismatches_on_deterministic_items"] = diff
        rc = 1 if diff else 0
    if a.counts_json:
        with open(a.counts_json, "w") as fh:
            json.dump({"reference_items": counts, "mismatches": len(diff) if a.other else None}, fh)
    if a.json:
        json.dump(rep, open(a.json, "w"), indent=1, sort_keys=True)
    if a.verbose:
        for k, v in rep["items"].items():
            if v["status"] != "identical":
                print("%-48s %s %s" % (k, v["status"], v.get("values")))
    print("counts:", counts)
    if a.other:
        d = rep["other_mismatches_on_deterministic_items"]
        print(
            "mismatches vs other on deterministic items: %d (missing in other: %d, extra in other: %d)"
            % (len(d), sum(1 for v in d.values() if v["missing"]), sum(1 for v in d.values() if v["original"] is None))
        )
        for k, v in list(d.items())[:80]:
            sample = v.get("context", v)
            label = "%s [%d:]" % (k, sample["offset"]) if "offset" in sample else k
            print(
                "  %-46s orig=%r other=%r"
                % (label, (sample["original"] or "")[:60], [x[:60] if x else x for x in sample["other"]])
            )
    return rc


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    r = sp.add_parser("run")
    r.add_argument("case")
    r.add_argument("run", type=int, nargs="?", default=1)
    r.add_argument("--server", choices=("original", "recon", "rust"), default="original")
    r.add_argument("--seed", default=str(DEFAULT_SEED), help="pinned game seed, or 'none'")
    r.add_argument(
        "--t0", type=int, default=None, help="absolute server time (ms) of the scenario start (default 20000)"
    )
    r.add_argument("--length", type=int, default=None, help="scenario length after t0 (ms, default 40000)")
    r.add_argument("--gdb-extra", help="gdb commands appended before 'run' (probes; requires a pinned seed)")
    r.add_argument(
        "--rotate-after", type=int, default=None, help="rotation cases: end this long after the 2nd level start"
    )
    r.add_argument("--no-spawn-pad", action="store_true", help="rotation cases: do not pad the 2nd SV_SpawnServer")
    r.add_argument("--tag", default="", help="suffix for the capture directory name")
    r.add_argument("extra", nargs="*")
    r.set_defaults(fn=run)
    s = sp.add_parser("session")
    s.add_argument("--port", type=int, default=28960)
    s.add_argument("--out", required=True)
    s.add_argument("--case", required=True)
    s.add_argument("--rcon", default=RCON)
    s.add_argument("--server-pid", type=int, default=0)
    s.add_argument("--basepath", required=True)
    s.add_argument("--t0", type=int, default=20000)
    s.add_argument("--length", type=int, default=40000)
    s.add_argument("--seed", default="none")
    s.add_argument("--rotate-after", type=int, default=0)
    s.set_defaults(fn=session)
    c = sp.add_parser("compare")
    c.add_argument("case_dir")
    c.add_argument("other", nargs="?")
    c.add_argument("--json")
    c.add_argument("--counts-json", help="write summary counts without the full per-field report")
    c.add_argument("-v", "--verbose", action="store_true")
    c.set_defaults(fn=compare)
    a = ap.parse_args()
    load_plugins()
    sys.exit(a.fn(a))


if __name__ == "__main__":
    main()
