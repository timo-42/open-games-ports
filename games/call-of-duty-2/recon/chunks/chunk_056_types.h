/*
 * chunk_056 types: effect (FX) template loader, primitive templates, playback
 * pool, frustum/cull planes, FX field parsers and envelope/curve builders of
 * cod2_lnxded 1.3, 0x0812b5fe..0x0812e89e (100 functions).
 *
 * Layouts are recovered from consistent offsets in the Ghidra pseudocode and
 * the disassembly. Field names come from the FX key strings dispatched by
 * the out-of-chunk key parser at 0x08130248 ("count", "life", "delay", ...)
 * where a key writes a field directly. Fields without a dispatch key keep
 * unk names. Pool block geometry (0x8000 bytes, 409 elements of 0x50 bytes,
 * header at 0x7ff0) is inferred from 0x0812c060 and 0x0812c186.
 */
#ifndef COD2_CHUNK_056_TYPES_H
#define COD2_CHUNK_056_TYPES_H

#include "cod2_types.h"

/* ------------------------------------------------------------------ */
/* Opaque types owned by other chunks.                                 */
/* ------------------------------------------------------------------ */
struct fx_emitter_s;     /* scene emitter passed as first pointer to 0x0812b8a8 */
struct fx_source_s;      /* placement source, vec3 read at +0x0c (0x0812b5fe)   */
struct fx_stream_s;      /* serialisation stream (0x08129e6c et al.)            */

/* ------------------------------------------------------------------ */
/* Pool (0x0812c030..0x0812c186).                                      */
/* ------------------------------------------------------------------ */
typedef struct FxPoolBlock FxPoolBlock;

/* Pool descriptor: capacity per block and the head of the block list. */
typedef struct FxPool {
    int blockCapacity;               /* 0x00: elements per block (0x30 or 0x199) */
    FxPoolBlock *head;           /* 0x04 */
} FxPool;
COD2_ASSERT_SIZE(FxPool, 0x08);
COD2_ASSERT_OFFSET(FxPool, head, 0x04);

/* One 0x8000-byte pool block. 409 elements of 0x50 bytes precede the header. */
struct FxPoolBlock {
    byte elements[0x7ff0];           /* 0x0000 element storage                */
    int freeCount;                   /* 0x7ff0 free elements remaining        */
    void *freeList;                  /* 0x7ff4 first free element             */
    FxPoolBlock *next;           /* 0x7ff8                                */
    FxPoolBlock *prev;           /* 0x7ffc                                */
};
COD2_ASSERT_SIZE(FxPoolBlock, 0x8000);
COD2_ASSERT_OFFSET(FxPoolBlock, freeCount, 0x7ff0);
COD2_ASSERT_OFFSET(FxPoolBlock, freeList, 0x7ff4);
COD2_ASSERT_OFFSET(FxPoolBlock, next, 0x7ff8);
COD2_ASSERT_OFFSET(FxPoolBlock, prev, 0x7ffc);

/* ------------------------------------------------------------------ */
/* Pointer list (0x0812c838..0x0812c92c). Used for shader, model and   */
/* FX-reference lists inside a primitive.                              */
/* ------------------------------------------------------------------ */
typedef struct fx_ptr_list_s {
    int *items;                      /* 0x00 */
    uint16 count;                    /* 0x04 */
    uint16 capacity;                 /* 0x06 */
} fx_ptr_list_s;
COD2_ASSERT_SIZE(fx_ptr_list_s, 0x08);
COD2_ASSERT_OFFSET(fx_ptr_list_s, count, 0x04);

/* ------------------------------------------------------------------ */
/* Curve reference: handle from 0x08123f3c plus a vec2 range.          */
/* ------------------------------------------------------------------ */
typedef struct FxCurveSrc {
    int table;                       /* 0x00 handle returned by 0x08123f3c   */
    vec2_t range;                    /* 0x04                                  */
} FxCurveSrc;
COD2_ASSERT_SIZE(FxCurveSrc, 0x0c);
COD2_ASSERT_OFFSET(FxCurveSrc, range, 0x04);

/* ------------------------------------------------------------------ */
/* Sampling channel (24 of them, 0x600 bytes total, 0x0812d9d6).       */
/* ------------------------------------------------------------------ */
typedef struct FxChannelInit {
    vec2_t pairA[3];                 /* 0x00 */
    vec2_t pairB[3];                 /* 0x18 */
    vec2_t extra;                    /* 0x30 */
    uint32 unk38;                    /* 0x38 */
    uint8 active;                    /* 0x3c tested by 0x0812daac             */
    uint8 unk3d[3];                  /* 0x3d */
} FxChannelInit;
COD2_ASSERT_SIZE(FxChannelInit, 0x40);
COD2_ASSERT_OFFSET(FxChannelInit, extra, 0x30);
COD2_ASSERT_OFFSET(FxChannelInit, active, 0x3c);

