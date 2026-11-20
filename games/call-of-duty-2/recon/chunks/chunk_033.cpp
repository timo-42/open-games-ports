#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_033_types.h"

/* Helpers: float tests used by the binary's "x != 0 || isnan(x)" pattern. */
static inline int Float_NonZeroOrNaN(float v)
{
    return v != 0.0f || v != v;
}

static inline XAnimNode *NodeAt(XAnimNodeTable *table, int id)
{
    return (XAnimNode *)((char *)table + id * 8);
}

static inline XAnimSlot *SlotFor(uint16 index)
{
    return &g_xanimSlotRing[index];
}

/* Forward declarations for every function of this chunk (address order). */
void XAnim_DumpNodeWeights(XAnimTree *tree, int nodeId, int depth);
void XAnim_BlendNode(XAnimTree *tree, int nodeId, float weight, int accumulate, int normalize, XAnimBlendAccum *out);
long double XAnim_NodeDurationSec(XAnimNodeTable *table, int nodeId);
int XAnim_NodeDurationMs(XAnimNodeTable *table, int nodeId);
long double XAnim_NodeTime(XAnimTree *tree, int nodeId);
long double XAnim_NodeOldWeight(XAnimTree *tree, int nodeId);
int XAnim_NodeFinished(XAnimTree *tree, int nodeId);
uint16 XAnim_NodeChildCount(XAnimNodeTable *table, int nodeId);
int XAnim_NodeChildId(XAnimNodeTable *table, int nodeId, int index);
const char *XAnim_NodeClipName(XAnimNodeTable *table, int nodeId);
uint8 XAnim_ClipFlag28(XAnimParts *clip);
const char *XAnim_NodeDisplayName(XAnimNodeTable *table, int nodeId);
uint32 XAnim_LoadU32(const void *p);
uint32 XAnim_LoadU32At4(const void *p);
int XAnim_NodeHasNoClipOrFlag(XAnimNodeTable *table, int nodeId);
void XAnim_BindingSetTime(XAnimBinding *binding, float arg);
int XAnim_BindingSetWeight(XAnimBinding *binding, float weight, int play);
int XAnim_BindingInit(int *out);
void XAnim_BindingRebuild(XAnimBinding *binding, const uint32 *mask);
void XAnim_DumpTree(XAnimBinding *binding);
void XAnim_BlendRootNode(XAnimTree *tree, int nodeId, float *dst2, float *dst3, uint8 flag);
void XAnim_BlendRootAll(XAnimTree *tree, int nodeId, float *dst2, float *dst3);
void XAnim_NodeBlendSample(XAnimNodeTable *table, int nodeId, float *dst2, float *dst3, float p5, float p6);
void XAnim_NodeBlendSampleB(XAnimNodeTable *table, int nodeId, float *dst2, float *dst3, float p5);
XAnimSlot *XAnim_AllocSlot(XAnimTree *tree, int nodeId);
void XAnim_SetSlotTime(XAnimTree *tree, int nodeId, float time);
void XAnim_SetSubtreeTime(XAnimTree *tree, int nodeId, float time);
void XAnim_SetChildrenTime(XAnimTree *tree, int nodeId, float time);
void XAnim_SetWeightLimit(XAnimTree *tree, int nodeId, float weight, float scale);
void XAnim_BlendStartA(XAnimTree *tree, int nodeId, float weight, float scale, float p5, int p6, int p7);
void XAnim_BlendStartB(XAnimTree *tree, int nodeId, float weight, float scale, float p5, int p6, int p7);
int XAnim_BlendChain(XAnimTree *tree, uint32 nodeId, uint32 stopId, float weight, float scale, float p6, int p7, int doStack);
void XAnim_FreeSlots(XAnimTree *tree, int nodeId);
void XAnim_FreeAllSlots(XAnimTree *tree);
void XAnim_ResetTree(XAnimTree *tree);
int XAnim_MaxWeightChild(XAnimTree *tree, int nodeId);
int XAnim_SetSlotWeight(XAnimTree *tree, int nodeId, float weight, float scale, float p5, char p6, int p7, short p8);
void XAnim_SetSlotValue24(XAnimTree *tree, int nodeId, uint32 raw);
void *XAnim_VoidLoopLoaded(int size);
void XAnim_SetNodeLoopMode(XAnimNodeTable *table, int nodeId, char mode);
void XAnim_ApplyLoopModeRec(XAnimNodeTable *table, int nodeId);
void XAnim_ApplyLoopModeRoot(XAnimNodeTable *table);
int XAnim_NodeIsLeaf(XAnimNodeTable *table, int nodeId);
void XAnim_ResetSlotTimes(XAnimTree *tree, int nodeId, float time);
void XAnim_UpdateSlotPhase(XAnimTree *tree, XAnimSlot *slot, const short *clipRef);
void XAnim_ReleaseNodeSlots(XAnimTree *tree, uint32 nodeId);
void XAnim_BlendTailChain(XAnimTree *tree, uint32 nodeId, float scale);
int XAnim_BlendStepC(XAnimTree *tree, int nodeId, float weight, float scale, float p5, int p6, int p7, int doStack);
void XAnim_SetSlotValueAlias(XAnimTree *tree, int nodeId, uint32 raw);
uint8 XAnim_NodeLoopFlag(XAnimNodeTable *table, int nodeId);
int XAnim_ClipHasKey(XAnimNodeTable *table, int nodeId, uint32 key);
void XAnim_BlendStepRec(XAnimTree *tree, uint32 nodeId, float weight, float scale, float p5, int p6, int p7, int doStack);
void XAnim_CopySlot(const XAnimSlot *src, XAnimSlot *dst);
void XAnim_CopyTreeSlots(XAnimTree *tree, XAnimTree *other);
void XAnim_InitSlotRecord(const XAnimSlot *src, XAnimSlot *dst);
XAnimSlot *XAnim_SlotAt(int index);
void XAnim_SamplePart2(const void *ctx, xanim_track_s *part, float weight, float *out);
void XAnim_SamplePart4(const void *ctx, xanim_track_s *part, float weight, float *out);
void XAnim_SamplePart3(const void *ctx, xanim_track_s *part, float weight, float *out);
void XAnim_BlendBoneChannels(uint16 *track, const uint8 *boneIdx, float ctx, float weight, uint8 *pose, const uint32 *mask);
void XAnim_BlendBoneChannelsB(uint16 *track, const uint8 *boneIdx, float ctx, float weight, uint8 *pose, const uint32 *mask);
void XAnim_SamplePart2B(const void *ctx, xanim_track_s *part, float weight, float *out);
void XAnim_SamplePart4B(const void *ctx, xanim_track_s *part, float weight, float *out);
void XAnim_SamplePart3B(const void *ctx, xanim_track_s *part, float weight, float *out);
void XAnim_BlendBoneChannelsB(uint16 *track, const uint8 *boneIdx, float ctx, float weight, uint8 *pose, const uint32 *mask);
long double XAnim_Abs(float v);
long double XAnim_RSqrt(float v);
void XAnim_Vec2Zero(float *v);
void XAnim_Vec2Copy(const float *src, float *dst);
void XAnim_Vec2Scale(const float *a, float s, float *out);
void XAnim_Vec2MulAdd(const float *a, float s, const float *b, float *out);
long double XAnim_Vec2LenSq(const float *v);
void XAnim_Vec3Zero(float *v);
void XAnim_Vec3Copy(const float *src, float *dst);
void XAnim_Vec3Add(const float *a, const float *b, float *out);
void XAnim_Vec3Sub(const float *a, const float *b, float *out);
void XAnim_Vec3Lerp(const float *a, const float *b, float t, float *out);
void XAnim_Vec3Scale(const float *a, float s, float *out);
void XAnim_Vec3MulAdd(const float *a, float s, const float *b, float *out);
void XAnim_Vec4Zero(float *v);
void XAnim_Vec4Scale(const float *a, float s, float *out);
void XAnim_Vec4MulAdd(const float *a, float s, const float *b, float *out);
long double XAnim_Vec4LenSq(const float *v);
void XAnim_DequantPair(const short *a, const short *b, float p, float *out);
void XAnim_LerpPair(const float *a, float s, const short *b, const short *c, float p, float *out);
void XAnim_LerpQuat(const float *a, float s, const short *b, const short *c, float p, float *out);
void XAnim_MulAddShort2(const float *a, float s, const short *b, float *out);
void XAnim_MulAddShort4(const float *a, float s, const short *b, float *out);
void XAnim_DequantVec4(const short *a, float s, float *out);

/* ---------------------------------------------------------------------- */

