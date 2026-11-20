/*
 * chunk_100_types.h -- cod2_lnxded 1.3 reconstruction, chunk 100.
 * Address range 0x0804b3cc..0x08075d58 (67 functions recovered from .eh_frame
 * FDE starts; Ghidra auto-analysis had not created them).
 *
 * Contents: precompiler (PC_*) source/define/token handling, script (PS_*)
 * token helpers, a ray/triangle test, BSP-plane winding helpers (BaseWindingForPlane,
 * ClipWinding), the console command table helpers (Cmd_*), dvar registration
 * commands, the file-system iwd checklist, Huffman message (de)compression
 * and a thin GSC compile entry.
 *
 * Layouts that other chunks already recovered (source_t, script_t, token_t,
 * define_t, huffman_s, node_s, msg_s, dvar_s, cmd_function_s) are mirrored here
 * with identical field names and offsets so the merge lane can de-duplicate them.
 */
#ifndef COD2_CHUNK_100_TYPES_H
#define COD2_CHUNK_100_TYPES_H

#include "cod2_types.h"

/* ------------------------------------------------------------------------ */
/* Precompiler / script tokens (PC_* and PS_* families).                    */
/* ------------------------------------------------------------------------ */
#define MAX_TOKEN_CHARS 1024
#define MAX_SOURCEFILES 64
#define DEFINEHASHSIZE 1024

enum {
    TT_STRING = 1,
    TT_LITERAL = 2,
    TT_NUMBER = 3,
    TT_NAME = 4,
    TT_PUNCTUATION = 5
};

enum {
    TT_DECIMAL = 0x0008,
    TT_HEX = 0x0100,
    TT_OCTAL = 0x0200,
    TT_BINARY = 0x0400,
    TT_FLOAT = 0x0800,
    TT_INTEGER = 0x1000,
    TT_LONG = 0x2000,
    TT_UNSIGNED = 0x4000
};

/* #if/#ifdef nesting entries (indent_t::type). */
#define INDENT_IF 0x0001
#define INDENT_ELSE 0x0002
#define INDENT_ELIF 0x0004
#define INDENT_IFNDEF 0x0010

/* One lexical token, 0x42c bytes. */
struct token_s {
    char string[MAX_TOKEN_CHARS];  /* 0x000 */
    int32 type;                    /* 0x400 */
    int32 subtype;                 /* 0x404 */
    uint32 intvalue;               /* 0x408 */
    long double floatvalue;        /* 0x40c (x87 80-bit) */
    char *whitespace_p;            /* 0x418 */
    char *endwhitespace_p;         /* 0x41c */
    int32 line;                    /* 0x420 */
    int32 linescrossed;            /* 0x424 */
    struct token_s *next;          /* 0x428 */
};
typedef struct token_s token_t;
COD2_ASSERT_OFFSET(token_t, type, 0x400);
COD2_ASSERT_OFFSET(token_t, subtype, 0x404);
COD2_ASSERT_OFFSET(token_t, intvalue, 0x408);
COD2_ASSERT_OFFSET(token_t, floatvalue, 0x40c);
COD2_ASSERT_OFFSET(token_t, linescrossed, 0x424);
COD2_ASSERT_OFFSET(token_t, next, 0x428);
COD2_ASSERT_SIZE(token_t, 0x42c);

/* Token record handed to script callers through the *Handle API:
 * type, subtype, intvalue, float value (stored as single precision), then the
 * string. The string starts at +0x10. */
struct pc_token_s {
    int32 type;                    /* 0x00 */
    int32 subtype;                 /* 0x04 */
    int32 intvalue;                /* 0x08 */
    float floatvalue;              /* 0x0c (fstp DWORD) */
    char string[MAX_TOKEN_CHARS];  /* 0x10 */
};
typedef struct pc_token_s pc_token_t;
COD2_ASSERT_OFFSET(pc_token_t, string, 0x10);
COD2_ASSERT_SIZE(pc_token_t, 0x410);

/* Punctuation table entry (opaque to this chunk). */
struct punctuation_s;

