/* chunk_023_types.h: chunk-private types for chunk 023 (0x809a408..0x809eae0): server snapshot
 * send, voice relay, entity link/trace helpers, vector helpers and filesystem path helpers.
 * Link-wave migration: canonical structs, globals and shared prototypes now come from
 * cod2_canon.h / cod2_globals.h / cod2_prototypes.h; only chunk-private declarations remain. */
#ifndef CHUNK_023_TYPES_H
#define CHUNK_023_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

struct entlist_s {         /* count + ids written by the visibility query (08098b98 / 08098e32) */
    int count;
    int ents[1030];
};

struct cmtrace_s {         /* local collision-model trace record (cf08 local at ebp-0x68) */
    float fraction;        /* +0 */
    float dist;            /* +4 */
    float normal[3];       /* +8 */
    int16 field_14;        /* +0x14 */
    int16 field_16;        /* +0x16 */
};
COD2_ASSERT_OFFSET(cmtrace_s, normal, 8);
COD2_ASSERT_OFFSET(cmtrace_s, field_14, 0x14);


/* Clip descriptors passed to the entity-clip callbacks (chunk 006 owns BoxHitTrace / BoxQueryTrace /
 * SegHitTrace / SegQueryA; these are this chunk's views of the same objects, passed with a cast).
 * Four layouts are observed. */
/* Clip descriptors passed to the entity-clip callbacks.  Four layouts are observed. */
struct lineclip_a_s {      /* d884 / cf08 */
    float start[3];        /* +0 */
    float end[3];          /* +0xc */
    float delta[3];        /* +0x18 (filled by Box_ComputeInverseExtents) */
    int passEntityNum;     /* +0x24 */
    int passOwnerNum;      /* +0x28 */
    int contentmask;       /* +0x2c */
    int param30;           /* +0x30 */
    int param34;           /* +0x34 */
};
COD2_ASSERT_OFFSET(lineclip_a_s, passEntityNum, 0x24);
COD2_ASSERT_OFFSET(lineclip_a_s, param34, 0x34);
COD2_ASSERT_SIZE(lineclip_a_s, 0x38);

struct lineclip_b_s {      /* d4ea / dbbe / de38 */
    float start[3];        /* +0 */
    float end[3];          /* +0xc */
    int passEntityNum;     /* +0x18 */
    int passOwnerNum;      /* +0x1c */
    int contentmask;       /* +0x20 */
    int flag24;            /* +0x24 */
};
COD2_ASSERT_OFFSET(lineclip_b_s, passEntityNum, 0x18);
COD2_ASSERT_OFFSET(lineclip_b_s, flag24, 0x24);

struct boxclip_a_s {       /* d884 / cd64 */
    float mins[3];         /* +0 */
    float maxs[3];         /* +0xc */
    float extentPlus1[3];  /* +0x18 */
    float start[3];        /* +0x24 */
    float end[3];          /* +0x30 */
    float delta[3];        /* +0x3c (filled by Box_ComputeInverseExtents) */
    int passEntityNum;     /* +0x48 */
    int passOwnerNum;      /* +0x4c */
    int contentmask;       /* +0x50 */
};
COD2_ASSERT_OFFSET(boxclip_a_s, start, 0x24);
COD2_ASSERT_OFFSET(boxclip_a_s, passEntityNum, 0x48);
COD2_ASSERT_OFFSET(boxclip_a_s, contentmask, 0x50);

struct boxclip_b_s {       /* dbbe / de38 / d37c */
    float mins[3];         /* +0 */
    float maxs[3];         /* +0xc */
    float extentPlus1[3];  /* +0x18 */
    float start[3];        /* +0x24 */
    float end[3];          /* +0x30 */
    int passEntityNum;     /* +0x3c */
    int passOwnerNum;      /* +0x40 */
    int contentmask;       /* +0x44 */
};
COD2_ASSERT_OFFSET(boxclip_b_s, start, 0x24);
COD2_ASSERT_OFFSET(boxclip_b_s, passEntityNum, 0x3c);
COD2_ASSERT_OFFSET(boxclip_b_s, contentmask, 0x44);


/* Globals in gaps between symbols.json globals (declared in chunk_023.json "globals" for the merge lane). */
extern int g_sendBytesHistory[20];                /* bytes-per-frame history (slot 19 aliases 0849f684) */
extern int g_unsentBytesHistory[20];                /* unsent-bytes history (slot 19 aliases 0849f6e0) */
extern int g_0849f688, g_0849f68c, g_0849f690;
extern int g_0849f6e4, g_0849f6e8;
extern float g_0849f6ec;
extern int g_0849f6f0;
extern float g_0817db88[3];
extern float g_0817db94[3];
extern char com_quitOnErrorFlag;

#endif
