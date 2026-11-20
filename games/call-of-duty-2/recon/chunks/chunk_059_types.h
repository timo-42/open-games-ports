/*
 * chunk_059 types: effect/emitter object spawn helpers, small vec3 + field setter
 * wrappers, fixed-size object pools and the localization (loc_*) module.
 * Layouts are recovered from consistent offsets in 0x8135d72..0x8138e18.
 * Everything not observed in this chunk is left as unnamed padding.
 */
#ifndef COD2_CHUNK_059_TYPES_H
#define COD2_CHUNK_059_TYPES_H

#include "cod2_types.h"

struct Orient;
struct dvar_s;

/* ---- generic effect object (vtable at +0). Size is the largest allocation (0x29c). ---- */
struct Effect {
    void **vtbl;                 /* +0x000 slot 1 = deleting destructor (observed: (*vtbl[1])(this)) */
    vec3_t origin;               /* +0x004 FxObj_SetOrigin */
    int field_10;                /* +0x010 */
    vec3_t vec_14;               /* +0x014 */
    vec3_t vec_20;               /* +0x020 */
    int field_2c;                /* +0x02c */
    int field_30;                /* +0x030 */
    int field_34;                /* +0x034 */
    int field_38;                /* +0x038 */
    unsigned char pad_3c[4];
    int field_40;                /* +0x040 */
    int field_44;                /* +0x044 */
    vec3_t vec_48;               /* +0x048 */
    unsigned char pad_54[0xa8 - 0x54];
    unsigned int flags_a8;       /* +0x0a8 */
    unsigned char pad_ac[4];
    int field_b0;
    int field_b4;                /* +0x0b4 */
    int field_b8;                /* +0x0b8 */
    unsigned char pad_bc[8];
    float field_c4;              /* +0x0c4 */
    float field_c8;              /* +0x0c8 */
    unsigned char pad_cc[0xf4 - 0xcc];
    int field_f4;                /* +0x0f4 */
    int field_f8;                /* +0x0f8 */
    unsigned char pad_fc[4];
    int field_100;               /* +0x100 */
    unsigned char field_104;     /* +0x104 */
    unsigned char pad_105[3];
    int field_108;               /* +0x108 */
    int field_10c;
    int field_110;
    int field_114;
    float field_118;             /* +0x118 */
    float field_11c;
    float field_120;
    float field_124;
    int field_128;               /* +0x128 */
    unsigned char pad_12c[0x24c - 0x12c];
    vec3_t vec_24c;              /* +0x24c */
    vec3_t vec_258;              /* +0x258 */
    vec3_t vec_264;              /* +0x264 */
    int field_270;               /* +0x270 */
    unsigned char pad_274[4];
    vec3_t vec_278;              /* +0x278 */
    vec3_t vec_284;              /* +0x284 */
    int field_290;               /* +0x290 */
    float field_294;             /* +0x294 */
    float field_298;             /* +0x298 */
};
COD2_ASSERT_SIZE(struct Effect, 0x29c);
COD2_ASSERT_OFFSET(struct Effect, origin, 0x04);
COD2_ASSERT_OFFSET(struct Effect, vec_14, 0x14);
COD2_ASSERT_OFFSET(struct Effect, vec_20, 0x20);
COD2_ASSERT_OFFSET(struct Effect, field_40, 0x40);
COD2_ASSERT_OFFSET(struct Effect, vec_48, 0x48);
COD2_ASSERT_OFFSET(struct Effect, flags_a8, 0xa8);
COD2_ASSERT_OFFSET(struct Effect, field_b4, 0xb4);
COD2_ASSERT_OFFSET(struct Effect, field_c4, 0xc4);
COD2_ASSERT_OFFSET(struct Effect, field_f4, 0xf4);
COD2_ASSERT_OFFSET(struct Effect, field_100, 0x100);
COD2_ASSERT_OFFSET(struct Effect, field_104, 0x104);
COD2_ASSERT_OFFSET(struct Effect, field_118, 0x118);
COD2_ASSERT_OFFSET(struct Effect, field_128, 0x128);
COD2_ASSERT_OFFSET(struct Effect, vec_24c, 0x24c);
COD2_ASSERT_OFFSET(struct Effect, vec_258, 0x258);
COD2_ASSERT_OFFSET(struct Effect, vec_264, 0x264);
COD2_ASSERT_OFFSET(struct Effect, field_270, 0x270);
COD2_ASSERT_OFFSET(struct Effect, vec_278, 0x278);
COD2_ASSERT_OFFSET(struct Effect, vec_284, 0x284);
COD2_ASSERT_OFFSET(struct Effect, field_298, 0x298);

