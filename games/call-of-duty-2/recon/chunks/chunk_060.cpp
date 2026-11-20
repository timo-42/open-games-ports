/*
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 060.
 * Range 0x08138e64..0x813b8fe: 100 functions. Binary SHA-256
 * 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * Function groups (address order):
 *   0x08138e64..0x81392d8  codepage-aware character reader, language table, classifiers
 *   0x081392d8..0x81394f0  file loading, recursive .str collection
 *   0x081394f0..0x81396d6  localized-string table reset, path helpers
 *   0x0813972e..0x8139c8e  .str line parser (keywords, value unquoting, comment stripping)
 *   0x813a104..0x813a81e  table operations: declare/set/lookup, file and language loading
 *   0x813a8c2..0x813ac92  map (red-black tree) accessors, string-search wrappers
 *   0x813acba..0x813b8f6  red-black tree internals and node/iterator helpers
 *
 * The map is libstdc++ 3.x _Rb_tree<std::string, pair<const std::string, std::string>>.
 * Iterators are node pointers. Where the binary passes an iterator by address, the
 * translation passes it by value. Where it returns an iterator through a hidden
 * pointer, the translation returns it directly, except for the out-of-chunk
 * _M_insert_node (RbTree_InsertWithHint), which keeps the hidden-pointer convention.
 * Operations on std::string go through the cod2_string_* shims in chunk_060_types.h.
 */
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_060_types.h"

/* Forward declarations of this chunk's functions (definitions follow in address order). */


/* ---------------------------------------------------------------------------
 * Character reader and classifiers
 * ------------------------------------------------------------------------- */

// 0x08138e64  size=636  FUN_08138e64  status=translated  naming=inferred
// naming: no literal names this function; it reads one character of localized text. The two-byte branches use codepage classes from the 0x0813815x..0x08138dxx helpers (outside this chunk) and the single-byte branch flags the sentence punctuation !?,.;: (0x21 0x3f 0x2c 0x2e 0x3b 0x3a). Descriptive name LocText_ReadChar.
uint16 LocText_ReadChar(char **cursor, uint32 *is_punct)
{
    const uint8 *p = (const uint8 *)*cursor;
    uint16 pair = (uint16)(((uint16)p[0] << 8) | (uint16)p[1]);
    uint16 ch;

    if (LocText_MultibyteEnabled() != 0) {
        int32 cls = Loc_GetLanguage();
        if (cls == 9) {
            if (Big5_IsValidChar(pair) != 0) {
                *cursor = *cursor + 2;
                if (is_punct == NULL) {
                    return pair;
                }
                *is_punct = Big5_IsSymbolChar(pair) & 0xff;
                return pair;
            }
        } else if (cls < 10) {
            if (cls == 8 && LocText_IsEucKrHangulPair(p[0], p[1]) != 0) {
                *cursor = *cursor + 2;
                if (is_punct == NULL) {
                    return pair;
                }
                *is_punct = 0;
                return pair;
            }
        } else if (cls == 10) {
            if (ShiftJis_IsValidChar(p[0], p[1]) != 0) {
                *cursor = *cursor + 2;
                if (is_punct == NULL) {
                    return pair;
                }
                *is_punct = ShiftJis_IsSymbolChar(pair) & 0xff;
                return pair;
            }
        } else if (cls == 0xb && LocText_IsDbcsPairClassB(pair) != 0) {
            *cursor = *cursor + 2;
            if (is_punct == NULL) {
                return pair;
            }
            *is_punct = (Dbcs8140_IsSymbolChar(pair) != 0) ? 1u : 0u;
            return pair;
        }
    }

    ch = p[0];
    *cursor = *cursor + 1;
    if (is_punct != NULL) {
        uint32 flag = 0;
        if (ch == 0x21 || ch == 0x3f || ch == 0x2c || ch == 0x2e || ch == 0x3b || ch == 0x3a) {
            flag = 1;
        }
        *is_punct = flag;
    }
    return ch;
}

// 0x081390e0  size=10  FUN_081390e0  status=translated  naming=inferred
// naming: returns the global g_loc_isDoubleByteLanguage that gates the double-byte branch of LocText_ReadChar. Descriptive name LocText_MultibyteEnabled.
int32 LocText_MultibyteEnabled(void)
{
    return g_loc_isDoubleByteLanguage;
}

// 0x081390ea  size=40  FUN_081390ea  status=translated  naming=inferred
// naming: true when the current position's byte class is not one of the double-byte classes 9, 10, 11 (unsigned range test 2 < cls-9). Descriptive name LocText_ClassIsSingleByte.
bool LocText_ClassIsSingleByte(void)
{
    int32 cls = Loc_GetLanguage();
    return 2u < (uint32)(cls - 9);
}

// 0x08139112  size=139  FUN_08139112  status=translated  naming=inferred
// naming: counts displayed characters of a localized string: the color code ^<digit> is skipped and CR/LF are not counted. Descriptive name LocText_CountVisibleChars.
int32 LocText_CountVisibleChars(char *text)
{
    int32 count = 0;
    char *cursor;

    if (text == NULL) {
        return 0;
    }
    cursor = text;
    while (*cursor != '\0') {
        int32 ch = (int32)LocText_ReadChar(&cursor, NULL);
        if (ch == 0x5e && cursor != NULL && *cursor != '^' && '/' < *cursor && *cursor < ':') {
            cursor = cursor + 1;
        } else if (ch != 10 && ch != 13) {
            count = count + 1;
        }
    }
    return count;
}

// 0x0813919e  size=48  FUN_0813919e  status=translated  naming=inferred
// naming: indexes the language table loc_languageTable (first entry "english"); out-of-range indices fall back to entry 0. Descriptive name LocLanguage_Name.
const char *LocLanguage_Name(int32 index)
{
    if (index < 0 || index > 0xe) {
        return loc_languageTable[0].name;
    }
    return loc_languageTable[index].name;
}

// 0x081391ce  size=95  FUN_081391ce  status=translated  naming=inferred
// naming: linear search of the 15-entry language table with Q_stricmp; writes the index (0 when absent). Descriptive name LocLanguage_Find.
int32 LocLanguage_Find(const char *name, int32 *index)
{
    int32 i = 0;
    while (true) {
        if (i > 0xe) {
            *index = 0;
            return 0;
        }
        if (Q_stricmp(name, loc_languageTable[i].name) == 0) {
            break;
        }
        i = i + 1;
    }
    *index = i;
    return 1;
}

// 0x0813922e  size=61  FUN_0813922e  status=translated  naming=inferred
// naming: EUC-KR style two-byte pair test: lead 0xB0..0xC8, trail above 0xA0 and not 0xFF. Descriptive name LocText_IsEucKrHangulPair.
int32 LocText_IsEucKrHangulPair(uint8 lead, uint8 trail)
{
    int32 result = 0;
    if (lead > 0xaf && lead < 0xc9 && trail > 0xa0 && trail != 0xff) {
        result = 1;
    }
    return result;
}

// 0x0813926c  size=36  FUN_0813926c  status=translated  naming=inferred
// naming: splits a 16-bit code into bytes and applies LocText_IsEucKrHangulPair. Descriptive name LocText_IsEucKrHangul.
uint8 LocText_IsEucKrHangul(uint16 code)
{
    return (uint8)LocText_IsEucKrHangulPair((uint8)((code >> 8) & 0xff), (uint8)(code & 0xff));
}

