#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_031_types.h"

/*
 * Chunk 031: DObj (model/skeleton instance) support and vector helpers,
 * 0x080b8dde..0x080bc452.  Names marked inferred follow the id Tech 3 / IW DObj API shape
 * (DObjCreate, DObjCalcSkel, DObjTraceline, ...) matched by call structure and the strings
 * "No Dobj", "Bone %d", "Max surfs exceeded".
 */

/* ---- forward declarations of in-chunk functions ---- */
void Orient_VecLocalToWorld(const Orient *xf, const float *v, float *out);
void Orient_VecWorldToLocal(const Orient *xf, const float *v, float *out);
long double Vec3_Dot_080b93c4(const float *a, const float *b);
void Vec3_Set_080bc0e0(float *v, float x, float y, float z);
void Vec3_MA_080bc1f4(const float *a, float s, const float *b, float *out);
void Vec3_Copy_080bc104(const float *src, float *dst);
void Vec3_Add_080bc134(const float *a, const float *b, float *out);
void Vec3_Sub_080bc178(const float *a, const float *b, float *out);
void Vec3_Scale_080bc1bc(const float *v, float s, float *out);
void Vec3_Clear_080bc0b6(float *v);
long double Vec3_LengthSq_080bc27c(const float *v);
long double Vec4_LengthSq_080bc308(const float *v);
void Vec2_Copy_080bc074(const float *src, float *dst);
long double Vec2_Dot_080bc094(const float *a, const float *b);
void Vec4_Set_080bc2b0(float *v, float x, float y, float z, float w);
float Float_SelectNonNegative_080bc2de(float x, float ifNonNeg, float ifNeg);
float Float_SignOne(float x);
void SinCos_Float_080bc046(float angle, float *outSin, float *outCos);
void Quat_UpdateScale(DObjBone *bone);
void Bone_TransformPointByParent(float *p, const DObjBone *parent);
void Quat_MultiplyInPlace(float *q1, const float *q2);
int DObj_GetBoneIndex(const DObj *obj, int boneNameId);
void DObj_BuildDuplicatePartMap(DObj *obj);
const char *DObj_GetBoneName(const DObj *obj, int boneIndex);
DObjBone *DObj_GetRotTransArray(const DObj *obj);
XModel *DObj_GetModel(const DObj *obj, int index);
void DObj_ComputeBounds(DObj *obj);
void DObj_BindAnimTree(DObj *obj, XAnimTree *tree);
void DObj_SetBoneTransform(DObj *obj, const float *origin, const float *angles, int boneIndex);
int DObj_SetRotTransIndex(DObj *obj, const uint32 *partBits, int boneIndex);
int DObj_SetControlRotTransIndex(DObj *obj, const uint32 *partBits, int boneIndex);
void DObj_WorldPointToBoneLocal(const float *point, const DObjBone *bone, float *out);
/* ---------------------------------------------------------------- */

// 0x080b8dde  size=155  FUN_080b8dde  status=translated  naming=unknown
// Naming evidence: out = sum_i v[i]*axis[i] (rows of the 3x3 at +0x0c); called by plane/orient helpers; descriptive name only.
void Orient_VecLocalToWorld(const Orient *xf, const float *v, float *out)
{
    out[0] = v[2] * xf->axis[2][0] + v[1] * xf->axis[1][0] + v[0] * xf->axis[0][0];
    out[1] = v[2] * xf->axis[2][1] + v[1] * xf->axis[1][1] + v[0] * xf->axis[0][1];
    out[2] = v[2] * xf->axis[2][2] + v[1] * xf->axis[1][2] + v[0] * xf->axis[0][2];
}

// 0x080b8e7a  size=76  FUN_080b8e7a  status=translated  naming=unknown
// Naming evidence: rotates the plane normal with Orient_VecLocalToWorld, then dist += dot(origin, newNormal); descriptive name only.
void Orient_PlaneLocalToWorld(const Orient *xf, const Plane4 *in, Plane4 *out)
{
    long double d;

    Orient_VecLocalToWorld(xf, in->normal, out->normal);
    d = Vec3_Dot_080b93c4(xf->origin, out->normal);
    out->dist = (float)((long double)in->dist + d);
}

// 0x080b8ec6  size=169  FUN_080b8ec6  status=translated  naming=unknown
// Naming evidence: (p - origin) dotted with each axis row; descriptive name only.
void Orient_PointWorldToLocal(const Orient *xf, const float *p, float *out)
{
    /* x87 store points of the original (fstp to the frame) are kept with volatile temporaries,
     * and each row sums in the original order x, y, z. */
    volatile float dx = p[0] - xf->origin[0];
    volatile float dy = p[1] - xf->origin[1];
    volatile float dz = p[2] - xf->origin[2];

    out[0] = dx * xf->axis[0][0] + dy * xf->axis[0][1] + dz * xf->axis[0][2];
    out[1] = dx * xf->axis[1][0] + dy * xf->axis[1][1] + dz * xf->axis[1][2];
    out[2] = dx * xf->axis[2][0] + dy * xf->axis[2][1] + dz * xf->axis[2][2];
}

// 0x080b8f70  size=155  FUN_080b8f70  status=translated  naming=unknown
// Naming evidence: out[i] = dot(v, axis[i]); inverse rotation of Orient_VecLocalToWorld; descriptive name only.
void Orient_VecWorldToLocal(const Orient *xf, const float *v, float *out)
{
    out[0] = v[2] * xf->axis[0][2] + v[1] * xf->axis[0][1] + v[0] * xf->axis[0][0];
    out[1] = v[2] * xf->axis[1][2] + v[1] * xf->axis[1][1] + v[0] * xf->axis[1][0];
    out[2] = v[2] * xf->axis[2][2] + v[1] * xf->axis[2][1] + v[0] * xf->axis[2][0];
}

// 0x080b900c  size=126  FUN_080b900c  status=translated  naming=unknown
// Naming evidence: rotates the three axis rows of 'in' by xf and finishes the origin with Orient_TransformPoint; descriptive name only.
void Orient_Concat(const Orient *in, const Orient *xf, Orient *out)
{
    Orient_VecLocalToWorld(xf, in->axis[0], out->axis[0]);
    Orient_VecLocalToWorld(xf, in->axis[1], out->axis[1]);
    Orient_VecLocalToWorld(xf, in->axis[2], out->axis[2]);
    Orient_TransformPoint((const float *)xf, (const float *)in, (float *)out);
}

// 0x080b908a  size=76  FUN_080b908a  status=translated  naming=unknown
// Naming evidence: inverse-rotates the normal, dist = in.dist - dot(origin, in.normal); descriptive name only.
void Orient_PlaneWorldToLocal(const Orient *xf, const Plane4 *in, Plane4 *out)
{
    long double d;

    Orient_VecWorldToLocal(xf, in->normal, out->normal);
    d = Vec3_Dot_080b93c4(xf->origin, in->normal);
    out->dist = (float)((long double)in->dist - d);
}

// 0x080b90d6  size=189  FUN_080b90d6  status=translated  naming=unknown
// Naming evidence: out = origin + scale * (v transformed by axis columns); descriptive name only.
void Orient_PointLocalToWorldScaled(const Orient *xf, float scale, const float *v, float *out)
{
    out[0] = xf->origin[0] +
             (v[2] * xf->axis[2][0] + v[1] * xf->axis[1][0] + v[0] * xf->axis[0][0]) * scale;
    out[1] = xf->origin[1] +
             (v[2] * xf->axis[2][1] + v[1] * xf->axis[1][1] + v[0] * xf->axis[0][1]) * scale;
    out[2] = xf->origin[2] +
             (v[2] * xf->axis[2][2] + v[1] * xf->axis[1][2] + v[0] * xf->axis[0][2]) * scale;
}

// 0x080b9194  size=81  FUN_080b9194  status=translated  naming=unknown
// Naming evidence: plane variant of the scaled local->world transform; descriptive name only.
void Orient_PlaneLocalToWorldScaled(const Orient *xf, float scale, const Plane4 *in, Plane4 *out)
{
    float inDist;
    long double d;

    Orient_VecLocalToWorld(xf, in->normal, out->normal);
    inDist = in->dist;
    d = Vec3_Dot_080b93c4(xf->origin, out->normal);
    out->dist = (float)(d + (long double)(inDist * scale));
}

