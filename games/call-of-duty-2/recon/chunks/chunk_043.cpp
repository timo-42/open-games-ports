#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
/* Pre-canonical type spellings that the generated cod2_prototypes.h may still carry for functions of
 * chunks that have already migrated (the merge lane regenerates it): map them onto the canonical
 * tags so calls compile against either spelling. No effect once the header is regenerated. */
#define PlayerState playerState_s
#define PmoveCtx pmove_s
#define JumpMoveOut pml_s
#define UserCmd040 pml_s
#define WeaponDef043_s weaponDef_s
#define playerState043_s playerState_s
#define pmove043_s pmove_s
#define pml043_s pml_s
#define weapon_def_s weaponDef_s
#include "cod2_prototypes.h"
#include "chunk_043_types.h"

/*
 * chunk_043: 0x080e982e..0x080ed440
 * Vector helpers, the player-movement slide/step code (Quake III PM_SlideMove /
 * PM_StepSlideMove lineage), the BG weapon-definition registry and per-player weapon
 * slot/ammo helpers, and the weapon aim/zoom fraction updaters.
 */

/* ------------------------------------------------------------------------- */
/* Vector helpers, first copy (0x080e982e..0x080e9a70)                       */
/* ------------------------------------------------------------------------- */

// 0x080e982e  size=62  FUN_080e982e  status=translated  naming=inferred
// Flips the sign bit of three floats with integer XOR; matches Q3 VectorNegate_080e982e.
void VectorNegate_080e982e(const vec_t *in, vec_t *out)
{
    const unsigned int *src = (const unsigned int *)in;
    unsigned int *dst = (unsigned int *)out;
    dst[0] = src[0] ^ 0x80000000u;
    dst[1] = src[1] ^ 0x80000000u;
    dst[2] = src[2] ^ 0x80000000u;
}

