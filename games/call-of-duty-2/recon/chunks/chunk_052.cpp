#include "cod2_types.h"
#include <strings.h>
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_052_types.h"

/*
 * chunk_052: cod2_lnxded 1.3, 0x0811c0d0..0x081203d4 (92 functions).
 * Game entity pool, attachment/child links, trigger charges, crosshair
 * targeting, configstring indices, temp entities, tracked slots, client
 * position snapshots and the bullet fire path.
 *
 * Constant and operand widths follow the binary: 32-bit int, x87 long double
 * where the decompile uses longdouble, float compares against double literals
 * (promoted), and truncation for the (int)ROUND() sites (the control word is
 * forced to round-toward-zero in the binary).
 */

/* ---- Forward declarations for in-chunk functions ---- */

/* Out-of-chunk, not in the shared header. */

/* ---- Helpers (chunk-local) ---- */
#define C52_ENT(i) (&g_gentities[(i)])
#define C52_CLIENT(i) ((gclient_s *)((uint8 *)c52_ptr(level_clients) + (i) * 0x28a4))
#define C52_ENT_FROM_PTR(p) ((gentity_s *)(p))

static float c52_table_f(const uint8 *base, int32 i)
{
    float f;
    memcpy(&f, base + i * 8, 4);
    return f;
}

/* ===================================================================== */

// 0x0811c0d0  size=119  FUN_0811c0d0  status=translated  naming=inferred
// evidence: Quake III / IW OnSameTeam counterpart; both entities need a client
// with a non-zero team (+0x274c) that compares equal; called from the bullet and
// damage paths (80ff630, 811fe90).
int OnSameTeam(gentity_s *a, gentity_s *b)
{
    int result;
    if (a->client == NULL || b->client == NULL) {
        result = 0;
    } else if (a->client->cs.team == 0) {
        result = 0;
    } else if (a->client->cs.team == b->client->cs.team) {
        result = 1;
    } else {
        result = 0;
    }
    return result;
}

// 0x0811c148  size=570  FUN_0811c148  status=translated  naming=unknown
// evidence: no string; computes an 8000-unit forward trace from the muzzle,
// stores the hit entity number and its cooldown field into the client.
void Client_UpdateCrosshairTarget(gentity_s *ent)
{
    gclient_s *cl = ent->client;
    float muzzle[3];
    float forward[3];
    float end[3];
    trace_s trace;
    uint32 hit;
    int32 hitValue;

    if (cl->sessionState == 0) {
        if (cl->cs.team == 0) {
            cl->ps.stats[3] = 0xffffffffu;
            cl->ps.stats[4] = 0;
            return;
        }
        G_GetPlayerViewOrigin(ent, muzzle);
        G_GetPlayerViewAxis(ent, forward, 0, 0);
        Vec3_MulAdd(muzzle, 8000.0f, forward, end);
    } else {
        G_GetPlayerViewOrigin(ent, muzzle);
        G_GetPlayerViewAxis(ent, forward, 0, 0);
        if (cl->ps.viewHeightCurrent < 8.0) {
            muzzle[2] = (float)((double)muzzle[2] + (8.0 - (double)cl->ps.viewHeightCurrent));
        }
        Vec3_MulAdd(muzzle, 8000.0f, forward, end);
    }

    G_TraceBox(&trace, muzzle, vec3_origin, vec3_origin, end, cl->ps.clientNum, 0x2000001);
    hit = trace.entityNum;
    if (hit < 0x40 && C52_ENT(hit)->client != NULL &&
        (G_IsSessionPlaying(ent) == 0 || C52_ENT(hit)->client->cs.team == cl->cs.team)) {
        hitValue = C52_ENT(hit)->health;
    } else {
        hit = 0xffffffffu;
        hitValue = 0;
    }
    cl->ps.stats[3] = hit;
    cl->ps.stats[4] = hitValue;
}

// 0x0811c382  size=132  FUN_0811c382  status=translated  naming=unknown
// evidence: no string; throttles refresh to once per server time step over all
// in-use entities whose client flag 0x400000 is clear.
void Client_UpdateAllCrosshairTargets(void)
{
    int32 i;
    if (0 < g_0864c56c - level_lastAimRefreshTime) {
        level_lastAimRefreshTime = g_0864c56c;
        for (i = 0; i < *(int32 *)((uint8 *)c52_ptr(g_087a2408) + 8); i++) {
            if (C52_ENT(i)->r.inuse != 0 &&
                (C52_ENT(i)->client->ps.pm_flags & 0x400000u) == 0) {
                Client_UpdateCrosshairTarget(C52_ENT(i));
            }
        }
    }
}

// 0x0811c406  size=83  FUN_0811c406  status=translated  naming=unknown
// evidence: out = a + scale * b per axis (VectorMA shape); no string.
void Vec3_MulAdd(const float *a, float scale, const float *b, float *out)
{
    out[0] = a[0] + scale * b[0];
    out[1] = a[1] + scale * b[1];
    out[2] = a[2] + scale * b[2];
}

// 0x0811c45c  size=168  FUN_0811c45c  status=translated  naming=unknown
// evidence: no string; appends an (entity, entity) pair with generations to a
// 256-entry queue, or sends the overflow event when the queue is full.
void G_QueueEntityPairEvent(gentity_s *a, gentity_s *b)
{
    int32 idx;
    if (Scr_IsInitialized(1) != 0) {
        if (g_entityPairQueueCount == 0x100) {
            Scr_AddEntityParam(b);
            Scr_NotifyEntity(a, scr_str_trigger, 1);
        } else {
            idx = g_entityPairQueueCount;
            g_entityPairQueueCount = g_entityPairQueueCount + 1;
            g_entityPairQueue[idx].a = (uint16)a->s.number;
            g_entityPairQueue[idx].b = (uint16)b->s.number;
            g_entityPairQueue[idx].genA = (uint32)a->spawnSerial;
            g_entityPairQueue[idx].genB = (uint32)b->spawnSerial;
        }
    }
}

// 0x0811c504  size=57  FUN_0811c504  status=translated  naming=unknown
// evidence: no string; body-entity init (contents 0x405c0008, link flag, flags |= 1).
void G_InitBodyEntity(gentity_s *ent)
{
    SV_SetBrushModel(ent);
    ent->r.contents = 0x405c0008;
    ent->r.svFlags = 1;
    ent->s.eFlags |= 1;
}

// 0x0811c53e  size=87  FUN_0811c53e  status=translated  naming=unknown
// evidence: no string; ORs a flag into effectFlags when a cvar-like gate is set
// and helper 0811a406 reports a non-positive value.
void Ent_AddFlagIfCondition(gentity_s *ent, uint32 flag)
{
    int ok;
    float f = 0.0f;
    if (*(uint8 *)&g_spawnVars != 0) {
        ok = G_SpawnFloat((const char *)g_0815eae1, (const char *)g_0815eae0, &f);
        if (ok != 0 && (double)f <= 0.0) {
            ent->spawnflags = flag | ent->spawnflags;
        }
    }
}

// 0x0811c596  size=178  FUN_0811c596  status=translated  naming=unknown
// evidence: no string; derives contents bits from effectFlags.
void Ent_UpdateContentsFromFlags(gentity_s *ent)
{
    ent->r.contents = 0;
    if ((ent->spawnflags & 8) == 0) {
        ent->r.contents = (int32)((uint32)ent->r.contents | 0x40000000u);
    }
    if ((ent->spawnflags & 1) != 0) {
        ent->r.contents = (int32)((uint32)ent->r.contents | 0x40000u);
    }
    if ((ent->spawnflags & 2) != 0) {
        ent->r.contents = (int32)((uint32)ent->r.contents | 0x80000u);
    }
    if ((ent->spawnflags & 4) != 0) {
        ent->r.contents = (int32)((uint32)ent->r.contents | 0x100000u);
    }
}

// 0x0811c648  size=37  FUN_0811c648  status=translated  naming=unknown
// evidence: no string; calls the link refresh when effectFlags bit 0x10 is set.
void Ent_RefreshIfFlag10(gentity_s *ent)
{
    if ((ent->spawnflags & 0x10) != 0) {
        G_CallDeleteScript(ent);
    }
}

// 0x0811ca92  size=39  FUN_0811ca92  status=translated  naming=unknown
// evidence: no string; toggles the entity kind between 2 and 3.
void Ent_ToggleKind(gentity_s *ent)
{
    if (ent->handler == 3) {
        ent->handler = 2;
    } else {
        ent->handler = 3;
    }
}