// 0x080c03d8  size=1011  FUN_080c03d8  status=translated  naming=inferred
// no string names this function; the "(weight)/(time)/(realtimedelta)" format strings and the
// tree walk over children identify it as a debug dump of the blend state of one node subtree
void XAnim_DumpNodeWeights(XAnimTree *tree, int nodeId, int depth)
{
    uint16 slotIndex;
    XAnimNode *node;
    uint16 childCount;
    XAnimSlot *slot;
    const char *prefix;
    const char *nodeName;
    float elapsed;
    float realtimeDelta;
    int i;

    slotIndex = tree->slotOfNode[nodeId];
    if (slotIndex == 0) {
        return;
    }
    node = NodeAt(tree->table, nodeId);
    childCount = node->childCount;
    slot = SlotFor(slotIndex);
    for (i = 0; i < depth; i = i + 1) {
        Com_Printf(" ");
    }
    nodeName = XAnim_NodeDisplayName(tree->table, nodeId);
    if (slot->weight < slot->oldWeight) {
        prefix = "^1";
    }
    else if (slot->oldWeight < slot->weight) {
        prefix = "^4";
    }
    else {
        prefix = "";
    }
    if (childCount == 0) {
        elapsed = slot->curTime - slot->prevTime;
        if (elapsed < 0.0f) {
            elapsed = elapsed + 1.0f;
        }
        if ((node->clip->length != 0.0f) || (node->clip->length != node->clip->length)) {
            realtimeDelta = elapsed / node->clip->length;
        }
        else {
            realtimeDelta = 0.0f;
        }
        if (slot->nameIndex == 0) {
            Com_Printf("%s%s: (weight) %.2f -> %.2f, (time) %.2f -> %.2f, (realtimedelta) %.2f\n",
                        prefix, nodeName, (double)slot->oldWeight, (double)slot->weight,
                        (double)slot->prevTime, (double)slot->curTime, (double)realtimeDelta);
        }
        else {
            const char *slotName = ScrStr_TextFromHandle(slot->nameIndex);
            Com_Printf("%s%s: (weight) %.2f -> %.2f, (time) %.2f -> %.2f, (realtimedelta) %.2f, '%s'\n",
                        prefix, nodeName, (double)slot->oldWeight, (double)slot->weight,
                        (double)slot->prevTime, (double)slot->curTime, (double)realtimeDelta,
                        slotName);
        }
    }
    else {
        if (slot->nameIndex == 0) {
            if (XAnim_NodeHasNoClipOrFlag(tree->table, nodeId) == 0) {
                Com_Printf("%s%s: (weight) %.2f -> %.2f\n", prefix, nodeName,
                            (double)slot->oldWeight, (double)slot->weight);
            }
            else {
                Com_Printf("%s%s: (weight) %.2f -> %.2f, (time) %.2f -> %.2f\n", prefix, nodeName,
                            (double)slot->oldWeight, (double)slot->weight,
                            (double)slot->prevTime, (double)slot->curTime);
            }
        }
        else {
            if (XAnim_NodeHasNoClipOrFlag(tree->table, nodeId) == 0) {
                const char *slotName = ScrStr_TextFromHandle(slot->nameIndex);
                Com_Printf("%s%s: (weight) %.2f -> %.2f, '%s'\n", prefix, nodeName,
                            (double)slot->oldWeight, (double)slot->weight, slotName);
            }
            else if (slot->childSlot == 0) {
                const char *slotName = ScrStr_TextFromHandle(slot->nameIndex);
                Com_Printf("%s%s: (weight) %.2f -> %.2f, (time) %.2f -> %.2f, '%s'\n", prefix,
                            nodeName, (double)slot->oldWeight, (double)slot->weight,
                            (double)slot->prevTime, (double)slot->curTime, slotName);
            }
            else {
                const char *slotName = ScrStr_TextFromHandle(slot->nameIndex);
                Com_Printf("%s%s: (weight) %.2f -> %.2f, (time) %.2f -> %.2f, '%s'\n", prefix,
                            nodeName, (double)slot->oldWeight, (double)slot->weight,
                            (double)slot->prevTime, (double)slot->curTime, slotName);
            }
        }
        for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
            XAnim_DumpNodeWeights(tree, (int)node->firstChild + i, depth + 1);
        }
    }
}

// 0x080c07cc  size=1387  FUN_080c07cc  status=translated  naming=inferred
// recursive weighted blend of a node's children into out. Each child's weight comes from its slot
// (old weight when flag07 is 0, else the current weight). When the first two non-zero children are
// found, they are blended pairwise into a scratch (or out when accumulate is set) and the rest
// are added; normalize selects whether the accumulator is renormalized. A node without children
// samples its own clip through XAnim_AccumulatePartMotion (flag06 == 0) or XAnim_ApplyKeyedPartDelta.
void XAnim_BlendNode(XAnimTree *tree, int nodeId, float weight, int accumulate, int normalize, XAnimBlendAccum *out)
{
    XAnimNode *rec = NodeAt(tree->table, nodeId);
    uint32 childCount = rec->childCount;
    uint32 firstChild = rec->firstChild;
    XAnimBlendAccum scratch;
    XAnimBlendAccum *dst;
    XAnimSlot *slot;
    XAnimParts *clip;
    uint16 slotIndex;
    float firstWeight;
    float secondWeight;
    float lenSq;
    long double rsqrtLen;
    int i;
    int j;

    if (childCount != 0) {
        i = 0;
        for (;;) {
            if ((int)childCount <= i) {
                if (accumulate == 0) {
                    return;
                }
                XAnim_Vec2Zero(out->xy);
                out->weight = 0.0f;
                XAnim_Vec3Zero(out->xyz);
                return;
            }
            slotIndex = tree->slotOfNode[firstChild + (uint32)i];
            if (slotIndex != 0) {
                slot = SlotFor(slotIndex);
                firstWeight = (tree->flag07 == 0) ? slot->oldWeight : slot->weight;
                j = i;
                if (firstWeight != 0.0f) {
                    do {
                        do {
                            j = j + 1;
                            if ((int)childCount <= j) {
                                XAnim_BlendNode(tree, (int)(firstChild + (uint32)i), weight, accumulate, normalize, out);
                                return;
                            }
                            slotIndex = tree->slotOfNode[firstChild + (uint32)j];
                        } while (slotIndex == 0);
                        slot = SlotFor(slotIndex);
                        secondWeight = (tree->flag07 == 0) ? slot->oldWeight : slot->weight;
                    } while (secondWeight == 0.0f);

                    dst = (accumulate == 0) ? &scratch : out;
                    XAnim_BlendNode(tree, (int)(firstChild + (uint32)i), firstWeight, 1, 1, dst);
                    XAnim_BlendNode(tree, (int)(firstChild + (uint32)j), secondWeight, 0, 1, dst);
                    j = j + 1;
                    while (j < (int)childCount) {
                        slotIndex = tree->slotOfNode[firstChild + (uint32)j];
                        if (slotIndex != 0) {
                            slot = SlotFor(slotIndex);
                            secondWeight = (tree->flag07 == 0) ? slot->oldWeight : slot->weight;
                            if (secondWeight != 0.0f) {
                                XAnim_BlendNode(tree, (int)(firstChild + (uint32)j), secondWeight, 0, 1, dst);
                            }
                        }
                        j = j + 1;
                    }
                    if (normalize == 0) {
                        if (Float_NonZeroOrNaN(out->weight)) {
                            XAnim_Vec3Scale(out->xyz, (float)(1.0 / out->weight), out->xyz);
                        }
                    }
                    else if (accumulate == 0) {
                        lenSq = (float)XAnim_Vec2LenSq(dst->xy);
                        if (Float_NonZeroOrNaN(lenSq)) {
                            rsqrtLen = XAnim_RSqrt(lenSq);
                            XAnim_Vec2MulAdd(out->xy, (float)(rsqrtLen * (long double)weight), dst->xy, out->xy);
                        }
                        if (Float_NonZeroOrNaN(dst->weight)) {
                            XAnim_Vec3MulAdd(out->xyz, weight / dst->weight, dst->xyz, out->xyz);
                            out->weight = out->weight + weight;
                        }
                    }
                    else {
                        lenSq = (float)XAnim_Vec2LenSq(out->xy);
                        if (Float_NonZeroOrNaN(lenSq)) {
                            rsqrtLen = XAnim_RSqrt(lenSq);
                            XAnim_Vec2Scale(out->xy, (float)(rsqrtLen * (long double)weight), out->xy);
                        }
                        if (Float_NonZeroOrNaN(out->weight)) {
                            XAnim_Vec3Scale(out->xyz, weight / out->weight, out->xyz);
                            out->weight = weight;
                        }
                    }
                    return;
                }
            }
            i = i + 1;
        }
    }

    if (accumulate != 0) {
        XAnim_Vec2Zero(out->xy);
        out->weight = 0.0f;
        XAnim_Vec3Zero(out->xyz);
    }
    clip = rec->clip;
    if (clip->hasData == 0) {
        return;
    }
    slotIndex = tree->slotOfNode[nodeId];
    if (slotIndex == 0) {
        return;
    }
    slot = SlotFor(slotIndex);
    if (tree->flag06 == 0) {
        XAnim_AccumulatePartMotion((const struct xanim_keyed_part_s *)clip, weight, slot->prevTime, slot->curTime, (float *)out, 1);
        return;
    }
    XAnim_ApplyKeyedPartDelta((const struct xanim_keyed_part_s *)clip, weight, slot->curTime, (struct xanim_part_state_s *)out);
}

// 0x080c0d38  size=52  FUN_080c0d38  status=translated  naming=inferred
// returns frames / frameRate of the node's clip: the clip duration in seconds (float division
// is carried out in x87 long double)
long double XAnim_NodeDurationSec(XAnimNodeTable *table, int nodeId)
{
    XAnimParts *clip = NodeAt(table, nodeId)->clip;
    return (long double)clip->numFrames / (long double)clip->frameRate;
}

// 0x080c0d6c  size=61  FUN_080c0d6c  status=translated  naming=inferred
// duration of the node's clip rounded to whole milliseconds (FPU round to nearest)
int XAnim_NodeDurationMs(XAnimNodeTable *table, int nodeId)
{
    long double seconds = XAnim_NodeDurationSec(table, nodeId);
    /* gameplay-fix(wpn_mantle_turret): 0x080c0d8c..0x080c0d9e: fistp under control word | 0xc00 (truncate), not round-to-nearest */
    return (int)(1000.0L * seconds);
}

