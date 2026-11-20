#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_059_types.h"

/* forward declarations of chunk functions used before their definition */
// 0x08135d72  size=47  FUN_08135d72  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0x258 bytes from pool 0x8a533a8, ctor Line_ConstructBase
void FxSpawner_SpawnLine(struct FxSpawner *ctx, const float *dir, const float *origin)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocLineObj(600);
    Line_ConstructBase((struct Line *)obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    struct fx_xform_s xf;
    vec3_t spawn_pos, pos_copy, rel_copy, local_vec;
    struct fx_primitive_s *def;
    int val;

    FxSpawner_ComputeOrigin(ctx, (float *)&xf, origin, (float *)dir);
    Particle_SetBasis(obj, dir);
    def = ctx->def;
    FxSpawner_ComputeTraceOrigin(def, (const float *)&xf, spawn_pos, origin, (float *)dir);
    Vec3_Copy_08137210(xf.origin, pos_copy);
    Vec3_Copy_08137210(spawn_pos, rel_copy);
    FxPickList_PickOut(&val, &def->value_68);
    if (!FxRef_IsValid(&ctx->ref)) {
        FxObj_SetVec24c(obj, rel_copy);
    } else {
        struct FxBinding *target = FxRef_Get(&ctx->ref);
        struct Orient *orient = (struct Orient *)FxBinding_ResolveFrame(target);
        Orient_PointWorldToLocal(orient, rel_copy, local_vec);
        FxObj_SetVec24c(obj, local_vec);
    }
    FxObj_SetOrigin(obj, pos_copy);
    FxObj_SetField40_B(obj, val);
    if ((def->flags_90 & 0x2000) != 0) {
        FxObj_SetField118(obj, Rand_Float(0.0f, 1.0f));
    }
    if ((def->flags_90 & 0x4000) != 0) {
        FxObj_SetField11c(obj, Rand_Float(0.0f, 1.0f));
    }
    if ((def->flags_90 & 0x8000) != 0) {
        FxObj_SetField120(obj, Rand_Float(0.0f, 1.0f));
    }
    if ((def->flags_90 & 0x10000) != 0) {
        FxObj_SetField124(obj, Rand_Float(0.0f, 1.0f));
    }
    FxEffect_ResetSortKey(obj);
}

// 0x0813604c  size=45  FUN_0813604c  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0x278 bytes from pool 0x8a533b8, ctor Tail_ConstructBase
void FxSpawner_SpawnTail(struct FxSpawner *ctx, const float *dir, const float *origin, int arg4, int arg5)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocTailObj(0x278);
    Tail_ConstructBase((struct Tail *)obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    struct fx_xform_s xf;
    vec3_t moved;

    FxSpawner_Randomize(ctx, obj, (float *)&xf, origin, (float *)dir); /* the binary also pushes arg5 as an unused 6th argument */
    FxSpawner_SetupSequence(ctx->def, obj, FxObj_GetSpan(obj), arg5);
    FxSpawner_ApplyElapsed(ctx->def, obj, (float *)&xf, arg4);
    Vec3_MA_08137300(xf.origin, -1.0f, dir, moved);
    FxObj_SetOrigin(obj, xf.origin);
    FxObj_SetVec24cC(obj, moved);
    /* store at +0x25c (float) overlaps vec_258.y in the generic layout */
    *(float *)((char *)obj + 0x25c) = Rand_Float(0.0f, 1.0f);
    Tail_UpdateLengthAndPoint(obj);
}

// 0x081361e6  size=47  FUN_081361e6  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0x278 bytes from pool 0x8a533c0, ctor Cylinder_ConstructBase; arg4 unused
void FxSpawner_SpawnCylinder(struct FxSpawner *ctx, const float *dir, const float *origin, int arg4, int arg5)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocCylinderObj(0x278);
    Cylinder_ConstructBase((struct Cylinder *)obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    struct fx_xform_s xf;
    vec3_t pos_copy, dir_copy, local_dir;

    FxSpawner_Randomize(ctx, obj, (float *)&xf, origin, (float *)dir); /* the binary also pushes arg5 as an unused 6th argument */
    FxSpawner_SetupSequence(ctx->def, obj, FxObj_GetSpan(obj), arg5);
    Vec3_Copy_08137210(xf.origin, pos_copy);
    Vec3_Copy_08137210(dir, dir_copy);
    if (!FxRef_IsValid(&ctx->ref)) {
        FxObj_SetVec48(obj, dir_copy);
    } else {
        struct FxBinding *target = FxRef_Get(&ctx->ref);
        struct Orient *orient = (struct Orient *)FxBinding_ResolveFrame(target);
        Orient_VecWorldToLocal(orient, dir_copy, local_dir);
        FxObj_SetVec48(obj, local_dir);
    }
    FxObj_SetOrigin(obj, pos_copy);
}

// 0x0813638c  size=61  FUN_0813638c  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0x29c bytes from pool 0x8a533c8, ctor Emitter_ConstructBase; Vec3_ToAngles(dir) + ranges at def+0x228..0x268
void FxSpawner_SpawnEmitter(struct FxSpawner *ctx, const float *dir, const float *origin, int arg4, int arg5)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocEmitterObj(0x29c);
    Emitter_ConstructBase((struct Emitter *)obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    struct fx_xform_s xf;
    struct fx_primitive_s *def;
    vec3_t base_vec, angles, spread_vec, local_vec;
    int kind_val;
    int field_290_val;
    float f294, f298;
    float rx, ry, rz;

    FxSpawner_Randomize(ctx, obj, (float *)&xf, origin, (float *)dir); /* the binary also pushes arg5 as an unused 6th argument */
    def = ctx->def;
    FxSpawner_ApplyElapsed(def, obj, (float *)&xf, arg4);
    rz = FxRange_Random(&def->range_238);
    ry = FxRange_Random(&def->range_230);
    rx = FxRange_Random(&def->range_228);
    Vec3_Set_081371ec(base_vec, rx, ry, rz);
    Vec3_ToAngles(dir, angles);
    Vec3_Add_08137240(base_vec, angles, base_vec);
    rz = FxRange_Random(&def->range_250);
    ry = FxRange_Random(&def->range_248);
    rx = FxRange_Random(&def->range_240);
    Vec3_Set_081371ec(spread_vec, rx, ry, rz);
    FxPickList_PickOut(&kind_val, &def->value_68);
    field_290_val = FxPickList_Pick(&def->value_80);
    f294 = FxRange_Random(&def->range_260);
    f298 = FxRange_Random(&def->range_268);
    if (!FxRef_IsValid(&ctx->ref)) {
        FxObj_SetVec264OrClear(obj, 0);
    } else {
        struct FxBinding *target = FxRef_Get(&ctx->ref);
        struct Orient *orient = (struct Orient *)FxBinding_ResolveFrame(target);
        FxObj_SetVec264OrClear(obj, (const float *)orient);
    }
    if (kind_val == 0) {
        FxObj_ClearFlagsA8(obj, 0x10);
    }
    Particle_GetStartVelocity(obj, local_vec);
    FxObj_SetOrigin(obj, xf.origin);
    FxObj_SetVec24cOrClear(obj, xf.origin);
    FxObj_SetVec258OrClear(obj, local_vec);
    FxObj_SetVec278OrClear(obj, base_vec);
    FxObj_SetVec284OrClear(obj, spread_vec);
    FxObj_SetFieldB4(obj, kind_val);
    FxObj_SetField290(obj, field_290_val);
    FxObj_SetField294(obj, f294);
    FxObj_SetField298(obj, f298);
    FxObj_SetField270(obj, g_fxSystem->time);
    FxObj29c_RandomizeValue((struct Emitter *)obj);
}

