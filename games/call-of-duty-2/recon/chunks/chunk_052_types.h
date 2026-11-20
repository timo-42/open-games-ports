/*
 * chunk_052 types: Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded).
 *
 * Covers the game entity pool (stride 0x230, pool base level_gentities / static
 * array g_gentities), attachment/child link nodes, the game-client fragment
 * reached through entity+0x158, the bullet trace result, the weapon-def
 * fragment reached through the bullet context, and the orientation matrix.
 *
 * Layouts are recovered from consistent offsets in the chunk's pseudocode and
 * disassembly. Fields never touched by this chunk are kept as byte gaps.
 * The entity pool here does not match the 0x274 stride used by the
 * gentity_s views in other chunks; that is an open question for the merge lane.
 */
#ifndef COD2_CHUNK_052_TYPES_H
#define COD2_CHUNK_052_TYPES_H

#include <stdint.h>
#include "cod2_types.h"
#include "cod2_canon.h"

/* Pointer <-> 32-bit value helpers (the binary is ELF32 i386). */
static inline void *c52_ptr(int32 v) { return (void *)(uintptr_t)(uint32)v; }
static inline void *c52_ptr(const void *p) { return (void *)p; }
static inline int32 c52_int(const void *p) { return (int32)(uintptr_t)p; }

/* Orientation: three axis vectors followed by an origin (48 bytes); canonical Orient lives in the owning chunk (031),
   this chunk only needs the layout. */
typedef struct game_orient_s {
    float axis[3][3];        /* 0x00: axis[0] forward, axis[1] right, axis[2] up (inferred) */
    float origin[3];         /* 0x24 */
} game_orient_s;
COD2_ASSERT_SIZE(game_orient_s, 0x30);
COD2_ASSERT_OFFSET(game_orient_s, origin, 0x24);


/* Child/attachment link node, allocated 0x70 bytes at entity+0x208. */
struct game_attach_node_s {
    gentity_s *owner;    /* 0x00 parent entity */
    gentity_s *next;     /* 0x04 next sibling in owner->attachChildren */
    uint16 scrName;          /* 0x08 tag name reference */
    byte unk_0a[2];          /* 0x0a */
    int32 tagIndex;          /* 0x0c tag index, -1 when unresolved or unnamed */
    float origin[3];         /* 0x10 */
    byte unk_1c[0x18];       /* 0x1c */
    float angles[3];         /* 0x34 */
    byte unk_40[0x30];       /* 0x40 */
};
COD2_ASSERT_SIZE(game_attach_node_s, 0x70);
COD2_ASSERT_OFFSET(game_attach_node_s, tagIndex, 0x0c);
COD2_ASSERT_OFFSET(game_attach_node_s, origin, 0x10);
COD2_ASSERT_OFFSET(game_attach_node_s, angles, 0x34);

/* Bullet context passed as the 7th argument of Bullet_Fire_Extended. */
typedef struct game_bullet_ctx_s {
    byte unk_000[0x3c];
    weaponDef_s *weapon;           /* 0x3c */
} game_bullet_ctx_s;
COD2_ASSERT_OFFSET(game_bullet_ctx_s, weapon, 0x3c);

/* Entry of the two-entity queue at g_entityPairQueue (12 bytes, 0x100 entries). */
typedef struct game_pair_queue_s {
    uint16 a;                        /* entity number of the first entity */
    uint16 b;                        /* entity number of the second entity */
    uint32 genA;                     /* first entity generation (entity+0x228) */
    uint32 genB;                     /* second entity generation (entity+0x228) */
} game_pair_queue_s;
COD2_ASSERT_SIZE(game_pair_queue_s, 12);

/* Kind callback table entry at g_08187d1c, stride 0x28, callback at offset 0. */
typedef void (*c52_kind_cb_t)(gentity_s *ent, void *buf);

/* ---- globals absent from cod2_globals.h under these names (the data image defines them at the original addresses) ---- */


/* ---- Out-of-chunk callees (placeholders until the merge lane names them) ---- */


#endif
