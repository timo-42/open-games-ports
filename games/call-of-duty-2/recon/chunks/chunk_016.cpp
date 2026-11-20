#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_016_types.h"

/* Script VM core: class-map lookup, VM init/shutdown, error plumbing and the
 * bytecode interpreter loop (VM_ExecuteInternal).  Unsupplied callee names stay
 * as fn_08xxxxxx placeholders. */

// 0x0807fd44  size=78  FUN_0807fd44  status=translated  naming=inferred
// Linear search of the 4-entry class map (ids 'e','h','p','v' -> entity, hudelem, pathnode, vehiclenode); CoD4 Scr_GetClassnumForCharId.
int Scr_GetClassnumForCharId(char charId)
{
    int i;

    for (i = 0;; i++) {
        if (i > 3)
            return -1;
        if (g_scrClassInfo[i].charId == charId)
            return i;
    }
}

// 0x0807fd92  size=13  FUN_0807fd92  status=translated  naming=inferred
// Thin wrapper over the array allocator at 0x0807bca8 (type tag 0x16 = array); return value passes through in eax.
unsigned int Scr_AllocArray(void)
{
    return ScrVar_AllocArray();
}

// 0x0807fe46  size=25  FUN_0807fe46  status=translated  naming=inferred
// fabs() on a single float, returned in st0 (CoD Q_fabs); caller 0x0807dac0 is a comparison operator.
float Q_fabs(float value)
{
    return fabsf(value);
}

// 0x0807fe60  size=35  FUN_0807fe60  status=translated  naming=inferred
// Zeroes the three error globals (message, dialog message, index); same trio as CoD4 Scr_ClearErrorMessage.
void Scr_ClearErrorMessage(void)
{
    g_scr_compile_error_message = 0;
    scrVmGlob_dialog_error_message = 0;
    scrVarPub_error_index = 0;
}

// 0x0807fe84  size=194  FUN_0807fe84  status=translated  naming=inferred
// Resets the VM state: value stack/frames/local-variable array, error state, allocates the temp variable object and clears the well-known object ids.
void VM_Init(void)
{
    scrVmPub.maxstack = &scrVmStack[SCR_VM_STACK_ENTRIES - 1];
    scrVmPub.top = &scrVmStack[0];
    scrVmPub.function_count = 0;
    scrVmPub.function_frame = &scrVmPub.function_frame_start[0];
    scrVmPub.localVars = (unsigned int *)&scrVmGlob_starttime;
    scr_compile_error_deferred = 0;
    scrVmPub.debugCode = 0;
    Scr_ClearErrorMessage();
    scrVmPub.terminal_error = 0;
    scrVmPub.inparamcount = 0;
    scrVmPub.outparamcount = 0;
    scrVarPub_tempVariable = ScrVar_AllocValue();
    scrVarPub_timeArrayId = 0;
    scrVarPub_notifyArrayId = 0;
    scrVarPub_levelId = 0;
    scrVarPub_gameId = 0;
    scrVarPub_animId = 0;
    scrVarPub_freeEntList = 0;
    scrVmStack[0].type = VAR_CODEPOS;
    scrVmGlob_loading = 0;
}

// 0x0807ff46  size=40  FUN_0807ff46  status=translated  naming=inferred
// Releases the temp variable object allocated by VM_Init (if any).
void VM_Shutdown(void)
{
    if (scrVarPub_tempVariable != 0) {
        ScrVar_ReleaseAndFree(scrVarPub_tempVariable);
        scrVarPub_tempVariable = 0;
    }
}

// 0x0807ff6e  size=105  FUN_0807ff6e  status=translated  naming=inferred
// One-shot script system init guarded by the inited byte: variables, compiler, VM, then clears eight unnamed counters.
void Scr_Init(void)
{
    if (scrVarPub_inited == 0) {
        ScrStr_EnsureInit();
        ScrVar_Reset();
        VM_Init();
        scrCompileDebuggerMode = 0;
        g_scr_animtree_groups_active = 0;
        g_scr_file_function_tables = 0;
        g_scr_loaded_scripts_table = 0;
        scrAnimTreeRoot = 0;
        g_scr_builtin_method_cache = 0;
        scrCompileFuncCallArrayId = 0;
        scrVarPub_inited = 1;
    }
}

// 0x0807ffd8  size=41  FUN_0807ffd8  status=translated  naming=inferred
// Stores the three script settings as booleans (CoD4 Scr_Settings(developer, developer_script, abort_on_error)).
void Scr_Settings(int developer, int developer_script, int abort_on_error)
{
    scr_debug_enabled = (developer != 0);
    g_scr_developer_script = (developer_script != 0);
    scrVmPub.abort_on_error = (abort_on_error != 0);
}

// 0x08080002  size=41  FUN_08080002  status=translated  naming=inferred
// Counterpart of Scr_Init: tears down VM temp object, compiler and variable tables once.
void Scr_Shutdown(void)
{
    if (scrVarPub_inited != 0) {
        scrVarPub_inited = 0;
        VM_Shutdown();
        ScrVar_ReleaseGlobalObject();
        ScrStr_ClearReady();
    }
}

// 0x0808002c  size=22  FUN_0808002c  status=translated  naming=unknown
// Clears the time-array id and the inited flag without shutting anything down (name describes the writes only).
void Scr_ClearInitState(void)
{
    scrVarPub_timeArrayId = 0;
    scrVarPub_inited = 0;
}

// 0x08080042  size=13  FUN_08080042  status=translated  naming=inferred
// Setter for the "loading" flag read by Scr_ErrorInternal and the jumpback timeout check.
void Scr_SetLoading(int loading)
{
    scrVmGlob_loading = loading;
}

// 0x08080050  size=145  FUN_08080050  status=translated  naming=inferred
// Script error dispatcher: unwinds to the active VM_ExecuteInternal frame via longjmp when script code is running, otherwise raises Com_Error(1, "%s", message).
void Scr_ErrorInternal(void)
{
    if (scr_compile_error_deferred == 0 && scrCompileDebuggerMode == 0) {
        if (scr_debug_enabled != 0 && scrVmGlob_loading != 0)
            scrVmPub.terminal_error = 1;
        if (scrVmPub.function_count != 0 || scrVmPub.debugCode != 0)
            longjmp((struct __jmp_buf_tag *)g_script_error[g_script_error_level], -1);
    } else if (scrVmPub.terminal_error == 0) {
        return;
    }
    Com_Error(1, "%s", g_scr_compile_error_message);
}

// 0x080800e2  size=10  FUN_080800e2  status=translated  naming=unknown
// Returns 0; no callers resolved (descriptive name only).
int Stub_ReturnZero(void)
{
    return 0;
}

// 0x080800ec  size=48  FUN_080800ec  status=translated  naming=inferred
// Releases the builtin-call arguments still on the value stack (inparamcount entries), moving scrVmPub.top down.
void Scr_ClearInParams(void)
{
    while (scrVmPub.inparamcount != 0) {
        Scr_RemoveRefToValue((struct VariableValue *)(scrVmPub.top));
        scrVmPub.top--;
        scrVmPub.inparamcount--;
    }
}

