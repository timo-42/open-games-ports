#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_057_types.h"

/* Globals referenced by this chunk (defined elsewhere in the binary). */

// 0x0812e89e  size=157  FUN_0812e89e  status=translated  naming=inferred
// Evidence: dispatched from the effect key parser (0x08130248) for keys "angle"/"angles"; fills the three angle ranges at +0x228.
bool fx_primitive_s::ParseAngles(const char *str)
{
    float lo[3];
    float hi[3];
    bool ok = FX_ParseVec3Pair((uint32)this, str, lo, hi);
    if (ok) {
        FxRange_Set(&angles[0], lo[0], hi[0]);
        FxRange_Set(&angles[1], lo[1], hi[1]);
        FxRange_Set(&angles[2], lo[2], hi[2]);
    }
    return ok;
}

// 0x0812e93c  size=157  FUN_0812e93c  status=translated  naming=inferred
// Evidence: dispatched for key "angleDelta" in 0x08130248; fills the ranges at +0x240.
bool fx_primitive_s::ParseAngleDelta(const char *str)
{
    float lo[3];
    float hi[3];
    bool ok = FX_ParseVec3Pair((uint32)this, str, lo, hi);
    if (ok) {
        FxRange_Set(&angleDelta[0], lo[0], hi[0]);
        FxRange_Set(&angleDelta[1], lo[1], hi[1]);
        FxRange_Set(&angleDelta[2], lo[2], hi[2]);
    }
    return ok;
}

// 0x0812e9da  size=627  FUN_0812e9da  status=translated  naming=inferred
// Evidence: dispatched for keys "velocity"/"vel" in 0x08130248; sets flag 0x80000 (useRandomVelocity in table 0x0815fae0) when lo != hi.
bool fx_primitive_s::ParseVelocity(const char *str)
{
    float lo[3];
    float hi[3];
    if (!FX_ParseVec3Pair((uint32)this, str, lo, hi))
        return false;

    if (hi[0] < lo[0])
        Float_Swap(&lo[0], &hi[0]);
    if (hi[1] < lo[1])
        Float_Swap(&lo[1], &hi[1]);
    if (hi[2] < lo[2])
        Float_Swap(&lo[2], &hi[2]);

    float m0 = FX_MaxAbsFloor((uint32)this, lo[0], hi[0]);
    float m1 = FX_MaxAbsFloor((uint32)this, lo[1], hi[1]);
    float m2 = FX_MaxAbsFloor((uint32)this, lo[2], hi[2]);
    m0 = m0 + m0;
    m1 = m1 + m1;
    m2 = m2 + m2;
    float m = Float_Max_08131618(m0, m1);
    m = Float_Max_08131618(m, m2);

    FX_BuildCurveRatio(this, lo[0], m, 0xc, m);
    FX_BuildCurveRatio(this, lo[1], m, 0xd, m);
    FX_BuildCurveRatio(this, lo[2], m, 0xe, m);
    if (!Vec3_Equal3(lo, hi)) {
        flags |= 0x80000;
        FX_BuildCurveRatio(this, hi[0], m, 0xf, 1.0f);
        FX_BuildCurveRatio(this, hi[1], m, 0x10, 1.0f);
        FX_BuildCurveRatio(this, hi[2], m, 0x11, 1.0f);
    }
    return true;
}

// 0x0812ec4e  size=141  FUN_0812ec4e  status=translated  naming=inferred
// Evidence: case-insensitive lookup in an array of {name, flags, spawnFlags} (12-byte entries) that ORs the matched entry into +0x90/+0x94.
bool fx_primitive_s::LookupFlag(const char *flagName, const FxFlagName *table, int tableCount)
{
    for (int i = 0; i < tableCount; i++) {
        const FxFlagName *entry = &table[i];
        if (strcasecmp(entry->name, flagName) == 0) {
            flags = entry->flags | flags;
            spawnFlags = entry->spawnFlags | spawnFlags;
            return true;
        }
    }
    return false;
}

// 0x0812ecdc  size=216  FUN_0812ecdc  status=translated  naming=inferred
// Evidence: splits a whitespace separated word list with sscanf("%s") and looks each word up with LookupFlag; fails on empty input or unknown word.
bool fx_primitive_s::ParseFlagList(const char *str, const FxFlagName *table, int tableCount)
{
    size_t len = strlen(str);
    if (len == 0)
        return false;

    char *word = (char *)Hunk_AllocateTempMemory((int)len);
    const char *cursor = str;
    int consumed = 0;
    bool result = true;
    while (consumed < (int)len) {
        int n = sscanf(cursor, "%s", word);
        if (n != 1) {
            result = false;
            break;
        }
        size_t wordLen = strlen(word);
        if (!LookupFlag(word, table, tableCount)) {
            result = false;
            break;
        }
        consumed = (int)wordLen + consumed + 1;
        cursor = str + consumed;
    }
    Hunk_FreeTempMemory((int *)word);
    return result;
}

// 0x0812edb4  size=45  FUN_0812edb4  status=translated  naming=inferred
// Evidence: dispatched for keys "flags"/"flag"; uses the 0x18-entry name table at 0x0815fae0 (depthHack, setShaderTime, ...).
bool fx_primitive_s::ParseFlags(const char *str)
{
    return ParseFlagList(str, g_0815fae0, 0x18);
}

// 0x0812ede2  size=45  FUN_0812ede2  status=translated  naming=inferred
// Evidence: dispatched for keys "spawnFlags"/"spawnFlag"; uses the 13-entry name table at 0x0815fcc0 (org2fromTrace, ...).
bool fx_primitive_s::ParseSpawnFlags(const char *str)
{
    return ParseFlagList(str, g_0815fcc0, 0xd);
}

