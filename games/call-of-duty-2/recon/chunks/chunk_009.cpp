/*
 * chunk_009: network message delta coding, netchan transport, loopback and
 * OOB packets, net profiling, Huffman table setup, and anim-tree script
 * parsing for cod2_lnxded 1.3 (0x08069c98 .. 0x0806d9fc).
 *
 * Conventions:
 *  - Record fields are read and written at recovered byte offsets through the
 *    LoadI32/StoreI32/LoadF32/StoreF32 helpers, which preserve raw 32-bit
 *    representation. Float fields in the delta tables are compared as floats
 *    and sent as raw bits, matching the disassembly.
 *  - Ghidra's float typing of integer paths is replaced by integer arithmetic
 *    where the disassembly shows integer loads and shifts.
 *  - Placeholder callees keep their fn_08xxxxxx names; see chunk_009_types.h.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_009_types.h"

enum {
    kPlayerStateBytes = 0x26a8,
    kPlayerStateFieldCount = 0x69,
    kNetMaxPacket = 0x514,
    kNetMaxMessage = 0x578,
    kNetFragmentLimit = 0x20000,
    kNetProfileSamples = 0x3c
};

/* ------------------------------------------------------------------------ */
/* Raw record accessors. Offsets are byte offsets inside the record.          */

static inline int LoadI32(const void *base, int off)
{
    int v;
    memcpy(&v, (const char *)base + off, sizeof v);
    return v;
}

static inline void StoreI32(void *base, int off, int v)
{
    memcpy((char *)base + off, &v, sizeof v);
}

static inline float LoadF32(const void *base, int off)
{
    float v;
    memcpy(&v, (const char *)base + off, sizeof v);
    return v;
}

static inline void StoreF32(void *base, int off, float v)
{
    memcpy((char *)base + off, &v, sizeof v);
}

static inline int FloatBits(float v)
{
    int b;
    memcpy(&b, &v, sizeof b);
    return b;
}

static inline int PtrToInt(const void *p)
{
    return (int)(long)p;
}

static inline void *IntToPtr(int v)
{
    return (void *)(long)v;
}

static inline int CvarLowByteSet(const dvar_s *cv)
{
    return (char)cv->current.integer != 0;
}

/* Returns the 16-bit field difference mask over 16 consecutive dwords. */
static int DiffMask16(const void *from, const void *to, int baseOff)
{
    int mask = 0;
    int j;
    for (j = 0; j < 16; j++) {
        if (LoadI32(from, baseOff + j * 4) != LoadI32(to, baseOff + j * 4)) {
            mask |= 1 << (j & 0x1f);
        }
    }
    return mask;
}

/* Forward declarations for functions defined later in this chunk. */
byte NET_SendPacket(int sock, int length, const void *data, netadr_s to);
int LittleLong_0806cf00(int value);
int Net_CurrentClientNum(void);

/* ------------------------------------------------------------------------ */
/* Global definitions owned by this chunk. The loopback queue, cvars and       */
/* profile pointers are declared in the types header.                          */

char net_adrToStringBuf[0x40];

/* ======================================================================== */
// 0x08069c98  size=178  FUN_08069c98  status=translated  naming=inferred
// naming: Q3 MSG_ReadDelta* family; reads or copies every field of a netField table.
// The caller passes a netField table whose entries carry byte offsets (evidence: recordFields32 field layout).
void MSG_ReadDeltaFieldsAll(msg_s *msg, const void *from, void *to, int count, const netField_s *fields)
{
    int i;

    if (MSG_ReadBit(msg) == 0) {
        for (i = 0; i < count; i++) {
            StoreI32(to, fields[i].offset, LoadI32(from, fields[i].offset));
        }
        return;
    }
    for (i = 0; i < count; i++) {
        MSG_ReadDeltaField(msg, from, to, &fields[i], 0);
    }
}

/* ======================================================================== */
// 0x08069d4a  size=318  FUN_08069d4a  status=translated  naming=inferred
// naming: Q3 MSG_ReadDeltaEntity/Struct. Bit 1 = removed, bit 0 = unchanged; then a byte count
// of changed fields, each read through a netField entry; the rest is copied from the baseline.
// param_6 is passed by every caller but never read by this routine.
int MSG_ReadDeltaStruct(msg_s *msg, const void *from, void *to, int number, int numFields,
                        int reserved6, const netField_s *fields)
{
    int changedCount;
    int i;

    (void)reserved6;
    if (MSG_ReadBit(msg) == 1) {
        return 1;
    }
    if (MSG_ReadBit(msg) == 0) {
        memcpy(to, from, numFields * 4 + 4);
        return 0;
    }
    changedCount = MSG_ReadByte(msg);
    if (numFields < changedCount) {
        msg->overflowed = 1;
        return 0;
    }
    StoreI32(to, 0, number);
    for (i = 0; i < changedCount; i++) {
        MSG_ReadDeltaField(msg, from, to, &fields[i], 0);
    }
    for (i = changedCount; i < numFields; i++) {
        StoreI32(to, fields[i].offset, LoadI32(from, fields[i].offset));
    }
    return 0;
}

/* ======================================================================== */
// 0x08069e88  size=70  FUN_08069e88  status=translated  naming=inferred
// naming: entityState delta reader. Table entityStateFields has first field "pos.trTime" (evidence: rodata).
void MSG_ReadDeltaEntity(msg_s *msg, const void *from, void *to, int number)
{
    MSG_ReadDeltaStruct(msg, from, to, number, 0x3b, 10, entityStateFields);
}

/* ======================================================================== */
// 0x08069ece  size=70  FUN_08069ece  status=translated  naming=inferred
// naming: shared-entity delta reader. Table sharedEntityFields has fields "absmin[1]", "absmax[1]" (evidence: rodata).
void MSG_ReadDeltaSharedEntity(msg_s *msg, const void *from, void *to, int number)
{
    MSG_ReadDeltaStruct(msg, from, to, number, 0x44, 10, sharedEntityFields);
}

/* ======================================================================== */
// 0x08069f14  size=112  FUN_08069f14  status=translated  naming=inferred
// naming: clientState delta reader. Table clientStateFields has fields "team", "name[0]", "modelindex" (evidence: rodata).
// A NULL baseline is replaced by a zeroed 0x5c-byte record.
void MSG_ReadDeltaClientState(msg_s *msg, const void *from, void *to, int number)
{
    unsigned char zeroBase[108];

    if (from == NULL) {
        from = zeroBase;
        memset(zeroBase, 0, 0x5c);
    }
    MSG_ReadDeltaStruct(msg, from, to, number, 0x16, 6, clientStateFields);
}

/* ======================================================================== */
// 0x08069f84  size=332  FUN_08069f84  status=translated  naming=unknown
// naming: writer for an array of 0x80-byte records (32 dword fields; table recordFields32 has fields
// "x", "y", "type", "color.rgba"). Record kind not identified; the name describes the wire layout.
void MSG_WriteDeltaHudElemArray(msg_s *msg, const unsigned char *from, const unsigned char *to, int maxCount)
{
    int count;
    int i;
    int j;

    for (count = 0; count < maxCount && LoadI32(to, count * 0x80) != 0; count++) {
    }
    MSG_WriteBits(msg, count, 5);
    for (i = 0; i < count; i++) {
        int last = 0;
        for (j = 0; j < 0x20; j++) {
            if (LoadI32(from + i * 0x80, recordFields32[j].offset) != LoadI32(to + i * 0x80, recordFields32[j].offset)) {
                last = j;
            }
        }
        MSG_WriteBits(msg, last, 5);
        for (j = 0; j <= last; j++) {
            MSG_WriteDeltaField(msg, from + i * 0x80, to + i * 0x80, &recordFields32[j]);
        }
    }
}

/* ======================================================================== */
// 0x0806a0d0  size=304  FUN_0806a0d0  status=translated  naming=unknown
// naming: reader counterpart of MSG_WriteDeltaHudElemArray (same 0x80-byte record layout).
void MSG_ReadDeltaHudElemArray(msg_s *msg, const unsigned char *from, unsigned char *to, int maxCount)
{
    int count;
    int i;
    int j;

    count = MSG_ReadBits(msg, 5);
    for (i = 0; i < count; i++) {
        int last = MSG_ReadBits(msg, 5);
        j = 0;
        for (; j <= last; j++) {
            MSG_ReadDeltaField(msg, from + i * 0x80, to + i * 0x80, &recordFields32[j], 0);
        }
        for (; j < 0x20; j++) {
            StoreI32(to + i * 0x80, recordFields32[j].offset, LoadI32(from + i * 0x80, recordFields32[j].offset));
        }
    }
    memset(to + count * 0x80, 0, (size_t)((maxCount - count) * 0x80));
}

