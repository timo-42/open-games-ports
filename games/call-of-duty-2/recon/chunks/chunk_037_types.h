/*
 * Types, globals and out-of-chunk prototypes for chunk 037 (0x080d006a..0x080d389e).
 * Content: zlib 1.1.4 deflate tree/bit-output code (trees.c layer), the zlib uncompress()
 * wrapper, a minizip-style ZIP central-directory reader (unzip.c layer), a DOS date helper,
 * a directory probe, and the tty console echo/shutdown helpers.
 *
 * Layouts are recovered from field offsets used by this chunk. deflate_state_s and z_stream_s
 * follow the same offsets as chunk_036_types.h (the zlib inflate/deflate owner); the merge
 * lane must keep one definition of each. Fields between the recovered ones are opaque padding.
 */
#ifndef COD2_CHUNK_037_TYPES_H
#define COD2_CHUNK_037_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include <stdio.h>

/* ---- status and error codes used by this chunk ------------------------- */
#define ZLIB_STREAM_END      1
#define ZLIB_FINISH          4
#define ZLIB_SYNC_FLUSH      2
#define ZLIB_BUF_ERROR      (-5)
#define ZLIB_DATA_UNKNOWN    2      /* Z_UNKNOWN in deflate_state.data_type */

#define UNZ_END_OF_LIST_OF_FILE (-100)
#define UNZ_ERRNO               (-1)
#define UNZ_PARAMERROR          (-102)
#define UNZ_BADZIPFILE          (-103)
#define UNZ_INTERNALERROR       (-104)

#define ZIP_LOCAL_HEADER_SIG    0x04034b50u
#define ZIP_CENTRAL_HEADER_SIG  0x02014b50u

#define DEFLATE_HEAP_SIZE 573

/* ---- zlib z_stream (same layout as chunk_036_types.h) ------------------- */
struct z_stream_s;
typedef void *(*zalloc_fn)(void *opaque, uint32 items, uint32 size);
typedef void (*zfree_fn)(void *opaque, void *address);

/* z_stream_s: promoted to cod2_types.h */
COD2_ASSERT_SIZE(z_stream_s, 0x38);
COD2_ASSERT_OFFSET(z_stream_s, next_out, 0xc);
COD2_ASSERT_OFFSET(z_stream_s, total_out, 0x14);

/* ---- deflate trees (same layout as chunk_036_types.h) ------------------- */
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

