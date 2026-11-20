#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_034_types.h"

/*
 * Chunk 034: float/vec3 helpers, XAnim loader, XModelParts / XModel loaders and
 * XModel collision tracing. Dedicated-server build: the xmodelsurfs loader is a stub.
 * Precision note: the original is x87 code; float expressions here are meant to be
 * evaluated with the same excess precision (-m32, x87), rounding only on stores.
 * The xanim key-list reading that the original repeats inline inside 0x080c3f8a is
 * factored into the static helpers XAnim_AllocKeyTable / XAnim_ReadRotKeys /
 * XAnim_ReadTransKeys; allocation and read order are unchanged.
 */

/* ======================================================================== */
/* small helpers                                                            */
/* ======================================================================== */

// 0x080c3be4  size=171  FUN_080c3be4  status=translated  naming=unknown
// Behaviour only: out[i] = base[i] + Lerp_Float_080c3ce2(a[i], b[i], t) * scale.
void Vec3_AddScaledLerp(const float *base, float scale, const float *a, const float *b, float t, float *out)
{
    out[0] = base[0] + Lerp_Float_080c3ce2(a[0], b[0], t) * scale;
    out[1] = base[1] + Lerp_Float_080c3ce2(a[1], b[1], t) * scale;
    out[2] = base[2] + Lerp_Float_080c3ce2(a[2], b[2], t) * scale;
}

// 0x080c3c90  size=31  FUN_080c3c90  status=translated  naming=unknown
// Behaviour only: converts two int16 to two floats.
void Short2_ToFloat2(const short *src, float *dst)
{
    dst[0] = (float)src[0];
    dst[1] = (float)src[1];
}

// 0x080c3cb0  size=50  FUN_080c3cb0  status=translated  naming=unknown
// Behaviour only: a + (b - a) * t on int16 endpoints; result left in extended precision (st0).
long double Lerp_Short(short a, short b, float t)
{
    return ((long double)((int)b - (int)a)) * (long double)t + (long double)a;
}

// 0x080c3ce2  size=17  FUN_080c3ce2  status=translated  naming=unknown
// Behaviour only: a + (b - a) * t.
long double Lerp_Float_080c3ce2(float a, float b, float t)
{
    return (((long double)b - (long double)a) * (long double)t) + (long double)a;
}

// 0x080c522a  size=31  FUN_080c522a  status=translated  naming=unknown
// Behaviour only: (float)sqrt((double)x).
float Float_Sqrt_080c522a(float x)
{
    double r = sqrt((double)x);
    return (float)r;
}

