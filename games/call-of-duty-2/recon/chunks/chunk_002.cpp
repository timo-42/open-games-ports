#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_002_types.h"

// 0x0804e618  size=171  FUN_0804e618  status=translated  naming=inferred
// Name evidence: string "misplaced #elif"; indent-stack pop/push around a #elif condition (id Tech 3 l_precomp PC_Directive_elif counterpart).
int PC_Directive_elif(source_t *source)
{
    int type;
    int skip;
    int value;

    PC_PopIndent(source, &type, &skip);
    if (type == 0 || type == INDENT_ELSE) {
        PC_SourceError(source, "misplaced #elif");
        return 0;
    }
    if (!PC_Evaluate(source, &value, 0, 1)) {
        return 0;
    }
    skip = (value == 0);
    PC_PushIndent(source, INDENT_ELIF, skip);
    return 1;
}

// 0x0804e7e0  size=120  FUN_0804e7e0  status=translated  naming=inferred
// Name evidence: pushes back a synthetic "-" punctuation token (subtype 30) located at the current script position (PC_UnreadSignToken counterpart).
void PC_UnreadSignToken(source_t *source)
{
    token_t token;

    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    strcpy(token.string, "-");
    token.type = TT_PUNCTUATION;
    token.subtype = P_SUB;
    PC_UnreadSourceToken(source, &token);
}

// 0x0804e858  size=240  FUN_0804e858  status=translated  naming=inferred
// Name evidence: evaluates an integer expression and pushes back its decimal text ("%d", abs value, then a sign token if negative); PC_Directive_evalint counterpart.
int PC_Directive_evalint(source_t *source)
{
    int value;
    token_t token;

    if (!PC_Evaluate(source, &value, 0, 1)) {
        return 0;
    }
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf(token.string, "%d", value < 0 ? -value : value);
    token.type = TT_NUMBER;
    token.subtype = TT_INTEGER | TT_LONG | TT_DECIMAL;
    PC_UnreadSourceToken(source, &token);
    if (value < 0) {
        PC_UnreadSignToken(source);
    }
    return 1;
}

// 0x0804e948  size=229  FUN_0804e948  status=translated  naming=inferred
// Name evidence: float variant of the above: "%1.2f" of Float_Abs(value), float subtype, sign token pushed for negatives (PC_Directive_evalfloat counterpart).
int PC_Directive_evalfloat(source_t *source)
{
    double value;
    token_t token;

    if (!PC_Evaluate(source, 0, &value, 0)) {
        return 0;
    }
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf(token.string, "%1.2f", (double)Float_Abs((float)value));
    token.type = TT_NUMBER;
    token.subtype = TT_FLOAT | TT_LONG | TT_DECIMAL;
    PC_UnreadSourceToken(source, &token);
    if (value < 0.0) {
        PC_UnreadSignToken(source);
    }
    return 1;
}

// 0x0804ea2e  size=284  FUN_0804ea2e  status=translated  naming=inferred
// Name evidence: strings "found # without name", "unknown precompiler directive %s", "found # at end of line"; dispatch through the name/handler table at 0x0817d3c0 (PC_ReadDirective counterpart).
int PC_ReadDirective(source_t *source)
{
    token_t token;
    int i;

    if (!PC_ReadSourceToken(source, &token)) {
        PC_SourceError(source, "found # without name");
        return 0;
    }
    if (token.linescrossed > 0) {
        PC_UnreadSourceToken(source, &token);
        PC_SourceError(source, "found # at end of line");
        return 0;
    }
    if (token.type == TT_NAME) {
        for (i = 0; g_0817d3c0_directives[i].name != 0; i++) {
            if (!strcmp(g_0817d3c0_directives[i].name, token.string)) {
                return g_0817d3c0_directives[i].func(source);
            }
        }
    }
    PC_SourceError(source, "unknown precompiler directive %s", token.string);
    return 0;
}

// 0x0804eb4a  size=252  FUN_0804eb4a  status=translated  naming=inferred
// Name evidence: '$' directive table entry "evalint" (0x0817d460 table); same shape as PC_Directive_evalint but with PC_DollarEvaluate and intvalue/floatvalue filled in.
int PC_DollarDirective_evalint(source_t *source)
{
    int value;
    token_t token;

    if (!PC_DollarEvaluate(source, &value, 0, 1)) {
        return 0;
    }
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf(token.string, "%d", value < 0 ? -value : value);
    token.type = TT_NUMBER;
    token.subtype = TT_INTEGER | TT_LONG | TT_DECIMAL;
    token.intvalue = value;
    token.floatvalue = (long double)value;
    PC_UnreadSourceToken(source, &token);
    if (value < 0) {
        PC_UnreadSignToken(source);
    }
    return 1;
}

// 0x0804ec46  size=295  FUN_0804ec46  status=translated  naming=inferred
// Name evidence: '$' directive float variant; intvalue = (int)(long long)value truncated via fistp with truncating control word.
int PC_DollarDirective_evalfloat(source_t *source)
{
    double value;
    token_t token;

    if (!PC_DollarEvaluate(source, 0, &value, 0)) {
        return 0;
    }
    token.line = source->scriptstack->line;
    token.whitespace_p = source->scriptstack->script_p;
    token.endwhitespace_p = source->scriptstack->script_p;
    token.linescrossed = 0;
    sprintf(token.string, "%1.2f", (double)Float_Abs((float)value));
    token.type = TT_NUMBER;
    token.subtype = TT_FLOAT | TT_LONG | TT_DECIMAL;
    token.intvalue = (unsigned int)(long long)value;
    token.floatvalue = (long double)value;
    PC_UnreadSourceToken(source, &token);
    if (value < 0.0) {
        PC_UnreadSignToken(source);
    }
    return 1;
}

// 0x0804ed6e  size=305  FUN_0804ed6e  status=translated  naming=inferred
// Name evidence: strings "found $ without name", "unknown precompiler directive %s", "found $ at end of line"; table at 0x0817d460 starting with "evalint".
int PC_ReadDollarDirective(source_t *source)
{
    token_t token;
    int i;

    if (!PC_ReadSourceToken(source, &token)) {
        PC_SourceError(source, "found $ without name");
        return 0;
    }
    if (token.linescrossed > 0) {
        PC_UnreadSourceToken(source, &token);
        PC_SourceError(source, "found $ at end of line");
        return 0;
    }
    if (token.type == TT_NAME) {
        for (i = 0; g_0817d460_dollardirectives[i].name != 0; i++) {
            if (!strcmp(g_0817d460_dollardirectives[i].name, token.string)) {
                return g_0817d460_dollardirectives[i].func(source);
            }
        }
    }
    PC_UnreadSourceToken(source, &token);
    PC_SourceError(source, "unknown precompiler directive %s", token.string);
    return 0;
}

