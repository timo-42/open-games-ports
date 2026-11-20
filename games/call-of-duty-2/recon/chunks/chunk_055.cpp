#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_055_types.h"

/*
 * Call of Duty 2 Linux server 1.3, chunk 055: effects (Fx) math, curve, pool and scheduler code.
 * x87 float returns are modelled as float; x87 truncating conversions (fistp with RC=trunc) as (int) casts.
 */

// 0x08128a16  size=94  FUN_08128a16  status=translated  naming=inferred
// name: FxObj29c_IntegrateVelocity -- pos(+0x278) = VectorMA(pos, frameTime*0.01, vel(+0x284)), then AnglesToAxis(pos, vec48); same 0x29c class as the serializer at 0x08128a7e
void FxObj29c_IntegrateVelocity(struct Emitter *self)
{
    FxVectorMA(self->pos, (float)g_fxSystem->frameTime * 0.01f, self->vel, self->pos);
    AnglesToAxis(self->pos, self->vec48);
}

// 0x08128a74  size=10  FUN_08128a74  status=translated  naming=unknown
// name: Emitter_GetTypeId -- virtual-sized function returning constant 5 (sibling of the constant-9 and constant-0xb getters)
int Emitter_GetTypeId(void)
{
    return 5;
}

// 0x08128a7e  size=289  FUN_08128a7e  status=translated  naming=inferred
// name: FxObj29c_Serialize -- base serializer 0x081268aa then field-by-field stream calls for offsets 0x24c..0x298 (pool element size 0x29c)
void FxObj29c_Serialize(struct Emitter *self, struct FxArchive *stream)
{
    ((struct Particle *)self)->Archive(stream);
    FxStream_Vec3(stream, self->vec24c);
    FxStream_Vec3(stream, self->vec258);
    FxStream_Vec3(stream, self->vec264);
    FxStream_Int(stream, &self->field270);
    FxStream_Float(stream, &self->value274);
    FxStream_Vec3(stream, self->pos);
    FxStream_Vec3(stream, self->vel);
    MsgStream_TransferStringRef(FXMS(stream), &self->field290);
    FxStream_Float(stream, &self->base294);
    FxStream_Float(stream, &self->range298);
    if (self->field_b4 == 0) {
        self->flags = self->flags & 0xffffffef;
    }
}

// 0x08128ba0  size=28  FUN_08128ba0  status=translated  naming=inferred
// name: Light_Construct -- calls base constructor 0x0812464c then stores vtable 0x0815f348 (identical twin of 0x08128bbc, complete/base ctor pair)
void Light_Construct(struct Light *self)
{
    Effect_Construct((struct Effect *)self);
    *(void **)self = vtbl_0815f348;
}

// 0x08128bbc  size=28  FUN_08128bbc  status=translated  naming=inferred
// name: Light_ConstructBase -- identical twin of 0x08128ba0
void Light_ConstructBase(struct Light *self)
{
    Effect_Construct((struct Effect *)self);
    *(void **)self = vtbl_0815f348;
}

// 0x08128bd8  size=51  FUN_08128bd8  status=translated  naming=inferred
// name: Light_DestructComplete -- stores vtable 0x0815f348 then calls base destructor 0x08124690 (no delete)
void Light_DestructComplete(struct Light *self)
{
    *(void **)self = vtbl_0815f348;
    Effect_DestructBase((struct Effect *)self);
}

// 0x08128e1a  size=132  FUN_08128e1a  status=translated  naming=inferred
// name: FxObjFc_EvalSizeCurve -- size(+0x88) = scalar curve sample at age(+0x3c), blended between the two size curves when flag 0x8000 is set
void FxObjFc_EvalSizeCurve(struct Light *self)
{
    if ((self->flags & 0x8000) == 0) {
        self->size = FxCurve_EvalScaled(&self->sizeCurveA, self->age);
    } else {
        self->size = FxCurve_EvalBlendScaled(&self->sizeCurveA, &self->sizeCurveB, self->sizeBlend, self->age);
    }
}

// 0x08128e9e  size=125  FUN_08128e9e  status=translated  naming=inferred
// name: FxObjFc_EvalColorCurve -- color(+0x6c) = vec3 curve sample at age(+0x3c), blended between the two colour curves when flag 0x2000 is set
void FxObjFc_EvalColorCurve(struct Light *self)
{
    if ((self->flags & 0x2000) == 0) {
        FxCurveVec3_EvalScaled(&self->colorCurveA, self->color, self->age);
    } else {
        FxCurveVec3_EvalBlendScaled(&self->colorCurveA, &self->colorCurveB, self->colorBlend, self->color, self->age);
    }
}

// 0x08128f1c  size=10  FUN_08128f1c  status=translated  naming=unknown
// name: Light_GetTypeId -- returns constant 9
int Light_GetTypeId(void)
{
    return 9;
}

// 0x08128f26  size=164  FUN_08128f26  status=translated  naming=inferred
// name: FxObjFc_Serialize -- base serializer 0x081248fe, four curves via 0x0812350a, then the two blend floats (+0xc4,+0xc8)
void FxObjFc_Serialize(struct Light *self, struct FxArchive *stream)
{
    ((struct Effect *)self)->Archive(stream);
    MsgStream_TransferValue(FXMS(stream), &self->colorCurveA);
    MsgStream_TransferValue(FXMS(stream), &self->colorCurveB);
    MsgStream_TransferValue(FXMS(stream), &self->sizeCurveA);
    MsgStream_TransferValue(FXMS(stream), &self->sizeCurveB);
    FxStream_Float(stream, &self->colorBlend);
    FxStream_Float(stream, &self->sizeBlend);
}

// 0x08128fca  size=98  FUN_08128fca  status=translated  naming=inferred
// name: FxObjFc_CopyFromTemplate -- base copy 0x08124a8a then copies the key-table pointer (first dword) of the four template curves at +0x100/+0x10c/+0x130/+0x13c
void FxObjFc_CopyFromTemplate(struct Light *self, struct FxCurvePairTemplate *tmpl)
{
    ((struct Effect *)self)->CopyFromTemplate((const struct FxParticleTemplate *)tmpl);
    self->colorCurveA.keys = tmpl->curveA.keys;
    self->colorCurveB.keys = tmpl->curveB.keys;
    self->sizeCurveA.keys = tmpl->curveC.keys;
    self->sizeCurveB.keys = tmpl->curveD.keys;
}

// 0x0812902c  size=53  FUN_0812902c  status=translated  naming=inferred
// name: FxObjFc_UpdateColor -- if fn_081247f0(self) is non-zero, evaluates the colour curves; returns that flag
bool FxObjFc_UpdateColor(struct Light *self)
{
    char ok = ((struct Effect *)self)->UpdateFraction();
    if (ok != '\0') {
        FxObjFc_EvalColorCurve(self);
    }
    return ok != '\0';
}