// 0x0811cb7e  size=311  FUN_0811cb7e  status=translated  naming=unknown
// evidence: no string; returns 0 when a blocking effectFlags bit matches the kind.
int Ent_CanTriggerKind(gentity_s *ent, int kind)
{
    uint32 f = ent->spawnflags;
    if ((f & 1) != 0 && kind == 1) return 0;
    if ((f & 2) != 0 && kind == 2) return 0;
    if ((f & 4) != 0 && !(3 < (uint32)(kind - 3))) return 0;
    if ((f & 8) != 0 && !(kind < 3 || (kind > 6 && kind != 14))) return 0;
    if ((f & 0x10) != 0 && (kind == 4 || kind == 6)) return 0;
    if ((f & 0x20) != 0 && kind == 7) return 0;
    if ((f & 0x100) != 0 &&
        (kind == 0 || (kind >= 0 && (uint32)(kind - 9) <= 4))) return 0;
    return 1;
}

// 0x0811ccb6  size=172  FUN_0811ccb6  status=translated  naming=unknown
// evidence: no string; consumes a charge when the kind check, count and cooldown allow.
void Ent_TryConsumeCharge(gentity_s *ent, gentity_s *other, int index, int kind)
{
    if ((ent->u.trigger.threshold < 1 || ent->u.trigger.threshold <= index) &&
        Ent_CanTriggerKind(ent, kind) != 0 &&
        (ent->u.trigger.accumulate == 0 || ent->u.trigger.accumulate <= 32000 - ent->health)) {
        if (kind != -1) {
            G_QueueEntityPairEvent(ent, other);
        }
        ent->health = 32000;
        if ((ent->spawnflags & 0x200) != 0) {
            G_CallDeleteScript(ent);
        }
    }
}

// 0x0811cd62  size=48  FUN_0811cd62  status=translated  naming=unknown
// evidence: no string; Ent_TryConsumeCharge with index count+1 and kind -1.
void Ent_TryConsumeChargeNext(gentity_s *ent, gentity_s *other)
{
    Ent_TryConsumeCharge(ent, other, ent->u.trigger.accumulate + 1, -1);
}

// 0x0811cd92  size=65  FUN_0811cd92  status=translated  naming=unknown
// evidence: no string; Ent_TryConsumeCharge, then resets the cooldown when count is zero.
void Ent_TryConsumeChargeAndReset(gentity_s *ent, gentity_s *other, int index, int p4, int kind)
{
    (void)p4; /* unused by the binary */
    Ent_TryConsumeCharge(ent, other, index, kind);
    if (ent->u.trigger.accumulate == 0) {
        ent->health = 32000;
    }
}

// 0x0811cea8  size=494  FUN_0811cea8  status=translated  naming=unknown
// evidence: no string; area query over [start,end], applies the trace filter
// on scrName == scrStr_trigger_damage and bounds the entity with its mins/maxs.
void Ent_AreaEffectApply(gentity_s *self, const float *start, const float *end, int p4, int p5)
{
    float box_lo[3];
    float box_hi[3];
    float bound_min[3];
    float bound_max[3];
    int32 list[1026];
    int32 count;
    int32 i;

    Vec3_CopyBits_0811d28a(start, box_lo);
    Vec3_CopyBits_0811d28a(start, box_hi);
    Bounds3_AddPoint(end, box_lo, box_hi);
    count = CM_AreaEntities(box_lo, box_hi, list, 0x400, 0x400000);
    for (i = 0; i < count; i++) {
        int32 idx = list[i];
        gentity_s *e = C52_ENT(idx);
        if ((int16)e->classname == scrStr_trigger_damage) {
            Vec3_Add_0811d2ba(e->r.currentOrigin, e->r.mins, bound_min);
            Vec3_Add_0811d2ba(e->r.currentOrigin, e->r.maxs, bound_max);
            if (SV_BoxEntityHit(start, vec3_origin, vec3_origin, end, e->s.number, -1) != 0) {
                Scr_AddEntityParam(self);
                Scr_AddIndex(p4);
                Scr_NotifyEntity(e, scrStr_damage, 2);
                Ent_TryConsumeCharge(e, self, p4, p5);
                if (e->u.trigger.accumulate == 0) {
                    C52_ENT(idx)->health = 32000;
                }
            }
        }
    }
}

// 0x0811d096  size=432  FUN_0811d096  status=translated  naming=unknown
// evidence: no string; like Ent_AreaEffectApply but filters on effect flag 0x4000.
void Ent_AreaEffectApplyFlagged(gentity_s *self, const float *start, const float *end, int p4, int p5)
{
    float box_lo[3];
    float box_hi[3];
    int32 list[1026];
    int32 count;
    int32 i;

    Vec3_CopyBits_0811d28a(start, box_lo);
    Vec3_CopyBits_0811d28a(start, box_hi);
    Bounds3_AddPoint(end, box_lo, box_hi);
    count = CM_AreaEntities(box_lo, box_hi, list, 0x400, 0x400000);
    for (i = 0; i < count; i++) {
        int32 idx = list[i];
        gentity_s *e = C52_ENT(idx);
        if ((int16)e->classname == scrStr_trigger_damage &&
            (e->flags & 0x4000u) != 0 &&
            SV_BoxEntityHit(start, vec3_origin, vec3_origin, end, e->s.number, -1) != 0) {
            Scr_AddEntityParam(self);
            Scr_AddIndex(p4);
            Scr_NotifyEntity(e, scrStr_damage, 2);
            Ent_TryConsumeCharge(e, self, p4, p5);
            if (e->u.trigger.accumulate == 0) {
                C52_ENT(idx)->health = 32000;
            }
        }
    }
}

// 0x0811d246  size=68  FUN_0811d246  status=translated  naming=unknown
// evidence: no string; static-entity init (contents 0x20000000, link flag, flags |= 1, relink).
void G_InitStaticEntity(gentity_s *ent)
{
    SV_SetBrushModel(ent);
    ent->r.contents = 0x20000000;
    ent->r.svFlags = 1;
    ent->s.eFlags |= 1;
    SV_LinkEntity(ent);
}

