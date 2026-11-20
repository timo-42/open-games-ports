/*
 * Types, globals and out-of-chunk prototypes for chunk 045 (0x080f14aa..0x080f5682).
 *
 * Content: first-person weapon view helpers (bob, kick springs, idle sway), WeaponDef file
 * loading and its field-table parser, AI accuracy graph files, box/entity trace sweeps,
 * entity touch dispatch, client button edges, inactivity drop and player event dispatch.
 *
 * Layouts: weaponDef_s is recovered from the weapon field table in rodata at 0x08186a60
 * (names are observed strings); playerState_s, gentity_s and gclient_s are recovered from the
 * field offsets used by this chunk only. gentity_s uses a 0x230 stride (g_gentities) and
 * gclient_s a 0x28a4 stride (level_clients); both are derived from the index arithmetic in the
 * binary. Other chunks describe gentity_s differently (see open questions).
 */
#ifndef COD2_CHUNK_045_TYPES_H
#define COD2_CHUNK_045_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include <strings.h>

struct playerState_s;
struct gentity_s;
struct gclient_s;
struct weaponDef_s;
struct view_motion_s;
struct trace_s;
struct kv_table_s;
struct kv_pair_s;
struct entity_handler_s;
struct accuracy_knot_s;
struct weapon_field_desc_s;
struct client_think_ctx_s;
struct touch_list_s;
struct view_spring_s;

typedef void (*gentity_touch_fn)(struct gentity_s *self, struct gentity_s *other, int trace);

/* ---- playerstate, gentity, gclient, weapon layouts (generated from field offsets) ---- */
/* Entity record, stride 0x230 (g_gentities array). Layout recovered from field offsets. */
/* gentity_s: canonical, see cod2_canon.h */
/* Player movement/view state reached through view_motion_s.ent (size not recovered; fields up to 0x5b8 used). */
/* playerState_s: canonical, see cod2_canon.h */
/* Client state, stride 0x28a4 (g_clients array). Only fields used by chunk 045 are named. */
/* gclient_s: canonical, see cod2_canon.h */
/* Weapon definition (WeaponDef), 0x604 bytes. Field names and types come from the weapon file parser table at 0x08186a60. */
/* weaponDef_s: canonical, see cod2_canon.h */

/* ---- small layouts ----------------------------------------------------------------- */
struct accuracy_knot_s {
    float x;                    /* 0x00 */
    float y;                    /* 0x04 */
};
COD2_ASSERT_SIZE(accuracy_knot_s, 8);

struct weapon_field_desc_s {
    const char *name;           /* 0x00 field name (observed) */
    int32 offset;               /* 0x04 offset inside weaponDef_s */
    int32 type;                 /* 0x08 parse type: 0 string, 6 float, others int-like */
};
COD2_ASSERT_SIZE(weapon_field_desc_s, 12);

/* Entity class callbacks: 0x28-byte records at g_08187d0c, indexed by gentity_s.entity_class. */
struct entity_handler_s {
    gentity_touch_fn touch;     /* 0x00 */
    void *slot_04[9];           /* 0x04..0x27 other callbacks (not called from this chunk) */
};
COD2_ASSERT_SIZE(entity_handler_s, 0x28);

/* Key/value block parsed by KeyValue_ParseBlock. */
struct kv_pair_s {
    const char *key;            /* 0x00 */
    const char *value;          /* 0x04 */
};
struct kv_table_s {
    byte parsed;                /* 0x00 set to 1 after a complete block */
    byte unk_01[3];             /* 0x01..0x03 */
    int32 count;                /* 0x04 number of pairs (max 0x40) */
    struct kv_pair_s pairs[0x40]; /* 0x08 */
    int32 pool_used;            /* 0x208 bytes used in pool */
    char pool[0x800];           /* 0x20c string pool */
};
COD2_ASSERT_OFFSET(kv_table_s, count, 4);
COD2_ASSERT_OFFSET(kv_table_s, pairs, 8);
COD2_ASSERT_OFFSET(kv_table_s, pool_used, 0x208);
COD2_ASSERT_OFFSET(kv_table_s, pool, 0x20c);
COD2_ASSERT_SIZE(kv_table_s, 0xa0c);

