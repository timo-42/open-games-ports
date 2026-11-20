/*
 * Chunk 060 (cod2_lnxded 1.3, 0x08138e64..0x813b8fe) types and out-of-chunk prototypes.
 *
 * Covers: the codepage-aware character reader used by localized text, the language
 * table, path helpers, recursive collection of localized .str files, the .str line
 * parser (VERSION / CONFIG / FILENOTES / NOTES / FLAGS / REFERENCE / ENDMARKER / LANG_),
 * and the red-black-tree map (std::map<std::string, std::string>) that holds entries.
 *
 * std::string here is the libstdc++ 3.x copy-on-write handle: one pointer (4 bytes).
 * Its operations are dynamic imports (chunk_000 PLT thunks, chunk_062 EXTERNAL entries).
 * They are declared below as cod2_string_* shims, each commented with the imported
 * mangled symbol it stands for.
 *
 * The red-black tree follows libstdc++ 3.x _Rb_tree (node = 0x18 bytes, sentinel header
 * allocated separately). Node colour 0 = red.
 *
 * Out-of-chunk callees named fn_08xxxxxx are placeholders for the merge queue. Unnamed
 * globals are g_08xxxxxx. Merged names (FS_*, Q_*, va, Com_*) are taken from symbols.json.
 */
#ifndef COD2_CHUNK_060_TYPES_H
#define COD2_CHUNK_060_TYPES_H

#include <ctype.h>
#include <strings.h>

/* ---- std::string (libstdc++ 3.x COW handle) ---- */
struct cod2_string_s {
    void *rep;                       /* pointer to the shared _Rep; 4 bytes in this ABI */
};
COD2_ASSERT_SIZE(struct cod2_string_s, 4);

/* std::string::string()                         _ZNSsC1Ev */
void cod2_string_init(struct cod2_string_s *s) __asm__("_ZNSsC1Ev");
/* std::string::string(const std::string &)      _ZNSsC1ERKSs */
void cod2_string_init_copy(struct cod2_string_s *s, const struct cod2_string_s *src) __asm__("_ZNSsC1ERKSs");
/* std::string::string(const char *)             _ZNSsC1EPKcRKSaIcE */
void cod2_string_init_cstr(struct cod2_string_s *s, const char *text) __asm__("_ZNSsC1EPKcRKSaIcE");
/* std::string::~string()                        _ZNSsD1Ev */
void cod2_string_destroy(struct cod2_string_s *s) __asm__("_ZNSsD1Ev");
/* std::string::operator=(const std::string &)   _ZNSsaSERKSs */
void cod2_string_assign(struct cod2_string_s *s, const struct cod2_string_s *src) __asm__("_ZNSsaSERKSs");
/* std::string::operator=(const char *)          _ZNSsaSEPKc */
void cod2_string_assign_cstr(struct cod2_string_s *s, const char *text) __asm__("_ZNSsaSEPKc");
/* std::string::operator+=(const char *)         _ZNSspLEPKc */
void cod2_string_append_cstr(struct cod2_string_s *s, const char *text) __asm__("_ZNSspLEPKc");
/* std::string::operator+=(char)                 _ZNSspLEc */
void cod2_string_append_char(struct cod2_string_s *s, char c) __asm__("_ZNSspLEc");
/* std::string::c_str() const                    _ZNKSs5c_strEv */
const char *cod2_string_c_str(const struct cod2_string_s *s) __asm__("_ZNKSs5c_strEv");
/* std::string::compare(const std::string &) const  _ZNKSs7compareERKSs; <0 when s < other */
int cod2_string_compare(const struct cod2_string_s *s, const struct cod2_string_s *other) __asm__("_ZNKSs7compareERKSs");
/* std::string::find(const char *, size_t pos) const  _ZNKSs4findEPKcj; npos = 0xffffffff */
uint32 cod2_string_find_cstr(const struct cod2_string_s *s, const char *needle, uint32 pos) __asm__("_ZNKSs4findEPKcj");
/* std::string::erase(size_t pos, size_t len)    _ZNSs5eraseEjj; len 0xffffffff = to end */
void cod2_string_erase(struct cod2_string_s *s, uint32 pos, uint32 len) __asm__("_ZNSs5eraseEjj");
/* std::string::operator[](size_t)               _ZNSsixEj */
char *cod2_string_at(struct cod2_string_s *s, uint32 pos) __asm__("_ZNSsixEj");