// 0x0812ee10  size=697  FUN_0812ee10  status=translated  naming=inferred
// Evidence: dispatched for keys "acceleration"/"accel"; sets flag 0x100000 (useRandomVelocity2) when lo != hi, scales by life (+0x5c) * 0.001f.
bool fx_primitive_s::ParseAcceleration(const char *str)
{
    float lo[3];
    float hi[3];
    if (!FX_ParseVec3Pair((uint32)this, str, lo, hi))
        return false;

    if (hi[0] < lo[0])
        Float_Swap(&lo[0], &hi[0]);
    if (hi[1] < lo[1])
        Float_Swap(&lo[1], &hi[1]);
    if (hi[2] < lo[2])
        Float_Swap(&lo[2], &hi[2]);

    float lifeMax = life.hi;
    float m0 = FX_MaxAbsFloor((uint32)this, lo[0], hi[0]);
    float m1 = FX_MaxAbsFloor((uint32)this, lo[1], hi[1]);
    float m2 = FX_MaxAbsFloor((uint32)this, lo[2], hi[2]);
    m0 = m0 + m0;
    m1 = m1 + m1;
    m2 = m2 + m2;
    float m = Float_Max_08131618(m0, m1);
    m = Float_Max_08131618(m, m2);
    float scaled = lifeMax * 0.001f * m;

    FX_BuildCurveRate(this, lo[0], scaled, lifeMax, 0x12, scaled);
    FX_BuildCurveRate(this, lo[1], scaled, lifeMax, 0x13, scaled);
    FX_BuildCurveRate(this, lo[2], scaled, lifeMax, 0x14, scaled);
    if (!Vec3_Equal3(lo, hi)) {
        flags |= 0x100000;
        FX_BuildCurveRate(this, hi[0], scaled, lifeMax, 0x15, 1.0f);
        FX_BuildCurveRate(this, hi[1], scaled, lifeMax, 0x16, 1.0f);
        FX_BuildCurveRate(this, hi[2], scaled, lifeMax, 0x17, 1.0f);
    }
    return true;
}

// 0x0812f0ca  size=93  FUN_0812f0ca  status=translated  naming=inferred
// Evidence: dispatched for key "gravity"; stores a float range at +0x258.
bool fx_primitive_s::ParseGravity(const char *str)
{
    float lo, hi;
    bool ok = FX_ParseVec2((uint32)this, str, &lo, &hi);
    if (ok)
        FxRange_Set(&gravity, lo, hi);
    return ok;
}

// 0x0812f128  size=93  FUN_0812f128  status=translated  naming=inferred
// Evidence: dispatched for key "density"; stores a float range at +0x260.
bool fx_primitive_s::ParseDensity(const char *str)
{
    float lo, hi;
    bool ok = FX_ParseVec2((uint32)this, str, &lo, &hi);
    if (ok)
        FxRange_Set(&density, lo, hi);
    return ok;
}

// 0x0812f186  size=93  FUN_0812f186  status=translated  naming=inferred
// Evidence: dispatched for key "variance"; stores a float range at +0x268.
bool fx_primitive_s::ParseVariance(const char *str)
{
    float lo, hi;
    bool ok = FX_ParseVec2((uint32)this, str, &lo, &hi);
    if (ok)
        FxRange_Set(&variance, lo, hi);
    return ok;
}

// 0x0812f1e4  size=67  FUN_0812f1e4  status=translated  naming=observed
// Evidence: key string "sequenceStartFrameMode" selects this handler in 0x08130248; accepts 0..2.
bool fx_primitive_s::ParseSequenceStartFrameMode(const char *str)
{
    int value = atoi(str);
    if (value < 0 || value > 2)
        return false;
    sequenceStartFrameMode = value;
    return true;
}

// 0x0812f228  size=64  FUN_0812f228  status=translated  naming=observed
// Evidence: key string "sequenceFixedFrameValue"; stores unconditionally, succeeds if > 0.
bool fx_primitive_s::ParseSequenceFixedFrameValue(const char *str)
{
    int value = atoi(str);
    sequenceFixedFrameValue = value;
    return sequenceFixedFrameValue > 0;
}

// 0x0812f268  size=67  FUN_0812f268  status=translated  naming=observed
// Evidence: key string "sequencePlayRateMode"; accepts 0..1.
bool fx_primitive_s::ParseSequencePlayRateMode(const char *str)
{
    int value = atoi(str);
    if (value < 0 || value > 1)
        return false;
    sequencePlayRateMode = value;
    return true;
}

// 0x0812f2ac  size=72  FUN_0812f2ac  status=translated  naming=observed
// Evidence: key string "sequenceFixedFpsValue"; stores unconditionally, succeeds if >= 0.
bool fx_primitive_s::ParseSequenceFixedFpsValue(const char *str)
{
    double value = atof(str);
    sequenceFixedFpsValue = (float)value;
    return 0.0 <= sequenceFixedFpsValue;
}

// 0x0812f2f4  size=67  FUN_0812f2f4  status=translated  naming=observed
// Evidence: key string "sequenceLoopMode"; accepts 0..1.
bool fx_primitive_s::ParseSequenceLoopMode(const char *str)
{
    int value = atoi(str);
    if (value < 0 || value > 1)
        return false;
    sequenceLoopMode = value;
    return true;
}

// 0x0812f338  size=64  FUN_0812f338  status=translated  naming=observed
// Evidence: key string "sequenceLoopTimes"; stores unconditionally, succeeds if >= 0.
bool fx_primitive_s::ParseSequenceLoopTimes(const char *str)
{
    int value = atoi(str);
    sequenceLoopTimes = value;
    return -1 < sequenceLoopTimes;
}

// 0x0812f378  size=67  FUN_0812f378  status=translated  naming=observed
// Evidence: key string "spawnFrustumCullRadius"; stores only if >= 0.
bool fx_primitive_s::ParseSpawnFrustumCullRadius(const char *str)
{
    float value = (float)atof(str);
    if (0.0 <= value)
        spawnFrustumCullRadius = value;
    return 0.0 <= value;
}

// 0x0812f3bc  size=148  FUN_0812f3bc  status=translated  naming=inferred
// Evidence: called for key "start" of rgb channels 0/1 in ParseChannel (0x0812ffa2); fills 3 start ranges of the channel and marks it used.
bool fx_primitive_s::ParseChannelStartVec3(FxChannelInit *init, const char *str, int channel)
{
    float lo[6];
    float hi[4];
    if (!FX_ParseVec3Pair((uint32)this, str, lo, hi))
        return false;
    for (int i = 0; i != 3; i++)
        FxRange_Set(&init[channel].start[i], lo[i], hi[i]);
    init[channel].used = 1;
    return true;
}

// 0x0812f450  size=151  FUN_0812f450  status=translated  naming=inferred
// Evidence: called for key "end" of channels 0/1; same as ParseChannelStartVec3 into the end[] ranges (+0x18).
bool fx_primitive_s::ParseChannelEndVec3(FxChannelInit *init, const char *str, int channel)
{
    float lo[6];
    float hi[4];
    if (!FX_ParseVec3Pair((uint32)this, str, lo, hi))
        return false;
    for (int i = 0; i != 3; i++)
        FxRange_Set(&init[channel].end[i], lo[i], hi[i]);
    init[channel].used = 1;
    return true;
}

// 0x0812f4e8  size=147  FUN_0812f4e8  status=translated  naming=inferred
// Evidence: called for key "start" of scalar channels (>= 2); sorts lo/hi and stores start[0].
bool fx_primitive_s::ParseChannelStart(FxChannelInit *init, const char *str, int channel)
{
    float lo, hi;
    if (!FX_ParseVec2((uint32)this, str, &lo, &hi))
        return false;
    if (hi < lo)
        Float_Swap(&lo, &hi);
    FxRange_Set(&init[channel].start[0], lo, hi);
    init[channel].used = 1;
    return true;
}

