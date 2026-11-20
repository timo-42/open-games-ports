#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_053_types.h"

/* Vector used as the half-extent of the use scan box (rodata 0x0815ee4c: 192, 192, 96). */
static const float kUseBoxHalf[3] = { 192.0f, 192.0f, 96.0f };

// 0x081203d4  size=176  FUN_081203d4  status=translated  naming=inferred
// no string evidence; loops over the weapon's pellet count with a per-pellet spread, called only for objType 3 from G_FireWeaponShot
void G_FireShotgunPellets(struct gentity_s *ent, int shotA, struct shot_view_s *view, int shotB, int shotC)
{
    float muzzle[3];
    float dir[3];
    int i;

    Vec3_Copy_08120ec2(view->origin, muzzle);
    for (i = 0; i < view->weapon->shotCount; i++) {
        Bullet_ApplySpread(shotC, dir, view, COD2_F32((uint32)view->weapon->minDamageRange));
        Bullet_Fire_Extended(ent, shotA, muzzle, dir, 1.0f, 0, view, ent, shotB);
    }
}

// 0x08120484  size=206  FUN_08120484  status=translated  naming=inferred
// no string evidence; objType 3 takes the pellet path, otherwise one trace-and-fire along the view with range 8192
void G_FireWeaponShot(struct gentity_s *ent, int aimAngleBits, struct shot_view_s *view,
                      struct gentity_s *ent2, int time)
{
    unsigned char scratch[832];
    float dir[6];

    G_AntilagRewindClients(time, scratch);
    if (view->weapon->weaponClass == 3) {
        G_FireShotgunPellets(ent2, (int)ent, view, time, aimAngleBits);
    } else {
        Bullet_ApplySpread(aimAngleBits, dir, view, COD2_F32(0x46000000));
        Bullet_Fire_Extended(ent2, (int)ent, view->origin, dir, 1.0f, 0, view, ent2, time);
    }
    G_AntilagRestoreClients(scratch);
}

// 0x08120552  size=200  FUN_08120552  status=translated  naming=inferred
// no string evidence; spawns a projectile from the view origin with the weapon's speed and lift, then adds the owner's velocity along the launch direction
struct gentity_s *G_LaunchWeaponMissile(struct gentity_s *ent, int weaponHandle, struct shot_view_s *view)
{
    float launchDir[3];
    struct gentity_s *missile;
    long double dot;

    Vec3_Scale_08120f36(view->forward, (float)view->weapon->projectileSpeed, launchDir);
    launchDir[2] = launchDir[2] + (float)view->weapon->projectileSpeedUp;
    missile = G_SpawnGrenade(ent, view->origin, launchDir, weaponHandle, view->weapon->fuseTime);
    Vec3_Normalize(launchDir);
    dot = Vec3_Dot_08120fc2(ent->client->ps.velocity, launchDir);
    Vec3_MultiplyAdd_08120f6e(missile->s.pos.trDelta, (float)dot, launchDir, missile->s.pos.trDelta);
    return missile;
}

// 0x0812061a  size=311  FUN_0812061a  status=translated  naming=unknown
// no string or callee name; the tan(fov) scale and the -64 velocity push have no named counterpart in this chunk
void G_ComputeFovViewOffset(struct gentity_s *ent, float fov, struct shot_view_s *view)
{
    double t;
    float polar[4];
    float scale;
    float local_18;
    float tmp[4];
    float origin[3];

    polar[1] = 16.0f;
    /* gameplay-fix(net_clients): 0x08120628: fov scaled by the double at 0x0815edc8 (0.017453292519943295) */
    t = tan((double)((long double)fov * 0.017453292519943295));
    scale = (float)((float)t * 16.0);
    Vec2_RandomDirection(polar, &local_18);
    polar[0] = polar[0] * scale;
    local_18 = local_18 * scale;
    Vec3_Scale_08120f36(view->forward, 16.0f, tmp);
    Vec3_MultiplyAdd_08120f6e(tmp, polar[0], view->right, tmp);
    Vec3_MultiplyAdd_08120f6e(tmp, local_18, view->up, tmp);
    Vec3_Normalize(tmp);
    Vec3_Copy_08120ec2(view->origin, origin);
    G_SpawnRocket(ent, origin, tmp);
    if (ent->client != NULL) {
        Vec3_MultiplyAdd_08120f6e(ent->client->ps.velocity, -64.0f, view->forward, ent->client->ps.velocity);
    }
}

// 0x08120752  size=153  FUN_08120752  status=translated  naming=unknown
// no string or callee name; a player-controlled source with a distinct, client-backed target below session type 6 and no blocking result from OnSameTeam
int G_ClientCanTargetEntity(struct gentity_s *ent, struct gentity_s *other)
{
    int r;

    if (ent->takedamage == 0) {
        return 0;
    }
    if (ent == other) {
        return 0;
    }
    if (ent->client == NULL) {
        return 0;
    }
    if (other->client == NULL) {
        return 0;
    }
    if (ent->client->ps.pm_type < 6) {
        r = OnSameTeam(ent, other);
        return (r == 0) ? 1 : 0;
    }
    return 0;
}

// 0x081207ec  size=132  FUN_081207ec  status=translated  naming=inferred
// no string evidence; builds the aim frame (forward/right/up from the base and aim angles) plus the eye origin
void G_GetViewFrame(struct gentity_s *ent, struct shot_view_s *view)
{
    float angles[3];

    Vec3_Copy_08120ec2(ent->client->ps.viewangles, angles);
    angles[0] = ent->client->fGunPitch;
    angles[1] = ent->client->fGunYaw;
    Angles_ToVectors(angles, view->forward, view->right, view->up);
    G_GetPlayerViewOrigin(ent, view->origin);
}

// 0x08120870  size=394  FUN_08120870  status=translated  naming=inferred
// format string "Unknown weapon type %i for %s." at 0x0815ede0 is printed by the default branch; the three branches are the three fire routines
void G_FireWeaponByMode(struct gentity_s *ent, int time)
{
    struct shot_view_s view;
    struct gclient_s *client;
    float local_5c;
    float local_58;
    float local_54;
    float local_50;

    client = ent->client;
    if (((client->ps.eFlags & 0x300) == 0) || (ent->active == 0)) {
        view.weapon = BG_GetWeaponDef(GENT_WEAPON_DEF_HANDLE(ent));
        G_GetViewFrame(ent, &view);
        local_50 = client->currentAimSpreadScale;
        BG_InterpolateWeaponStanceValues(&client->ps, GENT_WEAPON_DEF_HANDLE(ent), &local_58, &local_5c);
        if (client->ps.fWeaponPosFrac == 1.0f) {
            local_54 = view.weapon->adsSpread + (local_5c - view.weapon->adsSpread) * local_50;
        } else {
            local_54 = local_58 + (local_5c - local_58) * local_50;
        }
        if (view.weapon->weaponType == 0) {
            G_FireWeaponShot(ent, COD2_BITS(local_54), &view, ent, time);
        } else if (view.weapon->weaponType == 1) {
            G_LaunchWeaponMissile(ent, GENT_WEAPON_DEF_HANDLE(ent), &view);
        } else if (view.weapon->weaponType == 2) {
            G_ComputeFovViewOffset(ent, local_54, &view);
        } else {
            Com_Error(1, "Unknown weapon type %i for %s.\n", view.weapon->weaponType, view.weapon->szInternalName);
        }
    }
}

// 0x081209fa  size=28  FUN_081209fa  status=translated  naming=inferred
// no string evidence; fires the weapon with the current level time
void G_FireWeaponAtLevelTime(struct gentity_s *ent)
{
    G_FireWeaponByMode(ent, g_0864c56c);
}

// 0x08120a16  size=89  FUN_08120a16  status=translated  naming=inferred
// no string evidence; resolves the current weapon def handle and launches a missile along the aim frame
void G_FireMissileAtTarget(struct gentity_s *ent)
{
    struct shot_view_s view;
    struct gclient_s *client;

    client = ent->client;
    view.weapon = BG_GetWeaponDef(client->ps.offHandIndex);
    G_GetViewFrame(ent, &view);
    G_LaunchWeaponMissile(ent, client->ps.offHandIndex, &view);
}

