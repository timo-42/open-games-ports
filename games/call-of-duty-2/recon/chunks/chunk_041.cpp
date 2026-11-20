/*
 * Chunk 041 (cod2_lnxded 1.3, 0x080dfb76..0x080e3eb0): player movement (pmove),
 * playerState-to-entityState conversion, trace helpers and vector math.
 *
 * Conventions used throughout (from the disassembly):
 *   - ROUND(x) in the pseudocode is lowered to fistp with the x87 control word
 *     set to round-toward-zero (0xc00), so it is a plain C (int) cast here.
 *   - Float literals written as raw words (0x3f800000, 0x80000000, ...) are
 *     written as their float values; 0x80000000 is -0.0f.
 *   - The trace callback (g_pmTraceDispatch[traceMode].trace) takes the trace, start,
 *     mins, maxs, end, passEntity and content mask in that order.
 */
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
#include "chunk_041_types.h"

/* ---- in-chunk prototypes (definitions below, in address order) ---- */
/* Defined in chunk 043 (signature per chunk_043.json, canonical types). */
void PM_StepSlideMove(pmove_t *pm, pml_t *pml, int gravity);
void PM_NudgeDirectionAgainstNormal(const float *dir, const float *normal, float *out);
void BG_AddPredictableEventToPlayerstate(uint32 newEvent, uint8 eventParm, playerState_t *ps);
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *s, int snap, uint32 notifyRow);
int PM_ProbeClimbableLedge(int passEntity, const float *origin, float halfWidth, float height,
                           float yaw, float *outTorsoHeight, float *outAngleA, float *outAngleB,
                           int skipFirstTrace, int checkDuring, const float *groundNormal,
                           uint8 traceMode, int altMaskSelect, float probeHeight);
int PM_ProbeClimbableLedgeEntry(int passEntity, const float *origin, float halfWidth, float height,
                                float yaw, float *outTorsoHeight, float *outAngleA, float *outAngleB,
                                int skipFirstTrace, int checkDuring, const float *groundNormal,
                                uint8 traceMode, int altMaskSelect, float probeHeight);
void LerpColorOverTime(const cod2_color_fade_t *fade, int now, uint8 *out);
void VectorClear(float *v);
void VectorSet(float *v, float x, float y, float z);
void VectorCopy(const float *src, float *dst);
void VectorNegate(const float *src, float *dst);
void VectorAdd_080e10f8(const float *a, const float *b, float *out);
void VectorSubtract_080e113c(const float *a, const float *b, float *out);
void VectorScale_080e1180(const float *v, float scale, float *out);
void VectorMA_080e11b8(const float *v, float scale, const float *dir, float *out);
long double DotProduct_080e120c(const float *a, const float *b);
float VectorLength_080e1240(const float *v);
float FloatSqrt(float x);
void VectorLerp(const float *a, const float *b, float frac, float *out);
void PM_TraceRaw(pmove_t *pm, trace_t *trace, const float *start, const float *mins,
                 const float *maxs, const float *end, int passEntity, int contentMask);
void PM_Trace(pmove_t *pm, trace_t *trace, const float *start, const float *mins,
              const float *maxs, const float *end, int passEntity, int contentMask);
void PM_AddEvent(playerState_t *ps, int event);
void PM_AddTouchEnt(pmove_t *pm, int entityNum);
void PM_ClipVelocity(const float *in, const float *normal, float *out);
int PM_StanceIndex(const playerState_t *ps);
void PM_Friction(playerState_t *ps, pml_t *pml);
int PM_ShouldClampAcceleration(playerState_t *ps, float accel, const float *dir);
float PM_ClampAcceleration(playerState_t *ps, float accel, const float *dir);
void PM_Accelerate(playerState_t *ps, pml_t *pml, const float *wishdir, float wishspeed, float accel);
float PM_MaxAxisCmdScale(const playerState_t *ps, float x, float y, float z);
float PM_CmdScale2D(const playerState_t *ps, const usercmd_t *cmd);
float PM_CmdScale(pmove_t *pm, const usercmd_t *cmd);
float PM_PmTimeScale(int pmTime);
void PM_SetMovementDir(pmove_t *pm, pml_t *pml);
int PM_GroundSurfaceType(const pml_t *pml);
void PM_FreeMove(pmove_t *pm, pml_t *pml);
void PM_AirMove(pmove_t *pm, pml_t *pml);
void PM_WalkMove(pmove_t *pm, pml_t *pml);
void PM_ReduceGroundSpeed(playerState_t *ps, pml_t *pml);
void PM_FlyMove(pmove_t *pm, pml_t *pml);
void PM_ClimbMove(pmove_t *pm, pml_t *pml);
int PM_FootstepSoundIndex(const playerState_t *ps, const pml_t *pml);
int PM_GroundSoundIndexPlus18(const pml_t *pml);
int PM_GroundSoundIndexPlus1(const pml_t *pml);
int PM_GroundSoundIndexPlus5D(const pml_t *pml);
int PM_GroundSoundIndexPlus74(const pml_t *pml);
void PM_CrashLand(playerState_t *ps, pml_t *pml);
int PM_CorrectAllSolid(pmove_t *pm, pml_t *pml, trace_t *trace);
void PM_GroundProbe(pmove_t *pm, pml_t *pml);
void PM_GroundTrace(pmove_t *pm, pml_t *pml);
int PM_StanceDurationMs(const playerState_t *ps, int stanceTarget, int stanceToggled);
float PM_EvalStanceCurve(int percent, const cod2_pm_curve_key_s *table, float *scalarOut);
float PM_StanceTransitionProgress(const pmove_t *pm, int fromStance, int toStance);
void PM_UpdateStanceHeight(pmove_t *pm, pml_t *pml);

/* ======================================================================
 * 0x080dfb76  size=258  PM_NudgeDirectionAgainstNormal
 * naming=unknown: no string or callee names it. Negates the input direction,
 * then nudges it toward the normal (or +Z for flat surfaces) until the dot
 * product reaches a threshold (0.3, or 0.7 when normal.z > 0.8).
 * ====================================================================== */
void PM_NudgeDirectionAgainstNormal(const float *dir, const float *normal, float *out)
{
    float adjusted[3];
    float baseDir[3];
    float threshold;
    long double dot;

    threshold = 0.3f;
    if ((long double)VectorLength_080e1240(normal) < (long double)1) {
        VectorSet(baseDir, 0.0f, 0.0f, 1.0f);
    } else {
        VectorCopy(normal, baseDir);
    }
    VectorNegate(dir, adjusted);
    Vec3_Normalize(adjusted);
    if (0.8 < (double)normal[2]) {
        threshold = 0.7f;
    }
    for (;;) {
        dot = DotProduct_080e120c(adjusted, baseDir);
        if (!(dot < (long double)threshold)) {
            break;
        }
        VectorMA_080e11b8(adjusted, 0.5f, baseDir, adjusted);
        Vec3_Normalize(adjusted);
    }
    VectorCopy(adjusted, out);
}

/* ======================================================================
 * 0x080dfc78  size=74  BG_AddPredictableEventToPlayerstate
 * naming=inferred: ring of four events/eventParms indexed by eventSequence & 3,
 * same shape as the id Tech 3 counterpart of this name. Event 0 is ignored.
 * ====================================================================== */
void BG_AddPredictableEventToPlayerstate(uint32 newEvent, uint8 eventParm, playerState_t *ps)
{
    if (newEvent != 0) {
        ps->events[ps->eventSequence & 3] = (int32)(newEvent & 0xff);
        ps->eventParms[ps->eventSequence & 3] = (int32)eventParm;
        ps->eventSequence = ps->eventSequence + 1;
    }
}

/* ======================================================================
 * 0x080dfcc2  size=1330  BG_PlayerStateToEntityState
 * naming=inferred: copies a playerState into an entityState (origin and view
 * angles with optional snap-to-integer, flags, movement direction, stance
 * blend, event ring) and replays not-yet-notified events through the
 * dispatch row notifyRow (g_pmTraceDispatch[notifyRow].notify).
 * ====================================================================== */
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *s, int snap, uint32 notifyRow)
{
    float stanceBlend;
    float blendScale;
    int i;
    int tableIndex;
    int duration;
    uint32 eventSeq;
    uint8 eventValue;
    int listIndex;

    if ((ps->pm_flags & 0xc00000U) == 0) {
        s->eType = 5;
    } else {
        s->eType = 1;
    }
    s->pos.trType = 1;
    VectorCopy(ps->origin, s->pos.trBase);
    if (snap != 0) {
        for (i = 0; i < 3; i++) {
            s->pos.trBase[i] = (float)(int)s->pos.trBase[i];
        }
    }
    s->apos.trType = 1;
    VectorCopy(ps->viewangles, s->apos.trBase);
    if (snap != 0) {
        for (i = 0; i < 3; i++) {
            s->apos.trBase[i] = (float)(int)s->apos.trBase[i];
        }
    }
    s->angles2[1] = (float)ps->movementDir;
    s->legsAnim = ps->legsAnim;
    s->torsoAnim = ps->torsoAnim;
    s->clientNum = ps->clientNum;
    s->eFlags = (uint32)ps->eFlags;
    if ((ps->eFlags & 0x300U) != 0) {
        s->otherEntityNum = ps->viewlocked_entNum;
    }
    if (ps->pm_type < 6) {
        s->eFlags = s->eFlags & 0xfffdffffU;
    } else {
        s->eFlags = s->eFlags | 0x20000U;
    }
    if ((ps->pm_flags & 0x40U) == 0) {
        s->eFlags = s->eFlags & 0xfffbffffU;
    } else {
        s->eFlags = s->eFlags | 0x40000U;
    }
    *(int32 *)&s->leanf = *(const int32 *)&ps->leanf; /* raw bit copy (entityState +0xd4 / playerState +0x4c) */
    s->groundEntityNum = (uint16)ps->groundEntityNum;

    tableIndex = PM_StanceIndex(ps);
    if (tableIndex == 1) {
        if (ps->viewHeightLerpTime == 0) {
            stanceBlend = 1.0f;
        } else {
            duration = PM_StanceDurationMs(ps, ps->viewHeightLerpTarget, ps->viewHeightLerpDown);
            stanceBlend = (float)(ps->commandTime - ps->viewHeightLerpTime) / (float)duration;
            if (stanceBlend < 0.0f) {
                stanceBlend = 0.0f;
            } else if (1.0f < stanceBlend) {
                stanceBlend = 1.0f;
            }
            if (ps->viewHeightLerpDown == 0) {
                stanceBlend = 1.0f - stanceBlend;
            }
        }
        s->fTorsoHeight = (float)ps->fTorsoHeight * stanceBlend;
        blendScale = ps->fTorsoPitch * stanceBlend;
        s->fTorsoPitch = blendScale;
        blendScale = ps->fWaistPitch * stanceBlend;
        s->fWaistPitch = blendScale;
    } else {
        s->fTorsoHeight = 0.0f;
        s->fTorsoPitch = 0.0f;
        s->fWaistPitch = 0.0f;
    }

    if (ps->entityEventSequence - ps->eventSequence < 0) {
        if (4 < ps->eventSequence - ps->entityEventSequence) {
            ps->entityEventSequence = ps->eventSequence - 4;
        }
        s->eventParm = (uint32)(uint8)ps->eventParms[ps->entityEventSequence & 3];
        ps->entityEventSequence = ps->entityEventSequence + 1;
    } else {
        s->eventParm = 0;
    }

    for (eventSeq = (uint32)ps->oldEventSequence; eventSeq != (uint32)ps->eventSequence;
         eventSeq = eventSeq + 1) {
        eventValue = (uint8)ps->events[eventSeq & 3];
        if (g_pmTraceDispatch[notifyRow & 0xff].notify != 0) {
            g_pmTraceDispatch[notifyRow & 0xff].notify(s->number, (int)eventValue);
        }
        for (listIndex = 0; 0 < g_pmLocalOnlyEventIds[listIndex] && (uint32)g_pmLocalOnlyEventIds[listIndex] != (uint32)eventValue;
             listIndex++) {
        }
        if (g_pmLocalOnlyEventIds[listIndex] < 0) {
            s->events[s->eventSequence & 3] = (int32)eventValue;
            s->eventParms[s->eventSequence & 3] = (int32)(uint8)ps->eventParms[eventSeq & 3];
            s->eventSequence = s->eventSequence + 1;
        }
    }
    ps->oldEventSequence = ps->eventSequence;
    s->weapon = (int32)(uint8)ps->weapon;
}

