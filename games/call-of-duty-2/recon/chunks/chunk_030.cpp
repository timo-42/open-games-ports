#include "cod2_types.h"
#include "chunk_030_types.h"

static inline ParseSession *CurrentSession(ComParseState *st)
{
    return &st->sessions[st->sessionIndex];
}

// 0x080b6004  size=57  Com_EndParseSession  status=translated  naming=observed
// string "Com_EndParseSession: session underflow" at 0x814f640 is raised here
void Com_EndParseSession(void)
{
    ComParseState *st = ParseSession_Get();
    if (st->sessionIndex == 0) {
        Com_Error(0, "\025Com_EndParseSession: session underflow");
    }
    st->sessionIndex = st->sessionIndex - 1;
}

// 0x080b603e  size=29  Com_ResetParseSessions  status=translated  naming=inferred
// counterpart of Com_EndParseSession; resets the session stack index to 0
void Com_ResetParseSessions(void)
{
    ComParseState *st = ParseSession_Get();
    st->sessionIndex = 0;
}

// 0x080b605c  size=53  Com_SetSpaceDelimited  status=translated  naming=inferred
// writes session byte 0x405 which Com_ParseExt tests to read until char <= ' '
void Com_SetSpaceDelimited(int enable)
{
    ComParseState *st = ParseSession_Get();
    CurrentSession(st)->spaceDelimited = (enable != 0);
}

// 0x080b6092  size=53  Com_SetKeepStringQuotes  status=translated  naming=inferred
// writes session byte 0x406 which makes Com_ParseExt keep the enclosing quotes of strings
void Com_SetKeepStringQuotes(int enable)
{
    ComParseState *st = ParseSession_Get();
    CurrentSession(st)->keepStringQuotes = (enable != 0);
}

// 0x080b60c8  size=53  Com_SetCSVMode  status=translated  naming=inferred
// writes session byte 0x407 which routes Com_ParseExt to Com_ParseCSV
void Com_SetCSVMode(int enable)
{
    ComParseState *st = ParseSession_Get();
    CurrentSession(st)->csvMode = (enable != 0);
}

// 0x080b60fe  size=53  Com_SetParseNegativeNumbers  status=translated  naming=inferred
// writes session byte 0x408 which lets Com_ParseExt read "-5" as one number token
void Com_SetParseNegativeNumbers(int enable)
{
    ComParseState *st = ParseSession_Get();
    CurrentSession(st)->allowNegativeNumbers = (enable != 0);
}

// 0x080b6134  size=46  Com_GetParseLine  status=translated  naming=inferred
// returns the session line counter that Com_ParseError prints as "line %i"
int Com_GetParseLine(void)
{
    ComParseState *st = ParseSession_Get();
    return CurrentSession(st)->lines;
}

// 0x080b6162  size=49  Com_SetParseErrorPrefix  status=translated  naming=inferred
// stores the pointer printed as first %s by Com_ParseError
void Com_SetParseErrorPrefix(const char *prefix)
{
    ComParseState *st = ParseSession_Get();
    CurrentSession(st)->errorPrefix = prefix;
}

// 0x080b6194  size=49  Com_SetParseWarningPrefix  status=translated  naming=inferred
// stores the pointer printed as first %s by Com_ParseWarning
void Com_SetParseWarningPrefix(const char *prefix)
{
    ComParseState *st = ParseSession_Get();
    CurrentSession(st)->warningPrefix = prefix;
}

// 0x080b61c6  size=218  Com_ParseError  status=translated  naming=inferred
// formats the message then raises an error with "File %s, line %i: %s" (error code 1); counterpart of Com_ParseWarning
void Com_ParseError(const char *fmt, ...)
{
    char buffer[0x1000];
    ComParseState *st = ParseSession_Get();
    ParseSession *session = CurrentSession(st);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 0x1000, fmt, ap);
    va_end(ap);
    if (st->sessionIndex == 0) {
        Com_Error(1, "\025%s", buffer);
    } else {
        Com_Error(1, "\025%sFile %s, line %i: %s", session->errorPrefix, session->filename,
                    session->lines, buffer);
    }
}

// 0x080b62a0  size=202  Com_ParseWarning  status=translated  naming=inferred
// same as Com_ParseError but prints through the console print function instead of raising an error
void Com_ParseWarning(const char *fmt, ...)
{
    char buffer[0x1000];
    ComParseState *st = ParseSession_Get();
    ParseSession *session = CurrentSession(st);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 0x1000, fmt, ap);
    va_end(ap);
    if (st->sessionIndex == 0) {
        Com_Printf("%s", buffer);
    } else {
        Com_Printf("%sFile %s, line %i: %s", session->warningPrefix, session->filename,
                    session->lines, buffer);
    }
}

// 0x080b636a  size=89  Com_UngetToken  status=translated  naming=observed
// string "UngetToken called twice" (Q3 Com_UngetToken counterpart)
void Com_UngetToken(void)
{
    ComParseState *st = ParseSession_Get();
    ParseSession *session = CurrentSession(st);
    if (session->tokenUngotten != 0) {
        Com_ParseError("UngetToken called twice");
    }
    session->tokenUngotten = 1;
    st->lastTokenPos = st->prevTokenPos;
}

// 0x080b63c4  size=108  Com_SaveParseOffset  status=translated  naming=inferred
// snapshot of parser position, restored by Com_RestoreParseOffset (used by Com_CountTokensOnLine)
void Com_SaveParseOffset(const char **data_p, ParseSave *save)
{
    ComParseState *st = ParseSession_Get();
    ParseSession *session = CurrentSession(st);
    save->lines = session->lines;
    save->pos = *data_p;
    save->tokenUngotten = session->tokenUngotten;
    save->savedLines = session->savedLines;
    save->savedPos = (const char *)session->savedPos;
}

// 0x080b6430  size=111  Com_RestoreParseOffset  status=translated  naming=inferred
// restores what Com_SaveParseOffset stored
void Com_RestoreParseOffset(const char **data_p, const ParseSave *save)
{
    ComParseState *st = ParseSession_Get();
    ParseSession *session = CurrentSession(st);
    session->lines = save->lines;
    *data_p = save->pos;
    session->tokenUngotten = (save->tokenUngotten != 0);
    session->savedLines = save->savedLines;
    session->savedPos = save->savedPos;
}

// 0x080b64a0  size=109  Com_SkipWhitespace  status=translated  naming=inferred
// Q3 SkipWhitespace counterpart: skips chars <= ' ', counts '\n' in the session line number and flags *hasNewLines; NULL at end of text
const char *Com_SkipWhitespace(const char *data, int *hasNewLines)
{
    ComParseState *st = ParseSession_Get();
    int sessionIndex = st->sessionIndex;
    for (;;) {
        char c = *data;
        if (' ' < c) {
            return data;
        }
        if (c == '\0') {
            break;
        }
        if (c == '\n') {
            st->sessions[sessionIndex].lines = st->sessions[sessionIndex].lines + 1;
            *hasNewLines = 1;
        }
        data = data + 1;
    }
    return (const char *)0;
}

