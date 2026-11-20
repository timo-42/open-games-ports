#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_054_types.h"

// 0x081240d8  size=66  FUN_081240d8  status=translated  naming=inferred
// FxPool_Init: loop over 64 blocks of 0x8000 stamping the -1 'unused' marker at +0x7ff0 of the pool at 0x08853380; called once at startup
void FxPool_Init()
{
    unsigned int off;
    FxPoolBlock *blk = g_fxPoolBlocks;
    for (off = 0; off < FX_POOL_BYTES; off += FX_POOL_BLOCK_SIZE) {
        blk->freeCount = -1;
        blk = blk + 1;
    }
}

// 0x0812411a  size=271  FUN_0812411a  status=translated  naming=observed
// FxPool_AllocBlock: string '^1Out of effects memory!\n' printed via Com_DPrintf when no block is free
FxPoolBlock *FxPool_AllocBlock(FxPoolBlock *prev, int count, int elemSize)
{
    unsigned int off = 0;
    FxPoolBlock *blk = g_fxPoolBlocks;
    int i;
    for (;;) {
        if (0x1fffff < off) {
            Com_DPrintf("^1Out of effects memory!\n");
            return 0;
        }
        if (blk->freeCount < 0)
            break;
        blk = blk + 1;
        off += FX_POOL_BLOCK_SIZE;
    }
    if (g_fxPoolHighWater < (int)(off + FX_POOL_BLOCK_SIZE))
        g_fxPoolHighWater = off + FX_POOL_BLOCK_SIZE;
    blk->freeCount = count;
    for (i = 0; i < count - 1; i++)
        *(void **)((byte *)blk + i * elemSize) = (byte *)blk + (i + 1) * elemSize;
    *(void **)((byte *)blk + i * elemSize) = 0;
    blk->freeHead = blk;
    if (prev != 0)
        prev->next = blk;
    blk->prev = prev;
    blk->next = 0;
    return blk;
}

// 0x0812422a  size=103  FUN_0812422a  status=translated  naming=inferred
// FxPool_UnlinkBlock: doubly linked list unlink of the prev/next links at +0x7ff8/+0x7ffc, paired with FxPool_AllocBlock
void FxPool_UnlinkBlock(FxPoolBlock *blk)
{
    if (blk->next != 0)
        blk->next->prev = blk->prev;
    if (blk->prev != 0)
        blk->prev->next = blk->next;
    blk->next = 0;
    blk->prev = 0;
}

// 0x08124292  size=32  FUN_08124292  status=translated  naming=inferred
// FxPool_FreeBlock: unlinks the block then writes the -1 'unused' marker, the inverse of FxPool_AllocBlock
void FxPool_FreeBlock(FxPoolBlock *blk)
{
    FxPool_UnlinkBlock(blk);
    blk->freeCount = -1;
}

// 0x081242b2  size=87  FUN_081242b2  status=translated  naming=inferred
// FxPool_FreeElement: masks the element address to its 0x8000 block in the 0x08853380 pool and pushes it on that block's free list
FxPoolBlock *FxPool_FreeElement(void **elem)
{
    unsigned int off = ((unsigned int)elem - (unsigned int)g_fxPoolBlocks) & 0xffff8000u;
    FxPoolBlock *blk = (FxPoolBlock *)((byte *)g_fxPoolBlocks + off);
    *elem = blk->freeHead;
    blk->freeHead = elem;
    blk->freeCount = blk->freeCount + 1;
    return blk;
}

// 0x0812430c  size=122  FUN_0812430c  status=translated  naming=inferred
// Color_FloatsToBytes3: clamps (int)(f*255.0f) to 0..255 for 3 channels; truncating fistp (control word 0xc00)
void Color_FloatsToBytes3(const float *rgb, byte *out)
{
    int i;
    for (i = 0; i < 3; i++) {
        int v = FxClampIntSelect((int)(rgb[i] * 255.0f), 0, 0xff);
        out[i] = (byte)v;
    }
}

// 0x08124386  size=137  FUN_08124386  status=partial  naming=inferred
// FxBinding_FindOrCreate: walks the g_fxBindingList list for a matching key pair, else allocates a 0x44 record; C++ EH cleanup of the allocation not modelled
FxRef *FxBinding_FindOrCreate(FxRef *out, const FxBindingKey *key)
{
    FxBinding *b = g_fxBindingList;
    for (;;) {
        if (b == 0) {
            FxBinding *created = (FxBinding *)FxPool44_Alloc(0x44);
            FxBinding_ConstructBase(created, key);   /* original has an EH handler that frees on throw */
            FxRef_ConstructFromRaw(out, created);
            return out;
        }
        if (b->key0 == key->key0 && b->key1 == key->key1)
            break;
        b = b->next;
    }
    FxRef_ConstructFromRaw(out, b);
    return out;
}

// 0x08124442  size=85  FUN_08124442  status=translated  naming=inferred
// FxBinding_Release: decrements refCount at +0, unlinks from the g_fxBindingList list and frees the record on zero
void FxBinding_Release(FxBinding *b)
{
    FxBinding **pp;
    b->refCount = b->refCount - 1;
    if (b->refCount == 0) {
        for (pp = &g_fxBindingList; *pp != 0; pp = &(*pp)->next) {
            if (*pp == b) {
                *pp = b->next;
                break;
            }
        }
        FxPool44_Free(b);
    }
}

// 0x08124498  size=10  FUN_08124498  status=translated  naming=unknown
// FxBinding_ResolveFrame: always returns 0; callers treat the result as a frame pointer (Effect::ResolveBinding, Particle::GetStartVelocity)
const float *FxBinding_ResolveFrame(FxBinding *b)
{
    return 0;
}

// 0x081244a2  size=62  FUN_081244a2  status=translated  naming=inferred
// FxBinding_Construct: initialises refCount/field04, stores the key pair at +0x3c/+0x40 and pushes onto g_fxBindingList (C1 variant)
void FxBinding_Construct(FxBinding *b, const FxBindingKey *key)
{
    b->refCount = 0;
    b->field04 = 0;
    b->key0 = key->key0;
    b->key1 = key->key1;
    b->next = g_fxBindingList;
    g_fxBindingList = b;
}

// 0x081244e0  size=62  FUN_081244e0  status=translated  naming=inferred
// FxBinding_ConstructBase: identical body to FxBinding_Construct (C2 base-object variant)
void FxBinding_ConstructBase(FxBinding *b, const FxBindingKey *key)
{
    b->refCount = 0;
    b->field04 = 0;
    b->key0 = key->key0;
    b->key1 = key->key1;
    b->next = g_fxBindingList;
    g_fxBindingList = b;
}

// 0x0812451e  size=101  FUN_0812451e  status=partial  naming=inferred
// FxRef_Archive: writes -1 or the key pair when saving; reads the key pair and re-acquires via FxBinding_FindOrCreate when loading; EH cleanup of temporary not modelled
void FxRef_Archive(FxRef *ref, FxArchive *ar)
{
    if (FxArchive_IsReading(ar) == 0) {
        if (ref->ptr == 0) {
            FxStream_WriteInt(ar, -1);
        } else {
            FxStream_WriteInt(ar, ref->ptr->key0);
            FxStream_WriteInt(ar, ref->ptr->key1);
        }
    } else {
        FxBindingKey key;
        FxRef tmp;
        key.key0 = FxStream_ReadInt(ar);
        if (key.key0 < 0) {
            FxRef_AssignRaw(ref, 0);
        } else {
            key.key1 = FxStream_ReadInt(ar);
            FxBinding_FindOrCreate(&tmp, &key);
            FxRef_Assign(ref, &tmp);
            FxRef_Destruct(&tmp);
        }
    }
}

// 0x08124614  size=19  FUN_08124614  status=translated  naming=inferred
// Fx_AllocCallback: thin wrapper over Hunk_Alloc (0x080abf78); only used as the two allocator callbacks of Fx_LoadXModel
void *Fx_AllocCallback(int size)
{
    return Hunk_Alloc(size);
}

