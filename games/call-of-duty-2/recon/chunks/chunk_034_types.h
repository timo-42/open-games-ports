/*
 * Chunk 034 types: XAnim / XModel / XModelParts loaders and small math helpers
 * (cod2_lnxded 1.3, 0x080c3be4..0x080c79fe).
 * Layouts recovered from consistent offsets in the loaders; all sizes asserted.
 */
#ifndef COD2_CHUNK_034_TYPES_H
#define COD2_CHUNK_034_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include <float.h>

/* Allocator callback passed through the loaders: void *alloc(int size). */
typedef void *(*XAllocFn)(int size);

/* ---------------------------------------------------------------- XAnim */

/* Key list header. For lastIndex == 0 the single key is stored inline at +4
 * (quat: short[2..4], trans: float[3]); otherwise +4 is a pointer to the key
 * array and +8 optionally holds the frame-index table (uint8 or uint16 per
 * key; absent when every frame has a key). */
struct xanim_track_s {
    uint16 lastIndex;       /* 0x00 key count - 1 */
    uint16 pad;             /* 0x02 */
    void *keys;             /* 0x04 key array (when more than one key) */
    uint16 frameIndex[1];   /* 0x08 frame index table (uint8 or uint16 elements) */
};
COD2_ASSERT_OFFSET(xanim_track_s, keys, 4);
COD2_ASSERT_OFFSET(xanim_track_s, frameIndex, 8);

/* Per-bone key lists; the same layout is used for the delta track. */
struct XAnimBoneKeys {
    xanim_track_s *trans;  /* 0x00 */
    xanim_track_s *rot;    /* 0x04 */
};
COD2_ASSERT_SIZE(XAnimBoneKeys, 8);

struct XAnimNotify {
    uint16 name;            /* 0x00 script string */
    uint16 pad;             /* 0x02 */
    float time;             /* 0x04 normalized time */
};
COD2_ASSERT_SIZE(XAnimNotify, 8);

struct XAnimParts {
    uint16 numFrames;       /* 0x00 frames - 1 (+0 when looping) */
    uint8 looping;          /* 0x02 */
    uint8 hasDelta;         /* 0x03 */
    float frameRate;        /* 0x04 */
    float frequency;        /* 0x08 frameRate / numFrames, 0 when numFrames == 0 */
    uint8 numNotifies;      /* 0x0c includes the terminating "end" notify */
    uint8 pad0d;            /* 0x0d */
    uint16 numBones;        /* 0x0e */
    uint16 *boneNames;      /* 0x10 script strings */
    uint8 *twoDRotBits;     /* 0x14 one bit per bone: rotation stored as 2 shorts */
    XAnimBoneKeys *bones;   /* 0x18 */
    XAnimNotify *notifies;  /* 0x1c */
    XAnimBoneKeys *delta;   /* 0x20 */
    uint8 unused24[8];      /* 0x24 (allocated, never written by the loader) */
};
COD2_ASSERT_SIZE(XAnimParts, 0x2c);
COD2_ASSERT_OFFSET(XAnimParts, frameRate, 4);
COD2_ASSERT_OFFSET(XAnimParts, numNotifies, 0xc);
COD2_ASSERT_OFFSET(XAnimParts, numBones, 0xe);
COD2_ASSERT_OFFSET(XAnimParts, boneNames, 0x10);
COD2_ASSERT_OFFSET(XAnimParts, twoDRotBits, 0x14);
COD2_ASSERT_OFFSET(XAnimParts, bones, 0x18);
COD2_ASSERT_OFFSET(XAnimParts, notifies, 0x1c);
COD2_ASSERT_OFFSET(XAnimParts, delta, 0x20);

/* --------------------------------------------------------------- XModel */

struct DObjBone {
    float quat[4];          /* 0x00 */
    float trans[3];         /* 0x10 */
    float transWeight;      /* 0x1c */
};
COD2_ASSERT_SIZE(DObjBone, 0x20);

struct XModelBoneHierarchy {
    uint16 *names;          /* 0x00 script string per bone */
    int8 parentDelta[1];    /* 0x04 per non-root bone: index - parent index */
};

struct XModelParts {
    uint16 numBones;                /* 0x00 */
    uint16 numRootBones;            /* 0x02 */
    XModelBoneHierarchy *hierarchy; /* 0x04 */
    short *quats;                   /* 0x08 4 shorts per non-root bone */
    float *trans;                   /* 0x0c 3 floats per non-root bone */
    uint8 *partClassification;      /* 0x10 one byte per bone */
    uint32 boneMaskA[4];            /* 0x14 filled with 0xff by the loader */
    uint8 unknown24[16];            /* 0x24 */
    uint32 boneMaskB[4];            /* 0x34 filled with 0xff by the loader */
    DObjBone baseMat[1];         /* 0x44 numBones entries */
};
COD2_ASSERT_SIZE(XModelParts, 0x64);
COD2_ASSERT_OFFSET(XModelParts, hierarchy, 4);
COD2_ASSERT_OFFSET(XModelParts, quats, 8);
COD2_ASSERT_OFFSET(XModelParts, trans, 0xc);
COD2_ASSERT_OFFSET(XModelParts, partClassification, 0x10);
COD2_ASSERT_OFFSET(XModelParts, boneMaskA, 0x14);
COD2_ASSERT_OFFSET(XModelParts, boneMaskB, 0x34);
COD2_ASSERT_OFFSET(XModelParts, baseMat, 0x44);

