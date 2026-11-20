/*
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 032.
 * Range 0x080bc58a..0x080c03d8: XAnim node tables, track sampling, blend-slot pool,
 * part-state evaluation. Binary SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * Conventions verified against the disassembly (not the Ghidra signatures):
 *  - Ghidra "ROUND(x)" is a truncation: the x87 control word is set to chop (0xc00) before fistp.
 *    It is translated as a C (int) cast.
 *  - Ghidra "longdouble" returns are x87 st(0) values; they are translated as long double.
 *  - A "node" at table + 0x0c + 8k also serves as a clip/track holder (+4 pointer).
 * Float expressions keep the decompiler's operand order; x87 extended-precision intermediates
 * that the original may keep in registers are not reproduced bit-for-bit.
 */
#include <stdint.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_032_types.h"

/* Strings used by this chunk (rodata). The leading 0x15 byte is part of the original text. */
static const char kDefaultXAnimName[] = "void";

void XAnim_FreeNodeTableNames(XAnimNodeTable *tbl);

static inline XAnimNode *xanim_node_at(XAnimNodeTable *t, int k)
{
    return (XAnimNode *)((char *)t + 0x0c) + k;
}

static inline const XAnimNode *xanim_node_at_c(const XAnimNodeTable *t, int k)
{
    return (const XAnimNode *)((const char *)t + 0x0c) + k;
}

/* Track value block: inline at +4 when count == 0, otherwise after count entries of `stride`. */
static inline void *track_value_block(const xanim_track_s *t, int stride)
{
    if (t->count == 0) {
        return (void *)&t->data;
    }
    return (char *)t->data + (uint32)t->count * stride;
}

/* Byte-keyed and ushort-keyed key tables start at the track's +8 position. */
static inline const uint8 *track_key_bytes(const xanim_track_s *t)
{
    return t->key_table;
}

static inline const uint16 *track_key_words(const xanim_track_s *t)
{
    return (const uint16 *)t->key_table;
}

/* Bit patterns of the float constants used by this chunk (rodata 0x08150018..0x0815002c). */
static inline float f32_from_bits(uint32 bits)
{
    union { uint32 u; float f; } v;
    v.u = bits;
    return v.f;
}
static const float kTimeScale = f32_from_bits(0x38000100u);   /* 3.051851e-05f = 2^-15 scale */
static const float kAccumScale = f32_from_bits(0x30800200u);  /* 9.313794e-10f */
static const float kBlendFloor = f32_from_bits(0x358637beu);  /* 1.0000001e-06f */
static const float kBlendScale = f32_from_bits(0x3a83126fu);  /* 0.001f */

// 0x080bc58a  size=311  FUN_080bc58a  status=translated  naming=unknown
// naming: no string or callee name evidence; the body is a quaternion-style product with the cross-product terms in the reversed sign convention (b = a * b written back into b).
void Quat_MultiplyInPlace_080bc58a(const float *a, float *b)
{
    float a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
    float b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
    b[3] = ((a3 * b3 - a0 * b0) - a1 * b1) - a2 * b2;
    b[0] = ((a2 * b1 + a3 * b0) + a0 * b3) - a1 * b2;
    b[1] = (a0 * b2 + a3 * b1) + (a1 * b3 - a2 * b0);
    b[2] = a3 * b2 + ((a1 * b0 + a2 * b3) - a0 * b1);
}

// 0x080bc6c4  size=305  FUN_080bc6c4  status=translated  naming=inferred
// naming: builds the 4096-entry blend-slot ring (g_xanimSlotRing, 40-byte records, slot 0 is the sentinel) and reads com_developer+8 as a developer flag; slot names are not present, so the name comes from the pool role. Handle name "end" is rodata at 0x0814ff80.
void XAnim_InitSlotPool(void)
{
    for (int i = 0; i < 0x1000; i++) {
        g_xanimSlotRing[i].list_prev = (int16)((i + 0xfff) & 0xfff);
        g_xanimSlotRing[i].list_next = (int16)((i + 1) & 0xfff);
    }
    g_xanimSlotRing[0].window.time = 0.0f;
    g_xanimSlotRing[0].window.last_time = 0.0f;
    g_xanimSlotRing[0].window.index = 0;
    g_xanimSlotRing[0].window.last_index = 0;
    g_xanimEndNotifyHandle = ScrStr_InternCStringEx("end", 0, 3);
    g_xanimDebugNames = (uint8)(com_developer->current.integer != 0);
}

// 0x080bc7f6  size=40  FUN_080bc7f6  status=translated  naming=inferred
// naming: releases the handle created by XAnim_InitSlotPool (paired with it by the g_xanimEndNotifyHandle global); shutdown role inferred from the pairing.
void XAnim_ShutdownSlotPool(void)
{
    if (g_xanimEndNotifyHandle != 0) {
        ScrStr_Release(g_xanimEndNotifyHandle);
        g_xanimEndNotifyHandle = 0;
    }
}

// 0x080bc81e  size=15  FUN_080bc81e  status=translated  naming=unknown
// naming: clears g_xanimEndNotifyHandle without releasing it; no string evidence, name describes the store only.
void XAnim_ClearEndNotifyString(void)
{
    g_xanimEndNotifyHandle = 0;
}

// 0x080bc82e  size=135  FUN_080bc82e  status=translated  naming=inferred
// naming: releases every handle held by an xanim asset table (handle list at +0x10 and entry list at +0x1c); the same layout is cloned by XAnim_CloneHandleTable, so the release role is inferred.
void XAnim_ReleaseHandleTable(XAnimParts *t)
{
    int c;
    for (c = 0; c < t->handle_count; c++) {
        ScrStr_Release(t->handles[c]);
    }
    int16 e;
    xanim_handle_entry_s *entry = t->entries;
    for (e = 0; e < (int16)(uint16)t->entry_count; e++) {
        ScrStr_Release(entry->handle);
        entry++;
    }
}

// 0x080bc8b6  size=27  FUN_080bc8b6  status=translated  naming=inferred
// naming: thin wrapper that passes type 5 (xanim) and the name to the asset lookup; the caller strings "Couldn't find xanim" (0x0814ffa0) and "Cannot find xanim" (0x08150000) identify the asset kind.
XAnimParts *XAnim_Find(const char *name)
{
    return (XAnimParts *)Hunk_FindUserEntryByName(5, name);
}

// 0x080bc8d2  size=180  FUN_080bc8d2  status=translated  naming=inferred
// naming: copies a 0x2c-byte asset table through the allocator callback and takes one reference on each handle it holds; the xanim naming comes from its callers in XAnim_FindOrLoad.
XAnimParts *XAnim_CloneHandleTable(const XAnimParts *src, void *(*alloc)(int))
{
    XAnimParts *copy = (XAnimParts *)alloc(0x2c);
    uint32 *dst_words = (uint32 *)copy;
    const uint32 *src_words = (const uint32 *)src;
    int w;
    for (w = 0; w < 0x0b; w++) {
        dst_words[w] = src_words[w];
    }
    int c;
    for (c = 0; c < (int)copy->handle_count; c++) {
        ScrStr_AddRef(copy->handles[c]);
    }
    int16 e = 0;
    xanim_handle_entry_s *entry = copy->entries;
    for (; e < (int16)(uint16)copy->entry_count; e++) {
        ScrStr_AddRef(entry->handle);
        entry++;
    }
    return copy;
}

// 0x080bc986  size=298  FUN_080bc986  status=translated  naming=observed
// naming: string "^3WARNING: Couldn't find xanim '%s', using default xanim '%s' instead\n" (0x0814ffa0) names the function's purpose: find or load an xanim by name with a default fallback named "void".
XAnimParts *XAnim_FindOrLoad(const char *name, void *(*alloc)(int))
{
    XAnimParts *found = XAnim_Find(name);
    if (found == 0) {
        XAnimParts *local = (XAnimParts *)XAnim_LoadParts(name, alloc);
        if (local == 0) {
            Com_Printf("^3WARNING: Couldn't find xanim '%s', using default xanim '%s' instead\n",
                        name, kDefaultXAnimName);
            XAnimParts *fallback = XAnim_Find(kDefaultXAnimName);
            if (fallback == 0) {
                fallback = (XAnimParts *)XAnim_LoadParts(kDefaultXAnimName, alloc);
                if (fallback == 0) {
                    Com_Error(1, "\x15" "Cannot find xanim '%s'.", kDefaultXAnimName);
                    return 0;
                }
                Hunk_AddUserEntry(5, kDefaultXAnimName, fallback, alloc);
            }
            local = XAnim_CloneHandleTable(fallback, alloc);
            local->is_fallback = 1;
        }
        uint32 handle = Hunk_AddUserEntry(5, name, local, alloc);
        local->handle = handle;
        found = local;
    }
    return found;
}

