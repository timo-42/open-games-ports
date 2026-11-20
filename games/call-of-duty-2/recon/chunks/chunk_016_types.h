/*
 * chunk_016 types: script VM core (cod2_lnxded 1.3, 0x0807fd44..0x080825d4).
 * Layouts recovered from consistent offsets in VM_ExecuteInternal (0x08080158)
 * and the VM init/shutdown helpers.  Names follow the Call of Duty 4 / IW script
 * VM naming where structure, strings and call graph match (naming=inferred).
 */
#ifndef COD2_CHUNK_016_TYPES_H
#define COD2_CHUNK_016_TYPES_H

#include <setjmp.h>

/* VM_ExecuteInternal keeps its state in stack slots (the binary is unoptimised) and
 * relies on that across longjmp(); keep the reconstruction unoptimised too. */
#define COD2_VM_NOOPT __attribute__((optimize(0)))

/* Value type tags; names confirmed by the 0x0817d960 type-name table
 * ("undefined", "object", "string", "localized string", "vector", "float", "int",
 *  "codepos", "precodepos", "function", "stack", "animation", ...). */
enum {
    VAR_UNDEFINED = 0,
    VAR_POINTER = 1,       /* table name "object" */
    VAR_STRING = 2,
    VAR_ISTRING = 3,       /* "localized string" */
    VAR_VECTOR = 4,
    VAR_FLOAT = 5,
    VAR_INTEGER = 6,       /* "int" */
    VAR_CODEPOS = 7,       /* frame bottom marker on the value stack */
    VAR_PRECODEPOS = 8,    /* call/parameter start marker */
    VAR_FUNCTION = 9,
    VAR_STACK = 10,        /* saved VM stack (waiting thread) */
    VAR_ANIMATION = 11
};

/* Variable-slot kind returned by GetObjectType(): 0x15 is an entity-like object. */
enum { OBJECT_TYPE_ENTITY_LIKE = 0x15 };

enum { SCR_VM_MAX_FRAMES = 32, SCR_VM_STACK_ENTRIES = 2048 };

/* Opcodes handled by VM_ExecuteInternal (0x00..0x86). Names follow the CoD4 opcode
 * order; confirmed only where the handler's behaviour or error string agrees. */
enum ScrOpcode {
    OP_End = 0x00, OP_Return = 0x01, OP_GetUndefined = 0x02, OP_GetZero = 0x03,
    OP_GetByte = 0x04, OP_GetNegByte = 0x05, OP_GetUnsignedShort = 0x06,
    OP_GetNegUnsignedShort = 0x07, OP_GetInteger = 0x08, OP_GetFloat = 0x09,
    OP_GetString = 0x0a, OP_GetIString = 0x0b, OP_GetVector = 0x0c,
    OP_GetLevelObject = 0x0d, OP_GetAnimObject = 0x0e, OP_GetSelf = 0x0f,
    OP_GetLevel = 0x10, OP_GetGame = 0x11, OP_GetAnim = 0x12, OP_GetAnimation = 0x13,
    OP_GetGameRef = 0x14, OP_GetFunction = 0x15, OP_CreateLocalVariable = 0x16,
    OP_RemoveLocalVariables = 0x17,
    OP_EvalLocalVariableCached0 = 0x18, OP_EvalLocalVariableCached1 = 0x19,
    OP_EvalLocalVariableCached2 = 0x1a, OP_EvalLocalVariableCached3 = 0x1b,
    OP_EvalLocalVariableCached4 = 0x1c, OP_EvalLocalVariableCached5 = 0x1d,
    OP_EvalLocalVariableCached = 0x1e, OP_EvalLocalArrayCached = 0x1f,
    OP_EvalArray = 0x20, OP_EvalLocalArrayRefCached0 = 0x21,
    OP_EvalLocalArrayRefCached = 0x22, OP_EvalArrayRef = 0x23, OP_ClearArray = 0x24,
    OP_EmptyArray = 0x25, OP_GetSelfObject = 0x26,
    OP_EvalLevelFieldVariable = 0x27, OP_EvalAnimFieldVariable = 0x28,
    OP_EvalSelfFieldVariable = 0x29, OP_EvalFieldVariable = 0x2a,
    OP_EvalLevelFieldVariableRef = 0x2b, OP_EvalAnimFieldVariableRef = 0x2c,
    OP_EvalSelfFieldVariableRef = 0x2d, OP_EvalFieldVariableRef = 0x2e,
    OP_ClearFieldVariable = 0x2f, OP_SafeCreateVariableFieldCached = 0x30,
    OP_SafeSetVariableFieldCached0 = 0x31, OP_SafeSetVariableFieldCached = 0x32,
    OP_SafeSetWaittillVariableFieldCached = 0x33, OP_clearparams = 0x34,
    OP_checkclearparams = 0x35, OP_EvalLocalVariableRefCached0 = 0x36,
    OP_EvalLocalVariableRefCached = 0x37, OP_SetLevelFieldVariableField = 0x38,
    OP_SetVariableField = 0x39, OP_SetAnimFieldVariableField = 0x3a,
    OP_SetSelfFieldVariableField = 0x3b, OP_SetLocalVariableFieldCached0 = 0x3c,
    OP_SetLocalVariableFieldCached = 0x3d,
    OP_CallBuiltin0 = 0x3e, OP_CallBuiltin1 = 0x3f, OP_CallBuiltin2 = 0x40,
    OP_CallBuiltin3 = 0x41, OP_CallBuiltin4 = 0x42, OP_CallBuiltin5 = 0x43,
    OP_CallBuiltin = 0x44,
    OP_CallBuiltinMethod0 = 0x45, OP_CallBuiltinMethod1 = 0x46,
    OP_CallBuiltinMethod2 = 0x47, OP_CallBuiltinMethod3 = 0x48,
    OP_CallBuiltinMethod4 = 0x49, OP_CallBuiltinMethod5 = 0x4a,
    OP_CallBuiltinMethod = 0x4b,
    OP_wait = 0x4c, OP_waittillFrameEnd = 0x4d, OP_PreScriptCall = 0x4e,
    OP_PreScriptCallThenCall = 0x4f,
    OP_ScriptFunctionCall = 0x50, OP_ScriptFunctionCallPointer = 0x51,
    OP_ScriptMethodCall = 0x52, OP_ScriptMethodCallPointer = 0x53,
    OP_ScriptThreadCall = 0x54, OP_ScriptThreadCallPointer = 0x55,
    OP_ScriptMethodThreadCall = 0x56, OP_ScriptMethodThreadCallPointer = 0x57,
    OP_DecTop = 0x58, OP_CastFieldObject = 0x59,
    OP_EvalLocalVariableObjectCached = 0x5a, OP_CastBool = 0x5b, OP_BoolNot = 0x5c,
    OP_BoolComplement = 0x5d, OP_JumpOnFalse = 0x5e, OP_JumpOnTrue = 0x5f,
    OP_JumpOnFalseExpr = 0x60, OP_JumpOnTrueExpr = 0x61, OP_jump = 0x62,
    OP_jumpback = 0x63, OP_inc = 0x64, OP_dec = 0x65,
    OP_Binary66 = 0x66, OP_Binary67 = 0x67, OP_Binary68 = 0x68, OP_Binary69 = 0x69,
    OP_Binary6a = 0x6a, OP_Binary6b = 0x6b, OP_Binary6c = 0x6c, OP_Binary6d = 0x6d,
    OP_Binary6e = 0x6e, OP_Binary6f = 0x6f, OP_Binary70 = 0x70, OP_Binary71 = 0x71,
    OP_Binary72 = 0x72, OP_Binary73 = 0x73, OP_Binary74 = 0x74, OP_Binary75 = 0x75,
    OP_size = 0x76, OP_waittill77 = 0x77, OP_waittill78 = 0x78, OP_notify = 0x79,
    OP_endon = 0x7a, OP_voidCodepos = 0x7b, OP_switch = 0x7c, OP_endswitch = 0x7d,
    OP_vector = 0x7e, OP_NOP = 0x7f, OP_abort = 0x80, OP_object = 0x81,
    OP_thread_object = 0x82, OP_EvalLocalVariable = 0x83,
    OP_EvalLocalVariableRef = 0x84, OP_prof_begin = 0x85, OP_prof_end = 0x86
};