struct XModelSurfs {
    uint32 header;                  /* default instance: 0 */
};

struct XModelLod {
    float dist;                     /* 0x00 */
    const char *name;               /* 0x04 xmodelsurfs asset name ("" when unused) */
    uint16 numSurfNames;            /* 0x08 */
    uint16 pad0a;                   /* 0x0a */
    uint16 *surfNames;              /* 0x0c script strings */
    XModelSurfs *surfs;             /* 0x10 */
};
COD2_ASSERT_SIZE(XModelLod, 0x14);

struct XModelCollTri {
    float plane[4];                 /* 0x00 normal + dist */
    float svec[4];                  /* 0x10 */
    float tvec[4];                  /* 0x20 */
};
COD2_ASSERT_SIZE(XModelCollTri, 0x30);

struct XModelCollSurf {
    XModelCollTri *collTris;        /* 0x00 */
    int numCollTris;                /* 0x04 */
    float mins[3];                  /* 0x08 */
    float maxs[3];                  /* 0x14 */
    int boneIdx;                    /* 0x20 */
    int contents;                   /* 0x24 */
    int surfFlags;                  /* 0x28 */
};
COD2_ASSERT_SIZE(XModelCollSurf, 0x2c);
COD2_ASSERT_OFFSET(XModelCollSurf, mins, 8);
COD2_ASSERT_OFFSET(XModelCollSurf, maxs, 0x14);
COD2_ASSERT_OFFSET(XModelCollSurf, boneIdx, 0x20);

struct XBoneInfo {
    float bounds[2][3];             /* 0x00 mins, maxs */
    float offset[3];                /* 0x18 midpoint of bounds */
    float radiusSquared;            /* 0x24 */
};
COD2_ASSERT_SIZE(XBoneInfo, 0x28);

struct XModel {
    XModelParts *parts;             /* 0x00 */
    XModelLod lod[4];               /* 0x04 */
    XModelCollSurf *collSurfs;      /* 0x54 */
    int numCollSurfs;               /* 0x58 */
    int contents;                   /* 0x5c OR of collision surface contents */
    XBoneInfo *boneInfo;            /* 0x60 */
    float mins[3];                  /* 0x64 */
    float maxs[3];                  /* 0x70 */
    uint16 numLods;                 /* 0x7c */
    int16 collLod;                  /* 0x7e low 16 bits of a header int */
    int unknown80;                  /* 0x80 never written by the loaders */
    int memUsage;                   /* 0x84 bytes allocated by the loaders */
    const char *name;               /* 0x88 */
    uint8 flags;                    /* 0x8c header byte */
    uint8 isDefault;                /* 0x8d */
    uint8 pad8e[2];                 /* 0x8e */
};
COD2_ASSERT_SIZE(XModel, 0x90);
COD2_ASSERT_OFFSET(XModel, lod, 4);
COD2_ASSERT_OFFSET(XModel, collSurfs, 0x54);
COD2_ASSERT_OFFSET(XModel, boneInfo, 0x60);
COD2_ASSERT_OFFSET(XModel, mins, 0x64);
COD2_ASSERT_OFFSET(XModel, maxs, 0x70);
COD2_ASSERT_OFFSET(XModel, numLods, 0x7c);
COD2_ASSERT_OFFSET(XModel, memUsage, 0x84);
COD2_ASSERT_OFFSET(XModel, name, 0x88);
COD2_ASSERT_OFFSET(XModel, isDefault, 0x8d);

/* On-disk "xmodel/<name>" header as parsed into a stack buffer. */
struct XModelFileLod {
    char name[0x400];
    float dist;
};
COD2_ASSERT_SIZE(XModelFileLod, 0x404);

struct XModelFileHeader {
    XModelFileLod lod[4];           /* 0x000 */
    float mins[3];                  /* 0x1010 */
    float maxs[3];                  /* 0x101c */
    uint32 collLod;                 /* 0x1028 */
    uint8 flags;                    /* 0x102c */
    uint8 pad[3];
};
COD2_ASSERT_SIZE(XModelFileHeader, 0x1030);
COD2_ASSERT_OFFSET(XModelFileHeader, mins, 0x1010);
COD2_ASSERT_OFFSET(XModelFileHeader, collLod, 0x1028);
COD2_ASSERT_OFFSET(XModelFileHeader, flags, 0x102c);

/* Result of a line trace against an xmodel (same shape as trace_t). */
struct XModelTrace {
    float fraction;                 /* 0x00 */
    float normal[3];                /* 0x04 */
    int surfaceFlags;               /* 0x10 */
    int contents;                   /* 0x14 */
    void *material;                 /* 0x18 not touched here */
    uint16 entityNum;               /* 0x1c not touched here */
    uint16 partName;                /* 0x1e not touched here */
    uint16 partGroup;               /* 0x20 not touched here */
    uint8 allSolid;                 /* 0x22 */
    uint8 startSolid;               /* 0x23 */
};
COD2_ASSERT_SIZE(XModelTrace, 0x24);
COD2_ASSERT_OFFSET(XModelTrace, surfaceFlags, 0x10);
COD2_ASSERT_OFFSET(XModelTrace, allSolid, 0x22);

/* Working line used by the trace helpers: start, end, 1/(start-end) of the box test. */
struct XModelTraceLine {
    float start[3];
    float end[3];
    float invDelta[3];
};

/* Globals and out-of-chunk/own prototypes come from cod2_globals.h / cod2_prototypes.h. */

#endif
