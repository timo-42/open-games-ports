#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_051_types.h"

/*
 * Chunk 051: 0x08118260..0x0811bf64
 * Script callbacks, vector helpers, script-mover trajectory planning, spawn-variable parsing,
 * entity field tables, IP filter and server console commands.
 * Float arithmetic is x87 in the original (extended precision between operations); this
 * translation uses float/double C++ arithmetic, which may differ in the last ulp.
 */

static inline uint32 FloatBits(float f)
{
    union { float f; uint32 u; } v;
    v.f = f;
    return v.u;
}


/* ---------------------------------------------------------------- 0x08118260 */
// 0x08118260  size=105  FUN_08118260  status=translated  naming=unknown
// Linear search of the 0x84-stride table at 0x0884d72c by Q_stricmp on the entry name; returns the +0x40 payload.
byte *G_FindGametypeDescriptionByName(const char *name)
{
    int i;
    i = 0;
    while (true) {
        if (g_gametypeCount <= i) {
            return 0;
        }
        if (Q_stricmp(g_gametypeList[i].name, name) == 0) {
            break;
        }
        i = i + 1;
    }
    return (byte *)g_gametypeList[i].description; /* payload at +0x40 */
}

// 0x081182ca  size=42  FUN_081182ca  status=translated  naming=unknown
// Boolean wrapper of G_FindGametypeDescriptionByName.
bool G_IsGametypeListed(const char *name)
{
    byte *entry = G_FindGametypeDescriptionByName(name);
    return entry != 0;
}

// 0x081182f4  size=45  FUN_081182f4  status=translated  naming=unknown
// Calls the script function whose code offset is stored at 0x0884d708 (no args) and releases the returned thread handle; caller 0x08109096 also calls the 0x0884d70c variant.
void G_CallGametypeMain(void)
{
    uint16 handle = Scr_CallScriptOffset(g_gametypeMainScriptOffset, 0);
    Scr_ReleaseHandle(handle);
}

// 0x08118322  size=45  FUN_08118322  status=translated  naming=unknown
// Same as G_CallGametypeMain for the offset at 0x0884d70c.
void G_CallStartGameType(void)
{
    uint16 handle = Scr_CallScriptOffset(g_0884d70c, 0);
    Scr_ReleaseHandle(handle);
}

// 0x08118350  size=52  FUN_08118350  status=translated  naming=unknown
// Calls the entity-method script at offset 0x0884d710 with no args on ent (sibling of the PlayerDamage/Killed callbacks) and releases the handle.
void G_CallPlayerConnect(gentity_t *ent)
{
    uint16 handle = Scr_CallEntityMethod(ent, g_0884d710, 0);
    Scr_ReleaseHandle(handle);
}

// 0x08118384  size=52  FUN_08118384  status=translated  naming=unknown
// Same as G_CallPlayerConnect for the offset at 0x0884d714.
void G_CallPlayerDisconnect(gentity_t *ent)
{
    uint16 handle = Scr_CallEntityMethod(ent, g_0884d714, 0);
    Scr_ReleaseHandle(handle);
}

// 0x081183b8  size=218  FUN_081183b8  status=translated  naming=inferred
// Pushes the 10 arguments of the Callback_PlayerDamage-style script callback (inflictor, attacker, damage, dflags, MOD string, weapon name, point, dir, hit location, time offset) in reverse and calls offset 0x0884d718; "badMOD" string observed.
void G_CallPlayerDamageCallback(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int dflags,
                                uint32 meansOfDeath, int weapon, const float *point, const float *dir,
                                int hitLocation, int timeOffset)
{
    Scr_AddIndex(timeOffset);
    uint16 hitLocId = G_GetHitLocId(hitLocation);
    Scr_AddStringId(hitLocId);
    Scr_AddVectorOrUndefined(dir);
    Scr_AddVectorOrUndefined(point);
    weaponDef_t *weaponDef = BG_GetWeaponDef(weapon);
    Scr_AddString(weaponDef->szInternalName);
    if ((int)meansOfDeath < 0 || 0xe < meansOfDeath) {
        Scr_AddString("badMOD");
    } else {
        Scr_AddString(g_modNames[meansOfDeath]);
    }
    Scr_AddIndex(dflags);
    Scr_AddIndex(damage);
    Scr_AddEntityOrUndefined(attacker);
    Scr_AddEntityOrUndefined(inflictor);
    uint16 handle = Scr_CallEntityMethod(self, g_0884d718, 10);
    Scr_ReleaseHandle(handle);
}

// 0x08118492  size=207  FUN_08118492  status=translated  naming=inferred
// Pushes the 9 arguments of the Callback_PlayerKilled-style script callback (inflictor, attacker, damage, MOD string, weapon name, dir, hit location, time offset, death anim duration) and calls offset 0x0884d71c.
void G_CallPlayerKilledCallback(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage,
                                uint32 meansOfDeath, int weapon, const float *dir, int hitLocation,
                                int timeOffset, int deathAnimDuration)
{
    Scr_AddIndex(deathAnimDuration);
    Scr_AddIndex(timeOffset);
    uint16 hitLocId = G_GetHitLocId(hitLocation);
    Scr_AddStringId(hitLocId);
    Scr_AddVectorOrUndefined(dir);
    weaponDef_t *weaponDef = BG_GetWeaponDef(weapon);
    Scr_AddString(weaponDef->szInternalName);
    if ((int)meansOfDeath < 0 || 0xe < meansOfDeath) {
        Scr_AddString("badMOD");
    } else {
        Scr_AddString(g_modNames[meansOfDeath]);
    }
    Scr_AddIndex(damage);
    Scr_AddEntityOrUndefined(attacker);
    Scr_AddEntityOrUndefined(inflictor);
    uint16 handle = Scr_CallEntityMethod(self, g_0884d71c, 9);
    Scr_ReleaseHandle(handle);
}

// 0x08118562  size=71  FUN_08118562  status=translated  naming=unknown
// Pushes three strings and runs the notify with string id 0x0885329e on ent (argc 3).
void Scr_NotifyEntityCallVote(gentity_t *ent, const char *s1, const char *s2, const char *s3)
{
    Scr_AddString(s3);
    Scr_AddString(s2);
    Scr_AddString(s1);
    Scr_NotifyEntity(ent, scrStr_call_vote, 3);
}

// 0x081185aa  size=49  FUN_081185aa  status=translated  naming=unknown
// Pushes one string and notifies ent with string id 0x088532a0 (argc 1).
void Scr_NotifyEntityVote(gentity_t *ent, const char *s1)
{
    Scr_AddString(s1);
    Scr_NotifyEntity(ent, scrStr_vote, 1);
}

// 0x081185dc  size=10  FUN_081185dc  status=translated  naming=unknown
// Returns 0 (unreferenced stub in the original).
int Stub_ReturnZero_081185dc(void)
{
    return 0;
}

// 0x081185e6  size=54  FUN_081185e6  status=translated  naming=inferred
// (int)floor(x + 0.5), truncating cast (x87 control word forced to round-to-zero: fldcw with 0xc00). Name from behaviour.
int Float_RoundHalfUp(float x)
{
    double r = floor((double)(x + 0.5f));
    return (int)r;
}

// 0x0811861c  size=45  FUN_0811861c  status=translated  naming=inferred
// sin/cos of one angle into two floats (double libm calls, narrowed to float).
void Float_SinCos(float angle, float *outSin, float *outCos)
{
    double d;
    d = sin((double)angle);
    *outSin = (float)d;
    d = cos((double)angle);
    *outCos = (float)d;
}

// 0x0811864a  size=41  FUN_0811864a  status=translated  naming=inferred
// Zero a vec3 (VectorClear counterpart).
void Vec3_Clear_0811864a(float *v)
{
    v[0] = 0; v[1] = 0; v[2] = 0;
}

// 0x08118674  size=35  FUN_08118674  status=translated  naming=inferred
// Set a vec3 from three components (VectorSet counterpart); components copied as raw dwords.
void Vec3_Set_08118674(uint32 *v, uint32 x, uint32 y, uint32 z)
{
    v[0] = x; v[1] = y; v[2] = z;
}

// 0x08118698  size=47  FUN_08118698  status=translated  naming=inferred
// Copy vec3 src -> dst (VectorCopy counterpart).
void Vec3_Copy_08118698(const uint32 *src, uint32 *dst)
{
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
}