// 0x08139290  size=36  FUN_08139290  status=translated  naming=inferred
// naming: class-10 pair test; forwards the two bytes to ShiftJis_IsValidChar. Descriptive name LocText_IsDbcsPairClass10.
uint8 LocText_IsDbcsPairClass10(uint16 code)
{
    return (uint8)ShiftJis_IsValidChar((uint8)((code >> 8) & 0xff), (uint8)(code & 0xff));
}

// 0x081392b4  size=36  FUN_081392b4  status=translated  naming=inferred
// naming: class-11 pair test; forwards the two bytes to Dbcs_IsLeadTrailPair. Descriptive name LocText_IsDbcsPairClassB.
uint8 LocText_IsDbcsPairClassB(uint16 code)
{
    return (uint8)Dbcs_IsLeadTrailPair((uint8)((code >> 8) & 0xff), (uint8)(code & 0xff));
}

/* ---------------------------------------------------------------------------
 * File loading and collection
 * ------------------------------------------------------------------------- */

// 0x081392d8  size=53  FUN_081392d8  status=translated  naming=inferred
// naming: loads a whole file through FS_ReadFile; returns the buffer, or NULL when the length is below 1. Descriptive name LocFile_LoadText.
void *LocFile_LoadText(const char *qpath)
{
    void *buffer = NULL;
    int32 length = FS_ReadFile(qpath, &buffer);
    if (length < 1) {
        return NULL;
    }
    return buffer;
}

// 0x0813930e  size=19  FUN_0813930e  status=translated  naming=inferred
// naming: releases a buffer from LocFile_LoadText through FS_FreeFile. Descriptive name LocFile_Free.
void LocFile_Free(void *buffer)
{
    FS_FreeFile(buffer);
}

// 0x08139322  size=391  FUN_08139322  status=translated  naming=inferred
// naming: recursive directory walk. Lists subdirectories (extension "/"), recurses into each except names starting with '.', then appends every "dir/file" that matches the extension to the ';'-separated list and counts it in loc_strFileCount. Descriptive name FS_CollectFilesRecursive.
void FS_CollectFilesRecursive(const char *extension, const char *dir, struct cod2_string_s *list)
{
    char entry_path[76];
    int32 dir_count = 0;
    int32 file_count = 0;
    char **subdirs;
    char **files;
    int32 i;

    subdirs = FS_ListFiles(dir, "/", 0, &dir_count, 10);
    for (i = 0; i < dir_count; i = i + 1) {
        if (subdirs[i][0] != '\0' && subdirs[i][0] != '.') {
            sprintf(entry_path, "%s/%s", dir, subdirs[i]);
            FS_CollectFilesRecursive(extension, entry_path, list);
        }
    }

    files = FS_ListFiles(dir, extension, 0, &file_count, 10);
    for (i = 0; i < file_count; i = i + 1) {
        sprintf(entry_path, "%s/%s", dir, files[i]);
        cod2_string_append_cstr(list, entry_path);
        cod2_string_append_char(list, ';');
        loc_strFileCount = loc_strFileCount + 1;
    }
    FS_FreeFileList(files, 10);
    FS_FreeFileList(subdirs, 10);
}

// 0x081394aa  size=68  FUN_081394aa  status=translated  naming=inferred
// naming: resets the count and the list, collects every ".str" file under dir (the extension is the literal "str" at 0x08160add), and returns the count. Descriptive name FS_CollectLocalizedFiles.
int32 FS_CollectLocalizedFiles(const char *dir, struct cod2_string_s *list)
{
    loc_strFileCount = 0;
    cod2_string_assign_cstr(list, "");
    FS_CollectFilesRecursive("str", dir, list);
    return loc_strFileCount;
}

/* ---------------------------------------------------------------------------
 * Table reset and path helpers
 * ------------------------------------------------------------------------- */

// 0x081394f0  size=75  FUN_081394f0  status=translated  naming=inferred
// naming: clears the entry map, the ENDMARKER flag and the two strings (last keyword, last English value). Descriptive name LocTable_Reset.
void LocTable_Reset(struct cod2_loc_table_s *t)
{
    LocMap_ClearWrap(&t->entries);
    t->end_marker_seen = 0;
    cod2_string_assign_cstr(&t->last_keyword, "");
    cod2_string_assign_cstr(&t->last_english, "");
}

// 0x0813953c  size=130  FUN_0813953c  status=translated  naming=inferred
// naming: copies a path into loc_pathDirBuf (64 bytes) and cuts it at the last '/' or '\' (directory part). Descriptive name LocPath_GetDirectory.
char *LocPath_GetDirectory(void *ctx, const char *path)
{
    char *last_backslash;
    char *last_slash;
    char *cut;

    (void)ctx;
    Q_strncpyz(loc_pathDirBuf, path, 0x40);
    last_backslash = Str_FindLastChar(loc_pathDirBuf, 0x5c);
    last_slash = Str_FindLastChar(loc_pathDirBuf, 0x2f);
    cut = last_slash;
    if ((uint32)cut < (uint32)last_backslash) {
        cut = last_backslash;
    }
    if (cut != NULL) {
        *cut = '\0';
    }
    return loc_pathDirBuf;
}

// 0x081395be  size=157  FUN_081395be  status=translated  naming=inferred
// naming: copies a path into loc_pathStemBuf and cuts the last '.' when no '\' or '/' follows it (extension removal). Descriptive name LocPath_StripExtension.
char *LocPath_StripExtension(void *ctx, const char *path)
{
    char *dot;
    char *backslash;
    char *slash;

    (void)ctx;
    strcpy(loc_pathStemBuf, path);
    dot = Str_FindLastChar(loc_pathStemBuf, 0x2e);
    backslash = Str_FindLastChar(loc_pathStemBuf, 0x5c);
    slash = Str_FindLastChar(loc_pathStemBuf, 0x2f);
    if (dot != NULL
        && (backslash == NULL || backslash < dot)
        && (slash == NULL || slash < dot)) {
        *dot = '\0';
    }
    return loc_pathStemBuf;
}

// 0x0813965c  size=80  FUN_0813965c  status=translated  naming=inferred
// naming: copies the part after the last '/' or '\' into loc_pathBaseNameBuf (base name). Descriptive name LocPath_GetBaseName.
char *LocPath_GetBaseName(void *ctx, const char *path)
{
    const char *base = path;
    const char *p = path;

    (void)ctx;
    for (; *p != '\0'; p = p + 1) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    strcpy(loc_pathBaseNameBuf, base);
    return loc_pathBaseNameBuf;
}

// 0x081396ac  size=41  FUN_081396ac  status=translated  naming=inferred
// naming: base name of the directory of a path (calls LocPath_GetDirectory then LocPath_GetBaseName). Descriptive name LocPath_GetParentBaseName.
void LocPath_GetParentBaseName(void *ctx, const char *path)
{
    char *dir = LocPath_GetDirectory(ctx, path);
    LocPath_GetBaseName(ctx, dir);
}

// 0x081396d6  size=88  FUN_081396d6  status=translated  naming=inferred
// naming: stores the upper-cased file base name (extension removed) in table->file_base; this prefix builds every entry key. Descriptive name LocTable_SetFileBaseName.
void LocTable_SetFileBaseName(struct cod2_loc_table_s *t, const char *path)
{
    char name[72];
    char *stem;
    const char *base;

    stem = LocPath_StripExtension(t, path);
    base = LocPath_GetBaseName(t, stem);
    strcpy(name, base);
    Q_strupr(name);
    cod2_string_assign_cstr(&t->file_base, name);
}

