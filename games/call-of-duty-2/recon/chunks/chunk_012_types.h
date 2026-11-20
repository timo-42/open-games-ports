/*
 * chunk_012 types: Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 012.
 * Target SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * Scope: script (GSC) compiler code generation and statement/switch/loop walkers,
 * the function/include/symbol front end, and the 16-bit bucket tree allocator with its
 * statistics dump (0x08073902..0x08076be0).
 *
 * Conventions:
 *  - Parse-tree node words are `int`. Ghidra `param_1[n]` (n >= 1) is `op[n - 1]`.
 *    Pointers stored in node words are cast at use (`(struct ScriptNode *)op[0]`).
 *  - External callees are `fn_08xxxxxx` placeholders with unresolved arity (varargs).
 *    The merge lane replaces them with real prototypes.
 *  - Globals are `g_08xxxxxx` (no symbol name known). Their types follow observed use.
 */
#ifndef COD2_CHUNK_012_TYPES_H
#define COD2_CHUNK_012_TYPES_H

#include "cod2_types.h"
#include <ctype.h>

/* ---------------------------------------------------------------------------
 * Script parse tree.
 * ------------------------------------------------------------------------- */

/* Statement/expression record produced by the GSC front end. kind selects the
 * handler; op[0..7] are the operands (Ghidra words 1..8). */
struct ScriptNode {
    int kind;
    int op[8];
};
COD2_ASSERT_OFFSET(struct ScriptNode, op, 4);

/* Singly linked list cell. item is a pointer to the element record. */
struct ScriptListItem {
    int item;
    struct ScriptListItem *next;
};
COD2_ASSERT_SIZE(struct ScriptListItem, 8);
COD2_ASSERT_OFFSET(struct ScriptListItem, next, 4);

/* 4-byte slot of a parse-tree node (same layout as the chunk 011 view). */
union ScriptArg {
    int i;
    unsigned u;
    float f;
    struct ScriptNode *node;
    struct ScriptList *list;
};
typedef union ScriptArg ScriptArg;
struct ScriptList;

/* Container that owns a list. Reached through a word holding a seq pointer
 * (callers pass `int *` whose target word is this struct's address). */
struct cod2_scr_seq {
    int unused0;
    struct ScriptListItem *list;
};
COD2_ASSERT_OFFSET(struct cod2_scr_seq, list, 4);

/* Element of lists whose items are value/label or first/second pairs. */
struct ScriptNodeRef {
    int first;
    int second;
};
COD2_ASSERT_SIZE(struct ScriptNodeRef, 8);

/* Case-label record in the active switch (allocated 0x10 bytes). */
struct cod2_case_entry {
    int value;
    int label;
    int location;
    struct cod2_case_entry *next;
};
COD2_ASSERT_SIZE(struct cod2_case_entry, 0x10);
COD2_ASSERT_OFFSET(struct cod2_case_entry, next, 0xc);

/* Sort record for qsort() over case entries (8 bytes, value then label). */
struct cod2_case_sort_pair {
    uint32 value;
    int label;
};
COD2_ASSERT_SIZE(struct cod2_case_sort_pair, 8);

/* break/continue patch record (allocated 0xc bytes). */
struct ScriptPatchEntry {
    int patch_site;
    int label;
    struct ScriptPatchEntry *next;
};
COD2_ASSERT_SIZE(struct ScriptPatchEntry, 0xc);
COD2_ASSERT_OFFSET(struct ScriptPatchEntry, next, 8);

/* Include/script reference record in the g_scr_dependency_record_count table (12 bytes). */
struct ScrFuncRecord {
    uint16 script_id;
    uint8 pending;
    uint8 reserved;
    int location;
    int chain;
};
COD2_ASSERT_SIZE(struct ScrFuncRecord, 12);
COD2_ASSERT_OFFSET(struct ScrFuncRecord, location, 4);
COD2_ASSERT_OFFSET(struct ScrFuncRecord, chain, 8);

/* Function compile context (allocated 0x118 bytes). state is the word that the
 * statement walkers use as their `int *state` argument. */
struct ScriptScope {
    int state;
    int field_4;
    int field_8;
    int field_c;
    int field_10[2];
    uint8 body[0x118 - 0x18];
};
COD2_ASSERT_SIZE(struct ScriptScope, 0x118);
COD2_ASSERT_OFFSET(struct ScriptScope, field_10, 0x10);

/* Free-bucket tree node (pool of 0x10000 entries, 8 bytes each). child[] are
 * 16-bit pool indices, aux is the stored size/priority word. */
struct cod2_bucket_node {
    uint16 child[2];
    uint32 aux;
};
COD2_ASSERT_SIZE(struct cod2_bucket_node, 8);
COD2_ASSERT_OFFSET(struct cod2_bucket_node, aux, 4);

/* ---------------------------------------------------------------------------
 * Globals (addresses as in the binary; names unknown, see g_ placeholder rule).
 * ------------------------------------------------------------------------- */

/* Compile-state and symbol tables, 0x0820xxxx to 0x0820cxxx. */

/* Code-generation state. */

/* Free-bucket allocator. */

/* Front-end state. */

/* ---------------------------------------------------------------------------
 * Out-of-chunk callees. Arity and types unresolved; merge lane replaces these.
 * ------------------------------------------------------------------------- */




#endif