// 0x080c0daa  size=67  FUN_080c0daa  status=translated  naming=inferred
// current blend time of the node's slot, 0.0 when the node has no slot
long double XAnim_NodeTime(XAnimTree *tree, int nodeId)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    float value;
    if (slotIndex == 0) {
        value = 0.0f;
    }
    else {
        value = SlotFor(slotIndex)->curTime;
    }
    return (long double)value;
}

// 0x080c0dee  size=67  FUN_080c0dee  status=translated  naming=inferred
// blend weight at the start of the blend (oldWeight) of the node's slot, 0.0 without a slot
long double XAnim_NodeOldWeight(XAnimTree *tree, int nodeId)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    float value;
    if (slotIndex == 0) {
        value = 0.0f;
    }
    else {
        value = SlotFor(slotIndex)->oldWeight;
    }
    return (long double)value;
}

// 0x080c0e32  size=138  FUN_080c0e32  status=translated  naming=inferred
// reports whether the node's slot still needs updating: 1 without a slot, otherwise 1 when the
// time wrapped (current < previous or current == 1.0) or the cycle counter passed its limit
int XAnim_NodeFinished(XAnimTree *tree, int nodeId)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    XAnimSlot *slot;
    int result;
    if (slotIndex == 0) {
        return 1;
    }
    slot = SlotFor(slotIndex);
    result = 0;
    if ((slot->curTime < slot->prevTime) || (slot->curTime == 1.0f) ||
        ((int16)slot->cycleLimit < (int16)slot->cycleCount)) {
        result = 1;
    }
    return result;
}

// 0x080c0ebc  size=16  FUN_080c0ebc  status=translated  naming=unknown
// number of children of a node
uint16 XAnim_NodeChildCount(XAnimNodeTable *table, int nodeId)
{
    return NodeAt(table, nodeId)->childCount;
}

// 0x080c0ecc  size=19  FUN_080c0ecc  status=translated  naming=unknown
// id of the index-th child: children are consecutive from firstChild
int XAnim_NodeChildId(XAnimNodeTable *table, int nodeId, int index)
{
    return (int)((uint32)NodeAt(table, nodeId)->firstChild + (uint32)index);
}

// 0x080c0ee0  size=56  FUN_080c0ee0  status=translated  naming=inferred
// clip name of a leaf node; interior nodes return the empty string
const char *XAnim_NodeClipName(XAnimNodeTable *table, int nodeId)
{
    XAnimNode *node = NodeAt(table, nodeId);
    if (node->childCount == 0) {
        return node->clip->name;
    }
    return "";
}

// 0x080c0f18  size=12  FUN_080c0f18  status=translated  naming=inferred
// returns the flag byte at clip offset 0x28
uint8 XAnim_ClipFlag28(XAnimParts *clip)
{
    return clip->flag28;
}

// 0x080c0f24  size=194  FUN_080c0f24  status=translated  naming=inferred
// display name of a node: the table's name array when present, otherwise the clip name for
// leaves or the node id for interior nodes; a leaf without a name and with a flagged clip is
// marked as missing. Format strings: "%i" and "^3%s (missing)" (rodata)
const char *XAnim_NodeDisplayName(XAnimNodeTable *table, int nodeId)
{
    XAnimNode *node = NodeAt(table, nodeId);
    const char *name;
    int markMissing;

    if (table->nodeNames == 0) {
        if (node->childCount == 0) {
            name = node->clip->name;
        }
        else {
            name = va("%i", nodeId);
        }
        return name;
    }
    name = table->nodeNames[nodeId];
    markMissing = 0;
    if ((node->childCount == 0) && (XAnim_ClipFlag28(node->clip) != 0)) {
        markMissing = 1;
    }
    if (markMissing) {
        name = va("^3%s (missing)", name);
    }
    return name;
}

// 0x080c0fe6  size=10  FUN_080c0fe6  status=translated  naming=unknown
// reads the 32-bit word at the pointer (no structure recovered)
uint32 XAnim_LoadU32(const void *p)
{
    return *(const uint32 *)p;
}

// 0x080c0ff0  size=11  FUN_080c0ff0  status=translated  naming=unknown
// reads the 32-bit word at byte offset 4 of the pointer
uint32 XAnim_LoadU32At4(const void *p)
{
    return *(const uint32 *)((const char *)p + 4);
}

// 0x080c106c  size=89  FUN_080c106c  status=translated  naming=inferred
// forwards a time value to the binding's tree when the binding has one
void XAnim_BindingSetTime(XAnimBinding *binding, float arg)
{
    uint32 span[2];
    uint16 spanEnd;
    uint8 scratch[9];
    uint8 done;

    if (binding->tree != 0) {
        span[0] = 0;
        spanEnd = 0;
        XAnim_UpdateBlendRecursive((struct XAnimTree *)binding->tree, 0, (float *)span, arg, 1, scratch, &done);
    }
}

// 0x080c114e  size=254  FUN_080c114e  status=translated  naming=inferred
// sets the binding's weight: without play it stores the weight directly; with play it advances
// the blend by a small step and returns 1 when the blend is still running
int XAnim_BindingSetWeight(XAnimBinding *binding, float weight, int play)
{
    float current;

    if (binding->tree == 0) {
        return 0;
    }
    if (play == 0) {
        XAnim_AdvanceNodeCursor(binding->tree, 0, weight, 0);
        return 0;
    }
    current = XAnim_EvalNodeWeightMin(binding->tree, 0, weight);
    if ((current == 1.0f) || (current = weight * current + 0.001, weight < current)) {
        XAnim_AdvanceNodeCursor(binding->tree, 0, weight, 1);
        return 0;
    }
    XAnim_AdvanceNodeCursor(binding->tree, 0, current, 1);
    return 1;
}

// 0x080c124c  size=19  FUN_080c124c  status=translated  naming=inferred
// writes the address of the default xanim object to *out and returns its value word
int XAnim_BindingInit(int *out)
{
    *out = (int)g_xanimPendingEntries;
    return g_xanimPendingCount;
}

// 0x080c1260  size=603  FUN_080c1260  status=translated  naming=inferred
// rebuilds the pose of a binding from a 128-bit mask: bits that are clear in the combined mask
// reset the bone to identity, and each part's keys are resampled for the cleared bits
void XAnim_BindingRebuild(XAnimBinding *binding, const uint32 *mask)
{
    uint32 *pose = (uint32 *)binding->pose;
    /* Verified against the disassembly: combined[] (ebp-0x68) and shadow[] (ebp-0x58) are the
     * written_mask / skip_mask words at +0x4000 / +0x4010 of the 0x4020-byte calc buffer that
     * XAnim_EvalNodeParts fills (chunk 032 xanim_calc_buffer_s). */
    struct { uint8 parts[0x4000]; uint32 written[4]; uint32 skip[4]; } calc;
    uint32 *combined = calc.written;
    uint32 *shadow = calc.skip;
    int allSet;
    int i;
    int bit;
    int partIndex;
    int k;
    XAnimPoseBone *bone;
    xanim_track_s *part;
    uint8 *keyPtr;

    allSet = 1;
    for (i = 0; i < 4; i = i + 1) {
        combined[i] = ~mask[i] | pose[i];
        if (combined[i] != 0xffffffffu) {
            allSet = 0;
        }
    }
    if (allSet != 0) {
        return;
    }
    for (i = 0; i < 4; i = i + 1) {
        pose[i] = mask[i] | pose[i];
        shadow[i] = combined[i];
    }
    bone = (XAnimPoseBone *)((uint8 *)pose + 0x30);
    if (binding->tree != 0) {
        shadow[3] = shadow[3] | 0x80000000u;
        XAnim_EvalNodeParts((struct xanim_anim_state_s *)binding, 0, 1.0f, (struct xanim_part_state_s *)bone, 1, 0, (struct xanim_calc_buffer_s *)&calc, 0);
    }
    bit = 0;
    for (partIndex = 0; partIndex < (int)(uint32)binding->partCountByte; partIndex = partIndex + 1) {
        /* binary: mov eax,[binding+i*4+0x1c]; mov eax,[eax] -- the slot holds a model whose first word is the part record */
        part = *(xanim_track_s **)binding->parts[partIndex];
        for (k = (int)part->leadCount; k != 0; k = k - 1) {
            if (((combined[bit >> 5] >> (bit & 0x1f)) & 1u) == 0) {
                bone->rot[0] = 0.0f;
                bone->rot[1] = 0.0f;
                bone->rot[2] = 0.0f;
                bone->rot[3] = 1.0f;
                XAnim_Vec3Zero(bone->pos);
            }
            bone = bone + 1;
            bit = bit + 1;
        }
        (void)part->dataPtr;
        keyPtr = *(uint8 **)((uint8 *)part + 8);
        for (k = (int)part->count - (int)part->leadCount; k != 0; k = k - 1) {
            if (((combined[bit >> 5] >> (bit & 0x1f)) & 1u) == 0) {
                XAnim_DequantVec4((const short *)keyPtr, 3.0518509e-05f, bone->rot);
                XAnim_Vec3Zero(bone->pos);
            }
            bone = bone + 1;
            bit = bit + 1;
            keyPtr = keyPtr + 8;
        }
    }
}

// 0x080c14bc  size=71  FUN_080c14bc  status=translated  naming=inferred
// prints the tree dump of a binding, or "NO TREE" when it has none
void XAnim_DumpTree(XAnimBinding *binding)
{
    if (binding->tree == 0) {
        Com_Printf("NO TREE\n");
    }
    else {
        XAnim_DumpNodeWeights(binding->tree, 0, 0);
        Com_Printf("\n");
    }
}