/* ---------------------------------------------------------------------------
 * .str line parser
 * ------------------------------------------------------------------------- */

// 0x0813972e  size=123  FUN_0813972e  status=translated  naming=inferred
// naming: case-insensitive keyword match at the cursor; on a match, advances past the keyword and any following spaces or tabs. Descriptive name LocLine_ConsumeKeyword.
int32 LocLine_ConsumeKeyword(struct cod2_loc_table_s *t, const char *keyword, char **cursor)
{
    size_t len;

    (void)t;
    len = strlen(keyword);
    if (strncasecmp(keyword, *cursor, len) != 0) {
        return 0;
    }
    len = strlen(keyword);
    *cursor = *cursor + len;
    while (**cursor == '\t' || **cursor == ' ') {
        *cursor = *cursor + 1;
    }
    return 1;
}

// 0x081397aa  size=183  FUN_081397aa  status=translated  naming=inferred
// naming: checks the "&&<digit>" slot markers in a localized line: each digit (1..9 by value) may appear once, and a non-digit after "&&" fails. Descriptive name LocLine_CheckAmpersandSlots. Quirk: the binary indexes the flags by digit-'0'-1, so digit '0' reads the byte before the flag array; that byte is uninitialised there and is zeroed here (open question).
int32 LocLine_CheckAmpersandSlots(const char *line)
{
    uint8 flag_storage[10];
    uint8 *flags = flag_storage + 1;
    const char *cursor;

    memset(flag_storage, 0, sizeof flag_storage);
    cursor = Str_FindSubstr(line, "&&");
    while (true) {
        int32 index;
        if (cursor == NULL) {
            return 1;
        }
        cursor = cursor + 2;
        if (!isdigit((int)*cursor)) {
            return 0;
        }
        index = (int32)(*cursor) - '0' - 1;
        if (flags[index] != 0) {
            return 0;
        }
        flags[index] = 1;
        cursor = cursor + 1;
        cursor = Str_FindSubstr(cursor, "&&");
    }
}

// 0x08139862  size=126  FUN_08139862  status=translated  naming=inferred
// naming: copies the input string and replaces each two-character escape "\n" (0x08160b04) with a real newline. Descriptive name LocLine_UnescapeNewlines.
struct cod2_string_s *LocLine_UnescapeNewlines(struct cod2_string_s *out, void *ctx, const struct cod2_string_s *in)
{
    uint32 pos;

    (void)ctx;
    cod2_string_init(out);
    cod2_string_assign(out, in);
    while ((pos = cod2_string_find_cstr(out, "\\n", 0)) != 0xffffffffu) {
        *cod2_string_at(out, pos) = '\n';
        cod2_string_erase(out, pos + 1, 1);
    }
    return out;
}

// 0x0813990a  size=211  FUN_0813990a  status=translated  naming=inferred
// naming: removes a "//" comment outside quotes and trims trailing whitespace. The quote count accumulates across searches (binary behaviour kept). Descriptive name LocLine_StripComment.
void LocLine_StripComment(void *ctx, char *line)
{
    int32 quotes = 0;
    char *cursor = line;
    char *comment;
    int32 len;

    (void)ctx;
    while (true) {
        size_t i;
        comment = Str_FindSubstr(cursor, "//");
        if (comment == NULL) {
            return;
        }
        for (i = 0; (int32)i < (int32)(comment - cursor); i = i + 1) {
            if (cursor[i] == '"') {
                quotes = quotes + 1;
            }
        }
        if ((quotes & 1) == 0) { /* even number of quotes so far: the "//" starts a comment (asm: (q ^ 1) & 1 == 0 skips) */
            break;
        }
        cursor = comment + 1;
    }
    *comment = '\0';
    if (*cursor == '\0') {
        return;
    }
    len = (int32)strlen(cursor);
    while (true) {
        len = len - 1;
        if (len < 0) {
            return;
        }
        if (!isspace((int)cursor[len])) {
            break;
        }
        cursor[len] = '\0';
    }
}

// 0x081399de  size=316  FUN_081399de  status=translated  naming=inferred
// naming: reads the next line from the file text (skipping CR/LF after it), copies it to line, strips trailing whitespace and comments; returns 0 at end of text. Descriptive name LocLine_ReadNext.
int32 LocLine_ReadNext(struct cod2_loc_table_s *t, char **cursor, char *line)
{
    char *newline;
    int32 len;

    if (**cursor == '\0') {
        return 0;
    }
    newline = strchr(*cursor, '\n');
    if (newline != NULL) {
        int32 n = (int32)(newline - *cursor);
        strncpy(line, *cursor, (size_t)n);
        line[n] = '\0';
        *cursor = *cursor + n;
        while (**cursor != '\0' && strchr("\r\n", (int)(signed char)**cursor) != NULL) {
            *cursor = *cursor + 1;
        }
    } else {
        strcpy(line, *cursor);
        *cursor = *cursor + strlen(*cursor);
    }

    if (line[0] != '\0') {
        len = (int32)strlen(line) - 1;
        while (len >= 0 && isspace((int)(signed char)line[len]) != 0) {
            line[len] = '\0';
            len--;
        }
        LocLine_StripComment(t, line);
    }
    return 1;
}

// 0x08139b1a  size=329  FUN_08139b1a  status=translated  naming=inferred
// naming: value unquoting: skips leading blanks and one opening quote, copies the rest, then removes trailing blanks and one closing quote. Descriptive name LocLine_ParseValue.
// Quirk kept: if trimming empties the string, the binary reads the byte before the buffer on the next pass; this translation returns instead (open question).
struct cod2_string_s *LocLine_ParseValue(struct cod2_string_s *out, void *ctx, const char *text)
{
    (void)ctx;
    cod2_string_init(out);
    cod2_string_assign_cstr(out, "");
    while (*text == ' ' || *text == '\t') {
        text = text + 1;
    }
    if (*text == '"') {
        text = text + 1;
    }
    cod2_string_assign_cstr(out, text);
    if (*text == '\0') {
        return out;
    }
    while (true) {
        const char *s = cod2_string_c_str(out);
        uint32 len = (uint32)strlen(s);
        char last;
        if (len == 0) {
            return out;
        }
        last = s[len - 1];
        if (last != ' ' && last != '\t') {
            if (last == '"') {
                cod2_string_erase(out, len - 1, 0xffffffffu);
            }
            return out;
        }
        cod2_string_erase(out, len - 1, 0xffffffffu);
    }
}