// 0x080b650e  size=316  Com_Compress  status=translated  naming=inferred
// in-place removal of // and /* */ comments (keeps newlines), returns the new length; Q3 COM_Compress counterpart
int Com_Compress(char *data)
{
    int length = 0;
    char *out = data;
    char *in = data;
    if (data != (char *)0) {
        while (*in != '\0') {
            char c = *in;
            if (c == '\r' || c == '\n') {
                *out = c;
                out = out + 1;
                length = length + 1;
                in = in + 1;
            } else if (c == '/' && in[1] == '/') {
                for (; *in != '\0' && *in != '\n'; in = in + 1) {
                }
            } else if (c == '/' && in[1] == '*') {
                for (; *in != '\0' && (*in != '*' || in[1] != '/'); in = in + 1) {
                    if (*in == '\n') {
                        *out = '\n';
                        out = out + 1;
                        length = length + 1;
                    }
                }
                if (*in != '\0') {
                    in = in + 2;
                }
            } else {
                *out = c;
                out = out + 1;
                length = length + 1;
                in = in + 1;
            }
        }
    }
    *out = '\0';
    return length;
}

// 0x080b664a  size=25  Com_GetLastTokenPos  status=translated  naming=inferred
// returns the position at which the most recent token started
const char *Com_GetLastTokenPos(void)
{
    ComParseState *st = ParseSession_Get();
    return st->lastTokenPos;
}

// 0x080b6664  size=403  Com_ParseCSV  status=translated  naming=inferred
// CSV field parser used by Com_ParseExt when csvMode is set: reads up to ',' or '\n', handles "" quoting
char *Com_ParseCSV(const char **data_p, int allowLineBreaks)
{
    ComParseState *st = ParseSession_Get();
    char *token = (char *)CurrentSession(st);
    const char *p = *data_p;
    uint32 len = 0;
    token[0] = 0;
    if (allowLineBreaks == 0) {
        if (*p == '\r') {
            return token;
        }
        if (*p == '\n') {
            return token;
        }
    } else {
        for (; *p == '\r' || *p == '\n'; p = p + 1) {
        }
    }
    st->prevTokenPos = st->lastTokenPos;
    st->lastTokenPos = p;
    while (*p != '\0' && *p != ',' && *p != '\n') {
        if (*p == '\r') {
            p = p + 1;
        } else if (*p == '"') {
            p = p + 1;
            for (;;) {
                for (; *p != '"'; p = p + 1) {
                    if (len < 0x3ff) {
                        token[len] = *p;
                        len = len + 1;
                    }
                }
                if (p[1] != '"') {
                    break;
                }
                if (len < 0x3ff) {
                    token[len] = '"';
                    len = len + 1;
                }
                p = p + 2;
            }
            p = p + 1;
        } else {
            if (len < 0x3ff) {
                token[len] = *p;
                len = len + 1;
            }
            p = p + 1;
        }
    }
    if (*p == '\0') {
        *data_p = (const char *)0;
    } else {
        if (*p != '\n') {
            p = p + 1;
        }
        *data_p = p;
    }
    token[len] = 0;
    return token;
}

// 0x080b67f8  size=1466  Com_ParseExt  status=translated  naming=inferred
// Q3 COM_ParseExt counterpart: returns the session token buffer; supports //, /* */ comments, quoted strings,
// numbers (with exponent), identifiers and the multi-char punctuation table com_parsePunctuation
char *Com_ParseExt(const char **data_p, int allowLineBreaks)
{
    char *token;
    const char *p;
    int len;
    int hasNewLines;
    char c;
    ComParseState *st;
    ParseSession *session;

    c = '\0';
    hasNewLines = 0;
    st = ParseSession_Get();
    session = CurrentSession(st);
    token = session->token;
    p = *data_p;
    len = 0;
    token[0] = '\0';
    if (p == (const char *)0) {
        *data_p = (const char *)0;
        return token;
    }
    session->savedLines = session->lines;
    session->savedPos = *data_p;
    if (session->csvMode != 0) {
        return Com_ParseCSV(data_p, allowLineBreaks);
    }
    while ((p = Com_SkipWhitespace(p, &hasNewLines)) != (const char *)0) {
        if (hasNewLines != 0 && allowLineBreaks == 0) {
            return token;
        }
        c = *p;
        if (c == '/' && p[1] == '/') {
            for (; *p != '\0' && *p != '\n'; p = p + 1) {
            }
        } else {
            if (c != '/' || p[1] != '*') {
                st->prevTokenPos = st->lastTokenPos;
                st->lastTokenPos = p;
                if (c == '"') {
                    if (session->keepStringQuotes != 0) {
                        token[len] = '"';
                        len = len + 1;
                    }
                    p = p + 1;
                    for (;;) {
                        const char *next = p + 1;
                        c = *p;
                        if (c == '\\' && (*next == '"' || *next == '\\')) {
                            p = p + 2;
                            c = *next;
                        } else {
                            if (c == '"' || c == '\0') {
                                if (session->keepStringQuotes != 0) {
                                    token[len] = '"';
                                    len = len + 1;
                                }
                                token[len] = '\0';
                                *data_p = next;
                                return token;
                            }
                            p = next;
                            if (*next == '\n') {
                                session->lines = session->lines + 1;
                            }
                        }
                        if (len < 0x3ff) {
                            token[len] = c;
                            len = len + 1;
                        }
                    }
                }
                if (session->spaceDelimited != 0) {
                    do {
                        if (len < 0x3ff) {
                            token[len] = c;
                            len = len + 1;
                        }
                        p = p + 1;
                        c = *p;
                    } while (' ' < c);
                    if (len == 0x400) {
                        len = 0;
                    }
                    token[len] = '\0';
                    *data_p = p;
                    return token;
                }
                {
                    const char *scan = p;
                    if ((('/' < c && c < ':') ||
                         (session->allowNegativeNumbers != 0 && c == '-' && '/' < p[1] && p[1] < ':')) ||
                        (c == '.' && '/' < p[1] && p[1] < ':')) {
                        do {
                            p = scan;
                            if (len < 0x3ff) {
                                token[len] = c;
                                len = len + 1;
                            }
                            scan = p + 1;
                            c = *scan;
                        } while (('/' < c && c < ':') || c == '.');
                        if (c == 'e' || c == 'E') {
                            if (len < 0x3ff) {
                                token[len] = c;
                                len = len + 1;
                            }
                            c = p[2];
                            if (c == '-' || (scan = p + 2, c == '+')) {
                                if (len < 0x3ff) {
                                    token[len] = c;
                                    len = len + 1;
                                }
                                c = p[3];
                                scan = p + 3;
                            }
                            do {
                                p = scan;
                                if (len < 0x3ff) {
                                    token[len] = c;
                                    len = len + 1;
                                }
                                scan = p + 1;
                                c = *scan;
                            } while ('/' < c && c < ':');
                        }
                        p = scan;
                        if (len == 0x400) {
                            len = 0;
                        }
                        token[len] = '\0';
                        *data_p = p;
                        return token;
                    }
                }
                if (((c < 'a' || 'z' < c) && (c < 'A' || 'Z' < c)) &&
                    ((c != '_' && c != '/') && c != '\\')) {
                    const char **punct = com_parsePunctuation;
                    for (;;) {
                        int n;
                        int i;
                        if (*punct == (const char *)0) {
                            token[0] = *p;
                            token[1] = '\0';
                            *data_p = p + 1;
                            return token;
                        }
                        n = ScrStr_Strlen(*punct);
                        for (i = 0; i < n && p[i] == (*punct)[i]; i = i + 1) {
                        }
                        if (i == n) {
                            break;
                        }
                        punct = punct + 1;
                    }
                    {
                        size_t n = (size_t)ScrStr_Strlen(*punct);
                        memcpy(token, *punct, n);
                        token[n] = '\0';
                        *data_p = p + n;
                        return token;
                    }
                }
                do {
                    if (len < 0x3ff) {
                        token[len] = c;
                        len = len + 1;
                    }
                    p = p + 1;
                    c = *p;
                } while ((('`' < c && c < '{') || (('@' < c && c < '[') || c == '_')) ||
                         ('/' < c && c < ':'));
                if (len == 0x400) {
                    len = 0;
                }
                token[len] = '\0';
                *data_p = p;
                return token;
            }
            for (; *p != '\0' && (*p != '*' || p[1] != '/'); p = p + 1) {
                if (*p == '\n') {
                    session->lines = session->lines + 1;
                }
            }
            if (*p != '\0') {
                p = p + 2;
            }
        }
    }
    *data_p = (const char *)0;
    return token;
}

