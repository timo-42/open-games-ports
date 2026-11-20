/*
 * Types, globals and out-of-chunk prototypes for chunk 035 (0x080c79fe..0x080cbd2f):
 *   - a few XModel LOD accessors (0x080c79fe..0x080c7bd6),
 *   - the statically linked zlib 1.1.4 deflate side (adler32, compress, deflate*, lm_init,
 *     longest_match, fill_window, deflate_stored/fast/slow),
 *   - the zlib 1.1.4 inflate_blocks state machine (inflate_blocks*, inflate_set_dictionary).
 * Layouts were recovered from consistent field offsets in the decompilation and checked
 * against the sizes zlib 1.1.4 produces on i386 (z_stream_s 0x38, deflate_state_s 0x16b8,
 * inflate_blocks_state_s 0x40).
 */
#ifndef COD2_CHUNK_035_TYPES_H
#define COD2_CHUNK_035_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ------------------------------------------------------------------ */
/* XModel LOD accessors                                                */
/* ------------------------------------------------------------------ */

/* One LOD record; XModel::lod[i] is at XModel + 4 + i * 0x14. */
struct XModelLod {
    float dist;          /* 0x00 LOD distance cutoff (0 = last LOD / unlimited) */
    int unk04;           /* 0x04 not touched by this chunk */
    short numSurfs;      /* 0x08 surface count of the LOD */
    short unk0a;         /* 0x0a */
    int unk0c;           /* 0x0c */
    unsigned int *surfData; /* 0x10 block: [0] = leading word, [1..] = surface records */
};
COD2_ASSERT_SIZE(XModelLod, 0x14);
COD2_ASSERT_OFFSET(XModelLod, dist, 0x00);
COD2_ASSERT_OFFSET(XModelLod, numSurfs, 0x08);
COD2_ASSERT_OFFSET(XModelLod, surfData, 0x10);

/* Only the fields read by this chunk are named. */
struct XModel {
    int unk00;                 /* 0x00 */
    XModelLod lod[4];      /* 0x04 (array bound inferred from 0x7c - 0x04 gap; unvalidated) */
    unsigned char unk54[0x28]; /* 0x54 */
    short numLods;             /* 0x7c */
    unsigned char unk7e[0x0e]; /* 0x7e */
    unsigned char byte8c;      /* 0x8c */
};
COD2_ASSERT_OFFSET(XModel, lod, 0x04);
COD2_ASSERT_OFFSET(XModel, numLods, 0x7c);
COD2_ASSERT_OFFSET(XModel, byte8c, 0x8c);

/* Object whose first member points at a record with a leading short; its parts block is at +0x44. */
struct XModelRefHeader {
    short firstShort;          /* 0x00 */
};
struct XModelRef {
    XModelRefHeader *header;   /* 0x00 (first word of the object) */
};

/* Per-LOD distance override table (g_xmodelLodDistanceOverrides), 8-byte entries. */
struct LodDistanceOverride {
    unsigned char valid;       /* 0x00 set when the stored distance is >= 0 */
    unsigned char pad01[3];
    float dist;                /* 0x04 */
};
COD2_ASSERT_SIZE(LodDistanceOverride, 8);
COD2_ASSERT_OFFSET(LodDistanceOverride, dist, 4);

extern LodDistanceOverride g_xmodelLodDistanceOverrides[];

/* ------------------------------------------------------------------ */
/* zlib 1.1.4                                                          */
/* ------------------------------------------------------------------ */

typedef unsigned char Byte;
typedef unsigned int uInt;
typedef unsigned long uLong;
typedef unsigned short ush;

enum {
    Z_OK = 0,
    Z_STREAM_END = 1,
    Z_STREAM_ERROR = -2,
    Z_DATA_ERROR = -3,
    Z_MEM_ERROR = -4,
    Z_BUF_ERROR = -5,
    Z_VERSION_ERROR = -6
};
enum {
    Z_NO_FLUSH = 0,
    Z_PARTIAL_FLUSH = 1,
    Z_SYNC_FLUSH = 2,
    Z_FULL_FLUSH = 3,
    Z_FINISH = 4
};
enum { Z_FILTERED = 1, Z_HUFFMAN_ONLY = 2 };

struct deflate_state_s;
struct inflate_codes_state_s;
struct inflate_blocks_state_s;

typedef void *(*zalloc_func)(void *opaque, unsigned int items, unsigned int size);
typedef void (*zfree_func)(void *opaque, void *address);


