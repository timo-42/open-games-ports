#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_056_types.h"

#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/*
 * chunk_056: FX (effect) subsystem of cod2_lnxded 1.3, 0x0812b5fe..0x0812e89e.
 *
 * Naming evidence summary: the chunk has no function-name strings. Names come
 * from structure (pool block geometry, template registry, frustum plane set),
 * from the FX key dispatcher at 0x08130248 (outside this chunk) that calls the
 * field parsers 0x0812d.. and 0x0812e.., and from the FX diagnostic strings
 * ("fx/%s.efx", "FxScheduler: ...", "FX Error ..."). Out-of-chunk callees are
 * placeholders fn_08xxxxxx (symbols.json was not present).
 */

/* ------------------------------------------------------------------ */
/* Prelude: forward declarations of this chunk's functions.            */
/* ------------------------------------------------------------------ */
void FX_SpinAxes(fx_primitive_s *prim, fx_xform_s *xform, const struct fx_source_s *src);
void FX_DispatchPrimitive(fx_draw_ctx_s *ctx, fx_xform_s *xform, uint32 a3, uint32 a4, uint32 a5);
void FX_DrawPrimitive(uint32 unused, struct fx_emitter_s *emitter, fx_primitive_s *prim, uint32 a4, uint32 a5, const struct fx_source_s *src, uint32 a7, uint32 a8);
void FX_SerializePlaybackList(FxScheduler *list, struct fx_stream_s *st);
void FX_SerializePlayback(FxScheduledEffect *rec, struct fx_stream_s *st);
void FX_ResetTemplateCount(void);
FxEffectTemplate *FX_FindTemplate(const char *name);
int FX_RegisterTemplate(FxEffectTemplate *tpl);
FxEffectTemplate *FX_LoadTemplate(const char *name);
void FX_ReleaseTemplatesExcept(FxEffectTemplate *keep);
void FX_OnEvent(int type, int value);
float FX_Fabs(float v);
float FX_ClampF(float v, float lo, float hi);
void Vec3_Clear_0812bdf8(float *v);
void Vec3_Set_0812be22(float *v, float x, float y, float z);
void Vec3_Copy_0812be46(const float *src, float *dst);
void Vec3_Sub_0812be76(const float *a, const float *b, float *out);
void Vec3_Scale_0812beba(const float *v, float s, float *out);
void Vec3_MultAdd_0812bef2(const float *base, float scale, const float *dir, float *out);
void Vec3_Lerp(const float *a, const float *b, float t, float *out);
void FX_ResetAll(void);
void FX_DrawCtxInit(fx_draw_ctx_s *ctx);
void FX_DrawCtxFree(fx_draw_ctx_s *ctx);
void *FX_PlaybackAlloc(uint32 size);
void FX_PlaybackFree(void *rec);
void FX_PoolInitSmall(FxPool *pool);
void FX_PoolInitLarge(FxPool *pool);
void *FX_PoolAlloc(FxPool *pool, uint32 elemSize);
void FX_PoolBlockMoveToFront(FxPool *pool, FxPoolBlock *block);
void FX_PoolFree(FxPool *pool, void *elem);
void *FX_Alloc(uint32 size);
void FX_TemplateReleasePrims(FxEffectTemplate *tpl);
int FX_LoadEffectFile(ParseNode *tree, const char *name);
void FX_LoadMissingEffect(void);
FxEffectTemplate *FX_GetEffectByName(const char *name);
void FX_TemplateAddPrim(FxEffectTemplate *tpl, fx_primitive_s *prim);
int FX_PrimHasMaterials(const fx_primitive_s *prim);
int FX_PrimValidate(fx_primitive_s *prim);
FxEffectTemplate *FX_BuildTemplate(ParseNode *root, const char *name);
void FX_PtrListFree(fx_ptr_list_s *list);
void FX_PtrListPush(fx_ptr_list_s *list, int value);
void FX_PtrListAdd(fx_ptr_list_s *list, int value);
void FX_CopyEffectName(const char *src, char *dst);
int FX_StrNCaseCmp(const char *a, const char *b, int n);
char *ConfigBlock_GetName(ParseNode *node);
ParseNode *ConfigNode_GetNext(ParseNode *node);
ParseNode *ConfigBlock_GetFirstChild(ParseNode *node);
ParseNode *ConfigNode_Self(ParseNode *node);
void FX_ClearFour(int *p);
void FX_ClearFourB(int *p);
void FX_ClockReset(fx_clock_s *clock);
void FX_ClockAdvance(fx_clock_s *clock, int now);
void FX_ClockAdvanceFixedStep(fx_clock_s *clock, int now);
void FX_BuildFrustumPlanes(FxSystem *sc, const fx_view_s *view, float farDelta);
void FX_SetupFrustum(FxSystem *sc, const fx_view_s *view, float farDelta);
int FX_IsCulledByPlanes(uint32 unused, const float *point, float radius, int count);
int FX_IsCulledByPlanes2(uint32 unused, const float *point, float radius);
int FX_IsSegmentCulled(uint32 unused, const float *a, const float *b, float marginA, float marginB, int count);
void FX_InitSampleRecord(struct FxSystem *sc, float *rec, const float *start, const float *mins, const float *maxs, const float *end, int a7, uint32 mask);
void FX_RendererSubmit_Stub(void);
void FX_NoopB(void);
int FX_ScriptStringToText_Stub(void);
void FX_SerializeSlot(int *slot, struct fx_stream_s *st);
int FX_GetSpriteFrameCount_Stub(void);
int FX_IsEffectFlagged_Stub(void);
void FX_SubmitLight_Stub(void);
void FX_NoopD(void);
void FX_NoopE(void);
uint32 FX_RandomStep(FxSystem *sc);
void FX_SinCos(float angle, float *s, float *c);
void FX_Vec3Copy(const float *src, float *dst);
void FX_Vec3Negate(const float *src, float *dst);
void Vec3_ScaleDup(const float *v, float s, float *out);
void Vec3_MultAddDup(const float *base, float scale, const float *dir, float *out);
float Vec3_Dot_0812d3f0(const float *a, const float *b);
void FX_PrimSetDefaults(fx_primitive_s *prim);
void FX_PrimReleaseLists(fx_primitive_s *prim);
void FX_ChannelTableInit(uint32 unused, FxChannelInit *channels);
void FX_PrimBuildCurves(fx_primitive_s *prim, FxChannelInit *channels);
int FX_ParseVec2(uint32 unused, const char *text, float *a, float *b);
int FX_ParseVec3Pair(uint32 unused, const char *text, float *a, float *b);
uint8 FX_ParseWaveFlags(uint32 unused, const char *text, uint32 *flags);
int FX_PrimParseMin(fx_primitive_s *prim, const char *text);
int FX_PrimParseMax(fx_primitive_s *prim, const char *text);
int FX_PrimParseLife(fx_primitive_s *prim, const char *text);
int FX_PrimParseSpawnRange(fx_primitive_s *prim, const char *text);
int FX_PrimParseDelay(fx_primitive_s *prim, const char *text);
int FX_PrimParseCount(fx_primitive_s *prim, const char *text);
int FX_PrimParseIntensity(fx_primitive_s *prim, const char *text);
int FX_PrimParseOrigin(fx_primitive_s *prim, const char *text);
int FX_PrimParseOrigin2(fx_primitive_s *prim, const char *text);
int FX_PrimParseRadius(fx_primitive_s *prim, const char *text);
int FX_PrimParseHeight(fx_primitive_s *prim, const char *text);
int FX_PrimParseWind(fx_primitive_s *prim, const char *text);
int FX_PrimParseRotation(fx_primitive_s *prim, const char *text);
float FX_MaxAbsFloor(uint32 unused, float a, float b);
void FX_BuildEnvelopeCurve(fx_primitive_s *prim, float start, float divisor, float rate, int slot, float weight);
void FX_BuildCurveRatio(fx_primitive_s *prim, float a, float b, int slot, float weight);
void FX_BuildCurveRate(fx_primitive_s *prim, float a, float b, float c, int slot, float weight);
int FX_PrimParseRotationDelta(fx_primitive_s *prim, const char *text);

/* ------------------------------------------------------------------ */
/* Static data owned by this chunk's code.                             */
/* ------------------------------------------------------------------ */

