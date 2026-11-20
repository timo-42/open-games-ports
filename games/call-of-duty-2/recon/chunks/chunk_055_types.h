/*
 * Chunk 055 (0x08128a16..0x0812b52a) local types and out-of-chunk declarations.
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), effects (Fx) subsystem:
 * math helpers, keyframe curves, fixed-size block pools, ref handles, FxScheduler::PlayEffect.
 * Layouts are recovered from consistent offsets in this chunk only; unknown gaps are padding.
 */
#ifndef CHUNK_055_TYPES_H
#define CHUNK_055_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ---- keyframe curves ------------------------------------------------------ */

/* Curve cursor: keys -> { int comps; int ?; then keys of (comps+1) floats: time, v[comps] } at +8. */
struct FxCurve {
    void *keys;      /* 0x00 key table (header 8 bytes, keys start at +8) */
    int cursor;      /* 0x04 cached key index, advanced by FxCurve_Seek */
    float scale;     /* 0x08 output scale */
};
COD2_ASSERT_SIZE(FxCurve, 0xc);
COD2_ASSERT_OFFSET(FxCurve, cursor, 4);
COD2_ASSERT_OFFSET(FxCurve, scale, 8);

/* Stack temporary built by FxCurve_InitFromTemplate (real size unknown; frame gap is 16 bytes). */
struct FxCurveTemp {
    struct FxCurve curve;
    int pad;
};
COD2_ASSERT_SIZE(FxCurveTemp, 0x10);

/* ---- fixed-size block pools ---------------------------------------------- */

/* One 0x8000-byte block: element storage followed by a trailer. */
struct FxPoolBlock {
    unsigned char data[0x7ff0];
    int freeCount;                 /* 0x7ff0 */
    void **freeList;               /* 0x7ff4 first free element; its first dword links the next */
    struct FxPoolBlock *next;      /* 0x7ff8 */
    struct FxPoolBlock *prev;      /* 0x7ffc */
};
COD2_ASSERT_SIZE(FxPoolBlock, 0x8000);
COD2_ASSERT_OFFSET(FxPoolBlock, freeCount, 0x7ff0);
COD2_ASSERT_OFFSET(FxPoolBlock, freeList, 0x7ff4);
COD2_ASSERT_OFFSET(FxPoolBlock, next, 0x7ff8);
COD2_ASSERT_OFFSET(FxPoolBlock, prev, 0x7ffc);

struct FxPool {
    int perBlock;                  /* 0x00 elements per block (0x7ff0 / elemSize) */
    struct FxPoolBlock *head;      /* 0x04 front block, preferred for allocation */
};
COD2_ASSERT_SIZE(FxPool, 8);

/* ---- reference handle ------------------------------------------------------ */

/* Intrusive ref handle: obj points at an object whose first dword is its refcount. */
struct FxBinding {
    int refCount;                  /* 0x00 (object continues; see chunk 054) */
};
struct FxRef {
    struct FxBinding *obj;
};

/* Stack holder for a handle built by FxBinding_FindOrCreate (20 bytes in the frame). */
struct FxBindingKey;
struct FxRefHolder {
    struct FxRef ref;
    int pad[4];
};
COD2_ASSERT_SIZE(FxRefHolder, 0x14);

/* ---- global effects helper (pointer stored at 0x08188ac0) ---------------- */

struct FxSystem {
    int field0;                    /* 0x00 */
    int time;                      /* 0x04 current time, ms */
    int field8;                    /* 0x08 */
    int frameTime;                 /* 0x0c frame delta, ms */
    int field10;                   /* 0x10 */
    vec3_t viewOrigin;             /* 0x14 */
    vec3_t viewForward;            /* 0x20 */
    unsigned char pad2c[0xf8 - 0x2c];
    float distanceScale;           /* 0xf8 */
};
COD2_ASSERT_OFFSET(FxSystem, time, 4);
COD2_ASSERT_OFFSET(FxSystem, frameTime, 0xc);
COD2_ASSERT_OFFSET(FxSystem, viewOrigin, 0x14);
COD2_ASSERT_OFFSET(FxSystem, viewForward, 0x20);
COD2_ASSERT_OFFSET(FxSystem, distanceScale, 0xf8);