// 0x080b91e6  size=192  FUN_080b91e6  status=translated  naming=unknown
// Naming evidence: inverse of Orient_PointLocalToWorldScaled (divides by scale via 1.0/scale); descriptive name only.
void Orient_PointWorldToLocalScaled(const Orient *xf, float scale, const float *p, float *out)
{
    volatile float dx = p[0] - xf->origin[0];
    volatile float dy = p[1] - xf->origin[1];
    volatile float dz = p[2] - xf->origin[2];
    volatile float inv = (float)(1.0 / scale);

    out[0] = (dx * xf->axis[0][0] + dy * xf->axis[0][1] + dz * xf->axis[0][2]) * inv;
    out[1] = (dx * xf->axis[1][0] + dy * xf->axis[1][1] + dz * xf->axis[1][2]) * inv;
    out[2] = (dx * xf->axis[2][0] + dy * xf->axis[2][1] + dz * xf->axis[2][2]) * inv;
}

// 0x080b92a6  size=79  FUN_080b92a6  status=translated  naming=unknown
// Naming evidence: plane variant of the scaled world->local transform; descriptive name only.
void Orient_PlaneWorldToLocalScaled(const Orient *xf, float scale, const Plane4 *in, Plane4 *out)
{
    long double d;

    Orient_VecWorldToLocal(xf, in->normal, out->normal);
    d = Vec3_Dot_080b93c4(xf->origin, in->normal);
    out->dist = (float)(((long double)in->dist - d) / (long double)scale);
}

// 0x080b92f6  size=25  FUN_080b92f6  status=translated  naming=unknown
// Naming evidence: fabs on a float, result rounded to float; descriptive name only.
float Float_Abs_080b92f6(float x)
{
    return fabsf(x);
}

// 0x080b9310  size=31  FUN_080b9310  status=translated  naming=unknown
// Naming evidence: (float)sqrt((double)x); descriptive name only.
float Float_Sqrt_080b9310(float x)
{
    double r = sqrt((double)x);
    return (float)r;
}

// 0x080b9330  size=10  FUN_080b9330  status=translated  naming=unknown
// Naming evidence: returns the first dword of its argument; descriptive name only.
uint32 Read_FirstDword(const uint32 *p)
{
    return *p;
}

// 0x080b933a  size=17  FUN_080b933a  status=translated  naming=unknown
// Naming evidence: float identity; descriptive name only.
float Float_Identity_080b933a(float x)
{
    return x;
}

// 0x080b934c  size=35  FUN_080b934c  status=translated  naming=unknown
// Naming evidence: stores three values into a vec3; descriptive name only.
void Vec3_Set_b_080b934c(float *v, float x, float y, float z)
{
    /* the original copies the argument dwords with integer moves (no x87 load/store, so signaling NaN payloads survive) */
    ((volatile uint32 *)v)[0] = *(volatile uint32 *)&x;
    ((volatile uint32 *)v)[1] = *(volatile uint32 *)&y;
    ((volatile uint32 *)v)[2] = *(volatile uint32 *)&z;
}

// 0x080b9370  size=83  FUN_080b9370  status=translated  naming=unknown
// Naming evidence: out = a + s*b; descriptive name only.
void Vec3_MA_b(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x080b93c4  size=51  FUN_080b93c4  status=translated  naming=unknown
// Naming evidence: vec3 dot product accumulated in extended precision; descriptive name only.
long double Vec3_Dot_080b93c4(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2] + (long double)a[1] * (long double)b[1] +
           (long double)a[0] * (long double)b[0];
}

// 0x080b93f8  size=145  FUN_080b93f8  status=translated  naming=inferred
// Naming evidence: strcasecmp against "default" and the 22-entry surface-type table (bark..asphalt) at 0x08182e00; returns the 1-based type index from flags bits 20..24, 0 for "default", -1 if unknown.
int Com_SurfaceTypeFromName(const char *name)
{
    int i;

    if (strcasecmp(name, "default") == 0) {
        return 0;
    }
    for (i = 0; i < 22; i++) {
        if (strcasecmp(name, com_surfaceTypeTable[i].name) == 0) {
            return (int)(com_surfaceTypeTable[i].flags & 0x1f00000) >> 20;
        }
    }
    return -1;
}

// 0x080b948a  size=54  FUN_080b948a  status=translated  naming=inferred
// Naming evidence: inverse of Com_SurfaceTypeFromName; index 1..22 selects table[index-1].name, otherwise "default".
const char *Com_SurfaceTypeToName(int index)
{
    if (index < 1 || index > 0x16) {
        return "default";
    }
    return com_surfaceTypeTable[index - 1].name;
}

// 0x080b94c0  size=75  FUN_080b94c0  status=translated  naming=inferred
// Naming evidence: interns a 17-byte all-zero duplicate-part table (empty mask + terminator) into g_dobjEmptyDupPartTable; same table layout built by DObj_BuildDuplicatePartMap.
void DObj_InitGlobals(void)
{
    uint8 emptyTable[40];

    memset(emptyTable, 0, 0x14);
    g_dobjEmptyDupPartTable = ScrStr_Intern((const char *)emptyTable, 0, 0x11, 0xc);
}

// 0x080b950c  size=48  FUN_080b950c  status=translated  naming=inferred
// Naming evidence: releases the string interned by DObj_InitGlobals and clears g_dobjEmptyDupPartTable.
void DObj_ShutdownGlobals(void)
{
    if (g_dobjEmptyDupPartTable != 0) {
        ScrStr_DecRefOrFree(g_dobjEmptyDupPartTable, 0x11);
        g_dobjEmptyDupPartTable = 0;
    }
}

// 0x080b953c  size=15  FUN_080b953c  status=translated  naming=inferred
// Naming evidence: zeroes g_dobjEmptyDupPartTable without releasing it.
void DObj_ClearGlobals(void)
{
    g_dobjEmptyDupPartTable = 0;
}

// 0x080b954c  size=415  FUN_080b954c  status=translated  naming=inferred
// Naming evidence: prints "No Dobj", "Models:", "Bones:", "Part duplicates:" listings of a DObj (DObjDumpInfo-style).
void DObj_DumpInfo(const DObj *obj)
{
    int i;
    int boneCount;
    const uint8 *pair;

    if (obj == 0) {
        Com_Printf("No Dobj\n");
        return;
    }
    Com_Printf("\nModels:\n");
    {
        uint8 numModels = obj->numModels;
        int firstBone = 0;
        for (i = 0; i < (int)(unsigned)numModels; i++) {
            XModel *model = obj->models[i];
            Com_Printf("%d: '%s'\n", firstBone, model->name);
            firstBone = firstBone + model->parts->numBones;
        }
    }
    Com_Printf("\nBones:\n");
    boneCount = obj->numBones;
    for (i = 0; i < boneCount; i++) {
        const char *boneName = DObj_GetBoneName(obj, i);
        Com_Printf("Bone %d: '%s'\n", i, boneName);
    }
    if (obj->dupStringId == 0) {
        Com_Printf("\nNo part duplicates.\n");
    } else {
        char *dup;

        Com_Printf("\nPart duplicates:\n");
        dup = (char *)ScrStr_TextFromHandle(obj->dupStringId);
        for (pair = (const uint8 *)(dup + 0x10); *pair != 0; pair += 2) {
            const char *targetName = DObj_GetBoneName(obj, pair[1] - 1);
            const char *sourceName = DObj_GetBoneName(obj, *pair - 1);
            Com_Printf("%d ('%s') -> %d ('%s')\n", *pair - 1, sourceName, pair[1] - 1, targetName);
        }
    }
    Com_Printf("\n");
}

// 0x080b96ec  size=24  FUN_080b96ec  status=translated  naming=unknown
// Naming evidence: tests bit 'modelIndex' of DObj.modelFlagBits (set from the model definition's third field in DObj_Create); descriptive name only.
unsigned int DObj_IsModelFlagSet(const DObj *obj, uint8 modelIndex)
{
    return (int)(unsigned)obj->modelFlagBits >> (modelIndex & 0x1f) & 1;
}