// 0x0804eea0  size=507  FUN_0804eea0  status=translated  naming=inferred
// Name evidence: string "string longer than MAX_TOKEN %d\n"; handles '#'/'$' directives, adjacent string concatenation and define expansion (PC_ReadToken counterpart).
int PC_ReadToken(source_t *source, token_t *token)
{
    token_t newtoken;
    define_t *define;

    for (;;) {
        do {
            for (;;) {
                if (!PC_ReadSourceToken(source, token)) {
                    return 0;
                }
                if (token->type == TT_PUNCTUATION && token->string[0] == '#') {
                    if (!PC_ReadDirective(source)) {
                        return 0;
                    }
                    continue;
                }
                if (token->type == TT_PUNCTUATION && token->string[0] == '$') {
                    if (!PC_ReadDollarDirective(source)) {
                        return 0;
                    }
                    continue;
                }
                break;
            }
            if (token->type == TT_STRING && PC_ReadToken(source, &newtoken)) {
                if (newtoken.type == TT_STRING) {
                    token->string[strlen(token->string) - 1] = '\0';
                    if (strlen(token->string) + strlen(newtoken.string + 1) + 1 > MAX_TOKEN_CHARS - 1) {
                        PC_SourceError(source, "string longer than MAX_TOKEN %d\n", MAX_TOKEN_CHARS);
                        return 0;
                    }
                    strcat(token->string, newtoken.string + 1);
                } else {
                    PC_UnreadToken(source, &newtoken);
                }
            }
        } while (source->skip != 0);

        if (token->type != TT_NAME) {
            break;
        }
        define = PC_FindHashedDefine(source->definehash, token->string);
        if (define == 0) {
            break;
        }
        if (!PC_ExpandDefineIntoSource(source, token, define)) {
            return 0;
        }
    }
    memcpy(&source->token, token, TOKEN_COPY_SIZE);
    return 1;
}

// 0x0804f464  size=122  FUN_0804f464  status=translated  naming=inferred
// Name evidence: reads a token, returns 1 if it equals the given string, otherwise unreads it (PC_CheckTokenString counterpart).
int PC_CheckTokenString(source_t *source, char *string)
{
    token_t tok;

    if (!PC_ReadToken(source, &tok)) {
        return 0;
    }
    if (!strcmp(tok.string, string)) {
        return 1;
    }
    PC_UnreadSourceToken(source, &tok);
    return 0;
}

// 0x0804f4de  size=145  FUN_0804f4de  status=translated  naming=inferred
// Name evidence: reads a token; if type matches and subtype contains the requested bits copies it out, otherwise unreads (PC_CheckTokenType counterpart).
int PC_CheckTokenType(source_t *source, int type, unsigned int subtype, token_t *token)
{
    token_t tok;

    if (!PC_ReadToken(source, &tok)) {
        return 0;
    }
    if (tok.type == type && (subtype & tok.subtype) == subtype) {
        memcpy(token, &tok, TOKEN_COPY_SIZE);
        return 1;
    }
    PC_UnreadSourceToken(source, &tok);
    return 0;
}

// 0x0804f5ec  size=26  FUN_0804f5ec  status=translated  naming=inferred
// Name evidence: thin wrapper over the source-level unread (PC_UnreadToken counterpart).
void PC_UnreadToken(source_t *source, token_t *token)
{
    PC_UnreadSourceToken(source, token);
}

// 0x0804f606  size=112  FUN_0804f606  status=translated  naming=inferred
// Name evidence: copies up to 64 chars into source->includepath and appends '/' unless it already ends in '\\' or '/' (PC_SetIncludePath counterpart).
void PC_SetIncludePath(source_t *source, char *path)
{
    strncpy(source->includepath, path, 0x40);
    if (source->includepath[strlen(source->includepath) - 1] != '\\') {
        if (source->includepath[strlen(source->includepath) - 1] != '/') {
            strcat(source->includepath, "/");
        }
    }
}

// 0x0804f676  size=17  FUN_0804f676  status=translated  naming=inferred
// Name evidence: stores a punctuation table pointer at source+0x80 (PC_SetPunctuations counterpart).
void PC_SetPunctuations(source_t *source, punctuation_t *p)
{
    source->punctuations = p;
}

// 0x0804f688  size=231  FUN_0804f688  status=translated  naming=inferred
// Name evidence: loads a script file, allocates a 0x4c8-byte source, adds global defines (LoadSourceFile counterpart).
source_t *LoadSourceFile(char *filename)
{
    source_t *source;
    script_t *script;

    script = LoadScriptFile(filename);
    if (!script) {
        return 0;
    }
    script->next = 0;

    source = (source_t *)GetMemory(sizeof(source_t));
    memset(source, 0, sizeof(source_t));

    strncpy(source->filename, filename, 0x40);
    source->scriptstack = script;
    source->tokens = 0;
    source->defines = 0;
    source->indentstack = 0;
    source->skip = 0;

    source->definehash = (define_t **)GetClearedMemory(DEFINEHASHSIZE * sizeof(define_t *));
    PC_AddGlobalDefinesToSource(source);
    return source;
}

// 0x0804f866  size=384  FUN_0804f866  status=translated  naming=inferred
// Name evidence: frees script stack, pending tokens, 1024 define hash chains, indent stack, hash table and the source itself (FreeSource counterpart).
void FreeSource(source_t *source)
{
    script_t *script;
    token_t *token;
    define_t *define;
    indent_t *indent;
    int i;

    while (source->scriptstack) {
        script = source->scriptstack;
        source->scriptstack = source->scriptstack->next;
        FreeScript(script);
    }
    while (source->tokens) {
        token = source->tokens;
        source->tokens = source->tokens->next;
        PC_FreeToken(token);
    }
    for (i = 0; i < DEFINEHASHSIZE; i++) {
        while (source->definehash[i]) {
            define = source->definehash[i];
            source->definehash[i] = source->definehash[i]->hashnext;
            PC_FreeDefine(define);
        }
    }
    while (source->indentstack) {
        indent = source->indentstack;
        source->indentstack = source->indentstack->next;
        FreeMemory(indent);
    }
    if (source->definehash) {
        FreeMemory(source->definehash);
    }
    FreeMemory(source);
}