// 0x080c524a  size=47  FUN_080c524a  status=translated  naming=unknown
// Behaviour only: copies three 32-bit words.
void Vec3_CopyBits_080c524a(const float *src, float *dst)
{
    uint32 *d = (uint32 *)dst;
    const uint32 *s = (const uint32 *)src;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

// 0x080c527a  size=53  FUN_080c527a  status=translated  naming=unknown
// Behaviour only: reads an int16 from the cursor (memcpy) and advances it by 2.
short ByteCursor_ReadInt16(byte **cursor)
{
    short value;
    memcpy(&value, *cursor, 2);
    *cursor += 2;
    return value;
}

// 0x080c52b0  size=53  FUN_080c52b0  status=translated  naming=unknown
// Behaviour only: reads a uint16 from the cursor and advances it by 2.
uint16 ByteCursor_ReadUInt16(byte **cursor)
{
    uint16 value;
    memcpy(&value, *cursor, 2);
    *cursor += 2;
    return value;
}

// 0x080c52e6  size=58  FUN_080c52e6  status=translated  naming=unknown
// Behaviour only: reads a float from the cursor and advances it by 4.
float ByteCursor_ReadFloat32(byte **cursor)
{
    float value;
    memcpy(&value, *cursor, 4);
    *cursor += 4;
    return value;
}

// 0x080c79bc  size=52  FUN_080c79bc  status=translated  naming=unknown
// Behaviour only: reads a 32-bit word from the cursor and advances it by 4.
uint32 ByteCursor_ReadUInt32(byte **cursor)
{
    uint32 value;
    memcpy(&value, *cursor, 4);
    *cursor += 4;
    return value;
}

// 0x080c5e4c  size=10  FUN_080c5e4c  status=translated  naming=unknown
// Behaviour only: returns 0, no arguments.
int Stub_ReturnZero_080c5e4c(void)
{
    return 0;
}

// 0x080c5e56  size=14  FUN_080c5e56  status=translated  naming=unknown
// Behaviour only: empty function.
void Stub_Nop_080c5e56(void)
{
}

// 0x080c5e64  size=36  FUN_080c5e64  status=translated  naming=unknown
// Behaviour only: a - b >= 0 ? a : b, i.e. max(a, b) (NaN yields b).
float Float_Max_080c5e64(float a, float b)
{
    return Float_SelectNonNegative_080c5f84(a - b, a, b);
}

// 0x080c5e88  size=47  FUN_080c5e88  status=translated  naming=unknown
// Behaviour only: copies three 32-bit words (src -> dst).
void Vec3_CopyB(const float *src, float *dst)
{
    uint32 *d = (uint32 *)dst;
    const uint32 *s = (const uint32 *)src;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

// 0x080c5eb8  size=68  FUN_080c5eb8  status=translated  naming=unknown
// Behaviour only: out = a - b (vec3).
void Vec3_Sub_080c5eb8(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080c5efc  size=83  FUN_080c5efc  status=translated  naming=unknown
// Behaviour only: out = base + scale * dir (vec3 multiply-add).
void Vec3_MA_080c5efc(const float *base, float scale, const float *dir, float *out)
{
    out[0] = base[0] + scale * dir[0];
    out[1] = base[1] + scale * dir[1];
    out[2] = base[2] + scale * dir[2];
}

// 0x080c5f50  size=51  FUN_080c5f50  status=translated  naming=unknown
// Behaviour only: dot product, summed z + y + x.
long double Vec3_Dot_080c5f50(const float *a, const float *b)
{
    return a[2] * b[2] + a[1] * b[1] + a[0] * b[0];
}

// 0x080c5f84  size=41  FUN_080c5f84  status=translated  naming=unknown
// Behaviour only: cond >= 0 ? a : b.
float Float_SelectNonNegative_080c5f84(float cond, float a, float b)
{
    float result;
    if (0.0f <= cond) {
        result = a;
    } else {
        result = b;
    }
    return result;
}

// 0x080c5fae  size=56  FUN_080c5fae  status=translated  naming=unknown
// Behaviour only: out = scale * v (vec3).
void Vec3_Scale_080c5fae(const float *v, float scale, float *out)
{
    out[0] = scale * v[0];
    out[1] = scale * v[1];
    out[2] = scale * v[2];
}

// 0x080c5fe6  size=170  FUN_080c5fe6  status=translated  naming=unknown
// Behaviour only: out[j] = sum_i v[i] * axis[i*3 + j] (multiply by the transposed 3x3).
void Vec3_RotateTransposed(const float *v, const float *axis9, float *out)
{
    out[0] = v[2] * axis9[6] + v[1] * axis9[3] + v[0] * axis9[0];
    out[1] = v[2] * axis9[7] + v[1] * axis9[4] + v[0] * axis9[1];
    out[2] = v[2] * axis9[8] + v[1] * axis9[5] + v[0] * axis9[2];
}

// 0x080c6090  size=170  FUN_080c6090  status=translated  naming=unknown
// Behaviour only: out[j] = dot(v, axis row j) (multiply by the 3x3).
void Vec3_Rotate(const float *v, const float *axis9, float *out)
{
    out[0] = v[2] * axis9[2] + v[1] * axis9[1] + v[0] * axis9[0];
    out[1] = v[2] * axis9[5] + v[1] * axis9[4] + v[0] * axis9[3];
    out[2] = v[2] * axis9[8] + v[1] * axis9[7] + v[0] * axis9[6];
}

// 0x080c77cc  size=31  FUN_080c77cc  status=translated  naming=unknown
// Behaviour only: second copy of Float_Sqrt_080c522a, (float)sqrt((double)x).
float Float_Sqrt_080c77cc(float x)
{
    double r = sqrt((double)x);
    return (float)r;
}

// 0x080c77ec  size=41  FUN_080c77ec  status=translated  naming=unknown
// Behaviour only: zeroes three 32-bit words.
void Vec3_Clear_080c77ec(float *v)
{
    uint32 *w = (uint32 *)v;
    w[0] = 0;
    w[1] = 0;
    w[2] = 0;
}

// 0x080c7816  size=35  FUN_080c7816  status=translated  naming=unknown
// Behaviour only: v = (x, y, z). Callers pass raw bit patterns (FLT_MAX / -FLT_MAX).
void Vec3_Set_080c7816(float *v, float x, float y, float z)
{
    /* raw 32-bit moves (no x87 load/store, so signalling-NaN payloads survive) */
    memcpy(&v[0], &x, 4);
    memcpy(&v[1], &y, 4);
    memcpy(&v[2], &z, 4);
}

// 0x080c783a  size=47  FUN_080c783a  status=translated  naming=unknown
// Behaviour only: copies three 32-bit words (src -> dst).
void Vec3_CopyBits_080c783a(const float *src, float *dst)
{
    uint32 *d = (uint32 *)dst;
    const uint32 *s = (const uint32 *)src;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

// 0x080c786a  size=68  FUN_080c786a  status=translated  naming=unknown
// Behaviour only: out = a - b (vec3), second copy.
void Vec3_Sub_080c786a(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080c78ae  size=92  FUN_080c78ae  status=translated  naming=unknown
// Behaviour only: out = (a + b) * 0.5. The constant 0.5 is a double in the binary.
void Vec3_Midpoint(const float *a, const float *b, float *out)
{
    out[0] = (a[0] + b[0]) * 0.5;
    out[1] = (a[1] + b[1]) * 0.5;
    out[2] = (a[2] + b[2]) * 0.5;
}

// 0x080c790a  size=51  FUN_080c790a  status=translated  naming=unknown
// Behaviour only: squared length, summed z + y + x.
long double Vec3_LengthSq(const float *v)
{
    return v[2] * v[2] + v[1] * v[1] + v[0] * v[0];
}

// 0x080c793e  size=69  FUN_080c793e  status=translated  naming=unknown
// Behaviour only: squared length of a 4-vector, summed w + z + y + x.
long double Vec4_LengthSq_080c793e(const float *v)
{
    return v[3] * v[3] + v[2] * v[2] + v[1] * v[1] + v[0] * v[0];
}

// 0x080c7984  size=56  FUN_080c7984  status=translated  naming=unknown
// Behaviour only: out = scale * v (vec3), second copy.
void Vec3_Scale_080c7984(const float *v, float scale, float *out)
{
    out[0] = scale * v[0];
    out[1] = scale * v[1];
    out[2] = scale * v[2];
}

/* ======================================================================== */
/* xanim loader                                                             */
/* ======================================================================== */

// 0x080c3cf4  size=225  FUN_080c3cf4  status=translated  naming=unknown
// Reads x, y, z int16 and reconstructs w = round(sqrt(32767^2 - x^2 - y^2 - z^2)), 0 when not positive.
void XAnim_ReadQuat4(byte **cursor, short *out)
{
    short w;
    int remaining;

    out[0] = ByteCursor_ReadInt16(cursor);
    out[1] = ByteCursor_ReadInt16(cursor);
    out[2] = ByteCursor_ReadInt16(cursor);
    remaining = 0x3fff0001 - ((int)out[0] * (int)out[0] + (int)out[1] * (int)out[1] + (int)out[2] * (int)out[2]);
    if (remaining < 1) {
        w = 0;
    } else {
        float root = Float_Sqrt_080c522a((float)remaining);
        double rounded = floor((double)(0.5L + (long double)root));
        w = (short)(int)rounded;
    }
    out[3] = w;
}

// 0x080c3dd6  size=139  FUN_080c3dd6  status=translated  naming=unknown
// Reads x int16 and reconstructs y = round(sqrt(32767^2 - x^2)), 0 when not positive.
void XAnim_ReadQuat2(byte **cursor, short *out)
{
    short y;
    int remaining;

    out[0] = ByteCursor_ReadInt16(cursor);
    remaining = 0x3fff0001 - (int)out[0] * (int)out[0];
    if (remaining < 1) {
        y = 0;
    } else {
        float root = Float_Sqrt_080c522a((float)remaining);
        double rounded = floor((double)(0.5L + (long double)root));
        y = (short)(int)rounded;
    }
    out[1] = y;
}

// 0x080c3e62  size=295  FUN_080c3e62  status=translated  naming=unknown
// Loads the notify list of an xanim: count byte, then (name string, uint16 frame) pairs,
// followed by an implicit "end" notify at time 1.0. Times are frame / numFrames.
void XAnim_LoadNotifies(const char *name, byte **cursor, XAnimParts *parts, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    uint8 count;
    XAnimNotify *notify;
    int i;

    count = **cursor;
    *cursor += 1;
    parts->numNotifies = (uint8)(count + 1);
    notify = (XAnimNotify *)alloc((uint32)parts->numNotifies << 3);
    parts->notifies = notify;
    for (i = 0; i < (int)(uint32)count; i++) {
        uint16 frame;
        float time;

        notify->name = ScrStr_InternCStringEx((const char *)*cursor, 0, 3);
        *cursor += strlen((const char *)*cursor) + 1;
        frame = ByteCursor_ReadUInt16(cursor);
        if (parts->numFrames == 0) {
            time = 0.0f;
        } else {
            time = (float)frame / (float)parts->numFrames;
        }
        notify->time = time;
        notify++;
    }
    notify->name = ScrStr_InternCStringEx("end", 0, 3);
    notify->time = 1.0f;
}

/* Part of 0x080c3f8a: allocates a key-list header and, when the list does not cover every
 * frame (count < numFrames), copies the uint8/uint16 frame-index table that follows. */
static xanim_track_s *XAnim_AllocKeyTable(byte **cursor, uint16 count, uint16 numFrames, int smallIdx, XAllocFn alloc)
{
    xanim_track_s *keys;

    if (count < numFrames) {
        if (!smallIdx) {
            keys = (xanim_track_s *)alloc((uint32)count * 2 + 8);
            memcpy(keys->frameIndex, *cursor, (uint32)count * 2);
            *cursor += (uint32)count * 2;
        } else {
            keys = (xanim_track_s *)alloc((uint32)count + 8);
            memcpy(keys->frameIndex, *cursor, (uint32)count);
            *cursor += (uint32)count;
        }
    } else {
        keys = (xanim_track_s *)alloc(8);
    }
    return keys;
}

/* Part of 0x080c3f8a: translation key list (also used for the delta track). */
static xanim_track_s *XAnim_ReadTransKeys(byte **cursor, uint16 count, uint16 numFrames, int smallIdx, XAllocFn alloc)
{
    xanim_track_s *keys;
    int i;

    if (count == 0) {
        return 0;
    }
    if (count == 1) {
        float v[3];
        v[0] = ByteCursor_ReadFloat32(cursor);
        v[1] = ByteCursor_ReadFloat32(cursor);
        v[2] = ByteCursor_ReadFloat32(cursor);
        keys = (xanim_track_s *)alloc(0x10);
        keys->lastIndex = 0;
        Vec3_CopyBits_080c524a(v, (float *)((byte *)keys + 4));
        return keys;
    }
    keys = XAnim_AllocKeyTable(cursor, count, numFrames, smallIdx, alloc);
    keys->lastIndex = (uint16)(count - 1);
    keys->keys = alloc((uint32)count * 0xc);
    for (i = 0; i < (int)(uint32)count; i++) {
        float *key = (float *)((byte *)keys->keys + i * 0xc);
        key[0] = ByteCursor_ReadFloat32(cursor);
        key[1] = ByteCursor_ReadFloat32(cursor);
        key[2] = ByteCursor_ReadFloat32(cursor);
    }
    return keys;
}

/* Part of 0x080c3f8a: rotation key list. twoD selects 2-short keys (rotation about one axis),
 * otherwise 4-short keys. negateFirst negates the first key (bone track bit; never set for delta).
 * Consecutive keys are flipped into the same hemisphere. */
static xanim_track_s *XAnim_ReadRotKeys(byte **cursor, uint16 count, uint16 numFrames, int smallIdx,
                                         int twoD, int negateFirst, XAllocFn alloc)
{
    xanim_track_s *keys;
    short q[4];
    int i;

    if (count == 0) {
        return 0;
    }
    if (count == 1) {
        short *inlineKey;
        if (!twoD) {
            XAnim_ReadQuat4(cursor, q);
            if (negateFirst) {
                q[0] = -q[0];
                q[1] = -q[1];
                q[2] = -q[2];
                q[3] = -q[3];
            }
            keys = (xanim_track_s *)alloc(0xc);
            inlineKey = (short *)((byte *)keys + 4);
            inlineKey[0] = q[0];
            inlineKey[1] = q[1];
            inlineKey[2] = q[2];
            inlineKey[3] = q[3];
        } else {
            XAnim_ReadQuat2(cursor, q);
            if (negateFirst) {
                q[0] = -q[0];
                q[1] = -q[1];
            }
            keys = (xanim_track_s *)alloc(8);
            inlineKey = (short *)((byte *)keys + 4);
            inlineKey[0] = q[0];
            inlineKey[1] = q[1];
        }
        keys->lastIndex = 0;
        return keys;
    }

    keys = XAnim_AllocKeyTable(cursor, count, numFrames, smallIdx, alloc);
    if (!twoD) {
        short *frames;
        keys->keys = alloc((uint32)count << 3);
        frames = (short *)keys->keys;
        XAnim_ReadQuat4(cursor, q);
        if (negateFirst) {
            q[0] = -q[0];
            q[1] = -q[1];
            q[2] = -q[2];
            q[3] = -q[3];
        }
        frames[0] = q[0];
        frames[1] = q[1];
        frames[2] = q[2];
        frames[3] = q[3];
        for (i = 1; i < (int)(uint32)count; i++) {
            XAnim_ReadQuat4(cursor, q);
            frames[i * 4 + 0] = q[0];
            frames[i * 4 + 1] = q[1];
            frames[i * 4 + 2] = q[2];
            frames[i * 4 + 3] = q[3];
        }
        for (i = 1; i < (int)(uint32)count; i++) {
            short *cur = frames + i * 4;
            short *prev = cur - 4;
            int dot = (int)((uint32)((int)prev[1] * (int)cur[1]) + (uint32)((int)cur[0] * (int)prev[0]) +
                            (uint32)((int)prev[3] * (int)cur[3]) + (uint32)((int)cur[2] * (int)prev[2]));
            if (dot < 0) {
                cur[0] = -cur[0];
                cur[1] = -cur[1];
                cur[2] = -cur[2];
                cur[3] = -cur[3];
            }
        }
    } else {
        short *frames;
        keys->keys = alloc((uint32)count << 2);
        frames = (short *)keys->keys;
        XAnim_ReadQuat2(cursor, q);
        if (negateFirst) {
            q[0] = -q[0];
            q[1] = -q[1];
        }
        frames[0] = q[0];
        frames[1] = q[1];
        for (i = 1; i < (int)(uint32)count; i++) {
            XAnim_ReadQuat2(cursor, q);
            frames[i * 2 + 0] = q[0];
            frames[i * 2 + 1] = q[1];
        }
        for (i = 1; i < (int)(uint32)count; i++) {
            short *cur = frames + i * 2;
            short *prev = cur - 2;
            int dot = (int)((uint32)((int)prev[1] * (int)cur[1]) + (uint32)((int)cur[0] * (int)prev[0]));
            if (dot < 0) {
                cur[0] = -cur[0];
                cur[1] = -cur[1];
            }
        }
    }
    keys->lastIndex = (uint16)(count - 1);
    return keys;
}

// 0x080c3f8a  size=4768  FUN_080c3f8a  status=translated  naming=inferred
// Evidence: reads "xanim/%s" and reports "xanim '%s' ... (version %d, expecting %d)"; the
// parsed object matches the IW-engine XAnimParts (frame rate, frequency, per-bone key lists, notifies).
// Format version 14. Returns 0 on any failure after printing the matching error.
XAnimParts *XAnim_LoadParts(const char *name, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    char path[0x44];
    byte *fileBuf;
    byte *cursor;
    byte *negateBits;
    byte *twoDBits;
    int fileLen;
    short version;
    uint16 numFramesFile;
    uint16 numBones;
    uint16 *boneNames;
    uint8 flagsByte;
    uint8 looping;
    uint8 hasDelta;
    short frameRateShort;
    XAnimParts *parts;
    uint16 numFrames;
    int smallIdx;
    int i;

    if (Com_sprintf(path, 0x40, "xanim/%s", name) < 0) {
        Com_Printf("^1ERROR: filename '%s' too long\n", path);
        return 0;
    }
    fileLen = FS_ReadFile(path, (void **)&fileBuf);
    if (fileLen < 0) {
        Com_Printf("^1ERROR: xanim '%s' not found\n", name);
        return 0;
    }
    if (fileLen == 0) {
        Com_Printf("^1ERROR: xanim '%s' has 0 length\n", name);
        FS_FreeFile(fileBuf);
        return 0;
    }

    cursor = fileBuf;
    version = ByteCursor_ReadInt16(&cursor);
    if (version != 0xe) {
        FS_FreeFile(fileBuf);
        Com_Printf("^1ERROR: xanim '%s' out of date (version %d, expecting %d)\n", name, (int)version, 0xe);
        return 0;
    }

    numFramesFile = (uint16)ByteCursor_ReadInt16(&cursor);
    numBones = (uint16)ByteCursor_ReadInt16(&cursor);
    if (numBones == 0) {
        boneNames = 0;
    } else {
        boneNames = (uint16 *)alloc((short)numBones * 2);
    }
    flagsByte = *cursor;
    cursor += 1;
    looping = flagsByte & 1;
    hasDelta = (flagsByte >> 1) & 1;
    frameRateShort = ByteCursor_ReadInt16(&cursor);

    parts = (XAnimParts *)alloc(0x2c);
    parts->numBones = numBones;
    parts->boneNames = boneNames;
    parts->frameRate = (float)frameRateShort;
    parts->looping = looping;
    parts->hasDelta = hasDelta;
    if (looping == 0) {
        numFrames = numFramesFile;
    } else {
        numFrames = (uint16)(numFramesFile + 1);
    }
    smallIdx = numFrames < 0x101;
    parts->numFrames = (uint16)(numFrames - 1);
    if (parts->numFrames == 0) {
        parts->frequency = 0.0f;
    } else {
        parts->frequency = parts->frameRate / (float)parts->numFrames;
    }

    if (hasDelta != 0) {
        XAnimBoneKeys *delta = (XAnimBoneKeys *)alloc(8);
        uint16 count;
        parts->delta = delta;

        count = ByteCursor_ReadUInt16(&cursor);
        delta->rot = XAnim_ReadRotKeys(&cursor, count, numFrames, smallIdx, 1, 0, alloc);
        count = ByteCursor_ReadUInt16(&cursor);
        delta->trans = XAnim_ReadTransKeys(&cursor, count, numFrames, smallIdx, alloc);
    }

    if (numBones == 0) {
        negateBits = 0;
        twoDBits = 0;
    } else {
        uint32 bitBytes = (uint32)((((short)numBones - 1) >> 3) + 1);
        negateBits = cursor;
        cursor += bitBytes;
        twoDBits = (byte *)alloc(bitBytes);
        memcpy(twoDBits, cursor, bitBytes);
        cursor += bitBytes;
        parts->twoDRotBits = twoDBits;
        parts->bones = (XAnimBoneKeys *)alloc((int)(short)numBones << 3);
    }

    for (i = 0; i < (short)numBones; i++) {
        uint32 len = strlen((const char *)cursor) + 1;
        boneNames[i] = ScrStr_Intern((const char *)cursor, 0, len, 9);
        cursor += len;
    }

    for (i = 0; i < (short)numBones; i++) {
        uint32 negate = (int)(uint32)negateBits[i >> 3] >> ((byte)i & 7);
        uint32 twoD = (int)*(char *)(twoDBits + (i >> 3)) >> ((byte)i & 7);
        XAnimBoneKeys *bone = &parts->bones[i];
        uint16 count;

        count = ByteCursor_ReadUInt16(&cursor);
        bone->rot = XAnim_ReadRotKeys(&cursor, count, numFrames, smallIdx, (twoD & 1) != 0, (negate & 1) != 0, alloc);
        count = ByteCursor_ReadUInt16(&cursor);
        bone->trans = XAnim_ReadTransKeys(&cursor, count, numFrames, smallIdx, alloc);
    }

    XAnim_LoadNotifies(name, &cursor, parts, (void *)alloc);
    FS_FreeFile(fileBuf);
    return parts;
}

/* ======================================================================== */
/* xmodel                                                                   */
/* ======================================================================== */

// 0x080c5320  size=15  FUN_080c5320  status=translated  naming=inferred
// Returns the byte at +0x8d, which the default-model initialiser sets to 1 and the
// replacement path (XModel_ReplaceWith) treats as "this is the default model".
uint8 XModel_IsDefault(const XModel *model)
{
    return model->isDefault;
}

// 0x080c5330  size=13  FUN_080c5330  status=translated  naming=unknown
// Behaviour only: stores its argument in global g_085e4ff8. Meaning of the global unknown.
void Global_Set_085e4ff8(uint32 value)
{
    g_085e4ff8 = value;
}

// 0x080c533e  size=79  FUN_080c533e  status=translated  naming=inferred
// Releases the script-string reference of every bone name of an xmodelparts.
void XModelParts_ReleaseBoneNames(XModelParts *parts)
{
    uint16 *names = parts->hierarchy->names;
    short numBones = (short)parts->numBones;
    int i;

    for (i = 0; i < numBones; i++) {
        ScrStr_Release(names[i]);
    }
}

// 0x080c538e  size=194  FUN_080c538e  status=translated  naming=inferred
// For a non-default model, releases the script-string references of each lod's surface
// name list and clears the list pointer.
void XModel_ReleaseSurfNames(XModel *model)
{
    int lod;
    int i;

    if (XModel_IsDefault(model) == 0) {
        for (lod = 0; lod < 4; lod++) {
            if (model->lod[lod].surfNames != 0) {
                for (i = 0; i < (short)model->lod[lod].numSurfNames; i++) {
                    ScrStr_Release(model->lod[lod].surfNames[i]);
                }
                model->lod[lod].surfNames = 0;
            }
        }
    }
}

// 0x080c5450  size=126  FUN_080c5450  status=translated  naming=inferred
// Fills the static default xmodelparts (one root bone, no animation data) and returns it.
XModelParts *XModelParts_GetDefault(void)
{
    uint16 *names = &g_085e4f40;
    XModelBoneHierarchy *hier = &g_085e4f44;
    XModelParts *parts;

    hier->names = names;
    parts = &g_xmodelDefaultParts;
    parts->hierarchy = hier;
    parts->quats = 0;
    parts->trans = 0;
    parts->numBones = 1;
    parts->numRootBones = 1;
    parts->partClassification = (uint8 *)&g_085e4fec;
    g_085e4fec = 0;
    *names = 0;
    return parts;
}

// 0x080c54ce  size=27  FUN_080c54ce  status=translated  naming=inferred
// Zeroes and returns the static default xmodelsurfs.
XModelSurfs *XModelSurfs_GetDefault(void)
{
    g_xmodelDefaultSurfs.header = 0;
    return &g_xmodelDefaultSurfs;
}

// 0x080c54ea  size=347  FUN_080c54ea  status=translated  naming=inferred
// Initialises a freshly allocated XModel as the "DEFAULT" model: default parts, four empty
// lods (one dummy surface name each), bounds -16..16 and isDefault = 1.
void XModel_InitDefault(XModel *model)
{
    int i;
    XBoneInfo *bone;

    model->isDefault = 1;
    model->parts = XModelParts_GetDefault();
    for (i = 0; i < 4; i++) {
        model->lod[i].surfs = 0;
        model->lod[i].name = "";
        model->lod[i].dist = 0.0f;
        model->lod[i].numSurfNames = 1;
        model->lod[i].surfNames = &g_085e4fee;
        g_085e4fee = 0;
    }
    model->lod[0].surfs = XModelSurfs_GetDefault();
    model->numLods = 1;
    model->collLod = 0;
    model->name = "DEFAULT";
    bone = &g_xmodelDefaultBoneInfo;
    bone->bounds[0][0] = -16.0f;
    bone->bounds[0][1] = -16.0f;
    bone->bounds[0][2] = -16.0f;
    bone->bounds[1][0] = 16.0f;
    bone->bounds[1][1] = 16.0f;
    bone->bounds[1][2] = 16.0f;
    model->boneInfo = bone;
}

// 0x080c5646  size=37  FUN_080c5646  status=translated  naming=inferred
// Allocates 0x90 bytes with the supplied allocator and initialises them as the default model.
XModel *XModel_AllocDefault(void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    XModel *model = (XModel *)alloc(0x90);
    XModel_InitDefault(model);
    return model;
}

// 0x080c566c  size=27  FUN_080c566c  status=translated  naming=inferred
// Asset-table lookup, asset type 3 (xmodelparts; the loader error strings name the type).
XModelParts *DB_FindXModelParts(const char *name)
{
    return (XModelParts *)Hunk_FindUserEntryByName(3, name);
}

// 0x080c5688  size=41  FUN_080c5688  status=translated  naming=inferred
// Asset-table registration, asset type 3 (xmodelparts).
void DB_RegisterXModelParts(const char *name, XModelParts *parts, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    Hunk_AddUserEntry(3, name, (int)parts, (void *)alloc);
}

// 0x080c56b2  size=27  FUN_080c56b2  status=translated  naming=inferred
// Asset-table lookup, asset type 2 (xmodelsurfs).
XModelSurfs *DB_FindXModelSurfs(const char *name)
{
    return (XModelSurfs *)Hunk_FindUserEntryByName(2, name);
}

// 0x080c56ce  size=41  FUN_080c56ce  status=translated  naming=inferred
// Asset-table registration, asset type 2 (xmodelsurfs).
void DB_RegisterXModelSurfs(const char *name, XModelSurfs *surfs, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    Hunk_AddUserEntry(2, name, (int)surfs, (void *)alloc);
}

// 0x080c5714  size=58  FUN_080c5714  status=translated  naming=inferred
// Creates the default model and registers it as xmodel (asset type 4) under the requested name.
XModel *XModel_RegisterDefault(const char *name, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    XModel *model = XModel_AllocDefault((void *)(void *)alloc);
    Hunk_AddUserEntry(4, name, (int)model, (void *)alloc);
    return model;
}

// 0x080c574e  size=241  FUN_080c574e  status=translated  naming=inferred
// Evidence: error string "Could not load replacement model.". Overwrites dst with the 0x90
// bytes of src keeping dst's name, then takes a script-string reference for every lod surface
// name that is now shared.
void XModel_ReplaceWith(XModel *dst, const XModel *src)
{
    const char *savedName;
    int lod;
    int i;

    if (XModel_IsDefault(src) != 0) {
        Com_Error(1, "Could not load replacement model.");
    }
    savedName = dst->name;
    XModel_ReleaseSurfNames(dst);
    memcpy(dst, src, sizeof(XModel));
    dst->name = savedName;
    for (lod = 0; lod < 4; lod++) {
        if (dst->lod[lod].surfNames != 0) {
            for (i = 0; i < (short)dst->lod[lod].numSurfNames; i++) {
                ScrStr_AddRef(dst->lod[lod].surfNames[i]);
            }
        }
    }
}

// 0x080c5840  size=176  FUN_080c5840  status=translated  naming=inferred
// Evidence: error string "Cannot find xmodel '%s'.". Lookup in the asset table (type 4); on a
// miss loads the model, registers it and stores the registered name pointer at +0x88; when
// loading fails prints the error and falls back to a registered default model.
XModel *XModel_FindOrLoad(const char *name, void *alloc_p, void *allocColl_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    XAllocFn allocColl = (XAllocFn)allocColl_p;
    XModel *model = (XModel *)Hunk_FindUserEntryByName(4, name);

    if (model == 0) {
        model = XModel_LoadWithSurfs(name, (void *)(void *)alloc, (void *)(void *)allocColl);
        if (model == 0) {
            Com_Printf("^1ERROR: Cannot find xmodel '%s'.\n", name);
            model = XModel_RegisterDefault(name, (void *)(void *)alloc);
        } else {
            model->name = Hunk_AddUserEntry(4, name, (int)model, (void *)alloc);
        }
    }
    return model;
}

// 0x080c58f0  size=15  FUN_080c58f0  status=translated  naming=inferred
// Returns the bone-name script string array of the model's parts.
uint16 *XModel_GetBoneNames(const XModel *model)
{
    return model->parts->hierarchy->names;
}

// 0x080c5900  size=87  FUN_080c5900  status=translated  naming=inferred
// Searches the bone names from the last bone down for boneName; returns the index or -1.
int XModel_GetBoneIndex(const XModel *model, uint32 boneName)
{
    int index = (int)(short)model->parts->numBones;

    do {
        index = index - 1;
        if (index < 0) {
            return index;
        }
    } while (boneName != (uint32)model->parts->hierarchy->names[index]);
    return index;
}

// 0x080c5958  size=50  FUN_080c5958  status=translated  naming=inferred
// Copies the model bounds (mins at +0x64, maxs at +0x70) to the caller's vectors.
void XModel_GetBounds(const XModel *model, float *mins, float *maxs)
{
    Vec3_CopyB(model->mins, mins);
    Vec3_CopyB(model->maxs, maxs);
}

// 0x080c598a  size=28  FUN_080c598a  status=translated  naming=inferred
// Returns the surface-asset name of lod index.
const char *XModel_GetLodName(const XModel *model, int lod)
{
    return model->lod[lod].name;
}

// 0x080c59a6  size=11  FUN_080c59a6  status=translated  naming=inferred
// Returns the OR of all collision surface contents (+0x5c).
int XModel_GetContents(const XModel *model)
{
    return model->contents;
}

// 0x080c59b2  size=12  FUN_080c59b2  status=translated  naming=unknown
// Returns the int16 at +0x7e sign-extended (low half of a header int).
int XModel_GetCollLod(const XModel *model)
{
    return (int)model->collLod;
}

// 0x080c59be  size=14  FUN_080c59be  status=translated  naming=unknown
// Returns the 32-bit field at +0x80 (never written by the loaders).
int XModel_GetUnknown80(const XModel *model)
{
    return model->unknown80;
}

// 0x080c59cc  size=14  FUN_080c59cc  status=translated  naming=inferred
// Returns the allocation byte count accumulated by the loaders at +0x84.
int XModel_GetMemUsage(const XModel *model)
{
    return model->memUsage;
}

// 0x080c79f0  size=14  FUN_080c79f0  status=translated  naming=inferred
// Returns the model name pointer at +0x88.
const char *XModel_GetName(const XModel *model)
{
    return model->name;
}

// 0x080c59da  size=1057  FUN_080c59da  status=translated  naming=inferred
// Traces the segment start..end against the collision surfaces whose contents intersect
// contentMask, in each surface's bone space. On a closer hit fills results (fraction, normal,
// surfaceFlags, contents, allSolid = startSolid = 0) and remembers the bone; returns that bone
// index with the normal rotated back by the bone matrix, or -1 when nothing was hit.
int XModel_TraceLine(XModel *model, XModelTrace *results, DObjBone *boneMats, const float *start,
                     const float *end, uint32 contentMask)
{
    int hitBone = -1;
    int unusedBoneCount;
    int surfIndex;

    unusedBoneCount = XModelRef_GetHeaderShort((const struct XModelRef *)model);
    (void)unusedBoneCount;
    for (surfIndex = 0; surfIndex < model->numCollSurfs; surfIndex++) {
        XModelCollSurf *surf = &model->collSurfs[surfIndex];

        if ((contentMask & (uint32)surf->contents) != 0) {
            int bone = surf->boneIdx;
            DObjBone *mat = &boneMats[bone];
            float localStart[3];
            float localEnd[3];
            float axis[9];
            XModelTraceLine line;
            float dir[3];

            Vec3_Sub_080c5eb8(start, mat->trans, localStart);
            Vec3_Sub_080c5eb8(end, mat->trans, localEnd);
            QuatTransform_ToMat3((const struct QuatTransform_s *)mat, axis);
            Vec3_Rotate(localStart, axis, line.start);
            Vec3_Rotate(localEnd, axis, line.end);
            Box_ComputeInverseExtents(line.start);
            if (Box_SegmentSlabTest((const struct cod2_box_s *)line.start, surf->mins, surf->maxs, results->fraction) == 0) {
                int triIndex;

                Vec3_Sub_080c5eb8(line.end, line.start, dir);
                for (triIndex = 0; triIndex < surf->numCollTris; triIndex++) {
                    XModelCollTri *tri = &surf->collTris[triIndex];
                    float distEnd = Vec3_Dot_080c5f50(line.end, tri->plane) - tri->plane[3];

                    if (distEnd < 0.0f) {
                        float distStart = Vec3_Dot_080c5f50(line.start, tri->plane) - tri->plane[3];

                        if (0.0f < distStart) {
                            float fraction = (distStart - 0.125f) / (distStart - distEnd);

                            fraction = Float_Max_080c5e64(fraction, 0.0f);
                            if (fraction < results->fraction) {
                                float t = distStart / (distStart - distEnd);
                                float hit[3];
                                float u;

                                Vec3_MA_080c5efc(line.start, t, dir, hit);
                                u = Vec3_Dot_080c5f50(hit, tri->svec) - tri->svec[3];
                                if (-0.001f <= u && u <= 1.001f) {
                                    float v = Vec3_Dot_080c5f50(hit, tri->tvec) - tri->tvec[3];

                                    if (-0.001f <= v && u + v <= 1.001f) {
                                        hitBone = bone;
                                        results->startSolid = 0;
                                        results->allSolid = 0;
                                        results->fraction = fraction;
                                        results->surfaceFlags = surf->surfFlags;
                                        results->contents = surf->contents;
                                        Vec3_CopyB(tri->plane, results->normal);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (hitBone < 0) {
        return -1;
    } else {
        DObjBone *mat = &boneMats[hitBone];
        float axis[9];
        float rotated[3];

        QuatTransform_ToMat3((const struct QuatTransform_s *)mat, axis);
        Vec3_RotateTransposed(results->normal, axis, rotated);
        Vec3_CopyB(rotated, results->normal);
        return hitBone;
    }
}

// 0x080c5dfc  size=80  FUN_080c5dfc  status=translated  naming=inferred
// Evidence: strncasecmp(name, "xmodel", 6) followed by a '/' check (the binary tests '/' twice).
int String_IsXModelPath(const char *name)
{
    int result = 0;

    if (strncasecmp(name, "xmodel", 6) == 0 && (name[6] == '/' || name[6] == '/')) {
        result = 1;
    }
    return result;
}

// 0x080c613c  size=225  FUN_080c613c  status=translated  naming=unknown
// Duplicate of XAnim_ReadQuat4 (own copy of the sqrt wrapper, Float_Sqrt_080c77cc).
void XModelParts_ReadQuat4(byte **cursor, short *out)
{
    short w;
    int remaining;

    out[0] = ByteCursor_ReadInt16(cursor);
    out[1] = ByteCursor_ReadInt16(cursor);
    out[2] = ByteCursor_ReadInt16(cursor);
    remaining = 0x3fff0001 - ((int)out[0] * (int)out[0] + (int)out[1] * (int)out[1] + (int)out[2] * (int)out[2]);
    if (remaining < 1) {
        w = 0;
    } else {
        float root = Float_Sqrt_080c77cc((float)remaining);
        double rounded = floor((double)(0.5L + (long double)root));
        w = (short)(int)rounded;
    }
    out[3] = w;
}

// 0x080c621e  size=422  FUN_080c621e  status=translated  naming=inferred
// Builds the base-pose matrices of an xmodelparts: identity-like (quat 0,0,0,1; trans 0;
// weight 2.0) for the root bones, then for each other bone quat = short/32767 combined with
// the parent matrix, weight recomputed and translation transformed by the parent. Finally
// sets both 16-byte bone masks to 0xff.
void XModelParts_BuildBaseMats(XModelParts *parts)
{
    XModelBoneHierarchy *hier = parts->hierarchy;
    uint8 *parentDelta = (uint8 *)hier->parentDelta;
    int numBones = (int)(short)parts->numBones;
    short *quatShorts = parts->quats;
    float *trans = parts->trans;
    DObjBone *mat = parts->baseMat;
    int n;

    for (n = (int)(short)parts->numRootBones; n != 0; n--) {
        Vec3_Clear_080c77ec(mat->quat);
        mat->quat[3] = 1.0f;
        Vec3_Clear_080c77ec(mat->trans);
        mat->transWeight = 2.0f;
        mat++;
    }
    for (n = numBones - (int)(short)parts->numRootBones; n != 0; n--) {
        float q[4];
        DObjBone *parent = mat - (uint32)*parentDelta;

        q[0] = (float)quatShorts[0] * 3.0518509e-05f;
        q[1] = (float)quatShorts[1] * 3.0518509e-05f;
        q[2] = (float)quatShorts[2] * 3.0518509e-05f;
        q[3] = (float)quatShorts[3] * 3.0518509e-05f;
        Quat_Multiply(q, parent->quat, mat->quat);
        Quat_UpdateScale(mat);
        Vec3_TransformByQuatTransform(trans, (const struct QuatTransform_s *)parent, mat->trans);
        quatShorts += 4;
        trans += 3;
        mat++;
        parentDelta++;
    }
    memset(parts->boneMaskA, 0xff, 0x10);
    memset(parts->boneMaskB, 0xff, 0x10);
}

// 0x080c63c4  size=1175  FUN_080c63c4  status=translated  naming=inferred
// Evidence: reads "xmodelparts/%s"; errors "xmodelparts '%s' not found / has 0 length / out of
// date (version %d, expecting %d)" and "xmodel '%s' has more than %d bones" (limit 0x7f).
// Format version 20. Updates model->memUsage with every allocation (also before the bone
// limit check, as the original does).
XModelParts *XModelParts_Load(XModel *model, const char *name, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    char path[0x44];
    byte *fileBuf;
    byte *cursor;
    int fileLen;
    short version;
    short numChildBones;
    short numRootBones;
    short numBones;
    uint16 *boneNames;
    XModelBoneHierarchy *hier;
    XModelParts *parts;
    int8 *parentOut;
    short *quatOut;
    float *transOut;
    int size;
    int i;

    if (Com_sprintf(path, 0x40, "xmodelparts/%s", name) < 0) {
        Com_Printf("^1ERROR: filename '%s' too long\n", path);
        return 0;
    }
    fileLen = FS_ReadFile(path, (void **)&fileBuf);
    if (fileLen < 0) {
        Com_Printf("^1ERROR: xmodelparts '%s' not found\n", name);
        return 0;
    }
    if (fileLen == 0) {
        Com_Printf("^1ERROR: xmodelparts '%s' has 0 length\n", name);
        FS_FreeFile(fileBuf);
        return 0;
    }

    cursor = fileBuf;
    version = ByteCursor_ReadInt16(&cursor);
    if (version != 0x14) {
        FS_FreeFile(fileBuf);
        Com_Printf("^1ERROR: xmodelparts '%s' out of date (version %d, expecting %d).\n", name, (int)version, 0x14);
        return 0;
    }

    numChildBones = ByteCursor_ReadInt16(&cursor);
    numRootBones = ByteCursor_ReadInt16(&cursor);
    numBones = (short)(numRootBones + numChildBones);
    boneNames = (uint16 *)alloc(numBones * 2);
    model->memUsage = numBones * 2 + model->memUsage;
    if (numBones >= 0x80) {
        FS_FreeFile(fileBuf);
        Com_Printf("^1ERROR: xmodel '%s' has more than %d bones\n", name, 0x7f);
        return 0;
    }

    hier = (XModelBoneHierarchy *)alloc(numChildBones + 7);
    model->memUsage = numChildBones + 7 + model->memUsage;
    hier->names = boneNames;
    parentOut = hier->parentDelta;
    size = numBones * 0x20 + 0x44;
    parts = (XModelParts *)alloc(size);
    model->memUsage = size + model->memUsage;
    parts->hierarchy = hier;
    if (numChildBones == 0) {
        parts->quats = 0;
        parts->trans = 0;
    } else {
        parts->quats = (short *)alloc(numChildBones * 8);
        model->memUsage = numChildBones * 8 + model->memUsage;
        parts->trans = (float *)alloc(numChildBones * 0x10);
        model->memUsage = numChildBones * 0x10 + model->memUsage;
    }
    parts->partClassification = (uint8 *)alloc((int)numBones);
    model->memUsage = (int)numBones + model->memUsage;
    parts->numBones = (uint16)numBones;
    parts->numRootBones = (uint16)numRootBones;
    quatOut = parts->quats;
    transOut = parts->trans;

    for (i = numRootBones; i < numBones; i++) {
        char parentRel = (char)*cursor;
        cursor += 1;
        *parentOut = (int8)((char)i - parentRel);
        transOut[0] = ByteCursor_ReadFloat32(&cursor);
        transOut[1] = ByteCursor_ReadFloat32(&cursor);
        transOut[2] = ByteCursor_ReadFloat32(&cursor);
        XModelParts_ReadQuat4(&cursor, quatOut);
        quatOut += 4;
        transOut += 3;
        parentOut++;
    }
    for (i = 0; i < numBones; i++) {
        int len = ScrStr_Strlen((const char *)cursor);
        boneNames[i] = ScrStr_Intern((const char *)cursor, 0, len + 1, 10);
        cursor += len + 1;
    }
    memcpy(parts->partClassification, cursor, (int)numBones);
    cursor += numBones;
    FS_FreeFile(fileBuf);
    XModelParts_BuildBaseMats(parts);
    return parts;
}

// 0x080c685c  size=372  FUN_080c685c  status=translated  naming=inferred
// Evidence: error "xmodel '%s' out of date (version %d, expecting %d)." Parses the xmodel
// header (version 20): flag byte, mins, maxs, then four (float dist, string name) lods and a
// 32-bit word. Returns 1 on success, 0 on a version mismatch.
int XModel_ReadHeader(const char *name, byte **cursor, XModelFileHeader *hdr)
{
    short version = ByteCursor_ReadInt16(cursor);
    int i;

    if (version != 0x14) {
        Com_Printf("^1ERROR: xmodel '%s' out of date (version %d, expecting %d).\n", name, (int)version, 0x14);
        return 0;
    }
    hdr->flags = **cursor;
    *cursor += 1;
    hdr->mins[0] = ByteCursor_ReadFloat32(cursor);
    hdr->mins[1] = ByteCursor_ReadFloat32(cursor);
    hdr->mins[2] = ByteCursor_ReadFloat32(cursor);
    hdr->maxs[0] = ByteCursor_ReadFloat32(cursor);
    hdr->maxs[1] = ByteCursor_ReadFloat32(cursor);
    hdr->maxs[2] = ByteCursor_ReadFloat32(cursor);
    for (i = 0; i < 4; i++) {
        hdr->lod[i].dist = ByteCursor_ReadFloat32(cursor);
        strcpy(hdr->lod[i].name, (const char *)*cursor);
        *cursor += strlen((const char *)*cursor) + 1;
    }
    hdr->collLod = ByteCursor_ReadUInt32(cursor);
    return 1;
}

// 0x080c69d0  size=662  FUN_080c69d0  status=translated  naming=inferred
// Reads the collision section: surface count, then per surface the triangle list (12 floats
// each), bounds (mins widened by -0.001, maxs by +0.001), bone index, contents (masked with
// 0xdffffffb), surface flags. Accumulates model->contents.
void XModel_ReadCollision(byte **cursor, XModel *model, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    int surfIndex;
    int triIndex;

    model->numCollSurfs = (int)ByteCursor_ReadUInt32(cursor);
    if (model->numCollSurfs != 0) {
        model->collSurfs = (XModelCollSurf *)alloc(model->numCollSurfs * 0x2c);
        for (surfIndex = 0; surfIndex < model->numCollSurfs; surfIndex++) {
            XModelCollSurf *surf = &model->collSurfs[surfIndex];
            uint32 contents;

            surf->numCollTris = (int)ByteCursor_ReadUInt32(cursor);
            surf->collTris = (XModelCollTri *)alloc(surf->numCollTris * 0x30);
            for (triIndex = 0; triIndex < surf->numCollTris; triIndex++) {
                float *f = (float *)&surf->collTris[triIndex];
                int k;
                for (k = 0; k < 12; k++) {
                    f[k] = ByteCursor_ReadFloat32(cursor);
                }
            }
            surf->mins[0] = ByteCursor_ReadFloat32(cursor) - 0.001f;
            surf->mins[1] = ByteCursor_ReadFloat32(cursor) - 0.001f;
            surf->mins[2] = ByteCursor_ReadFloat32(cursor) - 0.001f;
            surf->maxs[0] = 0.001f + ByteCursor_ReadFloat32(cursor);
            surf->maxs[1] = 0.001f + ByteCursor_ReadFloat32(cursor);
            surf->maxs[2] = 0.001f + ByteCursor_ReadFloat32(cursor);
            surf->boneIdx = (int)ByteCursor_ReadUInt32(cursor);
            contents = ByteCursor_ReadUInt32(cursor);
            surf->contents = (int)(contents & 0xdffffffb);
            surf->surfFlags = (int)ByteCursor_ReadUInt32(cursor);
            model->contents = surf->contents | model->contents;
        }
    }
}

// 0x080c6c66  size=132  FUN_080c6c66  status=translated  naming=inferred
// Evidence: error "Cannot find xmodelparts '%s'.". Asset-table lookup (type 3); on a miss loads
// the parts and registers them.
XModelParts *XModel_FindOrLoadParts(XModel *model, const char *name, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    XModelParts *parts = DB_FindXModelParts(name);

    if (parts == 0) {
        parts = XModelParts_Load(model, name, (void *)alloc);
        if (parts == 0) {
            Com_Printf("^1ERROR: Cannot find xmodelparts '%s'.\n", name);
            parts = 0;
        } else {
            DB_RegisterXModelParts(name, parts, (void *)alloc);
        }
    }
    return parts;
}

// 0x080c6cea  size=1838  FUN_080c6cea  status=translated  naming=inferred
// Evidence: reads "xmodel/%s"; errors "xmodel '%s' not found / has 0 length", "filename '%s'
// too long". Loads one xmodel file: header, one block holding the model and the lod name
// strings, collision surfaces (allocColl), per-lod surface-name script strings, the parts
// (via the asset table) and per-bone bounds. Returns 0 on failure.
XModel *XModel_LoadFile(const char *name, void *allocModel_p, void *allocColl_p)
{
    XAllocFn allocModel = (XAllocFn)allocModel_p;
    XAllocFn allocColl = (XAllocFn)allocColl_p;
    char path[0x44];
    byte *fileBuf;
    byte *cursor;
    XModelFileHeader hdr;
    int lodNameLen[4];
    int totalNameBytes;
    int size;
    int fileLen;
    XModel *model;
    char *nameStore;
    int lod;
    int numBones;
    int boneBytes;
    XBoneInfo *boneInfo;
    int i;

    if (Com_sprintf(path, 0x40, "xmodel/%s", name) < 0) {
        Com_Printf("^1ERROR: filename '%s' too long\n", path);
        return 0;
    }
    fileLen = FS_ReadFile(path, (void **)&fileBuf);
    if (fileLen < 0) {
        Com_Printf("^1ERROR: xmodel '%s' not found\n", name);
        return 0;
    }
    if (fileLen == 0) {
        Com_Printf("^1ERROR: xmodel '%s' has 0 length\n", name);
        FS_FreeFile(fileBuf);
        return 0;
    }

    cursor = fileBuf;
    if ((char)XModel_ReadHeader(name, &cursor, &hdr) == 0) {
        FS_FreeFile(fileBuf);
        return 0;
    }

    totalNameBytes = 0;
    for (lod = 0; lod < 4; lod++) {
        lodNameLen[lod] = ScrStr_Strlen(hdr.lod[lod].name) + 1;
        totalNameBytes = totalNameBytes + lodNameLen[lod];
    }
    size = totalNameBytes + 0x90;
    model = (XModel *)allocModel(size);
    model->memUsage = size;
    XModel_ReadCollision(&cursor, model, (void *)allocColl);
    nameStore = (char *)model + 0x90;
    model->numLods = 0;
    for (lod = 0; lod < 4; lod++) {
        strcpy(nameStore, hdr.lod[lod].name);
        model->lod[lod].name = nameStore;
        if (nameStore[0] == '\0') {
            model->lod[lod].surfNames = 0;
        } else {
            int bytes;

            model->numLods = (uint16)(model->numLods + 1);
            model->lod[lod].numSurfNames = (uint16)ByteCursor_ReadInt16(&cursor);
            bytes = (short)model->lod[lod].numSurfNames * 2;
            model->lod[lod].surfNames = (uint16 *)allocModel(bytes);
            model->memUsage = bytes + model->memUsage;
            for (i = 0; i < (short)model->lod[lod].numSurfNames; i++) {
                const char *surfName = (const char *)cursor;
                cursor += strlen((const char *)cursor) + 1;
                model->lod[lod].surfNames[i] = ScrStr_InternCStringEx(surfName, 0, 8);
            }
        }
        model->lod[lod].dist = hdr.lod[lod].dist;
        nameStore += lodNameLen[lod];
    }

    model->parts = XModel_FindOrLoadParts(model, model->lod[0].name, (void *)allocModel);
    if (model->parts == 0) {
        FS_FreeFile(fileBuf);
        XModel_ReleaseSurfNames(model);
        return 0;
    }

    numBones = (int)(short)model->parts->numBones;
    boneBytes = numBones * 0x28;
    boneInfo = (XBoneInfo *)allocModel(boneBytes);
    model->memUsage = boneBytes + model->memUsage;
    for (i = 0; i < numBones; i++) {
        XBoneInfo *bone = &boneInfo[i];
        float radiusVec[3];

        bone->bounds[0][0] = ByteCursor_ReadFloat32(&cursor);
        bone->bounds[0][1] = ByteCursor_ReadFloat32(&cursor);
        bone->bounds[0][2] = ByteCursor_ReadFloat32(&cursor);
        bone->bounds[1][0] = ByteCursor_ReadFloat32(&cursor);
        bone->bounds[1][1] = ByteCursor_ReadFloat32(&cursor);
        bone->bounds[1][2] = ByteCursor_ReadFloat32(&cursor);
        Vec3_Midpoint(bone->bounds[0], bone->bounds[1], bone->offset);
        Vec3_Sub_080c786a(bone->bounds[1], bone->offset, radiusVec);
        bone->radiusSquared = Vec3_LengthSq(radiusVec);
    }
    model->boneInfo = boneInfo;
    FS_FreeFile(fileBuf);
    Vec3_CopyBits_080c783a(hdr.mins, model->mins);
    Vec3_CopyBits_080c783a(hdr.maxs, model->maxs);
    model->collLod = (int16)hdr.collLod;
    model->flags = hdr.flags;
    return model;
}

// 0x080c7418  size=10  FUN_080c7418  status=translated  naming=unknown
// Behaviour only: returns 0, no arguments (unreferenced in this chunk).
int Stub_ReturnZero_080c7418(void)
{
    return 0;
}

// 0x080c7422  size=10  FUN_080c7422  status=translated  naming=unknown
// Behaviour only: returns 0, no arguments (unreferenced in this chunk).
int Stub_ReturnZero_080c7422(void)
{
    return 0;
}

// 0x080c742c  size=10  FUN_080c742c  status=translated  naming=inferred
// Always 0 in this (dedicated server) build; gates the xmodelsurfs loading in XModel_LoadWithSurfs.
int XModel_SurfsLoadingEnabled(void)
{
    return 0;
}

// 0x080c7436  size=25  FUN_080c7436  status=translated  naming=inferred
// Dedicated-server stub of the xmodelsurfs loader: ignores its five arguments (only the int16
// is spilled) and returns 0.
int XModelSurfs_Load(XModel *model, const char *name, void *alloc_p, int numSurfNames, const char *modelName)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    short spilledSurfCount = (short)numSurfNames;
    int result = 0;

    (void)spilledSurfCount;
    return result;
}

// 0x080c7450  size=154  FUN_080c7450  status=translated  naming=inferred
// Evidence: error "Cannot find 'xmodelsurfs '%s'.". Asset-table lookup (type 2); on a miss
// calls the (stubbed) loader, prints the error on failure, registers on success.
XModelSurfs *XModelSurfs_FindOrLoad(XModel *model, const char *name, void *alloc_p, short numSurfNames,
                                    const char *modelName)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    XModelSurfs *surfs = DB_FindXModelSurfs(name);

    if (surfs == 0) {
        surfs = (XModelSurfs *)(long)XModelSurfs_Load(model, name, (void *)alloc, (int)numSurfNames, modelName);
        if (surfs == 0) {
            Com_Printf("^1ERROR: Cannot find 'xmodelsurfs '%s'.\n", name);
            surfs = 0;
        } else {
            DB_RegisterXModelSurfs(name, surfs, (void *)alloc);
        }
    }
    return surfs;
}

// 0x080c74ea  size=156  FUN_080c74ea  status=translated  naming=inferred
// Loads the surfaces of every lod with a non-empty name; returns 0 as soon as one fails.
int XModel_LoadAllSurfs(XModel *model, void *alloc_p)
{
    XAllocFn alloc = (XAllocFn)alloc_p;
    int lod = 0;

    while (lod < 4 && model->lod[lod].name[0] != '\0') {
        XModelLod *l = &model->lod[lod];

        l->surfs = XModelSurfs_FindOrLoad(model, l->name, (void *)alloc, (short)l->numSurfNames, model->name);
        if (l->surfs == 0) {
            return 0;
        }
        lod = lod + 1;
    }
    return 1;
}

// 0x080c7586  size=111  FUN_080c7586  status=translated  naming=inferred
// Loads the model file, then (when surface loading is enabled) the lod surfaces; on a surface
// failure releases the surface names and returns 0.
XModel *XModel_LoadWithSurfs(const char *name, void *allocModel_p, void *allocColl_p)
{
    XAllocFn allocModel = (XAllocFn)allocModel_p;
    XAllocFn allocColl = (XAllocFn)allocColl_p;
    XModel *model = XModel_LoadFile(name, (void *)allocModel, (void *)allocColl);

    if (model == 0) {
        return 0;
    }
    if ((char)XModel_SurfsLoadingEnabled() != 0 && (char)XModel_LoadAllSurfs(model, (void *)allocModel) == 0) {
        XModel_ReleaseSurfNames(model);
        return 0;
    }
    return model;
}

// 0x080c75f6  size=469  FUN_080c75f6  status=translated  naming=inferred
// Transforms the 8 corners of every collision-surface box by the 3x3 axis and accumulates their
// bounds into mins/maxs (initialised to +FLT_MAX / -FLT_MAX). Returns 0 (outputs untouched) when
// the model has no collision surfaces.
int XModel_ComputeBounds(const XModel *model, const float *axis9, float *mins, float *maxs)
{
    int surfIndex;
    int result;

    if (model->numCollSurfs == 0) {
        result = 0;
    } else {
        Vec3_Set_080c7816(mins, FLT_MAX, FLT_MAX, FLT_MAX);
        Vec3_Set_080c7816(maxs, -FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (surfIndex = 0; surfIndex < model->numCollSurfs; surfIndex++) {
            const XModelCollSurf *surf = &model->collSurfs[surfIndex];
            uint32 corner;

            for (corner = 0; (int)corner < 8; corner++) {
                float pt[3];
                float out[4];
                int axis;

                pt[0] = ((corner & 1) == 0) ? surf->maxs[0] : surf->mins[0];
                pt[1] = ((corner & 2) == 0) ? surf->maxs[1] : surf->mins[1];
                pt[2] = ((corner & 4) == 0) ? surf->maxs[2] : surf->mins[2];
                Vec3_RotateByTranspose(pt, axis9, out);
                for (axis = 0; axis < 3; axis++) {
                    if (out[axis] < mins[axis]) {
                        mins[axis] = out[axis];
                    }
                    if (maxs[axis] < out[axis]) {
                        maxs[axis] = out[axis];
                    }
                }
            }
        }
        result = 1;
    }
    return result;
}