// 0x0808011c  size=34  FUN_0808011c  status=translated  naming=unknown
// Clears the temp variable's value, then returns the object behind it (Scr_GetOrCreateObjectId); used by the VM error-recovery path for field-object ops.
unsigned int Scr_ClearTempAndGetObject(void)
{
    ScrVar_ClearValue(scrVarPub_tempVariable);
    return Scr_GetOrCreateObjectId(scrVarPub_tempVariable);
}

// 0x0808013e  size=26  FUN_0808013e  status=translated  naming=unknown
// Clears the temp variable's value and returns the temp variable id; used by the VM error-recovery path for field-reference ops.
unsigned int Scr_ClearTempAndGetId(void)
{
    ScrVar_ClearValue(scrVarPub_tempVariable);
    return scrVarPub_tempVariable;
}

// 0x08080158  size=9340  FUN_08080158  status=translated  naming=inferred
// Bytecode interpreter loop (CoD4 VM_ExecuteInternal): opcodes 0x00..0x86 dispatched through a jump table; one stack entry is VariableValue (8 bytes);
// errors raised by Scr_Error longjmp back here (g_script_error[level]) and are repaired per failing opcode in the error_recovery block.
// Observed strings: "script stack overflow (too many embedded function calls)", "%s is not a function pointer", "%s is not an object",
// "first parameter of waittill/notify/endon must evaluate to a string", "negative wait is not allowed", "potential infinite loop in script".
COD2_VM_NOOPT
unsigned int VM_ExecuteInternal(const char *codepos, unsigned int localId, int localVarCount,
                                VariableValue *top, VariableValue *startTop)
{
    const char *pos = codepos;       /* ebp+0x08 */
    int nestedCalls;                 /* number of inline thread calls entered by this invocation */
    unsigned int opcode;
    unsigned int objectId;           /* [ebp-0x18] */
    unsigned int fieldValueId;       /* [ebp-0x14] */
    unsigned int newLocalId;
    unsigned int selfId;
    unsigned int threadId;
    unsigned int stringId;
    unsigned int notifyObjectId;
    unsigned int variableId;
    unsigned int arrayVarId;
    struct ScrVarInfoPair entityRef;
    unsigned int typeIndex;
    unsigned int switchCount;
    int switchValue;
    int caseValue;
    const char *caseTarget;
    unsigned int offset;
    int waitTicks;
    int paramCount;
    int callCount;
    unsigned int builtinIndex;
    unsigned int pendingParams;
    unsigned int vmRetLocalId;
    VariableValue retVal;
    VariableValue value;
    VariableValue stackVal;
    VariableValue objVal;
    const char *funcPtr;
    VariableValue *p;
    ScrVmFrame *frame;
    unsigned int removeCount;
    byte removeByte;
    float waitFloat;

    nestedCalls = 0;
    g_script_error_level++;
    if (setjmp((struct __jmp_buf_tag *)g_script_error[g_script_error_level]) != 0)
        goto error_recovery;

next_opcode:
    opcode = *(const byte *)pos;
    pos++;
    if (opcode > 0x86)
        goto default_call;

    switch (opcode) {
    case OP_End:
        newLocalId = ScrVar_GetChildThreadParent(localId);
        ScrVar_RemoveThread(localId);
        scrVmPub.localVars -= localVarCount;
        while (top->type != VAR_CODEPOS) {
            Scr_RemoveRefToValue((struct VariableValue *)(top));
            top--;
        }
        scrVmPub.function_count--;
        scrVmPub.function_frame--;
        if (newLocalId == 0)
            goto thread_finished;
        top->type = VAR_UNDEFINED;
    resume_caller:
        ScrVar_Release(localId);
        localVarCount = scrVmPub.function_frame->localVarCount;
        localId = newLocalId;
        pos = scrVmPub.function_frame->pos;
        goto next_opcode;

    case OP_Return:
        newLocalId = ScrVar_GetChildThreadParent(localId);
        ScrVar_RemoveThread(localId);
        scrVmPub.localVars -= localVarCount;
        retVal = *top;
        p = top;
        for (;;) {
            top = p - 1;
            if (top->type == VAR_CODEPOS)
                break;
            Scr_RemoveRefToValue((struct VariableValue *)(top));
            p = top;
        }
        scrVmPub.function_count--;
        scrVmPub.function_frame--;
        if (newLocalId != 0) {
            *top = retVal;
            goto resume_caller;
        }
        *p = retVal;
        goto thread_return;

    case OP_GetUndefined:
        top[1].type = VAR_UNDEFINED;
        top++;
        goto next_opcode;

    case OP_GetZero:
        top[1].type = VAR_INTEGER;
        top[1].u.intValue = 0;
        top++;
        goto next_opcode;

    case OP_GetByte:
        top[1].type = VAR_INTEGER;
        top[1].u.intValue = (int)(unsigned int)*(const byte *)pos;
        pos++;
        top++;
        goto next_opcode;

    case OP_GetNegByte:
        top[1].type = VAR_INTEGER;
        top[1].u.intValue = -(int)(unsigned int)*(const byte *)pos;
        pos++;
        top++;
        goto next_opcode;

    case OP_GetUnsignedShort:
        top++;
        top->type = VAR_INTEGER;
        top->u.intValue = (int)(unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        goto next_opcode;

    case OP_GetNegUnsignedShort:
        top++;
        top->type = VAR_INTEGER;
        top->u.intValue = -(int)(unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        goto next_opcode;

    case OP_GetInteger:
        top++;
        top->type = VAR_INTEGER;
        top->u.intValue = Cursor_ReadInt(&pos);
        goto next_opcode;

    case OP_GetFloat:
        top++;
        top->type = VAR_FLOAT;
        top->u.floatValue = Cursor_ReadFloat(&pos);
        goto next_opcode;

    case OP_GetString:
        top++;
        top->type = VAR_STRING;
        top->u.stringValue = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        ScrStr_AddRef(top->u.stringValue);
        goto next_opcode;

    case OP_GetIString:
        top++;
        top->type = VAR_ISTRING;
        top->u.stringValue = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        ScrStr_AddRef(top->u.stringValue);
        goto next_opcode;

    case OP_GetVector:
        top++;
        top->type = VAR_VECTOR;
        top->u.vectorValue = Cursor_AdvanceVec3(&pos);
        goto next_opcode;

    case OP_GetLevelObject:
        objectId = scrVarPub_levelId;
        goto next_opcode;

    case OP_GetAnimObject:
        objectId = scrVarPub_animId;
        goto next_opcode;

    case OP_GetSelf:
        top++;
        top->type = VAR_POINTER;
        top->u.pointerValue = ScrVar_GetAux(localId);
        ScrVar_AddRef(top->u.pointerValue);
        goto next_opcode;

    case OP_GetLevel:
        top++;
        top->type = VAR_POINTER;
        top->u.pointerValue = scrVarPub_levelId;
        ScrVar_AddRef(scrVarPub_levelId);
        goto next_opcode;

    case OP_GetGame:
        top++;
        ScrVar_ReadVarValue(&value, scrVarPub_gameId);
        *top = value;
        goto next_opcode;

    case OP_GetAnim:
        top++;
        top->type = VAR_POINTER;
        top->u.pointerValue = scrVarPub_animId;
        ScrVar_AddRef(scrVarPub_animId);
        goto next_opcode;

    case OP_GetAnimation:
        top++;
        top->type = VAR_ANIMATION;
        top->u.intValue = Cursor_ReadInt(&pos);
        goto next_opcode;

    case OP_GetGameRef:
        fieldValueId = scrVarPub_gameId;
        goto next_opcode;

    case OP_GetFunction:
        top++;
        top->type = VAR_FUNCTION;
        top->u.codePosValue = Cursor_ReadIntAlias2(&pos);
        goto next_opcode;

    case OP_CreateLocalVariable:
        scrVmPub.localVars++;
        localVarCount++;
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        *scrVmPub.localVars = ScrVar_NewChildAVarId(localId, stringId);
        goto next_opcode;

    case OP_RemoveLocalVariables:
        removeByte = *(const byte *)pos;
        pos++;
        scrVmPub.localVars -= (unsigned int)removeByte;
        localVarCount -= (int)(unsigned int)removeByte;
        for (; removeByte != 0; removeByte--)
            ScrVar_RemoveFirstChild(localId);
        goto next_opcode;

    case OP_EvalLocalVariableCached0:
        top++;
        ScrVar_ReadVarValue(&value, *scrVmPub.localVars);
        *top = value;
        goto next_opcode;

    case OP_EvalLocalVariableCached1:
        top++;
        ScrVar_ReadVarValue(&value, Stack_PeekFromTop(1));
        *top = value;
        goto next_opcode;

    case OP_EvalLocalVariableCached2:
        top++;
        ScrVar_ReadVarValue(&value, Stack_PeekFromTop(2));
        *top = value;
        goto next_opcode;

    case OP_EvalLocalVariableCached3:
        top++;
        ScrVar_ReadVarValue(&value, Stack_PeekFromTop(3));
        *top = value;
        goto next_opcode;

    case OP_EvalLocalVariableCached4:
        top++;
        ScrVar_ReadVarValue(&value, Stack_PeekFromTop(4));
        *top = value;
        goto next_opcode;

    case OP_EvalLocalVariableCached5:
        top++;
        ScrVar_ReadVarValue(&value, Stack_PeekFromTop(5));
        *top = value;
        goto next_opcode;

    case OP_EvalLocalVariableCached:
        top++;
        ScrVar_ReadVarValue(&value, Stack_PeekFromTopByte(pos));
        *top = value;
        pos++;
        goto next_opcode;

    case OP_EvalLocalArrayCached:
        top++;
        ScrVar_ReadVarValue(&value, Stack_PeekFromTopByte(pos));
        *top = value;
        pos++;
        /* falls through into OP_EvalArray */
    case OP_EvalArray:
        Scr_EvalArrayIndex(top, top - 1);
        break;

    case OP_EvalLocalArrayRefCached0:
        fieldValueId = *scrVmPub.localVars;
        goto eval_array_ref;

    case OP_EvalLocalArrayRefCached:
        fieldValueId = Stack_PeekFromTopByte(pos);
        pos++;
        /* falls through */
    case OP_EvalArrayRef:
    eval_array_ref:
        arrayVarId = Scr_GetArrayForWrite(fieldValueId);
        fieldValueId = Scr_GetArrayVarForIndex(arrayVarId, top);
        break;

    case OP_ClearArray:
        Scr_SetArrayElement(fieldValueId, top);
        break;

    case OP_EmptyArray:
        top++;
        top->type = VAR_POINTER;
        top->u.pointerValue = ScrVar_AllocArray();
        goto next_opcode;

    case OP_GetSelfObject:
        objectId = ScrVar_GetAux(localId);
        if (Scr_IsVarTypeBelowArray(objectId) == 0)
            goto not_object_of_id;
        goto next_opcode;

    case OP_EvalLevelFieldVariable:
        objectId = scrVarPub_levelId;
        goto eval_named_field;

    case OP_EvalAnimFieldVariable:
        objectId = scrVarPub_animId;
    eval_named_field:
        top++;
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        variableId = ScrVar_FindVarValueId(objectId, stringId);
        ScrVar_ReadVarValue(&value, variableId);
        *top = value;
        goto next_opcode;

    case OP_EvalSelfFieldVariable:
        objectId = ScrVar_GetAux(localId);
        if (Scr_IsVarTypeBelowArray(objectId) == 0) {
            top++;
            Cursor_ReadShortZx(&pos);
            goto not_object_of_id;
        }
        /* falls through into OP_EvalFieldVariable */
    case OP_EvalFieldVariable:
        top++;
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        ScrVar_ReadFieldValue(&value, objectId, stringId);
        *top = value;
        goto next_opcode;

    case OP_EvalLevelFieldVariableRef:
        objectId = scrVarPub_levelId;
        goto eval_field_ref;

    case OP_EvalAnimFieldVariableRef:
        objectId = scrVarPub_animId;
        goto eval_field_ref;

    case OP_EvalSelfFieldVariableRef:
        objectId = ScrVar_GetAux(localId);
        /* falls through */
    case OP_EvalFieldVariableRef:
    eval_field_ref:
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        fieldValueId = ScrVar_GetVarIdForSet(objectId, stringId);
        goto next_opcode;

    case OP_ClearFieldVariable:
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        ScrVar_GetFieldIntoTarget(objectId, stringId, (struct ScrVarSetTarget *)top);
        goto next_opcode;

    case OP_SafeCreateVariableFieldCached:
        scrVmPub.localVars++;
        localVarCount++;
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        *scrVmPub.localVars = ScrVar_NewChildAVarId(localId, stringId);
        /* falls through */
    case OP_SafeSetVariableFieldCached0:
        if (top->type == VAR_PRECODEPOS)
            goto next_opcode;
        ScrVar_SetVarValueReleasing(*scrVmPub.localVars, top);
        break;

    case OP_SafeSetVariableFieldCached:
        if (top->type == VAR_PRECODEPOS) {
            pos++;
            goto next_opcode;
        }
        ScrVar_SetVarValueReleasing(Stack_PeekFromTopByte(pos), top);
        pos++;
        break;

    case OP_SafeSetWaittillVariableFieldCached:
        if (top->type == VAR_CODEPOS) {
            ScrVar_ClearValue(Stack_PeekFromTopByte(pos));
            pos++;
            goto next_opcode;
        }
        ScrVar_SetVarValueReleasing(Stack_PeekFromTopByte(pos), top);
        pos++;
        break;

    case OP_clearparams:
        while (top->type != VAR_CODEPOS) {
            Scr_RemoveRefToValue((struct VariableValue *)(top));
            top--;
        }
        goto next_opcode;

    case OP_checkclearparams:
        if (top->type != VAR_PRECODEPOS) {
            Scr_Error("function called with too many parameters");
            goto eval_local_ref0;
        }
        top->type = VAR_CODEPOS;
        goto next_opcode;

    case OP_EvalLocalVariableRefCached0:
    eval_local_ref0:
        fieldValueId = *scrVmPub.localVars;
        goto next_opcode;

    case OP_EvalLocalVariableRefCached:
        fieldValueId = Stack_PeekFromTopByte(pos);
        pos++;
        goto next_opcode;

    case OP_SetLevelFieldVariableField:
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        ScrVar_SetVarValueReleasing(ScrVar_GetNewVarValueId(scrVarPub_levelId, stringId), top);
        break;

    case OP_SetAnimFieldVariableField:
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        ScrVar_SetVarValueReleasing(ScrVar_GetNewVarValueId(scrVarPub_animId, stringId), top);
        break;

    case OP_SetSelfFieldVariableField:
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        objectId = ScrVar_GetAux(localId);
        fieldValueId = ScrVar_GetVarIdForSet(objectId, stringId);
        /* falls through */
    case OP_SetVariableField:
    set_variable_field:
        ScrVar_SetVarValueOrPending(fieldValueId, top);
        break;

    case OP_SetLocalVariableFieldCached0:
        ScrVar_SetVarValueReleasing(*scrVmPub.localVars, top);
        break;

    case OP_SetLocalVariableFieldCached:
        ScrVar_SetVarValueReleasing(Stack_PeekFromTopByte(pos), top);
        pos++;
        break;

    case OP_CallBuiltin1:
        scrVmPub.inparamcount = 1;
        goto call_builtin;
    case OP_CallBuiltin2:
        scrVmPub.inparamcount = 2;
        goto call_builtin;
    case OP_CallBuiltin3:
        scrVmPub.inparamcount = 3;
        goto call_builtin;
    case OP_CallBuiltin4:
        scrVmPub.inparamcount = 4;
        goto call_builtin;
    case OP_CallBuiltin5:
        scrVmPub.inparamcount = 5;
        goto call_builtin;
    case OP_CallBuiltin:
        scrVmPub.inparamcount = (int)(unsigned int)*(const byte *)pos;
        pos++;
        /* falls through */
    case OP_CallBuiltin0:
    call_builtin:
        scrVmPub.top = top;
        builtinIndex = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        scrVmPub.function_frame->pos = pos;
        ((ScrBuiltinFunction)scrFuncTable[builtinIndex])();
    after_builtin:
        pendingParams = (unsigned int)scrVmPub.inparamcount;
        top = scrVmPub.top;
        pos = scrVmPub.function_frame->pos;
        if (pendingParams != 0) {
            callCount = scrVmPub.inparamcount;
            scrVmPub.inparamcount = 0;
            scrVmPub.top -= pendingParams;
            do {
                Scr_RemoveRefToValue((struct VariableValue *)(top));
                top--;
                callCount--;
            } while (callCount != 0);
        }
        if (scrVmPub.outparamcount == 0) {
            top[1].type = VAR_UNDEFINED;
            top++;
        } else {
            scrVmPub.outparamcount = 0;
        }
        goto next_opcode;

    case OP_CallBuiltinMethod1:
        scrVmPub.inparamcount = 1;
        goto call_builtin_method;
    case OP_CallBuiltinMethod2:
        scrVmPub.inparamcount = 2;
        goto call_builtin_method;
    case OP_CallBuiltinMethod3:
        scrVmPub.inparamcount = 3;
        goto call_builtin_method;
    case OP_CallBuiltinMethod4:
        scrVmPub.inparamcount = 4;
        goto call_builtin_method;
    case OP_CallBuiltinMethod5:
        scrVmPub.inparamcount = 5;
        goto call_builtin_method;
    case OP_CallBuiltinMethod:
        scrVmPub.inparamcount = (int)(unsigned int)*(const byte *)pos;
        pos++;
        /* falls through */
    case OP_CallBuiltinMethod0:
    call_builtin_method:
        scrVmPub.top = top - 1;
        builtinIndex = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        if (top->type == VAR_POINTER) {
            objectId = top->u.pointerValue;
            if (Scr_GetVarType(objectId) != OBJECT_TYPE_ENTITY_LIKE) {
                typeIndex = Scr_GetVarType(objectId);
                ScrVar_Release(objectId);
                scrVarPub_error_index = -1;
                Scr_Error(va("%s is not an entity", g_scrTypeNames[typeIndex]));
                goto method_target_not_entity;
            }
            entityRef = Scr_GetVarInfoPair(objectId);
            ScrVar_Release(objectId);
            scrVmPub.function_frame->pos = pos;
            ((ScrBuiltinMethod)scrFuncTable[builtinIndex])(*(unsigned int *)&entityRef);
            goto after_builtin;
        }
    method_target_not_entity:
        typeIndex = (unsigned int)top->type;
        Scr_RemoveRefToValue((struct VariableValue *)(top));
        scrVarPub_error_index = -1;
        Scr_Error(va("%s is not an entity", g_scrTypeNames[typeIndex]));
        /* falls through into OP_wait */
    case OP_wait:
    op_wait:
        if (top->type == VAR_FLOAT) {
            waitFloat = top->u.floatValue;
            if (waitFloat < 0.0f)
                goto wait_negative;
            waitTicks = Float_RoundToInt_08085744(waitFloat * 20.0f);
            if (waitTicks == 0 && waitFloat != 0.0f)
                waitTicks = 1;
        } else if (top->type == VAR_INTEGER) {
            waitTicks = top->u.intValue * 20;
        } else {
            scrVarPub_error_index = 2;
            Scr_Error(va("type %s is not a float", g_scrTypeNames[top->type]));
        }
        if ((unsigned int)waitTicks > 0xfffffe) {
            scrVarPub_error_index = 2;
            if (waitTicks >= 0)
                Scr_Error("wait is too long");
            goto wait_negative;
        }
        if (waitTicks != 0)
            GScr_resettimeout();
        waitTicks = (int)(((unsigned int)waitTicks + scrVarPub_time) & 0xffffff);
        top--;
        stackVal.type = VAR_STACK;
        stackVal.u.stackValue = Scr_SaveStack((int)(top - startTop), pos, top, localVarCount, &localId);
        variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueId(scrVarPub_timeArrayId, (unsigned int)waitTicks));
        threadId = ScrVar_NewChildAVarIdBias(variableId, localId);
        ScrVar_SetVarValue(threadId, &stackVal);
        ScrVar_SetTimeThread(localId, (unsigned int)waitTicks);
        goto thread_waiting;

    wait_negative:
        Scr_Error("negative wait is not allowed");
        /* falls through into OP_waittillFrameEnd */
    case OP_waittillFrameEnd:
        stackVal.type = VAR_STACK;
        stackVal.u.stackValue = Scr_SaveStack((int)(top - startTop), pos, top, localVarCount, &localId);
        variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueId(scrVarPub_timeArrayId, scrVarPub_time));
        threadId = ScrVar_NewChildBVarIdBias(variableId, localId);
        ScrVar_SetVarValue(threadId, &stackVal);
        ScrVar_SetTimeThread(localId, scrVarPub_time);
    thread_waiting:
        goto thread_finished;

    case OP_PreScriptCall:
        top[1].type = VAR_PRECODEPOS;
        top++;
        goto next_opcode;

    case OP_PreScriptCallThenCall:
        top[1].type = VAR_PRECODEPOS;
        top++;
        /* falls through */
    case OP_ScriptFunctionCall:
        if (scrVmPub.function_count > 0x1e) {
            Scr_Error("script stack overflow (too many embedded function calls)");
            goto script_function_call_pointer;
        }
        selfId = ScrVar_GetAux(localId);
        ScrVar_AddRef(selfId);
        localId = ScrVar_AllocChildThread(selfId, localId);
        scrVmPub.function_frame->pos = pos;
        pos = Cursor_ReadIntAlias2(&scrVmPub.function_frame->pos);
        goto call_enter;

    case OP_ScriptFunctionCallPointer:
    script_function_call_pointer:
        if (top->type == VAR_FUNCTION) {
            if (scrVmPub.function_count < 0x1f) {
                selfId = ScrVar_GetAux(localId);
                ScrVar_AddRef(selfId);
                localId = ScrVar_AllocChildThread(selfId, localId);
                scrVmPub.function_frame->pos = pos;
                pos = top->u.codePosValue;
                top--;
                goto call_enter;
            }
            scrVarPub_error_index = 1;
            Scr_Error("script stack overflow (too many embedded function calls)");
        }
        Scr_Error(va("%s is not a function pointer", g_scrTypeNames[top->type]));
        /* falls through into OP_ScriptMethodCall */
    case OP_ScriptMethodCall:
        if (top->type == VAR_POINTER) {
            if (scrVmPub.function_count < 0x1f) {
                localId = ScrVar_AllocChildThread(top->u.pointerValue, localId);
                top--;
                scrVmPub.function_frame->pos = pos;
                pos = Cursor_ReadIntAlias2(&scrVmPub.function_frame->pos);
                goto call_enter;
            }
            Scr_Error("script stack overflow (too many embedded function calls)");
        }
        typeIndex = (unsigned int)top->type;
        goto not_object_set_index_1;

    case OP_ScriptMethodCallPointer:
        if (top->type != VAR_FUNCTION) {
            Scr_RemoveRefToValue((struct VariableValue *)(top));
            typeIndex = (unsigned int)top->type;
            top--;
            Scr_Error(va("%s is not a function pointer", g_scrTypeNames[typeIndex]));
            goto script_thread_call;
        }
        funcPtr = top->u.codePosValue;
        p = top - 1;
        top = p;
        if (p->type != VAR_POINTER)
            goto not_object_of_top_type;
        if (scrVmPub.function_count > 0x1e) {
            scrVarPub_error_index = 1;
            Scr_Error("script stack overflow (too many embedded function calls)");
            goto not_object_of_top_type;
        }
        localId = ScrVar_AllocChildThread(p->u.pointerValue, localId);
        top = p - 1;
        scrVmPub.function_frame->pos = pos;
        pos = funcPtr;
        goto call_enter;

    case OP_ScriptThreadCall:
    script_thread_call:
        if (scrVmPub.function_count > 0x1e) {
            scrVarPub_error_index = 1;
            Scr_Error("script stack overflow (too many embedded function calls)");
            goto script_thread_call_pointer;
        }
        selfId = ScrVar_GetAux(localId);
        ScrVar_AddRef(selfId);
        localId = ScrVar_AllocThread(selfId);
        scrVmPub.function_frame->pos = pos;
        scrVmPub.function_frame->startTop = startTop;
        pos = Cursor_ReadIntAlias2(&scrVmPub.function_frame->pos);
        paramCount = Cursor_ReadInt(&scrVmPub.function_frame->pos);
        startTop = top - paramCount;
        goto default_call;

    case OP_ScriptThreadCallPointer:
    script_thread_call_pointer:
        if (top->type == VAR_FUNCTION) {
            if (scrVmPub.function_count < 0x1f) {
                funcPtr = top->u.codePosValue;
                top--;
                selfId = ScrVar_GetAux(localId);
                ScrVar_AddRef(selfId);
                localId = ScrVar_AllocThread(selfId);
                scrVmPub.function_frame->pos = pos;
                scrVmPub.function_frame->startTop = startTop;
                pos = funcPtr;
                paramCount = Cursor_ReadInt(&scrVmPub.function_frame->pos);
                startTop = top - paramCount;
                goto default_call;
            }
            scrVarPub_error_index = 1;
            Scr_Error("script stack overflow (too many embedded function calls)");
        }
        Scr_Error(va("%s is not a function pointer", g_scrTypeNames[top->type]));
        /* falls through into OP_ScriptMethodThreadCall */
    case OP_ScriptMethodThreadCall:
        if (top->type == VAR_POINTER) {
            if (scrVmPub.function_count < 0x1f) {
                localId = ScrVar_AllocThread(top->u.pointerValue);
                top--;
                scrVmPub.function_frame->pos = pos;
                scrVmPub.function_frame->startTop = startTop;
                pos = Cursor_ReadIntAlias2(&scrVmPub.function_frame->pos);
                paramCount = Cursor_ReadInt(&scrVmPub.function_frame->pos);
                startTop = top - paramCount;
                goto default_call;
            }
            scrVarPub_error_index = 1;
            Scr_Error("script stack overflow (too many embedded function calls)");
        }
        goto not_object_of_top_type;

    case OP_ScriptMethodThreadCallPointer:
        if (top->type != VAR_FUNCTION) {
            Scr_RemoveRefToValue((struct VariableValue *)(top));
            typeIndex = (unsigned int)top->type;
            top--;
            Scr_Error(va("%s is not a function pointer", g_scrTypeNames[typeIndex]));
            goto dec_top;
        }
        funcPtr = top->u.codePosValue;
        p = top - 1;
        top = p;
        if (p->type == VAR_POINTER) {
            if (scrVmPub.function_count < 0x1f) {
                localId = ScrVar_AllocThread(p->u.pointerValue);
                top = p - 1;
                scrVmPub.function_frame->pos = pos;
                scrVmPub.function_frame->startTop = startTop;
                pos = funcPtr;
                paramCount = Cursor_ReadInt(&scrVmPub.function_frame->pos);
                startTop = top - paramCount;
                goto default_call;
            }
            scrVarPub_error_index = 1;
            Scr_Error("script stack overflow (too many embedded function calls)");
        }
        goto not_object_of_top_type;

    case OP_DecTop:
    dec_top:
        Scr_RemoveRefToValue((struct VariableValue *)(top));
        break;

    case OP_CastFieldObject:
        objectId = Scr_GetFieldObject(scrVarPub_tempVariable, top);
        break;

    case OP_EvalLocalVariableObjectCached:
        objectId = ScrVar_GetFieldObjectId(Stack_PeekFromTopByte(pos));
        pos++;
        goto next_opcode;

    case OP_CastBool:
        Scr_CastBool(top);
        goto next_opcode;

    case OP_BoolNot:
        Scr_EvalBoolNot(top);
        goto next_opcode;

    case OP_BoolComplement:
        Scr_EvalBitNot(top);
        goto next_opcode;

    case OP_JumpOnFalse:
        Scr_CastBool(top);
        offset = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        if (top->u.intValue == 0)
            pos += offset;
        break;

    case OP_JumpOnTrue:
        Scr_CastBool(top);
        offset = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        if (top->u.intValue != 0)
            pos += offset;
        break;

    case OP_JumpOnFalseExpr:
        Scr_CastBool(top);
        offset = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        if (top->u.intValue == 0) {
            pos += offset;
            goto next_opcode;
        }
        break;

    case OP_JumpOnTrueExpr:
        Scr_CastBool(top);
        offset = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        if (top->u.intValue != 0) {
            pos += offset;
            goto next_opcode;
        }
        break;

    case OP_jump:
        offset = (unsigned int)Cursor_ReadInt(&pos);
        pos += offset;
        goto next_opcode;

    case OP_jumpback:
        if (Sys_Milliseconds() - scrVmGlob_starttime < 0x9c4) {
            offset = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
            pos -= offset;
            goto next_opcode;
        }
        if (scrVmGlob_loading != 0) {
            Com_Printf("script runtime warning: potential infinite loop in script.\n");
            ScrDbg_PrintLocation(0, pos, 0);
            offset = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
            pos -= offset;
            GScr_resettimeout();
            goto next_opcode;
        }
        if (scrVmPub.abort_on_error != 0) {
            Scr_ErrorAndDisable("potential infinite loop in script");
            goto op_inc;
        }
        Com_Printf("script runtime error: potential infinite loop in script - killing thread.\n");
        ScrDbg_PrintLocation(0, pos, 0);
        GScr_resettimeout();
        for (;;) {
            newLocalId = ScrVar_GetChildThreadParent(localId);
            ScrVar_RemoveThread(localId);
            scrVmPub.localVars -= localVarCount;
            while (top->type != VAR_CODEPOS) {
                Scr_RemoveRefToValue((struct VariableValue *)(top));
                top--;
            }
            scrVmPub.function_count--;
            scrVmPub.function_frame--;
            if (newLocalId == 0)
                break;
            ScrVar_Release(localId);
            pos = scrVmPub.function_frame->pos;
            localVarCount = scrVmPub.function_frame->localVarCount;
            localId = newLocalId;
            top--;
        }
        goto thread_finished;

    case OP_inc:
    op_inc:
        top++;
        ScrVar_ReadVarValueOrPending(&value, fieldValueId);
        *top = value;
        if (top->type == VAR_INTEGER) {
            top->u.intValue = top->u.intValue + 1;
            pos++;
            goto set_variable_field;
        }
        Scr_Error(va("++ must be applied to an int (applied to %s)", g_scrTypeNames[top->type]));
        /* falls through into OP_dec */
    case OP_dec:
        top++;
        ScrVar_ReadVarValueOrPending(&value, fieldValueId);
        *top = value;
        if (top->type != VAR_INTEGER) {
            Scr_Error(va("-- must be applied to an int (applied to %s)", g_scrTypeNames[top->type]));
            Scr_EvalBitOr(top - 1, top);
            break;
        }
        top->u.intValue = top->u.intValue - 1;
        pos++;
        goto set_variable_field;

    case OP_Binary66:
        Scr_EvalBitOr(top - 1, top);
        break;
    case OP_Binary67:
        Scr_EvalBitXor(top - 1, top);
        break;
    case OP_Binary68:
        Scr_EvalBitAnd(top - 1, top);
        break;
    case OP_Binary69:
        Scr_EvalEquals(top - 1, top);
        break;
    case OP_Binary6a:
        Scr_EvalNotEquals(top - 1, top);
        break;
    case OP_Binary6b:
        Scr_EvalLess(top - 1, top);
        break;
    case OP_Binary6c:
        Scr_EvalGreater(top - 1, top);
        break;
    case OP_Binary6d:
        Scr_EvalLessEqual(top - 1, top);
        break;
    case OP_Binary6e:
        Scr_EvalGreaterEqual(top - 1, top);
        break;
    case OP_Binary6f:
        Scr_EvalShiftLeft(top - 1, top);
        break;
    case OP_Binary70:
        Scr_EvalShiftRight(top - 1, top);
        break;
    case OP_Binary71:
        Scr_EvalPlus(top - 1, top);
        break;
    case OP_Binary72:
        Scr_EvalMinus(top - 1, top);
        break;
    case OP_Binary73:
        Scr_EvalMultiply(top - 1, top);
        break;
    case OP_Binary74:
        Scr_EvalDivide(top - 1, top);
        break;
    case OP_Binary75:
        Scr_EvalModulo(top - 1, top);
        break;

    case OP_size:
        Scr_EvalSizeValue(top);
        goto next_opcode;

    case OP_waittill77:
    case OP_waittill78:
        if (top->type != VAR_POINTER)
            goto not_object_of_top_type;
        if (Scr_IsVarTypeBelowArray(top->u.pointerValue) != 0) {
            notifyObjectId = top->u.pointerValue;
            if (top[-1].type == VAR_STRING) {
                stringId = top[-1].u.stringValue;
                top -= 2;
                stackVal.type = VAR_STACK;
                stackVal.u.stackValue = Scr_SaveStack((int)(top - startTop), pos, top, localVarCount, &localId);
                variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueId(notifyObjectId, 0x1fffe) /* binary 0x08081b2c: push 0x1fffe (object notify-list head) */);
                variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueId(variableId, stringId));
                threadId = ScrVar_NewChildAVarIdBias(variableId, localId);
                ScrVar_SetVarValue(threadId, &stackVal);
                objVal.u.pointerValue = notifyObjectId;
                objVal.type = VAR_POINTER;
                selfId = ScrVar_GetAux(localId);
                variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueIdBias(scrVarPub_notifyArrayId, selfId));
                threadId = ScrVar_NewChildAVarIdBias(variableId, localId);
                ScrVar_SetVarValue(threadId, &objVal);
                ScrVar_SetNotifyThread(localId, stringId);
                goto thread_finished;
            }
            scrVarPub_error_index = 3;
            Scr_Error("first parameter of waittill must evaluate to a string");
        }
        typeIndex = Scr_GetVarType(top->u.pointerValue);
        goto not_object_set_index_2;

    case OP_notify:
        if (top->type == VAR_POINTER) {
            variableId = top->u.pointerValue;
            if (Scr_IsVarTypeBelowArray(variableId) != 0) {
                if (top[-1].type == VAR_STRING) {
                    stringId = top[-1].u.stringValue;
                    top -= 2;
                    scrVmPub.function_frame->pos = pos;
                    Scr_NotifyObject(variableId, stringId, top);
                    pos = scrVmPub.function_frame->pos;
                    ScrVar_Release(variableId);
                    ScrStr_Release(stringId);
                    for (; top->type != VAR_PRECODEPOS; top--)
                        Scr_RemoveRefToValue((struct VariableValue *)(top));
                    break;
                }
                scrVarPub_error_index = 1;
                Scr_Error("first parameter of notify must evaluate to a string");
            }
            typeIndex = Scr_GetVarType(top->u.pointerValue);
            goto not_object_set_index_2;
        }
        goto not_object_of_top_type;

    case OP_endon:
        if (top->type != VAR_POINTER) {
            typeIndex = (unsigned int)top->type;
            goto not_object_set_index_1;
        }
        if (Scr_IsVarTypeBelowArray(top->u.pointerValue) != 0) {
            if (top[-1].type == VAR_STRING) {
                stringId = top[-1].u.stringValue;
                ScrVar_AddRef(localId);
                threadId = ScrVar_AllocThread(localId);
                variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueId(top->u.pointerValue, 0x1fffe) /* binary 0x08081d36 */);
                variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueId(variableId, stringId));
                ScrVar_GetNewVarValueIdBias(variableId, threadId);
                ScrVar_Release(threadId);
                objVal.type = VAR_POINTER;
                objVal.u.pointerValue = top->u.pointerValue;
                variableId = Scr_GetOrCreateArrayId(ScrVar_GetNewVarValueIdBias(scrVarPub_notifyArrayId, localId));
                variableId = ScrVar_NewChildAVarIdBias(variableId, threadId);
                ScrVar_SetVarValue(variableId, &objVal);
                ScrVar_SetNotifyThread(threadId, stringId);
                top -= 2;
                goto next_opcode;
            }
            Scr_Error("first parameter of endon must evaluate to a string");
        }
        typeIndex = Scr_GetVarType(top->u.pointerValue);
        goto not_object_set_index_1;

    case OP_voidCodepos:
        top[1].type = VAR_PRECODEPOS;
        top++;
        goto next_opcode;

    case OP_switch:
        offset = (unsigned int)Cursor_ReadInt(&pos);
        pos += offset;
        switchCount = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        if (top->type == VAR_STRING) {
        switch_on_string:
            switchValue = (int)top->u.stringValue;
            ScrStr_Release(top->u.stringValue);
        } else if (top->type == VAR_INTEGER) {
            if (Scr_IsSmallNameIndex(top->u.intValue) == 0) {
                Scr_Error(va("switch index %d out of range", top->u.intValue));
                goto switch_on_string;
            }
            switchValue = (int)Scr_BiasName24(top->u.intValue);
        } else {
            Scr_Error(va("cannot switch on %s", g_scrTypeNames[top->type]));
        }
        if (switchCount != 0) {
            do {
                caseValue = Cursor_ReadIntAlias1(&pos);
                caseTarget = Cursor_ReadIntAlias2(&pos);
                if (caseValue == switchValue) {
                    pos = caseTarget;
                    goto pop_top;
                }
                switchCount--;
            } while (switchCount != 0);
            if (caseValue == 0)
                pos = caseTarget;
        }
        break;

    case OP_endswitch:
        switchCount = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        Cursor_AdvanceDwords(&pos, (int)(switchCount * 2));
        goto next_opcode;

    case OP_vector:
        top -= 2;
        Scr_CastVector(top);
        goto next_opcode;

    case OP_NOP:
        goto next_opcode;

    case OP_abort:
        vmRetLocalId = 0;
        goto vm_exit;

    case OP_object: {
        int objectClassIndex;
        int objectNameIndex;

        top++;
        objectClassIndex = Cursor_ReadInt(&pos);
        objectNameIndex = Cursor_ReadInt(&pos);
        top->u.pointerValue = Scr_FindClassEntityVar(objectNameIndex, objectClassIndex);
        if (top->u.pointerValue == 0) {
            top->type = VAR_UNDEFINED;
            Scr_Error("unknown object");
        }
        goto push_object_ref;
    }

    case OP_thread_object:
        top++;
        top->u.pointerValue = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
    push_object_ref:
        top->type = VAR_POINTER;
        ScrVar_AddRef(top->u.pointerValue);
        goto next_opcode;

    case OP_EvalLocalVariable:
        top++;
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        variableId = ScrVar_FindVarValueId(localId, stringId);
        ScrVar_ReadVarValue(&value, variableId);
        *top = value;
        goto next_opcode;

    case OP_EvalLocalVariableRef:
        stringId = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        fieldValueId = ScrVar_FindVarValueId(localId, stringId);
        if (fieldValueId == 0) {
            Scr_Error("cannot create a new local variable in the debugger");
            pos++;
        }
        goto next_opcode;

    case OP_prof_begin:
        pos++;
        goto next_opcode;

    case OP_prof_end:
        pos++;
        goto next_opcode;
    }