// 0x08139c8e  size=156  FUN_08139c8e  status=translated  naming=inferred
// naming: dispatches the .str keywords "VERSION", "CONFIG", "FILENOTES", "NOTES", "FLAGS", "REFERENCE", "ENDMARKER", "LANG_" (literals in the binary) and reports "Unexpected version number %d, expecting %d!", "Unknown keyword at linestart", "Illegal string format"; the literals describe the file format, not this function's name. Descriptive name LocLine_Parse.
const char *LocLine_Parse(struct cod2_loc_table_s *t, char *line, uint8 english_only)
{
    const char *error = NULL;

    if (line == NULL) {
        return NULL;
    }
    if (LocLine_ConsumeKeyword(t, "VERSION", &line) != 0) {
        struct cod2_string_s value;
        int32 version;
        LocLine_ParseValue(&value, t, line);
        version = atoi(cod2_string_c_str(&value));
        if (version != 1) {
            error = va("Unexpected version number %d, expecting %d!\n", version, 1);
        }
        cod2_string_destroy(&value);
        return error;
    }

    if (LocLine_ConsumeKeyword(t, "CONFIG", &line) == 0
        && LocLine_ConsumeKeyword(t, "FILENOTES", &line) == 0
        && LocLine_ConsumeKeyword(t, "NOTES", &line) == 0
        && LocLine_ConsumeKeyword(t, "FLAGS", &line) == 0) {
        if (LocLine_ConsumeKeyword(t, "REFERENCE", &line) != 0) {
            struct cod2_string_s key;
            LocLine_ParseValue(&key, t, line);
            LocTable_DeclareKey(t, cod2_string_c_str(&key));
            cod2_string_destroy(&key);
        } else if (LocLine_ConsumeKeyword(t, "ENDMARKER", &line) != 0) {
            t->end_marker_seen = 1;
        } else if (strncasecmp("LANG_", line, 5) == 0) {
            const char *keyword = LocTable_LastKeyword(t);
            if (*keyword == '\0') {
                error = "Error parsing file: Unexpected \"LANG_\"\n";
            } else {
                char *p = line + 5;
                char *end = p;
                char language[1032];
                size_t n;
                struct cod2_string_s raw;
                struct cod2_string_s value;
                int32 is_english;

                while (*end != '\0' && *end != ' ' && *end != '\t') {
                    end = end + 1;
                }
                memset(language, 0, 1024);
                n = (size_t)(end - p);
                if (n > 0x3ff) {
                    n = 0x3ff;
                }
                strncpy(language, p, n);
                p = p + strlen(language);

                LocLine_ParseValue(&raw, t, p);
                LocLine_UnescapeNewlines(&value, t, &raw);
                cod2_string_destroy(&raw);

                if (LocLine_CheckAmpersandSlots(cod2_string_c_str(&value)) == 0) {
                    error = va("Illegal string format \"%s\"\n", cod2_string_c_str(&value));
                }
                is_english = strcasecmp(language, "english");
                if (error == NULL && (is_english == 0 || english_only == 0)) {
                    LocTable_SetValue(t, keyword, cod2_string_c_str(&value), is_english == 0);
                }
                cod2_string_destroy(&value);
            }
        } else {
            error = va("Unknown keyword at linestart: \"%s\"\n", line);
        }
    }
    return error;
}

/* ---------------------------------------------------------------------------
 * Table operations
 * ------------------------------------------------------------------------- */

// 0x0813a104  size=22  FUN_0813a104  status=translated  naming=inferred
// naming: returns the C string of the last REFERENCE key (table field at +0x04). Descriptive name LocTable_LastKeyword.
const char *LocTable_LastKeyword(struct cod2_loc_table_s *t)
{
    return cod2_string_c_str(&t->last_keyword);
}

// 0x0813a11a  size=124  FUN_0813a11a  status=translated  naming=inferred
// naming: REFERENCE handling. Makes sure the entry "<file base>_<key>" exists (creating it with an empty value when missing) and sets the last keyword to the key. Descriptive name LocTable_DeclareKey.
void LocTable_DeclareKey(struct cod2_loc_table_s *t, const char *keyword)
{
    struct cod2_string_s key;
    struct cod2_rb_node_s *it;

    cod2_string_init_cstr(&key, va("%s_%s", cod2_string_c_str(&t->file_base), keyword));
    it = LocMap_Find(&t->entries, &key);
    cod2_string_destroy(&key);

    if (it == LocMap_End(&t->entries)) {
        struct cod2_string_s empty;
        struct cod2_string_s key2;
        struct cod2_string_s *slot;

        cod2_string_init(&empty);
        cod2_string_init_cstr(&key2, va("%s_%s", cod2_string_c_str(&t->file_base), keyword));
        slot = LocMap_Subscript(&t->entries, &key2);
        cod2_string_assign(slot, &empty);
        cod2_string_destroy(&key2);
        cod2_string_destroy(&empty);
    }
    cod2_string_assign_cstr(&t->last_keyword, keyword);
}

// 0x0813a31e  size=124  FUN_0813a31e  status=translated  naming=inferred
// naming: stores a value under "<file base>_<key>". English values also update last_english, and "#same" in a non-English value takes last_english. The key is assumed to exist (REFERENCE); a missing key writes into the sentinel node (binary behaviour kept). Descriptive name LocTable_SetValue.
void LocTable_SetValue(struct cod2_loc_table_s *t, const char *keyword, const char *value, uint8 is_english)
{
    struct cod2_string_s key;
    struct cod2_rb_node_s *it;
    struct cod2_string_s *slot;

    cod2_string_init_cstr(&key, va("%s_%s", cod2_string_c_str(&t->file_base), keyword));
    it = LocMap_Find(&t->entries, &key);
    cod2_string_destroy(&key);
    slot = (struct cod2_string_s *)&it->value;

    if (is_english == 0) {
        if (strcasecmp(value, "#same") == 0) {
            cod2_string_assign(slot, &t->last_english);
        } else {
            cod2_string_assign_cstr(slot, value);
        }
    } else {
        cod2_string_assign_cstr(slot, value);
        cod2_string_assign_cstr(&t->last_english, value);
    }
}

// 0x0813a46c  size=188  FUN_0813a46c  status=translated  naming=inferred
// naming: pops the first ';'-terminated entry of the file list into loc_listEntryBuf (at most 63 characters) and returns it, or NULL when the list is empty. Descriptive name LocList_NextEntry.
char *LocList_NextEntry(struct cod2_string_s *list)
{
    char *sep;

    if (cod2_string_c_str(list)[0] == '\0') {
        return NULL;
    }
    strncpy(loc_listEntryBuf, cod2_string_c_str(list), 0x3f);
    loc_listEntryBuf[63] = '\0';
    sep = Str_FindChar(loc_listEntryBuf, ';');
    if (sep != NULL) {
        *sep = '\0';
        cod2_string_erase(list, 0, (uint32)(sep - loc_listEntryBuf) + 1);
    } else {
        cod2_string_erase(list, 0, 0xffffffffu);
    }
    return loc_listEntryBuf;
}

// 0x0813a528  size=295  FUN_0813a528  status=translated  naming=inferred
// naming: loads one .str file into the current table; reports "Unable to load \"%s\"!" and "Truncated file, failed to find \"%s\" at file end!" (ENDMARKER check). Descriptive name LocTable_LoadFile.
const char *LocTable_LoadFile(const char *qpath, uint8 english_only)
{
    char line[16399];
    char *text_base;
    char *cursor;
    const char *error = NULL;

    text_base = (char *)LocFile_LoadText(qpath);
    cursor = text_base;
    if (cursor == NULL) {
        return va("Unable to load \"%s\"!", qpath);
    }

    LocTable_SetFileBaseName(loc_currentTable, qpath);
    while (error == NULL && LocLine_ReadNext(loc_currentTable, &cursor, line) != 0) {
        if (line[0] != '\0') {
            error = LocLine_Parse(loc_currentTable, line, english_only);
        }
    }
    LocFile_Free(text_base);

    if (error == NULL && LocTable_EndMarkerSeen(loc_currentTable) == 0) {
        error = va("Truncated file, failed to find \"%s\" at file end!", "ENDMARKER");
    }
    return error;
}

