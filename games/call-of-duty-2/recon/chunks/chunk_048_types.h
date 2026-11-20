/*
 * Chunk 048 (cod2_lnxded 1.3, 0x08103618..0x08109096) types and out-of-chunk prototypes.
 *
 * Covered subsystems: HUD element pool (g_hudelems, stride 0x8c) and script
 * field table (g_hudelemFieldTable, stride 0x1c); player ammo/clip bookkeeping; item and
 * weapon pickup/drop; g_* dvar registration (0x81084a4); item registry.
 *
 * Layouts marked "partial" in the generated section name only the offsets this
 * chunk reads or writes. Merge lane: gentity_s, gclient_s, gitem_s and
 * weaponDef_s are candidates for promotion to cod2_types.h once other chunks
 * agree on the same offsets.
 */
#ifndef COD2_CHUNK_048_TYPES_H
#define COD2_CHUNK_048_TYPES_H

/* ---- forward declarations ---- */
struct gentity_s;
struct gclient_s;
struct game_hudelem_s;
struct gitem_s;
struct weaponDef_s;
struct ent_ext_s;
struct dvar_s;     /* dvar_s: int/float/bool value at +0x08 (read at that offset by every dvar use in this chunk) */
struct DObj_s;

typedef void (*hudelem_method_fn)(void);





/* Script field table entry (stride 0x1c), terminated by name == NULL. */
struct hudelem_field_s {
    const char *name;                       /* 0x00 */
    int offset;                             /* 0x04 byte offset inside game_hudelem_s */
    int type;                               /* 0x08 3 = script string field */
    unsigned int mask;                      /* 0x0c packed-field mask */
    unsigned char shift;                    /* 0x10 packed-field shift */
    unsigned char pad_11[3];
    void (*getter)(struct game_hudelem_s *elem, int field);  /* 0x14 */
    void (*setter)(struct game_hudelem_s *elem, int field);  /* 0x18 */
};
COD2_ASSERT_SIZE(struct hudelem_field_s, 0x1c);
COD2_ASSERT_OFFSET(struct hudelem_field_s, offset, 0x04);
COD2_ASSERT_OFFSET(struct hudelem_field_s, type, 0x08);
COD2_ASSERT_OFFSET(struct hudelem_field_s, mask, 0x0c);
COD2_ASSERT_OFFSET(struct hudelem_field_s, shift, 0x10);
COD2_ASSERT_OFFSET(struct hudelem_field_s, getter, 0x14);
COD2_ASSERT_OFFSET(struct hudelem_field_s, setter, 0x18);

/* Script method table entry (stride 0x0c), searched by name. */
struct hudelem_method_s {
    const char *name;                       /* 0x00 */
    hudelem_method_fn handler;              /* 0x04 */
    unsigned int unk_08;                    /* 0x08 unused by lookup */
};
COD2_ASSERT_SIZE(struct hudelem_method_s, 0x0c);

/* Generated layouts (partial where noted). Sizes marked by COD2_ASSERT_SIZE are proven by stride use. */




/* item/weapon definition table entry at g_081850c0 (stride 0x2c); partial */
struct gitem_s {
    const char * itemName;
    unsigned char pad_0004[0x4];
    const char * model;
    const char * model2;
    unsigned char pad_0010[0x4];
    const char * name;
    int defaultAmount;
    int itemType;
    int weaponIndex;
    unsigned char pad_0024[0x8];
};


/* per-slot record at g_trackedEntitySlots (stride 0x4c8, 8 slots); partial */
struct ent_ext_s {
    int handle_00;
    unsigned char pad_0004[0x8];
    unsigned char objState[1208];
    unsigned char doneFlag;
    unsigned char pad_04c5[0x3];
};
COD2_ASSERT_SIZE(struct ent_ext_s, 0x4c8);
COD2_ASSERT_OFFSET(struct ent_ext_s, handle_00, 0x0);
COD2_ASSERT_OFFSET(struct ent_ext_s, objState, 0xc);
COD2_ASSERT_OFFSET(struct ent_ext_s, doneFlag, 0x4c4);


/* ---- out-of-chunk globals (merged name when known, else g_08xxxxxx) ---- */
extern int level_time;                              /* level time (ms) */
extern gclient_s *level;                       /* client array base (stride 0x28a4) */
extern int level_num_entities;                              /* level.num_entities */
extern int g_0864c3a0;
extern gentity_s g_dummyEntity;
extern unsigned char g_087a209c;

/* g_* dvar pointers registered by the server init (0x81084a4) */
extern struct dvar_s *g_087a2450, *g_087a2454, *g_087a2458, *g_087a245c, *g_087a2460;
extern struct dvar_s *g_087a247c, *g_087a2480, *g_087a2484, *g_087a248c, *g_087a2490;
extern struct dvar_s *g_087a2494, *g_087a2498, *g_087a249c, *g_087a24a0, *g_087a24a4;

/* ---- out-of-chunk callees with merged names (symbols.json) ---- */

/* ---- out-of-chunk callees, placeholder names (rewritten by the merge queue) ---- */

/* ---- Out-of-chunk globals not present in cod2_globals.h ---- */
extern struct dvar_s *g_087a2404;
extern struct dvar_s *g_087a2414;
extern struct dvar_s *g_087a241c;
extern struct dvar_s *g_087a242c;
extern struct dvar_s *g_087a2430;
extern struct dvar_s *g_087a243c;
extern struct dvar_s *g_087a2440;
extern struct dvar_s *g_087a2444;
extern struct dvar_s *g_087a2464;
extern struct dvar_s *g_087a2468;
extern struct dvar_s *g_087a24a8;
extern struct dvar_s *g_087a24ac;
extern struct dvar_s *g_087a24b4;
extern struct dvar_s *dvar_voteMode;

#endif