/* ======================================================================== */
// 0x0806a200  size=2249  FUN_0806a200  status=translated  naming=inferred
// naming: Q3 MSG_WriteDeltaClientState. Table playerStateFields starts with "commandTime" (evidence: rodata).
// Sub-groups: 6 misc dwords at 0x12c, two 4x16 dword masks at 0x144 and 0x344, 16 x 7-dword records
// at 0x5e4, and two 31-record arrays at 0x1728 and 0x7a8.
void MSG_WriteDeltaPlayerState(msg_s *msg, const unsigned char *from, const unsigned char *to)
{
    unsigned char zeroBase[9908];
    int lastChanged = 0;
    int i;
    int j;
    int mask;
    int masks[4];

    if (from == NULL) {
        from = zeroBase;
        memset(zeroBase, 0, kPlayerStateBytes);
    }
    for (i = 0; i < kPlayerStateFieldCount; i++) {
        if (LoadI32(from, playerStateFields[i].offset) != LoadI32(to, playerStateFields[i].offset)) {
            lastChanged = i + 1;
        }
    }
    MSG_WriteByte(msg, lastChanged);

    for (i = 0; i < lastChanged; i++) {
        const netField_s *field = &playerStateFields[i];
        int off = field->offset;
        float toValue = LoadF32(to, off);

        /* gameplay-fix(net_extra): the unchanged test is a raw dword compare (cmp eax,[edx] at 0x0806a316), not a float
         * compare: +0.0 vs -0.0 (and NaN fields) write a changed bit in the original, which changes the message size
         * and therefore the rate-limited snapshot schedule of non-LAN clients. The float path truncates (fistp with
         * RC=truncate, 0x0806a377) and compares in x87 precision; the angle path tests the raw dword for zero (0x0806a447). */
        if (LoadI32(from, off) == LoadI32(to, off)) {
            MSG_WriteBit0(msg);
            continue;
        }
        MSG_WriteBit1(msg);
        if (field->bits == 0) {
            int rounded = (int)toValue;
            if ((long double)rounded == (long double)toValue && -1 < rounded + 0x1000 && rounded + 0x1000 < 0x2000) {
                MSG_WriteBit0(msg);
                MSG_WriteBits(msg, rounded + 0x1000, 5);
                MSG_WriteByte(msg, (rounded + 0x1000) >> 5);
            } else {
                MSG_WriteBit1(msg);
                MSG_WriteLong(msg, FloatBits(toValue));
            }
        } else if (field->bits == -100) {
            if (LoadI32(to, off) == 0) {
                MSG_WriteBit0(msg);
            } else {
                MSG_WriteBit1(msg);
                MSG_WriteAngle16(msg, toValue);
            }
        } else {
            int value = LoadI32(to, off);
            int width = field->bits < 0 ? -field->bits : field->bits;
            int low = width & 7;
            int remaining = width;

            if (low != 0) {
                MSG_WriteBits(msg, value, low);
                remaining = width - low;
                value >>= low;
            }
            for (; remaining != 0; remaining -= 8) {
                MSG_WriteByte(msg, value);
                value >>= 8;
            }
        }
    }

    /* Six misc dwords at 0x12c..0x140. */
    mask = 0;
    for (i = 0; i < 6; i++) {
        if (LoadI32(to, 0x12c + i * 4) != LoadI32(from, 0x12c + i * 4)) {
            mask |= 1 << (i & 0x1f);
        }
    }
    if (mask == 0) {
        MSG_WriteBit0(msg);
    } else {
        MSG_WriteBit1(msg);
        MSG_WriteBits(msg, mask, 6);
        if (mask & 1) {
            MSG_WriteShort(msg, LoadI32(to, 0x12c));
        }
        if (mask & 2) {
            MSG_WriteShort(msg, LoadI32(to, 0x130));
        }
        if (mask & 4) {
            MSG_WriteShort(msg, LoadI32(to, 0x134));
        }
        if (mask & 8) {
            MSG_WriteBits(msg, LoadI32(to, 0x138), 6);
        }
        if (mask & 0x10) {
            MSG_WriteShort(msg, LoadI32(to, 0x13c));
        }
        if (mask & 0x20) {
            MSG_WriteByte(msg, LoadI32(to, 0x140));
        }
    }

    /* Four 16-dword groups at 0x144, outer flag then per-group flag. */
    for (i = 0; i < 4; i++) {
        masks[i] = DiffMask16(from, to, 0x144 + i * 64);
    }
    if (masks[0] == 0 && masks[1] == 0 && masks[2] == 0 && masks[3] == 0) {
        MSG_WriteBit0(msg);
    } else {
        MSG_WriteBit1(msg);
        for (i = 0; i < 4; i++) {
            if (masks[i] == 0) {
                MSG_WriteBit0(msg);
            } else {
                MSG_WriteBit1(msg);
                MSG_WriteShort(msg, masks[i]);
                for (j = 0; j < 16; j++) {
                    if (((masks[i] >> (j & 0x1f)) & 1) != 0) {
                        MSG_WriteShort(msg, LoadI32(to, 0x144 + (i * 16 + j) * 4));
                    }
                }
            }
        }
    }

    /* Four 16-dword groups at 0x344, per-group flag only. */
    for (i = 0; i < 4; i++) {
        mask = DiffMask16(from, to, 0x344 + i * 64);
        if (mask == 0) {
            MSG_WriteBit0(msg);
        } else {
            MSG_WriteBit1(msg);
            MSG_WriteShort(msg, mask);
            for (j = 0; j < 16; j++) {
                if (((mask >> (j & 0x1f)) & 1) != 0) {
                    MSG_WriteShort(msg, LoadI32(to, 0x344 + (i * 16 + j) * 4));
                }
            }
        }
    }

    /* Sixteen 7-dword records at 0x5e4. */
    if (memcmp(from + 0x5e4, to + 0x5e4, 0x1c0) == 0) {
        MSG_WriteBit0(msg);
    } else {
        MSG_WriteBit1(msg);
        for (i = 0; i < 16; i++) {
            MSG_WriteBits(msg, LoadI32(to, 0x5e4 + i * 0x1c), 3);
            MSG_WriteDeltaFieldGroup(msg, from + 0x5e4 + i * 0x1c, to + 0x5e4 + i * 0x1c, 0, 6, originFields);
        }
    }

    /* Two 31-record arrays at 0x1728 and 0x7a8. */
    if (memcmp(from + 0x7a8, to + 0x7a8, 0x1f00) == 0) {
        MSG_WriteBit0(msg);
    } else {
        MSG_WriteBit1(msg);
        MSG_WriteDeltaHudElemArray(msg, from + 0x1728, to + 0x1728, 0x1f);
        MSG_WriteDeltaHudElemArray(msg, from + 0x7a8, to + 0x7a8, 0x1f);
    }
}

