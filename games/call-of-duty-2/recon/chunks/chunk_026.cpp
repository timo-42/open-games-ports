#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_026_types.h"

/* raw 32-bit view of a float (integer moves keep signalling NaNs intact, unlike x87 fld/fstp) */
#define W32(x) (*(unsigned int *)&(x))

#define CROSS2(p, q, i, j) ((p)[i] * (q)[j] - (q)[i] * (p)[j])

// 0x080a79c6  size=164  FUN_080a79c6  status=translated  naming=inferred
// naming evidence: quaternion w^2+x^2+y^2+z^2 normaliser: ratio of vector part to total norm; no string, structure only
float Quat_VectorNormSqRatio(const float *q)
{
    /* x87: each product and the norm/reciprocal live in 32-bit stack slots in the original */
    volatile float xx = q[0] * q[0];
    volatile float yy = q[1] * q[1];
    volatile float zz = q[2] * q[2];
    volatile float normSq = xx + yy + zz + q[3] * q[3];
    volatile float result;
    if (normSq != 0.0f) {
        normSq = 1.0f / normSq;
        xx = xx * normSq;
        yy = yy * normSq;
        zz = zz * normSq;
        result = xx + yy + zz;
    } else {
        result = 0.0f;
    }
    return result;
}

// 0x080a7a6a  size=48  FUN_080a7a6a  status=translated  naming=unknown
// naming evidence: sin(deg*pi/180)^2 descriptive name
float Sin_DegreesSquared(float deg)
{
    double s = sin(deg * 0.017453292519943295);
    return (float)(s * s);
}

// 0x080a7a9a  size=62  FUN_080a7a9a  status=translated  naming=unknown
// naming evidence: combines quaternion b (via Quat_Conjugate) with a (Quat_Multiply) then Quat_VectorNormSqRatio; callee semantics unresolved
float Quat_RelativeVectorNormSq(const float *a, const float *b)
{
    float combined[4];
    float transformed[6];
    Quat_Conjugate(b, transformed);
    Quat_Multiply(a, transformed, combined);
    return Quat_VectorNormSqRatio(combined);
}

// 0x080a7ad8  size=115  FUN_080a7ad8  status=translated  naming=unknown
// naming evidence: atan2 of quaternion x/y terms scaled to degrees; descriptive name
float Quat_PlanarAngleDeg(const float *q)
{
    /* x87: xx, scale and the result are 32-bit stack slots in the original */
    volatile float xx = q[0] * q[0];
    volatile float scale = q[1] * q[1] + xx;
    volatile float result;
    scale = 2.0f / scale;
    double ang = atan2((double)(q[0] * q[1] * scale), (double)(1.0 - xx * scale));
    result = (float)(57.29577951308232 * ang);
    return result;
}

// 0x080a7c30  size=75  FUN_080a7c30  status=translated  naming=inferred
// naming evidence: axis-angle to quaternion: xyz = sin(half)*axis, w = cos(half)
void Quat_FromAxisAngleDeg(float deg, const float *axis, float *q)
{
    float sinHalf;
    deg = (float)(deg * 0.008726646259971648);
    SinCos_Float(deg, &sinHalf, q + 3);
    Vec3_Scale_080ab190(axis, sinHalf, q);
}

// 0x080a7e44  size=187  FUN_080a7e44  status=translated  naming=inferred
// naming evidence: projection matrix from fov x/y (degrees) and near distance; 0.9995003f epsilon factor; memset 0x40
void Matrix4_Perspective(float *m, float fovX, float fovY, float nearZ)
{
    float k = 0.9995003f;
    memset(m, 0, 0x40);
    m[0] = (float)(tan((90.0f - fovX * 0.5f) * 0.017453292519943295) * k);
    m[5] = (float)(tan((90.0f - fovY * 0.5f) * 0.017453292519943295) * k);
    m[10] = k;
    m[11] = 1.0f;
    m[14] = -nearZ * k;
}

// 0x080a7f00  size=109  FUN_080a7f00  status=translated  naming=inferred
// naming evidence: orthographic matrix: 2/w, 2/h, 0.5/depth, z offset 0.5
void Matrix4_Orthographic(float *m, float w, float h, float depth)
{
    memset(m, 0, 0x40);
    m[0] = 2.0f / w;
    m[5] = 2.0f / h;
    m[10] = 0.5f / depth;
    m[14] = 0.5f;
    m[15] = 1.0f;
}

// 0x080a7f6e  size=414  FUN_080a7f6e  status=translated  naming=inferred
// naming evidence: view matrix from origin and 3x3 axis (-axis1, axis2, axis0 columns)
void Matrix4_ViewFromAxis(float *m, const float *origin, const float *axis)
{
    /* Integer moves with a sign-bit xor (no x87 load, so signalling NaNs survive), as in the original. */
    volatile float t;
    W32(m[0]) = W32(((float *)axis)[3]) ^ 0x80000000u;
    W32(m[4]) = W32(((float *)axis)[4]) ^ 0x80000000u;
    W32(m[8]) = W32(((float *)axis)[5]) ^ 0x80000000u;
    t = origin[0] * m[0] + origin[1] * m[4] + origin[2] * m[8];
    W32(m[12]) = *(volatile unsigned int *)&t ^ 0x80000000u;
    W32(m[1]) = W32(((float *)axis)[6]);
    W32(m[5]) = W32(((float *)axis)[7]);
    W32(m[9]) = W32(((float *)axis)[8]);
    t = origin[0] * m[1] + origin[1] * m[5] + origin[2] * m[9];
    W32(m[13]) = *(volatile unsigned int *)&t ^ 0x80000000u;
    W32(m[2]) = W32(((float *)axis)[0]);
    W32(m[6]) = W32(((float *)axis)[1]);
    W32(m[10]) = W32(((float *)axis)[2]);
    t = origin[0] * m[2] + origin[1] * m[6] + origin[2] * m[10];
    W32(m[14]) = *(volatile unsigned int *)&t ^ 0x80000000u;
    m[3] = 0.0f;
    m[7] = 0.0f;
    m[11] = 0.0f;
    m[15] = 1.0f;
}

/* fistp WORD with truncation (RC=11b): out-of-range values and NaN produce the "integer indefinite" 0x8000,
 * unlike a 32-bit truncation followed by a narrowing cast. */
static inline short TruncToShortX87(double v)
{
    if (!(v > -32769.0 && v < 32768.0)) {
        return (short)0x8000;
    }
    return (short)(int)v;
}

// 0x080a810c  size=111  FUN_080a810c  status=translated  naming=inferred
// naming evidence: float rgb [0,1] to packed RGBA8 (alpha 0xff), truncating via fistp word
unsigned int Color3_ToRGBA8(float r, float g, float b)
{
    byte c[4];
    c[0] = (byte)TruncToShortX87((double)r * 255.0);
    c[1] = (byte)TruncToShortX87((double)g * 255.0);
    c[2] = (byte)TruncToShortX87((double)b * 255.0);
    c[3] = 0xff;
    return (unsigned int)c[0] | ((unsigned int)c[1] << 8) | ((unsigned int)c[2] << 16) | ((unsigned int)c[3] << 24);
}

