/*
 * chunk 054 types: effects ("Fx") object model of cod2_lnxded 1.3
 * (Effect / Particle / Cloud / Line / OrientedParticle / Tail / Cylinder / Emitter),
 * effects block pool, refcounted frame bindings.
 *
 * Class names come from RTTI typeinfo strings next to the vtables
 * (6Effect, 8Particle, 5Cloud, 4Line, 16OrientedParticle, 4Tail, 8Cylinder, 7Emitter).
 * Field names are inferred from usage; offsets are observed.
 */
#ifndef COD2_CHUNK_054_TYPES_H
#define COD2_CHUNK_054_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ---------------------------------------------------------------- pool */

/* 64 blocks of 0x8000 bytes at 0x08853380; a 16 byte trailer holds the free list. */
struct FxPoolBlock {
    byte data[0x7ff0];
    int freeCount;          /* 0x7ff0, -1 = block unused */
    void *freeHead;         /* 0x7ff4, singly linked list through the first word of each element */
    FxPoolBlock *prev;      /* 0x7ff8 */
    FxPoolBlock *next;      /* 0x7ffc */
};
COD2_ASSERT_SIZE(FxPoolBlock, 0x8000);
COD2_ASSERT_OFFSET(FxPoolBlock, freeCount, 0x7ff0);
COD2_ASSERT_OFFSET(FxPoolBlock, next, 0x7ffc);

#define FX_POOL_BLOCK_COUNT 64
#define FX_POOL_BLOCK_SIZE 0x8000
#define FX_POOL_BYTES 0x200000

/* ------------------------------------------------------------ bindings */

struct FxBindingKey {
    int key0;
    int key1;
};

/* Intrusive refcounted record (0x44 bytes) looked up by a pair of ints. */
struct FxBinding {
    int refCount;           /* 0x00 */
    int field04;            /* 0x04 */
    byte unknown08[0x30];   /* 0x08, never touched by this chunk */
    FxBinding *next;        /* 0x38 list link (head = g_fxBindingList) */
    int key0;               /* 0x3c */
    int key1;               /* 0x40 */
};
COD2_ASSERT_SIZE(FxBinding, 0x44);
COD2_ASSERT_OFFSET(FxBinding, next, 0x38);
COD2_ASSERT_OFFSET(FxBinding, key0, 0x3c);

/* Smart pointer to an FxBinding (4 bytes). */
struct FxRef {
    FxBinding *ptr;
};

/* Archive streams are msgstream_s objects of the (unrecovered) message layer. */
#define FXMS(ar) ((struct msgstream_s *)(ar))

/* ------------------------------------------------------------- engine */

/* Object behind the global pointer at 0x08188ac0 (only the used fields). */
struct FxSystem {
    int unknown00;          /* 0x00 */
    int time;               /* 0x04 current time in ms */
    int unknown08;          /* 0x08 */
    int frameTime;          /* 0x0c ms since last frame */
    byte unknown10[0x70];   /* 0x10 */
    int traceMask;          /* 0x80 */
};
COD2_ASSERT_OFFSET(FxSystem, time, 0x04);
COD2_ASSERT_OFFSET(FxSystem, frameTime, 0x0c);
COD2_ASSERT_OFFSET(FxSystem, traceMask, 0x80);

struct FxScheduler;         /* object at global 0x08a53804, opaque here */

/* Result of the world trace used by moving effects (0x38 bytes). */
struct FxTrace {
    float fraction;         /* 0x00 */
    float normal[3];        /* 0x04 */
    int surfaceFlags;       /* 0x10 */
    int contents;           /* 0x14 */
    void *material;         /* 0x18 */
    int hitType;            /* 0x1c */
    uint16 hitId;           /* 0x20 */
    byte allSolid;          /* 0x22 */
    byte startSolid;        /* 0x23 */
    byte unknown24[0x14];   /* 0x24 */
};
COD2_ASSERT_SIZE(FxTrace, 0x38);
COD2_ASSERT_OFFSET(FxTrace, normal, 0x04);
COD2_ASSERT_OFFSET(FxTrace, allSolid, 0x22);
COD2_ASSERT_OFFSET(FxTrace, startSolid, 0x23);

