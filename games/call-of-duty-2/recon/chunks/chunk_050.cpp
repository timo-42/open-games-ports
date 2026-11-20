#include "cod2_types.h"
#include <ctype.h>
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_050_types.h"

/* Float bits helper: the script float push takes the raw 32-bit pattern (cdecl, same stack slot as a float). */
static unsigned int Float_Bits(float f)
{
    union { float f; unsigned int u; } v;
    v.f = f;
    return v.u;
}

/* ===== projectile / mover section ===== */

// 0x0810e024  size=170  FUN_0810e024  status=translated  naming=inferred
// Spawns the 0xb6 temp-entity event describing a surface impact (surface normal code, velocity direction code, surface type, owner entity).
void G_AddImpactEvent(gentity_t *ent, trace_t *tr)
{
    vec3_t velAngles;
    gentity_t *event;

    Vec3_NormalizeCopy(ent->s.pos.trDelta, velAngles);
    if (velAngles[2] < 0.0f) {
        velAngles[2] = -velAngles[2];
    }
    event = (gentity_s*)G_TempEntity(ent->r.currentOrigin, 0xb6);
    event->s.eventParm = Normal_DirToIndex(tr->normal) & 0xff;
    event->s.scale = Normal_DirToIndex(velAngles) & 0xff;
    event->s.surfType = (int)((unsigned int)tr->surfaceFlags & 0x1f00000) >> 0x14;
    event->s.otherEntityNum = ent->s.number;
}

// 0x0810e0ce  size=1471  FUN_0810e0ce  status=translated  naming=inferred
// Per-frame missile mover (Quake III G_RunMissile counterpart: evaluate trajectory, trace, impact, think). No identifying strings.
void G_RunMissile(gentity_t *ent)
{
    trace_t tr;
    trace_t tr2;
    vec3_t origin;          /* local_1c: trajectory target position */
    vec3_t prevOrigin;      /* local_9c */
    vec3_t delta;           /* local_2c */
    vec3_t hitPos;          /* local_bc */
    int entKind;
    weaponDef_s *weapon;

    if (ent->s.pos.trType == 0 && ent->s.groundEntityNum != 0x3fe) {
        Vec3_Copy_0810ebae(ent->r.currentOrigin, origin);
        origin[2] = origin[2] - 1.5f;
        Turret_TraceAimWithFlags(&tr, ent->r.currentOrigin, origin, ent->r.ownerNum, ent->clipmask);
        if (tr.fraction == 1.0f) {
            ent->s.pos.trType = 5;
            ent->s.pos.trTime = g_0864c56c;
            ent->s.pos.trDuration = 0;
            Vec3_Copy_0810ebae(ent->r.currentOrigin, ent->s.pos.trBase);
            Vec3_Clear_0810eb84(ent->s.pos.trDelta);
        }
    }
    Vec3_Copy_0810ebae(ent->r.currentOrigin, prevOrigin);
    BG_EvaluateTrajectory((&ent->s.pos), g_0864c56c, origin);
    Vec3_Subtract_0810ec22(origin, ent->r.currentOrigin, delta);
    if ((long double)Vec3_Normalize(delta) < (long double)0.001f) {
        G_RunThink(ent);
        return;
    }
    if (Abs_Float(ent->s.pos.trDelta[2]) <= 30.0f || SV_PointContents(ent->r.currentOrigin, -1, 0x20) != 0) {
        Turret_TraceAimWithFlags(&tr, ent->r.currentOrigin, origin, ent->r.ownerNum, ent->clipmask);
    } else {
        Turret_TraceAimWithFlags(&tr, ent->r.currentOrigin, origin, ent->r.ownerNum, ent->clipmask | 0x20);
    }
    if (((unsigned int)tr.surfaceFlags & 0x1f00000) == 0x1400000) {
        G_AddImpactEvent(ent, &tr);
        Turret_TraceAimWithFlags(&tr, ent->r.currentOrigin, origin, ent->r.ownerNum, ent->clipmask);
    }
    entKind = g_entityHandlerTable[ent->handler].kind;
    if (entKind == 3 && (g_gentities[tr.entityNum].flags & 0x8000) != 0) {
        Turret_TraceAimNoContents(&tr, tr.entityNum, ent, origin);
    }
    VectorLerp(ent->r.currentOrigin, origin, tr.fraction, hitPos);
    Vec3_Copy_0810ebae(hitPos, ent->r.currentOrigin);
    if ((ent->s.eFlags & 0x1000000) != 0 &&
        (tr.fraction == 1.0f || (tr.fraction < 1.0f && 0.7f < tr.normal[2]))) {
        Vec3_Copy_0810ebae(ent->r.currentOrigin, origin);
        origin[2] = origin[2] - 1.5f;
        Turret_TraceAimWithFlags(&tr2, ent->r.currentOrigin, origin, ent->r.ownerNum, ent->clipmask);
        if (tr2.fraction != 1.0f && tr2.entityNum == 0x3fe) {
            tr = tr2;
            VectorLerp(ent->r.currentOrigin, origin, tr2.fraction, hitPos);
            ent->s.pos.trBase[2] = ent->s.pos.trBase[2] + ((hitPos[2] + 1.5f) - ent->r.currentOrigin[2]);
            Vec3_Copy_0810ebae(hitPos, ent->r.currentOrigin);
            ent->r.currentOrigin[2] = ent->r.currentOrigin[2] + 1.5f;
        }
    }
    SV_LinkEntity(ent);
    weapon = (weaponDef_s*)BG_GetWeaponDef(ent->s.weapon);
    if (entKind == 3) {
        Ent_AreaEffectApplyFlagged((ent), prevOrigin, ent->r.currentOrigin, weapon->explosionInnerDamage, 3);
    }
    if (tr.fraction != 1.0f) {
        if ((tr.surfaceFlags & 0x10) != 0) {
            G_FreeEntity((ent));
            return;
        }
        Turret_ProcessInput(ent, (turret_input_s *)&tr, delta, hitPos);
        if (ent->s.eType != 4) {
            return;
        }
    } else {
        if (Vec3_Length_0810ed26(ent->s.pos.trDelta) != 0.0f) {
            ent->s.groundEntityNum = 0x3ff;
            if (weapon->weaponType == 2 && (ent->flags & 0x20000) == 0) {
                Turret_Advance(ent);
            }
        }
    }
    G_RunThink(ent);
}

// 0x0810e68e  size=682  FUN_0810e68e  status=translated  naming=inferred
// Spawns a thrown-projectile entity (type 7, bouncing/tumbling trajectory); Quake III fire_grenade counterpart. Consumes the owner's client+0x3c fuse value.
gentity_t *G_SpawnGrenade(gentity_t *owner, const float *origin, const float *dir, int weaponIndex, int defaultFuseTime)
{
    gentity_t *ent;
    weaponDef_s *weapon;

    ent = (gentity_s*)G_Spawn();
    if (owner->client == 0 || owner->client->ps.grenadeTimeLeft == 0) {
        ent->nextthink = defaultFuseTime + g_0864c56c;
    } else {
        ent->nextthink = owner->client->ps.grenadeTimeLeft + g_0864c56c;
        owner->client->ps.grenadeTimeLeft = 0;
    }
    if (owner->client != 0) {
        owner->client->ps.grenadeTimeLeft = 0;
    }
    ent->handler = 7;
    ent->s.eType = 4;
    ent->r.svFlags = 8;
    ent->s.weapon = weaponIndex;
    ent->r.ownerNum = owner->s.number;
    ent->parent = owner;
    weapon = (weaponDef_s*)BG_GetWeaponDef(weaponIndex);
    ScrStr_AssignRef(&ent->classname, scrStr_grenade);
    ent->damage = weapon->damage;
    ent->s.eFlags = 0x1000000;
    ent->clipmask = 0x2802891;
    ent->s.time = g_0864c56c + 0x32;
    ent->s.pos.trType = 5;
    ent->s.pos.trTime = g_0864c56c;
    Vec3_Copy_0810ebae(origin, ent->s.pos.trBase);
    Vec3_Copy_0810ebae(dir, ent->s.pos.trDelta);
    ent->s.pos.trDelta[0] = (float)(int)ent->s.pos.trDelta[0];
    ent->s.pos.trDelta[1] = (float)(int)ent->s.pos.trDelta[1];
    ent->s.pos.trDelta[2] = (float)(int)ent->s.pos.trDelta[2];
    ent->s.apos.trType = 2;
    ent->s.apos.trTime = g_0864c56c;
    Vec3_ToAngles(dir, ent->s.apos.trBase);
    ent->s.apos.trBase[0] = AngleMod(ent->s.apos.trBase[0] - 120.0f);
    ent->s.apos.trDelta[0] = 720.0f + Rand_Float(-45.0f, 45.0f);
    ent->s.apos.trDelta[1] = 0.0f;
    ent->s.apos.trDelta[2] = 360.0f + Rand_Float(-45.0f, 45.0f);
    Vec3_Copy_0810ebae(origin, ent->r.currentOrigin);
    Vec3_Copy_0810ebae(ent->s.apos.trBase, ent->r.currentAngles);
    return ent;
}