// 0x0812f57c  size=150  FUN_0812f57c  status=translated  naming=inferred
// Evidence: called for key "end" of scalar channels; sorts lo/hi and stores end[0] (+0x18).
bool fx_primitive_s::ParseChannelEnd(FxChannelInit *init, const char *str, int channel)
{
    float lo, hi;
    if (!FX_ParseVec2((uint32)this, str, &lo, &hi))
        return false;
    if (hi < lo)
        Float_Swap(&lo, &hi);
    FxRange_Set(&init[channel].end[0], lo, hi);
    init[channel].used = 1;
    return true;
}

// 0x0812f612  size=115  FUN_0812f612  status=translated  naming=inferred
// Evidence: called for keys "parm"/"parms" in ParseChannel; stores the range at channel offset +0x30.
bool fx_primitive_s::ParseChannelParm(FxChannelInit *init, const char *str, int channel)
{
    float lo, hi;
    bool ok = FX_ParseVec2((uint32)this, str, &lo, &hi);
    if (ok) {
        FxRange_Set(&init[channel].parm, lo, hi);
        init[channel].used = 1;
    }
    return ok;
}

// 0x0812f686  size=109  FUN_0812f686  status=translated  naming=inferred
// Evidence: called for keys "flags"/"flag" in ParseChannel; ORs parsed flag bits (FX_ParseWaveFlags) into channel offset +0x38.
bool fx_primitive_s::ParseChannelFlags(FxChannelInit *init, const char *str, int channel)
{
    uint32 parsed;
    bool ok = FX_ParseWaveFlags((uint32)this, str, &parsed);
    if (ok) {
        init[channel].flags = parsed | init[channel].flags;
        init[channel].used = 1;
    }
    return ok;
}

// 0x0812f6f4  size=222  FUN_0812f6f4  status=translated  naming=observed
// Evidence: string "fx_primitive_s::ParseMaterials called with an empty list!\n"; keys "shaders"/"shader".
bool fx_primitive_s::ParseMaterials(ParseNode *node)
{
    if (g_fxRequireMaterials != 0) {
        if (!ConfigAttr_HasMultipleValues(node)) {
            const char *name = ConfigAttr_GetFirstValue(node);
            if (name == 0) {
                FxPrintf("fx_primitive_s::ParseMaterials called with an empty list!\n");
                return false;
            }
            int material = Material_RegisterStub(name);
            FX_PtrListPush(&materials, (int)material);
        } else {
            for (ParseNode *child = ConfigBlock_GetFirstAttr(node); child != 0; child = ConfigNode_GetNextAlias(child)) {
                const char *name = ConfigBlock_GetName(child);
                int material = Material_RegisterStub(name);
                FX_PtrListPush(&materials, (int)material);
            }
        }
    }
    return true;
}

// 0x0812f7d2  size=270  FUN_0812f7d2  status=translated  naming=observed
// Evidence: strings "fx_primitive_s::ParseModels called with an empty list!\n" and "...could not register model '%s'\n"; keys "models"/"model".
bool fx_primitive_s::ParseModels(ParseNode *node)
{
    if (!ConfigAttr_HasMultipleValues(node)) {
        const char *name = ConfigAttr_GetFirstValue(node);
        if (name == 0) {
            FxPrintf("fx_primitive_s::ParseModels called with an empty list!\n");
            return false;
        }
        void *model = Model_RegisterByName(name);
        if (model == 0) {
            FxPrintf("fx_primitive_s::ParseModels, could not register model '%s'\n", name);
            return false;
        }
        FX_PtrListPush(&materials, (int)model);
    } else {
        for (ParseNode *child = ConfigBlock_GetFirstAttr(node); child != 0; child = ConfigNode_GetNextAlias(child)) {
            const char *name = ConfigBlock_GetName(child);
            void *model = Model_RegisterByName(name);
            if (model == 0) {
                FxPrintf("fx_primitive_s::ParseModels, could not register model '%s'\n", name);
                return false;
            }
            FX_PtrListPush(&materials, (int)model);
        }
    }
    return true;
}

// 0x0812f8e0  size=284  FUN_0812f8e0  status=translated  naming=observed
// Evidence: strings "fx_primitive_s::ParseImpactFxStrings called with an empty list!\n", "FxTemplate: Impact effect file not found.\n"; key "impactfx"; sets flags 0x820.
bool fx_primitive_s::ParseImpactFxStrings(ParseNode *node)
{
    if (!ConfigAttr_HasMultipleValues(node)) {
        const char *name = ConfigAttr_GetFirstValue(node);
        if (name == 0) {
            FxPrintf("fx_primitive_s::ParseImpactFxStrings called with an empty list!\n");
            return false;
        }
        void *fx = FX_GetEffectByName(name);
        if (fx == 0) {
            FxPrintf("FxTemplate: Impact effect file not found.\n");
            return false;
        }
        FX_PtrListAdd(&impactFx, (int)fx);
    } else {
        for (ParseNode *child = ConfigBlock_GetFirstAttr(node); child != 0; child = ConfigNode_GetNextAlias(child)) {
            const char *name = ConfigBlock_GetName(child);
            void *fx = FX_GetEffectByName(name);
            if (fx == 0) {
                FxPrintf("FxTemplate: Impact effect file not found.\n");
                return false;
            }
            FX_PtrListAdd(&impactFx, (int)fx);
        }
    }
    flags |= 0x820;
    return true;
}

// 0x0812f9fc  size=284  FUN_0812f9fc  status=translated  naming=observed
// Evidence: strings "fx_primitive_s::ParseDeathFxStrings called with an empty list!\n", "FxTemplate: Death effect file not found.\n"; key "deathfx"; sets flag 0x200.
bool fx_primitive_s::ParseDeathFxStrings(ParseNode *node)
{
    if (!ConfigAttr_HasMultipleValues(node)) {
        const char *name = ConfigAttr_GetFirstValue(node);
        if (name == 0) {
            FxPrintf("fx_primitive_s::ParseDeathFxStrings called with an empty list!\n");
            return false;
        }
        void *fx = FX_GetEffectByName(name);
        if (fx == 0) {
            FxPrintf("FxTemplate: Death effect file not found.\n");
            return false;
        }
        FX_PtrListAdd(&deathFx, (int)fx);
    } else {
        for (ParseNode *child = ConfigBlock_GetFirstAttr(node); child != 0; child = ConfigNode_GetNextAlias(child)) {
            const char *name = ConfigBlock_GetName(child);
            void *fx = FX_GetEffectByName(name);
            if (fx == 0) {
                FxPrintf("FxTemplate: Death effect file not found.\n");
                return false;
            }
            FX_PtrListAdd(&deathFx, (int)fx);
        }
    }
    flags |= 0x200;
    return true;
}

