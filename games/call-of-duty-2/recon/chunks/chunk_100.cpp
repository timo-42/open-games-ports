#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_100_types.h"

/*
 * Chunk 100: 67 functions between 0x0804b3cc and 0x08075d58 that Ghidra
 * auto-analysis missed. Bodies were taken from .eh_frame FDE starts and
 * checked against objdump. Naming evidence is listed per function.
 *
 * Global side effects, call order, integer widths and failure paths are kept
 * as in the binary. Where the decompiler output is only a best reading
 * (uninitialised locals, a dead write path), the choice is noted next to the
 * function.
 */

/* ======================================================================== */
/* Precompiler: builtins, #include, global define list.                     */
/* ======================================================================== */

// 0x0804b3cc  size=253  FUN_0804b3cc  status=translated  naming=inferred
// Evidence: strings "__LINE__", "__FILE__", "__DATE__", "__TIME__" are referenced here; inserts the four builtin defines into the source hash (botlib precompiler counterpart).
void PC_AddBuiltinDefines(source_t *source)
{
    static const struct {
        const char *name;
        int32 builtin;
    } builtins[5] = {
        {"__LINE__", 1},
        {"__FILE__", 2},
        {"__DATE__", 3},
        {"__TIME__", 4},
        {NULL, 0},
    };
    int i;

    for (i = 0; builtins[i].name != NULL; i++) {
        size_t len = strlen(builtins[i].name);
        define_t *define = (define_t *)GetMemory((int)(len + 0x21));
        memset(define, 0, 0x20);
        define->name = (char *)define + 0x20;
        strcpy(define->name, builtins[i].name);
        define->flags |= 1;
        define->builtin = builtins[i].builtin;
        PC_AddDefineToHash(define, source->definehash);
    }
}

// 0x0804bcfe  size=638  FUN_0804bcfe  status=translated  naming=inferred
// Evidence: strings "#include without file name", "#include missing trailing >", "file %s not found" are used here; #include handler of the precompiler (botlib counterpart).
int PC_Include(source_t *source)
{
    token_t token;
    char path[64];
    script_t *script;

    if (source->skip > 0) {
        return 1;
    }
    if (PC_ReadSourceToken(source, &token) == 0) {
        PC_SourceError(source, "#include without file name");
        return 0;
    }
    if (token.linescrossed > 0) {
        PC_SourceError(source, "#include without file name");
        return 0;
    }
    if (token.type == TT_STRING) {
        StripDoubleQuotes(token.string);
        PC_ConvertPath(token.string);
        script = LoadScriptFile(token.string);
        if (script == NULL) {
            strcpy(path, source->includepath);
            strcat(path, token.string);
            script = LoadScriptFile(path);
        }
    } else {
        if (token.type != TT_PUNCTUATION || token.string[0] != '<') {
            PC_SourceError(source, "#include without file name");
            return 0;
        }
        strcpy(path, source->includepath);
        while (PC_ReadSourceToken(source, &token) != 0) {
            if (token.linescrossed > 0) {
                PC_UnreadSourceToken(source, &token);
                break;
            }
            if (token.type == TT_PUNCTUATION && token.string[0] == '>') {
                break;
            }
            strncat(path, token.string, 0x40);
        }
        if (token.string[0] != '>') {
            PC_SourceWarning(source, "#include missing trailing >");
        }
        if (path[0] == '\0') {
            PC_SourceError(source, "#include without file name between < >");
            return 0;
        }
        PC_ConvertPath(path);
        script = LoadScriptFile(path);
    }
    if (script == NULL) {
        PC_SourceError(source, "file %s not found", path);
        return 0;
    }
    PC_PushScript(source, script);
    return 1;
}

// 0x0804ca12  size=67  FUN_0804ca12  status=translated  naming=inferred
// Evidence: parses a define from a string and links it at the head of the global define list (botlib addglobaldefine counterpart).
int PC_AddGlobalDefine(const char *string)
{
    define_t *define = PC_DefineFromString(string);
    if (define != NULL) {
        define->next = pc_globaldefines;
        pc_globaldefines = define;
    }
    return define != NULL;
}

// 0x0804ca56  size=67  FUN_0804ca56  status=translated  naming=inferred
// Evidence: finds a define by name in the global list and frees it; the freed define is not unlinked from the list (same as the binary).
int PC_RemoveGlobalDefine(const char *name)
{
    define_t *define = PC_FindDefine(pc_globaldefines, name);
    if (define != NULL) {
        PC_FreeDefine(define);
    }
    return define != NULL;
}

// 0x0804ca9a  size=58  FUN_0804ca9a  status=translated  naming=inferred
// Evidence: walks the global define list via next (+0x18), freeing each entry.
void PC_RemoveAllGlobalDefines(void)
{
    while (pc_globaldefines != NULL) {
        define_t *define = pc_globaldefines;
        pc_globaldefines = define->next;
        PC_FreeDefine(define);
    }
}

// 0x0804cd92  size=27  FUN_0804cd92  status=translated  naming=inferred
// Evidence: forwards to PC_DirectiveIfdefCommon with type 0x10 (the #ifndef code path). Ghidra shows void; the disassembly returns the callee result in EAX, so the value is returned here.
int PC_DirectiveIfndef(source_t *source)
{
    return PC_DirectiveIfdefCommon(source, 0x10);
}

// 0x0804cdae  size=144  FUN_0804cdae  status=translated  naming=inferred
// Evidence: strings "misplaced #else" and "#else after #else" referenced here (#else handler).
int PC_DirectiveElse(source_t *source)
{
    int type = 0;
    int skip = 0;

    PC_PopIndent(source, &type, &skip);
    if (type == 0) {
        PC_SourceError(source, "misplaced #else");
        return 0;
    }
    if (type == INDENT_ELSE) {
        PC_SourceError(source, "#else after #else");
        return 0;
    }
    PC_PushIndent(source, INDENT_ELSE, skip == 0);
    return 1;
}

// 0x0804ce3e  size=77  FUN_0804ce3e  status=translated  naming=inferred
// Evidence: string "misplaced #endif" referenced here (#endif handler).
int PC_DirectiveEndif(source_t *source)
{
    int type = 0;
    int skip = 0;

    PC_PopIndent(source, &type, &skip);
    if (type == 0) {
        PC_SourceError(source, "misplaced #endif");
    }
    return type != 0;
}

// 0x0804e6c4  size=104  FUN_0804e6c4  status=translated  naming=inferred
// Evidence: evaluates the #if expression and pushes an INDENT_IF entry (botlib #if counterpart).
int PC_DirectiveIf(source_t *source)
{
    int value = 0;
    int ok = PC_Evaluate(source, &value, NULL, 1);

    if (ok != 0) {
        PC_PushIndent(source, INDENT_IF, value == 0);
    }
    return ok != 0;
}

// 0x0804e72c  size=32  FUN_0804e72c  status=translated  naming=inferred
// Evidence: string "#line directive not supported" referenced here.
int PC_DirectiveLine(source_t *source)
{
    PC_SourceError(source, "#line directive not supported");
    return 0;
}

// 0x0804e74c  size=88  FUN_0804e74c  status=translated  naming=inferred
// Evidence: string "#error directive: %s" referenced here; reads one token into a zeroed buffer and reports it.
int PC_DirectiveError(source_t *source)
{
    token_t token;

    strcpy(token.string, "");
    PC_ReadSourceToken(source, &token);
    PC_SourceError(source, "#error directive: %s", token.string);
    return 0;
}