// 0x0813a650  size=84  FUN_0813a650  status=translated  naming=inferred
// naming: looks up a key in the current table (no prefix); returns the value's C string, or NULL when absent. Descriptive name LocTable_Lookup.
const char *LocTable_Lookup(const char *name)
{
    struct cod2_string_s key;
    struct cod2_rb_node_s *it;
    const char *result = NULL;

    cod2_string_init_cstr(&key, name);
    it = LocMap_Find(&loc_currentTable->entries, &key);
    cod2_string_destroy(&key);
    if (it != LocMap_End(&loc_currentTable->entries)) {
        result = cod2_string_c_str(&it->key + 1);
    }
    return result;
}

// 0x0813a752  size=21  FUN_0813a752  status=translated  naming=inferred
// naming: resets the current table. Descriptive name LocTable_ResetCurrent.
void LocTable_ResetCurrent(void)
{
    LocTable_Reset(loc_currentTable);
}

// 0x0813a768  size=46  FUN_0813a768  status=translated  naming=inferred
// naming: allocates, constructs and resets a new table and makes it the current one. Descriptive name LocTable_Create.
void LocTable_Create(void)
{
    struct cod2_loc_table_s *t = (struct cod2_loc_table_s *)LocTable_Alloc();

    LocTable_Construct(t);
    loc_currentTable = t;
    LocTable_Reset(t);
}

// 0x0813a7d0  size=78  FUN_0813a7d0  status=translated  naming=inferred
// naming: resets and destroys the current table, then clears the global. Descriptive name LocTable_Destroy.
void LocTable_Destroy(void)
{
    struct cod2_loc_table_s *t;

    if (loc_currentTable != NULL) {
        LocTable_Reset(loc_currentTable);
        t = loc_currentTable;
        if (t != NULL) {
            LocTable_Dispose(t);
            LocTable_Free(t);
        }
        loc_currentTable = NULL;
    }
}

// 0x0813a81e  size=124  FUN_0813a81e  status=translated  naming=inferred
// naming: loads every .str file under the literal directory "localizedstrings" into the current table, stopping at the first error. Descriptive name LocTable_LoadAll.
const char *LocTable_LoadAll(uint8 english_only)
{
    struct cod2_string_s list;
    const char *error = NULL;

    cod2_string_init(&list);
    LocTable_ResetCurrent();
    FS_CollectLocalizedFiles("localizedstrings", &list);
    while (true) {
        char *name = LocList_NextEntry(&list);
        if (name == NULL || error != NULL) {
            break;
        }
        error = LocTable_LoadFile(name, english_only);
    }
    cod2_string_destroy(&list);
    return error;
}

/* ---------------------------------------------------------------------------
 * Map (red-black tree) accessors
 * ------------------------------------------------------------------------- */

// 0x0813a8c2  size=21  FUN_0813a8c2  status=translated  naming=inferred
// naming: iterator equality (node pointers). Descriptive name LocIter_Equal.
bool LocIter_Equal(struct cod2_rb_node_s **a, struct cod2_rb_node_s **b)
{
    return *a == *b;
}

// 0x0813a8d8  size=44  FUN_0813a8d8  status=translated  naming=inferred
// naming: find through the tree search (0x0813aff6). Descriptive name LocMap_Find.
struct cod2_rb_node_s *LocMap_Find(struct cod2_str_map_s *m, const struct cod2_string_s *key)
{
    return LocRb_Find(m, key);
}

// 0x0813a904  size=37  FUN_0813a904  status=translated  naming=inferred
// naming: end() of the map (the sentinel header). Descriptive name LocMap_End.
struct cod2_rb_node_s *LocMap_End(struct cod2_str_map_s *m)
{
    return LocMap_EndIter(m);
}

// 0x0813a92a  size=264  FUN_0813a92a  status=translated  naming=inferred
// naming: operator[] of the map: lower_bound, insert a pair (key, "") when the key is absent, return the value. Descriptive name LocMap_Subscript.
struct cod2_string_s *LocMap_Subscript(struct cod2_str_map_s *m, const struct cod2_string_s *key)
{
    struct cod2_rb_node_s *it = LocRb_LowerBoundWrap(m, key);

    if (it == LocMap_EndIter(m)
        || LocMap_KeyLess(&m->key_compare, key, &it->key) != 0) {
        struct cod2_string_s empty;
        struct cod2_string_s pair[2];

        cod2_string_init(&empty);
        LocPair_Init(pair, key, &empty);
        it = LocMap_InsertAt(m, it, pair);
        LocPair_Destroy(pair);
        cod2_string_destroy(&empty);
    }
    return (struct cod2_string_s *)((uint8 *)it + 0x14);
}

// 0x0813aaa8  size=19  FUN_0813aaa8  status=translated  naming=inferred
// naming: wrapper of LocIter_KeyAddr (the key slot of the node). Descriptive name LocIter_KeyAddrWrap.
struct cod2_string_s *LocIter_KeyAddrWrap(struct cod2_rb_node_s **it)
{
    return LocIter_KeyAddr(it);
}

// 0x0813aabc  size=21  FUN_0813aabc  status=translated  naming=inferred
// naming: iterator inequality. Descriptive name LocIter_NotEqual.
bool LocIter_NotEqual(struct cod2_rb_node_s **a, struct cod2_rb_node_s **b)
{
    return *a != *b;
}

// 0x0813aad2  size=69  FUN_0813aad2  status=translated  naming=inferred
// naming: insert with a hint: copies the hint, then _M_insert_unique with the hint. Descriptive name LocMap_InsertAt.
struct cod2_rb_node_s *LocMap_InsertAt(struct cod2_str_map_s *m, struct cod2_rb_node_s *hint, const struct cod2_string_s *pair)
{
    struct cod2_rb_node_s *copy = LocIter_Copy(hint);
    return LocRb_InsertHint(m, copy, pair);
}

// 0x0813ab18  size=26  FUN_0813ab18  status=translated  naming=inferred
// naming: thin wrapper of libc strchr (the call is a tail call; its result is the return value). Descriptive name Str_FindChar.
char *Str_FindChar(char *s, int c)
{
    return strchr(s, c);
}

// 0x0813ab32  size=26  FUN_0813ab32  status=translated  naming=inferred
// naming: thin wrapper of libc strrchr. Descriptive name Str_FindLastChar.
char *Str_FindLastChar(char *s, int c)
{
    return strrchr(s, c);
}

// 0x0813ab4c  size=26  FUN_0813ab4c  status=translated  naming=inferred
// naming: thin wrapper of libc strstr. Descriptive name Str_FindSubstr.
char *Str_FindSubstr(const char *haystack, const char *needle)
{
    return (char *)strstr(haystack, needle);
}

// 0x0813ab66  size=65  FUN_0813ab66  status=translated  naming=inferred
// naming: constructor of the table: three empty strings, then the map. Descriptive name LocTable_Construct.
void LocTable_Construct(struct cod2_loc_table_s *t)
{
    cod2_string_init(&t->last_keyword);
    cod2_string_init(&t->last_english);
    cod2_string_init(&t->file_base);
    LocMap_Construct(&t->entries);
}

// 0x0813abf8  size=23  FUN_0813abf8  status=translated  naming=inferred
// naming: destructor of the table: map first, then the strings in reverse order. Descriptive name LocTable_Dispose.
void LocTable_Dispose(struct cod2_loc_table_s *t)
{
    LocMap_DestroyMap(&t->entries);
    cod2_string_destroy(&t->file_base);
    cod2_string_destroy(&t->last_english);
    cod2_string_destroy(&t->last_keyword);
}