/* ======================================================================
 * 0x080e01f4  size=3171  PM_ProbeClimbableLedge
 * naming=unknown: a ledge/step probe. Traces down from above the origin,
 * then forward at +48 and at several heights, using the fraction, the hit
 * normal (ground slope >= 0.7) and startsolid. Returns 1 when the probe finds
 * a climbable surface, 0 otherwise. Output fields are set when non-null.
 * ====================================================================== */
int PM_ProbeClimbableLedge(int passEntity, const float *origin, float halfWidth, float height,
                           float yaw, float *outTorsoHeight, float *outAngleA, float *outAngleB,
                           int skipFirstTrace, int checkDuring, const float *groundNormal,
                           uint8 traceMode, int altMaskSelect, float probeHeight)
{
    cod2_pm_trace_fn traceFn;
    trace_t trace;
    int mask;
    float mins[3];
    float maxs[3];
    float start[3];
    float end[3];
    float fwd[3];
    float originCopy[3];
    float lerpA[3];
    float lerpB[3];
    float diff[3];
    float ledgeDist;
    float fVar2;
    float slope;
    long double lenA;
    long double lenB;
    int hasEdge;
    int ok;
    int result;

    traceFn = g_pmTraceDispatch[traceMode].trace;
    hasEdge = 0;
    ok = 1;
    result = 0;

    VectorSet(mins, -halfWidth, -halfWidth, 0.0f);
    VectorAdd_080e10f8(mins, origin, mins);
    VectorSet(maxs, halfWidth, halfWidth, height);
    VectorAdd_080e10f8(maxs, origin, maxs);
    mask = (altMaskSelect == 0) ? 0x810011 : 0x820011;

    if (skipFirstTrace == 0) {
        VectorSet(mins, -halfWidth, -halfWidth, 0.0f);
        VectorSet(maxs, halfWidth, halfWidth, height);
        VectorCopy(origin, start);
        VectorCopy(origin, end);
        end[2] = end[2] + 10.0f;
        traceFn(&trace, start, mins, maxs, end, passEntity, mask);
        if (trace.allsolid != 0) {
            return 0;
        }
    }
    if (checkDuring != 0 && groundNormal != 0 && (double)groundNormal[2] < 0.7) {
        return 0;
    }

    VectorSet(mins, -6.0f, -6.0f, -6.0f);
    VectorSet(maxs, 6.0f, 6.0f, 6.0f);
    VectorSet(end, 0.0f, (float)(yaw - 180.0), 0.0f);
    Angles_ToVectors(end, fwd, start, lerpA);
    height = (float)(height - 6.0);
    VectorCopy(origin, start);
    start[2] = start[2] + height;
    VectorMA_080e11b8(start, (float)(probeHeight - 6.0), fwd, end);
    traceFn(&trace, start, mins, maxs, end, passEntity, mask);

    if (trace.fraction < 1.0f) {
        if (checkDuring == 0) {
            return 0;
        }
        hasEdge = 1;
        ledgeDist = (float)((probeHeight - 6.0) * trace.fraction + 6.0);
        if (ledgeDist < halfWidth + 2.0f) {
            return 0;
        }
        if (ledgeDist < (double)height * 0.7 + 48.0) {
            hasEdge = 0;
            end[2] = end[2] + 22.0f;
            VectorSubtract_080e113c(end, start, diff);
            lenA = (long double)Vec3_NormalizeCopy(diff, fwd);
            traceFn(&trace, start, mins, maxs, end, passEntity, mask);
            if (trace.fraction < 1.0f) {
                hasEdge = 1;
                ledgeDist = (float)(trace.fraction * (float)lenA + 6.0);
                if (ledgeDist < (double)height * 0.7 + 48.0) {
                    return 0;
                }
            } else {
                ledgeDist = probeHeight;
            }
        }
    } else {
        ledgeDist = probeHeight;
    }

    VectorLerp(start, end, trace.fraction, lerpA);
    VectorMA_080e11b8(origin, 48.0f, fwd, start);
    start[2] = start[2] + height;
    VectorCopy(start, end);
    end[2] = (float)(end[2] - ((halfWidth * 2.5 + height) - 6.0));
    traceFn(&trace, start, mins, maxs, end, passEntity, mask);

    do {
    if (trace.fraction != 1.0f) {
        if (trace.normal[2] < 0.7f) {
            return 0;
        }
        fVar2 = (float)(((halfWidth * 2.5 + height) - 6.0) * trace.fraction + 6.0);
        VectorLerp(start, end, trace.fraction, lerpB);
        lerpB[2] = lerpB[2] - 6.0f;
        if (hasEdge != 0) {
            if ((double)(ledgeDist - fVar2) < (double)fVar2 * -0.75) {
                break;
            }
            VectorSubtract_080e113c(lerpA, lerpB, diff);
            VectorMA_080e11b8(diff, 6.0f, fwd, diff);
            diff[2] = diff[2] + 6.0f;
            Vec3_Normalize(diff);
            VectorMA_080e11b8(start, (float)((probeHeight - 6.0) - 48.0), diff, end);
            end[0] = (float)(((probeHeight - 6.0) * fwd[0] + origin[0] + end[0]) * 0.5);
            end[1] = (float)(((probeHeight - 6.0) * fwd[1] + origin[1] + end[1]) * 0.5);
            traceFn(&trace, start, mins, maxs, end, passEntity, mask);
            if (trace.fraction < 1.0f) {
                VectorLerp(start, end, trace.fraction, start);
                start[2] = start[2] + 18.0f;
                end[2] = end[2] + 18.0f;
                traceFn(&trace, start, mins, maxs, end, passEntity, mask);
                if (trace.fraction < 1.0f) {
                    break;
                }
            }
            VectorLerp(start, end, trace.fraction, lerpA);
        }
        VectorCopy(lerpA, start);
        VectorCopy(start, end);
        end[2] = end[2] - ((start[2] - lerpB[2]) + (start[2] - lerpB[2]) + halfWidth);
        traceFn(&trace, start, mins, maxs, end, passEntity, mask);
        if (trace.fraction != 1.0f) {
            if (trace.normal[2] < 0.7f) {
                return 0;
            }
            VectorLerp(start, end, trace.fraction, lerpA);
            lerpA[2] = lerpA[2] - 6.0f;
            VectorCopy(origin, originCopy);
            VectorSubtract_080e113c(lerpB, originCopy, diff);
            lenA = (long double)Vec3_ElevationDegrees(diff);
            VectorSubtract_080e113c(lerpA, lerpB, diff);
            lenB = (long double)Vec3_ElevationDegrees(diff);
            slope = AngleSubtract((float)lenB, (float)lenA);
            if (slope < -50.0f || 70.0f < slope) {
                ok = 0;
            }
            VectorSet(mins, -0.0f, -0.0f, -0.0f);
            VectorSet(maxs, 0.0f, 0.0f, 0.0f);
            VectorCopy(originCopy, start);
            start[2] = start[2] + 5.0f;
            VectorCopy(lerpB, end);
            end[2] = end[2] + 5.0f;
            traceFn(&trace, start, mins, maxs, end, passEntity, mask);
            if (trace.fraction < 1.0f) {
                ok = 0;
            }
            VectorCopy(end, start);
            VectorCopy(lerpA, end);
            end[2] = end[2] + 5.0f;
            traceFn(&trace, start, mins, maxs, end, passEntity, mask);
            if (trace.fraction < 1.0f) {
                ok = 0;
            }
            if (outTorsoHeight != 0) {
                *outTorsoHeight = 0.0f;
            }
            if (outAngleA != 0) {
                VectorSubtract_080e113c(originCopy, lerpB, diff);
                lenA = (long double)Vec3_ElevationDegrees(diff);
                *outAngleA = AngleNormalize180((float)lenA);
            }
            if (outAngleB != 0) {
                VectorSubtract_080e113c(lerpB, lerpA, diff);
                lenA = (long double)Vec3_ElevationDegrees(diff);
                *outAngleB = AngleNormalize180((float)lenA);
            }
            if (ok != 0) {
                return 1;
            }
        }
    }
    } while (0);
    if (checkDuring == 0) {
        if (outTorsoHeight != 0) {
            *outTorsoHeight = 0.0f;
        }
        if (outAngleA != 0) {
            *outAngleA = 0.0f;
        }
        if (outAngleB != 0) {
            *outAngleB = 0.0f;
        }
        result = 1;
    } else {
        result = 0;
    }
    return result;
}

/* ======================================================================
 * 0x080e0e58  size=117  PM_ProbeClimbableLedgeEntry
 * naming=unknown: forwarding entry point for PM_ProbeClimbableLedge with the
 * same 14 arguments (the pseudocode shows a plain tail call).
 * ====================================================================== */