// 0x080e986c  size=68  FUN_080e986c  status=translated  naming=inferred
// out = a + b per component; matches Q3 VectorAdd_080e986c.
void VectorAdd_080e986c(const vec_t *a, const vec_t *b, vec_t *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x080e98b0  size=68  FUN_080e98b0  status=translated  naming=inferred
// out = a - b per component; matches Q3 VectorSubtract_080e98b0.
void VectorSubtract_080e98b0(const vec_t *a, const vec_t *b, vec_t *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080e98f4  size=56  FUN_080e98f4  status=translated  naming=inferred
// out = scale * in; matches Q3 VectorScale_080e98f4.
void VectorScale_080e98f4(const vec_t *in, float scale, vec_t *out)
{
    out[0] = scale * in[0];
    out[1] = scale * in[1];
    out[2] = scale * in[2];
}

// 0x080e992c  size=83  FUN_080e992c  status=translated  naming=inferred
// out = base + scale * dir; matches Q3 VectorMA_080e992c.
void VectorMA_080e992c(const vec_t *base, float scale, const vec_t *dir, vec_t *out)
{
    out[0] = base[0] + scale * dir[0];
    out[1] = base[1] + scale * dir[1];
    out[2] = base[2] + scale * dir[2];
}

// 0x080e9980  size=51  FUN_080e9980  status=translated  naming=inferred
// Three-component dot product returned as float; matches Q3 DotProduct.
float DotProduct3(const vec_t *a, const vec_t *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// 0x080e99b4  size=51  FUN_080e99b4  status=translated  naming=inferred
// Squared length; matches Q3 VectorLengthSquared_080e99b4.
float VectorLengthSquared_080e99b4(const vec_t *v)
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

// 0x080e99e8  size=62  FUN_080e99e8  status=translated  naming=inferred
// Length = Float_Sqrt_080e9590(sum of squares). The callee is the square-root wrapper (not in this chunk);
// its float result is returned in st0 unchanged.
float VectorLength_080e99e8(const vec_t *v)
{
    return Float_Sqrt_080e9590(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// 0x080e9a26  size=41  FUN_080e9a26  status=translated  naming=inferred
// Float select: comparator >= 0.0 ? ifNonNegative : ifNegative (NaN selects ifNegative). Counterpart of Q_fsel.
float Q_fsel043(float comparator, float ifNonNegative, float ifNegative)
{
    if (comparator >= 0.0f) {
        return ifNonNegative;
    }
    return ifNegative;
}

// 0x080e9a50  size=31  FUN_080e9a50  status=translated  naming=inferred
// Integer select: comparator < 0 ? ifNegative : ifNonNegative. Counterpart of Q_isel.
int Q_isel043(int comparator, int ifNonNegative, int ifNegative)
{
    if (comparator < 0) {
        return ifNegative;
    }
    return ifNonNegative;
}

// 0x080e9a70  size=11  FUN_080e9a70  status=translated  naming=unknown
// Returns the sign bit of a 32-bit value (logical shift right by 31).
unsigned int SignBit32(unsigned int value)
{
    return value >> 31;
}

// 0x080e9a7c  size=255  FUN_080e9a7c  status=translated  naming=unknown
// Called by PM_StepSlideMove when the player is on the ground. If pm_flags bit 0 is clear it returns 1.
// Otherwise it runs PM_ProbeClimbableLedgeEntry (a position/clearance probe, 14 args); a zero result restores
// ps->origin from savedOrigin and ps->velocity from savedVelocity. Returns the probe result.
// Evidence: only caller is PM_StepSlideMove; callee identity unknown.
int PM_StepProbeOrRestore(pmove_t *pm, const vec_t *savedOrigin, const vec_t *savedVelocity)
{
    playerState_t *ps = pm->ps;
    int result;

    if ((ps->pm_flags & 1) == 0) {
        result = 1;
    } else {
        result = PM_ProbeClimbableLedgeEntry(ps->clientNum, ps->origin, ps->maxs[0], 30.0f, ps->proneDirection,
                             &ps->fTorsoHeight, &ps->fTorsoPitch, &ps->fWaistPitch, 1, 1, 0,
                             pm->handler, 0, 66.0f);
        if (result == 0) {
            VectorCopy_080eaede(savedOrigin, ps->origin);
            VectorCopy_080eaede(savedVelocity, ps->velocity);
        }
    }
    return result;
}

// 0x080e9b7c  size=197  FUN_080e9b7c  status=translated  naming=inferred
// Insertion-sorts plane indices by ascending dot(velocity, plane[i]); writes the ordering to order[]
// and returns the smallest dot product (the dot of order[0]). Used by PM_SlideMove for CoD2's
// multi-plane clipping (not in Q3).
float PM_SortPlanesByDot(const vec_t *velocity, int numPlanes, const vec_t *planes, int *order)
{
    volatile float dots[11]; /* volatile: the binary stores each dot as float and reloads it (no x87 excess precision) */
    int i;
    int j;

    for (i = 0; i < numPlanes; i++) {
        dots[i] = DotProduct3_alt(velocity, planes + i * 3);
        j = i;
        while (j != 0 && !(dots[i] > dots[order[j - 1]])) { /* ja exits; NaN keeps shifting */
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
    }
    return dots[order[0]];
}

// 0x080e9c42  size=1969  FUN_080e9c42  status=translated  naming=inferred
// PM_SlideMove: Q3 bump/clip loop (4 bumps, planes, gravity half-step) extended with CoD2's sorted-plane
// crease handling. Names from the Q3 counterpart structure (velocity/origin offsets 0x20/0x14, trace
// fraction, clip-velocity helper). Returns (bumpcount != 0), or 1 on the early "stuck" exits.
int PM_SlideMove(pmove_t *pm, pml_t *pml, int gravity)
{
    playerState_t *ps = pm->ps;
    int numbumps = 4;
    vec3_t primal_velocity;
    vec3_t endVelocity;
    vec3_t end;
    vec3_t planes[PM043_MAX_CLIP_PLANES + 1];
    unsigned char planeTail[8];
    int numplanes;
    int order[8];
    trace_t trace;
    float time_left;
    int bumpcount;
    int i;
    int j;
    int k;
    vec3_t clipVelocity;
    vec3_t clipEndVelocity;
    vec3_t dir;
    float d;
    float dist;
    int startedGround;

    (void)planeTail;
    VectorCopy_080eaede(ps->velocity, primal_velocity);
    VectorCopy_080eaede(ps->velocity, endVelocity);
    if (gravity != 0) {
        endVelocity[2] = endVelocity[2] - (float)ps->gravity * pml->frametime;
        ps->velocity[2] = (ps->velocity[2] + endVelocity[2]) * 0.5f;
        primal_velocity[2] = endVelocity[2];
        if (pml->groundPlane != 0) {
            PM_ClipVelocity(ps->velocity, pml->groundTrace.normal, ps->velocity);
        }
    }

    time_left = pml->frametime;
    startedGround = (pml->groundPlane != 0);
    if (startedGround) {
        VectorCopy_080eaede(pml->groundTrace.normal, planes[0]);
    }
    numplanes = startedGround ? 1 : 0;
    Vec3_NormalizeCopy(ps->velocity, planes[numplanes]);
    numplanes = numplanes + 1;

    for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
        VectorMA3_alt(ps->origin, time_left, ps->velocity, end);
        PM_Trace(pm, &trace, ps->origin, pm->mins, pm->maxs, end, ps->clientNum, pm->tracemask);
        if (trace.allsolid != 0) {
            ps->velocity[2] = 0.0f;
            return 1;
        }
        if (0.0f < trace.fraction) {
            VectorLerp(ps->origin, end, trace.fraction, ps->origin);
        }
        if (trace.fraction == 1.0f) {
            break;
        }
        PM_AddTouchEnt(pm, trace.entityNum);
        time_left = time_left - time_left * trace.fraction;
        if (7 < numplanes) {
            ClearVector3(ps->velocity);
            return 1;
        }
        for (i = 0; i < numplanes; i++) {
            if (0.999f < DotProduct3_alt(trace.normal, planes[i])) {
                PM_ClipVelocity(ps->velocity, trace.normal, ps->velocity);
                VectorAdd3_alt(trace.normal, ps->velocity, ps->velocity);
                break;
            }
        }
        if (numplanes <= i) {
            VectorCopy_080eaede(trace.normal, planes[numplanes]);
            numplanes = numplanes + 1;
            dist = PM_SortPlanesByDot(ps->velocity, numplanes, planes[0], order);
            if (dist < 0.1f) {
                if (pml->impactSpeed < -dist) {
                    pml->impactSpeed = -dist;
                }
                PM_ClipVelocity(ps->velocity, planes[order[0]], clipVelocity);
                PM_ClipVelocity(endVelocity, planes[order[0]], clipEndVelocity);
                for (j = 1; j < numplanes; j++) {
                    if (DotProduct3_alt(clipVelocity, planes[order[j]]) < 0.1f) {
                        PM_ClipVelocity(clipVelocity, planes[order[j]], clipVelocity);
                        PM_ClipVelocity(clipEndVelocity, planes[order[j]], clipEndVelocity);
                        if (DotProduct3_alt(clipVelocity, planes[order[0]]) < 0.0f) {
                            Vec3_Cross(planes[order[0]], planes[order[j]], dir);
                            Vec3_Normalize(dir);
                            d = DotProduct3_alt(dir, ps->velocity);
                            VectorScale3_alt(dir, d, clipVelocity);
                            d = DotProduct3_alt(dir, endVelocity);
                            VectorScale3_alt(dir, d, clipEndVelocity);
                            for (k = 1; k < numplanes; k++) {
                                if (k != j && DotProduct3_alt(clipVelocity, planes[order[k]]) < 0.1f) {
                                    ClearVector3(ps->velocity);
                                    return 1;
                                }
                            }
                        }
                    }
                }
                VectorCopy_080eaede(clipVelocity, ps->velocity);
                VectorCopy_080eaede(clipEndVelocity, endVelocity);
            }
        }
    }

    if (gravity != 0) {
        VectorCopy_080eaede(endVelocity, ps->velocity);
    }
    if (ps->pm_time != 0) {
        VectorCopy_080eaede(primal_velocity, ps->velocity);
    }
    return bumpcount != 0;
}

// 0x080ea3f4  size=2557  FUN_080ea3f4  status=translated  naming=inferred
// PM_StepSlideMove: run PM_SlideMove, then try stepping up (stepSize 18, or 10 when pm_flags bit 0 is set)
// and back down, restore the pre-step state when the step failed, and emit the step event and
// velocity damping. Names from the Q3 counterpart (slide, step up, trace down, event on step).
// NaN-aware != 0.0 tests are kept as written. pm_flags masks 0x20/0x80000/0x1 are unnamed bits.
void PM_StepSlideMove(pmove_t *pm, pml_t *pml, int gravity)
{
    playerState_t *ps = pm->ps;
    float stepUp = 0.0f;
    bool launched = false;
    int canStepDown;
    vec3_t start_o;
    vec3_t start_v;
    vec3_t down_o;
    vec3_t down_v;
    vec3_t delta2d;
    vec3_t moved2d;
    vec3_t up;
    vec3_t down;
    vec3_t tmp;
    trace_t trace;
    int slid;
    float stepSize;
    int stepped;
    int stepDelta;
    int absDelta;
    int oldBob;
    float absz;

    if ((ps->pm_flags & 0x20) == 0) {
        if (pml->groundPlane == 0) {
            canStepDown = 0;
            if ((ps->pm_flags & 0x80000) != 0 && ps->pm_time != 0) {
                Player_ClearJumpBaseZ(ps);
            }
        } else {
            canStepDown = 1;
        }
    } else {
        canStepDown = 0;
        Player_ClearJumpBaseZ(ps);
    }

    VectorCopy_080eaede(ps->origin, start_o);
    VectorCopy_080eaede(ps->velocity, start_v);
    slid = PM_SlideMove(pm, pml, gravity);

    if ((ps->pm_flags & 1) == 0) {
        stepSize = 18.0f;
    } else {
        stepSize = 10.0f;
    }

    if (ps->groundEntityNum == PM043_GROUND_NONE) {
        if ((ps->pm_flags & 0x80000) != 0 && ps->pm_time != 0) {
            Player_ClearJumpBaseZ(ps);
        }
        if (slid != 0 && (ps->pm_flags & 0x80000) != 0 && Player_CheckStepUp(ps, start_o, &stepSize) != 0) {
            if (stepSize < 1.0f) {
                return;
            }
            launched = true;
        }
        if (!launched) {
            if ((ps->pm_flags & 0x20) == 0) {
                return;
            }
            if (ps->velocity[2] <= 0.0f) {
                return;
            }
        }
    }

    VectorCopy_080eaede(ps->origin, down_o);
    VectorCopy_080eaede(ps->velocity, down_v);
    VectorSubtract2D(down_o, start_o, delta2d);

    if (slid != 0) {
        VectorCopy_080eaede(start_o, up);
        up[2] = up[2] + stepSize + 1.0f;
        PM_Trace(pm, &trace, start_o, pm->mins, pm->maxs, up, ps->clientNum, pm->tracemask);
        stepUp = (stepSize + 1.0f) * trace.fraction - 1.0f;
        if (stepUp < 1.0f) {
            stepUp = 0.0f;
        } else {
            VectorSet3(ps->origin, up[0], up[1], start_o[2] + stepUp);
            VectorCopy_080eaede(start_v, ps->velocity);
            PM_SlideMove(pm, pml, gravity);
        }
    }

    if (canStepDown != 0 || stepUp != 0.0f) {
        VectorCopy_080eaede(ps->origin, down);
        down[2] = down[2] - stepUp;
        if (canStepDown != 0) {
            down[2] = down[2] - 9.0f;
        }
        PM_Trace(pm, &trace, ps->origin, pm->mins, pm->maxs, down, ps->clientNum, pm->tracemask);
        if (trace.entityNum < 0x40) {
            VectorCopy_080eaede(down_o, ps->origin);
            VectorCopy_080eaede(down_v, ps->velocity);
            return;
        }
        if (trace.fraction < 1.0f) {
            if (trace.normal[2] < 0.3f) {
                VectorCopy_080eaede(down_o, ps->origin);
                VectorCopy_080eaede(down_v, ps->velocity);
                return;
            }
            VectorLerp(ps->origin, down, trace.fraction, ps->origin);
            PM_ClipVelocity(ps->velocity, trace.normal, ps->velocity);
        } else if (stepUp != 0.0f) {
            ps->origin[2] = ps->origin[2] - stepUp;
        }
    }

    VectorSubtract2D(ps->origin, start_o, moved2d);
    {
        float movedAlong = DotProduct2D(moved2d, ps->velocity);
        stepped = (movedAlong <= 0.001f + DotProduct2D(delta2d, ps->velocity));
    }
    if (stepped != 0 || (launched && Player_IsAboveJumpBase(ps) != 0)) {
        VectorCopy_080eaede(down_o, ps->origin);
        VectorCopy_080eaede(down_v, ps->velocity);
        stepUp = 0.0f;
        if (canStepDown != 0) {
            VectorCopy_080eaede(ps->origin, down);
            down[2] = down[2] - 9.0f;
            PM_Trace(pm, &trace, ps->origin, pm->mins, pm->maxs, down, ps->clientNum, pm->tracemask);
            if (trace.fraction < 1.0f) {
                VectorLerp(ps->origin, down, trace.fraction, tmp);
                stepUp = tmp[2] - ps->origin[2];
                VectorCopy_080eaede(tmp, ps->origin);
                PM_ClipVelocity(ps->velocity, trace.normal, ps->velocity);
            }
        }
    }

    if (launched) {
        Player_ClampJumpVelocity(ps, down_o);
    }

    if (canStepDown != 0 && ps->pm_type < 6 && PM_StepProbeOrRestore(pm, start_o, start_v) != 0 &&
        0.5f < Q_fabs043(ps->origin[2] - down_o[2]) &&
        (stepDelta = Q_roundToInt043(ps->origin[2] - down_o[2])) != 0) {
        if (stepDelta < -0x10) {
            stepDelta = -0x10;
        } else if (0x18 < stepDelta) {
            stepDelta = 0x18;
        }
        BG_AddPredictableEventToPlayerstate(0x8f, stepDelta + 0x80, ps);
        absz = Q_fabs043(ps->origin[2] - start_o[2]);
        VectorScale3_alt(ps->velocity, (1.0f - absz / stepSize) * 0.8f + 0.19999999f, ps->velocity);
        absDelta = stepDelta;
        if (stepDelta < 0) {
            absDelta = -stepDelta;
        }
        if (3 < absDelta && ps->groundEntityNum != PM043_GROUND_NONE && PM_FootstepSpeedOk(pm) != 0) {
            if (stepDelta < 0) {
                absDelta = -stepDelta;
            }
            stepDelta = absDelta / 2;
            if (4 < stepDelta) {
                stepDelta = 4;
            }
            oldBob = ps->bobCycle;
            ps->bobCycle = (int)((float)oldBob + (float)stepDelta * 1.25f + 7.0f) & 0xff;
            PM_FootstepEvent(pm, pml, oldBob, ps->bobCycle, 1);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Vector helpers, second copy (0x080eadf2..0x080eafde)                      */
/* ------------------------------------------------------------------------- */

// 0x080eadf2  size=25  FUN_080eadf2  status=translated  naming=inferred
// Absolute value of a float (x87 fabs); counterpart of Q_fabs.
float Q_fabs043(float x)
{
    return fabsf(x);
}

// 0x080eae0c  size=54  FUN_080eae0c  status=translated  naming=inferred
// Rounds to nearest: (int)floor((double)(x + 0.5f)) with truncating float->int conversion.
int Q_roundToInt043(float x)
{
    return (int)floor((double)(x + 0.5f));
}

// 0x080eae42  size=44  FUN_080eae42  status=translated  naming=unknown
// 2D subtract: out[0..1] = a - b; out[2] is not written.
void VectorSubtract2D(const vec_t *a, const vec_t *b, vec_t *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

// 0x080eae6e  size=33  FUN_080eae6e  status=translated  naming=unknown
// 2D dot product of the first two components.
float DotProduct2D(const vec_t *a, const vec_t *b)
{
    return a[0] * b[0] + a[1] * b[1];
}

// 0x080eae90  size=41  FUN_080eae90  status=translated  naming=inferred
// Zeroes three floats (VectorClear).
void ClearVector3(vec_t *v)
{
    ((unsigned int *)v)[0] = 0;
    ((unsigned int *)v)[1] = 0;
    ((unsigned int *)v)[2] = 0;
}

// 0x080eaeba  size=35  FUN_080eaeba  status=translated  naming=inferred
// Stores x, y, z into a vector (VectorSet).
void VectorSet3(vec_t *v, vec_t x, vec_t y, vec_t z)
{
    /* integer moves of the argument words (bit-exact, no x87 quieting of signalling NaNs) */
    ((int32 *)v)[0] = *(volatile const int32 *)&x;
    ((int32 *)v)[1] = *(volatile const int32 *)&y;
    ((int32 *)v)[2] = *(volatile const int32 *)&z;
}

// 0x080eaede  size=47  FUN_080eaede  status=translated  naming=inferred
// Copies three 32-bit words in to out (VectorCopy_080eaede).
void VectorCopy_080eaede(const vec_t *in, vec_t *out)
{
    ((unsigned int *)out)[0] = ((const unsigned int *)in)[0];
    ((unsigned int *)out)[1] = ((const unsigned int *)in)[1];
    ((unsigned int *)out)[2] = ((const unsigned int *)in)[2];
}

// 0x080eaf0e  size=68  FUN_080eaf0e  status=translated  naming=inferred
// Second compiled copy of VectorAdd_080e986c (identical body to 0x080e986c).
void VectorAdd3_alt(const vec_t *a, const vec_t *b, vec_t *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x080eaf52  size=56  FUN_080eaf52  status=translated  naming=inferred
// Second compiled copy of VectorScale_080e98f4 (identical body to 0x080e98f4).
void VectorScale3_alt(const vec_t *in, float scale, vec_t *out)
{
    out[0] = scale * in[0];
    out[1] = scale * in[1];
    out[2] = scale * in[2];
}

// 0x080eaf8a  size=83  FUN_080eaf8a  status=translated  naming=inferred
// Second compiled copy of VectorMA_080e992c (identical body to 0x080e992c).
void VectorMA3_alt(const vec_t *base, float scale, const vec_t *dir, vec_t *out)
{
    out[0] = base[0] + scale * dir[0];
    out[1] = base[1] + scale * dir[1];
    out[2] = base[2] + scale * dir[2];
}

// 0x080eafde  size=51  FUN_080eafde  status=translated  naming=inferred
// Second compiled copy of DotProduct (identical body to 0x080e9980).
float DotProduct3_alt(const vec_t *a, const vec_t *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/* ------------------------------------------------------------------------- */
/* BG weapon definition registry (0x080eb014..0x080ebcdc)                    */
/* ------------------------------------------------------------------------- */

int bg_numWeapons;
weaponDef_t *bg_weaponDefs[PM043_MAX_WEAPONS_ARRAY];
weaponDef_t *bg_ammoTypes[128];
int bg_numAmmoTypes;
weaponDef_t *bg_sharedAmmoCaps[128];
int bg_numSharedAmmoCaps;
weaponDef_t *bg_clipTypes[128];
int bg_numClipTypes;

// 0x080eb014  size=15  FUN_080eb014  status=translated  naming=unknown
// Resets the registered weapon count (global 0x08627080) to 0.
void BG_ResetWeaponCount(void)
{
    bg_numWeapons = 0;
}

// 0x080eb024  size=184  FUN_080eb024  status=translated  naming=inferred
// Fills the item-table entry for weaponIndex (stride 0x2c, table 0x081850c0) from its WeaponDef:
// copies defs fields 0x98/0x1b4/0x1b8/0x04/0x1c0/0x1c8/0x1d0 and sets type=1 and weaponIndex.
// Name guessed from BG_FillInAmmoItems's neighbours.
void BG_FillInWeaponItem(int weaponIndex)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    bgItem043_t *item = &bg_itemList[weaponIndex];

    item->fromWeapon98 = def->pickupSound;
    item->fromWeapon1b4 = def->worldModel;
    item->zero0c = 0;
    item->fromWeapon1b8 = def->hudIcon;
    item->name = def->displayName;
    item->fromWeapon1c0 = def->startAmmo;
    item->itemType = 1;
    item->weaponIndex = weaponIndex;
    item->ammoIndex = def->iAmmoIndex;
    item->clipIndex = def->iClipIndex;
}

// 0x080eb0dc  size=365  FUN_080eb0dc  status=translated  naming=observed
// String "\025BG_FillInAmmoItems: weapon def for default weapon could not be found!" at 0x081553c0.
// For item slots 0x81..0x82 with itemType 2, looks up the weapon named by the first word of
// item->name and stores its index/ammo/clip; falls back to "defaultweapon_mp" (index 0 def when missing).
void BG_FillInAmmoItems(BGWeaponLoadedCallback043 callback)
{
    int slot;
    bgItem043_t *item;
    char nameBuf[0x40 + 12];
    char *p;
    int weaponIndex;
    weaponDef_t *def;

    for (slot = 0x81; slot <= 0x82; slot++) {
        item = &bg_itemList[slot];
        if (item->itemType == 2) {
            Q_strncpyz(nameBuf, item->name, 0x40);
            for (p = nameBuf; *p != '\0'; p++) {
                if (*p == ' ') {
                    *p = '\0';
                    break;
                }
            }
            weaponIndex = BG_RegisterWeapon(nameBuf, callback);
            if (weaponIndex != 0) {
                def = BG_GetWeaponDef(weaponIndex);
                item->weaponIndex = weaponIndex;
                item->ammoIndex = def->iAmmoIndex;
                item->clipIndex = def->iClipIndex;
                return;
            }
            Com_Printf("^3WARNING^7: Could not find weapon for ammo item %s\n", item->name);
            weaponIndex = BG_RegisterWeapon("defaultweapon_mp", callback);
            if (weaponIndex == 0) {
                Com_Error(1, "\025BG_FillInAmmoItems: weapon def for default weapon could not be found!");
            }
            def = BG_GetWeaponDef(weaponIndex);
            item->weaponIndex = 1;
            item->ammoIndex = def->iAmmoIndex;
            item->clipIndex = def->iClipIndex;
        }
    }
}

// 0x080eb24a  size=373  FUN_080eb24a  status=translated  naming=inferred
// Error string "Max ammo mismatch for ... ammo" at 0x08155420. Registers def's ammo type: finds an equal
// ammoName (strcmp) among bg_ammoTypes or appends it; on a repeat name with a different maxAmmo it
// scans earlier weapon defs and raises Com_Error(1, ...) if another weapon already set that max.
void BG_FillInAmmoIndex(int weaponIndex)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    weaponDef_t *other;
    int i = 0;
    int j;

    for (;;) {
        if (bg_numAmmoTypes <= i) {
            bg_ammoTypes[i] = def;
            def->iAmmoIndex = i;
            bg_numAmmoTypes = bg_numAmmoTypes + 1;
            return;
        }
        if (strcmp(bg_ammoTypes[i]->ammoName, def->ammoName) == 0) {
            break;
        }
        i++;
    }
    def->iAmmoIndex = i;
    if (bg_ammoTypes[i]->maxAmmo == def->maxAmmo) {
        return;
    }
    if (i == 0) {
        return;
    }
    for (j = 1; j < weaponIndex; j++) {
        other = bg_weaponDefs[j];
        if (Q_stricmp(bg_ammoTypes[i]->ammoName, other->ammoName) == 0 &&
            other->maxAmmo == bg_ammoTypes[i]->maxAmmo) {
            Com_Error(1,
                      "\025Max ammo mismatch for \"%s\" ammo: '%s\" set it to %i, but \"%s\" already set it to %i.\n",
                      def->ammoName, def->szInternalName, def->maxAmmo, other->szInternalName, other->maxAmmo);
        }
    }
}

// 0x080eb3c0  size=439  FUN_080eb3c0  status=translated  naming=inferred
// Error string "Shared ammo cap mismatch for ..." at 0x08155480 and debug print "%s: %s\n".
// Same registration scheme as BG_FillInAmmoIndex for sharedAmmoCapName/sharedAmmoCap, case-insensitive
// (Q_stricmp) and skipped when the cap name is empty; index defaults to -1.
void BG_FillInSharedAmmoCapIndex(int weaponIndex)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    weaponDef_t *other;
    int i;
    int j;

    def->iSharedAmmoCapIndex = -1;
    if (*def->sharedAmmoCapName != '\0') {
        Com_DPrintf("%s: %s\n", def->szInternalName, def->sharedAmmoCapName);
        for (i = 0; i < bg_numSharedAmmoCaps; i++) {
            if (Q_stricmp(bg_sharedAmmoCaps[i]->sharedAmmoCapName, def->sharedAmmoCapName) == 0) {
                def->iSharedAmmoCapIndex = i;
                if (bg_sharedAmmoCaps[i]->sharedAmmoCap == def->sharedAmmoCap) {
                    return;
                }
                if (i == 0) {
                    return;
                }
                for (j = 1; j < weaponIndex; j++) {
                    other = bg_weaponDefs[j];
                    if (Q_stricmp(bg_sharedAmmoCaps[i]->sharedAmmoCapName, other->sharedAmmoCapName) == 0 &&
                        other->sharedAmmoCap == bg_sharedAmmoCaps[i]->sharedAmmoCap) {
                        Com_Error(1,
                                  "\025Shared ammo cap mismatch for \"%s\" shared ammo cap: '%s\" set it to %i, but \"%s\" already set it to %i.\n",
                                  def->sharedAmmoCapName, def->szInternalName, def->sharedAmmoCap,
                                  other->szInternalName, other->sharedAmmoCap);
                    }
                }
                return;
            }
        }
        bg_sharedAmmoCaps[i] = def;
        def->iSharedAmmoCapIndex = i;
        bg_numSharedAmmoCaps = bg_numSharedAmmoCaps + 1;
    }
}

// 0x080eb578  size=373  FUN_080eb578  status=translated  naming=inferred
// Error string "Clip Size mismatch for ... clip" at 0x08155500. Registers def's clip type by clipName
// (strcmp) in bg_clipTypes; on a repeat name with a different clipSize it scans earlier weapons
// (Q_stricmp) and raises Com_Error. The first error argument is def->ammoName (not clipName), as compiled.
void BG_FillInClipIndex(int weaponIndex)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    weaponDef_t *other;
    int i = 0;
    int j;

    for (;;) {
        if (bg_numClipTypes <= i) {
            bg_clipTypes[i] = def;
            def->iClipIndex = i;
            bg_numClipTypes = bg_numClipTypes + 1;
            return;
        }
        if (strcmp(bg_clipTypes[i]->clipName, def->clipName) == 0) {
            break;
        }
        i++;
    }
    def->iClipIndex = i;
    if (bg_clipTypes[i]->clipSize == def->clipSize) {
        return;
    }
    if (i == 0) {
        return;
    }
    for (j = 1; j < weaponIndex; j++) {
        other = bg_weaponDefs[j];
        if (Q_stricmp(bg_clipTypes[i]->clipName, other->clipName) == 0 &&
            other->clipSize == bg_clipTypes[i]->clipSize) {
            Com_Error(1,
                      "\025Clip Size mismatch for \"%s\" clip: '%s\" set it to %i, but \"%s\" already set it to %i.\n",
                      def->ammoName, def->szInternalName, def->clipSize, other->szInternalName, other->clipSize);
        }
    }
}

// 0x080eb6ee  size=270  FUN_080eb6ee  status=translated  naming=inferred
// Error strings "could not find altWeapon '%s' for weapon '%s'", "...same weaponSlot setting as its alt
// weapon", "...same slotStackable setting..." (0x08155560/0x081555a0/0x08155600). Resolves the alt weapon
// by name (registering it if needed) into altWeaponIndex and validates weaponSlot and slotStackable.
void BG_FillInAltWeaponIndex(int weaponIndex, BGWeaponLoadedCallback043 callback)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    weaponDef_t *alt;
    int altIndex;

    def->altWeaponIndex = 0;
    if (*def->altWeapon != '\0') {
        altIndex = BG_RegisterWeapon(def->altWeapon, callback);
        if (altIndex == 0) {
            Com_Error(1, "\025could not find altWeapon '%s' for weapon '%s'", def->altWeapon, def->szInternalName);
        }
        def->altWeaponIndex = altIndex;
        alt = bg_weaponDefs[altIndex];
        if (def->weaponSlot != alt->weaponSlot) {
            Com_Error(1, "\025weapon '%s' does not have same weaponSlot setting as its alt weapon '%s'",
                      def->szInternalName, alt->szInternalName);
        }
        if (def->slotStackable != alt->slotStackable) {
            Com_Error(1, "\025weapon '%s' does not have same slotStackable setting as its alt weapon '%s'",
                      def->szInternalName, alt->szInternalName);
        }
    }
}

// 0x080eb7fc  size=140  FUN_080eb7fc  status=translated  naming=unknown
// Initialises the registry: slot 0 of every table is the default weapon def returned by BG_GetNoWeaponDef,
// counts are set to 1, item types of slots 1..0x80 are cleared, then BG_AnimLoadPlayerAnimTypes and BG_ClearPlayerAnimTypeNames run.
void BG_InitWeaponRegistry(void)
{
    int i;

    bg_weaponDefs[0] = BG_GetNoWeaponDef();
    bg_numAmmoTypes = 1;
    bg_numSharedAmmoCaps = 1;
    bg_numClipTypes = 1;
    bg_ammoTypes[0] = bg_weaponDefs[0];
    bg_sharedAmmoCaps[0] = bg_weaponDefs[0];
    bg_clipTypes[0] = bg_weaponDefs[0];
    for (i = 1; i < 0x81; i++) {
        bg_itemList[i].itemType = 0;
    }
    BG_AnimLoadPlayerAnimTypes();
    BG_ClearPlayerAnimTypeNames();
}

// 0x080eb888  size=52  FUN_080eb888  status=translated  naming=inferred
// Runs the four index-filling steps for one weapon: ammo, shared ammo cap, clip, item entry.
void BG_FillInWeaponIndices(int weaponIndex)
{
    BG_FillInAmmoIndex(weaponIndex);
    BG_FillInSharedAmmoCapIndex(weaponIndex);
    BG_FillInClipIndex(weaponIndex);
    BG_FillInWeaponItem(weaponIndex);
}

// 0x080eb8bc  size=84  FUN_080eb8bc  status=translated  naming=inferred
// Appends def to bg_weaponDefs (count pre-incremented), fills its indices and alt weapon, invokes the
// optional callback with the new index, and returns that index.
int BG_RegisterWeaponDef(weaponDef_t *def, BGWeaponLoadedCallback043 callback)
{
    int index;

    bg_numWeapons = bg_numWeapons + 1;
    index = bg_numWeapons;
    bg_weaponDefs[bg_numWeapons] = def;
    BG_FillInWeaponIndices(index);
    BG_FillInAltWeaponIndex(index, callback);
    if (callback != 0) {
        callback(index);
    }
    return index;
}

// 0x080eb910  size=51  FUN_080eb910  status=translated  naming=inferred
// Re-runs BG_FillInWeaponIndices for every registered weapon 1..count.
void BG_FillInAllWeaponIndices(void)
{
    int count = BG_GetNumWeapons();
    int i;

    for (i = 1; i <= count; i++) {
        BG_FillInWeaponIndices(i);
    }
}

// 0x080eb944  size=47  FUN_080eb944  status=translated  naming=unknown
// If pm_type < 6: ps->weapAnim = ((ps->weapAnim & 0x200) ^ 0x200) | bits (bit 9 inverted, other old bits dropped).
void PlayerState_SetWeapAnim(playerState_t *ps, unsigned int bits)
{
    if (ps->pm_type < 6) {
        ps->weapAnim = ((ps->weapAnim & 0x200) ^ 0x200) | bits;
    }
}

// 0x080eb974  size=47  FUN_080eb974  status=translated  naming=unknown
// Calls PlayerState_SetWeapAnim only if (field_5d0 & ~0x200) differs from bits.
void PlayerState_SetWeapAnimIfChanged(playerState_t *ps, unsigned int bits)
{
    if ((ps->weapAnim & 0xfffffdffu) != bits) {
        PlayerState_SetWeapAnim(ps, bits);
    }
}

// 0x080eb9a4  size=15  FUN_080eb9a4  status=translated  naming=inferred
// Returns bg_weaponDefs[weaponIndex].
weaponDef_t *BG_GetWeaponDef(int weaponIndex)
{
    return bg_weaponDefs[weaponIndex];
}

// 0x080eba56  size=10  FUN_080eba56  status=translated  naming=inferred
// Returns the registered weapon count.
int BG_GetNumWeapons(void)
{
    return bg_numWeapons;
}

// 0x080eba60  size=10  FUN_080eba60  status=translated  naming=inferred
// Returns the number of ammo types.
int BG_GetNumAmmoTypes(void)
{
    return bg_numAmmoTypes;
}

// 0x080eba6a  size=21  FUN_080eba6a  status=translated  naming=inferred
// Returns maxAmmo of the weapon def that defined ammo type ammoIndex.
int BG_GetAmmoTypeMax(int ammoIndex)
{
    return bg_ammoTypes[ammoIndex]->maxAmmo;
}

// 0x080eba80  size=10  FUN_080eba80  status=translated  naming=inferred
// Returns the number of clip types.
int BG_GetNumClipTypes(void)
{
    return bg_numClipTypes;
}

// 0x080eba8a  size=21  FUN_080eba8a  status=translated  naming=inferred
// Returns clipSize of the weapon def that defined clip type clipIndex.
int BG_GetClipTypeSize(int clipIndex)
{
    return bg_clipTypes[clipIndex]->clipSize;
}

// 0x080ebaa0  size=21  FUN_080ebaa0  status=translated  naming=inferred
// Returns sharedAmmoCap of shared-ammo-cap entry index.
int BG_GetSharedAmmoCapMax(int index)
{
    return bg_sharedAmmoCaps[index]->sharedAmmoCap;
}

// 0x080ebab6  size=21  FUN_080ebab6  status=translated  naming=inferred
// Returns sharedAmmoCapName of shared-ammo-cap entry index (as a raw word, pointer-sized).
const char *BG_GetSharedAmmoCapName(int index)
{
    return bg_sharedAmmoCaps[index]->sharedAmmoCapName;
}

// 0x080ebacc  size=21  FUN_080ebacc  status=translated  naming=inferred
// Returns the ammoName of ammo type ammoIndex.
const char *BG_GetAmmoTypeName(int ammoIndex)
{
    return bg_ammoTypes[ammoIndex]->ammoName;
}

// 0x080ebae2  size=21  FUN_080ebae2  status=translated  naming=inferred
// Returns the clipName of clip type clipIndex.
const char *BG_GetClipTypeName(int clipIndex)
{
    return bg_clipTypes[clipIndex]->clipName;
}

// 0x080ebbd0  size=84  FUN_080ebbd0  status=translated  naming=inferred
// Case-insensitive search of registered weapons 1..count by internalName; returns the index or 0.
int BG_FindWeaponByName(const char *name)
{
    int i;

    for (i = 1; i <= bg_numWeapons; i++) {
        if (Q_stricmp(name, bg_weaponDefs[i]->szInternalName) == 0) {
            return i;
        }
    }
    return 0;
}

// 0x080ebc24  size=156  FUN_080ebc24  status=translated  naming=inferred
// String "Couldn't find weapon \"%s\"\n" at 0x081556f4+3. Empty or "none" names return 0; an already
// registered name returns its index; otherwise loads the def via BG_LoadWeaponDef("mp", name), registers
// it, and returns the new index (0 and a debug print when loading fails).
int BG_RegisterWeapon(const char *name, BGWeaponLoadedCallback043 callback)
{
    int index;
    weaponDef_t *def;

    if (*name == '\0' || strcasecmp(name, "none") == 0) {
        return 0;
    }
    index = BG_FindWeaponByName(name);
    if (index != 0) {
        return index;
    }
    def = BG_LoadWeaponDef("mp", name);
    if (def == 0) {
        Com_DPrintf("Couldn't find weapon \"%s\"\n", name);
        return 0;
    }
    return BG_RegisterWeaponDef(def, callback);
}

// 0x080ebcc0  size=27  FUN_080ebcc0  status=translated  naming=inferred
// Registers a weapon by name with no callback.
int BG_RegisterWeaponNoCallback(const char *name)
{
    return BG_RegisterWeapon(name, (BGWeaponLoadedCallback043)0);
}

// 0x080ebcdc  size=70  FUN_080ebcdc  status=translated  naming=unknown
// Returns the index (low 8 bits) of the first registered weapon whose def field78 == 3, or 0.
unsigned int BG_FindFirstBinocularsWeapon(void)
{
    unsigned int i;

    for (i = 1; ; i++) {
        if (bg_numWeapons < (int)i) {
            return 0;
        }
        if (bg_weaponDefs[i]->weaponType == 3) {
            break;
        }
    }
    return i & 0xff;
}

// 0x080ebd22  size=75  FUN_080ebd22  status=translated  naming=inferred
// Current weapon index for ps: pm_flags & 0x20000 -> first type-3 weapon; else pm_flags & 0x10 ->
// ps->offHandIndex; else ps->weapon.
int BG_GetCurrentWeapon(playerState_t *ps)
{
    if ((ps->pm_flags & 0x20000) == 0) {
        if ((ps->pm_flags & 0x10) == 0) {
            return ps->weapon;
        }
        return ps->offHandIndex;
    }
    return (int)BG_FindFirstBinocularsWeapon();
}

/* ------------------------------------------------------------------------- */
/* Per-player weapon helpers (0x080ebd6e..0x080ed440)                        */
/* ------------------------------------------------------------------------- */

// 0x080ebd6e  size=141  FUN_080ebd6e  status=translated  naming=unknown
// First weapon i (1..count) whose def field84 == weaponClass, that ps owns (bit set) and that has
// non-zero total ammo (ammoclip + ammo). Returns 0 if none.
int BG_FindUsableWeaponOfClass(playerState_t *ps, int weaponClass)
{
    int count = BG_GetNumWeapons();
    int i;

    for (i = 1; ; i++) {
        if (count < i) {
            return 0;
        }
        weaponDef_t *def = BG_GetWeaponDef(i);
        if (def->offhandClass == weaponClass && BitSet_Test(ps->weapons, i) != 0 &&
            BG_GetWeaponTotalAmmo(ps, i) != 0) {
            break;
        }
    }
    return i;
}

// 0x080ebdfc  size=117  FUN_080ebdfc  status=translated  naming=unknown
// Like BG_FindUsableWeaponOfClass but without the ammo test: first owned weapon with field84 == weaponClass.
int BG_FindOwnedWeaponOfClass(playerState_t *ps, int weaponClass)
{
    int count = BG_GetNumWeapons();
    int i;

    for (i = 1; ; i++) {
        if (count < i) {
            return 0;
        }
        weaponDef_t *def = BG_GetWeaponDef(i);
        if (def->offhandClass == weaponClass && BitSet_Test(ps->weapons, i) != 0) {
            break;
        }
    }
    return i;
}

// 0x080ebe72  size=25  FUN_080ebe72  status=translated  naming=unknown
// Returns def->aimDownSight for weaponIndex.
int BG_GetWeaponAimDownSight(int weaponIndex)
{
    return BG_GetWeaponDef(weaponIndex)->aimDownSight;
}

// 0x080ebe8c  size=41  FUN_080ebe8c  status=translated  naming=inferred
// True when 0 <= weaponIndex <= registered weapon count.
int BG_IsValidWeaponIndex(int weaponIndex)
{
    int result = 0;

    if (-1 < weaponIndex) {
        if (weaponIndex <= BG_GetNumWeapons()) {
            result = 1;
        }
    }
    return result;
}

// 0x080ebeb6  size=60  FUN_080ebeb6  status=translated  naming=unknown
// True when the weapon's weaponSlot is 1 or 2.
int BG_IsHeldSlotWeapon(int weaponIndex)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    int result = 0;

    if (def->weaponSlot == 1 || def->weaponSlot == 2) {
        result = 1;
    }
    return result;
}

// 0x080ebef2  size=49  FUN_080ebef2  status=translated  naming=unknown
// True when at least one of ps->weaponslots[1], weaponslots[2] is empty (0).
int PlayerState_HasFreeHeldSlot(playerState_t *ps)
{
    int result = 0;

    if ((signed char)ps->weaponslots[1] == 0 || (signed char)ps->weaponslots[2] == 0) {
        result = 1;
    }
    return result;
}

// 0x080ebf24  size=146  FUN_080ebf24  status=translated  naming=unknown
// True when weaponIndex is valid and owned by ps, except that a def with field84 == 0 that is neither
// in a held slot nor the alt of weaponIndex is reported false.
int BG_PlayerHasWeapon(playerState_t *ps, int weaponIndex)
{
    char valid = (char)BG_IsValidWeaponIndex(weaponIndex);
    char owned = BitSet_Test(ps->weapons, weaponIndex);
    bool result = (owned != '\0' && valid != '\0');
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);

    if (def->offhandClass == 0 && (signed char)ps->weaponslots[1] != weaponIndex && (signed char)ps->weaponslots[2] != weaponIndex &&
        def->altWeaponIndex != weaponIndex) {
        result = false;
    }
    return result;
}

// 0x080ebfb6  size=71  FUN_080ebfb6  status=translated  naming=unknown
// 0 when field7c == 9, else 1 if field84 == 0, else 0.
int BG_WeaponFlagA(int weaponIndex)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);

    if (def->weaponClass == 9) {
        return 0;
    }
    if (def->offhandClass == 0) {
        return 1;
    }
    return 0;
}

// 0x080ebffe  size=468  FUN_080ebffe  status=translated  naming=unknown
// Removes weaponIndex (and its alt-weapon chain) from ps->weapons. If it occupied a held slot, the slot is
// cleared or refilled with another owned slotStackable weapon of the same weaponSlot not already slotted
// (note: the stackable/slot checks read BG_GetWeaponDef(weaponIndex), not weapon i, as compiled).
// Clears ps->weapon if it was the removed weapon. Returns 0 when ps does not own it, else 1.
int BG_TakeWeapon(playerState_t *ps, int weaponIndex)
{
    weaponDef_t *def;
    weaponDef_t *def2;
    int slot;
    int i;

    if (BitSet_Test(ps->weapons, weaponIndex) == '\0') {
        return 0;
    }
    def = BG_GetWeaponDef(weaponIndex);
    slot = BG_FindWeaponSlot(ps, weaponIndex, 1);
    if (slot != 0) {
        if (def->slotStackable == 0) {
            ps->weaponslots[slot] = 0;
        } else {
            for (i = 1; i <= bg_numWeapons; i++) {
                def2 = BG_GetWeaponDef(weaponIndex);
                if (def2->slotStackable != 0 && def2->weaponSlot == def->weaponSlot &&
                    BitSet_Test(ps->weapons, i) != '\0' && BG_FindWeaponSlot(ps, i, 1) == 0) {
                    ps->weaponslots[slot] = (signed char)i;
                    break;
                }
            }
            if (bg_numWeapons < i) {
                ps->weaponslots[slot] = 0;
            }
        }
    }
    BitArray_ClearBit(ps->weapons, weaponIndex);
    i = def->altWeaponIndex;
    while (i != 0 && BitSet_Test(ps->weapons, i) != '\0') {
        BitArray_ClearBit(ps->weapons, i);
        def = BG_GetWeaponDef(i);
        i = def->altWeaponIndex;
    }
    if (weaponIndex == ps->weapon) {
        ps->weapon = 0;
    }
    return 1;
}

// 0x080ec1d2  size=182  FUN_080ec1d2  status=translated  naming=unknown
// Puts an owned weapon with weaponSlot 1 or 2 into held slot 1 or 2. If the slot's previous occupant
// differs from the weapon and from ps->weapon, ps->weapon is cleared. Returns 1 on success, else 0.
int BG_SetHeldSlot(playerState_t *ps, int slot, int weaponIndex)
{
    weaponDef_t *def;

    if (BitSet_Test(ps->weapons, weaponIndex) == '\0') {
        return 0;
    }
    def = BG_GetWeaponDef(weaponIndex);
    if ((unsigned int)(def->weaponSlot - 1) < 2u) {
        if (slot == 1 || slot == 2) {
            if (weaponIndex != (signed char)ps->weaponslots[slot] && weaponIndex != ps->weapon) {
                ps->weapon = 0;
            }
            ps->weaponslots[slot] = (signed char)weaponIndex;
            return 1;
        }
        return 0;
    }
    return 0;
}

// 0x080ec288  size=187  FUN_080ec288  status=translated  naming=unknown
// Finds which held slot (1 or 2) holds weaponIndex, following altWeaponIndex links when followAlt != 0.
// Returns 0 for weapons not owned or with weaponSlot outside 1..2. The walk loops until it returns to the
// start index (it can spin forever on a chain that does not return, as compiled).
int BG_FindWeaponSlot(playerState_t *ps, int weaponIndex, int followAlt)
{
    int cur;
    weaponDef_t *def;

    if (BitSet_Test(ps->weapons, weaponIndex) != '\0') {
        cur = weaponIndex;
        do {
            def = BG_GetWeaponDef(cur);
            if (1u < (unsigned int)(def->weaponSlot - 1)) {
                return 0;
            }
            if ((signed char)ps->weaponslots[1] == cur) {
                return 1;
            }
            if ((signed char)ps->weaponslots[2] == cur) {
                return 2;
            }
            if (followAlt != 0 && def->altWeaponIndex != 0) {
                cur = def->altWeaponIndex;
            }
        } while (cur != weaponIndex);
    }
    return 0;
}

// 0x080ec344  size=89  FUN_080ec344  status=translated  naming=unknown
// For a weapon with weaponSlot 1..2, returns the first empty held slot (1 or 2), else 0.
int BG_FindFreeHeldSlot(playerState_t *ps, int weaponIndex)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);

    if ((unsigned int)(def->weaponSlot - 1) < 2u) {
        if ((signed char)ps->weaponslots[1] == 0) {
            return 1;
        }
        if ((signed char)ps->weaponslots[2] == 0) {
            return 2;
        }
    }
    return 0;
}

