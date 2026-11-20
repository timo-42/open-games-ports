/*
 * Types, globals and out-of-chunk prototypes for chunk 033 (0x080c03d8..0x080c3b90).
 *
 * Subsystem: animation tree blending (XAnim-style). The binary builds a per-object tree of
 * animation nodes, keeps a pool of blend slots at g_xanimSlotRing (element 0 is the free-list
 * sentinel) and accumulates per-bone pose samples.
 *
 * Layout notes (observed from the disassembly, objdump of 0x80c26a6 / 0x80c0d38):
 *   - Node records are addressed as table + id * 8 with fields at +0x0c..+0x13 (scaled index
 *     instructions "[edx+eax*8+0xc]"). XAnimNode models one such view. The +0x10 tagged pointer
 *     and the +0x12 halfword overlap in the binary's access pattern; the union reproduces the
 *     exact byte semantics. See the open questions in the port notes.
 *   - Slot records (0x28 bytes) are indexed as g_xanimSlotRing[index]; several functions reach them
 *     through short* or ushort* and therefore use halfword offsets that map to XAnimSlot fields.
 */
#ifndef COD2_CHUNK_033_TYPES_H
#define COD2_CHUNK_033_TYPES_H

#include "cod2_types.h"

struct XAnimParts;
struct XAnimSlot;
struct XAnimNode;
struct XAnimNodeTable;
struct XAnimTree;
struct XAnimBinding;
struct xanim_track_s;
struct XAnimPoseBone;
struct XAnimChannel;
struct XAnimBlendAccum;

/* Clip/animation info record, pointed to by XAnimNode::clip (low 2 bits are tag flags). */
struct XAnimParts {
    uint16 numFrames;          /* 0x00 frame count; numFrames / frameRate is the duration */
    uint8 flags02;             /* 0x02 loop mode compared by XAnim_SetNodeLoopMode */
    uint8 hasData;             /* 0x03 non-zero when the clip carries animation data */
    float frameRate;           /* 0x04 frames per second */
    float length;              /* 0x08 length in the same units as blend times */
    uint8 partCount;           /* 0x0c entries in keyTable */
    uint8 pad0d[0x0f];         /* 0x0d (0x0e: int16 read by XAnim_FreeAllSlots) */
    uint16 *keyTable;          /* 0x1c array of 8-byte entries, first halfword is the key */
    uint8 pad20[0x04];         /* 0x20 */
    const char *name;          /* 0x24 clip name (printed by XAnim_NodeDisplayName) */
    uint8 flag28;              /* 0x28 read by XAnim_ClipFlag28 */
};
COD2_ASSERT_OFFSET(XAnimParts, numFrames, 0x00);
COD2_ASSERT_OFFSET(XAnimParts, flags02, 0x02);
COD2_ASSERT_OFFSET(XAnimParts, hasData, 0x03);
COD2_ASSERT_OFFSET(XAnimParts, frameRate, 0x04);
COD2_ASSERT_OFFSET(XAnimParts, length, 0x08);
COD2_ASSERT_OFFSET(XAnimParts, partCount, 0x0c);
COD2_ASSERT_OFFSET(XAnimParts, keyTable, 0x1c);
COD2_ASSERT_OFFSET(XAnimParts, name, 0x24);
COD2_ASSERT_OFFSET(XAnimParts, flag28, 0x28);

/* Node record view: node id lives at table + id * 8; fields at +0x0c..+0x13. */
struct XAnimNode {
    uint8 header[0x0c];        /* 0x00 shared with the table header / previous record */
    uint16 childCount;         /* 0x0c number of children */
    uint16 parentIndex;        /* 0x0e parent node id */
    union {
        XAnimParts *clip;       /* 0x10 tagged pointer; low 2 bits are flags */
        struct {
            uint16 clipFlagsLo;    /* 0x10 low halfword of the tagged pointer */
            uint16 firstChild;     /* 0x12 id of the first child (children are consecutive) */
        };
    };
};
COD2_ASSERT_OFFSET(XAnimNode, childCount, 0x0c);
COD2_ASSERT_OFFSET(XAnimNode, parentIndex, 0x0e);
COD2_ASSERT_OFFSET(XAnimNode, firstChild, 0x12);

/* Node table referenced by XAnimTree::table. Node i is at (char *)table + i * 8. */
struct XAnimNodeTable {
    const char *name;          /* 0x00 tree name, printed in loop-mode diagnostics */
    int32 nodeCount;           /* 0x04 number of nodes (read by XAnim_FreeAllSlots) */
    const char **nodeNames;    /* 0x08 optional per-node names (NULL uses the clip name) */
};
COD2_ASSERT_OFFSET(XAnimNodeTable, nodeCount, 0x04);
COD2_ASSERT_OFFSET(XAnimNodeTable, nodeNames, 0x08);

/* Tree: word 0 is the node table; a uint16 slot index per node follows at +0x08. A second
 * uint16 array of handles follows the slot array (nodeCount entries). */
