/*
 * chunk_029_types.h - types for cod2_lnxded 1.3 chunk 029 (0x080b27d2..0x080b5f32).
 *
 * Covers the dvar (dynamic variable) subsystem, the bounded message buffer
 * helpers, and the parse-session guard. Layouts are recovered from consistent
 * offsets in the disassembly; see the COD2_ASSERT_* lines.
 *
 * Placeholders: out-of-chunk callees are named fn_08xxxxxx and unnamed globals
 * g_08xxxxxx, as required by the worker brief. The merge lane rewrites them.
 */
#ifndef COD2_CHUNK_029_TYPES_H
#define COD2_CHUNK_029_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include <string.h>

/* ------------------------------------------------------------------------ */
/* dvar type tags (byte at dvar+0x06). Values are observed from the value   */
/* comparison switch in Dvar_ValuesEqual and the create/convert paths.      */
/* ------------------------------------------------------------------------ */
#define DVAR_TYPE_BOOL 0   /* uint8 value stored inline                     */
#define DVAR_TYPE_FLOAT 1  /* float bits stored inline                      */
#define DVAR_TYPE_VEC2 2   /* value/latched/reset are pointers to 2 floats  */
#define DVAR_TYPE_VEC3 3   /* pointers to 3 floats                          */
#define DVAR_TYPE_VEC4 4   /* pointers to 4 floats                          */
#define DVAR_TYPE_INT 5    /* int stored inline                             */
#define DVAR_TYPE_ENUM 6   /* int index; domain = (count, string list)      */
#define DVAR_TYPE_STRING 7 /* char pointer, owned by the dvar               */
#define DVAR_TYPE_COLOR 8  /* packed RGBA bytes stored inline (r = low byte) */

/* dvar flags (ushort at dvar+0x04). Bits 0x0010/0x0020/0x0040/0x0080 are    */
/* tied to observed messages; the rest are named by use.                     */
#define DVAR_FLAG_WRITE_PROTECT 0x0010  /* "%s is write protected."          */
#define DVAR_FLAG_LATCH 0x0020          /* "%s will be changed upon restarting." */
#define DVAR_FLAG_READ_ONLY 0x0040      /* "%s is read only."                */
#define DVAR_FLAG_CHEAT_PROTECT 0x0080  /* "%s is cheat protected."          */
#define DVAR_FLAG_UNKNOWN_0400 0x0400   /* cleared by Dvar_ClearUnknownFlag0400 */
#define DVAR_FLAG_UNKNOWN_1000 0x1000   /* gates enum domain update          */
#define DVAR_FLAG_UNKNOWN_2000 0x2000   /* part of the 0x7000 change mask    */
#define DVAR_FLAG_OWNS_NAME 0x4000      /* name (and string value) heap-owned */
#define DVAR_FLAG_UNKNOWN_8000 0x8000   /* set by Dvar_SetFromUserInput     */
#define DVAR_FLAG_CHANGE_MASK 0x7000    /* bits compared in Dvar_Update     */

/* Source of a set request (third argument of Dvar_SetVariant). 1 is the     */
/* console/user path with the cheat-protection check; 2 skips that check.   */
#define DVAR_SOURCE_INTERNAL 0
#define DVAR_SOURCE_USER 1
#define DVAR_SOURCE_OTHER 2

/* Same-named static helpers exist in several original translation units, so these chunk-029
 * functions carry their address suffix. cod2_prototypes.h dropped them while their names
 * collided; redeclared here (identical to chunk_029.json) so the chunk compiles either way. */
float Float_Max_080b56c6(float a, float b);
float Float_Min_080b56ea(float a, float b);
int Float_RoundToInt_080b570e(float v);
void Vec2_Clear_080b5744(float *v);
void Vec2_Set_080b5760(float *v, float x, float y);
void Vec2_Copy_080b5778(const float *src, float *dst);
void Vec3_Clear_080b57e6(float *v);
void Vec3_Set_080b5810(float *v, float x, float y, float z);
void Vec3_Copy_080b5834(const float *src, float *dst);
uint32 Vec3_Equal_080b5864(const float *a, const float *b);
void Vec4_Copy_080b5932(const float *src, float *dst);
float Float_SelectNonNegative_080b5ab8(float a, float b, float c);