// 0x080a817c  size=134  FUN_080a817c  status=translated  naming=inferred
// naming evidence: float rgba to packed RGBA8
unsigned int Color4_ToRGBA8(float r, float g, float b, float a)
{
    byte c[4];
    c[0] = (byte)TruncToShortX87((double)r * 255.0);
    c[1] = (byte)TruncToShortX87((double)g * 255.0);
    c[2] = (byte)TruncToShortX87((double)b * 255.0);
    c[3] = (byte)TruncToShortX87((double)a * 255.0);
    return (unsigned int)c[0] | ((unsigned int)c[1] << 8) | ((unsigned int)c[2] << 16) | ((unsigned int)c[3] << 24);
}

// 0x080a8202  size=173  FUN_080a8202  status=translated  naming=unknown
// naming evidence: divide vec3 by its max component (signed max, not abs); zero vec if max==0; returns max
float Vec3_DivideByMaxComponent(const float *in, float *out)
{
    float maxc = in[0];
    if (maxc < in[1])
        maxc = in[1];
    if (maxc < in[2])
        maxc = in[2];
    if (maxc == 0.0f) {
        Vec3_Clear_080ab08a(out);
    } else {
        out[0] = in[0] / maxc;
        out[1] = in[1] / maxc;
        out[2] = in[2] / maxc;
    }
    return maxc;
}

// 0x080a82b0  size=67  FUN_080a82b0  status=translated  naming=inferred
// naming evidence: Q3 AngleMod: (360/65536)*((int)(a*(65536/360)) & 65535); duplicate of 0x80a8432
float AngleMod_Dup(float a)
{
    return 0.0054931640625f * (float)(((int)(a * 182.04444885253906f)) & 0xffff);
}

// 0x080a82f4  size=112  FUN_080a82f4  status=translated  naming=inferred
// naming evidence: Q3 LerpAngle (shortest-path interpolation)
float LerpAngle(float from, float to, float frac)
{
    volatile float t = to;   /* x87: stack slots rounded to float, as in the original */
    volatile float result;
    if (180.0f < to - from)
        t = to - 360.0f;
    if (t - from < -180.0f)
        t = t + 360.0f;
    result = (t - from) * frac + from;
    return result;
}

// 0x080a8364  size=97  FUN_080a8364  status=translated  naming=inferred
// naming evidence: Q3 AngleSubtract
float AngleSubtract(float a, float b)
{
    volatile float d;   /* x87: d is a 32-bit stack slot, rounded on every step */
    for (d = a - b; 180.0f < d; d = d - 360.0f) {
    }
    for (; d < -180.0f; d = d + 360.0f) {
    }
    return d;
}

// 0x080a83c6  size=107  FUN_080a83c6  status=translated  naming=inferred
// naming evidence: Q3 AnglesSubtract (per component AngleSubtract)
void AnglesSubtract(const float *a, const float *b, float *out)
{
    float v;
    v = AngleSubtract(a[0], b[0]);
    out[0] = v;
    v = AngleSubtract(a[1], b[1]);
    out[1] = v;
    v = AngleSubtract(a[2], b[2]);
    out[2] = v;
}

// 0x080a8432  size=67  FUN_080a8432  status=translated  naming=inferred
// naming evidence: Q3 AngleMod
float AngleMod(float a)
{
    return 0.0054931640625f * (float)(((int)(a * 182.04444885253906f)) & 0xffff);
}

// 0x080a8476  size=65  FUN_080a8476  status=translated  naming=inferred
// naming evidence: Q3 AngleNormalize180 using AngleMod
float AngleNormalize180(float a)
{
    float r = AngleMod(a);
    if (180.0f < r)
        r = r - 360.0f;
    return r;
}

// 0x080a84b8  size=125  FUN_080a84b8  status=translated  naming=inferred
// naming evidence: loop-based normalise to [0,360)
float AngleNormalize360_Loop(float arg)
{
    volatile float a = arg;   /* x87: the original updates its float argument slot, rounding each step */
    float r;
    if (a < 0.0f) {
        do {
            a = a + 360.0f;
        } while (a < 0.0f);
        r = a;
    } else if (360.0f <= a) {
        do {
            a = a - 360.0f;
        } while (360.0f <= a);
        r = a;
    } else {
        r = a;
    }
    return r;
}

// 0x080a8536  size=133  FUN_080a8536  status=translated  naming=inferred
// naming evidence: loop-based normalise to (-180,180]
float AngleNormalize180_Loop(float arg)
{
    volatile float a = arg;   /* x87: the original updates its float argument slot, rounding each step */
    float r;
    if (a <= -180.0f) {
        do {
            a = a + 360.0f;
        } while (a <= -180.0f);
        r = a;
    } else if (180.0f < a) {
        do {
            a = a - 360.0f;
        } while (180.0f < a);
        r = a;
    } else {
        r = a;
    }
    return r;
}

// 0x080a85bc  size=22  FUN_080a85bc  status=translated  naming=inferred
// naming evidence: AngleNormalize180(a - b)
float AngleDelta(float a, float b)
{
    return AngleNormalize180(a - b);
}

// 0x080a85d2  size=142  FUN_080a85d2  status=translated  naming=unknown
// naming evidence: length of per-component max(|a|,|b|) vec3
float Vec3_MaxAbsLength(const float *a, const float *b)
{
    float tmp[7];
    int i;
    for (i = 0; i < 3; i = i + 1) {
        volatile float m = Float_Abs_080aaea4(a[i]);   /* x87: float stack slots */
        volatile float other = Float_Abs_080aaea4(b[i]);
        /* the original picks m only when m > other (NaN falls through to other) */
        tmp[i] = (m > other) ? (float)m : (float)other;
    }
    return Vec3_Length(tmp);
}

// 0x080a8660  size=142  FUN_080a8660  status=translated  naming=unknown
// naming evidence: 2D variant of Vec3_MaxAbsLength
float Vec2_MaxAbsLength(const float *a, const float *b)
{
    float tmp[3];
    int i;
    for (i = 0; i < 2; i = i + 1) {
        volatile float m = Float_Abs_080aaea4(a[i]);   /* x87: float stack slots */
        volatile float other = Float_Abs_080aaea4(b[i]);
        /* the original picks m only when m > other (NaN falls through to other) */
        tmp[i] = (m > other) ? (float)m : (float)other;
    }
    return Vec2_Length(tmp);
}

// 0x080a86ee  size=197  FUN_080a86ee  status=translated  naming=inferred
// naming evidence: expand mins/maxs by move delta (swept bounds)
void Bounds_ExpandByMove(float *mins, float *maxs, const float *delta)
{
    if (0.0f < delta[0])
        maxs[0] = maxs[0] + delta[0];
    else
        mins[0] = mins[0] + delta[0];
    if (0.0f < delta[1])
        maxs[1] = maxs[1] + delta[1];
    else
        mins[1] = mins[1] + delta[1];
    if (0.0f < delta[2])
        maxs[2] = maxs[2] + delta[2];
    else
        mins[2] = mins[2] + delta[2];
}

// 0x080a87b4  size=126  FUN_080a87b4  status=translated  naming=unknown
// naming evidence: if b.z-a.z delta below max(dx,dy), push a.z down and b.z up by half the shortfall
void Vec3Pair_PushZApart(float *a, float *b)
{
    float d[3];
    volatile float m;   /* x87: m and h are 32-bit stack slots in the original */
    volatile float h;
    Vec3_Sub(b, a, d);
    m = Float_Max_080aaede(d[0], d[1]);
    if (d[2] < m) {
        h = (m - d[2]) * 0.5f;
        a[2] = a[2] - h;
        b[2] = b[2] + h;
    }
}