// 0x08136762  size=77  FUN_08136762  status=translated  naming=unknown
// builds transform with FxSpawner_ComputeOrigin then hands (manager, def, xform, dir) to FX_DecalEvent_Stub; no strings
void FxSpawner_QueueDecalEvent(struct FxSpawner *ctx, const float *dir, const float *origin)
{
    struct fx_xform_s xf;

    FxSpawner_ComputeOrigin(ctx, xf.origin, origin, (float *)dir);
    FX_DecalEvent_Stub(); /* empty callee (push ebp; pop ebp; ret); the original passes (manager, def, xform, dir) */
}

// 0x081367b0  size=202  FUN_081367b0  status=translated  naming=unknown
// branch on attached reference; passes either dir or target+0x3c to FxScheduler_PlayEffect; no strings
void FxSpawner_QueueFxRunnerEvent(struct FxSpawner *ctx, const float *dir, const float *origin)
{
    struct fx_xform_s xf;
    struct fx_primitive_s *def;

    FxSpawner_ComputeOrigin(ctx, (float *)&xf, origin, (float *)dir);
    def = ctx->def;
    if (!FxRef_IsValid(&ctx->ref)) {
        FxScheduler_PlayEffect(g_fxScheduler, (struct FxEffectTemplate *)FxPickList_Pick(&def->value_88), (float *)&xf, (float *)dir, 0);
    } else {
        struct FxBinding *target = FxRef_Get(&ctx->ref);
        FxScheduler_PlayEffect(g_fxScheduler, (struct FxEffectTemplate *)FxPickList_Pick(&def->value_88), (float *)&xf, 0, (int *)((char *)target + 0x3c));
    }
}

// 0x0813687a  size=188  FUN_0813687a  status=translated  naming=unknown
// float->int truncation (fistp with control word 0xC00 in disassembly) of def ranges 0x58 and 0xe8; no strings
void FxSpawner_QueueCameraShakeEvent(struct FxSpawner *ctx, const float *dir, const float *origin)
{
    struct fx_xform_s xf;
    struct fx_primitive_s *def;
    int a, b;
    float f;

    FxSpawner_ComputeOrigin(ctx, (float *)&xf, origin, (float *)dir);
    def = ctx->def;
    a = (int)FxRange_Random(&def->range_58);
    b = (int)FxRange_Random(&def->range_e8);
    f = FxRange_Random(&def->range_280);
    FX_NoopE(); /* the binary passes (g_fxSystem, &xf, f, b, a) to this empty function */
}

// 0x08136936  size=44  FUN_08136936  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0xfc bytes from pool 0x8a53398, ctor Light_ConstructBase
void FxSpawner_SpawnLight(struct FxSpawner *ctx, const float *dir, const float *origin)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocLightFlashObj(0xfc);
    Light_ConstructBase((struct Light *)obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    struct fx_xform_s xf;
    struct fx_primitive_s *def;

    FxSpawner_ComputeOrigin(ctx, (float *)&xf, origin, (float *)dir);
    FxObj_SetOrigin(obj, xf.origin);
    def = ctx->def;
    if ((def->flags_90 & 0x2000) != 0) {
        FxObj_SetFieldC4(obj, Rand_Float(0.0f, 1.0f));
    }
    if ((def->flags_90 & 0x8000) != 0) {
        FxObj_SetFieldC8(obj, Rand_Float(0.0f, 1.0f));
    }
}

// 0x08136a8a  size=47  FUN_08136a8a  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0x258 bytes from pool 0x8a533b0, ctor OrientedParticle_ConstructBase
void FxSpawner_SpawnOrientedParticle(struct FxSpawner *ctx, const float *dir, const float *origin, int arg4, int arg5)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocOrientedParticleObj(600);
    OrientedParticle_ConstructBase((struct OrientedParticle *)obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    struct fx_xform_s xf;
    struct fx_primitive_s *def;
    vec3_t dir_copy, local_dir;

    FxSpawner_Randomize(ctx, obj, (float *)&xf, origin, (float *)dir); /* the binary also pushes arg5 as an unused 6th argument */
    def = (struct fx_primitive_s *)ctx->def;
    FxSpawner_SetupSequence(def, obj, FxObj_GetSpan(obj), arg5);
    FxSpawner_ApplyElapsed(def, obj, (float *)&xf, arg4);
    Vec3_Copy_08137210(dir, dir_copy);
    if (!FxRef_IsValid(&ctx->ref)) {
        FxObj_SetVec24cB(obj, dir_copy);
    } else {
        struct FxBinding *target = FxRef_Get(&ctx->ref);
        struct Orient *orient = (struct Orient *)FxBinding_ResolveFrame(target);
        Orient_VecWorldToLocal(orient, dir_copy, local_dir);
        FxObj_SetVec24cB(obj, local_dir);
    }
    FxObj_SetOrigin(obj, xf.origin);
}

