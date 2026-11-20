/*
 * Types, globals and out-of-chunk prototypes for chunk 040 (0x080dafa6..0x080df948).
 *
 * Contents: player state and view state used by the animation, sway, jump and
 * mantle code; the mantle stage table; BG trajectory evaluation; item table
 * lookups; the dvar (cvar) records registered by this chunk.
 *
 * Layouts are recovered from offsets used by the 80 functions of this chunk
 * only. Bytes not exercised here stay as named padding. The merge lane must
 * check these layouts against the other chunks before promoting them.
 */
#ifndef COD2_CHUNK_040_TYPES_H
#define COD2_CHUNK_040_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ---------------------------------------------------------------------------
 * Global engine block. g_gameContextPtr (image label 0x0860b420) is a pointer to a large global
 * block. This chunk touches the multiplayer animation slots at +0xb3bb4..+0xb3bf8
 * and three function-pointer slots at +0xb3bec, +0xb3bf0 and +0xb3bf4.
 * ------------------------------------------------------------------------- */
/* g_gameContextPtr (uint8 * in cod2_globals.h) is the animation/global block pointer used below. */

/* Raw word access into the global block at a fixed offset. */
#define COD2_G_U32(off) (*(unsigned int *)(g_gameContextPtr + (off)))
#define COD2_G_U16(off) (*(unsigned short *)(g_gameContextPtr + (off)))

/* Slot offsets inside the global block (names reflect observed use). */
#define COD2_G_MP_TREE_COPY   0xb3bb4u  /* u32: copy of the "multiplayer" tree handle */
#define COD2_G_MP_TORSO_COPY  0xb3bb8u  /* u16: copy of the torso anim index */
#define COD2_G_MP_LEGS_COPY   0xb3bbau  /* u16: copy of the legs anim index */
#define COD2_G_MP_TURN_COPY   0xb3bbcu  /* u16: copy of the turning anim index */
#define COD2_G_MP_TREE        0xb3bc8u  /* u32: "multiplayer" tree handle */
#define COD2_G_MP_ROOT_SLOT   0xb3bccu  /* u32 out: "root" anim slot */
#define COD2_G_MP_TORSO_SLOT  0xb3bd0u  /* u32 out: "torso" anim slot */
#define COD2_G_MP_LEGS_SLOT   0xb3bd4u  /* u32 out: "legs" anim slot */
#define COD2_G_MP_TURN_SLOT   0xb3bd8u  /* u32 out: "turning" anim slot */
#define COD2_G_MP_FLAGS       0xb3be8u  /* int: flags argument of anim lookups */
#define COD2_G_MP_SET         0xb3bf8u  /* u32: anim set handle */
#define COD2_G_FN_HANDLE      0xb3becu  /* fn ptr: u32 handle_from_ptr(void *) */
#define COD2_G_FN_BUILD       0xb3bf0u  /* fn ptr: build_anim_list(list, count, handle, ent, state) */
#define COD2_G_FN_RELEASE     0xb3bf4u  /* fn ptr: void release_anim(u32 handle) */

/* ---------------------------------------------------------------------------
 * Per-player view/animation state ("vs"). Observed: +0x380 vec3 lean, +0x3b0
 * vec3 sway, +0x3e4 float speed scale, +0x3e8 vec3 move velocity (x = forward
 * speed, tested by the move-parameter code), +0x3f4/+0x3f8 landing latch,
 * +0x4a4 anim tree handle, +0x4a8 cached slot index.
 * ------------------------------------------------------------------------- */
struct clientInfo_s {  /* canonical tag (opaque in cod2_canon.h); chunk-local animation/view layout */
    unsigned char pad_000[0x40];
    unsigned char validFlag40;      /* 0x40 "valid" byte tested by the anim-tree update */
    unsigned char pad_041[0x33f];   /* 0x41..0x37f */
    float lean380[3];               /* 0x380 */
    unsigned char pad_38c[0x24];    /* 0x38c..0x3af */
    float sway3b0[3];               /* 0x3b0 (z used as 0x3b8) */
    unsigned char pad_3bc[0x04];
    unsigned int flags3c0;          /* 0x3c0 bit 0x200 cleared on landing */
    unsigned char pad_3c4[0x20];    /* 0x3c4..0x3e3 */
    float speedScale3e4;            /* 0x3e4 */
    float velocity3e8[3];           /* 0x3e8 move velocity; x is forward speed */
    int landingTimer3f4;            /* 0x3f4 */
    int landingLatch3f8;            /* 0x3f8 */
    unsigned char pad_3fc[0x48];    /* 0x3fc..0x443 (vec3 bases at 0x3fc, 0x444, 0x450) */
    unsigned char pad_444[0x0c];    /* 0x444..0x44f */
    unsigned char pad_450[0x54];    /* 0x450..0x4a3 */
    unsigned int animTree4a4;       /* 0x4a4 */
    unsigned int animSlot4a8;       /* 0x4a8 cached index written by the anim-tree update */
};
COD2_ASSERT_OFFSET(clientInfo_s, validFlag40, 0x40);
COD2_ASSERT_OFFSET(clientInfo_s, lean380, 0x380);
COD2_ASSERT_OFFSET(clientInfo_s, sway3b0, 0x3b0);
COD2_ASSERT_OFFSET(clientInfo_s, flags3c0, 0x3c0);
COD2_ASSERT_OFFSET(clientInfo_s, speedScale3e4, 0x3e4);
COD2_ASSERT_OFFSET(clientInfo_s, velocity3e8, 0x3e8);
COD2_ASSERT_OFFSET(clientInfo_s, landingTimer3f4, 0x3f4);
COD2_ASSERT_OFFSET(clientInfo_s, animTree4a4, 0x4a4);
COD2_ASSERT_OFFSET(clientInfo_s, animSlot4a8, 0x4a8);