// 0x0804e7a4  size=60  FUN_0804e7a4  status=translated  naming=inferred
// Evidence: string "#pragma directive not supported" referenced here; drains the rest of the line.
int PC_DirectivePragma(source_t *source)
{
    token_t token;
    int ok;

    PC_SourceWarning(source, "#pragma directive not supported");
    do {
        ok = PC_ReadLine(source, &token);
    } while (ok != 0);
    return 1;
}

// 0x0804f09c  size=163  FUN_0804f09c  status=translated  naming=inferred
// Evidence: strings "couldn't find expected %s" and "expected %s, found %s" referenced here (PC_ExpectTokenString counterpart).
int PC_ExpectTokenString(source_t *source, const char *string)
{
    token_t token;

    if (PC_ReadToken(source, &token) == 0) {
        PC_SourceError(source, "couldn't find expected %s", string);
        return 0;
    }
    if (strcmp(token.string, string) == 0) {
        return 1;
    }
    PC_SourceError(source, "expected %s, found %s", string, token.string);
    return 0;
}

// 0x0804f140  size=735  FUN_0804f140  status=translated  naming=inferred
// Evidence: strings "expected a %s, found %s", "decimal", "hex", "octal", "binary", "expected %s, found %s" referenced here (PC_ExpectTokenType counterpart).
// Note: the description buffer is initialised to "" before the flag strings are appended; the binary's first strcat relied on an uninitialised buffer on the path where no base flag is set.
int PC_ExpectTokenType(source_t *source, int type, unsigned int subtype, token_t *token)
{
    char desc[1032];

    if (PC_ReadToken(source, token) == 0) {
        PC_SourceError(source, "couldn't read expected token");
        return 0;
    }
    if (token->type == type) {
        if (type == TT_NUMBER) {
            if (((unsigned int)token->subtype & subtype) != subtype) {
                strcpy(desc, "");
                if (subtype & 0x8) {
                    strcpy(desc, "decimal");
                }
                if (subtype & 0x100) {
                    strcpy(desc, "hex");
                }
                if (subtype & 0x200) {
                    strcpy(desc, "octal");
                }
                if (subtype & 0x400) {
                    strcpy(desc, "binary");
                }
                if (subtype & 0x2000) {
                    strcat(desc, " long");
                }
                if (subtype & 0x4000) {
                    strcat(desc, " unsigned");
                }
                if (subtype & 0x800) {
                    strcat(desc, " float");
                }
                if (subtype & 0x1000) {
                    strcat(desc, " integer");
                }
                PC_SourceError(source, "expected %s, found %s", desc, token->string);
                return 0;
            }
        } else if (type == TT_PUNCTUATION && (unsigned int)token->subtype != subtype) {
            PC_SourceError(source, "found %s", token->string);
            return 0;
        }
        return 1;
    }

    strcpy(desc, "");
    if (type == TT_STRING) {
        strcpy(desc, "string");
    }
    if (type == TT_LITERAL) {
        strcpy(desc, "literal");
    }
    if (type == TT_NUMBER) {
        strcpy(desc, "number");
    }
    if (type == TT_NAME) {
        strcpy(desc, "name");
    }
    if (type == TT_PUNCTUATION) {
        strcpy(desc, "punctuation");
    }
    PC_SourceError(source, "expected a %s, found %s", desc, token->string);
    return 0;
}

// 0x0804f420  size=68  FUN_0804f420  status=translated  naming=inferred
// Evidence: string "couldn't read expected token" referenced here (PC_ExpectAnyToken counterpart).
int PC_ExpectAnyToken(source_t *source, token_t *token)
{
    int ok = PC_ReadToken(source, token);
    if (ok == 0) {
        PC_SourceError(source, "couldn't read expected token");
    }
    return ok != 0;
}

// 0x0804f570  size=92  FUN_0804f570  status=translated  naming=inferred
// Evidence: strcmp loop over PC_ReadToken results (PC_SkipUntilString counterpart).
int PC_SkipUntilString(source_t *source, const char *string)
{
    token_t token;

    do {
        if (PC_ReadToken(source, &token) == 0) {
            return 0;
        }
    } while (strcmp(token.string, string) != 0);
    return 1;
}

// 0x0804f5cc  size=31  FUN_0804f5cc  status=translated  naming=inferred
// Evidence: pushes back the token stored at source+0x9c (PC_UnreadLastToken counterpart).
void PC_UnreadLastToken(source_t *source)
{
    PC_UnreadSourceToken(source, &source->token);
}

// 0x0804f770  size=245  FUN_0804f770  status=translated  naming=inferred
// Evidence: string-less; builds a source record around a script loaded from memory with a 0x1000-byte define hash (PC_LoadSourceFile counterpart).
source_t *PC_LoadSourceMemory(void *ptr, int length, char *filename)
{
    script_t *script;
    source_t *source;

    script = LoadScriptMemory(ptr, length, filename);
    if (script == NULL) {
        return NULL;
    }
    script->next = NULL;
    source = (source_t *)GetMemory(0x4c8);
    memset(source, 0, 0x4c8);
    strncpy(source->filename, filename, 0x40);
    source->scriptstack = script;
    source->definehash = (define_t **)GetClearedMemory(0x1000);
    PC_AddGlobalDefinesToSource(source);
    return source;
}

// 0x0804fa56  size=95  FUN_0804fa56  status=translated  naming=inferred
// Evidence: validates a handle in 1..63 and frees the source stored in g_08188ca0_sourceFiles.
int PC_FreeSourceHandle(int handle)
{
    if (handle < 1 || handle > MAX_SOURCEFILES - 1) {
        return 0;
    }
    if (g_08188ca0_sourceFiles[handle] == NULL) {
        return 0;
    }
    FreeSource(g_08188ca0_sourceFiles[handle]);
    g_08188ca0_sourceFiles[handle] = NULL;
    return 1;
}

// 0x0804fab6  size=198  FUN_0804fab6  status=translated  naming=inferred
// Evidence: reads a token from a handle and copies it into a pc_token record (botlib PC_ReadTokenHandle counterpart).
// Note: the token is zero-initialised first; the binary copies the local token even when the read fails, where it reads indeterminate stack data.
int PC_ReadTokenHandle(int handle, pc_token_t *pc_token)
{
    token_t token;
    int ok;

    if (handle < 1 || handle > MAX_SOURCEFILES - 1 || g_08188ca0_sourceFiles[handle] == NULL) {
        return 0;
    }
    memset(&token, 0, sizeof(token));
    ok = PC_ReadToken(g_08188ca0_sourceFiles[handle], &token);
    strcpy(pc_token->string, token.string);
    pc_token->type = token.type;
    pc_token->subtype = token.subtype;
    pc_token->intvalue = (int32)token.intvalue;
    pc_token->floatvalue = (float)token.floatvalue;
    if (pc_token->type == TT_STRING) {
        StripDoubleQuotes(pc_token->string);
    }
    return ok;
}

