/*
 * Chunk 002 types: script (l_script-style) and source (l_precomp-style) tokenizer,
 * plus BSP file loader / clipmap slot helpers at the end of the chunk.
 * Layouts recovered from consistent field offsets in 0x804e618..0x805271c.
 */
#ifndef COD2_CHUNK_002_TYPES_H
#define COD2_CHUNK_002_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "chunk_003_types.h" /* clipMap_s and BSP structs (same lane) */

#define MAX_TOKEN_CHARS 1024

/* token types (observed as immediate constants) */
enum {
    TT_STRING = 1,
    TT_LITERAL = 2,
    TT_NUMBER = 3,
    TT_NAME = 4,
    TT_PUNCTUATION = 5
};

/* number token subtypes */
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

/* script flags (script_t::flags at +0x68) */
enum {
    SCFL_NOERRORS = 0x0001,
    SCFL_NOWARNINGS = 0x0002,
    SCFL_NOSTRINGWHITESPACES = 0x0004,
    SCFL_NOSTRINGESCAPECHARS = 0x0008,
    SCFL_PRIMITIVE = 0x0010
};

/* conditional-compilation indent types */
enum {
    INDENT_ELSE = 0x0002,
    INDENT_ELIF = 0x0004
};

#define P_SUB 30 /* subtype of the "-" punctuation token (0x1e) */

#define MAX_SOURCEFILES 64
#define DEFINEHASHSIZE 1024

struct punctuation_s {
    char *p;                    /* 0x00 punctuation text */
    int n;                      /* 0x04 punctuation number */
    struct punctuation_s *next; /* 0x08 next in table chain */
};
typedef struct punctuation_s punctuation_t;
COD2_ASSERT_SIZE(punctuation_t, 0xc);

struct token_s {
    char string[MAX_TOKEN_CHARS];  /* 0x000 */
    int type;                      /* 0x400 */
    int subtype;                   /* 0x404 */
    unsigned int intvalue;         /* 0x408 */
    long double floatvalue;        /* 0x40c (12 bytes on i386) */
    char *whitespace_p;            /* 0x418 */
    char *endwhitespace_p;         /* 0x41c */
    int line;                      /* 0x420 */
    int linescrossed;              /* 0x424 */
    struct token_s *next;          /* 0x428 */
};
typedef struct token_s token_t;
COD2_ASSERT_OFFSET(token_t, type, 0x400);
COD2_ASSERT_OFFSET(token_t, subtype, 0x404);
COD2_ASSERT_OFFSET(token_t, intvalue, 0x408);
COD2_ASSERT_OFFSET(token_t, floatvalue, 0x40c);
COD2_ASSERT_OFFSET(token_t, whitespace_p, 0x418);
COD2_ASSERT_OFFSET(token_t, endwhitespace_p, 0x41c);
COD2_ASSERT_OFFSET(token_t, line, 0x420);
COD2_ASSERT_OFFSET(token_t, linescrossed, 0x424);
COD2_ASSERT_OFFSET(token_t, next, 0x428);
COD2_ASSERT_SIZE(token_t, 0x42c);

/* 0x42c bytes are copied by memcpy for "the token without next" */
#define TOKEN_COPY_SIZE 0x42c

struct script_s {
    char filename[64];                  /* 0x00 */
    char *buffer;                       /* 0x40 */
    char *script_p;                     /* 0x44 */
    char *end_p;                        /* 0x48 */
    char *lastscript_p;                 /* 0x4c */
    char *whitespace_p;                 /* 0x50 */
    char *endwhitespace_p;              /* 0x54 */
    int length;                         /* 0x58 */
    int line;                           /* 0x5c */
    int lastline;                       /* 0x60 */
    int tokenavailable;                 /* 0x64 */
    int flags;                          /* 0x68 */
    punctuation_t *punctuations;        /* 0x6c */
    punctuation_t **punctuationtable;   /* 0x70 (256 entries) */
    token_t token;                      /* 0x74 */
    struct script_s *next;              /* 0x4a0 */
};
typedef struct script_s script_t;
COD2_ASSERT_OFFSET(script_t, buffer, 0x40);
COD2_ASSERT_OFFSET(script_t, script_p, 0x44);
COD2_ASSERT_OFFSET(script_t, end_p, 0x48);
COD2_ASSERT_OFFSET(script_t, lastscript_p, 0x4c);
COD2_ASSERT_OFFSET(script_t, whitespace_p, 0x50);
COD2_ASSERT_OFFSET(script_t, endwhitespace_p, 0x54);
COD2_ASSERT_OFFSET(script_t, length, 0x58);
COD2_ASSERT_OFFSET(script_t, line, 0x5c);
COD2_ASSERT_OFFSET(script_t, lastline, 0x60);
COD2_ASSERT_OFFSET(script_t, tokenavailable, 0x64);
COD2_ASSERT_OFFSET(script_t, flags, 0x68);
COD2_ASSERT_OFFSET(script_t, punctuations, 0x6c);
COD2_ASSERT_OFFSET(script_t, punctuationtable, 0x70);
COD2_ASSERT_OFFSET(script_t, token, 0x74);
COD2_ASSERT_OFFSET(script_t, next, 0x4a0);
COD2_ASSERT_SIZE(script_t, 0x4a4);