// 0x080bcab0  size=161  FUN_080bcab0  status=translated  naming=inferred
// naming: stores a found xanim as a leaf node (count 0, payload = asset) and copies its name when debug names are on; the error text "Cannot find xanim '%s'" at 0x08150000 names the failure path.
void XAnim_SetLeafNode(XAnimNodeTable *tbl, int k, const char *name)
{
    XAnimParts *asset = XAnim_Find(name);
    if (asset == 0) {
        Com_Error(1, "\x15" "Cannot find xanim '%s'", name);
        return;
    }
    XAnimNode *node = xanim_node_at(tbl, k);
    node->child_count = 0;
    node->u.leaf_asset = asset;
    if (tbl->node_names != 0) {
        size_t len = strlen(name);
        char *copy = (char *)Com_MallocZeroed((int)(len + 1));
        strcpy(copy, name);
        tbl->node_names[k] = copy;
    }
}

// 0x080bcb52  size=171  FUN_080bcb52  status=translated  naming=inferred
// naming: defines a group node (child count, mode flags, first child) and writes its index onto each child's aux field; the name is copied for debug builds. Group role inferred from the child-range loop.
void XAnim_SetGroupNode(XAnimNodeTable *tbl, int k, const char *name, uint16 first_child,
                        uint32 child_count, uint16 mode_flags)
{
    XAnimNode *node = xanim_node_at(tbl, k);
    node->child_count = (uint16)child_count;
    node->u.group.mode_flags = mode_flags;
    node->u.group.first_child = first_child;
    uint32 j;
    for (j = 0; j < child_count; j++) {
        xanim_node_at(tbl, (int)(uint16)node->u.group.first_child + (int)j)->aux_index = (int16)k;
    }
    if (tbl->node_names != 0) {
        size_t len = strlen(name);
        char *copy = (char *)Com_MallocZeroed((int)(len + 1));
        strcpy(copy, name);
        tbl->node_names[k] = copy;
    }
}

// 0x080bcbfe  size=170  FUN_080bcbfe  status=translated  naming=inferred
// naming: allocates a node table with node_count 8-byte nodes after a 0x0c header; debug name and per-node name array are added when the debug flag g_xanimDebugNames is set. Table role inferred from the 0x0c/8k layout shared with XAnim_SetLeafNode.
XAnimNodeTable *XAnim_AllocNodeTable(const char *name, int node_count, void *(*alloc)(int))
{
    XAnimNodeTable *tbl = (XAnimNodeTable *)alloc(node_count * 8 + 0x0c);
    tbl->node_count = node_count;
    if (g_xanimDebugNames != 0) {
        size_t len = strlen(name);
        char *copy = (char *)Com_MallocZeroed((int)(len + 1));
        strcpy(copy, name);
        tbl->debug_name = copy;
        tbl->node_names = (char **)Com_MallocZeroed(node_count << 2);
    }
    int registered = Hunk_IsInHeap(tbl);
    if (registered != 0) {
        Hunk_AddAnonymousUserEntry(6, tbl, alloc);
    }
    return tbl;
}

// 0x080bcca8  size=52  FUN_080bcca8  status=translated  naming=unknown
// naming: frees a node table's names then calls the caller's init callback with the table and its new byte size; no string evidence.
void XAnim_ReinitNodeTable(XAnimNodeTable *tbl, void (*init)(void *, int))
{
    int count = tbl->node_count;
    XAnim_FreeNodeTableNames(tbl);
    init(tbl, count * 8 + 0x0c);
}

// 0x080bccdc  size=172  FUN_080bccdc  status=translated  naming=unknown
// naming: frees the debug name and the per-node name array of a node table, clearing each pointer; the free order is the name, then each entry, then the array.
void XAnim_FreeNodeTableNames(XAnimNodeTable *tbl)
{
    if (tbl->debug_name != 0) {
        Com_Free_A(tbl->debug_name);
        tbl->debug_name = 0;
    }
    if (tbl->node_names != 0) {
        uint32 k;
        for (k = 0; k < (uint32)tbl->node_count; k++) {
            if (tbl->node_names[k] != 0) {
                Com_Free_A(tbl->node_names[k]);
                tbl->node_names[k] = 0;
            }
        }
        Com_Free_A(tbl->node_names);
        tbl->node_names = 0;
    }
}

// 0x080bcd88  size=12  FUN_080bcd88  status=translated  naming=unknown
// naming: sets the byte flag g_085e4f25 to 1; no string evidence.
void XAnim_SetUnnamedFlag25(void)
{
    g_085e4f25 = 1;
}

// 0x080bcd94  size=39  FUN_080bcd94  status=translated  naming=unknown
// naming: byte size of a block for `count` entries: 5 bytes per entry plus a 9-byte header.
int32 XAnim_BlockSizeForCount(int32 count)
{
    return count * 5 + 9;
}

// 0x080bcdbc  size=82  FUN_080bcdbc  status=translated  naming=unknown
// naming: allocates a zeroed block sized from the owner's count and stores the owner pointer in its first word.
void **XAnim_AllocBlock(XAnimNodeTable *owner, void *(*alloc)(int))
{
    int32 size = XAnim_BlockSizeForCount(owner->node_count);
    void **block = (void **)alloc(size);
    memset(block, 0, size);
    block[0] = owner;
    return block;
}

// 0x080bce0e  size=70  FUN_080bce0e  status=translated  naming=unknown
// naming: releases a block and, when a callback is given, reinitialises it with the size taken before the release.
void XAnim_ReinitBlock(void **block, void (*init)(void *, int))
{
    uint32 count = (uint32)((XAnimNodeTable *)block[0])->node_count;
    XAnim_ResetTree(block);
    if (init != 0) {
        count = (uint32)XAnim_BlockSizeForCount((int32)count);
        init(block, (int)count);
    }
}

// 0x080bce54  size=10  FUN_080bce54  status=translated  naming=unknown
// naming: returns the first word of a block (its owner pointer).
uint32 XAnim_GetBlockOwnerWord(const uint32 *block)
{
    return *block;
}

// 0x080bce5e  size=584  FUN_080bce5e  status=translated  naming=unknown
// naming: builds the part-index remap for a node: for each linked child group it assigns the child part ids to slots (first occurrence wins) and returns the ScrStr_Intern handle of the bitset followed by the per-part slot map.
uint32 XAnim_BuildPartMap(XAnimNode *node, void **links, int link_count)
{
    xanim_clip_s *clip = node->u.clip;
    const uint16 *part_ids = clip->part_ids;
    int part_count = clip->part_count;
    /* One block, as in the original frame: the 128-bit written mask at [ebp-0x98] and the per-part slot map right
     * after it at [ebp-0x88]; ScrStr_Intern stores part_count + 0x10 bytes from the mask, so the map must follow it. */
    union {
        uint32 words[4 + 0x84 / 4];
        uint8 bytes[0x10 + 0x84];
    } block;
    uint32 *bits = block.words;
    uint8 *slot_of = block.bytes + 0x10;
    int i;
    for (i = 0; i < 4; i++) {
        bits[i] = 0;
    }
    for (i = part_count - 1; i >= 0; i--) {
        slot_of[i] = 0x7f;
    }
    int next_slot = 0;
    int link;
    for (link = 0; link < link_count; link++) {
        XAnimNode *child = *(XAnimNode **)links[link];
        const uint16 *child_ids = (const uint16 *)(uintptr_t)*(const uint32 *)child->u.clip;
        int child_len = child->child_count;
        int j;
        for (j = 0; j < child_len; j++) {
            int search = part_count;
            while (--search >= 0) {
                if (child_ids[j] == part_ids[search]) {
                    if (slot_of[search] == 0x7f) {
                        slot_of[search] = (uint8)next_slot;
                        bits[next_slot >> 5] = (1u << ((uint8)next_slot & 0x1f)) | bits[next_slot >> 5];
                    }
                    break;
                }
            }
            next_slot++;
        }
    }
    return ScrStr_Intern(bits, 0, part_count + 0x10, 0xb);
}

// 0x080bd0a6  size=72  FUN_080bd0a6  status=translated  naming=unknown
// naming: fills a 16-byte frame key from (value, int scale): value, scale, value*scale, and the truncated integer frame (fistp under chop rounding).
void XAnim_MakeFrameKey(float value, int32 scale, xanim_frame_key_s *out)
{
    out->value = value;
    out->scale = scale;
    out->scaled = value * (float)scale;
    /* the original reloads the stored float (fld [out+8]) before the truncating fistp */
    out->frame = (int32)*(volatile float *)&out->scaled;
}

// 0x080bd0ee  size=86  FUN_080bd0ee  status=translated  naming=unknown
// naming: applies a stride-4 track value to an output (the sample block follows count entries; inline when count is 0).
void XAnim_TrackValueStride4(const xanim_track_s *t, float scale, void *out)
{
    const void *src = track_value_block(t, 4);
    XAnim_MulAddShort2(out, scale, src, out);
}

