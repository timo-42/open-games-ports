#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_044_types.h"

/*
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 044.
 * Range 0x080ed5c0..0x080f14aa (59 functions). Binary SHA-256
 * 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * Subsystem: per-player weapon/offhand state machine (PM_Weapon_*), pmove/pml style
 * input handling, and view-sway helpers. Structure evidence: the first argument is a
 * wrapper whose [0] is the playerState and whose usercmd-shaped cmd (+4) and oldcmd
 * (+0x20) fields match the Quake III-lineage pmove_t layout; the second argument carries
 * forward/right/up, frametime (+0x24) and msec (+0x28) like pml_t. Naming of the PM_*
 * functions is therefore "inferred" from that layout; state-entry helpers are unknown.
 *
 * Quirks preserved on purpose: several functions take extra or unused arguments
 * (PM_Weapon_AmmoPerShot, PM_Weapon_OffhandDenied, PM_Weapon_BeginMeleeStrike callers);
 * WeaponView_UpdateMoveRotation passes the int-typed view offset array to float-taking helpers.
 */

/* Forward declarations of in-chunk functions (defined below, in address order). */
void PM_Weapon_SubClipAmmo(playerState_s *ps, int32 weapon, int32 count);
int32 PM_Weapon_GetClipAmmo(playerState_s *ps);
bool PM_Weapon_ClipEmpty(playerState_s *ps);
void PM_Weapon_ClearToReady(playerState_s *ps);
int32 PM_Weapon_TryStartRechamber(playerState_s *ps, int32 triggerHeld);
void PM_Weapon_ComputeReloadStep(playerState_s *ps);
void PM_Weapon_BeginReloadState(playerState_s *ps);
void PM_Weapon_TryBeginReload(playerState_s *ps);
void PM_Weapon_SelectWeapon(playerState_s *ps, int32 newWeapon);
void PM_Weapon_ChangeWeapon(pmove_s *pm);
void PM_Weapon_SetReadyState(playerState_s *ps);
int32 PM_Weapon_NeedsAutoReload(playerState_s *ps);
void PM_Weapon_ApplyReloadStep(playerState_s *ps);
void PM_Weapon_FinishFireCycle(playerState_s *ps, int32 fireHeld);
void PM_Weapon_FinishReloadCycle(playerState_s *ps, int32 fireHeld);
void PM_Weapon_SetReadyState_Alt(playerState_s *ps);
void PM_Weapon_UpdateInputState(pmove_s *pm);
void PM_Weapon_TakeEmptyClipOnlyWeapon(playerState_s *ps);
void PM_Weapon_UpdateAimSpread(pmove_s *pm, pml_s *pml);
int32 PM_Weapon_TickTimers(pmove_s *pm, pml_s *pml);
void PM_Weapon_ChangeSlot(pmove_s *pm);
int32 PM_Weapon_ReleaseFireHold(pmove_s *pm, int32 fireHeld);
void PM_Weapon_BeginFire(playerState_s *ps, int32 altFire);
int32 PM_Weapon_AmmoPerShot(int32 weapon);
int32 PM_Weapon_CheckClipOrReload(playerState_s *ps);
void PM_Weapon_SetFireAnimState(playerState_s *ps);
void PM_Weapon_AddFireAimSpread(playerState_s *ps);
void PM_Weapon_FireShot(playerState_s *ps, int32 altFire);
void PM_Weapon_BeginMeleeStrike(playerState_s *ps);
void PM_Weapon_ClearToReady_Alt(playerState_s *ps);
void PM_Weapon_TryStartMelee(pmove_s *pm, int32 fireHeld);
void PM_Weapon_ForceReady(playerState_s *ps);
void PM_Weapon_BeginOffhandLower(playerState_s *ps);
void PM_Weapon_EnterOffhandState13(playerState_s *ps);
void PM_Weapon_EnterOffhandState14(playerState_s *ps);
void PM_Weapon_ThrowOffhand(pmove_s *pm);
void PM_Weapon_BeginOffhandRaise(playerState_s *ps);
void PM_Weapon_OffhandDenied(playerState_s *ps, int32 hand);
void PM_Weapon_HandleOffhandButtons(pmove_s *pm);
int32 PM_Weapon_TickOffhandTimer(playerState_s *ps, pml_s *pml);
void PM_Weapon_SetBinocularsActive(playerState_s *ps);
void PM_Weapon_ClearBinocularsActive(playerState_s *ps);
void PM_Weapon_ResetAdsState(playerState_s *ps);
void PM_Weapon_BinocularsEnterBegin(playerState_s *ps);
void PM_Weapon_BinocularsLowerWeapon(playerState_s *ps);
void PM_Weapon_BinocularsRaise(playerState_s *ps);
void PM_Weapon_BinocularsHold(playerState_s *ps);
void PM_Weapon_BinocularsExitBegin(playerState_s *ps);
void PM_Weapon_BinocularsRestoreWeapon(pmove_s *pm);
void PM_Weapon_HandleButtonEdges(pmove_s *pm);
void PM_Weapon_Frame(pmove_s *pm, pml_s *pml);
void PM_Weapon_ForceReady_Entry(playerState_s *ps);
void BG_WeaponRandomFireKick(playerState_s *ps, float *angles, float *out);
long double BG_BobCyclePhase(const playerState_s *ps);
long double BG_BobSinePair(playerState_s *ps, float phase, float amp, float limit);
long double BG_BobSine(playerState_s *ps, float phase, float amp, float limit);
void WeaponView_UpdateMoveRotation(view_motion_s *view, float *out);
void WeaponView_ComputeAdsAimOffset(view_motion_s *view, float *out);
void WeaponView_ComputeIdleSway(view_motion_s *view, float *out);

// 0x080ed5c0  size=60  FUN_080ed5c0  status=translated  naming=inferred
// naming: PM_Weapon_* from pmove_s/pml_s layout (see file header); body is clip-ammo subtraction for the weapon's clip slot.
void PM_Weapon_SubClipAmmo(playerState_s *ps, int32 weapon, int32 count)
{
    int32 slot = BG_GetWeaponClipIndex(weapon);
    ps->ammoclip[slot] = ps->ammoclip[slot] - count;
}

// 0x080ed5fc  size=41  FUN_080ed5fc  status=translated  naming=inferred
// naming: PM_Weapon_* (pmove layout); returns the magazine count of the current weapon's clip slot.
int32 PM_Weapon_GetClipAmmo(playerState_s *ps)
{
    int32 slot = BG_GetWeaponClipIndex(ps->weapon);
    return ps->ammoclip[slot];
}

// 0x080ed626  size=62  FUN_080ed626  status=translated  naming=inferred
// naming: PM_Weapon_* (pmove layout); true when the current weapon's magazine is empty.
bool PM_Weapon_ClipEmpty(playerState_s *ps)
{
    int32 slot = BG_GetWeaponClipIndex(ps->weapon);
    return ps->ammoclip[slot] == 0;
}

// 0x080ed664  size=40  FUN_080ed664  status=translated  naming=unknown
// naming: descriptive; clears the state timer through PlayerState_SetWeapAnimIfChanged and returns the state to READY.
void PM_Weapon_ClearToReady(playerState_s *ps)
{
    PlayerState_SetWeapAnimIfChanged(ps, 0);
    ps->weaponstate = WEAPSTATE_READY;
}

// 0x080ed68c  size=552  FUN_080ed68c  status=translated  naming=unknown
// naming: descriptive; attempts to start a firing/load cycle when the trigger or a pending bit allows it. Returns 1 when the caller must stop.
int32 PM_Weapon_TryStartRechamber(playerState_s *ps, int32 triggerHeld)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    bool bitSet;

    if ((ps->weaponstate < 0x11 || 0x16 < ps->weaponstate) &&
        (ps->weaponstate < 0xc || 0x10 < ps->weaponstate) &&
        wd->boltAction != 0 &&
        (bitSet = BitSet_Test(ps->weaponrechamber, ps->weapon), bitSet)) {
        if (ps->weaponstate == WEAPSTATE_4 && triggerHeld != 0) {
            BitArray_ClearBit(ps->weaponrechamber, ps->weapon);
            PM_AddEvent(ps, 0xa2);
            if (ps->weaponTime != 0) {
                return 1;
            }
        }
        if (ps->weaponTime == 0 ||
            (ps->weaponstate != 3 && ps->weaponstate != 4 && ps->weaponstate != 10 &&
             ps->weaponstate != 0xb && ps->weaponDelay == 0)) {
            if (ps->weaponstate == WEAPSTATE_4) {
                PM_Weapon_ClearToReady(ps);
            } else if (ps->weaponstate == WEAPSTATE_READY) {
                if (0.75 < ps->fWeaponPosFrac) {
                    PlayerState_SetWeapAnim(ps, 7);
                } else {
                    PlayerState_SetWeapAnim(ps, 4);
                }
                ps->weaponstate = WEAPSTATE_4;
                ps->weaponTime = wd->rechamberTime;
                if (wd->rechamberBoltTime == 0 || wd->rechamberTime <= wd->rechamberBoltTime) {
                    ps->weaponDelay = 1;
                } else {
                    ps->weaponDelay = wd->rechamberBoltTime;
                }
                PM_AddEvent(ps, 0xa1);
            }
        }
    }
    return 0;
}

// 0x080ed8b4  size=360  FUN_080ed8b4  status=translated  naming=unknown
// naming: descriptive; computes the per-step count for the current load/reload phase and stores it in weaponDelay.
void PM_Weapon_ComputeReloadStep(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 count;
    int32 slot;
    bool pending;

    if (ps->weaponstate == WEAPSTATE_7 || ps->weaponstate == WEAPSTATE_8) {
        if (wd->reloadStartAddTime == 0) {
            count = 0;
        } else if (wd->reloadStartAddTime < wd->reloadStartTime) {
            count = wd->reloadStartAddTime;
        } else {
            count = wd->reloadStartTime;
        }
    } else {
        slot = BG_GetWeaponClipIndex(ps->weapon);
        if (ps->ammoclip[slot] == 0 && wd->weaponType == 0) {
            count = wd->reloadEmptyTime;
        } else {
            count = wd->reloadTime;
        }
        if (wd->reloadAddTime != 0 && wd->reloadAddTime < count) {
            count = wd->reloadAddTime;
        }
    }

    pending = (wd->boltAction != 0) && BitSet_Test(ps->weaponrechamber, ps->weapon);
    if (!pending) {
        if (count != 0) {
            ps->weaponDelay = count;
        }
        return;
    }
    if (count == 0) {
        count = ps->weaponTime;
    }
    if (wd->rechamberBoltTime < count) {
        count = wd->rechamberBoltTime;
    }
    if (count == 0) {
        count = 1;
    }
    ps->weaponDelay = count;
}

