/*
 * Types, globals and out-of-chunk prototypes for chunk 036 (0x080cbd40..0x080cf6a4).
 * Content: the statically linked zlib (inflate side: codes/fast/trees/header state machine;
 * deflate side: Huffman tree building and tree transmission). Layouts are recovered from
 * field offsets used by the binary; the error strings are zlib's own messages.
 */
#ifndef COD2_CHUNK_036_TYPES_H
#define COD2_CHUNK_036_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ---- z_stream --------------------------------------------------------- */
struct z_stream_s;
typedef void *(*zalloc_fn)(void *opaque, uint32 items, uint32 size);
typedef void (*zfree_fn)(void *opaque, void *address);
typedef unsigned long (*check_fn)(unsigned long check, const byte *buf, unsigned int len);

/* z_stream_s: promoted to cod2_types.h */
COD2_ASSERT_SIZE(z_stream_s, 0x38);
COD2_ASSERT_OFFSET(z_stream_s, avail_in, 4);
COD2_ASSERT_OFFSET(z_stream_s, total_in, 8);
COD2_ASSERT_OFFSET(z_stream_s, next_out, 0xc);
COD2_ASSERT_OFFSET(z_stream_s, avail_out, 0x10);
COD2_ASSERT_OFFSET(z_stream_s, total_out, 0x14);
COD2_ASSERT_OFFSET(z_stream_s, msg, 0x18);
COD2_ASSERT_OFFSET(z_stream_s, state, 0x1c);
COD2_ASSERT_OFFSET(z_stream_s, zalloc, 0x20);
COD2_ASSERT_OFFSET(z_stream_s, zfree, 0x24);
COD2_ASSERT_OFFSET(z_stream_s, opaque, 0x28);
COD2_ASSERT_OFFSET(z_stream_s, adler, 0x30);

/* ---- inflate ----------------------------------------------------------- */
/* Huffman table entry; base is an entry-relative offset for sub-table links. */
struct inflate_huft {
    byte exop;    /* 0x00 extra bits / operation flags (16 base, 32 end, 64 link) */
    byte bits;    /* 0x01 bits consumed by this code */
    uint16 pad;   /* 0x02 */
    uint32 base;  /* 0x04 literal / length base / distance base / link offset */
};
COD2_ASSERT_SIZE(inflate_huft, 8);
COD2_ASSERT_OFFSET(inflate_huft, bits, 1);
COD2_ASSERT_OFFSET(inflate_huft, base, 4);

enum {
    CODES_START = 0, CODES_LEN = 1, CODES_LENEXT = 2, CODES_DIST = 3, CODES_DISTEXT = 4,
    CODES_COPY = 5, CODES_LIT = 6, CODES_WASH = 7, CODES_END = 8, CODES_BADCODE = 9
};

struct inflate_codes_state_s {
    uint32 mode;                /* 0x00 */
    uint32 len;                 /* 0x04 */
    union {
        struct { inflate_huft *tree; uint32 need; } code; /* 0x08, 0x0c */
        uint32 lit;             /* 0x08 */
        struct { uint32 get; uint32 dist; } copy;          /* 0x08, 0x0c */
    } sub;
    byte lbits;                 /* 0x10 */
    byte dbits;                 /* 0x11 */
    uint16 pad12;               /* 0x12 */
    inflate_huft *ltree;        /* 0x14 */
    inflate_huft *dtree;        /* 0x18 */
};
COD2_ASSERT_SIZE(inflate_codes_state_s, 0x1c);
COD2_ASSERT_OFFSET(inflate_codes_state_s, len, 4);
COD2_ASSERT_OFFSET(inflate_codes_state_s, lbits, 0x10);
COD2_ASSERT_OFFSET(inflate_codes_state_s, dbits, 0x11);
COD2_ASSERT_OFFSET(inflate_codes_state_s, ltree, 0x14);
COD2_ASSERT_OFFSET(inflate_codes_state_s, dtree, 0x18);