// 0x080bd144  size=98  FUN_080bd144  status=translated  naming=unknown
// naming: applies a stride-8 (rotation) track value to an output; same sample-block rule as XAnim_TrackValueStride4.
void XAnim_TrackValueStride8(const xanim_track_s *t, float scale, void *out)
{
    const void *src = track_value_block(t, 8);
    XAnim_MulAddShort4(out, scale, src, out);
}

// 0x080bd1a6  size=104  FUN_080bd1a6  status=translated  naming=unknown
// naming: applies a stride-12 (three-float) track value to an output; same sample-block rule.
void XAnim_TrackValueStride12(const xanim_track_s *t, float scale, void *out)
{
    const void *src = track_value_block(t, 12);
    XAnim_Vec3MulAdd(out, scale, src, out);
}

// 0x080bd20e  size=319  FUN_080bd20e  status=translated  naming=unknown
// naming: evaluates every unmasked part of a clip at one time: rotation (or scalar) and position tracks plus time accumulation into the part state; the part map gives the output slot, the bitset selects the track kind.
void XAnim_ApplyPartTracks(const xanim_clip_s *clip, const uint8 *part_map, float time,
                           xanim_part_state_s *out, const uint32 *skip_mask)
{
    int part;
    for (part = 0; part < clip->part_count; part++) {
        uint8 slot = part_map[part];
        if (((int)skip_mask[slot >> 5] >> (slot & 0x1f) & 1u) != 0) {
            continue;
        }
        xanim_part_state_s *state = out + slot;
        const xanim_part_track_s *pair = clip->part_tracks + part;
        int bit = (int)(int8)clip->part_bitset[part >> 3] >> (part & 7) & 1;
        float scaled_time = time * kTimeScale;
        if (bit == 0) {
            XAnim_TrackValueStride8(pair->rot, scaled_time, state);
        } else if (pair->rot == 0) {
            state->time_a = state->time_a + time;
        } else {
            XAnim_TrackValueStride4(pair->rot, scaled_time, &state->scalar);
        }
        if (pair->pos != 0) {
            XAnim_TrackValueStride12(pair->pos, time, state->pos);
        }
        state->time_b = state->time_b + time;
    }
}

// 0x080bd34e  size=111  FUN_080bd34e  status=translated  naming=unknown
// naming: (3rd argument is the calc buffer; the skip mask is read at buf+0x4010 per the disassembly) resets the state of every part that is not in the skip mask (zero rotation origin, zero position, zero time).
void XAnim_ResetUnmaskedParts(const xanim_anim_state_s *anim, xanim_part_state_s *out,
                              const xanim_calc_buffer_s *buf)
{
    int part;
    for (part = 0; part < (int)anim->part_count; part++) {
        if (((int)buf->skip_mask[part >> 5] >> (part & 0x1f) & 1u) == 0) {
            XAnim_Vec4Zero(out);
            out->time_b = 0.0f;
            XAnim_Vec3Zero(out->pos);
        }
        out++;
    }
}

// 0x080bd3be  size=72  FUN_080bd3be  status=translated  naming=unknown
// naming: copies a stride-4 track value (inline when count is 0) to an output.
void XAnim_TrackCopy4(const xanim_track_s *t, void *out)
{
    Short2_ToFloat2(track_value_block(t, 4), out);
}

// 0x080bd406  size=63  FUN_080bd406  status=translated  naming=unknown
// naming: copies the rotation (+4) track of a part track pair, or returns (0, 32767) when the pair has none.
void XAnim_PairRotCopy4(const xanim_part_track_s *pair, float *out)
{
    if (pair->rot == 0) {
        out[0] = 0.0f;
        out[1] = 32767.0f;
    } else {
        XAnim_TrackCopy4(pair->rot, out);
    }
}

// 0x080bd446  size=76  FUN_080bd446  status=translated  naming=unknown
// naming: copies a stride-12 track value (inline when count is 0) to an output.
void XAnim_TrackCopy12(const xanim_track_s *t, void *out)
{
    XAnim_Vec3Copy(track_value_block(t, 12), out);
}

// 0x080bd492  size=49  FUN_080bd492  status=translated  naming=unknown
// naming: copies a track held in a pointer slot, or clears the output to zero vector when the slot is empty.
void XAnim_TrackSlotCopy12(xanim_track_s *const *slot, void *out)
{
    if (*slot == 0) {
        XAnim_Vec3Zero(out);
    } else {
        XAnim_TrackCopy12(*slot, out);
    }
}

// 0x080bd4c4  size=372  FUN_080bd4c4  status=translated  naming=unknown
// naming: binary search of a byte key table (bytes are window boundaries) for the entry containing key position; returns the index and the fractional position in that window. Truncation of count*value is the chop-rounded fistp.
void XAnim_LocateKeyBytes(const xanim_frame_key_s *key, const uint8 *table, int count,
                          int32 *out_index, float *out_frac)
{
    int32 local_c = (int32)((float)count * key->value);
    int32 frame = key->frame;
    int32 local_8;
    int32 local_14;
    if (frame < (int32)table[local_c]) {
        local_8 = 0;
        local_14 = local_c;
        for (;;) {
            for (;;) {
                local_c = (local_8 + local_14) / 2;
                if (!(frame < (int32)table[local_c])) {
                    break;
                }
                local_14 = local_c;
            }
            if (frame < (int32)table[local_c + 1]) {
                break;
            }
            local_8 = local_c + 1;
            local_14 = local_14 - 1;
            if (!(frame < (int32)table[local_14])) {
                local_c = local_14;
                break;
            }
        }
    } else if ((int32)table[local_c + 1] <= frame) {
        local_14 = count;
        int32 iv = local_c + 1;
        for (;;) {
            for (;;) {
                local_8 = iv;
                local_c = (local_8 + local_14) / 2;
                if (!((int32)table[local_c] <= frame)) {
                    break;
                }
                if (frame < (int32)table[local_c + 1]) {
                    goto found;
                }
                iv = local_c + 1;
            }
            local_14 = local_c;
            iv = local_8 + 1;
            if (!((int32)table[local_8 + 1] <= frame)) {
                local_c = local_8;
                break;
            }
        }
    }
found:
    *out_frac = (key->scaled - (float)table[local_c]) /
                (float)(int32)((uint32)table[local_c + 1] - (uint32)table[local_c]);
    *out_index = local_c;
}

// 0x080bd638  size=90  FUN_080bd638  status=translated  naming=unknown
// naming: locates the key for a frame, using the fast path (last window) when the count is not below the frame scale.
void XAnim_LocateKeyOrEndBytes(const xanim_frame_key_s *key, const uint8 *table, int count,
                               int32 *out_index, float *out_frac)
{
    if (count < key->scale) {
        XAnim_LocateKeyBytes(key, table, count, out_index, out_frac);
    } else {
        *out_frac = key->scaled - (float)key->frame;
        *out_index = key->frame;
    }
}

// 0x080bd692  size=163  FUN_080bd692  status=translated  naming=unknown
// naming: evaluates a stride-4 track at (value, scale) with byte keys: inline value when count is 0, otherwise interpolates the two samples around the located key.
void XAnim_EvalTrackStride4Bytes(const xanim_track_s *t, float value, int32 scale, void *out)
{
    if (t->count == 0) {
        Short2_ToFloat2(&t->data, out);
        return;
    }
    xanim_frame_key_s key;
    XAnim_MakeFrameKey(value, scale, &key);
    int32 idx;
    float frac;
    XAnim_LocateKeyOrEndBytes(&key, track_key_bytes(t), t->count, &idx, &frac);
    char *data = (char *)t->data;
    XAnim_DequantPair(data + idx * 4, data + idx * 4 + 4, frac, out);
}

// 0x080bd736  size=77  FUN_080bd736  status=translated  naming=unknown
// naming: part-pair wrapper for XAnim_EvalTrackStride4Bytes on the rotation track; no rotation track returns (0, 32767).
void XAnim_EvalPairRot4Bytes(const xanim_part_track_s *pair, float value, int32 scale, float *out)
{
    if (pair->rot == 0) {
        out[0] = 0.0f;
        out[1] = 32767.0f;
    } else {
        XAnim_EvalTrackStride4Bytes(pair->rot, value, scale, out);
    }
}

// 0x080bd784  size=178  FUN_080bd784  status=translated  naming=unknown
// naming: stride-12 variant of XAnim_EvalTrackStride4Bytes (three-float samples, vector interpolation).
void XAnim_EvalTrackStride12Bytes(const xanim_track_s *t, float value, int32 scale, void *out)
{
    if (t->count == 0) {
        XAnim_Vec3Copy(&t->data, out);
        return;
    }
    xanim_frame_key_s key;
    XAnim_MakeFrameKey(value, scale, &key);
    int32 idx;
    float frac;
    XAnim_LocateKeyOrEndBytes(&key, track_key_bytes(t), t->count, &idx, &frac);
    char *data = (char *)t->data;
    XAnim_Vec3Lerp(data + idx * 12, data + idx * 12 + 12, frac, out);
}

