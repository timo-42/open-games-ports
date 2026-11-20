#include <ctype.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_107_types.h"
#include <strings.h>

/*
 * Chunk 107: 87 functions that Ghidra's auto-analysis missed, recovered from .eh_frame FDE starts.
 * Pseudocode lives in .local/cod2/decomp/functions_eh/<addr>.c. Every range was checked against
 * objdump: each body ends in a ret, no branch leaves the range, and the next instruction is a
 * function prologue or alignment padding.
 *
 * Naming evidence summary
 *  - Script entity methods and trigger spawn functions: the method table (0x0815e3a0) and the
 *    spawn table (0x08188a18) list the script/class name string next to the function address.
 *    The functions do not reference those name strings themselves, so naming is "inferred".
 *  - Fx class members: vtables at 0x0815f308..0x0815f548 carry RTTI typeinfo names
 *    (8Particle, 16OrientedParticle, 5Cloud, 4Line, 4Tail, 8Cylinder, 7Emitter, 5Light, 5Flash);
 *    each function here is a vtable slot (0 dtor, 1 deleting dtor, 2 spawn-effect, 3 update,
 *    4 culling test, 5 draw, 6 ray visibility, 8 curve init) or a base-dtor/ctor variant.
 *  - G_PickTarget: the literal "G_PickTarget: target %s not found\n" is referenced here.
 */

/* Scr_ReleaseAnyValue (0x08084bb2) is ScrStr_TextFromHandle(Scr_GetAny(p)) (verified in the disassembly). */
static const char *Scr_GetTextParam107(uint32 p)
{
    return Scr_ReleaseAnyValue(p);
}

/* Resolves a script entity reference to a script mover entity (script_brushmodel/script_model/
 * script_origin) exactly as the solid/notsolid bodies do; the original keeps going with a NULL
 * entity when the "not an entity" error returns. */
static gentity_t *Scr_GetMoverEntity107(uint32 self)
{
    gentity_t *ent;
    if ((self >> 16) == 0) {
        ent = &g_gentities[self & 0xffff];
        if (ent->classname != scrStr_script_brushmodel && ent->classname != scr_str_script_model &&
            ent->classname != scr_str_script_origin) {
            char *msg = va("entity %i is not a script_brushmodel, script_model, or script_origin",
                           (int)(uint16)self);
            Scr_ErrorNoParam(msg);
        }
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = 0;
    }
    return ent;
}

/* ============================================================== game module */

// 0x0811a004  size=258  FUN_0811a004  status=translated  naming=inferred
// GScr_Solid: script entity method "solid" (method table entry 0x0815e41c -> this address).
// Evidence: strings "entity %i is not a script_brushmodel, script_model, or script_origin" and "cannot use the solid/notsolid commands on a script_origin entity( number %i )\n" referenced here.
void GScr_Solid(uint32 self)
{
    gentity_t *ent = Scr_GetMoverEntity107(self);

    if (ent->classname == scr_str_script_origin) {
        Com_DPrintf("cannot use the solid/notsolid commands on a script_origin entity( number %i )\n",
                    ent->s.number);
    } else {
        if (ent->classname == scr_str_script_model) {
            ent->r.contents = 0x2080;
        } else {
            ent->r.contents = 1;
            ent->s.eFlags = ent->s.eFlags & 0xfffffffe;
        }
        SV_LinkEntity((struct gentity_s *)ent);
    }
}

// 0x0811a106  size=243  FUN_0811a106  status=translated  naming=inferred
// GScr_NotSolid: script entity method "notsolid" (method table entry 0x0815e428 -> this address); same strings as GScr_Solid.
void GScr_NotSolid(uint32 self)
{
    gentity_t *ent = Scr_GetMoverEntity107(self);

    if (ent->classname == scr_str_script_origin) {
        Com_DPrintf("cannot use the solid/notsolid commands on a script_origin entity( number %i )\n",
                    ent->s.number);
    } else {
        ent->r.contents = 0;
        if (ent->classname != scr_str_script_model) {
            ent->s.eFlags = ent->s.eFlags | 1;
        }
        SV_LinkEntity((struct gentity_s *)ent);
    }
}

// 0x0811a994  size=38  FUN_0811a994  status=translated  naming=unknown
// Scr_CopyEntityRefChildren: thunk over Scr_CopyClassEntityChildren(src, dst, 0) taking both ids through pointers; no string, no table entry.
void Scr_CopyEntityRefChildren(uint32 *dst, uint32 *src)
{
    Scr_CopyClassEntityChildren(*src, *dst, 0);
}

// 0x0811b228  size=91  FUN_0811b228  status=translated  naming=inferred
// Scr_GetHudElemParam: resolves a script reference of class 1 to the hud element pool entry (stride 0x8c at 0x08628f80); otherwise error "not a hudelem".
void *Scr_GetHudElemParam(uint32 param)
{
    uint32 ref;
    void *elem;

    Scr_GetEntity(&ref, param);
    if ((ref >> 16) == 1) {
        elem = g_hudelems + (ref & 0xffff) * 0x8c;
    } else {
        Scr_ErrorAtParam(param, "not a hudelem");
        elem = 0;
    }
    return elem;
}

// 0x0811b312  size=252  FUN_0811b312  status=translated  naming=inferred
// GScr_GetEnt: script function "getent" (builtin table entry 0x0818808c -> this address); string "getent used with more than one entity" referenced here.
// Scans the entity array for an in-use entity whose script-string field (named by param 1, type 3 in the field table) equals param 0.
void GScr_GetEnt(void)
{
    short key = (short)Scr_GetAny(0);
    const char *fieldName = Scr_GetTextParam107(1);
    int field = Scr_FindClassFieldOffset(0, fieldName);

    if (field >= 0 && ((entfield107_t *)g_entityScriptFieldTable)[field].type == 3) {
        gentity_t *found = 0;
        gentity_t *ent = g_gentities;
        int i;
        for (i = 0; i < level_numEntities; i++) {
            if (ent->r.inuse != 0 &&
                *(short *)((byte *)ent + ((entfield107_t *)g_entityScriptFieldTable)[field].ofs) != 0 &&
                *(short *)((byte *)ent + ((entfield107_t *)g_entityScriptFieldTable)[field].ofs) == key) {
                if (found != 0) {
                    Scr_Error("getent used with more than one entity");
                }
                found = ent;
            }
            ent++;
        }
        if (found != 0) {
            Scr_AddEntityParam((struct gentity_s *)found);
        }
    }
}

// 0x0811b40e  size=312  FUN_0811b40e  status=translated  naming=inferred
// GScr_GetEntArray: script function "getentarray" (builtin table entry 0x08188098 -> this address).
// With no parameters returns every in-use entity; otherwise those whose script-string field matches (same lookup as GScr_GetEnt).
void GScr_GetEntArray(void)
{
    int i;
    gentity_t *ent;

    if (Scr_GetNumParams() == 0) {
        Scr_AddEntity();
        ent = g_gentities;
        for (i = 0; i < level_numEntities; i++) {
            if (ent->r.inuse != 0) {
                Scr_AddEntityParam((struct gentity_s *)ent);
                Scr_PopAndLink();
            }
            ent++;
        }
    } else {
        short key = (short)Scr_GetAny(0);
        const char *fieldName = Scr_GetTextParam107(1);
        int field = Scr_FindClassFieldOffset(0, fieldName);

        if (field >= 0 && ((entfield107_t *)g_entityScriptFieldTable)[field].type == 3) {
            Scr_AddEntity();
            ent = g_gentities;
            for (i = 0; i < level_numEntities; i++) {
                if (ent->r.inuse != 0 &&
                    *(short *)((byte *)ent + ((entfield107_t *)g_entityScriptFieldTable)[field].ofs) != 0 &&
                    *(short *)((byte *)ent + ((entfield107_t *)g_entityScriptFieldTable)[field].ofs) == key) {
                    Scr_AddEntityParam((struct gentity_s *)ent);
                    Scr_PopAndLink();
                }
                ent++;
            }
        }
    }
}