// 0x080a8832  size=165  FUN_080a8832  status=translated  naming=unknown
// naming evidence: for x and y: if delta (b-a) is below z delta, move a up and b down by half the shortfall
void Vec3Pair_PullXYToZ(float *a, float *b)
{
    float d[3];
    volatile float h;
    Vec3_Sub(b, a, d);
    if (d[2] < d[0]) {
        h = (d[0] - d[2]) * 0.5f;
        a[0] = a[0] + h;
        b[0] = b[0] - h;
    }
    if (d[2] < d[1]) {
        h = (d[1] - d[2]) * 0.5f;
        a[1] = a[1] + h;
        b[1] = b[1] - h;
    }
}

// 0x080a88d8  size=84  FUN_080a88d8  status=translated  naming=inferred
// naming evidence: Q3 ClearBounds: mins=+131072, maxs=-131072
void Bounds3_Clear(float *mins, float *maxs)
{
    Vec3_Set(mins, 131072.0f, 131072.0f, 131072.0f);
    Vec3_Set(maxs, -131072.0f, -131072.0f, -131072.0f);
}

// 0x080a892c  size=66  FUN_080a892c  status=translated  naming=inferred
// naming evidence: 2D ClearBounds
void Bounds2_Clear(float *mins, float *maxs)
{
    Vec2_Set(mins, 131072.0f, 131072.0f);
    Vec2_Set(maxs, -131072.0f, -131072.0f);
}

// 0x080a896e  size=233  FUN_080a896e  status=translated  naming=inferred
// naming evidence: Q3 AddPointToBounds
void Bounds3_AddPoint(const float *p, float *mins, float *maxs)
{
    if (p[0] < mins[0])
        mins[0] = p[0];
    if (maxs[0] < p[0])
        maxs[0] = p[0];
    if (p[1] < mins[1])
        mins[1] = p[1];
    if (maxs[1] < p[1])
        maxs[1] = p[1];
    if (p[2] < mins[2])
        mins[2] = p[2];
    if (maxs[2] < p[2])
        maxs[2] = p[2];
}

// 0x080a8a58  size=149  FUN_080a8a58  status=translated  naming=inferred
// naming evidence: 2D AddPointToBounds
void Bounds2_AddPoint(const float *p, float *mins, float *maxs)
{
    if (p[0] < mins[0])
        mins[0] = p[0];
    if (maxs[0] < p[0])
        maxs[0] = p[0];
    if (p[1] < mins[1])
        mins[1] = p[1];
    if (maxs[1] < p[1])
        maxs[1] = p[1];
}

// 0x080a8aee  size=183  FUN_080a8aee  status=translated  naming=inferred
// naming evidence: point inside 3D bounds
int Bounds3_ContainsPoint(const float *p, const float *mins, const float *maxs)
{
    int r;
    if (p[0] < mins[0] || maxs[0] < p[0])
        r = 0;
    else if (p[1] < mins[1] || maxs[1] < p[1])
        r = 0;
    else if (p[2] < mins[2] || maxs[2] < p[2])
        r = 0;
    else
        r = 1;
    return r;
}

// 0x080a8ba6  size=124  FUN_080a8ba6  status=translated  naming=inferred
// naming evidence: point inside 2D bounds
int Bounds2_ContainsPoint(const float *p, const float *mins, const float *maxs)
{
    int r;
    if (p[0] < mins[0] || maxs[0] < p[0])
        r = 0;
    else if (p[1] < mins[1] || maxs[1] < p[1])
        r = 0;
    else
        r = 1;
    return r;
}

// 0x080a8c22  size=192  FUN_080a8c22  status=translated  naming=inferred
// naming evidence: 3D bounds overlap test
int Bounds3_Intersect(const float *mins1, const float *maxs1, const float *mins2, const float *maxs2)
{
    int r;
    if (maxs2[0] < mins1[0] || maxs1[0] < mins2[0])
        r = 0;
    else if (maxs2[1] < mins1[1] || maxs1[1] < mins2[1])
        r = 0;
    else if (maxs2[2] < mins1[2] || maxs1[2] < mins2[2])
        r = 0;
    else
        r = 1;
    return r;
}

// 0x080a8ce2  size=128  FUN_080a8ce2  status=translated  naming=inferred
// naming evidence: 2D bounds overlap test
int Bounds2_Intersect(const float *mins1, const float *maxs1, const float *mins2, const float *maxs2)
{
    int r;
    if (maxs2[0] < mins1[0] || maxs1[0] < mins2[0])
        r = 0;
    else if (maxs2[1] < mins1[1] || maxs1[1] < mins2[1])
        r = 0;
    else
        r = 1;
    return r;
}

// 0x080a8d62  size=198  FUN_080a8d62  status=translated  naming=inferred
// naming evidence: 3D overlap with margin added to the maxs of both boxes
int Bounds3_IntersectMargin(const float *mins1, const float *maxs1, const float *mins2, const float *maxs2, float margin)
{
    int r;
    if (maxs2[0] + margin < mins1[0] || maxs1[0] + margin < mins2[0])
        r = 0;
    else if (maxs2[1] + margin < mins1[1] || maxs1[1] + margin < mins2[1])
        r = 0;
    else if (maxs2[2] + margin < mins1[2] || maxs1[2] + margin < mins2[2])
        r = 0;
    else
        r = 1;
    return r;
}

// 0x080a8e28  size=132  FUN_080a8e28  status=translated  naming=inferred
// naming evidence: 2D overlap with margin
int Bounds2_IntersectMargin(const float *mins1, const float *maxs1, const float *mins2, const float *maxs2, float margin)
{
    int r;
    if (maxs2[0] + margin < mins1[0] || maxs1[0] + margin < mins2[0])
        r = 0;
    else if (maxs2[1] + margin < mins1[1] || maxs1[1] + margin < mins2[1])
        r = 0;
    else
        r = 1;
    return r;
}

// 0x080a8eac  size=233  FUN_080a8eac  status=translated  naming=inferred
// naming evidence: 3D bounds union into accumulator
void Bounds3_Union(const float *mins, const float *maxs, float *accMins, float *accMaxs)
{
    if (mins[0] < accMins[0])
        accMins[0] = mins[0];
    if (accMaxs[0] < maxs[0])
        accMaxs[0] = maxs[0];
    if (mins[1] < accMins[1])
        accMins[1] = mins[1];
    if (accMaxs[1] < maxs[1])
        accMaxs[1] = maxs[1];
    if (mins[2] < accMins[2])
        accMins[2] = mins[2];
    if (accMaxs[2] < maxs[2])
        accMaxs[2] = maxs[2];
}

// 0x080a8f96  size=149  FUN_080a8f96  status=translated  naming=inferred
// naming evidence: 2D bounds union into accumulator
void Bounds2_Union(const float *mins, const float *maxs, float *accMins, float *accMaxs)
{
    if (mins[0] < accMins[0])
        accMins[0] = mins[0];
    if (accMaxs[0] < maxs[0])
        accMaxs[0] = maxs[0];
    if (mins[1] < accMins[1])
        accMins[1] = mins[1];
    if (accMaxs[1] < maxs[1])
        accMaxs[1] = maxs[1];
}

