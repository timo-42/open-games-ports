/*
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 102.
 * Range 0x080b30a0..0x080fa1b6: 80 functions created at .eh_frame FDE starts by
 * DecompEhStarts.java (Ghidra auto-analysis missed them). Pseudocode from
 * .local/cod2/decomp/functions_eh/<addr>.c; boundaries verified with objdump (each body ends
 * at its manifest size, final instruction ret or tail jmp). Binary SHA-256
 * 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * Conventions:
 *  - Ghidra "undefined4"/"undefined2" parameters are translated to the type implied by the
 *    callee (uint32/int32/uint16). Float arguments passed as stack words keep their bits.
 *  - "longdouble" returns are float-rounded values returned in st(0); they are returned as float.
 *  - Ghidra "ROUND" is x87 chop; translated as an (int) cast.
 *  - Stack-passed call trampolines (80b52ee) were checked against objdump, not against the
 *    Ghidra argument list.
 *  - Scratch buffers that Ghidra left uninitialised are zeroed (80c0ffc); this only removes
 *    reads of indeterminate values.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
/*HIDE_BEGIN*/
#include "cod2_prototypes.h"
/*HIDE_END*/
#include "chunk_102_types.h"


/* ---- rodata constants used by this chunk ---- */

typedef void (*dllEntry_fn)(uint32 arg);

/* Bit-pattern helpers for float arguments that travel as 32-bit words. */
static inline float chunk102_float_from_bits(uint32 bits)
{
    union { uint32 u; float f; } v;
    v.u = bits;
    return v.f;
}

static inline uint32 chunk102_bits_from_float(float f)
{
    union { uint32 u; float f; } v;
    v.f = f;
    return v.u;
}

/* Test bit i of a little-endian uint32 bitset (word i/32, bit i%32). */
static inline int chunk102_bit_test(const uint32 *words, int i)
{
    return (int)((words[i >> 5] >> (i & 31)) & 1u);
}

/* Client index: client records are 0x28a4 bytes; level_clients is the array base. */
static inline int chunk102_client_index(const struct gclient_s *cl)
{
    return (int)(cl - level_clients);
}

/* Script entity reference (param_1 of the player-entity commands). The high 16 bits must be
 * zero for an entity number; otherwise the script raises "not an entity". */
static struct gentity_s *chunk102_player_entity_from_ref(uint32 ref)
{
    struct gentity_s *ent;
    if ((ref >> 16) == 0) {
        ent = &g_gentities[(uint16)ref];
        if (ent->client == 0) {
            Scr_ErrorNoParam(va("entity %i is not a player", (int)(uint16)ref));
        }
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = 0;
    }
    return ent;
}

/* ======================================================================================= */

// 0x080b30a0  size=77  FUN_080b30a0  status=translated  naming=inferred
// evidence: returns the stored float[2] for a vec2 dvar (type 2), else parses the string value
// with Dvar_ParseVec2; zero fallback at rodata 0x0814cfc0. Lineage: Dvar_GetVec2 (Q3 Cvar_VariableVec counterpart).
const float *Dvar_GetVec2(const char *name)
{
    struct dvar_s *d = Dvar_Lookup(name);
    const float *result;
    if (d == 0) {
        result = g_0814cfc0;
    } else if (d->type == 2) {
        result = (const float *)(size_t)d->current.unsignedInt;
    } else {
        result = Dvar_ParseVec2((const char *)(size_t)d->current.unsignedInt);
    }
    return result;
}

// 0x080b30ee  size=77  FUN_080b30ee  status=translated  naming=inferred
// evidence: vec3 twin of Dvar_GetVec2 (type 3, Dvar_ParseVec3, zero vec at 0x0814cfc8).
const float *Dvar_GetVec3(const char *name)
{
    struct dvar_s *d = Dvar_Lookup(name);
    const float *result;
    if (d == 0) {
        result = vec3_origin;
    } else if (d->type == 3) {
        result = (const float *)(size_t)d->current.unsignedInt;
    } else {
        result = Dvar_ParseVec3((const char *)(size_t)d->current.unsignedInt);
    }
    return result;
}

// 0x080b313c  size=77  FUN_080b313c  status=translated  naming=inferred
// evidence: vec4 twin of Dvar_GetVec2 (type 4, Dvar_ParseVec4, zero vec at 0x0814cfd4).
const float *Dvar_GetVec4(const char *name)
{
    struct dvar_s *d = Dvar_Lookup(name);
    const float *result;
    if (d == 0) {
        result = g_0814cfd4;
    } else if (d->type == 4) {
        result = (const float *)(size_t)d->current.unsignedInt;
    } else {
        result = Dvar_ParseVec4((const char *)(size_t)d->current.unsignedInt);
    }
    return result;
}

// 0x080b3614  size=26  FUN_080b3614  status=translated  naming=unknown
// evidence: tail forwarder to Dvar_SetReset (0x080b3570); no name string.
void Dvar_SetResetForward(struct dvar_s *d, uint32 value)
{
    Dvar_SetReset(d, value);
}

// 0x080b3a80  size=48  FUN_080b3a80  status=translated  naming=unknown
// evidence: if Dvar_HasPendingLatch(d), latch the current value word again (Dvar_SetLatched(d, d->current.unsignedInt)).
void Dvar_LatchCurrentIfPending(struct dvar_s *d)
{
    if (Dvar_HasPendingLatch(d) != 0) {
        Dvar_SetLatched(d, d->current.unsignedInt);
    }
}

// 0x080b40de  size=106  FUN_080b40de  status=translated  naming=inferred
// evidence: builds a 2-float buffer (Vec2_Set_080b5760) and registers dvar type 2 (vec2) via Dvar_Register.
void Dvar_RegisterVec2(const char *name, float x, float y, uint32 min, uint32 max, uint16 flags)
{
    float buf[2];
    Vec2_Set_080b5760(buf, x, y);
    Dvar_Register(name, 2, flags, (uint32)(size_t)buf, min, max);
}

// 0x080b4148  size=113  FUN_080b4148  status=translated  naming=inferred
// evidence: vec3 registration (type 3) through Vec3_Set_080b5810 and Dvar_Register.
void Dvar_RegisterVec3(const char *name, float x, float y, float z, uint32 min, uint32 max, uint16 flags)
{
    float buf[3];
    Vec3_Set_080b5810(buf, x, y, z);
    Dvar_Register(name, 3, flags, (uint32)(size_t)buf, min, max);
}

// 0x080b41ba  size=120  FUN_080b41ba  status=translated  naming=inferred
// evidence: vec4 registration (type 4) through Vec4_Set and Dvar_Register.
void Dvar_RegisterVec4(const char *name, float x, float y, float z, float w, uint32 min, uint32 max, uint16 flags)
{
    float buf[4];
    Vec4_Set(buf, x, y, z, w);
    Dvar_Register(name, 4, flags, (uint32)(size_t)buf, min, max);
}

// 0x080b4e02  size=132  FUN_080b4e02  status=translated  naming=unknown
// evidence: format "%g %g %g %g" (va) registers a string dvar with flags 0x4000 when the dvar is
// missing; otherwise sets the vec4 value. No function-name string.
void Dvar_SetVec4OrRegisterString(const char *name, float x, float y, float z, float w)
{
    struct dvar_s *d = Dvar_Lookup(name);
    if (d == 0) {
        const char *text = va("%g %g %g %g", (double)x, (double)y, (double)z, (double)w);
        Dvar_RegisterString(name, text, 0x4000);
    } else {
        Dvar_SetVec4(d, x, y, z, w);
    }
}