// 0x0804fb7c  size=198  FUN_0804fb7c  status=translated  naming=inferred
// Evidence: line-oriented variant of PC_ReadTokenHandle using PC_ReadLine.
// Note: same zero-initialisation choice as PC_ReadTokenHandle.
int PC_ReadLineHandle(int handle, pc_token_t *pc_token)
{
    token_t token;
    int ok;

    if (handle < 1 || handle > MAX_SOURCEFILES - 1 || g_08188ca0_sourceFiles[handle] == NULL) {
        return 0;
    }
    memset(&token, 0, sizeof(token));
    ok = PC_ReadLine(g_08188ca0_sourceFiles[handle], &token);
    strcpy(pc_token->string, token.string);
    pc_token->type = token.type;
    pc_token->subtype = token.subtype;
    pc_token->intvalue = (int32)token.intvalue;
    pc_token->floatvalue = (float)token.floatvalue;
    if (pc_token->type == TT_STRING) {
        StripDoubleQuotes(pc_token->string);
    }
    return ok;
}

// 0x0804fcdc  size=75  FUN_0804fcdc  status=translated  naming=inferred
// Evidence: string "^1Error: file %s still open in precompiler" is printed here for each open handle.
void PC_CheckOpenSourceHandles(void)
{
    int i;

    for (i = 1; i < MAX_SOURCEFILES; i++) {
        if (g_08188ca0_sourceFiles[i] != NULL) {
            Com_Printf("^1Error: file %s still open in precompiler\n",
                       g_08188ca0_sourceFiles[i]->scriptstack->filename);
        }
    }
}

/* ======================================================================== */
/* Script (PS_) token helpers.                                              */
/* ======================================================================== */

// 0x0805173e  size=136  FUN_0805173e  status=translated  naming=inferred
// Evidence: reads a token, keeps it only if its type and subtype mask match, otherwise rewinds the script pointer (PS_TryReadTokenType counterpart).
int PS_TryReadTokenType(script_t *script, int type, unsigned int subtype, token_t *out)
{
    token_t token;
    int ok;

    ok = PS_ReadToken(script, &token);
    if (ok == 0) {
        return 0;
    }
    if (token.type == type && (subtype & (unsigned int)token.subtype) == subtype) {
        memcpy(out, &token, 0x42c);
        return 1;
    }
    script->script_p = script->lastscript_p;
    return 0;
}

// 0x080517c6  size=92  FUN_080517c6  status=translated  naming=inferred
// Evidence: strcmp loop over PS_ReadToken results (PS_SkipUntilString counterpart).
int PS_SkipUntilString(script_t *script, const char *string)
{
    token_t token;

    do {
        if (PS_ReadToken(script, &token) == 0) {
            return 0;
        }
    } while (strcmp(token.string, string) != 0);
    return 1;
}

// 0x08051928  size=209  FUN_08051928  status=translated  naming=inferred
// Evidence: string "expected float value, found %s\n" referenced here (PS_ParseFloat counterpart).
long double PS_ParseFloat(script_t *script)
{
    token_t token;
    long double sign = 1.0L;

    PS_ExpectAnyToken(script, &token);
    if (strcmp(token.string, "-") == 0) {
        sign = -1.0L;
        PS_ExpectTokenType(script, TT_NUMBER, 0, &token);
    } else if (token.type != TT_NUMBER) {
        ScriptError(script, "expected float value, found %s\n", token.string);
    }
    return token.floatvalue * sign;
}

// 0x080519fa  size=173  FUN_080519fa  status=translated  naming=inferred
// Evidence: string "expected integer value, found %s\n" referenced here (PS_ParseInt counterpart).
int PS_ParseInt(script_t *script)
{
    token_t token;
    int sign = 1;

    PS_ExpectAnyToken(script, &token);
    if (strcmp(token.string, "-") == 0) {
        sign = -1;
        PS_ExpectTokenType(script, TT_NUMBER, TT_INTEGER, &token);
    } else if (token.type != TT_NUMBER || token.subtype == TT_FLOAT) {
        ScriptError(script, "expected integer value, found %s\n", token.string);
    }
    return sign * (int32)token.intvalue;
}

/* ======================================================================== */
/* Geometry and collision helpers.                                          */
/* ======================================================================== */

// 0x0805228e  size=1110  FUN_0805228e  status=translated  naming=inferred
// Evidence: strings "EXE_ERR_COULDNT_LOAD\x15%s", "bad bsp version %d", "Failed to open file %s for writing" referenced here. Rewrites a BSP file with one lump replaced (lump offsets recomputed, 4-byte aligned).
// Note: the output file is opened through FS_ReturnZeroStub (0x080a0d54), which returns 0 in this build, so the write path always reaches the "Failed to open file" error; the rest of the routine is kept as recovered.
void CM_ReplaceBspLump(int lumpIndex, const void *lumpData, int lumpLength, uint32 *checksumOut)
{
    const int lumpCount = 0x27;
    int32 hdr[80];        /* header words: hdr[2+2i] = lump length, hdr[3+2i] = lump offset */
    int32 origHdr[80];    /* copy of the header before the lump table is rebuilt */
    int32 fileRec[3];     /* [0] file handle, [1] zero word for padding, [2] padding count */
    int32 fileLength;
    int32 headerBytes;
    int32 newOffset;
    int32 version;
    byte *fileBuf;
    const char *path;
    int i;

    path = *(const char **)&g_08188dc0_clipmap; /* clipMap_s.name at +0 */
    fileLength = FS_OpenFileRead(path, &fileRec[0], 0);
    if (fileRec[0] == 0) {
        Com_Error(1, "EXE_ERR_COULDNT_LOAD\x15%s", path);
    }
    fileBuf = (byte *)Com_MallocZeroed((unsigned int)(fileLength + 1));
    FS_Read(fileBuf, fileLength, fileRec[0]);
    fileBuf[fileLength] = 0;
    FS_FCloseFile(fileRec[0]);

    memcpy(hdr, fileBuf, 0x140);
    version = Identity_080527da(hdr[1]);
    if (version == 4 || version == 0x3d) {
        CM_TransformIntArray(hdr, lumpCount);
        memcpy(origHdr, hdr, 0x140);
        fileRec[0] = FS_ReturnZeroStub();
        if (fileRec[0] == 0) {
            Com_Error(1, "Failed to open file %s for writing", path);
        } else {
            headerBytes = lumpCount * 8 + 8;
            newOffset = headerBytes;
            for (i = 0; i < lumpCount; i++) {
                if (i == lumpIndex) {
                    hdr[2 + 2 * i] = lumpLength;
                }
                hdr[3 + 2 * i] = newOffset;
                newOffset = newOffset + (int32)(((uint32)hdr[2 + 2 * i] + 3U) & 0xfffffffcU);
            }
            CM_TransformIntArray(hdr, lumpCount);
            FS_Write(hdr, headerBytes, fileRec[0]);
            CM_TransformIntArray(hdr, lumpCount);

            fileRec[1] = 0;
            for (i = 0; i < lumpCount; i++) {
                if (hdr[2 + 2 * i] != 0) {
                    const void *src;
                    if (i == lumpIndex) {
                        src = lumpData;
                    } else {
                        src = fileBuf + origHdr[3 + 2 * i];
                    }
                    FS_Write(src, hdr[2 + 2 * i], fileRec[0]);
                    fileRec[2] = (int32)(((uint32)hdr[2 + 2 * i] + 3U) & 0xfffffffcU) - hdr[2 + 2 * i];
                    if (fileRec[2] != 0) {
                        FS_Write(&fileRec[1], fileRec[2], fileRec[0]);
                    }
                }
            }
            FS_FCloseFile(fileRec[0]);
            Com_Free_A(fileBuf);
            if (checksumOut != NULL) {
                void *readBuf = NULL;
                int32 readLength = FS_ReadFile(path, &readBuf);
                *checksumOut = Com_BlockChecksum(readBuf, readLength);
                FS_FreeFile(readBuf);
            }
        }
    } else {
        Com_Error(1, "bad bsp version %d", hdr[1]);
    }
}

