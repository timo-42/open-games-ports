#!/usr/bin/env python3
"""CoD2 1.3 (protocol 118) wire-protocol helpers for the fake-client oracle (python3 stdlib only).

Every algorithm here mirrors the ORIGINAL cod2_lnxded 1.3 (SHA-256 05e774a3...a7) and was checked against its
disassembly (see analysis/oracle-client.md for the address list):
  * static Huffman (tree built by Q3-style adaptive addRef over the fixed 256-entry table at 0x08142d00;
    Huff_CompressBytes 0x08067f4e / Huff_DecompressBytes 0x08067fa2),
  * hybrid byte/bit msg_t cursor (MSG_WriteBits 0x08067c62, MSG_ReadBits 0x08067e20, ...),
  * netchan framing/fragments (Netchan_Transmit 0x0806bc6c, Netchan_Process 0x0806be8a),
  * XOR keystreams (SV_Netchan_Encode 0x080974d8, SV_Netchan_Decode 0x08097562), Com_HashKey 0x080acaa4,
  * delta readers (MSG_ReadDeltaField 0x08069960, MSG_ReadDeltaStruct 0x08069d4a,
    MSG_ReadDeltaPlayerState 0x0806aaca, MSG_ReadDeltaRecordArray 0x0806a0d0),
  * usercmd keyed coding (reader MSG_ReadDeltaUsercmdKey 0x08068f9e).
Field tables below are copied verbatim from the binary's .rodata (names are data, not code).
"""
import hashlib
import os
import struct

