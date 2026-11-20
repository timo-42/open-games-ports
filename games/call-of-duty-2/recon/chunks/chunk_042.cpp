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
#include "chunk_042_types.h"

/* Functions of chunks 041/043 with canonical signatures (their chunk JSONs are authoritative; the
 * generated cod2_prototypes.h still carries the pre-canonical spellings until the merge lane
 * regenerates it). */
int PM_ProbeClimbableLedgeEntry(int passEntity, const float *origin, float halfWidth, float height,
                                float yaw, float *outTorsoHeight, float *outAngleA, float *outAngleB,
                                int skipFirstTrace, int checkDuring, const float *groundNormal,
                                uint8 traceMode, int altMaskSelect, float probeHeight);
void PM_ClearAdsFlag(playerState_t *ps);
void PM_UpdateAdsFlag(pmove_t *pm, pml_t *pml);
int PM_WeaponStateHandler(playerState_t *ps);
void PM_UpdateWeaponPosFrac(pmove_t *pm, pml_t *pml);
void PM_StepSlideMove(pmove_t *pm, pml_t *pml, int gravity);

/* g_gameContextPtr is a raw byte pointer in the global table; this chunk views it as the game-state block. */
#define GAMELOCALS042 ((struct gameLocals_s *)g_gameContextPtr)

/* Trace function selected by pm->clipType (table of 12-byte entries at 0x081869c0). */
#define PM_TRACE(pm) (g_pmTraceDispatch[(pm)->handler].trace)

// 0x080e4644  size=3518  FUN_080e4644  status=translated  naming=inferred
// Naming: Q3 PM_CheckDuck counterpart: sets pm mins/maxs per stance, trace-tests stand/crouch/prone transitions, updates ps->viewHeightTarget; called from PmoveSingle (0x080e8934).
void PM_CheckDuck(struct pmove_s *pm, struct pml_s *pml)
{
    struct playerState_s *ps = pm->ps;
    struct trace_s tr;
    float tmpPos[3];
    float tmpPos2[3];
    int stanceCode;
    unsigned int wasProne;
    unsigned int wasStanding;

    pm->proneChange = 0;
    if (ps->pm_type == 4) {
        pm->mins[0] = -8.0f;
        pm->mins[1] = -8.0f;
        pm->mins[2] = -8.0f;
        pm->maxs[0] = 8.0f;
        pm->maxs[1] = 8.0f;
        pm->maxs[2] = 16.0f;
        ps->pm_flags = ps->pm_flags & 0xfffffffc;
        if ((pm->cmd.buttons & 0x100U) != 0) {
            pm->cmd.buttons = pm->cmd.buttons & 0xfffffeff;
            BG_AddPredictableEventToPlayerstate(0x8c, 0, ps);
        }
        ps->viewHeightTarget = 0;
        ps->viewHeightCurrent = 0;
        return;
    }

    wasProne = (unsigned int)((ps->pm_flags & 1) != 0);
    wasStanding = (unsigned int)((ps->pm_flags & 3) == 0);
    (void)wasStanding;
    pm->mins[0] = ps->mins[0];
    pm->mins[1] = ps->mins[1];
    pm->maxs[0] = ps->maxs[0];
    pm->maxs[1] = ps->maxs[1];
    pm->mins[2] = ps->mins[2];

    if (ps->pm_type < 6) {
        if ((ps->eFlags & 0x300U) == 0) {
            if ((ps->pm_flags & 0x8000) == 0) {
                if (((ps->pm_flags & 0x20) != 0) && ((pm->cmd.buttons & 0x300U) != 0)) {
                    pm->cmd.buttons = pm->cmd.buttons & 0xfffffcff;
                    BG_AddPredictableEventToPlayerstate(0x8c, 0, ps);
                }
                if ((pm->cmd.buttons & 0x100U) == 0) {
                    if ((pm->cmd.buttons & 0x200U) == 0) {
                        if ((ps->pm_flags & 1) == 0) {
                            if ((ps->pm_flags & 2) != 0) {
                                pm->maxs[2] = ps->maxs[2];
                                PM_TRACE(pm)(&tr, ps->origin, pm->mins, pm->maxs, ps->origin,
                                             ps->clientNum, pm->tracemask & 0xfdffffff);
                                if (tr.allsolid == 0) {
                                    BG_AnimScriptEvent(ps, 0xe, 0, 0);
                                    ps->pm_flags = ps->pm_flags & 0xfffffffd;
                                } else if ((pm->cmd.buttons & 0x2000U) == 0) {
                                    BG_AddPredictableEventToPlayerstate(0x8d, 1, ps);
                                }
                            }
                        } else {
                            pm->maxs[2] = ps->maxs[2];
                            PM_TRACE(pm)(&tr, ps->origin, pm->mins, pm->maxs, ps->origin,
                                         ps->clientNum, pm->tracemask & 0xfdffffff);
                            if (tr.allsolid == 0) {
                                BG_AnimScriptEvent(ps, 0x10, 0, 0);
                                ps->pm_flags = ps->pm_flags & 0xfffffffc;
                            } else {
                                pm->maxs[2] = 50.0f;
                                PM_TRACE(pm)(&tr, ps->origin, pm->mins, pm->maxs, ps->origin,
                                             ps->clientNum, pm->tracemask & 0xfdffffff);
                                if (tr.allsolid == 0) {
                                    ps->pm_flags = ps->pm_flags & 0xfffffffe;
                                    ps->pm_flags = ps->pm_flags | 2;
                                } else if ((pm->cmd.buttons & 0x2000U) == 0) {
                                    BG_AddPredictableEventToPlayerstate(0x8e, 1, ps);
                                }
                            }
                        }
                    } else if ((ps->pm_flags & 1) == 0) {
                        BG_AnimScriptEvent(ps, 0xd, 0, 0);
                        ps->pm_flags = ps->pm_flags | 2;
                    } else {
                        pm->maxs[2] = 50.0f;
                        PM_TRACE(pm)(&tr, ps->origin, pm->mins, pm->maxs, ps->origin,
                                     ps->clientNum, pm->tracemask & 0xfdffffff);
                        if (tr.allsolid == 0) {
                            BG_AnimScriptEvent(ps, 0xc, 0, 0);
                            ps->pm_flags = ps->pm_flags & 0xfffffffe;
                            ps->pm_flags = ps->pm_flags | 2;
                        } else if ((pm->cmd.buttons & 0x2000U) == 0) {
                            BG_AddPredictableEventToPlayerstate(0x8e, 2, ps);
                        }
                    }
                } else if (((ps->pm_flags & 1) == 0) &&
                           ((ps->groundEntityNum == 0x3ff) ||
                            (PM_ProbeClimbableLedgeEntry(ps->clientNum, ps->origin, pm->maxs[0], 30.0f,
                                         ps->viewangles[1], &ps->fTorsoHeight, &ps->fTorsoPitch,
                                         &ps->fWaistPitch, 0, ps->groundEntityNum != 0x3ff, 0,
                                         (char)pm->handler, 0, 66.0f) == 0))) {
                    if ((ps->groundEntityNum != 0x3ff) &&
                        (ps->pm_flags = ps->pm_flags | 0x10000, (pm->cmd.buttons & 0x2000U) == 0)) {
                        if ((ps->pm_flags & 2) == 0) {
                            BG_AddPredictableEventToPlayerstate(0x8c, 0, ps);
                        } else {
                            BG_AddPredictableEventToPlayerstate(0x8d, 0, ps);
                        }
                    }
                } else {
                    ps->pm_flags = ps->pm_flags | 1;
                    ps->pm_flags = ps->pm_flags & 0xfffffffd;
                }
            }
        } else if (((ps->eFlags & 0x100) == 0) || ((ps->eFlags & 0x200) != 0)) {
            if (((ps->eFlags & 0x200) == 0) || ((ps->eFlags & 0x100) != 0)) {
                ps->pm_flags = ps->pm_flags & 0xfffffffc;
            } else {
                ps->pm_flags = ps->pm_flags | 2;
                ps->pm_flags = ps->pm_flags & 0xfffffffe;
            }
        } else {
            ps->pm_flags = ps->pm_flags | 1;
            ps->pm_flags = ps->pm_flags & 0xfffffffd;
        }

        if (ps->viewHeightLerpTime == 0) {
            if ((ps->pm_flags & 1) == 0) {
                if (ps->viewHeightTarget == 0xb) {
                    ps->viewHeightTarget = 0x28;
                    pm->proneChange = 1;
                    BG_PlayAnim(ps, 0, 2, 0, 0, 1, 1);
                } else if ((ps->pm_flags & 2) == 0) {
                    ps->viewHeightTarget = 0x3c;
                } else {
                    ps->viewHeightTarget = 0x28;
                }
            } else if (ps->viewHeightTarget == 0x3c) {
                ps->viewHeightTarget = 0x28;
            } else if (ps->viewHeightTarget != 0xb) {
                ps->viewHeightTarget = 0xb;
                pm->proneChange = 1;
                BG_PlayAnim(ps, 0, 2, 0, 0, 1, 1);
                Player_SetJumpPending(ps);
            }
        }
        PM_UpdateStanceHeight(pm, pml);
        stanceCode = PM_StanceIndex(ps);
        if (stanceCode == 1) {
            pm->maxs[2] = 30.0f;
            ps->eFlags = ps->eFlags | 8;
            ps->eFlags = ps->eFlags & 0xfffffffb;
            ps->pm_flags = ps->pm_flags | 1;
            ps->pm_flags = ps->pm_flags & 0xfffffffd;
        } else if (stanceCode == 2) {
            pm->maxs[2] = 50.0f;
            ps->eFlags = ps->eFlags | 4;
            ps->eFlags = ps->eFlags & 0xfffffff7;
            ps->pm_flags = ps->pm_flags | 2;
            ps->pm_flags = ps->pm_flags & 0xfffffffe;
        } else {
            pm->maxs[2] = ps->maxs[2];
            ps->eFlags = ps->eFlags & 0xfffffff3;
            ps->pm_flags = ps->pm_flags & 0xfffffffc;
        }

        if (((ps->pm_flags & 1) != 0) && (wasProne == 0)) {
            if ((pm->cmd.forwardmove != 0) || (pm->cmd.rightmove != 0)) {
                ps->pm_flags = ps->pm_flags & 0xfffff7ff;
                PM_ClearAdsFlag(ps);
            }
            Vec3_Copy_Dup(ps->origin, tmpPos);
            tmpPos[2] = tmpPos[2] + 10.0f;
            PM_TRACE(pm)(&tr, ps->origin, pm->mins, pm->maxs, tmpPos, ps->clientNum,
                         pm->tracemask & 0xfdffffff);
            VectorLerp(ps->origin, tmpPos, tr.fraction, tmpPos);
            PM_TRACE(pm)(&tr, tmpPos, pm->mins, pm->maxs, ps->origin, ps->clientNum,
                         pm->tracemask & 0xfdffffff);
            VectorLerp(tmpPos, ps->origin, tr.fraction, ps->origin);
            ps->proneDirection = ps->viewangles[1];
            Vec3_Copy_Dup(ps->origin, tmpPos2);
            tmpPos2[2] = tmpPos2[2] - 0.25f;
            PM_TRACE(pm)(&tr, ps->origin, pm->mins, pm->maxs, tmpPos2, ps->clientNum,
                         pm->tracemask & 0xfdffffff);
            if ((tr.startsolid != 0) || (1.0f <= tr.fraction)) {
                ps->proneDirectionPitch = 0;
            } else {
                ps->proneDirectionPitch = AnglesToForward_ProjectedNormalized(ps->proneDirection, tr.normal);
            }
            {
                float delta = AngleDelta(ps->proneDirectionPitch, ps->viewangles[0]);
                if (delta < -45.0f) {
                    ps->proneTorsoPitch = ps->viewangles[0] - 45.0f;
                } else if (45.0f < delta) {
                    ps->proneTorsoPitch = ps->viewangles[0] + 45.0f;
                } else {
                    ps->proneTorsoPitch = ps->proneDirectionPitch;
                }
            }
        }
    } else {
        pm->maxs[2] = ps->maxs[2];
        ps->viewHeightTarget = 8;
        PM_UpdateStanceHeight(pm, pml);
    }
}