// 0x08056006  size=389  FUN_08056006  status=translated  naming=inferred
// Evidence: Moller-Trumbore style ray/triangle test (edge cross products, barycentric u/v, t); epsilon 0.001f from rodata. No name string.
int Ray_IntersectTriangle(float *origin, float *dir, float *v0, float *v1, float *v2,
                          float *t_out, float *u_out, float *v_out)
{
    float edge1[3];
    float edge2[3];
    float pvec[3];
    float tvec[3];
    float qvec[3];
    float det;
    float u;
    float v;
    float t;

    VectorSubtract(v1, v0, edge1);
    VectorSubtract(v2, v0, edge2);
    Vec3_Cross(dir, edge2, pvec);
    det = (float)DotProduct(edge1, pvec);
    if (det < 0.0010000000474974513f) {
        return 0;
    }
    VectorSubtract(origin, v0, tvec);
    u = (float)DotProduct(tvec, pvec);
    if (u < 0.0f || det < u) {
        return 0;
    }
    Vec3_Cross(tvec, edge1, qvec);
    v = (float)DotProduct(dir, qvec);
    if (v < 0.0f || det < u + v) {
        return 0;
    }
    t = (float)((long double)DotProduct(edge2, qvec) / (long double)det);
    *t_out = t;
    if (u_out != NULL) {
        *u_out = u / det;
    }
    if (v_out != NULL) {
        *v_out = v / det;
    }
    return 1;
}

// 0x0805785a  size=635  FUN_0805785a  status=translated  naming=observed
// Evidence: string "BaseWindingForPlane: no axis found" (rodata 0x08140060) is referenced by this function; builds a 4-point square of half-extent 131072 in the plane (normal, dist).
int32 *BaseWindingForPlane(float *normal, float dist)
{
    float best = -131072.0f;
    int bestAxis = -1;
    float up[3];
    float org[3];
    float right[3];
    float dotv;
    float fabsv;
    int32 *block;
    float *v;
    int i;

    for (i = 0; i < 3; i++) {
        fabsv = cm_fabs(normal[i]);
        if (best < fabsv) {
            bestAxis = i;
            best = fabsv;
        }
    }
    if (bestAxis == -1) {
        Com_Error(1, "\x15" "BaseWindingForPlane: no axis found");
    }
    Vec3_CopyBits_08057fd0(vec3_origin, up);
    if (-1 < bestAxis) {
        if (bestAxis < 2) {
            up[2] = 1.0f;
        } else if (bestAxis == 2) {
            up[0] = 1.0f;
        }
    }
    dotv = Vec3_Dot_08058114(up, normal);
    Vec3_MA_080580c0(up, -dotv, normal, up);
    Vec3_NormalizeCopy(up, up);
    Vec3_Scale_08058088(normal, dist, org);
    Vec3_Cross(up, normal, right);
    Vec3_Scale_08058088(up, 131072.0f, up);
    Vec3_Scale_08058088(right, 131072.0f, right);

    block = (int32 *)AllocWinding(4);
    v = (float *)(block + 1);
    block[0] = 4;
    Vec3_Sub_08058044(org, right, v + 0);
    Vec3_Add_08058000(v + 0, up, v + 0);
    Vec3_Add_08058000(org, right, v + 3);
    Vec3_Add_08058000(v + 3, up, v + 3);
    Vec3_Add_08058000(org, right, v + 6);
    Vec3_Sub_08058044(v + 6, up, v + 6);
    Vec3_Sub_08058044(org, right, v + 9);
    Vec3_Sub_08058044(v + 9, up, v + 9);
    return block;
}

// 0x08057ad6  size=72  FUN_08057ad6  status=translated  naming=inferred
// Evidence: allocates a block with the source's point count and copies count*12+4 bytes (cm block clone).
int32 *CM_CloneBlock(int32 *src)
{
    int32 *dst = (int32 *)AllocWinding((uint32)src[0]);
    Com_Memcpy(dst, src, (size_t)(src[0] * 12 + 4));
    return dst;
}

// 0x08057b1e  size=1176  FUN_08057b1e  status=translated  naming=observed
// Evidence: strings "ClipWinding: points exceeded estimate" (0x081400a0) and "ClipWinding: MAX_POINTS_ON_WINDING" (0x081400e0) are referenced here; Sutherland-Hodgman clip of a point block against a plane, keeping the front side.
void ClipWinding(int32 **blockRef, float *normal, float dist, float epsilon)
{
    int32 *block = *blockRef;
    int32 sideCount[3];
    int32 side[68];
    float dists[71];
    float out[3];
    int32 origCount;
    int32 *newblock;
    int32 i;
    int32 k;

    sideCount[0] = 0;
    sideCount[1] = 0;
    sideCount[2] = 0;
    for (i = 0; i < block[0]; i++) {
        float *vert = (float *)(block + i * 3 + 1);
        g_08188ef8_clipWindingScratch = Vec3_Dot_08058114(vert, normal);
        g_08188ef8_clipWindingScratch = g_08188ef8_clipWindingScratch - dist;
        dists[i] = g_08188ef8_clipWindingScratch;
        if (epsilon < g_08188ef8_clipWindingScratch) {
            side[i] = 0;
        } else if (g_08188ef8_clipWindingScratch < -epsilon) {
            side[i] = 1;
        } else {
            side[i] = 2;
        }
        sideCount[side[i]] = sideCount[side[i]] + 1;
    }
    side[i] = side[0];
    dists[i] = dists[0];

    if (sideCount[0] == 0) {
        FreeWinding(block);
        *blockRef = NULL;
    } else if (sideCount[1] != 0) {
        origCount = block[0];
        newblock = (int32 *)AllocWinding((uint32)(origCount + 4));
        for (i = 0; i < block[0]; i++) {
            float *vert = (float *)(block + i * 3 + 1);
            if (side[i] == 2) {
                Vec3_CopyBits_08057fd0(vert, (float *)(newblock + newblock[0] * 3 + 1));
                newblock[0] = newblock[0] + 1;
            } else {
                if (side[i] == 0) {
                    Vec3_CopyBits_08057fd0(vert, (float *)(newblock + newblock[0] * 3 + 1));
                    newblock[0] = newblock[0] + 1;
                }
                if (side[i + 1] != 2 && side[i + 1] != side[i]) {
                    float *nextVert = (float *)(block + ((i + 1) % block[0]) * 3 + 1);
                    g_08188ef8_clipWindingScratch = dists[i] / (dists[i] - dists[i + 1]);
                    for (k = 0; k < 3; k++) {
                        if (normal[k] == 1.0f) {
                            out[k] = dist;
                        } else if (normal[k] == -1.0f) {
                            out[k] = -dist;
                        } else {
                            out[k] = vert[k] + (nextVert[k] - vert[k]) * g_08188ef8_clipWindingScratch;
                        }
                    }
                    Vec3_CopyBits_08057fd0(out, (float *)(newblock + newblock[0] * 3 + 1));
                    newblock[0] = newblock[0] + 1;
                }
            }
        }
        if (origCount + 4 < newblock[0]) {
            Com_Error(1, "\x15" "ClipWinding: points exceeded estimate");
        }
        if (0x40 < newblock[0]) {
            Com_Error(1, "\x15" "ClipWinding: MAX_POINTS_ON_WINDING");
        }
        FreeWinding(block);
        *blockRef = newblock;
    }
}