// 0x08124628  size=35  FUN_08124628  status=translated  naming=inferred
// Fx_LoadXModel: calls XModel_FindOrLoad(name, alloc, allocColl) with Fx_AllocCallback for both allocators
struct XModel *Fx_LoadXModel(const char *name)
{
    return XModel_FindOrLoad(name, (void *)Fx_AllocCallback, (void *)Fx_AllocCallback);
}

// 0x0812464c  size=33  FUN_0812464c  status=translated  naming=inferred
// Effect_Construct: sets vtable 0x0815f548 (typeinfo '6Effect') and initialises the binding ref at +0xc0 (C1 variant)
void Effect_Construct(Effect *e)
{
    e->vtbl = vtbl_Effect;
    FxRef_Construct(&e->binding);
}

// 0x0812466e  size=33  FUN_0812466e  status=translated  naming=inferred
// Effect_ConstructBase: identical body to Effect_Construct (C2 variant)
void Effect_ConstructBase(Effect *e)
{
    e->vtbl = vtbl_Effect;
    FxRef_Construct(&e->binding);
}

// 0x08124690  size=56  FUN_08124690  status=translated  naming=inferred
// Effect_DestructBase: resets vtable to Effect and releases the binding ref at +0xc0 (D2 variant)
void Effect_DestructBase(Effect *e)
{
    e->vtbl = vtbl_Effect;
    FxRef_Destruct(&e->binding);
}

// 0x081246c8  size=56  FUN_081246c8  status=translated  naming=inferred
// Effect_Destruct: identical body to Effect_DestructBase; vtable slot 0 of Effect (D1 complete destructor)
void Effect_Destruct(Effect *e)
{
    e->vtbl = vtbl_Effect;
    FxRef_Destruct(&e->binding);
}

// 0x08124700  size=56  FUN_08124700  status=translated  naming=inferred
// Effect_DeletingDestruct: vtable slot 1 of Effect: destroys then returns the object to the effects pool via FxPoolC4_Free (D0 deleting variant)
void Effect_DeletingDestruct(Effect *e)
{
    e->vtbl = vtbl_Effect;
    FxRef_Destruct(&e->binding);
    FxPoolC4_Free(e);
}

// 0x08124738  size=5  FUN_08124738  status=translated  naming=unknown
// Effect::OnExpire: empty virtual (vtable 0x0815f548 slot 2)
void Effect::OnExpire()
{
}

// 0x0812473e  size=10  FUN_0812473e  status=translated  naming=unknown
// Effect::IsCulled: virtual returning 0 (vtable slot 4)
bool Effect::IsCulled()
{
    return 0;
}

// 0x08124748  size=5  FUN_08124748  status=translated  naming=unknown
// Effect::Draw: empty virtual (vtable slot 5)
void Effect::Draw()
{
}

// 0x0812474e  size=86  FUN_0812474e  status=translated  naming=inferred
// Effect::ResolveBinding: if the binding ref is set, asks FxBinding_ResolveFrame for a frame; fails when it returns 0; used by Cloud::Update
bool Effect::ResolveBinding(const float **outFrame)
{
    if (FxRef_IsValid(&binding) != 0) {
        FxBinding *b = FxRef_Get(&binding);
        *outFrame = FxBinding_ResolveFrame(b);
        if (*outFrame == 0)
            return 0;
    }
    return 1;
}

// 0x081247a4  size=75  FUN_081247a4  status=translated  naming=inferred
// Effect::GetTraceMask: returns FxSystem+0x80, clamped to <=5 when flag 0x2000000 is set; used as the trace mask by Tail::TraceSegment
int Effect::GetTraceMask()
{
    int mask;
    if ((flags & FXF_CLAMP_TRACE_MASK) == 0)
        mask = g_fxSystem->traceMask;
    else
        mask = FxMinInt(5, g_fxSystem->traceMask);
    return mask;
}

// 0x081247f0  size=163  FUN_081247f0  status=translated  naming=inferred
// Effect::UpdateFraction: frac = (time-startTime)/(endTime-startTime) clamped to [0,1]; false while time < startTime
bool Effect::UpdateFraction()
{
    if (g_fxSystem->time < startTime)
        return 0;
    int duration = FxObj_GetSpan(this);
    frac = (float)(g_fxSystem->time - startTime) / (float)duration;
    if (1.0f < frac)
        frac = 1.0f;
    if (frac < 0.0f)
        frac = 0.0f;
    return 1;
}

// 0x08124894  size=29  FUN_08124894  status=translated  naming=inferred
// Effect::SetTimes: stores startTime (+0xb8) and endTime (+0xbc); duration accessor FxObj_GetSpan returns their difference
void Effect::SetTimes(int start, int end)
{
    startTime = start;
    endTime = end;
}

// 0x081248b2  size=34  FUN_081248b2  status=translated  naming=inferred
// Effect::GetAge: FxSystem time minus startTime
int Effect::GetAge()
{
    return g_fxSystem->time - startTime;
}

// 0x081248d4  size=31  FUN_081248d4  status=translated  naming=inferred
// Effect::SetBinding: assigns the binding smart pointer at +0xc0 (FxRef_Assign = FxRef assignment)
void Effect::SetBinding(FxRef *ref)
{
    FxRef_Assign(&binding, ref);
}

// 0x081248f4  size=10  FUN_081248f4  status=translated  naming=inferred
// Effect::GetTypeId: vtable slot 9 returns a small class id: Effect 0, Particle 1, Line 2, Tail 3, Cylinder 4, OrientedParticle 7, Cloud 12
int Effect::GetTypeId()
{
    return 0;
}

// 0x081248fe  size=395  FUN_081248fe  status=translated  naming=inferred
// Effect::Archive: save/load of the base fields through the FxArchive helpers; on load recomputes cachedHandle from worldOrigin
void Effect::Archive(FxArchive *ar)
{
    FxStream_Vec3(ar, origin);
    FxStream_Int(ar, &startTime);
    FxStream_Int(ar, &endTime);
    FxStream_Int(ar, (int *)&flags);
    FxStream_Int(ar, &field10);
    FxStream_Vec3(ar, mins);
    FxStream_Vec3(ar, maxs);
    MsgStream_TransferStringRef(FXMS(ar), &impactEffectId);
    MsgStream_TransferStringRef(FXMS(ar), &field30);
    MsgStream_TransferBytes(FXMS(ar), &templateId, 0x68);
    MsgStream_TransferStringRef(FXMS(ar), &field34);
    FxStream_Int(ar, &field38);
    MsgStream_TransferScrString(FXMS(ar), &templateId);
    MsgStream_TransferStringIdx(FXMS(ar), &fieldB4);
    FxRef_Archive(&binding, ar);
    if (FxArchive_IsReading(ar) != 0) {
        cachedHandle = FX_AddOriginRef(worldOrigin);
        FxEffect_ResetSortKey(this);
    }
}

// 0x08124a8a  size=5  FUN_08124a8a  status=translated  naming=unknown
// Effect::CopyFromTemplate: empty virtual (vtable slot 11 of Effect)
void Effect::CopyFromTemplate(const FxParticleTemplate *tmpl)
{
}

// 0x08124a90  size=342  FUN_08124a90  status=translated  naming=unknown
// Effect::SubmitToRenderer: fills a zeroed 0x74 byte descriptor from the instance and passes it to FX_RendererSubmit_Stub(FxSystem, desc, fieldB4)
void Effect::SubmitToRenderer(int mode)
{
    FxRenderDesc desc;
    memset(&desc, 0, 0x74);
    desc.mode = mode;
    desc.templateId = templateId;
    desc.angle = angle;
    Mat3_Copy(axes, desc.axes);
    FxVectorCopy(worldOrigin, desc.origin);
    desc.size = size;
    desc.size2 = size2;
    desc.rgba[0] = rgba[0];
    desc.rgba[1] = rgba[1];
    desc.rgba[2] = rgba[2];
    desc.rgba[3] = rgba[3];
    desc.frameIndex = frameIndex;
    desc.prevSize = prevSize;
    FxVectorCopy(prevWorldOrigin, desc.prevOrigin);
    if ((flags & FXF_SUBMIT_MODE_A) != 0)
        desc.flags = desc.flags | 8;
    if ((flags & FXF_SUBMIT_MODE_B) != 0)
        desc.flags = desc.flags | 0x80;
    FX_RendererSubmit_Stub(g_fxSystem, &desc, fieldB4);
}