// 0x0804f9e6  size=111  FUN_0804f9e6  status=translated  naming=inferred
// Name evidence: finds a free slot in the 64-entry source handle table (index 0 unused) and stores LoadSourceFile's result (PC_LoadSourceHandle counterpart).
int PC_LoadSourceHandle(char *filename)
{
    source_t *source;
    int i;

    for (i = 1; i < MAX_SOURCEFILES; i++) {
        if (!g_08188ca0_sourceFiles[i]) {
            break;
        }
    }
    if (i >= MAX_SOURCEFILES) {
        return 0;
    }
    source = LoadSourceFile(filename);
    if (!source) {
        return 0;
    }
    g_08188ca0_sourceFiles[i] = source;
    return i;
}

// 0x0804fc42  size=153  FUN_0804fc42  status=translated  naming=inferred
// Name evidence: handle range check 1..63, copies the top script's filename and line, or the source filename and line 0 (PC_SourceFileAndLine counterpart).
int PC_SourceFileAndLine(int handle, char *filename, int *line)
{
    script_t *script;

    if (handle < 1 || handle >= MAX_SOURCEFILES) {
        return 0;
    }
    if (!g_08188ca0_sourceFiles[handle]) {
        return 0;
    }
    script = g_08188ca0_sourceFiles[handle]->scriptstack;
    if (script) {
        strcpy(filename, script->filename);
        *line = script->line;
    } else {
        strcpy(filename, g_08188ca0_sourceFiles[handle]->filename);
        *line = 0;
    }
    return 1;
}

// 0x0804fd28  size=25  FUN_0804fd28  status=translated  naming=inferred
// Name evidence: single-precision absolute value (Q_fabs counterpart).
float Float_Abs(float value)
{
    return fabsf(value);
}

// 0x0804fd44  size=341  FUN_0804fd44  status=translated  naming=inferred
// Name evidence: builds a 256-entry first-character table of punctuation chains sorted by descending length (PS_CreatePunctuationTable counterpart); the table index is a signed char.
void PS_CreatePunctuationTable(script_t *script, punctuation_t *punctuations)
{
    int i;
    punctuation_t *p, *lastp, *newp;

    if (!script->punctuationtable) {
        script->punctuationtable = (punctuation_t **)GetMemory(256 * sizeof(punctuation_t *));
    }
    memset(script->punctuationtable, 0, 256 * sizeof(punctuation_t *));
    for (i = 0; punctuations[i].p; i++) {
        newp = &punctuations[i];
        lastp = 0;
        for (p = script->punctuationtable[(signed char)newp->p[0]]; p; p = p->next) {
            if (strlen(p->p) < strlen(newp->p)) {
                newp->next = p;
                if (!lastp) {
                    script->punctuationtable[(signed char)newp->p[0]] = newp;
                } else {
                    lastp->next = newp;
                }
                break;
            }
            lastp = p;
        }
        if (!p) {
            newp->next = 0;
            if (!lastp) {
                script->punctuationtable[(signed char)newp->p[0]] = newp;
            } else {
                lastp->next = newp;
            }
        }
    }
}

// 0x0804fe9a  size=123  FUN_0804fe9a  status=translated  naming=inferred
// Name evidence: string "unkown punctuation" (sic) returned when no entry has the requested number (PunctuationFromNum counterpart).
char *PunctuationFromNum(script_t *script, int num)
{
    int i;

    for (i = 0; script->punctuations[i].p; i++) {
        if (script->punctuations[i].n == num) {
            return script->punctuations[i].p;
        }
    }
    return "unkown punctuation";
}

// 0x0804ff16  size=113  FUN_0804ff16  status=translated  naming=inferred
// Name evidence: string "^1Error: file %s, line %d: %s\n", suppressed by flag bit 0 (ScriptError counterpart).
void ScriptError(script_t *script, const char *fmt, ...)
{
    char text[1024];
    va_list ap;

    if (script->flags & SCFL_NOERRORS) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    Com_Printf("^1Error: file %s, line %d: %s\n", script->filename, script->line, text);
}

// 0x0804ff88  size=113  FUN_0804ff88  status=translated  naming=inferred
// Name evidence: string "^3Warning: file %s, line %d: %s\n", suppressed by flag bit 1 (ScriptWarning counterpart).
void ScriptWarning(script_t *script, const char *fmt, ...)
{
    char text[1024];
    va_list ap;

    if (script->flags & SCFL_NOWARNINGS) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    Com_Printf("^3Warning: file %s, line %d: %s\n", script->filename, script->line, text);
}

// 0x0804fffa  size=72  FUN_0804fffa  status=translated  naming=inferred
// Name evidence: builds the punctuation table from the given array, or from the default table at 0x0817d500 when NULL (SetScriptPunctuations counterpart).
void SetScriptPunctuations(script_t *script, punctuation_t *p)
{
    if (p) {
        PS_CreatePunctuationTable(script, p);
        script->punctuations = p;
    } else {
        PS_CreatePunctuationTable(script, g_0817d500_default_punctuations);
        script->punctuations = g_0817d500_default_punctuations;
    }
}

// 0x08050042  size=336  FUN_08050042  status=translated  naming=inferred
// Name evidence: skips bytes < '!' (signed char compare), // and /* */ comments, counting newlines (PS_ReadWhiteSpace counterpart).
int PS_ReadWhiteSpace(script_t *script)
{
    for (;;) {
        while (*script->script_p < '!') {
            if (*script->script_p == '\0') {
                return 0;
            }
            if (*script->script_p == '\n') {
                script->line++;
            }
            script->script_p++;
        }
        if (*script->script_p != '/') {
            return 1;
        }
        if (*(script->script_p + 1) == '/') {
            script->script_p++;
            do {
                script->script_p++;
                if (*script->script_p == '\0') {
                    return 0;
                }
            } while (*script->script_p != '\n');
            script->line++;
            script->script_p++;
            if (*script->script_p == '\0') {
                return 0;
            }
        } else if (*(script->script_p + 1) == '*') {
            script->script_p++;
            do {
                script->script_p++;
                if (*script->script_p == '\0') {
                    return 0;
                }
                if (*script->script_p == '\n') {
                    script->line++;
                }
            } while (!(*script->script_p == '*' && *(script->script_p + 1) == '/'));
            script->script_p++;
            if (*script->script_p == '\0') {
                return 0;
            }
            script->script_p++;
            if (*script->script_p == '\0') {
                return 0;
            }
        } else {
            return 1;
        }
    }
}