pop_top:
    top--;
    goto next_opcode;

/* ---- shared tails ---------------------------------------------------------- */

call_enter:
    scrVmPub.function_frame->localVarCount = localVarCount;
    frame = scrVmPub.function_frame;
    localVarCount = 0;
    scrVmPub.function_count++;
    scrVmPub.function_frame++;
    frame[1].localId = localId;
    goto next_opcode;

default_call:
    /* inline thread call: remember the marker slot and mark it precodepos; also reached for opcodes > 0x86 */
    scrVmPub.function_frame->callMarker = startTop;
    scrVmPub.function_frame->callMarkerType = startTop->type;
    startTop->type = VAR_PRECODEPOS;
    nestedCalls++;
    goto call_enter;

thread_finished:
    startTop[1].type = VAR_UNDEFINED;
thread_return:
    if (nestedCalls == 0) {
        vmRetLocalId = localId;
        goto vm_exit;
    }
    nestedCalls--;
    ScrVar_Release(localId);
    frame = scrVmPub.function_frame;
    pos = frame->pos;
    localId = frame->localId;
    localVarCount = frame->localVarCount;
    startTop = frame->startTop;
    frame->callMarker->type = frame->callMarkerType;
    top = frame->callMarker + 1;
    goto next_opcode;

vm_exit:
    g_script_error_level--;
    return vmRetLocalId;