// 0x0811c66e  size=44  FUN_0811c66e  status=translated  naming=inferred
// Trigger_Touch: entClass table slot +0x0c (touch) of entClass 1 (0x08187d34 -> this address): queues the entity pair event then refreshes if flag 0x10.
void Trigger_Touch(gentity_t *self, gentity_t *other)
{
    G_QueueEntityPairEvent((struct gentity_s *)self, (struct gentity_s *)other);
    Ent_RefreshIfFlag10((struct gentity_s *)self);
}

// 0x0811c69a  size=70  FUN_0811c69a  status=translated  naming=inferred
// SP_trigger_multiple: spawn table (0x08188a18 "trigger_multiple" -> 0x0811c69a).
void SP_trigger_multiple(gentity_t *ent)
{
    ent->handler = 1;
    Ent_AddFlagIfCondition((struct gentity_s *)ent, 0x10);
    G_InitBodyEntity((struct gentity_s *)ent);
    Ent_UpdateContentsFromFlags((struct gentity_s *)ent);
    SV_LinkEntity((struct gentity_s *)ent);
}

// 0x0811c6e0  size=416  FUN_0811c6e0  status=translated  naming=inferred
// SP_trigger_radius: spawn table (0x08188a20 "trigger_radius" -> 0x0811c6e0); strings "USAGE: spawn( \"trigger_radius\", <origin>, <spawnflags>, <radius>, <height> )" and "radius not specified for trigger_radius at (%g %g %g)".
// With spawn vars (g_spawnVars set) the radius/height come from the map keys, otherwise from script params 3 and 4.
void SP_trigger_radius(gentity_t *ent)
{
    float radius;
    float height;

    if (*(byte *)&g_spawnVars == 0) {
        uint32 numParams = (uint32)Scr_GetNumParams();
        if (numParams < 5) {
            Scr_Error("USAGE: spawn( \"trigger_radius\", <origin>, <spawnflags>, <radius>, <height> )");
        }
        radius = Scr_GetFloat(3);
        height = Scr_GetFloat(4);
    } else {
        if (G_SpawnFloat("radius", "", &radius) == 0) {
            char *msg = va("radius not specified for trigger_radius at (%g %g %g)",
                           (double)ent->r.currentOrigin[0], (double)ent->r.currentOrigin[1],
                           (double)ent->r.currentOrigin[2]);
            Com_Error(1, msg);
        }
        if (G_SpawnFloat("height", "", &height) == 0) {
            char *msg = va("height not specified for trigger_radius at (%g %g %g)",
                           (double)ent->r.currentOrigin[0], (double)ent->r.currentOrigin[1],
                           (double)ent->r.currentOrigin[2]);
            Com_Error(1, msg);
        }
    }
    ent->handler = 1;
    ent->r.mins[0] = -radius;
    ent->r.mins[1] = -radius;
    ent->r.mins[2] = 0.0f;
    ent->r.maxs[0] = radius;
    ent->r.maxs[1] = radius;
    ent->r.maxs[2] = height;
    ent->r.svFlags = 0x21;
    Ent_AddFlagIfCondition((struct gentity_s *)ent, 0x10);
    Ent_UpdateContentsFromFlags((struct gentity_s *)ent);
    SV_LinkEntity((struct gentity_s *)ent);
}

// 0x0811c880  size=267  FUN_0811c880  status=translated  naming=inferred
// SP_trigger_disk: spawn table (0x08188a28 "trigger_disk" -> 0x0811c880); reads only "radius" (error string shared with trigger_radius), pads it by 64 and spans z in [-100000, 100000].
void SP_trigger_disk(gentity_t *ent)
{
    float radius;

    if (G_SpawnFloat("radius", "", &radius) == 0) {
        char *msg = va("radius not specified for trigger_radius at (%g %g %g)",
                       (double)ent->r.currentOrigin[0], (double)ent->r.currentOrigin[1],
                       (double)ent->r.currentOrigin[2]);
        Com_Error(1, msg);
    }
    ent->handler = 1;
    radius = 64.0f + radius;
    ent->r.mins[0] = -radius;
    ent->r.mins[1] = -radius;
    ent->r.mins[2] = -100000.0f;
    ent->r.maxs[0] = radius;
    ent->r.maxs[1] = radius;
    ent->r.maxs[2] = 100000.0f;
    ent->r.svFlags = 0x41;
    Ent_AddFlagIfCondition((struct gentity_s *)ent, 0x10);
    Ent_UpdateContentsFromFlags((struct gentity_s *)ent);
    SV_LinkEntity((struct gentity_s *)ent);
}

// 0x0811c98c  size=262  FUN_0811c98c  status=translated  naming=inferred
// Trigger_HurtTouch: entClass table slot +0x0c (touch) of entClass 3 (0x08187d84 -> this address); damages the touching entity with mod 13 (no direction/point) and re-arms after 50 ms, or 1000 ms when spawnflag 0x10 is set.
void Trigger_HurtTouch(gentity_t *self, gentity_t *other)
{
    if (other->takedamage != 0 && self->u.trigger.timestamp <= g_0864c56c) {
        int dflags;

        G_QueueEntityPairEvent((struct gentity_s *)self, (struct gentity_s *)other);
        if ((self->spawnflags & 0x10) == 0) {
            self->u.trigger.timestamp = g_0864c56c + 0x32;
        } else {
            self->u.trigger.timestamp = g_0864c56c + 1000;
        }
        if ((self->spawnflags & 8) == 0) {
            dflags = 0;
        } else {
            dflags = 0x10;
        }
        G_Damage((struct gentity_s *)other, (struct gentity_s *)self, (struct gentity_s *)self,
                 0, 0, self->damage, dflags, 0xd, 0, 0);
        if ((self->spawnflags & 0x20) != 0) {
            self->handler = 2;
        }
    }
}

// 0x0811caba  size=124  FUN_0811caba  status=translated  naming=inferred
// SP_trigger_hurt: spawn table (0x08188a30 "trigger_hurt" -> 0x0811caba); string keys "sound"/"world_hurt_me"; default damage 5; entClass 2 when spawnflag 1 is set, else 3.
void SP_trigger_hurt(gentity_t *ent)
{
    const char *sound;

    G_InitBodyEntity((struct gentity_s *)ent);
    G_SpawnString("sound", "world_hurt_me", &sound);
    if (ent->damage == 0) {
        ent->damage = 5;
    }
    ent->r.contents = 0x405c0008;
    if (((byte)ent->spawnflags ^ 1) & 1) {
        ent->handler = 3;
    } else {
        ent->handler = 2;
    }
}

// 0x0811cb36  size=72  FUN_0811cb36  status=translated  naming=inferred
// SP_trigger_once: spawn table (0x08188a38 "trigger_once" -> 0x0811cb36); forces spawnflag 0x10.
void SP_trigger_once(gentity_t *ent)
{
    ent->handler = 1;
    ent->spawnflags = ent->spawnflags | 0x10;
    G_InitBodyEntity((struct gentity_s *)ent);
    Ent_UpdateContentsFromFlags((struct gentity_s *)ent);
    SV_LinkEntity((struct gentity_s *)ent);
}

// 0x0811cdd4  size=65  FUN_0811cdd4  status=translated  naming=inferred
// Trigger_DamageDie: entClass table slot +0x18 of entClass 4 (0x08187db8 -> this address), signature (self, inflictor, attacker, damage, mod); forwards to Ent_TryConsumeCharge and keeps health at 32000 unless "accumulate" is set.
void Trigger_DamageDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
    Ent_TryConsumeCharge((struct gentity_s *)self, (struct gentity_s *)attacker, damage, mod);
    if (self->u.trigger.accumulate == 0) {
        self->health = 32000;
    }
}