/* ======================================================================== */
// 0x0806aaca  size=1634  FUN_0806aaca  status=translated  naming=inferred
// naming: Q3 MSG_ReadDeltaPlayerstate, reader counterpart of MSG_WriteDeltaPlayerState.
void MSG_ReadDeltaPlayerState(msg_s *msg, const unsigned char *from, unsigned char *to)
{
    unsigned char zeroBase[9908];
    int count;
    int i;
    int j;
    int mask;

    if (from == NULL) {
        from = zeroBase;
        memset(zeroBase, 0, kPlayerStateBytes);
    }
    memcpy(to, from, kPlayerStateBytes);

    count = MSG_ReadByte(msg);
    for (i = 0; i < count; i++) {
        const netField_s *field = &playerStateFields[i];
        int off = field->offset;

        if (MSG_ReadBit(msg) == 0) {
            StoreI32(to, off, LoadI32(from, off));
        } else if (field->bits == 0) {
            if (MSG_ReadBit(msg) == 0) {
                int v = MSG_ReadBits(msg, 5);
                v += MSG_ReadByte(msg) * 0x20 - 0x1000;
                StoreF32(to, off, (float)v);
            } else {
                StoreI32(to, off, MSG_ReadLong(msg));
            }
        } else if (field->bits == -100) {
            if (MSG_ReadBit(msg) == 0) {
                StoreF32(to, off, 0.0f);
            } else {
                StoreF32(to, off, MSG_ReadAngle16(msg));
            }
        } else {
            int isSigned = field->bits < 0;
            int width = isSigned ? -field->bits : field->bits;
            int low = width & 7;
            unsigned int value = 0;
            int shift;

            if (low != 0) {
                value = (unsigned int)MSG_ReadBits(msg, low);
            }
            for (shift = low; shift < width; shift += 8) {
                value |= (unsigned int)MSG_ReadByte(msg) << (shift & 0x1f);
            }
            if (isSigned && (((int)value >> ((width - 1) & 0x1f)) & 1) != 0) {
                value |= ~((1u << (width & 0x1f)) - 1u);
            }
            StoreI32(to, off, (int)value);
        }
    }
    for (i = count; i < kPlayerStateFieldCount; i++) {
        StoreI32(to, playerStateFields[i].offset, LoadI32(from, playerStateFields[i].offset));
    }

    /* Six misc dwords at 0x12c..0x140. */
    if (MSG_ReadBit(msg) != 0) {
        mask = MSG_ReadBits(msg, 6);
        if (mask & 1) {
            StoreI32(to, 0x12c, MSG_ReadShort(msg));
        }
        if (mask & 2) {
            StoreI32(to, 0x130, MSG_ReadShort(msg));
        }
        if (mask & 4) {
            StoreI32(to, 0x134, MSG_ReadShort(msg));
        }
        if (mask & 8) {
            StoreI32(to, 0x138, MSG_ReadBits(msg, 6));
        }
        if (mask & 0x10) {
            StoreI32(to, 0x13c, MSG_ReadShort(msg));
        }
        if (mask & 0x20) {
            StoreI32(to, 0x140, MSG_ReadByte(msg));
        }
    }

    /* Four 16-dword groups at 0x144. */
    if (MSG_ReadBit(msg) != 0) {
        for (i = 0; i < 4; i++) {
            if (MSG_ReadBit(msg) != 0) {
                mask = MSG_ReadShort(msg);
                for (j = 0; j < 16; j++) {
                    if (((mask >> (j & 0x1f)) & 1) != 0) {
                        StoreI32(to, 0x144 + (i * 16 + j) * 4, MSG_ReadShort(msg));
                    }
                }
            }
        }
    }

    /* Four 16-dword groups at 0x344. */
    for (i = 0; i < 4; i++) {
        if (MSG_ReadBit(msg) != 0) {
            mask = MSG_ReadShort(msg);
            for (j = 0; j < 16; j++) {
                if (((mask >> (j & 0x1f)) & 1) != 0) {
                    StoreI32(to, 0x344 + (i * 16 + j) * 4, MSG_ReadShort(msg));
                }
            }
        }
    }

    /* Sixteen 7-dword records at 0x5e4. */
    if (MSG_ReadBit(msg) != 0) {
        for (i = 0; i < 16; i++) {
            StoreI32(to, 0x5e4 + i * 0x1c, MSG_ReadBits(msg, 3));
            MSG_ReadDeltaFieldsAll(msg, from + 0x5e4 + i * 0x1c, to + 0x5e4 + i * 0x1c, 6, originFields);
        }
    }

    /* Two 31-record arrays at 0x1728 and 0x7a8. */
    if (MSG_ReadBit(msg) != 0) {
        MSG_ReadDeltaHudElemArray(msg, from + 0x1728, to + 0x1728, 0x1f);
        MSG_ReadDeltaHudElemArray(msg, from + 0x7a8, to + 0x7a8, 0x1f);
    }
}

/* ======================================================================== */
// 0x0806b12c  size=116  FUN_0806b12c  status=translated  naming=inferred
// naming: Q3 MSG_initHuffman. Feeds per-byte symbol counts (huff_symbol_counts) into two Huffman trees.
void MSG_InitHuffmanTables(void)
{
    int sym;
    int c;

    Huff_Init(&msgHuff);
    for (sym = 0; sym < 0x100; sym++) {
        for (c = 0; c < huff_symbol_counts[sym]; c++) {
            Huff_addRef((huffman_s *)&msgHuff, sym & 0xff);
            Huff_addRef((huffman_s *)msgHuffDecompressor, sym & 0xff);
        }
    }
}

/* ======================================================================== */
// 0x0806b1a0  size=23  FUN_0806b1a0  status=translated  naming=inferred
// naming: one-shot Huffman initialisation wrapper (sets msg_huffmanInitialized then calls MSG_InitHuffmanTables).
void MSG_InitHuffmanOnce(void)
{
    msg_huffmanInitialized = 1;
    MSG_InitHuffmanTables();
}

/* ======================================================================== */
// 0x0806b1b8  size=19  FUN_0806b1b8  status=translated  naming=unknown
// naming: 16-bit sign extension helper; no string or caller evidence for its purpose.
int LittleShort_0806b1b8(short value)
{
    return (int)value;
}

/* ======================================================================== */
// 0x0806b1cc  size=8  FUN_0806b1cc  status=translated  naming=unknown
// naming: 32-bit identity helper; used by the message write and read paths.
int LittleLong_0806b1cc(int value)
{
    return value;
}

/* ======================================================================== */
// 0x0806b1d4  size=262  FUN_0806b1d4  status=translated  naming=inferred
// naming: Q3 NET_AdrToString. Format strings "loopback" and "%i.%i.%i.%i:%i" (evidence: rodata).
char *NET_AdrToString(netadr_s adr)
{
    short port;

    if (adr.type == 2) {
        Com_sprintf(net_adrToStringBuf, 0x40, "loopback");
    } else if (adr.type == 4) {
        port = BigShort((int)(short)adr.port);
        Com_sprintf(net_adrToStringBuf, 0x40, "%i.%i.%i.%i:%i",
                    adr.ip[0], adr.ip[1], adr.ip[2], adr.ip[3], (int)port);
    } else {
        port = BigShort((int)(short)adr.port);
        Com_sprintf(net_adrToStringBuf, 0x40, "%02x%02x%02x%02x.%02x%02x%02x%02x%02x%02x:%i",
                    adr.ipx[0], adr.ipx[1], adr.ipx[2], adr.ipx[3], adr.ipx[4],
                    adr.ipx[5], adr.ipx[6], adr.ipx[7], adr.ipx[8], adr.ipx[9], (int)port);
    }
    return net_adrToStringBuf;
}

/* ======================================================================== */
// 0x0806b2da  size=239  FUN_0806b2da  status=translated  naming=inferred
// naming: Q3-lineage net profiling allocator. Strings "Net Profiling turned on/off" (evidence: rodata).
// Allocates a 0x5e0-byte profile block while net_profile is nonzero, frees it otherwise.
void NetProfile_Sync(netProfile_s **slot)
{
    if (cvar_net_profile->current.integer == 0) {
        if (net_profile_mode != 0) {
            net_profile_mode = 0;
            Com_Printf("Net Profiling turned off\n");
        }
        if (*slot != NULL) {
            free(*slot);
            *slot = NULL;
        }
    } else {
        if (net_profile_mode == 0) {
            if (!CvarLowByteSet(com_sv_running) ||
                (*(int *)((char *)g_0817d838 + 4) != 0 && cvar_net_profile->current.integer == 2)) {
                net_profile_mode = 1;
            } else {
                net_profile_mode = 2;
            }
            Com_Printf("Net Profiling turned on: %s\n", net_profile_names[net_profile_mode]);
        }
        if (*slot == NULL) {
            *slot = (netProfile_s *)malloc(sizeof(netProfile_s));
            memset(*slot, 0, sizeof(netProfile_s));
        }
    }
}

/* ======================================================================== */
// 0x0806b3ca  size=134  FUN_0806b3ca  status=translated  naming=inferred
// naming: ring-buffer sample recorder for one profile half (60 samples; time, bytes, fragmented).
void NetProfile_AddSample(netProfileStream_s *half, int bytes, int fragmented)
{
    int next = half->iCurrPacket + 1;

    half->iCurrPacket = next / kNetProfileSamples;
    half->iCurrPacket = next + half->iCurrPacket * -kNetProfileSamples;
    half->packets[half->iCurrPacket].iTime = Sys_Milliseconds();
    half->packets[half->iCurrPacket].iSize = bytes;
    half->packets[half->iCurrPacket].bFragment = fragmented;
}

/* ======================================================================== */
// 0x0806b450  size=130  FUN_0806b450  status=translated  naming=inferred
// naming: Q3-lineage send-side profile hook; prints "%s send%s: %i" with netsrc name (evidence: rodata).
void Netchan_ProfileSend(netchan_s *chan, int bytes, int fragmented)
{
    const char *fragText;

    if (net_profile_mode != 0) {
        NetProfile_AddSample(&chan->prof->send, bytes, fragmented);
        if (((unsigned int)cvar_net_showprofile->current.integer & 2u) != 0) {
            fragText = (fragmented == 0) ? "" : " fragment";
            Com_Printf("%s send%s: %i\n", net_sock_names[chan->sock], fragText, bytes);
        }
    }
}