// 0x080a902c  size=599  FUN_080a902c  status=translated  naming=inferred
// naming evidence: AABB (bounds[0..2]=mins,[3..5]=maxs) transformed by 3x3 axis + origin, sign-of-element selects min/max (Arvo method)
void Bounds3_TransformByAxis(const float *bounds, const float *origin, const float *axis, float *out)
{
    int i;
    int k;
    for (i = 0; i < 3; i = i + 1) {
        out[i] = origin[i];
        out[3 + i] = origin[i];
        for (k = 0; k < 3; k = k + 1) {
            const float *a = &axis[k * 3 + i];
            int neg = Float_Bits(a) < 0;
            float lo = neg ? bounds[k + 3] : bounds[k];
            float hi = neg ? bounds[k] : bounds[k + 3];
            out[i] = out[i] + lo * *a;
            out[3 + i] = out[3 + i] + hi * *a;
        }
    }
}

// 0x080a9284  size=78  FUN_080a9284  status=translated  naming=unknown
// naming evidence: transform bounds by axis/origin then union into accBounds
void Bounds3_TransformAndUnion(const float *bounds, const float *origin, const float *axis, float *accBounds)
{
    float tmp[6];
    Bounds3_TransformByAxis(bounds, origin, axis, tmp);
    Bounds3_Union(tmp, tmp + 3, accBounds, accBounds + 3);
}

// 0x080a92d2  size=119  FUN_080a92d2  status=translated  naming=inferred
// naming evidence: 3x3 identity
void Mat3_Identity_080a92d2(float *m)
{
    *(unsigned int *)&m[0] = 0x3f800000;
    *(unsigned int *)&m[1] = 0;
    *(unsigned int *)&m[2] = 0;
    *(unsigned int *)&m[3] = 0;
    *(unsigned int *)&m[4] = 0x3f800000;
    *(unsigned int *)&m[5] = 0;
    *(unsigned int *)&m[6] = 0;
    *(unsigned int *)&m[7] = 0;
    *(unsigned int *)&m[8] = 0x3f800000;
}

// 0x080a934a  size=74  FUN_080a934a  status=translated  naming=inferred
// naming evidence: copy 3x3 as three Vec3_Copy_080ab0d8
void Mat3_Copy(const float *src, float *dst)
{
    Vec3_Copy_080ab0d8(src, dst);
    Vec3_Copy_080ab0d8(src + 3, dst + 3);
    Vec3_Copy_080ab0d8(src + 6, dst + 6);
}

// 0x080a9394  size=143  FUN_080a9394  status=translated  naming=inferred
// naming evidence: 3x3 transpose with integer-bit copies
void Mat3_Transpose_080a9394(const float *in, float *out)
{
    const unsigned int *s = (const unsigned int *)in;
    unsigned int *d = (unsigned int *)out;
    d[0] = s[0];
    d[1] = s[3];
    d[2] = s[6];
    d[3] = s[1];
    d[4] = s[4];
    d[5] = s[7];
    d[6] = s[2];
    d[7] = s[5];
    d[8] = s[8];
}

// 0x080a9424  size=134  FUN_080a9424  status=translated  naming=unknown
// naming evidence: x*row0 + y*row1 + z*row2 of a 3x3 (row-major axis)
void Mat3_TransformComponents(const float *m, float x, float y, float z, float *out)
{
    out[0] = x * m[0] + y * m[3] + z * m[6];
    out[1] = x * m[1] + y * m[4] + z * m[7];
    out[2] = x * m[2] + y * m[5] + z * m[8];
}

// 0x080a94aa  size=72  FUN_080a94aa  status=translated  naming=inferred
// naming evidence: Q3 AnglesToAxis: forward, left (= zero - right), up
void AnglesToAxis(const float *angles, float *axis)
{
    float right[6];
    Angles_ToVectors(angles, axis, right, axis + 6);
    Vec3_Sub(vec3_origin, right, axis + 3);
}

// 0x080a94f2  size=101  FUN_080a94f2  status=translated  naming=inferred
// naming evidence: AnglesToAxis variant with up fixed to (0,0,1) (callee takes no up output)
void AnglesToAxis_ZUp(float yaw, float *axis)
{
    float right[6];
    Angle_YawToForwardRight(yaw, axis, right);
    *(unsigned int *)&axis[6] = 0;
    *(unsigned int *)&axis[7] = 0;
    *(unsigned int *)&axis[8] = 0x3f800000;
    Vec3_Sub(vec3_origin, right, axis + 3);
}

// 0x080a9558  size=324  FUN_080a9558  status=translated  naming=inferred
// naming evidence: axis to pitch/yaw/roll; roll from rotated left vector
void AxisToAngles(const float *axis, float *angles)
{
    float roll;
    float fv;
    float sn, cs;
    float ang;
    float v[3];
    Vec3_ToAngles(axis, angles);
    Vec3_Copy_080ab0d8(axis + 3, v);
    ang = (float)(-angles[1] * 0.017453292519943295);
    SinCos_Float(ang, &sn, &cs);
    fv = cs * v[0] - sn * v[1];
    v[1] = cs * v[1] + sn * v[0];
    ang = (float)(-angles[0] * 0.017453292519943295);
    SinCos_Float(ang, &sn, &cs);
    v[0] = cs * fv + sn * v[2];
    v[2] = cs * v[2] - sn * fv;
    roll = Vec3_ElevationDegreesRaw(v);
    if (v[1] < 0.0f) {
        if (roll < 0.0f)
            roll = roll + 180.0f;
        else
            roll = roll + -180.0f;
        angles[2] = roll;
    } else {
        angles[2] = -roll;
    }
}

// 0x080a969c  size=324  FUN_080a969c  status=translated  naming=inferred
// naming evidence: identical to AxisToAngles but uses Vec3_ToAnglesRaw for pitch/yaw
void AxisToAngles_Alt(const float *axis, float *angles)
{
    float roll;
    float fv;
    float sn, cs;
    float ang;
    float v[3];
    Vec3_ToAnglesRaw(axis, angles);
    Vec3_Copy_080ab0d8(axis + 3, v);
    ang = (float)(-angles[1] * 0.017453292519943295);
    SinCos_Float(ang, &sn, &cs);
    fv = cs * v[0] - sn * v[1];
    v[1] = cs * v[1] + sn * v[0];
    ang = (float)(-angles[0] * 0.017453292519943295);
    SinCos_Float(ang, &sn, &cs);
    v[0] = cs * fv + sn * v[2];
    v[2] = cs * v[2] - sn * fv;
    roll = Vec3_ElevationDegreesRaw(v);
    if (v[1] < 0.0f) {
        if (roll < 0.0f)
            roll = roll + 180.0f;
        else
            roll = roll + -180.0f;
        angles[2] = roll;
    } else {
        angles[2] = -roll;
    }
}