struct inflate_blocks_state_s {
    uint32 mode;                /* 0x00 */
    union {
        uint32 left;
        struct { uint32 table; uint32 index; uint32 *blens; uint32 bb; inflate_huft *tb; } trees;
        struct { inflate_codes_state_s *codes; } decode;
    } sub;                      /* 0x04..0x17 */
    int last;                   /* 0x18 */
    uint32 bitk;                /* 0x1c bits in bit buffer */
    uint32 bitb;                /* 0x20 bit buffer */
    inflate_huft *hufts;        /* 0x24 */
    byte *window;               /* 0x28 */
    byte *end;                  /* 0x2c */
    byte *read;                 /* 0x30 */
    byte *write;                /* 0x34 */
    check_fn checkfn;           /* 0x38 */
    uint32 check;               /* 0x3c */
};
COD2_ASSERT_SIZE(inflate_blocks_state_s, 0x40);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, last, 0x18);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, bitk, 0x1c);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, bitb, 0x20);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, window, 0x28);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, end, 0x2c);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, read, 0x30);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, write, 0x34);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, checkfn, 0x38);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, check, 0x3c);

/* Header/trailer state machine of one inflate stream (z_stream.state). */
enum {
    INFLATE_METHOD = 0, INFLATE_FLAG = 1, INFLATE_DICT4 = 2, INFLATE_DICT3 = 3, INFLATE_DICT2 = 4,
    INFLATE_DICT1 = 5, INFLATE_DICT0 = 6, INFLATE_BLOCKS = 7, INFLATE_CHECK4 = 8,
    INFLATE_CHECK3 = 9, INFLATE_CHECK2 = 10, INFLATE_CHECK1 = 11, INFLATE_DONE = 12,
    INFLATE_BAD = 13
};

struct inflate_state_s {
    uint32 mode;                /* 0x00 */
    uint32 sub;                 /* 0x04 method / check.was / marker */
    uint32 need;                /* 0x08 check.need */
    int nowrap;                 /* 0x0c */
    uint32 wbits;               /* 0x10 */
    inflate_blocks_state_s *blocks; /* 0x14 */
};
COD2_ASSERT_SIZE(inflate_state_s, 0x18);
COD2_ASSERT_OFFSET(inflate_state_s, need, 8);
COD2_ASSERT_OFFSET(inflate_state_s, nowrap, 0xc);
COD2_ASSERT_OFFSET(inflate_state_s, wbits, 0x10);
COD2_ASSERT_OFFSET(inflate_state_s, blocks, 0x14);

/* ---- deflate trees ------------------------------------------------------ */
struct ct_data {
    uint16 fc;
    uint16 dl;
};
COD2_ASSERT_SIZE(ct_data, 4);

struct static_tree_desc {
    const ct_data *static_tree;
    const int *extra_bits;
    int extra_base;
    int elems;
    int max_length;
};
COD2_ASSERT_SIZE(static_tree_desc, 0x14);

struct tree_desc {
    ct_data *dyn_tree;
    int max_code;
    const static_tree_desc *stat_desc;
};
COD2_ASSERT_SIZE(tree_desc, 0xc);

#define DEFLATE_HEAP_SIZE 573