/* Descriptor handed to the renderer submit call (0x74 bytes). */
struct FxRenderDesc {
    int mode;               /* 0x00 */
    unsigned int flags;     /* 0x04 */
    byte unknown08[0x0c];   /* 0x08 */
    float axes[9];          /* 0x14 */
    float prevSize;         /* 0x38 */
    float origin[3];        /* 0x3c */
    float prevOrigin[3];    /* 0x48 */
    int templateId;         /* 0x54 */
    byte rgba[4];           /* 0x58 */
    int unknown5c;          /* 0x5c */
    int frameIndex;         /* 0x60 */
    float size;             /* 0x64 */
    float size2;            /* 0x68 */
    float angle;            /* 0x6c */
    int unknown70;          /* 0x70 */
};
COD2_ASSERT_SIZE(FxRenderDesc, 0x74);
COD2_ASSERT_OFFSET(FxRenderDesc, axes, 0x14);
COD2_ASSERT_OFFSET(FxRenderDesc, origin, 0x3c);
COD2_ASSERT_OFFSET(FxRenderDesc, rgba, 0x58);
COD2_ASSERT_OFFSET(FxRenderDesc, size, 0x64);

/* Serialisation stream; byte at +4 is non-zero while reading. */
struct FxArchive {
    int unknown00;
    byte reading;
};
COD2_ASSERT_OFFSET(FxArchive, reading, 0x04);

/* Curve evaluator state: key table, cached segment index, per-instance scale. */
struct FxCurve {
    void *keys;             /* 0x00 */
    int cacheIndex;         /* 0x04 */
    float scale;            /* 0x08 */
};
COD2_ASSERT_SIZE(FxCurve, 0x0c);

/* Template side of a curve: key table plus a [min,max] scale range. */
struct FxCurveSrc {
    void *keys;
    float scaleMin;
    float scaleMax;
};
COD2_ASSERT_SIZE(FxCurveSrc, 0x0c);

struct FxParticleTemplate {
    float axes[9];              /* 0x00 three basis vectors */
    byte unknown24[0xdc];       /* 0x24 */
    FxCurveSrc curves[24];      /* 0x100 .. 0x220 */
};
COD2_ASSERT_OFFSET(FxParticleTemplate, curves, 0x100);

/* Entry of the per-frame draw list at 0x08a53d00 (count at 0x08a53cb0). */
struct FxDrawEntry {
    float origin[3];
    float sizeSq;
    float transparency;
};
COD2_ASSERT_SIZE(FxDrawEntry, 0x14);

/* Effect flag bits (offset 0xa8). Meaning inferred from the code that tests them. */
enum {
    FXF_SUBMIT_MODE_A     = 0x00000001, /* sets renderer desc bit 8 */
    FXF_ON_EXPIRE_SUBMIT  = 0x00000010,
    FXF_COLLIDE           = 0x00000020,
    FXF_TRACE_BOUNDS      = 0x00000040, /* use mins/maxs in the trace */
    FXF_ALPHA_ONLY        = 0x00000080, /* fade alpha instead of rgb */
    FXF_EMITTER_ACTIVE    = 0x00000100,
    FXF_KILL_ON_IMPACT    = 0x00000400,
    FXF_IMPACT_EFFECT     = 0x00000800,
    FXF_COLOR_RANGE       = 0x00002000,
    FXF_ALPHA_RANGE       = 0x00004000,
    FXF_SIZE_RANGE        = 0x00008000,
    FXF_SIZE2_RANGE       = 0x00010000,
    FXF_SPEED_RANGE       = 0x00020000,
    FXF_ROTATION_RANGE    = 0x00040000,
    FXF_VELOCITY_RANGE    = 0x00080000,
    FXF_ACCEL_RANGE       = 0x00100000,
    FXF_VELOCITY_WORLD    = 0x00200000,
    FXF_ACCEL_WORLD       = 0x00400000,
    FXF_STOPPED           = 0x01000000,
    FXF_CLAMP_TRACE_MASK  = 0x02000000,
    FXF_SUBMIT_MODE_B     = 0x04000000
};