/* Chunk 028 by-value functions; declared here until cod2_prototypes.h carries them. */
union DvarValue Dvar_ParseValueWord(uint8 type, union DvarLimits domain, const char *text);
union DvarValue Dvar_ClampValue(uint8 type, uint32 value, uint32 fallback, union DvarLimits limits);
union DvarValue Dvar_InitOut(struct dvar_s *d, union DvarValue value, uint8 type, uint16 flags, union DvarLimits domain);

#define DVAR_POOL_CAPACITY 0x500

/* Raw dvar words carry pointers as integers; these convert without a cast warning. */
#define DVAR_AS_PTR(w) ((void *)(size_t)(w))
#define DVAR_AS_STR(w) ((const char *)(size_t)(w))
#define DVAR_FROM_PTR(p) ((uint32)(size_t)(p))
#define DVAR_HASH_BUCKETS 0x100

/* Bounded message buffer (MSG_* style). Direction is identified by the      */
/* transfer pointer: MsgBuf_ReadData for readers, MsgBuf_WriteBytes for writers.   */
struct msgbuf_s;
typedef void (*msgbuf_transfer_fn)(struct msgbuf_s *buf, uint32 len, void *data);

struct msgbuf_s {
    uint8 *data;                 /* +0x00 */
    int32 maxsize;               /* +0x04 */
    int32 cursor;                /* +0x08 */
    uint8 warn_on_overflow;      /* +0x0c */
    uint8 overflowed;            /* +0x0d */
    msgbuf_transfer_fn transfer; /* +0x10 */
};
/* Total size is not observed (no array stride); only field offsets asserted. */
COD2_ASSERT_OFFSET(struct msgbuf_s, data, 0x00);
COD2_ASSERT_OFFSET(struct msgbuf_s, maxsize, 0x04);
COD2_ASSERT_OFFSET(struct msgbuf_s, cursor, 0x08);
COD2_ASSERT_OFFSET(struct msgbuf_s, warn_on_overflow, 0x0c);
COD2_ASSERT_OFFSET(struct msgbuf_s, overflowed, 0x0d);
COD2_ASSERT_OFFSET(struct msgbuf_s, transfer, 0x10);

/* Parse-session structs. The owner is chunk 030 (chunk_030_types.h); cod2_canon.h and
 * cod2_globals.h only forward-declare them, so the identical layout is repeated here. */
struct ParseSession {
    char token[0x400];
    int lines;                    /* 0x400 */
    unsigned char tokenUngotten;  /* 0x404 */
    unsigned char spaceDelimited; /* 0x405 */
    unsigned char keepStringQuotes; /* 0x406 */
    unsigned char csvMode;        /* 0x407 */
    unsigned char allowNegativeNumbers; /* 0x408 */
    unsigned char pad409[3];
    const char *errorPrefix;      /* 0x40c */
    const char *warningPrefix;    /* 0x410 */
    int savedLines;               /* 0x414 */
    const char *savedPos;         /* 0x418 */
    char filename[0x40];          /* 0x41c */
};
COD2_ASSERT_SIZE(ParseSession, 0x45c);
struct ComParseState {
    ParseSession sessions[16];
    int sessionIndex;             /* 0x45c0 */
    const char *lastTokenPos;     /* 0x45c4 */
    const char *prevTokenPos;     /* 0x45c8 */
};
COD2_ASSERT_OFFSET(ComParseState, sessionIndex, 0x45c0);
#define PARSE_SLOT_STRIDE 0x45c

/* ------------------------------------------------------------------------ */
/* Raw-word bit helpers. A dvar value word is 32 bits; float and pointer    */
/* values are carried as their bit patterns, as in the binary.              */
/* ------------------------------------------------------------------------ */
static inline float dvar_word_to_float(uint32 w)
{
    float f;
    memcpy(&f, &w, sizeof f);
    return f;
}

static inline uint32 dvar_float_to_word(float f)
{
    uint32 w;
    memcpy(&w, &f, sizeof w);
    return w;
}

#endif /* COD2_CHUNK_029_TYPES_H */