// 0x080eda1c  size=232  FUN_080eda1c  status=translated  naming=unknown
// naming: descriptive; enters the load/reload phase (state 5 or 6), selecting the timer by magazine state, then recomputes the step.
void PM_Weapon_BeginReloadState(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 slot = BG_GetWeaponClipIndex(ps->weapon);

    if (ps->ammoclip[slot] == 0 && wd->weaponType == 0) {
        PlayerState_SetWeapAnim(ps, 0xc);
        ps->weaponTime = wd->reloadEmptyTime;
        PM_AddEvent(ps, 0x97);
    } else {
        PlayerState_SetWeapAnim(ps, 0xb);
        ps->weaponTime = wd->reloadTime;
        PM_AddEvent(ps, 0x96);
    }
    if (ps->weaponstate == WEAPSTATE_8) {
        ps->weaponstate = WEAPSTATE_6;
    } else {
        ps->weaponstate = WEAPSTATE_5;
    }
    PM_Weapon_ComputeReloadStep(ps);
}

// 0x080edb04  size=299  FUN_080edb04  status=translated  naming=unknown
// naming: descriptive; starts a reload from ready/firing/recharge states when ammo allows; otherwise uses the BeginReloadState path.
void PM_Weapon_TryBeginReload(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);

    if ((ps->weaponstate == WEAPSTATE_READY || ps->weaponstate == WEAPSTATE_FIRING ||
         ps->weaponstate == WEAPSTATE_4) &&
        ps->weapon != 0 && ps->weapon <= BG_GetNumWeapons()) {
        if (BG_GetWeaponClipOnly(ps->weapon) == 0) {
            BG_AnimScriptEvent(ps, 10, 0, 1);
        }
        PM_AddEvent(ps, 0x95);
        if (wd->segmentedReload == 0 || wd->reloadStartTime == 0) {
            PM_Weapon_BeginReloadState(ps);
        } else {
            PlayerState_SetWeapAnim(ps, 0xd);
            ps->weaponTime = wd->reloadStartTime;
            ps->weaponstate = WEAPSTATE_7;
            PM_AddEvent(ps, 0x98);
            PM_Weapon_ComputeReloadStep(ps);
        }
    }
}

// 0x080edc30  size=572  FUN_080edc30  status=translated  naming=unknown
// naming: descriptive; weapon-switch state entry. Newly selected weapon is checked against the weaponsHeld bitset; state 2 is the lowering phase.
void PM_Weapon_SelectWeapon(playerState_s *ps, int32 newWeapon)
{
    bool same36c = false;
    bool hasAmmo = true;
    weaponDef_s *oldDef;
    int32 oldWeapon;
    int32 slot;

    if ((newWeapon == 0 || BitSet_Test(ps->weapons, newWeapon)) &&
        ps->weaponstate != WEAPSTATE_2) {
        ps->weaponDelay = 0;
        oldWeapon = ps->weapon;
        if (oldWeapon == 0 || !BitSet_Test(ps->weapons, oldWeapon) ||
            0 < ps->grenadeTimeLeft) {
            ps->weaponTime = 0;
            ps->weaponstate = WEAPSTATE_2;
            ps->grenadeTimeLeft = 0;
            PM_Flags_SetBit11IfBit0(ps);
        } else {
            oldDef = BG_GetWeaponDef(oldWeapon);
            if (newWeapon != 0 && newWeapon == oldDef->altWeaponIndex) {
                same36c = true;
            }
            slot = BG_GetWeaponClipOnly(oldWeapon);
            if (slot != 0) {
                slot = BG_GetWeaponClipIndex(oldWeapon);
                if (ps->ammoclip[slot] == 0) {
                    hasAmmo = false;
                }
            }
            ps->grenadeTimeLeft = 0;
            if (same36c) {
                PM_AddEvent(ps, 0x9c);
                PlayerState_SetWeapAnim(ps, 0xf);
            } else if (hasAmmo) {
                PM_AddEvent(ps, 0x9b);
                PlayerState_SetWeapAnim(ps, 9);
            }
            if (!same36c && (ps->pm_flags & 4) == 0) {
                BG_AnimScriptEvent(ps, 6, 0, 0);
            }
            ps->weaponstate = WEAPSTATE_2;
            PM_Flags_SetBit11IfBit0(ps);
            if (same36c) {
                ps->weaponTime = oldDef->altDropTime;
                oldWeapon = BG_FindWeaponSlot(ps, oldWeapon, 1);
                if (oldWeapon != 0) {
                    BG_SetHeldSlot(ps, oldWeapon, newWeapon);
                }
            } else {
                ps->weaponTime = oldDef->dropTime;
            }
        }
    }
}

// 0x080ede6c  size=830  FUN_080ede6c  status=translated  naming=inferred
// naming: PM_Weapon_* from pmove layout; applies a weapon-slot change from cmd.weapon, with the ammo, lock and selection gates of this function.
void PM_Weapon_ChangeWeapon(pmove_s *pm)
{
    playerState_s *ps = pm->ps;
    uint32 candidate;
    uint32 oldWeapon;
    weaponDef_s *def;
    bool same36c = false;
    bool pendingFlag;

    BG_GetWeaponDef(ps->weapon);
    if (!Mantle_OverAnimNotLow(ps)) {
        if ((ps->pm_flags & 0x20) == 0 &&
            BitSet_Test(ps->weapons, (int32)(int8)pm->cmd.weapon)) {
            if ((ps->pm_flags & 0x04000000) == 0 &&
                BitSet_Test(ps->weapons, (int32)(int8)pm->cmd.weapon)) {
                candidate = (uint32)pm->cmd.weapon;
                if ((int32)BG_GetNumWeapons() < (int32)candidate) {
                    candidate = 0;
                }
            } else {
                candidate = 0;
            }
        } else {
            candidate = 0;
        }
    } else {
        candidate = 0;
    }

    pendingFlag = BitSet_Test(ps->weapons, (int32)candidate);
    if (!pendingFlag) {
        candidate = 0;
    }

    oldWeapon = (uint32)ps->weapon;
    ps->weapon = (int32)candidate;
    BG_GetWeaponDef(ps->weapon);

    if (oldWeapon == candidate) {
        ps->weaponstate = WEAPSTATE_READY;
        PlayerState_SetWeapAnim(ps, 0);
    } else if (oldWeapon == 0) {
        ps->weaponstate = WEAPSTATE_1;
        def = BG_GetWeaponDef((int32)candidate);
        ps->weaponTime = def->raiseTime;
        ps->aimSpreadScale = 255.0f;
        PlayerState_SetWeapAnim(ps, 10);
        PM_Flags_SetBit11IfBit0(ps);
    } else {
        ps->weaponstate = WEAPSTATE_1;
        PM_Flags_SetBit11IfBit0(ps);
        if (candidate != 0 && candidate == (uint32)BG_GetWeaponDef((int32)oldWeapon)->altWeaponIndex) {
            same36c = true;
        }
        if (same36c) {
            def = BG_GetWeaponDef((int32)candidate);
            ps->weaponTime = def->altRaiseTime;
        } else {
            PM_AddEvent(ps, 0x9a);
            def = BG_GetWeaponDef((int32)candidate);
            ps->weaponTime = def->raiseTime;
        }
        def = BG_GetWeaponDef((int32)candidate);
        BG_UpdateConditionValue(ps->clientNum, 0, def->playerAnimType, 1);
        BG_UpdateConditionValue(ps->clientNum, 1, def->weaponClass, 1);
        if (candidate == 0 || candidate != (uint32)BG_GetWeaponDef((int32)oldWeapon)->altWeaponIndex) {
            BG_AnimScriptEvent(ps, 7, 0, 0);
            ps->aimSpreadScale = 255.0f;
            PlayerState_SetWeapAnim(ps, 0x0a);
        } else {
            if (ps->aimSpreadScale < 128.0) {
                ps->aimSpreadScale = 128.0f;
            }
            PlayerState_SetWeapAnim(ps, 0x10);
        }
    }
}

// 0x080ee1aa  size=40  FUN_080ee1aa  status=translated  naming=unknown
// naming: descriptive; returns the weapon state to READY and sends the idle animation.
void PM_Weapon_SetReadyState(playerState_s *ps)
{
    ps->weaponstate = WEAPSTATE_READY;
    PlayerState_SetWeapAnim(ps, 0);
}

// 0x080ee1d2  size=265  FUN_080ee1d2  status=translated  naming=unknown
// naming: descriptive; reports whether the current weapon has reserve ammo and a magazine deficit that warrants an automatic reload.
int32 PM_Weapon_NeedsAutoReload(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 clipSlot = BG_GetWeaponClipIndex(ps->weapon);
    int32 reserveSlot = BG_GetWeaponAmmoIndex(ps->weapon);
    int32 limit;

    if (ps->ammo[reserveSlot] != 0 &&
        (limit = BG_GetClipTypeSize(clipSlot), ps->ammoclip[clipSlot] < limit)) {
        if (wd->noPartialReload == 0) {
            return 1;
        }
        if (wd->reloadAmmoAdd == 0 || (limit = BG_GetClipTypeSize(clipSlot), limit <= wd->reloadAmmoAdd)) {
            if (ps->ammoclip[clipSlot] == 0) {
                return 1;
            }
        } else {
            limit = BG_GetClipTypeSize(clipSlot);
            if (wd->reloadAmmoAdd <= limit - ps->ammoclip[clipSlot]) {
                return 1;
            }
        }
    }
    return 0;
}