// 0x0810e938  size=562  FUN_0810e938  status=translated  naming=inferred
// Spawns a linear-trajectory projectile (type 8, 30 s lifetime) at the weapon's launch speed; Quake III fire_rocket counterpart.
gentity_t *G_SpawnRocket(gentity_t *owner, const float *origin, float *dir)
{
    weaponDef_s *weapon;
    gentity_t *ent;

    Vec3_Normalize(dir);
    weapon = (weaponDef_s*)BG_GetWeaponDef(owner->s.weapon);
    ent = (gentity_s*)G_Spawn();
    ScrStr_AssignRef(&ent->classname, scrStr_rocket);
    ent->nextthink = g_0864c56c + 30000;
    ent->handler = 8;
    ent->s.eType = 4;
    ent->s.eFlags = ent->s.eFlags | 0x400;
    ent->r.svFlags = 8;
    ent->s.weapon = owner->s.weapon;
    ent->r.ownerNum = owner->s.number;
    ent->parent = owner;
    ent->damage = weapon->damage;
    ent->clipmask = 0x2802891;
    ent->s.time = g_0864c56c + 0x32;
    ent->s.pos.trType = 2;
    ent->s.pos.trTime = g_0864c56c - 0x32;
    Vec3_Copy_0810ebae(origin, ent->s.pos.trBase);
    Vec3_Scale_0810ec66(dir, (float)weapon->projectileSpeed, ent->s.pos.trDelta);
    ent->s.pos.trDelta[0] = (float)(int)ent->s.pos.trDelta[0];
    ent->s.pos.trDelta[1] = (float)(int)ent->s.pos.trDelta[1];
    ent->s.pos.trDelta[2] = (float)(int)ent->s.pos.trDelta[2];
    Vec3_Copy_0810ebae(origin, ent->r.currentOrigin);
    Vec3_ToAngles(dir, ent->r.currentAngles);
    G_SetAnglesTrajectory((ent), ent->r.currentAngles);
    *(float *)&ent->u.raw[0x0] = ((float)weapon->destabilizeDistance / (float)weapon->projectileSpeed) * 1000.0f;
    ent->flags = (owner->flags & 0x20000) | ent->flags;
    return ent;
}

// 0x0810eb6a  size=25  FUN_0810eb6a  status=translated  naming=inferred
// fabsf; used on the vertical trajectory velocity. No strings.
float Abs_Float(float x)
{
    return fabsf(x);
}