/* ---- deflate_state (zlib 1.1.4 internal_state, as laid out in this binary) -- */
struct deflate_state_s {
    z_stream_s *strm;                     /* 0x00 */
    int status;                           /* 0x04 */
    byte *pending_buf;                    /* 0x08 */
    uint32 pending_buf_size;              /* 0x0c */
    byte *pending_out;                    /* 0x10 */
    uint32 pending;                       /* 0x14 */
    int noheader;                         /* 0x18 */
    byte data_type;                       /* 0x1c Z_BINARY / Z_ASCII / Z_UNKNOWN */
    byte pad_1d[0x7c - 0x1d];            /* 0x1d method .. last_flush, window fields, match state */
    int level;                            /* 0x7c */
    byte pad_80[0x8c - 0x80];            /* 0x80 strategy .. nice_match */
    ct_data dyn_ltree[DEFLATE_HEAP_SIZE]; /* 0x8c */
    ct_data dyn_dtree[61];                /* 0x980 */
    ct_data bl_tree[39];                  /* 0xa74 */
    tree_desc l_desc;                     /* 0xb10 */
    tree_desc d_desc;                     /* 0xb1c */
    tree_desc bl_desc;                    /* 0xb28 */
    uint16 bl_count[16];                  /* 0xb34 */
    int heap[DEFLATE_HEAP_SIZE];          /* 0xb54 */
    int heap_len;                         /* 0x1448 */
    int heap_max;                         /* 0x144c */
    byte depth[DEFLATE_HEAP_SIZE];        /* 0x1450 */
    byte pad_168d[3];                     /* 0x168d alignment */
    byte *l_buf;                          /* 0x1690 literal/length byte buffer */
    uint32 lit_bufsize;                   /* 0x1694 */
    uint32 last_lit;                      /* 0x1698 */
    uint16 *d_buf;                        /* 0x169c distance buffer */
    uint32 opt_len;                       /* 0x16a0 */
    uint32 static_len;                    /* 0x16a4 */
    uint32 matches;                       /* 0x16a8 */
    int last_eob_len;                     /* 0x16ac */
    uint16 bi_buf;                        /* 0x16b0 */
    uint16 pad_16b2;                      /* 0x16b2 */
    int bi_valid;                         /* 0x16b4 */
};
COD2_ASSERT_OFFSET(deflate_state_s, pending_buf, 0x8);
COD2_ASSERT_OFFSET(deflate_state_s, pending, 0x14);
COD2_ASSERT_OFFSET(deflate_state_s, data_type, 0x1c);
COD2_ASSERT_OFFSET(deflate_state_s, level, 0x7c);
COD2_ASSERT_OFFSET(deflate_state_s, dyn_ltree, 0x8c);
COD2_ASSERT_OFFSET(deflate_state_s, dyn_dtree, 0x980);
COD2_ASSERT_OFFSET(deflate_state_s, bl_tree, 0xa74);
COD2_ASSERT_OFFSET(deflate_state_s, l_desc, 0xb10);
COD2_ASSERT_OFFSET(deflate_state_s, d_desc, 0xb1c);
COD2_ASSERT_OFFSET(deflate_state_s, bl_desc, 0xb28);
COD2_ASSERT_OFFSET(deflate_state_s, heap, 0xb54);
COD2_ASSERT_OFFSET(deflate_state_s, heap_len, 0x1448);
COD2_ASSERT_OFFSET(deflate_state_s, depth, 0x1450);
COD2_ASSERT_OFFSET(deflate_state_s, l_buf, 0x1690);
COD2_ASSERT_OFFSET(deflate_state_s, lit_bufsize, 0x1694);
COD2_ASSERT_OFFSET(deflate_state_s, last_lit, 0x1698);
COD2_ASSERT_OFFSET(deflate_state_s, d_buf, 0x169c);
COD2_ASSERT_OFFSET(deflate_state_s, opt_len, 0x16a0);
COD2_ASSERT_OFFSET(deflate_state_s, static_len, 0x16a4);
COD2_ASSERT_OFFSET(deflate_state_s, matches, 0x16a8);
COD2_ASSERT_OFFSET(deflate_state_s, last_eob_len, 0x16ac);
COD2_ASSERT_OFFSET(deflate_state_s, bi_buf, 0x16b0);
COD2_ASSERT_OFFSET(deflate_state_s, bi_valid, 0x16b4);
/* Total size is owned by the deflateInit2 allocation (outside this chunk) and is not asserted. */

/* ---- minizip-style ZIP reader state (unzip.c layer) --------------------- */
struct unz_global_info_s {
    uint32 number_entry;        /* 0x00 */
    uint32 size_comment;        /* 0x04 */
};
COD2_ASSERT_SIZE(unz_global_info_s, 8);

struct tm_unz_s {
    uint32 tm_sec;              /* 0x00 */
    uint32 tm_min;              /* 0x04 */
    uint32 tm_hour;             /* 0x08 */
    uint32 tm_mday;             /* 0x0c */
    uint32 tm_mon;              /* 0x10 (0-based; may wrap for a zero month field) */
    uint32 tm_year;             /* 0x14 (absolute year) */
};
COD2_ASSERT_SIZE(tm_unz_s, 0x18);

struct unz_file_info_s {
    uint32 version;             /* 0x00 version made by */
    uint32 version_needed;      /* 0x04 */
    uint32 flag;                /* 0x08 general purpose bit flag */
    uint32 compression_method;  /* 0x0c */
    uint32 dosDate;             /* 0x10 */
    uint32 crc;                 /* 0x14 */
    uint32 compressed_size;     /* 0x18 */
    uint32 uncompressed_size;   /* 0x1c */
    uint32 size_filename;       /* 0x20 */
    uint32 size_file_extra;     /* 0x24 */
    uint32 size_file_comment;   /* 0x28 */
    uint32 disk_num_start;      /* 0x2c */
    uint32 internal_fa;         /* 0x30 */
    uint32 external_fa;         /* 0x34 */
    tm_unz_s tmu_date;          /* 0x38 */
};
COD2_ASSERT_SIZE(unz_file_info_s, 0x50);
COD2_ASSERT_OFFSET(unz_file_info_s, compression_method, 0x0c);
COD2_ASSERT_OFFSET(unz_file_info_s, size_filename, 0x20);
COD2_ASSERT_OFFSET(unz_file_info_s, size_file_comment, 0x28);
COD2_ASSERT_OFFSET(unz_file_info_s, tmu_date, 0x38);

