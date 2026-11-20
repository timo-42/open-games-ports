/*
 * Chunk 040: cod2_lnxded 1.3, 0x080dafa6..0x080df948 (80 functions).
 *
 * Coverage: player animation parameters, view sway, the multiplayer animation
 * slot setup, the jump start/push code, the mantle probe/start/move code, the
 * BG trajectory evaluators, the item table lookups, and the dvar registration
 * for the player, jump and mantle tunables.
 *
 * Naming evidence is given per function in the provenance comments. Names
 * without string evidence are descriptive (naming=unknown or inferred).
 * Out-of-chunk callees use fn_08xxxxxx placeholders (see chunk_040_types.h).
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
/* cod2_prototypes.h (generated) also lists the 132 PLT import thunks of chunk 000 without extern "C";
 * they must not clash with the system headers, so rename them while the shared prototypes are read. */
#define COD2_PLT_SHIM_BEGIN
/* cod2_prototypes.h is generated from the chunk JSONs; until it is regenerated it still names this chunk's
 * pre-canonical tags. Map them onto the canonical structs while it is read. */
#define PlayerState playerState_s
#define PmoveCtx pmove_s
#define JumpMoveOut pml_s
#define UserCmd040 pml_s
#define Trace040 trace_s
#define BGEntity entityState_s
#define BGItem gitem_s
#define WeaponDef040 weaponDef_s
#define PlayerViewState clientInfo_s
#define Trajectory trajectory_s
#define cvar_s dvar_s
#define Vec3_Length_080dc54a cod2_proto_Vec3_Length_080dc54a /* prototype still says void; defined as float here */
#include "cod2_prototypes.h"
#undef PlayerState
#undef PmoveCtx
#undef JumpMoveOut
#undef UserCmd040
#undef Trace040
#undef BGEntity
#undef BGItem
#undef WeaponDef040
#undef PlayerViewState
#undef Trajectory
#undef cvar_s
#undef Vec3_Length_080dc54a
#include "chunk_040_types.h"

/* ---------------------------------------------------------------------------
 * Globals owned by this chunk (declared in chunk_040_types.h).
 * ------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------
 * Local helpers. Not part of the binary; they only express the raw-word and
 * ROM accesses used by this chunk.
 * ------------------------------------------------------------------------- */

/* The original copies vec3 words with integer moves (mov eax,[src]; mov [dst],eax), which keeps NaN payloads;
 * a float assignment would go through the x87 stack and quiet signalling NaNs. */
static inline void cod2_copy_word(float *dst, const float *src)
{
    *(volatile unsigned int *)dst = *(const volatile unsigned int *)src;
}