// 0x0812fb18  size=284  FUN_0812fb18  status=translated  naming=observed
// Evidence: strings "fx_primitive_s::ParseEmitterFxStrings called with an empty list!\n", "FxTemplate: Emitter effect file not found.\n"; key "emitfx"; sets flag 0x100.
bool fx_primitive_s::ParseEmitterFxStrings(ParseNode *node)
{
    if (!ConfigAttr_HasMultipleValues(node)) {
        const char *name = ConfigAttr_GetFirstValue(node);
        if (name == 0) {
            FxPrintf("fx_primitive_s::ParseEmitterFxStrings called with an empty list!\n");
            return false;
        }
        void *fx = FX_GetEffectByName(name);
        if (fx == 0) {
            FxPrintf("FxTemplate: Emitter effect file not found.\n");
            return false;
        }
        FX_PtrListAdd(&emitFx, (int)fx);
    } else {
        for (ParseNode *child = ConfigBlock_GetFirstAttr(node); child != 0; child = ConfigNode_GetNextAlias(child)) {
            const char *name = ConfigBlock_GetName(child);
            void *fx = FX_GetEffectByName(name);
            if (fx == 0) {
                FxPrintf("FxTemplate: Emitter effect file not found.\n");
                return false;
            }
            FX_PtrListAdd(&emitFx, (int)fx);
        }
    }
    flags |= 0x100;
    return true;
}

// 0x0812fc34  size=264  FUN_0812fc34  status=translated  naming=observed
// Evidence: strings "fx_primitive_s::ParsePlayFxStrings called with an empty list!\n", "FxTemplate: Effect file not found.\n"; key "playfx" (no flag change).
bool fx_primitive_s::ParsePlayFxStrings(ParseNode *node)
{
    if (!ConfigAttr_HasMultipleValues(node)) {
        const char *name = ConfigAttr_GetFirstValue(node);
        if (name == 0) {
            FxPrintf("fx_primitive_s::ParsePlayFxStrings called with an empty list!\n");
            return false;
        }
        void *fx = FX_GetEffectByName(name);
        if (fx == 0) {
            FxPrintf("FxTemplate: Effect file not found.\n");
            return false;
        }
        FX_PtrListAdd(&playFx, (int)fx);
    } else {
        for (ParseNode *child = ConfigBlock_GetFirstAttr(node); child != 0; child = ConfigNode_GetNextAlias(child)) {
            const char *name = ConfigBlock_GetName(child);
            void *fx = FX_GetEffectByName(name);
            if (fx == 0) {
                FxPrintf("FxTemplate: Effect file not found.\n");
                return false;
            }
            FX_PtrListAdd(&playFx, (int)fx);
        }
    }
    return true;
}

// 0x0812fd3c  size=280  FUN_0812fd3c  status=translated  naming=observed
// Evidence: format "%f %f %f %f" (time + rgb) per child; called for key "curve" on channels 0/1; stores the curve at +0x100+channel*12.
void fx_primitive_s::ParseCurveVec3(ParseNode *node, int channel)
{
    ParseNode *first = ConfigBlock_GetFirstAttr(node);
    int count = 0;
    for (ParseNode *c = first; c != 0; c = ConfigNode_GetNextAlias(c))
        count++;
    float *points = (float *)Hunk_AllocateTempMemory(count << 4);
    count = 0;
    for (ParseNode *c = first; c != 0; c = ConfigNode_GetNextAlias(c)) {
        const char *text = ConfigBlock_GetName(c);
        sscanf(text, "%f %f %f %f", &points[count * 4], &points[count * 4 + 1], &points[count * 4 + 2], &points[count * 4 + 3]);
        count++;
    }
    channels[channel].curve = Curve_Create(points, 3, count);
    Hunk_FreeTempMemory((int *)points);
}

// 0x0812fe54  size=248  FUN_0812fe54  status=translated  naming=observed
// Evidence: format "%f %f" (time + value) per child; called for key "curve" on scalar channels.
void fx_primitive_s::ParseCurveFloat(ParseNode *node, int channel)
{
    ParseNode *first = ConfigBlock_GetFirstAttr(node);
    int count = 0;
    for (ParseNode *c = first; c != 0; c = ConfigNode_GetNextAlias(c))
        count++;
    float *points = (float *)Hunk_AllocateTempMemory(count << 3);
    count = 0;
    for (ParseNode *c = first; c != 0; c = ConfigNode_GetNextAlias(c)) {
        const char *text = ConfigBlock_GetName(c);
        sscanf(text, "%f %f", &points[count * 2], &points[count * 2 + 1]);
        count++;
    }
    channels[channel].curve = Curve_Create(points, 1, count);
    Hunk_FreeTempMemory((int *)points);
}

// 0x0812ff4c  size=86  FUN_0812ff4c  status=translated  naming=observed
// Evidence: key "scale" in ParseChannel; stores a range at channel slot +4 (+0x104+channel*12).
void fx_primitive_s::ParseChannelScale(const char *str, int channel)
{
    float lo, hi;
    if (FX_ParseVec2((uint32)this, str, &lo, &hi))
        FxRange_Set(&channels[channel].scale, lo, hi);
}

// 0x0812ffa2  size=636  FUN_0812ffa2  status=translated  naming=observed
// Evidence: strings "curve","scale","start","end","parm","parms","flags","flag" and "Unknown key parsing a channel: %s\n".
bool fx_primitive_s::ParseChannel(FxChannelInit *init, ParseNode *node, int channel)
{
    for (ParseNode *child = ConfigAttr_GetFirstValueAlias(node); child != 0; child = ConfigNode_GetNextAlias2(child)) {
        const char *key = ConfigBlock_GetName(child);
        const char *value = ConfigAttr_GetFirstValue(child);
        if (strcasecmp(key, "curve") == 0) {
            if (channel == 0 || channel == 1)
                ParseCurveVec3(child, channel);
            else
                ParseCurveFloat(child, channel);
        } else if (strcasecmp(key, "scale") == 0) {
            ParseChannelScale(value, channel);
        } else if (strcasecmp(key, "start") == 0) {
            if (channel == 0 || channel == 1)
                ParseChannelStartVec3(init, value, channel);
            else
                ParseChannelStart(init, value, channel);
        } else if (strcasecmp(key, "end") == 0) {
            if (channel == 0 || channel == 1)
                ParseChannelEndVec3(init, value, channel);
            else
                ParseChannelEnd(init, value, channel);
        } else if (strcasecmp(key, "parm") == 0 || strcasecmp(key, "parms") == 0) {
            ParseChannelParm(init, value, channel);
        } else if (strcasecmp(key, "flags") == 0 || strcasecmp(key, "flag") == 0) {
            ParseChannelFlags(init, value, channel);
        } else {
            FxPrintf("Unknown key parsing a channel: %s\n", key);
        }
    }
    return true;
}