// 0x080b6db2  size=110  Com_Parse  status=translated  naming=inferred
// Q3 COM_Parse counterpart: honours a pending UngetToken, then Com_ParseExt(data_p, 1)
char *Com_Parse(const char **data_p)
{
    ComParseState *st = ParseSession_Get();
    ParseSession *session = CurrentSession(st);
    if (session->tokenUngotten != 0) {
        session->tokenUngotten = 0;
        *data_p = session->savedPos;
        session->lines = session->savedLines;
    }
    return Com_ParseExt(data_p, 1);
}

// 0x080b6e20  size=136  Com_ParseOnLine  status=translated  naming=inferred
// Q3 COM_ParseOnLine counterpart: Com_ParseExt(data_p, 0); with an ungot token and no space-delimited mode the old token is returned as is
char *Com_ParseOnLine(const char **data_p)
{
    ComParseState *st = ParseSession_Get();
    ParseSession *session = CurrentSession(st);
    if (session->tokenUngotten != 0) {
        session->tokenUngotten = 0;
        if (session->spaceDelimited == 0) {
            return (char *)session;
        }
        *data_p = session->savedPos;
        session->lines = session->savedLines;
    }
    return Com_ParseExt(data_p, 0);
}

// 0x080b6ea8  size=123  Com_MatchToken  status=translated  naming=observed
// string "MatchToken: %s != %s\n"; parameter 3 selects Com_ParseError (0) or Com_ParseWarning (non-zero)
int Com_MatchToken(const char **data_p, const char *match, int warningOnly)
{
    const char *token = Com_Parse(data_p);
    int result;
    if (strcmp(token, match) == 0) {
        result = 1;
    } else {
        if (warningOnly == 0) {
            Com_ParseError("MatchToken: %s != %s\n", token, match);
        } else {
            Com_ParseWarning("MatchToken: %s != %s\n", token, match);
        }
        result = 0;
    }
    return result;
}

// 0x080b6f90  size=100  Com_SkipRestOfLine  status=translated  naming=inferred
// advances to just past the next '\n' (counting the line) or to end of text
void Com_SkipRestOfLine(const char **data_p)
{
    ComParseState *st = ParseSession_Get();
    const char *p = *data_p;
    if (p != (const char *)0) {
        char c;
        do {
            c = *p;
            if (c == '\0') {
                goto done;
            }
            p = p + 1;
        } while (c != '\n');
        st->sessions[st->sessionIndex].lines = st->sessions[st->sessionIndex].lines + 1;
done:
        *data_p = p;
    }
}

// 0x080b6ff4  size=85  Com_CountTokensOnLine  status=translated  naming=inferred
// counts the tokens remaining on the current line, then restores the parser position
int Com_CountTokensOnLine(const char **data_p)
{
    int saved[10];
    int count;
    const char *token;
    Com_SaveParseOffset(data_p, (ParseSave *)saved);
    count = 0;
    for (;;) {
        token = Com_ParseOnLine(data_p);
        if (*token == '\0') {
            break;
        }
        count = count + 1;
    }
    Com_RestoreParseOffset(data_p, (const ParseSave *)saved);
    return count;
}

// 0x080b7166  size=134  Com_Parse1DMatrix  status=translated  naming=inferred
// Q3 Parse1DMatrix counterpart: "(" n floats ")"
void Com_Parse1DMatrix(const char **data_p, int x, float *m)
{
    int i;
    Com_MatchToken(data_p, "(", 0);
    for (i = 0; i < x; i = i + 1) {
        const char *token = Com_Parse(data_p);
        double value = atof(token);
        m[i] = (float)value;
    }
    Com_MatchToken(data_p, ")", 0);
}

// 0x080b71ec  size=121  Com_Parse2DMatrix  status=translated  naming=inferred
// Q3 Parse2DMatrix counterpart: "(" y rows of Com_Parse1DMatrix ")"
void Com_Parse2DMatrix(const char **data_p, int y, int x, float *m)
{
    int i;
    Com_MatchToken(data_p, "(", 0);
    for (i = 0; i < y; i = i + 1) {
        Com_Parse1DMatrix(data_p, x, m + i * x);
    }
    Com_MatchToken(data_p, ")", 0);
}

// 0x080b72ec  size=48  ColorIndex  status=translated  naming=unknown
// returns c-'0' for '0'..'9' and 7 for any other character (looks like a colour index lookup); no string evidence
int ColorIndex(char c)
{
    byte digit = (byte)(c - 0x30);
    int result;
    if (digit <= 9) {
        result = (int)digit;
    } else {
        result = 7;
    }
    return result;
}