// 0x080ec39e  size=271  FUN_080ec39e  status=translated  naming=unknown
// Picks a held slot for a slotStackable weapon: the preferred slot if it is empty or its occupant is
// slotStackable, else slot 1 / slot 2 by the same rule. Returns 0 when slotStackable == 0 or no slot fits.
int BG_PickStackableSlot(playerState_t *ps, int weaponIndex, int preferredSlot)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    weaponDef_t *occupant;

    if (def->slotStackable == 0) {
        return 0;
    }
    if ((unsigned int)(def->weaponSlot - 1) < 2u) {
        if (preferredSlot == 1 || preferredSlot == 2) {
            if ((signed char)ps->weaponslots[preferredSlot] == 0) {
                return preferredSlot;
            }
            occupant = BG_GetWeaponDef((int)(signed char)ps->weaponslots[preferredSlot]);
            if (occupant->slotStackable != 0) {
                return preferredSlot;
            }
        }
        if ((signed char)ps->weaponslots[1] == 0) {
            return 1;
        }
        occupant = BG_GetWeaponDef((int)(signed char)ps->weaponslots[1]);
        if (occupant->slotStackable != 0) {
            return 1;
        }
        if ((signed char)ps->weaponslots[2] == 0) {
            return 2;
        }
        occupant = BG_GetWeaponDef((int)(signed char)ps->weaponslots[2]);
        if (occupant->slotStackable != 0) {
            return 2;
        }
    }
    return 0;
}