/* ------------------------------------------------------------- classes */

struct Effect {
    void **vtbl;                /* 0x00 */
    float origin[3];            /* 0x04 */
    int field10;                /* 0x10 */
    float mins[3];              /* 0x14 */
    float maxs[3];              /* 0x20 */
    int impactEffectId;         /* 0x2c */
    int field30;                /* 0x30 */
    int field34;                /* 0x34 */
    int field38;                /* 0x38 */
    float frac;                 /* 0x3c lifetime fraction 0..1 */
    int templateId;             /* 0x40 */
    float angle;                /* 0x44 degrees */
    float axes[9];              /* 0x48 */
    byte unknown6c[0x10];       /* 0x6c */
    float worldOrigin[3];       /* 0x7c */
    float size;                 /* 0x88 */
    float size2;                /* 0x8c */
    byte rgba[4];               /* 0x90 */
    int frameIndex;             /* 0x94 */
    float prevSize;             /* 0x98 */
    float prevWorldOrigin[3];   /* 0x9c */
    unsigned int flags;         /* 0xa8 */
    int cachedHandle;           /* 0xac */
    int fieldB0;                /* 0xb0 */
    int fieldB4;                /* 0xb4 */
    int startTime;              /* 0xb8 */
    int endTime;                /* 0xbc */
    FxRef binding;       /* 0xc0 */

    bool UpdateFraction();
    bool ResolveBinding(const float **outFrame);
    int GetTraceMask();
    void SetTimes(int start, int end);
    int GetAge();
    void SetBinding(FxRef *ref);
    void OnExpire();
    bool IsCulled();
    void Draw();
    int GetTypeId();
    void Archive(FxArchive *ar);
    void CopyFromTemplate(const FxParticleTemplate *tmpl);
    void SubmitToRenderer(int mode);
};
COD2_ASSERT_SIZE(Effect, 0xc4);
COD2_ASSERT_OFFSET(Effect, frac, 0x3c);
COD2_ASSERT_OFFSET(Effect, axes, 0x48);
COD2_ASSERT_OFFSET(Effect, worldOrigin, 0x7c);
COD2_ASSERT_OFFSET(Effect, size, 0x88);
COD2_ASSERT_OFFSET(Effect, rgba, 0x90);
COD2_ASSERT_OFFSET(Effect, prevWorldOrigin, 0x9c);
COD2_ASSERT_OFFSET(Effect, flags, 0xa8);
COD2_ASSERT_OFFSET(Effect, startTime, 0xb8);
COD2_ASSERT_OFFSET(Effect, binding, 0xc0);

struct Particle : Effect {
    float offset[3];            /* 0xc4 accumulated offset (reflection residue) */
    float basis[9];             /* 0xd0 three vec3 axes */
    float gravity;              /* 0xf4 */
    byte unknownF8[8];          /* 0xf8 */
    float bounce;               /* 0x100 */
    byte useSize2Curve;         /* 0x104 */
    byte pad105[3];
    int frameOffset;            /* 0x108 */
    float frameRate;            /* 0x10c */
    int loopMode;               /* 0x110 */
    int loopCount;              /* 0x114 */
    float colorRand;            /* 0x118 */
    float alphaRand;            /* 0x11c */
    float sizeRand;             /* 0x120 */
    float size2Rand;            /* 0x124 */
    float rotationRand;         /* 0x128 */
    float velocityRand[3];      /* 0x12c */
    float accelRand[3];         /* 0x138 */
    FxCurve colorCurve;         /* 0x144 */
    FxCurve colorCurveAlt;      /* 0x150 */
    FxCurve alphaCurve;         /* 0x15c */
    FxCurve alphaCurveAlt;      /* 0x168 */
    FxCurve sizeCurve;          /* 0x174 */
    FxCurve sizeCurveAlt;       /* 0x180 */
    FxCurve size2Curve;         /* 0x18c */
    FxCurve size2CurveAlt;      /* 0x198 */
    FxCurve rotationCurve;      /* 0x1a4 */
    FxCurve rotationCurveAlt;   /* 0x1b0 */
    FxCurve velCurveX;          /* 0x1bc */
    FxCurve velCurveY;          /* 0x1c8 */
    FxCurve velCurveZ;          /* 0x1d4 */
    FxCurve velCurveAltX;       /* 0x1e0 */
    FxCurve velCurveAltY;       /* 0x1ec */
    FxCurve velCurveAltZ;       /* 0x1f8 */
    FxCurve accCurveX;          /* 0x204 */
    FxCurve accCurveY;          /* 0x210 */
    FxCurve accCurveZ;          /* 0x21c */
    FxCurve accCurveAltX;       /* 0x228 */
    FxCurve accCurveAltY;       /* 0x234 */
    FxCurve accCurveAltZ;       /* 0x240 */

