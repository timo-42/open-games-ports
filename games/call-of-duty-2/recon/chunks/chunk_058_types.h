/*
 * chunk 058 types: config-tree (text token tree) container + FX effect list manager.
 * Address range 0x08132064..0x08135d72 of cod2_lnxded 1.3.
 */
#ifndef COD2_CHUNK_058_TYPES_H
#define COD2_CHUNK_058_TYPES_H

#include "cod2_types.h"

/* ------------------------------------------------------------------ */
/* Config tree ("Top Level" block / attribute / value lists)           */
/* ------------------------------------------------------------------ */

struct Fx_StringPage;      /* growable string arena / output buffer (ctor Fx_StringPage_Init2, 0x10 bytes) */
struct TokenStream;  /* text tokenizer read by Fx_ParseToken */

/* Generic 4-word list link shared by attributes and blocks (0x10 bytes). */
struct ConfigLink {
    char *name;               /* 0x00 */
    ConfigLink *next;         /* 0x04 insertion-order next */
    ConfigLink *sortNext;     /* 0x08 case-insensitive sorted next */
    ConfigLink *sortPrev;     /* 0x0c case-insensitive sorted prev */
};
COD2_ASSERT_SIZE(ConfigLink, 0x10);

/* Value list node: head->tail (+8) caches the last node of the list. */
struct ConfigValue {
    char *text;               /* 0x00 */
    ConfigValue *next;        /* 0x04 */
    ConfigValue *tail;        /* 0x08 only maintained in the list head */
    void *unused;             /* 0x0c */
};
COD2_ASSERT_SIZE(ConfigValue, 0x10);

/* key with a list of values (0x14 bytes). */
struct ConfigAttr {
    char *name;               /* 0x00 */
    ConfigAttr *next;         /* 0x04 */
    ConfigAttr *sortNext;     /* 0x08 */
    ConfigAttr *sortPrev;     /* 0x0c */
    ConfigValue *firstValue;  /* 0x10 */
};
COD2_ASSERT_SIZE(ConfigAttr, 0x14);
COD2_ASSERT_OFFSET(ConfigAttr, firstValue, 0x10);

/* named block holding attributes and child blocks (0x30 bytes). */
struct ParseNode {
    char *name;               /* 0x00 */
    ParseNode *next;        /* 0x04 */
    ParseNode *sortNext;    /* 0x08 */
    ParseNode *sortPrev;    /* 0x0c */
    ConfigAttr *firstAttr;    /* 0x10 */
    ConfigAttr *sortedAttrs;  /* 0x14 head of sorted attr list */
    ConfigAttr *lastAttr;     /* 0x18 (also used as scratch "next" during destruction) */
    ParseNode *firstChild;  /* 0x1c */
    ParseNode *sortedChildren; /* 0x20 */
    ParseNode *lastChild;   /* 0x24 (also scratch during destruction) */
    int expectUnclosed;       /* 0x28 nonzero => parse fails if the stream ends before '}' */
    byte flag;                /* 0x2c copied to children by the parser */
    byte pad2d[3];
};
COD2_ASSERT_SIZE(ParseNode, 0x30);
COD2_ASSERT_OFFSET(ParseNode, firstAttr, 0x10);
COD2_ASSERT_OFFSET(ParseNode, sortedAttrs, 0x14);
COD2_ASSERT_OFFSET(ParseNode, lastAttr, 0x18);
COD2_ASSERT_OFFSET(ParseNode, firstChild, 0x1c);
COD2_ASSERT_OFFSET(ParseNode, sortedChildren, 0x20);
COD2_ASSERT_OFFSET(ParseNode, lastChild, 0x24);
COD2_ASSERT_OFFSET(ParseNode, expectUnclosed, 0x28);
COD2_ASSERT_OFFSET(ParseNode, flag, 0x2c);