// 0x080ec4ae  size=102  FUN_080ec4ae  status=translated  naming=unknown
// True when weaponB is reachable from weaponA by following altWeaponIndex links (stops on a cycle back to A).
int BG_IsAltWeaponInChain(int weaponA, int weaponB)
{
    int cur = BG_GetWeaponDef(weaponA)->altWeaponIndex;

    for (;;) {
        if (cur == 0) {
            return 0;
        }
        if (cur == weaponB) {
            break;
        }
        if (cur == weaponA) {
            return 0;
        }
        cur = BG_GetWeaponDef(cur)->altWeaponIndex;
    }
    return 1;
}

// 0x080ec514  size=497  FUN_080ec514  status=translated  naming=unknown
// Remaining capacity for the weapon's ammo: for weapons without a shared cap (sharedAmmoCapIndex < 0)
// it is the type max minus current ammo (field340 == 0) or clip size minus current clip. For shared caps
// it is the cap minus the ammo/clip counts of every owned weapon with that cap index, counting each ammo
// or clip type once.
int BG_GetWeaponAmmoRoom(playerState_t *ps, int weaponIndex)
{
    int seenClip[128];
    int seenAmmo[128];
    int room;
    int i;
    int ammoIdx;
    int clipIdx;
    weaponDef_t *def;
    weaponDef_t *other;

    memset(seenAmmo, 0, 0x200);
    memset(seenClip, 0, 0x200);
    def = BG_GetWeaponDef(weaponIndex);
    ammoIdx = BG_GetWeaponAmmoIndex(weaponIndex);
    clipIdx = BG_GetWeaponClipIndex(weaponIndex);
    if (def->iSharedAmmoCapIndex < 0) {
        if (BG_GetWeaponClipOnly(weaponIndex) == 0) {
            room = BG_GetAmmoTypeMax(ammoIdx);
            room = room - ps->ammo[ammoIdx];
        } else {
            room = BG_GetClipTypeSize(clipIdx);
            room = room - ps->ammoclip[clipIdx];
        }
    } else {
        room = BG_GetSharedAmmoCapMax(def->iSharedAmmoCapIndex);
        for (i = 1; i <= bg_numWeapons; i++) {
            if (BitSet_Test(ps->weapons, i) != '\0' &&
                (other = BG_GetWeaponDef(i), other->iSharedAmmoCapIndex == def->iSharedAmmoCapIndex)) {
                if (BG_GetWeaponClipOnly(i) == 0) {
                    if (seenAmmo[BG_GetWeaponAmmoIndex(i)] == 0) {
                        seenAmmo[BG_GetWeaponAmmoIndex(i)] = 1;
                        room = room - ps->ammo[BG_GetWeaponAmmoIndex(i)];
                    }
                } else {
                    if (seenClip[BG_GetWeaponClipIndex(i)] == 0) {
                        seenClip[BG_GetWeaponClipIndex(i)] = 1;
                        room = room - ps->ammoclip[BG_GetWeaponClipIndex(i)];
                    }
                }
            }
        }
    }
    return room;
}

