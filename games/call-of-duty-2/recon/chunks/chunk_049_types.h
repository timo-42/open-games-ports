/*
 * Types, globals and out-of-chunk prototypes for chunk 049 (0x08109096..0x810de4e):
 * game-module init/shutdown, entity ordering and frame loop, trace wrappers,
 * turret entity state machine (G_SpawnTurret and its aim/use helpers).
 *
 * Layouts are recovered from consistent offsets in the Ghidra output and were
 * checked against the disassembly where the pseudocode was ambiguous. Unnamed
 * globals use g_<address>; out-of-chunk callees use fn_<address> placeholders
 * unless the merged symbol table already names them.
 */
#ifndef COD2_CHUNK_049_TYPES_H
#define COD2_CHUNK_049_TYPES_H

#include "cod2_types.h"


/* ---- trajectory (BG_EvaluateTrajectory input), 0x24 bytes ---- */
/* trajectory_s: promoted to cod2_types.h */
COD2_ASSERT_SIZE(struct trajectory_s, 0x24);



/* ---- turret mode table: 0x28-byte entries at g_entityHandlerTable, indexed by gentity.turretType ---- */
typedef void (*turret_think_fn)(struct gentity_s *ent);
struct entity_handler_s {
    turret_think_fn think;            /* 0x00 */
    uint8 pad04[0x20 - 4];
    int field20;                      /* 0x20 */
    int field24;                      /* 0x24 */
};
COD2_ASSERT_SIZE(struct entity_handler_s, 0x28);

/* ---- per-turret state, 0x44 bytes, 32 slots at g_turrets ---- */
struct turret_s {
    int inUse;                        /* 0x00 nonzero = slot allocated */
    uint32 flags;                     /* 0x04 bit 0x200 clamps aim, 0x400 reverses limit test, 0x800 needs reset */
    int cooldown08;                   /* 0x08 counts down by 50 per update */
    float arcTop;                     /* 0x0c */
    float arcRight;                   /* 0x10 */
    float arcBottom;                  /* 0x14 */
    float arcLeft;                    /* 0x18 */
    float restAngle;                  /* 0x1c */
    int weaponParam;                  /* 0x20 */
    int tagSlot;                      /* 0x24 -1 when unset */
    int timer;                        /* 0x28 counts down by 50 per update */
    float aimOrigin[3];               /* 0x2c */
    float maxAimRate;                 /* 0x38 */
    float unk3c;                      /* 0x3c aim limit */
    uint8 tag0;                       /* 0x40 */
    uint8 tag1;                       /* 0x41 */
    uint8 tag2;                       /* 0x42 */
    uint8 tag3;                       /* 0x43 */
};
COD2_ASSERT_SIZE(struct turret_s, 0x44);
COD2_ASSERT_OFFSET(struct turret_s, arcTop, 0x0c);
COD2_ASSERT_OFFSET(struct turret_s, restAngle, 0x1c);
COD2_ASSERT_OFFSET(struct turret_s, tag3, 0x43);


/* ---- pending entity pair (12 bytes): two entity indices and their serials ---- */
struct entity_pair_s {
    uint16 entityA;                   /* 0x00 */
    uint16 entityB;                   /* 0x02 */
    int serialA;                      /* 0x04 compared with gentity.spawnSerial */
    int serialB;                      /* 0x08 */
};
COD2_ASSERT_SIZE(struct entity_pair_s, 0x0c);

/* ---- turret input record (use/aim event), at least 0x24 bytes ---- */
struct turret_input_s {
    float fraction;                   /* 0x00 scale for (time - base) */
    float aim[3];                     /* 0x04 direction; aim[2] is tested against 0.1 and 0.7 */
    uint32 flags;                     /* 0x10 bits 0x1f00000 = seat index */
    float pad14[2];                   /* 0x14 */
    uint16 entityNum;                 /* 0x1c */
    int16 seatCmd;                    /* 0x1e */
    uint8 pad20[3];                   /* 0x20 */
    uint8 pendingFlag;                /* 0x23 */
};
COD2_ASSERT_OFFSET(struct turret_input_s, entityNum, 0x1c);
COD2_ASSERT_OFFSET(struct turret_input_s, pendingFlag, 0x23);

/* ---- small reference record: first word is a gentity pointer ---- */
struct entity_ref_s {
    struct gentity_s *ent;            /* 0x00 */
};



/* ---- per-seat turret use bank: 0x4b8 bytes at g_clientInfos, indexed by gentity.turretSlot ---- */
struct clientInfo_s {
    uint8 pad000[0x390];
    uint32 useFlags;                  /* 0x390 nonzero = in use; bit 0x200 cleared on reset */
    struct gentity_s *turret;         /* 0x394 */
    uint8 pad398[0x4a4 - 0x398];
    void *pose;                       /* 0x4a4 animation/pose handle */
    uint8 pad4a8[0x4b8 - 0x4a8];
};
COD2_ASSERT_SIZE(struct clientInfo_s, 0x4b8);
COD2_ASSERT_OFFSET(struct clientInfo_s, useFlags, 0x390);
COD2_ASSERT_OFFSET(struct clientInfo_s, pose, 0x4a4);

/* ---- globals (unnamed in the stripped binary) ---- */
extern int g_0864c598;                          /* sorted entity count */
extern int g_0864ceac;
extern int g_0864f96c;
extern uint8 g_087a21d0[];
extern uint8 g_dummyEntity[];
extern int g_087035a8;
extern uint32 g_087035ac;
extern uint32 g_087035b0;
extern uint32 g_087035b4;
extern uint32 g_087035b8;
extern int g_0870359c;
extern int g_087035a0;
extern int g_087035a4;

#endif