// 0x0813021e  size=42  FUN_0813021e  status=translated  naming=observed
// Evidence: key "shaderImpact" copies up to 0x20 bytes into +0x20.
bool fx_primitive_s::ParseShaderImpact(const char *str)
{
    Q_strncpyz(shaderImpact, str, 0x20);
    return true;
}

struct FxGroupKey {
    const char *name;
    int channel;
};

static const FxGroupKey g_fxGroupKeys[] = {
    { "rgb", 0 }, { "rgb2", 1 }, { "rgbRand", 1 }, { "alpha", 2 }, { "alphaRand", 3 },
    { "size", 4 }, { "width", 4 }, { "sizeRand", 5 }, { "size2", 6 }, { "width2", 6 },
    { "size2Rand", 7 }, { "length", 8 }, { "height", 8 }, { "lengthRand", 9 },
    { "rotationDelta", 10 }, { "rotationDeltaRand", 11 },
    { "velocityX", 0xc }, { "velocityY", 0xd }, { "velocityZ", 0xe },
    { "velocityXRand", 0xf }, { "velocityYRand", 0x10 }, { "velocityZRand", 0x11 },
    { "velocity2X", 0x12 }, { "velocity2Y", 0x13 }, { "velocity2Z", 0x14 },
    { "velocity2XRand", 0x15 }, { "velocity2YRand", 0x16 }, { "velocity2ZRand", 0x17 },
};

// 0x08130248  size=4695  FUN_08130248  status=translated  naming=inferred
// Evidence: parses all effect primitive keys (strings "count", "shaders", ..., "Unknown key parsing an effect primitive: %s\n", "^1FX Error while parsing key '%s'\n") and then the channel groups ("rgb", "alpha", "velocityX", ...).
bool fx_primitive_s::ParseKeys(FxChannelInit *init, ParseNode *node)
{
    ParseNode *kv = ConfigAttr_GetFirstValueAlias(node);
    const char *key;
    const char *value;
    bool ok;

    FX_ChannelTableInit((uint32)this, init);
    for (;; kv = ConfigNode_GetNextAlias2(kv)) {
        key = 0;
        if (kv == 0)
            break;
        key = ConfigBlock_GetName(kv);
        value = ConfigAttr_GetFirstValue(kv);

        if (strcasecmp(key, "count") == 0) {
            ok = FX_PrimParseCount(this, value);
        } else if (strcasecmp(key, "shaders") == 0 || strcasecmp(key, "shader") == 0) {
            ok = ParseMaterials(kv);
        } else if (strcasecmp(key, "models") == 0 || strcasecmp(key, "model") == 0) {
            ok = ParseModels(kv);
        } else if (strcasecmp(key, "impactfx") == 0) {
            ok = ParseImpactFxStrings(kv);
        } else if (strcasecmp(key, "deathfx") == 0) {
            ok = ParseDeathFxStrings(kv);
        } else if (strcasecmp(key, "emitfx") == 0) {
            ok = ParseEmitterFxStrings(kv);
        } else if (strcasecmp(key, "playfx") == 0) {
            ok = ParsePlayFxStrings(kv);
        } else if (strcasecmp(key, "life") == 0) {
            ok = FX_PrimParseLife(this, value);
        } else if (strcasecmp(key, "cullrange") == 0) {
            double d = atof(value);
            cullRange = (float)d;
            continue;
        } else if (strcasecmp(key, "spawnRange") == 0) {
            ok = FX_PrimParseSpawnRange(this, value);
        } else if (strcasecmp(key, "delay") == 0) {
            ok = FX_PrimParseDelay(this, value);
        } else if (strcasecmp(key, "bounce") == 0 || strcasecmp(key, "intensity") == 0) {
            ok = FX_PrimParseIntensity(this, value);
        } else if (strcasecmp(key, "min") == 0) {
            ok = FX_PrimParseMin(this, value);
        } else if (strcasecmp(key, "max") == 0) {
            ok = FX_PrimParseMax(this, value);
        } else if (strcasecmp(key, "angle") == 0 || strcasecmp(key, "angles") == 0) {
            ok = ParseAngles(value);
        } else if (strcasecmp(key, "angleDelta") == 0) {
            ok = ParseAngleDelta(value);
        } else if (strcasecmp(key, "velocity") == 0 || strcasecmp(key, "vel") == 0) {
            ok = ParseVelocity(value);
        } else if (strcasecmp(key, "acceleration") == 0 || strcasecmp(key, "accel") == 0) {
            ok = ParseAcceleration(value);
        } else if (strcasecmp(key, "gravity") == 0) {
            ok = ParseGravity(value);
        } else if (strcasecmp(key, "density") == 0) {
            ok = ParseDensity(value);
        } else if (strcasecmp(key, "variance") == 0) {
            ok = ParseVariance(value);
        } else if (strcasecmp(key, "origin") == 0) {
            ok = FX_PrimParseOrigin(this, value);
        } else if (strcasecmp(key, "origin2") == 0) {
            ok = FX_PrimParseOrigin2(this, value);
        } else if (strcasecmp(key, "radius") == 0) {
            ok = FX_PrimParseRadius(this, value);
        } else if (strcasecmp(key, "height") == 0) {
            ok = FX_PrimParseHeight(this, value);
        } else if (strcasecmp(key, "wind") == 0) {
            ok = FX_PrimParseWind(this, value);
        } else if (strcasecmp(key, "rotation") == 0) {
            ok = FX_PrimParseRotation(this, value);
        } else if (Q_stricmp(key, "rotationDelta") == 0) {
            ok = FX_PrimParseRotationDelta(this, value);
        } else if (strcasecmp(key, "flags") == 0 || strcasecmp(key, "flag") == 0) {
            ok = ParseFlags(value);
        } else if (strcasecmp(key, "spawnFlags") == 0 || strcasecmp(key, "spawnFlag") == 0) {
            ok = ParseSpawnFlags(value);
        } else if (strcasecmp(key, "nonUniformScale") == 0) {
            nonUniformScale = atoi(value) != 0;
            continue;
        } else if (strcasecmp(key, "useLength") == 0) {
            useLength = atoi(value) != 0;
            continue;
        } else if (strcasecmp(key, "name") == 0) {
            if (value == 0)
                break;
            Q_strncpyz(name, value, 0x20);
            continue;
        } else if (strcasecmp(key, "shaderImpact") == 0) {
            ok = ParseShaderImpact(value);
        } else if (strcasecmp(key, "sequenceStartFrameMode") == 0) {
            ok = ParseSequenceStartFrameMode(value);
        } else if (strcasecmp(key, "sequenceFixedFrameValue") == 0) {
            ok = ParseSequenceFixedFrameValue(value);
        } else if (strcasecmp(key, "sequencePlayRateMode") == 0) {
            ok = ParseSequencePlayRateMode(value);
        } else if (strcasecmp(key, "sequenceFixedFpsValue") == 0) {
            ok = ParseSequenceFixedFpsValue(value);
        } else if (strcasecmp(key, "sequenceLoopMode") == 0) {
            ok = ParseSequenceLoopMode(value);
        } else if (strcasecmp(key, "sequenceLoopTimes") == 0) {
            ok = ParseSequenceLoopTimes(value);
        } else if (strcasecmp(key, "spawnFrustumCullRadius") == 0) {
            ok = ParseSpawnFrustumCullRadius(value);
        } else {
            FxPrintf("Unknown key parsing an effect primitive: %s\n", key);
            return false;
        }
        if (!ok)
            break;
    }

    if (key != 0) {
        FxPrintf("^1FX Error while parsing key '%s'\n", key);
        return false;
    }

    if (maxs[0] < mins[0] || maxs[1] < mins[1] || maxs[2] < mins[2]) {
        FxPrintf("^1FX bounding box mins / maxs invalid for effect '%s'\n", (const char *)this);
        return false;
    }
    if (maxs[2] - mins[2] < maxs[0] - mins[0] || maxs[2] - mins[2] < maxs[1] - mins[1]) {
        FxPrintf("^1FX bounding box width or depth is larger than height for effect '%s'\n", (const char *)this);
        return false;
    }

    ParseNode *group = ConfigBlock_GetFirstChild(node);
    while (group != 0) {
        key = ConfigBlock_GetName(group);
        int channel = -1;
        for (unsigned i = 0; i < sizeof(g_fxGroupKeys) / sizeof(g_fxGroupKeys[0]); i++) {
            if (strcasecmp(key, g_fxGroupKeys[i].name) == 0) {
                channel = g_fxGroupKeys[i].channel;
                break;
            }
        }
        if (channel < 0) {
            FxPrintf("Unknown group key parsing a particle: %s\n", key);
            return false;
        }
        ok = ParseChannel(init, group, channel);
        if (!ok)
            break;
        group = ConfigNode_GetNext(group);
        key = 0;
    }
    if (key == 0) {
        FX_PrimBuildCurves(this, init);
        return true;
    }
    FxPrintf("^1FX Error while parsing key '%s'\n", key);
    return false;
}