// 0x080ee2dc  size=471  FUN_080ee2dc  status=translated  naming=unknown
// naming: descriptive; consumes one pending reload step for the current weapon and updates weaponDelay with the step size.
void PM_Weapon_ApplyReloadStep(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 count;
    int32 step;
    int32 slot;

    if (wd->boltAction == 0 || !BitSet_Test(ps->weaponrechamber, ps->weapon)) {
        BG_ReloadClipFromAmmo(ps);
        return;
    }
    BitArray_ClearBit(ps->weaponrechamber, ps->weapon);
    PM_AddEvent(ps, 0xa2);
    if ((ps->weaponstate != WEAPSTATE_7 && ps->weaponstate != WEAPSTATE_8) ||
        wd->reloadStartAddTime != 0) {
        if (ps->weaponTime == 0) {
            BG_ReloadClipFromAmmo(ps);
        } else {
            if (ps->weaponstate == WEAPSTATE_7 || ps->weaponstate == WEAPSTATE_8) {
                if (wd->reloadStartAddTime < wd->reloadStartTime) {
                    count = wd->reloadStartAddTime;
                } else {
                    count = wd->reloadStartTime;
                }
            } else {
                slot = BG_GetWeaponClipIndex(ps->weapon);
                if (ps->ammoclip[slot] == 0 && wd->weaponType == 0) {
                    count = wd->reloadEmptyTime;
                } else {
                    count = wd->reloadTime;
                }
                if (wd->reloadAddTime != 0 && wd->reloadAddTime < count) {
                    count = wd->reloadAddTime;
                }
            }
            if (wd->rechamberBoltTime < count) {
                step = wd->rechamberBoltTime;
            } else {
                step = 1;
            }
            if (count - step < 1) {
                BG_ReloadClipFromAmmo(ps);
            } else {
                ps->weaponDelay = count - step;
            }
        }
    }
}

// 0x080ee4b4  size=277  FUN_080ee4b4  status=translated  naming=unknown
// naming: descriptive; finishes a fire cycle. Applies a reload step when the fire input is held, then either re-enters the reload path or returns to READY / state 9.
void PM_Weapon_FinishFireCycle(playerState_s *ps, int32 fireHeld)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);

    if (fireHeld != 0) {
        PM_Weapon_ApplyReloadStep(ps);
    }
    if (ps->weaponTime != 0) {
        return;
    }
    if ((ps->weaponstate != WEAPSTATE_8 || PM_Weapon_GetClipAmmo(ps) == 0) &&
        PM_Weapon_NeedsAutoReload(ps) != 0) {
        PM_Weapon_BeginReloadState(ps);
        return;
    }
    BitArray_ClearBit(ps->weaponrechamber, ps->weapon);
    if (wd->reloadEndTime == 0) {
        ps->weaponstate = WEAPSTATE_READY;
        PlayerState_SetWeapAnim(ps, 0);
    } else {
        ps->weaponstate = WEAPSTATE_9;
        PlayerState_SetWeapAnim(ps, 0xe);
        ps->weaponTime = wd->reloadEndTime;
        PM_AddEvent(ps, 0x99);
    }
}

// 0x080ee5ca  size=306  FUN_080ee5ca  status=translated  naming=unknown
// naming: descriptive; finishes a reload cycle once the timer has run out, choosing READY, state 9 or a new reload phase.
void PM_Weapon_FinishReloadCycle(playerState_s *ps, int32 fireHeld)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);

    if ((fireHeld == 0 || (PM_Weapon_ApplyReloadStep(ps), ps->weaponTime == 0)) &&
        ps->weaponTime == 0) {
        BitArray_ClearBit(ps->weaponrechamber, ps->weapon);
        if (wd->segmentedReload == 0) {
            ps->weaponstate = WEAPSTATE_READY;
            PlayerState_SetWeapAnim(ps, 0);
        } else if (ps->weaponstate == WEAPSTATE_6 || PM_Weapon_NeedsAutoReload(ps) == 0) {
            if (wd->reloadEndTime == 0) {
                ps->weaponstate = WEAPSTATE_READY;
                PlayerState_SetWeapAnim(ps, 0);
            } else {
                ps->weaponstate = WEAPSTATE_9;
                PlayerState_SetWeapAnim(ps, 0xe);
                ps->weaponTime = wd->reloadEndTime;
                PM_AddEvent(ps, 0x99);
            }
        } else {
            PM_Weapon_BeginReloadState(ps);
        }
    }
}

// 0x080ee6fc  size=40  FUN_080ee6fc  status=translated  naming=unknown
// naming: descriptive; same body as PM_Weapon_SetReadyState (kept as a separate entry point).
void PM_Weapon_SetReadyState_Alt(playerState_s *ps)
{
    ps->weaponstate = WEAPSTATE_READY;
    PlayerState_SetWeapAnim(ps, 0);
}

// 0x080ee724  size=569  FUN_080ee724  status=translated  naming=inferred
// naming: PM_Weapon_* from pmove layout; per-frame input edge handling and reload-trigger logic (cmd buttons, oldcmd buttons, cmd movement bytes).
void PM_Weapon_UpdateInputState(pmove_s *pm)
{
    playerState_s *ps = pm->ps;
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    bool trigger = false;
    bool fireWasLatched;
    int32 clipSlot;
    int32 reserveSlot;

    if ((ps->weaponstate < 0x11 || 0x16 < ps->weaponstate) &&
        (ps->weaponstate < 0xc || 0x10 < ps->weaponstate)) {
        fireWasLatched = (ps->pm_flags & 0x8) != 0;
        if (fireWasLatched) {
            ps->pm_flags = ps->pm_flags & 0xfffffff7u;
        }
        if (wd->segmentedReload != 0 &&
            (ps->weaponstate == WEAPSTATE_7 || ps->weaponstate == WEAPSTATE_5) &&
            (pm->cmd.buttons & 1) != 0 && (pm->oldcmd.buttons & 1) == 0) {
            if (ps->weaponstate == WEAPSTATE_7) {
                ps->weaponstate = WEAPSTATE_8;
            } else if (ps->weaponstate == WEAPSTATE_5) {
                ps->weaponstate = WEAPSTATE_6;
            }
        }

        switch (ps->weaponstate) {
        default:
            clipSlot = BG_GetWeaponClipIndex(ps->weapon);
            reserveSlot = BG_GetWeaponAmmoIndex(ps->weapon);
            if ((fireWasLatched || (pm->cmd.buttons & 0x10) != 0) &&
                PM_Weapon_NeedsAutoReload(ps) != 0) {
                trigger = true;
            }
            if (ps->ammoclip[clipSlot] == 0 && ps->ammo[reserveSlot] != 0 &&
                ps->weaponstate != WEAPSTATE_FIRING &&
                ((ps->pm_flags & 1) == 0 || (pm->cmd.forwardmove == 0 && pm->cmd.rightmove == 0))) {
                trigger = true;
            }
            if (trigger) {
                PM_Weapon_TryBeginReload(ps);
            }
            break;
        case 1:
        case 2:
        case 10:
        case 0xb:
            break;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            if (pm->proneChange != 0 && BG_GetWeaponClipOnly(ps->weapon) == 0) {
                BG_AnimScriptEvent(ps, 10, 0, 1);
            }
            break;
        }
    }
}

// 0x080ee95e  size=143  FUN_080ee95e  status=translated  naming=unknown
// naming: descriptive; when the weapon has no magazine and no reserve, notifies event 0x92 and runs the empty-weapon hook.
void PM_Weapon_TakeEmptyClipOnlyWeapon(playerState_s *ps)
{
    if (BG_GetWeaponClipOnly(ps->weapon) != 0 &&
        ps->ammoclip[BG_GetWeaponClipIndex(ps->weapon)] == 0 &&
        ps->ammo[BG_GetWeaponAmmoIndex(ps->weapon)] == 0) {
        BG_TakeWeapon(ps, ps->weapon);
        PM_AddEvent(ps, 0x92);
    }
}

// 0x080ee9ee  size=677  FUN_080ee9ee  status=translated  naming=unknown
// naming: descriptive; updates the 0..255 weapon meter from the view-angle delta, velocity and movement inputs (uses pml frametime).
void PM_Weapon_UpdateAimSpread(pmove_s *pm, pml_s *pml)
{
    playerState_s *ps = pm->ps;
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    float local_1c = wd->hipSpreadDecayRate;
    float local_14 = 0.0f;
    float local_10 = 0.0f;
    float local_18 = 0.0f;
    int32 i;

    if (local_1c != 0.0f || local_1c != local_1c) {
        if (ps->groundEntityNum == 0x3ff && ps->pm_type != 1) {
            local_1c = local_1c * 0.5f;
        } else if ((ps->eFlags & 8) == 0) {
            if ((ps->eFlags & 4) != 0) {
                local_1c = local_1c * wd->hipSpreadDuckedDecay;
            }
        } else {
            local_1c = local_1c * wd->hipSpreadProneDecay;
        }
        local_14 = local_1c * pml->frametime;
        if (ps->fWeaponPosFrac == 1.0f) {
            local_10 = 0.0f;
        } else {
            local_18 = 0.0f;
            if (wd->hipSpreadTurnAdd != 0.0f || wd->hipSpreadTurnAdd != wd->hipSpreadTurnAdd) {
                for (i = 0; i < 2; i++) {
                    float diff = AngleSubtract((float)pm->cmd.angles[i] * 0.005493164f,
                                               (float)pm->oldcmd.angles[i] * 0.005493164f);
                    float absDiff = Float_Abs_080f28cc(diff);
                    long double lVar3 = (long double)absDiff;
                    local_18 = (float)((long double)local_18 +
                                       ((long double)0.01f * lVar3 * (long double)wd->hipSpreadTurnAdd) /
                                           (long double)pml->frametime);
                }
            }
            if ((wd->hipSpreadMoveAdd != 0.0f || wd->hipSpreadMoveAdd != wd->hipSpreadMoveAdd) &&
                (pm->cmd.forwardmove != 0 || pm->cmd.rightmove != 0)) {
                float speed = Vec2_LengthSquared(ps->velocity);
                float limit = Float_SquareLD(dvar_bg_aimSpreadMoveSpeedThreshold->current.value);
                if ((long double)limit < (long double)speed) {
                    local_18 = local_18 + wd->hipSpreadMoveAdd;
                }
            }
            if (ps->groundEntityNum == 0x3ff && ps->pm_type != 1) {
                for (i = 0; i < 2; i++) {
                    local_18 = local_18 + 1.28f;
                }
            }
            local_10 = local_18 * pml->frametime;
        }
    } else {
        local_10 = 0.0f;
        local_14 = 1.0f;
    }

    ps->aimSpreadScale = ps->aimSpreadScale + (local_10 - local_14) * 255.0;
    if (ps->aimSpreadScale < 0.0) {
        ps->aimSpreadScale = 0.0f;
    } else if (255.0 < ps->aimSpreadScale) {
        ps->aimSpreadScale = 255.0f;
    }
}