// 0x08124be6  size=44  FUN_08124be6  status=translated  naming=inferred
// Particle_Construct: Effect ctor then vtable 0x0815f508 (typeinfo '8Particle') and zeroes the offset vec3 at +0xc4 (C1)
void Particle_Construct(Particle *p)
{
    Effect_ConstructBase(p);
    p->vtbl = vtbl_Particle;
    FxVectorClear(p->offset);
}

// 0x08124c12  size=44  FUN_08124c12  status=translated  naming=inferred
// Particle_ConstructBase: identical body to Particle_Construct (C2 variant)
void Particle_ConstructBase(Particle *p)
{
    Effect_ConstructBase(p);
    p->vtbl = vtbl_Particle;
    FxVectorClear(p->offset);
}

// 0x08124c3e  size=51  FUN_08124c3e  status=translated  naming=inferred
// Particle_DestructBase: sets Particle vtable then runs the Effect base destructor (D2)
void Particle_DestructBase(Particle *p)
{
    p->vtbl = vtbl_Particle;
    Effect_DestructBase(p);
}

// 0x08124fd4  size=83  FUN_08124fd4  status=translated  naming=inferred
// Particle::SetBasis: copies three vec3 from the argument (or zeroes them if the address is null) into the basis at +0xd0/+0xdc/+0xe8
void Particle::SetBasis(const float *axes3)
{
    FxVectorCopyOrClear(axes3, &basis[0]);
    FxVectorCopyOrClear(axes3 + 3, &basis[3]);
    FxVectorCopyOrClear(axes3 + 6, &basis[6]);
}

// 0x08125028  size=53  FUN_08125028  status=translated  naming=inferred
// FxCurve_InitFromTemplate: copies the key table pointer and rolls the per-instance scale from the template's [min,max] range
void FxCurve_InitFromTemplate(const FxCurveSrc *src, FxCurve *dst)
{
    IntPair_Init(dst, src->keys);
    dst->scale = FxRange_Random(&src->scaleMin);
}

// 0x0812505e  size=624  FUN_0812505e  status=translated  naming=inferred
// Particle::InitCurves: vtable slot 8 of Particle: initialises the 22 curve slots at +0x144.. from template slots 0-7 and 10-23 (slots 8,9 are class specific)
void Particle::InitCurves(const FxParticleTemplate *tmpl)
{
    FxCurve_InitFromTemplate(&tmpl->curves[0], &colorCurve);
    FxCurve_InitFromTemplate(&tmpl->curves[1], &colorCurveAlt);
    FxCurve_InitFromTemplate(&tmpl->curves[2], &alphaCurve);
    FxCurve_InitFromTemplate(&tmpl->curves[3], &alphaCurveAlt);
    FxCurve_InitFromTemplate(&tmpl->curves[4], &sizeCurve);
    FxCurve_InitFromTemplate(&tmpl->curves[5], &sizeCurveAlt);
    FxCurve_InitFromTemplate(&tmpl->curves[6], &size2Curve);
    FxCurve_InitFromTemplate(&tmpl->curves[7], &size2CurveAlt);
    FxCurve_InitFromTemplate(&tmpl->curves[10], &rotationCurve);
    FxCurve_InitFromTemplate(&tmpl->curves[11], &rotationCurveAlt);
    FxCurve_InitFromTemplate(&tmpl->curves[12], &velCurveX);
    FxCurve_InitFromTemplate(&tmpl->curves[13], &velCurveY);
    FxCurve_InitFromTemplate(&tmpl->curves[14], &velCurveZ);
    FxCurve_InitFromTemplate(&tmpl->curves[15], &velCurveAltX);
    FxCurve_InitFromTemplate(&tmpl->curves[16], &velCurveAltY);
    FxCurve_InitFromTemplate(&tmpl->curves[17], &velCurveAltZ);
    FxCurve_InitFromTemplate(&tmpl->curves[18], &accCurveX);
    FxCurve_InitFromTemplate(&tmpl->curves[19], &accCurveY);
    FxCurve_InitFromTemplate(&tmpl->curves[20], &accCurveZ);
    FxCurve_InitFromTemplate(&tmpl->curves[21], &accCurveAltX);
    FxCurve_InitFromTemplate(&tmpl->curves[22], &accCurveAltY);
    FxCurve_InitFromTemplate(&tmpl->curves[23], &accCurveAltZ);
}

// 0x081252ce  size=45  FUN_081252ce  status=translated  naming=inferred
// Particle::SetVelocityRand: stores a vec3 at +0x12c, the random weights used by the velocity range curves in EvalVelocityTerm
void Particle::SetVelocityRand(float x, float y, float z)
{
    FxVectorSet(velocityRand, x, y, z);
}

// 0x081252fc  size=45  FUN_081252fc  status=translated  naming=inferred
// Particle::SetAccelRand: stores a vec3 at +0x138, the random weights used by the acceleration range curves in EvalAccelTerm
void Particle::SetAccelRand(float x, float y, float z)
{
    FxVectorSet(accelRand, x, y, z);
}

// 0x0812540c  size=125  FUN_0812540c  status=translated  naming=unknown
// Particle::AppendBlocker: vtable slot 7: appends worldOrigin, size squared and (1 - alpha/255) to the 20 byte draw list at 0x08a53d00
void Particle::AppendBlocker()
{
    FxDrawEntry *entry = &g_fxBlockers[g_fxBlockerCount];
    g_fxBlockerCount = g_fxBlockerCount + 1;
    FxVectorCopy(worldOrigin, entry->origin);
    entry->sizeSq = size * size;
    entry->transparency = 1.0f - (float)rgba[3] * 0.003921569f;
}

// 0x0812548a  size=50  FUN_0812548a  status=translated  naming=inferred
// Particle::ClampedRatio: num/den capped at 1.0 (no lower clamp); this is unused
float Particle::ClampedRatio(float num, float den)
{
    float r = num / den;
    if (1.0f < r)
        r = 1.0f;
    return r;
}

// 0x081254bc  size=101  FUN_081254bc  status=translated  naming=inferred
// Particle_ShouldStop: true when the hit normal points up (z>0) and the velocity is slow (|v|^2 < 16); otherwise true only if the trace fraction is exactly 0
bool Particle_ShouldStop(const float *vel, const float *normal, float fraction)
{
    /* written in the positive form so NaN inputs take the fraction test, as the x87 fucompp/ja chain does */
    if (normal[2] > 0.0f && FxVectorLengthSquared(vel) < 16.0f)
        return 1;
    if (fraction == 0.0f)
        return 1;
    return 0;
}

// 0x08125522  size=51  FUN_08125522  status=translated  naming=inferred
// Particle::StopMoving: clears collide/impact-effect bits (0x20,0x800) and sets the stopped bit 0x1000000
void Particle::StopMoving()
{
    flags = flags & 0xfffff7dfu;
    flags = flags | FXF_STOPPED;
}

// 0x08125556  size=298  FUN_08125556  status=translated  naming=inferred
// Particle::HandleImpact: reflects the accumulated offset about the hit normal scaled by +0x100 (bounce), or stops the particle via Particle_ShouldStop
void Particle::HandleImpact(const float *frame, float t, const float *vel, float traceFraction,
                            const float *normal)
{
    float tmp[3];
    float diff[3];
    float n[3];
    float dot;
    if (Particle_ShouldStop(vel, normal, traceFraction) == 0) {
        if (frame == 0)
            FxVectorCopy(normal, n);
        else
            Orient_VecWorldToLocal(frame, normal, n);
        EvalVelocityTotal(t, tmp, frame);
        FxVectorSubtract(tmp, offset, diff);
        FxVectorScale(tmp, bounce, tmp);
        FxVectorSubtract(tmp, diff, offset);
        dot = FxDotProduct(tmp, n);
        FxVectorMA(offset, dot * -2.0f, n, offset);
    } else {
        StopMoving();
    }
}

