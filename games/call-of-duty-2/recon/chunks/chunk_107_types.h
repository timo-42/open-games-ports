/*
 * Chunk 107 types: 87 functions recovered from .eh_frame FDE starts (0x0811a004..0x0813c5a8) that
 * Ghidra's auto-analysis missed. They are scattered between already reconstructed chunks:
 *   - game module: script entity methods (solid/notsolid/getent/getentarray), trigger spawn
 *     functions and entClass callbacks, G_PickTarget and small entity helpers
 *   - effects ("Fx") object model: virtual-slot overrides and destructors of Particle,
 *     OrientedParticle, Cloud, Line, Tail, Cylinder, Emitter, Light, Flash
 *   - effects system wrappers, serialisation and frame loops
 *   - localisation string assembly and PunkBuster server callbacks
 *
 * Layouts are partial views recovered from consistent offsets; the Fx class layouts follow the
 * chunk 054/055 reconstructions (class names come from the RTTI typeinfo strings next to the
 * vtables: 6Effect, 8Particle, 16OrientedParticle, 5Cloud, 4Line, 4Tail, 8Cylinder, 7Emitter,
 * 5Light, 5Flash). Field names are inferred from usage; offsets are observed.
 */
#ifndef COD2_CHUNK_107_TYPES_H
#define COD2_CHUNK_107_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ------------------------------------------------------------------ game entity */

/* gentity_s (canonical, cod2_canon.h) replaces the local gentity107_s view; trigger fields live in
 * gentity_s::u.trigger (threshold/accumulate/timestamp). The client pointer is the canonical
 * gclient_s *, whose origin lives at +0x14 (used through a byte view below). */

/* Entity field table entry (stride 0x10, base 0x0815e820) = canonical entityFieldDef_s.
 * type 3 = script string handle. Kept as a chunk-private view for the ofs/type accessors. */
typedef struct entfield107_s {
    const char *name;              /* 0x00 */
    int ofs;                       /* 0x04 */
    int type;                      /* 0x08 */
    void *setter;                  /* 0x0c */
} entfield107_t;
COD2_ASSERT_SIZE(entfield107_t, 0x10);

/* ------------------------------------------------------------------ Fx object model */

struct FxBindingRef {
    void *ptr;                     /* refcounted FxBinding */
};

/* Curve evaluator state: key table, cached segment index, per-instance scale. */
struct FxCurve {
    void *keys;                    /* 0x00 */
    int cacheIndex;                /* 0x04 */
    float scale;                   /* 0x08 */
};
COD2_ASSERT_SIZE(FxCurve, 0x0c);

/* Template side of a curve: key table plus a [min,max] scale range. */
struct FxCurveSrc {
    void *keys;                    /* 0x00 */
    float scaleMin;                /* 0x04 */
    float scaleMax;                /* 0x08 */
};
COD2_ASSERT_SIZE(FxCurveSrc, 0x0c);

/* Particle/Light template: curves[24] at 0x100, flags at 0x90. */
struct FxParticleTemplate {
    float axes[9];                 /* 0x00 */
    byte unknown24[0x90 - 0x24];   /* 0x24 */
    unsigned int flags;            /* 0x90 0x2000 colour range, 0x4000 alpha range, 0x8000 size range */
    byte unknown94[0x100 - 0x94];  /* 0x94 */
    FxCurveSrc curves[24];         /* 0x100 .. 0x220 */
};
COD2_ASSERT_OFFSET(FxParticleTemplate, flags, 0x90);
COD2_ASSERT_OFFSET(FxParticleTemplate, curves, 0x100);

/* Object behind the global pointer at 0x08188ac0 (only the used fields). */
struct FxSystem {
    int unknown00;                 /* 0x00 */
    int time;                      /* 0x04 current time in ms */
    int unknown08;                 /* 0x08 */
    int frameTime;                 /* 0x0c */
    byte unknown10[0x70];          /* 0x10 */
    int traceMask;                 /* 0x80 */
};
COD2_ASSERT_OFFSET(FxSystem, time, 0x04);
COD2_ASSERT_OFFSET(FxSystem, traceMask, 0x80);

