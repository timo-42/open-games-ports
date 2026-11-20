/*
 * chunk_029.cpp - cod2_lnxded 1.3, 0x080b27d2..0x080b5f32 (104 functions).
 *
 * Subsystems:
 *   - dvar (dynamic variable) store: pool, name-sorted list, hash buckets,
 *     value/latch/reset words, set/latch/restart policy and typed accessors;
 *   - float/vector helpers used by dvar code;
 *   - bounded message buffers (read/write with overflow flag);
 *   - parse-session guard used while nested parses are active.
 *
 * Every function is translated from the observed disassembly. Behaviour
 * quirks are kept and listed in the notes; nothing is fixed.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_029_types.h"

#include <strings.h>

/* Writer transfer tag: MsgBuf_WriteBytes (chunk 102, 0x080b5ae4) comes from the shared prototypes.
 * Chunk-internal functions are declared by cod2_prototypes.h (generated from chunk_029.json). */

/* Byte-offset slot access, matching the binary's index * 0x45c arithmetic. */
#define PARSE_SLOT_AT(session, k) \
    ((struct ParseSession *)((uint8 *)(session) + (k) * PARSE_SLOT_STRIDE))

/* Default parse prefix: the empty literal at rodata 0x0814f5ca (the NUL byte before "Already parsing:"). */
static const char kAlreadyParsing[] = "";
/* Read-only vec4 (1,1,1,1) used as the default colour. */
static const float kDvarWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};

/* Packs four float colour components as the binary does for COLOR dvars:
 * clamp to [0,1] (Float_Min_080b56ea then Float_Max_080b56c6), scale by 255, round half up,
 * and place r in the low byte. Shared by Dvar_RegisterColor and
 * Dvar_SetVec4OrColorEx (same arithmetic in both). */
static uint32 dvar_pack_color(float r, float g, float b, float a)
{
    uint32 packed;
    uint32 comp;

    comp = (uint8)Float_RoundToInt_080b570e(255.0f * Float_Max_080b56c6(0.0f, Float_Min_080b56ea(1.0f, r)));
    packed = comp;
    comp = (uint8)Float_RoundToInt_080b570e(255.0f * Float_Max_080b56c6(0.0f, Float_Min_080b56ea(1.0f, g)));
    packed = (packed & 0xffff00ffu) | (comp << 8);
    comp = (uint8)Float_RoundToInt_080b570e(255.0f * Float_Max_080b56c6(0.0f, Float_Min_080b56ea(1.0f, b)));
    packed = (packed & 0xff00ffffu) | (comp << 16);
    comp = (uint8)Float_RoundToInt_080b570e(255.0f * Float_Max_080b56c6(0.0f, Float_Min_080b56ea(1.0f, a)));
    packed = (packed & 0x00ffffffu) | (comp << 24);
    return packed;
}

/* ======================================================================== */
// 0x080b27d2  size=74  Dvar_DomainString  status=translated  naming=inferred
// Evidence: sole worker of the domain-description path; callers print its
// result with "  %s\n" (Dvar_PrintDomain). Forwards to Dvar_DomainText with flags 0.
char *Dvar_DomainString(int type, uint32 min, uint32 max, char *buf, int size)
{
    return Dvar_DomainText(type, min, max, buf, size, NULL);
}

// 0x080b281c  size=73  Dvar_DomainStringEx  status=translated  naming=inferred
// Evidence: same forwarding as Dvar_DomainString with the caller's flags word.
char *Dvar_DomainStringEx(int type, uint32 min, uint32 max, char *buf, int size, int32 *lineCount)
{
    return Dvar_DomainText(type, min, max, buf, size, lineCount);
}

// 0x080b2866  size=89  Dvar_PrintDomain  status=translated  naming=inferred
// Evidence: formats a domain string and prints it with "  %s\n"; called after
// the "'%s' is not a valid value for dvar '%s'" message in the set paths.
void Dvar_PrintDomain(int type, uint32 min, uint32 max)
{
    char buf[0x400];
    char *text;

    text = Dvar_DomainString(type, min, max, buf, 0x400);
    Com_Printf("  %s\n", text);
}

// 0x080b28c0  size=303  Dvar_ValuesEqual  status=translated  naming=inferred
// Evidence: type switch that compares dvar value words per type (bool byte,
// float, vec2/3/4, int, enum, string via strcmp, colour word). Name from role.
uint8 Dvar_ValuesEqual(int type, uint32 a, uint32 b)
{
    switch (type) {
    case 0:
        return (uint8)((a & 0xff) == (b & 0xff));
    case 1:
        return (uint8)(dvar_word_to_float(a) == dvar_word_to_float(b));
    case 2:
        return (uint8)Vec2_Equal((const float *)DVAR_AS_PTR(a), (const float *)DVAR_AS_PTR(b));
    case 3:
        return (uint8)Vec3_Equal_080b5864((const float *)DVAR_AS_PTR(a), (const float *)DVAR_AS_PTR(b));
    case 4:
        return (uint8)Vec4_Equal((const float *)DVAR_AS_PTR(a), (const float *)DVAR_AS_PTR(b));
    case 5:
    case 6:
        return (uint8)(a == b);
    case 7:
        return (uint8)(strcmp((const char *)DVAR_AS_PTR(a), (const char *)DVAR_AS_PTR(b)) == 0);
    case 8:
        return (uint8)Word_Equal(&a, &b);
    default:
        return 0;
    }
}

// 0x080b29f0  size=163  Dvar_SetLatched  status=translated  naming=inferred
// Evidence: writes the latched word (+0x0c) with per-type copy rules; the
// latch message "will be changed upon restarting" is set in Dvar_SetVariant.
void Dvar_SetLatched(struct dvar_s *d, uint32 value)
{
    switch (d->type) {
    case 3:
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        return;
    case 2:
        Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        return;
    case 4:
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        return;
    case 7:
        Dvar_ReleaseLatchedValue(d);
        Dvar_SetLatchedString(d, DVAR_AS_STR(value));
        return;
    default:
        d->latched.unsignedInt = value;
        return;
    }
}

// 0x080b2a94  size=52  Dvar_HasPendingLatch  status=translated  naming=inferred
// Returns true when the current value differs from the latched value.
uint8 Dvar_HasPendingLatch(struct dvar_s *d)
{
    uint8 same;

    same = Dvar_ValuesEqual(d->type, d->current.unsignedInt, d->latched.unsignedInt);
    return (uint8)(same ^ 1);
}

// 0x080b2ac8  size=50  Dvar_IsAtReset  status=translated  naming=inferred
// Returns true when the current value equals the reset value.
uint8 Dvar_IsAtReset(struct dvar_s *d)
{
    return Dvar_ValuesEqual(d->type, d->current.unsignedInt, d->reset.unsignedInt);
}

