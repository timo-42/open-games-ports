/*
 * chunk_008_types.h -- cod2_lnxded 1.3 reconstruction, chunk 008
 * Address range 0x08065f94..0x08069c98 (77 functions).
 *
 * Contents: MD4 / block checksum, adaptive Huffman coder (huff_s), bit-level
 * message buffer (msg_s) and its read/write helpers, delta encoders for
 * usercmd / netField tables, and the iwd checksum list parser.
 *
 * Layouts were recovered from consistent offsets in the Ghidra pseudocode and
 * checked against the disassembly. Partial structures (playerState_s) only
 * declare the offsets this chunk touches; their size is not asserted.
 */
#ifndef COD2_CHUNK_008_TYPES_H
#define COD2_CHUNK_008_TYPES_H

#include "cod2_types.h"

/* ------------------------------------------------------------------------ */
/* MD4 digest context (MD4Init / MD4Update / MD4Final, 0x080670d4..).       */
/* ------------------------------------------------------------------------ */
struct md4_ctx_s {
    uint32 state[4];   /* 0x00 */
    uint32 count[2];   /* 0x10: bit count low word, high word */
    byte buffer[64];   /* 0x18 */
};
COD2_ASSERT_SIZE(md4_ctx_s, 0x58);
COD2_ASSERT_OFFSET(md4_ctx_s, count, 0x10);
COD2_ASSERT_OFFSET(md4_ctx_s, buffer, 0x18);

/* ------------------------------------------------------------------------ */
/* Adaptive Huffman coder.                                                   */
/* Inferred Quake III huffman.c counterpart: 257 leaf/NYT slots, internal    */
/* symbol 0x101, not-yet-transmitted symbol 0x100, 768 nodes per half.      */
/* ------------------------------------------------------------------------ */
#define COD2_HUFF_NYT 0x100
#define COD2_HUFF_INTERNAL 0x101

struct node_s {
    node_s *left;      /* 0x00 */
    node_s *right;     /* 0x04 */
    node_s *parent;    /* 0x08 */
    node_s *next;      /* 0x0c: weight-ordered list */
    node_s *prev;      /* 0x10 */
    node_s **head;     /* 0x14: slot holding the leader of this weight block */
    int32 weight;      /* 0x18 */
    int32 symbol;      /* 0x1c */
};
COD2_ASSERT_SIZE(node_s, 0x20);
COD2_ASSERT_OFFSET(node_s, right, 0x04);
COD2_ASSERT_OFFSET(node_s, parent, 0x08);
COD2_ASSERT_OFFSET(node_s, next, 0x0c);
COD2_ASSERT_OFFSET(node_s, prev, 0x10);
COD2_ASSERT_OFFSET(node_s, head, 0x14);
COD2_ASSERT_OFFSET(node_s, weight, 0x18);
COD2_ASSERT_OFFSET(node_s, symbol, 0x1c);

struct huffman_s {
    int32 blocNode;            /* 0x00: nodeList slots handed out */
    int32 blocPtrs;            /* 0x04: nodePtrs slots handed out */
    node_s *tree;              /* 0x08 */
    node_s *lhead;             /* 0x0c */
    node_s *ltail;             /* 0x10 */
    node_s *loc[257];          /* 0x14: loc[ch]; loc[256] is the NYT node */
    node_s **freelist;         /* 0x418: free list of node-pointer slots */
    node_s nodeList[768];      /* 0x41c */
    node_s *nodePtrs[768];     /* 0x641c */
};
COD2_ASSERT_SIZE(huffman_s, 0x701c);
COD2_ASSERT_OFFSET(huffman_s, loc, 0x14);
COD2_ASSERT_OFFSET(huffman_s, freelist, 0x418);
COD2_ASSERT_OFFSET(huffman_s, nodeList, 0x41c);
COD2_ASSERT_OFFSET(huffman_s, nodePtrs, 0x641c);

/* Two halves: compressor at +0, decompressor at +0x701c. */
struct huff_s {
    huffman_s compressor;
    huffman_s decompressor;
};
COD2_ASSERT_SIZE(huff_s, 0xe038);
COD2_ASSERT_OFFSET(huff_s, decompressor, 0x701c);

/* msg_s, netField_s, usercmd_s, playerState_s, prototypes and globals come from the canonical headers. */
#endif /* COD2_CHUNK_008_TYPES_H */