// 0x080c1504  size=181  FUN_080c1504  status=translated  naming=inferred
// blends the root of a tree with weight 1 and writes the 2-component and 3-component results;
// a zero component in the blend gives the identity (0, 1) and (0, 0, 0)
void XAnim_BlendRootNode(XAnimTree *tree, int nodeId, float *dst2, float *dst3, uint8 flag)
{
    XAnimBlendAccum accum;

    tree->flag06 = 0;
    tree->flag07 = flag;
    XAnim_BlendNode(tree, nodeId, 1.0f, 1, 0, &accum);
    if ((accum.xy[0] == 0.0f) || (accum.xy[1] == 0.0f)) {
        dst2[0] = 0.0f;
        dst2[1] = 1.0f;
    }
    else {
        XAnim_Vec2Copy(accum.xy, dst2);
    }
    XAnim_Vec3Copy(accum.xyz, dst3);
}

// 0x080c15ba  size=170  FUN_080c15ba  status=translated  naming=inferred
// blends the root of a tree with weight 1 and marks the tree with flags 1/1; a non-zero
// component gives the blended value, otherwise the identity (0, 1)
void XAnim_BlendRootAll(XAnimTree *tree, int nodeId, float *dst2, float *dst3)
{
    XAnimBlendAccum accum;

    tree->flag06 = 1;
    tree->flag07 = 1;
    XAnim_BlendNode(tree, nodeId, 1.0f, 1, 0, &accum);
    if (Float_NonZeroOrNaN(accum.xy[0]) || Float_NonZeroOrNaN(accum.xy[1])) {
        XAnim_Vec2Copy(accum.xy, dst2);
    }
    else {
        dst2[0] = 0.0f;
        dst2[1] = 1.0f;
    }
    XAnim_Vec3Copy(accum.xyz, dst3);
}

// 0x080c1664  size=280  FUN_080c1664  status=translated  naming=inferred
// samples one node: nodes with a clip (leaf with data) are blended through the clip evaluator,
// everything else gives the identity
void XAnim_NodeBlendSample(XAnimNodeTable *table, int nodeId, float *dst2, float *dst3, float p5, float p6)
{
    XAnimNode *node = NodeAt(table, nodeId);
    XAnimParts *clip;
    XAnimBlendAccum accum;

    if ((node->childCount == 0) && (clip = node->clip, clip->hasData != 0)) {
        XAnim_Vec2Zero(accum.xy);
        accum.weight = 0.0f;
        XAnim_Vec3Zero(accum.xyz);
        XAnim_AccumulatePartMotion((const struct xanim_keyed_part_s *)clip, 1.0f, p5, p6, accum.xy, 0);
        if (Float_NonZeroOrNaN(accum.xy[0]) || Float_NonZeroOrNaN(accum.xy[1])) {
            XAnim_Vec2Copy(accum.xy, dst2);
        }
        else {
            dst2[0] = 0.0f;
            dst2[1] = 1.0f;
        }
        XAnim_Vec3Copy(accum.xyz, dst3);
    }
    else {
        dst2[0] = 0.0f;
        dst2[1] = 1.0f;
        XAnim_Vec3Zero(dst3);
    }
}

// 0x080c177c  size=265  FUN_080c177c  status=translated  naming=inferred
// same as XAnim_NodeBlendSample with a single clip parameter for the evaluator
void XAnim_NodeBlendSampleB(XAnimNodeTable *table, int nodeId, float *dst2, float *dst3, float p5)
{
    XAnimNode *node = NodeAt(table, nodeId);
    XAnimParts *clip;
    XAnimBlendAccum accum;

    if ((node->childCount == 0) && (clip = node->clip, clip->hasData != 0)) {
        XAnim_Vec2Zero(accum.xy);
        accum.weight = 0.0f;
        XAnim_Vec3Zero(accum.xyz);
        XAnim_ApplyKeyedPartDelta((const struct xanim_keyed_part_s *)clip, 1.0f, p5, (struct xanim_part_state_s *)accum.xy);
        if (Float_NonZeroOrNaN(accum.xy[0]) || Float_NonZeroOrNaN(accum.xy[1])) {
            XAnim_Vec2Copy(accum.xy, dst2);
        }
        else {
            dst2[0] = 0.0f;
            dst2[1] = 1.0f;
        }
        XAnim_Vec3Copy(accum.xyz, dst3);
    }
    else {
        dst2[0] = 0.0f;
        dst2[1] = 1.0f;
        XAnim_Vec3Zero(dst3);
    }
}

// 0x080c1886  size=152  FUN_080c1886  status=translated  naming=inferred
// takes a slot from the free list (element 0 holds the list head), links it to the node and
// returns it; reports "exceeded maximum number of anim info" when the pool is empty
XAnimSlot *XAnim_AllocSlot(XAnimTree *tree, int nodeId)
{
    uint16 head = g_xanimSlotRing[0].nextFree;
    if (head == 0) {
        Com_Error(1, "\025exceeded maximum number of anim info");
        return 0;
    }
    g_xanimSlotRing[0].nextFree = g_xanimSlotRing[head].nextFree;
    g_xanimSlotRing[g_xanimSlotRing[0].nextFree].unk08 = 0;
    tree->slotOfNode[nodeId] = head;
    return &g_xanimSlotRing[head];
}

// 0x080c191e  size=184  FUN_080c191e  status=translated  naming=inferred
// records a requested blend time for the node's slot: keeps the smallest request while the
// weight is zero, resets the weight and clears the old weight for a zero request
void XAnim_SetSlotTime(XAnimTree *tree, int nodeId, float time)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    XAnimSlot *slot;

    if (slotIndex != 0) {
        slot = SlotFor(slotIndex);
        if ((slot->weight != 0.0f) || (slot->weight != slot->weight)) {
            slot->minRequest = time;
        }
        else if (time < slot->minRequest) {
            slot->minRequest = time;
        }
        slot->weight = 0.0f;
        if (time == 0.0f) {
            slot->oldWeight = 0.0f;
        }
        XAnim_ReleaseSlotHandle(slot);
    }
}

// 0x080c19d6  size=131  FUN_080c19d6  status=translated  naming=inferred
// applies a requested time to the node's slot and recursively to all of its children
void XAnim_SetSubtreeTime(XAnimTree *tree, int nodeId, float time)
{
    XAnimNode *node;
    uint16 childCount;
    int i;

    if (tree->slotOfNode[nodeId] != 0) {
        XAnim_SetSlotTime(tree, nodeId, time);
        node = NodeAt(tree->table, nodeId);
        childCount = node->childCount;
        for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
            XAnim_SetSubtreeTime(tree, (int)node->firstChild + i, time);
        }
    }
}

// 0x080c1a5a  size=90  FUN_080c1a5a  status=translated  naming=inferred
// applies a requested time to each child of the node, without touching the node itself
void XAnim_SetChildrenTime(XAnimTree *tree, int nodeId, float time)
{
    XAnimNode *node = NodeAt(tree->table, nodeId);
    uint16 childCount = node->childCount;
    int i;

    for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
        XAnim_SetSubtreeTime(tree, (int)node->firstChild + i, time);
    }
}

// 0x080c1ab4  size=336  FUN_080c1ab4  status=translated  naming=inferred
// for the siblings of nodeId under its parent: sets their requested time to the largest
// current old weight among the group (with the node's own difference folded in) times scale
void XAnim_SetWeightLimit(XAnimTree *tree, int nodeId, float weight, float scale)
{
    XAnimNode *parent;
    uint16 childCount;
    uint16 slotIndex;
    float maxOld = 0.0f;
    float oldWeight;
    float candidate;
    int i;

    if (nodeId == 0) {
        return;
    }
    parent = NodeAt(tree->table, (int)NodeAt(tree->table, nodeId)->parentIndex);
    maxOld = 0.0f;
    childCount = parent->childCount;
    for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
        slotIndex = tree->slotOfNode[(int)parent->firstChild + i];
        if (slotIndex == 0) {
            oldWeight = 0.0f;
        }
        else {
            oldWeight = SlotFor(slotIndex)->oldWeight;
        }
        if ((int)parent->firstChild + i == nodeId) {
            candidate = (float)XAnim_Abs(weight - oldWeight);
        }
        else {
            candidate = oldWeight;
        }
        if (maxOld < candidate) {
            maxOld = candidate;
        }
    }
    for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
        if ((int)parent->firstChild + i != nodeId) {
            XAnim_SetSlotTime(tree, (int)parent->firstChild + i, maxOld * scale);
        }
    }
}

// 0x080c1c04  size=127  FUN_080c1c04  status=translated  naming=inferred
// weight below 0.001 is treated as zero; sets the weight limit and starts a blend step
void XAnim_BlendStartA(XAnimTree *tree, int nodeId, float weight, float scale, float p5, int p6, int p7)
{
    if (weight < 0.001) {
        weight = 0.0f;
    }
    XAnim_SetWeightLimit(tree, nodeId, weight, scale);
    XAnim_BlendStepRec(tree, (uint32)nodeId, weight, scale, p5, p6, 0, p7);
}