// 0x0813ac92  size=20  FUN_0813ac92  status=translated  naming=inferred
// naming: allocates one zeroed 0x20-byte table (the caller's size argument is ignored by the binary too). Descriptive name LocTable_Alloc.
void *LocTable_Alloc(void)
{
    return Com_MallocZeroed(0x20);
}

// 0x0813aca6  size=19  FUN_0813aca6  status=translated  naming=inferred
// naming: frees a table through Com_Free_A. Descriptive name LocTable_Free.
void LocTable_Free(void *p)
{
    Com_Free_A(p);
}

// 0x0813acba  size=10  FUN_0813acba  status=translated  naming=inferred
// naming: reads the ENDMARKER flag of the table. Descriptive name LocTable_EndMarkerSeen.
int32 LocTable_EndMarkerSeen(struct cod2_loc_table_s *t)
{
    return t->end_marker_seen;
}

// 0x0813acc4  size=19  FUN_0813acc4  status=translated  naming=inferred
// naming: wrapper that clears a map. Descriptive name LocMap_ClearWrap.
void LocMap_ClearWrap(struct cod2_str_map_s *m)
{
    LocMap_Clear(m);
}

// 0x0813acd8  size=114  FUN_0813acd8  status=translated  naming=inferred
// naming: _M_erase of the whole tree: erases the subtree from the root, resets the sentinel to point at itself, and zeroes the count. Descriptive name LocMap_Clear.
void LocMap_Clear(struct cod2_str_map_s *m)
{
    if (m->node_count != 0) {
        struct cod2_rb_node_s *root = *LocMap_Root(m);
        LocMap_EraseSubtree(m, root);
        m->header->left = m->header;
        m->header->parent = NULL;
        m->header->right = m->header;
        m->node_count = 0;
    }
}

// 0x0813ad4a  size=14  FUN_0813ad4a  status=translated  naming=inferred
// naming: the root of the tree (sentinel parent link). The binary returns the address of this link; returns the address of this link (verified: lea/add on [m->header]). Descriptive name LocMap_Root.
struct cod2_rb_node_s **LocMap_Root(const struct cod2_str_map_s *m)
{
    return &m->header->parent;
}

// 0x0813ad58  size=33  FUN_0813ad58  status=translated  naming=inferred
// naming: destroys a pair of strings (second, then first). Descriptive name LocPair_Destroy.
void LocPair_Destroy(struct cod2_string_s *pair)
{
    cod2_string_destroy(&pair[1]);
    cod2_string_destroy(&pair[0]);
}

// 0x0813ad7a  size=87  FUN_0813ad7a  status=translated  naming=inferred
// naming: _M_erase(x): erases the right subtree recursively, then destroys x and continues with its left child. Descriptive name LocMap_EraseSubtree.
void LocMap_EraseSubtree(struct cod2_str_map_s *m, struct cod2_rb_node_s *x)
{
    while (x != NULL) {
        struct cod2_rb_node_s *left;
        LocMap_EraseSubtree(m, x->right);
        left = x->left;
        LocMap_DestroyNode(m, x);
        x = left;
    }
}

// 0x0813add2  size=14  FUN_0813add2  status=translated  naming=inferred
// naming: the leftmost link of the sentinel (returns the address of the link). Descriptive name LocMap_Leftmost.
struct cod2_rb_node_s **LocMap_Leftmost(const struct cod2_str_map_s *m)
{
    return &m->header->left;
}

// 0x0813ade0  size=14  FUN_0813ade0  status=translated  naming=inferred
// naming: the rightmost link of the sentinel (returns the address of the link). Descriptive name LocMap_Rightmost.
struct cod2_rb_node_s **LocMap_Rightmost(const struct cod2_str_map_s *m)
{
    return &m->header->right;
}

// 0x0813adee  size=29  FUN_0813adee  status=translated  naming=inferred
// naming: iterator copy (the no-op prelude 0x0813b730 has no effect). Descriptive name LocIter_Copy.
struct cod2_rb_node_s *LocIter_Copy(struct cod2_rb_node_s *src)
{
    LocIter_NoopPrep();
    return src;
}

// 0x0813ae0c  size=44  FUN_0813ae0c  status=translated  naming=inferred
// naming: map constructor: runs the compare-tag no-op, then LocMap_Init. Descriptive name LocMap_Construct.
void LocMap_Construct(struct cod2_str_map_s *m)
{
    uint8 alloc_tag = 0;
    uint8 compare_tag = 0;

    LocMap_CompareInit(&compare_tag);
    LocMap_Init(m, &compare_tag, &alloc_tag);
}

// 0x0813ae38  size=5  FUN_0813ae38  status=translated  naming=inferred
// naming: no-op (empty key comparator initialisation). Descriptive name LocMap_CompareInit.
void LocMap_CompareInit(void *tag)
{
    (void)tag;
}

// 0x0813ae3e  size=47  FUN_0813ae3e  status=translated  naming=inferred
// naming: map initialisation: allocates the sentinel, sets the count to 0 and initialises the sentinel links. Descriptive name LocMap_Init.
void LocMap_Init(struct cod2_str_map_s *m, const void *compare_tag, const void *alloc_tag)
{
    LocMap_AllocHeader(m, alloc_tag);
    m->node_count = 0;
    LocMap_ResetHeader(m);
}

// 0x0813ae6e  size=48  FUN_0813ae6e  status=translated  naming=inferred
// naming: allocates the sentinel header (clears the pointer first, as the binary does). Descriptive name LocMap_AllocHeader.
void LocMap_AllocHeader(struct cod2_str_map_s *m, const void *alloc_tag)
{
    LocMap_ResetHeaderPtr(m, alloc_tag);
    m->header = (struct cod2_rb_node_s *)LocNode_AllocHeader(m);
}

// 0x0813ae9e  size=36  FUN_0813ae9e  status=translated  naming=inferred
// naming: clears the sentinel pointer (the allocator no-op runs first). Descriptive name LocMap_ResetHeaderPtr.
void LocMap_ResetHeaderPtr(struct cod2_str_map_s *m, const void *alloc_tag)
{
    LocAlloc_Noop();
    m->header = NULL;
}

// 0x0813aec2  size=5  FUN_0813aec2  status=translated  naming=inferred
// naming: no-op. Descriptive name LocAlloc_Noop.
void LocAlloc_Noop(void)
{
}

// 0x0813aec8  size=27  FUN_0813aec8  status=translated  naming=inferred
// naming: allocates one node (0x18 bytes) for the sentinel. Descriptive name LocNode_AllocHeader.
void *LocNode_AllocHeader(const void *alloc_tag)
{
    return LocNode_AllocArray(alloc_tag, 1);
}

// 0x0813aee4  size=28  FUN_0813aee4  status=translated  naming=inferred
// naming: malloc(count * 0x18): the node array allocation. Descriptive name LocNode_AllocArray.
void *LocNode_AllocArray(const void *alloc_tag, int32 count)
{
    (void)alloc_tag;
    return malloc((size_t)count * 0x18);
}

// 0x0813af00  size=87  FUN_0813af00  status=translated  naming=inferred
// naming: resets the sentinel: colour red, no root, left and right point at the sentinel itself. Descriptive name LocMap_ResetHeader.
void LocMap_ResetHeader(struct cod2_str_map_s *m)
{
    struct cod2_rb_node_s *header = (struct cod2_rb_node_s *)LocNode_Identity(m->header);
    header->color = 0;
    header->parent = NULL;
    header->left = header;
    header->right = header;
}