// 0x081314a0  size=66  FUN_081314a0  status=translated  naming=inferred
// Evidence: allocates the 0x600-byte (24 * 0x40) channel scratch block from temp memory, runs ParseKeys, frees it.
bool fx_primitive_s::Parse(ParseNode *node)
{
    FxChannelInit *init = (FxChannelInit *)Hunk_AllocateTempMemory(0x600);
    bool ok = ParseKeys(init, node);
    Hunk_FreeTempMemory((int *)init);
    return ok;
}

// 0x081314e2  size=67  FUN_081314e2  status=translated  naming=inferred
// Evidence: vsnprintf into a 0x400 buffer then forwards the text as the format string of Com_Printf (0x08060dea).
void FxPrintf(const char *fmt, ...)
{
    char buffer[1036];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 0x400, fmt, ap);
    va_end(ap);
    Com_Printf(buffer);
}

// 0x08131526  size=10  FUN_08131526  status=translated  naming=inferred
// Evidence: returns 0 unconditionally; used by ParseMaterials as the material registration of this (dedicated) build.
int Material_RegisterStub(const char *name)
{
    return 0;
}

// 0x08131530  size=58  FUN_08131530  status=translated  naming=inferred
// Evidence: String_IsXModelPath(name) gate, then registers name+7 via Fx_LoadXModel; used by ParseModels.
void *Model_RegisterByName(const char *name)
{
    if (!String_IsXModelPath(name))
        return 0;
    return Fx_LoadXModel(name + 7);
}

// 0x0813156a  size=56  FUN_0813156a  status=translated  naming=unknown
// Evidence: switch returning 1 only for input 5, 0 for every other value; no caller context.
int FX_IsEmitterType(int kind)
{
    switch (kind) {
    default:
        return 0;
    case 1:
    case 3:
    case 6:
    case 7:
    case 0xc:
        return 0;
    case 5:
        return 1;
    }
}

// 0x081315a2  size=22  FUN_081315a2  status=translated  naming=inferred
// Evidence: stores two 32-bit values at +0/+4; used for every {lo, hi} range write.
void FxRange_Set(FxRange *range, float lo, float hi)
{
    range->lo = lo;
    range->hi = hi;
}

// 0x081315b8  size=71  FUN_081315b8  status=translated  naming=inferred
// Evidence: returns lo if lo == hi, else Rand_Float(lo, hi) (0x080aaa84).
float FxRange_Random(const FxRange *range)
{
    float result;
    if (range->lo == range->hi)
        result = range->lo;
    else
        result = Rand_Float(range->lo, range->hi);
    return result;
}

// 0x08131600  size=24  FUN_08131600  status=translated  naming=inferred
// Evidence: (hi - lo) * t + lo (x87, single operand precision as stored).
float FxRange_Lerp(const FxRange *range, float t)
{
    return (range->hi - range->lo) * t + range->lo;
}

// 0x08131618  size=36  FUN_08131618  status=translated  naming=inferred
// Evidence: Float_SelectGE(a - b, a, b), i.e. max(a, b) for non-NaN input.
float Float_Max_08131618(float a, float b)
{
    return Float_SelectGE(a - b, a, b);
}

// 0x0813163c  size=61  FUN_0813163c  status=translated  naming=unknown
// Evidence: nested Float_SelectGE: inner = (a >= c) ? c : a ; result = (b >= a) ? b : inner. Purpose not established.
float FX_ClampSelect(float a, float b, float c)
{
    float inner = Float_SelectGE(a - c, c, a);
    return Float_SelectGE(b - a, b, inner);
}

// 0x0813167a  size=47  FUN_0813167a  status=translated  naming=inferred
// Evidence: copies three 32-bit values from param 1 to param 2.
void Vec3_Copy3(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// 0x081316aa  size=106  FUN_081316aa  status=translated  naming=inferred
// Evidence: returns 1 when all three float components compare equal.
int Vec3_Equal3(const float *a, const float *b)
{
    int result = 0;
    if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2])
        result = 1;
    return result;
}