// 0x080e5402  size=455  FUN_080e5402  status=translated  naming=inferred
// Naming: bob-cycle half-period test ((old+64)^(new+64))&128 is the Q3 PM_Footsteps footstep trigger; emits a footstep event (surface type from trace on ladders, PM_FootstepSoundIndex otherwise).
void PM_FootstepEvent(struct pmove_s *pm, struct pml_s *pml, char oldBobCycle, char newBobCycle,
                      int doEvent)
{
    struct playerState_s *ps = pm->ps;
    float traceMins[3];
    float traceMaxs[3];
    float traceEnd[3];
    struct trace_s tr;
    int surfaceType;
    int traceMask;

    if ((((oldBobCycle + 0x40U) ^ (newBobCycle + 0x40U)) & 0x80) != 0) {
        if (ps->groundEntityNum == 0x3ff) {
            if ((doEvent != 0) && ((ps->pm_flags & 0x20) != 0)) {
                Vec3_Copy_Dup(pm->mins, traceMins);
                traceMins[0] = traceMins[0] + 6.0f;
                traceMins[1] = traceMins[1] + 6.0f;
                traceMins[2] = 8.0f;
                Vec3_Copy_Dup(pm->maxs, traceMaxs);
                traceMaxs[0] = traceMaxs[0] - 6.0f;
                traceMaxs[1] = traceMaxs[1] - 6.0f;
                if (traceMaxs[2] < traceMins[2]) {
                    traceMaxs[2] = traceMins[2];
                }
                traceMask = pm->tracemask & 0xfdfeffff;
                VectorMA_080e992c(ps->origin, -31.0f, ps->vLadderVec, traceEnd);
                PM_Trace(pm, &tr, ps->origin, traceMins, traceMaxs, traceEnd, ps->clientNum,
                            traceMask);
                surfaceType = (tr.surfaceFlags & 0x1f00000) >> 0x14;
                if ((tr.fraction == 1.0f) || (surfaceType == 0)) {
                    surfaceType = 0x15;
                }
                PM_AddEvent(ps, surfaceType + 1);
            }
        } else if (doEvent != 0) {
            int ev = PM_FootstepSoundIndex(ps, pml);
            PM_AddEvent(ps, ev);
        }
    }
}

// 0x080e55ca  size=168  FUN_080e55ca  status=translated  naming=unknown
// Naming: descriptive; true when stance is standing (not prone/crouch per PM_StanceIndex), the 0x100 flag is clear and xyspeed >= dvar at 0x0862703c.
bool PM_FootstepSpeedOk(struct pmove_s *pm)
{
    struct playerState_s *ps = pm->ps;
    unsigned int flag100 = (unsigned int)ps->pm_flags & 0x100;
    int stance = PM_StanceIndex(ps);

    if ((stance != 1) && (stance != 2)) {
        if ((((unsigned char)ps->pm_flags) & 0x80) == 0) {
            if (flag100 == 0) {
                return dvar_player_footstepsThreshhold->current.value <= pm->xyspeed;
            }
        } else if (flag100 == 0) {
            return dvar_player_footstepsThreshhold->current.value <= pm->xyspeed;
        }
    }
    return false;
}

// 0x080e5672  size=185  FUN_080e5672  status=translated  naming=unknown
// Naming: descriptive; maps a yaw delta (degrees) to one of four animation indices 0x1f/0x21/0x22/0x20 by 90-degree quadrant.
int PM_YawQuadrantAnim(float yawDelta)
{
    int result;

    if (yawDelta < 0.0f) {
        yawDelta = yawDelta + 360.0f;
    }
    if ((315.0f <= yawDelta) || (yawDelta < 45.0f)) {
        result = 0x1f;
    } else if (45.0f <= yawDelta && yawDelta < 135.0f) {
        result = 0x21;
    } else if (135.0f <= yawDelta && yawDelta < 225.0f) {
        result = 0x20;
    } else {
        result = 0x22; /* also reached for NaN (0x080e571f), as in the binary */
    }
    return result;
}