// 0x0813af58  size=8  FUN_0813af58  status=translated  naming=inferred
// naming: identity on a pointer argument. Descriptive name LocNode_Identity.
void *LocNode_Identity(void *p)
{
    return p;
}

// 0x0813af60  size=11  FUN_0813af60  status=translated  naming=inferred
// naming: the left child link of a node (returns the address of the link). Descriptive name LocNode_Left.
struct cod2_rb_node_s **LocNode_Left(struct cod2_rb_node_s *n)
{
    return &n->left;
}

// 0x0813af6c  size=40  FUN_0813af6c  status=translated  naming=inferred
// naming: destroys a node: its pair first, then frees the node. Descriptive name LocMap_DestroyNode.
void LocMap_DestroyNode(struct cod2_str_map_s *m, struct cod2_rb_node_s *n)
{
    LocPair_DestroyAt(&n->key);
    LocMap_FreeNode(m, n);
}

// 0x0813af94  size=29  FUN_0813af94  status=translated  naming=inferred
// naming: key comparator call (comparator object is empty); returns a < b through the string comparison. Descriptive name LocMap_KeyLess.
uint8 LocMap_KeyLess(const void *comparator, const struct cod2_string_s *a, const struct cod2_string_s *b)
{
    (void)comparator;
    return (uint8)LocKey_IsLess(a, b);
}

// 0x0813afb2  size=29  FUN_0813afb2  status=translated  naming=inferred
// naming: string less-than: compare(a, b) is negative (the sign bit of the result). Descriptive name LocKey_IsLess.
uint32 LocKey_IsLess(const struct cod2_string_s *a, const struct cod2_string_s *b)
{
    int result = cod2_string_compare(a, b);
    return ((uint32)result) >> 31;
}

// 0x0813afd0  size=38  FUN_0813afd0  status=translated  naming=inferred
// naming: end() iterator (the sentinel header). Descriptive name LocMap_EndIter.
struct cod2_rb_node_s *LocMap_EndIter(struct cod2_str_map_s *m)
{
    return m->header;
}

// 0x0813aff6  size=274  FUN_0813aff6  status=translated  naming=inferred
// naming: map find: lower bound, then an equality test (not less either way). Returns end() when absent. Descriptive name LocRb_Find.
struct cod2_rb_node_s *LocRb_Find(struct cod2_str_map_s *m, const struct cod2_string_s *key)
{
    struct cod2_rb_node_s *y = m->header;
    struct cod2_rb_node_s *x = *LocMap_Root(m);

    while (x != NULL) {
        if (LocMap_KeyLess(&m->key_compare, &x->key, key) == 0) {
            y = x;
            x = *LocNode_Left(x);
        } else {
            x = *LocNode_RightAlt(x);
        }
    }
    if (y != LocMap_EndIter(m)
        && LocMap_KeyLess(&m->key_compare, key, &y->key) == 0) {
        return y;
    }
    return LocMap_EndIter(m);
}

// 0x0813b108  size=44  FUN_0813b108  status=translated  naming=inferred
// naming: wrapper of LocRb_LowerBound. Descriptive name LocRb_LowerBoundWrap.
struct cod2_rb_node_s *LocRb_LowerBoundWrap(struct cod2_str_map_s *m, const struct cod2_string_s *key)
{
    return LocRb_LowerBound(m, key);
}

// 0x0813b134  size=148  FUN_0813b134  status=translated  naming=inferred
// naming: lower_bound: first node whose key is not less than key (end() when none). Descriptive name LocRb_LowerBound.
struct cod2_rb_node_s *LocRb_LowerBound(struct cod2_str_map_s *m, const struct cod2_string_s *key)
{
    struct cod2_rb_node_s *y = m->header;
    struct cod2_rb_node_s *x = *LocMap_Root(m);

    while (x != NULL) {
        if (LocMap_KeyLess(&m->key_compare, &x->key, key) == 0) {
            y = x;
            x = *LocNode_Left(x);
        } else {
            x = *LocNode_RightAlt(x);
        }
    }
    return y;
}

// 0x0813b1c8  size=13  FUN_0813b1c8  status=translated  naming=inferred
// naming: address of the key slot of a node (node + 0x10). Descriptive name LocIter_KeyAddr.
struct cod2_string_s *LocIter_KeyAddr(struct cod2_rb_node_s **it)
{
    return &(*it)->key;
}

// 0x0813b1d6  size=37  FUN_0813b1d6  status=translated  naming=inferred
// naming: returns its first argument; the second is only forwarded to an identity helper. Descriptive name LocIter_Passthrough2.
void *LocIter_Passthrough2(void *dst, const void *src)
{
    (void)src;
    return LocIter_Identity(dst);
}

// 0x0813b1fc  size=10  FUN_0813b1fc  status=translated  naming=inferred
// naming: identity on a pointer argument. Descriptive name LocIter_Identity.
void *LocIter_Identity(void *p)
{
    return p;
}

// 0x0813b206  size=48  FUN_0813b206  status=translated  naming=inferred
// naming: value_type constructor: copies key into pair[0] and value into pair[1]. Descriptive name LocPair_Init.
void LocPair_Init(struct cod2_string_s *pair, const struct cod2_string_s *key, const struct cod2_string_s *value)
{
    cod2_string_init_copy(&pair[0], key);
    cod2_string_init_copy(&pair[1], value);
}

// 0x0813b25c  size=720  FUN_0813b25c  status=translated  naming=inferred
// naming: _M_insert_unique with a hint (libstdc++ 3.x shape): checks the hint against its neighbours; inserts at the leftmost, rightmost or hint position when the key fits, otherwise falls back to LocMap_InsertUnique. Descriptive name LocRb_InsertHint.
struct cod2_rb_node_s *LocRb_InsertHint(struct cod2_str_map_s *m, struct cod2_rb_node_s *hint, const struct cod2_string_s *pair)
{
    struct cod2_rb_node_s *header = m->header;
    struct cod2_rb_node_s *result = NULL;
    struct cod2_rb_insert_s ins;

    if (hint == *LocMap_Leftmost(m)) {
        if (RbNode_Left(m) != 0) {
            if (LocMap_KeyLess(&m->key_compare, pair, &hint->key) != 0) {
                RbTree_InsertWithHint(&result, m, hint, hint, (struct rb_pair_s *)pair);
                return result;
            }
        }
        LocMap_InsertUnique(&ins, m, pair);
        return ins.first;
    }

    if (hint == header) {
        struct cod2_rb_node_s *rightmost = *LocMap_Rightmost(m);
        if (LocMap_KeyLess(&m->key_compare, &rightmost->key, pair) != 0) {
            RbTree_InsertWithHint(&result, m, NULL, rightmost, (struct rb_pair_s *)pair);
            return result;
        }
        LocMap_InsertUnique(&ins, m, pair);
        return ins.first;
    }

    {
        struct cod2_rb_node_s *before = LocRb_Decrement(LocIter_Copy(hint));
        uint8 use_hint = 0;

        if (LocMap_KeyLess(&m->key_compare, &before->key, pair) != 0) {
            if (LocMap_KeyLess(&m->key_compare, pair, &hint->key) != 0) {
                use_hint = 1;
            }
        }
        if (use_hint != 0) {
            if (before->right == NULL) {
                RbTree_InsertWithHint(&result, m, NULL, before, (struct rb_pair_s *)pair);
            } else {
                RbTree_InsertWithHint(&result, m, hint, hint, (struct rb_pair_s *)pair);
            }
            return result;
        }
        LocMap_InsertUnique(&ins, m, pair);
        return ins.first;
    }
}