// 0x08050192  size=535  FUN_08050192  status=translated  naming=inferred
// Name evidence: strings "unknown escape char", "too large value in escape character"; \x accepts any ASCII letter as a digit (quirk kept) (PS_ReadEscapeCharacter counterpart).
int PS_ReadEscapeCharacter(script_t *script, char *ch)
{
    int c, val, digit;

    script->script_p++;
    switch (*script->script_p) {
    case '\\': c = '\\'; break;
    case 'a': c = 7; break;
    case 'b': c = 8; break;
    case 'f': c = 0xc; break;
    case 'n': c = 10; break;
    case 'r': c = 0xd; break;
    case 't': c = 9; break;
    case 'v': c = 0xb; break;
    case '\'': c = '\''; break;
    case '"': c = '"'; break;
    case '?': c = '?'; break;
    case 'x':
        script->script_p++;
        val = 0;
        for (;;) {
            digit = (int)*script->script_p;
            if (digit >= '0' && digit <= '9') {
                digit = digit - '0';
            } else if (digit >= 'A' && digit <= 'Z') {
                digit = digit - 'A' + 10;
            } else if (digit >= 'a' && digit <= 'z') {
                digit = digit - 'a' + 10;
            } else {
                break;
            }
            val = val * 16 + digit;
            script->script_p++;
        }
        script->script_p--;
        if (val > 0xff) {
            ScriptWarning(script, "too large value in escape character");
            val = 0xff;
        }
        c = val;
        break;
    default:
        if (*script->script_p < '0' || *script->script_p > '9') {
            ScriptError(script, "unknown escape char");
        }
        val = 0;
        while ((int)*script->script_p >= '0' && (int)*script->script_p <= '9') {
            val = val * 10 + *script->script_p - '0';
            script->script_p++;
        }
        script->script_p--;
        if (val > 0xff) {
            ScriptWarning(script, "too large value in escape character");
            val = 0xff;
        }
        c = val;
        break;
    }
    script->script_p++;
    *ch = (char)c;
    return 1;
}

// 0x080503aa  size=547  FUN_080503aa  status=translated  naming=inferred
// Name evidence: strings "string longer than MAX_TOKEN = %d", "missing trailing quote", "newline inside string %s" (PS_ReadString counterpart).
int PS_ReadString(script_t *script, token_t *token, int quote)
{
    int len;
    int tmpline;
    char *tmpscript_p;

    if (quote == '"') {
        token->type = TT_STRING;
    } else {
        token->type = TT_LITERAL;
    }
    len = 0;
    token->string[len++] = *script->script_p++;
    for (;;) {
        if (len >= MAX_TOKEN_CHARS - 2) {
            ScriptError(script, "string longer than MAX_TOKEN = %d", MAX_TOKEN_CHARS);
            return 0;
        }
        if (*script->script_p == '\\' && !(script->flags & SCFL_NOSTRINGESCAPECHARS)) {
            if (!PS_ReadEscapeCharacter(script, &token->string[len])) {
                token->string[len] = 0;
                return 0;
            }
            len++;
        } else if (*script->script_p == quote) {
            script->script_p++;
            if (script->flags & SCFL_NOSTRINGWHITESPACES) {
                break;
            }
            tmpscript_p = script->script_p;
            tmpline = script->line;
            if (!PS_ReadWhiteSpace(script)) {
                script->script_p = tmpscript_p;
                script->line = tmpline;
                break;
            }
            if (*script->script_p != quote) {
                script->script_p = tmpscript_p;
                script->line = tmpline;
                break;
            }
            script->script_p++;
        } else {
            if (*script->script_p == '\0') {
                token->string[len] = 0;
                ScriptError(script, "missing trailing quote");
                return 0;
            }
            if (*script->script_p == '\n') {
                token->string[len] = 0;
                ScriptError(script, "newline inside string %s", token->string);
                return 0;
            }
            token->string[len++] = *script->script_p++;
        }
    }
    token->string[len] = (char)quote;
    token->string[len + 1] = 0;
    token->subtype = len + 1;
    return 1;
}

// 0x080505ce  size=192  FUN_080505ce  status=translated  naming=inferred
// Name evidence: string "name longer than MAX_TOKEN = %d"; reads [A-Za-z0-9_] identifier (PS_ReadName counterpart).
int PS_ReadName(script_t *script, token_t *token)
{
    int len = 0;
    char c;

    token->type = TT_NAME;
    do {
        token->string[len++] = *script->script_p++;
        if (len >= MAX_TOKEN_CHARS) {
            ScriptError(script, "name longer than MAX_TOKEN = %d", MAX_TOKEN_CHARS);
            return 0;
        }
        c = *script->script_p;
    } while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
    token->string[len] = 0;
    token->subtype = len;
    return 1;
}

// 0x0805068e  size=695  FUN_0805068e  status=translated  naming=inferred
// Name evidence: converts the number text to intvalue/floatvalue by subtype flag (float, decimal, hex, octal, binary) (NumberValue counterpart). Constant 10.0L at 0x0813f2a0.
void NumberValue(char *string, int subtype, unsigned int *intvalue, long double *floatvalue)
{
    unsigned int dotfound = 0;

    *intvalue = 0;
    *floatvalue = 0;
    if (subtype & TT_FLOAT) {
        while (*string) {
            if (*string == '.') {
                if (dotfound) {
                    return;
                }
                dotfound = 10;
                string++;
            }
            if (dotfound) {
                *floatvalue = *floatvalue + (long double)(*string - '0') / (long double)dotfound;
                dotfound = dotfound * 10;
            } else {
                *floatvalue = *floatvalue * 10.0L + (long double)(*string - '0');
            }
            string++;
        }
        *intvalue = (unsigned int)(long long)*floatvalue;
    } else if (subtype & TT_DECIMAL) {
        while (*string) {
            *intvalue = *string + *intvalue * 10 - '0';
            string++;
        }
        *floatvalue = *intvalue;
    } else if (subtype & TT_HEX) {
        string += 2;
        while (*string) {
            *intvalue <<= 4;
            if (*string >= 'a' && *string <= 'f') {
                *intvalue = *string + *intvalue - 'a' + 10;
            } else if (*string >= 'A' && *string <= 'F') {
                *intvalue = *string + *intvalue - 'A' + 10;
            } else {
                *intvalue = *string + *intvalue - '0';
            }
            string++;
        }
        *floatvalue = *intvalue;
    } else if (subtype & TT_OCTAL) {
        string += 1;
        while (*string) {
            *intvalue = *string + *intvalue * 8 - '0';
            string++;
        }
        *floatvalue = *intvalue;
    } else if (subtype & TT_BINARY) {
        string += 2;
        while (*string) {
            *intvalue = *string + *intvalue * 2 - '0';
            string++;
        }
        *floatvalue = *intvalue;
    }
}