// 0x080eec94  size=625  FUN_080eec94  status=translated  naming=inferred
// naming: PM_Weapon_* from pmove layout; decrements the shot delay, weapon timer and weapon delay by frame msec and returns 1 when the delay count expires.
int32 PM_Weapon_TickTimers(pmove_s *pm, pml_s *pml)
{
    playerState_s *ps = pm->ps;
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 result;
    bool outerCond;

    if (0 < ps->weaponRestrictKickTime) {
        ps->weaponRestrictKickTime = ps->weaponRestrictKickTime - pml->msec;
        ps->weaponRestrictKickTime = Int_Max2(ps->weaponRestrictKickTime, 0);
    }
    outerCond = (ps->weaponTime != 0);
    if (outerCond) {
        ps->weaponTime = ps->weaponTime - pml->msec;
        outerCond = (ps->weaponTime < 1);
    }
    if (outerCond) {
        if ((ps->weaponstate < 0xc || 0x10 < ps->weaponstate) &&
            wd->semiAuto != 0 && (pm->cmd.buttons & 1) != 0 &&
            (uint32)ps->weapon == (uint32)pm->cmd.weapon &&
            PM_Weapon_GetClipAmmo(ps) != 0) {
            ps->weaponTime = 1;
            if (ps->weaponstate == WEAPSTATE_5 || ps->weaponstate == WEAPSTATE_7 ||
                ps->weaponstate == WEAPSTATE_9 || ps->weaponstate == WEAPSTATE_8 ||
                ps->weaponstate == WEAPSTATE_6) {
                ps->weaponTime = 0;
            }
            if ((ps->weaponstate == WEAPSTATE_5 || ps->weaponstate == WEAPSTATE_7 ||
                 ps->weaponstate == WEAPSTATE_9 || ps->weaponstate == WEAPSTATE_8 ||
                 ps->weaponstate == WEAPSTATE_6) &&
                ps->weaponstate != WEAPSTATE_9 &&
                wd->segmentedReload != 0 && wd->reloadStartTime != 0) {
                ps->weaponTime = 0;
            } else if (ps->weaponstate == WEAPSTATE_4) {
                PM_Weapon_ClearToReady(ps);
            } else if (ps->weaponstate == WEAPSTATE_FIRING || ps->weaponstate == WEAPSTATE_4 ||
                       ps->weaponstate == WEAPSTATE_10 || ps->weaponstate == WEAPSTATE_11) {
                PlayerState_SetWeapAnimIfChanged(ps, 0);
                ps->weaponstate = WEAPSTATE_READY;
            }
        } else {
            ps->weaponTime = 0;
        }
    }

    if (ps->weaponDelay == 0 ||
        (ps->weaponDelay = ps->weaponDelay - pml->msec, 0 < ps->weaponDelay)) {
        result = 0;
    } else {
        ps->weaponDelay = 0;
        result = 1;
    }
    return result;
}

// 0x080eef06  size=577  FUN_080eef06  status=translated  naming=inferred
// naming: PM_Weapon_* from pmove layout; decides whether the cmd weapon slot differs from the current weapon and requests a weapon change.
void PM_Weapon_ChangeSlot(pmove_s *pm)
{
    playerState_s *ps = pm->ps;
    bool cond;
    bool sameSlot;

    cond = ps->weaponTime == 0 || ps->weaponstate == WEAPSTATE_5 ||
           ps->weaponstate == WEAPSTATE_7 || ps->weaponstate == WEAPSTATE_9 ||
           ps->weaponstate == WEAPSTATE_8 || ps->weaponstate == WEAPSTATE_6 ||
           ps->weaponstate == WEAPSTATE_4 ||
           (ps->weaponstate != WEAPSTATE_FIRING && ps->weaponstate != WEAPSTATE_4 &&
            ps->weaponstate != WEAPSTATE_10 && ps->weaponstate != WEAPSTATE_11 &&
            ps->weaponDelay == 0);

    if ((ps->weaponstate < 0x11 || 0x16 < ps->weaponstate) &&
        (ps->weaponstate < 0xc || 0x10 < ps->weaponstate) && cond) {
        if (!Mantle_OverAnimNotLow(ps)) {
            if ((ps->pm_flags & 0x20) == 0) {
                if ((ps->pm_flags & 0x04000000) == 0) {
                    sameSlot = ((uint32)ps->weapon == (uint32)pm->cmd.weapon) ||
                               (((ps->pm_flags & 0x8000) != 0) && ps->weapon != 0) ||
                               (((int8)pm->cmd.weapon != 0) &&
                                !BG_PlayerHasWeapon(ps, (int8)pm->cmd.weapon));
                    if (sameSlot) {
                        if (ps->weapon != 0 && !BitSet_Test(ps->weapons, ps->weapon)) {
                            PM_Weapon_SelectWeapon(ps, 0);
                        }
                    } else {
                        PM_Weapon_SelectWeapon(ps, (int8)pm->cmd.weapon);
                    }
                } else if (ps->weapon != 0) {
                    PM_Weapon_SelectWeapon(ps, 0);
                }
            } else if (ps->weapon != 0) {
                PM_Weapon_SelectWeapon(ps, 0);
            }
        } else if (ps->weapon != 0) {
            PM_Weapon_SelectWeapon(ps, 0);
        }
    }
}

// 0x080ef148  size=100  FUN_080ef148  status=translated  naming=unknown
// naming: descriptive; releases a fire hold when the trigger is up and no hold-override is set; returns 1 when the release was taken.
int32 PM_Weapon_ReleaseFireHold(pmove_s *pm, int32 fireHeld)
{
    playerState_s *ps = pm->ps;

    if ((pm->cmd.buttons & 1) != 0 || fireHeld != 0) {
        return 0;
    }
    if (ps->weaponstate == WEAPSTATE_FIRING) {
        PlayerState_SetWeapAnimIfChanged(ps, 0);
    }
    ps->weaponstate = WEAPSTATE_READY;
    return 1;
}

// 0x080ef1ac  size=457  FUN_080ef1ac  status=translated  naming=unknown
// naming: descriptive; enters the FIRING state, starts the shot timers from the weapon definition, and sets the fire flag bit.
void PM_Weapon_BeginFire(playerState_s *ps, int32 altFire)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 hasClip;

    if (wd->weaponType == 1) {
        if (altFire == 0) {
            hasClip = PM_Weapon_GetClipAmmo(ps);
            if (hasClip != 0) {
                ps->grenadeTimeLeft = wd->fuseTime;
                PlayerState_SetWeapAnim(ps, 0x13);
                PM_AddEvent(ps, 0x9d);
            }
            ps->weaponDelay = wd->holdFireTime;
            ps->weaponTime = 0;
        }
    } else {
        ps->weaponDelay = wd->fireDelay;
        ps->weaponTime = wd->fireTime;
        if (wd->adsFire != 0) {
            /* Truncation: the FPU rounding control is set to toward-zero (control word |= 0xc00), i.e. a C (int) cast. */
            ps->weaponDelay = (int32)((1.0 / wd->adsTransInTimeInv) * (1.0 - ps->fWeaponPosFrac));
        }
        BG_AnimScriptEvent(ps, 2, 0, 1);
        if (wd->boltAction != 0) {
            BitSet_Set(ps->weaponrechamber, ps->weapon);
        }
        if (ps->weaponstate != WEAPSTATE_FIRING) {
            if (1.0f <= ps->fWeaponPosFrac) {
                ps->weaponRestrictKickTime = wd->fireDelay + wd->fireTime * wd->adsGunKickReducedKickBullets;
            } else {
                ps->weaponRestrictKickTime = wd->fireDelay + wd->fireTime * wd->hipGunKickReducedKickBullets;
            }
        }
    }
    ps->weaponstate = WEAPSTATE_FIRING;
    PM_Flags_SetBit11IfBit0(ps);
}

// 0x080ef376  size=10  FUN_080ef376  status=translated  naming=unknown
// naming: descriptive; constant one-unit ammo cost per shot. The weapon argument is ignored (the caller pushes it).
int32 PM_Weapon_AmmoPerShot(int32 weapon)
{
    (void)weapon;
    return 1;
}

// 0x080ef380  size=277  FUN_080ef380  status=translated  naming=unknown
// naming: descriptive; when the magazine is below one shot, either notifies empty and clears the reserve bit, or starts a reload.
int32 PM_Weapon_CheckClipOrReload(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 cost = PM_Weapon_AmmoPerShot(ps->weapon);
    int32 clip = PM_Weapon_GetClipAmmo(ps);
    int32 reserveSlot;
    bool noReserve;

    if (clip < cost) {
        reserveSlot = BG_GetWeaponAmmoIndex(ps->weapon);
        noReserve = ps->ammo[reserveSlot] < cost;
        if (wd->weaponType != 1 && noReserve) {
            PM_AddEvent(ps, 0x92);
        }
        if (noReserve) {
            BitArray_ClearBit(ps->weaponrechamber, ps->weapon);
            PlayerState_SetWeapAnimIfChanged(ps, 0);
            ps->weaponTime = ps->weaponTime + 500;
        } else {
            PM_Weapon_TryBeginReload(ps);
        }
        return 0;
    }
    return 1;
}