/* Boolean variable object: byte value at +8 (dvar-like). */
struct FxFlagVar {
    int word0;
    int word4;
    unsigned char value;           /* 0x08 */
};
COD2_ASSERT_OFFSET(FxFlagVar, value, 8);

/* ---- objects stored in the pools ------------------------------------------ */

/* Object with curve-driven size and colour; pool element size 0xfc. Vtable at +0. */
struct Light {
    void *vtbl;                    /* 0x00 */
    vec3_t origin;                 /* 0x04 */
    unsigned char pad10[0x3c - 0x10];
    float age;                     /* 0x3c curve sample time */
    unsigned char pad40[0x6c - 0x40];
    vec3_t color;                  /* 0x6c */
    unsigned char pad78[0x7c - 0x78];
    vec3_t screenPos;              /* 0x7c */
    float size;                    /* 0x88 */
    float size2;                   /* 0x8c */
    unsigned char colorBytes[4];   /* 0x90 */
    unsigned char pad94[0xa8 - 0x94];
    unsigned int flags;            /* 0xa8 0x2000 = blend colour curves, 0x8000 = blend size curves */
    unsigned char padac[0xc4 - 0xac];
    float colorBlend;              /* 0xc4 */
    float sizeBlend;               /* 0xc8 */
    struct FxCurve colorCurveA;    /* 0xcc */
    struct FxCurve colorCurveB;    /* 0xd8 */
    struct FxCurve sizeCurveA;     /* 0xe4 */
    struct FxCurve sizeCurveB;     /* 0xf0 */
};
COD2_ASSERT_SIZE(Light, 0xfc);
COD2_ASSERT_OFFSET(Light, origin, 4);
COD2_ASSERT_OFFSET(Light, age, 0x3c);
COD2_ASSERT_OFFSET(Light, color, 0x6c);
COD2_ASSERT_OFFSET(Light, screenPos, 0x7c);
COD2_ASSERT_OFFSET(Light, size, 0x88);
COD2_ASSERT_OFFSET(Light, size2, 0x8c);
COD2_ASSERT_OFFSET(Light, colorBytes, 0x90);
COD2_ASSERT_OFFSET(Light, flags, 0xa8);
COD2_ASSERT_OFFSET(Light, colorBlend, 0xc4);
COD2_ASSERT_OFFSET(Light, sizeBlend, 0xc8);
COD2_ASSERT_OFFSET(Light, colorCurveA, 0xcc);
COD2_ASSERT_OFFSET(Light, colorCurveB, 0xd8);
COD2_ASSERT_OFFSET(Light, sizeCurveA, 0xe4);
COD2_ASSERT_OFFSET(Light, sizeCurveB, 0xf0);

/* Object with several vectors and a random-range scalar; pool element size 0x29c. */
struct Emitter {
    void *vtbl;                    /* 0x00 */
    unsigned char pad4[0x48 - 4];
    vec3_t vec48;                  /* 0x48 */
    unsigned char pad54[0xa8 - 0x54];
    unsigned int flags;            /* 0xa8 bit 0x10 cleared when field_b4 == 0 */
    unsigned char padac[0xb4 - 0xac];
    int field_b4;                  /* 0xb4 */
    unsigned char padb8[0x24c - 0xb8];
    vec3_t vec24c;                 /* 0x24c */
    vec3_t vec258;                 /* 0x258 */
    vec3_t vec264;                 /* 0x264 */
    int field270;                  /* 0x270 */
    float value274;                /* 0x274 */
    vec3_t pos;                    /* 0x278 */
    vec3_t vel;                    /* 0x284 */
    int field290;                  /* 0x290 */
    float base294;                 /* 0x294 */
    float range298;                /* 0x298 */
};
COD2_ASSERT_SIZE(Emitter, 0x29c);
COD2_ASSERT_OFFSET(Emitter, vec48, 0x48);
COD2_ASSERT_OFFSET(Emitter, flags, 0xa8);
COD2_ASSERT_OFFSET(Emitter, field_b4, 0xb4);
COD2_ASSERT_OFFSET(Emitter, vec24c, 0x24c);
COD2_ASSERT_OFFSET(Emitter, vec258, 0x258);
COD2_ASSERT_OFFSET(Emitter, vec264, 0x264);
COD2_ASSERT_OFFSET(Emitter, field270, 0x270);
COD2_ASSERT_OFFSET(Emitter, value274, 0x274);
COD2_ASSERT_OFFSET(Emitter, pos, 0x278);
COD2_ASSERT_OFFSET(Emitter, vel, 0x284);
COD2_ASSERT_OFFSET(Emitter, field290, 0x290);
COD2_ASSERT_OFFSET(Emitter, base294, 0x294);
COD2_ASSERT_OFFSET(Emitter, range298, 0x298);