// 0x080b5202  size=235  FUN_080b5202  status=translated  naming=unknown
// evidence: walks the 256 dvar hash buckets (dvar_hashTable, chain at +0x20) under critical section 4
// and writes name/value NUL-terminated pairs for dvars whose flags match the mask. buf == NULL
// only measures the length. Returns the total byte count.
int Dvar_GetCombinedString(char *buf, uint16 mask)
{
    int total = 0;
    int bucket;
    struct dvar_s *d;

    Sys_EnterCriticalSection(4);
    for (bucket = 0; bucket < 0x100; bucket++) {
        for (d = dvar_hashTable[bucket]; d != 0; d = d->hashNext) {
            if ((d->flags & mask) != 0) {
                int valueOffset;
                const char *value;
                if (buf != 0) {
                    strcpy(buf + total, d->name);
                }
                valueOffset = (int)ScrStr_Strlen(d->name) + total + 1;
                value = Dvar_FormatCurrentValue(d);
                if (buf != 0) {
                    strcpy(buf + valueOffset, value);
                }
                total = (int)ScrStr_Strlen(value) + valueOffset + 1;
            }
        }
    }
    Sys_LeaveCriticalSection(4);
    return total;
}

// 0x080b52ee  size=245  FUN_080b52ee  status=translated  naming=unknown
// evidence: objdump (not the Ghidra arguments): parses name/value NUL-terminated pairs over
// [buf, buf+len). Known dvar: Dvar_ParseValueWord(dvar type, enum count, enum names) then
// Dvar_SetVariant(d, word, 0). Unknown dvar: Dvar_RegisterString(name, value, 0x4000).
void Dvar_SetFromCombinedString(const char *buf, int len)
{
    int pos = 0;
    Sys_EnterCriticalSection(4);
    while (pos < len) {
        const char *key = buf + pos;
        const char *value;
        struct dvar_s *d;
        uint32 parsed = 0;

        pos = (int)ScrStr_Strlen(key) + pos + 1;
        value = buf + pos;
        pos = (int)ScrStr_Strlen(value) + pos + 1;

        d = Dvar_FindHashed(key);
        if (d != 0) {
            parsed = Dvar_ParseValueWord(d->type, d->domain, value).integer;
            Dvar_SetVariant(d, parsed, 0);
        } else {
            Dvar_RegisterString(key, value, 0x4000);
        }
    }
    Sys_LeaveCriticalSection(4);
}

// 0x080b53e4  size=91  FUN_080b53e4  status=translated  naming=unknown
// evidence: walks dvar_vars (chain +0x1c) under critical section 4 and resets dvars whose flags
// match the mask through Dvar_ResetToDefault.
void Dvar_ResetAllWithFlags(uint16 mask, int source)
{
    struct dvar_s *d;
    Sys_EnterCriticalSection(4);
    for (d = dvar_vars; d != 0; d = d->next) {
        if ((d->flags & mask) != 0) {
            Dvar_ResetToDefault(d, source);
        }
    }
    Sys_LeaveCriticalSection(4);
}

// 0x080b5440  size=162  FUN_080b5440  status=translated  naming=unknown
// evidence: format "%s \"%s\"\n" writes one config line per named dvar into buf. Returns 1 when
// all names were written, 0 when snprintf fails.
int Dvar_FormatNamedAsConfigLines(const char *const *names, int count, char *buf, size_t size)
{
    int i = 0;
    for (;;) {
        struct dvar_s *d;
        const char *value;
        int n;
        if (count <= i) {
            return 1;
        }
        d = Dvar_Lookup(names[i]);
        value = Dvar_FormatCurrentValue(d);
        n = snprintf(buf, size, "%s \"%s\"\n", d->name, value);
        if (n < 0) {
            break;
        }
        buf += n;
        size -= (size_t)n;
        i++;
    }
    return 0;
}

