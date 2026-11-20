/*
 * chunk_001 types: the Quake III / IW "botlib" style C preprocessor (precompiler)
 * that lives at 0x0804a710..0x0804e26c in cod2_lnxded 1.3.
 * Layouts are recovered from consistent field offsets in this chunk; fields not
 * touched by chunk_001 are inferred from the id Tech 3 botlib counterpart and are
 * marked "inferred" below.
 */
#ifndef CHUNK_001_TYPES_H
#define CHUNK_001_TYPES_H

#include "cod2_types.h"

#define PC_MAX_TOKEN_CHARS 1024
#define PC_MAX_PATH 64
#define PC_MAX_DEFINEPARMS 128
#define PC_DEFINEHASHSIZE 1024
#define PC_MAX_VALUES 64
#define PC_MAX_OPERATORS 64

/* token types (observed as comparison constants) */
#define PC_TT_STRING 1
#define PC_TT_LITERAL 2
#define PC_TT_NUMBER 3
#define PC_TT_NAME 4
#define PC_TT_PUNCTUATION 5

/* number subtypes (observed: 0x1008 written by __LINE__ expansion) */
#define PC_TT_DECIMAL 0x0008
#define PC_TT_INTEGER 0x1000

/* builtin define kinds (observed in define_t.builtin switch) */
#define PC_BUILTIN_LINE 1
#define PC_BUILTIN_FILE 2
#define PC_BUILTIN_DATE 3
#define PC_BUILTIN_TIME 4

/* define flags: bit 0 = fixed / cannot be undefined or redefined (observed) */
#define PC_DEFINE_FIXED 1

/* indent kinds (observed: 8 is passed by the #ifdef entry point) */
#define PC_INDENT_IFDEF 0x0008

/* punctuation sub-types (observed numerically in operator handling) */
enum {
    PC_P_LOGIC_AND = 5,
    PC_P_LOGIC_OR = 6,
    PC_P_LOGIC_GEQ = 7,
    PC_P_LOGIC_LEQ = 8,
    PC_P_LOGIC_EQ = 9,
    PC_P_LOGIC_UNEQ = 10,
    PC_P_INC = 16,
    PC_P_DEC = 17,
    PC_P_RSHIFT = 21,
    PC_P_LSHIFT = 22,
    PC_P_MUL = 26,
    PC_P_DIV = 27,
    PC_P_MOD = 28,
    PC_P_ADD = 29,
    PC_P_SUB = 30,
    PC_P_BIN_AND = 32,
    PC_P_BIN_OR = 33,
    PC_P_BIN_XOR = 34,
    PC_P_BIN_NOT = 35,
    PC_P_LOGIC_NOT = 36,
    PC_P_LOGIC_GREATER = 37,
    PC_P_LOGIC_LESS = 38,
    PC_P_COLON = 42,
    PC_P_QUESTIONMARK = 43,
    PC_P_PARENTHESESOPEN = 44,
    PC_P_PARENTHESESCLOSE = 45
};

struct punctuation_s;
struct define_s;

/* One lexical token, 0x42c bytes. */
typedef struct token_s {
    char string[PC_MAX_TOKEN_CHARS];  /* 0x000 */
    int type;                         /* 0x400 */
    int subtype;                      /* 0x404 */
    unsigned int intvalue;            /* 0x408 */
    long double floatvalue;           /* 0x40c (x87 80-bit, 12 bytes) */
    char *whitespace_p;               /* 0x418 */
    char *endwhitespace_p;            /* 0x41c */
    int line;                         /* 0x420 */
    int linescrossed;                 /* 0x424 */
    struct token_s *next;             /* 0x428 */
} token_t;
COD2_ASSERT_SIZE(token_t, 0x42c);
COD2_ASSERT_OFFSET(token_t, type, 0x400);
COD2_ASSERT_OFFSET(token_t, subtype, 0x404);
COD2_ASSERT_OFFSET(token_t, intvalue, 0x408);
COD2_ASSERT_OFFSET(token_t, floatvalue, 0x40c);
COD2_ASSERT_OFFSET(token_t, whitespace_p, 0x418);
COD2_ASSERT_OFFSET(token_t, endwhitespace_p, 0x41c);
COD2_ASSERT_OFFSET(token_t, line, 0x420);
COD2_ASSERT_OFFSET(token_t, linescrossed, 0x424);
COD2_ASSERT_OFFSET(token_t, next, 0x428);

