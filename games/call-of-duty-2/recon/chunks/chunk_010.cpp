#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_010_types.h"

/* Forward declarations (functions of this chunk). */

// 0x0806da84  size=285  FUN_0806da84  status=translated  naming=unknown
// looks the tree name up in scrAnimTreeRoot, or adds it; callers: Scr_UsingAnimTree (string "bad anim tree name"), 0x806dfca
unsigned int Scr_RegisterAnimTree(const char *name, int *outIndex, int group)
{
    unsigned int nameId;
    unsigned int entry;
    unsigned int treeVar;
    unsigned int nameVar;
    unsigned int result;
    struct XAnimNodeTable *xresult;
    int i;

    nameId = Scr_NormalizeFilenameToStringId(name);
    entry = ScrVar_FindVarValueId(scrAnimTreeRoot, nameId);
    if (entry == 0) {
        nameVar = ScrVar_NewChildAVarId(scrAnimTreeRoot, nameId);
        treeVar = Scr_GetOrCreateObjectId(nameVar);
        scrAnimTreeCount[group] = scrAnimTreeCount[group] + 1;
        scrAnimTreeNames[group][scrAnimTreeCount[group]] = (unsigned short)nameVar;
        *outIndex = scrAnimTreeCount[group];
    } else {
        treeVar = Scr_GetVarValue(entry);
        *outIndex = 0;
        for (i = 1; i <= scrAnimTreeCount[group]; i = i + 1) {
            if (scrAnimTreeNames[group][i] == entry) {
                *outIndex = i;
                break;
            }
        }
    }
    result = ScrVar_GetNewVarValueId(treeVar, 0);
    result = Scr_GetOrCreateArrayId(result);
    ScrStr_Release(nameId);
    return result;
}

// 0x0806dba2  size=76  FUN_0806dba2  status=translated  naming=inferred
// string "bad anim tree name"; sets scrUsingAnimTreeIndex which Emit_GetAnimTree reports as "#using_animtree was not specified"
void Scr_UsingAnimTree(const char *name, unsigned int sourcePos)
{
    if (Scr_IsIdentifier(name) == 0) {
        ScrCompile_ErrorInFile(sourcePos, "bad anim tree name");
    } else {
        g_08205648 = (void *)Scr_RegisterAnimTree(name, &scrUsingAnimTreeIndex, 1);
    }
}

// 0x0806dbee  size=193  FUN_0806dbee  status=translated  naming=inferred
// string "animtrees/%s.atr" passed to sprintf then opened
bool Scr_LoadAnimTreeFile(const char *name, unsigned int treeObject, unsigned int rootVar)
{
    char path[72];
    char *file;
    char *savedFileName;
    char *savedAux;

    sprintf(path, "animtrees/%s.atr", name);
    savedAux = scr_last_source_text;
    file = ScrDbg_LoadFrameFileOrSaved(0, path, 0, 1);
    savedFileName = g_scr_current_source_name;
    if (file == 0) {
        return false;
    }
    g_scr_current_source_name = path; /* original stores the stack buffer address */
    Scr_AnimTreeParse(file, (int)treeObject, (int)rootVar);
    g_scr_current_source_name = savedFileName;
    scr_last_source_text = savedAux;
    Hunk_ClearTempMemoryHigh();
    return Scr_GetArraySize(treeObject) != 0;
}

// 0x0806dcb0  size=614  FUN_0806dcb0  status=translated  naming=inferred
// string "unknown anim tree '%s'" raised via Com_Error when Scr_LoadAnimTreeFile fails; fills scrAnimTreeObjects[group][index]
void Scr_LoadAnimTree(int index, unsigned int arg, int group)
{
    unsigned short nameVar;
    unsigned short treeName;
    unsigned int treeObj;
    unsigned int entry;
    unsigned int rootObj;
    unsigned int rootStr;
    unsigned int result;
    struct XAnimNodeTable *xresult;
    unsigned int holder;
    struct VariableValue value;
    const char *s;

    nameVar = scrAnimTreeNames[group][index];
    treeName = Scr_GetVarNameId(nameVar);
    treeObj = Scr_GetVarValue(nameVar);
    entry = ScrVar_FindVarValueId(treeObj, 1);
    if (entry == 0) {
        entry = ScrVar_FindVarValueId(treeObj, 0);
        if (entry == 0) {
            scrAnimTreeObjects[group][index] = 0;
        } else {
            rootObj = Scr_GetVarValue(entry);
            scrAnimTreeScratchObject = ScrVar_AllocArray();
            s = ScrStr_TextFromHandle(treeName);
            if (Scr_LoadAnimTreeFile(s, scrAnimTreeScratchObject, rootObj) == 0) {
                s = ScrStr_TextFromHandle(treeName);
                s = va("unknown anim tree '%s'", s);
                Com_Error(1, s);
            }
            result = Anim_CountTreeEntries(scrAnimTreeScratchObject);
            s = ScrStr_TextFromHandle(treeName);
            xresult = XAnim_AllocNodeTable(s, (int)result, (void *(*)(int))arg);
            rootStr = ScrStr_InternCStringEx("root", 0, 4);
            Anim_SetNodeListValue(rootObj, 0, treeName, rootStr, index);
            ScrStr_Release(rootStr);
            Anim_ReleaseTreeEntries(scrAnimTreeScratchObject);
            Anim_HashAndEmitTree((int)scrAnimTreeScratchObject, (int)rootObj, (int)xresult, 1, (int)"root", 0, (int)treeName, index);
            Anim_ReportUnresolved(rootObj, treeName);
            ScrVar_RemoveVar(treeObj, 0);
            ScrVar_Release(scrAnimTreeScratchObject);
            scrAnimTreeScratchObject = 0;
            value.type = 7;
            value.u.intValue = (int)xresult;
            holder = ScrVar_GetNewVarValueId(treeObj, 1);
            ScrVar_SetVarValueReleasing(holder, &value);
            XAnim_ApplyLoopModeRoot(xresult);
            scrAnimTreeObjects[group][index] = (int)xresult;
        }
    }
}

// 0x0806df16  size=180  FUN_0806df16  status=translated  naming=unknown
// looks the name up in scrAnimTreeRoot and returns the value stored under child 1 (0 if absent); returned through hidden pointer (ret 4)
// note: the binary returns the 4-byte struct through a hidden pointer (callee pops it, ret 4); the explicit-parameter form below is the cross-chunk contract (JSON signature).
struct ScrAnimTreeRef *Scr_GetAnimTreeValue(struct ScrAnimTreeRef *ret, const char *name)
{
    unsigned int nameId;
    unsigned int entry;
    unsigned int child;
    struct VariableValue value;

    nameId = Scr_NormalizeFilenameToStringId(name);
    entry = ScrVar_FindVarValueId(scrAnimTreeRoot, nameId);
    ScrStr_Release(nameId);
    ret->value = 0;
    if (entry != 0) {
        nameId = Scr_GetVarNameId(entry) & 0xffff;
        entry = Scr_GetVarValue(entry);
        child = ScrVar_FindVarValueId(entry, 1);
        if (child != 0) {
            ScrVar_ReadVarValue(&value, child);
            ret->value = value.u.intValue;
        }
    }
    return ret;
}

// 0x0806dfca  size=104  FUN_0806dfca  status=translated  naming=unknown
// creates a string for rootName, registers the tree and passes both to Anim_LinkTreeEntry
void Scr_AddAnimTreeEntry(const char *name, const char *rootName, unsigned int obj, int group)
{
    unsigned int rootStr;
    int index;
    unsigned int tree;

    rootStr = ScrStr_InternLowerCStringEx(rootName, 0, 4);
    tree = Scr_RegisterAnimTree(name, &index, group);
    Anim_LinkTreeEntry((int *)obj, (int)rootStr, (int)tree); /* binary also stores a 4th (unread) 0 outgoing arg */
    ScrStr_Release(rootStr);
}

// 0x0806e034  size=30  FUN_0806e034  status=translated  naming=inferred
// releases a string reference unless scrCompileKeepStringRefs is set; used by every string-emitting path
void Compile_ReleaseStringRef(unsigned int stringId)
{
    if (scrCompileKeepStringRefs == 0) {
        ScrStr_Release(stringId);
    }
}