// 0x080e572c  size=4183  FUN_080e572c  status=translated  naming=inferred
// Naming: Q3 PM_Footsteps counterpart (selects legs animation, advances ps->bobCycle by speed, emits footsteps); string 'turn anim end time is %i, time is %i\n' is printed from it.
void PM_Footsteps(struct pmove_s *pm, struct pml_s *pml)
{
    struct playerState_s *ps = pm->ps;
    struct clientTurnView_s *anim;
    int msSinceField94;
    int unusedMsSince;
    unsigned int stance;
    int animResult = -1;
    int leaning;
    int oldBob;
    int prevLeaning;
    float bobScale = 0.0f;
    float moveSpeedRef;
    float blend;
    char turnType;
    int footstepOk;
    int newBob;

    if (ps->pm_type >= 6) {
        return;
    }
    if (ps->clientNum < 0x40) {
        anim = &GAMELOCALS042->clientAnim[0] + ps->clientNum;
    } else {
        anim = 0;
    }
    msSinceField94 = ps->damageDuration - dvar_player_dmgtimer_stumbleTime->current.integer;
    if (msSinceField94 < 0) {
        msSinceField94 = 0;
    }
    unusedMsSince = ps->damageDuration - dvar_player_dmgtimer_flinchTime->current.integer;
    if (unusedMsSince < 0) {
        unusedMsSince = 0;
    }
    pm->xyspeed = Float_Sqrt_080e9590(ps->velocity[1] * ps->velocity[1] + ps->velocity[0] * ps->velocity[0]);

    if ((ps->eFlags & 0x300) == 0) {
        stance = (unsigned int)PM_StanceIndex(ps);
        if ((ps->groundEntityNum == 0x3ff) && (ps->pm_type != 1)) {
            if ((ps->pm_flags & 0x20) != 0) {
                float climbSpeed;
                if (pm->cmd.serverTime - ps->jumpTime < 300) {
                    return;
                }
                climbSpeed = ps->velocity[2];
                if ((((ps->pm_flags & 0x100) != 0) || (ps->leanf != 0.0f))) {
                    bobScale = (climbSpeed / 38.100002f) * 0.35f;
                } else {
                    bobScale = (climbSpeed / 95.25f) * 0.45f;
                }
                if (0.0f <= climbSpeed) {
                    BG_AnimScriptAnimation(ps, 3, 0x12, 1);
                } else {
                    BG_AnimScriptAnimation(ps, 3, 0x13, 1);
                }
                oldBob = ps->bobCycle;
                ps->bobCycle = (int)((float)pml->msec * bobScale + (float)oldBob) & 0xff;
                PM_FootstepEvent(pm, pml, (char)oldBob, (char)ps->bobCycle, 1);
            }
            if (stance == (unsigned int)(ps->pm_flags & 3)) {
                return;
            }
        }
        leaning = 0;
        if ((((ps->pm_flags & 0x100) != 0) || (ps->leanf != 0.0f))) {
            leaning = 1;
        }
        if ((pm->xyspeed < dvar_player_moveThreshhold->current.value) || (ps->pm_type == 1)) {
            if (pm->xyspeed < 1.0f) {
                ps->bobCycle = 0;
            }
            turnType = '\0';
            if ((anim != 0) && (dvar_player_turnAnims->current.enabled != '\0')) {
                if ((anim->turnAnimType != '\0') && (anim->turnAnimEndTime != 0)) {
                    Com_DPrintf("turn anim end time is %i, time is %i\n", anim->turnAnimEndTime,
                                GAMELOCALS042->time);
                }
                if (anim->turnActive == 0) {
                    if (GAMELOCALS042->time < anim->turnAnimEndTime) {
                        anim->turnYawPrev = anim->turnYawCurrent;
                        return;
                    }
                    if (anim->turnAnimEndTime != 0) {
                        anim->turnAnimEndTime = 0;
                        anim->turnYawPrev = anim->turnYawCurrent;
                    }
                } else {
                    if (anim->turnYawCurrent < anim->turnYawPrev) {
                        turnType = '\x0e';
                    } else {
                        turnType = '\x0f';
                    }
                    anim->turnYawPrev = anim->turnYawCurrent;
                    anim->turnAnimType = turnType;
                    if (anim->turnAnimEndTime < GAMELOCALS042->time) {
                        anim->turnAnimEndTime = 0;
                    }
                }
            }
            if (ps->viewHeightTarget == 0xb) {
                animResult = BG_AnimScriptAnimation(ps, 3, 3, 1);
            } else if (ps->viewHeightTarget == 0x28) {
                if (turnType == '\x0e') {
                    animResult = BG_AnimScriptAnimation(ps, 3, 0x10, 1);
                    if ((0 < animResult) && (anim->turnAnimEndTime == 0)) {
                        anim->turnAnimEndTime = ps->legsAnimDuration + GAMELOCALS042->time;
                    }
                } else if (turnType == '\x0f') {
                    animResult = BG_AnimScriptAnimation(ps, 3, 0x11, 1);
                    if ((0 < animResult) && (anim->turnAnimEndTime == 0)) {
                        anim->turnAnimEndTime = ps->legsAnimDuration + GAMELOCALS042->time;
                    }
                } else {
                    animResult = BG_AnimScriptAnimation(ps, 3, 2, 1);
                }
            } else if (turnType == '\x0e') {
                animResult = BG_AnimScriptAnimation(ps, 3, 0xe, 1);
                if ((0 < animResult) && (anim->turnAnimEndTime == 0)) {
                    anim->turnAnimEndTime = ps->legsAnimDuration + GAMELOCALS042->time;
                }
            } else if (turnType == '\x0f') {
                animResult = BG_AnimScriptAnimation(ps, 3, 0xf, 1);
                if ((0 < animResult) && (anim->turnAnimEndTime == 0)) {
                    anim->turnAnimEndTime = ps->legsAnimDuration + GAMELOCALS042->time;
                }
            } else {
                if (msSinceField94 < ps->damageTimer) {
                    int quadrantAnim = PM_YawQuadrantAnim((float)ps->flinchYaw);
                    BG_AnimScriptAnimation(ps, 3, quadrantAnim, 1);
                    return;
                }
                animResult = BG_AnimScriptAnimation(ps, 3, 1, 1);
            }
            if (animResult < 0) {
                if (ps->viewHeightTarget == 0x28) {
                    BG_AnimScriptAnimation(ps, 3, 2, 1);
                } else if (msSinceField94 < ps->damageTimer) {
                    BG_AnimScriptAnimation(ps, 3, 0x1f, 1);
                } else {
                    BG_AnimScriptAnimation(ps, 3, 1, 1);
                }
            }
        } else {
            prevLeaning = leaning;
            moveSpeedRef = (float)ps->speed;
            if ((char)pm->cmd.forwardmove == '\0') {
                if (pm->cmd.rightmove != 0) {
                    moveSpeedRef = moveSpeedRef * ((dvar_player_strafeSpeedScale->current.value - 1.0f) * 0.75f + 1.0f);
                    if (pm->cmd.rightmove < 1) {
                        BG_UpdateConditionValue(ps->clientNum, 8, 1, 1);
                    } else {
                        BG_UpdateConditionValue(ps->clientNum, 8, 2, 1);
                    }
                }
            } else {
                if (pm->cmd.rightmove == 0) {
                    if (pm->cmd.forwardmove < 0) {
                        moveSpeedRef = moveSpeedRef * dvar_player_backSpeedScale->current.value;
                    }
                } else {
                    moveSpeedRef = moveSpeedRef *
                                   ((dvar_player_strafeSpeedScale->current.value - 1.0f) * 0.75f + 1.0f + 1.0f) * 0.5f;
                    if (pm->cmd.forwardmove < 0) {
                        moveSpeedRef = moveSpeedRef * (dvar_player_backSpeedScale->current.value + 1.0f) * 0.5f;
                    }
                }
                BG_UpdateConditionValue(ps->clientNum, 8, 0, 1);
            }
            if (leaning != 0) {
                moveSpeedRef = moveSpeedRef * 0.4f;
            }
            blend = PM_StanceTransitionProgress(pm, 0x28, 0xb);
            if (blend != 0.0f) {
                moveSpeedRef = moveSpeedRef * ((1.0f - blend) * 0.65f + blend * 0.15f);
            } else {
                blend = PM_StanceTransitionProgress(pm, 0xb, 0x28);
                if (blend != 0.0f) {
                    moveSpeedRef = moveSpeedRef * ((1.0f - blend) * 0.15f + blend * 0.65f);
                } else if (stance == 1) {
                    moveSpeedRef = moveSpeedRef * 0.15f;
                } else if (stance == 2) {
                    moveSpeedRef = moveSpeedRef * 0.65f;
                }
            }
            if (stance == 1) {
                if (leaning == 0) {
                    bobScale = (pm->xyspeed / moveSpeedRef) * 0.25f;
                } else {
                    bobScale = (pm->xyspeed / moveSpeedRef) * 0.24f;
                }
                if ((((unsigned char)ps->pm_flags) & 0x80) == 0) {
                    animResult = BG_AnimScriptAnimation(ps, 3, 8, 1);
                } else {
                    animResult = BG_AnimScriptAnimation(ps, 3, 9, 1);
                }
            } else if (stance == 2) {
                if (leaning == 0) {
                    bobScale = (pm->xyspeed / moveSpeedRef) * 0.34f;
                } else {
                    bobScale = (pm->xyspeed / moveSpeedRef) * 0.315f;
                }
                if ((((unsigned char)ps->pm_flags) & 0x80) == 0) {
                    if (leaning == 0) {
                        if (msSinceField94 < ps->damageTimer) {
                            animResult = BG_AnimScriptAnimation(ps, 3, 0x27, 1);
                        } else {
                            animResult = BG_AnimScriptAnimation(ps, 3, 0xc, 1);
                        }
                    } else if (msSinceField94 < ps->damageTimer) {
                        animResult = BG_AnimScriptAnimation(ps, 3, 0x27, 1);
                    } else {
                        animResult = BG_AnimScriptAnimation(ps, 3, 6, 1);
                    }
                } else if (leaning == 0) {
                    if (msSinceField94 < ps->damageTimer) {
                        animResult = BG_AnimScriptAnimation(ps, 3, 0x28, 1);
                    } else {
                        animResult = BG_AnimScriptAnimation(ps, 3, 0xd, 1);
                    }
                } else if (msSinceField94 < ps->damageTimer) {
                    animResult = BG_AnimScriptAnimation(ps, 3, 0x28, 1);
                } else {
                    animResult = BG_AnimScriptAnimation(ps, 3, 7, 1);
                }
            } else if ((((unsigned char)ps->pm_flags) & 0x80) == 0) {
                if (leaning == 0) {
                    bobScale = (pm->xyspeed / moveSpeedRef) * 0.335f;
                    if (msSinceField94 < ps->damageTimer) {
                        animResult = BG_AnimScriptAnimation(ps, 3, 0x23, 1);
                    } else {
                        animResult = BG_AnimScriptAnimation(ps, 3, 10, 1);
                    }
                } else {
                    bobScale = (pm->xyspeed / moveSpeedRef) * 0.305f;
                    if (msSinceField94 < ps->damageTimer) {
                        animResult = BG_AnimScriptAnimation(ps, 3, 0x25, 1);
                    } else {
                        animResult = BG_AnimScriptAnimation(ps, 3, 4, 1);
                    }
                }
            } else if (leaning == 0) {
                bobScale = (pm->xyspeed / moveSpeedRef) * 0.36f;
                if (msSinceField94 < ps->damageTimer) {
                    animResult = BG_AnimScriptAnimation(ps, 3, 0x24, 1);
                } else {
                    animResult = BG_AnimScriptAnimation(ps, 3, 0xb, 1);
                }
            } else {
                bobScale = (pm->xyspeed / moveSpeedRef) * 0.325f;
                if (msSinceField94 < ps->damageTimer) {
                    animResult = BG_AnimScriptAnimation(ps, 3, 0x26, 1);
                } else {
                    animResult = BG_AnimScriptAnimation(ps, 3, 5, 1);
                }
            }
            footstepOk = PM_FootstepSpeedOk(pm);
            oldBob = ps->bobCycle;
            newBob = (int)((float)pml->msec * bobScale + (float)oldBob);
            ps->bobCycle = newBob & 0xff;
            if ((pm->cmd.forwardmove == 0) && (pm->cmd.rightmove == 0)) {
                if (pm->xyspeed <= 120.0f) {
                    if (ps->viewHeightTarget == 0xb) {
                        animResult = BG_AnimScriptAnimation(ps, 3, 3, 1);
                    } else if (ps->viewHeightTarget == 0x28) {
                        animResult = BG_AnimScriptAnimation(ps, 3, 2, 1);
                    }
                    if (animResult < 0) {
                        if (msSinceField94 < ps->damageTimer) {
                            BG_AnimScriptAnimation(ps, 3, 0x1f, 1);
                        } else {
                            BG_AnimScriptAnimation(ps, 3, 1, 1);
                        }
                    }
                }
            } else {
                if (animResult < 0) {
                    BG_AnimScriptAnimation(ps, 3, 1, 1);
                }
                PM_FootstepEvent(pm, pml, (char)oldBob, (char)ps->bobCycle, footstepOk);
            }
        }
    } else if ((ps->pm_flags & 1) == 0) {
        if ((ps->pm_flags & 2) == 0) {
            BG_AnimScriptAnimation(ps, 3, 1, 1);
        } else {
            BG_AnimScriptAnimation(ps, 3, 2, 1);
        }
    } else {
        BG_AnimScriptAnimation(ps, 3, 3, 1);
    }
}