// 0x08136c44  size=44  FUN_08136c44  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0xfc bytes from pool 0x8a53398; ctor wrapper Flash_Construct; origin param is used directly (no FxSpawner_ComputeOrigin)
void FxSpawner_SpawnFlash(struct FxSpawner *ctx, const float *dir, const float *origin)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocLightFlashObj(0xfc);
    Flash_Construct(obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    {
        struct fx_primitive_s *def = ctx->def;
        int val;

        FxPickList_PickOut(&val, &def->value_68);
        FxObj_SetOrigin(obj, origin);
        FxObj_SetField40_A(obj, val);
        if ((def->flags_90 & 0x2000) != 0) {
            FxObj_SetFieldC4(obj, Rand_Float(0.0f, 1.0f));
        }
        FxEffect_ResetSortKey(obj);
        FxObjFc_ApplyViewFalloff((struct Light *)obj);
    }
}

// 0x08136d84  size=45  FUN_08136d84  status=translated  naming=unknown
// object allocated from a fixed pool, constructed, init-checked via FxSpawner_Setup and destroyed through vtable slot 1 on failure; no strings; allocates 0x27c bytes from pool 0x8a533d0, ctor Cloud_Construct
void FxSpawner_SpawnCloud(struct FxSpawner *ctx, const float *dir, const float *origin, int arg4, int arg5)
{
    struct Effect *obj = (struct Effect *)FxPool_AllocCloudObj(0x27c);
    Cloud_Construct((struct Cloud *)obj);
    if (obj == 0) {
        return;
    }
    if (!FxSpawner_Setup(ctx, obj, origin)) {
        if (obj != 0) {
            ((void (*)(struct Effect *))obj->vtbl[1])(obj);
        }
        return;
    }
    struct fx_xform_s xf;
    struct fx_primitive_s *def;

    FxSpawner_Randomize(ctx, obj, (float *)&xf, origin, (float *)dir); /* the binary also pushes arg5 as an unused 6th argument */
    def = ctx->def;
    FxSpawner_SetupSequence(def, obj, FxObj_GetSpan(obj), arg5);
    FxSpawner_ApplyElapsed(def, obj, (float *)&xf, arg4);
    FxObj_SetOrigin(obj, xf.origin);
    *(float *)((char *)obj + 0x260) = Rand_Float(0.0f, 1.0f);
    *((unsigned char *)obj + 0x25c) = def->byte_9d;
}

// 0x08136ef6  size=379  FUN_08136ef6  status=translated  naming=unknown
// walks time-ordered pending list gated by a bool dvar; compares against clock time at g_fxSystem+4; no strings
void Fx_ProcessPendingEvents(void)
{
    struct fx_pending_s **link;
    struct fx_pending_s *node;
    unsigned char place[0x38]; /* FxPlacement: origin[3] at +0, axis at +0xc */

    if (g_08a53c80->value == 0) {
        return;
    }
    link = &g_fxScheduler->pending_head;
    node = *link;
    while (node != 0) {
        if (g_fxSystem->time < node->time) {
            link = &node->next;
        } else {
            struct fx_emitter_s *owner = node->owner;
            int slot_value = owner->slot[node->slot_index];

            Rand_SetSeed(node->seed);
            *link = node->next;
            g_fxScheduler->pending_count = g_fxScheduler->pending_count - 1;
            if (node->field_0c < 0) {
                FX_DrawPrimitive((uint32)g_fxScheduler, owner, (struct fx_primitive_s *)slot_value, (uint32)&node->field_0c,
                                 (uint32)node->field_14, (const struct fx_source_s *)node->field_20,
                                 g_fxSystem->time - node->time, node->arg_48);
            } else {
                if (FX_GetBoneOrientation(&node->field_0c, (struct FxPlacement *)place) != 0) {
                    FX_DrawPrimitive((uint32)g_fxScheduler, owner, (struct fx_primitive_s *)slot_value, (uint32)&node->field_0c,
                                     (uint32)place, (const struct fx_source_s *)(place + 12),
                                     g_fxSystem->time - node->time, node->arg_48);
                }
            }
            FX_PlaybackFree(node);
        }
        node = *link;
    }
}

// 0x08137154  size=80  FUN_08137154  status=translated  naming=unknown
// loop of 2 slots of 0xfc bytes (original: pointer advanced by 0xfc, counter 0 down to -1)
void Fx_ResetSlots(int a, int b)
{
    int *slot;
    int i;

    if (b == 0xffff && a == 1) {
        slot = (int *)g_08a5c9e0;
        for (i = 0; i != -1; ) {
            FX_ClearFourB(slot);
            slot = (int *)((char *)slot + 0xfc);
            i--;
        }
    }
}

// 0x081371a4  size=25  FUN_081371a4  status=translated  naming=unknown
// fabs x87 instruction then float store
float Float_Abs_081371a4(float x)
{
    return (float)fabs((double)x);
}

// 0x081371be  size=45  FUN_081371be  status=translated  naming=inferred
// calls libm sin then cos on the double-extended angle
void Math_SinCos_081371be(float angle, float *sin_out, float *cos_out)
{
    *sin_out = (float)sin((double)angle);
    *cos_out = (float)cos((double)angle);
}