// 0x080b2afa  size=857  Dvar_SetVariant  status=translated  naming=observed
// Evidence: format string "      dvar set %s %s\n" in this function, plus
// "%s is read only.", "%s is write protected.", "%s is cheat protected.",
// "%s will be changed upon restarting." and the invalid-value message.
void Dvar_SetVariant(struct dvar_s *d, uint32 value, int source)
{
    const char *text;
    const char *msg;
    signed char valid;

    text = Dvar_FormatValue(d, value);
    msg = va("      dvar set %s %s\n", d->name, text);
    Com_PrintMessage(4, (char *)msg);

    valid = Dvar_ValueInDomain(d->type, value, d->domain.enumeration.stringCount, d->domain.integer.max);
    if (valid != 1) {
        text = Dvar_FormatValue(d, value);
        Com_Printf("'%s' is not a valid value for dvar '%s'\n", text, d->name);
        Dvar_PrintDomain(d->type, d->domain.enumeration.stringCount, d->domain.integer.max);
        if (d->type == DVAR_TYPE_ENUM) {
            Dvar_SetVariant(d, d->reset.unsignedInt, source);
        }
        return;
    }

    if (source == DVAR_SOURCE_USER || source == DVAR_SOURCE_OTHER) {
        if ((d->flags & DVAR_FLAG_READ_ONLY) != 0) {
            Com_Printf("%s is read only.\n", d->name);
            return;
        }
        if ((d->flags & DVAR_FLAG_WRITE_PROTECT) != 0) {
            Com_Printf("%s is write protected.\n", d->name);
            return;
        }
        /* No null check on the sv_cheats dvar here, as in the binary. */
        if (source == DVAR_SOURCE_USER && (d->flags & DVAR_FLAG_CHEAT_PROTECT) != 0 &&
            (int8)g_dvar_sv_cheats->current.unsignedInt == 0) {
            Com_Printf("%s is cheat protected.\n", d->name);
            return;
        }
        if ((d->flags & DVAR_FLAG_LATCH) != 0) {
            Dvar_SetLatched(d, value);
            if (Dvar_ValuesEqual(d->type, d->latched.unsignedInt, d->current.unsignedInt) == 1) {
                return;
            }
            Com_Printf("%s will be changed upon restarting.\n", d->name);
            return;
        }
    }

    if (Dvar_ValuesEqual(d->type, d->current.unsignedInt, value) != 0) {
        Dvar_SetLatched(d, d->current.unsignedInt);
        return;
    }

    dvar_modifiedFlags |= d->flags;
    switch (d->type) {
    case 3:
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        break;
    case 2:
        Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        break;
    case 4:
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        break;
    case 7:
        Dvar_ReleaseCurrentValue(d);
        Dvar_SetCurrentString(d, DVAR_AS_STR(value));
        Dvar_ReleaseLatchedValue(d);
        d->latched.unsignedInt = d->current.unsignedInt;
        break;
    default:
        d->current.unsignedInt = value;
        d->latched.unsignedInt = value;
        break;
    }
    d->modified = 1;
}

// 0x080b2e54  size=132  Dvar_FindHashed  status=translated  naming=inferred
// Hash lookup: bucket from generateHashValue(name), chain through hash_next.
struct dvar_s *Dvar_FindHashed(const char *name)
{
    struct dvar_s *d;

    Sys_EnterCriticalSection(4);
    d = dvar_hashTable[generateHashValue(name)];
    while (true) {
        if (d == 0) {
            Sys_LeaveCriticalSection(4);
            return 0;
        }
        if (Q_stricmp(name, d->name) == 0) {
            break;
        }
        d = d->hashNext;
    }
    Sys_LeaveCriticalSection(4);
    return d;
}

// 0x080b2ed8  size=19  Dvar_Lookup  status=translated  naming=inferred
// Thin wrapper around Dvar_FindHashed (callers use the returned pointer).
struct dvar_s *Dvar_Lookup(const char *name)
{
    return Dvar_FindHashed(name);
}

// 0x080b2eec  size=12  Dvar_ClearModified  status=translated  naming=inferred
// Clears dvar+0x07 (modified flag).
void Dvar_ClearModified(struct dvar_s *d)
{
    d->modified = 0;
}

// 0x080b2ef8  size=12  Dvar_MarkModified  status=translated  naming=inferred
// Sets dvar+0x07 (modified flag).
void Dvar_MarkModified(struct dvar_s *d)
{
    d->modified = 1;
}

// 0x080b2f04  size=166  Dvar_SetEnumList  status=translated  naming=inferred
// Enum domain update: count the NULL-terminated string list, store count and
// list in the domain words, re-resolve the value, and latch it.
void Dvar_SetEnumList(struct dvar_s *d, const char *const *list)
{
    int count;
    uint32 resolved;

    count = 0;
    while (list[count] != 0) {
        count++;
    }
    d->domain.enumeration.stringCount = (uint32)count;
    d->domain.enumeration.strings = (const char **)list;
    resolved = Dvar_ClampValue(d->type, d->current.unsignedInt, d->reset.unsignedInt, d->domain).unsignedInt;
    d->current.unsignedInt = resolved;
    d->latched.unsignedInt = d->current.unsignedInt;
}

// 0x080b2faa  size=81  Dvar_GetBool  status=translated  naming=inferred
// Typed accessor: bool. Missing dvar yields 0.
uint8 Dvar_GetBool(const char *name)
{
    struct dvar_s *d;

    d = Dvar_Lookup(name);
    if (d == 0) {
        return 0;
    }
    if (d->type == DVAR_TYPE_BOOL) {
        return (uint8)d->current.unsignedInt;
    }
    return Dvar_ParseBool(DVAR_AS_STR(d->current.unsignedInt));
}

// 0x080b2ffc  size=88  Dvar_GetInt  status=translated  naming=inferred
// Typed accessor: int. Missing dvar yields 0.
int Dvar_GetInt(const char *name)
{
    struct dvar_s *d;

    d = Dvar_Lookup(name);
    if (d == 0) {
        return 0;
    }
    if (d->type == DVAR_TYPE_INT || d->type == DVAR_TYPE_ENUM) {
        return (int)d->current.unsignedInt;
    }
    return Dvar_ParseInt(DVAR_AS_STR(d->current.unsignedInt));
}

// 0x080b3054  size=75  Dvar_GetFloat  status=translated  naming=inferred
// Typed accessor: float. Missing dvar yields 0.0f.
float Dvar_GetFloat(const char *name)
{
    struct dvar_s *d;

    d = Dvar_Lookup(name);
    if (d == 0) {
        return 0.0f;
    }
    if (d->type == DVAR_TYPE_FLOAT) {
        return dvar_word_to_float(d->current.unsignedInt);
    }
    return Dvar_ParseFloat(DVAR_AS_STR(d->current.unsignedInt));
}

// 0x080b318a  size=74  Dvar_GetString  status=translated  naming=inferred
// Typed accessor: string. Missing dvar yields "". Enum returns its string.
const char *Dvar_GetString(const char *name)
{
    struct dvar_s *d;

    d = Dvar_Lookup(name);
    if (d == 0) {
        return "";
    }
    if (d->type == DVAR_TYPE_ENUM) {
        return Dvar_CurrentEnumString(d);
    }
    return (const char *)DVAR_AS_PTR(d->current.unsignedInt);
}

// 0x080b31d4  size=96  Dvar_GetDisplayString  status=translated  naming=inferred
// Display string: the value rendered through Dvar_FormatValue under the lock.
const char *Dvar_GetDisplayString(const char *name)
{
    struct dvar_s *d;
    const char *text;

    d = Dvar_Lookup(name);
    if (d == 0) {
        return "";
    }
    Sys_EnterCriticalSection(4);
    text = Dvar_FormatValue(d, d->current.unsignedInt);
    Sys_LeaveCriticalSection(4);
    return text;
}

// 0x080b3234  size=170  Dvar_GetColorFloat  status=translated  naming=inferred
// Colour dvar to four floats in [0,1]: bytes times 0.003921569f (1/255).
void Dvar_GetColorFloat(struct dvar_s *d, float *out)
{
    uint8 rgba[4];

    if (d->type == DVAR_TYPE_COLOR) {
        Word_Copy(&d->current.unsignedInt, (uint32 *)rgba);
    } else {
        Dvar_ParseColorRGBA(DVAR_AS_STR(d->current.unsignedInt), rgba);
    }
    out[0] = (float)rgba[0] * 0.003921569f;
    out[1] = (float)rgba[1] * 0.003921569f;
    out[2] = (float)rgba[2] * 0.003921569f;
    out[3] = (float)rgba[3] * 0.003921569f;
}