// 0x080e6784  size=434  FUN_080e6784  status=translated  naming=unknown
// Naming: descriptive; when xyspeed exceeds a dvar threshold and a speed-scaled delay has elapsed since ps->foliageSoundTime, traces a 0.75-scaled box in place and emits event 0x8b if it starts solid.
void PM_CheckMoveBump(struct pmove_s *pm)
{
    struct playerState_s *ps = pm->ps;
    float scaledMins[3];
    float scaledMaxs[3];
    struct trace_s tr;
    float frac;
    int delay;

    if (pm->xyspeed < dvar_bg_foliagesnd_minspeed->current.value) {
        if (ps->foliageSoundTime + dvar_bg_foliagesnd_resetinterval->current.integer < pm->cmd.serverTime) {
            ps->foliageSoundTime = 0;
        }
    } else {
        frac = (pm->xyspeed - dvar_bg_foliagesnd_minspeed->current.value) /
               (dvar_bg_foliagesnd_maxspeed->current.value - dvar_bg_foliagesnd_minspeed->current.value);
        if (1.0f < frac) {
            frac = 1.0f;
        }
        delay = (int)((float)dvar_bg_foliagesnd_slowinterval->current.integer +
                      (float)(dvar_bg_foliagesnd_fastinterval->current.integer - dvar_bg_foliagesnd_slowinterval->current.integer) * frac);
        if (ps->foliageSoundTime + delay < pm->cmd.serverTime) {
            VectorScale_080e98f4(pm->mins, 0.75f, scaledMins);
            VectorScale_080e98f4(pm->maxs, 0.75f, scaledMaxs);
            scaledMaxs[2] = pm->maxs[2] * 0.9f;
            PM_Trace(pm, &tr, ps->origin, scaledMins, scaledMaxs, ps->origin, ps->clientNum, 2);
            if (tr.startsolid != 0) {
                PM_AddEvent(ps, 0x8b);
                ps->foliageSoundTime = pm->cmd.serverTime;
            }
        }
    }
}

// 0x080e6936  size=190  FUN_080e6936  status=translated  naming=inferred
// Naming: Q3 PM_DropTimers counterpart: counts down ps->pm_time (clearing flag bits when it expires) and two other ms timers.
void PM_DropTimers(struct playerState_s *ps, struct pml_s *pml)
{
    if (ps->pm_time != 0) {
        if (pml->msec < ps->pm_time) {
            ps->pm_time = ps->pm_time - pml->msec;
        } else {
            ps->pm_flags = ps->pm_flags & 0xfff7f9ff;
            ps->pm_time = 0;
        }
    }
    if ((0 < ps->legsTimer) && (ps->legsTimer = ps->legsTimer - pml->msec, ps->legsTimer < 0)) {
        ps->legsTimer = 0;
    }
    if ((0 < ps->torsoTimer) && (ps->torsoTimer = ps->torsoTimer - pml->msec, ps->torsoTimer < 0)) {
        ps->torsoTimer = 0;
    }
}

// 0x080e69f4  size=951  FUN_080e69f4  status=translated  naming=inferred
// Naming: lean update; moves ps->leanf (+0x4c) toward -/+0.25 or 0.5 depending on buttons 0x40/0x80 and clamps it against a trace so the leaned head box stays clear.
void PM_UpdateLean(struct playerState_s *ps, float msec, struct usercmd_s *cmd, traceFn_t trace)
{
    int leanDir;
    int stance;
    float maxLean;
    float lean;
    float leanStart[3];
    float leanEnd[3];
    float traceMins[3];
    float traceMaxs[3];
    struct trace_s tr;
    float limit;
    float fraction;
    int signv;

    leanDir = 0;
    lean = 0.0f;
    if ((((((unsigned char)cmd->buttons) & 0xc0) != 0) && ((ps->pm_flags & 0x8000) == 0)) &&
        (ps->pm_type < 6) && ((ps->groundEntityNum != 0x3ff) || (ps->pm_type == 1))) {
        if ((cmd->buttons & 0x40U) != 0) {
            leanDir = -1;
        }
        if ((((unsigned char)cmd->buttons) & 0x80) != 0) {
            leanDir = leanDir + 1;
        }
    }
    if ((ps->eFlags & 0x300U) != 0) {
        leanDir = 0;
    }
    stance = PM_StanceIndex(ps);
    if (stance == 1) {
        maxLean = 0.25f;
    } else {
        maxLean = 0.5f;
    }
    lean = ps->leanf;
    if (leanDir == 0) {
        if (0.0f < lean) {
            lean = lean - (msec / 280.0f) * maxLean;
            if (lean < 0.0f) {
                lean = 0.0f;
            }
        } else if ((lean < 0.0f) && (lean = lean + (msec / 280.0f) * maxLean, 0.0f < lean)) {
            lean = 0.0f;
        }
    } else if (leanDir < 1) {
        if (-maxLean < lean) {
            lean = lean - (msec / 350.0f) * maxLean;
        }
        if (lean < -maxLean) {
            lean = -maxLean;
        }
    } else {
        if (lean < maxLean) {
            lean = lean + (msec / 350.0f) * maxLean;
        }
        if (maxLean < lean) {
            lean = maxLean;
        }
    }
    ps->leanf = lean;
    if (ps->leanf != 0.0f) {
        signv = Float_Sign(ps->leanf);
        Vec3_Copy_Dup(ps->origin, leanStart);
        leanStart[2] = leanStart[2] + ps->viewHeightCurrent;
        Vec3_Copy_Dup(leanStart, leanEnd);
        BG_ApplyLeanRotationOffset(leanEnd, ps->viewangles[1], (float)signv, 16.0f, 20.0f);
        Vec3_Set_Dup(traceMins, -8.0f, -8.0f, -8.0f);
        Vec3_Set_Dup(traceMaxs, 8.0f, 8.0f, 8.0f);
        trace(&tr, leanStart, traceMins, traceMaxs, leanEnd, ps->clientNum, 0x2810011);
        fraction = Ease_OutQuadInverse(tr.fraction);
        limit = Float_Abs_080e9576(ps->leanf);
        if (fraction < limit) {
            signv = Float_Sign(ps->leanf);
            ps->leanf = fraction * (float)signv;
        }
    }
}

// 0x080e6dac  size=267  FUN_080e6dac  status=translated  naming=unknown
// Naming: descriptive; tests (via PM_ProbeClimbableLedgeEntry) whether the body can face a candidate yaw, interpolating a probe yaw and distance by how far the yaw is from the view yaw (0..240 degrees).
int PM_CheckBodyYawClear(struct playerState_s *ps, float yaw, char clipType)
{
    float delta;
    float weight;
    float probeYaw;

    delta = AngleDelta(yaw, ps->viewangles[1]);
    weight = Float_Abs_080e9576(delta) / 240.0f;
    probeYaw = AngleNormalize360_Loop(yaw - (1.0f - weight) * delta);
    return PM_ProbeClimbableLedgeEntry(ps->clientNum, ps->origin, ps->maxs[0], 30.0f, probeYaw, &ps->fTorsoHeight,
                       &ps->fTorsoPitch, &ps->fWaistPitch, 1, ps->groundEntityNum != 0x3ff, 0,
                       clipType, 0, (1.0f - weight) * 66.0f + weight * 45.0f);
}