// 0x080ef496  size=116  FUN_080ef496  status=translated  naming=unknown
// naming: descriptive; picks the fire animation (2/3 below 0.75 blend, 5/6 above) from whether the magazine is empty.
void PM_Weapon_SetFireAnimState(playerState_s *ps)
{
    int32 anim;

    if (0.75 < ps->fWeaponPosFrac) {
        anim = PM_Weapon_ClipEmpty(ps) ? 6 : 5;
    } else {
        anim = PM_Weapon_ClipEmpty(ps) ? 3 : 2;
    }
    PlayerState_SetWeapAnim(ps, anim);
}

// 0x080ef50a  size=127  FUN_080ef50a  status=translated  naming=unknown
// naming: descriptive; adds the per-weapon meter increment (scaled by 255) while the blend is not fully aimed, clamped at 255.
void PM_Weapon_AddFireAimSpread(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);

    if (ps->fWeaponPosFrac != 1.0f) {
        ps->aimSpreadScale = ps->aimSpreadScale + wd->hipSpreadFireAdd * 255.0;
        if (255.0 < ps->aimSpreadScale) {
            ps->aimSpreadScale = 255.0f;
        }
    }
}

// 0x080ef58a  size=282  FUN_080ef58a  status=translated  naming=unknown
// naming: descriptive; full shot: begin fire, check clip, consume ammo, then notify the fire animation and the empty check.
void PM_Weapon_FireShot(playerState_s *ps, int32 altFire)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 ok;
    int32 clip;
    int32 cost;

    PM_Weapon_BeginFire(ps, altFire);
    ok = PM_Weapon_CheckClipOrReload(ps);
    if (ok != 0 && ps->weaponDelay == 0) {
        clip = PM_Weapon_GetClipAmmo(ps);
        if (clip != -1 && (ps->eFlags & 0x300) == 0) {
            cost = PM_Weapon_AmmoPerShot(ps->weapon);
            PM_Weapon_SubClipAmmo(ps, ps->weapon, cost);
        }
        if (wd->weaponType == 1) {
            ps->weaponTime = wd->fireTime;
        }
        PM_Weapon_SetFireAnimState(ps);
        if (PM_Weapon_ClipEmpty(ps)) {
            PM_AddEvent(ps, 0xa0);
        } else {
            PM_AddEvent(ps, 0x9e);
        }
        PM_ApplyBreathFireDelay(ps);
        PM_Weapon_AddFireAimSpread(ps);
        PM_Weapon_TakeEmptyClipOnlyWeapon(ps);
    }
}

// 0x080ef6a4  size=130  FUN_080ef6a4  status=translated  naming=unknown
// naming: descriptive; enters state 11 with a timer floor taken from the weapon definition.
void PM_Weapon_BeginMeleeStrike(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 floorTime = wd->meleeTime - wd->meleeDelay;

    if (ps->weaponTime < floorTime) {
        ps->weaponTime = floorTime;
    }
    PM_AddEvent(ps, 0xa4);
    ps->weaponstate = WEAPSTATE_11;
    PM_Flags_SetBit11IfBit0(ps);
}

// 0x080ef726  size=40  FUN_080ef726  status=translated  naming=unknown
// naming: descriptive; same body as PM_Weapon_ClearToReady (kept as a separate entry point), clears the timer first then sets READY.
void PM_Weapon_ClearToReady_Alt(playerState_s *ps)
{
    PlayerState_SetWeapAnimIfChanged(ps, 0);
    ps->weaponstate = WEAPSTATE_READY;
}

// 0x080ef74e  size=504  FUN_080ef74e  status=translated  naming=unknown
// naming: descriptive; starts the alternate fire (state 10 or 11) when the alt-fire flag is set and the fire button edge conditions hold.
void PM_Weapon_TryStartMelee(pmove_s *pm, int32 fireHeld)
{
    playerState_s *ps = pm->ps;
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);
    int32 st = ps->weaponstate;

    if ((st < 0x11 || 0x16 < st) && (st < 0xc || 0x10 < st) &&
        wd->meleeDamage != 0 && fireHeld == 0 &&
        (ps->weaponDelay == 0 || st == WEAPSTATE_5 || st == WEAPSTATE_7 ||
         st == WEAPSTATE_9 || st == WEAPSTATE_8 || st == WEAPSTATE_6)) {
        if ((pm->cmd.buttons & 4) == 0) {
            ps->pm_flags = ps->pm_flags & 0xffffdfffu;
        } else if ((ps->pm_flags & PMF_FLAG_0x2000) == 0) {
            ps->pm_flags = ps->pm_flags | PMF_FLAG_0x2000;
            if ((ps->fWeaponPosFrac <= 0.0f || wd->adsOverlayReticle == 0) &&
                (st < 1 || (2 < st && (uint32)(st - 10) > 1))) {
                BG_AnimScriptEvent(ps, 0x11, 0, 1);
                PlayerState_SetWeapAnim(ps, 8);
                PM_AddEvent(ps, 0xa3);
                if (wd->meleeDelay == 0) {
                    PM_Weapon_BeginMeleeStrike(ps);
                } else {
                    ps->weaponTime = wd->meleeTime;
                    ps->weaponDelay = wd->meleeDelay;
                    ps->weaponstate = WEAPSTATE_10;
                    PM_Flags_SetBit11IfBit0(ps);
                }
            }
        }
    }
}

// 0x080ef946  size=112  FUN_080ef946  status=translated  naming=unknown
// naming: descriptive; forces the weapon back to READY, clearing timers, the 0x810 pmFlags bits and the ADS reset for states 17..22.
void PM_Weapon_ForceReady(playerState_s *ps)
{
    ps->pm_flags = ps->pm_flags & 0xfffff7efu;
    if (0x10 < ps->weaponstate && ps->weaponstate < 0x17) {
        PM_Weapon_ResetAdsState(ps);
    }
    ps->weaponTime = 0;
    ps->weaponDelay = 0;
    ps->weaponstate = WEAPSTATE_READY;
    PlayerState_SetWeapAnim(ps, 0);
}

// 0x080ef9b6  size=111  FUN_080ef9b6  status=translated  naming=unknown
// naming: descriptive; enters state 12 (timer from weapon definition 0x240) and sends animation 0x11.
void PM_Weapon_BeginOffhandLower(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);

    ps->weaponstate = WEAPSTATE_12;
    ps->weaponTime = wd->quickDropTime;
    ps->weaponDelay = 0;
    ps->pm_flags = ps->pm_flags & 0xffffffefu;
    PM_ClearAdsFlag(ps);
    PlayerState_SetWeapAnim(ps, 0x11);
}

// 0x080efa26  size=143  FUN_080efa26  status=translated  naming=inferred
// naming: inferred offhand entry; enters state 13 for the offhand weapon (event 0xa5 and flag 0x10). Offhand from the grenade-only diagnostics in PM_Weapon_HandleOffhandButtons.
void PM_Weapon_EnterOffhandState13(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->offHandIndex);

    ps->weaponstate = WEAPSTATE_13;
    ps->weaponTime = wd->holdFireTime;
    ps->weaponDelay = 0;
    ps->pm_flags = ps->pm_flags | PMF_OFFHAND_ACTIVE;
    BG_AddPredictableEventToPlayerstate(0xa5, ps->offHandIndex, ps);
    PlayerState_SetWeapAnim(ps, 0x13);
    PM_Flags_SetBit11IfBit0(ps);
}

// 0x080efab6  size=91  FUN_080efab6  status=translated  naming=inferred
// naming: inferred offhand; enters state 14 with the offhand timer loaded from the weapon definition (0x248).
void PM_Weapon_EnterOffhandState14(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->offHandIndex);

    ps->weaponstate = WEAPSTATE_14;
    ps->weaponTime = 0;
    ps->weaponDelay = 0;
    ps->pm_flags = ps->pm_flags | PMF_OFFHAND_ACTIVE;
    ps->grenadeTimeLeft = wd->fuseTime;
}

// 0x080efb12  size=299  FUN_080efb12  status=translated  naming=inferred
// naming: inferred offhand throw; state 15, consumes one offhand unit and notifies 0x94 when the offhand weapon has no ammo left.
void PM_Weapon_ThrowOffhand(pmove_s *pm)
{
    playerState_s *ps = pm->ps;
    weaponDef_s *wd;
    int32 remaining;

    if ((pm->oldcmd.buttons & 0x30000) == 0 || (pm->cmd.buttons & 0x30000) == 0) {
        wd = BG_GetWeaponDef(ps->offHandIndex);
        ps->weaponstate = WEAPSTATE_15;
        ps->weaponTime = wd->fireTime;
        ps->weaponDelay = 0;
        ps->pm_flags = ps->pm_flags | PMF_OFFHAND_ACTIVE;
        BG_AddPredictableEventToPlayerstate(0xa6, ps->offHandIndex, ps);
        PlayerState_SetWeapAnim(ps, 2);
        PM_Weapon_SubClipAmmo(ps, ps->offHandIndex, 1);
        BG_AnimScriptEvent(ps, 2, 0, 1);
        remaining = BG_GetWeaponTotalAmmo(ps, ps->offHandIndex);
        if (remaining == 0) {
            PM_AddEvent(ps, 0x94);
        }
    } else {
        ps->weaponDelay = 1;
    }
}

// 0x080efc3e  size=136  FUN_080efc3e  status=translated  naming=unknown
// naming: descriptive; enters state 16 with the timer from definition field 0x244 (or a one-tick delay when no weapon is held).
void PM_Weapon_BeginOffhandRaise(playerState_s *ps)
{
    weaponDef_s *wd;

    if (ps->weapon == 0) {
        ps->weaponTime = 0;
        ps->weaponDelay = 1;
    } else {
        wd = BG_GetWeaponDef(ps->weapon);
        ps->weaponTime = wd->quickRaiseTime;
        ps->weaponDelay = 0;
        PlayerState_SetWeapAnim(ps, 0x12);
    }
    ps->weaponstate = WEAPSTATE_16;
    ps->pm_flags = ps->pm_flags & 0xfffff7efu;
}