/* ---- "is not an object" error exits --------------------------------------- */

not_object_of_id:
    typeIndex = Scr_GetVarType(objectId);
    goto not_object_report;

not_object_of_top_type:
    typeIndex = (unsigned int)top->type;
not_object_set_index_2:
    scrVarPub_error_index = 2;
    goto not_object_report;

not_object_set_index_1:
    scrVarPub_error_index = 1;
not_object_report:
    Scr_Error(va("%s is not an object", g_scrTypeNames[typeIndex]));
    /* Scr_Error returned (evaluation mode): continue with the same repair as after a longjmp */

/* ---- error recovery: runs after longjmp from Scr_ErrorInternal ------------- */

error_recovery:
    switch (opcode) {
    case OP_EvalLocalArrayRefCached0:
    case OP_EvalLocalArrayRefCached:
    case OP_EvalArrayRef:
    case OP_ClearArray:
    case OP_EvalLocalVariableRef:
        if (scrVarPub_error_index < 0)
            scrVarPub_error_index = 1;
        break;
    case OP_EvalSelfFieldVariable:
    case OP_EvalFieldVariable:
    case OP_ClearFieldVariable:
    case OP_SetVariableField:
    case OP_SetSelfFieldVariableField:
    case OP_inc:
    case OP_dec:
        scrVarPub_error_index = 0;
        break;
    case OP_CallBuiltin0:
    case OP_CallBuiltin1:
    case OP_CallBuiltin2:
    case OP_CallBuiltin3:
    case OP_CallBuiltin4:
    case OP_CallBuiltin5:
    case OP_CallBuiltin:
        if (scrVarPub_error_index > 0)
            scrVarPub_error_index = (scrVmPub.inparamcount - scrVarPub_error_index) + 1;
        break;
    case OP_CallBuiltinMethod0:
    case OP_CallBuiltinMethod1:
    case OP_CallBuiltinMethod2:
    case OP_CallBuiltinMethod3:
    case OP_CallBuiltinMethod4:
    case OP_CallBuiltinMethod5:
    case OP_CallBuiltinMethod:
        if (scrVarPub_error_index < 1) {
            if (scrVarPub_error_index < 0)
                scrVarPub_error_index = 1;
        } else {
            scrVarPub_error_index = (scrVmPub.inparamcount - scrVarPub_error_index) + 2;
        }
        break;
    default:
        break;
    }

    ScrRuntime_Error(pos, scrVarPub_error_index, g_scr_compile_error_message, scrVmGlob_dialog_error_message);
    Scr_ClearErrorMessage();

    switch (opcode) {
    case OP_EvalLocalArrayCached:
    case OP_EvalArray:
        Scr_RemoveRefToValue((struct VariableValue *)(top));
        top--;
        Scr_RemoveRefToValue((struct VariableValue *)(top));
        top->type = VAR_UNDEFINED;
        goto next_opcode;

    case OP_EvalLocalArrayRefCached0:
    case OP_EvalLocalArrayRefCached:
    case OP_EvalArrayRef:
    case OP_EvalLocalVariableRef:
        fieldValueId = Scr_ClearTempAndGetId();
        /* falls through */
    case OP_ClearArray:
    case OP_wait:
    recover_release_top:
        Scr_RemoveRefToValue((struct VariableValue *)(top));
        top--;
        goto next_opcode;

    case OP_GetSelfObject:
        objectId = Scr_ClearTempAndGetObject();
        goto next_opcode;

    case OP_EvalSelfFieldVariable:
    case OP_EvalFieldVariable:
        top->type = VAR_UNDEFINED;
        goto next_opcode;

    case OP_EvalSelfFieldVariableRef:
    case OP_EvalFieldVariableRef:
        fieldValueId = Scr_ClearTempAndGetId();
        goto next_opcode;

    case OP_ClearFieldVariable:
        if (scrVmPub.inparamcount != 0)
            scrVmPub.inparamcount = 0;
        goto next_opcode;

    case OP_checkclearparams:
        for (; top->type != VAR_PRECODEPOS; top--)
            Scr_RemoveRefToValue((struct VariableValue *)(top));
        top->type = VAR_CODEPOS;
        goto next_opcode;

    case OP_SetVariableField:
        if (scrVmPub.inparamcount != 0) {
            Scr_RemoveRefToValue((struct VariableValue *)(top));
            scrVmPub.inparamcount = 0;
        }
        goto pop_top;

    case OP_SetSelfFieldVariableField:
        Scr_RemoveRefToValue((struct VariableValue *)(top));
        scrVmPub.inparamcount = 0;
        goto pop_top;

    case OP_CallBuiltin0:
    case OP_CallBuiltin1:
    case OP_CallBuiltin2:
    case OP_CallBuiltin3:
    case OP_CallBuiltin4:
    case OP_CallBuiltin5:
    case OP_CallBuiltin:
    case OP_CallBuiltinMethod0:
    case OP_CallBuiltinMethod1:
    case OP_CallBuiltinMethod2:
    case OP_CallBuiltinMethod3:
    case OP_CallBuiltinMethod4:
    case OP_CallBuiltinMethod5:
    case OP_CallBuiltinMethod:
        Scr_ClearInParams();
        top = scrVmPub.top + 1;
        scrVmPub.top[1].type = VAR_UNDEFINED;
        goto next_opcode;

    case OP_PreScriptCallThenCall:
    case OP_ScriptFunctionCall:
    case OP_ScriptMethodCall:
        Cursor_ReadIntAlias2(&pos);
        /* falls through */
    case OP_ScriptFunctionCallPointer:
    case OP_ScriptMethodCallPointer:
        for (; top->type != VAR_PRECODEPOS; top--)
            Scr_RemoveRefToValue((struct VariableValue *)(top));
        top->type = VAR_UNDEFINED;
        goto next_opcode;

    case OP_ScriptThreadCall:
    case OP_ScriptMethodThreadCall:
        Cursor_ReadIntAlias2(&pos);
        /* falls through */
    case OP_ScriptThreadCallPointer:
    case OP_ScriptMethodThreadCallPointer:
        removeCount = (unsigned int)Cursor_ReadInt(&pos);
        while (removeCount != 0) {
            Scr_RemoveRefToValue((struct VariableValue *)(top));
            top--;
            removeCount--;
        }
        top[1].type = VAR_UNDEFINED;
        top++;
        goto next_opcode;

    case OP_CastFieldObject:
        objectId = Scr_ClearTempAndGetObject();
        goto pop_top;

    case OP_EvalLocalVariableObjectCached:
        pos++;
        objectId = Scr_ClearTempAndGetObject();
        goto next_opcode;

    case OP_JumpOnFalse:
    case OP_JumpOnTrue:
    case OP_JumpOnFalseExpr:
    case OP_JumpOnTrueExpr:
        Cursor_ReadShortZx(&pos);
        goto pop_top;

    case OP_jumpback:
        offset = (unsigned int)(Cursor_ReadShortZx(&pos) & 0xffff);
        pos -= offset;
        goto next_opcode;

    case OP_Binary66:
    case OP_Binary67:
    case OP_Binary68:
    case OP_Binary69:
    case OP_Binary6a:
    case OP_Binary6b:
    case OP_Binary6c:
    case OP_Binary6d:
    case OP_Binary6e:
    case OP_Binary6f:
    case OP_Binary70:
    case OP_Binary71:
    case OP_Binary72:
    case OP_Binary73:
    case OP_Binary74:
    case OP_Binary75:
        goto pop_top;

    case OP_waittill77:
        pos++;
        /* falls through */
    case OP_waittill78:
    case OP_endon:
        Scr_RemoveRefToValue((struct VariableValue *)(top));
        top--;
        goto recover_release_top;

    case OP_notify:
        for (; top->type != VAR_PRECODEPOS; top--)
            Scr_RemoveRefToValue((struct VariableValue *)(top));
        goto recover_release_top;

    case OP_switch:
        for (; switchCount != 0; switchCount--) {
            caseValue = Cursor_ReadIntAlias1(&pos);
            caseTarget = Cursor_ReadIntAlias2(&pos);
        }
        if (caseValue == 0)
            pos = caseTarget;
        goto recover_release_top;

    default:
        goto next_opcode;
    }
}