// 0x080b9704  size=123  FUN_080b9704  status=translated  naming=inferred
// Naming evidence: walks the models calling XModel_GetBoneIndex (per-model bone lookup) and adds the preceding models' bone counts (DObjGetBoneIndex-style); returns -1 if absent.
int DObj_GetBoneIndex(const DObj *obj, int boneNameId)
{
    unsigned numModels = obj->numModels;
    int baseBone = 0;
    int i = 0;

    for (;;) {
        XModel *model;
        int found;

        if ((int)numModels <= i) {
            return -1;
        }
        model = obj->models[i];
        found = XModel_GetBoneIndex(model, boneNameId);
        if (-1 < found) {
            return found + baseBone;
        }
        baseBone = baseBone + model->parts->numBones;
        i = i + 1;
    }
}

// 0x080b9780  size=535  FUN_080b9780  status=translated  naming=inferred
// Naming evidence: string "WARNING: Attempting to meld model, but root part '%s' ..."; builds the duplicate-part table (bitmask + (bone+1, target+1) byte pairs, 0 terminated) and interns it into DObj.dupStringId.
void DObj_BuildDuplicatePartMap(DObj *obj)
{
    struct DupTable {
        uint32 mask[4];
        uint8 pairs[1100];
    } table;
    int pairCount;
    int baseBone;
    int i;

    memset(table.mask, 0, 0x10);
    pairCount = 0;
    baseBone = (int)obj->models[0]->parts->numBones;
    for (i = 1; i < (int)(unsigned)obj->numModels; i++) {
        XModel *model = obj->models[i];

        if (obj->parentBone[i] == 0xff) {
            XModelParts *parts = model->parts;
            uint16 *boneNames = parts->hier->boneNames;
            int numBones = (int)parts->numBones;
            char rootRelocated = 0;
            int found = -1;
            int j;

            for (j = 0; j < numBones; j++) {
                found = DObj_GetBoneIndex(obj, boneNames[j]);
                if (found != j + baseBone) {
                    int globalIndex;

                    if (j == 0) {
                        rootRelocated = 1;
                    }
                    globalIndex = j + baseBone;
                    table.pairs[pairCount] = (uint8)globalIndex + 1;
                    table.mask[globalIndex >> 5] = 1u << (globalIndex & 0x1f) | table.mask[globalIndex >> 5];
                    table.pairs[pairCount + 1] = (char)found + 1;
                    pairCount = pairCount + 2;
                }
            }
            if (rootRelocated == 0) {
                char *rootName = (char *)ScrStr_TextFromHandle(boneNames[0]);
                Com_Printf("WARNING: Attempting to meld model, but root part '%s' of model '%s' not found in model '%s' or any of its descendants\n",
                            rootName, model->name, obj->models[0]->name);
            }
        }
        baseBone = baseBone + model->parts->numBones;
    }
    if (pairCount == 0) {
        obj->dupStringId = (uint16)g_dobjEmptyDupPartTable;
    } else {
        int length;

        table.pairs[pairCount] = 0;
        length = pairCount + 0x11;
        pairCount = pairCount + 1;
        obj->dupStringId = (uint16)ScrStr_Intern((const char *)table.mask, 0, length, 0xc);
    }
}

// 0x080b9998  size=487  FUN_080b9998  status=translated  naming=inferred
// Naming evidence: walks a bone up its parent chain (and through duplicate-part aliases) setting every visited bone's bit in partBits (DObjGetHierarchyBits-style).
void DObj_GetHierarchyBits(DObj *obj, unsigned boneIndex, uint32 *partBits)
{
    int modelBase[8];
    unsigned i;
    unsigned numModels;
    const uint8 *dupData;
    const uint8 *pairs;
    const uint8 *pair;
    XModelParts *parts;
    const uint8 *parentDelta;
    int rel;
    int lastIdx;

    for (i = 0; (int)i < 4; i = i + 1) {
        partBits[i] = 0;
    }
    numModels = obj->numModels;
    if (obj->dupStringId == 0) {
        DObj_BuildDuplicatePartMap(obj);
    }
    dupData = (const uint8 *)ScrStr_TextFromHandle(obj->dupStringId);
    pairs = dupData + 0x10;
    pair = pairs;
    modelBase[0] = 0;
    i = 0;
    for (;;) {
        parts = obj->models[i]->parts;
        lastIdx = (int)parts->numBones + modelBase[i];
        if ((int)boneIndex < lastIdx) {
            break;
        }
        i = i + 1;
        if (i == numModels) {
            return;
        }
        modelBase[i] = lastIdx;
    }
    parentDelta = parts->hier->parentDelta;
    for (;;) {
        int word;

        rel = boneIndex - modelBase[i];
        do {
            word = (int)boneIndex >> 5;
            partBits[word] = 1u << (boneIndex & 0x1f) | partBits[word];
            if ((((uint8)(((const int *)dupData)[word] >> (boneIndex & 0x1f)) ^ 1) & 1) == 0) {
                for (pair = pairs; (int)boneIndex != (int)*pair - 1; pair = pair + 2) {
                }
                boneIndex = pair[1] - 1;
            } else {
                lastIdx = rel - parts->numRootBones;
                if (-1 < lastIdx) {
                    break;
                }
                boneIndex = obj->parentBone[i];
                if (boneIndex == 0xff) {
                    return;
                }
            }
            do {
                i = i - 1;
                rel = boneIndex - modelBase[i];
            } while ((int)(boneIndex - modelBase[i]) < 0);
            parts = obj->models[i]->parts;
            parentDelta = parts->hier->parentDelta;
        } while (true);
        boneIndex = boneIndex - parentDelta[lastIdx];
    }
}

// 0x080b9d6e  size=44  FUN_080b9d6e  status=translated  naming=inferred
// Naming evidence: reads bit 'boneIndex' of the skeleton's locked/evaluated mask (+0x20).
unsigned int DObj_IsBoneLocked(const DObj *obj, int boneIndex)
{
    return (int)obj->skel->lockedBits[boneIndex >> 5] >> (boneIndex & 0x1f) & 1;
}

// 0x080b9d9a  size=95  FUN_080b9d9a  status=translated  naming=inferred
// Naming evidence: returns 1 when every bit of partBits is already set in the skeleton's locked mask.
int DObj_AreBonesLocked(const DObj *obj, const uint32 *partBits)
{
    int i = 0;

    for (;;) {
        if (3 < i) {
            return 1;
        }
        if ((~obj->skel->lockedBits[i] & partBits[i]) != 0) {
            break;
        }
        i = i + 1;
    }
    return 0;
}