// 0x080e6eb8  size=3399  FUN_080e6eb8  status=translated  naming=inferred
// Naming: Q3 PM_UpdateViewAngles counterpart: applies cmd angles + delta_angles with pitch clamps (dvars 0x08626fb4/b8), ladder/prone yaw limits and body-yaw follow, then calls PM_UpdateLean.
void PM_UpdateViewAngles(struct playerState_s *ps, float msec, struct usercmd_s *cmd,
                         byte clipType)
{
    float viewYawStart;
    float viewYawAfterCmd;
    unsigned int pitchUp;
    unsigned int pitchUpMasked;
    unsigned int pitchDown;
    unsigned int pitchDownMasked;
    int axis;
    short angle;
    float yawDiff;
    float slack;
    float targetYaw;
    float stepYaw;
    float delta;
    bool bStep;
    bool bAdjusted;
    int clearResult;
    int traceResult;
    int result2;
    float tmp;

    if (ps->pm_type == 5) {
        return;
    }
    if (ps->pm_type >= 6) {
        if (ps->stats[1] == 999) {
            ps->stats[1] = (int)((float)((short)ps->delta_angles[1] + (short)cmd->angles[1]) *
                                  0.005493164f);
        }
        PM_UpdateLean(ps, msec, cmd, g_pmTraceDispatch[(unsigned int)clipType].trace);
        return;
    }

    viewYawStart = ps->viewangles[1];
    pitchUp = (unsigned int)(dvar_player_view_pitch_up->current.value * 182.04445f);
    pitchUpMasked = pitchUp & 0xffff;
    pitchDown = (unsigned int)(dvar_player_view_pitch_down->current.value * 182.04445f);
    pitchDownMasked = pitchDown & 0xffff;
    for (axis = 0; axis < 3; axis = axis + 1) {
        angle = (short)((short)ps->delta_angles[axis] + (short)cmd->angles[axis]);
        if (axis == 0) {
            if ((int)pitchDownMasked < (int)angle) {
                ps->delta_angles[0] = pitchDownMasked - cmd->angles[0];
                angle = (short)pitchDown;
            } else if ((int)angle < (int)-pitchUpMasked) {
                ps->delta_angles[0] = -cmd->angles[0] - pitchUpMasked;
                angle = -(short)pitchUp;
            }
        }
        ps->viewangles[axis] = (float)angle * 0.005493164f;
    }
    viewYawAfterCmd = ps->viewangles[1];

    if ((ps->eFlags & 0x300) != 0) {
        for (axis = 0; axis < 2; axis = axis + 1) {
            delta = AngleDelta(ps->viewAngleClampBase[axis], ps->viewangles[axis]);
            if ((ps->viewAngleClampRange[axis] < delta) || (delta < -ps->viewAngleClampRange[axis])) {
                if (ps->viewAngleClampRange[axis] < delta) {
                    delta = delta - ps->viewAngleClampRange[axis];
                } else {
                    delta = delta + ps->viewAngleClampRange[axis];
                }
                ps->delta_angles[axis + 0] = ((int)(delta * 182.04445f) & 0xffffU) +
                                             ps->delta_angles[axis];
                if (0.0f < delta) {
                    ps->viewangles[axis] =
                        AngleNormalize360_Loop(ps->viewAngleClampBase[axis] - ps->viewAngleClampRange[axis]);
                } else {
                    ps->viewangles[axis] =
                        AngleNormalize360_Loop(ps->viewAngleClampBase[axis] + ps->viewAngleClampRange[axis]);
                }
            }
        }
        return;
    }

    if ((ps->pm_flags & 4) != 0) {
        Mantle_ApplyYawCap(ps);
        return;
    }

    if ((((ps->pm_flags & 0x20) != 0) && (ps->groundEntityNum == 0x3ff)) &&
        (dvar_bg_ladder_yawcap->current.value != 0.0f)) {
        tmp = Vec2_AngleDegrees(ps->vLadderVec);
        yawDiff = 180.0f + tmp;
        delta = AngleDelta(yawDiff, ps->viewangles[1]);
        if ((dvar_bg_ladder_yawcap->current.value < delta) || (delta < -dvar_bg_ladder_yawcap->current.value)) {
            if (dvar_bg_ladder_yawcap->current.value < delta) {
                delta = delta - dvar_bg_ladder_yawcap->current.value;
            } else {
                delta = delta + dvar_bg_ladder_yawcap->current.value;
            }
            ps->delta_angles[1] = ((int)(delta * 182.04445f) & 0xffffU) + ps->delta_angles[1];
            if (0.0f < delta) {
                ps->viewangles[1] = AngleNormalize360_Loop(yawDiff - dvar_bg_ladder_yawcap->current.value);
            } else {
                ps->viewangles[1] = AngleNormalize360_Loop(yawDiff + dvar_bg_ladder_yawcap->current.value);
            }
        }
    }

    if (((ps->pm_flags & 1) != 0) && ((ps->eFlags & 0x300) == 0)) {
        bAdjusted = false;
        delta = AngleDelta(ps->proneDirection, ps->viewangles[1]);
        if (((dvar_bg_prone_yawcap->current.value - 5.0f < delta) || (delta < -(dvar_bg_prone_yawcap->current.value - 5.0f))) ||
            (((cmd->forwardmove != 0) || (cmd->rightmove != 0)) && (delta != 0.0f))) {
            if (Float_Abs_080e9576(delta) < 0.001f * 55.0f * msec) {
                targetYaw = ps->viewangles[1];
            } else if (0.0f < delta) {
                targetYaw = ps->proneDirection - msec * 55.0f * 0.001f;
            } else {
                targetYaw = ps->proneDirection + msec * 55.0f * 0.001f;
            }
            bStep = true;
            while (clearResult = PM_CheckBodyYawClear(ps, targetYaw, (char)clipType),
                   clearResult == 0) {
                if (!bStep) {
                    goto after_probe;
                }
                stepYaw = AngleDelta(ps->proneDirection, targetYaw);
                bStep = 1.0f < Float_Abs_080e9576(stepYaw);
                if (bStep) {
                    if (0.0f < stepYaw) {
                        stepYaw = 1.0f;
                    } else {
                        stepYaw = -1.0f;
                    }
                } else {
                    bAdjusted = true;
                }
                targetYaw = AngleNormalize360_Loop(targetYaw + stepYaw);
            }
            traceResult = PM_ProbeClimbableLedgeEntry(ps->clientNum, ps->origin, ps->maxs[0], 30.0f,
                                      ps->viewangles[1], (float *)0, (float *)0, (float *)0, 1,
                                      ps->groundEntityNum != 0x3ff, 0, (char)clipType, 0, 45.0f);
            if ((traceResult != 0) &&
                (traceResult = PM_ProbeClimbableLedgeEntry(ps->clientNum, ps->origin, ps->maxs[0], 30.0f,
                                           targetYaw, (float *)0, (float *)0, (float *)0, 1, ps->groundEntityNum != 0x3ff, 0,
                                           (char)clipType, 0, 45.0f),
                 traceResult != 0)) {
                ps->proneDirection = targetYaw;
            }
            if (traceResult == 0) {
                bAdjusted = true;
            }
        }
after_probe:
        yawDiff = AngleDelta(ps->proneDirection, ps->viewangles[1]);
        if (yawDiff != 0.0f) {
            targetYaw = ps->proneDirection;
            bStep = true;
            while ((traceResult = PM_ProbeClimbableLedgeEntry(ps->clientNum, ps->origin, ps->maxs[0], 30.0f,
                                              targetYaw, (float *)0, (float *)0, (float *)0, 1,
                                              ps->groundEntityNum != 0x3ff, 0, (char)clipType, 0,
                                              45.0f),
                    traceResult == 0) ||
                   (result2 = PM_CheckBodyYawClear(ps, targetYaw, (char)clipType),
                    result2 == 0)) {
                if (!bStep) {
                    goto after_step;
                }
                bStep = 1.0f < Float_Abs_080e9576(yawDiff);
                if (bStep) {
                    if (0.0f < yawDiff) {
                        yawDiff = 1.0f;
                    } else {
                        yawDiff = -1.0f;
                    }
                }
                bAdjusted = true;
                ps->delta_angles[1] =
                    ((int)(yawDiff * 182.04445f) & 0xffffU) + ps->delta_angles[1];
                ps->viewangles[1] = AngleNormalize360_Loop(ps->viewangles[1] + yawDiff);
                yawDiff = AngleDelta(ps->proneDirection, ps->viewangles[1]);
                if (traceResult == 0) {
                    targetYaw = AngleNormalize360_Loop(targetYaw + yawDiff);
                }
            }
            ps->proneDirection = targetYaw;
        }
after_step:
        if ((dvar_bg_prone_yawcap->current.value < yawDiff) || (yawDiff < -dvar_bg_prone_yawcap->current.value)) {
            if (dvar_bg_prone_yawcap->current.value < yawDiff) {
                yawDiff = yawDiff - dvar_bg_prone_yawcap->current.value;
            } else {
                yawDiff = yawDiff + dvar_bg_prone_yawcap->current.value;
            }
            ps->delta_angles[1] = ((int)(yawDiff * 182.04445f) & 0xffffU) + ps->delta_angles[1];
            if (0.0f < yawDiff) {
                ps->viewangles[1] = AngleNormalize360_Loop(ps->proneDirection - dvar_bg_prone_yawcap->current.value);
            } else {
                ps->viewangles[1] = AngleNormalize360_Loop(ps->proneDirection + dvar_bg_prone_yawcap->current.value);
            }
        }
        if (bAdjusted) {
            ps->pm_flags = ps->pm_flags | 0x10000;
            slack = AngleDelta(viewYawStart, ps->viewangles[1]);
            if ((Float_Abs_080e9576(slack) <= 1.0f) &&
                (delta = AngleDelta(viewYawAfterCmd, ps->viewangles[1]), 0.0f < slack * delta)) {
                ps->viewangles[1] = AngleNormalize360_Loop(ps->viewangles[1] + slack * 0.98f);
                ps->delta_angles[1] =
                    ((int)(slack * 0.98f * 182.04445f) & 0xffffU) + ps->delta_angles[1];
            }
        }
        yawDiff = AngleDelta(ps->proneTorsoPitch, ps->viewangles[0]);
        if ((45.0f < yawDiff) || (yawDiff < -45.0f)) {
            if (45.0f < yawDiff) {
                yawDiff = yawDiff - 45.0f;
            } else {
                yawDiff = yawDiff + 45.0f;
            }
            ps->delta_angles[0] = ((int)(yawDiff * 182.04445f) & 0xffffU) + ps->delta_angles[0];
            if (0.0f < yawDiff) {
                ps->viewangles[0] = AngleNormalize180_Loop(ps->proneTorsoPitch - 45.0f);
            } else {
                ps->viewangles[0] = AngleNormalize180_Loop(ps->proneTorsoPitch + 45.0f);
            }
        }
    }
    if (((ps->pm_type != 3) && (ps->pm_type != 2)) && (ps->pm_type != 4)) {
        PM_UpdateLean(ps, msec, cmd, g_pmTraceDispatch[(unsigned int)clipType].trace);
    }
}