// 0x081371ec  size=35  FUN_081371ec  status=translated  naming=inferred
// three stores of args into a vec3
void Vec3_Set_081371ec(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// 0x08137210  size=47  FUN_08137210  status=translated  naming=inferred
// copies 3 dwords; 14 callers
void Vec3_Copy_08137210(const float *src, float *dst)
{
    /* integer moves in the original (no x87 round trip, so signalling NaN bits are preserved) */
    ((unsigned int *)dst)[0] = ((const unsigned int *)src)[0];
    ((unsigned int *)dst)[1] = ((const unsigned int *)src)[1];
    ((unsigned int *)dst)[2] = ((const unsigned int *)src)[2];
}

// 0x08137240  size=68  FUN_08137240  status=translated  naming=inferred
// component add
void Vec3_Add_08137240(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x08137284  size=68  FUN_08137284  status=translated  naming=inferred
// component subtract
void Vec3_Sub_08137284(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x081372c8  size=56  FUN_081372c8  status=translated  naming=inferred
// scalar multiply
void Vec3_Scale_081372c8(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x08137300  size=83  FUN_08137300  status=translated  naming=inferred
// base + s*dir (multiply-add)
void Vec3_MA_08137300(const float *base, float s, const float *dir, float *out)
{
    out[0] = base[0] + s * dir[0];
    out[1] = base[1] + s * dir[1];
    out[2] = base[2] + s * dir[2];
}

// 0x08137354  size=51  FUN_08137354  status=translated  naming=inferred
// x,y,z products summed in that order (disassembly)
float Vec3_Dot_08137354(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// 0x08137388  size=62  FUN_08137388  status=translated  naming=inferred
// sqrt of dot with itself
float Vec3_Length_08137388(const float *v)
{
    return Math_SqrtF(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// 0x081373c6  size=93  FUN_081373c6  status=translated  naming=unknown
// swaps two dwords of the array pointed to by global 0x8188ab8
void FX_ListA_SwapEntries(int i, int j)
{
    int *arr = (int *)g_fxEffectListA;
    int tmp = arr[i];

    arr[i] = arr[j];
    arr[j] = tmp;
}

// 0x08137424  size=93  FUN_08137424  status=translated  naming=unknown
// swaps two dwords of the array pointed to by global 0x8188abc
void FX_ListB_SwapEntries(int i, int j)
{
    int *arr = (int *)g_fxEffectListB;
    int tmp = arr[i];

    arr[i] = arr[j];
    arr[j] = tmp;
}

// 0x08137482  size=31  FUN_08137482  status=translated  naming=inferred
// libm sqrt via double
float Math_SqrtF(float x)
{
    return (float)sqrt((double)x);
}

// 0x081374a2  size=41  FUN_081374a2  status=translated  naming=inferred
// stores 3 zero dwords
void Vec3_Clear_081374a2(float *v)
{
    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
}

// 0x081374cc  size=45  FUN_081374cc  status=translated  naming=unknown
// NULL source zeroes destination
void Vec3_CopyOrClear(const float *src, float *dst)
{
    if (src != 0) {
        Vec3_Copy_08137210(src, dst);
    } else {
        Vec3_Clear_081374a2(dst);
    }
}

// 0x081374fa  size=28  FUN_081374fa  status=translated  naming=unknown
// forwards constants (1, 0xffff) to Fx_ResetSlots
void Fx_ResetAllSlots(void)
{
    Fx_ResetSlots(1, 0xffff);
}

// 0x08137516  size=28  FUN_08137516  status=translated  naming=unknown
// clears bits in +0xa8; FxSpawner_SpawnEmitter clears 0x10
void FxObj_ClearFlagsA8(struct Effect *obj, unsigned int mask)
{
    obj->flags_a8 = ~mask & obj->flags_a8;
}

// 0x08137532  size=51  FUN_08137532  status=translated  naming=unknown
// copies vec3 to vec_14, or zeroes it when the source pointer is NULL
void FxObj_SetVec14OrClear(struct Effect *obj, const float *v)
{
    if (v != 0) {
        Vec3_Copy_08137210(v, (float *)((char *)obj + 0x14));
    } else {
        Vec3_Clear_081374a2((float *)((char *)obj + 0x14));
    }
}

// 0x08137566  size=51  FUN_08137566  status=translated  naming=unknown
// copies vec3 to vec_20, or zeroes it when the source pointer is NULL
void FxObj_SetVec20OrClear(struct Effect *obj, const float *v)
{
    if (v != 0) {
        Vec3_Copy_08137210(v, (float *)((char *)obj + 0x20));
    } else {
        Vec3_Clear_081374a2((float *)((char *)obj + 0x20));
    }
}

// 0x0813759a  size=51  FUN_0813759a  status=translated  naming=unknown
// copies vec3 to origin, or zeroes it when the source pointer is NULL
void FxObj_SetOrigin(struct Effect *obj, const float *v)
{
    if (v != 0) {
        Vec3_Copy_08137210(v, (float *)((char *)obj + 0x4));
    } else {
        Vec3_Clear_081374a2((float *)((char *)obj + 0x4));
    }
}

// 0x081375ce  size=17  FUN_081375ce  status=translated  naming=unknown
// single store to object field flags_a8; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetFlagsA8(struct Effect *obj, unsigned int value)
{
    obj->flags_a8 = value;
}

// 0x081375e0  size=14  FUN_081375e0  status=translated  naming=unknown
// single store to object field field_10; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField10(struct Effect *obj, int value)
{
    obj->field_10 = value;
}

// 0x081375ee  size=14  FUN_081375ee  status=translated  naming=unknown
// single store to object field field_2c; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField2c(struct Effect *obj, int value)
{
    obj->field_2c = value;
}

// 0x081375fc  size=14  FUN_081375fc  status=translated  naming=unknown
// single store to object field field_30; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField30(struct Effect *obj, int value)
{
    obj->field_30 = value;
}

// 0x0813760a  size=14  FUN_0813760a  status=translated  naming=unknown
// single store to object field field_34; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField34(struct Effect *obj, int value)
{
    obj->field_34 = value;
}

// 0x08137618  size=14  FUN_08137618  status=translated  naming=unknown
// single store to object field field_38; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField38(struct Effect *obj, int value)
{
    obj->field_38 = value;
}

// 0x08137626  size=14  FUN_08137626  status=translated  naming=unknown
// returns object field field_b8; no strings
int FxObj_GetFieldB8(struct Effect *obj)
{
    return obj->field_b8;
}

// 0x08137634  size=11  FUN_08137634  status=translated  naming=unknown
// returns object field field_34; no strings
int FxObj_GetField34(struct Effect *obj)
{
    return obj->field_34;
}

// 0x08137640  size=11  FUN_08137640  status=translated  naming=unknown
// returns object field field_38; no strings
int FxObj_GetField38(struct Effect *obj)
{
    return obj->field_38;
}

// 0x0813764c  size=17  FUN_0813764c  status=translated  naming=unknown
// single store to object field field_c4; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetFieldC4(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_c4 = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x0813765e  size=17  FUN_0813765e  status=translated  naming=unknown
// single store to object field field_c8; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetFieldC8(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_c8 = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x08137670  size=27  FUN_08137670  status=translated  naming=unknown
// wrapper over pool allocator for 0xfc-byte objects (global pool 0x8a53398)
void *FxPool_AllocLightFlashObj(unsigned int size)
{
    return FxPool_PopFree_LightFlash(&g_fxPoolLightFlash, (int)size);
}

// 0x0813768c  size=28  FUN_0813768c  status=translated  naming=inferred
// calls base constructor Light_Construct then stores vtable pointer 0x815f308
void Flash_Construct(struct Effect *obj)
{
    Light_Construct((struct Light *)obj);
    obj->vtbl = vtbl_Flash;
}

// 0x081376a8  size=14  FUN_081376a8  status=translated  naming=unknown
// single store to object field field_40; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField40_A(struct Effect *obj, int value)
{
    obj->field_40 = value;
}

// 0x081376b6  size=14  FUN_081376b6  status=translated  naming=unknown
// single store to object field field_40; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField40_B(struct Effect *obj, int value)
{
    obj->field_40 = value;
}

// 0x081376c4  size=17  FUN_081376c4  status=translated  naming=unknown
// single store to object field field_f4; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetFieldF4(struct Effect *obj, int value)
{
    obj->field_f4 = value;
}

// 0x081376d6  size=14  FUN_081376d6  status=translated  naming=unknown
// single store to object field field_44; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField44(struct Effect *obj, int value)
{
    obj->field_44 = value;
}

// 0x081376e4  size=17  FUN_081376e4  status=translated  naming=unknown
// single store to object field field_100; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField100(struct Effect *obj, int value)
{
    obj->field_100 = value;
}

// 0x081376f6  size=27  FUN_081376f6  status=translated  naming=unknown
// single byte store at +0x104
void FxObj_SetField104(struct Effect *obj, unsigned char value)
{
    obj->field_104 = value;
}

// 0x08137712  size=17  FUN_08137712  status=translated  naming=unknown
// single store to object field field_108; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField108(struct Effect *obj, int value)
{
    obj->field_108 = value;
}

// 0x08137724  size=17  FUN_08137724  status=translated  naming=unknown
// single store to object field field_10c; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField10c(struct Effect *obj, int value)
{
    obj->field_10c = value;
}

// 0x08137736  size=17  FUN_08137736  status=translated  naming=unknown
// single store to object field field_110; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField110(struct Effect *obj, int value)
{
    obj->field_110 = value;
}

// 0x08137748  size=17  FUN_08137748  status=translated  naming=unknown
// single store to object field field_114; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField114(struct Effect *obj, int value)
{
    obj->field_114 = value;
}

// 0x0813775a  size=17  FUN_0813775a  status=translated  naming=unknown
// single store to object field field_118; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField118(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_118 = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x0813776c  size=17  FUN_0813776c  status=translated  naming=unknown
// single store to object field field_11c; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField11c(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_11c = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x0813777e  size=17  FUN_0813777e  status=translated  naming=unknown
// single store to object field field_120; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField120(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_120 = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x08137790  size=17  FUN_08137790  status=translated  naming=unknown
// single store to object field field_124; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField124(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_124 = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x081377a2  size=17  FUN_081377a2  status=translated  naming=unknown
// single store to object field field_128; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField128(struct Effect *obj, int value)
{
    obj->field_128 = value;
}

// 0x081377b4  size=17  FUN_081377b4  status=translated  naming=unknown
// single store to object field field_f8; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetFieldF8(struct Effect *obj, int value)
{
    obj->field_f8 = value;
}

// 0x081377c6  size=27  FUN_081377c6  status=translated  naming=unknown
// wrapper over pool allocator (global pool g_fxPoolParticle)
void *FxPool_AllocParticleObj(unsigned int size)
{
    return FxPool_PopFree_Particle(&g_fxPoolParticle, (int)size);
}

// 0x081377e2  size=27  FUN_081377e2  status=translated  naming=unknown
// wrapper over pool allocator (global pool g_fxPoolCloud)
void *FxPool_AllocCloudObj(unsigned int size)
{
    return FxPool_PopFree_Cloud(&g_fxPoolCloud, (int)size);
}

// 0x081377fe  size=31  FUN_081377fe  status=translated  naming=unknown
// copies vec3 to vec_24c (no NULL check)
void FxObj_SetVec24c(struct Effect *obj, const float *v)
{
    Vec3_Copy_08137210(v, (float *)((char *)obj + 0x24c));
}

// 0x0813781e  size=27  FUN_0813781e  status=translated  naming=unknown
// wrapper over pool allocator (global pool g_fxPoolLine)
void *FxPool_AllocLineObj(unsigned int size)
{
    return FxPool_PopFree_Line(&g_fxPoolLine, (int)size);
}

// 0x0813783a  size=31  FUN_0813783a  status=translated  naming=unknown
// copies vec3 to vec_24c (no NULL check)
void FxObj_SetVec24cB(struct Effect *obj, const float *v)
{
    Vec3_Copy_08137210(v, (float *)((char *)obj + 0x24c));
}

// 0x0813785a  size=27  FUN_0813785a  status=translated  naming=unknown
// wrapper over pool allocator (global pool g_fxPoolOrientedParticle)
void *FxPool_AllocOrientedParticleObj(unsigned int size)
{
    return FxPool_PopFree_OrientedParticle(&g_fxPoolOrientedParticle, (int)size);
}

// 0x08137876  size=31  FUN_08137876  status=translated  naming=unknown
// copies vec3 to vec_24c (no NULL check)
void FxObj_SetVec24cC(struct Effect *obj, const float *v)
{
    Vec3_Copy_08137210(v, (float *)((char *)obj + 0x24c));
}

// 0x08137896  size=27  FUN_08137896  status=translated  naming=unknown
// wrapper over pool allocator (global pool g_fxPoolTail)
void *FxPool_AllocTailObj(unsigned int size)
{
    return FxPool_PopFree_Tail(&g_fxPoolTail, (int)size);
}

// 0x081378b2  size=29  FUN_081378b2  status=translated  naming=unknown
// copies vec3 to vec_48 (no NULL check)
void FxObj_SetVec48(struct Effect *obj, const float *v)
{
    Vec3_Copy_08137210(v, (float *)((char *)obj + 0x48));
}

// 0x081378d0  size=27  FUN_081378d0  status=translated  naming=unknown
// wrapper over pool allocator (global pool g_fxPoolCylinder)
void *FxPool_AllocCylinderObj(unsigned int size)
{
    return FxPool_PopFree_Cylinder(&g_fxPoolCylinder, (int)size);
}

// 0x081378ec  size=17  FUN_081378ec  status=translated  naming=unknown
// single store to object field field_b4; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetFieldB4(struct Effect *obj, int value)
{
    obj->field_b4 = value;
}

// 0x081378fe  size=31  FUN_081378fe  status=translated  naming=unknown
// copies vec3 to vec_278, or zeroes it when the source pointer is NULL
void FxObj_SetVec278OrClear(struct Effect *obj, const float *v)
{
    Vec3_CopyOrClear(v, (float *)((char *)obj + 0x278));
}

// 0x0813791e  size=31  FUN_0813791e  status=translated  naming=unknown
// copies vec3 to vec_284, or zeroes it when the source pointer is NULL
void FxObj_SetVec284OrClear(struct Effect *obj, const float *v)
{
    Vec3_CopyOrClear(v, (float *)((char *)obj + 0x284));
}

// 0x0813793e  size=17  FUN_0813793e  status=translated  naming=unknown
// single store to object field field_290; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField290(struct Effect *obj, int value)
{
    obj->field_290 = value;
}

// 0x08137950  size=17  FUN_08137950  status=translated  naming=unknown
// single store to object field field_294; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField294(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_294 = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x08137962  size=17  FUN_08137962  status=translated  naming=unknown
// single store to object field field_298; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField298(struct Effect *obj, float value)
{
    /* plain dword move in the original (no x87 round trip) */
    *(unsigned int *)&obj->field_298 = *(volatile unsigned int *)&value; /* volatile: stop the host compiler turning the dword move into an x87 fld/fstp */
}

// 0x08137974  size=17  FUN_08137974  status=translated  naming=unknown
// single store to object field field_270; no strings; callers are the Fx_Spawn* functions of this chunk
void FxObj_SetField270(struct Effect *obj, int value)
{
    obj->field_270 = value;
}

// 0x08137986  size=31  FUN_08137986  status=translated  naming=unknown
// copies vec3 to vec_24c, or zeroes it when the source pointer is NULL
void FxObj_SetVec24cOrClear(struct Effect *obj, const float *v)
{
    Vec3_CopyOrClear(v, (float *)((char *)obj + 0x24c));
}

// 0x081379a6  size=31  FUN_081379a6  status=translated  naming=unknown
// copies vec3 to vec_258, or zeroes it when the source pointer is NULL
void FxObj_SetVec258OrClear(struct Effect *obj, const float *v)
{
    Vec3_CopyOrClear(v, (float *)((char *)obj + 0x258));
}

// 0x081379c6  size=31  FUN_081379c6  status=translated  naming=unknown
// copies vec3 to vec_264, or zeroes it when the source pointer is NULL
void FxObj_SetVec264OrClear(struct Effect *obj, const float *v)
{
    Vec3_CopyOrClear(v, (float *)((char *)obj + 0x264));
}

// 0x081379e6  size=27  FUN_081379e6  status=translated  naming=unknown
// wrapper over pool allocator (global pool g_fxPoolEmitter)
void *FxPool_AllocEmitterObj(unsigned int size)
{
    return FxPool_PopFree_Emitter(&g_fxPoolEmitter, (int)size);
}

// 0x08137a02  size=13  FUN_08137a02  status=translated  naming=unknown
// double indirection: returns word at +8 of the object the handle points to
int Handle_GetTargetField8(int **handle)
{
    return (*handle)[2];
}

// 0x08137a10  size=20  FUN_08137a10  status=translated  naming=inferred
// allocates 12 bytes via merged Com_MallocZeroed; return value passed through in eax (disassembly)
void *Fx_AllocSmall(void)
{
    return Com_MallocZeroed(0xc);
}

// 0x08137a24  size=19  FUN_08137a24  status=translated  naming=inferred
// forwards to merged Com_Free_A
void Fx_FreeSmall(void *p)
{
    Com_Free_A(p);
}

// 0x08137a38  size=222  FUN_08137a38  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPoolFC_PushFront, pops item and memsets 252 bytes; no strings
void *FxPool_PopFree_LightFlash(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPoolFC_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 252);
    return item;
}

// 0x08137b16  size=222  FUN_08137b16  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPool24C_PushFront, pops item and memsets 588 bytes; no strings
void *FxPool_PopFree_Particle(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPool24C_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 588);
    return item;
}

// 0x08137bf4  size=222  FUN_08137bf4  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPool27C_PushFront, pops item and memsets 636 bytes; no strings
void *FxPool_PopFree_Cloud(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPool27C_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 636);
    return item;
}

// 0x08137cd2  size=222  FUN_08137cd2  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPool258a_PushFront, pops item and memsets 600 bytes; no strings
void *FxPool_PopFree_Line(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPool258a_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 600);
    return item;
}

// 0x08137db0  size=222  FUN_08137db0  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPool258b_PushFront, pops item and memsets 600 bytes; no strings
void *FxPool_PopFree_OrientedParticle(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPool258b_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 600);
    return item;
}

// 0x08137e8e  size=222  FUN_08137e8e  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPool278a_PushFront, pops item and memsets 632 bytes; no strings
void *FxPool_PopFree_Tail(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPool278a_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 632);
    return item;
}

// 0x08137f6c  size=222  FUN_08137f6c  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPool278b_PushFront, pops item and memsets 632 bytes; no strings
void *FxPool_PopFree_Cylinder(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPool278b_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 632);
    return item;
}