# ----------------------------------------------------------------------------------------------
# Tables (generated from the original binary; see header comments)
# ----------------------------------------------------------------------------------------------
# ENTITY_FIELDS: 59 entries read from .rodata 0x081417a0 of cod2_lnxded 1.3 ({name, offset, bits})
ENTITY_FIELDS = [
    ('pos.trTime', 0x10, 32), ('pos.trBase[1]', 0x1c, 0), ('pos.trBase[0]', 0x18, 0),
    ('pos.trDelta[0]', 0x24, 0), ('pos.trDelta[1]', 0x28, 0), ('angles2[1]', 0x6c, 0),
    ('apos.trBase[1]', 0x40, -100), ('pos.trDelta[2]', 0x2c, 0), ('pos.trBase[2]', 0x20, 0),
    ('apos.trBase[0]', 0x3c, -100), ('eventSequence', 0xa4, 8), ('legsAnim', 0xcc, 10), ('eType', 0x4, 8),
    ('eFlags', 0x8, 24), ('otherEntityNum', 0x74, 10), ('surfType', 0x88, 8), ('eventParm', 0xa0, 8),
    ('scale', 0xd8, 8), ('clientNum', 0x90, 8), ('torsoAnim', 0xd0, 10), ('groundEntityNum', 0x7c, 10),
    ('events[0]', 0xa8, 8), ('events[1]', 0xac, 8), ('events[2]', 0xb0, 8), ('angles2[0]', 0x68, 0),
    ('events[3]', 0xb4, 8), ('apos.trBase[2]', 0x44, -100), ('pos.trType', 0xc, 8), ('fWaistPitch', 0xec, 0),
    ('fTorsoPitch', 0xe8, 0), ('apos.trTime', 0x34, 32), ('solid', 0x9c, 24), ('apos.trDelta[0]', 0x48, 0),
    ('apos.trType', 0x30, 8), ('animMovetype', 0xe0, 4), ('fTorsoHeight', 0xe4, 0),
    ('apos.trDelta[2]', 0x50, 0), ('weapon', 0xc8, 7), ('index', 0x8c, 10), ('apos.trDelta[1]', 0x4c, 0),
    ('eventParms[0]', 0xb8, 8), ('eventParms[1]', 0xbc, 8), ('eventParms[2]', 0xc0, 8),
    ('eventParms[3]', 0xc4, 8), ('iHeadIcon', 0x94, 4), ('pos.trDuration', 0x14, 32),
    ('iHeadIconTeam', 0x98, 2), ('time', 0x54, 32), ('leanf', 0xd4, 0), ('attackerEntityNum', 0x78, 10),
    ('time2', 0x58, 32), ('loopSound', 0x84, 8), ('origin2[2]', 0x64, 0), ('origin2[0]', 0x5c, 0),
    ('origin2[1]', 0x60, 0), ('angles2[2]', 0x70, 0), ('constantLight', 0x80, 32),
    ('apos.trDuration', 0x38, 32), ('dmgFlags', 0xdc, 32),
]
# CLIENT_FIELDS: 22 entries read from .rodata 0x08141f60 of cod2_lnxded 1.3 ({name, offset, bits})
CLIENT_FIELDS = [
    ('team', 0x4, 2), ('name[0]', 0x3c, 32), ('name[4]', 0x40, 32), ('modelindex', 0x8, 8),
    ('attachModelIndex[1]', 0x10, 8), ('attachModelIndex[0]', 0xc, 8), ('name[8]', 0x44, 32),
    ('name[12]', 0x48, 32), ('name[16]', 0x4c, 32), ('name[20]', 0x50, 32), ('name[24]', 0x54, 32),
    ('name[28]', 0x58, 32), ('attachTagIndex[5]', 0x38, 5), ('attachTagIndex[0]', 0x24, 5),
    ('attachTagIndex[1]', 0x28, 5), ('attachTagIndex[2]', 0x2c, 5), ('attachTagIndex[3]', 0x30, 5),
    ('attachTagIndex[4]', 0x34, 5), ('attachModelIndex[2]', 0x14, 8), ('attachModelIndex[3]', 0x18, 8),
    ('attachModelIndex[4]', 0x1c, 8), ('attachModelIndex[5]', 0x20, 8),
]
# PLAYER_FIELDS: 105 entries read from .rodata 0x08142520 of cod2_lnxded 1.3 ({name, offset, bits})
PLAYER_FIELDS = [
    ('commandTime', 0x0, 32), ('origin[1]', 0x18, 0), ('origin[0]', 0x14, 0), ('bobCycle', 0x8, 8),
    ('viewangles[1]', 0xec, -100), ('origin[2]', 0x1c, 0), ('velocity[1]', 0x24, 0), ('velocity[0]', 0x20, 0),
    ('viewangles[0]', 0xe8, -100), ('movementDir', 0x9c, -8), ('velocity[2]', 0x28, 0),
    ('eventSequence', 0xa4, 8), ('legsAnim', 0x7c, 10), ('aimSpreadScale', 0x5d4, 0),
    ('weaponTime', 0x34, -16), ('pm_flags', 0xc, 27), ('events[0]', 0xa8, 8), ('events[1]', 0xac, 8),
    ('events[2]', 0xb0, 8), ('events[3]', 0xb4, 8), ('weapAnim', 0x5d0, 10), ('viewHeightCurrent', 0xf8, 0),
    ('torsoTimer', 0x80, 16), ('torsoAnim', 0x84, 10), ('eFlags', 0xa0, 24), ('fWeaponPosFrac', 0xdc, 0),
    ('holdBreathScale', 0x5b4, 0), ('weaponstate', 0xd8, 5), ('viewHeightTarget', 0xf4, -8),
    ('weaponDelay', 0x38, -16), ('legsTimer', 0x78, 16), ('viewHeightLerpTarget', 0x100, -8),
    ('groundEntityNum', 0x60, 10), ('pm_time', 0x10, -16), ('eventParms[3]', 0xc4, 8),
    ('eventParms[1]', 0xbc, 8), ('eventParms[0]', 0xb8, 8), ('eventParms[2]', 0xc0, 8), ('weapon', 0xd4, 7),
    ('weapons[0]', 0x544, 32), ('viewHeightLerpDown', 0x104, 1), ('weaponslots[0]', 0x554, 32),
    ('delta_angles[0]', 0x54, 16), ('delta_angles[1]', 0x58, 16), ('cursorHintString', 0x59c, -8),
    ('offHandIndex', 0xd0, 7), ('clientNum', 0xcc, 8), ('viewlocked_entNum', 0x594, 16),
    ('viewmodelIndex', 0xe4, 8), ('viewHeightLerpTime', 0xfc, 32), ('speed', 0x50, 16), ('mins[1]', 0x570, 0),
    ('mins[0]', 0x56c, 0), ('maxs[2]', 0x580, 0), ('maxs[1]', 0x57c, 0), ('maxs[0]', 0x578, 0),
    ('gravity', 0x48, 16), ('damageTimer', 0x90, 16), ('cursorHint', 0x598, 8),
    ('mantleState.flags', 0x5c8, 4), ('flinchYaw', 0x98, 16), ('fWaistPitch', 0x5b0, 0),
    ('mantleState.timer', 0x5c0, 32), ('fTorsoPitch', 0x5ac, 0), ('proneTorsoPitch', 0x58c, 0),
    ('holdBreathTimer', 0x5b8, 16), ('jumpTime', 0x70, 32), ('viewangles[2]', 0xf0, -100),
    ('foliageSoundTime', 0x44, 32), ('weapons[1]', 0x548, 32), ('damageEvent', 0x11c, 8),
    ('damageDuration', 0x94, 16), ('damageYaw', 0x120, 8), ('proneDirection', 0x584, 0),
    ('proneDirectionPitch', 0x588, 0), ('mantleState.yaw', 0x5bc, 0), ('mantleState.transIndex', 0x5c4, 4),
    ('fTorsoHeight', 0x5a8, 0), ('damagePitch', 0x124, 8), ('jumpOriginZ', 0x74, 0), ('pm_type', 0x4, 8),
    ('viewlocked', 0x590, 8), ('weaponrechamber[0]', 0x55c, 32), ('vLadderVec[0]', 0x64, 0),
    ('weaponslots[4]', 0x558, 32), ('weaponRestrictKickTime', 0x40, -16), ('vLadderVec[1]', 0x68, 0),
    ('viewAngleClampRange[1]', 0x118, 0), ('viewAngleClampRange[0]', 0x114, 0),
    ('viewAngleClampBase[1]', 0x110, 0), ('weaponrechamber[1]', 0x560, 32), ('leanf', 0x4c, 0),
    ('damageCount', 0x128, 7), ('grenadeTimeLeft', 0x3c, -16), ('deltaTime', 0x7a4, 32),
    ('shellshockTime', 0x5dc, 32), ('shellshockIndex', 0x5d8, 4), ('shellshockDuration', 0x5e0, 16),
    ('vLadderVec[2]', 0x6c, 0), ('delta_angles[2]', 0x5c, 16), ('viewHeightLerpPosAdj', 0x108, 0),
    ('mins[2]', 0x574, 0), ('viewAngleClampBase[0]', 0x10c, 0), ('adsDelayTime', 0xe0, 32),
    ('iCompassFriendInfo', 0x5a4, 32),
]
# OBJECTIVE_FIELDS: 6 entries read from .rodata 0x08142a20 of cod2_lnxded 1.3 ({name, offset, bits})
OBJECTIVE_FIELDS = [
    ('origin[0]', 0x4, 0), ('origin[1]', 0x8, 0), ('origin[2]', 0xc, 0), ('icon', 0x18, 12),
    ('entNum', 0x10, 10), ('teamNum', 0x14, 4),
]
# HUDELEM_FIELDS: 32 entries read from .rodata 0x08142b80 of cod2_lnxded 1.3 ({name, offset, bits})
HUDELEM_FIELDS = [
    ('y', 0x8, -99), ('type', 0x0, 4), ('color.rgba', 0x20, 32), ('x', 0x4, -99), ('alignScreen', 0x1c, 6),
    ('fontScale', 0x10, 0), ('materialIndex', 0x3c, 8), ('width', 0x34, 10), ('height', 0x38, 10),
    ('fadeStartTime', 0x28, 32), ('fromColor.rgba', 0x24, 32), ('fadeTime', 0x2c, 16), ('value', 0x70, 0),
    ('time', 0x68, 32), ('z', 0xc, -99), ('alignOrg', 0x18, 4), ('sort', 0x78, 0), ('text', 0x74, 8),
    ('font', 0x14, 4), ('scaleStartTime', 0x48, 32), ('scaleTime', 0x4c, 16), ('fromHeight', 0x44, 10),
    ('label', 0x30, 8), ('fromWidth', 0x40, 10), ('moveStartTime', 0x60, 32), ('moveTime', 0x64, 16),
    ('fromX', 0x50, -99), ('fromY', 0x54, -99), ('fromAlignScreen', 0x5c, 6), ('fromAlignOrg', 0x58, 4),
    ('duration', 0x6c, 32), ('foreground', 0x7c, 1),
]
# huff_symbol_counts[256] from .rodata 0x08142d00; sum=1053340
HUFF_COUNTS = (
    250315, 41193, 6292, 7106, 3730, 3750, 6110, 23283, 33317, 6950, 7838, 9714, 9257, 17259, 3949, 1778,
    8288, 1604, 1590, 1663, 1100, 1213, 1238, 1134, 1749, 1059, 1246, 1149, 1273, 4486, 2805, 3472,
    21819, 1159, 1670, 1066, 1043, 1012, 1053, 1070, 1726, 888, 1180, 850, 960, 780, 1752, 3296,
    10630, 4514, 5881, 2685, 4650, 3837, 2093, 1867, 2584, 1949, 1972, 940, 1134, 1788, 1670, 1206,
    5719, 6128, 7222, 6654, 3710, 3795, 1492, 1524, 2215, 1140, 1355, 971, 2180, 1248, 1328, 1195,
    1770, 1078, 1264, 1266, 1168, 965, 1155, 1186, 1347, 1228, 1529, 1600, 2617, 2048, 2546, 3275,
    2410, 3585, 2504, 2800, 2675, 6146, 3663, 2840, 14253, 3164, 2221, 1687, 3208, 2739, 3512, 4796,
    4091, 3515, 5288, 4016, 7937, 6031, 5360, 3924, 4892, 3743, 4566, 4807, 5852, 6400, 6225, 8291,
    23243, 7838, 7073, 8935, 5437, 4483, 3641, 5256, 5312, 5328, 5370, 3492, 2458, 1694, 1821, 2121,
    1916, 1149, 1516, 1367, 1236, 1029, 1258, 1104, 1245, 1006, 1149, 1025, 1241, 952, 1287, 997,
    1713, 1009, 1187, 879, 1099, 929, 1078, 951, 1656, 930, 1153, 1030, 1262, 1062, 1214, 1060,
    1621, 930, 1106, 912, 1034, 892, 1158, 990, 1175, 850, 1121, 903, 1087, 920, 1144, 1056,
    3462, 2240, 4397, 12136, 7758, 1345, 1307, 3278, 1950, 886, 1023, 1112, 1077, 1042, 1061, 1071,
    1484, 1001, 1096, 915, 1052, 995, 1070, 876, 1111, 851, 1059, 805, 1112, 923, 1103, 817,
    1899, 1872, 976, 841, 1127, 956, 1159, 950, 7791, 954, 1289, 933, 1127, 3207, 1020, 927,
    1355, 768, 1040, 745, 952, 805, 1073, 740, 1013, 805, 1008, 796, 996, 1057, 11457, 13504,
)