// 0x080b731c  size=47  COM_SkipPath  status=translated  naming=inferred
// Q3 COM_SkipPath: pointer after the last '/'
char *COM_SkipPath(char *pathname)
{
    char *last = pathname;
    for (; *pathname != '\0'; pathname = pathname + 1) {
        if (*pathname == '/') {
            last = pathname + 1;
        }
    }
    return last;
}

// 0x080b741a  size=86  COM_GetExtension  status=translated  naming=inferred
// extension search: last '.' not followed by a '/' or '\\'; returns end of string if none
char *COM_GetExtension(char *name)
{
    char *dot = (char *)0;
    for (; *name != '\0'; name = name + 1) {
        if (*name == '.') {
            dot = name;
        } else if (*name == '/' || *name == '\\') {
            dot = (char *)0;
        }
    }
    if (dot == (char *)0) {
        dot = name;
    }
    return dot;
}

// 0x080b7470  size=61  COM_StripExtension  status=translated  naming=inferred
// Q3 COM_StripExtension counterpart: copies in up to the extension into out
void COM_StripExtension(const char *in, char *out)
{
    const char *ext = COM_GetExtension((char *)in);
    while (in != ext) {
        *out = *in;
        out = out + 1;
        in = in + 1;
    }
    *out = 0;
}

// 0x080b74ae  size=67  COM_ExtractFilePath  status=translated  naming=inferred
// copies the directory part (including the final '/') of path to dest
void COM_ExtractFilePath(const char *path, char *dest)
{
    const char *base = COM_SkipPath((char *)path);
    memcpy(dest, path, (size_t)(base - path));
    dest[base - path] = 0;
}

// 0x080b74f2  size=127  COM_DefaultExtension  status=translated  naming=inferred
// Q3 COM_DefaultExtension: appends extension when the last path component has no '.'
void COM_DefaultExtension(char *path, int maxSize, const char *extension)
{
    char oldPath[72];
    char *src = path + strlen(path);
    while (src = src - 1, *src != '/' && src != path) {
        if (*src == '.') {
            return;
        }
    }
    Q_strncpyz(oldPath, path, 0x40);
    Com_sprintf(path, maxSize, "%s%s", oldPath, extension);
}

// 0x080b7572  size=30  BigShort  status=translated  naming=inferred
// dispatches through the byte-order slot pfnBigShort (ShortSwap on little-endian hosts)
int BigShort(short l)
{
    short result = (*pfnBigShort)((int)l);
    return (int)result;
}

// 0x080b7590  size=21  BigLong  status=translated  naming=inferred
// dispatches through swap_bigLongFn (LongSwap on little-endian hosts)
int BigLong(int l)
{
    return (*swap_bigLongFn)(l);
}

// 0x080b75a6  size=40  BigLong64  status=translated  naming=inferred
// dispatches through g_085bc810 (64-bit swap on big-endian hosts only)
uint64 BigLong64(uint64 l)
{
    return (*g_085bc810)(l);
}

// 0x080b75ce  size=30  LittleShort_ViaSlot  status=translated  naming=inferred
// dispatches through pfnLittleShort (no-op on little-endian hosts)
int LittleShort_ViaSlot(short l)
{
    short result = (*pfnLittleShort)((int)l);
    return (int)result;
}

// 0x080b75ec  size=21  LittleLong_ViaSlot  status=translated  naming=inferred
// dispatches through pfnLittleLong (no-op on little-endian hosts)
int LittleLong_ViaSlot(int l)
{
    return (*pfnLittleLong)(l);
}

// 0x080b762e  size=55  ShortSwap  status=translated  naming=inferred
// swaps the two bytes of a 16-bit value, result sign-extended (cwde)
int ShortSwap(int value)
{
    short s = (short)value;
    unsigned char b0 = (unsigned char)(s & 0xff);
    unsigned char b1 = (unsigned char)((s >> 8) & 0xff);
    short r = (short)((b0 << 8) + b1);
    return (int)r;
}

// 0x080b7666  size=19  ShortNoSwap  status=translated  naming=inferred
// returns the 16-bit value sign-extended
int ShortNoSwap(int value)
{
    short s = (short)value;
    return (int)s;
}

// 0x080b767a  size=92  LongSwap  status=translated  naming=inferred
// reverses the four bytes of a 32-bit value
int LongSwap(uint32 value)
{
    return (int)(value * 0x1000000 + (value >> 8 & 0xff) * 0x10000 + (value >> 0x10 & 0xff) * 0x100 +
                 (value >> 0x18));
}

// 0x080b76d6  size=8  LongNoSwap  status=translated  naming=inferred
// identity
int LongNoSwap(int value)
{
    return value;
}

// 0x080b76de  size=315  Long64Swap  status=translated  naming=inferred
// reverses all eight bytes of a 64-bit value (low word and high word each byte swapped, then exchanged)
uint64 Long64Swap(uint64 value)
{
    uint32 lo = (uint32)value;
    uint32 hi = (uint32)(value >> 32);
    uint32 swappedHi = (uint32)LongSwap(hi);
    uint32 swappedLo = (uint32)LongSwap(lo);
    return ((uint64)swappedLo << 32) | (uint64)swappedHi;
}

// 0x080b781a  size=26  Long64NoSwap  status=translated  naming=inferred
// identity on 64-bit value
uint64 Long64NoSwap(uint64 value)
{
    return value;
}

// 0x080b7834  size=51  FloatSwap  status=translated  naming=inferred
// reverses the bytes of a 32-bit pattern and returns it as float (x87 fld)
float FloatSwap(uint32 bits)
{
    union {
        uint32 u;
        float f;
        unsigned char b[4];
    } in, out;
    in.u = bits;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];
    return out.f;
}

// 0x080b7868  size=19  FloatNoSwap  status=translated  naming=inferred
// forwards the bit pattern to Float_Identity_080b933a (no-swap float helper)
float FloatNoSwap(uint32 bits)
{
    /* the original moves the raw dword into the callee's float argument slot (no int->float conversion) */
    union {
        uint32 u;
        float f;
    } pun;

    pun.u = bits;
    return Float_Identity_080b933a(pun.f);
}

// 0x080b787c  size=45  FloatBitsSwap  status=translated  naming=inferred
// reverses the four bytes of a 32-bit pattern, returned as integer bits
int FloatBitsSwap(uint32 bits)
{
    union {
        uint32 u;
        unsigned char b[4];
    } in, out;
    in.u = bits;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];
    return (int)out.u;
}

// 0x080b78aa  size=19  FloatBitsNoSwap  status=translated  naming=inferred
// passes the address of its own argument to Read_FirstDword and forwards the result
int FloatBitsNoSwap(uint32 bits)
{
    return Read_FirstDword(&bits);
}