struct ct_data {
    ush freq_or_code;          /* 0x00 */
    ush dad_or_len;            /* 0x02 */
};
COD2_ASSERT_SIZE(ct_data, 4);

struct static_tree_desc;
struct tree_desc {
    ct_data *dyn_tree;
    int max_code;
    const static_tree_desc *stat_desc;
};
COD2_ASSERT_SIZE(tree_desc, 12);

enum {
    INIT_STATE = 42,
    BUSY_STATE = 113,
    FINISH_STATE = 666
};
enum { L_CODES = 286, D_CODES = 30, BL_CODES = 19, HEAP_SIZE = 573 };

struct deflate_state_s {
    z_stream_s *strm;            /* 0x0000 */
    int status;                /* 0x0004 */
    Byte *pending_buf;         /* 0x0008 */
    uLong pending_buf_size;    /* 0x000c */
    Byte *pending_out;         /* 0x0010 */
    int pending;               /* 0x0014 */
    int noheader;              /* 0x0018 */
    Byte data_type;            /* 0x001c */
    Byte method;               /* 0x001d */
    int last_flush;            /* 0x0020 */
    uInt w_size;               /* 0x0024 */
    uInt w_bits;               /* 0x0028 */
    uInt w_mask;               /* 0x002c */
    Byte *window;              /* 0x0030 */
    uLong window_size;         /* 0x0034 */
    ush *prev;                 /* 0x0038 */
    ush *head;                 /* 0x003c */
    uInt ins_h;                /* 0x0040 */
    uInt hash_size;            /* 0x0044 */
    uInt hash_bits;            /* 0x0048 */
    uInt hash_mask;            /* 0x004c */
    uInt hash_shift;           /* 0x0050 */
    long block_start;          /* 0x0054 */
    uInt match_length;         /* 0x0058 */
    uInt prev_match;           /* 0x005c */
    int match_available;       /* 0x0060 */
    uInt strstart;             /* 0x0064 */
    uInt match_start;          /* 0x0068 */
    uInt lookahead;            /* 0x006c */
    uInt prev_length;          /* 0x0070 */
    uInt max_chain_length;     /* 0x0074 */
    uInt max_lazy_match;       /* 0x0078 (also max_insert_length for deflate_fast) */
    int level;                 /* 0x007c */
    int strategy;              /* 0x0080 */
    uInt good_match;           /* 0x0084 */
    int nice_match;            /* 0x0088 */
    ct_data dyn_ltree[HEAP_SIZE];          /* 0x008c */
    ct_data dyn_dtree[2 * D_CODES + 1];    /* 0x0980 */
    ct_data bl_tree[2 * BL_CODES + 1];     /* 0x0a74 */
    tree_desc l_desc;          /* 0x0b10 */
    tree_desc d_desc;          /* 0x0b1c */
    tree_desc bl_desc;         /* 0x0b28 */
    ush bl_count[16];          /* 0x0b34 */
    int heap[2 * L_CODES + 1]; /* 0x0b54 */
    int heap_len;              /* 0x1448 */
    int heap_max;              /* 0x144c */
    Byte depth[2 * L_CODES + 1]; /* 0x1450 */
    Byte *l_buf;               /* 0x1690 */
    uInt lit_bufsize;          /* 0x1694 */
    uInt last_lit;             /* 0x1698 */
    ush *d_buf;                /* 0x169c */
    uLong opt_len;             /* 0x16a0 */
    uLong static_len;          /* 0x16a4 */
    uInt matches;              /* 0x16a8 */
    int last_eob_len;          /* 0x16ac */
    ush bi_buf;                /* 0x16b0 */
    int bi_valid;              /* 0x16b4 */
};
COD2_ASSERT_SIZE(deflate_state_s, 0x16b8);
COD2_ASSERT_OFFSET(deflate_state_s, status, 0x04);
COD2_ASSERT_OFFSET(deflate_state_s, last_flush, 0x20);
COD2_ASSERT_OFFSET(deflate_state_s, window, 0x30);
COD2_ASSERT_OFFSET(deflate_state_s, ins_h, 0x40);
COD2_ASSERT_OFFSET(deflate_state_s, block_start, 0x54);
COD2_ASSERT_OFFSET(deflate_state_s, strstart, 0x64);
COD2_ASSERT_OFFSET(deflate_state_s, nice_match, 0x88);
COD2_ASSERT_OFFSET(deflate_state_s, dyn_ltree, 0x8c);
COD2_ASSERT_OFFSET(deflate_state_s, dyn_dtree, 0x980);
COD2_ASSERT_OFFSET(deflate_state_s, bl_tree, 0xa74);
COD2_ASSERT_OFFSET(deflate_state_s, l_desc, 0xb10);
COD2_ASSERT_OFFSET(deflate_state_s, heap, 0xb54);
COD2_ASSERT_OFFSET(deflate_state_s, depth, 0x1450);
COD2_ASSERT_OFFSET(deflate_state_s, l_buf, 0x1690);
COD2_ASSERT_OFFSET(deflate_state_s, d_buf, 0x169c);
COD2_ASSERT_OFFSET(deflate_state_s, bi_valid, 0x16b4);