// 0x080bd836  size=63  FUN_080bd836  status=translated  naming=unknown
// naming: slot-pointer wrapper for XAnim_EvalTrackStride12Bytes; an empty slot yields the zero vector.
void XAnim_EvalSlotStride12Bytes(xanim_track_s *const *slot, float value, int32 scale, void *out)
{
    if (*slot == 0) {
        XAnim_Vec3Zero(out);
    } else {
        XAnim_EvalTrackStride12Bytes(*slot, value, scale, out);
    }
}

// 0x080bd876  size=422  FUN_080bd876  status=translated  naming=unknown
// naming: ushort-keyed version of XAnim_LocateKeyBytes for counts of 0x100 and above.
void XAnim_LocateKeyWords(const xanim_frame_key_s *key, const uint16 *table, int count,
                          int32 *out_index, float *out_frac)
{
    int32 local_10 = (int32)((float)count * key->value);
    int32 frame = key->frame;
    int32 local_c;
    int32 local_18;
    if (frame < (int32)table[local_10]) {
        local_c = 0;
        local_18 = local_10;
        for (;;) {
            for (;;) {
                local_10 = (local_c + local_18) / 2;
                if (!(frame < (int32)table[local_10])) {
                    break;
                }
                local_18 = local_10;
            }
            if (frame < (int32)table[local_10 + 1]) {
                break;
            }
            local_c = local_10 + 1;
            local_18 = local_18 - 1;
            if (!(frame < (int32)table[local_18])) {
                local_10 = local_18;
                break;
            }
        }
    } else if ((int32)table[local_10 + 1] <= frame) {
        local_18 = count;
        int32 iv = local_10 + 1;
        for (;;) {
            for (;;) {
                local_c = iv;
                local_10 = (local_c + local_18) / 2;
                if (!((int32)table[local_10] <= frame)) {
                    break;
                }
                if (frame < (int32)table[local_10 + 1]) {
                    goto found;
                }
                iv = local_10 + 1;
            }
            local_18 = local_10;
            iv = local_c + 1;
            if (!((int32)table[local_c + 1] <= frame)) {
                local_10 = local_c;
                break;
            }
        }
    }
found:
    *out_frac = (key->scaled - (float)table[local_10]) /
                (float)(int32)((uint32)table[local_10 + 1] - (uint32)table[local_10]);
    *out_index = local_10;
}

// 0x080bda1c  size=90  FUN_080bda1c  status=translated  naming=unknown
// naming: ushort-keyed version of XAnim_LocateKeyOrEndBytes.
void XAnim_LocateKeyOrEndWords(const xanim_frame_key_s *key, const uint16 *table, int count,
                               int32 *out_index, float *out_frac)
{
    if (count < key->scale) {
        XAnim_LocateKeyWords(key, table, count, out_index, out_frac);
    } else {
        *out_frac = key->scaled - (float)key->frame;
        *out_index = key->frame;
    }
}

// 0x080bda76  size=163  FUN_080bda76  status=translated  naming=unknown
// naming: ushort-keyed version of XAnim_EvalTrackStride4Bytes.
void XAnim_EvalTrackStride4Words(const xanim_track_s *t, float value, int32 scale, void *out)
{
    if (t->count == 0) {
        Short2_ToFloat2(&t->data, out);
        return;
    }
    xanim_frame_key_s key;
    XAnim_MakeFrameKey(value, scale, &key);
    int32 idx;
    float frac;
    XAnim_LocateKeyOrEndWords(&key, track_key_words(t), t->count, &idx, &frac);
    char *data = (char *)t->data;
    XAnim_DequantPair(data + idx * 4, data + idx * 4 + 4, frac, out);
}

// 0x080bdb1a  size=77  FUN_080bdb1a  status=translated  naming=unknown
// naming: part-pair wrapper for XAnim_EvalTrackStride4Words on the rotation track.
void XAnim_EvalPairRot4Words(const xanim_part_track_s *pair, float value, int32 scale, float *out)
{
    if (pair->rot == 0) {
        out[0] = 0.0f;
        out[1] = 32767.0f;
    } else {
        XAnim_EvalTrackStride4Words(pair->rot, value, scale, out);
    }
}

// 0x080bdb68  size=178  FUN_080bdb68  status=translated  naming=unknown
// naming: ushort-keyed version of XAnim_EvalTrackStride12Bytes.
void XAnim_EvalTrackStride12Words(const xanim_track_s *t, float value, int32 scale, void *out)
{
    if (t->count == 0) {
        XAnim_Vec3Copy(&t->data, out);
        return;
    }
    xanim_frame_key_s key;
    XAnim_MakeFrameKey(value, scale, &key);
    int32 idx;
    float frac;
    XAnim_LocateKeyOrEndWords(&key, track_key_words(t), t->count, &idx, &frac);
    char *data = (char *)t->data;
    XAnim_Vec3Lerp(data + idx * 12, data + idx * 12 + 12, frac, out);
}

// 0x080bdc1a  size=63  FUN_080bdc1a  status=translated  naming=unknown
// naming: slot-pointer wrapper for XAnim_EvalTrackStride12Words.
void XAnim_EvalSlotStride12Words(xanim_track_s *const *slot, float value, int32 scale, void *out)
{
    if (*slot == 0) {
        XAnim_Vec3Zero(out);
    } else {
        XAnim_EvalTrackStride12Words(*slot, value, scale, out);
    }
}

// 0x080bdc5a  size=249  FUN_080bdc5a  status=translated  naming=unknown
// naming: evaluates a keyed part at a time value: rotation and position pairs are sampled with the byte-keyed path for counts below 0x100 and the ushort-keyed path otherwise; a value of 1.0 or no keys samples the static values.
void XAnim_EvalKeyedPart(const xanim_keyed_part_s *part, float value, float *out_a, void *out_b)
{
    uint16 count = part->key_count;
    const xanim_part_track_s *pair = part->tracks;
    bool use_static = false;
    if (value == 1.0f || count == 0) {
        use_static = true;
    }
    if (use_static) {
        XAnim_PairRotCopy4(pair, out_a);
        XAnim_TrackSlotCopy12(&pair->pos, out_b);
    } else if (count < 0x100) {
        XAnim_EvalPairRot4Bytes(pair, value, (int32)count, out_a);
        XAnim_EvalSlotStride12Bytes(&pair->pos, value, (int32)count, out_b);
    } else {
        XAnim_EvalPairRot4Words(pair, value, (int32)count, out_a);
        XAnim_EvalSlotStride12Words(&pair->pos, value, (int32)count, out_b);
    }
}

// 0x080bdd54  size=174  FUN_080bdd54  status=translated  naming=unknown
// naming: applies a 2D reflection-style update of (p[1], p[0]) from the normalised direction given by the first pair; no string evidence.
void XAnim_ReflectPair(const float *dir, float *v)
{
    /* x87 store points of the original (fstp to the stack frame) are kept with volatile temporaries. */
    volatile float xx = dir[0] * dir[0];
    volatile float len2 = dir[1] * dir[1] + xx;
    if (len2 != 0.0f) {
        volatile float k = 2.0f / len2;
        volatile float scaledXx;
        volatile float xy;
        volatile float newV0;
        scaledXx = xx * k;
        xy = dir[0] * dir[1] * k;
        newV0 = (1.0f - scaledXx) * v[0] + xy * v[1];
        v[1] = v[1] - (scaledXx * v[1] + xy * v[0]);
        v[0] = newV0;
    }
}

// 0x080bde02  size=381  FUN_080bde02  status=translated  naming=unknown
// naming: samples a keyed part at two times, accumulates the position delta between them (with the wrap correction when the part wraps) into the motion accumulator, and advances the rotation/position pair.
void XAnim_AccumulatePartMotion(const xanim_keyed_part_s *part, float scale, float time0, float time1,
                                float *accum, int which)
{
    float local_1c[6];
    float local_2c[4];
    float local_3c[4];
    float local_4c[4];
    XAnim_EvalKeyedPart(part, time0, local_1c, local_2c);
    XAnim_EvalKeyedPart(part, time1, local_1c + 2, local_3c);
    if (part->wrap_flag != 0 && time1 < time0) {
        const xanim_track_s *pos = part->tracks->pos;
        if (pos != 0 && pos->count != 0) {
            XAnim_Vec3Add(local_3c, (char *)pos->data + (uint32)pos->count * 12, local_3c);
            XAnim_Vec3Sub(local_3c, pos->data, local_3c);
        }
    }
    accum[0] = accum[0] + (local_1c[2] * local_1c[1] - local_1c[3] * local_1c[0]) * scale * kAccumScale;
    accum[1] = accum[1] + (local_1c[3] * local_1c[1] + local_1c[2] * local_1c[0]) * scale * kAccumScale;
    XAnim_Vec3Sub(local_3c, local_2c, local_4c);
    XAnim_ReflectPair(local_1c + which * 2, local_4c);
    accum[2] = accum[2] + scale;
    XAnim_Vec3MulAdd(accum + 3, scale, local_4c, accum + 3);
}