/* Effect base object (0xc4 bytes, see chunk 054): only the span fields and the non-virtual
 * methods defined in chunk 054 are used here. */
struct FxArchive {
    int unknown00;
    byte reading;                  /* 0x04, non-zero while reading */
};
#define FXMS(ar) ((struct msgstream_s *)(ar))
struct Effect {
    unsigned char pad0[0xb8];
    int spanStart;                 /* 0xb8 */
    int spanEnd;                   /* 0xbc */
    unsigned char padc0[4];
    bool UpdateFraction();
    void Archive(struct FxArchive *ar);
    void CopyFromTemplate(const struct FxParticleTemplate *tmpl);
    void SubmitToRenderer(int mode);
};
COD2_ASSERT_SIZE(Effect, 0xc4);
struct Particle {                  /* Particle derives from Effect, 0x24c bytes */
    unsigned char raw[0x24c];
    void Archive(struct FxArchive *ar);
};
struct FxParticleTemplate;
struct FxCurveSrc;

/* Template-side view with the curve sources at +0x100.. and flags at +0x90. */
struct FxCurvePairTemplate {
    unsigned char pad0[0x90];
    unsigned int flags;            /* 0x90 0x2000 = blend, 2 = needs entity ref */
    unsigned char pad94[0x100 - 0x94];
    struct FxCurve curveA;         /* 0x100 */
    struct FxCurve curveB;         /* 0x10c */
    unsigned char pad118[0x130 - 0x118];
    struct FxCurve curveC;         /* 0x130 */
    struct FxCurve curveD;         /* 0x13c */
};
COD2_ASSERT_OFFSET(FxCurvePairTemplate, flags, 0x90);
COD2_ASSERT_OFFSET(FxCurvePairTemplate, curveA, 0x100);
COD2_ASSERT_OFFSET(FxCurvePairTemplate, curveB, 0x10c);
COD2_ASSERT_OFFSET(FxCurvePairTemplate, curveC, 0x130);
COD2_ASSERT_OFFSET(FxCurvePairTemplate, curveD, 0x13c);

/* ---- effect templates and scheduler ------------------------------------- */

struct FxRange {
    float min;
    float max;
};

/* Per-primitive effect template (only fields touched in this chunk). */
struct fx_primitive_s {
    unsigned char pad0[0x48];
    struct FxRange delay;          /* 0x48 min/max start delay */
    struct FxRange count;          /* 0x50 spawn-count range (read via FxRange_Random) */
    unsigned char pad58[4];
    float field5c;                 /* 0x5c */
    float minDistance;             /* 0x60 0 = no limit */
    float maxDistance;             /* 0x64 0 = no limit */
    unsigned char pad68[0x94 - 0x68];
    unsigned int flags;            /* 0x94 0x200 = spread delays, 0x400 = cull check */
    unsigned char pad98[0x2a0 - 0x98];
    int cullArg;                   /* 0x2a0 */
};
COD2_ASSERT_OFFSET(fx_primitive_s, delay, 0x48);
COD2_ASSERT_OFFSET(fx_primitive_s, count, 0x50);
COD2_ASSERT_OFFSET(fx_primitive_s, field5c, 0x5c);
COD2_ASSERT_OFFSET(fx_primitive_s, minDistance, 0x60);
COD2_ASSERT_OFFSET(fx_primitive_s, maxDistance, 0x64);
COD2_ASSERT_OFFSET(fx_primitive_s, flags, 0x94);
COD2_ASSERT_OFFSET(fx_primitive_s, cullArg, 0x2a0);

struct FxEffectTemplate {
    int field0;                    /* 0x00 */
    int primCount;                 /* 0x04 */
    struct fx_primitive_s *prims[1]; /* 0x08 primCount entries */
};
COD2_ASSERT_OFFSET(FxEffectTemplate, primCount, 4);
COD2_ASSERT_OFFSET(FxEffectTemplate, prims, 8);