// 0x080efcc6  size=27  FUN_080efcc6  status=translated  naming=unknown
// naming: descriptive; sends event 0x94. The second argument (1 or 2) is pushed by the caller and ignored by this body.
void PM_Weapon_OffhandDenied(playerState_s *ps, int32 hand)
{
    (void)hand;
    PM_AddEvent(ps, 0x94);
}

// 0x080efce2  size=465  FUN_080efce2  status=translated  naming=inferred
// naming: inferred offhand handler from the grenade-only and "No offhand class set" diagnostics in this body.
void PM_Weapon_HandleOffhandButtons(pmove_s *pm)
{
    playerState_s *ps = pm->ps;
    weaponDef_s *wd;
    int32 hand;
    int32 newOffhand;

    if ((ps->eFlags & 0x300) == 0 && (ps->pm_flags & 0x04000000) == 0 &&
        (ps->weaponstate < 0x11 || 0x16 < ps->weaponstate) &&
        ((ps->weaponstate < 0xc || 0x10 < ps->weaponstate) || ps->weaponstate == 0x10)) {
        if (BitSet_Test(ps->weapons, (int32)pm->cmd.offHandIndex)) {
            ps->offHandIndex = (int32)pm->cmd.offHandIndex;
        }
        if ((pm->cmd.buttons & 0x10000) == 0) {
            if ((pm->cmd.buttons & 0x20000) == 0) {
                return;
            }
            hand = 2;
        } else {
            hand = 1;
        }
        newOffhand = BG_FindUsableWeaponOfClass(ps, hand);
        if (newOffhand == 0) {
            PM_Weapon_OffhandDenied(ps, hand);
        } else {
            BG_AddPredictableEventToPlayerstate(0xa7, newOffhand, ps);
            ps->offHandIndex = newOffhand;
            wd = BG_GetWeaponDef(ps->offHandIndex);
            if (wd->weaponType != 1) {
                Com_Error(1, "[%s] Only grenades are currently supported for off hand use\n", wd->szInternalName);
            }
            if (wd->offhandClass == 0) {
                Com_Error(1, "[%s] No offhand class set\n", wd->szInternalName);
            }
            if (ps->weapon == 0 || ps->weaponstate == WEAPSTATE_16) {
                PM_Weapon_EnterOffhandState13(ps);
            } else {
                PM_Weapon_BeginOffhandLower(ps);
            }
        }
    }
}

// 0x080efeb4  size=205  FUN_080efeb4  status=translated  naming=inferred
// naming: inferred offhand timer; while the offhand flag is set, counts the offhand timer down by frame msec and throws (consumes one unit, event 0xc5) when it expires.
int32 PM_Weapon_TickOffhandTimer(playerState_s *ps, pml_s *pml)
{
    int32 result;
    int32 offhand;
    weaponDef_s *wd;

    if ((ps->pm_flags & PMF_OFFHAND_ACTIVE) == 0) {
        result = 0;
    } else {
        offhand = ps->offHandIndex;
        wd = BG_GetWeaponDef(offhand);
        if (wd->weaponType == 1) {
            if (ps->grenadeTimeLeft < 1) {
                result = 0;
            } else {
                if (wd->cookOffHold != 0) {
                    ps->grenadeTimeLeft = ps->grenadeTimeLeft - pml->msec;
                }
                if (ps->grenadeTimeLeft < 1) {
                    PM_Weapon_SubClipAmmo(ps, offhand, 1);
                    PM_AddEvent(ps, 0xc5);
                    result = 1;
                } else {
                    result = 0;
                }
            }
        } else {
            result = 0;
        }
    }
    return result;
}

// 0x080eff82  size=44  FUN_080eff82  status=translated  naming=unknown
// naming: descriptive; sets pmFlags bit 0x20000 and sends event 0xa8.
void PM_Weapon_SetBinocularsActive(playerState_s *ps)
{
    ps->pm_flags = ps->pm_flags | PMF_FLAG_0x20000;
    PM_AddEvent(ps, 0xa8);
}

// 0x080effae  size=44  FUN_080effae  status=translated  naming=unknown
// naming: descriptive; clears pmFlags bit 0x20000 and sends event 0xa9.
void PM_Weapon_ClearBinocularsActive(playerState_s *ps)
{
    ps->pm_flags = ps->pm_flags & 0xfffdffffu;
    PM_AddEvent(ps, 0xa9);
}

// 0x080effda  size=44  FUN_080effda  status=translated  naming=unknown
// naming: descriptive; resets the ADS blend to zero, clears the view-state through PM_ClearAdsFlag and clears flag 0x20000.
void PM_Weapon_ResetAdsState(playerState_s *ps)
{
    ps->fWeaponPosFrac = 0.0f;
    PM_ClearAdsFlag(ps);
    PM_Weapon_ClearBinocularsActive(ps);
}

// 0x080f0006  size=85  FUN_080f0006  status=translated  naming=unknown
// naming: descriptive; enters state 17 with a delay of (level time + definition field 0x2c4).
void PM_Weapon_BinocularsEnterBegin(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);

    PM_ClearAdsFlag(ps);
    ps->weaponTime = 0;
    ps->weaponDelay = dvar_player_adsExitDelay->current.integer + wd->adsTransOutTime;
    ps->weaponstate = WEAPSTATE_17;
}

// 0x080f005c  size=85  FUN_080f005c  status=translated  naming=unknown
// naming: descriptive; enters state 18 with the timer from definition field 0x240 and sends animation 0x11.
void PM_Weapon_BinocularsLowerWeapon(playerState_s *ps)
{
    weaponDef_s *wd = BG_GetWeaponDef(ps->weapon);

    ps->weaponstate = WEAPSTATE_18;
    ps->weaponTime = wd->quickDropTime;
    ps->weaponDelay = 0;
    PlayerState_SetWeapAnim(ps, 0x11);
}

// 0x080f00b2  size=98  FUN_080f00b2  status=translated  naming=unknown
// naming: descriptive; enters state 19 for the owner's weapon id (timer from field 0x2c0) and sets flag 0x20000.
void PM_Weapon_BinocularsRaise(playerState_s *ps)
{
    uint32 owner = BG_FindFirstBinocularsWeapon();
    weaponDef_s *wd = BG_GetWeaponDef((int32)owner);

    ps->weaponstate = WEAPSTATE_19;
    ps->weaponTime = wd->adsTransInTime;
    ps->weaponDelay = 0;
    PM_Weapon_SetBinocularsActive(ps);
    PlayerState_SetWeapAnim(ps, 0);
}

// 0x080f0114  size=38  FUN_080f0114  status=translated  naming=unknown
// naming: descriptive; enters state 20 with no timer.
void PM_Weapon_BinocularsHold(playerState_s *ps)
{
    ps->weaponstate = WEAPSTATE_20;
    ps->weaponTime = 0;
    ps->weaponDelay = 0;
}

// 0x080f013a  size=114  FUN_080f013a  status=translated  naming=unknown
// naming: descriptive; enters state 21 with a delay of (level time + definition field 0x2c4) and sends event 0xac.
void PM_Weapon_BinocularsExitBegin(playerState_s *ps)
{
    uint32 owner = BG_FindFirstBinocularsWeapon();
    weaponDef_s *wd = BG_GetWeaponDef((int32)owner);

    ps->weaponstate = WEAPSTATE_21;
    ps->weaponTime = dvar_player_adsExitDelay->current.integer + wd->adsTransOutTime;
    ps->weaponDelay = 0;
    PM_AddEvent(ps, 0xac);
    PlayerState_SetWeapAnim(ps, 0);
}

// 0x080f01ac  size=135  FUN_080f01ac  status=translated  naming=unknown
// naming: descriptive; enters state 22, clears flag 0x20000, and either sets a one-tick delay or loads the timer from field 0x244 according to the cmd weapon slot.
void PM_Weapon_BinocularsRestoreWeapon(pmove_s *pm)
{
    playerState_s *ps = pm->ps;
    weaponDef_s *wd;

    ps->weaponstate = WEAPSTATE_22;
    PM_Weapon_ClearBinocularsActive(ps);
    if ((int8)pm->cmd.weapon == 0) {
        ps->weaponTime = 0;
        ps->weaponDelay = 1;
    } else {
        wd = BG_GetWeaponDef(ps->weapon);
        ps->weaponTime = wd->quickRaiseTime;
        ps->weaponDelay = 0;
        PlayerState_SetWeapAnim(ps, 0x12);
    }
}

