#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include <time.h>
#include "chunk_001_types.h"

/* chunk_001: id Tech 3 style botlib C preprocessor (l_precomp equivalent) plus the
 * adjoining CRT helpers. Names for the preprocessor functions are inferred from the
 * id Tech 3 counterpart (matching error strings, token/define layouts and call
 * structure); no function-naming strings exist in the binary. */

/* CRT helpers (crtbegin.o __do_global_dtors_aux / frame_dummy): provided by the system
 * C runtime in the linked build; kept as documentation. */
#ifdef COD2_RECON_ORIGINAL_CRT
extern void (**g_0817d3a8)(void);
// 0x0804a710  size=62  FUN_0804a710  status=translated  naming=inferred
// CRT __do_global_dtors_aux: guard byte, walks the .dtors list through a cursor variable.
void __do_global_dtors_aux(void)
{
    void (*fn)(void);

    if (g_08188c68 == 0) {
        g_08188c68 = 0;
        fn = *g_0817d3a8;
        while (fn != 0) {
            g_0817d3a8 = g_0817d3a8 + 1;
            fn();
            fn = *g_0817d3a8;
        }
        g_08188c68 = 1;
    }
}

// 0x0804a750  size=52  FUN_0804a750  status=translated  naming=inferred
// CRT frame_dummy: tests __JCR_LIST__[0] (0x0817d02c) and a weak, link-time-null
// _Jv_RegisterClasses (constant 0 in the disassembly), so the call is never made.
void frame_dummy(void)
{
}

#endif /* COD2_RECON_ORIGINAL_CRT */

// 0x0804a784  size=67  FUN_0804a784  status=translated  naming=inferred
// GetMemory: allocates size+4, stores the 0x12345678 id word, returns the pointer past it.
// Evidence: id word 0x12345678 matches the id Tech 3 botlib allocator header; 8 callers in this chunk.
void *GetMemory(int size)
{
    unsigned int *block;

    block = (unsigned int *)Com_MallocZeroed(size + 4);
    if (block == 0) {
        return 0;
    }
    *block = 0x12345678;
    return block + 1;
}

// 0x0804a7c8  size=51  FUN_0804a7c8  status=translated  naming=inferred
// GetClearedMemory: GetMemory + memset 0 (NULL not checked, as in the binary).
void *GetClearedMemory(size_t size)
{
    void *ptr;

    ptr = GetMemory(size);
    memset(ptr, 0, size);
    return ptr;
}

// 0x0804a7fc  size=39  FUN_0804a7fc  status=translated  naming=inferred
// FreeMemory: releases only blocks carrying the 0x12345678 id word.
void FreeMemory(void *ptr)
{
    if (*((int *)ptr - 1) == 0x12345678) {
        Com_Free_A((int *)ptr - 1);
    }
}