// 0x081186c8  size=68  FUN_081186c8  status=translated  naming=inferred
// out = a + b (VectorAdd counterpart).
void Vec3_Add_081186c8(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x0811870c  size=68  FUN_0811870c  status=translated  naming=inferred
// out = a - b (VectorSubtract counterpart).
void Vec3_Sub_0811870c(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x08118750  size=56  FUN_08118750  status=translated  naming=inferred
// out = a * scale (VectorScale counterpart).
void Vec3_Scale_08118750(const float *a, float scale, float *out)
{
    out[0] = scale * a[0];
    out[1] = scale * a[1];
    out[2] = scale * a[2];
}

// 0x08118788  size=51  FUN_08118788  status=translated  naming=inferred
// Dot product (DotProduct counterpart). The original leaves the x87 sum unrounded in st0; float return here.
float Vec3_Dot_08118788(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// 0x081187bc  size=51  FUN_081187bc  status=translated  naming=inferred
// Squared length. The original returns the unrounded x87 sum; float return here.
float Vec3_LengthSquared_081187bc(const float *v)
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

// 0x081187f0  size=62  FUN_081187f0  status=translated  naming=inferred
// Vector length via Float_Sqrt_0811882e of the float-precision squared length (result discarded by the original ABI use: returns st0 of Float_Sqrt_0811882e).
float Vec3_Length_081187f0(const float *v)
{
    return Float_Sqrt_0811882e(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]);
}

// 0x0811882e  size=31  FUN_0811882e  status=translated  naming=inferred
// (float)sqrt((double)x), spilled through a float stack slot.
float Float_Sqrt_0811882e(float x)
{
    double d = sqrt((double)x);
    return (float)d;
}

/* ---------------------------------------------------------------- script mover */

// 0x08118850  size=480  FUN_08118850  status=translated  naming=inferred
// Advances a scripted-mover trajectory to its next phase: accelerate(7) -> linear-stop(3) -> decelerate(8) -> stationary(0).
// Returns 1 when the mover has finished (trajectory set to stationary at dest), else 0. Phase meaning inferred from trType transitions.
int G_ScriptMover_NextPhase(trajectory_t *tr, void *unusedCurrent, float speed, float cruiseTime, float decelTime,
                            const float *accelEnd, const float *cruiseEnd, const float *dest)
{
    float dir[7];
    float invDuration;
    int cruiseMs;

    cruiseMs = (int)(cruiseTime * 1000.0f);
    if (tr->trType == 7 && 0 < cruiseMs) {
        tr->trTime = g_0864c56c;
        tr->trDuration = cruiseMs;
        G_VecCopy(accelEnd, tr->trBase);
        G_VecSub(cruiseEnd, accelEnd, dir);
        invDuration = 1000.0f / (float)cruiseMs;
        G_VecScale(dir, invDuration, tr->trDelta);
        tr->trType = 3;
    } else {
        if ((((tr->trType != 7 || 0 < cruiseMs) && tr->trType != 3)) || decelTime <= 0.0f) {
            if (tr->trType == 5) {
                BG_EvaluateTrajectory((tr), g_0864c56c, tr->trBase);
            } else {
                G_VecCopy(dest, tr->trBase);
            }
            tr->trTime = g_0864c56c;
            tr->trType = 0;
            return 1;
        }
        tr->trTime = g_0864c56c;
        tr->trDuration = (int)(decelTime * 1000.0f);
        G_VecCopy(cruiseEnd, tr->trBase);
        G_VecSub(dest, cruiseEnd, dir);
        Vec3_Normalize(dir);
        G_VecScale(dir, speed, dir);
        G_VecCopy(dir, tr->trDelta);
        tr->trType = 8;
    }
    return 0;
}

// 0x08118a30  size=537  FUN_08118a30  status=translated  naming=inferred
// Per-frame update of a script mover: when the position (then the angle) trajectory phase has expired, advance it, re-evaluate
// currentOrigin/currentAngles, relink, and on completion notify the entity (ids 0x08853248 / 0x08853260).
void G_ScriptMover_Think(gentity_t *ent)
{
    int finished;

    if (ent->s.pos.trType != 0 && ent->s.pos.trTime + ent->s.pos.trDuration <= g_0864c56c) {
        finished = G_ScriptMover_NextPhase(&ent->s.pos, ent->r.currentOrigin, ent->u.mover.moveSpeed, ent->u.mover.moveCruiseTime,
                                           ent->u.mover.moveDecelTime, ent->u.mover.moveAccelEnd, ent->u.mover.moveCruiseEnd, ent->u.mover.moveDest);
        BG_EvaluateTrajectory((&ent->s.pos), g_0864c56c, ent->r.currentOrigin);
        SV_LinkEntity(ent);
        if (finished != 0) {
            Scr_NotifyEntity(ent, scrStr_movedone, 0);
        }
    }
    if (ent->s.apos.trType != 0 && ent->s.apos.trTime + ent->s.apos.trDuration <= g_0864c56c) {
        finished = G_ScriptMover_NextPhase(&ent->s.apos, ent->r.currentAngles, ent->u.mover.rotSpeed, ent->u.mover.rotCruiseTime,
                                           ent->u.mover.rotDecelTime, ent->u.mover.rotAccelEnd, ent->u.mover.rotCruiseEnd, ent->u.mover.rotDest);
        BG_EvaluateTrajectory((&ent->s.apos), g_0864c56c, ent->r.currentAngles);
        SV_LinkEntity(ent);
        if (finished != 0) {
            float a;
            a = AngleNormalize180(ent->r.currentAngles[0]);
            ent->r.currentAngles[0] = a;
            a = AngleMod(ent->r.currentAngles[1]);
            ent->r.currentAngles[1] = a;
            a = AngleNormalize180(ent->r.currentAngles[2]);
            ent->r.currentAngles[2] = a;
            Scr_NotifyEntity(ent, scrStr_rotatedone, 0);
        }
    }
}

// 0x08118c4a  size=948  FUN_08118c4a  status=translated  naming=inferred
// Plans a move of a script mover to dest over totalTime with optional accel/decel times (moveto): fills the trajectory for the
// first phase and records speed, cruise time, decel time and the phase end positions. Names inferred from field use.
void G_ScriptMover_PlanMove(trajectory_t *tr, const float *dest, float totalTime, float accelTime, float decelTime,
                            float *curPos, float *outSpeed, float *outCruiseTime, float *outDecelTime,
                            float *outAccelEnd, float *outCruiseEnd, float *outDest)
{
    float dir[3];
    float unitDir[4];
    float delta[6];
    float invDuration;
    float len;

    G_VecSub(dest, curPos, delta);
    if (tr->trType != 0) {
        BG_EvaluateTrajectory((tr), g_0864c56c, curPos);
    }
    if (accelTime == 0.0f && decelTime == 0.0f) {
        tr->trTime = g_0864c56c;
        tr->trDuration = (int)(totalTime * 1000.0f);
        *(volatile uint32 *)outCruiseTime = *(volatile const uint32 *)&totalTime; /* integer word move in the binary (no x87 quieting of NaN inputs) */
        *outDecelTime = 0.0f;
        G_VecCopy(dest, outDest);
        G_VecCopy(curPos, tr->trBase);
        invDuration = 1000.0f / (float)tr->trDuration;
        G_VecScale(delta, invDuration, tr->trDelta);
        tr->trType = 3;
        BG_EvaluateTrajectory((tr), g_0864c56c, curPos);
    } else {
        *outCruiseTime = (totalTime - accelTime) - decelTime;
        *(volatile uint32 *)outDecelTime = *(volatile const uint32 *)&decelTime; /* volatile keeps the integer move (GCC would turn it into fld/fstp) */
        len = G_VecLength(delta);
        *outSpeed = (len + len) / (((totalTime + totalTime) - accelTime) - decelTime);
        Vec3_NormalizeCopy(delta, unitDir);
        G_VecScale(unitDir, *outSpeed, unitDir);
        if (accelTime != 0.0f) {
            tr->trTime = g_0864c56c;
            tr->trDuration = (int)(accelTime * 1000.0f);
            G_VecCopy(curPos, tr->trBase);
            G_VecCopy(unitDir, tr->trDelta);
            tr->trType = 7;
            BG_EvaluateTrajectory((tr), tr->trDuration + g_0864c56c, outAccelEnd);
        } else {
            G_VecCopy(curPos, outAccelEnd);
            if (*outCruiseTime != 0.0f) {
                tr->trTime = g_0864c56c;
                tr->trDuration = (int)(*outCruiseTime * 1000.0f);
                G_VecCopy(curPos, tr->trBase);
                G_VecScale(unitDir, *outCruiseTime, delta);
                invDuration = 1000.0f / (float)tr->trDuration;
                G_VecScale(delta, invDuration, tr->trDelta);
                tr->trType = 3;
            } else {
                tr->trTime = g_0864c56c;
                tr->trDuration = (int)(*outDecelTime * 1000.0f);
                G_VecCopy(curPos, tr->trBase);
                G_VecCopy(unitDir, tr->trDelta);
                tr->trType = 8;
            }
        }
        G_VecMA(outAccelEnd, *outCruiseTime, unitDir, outCruiseEnd);
        G_VecCopy(dest, outDest);
        BG_EvaluateTrajectory((tr), g_0864c56c, curPos);
    }
}

// 0x08118ffe  size=937  FUN_08118ffe  status=translated  naming=inferred
// Rotation counterpart of G_ScriptMover_PlanMove: the "dest" vector is the angular velocity (delta) itself, so no speed
// normalisation is done; outSpeed receives its length. Used by the apos path of the rotate script methods.
void G_ScriptMover_PlanRotate(trajectory_t *tr, const float *angVel, float totalTime, float accelTime, float decelTime,
                              float *curAngles, float *outSpeed, float *outCruiseTime, float *outDecelTime,
                              float *outAccelEnd, float *outCruiseEnd, float *outDest)
{
    trajectory_t decel;
    int decelMs;
    float len;

    if (tr->trType != 0) {
        BG_EvaluateTrajectory((tr), g_0864c56c, curAngles);
    }
    if (accelTime == 0.0f && decelTime == 0.0f) {
        tr->trTime = g_0864c56c;
        tr->trDuration = (int)(totalTime * 1000.0f);
        *(volatile uint32 *)outCruiseTime = *(volatile const uint32 *)&totalTime; /* integer word move in the binary (no x87 quieting of NaN inputs) */
        *outDecelTime = 0.0f;
        G_VecCopy(curAngles, tr->trBase);
        G_VecCopy(angVel, tr->trDelta);
        tr->trType = 3;
        BG_EvaluateTrajectory((tr), g_0864c56c, curAngles);
        BG_EvaluateTrajectory((tr), tr->trDuration + g_0864c56c, outDest);
    } else {
        *outCruiseTime = (totalTime - accelTime) - decelTime;
        *(volatile uint32 *)outDecelTime = *(volatile const uint32 *)&decelTime; /* volatile keeps the integer move (GCC would turn it into fld/fstp) */
        len = G_VecLength(angVel);
        *outSpeed = len;
        if (accelTime != 0.0f) {
            tr->trTime = g_0864c56c;
            tr->trDuration = (int)(accelTime * 1000.0f);
            G_VecCopy(curAngles, tr->trBase);
            G_VecCopy(angVel, tr->trDelta);
            tr->trType = 7;
            BG_EvaluateTrajectory((tr), tr->trDuration + g_0864c56c, outAccelEnd);
        } else {
            G_VecCopy(curAngles, outAccelEnd);
            if (*outCruiseTime != 0.0f) {
                tr->trTime = g_0864c56c;
                tr->trDuration = (int)(*outCruiseTime * 1000.0f);
                G_VecCopy(curAngles, tr->trBase);
                G_VecCopy(angVel, tr->trDelta);
                tr->trType = 3;
            } else {
                tr->trTime = g_0864c56c;
                tr->trDuration = (int)(*outDecelTime * 1000.0f);
                G_VecCopy(curAngles, tr->trBase);
                G_VecCopy(angVel, tr->trDelta);
                tr->trType = 8;
            }
        }
        G_VecMA(outAccelEnd, *outCruiseTime, angVel, outCruiseEnd);
        if (*outDecelTime != 0.0f) {
            decel.trType = 8;
            decel.trTime = g_0864c56c;
            decelMs = (int)(*outDecelTime * 1000.0f);
            decel.trDuration = decelMs;
            G_VecCopy(outCruiseEnd, decel.trBase);
            G_VecCopy(angVel, decel.trDelta);
            BG_EvaluateTrajectory((&decel), decelMs + g_0864c56c, outDest);
        } else {
            G_VecCopy(outCruiseEnd, outDest);
        }
        BG_EvaluateTrajectory((tr), g_0864c56c, curAngles);
    }
}

// 0x081193a8  size=145  FUN_081193a8  status=translated  naming=inferred
// Starts a position move on the entity (pos trajectory, origin at +0x138) and relinks it.
void G_ScriptMover_StartMove(gentity_t *ent, const float *dest, float totalTime, float accelTime, float decelTime)
{
    G_ScriptMover_PlanMove(&ent->s.pos, dest, totalTime, accelTime, decelTime, ent->r.currentOrigin, &ent->u.mover.moveSpeed,
                           &ent->u.mover.moveCruiseTime, &ent->u.mover.moveDecelTime, ent->u.mover.moveAccelEnd, ent->u.mover.moveCruiseEnd,
                           ent->u.mover.moveDest);
    SV_LinkEntity(ent);
}

// 0x0811943a  size=165  FUN_0811943a  status=translated  naming=inferred
// Starts a gravity (trType 5) position trajectory with the given initial velocity and duration in seconds (movegravity).
void G_ScriptMover_StartGravity(gentity_t *ent, const float *velocity, float seconds)
{
    ent->s.pos.trTime = g_0864c56c;
    ent->s.pos.trDuration = (int)(seconds * 1000.0f);
    G_VecCopy(ent->r.currentOrigin, ent->s.pos.trBase);
    G_VecCopy(velocity, ent->s.pos.trDelta);
    ent->s.pos.trType = 5;
    BG_EvaluateTrajectory((&ent->s.pos), g_0864c56c, ent->r.currentOrigin);
    SV_LinkEntity(ent);
}

// 0x081194e0  size=145  FUN_081194e0  status=translated  naming=inferred
// Angle counterpart of G_ScriptMover_StartMove (apos trajectory, angles at +0x144) using the move planner (rotateto).
void G_ScriptMover_StartRotateTo(gentity_t *ent, const float *destAngles, float totalTime, float accelTime, float decelTime)
{
    G_ScriptMover_PlanMove(&ent->s.apos, destAngles, totalTime, accelTime, decelTime, ent->r.currentAngles, &ent->u.mover.rotSpeed,
                           &ent->u.mover.rotCruiseTime, &ent->u.mover.rotDecelTime, ent->u.mover.rotAccelEnd, ent->u.mover.rotCruiseEnd,
                           ent->u.mover.rotDest);
    SV_LinkEntity(ent);
}

// 0x08119572  size=145  FUN_08119572  status=translated  naming=inferred
// Angle trajectory started with the velocity-style rotate planner (G_ScriptMover_PlanRotate); no direct caller in this chunk.
void G_ScriptMover_StartRotateVelocity(gentity_t *ent, const float *angVel, float totalTime, float accelTime, float decelTime)
{
    G_ScriptMover_PlanRotate(&ent->s.apos, angVel, totalTime, accelTime, decelTime, ent->r.currentAngles, &ent->u.mover.rotSpeed,
                             &ent->u.mover.rotCruiseTime, &ent->u.mover.rotDecelTime, ent->u.mover.rotAccelEnd, ent->u.mover.rotCruiseEnd,
                             ent->u.mover.rotDest);
    SV_LinkEntity(ent);
}

/* ---------------------------------------------------------------- spawn helpers (forward) */

// 0x08119604  size=418  FUN_08119604  status=translated  naming=inferred
// Common setup for script_brushmodel / script_model / script_origin spawns: while parsing spawn vars it reads "light"
// (default "100") and "color" (default "1 1 1") into packedLight; then sets eType 6 (ET_SCRIPTMOVER), clears svFlags,
// copies origin/angles into the trajectory bases with trType 0 and sets flag 0x1000.
void SP_script_mover_common(gentity_t *ent)
{
    int hasLight;
    int hasColor;
    int r, g, b, a;
    float color[3];
    float light[3];

    if (g_spawnVars.parsing != 0) {
        hasLight = G_SpawnFloat("light", "100", light);
        hasColor = G_SpawnVector("color", "1 1 1", color);
        if (hasLight != 0 || hasColor != 0) {
            r = (int)(color[0] * 255.0f);
            if (0xff < r) {
                r = 0xff;
            }
            g = (int)(color[1] * 255.0f);
            if (0xff < g) {
                g = 0xff;
            }
            b = (int)(color[2] * 255.0f);
            if (0xff < b) {
                b = 0xff;
            }
            a = (int)(light[0] / 4.0f);
            if (0xff < a) {
                a = 0xff;
            }
            ent->s.constantLight = (uint32)(a << 24 | g << 8 | r | b << 16);
        }
    }
    ent->handler = 5;
    ent->r.svFlags = 0;
    ent->s.eType = 6;
    G_VecCopy(ent->r.currentOrigin, ent->s.pos.trBase);
    ent->s.pos.trType = 0;
    G_VecCopy(ent->r.currentAngles, ent->s.apos.trBase);
    ent->s.apos.trType = 0;
    ent->flags = ent->flags | 0x1000;
}

// 0x081197a6  size=54  FUN_081197a6  status=translated  naming=observed
// Spawn function for classname "script_brushmodel" (spawn table 0x08188a00 entry): brush model setup (SV_SetBrushModel), common
// mover setup, contents = 1, relink.
void SP_script_brushmodel(gentity_t *ent)
{
    SV_SetBrushModel(ent);
    SP_script_mover_common(ent);
    ent->r.contents = 1;
    SV_LinkEntity(ent);
}

// 0x081197dc  size=75  FUN_081197dc  status=translated  naming=observed
// Spawn function for classname "script_model" (spawn table entry): model setup (Ent_RefreshAttachments), common setup, svFlags |= 4,
// contents = 0x2080, relink.
void SP_script_model(gentity_t *ent)
{
    Ent_RefreshAttachments(ent);
    SP_script_mover_common(ent);
    ent->r.svFlags = ent->r.svFlags | 4;
    ent->r.contents = 0x2080;
    SV_LinkEntity(ent);
}

// 0x08119886  size=259  FUN_08119886  status=translated  naming=inferred
// Reads the mover time parameters from script params 1..3 (total, optional accel, optional decel) with validation errors
// (strings observed: "total time must be positive", "accel time must be nonnegative", ...).
void Scr_GetMoverTimeParams(float *total, float *accel, float *decel)
{
    int numParams;

    *total = Scr_GetFloat(1);
    if (*total <= 0.0f) {
        Scr_ErrorAtParam(1, "total time must be positive");
    }
    numParams = Scr_GetNumParams();
    if (numParams < 3) {
        *accel = 0.0f;
        *decel = 0.0f;
    } else {
        *accel = Scr_GetFloat(2);
        if (*accel < 0.0f) {
            Scr_ErrorAtParam(2, "accel time must be nonnegative");
        }
        if (numParams < 4) {
            *decel = 0.0f;
        } else {
            *decel = Scr_GetFloat(3);
            if (*decel < 0.0f) {
                Scr_ErrorAtParam(3, "decel time must be nonnegative");
            }
        }
    }
    if (*total < *accel + *decel) {
        Scr_Error("accel time plus decel time is greater than total time");
    }
}

// 0x08119b36  size=262  FUN_08119b36  status=translated  naming=inferred
// Script method body for movex/movey/movez (axis 0/1/2): resolves self to a script mover entity (error strings observed),
// adds the float distance param 0 to currentOrigin[axis] and starts the move. If the entity check fails the original keeps going
// with ent == NULL (Scr_ErrorNoParam does not return in practice).
void GScr_MoveAxis(uint32 self, int axis)
{
    gentity_t *ent;
    float dest[4];
    float totalTime, accelTime, decelTime;
    float distance;

    if ((self >> 16) == 0) {
        ent = &g_gentities[self & 0xffff];
        if (ent->classname != scrStr_script_brushmodel && ent->classname != scr_str_script_model && ent->classname != scr_str_script_origin) {
            char *msg = va("entity %i is not a script_brushmodel, script_model, or script_origin", (int)(uint16)self);
            Scr_ErrorNoParam(msg);
        }
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = 0;
    }
    distance = Scr_GetFloat(0);
    Scr_GetMoverTimeParams(&totalTime, &accelTime, &decelTime);
    G_VecCopy(ent->r.currentOrigin, dest);
    dest[axis] = dest[axis] + distance;
    G_ScriptMover_StartMove(ent, dest, totalTime, accelTime, decelTime);
}

// 0x08119c3c  size=27  FUN_08119c3c  status=translated  naming=observed
// Script entity method "movex" (method table 0x0815e3a0 entry points here).
void GScr_MoveX(uint32 self)
{
    GScr_MoveAxis(self, 0);
}

// 0x08119dc8  size=262  FUN_08119dc8  status=translated  naming=inferred
// Same as GScr_MoveAxis but adds to currentAngles[axis] and starts a rotate-to.
void GScr_RotateAxis(uint32 self, int axis)
{
    gentity_t *ent;
    float dest[4];
    float totalTime, accelTime, decelTime;
    float amount;

    if ((self >> 16) == 0) {
        ent = &g_gentities[self & 0xffff];
        if (ent->classname != scrStr_script_brushmodel && ent->classname != scr_str_script_model && ent->classname != scr_str_script_origin) {
            char *msg = va("entity %i is not a script_brushmodel, script_model, or script_origin", (int)(uint16)self);
            Scr_ErrorNoParam(msg);
        }
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = 0;
    }
    amount = Scr_GetFloat(0);
    Scr_GetMoverTimeParams(&totalTime, &accelTime, &decelTime);
    G_VecCopy(ent->r.currentAngles, dest);
    dest[axis] = dest[axis] + amount;
    G_ScriptMover_StartRotateTo(ent, dest, totalTime, accelTime, decelTime);
}

// 0x08119ece  size=27  FUN_08119ece  status=translated  naming=observed
// Script entity method "rotatepitch" (method table entry).
void GScr_RotatePitch(uint32 self)
{
    GScr_RotateAxis(self, 0);
}

// 0x0811a1fa  size=131  FUN_0811a1fa  status=translated  naming=inferred
// Looks up a script entity method by name in the 12-entry table at 0x0815e3a0 (strcmp); on a hit rewrites *name to the
// table's canonical string pointer and returns the handler, else NULL. Called from 0x08117dea.
void *GScr_FindEntityMethod(const char **name)
{
    const char *key = *name;
    uint32 i = 0;
    while (true) {
        if (0xb < i) {
            return 0;
        }
        if (strcmp(key, g_scriptMoverMethodTable[i].name) == 0) {
            break;
        }
        i = i + 1;
    }
    *name = g_scriptMoverMethodTable[i].name;
    return g_scriptMoverMethodTable[i].handler;
}

// 0x0811a27e  size=47  FUN_0811a27e  status=translated  naming=inferred
// vec3 copy (second translation unit's VectorCopy).
void G_VecCopy(const float *src, float *dst)
{
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
    ((uint32 *)dst)[2] = ((const uint32 *)src)[2];
}

// 0x0811a2ae  size=68  FUN_0811a2ae  status=translated  naming=inferred
// out = a - b.
void G_VecSub(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x0811a2f2  size=56  FUN_0811a2f2  status=translated  naming=inferred
// out = scale * a.
void G_VecScale(const float *a, float scale, float *out)
{
    out[0] = scale * a[0];
    out[1] = scale * a[1];
    out[2] = scale * a[2];
}

// 0x0811a32a  size=83  FUN_0811a32a  status=translated  naming=inferred
// out = a + scale * dir (VectorMA counterpart).
void G_VecMA(const float *a, float scale, const float *dir, float *out)
{
    out[0] = a[0] + scale * dir[0];
    out[1] = a[1] + scale * dir[1];
    out[2] = a[2] + scale * dir[2];
}


// 0x0811a37e  size=62  FUN_0811a37e  status=translated  naming=inferred
// Vector length (VectorLength counterpart). Original leaves the result in st0 although declared void by the decompiler.
float G_VecLength(const float *v)
{
    return G_Sqrt(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]);
}

// 0x0811a3bc  size=31  FUN_0811a3bc  status=translated  naming=inferred
// (float)sqrt((double)x).
float G_Sqrt(float x)
{
    double d = sqrt((double)x);
    return (float)d;
}

// 0x0811a3dc  size=41  FUN_0811a3dc  status=translated  naming=inferred
// G_SpawnString counterpart: fetches spawn variable key (or def) from the spawn-var parser state at 0x0864d6c8 into *out.
int G_SpawnString(const char *key, const char *def, const char **out)
{
    return KeyValue_Lookup((const kv_table_s *)&g_spawnVars, key, def, out); /* eax of the callee is returned */
}

// 0x0811a406  size=60  FUN_0811a406  status=translated  naming=inferred
// G_SpawnFloat counterpart: spawn string -> atof -> float; returns the G_SpawnString result.
int G_SpawnFloat(const char *key, const char *def, float *out)
{
    const char *s;
    int found = G_SpawnString(key, def, &s);
    double d = atof(s);
    *out = (float)d;
    return found;
}

// 0x0811a442  size=57  FUN_0811a442  status=translated  naming=inferred
// G_SpawnInt counterpart: spawn string -> atoi.
int G_SpawnInt(const char *key, const char *def, int *out)
{
    const char *s;
    int found = G_SpawnString(key, def, &s);
    *out = atoi(s);
    return found;
}


// 0x0811a47c  size=96  FUN_0811a47c  status=translated  naming=inferred
// G_SpawnVector counterpart: zero the vec3, then sscanf "%f %f %f".
int G_SpawnVector(const char *key, const char *def, float *out)
{
    const char *s;
    int found = G_SpawnString(key, def, &s);
    G_VecClear(out);
    sscanf(s, "%f %f %f", &out[0], &out[1], &out[2]);
    return found;
}

// 0x0811a4dc  size=20  FUN_0811a4dc  status=translated  naming=observed
// Field setter used for read-only entity fields (classname, model, spawnflags): raises the script error.
void Scr_ReadOnlyEntityField(void)
{
    Scr_Error("Tried to set a read only entity field");
}

// 0x0811a4f0  size=231  FUN_0811a4f0  status=translated  naming=inferred
// Looks up a script field by name; if it exists, pushes the value text parsed according to the field's script type
// (4 vec3 via sscanf, 2 string, 5 float, 6 int; other types push nothing) and returns the field id, else 0.
int G_ParseScrFieldValue(const char *key, const char *value)
{
    int fieldType;
    int field;
    int result;
    float vec[3];

    field = Scr_FindFieldDef(key, &fieldType);
    if (field == 0) {
        result = 0;
    } else {
        if (fieldType == 4) {
            G_VecClear(vec);
            sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
            Scr_AddVector(vec);
        } else if (fieldType < 5) {
            if (fieldType == 2) {
                Scr_AddString(value);
            }
        } else if (fieldType == 5) {
            double d = atof(value);
            Scr_AddFloatBits(FloatBits((float)d));
        } else if (fieldType == 6) {
            int n = atoi(value);
            Scr_AddIndex(n);
        }
        result = field;
    }
    return result;
}


// 0x0811a5d8  size=55  FUN_0811a5d8  status=translated  naming=inferred
// Sets a script-defined field on ent from a spawn key/value string pair.
void G_SetScrFieldFromString(const char *key, const char *value, gentity_t *ent)
{
    int field = G_ParseScrFieldValue(key, value);
    if (field != 0) {
        Scr_SetObjectFieldValue(ent, field);
    }
}

// 0x0811a610  size=416  FUN_0811a610  status=translated  naming=inferred
// G_ParseField counterpart: applies a spawn key/value to ent via the field table at 0x0815e820 (Q_stricmp on name): types 0
// int, 1 float, 3 script string (release then Print_UnescapeAndEmit), 4 vec3, 8 model ("*N" -> modelIndex, else Ent_SetModelByName). Unknown keys fall
// back to the script-defined fields. Other table types are ignored.
void G_ParseField(const char *key, const char *value, gentity_t *ent)
{
    entityFieldDef_t *field;
    byte *base = (byte *)ent;
    float v[3];

    field = g_entityScriptFieldTable;
    while (true) {
        if (field->name == 0) {
            G_SetScrFieldFromString(key, value, ent);
            return;
        }
        if (Q_stricmp(field->name, key) == 0) {
            break;
        }
        field = field + 1;
    }
    switch (field->type) {
    case 0:
        *(int *)(base + field->ofs) = atoi(value);
        return;
    case 1: {
        double d = atof(value);
        *(float *)(base + field->ofs) = (float)d;
        return;
    }
    default:
        return;
    case 3:
        ScrStr_AssignRef((uint16 *)(base + field->ofs), 0);
        *(uint16 *)(base + field->ofs) = Print_UnescapeAndEmit(value);
        return;
    case 4:
        G_VecClear(v);
        sscanf(value, "%f %f %f", &v[0], &v[1], &v[2]);
        ((uint32 *)(base + field->ofs))[0] = FloatBits(v[0]);
        ((uint32 *)(base + field->ofs))[1] = FloatBits(v[1]);
        ((uint32 *)(base + field->ofs))[2] = FloatBits(v[2]);
        return;
    case 8:
        break;
    }
    if (*value == '*') {
        uint32 n = (uint32)atoi(value + 1);
        ent->s.index = (int)(n & 0xffff);
        return;
    }
    Ent_SetModelByName(ent, value);
}

// 0x0811a7b0  size=120  FUN_0811a7b0  status=translated  naming=inferred
// Applies all parsed spawn vars (count at 0x0864d6cc, pairs at 0x0864d6d0) to the entity, then G_SetPositionTrajectory / G_SetAnglesTrajectory on its origin / angles
// (G_SetOrigin / G_SetAngles-like).
void G_ParseSpawnVarsToEntity(gentity_t *ent)
{
    int i;
    for (i = 0; i < g_spawnVars.numSpawnVars; i = i + 1) {
        G_ParseField(g_spawnVars.spawnVars[i].key, g_spawnVars.spawnVars[i].value, ent);
    }
    G_SetPositionTrajectory(ent, ent->r.currentOrigin);
    G_SetAnglesTrajectory(ent, ent->r.currentAngles);
}

// 0x0811a828  size=132  FUN_0811a828  status=translated  naming=inferred
// Spawns a script object (script_struct) from the current spawn vars: starts the script at 0x0884e7b4, takes the resulting
// object and sets every script-defined key.
void G_SpawnScriptStructFromSpawnVars(void)
{
    uint32 obj;
    int i;
    int field;

    Scr_SpawnThread(g_0884e7b4, 0);
    obj = Scr_GetObject(0);
    for (i = 0; i < g_spawnVars.numSpawnVars; i = i + 1) {
        field = G_ParseScrFieldValue(g_spawnVars.spawnVars[i].key, g_spawnVars.spawnVars[i].value);
        if (field != 0) {
            Scr_FinishCallAt(obj, field);
        }
    }
}

// 0x0811a8ac  size=109  FUN_0811a8ac  status=translated  naming=inferred
// Copies every table-described field (types 0/1 as dword, 3 script string with refcount, 4 vec3, 8 byte) from src to dst
// over the field table at 0x0815e820.
void G_CopyEntityFields(gentity_t *dst, gentity_t *src)
{
    entityFieldDef_t *field;
    byte *d = (byte *)dst;
    byte *s = (byte *)src;

    for (field = g_entityScriptFieldTable; field->name != 0; field = field + 1) {
        switch (field->type) {
        case 0:
            *(uint32 *)(d + field->ofs) = *(uint32 *)(s + field->ofs);
            break;
        case 1:
            *(uint32 *)(d + field->ofs) = *(uint32 *)(s + field->ofs);
            break;
        case 3:
            ScrStr_AssignRef((uint16 *)(d + field->ofs), *(uint16 *)(s + field->ofs));
            break;
        case 4: {
            G_VecCopy2((float *)(s + field->ofs), (float *)(d + field->ofs));
            break;
        }
        case 8:
            d[field->ofs] = s[field->ofs];
        }
    }
}

// 0x0811a9ba  size=192  FUN_0811a9ba  status=translated  naming=inferred
// Item lookup by classname: "weapon_<name>" resolves the weapon index (G_LoadWeaponDef) and returns its item (BG_GetItemByIndex);
// otherwise searches only table entries 0x81..0x82 of the 0x2c-stride list at 0x081850c0 by strcmp. Returns NULL if none.
gitem_t *G_FindItemForClassname(const char *classname)
{
    int weapon;
    int i;

    if (strncmp(classname, "weapon_", 7) == 0) {
        weapon = G_LoadWeaponDef(classname + 7);
        if (weapon != 0) {
            BG_GetWeaponDef(weapon);
            return (gitem_t *)BG_GetItemByIndex(weapon); /* gitem_t is a chunk-private layout of canonical struct gitem_s */
        }
    }
    i = 0x81;
    while (true) {
        if (0x82 < i) {
            return 0;
        }
        if (strcmp(g_081850c0[i].classname, classname) == 0) {
            break;
        }
        i = i + 1;
    }
    return &g_081850c0[i];
}

// 0x0811aa7a  size=232  FUN_0811aa7a  status=translated  naming=observed
// G_CallSpawn (string "G_CallSpawn: NULL classname\n"): reads "classname" from the spawn vars; item classnames spawn an item,
// otherwise the spawn-function table at 0x08188a00 is searched (entries whose handler is G_FreeEntity, script_struct, are skipped
// without creating an entity); an unmatched classname still creates and fills an entity but calls nothing and prints nothing.
void G_CallSpawn(void)
{
    const char *classname;
    gitem_t *item;
    gentity_t *ent;
    spawnDef_t *spawn;

    G_SpawnString("classname", "", &classname);
    if (classname == 0) {
        Com_Printf("G_CallSpawn: NULL classname\n");
    } else {
        item = G_FindItemForClassname(classname);
        if (item == 0) {
            item = 0;
            for (spawn = g_spawnFuncTable; spawn->name != 0; spawn = spawn + 1) {
                if (strcmp(spawn->name, classname) == 0) {
                    if (spawn->spawn == G_FreeEntity) {
                        return;
                    }
                    ent = G_Spawn();
                    G_ParseSpawnVarsToEntity(ent);
                    spawn->spawn(ent);
                    return;
                }
            }
            ent = G_Spawn();
            G_ParseSpawnVarsToEntity(ent);
        } else {
            ent = G_Spawn();
            G_ParseSpawnVarsToEntity(ent);
            Item_SetupDroppedWeapon(ent, item);
        }
    }
}

// 0x0811ab62  size=229  FUN_0811ab62  status=translated  naming=observed
// G_CallSpawnEntity (string "G_CallSpawnEntity: NULL classname\n"): spawns using ent's classname handle; returns 1 on success,
// 0 if no classname or no spawn function ("%s doesn't have a spawn function\n").
int G_CallSpawnEntity(gentity_t *ent)
{
    const char *classname;
    gitem_t *item;
    spawnDef_t *spawn;
    int result;

    if (ent->classname == 0) {
        Com_Printf("G_CallSpawnEntity: NULL classname\n");
        result = 0;
    } else {
        classname = ScrStr_TextFromHandle(ent->classname);
        item = G_FindItemForClassname(classname);
        if (item == 0) {
            for (spawn = g_spawnFuncTable; spawn->name != 0; spawn = spawn + 1) {
                if (strcmp(spawn->name, classname) == 0) {
                    spawn->spawn(ent);
                    return 1;
                }
            }
            const char *text = ScrStr_TextFromHandle(ent->classname);
            Com_Printf("%s doesn't have a spawn function\n", text);
            result = 0;
        } else {
            Item_SetupDroppedWeapon(ent, item);
            result = 1;
        }
    }
    return result;
}

// 0x0811ac48  size=77  FUN_0811ac48  status=translated  naming=inferred
// Registers every entity field-table name as a class-0 script field (index = table slot) then calls Scr_RegisterPlayerFields (further field registration).
void G_RegisterEntityScriptFields(void)
{
    entityFieldDef_t *field;
    for (field = g_entityScriptFieldTable; field->name != 0; field = field + 1) {
        Scr_AddClassField(0, field->name, (uint16)(field - g_entityScriptFieldTable));
    }
    Scr_RegisterPlayerFields();
}

// 0x0811ac96  size=28  FUN_0811ac96  status=translated  naming=observed
// Loads the "radiant" field description files with extension "txt".
void G_LoadRadiantFieldFiles(void)
{
    Scr_LoadFieldFiles("radiant", "txt");
}


// 0x0811acb2  size=204  FUN_0811acb2  status=translated  naming=inferred
// Script "set entity field" for class 0: indices with both top field bits (0xc000) go to the entity's class-1 object
// (Scr_SetPlayerField, 0 result if absent); otherwise the table entry's setter is called, or the generic param-based setter if it has none.
int Scr_SetEntityField(int entNum, uint32 fieldIndex)
{
    int result;
    gentity_t *ent = &g_gentities[entNum];

    if ((fieldIndex & 0xc000) == 0xc000) {
        if (ent->client == 0) {
            result = 0;
        } else {
            Scr_SetPlayerField(ent->client, fieldIndex & 0xffff3fff);
            result = 1;
        }
    } else if (g_entityScriptFieldTable[fieldIndex].setter == 0) {
        Scr_SetEntityFieldGeneric(ent, g_entityScriptFieldTable[fieldIndex].type, g_entityScriptFieldTable[fieldIndex].ofs);
        result = 1;
    } else {
        g_entityScriptFieldTable[fieldIndex].setter(ent, (int)fieldIndex);
        result = 1;
    }
    return result;
}

// 0x0811ad7e  size=227  FUN_0811ad7e  status=translated  naming=inferred
// Stores script param 0 into the entity field at byte offset ofs according to the field type: 0 int, 1 float, 3 script
// string, 4 vec3, 5 entity pointer, 6 y component of a vector stored as raw dword. Other types do nothing.
void Scr_SetEntityFieldGeneric(void *obj, int type, int ofs)
{
    gentity_t *ent = (gentity_t *)obj;
    byte *base = (byte *)obj;
    uint32 handle;
    float vec[3];
    int ivalue;
    float f;

    switch (type) {
    case 0:
        ivalue = Scr_GetInt(0);
        *(int *)(base + ofs) = ivalue;
        break;
    case 1:
        f = Scr_GetFloat(0);
        *(float *)(base + ofs) = f;
        break;
    case 3:
        handle = Scr_GetOptionalAny(0);
        ScrStr_AssignRef((uint16 *)(base + ofs), handle);
        break;
    case 4:
        Scr_GetVector(0, vec);
        *(uint32 *)(base + ofs) = FloatBits(vec[0]);
        *(uint32 *)(base + ofs + 4) = FloatBits(vec[1]);
        *(uint32 *)(base + ofs + 8) = FloatBits(vec[2]);
        break;
    case 5:
        *(gentity_t **)(base + ofs) = Scr_GetEntityParam(0);
        break;
    case 6:
        Scr_GetVector(0, vec);
        *(uint32 *)(base + ofs) = FloatBits(vec[1]);
    }
}

// 0x0811ae62  size=79  FUN_0811ae62  status=translated  naming=inferred
// Dispatch of "set field": class 0 = entity fields, class 1 = HudElem_ScriptSetField (always reports success).
int Scr_SetObjectField(int classnum, int entNum, uint32 fieldIndex)
{
    int result;
    if (classnum == 0) {
        result = Scr_SetEntityField(entNum, fieldIndex);
    } else {
        if (classnum == 1) {
            HudElem_ScriptSetField(entNum, fieldIndex);
        }
        result = 1;
    }
    return result;
}


// 0x0811aeb2  size=138  FUN_0811aeb2  status=translated  naming=inferred
// Pushes entity field fieldIndex on the script stack: 0xc000-tagged indices go to the class-1 object (Scr_GetPlayerField),
// others use Scr_AddEntityFieldValue with the field table's type and offset.
void Scr_GetEntityField(int entNum, uint32 fieldIndex)
{
    gentity_t *ent = &g_gentities[entNum];
    if ((fieldIndex & 0xc000) == 0xc000) {
        if (ent->client != 0) {
            Scr_GetPlayerField(ent->client, fieldIndex & 0xffff3fff);
        }
    } else {
        Scr_AddEntityFieldValue(ent, g_entityScriptFieldTable[fieldIndex].type, g_entityScriptFieldTable[fieldIndex].ofs);
    }
}

// 0x0811af3c  size=287  FUN_0811af3c  status=translated  naming=inferred
// Pushes the entity field at byte offset ofs by type: 0 int, 1 float bits, 2 inline string, 3 string id (if nonzero),
// 4 vec3, 5 entity (if nonnull), 6 vec3(0, value, 0), 7 object handle (if nonzero), 8 model name from the byte index.
void Scr_AddEntityFieldValue(void *obj, int type, int ofs)
{
    gentity_t *ent = (gentity_t *)obj;
    byte *base = (byte *)obj;
    float vec[3];
    uint16 handle;
    const char *text;

    switch (type) {
    case 0:
        Scr_AddIndex(*(int *)(base + ofs));
        break;
    case 1:
        Scr_AddFloatBits(*(uint32 *)(base + ofs));
        break;
    case 2:
        Scr_AddString((const char *)(base + ofs));
        break;
    case 3:
        /* gameplay-fix(gt_tdm): 0x0811af3c reads the string handle with movzx (u16), not movsx; ids >= 0x8000 were sign-extended */
        handle = *(uint16 *)(base + ofs);
        if (handle != 0) {
            Scr_AddStringId((uint32)handle);
        }
        break;
    case 4:
        Scr_AddVector((const float *)(base + ofs));
        break;
    case 5:
        if (*(int *)(base + ofs) != 0) {
            Scr_AddEntityParam(*(gentity_t **)(base + ofs));
        }
        break;
    case 6:
        ((uint32 *)vec)[0] = 0;
        ((uint32 *)vec)[1] = *(uint32 *)(base + ofs);
        ((uint32 *)vec)[2] = 0;
        Scr_AddVector(vec);
        break;
    case 7:
        /* gameplay-fix(gt_tdm): 0x0811b024 movzx word: the object id (client pers) is u16; ids >= 0x8000 were sign-extended */
        if (*(uint16 *)(base + ofs) != 0) {
            Scr_AddObject((uint32)*(uint16 *)(base + ofs));
        }
        break;
    case 8:
        text = G_GetModelName(*(byte *)(base + ofs));
        Scr_AddString(text);
    }
}

// 0x0811b05c  size=66  FUN_0811b05c  status=translated  naming=inferred
// Dispatch of "get field": class 0 -> Scr_GetEntityField, class 1 -> HudElem_ScriptGetField.
void Scr_GetObjectField(int classnum, int entNum, uint32 fieldIndex)
{
    if (classnum == 0) {
        Scr_GetEntityField(entNum, fieldIndex);
    } else if (classnum == 1) {
        HudElem_ScriptGetField(entNum, fieldIndex);
    }
}

// 0x0811b09e  size=138  FUN_0811b09e  status=translated  naming=inferred
// Releases script-string fields of the entity: every type-3 table field is reassigned 0, then the 7 byte/uint16 slot pairs
// at +0x210/+0x218 are cleared.
void G_FreeEntityFields(gentity_t *ent)
{
    entityFieldDef_t *field;
    int i;

    for (field = g_entityScriptFieldTable; field->name != 0; field = field + 1) {
        if (field->type == 3) {
            ScrStr_AssignRef((uint16 *)((byte *)ent + field->ofs), 0);
        }
    }
    for (i = 0; i < 7; i = i + 1) {
        ent->attachModelNames[i] = 0;
        ScrStr_AssignRef(&ent->attachTagNames[i], 0);
    }
}

// 0x0811b128  size=40  FUN_0811b128  status=translated  naming=inferred
// Frees the entity's script fields and queues its script object (class 0) for release.
void G_FreeScriptEntity(gentity_t *ent)
{
    G_FreeEntityFields(ent);
    Scr_QueueEntityRelease(ent->s.number, 0);
}

// 0x0811b150  size=29  FUN_0811b150  status=translated  naming=inferred
// Pushes the entity's script object (class 0) on the stack.
void Scr_AddEntityParam(gentity_t *ent)
{
    Scr_AddObjectByLookup(ent->s.number, 0);
}

// 0x0811b16e  size=91  FUN_0811b16e  status=translated  naming=observed
// Reads script param as an entity reference; high half nonzero -> error "not an entity" and NULL, else the entity from the array at 0x08716400.
gentity_t *Scr_GetEntityParam(uint32 param)
{
    uint32 ref;
    gentity_t *result;

    Scr_GetEntity(&ref, param);
    if ((ref >> 16) == 0) {
        result = &g_gentities[ref & 0xffff];
    } else {
        Scr_ErrorAtParam((int32)param, "not an entity");
        result = 0;
    }
    return result;
}

// 0x0811b1ca  size=52  FUN_0811b1ca  status=translated  naming=unknown
// Frees a class-1 script object living in the 0x8c-stride array at 0x08628f80: HudElem_ReleaseStringFields(obj), then queues release of
// its index (computed with the compiler's multiply-by-inverse of the 0x8c stride) with classnum 1.
void HudElem_FreeScriptObject(void *obj)
{
    HudElem_ReleaseStringFields((game_hudelem_s *)obj);
    Scr_QueueEntityRelease((((int)obj - (int)g_hudelems) >> 2) * (int)0x8af8af8b, 1) /* binary: obj - 0x08628f80 */;
}

// 0x0811b1fe  size=41  FUN_0811b1fe  status=translated  naming=unknown
// Pushes the class-1 object's script handle (index in the 0x8c-stride array at 0x08628f80).
void Scr_AddHudElemObject(void *obj)
{
    Scr_AddObjectByLookup((((int)obj - (int)g_hudelems) >> 2) * (int)0x8af8af8b, 1) /* binary: obj - 0x08628f80 */;
}

// 0x0811b284  size=46  FUN_0811b284  status=translated  naming=inferred
// Scr_CallMethod on the entity's number with nameId 0.
uint16 Scr_CallEntityMethod(gentity_t *ent, int codeOffset, int argc)
{
    return Scr_CallMethod((uint32)ent->s.number, 0, codeOffset, argc);
}

// 0x0811b2b2  size=43  FUN_0811b2b2  status=translated  naming=inferred
// Scr_SpawnMethodThread on the entity's number with nameId 0.
void Scr_SpawnEntityMethodThread(gentity_t *ent, int codeOffset, int argc)
{
    Scr_SpawnMethodThread((uint32)ent->s.number, 0, codeOffset, argc);
}

// 0x0811b2de  size=51  FUN_0811b2de  status=translated  naming=inferred
// Scr_NotifyNum-style notify on the entity's number (nameId 0), with the given notify string id and argument count.
void Scr_NotifyEntity(gentity_t *ent, uint16 stringId, int argc)
{
    Scr_NotifyNum((uint32)ent->s.number, 0, stringId, argc);
}

// 0x0811b546  size=36  FUN_0811b546  status=translated  naming=inferred
// Scr_FinishCallForObject on the entity's number with object class 0 and the given field id.
void Scr_SetObjectFieldValue(gentity_t *ent, int fieldId)
{
    Scr_FinishCallForObject((uint32)ent->s.number, 0, (uint32)fieldId);
}

// 0x0811b56a  size=517  FUN_0811b56a  status=translated  naming=observed
// SP_worldspawn (string "SP_worldspawn: The first entity isn't 'worldspawn'"): validates classname, then sets configstrings
// 2 ("cod"), 0xd (%i of 0x0864c578), 3 (ambient track "n\<name>" or ""), 4 (message), 0xe (dvar 0x087a243c string), the gravity
// dvar (default "800"), 0xb (northyaw or "0") and fills the globals at 0x087a2110/0x087a1fa0/0x087a2108/0x087a209c.
void SP_worldspawn(void)
{
    const char *s;
    char *text;
    double d;

    G_SpawnString("classname", "", &s);
    if (Q_stricmp(s, "worldspawn") != 0) {
        Com_Error(1, "\025SP_worldspawn: The first entity isn't 'worldspawn'");
    }
    SV_SetConfigstring(2, "cod");
    text = va("%i", level_startTime);
    SV_SetConfigstring(0xd, text);
    G_SpawnString("ambienttrack", "", &s);
    if (*s == '\0') {
        SV_SetConfigstring(3, "");
    } else {
        text = va("n\\%s", s);
        SV_SetConfigstring(3, text);
    }
    G_SpawnString("message", "", &s);
    SV_SetConfigstring(4, s);
    SV_SetConfigstring(0xe, g_087a243c->current.string);
    G_SpawnString("gravity", "800", &s);
    d = atof(s);
    Dvar_SetFloat(g_087a2414, (float)d);
    G_SpawnString("northyaw", "", &s);
    if (*s == '\0') {
        SV_SetConfigstring(0xb, "0");
    } else {
        SV_SetConfigstring(0xb, s);
    }
    G_SpawnString("spawnflags", "0", &s);
    g_087a2110 = atoi(s);
    g_dummyEntity = 0x3fe;
    ScrStr_AssignRef(&g_087a2108, scrStr_worldspawn);
    g_087a209c = 1;
}

// 0x0811b770  size=74  FUN_0811b770  status=translated  naming=inferred
// G_SpawnEntitiesFromString counterpart (string "SpawnEntities: no entities"): requires a first entity, spawns worldspawn, then
// spawns every following entity via G_CallSpawn.
void G_SpawnEntitiesFromString(void)
{
    if (KeyValue_ParseBlock((kv_table_s *)&g_spawnVars) == 0) {
        Com_Error(1, "\025SpawnEntities: no entities");
    }
    SP_worldspawn();
    while (KeyValue_ParseBlock((kv_table_s *)&g_spawnVars) != 0) {
        G_CallSpawn();
    }
}

// 0x0811b7ba  size=125  FUN_0811b7ba  status=translated  naming=inferred
// First pass over the entity string: runs the level script at 0x0884e7b0, then spawns script objects for every "script_struct"
// classname entity (SV_BeginEntityParse at the end).
void G_SpawnScriptStructs(void)
{
    const char *classname;
    uint16 handle = Scr_CallScriptOffset(g_0884e7b0, 0);
    Scr_ReleaseHandle(handle);
    while (KeyValue_ParseBlock((kv_table_s *)&g_spawnVars) != 0) {
        G_SpawnString("classname", "", &classname);
        if (strcmp("script_struct", classname) == 0) {
            G_SpawnScriptStructFromSpawnVars();
        }
    }
    SV_BeginEntityParse();
}

// 0x0811b838  size=41  FUN_0811b838  status=translated  naming=inferred
// Zero a vec3.
void G_VecClear(float *v)
{
    ((uint32 *)v)[0] = 0;
    ((uint32 *)v)[1] = 0;
    ((uint32 *)v)[2] = 0;
}

// 0x0811b862  size=47  FUN_0811b862  status=translated  naming=inferred
// vec3 copy (src, dst) -- a further duplicate of VectorCopy used by the field copier.
void G_VecCopy2(const float *src, float *dst)
{
    ((uint32 *)dst)[0] = ((const uint32 *)src)[0];
    ((uint32 *)dst)[1] = ((const uint32 *)src)[1];
    ((uint32 *)dst)[2] = ((const uint32 *)src)[2];
}

// 0x0811b894  size=361  FUN_0811b894  status=translated  naming=inferred
// StringToFilter counterpart: parses "a.b.c.d" (shorter forms allowed) into mask/compare bytes; a zero component leaves its mask
// byte 0. On a non-digit start prints "Bad filter address: %s\n" (with the remaining text) and returns 0.
int StringToFilter(const char *s, ipFilter_t *filter)
{
    byte compare[4];
    byte mask[4];
    char num[128];
    int i;
    int j;

    for (i = 0; i < 4; i = i + 1) {
        mask[i] = 0;
        compare[i] = 0;
    }
    for (i = 0; i < 4; i = i + 1) {
        if (*s < '0' || '9' < *s) {
            Com_Printf("Bad filter address: %s\n", s);
            return 0;
        }
        j = 0;
        while ('/' < *s && *s < ':') {
            num[j] = *s;
            j = j + 1;
            s = s + 1;
        }
        num[j] = '\0';
        compare[i] = (byte)atoi(num);
        if (compare[i] != 0) {
            mask[i] = 0xff;
        }
        if (*s == '\0') {
            break;
        }
        s = s + 1;
    }
    filter->mask = *(uint32 *)mask;
    filter->compare = *(uint32 *)compare;
    return 1;
}

// 0x0811b9fe  size=212  FUN_0811b9fe  status=translated  naming=inferred
// UpdateIPBans counterpart: rebuilds the "a.b.c.d " list of all used filters (compare != -1) from the compare words
// (low byte first) and stores it in the g_banIPs dvar (0x087a2484).
void UpdateIPBans(void)
{
    char list[1032];
    int i;
    uint32 b;
    size_t len1;
    size_t len2;

    list[0] = '\0';
    for (i = 0; i < g_08852e00; i = i + 1) {
        if (ipFilters[i].compare != 0xffffffff) {
            b = ipFilters[i].compare;
            len1 = strlen(list);
            len2 = strlen(list);
            Com_sprintf(list + len2, 0x400 - len1, "%i.%i.%i.%i ", b & 0xff, b >> 8 & 0xff, b >> 16 & 0xff, b >> 24);
        }
    }
    Dvar_SetString(g_087a2484, list);
}

// 0x0811bad2  size=142  FUN_0811bad2  status=translated  naming=observed
// AddIP: reuses the first free filter slot (compare == -1) or appends (max 0x400, "IP filter list is full\n"), parses the
// address (failure marks the slot free) and republishes g_banIPs.
void AddIP(const char *str)
{
    int i;

    for (i = 0; i < g_08852e00 && ipFilters[i].compare != 0xffffffff; i = i + 1) {
    }
    if (i == g_08852e00) {
        if (g_08852e00 == 0x400) {
            Com_Printf("IP filter list is full\n");
            return;
        }
        g_08852e00 = g_08852e00 + 1;
    }
    if (StringToFilter(str, &ipFilters[i]) == 0) {
        ipFilters[i].compare = 0xffffffff;
    }
    UpdateIPBans();
}

// 0x0811bb60  size=161  FUN_0811bb60  status=translated  naming=inferred
// G_ProcessIPBans counterpart: reloads the filters from the g_banIPs dvar string (space-separated, a token is only
// processed if followed by a space).
void G_ProcessIPBans(void)
{
    char buf[1032];
    char *token;
    char *p;

    g_08852e00 = 0;
    Q_strncpyz(buf, g_087a2484->current.string, 0x400);
    p = buf;
    while (*p != '\0') {
        token = p;
        p = strchr(p, 0x20);
        if (p == 0) {
            break;
        }
        for (; *p == ' '; p = p + 1) {
            *p = '\0';
        }
        if (*token != '\0') {
            AddIP(token);
        }
    }
}

// 0x0811bc02  size=79  FUN_0811bc02  status=translated  naming=observed
// Svcmd_AddIP_f ("Usage:  addip <ip-mask>\n"): takes console arg 1.
void Svcmd_AddIP_f(void)
{
    char arg[1032];
    if (Cmd_Argc_alt() < 2) {
        Com_Printf("Usage:  addip <ip-mask>\n");
    } else {
        Cmd_ArgvBuffer_alt(1, arg, 0x400);
        AddIP(arg);
    }
}

// 0x0811bc52  size=207  FUN_0811bc52  status=translated  naming=observed
// Svcmd_RemoveIP_f ("Usage:  sv removeip <ip-mask>\n", "Removed.\n", "Didn't find %s.\n"): frees the exact mask/compare match and republishes g_banIPs.
void Svcmd_RemoveIP_f(void)
{
    char arg[1028];
    ipFilter_t filter;
    int i;

    if (Cmd_Argc_alt() < 2) {
        Com_Printf("Usage:  sv removeip <ip-mask>\n");
    } else {
        Cmd_ArgvBuffer_alt(1, arg, 0x400);
        if (StringToFilter(arg, &filter) != 0) {
            for (i = 0; i < g_08852e00; i = i + 1) {
                if (ipFilters[i].mask == filter.mask && ipFilters[i].compare == filter.compare) {
                    ipFilters[i].compare = 0xffffffff;
                    Com_Printf("Removed.\n");
                    UpdateIPBans();
                    return;
                }
            }
            Com_Printf("Didn't find %s.\n", arg);
        }
    }
}

// 0x0811bd22  size=290  FUN_0811bd22  status=translated  naming=observed
// Svcmd_EntityList_f (strings "ET_GENERAL", "ET_PLAYER", ...): lists all in-use entities from index 1 with their type name
// (padded) and classname. Extra printf arguments of the original are kept.
void Svcmd_EntityList_f(void)
{
    gentity_t *ent;
    int i;
    int arg;

    ent = &g_gentities[1];
    for (i = 1; i < level_numEntities; i = i + 1) {
        if (ent->r.inuse != 0) {
            arg = i;
            Com_Printf("%3i:", i);
            switch (ent->s.eType) {
            case 0:
                Com_Printf("ET_GENERAL          ", arg);
                break;
            case 1:
                Com_Printf("ET_PLAYER           ", arg);
                break;
            default:
                arg = ent->s.eType;
                Com_Printf("%3i                 ", arg);
                break;
            case 3:
                Com_Printf("ET_ITEM             ", arg);
                break;
            case 4:
                Com_Printf("ET_MISSILE          ", arg);
                break;
            case 5:
                Com_Printf("ET_INVISIBLE        ", arg);
                break;
            case 6:
                Com_Printf("ET_SCRIPTMOVER      ", arg);
            }
            if (ent->classname != 0) {
                arg = (int)ScrStr_TextFromHandle(ent->classname);
                Com_Printf("%s", arg);
            }
            Com_Printf("\n", arg);
        }
        ent = ent + 1;
    }
}

// 0x0811be44  size=288  FUN_0811be44  status=translated  naming=inferred
// ClientForString counterpart over the server client array (stride 0x28a4): a leading digit selects a slot (validated against
// maxclients and the connected flag), otherwise matches the name (Q_stricmp). Returns NULL with a message on failure.
gclient_t *ClientForString(const char *s)
{
    gclient_t *cl;
    int slot;
    int i;
    gclient_t *result;

    if (*s < '0' || '9' < *s) {
        for (i = 0; i < level_maxclients; i = i + 1) {
            cl = (gclient_t *)((byte *)level_clients + i * 0x28a4);
            if (cl->connected != 0 && Q_stricmp(cl->cs.name, s) == 0) {
                return cl;
            }
        }
        Com_Printf("User %s is not on the server\n", s);
        result = 0;
    } else {
        slot = atoi(s);
        if (slot < 0 || level_maxclients <= slot) {
            Com_Printf("Bad client slot: %i\n", slot);
            result = 0;
        } else {
            result = (gclient_t *)((byte *)level_clients + slot * 0x28a4);
            if (result->connected == 0) {
                Com_Printf("Client %i is not connected\n", slot);
                result = 0;
            }
        }
    }
    return result;
}

// 0x0811bf64  size=362  FUN_0811bf64  status=translated  naming=observed
// ConsoleCommand (strings "entitylist", "addip", ...): dispatches entitylist / addip / removeip / listip (runs "g_banIPs\n"
// via Cbuf_ExecuteText insert); otherwise, if the dvar at 0x087a240c is nonzero and the command is "say", broadcasts a
// server chat message with the remaining args. Returns 1 if handled.
int G_ConsoleCommand(void)
{
    char cmd[1032];
    char *text;
    char *msg;

    Cmd_ArgvBuffer_alt(0, cmd, 0x400);
    if (Q_stricmp(cmd, "entitylist") == 0) {
        Svcmd_EntityList_f();
        return 1;
    }
    if (Q_stricmp(cmd, "addip") == 0) {
        Svcmd_AddIP_f();
        return 1;
    }
    if (Q_stricmp(cmd, "removeip") == 0) {
        Svcmd_RemoveIP_f();
        return 1;
    }
    if (Q_stricmp(cmd, "listip") == 0) {
        Cbuf_ExecuteText(1, "g_banIPs\n");
        return 1;
    }
    if (g_087a240c->current.integer != 0 && Q_stricmp(cmd, "say") == 0) {
        text = ConcatArgs(1);
        msg = va("%c \"GAME_SERVER\025: %s\"", 0x65, text);
        SV_SendServerCommandNum(-1, 0, msg);
        return 1;
    }
    return 0;
}