// 0x080ec8cc  size=246  FUN_080ec8cc  status=translated  naming=unknown
// Interpolates two value pairs of the weapon def by ps->viewHeightCurrent: above 40 it blends fields
// 0x28c->0x290 and 0x298->0x29c over (viewHeight-40)/20; otherwise 0x290->0x294 and 0x29c->0x2a0 over
// (viewHeight-11)/29. Outputs go to *outA (first pair) and *outB (second pair).
void BG_InterpolateWeaponStanceValues(playerState_t *ps, int weaponIndex, float *outA, float *outB)
{
    weaponDef_t *def = BG_GetWeaponDef(weaponIndex);
    float t;

    if (40.0f < ps->viewHeightCurrent) {
        t = (ps->viewHeightCurrent - 40.0f) / 20.0f;
        *outA = (def->hipSpreadStandMin - def->hipSpreadDuckedMin) * t + def->hipSpreadDuckedMin;
        *outB = (def->hipSpreadMax - def->hipSpreadDuckedMax) * t + def->hipSpreadDuckedMax;
    } else {
        t = (ps->viewHeightCurrent - 11.0f) / 29.0f;
        *outA = (def->hipSpreadDuckedMin - def->hipSpreadProneMin) * t + def->hipSpreadProneMin;
        *outB = (def->hipSpreadDuckedMax - def->hipSpreadProneMax) * t + def->hipSpreadProneMax;
    }
}