typedef int (*compress_func)(deflate_state_s *s, int flush);

/* configuration_table entry (12 bytes, table at 0x08150900). */
struct deflate_config {
    ush good_length;           /* 0x00 */
    ush max_lazy;              /* 0x02 */
    ush nice_length;           /* 0x04 */
    ush max_chain;             /* 0x06 */
    compress_func func;        /* 0x08 */
};
COD2_ASSERT_SIZE(deflate_config, 12);

/* inflate */
struct inflate_huft {
    Byte exop;                 /* 0x00 */
    Byte bits;                 /* 0x01 */
    ush pad;                   /* 0x02 */
    uInt base;                 /* 0x04 */
};
COD2_ASSERT_SIZE(inflate_huft, 8);

enum inflate_block_mode {
    IB_TYPE = 0, IB_LENS = 1, IB_STORED = 2, IB_TABLE = 3, IB_BTREE = 4,
    IB_DTREE = 5, IB_CODES = 6, IB_DRY = 7, IB_DONE = 8, IB_BAD = 9
};

typedef uLong (*check_func)(uLong check, const Byte *buf, uInt len);

struct inflate_blocks_state_s {
    int mode;                  /* 0x00 */
    union {
        uInt left;
        struct {
            uInt table;        /* 0x04 */
            uInt index;        /* 0x08 */
            uInt *blens;       /* 0x0c */
            uInt bb;           /* 0x10 */
            inflate_huft *tb;  /* 0x14 */
        } trees;
        struct {
            inflate_codes_state_s *codes; /* 0x04 */
        } decode;
    } sub;
    uInt last;                 /* 0x18 */
    uInt bitk;                 /* 0x1c */
    uLong bitb;                /* 0x20 */
    inflate_huft *hufts;       /* 0x24 */
    Byte *window;              /* 0x28 */
    Byte *end;                 /* 0x2c */
    Byte *read;                /* 0x30 */
    Byte *write;               /* 0x34 */
    check_func checkfn;        /* 0x38 */
    uLong check;               /* 0x3c */
};
COD2_ASSERT_SIZE(inflate_blocks_state_s, 0x40);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, sub, 0x04);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, last, 0x18);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, bitk, 0x1c);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, hufts, 0x24);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, read, 0x30);
COD2_ASSERT_OFFSET(inflate_blocks_state_s, check, 0x3c);

/* Prototype supplement: functions of chunks 035/036 that cod2_prototypes.h does not carry yet
 * (signatures as recorded in analysis/symbols/chunk_036.json / chunk_035.json). */
struct inflate_blocks_state_s *inflate_blocks_new(struct z_stream_s *z, check_func c, unsigned int w);
inflate_codes_state_s *inflate_codes_new(byte lbits, byte dbits, inflate_huft *tl, inflate_huft *td, z_stream_s *z);
int inflate_codes(inflate_blocks_state_s *s, z_stream_s *z, int r);
void inflate_codes_free(inflate_codes_state_s *c, z_stream_s *z);
int inflate_trees_bits(uint32 *c, uint32 *bb, inflate_huft **tb, inflate_huft *hp, z_stream_s *z);
int inflate_trees_dynamic(uint32 nl, uint32 nd, uint32 *c, uint32 *bl, uint32 *bd, inflate_huft **tl, inflate_huft **td, inflate_huft *hp, z_stream_s *z);
int inflate_trees_fixed(uint32 *bl, uint32 *bd, inflate_huft **tl, inflate_huft **td);
int inflate_flush(inflate_blocks_state_s *s, z_stream_s *z, int r);

/* z_stream_s::state is an untyped pointer in the canonical layout; the deflate side stores
 * a deflate_state_s there. */
#define DEFLATE_STATE(z) (*(deflate_state_s **)&(z)->state)

#endif