// 0x08125680  size=83  FUN_08125680  status=translated  naming=inferred
// Trace_DidHit: true when neither solid flag (+0x22,+0x23) is set and fraction != 1.0
bool Trace_DidHit(const FxTrace *tr)
{
    if (tr->startSolid == 0) {
        if (tr->allSolid == 0) {
            if (tr->fraction == 1.0f)
                return 0;
            return 1;
        }
        return 0;
    }
    return 0;
}

// 0x081256d4  size=941  FUN_081256d4  status=translated  naming=inferred
// Particle::UpdateMotion: integrates origin with EvalVelocityTotal, optionally traces the step (flag 0x20), spawns impact effect (0x800), kills (0x400) or bounces via HandleImpact
bool Particle::UpdateMotion(const float *frame)
{
    FxTrace tr;
    float vel[3];
    float newPos[3];
    float a[3];
    float b[3];
    float boxMins[3];
    float boxMaxs[3];
    float hit[3];
    float t;
    if ((flags & FXF_STOPPED) == 0 && g_fxSystem->frameTime != 0) {
        int dt = g_fxSystem->frameTime;
        int age = GetAge();
        int duration = FxObj_GetSpan(this);
        t = ClampedRatio((float)age * 0.001f, (float)duration * 0.001f);
        EvalVelocityTotal(t, vel, frame);
        FxVectorMA(origin, (float)dt * 0.001f, vel, newPos);
        if ((flags & FXF_COLLIDE) != 0) {
            if (frame == 0) {
                FxVectorCopy(origin, a);
                FxVectorCopy(newPos, b);
            } else {
                Orient_TransformPoint(frame, origin, a);
                Orient_TransformPoint(frame, newPos, b);
            }
            if ((flags & FXF_TRACE_BOUNDS) == 0) {
                FX_InitSampleRecord(g_fxSystem, &tr, a, vec3_origin, vec3_origin, b, -1, 1);
            } else {
                FxVectorCopy(mins, boxMins);
                FxVectorCopy(maxs, boxMaxs);
                FX_InitSampleRecord(g_fxSystem, &tr, a, boxMins, boxMaxs, b, -1, 1);
            }
            if (Trace_DidHit(&tr) != 0) {
                if ((flags & FXF_IMPACT_EFFECT) != 0) {
                    VectorLerp(a, b, tr.fraction, hit);
                    FxScheduler_PlayEffect_Forward(g_fxScheduler, FXTEMPLATE(impactEffectId), hit, tr.normal);
                }
                if ((flags & FXF_KILL_ON_IMPACT) == 0) {
                    t = ClampedRatio((float)dt * 0.001f * tr.fraction + (float)age * 0.001f,
                                     (float)duration * 0.001f);
                    HandleImpact(frame, t, vel, tr.fraction, tr.normal);
                    VectorLerp(origin, newPos, tr.fraction, origin);
                    return 1;
                }
                return 0;
            }
        }
        FxVectorCopy(newPos, origin);
    }
    return 1;
}

// 0x08125a82  size=404  FUN_08125a82  status=translated  naming=inferred
// Particle::EvalVelocityTerm: evaluates the three velocity curves (+0x1bc..; range variant with +0x1e0.. and rand +0x12c when flag 0x80000), rotates by basis unless world-space flag 0x200000
void Particle::EvalVelocityTerm(float t, float *out, const float *frame)
{
    float v[3];
    if ((flags & FXF_VELOCITY_RANGE) == 0) {
        v[0] = FxCurve_EvalScaled(&velCurveX, t);
        v[1] = FxCurve_EvalScaled(&velCurveY, t);
        v[2] = FxCurve_EvalScaled(&velCurveZ, t);
    } else {
        v[0] = FxCurve_EvalBlendScaled(&velCurveX, &velCurveAltX, velocityRand[0], t);
        v[1] = FxCurve_EvalBlendScaled(&velCurveY, &velCurveAltY, velocityRand[1], t);
        v[2] = FxCurve_EvalBlendScaled(&velCurveZ, &velCurveAltZ, velocityRand[2], t);
    }
    if ((flags & FXF_VELOCITY_WORLD) == 0) {
        if (frame == 0)
            Mat3_TransformComponents(basis, v[0], v[1], v[2], out);
        else
            FxVectorCopy(v, out);
    } else if (frame == 0) {
        FxVectorCopy(v, out);
    } else {
        Orient_VecWorldToLocal(frame, v, out);
    }
}

// 0x08125c16  size=404  FUN_08125c16  status=translated  naming=inferred
// Particle::EvalAccelTerm: same shape as EvalVelocityTerm over the acceleration curves (+0x204.., alt +0x228.., rand +0x138, flags 0x100000/0x400000)
void Particle::EvalAccelTerm(float t, float *out, const float *frame)
{
    float v[3];
    if ((flags & FXF_ACCEL_RANGE) == 0) {
        v[0] = FxCurve_EvalScaled(&accCurveX, t);
        v[1] = FxCurve_EvalScaled(&accCurveY, t);
        v[2] = FxCurve_EvalScaled(&accCurveZ, t);
    } else {
        v[0] = FxCurve_EvalBlendScaled(&accCurveX, &accCurveAltX, accelRand[0], t);
        v[1] = FxCurve_EvalBlendScaled(&accCurveY, &accCurveAltY, accelRand[1], t);
        v[2] = FxCurve_EvalBlendScaled(&accCurveZ, &accCurveAltZ, accelRand[2], t);
    }
    if ((flags & FXF_ACCEL_WORLD) == 0) {
        if (frame == 0)
            Mat3_TransformComponents(basis, v[0], v[1], v[2], out);
        else
            FxVectorCopy(v, out);
    } else if (frame == 0) {
        FxVectorCopy(v, out);
    } else {
        Orient_VecWorldToLocal(frame, v, out);
    }
}

// 0x08125daa  size=130  FUN_08125daa  status=translated  naming=inferred
// Particle::EvalGravityTerm: gravity vector for t*duration ms from GetGravityVec, rotated into the frame when one is bound
void Particle::EvalGravityTerm(float t, float *out, const float *frame)
{
    float g[3];
    int duration = FxObj_GetSpan(this);
    GetGravityVec((int)((float)duration * t), g);
    if (frame == 0)
        FxVectorCopy(g, out);
    else
        Orient_VecWorldToLocal(frame, g, out);
}

// 0x08125e2c  size=394  FUN_08125e2c  status=translated  naming=inferred
// Particle::EvalVelocityIntegral: integral variant (FxCurveRef_EvalScaled/FxCurveRef_EvalBlendScaled) of the velocity curves scaled by duration/1000
void Particle::EvalVelocityIntegral(float t, float *out)
{
    float v[3];
    if ((flags & FXF_VELOCITY_RANGE) == 0) {
        v[0] = FxCurveRef_EvalScaled(&velCurveX, t);
        v[1] = FxCurveRef_EvalScaled(&velCurveY, t);
        v[2] = FxCurveRef_EvalScaled(&velCurveZ, t);
    } else {
        v[0] = FxCurveRef_EvalBlendScaled(&velCurveX, &velCurveAltX, velocityRand[0], t);
        v[1] = FxCurveRef_EvalBlendScaled(&velCurveY, &velCurveAltY, velocityRand[1], t);
        v[2] = FxCurveRef_EvalBlendScaled(&velCurveZ, &velCurveAltZ, velocityRand[2], t);
    }
    if ((flags & FXF_VELOCITY_WORLD) == 0)
        Mat3_TransformComponents(basis, v[0], v[1], v[2], out);
    else
        FxVectorCopy(v, out);
    int duration = FxObj_GetSpan(this);
    FxVectorScale(out, (float)duration * 0.001f, out);
}

