/*
 * Types, globals and out-of-chunk prototypes for chunk 053 (0x081203d4..0x081240d0).
 * Covers weapon fire along a view frame, use and look-at trigger scans, the
 * message-stream codec used by the script/snapshot readers, script string
 * registration and the float curve builders.
 *
 * Only offsets that this chunk reads or writes are named. Structs whose full
 * extent is not proven by this chunk are asserted by offset only; their sizes
 * are listed as open questions in the symbols file.
 */
#ifndef COD2_CHUNK_053_TYPES_H
#define COD2_CHUNK_053_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* Reinterprets an untyped 32-bit argument pattern as float, e.g. 0x46000000 = 8192.0f. */
static inline float COD2_F32(uint32 bits)
{
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static inline int COD2_BITS(float f)
{
    int bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

#define COD2_ENTITYNUM_NONE 0x3ff
#define COD2_GENTITY_SIZE 0x230

typedef void (*cod2_impact_cb_t)(void *a, void *b, void *c);

/* gentity+0x1b4 = gentity_u @0x1a8 + 0xc (owner/entity number, no canonical view yet) */
#define GENT_OWNER_NUM(e) (*(int *)((unsigned char *)&(e)->u + 0xc))
#define CLIENT_TOUCHED_SET(c) ((unsigned int *)((unsigned char *)(c) + 0x544))
#define CLIENT_PENDING_SET(c) ((unsigned int *)((unsigned char *)(c) + 0x55c))

/* Entity fields reached by offset in this chunk but outside the struct above
 * (0x0c8 weapon-def handle, 0x0d8 use slot, 0x0dc use handle). */
#define GENT_WEAPON_DEF_HANDLE(e) (*(int *)((unsigned char *)(e) + 0x0c8))
#define GENT_USE_SLOT(e) (*(int *)((unsigned char *)(e) + 0x0d8))
#define GENT_USE_HANDLE(e) (*(int *)((unsigned char *)(e) + 0x0dc))

/* Weapon/object record returned by BG_GetWeaponDef(handle). Identity is not resolved
 * in this chunk; only the observed offsets are named. */
union num_value_u {
    int i;
    float f;
};
/* Shot frame passed to the fire routines: axes at 0x00/0x0c/0x18, origin at 0x24,
 * and the weapon record pointer at 0x3c. */
struct shot_view_s {
    float forward[3];              /* 0x00 */
    float right[3];                /* 0x0c */
    float up[3];                   /* 0x18 */
    float origin[3];               /* 0x24 */
    unsigned char pad30[0x3c - 0x30];
    struct weaponDef_s *weapon;      /* 0x3c */
};
COD2_ASSERT_OFFSET(struct shot_view_s, origin, 0x24);
COD2_ASSERT_OFFSET(struct shot_view_s, weapon, 0x3c);

/* Serialized script value: type and aux words, then the number at 0x08. */
struct msg_value_s {
    int type;                      /* 0x00 */
    int aux;                       /* 0x04 */
    union num_value_u value;       /* 0x08 */
};
COD2_ASSERT_OFFSET(struct msg_value_s, value, 8);

/* Scan result: entity pointer plus float sort key (8 bytes per entry). */
struct keyed_ent_s {
    struct gentity_s *ent;
    float key;
};
COD2_ASSERT_SIZE(struct keyed_ent_s, 8);
COD2_ASSERT_OFFSET(struct keyed_ent_s, key, 4);

/* Message byte buffer: data pointer at 0x00, cursize at 0x08. */
struct msgbuf_s {
    unsigned char *data;           /* 0x00 */
    unsigned char pad04[4];        /* 0x04 */
    int cursize;                   /* 0x08 */
};
COD2_ASSERT_OFFSET(struct msgbuf_s, cursize, 0x08);

/* Message stream codec state. Size unresolved beyond 0x18. */
struct msgstream_s {
    struct msgbuf_s *msg;          /* 0x00 underlying byte stream */
    unsigned char isReading;       /* 0x04 1 = decode, 0 = encode */
    unsigned char flag5;           /* 0x05 set by InitWrite */
    int bytesDone;                 /* 0x08 running byte count */
    int rawRemaining;              /* 0x0c literal bytes still to read raw */
    int zeroRemaining;             /* 0x10 zero bytes still to emit */
    int writePos;                  /* 0x14 write cursor */
};
COD2_ASSERT_OFFSET(struct msgstream_s, isReading, 0x04);
COD2_ASSERT_OFFSET(struct msgstream_s, bytesDone, 0x08);
COD2_ASSERT_OFFSET(struct msgstream_s, rawRemaining, 0x0c);
COD2_ASSERT_OFFSET(struct msgstream_s, zeroRemaining, 0x10);
COD2_ASSERT_OFFSET(struct msgstream_s, writePos, 0x14);

/* Impact table entry (stride 0x28): onTouch at +0, onUse at +4. */
struct entity_handler_s {
    cod2_impact_cb_t onTouch;      /* 0x00 */
    cod2_impact_cb_t onUse;        /* 0x04 */
    unsigned char pad08[0x28 - 0x08];
};
COD2_ASSERT_SIZE(struct entity_handler_s, 0x28);

/* Curve builder output: table pointer plus range (lo, hi). */
struct curve_ref_s {
    struct curve_table_s *curve;   /* 0x00 */
    float rangeLo;                 /* 0x04 */
    float rangeHi;                 /* 0x08 */
};
COD2_ASSERT_SIZE(struct curve_ref_s, 12);

/* Curve table header; rows follow at offset 8, stride (sampleCount + 1) floats. */
struct curve_table_s {
    int sampleCount;               /* 0x00 */
    int rowCount;                  /* 0x04 */
    float rows[1];                 /* 0x08 */
};
COD2_ASSERT_OFFSET(struct curve_table_s, rowCount, 4);
COD2_ASSERT_OFFSET(struct curve_table_s, rows, 8);

/* FxCurve_Build parameter block: pairs at 0x00..0x2c, midpoint pair at 0x30, flags at 0x38. */
struct fx_curve_spec_s {
    float pairs[12];               /* 0x00 */
    float midPair[2];              /* 0x30 */
    int flags;                     /* 0x38 raw bits: bit 0 linear/cos select, bits 0xc mode */
};
COD2_ASSERT_OFFSET(struct fx_curve_spec_s, midPair, 0x30);
COD2_ASSERT_OFFSET(struct fx_curve_spec_s, flags, 0x38);

/* ---- call-site adapters for the remaining loosely typed shared prototypes (no signature-fix request filed; see
 *      analysis/symbols/signature-requests.md, residual adapters) ---- */
#define FxRange_Set ((void (*)(float *dst, float lo, float hi))FxRange_Set)
#define FX_GetEffectByName ((int (*)(const char *path))FX_GetEffectByName)
#define Bullet_ApplySpread ((void (*)(int ownerNum, float *out, struct shot_view_s *view, float spreadOrRange))Bullet_ApplySpread)
#define Bullet_Fire_Extended ((void (*)(struct gentity_s *ent, int a, const float *origin, const float *dir, float scale, int c, struct shot_view_s *view, struct gentity_s *ent2, int e))Bullet_Fire_Extended)
#define FxRange_Lerp ((long double (*)(const float *pair, float t))FxRange_Lerp)
#define Model_RegisterByName ((int (*)(const char *s))Model_RegisterByName)



#define FX_ScriptStringToText_Stub ((int (*)(struct FxSystem *table, int handle))FX_ScriptStringToText_Stub)
#define XModel_GetName ((int (*)(int handle))XModel_GetName)
#define Bullet_FireFromEntity ((void (*)(struct gentity_s *ent, float *a, int b, int c, int d))Bullet_FireFromEntity)

#endif