// 0x0813804a  size=222  FUN_0813804a  status=translated  naming=unknown
// free-list pool allocator: finds block with free items (+0x7ff4), moves it to the front via FxPool29C_PushFront, pops item and memsets 668 bytes; no strings
void *FxPool_PopFree_Emitter(struct FxPool *pool, int request)
{
    struct FxPoolBlock *block = pool->head;
    struct fx_pool_free_s *item;

    while (block != 0) {
        if (block->free_head != 0) {
            if (block != pool->head) {
                FxPool29C_PushFront(pool, block);
            }
            break;
        }
        block = block->next;
    }
    if (block == 0) {
        block = FxPool_AllocBlock(pool->head, pool->block_arg, request);
        if (block == 0) {
            return 0;
        }
        pool->head = block;
    }
    item = block->free_head;
    block->free_head = item->next;
    block->free_count = block->free_count - 1;
    Com_Memset(item, 0, 668);
    return item;
}

// 0x08138128  size=43  FUN_08138128  status=translated  naming=inferred
// sets flag when loc_language is in 8..12 (unsigned compare); string loc_language in same module; consumers are CJK code-page helpers
void Loc_UpdateDoubleByteFlag(void)
{
    if ((unsigned int)(g_loc_language->current.integer - 8) <= 4U) {
        g_loc_isDoubleByteLanguage = 1;
    } else {
        g_loc_isDoubleByteLanguage = 0;
    }
}

