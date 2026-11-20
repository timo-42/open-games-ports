/*
 * Chunk 032 (cod2_lnxded 1.3, 0x080bc58a..0x080c03d8) types and out-of-chunk prototypes.
 *
 * Subsystem: XAnim node tables, clip/track sampling, blend-slot pool and part-state
 * accumulation (IW engine animation lineage). Layouts below are recovered from
 * consistent offsets in the disassembly; see chunk_032.cpp provenance for evidence.
 * Out-of-chunk callees are declared with placeholder names fn_08xxxxxx for the merge queue.
 */
#ifndef COD2_CHUNK_032_TYPES_H
#define COD2_CHUNK_032_TYPES_H

struct XAnimParts;
struct XAnimNodeTable;
struct XAnimNode;
struct xanim_clip_s;
struct xanim_track_s;
struct xanim_part_track_s;
struct xanim_part_state_s;
struct xanim_calc_buffer_s;
struct xanim_frame_key_s;
struct xanim_keyed_part_s;
struct xanim_anim_state_s;
struct XAnimTree;

/* Playback cursor: current time and key index, plus the previous (last) time and index. */
struct xanim_window_s {
    float time;                  /* +0x00 */
    float last_time;             /* +0x04 */
    int16 index;                 /* +0x08 */
    int16 last_index;            /* +0x0a */
};
COD2_ASSERT_SIZE(xanim_window_s, 0x0c);
COD2_ASSERT_OFFSET(xanim_window_s, last_index, 0x0a);

/* Blend slot, g_xanimSlotRing[0x1000], 40 bytes. Slot 0 is the list sentinel. */
struct XAnimSlot {
    uint16 unused_00;            /* +0x00 not referenced in this chunk */
    int16 cached_index;          /* +0x02 cached key index, -1 = must be recomputed */
    int16 handle;                /* +0x04 resource handle released by slot release */
    int16 track_count;           /* +0x06 count passed to the pending-entry insert */
    int16 list_prev;             /* +0x08 circular list, 12-bit wrap */
    int16 list_next;             /* +0x0a */
    xanim_window_s window;       /* +0x0c playback cursor */
    float blend_remaining;       /* +0x18 */
    float target_weight;         /* +0x1c */
    float weight;                /* +0x20 */
    float rate;                  /* +0x24 */
};
COD2_ASSERT_SIZE(XAnimSlot, 0x28);
COD2_ASSERT_OFFSET(XAnimSlot, window, 0x0c);
COD2_ASSERT_OFFSET(XAnimSlot, weight, 0x20);

/* Tree node, 8 bytes. Node k of a node table lives at table + 0x0c + 8k. */
struct XAnimNode {
    uint16 child_count;          /* +0x00 0 = leaf */
    int16 aux_index;             /* +0x02 group nodes: owner index written onto children;
                                    otherwise cached key index (-1 = not computed) */
    union {
        xanim_clip_s *clip;      /* +0x04 leaf payload */
        void *leaf_asset;        /* +0x04 leaf payload as loaded asset (SetLeafNode) */
        xanim_track_s *track;    /* +0x04 track pointer used by track holders */
        struct {
            uint16 mode_flags;   /* +0x04 bit 0x2, 0x4 ... blend mode */
            uint16 first_child;  /* +0x06 index of first child node */
        } group;
    } u;
};
COD2_ASSERT_SIZE(XAnimNode, 8);
COD2_ASSERT_OFFSET(XAnimNode, aux_index, 0x02);

/* Node table header, 0x0c bytes, followed by node_count XAnimNode records. */
struct XAnimNodeTable {
    char *debug_name;            /* +0x00 only when debug names are enabled */
    int32 node_count;            /* +0x04 */
    char **node_names;           /* +0x08 only when debug names are enabled */
};
COD2_ASSERT_SIZE(XAnimNodeTable, 0x0c);
COD2_ASSERT_OFFSET(XAnimNodeTable, node_count, 0x04);

/* Tree view: T[0] is the node table; ushort slot index per node at +8. */
struct XAnimTree {
    XAnimNodeTable *nodes;   /* +0x00 */
    uint16 node_count;           /* +0x04 */
    uint16 pad_06;               /* +0x06 */
    uint16 slot_of_node[1];      /* +0x08 blend-slot index per node, 0 = none */
};
COD2_ASSERT_OFFSET(XAnimTree, node_count, 0x04);
COD2_ASSERT_OFFSET(XAnimTree, slot_of_node, 0x08);

/* Sampled track, 8-byte header. count == 0 keeps the value inline at +4. */
struct xanim_track_s {
    uint16 count;                /* +0x00 */
    uint16 pad_02;               /* +0x02 */
    void *data;                  /* +0x04 sample block; value block follows count entries */
    uint8 key_table[1];          /* +0x08 byte keys (count < 0x100) or uint16 keys */
};
COD2_ASSERT_OFFSET(xanim_track_s, data, 0x04);
COD2_ASSERT_OFFSET(xanim_track_s, key_table, 0x08);