// 0x080c1c84  size=370  FUN_080c1c84  status=translated  naming=inferred
// blends a chain of nodes from nodeId up its parents until stopId (or the root); the first node
// gets the requested weight, parents get full weight
int XAnim_BlendChain(XAnimTree *tree, uint32 nodeId, uint32 stopId, float weight, float scale, float p6, int p7, int doStack)
{
    int result;

    if (weight < 0.001) {
        weight = 0.0f;
    }
    XAnim_SetWeightLimit(tree, (int)nodeId, weight, scale);
    result = XAnim_SetSlotWeight(tree, (int)nodeId, weight, scale, p6, 0, p7, 0);
    XAnim_BlendTailChain(tree, nodeId, scale);
    if (doStack != 0) {
        XAnim_ReleaseNodeSlots(tree, nodeId);
    }
    for (;;) {
        if (nodeId == 0) {
            return 1;
        }
        nodeId = (uint32)NodeAt(tree->table, (int)nodeId)->parentIndex;
        if (nodeId == stopId) {
            break;
        }
        XAnim_SetWeightLimit(tree, (int)nodeId, 1.0f, scale);
        XAnim_SetSlotWeight(tree, (int)nodeId, 1.0f, scale, 1.0f, 0, 0, 0);
        if (doStack != 0) {
            XAnim_ReleaseNodeSlots(tree, nodeId);
        }
    }
    return result;
}

// 0x080c1df6  size=127  FUN_080c1df6  status=translated  naming=inferred
// like XAnim_BlendStartA but routes through the step that does not release the chain
void XAnim_BlendStartB(XAnimTree *tree, int nodeId, float weight, float scale, float p5, int p6, int p7)
{
    if (weight < 0.001) {
        weight = 0.0f;
    }
    XAnim_SetWeightLimit(tree, nodeId, weight, scale);
    XAnim_BlendStepC(tree, nodeId, weight, scale, p5, p6, 0, p7);
}

// 0x080c1ed0  size=136  FUN_080c1ed0  status=translated  naming=inferred
// frees the slots of a subtree bottom-up and clears the node's slot index
void XAnim_FreeSlots(XAnimTree *tree, int nodeId)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    XAnimNode *node;
    uint16 childCount;
    int i;

    if (slotIndex != 0) {
        node = NodeAt(tree->table, nodeId);
        childCount = node->childCount;
        for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
            XAnim_FreeSlots(tree, (int)node->firstChild + i);
        }
        XAnim_PushFreeSlot((int32)(unsigned long)tree, (int)slotIndex);
        tree->slotOfNode[nodeId] = 0;
    }
}

// 0x080c1f58  size=157  FUN_080c1f58  status=translated  naming=inferred
// releases the second per-node handle array that follows the slot array: each live handle is
// passed to ScrStr_DecRefOrFree together with the clip's word at +0x0e plus 0x10
void XAnim_FreeAllSlots(XAnimTree *tree)
{
    int nodeCount = tree->table->nodeCount;
    uint16 *handles = &tree->slotOfNode[nodeCount];
    int i;

    for (i = 0; i < nodeCount; i = i + 1) {
        if (handles[i] != 0) {
            ScrStr_DecRefOrFree((int)handles[i],
                        (int)*(const int16 *)((const char *)NodeAt(tree->table, i)->clip + 0x0e) + 0x10);
            handles[i] = 0;
        }
    }
}

// 0x080c1ff6  size=38  FUN_080c1ff6  status=translated  naming=inferred
// frees all slots of the tree from the root and releases the handle array
void XAnim_ResetTree(XAnimTree *tree)
{
    XAnim_FreeSlots(tree, 0);
    XAnim_FreeAllSlots(tree);
}

// 0x080c201c  size=194  FUN_080c201c  status=translated  naming=inferred
// returns the child (searched recursively) with the largest weight; the node itself when it
// has no children
int XAnim_MaxWeightChild(XAnimTree *tree, int nodeId)
{
    XAnimNode *node = NodeAt(tree->table, nodeId);
    float best;
    int bestNode;
    int i;
    float weight;
    int found;

    if (node->childCount == 0) {
        return nodeId;
    }
    best = 0.0f;
    bestNode = 0;
    for (i = 0; i < (int)(uint32)node->childCount; i = i + 1) {
        weight = g_xanimSlotRing[tree->slotOfNode[(int)node->firstChild + i]].weight;
        if ((best < weight) && ((found = XAnim_MaxWeightChild(tree, (int)node->firstChild + i)) != 0)) {
            bestNode = found;
            best = weight;
        }
    }
    return bestNode;
}

// 0x080c20de  size=562  FUN_080c20de  status=translated  naming=inferred
// configures the node's slot: allocates one when a non-zero request needs it, stores the old
// weight, the requested time and the new weight, and links the dominant child for named slots.
// The slot is addressed through a short*, so the binary's +0x10/+0x0c/+0x0e offsets are byte
// offsets 0x20/0x18/0x1c of XAnimSlot.
int XAnim_SetSlotWeight(XAnimTree *tree, int nodeId, float weight, float scale, float p5, char p6, int p7, short p8)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    XAnimSlot *slot;
    XAnimNode *node;
    uint16 dominant;

    if (slotIndex == 0) {
        if ((weight == 0.0f) && (p6 == 0)) {
            return 0;
        }
        slot = XAnim_AllocSlot(tree, nodeId);
        XAnim_ResetSlotRecord(slot);
    }
    else {
        slot = SlotFor(slotIndex);
        XAnim_ReleaseSlotHandle(slot);
    }
    if (nodeId == 0) {
        weight = 1.0f;
        scale = 0.0f;
        p5 = 1.0f;
    }
    if (scale == 0.0f) {
        slot->oldWeight = weight;
    }
    else if (slot->oldWeight == 0.0f) {
        /* gameplay-fix(wpn_turret2): 0x080c21c7..0x080c21d2 multiplies by the float constant 0x8150274 (0.001f = 0x3a83126f), not the double 0.001 */
        slot->oldWeight = (float)((long double)weight * (long double)0.0010000000474974513f);
    }
    if ((weight != 0.0f) || (weight != weight)) {
        if (slot->oldWeight <= weight) {
            slot->minRequest = ((weight - slot->oldWeight) * scale) / weight;
        }
        else {
            slot->minRequest = ((slot->oldWeight - weight) * scale) / slot->oldWeight;
        }
    }
    else if ((slot->weight != 0.0f) || (slot->weight != slot->weight)) {
        slot->minRequest = scale;
    }
    else if (scale < slot->minRequest) {
        slot->minRequest = scale;
    }
    slot->weight = weight;
    slot->value24 = p5;
    slot->nameIndex = (int16)p7;
    if (p7 != 0) {
        ScrStr_AddRef(p7);
    }
    node = NodeAt(tree->table, nodeId);
    if ((p7 == 0) || (node->childCount == 0) || ((node->clipFlagsLo & 3) == 0)) {
        slot->childSlot = 0;
    }
    else {
        dominant = (uint16)XAnim_MaxWeightChild(tree, nodeId);
        slot->childSlot = dominant;
        if (slot->childSlot == 0) {
            return 2;
        }
    }
    slot->flags06 = p8;
    return 0;
}

// 0x080c2310  size=50  FUN_080c2310  status=translated  naming=inferred
// stores a raw 32-bit word into the slot's value24 field (the binary writes it unconverted)
void XAnim_SetSlotValue24(XAnimTree *tree, int nodeId, uint32 raw)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    *(uint32 *)&SlotFor(slotIndex)->value24 = raw;
}

// 0x080c2342  size=27  FUN_080c2342  status=translated  naming=inferred
// load callback for the "void_loop" clip: forwards to Hunk_AllocAlign with size 4
void *XAnim_VoidLoopLoaded(int size)
{
    return Hunk_AllocAlign((size_t)size, 4);
}

// 0x080c235e  size=523  FUN_080c235e  status=translated  naming=inferred
// sets the loop mode of a node: leaves check their clip's loop flag and load the "void_loop"
// clip when the clip carries the flag; interior nodes validate their first-child chain and
// propagate the mode to the children. Messages: "animation '%s' in '%s' cannot be sync ...",
// "duplicate specification of animation sync in '%s', %d nodes above '%s'"
void XAnim_SetNodeLoopMode(XAnimNodeTable *table, int nodeId, char mode)
{
    XAnimNode *rec = NodeAt(table, nodeId);
    uint16 childCount = rec->childCount;
    int depth;
    int i;

    if (childCount == 0) {
        if ((char)rec->clip->flags02 != mode) {
            if (XAnim_ClipFlag28(rec->clip) == 0) {
                if (mode == 0) {
                    Com_Error(1, "\025animation '%s' in '%s' cannot be sync nonlooping and looping",
                                XAnim_NodeDisplayName(table, nodeId), table->name);
                }
                else {
                    Com_Error(1, "\025animation '%s' in '%s' cannot be sync looping and nonlooping",
                                XAnim_NodeDisplayName(table, nodeId), table->name);
                }
            }
            else {
                XAnim_FindOrLoad("void_loop", XAnim_VoidLoopLoaded);
                rec->clip = XAnim_Find("void_loop");
                if (rec->clip == 0) {
                    Com_Error(1, "\025Cannot find 'xanim/%s'.\nThis is a default xanim file that you should have.\n",
                                "void_loop");
                }
            }
        }
    }
    else {
        if ((rec->clipFlagsLo & 3) != 0) {
            depth = 0;
            do {
                depth = depth + 1;
                rec = NodeAt(table, (int)rec->firstChild);
            } while (rec->childCount != 0);
            Com_Error(1, "\025duplicate specification of animation sync in '%s', %d nodes above '%s'",
                        table->name, depth, XAnim_NodeDisplayName(table, nodeId));
        }
        if (mode == 0) {
            rec->clipFlagsLo = (uint16)(rec->clipFlagsLo | 2);
        }
        else {
            rec->clipFlagsLo = (uint16)(rec->clipFlagsLo | 1);
        }
        for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
            XAnim_SetNodeLoopMode(table, (int)rec->firstChild + i, mode);
        }
    }
}