// 0x08138154  size=13  FUN_08138154  status=translated  naming=inferred
// returns loc_language dvar integer value
int Loc_GetLanguage(void)
{
    return g_loc_language->current.integer;
}

// 0x08138162  size=194  FUN_08138162  status=translated  naming=observed
// string dvar names loc_language, loc_forceEnglish, loc_translate, loc_warnings, loc_warningsAsErrors
void Loc_RegisterDvars(void)
{
    g_loc_language = Dvar_RegisterInt("loc_language", 0, 0, 0xe, 0x1021);
    g_loc_forceEnglish = Dvar_RegisterBool("loc_forceEnglish", 0, 0x1021);
    g_loc_translate = Dvar_RegisterBool("loc_translate", 1, 0x1020);
    g_loc_warnings = Dvar_RegisterBool("loc_warnings", 0, 0x1000);
    g_loc_warningsAsErrors = Dvar_RegisterBool("loc_warningsAsErrors", 0, 0x1000);
    Loc_UpdateDoubleByteFlag();
}

// 0x08138224  size=300  FUN_08138224  status=translated  naming=observed
// string "^1ERROR: No languages available because no localized assets were found"; re-registers loc dvars by name
// (results discarded), marks the 15 languages that have a loaded localized search path, then loads the current
// language or the first loadable one (setting loc_language to it), else sets loc_language to 0.
void Loc_InitLanguages(void)
{
    int available_count;
    int i;

    Dvar_RegisterInt(g_loc_language->name, 0, 0, 0xe, 0x1021);
    Dvar_RegisterBool(g_loc_forceEnglish->name, 0, 0x1021);
    Loc_UpdateDoubleByteFlag();
    available_count = 0;
    for (i = 0; i <= 0xe; i++) {
        if (FS_HasLoadedPakWithId(i) != 0) {
            loc_languageTable[i].reserved = 1;
            available_count++;
        } else {
            loc_languageTable[i].reserved = 0;
        }
    }
    if (available_count <= 0) {
        Com_Printf("^1ERROR: No languages available because no localized assets were found\n");
    }
    if (Loc_LoadLanguage(g_loc_language->current.integer) != 0) {
        return;
    }
    for (i = 0; i <= 0xe; i++) {
        Dvar_SetInt(g_loc_language, i);
        Loc_UpdateDoubleByteFlag();
        if (Loc_LoadLanguage(i) != 0) {
            return;
        }
    }
    Dvar_SetInt(g_loc_language, 0);
    Loc_UpdateDoubleByteFlag();
}