// 0x080b32de  size=67  Dvar_GetColorOrWhite  status=translated  naming=inferred
// Colour accessor; a missing dvar yields opaque white (vec4 1,1,1,1).
void Dvar_GetColorOrWhite(const char *name, float *out)
{
    struct dvar_s *d;

    d = Dvar_Lookup(name);
    if (d == 0) {
        Vec4_Copy_080b5932(kDvarWhite, out);
    } else {
        Dvar_GetColorFloat(d, out);
    }
}

// 0x080b3322  size=226  Dvar_Shutdown  status=translated  naming=inferred
// Shutdown: release value storage and owned names of every dvar in the
// name-sorted list, then clear all dvar globals and the hash buckets.
void Dvar_Shutdown(void)
{
    struct dvar_s *d;

    for (d = dvar_vars; d != 0; d = d->next) {
        if (d->type == DVAR_TYPE_STRING) {
            Dvar_ReleaseCurrentValue(d);
            Dvar_ReleaseResetValue(d);
            Dvar_ReleaseLatchedValue(d);
        } else if (d->type == DVAR_TYPE_VEC2 || d->type == DVAR_TYPE_VEC3 ||
                   d->type == DVAR_TYPE_VEC4) {
            Dvar_FreeValueBlock(d);
        }
        if ((d->flags & DVAR_FLAG_OWNS_NAME) != 0) {
            Dvar_ReleasePoolString(d->name);
        }
    }
    dvar_indexCount = 0;
    dvar_vars = 0;
    g_dvar_sv_cheats = 0;
    dvar_modifiedFlags = 0;
    dvar_initialised = 0;
    memset(dvar_hashTable, 0, 0x400);
}

// 0x080b3404  size=214  Dvar_ForceStringType  status=translated  naming=inferred
// Makes the dvar owned-name and string-typed: takes ownership of the name,
// converts the current value to a heap string, and releases a previous
// vector buffer.
void Dvar_ForceStringType(struct dvar_s *d)
{
    uint32 old_value;
    const char *reset_word;

    if ((d->flags & DVAR_FLAG_OWNS_NAME) == 0) {
        d->flags |= DVAR_FLAG_OWNS_NAME;
        d->name = Dvar_InternString(d->name);
    }
    if (d->type != DVAR_TYPE_STRING) {
        if (d->type == DVAR_TYPE_VEC2 || d->type == DVAR_TYPE_VEC3 || d->type == DVAR_TYPE_VEC4) {
            old_value = d->current.unsignedInt;
        } else {
            old_value = 0;
        }
        d->current.unsignedInt = DVAR_FROM_PTR(Dvar_CanonicalString(Dvar_FormatLatchedValue(d)));
        d->latched.unsignedInt = d->current.unsignedInt;
        reset_word = Dvar_FormatResetValue(d);
        Dvar_SetResetString(d, reset_word);
        d->type = DVAR_TYPE_STRING;
        if (old_value != 0) {
            Com_Free_A(DVAR_AS_PTR(old_value));
        }
    }
}

// 0x080b34da  size=82  Dvar_ClearFlagsIfNotOwned  status=translated  naming=inferred
// Clears flag bits on a dvar that does not own its name; converts it to a
// string when the 0x7000 change bits are then clear.
void Dvar_ClearFlagsIfNotOwned(struct dvar_s *d, uint16 mask)
{
    if ((d->flags & DVAR_FLAG_OWNS_NAME) == 0) {
        d->flags = (uint16)(~mask & d->flags);
        if ((d->flags & DVAR_FLAG_CHANGE_MASK) == 0) {
            Dvar_ForceStringType(d);
        }
    }
}

// 0x080b352c  size=67  Dvar_ClearFlagsGlobal  status=translated  naming=inferred
// Applies Dvar_ClearFlagsIfNotOwned to every dvar whose flags intersect mask.
void Dvar_ClearFlagsGlobal(uint32 mask)
{
    struct dvar_s *d;

    for (d = dvar_vars; d != 0; d = d->next) {
        if ((d->flags & mask) != 0) {
            Dvar_ClearFlagsIfNotOwned(d, (uint16)mask);
        }
    }
}

// 0x080b3570  size=163  Dvar_SetReset  status=translated  naming=inferred
// Stores the reset (default) value with the per-type copy rules.
void Dvar_SetReset(struct dvar_s *d, uint32 value)
{
    switch (d->type) {
    case 3:
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->reset.unsignedInt));
        return;
    case 2:
        Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->reset.unsignedInt));
        return;
    case 4:
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->reset.unsignedInt));
        return;
    case 7:
        Dvar_ReleaseResetValue(d);
        Dvar_SetResetString(d, DVAR_AS_STR(value));
        return;
    default:
        d->reset.unsignedInt = value;
        return;
    }
}

// 0x080b362e  size=274  Dvar_SetCurrentAndLatched  status=translated  naming=inferred
// Stores the current and latched values together with the per-type copy rules.
void Dvar_SetCurrentAndLatched(struct dvar_s *d, uint32 value)
{
    switch (d->type) {
    case 3:
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        return;
    case 2:
        Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        return;
    case 4:
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        return;
    case 7:
        if (value != d->current.unsignedInt) {
            Dvar_ReleaseCurrentValue(d);
            Dvar_SetCurrentString(d, DVAR_AS_STR(value));
        }
        d->latched.unsignedInt = value;
        return;
    default:
        d->current.unsignedInt = value;
        d->latched.unsignedInt = value;
        return;
    }
}

// 0x080b3740  size=356  Dvar_Redefine  status=translated  naming=inferred
// Re-types a dvar: sets type and domain, re-resolves the current value
// against the new domain (unless the cheat/read-only policy skips it), then
// stores reset and current/latched values and ORs the flags into the global.
void Dvar_Redefine(struct dvar_s *d, const char *name, uint8 type, uint16 flags,
                   uint32 value_in, uint32 min, uint32 max)
{
    uint32 result;
    (void)name;
    d->type = type;
    d->domain.enumeration.stringCount = min;
    d->domain.integer.max = max;

    if ((flags & DVAR_FLAG_READ_ONLY) == 0 &&
        ((flags & DVAR_FLAG_CHEAT_PROTECT) == 0 || g_dvar_sv_cheats == 0 ||
         (int8)g_dvar_sv_cheats->current.unsignedInt != 0)) {
        result = Dvar_ParseValueWord(d->type, d->domain, DVAR_AS_STR(d->current.unsignedInt)).unsignedInt;
        result = Dvar_ClampValue(d->type, result, value_in, d->domain).unsignedInt;
    } else {
        result = value_in;
    }

    if (d->type != DVAR_TYPE_STRING) {
        Dvar_ReleaseCurrentValue(d);
    }
    Dvar_ReleaseLatchedValue(d);
    Dvar_ReleaseResetValue(d);
    if (d->type == DVAR_TYPE_VEC2 || d->type == DVAR_TYPE_VEC3 || d->type == DVAR_TYPE_VEC4) {
        Dvar_AllocValueBlock(d);
    }
    Dvar_SetReset(d, value_in);
    Dvar_SetCurrentAndLatched(d, result);
    dvar_modifiedFlags |= flags;
}

// 0x080b38a4  size=52  Dvar_InitOut  status=translated  naming=unknown
// Stores a word into *out and returns out. The other two arguments are unused.
// Hidden aggregate-return pointer first (callee pops it). Only the value is copied to the
// result; the dvar, type, flags and domain arguments are accepted and ignored.
DvarValue Dvar_InitOut(struct dvar_s *d, DvarValue value, uint8 type, uint16 flags, DvarLimits domain)
{
    DvarValue out;

    out = value;
    return out;
}