// 0x08125fb6  size=394  FUN_08125fb6  status=translated  naming=inferred
// Particle::EvalAccelIntegral: integral variant of the acceleration curves; the range selector tests 0x80000 (the velocity-range bit), not 0x100000, as in the binary
void Particle::EvalAccelIntegral(float t, float *out)
{
    float v[3];
    if ((flags & FXF_VELOCITY_RANGE) == 0) {
        v[0] = FxCurveRef_EvalScaled(&accCurveX, t);
        v[1] = FxCurveRef_EvalScaled(&accCurveY, t);
        v[2] = FxCurveRef_EvalScaled(&accCurveZ, t);
    } else {
        v[0] = FxCurveRef_EvalBlendScaled(&accCurveX, &accCurveAltX, accelRand[0], t);
        v[1] = FxCurveRef_EvalBlendScaled(&accCurveY, &accCurveAltY, accelRand[1], t);
        v[2] = FxCurveRef_EvalBlendScaled(&accCurveZ, &accCurveAltZ, accelRand[2], t);
    }
    if ((flags & FXF_ACCEL_WORLD) == 0)
        Mat3_TransformComponents(basis, v[0], v[1], v[2], out);
    else
        FxVectorCopy(v, out);
    int duration = FxObj_GetSpan(this);
    FxVectorScale(out, (float)duration * 0.001f, out);
}

// 0x08126140  size=56  FUN_08126140  status=translated  naming=inferred
// Particle::GetGravityVec: (0, 0, ms * gravity(+0xf4) * 0.001)
void Particle::GetGravityVec(int ms, float *out)
{
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = (float)ms * gravity * 0.001f;
}

// 0x08126178  size=218  FUN_08126178  status=translated  naming=inferred
// Particle::EvalDisplacement: sum of velocity integral, acceleration integral and gravity*ms/1000 at ms into the lifetime (t capped at 1)
void Particle::EvalDisplacement(int ms, float *out)
{
    float vint[3];
    float aint[3];
    float g[3];
    float t;
    int duration = FxObj_GetSpan(this);
    if (ms < duration)
        t = (float)ms / (float)duration;
    else
        t = 1.0f;
    EvalVelocityIntegral(t, vint);
    EvalAccelIntegral(t, aint);
    GetGravityVec(ms, g);
    FxVectorScale(g, (float)ms * 0.001f, g);
    FxVectorAdd(vint, aint, out);
    FxVectorAdd(out, g, out);
}

// 0x08126252  size=115  FUN_08126252  status=translated  naming=inferred
// Particle::GetStartVelocity: EvalVelocityTotal at t=0 in the bound frame; zero vector if the binding cannot be resolved
void Particle::GetStartVelocity(float *out)
{
    const float *frame;
    if (FxRef_IsValid(&binding) == 0) {
        frame = 0;
    } else {
        FxBinding *b = FxRef_Get(&binding);
        frame = FxBinding_ResolveFrame(b);
        if (frame == 0) {
            FxVectorClear(out);
            return;
        }
    }
    EvalVelocityTotal(0.0f, out, frame);
}

// 0x081262c6  size=184  FUN_081262c6  status=translated  naming=inferred
// Particle::EvalVelocityTotal: velocity + acceleration + gravity terms plus the accumulated offset at +0xc4; the per-frame motion rate used by UpdateMotion and Emitter
void Particle::EvalVelocityTotal(float t, float *out, const float *frame)
{
    float vel[3];
    float acc[3];
    float grav[3];
    EvalVelocityTerm(t, vel, frame);
    EvalAccelTerm(t, acc, frame);
    EvalGravityTerm(t, grav, frame);
    FxVectorAdd(vel, acc, out);
    FxVectorAdd(out, grav, out);
    FxVectorAdd(out, offset, out);
}

// 0x0812637e  size=132  FUN_0812637e  status=translated  naming=inferred
// Particle::UpdateSize: size (+0x88) from the size curve (+0x174) or its range variant (flag 0x8000, rand +0x120) at frac
void Particle::UpdateSize()
{
    if ((flags & FXF_SIZE_RANGE) == 0)
        size = FxCurve_EvalScaled(&sizeCurve, frac);
    else
        size = FxCurve_EvalBlendScaled(&sizeCurve, &sizeCurveAlt, sizeRand, frac);
}

// 0x08126402  size=160  FUN_08126402  status=translated  naming=inferred
// Particle::UpdateSize2: size2 (+0x8c): copy of size when +0x104 is 0, else the +0x18c curve (range flag 0x10000, rand +0x124)
void Particle::UpdateSize2()
{
    if (useSize2Curve == 0)
        size2 = size;
    else if ((flags & FXF_SIZE2_RANGE) == 0)
        size2 = FxCurve_EvalScaled(&size2Curve, frac);
    else
        size2 = FxCurve_EvalBlendScaled(&size2Curve, &size2CurveAlt, size2Rand, frac);
}

// 0x081264a2  size=142  FUN_081264a2  status=translated  naming=inferred
// Particle::UpdateColor: rgb vec3 curve at +0x144 (range flag 0x2000, rand +0x118) converted to bytes at +0x90
void Particle::UpdateColor()
{
    float color[3];
    if ((flags & FXF_COLOR_RANGE) == 0)
        FxCurveVec3_EvalScaled(&colorCurve, color, frac);
    else
        FxCurveVec3_EvalBlendScaled(&colorCurve, &colorCurveAlt, colorRand, color, frac);
    Color_FloatsToBytes3(color, rgba);
}

// 0x08126530  size=365  FUN_08126530  status=translated  naming=inferred
// Particle::UpdateAlpha: alpha curve at +0x15c (range flag 0x4000, rand +0x11c) clamped to 0..1; scales rgb bytes (alpha=255) or writes alpha only when flag 0x80
void Particle::UpdateAlpha()
{
    float a;
    float c;
    if ((flags & FXF_ALPHA_RANGE) == 0)
        a = FxCurve_EvalScaled(&alphaCurve, frac);
    else
        a = FxCurve_EvalBlendScaled(&alphaCurve, &alphaCurveAlt, alphaRand, frac);
    c = FxClampFloatSelect(a, 0.0f, 1.0f);
    if ((flags & FXF_ALPHA_ONLY) == 0) {
        rgba[0] = (byte)(short)(int)((float)rgba[0] * c);
        rgba[1] = (byte)(short)(int)((float)rgba[1] * c);
        rgba[2] = (byte)(short)(int)((float)rgba[2] * c);
        rgba[3] = 0xff;
    } else {
        rgba[3] = (byte)(short)(int)(c * 255.0f);
    }
}

// 0x0812669e  size=144  FUN_0812669e  status=translated  naming=inferred
// Particle::UpdateRotation: angle (+0x44) += frameTime * 0.01 * rate, rate from the curve at +0x1a4 (range flag 0x40000, rand +0x128)
void Particle::UpdateRotation()
{
    float rate;
    if ((flags & FXF_ROTATION_RANGE) == 0)
        rate = FxCurve_EvalScaled(&rotationCurve, frac);
    else
        rate = FxCurve_EvalBlendScaled(&rotationCurve, &rotationCurveAlt, rotationRand, frac);
    angle = angle + (float)g_fxSystem->frameTime * 0.01f * rate;
}

// 0x0812672e  size=370  FUN_0812672e  status=translated  naming=inferred
// Particle::UpdateFrame: sprite frame index (+0x94) from age * frameRate + frameOffset with wrap (mode 0) or clamp/repeat-count (mode 1) over the frame count of the template; return value follows the decompiled register usage (returns this when no frame computed)
int Particle::UpdateFrame()
{
    int result;
    int count = FX_GetSpriteFrameCount_Stub(g_fxSystem, templateId);
    result = (int)this;
    if (count == 1) {
        frameIndex = 0;
    } else {
        frameIndex = (int)((float)(g_fxSystem->time - startTime) * frameRate) + frameOffset;
        if (loopMode == 0) {
            result = frameIndex;
            if (count <= frameIndex) {
                result = frameIndex;
                frameIndex = result % count;
                result = result / count;
            }
        } else if (loopMode == 1) {
            if (loopCount < 1) {
                result = frameIndex;
                if (count <= result) {
                    result = count - 1;
                    frameIndex = result;
                }
            } else {
                result = frameIndex;
                if (count <= result) {
                    if (frameIndex < (loopCount + 1) * count) {
                        result = frameIndex / count;
                        frameIndex = frameIndex % count;
                    } else {
                        loopCount = 0;
                        result = count - 1;
                        frameIndex = result;
                    }
                }
            }
        }
    }
    return result;
}