/* Effect flag bits (offset 0xa8). */
enum {
    FX107_FLAG_SUBMIT_ON_EXPIRE = 0x00000010,
    FX107_FLAG_PLAY_EFFECT_A    = 0x00000200,
    FX107_FLAG_KILL_ON_IMPACT   = 0x00000400,
    FX107_FLAG_VISIBILITY       = 0x00001000
};

struct Effect {
    void **vtbl;                   /* 0x00 */
    float origin[3];               /* 0x04 */
    int field10;                   /* 0x10 */
    float mins[3];                 /* 0x14 */
    float maxs[3];                 /* 0x20 */
    int impactEffectId;            /* 0x2c */
    int effectTemplate;            /* 0x30 FxEffectTemplate * played by slot 2 */
    int field34;                   /* 0x34 */
    int field38;                   /* 0x38 */
    float frac;                    /* 0x3c lifetime fraction 0..1 */
    int templateId;                /* 0x40 */
    float angle;                   /* 0x44 */
    float axes[9];                 /* 0x48 */
    float color[3];                /* 0x6c colour (Light reads it as the draw colour) */
    int unknown78;                 /* 0x78 */
    float worldOrigin[3];          /* 0x7c */
    float size;                    /* 0x88 */
    float size2;                   /* 0x8c */
    byte rgba[4];                  /* 0x90 */
    int frameIndex;                /* 0x94 */
    float prevSize;                /* 0x98 */
    float prevWorldOrigin[3];      /* 0x9c */
    unsigned int flags;            /* 0xa8 */
    int cachedHandle;              /* 0xac */
    int fieldB0;                   /* 0xb0 */
    int fieldB4;                   /* 0xb4 */
    int startTime;                 /* 0xb8 */
    int endTime;                   /* 0xbc */
    FxBindingRef binding;          /* 0xc0 */

    bool UpdateFraction();                       /* 0x081247f0 */
    bool ResolveBinding(const float **outFrame); /* 0x0812474e */
    int GetTraceMask();                          /* 0x081247a4 */
    void SubmitToRenderer(int mode);             /* 0x08124a90 */
};
COD2_ASSERT_SIZE(Effect, 0xc4);
COD2_ASSERT_OFFSET(Effect, effectTemplate, 0x30);
COD2_ASSERT_OFFSET(Effect, frac, 0x3c);
COD2_ASSERT_OFFSET(Effect, axes, 0x48);
COD2_ASSERT_OFFSET(Effect, worldOrigin, 0x7c);
COD2_ASSERT_OFFSET(Effect, size, 0x88);
COD2_ASSERT_OFFSET(Effect, rgba, 0x90);
COD2_ASSERT_OFFSET(Effect, prevSize, 0x98);
COD2_ASSERT_OFFSET(Effect, prevWorldOrigin, 0x9c);
COD2_ASSERT_OFFSET(Effect, flags, 0xa8);
COD2_ASSERT_OFFSET(Effect, startTime, 0xb8);
COD2_ASSERT_OFFSET(Effect, binding, 0xc0);

struct Particle : Effect {
    float offset[3];               /* 0xc4 */
    float basis[9];                /* 0xd0 */
    float gravity;                 /* 0xf4 */
    byte unknownF8[8];             /* 0xf8 */
    float bounce;                  /* 0x100 */
    byte useSize2Curve;            /* 0x104 */
    byte pad105[3];
    int frameOffset;               /* 0x108 */
    float frameRate;               /* 0x10c */
    int loopMode;                  /* 0x110 */
    int loopCount;                 /* 0x114 */
    float colorRand;               /* 0x118 */
    float alphaRand;               /* 0x11c */
    float sizeRand;                /* 0x120 */
    float size2Rand;               /* 0x124 */
    float rotationRand;            /* 0x128 */
    float velocityRand[3];         /* 0x12c */
    float accelRand[3];            /* 0x138 */
    byte curves144[0x24c - 0x144]; /* 0x144 24 FxCurve entries (colour, alpha, size, size2, rotation, velocity, accel pairs) */