int PM_ProbeClimbableLedgeEntry(int passEntity, const float *origin, float halfWidth, float height,
                                float yaw, float *outTorsoHeight, float *outAngleA, float *outAngleB,
                                int skipFirstTrace, int checkDuring, const float *groundNormal,
                                uint8 traceMode, int altMaskSelect, float probeHeight)
{
    return PM_ProbeClimbableLedge(passEntity, origin, halfWidth, height, yaw, outTorsoHeight, outAngleA,
                                  outAngleB, skipFirstTrace, checkDuring, groundNormal, traceMode,
                                  altMaskSelect, probeHeight);
}

/* ======================================================================
 * 0x080e0ece  size=366  LerpColorOverTime
 * naming=unknown: fades an RGBA color from 'to' toward 'from' over a
 * time window; returns 'from' outright when the window has expired.
 * ====================================================================== */
void LerpColorOverTime(const cod2_color_fade_t *fade, int now, uint8 *out)
{
    int elapsed;
    float frac;
    float value;

    elapsed = now - fade->startTime;
    if (fade->duration < 1 || fade->duration <= elapsed) {
        memcpy(out, fade->from, 4);
    } else {
        if (elapsed < 0) {
            elapsed = 0;
        }
        frac = (float)elapsed / (float)fade->duration;
        value = (float)(int)((uint32)fade->from[0] - (uint32)fade->to[0]) * frac + (float)fade->to[0];
        out[0] = (uint8)(char)(short)(int)value;
        value = (float)(int)((uint32)fade->from[1] - (uint32)fade->to[1]) * frac + (float)fade->to[1];
        out[1] = (uint8)(char)(short)(int)value;
        value = (float)(int)((uint32)fade->from[2] - (uint32)fade->to[2]) * frac + (float)fade->to[2];
        out[2] = (uint8)(char)(short)(int)value;
        value = (float)(int)((uint32)fade->from[3] - (uint32)fade->to[3]) * frac + (float)fade->to[3];
        out[3] = (uint8)(char)(short)(int)value;
    }
}

/* ======================================================================
 * 0x080e103c  size=41  VectorClear
 * naming=inferred: writes (0,0,0); id Tech 3 VectorClear shape.
 * ====================================================================== */