// 0x080b78be  size=75  Swap_InitLittleEndian  status=translated  naming=inferred
// installs the swapping (BigShort/BigLong = swap) function slots used on little-endian hosts (Q3 Swap_Init little-endian branch)
void Swap_InitLittleEndian(void)
{
    pfnBigShort = (short (*)(int))ShortSwap;
    pfnLittleShort = (short (*)(int))ShortNoSwap;
    swap_bigLongFn = (int (*)(int))LongSwap;
    pfnLittleLong = LongNoSwap;
    g_085bc810 = Long64NoSwap;
    g_085bc814 = FloatNoSwap;
    g_085bc818 = FloatBitsNoSwap;
}

// 0x080b790a  size=75  Swap_InitBigEndian  status=translated  naming=inferred
// opposite slot assignment (big-endian host branch of Swap_Init)
void Swap_InitBigEndian(void)
{
    pfnBigShort = (short (*)(int))ShortNoSwap;
    pfnLittleShort = (short (*)(int))ShortSwap;
    swap_bigLongFn = LongNoSwap;
    pfnLittleLong = (int (*)(int))LongSwap;
    g_085bc810 = Long64Swap;
    g_085bc814 = FloatSwap;
    g_085bc818 = FloatBitsSwap;
}

// 0x080b7956  size=35  Swap_Init  status=translated  naming=inferred
// Swap_Init: the endianness test was folded away by the compiler (only the little-endian branch survives)
void Swap_Init(void)
{
    Swap_InitLittleEndian();
}

// 0x080b797a  size=37  Q_isprint  status=translated  naming=inferred
// Q_isprint: 0x20..0x7e
int Q_isprint(int c)
{
    int result = 0;
    if (0x1f < c && c < 0x7f) {
        result = 1;
    }
    return result;
}

// 0x080b79a0  size=37  Q_islower  status=translated  naming=inferred
// Q_islower: 'a'..'z'
int Q_islower(int c)
{
    int result = 0;
    if (0x60 < c && c < 0x7b) {
        result = 1;
    }
    return result;
}

// 0x080b79c6  size=37  Q_isupper  status=translated  naming=inferred
// Q_isupper: 'A'..'Z'
int Q_isupper(int c)
{
    int result = 0;
    if (0x40 < c && c < 0x5b) {
        result = 1;
    }
    return result;
}

// 0x080b79ec  size=51  Q_isalpha  status=translated  naming=inferred
// Q_isalpha: letters
int Q_isalpha(int c)
{
    int result = 0;
    if ((0x60 < c && c < 0x7b) || (0x40 < c && c < 0x5b)) {
        result = 1;
    }
    return result;
}

// 0x080b7a20  size=37  Q_isnumeric  status=translated  naming=inferred
// Q_isnumeric: '0'..'9'
int Q_isnumeric(int c)
{
    int result = 0;
    if (0x2f < c && c < 0x3a) {
        result = 1;
    }
    return result;
}

// 0x080b7a46  size=57  Q_isalphanumeric  status=translated  naming=inferred
// Q_isalphanumeric
int Q_isalphanumeric(int c)
{
    int result = 0;
    char r = (char)Q_isalpha(c);
    if (r != '\0' || (r = (char)Q_isnumeric(c), r != '\0')) {
        result = 1;
    }
    return result;
}

// 0x080b7a80  size=54  Q_isidentifierchar  status=translated  naming=unknown
// alphanumeric, '_' or '-'; callers not in this chunk, so only the character class is known
int Q_isidentifierchar(int c)
{
    int result = 0;
    char r = (char)Q_isalphanumeric(c);
    if ((r != '\0' || c == 0x5f) || c == 0x2d) {
        result = 1;
    }
    return result;
}

// 0x080b7ab6  size=44  Q_strncpyz  status=translated  naming=inferred
// Q_strncpyz: strncpy(dest, src, destsize-1) then NUL terminator
void Q_strncpyz(char *dest, const char *src, int destsize)
{
    strncpy(dest, src, destsize - 1);
    dest[destsize - 1] = '\0';
}

// 0x080b7ae2  size=165  Q_stricmpn  status=translated  naming=inferred
// Q_stricmpn: case-insensitive compare of at most n characters (upper-casing via Q_islower)
int Q_stricmpn(const char *s1, const char *s2, int n)
{
    int c1;
    int c2;
    for (;;) {
        c1 = (int)*s1;
        s1 = s1 + 1;
        c2 = (int)*s2;
        s2 = s2 + 1;
        n = n - 1;
        if (n == -1) {
            return 0;
        }
        if (c1 != c2) {
            char r = (char)Q_islower(c1);
            if (r != '\0') {
                c1 = c1 - 0x20;
            }
            r = (char)Q_islower(c2);
            if (r != '\0') {
                c2 = c2 - 0x20;
            }
            if (c1 != c2) {
                int result;
                if (c1 < c2) {
                    result = -1;
                } else {
                    result = 1;
                }
                return result;
            }
        }
        if (c1 == 0) {
            return 0;
        }
    }
}

// 0x080b7b88  size=111  Q_strncmp  status=translated  naming=inferred
// Q_strncmp: case-sensitive compare of at most n characters (signed char compare)
int Q_strncmp(const char *s1, const char *s2, int n)
{
    char c1;
    char c2;
    for (;;) {
        c1 = *s1;
        s1 = s1 + 1;
        c2 = *s2;
        s2 = s2 + 1;
        n = n - 1;
        if (n == -1) {
            return 0;
        }
        if (c1 != c2) {
            break;
        }
        if (c1 == '\0') {
            return 0;
        }
    }
    if (c1 < c2) {
        return -1;
    }
    return 1;
}

// 0x080b7bf8  size=34  Q_stricmp  status=translated  naming=inferred
// Q_stricmp: Q_stricmpn with unlimited length (the result stays in eax)
int Q_stricmp(const char *s1, const char *s2)
{
    return Q_stricmpn(s1, s2, 0x7fffffff);
}

// 0x080b7c1a  size=34  Q_strcmp  status=translated  naming=inferred
// Q_strcmp: Q_strncmp with unlimited length
int Q_strcmp(const char *s1, const char *s2)
{
    return Q_strncmp(s1, s2, 0x7fffffff);
}