/* Segment type names in dispatch order: segment type = index + 1 (0x0812c4f2). */
static const char *const kFxSegmentTypeNames[12] = {
    "particle", "line", "tail", "cylinder", "emitter", "decal",
    "orientedparticle", "fxrunner", "light", "cameraShake", "flash", "cloud"
};

/* 128-byte rodata block at 0x0815fd76 copied by 0x0812dc70 (word slots of 32 bytes;
 * slot 3 starts with '0', the rest zero). */
static const char kFxWaveWordDefaults[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* ------------------------------------------------------------------ */
/* Primitive dispatch, draw and playback serialisation.                */
/* ------------------------------------------------------------------ */

// 0x0812b5fe  size=114  FUN_0812b5fe  status=translated  naming=inferred
// Evidence: spins the transform axes by a random angle in [0,360) when primitive flags2 bit 0x100 is set; uses RotatePointAroundVector and CrossProduct callees (no string).
void FX_SpinAxes(fx_primitive_s *prim, fx_xform_s *xform, const struct fx_source_s *src)
{
    float angle;
    const float *point;

    if ((prim->flags2 & 0x100) != 0) {
        angle = Rand_Float(0.0f, 360.0f);
        point = (const float *)((const char *)src + 0x0c);
        Vec3_RotateAroundAxis(xform->axis[1], xform->axis[0], point, angle);
        Vec3_Cross(xform->axis[0], xform->axis[1], xform->axis[2]);
    }
}

// 0x0812b670  size=567  FUN_0812b670  status=translated  naming=inferred
// Evidence: switch on the primitive segment type (prim+0x40, values 1..12 in the FX_BuildTemplate name table) choosing one of twelve per-type handlers.
void FX_DispatchPrimitive(fx_draw_ctx_s *ctx, fx_xform_s *xform, uint32 a3, uint32 a4, uint32 a5)
{
    switch (ctx->prim->type) {
    case 1:
        FxSpawner_Spawn(ctx, xform, a3, a4, a5);
        break;
    case 2:
        FxSpawner_SpawnLine(ctx, xform, a3, a4, a5);
        break;
    case 3:
        FxSpawner_SpawnTail(ctx, xform, a3, a4, a5);
        break;
    case 4:
        FxSpawner_SpawnCylinder(ctx, xform, a3, a4, a5);
        break;
    case 5:
        FxSpawner_SpawnEmitter(ctx, xform, a3, a4, a5);
        break;
    case 6:
        FxSpawner_QueueDecalEvent(ctx, xform, a3, a4, a5);
        break;
    case 7:
        FxSpawner_SpawnOrientedParticle(ctx, xform, a3, a4, a5);
        break;
    case 8:
        FxSpawner_QueueFxRunnerEvent(ctx, xform, a3, a4, a5);
        break;
    case 9:
        FxSpawner_SpawnLight(ctx, xform, a3, a4, a5);
        break;
    case 10:
        FxSpawner_QueueCameraShakeEvent(ctx, xform, a3, a4, a5);
        break;
    case 11:
        FxSpawner_SpawnFlash(ctx, xform, a3, a4, a5);
        break;
    case 12:
        FxSpawner_SpawnCloud(ctx, xform, a3, a4, a5);
        break;
    default:
        break;
    }
}

// 0x0812b8a8  size=159  FUN_0812b8a8  status=partial  naming=inferred
// Evidence: builds the draw context (placement transform, spin, cull test) and dispatches to FX_DispatchPrimitive; the C++ EH cleanup landing pad (0x0812b947) is not modelled, only the normal exit path.
void FX_DrawPrimitive(uint32 unused, struct fx_emitter_s *emitter, fx_primitive_s *prim, uint32 a4, uint32 a5, const struct fx_source_s *src, uint32 a7, uint32 a8)
{
    fx_draw_ctx_s ctx;
    char visible;

    FX_DrawCtxInit(&ctx);
    Mat3_Copy(src, &ctx.xform);
    FX_SpinAxes(prim, &ctx.xform, src);
    visible = FxPrim_ResolveBoltRef(prim, a4, &ctx.cull);
    if (visible == '\0') {
        FX_DrawCtxFree(&ctx);
    } else {
        ctx.emitter = emitter;
        ctx.prim = prim;
        FX_DispatchPrimitive(&ctx, &ctx.xform, a5, a7, a8);
        FX_DrawCtxFree(&ctx);
    }
}

// 0x0812b978  size=275  FUN_0812b978  status=translated  naming=inferred
// Evidence: serialises a playback list. Read mode (0x08129e6c nonzero) rebuilds the list and drops invalid records; write mode writes the global list at 0x08a53804. Name follows the record fields (template, prim index).
void FX_SerializePlaybackList(FxScheduler *list, struct fx_stream_s *st)
{
    FxScheduledEffect *node;
    FxScheduledEffect *rec;
    int remaining;

    if (FxArchive_IsReading(st) == 0) {
        /* Quirk preserved: the count written is the argument list's count, the
         * records written come from the global list at 0x08a53804. */
        FxStream_WriteInt(st, list->count);
        for (node = g_fxScheduler->head; node != 0; node = node->next) {
            FX_SerializePlayback(node, st);
        }
    } else {
        list->head = 0;
        list->count = 0;
        for (remaining = FxStream_ReadInt(st); remaining != 0; remaining--) {
            rec = (FxScheduledEffect *)FX_PlaybackAlloc(0x50);
            FX_SerializePlayback(rec, st);
            if (rec->effect == 0 || rec->primIndex < 0 ||
                rec->effect->primCount <= rec->primIndex ||
                rec->effect->prims[rec->primIndex] == 0) {
                FX_PlaybackFree(rec);
            } else {
                rec->next = list->head;
                list->head = rec;
                list->count = list->count + 1;
            }
        }
    }
}

// 0x0812ba8c  size=202  FUN_0812ba8c  status=translated  naming=inferred
// Evidence: field-by-field serialisation of a 0x50-byte playback record (template ref, prim index, 8 timing bytes, origin and three axes); field order fixed by the call sequence.
void FX_SerializePlayback(FxScheduledEffect *rec, struct fx_stream_s *st)
{
    MsgStream_TransferStringRef(st, &rec->effect);
    FxStream_Int(st, &rec->primIndex);
    FxStream_Int(st, &rec->unk08);
    MsgStream_TransferBytes(st, rec->timing, 8);
    FxStream_Vec3(st, rec->origin);
    FxStream_Vec3(st, rec->axis[0]);
    FxStream_Vec3(st, rec->axis[1]);
    FxStream_Vec3(st, rec->axis[2]);
    FxStream_Int(st, &rec->unk44);
}

/* ------------------------------------------------------------------ */
/* Template registry and effect file loading.                          */
/* ------------------------------------------------------------------ */

// 0x0812bb56  size=15  FUN_0812bb56  status=translated  naming=inferred
// Evidence: clears the template registry count (0x08a53800); single store.
void FX_ResetTemplateCount(void)
{
    g_fxTemplateCount = 0;
}

// 0x0812bb66  size=90  FUN_0812bb66  status=translated  naming=inferred
// Evidence: linear search of the 256-entry registry comparing template name (+0) with strcmp; returns NULL when absent.
FxEffectTemplate *FX_FindTemplate(const char *name)
{
    int i;
    FxEffectTemplate *tpl;

    i = 0;
    while (1) {
        if ((uint32)i >= g_fxTemplateCount) {
            return 0;
        }
        tpl = g_fxTemplates[i];
        if (strcmp(tpl->name, name) == 0) {
            break;
        }
        i = i + 1;
    }
    return tpl;
}

// 0x0812bbc0  size=81  FUN_0812bbc0  status=translated  naming=inferred
// Evidence: appends to the registry; the overflow message "^1Max effect templates of '%i' exceeded" names the 0x100 capacity.
int FX_RegisterTemplate(FxEffectTemplate *tpl)
{
    int ok;

    ok = (g_fxTemplateCount != 0x100);
    if (ok) {
        g_fxTemplates[g_fxTemplateCount] = tpl;
        g_fxTemplateCount = g_fxTemplateCount + 1;
    } else {
        FxPrintf("^1Max effect templates of '%i' exceeded\n", 0x100);
    }
    return ok;
}

// 0x0812bc12  size=161  FUN_0812bc12  status=partial  naming=inferred
// Evidence: find-or-build wrapper around the effect parser, with a parse-tree RAII object (ctor 0x0813311e, dtor 0x08133196); the EH landing pad at 0x0812bcb3 is not modelled.
FxEffectTemplate *FX_LoadTemplate(const char *name)
{
    ParseNode tree;
    FxEffectTemplate *found;
    FxEffectTemplate *built;

    ConfigTree_Init2(&tree);
    found = FX_FindTemplate(name);
    if (found == 0) {
        built = FX_BuildTemplate(&tree, name);
        if (built == 0) {
            ConfigTree_Destroy2(&tree);
            found = 0;
        } else if (FX_RegisterTemplate(built) == 0) {
            ConfigTree_Destroy2(&tree);
            found = 0;
        } else {
            ConfigTree_Destroy2(&tree);
            found = built;
        }
    } else {
        ConfigTree_Destroy2(&tree);
    }
    return found;
}

// 0x0812bcdc  size=130  FUN_0812bcdc  status=translated  naming=inferred
// Evidence: releases every registry template except the argument (its prims are released, the template kept); if the argument was present it is moved to slot 0 and the count becomes 1, otherwise the count becomes 0.
void FX_ReleaseTemplatesExcept(FxEffectTemplate *keep)
{
    int found;
    int i;

    found = 0;
    for (i = 0; i < (int)g_fxTemplateCount; i++) {
        if (g_fxTemplates[i] == keep) {
            found = 1;
        } else {
            FX_TemplateReleasePrims(g_fxTemplates[i]);
            g_fxTemplates[i] = 0;
        }
    }
    if (found != 0) {
        g_fxTemplates[0] = keep;
    }
    g_fxTemplateCount = (uint32)found;
}

// 0x0812bd5e  size=62  FUN_0812bd5e  status=translated  naming=inferred
// Evidence: on event (type 1, value 0xffff) resets both pools (capacity 0x30 and 0x199) without freeing blocks; called from FX_ResetAll.
void FX_OnEvent(int type, int value)
{
    if (value == 0xffff && type == 1) {
        FX_PoolInitSmall(&g_fxPoolSmall);
    }
    if (value == 0xffff && type == 1) {
        FX_PoolInitLarge(&g_fxPoolLarge);
    }
}

// 0x0812bd9c  size=25  FUN_0812bd9c  status=translated  naming=inferred
// Evidence: single fabs on a float; result is rounded to float before return.
float FX_Fabs(float v)
{
    float r;

    r = fabsf(v);
    return r;
}

// 0x0812bdb6  size=65  FUN_0812bdb6  status=translated  naming=inferred
// Evidence: clamps v to [lo, hi] with lo tested first (v < lo gives lo; hi < v gives hi).
float FX_ClampF(float v, float lo, float hi)
{
    float r;

    if (v < lo) {
        r = lo;
    } else if (hi < v) {
        r = hi;
    } else {
        r = v;
    }
    return r;
}

// 0x0812bdf8  size=41  FUN_0812bdf8  status=translated  naming=inferred
// Evidence: zeroes a three-float vector.
void Vec3_Clear_0812bdf8(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x0812be22  size=35  FUN_0812be22  status=translated  naming=inferred
// Evidence: stores three floats into a vector (x, y, z).
void Vec3_Set_0812be22(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// 0x0812be46  size=47  FUN_0812be46  status=translated  naming=inferred
// Evidence: copies a vector from src to dst.
void Vec3_Copy_0812be46(const float *src, float *dst)
{
    /* Integer moves in the original (bit exact). */
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
    ((uint32 *)dst)[2] = ((const uint32 *)src)[2];
}

// 0x0812be76  size=68  FUN_0812be76  status=translated  naming=inferred
// Evidence: out = a - b, component-wise.
void Vec3_Sub_0812be76(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x0812beba  size=56  FUN_0812beba  status=translated  naming=inferred
// Evidence: out = s * v, component-wise (scalar times vector).
void Vec3_Scale_0812beba(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x0812bef2  size=83  FUN_0812bef2  status=translated  naming=inferred
// Evidence: out = base + scale * dir (VectorMA shape).
void Vec3_MultAdd_0812bef2(const float *base, float scale, const float *dir, float *out)
{
    out[0] = base[0] + scale * dir[0];
    out[1] = base[1] + scale * dir[1];
    out[2] = base[2] + scale * dir[2];
}

// 0x0812bf46  size=106  FUN_0812bf46  status=translated  naming=inferred
// Evidence: out = a + (b - a) * t (linear interpolation).
void Vec3_Lerp(const float *a, const float *b, float t, float *out)
{
    /* Operand order follows the original (fsub, fmul, then faddp with a[i] second), which matters for NaN payloads. */
    out[0] = (b[0] - a[0]) * t + a[0];
    out[1] = (b[1] - a[1]) * t + a[1];
    out[2] = (b[2] - a[2]) * t + a[2];
}

// 0x0812bfb0  size=28  FUN_0812bfb0  status=translated  naming=inferred
// Evidence: calls the reset event (1, 0xffff); a shutdown wrapper.
void FX_ResetAll(void)
{
    FX_OnEvent(1, 0xffff);
}

// 0x0812bfcc  size=22  FUN_0812bfcc  status=translated  naming=inferred
// Evidence: constructs the 8-byte cull state at ctx+0x08 (0x08129bac, dtor 0x08129bd8 in FX_DrawCtxFree).
void FX_DrawCtxInit(fx_draw_ctx_s *ctx)
{
    FxRef_Construct(&ctx->cull);
}

// 0x0812bfe2  size=22  FUN_0812bfe2  status=translated  naming=inferred
// Evidence: destroys the cull state at ctx+0x08.
void FX_DrawCtxFree(fx_draw_ctx_s *ctx)
{
    FxRef_Destruct(&ctx->cull);
}

// 0x0812bff8  size=27  FUN_0812bff8  status=translated  naming=inferred
// Evidence: allocates one playback record from the large pool (0x08a533e8).
void *FX_PlaybackAlloc(uint32 size)
{
    return FX_PoolAlloc(&g_fxPoolLarge, size);
}

// 0x0812c014  size=27  FUN_0812c014  status=translated  naming=inferred
// Evidence: returns a playback record to the large pool.
void FX_PlaybackFree(void *rec)
{
    FX_PoolFree(&g_fxPoolLarge, rec);
}

// 0x0812c030  size=24  FUN_0812c030  status=translated  naming=inferred
// Evidence: pool config reset with capacity 0x30 and an empty block list.
void FX_PoolInitSmall(FxPool *pool)
{
    pool->blockCapacity = 0x30;
    pool->head = 0;
}

// 0x0812c048  size=24  FUN_0812c048  status=translated  naming=inferred
// Evidence: pool config reset with capacity 0x199 (409 elements per block) and an empty block list.
void FX_PoolInitLarge(FxPool *pool)
{
    pool->blockCapacity = 0x199;
    pool->head = 0;
}

// 0x0812c060  size=222  FUN_0812c060  status=translated  naming=inferred
// Evidence: block-pool allocator. Scans blocks for a free element, moves that block to the head, creates a block when none has one, pops the free list and zeroes 0x50 bytes. Block geometry (409 x 0x50 elements, header at 0x7ff0) inferred from this function and 0x0812c186.
void *FX_PoolAlloc(FxPool *pool, uint32 elemSize)
{
    FxPoolBlock *block;
    void *elem;

    block = pool->head;
    while (block != 0) {
        if (block->freeList != 0) {
            if (block != pool->head) {
                FX_PoolBlockMoveToFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->blockCapacity, (int)elemSize);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    elem = block->freeList;
    block->freeList = *(void **)elem;
    block->freeCount = block->freeCount - 1;
    Com_Memset(elem, 0, 0x50);
    return elem;
}

// 0x0812c13e  size=71  FUN_0812c13e  status=translated  naming=inferred
// Evidence: unlinks a block and prepends it to the pool block list.
void FX_PoolBlockMoveToFront(FxPool *pool, FxPoolBlock *block)
{
    FxPool_UnlinkBlock(block);
    block->prev = 0;
    block->next = pool->head;
    pool->head->prev = block;
    pool->head = block;
}

// 0x0812c186  size=152  FUN_0812c186  status=translated  naming=inferred
// Evidence: returns an element to its block (0x081242b2 finds the block and returns the element). Blocks with all elements free are released, except the head with no spare neighbour.
void FX_PoolFree(FxPool *pool, void *elem)
{
    FxPoolBlock *block;

    block = FxPool_FreeElement(elem, 0x50);
    if (pool->head->freeList == 0) {
        FX_PoolBlockMoveToFront(pool, block);
    } else if (pool->blockCapacity <= block->freeCount) {
        if (block == pool->head) {
            if (block->next == 0) {
                return;
            }
            if (block->next->freeCount == 0) {
                return;
            }
            pool->head = block->next;
        }
        FxPool_FreeBlock(block);
    }
}

// 0x0812c220  size=27  FUN_0812c220  status=translated  naming=inferred
// Evidence: allocation wrapper, tag 4, used for templates (0x68), primitives (0x2a4) and name strings; callers rely on zeroed fields.
void *FX_Alloc(uint32 size)
{
    return Hunk_AllocAlign(size, 4);
}

// 0x0812c23c  size=53  FUN_0812c23c  status=translated  naming=inferred
// Evidence: releases the primitive lists of every primitive in a template (the primitives themselves are kept).
void FX_TemplateReleasePrims(FxEffectTemplate *tpl)
{
    int i;

    for (i = 0; i < tpl->primCount; i++) {
        FX_PrimReleaseLists(tpl->prims[i]);
    }
}

// 0x0812c272  size=218  FUN_0812c272  status=translated  naming=inferred
// Evidence: format "fx/%s.efx" and "Effect file load failed: %s: file not found"; loads the file, copies it, parses it into the tree and frees the copy.
int FX_LoadEffectFile(ParseNode *tree, const char *name)
{
    char path[68];
    int fileBuffer;
    int length;
    char *text;
    char *lines[1];
    int ok;

    fileBuffer = 0;
    sprintf(path, "fx/%s.efx", name);
    length = FSH_FOpenFile(path, &fileBuffer, 0);
    ok = (-1 < length);
    if (ok) {
        text = (char *)Hunk_AllocateTempMemory(length + 1);
        FS_Read(text, length, fileBuffer);
        FS_FCloseFile(fileBuffer);
        text[length] = '\0';
        lines[0] = text;
        ConfigTree_Parse(tree, lines, 1, 0);
        Hunk_FreeTempMemory((int *)text);
    } else {
        FxPrintf("Effect file load failed: %s: file not found\n", path);
    }
    return ok;
}

// 0x0812c34c  size=62  FUN_0812c34c  status=translated  naming=inferred
// Evidence: loads the default effect "misc/missing_fx" and reports a fatal error (level 1) when it is missing.
void FX_LoadMissingEffect(void)
{
    g_fxMissingTemplate = FX_LoadTemplate("misc/missing_fx");
    if (g_fxMissingTemplate == 0) {
        Com_Error(1, "^1ERROR: could not load default effect file '%s'", "misc/missing_fx");
    }
}

// 0x0812c38a  size=153  FUN_0812c38a  status=translated  naming=inferred
// Evidence: message "Effect file '%s' must start with fx/."; strips a leading slash, requires the "fx/" prefix (case-insensitive), loads the rest and falls back to the missing-effect template.
FxEffectTemplate *FX_GetEffectByName(const char *name)
{
    char effectName[88];
    FxEffectTemplate *tpl;

    if (*name == '/' || *name == '\\') {
        name = name + 1;
    }
    if (FX_StrNCaseCmp(name, "fx/", 3) == 0) {
        FX_CopyEffectName(name + 3, effectName);
        tpl = FX_LoadTemplate(effectName);
        if (tpl != 0) {
            return tpl;
        }
    } else {
        FxPrintf("Effect file '%s' must start with fx/.\n", name);
    }
    return g_fxMissingTemplate;
}

// 0x0812c424  size=56  FUN_0812c424  status=translated  naming=inferred
// Evidence: appends a primitive to a template; capacity 0x18 and message "FxScheduler:  Error--too many primitives in an effect".
void FX_TemplateAddPrim(FxEffectTemplate *tpl, fx_primitive_s *prim)
{
    if (tpl->primCount < 0x18) {
        tpl->prims[tpl->primCount] = prim;
        tpl->primCount = tpl->primCount + 1;
    } else {
        FxPrintf("FxScheduler:  Error--too many primitives in an effect\n");
    }
}

// 0x0812c45c  size=55  FUN_0812c45c  status=translated  naming=inferred
// Evidence: when the material-requirement flag (0x08188ab0) is set, reports whether the shader list (lists[0], count at +0x6c) is non-empty.
int FX_PrimHasMaterials(const fx_primitive_s *prim)
{
    int r;

    if (g_fxRequireMaterials == 0) {
        r = 1;
    } else if (prim->lists[0].count == 0) {
        r = 0;
    } else {
        r = 1;
    }
    return r;
}

// 0x0812c494  size=93  FUN_0812c494  status=translated  naming=inferred
// Evidence: message "^1FX Error, no materials defined for primitive template of type '%i'"; segment types 1, 3 and 7 require materials.
int FX_PrimValidate(fx_primitive_s *prim)
{
    int type;

    type = prim->type;
    if ((type == 1 || type == 7 || type == 3) && FX_PrimHasMaterials(prim) == 0) {
        FxPrintf("^1FX Error, no materials defined for primitive template of type '%i'\n", type);
        return 0;
    }
    return 1;
}

// 0x0812c4f2  size=838  FUN_0812c4f2  status=translated  naming=inferred
// Evidence: strings "particle", "line", "tail", "cylinder", "emitter", "decal", "orientedparticle", "fxrunner", "light", "cameraShake", "flash", "cloud" (segment types 1..12), "^1FX Error while parsing segment type", "^1FX Error, invalid primitive template for effect". Builds a template from the parse tree. The C++ EH landing pad is not in this function's body (no try regions), so status is translated; the partial mark is for the caller-side RAII contract only.
FxEffectTemplate *FX_BuildTemplate(ParseNode *root, const char *name)
{
    FxEffectTemplate *tpl;
    ParseNode *node;
    fx_primitive_s *prim;
    int index;
    int type;
    int i;

    if (FX_LoadEffectFile(root, name) == 0) {
        return 0;
    }
    tpl = (FxEffectTemplate *)FX_Alloc(0x68);
    tpl->name = (char *)FX_Alloc((uint32)(strlen(name) + 1));
    strcpy(tpl->name, name);

    node = ConfigBlock_GetFirstChild(ConfigNode_Self(root));
    index = 0;
    while (node != 0) {
        type = 0;
        for (i = 0; i < 12; i++) {
            if (strcasecmp(ConfigBlock_GetName(node), kFxSegmentTypeNames[i]) == 0) {
                type = i + 1;
                break;
            }
        }
        if (type != 0) {
            prim = (fx_primitive_s *)FX_Alloc(0x2a4);
            FX_PrimSetDefaults(prim);
            prim->type = type;
            prim->index = index;
            if (prim->Parse(node) == 0) {
                FX_PrimReleaseLists(prim);
                FX_TemplateReleasePrims(tpl);
                FxPrintf("^1FX Error while parsing segment type '%s'\n", ConfigBlock_GetName(node));
                return 0;
            }
            if (FX_PrimValidate(prim) == 0) {
                FX_PrimReleaseLists(prim);
                FX_TemplateReleasePrims(tpl);
                FxPrintf("^1FX Error, invalid primitive template for effect '%s'\n", name);
                return 0;
            }
            FX_TemplateAddPrim(tpl, prim);
        }
        node = ConfigNode_GetNext(node);
        index = index + 1;
    }
    return tpl;
}

/* ------------------------------------------------------------------ */
/* Pointer lists and name helpers.                                     */
/* ------------------------------------------------------------------ */

// 0x0812c838  size=58  FUN_0812c838  status=translated  naming=inferred
// Evidence: frees a pointer list's storage; count and capacity are cleared only when storage existed.
void FX_PtrListFree(fx_ptr_list_s *list)
{
    if (list->items != 0) {
        Com_Free_A(list->items);
        list->items = 0;
        list->count = 0;
        list->capacity = 0;
    }
}

// 0x0812c872  size=185  FUN_0812c872  status=translated  naming=inferred
// Evidence: appends an int to a growable array (initial capacity 4, doubling); capacity is a 16-bit field.
void FX_PtrListPush(fx_ptr_list_s *list, int value)
{
    void *grown;

    if ((short)list->count == (short)list->capacity) {
        if (list->capacity == 0) {
            list->capacity = 4;
        } else {
            list->capacity = (uint16)(list->capacity * 2);
        }
        grown = Com_MallocZeroed((uint32)list->capacity << 2);
        if (list->items != 0) {
            memcpy(grown, list->items, (uint32)list->count << 2);
            Com_Free_A(list->items);
        }
        list->items = (int *)grown;
    }
    list->items[list->count] = value;
    list->count = (uint16)(list->count + 1);
}

// 0x0812c92c  size=32  FUN_0812c92c  status=translated  naming=inferred
// Evidence: thin wrapper around FX_PtrListPush.
void FX_PtrListAdd(fx_ptr_list_s *list, int value)
{
    FX_PtrListPush(list, value);
}

// 0x0812c94c  size=37  FUN_0812c94c  status=translated  naming=inferred
// Evidence: copies an effect name (0x080b7470) and then strips its extension (0x080d6792).
void FX_CopyEffectName(const char *src, char *dst)
{
    COM_StripExtension(src, dst);
    Q_strlwr_EndPtr(dst);
}

// 0x0812c972  size=152  FUN_0812c972  status=translated  naming=inferred
// Evidence: case-insensitive bounded string compare (tolower per char, signed char comparisons); returns -1, 0 or 1 like strncasecmp. Equal-length tail uses a raw byte compare, as in the binary.
int FX_StrNCaseCmp(const char *a, const char *b, int n)
{
    int ca;
    int cb;

    for (;;) {
        if (n == 0 || *a == '\0') {
            return (n == 0 || *a == *b) ? 0 : -1;
        }
        ca = tolower((int)*a);
        cb = tolower((int)*b);
        n = n - 1;
        a = a + 1;
        b = b + 1;
        if ((char)ca < (char)cb) {
            return -1;
        }
        if ((char)ca > (char)cb) {
            return 1;
        }
    }
}

// 0x0812ca0a  size=10  FUN_0812ca0a  status=translated  naming=inferred
// Evidence: parse-node accessor for the name field (offset 0x00).
char *ConfigBlock_GetName(ParseNode *node)
{
    return node->name;
}

// 0x0812ca14  size=11  FUN_0812ca14  status=translated  naming=inferred
// Evidence: parse-node accessor for the sibling link (offset 0x04).
ParseNode *ConfigNode_GetNext(ParseNode *node)
{
    return node->next;
}

// 0x0812ca20  size=11  FUN_0812ca20  status=translated  naming=inferred
// Evidence: parse-node accessor for the first child (offset 0x1c).
ParseNode *ConfigBlock_GetFirstChild(ParseNode *node)
{
    return node->child;
}

// 0x0812ca2c  size=8  FUN_0812ca2c  status=translated  naming=inferred
// Evidence: identity on the tree root; used to pass the root to the builder.
ParseNode *ConfigNode_Self(ParseNode *node)
{
    return node;
}

// 0x0812ca34  size=45  FUN_0812ca34  status=translated  naming=unknown
// Evidence: zeroes four ints starting at +4; no caller in this chunk and the role is not identified.
void FX_ClearFour(int *p)
{
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
    p[4] = 0;
}

// 0x0812ca62  size=45  FUN_0812ca62  status=translated  naming=unknown
// Evidence: byte-identical duplicate of FX_ClearFour at 0x0812ca34; kept as a separate function.
void FX_ClearFourB(int *p)
{
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
    p[4] = 0;
}

// 0x0812ca90  size=67  FUN_0812ca90  status=translated  naming=inferred
// Evidence: resets the clock's time fields and its saved time (offset 0xf4).
void FX_ClockReset(fx_clock_s *clock)
{
    clock->time = 0;
    clock->accumTime = 0;
    clock->prevAccumTime = 0;
    clock->deltaTime = 0;
    clock->unk10 = 0;
    clock->savedTime = 0;
}

// 0x0812cad4  size=194  FUN_0812cad4  status=translated  naming=inferred
// Evidence: advances the clock; delta = now - time clamped to 0..200 ms unless time is paused (0x08a53c98+8). Copies the current 28-int state into the saved block.
void FX_ClockAdvance(fx_clock_s *clock, int now)
{
    int i;

    if (g_08a53c98->timePaused == 0) {
        if (clock->time == 0) {
            clock->deltaTime = 0;
        } else {
            clock->deltaTime = now - clock->time;
            if (clock->deltaTime < 0) {
                clock->deltaTime = 0;
            } else if (200 < clock->deltaTime) {
                clock->deltaTime = 200;
            }
        }
        clock->prevAccumTime = clock->accumTime;
        clock->accumTime = clock->accumTime + clock->deltaTime;
    } else {
        clock->deltaTime = 0;
    }
    clock->time = now;
    clock->savedTime = clock->time;
    for (i = 0; i < 28; i++) {
        clock->savedState[i] = clock->curState[i];
    }
}

// 0x0812cb96  size=268  FUN_0812cb96  status=translated  naming=inferred
// Evidence: advances the clock in fixed 200 ms steps (0x08136ef6(200) plus two follow-up ticks) when the accumulated time lags the new time. The comparison uses accumTime (not time) as in the binary.
void FX_ClockAdvanceFixedStep(fx_clock_s *clock, int now)
{
    int local;

    if (g_08a53c98->timePaused == 0) {
        if (clock->time == 0) {
            clock->deltaTime = 0;
        } else {
            clock->prevAccumTime = clock->accumTime;
            local = now - clock->accumTime;
            if (local < 0) {
                clock->accumTime = now;
            } else {
                clock->deltaTime = 0;
                while (200 < local) {
                    clock->deltaTime = 200;
                    Fx_ProcessPendingEvents();
                    FX_UpdateListB();
                    FX_UpdateListA();
                    clock->prevAccumTime = clock->accumTime;
                    clock->accumTime = clock->accumTime + clock->deltaTime;
                    local = local - 200;
                }
                clock->deltaTime = local;
                Fx_ProcessPendingEvents();
                FX_UpdateListB();
                FX_UpdateListA();
                clock->accumTime = clock->accumTime + clock->deltaTime;
            }
        }
    } else {
        clock->deltaTime = 0;
    }
    clock->time = now;
    clock->savedTime = clock->time;
}

/* ------------------------------------------------------------------ */
/* Frustum planes and culling.                                         */
/* ------------------------------------------------------------------ */

// 0x0812cca2  size=592  FUN_0812cca2  status=translated  naming=inferred
// Evidence: builds five (or six) frustum planes from the view's forward/right/up vectors and the half field-of-view angles (degrees * pi/360, double constant 0.008726646259971648); the sixth (far) plane is the negated forward and is used when farDelta > 0.
void FX_BuildFrustumPlanes(FxSystem *sc, const fx_view_s *view, float farDelta)
{
    float halfAngle;
    float sinA;
    float cosA;
    int k;

    FX_Vec3Copy(view->forward, sc->planes[0].normal);

    halfAngle = (float)(view->fovX * 0.008726646259971648);
    FX_SinCos(halfAngle, &sinA, &cosA);
    Vec3_ScaleDup(view->forward, sinA, sc->planes[1].normal);
    Vec3_MultAddDup(sc->planes[1].normal, cosA, view->right, sc->planes[1].normal);
    Vec3_ScaleDup(view->forward, sinA, sc->planes[2].normal);
    Vec3_MultAddDup(sc->planes[2].normal, -cosA, view->right, sc->planes[2].normal);

    halfAngle = (float)(view->fovY * 0.008726646259971648);
    FX_SinCos(halfAngle, &sinA, &cosA);
    Vec3_ScaleDup(view->forward, sinA, sc->planes[3].normal);
    Vec3_MultAddDup(sc->planes[3].normal, cosA, view->up, sc->planes[3].normal);
    Vec3_ScaleDup(view->forward, sinA, sc->planes[4].normal);
    Vec3_MultAddDup(sc->planes[4].normal, -cosA, view->up, sc->planes[4].normal);

    sc->planeCount = 5;
    if (0.0 < farDelta) {
        FX_Vec3Negate(view->forward, sc->planes[5].normal);
        sc->planeCount = 6;
    }
    for (k = 0; k < sc->planeCount; k++) {
        sc->planes[k].dist = Vec3_Dot_0812d3f0(sc->origin, sc->planes[k].normal);
    }
    if (0.0 < farDelta) {
        sc->planes[5].dist = sc->planes[5].dist - farDelta;
    }
}

// 0x0812cef2  size=162  FUN_0812cef2  status=translated  naming=inferred
// Evidence: copies the view origin, builds the frustum planes and sets the zoom scale tan(40 deg) / tan(fovX/2) (1.0 when fovX == 80). Constants 0.6981317007977318 (40 deg) and 0.017453292 (pi/180) are double literals.
void FX_SetupFrustum(FxSystem *sc, const fx_view_s *view, float farDelta)
{
    double num;
    double den;

    FX_Vec3Copy(view->origin, sc->origin);
    FX_BuildFrustumPlanes(sc, view, farDelta);
    if (view->fovX != 80.0f || view->fovX != view->fovX) {
        num = tan(0.6981317007977318);
        den = tan((double)view->fovX * 0.5 * 0.017453292519943295);
        sc->fovScale = (float)(num / den);
    } else {
        sc->fovScale = 1.0f;
    }
}

// 0x0812cf94  size=128  FUN_0812cf94  status=translated  naming=inferred
// Evidence: sphere-vs-planes test over the first `count` planes of the frustum (dot(normal, point) - dist < -radius means outside). Returns 1 when outside a plane.
int FX_IsCulledByPlanes(uint32 unused, const float *point, float radius, int count)
{
    int k;
    FxSystem *sc;

    sc = g_fxSystem;
    k = 0;
    while (1) {
        if (count <= k) {
            return 0;
        }
        if ((float)(Vec3_Dot_0812d3f0(sc->planes[k].normal, point) - sc->planes[k].dist) < -radius) {
            break;
        }
        k = k + 1;
    }
    return 1;
}

// 0x0812d014  size=141  FUN_0812d014  status=translated  naming=inferred
// Evidence: the same sphere test over the second plane set (planes2 at +0x90, count at +0xf0).
int FX_IsCulledByPlanes2(uint32 unused, const float *point, float radius)
{
    int k;
    FxSystem *sc;

    sc = g_fxSystem;
    k = 0;
    while (1) {
        if (sc->planes2Count <= k) {
            return 0;
        }
        if ((float)(Vec3_Dot_0812d3f0(sc->planes2[k].normal, point) - sc->planes2[k].dist) < -radius) {
            break;
        }
        k = k + 1;
    }
    return 1;
}

// 0x0812d0a2  size=203  FUN_0812d0a2  status=translated  naming=inferred
// Evidence: segment-vs-planes test: returns 1 when some plane has both endpoints behind it by their margins (marginA for a, marginB for b).
int FX_IsSegmentCulled(uint32 unused, const float *a, const float *b, float marginA, float marginB, int count)
{
    int k;
    FxSystem *sc;

    sc = g_fxSystem;
    k = 0;
    while (1) {
        if (count <= k) {
            return 0;
        }
        /* The original tests "not (dist > -margin)" (fucompp + ja), so an unordered (NaN) compare counts as behind the plane. */
        if (!((float)(Vec3_Dot_0812d3f0(sc->planes[k].normal, a) - sc->planes[k].dist) > -marginA) &&
            !((float)(Vec3_Dot_0812d3f0(sc->planes[k].normal, b) - sc->planes[k].dist) > -marginB)) {
            break;
        }
        k = k + 1;
    }
    return 1;
}

// 0x0812d16e  size=113  FUN_0812d16e  status=translated  naming=unknown
// Evidence: calls an out-of-chunk initialiser (0x0805b894) on a float record and stores a 16-bit tag at +0x1c (0x3ff if rec[0] == 1.0, else 0x3fe). Role not identified.
void FX_InitSampleRecord(struct FxSystem *sc, float *rec, const float *start, const float *mins, const float *maxs, const float *end, int a7, uint32 mask)
{
    uint16 tag;

    CM_BoxTrace((struct trace_s *)rec, start, end, mins, maxs, 0, mask);
    if (rec[0] != 1.0f) {
        tag = 0x3fe;
    } else {
        tag = 0x3ff;
    }
    *(uint16 *)((char *)rec + 0x1c) = tag;
}

// 0x0812d1e0  size=5  FUN_0812d1e0  status=translated  naming=unknown
// Evidence: empty function in the binary (return only).
void FX_RendererSubmit_Stub(void)
{
}

// 0x0812d1e6  size=14  FUN_0812d1e6  status=translated  naming=unknown
// Evidence: empty function in the binary (return only).
void FX_NoopB(void)
{
}

// 0x0812d1f4  size=10  FUN_0812d1f4  status=translated  naming=unknown
// Evidence: returns constant 0.
int FX_ScriptStringToText_Stub(void)
{
    return 0;
}

// 0x0812d1fe  size=116  FUN_0812d1fe  status=translated  naming=inferred
// Evidence: serialises a 5-int slot (valid flag at 0, four values at 1..4) and clears the valid flag when reading.
void FX_SerializeSlot(int *slot, struct fx_stream_s *st)
{
    FxStream_Int(st, &slot[1]);
    FxStream_Int(st, &slot[2]);
    FxStream_Int(st, &slot[3]);
    FxStream_Int(st, &slot[4]);
    if (FxArchive_IsReading(st) != 0) {
        slot[0] = 0;
    }
}

// 0x0812d272  size=10  FUN_0812d272  status=translated  naming=unknown
// Evidence: returns constant 0.
int FX_GetSpriteFrameCount_Stub(void)
{
    return 0;
}

// 0x0812d27c  size=10  FUN_0812d27c  status=translated  naming=unknown
// Evidence: returns constant 0.
int FX_IsEffectFlagged_Stub(void)
{
    return 0;
}

// 0x0812d286  size=5  FUN_0812d286  status=translated  naming=unknown
// Evidence: empty function in the binary (return only).
void FX_SubmitLight_Stub(void)
{
}

// 0x0812d28c  size=5  FUN_0812d28c  status=translated  naming=unknown
// Evidence: empty function in the binary (return only).
void FX_NoopD(void)
{
}

// 0x0812d292  size=5  FUN_0812d292  status=translated  naming=unknown
// Evidence: empty function in the binary (return only).
void FX_NoopE(void)
{
}

// 0x0812d298  size=47  FUN_0812d298  status=translated  naming=inferred
// Evidence: returns the scene's random seed (+0xf4) and advances it by the multiplier 0x0369d035 (mod 2^32).
uint32 FX_RandomStep(FxSystem *sc)
{
    uint32 value;

    value = sc->rngSeed;
    sc->rngSeed = sc->rngSeed * 0x0369d035u;
    return value;
}

// 0x0812d2c8  size=45  FUN_0812d2c8  status=translated  naming=inferred
// Evidence: sin and cos of a float angle, each computed in double and stored as float.
void FX_SinCos(float angle, float *s, float *c)
{
    double v;

    v = sin((double)angle);
    *s = (float)v;
    v = cos((double)angle);
    *c = (float)v;
}

// 0x0812d2f6  size=47  FUN_0812d2f6  status=translated  naming=inferred
// Evidence: vector copy; identical to Vec3_Copy_0812be46 (0x0812be46).
void FX_Vec3Copy(const float *src, float *dst)
{
    /* The original copies with integer moves (bit exact, no x87 SNaN quieting). */
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
    ((uint32 *)dst)[2] = ((const uint32 *)src)[2];
}

// 0x0812d326  size=62  FUN_0812d326  status=translated  naming=inferred
// Evidence: vector negation by flipping the sign bit of each component.
void FX_Vec3Negate(const float *src, float *dst)
{
    dst[0] = -src[0];
    dst[1] = -src[1];
    dst[2] = -src[2];
}

// 0x0812d364  size=56  FUN_0812d364  status=translated  naming=inferred
// Evidence: scalar-times-vector; identical to Vec3_Scale_0812beba (0x0812beba).
void Vec3_ScaleDup(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x0812d39c  size=83  FUN_0812d39c  status=translated  naming=inferred
// Evidence: out = base + scale * dir; identical to Vec3_MultAdd_0812bef2 (0x0812bef2).
void Vec3_MultAddDup(const float *base, float scale, const float *dir, float *out)
{
    out[0] = base[0] + scale * dir[0];
    out[1] = base[1] + scale * dir[1];
    out[2] = base[2] + scale * dir[2];
}

// 0x0812d3f0  size=51  FUN_0812d3f0  status=translated  naming=inferred
// Evidence: three-component dot product, accumulated left to right in x87 extended precision and returned unrounded (as the binary does).
float Vec3_Dot_0812d3f0(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/* ------------------------------------------------------------------ */
/* Primitive defaults, curve tables and FX key parsers.                */
/* ------------------------------------------------------------------ */

// 0x0812d424  size=1377  FUN_0812d424  status=translated  naming=inferred
// Evidence: default values for a new primitive (life, count, radius, height = (1,1); curves 0..1 count 3, 2..9 value 1, 10..23 value 0; defaults260 = (10,10),(1,1),(1,1),(1,1); sequence fields 0,1,0,1.0,0,1,0). Field roles from the key dispatcher at 0x08130248.
void FX_PrimSetDefaults(fx_primitive_s *prim)
{
    int k;

    FxRange_Set(prim->life, 1.0f, 1.0f);
    FxRange_Set(prim->count, 1.0f, 1.0f);
    FxRange_Set(prim->radius, 1.0f, 1.0f);
    FxRange_Set(prim->height, 1.0f, 1.0f);

    Curve_CreateRamp(&prim->curves[0], 3, 1.0f, 0);
    Curve_CreateRamp(&prim->curves[1], 3, 1.0f, 0);
    for (k = 2; k <= 9; k++) {
        Curve_CreateRamp(&prim->curves[k], 1, 1.0f, 0);
    }
    for (k = 10; k <= 23; k++) {
        Curve_CreateRamp(&prim->curves[k], 1, 0.0f, 0);
    }

    FxRange_Set(prim->defaults260[2], 1.0f, 1.0f);
    FxRange_Set(prim->defaults260[3], 1.0f, 1.0f);
    FxRange_Set(prim->defaults260[1], 1.0f, 1.0f);
    FxRange_Set(prim->defaults260[0], 10.0f, 10.0f);

    prim->seqStartFrameMode = 0;
    prim->seqFixedFrameValue = 1;
    prim->seqPlayRateMode = 0;
    prim->seqFixedFpsValue = 1.0f;
    prim->seqLoopMode = 0;
    prim->seqLoopTimes = 1;
    prim->spawnFrustumCullRadius = 0.0f;
}

// 0x0812d986  size=80  FUN_0812d986  status=translated  naming=inferred
// Evidence: releases the five pointer lists of a primitive (offsets 0x68..0x88).
void FX_PrimReleaseLists(fx_primitive_s *prim)
{
    FX_PtrListFree(&prim->lists[0]);
    FX_PtrListFree(&prim->lists[1]);
    FX_PtrListFree(&prim->lists[2]);
    FX_PtrListFree(&prim->lists[3]);
    FX_PtrListFree(&prim->lists[4]);
}

// 0x0812d9d6  size=214  FUN_0812d9d6  status=translated  naming=inferred
// Evidence: zeroes the 0x600-byte channel table (24 x 0x40) and sets every vec2 slot used by the channels to (1,1) (extra slot (10,10) is not set here).
void FX_ChannelTableInit(uint32 unused, FxChannelInit *channels)
{
    int k;
    int c;

    memset(channels, 0, 0x600);
    for (k = 0; k != 0x18; k++) {
        for (c = 0; c != 3; c++) {
            FxRange_Set(channels[k].pairA[c], 1.0f, 1.0f);
            FxRange_Set(channels[k].pairB[c], 1.0f, 1.0f);
            FxRange_Set(channels[k].extra, 1.0f, 1.0f);
        }
    }
}

// 0x0812daac  size=173  FUN_0812daac  status=translated  naming=inferred
// Evidence: for each active channel builds one sampled curve, sampling the life range at 0.5 (0x08131600); channel 0 uses 3 samples, others 1; channel 2 passes the flag.
void FX_PrimBuildCurves(fx_primitive_s *prim, FxChannelInit *channels)
{
    int k;
    int count;
    float sample;

    for (k = 0; k != 0x18; k++) {
        if (channels[k].active != 0) {
            count = 1;
            if (k == 0) {
                count = 3;
            }
            sample = FxRange_Lerp(prim->life, 0.5f);
            FxCurve_Build(&channels[k], count, sample, k == 2, &prim->curves[k]);
        }
    }
}

// 0x0812db5a  size=106  FUN_0812db5a  status=translated  naming=inferred
// Evidence: format "%f %f"; a single value is copied to the second output. Returns 1 when at least one number is read.
int FX_ParseVec2(uint32 unused, const char *text, float *a, float *b)
{
    int n;

    if (a == 0 || b == 0) {
        return 0;
    }
    n = sscanf(text, "%f %f", a, b);
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        *b = *a;
    }
    return 1;
}

// 0x0812dbc4  size=171  FUN_0812dbc4  status=translated  naming=inferred
// Evidence: format "%f %f %f   %f %f %f"; accepts 3 numbers (copied to the second vector) or 6; counts 4 and 5 are rejected.
int FX_ParseVec3Pair(uint32 unused, const char *text, float *a, float *b)
{
    int n;

    if (a == 0 || b == 0) {
        return 0;
    }
    n = sscanf(text, "%f %f %f   %f %f %f", &a[0], &a[1], &a[2], &b[0], &b[1], &b[2]);
    if (n < 3 || n == 4 || n == 5) {
        return 0;
    }
    if (n == 3) {
        Vec3_Copy3(a, b);
    }
    return 1;
}

// 0x0812dc70  size=497  FUN_0812dc70  status=translated  naming=inferred
// Evidence: strings "linear", "nonlinear", "wave", "random", "clamp" (flags 1, 4, 8, 2, 0xc). Word slots are initialised from a 128-byte rodata block (0x0815fd76). Unknown words clear the return flag; a short token list returns 1 (binary behaviour).
uint8 FX_ParseWaveFlags(uint32 unused, const char *text, uint32 *flags)
{
    char words[4][32];
    int n;
    int k;
    uint8 ok;

    memcpy(words, kFxWaveWordDefaults, 128);
    ok = 1;
    n = sscanf(text, "%s %s %s %s", words[0], words[1], words[2], words[3]);
    *flags = 0;
    for (k = 0; ; k++) {
        if (k > 3) {
            return ok;
        }
        if (n < k + 1) {
            break;
        }
        if (strcasecmp(words[k], "linear") == 0) {
            *flags = *flags | 1;
        } else if (strcasecmp(words[k], "nonlinear") == 0) {
            *flags = *flags | 4;
        } else if (strcasecmp(words[k], "wave") == 0) {
            *flags = *flags | 8;
        } else if (strcasecmp(words[k], "random") == 0) {
            *flags = *flags | 2;
        } else if (strcasecmp(words[k], "clamp") == 0) {
            *flags = *flags | 0xc;
        } else {
            ok = 0;
        }
    }
    return 1;
}

// 0x0812de62  size=111  FUN_0812de62  status=translated  naming=inferred
// Evidence: key "min": parses a vec3 pair (both halves into one buffer) and stores the first vector as boundsMin (+0xa0); sets flags 0x60.
int FX_PrimParseMin(fx_primitive_s *prim, const char *text)
{
    float buf[6];
    int ok;

    ok = FX_ParseVec3Pair(0, text, buf, buf);
    if (ok == 1) {
        Vec3_Copy3(buf, prim->boundsMin);
        prim->flags = prim->flags | 0x60;
    }
    return ok == 1;
}

// 0x0812ded2  size=111  FUN_0812ded2  status=translated  naming=inferred
// Evidence: key "max": same shape as FX_PrimParseMin, storing boundsMax (+0xac); sets flags 0x60.
int FX_PrimParseMax(fx_primitive_s *prim, const char *text)
{
    float buf[6];
    int ok;

    ok = FX_ParseVec3Pair(0, text, buf, buf);
    if (ok == 1) {
        Vec3_Copy3(buf, prim->boundsMax);
        prim->flags = prim->flags | 0x60;
    }
    return ok == 1;
}

// 0x0812df42  size=91  FUN_0812df42  status=translated  naming=inferred
// Evidence: key "life" (dispatcher 0x08130248): parses a vec2 into prim->life (+0x58).
int FX_PrimParseLife(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok != 0) {
        FxRange_Set(prim->life, a, b);
    }
    return ok != 0;
}

// 0x0812df9e  size=117  FUN_0812df9e  status=translated  naming=inferred
// Evidence: key "spawnRange": parses a vec2 and rejects a reversed pair (b < a) before storing it at +0x60.
int FX_PrimParseSpawnRange(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok == 0) {
        return 0;
    }
    if (b < a) {
        return 0;
    }
    FxRange_Set(prim->spawnRange, a, b);
    return 1;
}

// 0x0812e014  size=91  FUN_0812e014  status=translated  naming=inferred
// Evidence: key "delay": parses a vec2 into +0x48.
int FX_PrimParseDelay(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok != 0) {
        FxRange_Set(prim->delay, a, b);
    }
    return ok != 0;
}

// 0x0812e070  size=91  FUN_0812e070  status=translated  naming=inferred
// Evidence: key "count": parses a vec2 into +0x50.
int FX_PrimParseCount(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok != 0) {
        FxRange_Set(prim->count, a, b);
    }
    return ok != 0;
}

// 0x0812e0cc  size=246  FUN_0812e0cc  status=translated  naming=inferred
// Evidence: keys "bounce" and "intensity": vec2 in [0,1] with a <= b; stored at +0x280 and sets flag 0x20.
int FX_PrimParseIntensity(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok == 0) {
        return 0;
    }
    if (b < a) {
        return 0;
    }
    if (a < 0.0f) {
        return 0;
    }
    if (1.0f < a) {
        return 0;
    }
    if (b < 0.0f) {
        return 0;
    }
    if (1.0f < b) {
        return 0;
    }
    FxRange_Set(prim->intensity, a, b);
    prim->flags = prim->flags | 0x20;
    return 1;
}

// 0x0812e1c2  size=157  FUN_0812e1c2  status=translated  naming=inferred
// Evidence: key "origin": two vec3 values (min and max per axis); each axis pair is stored as a vec2 at +0xb8, +0xc0, +0xc8.
int FX_PrimParseOrigin(fx_primitive_s *prim, const char *text)
{
    float a[3];
    float b[3];
    int ok;

    ok = FX_ParseVec3Pair(0, text, a, b);
    if (ok == 1) {
        FxRange_Set(prim->origin[0], a[0], b[0]);
        FxRange_Set(prim->origin[1], a[1], b[1]);
        FxRange_Set(prim->origin[2], a[2], b[2]);
    }
    return ok == 1;
}

// 0x0812e260  size=157  FUN_0812e260  status=translated  naming=inferred
// Evidence: key "origin2": same layout as FX_PrimParseOrigin, stored at +0xd0, +0xd8, +0xe0.
int FX_PrimParseOrigin2(fx_primitive_s *prim, const char *text)
{
    float a[3];
    float b[3];
    int ok;

    ok = FX_ParseVec3Pair(0, text, a, b);
    if (ok == 1) {
        FxRange_Set(prim->origin2[0], a[0], b[0]);
        FxRange_Set(prim->origin2[1], a[1], b[1]);
        FxRange_Set(prim->origin2[2], a[2], b[2]);
    }
    return ok == 1;
}

// 0x0812e2fe  size=93  FUN_0812e2fe  status=translated  naming=inferred
// Evidence: key "radius": vec2 at +0xe8.
int FX_PrimParseRadius(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok != 0) {
        FxRange_Set(prim->radius, a, b);
    }
    return ok != 0;
}

// 0x0812e35c  size=93  FUN_0812e35c  status=translated  naming=inferred
// Evidence: key "height": vec2 at +0xf0.
int FX_PrimParseHeight(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok != 0) {
        FxRange_Set(prim->height, a, b);
    }
    return ok != 0;
}

// 0x0812e3ba  size=93  FUN_0812e3ba  status=translated  naming=inferred
// Evidence: key "wind": vec2 at +0xf8.
int FX_PrimParseWind(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok != 0) {
        FxRange_Set(prim->wind, a, b);
    }
    return ok != 0;
}