// 0x0806e052  size=92  FUN_0806e052  status=translated  naming=inferred
// allocates 2 code bytes and stores the canonical string handle (Scr_AssignSlot) unless compile mode is 2
void EmitCanonicalString(unsigned int stringId)
{
    unsigned short *dst;
    unsigned short handle;

    scrLastEmitPtr = (unsigned char *)TempMemory_ReserveAliasA(2);
    if (scrCompileMode == 2) {
        Compile_ReleaseStringRef(stringId);
    } else {
        if (scrCompileKeepStringRefs != 0) {
            ScrStr_AddRef(stringId);
        }
        dst = (unsigned short *)scrLastEmitPtr;
        handle = Scr_AssignSlot(stringId);
        *dst = handle;
    }
}

// 0x0806e0ae  size=45  FUN_0806e0ae  status=translated  naming=inferred
// EmitCanonicalString with scrCompileKeepStringRefs forced to 1 around the call
void EmitCanonicalStringKeepRef(unsigned int stringId)
{
    char saved;

    saved = scrCompileKeepStringRefs;
    scrCompileKeepStringRefs = 1;
    EmitCanonicalString(stringId);
    scrCompileKeepStringRefs = saved;
}

// 0x0806e0dc  size=68  FUN_0806e0dc  status=translated  naming=unknown
// tracks a string reference (ScrStr_SetFlagOrDecRef) unless mode 2; releases it in mode 2
void EmitStringRef(unsigned int stringId, int arg)
{
    if (scrCompileMode == 2) {
        Compile_ReleaseStringRef(stringId);
    } else {
        if (scrCompileKeepStringRefs != 0) {
            ScrStr_AddRef(stringId);
        }
        ScrStr_SetFlagOrDecRef(stringId, arg);
    }
}

// 0x0806e120  size=1006  FUN_0806e120  status=translated  naming=inferred
// single sink for every opcode byte: flushes the deferred value stack, tracks stack depth, rolls a code hash and peephole-merges with the previous opcode; same shape as the IW script compiler EmitOpcode
// note: peephole fuse table kept literally; opcode ids unnamed because only structure is observed
void EmitOpcode(int opcode, int stackDelta, int callKind)
{
    int pending;
    int i;
    unsigned char last;

    pending = scrValueStackCount;
    if (scrValueStackCount != 0) {
        scrValueStackCount = 0;
        for (i = 0; i < pending; i = i + 1) {
            FlushDeferredValue(&scrValueStack[i]);
        }
    }
    scrOpcodeAtStackBase = 0;
    if (scrCompileStackDepth == 0 || callKind == 2 || callKind == 3) {
        scrOpcodeAtStackBase = 1;
    }
    scrCompileStackDepth = scrCompileStackDepth + stackDelta;
    if (scrCompileMaxStackDepth < scrCompileStackDepth) {
        scrCompileMaxStackDepth = scrCompileStackDepth;
    }
    if (callKind != 0 && scrCompileMaxCallStackDepth < scrCompileStackDepth) {
        scrCompileMaxCallStackDepth = scrCompileStackDepth;
    }
    g_scr_code_hash = g_scr_code_hash * 0x1f + opcode;
    if (scrLastOpcodePtr != (unsigned char *)0) {
        scrLastEmitPtr = scrLastOpcodePtr;
        switch (opcode) {
        case 0x20:
            if (*scrLastOpcodePtr == 0x1e) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x1f;
                return;
            }
            last = *scrLastOpcodePtr;
            if ((unsigned int)last - 0x18 < 6) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x1f;
                EmitByte((unsigned char)((last - 0x18) & 0xff));
                return;
            }
            break;
        case 0x23:
            if (*scrLastOpcodePtr == 0x37) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x22;
                return;
            }
            if (*scrLastOpcodePtr == 0x36) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x21;
                return;
            }
            break;
        case 0x2a:
            if (*scrLastOpcodePtr == 0x26) {
                *scrLastOpcodePtr = 0x29;
                return;
            }
            if (*scrLastOpcodePtr == 0xd) {
                *scrLastOpcodePtr = 0x27;
                return;
            }
            if (*scrLastOpcodePtr == 0xe) {
                *scrLastOpcodePtr = 0x28;
                return;
            }
            break;
        case 0x2e:
            if (*scrLastOpcodePtr == 0x26) {
                *scrLastOpcodePtr = 0x2d;
                return;
            }
            if (*scrLastOpcodePtr == 0xd) {
                *scrLastOpcodePtr = 0x2b;
                return;
            }
            if (*scrLastOpcodePtr == 0xe) {
                *scrLastOpcodePtr = 0x2c;
                return;
            }
            break;
        case 0x31:
            if (*scrLastOpcodePtr == 0x16) {
                *scrLastOpcodePtr = 0x30;
                return;
            }
            break;
        case 0x39:
            if (*scrLastOpcodePtr == 0x37) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x3d;
                return;
            }
            if (*scrLastOpcodePtr == 0x36) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x3c;
                return;
            }
            if (*scrLastOpcodePtr == 0x2d) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x3b;
                return;
            }
            if (*scrLastOpcodePtr == 0x2b) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x38;
                return;
            }
            if (*scrLastOpcodePtr == 0x2c) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x3a;
                return;
            }
            break;
        case 0x50:
            if (*scrLastOpcodePtr == 0x4e) {
                *scrLastOpcodePtr = 0x4f;
                return;
            }
            break;
        case 0x52:
            if (*scrLastOpcodePtr == 0xf) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x50;
                if (*scrPrevOpcodePtr == 'N') {
                    TempMemory_SetPos((int)scrLastOpcodePtr);
                    scrLastOpcodePtr = scrLastOpcodePtr - 1;
                    scrPrevOpcodePtr = (unsigned char *)0;
                    scrLastEmitPtr = scrLastOpcodePtr;
                    *scrLastOpcodePtr = 0x4f;
                    return;
                }
                return;
            }
            break;
        case 0x56:
            if (*scrLastOpcodePtr == 0xf) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x54;
                return;
            }
            break;
        case 0x59:
            if (*scrLastOpcodePtr == 0x1e) {
                *scrLastOpcodePtr = 0x5a;
                return;
            }
            last = *scrLastOpcodePtr;
            if ((unsigned int)last - 0x18 < 6) {
                *scrLastOpcodePtr = 0x5a;
                EmitByte((unsigned char)((last - 0x18) & 0xff));
                return;
            }
            break;
        case 0x5e:
            if (*scrLastOpcodePtr == 0x5c) {
                ScrDbg_PopLine();
                *scrLastOpcodePtr = 0x5f;
                return;
            }
            break;
        }
    }
    scrPrevOpcodePtr = scrLastOpcodePtr;
    scrLastOpcodePtr = (unsigned char *)TempMemory_Reserve(1);
    scrLastEmitPtr = scrLastOpcodePtr;
    *scrLastOpcodePtr = (unsigned char)opcode;
}

// 0x0806e50e  size=36  FUN_0806e50e  status=translated  naming=unknown
// EmitOpcode(0,0,0)
void EmitEnd(void)
{
    EmitOpcode(0, 0, 0);
}

// 0x0806e532  size=36  FUN_0806e532  status=translated  naming=unknown
// EmitOpcode(1,-1,0)
void EmitReturn(void)
{
    EmitOpcode(1, -1, 0);
}

// 0x0806e556  size=36  FUN_0806e556  status=translated  naming=inferred
// allocates 4 code bytes (TempMemory_ReserveAliasA) and stores a dword
void EmitInt32(int value)
{
    scrLastEmitPtr = (unsigned char *)TempMemory_ReserveAliasA(4);
    *(int *)scrLastEmitPtr = value;
}

// 0x0806e57a  size=45  FUN_0806e57a  status=translated  naming=inferred
// allocates 2 code bytes and stores a word
void EmitShort(short value)
{
    scrLastEmitPtr = (unsigned char *)TempMemory_ReserveAliasA(2);
    *(short *)scrLastEmitPtr = value;
}

// 0x0806e5a8  size=45  FUN_0806e5a8  status=translated  naming=inferred
// same body as EmitShort; used for 16-bit magnitudes in EmitGetInteger
void EmitUnsignedShort(unsigned short value)
{
    scrLastEmitPtr = (unsigned char *)TempMemory_ReserveAliasA(2);
    *(unsigned short *)scrLastEmitPtr = value;
}