// 0x080ec9c2  size=39  FUN_080ec9c2  status=translated  naming=unknown
// True when state is 0x13 or 0x14.
int Weapon_StateIsBinocularsRaiseOrHold(int state)
{
    int result = 0;

    if (state == 0x13 || state == 0x14) {
        result = 1;
    }
    return result;
}

// 0x080ec9ea  size=292  FUN_080ec9ea  status=translated  naming=unknown
// Decides from ps->pm_type, pml->field34, the current weapon's field32c and ps->weaponstate whether the
// weapon-zoom control may be active: false for pm_type 1 with pml->field34 set, for pm_type 2..7, for
// weapons with field32c == 0, and for weaponstate 1,2,10..16 and 17..22 except 19/20.
int PM_WeaponStateAllowsAds(playerState_t *ps, pml_t *pml)
{
    int type = ps->pm_type;
    weaponDef_t *def;
    int result;

    if (type == 1) {
        if (pml->almostGroundPlane != 0) {
            return 0;
        }
    } else if (0 < type && type < 8) {
        return 0;
    }
    def = BG_GetWeaponDef(BG_GetCurrentWeapon(ps));
    if (def->aimDownSight == 0) {
        result = 0;
    } else if (ps->weaponstate < 0xc || 0x10 < ps->weaponstate) {
        if (ps->weaponstate == 10 || ps->weaponstate == 0xb) {
            result = 0;
        } else if (ps->weaponstate == 1 || ps->weaponstate == 2) {
            result = 0;
        } else {
            if (0x10 < ps->weaponstate && ps->weaponstate < 0x17 &&
                (char)Weapon_StateIsBinocularsRaiseOrHold(ps->weaponstate) == '\0') {
                return 0;
            }
            result = 1;
        }
    } else {
        result = 0;
    }
    return result;
}