// 0x0812e418  size=93  FUN_0812e418  status=translated  naming=inferred
// Evidence: key "rotation": vec2 at +0x220.
int FX_PrimParseRotation(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok != 0) {
        FxRange_Set(prim->rotation, a, b);
    }
    return ok != 0;
}

// 0x0812e476  size=52  FUN_0812e476  status=translated  naming=inferred
// Evidence: returns max(|a|, b) (the first argument is unused); used by FX_PrimParseRotationDelta.
float FX_MaxAbsFloor(uint32 unused, float a, float b)
{
    float r;

    if (fabsf(a) < b) {
        r = b;
    } else {
        r = fabsf(a);
    }
    return r;
}

// 0x0812e4aa  size=364  FUN_0812e4aa  status=translated  naming=inferred
// Evidence: builds a 20-sample envelope (times i/19, values decaying by the factor 1 - w(rate/20*0.00065), w clamped by 0x0813163c) and stores it as curve slot `slot`, with weight vec2 (weight, weight). Table layout: [t0,v0,t1,v1,...] of 40 floats.
void FX_BuildEnvelopeCurve(fx_primitive_s *prim, float start, float divisor, float rate, int slot, float weight)
{
    float table[40];
    int i;
    float ratio;
    uint32 handle;

    for (i = 0; i <= 19; i++) {
        table[2 * i] = (float)i / 19.0f;
        if (i == 0) {
            if (divisor == 0.0f) {
                table[1] = start;
            } else {
                table[1] = start / divisor;
            }
        } else {
            ratio = FX_ClampSelect((rate / 20.0f) * 0.00065f, 0.0f, 1.0f);
            table[2 * i + 1] = table[2 * i - 1] * (1.0f - ratio);
        }
    }
    handle = (uint32)Curve_Create(table, 1, 20);
    prim->curves[slot].table = (int)handle;
    FxRange_Set(prim->curves[slot].range, weight, weight);
}

