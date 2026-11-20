"""Per-function overrides for difftest.py.

DEFAULT_INIT: ORIGINAL functions called once in the runner (before any case is forked), as
(address, [32-bit argument words]). They reproduce startup state that many leaf helpers
assume:
  0x080b805e Com_SetThreadBlocks(0)  registers the main thread's va()/slot 2/slot 3 blocks
             (inferred: the server calls it for thread 0 during startup; without it va()
             dereferences a NULL per-thread block).
  0x080b7956 Swap_Init()             fills the byte-order dispatch slots 0x085bc800..818.

SPECS[addr] keys (all optional):
  skip            reason string -> verdict "unsupported"
  signature       override the recorded signature
  args            {param name or index: generator(rng, objs, fixes, words, ctx, prev)}
  init            extra global ORIGINAL calls [(addr, [words])]
  pre             ORIGINAL calls made per case before the fork of both sides
  mem             [(addr, bytes)] or callable(rng) -> list, written into the image per case
  buf_caps        capacity choices for writable char * buffers
  null_prob       probability of passing NULL for const char * parameters
  strict_imports  import-trace differences fail the case instead of being noted
  timeout         per-side seconds (default 5)
"""

DEFAULT_INIT = [
    (0x080B805E, [0]),
    (0x080B7956, []),
]


def _info_buf(cap):
    def gen(rng, objs, fixes, words, ctx, prev):
        from difftest import gen_string  # late import: difftest imports this module

        s = gen_string(rng, "info", "Info_", prev)[: cap - 1]
        prev.append(s)
        objs.append((max(cap, len(s) + 1), s))
        ctx["last_cap"], ctx["last_len"] = cap, len(s)
        words.append(("o", len(objs) - 1))

    return gen


SPECS = {
    # chunk 030 -------------------------------------------------------------
    # Info_SetValueForKey / _Big write into s (MAX_INFO_STRING 0x400 / 0x2000 in the
    # original's checks); give s a buffer of that capacity holding an info string.
    0x080B85CE: {"args": {"s": _info_buf(0x400)}},
    0x080B8802: {"args": {"s": _info_buf(0x2000)}},
    0x080B8318: {"args": {"s": _info_buf(0x400)}},
    0x080B844A: {"args": {"s": _info_buf(0x2000)}},
    # Com_SetThreadBlocks(threadIndex): only small indices are meaningful.
    0x080B805E: {"args": {"threadIndex": lambda rng, o, f, w, c, p: w.append(("w", rng.randint(0, 3)))}},
}


def _matrix_text(rng, rows, cols):
    def row():
        return "( " + " ".join(rng.choice(["0", "1", "-2.5", "1e3", "abc", "3.25", "-0"]) for _ in range(cols)) + " )"

    if rows is None:
        return row()
    return "( " + " ".join(row() for _ in range(rows)) + " )"


def _matrix_1d(rng, objs, fixes, words, ctx, prev):
    x = rng.randint(0, 6)
    ctx["mx"] = x
    text = _matrix_text(rng, None, x if rng.random() < 0.9 else rng.randint(0, 6)).encode()
    objs.append((len(text) + 1, text))
    objs.append((4, b""))
    fixes.append((len(objs) - 1, 0, len(objs) - 2, 0))
    words += [("o", len(objs) - 1), ("w", x)]


def _matrix_2d(rng, objs, fixes, words, ctx, prev):
    y, x = rng.randint(0, 4), rng.randint(0, 4)
    text = _matrix_text(rng, y, x).encode()
    objs.append((len(text) + 1, text))
    objs.append((4, b""))
    fixes.append((len(objs) - 1, 0, len(objs) - 2, 0))
    words += [("o", len(objs) - 1), ("w", y), ("w", x)]


def _skip(*_):
    """Generator that emits nothing (the parameter was produced by an earlier generator)."""


# ComParseState lives at 0x0817e420 (ParseSession_Get); sessionIndex at +0x45c0.
_SESSION_INDEX = 0x0817E420 + 0x45C0

SPECS.update({
    0x080B6004: {"mem": lambda rng: [(_SESSION_INDEX, rng.randint(1, 15).to_bytes(4, "little"))]},
    0x080B7166: {"args": {"data_p": _matrix_1d, "x": _skip}},
    0x080B71EC: {"args": {"data_p": _matrix_2d, "y": _skip, "x": _skip}},
})