// 0x080a97e0  size=862  FUN_080a97e0  status=translated  naming=inferred
// naming evidence: intersection point of three planes (x,y,z,d rows) via Cramer's rule; fails when |det| < 0.001
bool Planes3_Intersect(const float *const *planes, float *out)
{
    const float *r0 = planes[0];
    const float *r1 = planes[1];
    const float *r2 = planes[2];
    float det = CROSS2(r1, r2, 1, 2) * r0[0] + CROSS2(r2, r0, 1, 2) * r1[0] + CROSS2(r0, r1, 1, 2) * r2[0];
    bool ok = 0.0010000000474974513 <= fabs((double)det);
    if (ok) {
        det = 1.0f / det;
        out[0] = (CROSS2(r1, r2, 1, 2) * r0[3] + CROSS2(r2, r0, 1, 2) * r1[3] + CROSS2(r0, r1, 1, 2) * r2[3]) * det;
        out[1] = (CROSS2(r1, r2, 2, 0) * r0[3] + CROSS2(r2, r0, 2, 0) * r1[3] + CROSS2(r0, r1, 2, 0) * r2[3]) * det;
        out[2] = (CROSS2(r1, r2, 0, 1) * r0[3] + CROSS2(r2, r0, 0, 1) * r1[3] + CROSS2(r0, r1, 0, 1) * r2[3]) * det;
    }
    return ok;
}

// 0x080a9b3e  size=418  FUN_080a9b3e  status=translated  naming=unknown
// naming evidence: snap vec to grid per component when within epsilon; accept only if still within epsilon of three planes
void Vec3_SnapToGridNearPlanes3(const float *const *planes, float *vec, float grid, float epsilon)
{
    float snapped[7];
    float rounded;
    float maxDist;
    float limit;
    float d;
    int i;
    for (i = 0; i < 3; i = i + 1) {
        int n = Float_RoundToInt(vec[i] / grid);
        rounded = (float)n * grid;
        if (Float_Abs_080aaea4(rounded - vec[i]) < epsilon)
            snapped[i] = rounded;
        else
            snapped[i] = vec[i];
    }
    if ((char)Vec3_Equal_080ab294(snapped, vec) == 0) {
        maxDist = 0.0f;
        limit = epsilon;
        for (i = 0; i < 3; i = i + 1) {
            d = Float_Abs_080aaea4(Vec3_Dot_080ab260(planes[i], snapped) - planes[i][3]);
            if (maxDist < d)
                maxDist = d;
            d = Float_Abs_080aaea4(Vec3_Dot_080ab260(planes[i], snapped) - planes[i][3]);
            if (limit < d)
                limit = d;
        }
        if (maxDist < limit)
            Vec3_Copy_080ab0d8(snapped, vec);
    }
}

// 0x080a9ce0  size=544  FUN_080a9ce0  status=translated  naming=unknown
// naming evidence: pull vec onto nearby planes (stride 16) then grid-snap like Vec3_SnapToGridNearPlanes3
void Vec3_SnapToGridNearPlanes(const float *planes, int numPlanes, float *vec, float grid, float epsilon)
{
    float snapped[6];
    float rounded;
    float maxDist;
    float limit;
    float d;
    int i;
    int j;
    for (j = 0; j < numPlanes; j = j + 1) {
        d = (float)(Vec3_Dot_080ab260(vec, planes + j * 4) - planes[j * 4 + 3]);
        if (d <= epsilon && -epsilon <= d)
            Vec3_MA(vec, -d, planes + j * 4, vec);
    }
    for (i = 0; i < 3; i = i + 1) {
        int n = Float_RoundToInt(vec[i] / grid);
        rounded = (float)n * grid;
        if (Float_Abs_080aaea4(rounded - vec[i]) < epsilon)
            snapped[i] = rounded;
        else
            snapped[i] = vec[i];
    }
    if ((char)Vec3_Equal_080ab294(snapped, vec) == 0) {
        maxDist = 0.0f;
        limit = epsilon;
        for (j = 0; j < numPlanes; j = j + 1) {
            d = Float_Abs_080aaea4((float)(Vec3_Dot_080ab260(planes + j * 4, snapped) - planes[j * 4 + 3]));
            if (maxDist < d)
                maxDist = d;
            d = Float_Abs_080aaea4((float)(Vec3_Dot_080ab260(planes + j * 4, snapped) - planes[j * 4 + 3]));
            if (limit < d)
                limit = d;
        }
        if (maxDist < limit)
            Vec3_Copy_080ab0d8(snapped, vec);
    }
}

// 0x080a9f00  size=329  FUN_080a9f00  status=translated  naming=inferred
// naming evidence: point inside convex polygon projected along dominant axes of normal
int Polygon_ContainsPoint(const float *verts, int numVerts, const float *normal, const float *point)
{
    volatile float w[2];   /* x87: float stack slots; every difference is rounded when stored */
    volatile float e[2];
    int axes[2];
    int axisB;
    int prev;
    int i;
    volatile float dotv;
    Vec3_DominantAxisOrder(normal, axes, &axisB);
    prev = numVerts + -1;
    i = 0;
    while (true) {
        if (numVerts <= i)
            return 1;
        e[0] = verts[i * 3 + axisB] - verts[prev * 3 + axisB];
        e[1] = verts[prev * 3 + axes[0]] - verts[i * 3 + axes[0]];
        w[0] = point[axes[0]] - verts[prev * 3 + axes[0]];
        w[1] = point[axisB] - verts[prev * 3 + axisB];
        dotv = Vec2_Dot_080ab03c((const float *)w, (const float *)e);   /* x87: stored to a float stack slot before the compare */
        if (dotv < 0.0f)
            break;
        prev = i;
        i = i + 1;
    }
    return 0;
}

// 0x080aa0e4  size=72  FUN_080aa0e4  status=translated  naming=inferred
// naming evidence: Q3 ProjectPointOnPlane-style: out = v - dot(normal,v)*normal
void Vec3_ProjectOnPlane(const float *v, const float *normal, float *out)
{
    float d = Vec3_Dot_080ab260(normal, v);
    Vec3_MA(v, -d, normal, out);
}

// 0x080aa12c  size=82  FUN_080aa12c  status=translated  naming=inferred
// naming evidence: Q3 SetPlaneSignbits (bit i set if normal[i] < 0, stored at +0x11)
void Plane_SetSignbits(struct cplane_s *p)
{
    int i;
    byte bits = 0;
    for (i = 0; i < 3; i = i + 1) {
        if (p->normal[i] < 0.0f)
            bits = bits | (byte)(1 << ((byte)i & 0x1f));
    }
    p->signbits = bits;
}