/* ======================================================================== */
// 0x0806b4d2  size=135  FUN_0806b4d2  status=translated  naming=inferred
// naming: receive-side counterpart of Netchan_ProfileSend (prints "recieve", as in the binary).
void Netchan_ProfileRecv(netchan_s *chan, int bytes, int fragmented)
{
    const char *fragText;

    if (net_profile_mode != 0) {
        NetProfile_AddSample(&chan->prof->recieve, bytes, fragmented);
        if (((unsigned int)cvar_net_showprofile->current.integer & 2u) != 0) {
            fragText = (fragmented == 0) ? "" : " fragment";
            Com_Printf("%s recieve%s: %i\n", net_sock_names[chan->sock], fragText, bytes);
        }
    }
}

/* ======================================================================== */
// 0x0806b55a  size=824  FUN_0806b55a  status=translated  naming=inferred
// naming: Q3-lineage profile statistics over the 60-sample window of one half.
void NetProfile_ComputeStats(netProfileStream_s *half)
{
    int activeCount = 0;
    int fragmentedCount = 0;
    int oldestIndex = -1;
    int oldestTime = Sys_Milliseconds();
    int totalBytes = 0;
    int minBytes = 9999;
    int maxBytes = 0;
    int c;

    for (c = 0; c < kNetProfileSamples; c++) {
        if (half->packets[c].iTime != 0 && Sys_Milliseconds() <= half->packets[c].iTime + 1000) {
            activeCount++;
            if (half->packets[c].bFragment != 0) {
                fragmentedCount++;
            }
            if (half->packets[c].iTime < oldestTime) {
                oldestIndex = c;
                oldestTime = half->packets[c].iTime;
            }
            totalBytes += half->packets[c].iSize;
            if (half->packets[c].iSize < minBytes) {
                minBytes = half->packets[c].iSize;
            }
            if (maxBytes < half->packets[c].iSize) {
                maxBytes = half->packets[c].iSize;
            }
        }
    }

    if (activeCount == 0) {
        half->iFragmentPercentage = 0;
        half->iLargestPacket = 0;
        half->iSmallestPacket = 0;
        half->iBytesPerSecond = 0;
        half->iLastBPSCalcTime = 0;
        half->iCountedPackets = 0;
        half->iCountedFragments = 0;
        return;
    }

    if (fragmentedCount == 0) {
        half->iFragmentPercentage = 0;
    } else {
        half->iFragmentPercentage = (fragmentedCount * 100) / activeCount;
    }
    half->iLargestPacket = maxBytes;
    half->iSmallestPacket = minBytes;

    {
        int lastUpdate = half->iLastBPSCalcTime;
        int now = Sys_Milliseconds();

        if (lastUpdate + 100 < now) {
            int nowAgain = Sys_Milliseconds();

            if (oldestIndex != -1) {
                totalBytes -= half->packets[oldestIndex].iSize;
                activeCount--;
                if (half->packets[oldestIndex].bFragment != 0) {
                    fragmentedCount--;
                }
            }
            if (nowAgain - oldestTime < 1 || activeCount == 0) {
                half->iBytesPerSecond = 0;
            } else {
                if (totalBytes == 0) {
                    half->iBytesPerSecond = 0;
                } else {
                    /* x87 in the original: fild total; fild dt; fld 0.001f; fmulp; fdivp; fistp with RC=truncate (0x0806b838). */
                    half->iBytesPerSecond = (int)((long double)totalBytes /
                                                  ((long double)(nowAgain - oldestTime) * (long double)0.001f));
                }
                half->iLastBPSCalcTime = Sys_Milliseconds();
            }
        }
    }
    half->iCountedPackets = activeCount;
    half->iCountedFragments = fragmentedCount;
}

/* ======================================================================== */
// 0x0806b892  size=43  FUN_0806b892  status=translated  naming=observed
// naming: observed. Registered as command "net_dumpprofile" in NET_InitCvars; message text in rodata.
void NET_DumpProfile_f(void)
{
    if (net_profile_mode == 0) {
        Com_Printf("Network profiling is not on. Set net_profile to turn on network profiling\n");
    } else {
        SV_PrintNetworkProfile(1);
    }
}

/* ======================================================================== */
// 0x0806b8be  size=273  FUN_0806b8be  status=translated  naming=observed
// naming: observed. Registers cvars showpackets, showdrop, packetDebug, net_profile,
// net_showprofile, net_lanauthorize and command net_dumpprofile (string evidence, rodata).
void NET_InitCvars(unsigned int qport)
{
    cvar_showpackets = Dvar_RegisterBool("showpackets", NULL, 0x1000);
    cvar_showdrop = Dvar_RegisterBool("showdrop", NULL, 0x1000);
    cvar_packetDebug = Dvar_RegisterBool("packetDebug", NULL, 0x1000);
    net_qport = (unsigned short)(qport & 0xffff);
    cvar_net_profile = Dvar_RegisterInt("net_profile", NULL, 0, 2, 0x1000);
    cvar_net_showprofile = Dvar_RegisterInt("net_showprofile", NULL, 0, 3, 0x1000);
    cvar_net_lanauthorize = Dvar_RegisterBool("net_lanauthorize", NULL, 0x1000);
    Cmd_AddCommand("net_dumpprofile", NET_DumpProfile_f);
}

/* ======================================================================== */
// 0x0806b9d0  size=135  FUN_0806b9d0  status=translated  naming=inferred
// naming: Q3 Netchan_Setup. Clears the 0x40040-byte channel, stores sock, remote address (20 bytes),
// qport, sets incomingSequence = 0 and outgoingSequence = 1, then syncs net profiling.
void Netchan_Setup(int sock, netchan_s *chan, netadr_s remote, int qport)
{
    memset(chan, 0, 0x40040);
    chan->sock = sock;
    chan->remoteAddress = remote;
    chan->qport = qport;
    chan->incomingSequence = 0;
    chan->outgoingSequence = 1;
    NetProfile_Sync(&chan->prof);
}

/* ======================================================================== */
// 0x0806ba4c  size=543  FUN_0806ba4c  status=translated  naming=inferred
// naming: Q3 Netchan_TransmitNextFragment. Sends the next up-to-0x514-byte slice of the pending payload.
// Evidence: debug string "[client %i] %s send %4i : s=%i fragment=%i,%i" (rodata).
bool Netchan_TransmitNextFragment(netchan_s *chan)
{
    msg_s msg;
    unsigned char msgData[0x580];
    int fragLen = kNetMaxPacket;
    bool sent;

    NetProfile_Sync(&chan->prof);
    MSG_Init(&msg, msgData, kNetMaxMessage);
    MSG_WriteLong(&msg, chan->outgoingSequence | (int)0x80000000u);
    if (chan->sock < 1) {
        MSG_WriteShort(&msg, chan->qport);
    }
    if (chan->unsentLength < chan->unsentFragmentStart + kNetMaxPacket) {
        fragLen = chan->unsentLength - chan->unsentFragmentStart;
    }
    MSG_WriteLong(&msg, chan->unsentFragmentStart);
    MSG_WriteShort(&msg, fragLen);
    MSG_WriteData(&msg, chan->unsentBuffer + chan->unsentFragmentStart, fragLen);

    sent = NET_SendPacket(chan->sock, msg.cursize, msg.data, chan->remoteAddress) != 0;
    Netchan_ProfileSend(chan, msg.cursize, 1);
    if (CvarLowByteSet(cvar_showpackets)) {
        Com_Printf("[client %i] %s send %4i : s=%i fragment=%i,%i\n",
                    Net_CurrentClientNum(), net_sock_names[chan->sock], msg.cursize,
                    chan->outgoingSequence - 1, chan->unsentFragmentStart, fragLen);
    }
    chan->unsentFragmentStart += fragLen;
    if (chan->unsentFragmentStart == chan->unsentLength && fragLen != kNetMaxPacket) {
        chan->outgoingSequence += 1;
        chan->unsentFragments = 0;
    }
    return sent;
}