    void InitCurves(const FxParticleTemplate *tmpl);  /* 0x0812505e */
    bool UpdateMotion(const float *frame);            /* 0x081256d4 */
    void UpdateSize();                                /* 0x0812637e */
    void UpdateSize2();                               /* 0x08126402 */
    void UpdateColor();                               /* 0x081264a2 */
    void UpdateAlpha();                               /* 0x08126530 */
    void UpdateRotation();                            /* 0x0812669e */
    int UpdateFrame();                                /* 0x0812672e */

    /* chunk 107: virtual slots */
    void PlayRandomDirEffect();                       /* slot 2 0x08124cda */
    bool Update();                                    /* slot 3 0x08124e98 */
    byte IsCulled();                                  /* slot 4 0x08124e12 */
    void Draw();                                      /* slot 5 0x08124e50 */
    float GetRayVisibility(const float *origin, const float *dir, float radius); /* slot 6 0x0812532a */
};
COD2_ASSERT_SIZE(Particle, 0x24c);
COD2_ASSERT_OFFSET(Particle, offset, 0xc4);
COD2_ASSERT_OFFSET(Particle, curves144, 0x144);

struct OrientedParticle : Particle {
    float extra24c[3];             /* 0x24c local-space axis, copied/rotated into axes[0..2] */

    bool Update();                                    /* slot 3 0x08126f1a */
    byte IsCulled();                                  /* slot 4 0x08126ec0 */
    void Draw();                                      /* slot 5 0x08126efe */
};
COD2_ASSERT_SIZE(OrientedParticle, 0x258);

struct Cloud : Particle {
    float axis[3];                 /* 0x24c random unit direction */
    float speed;                   /* 0x258 */
    byte useSpeedCurve;            /* 0x25c */
    byte pad25d[3];
    float speedRand;               /* 0x260 */
    FxCurve speedCurve;            /* 0x264 */
    FxCurve speedCurveAlt;         /* 0x270 */

    void InitRandomAxis();                            /* 0x0812722e */
    byte IsCulled();                                  /* slot 4 0x081271c8 */
    void Draw();                                      /* slot 5 0x08127306 */
    void InitCurves(const FxParticleTemplate *tmpl);  /* slot 8 0x08127322 */
};
COD2_ASSERT_SIZE(Cloud, 0x27c);
COD2_ASSERT_OFFSET(Cloud, speedCurve, 0x264);
COD2_ASSERT_OFFSET(Cloud, speedCurveAlt, 0x270);

struct Line : Particle {
    float endPoint[3];             /* 0x24c local-space end point, mapped into prevWorldOrigin */

    bool Update();                                    /* slot 3 0x081277de */
    byte IsCulled();                                  /* slot 4 0x0812776a */
    void Draw();                                      /* slot 5 0x081277c2 */
};
COD2_ASSERT_SIZE(Line, 0x258);
COD2_ASSERT_OFFSET(Line, endPoint, 0x24c);

struct Tail : Particle {
    float prevOrigin[3];           /* 0x24c origin before the motion step */
    float length;                  /* 0x258 */
    float lengthRand;              /* 0x25c */
    FxCurve lengthCurve;           /* 0x260 */
    FxCurve lengthCurveAlt;        /* 0x26c */

    void UpdateLength();                              /* 0x08127bea */
    void UpdateTailPoint(const float *frame);         /* 0x08127c6a */
    bool Update();                                    /* slot 3 0x08127ad0 */
    void Draw();                                      /* slot 5 0x08127ab4 */
    void InitCurves(const FxParticleTemplate *tmpl);  /* slot 8 0x08127a62 */
};
COD2_ASSERT_SIZE(Tail, 0x278);
COD2_ASSERT_OFFSET(Tail, length, 0x258);
COD2_ASSERT_OFFSET(Tail, lengthCurve, 0x260);