// 0x080c256a  size=221  FUN_080c256a  status=translated  naming=inferred
// propagates loop flags down the tree: flag 0 recurses, otherwise the node is marked (bit 4)
// and its children are set to loop mode when flag == 1
void XAnim_ApplyLoopModeRec(XAnimNodeTable *table, int nodeId)
{
    XAnimNode *rec = NodeAt(table, nodeId);
    uint32 childCount = rec->childCount;
    uint16 flags;
    int i;

    if (childCount != 0) {
        flags = rec->clipFlagsLo & 3;
        if (flags == 0) {
            for (i = 0; i < (int)childCount; i = i + 1) {
                XAnim_ApplyLoopModeRec(table, (int)rec->firstChild + i);
            }
        }
        else {
            if (flags == 3) {
                Com_Error(1, "\025animation cannot be sync looping and sync nonlooping");
            }
            rec->clipFlagsLo = (uint16)(rec->clipFlagsLo | 4);
            for (i = 0; i < (int)childCount; i = i + 1) {
                XAnim_SetNodeLoopMode(table, (int)rec->firstChild + i, (char)(flags == 1));
            }
        }
    }
}

// 0x080c2648  size=27  FUN_080c2648  status=translated  naming=inferred
// applies the loop flags from the root node
void XAnim_ApplyLoopModeRoot(XAnimNodeTable *table)
{
    XAnim_ApplyLoopModeRec(table, 0);
}

// 0x080c2664  size=65  FUN_080c2664  status=translated  naming=inferred
// 1 when the node is interior or its loop flags are set, 0 for a leaf without flags
int XAnim_NodeHasNoClipOrFlag(XAnimNodeTable *table, int nodeId)
{
    XAnimNode *rec = NodeAt(table, nodeId);
    int result = 0;
    if ((rec->childCount == 0) || ((rec->clipFlagsLo & 3) != 0)) {
        result = 1;
    }
    return result;
}

// 0x080c26a6  size=23  FUN_080c26a6  status=translated  naming=inferred
// 1 when the node has no children
int XAnim_NodeIsLeaf(XAnimNodeTable *table, int nodeId)
{
    return NodeAt(table, nodeId)->childCount == 0;
}

// 0x080c26be  size=130  FUN_080c26be  status=translated  naming=inferred
// resets the timing of the node's slot to a start time: current and previous time set to the
// argument, cycle counters cleared, unk02 set to 0xffff
void XAnim_ResetSlotTimes(XAnimTree *tree, int nodeId, float time)
{
    uint16 slotIndex = tree->slotOfNode[nodeId];
    XAnimSlot *slot;

    if (slotIndex != 0) {
        slot = SlotFor(slotIndex);
        slot->curTime = time;
        slot->cycleCount = 0;
        slot->prevTime = time;
        slot->cycleLimit = 0;
        slot->unk02 = 0xffff;
    }
}

// 0x080c2740  size=100  FUN_080c2740  status=translated  naming=inferred
// recomputes the slot's phase (unk02) from the clip position when the current time is not 1.0;
// a child-linked slot uses the child's key record (node record + 0xc)
void XAnim_UpdateSlotPhase(XAnimTree *tree, XAnimSlot *slot, const short *clipRef)
{
    uint16 phase;

    if (slot->curTime != 1.0f) {
        if (*clipRef != 0) {
            if (slot->childSlot == 0) {
                return;
            }
            clipRef = (const short *)((const char *)tree->table + (uint32)slot->childSlot * 8 + 0xc);
        }
        phase = (uint16)XAnim_FindKeyAtOrAfter((const XAnimNode *)clipRef, slot->curTime);
        slot->unk02 = phase;
    }
}

// 0x080c27a4  size=271  FUN_080c27a4  status=translated  naming=inferred
// releases the slot of the nearest ancestor with a flagged child, then its children's slots
void XAnim_ReleaseNodeSlots(XAnimTree *tree, uint32 nodeId)
{
    uint32 cur = nodeId;
    XAnimNode *rec = 0;
    int cookie;
    uint16 childCount;
    int i;

    for (;;) {
        if (cur == 0) {
            if (NodeAt(tree->table, (int)nodeId)->childCount == 0) {
                XAnim_ResetCursorIfMoved(SlotFor(tree->slotOfNode[nodeId]));
            }
            return;
        }
        rec = NodeAt(tree->table, (int)cur);
        if ((rec->childCount != 0) && ((rec->clipFlagsLo & 4) != 0)) {
            break;
        }
        cur = (uint32)rec->parentIndex;
    }
    cookie = XAnim_ResetCursorIfMoved(SlotFor(tree->slotOfNode[cur]));
    if (cookie == 0) {
        return;
    }
    childCount = rec->childCount;
    for (i = 0; i < (int)(uint32)childCount; i = i + 1) {
        XAnim_ResetCursorRecursive(tree, (int)rec->firstChild + i);
    }
}

// 0x080c28b4  size=123  FUN_080c28b4  status=translated  naming=inferred
// walks up from nodeId while the parent's slot is empty, creating a zero-weight slot with
// full value on each step (unlike XAnim_BlendChain this leaves the weight at zero)
void XAnim_BlendTailChain(XAnimTree *tree, uint32 nodeId, float scale)
{
    uint32 cur = nodeId;

    while ((cur != 0) &&
           (cur = (uint32)NodeAt(tree->table, (int)cur)->parentIndex,
            tree->slotOfNode[cur] == 0)) {
        XAnim_SetSlotWeight(tree, (int)cur, 0.0f, scale, 1.0f, 1, 0, 0);
    }
}

// 0x080c2930  size=150  FUN_080c2930  status=translated  naming=inferred
// blend step: clamps a tiny weight to zero, sets the slot, updates the tail and optionally
// releases the chain
int XAnim_BlendStepC(XAnimTree *tree, int nodeId, float weight, float scale, float p5, int p6, int p7, int doStack)
{
    int result;

    if (weight < 0.001) {
        weight = 0.0f;
    }
    result = XAnim_SetSlotWeight(tree, nodeId, weight, scale, p5, 0, p6, (short)p7);
    XAnim_BlendTailChain(tree, (uint32)nodeId, scale);
    if (doStack != 0) {
        XAnim_ReleaseNodeSlots(tree, (uint32)nodeId);
    }
    return result;
}

// 0x080c29c6  size=33  FUN_080c29c6  status=translated  naming=inferred
// alias of XAnim_SetSlotValue24 with the raw word argument
void XAnim_SetSlotValueAlias(XAnimTree *tree, int nodeId, uint32 raw)
{
    XAnim_SetSlotValue24(tree, nodeId, raw);
}

// 0x080c29e8  size=66  FUN_080c29e8  status=translated  naming=inferred
// loop flag of a node: the clip byte at +2 for a leaf, otherwise bit 0 of the clip word
uint8 XAnim_NodeLoopFlag(XAnimNodeTable *table, int nodeId)
{
    XAnimNode *rec = NodeAt(table, nodeId);
    if (rec->childCount == 0) {
        return rec->clip->flags02;
    }
    return (uint8)(rec->clipFlagsLo & 1);
}

// 0x080c2a2a  size=120  FUN_080c2a2a  status=translated  naming=inferred
// searches the clip's key table (8-byte entries, key in the first halfword) for a key
int XAnim_ClipHasKey(XAnimNodeTable *table, int nodeId, uint32 key)
{
    XAnimParts *clip = NodeAt(table, nodeId)->clip;
    uint16 *keys = clip->keyTable;
    int i;

    if (keys != 0) {
        for (i = 0; i < (int)(uint32)clip->partCount; i = i + 1) {
            if ((uint32)*keys == key) {
                return 1;
            }
            keys = keys + 4;
        }
    }
    return 0;
}

// 0x080c2aa2  size=276  FUN_080c2aa2  status=translated  naming=inferred
// blend step over the parent chain: sets the slot, then for each ancestor with no slot or a
// zero weight applies full weight; releases the chain when requested
void XAnim_BlendStepRec(XAnimTree *tree, uint32 nodeId, float weight, float scale, float p5, int p6, int p7, int doStack)
{
    uint32 cur;
    uint16 slotIndex;

    if (weight < 0.001) {
        weight = 0.0f;
    }
    XAnim_SetSlotWeight(tree, (int)nodeId, weight, scale, p5, 0, p6, (short)p7);
    cur = nodeId;
    while (cur != 0) {
        cur = (uint32)NodeAt(tree->table, (int)cur)->parentIndex;
        slotIndex = tree->slotOfNode[cur];
        if ((slotIndex == 0) ||
            ((SlotFor(slotIndex)->weight == 0.0f) && (SlotFor(slotIndex)->weight == SlotFor(slotIndex)->weight))) {
            XAnim_SetSlotWeight(tree, (int)cur, 1.0f, scale, 1.0f, 0, 0, 0);
        }
    }
    if (doStack != 0) {
        XAnim_ReleaseNodeSlots(tree, nodeId);
    }
}

// 0x080c2bb6  size=97  FUN_080c2bb6  status=translated  naming=inferred
// copies a slot record (10 words) from src to dst and releases the name when present
void XAnim_CopySlot(const XAnimSlot *src, XAnimSlot *dst)
{
    const uint32 *s = (const uint32 *)src;
    uint32 *d = (uint32 *)dst;
    int i;

    for (i = 0; i < 10; i = i + 1) {
        d[i] = s[i];
    }
    if (dst->nameIndex != 0) {
        ScrStr_AddRef((int)dst->nameIndex);
    }
}