// 0x08058148  size=19  FUN_08058148  status=translated  naming=inferred
// Evidence: forwards to Hunk_Alloc and discards the pointer (reserve-only wrapper; Hunk_Alloc forwards to Hunk_AllocAlign(size, 0x20)).
void CM_HunkReserveA(int size)
{
    (void)Hunk_Alloc(size);
}

// 0x0805815c  size=19  FUN_0805815c  status=translated  naming=inferred
// Evidence: identical to CM_HunkReserveA (same Hunk_Alloc call, result discarded); a second entry of the same reserve wrapper.
void CM_HunkReserveB(int size)
{
    (void)Hunk_Alloc(size);
}

/* ======================================================================== */
/* Console commands.                                                        */
/* ======================================================================== */

// 0x0805fc62  size=55  FUN_0805fc62  status=translated  naming=inferred
// Evidence: sets the wait counter from the first argument (atoi) or to 1 when no argument is given; the name is the merged symbol (inferred from the Cmd_Wait counterpart).
void Cmd_Wait_f(void)
{
    if (Cmd_Argc() == 2) {
        g_cmd_wait = atoi(Cmd_Argv(1));
    } else {
        g_cmd_wait = 1;
    }
}

// 0x080600f6  size=159  FUN_080600f6  status=translated  naming=inferred
// Evidence: name from the merged symbol Cmd_Vstr_f (registered as "vstr" by Cmd_Init); the literal name is not in the binary. Strings "%s doesn't exist\n", "%s is not a string-based dvar\n", "vstr <variablename> : execute a variable command\n" referenced here.
void Cmd_Vstr_f(void)
{
    if (Cmd_Argc() == 2) {
        const char *name = Cmd_Argv(1);
        struct dvar_s *d = Dvar_Lookup(name);
        if (d == NULL) {
            Com_Printf("%s doesn't exist\n", name);
        } else if (d->type == DVAR_TYPE_STRING || d->type == DVAR_TYPE_ENUM) {
            Cbuf_InsertText(va("%s\n", d->current.string));
        } else {
            Com_Printf("%s is not a string-based dvar\n", d->name);
        }
    } else {
        Com_Printf("vstr <variablename> : execute a variable command\n");
    }
}

// 0x08060196  size=73  FUN_08060196  status=translated  naming=unknown
// Evidence: merged symbol Cmd_PrintArgs_f (confidence unknown); prints arguments 1..argc-1 as "^3<arg> " followed by a newline.
void Cmd_PrintArgs_f(void)
{
    int i = 1;

    while (Cmd_Argc() > i) {
        Com_Printf("^3%s ", Cmd_Argv(i));
        i = i + 1;
    }
    Com_Printf("\n");
}

// 0x0806031c  size=42  FUN_0806031c  status=translated  naming=inferred
// Evidence: copies Cmd_ArgsFrom(1) into a caller buffer with Q_strncpyz (argument-string helper).
void Cmd_CopyArgs(char *dest, int destsize)
{
    Q_strncpyz(dest, Cmd_ArgsFrom(1), destsize);
}

// 0x08060808  size=48  FUN_08060808  status=translated  naming=inferred
// Evidence: walks the cmd_functions list (next at +0) and calls the callback with each name (+0x04).
void Cmd_ForEachName(void *callbackPtr)
{
    cod2_name_cb_t callback = (cod2_name_cb_t)callbackPtr;
    struct cmd_function_s *cmd;

    for (cmd = cmd_functions; cmd != NULL; cmd = cmd->next) {
        callback(cmd->name);
    }
}

// 0x08060838  size=110  FUN_08060838  status=translated  naming=inferred
// Evidence: looks up a command, then lists files with its path (aux8) and extension (auxC) through FS_ListFiles (file-completion helper).
// Note: the command lookup is not NULL-checked, as in the binary.
char **Cmd_FileCompletion(const char *name, int *numfiles, int unusedArg)
{
    struct cmd_function_s *cmd;

    *numfiles = 0;
    cmd = Cmd_FindCommand(name);
    if (cmd->autoCompleteDir == NULL || cmd->autoCompleteExt == NULL) {
        return NULL;
    }
    return FS_ListFiles((const char *)cmd->autoCompleteDir, (const char *)cmd->autoCompleteExt, 0, numfiles, unusedArg);
}

// 0x080609e0  size=162  FUN_080609e0  status=translated  naming=inferred
// Evidence: name from the merged symbol Cmd_List_f (registered as "cmdlist" by Cmd_Init); the literal name is not in the binary. Prints each matching command name and "%i commands".
void Cmd_List_f(void)
{
    char *filter = NULL;
    int count = 0;
    struct cmd_function_s *cmd;

    if (Cmd_Argc() >= 2) {
        filter = (char *)Cmd_Argv(1);
    }
    for (cmd = cmd_functions; cmd != NULL; cmd = cmd->next) {
        if (filter == NULL || Com_Filter(filter, cmd->name, 0) != 0) {
            Com_Printf("%s\n", cmd->name);
            count = count + 1;
        }
    }
    Com_Printf("%i commands\n", count);
}

// 0x08061bd2  size=80  FUN_08061bd2  status=translated  naming=inferred
// Evidence: loops Sys_GetEvent until an event of type 0 is returned, queuing each other event with Com_PushEvent (system event drain). Ghidra's stack-frame artefacts are dropped.
void Com_DrainSysEvents(void)
{
    struct sysEvent_s ev;
    struct sysEvent_s queued;

    for (;;) {
        ev = Sys_GetEvent();
        queued = ev;
        if (queued.evType == 0) {
            break;
        }
        Com_PushEvent(&queued);
    }
}

// 0x08061c22  size=60  FUN_08061c22  status=translated  naming=inferred
// Evidence: strings "\x15Testing fatal error" and "\x15Testing drop error" referenced here (error self-test command, Com_Error_f counterpart).
void Com_Error_f(void)
{
    if (Cmd_Argc() < 2) {
        Com_Error(0, "\x15" "Testing fatal error");
    } else {
        Com_Error(1, "\x15" "Testing drop error");
    }
}

// 0x08061c5e  size=109  FUN_08061c5e  status=translated  naming=inferred
// Evidence: string "freeze <seconds>\n" referenced here; busy-waits on the clock returned by Com_PumpSysEvents (0.001 per tick, double constant at 0x08140760).
void Cmd_Freeze_f(void)
{
    if (Cmd_Argc() == 2) {
        double seconds = atof(Cmd_Argv(1));
        int start = Com_PumpSysEvents();
        int now;
        do {
            now = Com_PumpSysEvents();
        } while (0.001 * (double)(now - start) <= (double)(float)seconds);
    } else {
        Com_Printf("freeze <seconds>\n");
    }
}

// 0x0806246e  size=124  FUN_0806246e  status=translated  naming=inferred
// Evidence: string "Usage: writeconfig <filename>\n" and "Writing %s.\n" referenced here; the default extension is ".cfg" (rodata 0x08140a3f).
void Cmd_WriteConfig_f(void)
{
    char path[72];

    if (Cmd_Argc() == 2) {
        Q_strncpyz(path, Cmd_Argv(1), 0x40);
        COM_DefaultExtension(path, 0x40, ".cfg");
        Com_Printf("Writing %s.\n", path);
        Com_WriteConfigToFile(path);
    } else {
        Com_Printf("Usage: writeconfig <filename>\n");
    }
}