/* A script being scanned (file or memory buffer), 0x4a4 bytes.
 * Observed: filename@0, line@0x5c, next@0x4a0. Others inferred. */
typedef struct script_s {
    char filename[PC_MAX_PATH];        /* 0x000 */
    char *buffer;                      /* 0x040 inferred */
    char *script_p;                    /* 0x044 inferred */
    char *end_p;                       /* 0x048 inferred */
    char *lastscript_p;                /* 0x04c inferred */
    char *whitespace_p;                /* 0x050 inferred */
    char *endwhitespace_p;             /* 0x054 inferred */
    int length;                        /* 0x058 inferred */
    int line;                          /* 0x05c observed */
    int lastline;                      /* 0x060 inferred */
    int tokenavailable;                /* 0x064 inferred */
    int flags;                         /* 0x068 inferred */
    struct punctuation_s *punctuations;      /* 0x06c inferred */
    struct punctuation_s **punctuationtable; /* 0x070 inferred */
    token_t token;                     /* 0x074 inferred */
    struct script_s *next;             /* 0x4a0 observed */
} script_t;
COD2_ASSERT_OFFSET(script_t, line, 0x5c);
COD2_ASSERT_OFFSET(script_t, token, 0x74);
COD2_ASSERT_OFFSET(script_t, next, 0x4a0);
COD2_ASSERT_SIZE(script_t, 0x4a4);

/* A #define, 0x20 bytes (name storage is allocated right after it). */
typedef struct define_s {
    char *name;               /* 0x00 */
    int flags;                /* 0x04 */
    int builtin;              /* 0x08 */
    int numparms;             /* 0x0c */
    token_t *parms;           /* 0x10 */
    token_t *tokens;          /* 0x14 */
    struct define_s *next;    /* 0x18 */
    struct define_s *hashnext;/* 0x1c */
} define_t;
COD2_ASSERT_SIZE(define_t, 0x20);
COD2_ASSERT_OFFSET(define_t, numparms, 0x0c);
COD2_ASSERT_OFFSET(define_t, hashnext, 0x1c);

/* #if/#ifdef nesting entry, 0x10 bytes. */
typedef struct indent_s {
    int type;                 /* 0x00 */
    int skip;                 /* 0x04 */
    script_t *script;         /* 0x08 */
    struct indent_s *next;    /* 0x0c */
} indent_t;
COD2_ASSERT_SIZE(indent_t, 0x10);

/* Preprocessor source, 0x4c8 bytes. */
typedef struct source_s {
    char filename[PC_MAX_PATH];        /* 0x00 */
    char includepath[PC_MAX_PATH];     /* 0x40 inferred */
    struct punctuation_s *punctuations;/* 0x80 inferred */
    script_t *scriptstack;             /* 0x84 */
    token_t *tokens;                   /* 0x88 */
    define_t *defines;                 /* 0x8c */
    define_t **definehash;             /* 0x90 */
    indent_t *indentstack;             /* 0x94 */
    int skip;                          /* 0x98 */
    token_t token;                     /* 0x9c inferred */
} source_t;
COD2_ASSERT_OFFSET(source_t, scriptstack, 0x84);
COD2_ASSERT_OFFSET(source_t, tokens, 0x88);
COD2_ASSERT_OFFSET(source_t, defines, 0x8c);
COD2_ASSERT_OFFSET(source_t, definehash, 0x90);
COD2_ASSERT_OFFSET(source_t, indentstack, 0x94);
COD2_ASSERT_OFFSET(source_t, skip, 0x98);
COD2_ASSERT_SIZE(source_t, 0x4c8);