// 0x0811ce16  size=146  FUN_0811ce16  status=translated  naming=inferred
// SP_trigger_damage: spawn table (0x08188a78 "trigger_damage" -> 0x0811ce16); spawn keys "accumulate" and "threshold" (default "0"), health 32000, entClass 4.
void SP_trigger_damage(gentity_t *ent)
{
    G_SpawnInt("accumulate", "0", &ent->u.trigger.accumulate);
    G_SpawnInt("threshold", "0", &ent->u.trigger.threshold);
    ent->health = 32000;
    ent->takedamage = 1;
    ent->handler = 4;
    Ent_AddFlagIfCondition((struct gentity_s *)ent, 0x200);
    G_InitBodyEntity((struct gentity_s *)ent);
    SV_LinkEntity((struct gentity_s *)ent);
}

// 0x0811e0d6  size=66  FUN_0811e0d6  status=translated  naming=unknown
// Ent_ConcatAttachTransform: builds the entity's attach transform (Ent_GetAttachTransform) and concatenates it with the matrix at attachLink+0x40; no string evidence.
void Ent_ConcatAttachTransform(gentity_t *ent, float *out)
{
    byte *link = (byte *)ent->tagInfo;
    byte orient[56];

    Ent_GetAttachTransform((struct gentity_s *)ent, (struct game_orient_s *)orient);
    Mat34_ConcatRowVector((const float *)(link + 0x40), (const float *)orient, out);
}

// 0x0811e78a  size=210  FUN_0811e78a  status=translated  naming=observed
// G_PickTarget: string "G_PickTarget: target %s not found\n" is referenced here. Collects up to 32 entities whose targetname (+0x16c) matches, returns one at random (rand() % count) or NULL (0 handle returns NULL without a message).
gentity_t *G_PickTarget(uint16 targetname)
{
    gentity_t *found[0x20];
    gentity_t *cur = 0;
    int count = 0;

    if (targetname == 0) {
        return 0;
    }
    do {
        cur = (gentity_t *)G_FindEntityByShortField((struct gentity_s *)cur, 0x16c,
                                                       (int16)targetname);
        if (cur == 0) {
            break;
        }
        found[count] = cur;
        count++;
    } while (count != 0x20);
    if (count == 0) {
        const char *text = ScrStr_TextFromHandle(targetname);
        Com_Printf("G_PickTarget: target %s not found\n", text);
        return 0;
    }
    return found[rand() % count];
}

// 0x0811f0b4  size=327  FUN_0811f0b4  status=translated  naming=unknown
// G_DamageClientsInBounds: offsets the entity's mins/maxs by its client's origin, queries entities in that box (mask 0x2000000, max 0x400) and applies G_Damage(100000, dflags 0x10, mod 10) to every linked entity that has a client.
void G_DamageClientsInBounds(gentity_t *ent)
{
    float absmin[7];
    float absmax[4];
    int list[0x400 + 2];
    int count;
    int i;

    Vec3_Add_0811f5bc((const float *)(ent->client + 0x14), ent->r.mins, absmin);
    Vec3_Add_0811f5bc((const float *)(ent->client + 0x14), ent->r.maxs, absmax);
    count = CM_AreaEntities(absmin, absmax, list, 0x400, 0x2000000);
    for (i = 0; i < count; i++) {
        gentity_t *hit = &g_gentities[list[i]];
        if (hit->client != 0 && hit->r.linked != 0) {
            G_Damage((struct gentity_s *)hit, (struct gentity_s *)ent, (struct gentity_s *)ent,
                     0, 0, 100000, 0x10, 10, 0, 0);
        }
    }
}

// 0x0811f2f6  size=55  FUN_0811f2f6  status=translated  naming=unknown
// G_AddTempEntityEventB3: when value is non-zero spawns a temp entity of event 0xb3 at origin and stores value in its eventParm (+0xa0).
void G_AddTempEntityEventB3(const float *origin, uint8 value)
{
    if (value != 0) {
        byte *tent = (byte *)G_TempEntity(origin, 0xb3);
        *(unsigned int *)(tent + 0xa0) = (unsigned int)value;
    }
}

// 0x0811f38e  size=55  FUN_0811f38e  status=translated  naming=unknown
// Ent_AddAnimEventB3: looks up an animation index by the name pointed to by *animName and forwards it to Ent_AddEventB3IfNonzero for the entity with the given number; returns 0.
int Ent_AddAnimEventB3(int entnum, const char **animName)
{
    uint8 anim = G_SoundIndex(*animName);
    Ent_AddEventB3IfNonzero((struct gentity_s *)&g_gentities[entnum], (int8)anim);
    return 0;
}

/* ============================================================== effects (Fx) */

// 0x081236cc  size=53  FUN_081236cc  status=translated  naming=unknown
// FxCurveSrc_Copy: copies a 0x0c byte curve source (key table through the identity function Int_Identity_081240d0, then scale min/max).
void FxCurveSrc_Copy(const struct FxCurveSrc *src, struct FxCurveSrc *dst)
{
    dst->keys = (void *)Int_Identity_081240d0((int)src->keys);
    /* the original copies the min/max pair as two raw dwords (no x87 round trip) */
    *(volatile unsigned int *)&dst->scaleMin = *(const volatile unsigned int *)&src->scaleMin;
    *(volatile unsigned int *)&dst->scaleMax = *(const volatile unsigned int *)&src->scaleMax;
}

// 0x08124c72  size=51  FUN_08124c72  status=translated  naming=inferred
// Particle_Destruct: vtable 0x0815f508 slot 0 (RTTI 8Particle): sets the Particle vtable and runs the Effect base destructor.
void Particle_Destruct(struct Particle *p)
{
    p->vtbl = vtbl_Particle;
    Effect_DestructBase(p);
}

// 0x08124ca6  size=51  FUN_08124ca6  status=translated  naming=inferred
// Particle_DeletingDestruct: vtable 0x0815f508 slot 1: destructor followed by the return of the element to pool 0x08a533a0 (0x24c).
void Particle_DeletingDestruct(struct Particle *p)
{
    p->vtbl = vtbl_Particle;
    Effect_DestructBase(p);
    FxPool24C_Free(p);
}

// 0x08124cda  size=312  FUN_08124cda  status=translated  naming=inferred
// Particle::PlayRandomDirEffect: vtable slot 2 shared by Particle/OrientedParticle/Tail/Cylinder/Emitter. If flags has 0x200, not 0x400 and an effect template is set, plays it at the origin along a random unit direction (axis (0,0,1) when the vector is shorter than 1e-6). Random call order is z, y, x.
void Particle::PlayRandomDirEffect()
{
    float dir[4];
    float rz, ry, rx;
    float len;

    if ((flags & 0x200) != 0 && (flags & 0x400) == 0 && effectTemplate != 0) {
        rz = Rand_Float(-1.0f, 1.0f);
        ry = Rand_Float(-1.0f, 1.0f);
        rx = Rand_Float(-1.0f, 1.0f);
        FxVectorSet(dir, rx, ry, rz);
        len = FxVectorLength(dir);
        if ((double)len < 1e-06) {
            FxVectorSet(dir, 0.0f, 0.0f, 1.0f);
        } else {
            FxVectorScale(dir, (float)(1.0 / len), dir);
        }
        FxScheduler_PlayEffect_Forward(g_fxScheduler, (struct FxEffectTemplate *)effectTemplate,
                                       origin, dir);
    }
}

// 0x08124e12  size=62  FUN_08124e12  status=translated  naming=inferred
// Particle::IsCulled: vtable slot 4; tests the sphere (worldOrigin, size) against the view planes with the effect's trace mask.
byte Particle::IsCulled()
{
    int mask = GetTraceMask();
    return (byte)FX_IsCulledByPlanes((uint32)(size_t)g_fxSystem, worldOrigin, size, mask);
}