// 0x0804a824  size=110  FUN_0804a824  status=translated  naming=inferred
// PC_SourceError: string "^1Error: file %s, line %d: %s\n" prints script filename and line.
void PC_SourceError(source_t *source, const char *fmt, ...)
{
    char text[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    Com_Printf("^1Error: file %s, line %d: %s\n", source->scriptstack->filename,
                source->scriptstack->line, text);
}

// 0x0804a892  size=110  FUN_0804a892  status=translated  naming=inferred
// PC_SourceWarning: string "^3Warning: file %s, line %d: %s\n".
void PC_SourceWarning(source_t *source, const char *fmt, ...)
{
    char text[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    Com_Printf("^3Warning: file %s, line %d: %s\n", source->scriptstack->filename,
                source->scriptstack->line, text);
}

// 0x0804a900  size=113  FUN_0804a900  status=translated  naming=inferred
// PC_PushIndent: pushes an indent_t (type, skip, current script) and accumulates source->skip.
void PC_PushIndent(source_t *source, int type, int skip)
{
    indent_t *indent;

    indent = (indent_t *)GetMemory(sizeof(indent_t));
    indent->type = type;
    indent->script = source->scriptstack;
    indent->skip = (skip != 0);
    source->skip += indent->skip;
    indent->next = source->indentstack;
    source->indentstack = indent;
}

// 0x0804a972  size=155  FUN_0804a972  status=translated  naming=inferred
// PC_PopIndent: pops the top indent only if it belongs to the current script.
void PC_PopIndent(source_t *source, int *type, int *skip)
{
    indent_t *indent;

    *type = 0;
    *skip = 0;
    indent = source->indentstack;
    if (indent != 0 && indent->script == source->scriptstack) {
        *type = indent->type;
        *skip = indent->skip;
        source->indentstack = source->indentstack->next;
        source->skip -= indent->skip;
        FreeMemory(indent);
    }
}

// 0x0804aa0e  size=122  FUN_0804aa0e  status=translated  naming=inferred
// PC_PushScript: string "%s recursively included"; rejects a script whose filename already
// appears on the stack (compare via out-of-chunk Q_stricmp, 0 = equal).
void PC_PushScript(source_t *source, script_t *script)
{
    script_t *s;

    for (s = source->scriptstack; s != 0; s = s->next) {
        if (Q_stricmp(s->filename, script->filename) == 0) {
            PC_SourceError(source, "%s recursively included", script->filename);
            return;
        }
    }
    script->next = source->scriptstack;
    source->scriptstack = script;
}

// 0x0804aa88  size=112  FUN_0804aa88  status=translated  naming=inferred
// PC_CopyToken: string "EXE_ERR_OUT_OF_MEMORY" on allocation failure; clears next, bumps token count.
token_t *PC_CopyToken(token_t *t)
{
    token_t *copy;

    copy = (token_t *)GetMemory(sizeof(token_t));
    if (copy == 0) {
        Com_Error(0, "EXE_ERR_OUT_OF_MEMORY");
        return 0;
    }
    memcpy(copy, t, sizeof(token_t));
    copy->next = 0;
    pc_numtokens++;
    return copy;
}

// 0x0804aaf8  size=25  FUN_0804aaf8  status=translated  naming=inferred
// PC_FreeToken: frees and decrements the live token count.
void PC_FreeToken(token_t *token)
{
    FreeMemory(token);
    pc_numtokens--;
}

// 0x0804ab12  size=341  FUN_0804ab12  status=translated  naming=inferred
// PC_ReadSourceToken: string "missing #endif"; pops pushed-back tokens first, then reads from the
// top script, popping finished scripts (and warning for unterminated indents).
int PC_ReadSourceToken(source_t *source, token_t *token)
{
    token_t *t;
    script_t *script;
    int type, skip;

    while (1) {
        if (source->tokens != 0) {
            t = source->tokens;
            memcpy(token, t, sizeof(token_t));
            source->tokens = source->tokens->next;
            PC_FreeToken(t);
            return 1;
        }
        if (PS_ReadToken(source->scriptstack, token) != 0) {
            return 1;
        }
        if (EndOfScript(source->scriptstack) != 0) {
            while (source->indentstack != 0 && source->indentstack->script == source->scriptstack) {
                PC_SourceWarning(source, "missing #endif");
                PC_PopIndent(source, &type, &skip);
            }
        }
        if (source->scriptstack->next == 0) {
            return 0;
        }
        script = source->scriptstack;
        source->scriptstack = source->scriptstack->next;
        FreeScript(script);
    }
}

// 0x0804ac68  size=57  FUN_0804ac68  status=translated  naming=inferred
// PC_UnreadSourceToken: pushes a copy of the token onto the front of source->tokens.
int PC_UnreadSourceToken(source_t *source, token_t *token)
{
    token_t *t;

    t = PC_CopyToken(token);
    t->next = source->tokens;
    source->tokens = t;
    return 1;
}

// 0x0804aca2  size=935  FUN_0804aca2  status=translated  naming=inferred
// PC_ReadDefineParms: strings "define %s missing parms", "define with more than %d parameters",
// "define %s with too many parms", "define %s has too many parms", "define %s incomplete",
// "too many comma's", "too few define parms". Collects macro-call arguments into token lists.
int PC_ReadDefineParms(source_t *source, define_t *define, token_t **parms, int maxparms)
{
    token_t token;
    token_t *t, *last;
    int i, done, lastcomma, numparms, indent;

    if (!PC_ReadSourceToken(source, &token)) {
        PC_SourceError(source, "define %s missing parms", define->name);
        return 0;
    }
    if (define->numparms > maxparms) {
        PC_SourceError(source, "define with more than %d parameters", maxparms);
        return 0;
    }
    for (i = 0; i < define->numparms; i++) {
        parms[i] = 0;
    }
    if (strcmp(token.string, "(") != 0) {
        PC_UnreadSourceToken(source, &token);
        PC_SourceError(source, "define %s missing parms", define->name);
        return 0;
    }
    done = 0;
    numparms = 0;
    indent = 0;
    while (!done) {
        if (numparms >= maxparms) {
            PC_SourceError(source, "define %s with too many parms", define->name);
            return 0;
        }
        if (numparms >= define->numparms) {
            PC_SourceWarning(source, "define %s has too many parms", define->name);
            return 0;
        }
        parms[numparms] = 0;
        lastcomma = 1;
        last = 0;
        while (1) {
            if (!PC_ReadSourceToken(source, &token)) {
                PC_SourceError(source, "define %s incomplete", define->name);
                return 0;
            }
            if (strcmp(token.string, ",") == 0 && indent < 1) {
                if (lastcomma) {
                    PC_SourceWarning(source, "too many comma's");
                }
                break;
            }
            lastcomma = 0;
            if (strcmp(token.string, "(") == 0) {
                indent++;
                continue;
            }
            if (strcmp(token.string, ")") == 0) {
                indent--;
                if (indent < 1) {
                    if (parms[define->numparms - 1] == 0) {
                        PC_SourceWarning(source, "too few define parms");
                    }
                    done = 1;
                    break;
                }
            }
            if (numparms < define->numparms) {
                t = PC_CopyToken(&token);
                t->next = 0;
                if (last == 0) {
                    parms[numparms] = t;
                } else {
                    last->next = t;
                }
                last = t;
            }
        }
        numparms++;
    }
    return 1;
}

// 0x0804b04a  size=190  FUN_0804b04a  status=translated  naming=inferred
// PC_StringizeTokens: strings "\"" used as quotes; builds a TT_STRING token "\"<tokens>\"" (the #
// operator). Always returns 1; the 1024-char buffer is bounded via strncat.
int PC_StringizeTokens(token_t *tokens, token_t *token)
{
    token_t *t;

    token->type = PC_TT_STRING;
    token->whitespace_p = 0;
    token->endwhitespace_p = 0;
    token->string[0] = '\0';
    strcat(token->string, "\"");
    for (t = tokens; t != 0; t = t->next) {
        strncat(token->string, t->string, PC_MAX_TOKEN_CHARS - strlen(token->string));
    }
    strncat(token->string, "\"", PC_MAX_TOKEN_CHARS - strlen(token->string));
    return 1;
}

// 0x0804b108  size=159  FUN_0804b108  status=translated  naming=inferred
// PC_MergeTokens: the ## operator; name+name/number concatenates, string+string drops the
// joining quotes. No bounds check on strcat (as in the binary).
int PC_MergeTokens(token_t *t1, token_t *t2)
{
    if (t1->type == PC_TT_NAME && (t2->type == PC_TT_NAME || t2->type == PC_TT_NUMBER)) {
        strcat(t1->string, t2->string);
        return 1;
    }
    if (t1->type == PC_TT_STRING && t2->type == PC_TT_STRING) {
        t1->string[strlen(t1->string) - 1] = '\0';
        strcat(t1->string, t2->string + 1);
        return 1;
    }
    return 0;
}

// 0x0804b1a8  size=95  FUN_0804b1a8  status=translated  naming=inferred
// PC_NameHash: sum of (i+0x77)*(signed char) folded with >>10 and >>20 (arithmetic), masked to 1024.
unsigned int PC_NameHash(const char *name)
{
    int i;
    int hash;

    hash = 0;
    for (i = 0; name[i] != '\0'; i++) {
        hash += (i + 0x77) * (int)name[i];
    }
    return (unsigned int)((hash ^ (hash >> 10) ^ (hash >> 20)) & (PC_DEFINEHASHSIZE - 1));
}

// 0x0804b208  size=65  FUN_0804b208  status=translated  naming=inferred
// PC_AddDefineToHash: pushes define on the front of its hash bucket chain (hashnext).
void PC_AddDefineToHash(define_t *define, define_t **definehash)
{
    int hash;

    hash = PC_NameHash(define->name);
    define->hashnext = definehash[hash];
    definehash[hash] = define;
}

// 0x0804b24a  size=102  FUN_0804b24a  status=translated  naming=inferred
// PC_FindHashedDefine: strcmp walk of one bucket via hashnext.
define_t *PC_FindHashedDefine(define_t **definehash, const char *name)
{
    define_t *d;
    int hash;

    hash = PC_NameHash(name);
    for (d = definehash[hash]; d != 0; d = d->hashnext) {
        if (strcmp(d->name, name) == 0) {
            return d;
        }
    }
    return 0;
}

// 0x0804b2b0  size=75  FUN_0804b2b0  status=translated  naming=inferred
// PC_FindDefine: linear strcmp search along the next chain.
define_t *PC_FindDefine(define_t *defines, const char *name)
{
    define_t *d;

    for (d = defines; d != 0; d = d->next) {
        if (strcmp(d->name, name) == 0) {
            return d;
        }
    }
    return 0;
}

// 0x0804b2fc  size=91  FUN_0804b2fc  status=translated  naming=inferred
// PC_FindDefineParm: index of a named parameter in define->parms, or -1.
int PC_FindDefineParm(define_t *define, const char *name)
{
    token_t *p;
    int i;

    i = 0;
    for (p = define->parms; p != 0; p = p->next) {
        if (strcmp(p->string, name) == 0) {
            return i;
        }
        i++;
    }
    return -1;
}

// 0x0804b358  size=115  FUN_0804b358  status=translated  naming=inferred
// PC_FreeDefine: frees parm tokens, body tokens, then the define.
void PC_FreeDefine(define_t *define)
{
    token_t *t, *next;

    for (t = define->parms; t != 0; t = next) {
        next = t->next;
        PC_FreeToken(t);
    }
    for (t = define->tokens; t != 0; t = next) {
        next = t->next;
        PC_FreeToken(t);
    }
    FreeMemory(define);
}

// 0x0804b4ca  size=630  FUN_0804b4ca  status=translated  naming=inferred
// PC_ExpandBuiltinDefine: __LINE__/__FILE__/__DATE__/__TIME__ (builtin 1..4); strings "%d".
// Date/time cases free() the pointer returned by ctime(), as the binary does (observed call to free).
int PC_ExpandBuiltinDefine(source_t *source, token_t *deftoken, define_t *define,
                           token_t **firsttoken, token_t **lasttoken)
{
    token_t *token;
    time_t t;
    char *curtime;

    token = PC_CopyToken(deftoken);
    switch (define->builtin) {
    default:
        *firsttoken = 0;
        *lasttoken = 0;
        break;
    case PC_BUILTIN_LINE:
        sprintf(token->string, "%d", deftoken->line);
        token->intvalue = deftoken->line;
        token->floatvalue = (long double)deftoken->line;
        token->type = PC_TT_NUMBER;
        token->subtype = PC_TT_DECIMAL | PC_TT_INTEGER;
        *firsttoken = token;
        *lasttoken = token;
        break;
    case PC_BUILTIN_FILE:
        strcpy(token->string, source->scriptstack->filename);
        token->type = PC_TT_NAME;
        token->subtype = (int)strlen(token->string);
        *firsttoken = token;
        *lasttoken = token;
        break;
    case PC_BUILTIN_DATE:
        t = time(0);
        curtime = ctime(&t);
        strcpy(token->string, "\"");
        strncat(token->string, curtime + 4, 7);
        strncat(token->string + 7, curtime + 20, 4);
        strcat(token->string, "\"");
        free(curtime);
        token->type = PC_TT_NAME;
        token->subtype = (int)strlen(token->string);
        *firsttoken = token;
        *lasttoken = token;
        break;
    case PC_BUILTIN_TIME:
        t = time(0);
        curtime = ctime(&t);
        strcpy(token->string, "\"");
        strncat(token->string, curtime + 11, 8);
        strcat(token->string, "\"");
        free(curtime);
        token->type = PC_TT_NAME;
        token->subtype = (int)strlen(token->string);
        *firsttoken = token;
        *lasttoken = token;
        break;
    }
    return 1;
}

// 0x0804b740  size=1210  FUN_0804b740  status=translated  naming=inferred
// PC_ExpandDefine: strings "stringizing operator without define parameter", "can't stringize
// tokens", "can't merge %s with %s". Substitutes macro-call parameters, handles # and ##.
int PC_ExpandDefine(source_t *source, token_t *deftoken, define_t *define,
                    token_t **firsttoken, token_t **lasttoken)
{
    token_t *parms[PC_MAX_DEFINEPARMS];
    token_t *dt, *pt, *t, *t1, *t2, *first, *last, *nextpt;
    token_t token;
    int parmnum, i;

    if (define->builtin != 0) {
        return PC_ExpandBuiltinDefine(source, deftoken, define, firsttoken, lasttoken);
    }
    if (define->numparms != 0) {
        if (!PC_ReadDefineParms(source, define, parms, PC_MAX_DEFINEPARMS)) {
            return 0;
        }
    }
    first = 0;
    last = 0;
    for (dt = define->tokens; dt != 0; dt = dt->next) {
        parmnum = -1;
        if (dt->type == PC_TT_NAME) {
            parmnum = PC_FindDefineParm(define, dt->string);
        }
        if (parmnum >= 0) {
            for (pt = parms[parmnum]; pt != 0; pt = pt->next) {
                t = PC_CopyToken(pt);
                t->next = 0;
                if (last == 0) {
                    first = t;
                } else {
                    last->next = t;
                }
                last = t;
            }
        } else {
            if (dt->string[0] == '#' && dt->string[1] == '\0') {
                if (dt->next == 0) {
                    parmnum = -1;
                } else {
                    parmnum = PC_FindDefineParm(define, dt->next->string);
                }
                if (parmnum < 0) {
                    PC_SourceWarning(source, "stringizing operator without define parameter");
                    continue;
                }
                dt = dt->next;
                if (!PC_StringizeTokens(parms[parmnum], &token)) {
                    PC_SourceError(source, "can't stringize tokens");
                    return 0;
                }
                t = PC_CopyToken(&token);
            } else {
                t = PC_CopyToken(dt);
            }
            t->next = 0;
            if (last == 0) {
                first = t;
            } else {
                last->next = t;
            }
            last = t;
        }
    }
    /* resolve ## token pasting */
    t = first;
    while (t != 0) {
        if (t->next != 0 && t->next->string[0] == '#' && t->next->string[1] == '#') {
            t1 = t;
            t2 = t->next->next;
            if (t2 != 0) {
                if (!PC_MergeTokens(t1, t2)) {
                    PC_SourceError(source, "can't merge %s with %s", t1->string, t2->string);
                    return 0;
                }
                PC_FreeToken(t1->next);
                t1->next = t2->next;
                if (t2 == last) {
                    last = t1;
                }
                PC_FreeToken(t2);
                continue;
            }
        }
        t = t->next;
    }
    *firsttoken = first;
    *lasttoken = last;
    for (i = 0; i < define->numparms; i++) {
        pt = parms[i];
        while (pt != 0) {
            nextpt = pt->next;
            PC_FreeToken(pt);
            pt = nextpt;
        }
    }
    return 1;
}

// 0x0804bbfa  size=121  FUN_0804bbfa  status=translated  naming=inferred
// PC_ExpandDefineIntoSource: expands, then pushes the result list onto source->tokens.
int PC_ExpandDefineIntoSource(source_t *source, token_t *deftoken, define_t *define)
{
    token_t *firsttoken, *lasttoken;

    if (!PC_ExpandDefine(source, deftoken, define, &firsttoken, &lasttoken)) {
        return 0;
    }
    if (firsttoken == 0 || lasttoken == 0) {
        return 0;
    }
    lasttoken->next = source->tokens;
    source->tokens = firsttoken;
    return 1;
}

// 0x0804bc74  size=137  FUN_0804bc74  status=translated  naming=inferred
// PC_ConvertPath: collapses doubled path separators, then maps every \ or / to '/'.
void PC_ConvertPath(char *path)
{
    char *ptr;

    ptr = path;
    while (*ptr != '\0') {
        if ((*ptr == '\\' || *ptr == '/') && (ptr[1] == '\\' || ptr[1] == '/')) {
            strcpy(ptr, ptr + 1);
        } else {
            ptr++;
        }
    }
    for (ptr = path; *ptr != '\0'; ptr++) {
        if (*ptr == '/' || *ptr == '\\') {
            *ptr = '/';
        }
    }
}

// 0x0804bf7c  size=251  FUN_0804bf7c  status=translated  naming=inferred
// PC_ReadLine: reads a token that must not cross a line (unless continued by a "\" token),
// expanding defines in place. Name from the id Tech 3 counterpart; the "\\" continuation string
// is the observed anchor.
int PC_ReadLine(source_t *source, token_t *token)
{
    int crossline;
    define_t *define;

    crossline = 0;
    do {
        if (!PC_ReadSourceToken(source, token)) {
            return 0;
        }
        if (token->linescrossed > crossline) {
            PC_UnreadSourceToken(source, token);
            return 0;
        }
        if (token->type == PC_TT_NAME) {
            define = PC_FindHashedDefine(source->definehash, token->string);
            if (define != 0) {
                if (!PC_ExpandDefineIntoSource(source, token, define)) {
                    return 0;
                }
                if (!PC_ReadSourceToken(source, token)) {
                    return 0;
                }
            }
        }
        crossline = 1;
    } while (strcmp(token->string, "\\") == 0);
    return 1;
}

// 0x0804c078  size=47  FUN_0804c078  status=translated  naming=inferred
// PC_WhiteSpaceBeforeToken: true when whitespace text lies between whitespace_p and endwhitespace_p.
int PC_WhiteSpaceBeforeToken(token_t *token)
{
    return (token->endwhitespace_p - token->whitespace_p) > 0;
}

// 0x0804c0a8  size=44  FUN_0804c0a8  status=translated  naming=inferred
// PC_ClearTokenWhiteSpace: zeroes whitespace pointers and linescrossed.
void PC_ClearTokenWhiteSpace(token_t *token)
{
    token->whitespace_p = 0;
    token->endwhitespace_p = 0;
    token->linescrossed = 0;
}

// 0x0804c0d4  size=447  FUN_0804c0d4  status=translated  naming=inferred
// PC_Directive_undef: strings "undef without name", "can't undef %s", "expected name, found %s".
int PC_Directive_undef(source_t *source)
{
    token_t token;
    define_t *d, *lastdefine;
    int hash;

    if (source->skip > 0) {
        return 1;
    }
    if (!PC_ReadLine(source, &token)) {
        PC_SourceError(source, "undef without name");
        return 0;
    }
    if (token.type != PC_TT_NAME) {
        PC_UnreadSourceToken(source, &token);
        PC_SourceError(source, "expected name, found %s", token.string);
        return 0;
    }
    hash = PC_NameHash(token.string);
    lastdefine = 0;
    for (d = source->definehash[hash]; d != 0; d = d->hashnext) {
        if (strcmp(d->name, token.string) == 0) {
            if ((d->flags & PC_DEFINE_FIXED) == 0) {
                if (lastdefine == 0) {
                    source->definehash[hash] = d->hashnext;
                } else {
                    lastdefine->hashnext = d->hashnext;
                }
                PC_FreeDefine(d);
            } else {
                PC_SourceWarning(source, "can't undef %s", token.string);
            }
            break;
        }
        lastdefine = d;
    }
    return 1;
}

// 0x0804c294  size=1436  FUN_0804c294  status=translated  naming=inferred
// PC_Directive_define: strings "#define without name", "can't redefine %s", "redefinition of %s",
// "expected define parameter", "invalid define parameter", "two the same define parameters",
// "define parameters not terminated", "define not terminated", "recursive define (removed recursion)",
// "define with misplaced ##", "expected name after #define, found %s".
int PC_Directive_define(source_t *source)
{
    token_t token;
    token_t *t, *last;
    define_t *define;
    if (source->skip > 0) {
        return 1;
    }
    if (!PC_ReadLine(source, &token)) {
        PC_SourceError(source, "#define without name");
        return 0;
    }
    if (token.type != PC_TT_NAME) {
        PC_UnreadSourceToken(source, &token);
        PC_SourceError(source, "expected name after #define, found %s", token.string);
        return 0;
    }
    define = PC_FindHashedDefine(source->definehash, token.string);
    if (define != 0) {
        if ((define->flags & PC_DEFINE_FIXED) != 0) {
            PC_SourceError(source, "can't redefine %s", token.string);
            return 0;
        }
        PC_SourceWarning(source, "redefinition of %s", token.string);
        PC_UnreadSourceToken(source, &token);
        if (!PC_Directive_undef(source)) {
            return 0;
        }
        PC_FindHashedDefine(source->definehash, token.string);
    }
    define = (define_t *)GetMemory((int)(strlen(token.string) + sizeof(define_t) + 1));
    memset(define, 0, sizeof(define_t));
    define->name = (char *)define + sizeof(define_t);
    strcpy(define->name, token.string);
    PC_AddDefineToHash(define, source->definehash);
    if (!PC_ReadLine(source, &token)) {
        return 1;
    }
    if (!PC_WhiteSpaceBeforeToken(&token) && strcmp(token.string, "(") == 0) {
        last = 0;
        if (!PC_CheckTokenString(source, ")")) {
            while (1) {
                if (!PC_ReadLine(source, &token)) {
                    PC_SourceError(source, "expected define parameter");
                    return 0;
                }
                if (token.type != PC_TT_NAME) {
                    PC_SourceError(source, "invalid define parameter");
                    return 0;
                }
                if (PC_FindDefineParm(define, token.string) >= 0) {
                    PC_SourceError(source, "two the same define parameters");
                    return 0;
                }
                t = PC_CopyToken(&token);
                PC_ClearTokenWhiteSpace(t);
                t->next = 0;
                if (last == 0) {
                    define->parms = t;
                } else {
                    last->next = t;
                }
                define->numparms++;
                if (!PC_ReadLine(source, &token)) {
                    PC_SourceError(source, "define parameters not terminated");
                    return 0;
                }
                if (strcmp(token.string, ")") == 0) {
                    break;
                }
                last = t;
                if (strcmp(token.string, ",") != 0) {
                    PC_SourceError(source, "define not terminated");
                    return 0;
                }
            }
        }
        if (!PC_ReadLine(source, &token)) {
            return 1;
        }
    }
    last = 0;
    do {
        t = PC_CopyToken(&token);
        if (t->type == PC_TT_NAME && strcmp(t->string, define->name) == 0) {
            PC_SourceError(source, "recursive define (removed recursion)");
        } else {
            PC_ClearTokenWhiteSpace(t);
            t->next = 0;
            if (last == 0) {
                define->tokens = t;
            } else {
                last->next = t;
            }
            last = t;
        }
    } while (PC_ReadLine(source, &token));
    if (last != 0 && (strcmp(define->tokens->string, "##") == 0 || strcmp(last->string, "##") == 0)) {
        PC_SourceError(source, "define with misplaced ##");
        return 0;
    }
    return 1;
}

// 0x0804c830  size=410  FUN_0804c830  status=translated  naming=inferred
// PC_DefineFromString: strings "*extern" (twice); builds a throw-away source on the stack around an
// in-memory script, runs PC_Directive_define on it, returns the first define in the new hash table.
define_t *PC_DefineFromString(const char *string)
{
    script_t *script;
    source_t src;
    define_t *def;
    token_t *t, *nexttoken;
    int res, i;

    script = LoadScriptMemory(string, (int)strlen(string), "*extern");
    memset(&src, 0, sizeof(source_t));
    strncpy(src.filename, "*extern", PC_MAX_PATH);
    src.scriptstack = script;
    src.definehash = (define_t **)GetClearedMemory(PC_DEFINEHASHSIZE * sizeof(define_t *));
    res = PC_Directive_define(&src);
    for (t = src.tokens; t != 0; t = nexttoken) {
        nexttoken = t->next;
        PC_FreeToken(t);
    }
    def = 0;
    for (i = 0; i < PC_DEFINEHASHSIZE; i++) {
        if (src.definehash[i] != 0) {
            def = src.definehash[i];
            break;
        }
    }
    FreeMemory(src.definehash);
    FreeScript(script);
    if (res > 0) {
        return def;
    }
    if (src.defines != 0) {
        PC_FreeDefine((define_t *)src.defines);
    }
    return 0;
}

// 0x0804c9ca  size=71  FUN_0804c9ca  status=translated  naming=inferred
// PC_AddDefine: defines "name value" from a string into source's hash; true on success.
int PC_AddDefine(source_t *source, const char *string)
{
    define_t *define;

    define = PC_DefineFromString(string);
    if (define == 0) {
        return 0;
    }
    PC_AddDefineToHash(define, source->definehash);
    return 1;
}

// 0x0804cad4  size=347  FUN_0804cad4  status=translated  naming=inferred
// PC_CopyDefine: deep copy of a define (name stored inline, token chains cloned).
define_t *PC_CopyDefine(source_t *source, define_t *define)
{
    define_t *newdefine;
    token_t *t, *newt, *last;

    (void)source;
    newdefine = (define_t *)GetMemory((int)(strlen(define->name) + sizeof(define_t) + 1));
    newdefine->name = (char *)newdefine + sizeof(define_t);
    strcpy(newdefine->name, define->name);
    newdefine->flags = define->flags;
    newdefine->builtin = define->builtin;
    newdefine->numparms = define->numparms;
    newdefine->next = 0;
    newdefine->hashnext = 0;
    newdefine->tokens = 0;
    last = 0;
    for (t = define->tokens; t != 0; t = t->next) {
        newt = PC_CopyToken(t);
        newt->next = 0;
        if (last == 0) {
            newdefine->tokens = newt;
        } else {
            last->next = newt;
        }
        last = newt;
    }
    newdefine->parms = 0;
    last = 0;
    for (t = define->parms; t != 0; t = t->next) {
        newt = PC_CopyToken(t);
        newt->next = 0;
        if (last == 0) {
            newdefine->parms = newt;
        } else {
            last->next = newt;
        }
        last = newt;
    }
    return newdefine;
}

// 0x0804cc30  size=80  FUN_0804cc30  status=translated  naming=inferred
// PC_AddGlobalDefinesToSource: copies every global define into the source hash.
void PC_AddGlobalDefinesToSource(source_t *source)
{
    define_t *define, *newdefine;

    for (define = pc_globaldefines; define != 0; define = define->next) {
        newdefine = PC_CopyDefine(source, define);
        PC_AddDefineToHash(newdefine, source->definehash);
    }
}

// 0x0804cc80  size=246  FUN_0804cc80  status=translated  naming=inferred
// Shared #ifdef/#ifndef body taking the indent type: strings "#ifdef without name",
// "expected name after #ifdef, found %s". Skip = (undefined) != (type != IFDEF).
int PC_DirectiveIfdefCommon(source_t *source, int type)
{
    token_t token;
    define_t *d;

    if (!PC_ReadLine(source, &token)) {
        PC_SourceError(source, "#ifdef without name");
        return 0;
    }
    if (token.type != PC_TT_NAME) {
        PC_UnreadSourceToken(source, &token);
        PC_SourceError(source, "expected name after #ifdef, found %s", token.string);
        return 0;
    }
    d = PC_FindHashedDefine(source->definehash, token.string);
    PC_PushIndent(source, type, (d == 0) != (type != PC_INDENT_IFDEF));
    return 1;
}

// 0x0804cd76  size=27  FUN_0804cd76  status=translated  naming=inferred
// PC_Directive_ifdef: forwards indent type 8 (IFDEF).
int PC_Directive_ifdef(source_t *source)
{
    return PC_DirectiveIfdefCommon(source, PC_INDENT_IFDEF);
}

// 0x0804ce8c  size=271  FUN_0804ce8c  status=translated  naming=inferred
// PC_OperatorPriority: precedence table over punctuation subtypes (0 for unlisted).
int PC_OperatorPriority(int op)
{
    switch (op) {
    case 5: return 7;
    case 6: return 6;
    case 7: return 12;
    case 8: return 12;
    case 9: return 11;
    case 10: return 11;
    case 0x15: return 13;
    case 0x16: return 13;
    case 0x1a: return 15;
    case 0x1b: return 15;
    case 0x1c: return 15;
    case 0x1d: return 14;
    case 0x1e: return 14;
    case 0x20: return 10;
    case 0x21: return 8;
    case 0x22: return 9;
    case 0x23: return 16;
    case 0x24: return 16;
    case 0x25: return 12;
    case 0x26: return 12;
    case 0x2a: return 5;
    case 0x2b: return 5;
    default: return 0;
    }
}

// 0x0804cf9c  size=3974  FUN_0804cf9c  status=translated  naming=inferred
// PC_EvaluateTokens: strings "defined", "undefined name %s in #if/#elif", "out of value space\n",
// "syntax error in #if/#elif", "mising values in #if/#elif", "divide by zero in #if/#elif\n", etc.
// Shunting-style evaluator over linked value/operator stacks computing both int and double results.
int PC_EvaluateTokens(source_t *source, token_t *tokens, int *intvalue, double *floatvalue, int integer)
{
    pc_operator_t operatorheap[PC_MAX_OPERATORS];
    pc_value_t valueheap[PC_MAX_VALUES];
    pc_operator_t *o, *firstoperator, *lastoperator;
    pc_value_t *v, *v1, *v2, *firstvalue, *lastvalue;
    token_t *t;
    int brace, parentheses, error, lastwasvalue, negativevalue;
    int questmarkintvalue, questmark, lastoperatortype;
    int numvalues, numoperators;
    double questmarkfloatvalue;
    define_t *def;

    brace = 0;
    parentheses = 0;
    error = 0;
    lastwasvalue = 0;
    negativevalue = 0;
    questmarkintvalue = 0;
    questmarkfloatvalue = 0.0;
    questmark = 0;
    lastoperatortype = 0;
    numoperators = 0;
    numvalues = 0;
    firstoperator = 0;
    lastoperator = 0;
    firstvalue = 0;
    lastvalue = 0;
    (void)brace;
    (void)lastoperatortype;

    if (intvalue != 0) {
        *intvalue = 0;
    }
    if (floatvalue != 0) {
        *floatvalue = 0;
    }
    brace = 0;
    for (t = tokens; t != 0; t = t->next) {
        switch (t->type) {
        case PC_TT_NAME:
            if (lastwasvalue || negativevalue) {
                PC_SourceError(source, "syntax error in #if/#elif");
                error = 1;
                break;
            }
            if (strcmp(t->string, "defined") != 0) {
                PC_SourceError(source, "undefined name %s in #if/#elif", t->string);
                error = 1;
                break;
            }
            t = t->next;
            if (strcmp(t->string, "(") == 0) {
                brace = 1;
                t = t->next;
            }
            if (t == 0 || t->type != PC_TT_NAME) {
                PC_SourceError(source, "defined without name in #if/#elif");
                error = 1;
                break;
            }
            if (numvalues >= PC_MAX_VALUES) {
                PC_SourceError(source, "out of value space\n");
                error = 1;
                break;
            }
            v = &valueheap[numvalues++];
            def = PC_FindHashedDefine(source->definehash, t->string);
            if (def == 0) {
                v->intvalue = 0;
                v->floatvalue = 0;
            } else {
                v->intvalue = 1;
                v->floatvalue = 1.0;
            }
            v->parentheses = parentheses;
            v->next = 0;
            v->prev = lastvalue;
            if (lastvalue == 0) {
                firstvalue = v;
            } else {
                lastvalue->next = v;
            }
            lastvalue = v;
            if (brace == 0 || ((t = t->next, t != 0) && strcmp(t->string, ")") == 0)) {
                brace = 0;
                lastwasvalue = 1;
            } else {
                PC_SourceError(source, "defined without ) in #if/#elif");
                error = 1;
                break;
            }
            break;
        case PC_TT_NUMBER:
            if (lastwasvalue) {
                PC_SourceError(source, "syntax error in #if/#elif");
                error = 1;
                break;
            }
            if (numvalues >= PC_MAX_VALUES) {
                PC_SourceError(source, "out of value space\n");
                error = 1;
                break;
            }
            v = &valueheap[numvalues++];
            if (negativevalue) {
                v->intvalue = -(int)t->intvalue;
                v->floatvalue = (double)-t->floatvalue;
            } else {
                v->intvalue = (int)t->intvalue;
                v->floatvalue = (double)t->floatvalue;
            }
            v->parentheses = parentheses;
            v->next = 0;
            v->prev = lastvalue;
            if (lastvalue == 0) {
                firstvalue = v;
            } else {
                lastvalue->next = v;
            }
            lastvalue = v;
            lastwasvalue = 1;
            negativevalue = 0;
            break;
        case PC_TT_PUNCTUATION:
            if (negativevalue) {
                PC_SourceError(source, "misplaced minus sign in #if/#elif");
                error = 1;
                break;
            }
            if (t->subtype == PC_P_PARENTHESESOPEN) {
                parentheses++;
            } else if (t->subtype == PC_P_PARENTHESESCLOSE) {
                parentheses--;
                if (parentheses < 0) {
                    PC_SourceError(source, "too many ) in #if/#elsif");
                    error = 1;
                }
            } else if (integer == 0 &&
                       (t->subtype == PC_P_BIN_NOT || t->subtype == PC_P_MOD ||
                        t->subtype == PC_P_RSHIFT || t->subtype == PC_P_LSHIFT ||
                        t->subtype == PC_P_BIN_AND || t->subtype == PC_P_BIN_OR ||
                        t->subtype == PC_P_BIN_XOR)) {
                PC_SourceError(source, "illigal operator %s on floating point operands\n", t->string);
                error = 1;
            } else {
                switch (t->subtype) {
                case PC_P_LOGIC_AND:
                case PC_P_LOGIC_OR:
                case PC_P_LOGIC_GEQ:
                case PC_P_LOGIC_LEQ:
                case PC_P_LOGIC_EQ:
                case PC_P_LOGIC_UNEQ:
                case PC_P_RSHIFT:
                case PC_P_LSHIFT:
                case PC_P_MUL:
                case PC_P_DIV:
                case PC_P_MOD:
                case PC_P_ADD:
                case PC_P_BIN_AND:
                case PC_P_BIN_OR:
                case PC_P_BIN_XOR:
                case PC_P_LOGIC_GREATER:
                case PC_P_LOGIC_LESS:
                case PC_P_COLON:
                case PC_P_QUESTIONMARK:
                    if (!lastwasvalue) {
                        PC_SourceError(source, "operator %s after operator in #if/#elif", t->string);
                        error = 1;
                    }
                    break;
                default:
                    PC_SourceError(source, "invalid operator %s in #if/#elif", t->string);
                    error = 1;
                    break;
                case PC_P_INC:
                case PC_P_DEC:
                    PC_SourceError(source, "++ or -- used in #if/#elif");
                    break;
                case PC_P_SUB:
                    if (!lastwasvalue) {
                        negativevalue = 1;
                    }
                    break;
                case PC_P_BIN_NOT:
                case PC_P_LOGIC_NOT:
                    if (lastwasvalue) {
                        PC_SourceError(source, "! or ~ after value in #if/#elif");
                        error = 1;
                    }
                    break;
                }
                if (!error && !negativevalue) {
                    if (numoperators >= PC_MAX_OPERATORS) {
                        PC_SourceError(source, "out of operator space\n");
                        error = 1;
                        break;
                    }
                    o = &operatorheap[numoperators++];
                    o->op = t->subtype;
                    o->priority = PC_OperatorPriority(t->subtype);
                    o->parentheses = parentheses;
                    o->next = 0;
                    o->prev = lastoperator;
                    if (lastoperator == 0) {
                        firstoperator = o;
                    } else {
                        lastoperator->next = o;
                    }
                    lastoperator = o;
                    lastwasvalue = 0;
                }
            }
            break;
        default:
            PC_SourceError(source, "unknown %s in #if/#elif", t->string);
            error = 1;
            break;
        }
        if (error) {
            break;
        }
    }
    if (!error) {
        if (!lastwasvalue) {
            PC_SourceError(source, "trailing operator in #if/#elif");
            error = 1;
        } else if (parentheses != 0) {
            PC_SourceError(source, "too many ( in #if/#elif");
            error = 1;
        }
    }
    questmark = 0;
    questmarkintvalue = 0;
    questmarkfloatvalue = 0.0;
    while (!error && firstoperator != 0) {
        v = firstvalue;
        o = firstoperator;
        while (1) {
            if (o->next == 0 || o->next->parentheses < o->parentheses ||
                (o->parentheses == o->next->parentheses && o->next->priority <= o->priority)) {
                break;
            }
            if (o->op != PC_P_LOGIC_NOT && o->op != PC_P_BIN_NOT) {
                v = v->next;
            }
            if (v == 0) {
                PC_SourceError(source, "mising values in #if/#elif");
                error = 1;
                break;
            }
            o = o->next;
        }
        if (error) {
            break;
        }
        v1 = v;
        v2 = v->next;
        switch (o->op) {
        case PC_P_LOGIC_AND:
            v1->intvalue = (v1->intvalue != 0 && v2->intvalue != 0);
            v1->floatvalue = (double)(v1->floatvalue != 0.0 && v2->floatvalue != 0.0);
            break;
        case PC_P_LOGIC_OR:
            v1->intvalue = (v1->intvalue != 0 || v2->intvalue != 0);
            v1->floatvalue = (double)(v1->floatvalue != 0.0 || v2->floatvalue != 0.0);
            break;
        case PC_P_LOGIC_GEQ:
            v1->intvalue = (v1->intvalue >= v2->intvalue);
            v1->floatvalue = (double)(v1->floatvalue >= v2->floatvalue);
            break;
        case PC_P_LOGIC_LEQ:
            v1->intvalue = (v1->intvalue <= v2->intvalue);
            v1->floatvalue = (double)(v1->floatvalue <= v2->floatvalue);
            break;
        case PC_P_LOGIC_EQ:
            v1->intvalue = (v1->intvalue == v2->intvalue);
            v1->floatvalue = (double)(v1->floatvalue == v2->floatvalue);
            break;
        case PC_P_LOGIC_UNEQ:
            v1->intvalue = (v1->intvalue != v2->intvalue);
            v1->floatvalue = (double)(v1->floatvalue != v2->floatvalue);
            break;
        case PC_P_RSHIFT:
            v1->intvalue = v1->intvalue >> ((unsigned char)v2->intvalue & 0x1f);
            break;
        case PC_P_LSHIFT:
            v1->intvalue = (int)((unsigned int)v1->intvalue << ((unsigned char)v2->intvalue & 0x1f));
            break;
        case PC_P_MUL:
            v1->intvalue = (int)((unsigned int)v1->intvalue * (unsigned int)v2->intvalue);
            v1->floatvalue = v1->floatvalue * v2->floatvalue;
            break;
        case PC_P_DIV:
            if (v2->intvalue == 0 || v2->floatvalue == 0.0) {
                PC_SourceError(source, "divide by zero in #if/#elif\n");
                error = 1;
            } else {
                v1->intvalue = v1->intvalue / v2->intvalue;
                v1->floatvalue = v1->floatvalue / v2->floatvalue;
            }
            break;
        case PC_P_MOD:
            if (v2->intvalue == 0) {
                PC_SourceError(source, "divide by zero in #if/#elif\n");
                error = 1;
            } else {
                v1->intvalue = v1->intvalue % v2->intvalue;
            }
            break;
        case PC_P_ADD:
            v1->intvalue = (int)((unsigned int)v2->intvalue + (unsigned int)v1->intvalue);
            v1->floatvalue = v1->floatvalue + v2->floatvalue;
            break;
        case PC_P_SUB:
            v1->intvalue = (int)((unsigned int)v1->intvalue - (unsigned int)v2->intvalue);
            v1->floatvalue = v1->floatvalue - v2->floatvalue;
            break;
        case PC_P_BIN_AND:
            v1->intvalue = v2->intvalue & v1->intvalue;
            break;
        case PC_P_BIN_OR:
            v1->intvalue = v2->intvalue | v1->intvalue;
            break;
        case PC_P_BIN_XOR:
            v1->intvalue = v2->intvalue ^ v1->intvalue;
            break;
        case PC_P_BIN_NOT:
            v1->intvalue = ~v1->intvalue;
            break;
        case PC_P_LOGIC_NOT:
            v1->intvalue = (v1->intvalue == 0);
            v1->floatvalue = (double)(v1->floatvalue == 0.0);
            break;
        case PC_P_LOGIC_GREATER:
            v1->intvalue = (v2->intvalue < v1->intvalue);
            v1->floatvalue = (double)(v2->floatvalue < v1->floatvalue);
            break;
        case PC_P_LOGIC_LESS:
            v1->intvalue = (v1->intvalue < v2->intvalue);
            v1->floatvalue = (double)(v1->floatvalue < v2->floatvalue);
            break;
        case PC_P_COLON:
            if (questmark == 0) {
                PC_SourceError(source, ": without ? in #if/#elif");
                error = 1;
            } else {
                if (integer == 0) {
                    if (questmarkfloatvalue == 0.0) {
                        v1->floatvalue = v2->floatvalue;
                    }
                } else if (questmarkintvalue == 0) {
                    v1->intvalue = v2->intvalue;
                }
                questmark = 0;
            }
            break;
        case PC_P_QUESTIONMARK:
            if (questmark == 0) {
                questmarkintvalue = v1->intvalue;
                questmarkfloatvalue = v1->floatvalue;
                questmark = 1;
            } else {
                PC_SourceError(source, "? after ? in #if/#elif");
                error = 1;
            }
            break;
        }
        if (error) {
            break;
        }
        lastoperatortype = o->op;
        if (o->op != PC_P_LOGIC_NOT && o->op != PC_P_BIN_NOT) {
            if (o->op != PC_P_QUESTIONMARK) {
                v = v->next;
            }
            if (v->prev == 0) {
                firstvalue = v->next;
            } else {
                v->prev->next = v->next;
            }
            if (v->next == 0) {
                lastvalue = v->prev;
            } else {
                v->next->prev = v->prev;
            }
        }
        if (o->prev == 0) {
            firstoperator = o->next;
        } else {
            o->prev->next = o->next;
        }
        if (o->next == 0) {
            lastoperator = o->prev;
        } else {
            o->next->prev = o->prev;
        }
    }
    if (firstvalue != 0) {
        if (intvalue != 0) {
            *intvalue = firstvalue->intvalue;
        }
        if (floatvalue != 0) {
            *floatvalue = firstvalue->floatvalue;
        }
    }
    /* the binary walks both remaining lists with empty loop bodies (no side effects) */
    if (!error) {
        return 1;
    }
    if (intvalue != 0) {
        *intvalue = 0;
    }
    if (floatvalue != 0) {
        *floatvalue = 0;
    }
    return 0;
}

// 0x0804df22  size=842  FUN_0804df22  status=translated  naming=inferred
// PC_Evaluate: strings "no value after #if/#elif", "defined", "can't evaluate %s, not defined",
// "can't evaluate %s". Reads the rest of the line, expands defines, evaluates.
int PC_Evaluate(source_t *source, int *intvalue, double *floatvalue, int integer)
{
    token_t token;
    token_t *firsttoken, *lasttoken;
    token_t *t, *nexttoken;
    define_t *define;
    int defined;

    defined = 0;
    if (intvalue != 0) {
        *intvalue = 0;
    }
    if (floatvalue != 0) {
        *floatvalue = 0;
    }
    if (!PC_ReadLine(source, &token)) {
        PC_SourceError(source, "no value after #if/#elif");
        return 0;
    }
    firsttoken = 0;
    lasttoken = 0;
    do {
        if (token.type == PC_TT_NAME) {
            if (defined) {
                defined = 0;
                t = PC_CopyToken(&token);
                t->next = 0;
                if (lasttoken == 0) {
                    firsttoken = t;
                } else {
                    lasttoken->next = t;
                }
                lasttoken = t;
            } else if (strcmp(token.string, "defined") == 0) {
                defined = 1;
                t = PC_CopyToken(&token);
                t->next = 0;
                if (lasttoken == 0) {
                    firsttoken = t;
                } else {
                    lasttoken->next = t;
                }
                lasttoken = t;
            } else {
                define = PC_FindHashedDefine(source->definehash, token.string);
                if (define == 0) {
                    PC_SourceError(source, "can't evaluate %s, not defined", token.string);
                    return 0;
                }
                if (!PC_ExpandDefineIntoSource(source, &token, define)) {
                    return 0;
                }
            }
        } else {
            if (token.type != PC_TT_NUMBER && token.type != PC_TT_PUNCTUATION) {
                PC_SourceError(source, "can't evaluate %s", token.string);
                return 0;
            }
            t = PC_CopyToken(&token);
            t->next = 0;
            if (lasttoken == 0) {
                firsttoken = t;
            } else {
                lasttoken->next = t;
            }
            lasttoken = t;
        }
    } while (PC_ReadLine(source, &token));
    if (!PC_EvaluateTokens(source, firsttoken, intvalue, floatvalue, integer)) {
        return 0;
    }
    for (t = firsttoken; t != 0; t = nexttoken) {
        nexttoken = t->next;
        PC_FreeToken(t);
    }
    return 1;
}

// 0x0804e26c  size=940  FUN_0804e26c  status=translated  naming=inferred
// PC_DollarEvaluate: strings "no leading ( after $evalint/$evalfloat", "nothing to evaluate",
// "defined", "can't evaluate %s, not defined", "can't evaluate %s". Same as PC_Evaluate but
// reads across lines with PC_ReadSourceToken until the matching ")".
int PC_DollarEvaluate(source_t *source, int *intvalue, double *floatvalue, int integer)
{
    int indent, defined;
    token_t token;
    token_t *firsttoken, *lasttoken;
    token_t *t, *nexttoken;
    define_t *define;

    defined = 0;
    if (intvalue != 0) {
        *intvalue = 0;
    }
    if (floatvalue != 0) {
        *floatvalue = 0;
    }
    if (!PC_ReadSourceToken(source, &token)) {
        PC_SourceError(source, "no leading ( after $evalint/$evalfloat");
        return 0;
    }
    if (!PC_ReadSourceToken(source, &token)) {
        PC_SourceError(source, "nothing to evaluate");
        return 0;
    }
    indent = 1;
    firsttoken = 0;
    lasttoken = 0;
    do {
        if (token.type == PC_TT_NAME) {
            if (defined) {
                defined = 0;
                t = PC_CopyToken(&token);
                t->next = 0;
                if (lasttoken == 0) {
                    firsttoken = t;
                } else {
                    lasttoken->next = t;
                }
                lasttoken = t;
            } else if (strcmp(token.string, "defined") == 0) {
                defined = 1;
                t = PC_CopyToken(&token);
                t->next = 0;
                if (lasttoken == 0) {
                    firsttoken = t;
                } else {
                    lasttoken->next = t;
                }
                lasttoken = t;
            } else {
                define = PC_FindHashedDefine(source->definehash, token.string);
                if (define == 0) {
                    PC_SourceError(source, "can't evaluate %s, not defined", token.string);
                    return 0;
                }
                if (!PC_ExpandDefineIntoSource(source, &token, define)) {
                    return 0;
                }
            }
        } else {
            if (token.type != PC_TT_NUMBER && token.type != PC_TT_PUNCTUATION) {
                PC_SourceError(source, "can't evaluate %s", token.string);
                return 0;
            }
            if (token.string[0] == '(') {
                indent++;
            } else if (token.string[0] == ')') {
                indent--;
            }
            if (indent <= 0) {
                break;
            }
            t = PC_CopyToken(&token);
            t->next = 0;
            if (lasttoken == 0) {
                firsttoken = t;
            } else {
                lasttoken->next = t;
            }
            lasttoken = t;
        }
    } while (PC_ReadSourceToken(source, &token));
    if (!PC_EvaluateTokens(source, firsttoken, intvalue, floatvalue, integer)) {
        return 0;
    }
    for (t = firsttoken; t != 0; t = nexttoken) {
        nexttoken = t->next;
        PC_FreeToken(t);
    }
    return 1;
}