/* Builtin function / method entry points called through the table at 0x08205cb8. */
typedef void (*ScrBuiltinFunction)(void);
typedef void (*ScrBuiltinMethod)(unsigned int entity);

int Stack_PeekFromTopByte(const char *indexPtr);
/* Chunk 18 bytecode cursor readers with their new (const char **) signatures; the generated
 * cod2_prototypes.h still carries the old char ** ones until the merge lane regenerates it. */
int Cursor_ReadInt(const char **cursor);
const float *Cursor_AdvanceVec3(const char **cursor);
const char *Cursor_AdvanceDwords(const char **cursor, int count);
unsigned short Cursor_ReadShortZx(const char **cursor);
int Cursor_ReadIntAlias1(const char **cursor);
float Cursor_ReadFloat(const char **cursor);
const char *Cursor_ReadIntAlias2(const char **cursor);
int Cursor_ReadShort(const char **cursor);

/* Pair returned through a hidden pointer by Scr_GetVarInfoPair (same layout as chunk 15's). */
struct ScrVarInfoPair {
    uint16 field_6;            /* entry +0x06 */
    uint16 nameId;             /* flags >> 8 truncated to 16 bits */
};
COD2_ASSERT_SIZE(ScrVarInfoPair, 4);

/* Callees owned by chunk 14 (not yet migrated: cod2_prototypes.h lacks them or still carries
 * the local ScrVarValue type).  Declared here with the owner's signatures, ScrVarValue read as the
 * canonical VariableValue; hidden-return-pointer functions are called with an explicit out pointer. */
struct ScrVarSetTarget;
VariableValue *ScrVar_ReadVarValue(VariableValue *out, int id);
VariableValue *ScrVar_ReadVarValueOrPending(VariableValue *out, int id);
VariableValue *ScrVar_ReadFieldValue(VariableValue *out, int id, int name);
void ScrVar_GetFieldIntoTarget(int id, int name, struct ScrVarSetTarget *dest);
void ScrVar_SetVarValue(int id, VariableValue *v);
void ScrVar_SetVarValueReleasing(int id, VariableValue *v);
void ScrVar_SetVarValueOrPending(int id, VariableValue *v);

/* Chunk 17 definitions with canonical types (the generated prototypes are regenerated later). */
ScrStackBuffer_s *Scr_SaveStack(int valueCount, const char *pos, VariableValue *top,
                                int localVarCount, unsigned int *localId);
void Scr_NotifyObject(unsigned int objectId, unsigned int stringId, VariableValue *top);

#endif /* COD2_CHUNK_016_TYPES_H */
