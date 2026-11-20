#ifndef COD2_CHUNK_039_TYPES_H
#define COD2_CHUNK_039_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include <ctype.h>

/* chunk_039: BG_Anim* player animation script (mp/playeranim.script) parser, condition
 * evaluation, command execution, lerp-frame animation and body angle code.
 * Layouts recovered from consistent offsets in 0x80d6ce0..0x80da9c8. */

/* ---- string tables ------------------------------------------------------ */

/* { string, lazily computed BG_StringHashValue (-1 until computed) }. 8 bytes. */
struct animStringItem_t {
    const char *string;
    int hash;
};
COD2_ASSERT_SIZE(animStringItem_t, 0x8);
COD2_ASSERT_OFFSET(animStringItem_t, hash, 0x4);

/* One entry of the condition definition table at 0x08184c60 (stride 8).
 * type 0 = bit flags (64-bit mask), type 1 = single value index. */
struct animConditionDef_t {
    int type;
    animStringItem_t *strings;
};
COD2_ASSERT_SIZE(animConditionDef_t, 0x8);

/* ---- animation / script structures ------------------------------------- */

/* One entry of the global animation table (stride 0x60). */
struct animation_t {
    char name[64];                 /* 0x00 */
    int blendTime;                 /* 0x40, 0 -> -1 after load */
    float moveSpeed;               /* 0x44 */
    int duration;                  /* 0x48, milliseconds */
    int nameHash;                  /* 0x4c, BG_StringHashValue(name) */
    unsigned int flags;            /* 0x50 */
    unsigned int moveTypeBits[2];  /* 0x54 / 0x58: 64-bit mask of move types */
    int referenced;                /* 0x5c, set by BG_ClearAnimReferencedFlags */
};
COD2_ASSERT_SIZE(animation_t, 0x60);
COD2_ASSERT_OFFSET(animation_t, blendTime, 0x40);
COD2_ASSERT_OFFSET(animation_t, moveSpeed, 0x44);
COD2_ASSERT_OFFSET(animation_t, duration, 0x48);
COD2_ASSERT_OFFSET(animation_t, nameHash, 0x4c);
COD2_ASSERT_OFFSET(animation_t, flags, 0x50);
COD2_ASSERT_OFFSET(animation_t, moveTypeBits, 0x54);
COD2_ASSERT_OFFSET(animation_t, referenced, 0x5c);

/* One command of a script item (stride 0x10). */
struct animCommand_t {
    short bodyPart[2];             /* 0x00: 0 none, 1 legs, 2 torso, 3 both (table 0x08184ac0) */
    short animIndex[2];            /* 0x04 */
    short animDuration[2];         /* 0x08 */
    int soundIndex;                /* 0x0c */
};
COD2_ASSERT_SIZE(animCommand_t, 0x10);
COD2_ASSERT_OFFSET(animCommand_t, animIndex, 0x4);
COD2_ASSERT_OFFSET(animCommand_t, animDuration, 0x8);
COD2_ASSERT_OFFSET(animCommand_t, soundIndex, 0xc);

/* One parsed condition: index into the condition table, then the 64-bit value. */
struct animConditionEntry_t {
    int index;
    unsigned int value[2];
};
COD2_ASSERT_SIZE(animConditionEntry_t, 0xc);

/* A script item: up to 9 conditions and up to 8 commands (size 0xf4). */
struct animScriptItem_t {
    int numConditions;                  /* 0x00 */
    animConditionEntry_t conditions[9]; /* 0x04 */
    int numCommands;                    /* 0x70 */
    animCommand_t commands[8];          /* 0x74 */
};
COD2_ASSERT_SIZE(animScriptItem_t, 0xf4);
COD2_ASSERT_OFFSET(animScriptItem_t, conditions, 0x4);
COD2_ASSERT_OFFSET(animScriptItem_t, numCommands, 0x70);
COD2_ASSERT_OFFSET(animScriptItem_t, commands, 0x74);