// 0x080ecb0e  size=51  FUN_080ecb0e  status=translated  naming=unknown
// True when ps->weaponstate is 0x13 or 0x14.
int PM_WeaponStateIsBinocularsRaiseOrHold(playerState_t *ps)
{
    int result = 0;

    if (ps->weaponstate == 0x13 || ps->weaponstate == 0x14) {
        result = 1;
    }
    return result;
}

// 0x080ecb42  size=303  FUN_080ecb42  status=translated  naming=unknown
// Recomputes pm_flags bit 0x40 (cleared first) from cmd button 0x1000, the weapon state, pm_type rules and
// previous-cmd/axis state, also setting 0x800 in one case; then reports the result through
// BG_UpdateConditionValue(clientNum, 7, bit40, 1).
void PM_UpdateAdsFlag(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    char allowed;
    bool wanted;

    ps->pm_flags = ps->pm_flags & 0xffffffbfu;
    allowed = (char)PM_WeaponStateAllowsAds(ps, pml);
    wanted = false;
    if ((pm->cmd.buttons & 0x1000) != 0 || (char)PM_WeaponStateIsBinocularsRaiseOrHold(ps) != '\0') {
        wanted = true;
    }
    if (wanted && allowed != '\0') {
        if ((ps->pm_flags & 1) == 0) {
            ps->pm_flags = ps->pm_flags | 0x40;
        } else if ((pm->oldcmd.buttons & 0x1000) == 0 || (pm->cmd.forwardmove == 0 && pm->cmd.rightmove == 0)) {
            ps->pm_flags = ps->pm_flags | 0x40;
            ps->pm_flags = ps->pm_flags | 0x800;
        }
    }
    if ((ps->pm_flags & 0x40) == 0) {
        BG_UpdateConditionValue(ps->clientNum, 7, 0, 1);
    } else {
        BG_UpdateConditionValue(ps->clientNum, 7, 1, 1);
    }
}

// 0x080ecc72  size=42  FUN_080ecc72  status=translated  naming=unknown
// Calls PM_AddEvent(ps, 0x95) and clears pm_flags bit 0x40.
void PM_ClearAdsFlag(playerState_t *ps)
{
    PM_AddEvent(ps, 0x95);
    ps->pm_flags = ps->pm_flags & 0xffffffbfu;
}

// 0x080ecc9c  size=719  FUN_080ecc9c  status=translated  naming=inferred
// Per-frame update of ps->weaponPosFrac (0..1): moves toward 1 when the zoom condition holds (pm_flags
// bit 0x40 and weaponstate rules, a dvar-timed hold via ps->adsDelayTime) and toward 0 otherwise, at the
// weapon def's per-ms rates field588 / field58c, clamped with Float_Clamp. A dvar/field128/field278
// combination instead resets the state (PM_ClearAdsFlag, fraction and timer 0). Name inferred from
// the 0..1 fraction with separate rise/fall rates; the gameplay meaning is not established.
void PM_UpdateWeaponPosFrac(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    weaponDef_t *def = BG_GetWeaponDef(BG_GetCurrentWeapon(ps));
    bool active;

    if (dvar_player_scopeExitOnDamage->current.enabled == '\0' || ps->damageCount == 0 || def->adsOverlayReticle == 0) {
        if (def->aimDownSight == 0) {
            ps->fWeaponPosFrac = 0.0f;
            ps->adsDelayTime = 0;
        } else {
            active = false;
            if (((def->segmentedReload == 0 && ps->weaponstate == 5 && 0 < ps->weaponTime - def->adsReloadTransTime) ||
                 (def->segmentedReload != 0 &&
                  (ps->weaponstate == 5 || ps->weaponstate == 6 || ps->weaponstate == 7 ||
                   ps->weaponstate == 8 ||
                   (ps->weaponstate == 9 && 0 < ps->weaponTime - def->adsReloadTransTime)))) ||
                (def->rechamberWhileAds == 0 && ps->weaponstate == 4)) {
                active = false;
            } else if ((ps->pm_flags & 0x40) != 0) {
                active = true;
            }
            if (def->adsFire != 0 && ps->weaponDelay != 0 && ps->weaponstate == 3) {
                active = true;
            }
            if (ps->fWeaponPosFrac != 1.0f || active || dvar_player_adsExitDelay->current.integer < 1) {
                ps->adsDelayTime = 0;
            } else {
                if (ps->adsDelayTime == 0) {
                    ps->adsDelayTime = dvar_player_adsExitDelay->current.integer + pm->cmd.serverTime;
                }
                if (pm->cmd.serverTime < ps->adsDelayTime) {
                    active = true;
                } else {
                    ps->adsDelayTime = 0;
                }
            }
            if ((active && ps->fWeaponPosFrac != 1.0f) || (!active && ps->fWeaponPosFrac != 0.0f)) {
                if (active) {
                    ps->fWeaponPosFrac = ps->fWeaponPosFrac + (float)pml->msec * def->adsTransInTimeInv;
                } else {
                    ps->fWeaponPosFrac = ps->fWeaponPosFrac - (float)pml->msec * def->adsTransOutTimeInv;
                }
                ps->fWeaponPosFrac = Float_Clamp(ps->fWeaponPosFrac, 0.0f, 1.0f);
            }
        }
    } else {
        PM_ClearAdsFlag(ps);
        ps->fWeaponPosFrac = 0.0f;
        ps->adsDelayTime = 0;
    }
}