// 0x08124e50  size=71  FUN_08124e50  status=translated  naming=inferred
// Particle::Draw: vtable slot 5; submits in mode 4 only when both size and size2 are non-zero.
void Particle::Draw()
{
    if (size != 0.0f && size2 != 0.0f) {
        SubmitToRenderer(4);
    }
}

// 0x08124e98  size=315  FUN_08124e98  status=translated  naming=inferred
// Particle::Update: vtable slot 3; fraction, binding, motion; world position from the binding frame; then size, size2 (each early-returns success when zero), colour, alpha, rotation, frame.
bool Particle::Update()
{
    const float *frame = 0;

    if (!UpdateFraction()) {
        return false;
    }
    if (!ResolveBinding(&frame)) {
        return false;
    }
    if (!UpdateMotion(frame)) {
        return false;
    }
    if (frame == 0) {
        FxVectorCopy(origin, worldOrigin);
    } else {
        Orient_TransformPoint(frame, origin, worldOrigin);
    }
    UpdateSize();
    if (size == 0.0f) {
        return true;
    }
    UpdateSize2();
    if (size2 == 0.0f) {
        return true;
    }
    UpdateColor();
    UpdateAlpha();
    UpdateRotation();
    UpdateFrame();
    return true;
}

// 0x0812532a  size=226  FUN_0812532a  status=translated  naming=unknown
// Particle::GetRayVisibility: vtable slot 6, called from Fx_GetRayAttenuation. Projects the effect onto a ray; returns 1.0, or 1 - rgba[3]/255 when the ray passes within `size` of the effect centre (|t - radius| <= radius required).
float Particle::GetRayVisibility(const float *rayOrigin, const float *rayDir, float radius)
{
    float result;
    float nearest[4];
    float delta[6];
    float t;

    FxVectorSubtract(worldOrigin, rayOrigin, delta);
    t = FxDotProduct(delta, rayDir);
    if (radius < FxFabs(t - radius)) {
        result = 1.0f;
    } else {
        FxVectorMA(rayOrigin, t, rayDir, nearest);
        if (Vec3_DistanceSquared(worldOrigin, nearest) < size * size) {
            result = 1.0f - 0.003921569f * (float)rgba[3];
        } else {
            result = 1.0f;
        }
    }
    return result;
}

// 0x08126e58  size=51  FUN_08126e58  status=translated  naming=inferred
// OrientedParticle_Destruct: vtable 0x0815f4c8 slot 0 (RTTI 16OrientedParticle): sets its vtable then the Particle base destructor.
void OrientedParticle_Destruct(struct OrientedParticle *p)
{
    p->vtbl = vtbl_OrientedParticle;
    Particle_DestructBase(p);
}

// 0x08126e8c  size=51  FUN_08126e8c  status=translated  naming=inferred
// OrientedParticle_DeletingDestruct: slot 1; returns the element to pool 0x08a533b0 (0x258, FxPool258b).
void OrientedParticle_DeletingDestruct(struct OrientedParticle *p)
{
    p->vtbl = vtbl_OrientedParticle;
    Particle_DestructBase(p);
    FxPool258b_Free(p);
}

// 0x08126ec0  size=62  FUN_08126ec0  status=translated  naming=inferred
// OrientedParticle::IsCulled: slot 4; identical body to Particle::IsCulled.
byte OrientedParticle::IsCulled()
{
    int mask = GetTraceMask();
    return (byte)FX_IsCulledByPlanes((uint32)(size_t)g_fxSystem, worldOrigin, size, mask);
}

// 0x08126efe  size=27  FUN_08126efe  status=translated  naming=inferred
// OrientedParticle::Draw: slot 5; submits in mode 7.
void OrientedParticle::Draw()
{
    SubmitToRenderer(7);
}

// 0x08126f1a  size=308  FUN_08126f1a  status=translated  naming=inferred
// OrientedParticle::Update: slot 3; like Particle::Update but runs every curve update unconditionally and finally maps the local axis at +0x24c into axes[0..2] (+0x48) through the binding frame.
bool OrientedParticle::Update()
{
    const float *frame = 0;

    if (!UpdateFraction()) {
        return false;
    }
    if (!ResolveBinding(&frame)) {
        return false;
    }
    if (!UpdateMotion(frame)) {
        return false;
    }
    UpdateSize();
    UpdateSize2();
    UpdateColor();
    UpdateAlpha();
    UpdateRotation();
    UpdateFrame();
    if (frame == 0) {
        FxVectorCopy(origin, worldOrigin);
        FxVectorCopy(extra24c, axes);
    } else {
        Orient_TransformPoint(frame, origin, worldOrigin);
        Orient_VecLocalToWorld((const struct Orient *)frame, extra24c, axes);
    }
    return true;
}

// 0x0812708a  size=77  FUN_0812708a  status=partial  naming=inferred
// Cloud_ConstructBase: Particle ctor, Cloud vtable 0x0815f488 (RTTI 5Cloud), then Cloud::InitRandomAxis. Partial: the EH landing pad at 0x081270b2 (Particle_DestructBase(c), then _Unwind_Resume) is not modelled because the shared prototype header already declares __cxa_begin_catch with a conflicting exception specifier.
void Cloud_ConstructBase(struct Cloud *c)
{
    Particle_Construct(c);
    c->vtbl = vtbl_Cloud;
    c->InitRandomAxis();   /* EH landing pad not modelled: original calls Particle_DestructBase(c) then _Unwind_Resume */
}

// 0x08127126  size=51  FUN_08127126  status=translated  naming=inferred
// Cloud_DestructBase: sets the Cloud vtable then Particle_DestructBase (D2 variant).
void Cloud_DestructBase(struct Cloud *c)
{
    c->vtbl = vtbl_Cloud;
    Particle_DestructBase(c);
}

// 0x0812715a  size=51  FUN_0812715a  status=translated  naming=inferred
// Cloud_Destruct: vtable 0x0815f488 slot 0 (D1 variant, same body as Cloud_DestructBase).
void Cloud_Destruct(struct Cloud *c)
{
    c->vtbl = vtbl_Cloud;
    Particle_DestructBase(c);
}

// 0x0812718e  size=51  FUN_0812718e  status=translated  naming=inferred
// Cloud_DeletingDestruct: slot 1; returns the element to pool 0x08a533d0 (0x27c).
void Cloud_DeletingDestruct(struct Cloud *c)
{
    c->vtbl = vtbl_Cloud;
    Particle_DestructBase(c);
    FxPool27C_Free(c);
}

// 0x081271c8  size=101  FUN_081271c8  status=translated  naming=inferred
// Cloud::IsCulled: slot 4; culling sphere radius is max(size, size2) + prevSize (+0x98).
byte Cloud::IsCulled()
{
    int mask = GetTraceMask();
    float radius = FxMaxFloat(size, size2) + prevSize;
    return (byte)FX_IsCulledByPlanes((uint32)(size_t)g_fxSystem, worldOrigin, radius, mask);
}

// 0x08127306  size=27  FUN_08127306  status=translated  naming=inferred
// Cloud::Draw: slot 5; submits in mode 6.
void Cloud::Draw()
{
    SubmitToRenderer(6);
}

// 0x08127322  size=82  FUN_08127322  status=translated  naming=inferred
// Cloud::InitCurves: slot 8; Particle::InitCurves then the two speed curves from template curves[8],[9] (+0x160,+0x16c) into +0x264/+0x270.
void Cloud::InitCurves(const FxParticleTemplate *tmpl)
{
    Particle::InitCurves(tmpl);
    FxCurve_InitFromTemplate(&tmpl->curves[8], &speedCurve);
    FxCurve_InitFromTemplate(&tmpl->curves[9], &speedCurveAlt);
}

// 0x081276fc  size=51  FUN_081276fc  status=translated  naming=inferred
// Line_Destruct: vtable 0x0815f448 slot 0 (RTTI 4Line).
void Line_Destruct(struct Line *l)
{
    l->vtbl = vtbl_Line;
    Particle_DestructBase(l);
}