// 0x0806e5d6  size=43  FUN_0806e5d6  status=translated  naming=inferred
// allocates 1 code byte (TempMemory_Reserve) and stores it
void EmitByte(unsigned char value)
{
    scrLastEmitPtr = (unsigned char *)TempMemory_Reserve(1);
    *scrLastEmitPtr = value;
}

// 0x0806e602  size=36  FUN_0806e602  status=translated  naming=inferred
// allocates 4 bytes through TempMemory_ReserveAliasB (distinct allocator from EmitInt32); callers pass raw float words
void EmitFloat(int floatBits)
{
    scrLastEmitPtr = (unsigned char *)TempMemory_ReserveAliasB(4);
    *(int *)scrLastEmitPtr = floatBits;
}

// 0x0806e626  size=37  FUN_0806e626  status=translated  naming=inferred
// same body as EmitShort; emitted after the string opcodes 10/11
void EmitStringIndex(unsigned short value)
{
    scrLastEmitPtr = (unsigned char *)TempMemory_ReserveAliasA(2);
    *(unsigned short *)scrLastEmitPtr = value;
}

// 0x0806e64c  size=36  FUN_0806e64c  status=translated  naming=inferred
// same body as EmitInt32; used to emit function addresses in EmitFunctionRef
void EmitCodePos(int value)
{
    scrLastEmitPtr = (unsigned char *)TempMemory_ReserveAliasA(4);
    *(int *)scrLastEmitPtr = value;
}

// 0x0806e670  size=26  FUN_0806e670  status=translated  naming=inferred
// fills a deferred-value entry with type 0; counterpart of EmitGetUndefined in FlushDeferredValue
void ScrDeferred_SetUndefined(unsigned int sourcePos, struct ScriptValue *entry)
{
    entry->type = 0;
    entry->sourcePos = sourcePos;
}