// 0x080b7c3c  size=215  Q_WildcardCompare  status=translated  naming=inferred
// wildcard compare: '*' matches any run (recursive), '?' any single char, otherwise tolower compare; 0 means match
int Q_WildcardCompare(const char *pattern, const char *text)
{
    char c1;
    char c2;
    int l1;
    int l2;
    for (;;) {
        c1 = *pattern;
        if (c1 == '*') {
            if (pattern[1] == '\0') {
                return 0;
            }
            if (*text != '\0' && Q_WildcardCompare(pattern, text + 1) == 0) {
                return 0;
            }
        } else {
            c2 = *text;
            text = text + 1;
            if (c1 != c2 && c1 != '?') {
                l1 = __builtin_tolower((int)c1);
                l2 = __builtin_tolower((int)c2);
                if (l1 != l2) {
                    int result;
                    if (l1 - l2 < 0) {
                        result = -1;
                    } else {
                        result = 1;
                    }
                    return result;
                }
            }
        }
        pattern = pattern + 1;
        if (c1 == '\0') {
            return 0;
        }
    }
}

// 0x080b7d14  size=66  Q_strlwr  status=translated  naming=inferred
// Q_strlwr: lower-cases in place via Q_isupper, returns s
char *Q_strlwr(char *s1)
{
    char *s;
    for (s = s1; *s != '\0'; s = s + 1) {
        char r = (char)Q_isupper((int)*s);
        if (r != '\0') {
            *s = *s + ' ';
        }
    }
    return s1;
}

// 0x080b7d56  size=66  Q_strupr  status=translated  naming=inferred
// Q_strupr: upper-cases in place via Q_islower, returns s
char *Q_strupr(char *s1)
{
    char *s;
    for (s = s1; *s != '\0'; s = s + 1) {
        char r = (char)Q_islower((int)*s);
        if (r != '\0') {
            *s = *s + -0x20;
        }
    }
    return s1;
}

// 0x080b7d98  size=83  I_strncat  status=translated  naming=observed
// string "I_strncat: already overflowed" (error code 0 when strlen(dest) >= size)
void I_strncat(char *dest, int size, const char *src)
{
    int len = ScrStr_Strlen(dest);
    if (size <= len) {
        Com_Error(0, "\025I_strncat: already overflowed");
    }
    Q_strncpyz(dest + len, src, size - len);
}

// 0x080b7dec  size=104  Q_PrintStrlen  status=translated  naming=inferred
// Q_PrintStrlen: string length ignoring ^0..^9 colour codes ("^^" is not a code)
int Q_PrintStrlen(const char *string)
{
    int len = 0;
    const char *p = string;
    while (*p != '\0') {
        if (((p == (const char *)0 || *p != '^') || p[1] == '\0') ||
            ((p[1] == '^' || p[1] < '0') || '9' < p[1])) {
            len = len + 1;
            p = p + 1;
        } else {
            p = p + 2;
        }
    }
    return len;
}

// 0x080b7e54  size=136  Q_CleanStr  status=translated  naming=inferred
// Q_CleanStr: strips colour codes and control characters (<0x20, 0x7f) in place
char *Q_CleanStr(char *string)
{
    char *d = string;
    char *s;
    char c;
    for (s = string; c = *s, c != '\0'; s = s + 1) {
        if ((((s == (char *)0 || *s != '^') || s[1] == '\0') || (s[1] == '^' || s[1] < '0')) ||
            '9' < s[1]) {
            if ('\x1f' < c && c != '\x7f') {
                *d = c;
                d = d + 1;
            }
        } else {
            s = s + 1;
        }
    }
    *d = '\0';
    return string;
}

// 0x080b7edc  size=46  Q_NormalizeApostrophe  status=translated  naming=unknown
// maps byte 0x92 (typographic apostrophe) to 0x27 ('); other chars returned as sign-extended char
int Q_NormalizeApostrophe(char c)
{
    int result;
    if (c == -0x6e) {
        result = 0x27;
    } else {
        result = (int)c;
    }
    return result;
}

// 0x080b7f0a  size=62  Com_sprintf  status=translated  naming=inferred
// Com_sprintf: vsnprintf then forced NUL at dest[size-1]; returns the vsnprintf result
int Com_sprintf(char *dest, size_t size, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsnprintf(dest, size, fmt, ap);
    va_end(ap);
    dest[size - 1] = '\0';
    return ret;
}

// 0x080b7f48  size=93  Com_IsPersistentPointer  status=translated  naming=unknown
// returns 0 when the pointer is within 0x2000 bytes above this frame's local (a caller's stack buffer) or inside the 0xc00-byte va() area, otherwise 1
int Com_IsPersistentPointer(const char *ptr)
{
    char probe[4];
    char *vaBuffer;
    int result;
    if (ptr < probe || (probe + 0x2000) <= ptr) {
        vaBuffer = (char *)Sys_GetValue(1);
        if (ptr < vaBuffer || vaBuffer + 0xc00 <= ptr) {
            result = 1;
        } else {
            result = 0;
        }
    } else {
        result = 0;
    }
    return result;
}

// 0x080b7fa6  size=184  va  status=translated  naming=observed
// string "Attempted to overrun string in call to va()"; two rotating 0x400 buffers in per-thread block slot 1
char *va(const char *fmt, ...)
{
    char *block;
    char *buf;
    int next;
    int ret;
    va_list ap;
    va_start(ap, fmt);
    block = (char *)Sys_GetValue(1);
    buf = block + *(int *)(block + 0x800) * 0x400;
    next = *(int *)(block + 0x800) + 1;
    *(int *)(block + 0x800) = next / 2;
    *(int *)(block + 0x800) = next + *(int *)(block + 0x800) * -2;
    ret = vsnprintf(buf, 0x400, fmt, ap);
    va_end(ap);
    buf[0x3ff] = '\0';
    if (ret < 0 || 0x3ff < ret) {
        Com_Error(1, "\025Attempted to overrun string in call to va()");
    }
    return buf;
}

// 0x080b805e  size=105  Com_SetThreadBlocks  status=translated  naming=inferred
// registers the three per-thread blocks (slot 1 = va state stride 0x804, slot 2 stride 0x9c, slot 3 stride 0x18) for thread index
void Com_SetThreadBlocks(int threadIndex)
{
    Sys_SetValue(1, (char *)com_vaThreadBuffers + threadIndex * 0x804);
    Sys_SetValue(2, (char *)com_threadJmpBufs + threadIndex * 0x9c);
    Sys_SetValue(3, (char *)g_085b7660_cmTraceSlots + threadIndex * 0x18);
}

// 0x080b80c8  size=64  Com_NarrowWideString  status=translated  naming=unknown
// narrows a 16-bit-per-char string by copying the high byte of each pair (offset +1), then NUL at dest[count-1]
void Com_NarrowWideString(const char *src, char *dest, int count)
{
    int i;
    for (i = 0; i < count; i = i + 1) {
        dest[i] = src[i * 2 + 1];
    }
    dest[count - 1] = 0;
}