    void SetBasis(const float *axes3);
    void InitCurves(const FxParticleTemplate *tmpl);
    void SetVelocityRand(float x, float y, float z);
    void SetAccelRand(float x, float y, float z);
    void AppendBlocker();
    float ClampedRatio(float num, float den);
    void StopMoving();
    void HandleImpact(const float *frame, float t, const float *vel, float traceFraction,
                      const float *normal);
    bool UpdateMotion(const float *frame);
    void EvalVelocityTerm(float t, float *out, const float *frame);
    void EvalAccelTerm(float t, float *out, const float *frame);
    void EvalGravityTerm(float t, float *out, const float *frame);
    void EvalVelocityIntegral(float t, float *out);
    void EvalAccelIntegral(float t, float *out);
    void GetGravityVec(int ms, float *out);
    void EvalDisplacement(int ms, float *out);
    void GetStartVelocity(float *out);
    void EvalVelocityTotal(float t, float *out, const float *frame);
    void UpdateSize();
    void UpdateSize2();
    void UpdateColor();
    void UpdateAlpha();
    void UpdateRotation();
    int UpdateFrame();
    int GetTypeId();
    void Archive(FxArchive *ar);
    void CopyFromTemplate(const FxParticleTemplate *tmpl);
};
COD2_ASSERT_SIZE(Particle, 0x24c);
COD2_ASSERT_OFFSET(Particle, offset, 0xc4);
COD2_ASSERT_OFFSET(Particle, basis, 0xd0);
COD2_ASSERT_OFFSET(Particle, gravity, 0xf4);
COD2_ASSERT_OFFSET(Particle, bounce, 0x100);
COD2_ASSERT_OFFSET(Particle, useSize2Curve, 0x104);
COD2_ASSERT_OFFSET(Particle, frameOffset, 0x108);
COD2_ASSERT_OFFSET(Particle, colorRand, 0x118);
COD2_ASSERT_OFFSET(Particle, velocityRand, 0x12c);
COD2_ASSERT_OFFSET(Particle, accelRand, 0x138);
COD2_ASSERT_OFFSET(Particle, colorCurve, 0x144);
COD2_ASSERT_OFFSET(Particle, rotationCurve, 0x1a4);
COD2_ASSERT_OFFSET(Particle, velCurveX, 0x1bc);
COD2_ASSERT_OFFSET(Particle, velCurveAltZ, 0x1f8);
COD2_ASSERT_OFFSET(Particle, accCurveX, 0x204);
COD2_ASSERT_OFFSET(Particle, accCurveAltZ, 0x240);

struct OrientedParticle : Particle {
    float extra24c[3];          /* 0x24c */
    int GetTypeId();
    void Archive(FxArchive *ar);
};
COD2_ASSERT_SIZE(OrientedParticle, 0x258);

struct Cloud : Particle {
    float axis[3];              /* 0x24c random unit direction */
    float speed;                /* 0x258 */
    byte useSpeedCurve;         /* 0x25c */
    byte pad25d[3];
    float speedRand;            /* 0x260 */
    FxCurve speedCurve;         /* 0x264 */
    FxCurve speedCurveAlt;      /* 0x270 */