ENTITY_SIZE, CLIENT_SIZE, PLAYER_SIZE = 0xF0, 0x5C, 0x26A8
HUDELEM_SIZE, OBJECTIVE_SIZE = 0x80, 0x1C
MASK32 = 0xFFFFFFFF


def s32(v):
    v &= MASK32
    return v - 0x100000000 if v & 0x80000000 else v


def f32bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def bits2f(u):
    return struct.unpack("<f", struct.pack("<I", u & MASK32))[0]


# ----------------------------------------------------------------------------------------------
# Static Huffman: rebuild the binary's tree exactly (MSG_InitHuffmanTables 0x0806b12c feeds every
# symbol counts[sym] times into Huff_addRef; the result is a fixed prefix code).
# ----------------------------------------------------------------------------------------------
_INTERNAL, _NYT = 0x101, 0x100


class _Node:
    __slots__ = ("left", "right", "parent", "next", "prev", "head", "weight", "symbol")

    def __init__(self):
        self.left = self.right = self.parent = self.next = self.prev = None
        self.head = None
        self.weight = 0
        self.symbol = 0


class _AdaptiveHuff:
    """Literal port of Huff_addRef/Huff_increment/swap/swaplist (0x080665ce, 0x08066466, 0x0806631e, 0x080663a6),
    including the Q3 quirk `*tnode->head = tnode2` kept by the binary. Slots (node**) are list indices."""

    def __init__(self):
        self.ptrs, self.free = [], None
        self.loc = [None] * 257
        n = _Node()
        n.symbol = _NYT
        self.tree = self.lhead = n
        self.loc[_NYT] = n

    def _alloc(self):
        if self.free is None:
            self.ptrs.append(None)
            return len(self.ptrs) - 1
        s = self.free
        self.free = self.ptrs[s][1]
        return s

    def _freeptr(self, s):
        self.ptrs[s] = ("free", self.free)
        self.free = s

    def _swap(self, a, b):
        pa, pb = a.parent, b.parent
        if pa is None:
            self.tree = b
        elif pa.left is a:
            pa.left = b
        else:
            pa.right = b
        if pb is None:
            self.tree = a
        elif pb.left is b:
            pb.left = a
        else:
            pb.right = a
        a.parent, b.parent = pb, pa

    @staticmethod
    def _swaplist(a, b):
        a.next, b.next = b.next, a.next
        a.prev, b.prev = b.prev, a.prev
        if a.next is a:
            a.next = b
        if b.next is b:
            b.next = a
        if a.next:
            a.next.prev = a
        if b.next:
            b.next.prev = b
        if a.prev:
            a.prev.next = a
        if b.prev:
            b.prev.next = b

    def _increment(self, node):
        P = self.ptrs
        if node is None:
            return
        if node.next is not None and node.next.weight == node.weight:
            lnode = P[node.head]
            if lnode is not node.parent:
                self._swap(lnode, node)
            self._swaplist(lnode, node)
        if node.prev is not None and node.prev.weight == node.weight:
            P[node.head] = node.prev
        else:
            P[node.head] = None
            self._freeptr(node.head)
        node.weight += 1
        if node.next is not None and node.next.weight == node.weight:
            node.head = node.next.head
        else:
            node.head = self._alloc()
            P[node.head] = node
        if node.parent is not None:
            self._increment(node.parent)
            if node.prev is node.parent:
                self._swaplist(node, node.parent)
                if P[node.head] is node:
                    P[node.head] = node.parent

    def addref(self, ch):
        P = self.ptrs
        if self.loc[ch] is not None:
            self._increment(self.loc[ch])
            return
        tnode, tnode2 = _Node(), _Node()
        lh = self.lhead
        tnode2.symbol, tnode2.weight, tnode2.next = _INTERNAL, 1, lh.next
        if lh.next is not None:
            lh.next.prev = tnode2
            if lh.next.weight == 1:
                tnode2.head = lh.next.head
            else:
                tnode2.head = self._alloc()
                P[tnode2.head] = tnode2
        else:
            tnode2.head = self._alloc()
            P[tnode2.head] = tnode2
        lh.next = tnode2
        tnode2.prev = lh
        tnode.symbol, tnode.weight, tnode.next = ch, 1, lh.next
        if lh.next is not None:
            lh.next.prev = tnode
            if lh.next.weight == 1:
                tnode.head = lh.next.head
            else:
                tnode.head = self._alloc()
                P[tnode.head] = tnode2  # binary/Q3 quirk, kept on purpose
        else:
            tnode.head = self._alloc()
            P[tnode.head] = tnode
        lh.next = tnode
        tnode.prev = lh
        lp = lh.parent
        if lp is not None:
            if lp.left is lh:
                lp.left = tnode2
            else:
                lp.right = tnode2
        else:
            self.tree = tnode2
        tnode2.right, tnode2.left = tnode, lh
        tnode2.parent = lh.parent
        lh.parent = tnode.parent = tnode2
        self.loc[ch] = tnode
        self._increment(tnode2.parent)