/* Raw float address inside a struct at a byte offset (for unnamed vec3 bases). */
#define COD2_F32_AT(p, off) ((float *)((unsigned char *)(p) + (off)))

/* Mantle stage table entry, g_mantleStages[7] (stride 0x0c).
 * upAnim/overAnim index g_mantleAnimNames; zHeight is the ledge height that selects the stage. */
struct MantleStage {
    int upAnim;
    int overAnim;
    float zHeight;
};
COD2_ASSERT_SIZE(MantleStage, 0x0c);

/* Item table entry, g_081850c0 (stride 0x2c). */
struct gitem_s {  /* canonical tag (opaque in cod2_canon.h); layout recovered by this chunk */
    const char *classname;          /* 0x00 */
    unsigned char pad_04[0x10];     /* 0x04..0x13 */
    const char *pickupAlias;        /* 0x14 */
    unsigned char pad_18[0x04];     /* 0x18..0x1b */
    int giType;                     /* 0x1c 0 = IT_BAD, 1 = weapon, 2 = ammo, 3 = armor */
    int giTag;                      /* 0x20 weapon/ammo index */
    unsigned char pad_24[0x08];     /* 0x24..0x2b */
};
COD2_ASSERT_SIZE(gitem_s, 0x2c);
COD2_ASSERT_OFFSET(gitem_s, pickupAlias, 0x14);
COD2_ASSERT_OFFSET(gitem_s, giType, 0x1c);
COD2_ASSERT_OFFSET(gitem_s, giTag, 0x20);

/* Mantle ledge record (ledge probe result). Written by Mantle_CheckLedge and
 * Mantle_FindSurface, read by Mantle_Start and Mantle_ComputeLedgeTarget.
 * start.z is the probe height (+0x14); ledgePos.z (+0x20) is the ledge height;
 * end.z (+0x2c) is overwritten with the hit height by Mantle_ComputeLedgeTarget. */
struct MantleLedge {
    float dir[3];                   /* 0x00 surface/facing direction */
    float start[3];                 /* 0x0c trace start; start[2] = height */
    float ledgePos[3];              /* 0x18 ledge position; ledgePos[2] at 0x20 */
    float end[3];                   /* 0x24 trace end; end[2] doubles as hit height at 0x2c */
    unsigned int flags;             /* 0x30 bit 1 = hang, bit 2 = end clear, bit 3 = found, bit 0 = surface */
};
COD2_ASSERT_SIZE(MantleLedge, 0x34);
COD2_ASSERT_OFFSET(MantleLedge, start, 0x0c);
COD2_ASSERT_OFFSET(MantleLedge, ledgePos, 0x18);
COD2_ASSERT_OFFSET(MantleLedge, end, 0x24);
COD2_ASSERT_OFFSET(MantleLedge, flags, 0x30);

/* Per-slot record in the global block: 0x60 bytes at g_gameContextPtr + slot * 0x60. */
struct SlotRecord040 {
    unsigned char pad_00[0x50];
    unsigned int flags50;           /* 0x50 bits 0x10 (mantle-type A), 0x20 (mantle-type B) */
    int value54;                    /* 0x54 low word of a 64-bit timer */
    int value58;                    /* 0x58 high word of a 64-bit timer */
    unsigned char pad_5c[0x04];
};
COD2_ASSERT_SIZE(SlotRecord040, 0x60);
COD2_ASSERT_OFFSET(SlotRecord040, flags50, 0x50);
COD2_ASSERT_OFFSET(SlotRecord040, value58, 0x58);

/* Float bit-pattern helper: the registration calls pass float defaults as raw bits. */
static inline float cod2_f32_bits(unsigned int bits)
{
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

/* Scr_GetAnimTreeValue (chunk 010, 0x0806df16) fills this 4-byte struct through the hidden return pointer; the shared
 * prototype only forward-declares it, so complete it here (same layout as chunk_010_types.h). */
struct ScrAnimTreeRef { int value; };
COD2_ASSERT_SIZE(ScrAnimTreeRef, 4);

#endif