struct define_s {
    char *name;                 /* 0x00 */
    int flags;                  /* 0x04 */
    int builtin;                /* 0x08 */
    int numparms;               /* 0x0c */
    token_t *parms;             /* 0x10 */
    token_t *tokens;            /* 0x14 */
    struct define_s *next;      /* 0x18 */
    struct define_s *hashnext;  /* 0x1c */
};
typedef struct define_s define_t;
COD2_ASSERT_OFFSET(define_t, hashnext, 0x1c);
COD2_ASSERT_SIZE(define_t, 0x20);

struct indent_s {
    int type;                   /* 0x00 */
    int skip;                   /* 0x04 */
    script_t *script;           /* 0x08 */
    struct indent_s *next;      /* 0x0c */
};
typedef struct indent_s indent_t;
COD2_ASSERT_OFFSET(indent_t, next, 0xc);
COD2_ASSERT_SIZE(indent_t, 0x10);

struct source_s {
    char filename[64];          /* 0x00 */
    char includepath[64];       /* 0x40 */
    punctuation_t *punctuations;/* 0x80 */
    script_t *scriptstack;      /* 0x84 */
    token_t *tokens;            /* 0x88 */
    define_t *defines;          /* 0x8c */
    define_t **definehash;      /* 0x90 (DEFINEHASHSIZE entries) */
    indent_t *indentstack;      /* 0x94 */
    int skip;                   /* 0x98 */
    token_t token;              /* 0x9c */
};
typedef struct source_s source_t;
COD2_ASSERT_OFFSET(source_t, includepath, 0x40);
COD2_ASSERT_OFFSET(source_t, punctuations, 0x80);
COD2_ASSERT_OFFSET(source_t, scriptstack, 0x84);
COD2_ASSERT_OFFSET(source_t, tokens, 0x88);
COD2_ASSERT_OFFSET(source_t, defines, 0x8c);
COD2_ASSERT_OFFSET(source_t, definehash, 0x90);
COD2_ASSERT_OFFSET(source_t, indentstack, 0x94);
COD2_ASSERT_OFFSET(source_t, skip, 0x98);
COD2_ASSERT_OFFSET(source_t, token, 0x9c);
COD2_ASSERT_SIZE(source_t, 0x4c8);

/* name/handler pair in the directive tables (null-name terminated) */
struct directive_s {
    const char *name;
    int (*func)(source_t *source);
};
typedef struct directive_s directive_t;
COD2_ASSERT_SIZE(directive_t, 8);

/* Two-entry table of per-slot buffers at 0x085b7660, stride 0x18 */
struct cm_slot_s {
    int field_00;               /* 0x00 zeroed */
    void *buf_04;               /* 0x04 g_08188e0c * 4 bytes */
    void *buf_08;               /* 0x08 g_08188e04 * 4 bytes */
    void *buf_0c;               /* 0x0c g_08188e24 * 2 bytes */
    int *copy_10;               /* 0x10 0x30-byte copy of *g_08188e5c */
    int *copy_14;               /* 0x14 0x48-byte copy of g_08188e60 */
};
typedef struct cm_slot_s cm_slot_t;
COD2_ASSERT_OFFSET(cm_slot_t, buf_04, 0x4);
COD2_ASSERT_OFFSET(cm_slot_t, copy_14, 0x14);
COD2_ASSERT_SIZE(cm_slot_t, 0x18);

#endif /* COD2_CHUNK_002_TYPES_H */