class Huffman:
    """Static code derived once (about 2 s in CPython). codes[sym] = list of bits root->leaf."""

    def __init__(self, counts=HUFF_COUNTS):
        import sys
        old = sys.getrecursionlimit()
        sys.setrecursionlimit(max(old, 20000))
        h = _AdaptiveHuff()
        for sym in range(256):
            for _ in range(counts[sym]):
                h.addref(sym)
        sys.setrecursionlimit(old)
        self.codes = []
        for sym in range(256):
            n, bits = h.loc[sym], []
            while n.parent is not None:
                bits.append(1 if n.parent.right is n else 0)
                n = n.parent
            self.codes.append(bits[::-1])
        # decoding trie: dict prefix-tuple-free; use nested lists [left,right] / int leaf
        self.root = [None, None]
        for sym, code in enumerate(self.codes):
            node = self.root
            for b in code[:-1]:
                if node[b] is None:
                    node[b] = [None, None]
                node = node[b]
            node[code[-1]] = sym
        self.fingerprint = hashlib.sha256("".join("".join(map(str, c)) + "," for c in self.codes).encode()).hexdigest()

    def compress(self, data):
        """Huff_CompressBytes: LSB-first bit packing; returns (bitOffset+7)>>3 bytes."""
        out = bytearray()
        acc = nbits = 0
        for byte in data:
            for b in self.codes[byte]:
                acc |= b << nbits
                nbits += 1
                if nbits == 8:
                    out.append(acc)
                    acc = nbits = 0
        if nbits:
            out.append(acc)
        return bytes(out)

    def decompress(self, data):
        """Huff_DecompressBytes: decode while bitOffset < len*8. The binary may read past the input for the
        trailing pad bits (garbage after the last real symbol); bits beyond the input are taken as 0 here.
        Returns (bytes, overrun_bits)."""
        out = bytearray()
        limit = len(data) * 8
        off = 0
        overrun = 0
        root = self.root
        while off < limit:
            node = root
            while True:
                if off < limit:
                    b = (data[off >> 3] >> (off & 7)) & 1
                else:
                    b = 0
                    overrun += 1
                off += 1
                node = node[b]
                if not isinstance(node, list):
                    break
            out.append(node)
        return bytes(out), overrun