void VectorClear(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

/* ======================================================================
 * 0x080e1066  size=35  VectorSet
 * naming=inferred: three-word store; the callers pass raw float bit patterns
 * (0x3f800000 = 1.0f, 0x40c00000 = 6.0f, 0xc0c00000 = -6.0f, 0x80000000 = -0.0f).
 * ====================================================================== */
void VectorSet(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

/* ======================================================================
 * 0x080e108a  size=47  VectorCopy
 * naming=inferred: id Tech 3 VectorCopy shape.
 * ====================================================================== */
void VectorCopy(const float *src, float *dst)
{
    /* The original copies three 32-bit words (mov), so signalling NaN bit patterns survive;
     * an x87 float load/store would quiet them. */
    ((int32 *)dst)[0] = ((const int32 *)src)[0];
    ((int32 *)dst)[1] = ((const int32 *)src)[1];
    ((int32 *)dst)[2] = ((const int32 *)src)[2];
}

/* ======================================================================
 * 0x080e10ba  size=62  VectorNegate
 * naming=inferred: the pseudocode XORs the sign bit (0x80000000), which is
 * the same as negation for every float value including zero and NaN.
 * ====================================================================== */
void VectorNegate(const float *src, float *dst)
{
    dst[0] = -src[0];
    dst[1] = -src[1];
    dst[2] = -src[2];
}

/* ======================================================================
 * 0x080e10f8  size=68  VectorAdd
 * naming=inferred: id Tech 3 VectorAdd shape.
 * ====================================================================== */
void VectorAdd_080e10f8(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

/* ======================================================================
 * 0x080e113c  size=68  VectorSubtract
 * naming=inferred: id Tech 3 VectorSubtract shape.
 * ====================================================================== */
void VectorSubtract_080e113c(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

/* ======================================================================
 * 0x080e1180  size=56  VectorScale
 * naming=inferred: id Tech 3 VectorScale shape (scale * v).
 * ====================================================================== */
void VectorScale_080e1180(const float *v, float scale, float *out)
{
    out[0] = scale * v[0];
    out[1] = scale * v[1];
    out[2] = scale * v[2];
}

/* ======================================================================
 * 0x080e11b8  size=83  VectorMA
 * naming=inferred: out = v + scale * dir (id Tech 3 VectorMA).
 * ====================================================================== */
void VectorMA_080e11b8(const float *v, float scale, const float *dir, float *out)
{
    out[0] = v[0] + scale * dir[0];
    out[1] = v[1] + scale * dir[1];
    out[2] = v[2] + scale * dir[2];
}

/* ======================================================================
 * 0x080e120c  size=51  DotProduct
 * naming=inferred: id Tech 3 DotProduct. The pseudocode returns a long double
 * (x87 st0) and the product is formed in extended precision; kept as such.
 * ====================================================================== */
long double DotProduct_080e120c(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2] + (long double)a[1] * (long double)b[1] +
           (long double)a[0] * (long double)b[0];
}

/* ======================================================================
 * 0x080e1240  size=62  VectorLength
 * naming=inferred: sqrt of the squared length, computed by FloatSqrt (the
 * pseudocode falls through from the sqrt call; its result is the float).
 * ====================================================================== */
float VectorLength_080e1240(const float *v)
{
    return FloatSqrt(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]);
}

/* ======================================================================
 * 0x080e127e  size=31  FloatSqrt
 * naming=inferred: calls libm sqrt (imports: sqrt) in double and rounds the
 * result to float; the only import in this helper.
 * ====================================================================== */
float FloatSqrt(float x)
{
    double d;

    d = sqrt((double)x);
    return (float)d;
}

/* ======================================================================
 * 0x080e129e  size=106  VectorLerp
 * naming=inferred: out = a + (b - a) * frac; in-place safe for out == a.
 * ====================================================================== */
void VectorLerp(const float *a, const float *b, float frac, float *out)
{
    out[0] = a[0] + (b[0] - a[0]) * frac;
    out[1] = a[1] + (b[1] - a[1]) * frac;
    out[2] = a[2] + (b[2] - a[2]) * frac;
}

/* ---- helpers shared by this chunk (not separate functions in the binary) ---- */
static float absf_041(float v)
{
    return __builtin_fabsf(v); /* x87 fabs clears the sign bit of NaNs too */
}

static int clampMovementDir_041(int dir)
{
    int absDir;

    absDir = dir;
    if (dir < 0) {
        absDir = -dir;
    }
    if (0x5a < absDir) {
        dir = (dir < 1) ? -0x5a : 0x5a;
    }
    return dir;
}

/* ======================================================================
 * 0x080e1308  size=87  PM_TraceRaw
 * naming=inferred: dispatches a trace through the row selected by
 * pmove.traceMode (g_pmTraceDispatch[traceMode].trace), no touch-list handling.
 * ====================================================================== */
void PM_TraceRaw(pmove_t *pm, trace_t *trace, const float *start, const float *mins,
                 const float *maxs, const float *end, int passEntity, int contentMask)
{
    g_pmTraceDispatch[pm->handler].trace(trace, start, mins, maxs, end, passEntity, contentMask);
}

/* ======================================================================
 * 0x080e1360  size=241  PM_Trace
 * naming=inferred: id Tech 3 PM_trace counterpart. If the trace is allsolid
 * against a body-class content (0x2000000), the hit entity is recorded in the
 * touch list and the trace is repeated with that content bit cleared.
 * ====================================================================== */
void PM_Trace(pmove_t *pm, trace_t *trace, const float *start, const float *mins,
              const float *maxs, const float *end, int passEntity, int contentMask)
{
    PM_TraceRaw(pm, trace, start, mins, maxs, end, passEntity, contentMask);
    if (trace->startsolid != 0 && (trace->contents & 0x2000000) != 0) {
        PM_AddTouchEnt(pm, (int)trace->entityNum);
        pm->tracemask = (int32)((uint32)pm->tracemask & 0xfdffffffU);
        PM_TraceRaw(pm, trace, start, mins, maxs, end, passEntity,
                    (int)((uint32)contentMask & 0xfdffffffU));
    }
}

/* ======================================================================
 * 0x080e1452  size=34  PM_AddEvent
 * naming=inferred: id Tech 3 PM_AddEvent (BG_AddPredictableEventToPlayerstate
 * with a zero event parameter).
 * ====================================================================== */
void PM_AddEvent(playerState_t *ps, int event)
{
    BG_AddPredictableEventToPlayerstate((uint32)event, 0, ps);
}

/* ======================================================================
 * 0x080e1474  size=96  PM_AddTouchEnt
 * naming=inferred: id Tech 3 PM_AddTouchEnt. Ignores entity 1022 (world)
 * and adds unique entity numbers up to 32 touch slots.
 * ====================================================================== */
void PM_AddTouchEnt(pmove_t *pm, int entityNum)
{
    int i;

    if (entityNum != 0x3fe && pm->numtouch != 0x20) {
        for (i = 0; i < pm->numtouch; i++) {
            if (pm->touchents[i] == entityNum) {
                return;
            }
        }
        pm->touchents[pm->numtouch] = entityNum;
        pm->numtouch = pm->numtouch + 1;
    }
}

/* ======================================================================
 * 0x080e14d4  size=93  PM_ClipVelocity
 * naming=inferred: id Tech 3 PM_ClipVelocity with the overbounce fixed at
 * 1.001 (backoff = dot - 0.001*|dot|, out = in + backoff' * normal).
 * ====================================================================== */
void PM_ClipVelocity(const float *in, const float *normal, float *out)
{
    float backoff;
    long double absBackoff;
    float scale;

    backoff = DotProduct3(in, normal);
    absBackoff = (long double)Float_Abs_080e9576(backoff);
    scale = -(float)((long double)backoff - (long double)0.001f * absBackoff);
    VectorMA_080e992c(in, scale, normal, out);
}

/* ======================================================================
 * 0x080e1532  size=60  PM_StanceIndex
 * naming=inferred: maps the stance target height to a class index:
 * 2 = crouch (height 40), 1 = prone (height 11), 0 = stand (anything else).
 * ====================================================================== */
int PM_StanceIndex(const playerState_t *ps)
{
    int index;

    if (ps->viewHeightTarget == 0x28) {
        index = 2;
    } else if (ps->viewHeightTarget == 0xb) {
        index = 1;
    } else {
        index = 0;
    }
    return index;
}

/* ======================================================================
 * 0x080e156e  size=413  PM_Friction
 * naming=inferred: id Tech 3 PM_Friction. Ground friction (walking, not on
 * a no-friction surface, not in a ledge state) plus the prone/dead factor.
 * Velocity below 1 unit/s is cleared.
 * ====================================================================== */
void PM_Friction(playerState_t *ps, pml_t *pml)
{
    float vel[3];
    float speed;
    float control;
    float controlScaled;
    float drop;
    float newspeed;

    VectorCopy(ps->velocity, vel);
    if (pml->walking != 0) {
        vel[2] = 0.0f;
    }
    speed = VectorLength_080e1240(vel);
    if (speed < 1.0f) {
        Vec3_Clear_Dup(ps->velocity);
        return;
    }

    drop = 0.0f;
    if (pml->walking != 0 && (pml->groundTrace.surfaceFlags & 2) == 0 &&
        (ps->pm_flags & 0x400U) == 0) {
        if (speed < dvar_stopspeed->current.value) {
            control = dvar_stopspeed->current.value;
        } else {
            control = speed;
        }
        controlScaled = control;
        if ((ps->pm_flags & 0x200U) == 0) {
            if ((ps->pm_flags & 0x80000U) != 0) {
                controlScaled = (float)((long double)control * (long double)Player_JumpScale(ps));
            }
        } else {
            controlScaled = control * 0.3f;
        }
        drop = controlScaled * dvar_friction->current.value * pml->frametime + 0.0f;
    }
    if (ps->pm_type == 4) {
        drop = drop + speed * 5.0f * pml->frametime;
    }
    newspeed = speed - drop;
    if (newspeed < 0.0f) {
        newspeed = 0.0f;
    }
    newspeed = newspeed / speed;
    ps->velocity[0] = ps->velocity[0] * newspeed;
    ps->velocity[1] = ps->velocity[1] * newspeed;
    ps->velocity[2] = ps->velocity[2] * newspeed;
}

/* ======================================================================
 * 0x080e170c  size=241  PM_ShouldClampAcceleration
 * naming=inferred: the two debug strings "angle is %f (oldVel is ...)" and
 * "clamping acceleration from %f to %f" name the clamp this belongs to. It
 * tests whether the new (velocity + accel*dir) direction turns more sharply
 * than the cosine limit dvar_inertiaAngle against the previous 2D velocity.
 * Returns 1 when the acceleration must be clamped.
 * ====================================================================== */
int PM_ShouldClampAcceleration(playerState_t *ps, float accel, const float *dir)
{
    float newVel[3];
    float oldVel[2];
    float cosAngle;

    Vec2_MA(ps->velocity, accel, dir, newVel);
    Vec2_Copy(ps->oldVelocity, oldVel);
    Vec2_Normalize(oldVel);
    Vec2_Normalize(newVel);
    cosAngle = Vec2_Dot_Dup(oldVel, newVel);
    if (dvar_inertiaAngle->current.value <= cosAngle) {
        return 0;
    }
    if (*(const char *)&dvar_inertiaDebug->current.value != 0) {
        Com_Printf("angle is %f (oldVel is (%f,%f), vel is (%f, %f))\n", (double)cosAngle,
                    (double)oldVel[0], (double)oldVel[1], (double)newVel[0], (double)newVel[1]);
        Com_Printf("clamping acceleration from %f to %f\n", (double)accel,
                    (double)dvar_inertiaMax->current.value);
    }
    return 1;
}

/* ======================================================================
 * 0x080e17fe  size=143  PM_ClampAcceleration
 * naming=inferred: pairs with PM_ShouldClampAcceleration. Limits the
 * acceleration to dvar_inertiaMax when the turn test fires (spectators pass
 * through unchanged).
 * ====================================================================== */
float PM_ClampAcceleration(playerState_t *ps, float accel, const float *dir)
{
    float result;
    float oldSpeed;

    if (ps->pm_type == 2) {
        result = accel;
    } else if (accel <= dvar_inertiaMax->current.value) {
        result = accel;
    } else {
        oldSpeed = Vec2_LengthSq(ps->oldVelocity);
        if ((double)oldSpeed < 0.0001) {
            result = accel;
        } else if (PM_ShouldClampAcceleration(ps, accel, dir) == 0) {
            result = accel;
        } else {
            result = dvar_inertiaMax->current.value;
        }
    }
    return result;
}

/* ======================================================================
 * 0x080e188e  size=371  PM_Accelerate
 * naming=inferred: id Tech 3 PM_Accelerate. Normal branch adds
 * accel*frametime*max(wishspeed, dvar_stopspeed) up to the speed deficit (with
 * the acceleration clamp applied). When pmFlags bit 0x20 is set, the velocity
 * is steered toward the wish vector instead.
 * ====================================================================== */
void PM_Accelerate(playerState_t *ps, pml_t *pml, const float *wishdir, float wishspeed, float accel)
{
    float currentSpeed;
    float addSpeed;
    float maxSpeed;
    float accelSpeed;
    float clamped;
    float scaled[3];
    float diff[3];
    float diffLen;
    float steerSpeed;

    if ((ps->pm_flags & 0x20U) == 0) {
        currentSpeed = DotProduct3(ps->velocity, wishdir);
        addSpeed = wishspeed - currentSpeed;
        if (0.0 < (double)addSpeed) {
            if (wishspeed < dvar_stopspeed->current.value) {
                maxSpeed = dvar_stopspeed->current.value;
            } else {
                maxSpeed = wishspeed;
            }
            accelSpeed = accel * pml->frametime * maxSpeed;
            if (addSpeed < accelSpeed) {
                accelSpeed = addSpeed;
            }
            clamped = PM_ClampAcceleration(ps, accelSpeed, wishdir);
            VectorMA_080e992c(ps->velocity, clamped, wishdir, ps->velocity);
        }
    } else {
        VectorScale_080e1180(wishdir, wishspeed, scaled);
        VectorSubtract_080e113c(scaled, ps->velocity, diff);
        diffLen = Vec3_Normalize(diff);
        steerSpeed = accel * pml->frametime * wishspeed;
        if (diffLen < steerSpeed) {
            steerSpeed = diffLen;
        }
        VectorMA_080e992c(ps->velocity, steerSpeed, diff, ps->velocity);
    }
}

/* ======================================================================
 * 0x080e1a02  size=284  PM_MaxAxisCmdScale
 * naming=inferred: id Tech 3 PM_CmdScale shape on a raw (x,y,z) input: the
 * largest |component| over the vector length, times speed/127, with the
 * same stance, pmType and moveAxis factors as the command variants.
 * ====================================================================== */
float PM_MaxAxisCmdScale(const playerState_t *ps, float x, float y, float z)
{
    float maxAbs;
    float len;
    float value;
    float result;

    maxAbs = absf_041(x);
    if (maxAbs < absf_041(y)) {
        maxAbs = absf_041(y);
    }
    if (maxAbs < absf_041(z)) {
        maxAbs = absf_041(z);
    }
    if (maxAbs == 0.0f) {
        result = 0.0f;
    } else {
        len = Float_Sqrt_080e9590(z * z + y * y + x * x);
        value = (float)(((float)ps->speed * maxAbs) / ((double)len * 127.0));
        if (((ps->pm_flags & 0x100U) != 0) || (ps->leanf != 0.0f) || (ps->leanf != ps->leanf)) {
            value = (float)(value * 0.4f);
        }
        if (ps->pm_type == 2) {
            value = (float)(value * 3.0);
        }
        if (ps->pm_type == 3) {
            value = (float)(value * 6.0);
        }
        if (ps->pm_type == 4) {
            value = (float)(value * dvar_player_spectateSpeedScale->current.value);
        }
        result = value;
    }
    return result;
}

/* ======================================================================
 * 0x080e1b1e  size=314  PM_CmdScale2D
 * naming=inferred: forward/right-only command scale (id Tech 3 PM_CmdScale
 * without the up axis), using max(|forward|,|right|) over the integer move
 * magnitudes, with the same modifiers as PM_MaxAxisCmdScale.
 * ====================================================================== */
float PM_CmdScale2D(const playerState_t *ps, const usercmd_t *cmd)
{
    int fwd;
    int right;
    int maxMove;
    int absFwd;
    int absRight;
    float len;
    float value;
    float result;

    fwd = (int)cmd->forwardmove;
    right = (int)cmd->rightmove;
    len = Float_Sqrt_080e9590((float)(right * right + fwd * fwd));
    absFwd = fwd;
    if (absFwd < 0) {
        absFwd = -absFwd;
    }
    maxMove = absFwd;
    absRight = right;
    if (absRight < 0) {
        absRight = -absRight;
    }
    if (absFwd < absRight) {
        maxMove = absRight;
    }
    if (maxMove == 0) {
        result = 0.0f;
    } else {
        value = (float)(((float)maxMove * (float)ps->speed) / ((double)len * 127.0));
        if (((ps->pm_flags & 0x100U) != 0) || (ps->leanf != 0.0f) || (ps->leanf != ps->leanf)) {
            value = (float)(value * 0.4f);
        }
        if (ps->pm_type == 2) {
            value = (float)(value * 3.0);
        }
        if (ps->pm_type == 3) {
            value = (float)(value * 6.0);
        }
        if (ps->pm_type == 4) {
            value = (float)(value * dvar_player_spectateSpeedScale->current.value);
        }
        result = value;
    }
    return result;
}

/* ======================================================================
 * 0x080e1c58  size=712  PM_CmdScale
 * naming=inferred: id Tech 3 PM_CmdScale. Scales the wish speed from the
 * usercmd move magnitudes (with the per-direction scale cvars), the stance
 * transition blend, a linked entity's scale (+0x264), and the 0x800 button.
 * ====================================================================== */
float PM_CmdScale(pmove_t *pm, const usercmd_t *cmd)
{
    playerState_t *ps;
    float lenXY;
    float fwdScale;
    float sideScale;
    float value;
    float blend;
    int stance;
    void *entity;
    float result;

    ps = pm->ps;
    lenXY = Float_Sqrt_080e9590((float)((int)cmd->rightmove * (int)cmd->rightmove +
                                (int)cmd->forwardmove * (int)cmd->forwardmove));
    if (cmd->forwardmove < 0) {
        fwdScale = Float_Abs_080e9576((float)(short)cmd->forwardmove * dvar_player_backSpeedScale->current.value);
    } else {
        fwdScale = Float_Abs_080e9576((float)(short)cmd->forwardmove);
    }
    sideScale = Float_Abs_080e9576((float)(short)cmd->rightmove * dvar_player_strafeSpeedScale->current.value);
    sideScale = Float_Max_080e95b0(fwdScale, sideScale);
    if (sideScale == 0.0f) {
        result = 0.0f;
    } else {
        value = (float)(((float)ps->speed * sideScale) / ((double)lenXY * 127.0));
        if (((ps->pm_flags & 0x100U) != 0) || (ps->leanf != 0.0f) || (ps->leanf != ps->leanf)) {
            value = (float)(value * 0.4f);
        }
        if (ps->pm_type == 2) {
            value = (float)(value * 3.0);
        } else if (ps->pm_type == 3) {
            value = (float)(value * 6.0);
        } else {
            stance = PM_StanceIndex(ps);
            blend = PM_StanceTransitionProgress(pm, 0x28, 0xb);
            if (blend != 0.0f || blend != blend) {
                /* gameplay-fix(net_clients): 0x080e1e09..0x080e1ee7 multiply by the float constants 0.15f/0.65f (0x8154fa0/0x8154fa4), not doubles */
                value = (float)(value * ((1.0 - blend) * 0.65f + blend * 0.15f));
            } else {
                blend = PM_StanceTransitionProgress(pm, 0xb, 0x28);
                if (blend != 0.0f || blend != blend) {
                    value = (float)(value * ((1.0 - blend) * 0.15f + blend * 0.65f));
                } else if (stance == 1) {
                    value = (float)(value * 0.15f);
                } else if (stance == 2) {
                    value = (float)(value * 0.65f);
                }
            }
        }
        if (ps->weapon != 0) {
            entity = BG_GetWeaponDef(ps->weapon);
            if (0.0 < (double)*(const float *)((const uint8 *)entity + 0x264)) {
                entity = BG_GetWeaponDef(ps->weapon);
                value = (float)(value * *(const float *)((const uint8 *)entity + 0x264));
            }
        }
        if ((cmd->buttons & 0x800) != 0) {
            value = (float)(value * 0.4f);
        }
        result = value;
    }
    return result;
}

/* ======================================================================
 * 0x080e1f20  size=107  PM_PmTimeScale
 * naming=unknown: linear scale of the pmTime countdown: 1 + pmTime * (-b/a)
 * for the two cvars dvar_player_dmgtimer_maxTime/dvar_player_dmgtimer_minScale, else 1 when either is zero.
 * ====================================================================== */
float PM_PmTimeScale(int pmTime)
{
    float result;

    if (pmTime == 0) {
        result = 1.0f;
    } else if (dvar_player_dmgtimer_maxTime->current.value == 0.0f) {
        result = 1.0f;
    } else {
        result = (float)((float)pmTime * (-dvar_player_dmgtimer_minScale->current.value / dvar_player_dmgtimer_maxTime->current.value) + 1.0);
    }
    return result;
}

/* ======================================================================
 * 0x080e1f8c  size=660  PM_SetMovementDir
 * naming=inferred: id Tech 3 PM_SetMovementDir. Sets ps->movementDir (a signed
 * byte in [-90, 90]) from the travel direction (origin - previous origin) and
 * the view yaw, or from the input direction when standing still. Branches on
 * pmFlags bit 0x1, buttonBits 0x300, pmFlags bit 0x20.
 * ====================================================================== */
void PM_SetMovementDir(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    float delta[3];
    float angles[2];
    float travelLen;
    float yawTarget;
    int dirDeg;
    long double lenValue;

    ps = pm->ps;
    if (((ps->pm_flags & 1U) == 0) || ((ps->eFlags & 0x300U) != 0)) {
        if ((ps->pm_flags & 0x20U) == 0) {
            VectorSubtract_080e113c(ps->origin, pml->previous_origin, delta);
            if ((pm->cmd.forwardmove != 0 || pm->cmd.rightmove != 0) && ps->groundEntityNum != 0x3ff) {
                travelLen = VectorLength_080e99e8(delta);
                if ((travelLen != 0.0f || travelLen != travelLen) &&
                    (double)pml->frametime * 5.0 < (double)travelLen) {
                    Vec3_NormalizeCopy(delta, angles);
                    Vec3_ToAngles(angles, angles);
                    lenValue = (long double)AngleDelta(angles[1], ps->viewangles[1]);
                    dirDeg = (int)lenValue;
                    if (pm->cmd.forwardmove < 0) {
                        dirDeg = (int)AngleNormalize180((float)(dirDeg + 180.0));
                    }
                    dirDeg = clampMovementDir_041(dirDeg);
                    ps->movementDir = (int32)(int8)dirDeg;
                    return;
                }
            }
            ps->movementDir = 0;
        } else {
            yawTarget = (float)(180.0 + Vec2_AngleDegrees(&ps->vLadderVec[0]));
            lenValue = (long double)AngleDelta(yawTarget, ps->viewangles[1]);
            dirDeg = clampMovementDir_041((int)lenValue);
            ps->movementDir = (int32)(int8)dirDeg;
        }
    } else {
        lenValue = (long double)AngleDelta(ps->proneDirection, ps->viewangles[1]);
        dirDeg = clampMovementDir_041((int)lenValue);
        ps->movementDir = (int32)(int8)dirDeg;
    }
}

/* ======================================================================
 * 0x080e2220  size=61  PM_GroundSurfaceType
 * naming=inferred: surface-type index from the ground trace surface flags:
 * bits 20..24 unless bit 0x2000 (no footsteps) is set, then 0.
 * ====================================================================== */
int PM_GroundSurfaceType(const pml_t *pml)
{
    int index;

    if ((pml->groundTrace.surfaceFlags & 0x2000) == 0) {
        index = (int)(((uint32)pml->groundTrace.surfaceFlags & 0x1f00000U) >> 0x14);
    } else {
        index = 0;
    }
    return index;
}

/* ======================================================================
 * 0x080e225e  size=410  PM_FreeMove
 * naming=unknown: a non-ground movement: friction, then accelerate toward a
 * wish vector built from the forward/right basis plus a vertical term from
 * the jump/crouch buttons (0x80 up, 0x40 down) scaled by the speed factor.
 * Ends with a step-slide. Not identified against a counterpart.
 * ====================================================================== */
void PM_FreeMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    float blend;
    float wish[3];
    float wishDir[3];
    float wishSpeed;
    long double scale;

    ps = pm->ps;
    PM_Friction(ps, pml);
    blend = PM_CmdScale2D(ps, &pm->cmd);
    if (blend == 0.0f) {
        VectorClear(wish);
    } else {
        int i;
        for (i = 0; i < 3; i++) {
            wish[i] = (float)(short)pm->cmd.rightmove * blend * pml->right[i] +
                      (float)(short)pm->cmd.forwardmove * blend * pml->forward[i];
        }
    }
    if (ps->speed != 0) {
        scale = (long double)PM_MaxAxisCmdScale(ps, 0.0f, 0.0f, 127.0f);
        if ((pm->cmd.buttons & 0x40) != 0) {
            wish[2] = (float)(wish[2] - (float)scale * 127.0);
        }
        if ((pm->cmd.buttons & 0x80) != 0) {
            wish[2] = (float)(wish[2] + (float)scale * 127.0);
        }
    }
    Vec3_Copy_Dup(wish, wishDir);
    wishSpeed = Vec3_Normalize(wishDir);
    PM_Accelerate(ps, pml, wishDir, wishSpeed, 8.0f);
    PM_StepSlideMove(pm, pml, 0);
}

/* ======================================================================
 * 0x080e23f8  size=429  PM_AirMove
 * naming=inferred: id Tech 3 PM_AirMove. Friction, flatten the forward and
 * right basis to the ground plane, accelerate along the wish direction, clip
 * against the ground normal when on a plane, step-slide, set movement dir.
 * ====================================================================== */
void PM_AirMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    usercmd_t cmdCopy;
    float fwdF;
    float sideF;
    float scale;
    float wish[3];
    float wishDir[3];
    float wishSpeed;
    int i;

    ps = pm->ps;
    PM_Friction(ps, pml);
    fwdF = (float)(short)pm->cmd.forwardmove;
    sideF = (float)(short)pm->cmd.rightmove;
    memcpy(&cmdCopy, &pm->cmd, sizeof(cmdCopy));
    scale = PM_CmdScale2D(ps, &cmdCopy);
    pml->forward[2] = 0.0f;
    pml->right[2] = 0.0f;
    Vec3_Normalize(pml->forward);
    Vec3_Normalize(pml->right);
    for (i = 0; i < 2; i++) {
        wish[i] = pml->right[i] * sideF + pml->forward[i] * fwdF;
    }
    wish[2] = 0.0f;
    Vec3_Copy_Dup(wish, wishDir);
    wishSpeed = Vec3_Normalize(wishDir) * scale;
    PM_Accelerate(ps, pml, wishDir, wishSpeed, 1.0f);
    if (pml->groundPlane != 0) {
        PM_ClipVelocity(ps->velocity, pml->groundTrace.normal, ps->velocity);
    }
    PM_StepSlideMove(pm, pml, 1);
    PM_SetMovementDir(pm, pml);
}