/* ---- effect definition (only the offsets read by this chunk) ---- */
struct FxRange { int words[2]; };     /* read with FxRange_Random -> float; stride 8 */
struct FxPickList { int words[2]; };     /* read with FxPickList_PickOut / FxPickList_Pick */

struct fx_primitive_s {
    unsigned char pad_00[0x58];
    struct FxRange range_58;          /* +0x058 */
    unsigned char pad_60[8];
    struct FxPickList value_68;          /* +0x068 */
    unsigned char pad_70[0x10];
    struct FxPickList value_80;          /* +0x080 */
    struct FxPickList value_88;          /* +0x088 */
    unsigned int flags_90;               /* +0x090 */
    unsigned char pad_94[9];
    unsigned char byte_9d;               /* +0x09d */
    unsigned char pad_9e[0xe8 - 0x9e];
    struct FxRange range_e8;          /* +0x0e8 */
    unsigned char pad_f0[0x228 - 0xf0];
    struct FxRange range_228;         /* +0x228 */
    struct FxRange range_230;
    struct FxRange range_238;
    struct FxRange range_240;
    struct FxRange range_248;
    struct FxRange range_250;
    unsigned char pad_258[8];
    struct FxRange range_260;         /* +0x260 */
    struct FxRange range_268;         /* +0x268 */
    unsigned char pad_270[0x280 - 0x270];
    struct FxRange range_280;         /* +0x280 */
};
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_58, 0x58);
COD2_ASSERT_OFFSET(struct fx_primitive_s, value_68, 0x68);
COD2_ASSERT_OFFSET(struct fx_primitive_s, value_80, 0x80);
COD2_ASSERT_OFFSET(struct fx_primitive_s, value_88, 0x88);
COD2_ASSERT_OFFSET(struct fx_primitive_s, flags_90, 0x90);
COD2_ASSERT_OFFSET(struct fx_primitive_s, byte_9d, 0x9d);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_e8, 0xe8);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_228, 0x228);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_238, 0x238);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_240, 0x240);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_250, 0x250);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_260, 0x260);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_268, 0x268);
COD2_ASSERT_OFFSET(struct fx_primitive_s, range_280, 0x280);

/* reference target (object an effect may be attached to); only +0x3c address is used */
struct FxBinding {
    unsigned char pad_00[0x3c];
    unsigned char field_3c[4];           /* address passed on, size unknown */
};
struct FxRef { int words[1]; };       /* optional attachment handle; size unknown */

/* spawn context passed to the Fx_Spawn* functions */
struct FxSpawner {
    int field_0;
    struct fx_primitive_s *def;                /* +0x04 */
    struct FxRef ref;                 /* +0x08 */
};
COD2_ASSERT_OFFSET(struct FxSpawner, def, 4);
COD2_ASSERT_OFFSET(struct FxSpawner, ref, 8);

/* transform scratch filled by FxSpawner_ComputeOrigin; origin first (observed), >= 24 bytes (28 reserved) */
struct fx_xform_s {
    vec3_t origin;
    unsigned char rest[16];
};

/* ---- clock / manager / pending event list ---- */
struct FxSystem { int field_0; int time; };    /* time at +4 */

