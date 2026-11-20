/*
 * Chunk 102 (cod2_lnxded 1.3): 80 functions recovered from .eh_frame FDE starts between
 * 0x080b30a0 and 0x080fa1b6 (Ghidra auto-analysis missed them). They belong to the Dvar,
 * message-buffer, token-parser, DObj/XAnim/XModel, Sys/dynamic-loader, weapon-def and
 * player-entity script subsystems.
 *
 * Layouts below are recovered from consistent offsets in the disassembly. Only offsets that
 * are used by this chunk are named; struct sizes are asserted only where another lane or the
 * array stride fixes them. Out-of-chunk callees without a merged name are declared as
 * fn_08xxxxxx placeholders for the merge queue.
 */
#ifndef COD2_CHUNK_102_TYPES_H
#define COD2_CHUNK_102_TYPES_H

struct gameLocals_s;
struct parse_session_s;
struct weaponDef_s;
struct weaponParse_s;
struct DObj_s;

/* Message buffer. Layout matches chunk 029 (data, maxsize, cursor, warn flag, overflow flag). */
struct msgbuf_s {
    uint8 *data;                 /* +0x00 */
    int32 maxsize;               /* +0x04 */
    int32 cursor;                /* +0x08 read position */
    uint8 warn_on_overflow;      /* +0x0c */
    uint8 overflowed;            /* +0x0d */
};
COD2_ASSERT_OFFSET(struct msgbuf_s, data, 0x00);
COD2_ASSERT_OFFSET(struct msgbuf_s, maxsize, 0x04);
COD2_ASSERT_OFFSET(struct msgbuf_s, cursor, 0x08);
COD2_ASSERT_OFFSET(struct msgbuf_s, warn_on_overflow, 0x0c);
COD2_ASSERT_OFFSET(struct msgbuf_s, overflowed, 0x0d);

/* Weapon-definition parse target for WeaponDef_ParseEnumField. */
struct weaponParse_s {
    const char *name;            /* +0x00 */
    byte pad_004[0x74 - 0x04];
    int32 playerAnimType;        /* +0x74 */
    int32 weaponType;            /* +0x78 */
    int32 weaponClassEnum;       /* +0x7c */
    int32 weaponSlotEnum;        /* +0x80 */
    int32 offhandClass;          /* +0x84 */
    byte pad_088[0x8c - 0x88];
    int32 stance;                /* +0x8c */
    byte pad_090[0x278 - 0x90];
    int32 overlayReticle;        /* +0x278 */
    byte pad_27c[0x390 - 0x27c];
    int32 projectileExplosion;   /* +0x390 */
};
COD2_ASSERT_OFFSET(struct weaponParse_s, playerAnimType, 0x74);
COD2_ASSERT_OFFSET(struct weaponParse_s, weaponType, 0x78);
COD2_ASSERT_OFFSET(struct weaponParse_s, weaponSlotEnum, 0x80);
COD2_ASSERT_OFFSET(struct weaponParse_s, offhandClass, 0x84);
COD2_ASSERT_OFFSET(struct weaponParse_s, stance, 0x8c);
COD2_ASSERT_OFFSET(struct weaponParse_s, overlayReticle, 0x278);
COD2_ASSERT_OFFSET(struct weaponParse_s, projectileExplosion, 0x390);

/* Partial 8-byte node view used by the XAnim node table (node k at table + 0x0c + 8k). */
struct xanim_node_chunk102_s {
    uint16 child_count;          /* +0x00 */
    int16 aux_index;             /* +0x02 */
    uint16 field04;              /* +0x04 */
    uint16 first_child;          /* +0x06 */
};
COD2_ASSERT_SIZE(struct xanim_node_chunk102_s, 8);
COD2_ASSERT_OFFSET(struct xanim_node_chunk102_s, first_child, 6);

/* DObj part block referenced from DObj +0x1c (one per part group). */
struct dobj_part_block_s {
    uint16 count;                /* +0x00 */
    uint16 skip;                 /* +0x02 */
    uint8 *values;               /* +0x04 values start at +4 */
};
COD2_ASSERT_OFFSET(struct dobj_part_block_s, skip, 2);
COD2_ASSERT_OFFSET(struct dobj_part_block_s, values, 4);

/* DObj slot table entry at +0x1c + 4k points to a group object whose first field is the block. */
struct dobj_part_group_s {
    const struct dobj_part_block_s *block;   /* +0x00 */
};

/* Blend slot pool entry (g_xanimSlotRing[k], 0x28 bytes) is addressed by byte offset here. */
#define COD2_BLEND_SLOT_STRIDE 0x28

/* chunk-local view of the shared BG_GetWeaponDef with the canonical weaponDef_s layout. */
static inline struct weaponDef_s *BG_GetWeaponDef102(int32 weapon) { return (struct weaponDef_s *)BG_GetWeaponDef(weapon); }

#endif