// 0x08050946  size=1074  FUN_08050946  status=translated  naming=inferred
// Name evidence: strings "hexadecimal/binary number longer than MAX_TOKEN = %d", "number longer than MAX_TOKEN = %d"; hex branch accepts only 'A' as an uppercase digit (quirk, confirmed in disassembly) (PS_ReadNumber counterpart).
int PS_ReadNumber(script_t *script, token_t *token)
{
    int len = 0, i;
    int octal, dot;
    char c;

    token->type = TT_NUMBER;
    if (*script->script_p == '0' && (*(script->script_p + 1) == 'x' || *(script->script_p + 1) == 'X')) {
        token->string[len++] = *script->script_p++;
        token->string[len++] = *script->script_p++;
        c = *script->script_p;
        while ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c > '@' && c < 'B')) {
            token->string[len++] = *script->script_p++;
            if (len >= MAX_TOKEN_CHARS) {
                ScriptError(script, "hexadecimal number longer than MAX_TOKEN = %d", MAX_TOKEN_CHARS);
                return 0;
            }
            c = *script->script_p;
        }
        token->subtype |= TT_HEX;
    } else if (*script->script_p == '0' && (*(script->script_p + 1) == 'b' || *(script->script_p + 1) == 'B')) {
        token->string[len++] = *script->script_p++;
        token->string[len++] = *script->script_p++;
        c = *script->script_p;
        for (;;) {
            if (c != '0' && c != '1') {
                token->subtype |= TT_BINARY;
                goto suffix;
            }
            token->string[len++] = *script->script_p++;
            if (len >= MAX_TOKEN_CHARS) {
                break;
            }
            c = *script->script_p;
        }
        ScriptError(script, "binary number longer than MAX_TOKEN = %d", MAX_TOKEN_CHARS);
        return 0;
    } else {
        octal = 0;
        dot = 0;
        if (*script->script_p == '0') {
            octal = 1;
        }
        do {
            c = *script->script_p;
            if (c == '.') {
                dot = 1;
            } else if (c == '8' || c == '9') {
                octal = 0;
            } else if (c < '0' || c > '9') {
                if (octal) {
                    token->subtype |= TT_OCTAL;
                } else {
                    token->subtype |= TT_DECIMAL;
                }
                if (dot) {
                    token->subtype |= TT_FLOAT;
                }
                goto suffix;
            }
            token->string[len++] = *script->script_p++;
        } while (len < MAX_TOKEN_CHARS - 1);
        ScriptError(script, "number longer than MAX_TOKEN = %d", MAX_TOKEN_CHARS);
        return 0;
    }

suffix:
    for (i = 0; i < 2; i++) {
        c = *script->script_p;
        if ((c == 'l' || c == 'L') && !(token->subtype & TT_LONG)) {
            script->script_p++;
            token->subtype |= TT_LONG;
        } else if ((c == 'u' || c == 'U') && !(token->subtype & (TT_UNSIGNED | TT_FLOAT))) {
            script->script_p++;
            token->subtype |= TT_UNSIGNED;
        }
    }
    token->string[len] = 0;
    NumberValue(token->string, token->subtype, &token->intvalue, &token->floatvalue);
    if (!(token->subtype & TT_FLOAT)) {
        token->subtype |= TT_INTEGER;
    }
    return 1;
}

// 0x08050d78  size=285  FUN_08050d78  status=translated  naming=inferred
// Name evidence: strings "end of file before trailing '", "too many characters in literal, ignored" (PS_ReadLiteral counterpart).
int PS_ReadLiteral(script_t *script, token_t *token)
{
    token->type = TT_LITERAL;
    token->string[0] = *script->script_p++;
    if (*script->script_p == '\0') {
        ScriptError(script, "end of file before trailing '");
        return 0;
    }
    if (*script->script_p == '\\') {
        if (!PS_ReadEscapeCharacter(script, &token->string[1])) {
            return 0;
        }
    } else {
        token->string[1] = *script->script_p++;
    }
    if (*script->script_p != '\'') {
        ScriptWarning(script, "too many characters in literal, ignored");
        while (*script->script_p != '\0' && *script->script_p != '\'' && *script->script_p != '\n') {
            script->script_p++;
        }
        if (*script->script_p == '\'') {
            script->script_p++;
        }
    }
    token->string[2] = *script->script_p++;
    token->string[3] = 0;
    token->subtype = (int)token->string[1];
    return 1;
}

// 0x08050e96  size=222  FUN_08050e96  status=translated  naming=inferred
// Name evidence: longest-first lookup in the punctuation table keyed by the signed first char; strncpy of 1024 bytes into token->string (PS_ReadPunctuation counterpart).
int PS_ReadPunctuation(script_t *script, token_t *token)
{
    punctuation_t *punc;
    char *p;
    int len;

    for (punc = script->punctuationtable[(signed char)*script->script_p]; punc; punc = punc->next) {
        p = punc->p;
        len = strlen(p);
        if ((unsigned int)(script->script_p + len) <= (unsigned int)script->end_p) {
            if (!strncmp(script->script_p, p, len)) {
                break;
            }
        }
    }
    if (!punc) {
        return 0;
    }
    strncpy(token->string, punc->p, MAX_TOKEN_CHARS);
    script->script_p += len;
    token->type = TT_PUNCTUATION;
    token->subtype = punc->n;
    return 1;
}

// 0x08050f74  size=168  FUN_08050f74  status=translated  naming=inferred
// Name evidence: string "primitive token longer than MAX_TOKEN = %d"; reads bytes > ' ' up to ';' (signed compare) and stores the token as script->token (PS_ReadPrimitive counterpart).
int PS_ReadPrimitive(script_t *script, token_t *token)
{
    int len = 0;

    while (*script->script_p > ' ' && *script->script_p != ';') {
        if (len >= MAX_TOKEN_CHARS) {
            ScriptError(script, "primitive token longer than MAX_TOKEN = %d", MAX_TOKEN_CHARS);
            return 0;
        }
        token->string[len++] = *script->script_p++;
    }
    token->string[len] = 0;
    memcpy(&script->token, token, TOKEN_COPY_SIZE);
    return 1;
}

