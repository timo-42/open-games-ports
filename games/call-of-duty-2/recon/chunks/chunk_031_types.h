/*
 * chunk 031 types: DObj (skeleton/model instance), XModel bone data and vector helpers.
 * Address range 0x080b8dde..0x080bc452 of cod2_lnxded 1.3.
 * Layouts recovered from consistent field offsets in this chunk; unknown regions are padded.
 */
#ifndef COD2_CHUNK_031_TYPES_H
#define COD2_CHUNK_031_TYPES_H

#include "cod2_types.h"

/* ---- math helper structs ---- */

/* Origin followed by three axis rows (row-major 3x3 at +0x0c). */
struct Orient {
    float origin[3];
    float axis[3][3];
};
COD2_ASSERT_OFFSET(Orient, axis, 0x0c);
COD2_ASSERT_SIZE(Orient, 0x30);

/* Plane-like record: normal then distance. */
struct Plane4 {
    float normal[3];
    float dist;
};
COD2_ASSERT_OFFSET(Plane4, dist, 0x0c);
COD2_ASSERT_SIZE(Plane4, 0x10);

/* ---- model data ---- */

/* Bone hierarchy blob: name table pointer, then one parent-delta byte per non-root bone. */
struct XModelHier {
    uint16 *boneNames;          /* 0x00: string ids, one per bone */
    uint8 parentDelta[1];       /* 0x04: variable length, indexed by (bone - numRootBones) */
};
COD2_ASSERT_OFFSET(XModelHier, parentDelta, 0x04);

struct XModelParts {
    int16 numBones;             /* 0x00 */
    int16 numRootBones;         /* 0x02 */
    XModelHier *hier;           /* 0x04 */
    uint8 unk08[4];             /* 0x08 */
    float (*boneOffsets)[3];    /* 0x0c: vec3 per non-root bone */
    uint8 *boneGroup;           /* 0x10: one group byte per bone */
    uint8 staticSkel[4];        /* 0x14: start of an embedded DObjSkel (see DObj_InitFromModelParts) */
};
COD2_ASSERT_OFFSET(XModelParts, hier, 0x04);
COD2_ASSERT_OFFSET(XModelParts, boneOffsets, 0x0c);
COD2_ASSERT_OFFSET(XModelParts, boneGroup, 0x10);
COD2_ASSERT_OFFSET(XModelParts, staticSkel, 0x14);

struct XModelLodSurfs {
    int *surfIds;               /* 0x00 */
    uint32 partBits[1];         /* 0x04: variable length bit set over model bones */
};
COD2_ASSERT_OFFSET(XModelLodSurfs, partBits, 0x04);

struct XModelLod {
    float unk00;                /* 0x00 */
    float unk04;                /* 0x04 */
    int16 numSurfs;             /* 0x08 */
    int16 unk0a;                /* 0x0a */
    uint16 *materialNames;      /* 0x0c: string id per surface (0 = default) */
    XModelLodSurfs *surfs;      /* 0x10 */
};
COD2_ASSERT_OFFSET(XModelLod, numSurfs, 0x08);
COD2_ASSERT_OFFSET(XModelLod, materialNames, 0x0c);
COD2_ASSERT_OFFSET(XModelLod, surfs, 0x10);
COD2_ASSERT_SIZE(XModelLod, 0x14);

/* Per-bone collision box. */
struct XBoneInfo {
    float mins[3];              /* 0x00 */
    float maxs[3];              /* 0x0c */
    float offset[3];            /* 0x18 */
    float radiusSq;             /* 0x24 */
};
COD2_ASSERT_OFFSET(XBoneInfo, offset, 0x18);
COD2_ASSERT_OFFSET(XBoneInfo, radiusSq, 0x24);
COD2_ASSERT_SIZE(XBoneInfo, 0x28);

struct XModel {
    XModelParts *parts;         /* 0x00 */
    XModelLod lods[4];          /* 0x04 */
    uint8 unk54[0x0c];          /* 0x54 */
    XBoneInfo *hitBoxes;     /* 0x60 */
    uint8 unk64[0x24];          /* 0x64 */
    const char *name;           /* 0x88 */
};
COD2_ASSERT_OFFSET(XModel, lods, 0x04);
COD2_ASSERT_OFFSET(XModel, hitBoxes, 0x60);
COD2_ASSERT_OFFSET(XModel, name, 0x88);

/* ---- animation tree (only the fields this chunk touches) ---- */
struct XAnimSet {
    int unk00;                  /* 0x00 */
    unsigned int numAnims;      /* 0x04 */
};
COD2_ASSERT_OFFSET(XAnimSet, numAnims, 0x04);

struct XAnimTree {
    XAnimSet *anims;            /* 0x00 */
    int16 entnum;               /* 0x04 */
    int16 pad06;                /* 0x06 */
};
COD2_ASSERT_OFFSET(XAnimTree, entnum, 0x04);