/* Script (file or memory buffer) being scanned, 0x4a4 bytes. */
struct script_s {
    char filename[64];                  /* 0x00 */
    char *buffer;                       /* 0x40 */
    char *script_p;                     /* 0x44 */
    char *end_p;                        /* 0x48 */
    char *lastscript_p;                 /* 0x4c */
    char *whitespace_p;                 /* 0x50 */
    char *endwhitespace_p;              /* 0x54 */
    int32 length;                       /* 0x58 */
    int32 line;                         /* 0x5c */
    int32 lastline;                     /* 0x60 */
    int32 tokenavailable;               /* 0x64 */
    int32 flags;                        /* 0x68 */
    struct punctuation_s *punctuations; /* 0x6c */
    struct punctuation_s **punctuationtable; /* 0x70 */
    token_t token;                      /* 0x74 */
    struct script_s *next;              /* 0x4a0 */
};
typedef struct script_s script_t;
COD2_ASSERT_OFFSET(script_t, script_p, 0x44);
COD2_ASSERT_OFFSET(script_t, lastscript_p, 0x4c);
COD2_ASSERT_OFFSET(script_t, line, 0x5c);
COD2_ASSERT_OFFSET(script_t, token, 0x74);
COD2_ASSERT_OFFSET(script_t, next, 0x4a0);
COD2_ASSERT_SIZE(script_t, 0x4a4);

/* #define entry, 0x20 bytes; the name string is stored right after it. */
struct define_s {
    char *name;                 /* 0x00 */
    int32 flags;                /* 0x04 */
    int32 builtin;              /* 0x08 */
    int32 numparms;             /* 0x0c */
    token_t *parms;             /* 0x10 */
    token_t *tokens;            /* 0x14 */
    struct define_s *next;      /* 0x18: global define list link */
    struct define_s *hashnext;  /* 0x1c */
};
typedef struct define_s define_t;
COD2_ASSERT_OFFSET(define_t, next, 0x18);
COD2_ASSERT_OFFSET(define_t, hashnext, 0x1c);
COD2_ASSERT_SIZE(define_t, 0x20);

/* #if nesting entry, 0x10 bytes. */
struct indent_s {
    int32 type;                 /* 0x00 */
    int32 skip;                 /* 0x04 */
    script_t *script;           /* 0x08 */
    struct indent_s *next;      /* 0x0c */
};
typedef struct indent_s indent_t;
COD2_ASSERT_OFFSET(indent_t, next, 0xc);
COD2_ASSERT_SIZE(indent_t, 0x10);

/* Preprocessor source, 0x4c8 bytes. */
struct source_s {
    char filename[64];          /* 0x00 */
    char includepath[64];       /* 0x40 */
    struct punctuation_s *punctuations; /* 0x80 */
    script_t *scriptstack;      /* 0x84: script_t::filename is the display name */
    token_t *tokens;            /* 0x88 */
    define_t *defines;          /* 0x8c */
    define_t **definehash;      /* 0x90 (DEFINEHASHSIZE slots) */
    indent_t *indentstack;      /* 0x94 */
    int32 skip;                 /* 0x98 */
    token_t token;              /* 0x9c */
};
typedef struct source_s source_t;
COD2_ASSERT_OFFSET(source_t, includepath, 0x40);
COD2_ASSERT_OFFSET(source_t, scriptstack, 0x84);
COD2_ASSERT_OFFSET(source_t, definehash, 0x90);
COD2_ASSERT_OFFSET(source_t, skip, 0x98);
COD2_ASSERT_OFFSET(source_t, token, 0x9c);
COD2_ASSERT_SIZE(source_t, 0x4c8);

/* Source handle table (g_08188ca0_sourceFiles, index 1..63). */
/* Global define list head (linked through define_t::next). */

/* ------------------------------------------------------------------------ */
/* Adaptive Huffman coder and bit message (mirrors chunk_008 layouts).      */
/* ------------------------------------------------------------------------ */
#define COD2_HUFF_NYT 0x100
#define COD2_HUFF_INTERNAL 0x101

struct node_s {
    struct node_s *left;      /* 0x00 */
    struct node_s *right;     /* 0x04 */
    struct node_s *parent;    /* 0x08 */
    struct node_s *next;      /* 0x0c */
    struct node_s *prev;      /* 0x10 */
    struct node_s **head;     /* 0x14 */
    int32 weight;             /* 0x18 */
    int32 symbol;             /* 0x1c */
};
COD2_ASSERT_SIZE(node_s, 0x20);
COD2_ASSERT_OFFSET(node_s, next, 0x0c);
COD2_ASSERT_OFFSET(node_s, prev, 0x10);