// 0x080e7c00  size=836  FUN_080e7c00  status=translated  naming=unknown
// Naming: descriptive; while prone, checks the body box via PM_ProbeClimbableLedgeEntry and eases ps->proneDirectionPitch / ps->field_58c (yaw-like angles) toward the ground-plane slope angle at 70 deg/s.
void PM_SmoothBodyAngles(struct pmove_s *pm, struct pml_s *pml)
{
    struct playerState_s *ps = pm->ps;
    const float *groundNormal;
    float target;
    float delta;
    int result;

    if (((ps->pm_flags ^ 1) & 1) == 0) {
        if (ps->groundEntityNum == 0x3ff) {
            if (pml->groundPlane == 0) {
                groundNormal = 0;
            } else {
                groundNormal = pml->groundTrace.normal;
            }
            result = PM_ProbeClimbableLedgeEntry(ps->clientNum, ps->origin, ps->maxs[0], 30.0f, ps->proneDirection,
                                 &ps->fTorsoHeight, &ps->fTorsoPitch, &ps->fWaistPitch, 1,
                                 ps->groundEntityNum != 0x3ff, groundNormal, (char)pm->handler,
                                 0, 66.0f);
            if (result == 0) {
                BG_AddPredictableEventToPlayerstate(0x8d, 0, ps);
                ps->pm_flags = ps->pm_flags | 0x10000;
            }
        } else if ((pml->groundPlane != 0) && (pml->groundTrace.normal[2] < 0.7f)) {
            BG_AddPredictableEventToPlayerstate(0x8d, 0, ps);
        }
        if (pml->groundPlane == 0) {
            target = 0.0f;
        } else {
            target = AnglesToForward_ProjectedNormalized(ps->proneDirection, pml->groundTrace.normal);
        }
        delta = AngleDelta(target, ps->proneDirectionPitch);
        if (delta != 0.0f) {
            if (70.0f * pml->frametime < Float_Abs_080e9576(delta)) {
                result = Float_Sign(delta);
                ps->proneDirectionPitch = ps->proneDirectionPitch + pml->frametime * 70.0f * (float)result;
            } else {
                ps->proneDirectionPitch = ps->proneDirectionPitch + delta;
            }
            ps->proneDirectionPitch = AngleNormalize180_Loop(ps->proneDirectionPitch);
        }
        if (pml->groundPlane == 0) {
            target = 0.0f;
        } else {
            target = AnglesToForward_ProjectedNormalized(ps->viewangles[1], pml->groundTrace.normal);
        }
        delta = AngleDelta(target, ps->proneTorsoPitch);
        if (delta != 0.0f) {
            if (70.0f * pml->frametime < Float_Abs_080e9576(delta)) {
                result = Float_Sign(delta);
                ps->proneTorsoPitch = ps->proneTorsoPitch + pml->frametime * 70.0f * (float)result;
            } else {
                ps->proneTorsoPitch = ps->proneTorsoPitch + delta;
            }
            ps->proneTorsoPitch = AngleNormalize180_Loop(ps->proneTorsoPitch);
        }
    }
}

// 0x080e7f44  size=36  FUN_080e7f44  status=translated  naming=unknown
// Naming: descriptive; if pm_flags bit 0 is set, also sets bit 0x800.
void PM_Flags_SetBit11IfBit0(struct playerState_s *ps)
{
    if ((ps->pm_flags & 1U) != 0) {
        ps->pm_flags = ps->pm_flags | 0x800;
    }
}

// 0x080e7f68  size=169  FUN_080e7f68  status=translated  naming=unknown
// Naming: descriptive; clears pm_flags 0x100, then sets it when button 0x1000 is held, pm_type<6, not prone, flag 0x40 set and weaponState not in {5,6,7,8,9}.
void PM_UpdateAdsSlowMoveFlag(struct pmove_s *pm)
{
    struct playerState_s *ps = pm->ps;

    ps->pm_flags = ps->pm_flags & 0xfffffeff;
    if ((((((ps->pm_type < 6) && ((pm->cmd.buttons & 0x1000U) != 0)) &&
           ((ps->pm_flags & 1) == 0)) &&
          (((ps->pm_flags & 0x40U) != 0 && (ps->weaponstate != 5)))) &&
         ((ps->weaponstate != 7 && ((ps->weaponstate != 9 && (ps->weaponstate != 8)))))) &&
        (ps->weaponstate != 6)) {
        ps->pm_flags = ps->pm_flags | 0x100;
    }
}

// 0x080e8012  size=20  FUN_080e8012  status=translated  naming=unknown
// Naming: descriptive; sets pm_flags 0x20 (ladder-attached flag, see fn 0x080e805a).
void PM_Flags_SetLadder(struct playerState_s *ps)
{
    ps->pm_flags = ps->pm_flags | 0x20;
}

// 0x080e8026  size=51  FUN_080e8026  status=translated  naming=unknown
// Naming: descriptive; clears pm_flags 0x20, setting 0x40000 first if 0x20 was set.
void PM_Flags_EndLadder(struct playerState_s *ps)
{
    if ((ps->pm_flags & 0x20U) != 0) {
        ps->pm_flags = ps->pm_flags | 0x40000;
    }
    ps->pm_flags = ps->pm_flags & 0xffffffdf;
}

// 0x080e805a  size=1059  FUN_080e805a  status=translated  naming=inferred
// Naming: Q3 PM_CheckLadder counterpart: traces forward (or opposite the stored ladder normal) with a shrunken box; on a surface with flag 8 sets/keeps the ladder flag 0x20 and stores the hit normal in ps->vLadderVec.
void PM_CheckLadder(struct pmove_s *pm, struct pml_s *pml)
{
    struct playerState_s *ps = pm->ps;
    bool onLadder;
    int stance;
    float checkDist;
    float dir[3];
    float traceMins[3];
    float traceMaxs[3];
    float traceEnd[3];
    struct trace_s tr;

    if (pml->walking != 0) {
        ps->pm_flags = ps->pm_flags & 0xfffbffff;
    }
    if (((ps->pm_time == 0) || ((ps->pm_flags & 0x20) != 0)) || ((ps->pm_flags & 0x600) == 0)) {
        if (pml->walking == 0) {
            checkDist = 30.0f;
        } else {
            checkDist = 8.0f;
        }
        onLadder = false;
        if (((ps->pm_flags & 0x20) != 0) && (ps->groundEntityNum == 0x3ff)) {
            onLadder = true;
        }
        if (onLadder) {
            VectorNegate(ps->vLadderVec, dir);
        } else {
            dir[0] = pml->forward[0];
            dir[1] = pml->forward[1];
            dir[2] = 0;
            Vec3_Normalize(dir);
        }
        if (ps->pm_type < 6) {
            if ((ps->pm_flags & 0x40000) == 0) {
                stance = PM_StanceIndex(ps);
                if (stance == 1) {
                    PM_Flags_EndLadder(ps);
                } else if (pm->cmd.serverTime - ps->jumpTime < 300) {
                    PM_Flags_EndLadder(ps);
                } else {
                    Vec3_Copy_Dup(pm->mins, traceMins);
                    traceMins[0] = traceMins[0] + 6.0f;
                    traceMins[1] = traceMins[1] + 6.0f;
                    traceMins[2] = 8.0f;
                    Vec3_Copy_Dup(pm->maxs, traceMaxs);
                    traceMaxs[0] = traceMaxs[0] - 6.0f;
                    traceMaxs[1] = traceMaxs[1] - 6.0f;
                    if (traceMaxs[2] < traceMins[2]) {
                        traceMaxs[2] = traceMins[2];
                    }
                    VectorMA_080e992c(ps->origin, checkDist, dir, traceEnd);
                    PM_Trace(pm, &tr, ps->origin, traceMins, traceMaxs, traceEnd, ps->clientNum,
                                pm->tracemask);
                    if (((tr.fraction < 1.0f) && ((tr.surfaceFlags & 8) != 0)) &&
                        ((pml->walking == 0) || ('\0' < (char)pm->cmd.forwardmove))) {
                        if ((ps->pm_flags & 0x20) != 0) {
                            PM_Flags_SetLadder(ps);
                            return;
                        }
                        Vec3_Copy_Dup(tr.normal, ps->vLadderVec);
                        VectorNegate(ps->vLadderVec, dir);
                        VectorMA_080e992c(ps->origin, checkDist, dir, traceEnd);
                        PM_Trace(pm, &tr, ps->origin, traceMins, traceMaxs, traceEnd,
                                    ps->clientNum, pm->tracemask);
                        if ((tr.fraction < 1.0f) && ((tr.surfaceFlags & 8) != 0)) {
                            PM_Flags_SetLadder(ps);
                            return;
                        }
                    }
                    PM_Flags_EndLadder(ps);
                    if (onLadder) {
                        BG_AnimScriptEvent(ps, 3, 0, 1);
                    }
                }
            } else {
                PM_Flags_EndLadder(ps);
            }
        } else {
            ps->groundEntityNum = 0x3ff;
            pml->groundPlane = 0;
            pml->almostGroundPlane = 0;
            pml->walking = 0;
            PM_Flags_EndLadder(ps);
        }
    }
}