/* ---- DObj ---- */

/* One bone: rotation quaternion, origin, and 2/|q|^2 scale. */
struct DObjBone {
    float quat[4];              /* 0x00 */
    float origin[3];            /* 0x10 */
    float quatScale;            /* 0x1c */
};
COD2_ASSERT_OFFSET(DObjBone, origin, 0x10);
COD2_ASSERT_OFFSET(DObjBone, quatScale, 0x1c);
COD2_ASSERT_SIZE(DObjBone, 0x20);

struct DObjSkel {
    uint32 setBits[4];          /* 0x00: bones given a transform this frame */
    uint32 controlBits[4];      /* 0x10: bones with a controlled (explicit) transform */
    uint32 lockedBits[4];       /* 0x20: bones already evaluated/locked */
    DObjBone bones[1];          /* 0x30: variable length */
};
COD2_ASSERT_OFFSET(DObjSkel, controlBits, 0x10);
COD2_ASSERT_OFFSET(DObjSkel, lockedBits, 0x20);
COD2_ASSERT_OFFSET(DObjSkel, bones, 0x30);

struct DObj {
    XAnimTree *tree;            /* 0x00 */
    DObjSkel *skel;             /* 0x04 */
    int skelTime;               /* 0x08 */
    uint8 *animTreeData;        /* 0x0c */
    uint16 dupStringId;         /* 0x10: string id of the duplicate-part table */
    uint16 pad12;               /* 0x12 */
    int unk14;                  /* 0x14 */
    uint8 numModels;            /* 0x18 */
    uint8 numBones;             /* 0x19 */
    uint8 modelFlagBits;        /* 0x1a: bit i set when model i's definition flag was non-zero */
    uint8 pad1b;                /* 0x1b */
    XModel *models[8];          /* 0x1c */
    uint8 parentBone[8];        /* 0x3c: 0xff = root model */
    uint8 boneBase[8];          /* 0x44: first bone index of each model */
    float mins[3];              /* 0x4c */
    float maxs[3];              /* 0x58 */
};
COD2_ASSERT_OFFSET(DObj, skel, 0x04);
COD2_ASSERT_OFFSET(DObj, skelTime, 0x08);
COD2_ASSERT_OFFSET(DObj, animTreeData, 0x0c);
COD2_ASSERT_OFFSET(DObj, dupStringId, 0x10);
COD2_ASSERT_OFFSET(DObj, numModels, 0x18);
COD2_ASSERT_OFFSET(DObj, numBones, 0x19);
COD2_ASSERT_OFFSET(DObj, modelFlagBits, 0x1a);
COD2_ASSERT_OFFSET(DObj, models, 0x1c);
COD2_ASSERT_OFFSET(DObj, parentBone, 0x3c);
COD2_ASSERT_OFFSET(DObj, boneBase, 0x44);
COD2_ASSERT_OFFSET(DObj, mins, 0x4c);
COD2_ASSERT_OFFSET(DObj, maxs, 0x58);
COD2_ASSERT_SIZE(DObj, 0x64);

/* Model definition passed to DObj_Create (array stride 0x0c). */
struct DObjModelDef {
    XModel *model;              /* 0x00 */
    const char *parentBoneName; /* 0x04 */
    int flag;                   /* 0x08 */
};
COD2_ASSERT_SIZE(DObjModelDef, 0x0c);

/* Result written by DObj_Traceline / DObj_GeomTraceline. */
struct DObjTraceResult {
    float fraction;             /* 0x00 */
    float field04;              /* 0x04 */
    float normal[3];            /* 0x08 */
    uint16 partName;            /* 0x14: bone name string id */
    uint16 partGroup;           /* 0x16 */
};
COD2_ASSERT_OFFSET(DObjTraceResult, normal, 0x08);
COD2_ASSERT_OFFSET(DObjTraceResult, partName, 0x14);
COD2_ASSERT_SIZE(DObjTraceResult, 0x18);

/* Trace record handed to the per-model collision call (XModel_TraceLine). */
struct ModelTrace {
    float fraction;             /* 0x00 */
    float normal[3];            /* 0x04 */
    float field10;              /* 0x10 */
};
COD2_ASSERT_SIZE(ModelTrace, 0x14);

/* Surface type table entry (stride 0x14) at 0x08182e00, 22 entries. */
struct SurfaceTypeEntry {
    const char *name;           /* 0x00 */
    int field04;                /* 0x04 */
    uint32 flags;               /* 0x08: (flags & 0x1f00000) >> 20 is the 1-based type index */
    int field0c;                /* 0x0c */
    int field10;                /* 0x10 */
};
COD2_ASSERT_SIZE(SurfaceTypeEntry, 0x14);

/* ---- globals ---- */

/* ---- out-of-chunk callees (placeholders rewritten by the merge lane) ---- */

#endif
