#include "cod2_types.h"
#include <ctype.h>
#include "cod2_canon.h"
#include "cod2_globals.h"
/* cod2_prototypes.h (generated) also lists the 132 PLT import thunks of chunk 000 without extern "C";
 * they must not clash with the system headers, so rename them while the shared prototypes are read. */
#define COD2_PLT_SHIM_BEGIN
#include "cod2_prototypes.h"
#include "chunk_039_types.h"

/* chunk_039: player animation script (BG_Anim*) code from 0x080d6c32 to 0x080da9c8.
 * Names containing BG_ are observed where an error string in the function carries the
 * name, otherwise inferred from the id Tech 3 / IW counterpart (bg_animation.c style
 * code: same script keywords, same error-message prefixes). All float constants are
 * 32-bit floats in the binary (fld dword), conversions to int truncate (fldcw 0xc00). */

/* ------------------------------------------------------------------------ */
/* Empty callback stubs (push ebp; mov ebp,esp; [mov eax,0;] pop ebp; ret). */
/* ------------------------------------------------------------------------ */

// 0x080d6c32  size=5  FUN_080d6c32  status=translated  naming=unknown
// Empty function (disassembly: push ebp; mov ebp,esp; pop ebp; ret). Self-referencing caller entry only.
void Noop_080d6c32(void)
{
}

// 0x080d6c38  size=5  FUN_080d6c38  status=translated  naming=unknown
// Empty function, same body as Noop_080d6c32.
void Noop_080d6c38(void)
{
}

// 0x080d6c3e  size=10  FUN_080d6c3e  status=translated  naming=unknown
// Returns 0 (mov eax,0).
int ReturnZero_080d6c3e(void)
{
    return 0;
}

// 0x080d6c48  size=5  FUN_080d6c48  status=translated  naming=unknown
// Empty function.
void Noop_080d6c48(void)
{
}

// 0x080d6c4e  size=10  FUN_080d6c4e  status=translated  naming=unknown
// Returns 0.
int ReturnZero_080d6c4e(void)
{
    return 0;
}

// 0x080d6c58  size=5  FUN_080d6c58  status=translated  naming=unknown
// Empty function.
void Noop_080d6c58(void)
{
}

// 0x080d6c5e  size=5  FUN_080d6c5e  status=translated  naming=unknown
// Empty function.
void Noop_080d6c5e(void)
{
}

// 0x080d6c64  size=5  FUN_080d6c64  status=translated  naming=unknown
// Empty function.
void Noop_080d6c64(void)
{
}

// 0x080d6c6a  size=5  FUN_080d6c6a  status=translated  naming=unknown
// Empty function.
void Noop_080d6c6a(void)
{
}

// 0x080d6c70  size=5  FUN_080d6c70  status=translated  naming=unknown
// Empty function.
void Noop_080d6c70(void)
{
}

// 0x080d6c76  size=5  FUN_080d6c76  status=translated  naming=unknown
// Empty function.
void Noop_080d6c76(void)
{
}

// 0x080d6ca6  size=5  FUN_080d6ca6  status=translated  naming=unknown
// Empty function.
void Noop_080d6ca6(void)
{
}

// 0x080d6cac  size=5  FUN_080d6cac  status=translated  naming=unknown
// Empty function; called from 0x0806288a and 0x080628fa.
void Noop_080d6cac(void)
{
}

// 0x080d6cb2  size=5  FUN_080d6cb2  status=translated  naming=unknown
// Empty function.
void Noop_080d6cb2(void)
{
}

// 0x080d6cb8  size=5  FUN_080d6cb8  status=translated  naming=unknown
// Empty function.
void Noop_080d6cb8(void)
{
}

// 0x080d6cbe  size=10  FUN_080d6cbe  status=translated  naming=unknown
// Returns 0.
int ReturnZero_080d6cbe(void)
{
    return 0;
}

// 0x080d6cc8  size=5  FUN_080d6cc8  status=translated  naming=unknown
// Empty function.
void Noop_080d6cc8(void)
{
}

// 0x080d6cce  size=5  FUN_080d6cce  status=translated  naming=unknown
// Empty function.
void Noop_080d6cce(void)
{
}

// 0x080d6cd4  size=10  FUN_080d6cd4  status=translated  naming=unknown
// Returns 0.
int ReturnZero_080d6cd4(void)
{
    return 0;
}

/* ------------------------------------------------------------------------ */
/* String helpers and animation lookup                                       */
/* ------------------------------------------------------------------------ */

// 0x080d6ce0  size=96  FUN_080d6ce0  status=translated  naming=inferred
// Case-insensitive position-weighted string hash (sum of tolower(c) * (i + 0x77), -1 mapped to 0).
// Evidence: id Tech BG_StringHashValue counterpart; used by every BG_*IndexForString lookup in this chunk.
int BG_StringHashValue(const char *string)
{
    int hash;
    int i;
    int c;

    hash = 0;
    for (i = 0; string[i] != '\0'; i++) {
        c = tolower((int)string[i]);
        hash = hash + (int)(char)c * (i + 0x77);
    }
    if (hash == -1) {
        hash = 0;
    }
    return hash;
}

// 0x080d6d40  size=143  FUN_080d6d40  status=translated  naming=inferred
// Formats the message into a 1024 byte buffer and raises a drop error, adding the script name and
// parse line while mp/playeranim.script is being parsed (g_playerAnimScriptPath non-null).
// Evidence: every "BG_..." parse-error string in this chunk goes through it.
void BG_AnimParseError(const char *fmt, ...)
{
    char message[1036];
    va_list args;

    va_start(args, fmt);
    vsnprintf(message, 0x400, fmt, args);
    va_end(args);
    if (g_playerAnimScriptPath == 0) {
        Com_Error(1, "\025%s", message);
    } else {
        Com_Error(1, "\025%s: (%s, line %i)", message, g_playerAnimScriptPath, Com_GetParseLine() + 1);
    }
}

// 0x080d6dd0  size=390  FUN_080d6dd0  status=translated  naming=observed
// Looks the animation name up (hash then Q_stricmp). Without a name table it scans the animation
// array; with a name table (g_animNameTable) it scans/extends that table via Scr_AddAnimTreeEntry.
// Evidence: string "BG_AnimationIndexForString: unknown player animation '%s'" at 0x081527c0.
int BG_AnimationIndexForString(const char *name)
{
    int hash;
    int i;
    animation_t *anim;
    animNameEntry_t *entry;

    if (g_animNameTable == 0) {
        hash = BG_StringHashValue(name);
        anim = g_curAnimScriptData->animations;
        for (i = 0; i < g_curAnimScriptData->numAnimations; i++) {
            if (hash == anim->nameHash && Q_stricmp(name, anim->name) == 0) {
                return i;
            }
            anim = anim + 1;
        }
        BG_AnimParseError("BG_AnimationIndexForString: unknown player animation '%s'", name);
        return -1;
    } else {
        hash = BG_StringHashValue(name);
        entry = g_animNameTable;
        for (i = 0; i < *g_animNameTableCount; i++) {
            if (hash == entry->hash && Q_stricmp(name, entry->name) == 0) {
                return i;
            }
            entry = entry + 1;
        }
        entry = g_animNameTable + *g_animNameTableCount;
        Scr_AddAnimTreeEntry("multiplayer", name, (unsigned int)entry, g_animData->isMultiplayer);
        strcpy(entry->name, name);
        entry->hash = hash;
        *g_animNameTableCount = *g_animNameTableCount + 1;
        return *g_animNameTableCount - 1;
    }
}

// 0x080d6f56  size=147  FUN_080d6f56  status=translated  naming=observed
// Returns the animation_t whose name matches, or raises an error and returns 0.
// Evidence: string "BG_AnimationForString: unknown player animation '%s'" at 0x08152820.
animation_t *BG_AnimationForString(const char *name)
{
    int hash;
    int i;
    animation_t *anim;

    hash = BG_StringHashValue(name);
    i = 0;
    anim = g_curAnimScriptData->animations;
    while (true) {
        if (g_curAnimScriptData->numAnimations <= i) {
            Com_Error(1, "\025BG_AnimationForString: unknown player animation '%s'", name);
            return 0;
        }
        if (hash == anim->nameHash && Q_stricmp(name, anim->name) == 0) {
            break;
        }
        i = i + 1;
        anim = anim + 1;
    }
    return anim;
}

// 0x080d6fea  size=169  FUN_080d6fea  status=translated  naming=observed
// Finds a token in a { string, hash } table that ends with a null string; computes missing hashes
// lazily. On a miss it errors unless allowFail is set and returns -1.
// Evidence: string "BG_IndexForString: unknown token '%s'" at 0x08152860.
int BG_IndexForString(const char *token, animStringItem_t *strings, int allowFail)
{
    int hash;
    int index;
    animStringItem_t *item;

    hash = BG_StringHashValue(token);
    index = 0;
    item = strings;
    while (true) {
        if (item->string == 0) {
            if (allowFail == 0) {
                BG_AnimParseError("BG_IndexForString: unknown token '%s'", token);
            }
            return -1;
        }
        if (item->hash == -1) {
            item->hash = BG_StringHashValue(item->string);
        }
        if (hash == item->hash && Q_stricmp(token, item->string) == 0) {
            break;
        }
        item = item + 1;
        index = index + 1;
    }
    return index;
}