// 0x08127730  size=51  FUN_08127730  status=translated  naming=inferred
// Line_DeletingDestruct: slot 1; returns the element to pool 0x08a533a8 (0x258, FxPool258a).
void Line_DeletingDestruct(struct Line *l)
{
    l->vtbl = vtbl_Line;
    Particle_DestructBase(l);
    FxPool258a_Free(l);
}

// 0x0812776a  size=87  FUN_0812776a  status=translated  naming=inferred
// Line::IsCulled: slot 4; segment (worldOrigin, prevWorldOrigin) with margin `size` on both ends.
byte Line::IsCulled()
{
    int mask = GetTraceMask();
    return (byte)FX_IsSegmentCulled((uint32)(size_t)g_fxSystem, worldOrigin, prevWorldOrigin,
                                    size, size, mask);
}

// 0x081277c2  size=27  FUN_081277c2  status=translated  naming=inferred
// Line::Draw: slot 5; submits in mode 8.
void Line::Draw()
{
    SubmitToRenderer(8);
}

// 0x081277de  size=245  FUN_081277de  status=translated  naming=inferred
// Line::Update: slot 3; no motion step. World start point from the origin and world end point (+0x9c) from the local end point at +0x24c, through the binding frame; then size, colour, alpha.
bool Line::Update()
{
    const float *frame = 0;

    if (!UpdateFraction()) {
        return false;
    }
    if (!ResolveBinding(&frame)) {
        return false;
    }
    if (frame == 0) {
        FxVectorCopy(origin, worldOrigin);
        FxVectorCopy(endPoint, prevWorldOrigin);
    } else {
        Orient_TransformPoint(frame, origin, worldOrigin);
        Orient_TransformPoint(frame, endPoint, prevWorldOrigin);
    }
    UpdateSize();
    UpdateColor();
    UpdateAlpha();
    return true;
}

// 0x0812797c  size=51  FUN_0812797c  status=translated  naming=inferred
// Tail_Destruct: vtable 0x0815f408 slot 0 (RTTI 4Tail).
void Tail_Destruct(struct Tail *t)
{
    t->vtbl = vtbl_Tail;
    Particle_DestructBase(t);
}

// 0x081279b0  size=51  FUN_081279b0  status=translated  naming=inferred
// Tail_DeletingDestruct: slot 1; returns the element to pool 0x08a533b8 (0x278, FxPool278a).
void Tail_DeletingDestruct(struct Tail *t)
{
    t->vtbl = vtbl_Tail;
    Particle_DestructBase(t);
    FxPool278a_Free(t);
}

// 0x08127a62  size=82  FUN_08127a62  status=translated  naming=inferred
// Tail::InitCurves: slot 8 (shared with Cylinder); Particle::InitCurves then the length curves from template curves[8],[9] into +0x260/+0x26c.
void Tail::InitCurves(const FxParticleTemplate *tmpl)
{
    Particle::InitCurves(tmpl);
    FxCurve_InitFromTemplate(&tmpl->curves[8], &lengthCurve);
    FxCurve_InitFromTemplate(&tmpl->curves[9], &lengthCurveAlt);
}

// 0x08127ab4  size=27  FUN_08127ab4  status=translated  naming=inferred
// Tail::Draw: slot 5; submits in mode 8.
void Tail::Draw()
{
    SubmitToRenderer(8);
}

// 0x08127ad0  size=282  FUN_08127ad0  status=translated  naming=inferred
// Tail::Update: slot 3; remembers the previous origin at +0x24c, runs motion, size, tail length, colour, alpha, frame and finally the tail end point.
bool Tail::Update()
{
    const float *frame = 0;

    if (!UpdateFraction()) {
        return false;
    }
    if (!ResolveBinding(&frame)) {
        return false;
    }
    FxVectorCopy(origin, prevOrigin);
    if (!UpdateMotion(frame)) {
        return false;
    }
    if (frame == 0) {
        FxVectorCopy(origin, worldOrigin);
    } else {
        Orient_TransformPoint(frame, origin, worldOrigin);
    }
    UpdateSize();
    UpdateLength();
    UpdateColor();
    UpdateAlpha();
    UpdateFrame();
    UpdateTailPoint(frame);
    return true;
}

// 0x08127e76  size=51  FUN_08127e76  status=translated  naming=inferred
// Cylinder_Destruct: vtable 0x0815f3c8 slot 0 (RTTI 8Cylinder); base destructor is Tail_DestructBase.
void Cylinder_Destruct(struct Cylinder *c)
{
    c->vtbl = vtbl_Cylinder;
    Tail_DestructBase(c);
}

// 0x08127eaa  size=51  FUN_08127eaa  status=translated  naming=inferred
// Cylinder_DeletingDestruct: slot 1; returns the element to pool 0x08a533c0 (0x278, FxPool278b).
void Cylinder_DeletingDestruct(struct Cylinder *c)
{
    c->vtbl = vtbl_Cylinder;
    Tail_DestructBase(c);
    FxPool278b_Free(c);
}

// 0x08127ede  size=87  FUN_08127ede  status=translated  naming=inferred
// Cylinder::IsCulled: slot 4; segment (worldOrigin, prevWorldOrigin) with margins size2 and size.
byte Cylinder::IsCulled()
{
    int mask = GetTraceMask();
    return (byte)FX_IsSegmentCulled((uint32)(size_t)g_fxSystem, worldOrigin, prevWorldOrigin,
                                    size2, size, mask);
}

// 0x08127f36  size=27  FUN_08127f36  status=translated  naming=inferred
// Cylinder::Draw: slot 5; submits in mode 9.
void Cylinder::Draw()
{
    SubmitToRenderer(9);
}

// 0x08127f52  size=327  FUN_08127f52  status=translated  naming=inferred
// Cylinder::Update: slot 3; no motion step. The segment runs from the origin to origin + length * axes[0]; both ends go through the binding frame when present.
bool Cylinder::Update()
{
    const float *frame = 0;
    float endPoint[7];

    if (!UpdateFraction()) {
        return false;
    }
    if (!ResolveBinding(&frame)) {
        return false;
    }
    UpdateSize();
    UpdateSize2();
    UpdateLength();
    UpdateColor();
    UpdateAlpha();
    if (frame == 0) {
        FxVectorCopy(origin, worldOrigin);
        FxVectorMA(origin, length, axes, prevWorldOrigin);
    } else {
        Orient_TransformPoint(frame, origin, worldOrigin);
        FxVectorMA(origin, length, axes, endPoint);
        Orient_TransformPoint(frame, endPoint, prevWorldOrigin);
    }
    return true;
}

// 0x0812812a  size=51  FUN_0812812a  status=translated  naming=inferred
// Emitter_Destruct: vtable 0x0815f388 slot 0 (RTTI 7Emitter).
void Emitter_Destruct(struct Emitter *e)
{
    e->vtbl = vtbl_Emitter;
    Particle_DestructBase(e);
}

// 0x0812815e  size=51  FUN_0812815e  status=translated  naming=inferred
// Emitter_DeletingDestruct: slot 1; returns the element to pool 0x08a533c8 (0x29c, FxPool29C).
void Emitter_DeletingDestruct(struct Emitter *e)
{
    e->vtbl = vtbl_Emitter;
    Particle_DestructBase(e);
    FxPool29C_Free(e);
}