// 0x080624ea  size=124  FUN_080624ea  status=translated  naming=inferred
// Evidence: string "Usage: writedefaults <filename>\n" and "Writing %s.\n" referenced here; same shape as Cmd_WriteConfig_f.
void Cmd_WriteDefaults_f(void)
{
    char path[72];

    if (Cmd_Argc() == 2) {
        Q_strncpyz(path, Cmd_Argv(1), 0x40);
        COM_DefaultExtension(path, 0x40, ".cfg");
        Com_Printf("Writing %s.\n", path);
        Com_WriteDefaultsToFile(path);
    } else {
        Com_Printf("Usage: writedefaults <filename>\n");
    }
}

// 0x080627dc  size=18  FUN_080627dc  status=translated  naming=inferred
// Evidence: stores Sys_Milliseconds into com_lastFrameTime (merged global name, inferred).
void Com_SetLastFrameTime(void)
{
    com_lastFrameTime = Sys_Milliseconds();
}

// 0x08062964  size=27  FUN_08062964  status=translated  naming=unknown
// Evidence: allocates a block for the given object through XAnim_AllocBlock with the Com_Call_0807706a_With5 allocator (callee not yet named).
void XAnim_CreateInstance(int param)
{
    XAnim_AllocBlock((struct XAnimNodeTable *)(unsigned long)param, (void *(*)(int))Com_Call_0807706a_With5);
}

// 0x08062980  size=27  FUN_08062980  status=translated  naming=unknown
// Evidence: releases the block through XAnim_ReinitBlock with the StrPool_FreeBlock release callback (callee not yet named).
void XAnim_ReleaseInstance(int *param)
{
    XAnim_ReinitBlock((void **)param, (void (*)(void *, int))StrPool_FreeBlock);
}

// 0x08062d34  size=90  FUN_08062d34  status=translated  naming=inferred
// Evidence: for each non-zero entry of the 0x480-entry slot table (g_dobjClientHandleToSlot), clears the DObj skeleton at slot base + index*100 (DObj_ClearSkel).
void DObj_ClearAllSkel(void)
{
    int i;

    for (i = 0; i < 0x480; i++) {
        if (g_dobjClientHandleToSlot[i] != 0) {
            DObj_ClearSkel((struct DObj *)(g_dobjPool + (int)g_dobjClientHandleToSlot[i] * 100));
        }
    }
}

// 0x08062f78  size=48  FUN_08062f78  status=translated  naming=inferred
// Evidence: walks the dvar list (next at +0x1c) and calls the callback with each name (+0).
void Dvar_ForEachName(void *callbackPtr)
{
    cod2_name_cb_t callback = (cod2_name_cb_t)callbackPtr;
    struct dvar_s *d;

    for (d = dvar_vars; d != NULL; d = d->next) {
        callback(d->name);
    }
}

// 0x08063558  size=88  FUN_08063558  status=translated  naming=inferred
// Evidence: string "%s toggled to %s\n" referenced here; toggles the named dvar through Dvar_Toggle_f and prints its new value.
void Cmd_ToggleDvar_f(void)
{
    if (Dvar_Toggle_f() != 0) {
        const char *name = Cmd_Argv(1);
        struct dvar_s *d = Dvar_Lookup(name);
        const char *value = Dvar_FormatCurrentValue(d);
        Com_Printf("%s toggled to %s\n", name, value);
    }
}

// 0x08063650  size=204  FUN_08063650  status=translated  naming=inferred
// Evidence: strings "USAGE: %s <name> <default>\n" and "dvar '%s' is not a boolean dvar\n" referenced here (register-bool command).
void Cmd_RegisterBool_f(void)
{
    if (Cmd_Argc() == 3) {
        const char *name = Cmd_Argv(1);
        int value = atoi(Cmd_Argv(2));
        struct dvar_s *d = Dvar_Lookup(name);
        if (d == NULL || (d->type == DVAR_TYPE_STRING && (d->flags & DVAR_CMD_BOOL_FLAG) != 0)) {
            Dvar_RegisterBool(name, (int8)(value != 0), (uint16)DVAR_CMD_BOOL_FLAG);
        } else if (d->type != DVAR_TYPE_BOOL) {
            Com_Printf("dvar '%s' is not a boolean dvar\n", d->name);
        }
    } else {
        Com_Printf("USAGE: %s <name> <default>\n", Cmd_Argv(0));
    }
}

// 0x0806371c  size=313  FUN_0806371c  status=translated  naming=inferred
// Evidence: strings "USAGE: %s <name> <default> <min> <max>\n", "dvar %s: min %i should not be greater than max %i\n", "dvar '%s' is not an integer dvar\n" referenced here (register-int command).
void Cmd_RegisterInt_f(void)
{
    if (Cmd_Argc() == 5) {
        const char *name = Cmd_Argv(1);
        int value = atoi(Cmd_Argv(2));
        int min = atoi(Cmd_Argv(3));
        int max = atoi(Cmd_Argv(4));
        if (max < min) {
            Com_Printf("dvar %s: min %i should not be greater than max %i\n", name, min, max);
        } else {
            struct dvar_s *d = Dvar_Lookup(name);
            if (d == NULL || (d->type == DVAR_TYPE_STRING && (d->flags & DVAR_CMD_BOOL_FLAG) != 0)) {
                Dvar_RegisterInt(name, (uint32)value, (uint32)min, (uint32)max, (uint16)DVAR_CMD_BOOL_FLAG);
            } else if (d->type != DVAR_TYPE_INT && d->type != DVAR_TYPE_ENUM) {
                Com_Printf("dvar '%s' is not an integer dvar\n", d->name);
            }
        }
    } else {
        Com_Printf("USAGE: %s <name> <default> <min> <max>\n", Cmd_Argv(0));
    }
}

// 0x08063856  size=310  FUN_08063856  status=translated  naming=inferred
// Evidence: strings "dvar %s: min %g should not be greater than max %g\n" and "USAGE: %s <name> <default> <min> <max>\n" referenced here (register-float command).
void Cmd_RegisterFloat_f(void)
{
    if (Cmd_Argc() == 5) {
        const char *name = Cmd_Argv(1);
        double valueD = atof(Cmd_Argv(2));
        float min = (float)atof(Cmd_Argv(3));
        float max = (float)atof(Cmd_Argv(4));
        if (max < min) {
            Com_Printf("dvar %s: min %g should not be greater than max %g\n", name, (double)min,
                       (double)max);
        } else {
            struct dvar_s *d = Dvar_Lookup(name);
            if (d == NULL || (d->type == DVAR_TYPE_STRING && (d->flags & DVAR_CMD_BOOL_FLAG) != 0)) {
                Dvar_RegisterFloat(name, (float)valueD, min, max, (uint16)DVAR_CMD_BOOL_FLAG);
            } else if (d->type != DVAR_TYPE_FLOAT) {
                Com_Printf("dvar '%s' is not an integer dvar\n", d->name);
            }
        }
    } else {
        Com_Printf("USAGE: %s <name> <default> <min> <max>\n", Cmd_Argv(0));
    }
}