// 0x080bdf80  size=133  FUN_080bdf80  status=translated  naming=unknown
// naming: evaluates a keyed part at a time and applies its rotation, scalar and position deltas to a part state.
void XAnim_ApplyKeyedPartDelta(const xanim_keyed_part_s *part, float scale, float time,
                               xanim_part_state_s *state)
{
    float local_14[4];
    float local_2c[6];
    XAnim_EvalKeyedPart(part, time, local_14, local_2c);
    XAnim_Vec2MulAdd(state, scale * kTimeScale, local_14, state);
    state->scalar = state->scalar + scale;
    XAnim_Vec3MulAdd(&state->time_a, scale, local_2c, &state->time_a);
}

// 0x080be006  size=51  FUN_080be006  status=translated  naming=unknown
// naming: releases the resource handle of a blend slot and marks its cached index invalid (-1).
void XAnim_ReleaseSlotHandle(XAnimSlot *slot)
{
    if (slot->handle != 0) {
        ScrStr_Release((uint32)(uint16)slot->handle);
        slot->handle = 0;
    }
    slot->cached_index = -1;
}

// 0x080be03a  size=109  FUN_080be03a  status=translated  naming=unknown
// naming: releases blend slot `index` and pushes it onto the front of the free ring (sentinel slot 0 links the ring).
void XAnim_PushFreeSlot(int32 unused, int index)
{
    (void)unused;
    XAnim_ReleaseSlotHandle(&g_xanimSlotRing[index]);
    g_xanimSlotRing[index].list_prev = 0;
    g_xanimSlotRing[index].list_next = g_xanimSlotRing[0].list_next;
    g_xanimSlotRing[(uint16)g_xanimSlotRing[0].list_next].list_prev = (int16)index;
    g_xanimSlotRing[0].list_next = (int16)index;
}

// 0x080be0a8  size=309  FUN_080be0a8  status=translated  naming=unknown
// naming: weighted length of a node: a leaf returns its clip length; an interior node returns the rate-weighted mean of its active children (recursive).
long double XAnim_NodeWeightedLength(XAnimTree *tree, int node_index)
{
    XAnimNode *node = xanim_node_at(tree->nodes, node_index);
    float result;
    if (node->child_count == 0) {
        result = node->u.clip->length;
    } else {
        float weight_sum = 0.0f;
        float value_sum = 0.0f;
        int c;
        for (c = 0; c < (int)(uint32)node->child_count; c++) {
            uint16 slot_index = tree->slot_of_node[(uint16)node->u.group.first_child + c];
            if (slot_index != 0) {
                const XAnimSlot *slot = &g_xanimSlotRing[slot_index];
                float w = slot->weight;
                if (w != 0.0f) {
                    long double child = XAnim_NodeWeightedLength(tree, (int)(uint16)node->u.group.first_child + c);
                    if ((float)child != 0.0f) {
                        weight_sum = weight_sum + w;
                        value_sum = value_sum + (float)child * w * slot->rate;
                    }
                }
            }
        }
        float mean;
        if (weight_sum != 0.0f || weight_sum != weight_sum) {
            mean = value_sum / weight_sum;
        } else {
            mean = 0.0f;
        }
        result = mean;
    }
    return (long double)result;
}

// 0x080be1de  size=141  FUN_080be1de  status=translated  naming=unknown
// naming: index of the smallest key time that is at or after the given time (0 when none), as a ushort.
uint32 XAnim_FindKeyAtOrAfter(const XAnimNode *node, float time)
{
    const xanim_clip_s *clip = node->u.clip;
    const XAnimNotify *best = 0;
    float best_time = 2.0f;
    const XAnimNotify *key = clip->keys;
    int k;
    for (k = 0; k < (int)(uint32)clip->key_count; k++) {
        float kt = key->time;
        if (time <= kt && kt < best_time) {
            best = key;
            best_time = kt;
        }
        key++;
    }
    int32 diff = (int32)(uintptr_t)best - (int32)(uintptr_t)clip->keys;
    return (uint32)((diff >> 3) & 0xffff);
}

// 0x080be26c  size=308  FUN_080be26c  status=translated  naming=unknown
// naming: fraction of a playback interval reached by time t, given the cursor window and the key window (wrap-aware); returns 1.0 when the interval is complete.
long double XAnim_WindowFraction(const xanim_window_s *cur, const xanim_window_s *key, float t, float length)
{
    float result;
    if (key->last_time == 1.0f) {
        result = 1.0f;
    } else if (key->time < key->last_time) {
        if (t < key->time) {
            result = ((t - cur->last_time) + (float)((key->last_index - cur->last_index) + 1)) / length;
        } else if (key->last_time <= t) {
            result = ((t - cur->last_time) + (float)(key->last_index - cur->last_index)) / length;
        } else {
            result = 1.0f;
        }
    } else if (((key->time <= t) && (key->time != 1.0f)) || (t < key->last_time)) {
        result = 1.0f;
    } else {
        result = ((t - cur->last_time) + (float)(key->last_index - cur->last_index)) / length;
    }
    return (long double)result;
}


void XAnim_FireKeysBetween(XAnimTree *tree, XAnimSlot *slot, XAnimNode *node, float t);
void XAnim_NotifyKeyEvent(const XAnimTree *tree, uint32 handle, uint32 track);
void XAnim_QueueNodeKeys(XAnimSlot *slot, XAnimNode *node, float length);
void XAnim_PropagateCursor(XAnimTree *tree, int node_index, char flag, xanim_window_s *cursor, float length);

// 0x080be3a0  size=270  FUN_080be3a0  status=translated  naming=unknown
// naming: evaluates the window fraction of a node for a key: a node without a cached key index is recomputed with XAnim_UpdateSlotPhase first; a missing handle or node count yields 1.0.
long double XAnim_EvalNodeWindow(XAnimTree *tree, XAnimSlot *slot, XAnimNode *node,
                                 xanim_window_s *cur, xanim_window_s *key, float length)
{
    float result;
    if (tree->node_count == 0 || slot->handle == 0) {
        result = 1.0f;
    } else {
        if (node->child_count != 0) {
            if (slot->unused_00 == 0) {
                result = (float)XAnim_WindowFraction(cur, key, 1.0f, length);
                return (long double)result;
            }
            node = xanim_node_at(tree->nodes, (int)slot->unused_00);
        }
        xanim_clip_s *clip = node->u.clip;
        if (slot->cached_index < 0) {
            XAnim_UpdateSlotPhase(tree, slot, node);
            if (slot->cached_index < 0) {
                result = (float)XAnim_WindowFraction(cur, key, 1.0f, length);
                return (long double)result;
            }
        }
        result = (float)XAnim_WindowFraction(cur, key, clip->keys[slot->cached_index].time, length);
    }
    return (long double)result;
}

// 0x080be4ae  size=166  FUN_080be4ae  status=translated  naming=unknown
// naming: inserts (resource, count, key) into the pending list g_xanimPendingEntries kept sorted by key (insertion without a capacity check).
void XAnim_InsertPendingEntry(int32 unused, uint32 handle, float key, int32 count)
{
    (void)unused;
    int32 i = g_xanimPendingCount;
    while (--i >= 0) {
        if (g_xanimPendingEntries[i].key <= key) {
            break;
        }
        g_xanimPendingEntries[i + 1] = g_xanimPendingEntries[i];
    }
    g_xanimPendingEntries[i + 1].resource = (uint32)(uintptr_t)ScrStr_TextFromHandle(handle);
    g_xanimPendingEntries[i + 1].key = key;
    g_xanimPendingEntries[i + 1].count = count;
    g_xanimPendingCount = g_xanimPendingCount + 1;
}