// 0x081268a0  size=10  FUN_081268a0  status=translated  naming=inferred
// Particle::GetTypeId: vtable slot 9 of Particle returns 1
int Particle::GetTypeId()
{
    return 1;
}

// 0x081268aa  size=923  FUN_081268aa  status=translated  naming=inferred
// Particle::Archive: Effect::Archive followed by the particle fields in order: vec3 +0xc4/+0xd0/+0xdc/+0xe8, +0x100, +0x104, 22 curves, +0x108.. +0x140
void Particle::Archive(FxArchive *ar)
{
    Effect::Archive(ar);
    FxStream_Vec3(ar, offset);
    FxStream_Vec3(ar, &basis[0]);
    FxStream_Vec3(ar, &basis[3]);
    FxStream_Vec3(ar, &basis[6]);
    FxStream_Float(ar, &bounce);
    FxStream_Byte(ar, &useSize2Curve);
    MsgStream_TransferValue(FXMS(ar), &colorCurve);
    MsgStream_TransferValue(FXMS(ar), &colorCurveAlt);
    MsgStream_TransferValue(FXMS(ar), &alphaCurve);
    MsgStream_TransferValue(FXMS(ar), &alphaCurveAlt);
    MsgStream_TransferValue(FXMS(ar), &sizeCurve);
    MsgStream_TransferValue(FXMS(ar), &sizeCurveAlt);
    MsgStream_TransferValue(FXMS(ar), &size2Curve);
    MsgStream_TransferValue(FXMS(ar), &size2CurveAlt);
    MsgStream_TransferValue(FXMS(ar), &rotationCurve);
    MsgStream_TransferValue(FXMS(ar), &rotationCurveAlt);
    MsgStream_TransferValue(FXMS(ar), &velCurveX);
    MsgStream_TransferValue(FXMS(ar), &velCurveY);
    MsgStream_TransferValue(FXMS(ar), &velCurveZ);
    MsgStream_TransferValue(FXMS(ar), &velCurveAltX);
    MsgStream_TransferValue(FXMS(ar), &velCurveAltY);
    MsgStream_TransferValue(FXMS(ar), &velCurveAltZ);
    MsgStream_TransferValue(FXMS(ar), &accCurveX);
    MsgStream_TransferValue(FXMS(ar), &accCurveY);
    MsgStream_TransferValue(FXMS(ar), &accCurveZ);
    MsgStream_TransferValue(FXMS(ar), &accCurveAltX);
    MsgStream_TransferValue(FXMS(ar), &accCurveAltY);
    MsgStream_TransferValue(FXMS(ar), &accCurveAltZ);
    FxStream_Int(ar, &frameOffset);
    FxStream_Float(ar, &frameRate);
    FxStream_Int(ar, &loopMode);
    FxStream_Int(ar, &loopCount);
    FxStream_Float(ar, &colorRand);
    FxStream_Float(ar, &alphaRand);
    FxStream_Float(ar, &sizeRand);
    FxStream_Float(ar, &size2Rand);
    FxStream_Float(ar, &rotationRand);
    FxStream_Vec3(ar, velocityRand);
    FxStream_Vec3(ar, accelRand);
}

// 0x08126c46  size=422  FUN_08126c46  status=translated  naming=inferred
// Particle::CopyFromTemplate: vtable slot 11: copies only the key table pointers of template curve slots 0-7 and 10-23 into the 22 curves at +0x144
void Particle::CopyFromTemplate(const FxParticleTemplate *tmpl)
{
    Effect::CopyFromTemplate(tmpl);
    colorCurve.keys = tmpl->curves[0].keys;
    colorCurveAlt.keys = tmpl->curves[1].keys;
    alphaCurve.keys = tmpl->curves[2].keys;
    alphaCurveAlt.keys = tmpl->curves[3].keys;
    sizeCurve.keys = tmpl->curves[4].keys;
    sizeCurveAlt.keys = tmpl->curves[5].keys;
    size2Curve.keys = tmpl->curves[6].keys;
    size2CurveAlt.keys = tmpl->curves[7].keys;
    rotationCurve.keys = tmpl->curves[10].keys;
    rotationCurveAlt.keys = tmpl->curves[11].keys;
    velCurveX.keys = tmpl->curves[12].keys;
    velCurveY.keys = tmpl->curves[13].keys;
    velCurveZ.keys = tmpl->curves[14].keys;
    velCurveAltX.keys = tmpl->curves[15].keys;
    velCurveAltY.keys = tmpl->curves[16].keys;
    velCurveAltZ.keys = tmpl->curves[17].keys;
    accCurveX.keys = tmpl->curves[18].keys;
    accCurveY.keys = tmpl->curves[19].keys;
    accCurveZ.keys = tmpl->curves[20].keys;
    accCurveAltX.keys = tmpl->curves[21].keys;
    accCurveAltY.keys = tmpl->curves[22].keys;
    accCurveAltZ.keys = tmpl->curves[23].keys;
}

// 0x08126dec  size=28  FUN_08126dec  status=translated  naming=inferred
// OrientedParticle_Construct: Particle ctor then vtable 0x0815f4c8 (typeinfo '16OrientedParticle')
void OrientedParticle_Construct(OrientedParticle *p)
{
    Particle_Construct(p);
    p->vtbl = vtbl_OrientedParticle;
}

// 0x08126e08  size=28  FUN_08126e08  status=translated  naming=inferred
// OrientedParticle_ConstructBase: identical to OrientedParticle_Construct (C2 variant)
void OrientedParticle_ConstructBase(OrientedParticle *p)
{
    Particle_Construct(p);
    p->vtbl = vtbl_OrientedParticle;
}

// 0x08126e24  size=51  FUN_08126e24  status=translated  naming=inferred
// OrientedParticle_DestructBase: sets the OrientedParticle vtable then runs Particle_DestructBase (D2)
void OrientedParticle_DestructBase(OrientedParticle *p)
{
    p->vtbl = vtbl_OrientedParticle;
    Particle_DestructBase(p);
}

// 0x0812704e  size=10  FUN_0812704e  status=translated  naming=inferred
// OrientedParticle::GetTypeId: vtable slot 9 of OrientedParticle returns 7
int OrientedParticle::GetTypeId()
{
    return 7;
}

// 0x08127058  size=49  FUN_08127058  status=translated  naming=inferred
// OrientedParticle::Archive: Particle::Archive plus vec3 at +0x24c
void OrientedParticle::Archive(FxArchive *ar)
{
    Particle::Archive(ar);
    FxStream_Vec3(ar, extra24c);
}

// 0x081270d8  size=40  FUN_081270d8  status=partial  naming=inferred
// Cloud_Construct: Particle ctor, vtable 0x0815f488 (typeinfo '5Cloud'), then Cloud::InitRandomAxis; EH cleanup of the base on throw not modelled
void Cloud_Construct(Cloud *c)
{
    Particle_Construct(c);
    c->vtbl = vtbl_Cloud;
    c->InitRandomAxis();   /* original has an EH handler that destroys the base on throw */
}

// 0x081271c2  size=5  FUN_081271c2  status=translated  naming=unknown
// Cloud::OnExpire: empty virtual (vtable 0x0815f488 slot 2)
void Cloud::OnExpire()
{
}

// 0x0812722e  size=215  FUN_0812722e  status=translated  naming=inferred
// Cloud::InitRandomAxis: up to 4 tries of a random vec3 in [-1,1]^3 with 0.01<=|v|^2<=1, then normalised; falls back to (1,0,0)
void Cloud::InitRandomAxis()
{
    int attempt = 0;
    for (;;) {
        int i;
        float lenSq;
        if (attempt == 4) {
            axis[0] = 1.0f;
            axis[1] = 0.0f;
            axis[2] = 0.0f;
            return;
        }
        for (i = 0; i != 3; i++)
            axis[i] = Rand_Float(-1.0f, 1.0f);
        lenSq = FxVectorLengthSquared(axis);
        if (0.01f <= lenSq && lenSq <= 1.0f)
            break;
        attempt++;
    }
    Vec3_Normalize(axis);
}