// 0x08138350  size=158  FUN_08138350  status=translated  naming=inferred
// reads the same 15-entry availability table as Loc_InitLanguages; returns first available language starting at lang (wrapping), else 0
int Loc_ClampToAvailableLanguage(int lang)
{
    int i;

    if (loc_languageTable[lang].reserved == 0) {
        for (i = 0; i < 0xf; i++) {
            if (loc_languageTable[(lang + i) % 0xf].reserved != 0) {
                return (lang + i) % 0xf;
            }
        }
        return 0;
    }
    return lang;
}

// 0x081383ee  size=13  FUN_081383ee  status=translated  naming=unknown
// tail wrapper into the localization-string backend (0x813a768); function purpose unknown
void Loc_CreateStringTable(void)
{
    LocTable_Create();
}

// 0x081383fc  size=13  FUN_081383fc  status=translated  naming=unknown
// tail wrapper into the localization-string backend (0x813a7d0); function purpose unknown
void LocTable_DestroyThunk(void)
{
    LocTable_Destroy();
}

// 0x0813840a  size=193  FUN_0813840a  status=translated  naming=observed
// strings "Could not load localization strings for %s: %s" (warning and error variants)
int Loc_LoadLanguage(int lang)
{
    const char *error;

    if (loc_languageTable[lang].reserved == 0) {
        return 0;
    }
    error = LocTable_LoadAll(g_loc_forceEnglish->current.enabled);
    if (error == NULL) {
        return 1;
    }
    if (fs_ignoreLocalized->current.enabled == 0 && g_loc_warnings->current.enabled != 0) {
        if (g_loc_warningsAsErrors->current.enabled != 0) {
            Com_Error(6, "Could not load localization strings for %s: %s", LocLanguage_Name(lang), error);
        } else {
            Com_Printf("^3WARNING: Could not load localization strings for %s: %s\n", LocLanguage_Name(lang), error);
        }
    }
    return 0;
}

// 0x081384cc  size=86  FUN_081384cc  status=translated  naming=inferred
// guarded by loc_translate dvar; empty and one-character strings returned unchanged (the first-byte test is
// duplicated in the binary); otherwise LocTable_Lookup's result (NULL when untranslated)
char *Loc_TranslateString(char *s)
{
    if (g_loc_translate == NULL || g_loc_translate->current.enabled == 0) {
        return s;
    }
    if (*s == '\0' || (*s != '\0' && s[1] == '\0')) {
        return s;
    }
    return (char *)LocTable_Lookup(s);
}

// 0x08138522  size=220  FUN_08138522  status=translated  naming=observed
// string "Could not translate exe string \"%s\"" and the "^1UNLOCALIZED(^7" wrapper
char *Loc_TranslateExeString(char *s)
{
    char *result = Loc_TranslateString(s);

    if (result != NULL) {
        return result;
    }
    if (g_loc_warnings->current.enabled != 0) {
        if (g_loc_warningsAsErrors->current.enabled != 0) {
            Com_Error(6, "Could not translate exe string \"%s\"", s);
        } else {
            Com_Printf("^3WARNING: Could not translate exe string \"%s\"\n", s);
        }
        strcpy(g_loc_unlocalizedBuffer, "^1UNLOCALIZED(^7");
        I_strncat(g_loc_unlocalizedBuffer, 0x400, s);
        I_strncat(g_loc_unlocalizedBuffer, 0x400, "^1)^7");
    } else {
        Q_strncpyz(g_loc_unlocalizedBuffer, s, 0x400);
    }
    return g_loc_unlocalizedBuffer;
}

