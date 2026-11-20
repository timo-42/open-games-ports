/*
 * Types, globals and out-of-chunk prototypes for chunk 030 (0x080b6004..0x080b8dde):
 * script/text parser sessions, byte-order helpers, Q_str* helpers, va(), info strings.
 */
#ifndef COD2_CHUNK_030_TYPES_H
#define COD2_CHUNK_030_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

/* One parser session; sessions are stacked in ComParseState (stride 0x45c, 16 deep). */
struct ParseSession {
    char token[0x400];            /* 0x000 current token buffer (returned by Com_ParseExt) */
    int lines;                    /* 0x400 current line number */
    unsigned char tokenUngotten;  /* 0x404 set by Com_UngetToken */
    unsigned char spaceDelimited; /* 0x405 token ends at first char <= ' ' */
    unsigned char keepStringQuotes; /* 0x406 keep enclosing '"' in quoted tokens */
    unsigned char csvMode;        /* 0x407 comma separated parsing (Com_ParseCSV) */
    unsigned char allowNegativeNumbers; /* 0x408 '-' followed by digit starts a number */
    unsigned char pad409[3];      /* 0x409 */
    const char *errorPrefix;      /* 0x40c first %s of the parse error message */
    const char *warningPrefix;    /* 0x410 first %s of the parse warning message */
    int savedLines;               /* 0x414 line count before the last token */
    const char *savedPos;         /* 0x418 read position before the last token */
    char filename[0x40];          /* 0x41c name shown in parse errors */
};
COD2_ASSERT_SIZE(ParseSession, 0x45c);
COD2_ASSERT_OFFSET(ParseSession, lines, 0x400);
COD2_ASSERT_OFFSET(ParseSession, tokenUngotten, 0x404);
COD2_ASSERT_OFFSET(ParseSession, spaceDelimited, 0x405);
COD2_ASSERT_OFFSET(ParseSession, keepStringQuotes, 0x406);
COD2_ASSERT_OFFSET(ParseSession, csvMode, 0x407);
COD2_ASSERT_OFFSET(ParseSession, allowNegativeNumbers, 0x408);
COD2_ASSERT_OFFSET(ParseSession, errorPrefix, 0x40c);
COD2_ASSERT_OFFSET(ParseSession, warningPrefix, 0x410);
COD2_ASSERT_OFFSET(ParseSession, savedLines, 0x414);
COD2_ASSERT_OFFSET(ParseSession, savedPos, 0x418);
COD2_ASSERT_OFFSET(ParseSession, filename, 0x41c);

struct ComParseState {
    ParseSession sessions[16];    /* 0x0000 */
    int sessionIndex;             /* 0x45c0 index of the active session */
    const char *lastTokenPos;     /* 0x45c4 start of the last parsed token */
    const char *prevTokenPos;     /* 0x45c8 start of the token before it */
};
COD2_ASSERT_OFFSET(ComParseState, sessionIndex, 0x45c0);
COD2_ASSERT_OFFSET(ComParseState, lastTokenPos, 0x45c4);
COD2_ASSERT_OFFSET(ComParseState, prevTokenPos, 0x45c8);

/* Saved parse position (Com_SaveParseOffset / Com_RestoreParseOffset), 20 bytes. */
struct ParseSave {
    int lines;
    const char *pos;
    int tokenUngotten;
    int savedLines;
    const char *savedPos;
};
COD2_ASSERT_SIZE(ParseSave, 0x14);

/* Field descriptor table entry for Com_ParseInfoFields. */
struct InfoField {
    const char *name;   /* info string key */
    int offset;         /* byte offset in the destination structure */
    int type;           /* 0 callback, 1..3 strings (0x400/0x40/0x100), 4 int, 5 bool, 6 float, 7 float*1000 */
};
COD2_ASSERT_SIZE(InfoField, 12);

#endif