// 0x08120a70  size=182  FUN_08120a70  status=translated  naming=unknown
// no string or callee name; passes three cvar values (+8 words) to a targeting routine after building the aim frame
void G_UpdateWeaponTargetLock(struct gentity_s *ent)
{
    struct gclient_s *client;
    struct weaponDef_s *weapon;
    /* gameplay-fix(wpn_melee_ads_lean): one 4x3 matrix at [ebp-0x48] (axis rows +0/+0xc/+0x18, origin +0x24); the decompile split it into eye/fwd/right/up (0x08120a70) */
    float orient[12];

    client = ent->client;
    if (((client->ps.eFlags & 0x300) == 0) || (ent->active == 0)) {
        weapon = BG_GetWeaponDef(GENT_WEAPON_DEF_HANDLE(ent));
        (void)weapon;
        G_GetPlayerViewOrigin(ent, orient + 9);
        G_GetPlayerViewAxis(ent, orient, orient + 3, orient + 6);
        Bullet_FireFromEntity(ent, orient, g_087a24c0->current.integer, g_087a24c4->current.integer, g_087a24c8->current.integer);
    }
}

// 0x08120b26  size=590  FUN_08120b26  status=translated  naming=unknown
// no string or callee name; per-client trigger touch bookkeeping over two entity sets and the two use slots, returns 1 when the touch is recorded
int G_TouchTriggerEntity(struct gclient_s *client, int entNum)
{
    char cVar1;
    struct weaponDef_s *obj;
    int local_10;
    int iVar2;
    int result;

    cVar1 = BitSet_Test(CLIENT_TOUCHED_SET(client), entNum);
    if (cVar1 == 0) {
        obj = BG_GetWeaponDef(entNum);
        if (obj->weaponClass == 7) {
            result = 0;
        } else if (obj->weaponClass == 8) {
            result = 0;
        } else {
            BitSet_Set(CLIENT_TOUCHED_SET(client), entNum);
            BitArray_ClearBit(CLIENT_PENDING_SET(client), entNum);
            if (obj->weaponClass == 9) {
                result = 1;
            } else if (obj->offhandClass == 0) {
                if ((unsigned int)(obj->weaponSlot - 1) < 2) {
                    if (client->ps.weaponslots[1] == 0) {
                        client->ps.weaponslots[1] = (unsigned char)entNum;
                    } else if (client->ps.weaponslots[2] == 0) {
                        client->ps.weaponslots[2] = (unsigned char)entNum;
                    }
                }
                local_10 = obj->altWeaponIndex;
                while (local_10 != 0) {
                    cVar1 = BitSet_Test(CLIENT_TOUCHED_SET(client), local_10);
                    if (cVar1 != 0) {
                        break;
                    }
                    BitSet_Set(CLIENT_TOUCHED_SET(client), local_10);
                    BitArray_ClearBit(CLIENT_PENDING_SET(client), entNum);
                    obj = BG_GetWeaponDef(local_10);
                    local_10 = obj->altWeaponIndex;
                }
                result = 1;
            } else {
                if (client->ps.offHandIndex == 0) {
                    client->ps.offHandIndex = entNum;
                    G_SendCharIndexC(client->ps.clientNum, client->ps.offHandIndex);
                } else {
                    iVar2 = BG_GetWeaponTotalAmmo(&client->ps, client->ps.offHandIndex);
                    if (iVar2 < 1) {
                        obj = BG_GetWeaponDef(client->ps.offHandIndex);
                        iVar2 = BG_FindUsableWeaponOfClass(&client->ps, obj->offhandClass);
                        if (iVar2 == 0) {
                            client->ps.offHandIndex = entNum;
                        } else {
                            client->ps.offHandIndex = iVar2;
                        }
                        G_SendCharIndexC(client->ps.clientNum, client->ps.offHandIndex);
                    }
                }
                result = 1;
            }
        }
    } else {
        result = 0;
    }
    return result;
}

// 0x08120d74  size=92  FUN_08120d74  status=translated  naming=observed
// string "Game: G_SetupWeaponDef" is printed by this function between two divider banners; the one-time default weapon load follows it
void G_SetupWeaponDef(void)
{
    Com_DPrintf("----------------------\n");
    Com_DPrintf("Game: G_SetupWeaponDef\n");
    if (bg_numWeapons == 0) {
        SV_SetPendingHandleOne();
        ItemRegistry_Reset();
        BG_InitWeaponRegistry();
        BG_FillInAmmoItems(G_PrecacheWeapon);
        G_LoadWeaponDef("defaultweapon_mp");
    }
    Com_DPrintf("----------------------\n");
}

// 0x08120dd0  size=58  FUN_08120dd0  status=translated  naming=inferred
// called from G_SetupWeaponDef with "defaultweapon_mp"; picks one of two loaders by the level_initializing mode flag
int G_LoadWeaponDef(const char *name)
{
    int result;

    if (level_initializing == 0) {
        result = BG_FindWeaponByName(name);
    } else {
        result = BG_RegisterWeapon(name, G_PrecacheWeapon);
    }
    return result;
}

// 0x08120e0a  size=58  FUN_08120e0a  status=translated  naming=unknown
// format "%c %i" with the character 'a'; the formatted text goes to the send routine on channel chan
void G_SendCharIndexA(int chan, int value)
{
    char *text;

    text = va("%c %i", 0x61, value);
    SV_SendServerCommandNum(chan, 1, text);
}

// 0x08120e44  size=58  FUN_08120e44  status=translated  naming=unknown
// same as G_SendCharIndexA with the character 'C' (0x43)
void G_SendCharIndexC(int chan, int value)
{
    char *text;

    text = va("%c %i", 0x43, value);
    SV_SendServerCommandNum(chan, 1, text);
}

// 0x08120e7e  size=21  FUN_08120e7e  status=translated  naming=inferred
// no string evidence; linear interpolation a + (b - a) * t written as b*t + (1 - t)*a in extended precision
long double Math_Lerp(float a, float b, float t)
{
    return (long double)b * (long double)t + ((long double)1 - (long double)t) * (long double)a;
}

// 0x08120e94  size=45  FUN_08120e94  status=translated  naming=inferred
// no string evidence; computes sine and cosine of one angle through double libm calls and stores them as float
void Math_SinCos(float angle, float *sinOut, float *cosOut)
{
    double d;

    d = sin((double)angle);
    *sinOut = (float)d;
    d = cos((double)angle);
    *cosOut = (float)d;
}

// 0x08120ec2  size=47  FUN_08120ec2  status=translated  naming=inferred
// no string evidence; copies a three-float vector
void Vec3_Copy_08120ec2(const float *src, float *dst)
{
    /* the original moves three raw dwords (no x87 round trip; NaN payloads are preserved) */
    ((volatile unsigned int *)dst)[0] = ((const volatile unsigned int *)src)[0];
    ((volatile unsigned int *)dst)[1] = ((const volatile unsigned int *)src)[1];
    ((volatile unsigned int *)dst)[2] = ((const volatile unsigned int *)src)[2];
}