/* ---- red-black tree map: std::map<std::string, std::string> (libstdc++ 3.x layout) ---- */
struct cod2_rb_node_s {
    int32 color;                     /* +0x00 _Rb_tree_node colour; 0 = red; sentinel header is red */
    struct cod2_rb_node_s *parent;   /* +0x04 */
    struct cod2_rb_node_s *left;     /* +0x08 */
    struct cod2_rb_node_s *right;    /* +0x0c */
    struct cod2_string_s key;        /* +0x10 value_type.first */
    struct cod2_string_s value;      /* +0x14 value_type.second */
};
COD2_ASSERT_OFFSET(struct cod2_rb_node_s, parent, 0x04);
COD2_ASSERT_OFFSET(struct cod2_rb_node_s, left, 0x08);
COD2_ASSERT_OFFSET(struct cod2_rb_node_s, right, 0x0c);
COD2_ASSERT_OFFSET(struct cod2_rb_node_s, key, 0x10);
COD2_ASSERT_OFFSET(struct cod2_rb_node_s, value, 0x14);
COD2_ASSERT_SIZE(struct cod2_rb_node_s, 0x18);

struct cod2_str_map_s {
    uint32 node_allocator_pad;       /* +0x00 empty _Node_allocator base; not read */
    struct cod2_rb_node_s *header;   /* +0x04 _M_header: sentinel node (malloc(0x18)) */
    uint32 node_count;               /* +0x08 _M_node_count */
    uint8 key_compare;               /* +0x0c empty key comparator; passed as `this`, not read */
};
COD2_ASSERT_OFFSET(struct cod2_str_map_s, header, 0x04);
COD2_ASSERT_OFFSET(struct cod2_str_map_s, node_count, 0x08);
COD2_ASSERT_OFFSET(struct cod2_str_map_s, key_compare, 0x0c);
COD2_ASSERT_SIZE(struct cod2_str_map_s, 0x10);

/* Result of _M_insert_unique: pair<iterator, bool>. */
struct cod2_rb_insert_s {
    struct cod2_rb_node_s *first;
    uint8 second;
};
COD2_ASSERT_SIZE(struct cod2_rb_insert_s, 8);

/* ---- localized-string table (global DAT_08a6f1a0 points at one; allocated 0x20 bytes) ---- */
struct cod2_loc_table_s {
    int32 end_marker_seen;           /* +0x00 set to 1 by ENDMARKER; cleared by reset */
    struct cod2_string_s last_keyword;   /* +0x04 last REFERENCE key; target of LANG_ lines */
    struct cod2_string_s last_english;   /* +0x08 last English value; default for "#same" */
    struct cod2_string_s file_base;      /* +0x0c upper-cased base name of the file being read */
    struct cod2_str_map_s entries;       /* +0x10 "BASE_KEY" -> text */
};
COD2_ASSERT_OFFSET(struct cod2_loc_table_s, last_keyword, 0x04);
COD2_ASSERT_OFFSET(struct cod2_loc_table_s, last_english, 0x08);
COD2_ASSERT_OFFSET(struct cod2_loc_table_s, file_base, 0x0c);
COD2_ASSERT_OFFSET(struct cod2_loc_table_s, entries, 0x10);
COD2_ASSERT_SIZE(struct cod2_loc_table_s, 0x20);

/* Language table entry (loc_languageTable, 8-byte stride, 15 entries). */
struct cod2_lang_entry_s {
    const char *name;
    uint32 reserved;                 /* always 0 in the table */
};
COD2_ASSERT_SIZE(struct cod2_lang_entry_s, 8);

/* ---- chunk-local data ---- */

/* ---- out-of-chunk callees (placeholders; merged names come from symbols.json) ---- */
/* RbTree_InsertWithHint (_M_insert_node, chunk 061): declared in cod2_prototypes.h as
 * void *(void *out, cod2_str_map_s *, node *x, node *y, rb_pair_s *value); out receives the new node. */

/* Merged names from symbols.json (other lanes). */

#endif
