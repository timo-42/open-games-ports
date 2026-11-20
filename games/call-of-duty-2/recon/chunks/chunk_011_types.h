/*
 * chunk 011 (0x0807085e..0x080736ec): script compiler, call/expression/control-flow emission.
 * Types are recovered from consistent offsets; all names are descriptive (binary is stripped).
 */
#ifndef COD2_CHUNK_011_TYPES_H
#define COD2_CHUNK_011_TYPES_H

#include "cod2_types.h"

struct ScriptNode;
struct ScriptList;
struct ScriptScope;

/* 4-byte slot of a parse-tree node whose interpretation depends on the node type. */
union ScriptArg {
    int i;
    unsigned u;
    float f;
    struct ScriptNode *node;
    struct ScriptList *list;
};
COD2_ASSERT_SIZE(ScriptArg, 4);

/* Parse-tree node: type tag followed by up to five type-dependent slots. */
struct ScriptNode {
    int type;
    ScriptArg a1;
    ScriptArg a2;
    ScriptArg a3;
    ScriptArg a4;
    ScriptArg a5;
};
COD2_ASSERT_OFFSET(ScriptNode, a1, 0x04);
COD2_ASSERT_OFFSET(ScriptNode, a2, 0x08);
COD2_ASSERT_OFFSET(ScriptNode, a3, 0x0c);
COD2_ASSERT_OFFSET(ScriptNode, a4, 0x10);
COD2_ASSERT_OFFSET(ScriptNode, a5, 0x14);

/* Observed node type tags (numeric tags observed; names are descriptive guesses). */
enum ScriptNodeType {
    SCR_NODE_WRAPPER = 6,
    SCR_NODE_INT_LITERAL = 7,
    SCR_NODE_FLOAT_LITERAL = 8,
    SCR_NODE_NEG_INT_LITERAL = 9,
    SCR_NODE_NEG_FLOAT_LITERAL = 10,
    SCR_NODE_STRING_LITERAL = 0x0b,
    SCR_NODE_ISTRING_LITERAL = 0x0c,
    SCR_NODE_LVALUE_PLAIN = 4,
    SCR_NODE_LVALUE_0D = 0x0d,
    SCR_NODE_LVALUE_0F = 0x0f,
    SCR_NODE_OBJECT_BASE = 0x11,
    SCR_NODE_IDENT_PATH = 0x12,
    SCR_NODE_CALL_EXPR = 0x13,
    SCR_NODE_IDENT = 0x14,
    SCR_NODE_CALL = 0x17,
    SCR_NODE_METHOD_CALL = 0x18,
    SCR_NODE_FUNC_REF = 0x1a,
    SCR_NODE_FILE_FUNC_REF = 0x1e,
    SCR_NODE_TYPE_1F = 0x1f,
    SCR_NODE_EXPR_LIST = 0x2e,
    SCR_NODE_OP_2F = 0x2f,
    SCR_NODE_OP_30 = 0x30,
    SCR_NODE_BINARY_OP = 0x31,
    SCR_NODE_BLOCK = 0x2c,
    SCR_NODE_COND_STMT = 0x41,
    SCR_NODE_LITERAL_FALSE = 0x48,
    SCR_NODE_LITERAL_TRUE = 0x49
};

/* Singly linked list of node references. */
struct ScriptNodeRef {
    struct ScriptNode *node;
    int srcPos;
};
COD2_ASSERT_SIZE(ScriptNodeRef, 8);

struct ScriptListItem {
    struct ScriptNodeRef *ref;
    struct ScriptListItem *next;
};
COD2_ASSERT_SIZE(ScriptListItem, 8);

struct ScriptList {
    struct ScriptListItem *head;
};

/* Compile-time constant (or evaluation stack entry). Stride 12. */
union ScriptValueData {
    int i;
    float f;
    void *p;
};

struct ScriptValue {
    ScriptValueData v;
    int type;      /* 4=vector, 5=float, 6=int; index into the type-name table */
    int srcPos;
};
COD2_ASSERT_OFFSET(ScriptValue, type, 0x04);
COD2_ASSERT_OFFSET(ScriptValue, srcPos, 0x08);
COD2_ASSERT_SIZE(ScriptValue, 12);

/* Control-flow/variable scope state; only the leading fields are used in this chunk. */
struct ScriptScope {
    int state;     /* 0 = falls through, 3 = initial value written by block compilers */
    int field4;    /* copied from a child scope by the loop compilers */
};
COD2_ASSERT_OFFSET(ScriptScope, field4, 0x04);


/* Pending jump patch entry (break/continue lists). */
struct ScriptPatchEntry {
    int *slot;
    int pos;
    struct ScriptPatchEntry *next;
};
COD2_ASSERT_OFFSET(ScriptPatchEntry, pos, 0x04);
COD2_ASSERT_OFFSET(ScriptPatchEntry, next, 0x08);

/* Source file table entry, stride 0x18, name pointer at +4. */
struct scr_frame_s {
    int field0;
    const char *name;
    int field8;
    int fieldC;
    int field10;
    int field14;
};
COD2_ASSERT_OFFSET(scr_frame_s, name, 0x04);
COD2_ASSERT_SIZE(scr_frame_s, 0x18);

static inline ScriptArg ScriptArg_FromNode(ScriptNode *n) { ScriptArg a; a.node = n; return a; }

/* ---- globals ---- */
/* 0x08206d0c lies in the 4-byte gap after g_scr_scope_list_secondary and has no symbol of its own in the data image. */
#define g_08206d0c (*(int **)((char *)&g_scr_scope_list_secondary + 4))

/* ---- out-of-chunk callees (placeholders) ---- */

/* ---- functions defined in this chunk ---- */

#endif
