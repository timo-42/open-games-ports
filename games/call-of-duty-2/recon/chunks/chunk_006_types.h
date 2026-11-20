/*
 * chunk 006 types, globals and out-of-chunk prototypes
 * Address range 0x0805ef22 .. 0x08061ccc of cod2_lnxded 1.3.
 * Contents: spatial-tree (node/entry) traces, float/vector helpers, Cbuf_*, Cmd_*,
 * Com_Printf/Com_Error/Com_Quit, command line parsing, Info_Print, pushed-event queue and
 * Com_EventLoop.
 */
#ifndef COD2_CHUNK_006_TYPES_H
#define COD2_CHUNK_006_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include <setjmp.h>
#include <time.h>
#include <strings.h>

/* ------------------------------------------------------------------ types */

typedef void (*xcommand_t)(void);
typedef void (*rd_flush_t)(char *buffer);

/* Command text buffer, stride 0xc, array at 0x0818f084, storage at 0x0818f100. */
struct cmd_text_s {
    byte *data;      /* 0x00 */
    int maxsize;     /* 0x04 */
    int cursize;     /* 0x08 */
};
COD2_ASSERT_SIZE(cmd_text_s, 0xc);
COD2_ASSERT_OFFSET(cmd_text_s, maxsize, 4);
COD2_ASSERT_OFFSET(cmd_text_s, cursize, 8);

/* Scoped 0x20000-byte buffer object built by Com_PoolAllocInto2 (constructor) / Com_PoolRestoreFrom2 (destructor). */
struct ScopedBuf16 {
    unsigned char opaque[16];
};
COD2_ASSERT_SIZE(ScopedBuf16, 0x10);

/* Spatial tree node, array at 0x0818901c (index 1 is the root), stride 0x18. Same tag as chunk 005's area node. */
struct cod2_area_node_s {
    unsigned int maskA;           /* 0x00  tested by Area_TraceSegmentRecords (contents mask of its entry list B) */
    unsigned int maskB;           /* 0x04  tested by the traces in this chunk */
    unsigned short entryHead;     /* 0x08  first svEntity_s index of list used by this chunk, 0 = none */
    unsigned short entryHeadAlt;  /* 0x0a  first index of the list used by Area_TraceSegmentRecords (other entry array) */
    float dist;                   /* 0x0c  split plane distance */
    unsigned short axis;          /* 0x10  split axis 0..2 */
    unsigned short pad12;         /* 0x12 */
    unsigned short child[2];      /* 0x14  child node indices */
};
COD2_ASSERT_SIZE(cod2_area_node_s, 0x18);
COD2_ASSERT_OFFSET(cod2_area_node_s, maskB, 4);
COD2_ASSERT_OFFSET(cod2_area_node_s, entryHead, 8);
COD2_ASSERT_OFFSET(cod2_area_node_s, dist, 0xc);
COD2_ASSERT_OFFSET(cod2_area_node_s, axis, 0x10);
COD2_ASSERT_OFFSET(cod2_area_node_s, child, 0x14);

/* 1-based svEntity_s record n of the entity link array at 0x084424a4 (stride 0x174). */
static inline svEntity_s *cm6_entry(unsigned int index)
{
    return (svEntity_s *)(g_084424a4_areaEntityLinkRecords + index * sizeof(svEntity_s));
}

/*
 * Trace contexts. The five traces below read different offsets of their context. Only observed
 * members are modelled; the true total sizes were not recovered, so no size asserts.
 */
struct SegQueryB {                /* built on the stack by Collision_IsSegmentClear, 0x28 bytes used */
    float start[3];               /* 0x00 */
    float end[3];                 /* 0x0c */
    float invDelta[3];            /* 0x18  written by Box_ComputeInverseExtents */
    unsigned int contentsMask;    /* 0x24 */
};
COD2_ASSERT_SIZE(SegQueryB, 0x28);

struct SegQueryA {                /* SpatialTree_QuerySegment */
    float start[3];               /* 0x00 */
    float end[3];                 /* 0x0c */
    unsigned char unk18[8];
    unsigned int contentsMask;    /* 0x20 */
};
COD2_ASSERT_OFFSET(SegQueryA, end, 0xc);
COD2_ASSERT_OFFSET(SegQueryA, contentsMask, 0x20);

struct SegHitTrace {              /* SpatialTree_TraceSegmentHits */
    float start[3];               /* 0x00 */
    float end[3];                 /* 0x0c */
    unsigned char unk18[0x14];
    unsigned int contentsMask;    /* 0x2c */
};
COD2_ASSERT_OFFSET(SegHitTrace, end, 0xc);
COD2_ASSERT_OFFSET(SegHitTrace, contentsMask, 0x2c);

struct BoxQueryTrace {            /* SpatialTree_TraceBoxFirstHit */
    unsigned char unk0[0x18];
    float extents[3];             /* 0x18 */
    float start[3];               /* 0x24 */
    float end[3];                 /* 0x30 */
    unsigned char unk3c[8];
    unsigned int contentsMask;    /* 0x44 */
};
COD2_ASSERT_OFFSET(BoxQueryTrace, extents, 0x18);
COD2_ASSERT_OFFSET(BoxQueryTrace, start, 0x24);
COD2_ASSERT_OFFSET(BoxQueryTrace, end, 0x30);
COD2_ASSERT_OFFSET(BoxQueryTrace, contentsMask, 0x44);

struct BoxHitTrace {              /* SpatialTree_TraceBoxHits */
    unsigned char unk0[0x18];
    float extents[3];             /* 0x18 */
    float start[3];               /* 0x24 */
    float end[3];                 /* 0x30 */
    unsigned char unk3c[0x14];
    unsigned int contentsMask;    /* 0x50 */
};
COD2_ASSERT_OFFSET(BoxHitTrace, extents, 0x18);
COD2_ASSERT_OFFSET(BoxHitTrace, start, 0x24);
COD2_ASSERT_OFFSET(BoxHitTrace, end, 0x30);
COD2_ASSERT_OFFSET(BoxHitTrace, contentsMask, 0x50);

#endif