// 0x080b38d8  size=244  Dvar_ReplaceOwnedName  status=translated  naming=inferred
// Replaces an owned name: stores the current value, converts to string, frees
// the old name, installs the new name, clears the owned bit and re-defines.
void Dvar_ReplaceOwnedName(struct dvar_s *d, const char *newname, uint8 type, uint16 flags,
                           uint32 value_in, uint32 min, uint32 max)
{
    DvarValue in;
    DvarLimits domain;
    uint32 value;

    in.unsignedInt = value_in;
    domain.integer.min = (int)min;
    domain.integer.max = (int)max;
    if ((d->flags & DVAR_FLAG_OWNS_NAME) != 0 && (flags & DVAR_FLAG_OWNS_NAME) == 0) {
        value = Dvar_InitOut(d, in, type, flags, domain).unsignedInt;
        Dvar_ForceStringType(d);
        Dvar_ReleasePoolString(d->name);
        d->name = newname;
        d->flags = (uint16)(d->flags & 0xbfff);
        Dvar_Redefine(d, newname, type, flags, value, min, max);
    }
}

// 0x080b39cc  size=116  Dvar_UpdateEnumNameAndDomain  status=translated  naming=inferred
// Installs a new name and, for enum dvars, the new domain, when the new flags
// gain the 0x1000 bit that the existing dvar lacks.
void Dvar_UpdateEnumNameAndDomain(struct dvar_s *d, const char *name, uint8 type, uint16 flags,
                                  uint32 value_in, uint32 min, uint32 max)
{
    (void)type;
    (void)value_in;
    if ((flags & DVAR_FLAG_OWNS_NAME) == 0 && (flags & DVAR_FLAG_UNKNOWN_1000) != 0 &&
        (d->flags & DVAR_FLAG_UNKNOWN_1000) == 0 && (d->name = name, d->type == DVAR_TYPE_ENUM)) {
        d->domain.enumeration.stringCount = min;
        d->domain.integer.max = max;
    }
}

// 0x080b3a40  size=63  Dvar_ApplyLatched  status=translated  naming=inferred
// Applies the latched value as a set from the internal source.
void Dvar_ApplyLatched(struct dvar_s *d)
{
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, d->latched.unsignedInt, DVAR_SOURCE_INTERNAL);
    Sys_LeaveCriticalSection(4);
}

// 0x080b3ab0  size=407  Dvar_Update  status=translated  naming=inferred
// Updates an existing dvar's definition and flags: domain/name refresh when
// the change bits differ, re-type for owned names, cheat-protected reset, and
// latch application.
void Dvar_Update(struct dvar_s *d, const char *name, uint8 type, uint16 flags,
                 uint32 value, uint32 min, uint32 max)
{
    Sys_EnterCriticalSection(4);
    if (((flags ^ d->flags) & DVAR_FLAG_CHANGE_MASK) != 0) {
        Dvar_ReplaceOwnedName(d, name, type, flags, value, min, max);
        Dvar_UpdateEnumNameAndDomain(d, name, type, flags, value, min, max);
    }
    if ((d->flags & DVAR_FLAG_OWNS_NAME) != 0 && d->type != type) {
        Dvar_Redefine(d, name, type, flags, value, min, max);
    }
    d->flags = (uint16)(d->flags | flags);
    if ((d->flags & DVAR_FLAG_CHEAT_PROTECT) != 0 && g_dvar_sv_cheats != 0 &&
        (int8)g_dvar_sv_cheats->current.unsignedInt == 0) {
        Dvar_SetVariant(d, d->reset.unsignedInt, DVAR_SOURCE_INTERNAL);
        Dvar_SetLatched(d, d->reset.unsignedInt);
    }
    if ((d->flags & DVAR_FLAG_LATCH) != 0) {
        Dvar_ApplyLatched(d);
    }
    Sys_LeaveCriticalSection(4);
}

// 0x080b3c48  size=730  Dvar_Create  status=translated  naming=observed
// Evidence: "Can't create dvar '%s': %i dvars already exist" (limit 0x500).
// Allocates the next pool entry, copies the name when the flags request
// ownership, stores the per-type value words, inserts into the name-sorted
// list and the hash bucket.
struct dvar_s *Dvar_Create(const char *name, uint8 type, uint16 flags,
                           uint32 value, uint32 min, uint32 max)
{
    struct dvar_s *d;
    struct dvar_s **link;
    int32 index;
    int hash;

    Sys_EnterCriticalSection(4);
    if (dvar_indexCount > 0x4ff) {
        Sys_LeaveCriticalSection(4);
        Com_Error(0, "Can't create dvar '%s': %i dvars already exist", name, 0x500);
    }
    index = dvar_indexCount;
    d = &dvar_pool[index];
    dvar_indexCount = index + 1;

    d->type = type;
    if ((flags & DVAR_FLAG_OWNS_NAME) == 0) {
        d->name = name;
    } else {
        d->name = Dvar_InternString(name);
    }

    if (type == DVAR_TYPE_VEC3) {
        Dvar_AllocValueBlock(d);
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        Vec3_Copy_080b5834((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->reset.unsignedInt));
    } else if (type < 4) {
        if (type == DVAR_TYPE_VEC2) {
            Dvar_AllocValueBlock(d);
            Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
            Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
            Vec2_Copy_080b5778((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->reset.unsignedInt));
        } else {
            d->current.unsignedInt = value;
            d->latched.unsignedInt = value;
            d->reset.unsignedInt = value;
        }
    } else if (type == DVAR_TYPE_VEC4) {
        Dvar_AllocValueBlock(d);
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->current.unsignedInt));
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->latched.unsignedInt));
        Vec4_Copy_080b5932((const float *)DVAR_AS_PTR(value), (float *)DVAR_AS_PTR(d->reset.unsignedInt));
    } else if (type == DVAR_TYPE_STRING) {
        d->current.unsignedInt = DVAR_FROM_PTR(Dvar_CanonicalString((const char *)DVAR_AS_PTR(value)));
        d->latched.unsignedInt = d->current.unsignedInt;
        d->reset.unsignedInt = d->current.unsignedInt;
    } else {
        d->current.unsignedInt = value;
        d->latched.unsignedInt = value;
        d->reset.unsignedInt = value;
    }

    d->domain.enumeration.stringCount = min;
    d->domain.integer.max = max;
    d->modified = 0;

    link = &dvar_vars;
    while (*link != 0 && strcasecmp(d->name, (*link)->name) >= 0) {
        link = &(*link)->next;
    }
    d->next = *link;
    *link = d;
    d->flags = flags;

    hash = generateHashValue(name);
    d->hashNext = dvar_hashTable[hash];
    dvar_hashTable[hash] = d;
    Sys_LeaveCriticalSection(4);
    return d;
}

// 0x080b3f22  size=175  Dvar_Register  status=translated  naming=inferred
// Register-or-update: find by name, then create or update in place.
struct dvar_s *Dvar_Register(const char *name, uint8 type, uint16 flags,
                             uint32 value, uint32 min, uint32 max)
{
    struct dvar_s *d;

    d = Dvar_FindHashed(name);
    if (d == 0) {
        d = Dvar_Create(name, type, flags, value, min, max);
    } else {
        Dvar_Update(d, name, type, flags, value, min, max);
    }
    return d;
}