struct deflate_state_s {
    z_stream_s *strm;           /* 0x00 */
    int status;                 /* 0x04 */
    byte *pending_buf;          /* 0x08 */
    uint32 pending_buf_size;    /* 0x0c */
    byte *pending_out;          /* 0x10 */
    uint32 pending;             /* 0x14 */
    byte pad18[0x8c - 0x18];    /* 0x18 window/hash/match state (not touched in this chunk) */
    ct_data dyn_ltree[DEFLATE_HEAP_SIZE]; /* 0x08c */
    ct_data dyn_dtree[61];      /* 0x980 */
    ct_data bl_tree[39];        /* 0xa74 */
    tree_desc l_desc;           /* 0xb10 */
    tree_desc d_desc;           /* 0xb1c */
    tree_desc bl_desc;          /* 0xb28 */
    uint16 bl_count[16];        /* 0xb34 */
    int heap[DEFLATE_HEAP_SIZE];/* 0xb54 */
    int heap_len;               /* 0x1448 */
    int heap_max;               /* 0x144c */
    byte depth[DEFLATE_HEAP_SIZE]; /* 0x1450 */
    byte pad168d[0x1698 - 0x1450 - DEFLATE_HEAP_SIZE]; /* l_buf / lit_bufsize */
    uint32 last_lit;            /* 0x1698 */
    uint16 *d_buf;              /* 0x169c */
    uint32 opt_len;             /* 0x16a0 */
    uint32 static_len;          /* 0x16a4 */
    uint32 matches;             /* 0x16a8 */
    int last_eob_len;           /* 0x16ac */
    uint16 bi_buf;              /* 0x16b0 */
    uint16 pad16b2;
    int bi_valid;               /* 0x16b4 */
};
COD2_ASSERT_OFFSET(deflate_state_s, pending_buf, 8);
COD2_ASSERT_OFFSET(deflate_state_s, pending, 0x14);
COD2_ASSERT_OFFSET(deflate_state_s, dyn_ltree, 0x8c);
COD2_ASSERT_OFFSET(deflate_state_s, dyn_dtree, 0x980);
COD2_ASSERT_OFFSET(deflate_state_s, bl_tree, 0xa74);
COD2_ASSERT_OFFSET(deflate_state_s, l_desc, 0xb10);
COD2_ASSERT_OFFSET(deflate_state_s, d_desc, 0xb1c);
COD2_ASSERT_OFFSET(deflate_state_s, bl_desc, 0xb28);
COD2_ASSERT_OFFSET(deflate_state_s, bl_count, 0xb34);
COD2_ASSERT_OFFSET(deflate_state_s, heap, 0xb54);
COD2_ASSERT_OFFSET(deflate_state_s, heap_len, 0x1448);
COD2_ASSERT_OFFSET(deflate_state_s, heap_max, 0x144c);
COD2_ASSERT_OFFSET(deflate_state_s, depth, 0x1450);
COD2_ASSERT_OFFSET(deflate_state_s, last_lit, 0x1698);
COD2_ASSERT_OFFSET(deflate_state_s, opt_len, 0x16a0);
COD2_ASSERT_OFFSET(deflate_state_s, static_len, 0x16a4);
COD2_ASSERT_OFFSET(deflate_state_s, matches, 0x16a8);
COD2_ASSERT_OFFSET(deflate_state_s, last_eob_len, 0x16ac);
COD2_ASSERT_OFFSET(deflate_state_s, bi_buf, 0x16b0);
COD2_ASSERT_OFFSET(deflate_state_s, bi_valid, 0x16b4);

/* Prototype supplement: chunk 035 functions with canonical tags (cod2_prototypes.h still carries the
 * pre-migration struct names until it is regenerated; identical redeclarations are harmless). */
void inflate_blocks_reset(inflate_blocks_state_s *s, z_stream_s *z, unsigned long *c);
inflate_blocks_state_s *inflate_blocks_new(z_stream_s *z, check_fn c, unsigned int w);
int inflate_blocks(inflate_blocks_state_s *s, z_stream_s *z, int r);
int inflate_blocks_free(inflate_blocks_state_s *s, z_stream_s *z);
void inflate_set_dictionary(inflate_blocks_state_s *s, const unsigned char *d, unsigned int n);
int inflate_blocks_sync_point(inflate_blocks_state_s *s);

/* ---- functions of this chunk used across the chunk --------------------- */
inflate_codes_state_s *inflate_codes_new(byte lbits, byte dbits, inflate_huft *tl, inflate_huft *td, z_stream_s *z);
int inflate_codes(inflate_blocks_state_s *s, z_stream_s *z, int r);
void inflate_codes_free(inflate_codes_state_s *c, z_stream_s *z);
int inflate_fast(uint32 bl, uint32 bd, inflate_huft *tl, inflate_huft *td, inflate_blocks_state_s *s, z_stream_s *z);
int inflateReset(z_stream_s *z);
int inflateEnd(z_stream_s *z);
int inflateInit2_(z_stream_s *z, int w, const char *version, int stream_size);
int inflate_flush(inflate_blocks_state_s *s, z_stream_s *z, int r);
int huft_build(const uint32 *b, uint32 n, uint32 s, const uint32 *d, const uint32 *e,
               inflate_huft **t, uint32 *m, inflate_huft *hp, uint32 *hn, uint32 *v);
void tr_static_init(void);
void init_block(deflate_state_s *s);
void pqdownheap(deflate_state_s *s, ct_data *tree, int k);
void gen_bitlen(deflate_state_s *s, tree_desc *desc);
void gen_codes(ct_data *tree, int max_code, const uint16 *bl_count);
void build_tree(deflate_state_s *s, tree_desc *desc);

#endif