// 0x0813b52c  size=481  FUN_0813b52c  status=translated  naming=inferred
// naming: _M_insert_unique(pair): walks to the insertion point, detects an existing key (returns it with second = 0) or inserts a node and returns it with second = 1. Descriptive name LocMap_InsertUnique.
void LocMap_InsertUnique(struct cod2_rb_insert_s *ret, struct cod2_str_map_s *m, const struct cod2_string_s *pair)
{
    struct cod2_rb_node_s *y = m->header;
    struct cod2_rb_node_s *x = *LocMap_Root(m);
    uint8 comp = 1;
    struct cod2_rb_node_s *j;
    struct cod2_rb_node_s *inserted = NULL;

    while (x != NULL) {
        y = x;
        comp = (uint8)(LocMap_KeyLess(&m->key_compare, pair, &x->key) != 0);
        x = comp ? *LocNode_Left(x) : *LocNode_RightAlt(x);
    }

    j = y;
    if (comp != 0) {
        if (j == *LocMap_Leftmost(m)) {           /* j == begin(): insert before it */
            RbTree_InsertWithHint(&inserted, m, x, y, (struct rb_pair_s *)pair);
            ret->first = inserted;
            ret->second = 1;
            return;
        }
        j = LocRb_Decrement(j);                   /* --j (LocRb_DecrementAt in the binary) */
    }

    if (LocMap_KeyLess(&m->key_compare, &j->key, pair) != 0) {
        RbTree_InsertWithHint(&inserted, m, x, y, (struct rb_pair_s *)pair);
        ret->first = inserted;
        ret->second = 1;
        return;
    }
    ret->first = j;
    ret->second = 0;
}

// 0x0813b70e  size=22  FUN_0813b70e  status=translated  naming=inferred
// naming: wrapper that decrements an iterator. Descriptive name LocRb_DecrementAt.
struct cod2_rb_node_s *LocRb_DecrementAt(struct cod2_rb_node_s *x)
{
    return LocRb_Decrement(x);
}

// 0x0813b724  size=11  FUN_0813b724  status=translated  naming=inferred
// naming: the right child link of a node (node + 0x0c, returns the address of the link). Descriptive name LocNode_RightLink.
struct cod2_rb_node_s **LocNode_RightLink(struct cod2_rb_node_s *n)
{
    return &n->right;
}

// 0x0813b730  size=5  FUN_0813b730  status=translated  naming=inferred
// naming: no-op (iterator assignment prelude). Descriptive name LocIter_NoopPrep.
void LocIter_NoopPrep(void)
{
}

// 0x0813b736  size=158  FUN_0813b736  status=translated  naming=inferred
// naming: _Rb_tree_decrement: header maps to the rightmost node; a node without a left child goes to the first ancestor it is a right child of; otherwise the rightmost node of the left subtree. Descriptive name LocRb_Decrement.
struct cod2_rb_node_s *LocRb_Decrement(struct cod2_rb_node_s *x)
{
    if (x->color == 0 && x->parent->parent == x) {
        return x->right;
    }
    if (x->left == NULL) {
        struct cod2_rb_node_s *y = x->parent;
        while (x == y->left) {
            x = y;
            y = y->parent;
        }
        return y;
    }
    {
        struct cod2_rb_node_s *y = x->left;
        while (y->right != NULL) {
            y = y->right;
        }
        return y;
    }
}

// 0x0813b7d4  size=19  FUN_0813b7d4  status=translated  naming=inferred
// naming: destroys the map (wrapper of LocMap_Destroy). Descriptive name LocMap_DestroyMap.
void LocMap_DestroyMap(struct cod2_str_map_s *m)
{
    LocMap_Destroy(m);
}

// 0x0813b7e8  size=11  FUN_0813b7e8  status=translated  naming=inferred
// naming: the right child link of a node (node + 0x0c, returns the address of the link), used by the recursive erase and find. Descriptive name LocNode_RightAlt.
struct cod2_rb_node_s **LocNode_RightAlt(struct cod2_rb_node_s *n)
{
    return &n->right;
}

// 0x0813b7f4  size=34  FUN_0813b7f4  status=translated  naming=inferred
// naming: frees a node through LocNode_Free. Descriptive name LocMap_FreeNode.
void LocMap_FreeNode(struct cod2_str_map_s *m, struct cod2_rb_node_s *n)
{
    LocNode_Free(m, n);
}

// 0x0813b816  size=19  FUN_0813b816  status=translated  naming=inferred
// naming: libc free of the node. Descriptive name LocNode_Free.
void LocNode_Free(const void *unused, void *node)
{
    (void)unused;
    free(node);
}

// 0x0813b82a  size=20  FUN_0813b82a  status=translated  naming=inferred
// naming: clears the map, then releases the sentinel header. Descriptive name LocMap_Destroy.
void LocMap_Destroy(struct cod2_str_map_s *m)
{
    LocMap_Clear(m);
    RbTree_Clear(m);
}

// 0x0813b86e  size=19  FUN_0813b86e  status=translated  naming=inferred
// naming: wrapper that destroys a pair. Descriptive name LocPair_DestroyAt.
void LocPair_DestroyAt(struct cod2_string_s *pair)
{
    LocPair_Destroy(pair);
}

// 0x0813b882  size=34  FUN_0813b882  status=translated  naming=inferred
// naming: key of a node (node + 0x10 through identity). Descriptive name LocNode_KeyOf.
struct cod2_string_s *LocNode_KeyOf(struct cod2_rb_node_s *n)
{
    return (struct cod2_string_s *)LocIter_Identity2(NULL, LocNode_KeyAddr(n));
}

// 0x0813b8a4  size=11  FUN_0813b8a4  status=translated  naming=inferred
// naming: address of the key slot (node + 0x10). Descriptive name LocNode_KeyAddr.
struct cod2_string_s *LocNode_KeyAddr(struct cod2_rb_node_s *n)
{
    return &n->key;
}

// 0x0813b8b0  size=8  FUN_0813b8b0  status=translated  naming=inferred
// naming: returns its second argument. Descriptive name LocIter_Identity2.
void *LocIter_Identity2(void *unused, void *p)
{
    (void)unused;
    return p;
}

// 0x0813b8b8  size=27  FUN_0813b8b8  status=translated  naming=inferred
// naming: iterator assignment from a node (no-op prelude, then the node). Descriptive name LocIter_FromNode.
struct cod2_rb_node_s *LocIter_FromNode(struct cod2_rb_node_s **dst, struct cod2_rb_node_s *node)
{
    LocIter_NoopPrep();
    *dst = node;
    return node;
}

// 0x0813b8d4  size=34  FUN_0813b8d4  status=translated  naming=inferred
// naming: key of a node reached through an iterator (same shape as LocNode_KeyOf). Descriptive name LocNode_KeyFromIter.
struct cod2_string_s *LocNode_KeyFromIter(struct cod2_rb_node_s *it)
{
    return (struct cod2_string_s *)LocIter_Identity2(NULL, LocNode_KeyAddr(it));
}

// 0x0813b8f6  size=8  FUN_0813b8f6  status=translated  naming=inferred
// naming: returns its second argument (the search key, forwarded). Descriptive name LocKey_Passthrough.
const struct cod2_string_s *LocKey_Passthrough(const void *unused, const struct cod2_string_s *key)
{
    (void)unused;
    return key;
}