// 0x080c2c18  size=242  FUN_080c2c18  status=translated  naming=inferred
// synchronizes the slots of another tree with this tree's slots, node by node
void XAnim_CopyTreeSlots(XAnimTree *tree, XAnimTree *other)
{
    int nodeCount = tree->table->nodeCount;
    int i;
    uint16 srcSlot;
    uint16 dstSlot;
    XAnimSlot *dst;

    for (i = 0; i < nodeCount; i = i + 1) {
        srcSlot = tree->slotOfNode[i];
        if (srcSlot == 0) {
            dstSlot = other->slotOfNode[i];
            if (dstSlot != 0) {
                XAnim_PushFreeSlot((int32)(unsigned long)other, (int)dstSlot);
                other->slotOfNode[i] = 0;
            }
        }
        else {
            if (other->slotOfNode[i] == 0) {
                dst = XAnim_AllocSlot(other, i);
            }
            else {
                dst = SlotFor(other->slotOfNode[i]);
                XAnim_ReleaseSlotHandle(dst);
            }
            XAnim_CopySlot(SlotFor(srcSlot), dst);
        }
    }
}

// 0x080c2d0a  size=95  FUN_080c2d0a  status=translated  naming=inferred
// copies a slot record and resets the copy's link, unk02 and name
void XAnim_InitSlotRecord(const XAnimSlot *src, XAnimSlot *dst)
{
    const uint32 *s = (const uint32 *)src;
    uint32 *d = (uint32 *)dst;
    int i;

    for (i = 0; i < 10; i = i + 1) {
        d[i] = s[i];
    }
    dst->childSlot = 0;
    dst->unk02 = 0xffff;
    dst->nameIndex = 0;
}

// 0x080c2de4  size=23  FUN_080c2de4  status=translated  naming=inferred
// address of blend slot index
XAnimSlot *XAnim_SlotAt(int index)
{
    return &g_xanimSlotRing[index];
}

// 0x080c2dfc  size=166  FUN_080c2dfc  status=translated  naming=inferred
// samples a 2-component key track: an empty track yields its inline value, otherwise the key
// pair around the sampled position is interpolated (4-byte keys, two shorts each)
void XAnim_SamplePart2(const void *ctx, xanim_track_s *part, float weight, float *out)
{
    int index;
    float extra;

    if (part->count == 0) {
        XAnim_MulAddShort2(out, weight, (const short *)((const uint8 *)part + 4), out);
    }
    else {
        XAnim_LocateKeyOrEndBytes((const struct xanim_frame_key_s *)ctx, (const uint8 *)part + 8, (int)part->count, &index, &extra);
        XAnim_LerpPair(out, weight, (const short *)(part->dataPtr + index * 4),
                       (const short *)(part->dataPtr + index * 4 + 4), extra, out);
    }
}

// 0x080c2ea2  size=166  FUN_080c2ea2  status=translated  naming=inferred
// samples a 4-component key track (quaternion, 8-byte keys)
void XAnim_SamplePart4(const void *ctx, xanim_track_s *part, float weight, float *out)
{
    int index;
    float extra;

    if (part->count == 0) {
        XAnim_MulAddShort4(out, weight, (const short *)((const uint8 *)part + 4), out);
    }
    else {
        XAnim_LocateKeyOrEndBytes((const struct xanim_frame_key_s *)ctx, (const uint8 *)part + 8, (int)part->count, &index, &extra);
        XAnim_LerpQuat(out, weight, (const short *)(part->dataPtr + index * 8),
                       (const short *)(part->dataPtr + index * 8 + 8), extra, out);
    }
}

// 0x080c2f48  size=178  FUN_080c2f48  status=translated  naming=inferred
// samples a 3-component float track (12-byte keys)
void XAnim_SamplePart3(const void *ctx, xanim_track_s *part, float weight, float *out)
{
    int index;
    float extra;

    if (part->count == 0) {
        XAnim_Vec3MulAdd(out, weight, (const float *)((const uint8 *)part + 4), out);
    }
    else {
        XAnim_LocateKeyOrEndBytes((const struct xanim_frame_key_s *)ctx, (const uint8 *)part + 8, (int)part->count, &index, &extra);
        Vec3_AddScaledLerp(out, weight, (const float *)(part->dataPtr + index * 0xc), (const float *)(part->dataPtr + index * 0xc + 0xc),
                    extra, out);
    }
}

// 0x080c2ffa  size=374  FUN_080c2ffa  status=translated  naming=inferred
// blends the bone channels of a track into the pose: for each enabled bone not covered by the
// mask, rotation and position channels are sampled with weight and the bone timer advances.
// Weights are scaled by 1/32767 (0x38000100), the short-to-float factor of the key data.
void XAnim_BlendBoneChannels(uint16 *track, const uint8 *boneIdx, float ctxArg, float weight, uint8 *pose, const uint32 *mask)
{
    uint8 scratch[16];
    XAnimChannel *channels;
    const uint8 *bits;
    int count;
    int i;
    uint32 boneId;
    float stepWeight;
    XAnimPoseBone *bone;
    XAnimChannel *channel;

    XAnim_MakeFrameKey(ctxArg, (int32)*track, (xanim_frame_key_s *)scratch);
    stepWeight = weight * 3.0518509e-05f;
    bits = *(const uint8 **)((const uint8 *)track + 0x14);
    count = (int)*(const int16 *)((const uint8 *)track + 0x0e);
    channels = *(XAnimChannel **)((const uint8 *)track + 0x18);
    for (i = 0; i < count; i = i + 1) {
        boneId = (uint32)boneIdx[i];
        if (((mask[boneId >> 5] >> (boneIdx[i] & 0x1f)) & 1u) == 0) {
            bone = (XAnimPoseBone *)(pose + boneId * 0x20);
            channel = channels + i;
            if (((bits[i >> 3] >> (i & 7)) & 1) == 0) {
                XAnim_SamplePart4(scratch, channel->rotPart, stepWeight, bone->rot);
            }
            else if (channel->rotPart == 0) {
                bone->rot[3] = bone->rot[3] + weight;
            }
            else {
                XAnim_SamplePart2(scratch, channel->rotPart, stepWeight, &bone->rot[2]);
            }
            if (channel->posPart != 0) {
                XAnim_SamplePart3(scratch, channel->posPart, weight, bone->pos);
            }
            bone->timer = bone->timer + weight;
        }
    }
}

// 0x080c3170  size=166  FUN_080c3170  status=translated  naming=inferred
// B variant of XAnim_SamplePart2 that resolves its keys through the second lookup (XAnim_LocateKeyOrEndWords)
void XAnim_SamplePart2B(const void *ctx, xanim_track_s *part, float weight, float *out)
{
    int index;
    float extra;

    if (part->count == 0) {
        XAnim_MulAddShort2(out, weight, (const short *)((const uint8 *)part + 4), out);
    }
    else {
        XAnim_LocateKeyOrEndWords((const struct xanim_frame_key_s *)ctx, (const uint16 *)((uint8 *)part + 8), (int)part->count, &index, &extra);
        XAnim_LerpPair(out, weight, (const short *)(part->dataPtr + index * 4),
                       (const short *)(part->dataPtr + index * 4 + 4), extra, out);
    }
}

// 0x080c3216  size=166  FUN_080c3216  status=translated  naming=inferred
// B variant of XAnim_SamplePart4 (XAnim_LocateKeyOrEndWords lookup)
void XAnim_SamplePart4B(const void *ctx, xanim_track_s *part, float weight, float *out)
{
    int index;
    float extra;

    if (part->count == 0) {
        XAnim_MulAddShort4(out, weight, (const short *)((const uint8 *)part + 4), out);
    }
    else {
        XAnim_LocateKeyOrEndWords((const struct xanim_frame_key_s *)ctx, (const uint16 *)((uint8 *)part + 8), (int)part->count, &index, &extra);
        XAnim_LerpQuat(out, weight, (const short *)(part->dataPtr + index * 8),
                       (const short *)(part->dataPtr + index * 8 + 8), extra, out);
    }
}

// 0x080c32bc  size=178  FUN_080c32bc  status=translated  naming=inferred
// B variant of XAnim_SamplePart3 (XAnim_LocateKeyOrEndWords lookup)
void XAnim_SamplePart3B(const void *ctx, xanim_track_s *part, float weight, float *out)
{
    int index;
    float extra;

    if (part->count == 0) {
        XAnim_Vec3MulAdd(out, weight, (const float *)((const uint8 *)part + 4), out);
    }
    else {
        XAnim_LocateKeyOrEndWords((const struct xanim_frame_key_s *)ctx, (const uint16 *)((uint8 *)part + 8), (int)part->count, &index, &extra);
        Vec3_AddScaledLerp(out, weight, (const float *)(part->dataPtr + index * 0xc), (const float *)(part->dataPtr + index * 0xc + 0xc),
                    extra, out);
    }
}