// 0x080b8108  size=344  Info_ValueForKey  status=translated  naming=observed
// string "Info_ValueForKey: oversize infostring"; walks \key\value pairs, copies the value into one of two 0x2000 static buffers
char *Info_ValueForKey(const char *s, const char *key)
{
    char keyBuf[0x2008];
    char *value;
    const char *prev;
    int bufOffset;

    if (s != (const char *)0 && key != (const char *)0) {
        size_t len = strlen(s);
        if (0x1fff < len) {
            Com_Error(1, "\025Info_ValueForKey: oversize infostring");
        }
        info_valueBufferIndex = info_valueBufferIndex ^ 1;
        if (*s == '\\') {
            s = s + 1;
        }
        for (;;) {
            char *k = keyBuf;
            while (*s != '\\') {
                if (*s == '\0') {
                    return (char *)"";
                }
                *k = *s;
                k = k + 1;
                s = s + 1;
            }
            *k = '\0';
            bufOffset = info_valueBufferIndex * 0x2000;
            value = (char *)info_valueBuffers + bufOffset;
            prev = s;
            while (s = prev + 1, *s != '\\' && *s != '\0') {
                *value = *s;
                value = value + 1;
                prev = s;
            }
            *value = '\0';
            if (Q_stricmp(key, keyBuf) == 0) {
                return (char *)info_valueBuffers + bufOffset;
            }
            if (*s == '\0') {
                break;
            }
            s = prev + 2;
        }
    }
    return (char *)"";
}

// 0x080b8260  size=184  Info_NextPair  status=translated  naming=inferred
// Q3 Info_NextPair counterpart: reads one \key\value pair at *head into key/value and advances *head
void Info_NextPair(const char **head, char *key, char *value)
{
    const char *p = *head;
    char *o;
    if (*p == '\\') {
        p = p + 1;
    }
    *key = '\0';
    *value = '\0';
    o = key;
    for (;;) {
        if (*p == '\\') {
            *o = '\0';
            o = value;
            while (p = p + 1, *p != '\\' && *p != '\0') {
                *o = *p;
                o = o + 1;
            }
            *o = '\0';
            *head = p;
            return;
        }
        if (*p == '\0') {
            break;
        }
        *o = *p;
        o = o + 1;
        p = p + 1;
    }
    *o = '\0';
    *head = p;
}

// 0x080b8318  size=305  Info_RemoveKey  status=translated  naming=observed
// string "Info_RemoveKey: oversize infostring" (0x400 limit); removes "\key\value" with matching key (case-sensitive strcmp) in place
void Info_RemoveKey(char *s, const char *key)
{
    char value[1024];
    char pkey[1036];
    char *start;
    char *o;
    if (strlen(s) > 0x3ff) {
        Com_Error(1, "\025Info_RemoveKey: oversize infostring");
    }
    if (strchr(key, 0x5c) == (char *)0) {
        do {
            start = s;
            if (*s == '\\') {
                s = s + 1;
            }
            o = pkey;
            while (*s != '\\') {
                if (*s == '\0') {
                    return;
                }
                *o = *s;
                o = o + 1;
                s = s + 1;
            }
            *o = '\0';
            o = value;
            while (s = s + 1, *s != '\\' && *s != '\0') {
                if (*s == '\0') {
                    return;
                }
                *o = *s;
                o = o + 1;
            }
            *o = '\0';
            if (strcmp(key, pkey) == 0) {
                strcpy(start, s);
                return;
            }
        } while (*s != '\0');
    }
}

// 0x080b844a  size=305  Info_RemoveKey_Big  status=translated  naming=observed
// string "Info_RemoveKey_Big: oversize infostring" (0x2000 limit); same as Info_RemoveKey with 8 KiB buffers
void Info_RemoveKey_Big(char *s, const char *key)
{
    char value[8192];
    char pkey[8204];
    char *start;
    char *o;
    if (strlen(s) > 0x1fff) {
        Com_Error(1, "\025Info_RemoveKey_Big: oversize infostring");
    }
    if (strchr(key, 0x5c) == (char *)0) {
        do {
            start = s;
            if (*s == '\\') {
                s = s + 1;
            }
            o = pkey;
            while (*s != '\\') {
                if (*s == '\0') {
                    return;
                }
                *o = *s;
                o = o + 1;
                s = s + 1;
            }
            *o = '\0';
            o = value;
            while (s = s + 1, *s != '\\' && *s != '\0') {
                if (*s == '\0') {
                    return;
                }
                *o = *s;
                o = o + 1;
            }
            *o = '\0';
            if (strcmp(key, pkey) == 0) {
                strcpy(start, s);
                return;
            }
        } while (*s != '\0');
    }
}

// 0x080b857c  size=82  Info_Validate  status=translated  naming=inferred
// Info_Validate: 0 if the string contains '"' or ';', else 1
int Info_Validate(const char *s)
{
    int result;
    if (strchr(s, 0x22) == (char *)0) {
        if (strchr(s, 0x3b) == (char *)0) {
            result = 1;
        } else {
            result = 0;
        }
    } else {
        result = 0;
    }
    return result;
}

// 0x080b85ce  size=563  Info_SetValueForKey  status=translated  naming=observed
// string "Info_SetValueForKey: oversize infostring" and key rejection messages; removes the key, then appends "\key\value" (value stripped of '\\', ';', '"')
void Info_SetValueForKey(char *s, const char *key, const char *value)
{
    char cleanValue[1024];
    char newi[1028];
    int cleanLen;
    int i;
    if (strlen(s) < 0x400) {
        char c;
        cleanLen = 0;
        for (i = 0; i < 0x3ff && (c = value[i], c != '\0'); i = i + 1) {
            if (c != '\\' && (c != ';' && c != '"')) {
                cleanValue[cleanLen] = c;
                cleanLen = cleanLen + 1;
            }
        }
        cleanValue[cleanLen] = '\0';
        if (strchr(key, 0x5c) == (char *)0) {
            if (strchr(key, 0x3b) == (char *)0) {
                if (strchr(key, 0x22) == (char *)0) {
                    int r;
                    Info_RemoveKey(s, key);
                    if (cleanValue[0] != '\0') {
                        r = Com_sprintf(newi, 0x400, "\\%s\\%s", key, cleanValue);
                        if (r < 1) {
                            Com_Printf("\025Info buffer length exceeded, not including key/value pair in response\n");
                        } else {
                            size_t newLen = strlen(newi);
                            size_t curLen = strlen(s);
                            if (curLen + newLen < 0x401) {
                                strcat(s, newi);
                            } else {
                                Com_Printf("\025Info string length exceeded\nkey: '%s'\nvalue: '%s'\nInfo string:\n%s\n",
                                            key, value, s);
                            }
                        }
                    }
                } else {
                    Com_Printf("\025Can't use keys with a \"\nkey: '%s'\nvalue: '%s'", key, value);
                }
            } else {
                Com_Printf("\025Can't use keys with a semicolon\nkey: '%s'\nvalue: '%s'", key, value);
            }
        } else {
            Com_Printf("\025Can't use keys with a \\\nkey: '%s'\nvalue: '%s'", key, value);
        }
    } else {
        Com_Printf("\025Info_SetValueForKey: oversize infostring");
    }
}