_HUFF = None


def huffman():
    global _HUFF
    if _HUFF is None:
        _HUFF = Huffman()
    return _HUFF


# ----------------------------------------------------------------------------------------------
# Hybrid msg_t (exact cursor semantics of chunk 008 / binary)
# ----------------------------------------------------------------------------------------------
class Msg:
    def __init__(self, data=None, maxsize=0x20000):
        if data is None:
            self.data = bytearray(maxsize)
            self.cursize = 0
        else:
            self.data = bytearray(data)
            self.cursize = len(data)
            maxsize = max(maxsize, len(data))
        self.maxsize = maxsize
        self.readcount = 0
        self.bit = 0
        self.overflowed = False

    def bytes(self):
        return bytes(self.data[: self.cursize])

    # --- writers ---
    def write_bits(self, value, bits):
        if self.maxsize - self.cursize < 4:
            self.overflowed = True
            return
        value &= MASK32
        while bits:
            bits -= 1
            old = self.bit
            if (old & 7) == 0:
                self.bit = self.cursize << 3
                self.data[self.cursize] = 0
                self.cursize += 1
            if value & 1:
                self.data[self.bit >> 3] |= 1 << (old & 7)
            self.bit += 1
            value >>= 1

    def write_bit(self, b):
        if self.cursize >= self.maxsize:
            self.overflowed = True
            return
        old = self.bit
        if (old & 7) == 0:
            self.bit = self.cursize << 3
            self.data[self.cursize] = 0
            self.cursize += 1
        if b:
            self.data[self.bit >> 3] |= 1 << (old & 7)
        self.bit += 1

    def write_data(self, b):
        if self.cursize + len(b) > self.maxsize:
            self.overflowed = True
            return
        self.data[self.cursize:self.cursize + len(b)] = b
        self.cursize += len(b)

    def write_byte(self, c):
        self.write_data(bytes([c & 0xFF]))

    def write_short(self, c):
        self.write_data(struct.pack("<H", c & 0xFFFF))

    def write_long(self, c):
        self.write_data(struct.pack("<I", c & MASK32))

    def write_string(self, s):
        if isinstance(s, str):
            s = s.encode("latin1")
        if len(s) >= 0x400:
            s = b""
        self.write_data(s + b"\0")

    # --- readers ---
    def read_bits(self, bits):
        result = 0
        for i in range(bits):
            old = self.bit
            if (old & 7) == 0:
                if self.cursize <= self.readcount:
                    self.overflowed = True
                    return -1
                self.bit = self.readcount << 3
                self.readcount += 1
            result |= ((self.data[self.bit >> 3] >> (old & 7)) & 1) << i
            self.bit += 1
        return result

    def read_bit(self):
        return self.read_bits(1)

    def read_byte(self):
        if self.readcount < self.cursize:
            v = self.data[self.readcount]
            self.readcount += 1
            return v
        self.overflowed = True
        return -1

    def read_short(self):
        if self.cursize < self.readcount + 2:
            self.overflowed = True
            return -1
        v = struct.unpack_from("<h", self.data, self.readcount)[0]
        self.readcount += 2
        return v

    def read_long(self):
        if self.cursize < self.readcount + 4:
            self.overflowed = True
            return -1
        v = struct.unpack_from("<i", self.data, self.readcount)[0]
        self.readcount += 4
        return v

    def read_string_raw(self, limit=0x2000):
        out = bytearray()
        while True:
            c = self.read_byte()
            if c == -1 or c == 0:
                break
            if len(out) < limit - 1:
                out.append(c)
        return bytes(out)


# ----------------------------------------------------------------------------------------------
# Keystreams and hashes
# ----------------------------------------------------------------------------------------------
def xor_stream(data, key, string):
    """SV_Netchan_Encode/Decode loop: key byte, per byte key ^= str[idx] << (i&1) (idx wraps at NUL)."""
    if isinstance(string, str):
        string = string.encode("latin1")
    # The binary indexes the 0x400-byte buffer: for an empty string it reads buf[1] (assumed 0 here: the
    # server's buffers are zeroed per client at connect; a stale tail after a shorter overwrite is not modelled).
    s = string.split(b"\0")[0] + b"\0\0"
    out = bytearray(data)
    key &= 0xFF
    idx = 0
    for i in range(len(out)):
        if s[idx] == 0:
            idx = 0
        key = (key ^ (s[idx] << (i & 1))) & 0xFF
        idx += 1
        out[i] ^= key
    return bytes(out)