// 0x080b9dfa  size=1358  FUN_080b9dfa  status=translated  naming=inferred
// Naming evidence: evaluates the skeleton bones requested in partBits by composing child quaternions/origins with parents and copying duplicate-part aliases (DObjCalcSkel-style); uses the dup table from DObj_BuildDuplicatePartMap.
void DObj_CalcSkel(DObj *obj, const uint32 *partBits)
{
    DObjSkel *skel;
    bool allDone;
    int i;
    uint32 skipBits[4];     /* ~partBits | locked : bit set = nothing to do (auStack_5c) */
    uint32 calcBits[4];     /* bones that need a fresh transform (auStack_4c) */
    uint32 combinedBits[4]; /* skipBits | control | duplicate (auStack_6c) */
    const uint8 *dupPair;
    const char *dupData;
    DObjBone *boneBase;
    DObjBone *cur;
    int boneIdx;
    int modelIdx;
    int numModels;

    skel = obj->skel;
    allDone = true;
    for (i = 0; i < 4; i++) {
        skipBits[i] = ~partBits[i] | skel->lockedBits[i];
        if (skipBits[i] != 0xffffffff) {
            allDone = false;
        }
    }
    if (allDone) {
        return;
    }
    if (obj->dupStringId == 0) {
        DObj_BuildDuplicatePartMap(obj);
    }
    dupData = (char *)ScrStr_TextFromHandle(obj->dupStringId);
    dupPair = (const uint8 *)(dupData + 0x10);
    for (i = 0; i < 4; i++) {
        skel->lockedBits[i] = partBits[i] | skel->lockedBits[i];
        calcBits[i] = ~skipBits[i] & skel->controlBits[i];
        combinedBits[i] = skipBits[i] | calcBits[i] | ((const uint32 *)dupData)[i];
    }
    for (i = 0; i < 4; i++) {
        calcBits[i] = ~combinedBits[i] | calcBits[i];
    }
    numModels = obj->numModels;
    boneBase = skel->bones;
    cur = skel->bones;
    boneIdx = 0;
    for (modelIdx = 0; modelIdx < numModels; modelIdx++) {
        XModelParts *parts = obj->models[modelIdx]->parts;
        uint8 parentBone = obj->parentBone[modelIdx];
        int count;

        if (parentBone == 0xff) {
            for (count = parts->numRootBones; count != 0; count--) {
                uint32 bit = 1u << (boneIdx & 0x1f);
                const uint8 *nextPair = dupPair;

                if ((bit & calcBits[boneIdx >> 5]) == 0) {
                    if (boneIdx == *dupPair - 1) {
                        nextPair = dupPair + 2;
                        if ((bit & skipBits[boneIdx >> 5]) == 0) {
                            *cur = boneBase[dupPair[1] - 1];
                        }
                    }
                } else {
                    Quat_UpdateScale(cur);
                }
                dupPair = nextPair;
                cur = cur + 1;
                boneIdx = boneIdx + 1;
            }
        } else {
            DObjBone *parentBonePtr = boneBase + parentBone;

            for (count = parts->numRootBones; count != 0; count--) {
                uint32 bit = 1u << (boneIdx & 0x1f);

                if ((bit & calcBits[boneIdx >> 5]) != 0) {
                    if ((combinedBits[boneIdx >> 5] & bit) == 0) {
                        Quat_MultiplyInPlace(cur->quat, parentBonePtr->quat);
                    } else {
                        Quat_MultiplyInPlace_080bc58a(parentBonePtr->quat, cur->quat);
                    }
                    Quat_UpdateScale(cur);
                    Bone_TransformPointByParent(cur->origin, parentBonePtr);
                }
                cur = cur + 1;
                boneIdx = boneIdx + 1;
            }
        }
        {
            float (*offsets)[3] = parts->boneOffsets;
            const uint8 *parentDelta = parts->hier->parentDelta;

            for (count = (int)parts->numBones - (int)parts->numRootBones; count != 0; count--) {
                int word = boneIdx >> 5;
                uint32 bit = 1u << (boneIdx & 0x1f);
                const uint8 *nextPair = dupPair;

                if ((bit & calcBits[word]) == 0) {
                    if (boneIdx == *dupPair - 1) {
                        nextPair = dupPair + 2;
                        if ((bit & skipBits[word]) == 0) {
                            *cur = boneBase[dupPair[1] - 1];
                        }
                    }
                } else {
                    DObjBone *parentBonePtr = cur - *parentDelta;

                    if ((combinedBits[word] & bit) == 0) {
                        Quat_MultiplyInPlace(cur->quat, parentBonePtr->quat);
                    } else {
                        Quat_MultiplyInPlace_080bc58a(parentBonePtr->quat, cur->quat);
                    }
                    Quat_UpdateScale(cur);
                    Vec3_Add_080bc134(cur->origin, *offsets, cur->origin);
                    Bone_TransformPointByParent(cur->origin, parentBonePtr);
                }
                dupPair = nextPair;
                cur = cur + 1;
                offsets = offsets + 1;
                parentDelta = parentDelta + 1;
                boneIdx = boneIdx + 1;
            }
        }
    }
}

// 0x080ba348  size=139  FUN_080ba348  status=translated  naming=inferred
// Naming evidence: stores the anim tree in the DObj and bumps a per-tree generation byte stored after the 2*numAnims-byte tables; on wrap it becomes 1 and the following numAnims bytes are cleared.
void DObj_BindAnimTree(DObj *obj, XAnimTree *tree)
{
    size_t numAnims;
    char *counter;
    char generation;

    obj->tree = tree;
    if (tree == 0) {
        obj->animTreeData = 0;
        return;
    }
    numAnims = tree->anims->numAnims;
    obj->animTreeData = (uint8 *)tree + numAnims * 2 + 8;
    counter = (char *)(numAnims * 2 + obj->animTreeData);
    generation = *counter + 1;
    if (generation == 0) {
        generation = 1;
        memset(counter + 1, 0, numAnims);
    }
    *counter = generation;
}

// 0x080ba3d4  size=255  FUN_080ba3d4  status=translated  naming=inferred
// Naming evidence: sums each model's bounds (XModel_GetBounds) into DObj.mins/maxs (adds, not min/max).
void DObj_ComputeBounds(DObj *obj)
{
    float totalMaxs[4];
    float totalMins[4];
    float modelMaxs[4];
    float modelMins[6];
    int i;
    unsigned numModels;

    numModels = obj->numModels;
    Vec3_Set_080bc0e0(totalMins, 0, 0, 0);
    Vec3_Set_080bc0e0(totalMaxs, 0, 0, 0);
    for (i = 0; i < (int)numModels; i++) {
        if (obj->models[i] != 0) {
            XModel_GetBounds(obj->models[i], modelMins, modelMaxs);
            Vec3_Add_080bc134(totalMins, modelMins, totalMins);
            Vec3_Add_080bc134(totalMaxs, modelMaxs, totalMaxs);
        }
    }
    Vec3_Copy_080bc104(totalMins, obj->mins);
    Vec3_Copy_080bc104(totalMaxs, obj->maxs);
}

// 0x080ba4d4  size=561  FUN_080ba4d4  status=translated  naming=inferred
// Naming evidence: strings "WARNING: Part '%s' not found in model '%s' or any of its descendants" and the 0x7f-bone limit error; fills a DObj from model definitions (DObjCreate-style).
void DObj_Create(DObjModelDef *defs, unsigned numDefs, XAnimTree *tree, DObj *obj, uint16 entnum)
{
    int modelCount;
    int totalBones;
    DObjModelDef *def;
    unsigned defIdx;

    obj->skel = 0;
    obj->skelTime = 0;
    obj->dupStringId = 0;
    obj->modelFlagBits = 0;
    DObj_BindAnimTree(obj, tree);
    if (tree != 0) {
        tree->entnum = (int16)entnum;
    }
    modelCount = 0;
    totalBones = 0;
    def = defs;
    defIdx = 0;
    for (;;) {
        XModel *model;
        int newTotal;

        if (numDefs <= defIdx) {
            break;
        }
        model = def->model;
        obj->models[modelCount] = model;
        obj->parentBone[modelCount] = 0xff;
        obj->boneBase[modelCount] = (uint8)totalBones;
        if (def->flag != 0) {
            obj->modelFlagBits = (uint8)(1 << (defIdx & 0x1f)) | obj->modelFlagBits;
        }
        if (defIdx != 0 && def->parentBoneName != 0 && *def->parentBoneName != '\0') {
            int nameId = ScrStr_LookupCString(def->parentBoneName);
            if (nameId != 0) {
                int k;
                for (k = 0; k < modelCount; k++) {
                    int found = XModel_GetBoneIndex(obj->models[k], nameId);
                    if (-1 < found) {
                        char foundByte = (char)found;
                        obj->parentBone[modelCount] = foundByte + (char)obj->boneBase[k];
                        goto parent_resolved;
                    }
                }
            }
            Com_Printf("WARNING: Part '%s' not found in model '%s' or any of its descendants\n",
                        def->parentBoneName, obj->models[0]->name);
        }
parent_resolved:
        newTotal = totalBones;
        if (model != 0) {
            if (0x7f < model->parts->numBones + totalBones) {
                Com_Error(1, "\025dobj for xmodel '%s' has more than %d bones", obj->models[0]->name, 0x7f);
                break;
            }
            newTotal = model->parts->numBones + totalBones;
        }
        totalBones = newTotal;
        modelCount = modelCount + 1;
        defIdx = defIdx + 1;
        def = def + 1;
    }
    obj->numModels = (uint8)modelCount;
    obj->numBones = (uint8)totalBones;
    DObj_ComputeBounds(obj);
}

// 0x080ba776  size=123  FUN_080ba776  status=translated  naming=inferred
// Naming evidence: inverse of DObj_Create/DObj_BuildDuplicatePartMap: clears the anim tree and releases the interned duplicate table unless it is the shared empty one (DObjFree-style).
void DObj_Free(DObj *obj)
{
    if (obj->tree != 0) {
        obj->animTreeData = 0;
        obj->tree = 0;
    }
    if (obj->dupStringId != 0) {
        if ((int)obj->dupStringId != g_dobjEmptyDupPartTable) {
            char *dup = (char *)ScrStr_TextFromHandle(obj->dupStringId);
            int length = ScrStr_Strlen(dup + 0x10);
            ScrStr_DecRefOrFree(obj->dupStringId, length + 0x11);
        }
        obj->dupStringId = 0;
    }
}