/* ------------------------------------------------------------------ */
/* Primitive template (0x2a4 bytes, 0x0812c220(0x2a4) in 0x0812c4f2).  */
/* ------------------------------------------------------------------ */
typedef struct fx_primitive_s {
    char name[0x20];                 /* 0x00 key "name" (0x080b7ab6, 0x20 bytes) */
    uint32 unk20[8];                 /* 0x20 */
    int type;                        /* 0x40 segment type 1..12                 */
    int index;                       /* 0x44 segment index within the effect    */
    vec2_t delay;                    /* 0x48 key "delay"                        */
    vec2_t count;                    /* 0x50 key "count"                        */
    vec2_t life;                     /* 0x58 key "life"                         */
    vec2_t spawnRange;               /* 0x60 key "spawnRange"; the key "cullrange"
                                      * writes the float at 0x64, which is
                                      * spawnRange[1] (overlap preserved).      */
    fx_ptr_list_s lists[5];          /* 0x68 shaders, models, impactfx, deathfx, emitfx
                                      * (lists[0].count at 0x6c is the shader count) */
    uint32 flags;                    /* 0x90 bit set by min/max/intensity/rotationDelta */
    uint32 flags2;                   /* 0x94 bit 0x100: spin axes (0x0812b5fe)  */
    uint32 unk98;                    /* 0x98 */
    uint8 nonUniformScale;           /* 0x9c key "nonUniformScale"              */
    uint8 useLength;                 /* 0x9d key "useLength"                    */
    uint8 unk9e[2];                  /* 0x9e */
    vec3_t boundsMin;                /* 0xa0 key "min"                          */
    vec3_t boundsMax;                /* 0xac key "max"                          */
    vec2_t origin[3];                /* 0xb8 key "origin" (per-axis min/max pairs) */
    vec2_t origin2[3];               /* 0xd0 key "origin2"                      */
    vec2_t radius;                   /* 0xe8 key "radius"                       */
    vec2_t height;                   /* 0xf0 key "height"                       */
    vec2_t wind;                     /* 0xf8 key "wind"                         */
    FxCurveSrc curves[24];           /* 0x100 sampled curves (stride 0x0c)      */
    vec2_t rotation;                 /* 0x220 key "rotation"                    */
    uint32 unk228[14];               /* 0x228                                   */
    vec2_t defaults260[4];           /* 0x260 set to (10,10),(1,1),(1,1),(1,1) by
                                      * 0x0812d424; role not identified          */
    vec2_t intensity;                /* 0x280 key "bounce"/"intensity" (range 0..1) */
    int seqStartFrameMode;           /* 0x288 key "sequenceStartFrameMode" (inferred) */
    int seqFixedFrameValue;          /* 0x28c key "sequenceFixedFrameValue" (inferred) */
    int seqPlayRateMode;             /* 0x290 key "sequencePlayRateMode" (inferred)   */
    float seqFixedFpsValue;          /* 0x294 key "sequenceFixedFpsValue" (inferred)  */
    int seqLoopMode;                 /* 0x298 key "sequenceLoopMode" (inferred)      */
    int seqLoopTimes;                /* 0x29c key "sequenceLoopTimes" (inferred)     */
    float spawnFrustumCullRadius;    /* 0x2a0 key "spawnFrustumCullRadius" (inferred) */

    bool Parse(struct ParseNode *node);   /* 0x081314a0, defined in chunk 057 */
} fx_primitive_s;
COD2_ASSERT_SIZE(fx_primitive_s, 0x2a4);
COD2_ASSERT_OFFSET(fx_primitive_s, type, 0x40);
COD2_ASSERT_OFFSET(fx_primitive_s, index, 0x44);
COD2_ASSERT_OFFSET(fx_primitive_s, life, 0x58);
COD2_ASSERT_OFFSET(fx_primitive_s, lists, 0x68);
COD2_ASSERT_OFFSET(fx_primitive_s, flags, 0x90);
COD2_ASSERT_OFFSET(fx_primitive_s, flags2, 0x94);
COD2_ASSERT_OFFSET(fx_primitive_s, nonUniformScale, 0x9c);
COD2_ASSERT_OFFSET(fx_primitive_s, boundsMin, 0xa0);
COD2_ASSERT_OFFSET(fx_primitive_s, boundsMax, 0xac);
COD2_ASSERT_OFFSET(fx_primitive_s, origin, 0xb8);
COD2_ASSERT_OFFSET(fx_primitive_s, origin2, 0xd0);
COD2_ASSERT_OFFSET(fx_primitive_s, radius, 0xe8);
COD2_ASSERT_OFFSET(fx_primitive_s, wind, 0xf8);
COD2_ASSERT_OFFSET(fx_primitive_s, curves, 0x100);
COD2_ASSERT_OFFSET(fx_primitive_s, rotation, 0x220);
COD2_ASSERT_OFFSET(fx_primitive_s, defaults260, 0x260);
COD2_ASSERT_OFFSET(fx_primitive_s, intensity, 0x280);
COD2_ASSERT_OFFSET(fx_primitive_s, seqFixedFpsValue, 0x294);
COD2_ASSERT_OFFSET(fx_primitive_s, spawnFrustumCullRadius, 0x2a0);