/* Per-part pair of tracks (8 bytes). */
struct xanim_part_track_s {
    xanim_track_s *pos;          /* +0x00 stride-12 (three floats per entry) */
    xanim_track_s *rot;          /* +0x04 stride-8 (rotation) or stride-4 (scalar) */
};
COD2_ASSERT_SIZE(xanim_part_track_s, 8);

/* Keyframe (8 bytes). */
struct XAnimNotify {
    uint16 track_index;          /* +0x00 node/track index */
    uint16 pad_02;
    float time;                  /* +0x04 */
};
COD2_ASSERT_SIZE(XAnimNotify, 8);

/* Clip payload of a leaf node. */
struct xanim_clip_s {
    uint8 head_00[2];            /* +0x00 16-bit sub count (interior-style use) */
    uint8 wrap_flag;             /* +0x02 */
    uint8 pad_03[5];
    float length;                /* +0x08 */
    uint8 key_count;             /* +0x0c */
    uint8 pad_0d;
    int16 part_count;            /* +0x0e */
    uint16 *part_ids;            /* +0x10 part id per node entry */
    uint8 *part_bitset;          /* +0x14 one bit per part */
    xanim_part_track_s *part_tracks; /* +0x18 */
    XAnimNotify *keys;           /* +0x1c */
};
COD2_ASSERT_SIZE(xanim_clip_s, 0x20);
COD2_ASSERT_OFFSET(xanim_clip_s, part_count, 0x0e);
COD2_ASSERT_OFFSET(xanim_clip_s, keys, 0x1c);

/* Evaluated per-part state, 32 bytes. */
struct xanim_part_state_s {
    uint8 rot_raw[8];            /* +0x00 rotation written by the stride-8 track */
    float scalar;                /* +0x08 written by the stride-4 track */
    float time_a;                /* +0x0c accumulated time (no rotation track) */
    float pos[3];                /* +0x10 */
    float time_b;                /* +0x1c accumulated time */
};
COD2_ASSERT_SIZE(xanim_part_state_s, 0x20);
COD2_ASSERT_OFFSET(xanim_part_state_s, pos, 0x10);

/* Scratch buffer for one evaluation pass. */
struct xanim_calc_buffer_s {
    xanim_part_state_s parts[0x200];   /* +0x0000 */
    uint32 written_mask[4];            /* +0x4000 */
    uint32 skip_mask[4];               /* +0x4010 */
};
COD2_ASSERT_SIZE(xanim_calc_buffer_s, 0x4020);
COD2_ASSERT_OFFSET(xanim_calc_buffer_s, skip_mask, 0x4010);

/* Frame key produced from (value, scale): value, scale, scale*value, floor(scale*value). */
struct xanim_frame_key_s {
    float value;                 /* +0x00 */
    int32 scale;                 /* +0x04 */
    float scaled;                /* +0x08 */
    int32 frame;                 /* +0x0c */
};
COD2_ASSERT_SIZE(xanim_frame_key_s, 0x10);

/* Keyed part: count at +0, wrap flag at +2, part track pair pointer at +0x20. */
struct xanim_keyed_part_s {
    uint16 key_count;            /* +0x00 */
    uint8 wrap_flag;             /* +0x02 */
    uint8 pad_03[0x1d];
    xanim_part_track_s *tracks;  /* +0x20 */
};
COD2_ASSERT_OFFSET(xanim_keyed_part_s, tracks, 0x20);

/* Handle entry inside an asset table (8 bytes). */
struct xanim_handle_entry_s {
    uint16 handle;               /* +0x00 */
    uint16 pad[3];
};
COD2_ASSERT_SIZE(xanim_handle_entry_s, 8);

/* Loaded xanim asset (handle table), 0x2c bytes. */
struct XAnimParts {
    uint32 raw_00[3];            /* +0x00 header words, copied verbatim on clone */
    uint8 entry_count;           /* +0x0c */
    uint8 pad_0d;
    int16 handle_count;          /* +0x0e */
    uint16 *handles;             /* +0x10 */
    uint32 pad_14[2];            /* +0x14 */
    xanim_handle_entry_s *entries; /* +0x1c */
    uint32 pad_20;               /* +0x20 */
    uint32 handle;               /* +0x24 loaded handle */
    uint8 is_fallback;           /* +0x28 1 = built from the default xanim */
    uint8 pad_29[3];
};
COD2_ASSERT_SIZE(XAnimParts, 0x2c);
COD2_ASSERT_OFFSET(XAnimParts, handle, 0x24);
COD2_ASSERT_OFFSET(XAnimParts, is_fallback, 0x28);