/* root block named "Top Level" plus its string pool (0x38 bytes). */
struct ConfigTree {
    ParseNode block;        /* 0x00 */
    Fx_StringPage *pool;            /* 0x30 */
    byte flag;                /* 0x34 */
    byte pad35[3];
};
COD2_ASSERT_SIZE(ConfigTree, 0x38);
COD2_ASSERT_OFFSET(ConfigTree, pool, 0x30);
COD2_ASSERT_OFFSET(ConfigTree, flag, 0x34);

/* ------------------------------------------------------------------ */
/* FX system                                                           */
/* ------------------------------------------------------------------ */

struct Effect;
struct FxMsg {            /* 32-byte message reader/writer (ctor MsgStream_ResetAlt) */
    byte data[32];
};
COD2_ASSERT_SIZE(FxMsg, 32);
struct fx_primitive_s;

struct FxRange {         /* randomised [lo,hi]; FxRange_Random samples it */
    float lo;
    float hi;
};

struct FxHandle {
    int value;
};

/* Entry of the origin-reference table at *g_fxOriginRefs (count g_fxOriginRefCount). */
struct FxOriginRef {
    float origin[3];          /* 0x00 */
    int refCount;             /* 0x0c */
};
COD2_ASSERT_SIZE(FxOriginRef, 0x10);

/* Occluder/attenuation entry at g_fxBlockers (count g_fxBlockerCount). */
struct FxBlocker {
    float origin[3];          /* 0x00 */
    float radius;             /* 0x0c compared with the distance of the ray point */
    float attenuation;        /* 0x10 multiplied into the result when hit */
};
COD2_ASSERT_SIZE(FxBlocker, 0x14);

/* Shared FX system state (*g_fxSystem). */
struct FxSystem {
    int pad00;
    int time;                 /* 0x04 */
    int pad08[3];
    float viewOrigin[3];      /* 0x14 */
};
COD2_ASSERT_OFFSET(FxSystem, time, 4);
COD2_ASSERT_OFFSET(FxSystem, viewOrigin, 0x14);

/* Scheduler object (0xc bytes); only the pending count at +8 is read here. */
struct FxScheduler {
    int pad00;
    int pad04;
    int pendingCount;         /* 0x08 */
};
COD2_ASSERT_OFFSET(FxScheduler, pendingCount, 8);

struct FxEffectVtbl {
    void (*slot00)(Effect *);                 /* 0x00 complete dtor */
    void (*Destroy)(Effect *);                /* 0x04 deleting dtor */
    void (*Release)(Effect *);                /* 0x08 */
    char (*Update)(Effect *);                 /* 0x0c returns 0 when the effect is finished */
    char (*IsHidden)(Effect *);               /* 0x10 */
    void (*Draw)(Effect *);                   /* 0x14 */
    void (*slot18)(Effect *);                 /* 0x18 */
    void (*UpdateFlagged)(Effect *);          /* 0x1c */
    void (*Init)(Effect *, fx_primitive_s *);      /* 0x20 */
    char (*GetType)(Effect *);                /* 0x24 serialisation type id */
    void (*Serialize)(Effect *, FxMsg *);     /* 0x28 */
    void (*Attach)(Effect *, void *);         /* 0x2c */
};

/* Common prefix of all effect classes (sizes 0xfc..0x29c). */
struct Effect {
    FxEffectVtbl *vtbl;       /* 0x00 */
    byte pad04[0x3c];
    int sortKey2;             /* 0x40 */
    byte pad44[0x38];
    float origin[3];          /* 0x7c */
    byte pad88[0x20];
    unsigned int flags;       /* 0xa8 bit 0x1000: per-frame flagged */
    int originRefIndex;       /* 0xac */
    int sortKey;              /* 0xb0 */
    byte padb4[4];
    int startTime;            /* 0xb8 */
    int endTime;              /* 0xbc */
    FxHandle handle;          /* 0xc0 */