    void OnExpire();
    void InitRandomAxis();
    void UpdateSpeed();
    bool Update();
    int GetTypeId();
    void Archive(FxArchive *ar);
    void CopyFromTemplate(const FxParticleTemplate *tmpl);
};
COD2_ASSERT_SIZE(Cloud, 0x27c);
COD2_ASSERT_OFFSET(Cloud, speed, 0x258);
COD2_ASSERT_OFFSET(Cloud, useSpeedCurve, 0x25c);
COD2_ASSERT_OFFSET(Cloud, speedCurve, 0x264);

struct Line : Particle {
    void OnExpire();
    float extra24c[3];          /* 0x24c */
    int GetTypeId();
    void Archive(FxArchive *ar);
};
COD2_ASSERT_SIZE(Line, 0x258);

struct Tail : Particle {
    float endPoint[3];          /* 0x24c */
    float length;               /* 0x258 */
    float lengthRand;           /* 0x25c */
    FxCurve lengthCurve;        /* 0x260 */
    FxCurve lengthCurveAlt;     /* 0x26c */

    bool TraceSegment();
    void UpdateLengthAndPoint();
    void UpdateLength();
    void UpdateTailPoint(const float *frame);
    int GetTypeId();
    void Archive(FxArchive *ar);
    void CopyFromTemplate(const FxParticleTemplate *tmpl);
};
COD2_ASSERT_SIZE(Tail, 0x278);
COD2_ASSERT_OFFSET(Tail, length, 0x258);
COD2_ASSERT_OFFSET(Tail, lengthCurve, 0x260);

struct Cylinder : Tail {
    int GetTypeId();
    void Archive(FxArchive *ar);
};
COD2_ASSERT_SIZE(Cylinder, 0x278);

struct Emitter : Particle {
    float pos[3];               /* 0x24c */
    float vel[3];               /* 0x258 */
    float drift[3];             /* 0x264 */
    int nextEmitTime;           /* 0x270 */
    float minStep;              /* 0x274 */
    byte unknown278[0x18];      /* 0x278 */
    int emitTemplateId;         /* 0x290 */
    float stepBase;             /* 0x294 */
    float stepRange;            /* 0x298 */

    bool IsCulled();
    void Draw();
    void EmitElements(const float *frameVel, const float *frame);
};
COD2_ASSERT_OFFSET(Emitter, pos, 0x24c);
COD2_ASSERT_OFFSET(Emitter, nextEmitTime, 0x270);
COD2_ASSERT_OFFSET(Emitter, emitTemplateId, 0x290);
COD2_ASSERT_OFFSET(Emitter, stepRange, 0x298);


/*
 * Call-site adapters for prototypes owned by other chunks whose recorded signature is too loose
 * (uint32/int pointers) or opaque for the types used here. The shared prototype stays
 * authoritative; each adapter only casts the arguments. Residual adapters: none of them was covered by a
 * signature-fix request (see analysis/symbols/signature-requests.md).
 */
#define IntPair_Init(dst, keys) IntPair_Init((int *)(dst), (int)(keys))
#define FxRange_Random(r) FxRange_Random((const struct FxRange *)(r))
#define Orient_VecWorldToLocal(f, v, o) Orient_VecWorldToLocal((const struct Orient *)(f), (v), (o))
#define FX_InitSampleRecord ((void (*)(struct FxSystem *, struct FxTrace *, const float *, const float *, const float *, const float *, int, int))FX_InitSampleRecord)
#define FX_RendererSubmit_Stub ((void (*)(struct FxSystem *, struct FxRenderDesc *, int))FX_RendererSubmit_Stub)
#define FX_GetSpriteFrameCount_Stub ((int (*)(struct FxSystem *, int))FX_GetSpriteFrameCount_Stub)
#define FX_IsSegmentCulled ((int (*)(struct FxSystem *, const float *, const float *, float, float, int))FX_IsSegmentCulled)
#define MsgStream_TransferBytes(s, p, n) MsgStream_TransferBytes((s), (unsigned char *)(p), (n))
#define MsgStream_TransferValue(s, c) MsgStream_TransferValue((s), (struct msg_value_s *)(c))
#define FXTEMPLATE(id) ((struct FxEffectTemplate *)(id))

#endif