// 0x080ba958  size=33  FUN_080ba958  status=translated  naming=inferred
// Naming evidence: bytes needed for a DObjSkel: 0x30 header + 0x20 per bone (DObjGetAllocSkelSize-style).
int DObj_GetAllocSkelSize(const DObj *obj)
{
    return (unsigned)obj->numBones * 0x20 + 0x30;
}

// 0x080ba97a  size=57  FUN_080ba97a  status=translated  naming=inferred
// Naming evidence: returns whether a skeleton exists for the given time; clears obj->skel when the stored time differs.
bool DObj_SkelExists(DObj *obj, int time)
{
    bool exists;

    if (obj->skelTime == time) {
        exists = obj->skel != 0;
    } else {
        obj->skel = 0;
        exists = false;
    }
    return exists;
}

// 0x080ba9b4  size=45  FUN_080ba9b4  status=translated  naming=inferred
// Naming evidence: read-only variant of DObj_SkelExists (does not clear the skeleton).
int DObj_SkelMatchesTime(const DObj *obj, int time)
{
    int result = 0;

    if (obj->skelTime == time && obj->skel != 0) {
        result = 1;
    }
    return result;
}

// 0x080ba9e2  size=25  FUN_080ba9e2  status=translated  naming=inferred
// Naming evidence: clears skeleton pointer and time.
void DObj_ClearSkel(DObj *obj)
{
    obj->skelTime = 0;
    obj->skel = 0;
}

// 0x080ba9fc  size=95  FUN_080ba9fc  status=translated  naming=inferred
// Naming evidence: attaches caller-provided skeleton memory with its time stamp and zeroes the three 128-bit masks (DObjCreateSkel-style).
void DObj_CreateSkel(DObj *obj, DObjSkel *skel, int time)
{
    int i;

    obj->skel = skel;
    obj->skelTime = time;
    for (i = 0; i < 4; i++) {
        skel->setBits[i] = 0;
        skel->controlBits[i] = 0;
        skel->lockedBits[i] = 0;
    }
}

// 0x080baa5c  size=12  FUN_080baa5c  status=translated  naming=inferred
// Naming evidence: returns DObj.numModels.
uint8 DObj_GetNumModels(const DObj *obj)
{
    return obj->numModels;
}

// 0x080baa68  size=15  FUN_080baa68  status=translated  naming=inferred
// Naming evidence: returns DObj.models[index].
XModel *DObj_GetModel(const DObj *obj, int index)
{
    return obj->models[index];
}

// 0x080baa78  size=50  FUN_080baa78  status=translated  naming=inferred
// Naming evidence: copies DObj.mins/maxs out.
void DObj_GetBounds(const DObj *obj, float *mins, float *maxs)
{
    Vec3_Copy_080bc104(obj->mins, mins);
    Vec3_Copy_080bc104(obj->maxs, maxs);
}

// 0x080baaaa  size=41  FUN_080baaaa  status=translated  naming=inferred
// Naming evidence: returns the skeleton's bone array (skel+0x30) or 0 (DObjGetRotTransArray-style).
DObjBone *DObj_GetRotTransArray(const DObj *obj)
{
    if (obj->skel == 0) {
        return 0;
    }
    return obj->skel->bones;
}

// 0x080baad4  size=18  FUN_080baad4  status=translated  naming=inferred
// Naming evidence: returns the first bone index of the given model (DObj.boneBase[]).
uint8 DObj_GetModelBoneBase(const DObj *obj, int modelIndex)
{
    return obj->boneBase[modelIndex];
}

// 0x080baae6  size=125  FUN_080baae6  status=translated  naming=inferred
// Naming evidence: fills out[] with the address of every bone's hit box across all models.
void DObj_GetHitBoxes(const DObj *obj, XBoneInfo **out)
{
    int m;
    int j;

    for (m = 0; m < (int)(unsigned)obj->numModels; m++) {
        XModel *model = obj->models[m];
        int16 numBones = model->parts->numBones;

        for (j = 0; j < numBones; j++) {
            *out = model->hitBoxes + j;
            out = out + 1;
        }
    }
}

// 0x080bab64  size=138  FUN_080bab64  status=translated  naming=inferred
// Naming evidence: marks bone in the skeleton's setBits when requested in partBits and not yet locked (DObjSetRotTransIndex-style).
int DObj_SetRotTransIndex(DObj *obj, const uint32 *partBits, int boneIndex)
{
    int word = boneIndex >> 5;
    uint32 bit = 1u << (boneIndex & 0x1f);

    if ((bit & partBits[word]) == 0) {
        return 0;
    }
    if ((bit & obj->skel->lockedBits[word]) == 0) {
        obj->skel->setBits[word] = bit | obj->skel->setBits[word];
        return 1;
    }
    return 0;
}

// 0x080babee  size=161  FUN_080babee  status=translated  naming=inferred
// Naming evidence: like DObj_SetRotTransIndex but also sets controlBits (DObjSetControlRotTransIndex-style).
int DObj_SetControlRotTransIndex(DObj *obj, const uint32 *partBits, int boneIndex)
{
    int word = boneIndex >> 5;
    uint32 bit = 1u << (boneIndex & 0x1f);
    DObjSkel *skel;

    if ((bit & partBits[word]) == 0) {
        return 0;
    }
    skel = obj->skel;
    if ((bit & skel->lockedBits[word]) == 0) {
        skel->controlBits[word] = bit | skel->controlBits[word];
        skel->setBits[word] = bit | skel->setBits[word];
        return 1;
    }
    return 0;
}

// 0x080bac90  size=126  FUN_080bac90  status=translated  naming=inferred
// Naming evidence: totals numSurfs of the selected lod of each model (lods[] signed bytes, negative = skipped); iterates models from last to first.
int DObj_GetNumSurfaces(const DObj *obj, const char *lods)
{
    int total = 0;
    int i = (int)(unsigned)obj->numModels;

    while (i = i - 1, -1 < i) {
        if (-1 < lods[i]) {
            XModelLod *lod = &obj->models[i]->lods[(int)lods[i]];
            if (lod->surfs != 0) {
                total = total + lod->numSurfs;
            }
        }
    }
    return total;
}

// 0x080bad0e  size=50  FUN_080bad0e  status=translated  naming=inferred
// Naming evidence: returns surfs->surfIds[surfaceIndex] of the model's lod.
int DObj_GetSurfaceId(const DObj *obj, int modelIndex, int surfaceIndex, int lodIndex)
{
    return obj->models[modelIndex]->lods[lodIndex].surfs->surfIds[surfaceIndex];
}

// 0x080bad40  size=86  FUN_080bad40  status=translated  naming=inferred
// Naming evidence: string "DEFAULT"; material name string id per surface, 0 maps to "DEFAULT".
const char *DObj_GetMaterialName(const DObj *obj, int modelIndex, int surfaceIndex, int lodIndex)
{
    int16 nameId = (int16)obj->models[modelIndex]->lods[lodIndex].materialNames[surfaceIndex];

    if (nameId == 0) {
        return "DEFAULT";
    }
    return ScrStr_TextFromHandle(nameId);
}