// 0x0811d28a  size=47  FUN_0811d28a  status=translated  naming=unknown
// evidence: no string; copies three 32-bit words (vector copy).
void Vec3_CopyBits_0811d28a(const void *src, void *dst)
{
    const uint32 *s = (const uint32 *)src;
    uint32 *d = (uint32 *)dst;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

// 0x0811d2ba  size=68  FUN_0811d2ba  status=translated  naming=unknown
// evidence: no string; out = a + b per axis.
void Vec3_Add_0811d2ba(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x0811d300  size=253  FUN_0811d300  status=translated  naming=inferred
// evidence: format "%s \"%s\" not precached" in the function; Quake III/IW
// G_FindConfigstringIndex counterpart (case-insensitive scan of a configstring
// range, optional create); callers pass CS base 0x51e/0x61e/0x6e/0x34e/0x48e/0x24e.
int G_FindConfigstringIndex(const char *name, int csBase, int maxCount, int create, const char *errPrefix)
{
    int i;
    const char *cs;
    const char *msg;

    if (name == NULL || *name == '\0') {
        return 0;
    }
    i = 1;
    while (i < maxCount && (cs = SV_GetConfigstringOrEmpty(i + csBase), *cs != '\0')) {
        if (strcasecmp(cs, name) == 0) {
            return i;
        }
        i++;
    }
    if (create == 0) {
        if (errPrefix != NULL) {
            msg = va("%s \"%s\" not precached", errPrefix, name);
            Scr_Error(msg);
        }
        return 0;
    }
    if (i == maxCount) {
        /* The binary passes csBase in a %s slot here; kept as-is. */
        msg = va((const char *)g_0815ec20, csBase, name);
        Com_Error(1, msg);
    }
    SV_SetConfigstring(i + csBase, name);
    return i;
}

// 0x0811d3fe  size=75  FUN_0811d3fe  status=translated  naming=inferred
// evidence: error label string "localized string" in the function; wrapper of
// the configstring lookup for CS base 0x51e, max 0x100.
int G_LocalizedStringIndex(const char *name)
{
    if (*name == '\0') {
        return 0;
    }
    return G_FindConfigstringIndex(name, 0x51e, 0x100, level_initializing, "localized string");
}

// 0x0811d44a  size=81  FUN_0811d44a  status=translated  naming=observed
// evidence: label string "shader" passed to the configstring lookup in this function.
int G_ShaderIndex(const char *name)
{
    char buf[72];
    strcpy(buf, name);
    Q_strlwr(buf);
    return G_FindConfigstringIndex(buf, 0x61e, 0x80, level_initializing, "shader");
}

// 0x0811d49c  size=235  FUN_0811d49c  status=translated  naming=observed
// evidence: format "model '%s' not precached" in the function.
int G_ModelIndex(const char *name)
{
    int i;
    const char *cs;
    const char *msg;
    int32 handle;
    int result;

    if (*name == '\0') {
        return 0;
    }
    i = 1;
    while (i < 0x100 && (cs = SV_GetConfigstringOrEmpty(i + 0x14e), *cs != '\0')) {
        if (strcasecmp(cs, name) == 0) {
            return i;
        }
        i++;
    }
    if (level_initializing == 0) {
        msg = va("model '%s' not precached", name);
        Scr_Error(msg);
    }
    if (i == 0x100) {
        Com_Error(1, (const char *)g_0815ec7f);
    }
    handle = (int32)(size_t)SV_RegisterModel(name);
    g_modelHandleTable[i] = handle;
    SV_SetConfigstring(i + 0x14e, name);
    result = i;
    return result;
}

// 0x0811d588  size=15  FUN_0811d588  status=translated  naming=unknown
// evidence: no string; returns the model handle table entry.
int G_GetModelHandle(int index)
{
    return g_modelHandleTable[index];
}

// 0x0811d598  size=27  FUN_0811d598  status=translated  naming=unknown
// evidence: no string; releases the model handle of an index.
int G_ModelIsDefault(int index)
{
    int32 handle = G_GetModelHandle(index);
    return XModel_IsDefault((const XModel *)handle);
}

// 0x0811d5b4  size=24  FUN_0811d5b4  status=translated  naming=unknown
// evidence: no string; returns the configstring of model index (0x14e base). Ghidra
// prints void, but the binary tail-returns the callee's eax (checked in disassembly),
// and callers use the returned pointer.
const char *G_GetModelName(int index)
{
    return SV_GetConfigstringOrEmpty(index + 0x14e);
}

// 0x0811d5cc  size=51  FUN_0811d5cc  status=translated  naming=unknown
// evidence: no string; configstring lookup at base 0x6e, max 0x20, no error text.
// Ghidra prints void; the binary returns the callee's eax.
int G_ItemIndex(const char *name)
{
    return G_FindConfigstringIndex(name, 0x6e, 0x20, 1, NULL);
}

// 0x0811d600  size=52  FUN_0811d600  status=translated  naming=observed
// evidence: label string "effect" passed to the configstring lookup here.
// Ghidra prints void; the binary returns the callee's eax.
int G_EffectIndex(const char *name)
{
    return G_FindConfigstringIndex(name, 0x34e, 0x40, level_initializing, "effect");
}

// 0x0811d634  size=51  FUN_0811d634  status=translated  naming=unknown
// evidence: no string; configstring lookup at base 0x48e, max 0x10, create without error.
// Ghidra prints void; the binary returns the callee's eax.
int G_ShellShockIndex(const char *name)
{
    return G_FindConfigstringIndex(name, 0x48e, 0x10, 1, NULL);
}

// 0x0811d668  size=54  FUN_0811d668  status=translated  naming=unknown
// evidence: no string; configstring lookup at base 0x24e, max 0x100, truncated to a byte.
uint8 G_SoundIndex(const char *name)
{
    return (uint8)G_FindConfigstringIndex(name, 0x24e, 0x100, 1, NULL);
}

// 0x0811d69e  size=475  FUN_0811d69e  status=translated  naming=unknown
// evidence: no string; rebuilds the model/tag list of an unparented entity and
// hands it to the render-side callee Com_ServerDObjCreate.
void Ent_RefreshAttachments(gentity_s *ent)
{
    uint32 list[26];
    uint32 local;
    uint32 count;
    int k;

    if (ent->client == NULL) {
        Ent_UnlinkEntity(ent);
        local = ent->model;
        if (local == 0) {
            Ent_UpdateChildrenTags(ent, 0);
        } else {
            list[0] = (uint32)G_GetModelHandle((int)local);
            list[1] = 0;
            list[2] = 0;
            count = 1;
            if (ent->s.eType == 0 || ent->s.eType == 6 || ent->s.eType == 9) {
                ent->s.index = (int32)local;
            }
            for (k = 0; k < 7; k++) {
                local = ent->attachModelNames[k];
                if (local != 0) {
                    list[count * 3] = (uint32)G_GetModelHandle((int)local);
                    list[count * 3 + 1] = (uint32)c52_int(ScrStr_TextFromHandle(ent->attachTagNames[k]));
                    list[count * 3 + 2] = ((uint32)ent->attachIgnoreCollision >> (k & 0x1f)) & 1u;
                    count++;
                }
            }
            Com_ServerDObjCreate(list, (int)(count & 0xffff), 0, ent->s.number);
            Ent_UpdateChildrenTags(ent, 1);
        }
    }
}

// 0x0811d87a  size=55  FUN_0811d87a  status=translated  naming=unknown
// evidence: no string; sets the model index from a name (0 for an empty name).
void Ent_SetModelByName(gentity_s *ent, const char *name)
{
    if (*name == '\0') {
        ent->model = 0;
    } else {
        ent->model = (uint8)G_ModelIndex(name);
    }
}

// 0x0811d8b2  size=78  FUN_0811d8b2  status=translated  naming=unknown
// evidence: no string; replaces the model handle at an index and notifies Hunk_SetUserEntryValue.
void G_SetModelHandle(int index, const char *name)
{
    const char *current = G_GetModelName(index);
    int32 handle = (int32)(size_t)SV_RegisterModel(name);
    g_modelHandleTable[index] = handle;
    Hunk_SetUserEntryValue(4, (const char *)(c52_int(current) + 7), handle);
}

// 0x0811d900  size=184  FUN_0811d900  status=translated  naming=unknown
// evidence: no string; adds a model/tag attachment into the first free slot.
int Ent_AddAttachment(gentity_s *ent, const char *name, int32 tagName, int linkFlag)
{
    int slot = 0;
    while (1) {
        if (6 < slot) {
            return 0;
        }
        if (ent->attachModelNames[slot] == 0) {
            break;
        }
        slot++;
    }
    ent->attachModelNames[slot] = (uint8)G_ModelIndex(name);
    ScrStr_AssignRef(&ent->attachTagNames[slot], tagName);
    if (linkFlag != 0) {
        ent->attachIgnoreCollision = (uint8)((1 << (slot & 0x1f)) | ent->attachIgnoreCollision);
    }
    Ent_RefreshAttachments(ent);
    return 1;
}

// 0x0811d9b8  size=409  FUN_0811d9b8  status=translated  naming=unknown
// evidence: no string; removes the attachment whose tag matches and whose model name matches.
uint32 Ent_RemoveAttachment(gentity_s *ent, const char *name, uint32 tagName)
{
    int slot = 0;
    const char *cur;
    while (1) {
        if (6 < slot) {
            return 0;
        }
        if ((uint32)ent->attachTagNames[slot] == tagName) {
            cur = G_GetModelName(ent->attachModelNames[slot]);
            if (strcasecmp(cur, name) == 0) {
                ent->attachModelNames[slot] = 0;
                ScrStr_AssignRef(&ent->attachTagNames[slot], 0);
                for (; slot < 6; slot++) {
                    ent->attachModelNames[slot] = ent->attachModelNames[slot + 1];
                    ent->attachTagNames[slot] = ent->attachTagNames[slot + 1];
                    if ((((uint32)ent->attachIgnoreCollision >> (((uint8)slot + 1) & 0x1f)) & 1u) == 0) {
                        ent->attachIgnoreCollision = (uint8)(~(uint8)(1 << (slot & 0x1f)) & ent->attachIgnoreCollision);
                    } else {
                        ent->attachIgnoreCollision = (uint8)((1 << (slot & 0x1f)) | ent->attachIgnoreCollision);
                    }
                }
                ent->attachModelNames[slot] = 0;
                ent->attachTagNames[slot] = 0;
                ent->attachIgnoreCollision = (uint8)(~(uint8)(1 << (slot & 0x1f)) & ent->attachIgnoreCollision);
                Ent_RefreshAttachments(ent);
                return 1;
            }
        }
        slot++;
    }
}

// 0x0811db52  size=94  FUN_0811db52  status=translated  naming=unknown
// evidence: no string; clears all seven attachment slots and their flags, then refreshes.
void Ent_ClearAttachments(gentity_s *ent)
{
    int i;
    for (i = 0; i < 7; i++) {
        ent->attachModelNames[i] = 0;
        ScrStr_AssignRef(&ent->attachTagNames[i], 0);
    }
    ent->attachIgnoreCollision = 0;
    Ent_RefreshAttachments(ent);
}

// 0x0811dbb0  size=333  FUN_0811dbb0  status=translated  naming=unknown
// evidence: no string; links child under parent's child list after a cycle check,
// creating a 0x70-byte link node.
int Ent_LinkChild(gentity_s *child, gentity_s *parent, int32 tagName)
{
    int32 tagIndex;
    gentity_s *cursor;
    game_attach_node_s *node;

    Ent_UnlinkFromParent(child);
    if (tagName == 0) {
        tagIndex = -1;
    } else {
        if (SV_DObjExists(parent) == 0) {
            return 0;
        }
        tagIndex = SV_DObjGetBoneIndexByName(parent, tagName);
        if (tagIndex < 0) {
            return 0;
        }
    }
    cursor = parent;
    while (1) {
        if (cursor == child) {
            return 0;
        }
        if (cursor->tagInfo == NULL) {
            break;
        }
        cursor = cursor->tagInfo->owner;
    }
    node = (game_attach_node_s *)StrPool_AllocBlock(0x70, 0x10);
    node->owner = parent;
    node->scrName = 0;
    ScrStr_AssignRef(&node->scrName, tagName);
    node->next = parent->tagChildren;
    node->tagIndex = tagIndex;
    memset(&node->origin, 0, 0x30);
    parent->tagChildren = child;
    child->tagInfo = node;
    memset((uint8 *)node + 0x40, 0, 0x30);
    return 1;
}

// 0x0811dcfe  size=75  FUN_0811dcfe  status=translated  naming=unknown
// evidence: no string; Ent_LinkChild and a transform sync on success.
bool Ent_LinkChildAndReset(gentity_s *child, gentity_s *parent, int32 tagName)
{
    int ok = Ent_LinkChild(child, parent, tagName);
    if (ok != 0) {
        Ent_SyncAttachTransform(child, 0);
    }
    return ok != 0;
}

// 0x0811dd4a  size=110  FUN_0811dd4a  status=translated  naming=unknown
// evidence: no string; Ent_LinkChild, then stores the angles and origin offsets in the link node.
bool Ent_LinkChildWithOffsets(gentity_s *child, gentity_s *parent, int32 tagName,
                              const float *angles, const float *origin)
{
    int ok = Ent_LinkChild(child, parent, tagName);
    if (ok != 0) {
        game_attach_node_s *node = child->tagInfo;
        AnglesToAxis(origin, node->origin);
        Vec3_CopyBits_0811f58c(angles, node->angles);
    }
    return ok != 0;
}

// 0x0811ddb8  size=299  FUN_0811ddb8  status=translated  naming=unknown
// evidence: no string; detaches a child from its parent's list and frees the link node.
void Ent_UnlinkFromParent(gentity_s *child)
{
    game_attach_node_s *node = child->tagInfo;
    gentity_s *parent;
    gentity_s *cursor;
    gentity_s *prev;
    float tmp[3];

    if (node == NULL) {
        return;
    }
    G_SetPositionTrajectory(child, child->r.currentOrigin);
    G_SetAnglesTrajectory(child, child->r.currentAngles);
    if (child->client != NULL) {
        Vec3_CopyBits_0811d28a(child->client->ps.viewangles, tmp);
        tmp[2] = 0.0f;
        SetClientViewAngle(child, tmp);
    }
    parent = node->owner;
    prev = NULL;
    for (cursor = parent->tagChildren; cursor != child; cursor = cursor->tagInfo->next) {
        prev = cursor;
    }
    if (prev == NULL) {
        parent->tagChildren = node->next;
    } else {
        prev->tagInfo->next = node->next;
    }
    child->tagInfo = NULL;
    ScrStr_AssignRef(&node->scrName, 0);
    StrPool_FreeBlock(node, 0x70);
}

// 0x0811dee4  size=53  FUN_0811dee4  status=translated  naming=unknown
// evidence: no string; true when the entity's link node owner is parent.
bool Ent_HasParent(gentity_s *child, gentity_s *parent)
{
    int result = 0;
    if (child->tagInfo != NULL && child->tagInfo->owner == parent) {
        result = 1;
    }
    return result != 0;
}

// 0x0811df1a  size=105  FUN_0811df1a  status=translated  naming=unknown
// evidence: no string; resolves the child's tag index by name, unlinking when unresolved.
void Ent_UpdateTagIndex(gentity_s *child, int force)
{
    game_attach_node_s *node = child->tagInfo;
    if (node->scrName == 0) {
        node->tagIndex = -1;
        return;
    }
    if (force != 0) {
        node->tagIndex = SV_DObjGetBoneIndexByName(node->owner, node->scrName);
        if (node->tagIndex >= 0) {
            return;
        }
    }
    Ent_UnlinkFromParent(child);
}

// 0x0811df84  size=69  FUN_0811df84  status=translated  naming=unknown
// evidence: no string; walks the child list and updates each tag index.
void Ent_UpdateChildrenTags(gentity_s *parent, int force)
{
    gentity_s *cursor = parent->tagChildren;
    while (cursor != NULL) {
        gentity_s *next = cursor->tagInfo->next;
        Ent_UpdateTagIndex(cursor, force);
        cursor = next;
    }
}

// 0x0811dfca  size=267  FUN_0811dfca  status=translated  naming=unknown
// evidence: no string; writes the child's attachment orientation (48 bytes) into out.
void Ent_GetAttachTransform(gentity_s *child, game_orient_s *out)
{
    game_attach_node_s *node = child->tagInfo;
    gentity_s *parent = node->owner;
    float angles_tmp[12];                 /* [ebp-0x48]: 4x3 matrix, AnglesToAxis rows then the origin row (+0x24) */
    float *origin_tmp = angles_tmp + 9;   /* the original copies/reads the origin at [ebp-0x48]+0x24 */
    game_orient_s tag;
    int32 base;

    if (node->tagIndex < 0) {
        AnglesToAxis(parent->r.currentAngles, (float *)out);
        Vec3_CopyBits_0811d28a(parent->r.currentOrigin, &out->origin);
    } else {
        AnglesToAxis(parent->r.currentAngles, angles_tmp);
        Vec3_CopyBits_0811d28a(parent->r.currentOrigin, origin_tmp);
        Ent_UpdateTagTransform(parent, node->tagIndex);
        base = node->tagIndex * 0x20 + ScrEntity_GetRotTransArray(parent);
        QuatTransform_ToMat3((const QuatTransform_s *)base, (float *)&tag);
        Mat3_Multiply((const float *)&tag, angles_tmp, (float *)out);
        Vec3_TransformAffine((const float *)(c52_ptr(base + 0x10)), angles_tmp, out->origin);
    }
}

// 0x0811e118  size=202  FUN_0811e118  status=translated  naming=unknown
// evidence: no string; sets the link node transform from the entity's attachment orientation.
void Ent_SyncAttachTransform(gentity_s *ent, int mode)
{
    game_orient_s orient;
    game_orient_s inv;
    float angles_tmp[12];                 /* [ebp-0x48]: 4x3 matrix, AnglesToAxis rows then the origin row (+0x24) */
    float *origin_tmp = angles_tmp + 9;   /* the original copies/reads the origin at [ebp-0x48]+0x24 */
    game_attach_node_s *node;

    Ent_GetAttachTransform(ent, &orient);
    AnglesToAxis(ent->r.currentAngles, angles_tmp);
    node = ent->tagInfo;
    if (mode == 0) {
        Rigid_Invert((const float *)&orient, (float *)&inv);
        Vec3_CopyBits_0811d28a(ent->r.currentOrigin, origin_tmp);
        Mat34_ConcatRowVector(angles_tmp, (const float *)&inv, node->origin);
    } else {
        Mat3_Transpose((const float *)&orient, (float *)&inv);
        Mat3_Multiply(angles_tmp, (const float *)&inv, node->origin);
    }
}

// 0x0811e1e2  size=308  FUN_0811e1e2  status=translated  naming=unknown
// evidence: no string; mode 0/1/2 copy the attachment origin, angles or length into the entity.
void Ent_SetAttachCurrent(gentity_s *ent, int mode)
{
    game_orient_s orient;
    float angles_tmp[12];                 /* [ebp-0x48]: 4x3 matrix, AnglesToAxis rows then the origin row (+0x24) */
    float *origin_tmp = angles_tmp + 9;   /* the original copies/reads the origin at [ebp-0x48]+0x24 */
    game_attach_node_s *node;
    float len;

    Ent_GetAttachTransform(ent, &orient);
    node = ent->tagInfo;
    if (mode == 1) {
        Mat34_ConcatRowVector(node->origin, (const float *)&orient, angles_tmp);
        Vec3_CopyBits_0811d28a(origin_tmp, ent->r.currentOrigin);
        len = Vec2_AngleDegrees(angles_tmp);
        ent->r.currentAngles[1] = len;
    } else if (mode < 2) {
        if (mode == 0) {
            Mat34_ConcatRowVector(node->origin, (const float *)&orient, angles_tmp);
            Vec3_CopyBits_0811d28a(origin_tmp, ent->r.currentOrigin);
            AxisToAngles(angles_tmp, ent->r.currentAngles);
        }
    } else if (mode == 2) {
        Vec3_TransformAffine(node->angles, (const float *)&orient, origin_tmp);
        Vec3_CopyBits_0811d28a(origin_tmp, ent->r.currentOrigin);
    }
}

// 0x0811e316  size=104  FUN_0811e316  status=translated  naming=unknown
// evidence: no string; resets the attachment state and both trajectory types to linear.
void Ent_ResetAttachState(gentity_s *ent)
{
    Ent_SetAttachCurrent(ent, 0);
    G_SetPositionTrajectory(ent, ent->r.currentOrigin);
    G_SetAnglesTrajectory(ent, ent->r.currentAngles);
    ent->s.pos.trType = 1;
    ent->s.apos.trType = 1;
    SV_LinkEntity(ent);
}

// 0x0811e37e  size=21  FUN_0811e37e  status=translated  naming=unknown
// evidence: no string; unlinks the entity by number.
void Ent_UnlinkEntity(gentity_s *ent)
{
    Com_ServerDObjFree(ent->s.number);
}

// 0x0811e394  size=44  FUN_0811e394  status=translated  naming=unknown
// evidence: no string; calls SV_DObjBindingSetWeight with the frame interval in seconds.
int Ent_UpdateTimeScaled(struct gentity_s *ent, int arg)
{
    return SV_DObjBindingSetWeight(ent, (double)(float)level_frameTime * 0.001, arg);
}

// 0x0811e3c0  size=148  FUN_0811e3c0  status=translated  naming=unknown
// evidence: no string; runs the kind callback at g_08187d1c after a state query.
void Ent_RunKindCallback(gentity_s *ent)
{
    uint8 buf[28];
    c52_kind_cb_t cb;
    int r;

    memset(buf, 0xff, 0x10);
    r = ScrEntity_LockBonesOrCreateSkel(ent, (int)buf);
    if (r == 0) {
        SV_DObjBindingRebuild(ent, (int)buf);
        cb = *(c52_kind_cb_t *)(g_08187d1c + (uint32)ent->handler * 0x28);
        if (cb != NULL) {
            cb(ent, buf);
        }
        SV_DObjCalcSkel(ent, (int)buf);
    }
}

// 0x0811e454  size=146  FUN_0811e454  status=translated  naming=unknown
// evidence: no string; tag transform refresh with the same kind callback as Ent_RunKindCallback.
void Ent_UpdateTagTransform(gentity_s *ent, int tag)
{
    uint8 buf[28];
    c52_kind_cb_t cb;

    if (SV_GetSkelForModel(ent, tag) == 0) {
        SV_DObjGetHierarchyBits(ent, tag, (int)buf);
        SV_DObjBindingRebuild(ent, (int)buf);
        cb = *(c52_kind_cb_t *)(g_08187d1c + (uint32)ent->handler * 0x28);
        if (cb != NULL) {
            cb(ent, buf);
        }
        SV_DObjCalcSkel(ent, (int)buf);
    }
}

// 0x0811e4e6  size=91  FUN_0811e4e6  status=translated  naming=unknown
// evidence: no string; returns the tag's slot base address, 0 when unresolved.
int32 Ent_GetSlotBase(gentity_s *ent, int32 name)
{
    int32 idx = SV_DObjGetBoneIndexByName(ent, name);
    int32 result;
    if (idx < 0) {
        result = 0;
    } else {
        Ent_UpdateTagTransform(ent, idx);
        result = idx * 0x20 + ScrEntity_GetRotTransArray(ent);
    }
    return result;
}

// 0x0811e542  size=183  FUN_0811e542  status=translated  naming=unknown
// evidence: no string; writes the slot orientation into out (48 + 24 bytes).
bool Ent_SetNodeFromSlot(gentity_s *ent, int32 name, int32 out)
{
    float angles_tmp[12];                 /* [ebp-0x48]: 4x3 matrix, AnglesToAxis rows then the origin row (+0x24) */
    float *origin_tmp = angles_tmp + 9;   /* the original copies/reads the origin at [ebp-0x48]+0x24 */
    game_orient_s orient;
    int32 slot;
    bool ok;

    slot = Ent_GetSlotBase(ent, name);
    ok = slot != 0;
    if (ok) {
        AnglesToAxis(ent->r.currentAngles, angles_tmp);
        Vec3_CopyBits_0811d28a(ent->r.currentOrigin, origin_tmp);
        QuatTransform_ToMat3((const QuatTransform_s *)slot, (float *)&orient);
        Mat3_Multiply((const float *)&orient, angles_tmp, (float *)c52_ptr(out));
        Vec3_TransformAffine((const float *)(c52_ptr(slot + 0x10)), angles_tmp, (float *)(c52_ptr(out + 0x24)));
    }
    return ok;
}

// 0x0811e5fa  size=131  FUN_0811e5fa  status=translated  naming=unknown
// evidence: no string; writes the slot origin into out.
bool Ent_SetNodeOrigin(gentity_s *ent, int32 name, float *out)
{
    float angles_tmp[12];                 /* [ebp-0x48]: 4x3 matrix, AnglesToAxis rows then the origin row (+0x24) */
    float *origin_tmp = angles_tmp + 9;   /* the original copies/reads the origin at [ebp-0x48]+0x24 */
    int32 slot;
    bool ok;

    slot = Ent_GetSlotBase(ent, name);
    ok = slot != 0;
    if (ok) {
        AnglesToAxis(ent->r.currentAngles, angles_tmp);
        Vec3_CopyBits_0811d28a(ent->r.currentOrigin, origin_tmp);
        Vec3_TransformAffine((const float *)(c52_ptr(slot + 0x10)), angles_tmp, (float *)(c52_ptr(out)));
    }
    return ok;
}

// 0x0811e67e  size=132  FUN_0811e67e  status=translated  naming=unknown
// evidence: no string; scans in-use entities for an int16 field at the given offset.
gentity_s *G_FindEntityByShortField(gentity_s *start, int32 offset, int16 value)
{
    uint8 *p;
    uint8 *end = (uint8 *)g_gentities + (int32)level_numEntities * 0x230;

    if (start == NULL) {
        p = (unsigned char*)g_gentities;
    } else {
        p = (uint8 *)start + 0x230;
    }
    while (1) {
        if (end <= p) {
            return NULL;
        }
        if (p[0xfc] != 0 && *(int16 *)(p + offset) != 0 && *(int16 *)(p + offset) == value) {
            break;
        }
        p += 0x230;
    }
    return (gentity_s *)p;
}

// 0x0811e702  size=135  FUN_0811e702  status=translated  naming=unknown
// evidence: no string; scans in-use entities for a scr-string field equal to value.
gentity_s *G_FindEntityByStringField(gentity_s *start, int32 offset, int32 value)
{
    uint8 *p;
    uint8 *end = (uint8 *)g_gentities + (int32)level_numEntities * 0x230;

    if (start == NULL) {
        p = (unsigned char*)g_gentities;
    } else {
        p = (uint8 *)start + 0x230;
    }
    while (1) {
        if (end <= p) {
            return NULL;
        }
        if (p[0xfc] != 0 && *(int32 *)(p + offset) != 0 &&
            Q_stricmp((const char *)(*(int32 *)(p + offset)), (const char *)value) == 0) {
            break;
        }
        p += 0x230;
    }
    return (gentity_s *)p;
}

// 0x0811e85c  size=119  FUN_0811e85c  status=translated  naming=unknown
// evidence: no string; resets an entity record and derives its number from its pool address.
void G_InitEntityRecord(gentity_s *e)
{
    e->nextFree = 0;
    e->r.inuse = 1;
    ScrStr_AssignRef(&e->classname, scrStr_noclass);
    e->s.number = (int32)((uint32)(((int32)((uintptr_t)e - (uintptr_t)g_gentities) /* binary: e - 0x08716400 */) >> 4) * 0x8af8af8bu);
    e->r.ownerNum = 0x3ff;
    e->eventTime = 0;
    e->freeAfterEvent = 0;
}

// 0x0811e8d4  size=179  FUN_0811e8d4  status=translated  naming=observed
// evidence: format "%4i: '%s', origin: %f %f %f\n" in the function.
void G_PrintEntities(void)
{
    int32 i;
    const char *name;
    gentity_s *e;

    for (i = 0; i < level_numEntities; i++) {
        e = C52_ENT(i);
        if (e->classname == 0) {
            name = (const char *)g_0815ece3;
        } else {
            name = ScrStr_TextFromHandle(e->classname);
        }
        Com_Printf("%4i: '%s', origin: %f %f %f\n", i, name,
                    (double)e->r.currentOrigin[0], (double)e->r.currentOrigin[1], (double)e->r.currentOrigin[2]);
    }
}

// 0x0811e988  size=77  FUN_0811e988  status=translated  naming=unknown
// evidence: no string; returns 1 when the free-list head may be reused (old enough, or pool full).
int G_FreeHeadIsReusable(const gentity_s *head)
{
    int result;
    if (head == NULL) {
        result = 0;
    } else if (g_0864c56c - head->eventTime < 500 && level_numEntities < 0x3fe) {
        result = 0;
    } else {
        result = 1;
    }
    return result;
}

// 0x0811e9d6  size=205  FUN_0811e9d6  status=translated  naming=unknown
// evidence: no string; spawns from the free list or grows the pool (reports at 0x3fe).
gentity_s *G_Spawn(void)
{
    int32 head = level_freeEntityHead;
    int32 local = level_freeEntityHead;

    if (G_FreeHeadIsReusable(C52_ENT_FROM_PTR(c52_ptr(level_freeEntityHead))) == 0) {
        if (level_numEntities == 0x3fe) {
            G_PrintEntities();
            Com_Error(1, (const char *)g_0815ed01);
        }
        local = level_numEntities * 0x230 + c52_int(level_gentities);
        level_numEntities = level_numEntities + 1;
        SV_InitGameEntityLayout((unsigned char *)(c52_ptr(level_gentities)), level_numEntities, 0x230, (unsigned char *)(c52_ptr(level_clients)), 0x28a4);
    } else {
        level_freeEntityHead = *(int32 *)((uint8 *)c52_ptr(level_freeEntityHead) + 0x22c);
        if (level_freeEntityHead == 0) {
            level_freeEntityTail = 0;
        }
        *(int32 *)((uint8 *)c52_ptr(head) + 0x22c) = 0;
    }
    G_InitEntityRecord(C52_ENT_FROM_PTR(c52_ptr(local)));
    return C52_ENT_FROM_PTR(c52_ptr(local));
}

// 0x0811eaa4  size=153  FUN_0811eaa4  status=translated  naming=unknown
// evidence: no string; temp-entity ring of 8 at pool index 64 (offset 0x8c00).
gentity_s *G_AllocTempEntity(void)
{
    int32 base = g_tempEntityRingIndex * 0x230 + c52_int(level_gentities);
    int32 ent = base + 0x8c00;
    int32 next = g_tempEntityRingIndex + 1;
    int32 local14 = next;
    uint32 savedFlags;

    if (next < 0) {
        local14 = g_tempEntityRingIndex + 8;
    }
    g_tempEntityRingIndex = next + (local14 >> 3) * -8;
    savedFlags = *(uint32 *)((uint8 *)c52_ptr(base) + 0x8c08);
    if (*(char *)((uint8 *)c52_ptr(base) + 0x8cfc) != '\0') {
        G_FreeEntity(C52_ENT_FROM_PTR(c52_ptr(ent)));
    }
    G_InitEntityRecord(C52_ENT_FROM_PTR(c52_ptr(ent)));
    *(uint32 *)((uint8 *)c52_ptr(base) + 0x8c08) = (savedFlags & 2u) ^ 2u;
    return C52_ENT_FROM_PTR(c52_ptr(ent));
}

// 0x0811eb3e  size=385  FUN_0811eb3e  status=translated  naming=unknown
// evidence: no string; clears references to a freed entity from entities, clients and the pair table.
void G_ClearEntityReferences(gentity_s *target)
{
    int32 num = target->s.number;
    int32 c;
    gentity_s *e;
    gclient_s *cl;

    for (c = 0; c < level_numEntities; c++) {
        e = C52_ENT(c);
        if (e->r.inuse != 0) {
            if (e->parent == target) {
                e->parent = NULL;
            }
            if (e->r.ownerNum == num) {
                e->r.ownerNum = 0x3ff;
                if (e->s.eType == 9) {
                    e->active = 0;
                }
            }
            if (e->s.groundEntityNum == num) {
                e->s.groundEntityNum = 0x3ff;
            }
        }
    }
    for (c = 0; c < 0x40; c++) {
        e = C52_ENT(c);
        if (e->r.inuse != 0) {
            cl = e->client;
            if (cl->pLookatEnt == target) {
                cl->pLookatEnt = NULL;
            }
            if (cl->useHoldEntity == num) {
                cl->useHoldEntity = 0x3ff;
            }
            if (cl->ps.cursorHintEntIndex == num) {
                cl->ps.cursorHintEntIndex = 0x3ff;
            }
        }
    }
    for (c = 0; c < 0x20; c++) {
        if (c52_ptr(g_droppedWeapons[c]) == (void *)target) {
            g_droppedWeapons[c] = 0;
        }
    }
}

// 0x0811ecc0  size=76  FUN_0811ecc0  status=translated  naming=unknown
// evidence: no string; index of the tracked slot whose number matches; 0 when none.
int G_FindTrackedSlot(const gentity_s *e)
{
    int c = 0;
    while (1) {
        if (7 < c) {
            return 0;
        }
        if (*(int32 *)(g_0884e7bc + c * 0x4c8) == e->s.number) {
            break;
        }
        c++;
    }
    return c;
}

// 0x0811ed0c  size=281  FUN_0811ed0c  status=translated  naming=unknown
// evidence: no string; picks the tracked slot farthest from a reference point,
// or the first empty slot, then frees the chosen entity.
int G_PickTrackedSlot(void)
{
    int32 best = 0;
    float bestDist = -1.0f;
    float ref[3];
    float dist;
    int32 i;
    gentity_s *e;

    e = G_FindEntityByShortField(NULL, 0x168, (int16)scrStr_player);
    Vec3_CopyBits_0811d28a(e->s.pos.trBase, ref);
    for (i = 0; ; i++) {
        if (7 < i) {
            e = C52_ENT(*(int32 *)(g_0884e7bc + best * 0x4c8));
            G_FreeEntity(e);
            *(int32 *)(g_0884e7bc + best * 0x4c8) = -1;
            return best;
        }
        if (*(int32 *)(g_0884e7bc + i * 0x4c8) == -1) {
            break;
        }
        e = C52_ENT(*(int32 *)(g_0884e7bc + i * 0x4c8));
        dist = Vec3_DistanceSquared(e->r.currentOrigin, ref);
        if (bestDist < dist) {
            best = i;
            bestDist = dist;
        }
    }
    return i;
}

// 0x0811ee26  size=41  FUN_0811ee26  status=translated  naming=unknown
// evidence: no string; clears the tracked slot holding the entity.
void G_ClearTrackedSlotFor(gentity_s *e)
{
    int idx = G_FindTrackedSlot(e);
    *(int32 *)(g_0884e7bc + idx * 0x4c8) = -1;
}

// 0x0811ee50  size=320  FUN_0811ee50  status=translated  naming=inferred
// evidence: Quake III G_FreeEntity counterpart (unlink children, clear refs, requeue
// on the free list with generation+1); no string.
void G_FreeEntity(gentity_s *e)
{
    int32 saved;
    int32 num;

    Ent_UnlinkFromParent(e);
    while (e->tagChildren != NULL) {
        Ent_UnlinkFromParent(e->tagChildren);
    }
    SV_UnlinkEntity(e);
    saved = SV_DObjGetTree(e);
    if (saved != 0) {
        XAnim_ResetTree((XAnimTree *)saved);
    }
    Com_ServerDObjFree(e->s.number);
    G_ClearEntityReferences(e);
    if (e->pTurretInfo != NULL) {
        Turret_Reset(e);
    }
    if (e->s.eType == 2) {
        G_ClearTrackedSlotFor(e);
    }
    G_FreeScriptEntity(e);
    saved = e->spawnSerial;
    memset(e, 0, 0x230);
    e->eventTime = g_0864c56c;
    /* Entity number from pool offset (same expression as G_InitEntityRecord, against level_gentities). */
    num = (int32)((uint32)(((int32)((uintptr_t)e - (uintptr_t)(uint32)level_gentities)) >> 4) * 0x8af8af8bu);
    if (num > 0x47) {
        if (level_freeEntityTail == 0) {
            level_freeEntityHead = c52_int(e);
        } else {
            *(int32 *)((uint8 *)c52_ptr(level_freeEntityTail) + 0x22c) = c52_int(e);
        }
        level_freeEntityTail = c52_int(e);
        e->nextFree = 0;
    }
    e->spawnSerial = saved + 1;
}

// 0x0811ef90  size=52  FUN_0811ef90  status=translated  naming=unknown
// evidence: no string; sends the entity event value through Scr_CallEntityMethod and Scr_ReleaseHandle.
void G_CallDeleteScript(gentity_s *e)
{
    uint16 v = Scr_CallEntityMethod(e, g_deleteScriptMainOffset, 0);
    Scr_ReleaseHandle((int)v);
}

// 0x0811efc4  size=239  FUN_0811efc4  status=translated  naming=unknown
// evidence: no string; spawns an entity with type = event + 10 at a truncated origin.
gentity_s *G_TempEntity(const float *origin, int32 eventType)
{
    gentity_s *e = G_Spawn();
    float p[3];

    e->s.eType = eventType + 10;
    ScrStr_AssignRef(&e->classname, scrStr_tempEntity);
    e->eventTime = g_0864c56c;
    e->r.eventTime = g_0864c56c;
    e->freeAfterEvent = 1;
    Vec3_CopyBits_0811d28a(origin, p);
    p[0] = (float)(int32)p[0];
    p[1] = (float)(int32)p[1];
    p[2] = (float)(int32)p[2];
    G_SetPositionTrajectory(e, p);
    SV_LinkEntity(e);
    return e;
}

// 0x0811f1fc  size=53  FUN_0811f1fc  status=translated  naming=unknown
// evidence: no string; forwards two values to the client when present.
void Ent_ForwardToClient(gentity_s *e, int32 a, int32 b)
{
    if (e->client != NULL) {
        BG_AddPredictableEventToPlayerstate(a, b, &e->client->ps);
    }
}

// 0x0811f232  size=195  FUN_0811f232  status=translated  naming=inferred
// evidence: Quake III G_AddEvent counterpart (ring of four events with a sequence
// counter, stored on the client when present, else on the entity); no string.
void G_AddEvent(gentity_s *e, int32 event, int32 parm)
{
    if (e->client == NULL) {
        e->s.events[e->s.eventSequence & 3] = event;
        e->s.eventParms[e->s.eventSequence & 3] = parm;
        e->s.eventSequence = e->s.eventSequence + 1;
    } else {
        gclient_s *cl = e->client;
        cl->ps.events[cl->ps.eventSequence & 3] = event;
        cl->ps.eventParms[cl->ps.eventSequence & 3] = parm;
        cl->ps.eventSequence = cl->ps.eventSequence + 1;
    }
    e->eventTime = g_0864c56c;
    e->r.eventTime = g_0864c56c;
}

// 0x0811f32e  size=47  FUN_0811f32e  status=translated  naming=unknown
// evidence: no string; event 0xb3 with the value when non-zero.
void Ent_AddEventB3IfNonzero(gentity_s *e, uint8 value)
{
    if (value != 0) {
        G_AddEvent(e, 0xb3, (int32)(uint32)value);
    }
}

// 0x0811f35e  size=47  FUN_0811f35e  status=translated  naming=unknown
// evidence: no string; event 0xb4 with the value when non-zero.
void Ent_AddEventB4IfNonzero(gentity_s *e, uint8 value)
{
    if (value != 0) {
        G_AddEvent(e, 0xb4, (int32)(uint32)value);
    }
}

// 0x0811f3c6  size=96  FUN_0811f3c6  status=translated  naming=unknown
// evidence: no string; sets the position trajectory to a stationary origin.
void G_SetPositionTrajectory(gentity_s *e, const float *origin)
{
    Vec3_CopyBits_0811d28a(origin, e->s.pos.trBase);
    e->s.pos.trType = 0;
    e->s.pos.trTime = 0;
    e->s.pos.trDuration = 0;
    Vec3_Zero_0811f562(e->s.pos.trDelta);
    Vec3_CopyBits_0811d28a(origin, e->r.currentOrigin);
}

// 0x0811f426  size=96  FUN_0811f426  status=translated  naming=unknown
// evidence: no string; sets the angular trajectory to stationary angles.
void G_SetAnglesTrajectory(gentity_s *e, const float *angles)
{
    Vec3_CopyBits_0811d28a(angles, e->s.apos.trBase);
    e->s.apos.trType = 0;
    e->s.apos.trTime = 0;
    e->s.apos.trDuration = 0;
    Vec3_Zero_0811f562(e->s.apos.trDelta);
    Vec3_CopyBits_0811d28a(angles, e->r.currentAngles);
}

// 0x0811f486  size=151  FUN_0811f486  status=translated  naming=unknown
// evidence: no string; true when b lies in front of a's facing direction.
bool G_IsFacingTarget(gentity_s *a, gentity_s *b)
{
    float fwd[8];
    float dir[6];
    long double d;

    Angles_ToVectors(a->r.currentAngles, fwd, 0, 0);
    Vec3_Sub_0811f600(b->r.currentOrigin, a->r.currentOrigin, dir);
    Vec3_Normalize(dir);
    d = Vec3_Dot_0811f644(dir, fwd);
    return 0.0 < (float)d;
}

// 0x0811f51e  size=10  FUN_0811f51e  status=translated  naming=unknown
// evidence: no string; constant zero result.
int G_ReturnZero(void)
{
    return 0;
}

// 0x0811f528  size=57  FUN_0811f528  status=translated  naming=unknown
// evidence: no string; resets a scr-string reference and stores the looked-up value.
void G_SetScrNameRef(uint16 *ref, int32 value)
{
    uint16 v;
    ScrStr_AssignRef(ref, 0);
    v = ScrStr_InternCString((const char *)value, 0);
    *ref = v;
}

// 0x0811f562  size=41  FUN_0811f562  status=translated  naming=unknown
// evidence: no string; zeroes a vector.
void Vec3_Zero_0811f562(void *v)
{
    uint32 *p = (uint32 *)v;
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
}

// 0x0811f58c  size=47  FUN_0811f58c  status=translated  naming=unknown
// evidence: no string; copies three 32-bit words (second vector copy form).
void Vec3_CopyBits_0811f58c(const void *src, void *dst)
{
    const uint32 *s = (const uint32 *)src;
    uint32 *d = (uint32 *)dst;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

// 0x0811f5bc  size=68  FUN_0811f5bc  status=translated  naming=unknown
// evidence: no string; out = a + b per axis (second add form).
void Vec3_Add_0811f5bc(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x0811f600  size=68  FUN_0811f600  status=translated  naming=unknown
// evidence: no string; out = a - b per axis.
void Vec3_Sub_0811f600(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x0811f644  size=51  FUN_0811f644  status=translated  naming=unknown
// evidence: no string; dot product in x87 extended precision.
long double Vec3_Dot_0811f644(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2] +
           (long double)a[1] * (long double)b[1] +
           (long double)a[0] * (long double)b[0];
}

// 0x0811f678  size=56  FUN_0811f678  status=translated  naming=unknown
// evidence: no string; out = s * v per axis.
void Vec3_Scale_0811f678(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x0811f6b0  size=404  FUN_0811f6b0  status=translated  naming=unknown
// evidence: no string; traces up to five spread rays from the muzzle; on the first
// ray applies the area effect; stops at the first non-passing hit.
int Bullet_TraceSpread(gentity_s *shooter, game_orient_s *muzzle, int32 p3, float p4,
                       float spreadX, float spreadY, trace_s *trace, float *point)
{
    int32 count;
    int32 i;
    float dir[3];

    if (0.0 < spreadX || 0.0 < spreadY) {
        count = 5;
    } else {
        count = 1;
    }
    for (i = 0; ; i++) {
        if (count <= i) {
            return 0;
        }
        Vec3_MultiplyAdd_08120f6e(muzzle->origin, p4, muzzle->axis[0], dir);
        Vec3_MultiplyAdd_08120f6e(dir, spreadX * c52_table_f(g_0815ed20, i), muzzle->axis[1], dir);
        Vec3_MultiplyAdd_08120f6e(dir, spreadY * c52_table_f(g_0815ed24, i), muzzle->axis[2], dir);
        G_TracePointEx(trace, muzzle->origin, dir, shooter->s.number, 0x2802831, (const char *)g_08187c0c);
        VectorLerp(muzzle->origin, dir, trace->fraction, point);
        if (i == 0) {
            Ent_AreaEffectApply(shooter, muzzle->origin, point, p3, 7);
        }
        if ((trace->surfaceFlags & 0x10u) == 0 && (double)trace->fraction != 1.0) {
            break;
        }
    }
    return 1;
}

// 0x0811f844  size=389  FUN_0811f844  status=translated  naming=unknown
// evidence: no string; fires one spread trace for the shooter, spawns the impact
// temp entity, and applies damage with a rand()-based value to the hit entity.
void Bullet_FireFromEntity(gentity_s *shooter, game_orient_s *muzzle,
                           float p3, float spreadX, float spreadY)
{
    trace_s trace;
    float point[5];
    gentity_s *hit;
    gentity_s *temp;
    void *slot;
    int32 range;
    int r;

    slot = BG_GetWeaponDef(shooter->s.weapon);
    range = *(int32 *)((uint8 *)slot + 500);
    if (Bullet_TraceSpread(shooter, muzzle, range, p3, spreadX, spreadY, &trace, point) != 0) {
        hit = C52_ENT(trace.entityNum);
        if (hit->client == NULL) {
            temp = G_TempEntity(point, 0xae);
        } else {
            temp = G_TempEntity(point, 0xad);
        }
        temp->s.otherEntityNum = trace.entityNum;
        temp->s.eventParm = Normal_DirToIndex(trace.normal) & 0xff;
        temp->s.weapon = shooter->s.weapon;
        if (trace.entityNum != 0x3fe && hit->takedamage != 0) {
            r = rand();
            /* hit location = the trace's part group (movzx word [ebp-0x18], trace at [ebp-0x38] + 0x20; 0x0811f97d) */
            G_Damage(hit, shooter, shooter, (const float *)muzzle, point, r % 5 + range, 0, 7, trace.partGroup, 0);
        }
    }
}

// 0x0811f9ca  size=166  FUN_0811f9ca  status=translated  naming=unknown
// evidence: no string; per axis floor if bound <= v, else ceil.
void Vec3_FloorOrCeilTowards(float *v, const float *bound)
{
    int i;
    double d;
    for (i = 0; i < 3; i++) {
        if (bound[i] <= v[i]) {
            d = floor((double)v[i]);
            v[i] = (float)d;
        } else {
            d = ceil((double)v[i]);
            v[i] = (float)d;
        }
    }
}

// 0x0811fa70  size=87  FUN_0811fa70  status=translated  naming=unknown
// evidence: no string; random 2D direction from two random values.
void Vec2_RandomDirection(float *x, float *y)
{
    long double lv;
    float local_8;
    float local_c;
    float local_10;
    float local_14;

    lv = (long double)Rand_Fraction();
    local_8 = (float)((long double)360.0 * lv);
    lv = (long double)Rand_Fraction();
    local_c = (float)lv;
    /* gameplay-fix(net_clients): 0x0811faa5: the angle is scaled by the double 0.017453292519943295 (0x0815ed50), not the truncated 0.017453292 */
    Math_SinCos((float)((long double)local_8 * 0.017453292519943295), &local_10, &local_14);
    *x = local_c * local_14;
    *y = local_c * local_10;
}

// 0x0811fac8  size=180  FUN_0811fac8  status=translated  naming=unknown
// evidence: no string; spread direction from tan(angle) and a random 2D offset.
void Bullet_ApplySpread(float angleDeg, float *out, const game_orient_s *o, float scale)
{
    float t;
    float scaled;
    float localX;
    float localY;

    t = (float)tan((double)((long double)angleDeg * 0.017453292519943295)); /* degrees->radians constant is the double at 0x0815ed58 */
    scaled = t * scale;
    Vec2_RandomDirection(&localX, &localY);
    localX = localX * scaled;
    localY = localY * scaled;
    Vec3_MultiplyAdd_08120f6e(o->origin, scale, o->axis[0], out);
    Vec3_MultiplyAdd_08120f6e(out, localX, o->axis[1], out);
    Vec3_MultiplyAdd_08120f6e(out, localY, o->axis[2], out);
}

// 0x0811fb7c  size=249  FUN_0811fb7c  status=translated  naming=unknown
// evidence: no string; distance falloff between the weapon's near and far ranges.
int Weapon_DamageForDistance(game_bullet_ctx_s *ctx, float dist)
{
    weaponDef_s *w = ctx->weapon;
    int result;
    float span;

    if (dist < w->maxDamageRange) {
        result = w->damage;
    } else if (dist < w->minDamageRange) {
        span = w->minDamageRange - w->maxDamageRange;
        if (span == 0.0f) {
            result = w->damage;
        } else {
            result = (int32)Math_Lerp((float)w->damage, (float)w->minDamage,
                                        (dist - w->maxDamageRange) / span);
        }
    } else {
        result = w->minDamage;
    }
    return result;
}

// 0x0811fc76  size=373  FUN_0811fc76  status=translated  naming=unknown
// evidence: no string; snapshots client origins into a 0x340-byte buffer for clients
// that are active and not moving, relinking them at the captured position.
void G_AntilagRewindClients(int32 time, uint8 *buf)
{
    int32 c;
    uint8 scratch[24];
    gclient_s *cl;

    if (cv_g_antilag->current.enabled != 0) {
        memset(buf, 0, 0x340);
        if ((int32)(1000LL / (int64)*(int32 *)((uint8 *)c52_ptr(cv_sv_fps) + 8)) < g_0864c56c - time) {
            for (c = 0; c < level_maxclients; c++) {
                cl = C52_CLIENT(c);
                /* gameplay-fix(net_clients): 0x0811fcf8/0x0811fd16: first test is gclient+0x26c4 (connected) == 2, second is +0x26a8 (sessionState) == 0 */
                if (cl->connected == 2 && cl->sessionState == 0 &&
                    SV_GetInterpolatedPlayerOrigin((int)c, time, (float *)scratch) != '\0') {
                    memcpy(buf + c * 0xc, C52_ENT(c)->r.currentOrigin, 0xc);
                    SV_UnlinkEntity(C52_ENT(c));
                    memcpy(C52_ENT(c)->r.currentOrigin, scratch, 0xc);
                    SV_LinkEntity(C52_ENT(c));
                    buf[0x300 + c] = 1;
                }
            }
        }
    }
}

// 0x0811fdec  size=163  FUN_0811fdec  status=translated  naming=unknown
// evidence: no string; restores client origins captured by G_AntilagRewindClients.
void G_AntilagRestoreClients(uint8 *buf)
{
    int32 c;
    if (cv_g_antilag->current.enabled != 0) {
        for (c = 0; c < level_maxclients; c++) {
            if (buf[0x300 + c] != '\0') {
                SV_UnlinkEntity(C52_ENT(c));
                memcpy(C52_ENT(c)->r.currentOrigin, buf + c * 0xc, 0xc);
                SV_LinkEntity(C52_ENT(c));
            }
        }
    }
}

// 0x0811fe90  size=1348  FUN_0811fe90  status=translated  naming=observed
// evidence: format string "Bullet_Fire_Extended: Too many resursions, bullet aborted\n"
// in the function (the misspelling is in the binary); scr_friendlyfire check.
void Bullet_Fire_Extended(gentity_s *shooter, gentity_s *skip, float *start, float *end,
                          float power, int32 depth, game_bullet_ctx_s *ctx, int32 *ownerNum,
                          int32 timeBase)
{
    trace_s trace;
    float point[4];
    float reflect[7];
    float dirTmp[6];
    float bounceScale;
    float distance;
    float dotv;
    uint32 flags = 0;
    int32 mod = 0;
    int32 tempEvt;
    int32 tempType;
    int32 dmg;
    int32 tmp;
    const char *surface;
    gentity_s *hit;
    gentity_s *temp;

    if (depth < 0xd) {
        if (ctx->weapon->rifleBullet == 0) {
            mod = 1;
        } else {
            mod = 2;
            flags = 0x20;
        }
        if (ctx->weapon->armorPiercing != 0) {
            flags |= 2;
        }
        if (ctx->weapon->rifleBullet == 0) {
            surface = (const char *)g_08187c0c;
        } else {
            surface = (const char *)g_08187c1f;
        }
        G_TracePointEx(&trace, start, end, shooter->s.number, 0x2802831, surface);
        VectorLerp(start, end, trace.fraction, point);
        Ent_AreaEffectApply(skip, start, point, ctx->weapon->damage, mod);
        hit = C52_ENT(trace.entityNum);
        Vec3_Subtract_08120ef2(end, start, reflect);
        Vec3_Normalize(reflect);
        dotv = Vec3_Dot_08120fc2(reflect, trace.normal);
        Vec3_MultiplyAdd_08120f6e(reflect, (float)(-2.0L * (long double)dotv), trace.normal, reflect);

        if ((trace.surfaceFlags & 4u) == 0 && hit->client == NULL && (double)trace.fraction < 1.0) {
            if (ctx->weapon->weaponClass == 3) {
                tempType = 0xb5;
            } else if (ctx->weapon->rifleBullet == 0) {
                tempType = 0xb6;
            } else {
                tempType = 0xb7;
            }
            if (ctx->weapon->rifleBullet == 0) {
                tempEvt = 0xb6;
            } else {
                tempEvt = 0xb7;
            }
            (void)tempType; /* selected but not stored by the binary's fall-through */
            temp = G_TempEntity(point, tempEvt);
            temp->s.eventParm = Normal_DirToIndex(trace.normal) & 0xff;
            temp->s.scale = Normal_DirToIndex(reflect) & 0xff;
            if (hit->s.eType == 2) {
                tmp = 7;
            } else {
                tmp = (int32)((trace.surfaceFlags & 0x1f00000u) >> 0x14);
            }
            temp->s.surfType = tmp;
            temp->s.otherEntityNum = (uint32)*ownerNum;
        }
        if ((trace.contents & 0x10u) == 0) {
            if (hit->takedamage != 0 && hit != skip) {
                Vec3_Subtract_08120ef2(start, point, dirTmp);
                distance = Vec3_LengthDiscard_08120ff6(dirTmp);
                tmp = Weapon_DamageForDistance(ctx, distance);
                dmg = (int32)((float)tmp * power);
                /* hit location = trace.partGroup (movzx word [ebp-0x18] at 0x081202ff; the bullet trace is at [ebp-0x38]) */
                G_Damage(hit, skip, skip, (const float *)ctx, point, dmg, (int)flags, mod, trace.partGroup,
                            g_0864c56c - timeBase);
                if (hit->client != NULL && (flags & 0x20u) != 0 &&
                    (Dvar_GetInt("scr_friendlyfire") != 0 || OnSameTeam(hit, skip) == 0)) {
                    Bullet_Fire_Extended(hit, skip, point, end, power * 0.5f, depth + 1, ctx,
                                         ownerNum, timeBase);
                }
            }
        } else {
            Vec3_Subtract_08120ef2(end, start, reflect);
            Vec3_Normalize(reflect);
            dotv = Vec3_Dot_08120fc2(trace.normal, reflect);
            if (0.125 <= -(double)dotv) {
                bounceScale = (float)(0.25 / -(double)dotv);
            } else {
                bounceScale = 0.0f;
            }
            Vec3_MultiplyAdd_08120f6e(point, bounceScale, reflect, start);
            Bullet_Fire_Extended(shooter, skip, start, end, power, depth + 1, ctx, ownerNum, timeBase);
        }
    } else {
        Com_DPrintf("Bullet_Fire_Extended: Too many resursions, bullet aborted\n");
    }
}