// 0x0806398c  size=87  FUN_0806398c  status=translated  naming=inferred
// Evidence: string "USAGE: setu <variable> <value>\n" referenced here; Cmd_Set_f then sets flag 2 on the variable (userinfo set).
void Cmd_SetUser_f(void)
{
    if (Cmd_Argc() < 3) {
        Com_Printf("USAGE: setu <variable> <value>\n");
    } else {
        struct dvar_s *d;
        Cmd_Set_f();
        d = Dvar_Lookup(Cmd_Argv(1));
        if (d != NULL) {
            Dvar_OrFlags(d, 2);
        }
    }
}

// 0x080639e4  size=87  FUN_080639e4  status=translated  naming=inferred
// Evidence: string "USAGE: sets <variable> <value>\n" referenced here; Cmd_Set_f then sets flag 4 on the variable (serverinfo set).
void Cmd_SetServer_f(void)
{
    if (Cmd_Argc() < 3) {
        Com_Printf("USAGE: sets <variable> <value>\n");
    } else {
        struct dvar_s *d;
        Cmd_Set_f();
        d = Dvar_Lookup(Cmd_Argv(1));
        if (d != NULL) {
            Dvar_OrFlags(d, 4);
        }
    }
}

// 0x08063a94  size=133  FUN_08063a94  status=translated  naming=inferred
// Evidence: strings "dvar '%s' doesn't exist\n" and "USAGE: setfromdvar <dest_dvar> <source_dvar>\n" referenced here.
void Cmd_SetFromDvar_f(void)
{
    if (Cmd_Argc() == 3) {
        struct dvar_s *src = Dvar_Lookup(Cmd_Argv(2));
        if (src == NULL) {
            Com_Printf("dvar '%s' doesn't exist\n", Cmd_Argv(2));
        } else {
            const char *text = Dvar_FormatCurrentValue(src);
            Dvar_SetFromUserInput(Cmd_Argv(1), text);
        }
    } else {
        Com_Printf("USAGE: setfromdvar <dest_dvar> <source_dvar>\n");
    }
}

// 0x08063b1a  size=82  FUN_08063b1a  status=translated  naming=inferred
// Evidence: string "USAGE: reset <variable>\n" referenced here; resets the dvar to its default (source 1).
void Cmd_ResetDvar_f(void)
{
    if (Cmd_Argc() == 2) {
        struct dvar_s *d = Dvar_Lookup(Cmd_Argv(1));
        if (d != NULL) {
            Dvar_ResetToDefault(d, 1);
        }
    } else {
        Com_Printf("USAGE: reset <variable>\n");
    }
}

// 0x08063c62  size=457  FUN_08063c62  status=translated  naming=inferred
// Evidence: strings " %s \"%s\"\n", "\n%i total dvars\n" and the flag legend (S U R I A L C) referenced here (dvar listing command).
void Dvar_List_f(void)
{
    char *filter = NULL;
    struct dvar_s *d;
    uint16 flags;

    if (Cmd_Argc() >= 2) {
        filter = (char *)Cmd_Argv(1);
    }
    for (d = dvar_vars; d != NULL; d = d->next) {
        if (filter == NULL || Com_Filter(filter, d->name, 0) != 0) {
            flags = d->flags;
            Com_Printf("%s", (flags & 0x404) == 0 ? " " : "S");
            Com_Printf("%s", (flags & 0x2) == 0 ? " " : "U");
            Com_Printf("%s", (flags & 0x40) == 0 ? " " : "R");
            Com_Printf("%s", (flags & 0x10) == 0 ? " " : "I");
            Com_Printf("%s", (flags & 0x1) == 0 ? " " : "A");
            Com_Printf("%s", (flags & 0x20) == 0 ? " " : "L");
            Com_Printf("%s", (flags & 0x80) == 0 ? " " : "C");
            Com_Printf(" %s \"%s\"\n", d->name, Dvar_FormatCurrentValue(d));
        }
    }
    Com_Printf("\n%i total dvars\n", dvar_indexCount);
}

// 0x080640b6  size=118  FUN_080640b6  status=translated  naming=inferred
// Evidence: walks the dvar list and returns the name of the first dvar whose value is outside its domain (Dvar_ValueInDomain); empty string when all are in range.
char *Dvar_FirstOutOfDomainName(char *out)
{
    struct dvar_s *d = dvar_vars;

    *out = '\0';
    while (d != NULL) {
        char inDomain = (char)Dvar_ValueInDomain((uint8)d->type, d->current.unsignedInt, (uint32)(unsigned long)d->domain.enumeration.stringCount, (uint32)(unsigned long)d->domain.enumeration.strings);
        if (inDomain != 1) {
            strcpy(out, d->name);
            return out;
        }
        d = d->next;
    }
    return out;
}

/* ======================================================================== */
/* File system helpers.                                                     */
/* ======================================================================== */

// 0x080649cc  size=162  FUN_080649cc  status=translated  naming=inferred
// Evidence: reads through the file-handle table entry at stride 0x11c; a handle flagged for stdio goes through Sys_Fread, others through FS_Read.
int FS_ReadThroughHandle(void *buf, int len, int handle)
{
    int result;
    int32 *slot;

    FS_Nop();
    if (handle == 0) {
        return 0;
    }
    slot = (int32 *)(g_0849fd98 + handle * 0x11c);
    if (*slot == 0) {
        result = FS_Read(buf, len, handle);
    } else {
        *slot = 0;
        result = Sys_Fread((int)(unsigned long)buf, len, 1, handle);
        *slot = 1;
    }
    return result;
}

// 0x080654a0  size=772  FUN_080654a0  status=translated  naming=inferred
// Evidence: strings "main", ".iwd", "%s.%08x.iwd", "(local file exists with wrong checksum)", "Need iwds: %s\n" referenced here (required-iwd checklist builder).
// Note: fs_referencedIwdNames is typed uint32 in the merged symbols, but its entries are used as C string pointers here (see cod2_iwd_name).
int FS_BuildMissingIwdList(char *out, int outSize, int verbose)
{
    int i;
    char buf[0x100];

    if (fs_numReferencedIwds == 0) {
        return 0;
    }
    out[0] = '\0';
    for (i = 0; i < fs_numReferencedIwds; i++) {
        const char *name = fs_referencedIwdNames[i];
        int found = 0;
        struct searchpath_s *sp;

        if (FS_iwIwd((char *)name, "main") == 0 && FS_IwdNameHasSvrMarker(name) == 0) {
            for (sp = fs_searchpaths; sp != NULL; sp = sp->next) {
                if (sp->iwd != NULL && sp->iwd->checksum == fs_referencedIwdSums[i]) {
                    found = 1;
                    break;
                }
            }
            if (found == 0 && name != NULL && name[0] != '\0') {
                if (verbose == 0) {
                    I_strncat(out, outSize, name);
                    I_strncat(out, outSize, ".iwd");
                    if (FS_SV_FileExists(va("%s.iwd", name))) {
                        I_strncat(out, outSize, " (local file exists with wrong checksum)");
                    }
                    I_strncat(out, outSize, "\n");
                } else {
                    I_strncat(out, outSize, "@");
                    I_strncat(out, outSize, name);
                    I_strncat(out, outSize, ".iwd");
                    I_strncat(out, outSize, "@");
                    if (FS_SV_FileExists(va("%s.iwd", name)) == 0) {
                        I_strncat(out, outSize, name);
                        I_strncat(out, outSize, ".iwd");
                    } else {
                        Com_sprintf(buf, 0x100, "%s.%08x.iwd", name, fs_referencedIwdSums[i]);
                        I_strncat(out, outSize, buf);
                    }
                }
            }
        }
    }
    if (out[0] == '\0') {
        return 0;
    }
    Com_Printf("Need iwds: %s\n", out);
    return 1;
}