/* ------------------------------------------------------------------ */
/* Effect template (0x68 bytes, 0x0812c220(0x68) in 0x0812c4f2).       */
/* ------------------------------------------------------------------ */
typedef struct FxEffectTemplate {
    char *name;                      /* 0x00 effect name (strcpy copy)          */
    int primCount;                   /* 0x04 at most 0x18                       */
    fx_primitive_s *prims[24];       /* 0x08                                    */
} FxEffectTemplate;
COD2_ASSERT_SIZE(FxEffectTemplate, 0x68);
COD2_ASSERT_OFFSET(FxEffectTemplate, prims, 0x08);


/* ------------------------------------------------------------------ */
/* Playback record (0x50 bytes, pool 0x08a533e8).                      */
/* ------------------------------------------------------------------ */
typedef struct FxScheduledEffect FxScheduledEffect;
struct FxScheduledEffect {
    FxEffectTemplate *effect;           /* 0x00 template reference               */
    int primIndex;                   /* 0x04 index into effect->prims         */
    uint32 unk08;                    /* 0x08                                  */
    uint8 timing[8];                 /* 0x0c serialised as 8 raw bytes        */
    vec3_t origin;                   /* 0x14                                  */
    vec3_t axis[3];                  /* 0x20 0x20, 0x2c, 0x38                 */
    int unk44;                       /* 0x44                                  */
    uint32 unk48;                    /* 0x48                                  */
    FxScheduledEffect *next;             /* 0x4c free-standing list link          */
};
COD2_ASSERT_SIZE(FxScheduledEffect, 0x50);
COD2_ASSERT_OFFSET(FxScheduledEffect, origin, 0x14);
COD2_ASSERT_OFFSET(FxScheduledEffect, next, 0x4c);

typedef struct FxScheduler {
    int unk00;                       /* 0x00 */
    FxScheduledEffect *head;             /* 0x04 */
    int count;                       /* 0x08 */
} FxScheduler;
COD2_ASSERT_SIZE(FxScheduler, 0x0c);


/* ------------------------------------------------------------------ */
/* Cull state (8 bytes) and draw context (0x0812b8a8).                 */
/* ------------------------------------------------------------------ */
typedef struct fx_cull_state_s {
    uint32 unk[2];                   /* constructed by 0x08129bac, destroyed by 0x08129bd8 */
} fx_cull_state_s;
COD2_ASSERT_SIZE(fx_cull_state_s, 0x08);

/* Three-axis transform (52 bytes) filled by 0x080a934a. */
typedef struct fx_xform_s {
    vec3_t axis[3];                  /* 0x00 axis[0], axis[1], axis[2]          */
    float extra[4];                  /* 0x24 role not identified                */
} fx_xform_s;
COD2_ASSERT_SIZE(fx_xform_s, 0x34);

typedef struct fx_draw_ctx_s {
    struct fx_emitter_s *emitter;    /* 0x00 */
    fx_primitive_s *prim;            /* 0x04 */
    fx_cull_state_s cull;            /* 0x08 */
    fx_xform_s xform;                /* 0x10 */
} fx_draw_ctx_s;
COD2_ASSERT_SIZE(fx_draw_ctx_s, 0x44);
COD2_ASSERT_OFFSET(fx_draw_ctx_s, xform, 0x10);

/* ------------------------------------------------------------------ */
/* View input to the frustum builder (0x0812cef2, 0x0812cca2).         */
/* ------------------------------------------------------------------ */
typedef struct fx_view_s {
    uint32 unk00[4];                 /* 0x00                                    */
    float fovX;                      /* 0x10 degrees; 80 means no zoom scale    */
    float fovY;                      /* 0x14                                    */
    vec3_t origin;                   /* 0x18                                    */
    vec3_t forward;                  /* 0x24                                    */
    vec3_t right;                    /* 0x30                                    */
    vec3_t up;                       /* 0x3c                                    */
} fx_view_s;
COD2_ASSERT_SIZE(fx_view_s, 0x48);
COD2_ASSERT_OFFSET(fx_view_s, forward, 0x24);
COD2_ASSERT_OFFSET(fx_view_s, up, 0x3c);