// 0x081281e0  size=630  FUN_081281e0  status=translated  naming=inferred
// Emitter::Update: slot 3; motion step, damps the velocity at +0x284 by 0.7 when the origin did not move, integrates velocity, then (flag 0x10) moves the world position/axes through the binding frame and derives a frame velocity from the frame origin delta since the last emit time. Finally EmitElements.
// Quirk kept: when flag 0x10 is set without a binding frame, frameVel is never initialised before EmitElements receives it.
bool Emitter::Update()
{
    const float *frame = 0;
    float prev[7];
    float frameVel[4];
    float rotated[12];
    int i;

    if (!UpdateFraction()) {
        return false;
    }
    if (!ResolveBinding(&frame)) {
        return false;
    }
    FxVectorCopy(origin, prev);
    if (!UpdateMotion(frame)) {
        return false;
    }
    if ((char)FxVectorCompare(prev, origin) != 0) {
        FxVectorScale(vel, 0.7f, vel);
    }
    FxObj29c_IntegrateVelocity((struct Emitter *)this);
    UpdateSize();
    if (frame != 0) {
        FxVectorClear(frameVel);
    }
    if ((flags & 0x10) != 0) {
        if (frame == 0) {
            FxVectorCopy(origin, worldOrigin);
            prevSize = size;
        } else {
            Orient_TransformPoint(frame, origin, worldOrigin);
            for (i = 0; i < 3; i++) {
                FxVectorCopy(&axes[i * 3], &rotated[i * 3]);
                Orient_VecLocalToWorld((const struct Orient *)frame, &rotated[i * 3], &axes[i * 3]);
            }
            prevSize = size;
            if (lastEmitTime < g_fxSystem->time) {
                FxVectorSubtract(frame, lastFrameOrigin, frameVel);
                FxVectorScale(frameVel, 0.001f * (float)(g_fxSystem->time - lastEmitTime), frameVel);
            }
        }
    }
    EmitElements(frameVel, frame);
    return true;
}

// 0x08128c0c  size=51  FUN_08128c0c  status=translated  naming=inferred
// Light_Destruct: vtable 0x0815f348 slot 0 (RTTI 5Light, derives from Effect).
void Light_Destruct(struct Light *l)
{
    l->vtbl = (void **)vtbl_0815f348; /* vtbl_Light, RTTI 5Light */
    Effect_DestructBase(l);
}

// 0x08128c40  size=51  FUN_08128c40  status=translated  naming=inferred
// Light_DeletingDestruct: slot 1; returns the element to pool 0x08a53398 (0xfc, FxPoolFC).
void Light_DeletingDestruct(struct Light *l)
{
    l->vtbl = (void **)vtbl_0815f348; /* vtbl_Light, RTTI 5Light */
    Effect_DestructBase(l);
    FxPoolFC_Free(l);
}

// 0x08128c74  size=120  FUN_08128c74  status=translated  naming=inferred
// Light::InitCurves: slot 8 (shared with Flash); four curves from template curves[0],[1],[4],[5] (+0x100,+0x10c,+0x130,+0x13c) into +0xcc,+0xd8,+0xe4,+0xf0.
void Light::InitCurves(const FxParticleTemplate *tmpl)
{
    FxCurve_InitFromTemplate(&tmpl->curves[0], &colorCurveA);
    FxCurve_InitFromTemplate(&tmpl->curves[1], &colorCurveB);
    FxCurve_InitFromTemplate(&tmpl->curves[4], &sizeCurveA);
    FxCurve_InitFromTemplate(&tmpl->curves[5], &sizeCurveB);
}

// 0x08128cec  size=62  FUN_08128cec  status=translated  naming=inferred
// Light::IsCulled: slot 4; sphere (worldOrigin, size) against the view planes.
byte Light::IsCulled()
{
    int mask = GetTraceMask();
    return (byte)FX_IsCulledByPlanes((uint32)(size_t)g_fxSystem, worldOrigin, size, mask);
}

// 0x08128d2a  size=74  FUN_08128d2a  status=translated  naming=inferred
// Light::Draw: slot 5; hands (world origin, size, colour rgb) to the effects-system no-op FX_SubmitLight_Stub (0x0812d286).
void Light::Draw()
{
    ((void (*)(struct FxSystem *, const float *, float, float, float, float))FX_SubmitLight_Stub)(
        g_fxSystem, worldOrigin, size, color[0], color[1], color[2]);
}

// 0x08128d74  size=165  FUN_08128d74  status=translated  naming=inferred
// Light::Update: slot 3; evaluates the size and colour curves, then the world position through the binding frame.
bool Light::Update()
{
    const float *frame = 0;

    if (!UpdateFraction()) {
        return false;
    }
    if (!ResolveBinding(&frame)) {
        return false;
    }
    FxObjFc_EvalSizeCurve((struct Light *)this);
    FxObjFc_EvalColorCurve((struct Light *)this);
    if (frame == 0) {
        FxVectorCopy(origin, worldOrigin);
    } else {
        Orient_TransformPoint(frame, origin, worldOrigin);
    }
    return true;
}

// 0x08129d0a  size=51  FUN_08129d0a  status=translated  naming=inferred
// Flash_Destruct: vtable 0x0815f308 slot 0 (RTTI 5Flash, derives from Light); base destructor is Light_DestructComplete (the Light destructor body at 0x08128bd8).
void Flash_Destruct(struct Flash *f)
{
    f->vtbl = vtbl_Flash;
    Light_DestructComplete((struct Light *)f);
}

// 0x08129d3e  size=51  FUN_08129d3e  status=translated  naming=inferred
// Flash_DeletingDestruct: slot 1; returns the element to the 0xfc pool (FxPoolFC_Free).
void Flash_DeletingDestruct(struct Flash *f)
{
    f->vtbl = vtbl_Flash;
    Light_DestructComplete((struct Light *)f);
    FxPoolFC_Free(f);
}

// 0x0812b3c2  size=188  FUN_0812b3c2  status=translated  naming=unknown
// FxTemplate_EvalStartAlpha: rolls the alpha curve pair of a particle template (curves[2] at +0x118, curves[3] at +0x124) at t=0, blending with a random factor when template flag 0x4000 is set, and clamps to [0,1]. First parameter is unused.
float FxTemplate_EvalStartAlpha(void *unused, const FxParticleTemplate *tmpl)
{
    struct FxCurve curveA;
    struct FxCurve curveB;
    float blend;
    float value;

    FxCurve_InitFromTemplate(&tmpl->curves[2], &curveA);
    if ((tmpl->flags & 0x4000) == 0) {
        value = FxCurve_EvalScaled(&curveA, 0.0f);
    } else {
        blend = Rand_Float(0.0f, 1.0f);
        FxCurve_InitFromTemplate(&tmpl->curves[3], &curveB);
        value = FxCurve_EvalBlendScaled(&curveA, &curveB, blend, 0.0f);
    }
    return FX_ClampF(value, 0.0f, 1.0f);
}

// 0x0812b47e  size=166  FUN_0812b47e  status=translated  naming=unknown
// FxTemplate_EvalStartSize: same as FxTemplate_EvalStartAlpha for the size curve pair (curves[4] at +0x130, curves[5] at +0x13c, flag 0x8000 tested as a 16-bit value) but without clamping.
float FxTemplate_EvalStartSize(void *unused, const FxParticleTemplate *tmpl)
{
    struct FxCurve curveA;
    struct FxCurve curveB;
    float blend;
    float value;

    FxCurve_InitFromTemplate(&tmpl->curves[4], &curveA);
    if (((unsigned short)tmpl->flags & 0x8000) == 0) {
        value = FxCurve_EvalScaled(&curveA, 0.0f);
    } else {
        blend = Rand_Float(0.0f, 1.0f);
        FxCurve_InitFromTemplate(&tmpl->curves[5], &curveB);
        value = FxCurve_EvalBlendScaled(&curveA, &curveB, blend, 0.0f);
    }
    return value;
}

// 0x08131838  size=42  FUN_08131838  status=translated  naming=unknown
// Fx_PlayEffect_Forward: forwards to the global scheduler (0x08a53804).
void Fx_PlayEffect_Forward(struct FxEffectTemplate *effect, float *origin, float *forward)
{
    FxScheduler_PlayEffect_Forward(g_fxScheduler, effect, origin, forward);
}