/* ======================================================================== */
// 0x0806bc6c  size=542  FUN_0806bc6c  status=translated  naming=inferred
// naming: Q3 Netchan_Transmit. Sends payloads up to 0x513 bytes directly, otherwise queues them and
// starts fragmented transmission. Evidence: "Netchan_Transmit: length = %i" (rodata).
bool Netchan_Transmit(netchan_s *chan, int length, const void *data)
{
    msg_s msg;
    unsigned char msgData[0x580];
    bool sent;

    if (length > kNetFragmentLimit) {
        Com_Error(1, "Netchan_Transmit: length = %i", length);
    }
    chan->unsentFragmentStart = 0;
    if (length < kNetMaxPacket) {
        NetProfile_Sync(&chan->prof);
        MSG_Init(&msg, msgData, kNetMaxMessage);
        MSG_WriteLong(&msg, chan->outgoingSequence);
        chan->outgoingSequence = chan->outgoingSequence + 1;
        if (chan->sock < 1) {
            MSG_WriteShort(&msg, chan->qport);
        }
        if (CvarLowByteSet(cvar_packetDebug)) {
            Com_Printf("Adding %i byte payload to packet\n", length);
        }
        MSG_WriteData(&msg, data, length);
        if (CvarLowByteSet(cvar_packetDebug)) {
            Com_Printf("Sending %i byte packet\n", msg.cursize);
        }
        sent = NET_SendPacket(chan->sock, msg.cursize, msg.data, chan->remoteAddress) != 0;
        Netchan_ProfileSend(chan, msg.cursize, 0);
        if (CvarLowByteSet(cvar_showpackets)) {
            Com_Printf("[client %i] %s send %4i : s=%i ack=%i\n",
                        Net_CurrentClientNum() + 1, net_sock_names[chan->sock], msg.cursize,
                        chan->outgoingSequence - 1, chan->incomingSequence);
        }
    } else {
        chan->unsentFragments = 1;
        chan->unsentLength = length;
        Com_Memcpy(chan->unsentBuffer, data, length);
        Netchan_TransmitNextFragment(chan);
        sent = true;
    }
    return sent;
}

/* ======================================================================== */
// 0x0806be8a  size=1220  FUN_0806be8a  status=translated  naming=inferred
// naming: Q3 Netchan_Process. Validates sequence, counts drops, reassembles fragments.
// Evidence: "[client %i] %s recv %4i : s=%i fragment=%i,%i" and out-of-order/drop messages (rodata).
int Netchan_Process(netchan_s *chan, msg_s *msg)
{
    unsigned int sequence;
    int fragmented;
    int fragIndex = 0;
    int fragLen = 0;

    NetProfile_Sync(&chan->prof);
    MSG_BeginReading(msg);
    sequence = (unsigned int)MSG_ReadLong(msg);
    fragmented = (sequence & 0x80000000u) != 0;
    if (fragmented) {
        sequence &= 0x7fffffffu;
    }
    if (chan->sock == 1) {
        MSG_ReadShort(msg);
    }
    if (fragmented) {
        fragIndex = MSG_ReadLong(msg);
        fragLen = MSG_ReadShort(msg);
    }

    Netchan_ProfileRecv(chan, msg->cursize, fragmented);
    if (CvarLowByteSet(cvar_showpackets)) {
        if (fragmented) {
            Com_Printf("[client %i] %s recv %4i : s=%i fragment=%i,%i\n",
                        Net_CurrentClientNum() + 1, net_sock_names[chan->sock], msg->cursize,
                        (int)sequence, fragIndex, fragLen);
        } else {
            Com_Printf("[client %i] %s recv %4i : s=%i\n",
                        Net_CurrentClientNum() + 1, net_sock_names[chan->sock], msg->cursize,
                        (int)sequence);
        }
    }

    if ((int)sequence <= chan->incomingSequence) {
        if (CvarLowByteSet(cvar_showdrop) || CvarLowByteSet(cvar_showpackets)) {
            const char *adrText = NET_AdrToString(chan->remoteAddress);
            Com_Printf("[client %i] %s:Out of order packet %i at %i\n",
                        Net_CurrentClientNum() + 1, adrText, (int)sequence, chan->incomingSequence);
        }
        return 0;
    }

    chan->dropped = ((int)sequence - chan->incomingSequence) - 1;
    if (0 < chan->dropped && (CvarLowByteSet(cvar_showdrop) || CvarLowByteSet(cvar_showpackets))) {
        const char *adrText = NET_AdrToString(chan->remoteAddress);
        Com_Printf("[client %i] %s: Dropped %i packets at %i\n",
                    Net_CurrentClientNum() + 1, adrText, chan->dropped, (int)sequence);
    }

    if (fragmented) {
        if ((int)sequence != chan->fragmentSequence) {
            chan->fragmentSequence = (int)sequence;
            chan->fragmentLength = 0;
        }
        if (fragIndex != chan->fragmentLength) {
            if (CvarLowByteSet(cvar_showdrop) || CvarLowByteSet(cvar_showpackets)) {
                const char *adrText = NET_AdrToString(chan->remoteAddress);
                Com_Printf("%s:Dropped a message fragment\n", adrText, (int)sequence);
            }
            return 0;
        }
        if (fragLen < 0 || msg->cursize < msg->readcount + fragLen ||
            kNetFragmentLimit < chan->fragmentLength + fragLen) {
            if (CvarLowByteSet(cvar_showdrop) || CvarLowByteSet(cvar_showpackets)) {
                const char *adrText = NET_AdrToString(chan->remoteAddress);
                Com_Printf("%s:illegal fragment length\n", adrText);
            }
            return 0;
        }
        memcpy(chan->fragmentBuffer + chan->fragmentLength, msg->data + msg->readcount, fragLen);
        chan->fragmentLength += fragLen;
        if (fragLen == kNetMaxPacket) {
            return 0;
        }
        if (msg->maxsize < chan->fragmentLength) {
            const char *adrText = NET_AdrToString(chan->remoteAddress);
            Com_Printf("%s:fragmentLength %i > msg->maxsize\n", adrText, chan->fragmentLength);
            return 0;
        }
        StoreI32(msg->data, 0, LittleLong_0806cf00((int)sequence));
        memcpy(msg->data + 4, chan->fragmentBuffer, chan->fragmentLength);
        msg->cursize = chan->fragmentLength + 4;
        chan->fragmentLength = 0;
        MSG_BeginReading(msg);
        MSG_ReadLong(msg);
    }
    chan->incomingSequence = (int)sequence;
    return 1;
}

/* ======================================================================== */
// 0x0806c34e  size=213  FUN_0806c34e  status=translated  naming=observed
// naming: observed. Anchor and error string "NET_CompareBaseAdrSigned: bad address type" (rodata).
int NET_CompareBaseAdrSigned(const netadr_s *a, const netadr_s *b)
{
    int result;

    if (a->type == b->type) {
        if (a->type == 2) {
            result = (int)((unsigned int)a->port - (unsigned int)b->port);
        } else if (a->type == 0) {
            result = (int)((unsigned int)a->port - (unsigned int)b->port);
        } else if (a->type == 4) {
            result = memcmp(a->ip, b->ip, 4);
        } else if (a->type == 5) {
            result = memcmp(a->ipx, b->ipx, 10);
        } else {
            Com_Printf("NET_CompareBaseAdrSigned: bad address type\n");
            result = 0;
        }
    } else {
        result = a->type - b->type;
    }
    return result;
}

/* ======================================================================== */
// 0x0806c424  size=45  FUN_0806c424  status=translated  naming=inferred
// naming: Q3 NET_CompareBaseAdr. Thin wrapper over NET_CompareBaseAdrSigned (evidence: call structure).
bool NET_CompareBaseAdr(netadr_s a, netadr_s b)
{
    return NET_CompareBaseAdrSigned(&a, &b) == 0;
}

/* ======================================================================== */
// 0x0806c452  size=249  FUN_0806c452  status=translated  naming=observed
// naming: observed. Anchor and error string "NET_CompareAdrSigned: bad address type" (rodata).
int NET_CompareAdrSigned(const netadr_s *a, const netadr_s *b)
{
    int result;

    if (a->type == b->type) {
        if (a->type == 2) {
            result = 0;
        } else if (a->type == 4) {
            if ((short)a->port == (short)b->port) {
                result = memcmp(a->ip, b->ip, 4);
            } else {
                result = (int)((unsigned int)a->port - (unsigned int)b->port);
            }
        } else if (a->type == 5) {
            if ((short)a->port == (short)b->port) {
                result = memcmp(a->ipx, b->ipx, 10);
            } else {
                result = (int)((unsigned int)a->port - (unsigned int)b->port);
            }
        } else {
            Com_Printf("NET_CompareAdrSigned: bad address type\n");
            result = 0;
        }
    } else {
        result = a->type - b->type;
    }
    return result;
}

/* ======================================================================== */
// 0x0806c54c  size=45  FUN_0806c54c  status=translated  naming=inferred
// naming: Q3 NET_CompareAdr. Thin wrapper over NET_CompareAdrSigned (evidence: call structure).
bool NET_CompareAdr(netadr_s a, netadr_s b)
{
    return NET_CompareAdrSigned(&a, &b) == 0;
}

/* ======================================================================== */
// 0x0806c57a  size=37  FUN_0806c57a  status=translated  naming=inferred
// naming: Q3 NET_IsLocalAddress. Returns true for address types 2 (loopback) and 0.
// The routine reads only the type dword of the by-value netadr_s argument.
int NET_IsLocalAddress(netadr_s adr)
{
    int result = 0;

    if (adr.type == 2 || adr.type == 0) {
        result = 1;
    }
    return result;
}