typedef struct fx_plane_s {
    vec3_t normal;                   /* 0x00 */
    float dist;                      /* 0x0c */
} fx_plane_s;
COD2_ASSERT_SIZE(fx_plane_s, 0x10);

/* Scene frustum and random seed (global at 0x08188ac0). */
typedef struct FxSystem {
    uint32 unk00[5];                 /* 0x00                                    */
    vec3_t origin;                   /* 0x14                                    */
    fx_plane_s planes[6];            /* 0x20 frustum planes: near, left, right, top, bottom, far */
    int planeCount;                  /* 0x80 5, or 6 when the far plane is used */
    uint32 unk84[3];                 /* 0x84                                    */
    fx_plane_s planes2[6];           /* 0x90 second plane set                   */
    int planes2Count;                /* 0xf0                                    */
    uint32 rngSeed;                  /* 0xf4 multiplicative generator state     */
    float fovScale;                  /* 0xf8 tan(40 deg) / tan(fovX/2)          */
} FxSystem;
COD2_ASSERT_SIZE(FxSystem, 0xfc);
COD2_ASSERT_OFFSET(FxSystem, origin, 0x14);
COD2_ASSERT_OFFSET(FxSystem, planes, 0x20);
COD2_ASSERT_OFFSET(FxSystem, planeCount, 0x80);
COD2_ASSERT_OFFSET(FxSystem, planes2, 0x90);
COD2_ASSERT_OFFSET(FxSystem, rngSeed, 0xf4);
COD2_ASSERT_OFFSET(FxSystem, fovScale, 0xf8);


/* ------------------------------------------------------------------ */
/* Timing clock (0x0812ca90, 0x0812cad4, 0x0812cb96).                  */
/* ------------------------------------------------------------------ */
typedef struct fx_clock_s {
    int time;                        /* 0x00 last sample time                   */
    int accumTime;                   /* 0x04                                    */
    int prevAccumTime;               /* 0x08                                    */
    int deltaTime;                   /* 0x0c clamped to 0..200                  */
    int unk10;                       /* 0x10                                    */
    int curState[28];                /* 0x14 copied to savedState on advance    */
    int savedState[28];              /* 0x84                                    */
    int savedTime;                   /* 0xf4 copy of time                       */
} fx_clock_s;
COD2_ASSERT_SIZE(fx_clock_s, 0xf8);
COD2_ASSERT_OFFSET(fx_clock_s, savedState, 0x84);
COD2_ASSERT_OFFSET(fx_clock_s, savedTime, 0xf4);

/* Runtime flags (0x08a53c98); byte at +8 is the time-pause flag. */
typedef struct FxFlagVar {
    uint8 unk00[8];
    uint8 timePaused;                /* 0x08                                    */
} FxFlagVar;
COD2_ASSERT_OFFSET(FxFlagVar, timePaused, 0x08);


/* ------------------------------------------------------------------ */
/* Parse tree node for effect text (0x0812c4f2, 0x0812ca0a..0x0812ca2c). */
/* ------------------------------------------------------------------ */
typedef struct ParseNode ParseNode;
struct ParseNode {
    char *name;                      /* 0x00                                    */
    ParseNode *next;           /* 0x04 sibling                            */
    uint32 unk08[5];                 /* 0x08                                    */
    ParseNode *child;          /* 0x1c first child                        */
    uint32 unk20[10];                /* 0x20                                    */
};
COD2_ASSERT_SIZE(ParseNode, 0x48);
COD2_ASSERT_OFFSET(ParseNode, child, 0x1c);

/* ------------------------------------------------------------------ */
/* Serialisation stream helper (0x08129e6c, 0x08129e78, 0x08129e9e).   */
/* ------------------------------------------------------------------ */

/* Placeholder prototypes for out-of-chunk callees.                    */
/* Names follow the rule fn_08xxxxxx; signatures are the call shapes.  */

/* Vector and matrix helpers */

/* Curves, channels and placement */

/* Per-type primitive handlers (switch in 0x0812b670) */

/* Pool and allocation */

/* Cull state */

/* Serialisation stream */

/* File, parse and console */