def com_hashkey(s, maxlen=32):
    if isinstance(s, str):
        s = s.encode("latin1")
    h = 0
    for i, c in enumerate(s[:maxlen]):
        if c == 0:
            break
        sc = c - 256 if c >= 128 else c  # movsx
        h = (h + (i + 0x77) * sc) & MASK32
    hs = s32(h)
    return (hs ^ (hs >> 10) ^ (hs >> 20)) & MASK32


# ----------------------------------------------------------------------------------------------
# Delta decoding (client side mirror of the server's writers)
# ----------------------------------------------------------------------------------------------
def read_delta_field(msg, frm, to, field):
    """MSG_ReadDeltaField 0x08069960 (entities, clientStates, hudelems, objectives). States are lists of raw dwords."""
    name, off, bits = field
    k = off >> 2
    if msg.read_bit() == 0:
        to[k] = frm[k]
        return
    if bits == 0 or bits == -99:
        if msg.read_bit() == 0:
            to[k] = 0
        elif msg.read_bit() == 0:
            if bits == 0:
                v = msg.read_bits(5) + msg.read_byte() * 32 - 0x1000
            else:
                v = msg.read_bits(2) + msg.read_byte() * 4 - 0x200
            to[k] = f32bits(float(v))
        else:
            to[k] = msg.read_long() & MASK32
    elif bits == -100:
        if msg.read_bit() == 0:
            to[k] = 0
        else:
            to[k] = f32bits(msg.read_short() * 0.0054931640625)
    else:
        if msg.read_bit() == 0:
            to[k] = 0
        else:
            to[k] = _read_int_field(msg, bits)


def _read_int_field(msg, bits):
    neg = bits < 0
    width = -bits if neg else bits
    low = width & 7
    v = msg.read_bits(low) if low else 0
    sh = low
    while sh < width:
        v |= (msg.read_byte() << sh) & MASK32
        sh += 8
    v &= MASK32
    if neg and (s32(v) >> ((width - 1) & 31)) & 1:
        v |= ~((1 << (width & 31)) - 1) & MASK32
    return v & MASK32


def read_delta_struct(msg, frm, number, fields):
    """MSG_ReadDeltaStruct 0x08069d4a. Returns (removed, newstate, changedCount or None)."""
    if msg.read_bit() == 1:
        return True, None, None
    if msg.read_bit() == 0:
        return False, list(frm), 0
    count = msg.read_byte()
    if count > len(fields) or count < 0:
        msg.overflowed = True
        return False, list(frm), count
    to = list(frm)
    to[0] = number & MASK32
    for i in range(count):
        read_delta_field(msg, frm, to, fields[i])
    return False, to, count


