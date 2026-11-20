/*
 * chunk 057 types: FX fx_primitive_s key parsing (effects script), token parser,
 * string-page allocator and small float helpers. Address range 0x812e89e..0x8132026.
 */
#ifndef COD2_CHUNK_057_TYPES_H
#define COD2_CHUNK_057_TYPES_H

#include "cod2_types.h"
#include <strings.h>

struct fx_primitive_s;

/* 8-byte {lo, hi} float range (FUN_081315a2 stores both). */
struct FxRange {
    float lo;
    float hi;
};
COD2_ASSERT_SIZE(FxRange, 8);

/* Growable pointer array (data, count, capacity); grown by FX_PtrListPush. */
struct fx_ptr_list_s {
    void **data;
    uint16 count;
    uint16 capacity;
};
COD2_ASSERT_SIZE(fx_ptr_list_s, 8);

/* One channel of the 0x600-byte scratch block (24 channels) used while parsing. */
struct FxChannelInit {
    FxRange start[3];   /* 0x00 */
    FxRange end[3];     /* 0x18 */
    FxRange parm;       /* 0x30 */
    uint32 flags;       /* 0x38 */
    byte used;          /* 0x3c */
    byte pad3d[3];
};
COD2_ASSERT_SIZE(FxChannelInit, 0x40);
COD2_ASSERT_OFFSET(FxChannelInit, parm, 0x30);
COD2_ASSERT_OFFSET(FxChannelInit, flags, 0x38);
COD2_ASSERT_OFFSET(FxChannelInit, used, 0x3c);

#define FX_CHANNEL_COUNT 24

/* Per-channel curve slot inside fx_primitive_s. */
struct FxCurveSrc {
    void *curve;        /* result of Curve_Create */
    FxRange scale;
};
COD2_ASSERT_SIZE(FxCurveSrc, 12);

/* Effect-script parse tree node (key string, siblings, first child at +0x10). */
struct ParseNode {
    const char *name;       /* 0x00 */
    ParseNode *next;        /* 0x04 */
    int field8;             /* 0x08 */
    int fieldC;             /* 0x0c */
    ParseNode *firstChild;  /* 0x10 */
};
COD2_ASSERT_OFFSET(ParseNode, next, 4);
COD2_ASSERT_OFFSET(ParseNode, firstChild, 0x10);

/* Linear string page used while serialising parse nodes. */
struct Fx_StringPage {
    char *buffer;           /* 0x00 */
    Fx_StringPage *next;    /* 0x04 */
    int size;               /* 0x08 */
    int used;               /* 0x0c */
};
COD2_ASSERT_SIZE(Fx_StringPage, 0x10);

/* Name/flag table entry (name, flags, spawnFlags), 12 bytes. */
struct FxFlagName {
    const char *name;
    uint32 flags;
    uint32 spawnFlags;
};
COD2_ASSERT_SIZE(FxFlagName, 12);

/* Effect primitive template. Only offsets observed in this chunk are named. */
struct fx_primitive_s {
    char name[0x20];                    /* 0x00 */
    char shaderImpact[0x20];            /* 0x20 */
    byte pad40[8];
    FxRange delay;                      /* 0x48 */
    FxRange count;                      /* 0x50 */
    FxRange life;                       /* 0x58 (hi read as float at 0x5c) */
    int field60;
    float cullRange;                    /* 0x64 */
    fx_ptr_list_s materials;              /* 0x68 */
    fx_ptr_list_s impactFx;               /* 0x70 */
    fx_ptr_list_s deathFx;                /* 0x78 */
    fx_ptr_list_s emitFx;                 /* 0x80 */
    fx_ptr_list_s playFx;                 /* 0x88 */
    uint32 flags;                       /* 0x90 */
    uint32 spawnFlags;                  /* 0x94 */
    int field98;
    byte nonUniformScale;               /* 0x9c */
    byte useLength;                     /* 0x9d */
    byte pad9e[2];
    float mins[3];                      /* 0xa0 */
    float maxs[3];                      /* 0xac */
    byte padB8[0x48];
    FxCurveSrc channels[FX_CHANNEL_COUNT]; /* 0x100 */
    byte pad220[8];
    FxRange angles[3];                  /* 0x228 */
    FxRange angleDelta[3];              /* 0x240 */
    FxRange gravity;                    /* 0x258 */
    FxRange density;                    /* 0x260 */
    FxRange variance;                   /* 0x268 */
    byte pad270[0x18];
    int sequenceStartFrameMode;         /* 0x288 */
    int sequenceFixedFrameValue;        /* 0x28c */
    int sequencePlayRateMode;           /* 0x290 */
    float sequenceFixedFpsValue;        /* 0x294 */
    int sequenceLoopMode;               /* 0x298 */
    int sequenceLoopTimes;              /* 0x29c */
    float spawnFrustumCullRadius;       /* 0x2a0 */