// 0x0812e616  size=173  FUN_0812e616  status=translated  naming=inferred
// Evidence: builds a constant two-point curve (0, r), (1, r) with r = a / b (0 when b == 0) into curve slot `slot`, weight vec2 (weight, weight).
void FX_BuildCurveRatio(fx_primitive_s *prim, float a, float b, int slot, float weight)
{
    float table[4];
    uint32 handle;

    table[0] = 0.0f;
    if (b == 0.0f) {
        table[1] = 0.0f;
    } else {
        table[1] = a / b;
    }
    table[2] = 1.0f;
    table[3] = table[1];
    handle = (uint32)Curve_Create(table, 1, 2);
    prim->curves[slot].table = (int)handle;
    FxRange_Set(prim->curves[slot].range, weight, weight);
}

// 0x0812e6c4  size=192  FUN_0812e6c4  status=translated  naming=inferred
// Evidence: builds a two-point ramp (0,0), (1, v) with v = (a * c * 0.001f) / b (0 when b == 0) into curve slot `slot`, weight vec2 (weight, weight).
void FX_BuildCurveRate(fx_primitive_s *prim, float a, float b, float c, int slot, float weight)
{
    float table[4];
    float value;
    uint32 handle;

    table[0] = 0.0f;
    table[1] = 0.0f;
    table[2] = 1.0f;
    if (b == 0.0f) {
        value = 0.0f;
    } else {
        value = (a * c * 0.001f) / b;
    }
    table[3] = value;
    handle = (uint32)Curve_Create(table, 1, 2);
    prim->curves[slot].table = (int)handle;
    FxRange_Set(prim->curves[slot].range, weight, weight);
}

// 0x0812e784  size=282  FUN_0812e784  status=translated  naming=inferred
// Evidence: key "rotationDelta" (dispatcher 0x08130248 string compare through 0x080b7bf8). Parses a vec2 (swapping a reversed pair), builds curve slot 10 from the life x-rate, and when the pair is not equal or NaN also sets flag 0x40000 and builds curve slot 11 from the life y-rate.
int FX_PrimParseRotationDelta(fx_primitive_s *prim, const char *text)
{
    float a;
    float b;
    int ok;
    float r;
    float f;

    ok = FX_ParseVec2(0, text, &a, &b);
    if (ok == 0) {
        return 0;
    }
    if (b < a) {
        Float_Swap(&a, &b);
    }
    r = FX_MaxAbsFloor(0, a, b);
    f = r + r;
    FX_BuildEnvelopeCurve(prim, a, f, prim->life[0], 10, f);
    if (a != b || a != a || b != b) {
        prim->flags = prim->flags | 0x40000;
        FX_BuildEnvelopeCurve(prim, b, f, prim->life[1], 11, 1.0f);
    }
    return 1;
}