// 0x08127374  size=142  FUN_08127374  status=translated  naming=inferred
// Cloud::UpdateSpeed: when useSpeedCurve is set, speed (+0x258) from the +0x264 curve (range flag 0x20000, rand +0x260)
void Cloud::UpdateSpeed()
{
    if (useSpeedCurve != 0) {
        if ((flags & FXF_SPEED_RANGE) == 0)
            speed = FxCurve_EvalScaled(&speedCurve, frac);
        else
            speed = FxCurve_EvalBlendScaled(&speedCurve, &speedCurveAlt, speedRand, frac);
    }
}

// 0x08127402  size=439  FUN_08127402  status=translated  naming=inferred
// Cloud::Update: vtable slot 3 of Cloud: per-frame update (fraction, binding, motion, size/size2/speed/colour/alpha/rotation, orientation from axis+angle, world position)
bool Cloud::Update()
{
    const float *frame = 0;
    float prevOrigin[3];
    float quat[4];
    if (Effect::UpdateFraction() == 0)
        return 0;
    if (Effect::ResolveBinding(&frame) == 0)
        return 0;
    FxVectorCopy(origin, prevOrigin);
    if (UpdateMotion(frame) == 0)
        return 0;
    UpdateSize();
    UpdateSize2();
    UpdateSpeed();
    UpdateColor();
    UpdateAlpha();
    UpdateRotation();
    Quat_FromAxisAngleDeg(angle, axis, quat);
    Quat_ToMat3(quat, axes);
    prevSize = size;
    size = size2;
    if (useSpeedCurve == 0)
        size2 = size;
    else
        size2 = speed;
    if (frame == 0)
        FxVectorCopy(origin, worldOrigin);
    else
        Orient_TransformPoint(frame, origin, worldOrigin);
    FxVectorCopy(prevOrigin, prevWorldOrigin);
    return 1;
}

// 0x081275ba  size=10  FUN_081275ba  status=translated  naming=inferred
// Cloud::GetTypeId: vtable slot 9 of Cloud returns 12
int Cloud::GetTypeId()
{
    return 12;
}

// 0x081275c4  size=141  FUN_081275c4  status=translated  naming=inferred
// Cloud::Archive: Particle::Archive plus vec3 +0x24c, curves +0x264/+0x270, float +0x260, byte +0x25c
void Cloud::Archive(FxArchive *ar)
{
    Particle::Archive(ar);
    FxStream_Vec3(ar, axis);
    MsgStream_TransferValue(FXMS(ar), &speedCurve);
    MsgStream_TransferValue(FXMS(ar), &speedCurveAlt);
    FxStream_Float(ar, &speedRand);
    FxStream_Byte(ar, &useSpeedCurve);
}

// 0x08127652  size=62  FUN_08127652  status=translated  naming=inferred
// Cloud::CopyFromTemplate: Particle::CopyFromTemplate plus key pointers of template curve slots 8 and 9
void Cloud::CopyFromTemplate(const FxParticleTemplate *tmpl)
{
    Particle::CopyFromTemplate(tmpl);
    speedCurve.keys = tmpl->curves[8].keys;
    speedCurveAlt.keys = tmpl->curves[9].keys;
}

// 0x08127690  size=28  FUN_08127690  status=translated  naming=inferred
// Line_Construct: Particle ctor then vtable 0x0815f448 (typeinfo '4Line')
void Line_Construct(Line *l)
{
    Particle_Construct(l);
    l->vtbl = vtbl_Line;
}

// 0x081276ac  size=28  FUN_081276ac  status=translated  naming=inferred
// Line_ConstructBase: identical to Line_Construct (C2 variant)
void Line_ConstructBase(Line *l)
{
    Particle_Construct(l);
    l->vtbl = vtbl_Line;
}

// 0x081276c8  size=51  FUN_081276c8  status=translated  naming=inferred
// Line_DestructBase: sets the Line vtable then Particle_DestructBase (D2)
void Line_DestructBase(Line *l)
{
    l->vtbl = vtbl_Line;
    Particle_DestructBase(l);
}

// 0x08127764  size=5  FUN_08127764  status=translated  naming=unknown
// Line::OnExpire: empty virtual (vtable 0x0815f448 slot 2)
void Line::OnExpire()
{
}

// 0x081278d4  size=10  FUN_081278d4  status=translated  naming=inferred
// Line::GetTypeId: vtable slot 9 of Line returns 2
int Line::GetTypeId()
{
    return 2;
}

// 0x081278de  size=49  FUN_081278de  status=translated  naming=inferred
// Line::Archive: Particle::Archive plus vec3 at +0x24c
void Line::Archive(FxArchive *ar)
{
    Particle::Archive(ar);
    FxStream_Vec3(ar, extra24c);
}

// 0x08127910  size=28  FUN_08127910  status=translated  naming=inferred
// Tail_Construct: Particle ctor then vtable 0x0815f408 (typeinfo '4Tail')
void Tail_Construct(Tail *t)
{
    Particle_Construct(t);
    t->vtbl = vtbl_Tail;
}

// 0x0812792c  size=28  FUN_0812792c  status=translated  naming=inferred
// Tail_ConstructBase: identical to Tail_Construct (C2 variant)
void Tail_ConstructBase(Tail *t)
{
    Particle_Construct(t);
    t->vtbl = vtbl_Tail;
}

// 0x08127948  size=51  FUN_08127948  status=translated  naming=inferred
// Tail_DestructBase: sets the Tail vtable then Particle_DestructBase (D2)
void Tail_DestructBase(Tail *t)
{
    t->vtbl = vtbl_Tail;
    Particle_DestructBase(t);
}

// 0x081279e4  size=38  FUN_081279e4  status=translated  naming=inferred
// Tail::UpdateLengthAndPoint: calls UpdateLength then UpdateTailPoint with no frame
void Tail::UpdateLengthAndPoint()
{
    UpdateLength();
    UpdateTailPoint(0);
}

// 0x08127a0a  size=87  FUN_08127a0a  status=translated  naming=inferred
// Tail::TraceSegment: vtable slot 4 of Tail: world trace between worldOrigin and prevWorldOrigin with size as radius and GetTraceMask()
bool Tail::TraceSegment()
{
    int mask = GetTraceMask();
    return FX_IsSegmentCulled(g_fxSystem, worldOrigin, prevWorldOrigin, size, size, mask) != 0;
}

// 0x08127bea  size=128  FUN_08127bea  status=translated  naming=inferred
// Tail::UpdateLength: length (+0x258) from the +0x260 curve (range flag 0x20000, rand +0x25c) at frac
void Tail::UpdateLength()
{
    if ((flags & FXF_SPEED_RANGE) == 0)
        length = FxCurve_EvalScaled(&lengthCurve, frac);
    else
        length = FxCurve_EvalBlendScaled(&lengthCurve, &lengthCurveAlt, lengthRand, frac);
}

// 0x08127c6a  size=226  FUN_08127c6a  status=translated  naming=inferred
// Tail::UpdateTailPoint: places prevWorldOrigin at origin + length * unit(endPoint - origin), through the frame when bound
void Tail::UpdateTailPoint(const float *frame)
{
    float dir[3];
    float len;
    FxVectorSubtract(endPoint, origin, dir);
    len = FxVectorLength(dir);
    if (0.0f < len) {
        FxVectorScale(dir, 1.0f / len, dir);
        if (frame == 0) {
            FxVectorMA(origin, length, dir, prevWorldOrigin);
        } else {
            FxVectorMA(origin, length, dir, dir);
            Orient_TransformPoint(frame, dir, prevWorldOrigin);
        }
    }
}