// 0x080be554  size=1029  FUN_080be554  status=translated  naming=unknown
// naming: queues the keys of a node that fall inside the cursor window into the pending list, weighted by the window fraction; the branch depends on whether the node is a leaf and on the window ordering.
void XAnim_QueueNodeKeys(XAnimSlot *slot, XAnimNode *node, float length)
{
    xanim_window_s *cur = &slot->window;
    uint16 count = (uint16)slot->track_count; /* the original zero-extends (movzx) before passing it on */
    if (count == 0) {
        return;
    }
    if (cur->last_time == 1.0f) {
        float frac = (float)XAnim_WindowFraction(cur, cur, 1.0f, length);
        XAnim_InsertPendingEntry(0, g_xanimEndNotifyHandle, frac, count);
    } else if (node->child_count == 0) {
        xanim_clip_s *clip = node->u.clip;
        uint32 local_e = XAnim_FindKeyAtOrAfter(node, cur->last_time);
        XAnimNotify *key = clip->keys + local_e;
        if (cur->time < cur->last_time) {
            if (key->time < cur->time) {
                do {
                    float frac = (float)XAnim_WindowFraction(cur, cur, key->time, length);
                    XAnim_InsertPendingEntry(0, key->track_index, frac, count);
                    local_e = local_e + 1;
                    if (clip->key_count <= local_e) {
                        return;
                    }
                    key++;
                } while (key->time < cur->time);
            } else if (cur->last_time <= key->time) {
                do {
                    float frac = (float)XAnim_WindowFraction(cur, cur, key->time, length);
                    XAnim_InsertPendingEntry(0, key->track_index, frac, count);
                    key++;
                    local_e = local_e + 1;
                } while (local_e < clip->key_count);
                for (key = clip->keys; key->time < cur->time; key++) {
                    float frac = (float)XAnim_WindowFraction(cur, cur, key->time, length);
                    XAnim_InsertPendingEntry(0, key->track_index, frac, count);
                }
            }
        } else if (cur->time == 1.0f) {
            if (cur->last_time <= key->time) {
                do {
                    float frac = (float)XAnim_WindowFraction(cur, cur, key->time, length);
                    XAnim_InsertPendingEntry(0, key->track_index, frac, count);
                    key++;
                    local_e = local_e + 1;
                } while (local_e < clip->key_count);
            }
        } else if ((key->time < cur->time) && (cur->last_time <= key->time)) {
            do {
                float frac = (float)XAnim_WindowFraction(cur, cur, key->time, length);
                XAnim_InsertPendingEntry(0, key->track_index, frac, count);
                local_e = local_e + 1;
                if (clip->key_count <= local_e) {
                    return;
                }
                key++;
            } while (key->time < cur->time);
        }
    } else if (cur->time < cur->last_time || cur->time == 1.0f) {
        float frac = (float)XAnim_WindowFraction(cur, cur, 1.0f, length);
        XAnim_InsertPendingEntry(0, g_xanimEndNotifyHandle, frac, count);
    }
}

// 0x080be95a  size=428  FUN_080be95a  status=translated  naming=unknown
// naming: propagates a cursor update down a node's subtree: refreshes the slot cursor when it changed, fires keys, queues keys and recurses into children.
void XAnim_PropagateCursor(XAnimTree *tree, int node_index, char flag, xanim_window_s *cursor, float length)
{
    char local_5 = flag;
    uint16 slot_index = tree->slot_of_node[node_index];
    if (slot_index == 0) {
        return;
    }
    XAnimSlot *slot = &g_xanimSlotRing[slot_index];
    if (slot->weight == 0.0f) {
        return;
    }
    if (slot->target_weight == 0.0f) {
        local_5 = 0;
    }
    XAnimNode *node = xanim_node_at(tree->nodes, node_index);
    if (slot->window.last_time != cursor->last_time || slot->window.last_time != slot->window.last_time ||
        cursor->last_time != cursor->last_time || slot->window.last_index != cursor->last_index) {
        slot->window.time = cursor->last_time;
        slot->window.index = cursor->last_index;
        slot->window.last_time = cursor->last_time;
        slot->window.last_index = cursor->last_index;
        slot->cached_index = -1;
    }
    if (local_5 != 0) {
        XAnim_FireKeysBetween(tree, slot, node, cursor->time);
    }
    slot->window.time = cursor->time;
    slot->window.index = cursor->index;
    slot->cached_index = -1;
    if (local_5 != 0) {
        XAnim_QueueNodeKeys(slot, node, length);
    }
    uint16 children = node->child_count;
    int c;
    for (c = 0; c < (int)children; c++) {
        XAnim_PropagateCursor(tree, (int)node->u.group.first_child + c, local_5, cursor, length);
    }
}

// 0x080beb06  size=921  FUN_080beb06  status=translated  naming=unknown
// naming: advances a node's playback cursor by dt (scaled by the slot rate and the clip length for leaves, or by the weighted mean length for groups), wraps the key index, and propagates the new cursor to the children.
void XAnim_AdvanceNodeCursor(XAnimTree *tree, int node_index, float dt, char flag)
{
    char local_5 = flag;
    uint16 slot_index = tree->slot_of_node[node_index];
    if (slot_index == 0) {
        return;
    }
    XAnimSlot *slot = &g_xanimSlotRing[slot_index];
    if (slot->weight == 0.0f) {
        return;
    }
    if (slot->target_weight == 0.0f) {
        local_5 = 0;
    }
    XAnimNode *node = xanim_node_at(tree->nodes, node_index);
    uint16 child_count = node->child_count;
    if (child_count == 0) {
        dt = dt * slot->rate * node->u.clip->length;
        if (dt != 0.0f) {
            float t = slot->window.last_time + dt;
            int16 idx = slot->window.index;
            if (1.0f <= t) {
                if (node->u.clip->wrap_flag == 0) {
                    t = 1.0f;
                } else {
                    do {
                        t = t - 1.0f;
                        idx = idx + 1;
                    } while (1.0f <= t);
                }
            }
            if (slot->window.time - t <= (float)((int)idx - (int)slot->window.index)) {
                if (local_5 != 0) {
                    XAnim_FireKeysBetween(tree, slot, node, t);
                }
                slot->window.time = t;
                slot->window.index = idx;
                slot->cached_index = -1;
                if (local_5 != 0) {
                    XAnim_QueueNodeKeys(slot, node, dt);
                }
            }
        }
    } else if ((node->u.group.mode_flags & 3) == 0) {
        float rate_dt = dt * slot->rate;
        if (rate_dt != 0.0f) {
            int c;
            for (c = 0; c < (int)child_count; c++) {
                XAnim_AdvanceNodeCursor(tree, (int)node->u.group.first_child + c, rate_dt, local_5);
            }
        }
    } else {
        long double weighted = XAnim_NodeWeightedLength(tree, node_index);
        float step = (float)((long double)dt * weighted * (long double)slot->rate);
        if (step != 0.0f) {
            float t = slot->window.last_time + step;
            int16 idx = slot->window.last_index;
            if (1.0f <= t) {
                if ((node->u.group.mode_flags & 2) == 0) {
                    do {
                        t = t - 1.0f;
                        idx = idx + 1;
                    } while (1.0f <= t);
                } else {
                    t = 1.0f;
                }
            }
            if (slot->window.time - t <= (float)((int)idx - (int)slot->window.index)) {
                if (local_5 != 0) {
                    XAnim_FireKeysBetween(tree, slot, node, t);
                }
                slot->window.time = t;
                slot->window.index = idx;
                slot->cached_index = -1;
                if (local_5 != 0) {
                    XAnim_QueueNodeKeys(slot, node, step);
                }
                int c;
                for (c = 0; c < (int)child_count; c++) {
                    XAnim_PropagateCursor(tree, (int)node->u.group.first_child + c, local_5, &slot->window, step);
                }
            }
        }
    }
}

// 0x080beea0  size=73  FUN_080beea0  status=translated  naming=unknown
// naming: resets a blend slot record: clears the cursor and weights block (0x1c bytes from +0x0c), the handle and track count, and marks the cached index invalid.
void XAnim_ResetSlotRecord(XAnimSlot *slot)
{
    memset(&slot->window, 0, 0x1c);
    slot->handle = 0;
    slot->cached_index = -1;
    slot->unused_00 = 0;
    slot->track_count = 0;
}

// 0x080beeea  size=303  FUN_080beeea  status=translated  naming=unknown
// naming: synchronises an instance tree's slot indices with the static tree: releases slots of nodes that lost their weight, and copies the cursor, weight and rate of each active slot into the instance slot.
void XAnim_SyncInstanceSlots(XAnimTree *tree, XAnimTree *inst)
{
    int count = tree->nodes->node_count;
    int k;
    for (k = 0; k < count; k++) {
        uint16 src_index = tree->slot_of_node[k];
        if (src_index == 0) {
            uint16 old = inst->slot_of_node[k];
            if (old != 0) {
                XAnim_PushFreeSlot(0, (int)old);
                inst->slot_of_node[k] = 0;
            }
        } else {
            XAnimSlot *dst;
            if (inst->slot_of_node[k] == 0) {
                dst = (XAnimSlot *)XAnim_AllocSlot(inst, k);
                XAnim_ResetSlotRecord(dst);
            } else {
                dst = &g_xanimSlotRing[inst->slot_of_node[k]];
            }
            XAnimSlot *src = &g_xanimSlotRing[src_index];
            dst->window.time = src->window.last_time;
            dst->window.index = src->window.last_index;
            dst->target_weight = src->target_weight;
            dst->rate = src->rate;
            dst->blend_remaining = src->blend_remaining;
        }
    }
}

// 0x080bf01a  size=114  FUN_080bf01a  status=translated  naming=unknown
// naming: resets a slot's cursor (time, index, last time, last index) and marks its cached index invalid; returns 1 when anything changed.
uint32 XAnim_ResetCursorIfMoved(XAnimSlot *slot)
{
    if (slot->window.time == 0.0f && slot->window.index == 0) {
        return 0;
    }
    slot->window.time = 0.0f;
    slot->window.index = 0;
    slot->window.last_time = 0.0f;
    slot->window.last_index = 0;
    slot->cached_index = -1;
    return 1;
}