// 0x080d7094  size=103  FUN_080d7094  status=translated  naming=observed
// Appends a string to a fixed buffer (offset advanced past the terminator) and returns its address.
// Evidence: string "BG_CopyStringIntoBuffer: out of buffer space" at 0x081528a0.
char *BG_CopyStringIntoBuffer(const char *string, char *buffer, unsigned int bufferSize, int *offset)
{
    int start;

    if (bufferSize <= strlen(string) + *offset + 1) {
        BG_AnimParseError("BG_CopyStringIntoBuffer: out of buffer space");
    }
    start = *offset;
    strcpy(buffer + start, string);
    *offset = strlen(string) + *offset + 1;
    return buffer + start;
}

// 0x080d70fc  size=47  FUN_080d70fc  status=translated  naming=inferred
// Stores a PLAYERANIMTYPE name and its hash in g_playerAnimTypeNameTable[index] (the table referenced as the
// string list of condition 0 in g_animConditionDefs). Called for each entry by BG_RegisterAnimTypes.
void BG_SetPlayerAnimTypeName(int index, const char *name)
{
    g_playerAnimTypeNameTable[index].string = name;
    g_playerAnimTypeNameTable[index].hash = BG_StringHashValue(name);
}

// 0x080d712c  size=41  FUN_080d712c  status=translated  naming=inferred
// Clears the 128 entry PLAYERANIMTYPE name table and re-registers the names (BG_RegisterAnimTypes calls
// BG_SetPlayerAnimTypeName for every loaded weapon anim type).
void BG_ClearPlayerAnimTypeNames(void)
{
    memset(g_playerAnimTypeNameTable, 0, 0x400);
    BG_RegisterAnimTypes();
}

// 0x080d7156  size=139  FUN_080d7156  status=translated  naming=inferred
// Returns the name-table entry whose id equals the animation index, or 0 when absent. Errors when
// the index is not below the animation count.
// Evidence: string "Player animation index %i out of 0 to %i range" at 0x081528e0.
animNameEntry_t *BG_AnimNameEntryForIndex(unsigned int index)
{
    animNameEntry_t *entry;
    int i;

    if ((unsigned int)g_curAnimScriptData->numAnimations <= index) {
        Com_Error(1, "\025Player animation index %i out of 0 to %i range", index,
                    g_curAnimScriptData->numAnimations);
    }
    i = 0;
    entry = g_animNameTable;
    while (true) {
        if (*g_animNameTableCount <= i) {
            return 0;
        }
        if (index == entry->id) {
            break;
        }
        i = i + 1;
        entry = entry + 1;
    }
    return entry;
}

// 0x080d71e2  size=260  FUN_080d71e2  status=translated  naming=inferred
// Clears animation_t::referenced for all animations, then (non multiplayer only) marks every
// animation named by a command of scriptEvents[10]. Called at the end of BG_LoadAnim.
void BG_ClearAnimReferencedFlags(animScriptData_t *data)
{
    int i;
    int j;
    animScriptItem_t *item;
    animCommand_t *command;

    for (i = 0; i < data->numAnimations; i++) {
        data->animations[i].referenced = 0;
    }
    if (g_animData->isMultiplayer == 0) {
        for (i = 0; i < data->scriptEvents[10].numItems; i++) {
            item = data->scriptEvents[10].items[i];
            for (j = 0; j < item->numCommands; j++) {
                command = &item->commands[j];
                if (command->bodyPart[0] != 0) {
                    data->animations[command->animIndex[0]].referenced = 1;
                }
                if (command->bodyPart[1] != 0) {
                    data->animations[command->animIndex[1]].referenced = 1;
                }
            }
        }
    }
}

// 0x080d72e6  size=785  FUN_080d72e6  status=translated  naming=inferred
// Builds animations[] from the loaded animation tree: entry 0 is "root", unreferenced slots are
// "unused", real animations get name/hash/duration (ms, min 500)/moveSpeed (delta length per second)
// and the looping flag, then runs BG_AnimParseAnimScript and BG_ClearAnimReferencedFlags.
// Evidence: strings "root" / "unused"; calls the parser and the referenced-flag clear.
void BG_LoadAnim(void)
{
    animScriptData_t *data;
    animation_t *anim;
    void *tree;
    int numAnims;
    int i;
    animNameEntry_t *entry;
    float length;
    float rotDelta[3];
    float posDelta[4];

    data = g_curAnimScriptData;
    tree = g_curAnimScriptData->animTree;
    numAnims = XAnim_LoadU32At4(tree);
    g_curAnimScriptData->numAnimations = numAnims;
    anim = &data->animations[0];
    anim->flags = anim->flags | 0x101;
    Q_strncpyz(anim->name, "root", 0x40);
    anim->nameHash = 0;
    for (i = 1; i < numAnims; i++) {
        anim = &data->animations[i];
        entry = BG_AnimNameEntryForIndex(i);
        if (entry == 0) {
            anim->flags = anim->flags | 0x100;
            Q_strncpyz(anim->name, "unused", 0x40);
            anim->nameHash = 0;
        } else {
            if (!XAnim_NodeIsLeaf((struct XAnimNodeTable *)tree, i)) {
                anim->flags = anim->flags | 1;
                if (entry == 0) {
                    Q_strncpyz(anim->name, XAnim_NodeClipName((struct XAnimNodeTable *)tree, i), 0x40);
                    anim->nameHash = 0;
                } else {
                    Q_strncpyz(anim->name, entry->name, 0x40);
                    anim->nameHash = entry->hash;
                }
                if (anim->blendTime == 0) {
                    anim->blendTime = -1;
                }
                anim->duration = 0;
                anim->moveSpeed = 0.0f;
            } else {
                Q_strncpyz(anim->name, XAnim_NodeClipName((struct XAnimNodeTable *)tree, i), 0x40);
                anim->nameHash = BG_StringHashValue(anim->name);
                if (anim->blendTime == 0) {
                    anim->blendTime = -1;
                }
                length = XAnim_NodeDurationSec((struct XAnimNodeTable *)tree, i);
                if (length != 0.0f) {
                    anim->duration = (int)(length * 1000.0f);
                    XAnim_NodeBlendSample((struct XAnimNodeTable *)tree, i, rotDelta, posDelta, 0.0f, 1.0f);
                    anim->moveSpeed = (float)(Vec3_Length_080dc54a(posDelta) / length);
                    if (anim->duration < 500) {
                        anim->duration = 500;
                    }
                } else {
                    anim->duration = 500;
                    anim->moveSpeed = 0.0f;
                }
                if (XAnim_NodeLoopFlag((struct XAnimNodeTable *)tree, i) != 0) {
                    anim->flags = anim->flags | 0x80;
                }
            }
        }
    }
    BG_AnimParseAnimScript(g_curAnimScriptData, 0, 0);
    BG_ClearAnimReferencedFlags(g_curAnimScriptData);
}

/* ------------------------------------------------------------------------ */
/* Script parsing                                                            */
/* ------------------------------------------------------------------------ */

// 0x080d75f8  size=891  FUN_080d75f8  status=translated  naming=observed
// Parses one comma-terminated condition expression ("a AND b NOT c", "none", "all", "MINUS"/"NOT")
// from the current line into a 64-bit mask. Words resolve first against the script-defined values
// (g_animDefineStrings/g_animDefineValues) and then against the condition's string table. Only the first dword of
// result is cleared on entry; the local mask persists between terms (quirks preserved).
// Evidence: strings "BG_ParseConditionBits: unexpected end of condition" and "...: unexpected '%s'".
void BG_ParseConditionBits(const char **text, animStringItem_t *stringTable, int conditionIndex,
                           unsigned int *result)
{
    int done;
    bool negate;
    char *token;
    char tokenBuf[64];
    unsigned int bits[2];
    int index;
    size_t len;

    done = 0;
    negate = false;
    token = 0;
    tokenBuf[0] = '\0';
    memset(result, 0, 4);
    memset(bits, 0, 8);
    for (;;) {
        for (;;) {
            do {
                for (;;) {
                    for (;;) {
                        if (done != 0) {
                            return;
                        }
                        token = Com_ParseOnLine(text);
                        if (token == 0 || *token == '\0') {
                            Com_UngetToken();
                            done = 1;
                            if (tokenBuf[0] == '\0') {
                                return;
                            }
                        }
                        if (Q_stricmp(token, ",") == 0) {
                            done = 1;
                        }
                        if (Q_stricmp(token, "none") != 0) {
                            break;
                        }
                        BitSet_Set(result, 0);
                    }
                    if (Q_stricmp(token, "none,") != 0) {
                        break;
                    }
                    BitSet_Set(result, 0);
                    done = 1;
                }
                if (Q_stricmp(token, "NOT") == 0) {
                    token = (char *)"MINUS";
                }
                if (done == 0 && Q_stricmp(token, "AND") != 0 && Q_stricmp(token, "MINUS") != 0) {
                    len = strlen(token);
                    if (token[len - 1] == ',') {
                        done = 1;
                        len = strlen(token);
                        token[len - 1] = '\0';
                    }
                    if (tokenBuf[0] != '\0') {
                        I_strncat(tokenBuf, 0x40, " ");
                    }
                    I_strncat(tokenBuf, 0x40, token);
                }
            } while (Q_stricmp(token, "AND") != 0 && Q_stricmp(token, "MINUS") != 0 && done == 0);
            if (tokenBuf[0] == '\0') {
                break;
            }
        apply_term:
            if (Q_stricmp(tokenBuf, "all") == 0) {
                bits[0] = 0xffffffff;
                bits[1] = 0xffffffff;
            } else {
                index = BG_IndexForString(tokenBuf, g_animDefineStrings[conditionIndex], 1);
                if (index < 0) {
                    index = BG_IndexForString(tokenBuf, stringTable, 0);
                    BitSet_Set(bits, index);
                } else {
                    bits[0] = g_animDefineValues[conditionIndex][index][0];
                    bits[1] = g_animDefineValues[conditionIndex][index][1];
                }
            }
            if (negate) {
                result[0] = ~bits[0] & result[0];
                result[1] = ~bits[1] & result[1];
            } else {
                result[0] = bits[0] | result[0];
                result[1] = bits[1] | result[1];
            }
            tokenBuf[0] = '\0';
            if (Q_stricmp(token, "MINUS") == 0) {
                negate = true;
            }
        }
        if (done != 0) {
            BG_AnimParseError("BG_ParseConditionBits: unexpected end of condition");
            goto apply_term;
        }
        if (Q_stricmp(token, "MINUS") != 0) {
            BG_AnimParseError("BG_ParseConditionBits: unexpected '%s'", token);
            goto apply_term;
        }
        negate = true;
    }
}