// 0x080aa17e  size=989  FUN_080aa17e  status=translated  naming=inferred
// naming evidence: Q3 BoxOnPlaneSide: 1=front, 2=back, 3=both; axial planes fast path, signbits switch otherwise
byte BoxOnPlaneSide(const float *mins, const float *maxs, const struct cplane_s *p)
{
    volatile float dist1;   /* x87: float stack slots in the original */
    volatile float dist2;
    byte sides;
    const float *n = p->normal;
    if (p->type < 3) {
        if (p->dist <= mins[p->type])
            sides = 1;
        else if (maxs[p->type] <= p->dist)
            sides = 2;
        else
            sides = 3;
    } else {
        switch (p->signbits) {
        case 0:
            dist1 = n[0] * maxs[0] + n[1] * maxs[1] + n[2] * maxs[2];
            dist2 = n[0] * mins[0] + n[1] * mins[1] + n[2] * mins[2];
            break;
        case 1:
            dist1 = n[0] * mins[0] + n[1] * maxs[1] + n[2] * maxs[2];
            dist2 = n[0] * maxs[0] + n[1] * mins[1] + n[2] * mins[2];
            break;
        case 2:
            dist1 = n[0] * maxs[0] + n[1] * mins[1] + n[2] * maxs[2];
            dist2 = n[0] * mins[0] + n[1] * maxs[1] + n[2] * mins[2];
            break;
        case 3:
            dist1 = n[0] * mins[0] + n[1] * mins[1] + n[2] * maxs[2];
            dist2 = n[0] * maxs[0] + n[1] * maxs[1] + n[2] * mins[2];
            break;
        case 4:
            dist1 = n[0] * maxs[0] + n[1] * maxs[1] + n[2] * mins[2];
            dist2 = n[0] * mins[0] + n[1] * mins[1] + n[2] * maxs[2];
            break;
        case 5:
            dist1 = n[0] * mins[0] + n[1] * maxs[1] + n[2] * mins[2];
            dist2 = n[0] * maxs[0] + n[1] * mins[1] + n[2] * maxs[2];
            break;
        case 6:
            dist1 = n[0] * maxs[0] + n[1] * mins[1] + n[2] * mins[2];
            dist2 = n[0] * mins[0] + n[1] * maxs[1] + n[2] * maxs[2];
            break;
        case 7:
            dist1 = n[0] * mins[0] + n[1] * mins[1] + n[2] * mins[2];
            dist2 = n[0] * maxs[0] + n[1] * maxs[1] + n[2] * maxs[2];
            break;
        default:
            dist2 = 0.0f;
            dist1 = 0.0f;
        }
        sides = (byte)(p->dist <= dist1);
        if (dist2 < p->dist)
            sides = sides | 2;
    }
    return sides;
}

// 0x080aa680  size=205  FUN_080aa680  status=translated  naming=unknown
// naming evidence: per axis where (a-origin) and (b-origin) share sign, accumulate min of squares; return threshold < sum
bool Vec3_SharedSideMinSqExceeds(const float *a, const float *b, const float *origin, float threshold)
{
    float da[7];
    float db[4];
    volatile float sum;     /* x87: float stack slots in the original */
    volatile float pa;
    volatile float pb;
    volatile float t;
    int i;
    Vec3_Sub(a, origin, da);
    Vec3_Sub(b, origin, db);
    sum = 0.0f;
    for (i = 0; i < 3; i = i + 1) {
        /* original: jae skips only when 0 >= da*db (ordered); a NaN product falls into the body */
        if (!(0.0f >= da[i] * db[i])) {
            pa = da[i] * da[i];
            pb = db[i] * db[i];
            if (pa > pb)
                t = sum + pb;
            else
                t = sum + pa;
            sum = t;
        }
    }
    return threshold < sum;
}

// 0x080aa74e  size=39  FUN_080aa74e  status=translated  naming=inferred
// naming evidence: floor(x+0.5) as float
float Float_Round(float x)
{
    double r = floor((double)(x + 0.5f));
    return (float)r;
}

// 0x080aa776  size=180  FUN_080aa776  status=translated  naming=unknown
// naming evidence: scale vec3 by 1/max component (signed max); (1,1,1) and return 0 if max==0; returns max
float Vec3_ScaleByInvMaxComponent(const float *in, float *out)
{
    float maxc = in[0];
    float ret;
    if (maxc < in[1])
        maxc = in[1];
    if (maxc < in[2])
        maxc = in[2];
    if (maxc == 0.0f) {
        *(unsigned int *)&out[2] = 0x3f800000;
        *(unsigned int *)&out[1] = 0x3f800000;
        *(unsigned int *)&out[0] = 0x3f800000;
        ret = 0.0f;
    } else {
        Vec3_Scale_080ab190(in, 1.0f / maxc, out);
        ret = maxc;
    }
    return ret;
}

// 0x080aa82a  size=329  FUN_080aa82a  status=translated  naming=unknown
// naming evidence: rotate vec by euler angles (degrees) one axis at a time using double sin/cos; skips zero angles
void Vec3_RotateByAngles(const float *v, const float *angles, float *out)
{
    double sinv;
    double cosv;
    double rad;
    int axisIdx;
    int pairs[8];
    float cur[4];
    float next[6];
    Vec3_Copy_080ab0d8(v, cur);
    Vec3_Copy_080ab0d8(cur, next);
    pairs[0] = 1;
    pairs[1] = 2;
    pairs[2] = 2;
    pairs[3] = 0;
    pairs[4] = 0;
    pairs[5] = 1;
    for (axisIdx = 0; axisIdx < 3; axisIdx = axisIdx + 1) {
        float a = angles[axisIdx];
        if (a != 0.0f) {
            rad = (double)((angles[axisIdx] * 3.141592653589793) / 180.0);
            SinCos_Double(rad, &sinv, &cosv);
            next[pairs[axisIdx * 2]] = cur[pairs[axisIdx * 2]] * cosv - cur[pairs[axisIdx * 2 + 1]] * sinv;
            next[pairs[axisIdx * 2 + 1]] = cur[pairs[axisIdx * 2 + 1]] * cosv + cur[pairs[axisIdx * 2]] * sinv;
        }
        Vec3_Copy_080ab0d8(next, cur);
    }
    Vec3_Copy_080ab0d8(next, out);
}

// 0x080aaa30  size=70  FUN_080aaa30  status=translated  naming=unknown
// naming evidence: forward vector of angles projected on plane(normal) then normalized; returns callee's result (type assumed float)
float AnglesToForward_ProjectedNormalized(float yaw, const float *normal)
{
    float projected[4];
    float forward[6];
    Angle_YawToForwardRight(yaw, forward, 0);
    Vec3_ProjectOnPlane(forward, normal, projected);
    return Vec3_ElevationDegrees(projected);
}

// 0x080aaa76  size=13  FUN_080aaa76  status=translated  naming=inferred
// naming evidence: seed MSVC-style LCG (0x343fd / 0x269ec3)
void Rand_SetSeed(unsigned int seed)
{
    g_randSeed = seed;
}

// 0x080aaa84  size=86  FUN_080aaa84  status=translated  naming=inferred
// naming evidence: float in [lo,hi) from LCG bits 17..31 / 32768
float Rand_Float(float lo, float hi)
{
    float r;
    g_randSeed = g_randSeed * 0x343fd + 0x269ec3;
    r = (float)(g_randSeed >> 0x11);
    return ((hi - lo) * r) / 32768.0f + lo;
}

// 0x080aaada  size=64  FUN_080aaada  status=translated  naming=inferred
// naming evidence: int in [lo,hi) from LCG
int Rand_Int(int lo, int hi)
{
    int r;
    g_randSeed = g_randSeed * 0x343fd + 0x269ec3;
    r = (int)(g_randSeed >> 0x11);
    return (((hi - lo) * r) >> 0xf) + lo;
}

// 0x080aab1a  size=33  FUN_080aab1a  status=translated  naming=unknown
// naming evidence: thin wrapper over Vec3_TransformByQuatTransform
void Vec3_TransformByQuatTransform_Wrap(const float *v, const struct QuatTransform_s *t, float *out)
{
    Vec3_TransformByQuatTransform(v, t, out);
}