/* ------------------------------------------------------------------ */
/* Manual call adapters (argument lists the original passes but the owners' prototypes omit). */
/* ------------------------------------------------------------------ */
/* The per-type handlers are called with (ctx, xform, a3, a4, a5); the owners declare shorter lists. */
typedef void (*FxPrimHandlerFn)(fx_draw_ctx_s *, fx_xform_s *, uint32, uint32, uint32);
#define FxSpawner_SpawnLine ((FxPrimHandlerFn)FxSpawner_SpawnLine)
#define FxSpawner_SpawnTail ((FxPrimHandlerFn)FxSpawner_SpawnTail)
#define FxSpawner_SpawnCylinder ((FxPrimHandlerFn)FxSpawner_SpawnCylinder)
#define FxSpawner_SpawnEmitter ((FxPrimHandlerFn)FxSpawner_SpawnEmitter)
#define FxSpawner_SpawnOrientedParticle ((FxPrimHandlerFn)FxSpawner_SpawnOrientedParticle)
#define FxSpawner_SpawnLight ((FxPrimHandlerFn)FxSpawner_SpawnLight)
#define FxSpawner_SpawnFlash ((FxPrimHandlerFn)FxSpawner_SpawnFlash)
#define FxSpawner_SpawnCloud ((FxPrimHandlerFn)FxSpawner_SpawnCloud)
#define FxSpawner_QueueDecalEvent ((FxPrimHandlerFn)FxSpawner_QueueDecalEvent)
#define FxSpawner_QueueFxRunnerEvent ((FxPrimHandlerFn)FxSpawner_QueueFxRunnerEvent)
#define FxSpawner_QueueCameraShakeEvent ((FxPrimHandlerFn)FxSpawner_QueueCameraShakeEvent)
/* Pool free: 0x0812c186 passes (elem, 0x50); the callee at 0x081242b2 reads only the element. */
typedef FxPoolBlock *(*FxPoolFreeElementFn)(void *, int);
#define FxPool_FreeElement ((FxPoolFreeElementFn)FxPool_FreeElement)
/* BEGIN call adapters */
template<class P> struct FxLoosePtr { P p; bool operator==(int v) const { return (int)p == v; } bool operator!=(int v) const { return (int)p != v; } FxLoosePtr(P q) : p(q) {} template<class T> operator T *() const { return (T *)p; } };
template<class P> inline FxLoosePtr<P> FxLoose(P p) { return FxLoosePtr<P>(p); }
#define ConfigTree_Destroy2(a0) ConfigTree_Destroy2(FxLoose(a0))
#define ConfigTree_Init2(a0) ConfigTree_Init2(FxLoose(a0))
#define ConfigTree_Parse(a0,a1,a2,a3) ConfigTree_Parse(FxLoose(a0),FxLoose(a1),a2,a3)
#define Curve_CreateRamp(a0,a1,a2,a3) Curve_CreateRamp(FxLoose(a0),a1,a2,a3)
#define FxPrim_ResolveBoltRef(a0,a1,a2) FxPrim_ResolveBoltRef(FxLoose(a0),FxLoose(a1),FxLoose(a2))
#define FxCurve_Build(a0,a1,a2,a3,a4) FxCurve_Build(FxLoose(a0),a1,a2,a3,FxLoose(a4))
#define FxArchive_IsReading(a0) FxArchive_IsReading(FxLoose(a0))
#define FxRange_Lerp(a0,a1) FxRange_Lerp(FxLoose(a0),a1)
#define FxRange_Set(a0,a1,a2) FxRange_Set(FxLoose(a0),a1,a2)
#define FxRef_Construct(a0) FxRef_Construct(FxLoose(a0))
#define FxRef_Destruct(a0) FxRef_Destruct(FxLoose(a0))
#define FxSpawner_Spawn(a0,a1,a2,a3,a4) FxSpawner_Spawn(FxLoose(a0),FxLoose(a1),FxLoose(a2),a3,a4)
#define FxStream_Int(a0,a1) FxStream_Int(FxLoose(a0),FxLoose(a1))
#define FxStream_ReadInt(a0) FxStream_ReadInt(FxLoose(a0))
#define FxStream_Vec3(a0,a1) FxStream_Vec3(FxLoose(a0),FxLoose(a1))
#define FxStream_WriteInt(a0,a1) FxStream_WriteInt(FxLoose(a0),a1)
#define Mat3_Copy(a0,a1) Mat3_Copy(FxLoose(a0),FxLoose(a1))
#define MsgStream_TransferBytes(a0,a1,a2) MsgStream_TransferBytes(FxLoose(a0),FxLoose(a1),a2)
#define MsgStream_TransferStringRef(a0,a1) MsgStream_TransferStringRef(FxLoose(a0),FxLoose(a1))
/* END call adapters */

#endif