def read_delta_playerstate(msg, frm):
    """MSG_ReadDeltaPlayerState 0x0806aaca (verified instruction by instruction)."""
    if frm is None:
        frm = [0] * (PLAYER_SIZE // 4)
    to = list(frm)
    count = msg.read_byte()
    if count < 0 or count > len(PLAYER_FIELDS):
        msg.overflowed = True
        return to, count
    for i in range(count):
        name, off, bits = PLAYER_FIELDS[i]
        k = off >> 2
        if msg.read_bit() == 0:
            continue
        if bits == 0:
            if msg.read_bit() == 0:
                v = msg.read_bits(5) + msg.read_byte() * 32 - 0x1000
                to[k] = f32bits(float(v))
            else:
                to[k] = msg.read_long() & MASK32
        elif bits == -100:
            if msg.read_bit() == 0:
                to[k] = 0
            else:
                to[k] = f32bits(msg.read_short() * 0.0054931640625)
        else:
            to[k] = _read_int_field(msg, bits)
    if msg.read_bit():
        mask = msg.read_bits(6)
        for bitn, off, kind in ((1, 0x12c, "s"), (2, 0x130, "s"), (4, 0x134, "s"), (8, 0x138, "b6"),
                                (16, 0x13c, "s"), (32, 0x140, "B")):
            if mask & bitn:
                v = msg.read_short() if kind == "s" else (msg.read_bits(6) if kind == "b6" else msg.read_byte())
                to[off >> 2] = v & MASK32
    if msg.read_bit():
        for i in range(4):
            if msg.read_bit():
                mask = msg.read_short()
                for j in range(16):
                    if (mask >> j) & 1:
                        to[(0x144 >> 2) + i * 16 + j] = msg.read_short() & MASK32
    for i in range(4):
        if msg.read_bit():
            mask = msg.read_short()
            for j in range(16):
                if (mask >> j) & 1:
                    to[(0x344 >> 2) + i * 16 + j] = msg.read_short() & MASK32
    if msg.read_bit():
        for i in range(16):
            base = (0x5e4 + i * OBJECTIVE_SIZE) >> 2
            to[base] = msg.read_bits(3) & MASK32
            fr = frm[base:base + OBJECTIVE_SIZE // 4]
            tt = to[base:base + OBJECTIVE_SIZE // 4]
            if msg.read_bit():
                for f in OBJECTIVE_FIELDS:
                    read_delta_field(msg, fr, tt, f)
            else:
                for f in OBJECTIVE_FIELDS:
                    tt[f[1] >> 2] = fr[f[1] >> 2]
            to[base + 1:base + OBJECTIVE_SIZE // 4] = tt[1:]
    if msg.read_bit():
        for base_off in (0x1728, 0x7a8):
            _read_hudelems(msg, frm, to, base_off >> 2, 31)
    return to, count


def _read_hudelems(msg, frm, to, base, maxcount):
    """MSG_ReadDeltaRecordArray 0x0806a0d0."""
    n = HUDELEM_SIZE // 4
    count = msg.read_bits(5)
    if count < 0:
        return
    for i in range(count):
        last = msg.read_bits(5)
        fr = frm[base + i * n: base + (i + 1) * n]
        tt = list(to[base + i * n: base + (i + 1) * n])
        j = 0
        while j <= last and j < len(HUDELEM_FIELDS):
            read_delta_field(msg, fr, tt, HUDELEM_FIELDS[j])
            j += 1
        while j < 32:
            k = HUDELEM_FIELDS[j][1] >> 2
            tt[k] = fr[k]
            j += 1
        to[base + i * n: base + (i + 1) * n] = tt
    for i in range(count, maxcount):
        to[base + i * n: base + (i + 1) * n] = [0] * n


def state_hash(words):
    return hashlib.sha256(struct.pack("<%dI" % len(words), *[w & MASK32 for w in words])).hexdigest()[:16]


def named_fields(words, fields, nonzero_only=True):
    out = {}
    for name, off, bits in fields:
        v = words[off >> 2]
        if nonzero_only and v == 0:
            continue
        if bits in (0, -99, -100):
            out[name] = round(bits2f(v), 3)
        else:
            out[name] = s32(v) if bits < 0 else v
    return out


# ----------------------------------------------------------------------------------------------
# usercmd writer (mirror of MSG_ReadDeltaUsercmdKey 0x08068f9e; the binary's writer has no callers)
# ----------------------------------------------------------------------------------------------
def write_usercmd_nochange(msg, key, server_time, prev_time):
    """Encode a usercmd identical to its predecessor except serverTime (keyed 'changed' bit = 0)."""
    delta = (server_time - prev_time) if prev_time is not None else None
    if delta is not None and 0 <= delta < 0x100:
        msg.write_bit(1)
        msg.write_byte(delta)
    else:
        msg.write_bit(0)
        msg.write_long(server_time)
    msg.write_bits(0 ^ (key & 1), 1)


def move_class(forward, right):
    """UserCmd_MoveClass 0x08068b20: forward >= 11 -> 1, <= -11 -> 2; right >= 11 -> 4, <= -11 -> 8."""
    c = 1 if forward >= 11 else (2 if forward < -10 else 0)
    return c | (4 if right >= 11 else (8 if right < -10 else 0))


def write_usercmd_full(msg, key, cmd):
    """Encode `cmd` so that MSG_ReadDeltaUsercmdKey 0x08068f9e reproduces every field regardless of the delta base.

    The server's first delta base is CL_CreateCmd(ps) (0x080689a6), which the client cannot know exactly, so every
    field is sent in the reader's long form with its 'changed' bit forced to 1 (the reader never compares against the
    base, it only reads the bit): serverTime as a long, keyed bits changed=1 / long form=1, buttons bit 0, angles[0..1]
    (keyed shorts), move class (4 keyed bits), then key ^= serverTime, angles[2], buttons >> 1 (18 bits), weapon (7)
    and offHandIndex (7). cmd: dict serverTime, buttons, angles (3 shorts), forwardmove, rightmove, weapon, offhand.
    weapon/offhand 0 make SV_UserMove substitute ps.weapon / ps.offHandIndex (BG_PlayerHasWeapon(ps, 0) fails)."""
    t = cmd["serverTime"] & MASK32
    msg.write_bit(0)
    msg.write_long(t)
    msg.write_bits(1 ^ (key & 1), 1)  # changed
    msg.write_bits(1 ^ (key & 1), 1)  # long form
    b = cmd.get("buttons", 0) & MASK32
    msg.write_bits((b & 1) ^ (key & 1), 1)
    ang = list(cmd.get("angles", (0, 0, 0)))
    for i in (0, 1):
        msg.write_bit(1)
        msg.write_short((ang[i] ^ key) & 0xFFFF)
    msg.write_bit(1)
    msg.write_bits((move_class(cmd.get("forwardmove", 0), cmd.get("rightmove", 0)) ^ key) & 0xF, 4)
    key = (key ^ t) & MASK32
    msg.write_bit(1)
    msg.write_short((ang[2] ^ key) & 0xFFFF)
    msg.write_bit(1)
    msg.write_bits(((b >> 1) ^ key) & 0x3FFFF, 18)
    msg.write_bit(1)
    msg.write_bits((cmd.get("weapon", 0) ^ key) & 0x7F, 7)
    msg.write_bit(1)
    msg.write_bits((cmd.get("offhand", 0) ^ key) & 0x7F, 7)


def read_usercmd(msg, key, frm):
    """Python mirror of MSG_ReadDeltaUsercmdKey 0x08068f9e (verified against the disassembly), for self-tests."""
    to = dict(frm)
    if msg.read_bit() == 0:
        to["serverTime"] = msg.read_long() & MASK32
    else:
        to["serverTime"] = (msg.read_byte() + frm["serverTime"]) & MASK32
    kb = lambda k, n: msg.read_bits(n) ^ (((1 << n) - 1) & k)  # noqa: E731
    def dk(k, old, n):
        return old if msg.read_bit() == 0 else kb(k, n)
    def dks(k, old):
        return old if msg.read_bit() == 0 else ((msg.read_short() ^ k) & 0xFFFF)
    if kb(key, 1) == 0:
        return to
    to["buttons"] = frm["buttons"] & ~1
    ang = list(frm["angles"])
    if kb(key, 1) == 0:
        key ^= to["serverTime"]
        to["buttons"] |= kb(key, 1)
        ang[0] = dks(key, frm["angles"][0])
        ang[1] = dks(key, frm["angles"][1])
        to["move_class"] = dk(key, move_class(frm["forwardmove"], frm["rightmove"]), 4)
    else:
        to["buttons"] |= kb(key, 1)
        ang[0] = dks(key, frm["angles"][0])
        ang[1] = dks(key, frm["angles"][1])
        to["move_class"] = dk(key, move_class(frm["forwardmove"], frm["rightmove"]), 4)
        key ^= to["serverTime"]
        ang[2] = dks(key, frm["angles"][2])
        to["buttons"] = (to["buttons"] & 1) | (dk(key, frm["buttons"] >> 1, 18) << 1)
        to["weapon"] = dk(key, frm["weapon"], 7)
        to["offhand"] = dk(key, frm["offhand"], 7)
    to["angles"] = ang
    return to


# ----------------------------------------------------------------------------------------------
# MD4 / iwd checksums (Com_BlockChecksum 0x08067b02, Com_BlockChecksumKey 0x08067b5e; iwd loader in
# chunk 024: CRC32 of every zip entry with uncompressedSize != 0, central-directory order)
# ----------------------------------------------------------------------------------------------
def _md4(data):
    def F(x, y, z): return (x & y) | (~x & z)
    def G(x, y, z): return (x & y) | (x & z) | (y & z)
    def H(x, y, z): return x ^ y ^ z
    def rot(v, s): v &= MASK32; return ((v << s) | (v >> (32 - s))) & MASK32
    msg = bytearray(data)
    ml = (len(data) * 8) & 0xFFFFFFFFFFFFFFFF
    msg.append(0x80)
    while len(msg) % 64 != 56:
        msg.append(0)
    msg += struct.pack("<Q", ml)
    a, b, c, d = 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476
    for off in range(0, len(msg), 64):
        X = struct.unpack_from("<16I", msg, off)
        aa, bb, cc, dd = a, b, c, d
        r1 = [3, 7, 11, 19]
        for i in range(16):
            k = i
            if i % 4 == 0: a = rot(a + F(b, c, d) + X[k], r1[0])
            elif i % 4 == 1: d = rot(d + F(a, b, c) + X[k], r1[1])
            elif i % 4 == 2: c = rot(c + F(d, a, b) + X[k], r1[2])
            else: b = rot(b + F(c, d, a) + X[k], r1[3])
        r2 = [3, 5, 9, 13]
        for i in range(16):
            k = (i % 4) * 4 + i // 4
            if i % 4 == 0: a = rot(a + G(b, c, d) + X[k] + 0x5a827999, r2[0])
            elif i % 4 == 1: d = rot(d + G(a, b, c) + X[k] + 0x5a827999, r2[1])
            elif i % 4 == 2: c = rot(c + G(d, a, b) + X[k] + 0x5a827999, r2[2])
            else: b = rot(b + G(c, d, a) + X[k] + 0x5a827999, r2[3])
        r3 = [3, 9, 11, 15]
        order = [0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15]
        for i in range(16):
            k = order[i]
            if i % 4 == 0: a = rot(a + H(b, c, d) + X[k] + 0x6ed9eba1, r3[0])
            elif i % 4 == 1: d = rot(d + H(a, b, c) + X[k] + 0x6ed9eba1, r3[1])
            elif i % 4 == 2: c = rot(c + H(d, a, b) + X[k] + 0x6ed9eba1, r3[2])
            else: b = rot(b + H(c, d, a) + X[k] + 0x6ed9eba1, r3[3])
        a, b, c, d = (a + aa) & MASK32, (b + bb) & MASK32, (c + cc) & MASK32, (d + dd) & MASK32
    return a, b, c, d


def block_checksum(data, key=None):
    if key is not None:
        data = struct.pack("<i", s32(key)) + data
    a, b, c, d = _md4(data)
    return s32(a ^ b ^ c ^ d)


_IWD_CRC_BLOBS = {}


def iwd_crc_blob(path):
    """Packed CRC32s of the non-empty members of an iwd, cached per (path, size, mtime): parsing a 20k-entry central
    directory takes 0.1-0.3 s, which would stall every scripted client at each map change."""
    import zipfile
    st = os.stat(path)
    key = (os.path.realpath(path), st.st_size, st.st_mtime_ns)
    if key not in _IWD_CRC_BLOBS:
        with zipfile.ZipFile(path) as z:
            crcs = [i.CRC for i in z.infolist() if i.file_size != 0]
        _IWD_CRC_BLOBS[key] = struct.pack("<%dI" % len(crcs), *crcs)
    return _IWD_CRC_BLOBS[key]


def iwd_checksums(path, feed):
    """(checksum, pure_checksum) of an iwd as the server computes them (signed ints as printed with %i)."""
    blob = iwd_crc_blob(path)
    return block_checksum(blob), block_checksum(blob, feed)