struct XAnimTree {
    XAnimNodeTable *table;     /* 0x00 */
    uint8 unk04[2];            /* 0x04 */
    uint8 flag06;              /* 0x06 selects the sampler used by a leaf (0: time-based) */
    uint8 flag07;              /* 0x07 selects old (0) or current weight for blends */
    uint16 slotOfNode[2];      /* 0x08 slot index per node (variable length) */
};
COD2_ASSERT_OFFSET(XAnimTree, table, 0x00);
COD2_ASSERT_OFFSET(XAnimTree, flag06, 0x06);
COD2_ASSERT_OFFSET(XAnimTree, flag07, 0x07);
COD2_ASSERT_OFFSET(XAnimTree, slotOfNode, 0x08);

/* One blend slot (0x28 bytes). Element 0 of g_xanimSlotRing is the free-list sentinel. */
struct XAnimSlot {
    uint16 childSlot;          /* 0x00 slot of the dominant child (XAnim_MaxWeightChild result) */
    uint16 unk02;              /* 0x02 phase value, set to 0xffff on reset */
    int16 nameIndex;           /* 0x04 animation name index, 0 = none */
    int16 flags06;             /* 0x06 value passed as the last argument of the slot setter */
    uint16 unk08;              /* 0x08 cleared when a slot is taken from the free list */
    uint16 nextFree;           /* 0x0a free-list link (sentinel element 0 holds the head) */
    float curTime;             /* 0x0c current blend time */
    float prevTime;            /* 0x10 previous blend time */
    uint16 cycleCount;         /* 0x14 completed cycle count */
    uint16 cycleLimit;         /* 0x16 cycle count threshold */
    float minRequest;          /* 0x18 smallest requested blend time while weight is zero */
    float weight;              /* 0x1c current weight */
    float oldWeight;           /* 0x20 weight at the start of the blend */
    float value24;             /* 0x24 raw word stored by the slot setters */
};
COD2_ASSERT_SIZE(XAnimSlot, 0x28);
COD2_ASSERT_OFFSET(XAnimSlot, childSlot, 0x00);
COD2_ASSERT_OFFSET(XAnimSlot, nameIndex, 0x04);
COD2_ASSERT_OFFSET(XAnimSlot, flags06, 0x06);
COD2_ASSERT_OFFSET(XAnimSlot, nextFree, 0x0a);
COD2_ASSERT_OFFSET(XAnimSlot, curTime, 0x0c);
COD2_ASSERT_OFFSET(XAnimSlot, prevTime, 0x10);
COD2_ASSERT_OFFSET(XAnimSlot, cycleCount, 0x14);
COD2_ASSERT_OFFSET(XAnimSlot, cycleLimit, 0x16);
COD2_ASSERT_OFFSET(XAnimSlot, minRequest, 0x18);
COD2_ASSERT_OFFSET(XAnimSlot, weight, 0x1c);
COD2_ASSERT_OFFSET(XAnimSlot, oldWeight, 0x20);
COD2_ASSERT_OFFSET(XAnimSlot, value24, 0x24);

/* Per-bone pose sample (0x20 bytes). */
struct XAnimPoseBone {
    float rot[4];              /* 0x00 quaternion (rot[3] = w, defaults to 1.0) */
    float pos[3];              /* 0x10 translation */
    float timer;               /* 0x1c accumulated time */
};
COD2_ASSERT_SIZE(XAnimPoseBone, 0x20);

/* Part record (short* in the binary): count @0, leadCount @2, data pointer @4, key area @8. */
struct xanim_track_s {
    int16 count;               /* 0x00 number of keys */
    int16 leadCount;           /* 0x02 keys consumed before the blend loop */
    uint8 *dataPtr;            /* 0x04 key data (4/8/12-byte entries by channel type) */
    uint8 keyArea[0x0c];       /* 0x08 key index area / key pointer */
};
COD2_ASSERT_OFFSET(xanim_track_s, dataPtr, 0x04);

/* Channel entry: 8 bytes, pointers to the position and rotation part records. */
struct XAnimChannel {
    xanim_track_s *posPart;     /* 0x00 */
    xanim_track_s *rotPart;     /* 0x04 */
};
COD2_ASSERT_SIZE(XAnimChannel, 8);

/* Binding: word 0 is the tree, word 1 the pose buffer, byte 0x18 the part count. */
struct XAnimBinding {
    XAnimTree *tree;           /* 0x00 */
    uint8 *pose;               /* 0x04 pose buffer; +0x30 holds XAnimPoseBone[] */
    uint8 pad08[0x10];
    uint8 partCountByte;       /* 0x18 number of part pointers that follow */
    uint8 pad19[0x03];
    xanim_track_s *parts[1];    /* 0x1c array of part records (partCountByte entries) */
};
COD2_ASSERT_OFFSET(XAnimBinding, tree, 0x00);
COD2_ASSERT_OFFSET(XAnimBinding, partCountByte, 0x18);
COD2_ASSERT_OFFSET(XAnimBinding, parts, 0x1c);

/* Blend accumulator filled by XAnim_BlendNode (0x18 bytes). */
struct XAnimBlendAccum {
    float xy[2];               /* 0x00 2-component accumulator */
    float weight;              /* 0x08 summed weight */
    float xyz[3];              /* 0x0c 3-component accumulator */
};
COD2_ASSERT_SIZE(XAnimBlendAccum, 0x18);

/* ---- call-compatibility shims over the shared prototypes (cod2_prototypes.h) ---- */
#define XAnim_DequantPair_Cast(a) ((const short *)(a))

#endif