// 0x0805101c  size=670  FUN_0805101c  status=translated  naming=inferred
// Name evidence: string "can't read token"; unread-token replay, whitespace/line bookkeeping and dispatch to string/literal/number/primitive/punctuation/name readers (PS_ReadToken counterpart).
int PS_ReadToken(script_t *script, token_t *token)
{
    if (script->tokenavailable) {
        script->tokenavailable = 0;
        memcpy(token, &script->token, TOKEN_COPY_SIZE);
        return 1;
    }
    script->lastscript_p = script->script_p;
    script->lastline = script->line;
    memset(token, 0, TOKEN_COPY_SIZE);
    script->whitespace_p = script->script_p;
    token->whitespace_p = script->script_p;
    if (!PS_ReadWhiteSpace(script)) {
        return 0;
    }
    script->endwhitespace_p = script->script_p;
    token->endwhitespace_p = script->script_p;
    token->line = script->line;
    token->linescrossed = script->line - script->lastline;

    if (*script->script_p == '"') {
        if (!PS_ReadString(script, token, '"')) {
            return 0;
        }
    } else if (*script->script_p == '\'') {
        if (!PS_ReadString(script, token, '\'')) {
            return 0;
        }
    } else if ((*script->script_p < '0' || *script->script_p > '9') &&
               (*script->script_p != '.' || *(script->script_p + 1) < '0' || *(script->script_p + 1) > '9')) {
        if (script->flags & SCFL_PRIMITIVE) {
            return PS_ReadPrimitive(script, token);
        }
        if ((*script->script_p < 'a' || *script->script_p > 'z') &&
            (*script->script_p < 'A' || *script->script_p > 'Z') &&
            *script->script_p != '_') {
            if (!PS_ReadPunctuation(script, token)) {
                ScriptError(script, "can't read token");
                return 0;
            }
        } else {
            if (!PS_ReadName(script, token)) {
                return 0;
            }
        }
    } else {
        if (!PS_ReadNumber(script, token)) {
            return 0;
        }
    }
    memcpy(&script->token, token, TOKEN_COPY_SIZE);
    return 1;
}

// 0x080512ba  size=163  FUN_080512ba  status=translated  naming=inferred
// Name evidence: strings "couldn't find expected %s", "expected %s, found %s" (PS_ExpectTokenString counterpart).
int PS_ExpectTokenString(script_t *script, char *string)
{
    token_t token;

    if (!PS_ReadToken(script, &token)) {
        ScriptError(script, "couldn't find expected %s", string);
        return 0;
    }
    if (strcmp(token.string, string)) {
        ScriptError(script, "expected %s, found %s", string, token.string);
        return 0;
    }
    return 1;
}

// 0x0805135e  size=810  FUN_0805135e  status=translated  naming=inferred
// Name evidence: strings "couldn't read expected token", "decimal", "hex", "BUG: wrong punctuation subtype" (PS_ExpectTokenType counterpart). Quirk kept: the punctuation mismatch error passes the whole punctuation_t (p, n, next) plus the token as varargs, so "%s" for the second slot receives the integer n.
int PS_ExpectTokenType(script_t *script, int type, int subtype, token_t *token)
{
    char str[1028];

    if (!PS_ReadToken(script, token)) {
        ScriptError(script, "couldn't read expected token");
        return 0;
    }
    if (token->type != type) {
        if (type == TT_STRING) strcpy(str, "string");
        if (type == TT_LITERAL) strcpy(str, "literal");
        if (type == TT_NUMBER) strcpy(str, "number");
        if (type == TT_NAME) strcpy(str, "name");
        if (type == TT_PUNCTUATION) strcpy(str, "punctuation");
        ScriptError(script, "expected a %s, found %s", str, token->string);
        return 0;
    }
    if (token->type == TT_NUMBER) {
        if ((token->subtype & subtype) != subtype) {
            if (subtype & TT_DECIMAL) strcpy(str, "decimal");
            if (subtype & TT_HEX) strcpy(str, "hex");
            if (subtype & TT_OCTAL) strcpy(str, "octal");
            if (subtype & TT_BINARY) strcpy(str, "binary");
            if (subtype & TT_LONG) strcat(str, " long");
            if (subtype & TT_UNSIGNED) strcat(str, " unsigned");
            if (subtype & TT_FLOAT) strcat(str, " float");
            if (subtype & TT_INTEGER) strcat(str, " integer");
            ScriptError(script, "expected %s, found %s", str, token->string);
            return 0;
        }
    } else if (token->type == TT_PUNCTUATION) {
        if (subtype < 0) {
            ScriptError(script, "BUG: wrong punctuation subtype");
            return 0;
        }
        if (token->subtype != subtype) {
            ScriptError(script, "expected %s, found %s",
                        script->punctuations[subtype].p,
                        script->punctuations[subtype].n,
                        script->punctuations[subtype].next,
                        token);
            return 0;
        }
    }
    return 1;
}

// 0x08051688  size=68  FUN_08051688  status=translated  naming=inferred
// Name evidence: string "couldn't read expected token" after a failed read (PS_ExpectAnyToken counterpart).
int PS_ExpectAnyToken(script_t *script, token_t *token)
{
    if (!PS_ReadToken(script, token)) {
        ScriptError(script, "couldn't read expected token");
        return 0;
    }
    return 1;
}

// 0x080516cc  size=113  FUN_080516cc  status=translated  naming=inferred
// Name evidence: reads a token, matches the string, otherwise rewinds script_p to lastscript_p (PS_CheckTokenString counterpart).
int PS_CheckTokenString(script_t *script, char *string)
{
    token_t tok;

    if (!PS_ReadToken(script, &tok)) {
        return 0;
    }
    if (!strcmp(tok.string, string)) {
        return 1;
    }
    script->script_p = script->lastscript_p;
    return 0;
}

// 0x08051822  size=15  FUN_08051822  status=translated  naming=inferred
// Name evidence: sets tokenavailable (PS_UnreadLastToken counterpart).
void PS_UnreadLastToken(script_t *script)
{
    script->tokenavailable = 1;
}

// 0x08051832  size=47  FUN_08051832  status=translated  naming=inferred
// Name evidence: stores the token as script->token and sets tokenavailable (PS_UnreadToken counterpart).
void PS_UnreadToken(script_t *script, token_t *token)
{
    memcpy(&script->token, token, TOKEN_COPY_SIZE);
    script->tokenavailable = 1;
}

// 0x08051862  size=49  FUN_08051862  status=translated  naming=inferred
// Name evidence: returns the next whitespace byte between whitespace_p and endwhitespace_p, 0 when exhausted (PS_NextWhiteSpaceChar counterpart).
int PS_NextWhiteSpaceChar(script_t *script)
{
    if (script->whitespace_p != script->endwhitespace_p) {
        return *script->whitespace_p++;
    }
    return 0;
}