struct unz_file_info_internal_s {
    uint32 offset_curfile;      /* 0x00 offset of the local header */
};
COD2_ASSERT_SIZE(unz_file_info_internal_s, 4);

struct file_in_zip_read_info_s;

struct unz_s {
    void *file;                                   /* 0x00 FILE* from Com_Fopen */
    unz_global_info_s gi;                         /* 0x04 number_entry, size_comment */
    uint32 byte_before_the_zipfile;               /* 0x0c */
    uint32 num_file;                              /* 0x10 index of the current entry */
    uint32 pos_in_central_dir;                    /* 0x14 */
    uint32 current_file_ok;                       /* 0x18 */
    uint32 central_pos;                           /* 0x1c end-of-central-directory offset */
    uint32 size_central_dir;                      /* 0x20 */
    uint32 offset_central_dir;                    /* 0x24 */
    unz_file_info_s cur_file_info;                /* 0x28 */
    unz_file_info_internal_s cur_file_info_internal; /* 0x78 */
    file_in_zip_read_info_s *pfile_in_zip_read;   /* 0x7c */
};
COD2_ASSERT_SIZE(unz_s, 0x80);
COD2_ASSERT_OFFSET(unz_s, gi, 0x04);
COD2_ASSERT_OFFSET(unz_s, byte_before_the_zipfile, 0x0c);
COD2_ASSERT_OFFSET(unz_s, pos_in_central_dir, 0x14);
COD2_ASSERT_OFFSET(unz_s, current_file_ok, 0x18);
COD2_ASSERT_OFFSET(unz_s, offset_central_dir, 0x24);
COD2_ASSERT_OFFSET(unz_s, cur_file_info, 0x28);
COD2_ASSERT_OFFSET(unz_s, cur_file_info_internal, 0x78);
COD2_ASSERT_OFFSET(unz_s, pfile_in_zip_read, 0x7c);

struct file_in_zip_read_info_s {
    byte *read_buffer;                /* 0x00 0x4000-byte compressed-data buffer */
    z_stream_s stream;                /* 0x04 inflate stream */
    uint32 pos_in_zipfile;            /* 0x3c next compressed byte to read (relative to byte_before) */
    uint32 stream_initialised;        /* 0x40 */
    uint32 offset_local_extrafield;   /* 0x44 */
    uint32 size_local_extrafield;     /* 0x48 */
    uint32 pos_local_extrafield;      /* 0x4c */
    uint32 rest_read_compressed;      /* 0x50 */
    uint32 rest_read_uncompressed;    /* 0x54 */
    void *file;                       /* 0x58 */
    uint32 compression_method;        /* 0x5c */
    uint32 byte_before_the_zipfile;   /* 0x60 */
};
COD2_ASSERT_SIZE(file_in_zip_read_info_s, 0x64);
COD2_ASSERT_OFFSET(file_in_zip_read_info_s, stream, 0x04);
COD2_ASSERT_OFFSET(file_in_zip_read_info_s, pos_in_zipfile, 0x3c);
COD2_ASSERT_OFFSET(file_in_zip_read_info_s, rest_read_compressed, 0x50);
COD2_ASSERT_OFFSET(file_in_zip_read_info_s, rest_read_uncompressed, 0x54);
COD2_ASSERT_OFFSET(file_in_zip_read_info_s, file, 0x58);
COD2_ASSERT_OFFSET(file_in_zip_read_info_s, compression_method, 0x5c);
COD2_ASSERT_OFFSET(file_in_zip_read_info_s, byte_before_the_zipfile, 0x60);

/* ---- console input field (same layout as chunk_038_types.h field_s) ----- */
/* field_s: promoted to cod2_types.h */
COD2_ASSERT_OFFSET(field_s, buffer, 0x18);

/* Out-of-chunk callees come from cod2_prototypes.h. Prototype supplement for the zlib tree
 * functions of chunk 036 that the shared header does not carry yet (analysis/symbols/chunk_036.json). */
void build_tree(deflate_state_s *s, tree_desc *desc);

#endif