// 0x0806e68a  size=55  FUN_0806e68a  status=translated  naming=inferred
// EmitOpcode(2,1,0) + source pos; dispatched from FlushDeferredValue case 0
void EmitGetUndefined(unsigned int sourcePos)
{
    EmitOpcode(2, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806e6c2  size=34  FUN_0806e6c2  status=translated  naming=inferred
// fills a deferred-value entry with type 6
void ScrDeferred_SetInteger(int value, unsigned int sourcePos, struct ScriptValue *entry)
{
    entry->type = 6;
    entry->value = value;
    entry->sourcePos = sourcePos;
}

// 0x0806e6e4  size=436  FUN_0806e6e4  status=translated  naming=inferred
// picks the narrowest of zero/byte/neg byte/word/neg word/dword encodings (opcodes 3..8)
void EmitGetInteger(int value, unsigned int sourcePos)
{
    if (value < 0) {
        if (-0x100 < value) {
            EmitOpcode(5, 1, 0);
            ScrDbg_PushLine(sourcePos, 1);
            EmitByte((unsigned char)(-(char)value));
            return;
        }
        if (-0x10000 < value) {
            EmitOpcode(7, 1, 0);
            ScrDbg_PushLine(sourcePos, 1);
            EmitUnsignedShort((unsigned short)(-(unsigned int)value & 0xffff));
            return;
        }
    } else {
        if (value == 0) {
            EmitOpcode(3, 1, 0);
            ScrDbg_PushLine(sourcePos, 1);
            return;
        }
        if (value < 0x100) {
            EmitOpcode(4, 1, 0);
            ScrDbg_PushLine(sourcePos, 1);
            EmitByte((unsigned char)(value & 0xff));
            return;
        }
        if (value < 0x10000) {
            EmitOpcode(6, 1, 0);
            ScrDbg_PushLine(sourcePos, 1);
            EmitUnsignedShort((unsigned short)(value & 0xffff));
            return;
        }
    }
    EmitOpcode(8, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
    EmitInt32(value);
}

// 0x0806e898  size=34  FUN_0806e898  status=translated  naming=inferred
// fills a deferred-value entry with type 5
void ScrDeferred_SetFloat(int floatBits, unsigned int sourcePos, struct ScriptValue *entry)
{
    entry->type = 5;
    entry->value = floatBits;
    entry->sourcePos = sourcePos;
}

// 0x0806e8ba  size=66  FUN_0806e8ba  status=translated  naming=inferred
// EmitOpcode(9,1,0), pos, EmitFloat
void EmitGetFloat(int floatBits, unsigned int sourcePos)
{
    EmitOpcode(9, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
    EmitFloat(floatBits);
}

// 0x0806e8fc  size=58  FUN_0806e8fc  status=translated  naming=observed
// string "#using_animtree was not specified"; otherwise emits the registered tree index as an integer
void EmitGetAnimTree(unsigned int sourcePos)
{
    if (scrUsingAnimTreeIndex == 0) {
        ScrCompile_ErrorInFile(sourcePos, "#using_animtree was not specified");
    } else {
        EmitGetInteger(scrUsingAnimTreeIndex, sourcePos);
    }
}

// 0x0806e936  size=55  FUN_0806e936  status=translated  naming=unknown
// EmitOpcode(0x39,-1,0) + pos(0); 0x39 is a fusion target in EmitOpcode
void EmitSetVariableField(unsigned int sourcePos)
{
    EmitOpcode(0x39, -1, 0);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806e96e  size=67  FUN_0806e96e  status=translated  naming=unknown
// linear search of block->vars[start..varCount) for stringId; -1 if absent
int ScrLocalBlock_FindVar(struct ScriptScope *block, int start, int stringId)
{
    int i;

    i = start;
    while (true) {
        if (block->varCount <= i) {
            return -1;
        }
        if (block->vars[i] == stringId) {
            break;
        }
        i = i + 1;
    }
    return i;
}

// 0x0806e9b2  size=351  FUN_0806e9b2  status=translated  naming=observed
// strings "uninitialised variable '%s'" and "unreachable code"; returns stack offset of the local from the top
// note: the in-loop break and the post-loop test are the same condition; kept as in the binary
int Compile_LookupLocalVar(int stringId, unsigned int sourcePos, char markInitialised, struct ScriptScope *block)
{
    unsigned char bit;
    int byteIndex;
    const char *name;
    int i;

    if (block != (struct ScriptScope *)0) {
        for (i = 0; i < block->varCount; i = i + 1) {
            if (i == block->emittedCount) {
                block->emittedCount = block->emittedCount + 1;
                EmitOpcode(0x16, 0, 0);
                EmitCanonicalStringKeepRef(block->vars[i]);
            }
            if (block->vars[i] == stringId) {
                Compile_ReleaseStringRef(stringId);
                bit = (unsigned char)(1 << ((unsigned char)i & 7));
                byteIndex = i >> 3;
                if ((bit & block->initBits[byteIndex]) == 0) {
                    if (markInitialised == 0 || scrCompileUninitIsError != 0) {
                        break;
                    }
                    block->initBits[byteIndex] = bit | block->initBits[byteIndex];
                }
                return (block->emittedCount - i) + -1;
            }
        }
        if (markInitialised == 0 || scrCompileUninitIsError != 0) {
            name = ScrStr_TextFromHandle(stringId);
            ScrCompile_ErrorInFile(sourcePos, "uninitialised variable '%s'", name);
            return 0;
        }
    }
    ScrCompile_ErrorInFile(sourcePos, "unreachable code");
    return 0;
}

// 0x0806eb12  size=111  FUN_0806eb12  status=translated  naming=unknown
// emits opcode 0x16 + name for every var in [emittedCount, targetCount) then sets emittedCount = targetCount
void ScrLocalBlock_EmitPendingVars(struct ScriptScope *block)
{
    int i;

    if (block->emittedCount != block->targetCount) {
        for (i = block->emittedCount; i < block->targetCount; i = i + 1) {
            EmitOpcode(0x16, 0, 0);
            EmitCanonicalStringKeepRef(block->vars[i]);
        }
        block->emittedCount = block->targetCount;
    }
}

// 0x0806eb82  size=95  FUN_0806eb82  status=translated  naming=unknown
// emits opcode 0x17 with the byte count of locals above target->targetCount, then block->emittedCount = block->targetCount (only when block->inactive==0)
void EmitRemoveLocalVars(struct ScriptScope *block, struct ScriptScope *target)
{
    unsigned int diff;

    if (block->inactive == 0) {
        diff = block->emittedCount - target->targetCount;
        if (diff != 0) {
            EmitOpcode(0x17, 0, 0);
            EmitByte((unsigned char)(diff & 0xff));
            block->emittedCount = block->targetCount;
        }
    }
}

// 0x0806ebe2  size=81  FUN_0806ebe2  status=translated  naming=unknown
// emits either the local cleanup or opcode 0 + pos; restores g_scr_code_hash to saved+1
void EmitFunctionEpilogue(char emitEnd, unsigned int sourcePos, struct ScriptScope *block)
{
    int savedHash;

    savedHash = g_scr_code_hash;
    if (emitEnd == 0) {
        EmitRemoveLocalVars(block, block);
    } else {
        EmitEnd();
        ScrDbg_PushLine(sourcePos, 1);
    }
    g_scr_code_hash = savedHash + 1;
}

// 0x0806ec34  size=39  FUN_0806ec34  status=translated  naming=observed
// string "MAX_SWITCH_CASES exceeded"
void Compile_CheckSwitchCaseCount(int count)
{
    if (0x3ff < count) {
        Com_Error(1, "MAX_SWITCH_CASES exceeded");
    }
}

// 0x0806ec5c  size=36  FUN_0806ec5c  status=translated  naming=observed
// string "LOCAL_VAR_STACK_SIZE exceeded"
void Compile_CheckLocalVarCount(int count)
{
    if (0x3f < count) {
        Com_Error(1, "LOCAL_VAR_STACK_SIZE exceeded");
    }
}

// 0x0806ec80  size=98  FUN_0806ec80  status=translated  naming=unknown
// appends stringId to block->vars if not present and block is active (inactive==0); checks LOCAL_VAR_STACK_SIZE
// note: sourcePos unused
void ScrLocalBlock_AddVar(int stringId, unsigned int sourcePos, struct ScriptScope *block)
{
    int i;

    if (block->inactive == 0) {
        for (i = 0; i < block->varCount; i = i + 1) {
            if (block->vars[i] == stringId) {
                return;
            }
        }
        Compile_CheckLocalVarCount(block->varCount);
        block->vars[block->varCount] = stringId;
        block->varCount = block->varCount + 1;
    }
}

// 0x0806ece2  size=122  FUN_0806ece2  status=translated  naming=unknown
// allocates a 0x118-byte block when *dst is null (Hunk_AllocateTempMemoryHigh) and copies src into it, then clears targetCount
// note: original uses an inlined dword copy loop (rep movs equivalent), memcpy is behaviourally identical
void ScrLocalBlock_Copy(struct ScriptScope *src, struct ScriptScope **dst)
{
    if (*dst == (struct ScriptScope *)0) {
        *dst = (struct ScriptScope *)Hunk_AllocateTempMemoryHigh(0x118);
    }
    memcpy(*dst, src, 0x118);
    (*dst)->targetCount = 0;
}

// 0x0806ed5c  size=306  FUN_0806ed5c  status=translated  naming=unknown
// dst->emittedCount = min(blocks[i]->targetCount); initBits[i] set only if set in every block
void ScrLocalBlock_IntersectInit(struct ScriptScope **blocks, int count, struct ScriptScope *dst)
{
    unsigned char bit;
    int byteIndex;
    int minCount;
    int i;
    int b;

    if (count != 0) {
        minCount = blocks[0]->targetCount;
        for (b = 1; b < count; b = b + 1) {
            if (blocks[b]->targetCount < minCount) {
                minCount = blocks[b]->targetCount;
            }
        }
        dst->emittedCount = minCount;
        for (i = 0; i < minCount; i = i + 1) {
            byteIndex = i >> 3;
            bit = (unsigned char)i;
            if ((((int)(unsigned int)dst->initBits[byteIndex] >> (bit & 7)) & 1U) == 0) {
                for (b = 0; b < count; b = b + 1) {
                    if ((((unsigned char)((int)(unsigned int)blocks[b]->initBits[byteIndex] >> (bit & 7)) ^ 1) & 1) != 0) {
                        goto next_var;
                    }
                }
                dst->initBits[byteIndex] = (unsigned char)(1 << (bit & 7)) | dst->initBits[byteIndex];
            }
next_var:
            ;
        }
    }
}

// 0x0806ee8e  size=263  FUN_0806ee8e  status=translated  naming=unknown
// resets each block->inactive, then appends to merged every variable of blocks[0] present in all other blocks
void ScrLocalBlock_MergeVars(struct ScriptScope **blocks, int count, struct ScriptScope *merged)
{
    int id;
    int idx;
    int b;
    int v;

    if (count != 0 && merged->inactive == 0) {
        for (b = 0; b < count; b = b + 1) {
            blocks[b]->inactive = 0;
        }
        for (v = 0; v < blocks[0]->varCount; v = v + 1) {
            id = blocks[0]->vars[v];
            idx = ScrLocalBlock_FindVar(merged, 0, id);
            if (idx < 0) {
                for (b = 1; b < count; b = b + 1) {
                    idx = ScrLocalBlock_FindVar(blocks[b], 0, id);
                    if (idx < 0) {
                        goto next_var;
                    }
                }
                merged->vars[merged->varCount] = id;
                merged->varCount = merged->varCount + 1;
            }
next_var:
            ;
        }
    }
}

// 0x0806ef96  size=267  FUN_0806ef96  status=translated  naming=unknown
// for every block sets targetCount = merged->varCount and reorders the block var list so merged vars come first in merged order
void ScrLocalBlock_ApplyMerged(struct ScriptScope **blocks, int count, struct ScriptScope *merged)
{
    struct ScriptScope *blk;
    int id;
    int idx;
    int b;
    int v;

    if (count != 0 && merged->inactive == 0) {
        for (b = 0; b < count; b = b + 1) {
            blk = blocks[b];
            blk->targetCount = merged->varCount;
            for (v = 0; v < merged->varCount; v = v + 1) {
                id = merged->vars[v];
                idx = ScrLocalBlock_FindVar(blk, v, id);
                if (idx < 0) {
                    idx = blk->varCount;
                    Compile_CheckLocalVarCount(blk->varCount);
                    blk->varCount = blk->varCount + 1;
                }
                for (; v < idx; idx = idx + -1) {
                    blk->vars[idx] = blk->vars[idx - 1];
                }
                blk->vars[v] = id;
            }
        }
    }
}

// 0x0806f0a2  size=314  FUN_0806f0a2  status=translated  naming=unknown
// merges src vars/init bits into dst ordering, sets dst->emittedCount = src->emittedCount and dst->inactive = 0
void ScrLocalBlock_RebaseInto(struct ScriptScope *src, struct ScriptScope *dst)
{
    int id;
    int idx;
    int byteIndex;
    int v;

    for (v = 0; v < dst->targetCount || v < src->emittedCount; v = v + 1) {
        id = src->vars[v];
        idx = ScrLocalBlock_FindVar(dst, v, id);
        if (idx < 0) {
            idx = dst->varCount;
            Compile_CheckLocalVarCount(dst->varCount);
            dst->varCount = dst->varCount + 1;
        }
        if (dst->targetCount <= idx) {
            dst->targetCount = dst->targetCount + 1;
        }
        for (; v < idx; idx = idx + -1) {
            dst->vars[idx] = dst->vars[idx - 1];
        }
        dst->vars[v] = id;
        byteIndex = v >> 3;
        if ((((int)(unsigned int)src->initBits[byteIndex] >> ((unsigned char)v & 7)) & 1U) != 0) {
            dst->initBits[byteIndex] = (unsigned char)(1 << ((unsigned char)v & 7)) | dst->initBits[byteIndex];
        }
    }
    dst->emittedCount = src->emittedCount;
    dst->inactive = 0;
}

// 0x0806f1dc  size=130  FUN_0806f1dc  status=translated  naming=inferred
// opcode 0x31 (offset 0) / 0x32 + byte offset, offset from Compile_LookupLocalVar; 0x31 is fused with a preceding 0x16 in EmitOpcode
void EmitSafeSetVariableFieldCached(int stringId, unsigned int sourcePos, struct ScriptScope *block)
{
    unsigned int offset;
    int opcode;

    offset = Compile_LookupLocalVar(stringId, sourcePos, 1, block);
    if (offset == 0) {
        opcode = 0x31;
    } else {
        opcode = 0x32;
    }
    EmitOpcode(opcode, 0, 0);
    if (offset != 0) {
        EmitByte((unsigned char)(offset & 0xff));
    }
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806f25e  size=33  FUN_0806f25e  status=translated  naming=unknown
// tail-forwards to ScrLocalBlock_AddVar
void ScrLocalBlock_AddVarThunk1(int stringId, unsigned int sourcePos, struct ScriptScope *block)
{
    ScrLocalBlock_AddVar(stringId, sourcePos, block);
}

// 0x0806f280  size=103  FUN_0806f280  status=translated  naming=unknown
// opcode 0x33 + byte offset from Compile_LookupLocalVar
void EmitSafeSetWaittillVariableFieldCached(int stringId, unsigned int sourcePos, struct ScriptScope *block)
{
    unsigned char offset;

    offset = (unsigned char)Compile_LookupLocalVar(stringId, sourcePos, 1, block);
    EmitOpcode(0x33, 0, 0);
    EmitByte(offset);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806f2e8  size=34  FUN_0806f2e8  status=translated  naming=inferred
// fills a deferred entry with type 2
void ScrDeferred_SetString(unsigned int stringId, unsigned int sourcePos, struct ScriptValue *entry)
{
    entry->type = 2;
    entry->value = (int)stringId;
    entry->sourcePos = sourcePos;
}

// 0x0806f30a  size=85  FUN_0806f30a  status=translated  naming=inferred
// opcode 10 + string index word + string ref tracking; dispatched from FlushDeferredValue case 2
void EmitGetString(unsigned int stringId, unsigned int sourcePos)
{
    EmitOpcode(10, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
    EmitStringIndex((unsigned short)stringId);
    EmitStringRef(stringId, 1);
}

// 0x0806f360  size=34  FUN_0806f360  status=translated  naming=inferred
// fills a deferred entry with type 3
void ScrDeferred_SetIString(unsigned int stringId, unsigned int sourcePos, struct ScriptValue *entry)
{
    entry->type = 3;
    entry->value = (int)stringId;
    entry->sourcePos = sourcePos;
}

// 0x0806f382  size=85  FUN_0806f382  status=translated  naming=inferred
// opcode 11, same layout as EmitGetString; dispatched from FlushDeferredValue case 3
void EmitGetIString(unsigned int stringId, unsigned int sourcePos)
{
    EmitOpcode(11, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
    EmitStringIndex((unsigned short)stringId);
    EmitStringRef(stringId, 1);
}

// 0x0806f3d8  size=112  FUN_0806f3d8  status=translated  naming=inferred
// opcode 12 + three float words, then frees the temporary vector (Scr_VectorRelease); dispatched from FlushDeferredValue case 4
void EmitGetVector(int *vec3Bits, unsigned int sourcePos)
{
    int i;

    EmitOpcode(12, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
    for (i = 0; i < 3; i = i + 1) {
        EmitFloat(vec3Bits[i]);
    }
    Scr_VectorRelease((char *)vec3Bits);
}

// 0x0806f448  size=198  FUN_0806f448  status=translated  naming=inferred
// switch on entry->type to the EmitGet* functions; called from EmitOpcode for each pending value-stack entry
void FlushDeferredValue(struct ScriptValue *entry)
{
    switch (entry->type) {
    case 0:
        EmitGetUndefined(entry->sourcePos);
        break;
    case 2:
        EmitGetString(entry->value, entry->sourcePos);
        break;
    case 3:
        EmitGetIString(entry->value, entry->sourcePos);
        break;
    case 4:
        EmitGetVector((int *)entry->value, entry->sourcePos);
        break;
    case 5:
        EmitGetFloat(entry->value, entry->sourcePos);
        break;
    case 6:
        EmitGetInteger(entry->value, entry->sourcePos);
    }
}

// 0x0806f50e  size=95  FUN_0806f50e  status=translated  naming=observed
// string "VALUE_STACK_SIZE exceeded"
void PushDeferredValue(const struct ScriptValue *entry)
{
    if (scrValueStackCount < SCR_VALUE_STACK_SIZE) {
        scrValueStack[scrValueStackCount].value = entry->value;
        scrValueStack[scrValueStackCount].type = entry->type;
        scrValueStack[scrValueStackCount].sourcePos = entry->sourcePos;
        scrValueStackCount = scrValueStackCount + 1;
    } else {
        ScrCompile_ErrorInFile(entry->sourcePos, "VALUE_STACK_SIZE exceeded");
    }
}

// 0x0806f56e  size=11  FUN_0806f56e  status=translated  naming=inferred
// decrements scrValueStackCount
void PopDeferredValue(void)
{
    scrValueStackCount = scrValueStackCount + -1;
}

// 0x0806f57a  size=55  FUN_0806f57a  status=translated  naming=unknown
// EmitOpcode(0x5b,0,0) + source pos(flag 0); name from opcode id
void EmitCastBool(unsigned int sourcePos)
{
    EmitOpcode(0x5b, 0, 0);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806f5b2  size=55  FUN_0806f5b2  status=translated  naming=unknown
// EmitOpcode(0x5c,0,0) + source pos(flag 0); name from opcode id
void EmitBoolNot(unsigned int sourcePos)
{
    EmitOpcode(0x5c, 0, 0);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806f5ea  size=55  FUN_0806f5ea  status=translated  naming=unknown
// EmitOpcode(0x5d,0,0) + source pos(flag 0); name from opcode id
void EmitBoolComplement(unsigned int sourcePos)
{
    EmitOpcode(0x5d, 0, 0);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806f622  size=73  FUN_0806f622  status=translated  naming=unknown
// compiles one expression (ScrCompile_PrimaryExprPush) then EmitOpcode(0x76,0,0)
void EmitSize(unsigned int exprNode, unsigned int sourcePos, struct ScriptScope *scope)
{
    ScrCompile_PrimaryExprPush((ScriptNode *)exprNode, scope);
    EmitOpcode(0x76, 0, 0);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806f66c  size=55  FUN_0806f66c  status=translated  naming=unknown
// EmitOpcode(0xf,1,0) + source pos(flag 1); name from opcode id
void EmitGetSelf(unsigned int sourcePos)
{
    EmitOpcode(0xf, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f6a4  size=55  FUN_0806f6a4  status=translated  naming=unknown
// EmitOpcode(0x10,1,0) + source pos(flag 1); name from opcode id
void EmitGetLevel(unsigned int sourcePos)
{
    EmitOpcode(0x10, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f6dc  size=55  FUN_0806f6dc  status=translated  naming=unknown
// EmitOpcode(0x11,1,0) + source pos(flag 1); name from opcode id
void EmitGetGame(unsigned int sourcePos)
{
    EmitOpcode(0x11, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f714  size=55  FUN_0806f714  status=translated  naming=unknown
// EmitOpcode(0x12,1,0) + source pos(flag 1); name from opcode id
void EmitGetAnim(unsigned int sourcePos)
{
    EmitOpcode(0x12, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f74c  size=55  FUN_0806f74c  status=translated  naming=unknown
// EmitOpcode(0x26,0,0) + source pos(flag 1); name from opcode id
void EmitGetSelfObject(unsigned int sourcePos)
{
    EmitOpcode(0x26, 0, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f784  size=55  FUN_0806f784  status=translated  naming=unknown
// EmitOpcode(0xd,0,0) + source pos(flag 1); name from opcode id
void EmitGetLevelObject(unsigned int sourcePos)
{
    EmitOpcode(0xd, 0, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f7bc  size=55  FUN_0806f7bc  status=translated  naming=unknown
// EmitOpcode(0xe,0,0) + source pos(flag 1); name from opcode id
void EmitGetAnimObject(unsigned int sourcePos)
{
    EmitOpcode(0xe, 0, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f7f4  size=132  FUN_0806f7f4  status=translated  naming=inferred
// offsets 0..5 map to opcodes 0x18..0x1d, otherwise opcode 0x1e + byte offset; fusion targets in EmitOpcode (cases 0x20, 0x59)
void EmitEvalLocalVariableCached(int stringId, unsigned int sourcePos, struct ScriptScope *block)
{
    unsigned int offset;
    int opcode;

    offset = Compile_LookupLocalVar(stringId, sourcePos, 0, block);
    if ((int)offset < 6) {
        opcode = offset + 0x18;
    } else {
        opcode = 0x1e;
    }
    EmitOpcode(opcode, 1, 0);
    if (opcode == 0x1e) {
        EmitByte((unsigned char)(offset & 0xff));
    }
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f878  size=130  FUN_0806f878  status=translated  naming=inferred
// opcode 0x36 (offset 0) / 0x37 + byte offset; fusion targets of opcodes 0x23/0x39 in EmitOpcode
void EmitEvalLocalVariableRefCached(int stringId, unsigned int sourcePos, struct ScriptScope *block)
{
    unsigned int offset;
    int opcode;

    offset = Compile_LookupLocalVar(stringId, sourcePos, 1, block);
    if (offset == 0) {
        opcode = 0x36;
    } else {
        opcode = 0x37;
    }
    EmitOpcode(opcode, 0, 0);
    if (offset != 0) {
        EmitByte((unsigned char)(offset & 0xff));
    }
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f8fa  size=33  FUN_0806f8fa  status=translated  naming=unknown
// tail-forwards to ScrLocalBlock_AddVar (second identical wrapper)
void ScrLocalBlock_AddVarThunk2(int stringId, unsigned int sourcePos, struct ScriptScope *block)
{
    ScrLocalBlock_AddVar(stringId, sourcePos, block);
}

// 0x0806f91c  size=55  FUN_0806f91c  status=translated  naming=unknown
// EmitOpcode(0x14,0,0) + source pos(flag 1); name from opcode id
void EmitGetGameRef(unsigned int sourcePos)
{
    EmitOpcode(0x14, 0, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806f954  size=74  FUN_0806f954  status=translated  naming=unknown
// EmitOpcode(0x20,-1,0), pos(B,0), pos(A,1); 0x20 is fused with preceding local eval opcodes
void EmitEvalArray(unsigned int srcPosA, unsigned int srcPosB)
{
    EmitOpcode(0x20, -1, 0);
    ScrDbg_PushLine(srcPosB, 0);
    ScrDbg_PushLine(srcPosA, 1);
}

// 0x0806f99e  size=74  FUN_0806f99e  status=translated  naming=unknown
// EmitOpcode(0x23,-1,0), pos(B,0), pos(A,1)
void EmitEvalArrayRef(unsigned int srcPosA, unsigned int srcPosB)
{
    EmitOpcode(0x23, -1, 0);
    ScrDbg_PushLine(srcPosB, 0);
    ScrDbg_PushLine(srcPosA, 1);
}

// 0x0806f9e8  size=74  FUN_0806f9e8  status=translated  naming=unknown
// EmitOpcode(0x24,-1,0), pos(B,0), pos(A,0)
void EmitClearArray(unsigned int srcPosA, unsigned int srcPosB)
{
    EmitOpcode(0x24, -1, 0);
    ScrDbg_PushLine(srcPosB, 0);
    ScrDbg_PushLine(srcPosA, 0);
}

// 0x0806fa32  size=55  FUN_0806fa32  status=translated  naming=unknown
// EmitOpcode(0x25,1,0) + source pos(flag 1); name from opcode id
void EmitEmptyArray(unsigned int sourcePos)
{
    EmitOpcode(0x25, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x0806fa6a  size=105  FUN_0806fa6a  status=translated  naming=inferred
// opcode 0x13 + int placeholder -1 patched via Anim_AddNodeToCurrentTree (anim reference fixup) and the name string released
void EmitGetAnimation(unsigned int animName, unsigned int sourcePos)
{
    EmitOpcode(0x13, 1, 0);
    ScrDbg_PushLine(sourcePos, 1);
    EmitInt32(-1);
    Anim_AddNodeToCurrentTree((int *)scrLastEmitPtr, animName, sourcePos);
    Compile_ReleaseStringRef(animName);
}

// 0x0806fad4  size=91  FUN_0806fad4  status=translated  naming=unknown
// compiles (ScrCompile_ObjectExpr) then EmitOpcode(0x2a,1,0), pos, canonical string; 0x2a is a fusion target
void EmitEvalFieldVariable(unsigned int exprA, unsigned int stringId, unsigned int sourcePos, struct ScriptScope *scope)
{
    ScrCompile_ObjectExpr((ScriptNode *)exprA, sourcePos, scope);
    EmitOpcode(0x2a, 1, 0);
    ScrDbg_PushLine(sourcePos, 0);
    EmitCanonicalString(stringId);
}

// 0x0806fb30  size=72  FUN_0806fb30  status=translated  naming=unknown
// compiles (ScrCompile_ObjectExpr) then EmitOpcode(0x2e,0,0), canonical string
void EmitEvalFieldVariableRef(unsigned int exprA, unsigned int stringId, unsigned int sourcePos, struct ScriptScope *scope)
{
    ScrCompile_ObjectExpr((ScriptNode *)exprA, sourcePos, scope);
    EmitOpcode(0x2e, 0, 0);
    EmitCanonicalString(stringId);
}

// 0x0806fb78  size=91  FUN_0806fb78  status=translated  naming=unknown
// compiles (ScrCompile_ObjectExpr) then EmitOpcode(0x2f,0,0), pos2, canonical string
void EmitClearFieldVariable(unsigned int exprA, unsigned int stringId, unsigned int sourcePos, unsigned int sourcePos2, struct ScriptScope *scope)
{
    ScrCompile_ObjectExpr((ScriptNode *)exprA, sourcePos, scope);
    EmitOpcode(0x2f, 0, 0);
    ScrDbg_PushLine(sourcePos2, 0);
    EmitCanonicalString(stringId);
}

// 0x0806fbd4  size=369  FUN_0806fbd4  status=translated  naming=observed
// strings "bad expression" and "$ can only be used in the script debugger"; parses a $tNNN or $<letter>NNN token
void EmitDebuggerVariable(unsigned int stringId, unsigned int sourcePos)
{
    char first;
    const char *text;
    unsigned int number;
    int kind;
    int value;

    if (scrCompileDebuggerMode == 0) {
        text = ScrStr_TextFromHandle(stringId);
        if (*text == 't') {
            number = atoi(text + 1);
            if (number != 0 && number < 0xfffe) {
                first = Scr_IsVarUnreferenced((unsigned short)(number & 0xffff));
                if (first == 0 && (kind = Scr_GetVarType((unsigned short)(number & 0xffff)), 0xe < kind) &&
                    (kind < 0x13 || kind == 0x17)) {
                    EmitOpcode(0x82, 1, 0);
                    EmitShort((short)number);
                    return;
                }
            }
            ScrCompile_ErrorInFile(sourcePos, "bad expression");
        } else {
            kind = Scr_GetClassnumForCharId((int)*text);
            if (kind < 0) {
                ScrCompile_ErrorInFile(sourcePos, "bad expression");
            } else {
                value = atoi(text + 1);
                if (value == 0 && text[1] != '0') {
                    ScrCompile_ErrorInFile(sourcePos, "bad expression");
                } else {
                    EmitOpcode(0x81, 1, 0);
                    EmitInt32(kind);
                    EmitInt32(value);
                }
            }
        }
    } else {
        ScrCompile_ErrorInFile(sourcePos, "$ can only be used in the script debugger");
    }
}

// 0x0806fd46  size=36  FUN_0806fd46  status=translated  naming=unknown
// EmitOpcode(0x58,-1,0)
void EmitDecTop(void)
{
    EmitOpcode(0x58, -1, 0);
}

// 0x0806fd6a  size=55  FUN_0806fd6a  status=translated  naming=unknown
// EmitOpcode(0x59,-1,0) + pos(0); 0x59 is fused with preceding local eval opcodes
void EmitCastFieldObject(unsigned int sourcePos)
{
    EmitOpcode(0x59, -1, 0);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0806fda2  size=62  FUN_0806fda2  status=translated  naming=unknown
// compiles rhs (ScrCompile_ExprPush) and lhs (ScrCompile_PrimaryExprPush) then EmitEvalArray
void ScrCompile_EmitEvalArrayExpr(unsigned int lhs, unsigned int rhs, unsigned int srcPosA, unsigned int srcPosB, struct ScriptScope *scope)
{
    ScrCompile_ExprPush((ScriptNode *)rhs, scope);
    ScrCompile_PrimaryExprPush((ScriptNode *)lhs, scope);
    EmitEvalArray(srcPosA, srcPosB);
}

// 0x0806fde0  size=69  FUN_0806fde0  status=translated  naming=unknown
// compiles rhs (ScrCompile_ExprPush) and lhs (ScrCompile_StoreLvalueChecked((ScriptNode *)lhs, srcPosA, scope)) then EmitEvalArrayRef
void ScrCompile_EmitEvalArrayRefExpr(unsigned int lhs, unsigned int rhs, unsigned int srcPosA, unsigned int srcPosB, struct ScriptScope *scope)
{
    ScrCompile_ExprPush((ScriptNode *)rhs, scope);
    ScrCompile_StoreLvalueChecked((ScriptNode *)lhs, srcPosA, scope);
    EmitEvalArrayRef(srcPosA, srcPosB);
}

// 0x0806fe26  size=26  FUN_0806fe26  status=translated  naming=unknown
// tail-forwards to ScrCompile_LvalueFromObjectBase
void ScrCompile_LvalueFromObjectBaseThunk(ScriptNode *a, ScriptScope *b)
{
    ScrCompile_LvalueFromObjectBase(a, b);
}

// 0x0806fe40  size=69  FUN_0806fe40  status=translated  naming=unknown
// compiles rhs (ScrCompile_ExprPush) and lhs (ScrCompile_StoreLvalueChecked((ScriptNode *)lhs, srcPosA, scope)) then EmitClearArray
void ScrCompile_EmitClearArrayExpr(unsigned int lhs, unsigned int rhs, unsigned int srcPosA, unsigned int srcPosB, struct ScriptScope *scope)
{
    ScrCompile_ExprPush((ScriptNode *)rhs, scope);
    ScrCompile_StoreLvalueChecked((ScriptNode *)lhs, srcPosA, scope);
    EmitClearArray(srcPosA, srcPosB);
}

// 0x0806fe86  size=302  FUN_0806fe86  status=translated  naming=observed
// strings "self field in assignment expression not currently supported" / "self field can only be used in the script debugger"; dispatch on node->type
// note: node field meaning inferred from forwarded args; type 4 forwards (a,b,scope) into the local-variable eval emitter
void EmitAssignTarget(struct ScriptNode *node, struct ScriptScope *scope)
{
    int type;
    const char *message;

    type = node->type;
    if (type == 0xf) {
        EmitEvalFieldVariable(node->a, node->b, node->c, scope);
    } else if (type < 0x10) {
        if (type == 4) {
            EmitEvalLocalVariableCached(node->a, node->b, scope);
        } else if (type == 0xd) {
            ScrCompile_EmitEvalArrayExpr(node->a, node->b, node->c, node->d, scope);
        }
    } else if (type == 0x35) {
        if (scrCompileDebuggerMode == 0) {
            message = "self field in assignment expression not currently supported";
        } else {
            message = "self field can only be used in the script debugger";
        }
        ScrCompile_ErrorInFile(node->b, message);
    } else if (type == 0x4f) {
        EmitDebuggerVariable(node->a, node->b);
    }
}

// 0x0806ffb4  size=52  FUN_0806ffb4  status=translated  naming=unknown
// counts links from list->head
int ScrNodeList_Count(struct ScriptList *list)
{
    int count;
    struct ScriptListItem *link;

    count = 0;
    for (link = list->head; link != (struct ScriptListItem *)0; link = link->next) {
        count = count + 1;
    }
    return count;
}

// 0x0806ffe8  size=74  FUN_0806ffe8  status=translated  naming=unknown
// compiles each item (ScrCompile_ExprPush(first dword of item)) and returns the item count
// note: original reads the first dword of the item (*item); modelled as node->type, but the item may be a different type of record: the item pointer is dereferenced once and the loaded dword passed on
int ScrNodeList_CompileExprs(struct ScriptList *list, struct ScriptScope *scope)
{
    int count;
    struct ScriptListItem *link;

    count = 0;
    for (link = list->head; link != (struct ScriptListItem *)0; link = link->next) {
        ScrCompile_ExprPush((ScriptNode *)link->item->type, scope);
        count = count + 1;
    }
    return count;
}

// 0x08070032  size=60  FUN_08070032  status=translated  naming=unknown
// returns list->head when it is the only link, else 0
struct ScriptListItem *ScrNodeList_GetSingle(struct ScriptList *list)
{
    struct ScriptListItem *link;

    link = list->head;
    if (link == (struct ScriptListItem *)0) {
        link = (struct ScriptListItem *)0;
    } else if (link->next != (struct ScriptListItem *)0) {
        link = (struct ScriptListItem *)0;
    }
    return link;
}

// 0x0807006e  size=74  FUN_0807006e  status=translated  naming=unknown
// when scr_debug_enabled is set, registers item->a as a source position for every link
void ScrNodeList_AddDebugPositions(struct ScriptList *list)
{
    struct ScriptListItem *link;

    if (scr_debug_enabled != 0) {
        for (link = list->head; link != (struct ScriptListItem *)0; link = link->next) {
            ScrDbg_PushLine((unsigned int)link->item->a, 0);
        }
    }
}

// 0x080700b8  size=108  FUN_080700b8  status=translated  naming=unknown
// appends a 12-byte record at scrFuncRecordPtr and returns the variable object under g_scr_file_function_tables[stringId]
unsigned int Compile_AddFuncRecord(unsigned int stringId, unsigned int sourcePos, unsigned char flag)
{
    unsigned int var;

    ScrStr_AddRef(stringId);
    Compile_ReleaseStringRef(stringId);
    scrFuncRecordPtr->name = (unsigned short)stringId;
    scrFuncRecordPtr->sourcePos = (int)sourcePos;
    scrFuncRecordPtr->flag = flag;
    scrFuncRecordPtr = scrFuncRecordPtr + 1;
    var = ScrVar_GetNewVarValueId(g_scr_file_function_tables, stringId);
    return Scr_GetOrCreateObjectId(var);
}

// 0x08070124  size=856  FUN_08070124  status=translated  naming=observed
// strings "unknown function" (0x8143ad3) and "normal script cannot reference a function in a /# ... #/ comment"; resolves a function name (optionally file::func), emits its code position and records a fixup
// note: Ghidra decompile did not settle; reconstructed from disassembly 0x8070124..0x807047b. Names of fn_0807* variable-system callees unknown. In the file::func branch the lookup of the file entry is ScrVar_ReadVarValue(ScrVar_FindVarValueId(g_scr_loaded_scripts_table, nameId)); the code reads its type field only (u stored in a dead local).
void EmitFunctionRef(struct ScriptNode *node, unsigned int sourcePos)
{
    int flag;
    unsigned int parent;
    unsigned int func;
    unsigned int nameId;
    unsigned int fileVar;
    unsigned int funcVar;
    unsigned int fixupHolder;
    unsigned int fixupList;
    unsigned int slot;
    struct VariableValue fileValue;
    struct VariableValue headValue;
    struct VariableValue fixupValue;
    struct VariableValue newValue;
    char hasFile;
    const char *text;

    if (scrCompileMode == 2) {
        Compile_ReleaseStringRef((unsigned int)node->a);
        if (node->type == 0x15) {
            Compile_ReleaseStringRef((unsigned int)node->b);
            g_scr_dependency_record_count = g_scr_dependency_record_count + -1;
        }
        return;
    }
    funcVar = 0;
    if (node->type == 0x14) {
        flag = 0;
        parent = ScrVar_GetNewVarValueId((unsigned int)g_scr_current_file_symtab, (unsigned int)node->a);
        EmitStringRef((unsigned int)node->a, 2);
        funcVar = Scr_GetOrCreateObjectId(parent);
    } else {
        flag = 1;
        text = ScrStr_TextFromHandle((unsigned int)node->a);
        nameId = Scr_NormalizeFilenameToStringId(text);
        Compile_ReleaseStringRef((unsigned int)node->a);
        ScrVar_ReadVarValue(&fileValue, ScrVar_FindVarValueId(g_scr_loaded_scripts_table, nameId));
        hasFile = fileValue.type != 0;
        fileVar = Compile_AddFuncRecord(nameId, sourcePos, 0);
        if (hasFile == 0) {
            parent = ScrVar_GetNewVarValueId(fileVar, (unsigned int)node->b);
        } else {
            parent = ScrVar_FindVarValueId(fileVar, (unsigned int)node->b);
            if (parent == 0) {
                ScrCompile_ErrorInFile(sourcePos, "unknown function");
                return;
            }
            if (Scr_GetVarType((unsigned short)parent) != 1) {
                ScrCompile_ErrorInFile(sourcePos, "unknown function");
                return;
            }
        }
        EmitStringRef((unsigned int)node->b, 2);
        funcVar = Scr_GetOrCreateObjectId(parent);
        slot = ScrVar_FindVarValueId(funcVar, 1);
        if (slot != 0) {
            ScrVar_ReadVarValue(&fixupValue, slot);
            if (fixupValue.type == 0xd) {
                ScrCompile_ErrorInFile(sourcePos, "unknown function");
                return;
            }
            if (fixupValue.u.intValue != 0) {
                if (fixupValue.type == 7) {
                    EmitCodePos(fixupValue.u.intValue);
                    return;
                }
                if (scrCompileMode == 0) {
                    ScrCompile_ErrorInFile(sourcePos, "normal script cannot reference a function in a /# ... #/ comment");
                    return;
                }
                EmitCodePos(fixupValue.u.intValue);
                return;
            }
        }
    }
    EmitInt32(flag);
    fixupList = ScrVar_GetNewVarValueId(funcVar, 0);
    ScrVar_ReadVarValue(&headValue, fixupList);
    if (headValue.type == 0) {
        headValue.type = 6;
        headValue.u.intValue = 0;
    }
    fixupHolder = ScrVar_NewChildAVarId(funcVar, (unsigned int)(headValue.u.intValue + 2));
    newValue.u.intValue = (int)scrLastEmitPtr;
    if (scrCompileMode == 0) {
        newValue.type = 7;
    } else {
        newValue.type = 0xc;
    }
    ScrVar_SetVarValue(fixupHolder, &newValue);
    headValue.u.intValue = headValue.u.intValue + 1;
    ScrVar_SetVarValueReleasing(fixupList, &headValue);
    ScrDbg_PushLine(sourcePos, 0);
}

// 0x0807047c  size=73  FUN_0807047c  status=translated  naming=inferred
// EmitOpcode(0x15,1,0) then pos(3) then EmitFunctionRef; 0x15 is the opcode whose operand is a function reference (node type 0x15 handled in EmitFunctionRef)
void EmitGetFunction(struct ScriptNode *node, unsigned int sourcePos)
{
    EmitOpcode(0x15, 1, 0);
    ScrDbg_PushLine(sourcePos, 3);
    EmitFunctionRef(node, sourcePos);
}

// 0x080704c6  size=120  FUN_080704c6  status=translated  naming=observed
// string "SCR_FUNC_TABLE_SIZE exceeded" (prefixed by a 0x15 byte in rodata); linear search in a 0x400-entry table, appending on miss
int Scr_FindOrAddFuncTableEntry(int value)
{
    int i;

    i = 0;
    while (true) {
        if (scrFuncTableCount <= i) {
            if (scrFuncTableCount == SCR_FUNC_TABLE_MAX) {
                Com_Error(1, "\025SCR_FUNC_TABLE_SIZE exceeded");
            }
            scrFuncTable[scrFuncTableCount] = value;
            scrFuncTableCount = scrFuncTableCount + 1;
            return i;
        }
        if (scrFuncTable[i] == value) {
            break;
        }
        i = i + 1;
    }
    return i;
}

// 0x0807053e  size=118  FUN_0807053e  status=translated  naming=inferred
// opcode 0x50 (stack -argc) or 0x52 (-1-argc) with call kind 3, pos(3), then EmitFunctionRef; 0x50 is a peephole fusion target of 0x4e->0x4f
// note: param 3 was `char`; true when the call variant with an extra stack slot (opcode 0x52) is wanted
void EmitCallFunction(struct ScriptNode *funcNode, int argCount, char threaded, unsigned int sourcePos)
{
    if (threaded == 0) {
        EmitOpcode(0x50, -argCount, 3);
    } else {
        EmitOpcode(0x52, -1 - argCount, 3);
    }
    ScrDbg_PushLine(sourcePos, 3);
    EmitFunctionRef(funcNode, sourcePos);
}

// 0x080705b4  size=140  FUN_080705b4  status=translated  naming=inferred
// compiles the function-pointer expression then opcode 0x51 (-1-argc) or 0x53 (-2-argc) with call kind 3
void EmitCallFunctionPtr(unsigned int ptrExpr, int argCount, char threaded, unsigned int sourcePos, unsigned int srcPosB, struct ScriptScope *scope)
{
    ScrCompile_ExprPush((ScriptNode *)ptrExpr, scope);
    if (threaded == 0) {
        EmitOpcode(0x51, -1 - argCount, 3);
    } else {
        EmitOpcode(0x53, -2 - argCount, 3);
    }
    ScrDbg_PushLine(srcPosB, 0);
    ScrDbg_PushLine(sourcePos, 1);
}

// 0x08070640  size=131  FUN_08070640  status=translated  naming=inferred
// opcode 0x54 (1-argc) or 0x56 (-argc) with call kind 2, pos(3), EmitFunctionRef, then argCount as dword
// note: 0x54/0x56 are the targets of the 0xf-fusions in EmitOpcode (cases 0x52 -> 0x50, 0x56 -> 0x54)
void EmitThreadCallFunction(struct ScriptNode *funcNode, int argCount, char threaded, unsigned int sourcePos)
{
    if (threaded == 0) {
        EmitOpcode(0x54, 1 - argCount, 2);
    } else {
        EmitOpcode(0x56, -argCount, 2);
    }
    ScrDbg_PushLine(sourcePos, 3);
    EmitFunctionRef(funcNode, sourcePos);
    EmitInt32(argCount);
}

// 0x080706c4  size=129  FUN_080706c4  status=translated  naming=inferred
// compiles the pointer expression then opcode 0x55 (-argc) or 0x57 (-1-argc) with call kind 2, pos(1), argCount as dword
void EmitThreadCallFunctionPtr(unsigned int ptrExpr, int argCount, char threaded, unsigned int sourcePos, struct ScriptScope *scope)
{
    ScrCompile_ExprPush((ScriptNode *)ptrExpr, scope);
    if (threaded == 0) {
        EmitOpcode(0x55, -argCount, 2);
    } else {
        EmitOpcode(0x57, -1 - argCount, 2);
    }
    ScrDbg_PushLine(sourcePos, 1);
    EmitInt32(argCount);
}

// 0x08070746  size=133  FUN_08070746  status=translated  naming=inferred
// node type 0x12 -> EmitCallFunction, 0x16 -> EmitCallFunctionPtr
// note: callee->a is an ScriptNode * for type 0x12 (forwarded as funcNode) and an expression id for type 0x16
void EmitCallDispatch(struct ScriptNode *callee, int argCount, char threaded, unsigned int sourcePos, struct ScriptScope *scope)
{
    if (callee->type == 0x12) {
        EmitCallFunction((struct ScriptNode *)callee->a, argCount, threaded, sourcePos);
    } else if (callee->type == 0x16) {
        EmitCallFunctionPtr((unsigned int)callee->a, argCount, threaded, sourcePos, (unsigned int)callee->b, scope);
    }
}

// 0x080707cc  size=145  FUN_080707cc  status=translated  naming=inferred
// node type 0x12 -> EmitThreadCallFunction, 0x16 -> EmitThreadCallFunctionPtr, then pos(srcPosA,0)
void EmitThreadCallDispatch(struct ScriptNode *callee, int argCount, char threaded, unsigned int srcPosA, unsigned int sourcePos, struct ScriptScope *scope)
{
    if (callee->type == 0x12) {
        EmitThreadCallFunction((struct ScriptNode *)callee->a, argCount, threaded, sourcePos);
    } else if (callee->type == 0x16) {
        EmitThreadCallFunctionPtr((unsigned int)callee->a, argCount, threaded, (unsigned int)callee->b, scope);
    }
    ScrDbg_PushLine(srcPosA, 0);
}