// 0x08051894  size=73  FUN_08051894  status=translated  naming=inferred
// Name evidence: removes a leading and a trailing '"' (StripDoubleQuotes counterpart).
void StripDoubleQuotes(char *string)
{
    if (*string == '"') {
        strcpy(string, string + 1);
    }
    if (string[strlen(string) - 1] == '"') {
        string[strlen(string) - 1] = '\0';
    }
}

// 0x080518de  size=73  FUN_080518de  status=translated  naming=inferred
// Name evidence: removes a leading and a trailing '\'' (StripSingleQuotes counterpart).
void StripSingleQuotes(char *string)
{
    if (*string == '\'') {
        strcpy(string, string + 1);
    }
    if (string[strlen(string) - 1] == '\'') {
        string[strlen(string) - 1] = '\0';
    }
}

// 0x08051aa8  size=14  FUN_08051aa8  status=translated  naming=inferred
// Name evidence: stores script flags at +0x68 (SetScriptFlags counterpart).
void SetScriptFlags(script_t *script, int flags)
{
    script->flags = flags;
}

// 0x08051ab6  size=11  FUN_08051ab6  status=translated  naming=inferred
// Name evidence: reads script flags at +0x68 (GetScriptFlags counterpart).
int GetScriptFlags(script_t *script)
{
    return script->flags;
}

// 0x08051ac2  size=112  FUN_08051ac2  status=translated  naming=inferred
// Name evidence: rewinds script_p to the buffer start, clears whitespace pointers, line=1, clears the cached token (ResetScript counterpart).
void ResetScript(script_t *script)
{
    script->script_p = script->buffer;
    script->lastscript_p = script->buffer;
    script->whitespace_p = 0;
    script->endwhitespace_p = 0;
    script->tokenavailable = 0;
    script->line = 1;
    script->lastline = 1;
    memset(&script->token, 0, TOKEN_COPY_SIZE);
}

// 0x08051b32  size=23  FUN_08051b32  status=translated  naming=inferred
// Name evidence: unsigned compare script_p >= end_p (EndOfScript counterpart).
int EndOfScript(script_t *script)
{
    return script->script_p >= script->end_p;
}

// 0x08051b4a  size=19  FUN_08051b4a  status=translated  naming=inferred
// Name evidence: line - lastline (NumLinesCrossed counterpart).
int NumLinesCrossed(script_t *script)
{
    return script->line - script->lastline;
}

// 0x08051b5e  size=121  FUN_08051b5e  status=translated  naming=inferred
// Name evidence: advances through whitespace and characters until the given string is found (ScriptSkipTo counterpart).
int ScriptSkipTo(script_t *script, char *string)
{
    char firstchar;
    int len;

    firstchar = *string;
    len = strlen(string);
    do {
        if (!PS_ReadWhiteSpace(script)) {
            return 0;
        }
        if (*script->script_p == firstchar) {
            if (!strncmp(script->script_p, string, len)) {
                return 1;
            }
        }
        script->script_p++;
    } while (1);
}

// 0x08051bd8  size=328  FUN_08051bd8  status=translated  naming=inferred
// Name evidence: opens a file by name, allocates 0x4a5+length bytes, sets buffer after the 0x4a4-byte header, installs default punctuations (LoadScriptFile counterpart). strcpy of the filename into the 64-byte field is unbounded (kept).
script_t *LoadScriptFile(char *filename)
{
    int handle;
    int length;
    char pathname[76];
    script_t *script;

    Com_sprintf(pathname, 0x40, "%s", filename);
    length = FS_OpenFileRead(pathname, &handle, 1);
    if (!handle) {
        return 0;
    }
    script = (script_t *)GetClearedMemory(sizeof(script_t) + length + 1);
    strcpy(script->filename, filename);
    script->buffer = (char *)script + sizeof(script_t);
    script->buffer[length] = 0;
    script->length = length;
    script->script_p = script->buffer;
    script->lastscript_p = script->buffer;
    script->end_p = &script->buffer[length];
    script->tokenavailable = 0;
    script->line = 1;
    script->lastline = 1;
    SetScriptPunctuations(script, 0);
    FS_Read(script->buffer, length, handle);
    FS_FCloseFile(handle);
    script->length = Com_Compress(script->buffer);
    return script;
}

// 0x08051d20  size=205  FUN_08051d20  status=translated  naming=inferred
// Name evidence: same layout as LoadScriptFile but copies from a memory block (LoadScriptMemory counterpart).
script_t *LoadScriptMemory(const void *ptr, int length, const char *name)
{
    script_t *script;

    script = (script_t *)GetClearedMemory(sizeof(script_t) + length + 1);
    strcpy(script->filename, name);
    script->buffer = (char *)script + sizeof(script_t);
    script->buffer[length] = 0;
    script->length = length;
    script->script_p = script->buffer;
    script->lastscript_p = script->buffer;
    script->end_p = &script->buffer[length];
    script->tokenavailable = 0;
    script->line = 1;
    script->lastline = 1;
    SetScriptPunctuations(script, 0);
    memcpy(script->buffer, ptr, length);
    return script;
}

// 0x08051dee  size=42  FUN_08051dee  status=translated  naming=inferred
// Name evidence: frees the punctuation table (if any) then the script (FreeScript counterpart).
void FreeScript(script_t *script)
{
    if (script->punctuationtable) {
        FreeMemory(script->punctuationtable);
    }
    FreeMemory(script);
}

// 0x08051e18  size=49  FUN_08051e18  status=translated  naming=unknown
// Name evidence: none; formats "%s %s" with "pc_1.3_1_1" and the build timestamp "Mon May 01 2006 05:05:43PM" into a global buffer and returns it.
char *Com_FormatBuildVersion(void)
{
    sprintf(com_buildVersionString, "%s %s", "pc_1.3_1_1", "Mon May 01 2006 05:05:43PM");
    return com_buildVersionString;
}

// 0x08051e4c  size=18  FUN_08051e4c  status=translated  naming=unknown
// Name evidence: none; reports whether the BSP file image pointer (0x08188da8) is non-NULL.
int Com_IsBspLoaded(void)
{
    return com_bspData != 0;
}

// 0x08051e5e  size=42  FUN_08051e5e  status=translated  naming=unknown
// Name evidence: none; returns the BSP image pointer and optionally its length and checksum through the two out-pointers.
int *Com_GetBspHeader(int *length, int *checksum)
{
    if (length) {
        *length = com_bspLength;
    }
    if (checksum) {
        *checksum = com_bspChecksum;
    }
    return com_bspData;
}