// 0x08129062  size=252  FUN_08129062  status=translated  naming=unknown
// name: FxObjFc_ApplyViewFalloff -- scales colour curve A's scale (+0xd4) by a factor from distance to the view origin (0..600 units, quadratic) and facing dot with the view forward vector
void FxObjFc_ApplyViewFalloff(struct Light *self)
{
    vec3_t diff;
    float dist;
    float facing;
    float factor;

    facing = 1.0f;
    FxVectorSubtract(self->origin, g_fxSystem->viewOrigin, diff);
    dist = Vec3_Normalize(diff);
    facing = FxDotProduct(diff, g_fxSystem->viewForward);
    if (dist > 600.0f || (facing < 0.5f && dist > 100.0f)) {
        facing = 0.0f;
    } else if (facing < 0.5f && dist <= 100.0f) {
        facing = facing + 1.1f;
    }
    factor = (1.0f - (dist * dist) / 360000.0f) * facing;
    self->colorCurveA.scale = self->colorCurveA.scale * factor;
}

// 0x0812915e  size=231  FUN_0812915e  status=translated  naming=unknown
// name: Flash_Draw -- clamps colour(+0x6c) to 0..1, packs it to bytes (+0x90, alpha 1), puts screenPos(+0x7c) at viewOrigin + 8*viewForward, sets size and size2 to 12.0, then fn_08124a90(self, 4)
void Flash_Draw(struct Light *self)
{
    float rgba[7];
    int i;

    for (i = 0; i < 3; i = i + 1) {
        rgba[i] = FxClampFloat(self->color[i], 0.0f, 1.0f);
    }
    rgba[3] = 1.0f;
    FxColorFloatToBytes(rgba, self->colorBytes);
    FxVectorCopy(g_fxSystem->viewOrigin, self->screenPos);
    FxVectorMA(self->screenPos, 8.0f, g_fxSystem->viewForward, self->screenPos);
    self->size = 12.0f;
    self->size2 = self->size;
    ((struct Effect *)self)->SubmitToRenderer(4);
}

// 0x08129246  size=10  FUN_08129246  status=translated  naming=unknown
// name: Flash_GetTypeId -- returns constant 0xb
int Flash_GetTypeId(void)
{
    return 0xb;
}

// 0x08129250  size=26  FUN_08129250  status=translated  naming=inferred
// name: FxObjFc_Serialize_Thunk -- forwards (obj, stream) to 0x08128f26
void FxObjFc_Serialize_Thunk(struct Light *self, struct FxArchive *stream)
{
    FxObjFc_Serialize(self, stream);
}

// 0x0812926a  size=278  FUN_0812926a  status=translated  naming=inferred
// name: __static_initialization_and_destruction_0 -- GCC static-init shape (initialize_p == 1, priority == 0xffff); constructs the ten pool descriptors in order
void __static_initialization_and_destruction_0(int initialize_p, int priority)
{
    if (priority == 0xffff && initialize_p == 1) {
        FxPool44_Init(&g_fxPoolBinding);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPoolC4_Init(&g_fxPoolEffect);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPoolFC_Init(&g_fxPoolLightFlash);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPool24C_Init(&g_fxPoolParticle);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPool258a_Init(&g_fxPoolLine);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPool258b_Init(&g_fxPoolOrientedParticle);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPool278a_Init(&g_fxPoolTail);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPool278b_Init(&g_fxPoolCylinder);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPool29C_Init(&g_fxPoolEmitter);
    }
    if (priority == 0xffff && initialize_p == 1) {
        FxPool27C_Init(&g_fxPoolCloud);
    }
}

// 0x08129380  size=25  FUN_08129380  status=translated  naming=inferred
// name: FxFabs -- absolute value of a float
float FxFabs(float x)
{
    return fabsf(x);
}

// 0x0812939a  size=36  FUN_0812939a  status=translated  naming=inferred
// name: FxMaxFloat -- select(a-b >= 0 ? a : b) via 0x081298b2
float FxMaxFloat(float a, float b)
{
    return FxSelectFloatGE(a - b, a, b);
}

// 0x081293be  size=38  FUN_081293be  status=translated  naming=inferred
// name: FxMinInt -- select(b-a < 0 ? b : a) via 0x081298fc (Ghidra shows void; the callee result is returned in eax)
int FxMinInt(int a, int b)
{
    return FxSelectIntLT(b - a, a, b);
}

// 0x081293e4  size=61  FUN_081293e4  status=translated  naming=inferred
// name: FxClampFloatSelect -- clamp(value, lo, hi) built from two select calls (Ghidra shows void; result is returned in st0)
float FxClampFloatSelect(float value, float lo, float hi)
{
    float t = FxSelectFloatGE(value - hi, hi, value);
    return FxSelectFloatGE(lo - value, lo, t);
}

// 0x08129422  size=65  FUN_08129422  status=translated  naming=inferred
// name: FxClampIntSelect -- integer twin of 0x081293e4 (result returned in eax)
int FxClampIntSelect(int value, int lo, int hi)
{
    int t = FxSelectIntLT(value - hi, hi, value);
    return FxSelectIntLT(lo - value, lo, t);
}

// 0x08129464  size=65  FUN_08129464  status=translated  naming=inferred
// name: FxClampFloat -- clamp(value, lo, hi) with explicit branches (lo wins first)
float FxClampFloat(float value, float lo, float hi)
{
    float r;
    if (value < lo) {
        r = lo;
    } else if (hi < value) {
        r = hi;
    } else {
        r = value;
    }
    return r;
}

// 0x081294a6  size=54  FUN_081294a6  status=translated  naming=inferred
// name: FxRoundToInt -- floor(x + 0.5) converted to int with truncating x87 control word
int FxRoundToInt(float x)
{
    double r = floor((double)(x + 0.5f));
    return (int)r;
}

// 0x081294dc  size=41  FUN_081294dc  status=translated  naming=inferred
// name: FxVectorClear -- sets three dwords to zero
void FxVectorClear(float *v)
{
    ((int *)v)[0] = 0;
    ((int *)v)[1] = 0;
    ((int *)v)[2] = 0;
}