/* A script: list of up to 128 item pointers (size 0x204). */
struct animScript_t {
    int numItems;
    animScriptItem_t *items[128];
};
COD2_ASSERT_SIZE(animScript_t, 0x204);

/* Optional name collection table used while parsing (stride 0x48). */
struct animNameEntry_t {
    unsigned short id;             /* 0x00, written by Scr_AddAnimTreeEntry */
    short pad2;
    int hash;                      /* 0x04 */
    char name[64];                 /* 0x08 */
};
COD2_ASSERT_SIZE(animNameEntry_t, 0x48);
COD2_ASSERT_OFFSET(animNameEntry_t, hash, 0x4);
COD2_ASSERT_OFFSET(animNameEntry_t, name, 0x8);

/* Per animated entity lerp state (legs at clientInfo_s+0x380, torso at +0x3b0). */
struct playerAnimLerp_t {
    float yawAngle;                /* 0x00 */
    int yawing;                    /* 0x04 */
    float pitchAngle;              /* 0x08 (torso only) */
    int pitching;                  /* 0x0c (torso only) */
    unsigned int animationNumber;  /* 0x10, bit 0x200 toggles on restart */
    animation_t *animation;        /* 0x14 */
    int animationTime;             /* 0x18, current blend time in ms */
    float oldFramePos[3];          /* 0x1c */
    float animSpeedScale;          /* 0x28 */
    int oldFrameSnapshotTime;      /* 0x2c */
};
COD2_ASSERT_SIZE(playerAnimLerp_t, 0x30);
COD2_ASSERT_OFFSET(playerAnimLerp_t, animationNumber, 0x10);
COD2_ASSERT_OFFSET(playerAnimLerp_t, animation, 0x14);
COD2_ASSERT_OFFSET(playerAnimLerp_t, animationTime, 0x18);
COD2_ASSERT_OFFSET(playerAnimLerp_t, oldFramePos, 0x1c);
COD2_ASSERT_OFFSET(playerAnimLerp_t, animSpeedScale, 0x28);
COD2_ASSERT_OFFSET(playerAnimLerp_t, oldFrameSnapshotTime, 0x2c);


/* Per client animation state inside animScriptData_t.clients (stride 0x4b8). */
struct clientInfo_s {  /* canonical tag (cod2_canon.h forward-declares it opaque); this chunk defines the animation view */
    char pad0[8];
    int entityNumber;              /* 0x008 (used as phase seed) */
    char padC[0x374];
    playerAnimLerp_t legs;         /* 0x380 */
    playerAnimLerp_t torso;        /* 0x3b0 */
    float viewYawOffset;           /* 0x3e0 */
    float easeParam;               /* 0x3e4, only fed to Ease_OutQuadSigned (result unused) */
    float viewAngles[3];           /* 0x3e8 */
    int flag3f4;                   /* 0x3f4 */
    int flag3f8;                   /* 0x3f8 */
    char pad3fc[0x60];
    unsigned int conditions[9][2]; /* 0x45c: 64-bit value per condition index */
    void *animTree;                /* 0x4a4 */
    int pad4a8;
    int moveBlendUntil;            /* 0x4ac */
    char pad4b0[8];
};
COD2_ASSERT_SIZE(clientInfo_s, 0x4b8);
COD2_ASSERT_OFFSET(clientInfo_s, entityNumber, 0x8);
COD2_ASSERT_OFFSET(clientInfo_s, legs, 0x380);
COD2_ASSERT_OFFSET(clientInfo_s, torso, 0x3b0);
COD2_ASSERT_OFFSET(clientInfo_s, viewYawOffset, 0x3e0);
COD2_ASSERT_OFFSET(clientInfo_s, easeParam, 0x3e4);
COD2_ASSERT_OFFSET(clientInfo_s, viewAngles, 0x3e8);
COD2_ASSERT_OFFSET(clientInfo_s, flag3f4, 0x3f4);
COD2_ASSERT_OFFSET(clientInfo_s, flag3f8, 0x3f8);
COD2_ASSERT_OFFSET(clientInfo_s, conditions, 0x45c);
COD2_ASSERT_OFFSET(clientInfo_s, animTree, 0x4a4);
COD2_ASSERT_OFFSET(clientInfo_s, moveBlendUntil, 0x4ac);


