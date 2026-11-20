/*
 * Chunk 010 types: GSC script compiler code emission (0x806da84..0x807085e).
 * Evidence: error strings "VALUE_STACK_SIZE exceeded", "LOCAL_VAR_STACK_SIZE exceeded",
 * "MAX_SWITCH_CASES exceeded", "SCR_FUNC_TABLE_SIZE exceeded", "bad anim tree name",
 * "#using_animtree was not specified" in this chunk. Names of globals are descriptive
 * proposals (naming=inferred) and are recorded in analysis/symbols/chunk_010.json.
 */
#ifndef COD2_CHUNK_010_TYPES_H
#define COD2_CHUNK_010_TYPES_H

#include "cod2_types.h"


/* 4-byte struct returned through a hidden pointer (ret 4) by 0x806df16. */
struct ScrAnimTreeRef {
    int value;
};
COD2_ASSERT_SIZE(ScrAnimTreeRef, 4);

/* Local-variable scope block used by the script compiler (copied with a 0x118-byte block copy). */
#define SCR_LOCAL_BLOCK_MAX_VARS 0x40
struct ScriptScope {
    int inactive;                               /* +0x00 0 while the block can still record variables */
    int emittedCount;                           /* +0x04 locals already created at run time */
    int targetCount;                            /* +0x08 locals the block has to end up with */
    int varCount;                               /* +0x0c number of entries in vars[] */
    unsigned char initBits[8];                  /* +0x10 bit per vars[] entry: initialised */
    int vars[SCR_LOCAL_BLOCK_MAX_VARS];         /* +0x18 canonical string ids */
};
COD2_ASSERT_OFFSET(ScriptScope, inactive, 0x00);
COD2_ASSERT_OFFSET(ScriptScope, emittedCount, 0x04);
COD2_ASSERT_OFFSET(ScriptScope, targetCount, 0x08);
COD2_ASSERT_OFFSET(ScriptScope, varCount, 0x0c);
COD2_ASSERT_OFFSET(ScriptScope, initBits, 0x10);
COD2_ASSERT_OFFSET(ScriptScope, vars, 0x18);
COD2_ASSERT_SIZE(ScriptScope, 0x118);

/* Deferred constant (value stack) entry, 12 bytes each. */
enum ScrDeferredType {
    SCR_DEFER_UNDEFINED = 0,
    SCR_DEFER_STRING = 2,
    SCR_DEFER_ISTRING = 3,
    SCR_DEFER_VECTOR = 4,
    SCR_DEFER_FLOAT = 5,
    SCR_DEFER_INTEGER = 6
};
struct ScriptValue {
    int value;          /* +0 payload: int, float bits, string id or vec3 pointer */
    int type;           /* +4 ScrDeferredType */
    unsigned sourcePos; /* +8 source position for error/debug records */
};
COD2_ASSERT_OFFSET(ScriptValue, type, 4);
COD2_ASSERT_OFFSET(ScriptValue, sourcePos, 8);
COD2_ASSERT_SIZE(ScriptValue, 12);

/* Syntax tree node: node->type selects the meaning of operands a..d. */
struct ScriptNode {
    int type;       /* +0x00 */
    int a;          /* +0x04 */
    int b;          /* +0x08 */
    int c;          /* +0x0c */
    int d;          /* +0x10 */
};
COD2_ASSERT_OFFSET(ScriptNode, a, 4);
COD2_ASSERT_OFFSET(ScriptNode, d, 0x10);

/* Singly linked list of nodes: head->item / head->next. */
struct ScriptListItem {
    struct ScriptNode *item;           /* +0 */
    struct ScriptListItem *next;   /* +4 */
};
COD2_ASSERT_SIZE(ScriptListItem, 8);
struct ScriptList {
    struct ScriptListItem *head;   /* +0 */
};

/* Record appended per compiled function (12-byte stride). */
struct ScrFuncRecord {
    unsigned short name;    /* +0 */
    unsigned char flag;     /* +2 */
    unsigned char pad;      /* +3 */
    int sourcePos;          /* +4 */
    int reserved;           /* +8 (not written in this chunk) */
};
COD2_ASSERT_OFFSET(ScrFuncRecord, flag, 2);
COD2_ASSERT_OFFSET(ScrFuncRecord, sourcePos, 4);
COD2_ASSERT_SIZE(ScrFuncRecord, 12);

#define SCR_VALUE_STACK_SIZE 0x20
#define SCR_ANIMTREE_SLOTS 0x80
#define SCR_FUNC_TABLE_MAX 0x400

/* ---- globals (addresses in comments) ---- */

/* ---- out-of-chunk callees (placeholders; merge lane rewrites names) ---- */

#endif
