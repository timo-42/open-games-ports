/*
 * chunk 050 (0x0810e024..0x08118260) types, globals and out-of-chunk prototypes.
 * Entity/mover/missile helpers plus the game-script (GSC) builtin glue.
 * Layouts are recovered from consistent offsets in this chunk only; unnamed
 * fields are padding or fields not touched here.
 */
#ifndef COD2_CHUNK_050_TYPES_H
#define COD2_CHUNK_050_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"




/* Per-entClass callback table, stride 0x28, base 0x08187d00. */
typedef struct entity_handler_s {
    int unk_00;                                             /* 0x00 */
    void (*onTrajectoryEnd)(gentity_t *ent);           /* 0x04 */
    void (*onBlocked)(gentity_t *ent, gentity_t *obstacle); /* 0x08 */
    byte unk_0c[0x14];                                      /* 0x0c */
    int kind;                                               /* 0x20: 3 is tested by the missile runner */
    byte unk_24[4];                                         /* 0x24 */
} entity_handler_s;
COD2_ASSERT_SIZE(entity_handler_s, 0x28);
COD2_ASSERT_OFFSET(entity_handler_s, onTrajectoryEnd, 4);
COD2_ASSERT_OFFSET(entity_handler_s, onBlocked, 8);
COD2_ASSERT_OFFSET(entity_handler_s, kind, 0x20);


/* Mover push undo record, 0x20 bytes. */
typedef struct pushed_s {
    gentity_t *ent;        /* 0x00 */
    vec3_t origin;              /* 0x04 */
    byte unk_10[0xc];           /* 0x10 */
    float deltaYaw;             /* 0x1c */
} pushed_t;
COD2_ASSERT_SIZE(pushed_t, 0x20);
COD2_ASSERT_OFFSET(pushed_t, deltaYaw, 0x1c);

/* Gametype list entry, 0x84 bytes, 0x20 slots at 0x0884d72c. */
typedef struct cod2_gametype_s {
    char name[0x40];            /* 0x00 */
    char description[0x40];     /* 0x40 */
    int teamBased;              /* 0x80 */
} cod2_gametype_s;
COD2_ASSERT_SIZE(cod2_gametype_s, 0x84);
COD2_ASSERT_OFFSET(cod2_gametype_s, description, 0x40);
COD2_ASSERT_OFFSET(cod2_gametype_s, teamBased, 0x80);

/* Script builtin table entry (name, handler, flag), 12 bytes. */
typedef struct cod2_scrfunc_s {
    const char *name;           /* 0x00 */
    void (*handler)(void);      /* 0x04 */
    int developer;              /* 0x08 */
} cod2_scrfunc_s;
COD2_ASSERT_SIZE(cod2_scrfunc_s, 12);


/* globals absent from cod2_globals.h under these names (defined by the data image at the original addresses) */
extern int g_0884d70c, g_0884d710, g_0884d714, g_0884d718, g_0884d71c; /* CodeCallback_* offsets */
extern int g_0884e7b0, g_0884e7b4;          /* codescripts initstructs/createstruct */

#endif