// 0x080bf08c  size=134  FUN_080bf08c  status=translated  naming=unknown
// naming: resets the cursor of a node's slot and recursively of its children.
void XAnim_ResetCursorRecursive(XAnimTree *tree, int node_index)
{
    uint16 slot_index = tree->slot_of_node[node_index];
    if (slot_index != 0) {
        XAnim_ResetCursorIfMoved(&g_xanimSlotRing[slot_index]);
        XAnimNode *node = xanim_node_at(tree->nodes, node_index);
        uint16 count = node->child_count;
        int c;
        for (c = 0; c < (int)count; c++) {
            XAnim_ResetCursorRecursive(tree, (int)node->u.group.first_child + c);
        }
    }
}

// 0x080bf112  size=737  FUN_080bf112  status=translated  naming=unknown
// naming: updates the blend weight of a node's slot (linear blend towards the target over the remaining blend time), recurses into children and reports whether the node is still active and whether two blends conflict.
void XAnim_UpdateBlendRecursive(XAnimTree *tree, int node_index, float *src, float dt, char allow_blend,
                                uint8 *active_out, uint8 *conflict_out)
{
    uint16 slot_index = tree->slot_of_node[node_index];
    if (slot_index == 0) {
        return;
    }
    XAnimSlot *slot = &g_xanimSlotRing[slot_index];
    float *cursor = &slot->window.time;
    uint8 local_2d = 0;
    if (allow_blend != 0 && (slot->weight != 0.0f || slot->weight != slot->weight)) {
        local_2d = 1;
    }
    uint8 local_25 = local_2d;
    if (allow_blend == 0 || slot->blend_remaining <= dt) {
        slot->weight = slot->target_weight;
        slot->blend_remaining = 0.0f;
    } else {
        slot->weight = slot->weight + ((slot->target_weight - slot->weight) * dt) / slot->blend_remaining;
        if (slot->weight < kBlendFloor) {
            slot->weight = slot->target_weight * kBlendScale;
        }
        slot->blend_remaining = slot->blend_remaining - dt;
    }
    uint8 local_2e = 0;
    if (local_2d != 0 || slot->target_weight != 0.0f || slot->target_weight != slot->target_weight) {
        local_2e = 1;
    }
    uint8 local_a = local_2e;
    XAnimNode *node = xanim_node_at(tree->nodes, node_index);
    uint16 child_count = node->child_count;
    uint8 local_26;
    if (child_count == 0) {
        local_26 = (node->u.clip->length != 0.0f);
    } else {
        local_26 = 0;
        if ((node->u.group.mode_flags & 4) != 0) {
            src = cursor;
        }
        int c;
        for (c = 0; c < (int)child_count; c++) {
            XAnim_UpdateBlendRecursive(tree, (int)node->u.group.first_child + c, src, dt, local_25,
                                       &local_a, &local_26);
        }
    }
    if (local_a == 0) {
        XAnim_PushFreeSlot(0, (int)slot_index);
        tree->slot_of_node[node_index] = 0;
    } else {
        if (local_25 == 0 || local_26 == 0) {
            if (child_count == 0 || (node->u.group.mode_flags & 4) == 0) {
                if (cursor[0] != src[0] || cursor[0] != cursor[0] || src[0] != src[0] ||
                    slot->window.index != *(int16 *)(src + 2)) {
                    cursor[0] = src[0];
                    slot->window.index = *(int16 *)(src + 2);
                    slot->cached_index = -1;
                }
            } else {
                if (XAnim_ResetCursorIfMoved(slot) != 0) {
                    int c;
                    for (c = 0; c < (int)child_count; c++) {
                        XAnim_ResetCursorRecursive(tree, (int)node->u.group.first_child + c);
                    }
                }
            }
        } else {
            *conflict_out = 1;
        }
        cursor[1] = cursor[0];
        slot->window.last_index = slot->window.index;
        *active_out = 1;
    }
}

// 0x080bf3f4  size=58  FUN_080bf3f4  status=translated  naming=unknown
// naming: records a key event for a track (clearing its pending state) and queues a tree-wide notification on the last node index.
void XAnim_NotifyKeyEvent(const XAnimTree *tree, uint32 handle, uint32 track)
{
    Scr_AddStringId(track);
    Scr_NotifyNum((int)tree->node_count - 1, 0, (int)handle, 1);
}

// 0x080bf42e  size=286  FUN_080bf42e  status=translated  naming=unknown
// naming: minimum window fraction over a node and its active children (recursive).
long double XAnim_EvalNodeWindowMin(XAnimTree *tree, XAnimSlot *slot, XAnimNode *node,
                                    xanim_window_s *cur, xanim_window_s *key, float length)
{
    float result = (float)XAnim_EvalNodeWindow(tree, slot, node, cur, key, length);
    int c;
    for (c = 0; c < (int)node->child_count; c++) {
        uint16 child_slot = tree->slot_of_node[(int)node->u.group.first_child + c];
        if (child_slot != 0) {
            XAnimSlot *cs = &g_xanimSlotRing[child_slot];
            if (cs->weight != 0.0f && cs->target_weight != 0.0f) {
                long double v = XAnim_EvalNodeWindowMin(tree, cs, xanim_node_at(tree->nodes, (int)node->u.group.first_child + c),
                                                        cur, key, length);
                if ((float)v < result) {
                    result = (float)v;
                }
            }
        }
    }
    return (long double)result;
}

// 0x080bf54c  size=904  FUN_080bf54c  status=translated  naming=unknown
// naming: minimum time-to-next-key fraction over a node and its weighted children (recursive); the leaf case computes the next window and evaluates it.
long double XAnim_EvalNodeWeightMin(XAnimTree *tree, int node_index, float dt)
{
    uint16 slot_index = tree->slot_of_node[node_index];
    float result;
    if (slot_index == 0) {
        return 1.0f;
    }
    XAnimSlot *slot = &g_xanimSlotRing[slot_index];
    if (slot->weight == 0.0f || slot->target_weight == 0.0f) {
        return 1.0f;
    }
    XAnimNode *node = xanim_node_at(tree->nodes, node_index);
    uint16 count = node->child_count;
    if (count == 0) {
        xanim_clip_s *clip = node->u.clip;
        dt = dt * slot->rate * clip->length;
        if (dt == 0.0f) {
            return 1.0f;
        }
        float t = slot->window.last_time + dt;
        int16 idx = slot->window.last_index;
        if (clip->wrap_flag == 0) {
            if (1.0f <= t) {
                t = 1.0f;
            }
        } else {
            for (; 1.0f <= t; t = t - 1.0f) {
                idx = idx + 1;
            }
        }
        if ((float)((int)idx - (int)slot->window.index) < slot->window.time - t) {
            return 1.0f;
        }
        xanim_window_s key;
        key.time = t;
        key.last_time = slot->window.time;
        key.index = idx;
        key.last_index = slot->window.index;
        result = (float)XAnim_EvalNodeWindow(tree, slot, node, &slot->window, &key, dt);
    } else if ((node->u.group.mode_flags & 3) == 0) {
        float rate_dt = dt * slot->rate;
        if (rate_dt == 0.0f) {
            return 1.0f;
        }
        float best = 1.0f;
        int c;
        for (c = 0; c < (int)count; c++) {
            float v = (float)XAnim_EvalNodeWeightMin(tree, (int)node->u.group.first_child + c, rate_dt);
            if (v < best) {
                best = v;
            }
        }
        result = best;
    } else {
        long double weighted = XAnim_NodeWeightedLength(tree, node_index);
        float step = (float)((long double)dt * weighted * (long double)slot->rate);
        if (step == 0.0f) {
            return 1.0f;
        }
        float t = slot->window.last_time + step;
        int16 idx = slot->window.last_index;
        if ((node->u.group.mode_flags & 2) == 0) {
            for (; 1.0f <= t; t = t - 1.0f) {
                idx = idx + 1;
            }
        } else if (1.0f <= t) {
            t = 1.0f;
        }
        if ((float)((int)idx - (int)slot->window.index) < slot->window.time - t) {
            return 1.0f;
        }
        xanim_window_s key;
        key.time = t;
        key.last_time = slot->window.time;
        key.index = idx;
        key.last_index = slot->window.index;
        result = (float)XAnim_EvalNodeWindowMin(tree, slot, node, &slot->window, &key, step);
    }
    return (long double)result;
}