// 0x080bad96  size=824  FUN_080bad96  status=translated  naming=inferred
// Naming evidence: strings "ERROR: models with more than %i total surfaces", "Max surfs exceeded - see console for details"; emits (model,surface) pairs and merges per-model surface bits into partBits (DObjGetSurfaces-style).
int DObj_GetSurfaces(const DObj *obj, int16 *surfInfo, uint32 *partBits, const char *lods)
{
    unsigned numModels;
    int total;
    int m;

    memset(partBits, 0, 0x10);
    numModels = obj->numModels;
    total = 0;
    for (m = 0; m < (int)numModels; m++) {
        if (-1 < lods[m]) {
            XModel *model = obj->models[m];
            int lodIndex = lods[m];
            XModelLodSurfs *surfs = model->lods[lodIndex].surfs;

            if (surfs != 0) {
                int lastWord = (model->parts->numBones - 1) >> 5;
                int numSurfs = (int)model->lods[lodIndex].numSurfs;
                int s;
                unsigned base;
                int shift;
                int word;
                const uint32 *bits;
                int k;

                if (0x40 < numSurfs + total) {
                    int e;

                    Com_Printf("ERROR: models with more than %i total surfaces\n", 0x40);
                    for (e = 0; e < (int)numModels; e++) {
                        int scratchA[1];
                        int scratchB[1];
                        int surfCount;
                        const char *modelName;

                        model = DObj_GetModel(obj, e);
                        surfCount = XModel_GetLodSurfaces(model, (unsigned int *)scratchA, (int)lods[e], (unsigned int **)scratchB);
                        modelName = XModel_GetName(model);
                        Com_Printf("  model '%s' lod %i has %i surfaces\n", modelName, (int)lods[e], surfCount);
                    }
                    Com_Error(1, "Max surfs exceeded - see console for details");
                }
                for (s = 0; s < numSurfs; s++) {
                    surfInfo[1 + total * 2] = (int16)s;
                    surfInfo[total * 2] = (int16)m;
                    total = total + 1;
                }
                base = obj->boneBase[m];
                bits = (const uint32 *)surfs->partBits;
                shift = (int)(base & 0x1f);
                word = (int)base >> 5;
                if ((base & 0x1f) == 0) {
                    for (k = 0; k <= lastWord; k++) {
                        partBits[word] = bits[k] | partBits[word];
                        word = word + 1;
                    }
                } else {
                    int invShift = 0x20 - shift;

                    partBits[word] = bits[0] << (shift & 0xff) | partBits[word];
                    k = 0;
                    for (;;) {
                        word = word + 1;
                        if (lastWord <= k) {
                            break;
                        }
                        partBits[word] = bits[k + 1] << (shift & 0xff) | bits[k] >> (invShift & 0x1f) | partBits[word];
                        k = k + 1;
                    }
                    partBits[word] = bits[k] >> (invShift & 0x1f) | partBits[word];
                }
            }
        }
    }
    return total;
}

// 0x080bb0ce  size=32  FUN_080bb0ce  status=translated  naming=inferred
// Naming evidence: thin wrapper over DObj_GetBoneIndex.
int DObj_GetBoneIndexByName(const DObj *obj, int boneNameId)
{
    int index = DObj_GetBoneIndex(obj, boneNameId);
    return index;
}

// 0x080bb0ee  size=159  FUN_080bb0ee  status=translated  naming=inferred
// Naming evidence: finds the model owning boneIndex and converts its bone-name string id to text (used by DObj_DumpInfo "Bone %d" listing); returns 0 when out of range.
const char *DObj_GetBoneName(const DObj *obj, int boneIndex)
{
    int base = 0;
    int i = 0;

    for (;;) {
        XModelParts *parts;
        int numBones;

        if ((int)(unsigned)obj->numModels <= i) {
            return 0;
        }
        parts = obj->models[i]->parts;
        numBones = (int)parts->numBones;
        if (boneIndex - base < numBones) {
            return ScrStr_TextFromHandle(parts->hier->boneNames[boneIndex - base]);
        }
        base = base + numBones;
        i = i + 1;
    }
}

// 0x080bb18e  size=10  FUN_080bb18e  status=translated  naming=unknown
// Naming evidence: returns the first dword of a DObj, i.e. its anim tree pointer.
XAnimTree *DObj_GetTree(const DObj *obj)
{
    return obj->tree;
}

// 0x080bb198  size=75  FUN_080bb198  status=translated  naming=unknown
// Naming evidence: true when XModel_IsDefault is non-zero for any model (last to first); property meaning of XModel_IsDefault unknown.
int DObj_AnyModelHasProperty(const DObj *obj)
{
    int i = (int)(unsigned)obj->numModels;

    do {
        i = i - 1;
        if (i < 0) {
            return 0;
        }
    } while (XModel_IsDefault(obj->models[i]) == 0);
    return 1;
}

// 0x080bb1e4  size=12  FUN_080bb1e4  status=translated  naming=inferred
// Naming evidence: returns DObj.numBones.
uint8 DObj_GetNumBones(const DObj *obj)
{
    return obj->numBones;
}

// 0x080bb1f0  size=141  FUN_080bb1f0  status=translated  naming=inferred
// Naming evidence: (point - bone.origin) projected on the three rows of the bone's rotation matrix from QuatTransform_ToMat3.
void DObj_WorldPointToBoneLocal(const float *point, const DObjBone *bone, float *out)
{
    float m[9];
    float d[3];

    Vec3_Sub_080bc178(point, bone->origin, d);
    QuatTransform_ToMat3((const QuatTransform_s *)bone, m);
    out[0] = d[2] * m[2] + d[1] * m[1] + d[0] * m[0];
    out[1] = d[2] * m[5] + d[1] * m[4] + d[0] * m[3];
    out[2] = d[2] * m[8] + d[1] * m[7] + d[0] * m[6];
}

// 0x080bb27e  size=2228  FUN_080bb27e  status=translated  naming=inferred
// Naming evidence: segment trace against per-bone hit boxes/spheres filtered by a per-part-group priority table (DObjTraceline-style); writes fraction, normal, hit bone name id and part group.
void DObj_Traceline(DObj *obj, const float *start, const float *end, const uint8 *partPriority, DObjTraceResult *res)
{
    uint16 groupOfBone[131];
    float dir[3];
    float lenSq;
    DObjBone *bones;
    float invLenSq;
    unsigned threshold;
    const uint8 *pairs;
    int boneIdx;
    int hitAxis;
    int bestAxis;
    float bestSign;
    DObjBone *bestBone;
    float initialFraction;
    int m;
    long double lv;

    res->field04 = 0.0f;
    res->partName = 0;
    res->partGroup = 0;
    Vec3_Clear_080bc0b6(res->normal);
    Vec3_Sub_080bc178(end, start, dir);
    lv = Vec3_LengthSq_080bc27c(dir);
    lenSq = (float)lv;
    if (!(lenSq != 0.0f)) {
        return;
    }
    bones = DObj_GetRotTransArray(obj);
    if (bones == 0) {
        return;
    }
    invLenSq = (float)(1.0 / lenSq);
    threshold = 2;
    {
        char *dupData = (char *)ScrStr_TextFromHandle(obj->dupStringId);
        pairs = (const uint8 *)(dupData + 0x10);
    }
    boneIdx = 0;
    hitAxis = -1;
    bestAxis = -1;
    bestSign = 0.0f;
    bestBone = 0;
    initialFraction = res->fraction;
    for (m = 0; m < (int)(unsigned)obj->numModels; m++) {
        XModel *model = obj->models[m];
        XModelParts *parts = model->parts;
        XModelHier *hier = parts->hier;
        uint16 *boneNames = hier->boneNames;
        int numBones = (int)parts->numBones;
        unsigned flagged = 1u << (m & 0x1f) & (unsigned)obj->modelFlagBits;
        int b;
        const uint8 *pbVar1;

        for (b = 0; pbVar1 = pairs, b < numBones; b++) {
            uint16 group = (uint16)parts->boneGroup[b];
            unsigned priority = (unsigned)partPriority[group];
            XBoneInfo *hb;

            if (boneIdx == *pairs - 1) {
                pairs = pairs + 2;
                if (priority == 1) {
                    group = groupOfBone[pbVar1[1]];
                    priority = (unsigned)partPriority[group];
                }
            } else if (priority == 1) {
                if (b < parts->numRootBones) {
                    uint8 parent = obj->parentBone[m];
                    if (parent == 0xff) {
                        groupOfBone[0] = 0;
                    } else {
                        groupOfBone[0] = groupOfBone[parent + 1];
                    }
                    group = groupOfBone[0];
                } else {
                    group = groupOfBone[(boneIdx - (unsigned)hier->parentDelta[b - parts->numRootBones]) + 1];
                }
                priority = (unsigned)partPriority[group];
            }
            groupOfBone[boneIdx + 1] = group;
            if (flagged == 0) {
                hb = &model->hitBoxes[b];
                if (hb->radiusSq != 0.0f && threshold <= priority) {
                    float center[3];
                    float toStart[3];
                    float toEnd[3];
                    float closest[3];
                    float tClosest;
                    float distSq;
                    float overlap;

                    Vec3_TransformByQuatTransform(hb->offset, (const QuatTransform_s *)bones, center);
                    Vec3_Sub_080bc178(start, center, toStart);
                    lv = Vec3_Dot_080b93c4(toStart, dir);
                    tClosest = (float)(-lv * (long double)invLenSq);
                    if (tClosest < 1.0) {
                        if (0.0 < tClosest) {
                            Vec3_MA_080bc1f4(toStart, tClosest, dir, closest);
                            lv = Vec3_LengthSq_080bc27c(closest);
                            distSq = (float)lv;
                        } else {
                            lv = Vec3_LengthSq_080bc27c(toStart);
                            distSq = (float)lv;
                        }
                    } else {
                        Vec3_Sub_080bc178(end, center, toEnd);
                        lv = Vec3_LengthSq_080bc27c(toEnd);
                        distSq = (float)lv;
                    }
                    overlap = hb->radiusSq - distSq;
                    if (0.0 < overlap &&
                        (threshold != priority || tClosest - sqrtf(overlap * invLenSq) < res->fraction)) {
                        float localStart[7];
                        float localEnd[4];
                        float limit;
                        char allInside;
                        char startInside;
                        float side;
                        const float *box;
                        int k;
                        float tNear;

                        DObj_WorldPointToBoneLocal(start, bones, localStart);
                        DObj_WorldPointToBoneLocal(end, bones, localEnd);
                        tNear = 0.0f;
                        if (threshold == priority) {
                            limit = res->fraction;
                        } else {
                            limit = initialFraction;
                        }
                        allInside = 1;
                        startInside = 1;
                        side = -1.0f;
                        box = hb->mins;
                        for (;;) {
                            for (k = 0; k < 3; k++) {
                                float ds = (localStart[k] - box[k]) * side;
                                float de = (localEnd[k] - box[k]) * side;
                                float span;

                                if (0.0 < ds) {
                                    if (0.0 < de) {
                                        goto next_bone;
                                    }
                                    allInside = 0;
                                    span = ds - de;
                                    if (tNear * span < ds) {
                                        tNear = ds / span;
                                        if (limit <= ds / span) {
                                            goto next_bone;
                                        }
                                        bestSign = side;
                                        hitAxis = k;
                                    }
                                } else if (0.0 < de) {
                                    startInside = 0;
                                    span = ds - de;
                                    if (limit * span < ds) {
                                        limit = ds / span;
                                        if (ds / span <= tNear) {
                                            goto next_bone;
                                        }
                                    }
                                }
                            }
                            if (side == 1.0) {
                                break;
                            }
                            side = 1.0f;
                            box = hb->maxs;
                        }
                        if (allInside == 0) {
                            if (threshold == priority) {
                                if (res->fraction <= tNear) {
                                    goto next_bone;
                                }
                            } else {
                                threshold = priority;
                            }
                            res->fraction = tNear;
                            res->partName = boneNames[b];
                            res->partGroup = group;
                            bestAxis = hitAxis;
                            bestBone = bones;
                        } else if (startInside != 0 && (lv = Vec2_Dot_080bc094(dir, start), lv <= (long double)0)) {
                            res->fraction = 0.0f;
                            res->partName = boneNames[b];
                            res->partGroup = group;
                            if (dir[0] == 0.0f && !(dir[0] != dir[0]) && dir[1] == 0.0f && !(dir[1] != dir[1])) {
                                lv = Float_SignOne(dir[2]);
                                res->normal[2] = -(float)lv;
                                return;
                            }
                            Vec2_Copy_080bc074(start, res->normal);
                            Vec2_Normalize(res->normal);
                            return;
                        }
                    }
                }
            }
next_bone:
            bones = bones + 1;
            boneIdx = boneIdx + 1;
        }
    }
    if (bestBone != 0) {
        float mat[13];

        QuatTransform_ToMat3((const QuatTransform_s *)bestBone, mat);
        Vec3_Scale_080bc1bc(mat + bestAxis * 3, bestSign, res->normal);
    }
}