struct Cylinder : Tail {
    bool Update();                                    /* slot 3 0x08127f52 */
    byte IsCulled();                                  /* slot 4 0x08127ede */
    void Draw();                                      /* slot 5 0x08127f36 */
};
COD2_ASSERT_SIZE(Cylinder, 0x278);

struct Emitter : Particle {
    float prevPos[3];              /* 0x24c */
    float field258[3];             /* 0x258 */
    float lastFrameOrigin[3];      /* 0x264 */
    int lastEmitTime;              /* 0x270 */
    float minStep;                 /* 0x274 */
    float pos[3];                  /* 0x278 */
    float vel[3];                  /* 0x284 */
    int emitTemplateId;            /* 0x290 */
    float stepBase;                /* 0x294 */
    float stepRange;               /* 0x298 */

    void EmitElements(const float *frameVel, const float *frame);  /* 0x08128456 */
    bool Update();                                    /* slot 3 0x081281e0 */
};
COD2_ASSERT_SIZE(Emitter, 0x29c);
COD2_ASSERT_OFFSET(Emitter, lastFrameOrigin, 0x264);
COD2_ASSERT_OFFSET(Emitter, lastEmitTime, 0x270);
COD2_ASSERT_OFFSET(Emitter, vel, 0x284);

/* Light (RTTI "5Light", derives from Effect, pool element 0xfc); Flash (RTTI "5Flash") derives from Light. */
struct Light : Effect {
    float colorBlend;              /* 0xc4 */
    float sizeBlend;               /* 0xc8 */
    FxCurve colorCurveA;           /* 0xcc */
    FxCurve colorCurveB;           /* 0xd8 */
    FxCurve sizeCurveA;            /* 0xe4 */
    FxCurve sizeCurveB;            /* 0xf0 */

    void InitCurves(const FxParticleTemplate *tmpl);  /* slot 8 0x08128c74 */
    byte IsCulled();                                  /* slot 4 0x08128cec */
    void Draw();                                      /* slot 5 0x08128d2a */
    bool Update();                                    /* slot 3 0x08128d74 */
};
COD2_ASSERT_SIZE(Light, 0xfc);
COD2_ASSERT_OFFSET(Light, color, 0x6c);
COD2_ASSERT_OFFSET(Light, colorBlend, 0xc4);
COD2_ASSERT_OFFSET(Light, colorCurveA, 0xcc);
COD2_ASSERT_OFFSET(Light, sizeCurveB, 0xf0);

struct Flash : Light {
};
COD2_ASSERT_SIZE(Flash, 0xfc);

/* ------------------------------------------------------------------ effects system views */

/* Serialisation stream temporary built by MsgStream_ResetAlt/MsgStream_InitWrite (0x28 bytes in the frame). */
struct fx107_stream_s {
    int word[10];
};
COD2_ASSERT_SIZE(fx107_stream_s, 0x28);

/* Effect-list virtual slot 6: visibility/attenuation along a ray. */
typedef float (*fx107_visibility_fn)(void *self, const float *origin, const float *dir, float radius);

/* ------------------------------------------------------------------ globals */


extern int g_08a5c9a0;                       /* list A count */
extern int g_08a5c9a4;                       /* list B count */
extern int g_08a5c9b4;
extern int g_08a5c9b8;
extern int g_08a5c9bc;
extern int g_08a5c9c0;
extern int g_08a5cae0;
extern int g_08a5cae4;



/* ------------------------------------------------------------------ out-of-chunk callees */

/* Defined in chunk 055 (FxScheduler::Clear, 0x0812ac0a); not yet in the generated prototype header. */
/* Defined in chunk 058 (0x08132e82 / 0x08133fa0); not yet in the generated prototype header. */

#endif