/* Anim state object (wrapper around a tree). */
struct xanim_anim_state_s {
    XAnimTree *tree;          /* +0x00 */
    uint8 pad_04[8];
    uint16 *node_handles;        /* +0x0c per-node handle, byte generations follow */
    uint8 pad_10[8];
    uint8 link_count;            /* +0x18 */
    uint8 part_count;            /* +0x19 */
    uint8 pad_1a[2];
    void *link_slots[1];         /* +0x1c array of link pointers */
};
COD2_ASSERT_OFFSET(xanim_anim_state_s, node_handles, 0x0c);
COD2_ASSERT_OFFSET(xanim_anim_state_s, part_count, 0x19);
COD2_ASSERT_OFFSET(xanim_anim_state_s, link_slots, 0x1c);

/* Pending sorted entry (12 bytes) in g_xanimPendingEntries. */
struct xanim_pending_s {
    uint32 resource;             /* +0x00 */
    int32 count;                 /* +0x04 */
    float key;                   /* +0x08 */
};
COD2_ASSERT_SIZE(xanim_pending_s, 12);

/* Global configuration record pointed to by com_developer (only +8 is read here). */
struct g_081a2174_s {
    uint8 pad_00[8];
    int32 value_08;
};
COD2_ASSERT_OFFSET(g_081a2174_s, value_08, 0x08);

/* Globals (names are Ghidra-derived addresses; merge lane renames). */

/* ---- call-compatibility shims over the shared prototypes (cod2_prototypes.h) ----
 * The prototypes were generated from other chunks' decompiler signatures; this chunk's call sites
 * were written against untyped (void *) views. Each macro keeps the original argument slots and only
 * adds the pointer/width casts. Function-like macros do not re-expand themselves. */
#define XAnim_LoadParts(n, a) XAnim_LoadParts((n), (void *)(a))
#define Hunk_FindUserEntryByName(t, n) ((void *)Hunk_FindUserEntryByName((t), (n)))
#define Hunk_AddUserEntry(t, n, a, al) ((uint32)(unsigned long)Hunk_AddUserEntry((unsigned char)(t), (n), (int)(a), (void *)(al)))
#define Hunk_IsInHeap(t) Hunk_IsInHeap((unsigned int)(t))
#define Hunk_AddAnonymousUserEntry(t, tbl, al) Hunk_AddAnonymousUserEntry((unsigned char)(t), (int)(tbl), (void *)(al))
#define XAnim_ResetTree(b) XAnim_ResetTree((XAnimTree *)(b))
#define ScrStr_Intern(d, z, n, k) ScrStr_Intern((const char *)(d), (z), (n), (k))
#define Short2_ToFloat2(a, b) Short2_ToFloat2((const short *)(a), (float *)(b))
#define XAnim_UpdateSlotPhase(t, s, n) XAnim_UpdateSlotPhase((t), (s), (const short *)(n))
#define XAnim_MulAddShort2(a, s, b, o) XAnim_MulAddShort2((const float *)(a), (s), (const short *)(b), (float *)(o))
#define XAnim_MulAddShort4(a, s, b, o) XAnim_MulAddShort4((const float *)(a), (s), (const short *)(b), (float *)(o))
#define XAnim_Vec3MulAdd(a, s, b, o) XAnim_Vec3MulAdd((const float *)(a), (s), (const float *)(b), (float *)(o))
#define XAnim_Vec4MulAdd(a, s, b, o) XAnim_Vec4MulAdd((const float *)(a), (s), (const float *)(b), (float *)(o))
#define XAnim_Vec2MulAdd(a, s, b, o) XAnim_Vec2MulAdd((const float *)(a), (s), (const float *)(b), (float *)(o))
#define XAnim_Vec3Copy(a, b) XAnim_Vec3Copy((const float *)(a), (float *)(b))
#define XAnim_Vec3Zero(a) XAnim_Vec3Zero((float *)(a))
#define XAnim_Vec4Zero(a) XAnim_Vec4Zero((float *)(a))
#define XAnim_Vec3Add(a, b, o) XAnim_Vec3Add((const float *)(a), (const float *)(b), (float *)(o))
#define XAnim_Vec3Sub(a, b, o) XAnim_Vec3Sub((const float *)(a), (const float *)(b), (float *)(o))
#define XAnim_Vec3Lerp(a, b, t, o) XAnim_Vec3Lerp((const float *)(a), (const float *)(b), (t), (float *)(o))
#define XAnim_Vec3Scale(a, s, o) XAnim_Vec3Scale((const float *)(a), (s), (float *)(o))
#define XAnim_Vec4Scale(a, s, o) XAnim_Vec4Scale((const float *)(a), (s), (float *)(o))
#define XAnim_Vec4LenSq(a) XAnim_Vec4LenSq((const float *)(a))
#define XAnim_DequantPair(a, b, f, o) XAnim_DequantPair((const short *)(a), (const short *)(b), (f), (float *)(o))

#endif