// 0x081385fe  size=229  FUN_081385fe  status=translated  naming=observed
// string "Could not translate part of %s: \"%s\""; "%s" format at 0x8160a09 (rodata dump)
int Loc_TranslatePart(char *dest, char *src, const char *context, int mode)
{
    char *result = Loc_TranslateString(src);

    if (result == 0) {
        if (g_loc_warnings != 0 && Dvar_ValueBool(g_loc_warnings) != 0) {
            if (g_loc_warningsAsErrors != 0 && Dvar_ValueBool(g_loc_warningsAsErrors) != 0 && mode != 1) {
                Com_Error(6, "Could not translate part of %s: \"%s\"", context, src);
            } else {
                Com_Printf("^3WARNING: Could not translate part of %s: \"%s\".\n", context, src);
            }
            result = va("^1UNLOCALIZED(^7%s^1)^7", src);
        } else {
            result = va("%s", src);
        }
        if (mode == 1) {
            return 0;
        }
    }
    strcpy(dest, result);
    return 1;
}

// 0x08138b7e  size=77  FUN_08138b7e  status=translated  naming=inferred
// base 0xb0a0, row stride 0x60 matches GB2312 level-1 hanzi starting at 0xB0A1; validity delegated to LocText_IsEucKrHangul
int EucKr_CharIndex(int c)
{
    unsigned int rel;

    if (LocText_IsEucKrHangul(c) == 0) {
        return 0;
    }
    rel = (unsigned int)c - 0xb0a0U;
    return (int)((rel & 0xff) + (rel >> 8) * 0x60);
}

// 0x08138bcc  size=95  FUN_08138bcc  status=translated  naming=inferred
// lead 0xa1-0xc6/0xc9-0xf9, trail 0x40-0x7e/0xa1-0xfe are Big5 ranges
int Big5_IsValidChar(unsigned int c)
{
    unsigned char lead = (unsigned char)(c >> 8);
    unsigned char trail = (unsigned char)c;

    if ((((lead < 0xa1) || (0xc6 < lead)) && ((lead < 0xc9) || (0xf9 < lead))) ||
        (((trail < 0x40) || (0x7e < trail)) && ((trail < 0xa1) || (trail == 0xff)))) {
        return 0;
    }
    return 1;
}

// 0x08138c2c  size=45  FUN_08138c2c  status=translated  naming=inferred
// range 0xa140..0xa153 (Big5 punctuation block start)
int Big5_IsSymbolChar(unsigned int c)
{
    if (c < 0xa140 || 0xa153 < c) {
        return 0;
    }
    return 1;
}

// 0x08138c5a  size=91  FUN_08138c5a  status=translated  naming=inferred
// base 0xa140; 0x5f trail-low-range then skip to 0xa1 gives 0xa0 stride; matches Big5 layout
int Big5_CharIndex(unsigned int c)
{
    unsigned int orig = c;
    unsigned int rel;

    if (Big5_IsValidChar(c) == 0) {
        return 0;
    }
    rel = c - 0xa140;
    if (0x5f < (unsigned char)rel) {
        rel = orig - 0xa160;
    }
    return (int)((rel & 0xff) + (rel >> 8) * 0xa0);
}

// 0x08138cb6  size=91  FUN_08138cb6  status=translated  naming=inferred
// lead 0x81-0x9f/0xe0-0xef, trail 0x40-0x7e/0x80-0xfc are Shift-JIS ranges
int ShiftJis_IsValidChar(unsigned char lead, unsigned char trail)
{
    if ((((lead < 0x81) || (0x9f < lead)) && ((lead < 0xe0) || (0xef < lead))) ||
        (((trail < 0x40) || (0x7e < trail)) && ((-1 < (char)trail) || (0xfc < trail)))) {
        return 0;
    }
    return 1;
}

// 0x08138d12  size=45  FUN_08138d12  status=translated  naming=inferred
// range 0x8140..0x8151 (start of Shift-JIS symbol block)
int ShiftJis_IsSymbolChar(unsigned int c)
{
    if (c < 0x8140 || 0x8151 < c) {
        return 0;
    }
    return 1;
}

// 0x08138d40  size=107  FUN_08138d40  status=translated  naming=inferred
// base 0x8140, stride 0xbc (188 trail bytes) and 0x4000 gap skip match Shift-JIS; validity via LocText_IsDbcsPairClass10
int ShiftJis_CharIndex(unsigned int c)
{
    unsigned int orig = c;
    unsigned int rel;

    if (LocText_IsDbcsPairClass10(c) == 0) {
        return 0;
    }
    rel = c - 0x8140;
    if (0x3f < (unsigned char)rel) {
        rel = orig - 0x8141;
    }
    if (0x5e < ((rel >> 8) & 0xff)) {
        rel = rel - 0x4000;
    }
    return (int)((rel & 0xff) + (rel >> 8) * 0xbc);
}

// 0x08138dac  size=61  FUN_08138dac  status=translated  naming=unknown
// loose double-byte check: lead 0x81-0xfe, trail 0x41-0xfe; code page not identified
int Dbcs_IsLeadTrailPair(unsigned char lead, unsigned char trail)
{
    int result = 0;

    if ((0x80 < lead) && (lead != 0xff) && (0x40 < trail) && (trail != 0xff)) {
        result = 1;
    }
    return result;
}

// 0x08138dea  size=45  FUN_08138dea  status=translated  naming=unknown
// range 0x8141..0x814d; code page not identified
int Dbcs8140_IsSymbolChar(unsigned int c)
{
    if (c < 0x8141 || 0x814d < c) {
        return 0;
    }
    return 1;
}

// 0x08138e18  size=76  FUN_08138e18  status=translated  naming=unknown
// base 0x8140, stride 0x5f; code page not identified; validity via LocText_IsDbcsPairClassB
int Dbcs8140_CharIndex(int c)
{
    unsigned int rel;

    if (LocText_IsDbcsPairClassB(c) == 0) {
        return 0;
    }
    rel = (unsigned int)c - 0x8140U;
    return (int)((rel >> 8) * 0x5f + (rel & 0xff));
}