def _info_set(cap):
    """s, key, value for Info_SetValueForKey(_Big): often sized so that the result lands
    within +-3 bytes of the original's length limits (cap and 1024-byte value/newi limits)."""

    def gen(rng, objs, fixes, words, ctx, prev):
        from difftest import gen_string

        key = gen_string(rng, "key", "Info_", [])
        value = gen_string(rng, "value", "Info_", [])
        s = gen_string(rng, "info", "Info_", [key])
        if rng.random() < 0.5:
            # pad s with a filler pair so len(s) + len("\\key\\value") is near cap
            need = cap - (len(key) + len(value) + 2) + rng.randint(-3, 3) - len(s) - 2
            if need > 0:
                s += b"\\f\\" + b"x" * max(need - 1, 0)
        s = s[: cap + 8]
        objs.append((max(cap + 0x100, len(s) + 1), s))
        objs.append((len(key) + 1, key))
        objs.append((len(value) + 1, value))
        words += [("o", len(objs) - 3), ("o", len(objs) - 2), ("o", len(objs) - 1)]

    return gen


# va() keeps a 0/1 rotation index at block+0x800 of the main-thread block 0x085b76a0.
_VA_INDEX = 0x085B76A0 + 0x800

SPECS.update({
    0x080B85CE: {"args": {"s": _info_set(0x400), "key": _skip, "value": _skip}},
    0x080B8802: {"args": {"s": _info_set(0x2000), "key": _skip, "value": _skip}},
    0x080B7FA6: {"mem": lambda rng: [(_VA_INDEX, rng.randint(0, 1).to_bytes(4, "little"))]},
})


def _huft_build_args(rng, objs, fixes, words, ctx, prev):
    """zlib huft_build(b, n, s, d, e, t, m, hp, hn, v): b holds n code lengths in 0..15 (real
    callers guarantee this; random words index c[] far outside the frame and make both sides
    corrupt their own stack differently), d/e are 288-entry base/extra tables, hp has 1440 entries."""
    import struct

    n = rng.choice([1, 2, 7, 19, 30, 32, 100, 257, 288, rng.randint(1, 288)])
    mode = rng.random()
    if mode < 0.4:  # plausible: lengths mostly small, often a (near-)complete code
        lens = [rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 9]) for _ in range(n)]
    elif mode < 0.7:
        lens = [rng.randint(0, 15) for _ in range(n)]
    else:
        lens = [rng.choice([0, 0, 5, 5, 5, 5]) for _ in range(n)]
    s = rng.choice([0, 19, 257, rng.randint(0, 288)])

    def u32(vs):
        return b"".join(struct.pack("<I", x & 0xFFFFFFFF) for x in vs)

    objs.append((4 * n, u32(lens)))
    b = len(objs) - 1
    objs.append((4 * 288, u32([rng.randint(0, 0x8000) for _ in range(288)])))
    d = len(objs) - 1
    objs.append((4 * 288, u32([rng.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 99, 112, 192]) for _ in range(288)])))
    e = len(objs) - 1
    objs.append((4, b""))
    t = len(objs) - 1
    objs.append((4, u32([rng.randint(0, 15)])))
    m = len(objs) - 1
    objs.append((1440 * 8, b""))
    hp = len(objs) - 1
    objs.append((4, u32([rng.choice([0, 0, 0, 1000, 1430, 1439])])))
    hn = len(objs) - 1
    objs.append((4 * max(n, 1), b""))
    v = len(objs) - 1
    words += [("o", b), ("w", n), ("w", s), ("o", d), ("o", e), ("o", t), ("o", m), ("o", hp), ("o", hn), ("o", v)]


SPECS[0x080CDD38] = {"args": {"b": _huft_build_args, **{k: _skip for k in ("n", "s", "d", "e", "t", "m", "hp", "hn", "v")}}}


def _ps_from(rng, objs, fixes, words, ctx, prev):
    """MSG_ReadDeltaPlayerState `from`: NULL or a full playerState_t (0x26a8 bytes). Random
    tiny objects make the 0x26a8-byte copies overlap neighbouring objects (unspecified for memcpy)."""
    if rng.random() < 0.3:
        words.append(("w", 0))
        return
    objs.append((0x26A8, bytes(rng.getrandbits(8) for _ in range(0x26A8)) if rng.random() < 0.5 else b""))
    words.append(("o", len(objs) - 1))


def _ps_to(rng, objs, fixes, words, ctx, prev):
    objs.append((0x26A8, b""))
    words.append(("o", len(objs) - 1))


SPECS[0x0806AACA] = {"args": {"from": _ps_from, "to": _ps_to}}


SPECS[0x0811ACB2] = {
    "args": {
        "entNum": lambda rng, o, f, w, c, p: w.append(("w", rng.choice([0, 1, 2, 5, 63, 100, 1022]))),
        "fieldIndex": lambda rng, o, f, w, c, p: w.append(
            ("w", rng.choice([rng.randint(0, 9), rng.randint(0, 9), 0xC000 | rng.randint(0, 9), 0x4000 | rng.randint(0, 9)]))
        ),
    },
}