    /* Members defined in chunk 054 (0x08124894, 0x081248d4). */
    void SetTimes(int start, int end);
    void SetBinding(struct FxRef *ref);
};
COD2_ASSERT_OFFSET(Effect, sortKey2, 0x40);
COD2_ASSERT_OFFSET(Effect, origin, 0x7c);
COD2_ASSERT_OFFSET(Effect, flags, 0xa8);
COD2_ASSERT_OFFSET(Effect, originRefIndex, 0xac);
COD2_ASSERT_OFFSET(Effect, sortKey, 0xb0);
COD2_ASSERT_OFFSET(Effect, startTime, 0xb8);
COD2_ASSERT_OFFSET(Effect, endTime, 0xbc);
COD2_ASSERT_OFFSET(Effect, handle, 0xc0);

/* Particle subclass members (chunk 054): called through the Effect prefix. */
struct Particle : Effect {
    void SetBasis(const float *axes3);                 /* 0x08124fd4 */
    void SetVelocityRand(float x, float y, float z);   /* 0x081252ce */
    void SetAccelRand(float x, float y, float z);      /* 0x081252fc */
    void EvalDisplacement(int ms, float *out);         /* 0x08126178 */
};

/* (effect, camera distance) pairs sorted for drawing (8 bytes). */
struct FxDrawEntry {
    Effect *effect;
    float dist;
};
COD2_ASSERT_SIZE(FxDrawEntry, 8);

/* Effect element definition, fields recovered from use only. */
struct fx_primitive_s {
    byte pad00[0x40];
    int id40;                 /* 0x40 */
    int id44;                 /* 0x44 */
    byte pad48[0x10];
    FxRange lifetime;    /* 0x58 */
    byte pad60[8];
    FxHandle seqHandle;       /* 0x68 */
    byte pad6c[4];
    FxHandle handle70;        /* 0x70 */
    byte pad74[4];
    FxHandle handle78;        /* 0x78 */
    byte pad7c[0x14];
    unsigned int flags;       /* 0x90 */
    unsigned int flags2;      /* 0x94 */
    int value98;              /* 0x98 */
    byte value9c;             /* 0x9c */
    byte pad9d[3];
    int valueA0[3];           /* 0xa0 */
    int valueAC[3];           /* 0xac */
    FxRange offX;        /* 0xb8 */
    FxRange offY;        /* 0xc0 */
    FxRange offZ;        /* 0xc8 */
    FxRange traceX;      /* 0xd0 */
    FxRange traceY;      /* 0xd8 */
    FxRange traceZ;      /* 0xe0 */
    FxRange radius;      /* 0xe8 */
    FxRange spread;      /* 0xf0 */
    FxRange rangeF8;     /* 0xf8 */
    byte padx100[0x120];
    FxRange range220;    /* 0x220 */
    byte pad228[0x30];
    FxRange range258;    /* 0x258 (600) */
    byte pad260[0x20];
    FxRange range280;    /* 0x280 */
    int frameMode;            /* 0x288 */
    int frameCount;           /* 0x28c */
    int rateMode;             /* 0x290 */
    float rateMs;             /* 0x294 */
    int value298;             /* 0x298 */
    int value29c;             /* 0x29c */
};
COD2_ASSERT_OFFSET(fx_primitive_s, lifetime, 0x58);
COD2_ASSERT_OFFSET(fx_primitive_s, seqHandle, 0x68);
COD2_ASSERT_OFFSET(fx_primitive_s, handle78, 0x78);
COD2_ASSERT_OFFSET(fx_primitive_s, flags, 0x90);
COD2_ASSERT_OFFSET(fx_primitive_s, flags2, 0x94);
COD2_ASSERT_OFFSET(fx_primitive_s, valueA0, 0xa0);
COD2_ASSERT_OFFSET(fx_primitive_s, valueAC, 0xac);
COD2_ASSERT_OFFSET(fx_primitive_s, offX, 0xb8);
COD2_ASSERT_OFFSET(fx_primitive_s, offZ, 0xc8);
COD2_ASSERT_OFFSET(fx_primitive_s, traceX, 0xd0);
COD2_ASSERT_OFFSET(fx_primitive_s, traceZ, 0xe0);
COD2_ASSERT_OFFSET(fx_primitive_s, radius, 0xe8);
COD2_ASSERT_OFFSET(fx_primitive_s, spread, 0xf0);
COD2_ASSERT_OFFSET(fx_primitive_s, rangeF8, 0xf8);
COD2_ASSERT_OFFSET(fx_primitive_s, range220, 0x220);
COD2_ASSERT_OFFSET(fx_primitive_s, range258, 0x258);
COD2_ASSERT_OFFSET(fx_primitive_s, range280, 0x280);
COD2_ASSERT_OFFSET(fx_primitive_s, frameMode, 0x288);
COD2_ASSERT_OFFSET(fx_primitive_s, value29c, 0x29c);