/* ======================================================================== */
// 0x0806c5ba  size=325  FUN_0806c5ba  status=translated  naming=inferred
// naming: Q3 NET_GetLoopPacket. 16-slot loopback queue per index; clamps get to send-16.
bool NET_GetLoopPacket(int sock, netadr_s *from, msg_s *msg)
{
    loopback_s *loop;
    unsigned int slot;
    int get;
    int send;

    loop = &net_loopbacks[sock];
    if (0x10 < loop->send - loop->get) {
        loop->get = loop->send - 0x10;
    }
    get = loop->get;
    send = loop->send;
    if (get < send) {
        slot = (unsigned int)loop->get & 0xf;
        loop->get = loop->get + 1;
        memcpy(msg->data, loop->msgs[slot].data, loop->msgs[slot].datalen);
        msg->cursize = loop->msgs[slot].datalen;
        memset(from, 0, sizeof(netadr_s));
        from->type = 2;
        from->port = (unsigned short)loop->msgs[slot].source;
    }
    return get < send;
}

/* ======================================================================== */
// 0x0806c700  size=33  FUN_0806c700  status=translated  naming=inferred
// naming: wrapper that discards NET_GetLoopPacket's result (called from the receive loop).
int NET_PollLoopPacket(int sock, netadr_s *from, msg_s *msg)
{
    return NET_GetLoopPacket(sock, from, msg);
}

/* ======================================================================== */
// 0x0806c722  size=202  FUN_0806c722  status=translated  naming=inferred
// naming: Q3 NET_SendLoopPacket. The queue index is the sock value, or the port when sock == 1.
// Parameters after data are not read by the routine (only the low 16 bits of the last one are used).
void NET_SendLoopPacket(unsigned int sock, size_t length, const void *data, int unusedType,
                        int unusedIp, unsigned short toPort)
{
    unsigned int index = sock;
    int tag = 0;
    unsigned int slot;
    loopback_s *loop;

    (void)unusedType;
    (void)unusedIp;
    if ((int)index < 1) {
        tag = (int)index;
        index = 1;
    } else if (index == 1) {
        index = toPort;
    }
    loop = &net_loopbacks[index];
    slot = (unsigned int)loop->send & 0xf;
    loop->send = loop->send + 1;
    memcpy(loop->msgs[slot].data, data, length);
    loop->msgs[slot].datalen = (int)length;
    loop->msgs[slot].source = tag;
}

/* ======================================================================== */
// 0x0806c7ec  size=224  FUN_0806c7ec  status=translated  naming=inferred
// naming: Q3 NET_SendPacket. Routes loopback to the loop queue; types 0 and 1 are dropped;
// other types go to Sys_SendPacket (the IP transport). Debug text "[client %i] send packet %4i".
byte NET_SendPacket(int sock, int length, const void *data, netadr_s to)
{
    byte result;

    if (CvarLowByteSet(cvar_showpackets) && *(const int *)data == -1) {
        Com_Printf("[client %i] send packet %4i\n", Net_CurrentClientNum(), length);
    }
    if (to.type == 2) {
        NET_SendLoopPacket((unsigned int)sock, (size_t)length, data, 2, 0, to.port);
        result = 1;
    } else if (to.type == 1) {
        result = 0;
    } else if (to.type == 0) {
        result = 0;
    } else {
        result = Sys_SendPacket(length, data, to);
    }
    return result;
}

/* ======================================================================== */
// 0x0806c8cc  size=330  FUN_0806c8cc  status=translated  naming=inferred
// naming: Q3 NET_OutOfBandPrint. Sends 0xffffffff-prefixed text. Evidence: "OOB Print: %s" and
// "OOB Packet is %i bytes - too large to send" (rodata).
bool NET_OutOfBandPrint(int sock, netadr_s to, const char *text)
{
    int buf;
    byte *out;
    size_t textLen;
    size_t total;
    byte result;
    bool sent;

    Com_PoolAllocInto2(&buf, kNetFragmentLimit);
    out = (byte *)Com_PoolPtrFromOffset(&buf);
    out[0] = 0xff;
    out[1] = 0xff;
    out[2] = 0xff;
    out[3] = 0xff;
    if (CvarLowByteSet(cvar_showpackets)) {
        Com_DPrintf("OOB Print: %s\n", text);
    }
    textLen = strlen(text);
    if (textLen + 1 < 0x1fffd) {
        textLen = strlen(text);
        memcpy(out + 4, text, textLen + 1);
        total = strlen((const char *)out);
        result = NET_SendPacket(sock, (int)total, out, to);
        if (sock == 1) {
            SV_NetProfile_RecordOOB((int)total);
        }
        sent = 0 < (int)result;
        Com_PoolRestoreFrom2(&buf);
    } else {
        Com_DPrintf("OOB Packet is %i bytes - too large to send\n", (int)strlen(text));
        Com_PoolRestoreFrom2(&buf);
        sent = false;
    }
    return sent;
}

/* ======================================================================== */
// 0x0806ca3e  size=242  FUN_0806ca3e  status=translated  naming=inferred
// naming: Q3-lineage OOB raw-data sender (0xffffffff prefix, no length check).
bool NET_OutOfBandData(int sock, netadr_s to, const unsigned char *data, int len)
{
    int buf;
    byte *out;
    int total;
    byte result;
    bool sent;

    Com_PoolAllocInto2(&buf, kNetFragmentLimit);
    out = (byte *)Com_PoolPtrFromOffset(&buf);
    out[0] = 0xff;
    out[1] = 0xff;
    out[2] = 0xff;
    out[3] = 0xff;
    memcpy(out + 4, data, len);
    total = len + 4;
    result = NET_SendPacket(sock, total, out, to);
    if (sock == 1) {
        SV_NetProfile_RecordOOB(total);
    }
    sent = 0 < (int)result;
    Com_PoolRestoreFrom2(&buf);
    return sent;
}

/* ======================================================================== */
// 0x0806cb5c  size=237  FUN_0806cb5c  status=translated  naming=inferred
// naming: Q3-lineage OOB send. Evidence: "OOB Packet is %i bytes - too large to send" (rodata).
void NET_OutOfBandSend(int sock, netadr_s to, const void *data, int len)
{
    int buf;
    byte *out;

    Com_PoolAllocInto2(&buf, 0x20004);
    out = (byte *)Com_PoolPtrFromOffset(&buf);
    if (len < 0x20001) {
        out[0] = 0xff;
        out[1] = 0xff;
        out[2] = 0xff;
        out[3] = 0xff;
        memcpy(out + 4, data, len);
        NET_SendPacket(sock, len + 4, out, to);
        if (sock == 1) {
            SV_NetProfile_RecordOOB(len + 4);
        }
        Com_PoolRestoreFrom2(&buf);
    } else {
        Com_DPrintf("OOB Packet is %i bytes - too large to send\n", len);
        Com_PoolRestoreFrom2(&buf);
    }
}

/* ======================================================================== */
// 0x0806cc7a  size=237  FUN_0806cc7a  status=translated  naming=observed
// naming: observed. Evidence: "OOB Voice Packet is %i bytes - too large to send" (rodata).
void NET_OutOfBandVoice(int sock, netadr_s to, const void *data, int len)
{
    int buf;
    byte *out;

    Com_PoolAllocInto2(&buf, 0x20004);
    out = (byte *)Com_PoolPtrFromOffset(&buf);
    if (len < 0x20001) {
        out[0] = 0xff;
        out[1] = 0xff;
        out[2] = 0xff;
        out[3] = 0xff;
        memcpy(out + 4, data, len);
        NET_SendPacket(sock, len + 4, out, to);
        if (sock == 1) {
            SV_NetProfile_RecordOOB(len + 4);
        }
        Com_PoolRestoreFrom2(&buf);
    } else {
        Com_DPrintf("OOB Voice Packet is %i bytes - too large to send\n", len);
        Com_PoolRestoreFrom2(&buf);
    }
}

/* ======================================================================== */
// 0x0806cd98  size=360  FUN_0806cd98  status=translated  naming=inferred
// naming: Q3 NET_StringToAdr. Handles "localhost", "host[:port]" with default port 0x7120 (evidence: rodata).
int NET_StringToAdr(const char *text, netadr_s *adr)
{
    char copy[1036];
    char *colon;
    int ok;
    int result;

    if (strcmp(text, "localhost") == 0) {
        memset(adr, 0, sizeof(netadr_s));
        adr->type = 2;
        result = 1;
    } else {
        Q_strncpyz(copy, text, 0x400);
        colon = strstr(copy, ":");
        if (colon != NULL) {
            *colon = '\0';
            colon = colon + 1;
        }
        ok = Sys_StringToAdr(copy, adr);
        if (ok == 0) {
            adr->type = 1;
            result = 0;
        } else if (adr->ip[0] == 0xff && adr->ip[1] == 0xff && adr->ip[2] == 0xff && adr->ip[3] == 0xff) {
            adr->type = 1;
            result = 0;
        } else {
            if (colon == NULL) {
                adr->port = (unsigned short)BigShort(0x7120);
            } else {
                adr->port = (unsigned short)BigShort((int)(short)atoi(colon));
            }
            result = 1;
        }
    }
    return result;
}