/* ======================================================================
 * 0x080e25a6  size=1147  PM_WalkMove
 * naming=inferred: id Tech 3 PM_WalkMove dispatch. When the walk test
 * (Player_TryJump) fails, runs the airborne branch: friction, command scale,
 * pmTime countdown, clip the basis to the ground normal, accelerate with a
 * stance-dependent rate, gravity, clip, and step-slide when moving in XY.
 * Otherwise hands off to PM_AirMove.
 * ====================================================================== */
void PM_WalkMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    usercmd_t cmdCopy;
    float fwdF;
    float sideF;
    float scale;
    float timeScale;
    float wish[3];
    float wishDir[3];
    float wishSpeed;
    float accel;
    float saved[3];
    float speed;
    long double lenV;
    long double dotV;
    int stance;
    int i;

    ps = pm->ps;
    if ((ps->pm_flags & 0x80000U) != 0) {
        Player_UpdateJumpPhase(ps);
    }
    if (Player_TryJump(pm, pml) == 0) {
        PM_Friction(ps, pml);
        fwdF = (float)(short)pm->cmd.forwardmove;
        sideF = (float)(short)pm->cmd.rightmove;
        memcpy(&cmdCopy, &pm->cmd, sizeof(cmdCopy));
        scale = PM_CmdScale(pm, &cmdCopy);
        timeScale = PM_PmTimeScale(ps->damageTimer);
        scale = (float)(scale * timeScale);
        ps->damageTimer = ps->damageTimer - (int)(pml->frametime * 1000.0);
        if (ps->damageTimer < 1) {
            ps->damageTimer = 0;
        }
        pml->forward[2] = 0.0f;
        pml->right[2] = 0.0f;
        PM_ClipVelocity(pml->forward, pml->groundTrace.normal, pml->forward);
        PM_ClipVelocity(pml->right, pml->groundTrace.normal, pml->right);
        Vec3_Normalize(pml->forward);
        Vec3_Normalize(pml->right);
        for (i = 0; i < 3; i++) {
            wish[i] = pml->right[i] * sideF + pml->forward[i] * fwdF;
        }
        Vec3_Copy_Dup(wish, wishDir);
        lenV = (long double)Vec3_Normalize(wishDir);
        wishSpeed = (float)(lenV * scale);
        stance = PM_StanceIndex(ps);
        if (((pml->groundTrace.surfaceFlags & 2) == 0) && ((ps->pm_flags & 0x400U) == 0)) {
            if (stance == 1) {
                accel = 19.0f;
            } else if (stance == 2) {
                accel = 12.0f;
            } else {
                accel = 9.0f;
            }
        } else {
            accel = 1.0f;
        }
        if ((ps->pm_flags & 0x200U) != 0) {
            accel = (float)(accel * 0.25);
        }
        PM_Accelerate(ps, pml, wishDir, wishSpeed, accel);
        if (((pml->groundTrace.surfaceFlags & 2) != 0) || ((ps->pm_flags & 0x400U) != 0)) {
            ps->velocity[2] = (float)(ps->velocity[2] - (float)ps->gravity * pml->frametime);
        }
        speed = VectorLength_080e99e8(ps->velocity);
        VectorCopy(ps->velocity, saved);
        PM_ClipVelocity(ps->velocity, pml->groundTrace.normal, ps->velocity);
        dotV = (long double)DotProduct3(ps->velocity, saved);
        if (0.0L < dotV) {
            Vec3_Normalize(ps->velocity);
            VectorScale_080e98f4(ps->velocity, speed, ps->velocity);
        }
        if (ps->velocity[0] != 0.0f || ps->velocity[0] != ps->velocity[0] ||
            ps->velocity[1] != 0.0f || ps->velocity[1] != ps->velocity[1]) {
            PM_StepSlideMove(pm, pml, 0);
        }
        PM_SetMovementDir(pm, pml);
    } else {
        PM_AirMove(pm, pml);
    }
}