// 0x080c336e  size=374  FUN_080c336e  status=translated  naming=inferred
// B variant of XAnim_BlendBoneChannels: same bone walk and channel rules, using the B samplers
void XAnim_BlendBoneChannelsB(uint16 *track, const uint8 *boneIdx, float ctxArg, float weight, uint8 *pose, const uint32 *mask)
{
    uint8 scratch[16];
    XAnimChannel *channels;
    const uint8 *bits;
    int count;
    int i;
    uint32 boneId;
    float stepWeight;
    XAnimPoseBone *bone;
    XAnimChannel *channel;

    XAnim_MakeFrameKey(ctxArg, (int32)*track, (xanim_frame_key_s *)scratch);
    stepWeight = weight * 3.0518509e-05f;
    bits = *(const uint8 **)((const uint8 *)track + 0x14);
    count = (int)*(const int16 *)((const uint8 *)track + 0x0e);
    channels = *(XAnimChannel **)((const uint8 *)track + 0x18);
    for (i = 0; i < count; i = i + 1) {
        boneId = (uint32)boneIdx[i];
        if (((mask[boneId >> 5] >> (boneIdx[i] & 0x1f)) & 1u) == 0) {
            bone = (XAnimPoseBone *)(pose + boneId * 0x20);
            channel = channels + i;
            if (((bits[i >> 3] >> (i & 7)) & 1) == 0) {
                XAnim_SamplePart4B(scratch, channel->rotPart, stepWeight, bone->rot);
            }
            else if (channel->rotPart == 0) {
                bone->rot[3] = bone->rot[3] + weight;
            }
            else {
                XAnim_SamplePart2B(scratch, channel->rotPart, stepWeight, &bone->rot[2]);
            }
            if (channel->posPart != 0) {
                XAnim_SamplePart3B(scratch, channel->posPart, weight, bone->pos);
            }
            bone->timer = bone->timer + weight;
        }
    }
}

// 0x080c34e4  size=25  FUN_080c34e4  status=translated  naming=inferred
// absolute value, returned in x87 long double
long double XAnim_Abs(float v)
{
    return (long double)fabsf(v);
}

// 0x080c34fe  size=99  FUN_080c34fe  status=translated  naming=inferred
// reciprocal square root by the integer bit trick (magic 0x5f3759df, arithmetic shift) and one
// Newton step; intermediate float stores are kept, the final value is returned in x87 long double
long double XAnim_RSqrt(float v)
{
    float half;
    uint32 bits;
    float guess;
    float result;
    long double refined;

    half = v * 0.5f;
    memcpy(&bits, &v, sizeof bits);
    bits = 0x5f3759dfu - (uint32)(((int32)bits) >> 1);
    memcpy(&guess, &bits, sizeof guess);
    refined = 1.5L - (long double)half * (long double)guess * (long double)guess;
    result = (float)((long double)guess * refined);
    return (long double)result;
}

// 0x080c3562  size=28  FUN_080c3562  status=translated  naming=inferred
// zeroes a 2-component vector
void XAnim_Vec2Zero(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
}

// 0x080c357e  size=31  FUN_080c357e  status=translated  naming=inferred
// copies a 2-component vector from src to dst
void XAnim_Vec2Copy(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
}

// 0x080c359e  size=37  FUN_080c359e  status=translated  naming=inferred
// scales a 2-component vector: out = s * a
void XAnim_Vec2Scale(const float *a, float s, float *out)
{
    out[0] = s * a[0];
    out[1] = s * a[1];
}

// 0x080c35c4  size=54  FUN_080c35c4  status=translated  naming=inferred
// out = a + s * b for 2-component vectors
void XAnim_Vec2MulAdd(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
}

// 0x080c35fa  size=33  FUN_080c35fa  status=translated  naming=inferred
// squared length of a 2-component vector in long double
long double XAnim_Vec2LenSq(const float *v)
{
    return (long double)v[1] * (long double)v[1] + (long double)v[0] * (long double)v[0];
}

// 0x080c361c  size=41  FUN_080c361c  status=translated  naming=inferred
// zeroes a 3-component vector
void XAnim_Vec3Zero(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x080c3646  size=47  FUN_080c3646  status=translated  naming=inferred
// copies a 3-component vector from src to dst
void XAnim_Vec3Copy(const float *src, float *dst)
{
    /* integer dword moves in the original (no x87 round trip, signaling NaN payloads survive) */
    ((volatile uint32 *)dst)[0] = ((const volatile uint32 *)src)[0];
    ((volatile uint32 *)dst)[1] = ((const volatile uint32 *)src)[1];
    ((volatile uint32 *)dst)[2] = ((const volatile uint32 *)src)[2];
}

// 0x080c3676  size=68  FUN_080c3676  status=translated  naming=inferred
// out = a + b for 3-component vectors
void XAnim_Vec3Add(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x080c36ba  size=68  FUN_080c36ba  status=translated  naming=inferred
// out = a - b for 3-component vectors
void XAnim_Vec3Sub(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080c36fe  size=106  FUN_080c36fe  status=translated  naming=inferred
// linear interpolation of 3-component vectors: out = a + (b - a) * t
void XAnim_Vec3Lerp(const float *a, const float *b, float t, float *out)
{
    out[0] = a[0] + (b[0] - a[0]) * t;
    out[1] = a[1] + (b[1] - a[1]) * t;
    out[2] = a[2] + (b[2] - a[2]) * t;
}

// 0x080c3768  size=56  FUN_080c3768  status=translated  naming=inferred
// scales a 3-component vector: out = s * a
void XAnim_Vec3Scale(const float *a, float s, float *out)
{
    out[0] = s * a[0];
    out[1] = s * a[1];
    out[2] = s * a[2];
}

// 0x080c37a0  size=83  FUN_080c37a0  status=translated  naming=inferred
// out = a + s * b for 3-component vectors
void XAnim_Vec3MulAdd(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x080c37f4  size=54  FUN_080c37f4  status=translated  naming=inferred
// zeroes a 4-component vector
void XAnim_Vec4Zero(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    v[3] = 0.0f;
}

// 0x080c382a  size=75  FUN_080c382a  status=translated  naming=inferred
// scales a 4-component vector: out = s * a
void XAnim_Vec4Scale(const float *a, float s, float *out)
{
    out[0] = s * a[0];
    out[1] = s * a[1];
    out[2] = s * a[2];
    out[3] = s * a[3];
}

// 0x080c3876  size=112  FUN_080c3876  status=translated  naming=inferred
// out = a + s * b for 4-component vectors
void XAnim_Vec4MulAdd(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
    out[3] = a[3] + s * b[3];
}

// 0x080c38e6  size=69  FUN_080c38e6  status=translated  naming=inferred
// squared length of a 4-component vector in long double (summed in the binary's order)
long double XAnim_Vec4LenSq(const float *v)
{
    return (long double)v[3] * (long double)v[3] + (long double)v[2] * (long double)v[2] +
           (long double)v[1] * (long double)v[1] + (long double)v[0] * (long double)v[0];
}

// 0x080c392c  size=89  FUN_080c392c  status=translated  naming=inferred
// dequantizes two shorts with the 3-component helper Lerp_Short into a 2-float result
void XAnim_DequantPair(const short *a, const short *b, float p, float *out)
{
    out[0] = (float)Lerp_Short((int)a[0], (int)b[0], p);
    out[1] = (float)Lerp_Short((int)a[1], (int)b[1], p);
}

// 0x080c3986  size=119  FUN_080c3986  status=translated  naming=inferred
// out = a + s * lerp(b, c) for two components, lerp through Lerp_Short
void XAnim_LerpPair(const float *a, float s, const short *b, const short *c, float p, float *out)
{
    long double lerp;

    lerp = (long double)Lerp_Short((int)b[0], (int)c[0], p);
    out[0] = (float)((long double)a[0] + lerp * (long double)s);
    lerp = (long double)Lerp_Short((int)b[1], (int)c[1], p);
    out[1] = (float)((long double)a[1] + lerp * (long double)s);
}

// 0x080c39fe  size=235  FUN_080c39fe  status=translated  naming=inferred
// out = a + s * lerp(b, c) for four components
void XAnim_LerpQuat(const float *a, float s, const short *b, const short *c, float p, float *out)
{
    long double lerp;

    lerp = (long double)Lerp_Short((int)b[0], (int)c[0], p);
    out[0] = (float)((long double)a[0] + lerp * (long double)s);
    lerp = (long double)Lerp_Short((int)b[1], (int)c[1], p);
    out[1] = (float)((long double)a[1] + lerp * (long double)s);
    lerp = (long double)Lerp_Short((int)b[2], (int)c[2], p);
    out[2] = (float)((long double)a[2] + lerp * (long double)s);
    lerp = (long double)Lerp_Short((int)b[3], (int)c[3], p);
    out[3] = (float)((long double)a[3] + lerp * (long double)s);
}

// 0x080c3aea  size=54  FUN_080c3aea  status=translated  naming=inferred
// out = a + s * b for a 2-component short vector
void XAnim_MulAddShort2(const float *a, float s, const short *b, float *out)
{
    out[0] = a[0] + (float)b[0] * s;
    out[1] = a[1] + (float)b[1] * s;
}

// 0x080c3b20  size=112  FUN_080c3b20  status=translated  naming=inferred
// out = a + s * b for a 4-component short vector
void XAnim_MulAddShort4(const float *a, float s, const short *b, float *out)
{
    out[0] = a[0] + (float)b[0] * s;
    out[1] = a[1] + (float)b[1] * s;
    out[2] = a[2] + (float)b[2] * s;
    out[3] = a[3] + (float)b[3] * s;
}

// 0x080c3b90  size=83  FUN_080c3b90  status=translated  naming=inferred
// dequantizes a 4-component short vector: out = s * b
void XAnim_DequantVec4(const short *a, float s, float *out)
{
    out[0] = s * (float)a[0];
    out[1] = s * (float)a[1];
    out[2] = s * (float)a[2];
    out[3] = s * (float)a[3];
}

/*@@NEXT@@*/