// 0x08051e88  size=460  FUN_08051e88  status=translated  naming=unknown
// Name evidence: strings "EXE_ERR_COULDNT_LOAD\x15%s", "EXE_ERR_WRONG_MAP_VERSION_NUM\x15%s"; header magic 0x50534249 ("IBSP"), version 4, minimum size 0x140 = 8 + 39 lump entries of 8 bytes. Error code 1 is passed to Com_Error and may return, in which case execution continues as in the original.
void Com_LoadBsp(char *name)
{
    int handle;
    unsigned int nread;
    int i;
    int *header;

    com_bspLength = FS_OpenFileRead(name, &handle, 0);
    if (handle == 0) {
        Com_Error(1, va("EXE_ERR_COULDNT_LOAD\x15%s", name));
    }
    com_bspData = (int *)Com_Malloc(com_bspLength);
    nread = FS_Read(com_bspData, com_bspLength, handle);
    FS_FCloseFile(handle);
    if (nread != (unsigned int)com_bspLength || com_bspLength < 0x140) {
        Com_Free_A(com_bspData);
        Com_Error(1, va("EXE_ERR_COULDNT_LOAD\x15%s", name));
    }
    com_bspChecksum = Com_BlockChecksum(com_bspData, com_bspLength);
    header = com_bspData;
    header[0] = LittleLong(com_bspData[0]);
    header = com_bspData;
    header[1] = LittleLong(com_bspData[1]);
    if (com_bspData[0] != 0x50534249 || com_bspData[1] != 4) {
        Com_Free_A(com_bspData);
        Com_Error(1, va("EXE_ERR_WRONG_MAP_VERSION_NUM\x15%s", name));
    }
    for (i = 0; i != 0x27; i++) {
        int off = LittleLong(com_bspData[i * 2 + 2]);
        com_bspData[i * 2 + 2] = off;
        int len = LittleLong(com_bspData[i * 2 + 3]);
        com_bspData[i * 2 + 3] = len;
    }
}

// 0x08052054  size=31  FUN_08052054  status=translated  naming=unknown
// Name evidence: none; frees the BSP image and clears the pointer.
void Com_UnloadBsp(void)
{
    Com_Free_A(com_bspData);
    com_bspData = 0;
}

// 0x08052074  size=22  FUN_08052074  status=translated  naming=unknown
// Name evidence: none; frees the BSP image only if one is loaded.
void Com_UnloadBspIfLoaded(void)
{
    if (Com_IsBspLoaded()) {
        Com_UnloadBsp();
    }
}

// 0x0805208a  size=8  FUN_0805208a  status=translated  naming=inferred
// Name evidence: identity function applied to every BSP header/lump field, i.e. a little-endian host LittleLong.
int LittleLong(int value)
{
    return value;
}

// 0x08052094  size=205  FUN_08052094  status=translated  naming=unknown
// Name evidence: none; resets one 0x18-byte slot at 0x085b7660 and allocates its buffers (sized from globals 0x08188e24/e0c/e04) plus copies of 0x30 and 0x48 bytes from 0x08188e5c / 0x08188e60.
void CM_InitSlotBuffers(int index)
{
    cm_slot_t *slot = &g_085b7660_cmTraceSlots[index];

    slot->field_00 = 0;
    slot->buf_0c = Hunk_Alloc(g_08188dc0_clipmap.numPartitions * 2);
    slot->buf_04 = Hunk_Alloc(g_08188dc0_clipmap.numEdges << 2);
    slot->buf_08 = Hunk_Alloc(g_08188dc0_clipmap.numVerts << 2);
    slot->copy_10 = (int *)Hunk_Alloc(0x30);
    memcpy(slot->copy_10, g_08188dc0_clipmap.box_brush, 0x30);
    slot->copy_14 = (int *)Hunk_Alloc(0x48);
    memcpy(slot->copy_14, (const char *)&g_08188dc0_clipmap + 0xa0, 0x48);
}

// 0x08052162  size=32  FUN_08052162  status=translated  naming=unknown
// Name evidence: none; initialises both slot buffer sets (indices 0 and 1).
void CM_InitAllSlotBuffers(void)
{
    CM_InitSlotBuffers(0);
    CM_InitSlotBuffers(1);
}

// 0x08052182  size=32  FUN_08052182  status=translated  naming=unknown
// Name evidence: none; helper used by CM_LoadMap: calls CM_LoadMapFromBsp(name, 1) then cm_load_static_models().
void CM_LoadMapData(char *name)
{
    CM_LoadMapFromBsp(name, 1);
    cm_load_static_models();
}

// 0x080521a2  size=103  FUN_080521a2  status=translated  naming=observed
// Name evidence: string "\x15CM_LoadMap: NULL name" passed to the error function (Com_Error) when name is NULL or empty.
void CM_LoadMap(char *name, int *checksum)
{
    if (name == 0 || *name == '\0') {
        Com_Error(1, "\x15" "CM_LoadMap: NULL name");
    }
    if (g_08188dc0_clipmap.name == 0 || strcasecmp(g_08188dc0_clipmap.name, name) != 0) {
        CM_LoadMapData(name);
        CM_InitAllSlotBuffers();
    }
    *checksum = g_08188dc0_clipmap.checksum;
}

// 0x0805220a  size=36  FUN_0805220a  status=translated  naming=inferred
// Name evidence: clears the 0x110-byte clipmap record at 0x08188dc0 (CM_ClearMap counterpart).
void CM_ClearMap(void)
{
    Com_Memset(&g_08188dc0_clipmap, 0, 0x110);
}

// 0x0805222e  size=95  FUN_0805222e  status=translated  naming=unknown
// Name evidence: none; replaces each int of the array by Identity_080527da(value).
void CM_TransformIntArray(int *array, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        array[i] = Identity_080527da(array[i]);
    }
}

// 0x080526e4  size=10  FUN_080526e4  status=translated  naming=unknown
// Name evidence: none; returns global 0x08188e34.
int CM_NumSubModels(void)
{
    return g_08188dc0_clipmap.numSubModels;
}

// 0x080526ee  size=10  FUN_080526ee  status=translated  naming=unknown
// Name evidence: none; returns global 0x08188e58.
int CM_EntityString(void)
{
    return (int)g_08188dc0_clipmap.entityString;
}

// 0x080526f8  size=36  FUN_080526f8  status=translated  naming=unknown
// Name evidence: none; returns the sign-extended 16-bit field at +0x28 of the 0x2c-byte record with the given index (base pointer at 0x08188de8).
int CM_LeafCluster(int index)
{
    return (int)g_08188dc0_clipmap.leafs[index].cluster;
}