/* ======================================================================
 * 0x080e2a22  size=127  PM_ReduceGroundSpeed
 * naming=unknown: while walking, reduces the velocity length by 20 units
 * (clears it when it would go to zero or below).
 * ====================================================================== */
void PM_ReduceGroundSpeed(playerState_t *ps, pml_t *pml)
{
    float speed;

    if (pml->walking != 0) {
        speed = VectorLength_080e99e8(ps->velocity);
        if ((double)speed - 20.0 <= 0.0) {
            Vec3_Clear_Dup(ps->velocity);
        } else {
            Vec3_Normalize(ps->velocity);
            VectorScale_080e98f4(ps->velocity, (float)((double)speed - 20.0), ps->velocity);
        }
    }
}

/* ======================================================================
 * 0x080e2aa2  size=586  PM_FlyMove
 * naming=unknown: no-gravity movement (spectator/fly): sets the stance to
 * standing (60), applies friction, accelerates with the full 3D command
 * (up from buttons 0x80/0x40) at rate 9, and integrates the origin.
 * ====================================================================== */
void PM_FlyMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    float speed;
    float friction;
    float refSpeed;
    float loss;
    float newspeed;
    float fwdF;
    float sideF;
    float upF;
    float scale;
    float wish[3];
    float wishDir[3];
    float wishSpeed;
    int i;

    ps = pm->ps;
    ps->viewHeightTarget = 0x3c;
    speed = VectorLength_080e99e8(ps->velocity);
    if (speed < 1.0f) {
        Vec3_Copy_Dup(vec3_origin, ps->velocity);
    } else {
        friction = dvar_friction->current.value * 1.5f;
        if (speed < dvar_stopspeed->current.value) {
            refSpeed = dvar_stopspeed->current.value;
        } else {
            refSpeed = speed;
        }
        loss = refSpeed * friction * pml->frametime + 0.0f;
        newspeed = speed - loss;
        if (newspeed < 0.0f) {
            newspeed = 0.0f;
        }
        newspeed = newspeed / speed;
        VectorScale_080e98f4(ps->velocity, newspeed, ps->velocity);
    }
    fwdF = (float)(short)pm->cmd.forwardmove;
    sideF = (float)(short)pm->cmd.rightmove;
    upF = 0.0f;
    if ((pm->cmd.buttons & 0x80) != 0) {
        upF = 127.0f;
    }
    if ((pm->cmd.buttons & 0x40) != 0) {
        upF = upF - 127.0f;
    }
    scale = PM_MaxAxisCmdScale(ps, fwdF, sideF, upF);
    for (i = 0; i < 3; i++) {
        wish[i] = pml->up[i] * upF + pml->right[i] * sideF + pml->forward[i] * fwdF;
    }
    Vec3_Copy_Dup(wish, wishDir);
    wishSpeed = Vec3_Normalize(wishDir) * scale;
    PM_Accelerate(ps, pml, wishDir, wishSpeed, 9.0f);
    VectorMA_080e992c(ps->origin, pml->frametime, ps->velocity, ps->origin);
}

/* ======================================================================
 * 0x080e2cec  size=707  PM_ClimbMove
 * naming=unknown: like PM_FlyMove, but the side vector is built from the
 * up axis crossed with the right basis (a surface-aligned frame), so the
 * forward component follows the cross product. Stance set to standing.
 * ====================================================================== */
void PM_ClimbMove(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    float fwdF;
    float sideF;
    float upF;
    float speed;
    float friction;
    float refSpeed;
    float loss;
    float newspeed;
    float scale;
    float upAxis[3];
    float crossFwd[3];
    float wish[3];
    float wishDir[3];
    float wishSpeed;
    int i;

    ps = pm->ps;
    ps->viewHeightTarget = 0x3c;
    fwdF = (float)(short)pm->cmd.forwardmove;
    sideF = (float)(short)pm->cmd.rightmove;
    upF = 0.0f;
    if ((pm->cmd.buttons & 0x80) != 0) {
        upF = 127.0f;
    }
    if ((pm->cmd.buttons & 0x40) != 0) {
        upF = upF - 127.0f;
    }
    if (fwdF == 0.0f && sideF == 0.0f && upF == 0.0f) {
        speed = 0.0f;
    } else {
        speed = VectorLength_080e99e8(ps->velocity);
    }
    if (speed < 1.0f) {
        Vec3_Copy_Dup(vec3_origin, ps->velocity);
    } else {
        friction = dvar_friction->current.value * 1.5f;
        if (speed < dvar_stopspeed->current.value) {
            refSpeed = dvar_stopspeed->current.value;
        } else {
            refSpeed = speed;
        }
        loss = refSpeed * friction * pml->frametime + 0.0f;
        newspeed = speed - loss;
        if (newspeed < 0.0f) {
            newspeed = 0.0f;
        }
        newspeed = newspeed / speed;
        VectorScale_080e98f4(ps->velocity, newspeed, ps->velocity);
    }
    scale = PM_MaxAxisCmdScale(ps, fwdF, sideF, upF);
    upAxis[0] = 0.0f;
    upAxis[1] = 0.0f;
    upAxis[2] = 1.0f;
    Vec3_Cross(upAxis, pml->right, crossFwd);
    for (i = 0; i < 3; i++) {
        wish[i] = upAxis[i] * upF + pml->right[i] * sideF + crossFwd[i] * fwdF;
    }
    Vec3_Copy_Dup(wish, wishDir);
    wishSpeed = Vec3_Normalize(wishDir) * scale;
    PM_Accelerate(ps, pml, wishDir, wishSpeed, 9.0f);
    VectorMA_080e992c(ps->origin, pml->frametime, ps->velocity, ps->origin);
}

/* ======================================================================
 * 0x080e2fb0  size=121  PM_FootstepSoundIndex
 * naming=inferred: footstep event index: surface index, offset by 0x18 when
 * moving (pmFlags 0x100 or moveAxis set), +1 when standing, +0x2f when the
 * 0x1 flag (ducked) is set; 0 when there is no surface index.
 * ====================================================================== */
int PM_FootstepSoundIndex(const playerState_t *ps, const pml_t *pml)
{
    int index;

    index = PM_GroundSurfaceType(pml);
    if (index == 0) {
        index = 0;
    } else if ((ps->pm_flags & 1U) == 0) {
        if (((ps->pm_flags & 0x100U) != 0) || (ps->leanf != 0.0f) || (ps->leanf != ps->leanf)) {
            index = index + 0x18;
        } else {
            index = index + 1;
        }
    } else {
        index = index + 0x2f;
    }
    return index;
}

/* ======================================================================
 * 0x080e302a  size=49  PM_GroundSoundIndexPlus18
 * naming=unknown: surface index + 0x18 (0 when there is no surface index).
 * ====================================================================== */
int PM_GroundSoundIndexPlus18(const pml_t *pml)
{
    int index;

    index = PM_GroundSurfaceType(pml);
    if (index == 0) {
        index = 0;
    } else {
        index = index + 0x18;
    }
    return index;
}

/* ======================================================================
 * 0x080e305c  size=47  PM_GroundSoundIndexPlus1
 * naming=unknown: surface index + 1 (0 when there is no surface index).
 * ====================================================================== */
int PM_GroundSoundIndexPlus1(const pml_t *pml)
{
    int index;

    index = PM_GroundSurfaceType(pml);
    if (index == 0) {
        index = 0;
    } else {
        index = index + 1;
    }
    return index;
}

/* ======================================================================
 * 0x080e308c  size=28  PM_GroundSoundIndexPlus5D
 * naming=unknown: surface index + 0x5d, with no zero check (the pseudocode
 * returns the raw sum).
 * ====================================================================== */
int PM_GroundSoundIndexPlus5D(const pml_t *pml)
{
    int index;

    index = PM_GroundSurfaceType(pml);
    return index + 0x5d;
}

/* ======================================================================
 * 0x080e30a8  size=28  PM_GroundSoundIndexPlus74
 * naming=unknown: surface index + 0x74, with no zero check.
 * ====================================================================== */
int PM_GroundSoundIndexPlus74(const pml_t *pml)
{
    int index;

    index = PM_GroundSurfaceType(pml);
    return index + 0x74;
}

/* ======================================================================
 * 0x080e30c4  size=1026  PM_CrashLand
 * naming=inferred: fall damage. The string "bg_fallDamageMaxHeight must be
 * greater than bg_fallDamageMinHeight" names the cvar pair dvar_bg_fallDamageMinHeight/
 * dvar_bg_fallDamageMaxHeight. The fall height is recovered from the previous vertical
 * velocity and the gravity, mapped to a damage percentage between the two
 * height cvars, and emits landing events and a velocity damp.
 * ====================================================================== */