// 0x080bf8d4  size=815  FUN_080bf8d4  status=translated  naming=unknown
// naming: fires (notifies) the keys of a node that the cursor passes between its time and the new time t; a completed cursor (time 1.0) fires the node's notify event.
void XAnim_FireKeysBetween(XAnimTree *tree, XAnimSlot *slot, XAnimNode *node, float t)
{
    if (tree->node_count == 0 || slot->handle == 0) {
        return;
    }
    if (slot->window.time == 1.0f) {
        Scr_AddStringId(g_xanimEndNotifyHandle);
        Scr_NotifyNum((int)tree->node_count - 1, 0, (int)(uint16)slot->handle, 1);
        return;
    }
    if (slot->cached_index < 0) {
        XAnim_UpdateSlotPhase(tree, slot, node);
        if (slot->cached_index < 0) {
            if (t < slot->window.time || t == 1.0f) {
                Scr_AddStringId(g_xanimEndNotifyHandle);
                Scr_NotifyNum((int)tree->node_count - 1, 0, (int)(uint16)slot->handle, 1);
            }
            return;
        }
    }
    if (node->child_count != 0) {
        node = xanim_node_at(tree->nodes, (int)slot->unused_00);
    }
    xanim_clip_s *clip = node->u.clip;
    int local_10 = (int)slot->cached_index;
    XAnimNotify *key = clip->keys + local_10;
    if (t < slot->window.time) {
        if (key->time < t) {
            do {
                XAnim_NotifyKeyEvent(tree, (uint32)(uint16)slot->handle, key->track_index);
                local_10 = local_10 + 1;
                if ((int)clip->key_count <= local_10) {
                    return;
                }
                key++;
            } while (key->time < t);
        } else if (slot->window.time <= key->time) {
            do {
                XAnim_NotifyKeyEvent(tree, (uint32)(uint16)slot->handle, key->track_index);
                key++;
                local_10 = local_10 + 1;
            } while (local_10 < (int)clip->key_count);
            for (key = clip->keys; key->time < t; key++) {
                XAnim_NotifyKeyEvent(tree, (uint32)(uint16)slot->handle, key->track_index);
            }
        }
    } else if (t == 1.0f) {
        if (slot->window.time <= key->time) {
            do {
                XAnim_NotifyKeyEvent(tree, (uint32)(uint16)slot->handle, key->track_index);
                key++;
                local_10 = local_10 + 1;
            } while (local_10 < (int)clip->key_count);
        }
    } else if ((key->time < t) && (slot->window.time <= key->time)) {
        do {
            XAnim_NotifyKeyEvent(tree, (uint32)(uint16)slot->handle, key->track_index);
            local_10 = local_10 + 1;
            if ((int)clip->key_count <= local_10) {
                return;
            }
            key++;
        } while (key->time < t);
    }
}

// 0x080bfc04  size=2004  FUN_080bfc04  status=translated  naming=unknown
// naming: evaluates the parts of one node into an output buffer: children are blended pairwise by weight, a leaf reads its clip through the cached part map, and the masks track which parts were written.
void XAnim_EvalNodeParts(xanim_anim_state_s *anim, int node_index, float time, xanim_part_state_s *out,
                         char root, char no_blend, xanim_calc_buffer_s *buf, int used)
{
    XAnimTree *tree = anim->tree;
    XAnimNode *node = xanim_node_at(tree->nodes, node_index);
    uint32 count = node->child_count;
    if (count != 0) {
        int first = (int)node->u.group.first_child;
        int c = 0;
        uint16 s1 = 0;
        for (;;) {
            if ((int)count <= c) {
                if (root == 0) {
                    return;
                }
                XAnim_ResetUnmaskedParts(anim, out, buf);
                return;
            }
            s1 = tree->slot_of_node[first + c];
            if (s1 != 0 && g_xanimSlotRing[s1].weight != 0.0f) {
                break;
            }
            c++;
        }
        int i11 = c;
        int j;
        uint16 s2;
        float w2;
        for (;;) {
            int i5 = i11;
            j = i5 + 1;
            if ((int)count <= j) {
                XAnim_EvalNodeParts(anim, first + c, time, out, root, no_blend, buf, used);
                return;
            }
            s2 = tree->slot_of_node[first + j];
            i11 = j;
            if (s2 != 0) {
                w2 = g_xanimSlotRing[s2].weight;
                if (w2 != 0.0f) {
                    break;
                }
            }
        }
        float w1 = g_xanimSlotRing[s1].weight;
        xanim_part_state_s *local_2c;
        int used2 = used;
        if (root == 0) {
            local_2c = (xanim_part_state_s *)((char *)buf + used * 0x20);
            used2 = used + (int)anim->part_count;
            if (used2 > 0x200) {
                Com_Printf("MAX_CALC_ANIM_BUFFER exceeded\n");
                return;
            }
        } else {
            local_2c = out;
        }
        XAnim_EvalNodeParts(anim, first + c, w1, local_2c, 1, 1, buf, used2);
        XAnim_EvalNodeParts(anim, first + j, w2, local_2c, 0, 1, buf, used2);
        int k;
        for (k = j + 1; k < (int)count; k++) {
            uint16 s3 = tree->slot_of_node[first + k];
            if (s3 != 0 && g_xanimSlotRing[s3].weight != 0.0f) {
                XAnim_EvalNodeParts(anim, first + k, g_xanimSlotRing[s3].weight, local_2c, 0, 1, buf, used2);
            }
        }
        int part;
        int part_count = (int)anim->part_count;
        if (no_blend == 0) {
            for (part = 0; part < part_count; part++) {
                if (((int)buf->skip_mask[part >> 5] >> (part & 0x1f) & 1) == 0 &&
                    (out->time_b != 0.0f || out->time_b != out->time_b)) {
                    float inv = 1.0f / out->time_b;
                    XAnim_Vec4Scale(out, inv, out);
                    XAnim_Vec3Scale(out->pos, inv, out->pos);
                }
                out++;
            }
        } else if (root == 0) {
            for (part = 0; part < part_count; part++) {
                if (((int)buf->skip_mask[part >> 5] >> (part & 0x1f) & 1) == 0) {
                    float fv = XAnim_Vec4LenSq(local_2c);
                    if (fv != 0.0f || fv != fv) {
                        XAnim_Vec4MulAdd(out, (float)((long double)XAnim_RSqrt(fv) * (long double)time), local_2c, out);
                    }
                    if (local_2c->time_b != 0.0f || local_2c->time_b != local_2c->time_b) {
                        XAnim_Vec3MulAdd(out->pos, time / local_2c->time_b, local_2c->pos, out->pos);
                        out->time_b = out->time_b + time;
                    }
                }
                out++;
                local_2c++;
            }
        } else {
            for (part = 0; part < part_count; part++) {
                if (((int)buf->skip_mask[part >> 5] >> (part & 0x1f) & 1) == 0) {
                    float fv = XAnim_Vec4LenSq(out);
                    if (fv != 0.0f || fv != fv) {
                        XAnim_Vec4Scale(out, (float)(XAnim_RSqrt(fv) * (long double)time), out);
                    }
                    if (out->time_b != 0.0f || out->time_b != out->time_b) {
                        XAnim_Vec3Scale(out->pos, time / out->time_b, out->pos);
                        out->time_b = time;
                    }
                }
                out++;
            }
        }
        return;
    }
    if (root != 0) {
        XAnim_ResetUnmaskedParts(anim, out, buf);
    }
    uint16 *handles = anim->node_handles;
    uint8 *gen = (uint8 *)handles + (uintptr_t)(uint32)tree->nodes->node_count * 2;
    xanim_clip_s *clip = node->u.clip;
    if (handles[node_index] == 0) {
        gen[node_index + 1] = gen[0];
        handles[node_index] = (uint16)XAnim_BuildPartMap(node, anim->link_slots, (int)anim->link_count);
    } else if (gen[node_index + 1] != gen[0]) {
        gen[node_index + 1] = gen[0];
        ScrStr_DecRefOrFree((uint32)handles[node_index], (int)clip->part_count + 0x10);
        handles[node_index] = (uint16)XAnim_BuildPartMap(node, anim->link_slots, (int)anim->link_count);
    }
    const uint32 *res = (const uint32 *)ScrStr_TextFromHandle((uint32)handles[node_index]);
    int i;
    for (i = 0; i < 4; i++) {
        buf->written_mask[i] = (~buf->skip_mask[i] & res[i]) | buf->written_mask[i];
    }
    float fv = g_xanimSlotRing[tree->slot_of_node[node_index]].window.time;
    if (fv == 1.0f || *(const uint16 *)clip->head_00 == 0) {
        XAnim_ApplyPartTracks(clip, (const uint8 *)res + 0x10, time, out, buf->skip_mask);
        return;
    }
    if (*(const uint16 *)clip->head_00 < 0x100) {
        XAnim_BlendBoneChannels((uint16 *)clip, (const uint8 *)res + 0x10, fv, time, (uint8 *)out, buf->skip_mask);
        return;
    }
    XAnim_BlendBoneChannelsB((uint16 *)clip, (const uint8 *)res + 0x10, fv, time, (uint8 *)out, buf->skip_mask);
}