// 0x080f0234  size=1082  FUN_080f0234  status=translated  naming=inferred
// naming: inferred pmove button-edge handler (cmd.buttons vs oldcmd.buttons, weapon/offhand slot changes, state 0x14 and 0x11..0x15 transitions).
void PM_Weapon_HandleButtonEdges(pmove_s *pm)
{
    playerState_s *ps = pm->ps;
    int32 st = ps->weaponstate;
    bool changed = false;
    bool altA = false;
    bool altB = false;
    bool c1;
    bool c2;
    bool wasDown;

    c1 = (0x10 < st && st < 0x17) ||
         ((ps->fWeaponPosFrac <= 0.0f || BG_GetWeaponDef(ps->weapon)->adsOverlayReticle == 0) &&
          (pm->cmd.buttons & 0x30015) == 0);
    c2 = st != 5 && st != 7 && st != 9 && st != 8 && st != 6 && st != 3 && st != 4 &&
         st != 10 && st != 0xb && st != 1 && st != 2 && (st < 0xc || 0x10 < st);

    if (c1 && c2) {
        if (ps->damageCount != 0 || (int8)pm->oldcmd.weapon != (int8)pm->cmd.weapon ||
            (int8)pm->oldcmd.offHandIndex != (int8)pm->cmd.offHandIndex || (pm->cmd.buttons & 0x30015) != 0) {
            changed = true;
        }
        if (st < 0x11 || 0x16 < st || !changed) {
            if ((pm->cmd.buttons & 1) == 0) {
                if (st == 0x14) {
                    bool rising = false;
                    bool falling = false;
                    if ((pm->oldcmd.buttons & 0x28) == 0 && (pm->cmd.buttons & 0x28) != 0) {
                        rising = true;
                    }
                    if ((pm->oldcmd.buttons & 0x28) != 0 && (pm->cmd.buttons & 0x28) == 0) {
                        falling = true;
                    }
                    if (rising) {
                        PM_AddEvent(ps, 0xaa);
                    } else if (falling) {
                        PM_AddEvent(ps, 0xab);
                    }
                }
                if (dvar_player_toggleBinoculars->current.enabled == 0) {
                    if (((pm->cmd.buttons >> 14) & 1) != 0 && (st < 0x11 || 0x16 < st)) {
                        altA = true;
                    }
                    if (((pm->cmd.buttons >> 14) & 1) == 0 && 0x11 < st && st < 0x15) {
                        altB = true;
                    }
                } else {
                    wasDown = false;
                    if ((pm->oldcmd.buttons & 0x4000) == 0 && (pm->cmd.buttons & 0x4000) != 0) {
                        wasDown = true;
                    }
                    if (wasDown && (st < 0x11 || 0x16 < st)) {
                        altA = true;
                    }
                    if (wasDown && 0x11 < st && st < 0x15) {
                        altB = true;
                    }
                }
                if (altA) {
                    if (BG_FindFirstBinocularsWeapon() != 0) {
                        if ((int8)pm->cmd.weapon == 0) {
                            PM_Weapon_BinocularsRaise(ps);
                        } else if ((pm->cmd.buttons & 0x1000) == 0) {
                            PM_Weapon_BinocularsLowerWeapon(ps);
                        } else {
                            PM_Weapon_BinocularsEnterBegin(ps);
                        }
                    }
                } else if (altB) {
                    if (st == 0x12) {
                        PM_Weapon_BinocularsRestoreWeapon(pm);
                    } else {
                        PM_Weapon_BinocularsExitBegin(ps);
                    }
                }
            }
        } else {
            PM_Weapon_ForceReady(ps);
            if ((pm->cmd.buttons & 1) != 0) {
                ps->weaponTime = 1;
            }
        }
    }
}

// 0x080f066e  size=728  FUN_080f066e  status=translated  naming=inferred
// naming: PM_Weapon_Frame, inferred from pmove/pml layout; per-frame weapon dispatch (offhand timer, timers, button edges, slot change, input, fire) and the state switch.
void PM_Weapon_Frame(pmove_s *pm, pml_s *pml)
{
    playerState_s *ps = pm->ps;
    int32 fireHeld;
    int32 handled;

    if ((ps->pm_flags & 0x1000) == 0) {
        if (ps->pm_type < 6) {
            if ((ps->eFlags & 0x300) == 0) {
                PM_UpdateWeaponPosFrac(pm, pml);
                PM_UpdateHoldBreath(pm, pml);
                if (PM_Weapon_TickOffhandTimer(ps, pml) == 0) {
                    fireHeld = PM_Weapon_TickTimers(pm, pml);
                    PM_Weapon_HandleButtonEdges(pm);
                    PM_Weapon_HandleOffhandButtons(pm);
                    PM_Weapon_ChangeSlot(pm);
                    PM_Weapon_UpdateInputState(pm);
                    PM_Weapon_TryStartMelee(pm, fireHeld);
                    handled = PM_Weapon_TryStartRechamber(ps, fireHeld);
                    if (handled == 0) {
                        if (((ps->pm_flags & 1) != 0 &&
                             (pm->cmd.forwardmove != 0 || pm->cmd.rightmove != 0)) ||
                            ps->weaponstate == WEAPSTATE_10 || ps->weaponstate == WEAPSTATE_11) {
                            ps->aimSpreadScale = 255.0f;
                        }
                        if (fireHeld != 0 || (ps->weaponTime == 0 && ps->weaponDelay == 0)) {
                            switch (ps->weaponstate) {
                            default:
                                if (ps->weapon != 0 && PM_Weapon_ReleaseFireHold(pm, fireHeld) == 0) {
                                    PM_Weapon_FireShot(ps, fireHeld);
                                }
                                break;
                            case 1:
                                PM_Weapon_SetReadyState(ps);
                                break;
                            case 2:
                                PM_Weapon_ChangeWeapon(pm);
                                break;
                            case 5:
                            case 6:
                                PM_Weapon_FinishReloadCycle(ps, fireHeld);
                                break;
                            case 7:
                            case 8:
                                PM_Weapon_FinishFireCycle(ps, fireHeld);
                                break;
                            case 9:
                                PM_Weapon_SetReadyState_Alt(ps);
                                break;
                            case 10:
                                PM_Weapon_BeginMeleeStrike(ps);
                                break;
                            case 0xb:
                                PM_Weapon_ClearToReady_Alt(ps);
                                break;
                            case 0xc:
                                PM_Weapon_EnterOffhandState13(ps);
                                break;
                            case 0xd:
                                PM_Weapon_EnterOffhandState14(ps);
                                break;
                            case 0xe:
                                PM_Weapon_ThrowOffhand(pm);
                                break;
                            case 0xf:
                                PM_Weapon_BeginOffhandRaise(ps);
                                break;
                            case 0x10:
                                PM_Weapon_ForceReady(ps);
                                break;
                            case 0x11:
                                PM_Weapon_BinocularsLowerWeapon(ps);
                                break;
                            case 0x12:
                                PM_Weapon_BinocularsRaise(ps);
                                break;
                            case 0x13:
                                PM_Weapon_BinocularsHold(ps);
                                break;
                            case 0x14:
                                break;
                            case 0x15:
                                PM_Weapon_BinocularsRestoreWeapon(pm);
                                break;
                            case 0x16:
                                PM_Weapon_ForceReady(ps);
                                break;
                            }
                        }
                    }
                }
            }
        } else {
            ps->weapon = 0;
        }
    }
}

// 0x080f0946  size=19  FUN_080f0946  status=translated  naming=inferred
// naming: entry wrapper that forwards to PM_Weapon_ForceReady (wrapper body only, no extra behavior).
void PM_Weapon_ForceReady_Entry(playerState_s *ps)
{
    PM_Weapon_ForceReady(ps);
}

// 0x080f095a  size=644  FUN_080f095a  status=translated  naming=unknown
// naming: descriptive; computes a random sway offset (two 2-axis random ranges, ADS or hip table selected by blend) and adds it to the angles.
void BG_WeaponRandomFireKick(playerState_s *ps, float *angles, float *out)
{
    uint32 owner = BG_GetCurrentWeapon(ps);
    weaponDef_s *wd = BG_GetWeaponDef((int32)owner);
    float blend = ps->fWeaponPosFrac;
    float local_20 = 1.0f;
    float local_10;
    float local_14;
    float fA;
    float fB;
    long double lVar6;
    long double lVar7;

    if (0 < ps->weaponRestrictKickTime) {
        if (blend == 1.0f) {
            local_20 = wd->adsGunKickReducedKickPercent * 0.01f;
        } else {
            local_20 = wd->hipGunKickReducedKickPercent * 0.01f;
        }
    }

    if (blend == 1.0f) {
        fA = wd->adsViewKickPitchMax;
        fB = wd->adsViewKickPitchMin;
        lVar6 = Rand_Fraction();
        lVar7 = (long double)wd->adsViewKickPitchMin + (long double)(fA - fB) * lVar6;
        fA = wd->adsViewKickYawMax;
        fB = wd->adsViewKickYawMin;
        lVar6 = Rand_Fraction();
        lVar6 = (long double)wd->adsViewKickYawMin + (long double)(fA - fB) * lVar6;
    } else {
        fA = wd->hipViewKickPitchMax;
        fB = wd->hipViewKickPitchMin;
        lVar6 = Rand_Fraction();
        lVar7 = (long double)wd->hipViewKickPitchMin + (long double)(fA - fB) * lVar6;
        fA = wd->hipViewKickYawMax;
        fB = wd->hipViewKickYawMin;
        lVar6 = Rand_Fraction();
        lVar6 = (long double)wd->hipViewKickYawMin + (long double)(fA - fB) * lVar6;
    }
    local_10 = (float)lVar7;
    local_14 = (float)lVar6;
    out[0] = -(local_10 * local_20);
    out[1] = local_14 * local_20;
    out[2] = out[1] * -0.5f;

    if (0.0f < blend) {
        fA = wd->adsGunKickPitchMax;
        fB = wd->adsGunKickPitchMin;
        lVar6 = Rand_Fraction();
        lVar7 = (long double)wd->adsGunKickPitchMin + (long double)(fA - fB) * lVar6;
        fA = wd->adsGunKickYawMax;
        fB = wd->adsGunKickYawMin;
        lVar6 = Rand_Fraction();
        lVar6 = (long double)wd->adsGunKickYawMin + (long double)(fA - fB) * lVar6;
    } else {
        fA = wd->hipGunKickPitchMax;
        fB = wd->hipGunKickPitchMin;
        lVar6 = Rand_Fraction();
        lVar7 = (long double)wd->hipGunKickPitchMin + (long double)(fA - fB) * lVar6;
        fA = wd->hipGunKickYawMax;
        fB = wd->hipGunKickYawMin;
        lVar6 = Rand_Fraction();
        lVar6 = (long double)wd->hipGunKickYawMin + (long double)(fA - fB) * lVar6;
    }
    local_10 = (float)lVar7;
    local_14 = (float)lVar6;
    angles[0] = angles[0] + local_10 * local_20;
    angles[1] = angles[1] + local_14 * local_20;
}