void PM_CrashLand(playerState_t *ps, pml_t *pml)
{
    float prevVelZ;
    float halfG;
    float disc;
    float fallHeight;
    int damageRating;
    int heightBand;
    int eventValue;
    float dampScale;
    int timeCost;
    int gravityInt;

    prevVelZ = pml->previous_velocity[2];
    gravityInt = ps->gravity;
    halfG = -(float)gravityInt * 0.5f;
    disc = prevVelZ * prevVelZ - halfG * 4.0 * (pml->previous_origin[2] - ps->origin[2]);
    if (0.0 <= (double)disc) {
        double root = sqrt((double)disc);
        fallHeight = -(((-prevVelZ - (float)root) / (halfG + halfG)) * -(float)gravityInt + prevVelZ);
        fallHeight = (fallHeight * fallHeight) / ((float)ps->gravity + (float)ps->gravity);

        if (dvar_bg_fallDamageMaxHeight->current.value <= dvar_bg_fallDamageMinHeight->current.value) {
            Com_Printf("bg_fallDamageMaxHeight must be greater than bg_fallDamageMinHeight\n");
            damageRating = 0;
        } else if (fallHeight <= dvar_bg_fallDamageMinHeight->current.value || (pml->groundTrace.surfaceFlags & 1) != 0 ||
                   5 < ps->pm_type) {
            damageRating = 0;
        } else if (dvar_bg_fallDamageMaxHeight->current.value <= fallHeight) {
            damageRating = 100;
        } else {
            damageRating = Int_SelectClamp(
                (int)(((fallHeight - dvar_bg_fallDamageMinHeight->current.value) / (dvar_bg_fallDamageMaxHeight->current.value - dvar_bg_fallDamageMinHeight->current.value)) * 100.0),
                0, 100);
        }

        if (fallHeight <= 12.0f) {
            heightBand = 0;
        } else {
            heightBand = (int)(((fallHeight - 12.0) / 26.0) * 4.0 + 4.0);
            if (0x18 < heightBand) {
                heightBand = 0x18;
            }
            BG_AnimScriptEvent(ps, 5, 0, 1);
        }

        if (damageRating == 0) {
            if (4.0 < (double)fallHeight) {
                if (fallHeight < 8.0f) {
                    eventValue = PM_GroundSoundIndexPlus18(pml);
                    PM_AddEvent(ps, eventValue);
                } else if (fallHeight < 12.0f) {
                    eventValue = PM_GroundSoundIndexPlus1(pml);
                    PM_AddEvent(ps, eventValue);
                } else {
                    VectorScale_080e98f4(ps->velocity, 0.67f, ps->velocity);
                    eventValue = PM_GroundSoundIndexPlus5D(pml);
                    BG_AddPredictableEventToPlayerstate((uint32)eventValue, (uint8)heightBand, ps);
                }
            }
        } else {
            if (damageRating < 100 && (pml->groundTrace.surfaceFlags & 2) == 0) {
                timeCost = damageRating * 0x23 + 500;
                if (2000 < timeCost) {
                    timeCost = 2000;
                }
                if (timeCost < 0x1f5) {
                    dampScale = 0.5f;
                } else if (timeCost < 0x5dc) {
                    dampScale = (float)(0.5 - (((float)timeCost - 500.0) / 1000.0) * 0.3);
                } else {
                    dampScale = 0.2f;
                }
                ps->pm_time = timeCost;
                ps->pm_flags = ps->pm_flags | 0x200U;
                VectorScale_080e98f4(ps->velocity, dampScale, ps->velocity);
            } else {
                VectorScale_080e98f4(ps->velocity, 0.67f, ps->velocity);
            }
            eventValue = PM_GroundSoundIndexPlus74(pml);
            BG_AddPredictableEventToPlayerstate((uint32)eventValue, (uint8)damageRating, ps);
        }
    }
}

/* ======================================================================
 * 0x080e34c6  size=483  PM_CorrectAllSolid
 * naming=inferred: id Tech 3 PM_CorrectAllSolid. Tries the 26 neighbour
 * offsets g_pmCorrectAllSolidOffsets[i] around the origin; the first one that is not allsolid
 * becomes the origin and the ground trace is redone. If none works, the
 * airborne state is set and 0 is returned.
 * ====================================================================== */
int PM_CorrectAllSolid(pmove_t *pm, pml_t *pml, trace_t *trace)
{
    playerState_t *ps;
    int i;
    float candidate[3];

    ps = pm->ps;
    for (i = 0;; i++) {
        if (0x19 < i) {
            ps->groundEntityNum = 0x3ff;
            pml->groundPlane = 0;
            pml->almostGroundPlane = 0;
            pml->walking = 0;
            Player_ClearJumpBaseZ(ps);
            return 0;
        }
        VectorAdd_080e986c(ps->origin, g_pmCorrectAllSolidOffsets[i], candidate);
        PM_Trace(pm, trace, candidate, pm->mins, pm->maxs, candidate, ps->clientNum, pm->tracemask);
        if (trace->startsolid == 0) {
            break;
        }
    }
    Vec3_Copy_Dup(candidate, ps->origin);
    candidate[0] = ps->origin[0];
    candidate[1] = ps->origin[1];
    candidate[2] = ps->origin[2] - 1.0f;
    PM_Trace(pm, trace, ps->origin, pm->mins, pm->maxs, candidate, ps->clientNum, pm->tracemask);
    memcpy(&pml->groundTrace, trace, sizeof(trace_t));
    VectorLerp(ps->origin, candidate, trace->fraction, ps->origin);
    return 1;
}

/* ======================================================================
 * 0x080e36aa  size=504  PM_GroundProbe
 * naming=unknown: a single downward probe used when the ground trace
 * reports fraction 1.0. When already airborne it probes 1 unit down, else
 * 64 units; falling without ground emits event 3 (or 4 when moving backward).
 * Clears groundEntityNum, groundPlane and walking at the end.
 * ====================================================================== */
void PM_GroundProbe(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    trace_t trace;
    float end[3];

    ps = pm->ps;
    if (ps->groundEntityNum == 0x3ff) {
        Vec3_Copy_Dup(ps->origin, end);
        end[2] = end[2] - 1.0f;
        g_pmTraceDispatch[pm->handler].trace(&trace, ps->origin, pm->mins, pm->maxs, end, ps->clientNum,
                                        pm->tracemask);
        pml->almostGroundPlane = (trace.fraction != 1.0f) ? 1 : 0;
    } else {
        Vec3_Copy_Dup(ps->origin, end);
        end[2] = end[2] - 64.0f;
        g_pmTraceDispatch[pm->handler].trace(&trace, ps->origin, pm->mins, pm->maxs, end, ps->clientNum,
                                        pm->tracemask);
        if (trace.fraction == 1.0f) {
            if (pm->cmd.forwardmove < 0) {
                BG_AnimScriptEvent(ps, 4, 0, 1);
            } else {
                BG_AnimScriptEvent(ps, 3, 0, 1);
            }
            pml->almostGroundPlane = 0;
        } else {
            pml->almostGroundPlane = (trace.fraction < 0.015625f) ? 1 : 0;
        }
    }
    ps->groundEntityNum = 0x3ff;
    pml->groundPlane = 0;
    pml->walking = 0;
}

/* ======================================================================
 * 0x080e38a2  size=932  PM_GroundTrace
 * naming=inferred: id Tech 3 PM_GroundTrace. Traces from origin +/-0.25 in
 * Z (or 1 unit down when buttonBits 0x300 is set), copies the result into the
 * ground trace, handles startsolid/allsolid with PM_CorrectAllSolid, then
 * decides between walking on ground (landing via PM_CrashLand), steep slope,
 * or airborne (fall event).
 * ====================================================================== */
void PM_GroundTrace(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    trace_t trace;
    float start[3];
    float end[3];
    float dotNormal;

    ps = pm->ps;
    start[0] = ps->origin[0];
    start[1] = ps->origin[1];
    end[0] = ps->origin[0];
    end[1] = ps->origin[1];
    if ((ps->eFlags & 0x300U) == 0) {
        start[2] = ps->origin[2] + 0.25f;
        end[2] = ps->origin[2] - 0.25f;
    } else {
        start[2] = ps->origin[2];
        end[2] = ps->origin[2] - 1.0f;
    }
    PM_Trace(pm, &trace, start, pm->mins, pm->maxs, end, ps->clientNum, pm->tracemask);
    memcpy(&pml->groundTrace, &trace, sizeof(trace_t));

    if (trace.allsolid == 0 || PM_CorrectAllSolid(pm, pml, &trace) != 0) {
        if (trace.startsolid != 0) {
            start[2] = ps->origin[2] - 0.001f;
            PM_Trace(pm, &trace, start, pm->mins, pm->maxs, end, ps->clientNum, pm->tracemask);
            if (trace.startsolid != 0) {
                ps->groundEntityNum = 0x3ff;
                pml->groundPlane = 0;
                pml->almostGroundPlane = 0;
                pml->walking = 0;
                return;
            }
            memcpy(&pml->groundTrace, &trace, sizeof(trace_t));
        }

        if (trace.fraction == 1.0f) {
            PM_GroundProbe(pm, pml);
        } else {
            dotNormal = DotProduct3(ps->velocity, trace.normal);
            if ((ps->pm_flags & 0x20U) != 0 || ps->velocity[2] <= 0.0f || (double)dotNormal <= 10.0) {
                if (trace.normal[2] < 0.7f) {
                    ps->groundEntityNum = 0x3ff;
                    pml->groundPlane = 1;
                    pml->almostGroundPlane = 1;
                    pml->walking = 0;
                    Player_ClearJumpBaseZ(ps);
                } else {
                    pml->groundPlane = 1;
                    pml->almostGroundPlane = 1;
                    pml->walking = 1;
                    if (ps->groundEntityNum == 0x3ff) {
                        PM_CrashLand(ps, pml);
                    }
                    ps->groundEntityNum = (int32)(trace.entityNum & 0xffffU);
                    PM_AddTouchEnt(pm, (int)(trace.entityNum & 0xffffU));
                }
            } else {
                if (pm->cmd.forwardmove < 0) {
                    BG_AnimScriptEvent(ps, 4, 0, 0);
                } else {
                    BG_AnimScriptEvent(ps, 3, 0, 0);
                }
                pml->almostGroundPlane = 0;
                ps->groundEntityNum = 0x3ff;
                pml->groundPlane = 0;
                pml->walking = 0;
            }
        }
    }
}

/* ======================================================================
 * 0x080e3c46  size=63  PM_StanceDurationMs
 * naming=unknown: stance transition duration in milliseconds: 400 for a
 * change to or from prone (11), and for crouch (40) when the toggle is clear;
 * 200 otherwise. The playerState argument is not read.
 * ====================================================================== */