// 0x080d7974  size=468  FUN_080d7974  status=translated  naming=observed
// Parses the condition list of one script item up to "default" or end of line. Each condition name
// resolves through g_animConditionNames; bit-flag conditions go to BG_ParseConditionBits, value conditions read
// one more token. The two value dwords persist across conditions (stale value[1] for value type).
// Evidence: strings "BG_ParseConditions: no conditions found" and "...expected condition value...".
int BG_ParseConditions(const char **text, animScriptItem_t *item)
{
    char *token;
    int conditionIndex;
    unsigned int value[2];
    size_t len;

    memset(value, 0, 8);
    while ((token = Com_ParseOnLine(text)) != 0 && *token != '\0') {
        if (Q_stricmp(token, "default") == 0) {
            return 1;
        }
        conditionIndex = BG_IndexForString(token, g_animConditionNames, 0);
        if (g_animConditionDefs[conditionIndex].type == 0) {
            BG_ParseConditionBits(text, g_animConditionDefs[conditionIndex].strings, conditionIndex, value);
        } else if (g_animConditionDefs[conditionIndex].type == 1) {
            if (g_animConditionDefs[conditionIndex].strings == 0) {
                value[0] = 1;
            } else {
                token = Com_ParseOnLine(text);
                if (token == 0 || *token == '\0') {
                    BG_AnimParseError("BG_ParseConditions: expected condition value, found end of line");
                }
                len = strlen(token);
                if (token[len - 1] == ',') {
                    len = strlen(token);
                    token[len - 1] = '\0';
                }
                value[0] = BG_IndexForString(token, g_animConditionDefs[conditionIndex].strings, 0);
            }
        }
        item->conditions[item->numConditions].index = conditionIndex;
        item->conditions[item->numConditions].value[0] = value[0];
        item->conditions[item->numConditions].value[1] = value[1];
        item->numConditions = item->numConditions + 1;
    }
    if (item->numConditions == 0) {
        BG_AnimParseError("BG_ParseConditions: no conditions found");
    }
    return 1;
}

// 0x080d7b48  size=1903  FUN_080d7b48  status=translated  naming=observed
// Parses the command block of one script item: "<bodypart> <animation> [duration n] [turretanim]
// [blendtime n]" pairs (a second part unless the first is 'both'), then "sound <alias>" entries,
// until the closing brace (which is pushed back). While not collecting names (g_animNameTable == 0) it
// also updates animation_t move-type bits, flags and blend times.
// Evidence: strings "BG_ParseCommands: expected animation", "...unknown parameter '%s'" etc.
void BG_ParseCommands(const char **text, animScriptItem_t *item, animScriptData_t *data)
{
    animCommand_t *command;
    int partIndex;
    char *token;
    animation_t *anim;
    unsigned long long moveBit;
    bool more;
    int i;
    int blendTime;
    int soundIndex;

    command = 0;
    partIndex = 0;
next_token:
    if (partIndex < 1) {
        token = Com_Parse(text);
    } else {
        token = Com_ParseOnLine(text);
    }
    if (token != 0 && *token != '\0') {
        if (Q_stricmp(token, "}") != 0) {
            if (partIndex == 0) {
                if (7 < item->numCommands) {
                    BG_AnimParseError("BG_ParseCommands: exceeded maximum number of animations (%i)", 8);
                }
                command = &item->commands[item->numCommands];
                item->numCommands = item->numCommands + 1;
                memset(command, 0, 4);
            }
            command->bodyPart[partIndex] = (short)BG_IndexForString(token, g_animBodyPartNames, 1);
            if (0 < command->bodyPart[partIndex]) {
                goto parse_animation;
            }
            *text = *text - strlen(token);
            goto parse_sounds;
        }
        *text = *text - strlen(token);
    }
    return;

parse_animation:
    token = Com_ParseOnLine(text);
    if (token == 0 || *token == '\0') {
        BG_AnimParseError("BG_ParseCommands: expected animation");
    }
    command->animIndex[partIndex] = (short)BG_AnimationIndexForString(token);
    command->animDuration[partIndex] = (short)data->animations[command->animIndex[partIndex]].duration;
    if (g_animNameTable == 0) {
        if (g_animParseMoveType != 0 && command->bodyPart[partIndex] != 2) {
            anim = &data->animations[command->animIndex[partIndex]];
            moveBit = 1ULL << g_animParseMoveType;
            anim->moveTypeBits[0] = anim->moveTypeBits[0] | (unsigned int)moveBit;
            anim->moveTypeBits[1] = anim->moveTypeBits[1] | (unsigned int)(moveBit >> 32);
            if ((g_animParseMoveType == 0x12 || g_animParseMoveType == 0x13) && anim->moveSpeed != 0.0f) {
                anim->flags = anim->flags | 2;
            }
            for (i = 0; i < item->numConditions; i++) {
                if (item->conditions[i].index == 8) {
                    if (item->conditions[i].value[0] == 1) {
                        data->animations[command->animIndex[partIndex]].flags =
                            data->animations[command->animIndex[partIndex]].flags | 0x10;
                    } else if (item->conditions[i].value[0] == 2) {
                        data->animations[command->animIndex[partIndex]].flags =
                            data->animations[command->animIndex[partIndex]].flags | 0x20;
                    }
                    break;
                }
            }
        }
        if (g_animParseEventIndex == 2) {
            data->animations[command->animIndex[partIndex]].flags =
                data->animations[command->animIndex[partIndex]].flags | 8;
            data->animations[command->animIndex[partIndex]].blendTime = 0x1e;
        } else if (g_animParseEventIndex == 1) {
            data->animations[command->animIndex[partIndex]].moveSpeed = 0.0f;
            data->animations[command->animIndex[partIndex]].flags =
                data->animations[command->animIndex[partIndex]].flags | 0x40;
        } else if (g_animParseEventIndex == 10) {
            data->animations[command->animIndex[partIndex]].moveSpeed = 0.0f;
        } else if (0x14 < g_animParseMoveType && g_animParseMoveType < 0x1f) {
            data->animations[command->animIndex[partIndex]].moveSpeed = 0.0f;
        }
    }
    do {
        more = false;
        token = Com_ParseOnLine(text);
        if (token == 0 || *token == '\0') {
            Com_UngetToken();
        } else if (Q_stricmp(token, "duration") == 0) {
            more = true;
            token = Com_ParseOnLine(text);
            if (token == 0 || *token == '\0') {
                BG_AnimParseError("BG_ParseCommands: expected duration value");
            }
            command->animDuration[partIndex] = (short)atoi(token);
        } else if (Q_stricmp(token, "turretanim") == 0) {
            more = true;
            if (g_animNameTable == 0) {
                data->animations[command->animIndex[partIndex]].flags =
                    data->animations[command->animIndex[partIndex]].flags | 4;
            }
            if (command->bodyPart[partIndex] != 3) {
                BG_AnimParseError(
                    "BG_ParseCommands: Turret animations can only be played on the 'both' body part");
            }
        } else if (Q_stricmp(token, "blendtime") == 0) {
            more = true;
            token = Com_ParseOnLine(text);
            if (token == 0 || *token == '\0') {
                BG_AnimParseError("BG_ParseCommands: expected blendtime value");
            }
            if (g_animNameTable == 0) {
                blendTime = atoi(token);
                data->animations[command->animIndex[partIndex]].blendTime = blendTime;
            }
        } else {
            Com_UngetToken();
        }
    } while (more);
    if (command->bodyPart[partIndex] == 3) {
        goto parse_sounds;
    }
    more = 0 < partIndex;
    partIndex = partIndex + 1;
    if (more) {
        goto parse_sounds;
    }
    goto next_token;

parse_sounds:
    while ((token = Com_ParseOnLine(text)) != 0 && *token != '\0') {
        if (Q_stricmp(token, "sound") == 0) {
            token = Com_ParseOnLine(text);
            if (token == 0 || *token == '\0') {
                BG_AnimParseError("BG_ParseCommands: expected sound");
            }
            if (strstr(token, ".wav") != 0) {
                BG_AnimParseError("BG_ParseCommands: wav files not supported, only sound scripts");
            }
            soundIndex = g_animData->soundIndexFn(token);
            command->soundIndex = soundIndex;
        } else {
            BG_AnimParseError("BG_ParseCommands: unknown parameter '%s'", token);
        }
    }
    partIndex = 0;
    goto next_token;
}