/* Trace result (9 words). The plane_dist_or_entity word carries the owner entity number in its
 * low 16 bits (set by Trace_SweepEntities); flags word bytes 2 and 3 are startsolid/allsolid. */
/* trace_s: canonical, see cod2_canon.h (byte 0x22 allsolid, 0x23 startsolid) */

/* Raw views of the entity record used by the sweep helpers; the canonical gentity_s names these
 * offsets differently (+0xf0 is read as a dword here, +0x18c as an integer, +0x1ec/+0x1f8 as vec3). */
#define ENT_RAW_I32(e, off) (*(int32 *)((byte *)(e) + (off)))
#define ENT_BOXA(e) ((float *)((byte *)(e) + 0x1ec))
#define ENT_BOXB(e) ((float *)((byte *)(e) + 0x1f8))

/* Per-player first-person view motion state. Slot names are generic: the same integer slot is
 * used with different meanings by different helpers (see open questions). */
struct view_motion_s {
    struct playerState_s *ps;   /* 0x00 */
    int32 slot_04;              /* 0x04 timed kick A start (zero = inactive) / bob scale */
    int32 slot_08;              /* 0x08 timed kick A current / remaining time */
    int32 slot_0c;              /* 0x0c kick A pitch */
    int32 slot_10;              /* 0x10 kick A yaw */
    int32 slot_14;              /* 0x14 bob amplitude */
    int32 slot_18;              /* 0x18 slew rate for the sway slot */
    int32 slot_1c;              /* 0x1c slewed sway value / kick B current time */
    union { int32 value; int32 *ptr; } slot_20; /* 0x20 kick B start time / phase counter pointer */
    int32 slot_24;              /* 0x24 kick B yaw */
    int32 slot_28;              /* 0x28 kick B pitch */
    float pos[3];              /* 0x2c gun kick position (spring) */
    float vel[3];              /* 0x38 gun kick velocity (spring) */
    float angle_target[2];      /* 0x44 angle targets */
};
COD2_ASSERT_OFFSET(view_motion_s, slot_20, 0x20);
COD2_ASSERT_OFFSET(view_motion_s, pos, 0x2c);
COD2_ASSERT_OFFSET(view_motion_s, vel, 0x38);
COD2_ASSERT_OFFSET(view_motion_s, angle_target, 0x44);

/* Context passed to the player move hook from Client_ProcessButtonsAndDrop. */
struct client_think_ctx_s {
    struct gclient_s *client;   /* 0x00 */
    int32 cmd[7];               /* 0x04 copy of the 7-word command */
    byte unk_20[0x1c];          /* 0x20..0x3b */
    int32 unk_3c;               /* 0x3c set to 0x800811 */
    byte unk_40[0xa4];          /* 0x40..0xe3 */
    byte flag_e4;               /* 0xe4 set to 1 */
    byte unk_e5[0x13];          /* 0xe5..0xf7 */
};
COD2_ASSERT_SIZE(client_think_ctx_s, 0xf8);

/* Touch list: count at 0x40, entity numbers follow at 0x44. */
struct touch_list_s {
    byte unk_00[0x40];
    int32 count;                /* 0x40 */
};
COD2_ASSERT_OFFSET(touch_list_s, count, 0x40);

/* Two-float spring state used by WeaponView_ApplyAngleSpring (float slots at +4 and +8). */
struct view_spring_s {
    float unk_00;
    float horiz;                /* 0x04 */
    float vert;                 /* 0x08 */
};
COD2_ASSERT_OFFSET(view_spring_s, horiz, 4);
COD2_ASSERT_OFFSET(view_spring_s, vert, 8);

/* ---- globals ----------------------------------------------------------------------- */

/* Out-of-chunk callees (Com_ParseInfoFields, CM_BoxTrace, Sweep_TraceBoxWithOffsetInit, player_die, G_Damage) use the
 * shared prototypes in cod2_prototypes.h; their local re-declarations were removed. */
#endif