// 0x080f0bde  size=92  FUN_080f0bde  status=translated  naming=unknown
// naming: descriptive; returns 2*pi*t + 2*pi for t = byte8/255 (a phase value; the byte is ps->bobCycle, read at +8).
long double BG_BobCyclePhase(const playerState_s *ps)
{
    /* gameplay-fix(wpn_melee_ads_lean): the original keeps t in x87 extended precision, sums t*pi + t*pi + 2*pi unrounded and rounds once to float (fstp [ebp-4]; fld) at 0x080f0c2c */
    long double t = (long double)(float)(byte)ps->bobCycle / 255.0L;
    long double pi_d = (long double)3.141592653589793;
    float r = (float)(t * pi_d + t * pi_d + (long double)6.283185307179586);

    return (long double)r;
}

// 0x080f0c3a  size=192  FUN_080f0c3a  status=translated  naming=unknown
// naming: descriptive; two-harmonic sine sway (sin(2a) + 0.2*sin(4a + pi/2)), scaled by the kind-selected cvar and clamped by the limit.
long double BG_BobSinePair(playerState_s *ps, float phase, float amp, float limit)
{
    float scaled;
    double s1;
    double s2;

    if (ps->viewHeightTarget == 0xb) {
        scaled = amp * dvar_bg_bobAmplitudeProne->current.value;
    } else if (ps->viewHeightTarget == 0x28) {
        scaled = amp * dvar_bg_bobAmplitudeDucked->current.value;
    } else {
        scaled = amp * dvar_bg_bobAmplitudeStanding->current.value;
    }
    if (limit < scaled) {
        scaled = limit;
    }
    s1 = sin((double)(phase + phase));
    s2 = sin((double)(phase * 4.0f + 1.5707963267948966));
    return (long double)(((float)s1 + (float)s2 * 0.2f) * 0.75f * scaled);
}

// 0x080f0cfa  size=121  FUN_080f0cfa  status=translated  naming=unknown
// naming: descriptive; single sine sway sin(phase) scaled by the kind-selected cvar and clamped by the limit.
long double BG_BobSine(playerState_s *ps, float phase, float amp, float limit)
{
    float scaled;
    double s;

    if (ps->viewHeightTarget == 0xb) {
        scaled = amp * dvar_bg_bobAmplitudeProne->current.value;
    } else if (ps->viewHeightTarget == 0x28) {
        scaled = amp * dvar_bg_bobAmplitudeDucked->current.value;
    } else {
        scaled = amp * dvar_bg_bobAmplitudeStanding->current.value;
    }
    if (limit < scaled) {
        scaled = limit;
    }
    s = sin((double)phase);
    return (long double)(float)s * (long double)scaled;
}

// 0x080f0d74  size=971  FUN_080f0d74  status=translated  naming=unknown
// naming: descriptive; spring-like approach of the view offset toward the target blend vector, with rate limits from weapon-def fields 0x1a0/0x1a4.
// NOTE (preserved): view->offset[] is int-typed in the update loop but is passed as a float* to the
// Vec3_Add_080f2a1a/Vec3_MultAdd helpers below, so those helpers read the raw int bit patterns. Kept as observed.
void WeaponView_UpdateMoveRotation(view_motion_s *view, float *out)
{
    playerState_s *ps = view->ps;
    uint32 owner = BG_GetCurrentWeapon(ps);
    weaponDef_s *wd = BG_GetWeaponDef((int32)owner);
    float local_1c;
    float local_2c[3];
    float local_14;
    float local_18;
    long double lVar4;
    int32 i;

    if ((ps->eFlags & 8) == 0) {
        if ((ps->eFlags & 4) == 0) {
            local_1c = wd->standRotMinSpeed;
        } else {
            local_1c = wd->duckedRotMinSpeed;
        }
    } else {
        local_1c = wd->proneRotMinSpeed;
    }

    if ((float)view->elapsedMs <= local_1c || ps->weaponstate == WEAPSTATE_5) {
        Vec3_Clear_080f29c0(local_2c);
    } else {
        local_14 = ((float)view->elapsedMs - local_1c) / ((float)ps->speed - local_1c);
        lVar4 = (long double)Float_Max2(0.0f, local_14);
        lVar4 = (long double)Float_Min2(1.0f, (float)lVar4);
        local_14 = (float)lVar4;
        if ((ps->eFlags & 8) == 0) {
            if ((ps->eFlags & 4) == 0) {
                Vec3_Scale_080f2a5e(&wd->standRotP, local_14, local_2c);
            } else {
                Vec3_Scale_080f2a5e(&wd->duckedRotP, local_14, local_2c);
            }
        } else {
            Vec3_Scale_080f2a5e(&wd->proneRotP, local_14, local_2c);
        }
    }

    if (ps->fWeaponPosFrac != 0.0f || ps->fWeaponPosFrac != ps->fWeaponPosFrac) {
        local_18 = 1.0f - ps->fWeaponPosFrac;
        Vec3_Scale_080f2a5e(local_2c, local_18, local_2c);
    }

    for (i = 0; i < 3; i++) {
        if ((float)view->offset[i] != local_2c[i]) {
            if (ps->viewHeightCurrent == 11.0f) {
                local_18 = (local_2c[i] - (float)view->offset[i]) * (float)view->deltaMs * wd->posProneRotRate;
            } else {
                local_18 = (local_2c[i] - (float)view->offset[i]) * (float)view->deltaMs * wd->posRotRate;
            }
            if ((float)view->offset[i] < local_2c[i]) {
                if (local_18 < (float)view->deltaMs * 0.1f) {
                    local_18 = (float)view->deltaMs * 0.1f;
                }
                view->offset[i] = view->offset[i] + local_18;
                if (local_2c[i] < (float)view->offset[i]) {
                    view->offset[i] = local_2c[i];
                }
            } else {
                if ((float)view->deltaMs * -0.1f < local_18) {
                    local_18 = (float)view->deltaMs * -0.1f;
                }
                view->offset[i] = view->offset[i] + local_18;
                if ((float)view->offset[i] < local_2c[i]) {
                    view->offset[i] = local_2c[i];
                }
            }
        }
    }

    if (ps->fWeaponPosFrac != 0.0f || ps->fWeaponPosFrac != ps->fWeaponPosFrac) {
        if (ps->fWeaponPosFrac < 0.5f) {
            local_18 = 1.0f - (ps->fWeaponPosFrac + ps->fWeaponPosFrac);
            Vec3_MultAdd(out, local_18, (const float *)view->offset, out);
        }
    } else {
        Vec3_Add_080f2a1a(out, (const float *)view->offset, out);
    }
}

// 0x080f1140  size=112  FUN_080f1140  status=translated  naming=unknown
// naming: descriptive; adds the gated aim offset (blend * def field 0x46c) to out[0], then applies the spring step.
void WeaponView_ComputeAdsAimOffset(view_motion_s *view, float *out)
{
    playerState_s *ps = view->ps;
    uint32 owner = BG_GetCurrentWeapon(ps);
    weaponDef_s *wd = BG_GetWeaponDef((int32)owner);

    if (BG_GetWeaponAimDownSight(owner) != 0) {
        out[0] = out[0] + ps->fWeaponPosFrac * wd->adsAimPitch;
    }
    WeaponView_UpdateMoveRotation(view, out);
}

// 0x080f11b0  size=762  FUN_080f11b0  status=translated  naming=unknown
// naming: descriptive; computes the sway vector from the blend-scaled sine terms (phase accumulated in view->phase, truncated to int) and adds it to out.
void WeaponView_ComputeIdleSway(view_motion_s *view, float *out)
{
    playerState_s *ps = view->ps;
    uint32 owner = BG_GetCurrentWeapon(ps);
    weaponDef_s *wd = BG_GetWeaponDef((int32)owner);
    int32 gate = BG_GetWeaponAimDownSight(owner);
    float local_14;
    float local_28;
    float local_18;
    double d;

    if (gate == 0) {
        if (wd->hipIdleAmount != 0.0f || wd->hipIdleAmount != wd->hipIdleAmount) {
            local_14 = wd->hipIdleAmount;
            local_28 = wd->hipIdleSpeed;
        } else {
            local_14 = 80.0f;
            local_28 = 1.0f;
        }
    } else {
        local_14 = wd->hipIdleAmount + (wd->adsIdleAmount - wd->hipIdleAmount) * ps->fWeaponPosFrac;
        local_28 = wd->hipIdleSpeed + (wd->adsIdleSpeed - wd->hipIdleSpeed) * ps->fWeaponPosFrac;
    }

    if ((ps->eFlags & 8) == 0) {
        if ((ps->eFlags & 4) == 0) {
            local_18 = 1.0f;
        } else {
            local_18 = wd->idleCrouchFactor;
        }
    } else {
        local_18 = wd->idleProneFactor;
    }

    if ((wd->adsOverlayReticle == 0 || ps->fWeaponPosFrac == 0.0f) &&
        (local_18 != (float)view->blend || local_18 != local_18 ||
         (float)view->blend != (float)view->blend)) {
        if ((float)view->blend < local_18) {
            view->blend = view->blend + view->deltaMs * 0.5f;
            if (local_18 < (float)view->blend) {
                view->blend = local_18;
            }
        } else {
            view->blend = view->blend - view->deltaMs * 0.5f;
            if ((float)view->blend < local_18) {
                view->blend = local_18;
            }
        }
    }

    local_14 = local_14 * (float)view->blend;
    if (wd->adsOverlayReticle != 0) {
        local_14 = local_14 * (1.0 - ps->fWeaponPosFrac);
    }

    /* Truncation: FPU rounding control set to toward-zero (control word |= 0xc00), i.e. a C (int) cast. */
    *view->phase = (int32)((float)view->deltaMs * 1000.0f * local_28) + *view->phase;

    /* gameplay-fix(wpn_melee_ads_lean): fild phase * float const in extended precision, stored as a double for sin (0x080f1401) */
    d = sin((double)((long double)*view->phase * (long double)0.0005f));
    out[2] = out[2] + local_14 * (float)d * 0.01f;
    d = sin((double)((long double)*view->phase * (long double)0.0007f));
    out[1] = out[1] + local_14 * (float)d * 0.01f;
    d = sin((double)((long double)*view->phase * (long double)0.001f));
    out[0] = out[0] + local_14 * (float)d * 0.01f;
}