// 0x080b3fd2  size=103  Dvar_RegisterBool  status=translated  naming=inferred
// Bool registration with zero domain. Returns the dvar (eax passes through).
// The upper three bytes of the value word are indeterminate in the binary;
// only the low byte is observable, so the value is zero-extended here.
struct dvar_s *Dvar_RegisterBool(const char *name, int8 value, uint16 flags)
{
    return Dvar_Register(name, DVAR_TYPE_BOOL, flags, (uint32)(uint8)value, 0, 0);
}

// 0x080b403a  size=81  Dvar_RegisterInt  status=translated  naming=inferred
// Int registration with min/max domain.
struct dvar_s *Dvar_RegisterInt(const char *name, uint32 value, uint32 min, uint32 max, uint16 flags)
{
    return Dvar_Register(name, DVAR_TYPE_INT, flags, value, min, max);
}

// 0x080b408c  size=81  Dvar_RegisterFloat  status=translated  naming=inferred
// Float registration; value and domain are float stack words (callers push the float constants as immediates).
struct dvar_s *Dvar_RegisterFloat(const char *name, float value, float min, float max, uint16 flags)
{
    union { float f; uint32 u; } v, lo, hi;
    v.f = value;
    lo.f = min;
    hi.f = max;
    return Dvar_Register(name, DVAR_TYPE_FLOAT, flags, v.u, lo.u, hi.u);
}

// 0x080b4232  size=96  Dvar_RegisterString  status=translated  naming=inferred
// String registration (zero domain).
struct dvar_s *Dvar_RegisterString(const char *name, const char *value, uint16 flags)
{
    return Dvar_Register(name, DVAR_TYPE_STRING, flags, DVAR_FROM_PTR(value), 0, 0);
}

// 0x080b4292  size=110  Dvar_RegisterEnum  status=translated  naming=inferred
// Enum registration: domain is (count of list, list); default index as value.
struct dvar_s *Dvar_RegisterEnum(const char *name, const char *const *list, uint32 defval, uint16 flags)
{
    int count;

    count = 0;
    while (list[count] != 0) {
        count++;
    }
    return Dvar_Register(name, DVAR_TYPE_ENUM, flags, defval, (uint32)count, DVAR_FROM_PTR(list));
}

// 0x080b4300  size=314  Dvar_RegisterColor  status=translated  naming=inferred
// Colour registration from four float components, packed to one word.
struct dvar_s *Dvar_RegisterColor(const char *name, float r, float g, float b, float a, uint16 flags)
{
    return Dvar_Register(name, DVAR_TYPE_COLOR, flags, dvar_pack_color(r, g, b, a), 0, 0);
}