/* Delayed spawn record queued by PlayEffect (operator-new'd, 0x50 bytes). */
struct FxScheduledEffect {
    struct FxEffectTemplate *effect;  /* 0x00 */
    int primIndex;                    /* 0x04 */
    int startTime;                    /* 0x08 helper time + delay */
    int bolt[2];                      /* 0x0c entity/bolt ref, -1/-1 when none */
    vec3_t origin;                    /* 0x14 */
    float axis[3][3];                 /* 0x20 */
    int seed;                         /* 0x44 */
    int loopIndex;                    /* 0x48 */
    struct FxScheduledEffect *next;   /* 0x4c */
};
COD2_ASSERT_SIZE(FxScheduledEffect, 0x50);
COD2_ASSERT_OFFSET(FxScheduledEffect, startTime, 8);
COD2_ASSERT_OFFSET(FxScheduledEffect, bolt, 0xc);
COD2_ASSERT_OFFSET(FxScheduledEffect, origin, 0x14);
COD2_ASSERT_OFFSET(FxScheduledEffect, axis, 0x20);
COD2_ASSERT_OFFSET(FxScheduledEffect, seed, 0x44);
COD2_ASSERT_OFFSET(FxScheduledEffect, loopIndex, 0x48);
COD2_ASSERT_OFFSET(FxScheduledEffect, next, 0x4c);

struct FxScheduler {
    int seed;                         /* 0x00 last random seed taken from the helper */
    struct FxScheduledEffect *head;   /* 0x04 delayed-spawn list */
    int count;                        /* 0x08 */
};
COD2_ASSERT_SIZE(FxScheduler, 0xc);

/* Origin + axis filled by FX_GetBoneOrientation for entity-attached effects (frame region is 56 bytes). */
struct FxPlacement {
    vec3_t origin;
    float axis[3][3];
    int spare[2];
};
COD2_ASSERT_SIZE(FxPlacement, 0x38);

/* Random-pick list: items array and a 16-bit count at +4. */
struct FxPickList {
    int *items;                       /* 0x00 */
    unsigned short count;             /* 0x04 (upper half of the dword unused here) */
};


/*
 * Call-site adapters for prototypes owned by other chunks whose recorded signature is too loose
 * or opaque for the types used here (shared prototype stays authoritative; residual adapters, see analysis/symbols/signature-requests.md).
 */
#define MsgStream_TransferBytes(s, p, n) MsgStream_TransferBytes((s), (unsigned char *)(p), (n))
#define MsgStream_ReadCompressed(s, p, n) MsgStream_ReadCompressed((s), (unsigned char *)(p), (n))
#define MsgStream_WriteCompressed(s, p, n) MsgStream_WriteCompressed((s), (const unsigned char *)(p), (n))
#define MsgStream_TransferValue(s, c) MsgStream_TransferValue((s), (struct msg_value_s *)(c))
#define Curve_IntegrateToKey(k, t) Curve_IntegrateToKey((const struct curve_table_s *)(k), (t))
#define Curve_InterpolateSegment(r, t) Curve_InterpolateSegment((const float *)(r), (t))
#define FxPool_FreeElement(e, sz) ((struct FxPoolBlock *(*)(void *, int))FxPool_FreeElement)((e), (sz))
#define FX_NoopB(h, arg) ((void (*)(struct FxSystem *, int))FX_NoopB)((h), (arg))
#define FX_IsCulledByPlanes2(h, o, a) ((char (*)(struct FxSystem *, const float *, int))FX_IsCulledByPlanes2)((h), (o), (a))
#define FX_DrawPrimitive(s, e, p, b, o, ax, d, i) \
    ((void (*)(struct FxScheduler *, struct FxEffectTemplate *, struct fx_primitive_s *, int *, float *, float *, int, int))FX_DrawPrimitive)((s), (e), (p), (b), (o), (ax), (d), (i))
#define FxCurve_InitFromTemplate(s, d) FxCurve_InitFromTemplate((const struct FxCurveSrc *)(s), (struct FxCurve *)(d))

#endif /* CHUNK_055_TYPES_H */