/* The global animation script data block (two instances of the pointer exist:
 * g_gameContextPtr and g_curAnimScriptData; the parser targets g_curAnimScriptData). */
struct animScriptData_t {
    animation_t animations[512];                 /* 0x00000 */
    int numAnimations;                           /* 0x0c000 */
    animScript_t scriptAnims[4][41];             /* 0x0c004: [state][moveType] */
    animScript_t scriptCannedAnims[4][41];       /* 0x20a94 */
    animScript_t scriptStateChange[4][4];        /* 0x35524: [fromState][toState] */
    animScript_t scriptEvents[19];               /* 0x37564 */
    animScriptItem_t scriptItems[2048];          /* 0x39bb0 */
    int numScriptItems;                          /* 0xb3bb0 */
    void *animTree;                              /* 0xb3bb4 */
    unsigned short rootAnimA;                    /* 0xb3bb8 */
    unsigned short rootAnimB;                    /* 0xb3bba */
    char padBC[4];
    int (*soundIndexFn)(const char *);           /* 0xb3bc0 */
    void (*playSoundFn)(int, int);               /* 0xb3bc4 */
    char padC8[4];
    unsigned short deathRootAnim;                /* 0xb3bcc */
    char padCE[0xe];
    int time;                                    /* 0xb3bdc */
    int snapshotTime;                            /* 0xb3be0 */
    int frameTime;                               /* 0xb3be4 */
    int isMultiplayer;                           /* 0xb3be8 */
    char padEC[0x10];
    clientInfo_s clients[1];                 /* 0xb3bfc, client count not recovered */
};
COD2_ASSERT_OFFSET(animScriptData_t, numAnimations, 0xc000);
COD2_ASSERT_OFFSET(animScriptData_t, scriptAnims, 0xc004);
COD2_ASSERT_OFFSET(animScriptData_t, scriptCannedAnims, 0x20a94);
COD2_ASSERT_OFFSET(animScriptData_t, scriptStateChange, 0x35524);
COD2_ASSERT_OFFSET(animScriptData_t, scriptEvents, 0x37564);
COD2_ASSERT_OFFSET(animScriptData_t, scriptItems, 0x39bb0);
COD2_ASSERT_OFFSET(animScriptData_t, numScriptItems, 0xb3bb0);
COD2_ASSERT_OFFSET(animScriptData_t, animTree, 0xb3bb4);
COD2_ASSERT_OFFSET(animScriptData_t, rootAnimA, 0xb3bb8);
COD2_ASSERT_OFFSET(animScriptData_t, rootAnimB, 0xb3bba);
COD2_ASSERT_OFFSET(animScriptData_t, soundIndexFn, 0xb3bc0);
COD2_ASSERT_OFFSET(animScriptData_t, playSoundFn, 0xb3bc4);
COD2_ASSERT_OFFSET(animScriptData_t, deathRootAnim, 0xb3bcc);
COD2_ASSERT_OFFSET(animScriptData_t, time, 0xb3bdc);
COD2_ASSERT_OFFSET(animScriptData_t, snapshotTime, 0xb3be0);
COD2_ASSERT_OFFSET(animScriptData_t, frameTime, 0xb3be4);
COD2_ASSERT_OFFSET(animScriptData_t, isMultiplayer, 0xb3be8);
COD2_ASSERT_OFFSET(animScriptData_t, clients, 0xb3bfc);

/* g_gameContextPtr is declared uint8 * in cod2_globals.h; this chunk uses it as the animScriptData_t pointer. */
#define g_animData ((animScriptData_t *)g_gameContextPtr)

/* ---- functions defined in this chunk ------------------------------------ */

