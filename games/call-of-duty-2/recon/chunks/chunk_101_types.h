/*
 * chunk 101: functions recovered from .eh_frame FDE starts (missed by Ghidra auto-analysis).
 * Scattered over 0x08078536..0x080b0fd8: script VM helpers, server operator commands,
 * client download commands, game-import wrappers, vector/random helpers, hunk and sound-alias
 * helpers. Layouts below are recovered from access patterns in these functions only.
 */
#ifndef COD2_CHUNK_101_TYPES_H
#define COD2_CHUNK_101_TYPES_H

/* Heap-allocated word tuple returned by pointer-in-struct (hidden return pointer, ret 4). */
struct HeapWords_s {
    int *words;
};

/* ---- game / server callees with shapes the merged prototypes lack or misstate ---- */

struct lineclip_a_s {
    float start[3];          /* +0x00 */
    float end[3];            /* +0x0c */
    float invDelta[3];       /* +0x18 filled by Box_ComputeInverseExtents */
    int unk_24;              /* +0x24 = -1 */
    int unk_28;              /* +0x28 = -1 */
    int contentmask;         /* +0x2c */
    int unk_30;              /* +0x30 = 1 */
    int traceFlags;          /* +0x34 */
};
COD2_ASSERT_SIZE(struct lineclip_a_s, 0x38);
COD2_ASSERT_OFFSET(struct lineclip_a_s, invDelta, 0x18);
COD2_ASSERT_OFFSET(struct lineclip_a_s, contentmask, 0x2c);

/* not yet in cod2_prototypes.h (owner chunk 014 pending migration) */
void ScrVar_SetVarValueReleasing(int id, struct VariableValue *v); /* 0x0807caac */

struct sv_entity_s;
void SV_ClipMoveToEntity_Line(struct lineclip_a_s *clip, struct sv_entity_s *touch, struct trace_s *tr); /* 0x0809cf08 */

/* ---- sound aliases ---- */
struct snd_alias_t {
    const char *aliasName;   /* +0x00 */
    unsigned char unk_04[0x40];
};
COD2_ASSERT_SIZE(struct snd_alias_t, 0x44);
struct snd_alias_list_t {
    const char *aliasName;   /* +0x00 */
    struct snd_alias_t *head;/* +0x04 */
    int count;               /* +0x08 */
    struct snd_alias_list_t *next; /* +0x0c */
};
COD2_ASSERT_SIZE(struct snd_alias_list_t, 0x10);

#endif /* COD2_CHUNK_101_TYPES_H */