// 0x080aab3c  size=409  FUN_080aab3c  status=translated  naming=inferred
// naming evidence: rotation matrix to quaternion (largest of four candidate vectors, normalised)
void Mat3_ToQuat(const float *m, float *quat)
{
    float q[16];
    volatile float best;   /* x87: float stack slot */
    int bestIdx;
    int i;
    float len;
    q[0] = m[5] - m[7];
    q[1] = m[6] - m[2];
    q[2] = m[1] - m[3];
    q[3] = m[0] + m[4] + m[8] + 1.0f;
    q[4] = ((m[0] - m[4]) - m[8]) + 1.0f;
    q[5] = m[3] + m[1];
    q[6] = m[6] + m[2];
    q[9] = ((m[4] - m[0]) - m[8]) + 1.0f;
    q[10] = m[7] + m[5];
    q[14] = ((m[8] - m[4]) - m[0]) + 1.0f;
    bestIdx = -1;
    best = 0.0f;
    W32(q[7]) = W32(q[0]);    /* integer moves in the original (signalling NaNs survive) */
    W32(q[8]) = W32(q[5]);
    W32(q[11]) = W32(q[1]);
    W32(q[12]) = W32(q[6]);
    W32(q[13]) = W32(q[10]);
    W32(q[15]) = W32(q[2]);
    for (i = 0; i < 4; i = i + 1) {
        volatile float n = Vec4_LengthSq(q + i * 4);
        /* original: jae skips when best >= n; a NaN n therefore replaces best */
        if (!(best >= n)) {
            bestIdx = i;
            best = n;
        }
    }
    len = sqrtf(best);
    Vec4_Scale(q + bestIdx * 4, 1.0f / len, quat);
}

// 0x080aadca  size=101  FUN_080aadca  status=translated  naming=unknown
// naming evidence: integer-bit rounding of a float's low mantissa bits (granularity 1<<bits) within tolerance; Ghidra showed it as float cast, disassembly shows int ops on bits
float Float_RoundMantissaBits(float v, int tolerance, byte bits)
{
    union { float f; int i; } u;
    int step = 1 << (bits & 0x1f);
    int low;
    u.f = v;
    low = (step - 1) & u.i;
    if (tolerance < low) {
        if (step - low <= tolerance)
            u.i = u.i + (step - low);
    } else {
        u.i = u.i - low;
    }
    return u.f;
}

// 0x080aae30  size=115  FUN_080aae30  status=translated  naming=unknown
// naming evidence: snap float to multiples of 1/step if within tolerance after mantissa rounding (4, 12 bits)
float Float_SnapToStep(float v, float step, float tolerance)
{
    volatile float r = Float_RoundMantissaBits(v, 4, 0xc);   /* x87: stored to the float argument slot */
    int n = Float_RoundToInt(r * step);
    volatile float result = (float)n / step;
    volatile float err = Float_Abs_080aaea4(result - r);
    /* original: jae keeps the snapped value only when tolerance >= err (a NaN err falls back to r) */
    if (!(tolerance >= err))
        result = r;
    return result;
}

// 0x080aaea4  size=25  FUN_080aaea4  status=translated  naming=inferred
// naming evidence: fabsf
float Float_Abs_080aaea4(float x)
{
    return fabsf(x);
}

// 0x080aaebe  size=31  FUN_080aaebe  status=translated  naming=inferred
// naming evidence: (float)sqrt((double)x)
float Float_Sqrt_080aaebe(float x)
{
    return (float)sqrt((double)x);
}

// 0x080aaede  size=36  FUN_080aaede  status=translated  naming=inferred
// naming evidence: max(a,b) via Float_Select(a-b,a,b)
float Float_Max_080aaede(float a, float b)
{
    return Float_Select(a - b, a, b);
}

// 0x080aaf02  size=61  FUN_080aaf02  status=translated  naming=unknown
// naming evidence: nested Float_Select; returns b if b>=a else min(a,c)
float Float_Select2(float a, float b, float c)
{
    float r = Float_Select(a - c, c, a);
    return Float_Select(b - a, b, r);
}

// 0x080aaf40  size=10  FUN_080aaf40  status=translated  naming=unknown
// naming evidence: reads float bits as int
int Float_Bits(const float *p)
{
    return *(const int *)p;
}

// 0x080aaf4a  size=54  FUN_080aaf4a  status=translated  naming=inferred
// naming evidence: (int)floor(x+0.5)
int Float_RoundToInt(float x)
{
    double r = floor((double)(x + 0.5f));
    return (int)r;
}

// 0x080aaf80  size=45  FUN_080aaf80  status=translated  naming=inferred
// naming evidence: sin/cos of float angle (radians) in double, results stored as float
void SinCos_Float(float ang, float *s, float *c)
{
    *s = (float)sin((double)ang);
    *c = (float)cos((double)ang);
}

// 0x080aafae  size=46  FUN_080aafae  status=translated  naming=inferred
// naming evidence: sin/cos in double
void SinCos_Double(double ang, double *s, double *c)
{
    *s = sin(ang);
    *c = cos(ang);
}

// 0x080aafdc  size=28  FUN_080aafdc  status=translated  naming=inferred
// naming evidence: vec2 clear
void Vec2_Clear(float *v)
{
    *(unsigned int *)&v[0] = 0;
    *(unsigned int *)&v[1] = 0;
}

// 0x080aaff8  size=24  FUN_080aaff8  status=translated  naming=inferred
// naming evidence: vec2 set (raw 4-byte copies)
void Vec2_Set(float *v, float x, float y)
{
    W32(v[0]) = W32(x);   /* integer moves in the original (signalling NaNs survive) */
    W32(v[1]) = W32(y);
}

// 0x080ab010  size=44  FUN_080ab010  status=translated  naming=inferred
// naming evidence: vec2 subtract
void Vec2_Sub(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

// 0x080ab03c  size=33  FUN_080ab03c  status=translated  naming=inferred
// naming evidence: vec2 dot
float Vec2_Dot_080ab03c(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1];
}

// 0x080ab05e  size=44  FUN_080ab05e  status=translated  naming=inferred
// naming evidence: vec2 length
float Vec2_Length(const float *v)
{
    return Float_Sqrt_080aaebe(v[0] * v[0] + v[1] * v[1]);
}

// 0x080ab08a  size=41  FUN_080ab08a  status=translated  naming=inferred
// naming evidence: vec3 clear
void Vec3_Clear_080ab08a(float *v)
{
    *(unsigned int *)&v[0] = 0;
    *(unsigned int *)&v[1] = 0;
    *(unsigned int *)&v[2] = 0;
}

// 0x080ab0b4  size=35  FUN_080ab0b4  status=translated  naming=inferred
// naming evidence: vec3 set
void Vec3_Set(float *v, float x, float y, float z)
{
    W32(v[0]) = W32(x);   /* integer moves in the original (signalling NaNs survive) */
    W32(v[1]) = W32(y);
    W32(v[2]) = W32(z);
}

// 0x080ab0d8  size=47  FUN_080ab0d8  status=translated  naming=inferred
// naming evidence: vec3 copy (integer-bit copy)
void Vec3_Copy_080ab0d8(const float *in, float *out)
{
    *(unsigned int *)&out[0] = *(const unsigned int *)&in[0];
    *(unsigned int *)&out[1] = *(const unsigned int *)&in[1];
    *(unsigned int *)&out[2] = *(const unsigned int *)&in[2];
}

