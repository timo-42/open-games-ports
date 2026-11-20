/*
 * chunk_013 types: cod2_lnxded 1.3 reconstruction, address range 0x08076d26..0x08079e74.
 *
 * Covers: the MT_ power-of-two block allocator over a 16-bit handle space, the
 * string pool (ScrStr_*, 8-byte records with a 0x4000-bucket hash table), the
 * script source-line/debug tables (ScrDbg_*), script compile/runtime error
 * reporting (ScrCompile_*, ScrRuntime_*), and small script value/list boxes.
 *
 * Out-of-chunk callees use the fn_08xxxxxx placeholder convention; unnamed
 * globals use g_08xxxxxx. Names for globals used only by this chunk are inferred
 * from the code (see analysis/symbols/chunk_013.json for per-item confidence).
 */
#ifndef COD2_CHUNK_013_TYPES_H
#define COD2_CHUNK_013_TYPES_H

#include <ctype.h>


/* ---- String pool record (8-byte granularity, handle = (record - str_pool) / 8) ---- */
struct scr_string_s {
    uint8 length;      /* byte 0: stored length (text length + 1 when interned as C string) */
    uint8 flags;       /* byte 1: usage flag bits (0x04 = permanent/reset-to-one) */
    int16 refcount;    /* bytes 2-3 */
    char text[4];      /* bytes 4..: inline text; longer strings continue in the pool block */
};
COD2_ASSERT_OFFSET(struct scr_string_s, text, 4);
COD2_ASSERT_SIZE(struct scr_string_s, 8);

/* ---- String hash table node (sl_hash[0x4000]), 4 bytes each ----
 * link   : bits 0-13 next node index in the bucket/free chain, bits 14-15 state
 *          (0x8000 = bucket head in use, 0x4000 = node in chain, 0 = free/unused)
 * handle : string pool handle held by this node (node 0.handle is the free-list tail)
 * Node 0.link doubles as the free-list head.
 */
struct sl_hash_node_s {
    uint16 link;
    uint16 handle;
};
COD2_ASSERT_SIZE(struct sl_hash_node_s, 4);

/* ---- Debug source file table entry (scr_file_table), 0x14 bytes ---- */
struct scr_file_entry_s {
    uint32 key;          /* source file key (matches scrLastOpcodePtr at push time) */
    int32 first_line;    /* index of this file's first entry in scr_line_table */
    int32 line_count;    /* number of line entries owned by this file */
    uint32 unk_0c[2];    /* not touched by chunk_013 */
};
COD2_ASSERT_OFFSET(struct scr_file_entry_s, line_count, 8);
COD2_ASSERT_SIZE(struct scr_file_entry_s, 0x14);

/* ---- Debug line table entry (scr_line_table), 8 bytes ---- */
struct scr_line_s {
    int32 value;         /* source line number, or -1/-2 pending-marker sentinel */
    uint32 flags;        /* line flag bits (0x1 = resolves pending -1, 0x4 = set by SetPendingEntry) */
};
COD2_ASSERT_SIZE(struct scr_line_s, 8);

/* ---- Script frame / source-text record (scr_frame_table), 0x18 bytes ---- */
struct scr_frame_s {
    uint32 id;           /* code key (start address of the frame's code range) */
    char *name;          /* file name (heap copy) */
    char *source;        /* source text (second block after name), or NULL */
    int32 source_len;    /* byte length of source (-1 when no text) */
    int32 current_line;  /* initialised to -1 */
    uint8 is_debug;      /* nonzero for debug-loaded frames */
};
COD2_ASSERT_OFFSET(struct scr_frame_s, source_len, 0xc);
COD2_ASSERT_SIZE(struct scr_frame_s, 0x18);

/* ---- Saved-source stack entry (scr_saved_src), 8 bytes ---- */
struct scr_saved_src_s {
    char *text;
    int32 length;
};
COD2_ASSERT_SIZE(struct scr_saved_src_s, 8);

/* ---- Script value box helpers: singly linked list (value, next) ---- */
struct scr_list_node_s {
    uint32 value;
    struct scr_list_node_s *next;
};
COD2_ASSERT_SIZE(struct scr_list_node_s, 8);

struct scr_list_s {
    struct scr_list_node_s *head;
    struct scr_list_node_s *tail;
};
COD2_ASSERT_OFFSET(struct scr_list_s, tail, 4);
COD2_ASSERT_SIZE(struct scr_list_s, 8);

/* Globals and out-of-chunk callees come from cod2_globals.h / cod2_prototypes.h. */

#endif /* COD2_CHUNK_013_TYPES_H */