// 0x080e847e  size=1205  FUN_080e847e  status=translated  naming=inferred
// Naming: ladder movement (Q3 has no counterpart; called from PmoveSingle when pm_flags 0x20 is set): forward/back cmd climbs along the ladder normal, strafe moves along the plane, then PM_Accelerate accelerates; falls back to PM_AirMove when Player_TryJump says so.
void PM_LadderMove(struct pmove_s *pm, struct pml_s *pml)
{
    struct playerState_s *ps = pm->ps;
    char canWalk;
    float climbScale;
    float cmdScale;
    float wishdir[3];
    float wishvel[3];
    float rightPlane[3];
    float rightDir2[2];
    float wishspeed;
    float rightDot;
    float decel;
    float yaw;
    int moveDelta;
    int moveDeltaAbs;
    float dotv;

    canWalk = Player_TryJump(pm, pml);
    if (canWalk != '\0') {
        PM_AirMove(pm, pml);
        return;
    }

    climbScale = (pml->forward[2] + 0.25f) * 2.5f;
    if (1.0f < climbScale) {
        climbScale = 1.0f;
    } else if (climbScale < -1.0f) {
        climbScale = -1.0f;
    }
    pml->forward[2] = 0;
    Vec3_Normalize(pml->forward);
    pml->right[2] = 0;
    Vec3_NormalizeCopy(pml->right, rightPlane);
    Vec3_ProjectOnPlane(rightPlane, ps->vLadderVec, pml->right);
    cmdScale = PM_CmdScale2D(ps, &pm->cmd);
    Vec3_Clear_Dup(wishvel);
    if (pm->cmd.forwardmove != 0) {
        wishvel[2] = (float)(short)pm->cmd.forwardmove * climbScale * 0.5f * cmdScale;
    }
    if (pm->cmd.rightmove != 0) {
        VectorMA_080e992c(wishvel, (float)(short)pm->cmd.rightmove * cmdScale * 0.2f, pml->right,
                    wishvel);
    }
    wishspeed = Vec3_NormalizeCopy(wishvel, wishdir);
    PM_Accelerate(ps, pml, wishdir, wishspeed, 9.0f);

    if (pm->cmd.forwardmove == 0) {
        if (0.0f < ps->velocity[2]) {
            ps->velocity[2] = ps->velocity[2] - (float)ps->gravity * pml->frametime;
            if (ps->velocity[2] < 0.0f) {
                ps->velocity[2] = 0;
            }
        } else {
            ps->velocity[2] = ps->velocity[2] + (float)ps->gravity * pml->frametime;
            if (0.0f < ps->velocity[2]) {
                ps->velocity[2] = 0;
            }
        }
    }
    if (pm->cmd.rightmove == 0) {
        Vec2_Copy(pml->right, rightDir2);
        Vec2_Normalize(rightDir2);
        rightDot = Vec2_Dot_Dup(rightDir2, ps->velocity);
        if (rightDot != 0.0f) {
            Vec2_MA(ps->velocity, -rightDot, rightDir2, ps->velocity);
            decel = rightDot * pml->frametime * 16.0f;
            if (Float_Abs_080e9576(decel) < Float_Abs_080e9576(rightDot)) {
                if (Float_Abs_080e9576(decel) < 1.0f) {
                    int s = Float_Sign(decel);
                    decel = (float)s;
                }
                Vec2_MA(ps->velocity, rightDot - decel, rightDir2, ps->velocity);
            }
        }
    }
    if (pml->walking == 0) {
        dotv = Vec2_Dot_Dup(ps->vLadderVec, ps->velocity);
        Vec2_MA(ps->velocity, -dotv, ps->vLadderVec, ps->velocity);
        if (Vec2_LengthSq(ps->velocity) <= Float_Square(ps->velocity[2])) {
            Vec2_MA(ps->velocity, -50.0f, ps->vLadderVec, ps->velocity);
        }
    }
    PM_StepSlideMove(pm, pml, 0);
    yaw = 180.0f + Vec2_AngleDegrees(ps->vLadderVec);
    moveDelta = (int)AngleDelta(yaw, ps->viewangles[1]);
    moveDeltaAbs = moveDelta;
    if (moveDelta < 0) {
        moveDeltaAbs = -moveDelta;
    }
    if (0x4b < moveDeltaAbs) {
        if (moveDelta < 1) {
            moveDelta = -0x4b;
        } else {
            moveDelta = 0x4b;
        }
    }
    ps->movementDir = (int)(char)moveDelta;
}

// 0x080e8934  size=2863  FUN_080e8934  status=translated  naming=inferred
// Naming: Q3 PmoveSingle counterpart: builds pml, runs duck/ladder/walk/air/fly movement per pm_type, footsteps, drop timers; called from PM_Pmove in <=66 ms steps.
void PM_SingleStep(struct pmove_s *pm)
{
    struct playerState_s *ps;
    struct pml_s pml;
    float velDelta[3];
    float velBlend[3];
    float blendFrac;
    int stance;
    float frametime;
    int msec;
    float tmpf;
    float lenSqDelta;

    ps = pm->ps;
    BG_UpdateConditionsFromPmove(pm);
    if ((ps->pm_flags & 0x8000) != 0) {
        pm->cmd.buttons = pm->cmd.buttons & 0x2300;
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        Vec3_Set_Dup(ps->velocity, 0, 0, 0);
    }
    if ((pm->cmd.buttons & 0x40000) != 0) {
        pm->cmd.buttons = pm->cmd.buttons & 0x43300;
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
    }
    ps->pm_flags = ps->pm_flags & 0xfffeffff;
    if (5 < ps->pm_type) {
        pm->tracemask = pm->tracemask & 0xfdffffff;
    }
    if ((ps->pm_flags & 1U) == 0) {
        ps->pm_flags = ps->pm_flags & 0xfffff7ff;
    } else if ((pm->cmd.forwardmove == pm->oldcmd.forwardmove) ||
               (Float_Abs_080e9576((float)(short)pm->cmd.forwardmove) <=
                Float_Abs_080e9576((float)(short)pm->oldcmd.forwardmove))) {
        if ((pm->cmd.rightmove != pm->oldcmd.rightmove) &&
            (Float_Abs_080e9576((float)(short)pm->oldcmd.rightmove) <
             Float_Abs_080e9576((float)(short)pm->cmd.rightmove))) {
            goto prone_move_start;
        }
        if (((ps->pm_flags & 0x40U) == 0) &&
            ((((ps->weaponstate == 0) || (ps->weaponstate == 1)) || (ps->weaponstate == 2)) ||
             (ps->weaponstate == 5))) {
            ps->pm_flags = ps->pm_flags & 0xfffff7ff;
        }
    } else {
prone_move_start:
        if (PM_WeaponStateHandler(ps) != 0) {
            ps->pm_flags = ps->pm_flags & 0xfffff7ff;
            PM_ClearAdsFlag(ps);
        }
    }
    stance = PM_StanceIndex(ps);
    if (((ps->pm_flags & 0x40U) != 0) && (stance == 1)) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
    }
    if ((pm->cmd.buttons & 0x40000) == 0) {
        ps->eFlags = ps->eFlags & 0xffdfffff;
    } else {
        ps->eFlags = ps->eFlags | 0x200000;
    }
    ps->eFlags = ps->eFlags & 0xffffffbf;
    if ((((ps->pm_type != 5) && ((ps->pm_flags & 0x1000U) == 0)) &&
         ((ps->weaponstate == 0) || (ps->weaponstate == 3))) &&
        ((PM_Weapon_GetClipAmmo(ps) != 0) && ((pm->cmd.buttons & 1) != 0))) {
        ps->eFlags = ps->eFlags | 0x40;
    }
    if ((ps->pm_type < 6) && ((pm->cmd.buttons & 0x4001) == 0)) {
        ps->pm_flags = ps->pm_flags & 0xffffefff;
    }
    memset(&pml, 0, 0x7c);
    msec = pm->cmd.serverTime - ps->commandTime;
    if (msec < 1) {
        msec = 1;
    } else if (200 < msec) {
        msec = 200;
    }
    pml.msec = msec;
    ps->commandTime = pm->cmd.serverTime;
    Vec3_Copy_Dup(ps->origin, pml.previous_origin);
    Vec3_Copy_Dup(ps->velocity, pml.previous_velocity);
    frametime = (float)msec * 0.001f;
    pml.frametime = frametime;
    PM_Weapon_UpdateAimSpread(pm, &pml);
    PM_UpdateViewAngles(ps, (float)msec, &pm->cmd, pm->handler);
    Angles_ToVectors(ps->viewangles, pml.forward, pml.right, pml.up);
    if (pm->cmd.forwardmove < 0) {
        ps->pm_flags = ps->pm_flags | 0x80;
    } else if ((0 < pm->cmd.forwardmove) ||
               ((pm->cmd.forwardmove == 0) && (pm->cmd.rightmove != 0))) {
        ps->pm_flags = ps->pm_flags & 0xffffff7f;
    }
    if (5 < ps->pm_type) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
    }
    if ((stance == 1) && ((ps->pm_flags & 0x800U) != 0)) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
    }
    Mantle_ClearSurfaceFlag(ps);

    switch (ps->pm_type) {
    default:
        if ((ps->eFlags & 0x300U) == 0) {
            if ((ps->pm_flags & 4U) == 0) {
                PM_CheckDuck(pm, &pml);
                PM_GroundTrace(pm, &pml);
            }
            Mantle_TryStart(pm, &pml);
            if ((ps->pm_flags & 4U) == 0) {
                PM_UpdateAdsFlag(pm, &pml);
                PM_UpdateAdsSlowMoveFlag(pm);
                PM_SmoothBodyAngles(pm, &pml);
                if (ps->pm_type == 6) {
                    PM_ReduceGroundSpeed(ps, &pml);
                }
                PM_CheckLadder(pm, &pml);
                PM_DropTimers(ps, &pml);
                if ((ps->pm_flags & 0x20U) == 0) {
                    if (pml.walking == 0) {
                        PM_AirMove(pm, &pml);
                    } else {
                        PM_WalkMove(pm, &pml);
                    }
                } else {
                    PM_LadderMove(pm, &pml);
                }
                PM_GroundTrace(pm, &pml);
                PM_Footsteps(pm, &pml);
                PM_Weapon_Frame(pm, &pml);
                PM_CheckMoveBump(pm);
                VectorSubtract_080e98b0(ps->origin, pml.previous_origin, velDelta);
                lenSqDelta = VectorLengthSquared_080e99b4(velDelta);
                if ((float)(lenSqDelta / (frametime * frametime)) <
                    VectorLengthSquared_080e99b4(ps->velocity) * 0.25f) {
                    VectorScale_080e98f4(velDelta, 1.0f / frametime, ps->velocity);
                }
                Vec2_Sub_Dup(ps->velocity, ps->oldVelocity, velBlend);
                blendFrac = Float_Min_080e95d4(frametime, 1.0f);
                Vec2_Scale(velBlend, blendFrac, velBlend);
                Vec2_Add_080e968c(ps->oldVelocity, velBlend, ps->oldVelocity);
                Sys_SnapVector(ps->velocity);
            } else {
                PM_Flags_EndLadder(ps);
                ps->groundEntityNum = 0x3ff;
                pml.groundPlane = 0;
                pml.walking = 0;
                PM_UpdateAdsFlag(pm, &pml);
                PM_UpdateAdsSlowMoveFlag(pm);
                PM_CheckDuck(pm, &pml);
                Mantle_UpdateMove(pm, ps, &pml);
                PM_Weapon_Frame(pm, &pml);
            }
        } else {
            PM_Flags_EndLadder(ps);
            ps->groundEntityNum = 0x3ff;
            pml.groundPlane = 0;
            pml.almostGroundPlane = 0;
            pml.walking = 0;
            Vec3_Clear_Dup(ps->velocity);
            PM_UpdateAdsFlag(pm, &pml);
            PM_UpdateAdsSlowMoveFlag(pm);
            PM_CheckDuck(pm, &pml);
            PM_DropTimers(ps, &pml);
            PM_Footsteps(pm, &pml);
            PM_Weapon_ForceReady_Entry(ps);
        }
        break;
    case 1:
    case 7:
        PM_Flags_EndLadder(ps);
        ps->groundEntityNum = 0x3ff;
        pml.groundPlane = 0;
        pml.almostGroundPlane = 0;
        pml.walking = 0;
        Vec3_Clear_Dup(ps->velocity);
        PM_UpdateAdsFlag(pm, &pml);
        PM_UpdateAdsSlowMoveFlag(pm);
        PM_CheckDuck(pm, &pml);
        PM_DropTimers(ps, &pml);
        PM_Footsteps(pm, &pml);
        PM_Weapon_Frame(pm, &pml);
        break;
    case 2:
        PM_Flags_EndLadder(ps);
        PM_UpdateAdsFlag(pm, &pml);
        PM_UpdateAdsSlowMoveFlag(pm);
        PM_FlyMove(pm, &pml);
        PM_DropTimers(ps, &pml);
        PM_UpdateWeaponPosFrac(pm, &pml);
        break;
    case 3:
        PM_Flags_EndLadder(ps);
        PM_UpdateAdsFlag(pm, &pml);
        PM_UpdateAdsSlowMoveFlag(pm);
        PM_ClimbMove(pm, &pml);
        PM_DropTimers(ps, &pml);
        PM_UpdateWeaponPosFrac(pm, &pml);
        break;
    case 4:
        PM_Flags_EndLadder(ps);
        PM_UpdateAdsFlag(pm, &pml);
        PM_UpdateAdsSlowMoveFlag(pm);
        PM_CheckDuck(pm, &pml);
        PM_FreeMove(pm, &pml);
        PM_DropTimers(ps, &pml);
        PM_UpdateWeaponPosFrac(pm, &pml);
        break;
    case 5:
        PM_Flags_EndLadder(ps);
        PM_UpdateAdsFlag(pm, &pml);
        PM_UpdateWeaponPosFrac(pm, &pml);
        break;
    }
}