// 0x08120ef2  size=68  FUN_08120ef2  status=translated  naming=inferred
// no string evidence; out = a - b
void Vec3_Subtract_08120ef2(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x08120f36  size=56  FUN_08120f36  status=translated  naming=inferred
// no string evidence; out = s * v
void Vec3_Scale_08120f36(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x08120f6e  size=83  FUN_08120f6e  status=translated  naming=inferred
// no string evidence; out = a + s * b
void Vec3_MultiplyAdd_08120f6e(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x08120fc2  size=51  FUN_08120fc2  status=translated  naming=inferred
// no string evidence; dot product in extended precision, summed x then y then z as compiled
long double Vec3_Dot_08120fc2(const float *a, const float *b)
{
    return (long double)a[0] * b[0] + (long double)a[1] * b[1] + (long double)a[2] * b[2];
}

// 0x08120ff6  size=62  FUN_08120ff6  status=translated  naming=unknown
// computes the length of a vector and discards the result (no caller reads it in this chunk)
float Vec3_LengthDiscard_08120ff6(const float *v)
{
    return Math_Sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// 0x08121034  size=31  FUN_08121034  status=translated  naming=inferred
// no string evidence; square root through a double libm call, rounded to float before returning
float Math_Sqrt(float v)
{
    double d;
    float f;

    d = sqrt((double)v);
    f = (float)d;
    return f;
}

// 0x08121054  size=288  FUN_08121054  status=translated  naming=inferred
// notify strings "touch" (eType 3) and "trigger" (other types) are sent to the target; the impact table callbacks run; the client's use entity is cleared
void G_NotifyTriggerUse(struct gentity_s *ent, struct gentity_s *other)
{
    cod2_impact_cb_t onUse;
    cod2_impact_cb_t onTouch;

    onUse = g_08187d0c[other->handler].onUse;
    onTouch = g_08187d0c[other->handler].onTouch;
    if (other->s.eType == 3) {
        Scr_AddEntityParam(ent);
        Scr_NotifyEntity(other, scrStr_touch, 1);
        other->active = 1;
        if (onTouch != NULL) {
            onTouch(other, ent, NULL);
        }
    } else if ((other->s.eType != 9) || (Turret_CanBeUsed(other, ent) != 0)) {
        Scr_AddEntityParam(ent);
        Scr_NotifyEntity(other, scr_str_trigger, 1);
        if (onUse != NULL) {
            onUse(other, ent, ent);
        }
    }
    ent->client->useHoldEntity = COD2_ENTITYNUM_NONE;
}

// 0x08121174  size=240  FUN_08121174  status=translated  naming=unknown
// no string or callee name; chooses the client's use target from the stored use-target number and updates the use state byte
int G_UpdateUseTargetState(struct gentity_s *ent)
{
    struct gclient_s *client;

    if (Scr_IsInitialized(1) == 0) {
        return 0;
    }
    client = ent->client;
    client->useHoldEntity = COD2_ENTITYNUM_NONE;
    if (ent->active == 0) {
        if ((client->ps.pm_flags & 4) == 0) {
            if (client->ps.cursorHintEntIndex == COD2_ENTITYNUM_NONE) {
                return 0;
            }
            client->useHoldEntity = client->ps.cursorHintEntIndex;
            client->useHoldTime = g_0864c56c;
            return 1;
        }
        return 1;
    }
    if ((client->ps.eFlags & 0x300) == 0) {
        ent->active = 0;
    } else {
        ent->active = 2;
    }
    return 1;
}

// 0x08121264  size=175  FUN_08121264  status=translated  naming=unknown
// no string or callee name; fires the notify when the use target is set and both timers have elapsed
void G_TryActivateUseTarget(struct gentity_s *ent)
{
    struct gclient_s *client;

    client = ent->client;
    if ((Scr_IsInitialized(1) != 0) && (client->useHoldEntity != COD2_ENTITYNUM_NONE)
        && (g_087a2424->current.integer <= g_0864c56c - client->nextUseTime)
        && (g_087a2420->current.integer <= g_0864c56c - client->useHoldTime)) {
        G_NotifyTriggerUse(ent, &g_gentities[client->useHoldEntity]);
    }
}

// 0x08121314  size=324  FUN_08121314  status=translated  naming=unknown
// no string or callee name; drives the client use flags from the button edge bits 0x20 and 0x28
void G_UpdateUseState(struct gentity_s *ent)
{
    struct gclient_s *client;
    char local5;

    client = ent->client;
    client->ps.pm_flags = client->ps.pm_flags & 0xfffffff7u;
    if ((client->ps.weaponstate < 0x11) || (client->ps.weaponstate > 0x16)) {
        local5 = 0;
        if ((client->useHoldEntity == COD2_ENTITYNUM_NONE)
            || (((client->oldbuttons & 0x20) == 0) || ((client->buttons & 0x20) != 0))) {
            if ((client->latched_buttons & 0x28) != 0) {
                local5 = (char)G_UpdateUseTargetState(ent);
            }
            if ((client->useHoldEntity == COD2_ENTITYNUM_NONE) && (local5 == 0)) {
                if ((client->latched_buttons & 0x20) != 0) {
                    client->ps.pm_flags = client->ps.pm_flags | 8;
                }
            } else if ((client->buttons & 0x28) != 0) {
                G_TryActivateUseTarget(ent);
            }
        } else {
            client->ps.pm_flags = client->ps.pm_flags | 8;
        }
    }
}

// 0x08121458  size=59  FUN_08121458  status=translated  naming=inferred
// used as the qsort comparator for keyed entity lists (the float key sits at +4 of each 8-byte entry)
int G_CompareKeyedEntity(const void *a, const void *b)
{
    const struct keyed_ent_s *pa;
    const struct keyed_ent_s *pb;

    pa = (const struct keyed_ent_s *)a;
    pb = (const struct keyed_ent_s *)b;
    return (int)(pa->key - pb->key);
}

// 0x08121494  size=1558  FUN_08121494  status=translated  naming=inferred
// strings "trigger_use"/"trigger_use_touch" select the two candidate classes; a box scan around the eye feeds two qsort passes keyed by view distance
int G_CollectUseCandidates(struct gentity_s *ent, struct keyed_ent_s *out)
{
    struct gclient_s *client;
    int entList[0x400];
    int penaltyTrace = 0;
    int penaltyAim = 0;
    int count;
    int n;
    int i;
    float eye[4];
    float fwd[6];
    float boxA[3];
    float boxB[3];
    float lo[4];
    float hi[4];
    float center[4];
    float dir[4];
    float len;
    long double dotv;
    float key;
    struct gentity_s *ent2;
    struct gentity_s *c2;
    int num;

    client = ent->client;
    G_GetPlayerViewOrigin(ent, eye);
    G_GetPlayerViewAxis(ent, fwd, NULL, NULL);
    Vec3_Add_0812237a(client->ps.origin, client->ps.mins, boxA);
    Vec3_Add_0812237a(client->ps.origin, client->ps.maxs, boxB);
    Vec3_SubtractAlt(eye, kUseBoxHalf, lo);
    Vec3_Add_0812237a(eye, kUseBoxHalf, hi);
    count = CM_AreaEntities(lo, hi, entList, 0x400, 0x200000);
    n = 0;
    for (i = 0; i < count; i++) {
        num = entList[i];
        ent2 = &g_gentities[num];
        if ((ent != ent2) && ((ent2->s.eType == 3) || ((ent2->r.contents & 0x200000) != 0))) {
            if (ent2->classname == scr_str_trigger_use_touch) {
                if ((ent2->r.absmin[0] <= boxB[0]) && (boxA[0] <= ent2->r.absmax[0])
                    && (ent2->r.absmin[1] <= boxB[1]) && ((boxA[1] <= ent2->r.absmax[1])
                    && (ent2->r.absmin[2] <= boxB[2])) && ((boxA[2] <= ent2->r.absmax[2])
                    && (SV_EntityContact(boxA, boxB, ent2) != 0))) {
                    out[n].ent = ent2;
                    out[n].key = -256.0f;
                    n++;
                }
            } else {
                Vec3_Add_0812237a(ent2->r.absmin, ent2->r.absmax, center);
                Vec3_Scale_08122402(center, 0.5f, center);
                Vec3_SubtractAlt(center, eye, dir);
                len = Vec3_Normalize(dir);
                if (len <= 128.0f) {
                    dotv = Vec3_Dot_0812248e(dir, fwd);
                    key = (float)(1.0 - (((float)dotv + 1.0) * 0.5));
                    out[n].key = (float)(key * 256.0);
                    if (ent2->classname == scr_str_trigger_use) {
                        out[n].key = (float)(out[n].key - 256.0);
                    }
                    if ((ent2->s.eType == 3) && (BG_CanItemBeGrabbed(&ent2->s, &client->ps, 0) == 0)) {
                        out[n].key = (float)(out[n].key + 10000.0);
                        penaltyTrace++;
                    }
                    out[n].ent = ent2;
                    out[n].key = (float)(out[n].key + len);
                    n++;
                }
            }
        }
    }
    qsort(out, (size_t)n, 8, G_CompareKeyedEntity);
    n = n - penaltyTrace;
    for (i = 0; i < n; i++) {
        c2 = out[i].ent;
        if (c2->classname != scr_str_trigger_use_touch) {
            Vec3_Add_0812237a(c2->r.absmin, c2->r.absmax, center);
            Vec3_Scale_08122402(center, 0.5f, center);
            if (c2->s.eType == 9) {
                Ent_SetNodeOrigin(c2, scrStr_tag_aim, center);
            }
            if (G_TraceIsClearBox(eye, vec3_origin, vec3_origin, center, client->ps.clientNum, 0x11) == 0) {
                out[i].key = (float)(out[i].key + 10000.0);
                penaltyAim++;
            }
        }
    }
    qsort(out, (size_t)n, 8, G_CompareKeyedEntity);
    return n - penaltyAim;
}

// 0x08121aaa  size=139  FUN_08121aaa  status=translated  naming=unknown
// no string or callee name; reads a 0x2c-byte entry for the candidate's weapon index and returns the target number + 4 when it is free
int G_CheckUseCandidateEntity(struct gclient_s *client, struct gentity_s *ent)
{
    int idx;
    int result;
    int kind;
    int tgt;
    struct weaponDef_s *obj;
    char cVar2;

    idx = (int)((unsigned int)ent->u.item.index * 0x2c);
    result = 0;
    kind = *(const int *)(g_081850dc + idx);
    if (kind == 1) {
        tgt = *(const int *)(g_081850dc + idx + 4);
        obj = BG_GetWeaponDef(tgt);
        if (obj->weaponType != 1) {
            cVar2 = BitSet_Test(CLIENT_TOUCHED_SET(client), tgt);
            if (cVar2 == 0) {
                result = tgt + 4;
            }
        }
    }
    return result;
}

// 0x08121b36  size=143  FUN_08121b36  status=translated  naming=unknown
// no string or callee name; when the current weapon's record has a non-empty name string, copies the weapon's handle and slot into the client
void G_SetUseTargetFromSlot(struct gentity_s *ent)
{
    struct gclient_s *client;
    struct gentity_s *went;
    struct weaponDef_s *obj;

    client = ent->client;
    went = level_gentities + client->ps.viewlocked_entNum;
    obj = BG_GetWeaponDef(GENT_WEAPON_DEF_HANDLE(went));
    if (*obj->dropHintString != '\0') {
        client->ps.cursorHintEntIndex = COD2_ENTITYNUM_NONE;
        client->ps.cursorHint = GENT_WEAPON_DEF_HANDLE(went) + 4;
        obj = BG_GetWeaponDef(GENT_WEAPON_DEF_HANDLE(went));
        client->ps.cursorHintString = obj->unk_0x578;
    }
}

// 0x08121bc6  size=904  FUN_08121bc6  status=translated  naming=inferred
// use-target selection: scans the collected candidates in order and records the first usable handle, slot and number; uses trigger_use classnames
void G_UpdateUseTarget(struct gentity_s *ent)
{
    struct gclient_s *client;
    struct keyed_ent_s candidates[1025];
    int count;
    int i;
    int kind;
    int h;
    int useHandle;
    int useSlot;
    struct gentity_s *cand;
    struct weaponDef_s *obj;

    client = ent->client;
    client->ps.cursorHint = 0;
    client->ps.cursorHintString = -1;
    client->ps.cursorHintEntIndex = COD2_ENTITYNUM_NONE;
    if ((ent->health > 0) && ((client->ps.weaponstate < 0x11) || (client->ps.weaponstate > 0x16))) {
        if (ent->active == 0) {
            if (((client->ps.pm_flags & 4) == 0) && ((count = G_CollectUseCandidates(ent, candidates)) != 0)) {
                useHandle = 0;
                useSlot = -1;
                cand = NULL;
                for (i = 0; i < count; i++) {
                    cand = candidates[i].ent;
                    kind = cand->s.eType;
                    if (kind == 3) {
                        h = G_CheckUseCandidateEntity(client, cand);
                        if (h != 0) {
                            useHandle = h;
                            goto apply;
                        }
                    } else if (kind < 4) {
                        if (kind == 0) {
                            if ((cand->classname != scr_str_trigger_use) && (cand->classname != scr_str_trigger_use_touch)) {
                                goto apply;
                            }
                            if (((cand->team == 0) || ((unsigned int)cand->team == (unsigned int)client->cs.team))
                                && ((GENT_OWNER_NUM(cand) == COD2_ENTITYNUM_NONE) || (GENT_OWNER_NUM(cand) == client->ps.clientNum))) {
                                useHandle = GENT_USE_HANDLE(cand);
                                if ((GENT_USE_HANDLE(cand) != 0) && (GENT_USE_SLOT(cand) != 0xff)) {
                                    useSlot = GENT_USE_SLOT(cand);
                                }
                                goto apply;
                            }
                        }
                    } else if ((kind == 9) && (Turret_CanBeUsed(cand, ent) != 0)) {
                        useHandle = GENT_WEAPON_DEF_HANDLE(cand) + 4;
                        obj = BG_GetWeaponDef(GENT_WEAPON_DEF_HANDLE(cand));
                        if (*obj->useHintString != '\0') {
                            obj = BG_GetWeaponDef(GENT_WEAPON_DEF_HANDLE(cand));
                            useSlot = obj->unk_0x574;
                        }
                        goto apply;
                    }
                }
                return;
apply:
                client->ps.cursorHintEntIndex = cand->s.number;
                client->ps.cursorHint = useHandle;
                client->ps.cursorHintString = useSlot;
                if (client->ps.cursorHint == 0) {
                    client->ps.cursorHintEntIndex = COD2_ENTITYNUM_NONE;
                }
                return;
            }
        } else if ((client->ps.eFlags & 0x300) != 0) {
            G_SetUseTargetFromSlot(ent);
        }
    }
}

// 0x08121f4e  size=188  FUN_08121f4e  status=translated  naming=unknown
// traces start to end with the given mask and returns the hit entity when the hit is within the ray (hit distance test uses FX_GetAttenuationThunk)
struct gentity_s *G_TraceUseRay(struct trace_s *trace, const float *start, const float *end, int owner, int mask, const char *surface, const float *dir)
{
    float hit[4];
    struct gentity_s *result;
    long double d;

    G_TracePointEx(trace, start, end, owner, mask, surface);
    if (trace->entityNum < 0x3fe) {
        Vec3_MultiplyAddAlt(start, (float)(trace->fraction * 15000.0), dir, hit);
        d = FX_GetAttenuationThunk(start, hit);
        if ((float)d < 0.2) {
            result = NULL;
        } else {
            result = &g_gentities[trace->entityNum];
        }
    } else {
        result = NULL;
    }
    return result;
}

// 0x0812200a  size=868  FUN_0812200a  status=translated  naming=inferred
// the "trigger_lookat" classname (0x0885327c) marks look-at triggers; a 15000-unit aim ray picks the target and sets the look-at flags
void G_UpdateLookAtTrigger(struct gentity_s *ent)
{
    struct gclient_s *client;
    struct weaponDef_s *local10;
    struct trace_s trace;
    struct gentity_s *hit;
    float eye[4];
    float fwd[6];
    float end[4];
    float toHit[4];
    const char *sel;
    long double lVar1;
    long double lVar2;

    client = ent->client;
    client->ps.pm_flags = client->ps.pm_flags & 0xffcfffffu;
    client->pLookatEnt = NULL;
    G_GetPlayerViewOrigin(ent, eye);
    G_GetPlayerViewAxis(ent, fwd, NULL, NULL);
    if ((client->ps.eFlags & 0x300) == 0) {
        local10 = BG_GetWeaponDef(client->ps.weapon);
    } else {
        local10 = BG_GetWeaponDef(GENT_WEAPON_DEF_HANDLE(&g_gentities[client->ps.viewlocked_entNum]));
    }
    if ((client->ps.weapon == 0) || (local10->rifleBullet == 0)) {
        sel = (const char *)g_08187c0c;
    } else {
        sel = (const char *)g_08187c1f;
    }
    Vec3_MultiplyAddAlt(eye, COD2_F32(0x466a6000), fwd, end);
    hit = G_TraceUseRay(&trace, eye, end, ent->s.number, 0x22802801, sel, fwd);
    if (hit != NULL) {
        if (hit->classname == scrStr_trigger_lookat) {
            client->pLookatEnt = hit;
            G_QueueEntityPairEvent(hit, ent);
            hit = G_TraceUseRay(&trace, eye, end, ent->s.number, 0x2802801, sel, fwd);
            if (hit == NULL) {
                return;
            }
        }
        if ((hit->s.eType == 1) && ((trace.surfaceFlags & 0x10) == 0)) {
            Vec3_SubtractAlt(hit->r.currentOrigin, eye, toHit);
            if ((hit->client->cs.team == client->cs.team) && (client->cs.team != 0)) {
                lVar1 = Vec3_LengthSquared_081224c2(toHit);
                lVar2 = Math_Square(g_087a24bc->current.value);
                if (((float)lVar1 < lVar2) && (client->pLookatEnt == NULL)) {
                    client->pLookatEnt = hit;
                }
                lVar1 = Vec3_LengthSquared_081224c2(toHit);
                lVar2 = Math_Square(g_087a24b8->current.value);
                if ((float)lVar1 < lVar2) {
                    client->ps.pm_flags = client->ps.pm_flags | 0x100000;
                }
            } else {
                lVar1 = Vec3_LengthSquared_081224c2(toHit);
                lVar2 = Math_Square(local10->enemyCrosshairRange);
                if ((float)lVar1 < lVar2) {
                    if (client->pLookatEnt == NULL) {
                        client->pLookatEnt = hit;
                    }
                    client->ps.pm_flags = client->ps.pm_flags | 0x200000;
                }
            }
        }
    }
}

// 0x0812236e  size=11  FUN_0812236e  status=translated  naming=inferred
// no string evidence; squares a float in extended precision
long double Math_Square(float v)
{
    return (long double)v * (long double)v;
}

// 0x0812237a  size=68  FUN_0812237a  status=translated  naming=inferred
// no string evidence; out = a + b
void Vec3_Add_0812237a(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x081223be  size=68  FUN_081223be  status=translated  naming=inferred
// identical body to Vec3_Subtract_08120ef2 (0x08120ef2); kept as a separate symbol because the binary has two copies
void Vec3_SubtractAlt(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x08122402  size=56  FUN_08122402  status=translated  naming=inferred
// identical body to Vec3_Scale_08120f36 (0x08120f36); second copy in the binary
void Vec3_Scale_08122402(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x0812243a  size=83  FUN_0812243a  status=translated  naming=inferred
// identical body to Vec3_MultiplyAdd_08120f6e (0x08120f6e); second copy in the binary
void Vec3_MultiplyAddAlt(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x0812248e  size=51  FUN_0812248e  status=translated  naming=inferred
// identical body to Vec3_Dot_08120fc2 (0x08120fc2); second copy in the binary
long double Vec3_Dot_0812248e(const float *a, const float *b)
{
    return (long double)a[0] * b[0] + (long double)a[1] * b[1] + (long double)a[2] * b[2];
}

// 0x081224c2  size=51  FUN_081224c2  status=translated  naming=inferred
// no string evidence; squared length of a vector in extended precision, summed x then y then z as compiled
long double Vec3_LengthSquared_081224c2(const float *v)
{
    return (long double)v[0] * v[0] + (long double)v[1] * v[1] + (long double)v[2] * v[2];
}

// 0x081224f8  size=1628  FUN_081224f8  status=translated  naming=inferred
// no single string names the function; it interns the script keyword table (classnames, tags, notify names, sound priorities) into 16-bit handles
void G_InitScriptStrings(void)
{
    scr_str_emptystring = G_NewScriptString("");
    scr_str_allies = G_NewScriptString("allies");
    scr_str_axis = G_NewScriptString("axis");
    scr_str_current = G_NewScriptString("current");
    scrStr_damage = G_NewScriptString("damage");
    scr_str_death = G_NewScriptString("death");
    scrStr_dlight = G_NewScriptString("dlight");
    scrStr_done = G_NewScriptString("done");
    scr_str_empty = G_NewScriptString("empty");
    scr_str_entity = G_NewScriptString("entity");
    scrStr_failed = G_NewScriptString("failed");
    scr_str_fraction = G_NewScriptString("fraction");
    scrStr_goal = G_NewScriptString("goal");
    scrStr_grenade = G_NewScriptString("grenade");
    scrStr_info_notnull = G_NewScriptString("info_notnull");
    scr_str_invisible = G_NewScriptString("invisible");
    scrStr_key1 = G_NewScriptString("key1");
    scrStr_key2 = G_NewScriptString("key2");
    scrStr_killanimscript = G_NewScriptString("killanimscript");
    scrStr_left = G_NewScriptString("left");
    scrStr_movedone = G_NewScriptString("movedone");
    scrStr_noclass = G_NewScriptString("noclass");
    scr_str_normal = G_NewScriptString("normal");
    scrStr_pistol = G_NewScriptString("pistol");
    scrStr_plane_waypoint = G_NewScriptString("plane_waypoint");
    scrStr_player = G_NewScriptString("player");
    scr_str_position = G_NewScriptString("position");
    scrStr_primary = G_NewScriptString("primary");
    scrStr_primaryb = G_NewScriptString("primaryb");
    scrStr_prone = G_NewScriptString("prone");
    scrStr_right = G_NewScriptString("right");
    scrStr_rocket = G_NewScriptString("rocket");
    scrStr_rotatedone = G_NewScriptString("rotatedone");
    scrStr_script_brushmodel = G_NewScriptString("script_brushmodel");
    scr_str_script_model = G_NewScriptString("script_model");
    scr_str_script_origin = G_NewScriptString("script_origin");
    scr_str_spectator = G_NewScriptString("spectator");
    scrStr_stand = G_NewScriptString("stand");
    scr_str_surfacetype = G_NewScriptString("surfacetype");
    scrStr_target_script_trigger = G_NewScriptString("target_script_trigger");
    scrStr_tempEntity = G_NewScriptString("tempEntity");
    scrStr_touch = G_NewScriptString("touch");
    scr_str_trigger = G_NewScriptString("trigger");
    scr_str_trigger_use = G_NewScriptString("trigger_use");
    scr_str_trigger_use_touch = G_NewScriptString("trigger_use_touch");
    scrStr_trigger_damage = G_NewScriptString("trigger_damage");
    scrStr_trigger_lookat = G_NewScriptString("trigger_lookat");
    scrStr_truck_cam = G_NewScriptString("truck_cam");
    scrStr_worldspawn = G_NewScriptString("worldspawn");
    scrStr_binocular_enter = G_NewScriptString("binocular_enter");
    scrStr_binocular_exit = G_NewScriptString("binocular_exit");
    scrStr_binocular_fire = G_NewScriptString("binocular_fire");
    scrStr_binocular_release = G_NewScriptString("binocular_release");
    scrStr_binocular_drop = G_NewScriptString("binocular_drop");
    scrStr_begin = G_NewScriptString("begin");
    scr_str_intermission = G_NewScriptString("intermission");
    scrStr_menuresponse = G_NewScriptString("menuresponse");
    scr_str_playing = G_NewScriptString("playing");
    scr_str_none = G_NewScriptString("none");
    scr_str_dead = G_NewScriptString("dead");
    scr_str_auto_change = G_NewScriptString("auto_change");
    scr_str_manual_change = G_NewScriptString("manual_change");
    scr_str_freelook = G_NewScriptString("freelook");
    scrStr_call_vote = G_NewScriptString("call_vote");
    scrStr_vote = G_NewScriptString("vote");
    scr_str_snd_enveffectsprio_level = G_NewScriptString("snd_enveffectsprio_level");
    scr_str_snd_enveffectsprio_shellshock = G_NewScriptString("snd_enveffectsprio_shellshock");
    scr_str_snd_channelvolprio_holdbreath = G_NewScriptString("snd_channelvolprio_holdbreath");
    scr_str_snd_channelvolprio_pain = G_NewScriptString("snd_channelvolprio_pain");
    scr_str_snd_channelvolprio_shellshock = G_NewScriptString("snd_channelvolprio_shellshock");
    scrStr_tag_flash = G_NewScriptString("tag_flash");
    scrStr_tag_flash_11 = G_NewScriptString("tag_flash_11");
    scrStr_tag_flash_2 = G_NewScriptString("tag_flash_2");
    scrStr_tag_flash_22 = G_NewScriptString("tag_flash_22");
    scrStr_tag_brass = G_NewScriptString("tag_brass");
    scrStr_j_head = G_NewScriptString("j_head");
    scrStr_tag_weapon = G_NewScriptString("tag_weapon");
    scrStr_tag_player = G_NewScriptString("tag_player");
    scrStr_tag_camera = G_NewScriptString("tag_camera");
    scrStr_tag_aim = G_NewScriptString("tag_aim");
    scrStr_tag_aim_animated = G_NewScriptString("tag_aim_animated");
    scrStr_tag_origin = G_NewScriptString("tag_origin");
    scrStr_tag_butt = G_NewScriptString("tag_butt");
    scr_str_tag_weapon_right = G_NewScriptString("tag_weapon_right");
    scrStr_back_low = G_NewScriptString("back_low");
    scrStr_back_mid = G_NewScriptString("back_mid");
    scrStr_back_up = G_NewScriptString("back_up");
    scrStr_neck = G_NewScriptString("neck");
    scrStr_head = G_NewScriptString("head");
    scrStr_pelvis = G_NewScriptString("pelvis");
}

// 0x08122b54  size=68  FUN_08122b54  status=translated  naming=inferred
// zeroes every field of a message stream (byte buffer, mode flags, counters)
void MsgStream_Reset(struct msgstream_s *s)
{
    s->msg = NULL;
    s->isReading = 0;
    s->flag5 = 0;
    s->bytesDone = 0;
    s->writePos = 0;
    s->rawRemaining = 0;
    s->zeroRemaining = 0;
}

// 0x08122b98  size=68  FUN_08122b98  status=translated  naming=inferred
// identical body to MsgStream_Reset (0x08122b54); second copy in the binary
void MsgStream_ResetAlt(struct msgstream_s *s)
{
    s->msg = NULL;
    s->isReading = 0;
    s->flag5 = 0;
    s->bytesDone = 0;
    s->writePos = 0;
    s->rawRemaining = 0;
    s->zeroRemaining = 0;
}

// 0x08122bdc  size=71  FUN_08122bdc  status=translated  naming=inferred
// binds a message stream to a buffer for reading; the write cursor starts at the buffer's cursize
void MsgStream_InitRead(struct msgstream_s *s, struct msgbuf_s *msg)
{
    s->msg = msg;
    s->isReading = 1;
    s->flag5 = 0;
    s->bytesDone = 0;
    s->writePos = msg->cursize;
    s->rawRemaining = 0;
    s->zeroRemaining = 0;
}

// 0x08122c24  size=71  FUN_08122c24  status=translated  naming=inferred
// binds a message stream to a buffer for writing; sets the write-mode flag byte at +5
void MsgStream_InitWrite(struct msgstream_s *s, struct msgbuf_s *msg)
{
    s->msg = msg;
    s->isReading = 0;
    s->flag5 = 1;
    s->bytesDone = 0;
    s->writePos = msg->cursize;
    s->rawRemaining = 0;
    s->zeroRemaining = 0;
}

// 0x08122c6c  size=149  FUN_08122c6c  status=translated  naming=inferred
// format "fx/%s" at 0x0815f23c names the effect path built from the decoded name
int MsgStream_ReadFxEffect(struct msgstream_s *s)
{
    unsigned char name[79];
    char path[64];
    unsigned char len;
    int result;

    len = MsgStream_ReadDecodedByte(s);
    if ((len == 0) || (len > 0x3f)) {
        result = 0;
    } else {
        MsgStream_ReadCompressed(s, name, len);
        name[len] = 0;
        Com_sprintf(path, 0x40, "fx/%s", (const char *)name);
        result = FX_GetEffectByName(path);
    }
    return result;
}

// 0x08122d02  size=97  FUN_08122d02  status=translated  naming=inferred
// decodes a length-prefixed name (1..63 bytes) and returns its interned handle through Material_RegisterStub
int MsgStream_ReadScrString(struct msgstream_s *s)
{
    unsigned char name[79];
    unsigned char len;
    int result;

    len = MsgStream_ReadDecodedByte(s);
    if ((len == 0) || (len > 0x3f)) {
        result = 0;
    } else {
        MsgStream_ReadCompressed(s, name, len);
        name[len] = 0;
        result = Material_RegisterStub((const char *)name);
    }
    return result;
}

// 0x08122d64  size=97  FUN_08122d64  status=translated  naming=inferred
// same decoding as MsgStream_ReadScrString but returns the handle through Model_RegisterByName
int MsgStream_ReadScrStringAlt(struct msgstream_s *s)
{
    unsigned char name[79];
    unsigned char len;
    int result;

    len = MsgStream_ReadDecodedByte(s);
    if ((len == 0) || (len > 0x3f)) {
        result = 0;
    } else {
        MsgStream_ReadCompressed(s, name, len);
        name[len] = 0;
        result = Model_RegisterByName((const char *)name);
    }
    return result;
}

// 0x08122dc6  size=49  FUN_08122dc6  status=translated  naming=inferred
// reads one float value into a typed script value: type 0, aux 0, number as float
void MsgStream_ReadValueFloat(struct msgstream_s *s, struct msg_value_s *v)
{
    v->aux = 0;
    v->value.f = MsgStream_ReadDecodedFloat(s);
    v->type = 0;
}

// 0x08122df8  size=313  FUN_08122df8  status=translated  naming=inferred
// run-length decoder: the top two bits of each control byte select a raw run, a zero run, or a repeat-of-previous-byte run; raw bytes come from the message buffer
void MsgStream_ReadCompressed(struct msgstream_s *s, unsigned char *out, int count)
{
    unsigned char *dst;
    unsigned char b;
    unsigned char cls;

    dst = out;
    b = 0;
    cls = 0;
    s->bytesDone = count + s->bytesDone;
    do {
        for (;;) {
            for (;;) {
                while (s->rawRemaining != 0) {
                    s->rawRemaining = s->rawRemaining - 1;
                    count = count - 1;
                    *dst = MsgStream_ReadRawByte(s->msg);
                    dst++;
                    if (count == 0) {
                        return;
                    }
                }
                while ((s->zeroRemaining != 0) && (count != 0)) {
                    s->zeroRemaining = s->zeroRemaining - 1;
                    count = count - 1;
                    *dst = 0;
                    dst++;
                    if (count == 0) {
                        return;
                    }
                }
                b = MsgStream_ReadRawByte(s->msg);
                cls = (unsigned char)(b & 0xc0);
                if (cls != 0x40) {
                    break;
                }
                s->rawRemaining = 2;
                s->zeroRemaining = (b & 0x3f) + 1;
            }
            if (cls > 0x40) {
                break;
            }
            if ((b & 0xc0) == 0) {
                s->rawRemaining = 1;
                s->zeroRemaining = (b & 0x3f) + 1;
            } else {
                s->rawRemaining = (b & 0x3f) + 1;
                s->zeroRemaining = 0;
            }
        }
        if (cls != 0x80) {
            s->rawRemaining = (b & 0x3f) + 1;
            s->zeroRemaining = 0;
        } else {
            s->rawRemaining = 4;
            s->zeroRemaining = (b & 0x3f) + 1;
        }
    } while (1);
}

// 0x08122f32  size=95  FUN_08122f32  status=translated  naming=inferred
// writes a length byte then the string bytes; a NULL reference writes the empty string
void MsgStream_WriteStringRef(struct msgstream_s *s, const char **strRef)
{
    const char *text;
    size_t len;

    if (strRef == NULL) {
        text = "";
    } else {
        text = *strRef;
    }
    len = strlen(text);
    MsgStream_WriteDecodedByte(s, (unsigned char)(len & 0xff));
    if (len != 0) {
        MsgStream_WriteCompressed(s, (const unsigned char *)text, (int)len);
    }
}

// 0x08122f92  size=110  FUN_08122f92  status=translated  naming=inferred
// writes a string selected by an interned handle (lookup table at g_fxSystem); handle 0 writes the empty string
void MsgStream_WriteScrString(struct msgstream_s *s, int handle)
{
    const char *text;
    size_t len;

    if (handle == 0) {
        text = "";
    } else {
        text = (const char *)FX_ScriptStringToText_Stub(g_fxSystem, handle);
    }
    len = strlen(text);
    MsgStream_WriteDecodedByte(s, (unsigned char)(len & 0xff));
    if (len != 0) {
        MsgStream_WriteCompressed(s, (const unsigned char *)text, (int)len);
    }
}

// 0x08123000  size=107  FUN_08123000  status=translated  naming=inferred
// writes a string selected by a string index through XModel_GetName; index 0 writes the empty string
void MsgStream_WriteStringIdx(struct msgstream_s *s, int handle)
{
    const char *text;
    size_t len;

    if (handle == 0) {
        text = "";
    } else {
        text = (const char *)XModel_GetName(handle);
    }
    len = strlen(text);
    MsgStream_WriteDecodedByte(s, (unsigned char)(len & 0xff));
    if (len != 0) {
        MsgStream_WriteCompressed(s, (const unsigned char *)text, (int)len);
    }
}

// 0x0812306c  size=29  FUN_0812306c  status=translated  naming=inferred
// writes the integer number of a typed script value (its +8 word)
void MsgStream_WriteValueInt(struct msgstream_s *s, const struct msg_value_s *v)
{
    MsgStream_WriteDecodedInt32(s, v->value.i);
}

// 0x0812308a  size=905  FUN_0812308a  status=translated  naming=inferred
// run-length encoder, the counterpart of MsgStream_ReadCompressed: rewrites the previous control byte in place (adds 1, switches the run kind, or merges two bytes) before appending new raw bytes
void MsgStream_WriteCompressed(struct msgstream_s *s, const unsigned char *src, int count)
{
    struct msgbuf_s *msg;
    unsigned char bVar1;
    int local_c;
    const unsigned char *local_8;

    msg = s->msg;
    s->bytesDone = count + s->bytesDone;
    local_8 = src;
    if (s->writePos == msg->cursize) {
        MsgStream_WriteRawByte(msg, 0xc0);
        MsgStream_WriteRawByte(msg, src[0]);
        local_8 = src + 1;
        count = count - 1;
    }
    local_c = 0;
write_restart:
    for (;;) {
        if (count <= local_c) {
            return;
        }
        bVar1 = (unsigned char)(msg->data[s->writePos] & 0xc0);
        if (bVar1 == 0x40) {
write_run_start:
            for (; local_c < count; local_c++) {
                if ((local_8[local_c] != 0) || ((msg->data[s->writePos] & 0x3f) == 0x3f)) {
                    s->writePos = msg->cursize;
                    MsgStream_WriteRawByte(msg, 0xc0);
                    MsgStream_WriteRawByte(msg, local_8[local_c]);
                    local_c++;
                    break;
                }
                msg->data[s->writePos]++;
            }
            goto write_restart;
        }
        if (bVar1 > 0x40) {
            if (bVar1 != 0x80) {
                goto write_mode_loop;
            }
            goto write_run_start;
        }
        if ((msg->data[s->writePos] & 0xc0) == 0) {
            goto write_run_start;
        }
write_mode_loop:
        while (local_c < count) {
            if ((msg->data[s->writePos] & 0x3f) == 0) {
                if (local_8[local_c] == 0) {
                    msg->data[s->writePos] = 0;
                    local_c++;
                    break;
                }
write_literal:
                msg->data[s->writePos]++;
                MsgStream_WriteRawByte(msg, local_8[local_c]);
                local_c++;
            } else {
                if ((msg->data[s->writePos] & 0x3f) == 1) {
                    if (local_8[local_c] != 0) {
                        goto write_literal;
                    }
                    msg->data[s->writePos] = 0x40;
                    local_c++;
                    break;
                }
                if ((msg->data[s->writePos] & 0x3f) == 3) {
                    if (local_8[local_c] != 0) {
                        goto write_literal;
                    }
                    msg->data[s->writePos] = 0x80;
                    local_c++;
                    break;
                }
                if ((msg->data[s->writePos] & 0x3f) != 0x3f) {
                    if ((((msg->data[s->writePos] & 0x3f) < 6) || (local_8[local_c] != 0))
                        || (msg->data[msg->cursize - 2] != 0)
                        || (msg->data[msg->cursize - 1] != 0)) {
                        goto write_literal;
                    }
                    msg->data[s->writePos] = (unsigned char)(msg->data[s->writePos] - 3);
                    msg->data[msg->cursize - 2] = msg->data[msg->cursize - 3];
                    msg->data[msg->cursize - 3] = 2;
                    s->writePos = msg->cursize - 3;
                    msg->cursize = msg->cursize - 1;
                    local_c++;
                    break;
                }
                s->writePos = msg->cursize;
                MsgStream_WriteRawByte(msg, 0xc0);
                MsgStream_WriteRawByte(msg, local_8[local_c]);
                local_c++;
            }
        }
    }
}

// 0x08123414  size=69  FUN_08123414  status=translated  naming=inferred
// direction dispatch: encodes when the stream is in write mode, decodes when it is in read mode
void MsgStream_TransferBytes(struct msgstream_s *s, unsigned char *buf, int count)
{
    if (s->isReading == 0) {
        MsgStream_WriteCompressed(s, buf, count);
    } else {
        MsgStream_ReadCompressed(s, buf, count);
    }
}

// 0x0812345a  size=60  FUN_0812345a  status=translated  naming=inferred
// direction dispatch for a string reference slot: write the slot's string, or read an effect name into it
void MsgStream_TransferStringRef(struct msgstream_s *s, int *slot)
{
    if (s->isReading == 0) {
        MsgStream_WriteStringRef(s, (const char **)(*slot));
    } else {
        *slot = MsgStream_ReadFxEffect(s);
    }
}

// 0x08123496  size=57  FUN_08123496  status=translated  naming=inferred
// direction dispatch for a string-handle slot (table lookup on write, decode on read)
void MsgStream_TransferScrString(struct msgstream_s *s, int *slot)
{
    if (s->isReading == 0) {
        MsgStream_WriteScrString(s, *slot);
    } else {
        *slot = MsgStream_ReadScrString(s);
    }
}

// 0x081234d0  size=57  FUN_081234d0  status=translated  naming=inferred
// direction dispatch for a string-index slot (XModel_GetName on write, alternate handle decode on read)
void MsgStream_TransferStringIdx(struct msgstream_s *s, int *slot)
{
    if (s->isReading == 0) {
        MsgStream_WriteStringIdx(s, *slot);
    } else {
        *slot = MsgStream_ReadScrStringAlt(s);
    }
}

// 0x0812350a  size=55  FUN_0812350a  status=translated  naming=inferred
// direction dispatch for a typed script value: write the integer number, or read a float number
void MsgStream_TransferValue(struct msgstream_s *s, struct msg_value_s *v)
{
    if (s->isReading == 0) {
        MsgStream_WriteValueInt(s, v);
    } else {
        MsgStream_ReadValueFloat(s, v);
    }
}

// 0x08123542  size=40  FUN_08123542  status=translated  naming=inferred
// writes one raw byte through the message write routine
void MsgStream_WriteRawByte(struct msgbuf_s *msg, unsigned char v)
{
    unsigned char b;

    b = v;
    MsgBuf_WriteData(msg, 1, &b);
}

// 0x0812356a  size=38  FUN_0812356a  status=translated  naming=inferred
// reads one raw byte through the message read routine
unsigned char MsgStream_ReadRawByte(struct msgbuf_s *msg)
{
    unsigned char b; /* deliberately uninitialised: the original reads the stack byte [ebp-1] after the call */

    MsgBuf_ReadData(msg, 1, &b);
    return b;
}

// 0x08123590  size=38  FUN_08123590  status=translated  naming=inferred
// reads one byte through the run-length decoder
unsigned char MsgStream_ReadDecodedByte(struct msgstream_s *s)
{
    unsigned char b;

    b = 0;
    MsgStream_ReadCompressed(s, &b, 1);
    return b;
}

// 0x081235b6  size=43  FUN_081235b6  status=translated  naming=inferred
// reads four bytes through the run-length decoder as one float
float MsgStream_ReadDecodedFloat(struct msgstream_s *s)
{
    float v;

    MsgStream_ReadCompressed(s, (unsigned char *)&v, 4);
    return v;
}

// 0x081235e2  size=40  FUN_081235e2  status=translated  naming=inferred
// writes one byte through the run-length encoder
void MsgStream_WriteDecodedByte(struct msgstream_s *s, unsigned char v)
{
    unsigned char b;

    b = v;
    MsgStream_WriteCompressed(s, &b, 1);
}

// 0x0812360a  size=34  FUN_0812360a  status=translated  naming=inferred
// writes a four-byte integer through the run-length encoder
void MsgStream_WriteDecodedInt32(struct msgstream_s *s, int value)
{
    MsgStream_WriteCompressed(s, (const unsigned char *)&value, 4);
}

// 0x0812362c  size=159  FUN_0812362c  status=translated  naming=inferred
// builds a two-row curve from a constant "from" value at key 0 to a constant "to" value at key 1, then sets the range to (1, 1)
void Curve_CreateRamp(struct curve_ref_s *out, int sampleCount, float from, float to)
{
    float rows[19];
    int i;

    rows[0] = 0.0f;
    rows[sampleCount + 1] = 1.0f;
    for (i = 0; i != sampleCount; i++) {
        rows[i + 1] = from;
        rows[sampleCount + 1 + i + 1] = to;
    }
    out->curve = Curve_Create(rows, sampleCount, 2);
    FxRange_Set(&out->rangeLo, 1.0f, 1.0f);
}

// 0x08123702  size=146  FUN_08123702  status=translated  naming=inferred
// builds a two-row curve (key 0 and key 1) from two sample arrays; writes only the curve pointer
void Curve_CreateLinear(struct curve_ref_s *out, int sampleCount, const float *from, const float *to)
{
    float rows[17];
    int i;

    rows[0] = 0.0f;
    rows[sampleCount + 1] = 1.0f;
    for (i = 0; i != sampleCount; i++) {
        rows[i + 1] = from[i];
        rows[sampleCount + 1 + i + 1] = to[i];
    }
    out->curve = Curve_Create(rows, sampleCount, 2);
}

// 0x08123794  size=192  FUN_08123794  status=translated  naming=inferred
// builds a three-row curve: key 0 with the first array, a middle key with the first array again, and key 1 with the second array
void Curve_CreateMidpoint(struct curve_ref_s *out, int sampleCount, const float *a, const float *b, float midKey)
{
    float rows[25];
    int stride;
    int i;

    stride = sampleCount + 1;
    rows[0] = 0.0f;
    rows[stride] = midKey;
    rows[stride * 2] = 1.0f;
    for (i = 0; i != sampleCount; i++) {
        rows[i + 1] = a[i];
        rows[stride + i + 1] = a[i];
        rows[stride * 2 + i + 1] = b[i];
    }
    out->curve = Curve_Create(rows, sampleCount, 3);
}

// 0x08123854  size=777  FUN_08123854  status=translated  naming=inferred
// samples 16 keys in steps of 1/15 and blends a and b with a mode-selected weight: linear, cosine, or edge fade; mode bit 2 clamps through Rand_Float
void Curve_CreateWave(struct curve_ref_s *out, int sampleCount, unsigned int mode, const float *a,
                      const float *b, float ratioEdge, float cosScale)
{
    float rows[129];
    float t;
    float w;
    float tmp;
    float v;
    int r;
    int i;
    long double lv;
    double dv;

    t = 0.0f;
    for (r = 0; r != 16; r++) {
        if (1.0 < t) {
            t = 1.0f;
        }
        w = 1.0f;
        tmp = 1.0f;
        if ((mode & 1) != 0) {
            w = 1.0f - t;
        }
        if ((mode & 0xc) == 4) {
            if (ratioEdge < t) {
                tmp = (float)(1.0 - ((double)(t - ratioEdge)) / (1.0 - (double)ratioEdge));
            }
            if ((mode & 1) == 0) {
                w = tmp;
            } else {
                w = (float)((double)tmp * 0.5 + (double)w * 0.5);
            }
        } else if ((mode & 0xc) == 8) {
            dv = cos((double)(t * cosScale));
            w = w * (float)dv;
        } else if ((mode & 0xc) == 0xc) {
            if (t < ratioEdge) {
                tmp = (ratioEdge - t) / ratioEdge;
            } else {
                tmp = 0.0f;
            }
            if ((mode & 1) == 0) {
                w = tmp;
            } else {
                w = (float)((double)tmp * 0.5 + (double)w * 0.5);
            }
        }
        rows[r * (sampleCount + 1)] = t;
        for (i = 0; i != sampleCount; i++) {
            v = (float)((1.0 - (double)w) * (double)b[i] + (double)a[i] * (double)w);
            if (v < 0.0) {
                v = 0.0f;
            } else if (1.0 < w) {
                v = 1.0f;
            }
            if ((mode & 2) != 0) {
                lv = Rand_Float(0, v);
                v = (float)lv;
            }
            rows[r * (sampleCount + 1) + i + 1] = v;
        }
        t = (float)((double)t + 0.06666667);
    }
    out->curve = Curve_Create(rows, sampleCount, 16);
}

// 0x08123b5e  size=544  FUN_08123b5e  status=translated  naming=unknown
// fx parameter-curve builder: derives per-dimension lo/hi samples and a range from the spec pairs, then calls one of the three curve builders selected by the raw flag word
void FxCurve_Build(const struct fx_curve_spec_s *spec, int sampleCount, float scale, char linearOnly,
                   struct curve_ref_s *out)
{
    float local_14;
    float local_18;
    float local_1c;
    float local_2c[4];
    float local_3c[4];
    float local_40;
    int k;
    long double lVar2;
    long double lVar3;
    unsigned int flags;

    local_14 = 1.0f;
    if (linearOnly == 0) {
        lVar2 = FxRange_Lerp(spec->pairs + 6, 0.5f);
        lVar3 = FxRange_Lerp(spec->pairs, 0.5f);
        lVar2 = (long double)Math_Max((float)lVar3, (float)lVar2);
        local_14 = (float)(1.3333334 * lVar2);
    }
    if (local_14 < 1.0f) {
        local_14 = 1.0f;
    }
    local_1c = (float)((spec->pairs[1] - spec->pairs[0]) / 2.0);
    FxRange_Set(&out->rangeLo, local_14 - local_1c, local_14 + local_1c);
    local_18 = 1.0f / local_14;
    local_40 = 0.0f;
    for (k = 0; k != sampleCount; k++) {
        lVar2 = FxRange_Lerp(spec->pairs + k * 2, 0.5f);
        local_2c[k] = (float)(lVar2 * (long double)local_18);
        lVar2 = FxRange_Lerp(spec->pairs + k * 2 + 6, 0.5f);
        local_3c[k] = (float)(lVar2 * (long double)local_18);
        lVar2 = FxRange_Lerp(spec->midPair, 0.5f);
        local_40 = (float)(lVar2 * (long double)local_18);
    }
    flags = (unsigned int)spec->flags;
    if (((flags & 1) == 0) || ((flags & 0xc) != 0)) {
        if (((flags & 1) != 0) || ((flags & 0xc) != 4)) {
            Curve_CreateWave(out, sampleCount, flags, local_2c, local_3c,
                             (float)(local_40 * 0.01), (float)(local_40 * 0.003141593 * scale));
        } else {
            Curve_CreateMidpoint(out, sampleCount, local_2c, local_3c, (float)(local_40 * 0.01));
        }
    } else {
        Curve_CreateLinear(out, sampleCount, local_2c, local_3c);
    }
}

// 0x08123d7e  size=36  FUN_08123d7e  status=translated  naming=inferred
// maximum of two floats via the select helper (ties return the first argument)
float Math_Max(float a, float b)
{
    return Math_SelectNonNegative(a - b, a, b);
}

// 0x08123da2  size=41  FUN_08123da2  status=translated  naming=inferred
// returns the first candidate when the difference is non-negative, otherwise the second
float Math_SelectNonNegative(float diff, float ifNonNeg, float ifNeg)
{
    float result;

    if (0.0 <= diff) {
        result = ifNonNeg;
    } else {
        result = ifNeg;
    }
    return result;
}

// 0x08123dcc  size=23  FUN_08123dcc  status=translated  naming=inferred
// stores an int and clears its companion word
void IntPair_Init(int *p, int v)
{
    p[0] = v;
    p[1] = 0;
}

// 0x08123de4  size=14  FUN_08123de4  status=translated  naming=inferred
// clears one int
void IntRef_Clear(int *p)
{
    *p = 0;
}

// 0x08123df2  size=27  FUN_08123df2  status=translated  naming=inferred
// trapezoid area between two keys: (v0 + v1) * 0.5 * (k1 - k0)
long double Curve_TrapezoidArea(float k0, float k1, float v0, float v1)
{
    return ((long double)v0 + v1) * 0.5L * ((long double)k1 - k0);
}

// 0x08123e0e  size=226  FUN_08123e0e  status=translated  naming=inferred
// integrates a curve's first value column over key from the first row up to the given key (piecewise trapezoids plus the last partial segment)
float Curve_IntegrateToKey(const struct curve_table_s *curve, float key)
{
    int stride;
    float sum;
    const float *row;
    const float *next;
    long double seg;
    long double tail;

    stride = curve->sampleCount + 1;
    sum = 0.0f;
    row = curve->rows;
    next = row + stride;
    while (next[0] < key) {
        seg = Curve_TrapezoidArea(row[0], next[0], row[1], next[1]);
        sum = (float)((long double)sum + seg);
        row = next;
        next = row + stride;
    }
    seg = Curve_InterpolateSegment(row, key);
    tail = Curve_TrapezoidArea(row[0], key, row[1], (float)seg);
    return (float)((long double)sum + tail);
}

// 0x08123ef0  size=74  FUN_08123ef0  status=translated  naming=inferred
// linear interpolation of a row's value at key between its first two samples
long double Curve_InterpolateSegment(const float *row, float key)
{
    return (((long double)row[3] - row[1]) * ((long double)key - row[0]))
           / ((long double)row[2] - row[0]) + row[1];
}

// 0x08123f3c  size=403  FUN_08123f3c  status=translated  naming=inferred
// allocates a curve table, copies the rows, and adds a leading row (key 0) when the first key is non-zero and a trailing row (key 1) when the last key is not 1.0; the added rows copy row 0's values, as the binary does
struct curve_table_s *Curve_Create(const float *rows, int sampleCount, int rowCount)
{
    int stride;
    int extraLast;
    int rowsOut;
    int rowsTotal;
    struct curve_table_s *tab;
    int *words;
    int local_10;
    int k;

    stride = sampleCount + 1;
    extraLast = (rows[(rowCount - 1) * stride] != 1.0f);
    rowsOut = rowCount;
    if (rows[0] != 0.0f) {
        rowsOut = rowCount + 1;
    }
    rowsTotal = rowsOut;
    if (extraLast) {
        rowsTotal = rowsOut + 1;
    }
    tab = (struct curve_table_s *)Hunk_AllocAlign((size_t)(stride * rowsTotal * 4 + 8), 4);
    words = (int *)tab;
    tab->sampleCount = sampleCount;
    local_10 = 0;
    if (rows[0] != 0.0f) {
        words[2] = 0;
        for (k = 0; k != sampleCount; k++) {
            words[k + 3] = COD2_BITS(rows[k + 1]);
        }
        local_10 = 1;
    }
    memcpy(words + local_10 * stride + 2, rows, (size_t)(rowCount * stride * 4));
    if (extraLast) {
        words[(local_10 + rowCount) * stride + 2] = COD2_BITS(1.0f);
        for (k = 0; k != sampleCount; k++) {
            words[(local_10 + rowCount) * stride + k + 3] = COD2_BITS(rows[k + 1]);
        }
    }
    words[1] = rowCount;
    return tab;
}

// 0x081240d0  size=8  FUN_081240d0  status=translated  naming=unknown
// returns its argument unchanged
int Int_Identity_081240d0(int v)
{
    return v;
}