// 0x080d82b8  size=3318  FUN_080d82b8  status=translated  naming=observed
// Parses mp/playeranim.script (loaded once into g_animScriptText) into scriptData: sections "defines"
// (set <type> <name> = <bits>), "animations", "canned_animations", "statechanges" and "events".
// Depth/stack bookkeeping: depth 0 = section level, then state / move type / item / commands.
// The "unexpected end of file" error is raised with a format that has %s but no argument (as in the
// binary). While parsing, g_curAnimScriptData/g_animNameTable/g_animNameTableCount are set; g_playerAnimScriptPath is cleared at the end.
// Evidence: strings "BG_AnimParseAnimScript", "mp/playeranim.script" (g_playerAnimScriptPath), many "BG_AnimParseAnimScript: ..." errors.
void BG_AnimParseAnimScript(animScriptData_t *data, animNameEntry_t *animNames, int *numAnimNames)
{
    int fileHandle;
    unsigned int fileLength;
    int section;
    int depth;
    int stack[4];
    const char *text;
    char *token;
    int keyword;
    int conditionType;
    int fromState;
    animScript_t *script;
    animScriptItem_t *item;
    animScriptItem_t itemBuffer;
    int i;
    unsigned int *src;
    unsigned int *dst;

    item = 0;
    if (g_animScriptTextLoaded == 0) {
        fileLength = FSH_FOpenFile(g_playerAnimScriptPath, &fileHandle, 0);
        if ((int)fileLength < 0) {
            Com_Error(1, "\025Couldn't load player animation script %s\n", g_playerAnimScriptPath);
        }
        if (0x1869e < fileLength) {
            Com_Error(1, "\025Couldn't load player animation script %s\n", g_playerAnimScriptPath);
        }
        FS_Read(g_animScriptText, fileLength, fileHandle);
        g_animScriptText[fileLength] = '\0';
        FS_FCloseFile(fileHandle);
        g_animScriptTextLoaded = 1;
    }
    g_curAnimScriptData = data;
    g_animNameTable = animNames;
    g_animNameTableCount = numAnimNames;
    section = 0;
    BG_ClearPlayerAnimTypeNames();
    memset(g_animDefineStrings, 0, 0x480);
    memset(g_animDefineStringBuf, 0, 10000);
    memset(g_animDefineCounts, 0, 0x24);
    g_animDefineStringBufUsed = 0;
    for (i = 0; i < 3; i++) {
        stack[i] = -1;
    }
    depth = 0;
    script = 0;
    text = g_animScriptText;
    Com_BeginParseSession("BG_AnimParseAnimScript");
    while (true) {
        token = Com_Parse(&text);
        if (token == 0 || *token == '\0') {
            if (depth != 0) {
                BG_AnimParseError("BG_AnimParseAnimScript: unexpected end of file: %s");
            }
            g_playerAnimScriptPath = 0;
            Com_EndParseSession();
            return;
        }
        keyword = BG_IndexForString(token, g_animScriptSectionNames, 1);
        if (keyword >= 0) {
            if (depth != 0) {
                BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
            }
            section = keyword;
            g_animParseMoveType = 0;
            g_animParseEventIndex = -1;
            continue;
        }
        switch (section) {
        case 0:
            if (Q_stricmp(token, "set") == 0) {
                token = Com_ParseOnLine(&text);
                if (token == 0 || *token == '\0') {
                    BG_AnimParseError("BG_AnimParseAnimScript: expected condition type string");
                }
                conditionType = BG_IndexForString(token, g_animConditionNames, 0);
                if (g_animConditionDefs[conditionType].type != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: can not make a define of type '%s'", token);
                }
                token = Com_ParseOnLine(&text);
                if (token == 0 || *token == '\0') {
                    BG_AnimParseError("BG_AnimParseAnimScript: expected condition define string");
                }
                g_animDefineStrings[conditionType][g_animDefineCounts[conditionType]].string =
                    BG_CopyStringIntoBuffer(token, g_animDefineStringBuf, 10000, &g_animDefineStringBufUsed);
                g_animDefineStrings[conditionType][g_animDefineCounts[conditionType]].hash =
                    BG_StringHashValue(g_animDefineStrings[conditionType][g_animDefineCounts[conditionType]].string);
                token = Com_ParseOnLine(&text);
                if (token == 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: expected '=', found end of line");
                }
                if (Q_stricmp(token, "=") != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: expected '=', found '%s'", token);
                }
                BG_ParseConditionBits(&text, g_animConditionDefs[conditionType].strings, conditionType,
                                      g_animDefineValues[conditionType][g_animDefineCounts[conditionType]]);
                g_animDefineCounts[conditionType] = g_animDefineCounts[conditionType] + 1;
            }
            break;
        case 1:
        case 2:
            if (Q_stricmp(token, "{") == 0) {
                if (2 < depth) {
                    BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
                }
                if (stack[depth] < 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
                }
                depth = depth + 1;
            } else if (Q_stricmp(token, "}") == 0) {
                depth = depth - 1;
                if (depth < 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
                }
                if (depth == 1) {
                    script = 0;
                }
                stack[depth] = -1;
            } else if (depth == 0 && stack[0] < 0) {
                if (Q_stricmp(token, "state") != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: expected 'state'");
                }
                token = Com_ParseOnLine(&text);
                if (token == 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: expected state type");
                }
                stack[depth] = BG_IndexForString(token, g_animStateNames, 0);
                token = Com_Parse(&text);
                if (token == 0 || Q_stricmp(token, "{") != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: expected '{'");
                }
                depth = depth + 1;
            } else if (depth == 1 && stack[1] < 0) {
                stack[1] = BG_IndexForString(token, g_animMoveTypeNames, 0);
                if (section == 1) {
                    script = &data->scriptAnims[stack[0]][stack[1]];
                    g_animParseMoveType = stack[1];
                } else if (section == 2) {
                    script = &data->scriptCannedAnims[stack[0]][stack[1]];
                }
                memset(script, 0, 0x204);
            } else if (depth == 2 && stack[2] < 0) {
                text = text - strlen(token);
                if (Q_strncmp(text, token, strlen(token)) != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: internal error");
                }
                memset(&itemBuffer, 0, 0xf4);
                stack[depth] = BG_ParseConditions(&text, &itemBuffer);
                if (0x7f < script->numItems) {
                    BG_AnimParseError("BG_AnimParseAnimScript: exceeded maximum items per script (%i)", 0x80);
                }
                if (0x7ff < data->numScriptItems) {
                    BG_AnimParseError("BG_AnimParseAnimScript: exceeded maximum global items (%i)", 0x800);
                }
                script->items[script->numItems] = &data->scriptItems[data->numScriptItems];
                data->numScriptItems = data->numScriptItems + 1;
                item = script->items[script->numItems];
                script->numItems = script->numItems + 1;
                src = (unsigned int *)&itemBuffer;
                dst = (unsigned int *)item;
                for (i = 0x3d; i != 0; i--) {
                    *dst = *src;
                    src = src + 1;
                    dst = dst + 1;
                }
            } else if (depth == 3) {
                text = text - strlen(token);
                if (Q_strncmp(text, token, strlen(token)) != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: internal error");
                }
                BG_ParseCommands(&text, item, data);
            } else {
                BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
            }
            break;
        case 3:
        case 4:
            if (Q_stricmp(token, "{") == 0) {
                if (2 < depth) {
                    BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
                }
                if (stack[depth] < 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
                }
                depth = depth + 1;
            } else if (Q_stricmp(token, "}") == 0) {
                depth = depth - 1;
                if (depth < 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
                }
                if (depth == 0) {
                    script = 0;
                }
                stack[depth] = -1;
            } else if (depth == 0 && stack[0] < 0) {
                if (section == 3) {
                    if (Q_stricmp(token, "statechange") != 0) {
                        BG_AnimParseError("BG_AnimParseAnimScript: expected 'statechange', got '%s'", token);
                    }
                    token = Com_ParseOnLine(&text);
                    if (token == 0) {
                        BG_AnimParseError("BG_AnimParseAnimScript: expected <state type>");
                    }
                    fromState = BG_IndexForString(token, g_animStateNames, 0);
                    token = Com_ParseOnLine(&text);
                    if (token == 0) {
                        BG_AnimParseError("BG_AnimParseAnimScript: expected <state type>");
                    }
                    stack[depth] = BG_IndexForString(token, g_animStateNames, 0);
                    script = &data->scriptStateChange[fromState][stack[depth]];
                    token = Com_Parse(&text);
                    if (token == 0 || Q_stricmp(token, "{") != 0) {
                        BG_AnimParseError("BG_AnimParseAnimScript: expected '{'");
                    }
                    depth = depth + 1;
                } else {
                    stack[0] = BG_IndexForString(token, g_animEventNames, 0);
                    script = &data->scriptEvents[stack[0]];
                    g_animParseEventIndex = stack[depth];
                }
                memset(script, 0, 0x204);
            } else if (depth == 1 && stack[1] < 0) {
                text = text - strlen(token);
                if (Q_strncmp(text, token, strlen(token)) != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: internal error");
                }
                memset(&itemBuffer, 0, 0xf4);
                stack[depth] = BG_ParseConditions(&text, &itemBuffer);
                if (0x7f < script->numItems) {
                    BG_AnimParseError("BG_AnimParseAnimScript: exceeded maximum items per script (%i)", 0x80);
                }
                if (0x7ff < data->numScriptItems) {
                    BG_AnimParseError("BG_AnimParseAnimScript: exceeded maximum global items (%i)", 0x800);
                }
                script->items[script->numItems] = &data->scriptItems[data->numScriptItems];
                data->numScriptItems = data->numScriptItems + 1;
                item = script->items[script->numItems];
                script->numItems = script->numItems + 1;
                src = (unsigned int *)&itemBuffer;
                dst = (unsigned int *)item;
                for (i = 0x3d; i != 0; i--) {
                    *dst = *src;
                    src = src + 1;
                    dst = dst + 1;
                }
            } else if (depth == 2) {
                text = text - strlen(token);
                if (Q_strncmp(text, token, strlen(token)) != 0) {
                    BG_AnimParseError("BG_AnimParseAnimScript: internal error");
                }
                BG_ParseCommands(&text, item, data);
            } else {
                BG_AnimParseError("BG_AnimParseAnimScript: unexpected '%s'", token);
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Condition evaluation and command execution                                */
/* ------------------------------------------------------------------------ */

// 0x080d8fae  size=185  FUN_080d8fae  status=translated  naming=inferred
// Returns 1 when every condition of the item matches the client's condition values: bit-flag
// conditions need a shared bit (low or high dword), value conditions need equality.
// Evidence: id Tech BG_EvaluateConditions counterpart; reads the g_animConditionDefs type per condition.
int BG_EvaluateConditions(clientInfo_s *client, animScriptItem_t *item)
{
    int i;
    animConditionEntry_t *condition;

    condition = item->conditions;
    i = 0;
    do {
        if (item->numConditions <= i) {
            return 1;
        }
        if (g_animConditionDefs[condition->index].type == 0) {
            if ((condition->value[0] & client->conditions[condition->index][0]) == 0 &&
                (condition->value[1] & client->conditions[condition->index][1]) == 0) {
                return 0;
            }
        } else if (g_animConditionDefs[condition->index].type == 1 &&
                   (int)client->conditions[condition->index][0] != (int)condition->value[0]) {
            return 0;
        }
        i = i + 1;
        condition = condition + 1;
    } while (true);
}

// 0x080d9068  size=110  FUN_080d9068  status=translated  naming=inferred
// Returns the first item of the script whose conditions match the client's current condition
// values (g_gameContextPtr client table), or 0.
// Evidence: id Tech BG_FirstValidItem counterpart.
animScriptItem_t *BG_FirstValidItem(int clientNum, animScript_t *script)
{
    int i;
    animScriptItem_t **itemPtr;

    i = 0;
    itemPtr = script->items;
    while (true) {
        if (script->numItems <= i) {
            return 0;
        }
        if (BG_EvaluateConditions(&g_animData->clients[clientNum], *itemPtr) != 0) {
            break;
        }
        i = i + 1;
        itemPtr = itemPtr + 1;
    }
    return *itemPtr;
}

// 0x080d90d6  size=458  FUN_080d90d6  status=translated  naming=inferred
// Starts an animation on legs (body part 1), torso (2) or both (3) of the player state. Duration is
// the animation duration + 50 ms unless forced. Returns the legs duration when the legs part was
// (re)started, otherwise -1. For 'both' the torso part is started with animation index 0 (quirk).
// Evidence: id Tech BG_PlayAnim counterpart (ps legs/torso anim + timer toggling bit 0x200).
int BG_PlayAnim(playerState_s *ps, unsigned int animIndex, int bodyPart, int forceDuration,
                int setTimer, int isContinue, int force)
{
    bool started;
    int duration;

    started = false;
    if (forceDuration == 0) {
        duration = g_curAnimScriptData->animations[animIndex].duration + 0x32;
    } else {
        duration = forceDuration;
    }
    if (bodyPart != 2) {
        if (bodyPart < 3) {
            if (bodyPart != 1) {
                goto finish;
            }
        } else if (bodyPart != 3) {
            goto finish;
        }
        if (ps->legsTimer < 0x32 || force != 0) {
            if (isContinue == 0 || (ps->legsAnim & 0xfffffdff) != animIndex) {
                started = true;
                ps->legsAnimDuration = duration;
                ps->legsAnim = (ps->legsAnim & 0x200 ^ 0x200) | animIndex;
                if (setTimer != 0) {
                    ps->legsTimer = duration;
                }
            } else if (setTimer != 0 && (g_curAnimScriptData->animations[animIndex].flags & 0x80) != 0) {
                ps->legsTimer = duration;
            }
        }
        if (bodyPart != 3) {
            goto finish;
        }
        animIndex = 0;
    }
    if (ps->torsoTimer < 0x32 || force != 0) {
        if (isContinue == 0 || (ps->torsoAnim & 0xfffffdff) != animIndex) {
            ps->torsoAnim = (ps->torsoAnim & 0x200 ^ 0x200) | animIndex;
            if (setTimer != 0) {
                ps->torsoTimer = duration;
            }
            ps->torsoAnimDuration = duration;
        } else if (setTimer != 0 && (g_curAnimScriptData->animations[animIndex].flags & 0x80) != 0) {
            ps->torsoTimer = duration;
        }
    }
finish:
    if (started) {
        return duration;
    }
    return -1;
}

// 0x080d92a0  size=72  FUN_080d92a0  status=translated  naming=inferred
// BG_PlayAnim by animation name (result discarded, forceDuration 0).
void BG_PlayAnimName(playerState_s *ps, const char *animName, int bodyPart, int setTimer,
                     int isContinue, int force)
{
    int animIndex;

    animIndex = BG_AnimationIndexForString(animName);
    BG_PlayAnim(ps, animIndex, bodyPart, 0, setTimer, isContinue, force);
}

// 0x080d92e8  size=446  FUN_080d92e8  status=translated  naming=inferred
// Executes one script command: plays each of its two parts with the part-0 duration + 50 ms (the
// second part also tests bodyPart[0], not bodyPart[1], when choosing the result path), then triggers
// the sound callback. Returns the duration when a legs/both part started, otherwise -1.
// Evidence: id Tech BG_ExecuteCommand counterpart; sound callback at animScriptData + 0xb3bc4.
int BG_ExecuteCommand(playerState_s *ps, animCommand_t *command, int setTimer, int isContinue,
                      int force)
{
    int duration;
    unsigned int result;
    unsigned int played;

    duration = -1;
    played = 0;
    if (command->bodyPart[0] != 0) {
        duration = command->animDuration[0] + 0x32;
        if (command->bodyPart[0] == 1 || command->bodyPart[0] == 3) {
            result = BG_PlayAnim(ps, command->animIndex[0], command->bodyPart[0], duration, setTimer,
                                 isContinue, force);
            played = ~result >> 0x1f;
        } else {
            BG_PlayAnim(ps, command->animIndex[0], command->bodyPart[0], duration, setTimer,
                        isContinue, force);
        }
    }
    if (command->bodyPart[1] != 0) {
        duration = command->animDuration[0] + 0x32;
        if (command->bodyPart[0] == 1 || command->bodyPart[0] == 3) {
            result = BG_PlayAnim(ps, command->animIndex[1], command->bodyPart[1], duration, setTimer,
                                 isContinue, force);
            played = ~result >> 0x1f;
        } else {
            BG_PlayAnim(ps, command->animIndex[1], command->bodyPart[1], duration, setTimer,
                        isContinue, force);
        }
    }
    if (command->soundIndex != 0) {
        g_curAnimScriptData->playSoundFn(ps->clientNum, command->soundIndex);
    }
    if (played == 0) {
        return -1;
    }
    return duration;
}

// 0x080d94a6  size=346  FUN_080d94a6  status=translated  naming=inferred
// Plays the movement animation for (state, moveType): walks down from the given state to the first
// state whose script has a matching item, sets condition 3 (MOVETYPE) and executes the command picked
// by clientNum modulo the command count. Returns 1/0 (started or not) or -1 when nothing applies.
// Evidence: id Tech BG_AnimScriptAnimation counterpart; scriptAnims[state][moveType] indexing.
unsigned int BG_AnimScriptAnimation(playerState_s *ps, int state, int moveType, int isContinue)
{
    animScript_t *script;
    animScriptItem_t *item;
    int result;

    item = 0;
    if (ps->pm_type < 6) {
        while (item == 0 && -1 < state) {
            script = &g_curAnimScriptData->scriptAnims[state][moveType];
            if (script->numItems == 0) {
                state = state - 1;
            } else {
                item = BG_FirstValidItem(ps->clientNum, script);
                if (item == 0) {
                    state = state - 1;
                }
            }
        }
        if (item == 0) {
            return 0xffffffff;
        } else if (item->numCommands == 0) {
            return 0xffffffff;
        } else {
            BG_UpdateConditionValue(ps->clientNum, 3, moveType, 1);
            result = BG_ExecuteCommand(ps, &item->commands[ps->clientNum % item->numCommands], 0,
                                       isContinue, 0);
            return (unsigned int)(result != -1);
        }
    } else {
        return 0xffffffff;
    }
}

// 0x080d9600  size=238  FUN_080d9600  status=translated  naming=inferred
// Runs the state-change script (fromState -> toState): first matching item, random command.
// Returns the command duration or -1.
// Evidence: id Tech BG_AnimScriptStateChange counterpart; scriptStateChange[from][to] indexing.
int BG_AnimScriptStateChange(playerState_s *ps, int toState, int fromState)
{
    animScript_t *script;
    animScriptItem_t *item;
    int r;

    if (ps->pm_type < 6) {
        script = &g_curAnimScriptData->scriptStateChange[fromState][toState];
        if (script->numItems == 0) {
            return -1;
        }
        item = BG_FirstValidItem(ps->clientNum, script);
        if (item == 0) {
            return -1;
        } else if (item->numCommands == 0) {
            return -1;
        } else {
            r = rand();
            return BG_ExecuteCommand(ps, &item->commands[r % item->numCommands], 1, 0, 0);
        }
    } else {
        return -1;
    }
}

// 0x080d96ee  size=224  FUN_080d96ee  status=translated  naming=inferred
// Runs the event script (pain, death, ...) for the player: first matching item, random command,
// timer set. Event 1 (DEATH) also runs for pm_type >= 6. Returns the duration or -1.
// Evidence: id Tech BG_AnimScriptEvent counterpart; scriptEvents[event]; 15 callers.
int BG_AnimScriptEvent(playerState_s *ps, int event, int isContinue, int force)
{
    animScript_t *script;
    animScriptItem_t *item;
    int r;

    if (event == 1 || ps->pm_type < 6) {
        script = &g_curAnimScriptData->scriptEvents[event];
        if (script->numItems == 0) {
            return -1;
        }
        item = BG_FirstValidItem(ps->clientNum, script);
        if (item == 0) {
            return -1;
        } else if (item->numCommands == 0) {
            return -1;
        } else {
            r = rand();
            return BG_ExecuteCommand(ps, &item->commands[r % item->numCommands], 1, isContinue, force);
        }
    } else {
        return -1;
    }
}

// 0x080d97ce  size=55  FUN_080d97ce  status=translated  naming=observed
// Returns animations[index] of the parse target; errors when the index is out of range.
// Evidence: string "BG_GetAnimString: anim index is out of range" at 0x08153100.
animation_t *BG_GetAnimString(int unused, unsigned int index)
{
    if ((unsigned int)g_curAnimScriptData->numAnimations <= index) {
        BG_AnimParseError("BG_GetAnimString: anim index is out of range");
    }
    return &g_curAnimScriptData->animations[index];
}

// 0x080d9806  size=158  FUN_080d9806  status=translated  naming=inferred
// Sets a client's animation condition: bit-flag conditions (type 0) with checkConversion get a single
// bit set in a cleared 64-bit mask, everything else stores the raw value in the low dword.
// Evidence: id Tech BG_UpdateConditionValue counterpart; table type at g_animConditionDefs[condition].
void BG_UpdateConditionValue(int clientNum, int condition, int value, int checkConversion)
{
    clientInfo_s *client;

    client = &g_animData->clients[clientNum];
    if (checkConversion == 0 || g_animConditionDefs[condition].type != 0) {
        client->conditions[condition][0] = value;
    } else {
        client->conditions[condition][0] = 0;
        client->conditions[condition][1] = 0;
        BitSet_Set(client->conditions[condition], value);
    }
}

// 0x080d98a4  size=124  FUN_080d98a4  status=translated  naming=inferred
// Reads a client's condition value; with checkConversion on a bit-flag condition it returns the index
// of the first set bit (0..63) instead, or 0 when no bit is set.
// Evidence: id Tech BG_GetConditionValue counterpart.
unsigned int BG_GetConditionValue(clientInfo_s *client, int condition, int checkConversion)
{
    unsigned int value;
    unsigned int bit;

    value = client->conditions[condition][0];
    if (checkConversion != 0 && g_animConditionDefs[condition].type == 0) {
        for (bit = 0; bit < 0x40; bit++) {
            if (BitSet_Test(client->conditions[condition], bit) != '\0') {
                return bit;
            }
        }
        value = 0;
    }
    return value;
}

// 0x080d9920  size=188  FUN_080d9920  status=translated  naming=inferred
// Like BG_AnimScriptEvent but only picks a random command of the first matching event item and
// returns its first animation index instead of executing it (-1 when none).
int BG_AnimScriptEventAnimIndex(playerState_s *ps, int event)
{
    animScript_t *script;
    animScriptItem_t *item;
    int r;

    if (event == 1 || ps->pm_type < 6) {
        script = &g_curAnimScriptData->scriptEvents[event];
        if (script->numItems == 0) {
            return -1;
        }
        item = BG_FirstValidItem(ps->clientNum, script);
        if (item == 0) {
            return -1;
        } else if (item->numCommands == 0) {
            return -1;
        } else {
            r = rand();
            return (int)item->commands[r % item->numCommands].animIndex[0];
        }
    } else {
        return -1;
    }
}

// 0x080d99dc  size=63  FUN_080d99dc  status=translated  naming=observed
// Returns animations[index] of the parse target (g_curAnimScriptData); raises an error when out of range.
// Evidence: string "BG_GetAnimationForIndex: index out of bounds" at 0x08153140.
animation_t *BG_GetAnimationForIndex(int unused, unsigned int index)
{
    if ((unsigned int)g_curAnimScriptData->numAnimations <= index) {
        Com_Error(1, "\025BG_GetAnimationForIndex: index out of bounds");
    }
    return &g_curAnimScriptData->animations[index];
}

// 0x080d9a1c  size=486  FUN_080d9a1c  status=translated  naming=inferred
// Refreshes the animation conditions from the pmove: 0 PLAYERANIMTYPE and 1 WEAPONCLASS from the
// current weapon, 7 WEAPON_POSITION (eFlags 0x40000), 2 MOUNTED (eFlags 0x300), 4 UNDERHAND
// (ps field 0xe8 > 0), 6 FIRING (button bit 0).
// Evidence: condition names at g_animConditionNames (index order); id Tech BG_UpdateConditionValue use.
void BG_UpdateConditionsFromPmove(pmove_s *pm)
{
    playerState_s *ps;
    weaponDef_s *weapon;
    int weaponIndex;

    ps = pm->ps;
    weaponIndex = BG_GetCurrentWeapon(ps);
    weapon = BG_GetWeaponDef(weaponIndex);
    BG_UpdateConditionValue(ps->clientNum, 0, weapon->playerAnimType, 1);
    BG_UpdateConditionValue(ps->clientNum, 1, weapon->weaponClass, 1);
    if ((ps->eFlags & 0x40000) == 0) {
        BG_UpdateConditionValue(ps->clientNum, 7, 0, 1);
    } else {
        BG_UpdateConditionValue(ps->clientNum, 7, 1, 1);
    }
    if ((ps->eFlags & 0x300) == 0) {
        BG_UpdateConditionValue(ps->clientNum, 2, 0, 1);
    } else {
        BG_UpdateConditionValue(ps->clientNum, 2, 1, 1);
    }
    BG_UpdateConditionValue(ps->clientNum, 4, 0.0f < ps->viewangles[0], 1);
    if ((pm->cmd.buttons & 1) == 0) {
        BG_UpdateConditionValue(ps->clientNum, 6, 0, 1);
    } else {
        BG_UpdateConditionValue(ps->clientNum, 6, 1, 1);
    }
}

// 0x080d9c02  size=86  FUN_080d9c02  status=translated  naming=unknown
// True when the animation's move-type mask intersects lo 0xc4 or hi 0x180 (animation number with the
// restart toggle bit 0x200 removed).
bool BG_AnimFlagsTestA(clientInfo_s *client, unsigned int animNumber)
{
    animation_t *anim;

    anim = BG_GetAnimationForIndex(client->entityNumber, animNumber & 0xfffffdff);
    return (anim->moveTypeBits[0] & 0xc4) != 0 || (anim->moveTypeBits[1] & 0x180) != 0;
}

// 0x080d9c58  size=83  FUN_080d9c58  status=translated  naming=unknown
// True when the animation's low move-type mask intersects 0x308.
bool BG_AnimFlagsTestB(clientInfo_s *client, unsigned int animNumber)
{
    animation_t *anim;

    anim = BG_GetAnimationForIndex(client->entityNumber, animNumber & 0xfffffdff);
    return (anim->moveTypeBits[0] & 0x308) != 0;
}

/* ------------------------------------------------------------------------ */
/* Client side lerp frame animation and body angles                          */
/* ------------------------------------------------------------------------ */

// 0x080d9d00  size=1834  FUN_080d9d00  status=translated  naming=inferred
// Switches a lerp frame (legs or torso of a client) to a new animation number: picks the blend time
// (170/250/120 ms defaults, never below the pending move-blend deadline), recomputes the run-cycle
// phase for moving animations and drives the animation tree (weights/times/roots). Legs updates do
// not touch the shared root animations; death animations (flag 0x40) must not loop.
// Evidence: string "death animation '%s' is looping" at 0x081531c0; id Tech CG_SetLerpFrameAnimation counterpart.
void BG_SetNewAnimation(clientInfo_s *client, playerAnimLerp_t *lerp, unsigned int newAnimation,
                        int isDeath)
{
    int blendTime;
    float phase;
    animation_t *oldAnim;
    unsigned int oldAnimation;
    bool isLegs;
    animation_t *anim;
    void *tree;
    void *globalTree;
    bool oldMoveA;
    bool newMoveA;
    bool oldMoveB;
    bool newMoveB;
    int remaining;
    unsigned int oldMasked;
    int cycleLength;
    bool resetTime;

    blendTime = -1;
    phase = 0.0f;
    oldAnim = lerp->animation;
    oldAnimation = lerp->animationNumber;
    isLegs = lerp == &client->legs;
    lerp->animationNumber = newAnimation;
    newAnimation = newAnimation & 0xfffffdff;
    if ((unsigned int)g_animData->numAnimations <= newAnimation) {
        Com_Error(1, "\025Player animation index out of range (%i): %i", g_animData->numAnimations,
                    newAnimation);
    }
    tree = client->animTree;
    globalTree = g_animData->animTree;
    if (newAnimation == 0) {
        anim = 0;
        lerp->animation = 0;
        lerp->animationTime = 200;
    } else {
        anim = &g_animData->animations[newAnimation];
        lerp->animation = anim;
        lerp->animationTime = anim->blendTime;
        newMoveA = BG_AnimFlagsTestA(client, newAnimation);
        oldMoveA = BG_AnimFlagsTestA(client, oldAnimation);
        newMoveB = BG_AnimFlagsTestB(client, newAnimation);
        oldMoveB = BG_AnimFlagsTestB(client, oldAnimation);
        if (isLegs && ((int)newMoveA != (int)oldMoveA || (int)newMoveB != (int)oldMoveB)) {
            client->moveBlendUntil = g_animData->time + 400;
        }
    }
    if (oldAnim == 0 && isLegs) {
        lerp->animationTime = 0;
    } else {
        if (anim == 0 || lerp->animationTime < 1) {
            if (anim == 0 || anim->moveSpeed == 0.0f) {
                if (oldAnim == 0 || oldAnim->moveSpeed == 0.0f) {
                    blendTime = 0xaa;
                } else {
                    blendTime = 0xfa;
                }
            } else {
                blendTime = 0x78;
            }
        }
        remaining = client->moveBlendUntil - g_animData->time;
        if (blendTime < remaining) {
            blendTime = remaining;
        }
        if (lerp->animationTime < blendTime) {
            lerp->animationTime = blendTime;
        }
    }
    if (anim != 0 && anim->moveSpeed != 0.0f && XAnim_NodeLoopFlag((struct XAnimNodeTable *)globalTree, newAnimation) != 0) {
        oldMasked = oldAnimation & 0xfffffdff;
        if (oldAnim == 0 || oldAnim->moveSpeed == 0.0f || XAnim_NodeLoopFlag((struct XAnimNodeTable *)globalTree, oldMasked) == 0) {
            if (!XAnim_NodeIsLeaf((struct XAnimNodeTable *)globalTree, oldMasked)) {
                cycleLength = 1000;
            } else {
                cycleLength = XAnim_NodeDurationMs((struct XAnimNodeTable *)globalTree, oldMasked) + 200;
            }
            phase = (float)client->entityNumber * 0.36f +
                    (float)(g_animData->time % cycleLength) / (float)cycleLength;
            phase = phase - (float)(int)phase;
        } else {
            phase = XAnim_NodeTime((struct XAnimTree *)tree, oldMasked);
        }
    }
    if (oldAnim != 0) {
        XAnim_SetSlotTime((struct XAnimTree *)tree, oldAnimation & 0xfffffdff, (float)lerp->animationTime * 0.001f);
    }
    if (newAnimation == 0) {
        if (!isLegs) {
            XAnim_BlendStepRec((struct XAnimTree *)tree, g_animData->rootAnimA, 0.0f, (float)lerp->animationTime * 0.001f, 1.0f,
                        0, 0, 0);
            XAnim_BlendStepRec((struct XAnimTree *)tree, g_animData->rootAnimB, 1.0f, (float)lerp->animationTime * 0.001f, 1.0f,
                        0, 0, 0);
        }
    } else {
        if (!isLegs) {
            client->flag3f4 = 0;
            client->flag3f8 = 1;
        }
        if ((anim->flags & 0x40) == 0) {
            resetTime = false;
            if (anim->moveSpeed != 0.0f && XAnim_NodeOldWeight((struct XAnimTree *)tree, newAnimation) == 0.0f) {
                resetTime = true;
            }
            XAnim_BlendStepRec((struct XAnimTree *)tree, newAnimation, 1.0f, (float)lerp->animationTime * 0.001f, 1.0f, 0,
                        anim->referenced, !isLegs);
            if (resetTime) {
                XAnim_ResetSlotTimes((struct XAnimTree *)tree, newAnimation, phase);
            }
        } else {
            if (XAnim_NodeLoopFlag((struct XAnimNodeTable *)globalTree, newAnimation) != 0) {
                Com_Error(1, "death animation '%s' is looping", anim);
            }
            if (isDeath == 0) {
                XAnim_BlendChain((struct XAnimTree *)tree, newAnimation, g_animData->deathRootAnim, 1.0f, 0, 1.0f, 0, 0);
                XAnim_ResetSlotTimes((struct XAnimTree *)tree, newAnimation, 1.0f);
            } else {
                XAnim_BlendStepRec((struct XAnimTree *)tree, newAnimation, 1.0f, (float)lerp->animationTime * 0.001f, 1.0f, 0, 0,
                            0);
            }
        }
        if (!isLegs) {
            XAnim_BlendStepRec((struct XAnimTree *)tree, g_animData->rootAnimA, 1.0f, (float)lerp->animationTime * 0.001f, 1.0f,
                        0, anim->referenced, 0);
            XAnim_BlendStepRec((struct XAnimTree *)tree, g_animData->rootAnimB, 0.01f, (float)lerp->animationTime * 0.001f, 1.0f,
                        0, anim->referenced, 0);
        }
    }
}

// 0x080da42a  size=843  FUN_080da42a  status=translated  naming=inferred
// Per frame update of a lerp frame: starts a new animation when the number changed, then estimates
// the animation speed scale from the distance moved since the last snapshot (vertical distance only
// for run-speed flagged animations), clamps it (0.1 minimum, move-speed dependent maximum) and applies
// it to the tree animation.
// Evidence: id Tech CG_RunLerpFrame counterpart; calls BG_SetNewAnimation with eFlags bit 19.
void BG_RunLerpFrame(clientInfo_s *client, playerAnimLerp_t *lerp, unsigned int newAnimation,
                     entityState_s *es)
{
    void *tree;
    animation_t *anim;
    bool verticalOnly;
    float distance;
    float fallback;

    verticalOnly = false;
    if (lerp->animation != 0 && (lerp->animation->flags & 2) != 0) {
        verticalOnly = true;
    }
    tree = client->animTree;
    if (newAnimation != lerp->animationNumber ||
        (lerp->animation == 0 && (newAnimation & 0xfffffdff) != 0)) {
        BG_SetNewAnimation(client, lerp, newAnimation, es->eFlags >> 0x13 & 1);
    }
    if ((newAnimation & 0xfffffdff) != 0) {
        anim = lerp->animation;
        if (anim->moveSpeed == 0.0f || lerp->oldFrameSnapshotTime == 0) {
            lerp->animSpeedScale = 1.0f;
            lerp->oldFrameSnapshotTime = g_animData->snapshotTime;
            Vec3_Copy_080dc44e(es->pos.trBase, lerp->oldFramePos);
        } else if (g_animData->snapshotTime != lerp->oldFrameSnapshotTime) {
            if (verticalOnly) {
                distance = Float_Abs_080dc354(lerp->oldFramePos[2] - es->pos.trBase[2]);
            } else {
                distance = Vec3_Distance(lerp->oldFramePos, es->pos.trBase);
            }
            lerp->animSpeedScale =
                (distance / ((float)(g_animData->snapshotTime - lerp->oldFrameSnapshotTime) * 0.001f)) /
                anim->moveSpeed;
            lerp->oldFrameSnapshotTime = g_animData->snapshotTime;
            Vec3_Copy_080dc44e(es->pos.trBase, lerp->oldFramePos);
            if (lerp->animSpeedScale < 0.1f) {
                if (0.01f <= lerp->animSpeedScale || !verticalOnly) {
                    lerp->animSpeedScale = 0.1f;
                } else {
                    lerp->animSpeedScale = 0.0f;
                }
            } else if (2.0f < lerp->animSpeedScale) {
                if ((anim->flags & 2) == 0) {
                    if (150.0f < anim->moveSpeed) {
                        lerp->animSpeedScale = 2.0f;
                    } else if (anim->moveSpeed < 20.0f) {
                        if (3.0f < lerp->animSpeedScale) {
                            lerp->animSpeedScale = 3.0f;
                        }
                    } else {
                        fallback = 3.0f - (anim->moveSpeed - 20.0f) / 130.0f;
                        if (fallback < lerp->animSpeedScale) {
                            lerp->animSpeedScale = fallback;
                        }
                    }
                } else if (4.0f < lerp->animSpeedScale) {
                    lerp->animSpeedScale = 4.0f;
                }
            }
        }
        if (lerp->animationNumber != 0) {
            XAnim_SetSlotValueAlias((struct XAnimTree *)tree, lerp->animationNumber & 0xfffffdff, *(uint32 *)&lerp->animSpeedScale);
        }
    }
}

// 0x080da776  size=102  FUN_080da776  status=translated  naming=unknown
// When the lerp frame's animation has finished (tree value 0), clears it and resets the blend time
// to 150 ms.
void BG_ClearFinishedAnimation(void *tree, playerAnimLerp_t *lerp)
{
    if (lerp->animationNumber != 0 &&
        XAnim_NodeOldWeight((struct XAnimTree *)tree, lerp->animationNumber & 0xfffffdff) == 0.0f) {
        lerp->animationNumber = 0;
        lerp->animation = 0;
        lerp->animationTime = 0x96;
    }
}

// 0x080da7dc  size=491  FUN_080da7dc  status=translated  naming=inferred
// Swings *angle toward the destination yaw/pitch: starts swinging when the difference exceeds the
// tolerance, moves by up to frameTime * max(0.5, |diff| * 0.05) * speed per call and clamps the final
// angle to within clampTolerance of the destination.
// Evidence: id Tech CG_SwingAngles counterpart (same parameter list and update rule).
void BG_SwingAngles(float destination, float swingTolerance, float clampTolerance, float speed,
                    float *angle, int *swinging)
{
    float swing;
    float move;
    float scale;
    float limit;

    if (*swinging == 0) {
        swing = AngleSubtract(*angle, destination);
        if (swingTolerance < swing || swing < -swingTolerance) {
            *swinging = 1;
        }
    }
    if (*swinging != 0) {
        move = AngleSubtract(destination, *angle);
        scale = Float_Abs_080dc354(move) * 0.05f;
        if (scale < 0.5f) {
            scale = 0.5f;
        }
        if (0.0f <= move) {
            speed = (float)g_animData->frameTime * scale * speed;
            if (move <= speed) {
                *swinging = 0;
            } else {
                *swinging = 1;
                move = speed;
            }
            *angle = AngleMod_Dup(*angle + move);
        } else if (move < 0.0f) {
            limit = -speed * (float)g_animData->frameTime * scale;
            if (limit <= move) {
                *swinging = 0;
            } else {
                *swinging = 1;
                move = limit;
            }
            *angle = AngleMod_Dup(*angle + move);
        }
        swing = AngleSubtract(destination, *angle);
        if (clampTolerance < swing) {
            *angle = AngleMod_Dup(destination - clampTolerance);
        } else if (swing < -clampTolerance) {
            *angle = AngleMod_Dup(destination + clampTolerance);
        }
    }
}

// 0x080da9c8  size=1502  FUN_080da9c8  status=translated  naming=inferred
// Computes the client's torso/legs yaw and torso pitch swing from the view angles and the entity's
// eFlags / MOVETYPE condition bits, calling BG_SwingAngles for torso yaw, legs yaw and torso pitch.
// Evidence: id Tech CG_PlayerAngles counterpart (legs/torso yaw swing, pitch * 0.6, 150 degree clamp).
void BG_PlayerAngles(entityState_s *es, clientInfo_s *client)
{
    float viewYawOffset;
    float viewAngles[3];
    float legsAngles[3];
    float torsoAngles[3];
    float torsoClamp;
    float pitch;
    unsigned int moveBits;
    int firing;

    Ease_OutQuadSigned(client->easeParam);
    viewYawOffset = client->viewYawOffset;
    Vec3_Copy_080dc44e(client->viewAngles, viewAngles);
    viewAngles[1] = AngleMod_Dup(viewAngles[1]);
    Vec3_Clear_080f29c0(legsAngles);
    Vec3_Clear_080f29c0(torsoAngles);
    if ((es->eFlags & 0x300) == 0) {
        moveBits = BG_GetConditionValue(client, 3, 0);
        if ((moveBits & 0xc0000) == 0) {
            if ((es->eFlags & 0x4000) == 0) {
                moveBits = BG_GetConditionValue(client, 3, 0);
                if ((moveBits & 6) == 0) {
                    client->torso.yawing = 1;
                    client->torso.pitching = 1;
                    client->legs.yawing = 1;
                } else {
                    firing = BG_GetConditionValue(client, 6, 1);
                    if (firing != 0) {
                        client->torso.yawing = 1;
                        client->torso.pitching = 1;
                    }
                }
            } else {
                client->torso.yawing = 1;
                client->torso.pitching = 1;
                client->legs.yawing = 1;
            }
        } else {
            client->torso.yawing = 1;
            client->torso.pitching = 1;
            client->legs.yawing = 1;
        }
    } else {
        client->torso.yawing = 1;
        client->torso.pitching = 1;
        client->legs.yawing = 1;
    }
    legsAngles[1] = viewAngles[1] + viewYawOffset;
    if ((es->eFlags & 0x20000) == 0) {
        moveBits = BG_GetConditionValue(client, 3, 0);
        if ((moveBits & 0xc0000) == 0) {
            if ((es->eFlags & 0x4000) == 0) {
                if ((es->eFlags & 8) == 0) {
                    if ((es->eFlags & 0x40) == 0) {
                        if ((es->eFlags & 0x40000) == 0) {
                            torsoAngles[1] = viewAngles[1] + viewYawOffset * 0.3f;
                        } else {
                            torsoAngles[1] = viewAngles[1];
                        }
                        torsoClamp = 90.0f;
                    } else {
                        torsoAngles[1] = viewAngles[1];
                        torsoClamp = 45.0f;
                    }
                } else {
                    torsoAngles[1] = viewAngles[1];
                    torsoClamp = 90.0f;
                }
            } else {
                torsoAngles[1] = viewAngles[1];
                legsAngles[1] = viewAngles[1];
                torsoAngles[0] = 90.0f;
                torsoClamp = 90.0f;
            }
        } else {
            torsoAngles[1] = legsAngles[1];
            torsoClamp = 0.0f;
        }
    } else {
        legsAngles[1] = viewAngles[1];
        torsoAngles[1] = viewAngles[1];
        torsoClamp = 90.0f;
    }
    BG_SwingAngles(torsoAngles[1], 0.0f, torsoClamp, dvar_bg_swingSpeed->current.value, &client->torso.yawAngle,
                   &client->torso.yawing);
    torsoClamp = 150.0f;
    if ((es->eFlags & 0x20000) == 0) {
        if ((es->eFlags & 8) == 0) {
            if ((g_animData->animations[es->legsAnim & 0xfffffdff].flags & 0x30) == 0) {
                if (client->legs.yawing == 0) {
                    BG_SwingAngles(legsAngles[1], 40.0f, 150.0f, dvar_bg_swingSpeed->current.value,
                                   &client->legs.yawAngle, &client->legs.yawing);
                } else {
                    BG_SwingAngles(legsAngles[1], 0.0f, 150.0f, dvar_bg_swingSpeed->current.value,
                                   &client->legs.yawAngle, &client->legs.yawing);
                }
            } else {
                client->legs.yawing = 0;
                legsAngles[1] = viewAngles[1];
                BG_SwingAngles(viewAngles[1], 0.0f, 150.0f, dvar_bg_swingSpeed->current.value, &client->legs.yawAngle,
                               &client->legs.yawing);
            }
        } else {
            client->legs.yawing = 0;
            client->legs.yawAngle = viewAngles[1] + viewYawOffset;
        }
    } else {
        BG_SwingAngles(legsAngles[1], 0.0f, 150.0f, dvar_bg_swingSpeed->current.value, &client->legs.yawAngle,
                       &client->legs.yawing);
    }
    if ((es->eFlags & 0x300) == 0) {
        moveBits = BG_GetConditionValue(client, 3, 0);
        if ((moveBits & 0xc0000) != 0) {
            client->torso.yawAngle = viewAngles[1] + viewYawOffset;
            client->legs.yawAngle = viewAngles[1] + viewYawOffset;
        }
    } else {
        client->torso.yawAngle = viewAngles[1];
        client->legs.yawAngle = viewAngles[1];
    }
    if ((es->eFlags & 0x20000) == 0) {
        if ((es->eFlags & 0x300) == 0) {
            moveBits = BG_GetConditionValue(client, 3, 0);
            if ((moveBits & 0xc0000) == 0) {
                if (es->eFlags == 0x4000) {
                    pitch = 0.0f;
                } else if (180.0f < viewAngles[0]) {
                    pitch = (viewAngles[0] - 360.0f) * 0.6f;
                } else {
                    pitch = viewAngles[0] * 0.6f;
                }
            } else {
                pitch = 0.0f;
            }
        } else {
            pitch = 0.0f;
        }
    } else {
        pitch = 0.0f;
    }
    BG_SwingAngles(pitch, 0.0f, 45.0f, 0.15f, &client->torso.pitchAngle, &client->torso.pitching);
}