// 0x080e9464  size=195  FUN_080e9464  status=translated  naming=inferred
// Naming: Q3 Pmove counterpart: advances ps->commandTime to cmd.serverTime in steps of at most 66 ms (capped to 1000 ms behind), running PM_SingleStep and copying cmd to oldcmd after each step.
void PM_Pmove(struct pmove_s *pm)
{
    struct playerState_s *ps = pm->ps;
    int finalTime = pm->cmd.serverTime;
    int step;

    if (ps->commandTime <= finalTime) {
        if (ps->commandTime + 1000 < finalTime) {
            ps->commandTime = finalTime - 1000;
        }
        pm->numtouch = 0;
        while (ps->commandTime != finalTime) {
            step = finalTime - ps->commandTime;
            if (0x42 < step) {
                step = 0x42;
            }
            pm->cmd.serverTime = step + ps->commandTime;
            PM_SingleStep(pm);
            pm->oldcmd = pm->cmd;
        }
    }
}

// 0x080e9528  size=77  FUN_080e9528  status=translated  naming=unknown
// Naming: descriptive; returns the xy speed (Vec2 length of velocity) normally, or while on a ladder (0x20) 0 for 500 ms after ps->jumpTime and then velocity[2].
float PM_GetMoveSpeed(struct playerState_s *ps, int time)
{
    float result;

    if ((ps->pm_flags & 0x20) == 0) {
        result = Vec2_Length_Dup(ps->velocity);
    } else if (time - ps->jumpTime < 500) {
        result = 0.0f;
    } else {
        result = ps->velocity[2];
    }
    return result;
}

// 0x080e9576  size=25  FUN_080e9576  status=translated  naming=inferred
// Naming: float absolute value (x87 fabs).
float Float_Abs_080e9576(float x)
{
    return fabsf(x);
}

// 0x080e9590  size=31  FUN_080e9590  status=translated  naming=inferred
// Naming: float square root via double sqrt() then narrowing.
float Float_Sqrt_080e9590(float x)
{
    double r = sqrt((double)x);
    return (float)r;
}

// 0x080e95b0  size=36  FUN_080e95b0  status=translated  naming=inferred
// Naming: calls Q_fsel043(x,a,b) = (x>=0 ? a : b) with x=a-b, i.e. max(a,b).
float Float_Max_080e95b0(float a, float b)
{
    return Q_fsel043(a - b, a, b);
}

// 0x080e95d4  size=36  FUN_080e95d4  status=translated  naming=inferred
// Naming: calls Q_fsel043(x,a,b) with x=b-a, i.e. min(a,b).
float Float_Min_080e95d4(float a, float b)
{
    return Q_fsel043(b - a, a, b);
}

// 0x080e95f8  size=65  FUN_080e95f8  status=translated  naming=unknown
// Naming: descriptive; two chained integer selects via Q_isel043(x,a,b) = (x>=0 ? a : b): t = select(a-c, c, a); result = select(b-a, b, t).
int Int_SelectClamp(int a, int b, int c)
{
    int t = Q_isel043(a - c, c, a);
    return Q_isel043(b - a, b, t);
}

// 0x080e963a  size=11  FUN_080e963a  status=translated  naming=inferred
// Naming: float square.
float Float_Square(float x)
{
    return x * x;
}

// 0x080e9646  size=38  FUN_080e9646  status=translated  naming=inferred
// Naming: returns 1 - 2*signbit(x) (SignBit32 returns the IEEE sign bit), i.e. +1 or -1.
int Float_Sign(float x)
{
    int signBit = SignBit32(*(unsigned int *)&x); /* the float argument word is passed as raw bits */
    return signBit * -2 + 1;
}

// 0x080e966c  size=31  FUN_080e966c  status=translated  naming=inferred
// Naming: copies two floats (2D vector copy).
void Vec2_Copy(const float *in, float *out)
{
    ((unsigned int *)out)[0] = ((const unsigned int *)in)[0];
    ((unsigned int *)out)[1] = ((const unsigned int *)in)[1];
}

// 0x080e968c  size=44  FUN_080e968c  status=translated  naming=inferred
// Naming: 2D vector add.
void Vec2_Add_080e968c(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
}

// 0x080e96b8  size=44  FUN_080e96b8  status=translated  naming=inferred
// Naming: 2D vector subtract (duplicate of Vec2_Sub at 0x080ab010).
void Vec2_Sub_Dup(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

// 0x080e96e4  size=37  FUN_080e96e4  status=translated  naming=inferred
// Naming: 2D vector scale (scalar first-multiplied).
void Vec2_Scale(const float *in, float s, float *out)
{
    out[0] = s * in[0];
    out[1] = s * in[1];
}

// 0x080e970a  size=54  FUN_080e970a  status=translated  naming=inferred
// Naming: 2D multiply-add: out = base + s*dir.
void Vec2_MA(const float *base, float s, const float *dir, float *out)
{
    out[0] = base[0] + s * dir[0];
    out[1] = base[1] + s * dir[1];
}

// 0x080e9740  size=33  FUN_080e9740  status=translated  naming=inferred
// Naming: 2D dot product (duplicate of Vec2_Dot_080ab03c at 0x080ab03c; operand order y*y' + x*x').
float Vec2_Dot_Dup(const float *a, const float *b)
{
    return a[1] * b[1] + a[0] * b[0];
}

// 0x080e9762  size=33  FUN_080e9762  status=translated  naming=inferred
// Naming: 2D squared length.
float Vec2_LengthSq(const float *v)
{
    return v[1] * v[1] + v[0] * v[0];
}

// 0x080e9784  size=44  FUN_080e9784  status=translated  naming=inferred
// Naming: 2D length via Float_Sqrt_080e9590 of y*y + x*x (duplicate of Vec2_Length at 0x080ab05e).
float Vec2_Length_Dup(const float *v)
{
    return Float_Sqrt_080e9590(v[1] * v[1] + v[0] * v[0]);
}

// 0x080e97b0  size=41  FUN_080e97b0  status=translated  naming=inferred
// Naming: zeroes a 3-float vector (duplicate of Vec3_Clear_0809bf66 at 0x080ab08a).
void Vec3_Clear_Dup(float *v)
{
    *(unsigned int *)&v[0] = 0;
    *(unsigned int *)&v[1] = 0;
    *(unsigned int *)&v[2] = 0;
}

// 0x080e97da  size=35  FUN_080e97da  status=translated  naming=inferred
// Naming: sets a 3-float vector (duplicate of Vec3_Set at 0x080ab0b4); the original moves raw 32-bit words.
void Vec3_Set_Dup(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// 0x080e97fe  size=47  FUN_080e97fe  status=translated  naming=inferred
// Naming: copies a 3-float vector as raw words (duplicate of Vec3_Copy_0809bf90 at 0x080ab0d8).
void Vec3_Copy_Dup(const float *in, float *out)
{
    ((unsigned int *)out)[0] = ((const unsigned int *)in)[0];
    ((unsigned int *)out)[1] = ((const unsigned int *)in)[1];
    ((unsigned int *)out)[2] = ((const unsigned int *)in)[2];
}