// 0x080b443a  size=109  Dvar_SetBoolEx  status=translated  naming=observed
// Evidence: rodata 0x0814ee41 "1" and 0x0814ee43 "0". Bool dvars take the raw byte (only the low
// byte of the value word is written; the upper bytes are uninitialised stack in the binary and
// are zero here). Other dvar types are given a pointer to "1" or "0" and parse it.
void Dvar_SetBoolEx(struct dvar_s *d, int8 value, int source)
{
    uint32 text;

    if (d->type == DVAR_TYPE_BOOL) {
        text = (uint8)value;
    } else if (value != 0) {
        text = DVAR_FROM_PTR("1");
    } else {
        text = DVAR_FROM_PTR("0");
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b44a8  size=125  Dvar_SetIntEx  status=translated  naming=inferred
// Int set. For int and enum dvars the raw value is used; otherwise the value
// is formatted with "%i" and the text is parsed by Dvar_SetVariant.
void Dvar_SetIntEx(struct dvar_s *d, uint32 value, int source)
{
    char buf[44];
    uint32 text;

    if (d->type == DVAR_TYPE_INT || d->type == DVAR_TYPE_ENUM) {
        text = value;
    } else {
        Com_sprintf(buf, 0x20, "%i", (int)value);
        text = DVAR_FROM_PTR(buf);
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b4526  size=114  Dvar_SetFloatEx  status=translated  naming=inferred
// Float set. Float dvars take the float bits directly; others format "%g".
void Dvar_SetFloatEx(struct dvar_s *d, float value, int source)
{
    char buf[44];
    uint32 text;

    if (d->type == DVAR_TYPE_FLOAT) {
        text = dvar_float_to_word(value);
    } else {
        Com_sprintf(buf, 0x20, "%g", (double)value);
        text = DVAR_FROM_PTR(buf);
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b4598  size=146  Dvar_SetVec2Ex  status=translated  naming=inferred
// Two-float set. The vector path is taken when the dvar type is 4 (vec4), as
// in the binary, which then builds only two floats; otherwise "%g %g".
void Dvar_SetVec2Ex(struct dvar_s *d, float x, float y, int source)
{
    char buf[64];
    float vec[2];
    uint32 text;

    if (d->type == DVAR_TYPE_VEC4) {
        Vec2_Set_080b5760(vec, x, y);
        text = DVAR_FROM_PTR(vec);
    } else {
        Com_sprintf(buf, 0x40, "%g %g", (double)x, (double)y);
        text = DVAR_FROM_PTR(buf);
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b462a  size=169  Dvar_SetVec3Ex  status=translated  naming=inferred
// Three-float set: vec3 path for type 3, otherwise "%g %g %g".
void Dvar_SetVec3Ex(struct dvar_s *d, float x, float y, float z, int source)
{
    char buf[96];
    float vec[3];
    uint32 text;

    if (d->type == DVAR_TYPE_VEC3) {
        Vec3_Set_080b5810(vec, x, y, z);
        text = DVAR_FROM_PTR(vec);
    } else {
        Com_sprintf(buf, 0x60, "%g %g %g", (double)x, (double)y, (double)z);
        text = DVAR_FROM_PTR(buf);
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b46d4  size=183  Dvar_SetVec4Ex  status=translated  naming=inferred
// Four-float set: vec4 path for type 4, otherwise "%g %g %g %g".
void Dvar_SetVec4Ex(struct dvar_s *d, float x, float y, float z, float w, int source)
{
    char buf[128];
    float vec[4];
    uint32 text;

    if (d->type == DVAR_TYPE_VEC4) {
        Vec4_Set(vec, x, y, z, w);
        text = DVAR_FROM_PTR(vec);
    } else {
        Com_sprintf(buf, 0x80, "%g %g %g %g", (double)x, (double)y, (double)z, (double)w);
        text = DVAR_FROM_PTR(buf);
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b478c  size=133  Dvar_SetStringEx  status=translated  naming=inferred
// String set: string dvars copy the text (bounded to 0x400); others parse
// the text through Dvar_ParseEnumIndex against the dvar's domain words.
void Dvar_SetStringEx(struct dvar_s *d, const char *s, int source)
{
    char buf[1036];
    uint32 text;

    if (d->type == DVAR_TYPE_STRING) {
        Q_strncpyz(buf, s, 0x400);
        text = DVAR_FROM_PTR(buf);
    } else {
        text = DVAR_FROM_PTR(Dvar_ParseEnumIndex(&d->domain.enumeration, s));
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b4812  size=366  Dvar_SetVec4OrColorEx  status=translated  naming=inferred
// Four-float set: colour dvars pack the components; others format "%g %g %g %g".
void Dvar_SetVec4OrColorEx(struct dvar_s *d, float r, float g, float b, float a, int source)
{
    char buf[140];
    uint32 text;

    if (d->type == DVAR_TYPE_COLOR) {
        text = dvar_pack_color(r, g, b, a);
    } else {
        Com_sprintf(buf, 0x80, "%g %g %g %g", (double)r, (double)g, (double)b, (double)a);
        text = DVAR_FROM_PTR(buf);
    }
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, text, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b4980  size=41  Dvar_SetBool  status=translated  naming=inferred
// Bool set from the internal source.
void Dvar_SetBool(struct dvar_s *d, int8 value)
{
    Dvar_SetBoolEx(d, value, DVAR_SOURCE_INTERNAL);
}

// 0x080b49aa  size=34  Dvar_SetInt  status=translated  naming=inferred
// Int set from the internal source.
void Dvar_SetInt(struct dvar_s *d, uint32 value)
{
    Dvar_SetIntEx(d, value, DVAR_SOURCE_INTERNAL);
}

// 0x080b49cc  size=34  Dvar_SetFloat  status=translated  naming=inferred
// Float set from the internal source.
void Dvar_SetFloat(struct dvar_s *d, float value)
{
    Dvar_SetFloatEx(d, value, DVAR_SOURCE_INTERNAL);
}

// 0x080b49ee  size=41  Dvar_SetVec2  status=translated  naming=inferred
// Two-float set from the internal source.
void Dvar_SetVec2(struct dvar_s *d, float x, float y)
{
    Dvar_SetVec2Ex(d, x, y, DVAR_SOURCE_INTERNAL);
}

// 0x080b4a18  size=48  Dvar_SetVec3  status=translated  naming=inferred
// Three-float set from the internal source.
void Dvar_SetVec3(struct dvar_s *d, float x, float y, float z)
{
    Dvar_SetVec3Ex(d, x, y, z, DVAR_SOURCE_INTERNAL);
}

// 0x080b4a48  size=55  Dvar_SetVec4  status=translated  naming=inferred
// Four-float set from the internal source.
void Dvar_SetVec4(struct dvar_s *d, float x, float y, float z, float w)
{
    Dvar_SetVec4Ex(d, x, y, z, w, DVAR_SOURCE_INTERNAL);
}

// 0x080b4a80  size=34  Dvar_SetString  status=translated  naming=inferred
// String set from the internal source.
void Dvar_SetString(struct dvar_s *d, const char *s)
{
    Dvar_SetStringEx(d, s, DVAR_SOURCE_INTERNAL);
}

// 0x080b4aa2  size=55  Dvar_SetVec4OrColor  status=translated  naming=inferred
// Four-component colour/vector set from the internal source.
void Dvar_SetVec4OrColor(struct dvar_s *d, float r, float g, float b, float a)
{
    Dvar_SetVec4OrColorEx(d, r, g, b, a, DVAR_SOURCE_INTERNAL);
}

// 0x080b4ada  size=249  Dvar_SetFromStringEx  status=translated  naming=observed
// Evidence: "'%s' is not a valid value for dvar '%s'" in this function.
// Parses text against the dvar domain; an enum parse failure (-0x539) reports
// the error and falls back to the reset value.
void Dvar_SetFromStringEx(struct dvar_s *d, const char *text, int source)
{
    char buf[1036];
    int parsed;
    uint32 value;

    Sys_EnterCriticalSection(4);
    Q_strncpyz(buf, text, 0x400);
    parsed = Dvar_ParseValueWord(d->type, d->domain, buf).integer;
    value = (uint32)parsed;
    if (d->type == DVAR_TYPE_ENUM && parsed == -0x539) {
        Com_Printf("'%s' is not a valid value for dvar '%s'\n", buf, d->name);
        Dvar_PrintDomain(d->type, d->domain.enumeration.stringCount, d->domain.integer.max);
        value = d->reset.unsignedInt;
    }
    Dvar_SetVariant(d, value, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b4bd4  size=34  Dvar_SetFromString  status=translated  naming=inferred
// Text set from the internal source.
void Dvar_SetFromString(struct dvar_s *d, const char *text)
{
    Dvar_SetFromStringEx(d, text, DVAR_SOURCE_INTERNAL);
}

// 0x080b4bf6  size=103  Dvar_SetOrCreateBool  status=translated  naming=inferred
// Bool set by name: creates a string dvar "0"/"1" (owned name) when absent.
void Dvar_SetOrCreateBool(const char *name, int8 value)
{
    struct dvar_s *d;
    const char *text;

    d = Dvar_Lookup(name);
    if (d == 0) {
        if (value == 0) {
            text = "0";
        } else {
            text = "1";
        }
        Dvar_RegisterString(name, text, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetBool(d, value);
    }
}

// 0x080b4c5e  size=108  Dvar_SetOrCreateInt  status=translated  naming=inferred
// Int set by name: creates a string dvar with "%i" text when absent.
void Dvar_SetOrCreateInt(const char *name, uint32 value)
{
    struct dvar_s *d;
    char buf[44];

    d = Dvar_Lookup(name);
    if (d == 0) {
        Com_sprintf(buf, 0x20, "%i", (int)value);
        Dvar_RegisterString(name, buf, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetInt(d, value);
    }
}

// 0x080b4cca  size=90  Dvar_SetOrCreateFloat  status=translated  naming=inferred
// Float set by name: creates a string dvar with "%g" text when absent.
void Dvar_SetOrCreateFloat(const char *name, float value)
{
    struct dvar_s *d;
    const char *text;

    d = Dvar_Lookup(name);
    if (d == 0) {
        text = va("%g", (double)value);
        Dvar_RegisterString(name, text, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetFloat(d, value);
    }
}

// 0x080b4d24  size=104  Dvar_SetOrCreateVec2  status=translated  naming=inferred
// Two-float set by name: creates a string dvar "%g %g" when absent.
void Dvar_SetOrCreateVec2(const char *name, float x, float y)
{
    struct dvar_s *d;
    const char *text;

    d = Dvar_Lookup(name);
    if (d == 0) {
        text = va("%g %g", (double)x, (double)y);
        Dvar_RegisterString(name, text, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetVec2(d, x, y);
    }
}

// 0x080b4d8c  size=118  Dvar_SetOrCreateVec3  status=translated  naming=inferred
// Three-float set by name: creates a string dvar "%g %g %g" when absent.
void Dvar_SetOrCreateVec3(const char *name, float x, float y, float z)
{
    struct dvar_s *d;
    const char *text;

    d = Dvar_Lookup(name);
    if (d == 0) {
        text = va("%g %g %g", (double)x, (double)y, (double)z);
        Dvar_RegisterString(name, text, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetVec3(d, x, y, z);
    }
}

// 0x080b4e86  size=74  Dvar_SetOrCreateString  status=translated  naming=inferred
// String set by name: creates a string dvar with the text when absent.
void Dvar_SetOrCreateString(const char *name, const char *s)
{
    struct dvar_s *d;

    d = Dvar_Lookup(name);
    if (d == 0) {
        Dvar_RegisterString(name, s, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetString(d, s);
    }
}

// 0x080b4ed0  size=209  Dvar_SetOrCreateColor  status=translated  naming=inferred
// Colour set by name. When absent, the text is formatted with "%g %g %g %g"
// from the four byte components passed as ints (as the binary pushes them;
// this is a varargs/format mismatch present in the original). When present,
// the bytes are converted to float and set as a vec4-or-colour value.
void Dvar_SetOrCreateColor(const char *name, uint8 r, uint8 g, uint8 b, uint8 a)
{
    struct dvar_s *d;
    const char *text;

    d = Dvar_Lookup(name);
    if (d == 0) {
        text = va("%g %g %g %g", (int)r, (int)g, (int)b, (int)a);
        Dvar_RegisterString(name, text, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetVec4OrColor(d, (float)r, (float)g, (float)b, (float)a);
    }
}

// 0x080b4fa2  size=87  Dvar_SetStringOrCreateEx  status=translated  naming=inferred
// Text set by name with a source: creates a string dvar when absent, else
// applies the text. Returns the dvar.
struct dvar_s *Dvar_SetStringOrCreateEx(const char *name, const char *text, int source)
{
    struct dvar_s *d;

    d = Dvar_Lookup(name);
    if (d == 0) {
        d = Dvar_RegisterString(name, text, DVAR_FLAG_OWNS_NAME);
    } else {
        Dvar_SetFromStringEx(d, text, source);
    }
    return d;
}

// 0x080b4ffa  size=34  Dvar_SetStringOrCreate  status=translated  naming=inferred
// Text set by name from the internal source. Returns the dvar.
struct dvar_s *Dvar_SetStringOrCreate(const char *name, const char *text)
{
    return Dvar_SetStringOrCreateEx(name, text, DVAR_SOURCE_INTERNAL);
}

// 0x080b501c  size=94  Dvar_SetFromUserInput  status=translated  naming=inferred
// User text set: applies with source 1; when the global gate dvar_loadingConfig is
// set, marks the dvar 0x8000 and copies its value into the reset word.
void Dvar_SetFromUserInput(const char *name, const char *text)
{
    struct dvar_s *d;

    d = Dvar_SetStringOrCreateEx(name, text, DVAR_SOURCE_USER);
    if (d != 0 && dvar_loadingConfig != 0) {
        Dvar_OrFlags(d, DVAR_FLAG_UNKNOWN_8000);
        Dvar_SetReset(d, d->current.unsignedInt);
    }
}

// 0x080b507a  size=24  Dvar_OrFlags  status=translated  naming=inferred
// ORs flags into dvar+0x04.
void Dvar_OrFlags(struct dvar_s *d, uint16 flags)
{
    d->flags = (uint16)(flags | d->flags);
}

// 0x080b5092  size=62  Dvar_ResetToDefault  status=translated  naming=inferred
// Sets the dvar to its reset value from the given source.
void Dvar_ResetToDefault(struct dvar_s *d, int source)
{
    Sys_EnterCriticalSection(4);
    Dvar_SetVariant(d, d->reset.unsignedInt, source);
    Sys_LeaveCriticalSection(4);
}

// 0x080b50d0  size=104  Dvar_ResetAllCheatProtected  status=translated  naming=inferred
// Resets every dvar with the cheat-protect flag to its reset value, walking
// the name-sorted list from the head.
void Dvar_ResetAllCheatProtected(void)
{
    struct dvar_s *d;

    Sys_EnterCriticalSection(4);
    for (d = dvar_vars; d != 0; d = d->next) {
        if ((d->flags & DVAR_FLAG_CHEAT_PROTECT) != 0) {
            Dvar_SetVariant(d, d->reset.unsignedInt, DVAR_SOURCE_INTERNAL);
        }
    }
    Sys_LeaveCriticalSection(4);
}

// 0x080b5138  size=53  Dvar_InitSvCheats  status=translated  naming=observed
// Evidence: registers the "sv_cheats" dvar (flags 0x1018, bool 0), keeps the
// pointer in g_dvar_sv_cheats and sets dvar_initialised.
void Dvar_InitSvCheats(void)
{
    dvar_initialised = 1;
    g_dvar_sv_cheats = Dvar_RegisterBool("sv_cheats", 0, 0x1018);
    Dvar_AddCommands();
}

// 0x080b516e  size=77  Dvar_ClearUnknownFlag0400  status=translated  naming=unknown
// Clears flag 0x0400 on every pool entry. The walk starts at pool entry 0 and
// follows sorted_next from there, not from the list head (kept as in binary).
void Dvar_ClearUnknownFlag0400(void)
{
    struct dvar_s *d;

    Sys_EnterCriticalSection(4);
    for (d = dvar_pool; d != 0; d = d->next) {
        d->flags = (uint16)(d->flags & 0xfbff);
    }
    Sys_LeaveCriticalSection(4);
}

// 0x080b51bc  size=69  Dvar_AnyLatchPending  status=translated  naming=inferred
// Returns 1 when any dvar in the sorted list has a pending latch.
uint32 Dvar_AnyLatchPending(void)
{
    struct dvar_s *d;

    for (d = dvar_vars; d != 0; d = d->next) {
        if (Dvar_HasPendingLatch(d) != 0) {
            return 1;
        }
    }
    return 0;
}

// 0x080b56c6  size=36  Float_Max_080b56c6  status=translated  naming=inferred
// max(a, b) via Float_SelectNonNegative_080b5ab8(a - b, a, b).
float Float_Max_080b56c6(float a, float b)
{
    return Float_SelectNonNegative_080b5ab8(a - b, a, b);
}

// 0x080b56ea  size=36  Float_Min_080b56ea  status=translated  naming=inferred
// min(a, b) via Float_SelectNonNegative_080b5ab8(b - a, a, b).
float Float_Min_080b56ea(float a, float b)
{
    return Float_SelectNonNegative_080b5ab8(b - a, a, b);
}

// 0x080b570e  size=54  Float_RoundToInt_080b570e  status=translated  naming=inferred
// Round half up: floor(v + 0.5) converted to int.
int Float_RoundToInt_080b570e(float v)
{
    return (int)floor((double)v + 0.5);
}

// 0x080b5744  size=28  Vec2_Clear_080b5744  status=translated  naming=unknown
// Zeroes a two-float vector.
void Vec2_Clear_080b5744(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
}

// 0x080b5760  size=24  Vec2_Set_080b5760  status=translated  naming=unknown
// Stores two floats into a vector.
void Vec2_Set_080b5760(float *v, float x, float y)
{
    /* Raw word moves: the binary copies the bits, an x87 float round trip would quiet NaNs. */
    memcpy(&v[0], &x, sizeof x);
    memcpy(&v[1], &y, sizeof y);
}

// 0x080b5778  size=31  Vec2_Copy_080b5778  status=translated  naming=unknown
// Copies a two-float vector from src to dst.
void Vec2_Copy_080b5778(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
}

// 0x080b5798  size=77  Vec2_Equal  status=translated  naming=unknown
// Returns 1 when two two-float vectors compare equal.
uint32 Vec2_Equal(const float *a, const float *b)
{
    uint32 same;

    same = 0;
    if (a[0] == b[0] && a[1] == b[1]) {
        same = 1;
    }
    return same;
}

// 0x080b57e6  size=41  Vec3_Clear_080b57e6  status=translated  naming=unknown
// Zeroes a three-float vector.
void Vec3_Clear_080b57e6(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x080b5810  size=35  Vec3_Set_080b5810  status=translated  naming=unknown
// Stores three floats into a vector.
void Vec3_Set_080b5810(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

// 0x080b5834  size=47  Vec3_Copy_080b5834  status=translated  naming=unknown
// Copies a three-float vector from src to dst.
void Vec3_Copy_080b5834(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// 0x080b5864  size=106  Vec3_Equal_080b5864  status=translated  naming=unknown
// Returns 1 when two three-float vectors compare equal.
uint32 Vec3_Equal_080b5864(const float *a, const float *b)
{
    uint32 same;

    same = 0;
    if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]) {
        same = 1;
    }
    return same;
}

// 0x080b58ce  size=54  Vec4_Clear  status=translated  naming=unknown
// Zeroes a four-float vector.
void Vec4_Clear(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    v[3] = 0.0f;
}

// 0x080b5904  size=46  Vec4_Set  status=translated  naming=unknown
// Stores four floats into a vector.
void Vec4_Set(float *v, float x, float y, float z, float w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

// 0x080b5932  size=63  Vec4_Copy_080b5932  status=translated  naming=unknown
// Copies a four-float vector from src to dst.
void Vec4_Copy_080b5932(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// 0x080b5972  size=135  Vec4_Equal  status=translated  naming=unknown
// Returns 1 when two four-float vectors compare equal.
uint32 Vec4_Equal(const float *a, const float *b)
{
    uint32 same;

    same = 0;
    if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]) {
        same = 1;
    }
    return same;
}

// 0x080b59fa  size=15  Word_Copy  status=translated  naming=unknown
// Copies one word from src to dst.
void Word_Copy(const uint32 *src, uint32 *dst)
{
    *dst = *src;
}

// 0x080b5a0a  size=21  Word_Equal  status=translated  naming=unknown
// Compares two words (bool result).
bool Word_Equal(const uint32 *a, const uint32 *b)
{
    return *a == *b;
}

// 0x080b5a20  size=152  Str_NCaseCmpN  status=translated  naming=inferred
// Bounded case-insensitive compare (strncasecmp-like). Equal-length tails
// compare by raw byte; differing case-folded characters decide the sign.
int Str_NCaseCmpN(const char *s1, const char *s2, int n)
{
    int c1;
    int c2;

    do {
        if (n == 0 || *s1 == '\0') {
            if (n == 0 || *s1 == *s2) {
                return 0;
            }
            return -1;
        }
        c1 = __builtin_tolower((int)*s1);
        c2 = __builtin_tolower((int)*s2);
        n = n - 1;
        s1 = s1 + 1;
        s2 = s2 + 1;
        if ((char)c1 < (char)c2) {
            return -1;
        }
    } while ((char)c1 <= (char)c2);
    return 1;
}

// 0x080b5ab8  size=41  Float_SelectNonNegative_080b5ab8  status=translated  naming=unknown
// Returns b when a >= 0 (comparison as in the binary), otherwise c.
float Float_SelectNonNegative_080b5ab8(float a, float b, float c)
{
    if (0.0f <= a) {
        return b;
    }
    return c;
}

// 0x080b5b06  size=58  MsgBuf_Init  status=translated  naming=inferred
// Message buffer init: data, capacity, zero cursor, warn flag, clear overflow.
void MsgBuf_Init(struct msgbuf_s *buf, int32 size, void *data, uint8 warn)
{
    buf->data = (uint8 *)data;
    buf->maxsize = size;
    buf->cursor = 0;
    buf->warn_on_overflow = warn;
    buf->overflowed = 0;
}

// 0x080b5b40  size=51  MsgBuf_InitRead  status=translated  naming=inferred
// Reader init: warn enabled, transfer = MsgBuf_ReadData.
void MsgBuf_InitRead(struct msgbuf_s *buf, int32 size, void *data)
{
    MsgBuf_Init(buf, size, data, 1);
    buf->transfer = MsgBuf_ReadData;
}

// 0x080b5b74  size=57  MsgBuf_InitWrite  status=translated  naming=inferred
// Writer init: caller-chosen warn flag, transfer = MsgBuf_WriteBytes (writer tag).
void MsgBuf_InitWrite(struct msgbuf_s *buf, int32 size, void *data, uint8 warn)
{
    MsgBuf_Init(buf, size, data, warn);
    buf->transfer = (msgbuf_transfer_fn)MsgBuf_WriteBytes;
}

// 0x080b5bae  size=35  Word_Clear  status=translated  naming=unknown
// Zeroes a four-byte field.
void Word_Clear(void *p)
{
    memset(p, 0, 4);
}

// 0x080b5bd2  size=21  MsgBuf_IsReader  status=translated  naming=inferred
// True when the transfer tag is the reader.
bool MsgBuf_IsReader(const struct msgbuf_s *buf)
{
    return buf->transfer == MsgBuf_ReadData;
}

// 0x080b5be8  size=21  MsgBuf_IsWriter  status=translated  naming=inferred
// True when the transfer tag is the writer.
bool MsgBuf_IsWriter(const struct msgbuf_s *buf)
{
    return buf->transfer == (msgbuf_transfer_fn)MsgBuf_WriteBytes;
}

// 0x080b5bfe  size=173  MsgBuf_WriteData  status=translated  naming=observed
// Evidence: "Couldn't write %i bytes to %i-byte buffer (only %i bytes free)".
// Appends len bytes; on overflow sets the flag and optionally warns.
void MsgBuf_WriteData(struct msgbuf_s *buf, uint32 len, const void *src)
{
    if (len != 0 && buf->overflowed == 0) {
        if (buf->maxsize < (int)(len + (uint32)buf->cursor)) {
            if (buf->warn_on_overflow != 0) {
                Com_Error(1, "Couldn't write %i bytes to %i-byte buffer (only %i bytes free)\n",
                            (int)len, buf->maxsize, buf->maxsize - buf->cursor);
            }
            buf->overflowed = 1;
        } else {
            memcpy(buf->data + buf->cursor, src, len);
            buf->cursor = (int)(len + (uint32)buf->cursor);
        }
    }
}

// 0x080b5d5e  size=173  MsgBuf_ReadData  status=translated  naming=observed
// Evidence: "Couldn't read %i bytes from %i-byte buffer (only %i bytes left)".
// Reads len bytes from the cursor into dst; on overflow sets the flag.
void MsgBuf_ReadData(struct msgbuf_s *buf, uint32 len, void *dst)
{
    if (len != 0 && buf->overflowed == 0) {
        if (buf->maxsize < (int)(len + (uint32)buf->cursor)) {
            if (buf->warn_on_overflow != 0) {
                Com_Error(1, "Couldn't read %i bytes from %i-byte buffer (only %i bytes left)\n",
                            (int)len, buf->maxsize, buf->maxsize - buf->cursor);
            }
            buf->overflowed = 1;
        } else {
            memcpy(dst, buf->data + buf->cursor, len);
            buf->cursor = (int)(len + (uint32)buf->cursor);
        }
    }
}

// 0x080b5eb0  size=10  ParseSession_Get  status=translated  naming=inferred
// Returns the parse-session singleton (g_0817e420).
struct ComParseState *ParseSession_Get(void)
{
    return &g_0817e420;
}

// 0x080b5eba  size=120  ParseSlot_Init  status=translated  naming=inferred
// Initialises a parse slot: fixed marker fields and the shared literal pointers.
void ParseSlot_Init(struct ParseSession *slot)
{
    slot->lines = 1;
    slot->tokenUngotten = 0;
    slot->spaceDelimited = 1;
    slot->keepStringQuotes = 0;
    slot->csvMode = 0;
    slot->allowNegativeNumbers = 0;
    slot->errorPrefix = kAlreadyParsing;
    slot->warningPrefix = kAlreadyParsing;
    slot->savedLines = 0;
    slot->savedPos = 0;
}

// 0x080b5f32  size=209  Com_BeginParseSession  status=translated  naming=observed
// Evidence: "\x15Com_BeginParseSession: session overflow trying to parse %s\n"
// (0x0814f600) and the "Already parsing:" listing. Pushes a named slot. At 15
// active slots it lists them and reports overflow, then pushes anyway (the
// binary does not return), writing past the struct as the original does.
void Com_BeginParseSession(const char *name)
{
    struct ComParseState *session;
    struct ParseSession *slot;
    int i;

    session = ParseSession_Get();
    if (session->sessionIndex == 0xf) {
        Com_Printf("Already parsing:\n");
        for (i = 0; i < session->sessionIndex; i++) {
            Com_Printf("%i. %s\n", i, PARSE_SLOT_AT(session, i)->filename);
        }
        Com_Error(0, "\x15" "Com_BeginParseSession: session overflow trying to parse %s\n", name);
    }
    session->sessionIndex = session->sessionIndex + 1;
    slot = PARSE_SLOT_AT(session, session->sessionIndex);
    ParseSlot_Init(slot);
    Q_strncpyz(slot->filename, name, 0x40);
}