// 0x080bbb32  size=283  FUN_080bbb32  status=translated  naming=inferred
// Naming evidence: per-model collision trace via XModel_TraceLine over the bone array, advancing 0x20 bytes per bone; reports the hit bone's name id (DObjGeomTraceline-style).
void DObj_GeomTraceline(DObj *obj, const void *a, const void *b, const void *c, DObjTraceResult *res)
{
    ModelTrace trace;
    DObjBone *bones;
    int m;

    res->partName = 0;
    res->partGroup = 0;
    trace.fraction = res->fraction;
    trace.field10 = 0;
    Vec3_Clear_080bc0b6(trace.normal);
    bones = DObj_GetRotTransArray(obj);
    if (bones != 0) {
        for (m = 0; m < (int)(unsigned)obj->numModels; m++) {
            XModel *model = obj->models[m];
            XModelParts *parts = model->parts;
            uint16 *boneNames = parts->hier->boneNames;
            int hit = XModel_TraceLine(model, (XModelTrace *)&trace, bones, (const float *)a, (const float *)b, (uint32)(unsigned long)c);

            if (-1 < hit) {
                res->partName = boneNames[hit];
            }
            bones = bones + parts->numBones;
        }
    }
    res->fraction = trace.fraction;
    res->field04 = trace.field10;
    Vec3_Copy_080bc104(trace.normal, res->normal);
}

// 0x080bbcd0  size=80  FUN_080bbcd0  status=translated  naming=inferred
// Naming evidence: true if any model's XModel_GetContents value intersects the mask (DObjHasContents-style).
int DObj_HasContents(const DObj *obj, unsigned mask)
{
    int i = 0;

    for (;;) {
        unsigned contents;

        if ((int)(unsigned)obj->numModels <= i) {
            return 0;
        }
        contents = XModel_GetContents(obj->models[i]);
        if ((contents & mask) != 0) {
            break;
        }
        i = i + 1;
    }
    return 1;
}

// 0x080bbd20  size=388  FUN_080bbd20  status=translated  naming=inferred
// Naming evidence: writes bone quaternion from Euler angles (degrees * pi/360, i.e. half-angle sin/cos), identity when angles==NULL, quatScale=0 and the origin into the bone slot.
void DObj_SetBoneTransform(DObj *obj, const float *origin, const float *angles, int boneIndex)
{
    DObjBone *bones = DObj_GetRotTransArray(obj);
    DObjBone *bone;
    float sinB, cosB;   /* from angles[1] (local_1c / local_18) */
    float sinA, cosA;   /* from angles[0] (local_2c / local_28) */
    float sinC, cosC;   /* from angles[2] (local_24 / local_20) */
    float *q;

    if (bones == 0) {
        return;
    }
    bone = bones + boneIndex;
    q = bone->quat;
    if (angles == 0) {
        Vec4_Set_080bc2b0(q, 0, 0, 0, 1.0f);
    } else {
        SinCos_Float_080bc046((float)((double)angles[1] * 0.008726646259971648), &sinB, &cosB);
        SinCos_Float_080bc046((float)((double)angles[0] * 0.008726646259971648), &sinA, &cosA);
        SinCos_Float_080bc046((float)((double)angles[2] * 0.008726646259971648), &sinC, &cosC);
        q[0] = cosC * -sinA * sinB + sinC * cosA * cosB;
        q[1] = sinC * cosA * sinB + cosC * sinA * cosB;
        q[2] = cosC * cosA * sinB + -sinC * sinA * cosB;
        q[3] = cosC * cosA * cosB - sinC * -sinA * sinB;
    }
    q[7] = 0.0f;
    Vec3_Copy_080bc104(origin, q + 4);
}

// 0x080bbea4  size=125  FUN_080bbea4  status=translated  naming=inferred
// Naming evidence: look up bone by name, mark it controlled, set a rotation-only transform at the zero origin vec3_origin.
int DObj_SetControlBoneByName(DObj *obj, const uint32 *partBits, int boneNameId, const float *angles)
{
    int index = DObj_GetBoneIndexByName(obj, boneNameId);

    if (index < 0) {
        return 0;
    }
    if (DObj_SetControlRotTransIndex(obj, partBits, index) == 0) {
        return 0;
    }
    DObj_SetBoneTransform(obj, vec3_origin, angles, index);
    return 1;
}

// 0x080bbf22  size=124  FUN_080bbf22  status=translated  naming=inferred
// Naming evidence: look up bone by name, mark it set (not controlled), apply origin+angles transform.
int DObj_SetBoneByName(DObj *obj, const uint32 *partBits, int boneNameId, const float *origin, const float *angles)
{
    int index = DObj_GetBoneIndexByName(obj, boneNameId);

    if (index < 0) {
        return 0;
    }
    if (DObj_SetRotTransIndex(obj, partBits, index) == 0) {
        return 0;
    }
    DObj_SetBoneTransform(obj, origin, angles, index);
    return 1;
}

