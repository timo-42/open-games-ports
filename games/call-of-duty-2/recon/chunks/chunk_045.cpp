#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_045_types.h"


/* Chunk 045 (0x080f14aa..0x080f5682): first-person weapon view motion, WeaponDef loading and
 * the field parser, AI accuracy graphs, box/entity trace sweeps, entity touch dispatch, client
 * button edges, inactivity drop and player event dispatch. Names are described per function. */

/* ---- helpers for the decompiled idioms ----------------------------------------------- */

/* 0x080a3d66 leaves its float result in st(0) (tail call to 0x080a8476 which fld's the value).
 * The shared prototype declares it void, so the call goes through a float-returning pointer type
 * that matches the binary's use of the return value. Remove when the shared prototype is fixed. */
typedef float (*angle_approach_wrapped_fn)(float target, float current, float gain, float frameTime);
static float Angle_ApproachProportionalWrapped_F(float target, float current, float gain, float frameTime)
{
    return ((angle_approach_wrapped_fn)Angle_ApproachProportionalWrapped)(target, current, gain, frameTime);
}

/* Ghidra prints these conversions as ROUND(); the binary emits fldcw (RC = truncate) + fistp around each
 * of them (0x080f2137, 0x080f431c.., 0x080f4a3d, 0x080f4a5f, 0x080f540f), i.e. a C (int) cast. */
static int32 Round_To_Int(double x)
{
    return (int32)x;
}