/* Raw bit pattern of a float (the undefined4 argument passed through as-is). */
static inline unsigned int cod2_bits_of(float f)
{
    unsigned int bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

/* NaN test as used by the NAN(x) branches of the sway code. */
static inline int cod2_isnanf(float x)
{
    return x != x;
}

/* Reads a 16-bit word from the read-only image at a fixed address (table at 0x08184ca8). */
#define COD2_ROM_U16(addr) (*(const unsigned short *)(unsigned long)(addr))

/* Dispatch through the function-pointer slots of the global block. */
typedef unsigned int (*FnHandleFromPtr)(void *);
typedef void (*FnBuildAnimList)(unsigned int *, unsigned short, unsigned int, int, void *);
typedef void (*FnReleaseAnim)(unsigned int);
#define COD2_CALL_HANDLE(p) (((FnHandleFromPtr)(unsigned long)COD2_G_U32(COD2_G_FN_HANDLE))(p))
#define COD2_CALL_BUILD(list, count, tree, client, vs) \
    (((FnBuildAnimList)(unsigned long)COD2_G_U32(COD2_G_FN_BUILD))((list), (count), (tree), (client), (vs)))
#define COD2_CALL_RELEASE(handle) \
    (((FnReleaseAnim)(unsigned long)COD2_G_U32(COD2_G_FN_RELEASE))(handle))

/* Slot record in the global block (0x60 bytes per slot). */
#define COD2_SLOT_RECORD(slot) ((SlotRecord040 *)(g_gameContextPtr + (unsigned int)(slot) * 0x60u))

/* ---------------------------------------------------------------------------
 * Forward declarations for this chunk's functions (address order).
 * ------------------------------------------------------------------------- */
void PlayerAnim_SetMoveParams(entityState_s *es, clientInfo_s *vs);
void PlayerView_ComputeSway(unsigned int unusedArg0, entityState_s *es, unsigned int unusedArg2,
                            clientInfo_s *vs, float *out);
void Vec3_StepToward(const float *target, float maxStep, float *cur);
void Vec3_ApproachLimited(const float *target, float maxDist, float *cur);
void PlayerView_ApplySwayBlend(unsigned int h1, entityState_s *es, unsigned int h3, clientInfo_s *vs, int scale);
void PlayerView_Update(unsigned int unusedHandle, entityState_s *es, clientInfo_s *vs);
void PlayerAnim_UpdateTree(int forceRebuild, entityState_s *es, clientInfo_s *vs, int flagArg);
void Player_RegisterMpAnimSlots(void);
int *Player_LookupAnimTree(int *out, const char *name, int required);
void Player_InitMpAnimCache(void);
void Player_LoadMultiplayerAnims(void);
long double Float_Abs_080dc354(float v);
long double Math_InvSqrtApprox(float x);
void Math_SinCos_080dc3d2(float angle, float *s, float *c);
void Vec3_Zero(float *v);
void Vec3_Set_080dc42a(float *v, float x, float y, float z);
void Vec3_Copy_080dc44e(const float *src, float *dst);
void Vec3_Sub_080dc47e(const float *a, const float *b, float *out);
void Vec3_MA_080dc4c2(const float *a, float s, const float *b, float *out);
long double Vec3_LengthSq_080dc516(const float *v);
float Vec3_Length_080dc54a(const float *v);
long double Math_SqrtF_080dc588(float x);
unsigned int BitSet_Test(const unsigned int *bits, int idx);
void BitSet_Set(unsigned int *bits, int idx);
void Player_RegisterJumpDvars(void);
void Player_ClearJumpBaseZ(playerState_s *ps);
int Player_CheckStepUp(const playerState_s *ps, const float *pos, float *stepOut);
int Player_IsAboveJumpBase(const playerState_s *ps);
void Player_SetJumpPending(playerState_s *ps);
void Player_UpdateJumpPhase(playerState_s *ps);
long double Player_JumpSpeedFactor(const playerState_s *ps);
long double Player_JumpScale(playerState_s *ps);
void Player_ClampJumpVelocity(playerState_s *ps, const float *pos);
long double Player_JumpSpeedFactorB(const playerState_s *ps);
void Player_BeginJump(pmove_s *pm, pml_s *out, float jumpHeight);
void Player_ApplyJumpPush(playerState_s *ps, const float *dir);
void Player_PlayJumpAnim(playerState_s *ps, void *ent);
int Player_TryJump(pmove_s *pm, pml_s *out);
long double Float_Sqrt_080dce30(float x);
void Vec2_Scale_080dce50(const float *in, float s, float *out);
void Vec3_Set_080dce76(float *v, float x, float y, float z);
void Vec3_CopyBits_080dce9a(const float *src, float *dst);
void Vec3_MA_080dcf02(const float *a, float s, const float *b, float *out);
void Vec3_Scale_080dceca(const float *in, float s, float *out);
long double Vec3_Dot_080dcf56(const float *a, const float *b);
void Mantle_RegisterDvars(void);
void Mantle_OnAnimEvent(unsigned int handle);
void Mantle_DebugPrint(const char *msg);
int Mantle_UpAnimDuration(const MantleState *m);
int Mantle_OverAnimDuration(const MantleState *m);
int Mantle_SelectAnim(const MantleState *m);
void Mantle_EvalStagePos(MantleState *m, int time, float *out);
int Mantle_FindNearestStage(float a, float b);
void Mantle_ComputeLedgeTarget(pmove_s *pm, MantleLedge *ledge);
void Mantle_Start(pmove_s *pm, playerState_s *ps, MantleLedge *ledge);
int Mantle_CheckLedge(pmove_s *pm, const float *eye, MantleLedge *ledge, float probeDist);
void Mantle_ValidateAnims(unsigned int ent);
void Mantle_ResetTree(void);
int Mantle_FindSurface(pmove_s *pm, const float *eye, trace_s *tr, float *normalOut);
void Mantle_TryStart(pmove_s *pm, struct pml_s *pml);
void Mantle_UpdateMove(pmove_s *pm, playerState_s *ps, pml_s *cmd);
void Mantle_ApplyYawCap(playerState_s *ps);
void Mantle_ClearSurfaceFlag(playerState_s *ps);
int Mantle_OverAnimNotLow(const playerState_s *ps);
void Vec3_Set_080de654(float *v, float x, float y, float z);
void Vec3_CopyBits_080de678(const float *src, float *dst);
void Vec3_NegateBits(const unsigned int *in, unsigned int *out);
void Vec3_Add_080de6e6(const float *a, const float *b, float *out);
void Vec3_Sub_080de72a(const float *a, const float *b, float *out);
void Vec3_ScaleAlt(const float *in, float s, float *out);
void Vec3_MA_080de7a6(const float *a, float s, const float *b, float *out);
long double Vec3_DotAlt(const float *a, const float *b);
void Player_RegisterDvars(void);
int BG_PlayerNearEntityTrajectory(const playerState_s *ps, const void *ent, int atTime);
int BG_CanItemBeGrabbed(const entityState_s *ent, const playerState_s *ps, int checkOnly);
void BG_EvaluateTrajectory(const trajectory_s *tr, int atTime, float *result);
void BG_EvaluateTrajectoryDelta(const trajectory_s *tr, int atTime, float *result);

// 0x080dafa6  size=955  FUN_080dafa6  status=translated  naming=inferred  name=PlayerAnim_SetMoveParams
// Naming: sets nine anim-tree parameters from the weapon anim tracks, pmFlags
// bits and forward velocity; no string evidence. Calls BG_GetWeaponDef (weapon table).
void PlayerAnim_SetMoveParams(entityState_s *es, clientInfo_s *vs)
{
    weaponDef_s *weapon;
    unsigned int clientSlot;
    SlotRecord040 *rec;
    int cur;

    weapon = BG_GetWeaponDef((int)es->weapon);
    BG_UpdateConditionValue(es->clientNum, 0, weapon->playerAnimType, 1);
    BG_UpdateConditionValue(es->clientNum, 1, weapon->weaponClass, 1);
    BG_UpdateConditionValue(es->clientNum, 7, (es->eFlags & 0x40000u) != 0, 1);
    BG_UpdateConditionValue(es->clientNum, 2, (es->eFlags & 0x300u) != 0, 1);
    BG_UpdateConditionValue(es->clientNum, 4, 0.0f < vs->velocity3e8[0], 1);
    BG_UpdateConditionValue(es->clientNum, 5, (es->eFlags & 4u) != 0, 1);
    BG_UpdateConditionValue(es->clientNum, 6, (es->eFlags & 0x40u) != 0, 1);

    clientSlot = (unsigned int)es->legsAnim & 0xfffffdffu;
    rec = COD2_SLOT_RECORD(clientSlot);
    if (rec->value54 != 0 || rec->value58 != 0) {
        cur = (int)BG_GetConditionValue(vs, 3, 0);
        rec = COD2_SLOT_RECORD(clientSlot);
        if (cur != rec->value54 || (cur >> 31) != rec->value58) {
            BG_UpdateConditionValue(es->clientNum, 3, rec->value54, 0);
        }
    }
    rec = COD2_SLOT_RECORD(clientSlot);
    if ((rec->flags50 & 0x10u) == 0) {
        if ((rec->flags50 & 0x20u) == 0) {
            BG_UpdateConditionValue(es->clientNum, 8, 0, 1);
        } else {
            BG_UpdateConditionValue(es->clientNum, 8, 2, 1);
        }
    } else {
        BG_UpdateConditionValue(es->clientNum, 8, 1, 1);
    }
}

// 0x080db362  size=1946  FUN_080db362  status=translated  naming=inferred  name=PlayerView_ComputeSway
// Naming: builds six sway vectors plus two offset vectors (0x60 bytes) from the
// lean/sway/velocity state and the view pitch/yaw of the player state; no string
// evidence. Zero output when pmFlags has 0x300 set.
// Stack layout: the decompiler's local_5c..local_20 are one contiguous float run
// of 18 values (six vec3). They are kept here as lane[18] in that order.
void PlayerView_ComputeSway(unsigned int unusedArg0, entityState_s *es, unsigned int unusedArg2,
                            clientInfo_s *vs, float *out)
{
    float a[3];      /* local_6c, local_68, local_64: position offset */
    float b[3];      /* local_9c, local_98, local_94: sway/velocity vector */
    float v[3];      /* local_ac, local_a8, local_a4: copy of move velocity */
    float c[3];      /* local_7c, local_78, local_74: view offset */
    float lane[18];  /* local_5c[0..4], local_48, local_44, ..., local_20[0..2] */
    volatile float speed; /* local_80: stored to the stack as a float (fstp [ebp-0x7c]) */
    float sinA;      /* local_b4 */
    float cosA;      /* local_b8 */
    unsigned int treeFlags;
    int i;

    if ((es->eFlags & 0x300u) != 0) {
        memset(out, 0, 0x60);
        return;
    }

    Vec3_Zero(a);
    Vec3_Zero(b);
    Vec3_Copy_080dc44e(vs->velocity3e8, v);
    a[1] = vs->lean380[0];
    b[1] = vs->sway3b0[0];
    treeFlags = BG_GetConditionValue(vs, 3, 0);
    if (((treeFlags & 0xc0000u) == 0) && (b[0] = vs->sway3b0[2], (es->eFlags & 8u) != 0)) {
        b[0] = AngleNormalize180(b[0]);
        if (0.0f < b[0]) {
            b[0] = b[0] * 0.5f;
        } else {
            b[0] = b[0] * 0.25f;
        }
    }
    AnglesSubtract(v, b, v);
    AnglesSubtract(b, a, b);
    Vec3_Set_080dc42a(c, 0.0f, 0.0f, es->fTorsoHeight);

    speed = Ease_OutQuadSigned(vs->speedScale3e4); /* the original passes the raw word */
    b[2] = speed * 50.0f * 0.925f;
    v[2] = speed * 50.0f * 0.925f;
    if (speed != 0.0f || cod2_isnanf(speed)) {
        if ((es->eFlags & 4u) == 0) {
            if (0.0f < speed) {
                c[1] = c[1] + -speed * 2.5f;
            } else {
                c[1] = c[1] + -speed * 5.0f;
            }
        } else if (0.0f < speed) {
            c[1] = c[1] + -speed * 2.5f;
        } else {
            c[1] = c[1] + -speed * 12.5f;
        }
    }
    if ((es->eFlags & 0x20000u) == 0) {
        a[1] = AngleSubtract(a[1], vs->velocity3e8[1]);
    }

    if ((es->eFlags & 8u) == 0) {
        /* Branch A: pmFlags bit 0x8 clear. */
        if (speed != 0.0f || cod2_isnanf(speed)) {
            if ((es->eFlags & 4u) == 0) {
                if (0.0f < speed) {
                    b[2] = b[2] * 1.25f;
                    v[2] = v[2] * 1.25f;
                } else {
                    b[2] = b[2] * 1.25f;
                    v[2] = v[2] * 1.25f;
                }
            } else if (speed <= 0.0f) {
                b[2] = b[2] * 1.25f;
                v[2] = v[2] * 1.25f;
            }
        }
        a[2] = a[2] + speed * 50.0f * 0.075f;
        lane[0] = b[0] * 0.2f;
        lane[1] = b[1] * 0.4f;
        lane[2] = b[2] * 0.5f;
        if (((es->fTorsoPitch != 0.0f) || cod2_isnanf(es->fTorsoPitch)) ||
            ((es->fWaistPitch != 0.0f) || cod2_isnanf(es->fWaistPitch))) {
            lane[0] = lane[0] + AngleSubtract(es->fTorsoPitch, es->fWaistPitch);
        }
        lane[3] = b[0] * 0.3f;
        lane[4] = b[1] * 0.4f;
        lane[5] = b[2] * 0.5f;
        lane[6] = b[0] * 0.5f;
        lane[7] = b[1] * 0.2f;
        lane[8] = b[2] * -0.6f;
    } else {
        /* Branch B: pmFlags bit 0x8 set. */
        if (speed != 0.0f || cod2_isnanf(speed)) {
            v[2] = v[2] * 0.5f;
        }
        a[0] = a[0] + es->fTorsoPitch;
        Math_SinCos_080dc3d2((float)(b[1] * 0.017453292519943295), &sinA, &cosA);
        c[0] = c[0] + (1.0f - cosA) * -24.0f;
        c[1] = c[1] + sinA * -12.0f;
        if (0.0f < speed * sinA) {
            c[1] = c[1] + (1.0f - cosA) * -speed * 16.0f;
        }
        lane[0] = 0.0f;
        lane[1] = b[2] * -1.2f;
        lane[2] = b[2] * 0.3f;
        if (((es->fTorsoPitch != 0.0f) || cod2_isnanf(es->fTorsoPitch)) ||
            ((es->fWaistPitch != 0.0f) || cod2_isnanf(es->fWaistPitch))) {
            lane[0] = lane[0] + AngleSubtract(es->fTorsoPitch, es->fWaistPitch);
        }
        lane[3] = 0.0f;
        lane[4] = b[1] * 0.1f - b[2] * 0.2f;
        lane[5] = b[2] * 0.2f;
        lane[6] = b[0];
        lane[7] = b[1] * 0.8f + b[2];
        lane[8] = b[2] * -0.2f;
    }

    lane[9] = v[0] * 0.3f;
    lane[10] = v[1] * 0.3f;
    lane[11] = 0.0f;
    lane[12] = v[0] * 0.7f;
    lane[13] = v[1] * 0.7f;
    lane[14] = v[2] * -0.3f;
    lane[15] = 0.0f;
    lane[16] = 0.0f;
    lane[17] = 0.0f;
    if (((es->fWaistPitch != 0.0f) || cod2_isnanf(es->fWaistPitch)) ||
        ((es->fTorsoPitch != 0.0f) || cod2_isnanf(es->fTorsoPitch))) {
        lane[15] = AngleSubtract(es->fWaistPitch, es->fTorsoPitch);
    }

    for (i = 0; i < 6; i++) {
        Vec3_Copy_080dc44e(&lane[i * 3], out + i * 3);
    }
    Vec3_Copy_080dc44e(a, out + 18);
    Vec3_Copy_080dc44e(c, out + 21);
}

// 0x080dbafc  size=224  FUN_080dbafc  status=translated  naming=unknown  name=Vec3_StepToward
// Naming: per-component step of cur toward target by at most maxStep; exact copy
// of target when the difference is within the step. No string evidence.
void Vec3_StepToward(const float *target, float maxStep, float *cur)
{
    int i;
    volatile float diff; /* the original stores diff to the stack as a float (fstp [ebp-8]) */

    for (i = 0; i < 3; i++) {
        diff = target[i] - cur[i];
        if (maxStep < diff) {
            cur[i] = cur[i] + maxStep;
        } else if (diff < -maxStep) {
            cur[i] = cur[i] - maxStep;
        } else {
            cod2_copy_word(&cur[i], &target[i]);
        }
    }
}

// 0x080dbbdc  size=148  FUN_080dbbdc  status=translated  naming=unknown  name=Vec3_ApproachLimited
// Naming: moves cur toward target along the connecting line by at most maxDist
// (normalised step scaled by 1/|delta|); copies target when it is within reach.
// No string evidence.
void Vec3_ApproachLimited(const float *target, float maxDist, float *cur)
{
    float delta[3];
    long double lenSq;
    long double inv;
    float scale;

    Vec3_Sub_080dc47e(target, cur, delta);
    lenSq = Vec3_LengthSq_080dc516(delta);
    if ((float)lenSq != 0.0f) {
        inv = Math_InvSqrtApprox((float)lenSq);
        scale = (float)((long double)maxDist * inv);
        if (scale < 1.0) {
            Vec3_MA_080dc4c2(cur, scale, delta, cur);
        } else {
            Vec3_Copy_080dc44e(target, cur);
        }
    }
}

/* .data table of 6 pointers to the bone-name words (0x085328c8 + 2*i in the original image). */
extern unsigned short *g_08184ca8[6];

// 0x080dbc70  size=332  FUN_080dbc70  status=translated  naming=inferred  name=PlayerView_ApplySwayBlend
// Naming: computes the sway block for the player, steps six per-bone offsets and
// the two view offsets toward it, then applies the blended result to the bones.
// No string evidence. Bone indices are read from the image table at 0x08184ca8.
void PlayerView_ApplySwayBlend(unsigned int h1, entityState_s *es, unsigned int h3, clientInfo_s *vs, int scale)
{
    float sway[24];   /* six per-bone vec3 (0x48 bytes) followed by two vec3 (0x0c each) */
    float stepScale;
    int i;

    stepScale = (float)scale * 0.36;
    PlayerView_ComputeSway(h1, es, h3, vs, sway);
    for (i = 0; i < 6; i++) {
        Vec3_StepToward(&sway[i * 3], stepScale, COD2_F32_AT(vs, 0x3fc + i * 0xc));
        DObj_SetControlBoneByName((struct DObj *)h1, (const uint32 *)h3, *g_08184ca8[i], COD2_F32_AT(vs, 0x3fc + i * 0xc));
    }
    Vec3_StepToward(&sway[18], stepScale, COD2_F32_AT(vs, 0x444));
    Vec3_ApproachLimited(&sway[21], (float)scale * 0.1, COD2_F32_AT(vs, 0x450));
    DObj_SetBoneByName((struct DObj *)h1, (const uint32 *)h3, scrStr_tag_origin, COD2_F32_AT(vs, 0x450), COD2_F32_AT(vs, 0x444));
}

// 0x080dbdbc  size=244  FUN_080dbdbc  status=translated  naming=inferred  name=PlayerView_Update
// Naming: per-frame view/animation update: runs the view helpers, the move
// parameters, refreshes two anim-tree vectors and clears the landing latch.
// No string evidence.
void PlayerView_Update(unsigned int unusedHandle, entityState_s *es, clientInfo_s *vs)
{
    unsigned int tree;

    BG_PlayerAngles(es, vs);
    PlayerAnim_SetMoveParams(es, vs);
    tree = vs->animTree4a4;
    BG_ClearFinishedAnimation((void *)tree, (struct playerAnimLerp_t *)vs->lean380);
    BG_ClearFinishedAnimation((void *)tree, (struct playerAnimLerp_t *)vs->sway3b0);
    if (vs->landingTimer3f4 != 0 && (vs->flags3c0 & 0xfffffdffu) == 0) {
        vs->landingTimer3f4 = 0;
        vs->landingLatch3f8 = 1;
    }
    BG_RunLerpFrame(vs, (struct playerAnimLerp_t *)vs->lean380, es->legsAnim, es);
    BG_RunLerpFrame(vs, (struct playerAnimLerp_t *)vs->sway3b0, es->torsoAnim, es);
}

// 0x080dbeb0  size=562  FUN_080dbeb0  status=translated  naming=inferred  name=PlayerAnim_UpdateTree
// Naming: rebuilds the per-player animation list (up to six entries) through the
// global block's build callback when the weapon/anim state changes, and releases
// the handle through the release callback otherwise. No string evidence.
void PlayerAnim_UpdateTree(int forceRebuild, entityState_s *es, clientInfo_s *vs, int flagArg)
{
    unsigned int weaponSlot;
    unsigned int treeHandle;
    unsigned int list[25];
    unsigned int count;
    unsigned int uVar1;
    int i;

    weaponSlot = (unsigned int)es->weapon;
    if ((es->eFlags & 0x300u) != 0) {
        weaponSlot = 0;
    }
    treeHandle = vs->animTree4a4;

    if (((int *)vs)[0] == 0 || ((unsigned char *)vs)[0x40] == 0) {
        XAnim_ResetTree((struct XAnimTree *)treeHandle);
        COD2_CALL_RELEASE((unsigned int)es->number);
        return;
    }

    if (forceRebuild != 0) {
        if (vs->animSlot4a8 == weaponSlot && vs->landingLatch3f8 == 0) {
            return;
        }
        COD2_CALL_RELEASE((unsigned int)es->number);
    }

    list[0] = COD2_CALL_HANDLE((unsigned char *)vs + 0x40);
    list[1] = 0;
    list[2] = 0;
    count = 1;
    for (i = 0; i < 6; i++) {
        if (((unsigned char *)vs)[0x80 + i * 0x40] != 0) {
            uVar1 = COD2_CALL_HANDLE((unsigned char *)vs + 0x80 + i * 0x40);
            list[count * 3] = uVar1;
            list[count * 3 + 1] = (unsigned int)((unsigned char *)vs + 0x200 + i * 0x40);
            list[count * 3 + 2] = (unsigned int)((flagArg >> (i & 0x1f)) & 1);
            count++;
        }
    }
    vs->animSlot4a8 = weaponSlot;
    COD2_CALL_BUILD(list, (unsigned short)count, treeHandle, es->number, vs);
    vs->landingLatch3f8 = 0;
}

// 0x080dc0e2  size=204  FUN_080dc0e2  status=translated  naming=inferred  name=Player_RegisterMpAnimSlots
// Naming: registers the four "multiplayer" anim slots (root, torso, legs,
// turning) through the anim-tree lookup. Strings "multiplayer", "root", "torso",
// "legs", "turning" are the lookup names (observed).
void Player_RegisterMpAnimSlots(void)
{
    Scr_AddAnimTreeEntry("multiplayer", "root", (unsigned int)(g_gameContextPtr + COD2_G_MP_ROOT_SLOT),
                COD2_G_U32(COD2_G_MP_FLAGS));
    Scr_AddAnimTreeEntry("multiplayer", "torso", (unsigned int)(g_gameContextPtr + COD2_G_MP_TORSO_SLOT),
                COD2_G_U32(COD2_G_MP_FLAGS));
    Scr_AddAnimTreeEntry("multiplayer", "legs", (unsigned int)(g_gameContextPtr + COD2_G_MP_LEGS_SLOT),
                COD2_G_U32(COD2_G_MP_FLAGS));
    Scr_AddAnimTreeEntry("multiplayer", "turning", (unsigned int)(g_gameContextPtr + COD2_G_MP_TURN_SLOT),
                COD2_G_U32(COD2_G_MP_FLAGS));
}

// 0x080dc1ae  size=83  FUN_080dc1ae  status=translated  naming=observed  name=Player_LookupAnimTree
// Naming: observed from the message "Could not find animation tree '%s'".
int *Player_LookupAnimTree(int *out, const char *name, int required)
{
    ScrAnimTreeRef tree;

    Scr_GetAnimTreeValue(&tree, name);
    *out = tree.value;
    if (*out == 0 && required != 0) {
        /* The leading 0x15 byte is part of the image string at 0x081532a0. */
        Com_Error(1, "\x15" "Could not find animation tree '%s'", name);
    }
    return out;
}

// 0x080dc202  size=160  FUN_080dc202  status=translated  naming=inferred  name=Player_InitMpAnimCache
// Naming: loads the "multiplayer" tree handle and copies it and the three
// 16-bit anim indices into the cache slots. "multiplayer" is observed.
void Player_InitMpAnimCache(void)
{
    int tree;

    Player_LookupAnimTree(&tree, "multiplayer", 1);
    COD2_G_U32(COD2_G_MP_TREE) = (unsigned int)tree;
    COD2_G_U32(COD2_G_MP_TREE_COPY) = COD2_G_U32(COD2_G_MP_TREE);
    COD2_G_U16(COD2_G_MP_TORSO_COPY) = COD2_G_U16(COD2_G_MP_TORSO_SLOT);
    COD2_G_U16(COD2_G_MP_LEGS_COPY) = COD2_G_U16(COD2_G_MP_LEGS_SLOT);
    COD2_G_U16(COD2_G_MP_TURN_COPY) = COD2_G_U16(COD2_G_MP_TURN_SLOT);
}

// 0x080dc2a2  size=130  FUN_080dc2a2  status=translated  naming=inferred  name=Player_LoadMultiplayerAnims
// Naming: builds the multiplayer anim setup inside a scratch buffer; the buffer is
// released on both the normal and exception path (catch handler at 0x080dc324).
void Player_LoadMultiplayerAnims(void)
{
    unsigned char scratch[28];
    unsigned int handle;
    unsigned int out2[2];

    Com_PoolAllocInto2((int *)scratch, 0x9000);
    try {
        handle = (unsigned int)Com_PoolPtrFromOffset((int *)scratch);
        out2[0] = 0;
        Player_RegisterMpAnimSlots();
        BG_AnimParseAnimScript((struct animScriptData_t *)g_gameContextPtr, (struct animNameEntry_t *)handle, (int *)out2);
        Scr_LoadAnimTrees(COD2_G_U32(COD2_G_MP_SET), COD2_G_U32(COD2_G_MP_FLAGS));
        Player_InitMpAnimCache();
        Scr_ShutdownAnimTrees();
        BG_LoadAnim();
    } catch (...) {
        Com_PoolRestoreFrom2((int *)scratch);
        throw;
    }
    Com_PoolRestoreFrom2((int *)scratch);
}

// 0x080dc354  size=25  FUN_080dc354  status=translated  naming=unknown  name=Float_Abs_080dc354
long double Float_Abs_080dc354(float v)
{
    volatile float r = __builtin_fabsf(v); /* fabs, then stored to the stack as a float */
    return (long double)r;
}

// 0x080dc36e  size=99  FUN_080dc36e  status=translated  naming=inferred  name=Math_InvSqrtApprox
// Naming: the 0x5f3759df magic is the classic reciprocal square root estimate
// with one Newton step. The float argument is reinterpreted as its bit pattern.
long double Math_InvSqrtApprox(float x)
{
    float half;
    unsigned int bits;
    int i;
    float y;
    volatile float r;

    half = x * 0.5f;
    memcpy(&bits, &x, sizeof(bits));
    i = (int)bits;
    i = 0x5f3759df - (i >> 1);
    memcpy(&y, &i, sizeof(y));
    r = y * (1.5f - half * y * y); /* stored as a float on the stack and reloaded in the original */
    return (long double)r;
}

// 0x080dc3d2  size=45  FUN_080dc3d2  status=translated  naming=unknown  name=Math_SinCos_080dc3d2
void Math_SinCos_080dc3d2(float angle, float *s, float *c)
{
    double dv;

    dv = sin((double)angle);
    *s = (float)dv;
    dv = cos((double)angle);
    *c = (float)dv;
}

// 0x080dc400  size=41  FUN_080dc400  status=translated  naming=unknown  name=Vec3_Zero
void Vec3_Zero(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x080dc42a  size=35  FUN_080dc42a  status=translated  naming=unknown  name=Vec3_Set_080dc42a
void Vec3_Set_080dc42a(float *v, float x, float y, float z)
{
    cod2_copy_word(&v[0], &x);
    cod2_copy_word(&v[1], &y);
    cod2_copy_word(&v[2], &z);
}

// 0x080dc44e  size=47  FUN_080dc44e  status=translated  naming=unknown  name=Vec3_Copy_080dc44e
void Vec3_Copy_080dc44e(const float *src, float *dst)
{
    cod2_copy_word(&dst[0], &src[0]);
    cod2_copy_word(&dst[1], &src[1]);
    cod2_copy_word(&dst[2], &src[2]);
}

// 0x080dc47e  size=68  FUN_080dc47e  status=translated  naming=unknown  name=Vec3_Sub_080dc47e
void Vec3_Sub_080dc47e(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080dc4c2  size=83  FUN_080dc4c2  status=translated  naming=unknown  name=Vec3_MA_080dc4c2
// out = a + s * b
void Vec3_MA_080dc4c2(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x080dc516  size=51  FUN_080dc516  status=translated  naming=unknown  name=Vec3_LengthSq_080dc516
// Returns the squared length in extended precision (x87 return in st0).
long double Vec3_LengthSq_080dc516(const float *v)
{
    return (long double)v[0] * (long double)v[0] +
           (long double)v[1] * (long double)v[1] +
           (long double)v[2] * (long double)v[2];
}

// 0x080dc54a  size=62  FUN_080dc54a  status=translated  naming=unknown  name=Vec3_Length_080dc54a
// Computes the squared length of v and returns Math_SqrtF_080dc588 of it (st0 is left untouched by the
// epilogue, so the float result is the return value; chunk 039 reads it as the anim move speed).
float Vec3_Length_080dc54a(const float *v)
{
    volatile float sq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2]; /* argument slot is a float (fstp [esp]) */

    return (float)Math_SqrtF_080dc588(sq);
}

// 0x080dc588  size=31  FUN_080dc588  status=translated  naming=unknown  name=Math_SqrtF_080dc588
long double Math_SqrtF_080dc588(float x)
{
    double dv;

    dv = sqrt((double)x);
    return (long double)(float)dv;
}

// 0x080dc5a8  size=38  FUN_080dc5a8  status=translated  naming=unknown  name=BitSet_Test
unsigned int BitSet_Test(const unsigned int *bits, int idx)
{
    return (bits[idx >> 5] >> ((unsigned int)idx & 0x1f)) & 1u;
}

// 0x080dc5ce  size=62  FUN_080dc5ce  status=translated  naming=unknown  name=BitSet_Set
void BitSet_Set(unsigned int *bits, int idx)
{
    bits[idx >> 5] = (1u << ((unsigned int)idx & 0x1f)) | bits[idx >> 5];
}

// 0x080dc60c  size=268  FUN_080dc60c  status=translated  naming=observed  name=Player_RegisterJumpDvars
// Strings: jump_height, jump_stepSize, jump_slowdownEnable, jump_ladderPushVel,
// jump_spreadAdd (observed registration names).
void Player_RegisterJumpDvars(void)
{
    dvar_jump_height = Dvar_RegisterFloat("jump_height", 39.0f, 0.0f, 128.0f, 0x1180u);
    dvar_jump_stepSize = Dvar_RegisterFloat("jump_stepSize", 18.0f, 0.0f, 64.0f, 0x1180u);
    dvar_jump_slowdownEnable = Dvar_RegisterBool("jump_slowdownEnable", 1, 0x1180u);
    dvar_jump_ladderPushVel = Dvar_RegisterFloat("jump_ladderPushVel", 128.0f, 0.0f, 1024.0f, 0x1180u);
    dvar_jump_spreadAdd = Dvar_RegisterFloat("jump_spreadAdd", 64.0f, 0.0f, 512.0f, 0x1180u);
}

// 0x080dc718  size=33  FUN_080dc718  status=translated  naming=inferred  name=Player_ClearJumpBaseZ
// Clears the ground-contact bit 0x80000 and zeroes the jump base height.
void Player_ClearJumpBaseZ(playerState_s *ps)
{
    ps->pm_flags = ps->pm_flags & 0xfff7ffffu;
    ps->jumpOriginZ = 0.0f;
}

// 0x080dc73a  size=141  FUN_080dc73a  status=translated  naming=inferred  name=Player_CheckStepUp
// Naming: step-up test against the jump base height using jump_height and
// jump_stepSize (both observed dvars). Returns 1 and writes the step height.
int Player_CheckStepUp(const playerState_s *ps, const float *pos, float *stepOut)
{
    if (pos[2] < ps->jumpOriginZ + dvar_jump_height->current.value) {
        *stepOut = dvar_jump_stepSize->current.value;
        if (ps->jumpOriginZ + dvar_jump_height->current.value < pos[2] + *stepOut) {
            *stepOut = (ps->jumpOriginZ + dvar_jump_height->current.value) - pos[2];
        }
        return 1;
    }
    return 0;
}

// 0x080dc7c8  size=37  FUN_080dc7c8  status=translated  naming=inferred  name=Player_IsAboveJumpBase
int Player_IsAboveJumpBase(const playerState_s *ps)
{
    return ps->jumpOriginZ + dvar_jump_height->current.value <= ps->origin[2];
}

// 0x080dc7ee  size=41  FUN_080dc7ee  status=translated  naming=inferred  name=Player_SetJumpPending
void Player_SetJumpPending(playerState_s *ps)
{
    if (ps->pm_time == 0) {
        ps->pm_flags = ps->pm_flags | 0x80000u;
        ps->pm_time = 0x708;
    }
}

// 0x080dc818  size=177  FUN_080dc818  status=translated  naming=inferred  name=Player_UpdateJumpPhase
// Phase machine for the jump (0x4b0, 0x708, 0x709 thresholds); scales the
// velocity by a per-phase factor, or by 1.0 when jump_slowdownEnable is off.
void Player_UpdateJumpPhase(playerState_s *ps)
{
    float scale;

    scale = 1.0f;
    if (ps->pm_time < 0x709) {
        if (ps->pm_time == 0) {
            if (ps->origin[2] < ps->jumpOriginZ + 18.0) {
                ps->pm_time = 0x708;
                scale = 0.65f;
            } else {
                ps->pm_time = 0x4b0;
                scale = 0.5f;
            }
        }
    } else {
        Player_ClearJumpBaseZ(ps);
        scale = 0.65f;
    }
    if (dvar_jump_slowdownEnable->current.enabled == 0) {
        scale = 1.0f;
    }
    Vec3_Scale_080dceca(ps->velocity, scale, ps->velocity);
}

// 0x080dc8ca  size=81  FUN_080dc8ca  status=translated  naming=inferred  name=Player_JumpSpeedFactor
long double Player_JumpSpeedFactor(const playerState_s *ps)
{
    float f;

    if (dvar_jump_slowdownEnable->current.enabled == 0) {
        f = 1.0f;
    } else if (ps->pm_time < 0x6a4) {
        f = (float)ps->pm_time * 1.5 * 0.00058823527 + 1.0;
    } else {
        f = 2.5f;
    }
    return (long double)f;
}

// 0x080dc91c  size=64  FUN_080dc91c  status=translated  naming=inferred  name=Player_JumpScale
long double Player_JumpScale(playerState_s *ps)
{
    float f;

    if (ps->pm_time < 0x709) {
        f = (float)Player_JumpSpeedFactor(ps);
    } else {
        Player_ClearJumpBaseZ(ps);
        f = 1.0f;
    }
    return (long double)f;
}

// 0x080dc95c  size=145  FUN_080dc95c  status=translated  naming=inferred  name=Player_ClampJumpVelocity
// Caps the vertical jump velocity so the apex stays at jump_height above the
// jump base; zeroes it when the remaining height is below 0.1.
void Player_ClampJumpVelocity(playerState_s *ps, const float *pos)
{
    float remain;
    long double root;

    if (0.0 < ps->origin[2] - pos[2]) {
        remain = (ps->jumpOriginZ + dvar_jump_height->current.value) - ps->origin[2];
        if (remain < 0.1) {
            ps->velocity[2] = 0.0f;
        } else {
            root = Float_Sqrt_080dce30((float)ps->gravity * (remain + remain));
            if ((float)root < ps->velocity[2]) {
                ps->velocity[2] = (float)root;
            }
        }
    }
}

// 0x080dc9ee  size=81  FUN_080dc9ee  status=translated  naming=unknown  name=Player_JumpSpeedFactorB
// Same body as Player_JumpSpeedFactor (separate function in the binary).
long double Player_JumpSpeedFactorB(const playerState_s *ps)
{
    return Player_JumpSpeedFactor(ps);
}

// 0x080dca40  size=264  FUN_080dca40  status=translated  naming=inferred  name=Player_BeginJump
// Starts a jump: initial vertical velocity sqrt(2*gravity*height), jump bookkeeping
// and the accumulated jump spread (jump_spreadAdd, capped at 255).
void Player_BeginJump(pmove_s *pm, pml_s *out, float jumpHeight)
{
    playerState_s *ps;
    float v;
    long double factor;

    ps = pm->ps;
    v = (float)ps->gravity * (jumpHeight + jumpHeight);
    if (((ps->pm_flags & 0x80000u) != 0) && (ps->pm_time < 0x709)) {
        factor = Player_JumpSpeedFactorB(ps);
        v = v / (float)factor;
    }
    out->groundPlane = 0;
    out->almostGroundPlane = 0;
    out->walking = 0;
    ps->groundEntityNum = 0x3ff;
    ps->jumpTime = pm->cmd.serverTime;
    ps->jumpOriginZ = ps->origin[2];
    ps->pm_flags = ps->pm_flags | 0x80000u;
    ps->pm_time = 0;
    ps->velocity[2] = (float)Math_SqrtF_080dc588(v);
    ps->aimSpreadScale = ps->aimSpreadScale + dvar_jump_spreadAdd->current.value;
    if (255.0 < ps->aimSpreadScale) {
        ps->aimSpreadScale = 255.0f;
    }
}

// 0x080dcb48  size=266  FUN_080dcb48  status=translated  naming=inferred  name=Player_ApplyJumpPush
// Ladder/wall push on jump: decays the vertical velocity, projects the move
// direction along the wall normal and applies jump_ladderPushVel to the planar
// velocity. Clears pmFlags2 bit 0x20.
void Player_ApplyJumpPush(playerState_s *ps, const float *dir)
{
    float dirFlat[3];
    float pushed[3];
    long double dot;

    ps->velocity[2] = ps->velocity[2] * 0.75;
    Vec3_Set_080dce76(dirFlat, dir[0], dir[1], 0.0f);
    Vec3_Normalize(dirFlat);
    dot = Vec3_Dot_080dcf56(ps->vLadderVec, dir);
    if ((float)dot < 0.0) {
        dot = Vec3_Dot_080dcf56(dirFlat, ps->vLadderVec);
        Vec3_MA_080dcf02(dirFlat, (float)((float)dot * -2.0), ps->vLadderVec, pushed);
        Vec3_Normalize(pushed);
    } else {
        Vec3_CopyBits_080dce9a(dirFlat, pushed);
    }
    Vec2_Scale_080dce50(pushed, dvar_jump_ladderPushVel->current.value, ps->velocity);
    ps->pm_flags = ps->pm_flags & 0xffffffdfu;
}

// 0x080dcc52  size=85  FUN_080dcc52  status=translated  naming=inferred  name=Player_PlayJumpAnim
// Chooses the jump anim (0x5b) or the entity-specific anim (lookup + 0x46).
void Player_PlayJumpAnim(playerState_s *ps, void *ent)
{
    int anim;

    if ((ps->pm_flags & 0x20u) == 0) {
        anim = PM_GroundSurfaceType((const pml_s *)ent);
        if (anim != 0) {
            PM_AddEvent(ps, anim + 0x46);
        }
    } else {
        PM_AddEvent(ps, 0x5b);
    }
}

// 0x080dcca8  size=391  FUN_080dcca8  status=translated  naming=inferred  name=Player_TryJump
// Jump request: gated by the 500 ms re-jump window, the stance/mantle state and
// the jump button (0x400, edge-triggered on oldButtons). Returns 1 when a jump
// started.
int Player_TryJump(pmove_s *pm, pml_s *out)
{
    playerState_s *ps;
    int blocked;

    ps = pm->ps;
    if (pm->cmd.serverTime - ps->jumpTime < 500) {
        return 0;
    }
    if ((ps->pm_flags & 0x1000u) != 0) {
        return 0;
    }
    if ((ps->pm_flags & 4u) != 0) {
        return 0;
    }
    if (!(ps->pm_type < 6)) {
        return 0;
    }
    blocked = PM_StanceIndex(ps);
    if (blocked != 0) {
        return 0;
    }
    if ((pm->cmd.buttons & 0x400u) == 0) {
        return 0;
    }
    if ((pm->oldcmd.buttons & 0x400u) != 0) {
        pm->cmd.buttons = pm->cmd.buttons & 0xfffffbffu;
        return 0;
    }
    Player_BeginJump(pm, out, dvar_jump_height->current.value);
    Player_PlayJumpAnim(ps, out);
    if ((ps->pm_flags & 0x20u) != 0) {
        Player_ApplyJumpPush(ps, out->forward);
    }
    if (pm->cmd.forwardmove < 0) {
        BG_AnimScriptEvent(ps, 4, 0, 1);
    } else {
        BG_AnimScriptEvent(ps, 3, 0, 1);
    }
    return 1;
}

// 0x080dce30  size=31  FUN_080dce30  status=translated  naming=unknown  name=Float_Sqrt_080dce30
// Second copy of the square root wrapper (same body as Math_SqrtF_080dc588).
long double Float_Sqrt_080dce30(float x)
{
    return Math_SqrtF_080dc588(x);
}

// 0x080dce50  size=37  FUN_080dce50  status=translated  naming=unknown  name=Vec2_Scale_080dce50
// Scales the x and y components only.
void Vec2_Scale_080dce50(const float *in, float s, float *out)
{
    out[0] = s * in[0];
    out[1] = s * in[1];
}

// 0x080dce76  size=35  FUN_080dce76  status=translated  naming=unknown  name=Vec3_Set_080dce76
void Vec3_Set_080dce76(float *v, float x, float y, float z)
{
    cod2_copy_word(&v[0], &x);
    cod2_copy_word(&v[1], &y);
    cod2_copy_word(&v[2], &z);
}

// 0x080dce9a  size=47  FUN_080dce9a  status=translated  naming=unknown  name=Vec3_CopyBits_080dce9a
void Vec3_CopyBits_080dce9a(const float *src, float *dst)
{
    cod2_copy_word(&dst[0], &src[0]);
    cod2_copy_word(&dst[1], &src[1]);
    cod2_copy_word(&dst[2], &src[2]);
}

// 0x080dceca  size=56  FUN_080dceca  status=translated  naming=unknown  name=Vec3_Scale_080dceca
void Vec3_Scale_080dceca(const float *in, float s, float *out)
{
    out[0] = s * in[0];
    out[1] = s * in[1];
    out[2] = s * in[2];
}

// 0x080dcf02  size=83  FUN_080dcf02  status=translated  naming=unknown  name=Vec3_MA_080dcf02
void Vec3_MA_080dcf02(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x080dcf56  size=51  FUN_080dcf56  status=translated  naming=unknown  name=Vec3_Dot_080dcf56
long double Vec3_Dot_080dcf56(const float *a, const float *b)
{
    return (long double)a[0] * (long double)b[0] +
           (long double)a[1] * (long double)b[1] +
           (long double)a[2] * (long double)b[2];
}

// 0x080dcf8c  size=336  FUN_080dcf8c  status=translated  naming=observed  name=Mantle_RegisterDvars
// Strings: mantle_enable, mantle_debug, mantle_check_range, mantle_check_radius,
// mantle_check_angle, mantle_view_yawcap (observed registration names).
void Mantle_RegisterDvars(void)
{
    dvar_mantle_enable = Dvar_RegisterBool("mantle_enable", 1, 0x1180u);
    dvar_mantle_debug = Dvar_RegisterBool("mantle_debug", 0, 0x1180u);
    dvar_mantle_check_range = Dvar_RegisterFloat("mantle_check_range", 20.0f, 0.0f, 128.0f, 0x1180u);
    dvar_mantle_check_radius = Dvar_RegisterFloat("mantle_check_radius", 0.1f, 0.0f, 15.0f, 0x1180u);
    dvar_mantle_check_angle = Dvar_RegisterFloat("mantle_check_angle", 60.0f, 0.0f, 180.0f, 0x1180u);
    dvar_mantle_view_yawcap = Dvar_RegisterFloat("mantle_view_yawcap", 60.0f, 0.0f, 180.0f, 0x1180u);
}

// 0x080dd0dc  size=27  FUN_080dd0dc  status=translated  naming=inferred  name=Mantle_OnAnimEvent
// Anim-event callback registered for each mantle animation name.
void Mantle_OnAnimEvent(unsigned int handle)
{
    Hunk_AllocAlign(handle, 4);
}

// 0x080dd0f8  size=40  FUN_080dd0f8  status=translated  naming=observed  name=Mantle_DebugPrint
// Prints through "%s\n" when the mantle_debug dvar is set. Mantle message strings
// are observed in the callers.
void Mantle_DebugPrint(const char *msg)
{
    if (dvar_mantle_debug->current.enabled != 0) {
        Com_Printf("%s\n", msg);
    }
}

// 0x080dd120  size=58  FUN_080dd120  status=translated  naming=inferred  name=Mantle_UpAnimDuration
// Duration of the stage's up animation in the mantle tree.
int Mantle_UpAnimDuration(const MantleState *m)
{
    return (int)XAnim_NodeDurationMs((struct XAnimNodeTable *)g_mantleAnimTree, (unsigned int)g_mantleStages[m->transIndex].upAnim);
}

// 0x080dd15a  size=88  FUN_080dd15a  status=translated  naming=inferred  name=Mantle_OverAnimDuration
// Duration of the stage's over animation; zero unless flags bit 0 is set.
int Mantle_OverAnimDuration(const MantleState *m)
{
    int dur;

    if ((m->flags & 1u) != 0) {
        dur = (int)XAnim_NodeDurationMs((struct XAnimNodeTable *)g_mantleAnimTree, (unsigned int)g_mantleStages[m->transIndex].overAnim);
    } else {
        dur = 0;
    }
    return dur;
}

// 0x080dd1b2  size=100  FUN_080dd1b2  status=translated  naming=inferred  name=Mantle_SelectAnim
// Returns the over animation once the stage time passes the up duration, else
// the up animation.
int Mantle_SelectAnim(const MantleState *m)
{
    int upDur;

    upDur = Mantle_UpAnimDuration(m);
    Mantle_OverAnimDuration(m);
    if (upDur < m->timer) {
        return g_mantleStages[m->transIndex].overAnim;
    }
    return g_mantleStages[m->transIndex].upAnim;
}

// 0x080dd216  size=326  FUN_080dd216  status=translated  naming=inferred  name=Mantle_EvalStagePos
// Evaluates the mantle position at a stage time: the up animation, then the over
// animation blended by the remaining time, offset by the stage yaw.
void Mantle_EvalStagePos(MantleState *m, int time, float *out)
{
    int upDur;
    int overDur;
    float scratchA[3];
    float upPos[3];
    const MantleStage *st;

    upDur = Mantle_UpAnimDuration(m);
    overDur = Mantle_OverAnimDuration(m);
    st = &g_mantleStages[m->transIndex];
    if (upDur < time) {
        XAnim_NodeBlendSampleB((struct XAnimNodeTable *)g_mantleAnimTree, (unsigned int)st->upAnim, scratchA, upPos, 1.0f);
        XAnim_NodeBlendSampleB((struct XAnimNodeTable *)g_mantleAnimTree, (unsigned int)st->overAnim, scratchA, out,
                    (float)(time - upDur) / (float)overDur);
        Vec3_Add_080de6e6(out, upPos, out);
    } else {
        XAnim_NodeBlendSampleB((struct XAnimNodeTable *)g_mantleAnimTree, (unsigned int)st->upAnim, scratchA, out, (float)time / (float)upDur);
    }
    Vec2_RotateDegrees(out, m->yaw);
}

// 0x080dd35c  size=128  FUN_080dd35c  status=translated  naming=inferred  name=Mantle_FindNearestStage
// Index of the stage whose height is closest to (b - a) (the ledge height step).
int Mantle_FindNearestStage(float a, float b)
{
    int best;
    float bestDist;
    float dist;
    volatile float d; /* b - a is stored to the stack as a float (fstp [ebp-4]) */
    int k;

    best = 0;
    d = b - a;
    bestDist = (float)Float_Abs_080de63a(g_mantleStages[0].zHeight - d);
    for (k = 1; k < 7; k++) {
        dist = (float)Float_Abs_080de63a(g_mantleStages[k].zHeight - d);
        if (dist < bestDist) {
            best = k;
            bestDist = dist;
        }
    }
    return best;
}

// 0x080dd3dc  size=373  FUN_080dd3dc  status=partial  naming=inferred  name=Mantle_ComputeLedgeTarget
// Probes the ledge target: the mantle-bounds trace from the ledge position along
// its direction, writing the hit height into ledge end.z (0x2c). Trace layout and
// the allSolid/startSolid byte mapping are inferred (see open questions).
void Mantle_ComputeLedgeTarget(pmove_s *pm, MantleLedge *ledge)
{
    playerState_s *ps;
    float mins[3];
    float maxs[3];
    float start[3];
    float end[3];
    trace_s tr;
    float height;

    ps = pm->ps;
    if ((ledge->flags & 1u) != 0) {
        Vec3_Copy_080dc44e(ps->mins, mins);
        Vec3_Copy_080dc44e(ps->maxs, maxs);
        maxs[2] = 50.0f;
        /* gameplay-fix(wpn_mantle_turret): 0x080dd45f..0x080dd481 copy ledgePos (+0x18), not start (+0x0c), into the probe start; 0x080dd4eb tests startsolid */
        Vec3_Copy_080dc44e(ledge->ledgePos, start);
        Vec3_MA_080dc4c2(start, 31.0f, ledge->dir, start);
        Vec3_Copy_080dc44e(start, end);
        end[2] = end[2] - 18.0f;
        PM_TraceRaw(pm, &tr, start, mins, maxs, end, ps->clientNum, pm->tracemask);
        if (tr.startsolid != 0 || tr.fraction < 1.0f) {
            ledge->flags = ledge->flags & 0xfffffffeu;
            Vec3_Copy_080dc44e(ledge->ledgePos, ledge->end);
        } else {
            Vec3_Copy_080dc44e(end, ledge->end);
            height = (end[2] - start[2]) * tr.fraction + start[2];
            ledge->end[2] = height;
        }
    } else {
        Vec3_Copy_080dc44e(ledge->ledgePos, ledge->end);
    }
}

// 0x080dd552  size=264  FUN_080dd552  status=translated  naming=inferred  name=Mantle_Start
// Starts the mantle: records the ledge yaw and stage, copies the ledge flags,
// computes the stage time and the origin, sets the mantling state bits.
void Mantle_Start(pmove_s *pm, playerState_s *ps, MantleLedge *ledge)
{
    MantleState *m;
    float pos[3];
    int upDur;
    int overDur;

    m = &ps->mantleState;
    m->yaw = Vec2_AngleDegrees((const float *)ledge);
    m->timer = 0;
    m->transIndex = Mantle_FindNearestStage(ledge->start[2], ledge->ledgePos[2]);
    m->flags = ledge->flags;

    upDur = Mantle_UpAnimDuration(m);
    overDur = Mantle_OverAnimDuration(m);
    Mantle_EvalStagePos(m, overDur + upDur, pos);
    Vec3_Sub_080dc47e(ledge->end, pos, ps->origin);
    ps->pm_flags = ps->pm_flags | 4u;
    ps->eFlags = ps->eFlags | 0x4000u;
    Vec3_Copy_080dc44e(ledge->end, pm->mantleEndPos);
    pm->mantleDuration = overDur + upDur;
    pm->mantleStarted = 1;
}

// 0x080dd65a  size=1097  FUN_080dd65a  status=partial  naming=observed  name=Mantle_CheckLedge
// Strings: "Checking for ledge at %f units", "Mantle Failed: Ledge is too far
// away", "Mantle Failed: Ledge is too steep", "Mantle available!",
// "Mantle Failed: Player can't fit crouched on ledge", "Mantle Failed: Can't find
// ledge". Trace-flag mapping is inferred (see open questions).
int Mantle_CheckLedge(pmove_s *pm, const float *eye, MantleLedge *ledge, float probeDist)
{
    playerState_s *ps;
    float r;
    float mins[3];
    float maxs[3];
    float start[3];
    float end[3];
    trace_s tr;
    float top;
    const char *msg;

    (void)eye;
    ps = pm->ps;
    r = ps->maxs[0];
    msg = va("Checking for ledge at %f units", (double)probeDist);
    Mantle_DebugPrint(msg);
    Vec3_Set_080dc42a(mins, -r, -r, 0.0f);
    Vec3_Set_080dc42a(maxs, r, r, r + r);
    Vec3_Copy_080dc44e(ledge->start, start);
    start[2] = start[2] + probeDist;
    Vec3_MA_080dc4c2(start, 16.0f, ledge->dir, end);
    PM_TraceRaw(pm, &tr, start, mins, maxs, end, ps->clientNum, pm->tracemask);
    /* gameplay-fix(wpn_mantle_turret): 0x080dd774 tests tr.startsolid ([ebp-0x25]), not allsolid */
    if (tr.startsolid != 0 || tr.fraction < 1.0f) {
        Mantle_DebugPrint("Mantle Failed: Ledge is too far away");
        return 0;
    }

    Vec3_Copy_080dc44e(end, start);
    /* gameplay-fix(wpn_mantle_turret): 0x080dd7b8..0x080dd7c3 store ledge->start[2] + 18.0 into end[2] ([ebp-0x60]); the second trace sweeps down from end to it */
    top = (float)(ledge->start[2] + 18.0f);
    end[2] = top;
    PM_TraceRaw(pm, &tr, start, mins, maxs, end, ps->clientNum, pm->tracemask);
    if (tr.startsolid == 0 && tr.fraction != 1.0f) {
        if (tr.normal[2] < 0.7f) {
            Mantle_DebugPrint("Mantle Failed: Ledge is too steep");
            return 0;
        }
        Vec3_Copy_080dc44e(end, ledge->ledgePos);
        ledge->ledgePos[2] = (top - start[2]) * tr.fraction + start[2];
        PM_TraceRaw(pm, &tr, ledge->ledgePos, mins, maxs, ledge->ledgePos, ps->clientNum, pm->tracemask);
        if (tr.startsolid == 0) {
            ps->mantleState.flags = ps->mantleState.flags | 8u;
            ledge->flags = ledge->flags | 8u;
            Mantle_DebugPrint("Mantle available!");
            if ((pm->cmd.buttons & 0x400u) == 0) {
                return 1;
            }
            Mantle_ComputeLedgeTarget(pm, ledge);
            if ((ps->eFlags & 4u) == 0) {
                PM_TraceRaw(pm, &tr, ledge->ledgePos, ps->mins, ps->maxs, ledge->ledgePos,
                            ps->clientNum, pm->tracemask);
                if (tr.startsolid != 0) {
                    ledge->flags = ledge->flags | 2u;
                }
                PM_TraceRaw(pm, &tr, ledge->end, ps->mins, ps->maxs, ledge->end,
                            ps->clientNum, pm->tracemask);
                if (tr.startsolid == 0) {
                    ledge->flags = ledge->flags | 4u;
                }
            }
            Mantle_Start(pm, ps, ledge);
            return 1;
        }
        Mantle_DebugPrint("Mantle Failed: Player can't fit crouched on ledge");
        return 0;
    }
    Mantle_DebugPrint("Mantle Failed: Can't find ledge");
    return 0;
}

// 0x080ddaa4  size=842  FUN_080ddaa4  status=translated  naming=observed  name=Mantle_ValidateAnims
// Strings: "PLAYER_MANTLE", "Mantle anim [%s] has {X,Y,Z} translation %f, should
// be %f\n" (observed). Loads the mantle tree once and reports anims whose root
// translation is off the expected stage values.
void Mantle_ValidateAnims(unsigned int ent)
{
    unsigned int tree;
    float scratch[4];
    float anim[3];
    int k;
    int idx;

    if (g_mantleAnimTree != 0) {
        return;
    }
    g_mantleAnimTree = (unsigned int)XAnim_AllocNodeTable("PLAYER_MANTLE", 0xb, (void *(*)(int))ent);
    tree = g_mantleAnimTree;
    XAnim_SetGroupNode((struct XAnimNodeTable *)tree, 0, (const char *)g_mantleAnimNames, 1, 10, 0);
    for (k = 1; k < 0xb; k++) {
        XAnim_FindOrLoad(g_mantleAnimNames[k], (void *(*)(int))Mantle_OnAnimEvent);
        XAnim_SetLeafNode((struct XAnimNodeTable *)tree, k, g_mantleAnimNames[k]);
    }

    for (k = 0; k < 7; k++) {
        idx = g_mantleStages[k].upAnim;
        XAnim_NodeBlendSampleB((struct XAnimNodeTable *)tree, (unsigned int)idx, scratch, anim, 1.0f);
        if (1.0L < Float_Abs_080dc354((float)(anim[0] - 16.0))) {
            Com_Error(1, "Mantle anim [%s] has X translation %f, should be %f\n",
                        g_mantleAnimNames[idx], (double)anim[0], 16.0);
        }
        if (1.0L < Float_Abs_080dc354(anim[1])) {
            Com_Error(1, "Mantle anim [%s] has Y translation %f, should be %f\n",
                        g_mantleAnimNames[idx], (double)anim[1], 0.0);
        }
        if (1.0L < Float_Abs_080dc354((float)(anim[2] - g_mantleStages[k].zHeight))) {
            Com_Error(1, "Mantle anim [%s] has Z translation %f, should be %f\n",
                        g_mantleAnimNames[idx], (double)anim[2], (double)g_mantleStages[k].zHeight);
        }

        idx = g_mantleStages[k].overAnim;
        XAnim_NodeBlendSampleB((struct XAnimNodeTable *)tree, (unsigned int)idx, scratch, anim, 1.0f);
        if (1.0L < Float_Abs_080dc354((float)(anim[0] - 31.0))) {
            Com_Error(1, "Mantle anim [%s] has X translation %f, should be %f\n",
                        g_mantleAnimNames[idx], (double)anim[0], 31.0);
        }
        if (1.0L < Float_Abs_080dc354(anim[1])) {
            Com_Error(1, "Mantle anim [%s] has Y translation %f, should be %f\n",
                        g_mantleAnimNames[idx], (double)anim[1], 0.0);
        }
        if (1.0L < Float_Abs_080dc354((float)(anim[2] - -18.0))) {
            Com_Error(1, "Mantle anim [%s] has Z translation %f, should be %f\n",
                        g_mantleAnimNames[idx], (double)anim[2], -18.0);
        }
    }
}

// 0x080dddee  size=15  FUN_080dddee  status=translated  naming=inferred  name=Mantle_ResetTree
void Mantle_ResetTree(void)
{
    g_mantleAnimTree = 0;
}

// 0x080dddfe  size=650  FUN_080dddfe  status=partial  naming=observed  name=Mantle_FindSurface
// Strings: "Mantle Failed: No mantle surface found", "...No mantle surface with
// MANTLE_ON or MANTLE_OVER found", "...Mantle surface has vertical normal",
// "...Player is not facing mantle surface", "...Mantle brush is too thick".
// MANTLE_ON/MANTLE_OVER are the surface-flag masks 0x6000000 tested here.
int Mantle_FindSurface(pmove_s *pm, const float *eye, trace_s *tr, float *normalOut)
{
    playerState_s *ps;
    float r;
    float radius;
    float fVar3;
    float fRange;
    float mins[3];
    float maxs[3];
    float dirFlat[3];
    float start[3];
    float end[3];
    long double l;
    double angle;
    int result;

    ps = pm->ps;
    r = ps->maxs[0];
    radius = dvar_mantle_check_radius->current.value;
    Vec3_Set_080dce76(mins, -radius, -radius, ps->mins[2]);
    Vec3_Set_080dce76(maxs, radius, radius, ps->maxs[2]);
    fVar3 = r - radius;
    fRange = dvar_mantle_check_range->current.value;
    Vec3_Copy_080dc44e(eye, dirFlat);
    dirFlat[2] = 0.0f;
    Vec3_Normalize(dirFlat);
    Vec3_MA_080dc4c2(ps->origin, -fVar3, dirFlat, start);
    Vec3_MA_080dc4c2(ps->origin, fRange + fVar3, dirFlat, end);
    PM_TraceRaw(pm, tr, start, mins, maxs, end, ps->clientNum, 0x1000000);

    result = 0;
    if (tr->startsolid == 0 && tr->allsolid == 0) {
        if (tr->fraction == 1.0f) {
            Mantle_DebugPrint("Mantle Failed: No mantle surface found");
        } else if ((tr->surfaceFlags & 0x6000000u) == 0) {
            Mantle_DebugPrint("Mantle Failed: No mantle surface with MANTLE_ON or MANTLE_OVER found");
        } else {
            Vec3_NegateBits((const unsigned int *)tr->normal, (unsigned int *)normalOut);
            normalOut[2] = 0.0f;
            l = (long double)Vec3_Normalize(normalOut);
            if ((float)l < 0.0001) {
                Mantle_DebugPrint("Mantle Failed: Mantle surface has vertical normal");
            } else {
                l = Vec3_DotAlt(dirFlat, normalOut);
                angle = acos((double)(float)l);
                if ((double)dvar_mantle_check_angle->current.value < angle * 57.29577951308232) {
                    Mantle_DebugPrint("Mantle Failed: Player is not facing mantle surface");
                } else {
                    result = 1;
                }
            }
        }
    } else {
        Mantle_DebugPrint("Mantle Failed: Mantle brush is too thick");
    }
    return result;
}

// 0x080de088  size=472  FUN_080de088  status=partial  naming=observed  name=Mantle_TryStart
// Strings: "====== Mantle Debug ======", "Mantle Failed: Not enabled", ...using
// binoculars, not standing, already mantling, is dead. Tries the three probe
// heights (60, 40, 20) in order.
void Mantle_TryStart(pmove_s *pm, struct pml_s *pml)
{
    const float *eye = (const float *)pml;
    playerState_s *ps;
    trace_s tr;
    float normal[3];
    MantleLedge ledge;

    Mantle_DebugPrint("====== Mantle Debug ======");
    if (dvar_mantle_enable->current.enabled == 0) {
        Mantle_DebugPrint("Mantle Failed: Not enabled");
        return;
    }
    ps = pm->ps;
    Mantle_ClearSurfaceFlag(ps);
    if (ps->pm_type < 6) {
        if ((ps->pm_flags & 4u) == 0) {
            if ((ps->eFlags & 0xcu) == 0) {
                if (ps->weaponstate < 0x11 || 0x16 < ps->weaponstate) {
                    if (Mantle_FindSurface(pm, eye, &tr, normal) != 0) {
                        memset(&ledge, 0, sizeof(ledge));
                        Vec3_Copy_080dc44e(normal, ledge.dir);
                        Vec3_Copy_080dc44e(ps->origin, ledge.start);
                        if ((tr.surfaceFlags & 0x4000000u) != 0) {
                            ledge.flags = ledge.flags | 1u;
                        }
                        if (Mantle_CheckLedge(pm, eye, &ledge, 60.0f) == 0) {
                            if (Mantle_CheckLedge(pm, eye, &ledge, 40.0f) == 0) {
                                Mantle_CheckLedge(pm, eye, &ledge, 20.0f);
                            }
                        }
                    }
                } else {
                    Mantle_DebugPrint("Mantle Failed: Player using binoculars");
                }
            } else {
                Mantle_DebugPrint("Mantle Failed: Player not standing");
            }
        } else {
            Mantle_DebugPrint("Mantle Failed: Player already mantling");
        }
    } else {
        Mantle_DebugPrint("Mantle Failed: Player is dead");
    }
}

// 0x080de260  size=537  FUN_080de260  status=translated  naming=inferred  name=Mantle_UpdateMove
// Advances the mantle by the usercmd delta: moves the origin along the stage
// curve, sets the velocity, and ends the mantle when the stage time is reached.
void Mantle_UpdateMove(pmove_s *pm, playerState_s *ps, pml_s *cmd)
{
    MantleState *m;
    float oldPos[3];
    float newPos[3];
    int upDur;
    int overDur;
    int total;
    int oldTime;
    int newTime;
    int anim;

    if (dvar_mantle_enable->current.enabled == 0) {
        return;
    }
    m = &ps->mantleState;
    ps->mantleState.flags = ps->mantleState.flags & 0xfffffff7u;
    if ((ps->mantleState.flags & 2u) != 0) {
        BG_AddPredictableEventToPlayerstate(0x8d, 0, ps);
    }
    upDur = Mantle_UpAnimDuration(m);
    overDur = Mantle_OverAnimDuration(m);
    total = overDur + upDur;
    oldTime = m->timer;
    m->timer = cmd->msec + m->timer;
    if (total < m->timer) {
        m->timer = total;
    }
    newTime = m->timer;
    Mantle_EvalStagePos(m, oldTime, oldPos);
    Mantle_EvalStagePos(m, newTime, newPos);
    anim = Mantle_SelectAnim(m);
    BG_AnimScriptAnimation(ps, 3, anim + 0x14, 1);
    Vec3_Sub_080dc47e(newPos, oldPos, newPos);
    Vec3_Add_080de6e6(newPos, ps->origin, ps->origin);
    /* gameplay-fix(wpn_mantle_turret): 0x080de3be..0x080de3cd: fild dt; fmul float const 0.001f (0x8153928); fld1; fdivr, all in x87 extended, stored once as float */
    Vec3_Scale_080dceca(newPos, (float)((long double)1.0 / ((long double)(newTime - oldTime) * (long double)0.001f)), ps->velocity);

    if (m->timer == total) {
        ps->pm_flags = ps->pm_flags & 0xfffffffbu;
        pm->mantleStarted = 0;
        if ((m->flags & 1u) != 0) {
            BG_AnimScriptEvent(ps, 3, 0, 1);
        }
        if ((m->flags & 4u) != 0) {
            BG_AddPredictableEventToPlayerstate(0x8c, 0, ps);
            ps->eFlags = ps->eFlags & 0xffffbfffu;
        }
    }
}

// 0x080de47a  size=319  FUN_080de47a  status=translated  naming=inferred  name=Mantle_ApplyYawCap
// Wraps the yaw difference between the mantle yaw and the view yaw into the
// mantle_view_yawcap range, accumulates the wrapped part into the yaw delta and
// re-aims the view at the capped yaw.
void Mantle_ApplyYawCap(playerState_s *ps)
{
    float diff;
    float cap;
    float step;

    if (dvar_mantle_enable->current.enabled == 0) {
        return;
    }
    diff = AngleDelta(ps->mantleState.yaw, ps->viewangles[1]);
    cap = dvar_mantle_view_yawcap->current.value;
    if (diff < -cap || cap < diff) {
        for (; diff < -cap; diff = diff + cap) {
        }
        for (; cap < diff; diff = diff - cap) {
        }
        step = cap;
        if (0.0 < diff) {
            step = -step;
        }
        ps->delta_angles[1] = ((unsigned int)lrintf((float)(diff * 182.04445)) & 0xffffu) + ps->delta_angles[1];
        ps->viewangles[1] = AngleNormalize360_Loop(ps->mantleState.yaw + step);
    }
}

// 0x080de5ba  size=26  FUN_080de5ba  status=translated  naming=inferred  name=Mantle_ClearSurfaceFlag
void Mantle_ClearSurfaceFlag(playerState_s *ps)
{
    ps->mantleState.flags = ps->mantleState.flags & 0xfffffff7u;
}

// 0x080de5d4  size=101  FUN_080de5d4  status=translated  naming=inferred  name=Mantle_OverAnimNotLow
// True when mantling and the stage's over animation is not the low-over anim (10).
int Mantle_OverAnimNotLow(const playerState_s *ps)
{
    if (dvar_mantle_enable->current.enabled == 0) {
        return 0;
    }
    if ((ps->pm_flags & 4u) == 0) {
        return 0;
    }
    return g_mantleStages[ps->mantleState.transIndex].overAnim != 10;
}

// 0x080de63a  size=25  FUN_080de63a  status=translated  naming=unknown  name=Float_Abs_080de63a
// Second copy of the absolute-value helper; the binary calls this one from the
// mantle code. Same body as Float_Abs_080dc354.
long double Float_Abs_080de63a(float v);
long double Float_Abs_080de63a(float v)
{
    return Float_Abs_080dc354(v);
}

// 0x080de654  size=35  FUN_080de654  status=translated  naming=unknown  name=Vec3_Set_080de654
void Vec3_Set_080de654(float *v, float x, float y, float z)
{
    cod2_copy_word(&v[0], &x);
    cod2_copy_word(&v[1], &y);
    cod2_copy_word(&v[2], &z);
}

// 0x080de678  size=47  FUN_080de678  status=translated  naming=unknown  name=Vec3_CopyBits_080de678
void Vec3_CopyBits_080de678(const float *src, float *dst)
{
    cod2_copy_word(&dst[0], &src[0]);
    cod2_copy_word(&dst[1], &src[1]);
    cod2_copy_word(&dst[2], &src[2]);
}

// 0x080de6a8  size=62  FUN_080de6a8  status=translated  naming=unknown  name=Vec3_NegateBits
// Negates by flipping the sign bit of each word (bit-exact for finite and NaN).
void Vec3_NegateBits(const unsigned int *in, unsigned int *out)
{
    out[0] = in[0] ^ 0x80000000u;
    out[1] = in[1] ^ 0x80000000u;
    out[2] = in[2] ^ 0x80000000u;
}

// 0x080de6e6  size=68  FUN_080de6e6  status=translated  naming=unknown  name=Vec3_Add_080de6e6
void Vec3_Add_080de6e6(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x080de72a  size=68  FUN_080de72a  status=translated  naming=unknown  name=Vec3_Sub_080de72a
void Vec3_Sub_080de72a(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x080de76e  size=56  FUN_080de76e  status=translated  naming=unknown  name=Vec3_ScaleAlt
void Vec3_ScaleAlt(const float *in, float s, float *out)
{
    out[0] = s * in[0];
    out[1] = s * in[1];
    out[2] = s * in[2];
}

// 0x080de7a6  size=83  FUN_080de7a6  status=translated  naming=unknown  name=Vec3_MA_080de7a6
void Vec3_MA_080de7a6(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x080de7fa  size=51  FUN_080de7fa  status=translated  naming=unknown  name=Vec3_DotAlt
long double Vec3_DotAlt(const float *a, const float *b)
{
    return (long double)a[0] * (long double)b[0] +
           (long double)a[1] * (long double)b[1] +
           (long double)a[2] * (long double)b[2];
}

// 0x080de830  size=2522  FUN_080de830  status=translated  naming=observed  name=Player_RegisterDvars
// Registers the player/BG tunables (names observed in the registration strings):
// player_view_pitch_*, bg_*, inertia*, friction, stopspeed, bg_bob*, player_breath_*,
// player_toggleBinoculars, player_scopeExitOnDamage, player_adsExitDelay,
// player_*Threshhold, player_*SpeedScale, player_turnAnims, player_dmgtimer_*.
// Then registers the jump and mantle dvars.
void Player_RegisterDvars(void)
{
    dvar_player_view_pitch_up = Dvar_RegisterFloat("player_view_pitch_up", 85.0f, 0.0f, 90.0f, 0x1180u);
    dvar_player_view_pitch_down = Dvar_RegisterFloat("player_view_pitch_down", 85.0f, 0.0f, 90.0f, 0x1180u);
    dvar_bg_ladder_yawcap = Dvar_RegisterFloat("bg_ladder_yawcap", 100.0f, 0.0f, 360.0f, 0x1180u);
    dvar_bg_prone_yawcap = Dvar_RegisterFloat("bg_prone_yawcap", 85.0f, 0.0f, 360.0f, 0x1180u);
    dvar_bg_foliagesnd_minspeed = Dvar_RegisterFloat("bg_foliagesnd_minspeed", 40.0f, 0.0f, 3.40282347e+38f, 0x1180u);
    dvar_bg_foliagesnd_maxspeed = Dvar_RegisterFloat("bg_foliagesnd_maxspeed", 180.0f, 0.0f, 3.40282347e+38f, 0x1180u);
    dvar_bg_foliagesnd_slowinterval = Dvar_RegisterInt("bg_foliagesnd_slowinterval", 0x5dc, 0, 0x7fffffff, 0x1180u);
    dvar_bg_foliagesnd_fastinterval = Dvar_RegisterInt("bg_foliagesnd_fastinterval", 500, 0, 0x7fffffff, 0x1180u);
    dvar_bg_foliagesnd_resetinterval = Dvar_RegisterInt("bg_foliagesnd_resetinterval", 500, 0, 0x7fffffff, 0x1180u);
    dvar_bg_fallDamageMinHeight = Dvar_RegisterFloat("bg_fallDamageMinHeight", 256.0f, 1.0f, 3.40282347e+38f, 0x1188u);
    dvar_bg_fallDamageMaxHeight = Dvar_RegisterFloat("bg_fallDamageMaxHeight", 480.0f, 1.0f, 3.40282347e+38f, 0x1188u);
    dvar_inertiaMax = Dvar_RegisterFloat("inertiaMax", 50.0f, 0.0f, 1000.0f, 0x1180u);
    dvar_inertiaDebug = Dvar_RegisterBool("inertiaDebug", 0, 0x1180u);
    dvar_inertiaAngle = Dvar_RegisterFloat("inertiaAngle", 0.0f, -1.0f, 1.0f, 0x1180u);
    dvar_friction = Dvar_RegisterFloat("friction", 5.5f, 0.0f, 100.0f, 0x1180u);
    dvar_stopspeed = Dvar_RegisterFloat("stopspeed", 100.0f, 0.0f, 1000.0f, 0x1180u);
    dvar_bg_swingSpeed = Dvar_RegisterFloat("bg_swingSpeed", 0.2f, 0.0f, 1.0f, 0x1080u);
    dvar_bg_bobAmplitudeStanding = Dvar_RegisterFloat("bg_bobAmplitudeStanding", 0.007f, 0.0f, 1.0f, 0x1180u);
    dvar_bg_bobAmplitudeDucked = Dvar_RegisterFloat("bg_bobAmplitudeDucked", 0.0075f, 0.0f, 1.0f, 0x1180u);
    dvar_bg_bobAmplitudeProne = Dvar_RegisterFloat("bg_bobAmplitudeProne", 0.03f, 0.0f, 1.0f, 0x1180u);
    dvar_bg_bobMax = Dvar_RegisterFloat("bg_bobMax", 8.0f, 0.0f, 36.0f, 0x1180u);
    dvar_bg_aimSpreadMoveSpeedThreshold = Dvar_RegisterFloat("bg_aimSpreadMoveSpeedThreshold", 11.0f, 0.0f, 300.0f, 0x1180u);
    dvar_player_breath_hold_time = Dvar_RegisterFloat("player_breath_hold_time", 4.5f, 0.0f, 30.0f, 0x1180u);
    dvar_player_breath_gasp_time = Dvar_RegisterFloat("player_breath_gasp_time", 1.0f, 0.0f, 30.0f, 0x1180u);
    dvar_player_breath_fire_delay = Dvar_RegisterFloat("player_breath_fire_delay", 0.0f, 0.0f, 30.0f, 0x1180u);
    dvar_player_breath_gasp_scale = Dvar_RegisterFloat("player_breath_gasp_scale", 4.5f, 0.0f, 50.0f, 0x1180u);
    dvar_player_breath_hold_lerp = Dvar_RegisterFloat("player_breath_hold_lerp", 4.0f, 0.0f, 50.0f, 0x1180u);
    dvar_player_breath_gasp_lerp = Dvar_RegisterFloat("player_breath_gasp_lerp", 6.0f, 0.0f, 50.0f, 0x1180u);
    dvar_player_breath_snd_lerp = Dvar_RegisterFloat("player_breath_snd_lerp", 2.0f, 0.0f, 100.0f, 0x1180u);
    dvar_player_breath_snd_delay = Dvar_RegisterFloat("player_breath_snd_delay", 1.0f, 0.0f, 2.0f, 0x1180u);
    dvar_player_toggleBinoculars = Dvar_RegisterBool("player_toggleBinoculars", 1, 0x1100u);
    dvar_player_scopeExitOnDamage = Dvar_RegisterBool("player_scopeExitOnDamage", 0, 0x1180u);
    dvar_player_adsExitDelay = Dvar_RegisterInt("player_adsExitDelay", 0, 0, 1000, 0x1180u);
    dvar_player_moveThreshhold = Dvar_RegisterFloat("player_moveThreshhold", 10.0f, 1e-08f, 20.0f, 0x11c0u);
    dvar_player_footstepsThreshhold = Dvar_RegisterFloat("player_footstepsThreshhold", 0.0f, 0.0f, 50000.0f, 0x11c0u);
    dvar_player_strafeSpeedScale = Dvar_RegisterFloat("player_strafeSpeedScale", 0.8f, 0.0f, 20.0f, 0x1180u);
    dvar_player_backSpeedScale = Dvar_RegisterFloat("player_backSpeedScale", 0.7f, 0.0f, 20.0f, 0x1180u);
    dvar_player_spectateSpeedScale = Dvar_RegisterFloat("player_spectateSpeedScale", 2.0f, 0.0f, 20.0f, 0x1180u);
    dvar_player_turnAnims = Dvar_RegisterBool("player_turnAnims", 0, 0x1180u);
    dvar_player_dmgtimer_timePerPoint = Dvar_RegisterFloat("player_dmgtimer_timePerPoint", 100.0f, 0.0f, 3.40282347e+38f, 0x1180u);
    dvar_player_dmgtimer_maxTime = Dvar_RegisterFloat("player_dmgtimer_maxTime", 750.0f, 0.0f, 3.40282347e+38f, 0x1180u);
    dvar_player_dmgtimer_minScale = Dvar_RegisterFloat("player_dmgtimer_minScale", 0.0f, 0.0f, 1.0f, 0x1180u);
    dvar_player_dmgtimer_stumbleTime = Dvar_RegisterInt("player_dmgtimer_stumbleTime", 500, 0, 2000, 0x1180u);
    dvar_player_dmgtimer_flinchTime = Dvar_RegisterInt("player_dmgtimer_flinchTime", 500, 0, 2000, 0x1180u);
    Player_RegisterJumpDvars();
    Mantle_RegisterDvars();
}

// 0x080df20a  size=27  FUN_080df20a  status=translated  naming=observed  name=BG_GetItemByIndex
// Item table access (BG_CanItemBeGrabbed message names the item fields).
gitem_s *BG_GetItemByIndex(int index)
{
    return &g_081850c0[index];
}

// 0x080df226  size=180  FUN_080df226  status=translated  naming=inferred  name=BG_FindItemByName
// Looks up an item by pickup alias or classname over items 0x81..0x82, then falls
// back to the generic name lookup G_LoadWeaponDef.
gitem_s *BG_FindItemByName(const char *name)
{
    int i;
    int idx;

    i = 0x81;
    for (;;) {
        if (0x82 < i) {
            idx = G_LoadWeaponDef((const char *)name);
            if (idx == 0) {
                return 0;
            }
            return &g_081850c0[idx];
        }
        if (Q_stricmp(g_081850c0[i].pickupAlias, name) == 0) {
            break;
        }
        if (Q_stricmp(g_081850c0[i].classname, name) == 0) {
            break;
        }
        i++;
    }
    return &g_081850c0[i];
}

// 0x080df2da  size=195  FUN_080df2da  status=translated  naming=inferred  name=BG_PlayerNearEntityTrajectory
// Returns 1 when the player origin lies within the box around the entity's
// evaluated trajectory position (+/-36 in x/y, -88..+18 in z).
int BG_PlayerNearEntityTrajectory(const playerState_s *ps, const void *ent, int atTime)
{
    float res[3];

    BG_EvaluateTrajectory((const trajectory_s *)((const unsigned char *)ent + 0xc), atTime, res);
    if (36.0 < ps->origin[0] - res[0] || ps->origin[0] - res[0] < -36.0 ||
        36.0 < ps->origin[1] - res[1] || ps->origin[1] - res[1] < -36.0 ||
        18.0 < ps->origin[2] - res[2] || ps->origin[2] - res[2] < -88.0) {
        return 0;
    }
    return 1;
}

// 0x080df39e  size=565  FUN_080df39e  status=translated  naming=observed  name=BG_CanItemBeGrabbed
// Strings: "BG_CanItemBeGrabbed: index out of range (index is %i, eType is %i)",
// "BG_CanItemBeGrabbed: IT_BAD" (observed). Item types: 1 weapon, 2 ammo, 3 armor
// (the item-type switch in the binary).
int BG_CanItemBeGrabbed(const entityState_s *ent, const playerState_s *ps, int checkOnly)
{
    int result;
    int type;
    int tag;
    unsigned char cVar1;
    int iVar3;
    const char *msg;

    if (ent->index < 1 || 0x82 < ent->index) {
        msg = va("\x15" "BG_CanItemBeGrabbed: index out of range (index is %i, eType is %i)",
                          ent->index, ent->eType);
        Com_Error(1, msg);
    }
    if (ent->clientNum == ps->clientNum) {
        return 0;
    }
    type = g_081850c0[ent->index].giType;
    tag = g_081850c0[ent->index].giTag;
    result = 0;
    if (type == 1) {
        cVar1 = BG_WeaponFlagA(tag);
        if (cVar1 == 0 || BitSet_Test(ps->weapons, tag) != 0) {
            iVar3 = BG_GetWeaponAmmoRoom((playerState_t *)ps, tag);
            if (iVar3 < 1) {
                return 0;
            }
        } else if (checkOnly != 0 || (iVar3 = BG_GetWeaponAmmoRoom((playerState_t *)ps, tag), iVar3 < 1)) {
            return 0;
        }
        result = 1;
    } else {
        if (type < 2) {
            if (type == 0) {
                Com_Error(1, "\x15" "BG_CanItemBeGrabbed: IT_BAD");
            }
        } else {
            if (type == 2) {
                if (BitSet_Test(ps->weapons, tag) == 0) {
                    iVar3 = BG_GetWeaponClipOnly(tag);
                    if (iVar3 == 0 || (iVar3 = BG_GetWeaponAmmoRoom((playerState_t *)ps, tag), iVar3 < 1)) {
                        return 0;
                    }
                } else {
                    iVar3 = BG_GetWeaponAmmoRoom((playerState_t *)ps, tag);
                    if (iVar3 < 1) {
                        return 0;
                    }
                }
                return 1;
            }
            if (type == 3) {
                return ps->stats[0] < ps->stats[2];
            }
        }
    }
    return result;
}

// 0x080df5d4  size=883  FUN_080df5d4  status=translated  naming=observed  name=BG_EvaluateTrajectory
// String: "BG_EvaluateTrajectory: unknown trType: %i" (observed). Position at atTime.
void BG_EvaluateTrajectory(const trajectory_s *tr, int atTime, float *result)
{
    /* gameplay-fix(wpn_grenade): time scale is the float constant 0.001f (0x8154d2c/0x8154dbc), product in x87 extended; gravity 0x080df75d..0x080df7a1 */
    int t;
    float dt;
    float ratio;
    long double lv;
    double dv;

    t = atTime;
    switch (tr->trType) {
    case 0:
    case 1:
    case 6:
        Vec3_CopyBits_080de678(tr->trBase, result);
        break;
    case 2:
        dt = (float)((long double)(t - tr->trTime) * (long double)0.001f);
        VectorMA_080e11b8(tr->trBase, dt, tr->trDelta, result);
        break;
    case 3:
        if (tr->trDuration + tr->trTime < t) {
            t = tr->trDuration + tr->trTime;
        }
        dt = (float)((long double)(t - tr->trTime) * (long double)0.001f);
        if (dt < 0.0f) {
            dt = 0.0f;
        }
        VectorMA_080e11b8(tr->trBase, dt, tr->trDelta, result);
        break;
    case 4:
        ratio = (float)(t - tr->trTime) / (float)tr->trDuration;
        dv = sin((double)(ratio * 3.1415927 + ratio * 3.1415927));
        VectorMA_080e11b8(tr->trBase, (float)dv, tr->trDelta, result);
        break;
    case 5:
        dt = (float)((long double)(t - tr->trTime) * (long double)0.001f);
        VectorMA_080e11b8(tr->trBase, dt, tr->trDelta, result);
        result[2] = (float)((long double)result[2] - (long double)dt * (long double)400.0f * (long double)dt);
        break;
    case 7:
        if (tr->trDuration + tr->trTime < t) {
            t = tr->trDuration + tr->trTime;
        }
        dt = (float)((long double)(t - tr->trTime) * (long double)0.001f);
        lv = (long double)VectorLength_080e1240(tr->trDelta);
        ratio = (float)(lv / ((long double)0.001f * (long double)tr->trDuration));
        Vec3_NormalizeCopy(tr->trDelta, result);
        VectorMA_080e11b8(tr->trBase, (float)((long double)ratio * (long double)0.5f * (long double)dt * (long double)dt), result, result);
        break;
    case 8:
        if (tr->trDuration + tr->trTime < t) {
            t = tr->trDuration + tr->trTime;
        }
        dt = (float)((long double)(t - tr->trTime) * (long double)0.001f);
        lv = (long double)VectorLength_080e1240(tr->trDelta);
        ratio = (float)(lv / ((long double)0.001f * (long double)tr->trDuration));
        Vec3_NormalizeCopy(tr->trDelta, result);
        {
            float tmp[3];
            VectorMA_080e11b8(tr->trBase, dt, tr->trDelta, tmp);
            VectorMA_080e11b8(tmp, (float)((long double)(-ratio) * (long double)0.5f * (long double)dt * (long double)dt), result, result);
        }
        break;
    default:
        Com_Error(1, "\x15" "BG_EvaluateTrajectory: unknown trType: %i", tr->trType);
        break;
    }
}

// 0x080df948  size=558  FUN_080df948  status=translated  naming=observed  name=BG_EvaluateTrajectoryDelta
// String: "BG_EvaluateTrajectoryDelta: unknown trType: %i" (observed). Velocity at
// atTime.
void BG_EvaluateTrajectoryDelta(const trajectory_s *tr, int atTime, float *result)
{
    float f;
    double dv;

    switch (tr->trType) {
    case 0:
    case 1:
        VectorClear(result);
        break;
    case 2:
        VectorCopy(tr->trDelta, result);
        break;
    case 3:
        if (tr->trDuration + tr->trTime < atTime) {
            VectorClear(result);
        } else {
            VectorCopy(tr->trDelta, result);
        }
        break;
    case 4:
        f = ((float)(atTime - tr->trTime) / (float)tr->trDuration) * 3.1415927;
        dv = cos((double)(f + f));
        VectorScale_080e1180(tr->trDelta, (float)dv * 0.5f, result);
        break;
    case 5:
        VectorCopy(tr->trDelta, result);
        {
            float dtd = (float)((long double)(atTime - tr->trTime) * (long double)0.001f);
            result[2] = (float)((long double)result[2] - (long double)dtd * (long double)800.0f);
        }
        break;
    default:
        Com_Error(1, "\x15" "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trType);
        break;
    case 7:
        if (tr->trDuration + tr->trTime < atTime) {
            VectorClear(result);
        } else {
            f = (float)((long double)(atTime - tr->trTime) * (long double)0.001f);
            VectorScale_080e1180(tr->trDelta, f * f, result);
        }
        break;
    case 8:
        if (tr->trDuration + tr->trTime < atTime) {
            VectorClear(result);
        } else {
            VectorScale_080e1180(tr->trDelta, (float)((long double)(atTime - tr->trTime) * (long double)0.001f), result);
        }
        break;
    }
}