# ScrVar ids are 16-bit table indices; a random 32-bit parent addresses g_scrVar far outside the
# table (ASLR-dependent shared-library memory), which both sides then corrupt differently.
SPECS[0x0807C6B0] = {"args": {"parent": lambda rng, o, f, w, c, p: w.append(("w", rng.randint(0, 0xFFFD)))}}


def _pb_obj(rng, objs, fixes, words, ctx, prev):
    """punkbuster_s (0x170 bytes) whose pb_dir (+0x34, 256 bytes) is a NUL-terminated path; an
    unterminated random pb_dir makes strcpy run past the 512-byte stack buffer on both sides."""
    from difftest import gen_string

    d = gen_string(rng, "pb_dir", "PB_", [])[: rng.choice([0, 3, 20, 100, 250])]
    data = bytearray(0x170)
    data[0x34:0x34 + len(d)] = d
    objs.append((0x170, bytes(data)))
    words.append(("o", len(objs) - 1))


SPECS[0x0813CF38] = {"args": {"pb": _pb_obj}}


def _pm_sort_args(rng, objs, fixes, words, ctx, prev):
    """PM_SortPlanesByDot(velocity, numPlanes, planes, order): numPlanes 1..11 (the callee's dots[11]
    holds at most that many; numPlanes 0 returns the uninitialised dots[order[0]])."""
    import struct

    n = rng.randint(1, 11)
    fl = lambda k: b"".join(struct.pack("<f", rng.choice([0.0, 1.0, -1.0, 0.5, -0.5, rng.uniform(-1, 1), rng.uniform(-300, 300)])) for _ in range(k))
    objs.append((12, fl(3)))
    v = len(objs) - 1
    objs.append((12 * n, fl(3 * n)))
    pl = len(objs) - 1
    objs.append((4 * 11, b""))
    od = len(objs) - 1
    words += [("o", v), ("w", n), ("o", pl), ("o", od)]


SPECS[0x080E9B7C] = {"args": {"velocity": _pm_sort_args, "numPlanes": _skip, "planes": _skip, "order": _skip}}


def _fx_name(rng, objs, fixes, words, ctx, prev):
    """FX_GetEffectByName(name): a name without '%' (the 'must start with fx/' message goes through
    FxPrintf, which hands the formatted text to Com_Printf as a format string again)."""
    from difftest import gen_string

    s = gen_string(rng, "name", "FX_", []).replace(b"%", b"_")
    if rng.random() < 0.5:
        s = rng.choice([b"fx/", b"FX/", b"/fx/", b"\\Fx/"]) + s[:60]
    objs.append((len(s) + 1, s))
    words.append(("o", len(objs) - 1))


SPECS[0x0812C38A] = {"args": {"name": _fx_name}}


def _rc_client(rng, objs, fixes, words, ctx, prev):
    """SV_FindReliableCommand(cl, text): client_s with reliableSequence (+0x2080c) within 0..140 of
    reliableSent (+0x20814); random 32-bit values make the ring walk run for billions of iterations."""
    import struct

    size = 0x20900
    data = bytearray(rng.getrandbits(8) if rng.random() < 0.3 else 0 for _ in range(size))
    for slot in range(128):  # plausible entries: non-zero type and a short command string
        base = slot * 0x408
        data[base + 0x810:base + 0x814] = struct.pack("<I", rng.choice([0, 1, 2]))
        cmd = bytes(rng.choice(b"abcCDobAZ0") for _ in range(rng.randint(1, 6)))
        data[base + 0x400:base + 0x400 + len(cmd) + 1] = cmd + b"\0"
    sent = rng.randint(0, 0x7FFFFF00)
    data[0x20814:0x20818] = struct.pack("<i", sent)
    data[0x2080C:0x20810] = struct.pack("<i", sent + rng.choice([0, 1, 2, 10, 127, 128, 140]))
    objs.append((size, bytes(data)))
    words.append(("o", len(objs) - 1))


SPECS[0x08094530] = {"args": {"cl": _rc_client}}


def _fx_vec2_text(rng, objs, fixes, words, ctx, prev):
    """FX_PrimParse* text: always contains at least one number. For empty/whitespace-only text
    sscanf returns EOF (-1), which FX_ParseVec2 reports as success while leaving both outputs
    untouched, so the callers store uninitialised stack floats (compiler-frame dependent)."""
    a = rng.choice(["0", "1", "-2.5", "3.25", "100", "1e3", "0.5", "-0"])
    b = rng.choice(["", " 2", " -1.5", " 7", " 0", " 1e-3", " x"])
    s = (a + b).encode()
    objs.append((len(s) + 1, s))
    words.append(("o", len(objs) - 1))


for _a in (0x0812DF42, 0x0812DF9E, 0x0812E014, 0x0812E070, 0x0812E0CC, 0x0812E2FE, 0x0812E35C, 0x0812E3BA, 0x0812E418):
    SPECS[_a] = {"args": {"text": _fx_vec2_text}}