/* ======================================================================== */
// 0x0806cf00  size=8  FUN_0806cf00  status=translated  naming=unknown
// naming: identity on the sequence value; stored into the reassembled message header.
int LittleLong_0806cf00(int value)
{
    return value;
}

/* ======================================================================== */
// 0x0806cf08  size=10  FUN_0806cf08  status=translated  naming=unknown
// naming: returns 0 in this binary; used as the client number in debug output.
int Net_CurrentClientNum(void)
{
    return 0;
}

/* ======================================================================== */
// 0x0806cf14  size=5  FUN_0806cf14  status=translated  naming=unknown
// naming: empty function kept as a call target.
void Net_Noop(void)
{
}

/* ======================================================================== */
// 0x0806cf1a  size=24  FUN_0806cf1a  status=translated  naming=unknown
// naming: returns the address and count of the static 0x38-byte record g_08205600.
void Net_GetStaticRecord(void **record, int *count)
{
    *record = g_08205600;
    *count = g_08205638;
}

/* ======================================================================== */
// 0x0806cf32  size=46  FUN_0806cf32  status=translated  naming=unknown
// naming: clears the static 0x38-byte record g_08205600 and its count.
void Net_ClearStaticRecord(void)
{
    memset(g_08205600, 0, 0x38);
    g_08205638 = 0;
}

/* ======================================================================== */
// 0x0806cf60  size=13  FUN_0806cf60  status=translated  naming=unknown
// naming: stores the anim parse flag g_animParseSkipPrimitives (read by Anim_ParseBlock).
void Anim_SetParseFlag(int value)
{
    g_animParseSkipPrimitives = value;
}

/* ======================================================================== */
// 0x0806cf6e  size=55  FUN_0806cf6e  status=translated  naming=inferred
// naming: anim-script error reporter; reports the cursor column and the message (evidence: "Scr_AnimTreeParse").
void Anim_ParseError(const char *message)
{
    int cursorColumn = (int)(long)Com_GetLastTokenPos();

    Com_EndParseSession();
    ScrCompile_ErrorInFile(cursorColumn - (int)(long)g_animParseTextStart, "%s", message);
}

/* ======================================================================== */
// 0x0806cfa6  size=180  FUN_0806cfa6  status=translated  naming=inferred
// naming: parser for animation property words; words are loopsync, nonloopsync, complete (evidence: rodata).
unsigned int Anim_ParseProperties(void)
{
    unsigned int flags = 0;

    for (;;) {
        char *token = Com_ParseOnLine((const char **)&g_animParseCursor);
        unsigned int index;

        if (*token == '\0') {
            return flags;
        }
        index = 0;
        while (index < 3 && strcasecmp(token, anim_property_words[index]) != 0) {
            index++;
        }
        if (index == 1) {
            flags |= 2;
            continue;
        }
        if (index > 1) {
            if (index == 2) {
                flags |= 8;
                continue;
            }
        } else if (index == 0) {
            flags |= 1;
            continue;
        }
        Anim_ParseError("unknown anim property");
    }
}

/* ======================================================================== */
// 0x0806d05a  size=130  FUN_0806d05a  status=translated  naming=unknown
// naming: links a new anim node into the name's list, or creates the list entry (kind 7).
// Anim_LinkTreeEntry is named by structure only.
struct anim_link_s {
    int *node;
    int kind;
};

void Anim_LinkTreeEntry(int *node, int name, int tree)
{
    int existing = ScrVar_FindVarValueId(tree, name);

    if (existing == 0) {
        int list = ScrVar_NewChildAVarId(tree, name);
        anim_link_s link;

        *node = 0;
        link.node = node;
        link.kind = 7;
        ScrVar_SetVarValueReleasing(list, (VariableValue *)&link);
    } else {
        int *head = (int *)ScrVar_GetValuePtr(existing);

        *node = *head;
        *head = PtrToInt(node);
    }
}

/* ======================================================================== */
// 0x0806d0dc  size=72  FUN_0806d0dc  status=translated  naming=observed
// naming: observed. Error text "#using_animtree was not specified" (rodata).
void Anim_AddNodeToCurrentTree(int *node, int name, int line)
{
    if (g_08205648 == NULL) {
        ScrCompile_ErrorInFile(line, "#using_animtree was not specified");
    } else {
        Anim_LinkTreeEntry(node, name, PtrToInt(g_08205648));
    }
}

/* ======================================================================== */
// 0x0806d124  size=919  FUN_0806d124  status=translated  naming=inferred
// naming: recursive anim-block parser. Words "void" / "void_loop" (evidence: rodata). Returns 1 when the
// input ends before the closing brace, 0 when a closing brace terminates the block.
byte Anim_ParseBlock(int node, int tree, char emitDefaultVoid, char loopsync, char forceReset)
{
    char *token;
    char *peek;
    const char *voidName;
    unsigned int props = 0;
    int entry[4];
    byte isPrimitive = 0;
    byte result = 1;
    byte primTmp;
    char treeTest;
    int blockFlags;
    int nestedFlags;
    int nestedNode;
    int iVar4;

    entry[0] = 0;
    entry[1] = 6;
    entry[2] = 0;
    entry[3] = 0;

LAB_top:
    token = Com_Parse((const char **)&g_animParseCursor);
    if (g_animParseCursor != NULL) {
        treeTest = Scr_IsIdentifier(token);
        if (treeTest != 0) {
            goto code_name;
        }
        goto LAB_block;
    }
    result = 1;

LAB_end:
    if (isPrimitive != 0) {
        ScrVar_RemoveVar(node, entry[2]);
    }
    if (emitDefaultVoid != 0 && Scr_GetArraySize(node) == 0) {
        voidName = (loopsync == 0) ? "void" : "void_loop";
        entry[2] = ScrStr_InternCStringEx(voidName, 0, 4);
        ScrVar_GetNewVarValueId(node, entry[2]);
        ScrStr_Release(entry[2]);
    }
    return result;

code_name:
    if (isPrimitive != 0) {
        ScrVar_RemoveVar(node, entry[2]);
    }
    entry[2] = ScrStr_InternLowerCStringEx(token, 2, 4);
    iVar4 = ScrVar_FindVarValueId(node, entry[2]);
    if (iVar4 != 0) {
        Anim_ParseError("duplicate animation");
    }
    entry[3] = ScrVar_GetNewVarValueId(node, entry[2]);
    primTmp = 0;
    if (forceReset == 0 && ScrVar_FindVarValueId(tree, entry[2]) == 0 && g_animParseSkipPrimitives == 0) {
        primTmp = 1;
    }
    isPrimitive = primTmp;
    props = 0;
    peek = Com_ParseOnLine((const char **)&g_animParseCursor);
    token = peek;
    if (*token != '\0') {
        treeTest = Scr_IsIdentifier(token);
        if (treeTest != 0) {
            Anim_ParseError("FIXME: aliases not yet implemented");
        }
        if (*token != ':' || token[1] != '\0') {
            Anim_ParseError("bad token");
        }
        props = Anim_ParseProperties();
        token = Com_Parse((const char **)&g_animParseCursor);
        if (*token != '{' || token[1] != '\0') {
            Anim_ParseError("properties cannot be applied to primitive animations");
        }
        goto LAB_block;
    }
    goto LAB_top;

LAB_block:
    if (*token == '{') {
        if (token[1] != '\0') {
            Anim_ParseError("bad token");
        }
        peek = Com_ParseOnLine((const char **)&g_animParseCursor);
        if (*peek != '\0') {
            Anim_ParseError("token not allowed after '{'");
        }
        if (entry[3] == 0) {
            Anim_ParseError("no animation specified for this block");
        }
        nestedNode = Scr_GetOrCreateArrayId(entry[3]);
        nestedFlags = 0;
        if (forceReset != 0 || ((props & 8) != 0 && isPrimitive == 0)) {
            nestedFlags = 1;
        }
        blockFlags = Anim_ParseBlock(nestedNode, tree, (char)(isPrimitive ^ 1),
                                     (char)(props & 1), (char)nestedFlags);
        if (blockFlags != 0) {
            Anim_ParseError("unexpected end of file");
        }
        iVar4 = Scr_GetArraySize(nestedNode);
        if (iVar4 == 0) {
            ScrVar_RemoveVar(node, entry[2]);
        } else {
            entry[0] = (int)props;
            nestedNode = ScrVar_GetOrNewVarValueId24(nestedNode, 0);
            ScrVar_SetVarValueReleasing(nestedNode, (VariableValue *)entry);
        }
        entry[3] = 0;
        isPrimitive = 0;
        goto LAB_top;
    }
    if (*token == '}') {
        if (token[1] != '\0') {
            Anim_ParseError("bad token");
        }
        peek = Com_ParseOnLine((const char **)&g_animParseCursor);
        if (*peek != '\0') {
            Anim_ParseError("token not allowed after '}'");
        }
        result = 0;
        goto LAB_end;
    }
    Anim_ParseError("bad token");
    goto LAB_top;
}