// 0x08127d4c  size=10  FUN_08127d4c  status=translated  naming=inferred
// Tail::GetTypeId: vtable slot 9 of Tail returns 3
int Tail::GetTypeId()
{
    return 3;
}

// 0x08127d56  size=118  FUN_08127d56  status=translated  naming=inferred
// Tail::Archive: Particle::Archive plus vec3 +0x24c, curves +0x260/+0x26c, float +0x25c
void Tail::Archive(FxArchive *ar)
{
    Particle::Archive(ar);
    FxStream_Vec3(ar, endPoint);
    MsgStream_TransferValue(FXMS(ar), &lengthCurve);
    MsgStream_TransferValue(FXMS(ar), &lengthCurveAlt);
    FxStream_Float(ar, &lengthRand);
}

// 0x08127dcc  size=62  FUN_08127dcc  status=translated  naming=inferred
// Tail::CopyFromTemplate: Particle::CopyFromTemplate plus key pointers of template curve slots 8 and 9
void Tail::CopyFromTemplate(const FxParticleTemplate *tmpl)
{
    Particle::CopyFromTemplate(tmpl);
    lengthCurve.keys = tmpl->curves[8].keys;
    lengthCurveAlt.keys = tmpl->curves[9].keys;
}

// 0x08127e0a  size=28  FUN_08127e0a  status=translated  naming=inferred
// Cylinder_Construct: Tail ctor then vtable 0x0815f3c8 (typeinfo '8Cylinder')
void Cylinder_Construct(Cylinder *c)
{
    Tail_Construct(c);
    c->vtbl = vtbl_Cylinder;
}

// 0x08127e26  size=28  FUN_08127e26  status=translated  naming=inferred
// Cylinder_ConstructBase: identical to Cylinder_Construct (C2 variant)
void Cylinder_ConstructBase(Cylinder *c)
{
    Tail_Construct(c);
    c->vtbl = vtbl_Cylinder;
}

// 0x08127e42  size=51  FUN_08127e42  status=translated  naming=inferred
// Cylinder_DestructBase: sets the Cylinder vtable then Tail_DestructBase (D2)
void Cylinder_DestructBase(Cylinder *c)
{
    c->vtbl = vtbl_Cylinder;
    Tail_DestructBase(c);
}

// 0x0812809a  size=10  FUN_0812809a  status=translated  naming=inferred
// Cylinder::GetTypeId: vtable slot 9 of Cylinder returns 4
int Cylinder::GetTypeId()
{
    return 4;
}

// 0x081280a4  size=26  FUN_081280a4  status=translated  naming=inferred
// Cylinder::Archive: forwards to Tail::Archive
void Cylinder::Archive(FxArchive *ar)
{
    Tail::Archive(ar);
}

// 0x081280be  size=28  FUN_081280be  status=translated  naming=inferred
// Emitter_Construct: Particle ctor then vtable 0x0815f388 (typeinfo '7Emitter')
void Emitter_Construct(Emitter *e)
{
    Particle_Construct(e);
    e->vtbl = vtbl_Emitter;
}

// 0x081280da  size=28  FUN_081280da  status=translated  naming=inferred
// Emitter_ConstructBase: identical to Emitter_Construct (C2 variant)
void Emitter_ConstructBase(Emitter *e)
{
    Particle_Construct(e);
    e->vtbl = vtbl_Emitter;
}

// 0x081280f6  size=51  FUN_081280f6  status=translated  naming=inferred
// Emitter_DestructBase: sets the Emitter vtable then Particle_DestructBase (D2)
void Emitter_DestructBase(Emitter *e)
{
    e->vtbl = vtbl_Emitter;
    Particle_DestructBase(e);
}

// 0x08128192  size=10  FUN_08128192  status=translated  naming=unknown
// Emitter::IsCulled: virtual returning 0 (vtable 0x0815f388 slot 4)
bool Emitter::IsCulled()
{
    return 0;
}

// 0x0812819c  size=67  FUN_0812819c  status=translated  naming=unknown
// Emitter::Draw: vtable slot 5: when flag 0x10 is set and prevSize (+0x98) is non-zero, submits a descriptor with mode 1
void Emitter::Draw()
{
    if ((flags & FXF_ON_EXPIRE_SUBMIT) != 0 && prevSize != 0.0f)
        SubmitToRenderer(1);
}

// 0x08128456  size=1471  FUN_08128456  status=translated  naming=inferred
// Emitter::EmitElements: steps a point along EvalVelocityTotal in 12 ms increments until it moves minStep, then spawns effect emitTemplateId at it via FxScheduler_PlayEffect; re-rolls minStep with FxObj29c_RandomizeValue
void Emitter::EmitElements(const float *frameVel, const float *frame)
{
    if ((flags & FXF_EMITTER_ACTIVE) != 0 && g_fxSystem->frameTime != 0) {
        float minStepSq = minStep * minStep;
        int stepMs = 0;
        float elapsedSec = 0.0f;
        int nextTime = nextEmitTime;
        float ageSec = (float)GetAge() * 0.001f;
        float durSec = (float)FxObj_GetSpan(this) * 0.001f;
        float stepSec;
        while (nextTime < g_fxSystem->time) {
            float t;
            float v[3];
            float target[3];
            float p1[3];
            float nextVel[3];
            float mat[9];
            int i;
            stepMs += 12;
            stepSec = (float)stepMs * 0.001f;
            FxVectorAdd(pos, drift, target);
            t = ClampedRatio(ageSec + elapsedSec, durSec);
            EvalVelocityTotal(t, v, frame);
            FxVectorMA(pos, stepSec, v, p1);
            if (frame != 0) {
                for (i = 0; i < 3; i++)
                    p1[i] = p1[i] + stepSec * frameVel[i];
            }
            if (Vec3_DistanceSquared(p1, target) < minStepSq) {
                nextTime += 12;
            } else {
                float speedSq;
                float stepSecNow;
                float denom;
                if (frame == 0) {
                    float len = FxVectorLength(v);
                    if (0.0f < len) {
                        FxVectorScale(v, 1.0f / len, &mat[0]);
                        Vec3_PerpendicularBasis(&mat[0], &mat[3], &mat[6]);
                    } else {
                        FxVectorCopy(&axes[0], &mat[0]);
                        FxVectorCopy(&axes[3], &mat[3]);
                        FxVectorCopy(&axes[6], &mat[6]);
                    }
                    FxScheduler_PlayEffect(g_fxScheduler, FXTEMPLATE(emitTemplateId), p1, mat, 0);
                } else {
                    float local[3];
                    FxBinding *b;
                    Orient_TransformPoint(frame, p1, local);
                    b = FxRef_Get(&binding);
                    FxScheduler_PlayEffect(g_fxScheduler, FXTEMPLATE(emitTemplateId), local, 0, &b->key0);
                }
                speedSq = FxDotProduct(v, v);
                stepSecNow = (float)stepMs * 0.001f;
                denom = (speedSq + speedSq) * stepSecNow;
                if (denom != 0.0f) {
                    int adjust = FxRoundToInt(((speedSq * stepSecNow * stepSecNow - minStepSq) / denom) * 1000.0f);
                    stepMs = stepMs - adjust;
                    if (stepMs < nextTime - nextEmitTime)
                        stepMs = nextTime - nextEmitTime;
                    stepSec = (float)stepMs * 0.001f;
                    t = ClampedRatio(ageSec + elapsedSec + stepSec, durSec);
                    EvalVelocityTotal(t, v, frame);
                    FxVectorMA(pos, stepSec, v, p1);
                }
                t = ClampedRatio(ageSec + elapsedSec + stepSec, durSec);
                EvalVelocityTotal(t, nextVel, frame);
                FxVectorCopy(p1, pos);
                FxVectorCopy(nextVel, vel);
                if (frame != 0)
                    FxVectorMA(drift, (float)stepMs * 0.001f, frameVel, drift);
                nextEmitTime = stepMs + nextEmitTime;
                elapsedSec = elapsedSec + stepSec;
                stepMs = 0;
                nextTime = nextEmitTime;
                FxObj29c_RandomizeValue(this);
                minStepSq = minStep * minStep;
            }
        }
    }
}