void Noop_080d6c32(void);
void Noop_080d6c38(void);
int ReturnZero_080d6c3e(void);
void Noop_080d6c48(void);
int ReturnZero_080d6c4e(void);
void Noop_080d6c58(void);
void Noop_080d6c5e(void);
void Noop_080d6c64(void);
void Noop_080d6c6a(void);
void Noop_080d6c70(void);
void Noop_080d6c76(void);
void Noop_080d6ca6(void);
void Noop_080d6cac(void);
void Noop_080d6cb2(void);
void Noop_080d6cb8(void);
int ReturnZero_080d6cbe(void);
void Noop_080d6cc8(void);
void Noop_080d6cce(void);
int ReturnZero_080d6cd4(void);
int BG_StringHashValue(const char *string);
void BG_AnimParseError(const char *fmt, ...);
int BG_AnimationIndexForString(const char *name);
animation_t *BG_AnimationForString(const char *name);
int BG_IndexForString(const char *token, animStringItem_t *strings, int allowFail);
char *BG_CopyStringIntoBuffer(const char *string, char *buffer, unsigned int bufferSize, int *offset);
void BG_SetPlayerAnimTypeName(int index, const char *name);
void BG_ClearPlayerAnimTypeNames(void);
animNameEntry_t *BG_AnimNameEntryForIndex(unsigned int index);
void BG_ClearAnimReferencedFlags(animScriptData_t *data);
void BG_LoadAnim(void);
void BG_ParseConditionBits(const char **text, animStringItem_t *stringTable, int conditionIndex,
                           unsigned int *result);
int BG_ParseConditions(const char **text, animScriptItem_t *item);
void BG_ParseCommands(const char **text, animScriptItem_t *item, animScriptData_t *data);
void BG_AnimParseAnimScript(animScriptData_t *data, animNameEntry_t *animNames, int *numAnimNames);
int BG_EvaluateConditions(clientInfo_s *client, animScriptItem_t *item);
animScriptItem_t *BG_FirstValidItem(int clientNum, animScript_t *script);
int BG_PlayAnim(playerState_s *ps, unsigned int animIndex, int bodyPart, int forceDuration,
                int setTimer, int isContinue, int force);
void BG_PlayAnimName(playerState_s *ps, const char *animName, int bodyPart, int setTimer,
                     int isContinue, int force);
int BG_ExecuteCommand(playerState_s *ps, animCommand_t *command, int setTimer, int isContinue,
                      int force);
unsigned int BG_AnimScriptAnimation(playerState_s *ps, int state, int moveType, int isContinue);
int BG_AnimScriptStateChange(playerState_s *ps, int toState, int fromState);
int BG_AnimScriptEvent(playerState_s *ps, int event, int isContinue, int force);
animation_t *BG_GetAnimString(int unused, unsigned int index);
void BG_UpdateConditionValue(int clientNum, int condition, int value, int checkConversion);
unsigned int BG_GetConditionValue(clientInfo_s *client, int condition, int checkConversion);
int BG_AnimScriptEventAnimIndex(playerState_s *ps, int event);
animation_t *BG_GetAnimationForIndex(int unused, unsigned int index);
void BG_UpdateConditionsFromPmove(pmove_s *pm);
bool BG_AnimFlagsTestA(clientInfo_s *client, unsigned int animNumber);
bool BG_AnimFlagsTestB(clientInfo_s *client, unsigned int animNumber);
void BG_SetNewAnimation(clientInfo_s *client, playerAnimLerp_t *lerp, unsigned int newAnimation,
                        int isDeath);
void BG_RunLerpFrame(clientInfo_s *client, playerAnimLerp_t *lerp, unsigned int newAnimation,
                     entityState_s *es);
void BG_ClearFinishedAnimation(void *tree, playerAnimLerp_t *lerp);
void BG_SwingAngles(float destination, float swingTolerance, float clampTolerance, float speed,
                    float *angle, int *swinging);
void BG_PlayerAngles(entityState_s *es, clientInfo_s *client);

#endif