// 0x08129506  size=35  FUN_08129506  status=translated  naming=inferred
// name: FxVectorSet -- v = (x, y, z)
void FxVectorSet(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// 0x0812952a  size=47  FUN_0812952a  status=translated  naming=inferred
// name: FxVectorCopy -- dst = src (three dwords)
void FxVectorCopy(const float *src, float *dst)
{
    ((int *)dst)[0] = ((const int *)src)[0];
    ((int *)dst)[1] = ((const int *)src)[1];
    ((int *)dst)[2] = ((const int *)src)[2];
}

// 0x0812955a  size=45  FUN_0812955a  status=translated  naming=inferred
// name: FxVectorCopyOrClear -- clears dst when src is NULL, else copies
void FxVectorCopyOrClear(const float *src, float *dst)
{
    if (src == 0) {
        FxVectorClear(dst);
    } else {
        FxVectorCopy(src, dst);
    }
}

// 0x08129588  size=68  FUN_08129588  status=translated  naming=inferred
// name: FxVectorAdd -- out = a + b
void FxVectorAdd(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x081295cc  size=68  FUN_081295cc  status=translated  naming=inferred
// name: FxVectorSubtract -- out = a - b
void FxVectorSubtract(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x08129610  size=56  FUN_08129610  status=translated  naming=inferred
// name: FxVectorScale -- out = s * v
void FxVectorScale(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x08129648  size=83  FUN_08129648  status=translated  naming=inferred
// name: FxVectorMA -- out = a + s * b
void FxVectorMA(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x0812969c  size=51  FUN_0812969c  status=translated  naming=inferred
// name: FxDotProduct -- dot(a, b) accumulated in extended precision, z term first
float FxDotProduct(const float *a, const float *b)
{
    return (float)((long double)a[2] * (long double)b[2] + (long double)a[1] * (long double)b[1] +
                   (long double)a[0] * (long double)b[0]);
}

// 0x081296d0  size=106  FUN_081296d0  status=translated  naming=inferred
// name: FxVectorCompare -- exact component equality, 1 when all three match
int FxVectorCompare(const float *a, const float *b)
{
    int r = 0;
    if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]) {
        r = 1;
    }
    return r;
}

// 0x0812973a  size=51  FUN_0812973a  status=translated  naming=inferred
// name: FxVectorLengthSquared -- dot(v, v) in extended precision
float FxVectorLengthSquared(const float *v)
{
    return (float)((long double)v[2] * (long double)v[2] + (long double)v[1] * (long double)v[1] +
                   (long double)v[0] * (long double)v[0]);
}

// 0x0812976e  size=62  FUN_0812976e  status=translated  naming=inferred
// name: FxVectorLength -- FxSqrt(z*z + y*y + x*x) (Ghidra shows void; the result is returned in st0)
float FxVectorLength(const float *v)
{
    return FxSqrt(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]);
}

// 0x081297ac  size=261  FUN_081297ac  status=translated  naming=inferred
// name: FxColorFloatToBytes -- four floats * 255, rounded, clamped to [0,255] (min 255 then max 0), stored as bytes
void FxColorFloatToBytes(const float *rgba, unsigned char *out)
{
    int v;

    v = FxRoundToInt(rgba[0] * 255.0f);
    v = FxMinInt(0xff, v);
    out[0] = (unsigned char)FxMaxInt(0, v);
    v = FxRoundToInt(rgba[1] * 255.0f);
    v = FxMinInt(0xff, v);
    out[1] = (unsigned char)FxMaxInt(0, v);
    v = FxRoundToInt(rgba[2] * 255.0f);
    v = FxMinInt(0xff, v);
    out[2] = (unsigned char)FxMaxInt(0, v);
    v = FxRoundToInt(rgba[3] * 255.0f);
    v = FxMinInt(0xff, v);
    out[3] = (unsigned char)FxMaxInt(0, v);
}

// 0x081298b2  size=41  FUN_081298b2  status=translated  naming=inferred
// name: FxSelectFloatGE -- returns a when cond >= 0.0 else b
float FxSelectFloatGE(float cond, float a, float b)
{
    float r;
    if (0.0f <= cond) {
        r = a;
    } else {
        r = b;
    }
    return r;
}

// 0x081298dc  size=31  FUN_081298dc  status=translated  naming=inferred
// name: FxSqrt -- float sqrt through double sqrt
float FxSqrt(float x)
{
    double r = sqrt((double)x);
    return (float)r;
}

// 0x081298fc  size=31  FUN_081298fc  status=translated  naming=inferred
// name: FxSelectIntLT -- returns b when cond < 0 else a
int FxSelectIntLT(int cond, int a, int b)
{
    int r;
    if (cond < 0) {
        r = b;
    } else {
        r = a;
    }
    return r;
}

// 0x0812991c  size=38  FUN_0812991c  status=translated  naming=inferred
// name: FxMaxInt -- select(a-b < 0 ? b : a) via 0x081298fc (Ghidra shows void; result returned in eax)
int FxMaxInt(int a, int b)
{
    return FxSelectIntLT(a - b, a, b);
}

// 0x08129942  size=106  FUN_08129942  status=translated  naming=inferred
// name: FxVectorLerp -- out = a + (b - a) * t
void FxVectorLerp(const float *a, const float *b, float t, float *out)
{
    out[0] = a[0] + (b[0] - a[0]) * t;
    out[1] = a[1] + (b[1] - a[1]) * t;
    out[2] = a[2] + (b[2] - a[2]) * t;
}

// 0x081299ac  size=28  FUN_081299ac  status=translated  naming=inferred
// name: _GLOBAL__I_FxPools -- static-constructor entry calling __static_initialization_and_destruction_0(1, 0xffff)
void _GLOBAL__I_FxPools(void)
{
    __static_initialization_and_destruction_0(1, 0xffff);
}

// 0x081299c8  size=32  FUN_081299c8  status=translated  naming=inferred
// name: FxCurve_EvalScaled -- scalar curve sample at t times the curve scale (+8)
float FxCurve_EvalScaled(struct FxCurve *curve, float t)
{
    float v = FxCurve_Eval(curve, t);
    return (float)((long double)v * (long double)curve->scale);
}

// 0x081299e8  size=61  FUN_081299e8  status=translated  naming=inferred
// name: FxCurveVec3_EvalScaled -- vec3 curve sample at t into out, then out *= curve scale
void FxCurveVec3_EvalScaled(struct FxCurve *curve, float *out, float t)
{
    FxCurveVec3_Eval(curve, out, t);
    FxVectorScale(out, curve->scale, out);
}

// 0x08129a26  size=76  FUN_08129a26  status=translated  naming=inferred
// name: FxCurve_EvalBlendScaled -- (a + (b - a) * blend) * curveA.scale where a/b are samples of curves A/B at t
float FxCurve_EvalBlendScaled(struct FxCurve *curveA, struct FxCurve *curveB, float blend, float t)
{
    float a = FxCurve_Eval(curveA, t);
    float b = FxCurve_Eval(curveB, t);
    return (float)((long double)(a + (b - a) * blend) * (long double)curveA->scale);
}

// 0x08129a72  size=143  FUN_08129a72  status=translated  naming=inferred
// name: FxCurveVec3_EvalBlendScaled -- vec3 samples of curves A and B at t, out = A + blend * (B - A), then out *= curveA.scale
void FxCurveVec3_EvalBlendScaled(struct FxCurve *curveA, struct FxCurve *curveB, float blend, float *out, float t)
{
    float diff[4];
    float other[6];

    FxCurveVec3_Eval(curveA, out, t);
    FxCurveVec3_Eval(curveB, other, t);
    FxVectorSubtract(other, out, diff);
    FxVectorMA(out, blend, diff, out);
    FxVectorScale(out, curveA->scale, out);
}

// 0x08129b02  size=34  FUN_08129b02  status=translated  naming=inferred
// name: FxCurveRef_EvalScaled -- Curve_IntegrateToKey(keys, t) times the scale float at +8 (curve handle flavour with an opaque key evaluator)
float FxCurveRef_EvalScaled(struct FxCurve *curve, float t)
{
    return Curve_IntegrateToKey(curve->keys, t) * curve->scale;
}

// 0x08129b24  size=80  FUN_08129b24  status=translated  naming=inferred
// name: FxCurveRef_EvalBlendScaled -- blend of two opaque-evaluator samples at t, times curveA.scale
float FxCurveRef_EvalBlendScaled(struct FxCurve *curveA, struct FxCurve *curveB, float blend, float t)
{
    float a = Curve_IntegrateToKey(curveA->keys, t);
    float b = Curve_IntegrateToKey(curveB->keys, t);
    return (float)((long double)(a + (b - a) * blend) * (long double)curveA->scale);
}

// 0x08129b74  size=27  FUN_08129b74  status=translated  naming=inferred
// name: FxPool44_Alloc -- allocates one 0x44-byte element from pool 0x08a53388 (tail call; allocation pointer returned)
void *FxPool44_Alloc(int arg)
{
    return FxPool44_AllocFrom(&g_fxPoolBinding, arg);
}

// 0x08129b90  size=27  FUN_08129b90  status=translated  naming=inferred
// name: FxPool44_Free -- returns an element to pool 0x08a53388
void FxPool44_Free(void *elem)
{
    FxPool44_FreeTo(&g_fxPoolBinding, elem);
}

// 0x08129bac  size=14  FUN_08129bac  status=translated  naming=inferred
// name: FxRef_Construct -- sets the handle to NULL
void FxRef_Construct(struct FxRef *ref)
{
    ref->obj = 0;
}

// 0x08129bba  size=29  FUN_08129bba  status=translated  naming=inferred
// name: FxRef_ConstructFromRaw -- handle = FxRef_AddRef(obj) (increments obj's refcount, no NULL check)
void FxRef_ConstructFromRaw(struct FxRef *ref, struct FxBinding *obj)
{
    ref->obj = FxRef_AddRef(obj);
}

// 0x08129bd8  size=29  FUN_08129bd8  status=translated  naming=inferred
// name: FxRef_Destruct -- releases the object when the handle is non-NULL
void FxRef_Destruct(struct FxRef *ref)
{
    if (ref->obj != 0) {
        FxBinding_Release(ref->obj);
    }
}

// 0x08129bf6  size=90  FUN_08129bf6  status=translated  naming=inferred
// name: FxRef_Assign -- handle assignment: when objects differ, release old (and null it), then take a ref on the source if it is valid
void FxRef_Assign(struct FxRef *dst, struct FxRef *src)
{
    if (dst->obj != src->obj) {
        if (dst->obj != 0) {
            FxBinding_Release(dst->obj);
            dst->obj = 0;
        }
        char valid = FxRef_IsValid(src);
        if (valid != '\0') {
            dst->obj = FxRef_AddRef(src->obj);
        }
    }
}

// 0x08129c50  size=17  FUN_08129c50  status=translated  naming=inferred
// name: FxRef_IsValid -- obj != NULL
bool FxRef_IsValid(struct FxRef *ref)
{
    return ref->obj != 0;
}

// 0x08129c62  size=62  FUN_08129c62  status=translated  naming=inferred
// name: FxRef_AssignRaw -- when obj differs: release the old object if any, then take a ref on the new one (no NULL check on the new pointer)
void FxRef_AssignRaw(struct FxRef *dst, struct FxBinding *obj)
{
    if (dst->obj != obj) {
        if (dst->obj != 0) {
            FxBinding_Release(dst->obj);
        }
        struct FxBinding *r = FxRef_AddRef(obj);
        dst->obj = r;
    }
}

// 0x08129ca0  size=10  FUN_08129ca0  status=translated  naming=inferred
// name: FxRef_Get -- returns the raw object pointer
struct FxBinding *FxRef_Get(struct FxRef *ref)
{
    return ref->obj;
}

// 0x08129caa  size=7  FUN_08129caa  status=translated  naming=unknown
// name: Effect_GetRayVisibility -- returns 1.0 in st0
float Effect_GetRayVisibility(void)
{
    return 1.0f;
}

// 0x08129cb2  size=5  FUN_08129cb2  status=translated  naming=unknown
// name: Effect_AddBlocker -- empty function
void Effect_AddBlocker(void)
{
}

// 0x08129cb8  size=25  FUN_08129cb8  status=translated  naming=inferred
// name: FxObj_GetSpan -- spanEnd(+0xbc) - spanStart(+0xb8)
int FxObj_GetSpan(struct Effect *obj)
{
    return obj->spanEnd - obj->spanStart;
}

// 0x08129cd2  size=27  FUN_08129cd2  status=translated  naming=inferred
// name: FxPoolC4_Free -- returns an element to pool 0x08a53390
void FxPoolC4_Free(void *elem)
{
    FxPoolC4_FreeTo(&g_fxPoolEffect, elem);
}

// 0x08129cee  size=27  FUN_08129cee  status=translated  naming=inferred
// name: FxPoolFC_Free -- returns an element to pool 0x08a53398
void FxPoolFC_Free(void *elem)
{
    FxPoolFC_FreeTo(&g_fxPoolLightFlash, elem);
}

// 0x08129d72  size=10  FUN_08129d72  status=translated  naming=unknown
// name: Flash_IsCulled -- returns constant 0
int Flash_IsCulled(void)
{
    return 0;
}

// 0x08129d7c  size=27  FUN_08129d7c  status=translated  naming=inferred
// name: FxPool24C_Free -- returns an element to pool 0x08a533a0
void FxPool24C_Free(void *elem)
{
    FxPool24C_FreeTo(&g_fxPoolParticle, elem);
}

// 0x08129d98  size=27  FUN_08129d98  status=translated  naming=inferred
// name: FxPool27C_Free -- returns an element to pool 0x08a533d0
void FxPool27C_Free(void *elem)
{
    FxPool27C_FreeTo(&g_fxPoolCloud, elem);
}

// 0x08129db4  size=27  FUN_08129db4  status=translated  naming=inferred
// name: FxPool258a_Free -- returns an element to pool 0x08a533a8
void FxPool258a_Free(void *elem)
{
    FxPool258a_FreeTo(&g_fxPoolLine, elem);
}

// 0x08129dd0  size=27  FUN_08129dd0  status=translated  naming=inferred
// name: FxPool258b_Free -- returns an element to pool 0x08a533b0
void FxPool258b_Free(void *elem)
{
    FxPool258b_FreeTo(&g_fxPoolOrientedParticle, elem);
}

// 0x08129dec  size=27  FUN_08129dec  status=translated  naming=inferred
// name: FxPool278a_Free -- returns an element to pool 0x08a533b8
void FxPool278a_Free(void *elem)
{
    FxPool278a_FreeTo(&g_fxPoolTail, elem);
}

// 0x08129e08  size=27  FUN_08129e08  status=translated  naming=inferred
// name: FxPool278b_Free -- returns an element to pool 0x08a533c0
void FxPool278b_Free(void *elem)
{
    FxPool278b_FreeTo(&g_fxPoolCylinder, elem);
}

// 0x08129e24  size=43  FUN_08129e24  status=translated  naming=inferred
// name: FxObj29c_RandomizeValue -- value274 = base294 + Rand_Signed() * range298 (random float in [base, base+range] shape)
void FxObj29c_RandomizeValue(struct Emitter *self)
{
    /* Rand_Signed returns its result in st(0); the multiply-add stays in extended precision and is rounded once on fstp */
    self->value274 = (float)(Rand_Signed() * (long double)self->range298 + (long double)self->base294);
}

// 0x08129e50  size=27  FUN_08129e50  status=translated  naming=inferred
// name: FxPool29C_Free -- returns an element to pool 0x08a533c8
void FxPool29C_Free(void *elem)
{
    FxPool29C_FreeTo(&g_fxPoolEmitter, elem);
}

// 0x08129e6c  size=12  FUN_08129e6c  status=translated  naming=unknown
// name: FxArchive_IsReading -- returns the byte at +4
unsigned char FxArchive_IsReading(struct FxArchive *obj)
{
    return obj->reading;
}

// 0x08129e78  size=37  FUN_08129e78  status=translated  naming=inferred
// name: FxStream_ReadInt -- reads 4 bytes from the stream via MsgStream_ReadCompressed and returns them
int FxStream_ReadInt(struct FxArchive *stream)
{
    int v;
    MsgStream_ReadCompressed(FXMS(stream), &v, 4);
    return v;
}

// 0x08129e9e  size=34  FUN_08129e9e  status=translated  naming=inferred
// name: FxStream_WriteInt -- writes the 4-byte stack slot of the value argument to the stream via MsgStream_WriteCompressed
void FxStream_WriteInt(struct FxArchive *stream, int value)
{
    MsgStream_WriteCompressed(FXMS(stream), &value, 4);
}

// 0x08129ec0  size=34  FUN_08129ec0  status=translated  naming=inferred
// name: FxStream_Byte -- (de)serializes one byte through MsgStream_TransferBytes(FXMS(stream), p, 1)
void FxStream_Byte(struct FxArchive *stream, void *p)
{
    MsgStream_TransferBytes(FXMS(stream), p, 1);
}

// 0x08129ee2  size=34  FUN_08129ee2  status=translated  naming=inferred
// name: FxStream_Int -- (de)serializes four bytes through MsgStream_TransferBytes
void FxStream_Int(struct FxArchive *stream, int *p)
{
    MsgStream_TransferBytes(FXMS(stream), p, 4);
}

// 0x08129f04  size=34  FUN_08129f04  status=translated  naming=inferred
// name: FxStream_Float -- (de)serializes four bytes through MsgStream_TransferBytes (same body as FxStream_Int)
void FxStream_Float(struct FxArchive *stream, float *p)
{
    MsgStream_TransferBytes(FXMS(stream), p, 4);
}

// 0x08129f26  size=34  FUN_08129f26  status=translated  naming=inferred
// name: FxStream_Vec3 -- (de)serializes twelve bytes through MsgStream_TransferBytes
void FxStream_Vec3(struct FxArchive *stream, float *p)
{
    MsgStream_TransferBytes(FXMS(stream), p, 0xc);
}

// 0x08129f48  size=24  FUN_08129f48  status=translated  naming=inferred
// name: FxPool44_Init -- perBlock = 0x1e1, head = NULL
void FxPool44_Init(struct FxPool *pool)
{
    pool->perBlock = 0x1e1;
    pool->head = 0;
}

// 0x08129f60  size=24  FUN_08129f60  status=translated  naming=inferred
// name: FxPoolC4_Init -- perBlock = 0xa7, head = NULL
void FxPoolC4_Init(struct FxPool *pool)
{
    pool->perBlock = 0xa7;
    pool->head = 0;
}

// 0x08129f78  size=24  FUN_08129f78  status=translated  naming=inferred
// name: FxPoolFC_Init -- perBlock = 0x81, head = NULL
void FxPoolFC_Init(struct FxPool *pool)
{
    pool->perBlock = 0x81;
    pool->head = 0;
}

// 0x08129f90  size=24  FUN_08129f90  status=translated  naming=inferred
// name: FxPool24C_Init -- perBlock = 0x37, head = NULL
void FxPool24C_Init(struct FxPool *pool)
{
    pool->perBlock = 0x37;
    pool->head = 0;
}

// 0x08129fa8  size=24  FUN_08129fa8  status=translated  naming=inferred
// name: FxPool258a_Init -- perBlock = 0x36, head = NULL
void FxPool258a_Init(struct FxPool *pool)
{
    pool->perBlock = 0x36;
    pool->head = 0;
}

// 0x08129fc0  size=24  FUN_08129fc0  status=translated  naming=inferred
// name: FxPool258b_Init -- perBlock = 0x36, head = NULL
void FxPool258b_Init(struct FxPool *pool)
{
    pool->perBlock = 0x36;
    pool->head = 0;
}

// 0x08129fd8  size=24  FUN_08129fd8  status=translated  naming=inferred
// name: FxPool278a_Init -- perBlock = 0x33, head = NULL
void FxPool278a_Init(struct FxPool *pool)
{
    pool->perBlock = 0x33;
    pool->head = 0;
}

// 0x08129ff0  size=24  FUN_08129ff0  status=translated  naming=inferred
// name: FxPool278b_Init -- perBlock = 0x33, head = NULL
void FxPool278b_Init(struct FxPool *pool)
{
    pool->perBlock = 0x33;
    pool->head = 0;
}

// 0x0812a008  size=24  FUN_0812a008  status=translated  naming=inferred
// name: FxPool29C_Init -- perBlock = 0x31, head = NULL
void FxPool29C_Init(struct FxPool *pool)
{
    pool->perBlock = 0x31;
    pool->head = 0;
}

// 0x0812a020  size=24  FUN_0812a020  status=translated  naming=inferred
// name: FxPool27C_Init -- perBlock = 0x33, head = NULL
void FxPool27C_Init(struct FxPool *pool)
{
    pool->perBlock = 0x33;
    pool->head = 0;
}

// 0x0812a038  size=76  FUN_0812a038  status=translated  naming=inferred
// name: FxCurve_Eval -- seeks the cursor for t then evaluates the key at cursor*8+8 with Curve_InterpolateSegment (scalar curve, 2 floats per key)
float FxCurve_Eval(struct FxCurve *curve, float t)
{
    FxCurve_Seek(curve, t);
    float v = Curve_InterpolateSegment((unsigned char *)curve->keys + curve->cursor * 8 + 8, t);
    return v;
}

// 0x0812a084  size=71  FUN_0812a084  status=translated  naming=inferred
// name: FxCurveVec3_Eval -- seeks the cursor for t then lerps key cursor*0x10+8 with the next key (vec3 curve, 4 floats per key) into out
void FxCurveVec3_Eval(struct FxCurve *curve, float *out, float t)
{
    FxCurve_Seek(curve, t);
    FxCurveVec3_Interp((float *)((unsigned char *)curve->keys + curve->cursor * 0x10 + 8), t, out);
}

// 0x0812a0cc  size=13  FUN_0812a0cc  status=translated  naming=inferred
// name: FxRef_AddRef -- increments the first dword (refcount) of the object and returns the pointer
struct FxBinding *FxRef_AddRef(struct FxBinding *obj)
{
    obj->refCount = obj->refCount + 1;
    return obj;
}

// 0x0812a0da  size=222  FUN_0812a0da  status=translated  naming=inferred
// name: FxPool44_AllocFrom -- finds the first block with a free element (moving it to the front when it is not the head), allocating a new block via FxPool_AllocBlock when none, pops the free list, and zeroes 0x44 bytes
void *FxPool44_AllocFrom(struct FxPool *pool, int arg)
{
    struct FxPoolBlock *blk = pool->head;
    void **elem;

    while (blk != 0) {
        if (blk->freeList != 0) {
            if (blk != pool->head) {
                FxPool44_PushFront(pool, blk);
            }
            break;
        }
        blk = blk->next;
    }
    if (blk == 0) {
        blk = FxPool_AllocBlock(pool->head, pool->perBlock, arg);
        if (blk == 0) {
            return 0;
        }
        pool->head = blk;
    }
    elem = blk->freeList;
    blk->freeList = (void **)*blk->freeList;
    blk->freeCount = blk->freeCount - 1;
    Com_Memset(elem, 0, 0x44);
    return elem;
}

// 0x0812a1b8  size=71  FUN_0812a1b8  status=translated  naming=inferred
// name: FxPool44_PushFront -- unlinks the block, then makes it the new head of the block list
void FxPool44_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812a200  size=152  FUN_0812a200  status=translated  naming=inferred
// name: FxPool44_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x44); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool44_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x44);
    if (pool->head->freeList == 0) {
        FxPool44_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a298  size=152  FUN_0812a298  status=translated  naming=inferred
// name: FxPoolC4_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0xc4); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPoolC4_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0xc4);
    if (pool->head->freeList == 0) {
        FxPoolC4_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a330  size=152  FUN_0812a330  status=translated  naming=inferred
// name: FxPoolFC_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0xfc); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPoolFC_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0xfc);
    if (pool->head->freeList == 0) {
        FxPoolFC_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a3c8  size=152  FUN_0812a3c8  status=translated  naming=inferred
// name: FxPool24C_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x24c); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool24C_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x24c);
    if (pool->head->freeList == 0) {
        FxPool24C_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a460  size=152  FUN_0812a460  status=translated  naming=inferred
// name: FxPool27C_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x27c); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool27C_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x27c);
    if (pool->head->freeList == 0) {
        FxPool27C_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a4f8  size=152  FUN_0812a4f8  status=translated  naming=inferred
// name: FxPool258a_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x258); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool258a_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x258);
    if (pool->head->freeList == 0) {
        FxPool258a_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a590  size=152  FUN_0812a590  status=translated  naming=inferred
// name: FxPool258b_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x258); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool258b_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x258);
    if (pool->head->freeList == 0) {
        FxPool258b_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a628  size=152  FUN_0812a628  status=translated  naming=inferred
// name: FxPool278a_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x278); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool278a_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x278);
    if (pool->head->freeList == 0) {
        FxPool278a_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a6c0  size=152  FUN_0812a6c0  status=translated  naming=inferred
// name: FxPool278b_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x278); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool278b_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x278);
    if (pool->head->freeList == 0) {
        FxPool278b_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a758  size=152  FUN_0812a758  status=translated  naming=inferred
// name: FxPool29C_FreeTo -- finds the owning block with FxPool_FreeElement(elem, 0x29c); when the head block has no free element the owner becomes the head, else when the owner has at least perBlock free elements it is released with FxPool_FreeBlock (unless it is the head and the next block is missing or full)
void FxPool29C_FreeTo(struct FxPool *pool, void *elem)
{
    struct FxPoolBlock *blk = FxPool_FreeElement(elem, 0x29c);
    if (pool->head->freeList == 0) {
        FxPool29C_PushFront(pool, blk);
    } else if (pool->perBlock <= blk->freeCount) {
        if (blk == pool->head) {
            if (blk->next == 0) {
                return;
            }
            if (blk->next->freeCount == 0) {
                return;
            }
            pool->head = blk->next;
        }
        FxPool_FreeBlock(blk);
    }
}

// 0x0812a7f0  size=88  FUN_0812a7f0  status=translated  naming=inferred
// name: FxCurveVec3_Interp -- lerps the vec3 of key (floats 1..3) toward the next key (floats 5..7) by (t - key.time) / (nextKey.time - key.time)
void FxCurveVec3_Interp(float *key, float t, float *out)
{
    FxVectorLerp(key + 1, key + 5, (t - key[0]) / (key[4] - key[0]), out);
}

// 0x0812a848  size=135  FUN_0812a848  status=translated  naming=inferred
// name: FxCurve_Seek -- moves the curve cursor so that key[cursor+1].time >= t: restarts at key 0 when t is before the current key, then advances while the next key time is < t
void FxCurve_Seek(struct FxCurve *curve, float t)
{
    int stride = *(int *)curve->keys + 1;
    float *key = (float *)((unsigned char *)curve->keys + curve->cursor * stride * 4 + 8);

    if (t < *key) {
        curve->cursor = 0;
        key = (float *)((unsigned char *)curve->keys + 8);
    }
    for (; key[stride] < t; key = key + stride) {
        curve->cursor = curve->cursor + 1;
    }
}

// 0x0812a8d0  size=71  FUN_0812a8d0  status=translated  naming=inferred
// name: FxPoolC4_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPoolC4_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812a918  size=71  FUN_0812a918  status=translated  naming=inferred
// name: FxPoolFC_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPoolFC_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812a960  size=71  FUN_0812a960  status=translated  naming=inferred
// name: FxPool24C_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPool24C_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812a9a8  size=71  FUN_0812a9a8  status=translated  naming=inferred
// name: FxPool27C_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPool27C_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812a9f0  size=71  FUN_0812a9f0  status=translated  naming=inferred
// name: FxPool258a_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPool258a_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812aa38  size=71  FUN_0812aa38  status=translated  naming=inferred
// name: FxPool258b_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPool258b_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812aa80  size=71  FUN_0812aa80  status=translated  naming=inferred
// name: FxPool278a_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPool278a_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812aac8  size=71  FUN_0812aac8  status=translated  naming=inferred
// name: FxPool278b_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPool278b_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812ab10  size=71  FUN_0812ab10  status=translated  naming=inferred
// name: FxPool29C_PushFront -- unlinks the block, then makes it the new head of the block list (clone of FxPool44_PushFront)
void FxPool29C_PushFront(struct FxPool *pool, struct FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->prev = 0;
    blk->next = pool->head;
    pool->head->prev = blk;
    pool->head = blk;
}

// 0x0812ab58  size=5  FUN_0812ab58  status=translated  naming=unknown
// name: FxNoOp_0812ab58 -- empty function
void FxNoOp_0812ab58(void)
{
}

// 0x0812ab5e  size=32  FUN_0812ab5e  status=translated  naming=inferred
// name: FxPickList_Pick -- returns the element chosen by FxPickList_PickOut
int FxPickList_Pick(struct FxPickList *list)
{
    int picked;
    FxPickList_PickOut(&picked, list);
    return picked;
}

// 0x0812ab7e  size=88  FUN_0812ab7e  status=translated  naming=inferred
// name: FxPickList_PickOut -- *out = 0 when the 16-bit count is zero, else items[Rand_Int(0, count)]; returns out (hidden-pointer return, ret 4)
int *FxPickList_PickOut(int *out, struct FxPickList *list)
{
    if (list->count == 0) {
        *out = 0;
    } else {
        int idx = Rand_Int(0, list->count);
        *out = list->items[idx];
    }
    return out;
}

// 0x0812abd6  size=25  FUN_0812abd6  status=translated  naming=inferred
// name: FxScheduler_ResetList_A -- head = NULL, count = 0
void FxScheduler_ResetList_A(struct FxScheduler *self)
{
    self->head = 0;
    self->count = 0;
}

// 0x0812abf0  size=25  FUN_0812abf0  status=translated  naming=inferred
// name: FxScheduler_ResetList_B -- identical twin of 0x0812abd6
void FxScheduler_ResetList_B(struct FxScheduler *self)
{
    self->head = 0;
    self->count = 0;
}

// 0x0812ac0a  size=89  FUN_0812ac0a  status=translated  naming=inferred
// name: FxScheduler::Clear -- pops and frees every scheduled-effect record (FX_PlaybackFree), zeroes the count, optionally calls FX_ReleaseTemplatesExcept(arg)
void FxScheduler_Clear(struct FxScheduler *self, char runCallback, struct FxEffectTemplate *arg)
{
    while (self->head != 0) {
        struct FxScheduledEffect *e = self->head;
        self->head = e->next;
        FX_PlaybackFree(e);
    }
    self->count = 0;
    if (runCallback != '\0') {
        FX_ReleaseTemplatesExcept(arg);
    }
}

// 0x0812ac64  size=20  FUN_0812ac64  status=translated  naming=observed
// name: FxScheduler_ReportInvalidEffect -- prints "FxScheduler::PlayEffect called with invalid effect\n" through FxPrintf
void FxScheduler_ReportInvalidEffect(void)
{
    FxPrintf("FxScheduler::PlayEffect called with invalid effect\n");
}

// 0x0812ac78  size=168  FUN_0812ac78  status=translated  naming=inferred
// name: FxScheduler::PlayEffect (default axis) -- builds axis (0,0,1),(1,0,0),(0,1,0) and calls the full PlayEffect with no bolt
void FxScheduler_PlayEffect_DefaultAxis(struct FxScheduler *self, struct FxEffectTemplate *effect, float *origin)
{
    float axis[3][3];

    Vec3_Set_0812be22(axis[0], 0.0f, 0.0f, 1.0f);
    Vec3_Set_0812be22(axis[1], 1.0f, 0.0f, 0.0f);
    Vec3_Set_0812be22(axis[2], 0.0f, 1.0f, 0.0f);
    FxScheduler_PlayEffect(self, effect, origin, &axis[0][0], 0);
}

// 0x0812ad20  size=97  FUN_0812ad20  status=translated  naming=inferred
// name: FxScheduler::PlayEffect (forward only) -- axis[0] = copy of the forward vector, right/up from Vec3_PerpendicularBasis(forward, right, up), then PlayEffect with no bolt
void FxScheduler_PlayEffect_Forward(struct FxScheduler *self, struct FxEffectTemplate *effect, float *origin, float *forward)
{
    float axis[3][3];

    Vec3_Copy_0812be46(forward, axis[0]);
    Vec3_PerpendicularBasis(axis[0], axis[1], axis[2]);
    FxScheduler_PlayEffect(self, effect, origin, &axis[0][0], 0);
}

// 0x0812ad82  size=118  FUN_0812ad82  status=translated  naming=inferred
// name: FxScheduler::PlayEffect (forward and up) -- axis[0] = forward, axis[2] = up, axis[1] = Vec3_Cross(forward, up) (cross), then PlayEffect with no bolt
void FxScheduler_PlayEffect_ForwardUp(struct FxScheduler *self, struct FxEffectTemplate *effect, float *origin, float *forward, float *up)
{
    float axis[3][3];

    Vec3_Copy_0812be46(forward, axis[0]);
    Vec3_Copy_0812be46(up, axis[2]);
    Vec3_Cross(axis[0], axis[2], axis[1]);
    FxScheduler_PlayEffect(self, effect, origin, &axis[0][0], 0);
}

// 0x0812adf8  size=244  FUN_0812adf8  status=translated  naming=inferred
// name: FxPrimTemplate_PassesDistanceLimits -- squared distance from origin to the view origin must be >= (minDistance*scale)^2 and <= (maxDistance*scale)^2 for the non-zero limits
bool FxPrimTemplate_PassesDistanceLimits(struct fx_primitive_s *prim, const float *origin)
{
    bool haveDist = false;
    float distSq = 0.0f;
    float lim;

    if (prim->minDistance != 0.0f || isnan(prim->minDistance)) {
        haveDist = true;
        distSq = Vec3_DistanceSquared(origin, g_fxSystem->viewOrigin);
        lim = prim->minDistance * g_fxSystem->distanceScale;
        if (distSq < lim * lim) {
            return false;
        }
    }
    if (prim->maxDistance != 0.0f || isnan(prim->maxDistance)) {
        if (!haveDist) {
            distSq = Vec3_DistanceSquared(origin, g_fxSystem->viewOrigin);
        }
        lim = prim->maxDistance * g_fxSystem->distanceScale;
        if (lim * lim < distSq) {
            return false;
        }
    }
    return true;
}

// 0x0812aeec  size=958  FUN_0812aeec  status=translated  naming=observed
// name: FxScheduler::PlayEffect -- stores a helper seed, falls back to "fx/error.efx" (and reports) when the effect is NULL, then for every primitive template spawns count = (int)(range + 0.5) records: delay <= 0 spawns now via FX_DrawPrimitive, else queues a 0x50-byte FxScheduledEffect
void FxScheduler_PlayEffect(struct FxScheduler *self, struct FxEffectTemplate *effect, float *origin, float *axis, int *bolt)
{
    struct FxPlacement place;
    int primIndex;
    int prim_i;
    int count;
    int delay;
    int loopIndex;
    int totalSpawned;
    float step;
    struct fx_primitive_s *prim;
    struct FxScheduledEffect *rec;
    int seedBase;

    primIndex = 0;
    count = 0;
    delay = 0;
    step = 0.0f;
    self->seed = FX_RandomStep(g_fxSystem);
    Rand_SetSeed(self->seed);
    if (effect == 0) {
        FX_NoopB(g_fxSystem, 1);
        effect = FX_GetEffectByName("fx/error.efx");
        FX_NoopB(g_fxSystem, 0);
        if (effect == 0) {
            FxScheduler_ReportInvalidEffect();
            return;
        }
    }
    if (g_08a53c98->value == '\0' && g_08a53c80->value != '\0') {
        if (bolt == 0) {
            if (origin == 0) {
                Vec3_Clear_0812bdf8(place.origin);
            } else {
                Vec3_Copy_0812be46(origin, place.origin);
            }
            Mat3_Copy(axis, &place.axis[0][0]);
        } else {
            if (*bolt < 0) {
                return;
            }
            char ok = FX_GetBoneOrientation(bolt, &place);
            if (ok == '\0') {
                return;
            }
        }
        totalSpawned = 0;
        for (primIndex = 0; primIndex < effect->primCount; primIndex = primIndex + 1) {
            prim = effect->prims[primIndex];
            if (!FxPrimTemplate_PassesDistanceLimits(prim, origin)) {
                continue;
            }
            if ((prim->flags & 0x400) != 0) {
                char culled = FX_IsCulledByPlanes2(g_fxSystem, origin, prim->cullArg);
                if (culled != '\0') {
                    continue;
                }
            }
            count = (int)(FxRange_Random(&prim->count) + 0.5f);
            if (count == 0) {
                continue;
            }
            if ((prim->flags & 0x200) != 0) {
                step = FX_Fabs(prim->delay.max - prim->delay.min) / (float)count;
            }
            totalSpawned = totalSpawned + count;
            for (loopIndex = 0; loopIndex < count; loopIndex = loopIndex + 1) {
                if ((prim->flags & 0x200) == 0) {
                    delay = (int)FxRange_Random(&prim->delay);
                } else {
                    delay = (int)((float)loopIndex * step);
                }
                if (delay < 1) {
                    FX_DrawPrimitive(self, effect, prim, bolt, place.origin, &place.axis[0][0], -delay, loopIndex);
                } else {
                    rec = (struct FxScheduledEffect *)FX_PlaybackAlloc(0x50);
                    if (rec != 0) {
                        rec->startTime = delay + g_fxSystem->time;
                        rec->effect = effect;
                        rec->primIndex = primIndex;
                        rec->loopIndex = loopIndex;
                        seedBase = FX_RandomStep(g_fxSystem);
                        rec->seed = primIndex * 0x369d035 + seedBase;
                        if (bolt == 0) {
                            rec->bolt[0] = -1;
                            rec->bolt[1] = -1;
                        } else {
                            int b1 = bolt[1];
                            rec->bolt[0] = bolt[0];
                            rec->bolt[1] = b1;
                        }
                        Vec3_Copy_0812be46(place.origin, rec->origin);
                        Mat3_Copy(&place.axis[0][0], &rec->axis[0][0]);
                        rec->next = self->head;
                        self->head = rec;
                        self->count = self->count + 1;
                    }
                }
            }
        }
    }
}

// 0x0812b2aa  size=112  FUN_0812b2aa  status=translated  naming=unknown
// name: FxEffectTemplate_GetDuration -- maximum over primitive templates of (field5c + delay.max); first parameter unused
float FxEffectTemplate_GetDuration(void *unused, struct FxEffectTemplate *effect)
{
    float best = 0.0f;
    int i;

    for (i = 0; i < effect->primCount; i = i + 1) {
        struct fx_primitive_s *prim = effect->prims[i];
        float maxDelay = prim->delay.max;
        float f5c = prim->field5c;
        if (best < f5c + maxDelay) {
            best = f5c + maxDelay;
        }
    }
    return best;
}

// 0x0812b31a  size=167  FUN_0812b31a  status=translated  naming=unknown
// name: FxPrim_SampleStartRgb -- builds temp curves from template curves A (+0x100) and, when flag 0x2000 is set, B (+0x10c), then samples at t=0: blended by Rand_Float(0,1.0) random, else plain; first parameter unused
void FxPrim_SampleStartRgb(void *unused, struct FxCurvePairTemplate *tmpl, float *out)
{
    struct FxCurveTemp a;
    struct FxCurveTemp b;
    float blend;

    FxCurve_InitFromTemplate(&tmpl->curveA, &a);
    if ((tmpl->flags & 0x2000) == 0) {
        FxCurveVec3_EvalScaled(&a.curve, out, 0.0f);
    } else {
        blend = Rand_Float(0.0f, 1.0f);
        FxCurve_InitFromTemplate(&tmpl->curveB, &b);
        FxCurveVec3_EvalBlendScaled(&a.curve, &b.curve, blend, out, 0.0f);
    }
}

// 0x0812b524  size=5  FUN_0812b524  status=translated  naming=unknown
// name: FX_DecalEvent_Stub -- empty function
void FX_DecalEvent_Stub(void)
{
}

// 0x0812b52a  size=90  FUN_0812b52a  status=translated  naming=unknown
// name: FxPrim_ResolveBoltRef -- returns 1 when flag 2 is clear, bolt is NULL or bolt[0] < 0; else builds a ref holder from bolt (FxBinding_FindOrCreate), assigns it to *outRef, and returns whether the ref is valid and FxBinding_ResolveFrame on it is non-zero
int FxPrim_ResolveBoltRef(struct FxCurvePairTemplate *tmpl, int *bolt, struct FxRef *outRef)
{
    int result;
    struct FxRefHolder holder;

    if ((tmpl->flags & 2) == 0 || bolt == 0 || *bolt < 0) {
        result = 1;
    } else {
        FxBinding_FindOrCreate(&holder.ref, (const struct FxBindingKey *)bolt);
        FxRef_Assign(outRef, &holder.ref);
        FxRef_Destruct(&holder.ref);
        char valid = FxRef_IsValid(outRef);
        if (valid == '\0') {
            result = 0;
        } else {
            struct FxBinding *obj = FxRef_Get(outRef);
            if (FxBinding_ResolveFrame(obj) == 0) {
                result = 0;
            } else {
                result = 1;
            }
        }
    }
    return result;
}