/* ======================================================================== */
// 0x0806d4bc  size=96  FUN_0806d4bc  status=translated  naming=observed
// naming: observed. Profiler/marker string "Scr_AnimTreeParse" passed to Com_BeginParseSession (rodata anchor).
void Scr_AnimTreeParse(char *text, int node, int tree)
{
    byte ended;

    Com_BeginParseSession("Scr_AnimTreeParse");
    g_animParseCursor = text;
    g_animParseTextStart = text;
    ended = Anim_ParseBlock(node, tree, 1, 0, 0);
    if (ended == 0) {
        Anim_ParseError("bad token");
    }
    Com_EndParseSession();
}

/* ======================================================================== */
// 0x0806d51c  size=27  FUN_0806d51c  status=translated  naming=inferred
// naming: per-entry callback that frees an anim node (calls Hunk_AllocAlign with size 4).
void *Anim_FreeEntry(int value)
{
    return Hunk_AllocAlign((size_t)value, 4);
}

/* ======================================================================== */
// 0x0806d538  size=141  FUN_0806d538  status=translated  naming=unknown
// naming: recursive count of tree entries (leaves count 1; a non-empty subtree adds one).
int Anim_CountTreeEntries(int node)
{
    int count = 0;
    int child;

    for (child = Scr_GetNextSibling(node); child != 0; child = Scr_GetNextSibling(child)) {
        unsigned int key = Scr_GetVarNameId(child);
        if (key < 0x10000) {
            if (Scr_GetVarType(child) == 1) {
                int sub = Scr_GetVarValue(child);
                count += Anim_CountTreeEntries(sub);
            } else {
                count += 1;
            }
        }
    }
    if (count != 0) {
        count += 1;
    }
    return count;
}

/* ======================================================================== */
// 0x0806d5c6  size=182  FUN_0806d5c6  status=translated  naming=inferred
// naming: walks a pending anim-node list, overwriting each link with (kind << 16 | value) and
// clearing the list head. Reports "duplicate animation '%s' in 'animtrees/%s.atr'" if the list is empty.
void Anim_SetNodeListValue(int tree, unsigned short low16, int treeName, int animName, unsigned short high16)
{
    int found = ScrVar_FindVarValueId(tree, animName);
    int *head;

    if (found != 0) {
        head = (int *)ScrVar_GetValuePtr(found);
        if (*head == 0) {
            const char *treeStr = ScrStr_TextFromHandle(treeName);
            const char *animStr = ScrStr_TextFromHandle(animName);
            Com_Error(1, "duplicate animation '%s' in 'animtrees/%s.atr'", animStr, treeStr);
        }
        {
            int *cur = (int *)IntToPtr(*head);

            while (cur != NULL) {
                int *next = (int *)IntToPtr(*cur);

                *cur = (int)(((unsigned int)high16 << 16) | (unsigned int)low16);
                cur = next;
            }
        }
        *head = 0;
    }
}

/* ======================================================================== */
// 0x0806d67c  size=64  FUN_0806d67c  status=translated  naming=unknown
// naming: scans g_0820584c downward from g_08205a50 for value; returns the index (0 when absent).
int Anim_FindTreeSlot(int value)
{
    int index;

    for (index = g_08205a50; index != 0 && g_0820584c[index] != value; index--) {
    }
    return index;
}

/* ======================================================================== */
// 0x0806d6bc  size=15  FUN_0806d6bc  status=translated  naming=unknown
// naming: returns g_0820584c[index].
int Anim_GetTreeSlot(int index)
{
    return g_0820584c[index];
}

/* ======================================================================== */
// 0x0806d6cc  size=600  FUN_0806d6cc  status=translated  naming=inferred
// naming: recursive anim-tree hash and emit walk. Mixes counts into g_scr_code_hash with multiplier 0x1f
// and emits entries with XAnim_SetLeafNode / XAnim_SetGroupNode. Structure only; no string evidence.
int Anim_HashAndEmitTree(int node, int tree, int a3, int index, int a5, int base, int treeName, int a8)
{
    int leafCount = 0;
    int child;
    unsigned short baseKey;
    int sub;
    int cur = index;
    unsigned int key;

    for (child = Scr_GetNextSibling(node); child != 0; child = Scr_GetNextSibling(child)) {
        key = Scr_GetVarNameId(child);
        if (key < 0x10000) {
            leafCount++;
        }
    }
    {
        int first = ScrVar_FindVarValueId24(node, 0);

        if (first == 0) {
            baseKey = 0;
        } else {
            baseKey = *(unsigned short *)ScrVar_GetValuePtr(first);
        }
    }
    g_scr_code_hash = g_scr_code_hash * 0x1f + (unsigned int)base;
    g_scr_code_hash = g_scr_code_hash * 0x1f + (unsigned int)index;
    g_scr_code_hash = g_scr_code_hash * 0x1f + (unsigned int)leafCount;
    g_scr_code_hash = g_scr_code_hash * 0x1f + (unsigned int)baseKey;
    XAnim_SetGroupNode((struct XAnimNodeTable *)(long)a3, base, (const char *)(long)a5, index, leafCount, baseKey);

    base = index;
    cur = index + leafCount;
    for (child = Scr_GetNextSibling(node); child != 0; child = Scr_GetNextSibling(child)) {
        key = Scr_GetVarNameId(child);
        if (key < 0x10000) {
            Anim_SetNodeListValue(tree, (unsigned short)base, treeName, (int)(key & 0xffff), (unsigned short)a8);
            if (Scr_GetVarType(child) == 1) {
                const char *keyStr = ScrStr_TextFromHandle((int)key);
                sub = Scr_GetVarValue(child);
                cur = Anim_HashAndEmitTree(sub, tree, a3, cur, (int)(long)keyStr, base, treeName, a8);
            } else {
                const char *keyStr;

                g_scr_code_hash = g_scr_code_hash * 0x1f + (unsigned int)base;
                keyStr = ScrStr_TextFromHandle((int)key);
                XAnim_SetLeafNode((struct XAnimNodeTable *)(long)a3, base, keyStr);
            }
            base = base + 1;
        }
    }
    return cur;
}

/* ======================================================================== */
// 0x0806d924  size=216  FUN_0806d924  status=translated  naming=observed
// naming: observed. Error text "animation '%s' not defined in anim tree '%s'" (rodata).
void Anim_ReportUnresolved(int tree, int treeName)
{
    int entry;
    int *head;
    int key;
    const char *message;

    for (entry = Scr_GetNextSibling(tree); entry != 0; entry = Scr_GetNextSibling(entry)) {
        key = (int)Scr_GetVarNameId(entry);
        head = (int *)ScrVar_GetValuePtr(entry);
        if (*head != 0) {
            const char *treeStr = ScrStr_TextFromHandle(treeName);
            const char *keyStr = ScrStr_TextFromHandle(key);

            message = va("animation '%s' not defined in anim tree '%s'", keyStr, treeStr);
            if (Scr_IsInCodeRange(*head) == 0) {
                Com_Error(1, "%s", message);
            } else {
                ScrCompile_ErrorAt((const char *)(long)*head, "%s", message);
            }
        }
    }
}

/* ======================================================================== */
// 0x0806d9fc  size=135  FUN_0806d9fc  status=translated  naming=inferred
// naming: recursive release of an anim tree; leaf names are passed to Anim_FreeEntry through XAnim_FindOrLoad.
void Anim_ReleaseTreeEntries(int node)
{
    int entry;

    for (entry = Scr_GetNextSibling(node); entry != 0; entry = Scr_GetNextSibling(entry)) {
        unsigned int key = Scr_GetVarNameId(entry);
        if (key < 0x10000) {
            if (Scr_GetVarType(entry) == 1) {
                int sub = Scr_GetVarValue(entry);
                Anim_ReleaseTreeEntries(sub);
            } else {
                const char *keyStr = ScrStr_TextFromHandle((int)key);
                XAnim_FindOrLoad(keyStr, (void *(*)(int))Anim_FreeEntry);
            }
        }
    }
}