/* Spawn request: effect id, definition, owner handle. */
struct FxSpawner {
    int id;                   /* 0x00 */
    fx_primitive_s *def;           /* 0x04 */
    FxHandle handle;          /* 0x08 */
};
COD2_ASSERT_OFFSET(FxSpawner, def, 4);
COD2_ASSERT_OFFSET(FxSpawner, handle, 8);

/* Table of effect lookup (count at +4, entries at +8). */
struct FxEffectTemplate {
    int pad00;
    int count;                /* 0x04 */
    void *entries[1];         /* 0x08 */
};

/* Dvar-like object; the bool/float value sits at +8. */
struct FxCvar {
    byte pad[8];
    union { byte enabled; float value; };
};
COD2_ASSERT_OFFSET(FxCvar, enabled, 8);

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */
extern int g_08a5c9a0;                  /* list A count */
extern int g_08a5c9a4;                  /* list B count */
extern int g_08a5c9a8;                  /* total active effects */
extern int g_08a5c9b4;                  /* list A count at start of update */
extern int g_08a5c9b8;                  /* list B count at start of update */
extern int g_08a5c9bc;                  /* list A first uncollected index */
extern int g_08a5c9c0;                  /* list B first uncollected index */
extern int g_08a5cadc[];                /* per-context init flags */
extern int g_08a5cae0;                  /* draw list B count */
extern int g_08a5cae4;                  /* draw list A count */
extern FxCvar *g_08a53c84;
extern FxCvar *g_08a53c88;
extern FxCvar *g_08a53c8c;
extern FxCvar *g_08a53c90;
extern int *g_08a53cac;                 /* rank table used by effect sort */
extern FxDrawEntry g_08a674c0[];        /* draw list B (1800 entries) */
extern FxDrawEntry g_08a6ad00[];        /* draw list A (1800 entries) */

/* ------------------------------------------------------------------ */
/* Out-of-chunk callees (placeholders, rewritten by the merge queue)   */
/* ------------------------------------------------------------------ */

/* FX_InitSampleRecord (0x0812d16e, chunk 056): shared prototype is authoritative. */
void FX_InitSampleRecord(struct FxSystem *sc, float *rec, const float *start, const float *mins, const float *maxs, const float *end, int a7, uint32 mask);