/* Reinterpret an int32 slot as float bits (passed raw to a float parameter in the binary). */
static float Int_Bits_To_Float(int32 bits)
{
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

/* Keep the upper 16 bits of f and replace the lower 16 bits with low (entity number word). */
static float Float_SetLow16(float f, uint16 low)
{
    uint32 bits;
    memcpy(&bits, &f, sizeof(bits));
    bits = (bits & 0xffff0000u) | (uint32)low;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

/* Ghidra "x != 0.0 || NAN(x)" test. */
static int Float_NonZeroOrNaN(float x)
{
    return (x != 0.0f) || (x != x);
}

/* Client number from a gclient pointer: ((ptr - base) >> 2) * 0x644b3219 (stride 0x28a4). */
static int32 Client_Number(const struct gclient_s *cl)
{
    int32 diff = (int32)((const char *)cl - (const char *)level_clients);
    return (int32)((uint32)(diff >> 2) * 0x644b3219u);
}

/* ---- forward declarations of chunk functions (defined below in address order) ----------- */
float Float_Abs_080f28cc(float x);
float Float_Max2(float a, float b);
float Float_Min2(float a, float b);
int32 Int_Max2(int32 a, int32 b);
float Float_Clamp(float x, float lo, float hi);
long double Float_SquareLD(float x);
long double Vec2_LengthSquared(const float *v);
void Vec3_Clear_080f29c0(float *v);
void Vec3_Copy_080f29ea(const float *src, float *dst);
void Vec3_Add_080f2a1a(const float *a, const float *b, float *out);
void Vec3_Scale_080f2a5e(const float *v, float s, float *out);
void Vec3_MultAdd(const float *a, float s, const float *b, float *out);
float Float_SelectNonNegative(float x, float a, float b);
int32 Int_SelectIfNegative(int32 x, int32 a, int32 b);
void BitArray_ClearBit(uint32 *bits, int32 bit);
const char *BG_WeaponTypeName(int32 index);
int32 BG_WeaponSlotFromName(const char *name);
const char *BG_WeaponSlotName(int32 index);
void BG_RegisterAnimTypes(void);
void BG_AnimLoadPlayerAnimTypes(void);
int32 StrArray_FindCI(const char *s, const char *const *arr, int32 count);
void WeaponDef_ComputeAdsTransInverse(struct weaponDef_s *wd);
void WeaponDef_SetDefaultDamageRange(struct weaponDef_s *wd);
void WeaponDef_ValidateDestabilization(struct weaponDef_s *wd);
void WeaponDef_SetString(char **dst, const char *value);
void WeaponDef_SetStringCallback(char **dst, const char *value);
void WeaponDef_SetDefaults(struct weaponDef_s *wd);
struct weaponDef_s *BG_LoadWeaponFile(const char *folder, const char *name);
struct weaponDef_s *BG_GetNoWeaponDef(void);
void Trace_ResetGlobals(void);
void Noop_Callback(void);
void Trace_ComputeSweepBounds(const float *start, const float *mins, const float *maxs,
                              const float *p4, float fraction, float *out6);
void Trace_SweepEntities(const float *start, const float *mins, const float *maxs, const float *p4,
                         int32 owner, uint32 mask, uint32 unused, struct trace_s *out);
void Trace_BoxWorldThenEntities(struct trace_s *trace, const float *start, const float *mins,
                                const float *maxs, const float *p5, int32 owner, uint32 mask);
uint32 Trace_ContentsWithEntities(const float *point, int32 owner, uint32 mask);
float Float_MaxB(float a, float b);
float Float_MinB(float a, float b);
void Vec3_ClearB(float *v);
void Vec3_CopyBits_080f3ef2(const float *src, float *dst);
float Float_SelectNonNegativeB(float x, float a, float b);
char *KeyValue_PoolAppend(struct kv_table_s *t, const char *s);
int32 KeyValue_ParseBlock(struct kv_table_s *t);
int32 KeyValue_Lookup(const struct kv_table_s *t, const char *key, const char *def,
                      const char **out);
uint32 Print_UnescapeAndEmit(const char *src);
const char *Vec3_FormatRing(const float *v);
int32 WeaponAccuracy_ParseGraph(const char *data, const char *name,
                                struct accuracy_knot_s *knots, int32 *count_out);
int32 WeaponAccuracy_LoadFile(struct weaponDef_s *wd, const char *folder, const char *name,
                              struct accuracy_knot_s *knots, int32 *count_out);
int32 WeaponDef_LoadAccuracyGraphs(struct weaponDef_s *wd);
void Client_ApplyRecoilKick(struct gentity_s *ent);
void Entity_ClearField84(void *ent);
void Entity_TouchList(struct gentity_s *self, struct pmove_s *pm);
void Entity_TouchLinked(struct gentity_s *self);
void Client_ProcessButtonsAndDrop(struct gentity_s *ent, const struct usercmd_s *cmd);
int32 Client_CheckInactivity(struct gclient_s *cl);
void Client_UpdateButtonEdges(struct gentity_s *ent, struct usercmd_s *cmd);
void Client_DispatchEvents(struct gentity_s *ent, uint32 start);
void Client_SetLastActivityTime(int32 clientNum, int32 time);
void Entity_SetContentsForClientFlags(struct gentity_s *ent);
void WeaponView_ApplyYawBob(struct view_motion_s *vm, float *out);
void WeaponView_ApplyTimedKickB(struct view_motion_s *vm, float *out);
int32 Spring1D_StepLimited(float *pos, float *vel, float dt, float posLimit, float restoreK,
                           float velLimit, float friction, float decel);
void WeaponView_IntegrateGunKick(struct view_motion_s *vm, float *out);
void WeaponView_ComputeOffset(struct view_motion_s *vm, float *out);
void WeaponView_ApplyTimedKickA(struct view_motion_s *vm, float *out);
void WeaponView_ApplyIdleSway(struct view_motion_s *vm, float *out);
void WeaponView_ApplyBobB(struct view_motion_s *vm, float *out);
void WeaponView_ApplyCrouchSway(struct view_motion_s *vm, float *out);
void WeaponView_Accumulate(struct view_motion_s *vm, float *out);
void WeaponView_ApplyAngleSpring(struct playerState_s *ps, float *viewAngles,
                                 struct view_spring_s *spring, float *angleState, float scale,
                                 int32 ms);
struct weaponDef_s *BG_LoadWeaponDef(const char *folder, const char *name);

// 0x080f14aa  size=449  FUN_080f14aa  status=translated  naming=inferred
// Evidence: phase from the playerstate yaw byte; amplitude from view_motion slot_04 * 0.16; output scaled by adsBobFactor (0x284), and by (1 - blend) when the weapon has an ADS overlay (0x278). Callers are WeaponView_ComputeOffset (0x080f1c60). No string names it.
void WeaponView_ApplyYawBob(struct view_motion_s *vm, float *out)
{
    struct playerState_s *ps = vm->ps;
    uint32 weapon = BG_GetCurrentWeapon(ps);
    struct weaponDef_s *wd = BG_GetWeaponDef(weapon);
    /* gameplay-fix(wpn_melee_ads_lean): x87 extended chain from 0x080f14d7: t=byte/255, 2*t*pi+2*pi rounded to float once ([ebp-0x2c]), then +pi/4+2*pi rounded once ([ebp-0x10]); the constants are the doubles at 0x081558a0..0x081558b0 */
    long double tL = (long double)(float)(byte)ps->bobCycle / 255.0L;
    long double tPi = tL * (long double)3.141592653589793;
    float phase = (float)(tPi + tPi + (long double)6.283185307179586);
    float offset[3];
    float phase_sum = (float)((long double)phase + (long double)0.7853981633974483 + (long double)6.283185307179586);
    /* gameplay-fix(net_clients): 0x080f1519: slot +4 is loaded with fld DWORD (float reinterpretation of the dword, e.g. an int timestamp -> ~0), not fild */
    float amplitude = Int_Bits_To_Float(vm->slot_04) * 0.16f;
    float blend = ps->fWeaponPosFrac;
    float scale;

    offset[0] = -(float)BG_BobSinePair(ps, phase_sum, amplitude, 10.0f);
    offset[1] = -(float)BG_BobSine(ps, phase_sum, amplitude, 10.0f);
    phase_sum = (float)((long double)phase_sum - (long double)0.4712389167638204); /* double at 0x081558c0 (0x080f1589) */
    amplitude = amplitude * 1.5f;
    offset[2] = Float_Min2((float)BG_BobSine(ps, phase_sum, amplitude, 10.0f), 0.0f);
    if (Float_NonZeroOrNaN(blend)) {
        scale = 1.0f - (1.0f - wd->adsBobFactor) * blend;
        offset[0] = offset[0] * scale;
        offset[1] = offset[1] * scale;
        offset[2] = offset[2] * scale;
    }
    if (wd->adsOverlayReticle != 0) {
        Vec3_Scale_080f2a5e(offset, 1.0f - blend, offset);
    }
    Vec3_Add_080f2a1a(out, offset, out);
}

// 0x080f166c  size=502  FUN_080f166c  status=translated  naming=inferred
// Evidence: timed kick channel B: start time in slot_20, current time in slot_1c, two integer amounts in slots 24/28; Ease_OutQuadSigned shapes the 100 ms rise and 400 ms fall.
void WeaponView_ApplyTimedKickB(struct view_motion_s *vm, float *out)
{
    if (vm->slot_20.value != 0) {
        struct playerState_s *ps = vm->ps;
        uint32 weapon = BG_GetCurrentWeapon(ps);
        struct weaponDef_s *wd = BG_GetWeaponDef(weapon);
        float kick_scale = ps->fWeaponPosFrac * 0.5f + 0.5f;
        float rise_end = kick_scale * 100.0f;
        float fall_span = kick_scale * 400.0f;
        float elapsed;
        float t;

        if (Float_NonZeroOrNaN(ps->fWeaponPosFrac) && wd->adsOverlayReticle != 0) {
            kick_scale = kick_scale * (1.0f - ps->fWeaponPosFrac * 0.75f);
        }
        elapsed = (float)(vm->slot_1c - vm->slot_20.value);
        if (elapsed < rise_end) {
            t = Ease_OutQuadSigned(elapsed / rise_end);
            kick_scale = t * kick_scale;
        } else {
            rise_end = 1.0f - (elapsed - rise_end) / fall_span;
            if (0.0f < rise_end) {
                t = Ease_OutQuadSigned(1.0f - rise_end);
                kick_scale = (1.0f - t) * kick_scale;
            } else {
                return;
            }
        }
        /* gameplay-fix(net_clients): 0x080f1774/0x080f1797/0x080f17b2 (and the fall-off branch 0x080f1811..0x080f184f): fmul DWORD [+0x24]/[+0x28] multiplies by the dwords as floats */
        out[0] = out[0] + kick_scale * Int_Bits_To_Float(vm->slot_24) * 0.5f;
        out[1] = out[1] - kick_scale * Int_Bits_To_Float(vm->slot_28);
        out[2] = out[2] + kick_scale * Int_Bits_To_Float(vm->slot_28) * 0.5f;
    }
}

// 0x080f1862  size=520  FUN_080f1862  status=translated  naming=inferred
// Evidence: one-axis damped spring with position clamp, restore toward zero, friction and a constant decel; resets both values and returns 1 when near rest and far from target.
int32 Spring1D_StepLimited(float *pos, float *vel, float dt, float posLimit, float restoreK,
                           float velLimit, float friction, float decel)
{
    /* The binary reloads *pos / *vel from memory (rounded to float) before every use; volatile keeps the
     * x87 from carrying extended precision across statements. */
    volatile float *p = pos;
    volatile float *v = vel;
    int32 reset = 0;

    /* binary: ja on (0.25 > |pos|) then (1.0 > |vel|) takes the reset path; an unordered compare falls into the step */
    if (!(0.25f > Float_Abs_080f28cc(*p) && 1.0f > Float_Abs_080f28cc(*v))) {
        *p = *p + *v * dt;
        if (posLimit < *p) {
            *p = posLimit;
            if (0.0f < *v) {
                *v = 0.0f;
            }
        } else if (*p < -posLimit && (*p = -posLimit, *v < 0.0f)) {
            *v = 0.0f;
        }
        if (0.0f < *p) {
            *v = *v - restoreK * dt;
        } else if (*p < 0.0f) {
            *v = *v + restoreK * dt;
        }
        *v = *v - *v * friction * dt;
        if (0.0f < *v) {
            *v = *v - decel * dt;
            if (*v < 0.0f) {
                *v = 0.0f;
            }
        } else {
            *v = *v + decel * dt;
            if (0.0f < *v) {
                *v = 0.0f;
            }
        }
        if (velLimit < *v) {
            *v = velLimit;
        } else if (*v < -velLimit) {
            *v = -velLimit;
        }
    } else {
        *p = 0.0f;
        *v = 0.0f;
        reset = 1;
    }
    return reset;
}

// 0x080f1a6a  size=502  FUN_080f1a6a  status=translated  naming=inferred
// Evidence: two Spring1D_StepLimited channels (gunMaxPitch 0x2e0, gunMaxYaw 0x2e4) integrated in 5 ms steps over slot_08; accel/speed/decay are blended hip (0x4d8..) to ADS (0x490..).
void WeaponView_IntegrateGunKick(struct view_motion_s *vm, float *out)
{
    struct playerState_s *ps = vm->ps;
    uint32 weapon = BG_GetCurrentWeapon(ps);
    struct weaponDef_s *wd = BG_GetWeaponDef(weapon);
    int32 active = BG_GetWeaponAimDownSight(weapon);

    if (active != 0) {
        float blend = ps->fWeaponPosFrac;
        float accel = wd->hipGunKickAccel + (wd->adsGunKickAccel - wd->hipGunKickAccel) * blend;
        float speedMax = wd->hipGunKickSpeedMax
                         + (wd->adsGunKickSpeedMax - wd->hipGunKickSpeedMax) * blend;
        float speedDecay = wd->hipGunKickSpeedDecay
                           + (wd->adsGunKickSpeedDecay - wd->hipGunKickSpeedDecay) * blend;
        float staticDecay = wd->hipGunKickStaticDecay
                            + (wd->adsGunKickStaticDecay - wd->hipGunKickStaticDecay) * blend;
        /* gameplay-fix(net_loss): 0x080f1b47..0x080f1b50 moves the dword at +0x08 into a stack slot and fld's it as a float
         * (no fild): the int is reinterpreted, not converted. (float)int made `remaining` ~1e9 and the loop below ran
         * until both springs rested (a ~400 ms server stall after gun fire with kick state). */
        float remaining;
        memcpy(&remaining, &vm->slot_08, sizeof(remaining));
        float step;
        int32 a;
        int32 b;

        do {
            if (remaining <= 0.0f) {
                break;
            }
            if (0.005 < remaining) {
                step = 0.005f;
                /* gameplay-fix(net_clients): 0x080f1b82..0x080f1b88: remaining - the double 0.005 (0x081558e0) in extended precision, stored to float; step stays the float 0.005f (0x3ba3d70a) */
                remaining = (float)((long double)remaining - (long double)0.005);
            } else {
                step = remaining;
                remaining = 0.0f;
            }
            a = Spring1D_StepLimited(&vm->pos[0], &vm->vel[0], step, wd->gunMaxPitch, accel,
                                     speedMax, speedDecay, staticDecay);
            b = Spring1D_StepLimited(&vm->pos[1], &vm->vel[1], step, wd->gunMaxYaw, accel,
                                     speedMax, speedDecay, staticDecay);
        } while (b == 0 || a == 0);
        Vec3_Add_080f2a1a(out, vm->pos, out);
    }
}

// 0x080f1c60  size=242  FUN_080f1c60  status=translated  naming=inferred
// Evidence: zeroes the output, adds lean (Ease_OutQuadSigned of 0x4c), gun/bob/kick terms, then AngleSubtract of the two angle targets. Orchestrates the view motion channels.
void WeaponView_ComputeOffset(struct view_motion_s *vm, float *out)
{
    struct playerState_s *ps = vm->ps;

    Vec3_Clear_080f29c0(out);
    if (Float_NonZeroOrNaN(ps->leanf)) {
        float t = Ease_OutQuadSigned(ps->leanf);
        out[2] = out[2] - (t + t);
    }
    WeaponView_ComputeAdsAimOffset(vm, out);
    WeaponView_ComputeIdleSway(vm, out);
    WeaponView_ApplyYawBob(vm, out);
    WeaponView_ApplyTimedKickB(vm, out);
    WeaponView_IntegrateGunKick(vm, out);
    out[0] = AngleSubtract(out[0], vm->angle_target[0]);
    out[1] = AngleSubtract(out[1], vm->angle_target[1]);
}

// 0x080f1d52  size=396  FUN_080f1d52  status=translated  naming=inferred
// Evidence: timed kick channel A: start in slot_04, current time in slot_08, amounts in slot_0c (x) and slot_10 (z); same Ease_OutQuadSigned rise (100) and fall (400) shape as channel B.
void WeaponView_ApplyTimedKickA(struct view_motion_s *vm, float *out)
{
    if (vm->slot_04 != 0) {
        struct playerState_s *ps = vm->ps;
        uint32 weapon = BG_GetCurrentWeapon(ps);
        struct weaponDef_s *wd = BG_GetWeaponDef(weapon);
        float kick_scale = 1.0f - ps->fWeaponPosFrac * 0.5f;
        float elapsed;
        float t;

        if (Float_NonZeroOrNaN(ps->fWeaponPosFrac) && wd->adsOverlayReticle != 0) {
            kick_scale = kick_scale * (ps->fWeaponPosFrac * 0.5f + 1.0f);
        }
        elapsed = (float)(vm->slot_08 - vm->slot_04);
        if (elapsed < 100.0f) {
            t = Ease_OutQuadSigned(elapsed / 100.0f);
            /* gameplay-fix(net_clients): 0x080f1e3f/0x080f1e5a: fmul DWORD [+0xc]/[+0x10] (floats), not fild */
            out[0] = out[0] + t * kick_scale * Int_Bits_To_Float(vm->slot_0c);
            out[2] = out[2] + t * kick_scale * Int_Bits_To_Float(vm->slot_10);
        } else {
            elapsed = 1.0f - (elapsed - 100.0f) / 400.0f;
            if (0.0f < elapsed) {
                t = Ease_OutQuadSigned(1.0f - elapsed);
                kick_scale = (1.0f - t) * kick_scale;
                out[0] = out[0] + kick_scale * Int_Bits_To_Float(vm->slot_0c);
                out[2] = out[2] + kick_scale * Int_Bits_To_Float(vm->slot_10);
            }
        }
    }
}

// 0x080f1ede  size=732  FUN_080f1ede  status=translated  naming=inferred
// Evidence: ADS-gated idle sway. Idle amount/speed blend hip (0x2cc/0x2d4) to ADS (0x2c8/0x2d0), stance factor from pm_flags, slew of slot_1c toward the factor, and a sine pair advancing the phase counter behind slot_20 (time-dependent sway).
void WeaponView_ApplyIdleSway(struct view_motion_s *vm, float *out)
{
    struct playerState_s *ps = vm->ps;
    uint32 weapon = BG_GetCurrentWeapon(ps);
    struct weaponDef_s *wd = BG_GetWeaponDef(weapon);

    if (wd->adsOverlayReticle != 0) {
        float amount;
        float speed;
        float factor;
        float sway_amount;
                int32 active = BG_GetWeaponAimDownSight(weapon);

        if (active == 0) {
            if (Float_NonZeroOrNaN(wd->hipIdleAmount)) {
                amount = wd->hipIdleAmount;
                speed = wd->hipIdleSpeed;
            } else {
                amount = 80.0f;
                speed = 1.0f;
            }
        } else {
            amount = wd->hipIdleAmount + (wd->adsIdleAmount - wd->hipIdleAmount) * ps->fWeaponPosFrac;
            speed = wd->hipIdleSpeed + (wd->adsIdleSpeed - wd->hipIdleSpeed) * ps->fWeaponPosFrac;
        }
        if ((ps->eFlags & 8) == 0) {
            if ((ps->eFlags & 4) == 0) {
                factor = 1.0f;
            } else {
                factor = wd->idleCrouchFactor;
            }
        } else {
            factor = wd->idleProneFactor;
        }
        /* gameplay-fix(wpn_rockets): 0x080f1ede: slots +0x18 (frame seconds) and +0x1c (idle factor) are floats in the original (fld/fstp [vm+0x18], [vm+0x1c] at 0x080f2039..0x080f20d2); the client mirror (gclient 0x287c) is a raw dword, so the int casts here wrote garbage (20d99999) into the factor for scoped weapons */
        float slot18 = Int_Bits_To_Float(vm->slot_18);
        float slot1c = Int_Bits_To_Float(vm->slot_1c);
        float tmp;
        if (Float_NonZeroOrNaN(ps->fWeaponPosFrac) && (factor != slot1c || factor != factor || slot1c != slot1c)) {
            if (factor > slot1c) {
                tmp = (float)((long double)slot18 * (long double)0.5f + (long double)slot1c);
                memcpy(&vm->slot_1c, &tmp, sizeof(tmp));
                if (tmp > factor) {
                    memcpy(&vm->slot_1c, &factor, sizeof(factor));
                }
            } else {
                tmp = (float)((long double)slot1c - (long double)slot18 * (long double)0.5f);
                memcpy(&vm->slot_1c, &tmp, sizeof(tmp));
                if (factor > tmp) {
                    memcpy(&vm->slot_1c, &factor, sizeof(factor));
                }
            }
        }
        slot1c = Int_Bits_To_Float(vm->slot_1c);
        /* three float stores in a row at 0x080f20d5..0x080f20fc: amount*factor, *fWeaponPosFrac, *ps[0x5b4] */
        {
            volatile float a1 = (float)((long double)amount * (long double)slot1c);
            volatile float a2 = (float)((long double)a1 * (long double)ps->fWeaponPosFrac);
            volatile float a3 = (float)((long double)a2 * (long double)ps->holdBreathScale);
            sway_amount = a3;
        }
        /* 0x080f2111..0x080f213d: hb * frame seconds * 1000 * speed in x87 extended, fistp with RC=truncate */
        {
            long double ph = (long double)ps->holdBreathScale * (long double)slot18;
            ph = ph * (long double)1000.0f;
            ph = ph * (long double)speed;
            *vm->slot_20.ptr = (int32)ph + *vm->slot_20.ptr;
        }
        /* 0x080f2144..0x080f21b2: sin(fild phase * float const) as a double; result stored to float; out += amount*sin*0.01f in extended */
        {
            volatile float sn = (float)sin((double)((long double)*vm->slot_20.ptr * (long double)0.0007f));
            out[1] = (float)((long double)out[1] + (long double)sway_amount * (long double)sn * (long double)0.01f);
            sn = (float)sin((double)((long double)*vm->slot_20.ptr * (long double)0.001f));
            out[0] = (float)((long double)out[0] + (long double)sway_amount * (long double)sn * (long double)0.01f);
        }
    }
}

// 0x080f21ba  size=452  FUN_080f21ba  status=translated  naming=inferred
// Evidence: the yaw-bob helper again, using slot_14 as the amplitude and the unconditional Vec3_Scale_080f2a5e by the blend (no reticle gate).
void WeaponView_ApplyBobB(struct view_motion_s *vm, float *out)
{
    struct playerState_s *ps = vm->ps;
    uint32 weapon = BG_GetCurrentWeapon(ps);
    struct weaponDef_s *wd = BG_GetWeaponDef(weapon);

    if (wd->adsOverlayReticle != 0) {
        float phase = (float)(((double)((float)(byte)ps->bobCycle / 255.0)) * 3.1415927);
        float offset[3];
        float phase_sum = phase + phase + 6.2831855f + 0.7853982f + 6.2831855f;
        float amplitude = (float)vm->slot_14 * 0.16f;
        float blend = ps->fWeaponPosFrac;
        float scale;

        offset[0] = -(float)BG_BobSinePair(ps, phase_sum, amplitude, 10.0f);
        offset[1] = -(float)BG_BobSine(ps, phase_sum, amplitude, 10.0f);
        phase_sum = (float)((long double)phase_sum - (long double)0.4712389167638204); /* double at 0x081558c0 (0x080f1589) */
        amplitude = amplitude * 1.5f;
        offset[2] = Float_Min2((float)BG_BobSine(ps, phase_sum, amplitude, 10.0f), 0.0f);
        if (Float_NonZeroOrNaN(blend)) {
            scale = 1.0f - (1.0f - wd->adsBobFactor) * blend;
            offset[0] = offset[0] * scale;
            offset[1] = offset[1] * scale;
            offset[2] = offset[2] * scale;
        }
        Vec3_Scale_080f2a5e(offset, blend, offset);
        Vec3_Add_080f2a1a(out, offset, out);
    }
}

// 0x080f237e  size=330  FUN_080f237e  status=translated  naming=inferred
// Evidence: crouch/prone sway term scaled by stance blend and adsViewBobMult (0x288); slot_14 is passed to the sine helper as raw float bits, unlike the int uses elsewhere (open question).
void WeaponView_ApplyCrouchSway(struct view_motion_s *vm, float *out)
{
    struct playerState_s *ps = vm->ps;
    uint32 weapon = BG_GetCurrentWeapon(ps);
    struct weaponDef_s *wd = BG_GetWeaponDef(weapon);

    if ((ps->eFlags & 0x300) == 0 && Float_NonZeroOrNaN(ps->fWeaponPosFrac)
        && Float_NonZeroOrNaN(wd->adsViewBobMult)) {
        float phase_base = (float)(((double)((float)(byte)ps->bobCycle / 255.0)) * 3.1415927);
        float t;

        phase_base = phase_base + phase_base + 6.2831855f;
        t = BG_BobSinePair(ps, phase_base, Int_Bits_To_Float(vm->slot_14), 45.0f);
        out[0] = out[0] - t * ps->fWeaponPosFrac * wd->adsViewBobMult;
        t = BG_BobSine(ps, phase_base, Int_Bits_To_Float(vm->slot_14), 45.0f);
        out[1] = out[1] - t * ps->fWeaponPosFrac * wd->adsViewBobMult;
    }
}

// 0x080f24c8  size=91  FUN_080f24c8  status=translated  naming=inferred
// Evidence: zeroes the output and accumulates the four view channels in order.
void WeaponView_Accumulate(struct view_motion_s *vm, float *out)
{
    Vec3_Clear_080f29c0(out);
    WeaponView_ApplyTimedKickA(vm, out);
    WeaponView_ApplyIdleSway(vm, out);
    WeaponView_ApplyBobB(vm, out);
    WeaponView_ApplyCrouchSway(vm, out);
}

// 0x080f2524  size=799  FUN_080f2524  status=translated  naming=inferred
// Evidence: two-axis spring toward the angle difference between playerstate view angles (0xe8) and viewAngles; limits from weapon sway fields (0x2e8..0x2fc, ADS 0x304..0x318), approach via Float_ApproachProportional and Angle_ApproachProportionalWrapped.
void WeaponView_ApplyAngleSpring(struct playerState_s *ps, float *viewAngles,
                                 struct view_spring_s *spring, float *angleState, float scale,
                                 int32 ms)
{
    float blend = ps->fWeaponPosFrac;

    if (ms != 0) {
        uint32 weapon = BG_GetCurrentWeapon(ps);
        struct weaponDef_s *wd = BG_GetWeaponDef(weapon);
        float dt = (float)ms * 0.001f;
        float maxAngle;
        float lerpSpeed;
        float pitchScale;
        float yawScale;
        float horizScale;
        float vertScale;
        float ang[3];
        float tmp;

        if (BG_GetWeaponAimDownSight(weapon) == 0) {
            maxAngle = wd->swayMaxAngle;
            lerpSpeed = wd->swayLerpSpeed;
            pitchScale = wd->swayPitchScale;
            yawScale = wd->swayYawScale;
            horizScale = wd->swayHorizScale;
            vertScale = wd->swayVertScale;
        } else {
            if (0.0f < blend && wd->adsOverlayReticle != 0) {
                return;
            }
            maxAngle = (wd->adsSwayMaxAngle - wd->swayMaxAngle) * blend + wd->swayMaxAngle;
            lerpSpeed = (wd->adsSwayLerpSpeed - wd->swayLerpSpeed) * blend + wd->swayLerpSpeed;
            pitchScale = (wd->adsSwayPitchScale - wd->swayPitchScale) * blend + wd->swayPitchScale;
            yawScale = (wd->adsSwayYawScale - wd->swayYawScale) * blend + wd->swayYawScale;
            horizScale = (wd->adsSwayHorizScale - wd->swayHorizScale) * blend + wd->swayHorizScale;
            vertScale = (wd->adsSwayVertScale - wd->swayVertScale) * blend + wd->swayVertScale;
        }
        AnglesSubtract(ps->viewangles, viewAngles, ang);
        Vec3_Scale_080f2a5e(ang, (float)(1.0 / ((double)dt * 60.0)), ang);
        ang[0] = Float_Clamp(ang[0], -maxAngle, maxAngle);
        ang[1] = Float_Clamp(ang[1], -maxAngle, maxAngle);
        tmp = ang[0] * vertScale * scale;
        spring->horiz = Float_ApproachProportional(ang[1] * horizScale * scale, spring->horiz,
                                                   lerpSpeed, dt);
        spring->vert = Float_ApproachProportional(tmp, spring->vert, lerpSpeed, dt);
        ang[0] = ang[0] * pitchScale * scale;
        ang[1] = ang[1] * yawScale * scale;
        angleState[0] = Angle_ApproachProportionalWrapped_F(ang[0], angleState[0], lerpSpeed, dt);
        angleState[1] = Angle_ApproachProportionalWrapped_F(ang[1], angleState[1], lerpSpeed, dt);
        Vec3_Copy_080f29ea(ps->viewangles, viewAngles);
    }
}

// 0x080f2844  size=135  FUN_080f2844  status=translated  naming=observed
// Evidence: string "BG_LoadWeaponDef: Could not find default weapon" (rodata 0x08155960), falls back to "defaultweapon_mp".
struct weaponDef_s *BG_LoadWeaponDef(const char *folder, const char *name)
{
    struct weaponDef_s *wd = NULL;

    if (*name != '\0') {
        wd = BG_LoadWeaponFile(folder, name);
        if (wd == NULL) {
            wd = BG_LoadWeaponFile(folder, "defaultweapon_mp");
            if (wd == NULL) {
                Com_Error(1, "\x15" "BG_LoadWeaponDef: Could not find default weapon");
            }
            WeaponDef_SetString((char **)(void *)wd, name);
        }
    }
    return wd;
}

// 0x080f28cc  size=25  FUN_080f28cc  status=translated  naming=inferred
// Evidence: absolute value of a float.
float Float_Abs_080f28cc(float x)
{
    return (float)__builtin_fabs((double)x); /* fabs (0x080f28d5) */
}

// 0x080f28e6  size=36  FUN_080f28e6  status=translated  naming=inferred
// Evidence: max of two floats via Float_SelectNonNegative(a - b, a, b).
float Float_Max2(float a, float b)
{
    return Float_SelectNonNegative(a - b, a, b);
}

// 0x080f290a  size=36  FUN_080f290a  status=translated  naming=inferred
// Evidence: min of two floats via Float_SelectNonNegative(b - a, a, b).
float Float_Min2(float a, float b)
{
    return Float_SelectNonNegative(b - a, a, b);
}

// 0x080f292e  size=38  FUN_080f292e  status=translated  naming=inferred
// Evidence: integer max via Int_SelectIfNegative(a - b, a, b).
int32 Int_Max2(int32 a, int32 b)
{
    return Int_SelectIfNegative(a - b, a, b);
}

// 0x080f2954  size=61  FUN_080f2954  status=translated  naming=inferred
// Evidence: clamp of x into [lo, hi] (sign-test based, same branch order as the binary).
float Float_Clamp(float x, float lo, float hi)
{
    float t = Float_SelectNonNegative(x - hi, hi, x);
    return Float_SelectNonNegative(lo - x, lo, t);
}

// 0x080f2992  size=11  FUN_080f2992  status=translated  naming=inferred
// Evidence: square of a float in long double precision.
long double Float_SquareLD(float x)
{
    return (long double)x * (long double)x;
}

// 0x080f299e  size=33  FUN_080f299e  status=translated  naming=inferred
// Evidence: squared length of the x,y components of a vector, long double precision.
long double Vec2_LengthSquared(const float *v)
{
    return (long double)v[1] * (long double)v[1] + (long double)v[0] * (long double)v[0];
}

// 0x080f29c0  size=41  FUN_080f29c0  status=translated  naming=inferred
// Evidence: zero a vec3.
void Vec3_Clear_080f29c0(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x080f29ea  size=47  FUN_080f29ea  status=translated  naming=inferred
// Evidence: copy a vec3 (src to dst).
void Vec3_Copy_080f29ea(const float *src, float *dst)
{
    /* integer word copy as in the binary (preserves signalling NaN payloads) */
    ((uint32 *)(void *)dst)[0] = ((const uint32 *)(const void *)src)[0];
    ((uint32 *)(void *)dst)[1] = ((const uint32 *)(const void *)src)[1];
    ((uint32 *)(void *)dst)[2] = ((const uint32 *)(const void *)src)[2];
}

// 0x080f2a1a  size=68  FUN_080f2a1a  status=translated  naming=inferred
// Evidence: out = a + b for vec3.
void Vec3_Add_080f2a1a(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x080f2a5e  size=56  FUN_080f2a5e  status=translated  naming=inferred
// Evidence: out = v s for vec3.
void Vec3_Scale_080f2a5e(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

// 0x080f2a96  size=83  FUN_080f2a96  status=translated  naming=inferred
// Evidence: out = a + s b for vec3.
void Vec3_MultAdd(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x080f2aea  size=41  FUN_080f2aea  status=translated  naming=inferred
// Evidence: returns a if x >= 0.0 else b (the select used by Float_Min2/Max2/Clamp).
float Float_SelectNonNegative(float x, float a, float b)
{
    float selected;

    if (0.0f <= x) {
        selected = a;
    } else {
        selected = b;
    }
    return selected;
}

// 0x080f2b14  size=31  FUN_080f2b14  status=translated  naming=inferred
// Evidence: returns b if x < 0 else a (integer select).
int32 Int_SelectIfNegative(int32 x, int32 a, int32 b)
{
    int32 selected;

    if (x < 0) {
        selected = b;
    } else {
        selected = a;
    }
    return selected;
}

// 0x080f2b34  size=64  FUN_080f2b34  status=translated  naming=inferred
// Evidence: clear one bit in a 32-bit word array.
void BitArray_ClearBit(uint32 *bits, int32 bit)
{
    uint32 *word = bits + (bit >> 5);

    *word = ~((uint32)1 << ((byte)bit & 0x1f)) & *word;
}

// 0x080f2b74  size=15  FUN_080f2b74  status=translated  naming=inferred
// Evidence: index into the 8-entry string table at 0x081869e0 (bullet, grenade, projectile...).
const char *BG_WeaponTypeName(int32 index)
{
    return bg_weaponTypeNames[index];
}

// 0x080f2b84  size=77  FUN_080f2b84  status=translated  naming=inferred
// Evidence: case-insensitive match against the three slot names none/primary/primaryb (table 0x081869f0). Returns 0 when nothing matches, as the binary does.
int32 BG_WeaponSlotFromName(const char *name)
{
    int32 idx = 0;

    for (;;) {
        if (2 < idx) {
            return 0;
        }
        if (strcasecmp(name, bg_weaponSlotNames[idx]) == 0) {
            break;
        }
        idx = idx + 1;
    }
    return idx;
}

// 0x080f2bd2  size=15  FUN_080f2bd2  status=translated  naming=inferred
// Evidence: index into the slot name table at 0x081869f0.
const char *BG_WeaponSlotName(int32 index)
{
    return bg_weaponSlotNames[index];
}

// 0x080f2be2  size=60  FUN_080f2be2  status=translated  naming=inferred
// Evidence: registers each loaded player anim type name through BG_SetPlayerAnimTypeName.
void BG_RegisterAnimTypes(void)
{
    int32 loop_index;

    for (loop_index = 0; loop_index < g_playerAnimTypeCount; loop_index = loop_index + 1) {
        BG_SetPlayerAnimTypeName(loop_index, g_playerAnimTypeStrings[loop_index]);
    }
}

// 0x080f2c1e  size=372  FUN_080f2c1e  status=translated  naming=inferred
// Evidence: loads "mp/playeranimtypes.txt" (rodata 0x0815712e/0x08157147 messages) and parses it with the parse session tag "BG_AnimParseAnimScript"; names are copied into hunk memory.
void BG_AnimLoadPlayerAnimTypes(void)
{
    char buf[4104];
    char *ptr;
    char *tok;
    int32 handle = 0;
    uint32 length;
    int32 signedLength;

    g_playerAnimTypeCount = 0;
    signedLength = FSH_FOpenFile("mp/playeranimtypes.txt", &handle, 0);
    length = (uint32)signedLength;
    if (signedLength < 0) {
        Com_Error(1, "\x15" "Couldn't load file '%s'", "mp/playeranimtypes.txt");
    }
    if (0xfff < length) {
        FS_FCloseFile(handle);
        Com_Error(1, "\x15" "'%s' max size exceeded", "mp/playeranimtypes.txt");
    }
    FS_Read(buf, (int)length, handle);
    buf[length] = 0;
    FS_FCloseFile(handle);
    ptr = buf;
    Com_BeginParseSession("BG_AnimParseAnimScript");
    for (;;) {
        tok = Com_Parse((const char **)&ptr);
        if (tok == NULL || *tok == '\0') {
            break;
        }
        if (0x3f < g_playerAnimTypeCount) {
            Com_Error(1, "\x15" "Player anim type array size exceeded");
        }
        {
            int32 idx = g_playerAnimTypeCount;
            size_t n = strlen(tok);

            g_playerAnimTypeStrings[idx] = (char *)Hunk_AllocLow((int)(n + 1));
            strcpy(g_playerAnimTypeStrings[idx], tok);
        }
        g_playerAnimTypeCount = g_playerAnimTypeCount + 1;
    }
    Com_EndParseSession();
}

// 0x080f2d92  size=85  FUN_080f2d92  status=translated  naming=inferred
// Evidence: case-insensitive index of a name in a string array; -1 when absent.
int32 StrArray_FindCI(const char *s, const char *const *arr, int32 count)
{
    int32 idx = 0;

    for (;;) {
        if (count <= idx) {
            return -1;
        }
        if (strcasecmp(s, arr[idx]) == 0) {
            break;
        }
        idx = idx + 1;
    }
    return idx;
}

// 0x080f310a  size=124  FUN_080f310a  status=translated  naming=inferred
// Evidence: reciprocal ADS transition times (fields adsTransInTime / adsTransOutTime, observed names from the weapon table) with fixed defaults when the time is below 1.
void WeaponDef_ComputeAdsTransInverse(struct weaponDef_s *wd)
{
    if (wd->adsTransInTime < 1) {
        wd->adsTransInTimeInv = Int_Bits_To_Float(0x3b5a740e);
    } else {
        wd->adsTransInTimeInv = (float)(1.0 / (double)wd->adsTransInTime);
    }
    if (wd->adsTransOutTime < 1) {
        wd->adsTransOutTimeInv = Int_Bits_To_Float(0x3b03126f);
    } else {
        wd->adsTransOutTimeInv = (float)(1.0 / (double)wd->adsTransOutTime);
    }
}

// 0x080f3186  size=83  FUN_080f3186  status=translated  naming=inferred
// Evidence: default damage range values when the stored range is not positive.
void WeaponDef_SetDefaultDamageRange(struct weaponDef_s *wd)
{
    if (wd->maxDamageRange <= 0.0f) {
        wd->maxDamageRange = Int_Bits_To_Float(0x497423f0);
    }
    if (wd->minDamageRange <= 0.0f) {
        wd->minDamageRange = Int_Bits_To_Float(0x497423f2);
    }
}

// 0x080f31da  size=232  FUN_080f31da  status=translated  naming=inferred
// Evidence: warning strings "Projectile speed for WeapType %s ...", "Destabilization ...".
void WeaponDef_ValidateDestabilization(struct weaponDef_s *wd)
{
    if (wd->projectileSpeed <= 0) {
        Com_Error(1, "\x15" "Projectile speed for WeapType %s must be greater than 0.0",
                  wd->displayName);
    }
    if (45.0 <= wd->destabilizationAngleMax || wd->destabilizationAngleMax < 0.0f) {
        Com_Error(1, "\x15" "Destabilization angle for for WeapType %s must be between 0 and 45 degrees",
                  wd->displayName);
    }
    if (wd->destabilizationBaseTime <= 0.0f) {
        Com_Error(1, "\x15" "Destabilization base time for for WeapType %s must be positive",
                  wd->displayName);
    }
    if (wd->destabilizationTimeReductionRatio <= 0.0f) {
        Com_Error(1, "\x15" "Destabilization reduction ratio for for WeapType %s must be positive",
                  wd->displayName);
    }
}

// 0x080f32c2  size=84  FUN_080f32c2  status=translated  naming=inferred
// Evidence: stores a copy of a string in hunk memory; an empty string maps to "".
void WeaponDef_SetString(char **dst, const char *value)
{
    if (*value == '\0') {
        *dst = (char *)"";
    } else {
        size_t n = strlen(value);
        char *copy = (char *)Hunk_AllocLowAlign(n + 1, 1);

        strcpy(copy, value);
        *dst = copy;
    }
}

// 0x080f3316  size=26  FUN_080f3316  status=translated  naming=inferred
// Evidence: field-parser callback that forwards to WeaponDef_SetString.
void WeaponDef_SetStringCallback(char **dst, const char *value)
{
    WeaponDef_SetString(dst, value);
}

// 0x080f3330  size=82  FUN_080f3330  status=translated  naming=inferred
// Evidence: name is reset to "" and every string-typed field of the 366-entry table (type 0) is reset to "".
void WeaponDef_SetDefaults(struct weaponDef_s *wd)
{
    int32 i;

    wd->szInternalName = (char *)"";
    for (i = 0; i < 0x16e; i = i + 1) {
        if (bg_weaponFieldTable[i].type == 0) {
            *(const char **)((byte *)wd + bg_weaponFieldTable[i].offset) = "";
        }
    }
}

// 0x080f3382  size=848  FUN_080f3382  status=translated  naming=inferred
// Evidence: "weapons/%s/%s", magic "WEAPONFILE", warnings "Could not load weapon file", "is not a valid weapon file", "Is too long of a weapon file to parse". Field parse via Com_ParseInfoFields over the 366-entry table; ammo/clip names lowercased.
struct weaponDef_s *BG_LoadWeaponFile(const char *folder, const char *name)
{
    struct weaponDef_s *wd = (struct weaponDef_s *)Hunk_AllocLow(0x604);
    char path[0x40];
    char buf[0x2000];
    int32 handle = 0;
    int32 length;

    WeaponDef_SetDefaults(wd);
    Com_sprintf(path, 0x40, "weapons/%s/%s", folder, name);
    length = FSH_FOpenFile(path, &handle, 0);
    if (length < 0) {
        Com_Printf("^3WARNING: Could not load weapon file '%s'\n", path);
        return NULL;
    }
    FS_Read(buf, 10, handle);
    buf[10] = '\0';
    if (strncmp(buf, "WEAPONFILE", 10) != 0) {
        Com_Printf("^3WARNING: \"%s\" does not appear to be a weapon file\n", path);
        FS_FCloseFile(handle);
        return NULL;
    }
    if (length - 10 >= 0x2000) {
        Com_Printf("^3WARNING: \"%s\" Is too long of a weapon file to parse\n", path);
        FS_FCloseFile(handle);
        return NULL;
    }
    memset(buf, 0, 0x2000);
    FS_Read(buf, length - 10, handle);
    buf[length - 10] = '\0';
    FS_FCloseFile(handle);
    if (Info_Validate(buf) == 0) {
        Com_Printf("^3WARNING: \"%s\" is not a valid weapon file\n", path);
        return NULL;
    }
    WeaponDef_SetString((char **)(void *)wd, name);
    if (Com_ParseInfoFields((char *)wd, (struct InfoField *)&bg_weaponFieldTable[0], 0x16e, buf, 0x10,
                            (int (*)(char *, const char *, int))g_080f2de8,
                            (void (*)(void *, const char *))WeaponDef_SetStringCallback) == 0) {
        return NULL;
    }
    WeaponDef_ComputeAdsTransInverse(wd);
    WeaponDef_SetDefaultDamageRange(wd);
    if (15000.0f < wd->enemyCrosshairRange) {
        Com_Error(1, "\x15" "Enemy crosshair ranges should be less than %f ", 15000.0);
    }
    if (wd->weaponType == 2) {
        WeaponDef_ValidateDestabilization(wd);
    }
    if (WeaponDef_LoadAccuracyGraphs(wd) == 0) {
        return NULL;
    }
    Q_strlwr((char *)wd->ammoName);
    Q_strlwr((char *)wd->clipName);
    return wd;
}

// 0x080f36d2  size=75  FUN_080f36d2  status=translated  naming=inferred
// Evidence: builds the global "none" weapon with accuracy files "noweapon.accu".
struct weaponDef_s *BG_GetNoWeaponDef(void)
{
    WeaponDef_SetDefaults(&bg_noWeaponDef);
    bg_noWeaponDef.szInternalName = (char *)"none";
    bg_noWeaponDef.aiVsAiAccuracyGraph = (char *)"noweapon.accu";
    bg_noWeaponDef.aiVsPlayerAccuracyGraph = (char *)"noweapon.accu";
    WeaponDef_LoadAccuracyGraphs(&bg_noWeaponDef);
    return &bg_noWeaponDef;
}

// 0x080f3720  size=25  FUN_080f3720  status=translated  naming=inferred
// Evidence: clears the two trace-related globals.
void Trace_ResetGlobals(void)
{
    g_08628178 = 0;
    g_08628580 = 0;
}

// 0x080f373a  size=5  FUN_080f373a  status=translated  naming=unknown
// Evidence: the binary body is a bare return; kept as a no-op for address-compatible callers.
void Noop_Callback(void)
{
}

// 0x080f3740  size=264  FUN_080f3740  status=translated  naming=inferred
// Evidence: expands box bounds by the trace offset from VectorLerp; min/max selects per axis.
void Trace_ComputeSweepBounds(const float *start, const float *mins, const float *maxs,
                              const float *p4, float fraction, float *out6)
{
    float local[3];

    VectorLerp(start, p4, fraction, local);
    out6[0] = Float_MinB(start[0], local[0]) + mins[0];
    out6[1] = Float_MinB(start[1], local[1]) + mins[1];
    out6[2] = Float_MinB(start[2], local[2]) + mins[2];
    out6[3] = Float_MaxB(start[0], local[0]) + maxs[0];
    out6[4] = Float_MaxB(start[1], local[1]) + maxs[1];
    out6[5] = Float_MaxB(start[2], local[2]) + maxs[2];
}

/* Shared tail of the entity hit test (the LAB_080f3bbc path): runs the box trace against one
 * entity, keeps the nearest hit, and returns 1 when the trace is start-solid. */
static int32 Trace_TestEntity(const float *start, const float *mins, const float *maxs, const float *p4,
                              int32 owner, uint32 mask, float *box, struct gentity_s *ent,
                              int32 model, const float *boxB, struct trace_s *out)
{
    struct trace_s tr;

    (void)mask;
    Sweep_TraceBoxWithOffsetInit(&tr, start, p4, mins, maxs, model, mask, ENT_BOXA(ent), (float *)boxB);
    if (tr.fraction < out->fraction) {
        tr.entityNum = (uint16)owner;
        *out = tr;
        Trace_ComputeSweepBounds(start, mins, maxs, p4, tr.fraction, box);
    } else if (tr.allsolid != 0) {
        tr.entityNum = (uint16)owner;
        *out = tr;
    } else if (tr.startsolid != 0) {
        out->startsolid = 1;
    }
    return out->allsolid != 0;
}

// 0x080f3848  size=1206  FUN_080f3848  status=translated  naming=inferred
// Evidence: sweeps the linked entity list (g_08628180) against the swept box; entities with box_mode 0xffffff use their collision model, others use packed bounds in box_mode bytes. The unused parameter is passed as 1 by Trace_BoxWorldThenEntities.
void Trace_SweepEntities(const float *start, const float *mins, const float *maxs, const float *p4,
                         int32 owner, uint32 mask, uint32 unused, struct trace_s *out)
{
    float box[6];
    int32 i;

    (void)unused;
    Trace_ComputeSweepBounds(start, mins, maxs, p4, out->fraction, box);
    for (i = 0;; i = i + 1) {
        struct gentity_s *ent;

        if (g_08628178 <= i) {
            return;
        }
        ent = g_08628180[i];
        if (ENT_RAW_I32(ent, 0xf0) == owner) {
            continue;
        }
        if (ENT_RAW_I32(ent, 0x18c) == 0xffffff) {
            int32 model = ent->freeAfterEvent;
            uint32 cmask = cm_model_surface_mask(model);

            if ((mask & cmask) != 0) {
                float radius = cm_model_radius(model);

                if (((ENT_BOXA(ent)[0] - radius < box[3]) && (ENT_BOXA(ent)[1] - radius < box[4]))
                    && (box[0] < ENT_BOXA(ent)[0] + radius)
                    && (box[1] < ENT_BOXA(ent)[1] + radius)
                    && (ENT_BOXA(ent)[2] - radius < box[5])
                    && (box[2] < ENT_BOXA(ent)[2] + radius)) {
                    float boxB[3];

                    Vec3_Copy_080f29ea(ENT_BOXB(ent), boxB);
                    if (Trace_TestEntity(start, mins, maxs, p4, owner, mask, box, ent, model,
                                         boxB, out)) {
                        return;
                    }
                }
            }
        } else {
            uint32 cmask = (ent->r.clientMask[0] == 1) ? 0x2000000u : 1u;

            if ((mask & cmask) != 0) {
                int32 packed = ENT_RAW_I32(ent, 0x18c);
                int32 l14 = packed & 0xff;
                int32 l18 = ((packed >> 8) & 0xff) - 1;
                int32 l1c = ((packed >> 16) & 0xff) - 0x20;
                float mins3[3];
                float maxs3[3];
                float zero[3] = { 0.0f, 0.0f, 0.0f };
                int32 model;

                mins3[0] = 1.0f - (float)l14;
                mins3[1] = mins3[0];
                mins3[2] = 1.0f - (float)l18;
                maxs3[0] = (float)l14 - 1.0f;
                maxs3[1] = maxs3[0];
                maxs3[2] = (float)l1c - 1.0f;
                if (((ENT_BOXA(ent)[0] + mins3[0] < box[3]) && (ENT_BOXA(ent)[1] + mins3[1] < box[4]))
                    && (box[0] < ENT_BOXA(ent)[0] + maxs3[0])
                    && (box[1] < ENT_BOXA(ent)[1] + maxs3[1])
                    && (ENT_BOXA(ent)[2] + mins3[2] < box[5])
                    && (box[2] < ENT_BOXA(ent)[2] + maxs3[2])) {
                    model = CM_TempBoxModel(mins3, maxs3, cmask);
                    if (Trace_TestEntity(start, mins, maxs, p4, owner, mask, box, ent, model,
                                         zero, out)) {
                        return;
                    }
                }
            }
        }
    }
}

// 0x080f3cfe  size=194  FUN_080f3cfe  status=translated  naming=inferred
// Evidence: box trace against world (CM_BoxTrace), then an entity sweep when the fraction is non-zero. The entity word of the result gets 0x3fe or 0x3ff (fraction != 1 or == 1).
void Trace_BoxWorldThenEntities(struct trace_s *trace, const float *start, const float *mins,
                                const float *maxs, const float *p5, int32 owner, uint32 mask)
{
    uint16 entity_word;

    CM_BoxTrace(trace, start, p5, mins, maxs, 0, mask);
    if (trace->fraction != 1.0f || trace->fraction != trace->fraction) {
        entity_word = 0x3fe;
    } else {
        entity_word = 0x3ff;
    }
    trace->entityNum = entity_word;
    if (trace->fraction != 0.0f || trace->fraction != trace->fraction) {
        Trace_SweepEntities(start, mins, maxs, p5, owner, mask, 1, trace);
    }
}

// 0x080f3dc0  size=191  FUN_080f3dc0  status=translated  naming=inferred
// Evidence: contents query: world contents at a point, OR-ed with box entities (other than the owner), masked by the requested mask.
uint32 Trace_ContentsWithEntities(const float *point, int32 owner, uint32 mask)
{
    uint32 result = CM_PointContents((float *)point, 0);
    int32 i;

    for (i = 0; i < g_08628178; i = i + 1) {
        struct gentity_s *ent = g_08628180[i];

        if (ENT_RAW_I32(ent, 0xf0) != owner && ENT_RAW_I32(ent, 0x18c) == 0xffffff && ent->freeAfterEvent != 0) {
            result = result | CM_TransformedPointContents(point, ent->freeAfterEvent, ENT_BOXA(ent), ENT_BOXB(ent));
        }
    }
    return mask & result;
}

// 0x080f3e80  size=36  FUN_080f3e80  status=translated  naming=inferred
// Evidence: max of two floats via Float_SelectNonNegativeB(a - b, a, b).
float Float_MaxB(float a, float b)
{
    return Float_SelectNonNegativeB(a - b, a, b);
}

// 0x080f3ea4  size=36  FUN_080f3ea4  status=translated  naming=inferred
// Evidence: min of two floats via Float_SelectNonNegativeB(b - a, a, b).
float Float_MinB(float a, float b)
{
    return Float_SelectNonNegativeB(b - a, a, b);
}

// 0x080f3ec8  size=41  FUN_080f3ec8  status=translated  naming=inferred
// Evidence: zero a vec3 (second copy used by the trace path).
void Vec3_ClearB(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x080f3ef2  size=47  FUN_080f3ef2  status=translated  naming=inferred
// Evidence: copy a vec3 (second copy used by the trace path).
void Vec3_CopyBits_080f3ef2(const float *src, float *dst)
{
    /* integer word copy as in the binary (preserves signalling NaN payloads) */
    ((uint32 *)(void *)dst)[0] = ((const uint32 *)(const void *)src)[0];
    ((uint32 *)(void *)dst)[1] = ((const uint32 *)(const void *)src)[1];
    ((uint32 *)(void *)dst)[2] = ((const uint32 *)(const void *)src)[2];
}

// 0x080f3f22  size=41  FUN_080f3f22  status=translated  naming=inferred
// Evidence: same select as Float_SelectNonNegative (x >= 0 ? a : b).
float Float_SelectNonNegativeB(float x, float a, float b)
{
    float selected;

    if (0.0f <= x) {
        selected = a;
    } else {
        selected = b;
    }
    return selected;
}

// 0x080f3f4c  size=133  FUN_080f3f4c  status=translated  naming=inferred
// Evidence: string pool append for the key/value table; guard message "G_AddSpawnVarToken: MAX_SPAWN_VARS" (rodata 0x081576a0).
char *KeyValue_PoolAppend(struct kv_table_s *t, const char *s)
{
    size_t len = strlen(s);
    char *dest;

    if (0x800 < (int)(len + (size_t)t->pool_used + 1)) {
        Com_Error(1, "\x15" "G_AddSpawnVarToken: MAX_SPAWN_VARS");
    }
    dest = t->pool + t->pool_used;
    memcpy(dest, s, len + 1);
    t->pool_used = (int32)(len + (size_t)t->pool_used + 1);
    return dest;
}

// 0x080f3fd2  size=395  FUN_080f3fd2  status=translated  naming=inferred
// Evidence: "{ key value ... }" block parser (messages "G_ParseSpawnVars: ..." at rodata 0x081576e0..0x081577a0). Stores up to 64 pairs in the table.
int32 KeyValue_ParseBlock(struct kv_table_s *t)
{
    char value[1024];
    char key[1024];

    t->parsed = 0;
    t->count = 0;
    t->pool_used = 0;
    if (SV_GetEntityToken(value, 0x400) == 0) {
        return 0;
    }
    if (value[0] != '{') {
        Com_Error(1, "\x15" "G_ParseSpawnVars: found %s when expecting {", value);
    }
    for (;;) {
        int32 idx;

        if (SV_GetEntityToken(key, 0x400) == 0) {
            Com_Error(1, "\x15" "G_ParseSpawnVars: EOF without closing brace");
        }
        if (key[0] == '}') {
            break;
        }
        if (SV_GetEntityToken(value, 0x400) == 0) {
            Com_Error(1, "\x15" "G_ParseSpawnVars: EOF without closing brace");
        }
        if (value[0] == '}') {
            Com_Error(1, "\x15" "G_ParseSpawnVars: closing brace without data");
        }
        if (t->count == 0x40) {
            Com_Error(1, "\x15" "G_ParseSpawnVars: MAX_SPAWN_VARS");
        }
        idx = t->count;
        t->pairs[idx].key = KeyValue_PoolAppend(t, key);
        idx = t->count;
        t->pairs[idx].value = KeyValue_PoolAppend(t, value);
        t->count = t->count + 1;
    }
    t->parsed = 1;
    return 1;
}

// 0x080f415e  size=106  FUN_080f415e  status=translated  naming=inferred
// Evidence: linear key lookup in the pair table, falling back to the default value.
int32 KeyValue_Lookup(const struct kv_table_s *t, const char *key, const char *def,
                      const char **out)
{
    int32 idx = 0;

    for (;;) {
        if (t->count <= idx) {
            *out = def;
            return 0;
        }
        if (strcmp(key, t->pairs[idx].key) == 0) {
            break;
        }
        idx = idx + 1;
    }
    *out = t->pairs[idx].value;
    return 1;
}

// 0x080f41c8  size=274  FUN_080f41c8  status=translated  naming=inferred
// Evidence: unescapes backslash sequences ("\n" becomes a newline) and emits the result via ScrStr_InternCString; length guard message "G_NewString: len = %i > %i" (rodata 0x081577c2).
uint32 Print_UnescapeAndEmit(const char *src)
{
    size_t src_len = strlen(src);
    uint32 src_len_nul = (uint32)(src_len + 1);
    char buf[16392];
    char *dst = buf;
    int32 i;

    if (0x4000 < src_len_nul) {
        Com_Error(1, "\x15" "G_NewString: len = %i > %i\n", src_len_nul, 0x4000);
    }
    for (i = 0; i < (int32)src_len_nul; i = i + 1) {
        if (src[i] == '\\' && i < (int32)src_len) {
            i = i + 1;
            if (src[i] == 'n') {
                *dst = '\n';
            } else {
                *dst = '\\';
            }
        } else {
            *dst = src[i];
        }
        dst = dst + 1;
    }
    return ScrStr_InternCString(buf, 0); /* script string id: eax of the tail call (0x080f42c2..0x080f42d9) */
}

// 0x080f42da  size=156  FUN_080f42da  status=translated  naming=inferred
// Evidence: formats an integer vec3 "(%i %i %i)" into an 8-slot ring of 32-byte strings.
const char *Vec3_FormatRing(const float *v)
{
    int32 idx = g_086289a4 * 0x20;

    g_086289a4 = (g_086289a4 + 1) & 7;
    Com_sprintf(g_086289c0 + idx, 0x20, "(%i %i %i)", Round_To_Int((double)v[0]),
                Round_To_Int((double)v[1]), Round_To_Int((double)v[2]));
    return g_086289c0 + idx;
}

// 0x080f4378  size=278  FUN_080f4378  status=translated  naming=inferred
// Evidence: parses a knot count followed by (x y) pairs; warnings "has too many graph knots" and "Error in parsing an ai weapon accuracy file". Maximum 16 knots.
int32 WeaponAccuracy_ParseGraph(const char *data, const char *name,
                                struct accuracy_knot_s *knots, int32 *count_out)
{
    char *tok;
    int32 knot;

    Com_BeginParseSession(name);
    tok = Com_Parse(&data);
    *count_out = atoi(tok);
    knot = 0;
    for (;;) {
        tok = Com_Parse(&data);
        if (!(*tok != '\0' && *tok != '}')) {
            break;
        }
        knots[knot].x = (float)atof(tok);
        tok = Com_Parse(&data);
        if (*tok == '\0' || *tok == '}') {
            break;
        }
        knots[knot].y = (float)atof(tok);
        knot = knot + 1;
        if (0xf < knot) {
            Com_Printf("^3WARNING: \"%s\" has too many graph knots\n", name);
            Com_EndParseSession();
            return 0;
        }
    }
    Com_EndParseSession();
    if (knot == *count_out) {
        return 1;
    }
    Com_Printf("^3WARNING: \"%s\" Error in parsing an ai weapon accuracy file\n", name);
    return 0;
}

// 0x080f448e  size=580  FUN_080f448e  status=translated  naming=inferred
// Evidence: "WEAPONACCUFILE" magic, "accuracy/%s/%s" path, warnings "Could not load ai weapon accuracy file" and "Is too long of an ai weapon accuracy file". Only weapon types 0 and 2.
int32 WeaponAccuracy_LoadFile(struct weaponDef_s *wd, const char *folder, const char *name,
                              struct accuracy_knot_s *knots, int32 *count_out)
{
    const char *magic = "WEAPONACCUFILE";
    size_t magicLen = strlen(magic);
    char path[72];
    char buf[0x2000];
    int32 handle = 0;
    int32 length;
    int32 count;

    if (wd->weaponType == 0 || wd->weaponType == 2) {
        if (*name == '\0') {
            return 1;
        }
        sprintf(path, "accuracy/%s/%s", folder, name);
        length = FSH_FOpenFile(path, &handle, 0);
        if (length < 0) {
            Com_Printf("^3WARNING: Could not load ai weapon accuracy file '%s'\n", path);
            return 0;
        }
        FS_Read(buf, (int32)magicLen, handle);
        buf[magicLen] = '\0';
        if (strncmp(buf, magic, magicLen) != 0) {
            Com_Printf("^3WARNING: \"%s\" does not appear to be an ai weapon accuracy file\n", path);
            FS_FCloseFile(handle);
            return 0;
        }
        if (length - (int32)magicLen >= 0x2000) {
            Com_Printf("^3WARNING: \"%s\" Is too long of an ai weapon accuracy file to parse\n", path);
            FS_FCloseFile(handle);
            return 0;
        }
        memset(buf, 0, 0x2000);
        FS_Read(buf, length - (int32)magicLen, handle);
        buf[length - (int32)magicLen] = '\0';
        FS_FCloseFile(handle);
        count = 0;
        if (WeaponAccuracy_ParseGraph(buf, path, knots, &count) == 0) {
            return 0;
        }
        *count_out = count;
        return 1;
    }
    return 1;
}

// 0x080f46d2  size=329  FUN_080f46d2  status=translated  naming=inferred
// Evidence: loads the two AI accuracy graphs ("aivsai", "aivsplayer" folders, names in weapon fields 0x50c/0x510); stores hunk copies of the knots and their counts.
int32 WeaponDef_LoadAccuracyGraphs(struct weaponDef_s *wd)
{
    int32 i = 0;

    for (;;) {
        byte knots[0x80];
        int32 count = 0;
        const char *graphName;

        if (1 < i) {
            return 1;
        }
        memset(knots, 0, 0x80);
        graphName = (i == 0) ? wd->aiVsAiAccuracyGraph : wd->aiVsPlayerAccuracyGraph;
        if (WeaponAccuracy_LoadFile(wd, bg_accuracyFolderNames[i], graphName,
                                    (struct accuracy_knot_s *)(void *)knots, &count) == 0) {
            return 0;
        }
        if (0 < count) {
            size_t n = (size_t)count << 3;
            void *copy = Hunk_AllocLowAlign(n, 4);

            wd->accuracy_knots[i] = (struct accuracy_knot_s *)copy;
            memcpy(copy, knots, n);
            wd->accuracy_knot_count[i] = count;
        }
        i = i + 1;
    }
}

// 0x080f481c  size=648  FUN_080f481c  status=translated  naming=inferred
// Evidence: recoil kick from the shot sample (0x27e0 * 100 / 0x2728, capped at 127), converting the recoil angles to axes with AnglesToAxis; sets 256ths-of-turn view deltas (0x120/0x124).
void Client_ApplyRecoilKick(struct gentity_s *ent)
{
    struct gclient_s *cl = ent->client;

    if (cl->ps.pm_type < 6) {
        cl->ps.damageCount = 0;
        if (0 < cl->damageTaken && 0 < cl->maxHealth) {
            int32 recoil_level = (cl->damageTaken * 100) / cl->maxHealth;
            float recoil_scale;
            float ang[3];
            float axis[12];
            float *axis_fwd = axis;
            float *axis_right = axis + 3;

            if (0x7f < recoil_level) {
                recoil_level = 0x7f;
            }
            cl->ps.aimSpreadScale = cl->ps.aimSpreadScale + (float)recoil_level;
            if (255.0f < cl->ps.aimSpreadScale) {
                cl->ps.aimSpreadScale = 255.0f;
            }
            recoil_scale = (float)recoil_level * 0.2f;
            if (recoil_scale < 5.0f) {
                recoil_scale = 5.0f;
            } else if (90.0f < recoil_scale) {
                recoil_scale = 90.0f;
            }
            if (cl->damageDirIsSelf == 0) {
                float neg_scale;
                float axis_dot;

                Vec3_ToAngles(cl->damageDir, ang);
                AnglesToAxis(cl->ps.viewangles, axis_fwd);
                neg_scale = -recoil_scale;
                axis_dot = DotProduct_080f7d3a(cl->damageDir, axis_right);
                cl->recoilPitchOut = (float)(axis_dot * neg_scale);
                axis_dot = DotProduct_080f7d3a(cl->damageDir, axis_fwd);
                cl->recoilPitchNeg = (float)((double)recoil_scale * (double)axis_dot);
                cl->ps.damagePitch = Round_To_Int((ang[0] / 360.0) * 256.0);
                cl->ps.damageYaw = Round_To_Int((ang[1] / 360.0) * 256.0);
            } else {
                cl->recoilPitchOut = 0.0f;
                cl->recoilPitchNeg = -recoil_scale;
                cl->ps.damagePitch = 0xff;
                cl->ps.damageYaw = 0xff;
                cl->damageDirIsSelf = 0;
            }
            cl->ps.damageEvent = cl->ps.damageEvent + 1;
            cl->recoilStamp = g_0864c56c - 20;
            cl->ps.damageCount = recoil_level;
            cl->damageTaken = 0;
        }
    }
}

// 0x080f4aa4  size=18  FUN_080f4aa4  status=translated  naming=unknown
// Evidence: clears the int at +0x84 of the object (field not otherwise named in this chunk).
void Entity_ClearField84(void *ent)
{
    *(int32 *)((byte *)ent + 0x84) = 0;
}

// 0x080f4ab6  size=355  FUN_080f4ab6  status=translated  naming=inferred
// Evidence: touch dispatch over a list of entity numbers (deduplicated); Scr-gated event pair (0811b150/0811b2de) and the per-class touch callbacks (self, other, 1).
void Entity_TouchList(struct gentity_s *self, struct pmove_s *pm)
{
    gentity_touch_fn selfTouch = g_08187d0c[self->handler].touch;
    int32 *nums = pm->touchents;
    int32 c;

    for (c = 0; c < pm->numtouch; c = c + 1) {
        int32 k;

        for (k = 0; k < c && nums[k] != nums[c]; k = k + 1) {
        }
        if (k == c) {
            struct gentity_s *other = &g_gentities[nums[c]];

            if (Scr_IsInitialized(1)) {
                Scr_AddEntityParam(other);
                Scr_NotifyEntity(self, scrStr_touch, 1);
                Scr_AddEntityParam(self);
                Scr_NotifyEntity(other, scrStr_touch, 1);
            }
            if (g_08187d0c[other->handler].touch != NULL) {
                g_08187d0c[other->handler].touch(other, self, 1);
            }
            if (selfTouch != NULL) {
                selfTouch(self, other, 1);
            }
        }
    }
}

// 0x080f4c1a  size=664  FUN_080f4c1a  status=translated  naming=inferred
// Evidence: Quake-III-style trigger touch for linked entities: box query around the client, per-entity touch callbacks (other, self, 1) and self touch, gated by client state < 2.
void Entity_TouchLinked(struct gentity_s *self)
{
    struct gclient_s *cl = self->client;

    if (cl->ps.pm_type < 2) {
        float mins[7];
        float maxs[4];
        int32 list[1026];
        int32 count;
        gentity_touch_fn selfTouch = g_08187d0c[self->handler].touch;
        int32 k;

        VectorSubtract_080f7cf6(cl->ps.origin, (const float *)g_08187b94, mins);
        VectorAdd_080f7cb2(cl->ps.origin, (const float *)g_08187b94, maxs);
        count = CM_AreaEntities(mins, maxs, list, 0x400, 0x405c0008u);
        VectorAdd_080f7cb2(cl->ps.origin, self->r.mins, mins);
        VectorAdd_080f7cb2(cl->ps.origin, self->r.maxs, maxs);
        Vec3Pair_PullXYToZ(mins, maxs);
        for (k = 0; k < count; k = k + 1) {
            struct gentity_s *other = &g_gentities[list[k]];
            gentity_touch_fn otherTouch = g_08187d0c[other->handler].touch;

            if (otherTouch != NULL || selfTouch != NULL) {
                int32 hit;

                if (other->s.eType == 3) {
                    hit = BG_PlayerNearEntityTrajectory(&cl->ps, other, g_0864c56c);
                } else {
                    hit = SV_EntityContact(mins, maxs, other);
                }
                if (hit != 0) {
                    if (Scr_IsInitialized(1)) {
                        Scr_AddEntityParam(self);
                        Scr_NotifyEntity(other, scrStr_touch, 1);
                        Scr_AddEntityParam(other);
                        Scr_NotifyEntity(self, scrStr_touch, 1);
                    }
                    if (otherTouch != NULL) {
                        otherTouch(other, self, 1);
                    }
                }
            }
        }
    }
}

// 0x080f4eb2  size=620  FUN_080f4eb2  status=translated  naming=inferred
// Evidence: button edge bookkeeping (0x26cc/0x27bc/0x27c0/0x27c8), a deferred-drop check (Client_SpectateBitClear), the player move hook with a 0xf8-byte context, and SV_UnlinkEntity.
void Client_ProcessButtonsAndDrop(struct gentity_s *ent, const struct usercmd_s *cmd)
{
    struct gclient_s *cl = ent->client;
    pmove_t ctx;
    int32 i;

    cl->oldbuttons = cl->buttons;
    cl->buttons = cl->cmd.buttons;
    cl->buttonsSinceLastFrame = (~cl->oldbuttons & cl->buttons) | cl->buttonsSinceLastFrame;
    if (cl->spectatorClient < 0 && Client_SpectateBitClear(cl, 4) != 0 && -1 < cl->lastFollowedClient
        && (cl->buttons & 4) != (cl->oldbuttons & 4)) {
        StopFollowing(ent);
    }
    if ((cl->buttons & 1) == 0 || (cl->oldbuttons & 1) != 0) {
        if ((cl->buttons & 0x1000) != 0 && (cl->oldbuttons & 0x1000) == 0) {
            Cmd_FollowCycle_f(ent, -1);
        }
    } else {
        Cmd_FollowCycle_f(ent, 1);
    }
    if ((cl->ps.pm_flags & 0x400000) == 0) {
        int32 deferred;

        cl->ps.pm_type = 4;
        deferred = Client_SpectateBitClear(cl, 4);
        cl->ps.speed = (deferred == 0) ? 0 : 400;
        memset(&ctx, 0, sizeof(ctx));
        ctx.cmd = *cmd;
        ctx.ps = &cl->ps;
        ctx.tracemask = 0x800811;
        ctx.handler = 1;
        PM_Pmove(&ctx);
        VectorCopy_080f7c82(cl->ps.origin, ent->r.currentOrigin);
        SV_UnlinkEntity(ent);
    }
}

// 0x080f511e  size=343  FUN_080f511e  status=translated  naming=inferred
// Evidence: inactivity drop: warning text "GAME_INACTIVEDROPWARNING" and drop text "GAME_DROPPEDFORINACTIVITY" (rodata); the timeout comes from the cvar block at g_087a2428.
int32 Client_CheckInactivity(struct gclient_s *cl)
{
    if (g_087a2428->current.integer == 0) {
        cl->inactivityTime = g_0864c56c + 60000;
        cl->inactivityWarning = 0;
    } else if (cl->cmd.forwardmove == 0 && cl->cmd.rightmove == 0 && (cl->cmd.buttons & 0x401) == 0) {
        if (cl->localClient == 0) {
            if (cl->inactivityTime < g_0864c56c) {
                SV_DropClientNumChecked(Client_Number(cl), "GAME_DROPPEDFORINACTIVITY");
                return 0;
            }
            if (cl->inactivityTime - 10000 < g_0864c56c && cl->inactivityWarning == 0) {
                char *msg;

                cl->inactivityWarning = 1;
                msg = va("%c \"GAME_INACTIVEDROPWARNING\"", 99);
                SV_SendServerCommandNum(Client_Number(cl), 0, msg);
            }
        }
    } else {
        cl->inactivityTime = g_087a2428->current.integer * 1000 + g_0864c56c;
        cl->inactivityWarning = 0;
    }
    return 1;
}

// 0x080f5276  size=99  FUN_080f5276  status=translated  naming=inferred
// Evidence: shifts the button history and updates the edge mask (same as the first lines of Client_ProcessButtonsAndDrop).
void Client_UpdateButtonEdges(struct gentity_s *ent, struct usercmd_s *cmd)
{
    struct gclient_s *cl = ent->client;

    (void)cmd; /* second argument is passed by the caller but not read (0x080f5276 reads only [ebp+8]) */
    cl->oldbuttons = cl->buttons;
    cl->buttons = cl->cmd.buttons;
    cl->buttonsSinceLastFrame = (~cl->oldbuttons & cl->buttons) | cl->buttonsSinceLastFrame;
}

// 0x080f52da  size=825  FUN_080f52da  status=translated  naming=inferred
// Evidence: dispatches the player event ring (4 entries; ids 0x74..0x8a are timed, others are switch cases) to the server event helpers.
void Client_DispatchEvents(struct gentity_s *ent, uint32 start)
{
    struct gclient_s *cl = ent->client;
    int32 idx = (int32)start;

    if (idx < cl->ps.eventSequence - 4) {
        idx = cl->ps.eventSequence - 4;
    }
    for (;;) {
        int32 ev;
        int32 parm;

        if (cl->ps.eventSequence <= idx) {
            return;
        }
        ev = cl->ps.events[idx & 3];
        parm = cl->ps.eventParms[idx & 3];
        if (ev < 0x74 || 0x8a < ev) {
            switch (ev) {
            case 0x9e:
            case 0x9f:
            case 0xa0:
            case 0xaf:
                if (cv_g_antilag->current.enabled == 0) {
                    G_FireWeaponByMode(ent, g_0864c56c);
                } else {
                    G_FireWeaponByMode(ent, cl->lastActivityTime);
                }
                break;
            case 0xa4:
                G_UpdateWeaponTargetLock(ent);
                break;
            case 0xa6:
                G_FireMissileAtTarget(ent);
                break;
            case 0xa8:
                Scr_NotifyEntity(ent, scrStr_binocular_enter, 0);
                break;
            case 0xa9:
                Scr_NotifyEntity(ent, scrStr_binocular_exit, 0);
                break;
            case 0xaa:
                Scr_NotifyEntity(ent, scrStr_binocular_fire, 0);
                break;
            case 0xab:
                Scr_NotifyEntity(ent, scrStr_binocular_release, 0);
                break;
            case 0xac:
                Scr_NotifyEntity(ent, scrStr_binocular_drop, 0);
                break;
            case 0xc5:
                if (ent->client != NULL && (ent->flags & 3) == 0) {
                    ent->health = 0;
                    cl->ps.stats[0] = 0;
                    player_die(ent, ent, ent, 100000, 0xc, 0, 0, 0, 0);
                }
                break;
            default:
                break;
            }
        } else {
            float scale;

            if (ent->s.eType != 1) {
                return;
            }
            if (parm < 100) {
                scale = (float)parm * 0.01f;
            } else {
                scale = 1.1f;
            }
            if (Float_NonZeroOrNaN(scale)) {
                float dir[3];

                scale = scale * (float)cl->ps.stats[2];
                VectorSet_080f7c5e(dir, 0, 0, 1.0f);
                G_Damage(ent, 0, 0, 0, 0, Round_To_Int((double)scale), 0, 0xb, 0, 0);
            }
        }
        idx = idx + 1;
    }
}

// 0x080f5614  size=109  FUN_080f5614  status=translated  naming=inferred
// Evidence: stores a clamped activity time on the client of clientNum (gentity array index).
void Client_SetLastActivityTime(int32 clientNum, int32 time)
{
    struct gclient_s *cl = g_gentities[clientNum].client;

    if (1000 < g_0864c56c - time) {
        time = g_0864c56c - 1000;
    }
    if (cl->lastActivityTime <= time || g_0864c56c <= time) {
        cl->lastActivityTime = time;
    }
}

// 0x080f5682  size=117  FUN_080f5682  status=translated  naming=inferred
// Evidence: contents of the entity from client flags (0x27ac, 0x27b0, 0x26a8); 0x2000000 is the contents value used when the client is not flagged.
void Entity_SetContentsForClientFlags(struct gentity_s *ent)
{
    struct gclient_s *cl = ent->client;

    if (cl->noclip == 0) {
        if (cl->ufo == 0) {
            if (cl->sessionState == 1) {
                ent->r.contents = 0;
            } else {
                ent->r.contents = 0x2000000u;
            }
        } else {
            ent->r.contents = 0;
        }
    } else {
        ent->r.contents = 0;
    }
}