// 0x080ecf6c  size=23  FUN_080ecf6c  status=translated  naming=unknown
// Sets pm_flags bit 0x4000.
void PM_SetHoldingBreathFlag(playerState_t *ps)
{
    ps->pm_flags = ps->pm_flags | 0x4000;
}

// 0x080ecf84  size=23  FUN_080ecf84  status=translated  naming=unknown
// Clears pm_flags bit 0x4000.
void PM_ClearHoldingBreathFlag(playerState_t *ps)
{
    ps->pm_flags = ps->pm_flags & 0xffffbfffu;
}

// 0x080ecf9c  size=232  FUN_080ecf9c  status=translated  naming=unknown
// When weaponPosFrac == 1.0, the current weapon def has field278 != 0 and field7c != 9: if
// field_5b8 is below (int)(dvar_0c*1000) it is advanced by (int)(dvar_14*1000) and re-clamped to the
// first value; PM_ClearHoldingBreathFlag is called.
void PM_ApplyBreathFireDelay(playerState_t *ps)
{
    weaponDef_t *def = BG_GetWeaponDef(BG_GetCurrentWeapon(ps));
    int limit;

    if (ps->fWeaponPosFrac == 1.0f && def->adsOverlayReticle != 0 && def->weaponClass != 9) {
        limit = (int)(dvar_player_breath_hold_time->current.value * 1000.0f);
        if (ps->holdBreathTimer < limit) {
            ps->holdBreathTimer = (int)(dvar_player_breath_fire_delay->current.value * 1000.0f) + ps->holdBreathTimer;
            if (limit < ps->holdBreathTimer) {
                ps->holdBreathTimer = limit;
            }
        }
        PM_ClearHoldingBreathFlag(ps);
    }
}

// 0x080ed084  size=601  FUN_080ed084  status=translated  naming=unknown
// Updates the ps->holdBreathTimer timer (ms) and ps->holdBreathScale float: pm_flags bit 0x4000 is set when cmd button
// 0x8000 is held with the timer at 0 (and the weapon conditions hold), cleared otherwise; the timer
// counts up while set (reset to dvar_10*1000 + dvar_0c*1000 and the flag cleared once above dvar_0c*1000)
// and down otherwise; field_5b4 then eases toward a target scale via Float_ApproachProportional. With dvar_0c*1000 <= 0
// it resets field_5b4 = 1.0 and the timer to 0.
void PM_UpdateHoldBreath(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps = pm->ps;
    weaponDef_t *def = BG_GetWeaponDef(BG_GetCurrentWeapon(ps));
    int limit = (int)(dvar_player_breath_hold_time->current.value * 1000.0f);
    int extra = (int)(dvar_player_breath_gasp_time->current.value * 1000.0f);
    float scale;
    float rate;

    if (limit < 1) {
        PM_ClearHoldingBreathFlag(ps);
        ps->holdBreathScale = 1.0f;
        ps->holdBreathTimer = 0;
    } else {
        if (ps->fWeaponPosFrac != 1.0f || def->adsOverlayReticle == 0 || def->weaponClass == 9) {
            PM_ClearHoldingBreathFlag(ps);
        } else if ((pm->cmd.buttons & 0x8000) == 0) {
            PM_ClearHoldingBreathFlag(ps);
        } else if (ps->holdBreathTimer == 0) {
            PM_SetHoldingBreathFlag(ps);
        }
        if ((ps->pm_flags & 0x4000) == 0) {
            ps->holdBreathTimer = ps->holdBreathTimer - pml->msec;
        } else {
            ps->holdBreathTimer = pml->msec + ps->holdBreathTimer;
        }
        if (ps->holdBreathTimer < 0) {
            ps->holdBreathTimer = 0;
        }
        if ((ps->pm_flags & 0x4000) != 0 && limit < ps->holdBreathTimer) {
            ps->holdBreathTimer = extra + limit;
            PM_ClearHoldingBreathFlag(ps);
        }
        if ((ps->pm_flags & 0x4000) == 0) {
            scale = (dvar_player_breath_gasp_scale->current.value - 1.0f) * ((float)ps->holdBreathTimer / (float)(extra + limit)) + 1.0f;
            rate = dvar_player_breath_gasp_lerp->current.value;
        } else {
            scale = 0.0f;
            rate = dvar_player_breath_hold_lerp->current.value;
        }
        ps->holdBreathScale = Float_ApproachProportional((scale - 1.0f) * ps->fWeaponPosFrac + 1.0f, ps->holdBreathScale, rate, pml->frametime);
    }
}

// 0x080ed2de  size=207  FUN_080ed2de  status=translated  naming=unknown
// Returns 1 for weaponstate 0,1,2,4..9; 0 for weaponstate 3, 11 and 12..16; for any other state calls
// PM_Weapon_ForceReady(ps) and returns 1.
int PM_WeaponStateHandler(playerState_t *ps)
{
    int state = ps->weaponstate;

    if (state == 0 || state == 1 || state == 2 || state == 5 || state == 7 || state == 9 || state == 8 ||
        state == 6 || state == 4) {
        return 1;
    }
    if (state == 3 || state == 0xb || (0xb < state && state < 0x11)) {
        return 0;
    }
    PM_Weapon_ForceReady(ps);
    return 1;
}

// 0x080ed3ae  size=25  FUN_080ed3ae  status=translated  naming=inferred
// Returns the weapon's clipIndex (def + 0x1d0).
int BG_GetWeaponClipIndex(int weaponIndex)
{
    return BG_GetWeaponDef(weaponIndex)->iClipIndex;
}

// 0x080ed3c8  size=25  FUN_080ed3c8  status=translated  naming=inferred
// Returns the weapon's ammoIndex (def + 0x1c8).
int BG_GetWeaponAmmoIndex(int weaponIndex)
{
    return BG_GetWeaponDef(weaponIndex)->iAmmoIndex;
}

// 0x080ed3e2  size=25  FUN_080ed3e2  status=translated  naming=unknown
// Returns def->field340 (0 = the weapon draws from the ammo pool, otherwise from the clip pool).
int BG_GetWeaponClipOnly(int weaponIndex)
{
    return BG_GetWeaponDef(weaponIndex)->clipOnly;
}

// 0x080ed3fc  size=67  FUN_080ed3fc  status=translated  naming=inferred
// Total ammo carried for weaponIndex: ps->ammoclip[clipIndex] + ps->ammo[ammoIndex].
int BG_GetWeaponTotalAmmo(playerState_t *ps, int weaponIndex)
{
    int ammoIdx = BG_GetWeaponAmmoIndex(weaponIndex);
    int clipIdx = BG_GetWeaponClipIndex(weaponIndex);

    return ps->ammoclip[clipIdx] + ps->ammo[ammoIdx];
}

// 0x080ed440  size=383  FUN_080ed440  status=translated  naming=unknown
// Moves ammo from the ammo pool to the clip for ps->weapon: amount = min(ammo, clipSize - clip), capped to
// def->reloadStartAdd for weaponstate 7/8 (only when below the clip size) or to def->reloadAmmoAdd otherwise (when
// non-zero and below the clip size). Skips weaponstate 7/8 when field364 == 0.
void BG_ReloadClipFromAmmo(playerState_t *ps)
{
    weaponDef_t *def = BG_GetWeaponDef(ps->weapon);
    int ammoIdx;
    int clipIdx;
    int ammo;
    int clip;
    int amount;

    if ((ps->weaponstate != 7 && ps->weaponstate != 8) || def->reloadStartAdd != 0) {
        ammoIdx = BG_GetWeaponAmmoIndex(ps->weapon);
        clipIdx = BG_GetWeaponClipIndex(ps->weapon);
        ammo = ps->ammo[ammoIdx];
        clip = ps->ammoclip[clipIdx];
        amount = BG_GetClipTypeSize(clipIdx);
        amount = amount - clip;
        if (ammo < amount) {
            amount = ammo;
        }
        if (ps->weaponstate == 7 || ps->weaponstate == 8) {
            if (def->reloadStartAdd < BG_GetClipTypeSize(clipIdx) && def->reloadStartAdd < amount) {
                amount = def->reloadStartAdd;
            }
        } else if (def->reloadAmmoAdd != 0 && def->reloadAmmoAdd < BG_GetClipTypeSize(clipIdx) && def->reloadAmmoAdd < amount) {
            amount = def->reloadAmmoAdd;
        }
        if (amount != 0) {
            ps->ammo[ammoIdx] = ps->ammo[ammoIdx] - amount;
            ps->ammoclip[clipIdx] = amount + ps->ammoclip[clipIdx];
        }
    }
}