// 0x08131862  size=49  FUN_08131862  status=translated  naming=unknown
// Fx_PlayEffect_ForwardUp: forwards to the global scheduler.
void Fx_PlayEffect_ForwardUp(struct FxEffectTemplate *effect, float *origin, float *forward, float *up)
{
    FxScheduler_PlayEffect_ForwardUp(g_fxScheduler, effect, origin, forward, up);
}

// 0x08131894  size=49  FUN_08131894  status=translated  naming=unknown
// Fx_PlayEffect_Axis: forwards to the global scheduler's FxScheduler_PlayEffect (origin, axis, bolt).
void Fx_PlayEffect_Axis(struct FxEffectTemplate *effect, float *origin, float *axis, int *bolt)
{
    FxScheduler_PlayEffect(g_fxScheduler, effect, origin, axis, bolt);
}

// 0x08131936  size=58  FUN_08131936  status=translated  naming=unknown
// Fx_ClockAdvanceWrap: clears g_08a53818, advances the effects clock, then resets the two list update bounds to the current list counts.
void Fx_ClockAdvanceWrap(int now)
{
    g_08a53818 = 0;
    FX_ClockAdvance((struct fx_clock_s *)g_fxSystem, now);
    g_fxListAUpdateBound = g_08a5c9a0;
    g_fxListBUpdateBound = g_08a5c9a4;
}

// 0x08131970  size=28  FUN_08131970  status=translated  naming=unknown
// Fx_ClockAdvanceFixedStepWrap: forwards to FX_ClockAdvanceFixedStep with the global clock.
void Fx_ClockAdvanceFixedStepWrap(int now)
{
    FX_ClockAdvanceFixedStep((struct fx_clock_s *)g_fxSystem, now);
}

// 0x08131cc0  size=21  FUN_08131cc0  status=translated  naming=unknown
// Fx_FreeFirstWord: frees the pointer stored in the first word of the object (Com_Free_A).
void Fx_FreeFirstWord(void **obj)
{
    Com_Free_A(*obj);
}

// 0x081332e2  size=37  FUN_081332e2  status=translated  naming=unknown
// ConfigBlock_WriteByte: writes one byte through ConfigBlock_Write with depth -1 (the byte's address is passed where the merged signature expects a StrPool **).
byte ConfigBlock_WriteByte(struct ConfigBlock *block, byte value)
{
    return (byte)ConfigBlock_Write((struct ParseNode *)block, (struct Fx_StringPage **)&value, -1);
}

// 0x081339ee  size=353  FUN_081339ee  status=translated  naming=unknown
// Fx_GetRayAttenuation: when enabled (g_08a5c9c4) and the distance between the two points is at least the dvar threshold, multiplies the slot-6 visibility results of every effect flagged 0x1000 in list B then list A, using radius 0.5 * distance. Returns 1.0 otherwise.
float Fx_GetRayAttenuation(const float *origin, const float *target)
{
    float dir[6];
    float radius;
    float scale;
    float len;
    int i;

    if (g_08a5c9c4 == 0) {
        return 1.0f;
    }
    Vec3_Sub_08137284(target, origin, dir);
    len = Vec3_Normalize(dir);
    if (len < g_08a53ca0->current.value) {
        return 1.0f;
    }
    radius = 0.5f * len;
    scale = 1.0f;
    for (i = 0; i < g_08a5c9a4; i++) {
        struct Effect *e = g_fxEffectListB[i];
        if ((e->flags & FX107_FLAG_VISIBILITY) != 0) {
            scale = scale * ((fx107_visibility_fn)e->vtbl[6])(e, origin, dir, radius);
        }
    }
    for (i = 0; i < g_08a5c9a0; i++) {
        struct Effect *e = g_fxEffectListA[i];
        if ((e->flags & FX107_FLAG_VISIBILITY) != 0) {
            scale = scale * ((fx107_visibility_fn)e->vtbl[6])(e, origin, dir, radius);
        }
    }
    return scale;
}

// 0x08133eaa  size=245  FUN_08133eaa  status=translated  naming=unknown
// Fx_RewindToTime: removes (without releasing) every effect in lists A and B whose start time (+0xb8) is later than maxStart, frees the removed lists, then clears the scheduler.
void Fx_RewindToTime(int maxStart)
{
    int i;
    int start;

    g_fxListAUpdateBound = g_08a5c9a0;
    g_08a5c9b4 = g_08a5c9a0;
    i = 0;
    while (i < g_fxListAUpdateBound) {
        start = FxObj_GetFieldB8((struct Effect *)g_fxEffectListA[i]);
        if (maxStart < start) {
            FX_RemoveFromListA(i, 0);
        } else {
            i++;
        }
    }
    FX_FreeRemovedListA();
    g_fxListBUpdateBound = g_08a5c9a4;
    g_08a5c9b8 = g_08a5c9a4;
    i = 0;
    while (i < g_fxListBUpdateBound) {
        start = FxObj_GetFieldB8((struct Effect *)g_fxEffectListB[i]);
        if (maxStart < start) {
            FX_RemoveFromListB(i, 0);
        } else {
            i++;
        }
    }
    FX_FreeRemovedListB();
    FxScheduler_Clear(g_fxScheduler, 0, 0);
}

// 0x08133ffe  size=239  FUN_08133ffe  status=translated  naming=unknown
// Fx_SaveState: attaches a 0x28-byte stream temporary to the target, serialises the effects slot, the scheduler playback list and every effect of list A then B, writes a terminating zero byte and finishes the stream.
void Fx_SaveState(void *target)
{
    struct fx107_stream_s st;
    int i;

    MsgStream_ResetAlt((struct msgstream_s *)&st);
    MsgStream_InitWrite((struct msgstream_s *)&st, (struct msgbuf_s *)target);
    FX_SerializeSlot((int *)g_fxSystem, (struct fx_stream_s *)&st);
    FX_SerializePlaybackList(g_fxScheduler, (struct fx_stream_s *)&st);
    for (i = 0; i < g_08a5c9a0; i++) {
        FxEffect_Write((struct Effect *)g_fxEffectListA[i], (struct FxMsg *)&st);
    }
    for (i = 0; i < g_08a5c9a4; i++) {
        FxEffect_Write((struct Effect *)g_fxEffectListB[i], (struct FxMsg *)&st);
    }
    MsgStream_WriteDecodedByte((struct msgstream_s *)&st, 0);
    Handle_GetTargetField8((int **)&st);
}

// 0x08137072  size=58  FUN_08137072  status=translated  naming=unknown
// Fx_RunListB: when the gating dvar (+8) is set, clears two counters and updates then collects draw list B from index 0.
void Fx_RunListB(void)
{
    if (((dvar_t *)g_08a53c80)->current.enabled != 0) {
        g_08a5c9c0 = 0;
        g_08a5cae0 = 0;
        FX_UpdateListBFrom(0);
        FX_CollectDrawListB();
    }
}

// 0x081370ac  size=58  FUN_081370ac  status=translated  naming=unknown
// Fx_RunListA: same as Fx_RunListB for list A (counters g_08a5c9bc, g_08a5cae4).
void Fx_RunListA(void)
{
    if (((dvar_t *)g_08a53c80)->current.enabled != 0) {
        g_08a5c9bc = 0;
        g_08a5cae4 = 0;
        FX_UpdateListAFrom(0);
        FX_CollectDrawListA();
    }
}

// 0x081370e6  size=109  FUN_081370e6  status=translated  naming=unknown
// Fx_RunFrame: when the gating dvar is set: frees removed entries, processes pending events, then updates list B and list A from their previous bounds (freeing removed entries after each) and draws.
void Fx_RunFrame(void)
{
    int oldBound;

    if (((dvar_t *)g_08a53c80)->current.enabled != 0) {
        FX_FreeRemovedListB();
        FX_FreeRemovedListA();
        Fx_ProcessPendingEvents();
        oldBound = g_fxListBUpdateBound;
        g_fxListBUpdateBound = g_08a5c9a4;
        FX_UpdateListBFrom(oldBound);
        FX_FreeRemovedListB();
        oldBound = g_fxListAUpdateBound;
        g_fxListAUpdateBound = g_08a5c9a0;
        FX_UpdateListAFrom(oldBound);
        FX_FreeRemovedListA();
        FX_DrawEffects();
    }
}

