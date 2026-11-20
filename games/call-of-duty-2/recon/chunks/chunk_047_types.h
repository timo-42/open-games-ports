/*
 * Types, globals and out-of-chunk prototypes for chunk 047 (0x080fe9ca..0x081034de).
 * Content: game-module client commands (give/take/god/noclip/follow/say/callvote/vote/...),
 * damage and death handling (G_Damage, player_die, radius damage, hit-location table),
 * debug circle/arc helpers and the hudelem pool. Layouts are recovered from the offsets
 * used by this chunk only (partial structs; padding is explicit).
 */
#ifndef COD2_CHUNK_047_TYPES_H
#define COD2_CHUNK_047_TYPES_H

#include "cod2_types.h"









/* Field descriptor consumed by Com_ParseInfoFields (Com_ParseInfoFields). */
struct InfoField_s {
    const char *name;  /* 0x0 */
    int offset;  /* 0x4 */
    int type;  /* 0x8 6 observed for all hitloc fields */
};
COD2_ASSERT_OFFSET(InfoField_s, name, 0x0);
COD2_ASSERT_OFFSET(InfoField_s, offset, 0x4);
COD2_ASSERT_OFFSET(InfoField_s, type, 0x8);
COD2_ASSERT_SIZE(InfoField_s, 0xc);

/* Bit-field descriptor used by HudElem_SetEnumField. */
struct hudelem_field_s {
    const char *name;  /* 0x0 */
    int offset;  /* 0x4 */
    int field_08;  /* 0x8 */
    uint32 mask;  /* 0xc */
    uint8 shift;  /* 0x10 */
    char pad_0011[0x3];
};
COD2_ASSERT_OFFSET(hudelem_field_s, name, 0x0);
COD2_ASSERT_OFFSET(hudelem_field_s, offset, 0x4);
COD2_ASSERT_OFFSET(hudelem_field_s, field_08, 0x8);
COD2_ASSERT_OFFSET(hudelem_field_s, mask, 0xc);
COD2_ASSERT_OFFSET(hudelem_field_s, shift, 0x10);
COD2_ASSERT_SIZE(hudelem_field_s, 0x14);

/* Pain/die callbacks; array element k lives at g_entityHandlers + 0x28*k. */
struct entity_handler_s {
    void (*pain)(struct gentity_s *self, struct gentity_s *attacker, int damage, const float *point, int mod, const float *dir, int hitLoc);  /* 0x0 */
    void (*die)(struct gentity_s *self, struct gentity_s *inflictor, struct gentity_s *attacker, int damage, int mod, int weapon, const float *dir, int hitLoc, int extra);  /* 0x4 */
    char pad_0008[0x20];
};
COD2_ASSERT_OFFSET(entity_handler_s, pain, 0x0);
COD2_ASSERT_OFFSET(entity_handler_s, die, 0x4);
COD2_ASSERT_SIZE(entity_handler_s, 0x28);

/* ---- Globals (placeholder names are address-keyed; meanings in comments are inferred) ---- */
extern struct gentity_s g_entities[];        /* game entity array, stride 0x230 */
extern struct gentity_s g_dummyEntity;          /* fallback entity used when inflictor/attacker is NULL */
extern struct gclient_s *level;         /* level clients array, stride 0x28a4 */
extern int level_num_entities;                       /* level entity count */
extern int level_time;                       /* level time (ms) */
extern char level_voteString[0x400];               /* vote command string */
extern struct dvar_s *dvar_voteMode;            /* selects script-vote path when clear */
extern struct dvar_s *dvar_cheats;            /* cheats enabled */
extern struct dvar_s *g_087a242c;            /* damage debug print */
extern struct dvar_s *dvar_gametype;            /* current gametype (string) */
extern struct dvar_s *g_087a247c;            /* chat across dead/alive allowed */
extern uint16 g_scrCallbackMenuResponse;                    /* script callback handles */
extern uint16 g_scrCallbackDeath;
extern uint16 g_scrCallbackDamage;

#define ENTNUM(e) ((int)((e) - g_entities))

/* ---- Out-of-chunk callees (placeholders rewritten by the merge lane) ---- */

/* Merged names from other chunks (signatures as used here). */

/* ---- In-chunk functions ---- */

#endif /* COD2_CHUNK_047_TYPES_H */