// 0x080b8802  size=563  Info_SetValueForKey_Big  status=translated  naming=observed
// big-buffer variant of Info_SetValueForKey (0x2000 buffers); note it still limits the result to strlen sum < 0x401 and calls Info_RemoveKey_Big
void Info_SetValueForKey_Big(char *s, const char *key, const char *value)
{
    char cleanValue[8192];
    char newi[8196];
    int cleanLen;
    int i;
    if (strlen(s) < 0x2000) {
        char c;
        cleanLen = 0;
        for (i = 0; i < 0x1fff && (c = value[i], c != '\0'); i = i + 1) {
            if (c != '\\' && (c != ';' && c != '"')) {
                cleanValue[cleanLen] = c;
                cleanLen = cleanLen + 1;
            }
        }
        cleanValue[cleanLen] = '\0';
        if (strchr(key, 0x5c) == (char *)0) {
            if (strchr(key, 0x3b) == (char *)0) {
                if (strchr(key, 0x22) == (char *)0) {
                    int r;
                    Info_RemoveKey_Big(s, key);
                    if (cleanValue[0] != '\0') {
                        r = Com_sprintf(newi, 0x2000, "\\%s\\%s", key, cleanValue);
                        if (r < 1) {
                            Com_Printf("\025Info buffer length exceeded, not including key/value pair in response\n");
                        } else {
                            size_t newLen = strlen(newi);
                            size_t curLen = strlen(s);
                            if (curLen + newLen < 0x401) {
                                strcat(s, newi);
                            } else {
                                Com_Printf("\025Info string length exceeded\nkey: '%s'\nvalue: '%s'\nInfo string:\n%s\n",
                                            key, value, s);
                            }
                        }
                    }
                } else {
                    Com_Printf("\025Can't use keys with a \"\nkey: '%s'\nvalue: '%s'", key, value);
                }
            } else {
                Com_Printf("\025Can't use keys with a semicolon\nkey: '%s'\nvalue: '%s'", key, value);
            }
        } else {
            Com_Printf("\025Can't use keys with a \\\nkey: '%s'\nvalue: '%s'", key, value);
        }
    } else {
        Com_Printf("\025Info_SetValueForKey: oversize infostring");
    }
}

// 0x080b8a36  size=553  Com_ParseInfoFields  status=translated  naming=inferred
// fills a structure from an info string according to an InfoField table: callback / string copies (0x400,0x40,0x100) / int / bool / float / int(float*1000, truncated)
bool Com_ParseInfoFields(char *base, InfoField *fields, int count, const char *info, int numCustomTypes,
                         int (*customParse)(char *base, const char *value, int type),
                         void (*setField)(void *dst, const char *value))
{
    int i = 0;
    InfoField *f = fields;
    for (;;) {
        const char *value;
        if (count <= i) {
            return i == count;
        }
        value = Info_ValueForKey(info, f->name);
        if (*value != '\0') {
            if (f->type < 8) {
                switch ((unsigned int)f->type) {
                case 0:
                    (*setField)(base + f->offset, value);
                    break;
                case 1:
                    Q_strncpyz(base + f->offset, value, 0x400);
                    break;
                case 2:
                    Q_strncpyz(base + f->offset, value, 0x40);
                    break;
                case 3:
                    Q_strncpyz(base + f->offset, value, 0x100);
                    break;
                case 4:
                    *(int *)(base + f->offset) = atoi(value);
                    break;
                case 5:
                    *(uint32 *)(base + f->offset) = (uint32)(atoi(value) != 0);
                    break;
                case 6: {
                    double d = atof(value);
                    *(float *)(base + f->offset) = (float)d;
                    break;
                }
                case 7: {
                    double d = atof(value);
                    *(int *)(base + f->offset) = (int)((float)d * 1000.0);
                }
                }
            } else if (numCustomTypes < 1 || numCustomTypes <= f->type) {
                Com_Error(1, "\025Bad field type %i\n", f->type);
            } else {
                if ((*customParse)(base, value, f->type) == 0) {
                    return false;
                }
            }
        }
        i = i + 1;
        f = f + 1;
    }
}

// 0x080b8c60  size=30  Ease_OutQuadSigned  status=translated  naming=unknown
// t * (2 - Float_Abs_080b92f6(t)); no string evidence (easing-style helper pair with 0x080b8c7e)
float Ease_OutQuadSigned(float t)
{
    float r = Float_Abs_080b92f6(t);
    return (2.0f - r) * t;
}

// 0x080b8c7e  size=25  Ease_OutQuadInverse  status=translated  naming=unknown
// 1 - Float_Sqrt_080b9310(1 - t)
float Ease_OutQuadInverse(float t)
{
    float r = Float_Sqrt_080b9310(1.0f - t);
    return 1.0f - r;
}

// 0x080b8c98  size=152  BG_ApplyLeanRotationOffset  status=translated  naming=unknown
// when param_3 != 0: builds a rotation from (yaw=param_2, angle=eased*param_4), converts it, then applies it scaled by eased*param_5 to param_1; the callees at 0x080b934c/0x080a5226/0x080b9370 are outside this chunk and have unknown argument types
void BG_ApplyLeanRotationOffset(float *dst, float yaw, float t, float angleScale, float blendScale)
{
    float quat[4];
    float mat[7];
    float eased;
    if (t != 0.0f) {
        eased = Ease_OutQuadSigned(t);
        /* 0x080b8c98 forwards its second stack word (a float yaw, viewangles[1]) as the middle component. */
        Vec3_Set_b_080b934c(quat, 0.0f, yaw, eased * angleScale);
        Angles_ToVectors(quat, 0, mat, 0);
        eased = eased * blendScale;
        Vec3_MA_b(dst, eased, mat, dst);
    }
}

// 0x080b8d30  size=174  Orient_TransformPoint  status=translated  naming=unknown
// affine transform: frame[0..2] origin plus the vector rotated by the 3x3 block at frame[3..11] (column-major style indexing as in the decompile)
void Orient_TransformPoint(const float *frame, const float *in, float *out)
{
    out[0] = in[2] * frame[9] + in[1] * frame[6] + in[0] * frame[3] + frame[0];
    out[1] = in[2] * frame[10] + in[1] * frame[7] + in[0] * frame[4] + frame[1];
    out[2] = in[2] * frame[11] + in[1] * frame[8] + in[0] * frame[5] + frame[2];
}