// 0x08131714  size=41  FUN_08131714  status=translated  naming=inferred
// Evidence: fcmp against 0.0; returns a when cond >= 0 else b.
float Float_SelectGE(float cond, float a, float b)
{
    float result;
    if (0.0 <= cond)
        result = a;
    else
        result = b;
    return result;
}

// 0x0813173e  size=34  FUN_0813173e  status=translated  naming=inferred
// Evidence: exchanges two 32-bit values.
void Float_Swap(float *a, float *b)
{
    uint32 t = *(uint32 *)a;
    *(uint32 *)a = *(uint32 *)b;
    *(uint32 *)b = t;
}

// 0x08131760  size=11  FUN_08131760  status=translated  naming=inferred
// Evidence: returns the field at +4 (next sibling) of a parse node; used to iterate children.
ParseNode *ConfigNode_GetNextAlias(ParseNode *node)
{
    return node->next;
}

// 0x0813176c  size=11  FUN_0813176c  status=translated  naming=inferred
// Evidence: identical to ConfigNode_GetNextAlias (duplicate instance); used by ParseChannel and ParseKeys.
ParseNode *ConfigNode_GetNextAlias2(ParseNode *node)
{
    return node->next;
}

// 0x08131778  size=11  FUN_08131778  status=translated  naming=inferred
// Evidence: returns the field at +0x10 (first child) of a parse node.
ParseNode *ConfigBlock_GetFirstAttr(ParseNode *node)
{
    return node->firstChild;
}

// 0x08131784  size=11  FUN_08131784  status=translated  naming=inferred
// Evidence: identical to ConfigBlock_GetFirstAttr (duplicate instance).
ParseNode *ConfigAttr_GetFirstValueAlias(ParseNode *node)
{
    return node->firstChild;
}

// 0x08131790  size=5  FUN_08131790  status=translated  naming=unknown
// Evidence: empty function taking one int argument (ignored).
void Fx_NoOp(int arg)
{
}

// 0x08131796  size=56  FUN_08131796  status=translated  naming=unknown
// Evidence: scans g_08a533f0[] but the loop bound (index must be <= 0) examines only slot 0; calls Fx_NoOp(0) if it is nonzero.
void Fx_CheckClientSlot0(void)
{
    int i = 0;
    for (;;) {
        if (0 < i)
            return;
        if (g_08a533f0[i] != 0)
            break;
        i = i + 1;
    }
    Fx_NoOp(i);
}

// 0x081317ce  size=70  FUN_081317ce  status=translated  naming=inferred
// Evidence: gets the client's DObj via Com_GetClientDObj(handle, Net_CurrentClientNum()) and returns DObj_GetBoneIndexByName, or -1 when no DObj.
int Fx_GetBoneIndex(int handle, int boneNameId)
{
    int clientNum = Net_CurrentClientNum();
    struct DObj_s *obj = Com_GetClientDObj(handle, clientNum);
    if (obj == 0)
        return -1;
    return DObj_GetBoneIndexByName((const struct DObj *)obj, boneNameId);
}

// 0x08131814  size=35  FUN_08131814  status=translated  naming=unknown
// Evidence: forwards (g_fxScheduler, a, b) to FxScheduler_PlayEffect_DefaultAxis (builds three vectors and calls FxScheduler_PlayEffect).
void FX_PlayEffect(struct FxEffectTemplate *effect, float *origin)
{
    FxScheduler_PlayEffect_DefaultAxis(g_fxScheduler, effect, origin);
}

// 0x081318c6  size=26  FUN_081318c6  status=translated  naming=unknown
// Evidence: forwards its char argument to FX_Init (per-client init, uses Net_CurrentClientNum slots).
void FX_InitThunk(char flag)
{
    FX_Init(flag);
}

// 0x081318e0  size=20  FUN_081318e0  status=translated  naming=unknown
// Evidence: calls FX_ShutdownSystem(1) (clears g_fxScheduler and the per-client slot when flag is set).
void FX_ShutdownSystem_Full(void)
{
    FX_ShutdownSystem(1);
}

// 0x081318f4  size=20  FUN_081318f4  status=translated  naming=unknown
// Evidence: calls FX_ShutdownSystem(0).
void FX_ShutdownSystem_Partial(void)
{
    FX_ShutdownSystem(0);
}

// 0x08131908  size=45  FUN_08131908  status=translated  naming=unknown
// Evidence: calls FX_SetupFrustum(g_fxSystem, a, b) (sets up fov scale) then sets g_08a53818 = 1.
void Fx_SetView(const struct fx_view_s *view, float farDelta)
{
    FX_SetupFrustum(g_fxSystem, view, farDelta);
    g_08a53818 = 1;
}

// 0x0813198c  size=28  FUN_0813198c  status=translated  naming=unknown
// Evidence: tail-forwards to FxEffectTemplate_GetDuration(g_fxScheduler, a); the callee's float result is left in st0 (decompiler showed void).
float FX_GetEffectDuration(struct FxEffectTemplate *effect)
{
    return FxEffectTemplate_GetDuration(g_fxScheduler, effect);
}

// 0x081319a8  size=668  FUN_081319a8  status=translated  naming=inferred
// Evidence: id Tech style token reader (skips whitespace, '//' and '/* */' comments, quoted strings, read-to-EOL mode) into the 0x400 byte buffer g_fxParseTokenBuf.
char *Fx_ParseToken(char **data_p, char allowLineBreaks, char readToEol)
{
    char *data = *data_p;
    size_t len = 0;
    g_fxParseTokenBuf[0] = 0;
    if (data == 0)
        return g_fxParseTokenBuf;

    char c;
    for (;;) {
        bool newlines = false;
        for (; (c = *data) < '!'; data++) {
            if (c == 0) {
                *data_p = 0;
                return g_fxParseTokenBuf;
            }
            if (c == '\n')
                newlines = true;
        }
        if (newlines && allowLineBreaks == 0) {
            *data_p = data;
            return g_fxParseTokenBuf;
        }
        c = *data;
        if (c == '/' && data[1] == '/') {
            for (data = data + 2; *data != 0 && *data != '\n'; data++) {
            }
            continue;
        }
        if (c == '/' && data[1] == '*') {
            for (data = data + 2; *data != 0 && (*data != '*' || data[1] != '/'); data++) {
            }
            if (*data != 0)
                data = data + 2;
            continue;
        }
        break;
    }

    if (c == '"' && readToEol == 0) {
        data = data + 1;
        for (;;) {
            char ch = *data;
            data = data + 1;
            if (ch == '"' || ch == 0)
                break;
            if ((int)len < 0x400) {
                g_fxParseTokenBuf[len] = ch;
                len = len + 1;
            }
        }
    } else if (readToEol == 0) {
        while (' ' < c) {
            if ((int)len < 0x400) {
                g_fxParseTokenBuf[len] = c;
                len = len + 1;
            }
            data = data + 1;
            c = *data;
        }
    } else {
        while (c != '\n' && c != '\r' && (c != '/' || (data[1] != '/' && data[1] != '*'))) {
            if ((int)len < 0x400) {
                g_fxParseTokenBuf[len] = c;
                len = len + 1;
            }
            data = data + 1;
            c = *data;
        }
        for (; len != 0 && g_fxParseTokenBuf[len - 1] < ' '; len = len - 1) {
        }
    }

    size_t n = len;
    if (g_fxParseTokenBuf[0] == '"') {
        n = len - 1;
        memmove(g_fxParseTokenBuf, g_fxParseTokenBuf + 1, n);
        if (n != 0 && g_fxParseTokenBuf[len - 2] == '"')
            n = len - 2;
    }
    len = n;
    if (0x3ff < (int)len)
        len = 0;
    g_fxParseTokenBuf[len] = 0;
    *data_p = data;
    return g_fxParseTokenBuf;
}