/* Expression evaluator value stack entry, 0x18 bytes. */
typedef struct pc_value_s {
    int intvalue;              /* 0x00 */
    double floatvalue;         /* 0x04 (4-byte aligned on i386) */
    int parentheses;           /* 0x0c */
    struct pc_value_s *prev;   /* 0x10 */
    struct pc_value_s *next;   /* 0x14 */
} pc_value_t;
COD2_ASSERT_SIZE(pc_value_t, 0x18);
COD2_ASSERT_OFFSET(pc_value_t, floatvalue, 4);
COD2_ASSERT_OFFSET(pc_value_t, next, 0x14);

/* Expression evaluator operator stack entry, 0x14 bytes. */
typedef struct pc_operator_s {
    int op;                    /* 0x00 punctuation subtype */
    int priority;              /* 0x04 */
    int parentheses;           /* 0x08 */
    struct pc_operator_s *prev;/* 0x0c */
    struct pc_operator_s *next;/* 0x10 */
} pc_operator_t;
COD2_ASSERT_SIZE(pc_operator_t, 0x14);

/* ---- in-chunk functions ---- */



void PC_SourceError(source_t *source, const char *fmt, ...);
void PC_SourceWarning(source_t *source, const char *fmt, ...);
void PC_PushIndent(source_t *source, int type, int skip);
void PC_PopIndent(source_t *source, int *type, int *skip);
void PC_PushScript(source_t *source, script_t *script);
token_t *PC_CopyToken(token_t *t);
void PC_FreeToken(token_t *token);
int PC_ReadSourceToken(source_t *source, token_t *token);
int PC_UnreadSourceToken(source_t *source, token_t *token);
int PC_ReadDefineParms(source_t *source, define_t *define, token_t **parms, int maxparms);
int PC_StringizeTokens(token_t *tokens, token_t *token);
int PC_MergeTokens(token_t *t1, token_t *t2);
unsigned int PC_NameHash(const char *name);
void PC_AddDefineToHash(define_t *define, define_t **definehash);
define_t *PC_FindHashedDefine(define_t **definehash, const char *name);
define_t *PC_FindDefine(define_t *defines, const char *name);
int PC_FindDefineParm(define_t *define, const char *name);
void PC_FreeDefine(define_t *define);
int PC_ExpandBuiltinDefine(source_t *source, token_t *deftoken, define_t *define, token_t **firsttoken, token_t **lasttoken);
int PC_ExpandDefine(source_t *source, token_t *deftoken, define_t *define, token_t **firsttoken, token_t **lasttoken);
int PC_ExpandDefineIntoSource(source_t *source, token_t *deftoken, define_t *define);
void PC_ConvertPath(char *path);
int PC_ReadLine(source_t *source, token_t *token);
int PC_WhiteSpaceBeforeToken(token_t *token);
void PC_ClearTokenWhiteSpace(token_t *token);
int PC_Directive_undef(source_t *source);
int PC_Directive_define(source_t *source);
define_t *PC_DefineFromString(const char *string);
int PC_AddDefine(source_t *source, const char *string);
define_t *PC_CopyDefine(source_t *source, define_t *define);
void PC_AddGlobalDefinesToSource(source_t *source);
int PC_DirectiveIfdefCommon(source_t *source, int type);
int PC_Directive_ifdef(source_t *source);
int PC_OperatorPriority(int op);
int PC_EvaluateTokens(source_t *source, token_t *tokens, int *intvalue, double *floatvalue, int integer);
int PC_Evaluate(source_t *source, int *intvalue, double *floatvalue, int integer);
int PC_DollarEvaluate(source_t *source, int *intvalue, double *floatvalue, int integer);

#endif
