/*
 * chunk 042 types: player-movement (Pmove) support code, 0x080e4644..0x080e982e.
 * All layouts are recovered from consistent offsets in this chunk only; fields that are
 * only touched by other chunks are left as padding. Names are inferred (Q3-style Pmove
 * counterparts) unless noted.
 */
#ifndef COD2_CHUNK_042_TYPES_H
#define COD2_CHUNK_042_TYPES_H


typedef void (*traceFn_t)(struct trace_s *results, const float *start, const float *mins,
                          const float *maxs, const float *end, int passEntityNum, int contentMask);

/* Trace dispatch row (12 bytes at 0x081869c0); the global's type is declared in cod2_globals.h. */
struct cod2_pm_dispatch_s {
    traceFn_t trace;
    void *unknown4;
    void *unknown8;
};
COD2_ASSERT_SIZE(cod2_pm_dispatch_s, 12);

/* ---- per-client animation controller (array at level+0xb3bfc, stride 0x4b8) ---- */
struct clientTurnView_s {
    byte pad0[0x380];
    float turnYawPrev;      /* +0x380 */
    int turnActive;         /* +0x384 */
    byte pad388[0x3b0 - 0x388];
    float turnYawCurrent;   /* +0x3b0 */
    byte pad3b4[0x4b0 - 0x3b4];
    int turnAnimEndTime;    /* +0x4b0 (string: "turn anim end time is %i, time is %i") */
    char turnAnimType;      /* +0x4b4 (0x0e / 0x0f) */
    byte pad4b5[3];
};
COD2_ASSERT_SIZE(clientTurnView_s, 0x4b8);
COD2_ASSERT_OFFSET(clientTurnView_s, turnYawPrev, 0x380);
COD2_ASSERT_OFFSET(clientTurnView_s, turnActive, 0x384);
COD2_ASSERT_OFFSET(clientTurnView_s, turnYawCurrent, 0x3b0);
COD2_ASSERT_OFFSET(clientTurnView_s, turnAnimEndTime, 0x4b0);
COD2_ASSERT_OFFSET(clientTurnView_s, turnAnimType, 0x4b4);

/* ---- game-state block pointed to by the pointer at 0x0860b420 ---- */
struct gameLocals_s {
    byte pad0[0xb3bdc];
    int time;               /* +0xb3bdc */
    byte padb3be0[0x1c];
    struct clientTurnView_s clientAnim[64]; /* +0xb3bfc (indexed by clientNum, unchecked) */
};
COD2_ASSERT_OFFSET(gameLocals_s, time, 0xb3bdc);
COD2_ASSERT_OFFSET(gameLocals_s, clientAnim, 0xb3bfc);

/* ---- functions defined in this chunk (needed for forward references) ---- */
void PM_CheckDuck(struct pmove_s *pm, struct pml_s *pml);
void PM_FootstepEvent(struct pmove_s *pm, struct pml_s *pml, char oldBobCycle, char newBobCycle,
                      int doEvent);
bool PM_FootstepSpeedOk(struct pmove_s *pm);
int PM_YawQuadrantAnim(float yawDelta);
void PM_Footsteps(struct pmove_s *pm, struct pml_s *pml);
void PM_CheckMoveBump(struct pmove_s *pm);
void PM_DropTimers(struct playerState_s *ps, struct pml_s *pml);
void PM_UpdateLean(struct playerState_s *ps, float msec, struct usercmd_s *cmd, traceFn_t trace);
int PM_CheckBodyYawClear(struct playerState_s *ps, float yaw, char clipType);
void PM_UpdateViewAngles(struct playerState_s *ps, float msec, struct usercmd_s *cmd,
                         byte clipType);
void PM_SmoothBodyAngles(struct pmove_s *pm, struct pml_s *pml);
void PM_UpdateAdsSlowMoveFlag(struct pmove_s *pm);
void PM_Flags_SetBit11IfBit0(struct playerState_s *ps);
void PM_Flags_SetLadder(struct playerState_s *ps);
void PM_Flags_EndLadder(struct playerState_s *ps);
void PM_CheckLadder(struct pmove_s *pm, struct pml_s *pml);
void PM_LadderMove(struct pmove_s *pm, struct pml_s *pml);
void PM_SingleStep(struct pmove_s *pm);
void PM_Pmove(struct pmove_s *pm);
float PM_GetMoveSpeed(struct playerState_s *ps, int time);
float Float_Abs_080e9576(float x);
float Float_Sqrt_080e9590(float x);
float Float_Max_080e95b0(float a, float b);
float Float_Min_080e95d4(float a, float b);
int Int_SelectClamp(int a, int b, int c);
float Float_Square(float x);
int Float_Sign(float x);
void Vec2_Copy(const float *in, float *out);
void Vec2_Add_080e968c(const float *a, const float *b, float *out);
void Vec2_Sub_Dup(const float *a, const float *b, float *out);
void Vec2_Scale(const float *in, float s, float *out);
void Vec2_MA(const float *base, float s, const float *dir, float *out);
float Vec2_Dot_Dup(const float *a, const float *b);
float Vec2_LengthSq(const float *v);
float Vec2_Length_Dup(const float *v);
void Vec3_Clear_Dup(float *v);
void Vec3_Set_Dup(float *v, float x, float y, float z);
void Vec3_Copy_Dup(const float *in, float *out);

#endif /* COD2_CHUNK_042_TYPES_H */