int PM_StanceDurationMs(const playerState_t *ps, int stanceTarget, int stanceToggled)
{
    int duration;

    (void)ps;
    if (stanceTarget == 0xb) {
        duration = 400;
    } else if (stanceTarget == 0x28) {
        if (stanceToggled == 0) {
            duration = 400;
        } else {
            duration = 200;
        }
    } else {
        duration = 200;
    }
    return duration;
}

/* ======================================================================
 * 0x080e3c86  size=295  PM_EvalStanceCurve
 * naming=unknown: piecewise-linear lookup over a table of 12-byte keys
 * (key, float value, int scalar), terminated by key == -1. Returns the value
 * interpolated at 'percent' and writes the interpolated scalar to scalarOut.
 * Key 0 is the starting row (used when percent is 0 or past the last key).
 * ====================================================================== */
float PM_EvalStanceCurve(int percent, const cod2_pm_curve_key_s *table, float *scalarOut)
{
    const cod2_pm_curve_key_s *cur;
    const cod2_pm_curve_key_s *prev;
    int index;
    float frac;
    float value;

    if (percent == 0) {
        *scalarOut = (float)table[0].scalar;
        return table[0].value;
    }
    index = 1;
    cur = &table[1];
    do {
        if (percent == cur->key) {
            *scalarOut = (float)cur->scalar;
            return cur->value;
        }
        if (percent < cur->key) {
            prev = &table[index - 1];
            /* fild of the int differences stays in x87 extended precision (no float rounding) */
            frac = (float)((long double)(percent - prev->key) / (long double)(cur->key - prev->key));
            *scalarOut = (float)((long double)prev->scalar + (long double)(cur->scalar - prev->scalar) * frac);
            value = (cur->value - prev->value) * frac + prev->value;
            return value;
        }
        index = index + 1;
        cur = &table[index];
    } while (cur->key != -1);
    *scalarOut = (float)table[0].scalar;
    return table[0].value;
}

/* ======================================================================
 * 0x080e3dae  size=258  PM_StanceTransitionProgress
 * naming=unknown: fraction [0,1] of the current stance transition that has
 * elapsed, for a transition from 'fromStance' to 'toStance' (-1 means any).
 * Returns 0 when no transition is running or the stance pair does not match.
 * ====================================================================== */
float PM_StanceTransitionProgress(const pmove_t *pm, int fromStance, int toStance)
{
    const playerState_t *ps;
    float result;
    float frac;
    int duration;

    ps = pm->ps;
    if (ps->viewHeightLerpTime == 0) {
        result = 0.0f;
    } else if (fromStance == -1 || toStance == -1 ||
               (toStance == ps->viewHeightLerpTarget &&
                ((toStance != 0x28 || (fromStance == 0xb && ps->viewHeightLerpDown == 0)) ||
                 (fromStance == 0x3c && ps->viewHeightLerpDown != 0)))) {
        duration = PM_StanceDurationMs(ps, ps->viewHeightLerpTarget, ps->viewHeightLerpDown);
        frac = (float)(pm->cmd.serverTime - ps->viewHeightLerpTime) / (float)duration;
        if (frac < 0.0f) {
            frac = 0.0f;
        } else if (1.0f < frac) {
            frac = 1.0f;
        }
        result = frac;
    } else {
        result = 0.0f;
    }
    return result;
}

/* ======================================================================
 * 0x080e3eb0  size=1939  PM_UpdateStanceHeight
 * naming=unknown: moves ps->viewHeightCurrent toward the stance height
 * (viewheightTarget: 60 stand, 40 crouch, 11 prone) over time. A transition
 * (stanceTransitionStart, stanceTarget, stanceToggled) follows the curve
 * tables and nudges the velocity along the forward basis; outside the three
 * stance heights the view height moves at 180 units/s per frametime.
 * ====================================================================== */
void PM_UpdateStanceHeight(pmove_t *pm, pml_t *pml)
{
    playerState_t *ps;
    const cod2_pm_curve_key_s *table;
    float scalarOut;
    float value;
    float delta;
    float wish[3];
    float savedVel[3];
    int percent;
    int duration;

    ps = pm->ps;
    if (ps->viewHeightTarget == 0 || ps->viewHeightCurrent == 0.0f) {
        if (ps->pm_type == 4) {
            ps->viewHeightCurrent = 0.0f;
        } else {
            ps->viewHeightCurrent = (float)ps->viewHeightTarget;
        }
        return;
    }
    if (ps->viewHeightCurrent == (float)ps->viewHeightTarget && ps->viewHeightCurrent == ps->viewHeightCurrent &&
        ps->viewHeightLerpTime == 0) {
        return;
    }

    percent = 0;
    if (ps->viewHeightTarget == 0xb || ps->viewHeightTarget == 0x28 || ps->viewHeightTarget == 0x3c) {
        if (ps->viewHeightLerpTime != 0) {
            duration = PM_StanceDurationMs(ps, ps->viewHeightLerpTarget, ps->viewHeightLerpDown);
            percent = ((pm->cmd.serverTime - ps->viewHeightLerpTime) * 100) / duration;
            if (percent < 0) {
                percent = 0;
            } else if (100 < percent) {
                percent = 100;
            }
            if (percent == 100) {
                ps->viewHeightCurrent = (float)ps->viewHeightLerpTarget;
                ps->viewHeightLerpTime = 0;
                ps->viewHeightLerpPosAdj = 0.0f;
            } else {
                if (ps->viewHeightLerpTarget == 0xb) {
                    table = g_stanceHeightCurveToProne;
                } else if (ps->viewHeightLerpTarget == 0x28) {
                    table = (ps->viewHeightLerpDown == 0) ? g_stanceHeightCurveProneToCrouch : g_stanceHeightCurveStandToCrouch;
                } else {
                    table = g_stanceHeightCurveCrouchToStand;
                }
                value = PM_EvalStanceCurve(percent, table, &scalarOut);
                ps->viewHeightCurrent = value;
                if (0.05 < (double)absf_041(ps->viewHeightLerpPosAdj - scalarOut)) {
                    VectorCopy(ps->velocity, savedVel);
                    delta = scalarOut - ps->viewHeightLerpPosAdj;
                    if (ps->groundEntityNum == 0x3ff) {
                        delta = delta * 0.5f;
                    }
                    delta = delta / pml->frametime;
                    wish[0] = pml->forward[0];
                    wish[1] = pml->forward[1];
                    wish[2] = 0.0f;
                    Vec3_Normalize(wish);
                    VectorScale_080e98f4(wish, delta, ps->velocity);
                    PM_StepSlideMove(pm, pml, 1);
                    Vec3_Copy_Dup(savedVel, ps->velocity);
                    ps->viewHeightLerpPosAdj = scalarOut;
                }
            }
        }

        if (ps->viewHeightLerpTime == 0) {
            if (ps->viewHeightCurrent != (float)ps->viewHeightTarget || ps->viewHeightCurrent != ps->viewHeightCurrent) {
                ps->viewHeightLerpTime = pm->cmd.serverTime;
                if (ps->viewHeightTarget == 0xb) {
                    ps->viewHeightLerpDown = 1;
                    if (40.0 < (double)ps->viewHeightCurrent) {
                        ps->viewHeightLerpTarget = 0x28;
                    } else {
                        ps->viewHeightLerpTarget = 0xb;
                    }
                } else if (ps->viewHeightTarget == 0x28) {
                    if ((float)ps->viewHeightTarget < ps->viewHeightCurrent) {
                        ps->viewHeightLerpDown = 1;
                    } else {
                        ps->viewHeightLerpDown = 0;
                    }
                    ps->viewHeightLerpTarget = 0x28;
                } else if (ps->viewHeightTarget == 0x3c) {
                    ps->viewHeightLerpDown = 0;
                    if (ps->viewHeightCurrent < 40.0) {
                        ps->viewHeightLerpTarget = 0x28;
                    } else {
                        ps->viewHeightLerpTarget = 0x3c;
                    }
                }
            }
        } else if (ps->viewHeightTarget != ps->viewHeightLerpTarget &&
                   ((ps->viewHeightTarget < ps->viewHeightLerpTarget && ps->viewHeightLerpDown == 0) ||
                    (ps->viewHeightLerpTarget < ps->viewHeightTarget && ps->viewHeightLerpDown != 0))) {
            percent = 100 - percent;
            ps->viewHeightLerpDown = ps->viewHeightLerpDown ^ 1;
            if (ps->viewHeightLerpDown == 0) {
                if (ps->viewHeightLerpTarget == 0xb) {
                    ps->viewHeightLerpTarget = 0x28;
                } else if (ps->viewHeightLerpTarget == 0x28) {
                    ps->viewHeightLerpTarget = 0x3c;
                }
            } else if (ps->viewHeightLerpTarget == 0x3c) {
                ps->viewHeightLerpTarget = 0x28;
            } else if (ps->viewHeightLerpTarget == 0x28) {
                ps->viewHeightLerpTarget = 0xb;
            }
            if (percent == 100) {
                ps->viewHeightCurrent = (float)ps->viewHeightLerpTarget;
                ps->viewHeightLerpTime = 0;
                ps->viewHeightLerpPosAdj = 0.0f;
            } else {
                duration = PM_StanceDurationMs(ps, ps->viewHeightLerpTarget, ps->viewHeightLerpDown);
                ps->viewHeightLerpTime =
                    pm->cmd.serverTime - (int)((float)duration * (float)percent * 0.01);
                if (ps->viewHeightLerpTarget == 0xb) {
                    table = g_stanceHeightCurveToProne;
                } else if (ps->viewHeightLerpTarget == 0x28) {
                    table = (ps->viewHeightLerpDown == 0) ? g_stanceHeightCurveProneToCrouch : g_stanceHeightCurveStandToCrouch;
                } else {
                    table = g_stanceHeightCurveCrouchToStand;
                }
                PM_EvalStanceCurve(percent, table, &scalarOut);
                ps->viewHeightLerpPosAdj = scalarOut;
            }
        }
    } else {
        ps->viewHeightLerpTime = 0;
        if (ps->viewHeightCurrent < (float)ps->viewHeightTarget) {
            ps->viewHeightCurrent = ps->viewHeightCurrent + pml->frametime * 180.0f;
            if ((float)ps->viewHeightTarget <= ps->viewHeightCurrent) {
                ps->viewHeightCurrent = (float)ps->viewHeightTarget;
            }
        } else {
            ps->viewHeightCurrent = ps->viewHeightCurrent - pml->frametime * 180.0f;
            if (ps->viewHeightCurrent <= (float)ps->viewHeightTarget) {
                ps->viewHeightCurrent = (float)ps->viewHeightTarget;
            }
        }
    }
}