// 0x080ab108  size=68  FUN_080ab108  status=translated  naming=inferred
// naming evidence: vec3 add
void Vec3_Add_080ab108(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x080ab14c  size=68  FUN_080ab14c  status=translated  naming=inferred
// naming evidence: vec3 subtract
void Vec3_Sub(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080ab190  size=56  FUN_080ab190  status=translated  naming=inferred
// naming evidence: vec3 scale
void Vec3_Scale_080ab190(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x080ab1c8  size=68  FUN_080ab1c8  status=translated  naming=inferred
// naming evidence: vec3 componentwise multiply
void Vec3_Mul(const float *a, const float *b, float *out)
{
    out[0] = a[0] * b[0];
    out[1] = a[1] * b[1];
    out[2] = a[2] * b[2];
}

// 0x080ab20c  size=83  FUN_080ab20c  status=translated  naming=inferred
// naming evidence: Q3 VectorMA: out = base + s*dir
void Vec3_MA(const float *base, float s, const float *dir, float *out)
{
    out[0] = s * dir[0] + base[0];
    out[1] = s * dir[1] + base[1];
    out[2] = s * dir[2] + base[2];
}

// 0x080ab260  size=51  FUN_080ab260  status=translated  naming=inferred
// naming evidence: vec3 dot (z,y,x summation order)
float Vec3_Dot_080ab260(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// 0x080ab294  size=106  FUN_080ab294  status=translated  naming=inferred
// naming evidence: exact vec3 equality
int Vec3_Equal_080ab294(const float *a, const float *b)
{
    int r = 0;
    if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2])
        r = 1;
    return r;
}

// 0x080ab2fe  size=62  FUN_080ab2fe  status=translated  naming=inferred
// naming evidence: vec3 length
float Vec3_Length(const float *v)
{
    return Float_Sqrt_080aaebe(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// 0x080ab33c  size=75  FUN_080ab33c  status=translated  naming=inferred
// naming evidence: vec4 scale
void Vec4_Scale(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
    out[3] = s * v[3];
}

// 0x080ab388  size=69  FUN_080ab388  status=translated  naming=inferred
// naming evidence: vec4 squared length
float Vec4_LengthSq(const float *v)
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
}

// 0x080ab3ce  size=41  FUN_080ab3ce  status=translated  naming=inferred
// naming evidence: Q_fsel-style select: cond >= 0 ? a : b
float Float_Select(float cond, float a, float b)
{
    float r;
    if (0.0f <= cond)
        r = a;
    else
        r = b;
    return r;
}

// 0x080ab3f8  size=167  FUN_080ab3f8  status=translated  naming=inferred
// naming evidence: v transformed by quat->mat3 (columns as read here) plus origin
void Vec3_TransformByQuatTransform(const float *v, const struct QuatTransform_s *t, float *out)
{
    float m[9];
    QuatTransform_ToMat3(t, m);
    out[0] = v[0] * m[0] + v[1] * m[3] + v[2] * m[6] + t->origin[0];
    out[1] = v[0] * m[1] + v[1] * m[4] + v[2] * m[7] + t->origin[1];
    out[2] = v[0] * m[2] + v[1] * m[5] + v[2] * m[8] + t->origin[2];
}

// 0x080ab4a0  size=278  FUN_080ab4a0  status=translated  naming=inferred
// naming evidence: quaternion (x,y,z,w) with scale factor at +0x1c to 3x3 rotation
void QuatTransform_ToMat3(const struct QuatTransform_s *t, float *m)
{
    float s[3];
    /* x87: the nine products are 32-bit stack slots in the original (names: s<i> * quat[j]) */
    volatile float s0q0, s0q1, s0q2, s0q3, s1q1, s1q2, s1q3, s2q2, s2q3;
    Vec3_Scale_080ab190(t->quat, t->quatScale, s);
    s0q0 = s[0] * t->quat[0];
    s0q1 = s[0] * t->quat[1];
    s0q2 = s[0] * t->quat[2];
    s0q3 = s[0] * t->quat[3];
    s1q1 = s[1] * t->quat[1];
    s1q2 = s[1] * t->quat[2];
    s1q3 = s[1] * t->quat[3];
    s2q2 = s[2] * t->quat[2];
    s2q3 = s[2] * t->quat[3];
    m[0] = 1.0f - (s1q1 + s2q2);
    m[1] = s0q1 + s2q3;
    m[2] = s0q2 - s1q3;
    m[3] = s0q1 - s2q3;
    m[4] = 1.0f - (s0q0 + s2q2);
    m[5] = s1q2 + s0q3;
    m[6] = s0q2 + s1q3;
    m[7] = s1q2 - s0q3;
    m[8] = 1.0f - (s0q0 + s1q1);
}

// 0x080ab5b8  size=19  FUN_080ab5b8  status=translated  naming=inferred
// naming evidence: free() wrapper (one of three identical)
void Com_Free_A(void *p)
{
    free(p);
}

// 0x080ab5cc  size=19  FUN_080ab5cc  status=translated  naming=inferred
// naming evidence: free() wrapper (one of three identical)
void Com_Free_B(void *p)
{
    free(p);
}

// 0x080ab5e0  size=19  FUN_080ab5e0  status=translated  naming=inferred
// naming evidence: free() wrapper (one of three identical)
void Com_Free_C(void *p)
{
    free(p);
}

// 0x080ab5f4  size=28  FUN_080ab5f4  status=translated  naming=observed
// naming evidence: calls error handler with "universal/com_memory.cpp", line 0xe4 (string observed)
void Com_AllocFailed_228(void)
{
    Sys_OutOfMemory("universal/com_memory.cpp", 0xe4);
}

// 0x080ab610  size=19  FUN_080ab610  status=translated  naming=inferred
// naming evidence: malloc wrapper
void *Com_Malloc_Raw(unsigned int size)
{
    return malloc(size);
}

// 0x080ab624  size=57  FUN_080ab624  status=translated  naming=inferred
// naming evidence: malloc then zero-fill via Com_Memset (memset-like)
void *Com_MallocZeroed_Raw(unsigned int size)
{
    void *p = Com_Malloc_Raw(size);
    if (p != 0)
        Com_Memset(p, 0, size);
    return p;
}

// 0x080ab65e  size=42  FUN_080ab65e  status=translated  naming=inferred
// naming evidence: zeroed malloc, calls Com_AllocFailed_228 on NULL (original passes size as ignored extra stack arg)
void *Com_MallocZeroed(unsigned int size)
{
    void *p = Com_MallocZeroed_Raw(size);
    if (p == 0)
        Com_AllocFailed_228();
    return p;
}

// 0x080ab688  size=42  FUN_080ab688  status=translated  naming=inferred
// naming evidence: malloc, calls Com_AllocFailed_228 on NULL
void *Com_Malloc(unsigned int size)
{
    void *p = Com_Malloc_Raw(size);
    if (p == 0)
        Com_AllocFailed_228();
    return p;
}

// 0x080ab6b2  size=27  FUN_080ab6b2  status=translated  naming=inferred
// naming evidence: calloc(1,size) wrapper
void *Com_Calloc_Raw(unsigned int size)
{
    return calloc(1, size);
}

// 0x080ab6ce  size=51  FUN_080ab6ce  status=translated  naming=observed
// naming evidence: calloc; on NULL calls error handler with "universal/com_memory.cpp", line 0x218 (string observed)
void *Com_Calloc(unsigned int size)
{
    void *p = Com_Calloc_Raw(size);
    if (p == 0)
        Sys_OutOfMemory("universal/com_memory.cpp", 0x218);
    return p;
}