// 0x08131c44  size=61  FUN_08131c44  status=translated  naming=inferred
// Evidence: initialises a string page {buffer = Com_MallocZeroed(size), next = 0, size, used = 0}.
void Fx_StringPage_Init(Fx_StringPage *page, int size)
{
    page->next = 0;
    page->size = size;
    page->used = 0;
    page->buffer = (char *)Com_MallocZeroed(page->size);
}

// 0x08131c82  size=61  FUN_08131c82  status=translated  naming=inferred
// Evidence: identical to Fx_StringPage_Init (duplicate instance used by Fx_StringPage_Append).
void Fx_StringPage_Init2(Fx_StringPage *page, int size)
{
    page->next = 0;
    page->size = size;
    page->used = 0;
    page->buffer = (char *)Com_MallocZeroed(page->size);
}

// 0x08131cd6  size=21  FUN_08131cd6  status=translated  naming=inferred
// Evidence: releases the page buffer with Com_Free_A.
void Fx_StringPage_FreeBuffer(Fx_StringPage *page)
{
    Com_Free_A(page->buffer);
}

// 0x08131cec  size=149  FUN_08131cec  status=translated  naming=inferred
// Evidence: appends a string (plus NUL slot if addNul) to the page; when full and a head pointer is given, allocates a new page of the same size, links it and retries there. C++ EH cleanup for the page allocation is not reproduced.
char *Fx_StringPage_Append(Fx_StringPage *page, const char *str, char addNul, Fx_StringPage **head)
{
    size_t len = strlen(str);
    if (addNul != 0)
        len = len + 1;
    if (page->size < (int)(len + page->used + 1)) {
        if (head == 0)
            return 0;
        Fx_StringPage *fresh = (Fx_StringPage *)ConfigPool_Alloc(0x10);
        Fx_StringPage_Init2(fresh, page->size);
        ConfigLink_SetNextAlt(*head, fresh);
        *head = ConfigLink_GetNext(*head);
        return Fx_StringPage_Append(*head, str, addNul, 0);
    }
    strcpy(page->buffer + page->used, str);
    page->used = len + page->used;
    page->buffer[page->used] = 0;
    return (page->buffer + page->used) - len;
}

// 0x08131e40  size=73  FUN_08131e40  status=translated  naming=inferred
// Evidence: walks the page chain (next via ConfigLink_GetNext), freeing each buffer and node.
void Fx_StringPage_FreeChain(Fx_StringPage *page)
{
    while (page != 0) {
        Fx_StringPage *next = ConfigLink_GetNext(page);
        if (page != 0) {
            Fx_StringPage_FreeBuffer(page);
            ConfigPool_Free(page);
        }
        page = next;
    }
}

// 0x08131e8a  size=43  FUN_08131e8a  status=translated  naming=inferred
// Evidence: sets +0 to the name pointer and zeroes +4/+8/+0xc of a parse node.
void ParseNode_Init(ParseNode *node, const char *name)
{
    node->name = name;
    node->next = 0;
    node->field8 = 0;
    node->fieldC = 0;
}

// 0x08131eb6  size=43  FUN_08131eb6  status=translated  naming=inferred
// Evidence: identical to ParseNode_Init (duplicate instance used by ConfigAttr_AddValue).
void ParseNode_Init2(ParseNode *node, const char *name)
{
    node->name = name;
    node->next = 0;
    node->field8 = 0;
    node->fieldC = 0;
}

// 0x08131ee2  size=188  FUN_08131ee2  status=translated  naming=inferred
// Evidence: appends the text to the string page, wrapping it in '"' (rodata 0x08160620) when it contains a space or is empty.
bool Fx_StringPage_AppendQuoted(void *unused, Fx_StringPage **head, const char *text)
{
    if (strchr(text, ' ') == 0 && *text != 0) {
        Fx_StringPage_Append(*head, text, 0, head);
    } else {
        Fx_StringPage_Append(*head, "\"", 0, head);
        Fx_StringPage_Append(*head, text, 0, head);
        Fx_StringPage_Append(*head, "\"", 0, head);
    }
    return true;
}

// 0x08131f9e  size=68  FUN_08131f9e  status=translated  naming=inferred
// Evidence: ParseNode_Init, clears the child list at +0x10, and if text is given calls ConfigAttr_AddValue(node, text, 0) to add it as a child.
void ParseNode_ConstructWithText(ParseNode *node, const char *name, const char *text)
{
    ParseNode_Init(node, name);
    node->firstChild = 0;
    if (text != 0)
        ConfigAttr_AddValue(node, text, 0);
}

// 0x08131fe2  size=68  FUN_08131fe2  status=translated  naming=inferred
// Evidence: identical to ParseNode_ConstructWithText (duplicate instance).
void ParseNode_ConstructWithText2(ParseNode *node, const char *name, const char *text)
{
    ParseNode_Init(node, name);
    node->firstChild = 0;
    if (text != 0)
        ConfigAttr_AddValue(node, text, 0);
}

// 0x08132026  size=62  FUN_08132026  status=translated  naming=inferred
// Evidence: frees every node of the child list at +0x10 via ConfigLink_Free, advancing with ConfigNode_GetNextAlias.
void ParseNode_FreeChildren(ParseNode *node)
{
    while (node->firstChild != 0) {
        ParseNode *next = ConfigNode_GetNextAlias(node->firstChild);
        ConfigLink_Free(node->firstChild);
        node->firstChild = next;
    }
}