    bool ParseAngles(const char *str);
    bool ParseAngleDelta(const char *str);
    bool ParseVelocity(const char *str);
    bool ParseAcceleration(const char *str);
    bool LookupFlag(const char *flagName, const FxFlagName *table, int tableCount);
    bool ParseFlagList(const char *str, const FxFlagName *table, int tableCount);
    bool ParseFlags(const char *str);
    bool ParseSpawnFlags(const char *str);
    bool ParseGravity(const char *str);
    bool ParseDensity(const char *str);
    bool ParseVariance(const char *str);
    bool ParseSequenceStartFrameMode(const char *str);
    bool ParseSequenceFixedFrameValue(const char *str);
    bool ParseSequencePlayRateMode(const char *str);
    bool ParseSequenceFixedFpsValue(const char *str);
    bool ParseSequenceLoopMode(const char *str);
    bool ParseSequenceLoopTimes(const char *str);
    bool ParseSpawnFrustumCullRadius(const char *str);
    bool ParseChannelStartVec3(FxChannelInit *init, const char *str, int channel);
    bool ParseChannelEndVec3(FxChannelInit *init, const char *str, int channel);
    bool ParseChannelStart(FxChannelInit *init, const char *str, int channel);
    bool ParseChannelEnd(FxChannelInit *init, const char *str, int channel);
    bool ParseChannelParm(FxChannelInit *init, const char *str, int channel);
    bool ParseChannelFlags(FxChannelInit *init, const char *str, int channel);
    bool ParseMaterials(ParseNode *node);
    bool ParseModels(ParseNode *node);
    bool ParseImpactFxStrings(ParseNode *node);
    bool ParseDeathFxStrings(ParseNode *node);
    bool ParseEmitterFxStrings(ParseNode *node);
    bool ParsePlayFxStrings(ParseNode *node);
    void ParseCurveVec3(ParseNode *node, int channel);
    void ParseCurveFloat(ParseNode *node, int channel);
    void ParseChannelScale(const char *str, int channel);
    bool ParseChannel(FxChannelInit *init, ParseNode *node, int channel);
    bool ParseShaderImpact(const char *str);
    bool ParseKeys(FxChannelInit *init, ParseNode *node);
    bool Parse(ParseNode *node);
};
COD2_ASSERT_OFFSET(fx_primitive_s, shaderImpact, 0x20);
COD2_ASSERT_OFFSET(fx_primitive_s, delay, 0x48);
COD2_ASSERT_OFFSET(fx_primitive_s, life, 0x58);
COD2_ASSERT_OFFSET(fx_primitive_s, cullRange, 0x64);
COD2_ASSERT_OFFSET(fx_primitive_s, materials, 0x68);
COD2_ASSERT_OFFSET(fx_primitive_s, playFx, 0x88);
COD2_ASSERT_OFFSET(fx_primitive_s, flags, 0x90);
COD2_ASSERT_OFFSET(fx_primitive_s, spawnFlags, 0x94);
COD2_ASSERT_OFFSET(fx_primitive_s, nonUniformScale, 0x9c);
COD2_ASSERT_OFFSET(fx_primitive_s, mins, 0xa0);
COD2_ASSERT_OFFSET(fx_primitive_s, maxs, 0xac);
COD2_ASSERT_OFFSET(fx_primitive_s, channels, 0x100);
COD2_ASSERT_OFFSET(fx_primitive_s, angles, 0x228);
COD2_ASSERT_OFFSET(fx_primitive_s, angleDelta, 0x240);
COD2_ASSERT_OFFSET(fx_primitive_s, gravity, 0x258);
COD2_ASSERT_OFFSET(fx_primitive_s, density, 0x260);
COD2_ASSERT_OFFSET(fx_primitive_s, variance, 0x268);
COD2_ASSERT_OFFSET(fx_primitive_s, sequenceStartFrameMode, 0x288);
COD2_ASSERT_OFFSET(fx_primitive_s, spawnFrustumCullRadius, 0x2a0);

/* Globals */

/* Merged names from other lanes (symbols.json) */

/* Out-of-chunk callees (placeholders) */

/* Functions defined in chunk 057 that are used across the chunk */
/* BEGIN call adapters */
template<class P> struct FxLoosePtr { P p; bool operator==(int v) const { return (int)p == v; } bool operator!=(int v) const { return (int)p != v; } FxLoosePtr(P q) : p(q) {} template<class T> operator T *() const { return (T *)p; } };
template<class P> inline FxLoosePtr<P> FxLoose(P p) { return FxLoosePtr<P>(p); }
#define ConfigAttr_AddValue(a0,a1,a2) ConfigAttr_AddValue(FxLoose(a0),FxLoose(a1),FxLoose(a2))
#define ConfigAttr_GetFirstValue(a0) ConfigAttr_GetFirstValue(FxLoose(a0))
#define ConfigAttr_HasMultipleValues(a0) ConfigAttr_HasMultipleValues(FxLoose(a0))
#define ConfigLink_GetNext(a0) FxLoose(ConfigLink_GetNext(FxLoose(a0)))
#define ConfigLink_SetNextAlt(a0,a1) ConfigLink_SetNextAlt(FxLoose(a0),FxLoose(a1))
/* END call adapters */

#endif