// 0x0810eb84  size=41  FUN_0810eb84  status=translated  naming=inferred
// VectorClear (zero a 3-vector).
void Vec3_Clear_0810eb84(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x0810ebae  size=47  FUN_0810ebae  status=translated  naming=inferred
// VectorCopy(src, dst). The binary copies the three words with integer moves (no x87 load/store), so NaN payloads survive.
void Vec3_Copy_0810ebae(const float *src, float *dst)
{
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
    ((uint32 *)dst)[2] = ((const uint32 *)src)[2];
}

// 0x0810ebde  size=68  FUN_0810ebde  status=translated  naming=inferred
// VectorAdd(a, b, out).
void Vec3_Add_0810ebde(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x0810ec22  size=68  FUN_0810ec22  status=translated  naming=inferred
// VectorSubtract(a, b, out) = a - b.
void Vec3_Subtract_0810ec22(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x0810ec66  size=56  FUN_0810ec66  status=translated  naming=inferred
// VectorScale(v, scale, out).
void Vec3_Scale_0810ec66(const float *v, float scale, float *out)
{
    out[0] = scale * v[0];
    out[1] = scale * v[1];
    out[2] = scale * v[2];
}

// 0x0810ec9e  size=83  FUN_0810ec9e  status=translated  naming=inferred
// VectorMA(base, scale, dir, out) = base + scale*dir.
void Vec3_MA_0810ec9e(const float *base, float scale, const float *dir, float *out)
{
    out[0] = base[0] + scale * dir[0];
    out[1] = base[1] + scale * dir[1];
    out[2] = base[2] + scale * dir[2];
}

// 0x0810ecf2  size=51  FUN_0810ecf2  status=translated  naming=inferred
// DotProduct; accumulated in extended precision (x87 return in st0, z term first).
long double Vec3_Dot_0810ecf2(const float *a, const float *b)
{
    /* gameplay-fix(wpn_grenade): the original sums x, y, z (0x0810ecfb..0x0810ed21), not z, y, x */
    return (long double)a[0] * (long double)b[0] +
           (long double)a[1] * (long double)b[1] +
           (long double)a[2] * (long double)b[2];
}

// 0x0810ed26  size=62  FUN_0810ed26  status=translated  naming=inferred
// VectorLength: sqrtf of the sum of squares (z, y, x order), result returned in st0.
float Vec3_Length_0810ed26(const float *v)
{
    /* gameplay-fix(wpn_grenade): x,y,z sum on the x87 stack, rounded once when passed to the sqrt wrapper (0x0810ed32..0x0810ed5a) */
    return Sqrt_Float((float)((long double)v[0] * (long double)v[0] + (long double)v[1] * (long double)v[1] +
                              (long double)v[2] * (long double)v[2]));
}

// 0x0810ed64  size=31  FUN_0810ed64  status=translated  naming=inferred
// Sqrt of a float via the double libm sqrt, narrowed back to float.
float Sqrt_Float(float x)
{
    double r = sqrt((double)x);
    return (float)r;
}

// 0x0810ed84  size=260  FUN_0810ed84  status=translated  naming=inferred
// G_TestEntityPosition counterpart: boxes the entity at `origin` and returns the entity it is stuck in, or NULL.
gentity_t *G_TestEntityPosition(gentity_t *ent, const float *origin)
{
    trace_t tr;
    unsigned int mask;

    if (ent->clipmask == 0) {
        mask = 0x811;
    } else {
        if ((ent->r.contents & 0x4000000) != 0) {
            return 0;
        }
        mask = ent->clipmask;
    }
    if (ent->s.eType == 4) {
        G_TraceBox(&tr, origin, ent->r.mins, ent->r.maxs, origin, ent->r.ownerNum, mask);
    } else {
        G_TraceBox(&tr, origin, ent->r.mins, ent->r.maxs, origin, ent->s.number, mask);
    }
    if (tr.startsolid == 0 && tr.allsolid == 0) { /* canonical names: 0x22 allsolid, 0x23 startsolid (swapped vs the original chunk header) */
        return 0;
    }
    return &g_gentities[tr.entityNum];
}

// 0x0810ee88  size=60  FUN_0810ee88  status=translated  naming=inferred
// AnglesToAxis_0810ee88: forward/right/up from Euler angles, then the second row (right) is negated.
void AnglesToAxis_0810ee88(const float *angles, float axis[3][3])
{
    Angles_ToVectors(angles, axis[0], axis[1], axis[2]);
    Vec3_Negate_b(axis[1]);
}

// 0x0810eec4  size=107  FUN_0810eec4  status=translated  naming=inferred
// 3x3 matrix transpose: out[r][c] = in[c][r].
void Matrix3_Transpose(const float in[3][3], float out[3][3])
{
    int r;
    int c;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            ((uint32 *)out[r])[c] = ((const uint32 *)in[c])[r]; /* integer word move, no x87 (preserves NaN payloads) */
        }
    }
}

// 0x0810ef30  size=107  FUN_0810ef30  status=translated  naming=inferred
// Rotates `point` in place by the 3x3 matrix (row i dotted with the original point).
void Matrix3_RotatePoint(float *point, const float matrix[3][3])
{
    vec3_t orig;

    Vec3_Copy_b(point, orig);
    point[0] = (float)Vec3_Dot_b(matrix[0], orig);
    point[1] = (float)Vec3_Dot_b(matrix[1], orig);
    point[2] = (float)Vec3_Dot_b(matrix[2], orig);
}

// 0x0810ef9c  size=1363  FUN_0810ef9c  status=translated  naming=inferred
// G_TryPushingEntity counterpart: moves `check` by the pusher's linear and rotational delta; if blocked, probes nearby offsets before giving up.
// Reads/writes the push undo stack top (g_pushedStackTop) and the client's yaw accumulator (+0x58).
int G_TryPushingEntity(gentity_t *check, gentity_t *pusher, const float *move, const float *amove)
{
    vec3_t newOrigin;       /* local_dc */
    float axis[3][3];       /* local_ac */
    float axisInv[3][3];    /* local_7c */
    vec3_t rel;             /* local_1c */
    vec3_t rotated;         /* local_2c */
    vec3_t shift;           /* local_3c */
    gentity_t *obstacle;

    Vec3_Add_b(check->r.currentOrigin, move, newOrigin);
    AnglesToAxis_0810ee88(amove, axis);
    Matrix3_Transpose(axis, axisInv);
    Vec3_Subtract_b(newOrigin, pusher->r.currentOrigin, rel);
    Vec3_Copy_b(rel, rotated);
    Matrix3_RotatePoint(rotated, axisInv);
    Vec3_Subtract_b(rotated, rel, shift);
    Vec3_Add_b(newOrigin, shift, newOrigin);
    obstacle = G_TestEntityPosition(check, newOrigin);
    if (obstacle == 0) {
        if (check->s.groundEntityNum != pusher->s.number) {
            check->s.groundEntityNum = 0x3ff;
        }
        Vec3_Copy_b(newOrigin, check->r.currentOrigin);
        Vec3_Copy_b(newOrigin, check->s.pos.trBase);
        if (check->client != 0) {
            check->client->ps.delta_angles[1] = ((int)(amove[1] * 182.04445f) & 0xffff) + check->client->ps.delta_angles[1];
            Vec3_Copy_b(newOrigin, check->client->ps.origin);
        }
        g_pushedStackTop = (pushed_t *)((char *)g_pushedStackTop + 0x20);
        return 1;
    }
    if (4.0 < check->r.maxs[0] / 2.0) {
        float zMag;
        float zOff;
        float xMag;
        float xOff;
        float yMag;
        float yOff;
        vec3_t base;        /* local_1c reused as the probe base */
        vec3_t offset;      /* local_3c */
        vec3_t probe;       /* local_2c */

        Vec3_Copy_b(newOrigin, base);
        for (zMag = 0.0f; zMag < check->r.maxs[0] / 2.0; zMag = zMag + 4.0f) {
            for (zOff = -zMag; zOff <= zMag; zOff = zOff + zMag + zMag) {
                for (xMag = 4.0f; xMag < check->r.maxs[0] / 2.0; xMag = xMag + 4.0f) {
                    for (xOff = -xMag; xOff <= xMag; xOff = xOff + xMag + xMag) {
                        for (yMag = 4.0f; yMag < check->r.maxs[0] / 2.0; yMag = yMag + 4.0f) {
                            for (yOff = -yMag; yOff <= yMag; yOff = yOff + yMag + yMag) {
                                Vec3_Set_b(offset, xOff, yOff, zOff);
                                Vec3_Add_b(base, offset, probe);
                                obstacle = G_TestEntityPosition(check, probe);
                                if (obstacle == 0) {
                                    if (check->s.groundEntityNum != pusher->s.number) {
                                        check->s.groundEntityNum = 0x3ff;
                                    }
                                    Vec3_Copy_b(probe, check->r.currentOrigin);
                                    Vec3_Copy_b(probe, check->s.pos.trBase);
                                    if (check->client != 0) {
                                        check->client->ps.delta_angles[1] = ((int)(amove[1] * 182.04445f) & 0xffff) + check->client->ps.delta_angles[1];
                                        Vec3_Copy_b(probe, check->client->ps.origin);
                                    }
                                    g_pushedStackTop = (pushed_t *)((char *)g_pushedStackTop + 0x20);
                                    return 1;
                                }
                            }
                        }
                    }
                }
                if (zOff == 0.0f) {
                    break;
                }
            }
        }
    }
    if (G_TestEntityPosition(check, check->r.currentOrigin) == 0) {
        check->s.groundEntityNum = 0x3ff;
        return 1;
    }
    return 0;
}

// 0x0810f4f0  size=1562  FUN_0810f4f0  status=translated  naming=inferred
// G_MoverPush counterpart: moves `pusher` by move/amove, pushes every overlapping or riding entity, and returns 0 (with *obstacle set) if one is blocked.
// Pushed state is logged in the push undo stack g_pushedStack/g_pushedStackTop.
char G_MoverPush(gentity_t *pusher, const float *move, const float *amove, gentity_t **obstacle)
{
    char result;
    float totalMins[3];     /* local_2c */
    float totalMaxs[3];     /* local_3c */
    float mins[3];          /* local_205c */
    float maxs[3];          /* local_206c */
    int entityList[1024];   /* local_103c */
    int pushList[1024];     /* aiStack_203c */
    int numEntities;
    int numPush;
    int i;
    gentity_t *check;

    *obstacle = 0;
    result = 1;
    if (pusher->r.currentAngles[0] != 0.0f || pusher->r.currentAngles[1] != 0.0f || pusher->r.currentAngles[2] != 0.0f ||
        amove[0] != 0.0f || amove[1] != 0.0f || amove[2] != 0.0f) {
        float radius = Vec3_MaxAbsLength(pusher->r.mins, pusher->r.maxs);
        for (i = 0; i < 3; i++) {
            totalMins[i] = (pusher->r.currentOrigin[i] - radius) + move[i];
            totalMaxs[i] = pusher->r.currentOrigin[i] + radius + move[i];
            mins[i] = pusher->r.currentOrigin[i] - radius;
            maxs[i] = pusher->r.currentOrigin[i] + radius;
        }
    } else {
        for (i = 0; i < 3; i++) {
            totalMins[i] = pusher->r.absmin[i] + move[i];
            totalMaxs[i] = pusher->r.absmax[i] + move[i];
        }
        Vec3_Copy_b(pusher->r.absmin, mins);
        Vec3_Copy_b(pusher->r.absmax, maxs);
    }
    for (i = 0; i < 3; i++) {
        if (0.0f < move[i]) {
            maxs[i] = maxs[i] + move[i];
        } else {
            mins[i] = mins[i] + move[i];
        }
    }
    SV_UnlinkEntity(pusher);
    numEntities = CM_AreaEntities(mins, maxs, entityList, 0x400, 0x2000180);
    Vec3_Add_b(pusher->r.currentOrigin, move, pusher->r.currentOrigin);
    Vec3_Add_b(pusher->r.currentAngles, amove, pusher->r.currentAngles);
    SV_LinkEntity(pusher);
    numPush = 0;
    for (i = 0; i < numEntities; i++) {
        int num = entityList[i];
        check = &g_gentities[num];
        if ((check->s.eType == 4 || check->s.eType == 3 || check->s.eType == 1 || check->physicsObject != 0) &&
            (check->s.groundEntityNum == pusher->s.number ||
             (check->r.absmin[0] < totalMaxs[0] && check->r.absmin[1] < totalMaxs[1] && check->r.absmin[2] < totalMaxs[2] &&
              totalMins[0] < check->r.absmax[0] && totalMins[1] < check->r.absmax[1] && totalMins[2] < check->r.absmax[2] &&
              G_TestEntityPosition(check, check->r.currentOrigin) == pusher))) {
            pushList[numPush] = entityList[i];
            numPush++;
        }
    }
    for (i = 0; i < numPush; i++) {
        check = &g_gentities[pushList[i]];
        SV_UnlinkEntity(check);
    }
    for (i = 0; i < numPush; i++) {
        int num = pushList[i];
        check = &g_gentities[num];
        g_pushedStackTop->ent = check;
        Vec3_Copy_b(check->r.currentOrigin, g_pushedStackTop->origin);
        g_pushedStackTop->deltaYaw = amove[1];
        if (G_TryPushingEntity(check, pusher, move, amove) == 0) {
            if (check->s.eType == 3) {
                SV_LinkEntity(check);
            } else if (pusher->s.pos.trType == 4 || pusher->s.apos.trType == 4) {
                G_Damage(check, pusher, pusher, 0, 0, 99999, 0, 9, 0, 0);
            } else {
                *obstacle = check;
                result = 0;
            }
        } else {
            SV_LinkEntity(check);
        }
    }
    for (i = 0; i < numPush; i++) {
        check = &g_gentities[pushList[i]];
        SV_LinkEntity(check);
    }
    return result;
}

// 0x0810fb0a  size=756  FUN_0810fb0a  status=translated  naming=inferred
// G_RunMover counterpart: evaluates the move/rotate trajectories, pushes, and on a block rolls back the push stack and shifts the trajectory times; fires the entClass callbacks.
void G_RunMover(gentity_t *ent)
{
    gentity_t *obstacle;
    vec3_t newOrigin;       /* local_4c */
    vec3_t newAngles;       /* local_5c */
    vec3_t move;            /* local_1c */
    vec3_t amove;           /* local_2c */
    char ok;

    obstacle = 0;
    g_pushedStackTop = g_pushedStack;
    BG_EvaluateTrajectory((&ent->s.pos), g_0864c56c, newOrigin);
    BG_EvaluateTrajectory((&ent->s.apos), g_0864c56c, newAngles);
    Vec3_Subtract_b(newOrigin, ent->r.currentOrigin, move);
    Vec3_Subtract_b(newAngles, ent->r.currentAngles, amove);
    ok = G_MoverPush(ent, move, amove, &obstacle);
    if (ok == 0) {
        pushed_t *top = g_pushedStackTop;
        pushed_t *rec;

        while (rec = top - 1, rec >= g_pushedStack) {
            gentity_t *pushedEnt = rec->ent;
            Vec3_Copy_b(rec->origin, pushedEnt->r.currentOrigin);
            Vec3_Copy_b(rec->origin, pushedEnt->s.pos.trBase);
            if (pushedEnt->client != 0) {
                pushedEnt->client->ps.delta_angles[1] = pushedEnt->client->ps.delta_angles[1] - ((int)(rec->deltaYaw * 182.04445f) & 0xffff);
                Vec3_Copy_b(rec->origin, pushedEnt->client->ps.origin);
            }
            SV_LinkEntity(pushedEnt);
            top = rec;
        }
        ent->s.pos.trTime = (g_0864c56c - level_previousTime) + ent->s.pos.trTime;
        ent->s.apos.trTime = (g_0864c56c - level_previousTime) + ent->s.apos.trTime;
        BG_EvaluateTrajectory((&ent->s.pos), g_0864c56c, ent->r.currentOrigin);
        BG_EvaluateTrajectory((&ent->s.apos), g_0864c56c, ent->r.currentAngles);
        SV_LinkEntity(ent);
        if (g_entityHandlerTable[ent->handler].onBlocked != 0) {
            g_entityHandlerTable[ent->handler].onBlocked(ent, obstacle);
        }
    } else {
        if (ent->s.pos.trType != 0 && ent->s.pos.trDuration + ent->s.pos.trTime <= g_0864c56c &&
            g_entityHandlerTable[ent->handler].onTrajectoryEnd != 0) {
            g_entityHandlerTable[ent->handler].onTrajectoryEnd(ent);
        }
        if (ent->s.apos.trType != 0 && ent->s.apos.trDuration + ent->s.apos.trTime <= g_0864c56c &&
            g_entityHandlerTable[ent->handler].onTrajectoryEnd != 0) {
            g_entityHandlerTable[ent->handler].onTrajectoryEnd(ent);
        }
    }
}

// 0x0810fdfe  size=75  FUN_0810fdfe  status=translated  naming=inferred
// Per-frame entity runner for mover-class entities: runs the mover when it has a trajectory, otherwise defers to Ent_ResetAttachState when +0x208 is set; then the generic think.
void G_RunMoverEntity(gentity_t *ent)
{
    if (ent->tagInfo == 0) {
        if (ent->s.pos.trType != 0 || ent->s.apos.trType != 0) {
            G_RunMover(ent);
        }
    } else {
        Ent_ResetAttachState((ent));
    }
    G_RunThink(ent);
}

// 0x0810fe4a  size=5  FUN_0810fe4a  status=translated  naming=unknown
// Empty function (no-op callback). No strings or callers evidence beyond the empty body.
void Nop_0810fe4a(void)
{
}

// 0x0810fe50  size=493  FUN_0810fe50  status=translated  naming=inferred
// trigger_use spawn function (strings "cursorhint", "HINT_INHERIT", "hintstring", "...trigger_use entities").
void SP_trigger_use(gentity_t *ent)
{
    const char *value;
    char buf[0x400];
    int idx;
    unsigned int i;

    SV_SetBrushModel(ent);
    SV_LinkEntity(ent);
    *(int *)&ent->u.raw[0xc] = 0x3ff;
    ent->s.pos.trType = 0;
    Vec3_Copy_b(ent->r.currentOrigin, ent->s.pos.trBase);
    ent->r.contents = 0x200000;
    ent->r.svFlags = 1;
    ent->handler = 0x12;
    ent->s.dmgFlags = 2;
    idx = G_SpawnString("cursorhint", "", &value);
    if (idx != 0) {
        if (Q_stricmp(value, "HINT_INHERIT") == 0) {
            ent->s.dmgFlags = -1;
        } else {
            for (i = 1; i < 6; i++) {
                if (Q_stricmp(value, g_cursorHintNames[i]) == 0) {
                    ent->s.dmgFlags = i;
                    break;
                }
            }
        }
    }
    ent->s.scale = 0xff;
    idx = G_SpawnString("hintstring", "", &value);
    if (idx != 0) {
        for (i = 0; (int)i < 0x20; i++) {
            SV_GetConfigstring(i + 0x4fe, buf, 0x400);
            if (buf[0] == '\0') {
                SV_SetConfigstring(i + 0x4fe, value);
                ent->s.scale = i & 0xff;
                break;
            }
            if (strcmp(value, buf) == 0) {
                ent->s.scale = i & 0xff;
                break;
            }
        }
        if (i == 0x20) {
            Com_Error(1, "\x15Too many different hintstring key values on trigger_use entities. Max allowed is %i", 0x20);
        }
    }
}

/* ===== second vector helper copy (duplicate of the helpers above, in a different object file) ===== */

// 0x08110066  size=35  FUN_08110066  status=translated  naming=inferred
// VectorSet(v, x, y, z).
void Vec3_Set_b(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// 0x0811008a  size=47  FUN_0811008a  status=translated  naming=inferred
// VectorCopy (second copy of Vec3_Copy_0810ebae); integer word moves like the original.
void Vec3_Copy_b(const float *src, float *dst)
{
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
    ((uint32 *)dst)[2] = ((const uint32 *)src)[2];
}

// 0x081100ba  size=62  FUN_081100ba  status=translated  naming=inferred
// VectorNegate by flipping each sign bit.
void Vec3_Negate_b(float *v)
{
    unsigned int *u = (unsigned int *)v;
    u[0] = u[0] ^ 0x80000000u;
    u[1] = u[1] ^ 0x80000000u;
    u[2] = u[2] ^ 0x80000000u;
}

// 0x081100f8  size=68  FUN_081100f8  status=translated  naming=inferred
// VectorAdd (second copy).
void Vec3_Add_b(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x0811013c  size=68  FUN_0811013c  status=translated  naming=inferred
// VectorSubtract (second copy).
void Vec3_Subtract_b(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x08110180  size=51  FUN_08110180  status=translated  naming=inferred
// DotProduct (second copy), extended precision, z term first.
long double Vec3_Dot_b(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2] +
           (long double)a[1] * (long double)b[1] +
           (long double)a[0] * (long double)b[0];
}

/* ===== game-script glue ===== */

// 0x081101b4  size=27  FUN_081101b4  status=translated  naming=unknown
// Interns a C string as a script string and returns its id (tail value of ScrStr_InternCStringFlag1 in eax; 0x081101b4, requests/L17-1).
unsigned short G_NewScriptString(const char *string)
{
    return (unsigned short)ScrStr_InternCStringFlag1(string); /* the binary also stores a constant 1 in the unused second stack slot */
}

// 0x081101d0  size=56  FUN_081101d0  status=translated  naming=inferred
// Runs the level script's main() (offset cached in g_levelMainScriptOffset) and releases the thread handle.
void GScr_RunLevelMain(void)
{
    unsigned short handle;

    if (g_levelMainScriptOffset != 0) {
        handle = Scr_CallScriptOffset(g_levelMainScriptOffset, 0);
        Scr_ReleaseHandle(handle);
    }
}

// 0x08110208  size=126  FUN_08110208  status=translated  naming=observed
// Resolves script+label to a function offset; optionally reports "Could not find script/label" via Com_Error(1). Strings observed.
int GScr_GetFunctionOffset(const char *script, const char *label, int errorIfMissing)
{
    int offset;

    offset = Scr_LoadScriptFile(script);
    if (offset == 0 && errorIfMissing != 0) {
        Com_Error(1, "\x15Could not find script '%s'", script);
    }
    offset = Scr_ResolveFunctionOffset(script, label);
    if (offset == 0 && errorIfMissing != 0) {
        Com_Error(1, "\x15Could not find label '%s' in script '%s'", label, script);
    }
    return offset;
}

// 0x08110286  size=244  FUN_08110286  status=translated  naming=observed
// Loads the gametype script maps/mp/gametypes/<g_gametype> main and the CodeCallback_* entry points from _callbacksetup.
void GScr_LoadGametypeScript(void)
{
    char path[72];

    Com_sprintf(path, 0x40, "maps/mp/gametypes/%s", g_087a2400->current.string);
    g_gametypeMainScriptOffset = GScr_GetFunctionOffset(path, "main", 1);
    g_0884d70c = GScr_GetFunctionOffset("maps/mp/gametypes/_callbacksetup", "CodeCallback_StartGameType", 1);
    g_0884d710 = GScr_GetFunctionOffset("maps/mp/gametypes/_callbacksetup", "CodeCallback_PlayerConnect", 1);
    g_0884d714 = GScr_GetFunctionOffset("maps/mp/gametypes/_callbacksetup", "CodeCallback_PlayerDisconnect", 1);
    g_0884d718 = GScr_GetFunctionOffset("maps/mp/gametypes/_callbacksetup", "CodeCallback_PlayerDamage", 1);
    g_0884d71c = GScr_GetFunctionOffset("maps/mp/gametypes/_callbacksetup", "CodeCallback_PlayerKilled", 1);
}

// 0x0811037a  size=108  FUN_0811037a  status=translated  naming=observed
// Loads maps/mp/<mapname> main (optional; no error if missing). Dvar "mapname" registered with flags 0x1044.
void GScr_LoadLevelScript(void)
{
    struct dvar_s *mapname;
    char path[72];

    mapname = Dvar_RegisterString("mapname", "", 0x1044);
    Com_sprintf(path, 0x40, "maps/mp/%s", mapname->current.string);
    g_levelMainScriptOffset = GScr_GetFunctionOffset(path, "main", 0);
}

// 0x081103e6  size=56  FUN_081103e6  status=translated  naming=inferred
// Initialises script classes 0..3, then three out-of-chunk initialisers.
void GScr_InitClasses(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        Scr_InitClass(i);
    }
    G_RegisterEntityScriptFields();
    HudElem_RegisterScriptFields();
    G_LoadRadiantFieldFiles();
}

// 0x0811041e  size=137  FUN_0811041e  status=translated  naming=observed
// Script load sequence: compile state, codescripts/delete + codescripts/struct entry points, gametype and level scripts, then class init.
void GScr_LoadScripts(void)
{
    Scr_InitCompileState();
    g_deleteScriptMainOffset = GScr_GetFunctionOffset("codescripts/delete", "main", 1);
    g_0884e7b0 = GScr_GetFunctionOffset("codescripts/struct", "initstructs", 1);
    g_0884e7b4 = GScr_GetFunctionOffset("codescripts/struct", "createstruct", 1);
    GScr_LoadGametypeScript();
    GScr_LoadLevelScript();
    Scr_ReleasePools();
    GScr_InitClasses();
    Scr_ShutdownCompileState();
}

// 0x081104a8  size=41  FUN_081104a8  status=translated  naming=inferred
// Shuts down script classes 0..3.
void GScr_ShutdownClasses(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        Scr_ShutdownClass(i);
    }
}

// 0x081104d2  size=57  FUN_081104d2  status=translated  naming=observed
// Entity reference (low 16 bits = entity number, high 16 bits must be 0) to entity pointer; "not an entity" script error otherwise.
gentity_t *GScr_GetEntityFromRef(unsigned int ref)
{
    gentity_t *ent;

    if ((ref >> 16) == 0) {
        ent = &g_gentities[ref & 0xffff];
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = 0;
    }
    return ent;
}

// 0x0811050c  size=180  FUN_0811050c  status=translated  naming=observed
// Like GScr_GetEntityFromRef but the entity must be a player (has a client); otherwise Scr_Error with entity number, origin, classname and targetname.
gentity_t *GScr_GetPlayerEntity(unsigned int ref)
{
    gentity_t *ent;
    const char *targetname;
    const char *classname;
    const char *msg;

    ent = (gentity_s*)GScr_GetEntityFromRef(ref);
    if (ent->client == 0) {
        if (ent->targetname == 0) {
            targetname = "<undefined>";
        } else {
            targetname = ScrStr_TextFromHandle(ent->targetname);
        }
        classname = ScrStr_TextFromHandle(ent->classname);
        msg = va("only valid on players; called on entity %i at %.0f %.0f %.0f classname %s targetname %s\n",
                 (int)(ref & 0xffff), (double)ent->r.currentOrigin[0], (double)ent->r.currentOrigin[1],
                 (double)ent->r.currentOrigin[2], classname, targetname);
        Scr_Error(msg);
    }
    return ent;
}

// 0x081105c0  size=20  FUN_081105c0  status=translated  naming=observed
// Script method getclanid (builtin method table at 0x08188720): returns the string "0".
void ScriptEntCmd_getclanid(void)
{
    Scr_AddString("0");
}

// 0x081105d4  size=20  FUN_081105d4  status=translated  naming=observed
// Script method getclanname (method table): returns an empty string.
void ScriptEntCmd_getclanname(void)
{
    Scr_AddString("");
}

// 0x081105e8  size=20  FUN_081105e8  status=translated  naming=observed
// Script method getclanmotto (method table): returns an empty string.
void ScriptEntCmd_getclanmotto(void)
{
    Scr_AddString("");
}

// 0x08110624  size=80  FUN_08110624  status=translated  naming=observed
// Script builtin print (function table at 0x08188040): prints each argument with "%s" when the gating dvar byte is zero.
void GScr_print(void)
{
    int n;
    int i;

    if (g_087a24b0->current.enabled == '\0') {
        n = Scr_GetNumParams();
        for (i = 0; i < n; i++) {
            Com_Printf("%s", Scr_GetString(i));
        }
    }
}

// 0x08110674  size=38  FUN_08110674  status=translated  naming=observed
// Script builtin println: print plus newline under the same dvar gate.
void GScr_println(void)
{
    if (g_087a24b0->current.enabled == '\0') {
        GScr_print();
        Com_Printf("\n");
    }
}

// 0x0811069a  size=27  FUN_0811069a  status=translated  naming=unknown
// Raises Com_Error(6, message) with the message used directly as the format string; `param` is ignored.
void Scr_RaiseError6(int param, const char *msg)
{
    Com_Error(6, msg);
}

// 0x081106b6  size=106  FUN_081106b6  status=translated  naming=observed
// Validates a localized-string reference: every character must be alphanumeric or '_' (only checked when length > 1).
void GScr_CheckLocalizedRef(int param, const char *str, int len)
{
    int i;

    if (1 < len) {
        for (i = 0; i < len; i++) {
            if (isalnum((int)str[i]) == 0 && str[i] != '_') {
                Scr_ErrorAtParam(param, va("Illegal localized string reference: %s must contain only alpha-numeric characters and underscores", str));
            }
        }
    }
}

// 0x08110720  size=831  FUN_08110720  status=translated  naming=observed
// Concatenates script parameters first..last into `out` with 0x14 (localized separator) / 0x15 (literal/player-name marker) prefixes; control bytes inside text become '.'.
void GScr_BuildMessageString(int first, int last, const char *what, char *out, int outSize)
{
    bool canMarkLiteral = true;
    int outLen = 0;
    int param = first;
    const char *str;
    size_t len;
    int i;
    int type;

    for (;;) {
        if (last < param) {
            out[outLen] = 0;
            return;
        }
        type = (int)Scr_GetParamType(param);
        if (type == 3) {
            str = Scr_ReleaseLocalizedString(param);
            len = strlen(str);
            GScr_CheckLocalizedRef(param, str, (int)len);
            if (outSize <= (int)(len + outLen + 1)) {
                Scr_ErrorAtParam(param, va("%s is too long. Max length is %i\n", what, outSize));
            }
            if (outLen != 0) {
                out[outLen] = 0x14;
                outLen = outLen + 1;
            }
            canMarkLiteral = true;
        } else if (type == 1 && Scr_GetPointer(param) == 0x15) {
            gentity_t *ent = Scr_GetEntityParam(param);
            if (ent->client == 0) {
                Scr_ErrorAtParam(param, "Entity is not a player");
            }
            str = va("%s^7", ent->client->cs.name);
            len = strlen(str);
            if (outSize <= (int)(len + outLen + 1)) {
                Scr_ErrorAtParam(param, va("%s is too long. Max length is %i\n", what, outSize));
            }
            if (canMarkLiteral) {
                out[outLen] = 0x15;
                outLen = outLen + 1;
            }
            canMarkLiteral = false;
        } else {
            str = Scr_ReleaseAnyValue(param);
            len = strlen(str);
            for (i = 0; i < (int)len; i++) {
                if (str[i] == '\x14' || str[i] == '\x15' || str[i] == '\x16') {
                    Scr_ErrorAtParam(param, va("bad escape character (%i) present in string", (int)str[i]));
                }
                if (isalpha((int)str[i]) != 0) {
                    if (g_loc_warningsAsErrors->current.enabled == '\0') {
                        Com_Printf("^3WARNING: Non-localized %s string is not allowed to have letters in it. Must be changed over to a localized string: \"%s\"\n", what, str);
                    } else {
                        Scr_RaiseError6(param, va("non-localized %s strings are not allowed to have letters in them: \"%s\"", what, str));
                    }
                    break;
                }
            }
            if (outSize <= (int)(len + outLen + 1)) {
                Scr_ErrorAtParam(param, va("%s is too long. Max length is %i\n", what, outSize));
            }
            if (canMarkLiteral) {
                out[outLen] = 0x15;
                outLen = outLen + 1;
            }
            canMarkLiteral = false;
        }
        for (i = 0; i < (int)len; i++) {
            if (str[i] == '\x14' || str[i] == '\x15' || str[i] == '\x16') {
                out[outLen] = 0x2e;
            } else {
                out[outLen] = str[i];
            }
            outLen = outLen + 1;
        }
        param = param + 1;
    }
}

// 0x08110a60  size=113  FUN_08110a60  status=translated  naming=observed
// Builds a "Game Message" string from all script arguments and sends `<cmd> "<text>"` to the client (iprintln-style helper; sender is SV_SendServerCommandNum).
void GScr_SendGameMessage(int clientNum, const char *cmd)
{
    int numParams;
    char text[0x408];

    numParams = Scr_GetNumParams();
    GScr_BuildMessageString(0, numParams - 1, "Game Message", text, 0x400);
    SV_SendServerCommandNum(clientNum, 0, va("%s \"%s\"", cmd, text));
}

// 0x08110c02  size=5  FUN_08110c02  status=translated  naming=observed
// Script builtin "line" (function table entry): empty body.
void GScr_line(void)
{
}

// 0x08111e90  size=5  FUN_08111e90  status=translated  naming=observed
// Script method playrumble (method table): empty body.
void ScriptEntCmd_playrumble(void)
{
}

// 0x08111e96  size=5  FUN_08111e96  status=translated  naming=observed
// Script method playlooprumble (method table): empty body.
void ScriptEntCmd_playlooprumble(void)
{
}

// 0x08111e9c  size=5  FUN_08111e9c  status=translated  naming=observed
// Script method stoprumble (method table): empty body.
void ScriptEntCmd_stoprumble(void)
{
}

// 0x08112432  size=195  FUN_08112432  status=translated  naming=inferred
// Finds `str` in the hintstring config strings (0x4fe..0x51d) or claims the first empty slot; returns 1 with the index, or 0 with -1 when all 32 are taken.
int G_FindOrAddHintString(int *outIndex, const char *str)
{
    char buf[0x40c];
    int i;

    i = 0;
    for (;;) {
        if (0x1f < i) {
            *outIndex = -1;
            return 0;
        }
        SV_GetConfigstring(i + 0x4fe, buf, 0x400);
        if (buf[0] == '\0') {
            break;
        }
        if (strcmp(str, buf) == 0) {
            *outIndex = i;
            return 1;
        }
        i = i + 1;
    }
    SV_SetConfigstring(i + 0x4fe, str);
    *outIndex = i;
    return 1;
}

// 0x08112796  size=56  FUN_08112796  status=translated  naming=inferred
// Clears all 16 script objective slots.
void G_ClearObjectives(void)
{
    int i;

    for (i = 0; i < 0x10; i++) {
        G_ClearObjective((&level_objectives[i]));
    }
}

// 0x081127ce  size=105  FUN_081127ce  status=translated  naming=inferred
// Maps one of three known script string ids to an index (scr_str_empty -> 0, scr_str_invisible -> 2, scr_str_current -> 4); returns 0 with *out=0 otherwise.
int G_ScriptStringToIndex(int *out, unsigned int stringId)
{
    if (stringId == scr_str_empty) {
        *out = 0;
    } else if (stringId == scr_str_invisible) {
        *out = 2;
    } else {
        if (stringId != scr_str_current) {
            *out = 0;
            return 0;
        }
        *out = 4;
    }
    return 1;
}

// 0x08112838  size=88  FUN_08112838  status=translated  naming=inferred
// Detaches an objective from its entity: clears bit 0x10 of entity byte +0xf2 when entity byte +0xfc is set, then marks the objective entity-less (0x3ff).
void G_ClearObjectiveEntity(objective_t *obj)
{
    gentity_t *ent;

    if (obj->entNum != 0x3ff) {
        ent = &g_gentities[obj->entNum];
        if (ent->r.inuse != 0) {
            ent->r.svFlags = ent->r.svFlags & 0xef;
        }
        obj->entNum = 0x3ff;
    }
}

// 0x08112890  size=61  FUN_08112890  status=translated  naming=inferred
// Resets one objective slot to its empty state (no entity, no icon).
void G_ClearObjective(objective_t *obj)
{
    obj->state = 0;
    Vec3_Clear_0811864a(obj->origin);
    obj->entNum = 0x3ff;
    obj->teamNum = 0;
    obj->icon = 0;
}

// 0x081128ce  size=205  FUN_081128ce  status=translated  naming=observed
// objective_icon helper: validates the icon name argument (printable, < 64 chars; errors reported against param 3) and stores its index in objective+0x18.
void G_SetObjectiveIcon(objective_t *obj, int param)
{
    const char *name;
    int i;

    name = Scr_ReleaseAnyValue(param);
    for (i = 0; name[i] != '\0'; i++) {
        if (name[i] < ' ' || name[i] == '\x7f') {
            Scr_ErrorAtParam(3, va("Illegal character '%c'(ascii %i) in objective icon name: %s\n", (int)name[i], (unsigned int)(unsigned char)name[i], name));
        }
    }
    if (0x3f < i) {
        Scr_ErrorAtParam(3, va("Objective icon name is too long (> %i): %s\n", 0x3f, name));
    }
    obj->icon = G_ShaderIndex(name);
}

// 0x0811344a  size=146  FUN_0811344a  status=translated  naming=observed
// Menu config-string index (0x4de..0x4fd) for a precached menu name; script error "Menu '%s' was not precached" otherwise (returns 0).
int G_MenuIndex(const char *name)
{
    char buf[0x40c];
    int i;

    i = 0;
    for (;;) {
        if (0x1f < i) {
            Scr_Error(va("Menu '%s' was not precached\n", name));
            return 0;
        }
        SV_GetConfigstring(i + 0x4de, buf, 0x400);
        if (Q_stricmp(buf, name) == 0) {
            break;
        }
        i = i + 1;
    }
    return i;
}

// 0x081134dc  size=246  FUN_081134dc  status=translated  naming=observed
// Script builtin precachestatusicon (function table): registers a status icon name in config strings 0x17..0x1e (max 8); duplicates only log a developer message.
void GScr_PrecacheStatusIcon(void)
{
    char buf[0x408];
    const char *name;
    int i;

    name = Scr_ReleaseAnyValue(0);
    i = 0;
    for (;;) {
        if (7 < i) {
            i = 0;
            while (i < 8) {
                SV_GetConfigstring(i + 0x17, buf, 0x400);
                if (buf[0] == '\0') {
                    break;
                }
                i = i + 1;
            }
            if (i == 8) {
                Scr_Error(va("Too many player status icons precached. Max allowed is %i", 8));
            }
            SV_SetConfigstring(i + 0x17, name);
            return;
        }
        SV_GetConfigstring(i + 0x17, buf, 0x400);
        if (Q_stricmp(buf, name) == 0) {
            break;
        }
        i = i + 1;
    }
    Com_DPrintf("Script tried to precache the player status icon '%s' more than once\n", name);
}

// 0x081135d2  size=168  FUN_081135d2  status=translated  naming=observed
// 1-based status-icon index for a precached name; 0 for an empty name; script error "Status icon '%s' was not precached" if missing.
int G_StatusIconIndex(const char *name)
{
    char buf[0x40c];
    int i;

    if (*name != '\0') {
        for (i = 0; i < 8; i++) {
            SV_GetConfigstring(i + 0x17, buf, 0x400);
            if (Q_stricmp(buf, name) == 0) {
                return i + 1;
            }
        }
        Scr_Error(va("Status icon '%s' was not precached\n", name));
    }
    return 0;
}

// 0x0811367a  size=246  FUN_0811367a  status=translated  naming=observed
// Script builtin precacheheadicon (function table): registers a head icon in config strings 0x1f..0x2d (max 15).
void GScr_PrecacheHeadIcon(void)
{
    char buf[0x408];
    const char *name;
    int i;

    name = Scr_ReleaseAnyValue(0);
    i = 0;
    for (;;) {
        if (0xe < i) {
            i = 0;
            while (i < 0xf) {
                SV_GetConfigstring(i + 0x1f, buf, 0x400);
                if (buf[0] == '\0') {
                    break;
                }
                i = i + 1;
            }
            if (i == 0xf) {
                Scr_Error(va("Too many player head icons precached. Max allowed is %i", 0xf));
            }
            SV_SetConfigstring(i + 0x1f, name);
            return;
        }
        SV_GetConfigstring(i + 0x1f, buf, 0x400);
        if (Q_stricmp(buf, name) == 0) {
            break;
        }
        i = i + 1;
    }
    Com_DPrintf("Script tried to precache the player head icon '%s' more than once\n", name);
}

// 0x08113770  size=168  FUN_08113770  status=translated  naming=observed
// 1-based head-icon index for a precached name; 0 for an empty name; script error "Head icon '%s' was not precached" if missing.
int G_HeadIconIndex(const char *name)
{
    char buf[0x40c];
    int i;

    if (*name != '\0') {
        for (i = 0; i < 0xf; i++) {
            SV_GetConfigstring(i + 0x1f, buf, 0x400);
            if (Q_stricmp(buf, name) == 0) {
                return i + 1;
            }
        }
        Scr_Error(va("Head icon '%s' was not precached\n", name));
    }
    return 0;
}

// 0x08113818  size=561  FUN_08113818  status=translated  naming=observed
// Script builtin bullettrace (function table): params start, end, hitCharacters, ignoreEntity; returns an object with fraction, position, entity, normal and surfacetype.
void GScr_bulletTrace(void)
{
    trace_t tr;
    vec3_t start;
    vec3_t end;
    vec3_t pos;
    vec3_t dir;
    unsigned int mask;
    int passEntNum;
    gentity_t *passEnt;

    passEnt = 0;
    passEntNum = 0x3ff;
    mask = 0x2802831;
    Scr_GetVector(0, start);
    Scr_GetVector(1, end);
    if (Scr_GetInt(2) == 0) {
        mask = mask & 0xfdffffff;
    }
    if (Scr_GetParamType(3) == 1 && Scr_GetPointer(3) == 0x15) {
        passEnt = Scr_GetEntityParam(3);
        passEntNum = passEnt->s.number;
    }
    G_TracePointEx(&tr, start, end, passEntNum, mask, 0);
    Scr_AddEntity();
    Scr_AddFloatBits(Float_Bits(tr.fraction));
    Scr_PopAndRelease(scr_str_fraction);
    VectorLerp(start, end, tr.fraction, pos);
    Scr_AddVector(pos);
    Scr_PopAndRelease(scr_str_position);
    if (tr.entityNum == 0x3ff || tr.entityNum == 0x3fe) {
        Scr_AddUndefined();
    } else {
        Scr_AddEntityParam(&g_gentities[tr.entityNum]);
    }
    Scr_PopAndRelease(scr_str_entity);
    if (tr.fraction < 1.0f) {
        Scr_AddVector(tr.normal);
        Scr_PopAndRelease(scr_str_normal);
        Scr_AddString(Com_SurfaceTypeToName((int)((unsigned int)tr.surfaceFlags & 0x1f00000) >> 0x14));
        Scr_PopAndRelease(scr_str_surfacetype);
    } else {
        Vec3_Sub_0811870c(end, start, dir);
        Vec3_Normalize(dir);
        Scr_AddVector(dir);
        Scr_PopAndRelease(scr_str_normal);
        Scr_AddStringId(scr_str_none);
        Scr_PopAndRelease(scr_str_surfacetype);
    }
}

// 0x08114a36  size=5  FUN_08114a36  status=translated  naming=observed
// Script builtin precacherumble (function table): empty body.
void GScr_precacheRumble(void)
{
}

// 0x08115034  size=112  FUN_08115034  status=translated  naming=observed
// Script error "<msg> (effect = <name>)" where the effect name is config string 0x34e+index, or "not successfully loaded" for index 0.
void GScr_FxError(const char *msg, int fxIndex)
{
    char name[0x408];

    if (fxIndex == 0) {
        strcpy(name, "not successfully loaded");
    } else {
        SV_GetConfigstring(fxIndex + 0x34e, name, 0x400);
    }
    Scr_Error(va("%s (effect = %s)\n", msg, name));
}

// 0x081150a4  size=561  FUN_081150a4  status=translated  naming=observed
// Script builtin playfx (function table): params fx, origin[, forward[, up]]; spawns temp entity event 0xc2 with the orientation angles in apos.trBase.
void GScr_playFx(void)
{
    int numParams;
    unsigned int fxId;
    gentity_t *event;
    vec3_t origin;
    float axis[3][3];       /* [0] forward, [1] cross product, [2] up */
    float len;

    numParams = Scr_GetNumParams();
    if (numParams < 2 || 4 < numParams) {
        Scr_Error("Incorrect number of parameters");
    }
    fxId = (unsigned int)Scr_GetInt(0);
    Scr_GetVector(1, origin);
    event = (gentity_s*)G_TempEntity(origin, 0xc2);
    event->s.eventParm = fxId & 0xff;
    if (numParams == 2) {
        event->s.apos.trBase[0] = -90.0f;
    } else {
        Scr_GetVector(2, axis[0]);
        len = Vec3_Length_081187f0(axis[0]);
        if (len == 0.0f) {
            GScr_FxError("playFx called with (0 0 0) forward direction", (int)fxId);
        }
        Vec3_Scale_08118750(axis[0], 1.0f / len, axis[0]);
        if (numParams == 3) {
            Vec3_ToAngles(axis[0], event->s.apos.trBase);
        } else {
            Scr_GetVector(3, axis[2]);
            len = Vec3_Length_081187f0(axis[2]);
            if (len == 0.0f) {
                GScr_FxError("playFx called with (0 0 0) up direction", (int)fxId);
            }
            Vec3_Scale_08118750(axis[2], 1.0f / len, axis[2]);
            Vec3_Cross(axis[2], axis[0], axis[1]);
            len = Vec3_Length_081187f0(axis[1]);
            if (len < 0.001f) {
                GScr_FxError("playFx called an up direction 0 or 180 degrees from forward", (int)fxId);
            } else if (len < 0.999f) {
                Vec3_Scale_08118750(axis[1], 1.0f / len, axis[1]);
                Vec3_Cross(axis[0], axis[1], axis[2]);
            }
            AxisToAngles(axis[0], event->s.apos.trBase);
        }
    }
}

// 0x0811571c  size=316  FUN_0811571c  status=translated  naming=observed
// Validates cull-fog parameters (near >= 0, near < far, rgb in [0,1], time >= 0) and sends "%g %g %g %g %g %g %.0f" (time in ms) via G_ParseConfigstring12Bounds.
void GScr_SetCullFogParams(const char *funcName, float nearDist, float farDist, float param4, float red, float green, float blue, float transitionTime)
{
    if (nearDist < 0.0f) {
        Scr_Error(va("%s: near distance must be >= 0", funcName));
    }
    if (farDist <= nearDist) {
        Scr_Error(va("%s: near distance must be less than far distance", funcName));
    }
    if (red < 0.0f || 1.0f < red || green < 0.0f || 1.0f < green || blue < 0.0f || 1.0f < blue) {
        Scr_Error(va("%s: red/green/blue color components must be in the range [0, 1]", funcName));
    }
    if (transitionTime < 0.0f) {
        Scr_Error(va("%s: transition time must be >= 0 seconds", funcName));
    }
    G_ParseConfigstring12Bounds(va("%g %g %g %g %g %g %.0f", (double)nearDist, (double)farDist, (double)param4,
                   (double)red, (double)green, (double)blue, (double)(transitionTime * 1000.0f)));
}

// 0x08115858  size=183  FUN_08115858  status=translated  naming=observed
// Script builtin setcullfog (function table): six float params near, far, r, g, b, time; passes 1.0 as the constant fourth value.
void GScr_setCullFog(void)
{
    float nearDist;
    float farDist;
    float red;
    float green;
    float blue;
    float time;

    if (Scr_GetNumParams() != 6) {
        Scr_Error("USAGE: setCullFog(near distance, far distance, red, green, blue, transition time);\n");
    }
    nearDist = Scr_GetFloat(0);
    farDist = Scr_GetFloat(1);
    red = Scr_GetFloat(2);
    green = Scr_GetFloat(3);
    blue = Scr_GetFloat(4);
    time = Scr_GetFloat(5);
    GScr_SetCullFogParams("setCullFog", nearDist, farDist, 1.0f, red, green, blue, time);
}

// 0x08117906  size=5  FUN_08117906  status=translated  naming=observed
// Script builtin playrumbleonpos (function table): empty body.
void GScr_playRumbleOnPos(void)
{
}

// 0x0811790c  size=5  FUN_0811790c  status=translated  naming=observed
// Script builtin playlooprumbleonpos (function table): empty body.
void GScr_playLoopRumbleOnPos(void)
{
}

// 0x08117912  size=5  FUN_08117912  status=translated  naming=observed
// Script builtin stopallrumbles (function table): empty body.
void GScr_stopAllRumbles(void)
{
}

// 0x0811792c  size=5  FUN_0811792c  status=translated  naming=observed
// Script builtin matchend (function table): empty body.
void GScr_matchend(void)
{
}

// 0x08117932  size=5  FUN_08117932  status=translated  naming=observed
// Script builtin setplayerteamrank (function table): empty body.
void GScr_setPlayerTeamRank(void)
{
}

// 0x08117938  size=5  FUN_08117938  status=translated  naming=observed
// Script builtin sendranks (function table): empty body.
void GScr_sendRanks(void)
{
}

// 0x0811793e  size=5  FUN_0811793e  status=translated  naming=observed
// Script builtin endparty (function table): empty body.
void GScr_endParty(void)
{
}

// 0x08117cb2  size=157  FUN_08117cb2  status=translated  naming=inferred
// Looks up a builtin function by name (case-sensitive strcmp) in the 0x91-entry table at 0x08188040; rewrites *name to the table's canonical string, stores the flag in *developer, returns the handler or NULL.
void *GScr_GetBuiltinFunction(const char **name, int *developer)
{
    const char *want;
    unsigned int i;

    want = *name;
    i = 0;
    for (;;) {
        if (0x90 < i) {
            return 0;
        }
        if (strcmp(want, g_scrBuiltinFunctionTable[i].name) == 0) {
            break;
        }
        i = i + 1;
    }
    *name = g_scrBuiltinFunctionTable[i].name;
    *developer = g_scrBuiltinFunctionTable[i].developer;
    return (void *)g_scrBuiltinFunctionTable[i].handler;
}

// 0x08117d50  size=154  FUN_08117d50  status=translated  naming=inferred
// Same lookup against the 0x3b-entry entity-method table at 0x08188720 (attach, detach, ... releaseclaimedtrigger).
void *Scr_GetEntityMethod(const char **name, int *developer)
{
    const char *want;
    unsigned int i;

    want = *name;
    i = 0;
    for (;;) {
        if (0x3a < i) {
            return 0;
        }
        if (strcmp(want, g_scrEntityMethodTable[i].name) == 0) {
            break;
        }
        i = i + 1;
    }
    *name = g_scrEntityMethodTable[i].name;
    *developer = g_scrEntityMethodTable[i].developer;
    return (void *)g_scrEntityMethodTable[i].handler;
}

// 0x08117dea  size=119  FUN_08117dea  status=translated  naming=inferred
// Method lookup across several method tables in order (Scr_FindPlayerMethod, GScr_FindEntityMethod, entity methods, HudElem_LookupMethod); the first non-null handler wins, but all four are always evaluated (call order preserved).
int Scr_GetMethod(const char **name, int *developer)
{
    int handler;
    int next;

    *developer = 0;
    handler = (int)Scr_FindPlayerMethod(name);
    next = (int)GScr_FindEntityMethod(name);
    if (handler == 0) {
        handler = next;
    }
    next = (int)Scr_GetEntityMethod(name, developer);
    if (handler == 0) {
        handler = next;
    }
    next = (int)HudElem_LookupMethod(name);
    if (handler == 0) {
        handler = next;
    }
    return handler;
}

// 0x08117f30  size=32  FUN_08117f30  status=translated  naming=inferred
// Pushes a vector onto the script stack, or undefined when the pointer is NULL.
void Scr_AddVectorOrUndefined(const float *vec)
{
    if (vec == 0) {
        Scr_AddUndefined();
    } else {
        Scr_AddVector(vec);
    }
}

// 0x08117f50  size=32  FUN_08117f50  status=translated  naming=inferred
// Pushes an entity onto the script stack, or undefined when the pointer is NULL.
void Scr_AddEntityOrUndefined(gentity_t *ent)
{
    if (ent == 0) {
        Scr_AddUndefined();
    } else {
        Scr_AddEntityParam(ent);
    }
}

// 0x08117f70  size=751  FUN_08117f70  status=translated  naming=observed
// Builds the gametype list from maps/mp/gametypes/*.gsc (skipping names starting with '_'): name, description and team flag from <name>.txt (second token "team"). Caps at 0x20 entries.
void GScr_LoadGametypeList(void)
{
    char descBuf[0x40c];
    char fileList[0x1000];
    void *fileBuf;
    const char *parse;
    const char *token;
    cod2_gametype_s *entry;
    char *name;
    int count;
    int numFiles;
    int fileIndex;
    int fileLen;
    size_t nameLen;

    memset(descBuf, 0, 0x400);
    count = 0;
    memset(g_gametypeList, 0, 0x1080);
    numFiles = FS_GetFileList("maps/mp/gametypes", "gsc", 0, fileList, 0x1000);
    name = fileList;
    fileIndex = 0;
    for (;;) {
        if (numFiles <= fileIndex) {
            g_gametypeCount = count;
            return;
        }
        entry = &g_gametypeList[count];
        nameLen = strlen(name);
        if (*name != '_') {
            if (Q_stricmp(name + (nameLen - 4), ".gsc") == 0) {
                name[nameLen - 4] = '\0';
            }
            if (count == 0x20) {
                Com_Printf("Too many game type scripts found! Only loading the first %i\n", 0x1f);
                g_gametypeCount = count;
                return;
            }
            Q_strncpyz(entry->name, name, 0x40);
            Q_strlwr_EndPtr(entry->name);
            fileLen = FSH_FOpenFile(va("maps/mp/gametypes/%s.txt", name), (int32 *)&fileBuf, 0);
            if (fileLen < 1 || 0x3ff < fileLen) {
                if (fileLen < 1) {
                    Com_Printf("WARNING: Could not load GameType description file %s for gametype %s\n",
                               va("maps/mp/gametypes/%s.txt", name), name);
                } else {
                    Com_Printf("WARNING: GameType description file %s is too big to load.\n",
                               va("maps/mp/gametypes/%s.txt", name));
                }
                Q_strncpyz(entry->description, entry->name, 0x40);
                entry->teamBased = 0;
            } else {
                FS_Read(descBuf, fileLen, (int)fileBuf);
                parse = descBuf;
                token = Com_Parse(&parse);
                Q_strncpyz(entry->description, token, 0x40);
                token = Com_Parse(&parse);
                if (token == 0 || Q_stricmp(token, "team") != 0) {
                    entry->teamBased = 0;
                } else {
                    entry->teamBased = 1;
                }
            }
            count = count + 1;
            if (0 < fileLen) {
                FS_FCloseFile((int)fileBuf);
            }
        }
        name = name + nameLen + 1;
        fileIndex = fileIndex + 1;
    }
}
