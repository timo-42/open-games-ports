/*
 * chunk 104 types: 89 functions recovered from .eh_frame FDE starts, 0x080fda40 .. 0x0811168e.
 * Layouts are partial: only fields these functions touch are named; the rest is padding.
 * Offsets were read from the decompiled pseudocode in .local/cod2/decomp/functions_eh and checked
 * against objdump for the ambiguous functions (float vs double, FPU rounding, argument counts).
 */
#ifndef COD2_CHUNK_104_TYPES_H
#define COD2_CHUNK_104_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

struct turret_s;
struct hudelem_field_s;

/* ---- turret (partial; no canonical definition) ---- */
struct turret_s {
    uint8 pad00[0x04];
    uint32 flags04;                   /* 0x04 bit 0x800 set while occupied */
    uint8 pad08[0x0c - 0x08];
    float arc0c;                      /* 0x0c yaw lower limit */
    float arc10;                      /* 0x10 pitch lower limit */
    float off14;                      /* 0x14 yaw offset */
    float off18;                      /* 0x18 pitch offset */
    float aimOffset;                  /* 0x1c first clear aim step */
    int mode;                         /* 0x20 0/1/2 */
    int aimType;                      /* 0x24 */
    uint8 pad28[0x2c - 0x28];
    float origin[3];                  /* 0x2c */
};
COD2_ASSERT_OFFSET(struct turret_s, flags04, 0x04);
COD2_ASSERT_OFFSET(struct turret_s, aimOffset, 0x1c);
COD2_ASSERT_OFFSET(struct turret_s, mode, 0x20);
COD2_ASSERT_OFFSET(struct turret_s, origin, 0x2c);

/* ---- hud field table record (stride 0x1c at 0x08159ee0): hudelem_field_s (0x14) + two setters ---- */
struct hudfield104_s {
    const char *name;                 /* 0x00 */
    int offset;                       /* 0x04 byte offset inside hudelem_s */
    int kind;                         /* 0x08 */
    uint32 mask;                      /* 0x0c */
    uint8 shift;                      /* 0x10 */
    uint8 pad11[3];
    void *setFn;                      /* 0x14 */
    void *altSetFn;                   /* 0x18 */
};
COD2_ASSERT_SIZE(struct hudfield104_s, 0x1c);
COD2_ASSERT_OFFSET(struct hudfield104_s, offset, 0x04);
COD2_ASSERT_OFFSET(struct hudfield104_s, setFn, 0x14);

/* ---- weapon definition fields are read through raw offsets (WeaponDef043_s is not complete here) ---- */
#define WD104_FIELD(def, type, off) (*(type *)((uint8 *)(def) + (off)))

/* ---- bg item (stride 0x2c): item type dword at +0x1c ---- */
#define BG_ITEM104_TYPE(idx) (*(int *)((uint8 *)g_081850c0 + (size_t)(idx) * 0x2c + 0x1c))


/* hud field table record view (the canonical hudelem_field_s is still incomplete) */
#define HUDFIELD104(i) (((struct hudfield104_s *)g_hudelemFieldTable)[(i)])


#endif /* COD2_CHUNK_104_TYPES_H */