struct fx_pending_s {
    struct fx_emitter_s *owner;            /* +0x00 */
    int slot_index;                      /* +0x04 */
    int time;                            /* +0x08 */
    int field_0c;                        /* +0x0c sign tested */
    unsigned char rest_10[4];
    unsigned char field_14[12];          /* +0x14 */
    unsigned char field_20[36];          /* +0x20 */
    unsigned int seed;                   /* +0x44 */
    int arg_48;                          /* +0x48 */
    struct fx_pending_s *next;           /* +0x4c */
};
COD2_ASSERT_SIZE(struct fx_pending_s, 0x50);
COD2_ASSERT_OFFSET(struct fx_pending_s, time, 8);
COD2_ASSERT_OFFSET(struct fx_pending_s, field_0c, 0xc);
COD2_ASSERT_OFFSET(struct fx_pending_s, field_14, 0x14);
COD2_ASSERT_OFFSET(struct fx_pending_s, field_20, 0x20);
COD2_ASSERT_OFFSET(struct fx_pending_s, seed, 0x44);
COD2_ASSERT_OFFSET(struct fx_pending_s, next, 0x4c);

struct fx_emitter_s { int field_0; int field_4; int slot[1]; };  /* slot[] at +8 */
COD2_ASSERT_OFFSET(struct fx_emitter_s, slot, 8);

struct FxScheduler { int field_0; struct fx_pending_s *pending_head; int pending_count; };
COD2_ASSERT_OFFSET(struct FxScheduler, pending_head, 4);
COD2_ASSERT_OFFSET(struct FxScheduler, pending_count, 8);

/* ---- fixed-size object pools (blocks of 0x8000 bytes, bookkeeping at the end) ---- */
struct fx_pool_free_s { struct fx_pool_free_s *next; };
struct FxPoolBlock {
    unsigned char data[0x7ff0];
    int free_count;                      /* +0x7ff0 */
    struct fx_pool_free_s *free_head;    /* +0x7ff4 */
    struct FxPoolBlock *next;        /* +0x7ff8 */
};
COD2_ASSERT_OFFSET(struct FxPoolBlock, free_count, 0x7ff0);
COD2_ASSERT_OFFSET(struct FxPoolBlock, free_head, 0x7ff4);
COD2_ASSERT_OFFSET(struct FxPoolBlock, next, 0x7ff8);
struct FxPool { int block_arg; struct FxPoolBlock *head; };

/* ---- localization ---- */
struct cod2_lang_entry_s { const char *name; unsigned int reserved; };  /* stride 8; 15 languages; reserved = availability flag at runtime */
COD2_ASSERT_SIZE(struct cod2_lang_entry_s, 8);

/* bool flag variable object (g_08a53c80): byte value at +8 */
struct FxFlagVar { int word0; int word4; unsigned char value; };


/* ---- globals (placeholders keep their address) ---- */
extern unsigned char g_08a5c9e0[2][0xfc];


/* dvar value views (value word at +8; name pointer at +0) */
static inline int Dvar_ValueInt(const struct dvar_s *d) { return *(const int *)((const char *)d + 8); }
static inline char Dvar_ValueBool(const struct dvar_s *d) { return *(const char *)((const char *)d + 8); }
static inline const char *Dvar_NameOf(const struct dvar_s *d) { return *(const char *const *)d; }

/* ---- out-of-chunk callees (merged names where known, else fn_08xxxxxx) ---- */
extern "C++" {

/* Member functions of chunk 054 (Itanium ABI: this passed as first stack argument, same as a plain
 * cdecl call); bound by asm label because chunk 054 defines them as class members. */
void Particle_SetBasis(struct Effect *o, const float *axes3) __asm__("_ZN8Particle8SetBasisEPKf");  /* 0x08124fd4 */
void Particle_GetStartVelocity(struct Effect *o, float *out) __asm__("_ZN8Particle16GetStartVelocityEPf");  /* 0x08126252 */
void Tail_UpdateLengthAndPoint(struct Effect *o) __asm__("_ZN4Tail20UpdateLengthAndPointEv");  /* 0x081279e4 */
}

/* ---- chunk functions needed across the file ---- */

#endif