struct huffman_s {
    int32 blocNode;            /* 0x00: nodeList slots handed out */
    int32 blocPtrs;            /* 0x04: nodePtrs slots handed out */
    struct node_s *tree;       /* 0x08 */
    struct node_s *lhead;      /* 0x0c */
    struct node_s *ltail;      /* 0x10 */
    struct node_s *loc[257];   /* 0x14: loc[ch]; loc[256] is the NYT node */
    struct node_s **freelist;  /* 0x418 */
    struct node_s nodeList[768]; /* 0x41c */
    struct node_s *nodePtrs[768]; /* 0x641c */
};
COD2_ASSERT_SIZE(huffman_s, 0x701c);
COD2_ASSERT_OFFSET(huffman_s, loc, 0x14);
COD2_ASSERT_OFFSET(huffman_s, nodeList, 0x41c);


/* Huffman bit cursor (absolute bit index) shared by the bit helpers. */

/* ------------------------------------------------------------------------ */
/* Console commands, cvars (dvars) and system events.                       */
/* ------------------------------------------------------------------------ */

/* cmd_function_s: canonical (cod2_canon.h). */
typedef void (*cod2_name_cb_t)(const char *name);

/* dvar_s / DvarValue: canonical (cod2_canon.h). */

/* Head of the dvar list and its count. */

/* Dvar flag bits tested by the dvar commands. */
#define DVAR_CMD_BOOL_FLAG 0x4000
#define DVAR_TYPE_BOOL 0
#define DVAR_TYPE_FLOAT 1
#define DVAR_TYPE_INT 5
#define DVAR_TYPE_ENUM 6
#define DVAR_TYPE_STRING 7

/* sysEvent_s (6 words, type at +4) is provided by the shared cod2_types.h. */

/* Global command/state scalars used by these commands. */

/* ------------------------------------------------------------------------ */
/* File system: loaded search paths and iwd checksum tables.                */
/* ------------------------------------------------------------------------ */

/* searchpath_s / globals: canonical (cod2_canon.h, cod2_globals.h). */


/* ------------------------------------------------------------------------ */
/* Collision / BSP plane helpers.                                            */
/* ------------------------------------------------------------------------ */


/* Bytes of a DObj slot pool and its index table. */

/* ------------------------------------------------------------------------ */
/* Out-of-chunk callees that are not already in cod2_prototypes.h.          */
/* ------------------------------------------------------------------------ */
void PC_SourceError(source_t *source, const char *fmt, ...);
void PC_SourceWarning(source_t *source, const char *fmt, ...);
void PC_PushIndent(source_t *source, int type, int skip);
void PC_PopIndent(source_t *source, int *type, int *skip);
void PC_PushScript(source_t *source, script_t *script);
int PC_ReadSourceToken(source_t *source, token_t *token);
int PC_UnreadSourceToken(source_t *source, token_t *token);
void PC_AddDefineToHash(define_t *define, define_t **definehash);
define_t *PC_FindDefine(define_t *defines, const char *name);
void PC_FreeDefine(define_t *define);
void PC_ConvertPath(char *path);
int PC_ReadLine(source_t *source, token_t *token);
define_t *PC_DefineFromString(const char *string);
void PC_AddGlobalDefinesToSource(source_t *source);
int PC_DirectiveIfdefCommon(source_t *source, int type);
int PC_Evaluate(source_t *source, int *intvalue, double *floatvalue, int integer);
int PC_ReadToken(source_t *source, token_t *token);
void FreeSource(source_t *source);
void ScriptError(script_t *script, const char *fmt, ...);
int PS_ReadToken(script_t *script, token_t *token);
int PS_ExpectTokenType(script_t *script, int type, int subtype, token_t *token);
int PS_ExpectAnyToken(script_t *script, token_t *token);
void StripDoubleQuotes(char *string);
script_t *LoadScriptFile(char *filename);
script_t *LoadScriptMemory(const void *ptr, int length, const char *name);

/* Placeholders for callees with no merged name yet (rewritten by the merge lane). */

/* Huffman-side helpers from chunk_008 (declared in cod2_prototypes.h as well). */
void Huff_addRef(struct huffman_s *h, uint32 ch);
void Huff_transmit(struct huffman_s *h, int ch, byte *fout);
int Huff_Receive(struct node_s *node, int *ch, const byte *fin);
int Huff_getBit(const byte *fin);

/* Float bit pattern helper: the binary passes float arguments by value; the shared
 * prototypes take uint32 bit patterns for Dvar_RegisterFloat. */
static inline uint32 cod2_float_bits(float f)
{
    union {
        float f;
        uint32 u;
    } v;
    v.f = f;
    return v.u;
}

#endif /* COD2_CHUNK_100_TYPES_H */