// 0x080bbf9e  size=88  FUN_080bbf9e  status=translated  naming=inferred
// Naming evidence: index variant of DObj_SetBoneByName.
bool DObj_SetBoneByIndex(DObj *obj, const uint32 *partBits, int boneIndex, const float *origin, const float *angles)
{
    int ok = DObj_SetRotTransIndex(obj, partBits, boneIndex);

    if (ok != 0) {
        DObj_SetBoneTransform(obj, origin, angles, boneIndex);
    }
    return ok != 0;
}

// 0x080bbff6  size=42  FUN_080bbff6  status=translated  naming=unknown
// Naming evidence: binds a single XModel as models[0] slot field (+0x1c), taking numBones from the first byte of parts and the skeleton pointer from parts+0x14; descriptive name only.
void DObj_InitFromModelParts(DObj *obj, XModel *model)
{
    obj->skel = (DObjSkel *)model->parts->staticSkel;
    obj->numBones = *(uint8 *)model->parts;
    obj->models[0] = model;
}

// 0x080bc020  size=37  FUN_080bc020  status=translated  naming=unknown
// Naming evidence: returns +1.0f for x >= 0 and -1.0f otherwise (via Float_SelectNonNegative_080bc2de); descriptive name only.
float Float_SignOne(float x)
{
    return Float_SelectNonNegative_080bc2de(x, 1.0f, -1.0f);
}

// 0x080bc046  size=45  FUN_080bc046  status=translated  naming=unknown
// Naming evidence: sin/cos of a float through the double libm entry points; descriptive name only.
void SinCos_Float_080bc046(float angle, float *outSin, float *outCos)
{
    double v;

    v = sin((double)angle);
    *outSin = (float)v;
    v = cos((double)angle);
    *outCos = (float)v;
}

// 0x080bc074  size=31  FUN_080bc074  status=translated  naming=unknown
// Naming evidence: copies two dwords; descriptive name only.
void Vec2_Copy_080bc074(const float *src, float *dst)
{
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
}

// 0x080bc094  size=33  FUN_080bc094  status=translated  naming=unknown
// Naming evidence: 2-component dot product in extended precision; descriptive name only.
long double Vec2_Dot_080bc094(const float *a, const float *b)
{
    return (long double)a[1] * (long double)b[1] + (long double)a[0] * (long double)b[0];
}

// 0x080bc0b6  size=41  FUN_080bc0b6  status=translated  naming=unknown
// Naming evidence: zeroes three dwords; descriptive name only.
void Vec3_Clear_080bc0b6(float *v)
{
    ((uint32 *)v)[0] = 0;
    ((uint32 *)v)[1] = 0;
    ((uint32 *)v)[2] = 0;
}

// 0x080bc0e0  size=35  FUN_080bc0e0  status=translated  naming=unknown
// Naming evidence: stores three values into a vec3 (same body as the function at 0x080b934c); descriptive name only.
void Vec3_Set_080bc0e0(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// 0x080bc104  size=47  FUN_080bc104  status=translated  naming=unknown
// Naming evidence: copies three dwords; descriptive name only.
void Vec3_Copy_080bc104(const float *src, float *dst)
{
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
    ((uint32 *)dst)[2] = ((const uint32 *)src)[2];
}

// 0x080bc134  size=68  FUN_080bc134  status=translated  naming=unknown
// Naming evidence: componentwise vec3 add; descriptive name only.
void Vec3_Add_080bc134(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x080bc178  size=68  FUN_080bc178  status=translated  naming=unknown
// Naming evidence: componentwise vec3 subtract a - b; descriptive name only.
void Vec3_Sub_080bc178(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080bc1bc  size=56  FUN_080bc1bc  status=translated  naming=unknown
// Naming evidence: vec3 scaled by a scalar; descriptive name only.
void Vec3_Scale_080bc1bc(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x080bc1f4  size=83  FUN_080bc1f4  status=translated  naming=unknown
// Naming evidence: out = a + s*b (same body as the function at 0x080b9370); descriptive name only.
void Vec3_MA_080bc1f4(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x080bc248  size=51  FUN_080bc248  status=translated  naming=unknown
// Naming evidence: vec3 dot product (same body as the function at 0x080b93c4); descriptive name only.
long double Vec3_Dot_b_080bc248(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2] + (long double)a[1] * (long double)b[1] +
           (long double)a[0] * (long double)b[0];
}

// 0x080bc27c  size=51  FUN_080bc27c  status=translated  naming=unknown
// Naming evidence: squared length of a vec3 in extended precision; descriptive name only.
long double Vec3_LengthSq_080bc27c(const float *v)
{
    return (long double)v[2] * (long double)v[2] + (long double)v[1] * (long double)v[1] +
           (long double)v[0] * (long double)v[0];
}

// 0x080bc2b0  size=46  FUN_080bc2b0  status=translated  naming=unknown
// Naming evidence: stores four values into a vec4; descriptive name only.
void Vec4_Set_080bc2b0(float *v, float x, float y, float z, float w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

// 0x080bc2de  size=41  FUN_080bc2de  status=translated  naming=unknown
// Naming evidence: returns ifNonNeg when 0.0 <= x, else ifNeg; descriptive name only.
float Float_SelectNonNegative_080bc2de(float x, float ifNonNeg, float ifNeg)
{
    float result;

    if (0.0 <= x) {
        result = ifNonNeg;
    } else {
        result = ifNeg;
    }
    return result;
}

// 0x080bc308  size=69  FUN_080bc308  status=translated  naming=unknown
// Naming evidence: squared length of a vec4 in extended precision; descriptive name only.
long double Vec4_LengthSq_080bc308(const float *v)
{
    return (long double)v[3] * (long double)v[3] + (long double)v[2] * (long double)v[2] +
           (long double)v[1] * (long double)v[1] + (long double)v[0] * (long double)v[0];
}

// 0x080bc34e  size=79  FUN_080bc34e  status=translated  naming=inferred
// Naming evidence: bone.quatScale = 2/|q|^2 (the factor used to build a rotation matrix from a non-unit quaternion); a zero-length quaternion becomes identity (quat[3]=1) with scale 2.
void Quat_UpdateScale(DObjBone *bone)
{
    float lenSq;
    long double lv;

    lv = Vec4_LengthSq_080bc308(bone->quat);
    lenSq = (float)lv;
    if (lenSq != 0.0f || lenSq != lenSq) {
        bone->quatScale = (float)(2.0 / lenSq);
    } else {
        bone->quat[3] = 1.0f;
        bone->quatScale = 2.0f;
    }
}

// 0x080bc39e  size=179  FUN_080bc39e  status=translated  naming=inferred
// Naming evidence: rotates a point by the parent bone's quaternion matrix (QuatTransform_ToMat3) and adds the parent bone origin.
void Bone_TransformPointByParent(float *p, const DObjBone *parent)
{
    float m[9];
    float oldZ, oldX, oldZ2, oy;

    QuatTransform_ToMat3((const QuatTransform_s *)parent, m);
    oldZ = p[2];
    oy = parent->origin[1];
    oldX = p[0];
    oldZ2 = p[2];
    p[2] = p[2] * m[8] + p[1] * m[5] + p[0] * m[2] + parent->origin[2];
    p[0] = oldZ * m[6] + p[1] * m[3] + p[0] * m[0] + parent->origin[0];
    p[1] = oldZ2 * m[7] + p[1] * m[4] + oldX * m[1] + oy;
}

// 0x080bc452  size=311  FUN_080bc452  status=translated  naming=inferred
// Naming evidence: Hamilton product q1 = q1 * q2 on (x,y,z,w) quaternions, in place.
void Quat_MultiplyInPlace(float *q1, const float *q2)
{
    float bw = q2[3];
    float aw = q1[3];
    float bx = q2[0];
    float by = q2[1];
    float bz = q2[2];
    float ax = q1[0];
    float ay = q1[1];

    q1[3] = ((q1[3] * q2[3] - q1[0] * q2[0]) - q1[1] * q2[1]) - q1[2] * q2[2];
    q1[0] = (q1[2] * by + aw * bx + q1[0] * bw) - q1[1] * bz;
    q1[1] = ax * bz + aw * by + (q1[1] * bw - q1[2] * bx);
    q1[2] = aw * bz + ((ay * bx + q1[2] * bw) - ax * by);
}