// 0x080b54e2  size=484  FUN_080b54e2  status=translated  naming=unknown
// evidence: strings "^1ERROR: the following dvars were not specified in file '%s'", "^3WARNING:
// unknown dvar '%s' in file '%s'". Resets the named dvars, parses the file token stream, applies
// each matched value with Dvar_SetFromString and reports the missing names.
int Dvar_LoadNamedSetFromFile(const char *const *names, size_t count, const char *data, const char *filename)
{
    unsigned char found[0x4000];
    size_t foundCount = 0;
    int idx;
    const char *token;
    int result;

    memset(found, 0, count);
    for (idx = 0; idx < (int)count; idx++) {
        struct dvar_s *d = Dvar_Lookup(names[idx]);
        Dvar_ResetToDefault(d, 0);
    }

    Com_BeginParseSession(filename);
    for (;;) {
        token = Com_Parse(&data);
        if (*token == '\0') {
            Com_EndParseSession();
            if (foundCount == count) {
                result = 1;
            } else {
                Com_Printf("^1ERROR: the following dvars were not specified in file '%s'\n", filename);
                for (idx = 0; idx < (int)count; idx++) {
                    if (found[idx] == 0) {
                        Com_Printf("^1  %s\n", names[idx]);
                    }
                }
                result = 0;
            }
            return result;
        }

        {
            int matched = 0;
            for (idx = 0; idx < (int)count; idx++) {
                if (strcasecmp(token, names[idx]) == 0) {
                    struct dvar_s *d = Dvar_Lookup(names[idx]);
                    token = Com_ParseOnLine(&data);
                    Dvar_SetFromString(d, token);
                    if (found[idx] == 0) {
                        found[idx] = 1;
                        foundCount++;
                    }
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                Com_Printf("^3WARNING: unknown dvar '%s' in file '%s'\n", token, filename);
            }
        }
        Com_SkipRestOfLine(&data);
    }
}

// 0x080b5ae4  size=33  FUN_080b5ae4  status=translated  naming=unknown
// evidence: forwarder to MsgBuf_WriteData(buf, len, src).
void MsgBuf_WriteBytes(struct msgbuf_s *buf, uint32 len, const void *src)
{
    MsgBuf_WriteData(buf, len, src);
}

// 0x080b5cac  size=44  FUN_080b5cac  status=translated  naming=inferred
// evidence: writes strlen+1 bytes (including the NUL) via MsgBuf_WriteData; lineage Q3 MSG_WriteString.
void MsgBuf_WriteString(struct msgbuf_s *buf, const char *s)
{
    size_t n = strlen(s);
    MsgBuf_WriteData(buf, (uint32)(n + 1), s);
}

// 0x080b5cd8  size=134  FUN_080b5cd8  status=translated  naming=inferred
// evidence: "Couldn't skip %i bytes from %i-byte buffer (only %i bytes left)"; lineage Q3 MSG_ReadSkip.
void MsgBuf_Skip(struct msgbuf_s *msg, int count)
{
    if (count != 0 && msg->overflowed == 0) {
        if (msg->maxsize < count + msg->cursor) {
            if (msg->warn_on_overflow != 0) {
                Com_Error(1, "Couldn't skip %i bytes from %i-byte buffer (only %i bytes left)\n",
                          count, msg->maxsize, msg->maxsize - msg->cursor);
            }
            msg->overflowed = 1;
        } else {
            msg->cursor = count + msg->cursor;
        }
    }
}

// 0x080b5e0c  size=162  FUN_080b5e0c  status=translated  naming=inferred
// evidence: "End of memory file while reading string (%i bytes read)"; lineage Q3 MSG_ReadString.
// Returns a pointer into the buffer (NUL-terminated) or the empty string on overflow.
const char *MsgBuf_ReadString(struct msgbuf_s *msg)
{
    if (msg->overflowed == 0) {
        int start = msg->cursor;
        do {
            if (msg->data[msg->cursor] == 0) {
                msg->cursor++;
                return (const char *)(msg->data + start);
            }
            msg->cursor++;
        } while (msg->cursor != msg->maxsize);
        if (msg->warn_on_overflow != 0) {
            Com_Error(1, "End of memory file while reading string (%i bytes read)\n", msg->cursor - start);
        }
        msg->overflowed = 1;
    }
    return "";
}

// 0x080b6f24  size=107  FUN_080b6f24  status=translated  naming=unknown
// evidence: token walk counting '{' and '}' single-character tokens; returns 1 when the block at
// nesting level `depth` is found.
int Com_SeekBracedBlock(const char **data_p, int depth)
{
    int found = 0;
    int level = 0;
    do {
        const char *token = Com_Parse(data_p);
        if (token[1] == '\0') {
            if (token[0] == '{') {
                if (level == depth) {
                    found = 1;
                } else {
                    level++;
                }
            } else if (token[0] == '}') {
                level--;
            }
        }
    } while (level != 0 && *data_p != 0);
    return found;
}

// 0x080b704a  size=123  FUN_080b704a  status=translated  naming=unknown
// evidence: joins the remaining line tokens with single spaces into the parse-session buffer
// (session + 0x45cc, 0x400 bytes) and returns it.
char *Com_ParseJoinedLine(const char **data_p)
{
    char *dst = (char *)ParseSession_Get() + 0x45cc;
    *dst = '\0';
    for (;;) {
        char *tok = Com_ParseOnLine(data_p);
        if (*tok == '\0') {
            break;
        }
        if (*dst != '\0') {
            I_strncat(dst, 0x400, " ");
        }
        I_strncat(dst, 0x400, tok);
    }
    return dst;
}

// 0x080b70c6  size=45  FUN_080b70c6  status=translated  naming=unknown
// evidence: Com_Parse token through atof, rounded to float (returned in st(0)).
float Com_ParseFloat(const char **data_p)
{
    char *tok = Com_Parse(data_p);
    double v = atof(tok);
    return (float)v;
}

// 0x080b70f4  size=45  FUN_080b70f4  status=translated  naming=unknown
// evidence: Com_ParseOnLine token through atof, rounded to float.
float Com_ParseOnLineFloat(const char **data_p)
{
    char *tok = Com_ParseOnLine(data_p);
    double v = atof(tok);
    return (float)v;
}

// 0x080b7122  size=33  FUN_080b7122  status=translated  naming=unknown
// evidence: Com_Parse token through atoi; the value is discarded.
void Com_ParseIntDiscard(const char **data_p)
{
    char *tok = Com_Parse(data_p);
    (void)atoi(tok);
}

// 0x080b7144  size=33  FUN_080b7144  status=translated  naming=unknown
// evidence: Com_ParseOnLine token through atoi; the value is discarded.
void Com_ParseOnLineIntDiscard(const char **data_p)
{
    char *tok = Com_ParseOnLine(data_p);
    (void)atoi(tok);
}

// 0x080b7266  size=132  FUN_080b7266  status=translated  naming=unknown
// evidence: literal tokens "(" and ")" (rodata 0x0814f6ce, 0x0814f6d0) around `count` calls to
// Com_Parse2DMatrix with stride y*x*4 bytes.
void Com_ParseFloatBlocks(const char **data_p, int count, int y, int x, float *out)
{
    int i;
    (void)Com_MatchToken(data_p, "(", 0);
    for (i = 0; i < count; i++) {
        Com_Parse2DMatrix(data_p, y, x, (float *)((char *)out + i * x * y * 4));
    }
    (void)Com_MatchToken(data_p, ")", 0);
}

// 0x080b734c  size=205  FUN_080b734c  status=translated  naming=unknown
// evidence: "filepath '%s%s%s' is longer than %i characters"; concatenates three strings into
// dest with the third string's NUL terminator.
void FS_JoinPathChecked(const char *a, const char *b, const char *c, char *dest, int destsize)
{
    unsigned int na = ScrStr_Strlen(a);
    unsigned int nb = ScrStr_Strlen(b);
    int nc = (int)ScrStr_Strlen(c);
    if (destsize <= (int)(nb + na + (unsigned int)nc)) {
        Com_Error(1, "filepath '%s%s%s' is longer than %i characters", a, b, c, destsize - 1);
    }
    memcpy(dest, a, na);
    memcpy(dest + na, b, nb);
    memcpy(dest + na + nb, c, (size_t)nc + 1);
}

// 0x080b7602  size=21  FUN_080b7602  status=translated  naming=unknown
// evidence: indirect call through the float-returning hook slot g_085bc814; result discarded.
void Swap_CallFloatHook(uint32 bits)
{
    (void)g_085bc814(bits);
}

// 0x080b7618  size=21  FUN_080b7618  status=translated  naming=unknown
// evidence: indirect call through the int-returning hook slot g_085bc818; result discarded.
void Swap_CallIntHook(uint32 bits)
{
    (void)g_085bc818(bits);
}

// 0x080b9b80  size=494  FUN_080b9b80  status=translated  naming=unknown
// evidence: part-group walk. Group objects are reached through the DObj slot table (+0x1c + 4k);
// each block header (count, skip, values) gives the bit range of a group. The part-map handle
// (+0x10) selects a bit table (words at +0) and a pair table (k, v bytes from +0x10) that maps
// part indices. Marked parts are OR-ed into the bitset `bits`. Structure kept as the decompiled
// gotos; the jump into the inner loop body (LAB_080b9d29) is kept as a label.
void DObj_PropagatePartBits(struct DObj *obj, uint32 *bits)
{
    uint8 *p = (uint8 *)obj;
    int bit;                     /* local_64 */
    int cum[8];                  /* local_5c cumulative block counts */
    int group;                   /* local_24 */
    int local;                   /* local_34 index inside the current block */
    int target;                  /* local_3c */
    const struct dobj_part_block_s *blk = 0;   /* local_28 */
    const uint8 *valBase = 0;    /* local_30 */
    const uint32 *table;         /* iVar1 part-map handle text */
    const uint8 *pairStart;      /* local_2c */
    const uint8 *pair;           /* local_38 */

    bit = (int)*(uint8 *)(p + 0x19) - 1;
    if (*(uint16 *)(p + 0x10) == 0) {
        DObj_BuildDuplicatePartMap(obj);
    }
    table = (const uint32 *)ScrStr_TextFromHandle(*(uint16 *)(p + 0x10));
    pairStart = (const uint8 *)table + 0x10;

    cum[0] = 0;
    group = 0;
    for (;;) {
        blk = (*(struct dobj_part_group_s **)(p + 0x1c + group * 4))->block;
        target = (int)blk->count + cum[group];
        if (bit < target) {
            break;
        }
        group++;
        cum[group] = target;
    }
    valBase = (const uint8 *)blk->values + 4;

    for (;;) {                   /* do { ... } while (1) */
        for (;;) {               /* L2: while (1) */
            for (;;) {           /* L3: while (1) */
                local = bit - cum[group];
                while (local < 0) {
                    group--;
                    if (group < 0) {
                        return;
                    }
                    blk = (*(struct dobj_part_group_s **)(p + 0x1c + group * 4))->block;
                    valBase = (const uint8 *)blk->values + 4;
                    local = bit - cum[group];
                }
                if (chunk102_bit_test((const uint32 *)bits, bit) != 0) {
                    break;
                }
                bit--;
            }
            if (chunk102_bit_test(table, bit) == 0) {
                break;
            }
            for (pair = pairStart; bit != (int)pair[0] - 1; pair += 2) {
            }
            target = (int)pair[1] - 1;
        LAB_set_part_bit:
            bits[target >> 5] |= 1u << (target & 31);
            bit--;
        }
        if (local - (int)blk->skip >= 0) {
            target = bit - (int)valBase[local - (int)blk->skip];
            goto LAB_set_part_bit;
        }
        target = (int)p[group + 0x3c];
        if (target != 0xff) {
            goto LAB_set_part_bit;
        }
        bit--;
    }
}

// 0x080ba706  size=112  FUN_080ba706  status=translated  naming=unknown
// evidence: copies a 25-word template into the DObj record, clears its handle word at +0x04,
// releases the script string (ScrStr_AddRef) when the handle differs from the empty-table id, and
// binds the anim tree.
void DObj_InitFromTemplate(const uint32 *src, struct XAnimTree *tree, struct DObj *dst)
{
    uint32 *d = (uint32 *)dst;
    int i;
    for (i = 0x19; i != 0; i--) {
        *d = *src;
        src++;
        d++;
    }
    *(uint32 *)((uint8 *)dst + 4) = 0;
    if (*(int16 *)((uint8 *)dst + 0x10) != 0 &&
        (int)*(uint16 *)((uint8 *)dst + 0x10) != g_dobjEmptyDupPartTable) {
        ScrStr_AddRef(*(uint16 *)((uint8 *)dst + 0x10));
    }
    DObj_BindAnimTree(dst, tree);
}

// 0x080ba7f2  size=357  FUN_080ba7f2  status=translated  naming=unknown
// evidence: fills the part-info output triples (name id, text, flag bit) for the model parts; the
// per-part name text comes from XModel_GetBoneNames of the owning model in the group walk.
void XModelParts_CollectPartInfo(int32 *parts, int32 *out, uint16 *outCount, int32 *outModel, uint16 *outFirst)
{
    uint8 *p = (uint8 *)parts;
    int32 *c = out;
    int local10 = 0;
    *outCount = (uint16)*(uint8 *)(p + 6);
    *outModel = parts[0];
    *outFirst = (parts[0] == 0) ? (uint16)0 : *(uint16 *)((uint8 *)(size_t)parts[0] + 4);

    for (;;) {
        int iVar4;
        int iVar3;
        if ((int)(uint32)*(uint8 *)(p + 6) <= local10) {
            return;
        }
        c[0] = parts[local10 + 7];
        c[1] = 0;
        c[2] = (int32)((((uint32)*(uint8 *)(p + 0x1a)) >> (local10 & 31) & 1u) != 0);
        iVar4 = local10;
        if (*(int8 *)(p + local10 + 0x3c) != -1) {
            for (;;) {
                iVar3 = iVar4;
                if (iVar3 - 1 < 0) {
                    break;
                }
                iVar4 = iVar3 - 1;
                if (!(*(uint8 *)(p + local10 + 0x3c) < *(uint8 *)(p + iVar3 + 0x43))) {
                    uint8 bVar1 = *(uint8 *)(p + local10 + 0x3c);
                    uint8 bVar2 = *(uint8 *)(p + iVar3 + 0x43);
                    const uint16 *bones = XModel_GetBoneNames((const struct XModel *)(size_t)parts[iVar3 + 6]);
                    c[1] = (int32)(size_t)ScrStr_TextFromHandle(
                        *(const uint16 *)((const uint8 *)bones + ((uint32)bVar1 - (uint32)bVar2) * 2));
                    break;
                }
            }
        }
        local10++;
        c += 3;
    }
}

// 0x080bbc4e  size=33  FUN_080bbc4e  status=translated  naming=unknown
// evidence: calls XModel_GetLodIndexForDistance on the part's model (parts + 0x1c + 4*part); the
// result is discarded.
void XModel_TouchLodForPart(const struct XModelParts *parts, int part, float distance)
{
    const uint8 *p = (const uint8 *)parts;
    (void)XModel_GetLodIndexForDistance(
        (const struct XModel *)(size_t)*(const int32 *)(p + 0x1c + part * 4), distance);
}

// 0x080bbc70  size=95  FUN_080bbc70  status=translated  naming=unknown
// evidence: maximum of XModel_GetLastLodDistance over the part's models (count at +0x18).
float XModel_GetMaxLastLodDistance(const struct XModelParts *parts)
{
    const uint8 *p = (const uint8 *)parts;
    float best = 0.0f;
    int i;
    for (i = 0; i < (int)*(const uint8 *)(p + 0x18); i++) {
        float v = XModel_GetLastLodDistance(
            (const struct XModel *)(size_t)*(const int32 *)(p + 0x1c + i * 4));
        if (best < v) {
            best = v;
        }
    }
    return best;
}

// 0x080c0ffc  size=111  FUN_080c0ffc  status=translated  naming=unknown
// evidence: when the slot holds a handle, runs the release helper with a zeroed scratch record
// and then XAnim_SyncInstanceSlots(slot, ref). Scratch fields zeroed (Ghidra left them unset).
void XAnim_ReleaseSlot(int32 *slot, uint32 *ref, uint32 arg)
{
    if (slot[0] != 0) {
        uint32 scratch[2];
        uint16 scratchShort = 0;
        uint8 flagByte = 0;
        uint8 scratchBytes[9];
        scratch[0] = 0;
        scratch[1] = 0;
        (void)scratchShort;
        XAnim_UpdateBlendRecursive((struct XAnimTree *)(size_t)*ref, 0, (float *)scratch, chunk102_float_from_bits(arg), 1, scratchBytes, &flagByte);
        XAnim_SyncInstanceSlots((struct XAnimTree *)(size_t)slot[0], (struct XAnimTree *)(size_t)*ref);
    }
}

// 0x080c10c6  size=135  FUN_080c10c6  status=translated  naming=unknown
// evidence: clears g_xanimPendingCount and runs the same release helper as XAnim_ReleaseSlot, then
// XAnim_AdvanceNodeCursor(handle, 0, arg, 1).
void XAnim_ResetSlot(int32 *slot, uint32 arg)
{
    g_xanimPendingCount = 0;
    if (slot[0] != 0) {
        uint32 scratch[2];
        uint16 scratchShort = 0;
        uint8 flagByte = 0;
        uint8 scratchBytes[9];
        scratch[0] = 0;
        scratch[1] = 0;
        (void)scratchShort;
        XAnim_UpdateBlendRecursive((struct XAnimTree *)(size_t)slot[0], 0, (float *)scratch, chunk102_float_from_bits(arg), 1, scratchBytes, &flagByte);
        XAnim_AdvanceNodeCursor((struct XAnimTree *)(size_t)slot[0], 0, chunk102_float_from_bits(arg), 1);
    }
}

// 0x080c1e76  size=90  FUN_080c1e76  status=translated  naming=unknown
// evidence: node k of the XAnim node table (table + 0x0c + 8k) recurses into its children
// (count at node +0, first child index at node +6) through XAnim_SetSlotTime.
void XAnim_NotifyNodeChildren(int32 *tree, int32 node, float time)
{
    struct xanim_node_chunk102_s *n =
        (struct xanim_node_chunk102_s *)((uint8 *)(size_t)tree[0] + 0x0c) + node;
    int count = (int)n->child_count;
    int i;
    for (i = 0; i < count; i++) {
        XAnim_SetSlotTime((struct XAnimTree *)(void *)tree, (int32)n->first_child + i, time);
    }
}

// 0x080c2d6a  size=122  FUN_080c2d6a  status=translated  naming=unknown
// evidence: for every node with a non-zero blend-slot index (uint16 array at tree + 8), queries
// XAnim_AllocSlot(ctx, node) and applies it to the blend slot g_xanimSlotRing[slot] (0x28-byte entries).
void XAnim_ApplyBlendSlots(int32 *tree, uint32 ctx)
{
    int32 count = *(const int32 *)((const uint8 *)(size_t)tree[0] + 4);
    int k;
    for (k = 0; k < count; k++) {
        uint16 slot = *(const uint16 *)((const uint8 *)tree + 8 + k * 2);
        if (slot != 0) {
            struct XAnimSlot *v = XAnim_AllocSlot((struct XAnimTree *)(size_t)ctx, k);
            XAnim_InitSlotRecord((const struct XAnimSlot *)((uint8 *)g_xanimSlotRing + (uint32)slot * COD2_BLEND_SLOT_STRIDE), v);
        }
    }
}

// 0x080c56f8  size=27  FUN_080c56f8  status=translated  naming=unknown
// evidence: Hunk_FindUserEntryByName(type 4, name) with the result discarded.
void Hunk_FindUserEntryType4(const char *name)
{
    (void)Hunk_FindUserEntryByName(4, name);
}

// 0x080c7a0e  size=79  FUN_080c7a0e  status=translated  naming=unknown
// evidence: literal "DEFAULT" when the handle at table(row) + col*2 is zero; otherwise the text of
// that script string handle.
const char *XAnim_LookupNameOrDefault(const void *table, int32 col, int32 row)
{
    const uint8 *t = (const uint8 *)table;
    int16 handle = *(const int16 *)(*(const uint8 * const *)(t + row * 0x14 + 0x10) + col * 2);
    if (handle == 0) {
        return "DEFAULT";
    }
    return ScrStr_TextFromHandle((uint32)(int32)handle);
}

// 0x080d3bdc  size=104  FUN_080d3bdc  status=translated  naming=inferred
// evidence: "Warning: %s" to stderr with the same tty toggling as the console path (ttycon_on).
// Lineage: Q3 unix Sys_Warn (naming reference only).
void Sys_Warn(const char *fmt, ...)
{
    char buf[1036];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if (ttycon_on != 0) {
        Tty_HideInput();
    }
    fprintf(stderr, "Warning: %s", buf);
    if (ttycon_on != 0) {
        Tty_ShowInput();
    }
}

// 0x080d43c8  size=28  FUN_080d43c8  status=translated  naming=unknown
// evidence: "Unable to load shared library" via Com_Error(0, ...).
void Sys_DllLoadFailed(void)
{
    Com_Error(0, "Unable to load shared library\n");
}

// 0x080d43e4  size=861  FUN_080d43e4  status=translated  naming=observed
// evidence: literal "Sys_LoadDll(%s)... " and "Sys_LoadDll(%s) succeeded!" are in the function;
// its body searches cwd, fs_homepath and fs_basepath for "<name>.mp.i386.so" with dlopen.
int Sys_LoadDll(const char *name, char *outPath, int32 *vmMainOut, uint32 arg)
{
    char qpath[264];
    char path[256];
    const char *cwd;
    const char *home;
    const char *base;
    const char *game;
    void *handle;
    dllEntry_fn dllEntry;
    int32 vmMain;
    const char *err;

    *outPath = '\0';
    snprintf(qpath, 0x100, "%s.mp.i386.so", name);
    cwd = Sys_Cwd();
    home = Dvar_GetString("fs_homepath");
    base = Dvar_GetString("fs_basepath");
    game = Dvar_GetString("fs_game");

    FS_BuildOSPath_Fatal(cwd, game, qpath, path);
    Com_Printf("Sys_LoadDll(%s)... ", path);
    handle = dlopen(path, 2);
    if (handle == 0) {
        Com_Printf("failed\n");
        FS_BuildOSPath_Fatal(home, game, qpath, path);
        Com_Printf("Sys_LoadDll(%s)... ", path);
        handle = dlopen(path, 2);
        if (handle == 0) {
            Com_Printf("failed\n");
            FS_BuildOSPath_Fatal(base, game, qpath, path);
            Com_Printf("Sys_LoadDll(%s)... ", path);
            handle = dlopen(path, 2);
            if (handle == 0) {
                err = dlerror();
                Com_Printf("\nSys_LoadDll(%s) failed:\n\"%s\"\n", path, err);
            } else {
                Com_Printf("ok\n");
            }
            if (handle == 0) {
                Com_Error(0, "Sys_LoadDll(%s) failed dlopen() completely!\n", name);
                return 0;
            }
        } else {
            Com_Printf("ok\n");
        }
    } else {
        Com_Printf("ok\n");
    }

    Q_strncpyz(outPath, path, 0x40);
    dllEntry = (dllEntry_fn)dlsym(handle, "dllEntry");
    vmMain = (int32)(size_t)dlsym(handle, "vmMain");
    *vmMainOut = vmMain;
    if (*vmMainOut == 0 || dllEntry == 0) {
        err = dlerror();
        Com_Error(0, "Sys_LoadDll(%s) failed dlsym(vmMain):\n\"%s\" !\n", name, err);
        dlclose(handle);
        err = dlerror();
        if (err != 0) {
            Com_Printf("Sys_LoadDll(%s) failed dlcose:\n\"%s\"\n", name, err);
        }
        return 0;
    }
    Com_Printf("Sys_LoadDll(%s) found **vmMain** at  %p  \n", name, (void *)(size_t)*vmMainOut);
    dllEntry(arg);
    Com_Printf("Sys_LoadDll(%s) succeeded!\n", name);
    return (int)(size_t)handle;
}

// 0x080d4f7c  size=419  FUN_080d4f7c  status=translated  naming=inferred
// evidence: process entry body (argc/argv in, never returns): uid setup, Dvar/Sys init, builds the
// command line, Com_Init, then the Com_Frame loop with 5 ms sleeps. Lineage: Q3 unix_main.c main().
int main(int argc, char **argv)   /* was Sys_Main; ELF entry per __libc_start_main call in _start */
{
    unsigned int uid;   /* uid_t as declared by the shared prototypes */
    int total;
    int i;
    char *cmdline;
    int fl;

    g_085e5044 = (int32)geteuid();
    uid = getuid();
    seteuid(uid);
    Com_Noop_08062b64();
    Sys_InitMutexes();
    Sys_SetMainThread();
    Dvar_InitSvCheats();
    Sys_ParseArgs(argc, argv);
    Sys_SetDefaultCDPath("");

    total = 1;
    for (i = 1; i < argc; i++) {
        total = (int)strlen(argv[i]) + total + 1;
    }
    cmdline = new char[total];
    *cmdline = '\0';
    for (i = 1; i < argc; i++) {
        if (i > 1) {
            strcat(cmdline, " ");
        }
        strcat(cmdline, argv[i]);
    }

    memset(eventQue, 0, 0x1800);
    memset(sys_packetReceived, 0, 0x20000);
    (void)Sys_Milliseconds();
    Com_Init(cmdline);
    Sys_ConsoleInputInit();
    fl = fcntl(0, 3 /* F_GETFL */, 0);
    fcntl(0, 4 /* F_SETFL */, fl | 0x800 /* O_NONBLOCK */);
    PB_ServerInit();
    for (;;) {
        Sys_ConfigureFPU();
        usleep(5000);
        Com_Frame();
        PB_QueryServerEvent0();
    }
}

// 0x080d5120  size=33  FUN_080d5120  status=translated  naming=observed
// evidence: literal "Sys_LoadRenderer is unimplemented" in the function.
int Sys_LoadRenderer(void)
{
    Com_Error(0, "Sys_LoadRenderer is unimplemented\n");
    return 0;
}

// 0x080d5142  size=28  FUN_080d5142  status=translated  naming=observed
// evidence: literal "Sys_UnloadRenderer is unimplemented" in the function.
void Sys_UnloadRenderer(void)
{
    Com_Error(0, "Sys_UnloadRenderer is unimplemented\n");
}

// 0x080d65fa  size=35  FUN_080d65fa  status=translated  naming=inferred
// evidence: copies the argument into sys_installpath (0x100 bytes, global name from symbols).
void Sys_SetInstallPath(const char *path)
{
    Q_strncpyz(sys_installpath, path, 0x100);
}

// 0x080d6644  size=35  FUN_080d6644  status=translated  naming=inferred
// evidence: copies the argument into sys_homepath (0x100 bytes, global name from symbols).
void Sys_SetHomePath(const char *path)
{
    Q_strncpyz(sys_homepath, path, 0x100);
}

// 0x080d6c7c  size=41  FUN_080d6c7c  status=translated  naming=unknown
// evidence: registers the bool dvar "snd_errorOnMissing" (flags 0x1001) into g_0860b3ec.
void Snd_RegisterErrorOnMissing(void)
{
    g_0860b3ec = Dvar_RegisterBool("snd_errorOnMissing", 0, 0x1001);
}

// 0x080d9cac  size=83  FUN_080d9cac  status=translated  naming=unknown
// evidence: looks up a record for (obj[2], flags & ~0x200) and tests the exponent bits of the
// float at +0x54 (non-finite value test).
bool NonFinite_FieldTest(const int32 *obj, uint32 flags)
{
    const uint8 *entry = (const uint8 *)BG_GetAnimationForIndex((uint32)obj[2], flags & 0xfffffdffu);
    return (*(const uint32 *)(entry + 0x54) & 0x7ff00000u) != 0;
}

// 0x080eb9b4  size=162  FUN_080eb9b4  status=translated  naming=unknown
// evidence: literal "defaultweapon_mp" is a lookup key (weapon name), not the function name; copies
// the default weapon's fields 0x0c, 0x10 and 0x14..0x5c into the weapon record at the given index.
void WeaponDef_CopyDefaultFields(int32 index)
{
    int32 src = BG_RegisterWeaponNoCallback("defaultweapon_mp");
    struct weaponDef_s *dst;
    struct weaponDef_s *from;
    int i;
    if (src == 0) {
        Com_Error(1, "\x15" "CG_SetWeaponDefToDefaultWeapon: weapon def for default weapon could not be found!");
    }
    dst = bg_weaponDefs[index];
    from = bg_weaponDefs[src];
    dst->gunModel = from->gunModel;
    dst->handModel = from->handModel;
    for (i = 0; i < 0x17; i++) {
        ((int32 *)&dst->unk_0x14)[i] = ((const int32 *)&from->unk_0x14)[i];
    }
}

// 0x080ebaf8  size=107  FUN_080ebaf8  status=translated  naming=unknown
// evidence: linear search of the weapon-def table (count bg_numAmmoTypes) by ammoName; Com_DPrintf on
// a miss. Returns the table index, 0 on miss.
int32 WeaponDef_FindAmmoType(const char *name)
{
    int32 i = 0;
    for (;;) {
        if (bg_numAmmoTypes <= i) {
            Com_DPrintf("Couldn't find ammo type \"%s\"\n", name);
            return 0;
        }
        if (Q_stricmp(bg_ammoTypes[i]->ammoName, name) == 0) {
            break;
        }
        i++;
    }
    return i;
}

// 0x080ebb64  size=107  FUN_080ebb64  status=translated  naming=unknown
// evidence: linear search of the clip table (count bg_numClipTypes) by clipName; Com_DPrintf on miss.
int32 WeaponDef_FindAmmoClip(const char *name)
{
    int32 i = 0;
    for (;;) {
        if (bg_numClipTypes <= i) {
            Com_DPrintf("Couldn't find ammo clip \"%s\"\n", name);
            return 0;
        }
        if (Q_stricmp(bg_clipTypes[i]->clipName, name) == 0) {
            break;
        }
        i++;
    }
    return i;
}

// 0x080ec706  size=454  FUN_080ec706  status=translated  naming=unknown
// evidence: sums the ammo (+0x144) or clip (+0x344) of the client's owned weapons that share the
// weapon class; a weapon without class (negative) counts only its own slot. The 0x200-byte seen
// marks deduplicate slots.
int32 Player_GetAmmoTotalForWeapon(struct gclient_s *cl, int32 weapon)
{
    int32 seenA[130];
    int32 seenB[128];
    int32 total = 0;
    int32 slotA;
    int32 slotB;
    struct weaponDef_s *def;
    int32 i;

    slotA = BG_GetWeaponAmmoIndex(weapon);
    slotB = BG_GetWeaponClipIndex(weapon);
    memset(seenA, 0, 0x200);
    memset(seenB, 0, 0x200);
    def = BG_GetWeaponDef102(weapon);

    if (def->iSharedAmmoCapIndex < 0) {
        if (BG_GetWeaponClipOnly(weapon) == 0) {
            total = cl->ps.ammo[slotA];
        } else {
            total = cl->ps.ammoclip[slotB];
        }
    } else {
        for (i = 1; i <= bg_numWeapons; i++) {
            if (BitSet_Test(cl->ps.weapons, i)) {
                struct weaponDef_s *other = BG_GetWeaponDef102(i);
                if (other->iSharedAmmoCapIndex == def->iSharedAmmoCapIndex) {
                    if (BG_GetWeaponClipOnly(i) == 0) {
                        int32 a = BG_GetWeaponAmmoIndex(i);
                        if (seenA[a] == 0) {
                            a = BG_GetWeaponAmmoIndex(i);
                            seenA[a] = 1;
                            a = BG_GetWeaponAmmoIndex(i);
                            total += cl->ps.ammo[a];
                        }
                    } else {
                        int32 b = BG_GetWeaponClipIndex(i);
                        if (seenB[b] == 0) {
                            b = BG_GetWeaponClipIndex(i);
                            seenB[b] = 1;
                            b = BG_GetWeaponClipIndex(i);
                            total += cl->ps.ammoclip[b];
                        }
                    }
                }
            }
        }
    }
    return total;
}

// 0x080f2de8  size=801  FUN_080f2de8  status=translated  naming=unknown
// evidence: enum field parser; each case looks up the text in a rodata name table and reports
// "Unknown weapon <kind> \"%s\" in \"%s\"" on a miss (rodata 0x081571c0..0x08157380).
int32 WeaponDef_ParseEnumField(struct weaponParse_s *wp, const char *text, int32 fieldType)
{
    int32 idx;
    switch (fieldType) {
    case 8:
        idx = StrArray_FindCI(text, bg_weaponTypeNames, 4);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown weapon type \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->weaponType = idx;
        break;
    case 9:
        idx = StrArray_FindCI(text, bg_weaponClassNames, 10);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown weapon class \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->weaponClassEnum = idx;
        break;
    case 10:
        idx = StrArray_FindCI(text, bg_overlayReticleNames, 5);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown weapon overlay reticle \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->overlayReticle = idx;
        break;
    case 0xb:
        idx = StrArray_FindCI(text, bg_weaponSlotNames, 3);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown weapon slot \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->weaponSlotEnum = idx;
        break;
    case 0xc:
        idx = StrArray_FindCI(text, bg_weaponStanceNames, 3);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown weapon stance \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->stance = idx;
        break;
    case 0xd:
        idx = StrArray_FindCI(text, bg_projExplosionNames, 3);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown projectile explosion \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->projectileExplosion = idx;
        break;
    case 0xe:
        idx = StrArray_FindCI(text, bg_offhandClassNames, 3);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown offhand class \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->offhandClass = idx;
        break;
    case 0xf:
        idx = StrArray_FindCI(text, g_playerAnimTypeStrings, g_playerAnimTypeCount);
        if (idx < 0) {
            Com_Error(1, "\x15" "Unknown playerAnimType \"%s\" in \"%s\"\n", text, wp->name);
        }
        wp->playerAnimType = idx;
        break;
    default:
        Com_Error(1, "\x15" "Bad field type %i in %s\n", fieldType, wp->name);
        return 0;
    }
    return 1;
}

// 0x080f729a  size=94  FUN_080f729a  status=translated  naming=unknown
// evidence: sets g_gameContextPtr to the game-locals block and hands the server DObj, the entity and
// the 0x4b8-stride animation record (g_clientInfos) to PlayerView_ApplySwayBlend.
void DObj_ServerAnimUpdate(int32 *ent, uint32 arg)
{
    int32 iVar1 = ent[0x24];
    struct DObj *obj;
    g_gameContextPtr = g_levelBlock;
    obj = Com_GetServerDObj(ent[0]);
    PlayerView_ApplySwayBlend((unsigned int)(size_t)obj, (struct entityState_s *)(void *)ent, arg, (struct clientInfo_s *)((uint8 *)g_clientInfos + iVar1 * 0x4b8), level_frameTime);
}

// 0x080f7d90  size=37  FUN_080f7d90  status=translated  naming=unknown
// evidence: "player field %s is read-only" through Scr_Error.
void Player_ReadOnlyField(uint32 unused, const uint32 *field)
{
    const char *msg = va("player field %s is read-only", (const char *)(size_t)*field);
    (void)unused;
    Scr_Error(msg);
}

// 0x080f7db6  size=205  FUN_080f7db6  status=translated  naming=unknown
// evidence: maps the script team string to 0 none, 1 allies, 2 axis, 3 spectator; error
// "'%s' is an illegal sessionteam string". Then refreshes the client (ClientUserinfoChanged, G_CalculateRanks).
void Player_SetSessionTeam(struct gclient_s *cl)
{
    int16 sVar1 = (int16)Scr_GetAny(0);
    if (sVar1 == (int16)scr_str_axis) {
        cl->cs.team = 1;
    } else if (sVar1 == (int16)scr_str_allies) {
        cl->cs.team = 2;
    } else if (sVar1 == (int16)scr_str_spectator) {
        cl->cs.team = 3;
    } else if (sVar1 == (int16)scr_str_none) {
        cl->cs.team = 0;
    } else {
        const char *text = ScrStr_TextFromHandle((uint32)(int32)sVar1);
        const char *msg = va("'%s' is an illegal sessionteam string. Must be allies, axis, none, or spectator.", text);
        Scr_Error(msg);
    }
    ClientUserinfoChanged(chunk102_client_index(cl));
    G_CalculateRanks();
}

// 0x080f7e84  size=120  FUN_080f7e84  status=translated  naming=unknown
// evidence: inverse of Player_SetSessionTeam (script string id per team value).
void Player_GetSessionTeam(struct gclient_s *cl)
{
    int32 t = cl->cs.team;
    if (t == 1) {
        Scr_AddStringId(scr_str_axis);
    } else if (t < 2) {
        if (t == 0) {
            Scr_AddStringId(scr_str_none);
        }
    } else if (t == 2) {
        Scr_AddStringId(scr_str_allies);
    } else if (t == 3) {
        Scr_AddStringId(scr_str_spectator);
    }
}

// 0x080f7efc  size=196  FUN_080f7efc  status=translated  naming=unknown
// evidence: sessionstate string parser; "intermission" toggles the 0x2 flag of the client flags.
// Error "'%s' is an illegal sessionstate string. Must be playing, dead, spectator, or intermission."
void Player_SetSessionState(struct gclient_s *cl)
{
    int16 sVar1 = (int16)Scr_GetAny(0);
    if (sVar1 == (int16)scr_str_playing) {
        cl->sessionState = 0;
    } else if (sVar1 == (int16)scr_str_dead) {
        cl->sessionState = 1;
    } else if (sVar1 == (int16)scr_str_spectator) {
        cl->sessionState = 2;
    } else if (sVar1 == (int16)scr_str_intermission) {
        cl->ps.eFlags = cl->ps.eFlags ^ 2u;
        cl->sessionState = 3;
    } else {
        const char *text = ScrStr_TextFromHandle((uint32)(int32)sVar1);
        const char *msg = va("'%s' is an illegal sessionstate string. Must be playing, dead, spectator, or intermission.", text);
        Scr_Error(msg);
    }
}

// 0x080f7fc0  size=120  FUN_080f7fc0  status=translated  naming=unknown
// evidence: inverse of Player_SetSessionState.
void Player_GetSessionState(struct gclient_s *cl)
{
    int32 t = cl->sessionState;
    if (t == 1) {
        Scr_AddStringId(scr_str_dead);
    } else if (t < 2) {
        if (t == 0) {
            Scr_AddStringId(scr_str_playing);
        }
    } else if (t == 2) {
        Scr_AddStringId(scr_str_spectator);
    } else if (t == 3) {
        Scr_AddStringId(scr_str_intermission);
    }
}

// 0x080f8038  size=171  FUN_080f8038  status=translated  naming=unknown
// evidence: stores the requested health (min 1), lowers maxHealth to it when smaller, copies
// maxHealth into the entity health (gentity +0x194) and sets the current health.
void Player_SetMaxHealth(struct gclient_s *cl)
{
    cl->maxHealth = Scr_GetInt(0);
    if (cl->maxHealth < 1) {
        cl->maxHealth = 1;
    }
    if (cl->maxHealth < cl->ps.stats[0]) {
        cl->ps.stats[0] = cl->maxHealth;
    }
    g_gentities[chunk102_client_index(cl)].health = cl->ps.stats[0];
    cl->ps.stats[2] = cl->maxHealth;
}

// 0x080f80e4  size=40  FUN_080f80e4  status=translated  naming=unknown
// evidence: stores Scr_GetInt(0) at +0x26b8 and runs G_CalculateRanks.
void Player_SetScore(struct gclient_s *cl)
{
    cl->score = Scr_GetInt(0);
    G_CalculateRanks();
}

// 0x080f810c  size=61  FUN_080f810c  status=translated  naming=unknown
// evidence: "spectatorclient can only be set to -1, or a valid client number".
void Player_SetSpectatorClient(struct gclient_s *cl)
{
    int32 v = Scr_GetInt(0);
    if (v < -1 || 0x3f < v) {
        Scr_Error("spectatorclient can only be set to -1, or a valid client number");
    }
    cl->spectatorClient = v;
}

// 0x080f814a  size=48  FUN_080f814a  status=translated  naming=unknown
// evidence: converts the script value with G_StatusIconIndex and stores it as the configstring index.
void Player_SetStatusIcon(struct gclient_s *cl)
{
    uint32 v = (uint32)(size_t)Scr_ReleaseAnyValue(0);
    v = G_StatusIconIndex((const char *)(size_t)v);
    cl->killCamEntity = (int32)v;
}

// 0x080f817a  size=89  FUN_080f817a  status=translated  naming=unknown
// evidence: returns configstring (configIndex + 0x16) text, or "" when the index is zero.
void Player_GetStatusIcon(struct gclient_s *cl)
{
    if (cl->killCamEntity == 0) {
        Scr_AddString("");
    } else {
        char buf[1032];
        SV_GetConfigstring(cl->killCamEntity + 0x16, buf, 0x400);
        Scr_AddString(buf);
    }
}

// 0x080f81d4  size=86  FUN_080f81d4  status=translated  naming=unknown
// evidence: stores the converted head-icon value into entity[client index].s.iHeadIcon (+0x94).
void Player_SetHeadIcon(struct gclient_s *cl)
{
    int idx = chunk102_client_index(cl);
    uint32 v = (uint32)(size_t)Scr_ReleaseAnyValue(0);
    v = (uint32)G_HeadIconIndex((const char *)(size_t)v);
    g_gentities[idx].s.iHeadIcon = (int32)v;
}

// 0x080f822a  size=139  FUN_080f822a  status=translated  naming=unknown
// evidence: head icon getter; index 0 gives "", indices below 0x10 read configstring +0x1e.
void Player_GetHeadIcon(struct gclient_s *cl)
{
    int idx = chunk102_client_index(cl);
    int32 hi = g_gentities[idx].s.iHeadIcon;
    if (hi == 0) {
        Scr_AddString("");
    } else if (hi < 0x10) {
        char buf[1036];
        SV_GetConfigstring(hi + 0x1e, buf, 0x400);
        Scr_AddString(buf);
    }
}

// 0x080f82b6  size=210  FUN_080f82b6  status=translated  naming=unknown
// evidence: head-icon team parser (none 0, allies 1, axis 2, other 3); spectator is an error
// "'%s' is an illegal head icon team string. Must be none, allies, axis, or spectator."
void Player_SetHeadIconTeam(struct gclient_s *cl)
{
    int idx = chunk102_client_index(cl);
    int16 sVar1 = (int16)Scr_GetAny(0);
    if (sVar1 == (int16)scr_str_none) {
        g_gentities[idx].s.iHeadIconTeam = 0;
    } else if (sVar1 == (int16)scr_str_allies) {
        g_gentities[idx].s.iHeadIconTeam = 2;
    } else if (sVar1 == (int16)scr_str_axis) {
        g_gentities[idx].s.iHeadIconTeam = 1;
    } else if (sVar1 == (int16)scr_str_spectator) {
        const char *text = ScrStr_TextFromHandle((uint32)(int32)sVar1);
        const char *msg = va("'%s' is an illegal head icon team string. Must be none, allies, axis, or spectator.", text);
        Scr_Error(msg);
    } else {
        g_gentities[idx].s.iHeadIconTeam = 3;
    }
}

// 0x080f8388  size=152  FUN_080f8388  status=translated  naming=unknown
// evidence: inverse of Player_SetHeadIconTeam.
void Player_GetHeadIconTeam(struct gclient_s *cl)
{
    int idx = chunk102_client_index(cl);
    int32 t = g_gentities[idx].s.iHeadIconTeam;
    if (t == 2) {
        Scr_AddStringId(scr_str_allies);
    } else {
        if (t < 3) {
            if (t == 1) {
                Scr_AddStringId(scr_str_axis);
                return;
            }
        } else if (t == 3) {
            Scr_AddStringId(scr_str_spectator);
            return;
        }
        Scr_AddStringId(scr_str_none);
    }
}

// 0x080f8420  size=63  FUN_080f8420  status=translated  naming=unknown
// evidence: stores (int)(1000.0f * Scr_GetFloat(0)) at +0x26b4; the product is taken in long double
// and truncated (x87 chop, rodata 0x08157c54 = 1000.0f).
void Player_SetArchiveTime(struct gclient_s *cl)
{
    float f = Scr_GetFloat(0);
    cl->archiveTime = (int32)((long double)1000.0f * (long double)f);
}

// 0x080f8460  size=33  FUN_080f8460  status=translated  naming=unknown
// evidence: pushes 0.001f * scaled value as float bits (rodata 0x08157c58 = 0.001f).
void Player_GetArchiveTime(struct gclient_s *cl)
{
    float v = 0.001f * (float)cl->archiveTime;
    Scr_AddFloatBits(chunk102_bits_from_float(v));
}

// 0x080f8482  size=34  FUN_080f8482  status=translated  naming=unknown
// evidence: stores Scr_GetInt(0) at +0x27a4.
void Player_SetPsOffsetTime(struct gclient_s *cl)
{
    cl->psOffsetTime = Scr_GetInt(0);
}

// 0x080f84a4  size=25  FUN_080f84a4  status=translated  naming=unknown
// evidence: pushes the scaled value at +0x26b4 as an index.
void Player_GetPsOffsetTime(struct gclient_s *cl)
{
    Scr_AddIndex(cl->archiveTime);
}

// 0x080f9b8c  size=510  FUN_080f9b8c  status=translated  naming=unknown
// evidence: strings "Cannot give %s weapon %s without having an empty weapon slot - player
// currently has a %s and a %s", "%c \"%i\"". Gives a weapon to a player entity: ammo for the
// missing amount, server notice, owned-weapon slot check.
void PlayerCmd_giveweapon(uint32 entRef)
{
    struct gentity_s *ent = chunk102_player_entity_from_ref(entRef);
    uint32 nameWord;
    int32 weapon;
    struct gclient_s *cl;
    int owned;
    struct weaponDef_s *def;
    int32 remaining;

    nameWord = (uint32)(size_t)Scr_ReleaseAnyValue(0);
    weapon = G_LoadWeaponDef((const char *)(size_t)nameWord);
    cl = ent->client;
    owned = BitSet_Test(cl->ps.weapons, weapon) ? 1 : 0;
    def = BG_GetWeaponDef102(weapon);
    if (BG_WeaponFlagA(weapon)) {
        if (BG_FindFreeHeldSlot(&cl->ps, weapon) == 0) {
            struct weaponDef_s *slot556 = BG_GetWeaponDef102((int32)cl->ps.weaponslots[2]);
            struct weaponDef_s *slot555 = BG_GetWeaponDef102((int32)cl->ps.weaponslots[1]);
            const char *msg = va("Cannot give %s weapon %s without having an empty weapon slot - player currently has a %s and a %s\n",
                                 cl->cs.name, def->displayName,
                                 slot555->displayName, slot556->displayName);
            Scr_ErrorAtParam(0, msg);
        }
    }
    if (G_TouchTriggerEntity(cl, weapon) != 0) {
        const char *msg = va("%c \"%i\"", 0x49, 1);
        SV_SendServerCommandNum((int)(ent - g_gentities), 0, msg);
    }
    remaining = def->startAmmo - cl->ps.ammo[def->iAmmoIndex];
    if (0 < remaining) {
        Player_GiveWeaponAmmo(ent, weapon, remaining, !owned);
    }
}

// 0x080f9d8a  size=218  FUN_080f9d8a  status=translated  naming=unknown
// evidence: zeroes the ammo and clip of the weapon's slots and runs BG_TakeWeapon (remove weapon).
void PlayerCmd_takeweapon(uint32 entRef)
{
    struct gentity_s *ent = chunk102_player_entity_from_ref(entRef);
    uint32 nameWord;
    int32 weapon;
    nameWord = (uint32)(size_t)Scr_ReleaseAnyValue(0);
    weapon = G_LoadWeaponDef((const char *)(size_t)nameWord);
    ent->client->ps.ammo[BG_GetWeaponAmmoIndex(weapon)] = 0;
    ent->client->ps.ammoclip[BG_GetWeaponClipIndex(weapon)] = 0;
    BG_TakeWeapon(&ent->client->ps, weapon);
}

// 0x080f9e64  size=234  FUN_080f9e64  status=translated  naming=unknown
// evidence: clears the script weapon (+0xd4) and every weapon's ammo, clip and state.
void PlayerCmd_takeallweapons(uint32 entRef)
{
    struct gentity_s *ent = chunk102_player_entity_from_ref(entRef);
    int32 i;
    ent->client->ps.weapon = 0;
    i = 1;
    for (;;) {
        if (BG_GetNumWeapons() < i) {
            break;
        }
        ent->client->ps.ammo[BG_GetWeaponAmmoIndex(i)] = 0;
        ent->client->ps.ammoclip[BG_GetWeaponClipIndex(i)] = 0;
        BG_TakeWeapon(&ent->client->ps, i);
        i++;
    }
}

// 0x080f9f6a  size=198  FUN_080f9f6a  status=translated  naming=unknown
// evidence: pushes the name of the script weapon at +0xd4 or "none" (mirrors the +0xd0 variant).
void PlayerCmd_getcurrentweapon(uint32 entRef)
{
    struct gentity_s *ent = chunk102_player_entity_from_ref(entRef);
    if (G_ClientIsPlaying(ent) == 0) {
        Scr_AddString("none");
    } else if (ent->client->ps.weapon < 1) {
        Scr_AddString("none");
    } else {
        struct weaponDef_s *def = BG_GetWeaponDef102(ent->client->ps.weapon);
        Scr_AddString(def->szInternalName);
    }
}

// 0x080fa030  size=198  FUN_080fa030  status=translated  naming=unknown
// evidence: the +0xd0 variant of PlayerCmd_getcurrentweapon.
void PlayerCmd_getcurrentoffhand(uint32 entRef)
{
    struct gentity_s *ent = chunk102_player_entity_from_ref(entRef);
    if (G_ClientIsPlaying(ent) == 0) {
        Scr_AddString("none");
    } else if (ent->client->ps.offHandIndex < 1) {
        Scr_AddString("none");
    } else {
        struct weaponDef_s *def = BG_GetWeaponDef102(ent->client->ps.offHandIndex);
        Scr_AddString(def->szInternalName);
    }
}

// 0x080fa0f6  size=192  FUN_080fa0f6  status=translated  naming=unknown
// evidence: pushes 1 when the player owns the named weapon, else 0.
void PlayerCmd_hasweapon(uint32 entRef)
{
    struct gentity_s *ent = chunk102_player_entity_from_ref(entRef);
    uint32 nameWord = (uint32)(size_t)Scr_ReleaseAnyValue(0);
    int32 weapon = BG_FindWeaponByName((const char *)(size_t)nameWord);
    if (weapon == 0 || !BitSet_Test(ent->client->ps.weapons, weapon)) {
        Scr_AddInt(0);
    } else {
        Scr_AddInt(1);
    }
}

// 0x080fa1b6  size=246  FUN_080fa1b6  status=translated  naming=unknown
// evidence: "unknown weapon '%s'" error; when owned, selects the weapon (G_SendCharIndexA) and pushes 1.
void PlayerCmd_switchtoweapon(uint32 entRef)
{
    struct gentity_s *ent = chunk102_player_entity_from_ref(entRef);
    uint32 nameWord = (uint32)(size_t)Scr_ReleaseAnyValue(0);
    int32 weapon = G_LoadWeaponDef((const char *)(size_t)nameWord);
    if (weapon == 0) {
        const char *msg = va("unknown weapon '%s'", (const char *)(size_t)nameWord);
        Scr_ErrorAtParam(0, msg);
    }
    if (!BitSet_Test(ent->client->ps.weapons, weapon)) {
        Scr_AddInt(0);
    } else {
        G_SendCharIndexA((uint16)entRef, weapon);
        Scr_AddInt(1);
    }
}