/* Chunk-local functions used across the file. */
/* Calls whose callee ignores (or lacks) the argument list the original passes. */
#define Fx_AllocSmall ((void *(*)(int))Fx_AllocSmall)
#define FX_GetSpriteFrameCount_Stub ((int (*)(FxSystem *, int))FX_GetSpriteFrameCount_Stub)
#define FX_IsEffectFlagged_Stub ((int (*)(FxSystem *, int))FX_IsEffectFlagged_Stub)
/* BEGIN call adapters */
template<class P> struct FxLoosePtr { P p; bool operator==(int v) const { return (int)p == v; } bool operator!=(int v) const { return (int)p != v; } FxLoosePtr(P q) : p(q) {} template<class T> operator T *() const { return (T *)p; } };
template<class P> inline FxLoosePtr<P> FxLoose(P p) { return FxLoosePtr<P>(p); }
#define Cloud_Construct(a0) Cloud_Construct(FxLoose(a0))
#define ConfigBlock_GetName(a0) ConfigBlock_GetName(FxLoose(a0))
#define ConfigNode_GetNextAlias(a0) FxLoose(ConfigNode_GetNextAlias(FxLoose(a0)))
#define ConfigNode_GetNextAlias2(a0) FxLoose(ConfigNode_GetNextAlias2(FxLoose(a0)))
#define Cylinder_ConstructBase(a0) Cylinder_ConstructBase(FxLoose(a0))
#define Emitter_ConstructBase(a0) Emitter_ConstructBase(FxLoose(a0))
#define FX_ClockReset(a0) FX_ClockReset(FxLoose(a0))
#define FX_SerializePlaybackList(a0,a1) FX_SerializePlaybackList(FxLoose(a0),FxLoose(a1))
#define FX_SerializeSlot(a0,a1) FX_SerializeSlot(FxLoose(a0),FxLoose(a1))
#define FxObj_SetVec14OrClear(a0,a1) FxObj_SetVec14OrClear(FxLoose(a0),FxLoose(a1))
#define FxObj_SetVec20OrClear(a0,a1) FxObj_SetVec20OrClear(FxLoose(a0),FxLoose(a1))
#define FxPickList_Pick(a0) FxPickList_Pick(FxLoose(a0))
#define FxPickList_PickOut(a0,a1) FxPickList_PickOut(FxLoose(a0),FxLoose(a1))
#define FxRef_Get(a0) FxRef_Get(FxLoose(a0))
#define FxRef_IsValid(a0) FxRef_IsValid(FxLoose(a0))
#define FxStream_ReadInt(a0) FxStream_ReadInt(FxLoose(a0))
#define FxStream_WriteInt(a0,a1) FxStream_WriteInt(FxLoose(a0),a1)
#define Fx_ParseToken(a0,a1,a2) Fx_ParseToken(FxLoose(a0),a1,a2)
#define Handle_GetTargetField8(a0) Handle_GetTargetField8(FxLoose(a0))
#define Light_ConstructBase(a0) Light_ConstructBase(FxLoose(a0))
#define Line_ConstructBase(a0) Line_ConstructBase(FxLoose(a0))
#define MsgStream_InitRead(a0,a1) MsgStream_InitRead(FxLoose(a0),FxLoose(a1))
#define MsgStream_ReadDecodedByte(a0) MsgStream_ReadDecodedByte(FxLoose(a0))
#define MsgStream_ResetAlt(a0) MsgStream_ResetAlt(FxLoose(a0))
#define MsgStream_WriteDecodedByte(a0,a1) MsgStream_WriteDecodedByte(FxLoose(a0),a1)
#define Orient_PointWorldToLocal(a0,a1,a2) Orient_PointWorldToLocal(FxLoose(a0),FxLoose(a1),FxLoose(a2))
#define OrientedParticle_ConstructBase(a0) OrientedParticle_ConstructBase(FxLoose(a0))
#define ParseNode_ConstructWithText2(a0,a1,a2) ParseNode_ConstructWithText2(FxLoose(a0),FxLoose(a1),FxLoose(a2))
#define ParseNode_Init(a0,a1) ParseNode_Init(FxLoose(a0),FxLoose(a1))
#define ParseNode_Init2(a0,a1) ParseNode_Init2(FxLoose(a0),FxLoose(a1))
#define Particle_ConstructBase(a0) Particle_ConstructBase(FxLoose(a0))
#define Tail_ConstructBase(a0) Tail_ConstructBase(FxLoose(a0))
/* END call adapters */

#endif