/* ============================================================== localisation */

// 0x081386e4  size=1177  FUN_081386e4  status=translated  naming=unknown
// Loc_TranslateMarkedSegments: walks the text in segments delimited by control bytes (0x14 = translate on, 0x15 = translate off, 0x16 = marker, NUL). Segments are copied (Q_strncpyz), translated with Loc_TranslatePart when enabled, appended to one of two 0x400-byte ring buffers (g_08a6e980) and "&&N" conversion markers are reordered so that segment order follows N. Overflow warns through Com_Printf (and Com_Error when loc_warnings and loc_warningsAsErrors are set and mode != 1). Evidence: strings "%s too long when translated: \"%s\"" and "%s cannot have &&0 as conversion format: \"%s\"\n".
// Quirk kept: `rest` is only written when the matching &&N was found, but is copied unconditionally afterwards.
char *Loc_TranslateMarkedSegments(char *text, const char *context, int mode)
{
    char tmp[1024];
    char rest[1024];
    char *buf;
    size_t segLen;
    int outLen;
    int i;
    bool translate;
    bool flagA;
    bool replaced;
    int pending;
    int nextIdx;
    char *segStart;
    char *scan;
    char c;

    g_08a6e960 = (g_08a6e960 + 1) % 2;
    memset(g_08a6e980[g_08a6e960], 0, 0x400);
    buf = g_08a6e980[g_08a6e960];
    outLen = 0;
    translate = true;
    flagA = true;
    pending = 0;
    nextIdx = 1;
    replaced = false;
    segStart = text;
    scan = text;

    for (;;) {
        while (true) {
            if (*segStart == '\0') {
                if (replaced) {
                    for (i = 0; i < outLen; i++) {
                        if (buf[i] == '\x16') {
                            buf[i] = '%';
                        }
                    }
                }
                return buf;
            }
            if (*scan == '\0' || *scan == '\x14' || *scan == '\x15' || *scan == '\x16') {
                break;
            }
            scan = scan + 1;
        }
        if (segStart < scan) {
            segLen = (size_t)(scan - segStart);
            Q_strncpyz(tmp, segStart, (int)(segLen + 1));
            if (translate) {
                if (Loc_TranslatePart(tmp, tmp, context, mode) == 0) {
                    return 0;
                }
                segLen = strlen(tmp);
            }
            if (0x3ff < segLen + (unsigned int)outLen) {
                if (g_loc_warnings != 0 && g_loc_warnings->current.enabled != 0 &&
                    g_loc_warningsAsErrors != 0 && g_loc_warningsAsErrors->current.enabled != 0 &&
                    mode != 1) {
                    Com_Error(1, "%s too long when translated: \"%s\"", context, text);
                }
                Com_Printf("%s too long when translated: \"%s\"\n", context, text);
            }
            for (i = 0; i < (int)(segLen - 2); i++) {
                if (strncmp(tmp + i, "&&", 2) == 0 && isdigit((int)tmp[i + 2]) != 0) {
                    if (flagA) {
                        pending = pending + 1;
                    } else {
                        tmp[i] = '\x16';
                        replaced = true;
                    }
                }
            }
            if (pending < 1 || outLen < 1) {
                strcpy(buf + outLen, tmp);
            } else {
                for (i = 0; i < outLen - 2; i++) {
                    if (strncmp(buf + i, "&&", 2) == 0 && isdigit((int)(char)buf[i + 2]) != 0) {
                        c = buf[i + 2];
                        if (c == '0') {
                            Com_Printf("%s cannot have &&0 as conversion format: \"%s\"\n", context, text);
                        }
                        if (c - '0' == nextIdx) {
                            strcpy(rest, buf + i + 3);
                            buf[i] = 0;
                            nextIdx = nextIdx + 1;
                            break;
                        }
                    }
                }
                strcpy(buf + i, tmp);
                strcpy(buf + segLen + i, rest);
                outLen = outLen - 3;
                pending = pending - 1;
            }
            outLen = outLen + (int)segLen;
        }
        if (*scan == '\x14') {
            translate = true;
            scan = scan + 1;
        } else if (*scan == '\x15') {
            translate = false;
            scan = scan + 1;
        }
        c = *scan;
        if (c == '\x16') {
            scan = scan + 1;
        }
        flagA = (c != '\x16');
        segStart = scan;
    }
}

/* ============================================================== PunkBuster glue */

// 0x0813c160  size=60  FUN_0813c160  status=translated  naming=unknown
// PB_DispatchEvent71: PB_Dispatch(&pb, 0x71, -1, 0, 0, 0) on the global PunkBuster object.
void PB_DispatchEvent71(void)
{
    PB_Dispatch(&g_08a6f2c0, 0x71, -1, 0, 0, 0);
}

// 0x0813c43a  size=233  FUN_0813c43a  status=translated  naming=unknown
// PB_ServerQuery: PunkBuster server query callback (0x65 max clients, 0x66 client info, 0x67 dvar display string, 0x72 client score text); the buffer is NUL-terminated at index 0xff first. Returns "PB Error: Query Failed" when a client lookup fails, otherwise NULL.
char *PB_ServerQuery(int query, char *buffer)
{
    buffer[0xff] = '\0';
    if (query == 0x66) {
        int n = atoi(buffer);
        if (SV_PB_GetClientInfo(n, buffer) == 0) {
            return (char *)"PB Error: Query Failed";
        }
    } else if (query < 0x67) {
        if (query == 0x65) {
            int maxClients = SV_PB_GetMaxClients();
            Int_ToStringRadix((unsigned int)maxClients, buffer, 10);
        }
    } else if (query == 0x67) {
        const char *src = Dvar_GetDisplayString(buffer);
        strncpy(buffer, src, 0xff);
    } else if (query == 0x72) {
        int n = atoi(buffer);
        if (SV_PB_GetClientScoreText(n, buffer) == 0) {
            return (char *)"PB Error: Query Failed";
        }
    }
    return 0;
}

// 0x0813c524  size=93  FUN_0813c524  status=translated  naming=unknown
// PB_PrintCallback: output sink for PunkBuster text; text tagged "[skipnotify]" (compared against the PB object's name field at +0x14) is printed with Com_Printf("%s: %s\n") using the string at PB+0x20, everything else goes to SV_PrintTagged. Returns 0.
int PB_PrintCallback(const char *text)
{
    if (strncasecmp((char *)&g_08a6f2c0 + 0x14, "[skipnotify]", 0xc) == 0) {
        Com_Printf("%s: %s\n", (char *)&g_08a6f2c0 + 0x20, text);
    } else {
        SV_PrintTagged((char *)&g_08a6f2c0 + 0x14, text);
    }
    return 0;
}

// 0x0813c582  size=38  FUN_0813c582  status=translated  naming=unknown
// PB_SendToClientsCallback: forwards to SV_PB_SendOOBToClients and returns 0.
int PB_SendToClientsCallback(int arg1, int arg2, int clientNum)
{
    SV_PB_SendOOBToClients(arg1, arg2, clientNum);
    return 0;
}

// 0x0813c5a8  size=53  FUN_0813c5a8  status=translated  naming=unknown
// PB_SendPacketCallback: forwards to Sys_SendPacketToHost (port narrowed to 16 bits) and returns 0.
int PB_SendPacketCallback(const char *host, uint16 port, int length, const void *data)
{
    Sys_SendPacketToHost(host, port, length, data);
    return 0;
}