// 0x080657a4  size=56  FUN_080657a4  status=translated  naming=inferred
// Evidence: removes the commands "path", "dir", "fdir" and "touchFile" (file-system command cleanup).
void FS_UnregisterCommands(void)
{
    Cmd_RemoveCommand("path");
    Cmd_RemoveCommand("dir");
    Cmd_RemoveCommand("fdir");
    Cmd_RemoveCommand("touchFile");
}

/* ======================================================================== */
/* Huffman message coding.                                                  */
/* ======================================================================== */

// 0x08066a4e  size=682  FUN_08066a4e  status=translated  naming=inferred
// Evidence: adaptive Huffman decode (Quake III Huff_Decompress counterpart). The message keeps a 16-bit big-endian length at data+offset, then the bitstream from bit 16. Scratch: 0x10000-byte output and one 0x701c huffman_s from the pool.
void Huff_Decompress(msg_s *msg, int offset)
{
    int seqSave = 0;
    int huffSave = 0;
    byte *outbuf;
    struct huffman_s *huff;
    int remaining;
    byte *in;
    unsigned int outlen;
    int i;
    int ch;

    Com_PoolAllocInto2(&seqSave, 0x10000);
    outbuf = (byte *)Com_PoolAt(seqSave);
    Com_PoolAllocInto2(&huffSave, 0x701c);
    huff = (struct huffman_s *)Com_PoolAt(huffSave);

    remaining = msg->cursize - offset;
    in = msg->data + offset;
    if (remaining < 1) {
        Com_PoolRestoreFrom2(&huffSave);
        Com_PoolRestoreFrom2(&seqSave);
        return;
    }

    Com_Memset(huff, 0, 0x701c);
    /* Decoder tree: one NYT node at the first nodeList slot. */
    huff->loc[COD2_HUFF_NYT] = &huff->nodeList[huff->blocNode];
    huff->blocNode = huff->blocNode + 1;
    huff->ltail = huff->loc[COD2_HUFF_NYT];
    huff->lhead = huff->loc[COD2_HUFF_NYT];
    huff->tree = huff->loc[COD2_HUFF_NYT];
    huff->tree->symbol = COD2_HUFF_NYT;
    huff->tree->weight = 0;
    huff->lhead->prev = NULL;
    huff->lhead->next = NULL;
    huff->tree->right = NULL;
    huff->tree->left = NULL;
    huff->tree->parent = NULL;

    outlen = (unsigned int)((((unsigned int)in[0]) << 8) | (unsigned int)in[1]);
    if (msg->maxsize - offset < (int)outlen) {
        outlen = (unsigned int)(msg->maxsize - offset);
    }

    huff_bitCursor = 0x10;
    for (i = 0; i < (int)outlen; i++) {
        ch = 0;
        if (remaining < (huff_bitCursor >> 3)) {
            outbuf[i] = 0;
            break;
        }
        Huff_Receive(huff->tree, &ch, in);
        if (ch == COD2_HUFF_NYT) {
            int bit;
            ch = 0;
            for (bit = 0; bit < 8; bit++) {
                int shifted = ch * 2;
                int value = Huff_getBit(in);
                ch = value + shifted;
            }
        }
        outbuf[i] = (byte)ch;
        Huff_addRef(huff, (uint32)(ch & 0xff));
    }

    msg->cursize = offset + (int)outlen;
    Com_Memcpy(msg->data + offset, outbuf, (size_t)outlen);
    Com_PoolRestoreFrom2(&huffSave);
    Com_PoolRestoreFrom2(&seqSave);
}

// 0x08066cf8  size=568  FUN_08066cf8  status=translated  naming=inferred
// Evidence: adaptive Huffman encode of the message body (Quake III Huff_Compress counterpart). Writes a 16-bit big-endian length, then the coded bits; the bit cursor is advanced by 8 at the end before the byte count is taken.
void Huff_Compress(msg_s *msg, int offset)
{
    int seqSave = 0;
    int huffSave = 0;
    byte *outbuf;
    struct huffman_s *huff;
    int remaining;
    byte *src;
    int i;

    Com_PoolAllocInto2(&seqSave, 0x10000);
    outbuf = (byte *)Com_PoolAt(seqSave);
    Com_PoolAllocInto2(&huffSave, 0x701c);
    huff = (struct huffman_s *)Com_PoolAt(huffSave);

    remaining = msg->cursize - offset;
    src = msg->data + offset;
    if (remaining < 1) {
        Com_PoolRestoreFrom2(&huffSave);
        Com_PoolRestoreFrom2(&seqSave);
        return;
    }

    Com_Memset(huff, 0, 0x701c);
    /* Encoder tree: one NYT node; ltail stays NULL (as in the binary). */
    huff->loc[COD2_HUFF_NYT] = &huff->nodeList[huff->blocNode];
    huff->blocNode = huff->blocNode + 1;
    huff->lhead = huff->loc[COD2_HUFF_NYT];
    huff->tree = huff->loc[COD2_HUFF_NYT];
    huff->tree->symbol = COD2_HUFF_NYT;
    huff->tree->weight = 0;
    huff->lhead->prev = NULL;
    huff->lhead->next = NULL;
    huff->tree->right = NULL;
    huff->tree->left = NULL;
    huff->tree->parent = NULL;
    huff->loc[COD2_HUFF_NYT] = huff->tree;

    outbuf[0] = (byte)(((unsigned int)remaining) >> 8);
    outbuf[1] = (byte)remaining;

    huff_bitCursor = 0x10;
    for (i = 0; i < remaining; i++) {
        unsigned int ch = (unsigned int)src[i];
        Huff_transmit(huff, (int)ch, outbuf);
        Huff_addRef(huff, ch & 0xff);
    }
    huff_bitCursor = huff_bitCursor + 8;

    msg->cursize = (huff_bitCursor >> 3) + offset;
    Com_Memcpy(msg->data + offset, outbuf, (size_t)(huff_bitCursor >> 3));
    Com_PoolRestoreFrom2(&huffSave);
    Com_PoolRestoreFrom2(&seqSave);
}

/* ======================================================================== */
/* Network and script entry wrappers.                                       */
/* ======================================================================== */

// 0x0806c5a0  size=26  FUN_0806c5a0  status=translated  naming=inferred
// Evidence: forwards to Sys_GetPacket (NET_GetPacket counterpart) and discards its result, as in the binary.
void NET_ReceivePacket(struct netadr_s *from, struct msg_s *msg)
{
    (void)Sys_GetPacket(from, msg);
}

// 0x08075d58  size=71  FUN_08075d58  status=translated  naming=inferred
// Evidence: compiles the statement with flag 0 and empty context, then emits the terminating opcode 0x80 (Scr_CompileStmt and EmitOpcode callers).
void Scr_CompileTopLevel(struct cod2_scr_node *node)
{
    Scr_CompileStmt((struct ScriptNode *)node, 0, 0, 0);
    EmitOpcode(0x80, 0, 0);
}
