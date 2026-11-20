#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_011_types.h"

// 0x0807085e  size=44  FUN_0807085e  status=translated  naming=inferred
// naming evidence: structure only: emits opcode 0x4e when the node is a function reference
void ScrCompile_EmitCallPrefix(ScriptNode *n)
{
    if (n->type == SCR_NODE_FUNC_REF) {
        EmitOpcode(0x4e, 1, 0);
    }
}

// 0x0807088a  size=150  FUN_0807088a  status=translated  naming=inferred
// naming evidence: dispatches on function-reference node kind (plain / file::function)
void ScrCompile_EmitCallTarget(ScriptNode *n, int argc, char isMethod, ScriptScope *scope)
{
    if (n->type == SCR_NODE_FUNC_REF) {
        EmitCallDispatch(n->a1.node, argc, isMethod, n->a2.u, scope);
    } else if (n->type == SCR_NODE_FILE_FUNC_REF) {
        EmitThreadCallDispatch(n->a1.node, argc, isMethod, n->a2.u, n->a3.u, scope);
    }
}

// 0x08070920  size=133  FUN_08070920  status=translated  naming=inferred
// naming evidence: returns the identifier id of a bare function reference that is not in the user function table (builtin candidate)
int ScrCompile_GetUnresolvedCalleeId(ScriptNode *n)
{
    int result = 0;
    if (n->type == SCR_NODE_FUNC_REF) {
        ScriptNode *path = n->a1.node;
        if (path->type == SCR_NODE_IDENT_PATH) {
            ScriptNode *ident = path->a1.node;
            if (ident->type == SCR_NODE_IDENT) {
                if (ScrVar_FindVarValueId((int)g_scr_current_file_symtab, ident->a1.i) == 0) {
                    result = ident->a1.i;
                }
            }
        }
    }
    return result;
}

// 0x080709a6  size=87  FUN_080709a6  status=translated  naming=inferred
// naming evidence: developer-block state machine (/# ... #/ comment blocks per sibling error strings)
void ScrCompile_DevBlockEnter(int *entered, int *savedPos)
{
    if (scrCompileMode == 0) {
        if (g_scr_developer_script == 0) {
            *savedPos = TempMemory_Reserve(0);
            scrCompileMode = 2;
        } else {
            scrCompileMode = 1;
        }
        *entered = 1;
    } else {
        *entered = 0;
    }
}

// 0x080709fe  size=48  FUN_080709fe  status=translated  naming=inferred
// naming evidence: counterpart of DevBlockEnter
void ScrCompile_DevBlockLeave(int entered, int *savedPos)
{
    if (entered == 1) {
        scrCompileMode = 0;
        if (g_scr_developer_script == 0) {
            TempMemory_SetPos(*savedPos);
        }
    }
}

// 0x08070a2e  size=36  FUN_08070a2e  status=translated  naming=unknown
// naming evidence: maps developer flag to function-kind tag 7 (normal) or 0xc (developer)
int ScrCompile_DevFlagToFuncKind(int devFlag)
{
    if (devFlag == 0) {
        return 7;
    }
    return 0xc;
}

// 0x08070a52  size=37  FUN_08070a52  status=translated  naming=unknown
// naming evidence: kind tag != 7 means developer function
bool ScrCompile_FuncKindIsDeveloper(int kind)
{
    return kind != 7;
}

// 0x08070a78  size=102  FUN_08070a78  status=translated  naming=unknown
// naming evidence: emits call opcode 0x3e+argc (argc<6) or generic 0x44 with explicit count byte
void ScrCompile_EmitCallOpcode(unsigned argc, int srcPos)
{
    int opcode;
    if ((int)argc < 6) {
        opcode = argc + 0x3e;
    } else {
        opcode = 0x44;
    }
    EmitOpcode(opcode, 1 - argc, 1);
    ScrDbg_PushLine(srcPos, 1);
    if (opcode == 0x44) {
        EmitByte(argc & 0xff);
    }
}

// 0x08070ade  size=99  FUN_08070ade  status=translated  naming=unknown
// naming evidence: method-call variant: opcode 0x45+argc or 0x4b with explicit count byte
void ScrCompile_EmitMethodCallOpcode(unsigned argc, int srcPos)
{
    int opcode;
    if ((int)argc < 6) {
        opcode = argc + 0x45;
    } else {
        opcode = 0x4b;
    }
    EmitOpcode(opcode, -(int)argc, 1);
    ScrDbg_PushLine(srcPos, 1);
    if (opcode == 0x4b) {
        EmitByte(argc & 0xff);
    }
}

// 0x08070b42  size=548  FUN_08070b42  status=translated  naming=inferred
// naming evidence: strings "parameter count exceeds 256" and the developer-command return-value error identify call compilation; builtin lookup via scrCompileFuncCallArrayId
void ScrCompile_FunctionCall(ScriptNode *funcRef, ScriptList *args, char resultUsed, ScriptScope *scope)
{
    int calleeId = ScrCompile_GetUnresolvedCalleeId(funcRef);
    if (calleeId != 0) {
        const char *name = ScrStr_TextFromHandle(calleeId);
        int srcPos = funcRef->a2.i;
        int isDeveloper;
        void *fn;
        int entered;
        int savedPos;
        int handle = ScrVar_FindVarValueId((int)scrCompileFuncCallArrayId, calleeId);
        if (handle == 0) {
            VariableValue entry;
            isDeveloper = 0;
            fn = GScr_GetBuiltinFunction(&name, &isDeveloper);
            handle = ScrVar_NewChildAVarId((int)scrCompileFuncCallArrayId, calleeId);
            entry.u.intValue = (int)fn;
            entry.type = ScrCompile_DevFlagToFuncKind(isDeveloper);
            ScrVar_SetVarValueReleasing(handle, &entry);
        } else {
            VariableValue entry;
            ScrVar_ReadVarValue(&entry, handle);
            fn = (void *)entry.u.intValue;
            isDeveloper = ScrCompile_FuncKindIsDeveloper(entry.type);
        }
        if (fn != 0) {
            int argc;
            if (isDeveloper == 1) {
                ScrCompile_DevBlockEnter(&isDeveloper, &savedPos);
                if (isDeveloper == 1 && resultUsed == 0) {
                    ScrCompile_ErrorInFile(srcPos, "return value of developer command can not be accessed if not in a /# ... #/ comment");
                    return;
                }
            }
            argc = ScrNodeList_CompileExprs(args, scope);
            if (argc > 0xff) {
                ScrCompile_ErrorInFile(srcPos, "parameter count exceeds 256");
                return;
            }
            Compile_ReleaseStringRef(calleeId);
            ScrCompile_EmitCallOpcode(argc, srcPos);
            EmitUnsignedShort(Scr_FindOrAddFuncTableEntry((int)fn) /* binary also pushes an unused 2nd arg (name) */ & 0xffff);
            ScrNodeList_AddDebugPositions(args);
            if (resultUsed != 0) {
                EmitDecTop();
            }
            ScrCompile_DevBlockLeave(isDeveloper, &savedPos);
            return;
        }
    }
    {
        int argc;
        ScrCompile_EmitCallPrefix(funcRef);
        argc = ScrNodeList_CompileExprs(args, scope);
        ScrCompile_EmitCallTarget(funcRef, argc, 0, scope);
        ScrNodeList_AddDebugPositions(args);
        if (resultUsed != 0) {
            EmitDecTop();
        }
    }
}

// 0x08070d66  size=622  FUN_08070d66  status=translated  naming=inferred
// naming evidence: method-call twin of FunctionCall using method table g_scr_builtin_method_cache and object expression node
void ScrCompile_MethodCall(ScriptNode *objExpr, ScriptNode *funcRef, ScriptList *args, int srcPosObj, char resultUsed, ScriptScope *scope)
{
    int calleeId = ScrCompile_GetUnresolvedCalleeId(funcRef);
    if (calleeId != 0) {
        const char *name = ScrStr_TextFromHandle(calleeId);
        int srcPos = funcRef->a2.i;
        int isDeveloper;
        void *fn;
        int entered;
        int savedPos;
        int handle = ScrVar_FindVarValueId((int)g_scr_builtin_method_cache, calleeId);
        if (handle == 0) {
            VariableValue entry;
            isDeveloper = 0;
            fn = (void *)Scr_GetMethod(&name, &isDeveloper);
            handle = ScrVar_NewChildAVarId((int)g_scr_builtin_method_cache, calleeId);
            entry.u.intValue = (int)fn;
            entry.type = ScrCompile_DevFlagToFuncKind(isDeveloper);
            ScrVar_SetVarValueReleasing(handle, &entry);
        } else {
            VariableValue entry;
            ScrVar_ReadVarValue(&entry, handle);
            fn = (void *)entry.u.intValue;
            isDeveloper = ScrCompile_FuncKindIsDeveloper(entry.type);
        }
        if (fn != 0) {
            int argc;
            if (isDeveloper == 1) {
                ScrCompile_DevBlockEnter(&isDeveloper, &savedPos);
                if (isDeveloper == 1 && resultUsed == 0) {
                    ScrCompile_ErrorInFile(srcPos, "return value of developer command can not be accessed if not in a /# ... #/ comment");
                    return;
                }
            }
            argc = ScrNodeList_CompileExprs(args, scope);
            ScrCompile_PrimaryExprPush(objExpr, scope);
            if (argc > 0xff) {
                ScrCompile_ErrorInFile(srcPos, "parameter count exceeds 256");
                return;
            }
            Compile_ReleaseStringRef(calleeId);
            ScrCompile_EmitMethodCallOpcode(argc, srcPos);
            EmitUnsignedShort(Scr_FindOrAddFuncTableEntry((int)fn) /* binary also pushes an unused 2nd arg (name) */ & 0xffff);
            ScrDbg_PushLine(srcPosObj, 0);
            ScrNodeList_AddDebugPositions(args);
            if (resultUsed != 0) {
                EmitDecTop();
            }
            ScrCompile_DevBlockLeave(isDeveloper, &savedPos);
            return;
        }
    }
    {
        int argc;
        ScrCompile_EmitCallPrefix(funcRef);
        argc = ScrNodeList_CompileExprs(args, scope);
        ScrCompile_PrimaryExprPush(objExpr, scope);
        ScrCompile_EmitCallTarget(funcRef, argc, 1, scope);
        ScrDbg_PushLine(srcPosObj, 0);
        ScrNodeList_AddDebugPositions(args);
        if (resultUsed != 0) {
            EmitDecTop();
        }
    }
}

// 0x08070fd4  size=283  FUN_08070fd4  status=translated  naming=inferred
// naming evidence: strings "normal script cannot reference a function in a /# ... #/ comment" and "unknown function" show forward function-reference resolution
void ScrCompile_ResolveFunctionRefs(int refTable, VariableValue *res, char devAllowed)
{
    int listHandle = ScrVar_FindVarValueId(refTable, 0);
    if (listHandle != 0) {
        VariableValue info;
        int i;
        ScrVar_ReadVarValue(&info, listHandle);
        for (i = 0; i < info.u.intValue; i++) {
            int entry = ScrVar_FindVarValueId(refTable, i + 2);
            int **slotp = (int **)ScrVar_GetValuePtr(entry);
            int kind = Scr_GetVarType(entry);
            int *target = *slotp;
            if (res->type == 0xc && kind == 7) {
                ScrCompile_ErrorAt((const char *)target, "normal script cannot reference a function in a /# ... #/ comment");
                return;
            }
            if (res->type == 0) {
                ScrCompile_ErrorAt((const char *)target, "unknown function");
                return;
            }
            if (devAllowed == 0 && *target == 1) {
                ScrCompile_ErrorAt((const char *)target, "unknown function");
                return;
            }
            *target = res->u.intValue;
        }
    }
}

// 0x080710f0  size=221  FUN_080710f0  status=translated  naming=inferred
// naming evidence: walks every entry of a table and resolves its pending function references
void ScrCompile_ResolveAllFunctionRefs(int root)
{
    VariableValue unresolved;
    int node;
    unresolved.u.intValue = 0;
    unresolved.type = 0;
    node = Scr_GetNextSibling(root);
    while (node != 0) {
        int refTable = Scr_GetVarValue(node);
        int handle = ScrVar_FindVarValueId(refTable, 1);
        if (handle == 0) {
            ScrCompile_ResolveFunctionRefs(refTable, &unresolved, 1);
        } else {
            VariableValue entry;
            ScrVar_ReadVarValue(&entry, handle);
            if (entry.type == 0xd) {
                ScrVar_SetVarValueReleasing(node, &unresolved);
            } else {
                ScrCompile_ResolveFunctionRefs(refTable, &entry, 1);
            }
        }
        node = Scr_GetNextSibling(node);
    }
}

// 0x080711ce  size=243  FUN_080711ce  status=translated  naming=observed
// naming evidence: strings "function '%s' already defined" and "... already defined in '%s'" (duplicate function definition check)
int ScrCompile_DefineFunction(int table, int nameId, int srcPos, int tag)
{
    int handle = ScrVar_GetNewVarValueId(table, 1);
    VariableValue current;
    ScrVar_ReadVarValue(&current, handle);
    if (current.type == 0) {
        VariableValue fresh;
        fresh.u.intValue = 0;
        fresh.type = tag;
        ScrVar_SetVarValue(handle, &fresh);
        return handle;
    }
    if (current.u.intValue == 0) {
        ScrCompile_ErrorInFile(srcPos, "function '%s' already defined", ScrStr_TextFromHandle(nameId));
    } else {
        int fileIdx = ScrDbg_FindFrameIndex(current.u.intValue);
        const char *nm = ScrStr_TextFromHandle(nameId);
        ScrCompile_ErrorInFile(srcPos, "function '%s' already defined in '%s'", nm, g_08287334[fileIdx].name);
    }
    return 0;
}

// 0x080712c2  size=56  FUN_080712c2  status=translated  naming=unknown
// naming evidence: stores the current code position into the function-start slot of a function table
void ScrCompile_RecordFunctionStart(int table)
{
    int handle = ScrVar_FindVarValueId(table, 1);
    int *slot = (int *)ScrVar_GetValuePtr(handle);
    *slot = TempMemory_Reserve(0);
}

// 0x080712fa  size=148  FUN_080712fa  status=translated  naming=inferred
// naming evidence: dispatch on call node kinds 0x17 (plain) and 0x18 (method)
void ScrCompile_CallNode(ScriptNode *n, char resultUsed, ScriptScope *scope)
{
    if (n->type == SCR_NODE_CALL) {
        ScrCompile_FunctionCall(n->a1.node, n->a2.list, resultUsed, scope);
    } else if (n->type == SCR_NODE_METHOD_CALL) {
        ScrCompile_MethodCall(n->a1.node, n->a2.node, n->a3.list, n->a4.i, resultUsed, scope);
    }
}

// 0x0807138e  size=177  FUN_0807138e  status=translated  naming=inferred
// naming evidence: statement form of a call node: compile the call (result unused) then emit the trailing op with the node's last slot
void ScrCompile_CallAsObject(ScriptNode *n, ScriptScope *scope)
{
    if (n->type == SCR_NODE_CALL) {
        ScrCompile_FunctionCall(n->a1.node, n->a2.list, 0, scope);
        EmitCastFieldObject(n->a3.u);
    } else if (n->type == SCR_NODE_METHOD_CALL) {
        ScrCompile_MethodCall(n->a1.node, n->a2.node, n->a3.list, n->a4.i, 0, scope);
        EmitCastFieldObject(n->a5.u);
    }
}

// 0x08071440  size=236  FUN_08071440  status=translated  naming=inferred
// naming evidence: string "type %s is not a float": builds a vector constant from three int/float values (components stored in reverse order)
void ScrCompile_MakeVectorValue(ScriptValue *src3, ScriptValue *out)
{
    float xyz[3];
    int i;
    for (i = 0; i <= 2; i++) {
        int type = src3[i].type;
        if (type == 5) {
            xyz[2 - i] = src3[i].v.f;
        } else if (type == 6) {
            xyz[2 - i] = (float)src3[i].v.i;
        } else {
            ScrCompile_ErrorInFile(src3[i].srcPos, "type %s is not a float", g_scrTypeNames[type]);
            return;
        }
    }
    out->type = 4;
    out->v.i = (int)Scr_CloneVector((const unsigned int *)xyz);
}

// 0x0807152c  size=220  FUN_0807152c  status=translated  naming=inferred
// naming evidence: string "expression list must have 1 or 3 parameters" appears in the sibling VectorExpr; constant-fold variant
char ScrCompile_FoldVectorList(ScriptList *list, int srcPos, ScriptValue *out)
{
    int count = ScrNodeList_Count(list);
    if (count == 1) {
        ScriptListItem *item = list->head;
        return ScrCompile_FoldConstExpr(item->ref->node, out);
    }
    if (count == 3) {
        ScriptValue vals[4];
        ScriptListItem *item;
        int n = 0;
        for (item = list->head; item != 0; item = item->next) {
            char ok = ScrCompile_FoldConstExpr(item->ref->node, &vals[n]);
            if (ok != 1) {
                return 0;
            }
            n++;
        }
        ScrCompile_MakeVectorValue(vals, out);
        out->srcPos = srcPos;
        return 1;
    }
    return 0;
}

// 0x08071608  size=362  FUN_08071608  status=translated  naming=observed
// naming evidence: string "expression list must have 1 or 3 parameters"
char ScrCompile_VectorExpr(ScriptList *list, int srcPos, ScriptValue *out, ScriptScope *scope)
{
    int count = ScrNodeList_Count(list);
    if (count == 1) {
        ScriptListItem *item = list->head;
        return ScrCompile_Expr(item->ref->node, out, scope);
    }
    if (count == 3) {
        char allConst = 1;
        ScriptValue tmp;
        ScriptListItem *item;
        for (item = list->head; item != 0; item = item->next) {
            if (allConst == 0) {
                ScrCompile_ExprPush(item->ref->node, scope);
            } else {
                allConst = ScrCompile_Expr(item->ref->node, &tmp, scope);
                if (allConst != 0) {
                    PushDeferredValue(&tmp);
                }
            }
        }
        if (allConst == 0) {
            EmitOpcode(0x7e, -2, 0);
            ScrDbg_PushLine(srcPos, 1);
            ScrNodeList_AddDebugPositions(list);
            return 0;
        }
        scrValueStackCount = scrValueStackCount - 3;
        ScrCompile_MakeVectorValue(&scrValueStack[scrValueStackCount], out);
        out->srcPos = srcPos;
        return 1;
    }
    ScrCompile_ErrorInFile(srcPos, "expression list must have 1 or 3 parameters");
    return 0;
}

// 0x08071772  size=85  FUN_08071772  status=translated  naming=observed
// naming evidence: string "not an object"
void ScrCompile_ObjectFromList(ScriptList *list, int srcPos, ScriptScope *scope)
{
    ScriptListItem *slot = ScrNodeList_GetSingle(list);
    if (slot == 0) {
        ScrCompile_ErrorInFile(srcPos, "not an object");
    } else {
        ScriptNodeRef *ref = slot->ref;
        ScrCompile_ObjectRefNode(ref->node, ref->srcPos, scope);
    }
}

// 0x080717c8  size=495  FUN_080717c8  status=translated  naming=inferred
// naming evidence: literal node kinds (int/float/negated/string/istring/true/false) folded into an output constant
char ScrCompile_FoldLiteral(ScriptNode *n, ScriptValue *out)
{
    switch (n->type) {
    case SCR_NODE_INT_LITERAL:
        ScrDeferred_SetInteger(n->a1.i, n->a2.i, out);
        return 1;
    case SCR_NODE_FLOAT_LITERAL:
        ScrDeferred_SetFloat(n->a1.u, n->a2.i, out);
        return 1;
    case SCR_NODE_NEG_INT_LITERAL:
        ScrDeferred_SetInteger(-n->a1.i, n->a2.i, out);
        return 1;
    case SCR_NODE_NEG_FLOAT_LITERAL:
        ScrDeferred_SetFloat(n->a1.u ^ 0x80000000u, n->a2.i, out);
        return 1;
    case SCR_NODE_STRING_LITERAL:
        ScrDeferred_SetString(n->a1.i, n->a2.i, out);
        return 1;
    case SCR_NODE_ISTRING_LITERAL:
        ScrDeferred_SetIString(n->a1.i, n->a2.i, out);
        return 1;
    case SCR_NODE_TYPE_1F:
        ScrDeferred_SetUndefined(n->a1.u, out);
        return 1;
    case SCR_NODE_EXPR_LIST:
        return ScrCompile_FoldVectorList(n->a1.list, n->a2.i, out);
    case SCR_NODE_LITERAL_FALSE:
        ScrDeferred_SetInteger(0, n->a1.i, out);
        return 1;
    case SCR_NODE_LITERAL_TRUE:
        ScrDeferred_SetInteger(1, n->a1.i, out);
        return 1;
    default:
        return 0;
    }
}

// 0x080719b8  size=482  FUN_080719b8  status=translated  naming=inferred
// naming evidence: primary-expression dispatch; returns 1 when a constant was produced in out
char ScrCompile_PrimaryExpr(ScriptNode *n, ScriptValue *out, ScriptScope *scope)
{
    switch (n->type) {
    case SCR_NODE_OBJECT_BASE:
        EmitAssignTarget(n->a1.node, scope);
        return 0;
    case SCR_NODE_IDENT_PATH:
        EmitGetFunction(n->a1.node, n->a2.u);
        return 0;
    case SCR_NODE_CALL_EXPR:
        ScrCompile_CallNode(n->a1.node, 0, scope);
        return 0;
    case 0x20:
        EmitGetSelf(n->a1.u);
        return 0;
    case 0x22:
        EmitGetLevel(n->a1.u);
        return 0;
    case 0x23:
        EmitGetGame(n->a1.u);
        return 0;
    case 0x24:
        EmitGetAnim(n->a1.u);
        return 0;
    case SCR_NODE_EXPR_LIST:
        return ScrCompile_VectorExpr(n->a1.list, n->a2.i, out, scope);
    case 0x34:
        EmitSize(n->a1.u, n->a2.u, scope);
        return 0;
    case 0x42:
        EmitEmptyArray(n->a1.u);
        return 0;
    case 0x43:
        EmitGetAnimation(n->a1.u, n->a2.u);
        return 0;
    case 0x4a:
        EmitGetAnimTree(n->a1.u);
        return 0;
    default:
        return ScrCompile_FoldLiteral(n, out);
    }
}

// 0x08071b9a  size=52  FUN_08071b9a  status=translated  naming=unknown
// naming evidence: compile primary expression and push the folded constant if one was produced
void ScrCompile_PrimaryExprPush(ScriptNode *n, ScriptScope *scope)
{
    ScriptValue tmp;
    char folded = ScrCompile_PrimaryExpr(n, &tmp, scope);
    if (folded == 1) {
        FlushDeferredValue(&tmp);
    }
}

// 0x08071bce  size=164  FUN_08071bce  status=translated  naming=unknown
// naming evidence: binary node 0x2f: lhs, opcode 0x61 with 16-bit forward jump over rhs
void ScrCompile_OrExpr(ScriptNode *lhs, ScriptNode *rhs, int srcPosOp, int srcPosEnd, ScriptScope *scope)
{
    short *patch;
    short startPos;
    short endPos;
    ScrCompile_ExprPush(lhs, scope);
    EmitOpcode(0x61, -1, 0);
    ScrDbg_PushLine(srcPosOp, 0);
    EmitUnsignedShort(0);
    patch = (short *)scrLastEmitPtr;
    startPos = (short)TempMemory_Reserve(0);
    ScrCompile_ExprPush(rhs, scope);
    EmitCastBool(srcPosEnd);
    endPos = (short)TempMemory_Reserve(0);
    *patch = endPos - startPos;
}

// 0x08071c72  size=164  FUN_08071c72  status=translated  naming=unknown
// naming evidence: binary node 0x30: same as the 0x61 form with opcode 0x60
void ScrCompile_AndExpr(ScriptNode *lhs, ScriptNode *rhs, int srcPosOp, int srcPosEnd, ScriptScope *scope)
{
    short *patch;
    short startPos;
    short endPos;
    ScrCompile_ExprPush(lhs, scope);
    EmitOpcode(0x60, -1, 0);
    ScrDbg_PushLine(srcPosOp, 0);
    EmitUnsignedShort(0);
    patch = (short *)scrLastEmitPtr;
    startPos = (short)TempMemory_Reserve(0);
    ScrCompile_ExprPush(rhs, scope);
    EmitCastBool(srcPosEnd);
    endPos = (short)TempMemory_Reserve(0);
    *patch = endPos - startPos;
}

// 0x08071d16  size=205  FUN_08071d16  status=translated  naming=inferred
// naming evidence: constant folding of a binary operator; error text comes from g_scr_compile_error_message via "%s"
int ScrCompile_FoldBinaryConst(ScriptNode *a, ScriptNode *b, int op, int srcPos, ScriptValue *out)
{
    ScriptValue v1;
    ScriptValue v2;
    if (ScrCompile_FoldConstExpr(a, &v1) != 1) {
        return 0;
    }
    if (ScrCompile_FoldConstExpr(b, &v2) != 1) {
        return 0;
    }
    Scr_AddRefToValue((struct VariableValue *)&v1);
    Scr_AddRefToValue((struct VariableValue *)&v2);
    Scr_EvalBinaryOperator(op, (VariableValue *)&v1, (VariableValue *)&v2);
    if (g_scr_compile_error_message == 0) {
        out->v = v1.v;
        out->type = v1.type;
        out->srcPos = srcPos;
        return 1;
    }
    ScrCompile_ErrorInFile(srcPos, "%s", g_scr_compile_error_message);
    return 0;
}

// 0x08071de4  size=273  FUN_08071de4  status=translated  naming=inferred
// naming evidence: binary operator expression: folds when both operands are constant, otherwise emits operand code and the operator opcode
int ScrCompile_BinaryExpr(ScriptNode *a, ScriptNode *b, int op, int srcPos, ScriptValue *out, ScriptScope *scope)
{
    ScriptValue v1;
    ScriptValue v2;
    if (ScrCompile_Expr(a, &v1, scope) == 1) {
        PushDeferredValue(&v1);
        if (ScrCompile_Expr(b, &v2, scope) == 1) {
            PopDeferredValue();
            Scr_EvalBinaryOperator(op, (VariableValue *)&v1, (VariableValue *)&v2);
            if (g_scr_compile_error_message == 0) {
                out->v = v1.v;
                out->type = v1.type;
                out->srcPos = srcPos;
                return 1;
            }
            ScrCompile_ErrorInFile(srcPos, "%s", g_scr_compile_error_message);
            return 0;
        }
    } else {
        ScrCompile_ExprPush(b, scope);
    }
    EmitOpcode((int)(char)op, -1, 0);
    ScrDbg_PushLine(srcPos, 0);
    return 0;
}

// 0x08071ef6  size=134  FUN_08071ef6  status=translated  naming=unknown
// naming evidence: lvalue read, rhs, operator opcode, then store back (compound assignment shape)
void ScrCompile_CompoundAssign(ScriptNode *lvalue, ScriptNode *rhs, char op, int srcPos, ScriptScope *scope)
{
    scrCompileKeepStringRefs = 1;
    EmitAssignTarget(lvalue, scope);
    scrCompileKeepStringRefs = 0;
    ScrCompile_ExprPush(rhs, scope);
    EmitOpcode((int)op, -1, 0);
    ScrDbg_PushLine(srcPos, 0);
    ScrCompile_StoreLvalue(lvalue, scope);
    EmitSetVariableField(srcPos);
}

// 0x08071f7c  size=26  FUN_08071f7c  status=translated  naming=unknown
// naming evidence: thin wrapper forwarding to LvalueVariant
void ScrCompile_LvalueVariantThunk(ScriptNode *lvalue, ScriptScope *scope)
{
    ScrCompile_LvalueVariant(lvalue, scope);
}

// 0x08071f96  size=138  FUN_08071f96  status=translated  naming=inferred
// naming evidence: constant-only expression evaluation (wrapper node 6 or binary op 0x31)
char ScrCompile_FoldConstExpr(ScriptNode *n, ScriptValue *out)
{
    if (n->type == SCR_NODE_WRAPPER) {
        return ScrCompile_FoldLiteral(n->a1.node, out);
    }
    if (n->type == SCR_NODE_BINARY_OP) {
        return (char)ScrCompile_FoldBinaryConst(n->a1.node, n->a2.node, n->a3.i, n->a4.i, out);
    }
    return 0;
}

// 0x08072020  size=404  FUN_08072020  status=translated  naming=inferred
// naming evidence: general expression compile; returns 1 if a constant result was left in out
char ScrCompile_Expr(ScriptNode *n, ScriptValue *out, ScriptScope *scope)
{
    switch (n->type) {
    case SCR_NODE_WRAPPER:
        return ScrCompile_PrimaryExpr(n->a1.node, out, scope);
    case SCR_NODE_OP_2F:
        ScrCompile_OrExpr(n->a1.node, n->a2.node, n->a3.i, n->a4.i, scope);
        return 0;
    case SCR_NODE_OP_30:
        ScrCompile_AndExpr(n->a1.node, n->a2.node, n->a3.i, n->a4.i, scope);
        return 0;
    case SCR_NODE_BINARY_OP:
        return (char)ScrCompile_BinaryExpr(n->a1.node, n->a2.node, n->a3.i, n->a4.i, out, scope);
    case 0x32:
        ScrCompile_ExprPush(n->a1.node, scope);
        EmitBoolNot(n->a2.u);
        return 0;
    case 0x33:
        ScrCompile_ExprPush(n->a1.node, scope);
        EmitBoolComplement(n->a2.u);
        return 0;
    default:
        return 0;
    }
}

// 0x080721b4  size=52  FUN_080721b4  status=translated  naming=unknown
// naming evidence: compile expression and push the folded constant if one was produced
void ScrCompile_ExprPush(ScriptNode *n, ScriptScope *scope)
{
    ScriptValue tmp;
    char folded = ScrCompile_Expr(n, &tmp, scope);
    if (folded == 1) {
        FlushDeferredValue(&tmp);
    }
}

// 0x080721e8  size=272  FUN_080721e8  status=translated  naming=observed
// naming evidence: strings "not an lvalue" and "$ and self field can only be used in the script debugger"
void ScrCompile_StoreLvalue(ScriptNode *n, ScriptScope *scope)
{
    int type = n->type;
    if (type == SCR_NODE_LVALUE_0F) {
        EmitEvalFieldVariableRef(n->a1.u, n->a2.u, n->a3.u, scope);
    } else if (type < 0x10) {
        if (type == SCR_NODE_LVALUE_PLAIN) {
            EmitEvalLocalVariableRefCached(n->a1.i, n->a2.u, scope);
        } else if (type == SCR_NODE_LVALUE_0D) {
            ScrCompile_EmitEvalArrayRefExpr(n->a1.u, n->a2.u, n->a3.u, n->a4.u, scope);
        }
    } else if (type == 0x35 || type == 0x4f) {
        const char *msg;
        if (scrCompileDebuggerMode == 0) {
            msg = "not an lvalue";
        } else {
            msg = "$ and self field can only be used in the script debugger";
        }
        ScrCompile_ErrorInFile(n->a2.i, msg);
    }
}

// 0x080722f8  size=90  FUN_080722f8  status=translated  naming=unknown
// naming evidence: lvalue node kinds 4 and 0xd dispatch
void ScrCompile_LvalueVariant(ScriptNode *n, ScriptScope *scope)
{
    if (n->type == SCR_NODE_LVALUE_PLAIN) {
        ScrLocalBlock_AddVarThunk2(n->a1.i, n->a2.u, scope);
    } else if (n->type == SCR_NODE_LVALUE_0D) {
        ScrCompile_LvalueFromObjectBaseThunk(n->a1.node, scope);
    }
}

// 0x08072352  size=92  FUN_08072352  status=translated  naming=observed
// naming evidence: string "not an lvalue"
void ScrCompile_StoreLvalueChecked(ScriptNode *n, int srcPos, ScriptScope *scope)
{
    if (n->type == SCR_NODE_OBJECT_BASE) {
        ScrCompile_StoreLvalue(n->a1.node, scope);
    } else if (n->type == 0x23) {
        EmitGetGameRef(n->a1.u);
    } else {
        ScrCompile_ErrorInFile(srcPos, "not an lvalue");
    }
}

// 0x080723ae  size=43  FUN_080723ae  status=translated  naming=unknown
// naming evidence: object-base node: forward its child to LvalueVariant
void ScrCompile_LvalueFromObjectBase(ScriptNode *n, ScriptScope *scope)
{
    if (n->type == SCR_NODE_OBJECT_BASE) {
        ScrCompile_LvalueVariant(n->a1.node, scope);
    }
}

// 0x080723da  size=215  FUN_080723da  status=translated  naming=observed
// naming evidence: string "not an object"
void ScrCompile_ObjectExpr(ScriptNode *n, int srcPos, ScriptScope *scope)
{
    switch (n->type) {
    case SCR_NODE_OBJECT_BASE:
        EmitAssignTarget(n->a1.node, scope);
        EmitCastFieldObject(n->a2.u);
        break;
    case SCR_NODE_CALL_EXPR:
        ScrCompile_CallAsObject(n->a1.node, scope);
        break;
    case 0x20:
        EmitGetSelfObject(n->a1.u);
        break;
    case 0x22:
        EmitGetLevelObject(n->a1.u);
        break;
    case 0x24:
        EmitGetAnimObject(n->a1.u);
        break;
    case SCR_NODE_EXPR_LIST:
        ScrCompile_ObjectFromList(n->a1.list, srcPos, scope);
        break;
    default:
        ScrCompile_ErrorInFile(srcPos, "not an object");
        break;
    }
}

// 0x080724b2  size=76  FUN_080724b2  status=translated  naming=observed
// naming evidence: string "not an object"
void ScrCompile_ObjectRefNode(ScriptNode *n, int srcPos, ScriptScope *scope)
{
    if (n->type == SCR_NODE_WRAPPER) {
        ScrCompile_ObjectExpr(n->a1.node, n->a2.i, scope);
    } else {
        ScrCompile_ErrorInFile(srcPos, "not an object");
    }
}

// 0x080724fe  size=68  FUN_080724fe  status=translated  naming=inferred
// naming evidence: patches every pending entry of list g_scr_break_patch_list with (current position - recorded position); list is patched at loop end (break targets)
void ScrCompile_PatchBreakList(void)
{
    int pos = TempMemory_Reserve(0);
    ScriptPatchEntry *e;
    for (e = g_scr_break_patch_list; e != 0; e = e->next) {
        *e->slot = pos - e->pos;
    }
}

// 0x08072542  size=68  FUN_08072542  status=translated  naming=inferred
// naming evidence: same patching for list g_scr_continue_patch_list just before the loop back-jump (continue targets)
void ScrCompile_PatchContinueList(void)
{
    int pos = TempMemory_Reserve(0);
    ScriptPatchEntry *e;
    for (e = g_scr_continue_patch_list; e != 0; e = e->next) {
        *e->slot = pos - e->pos;
    }
}

// 0x08072586  size=17  FUN_08072586  status=translated  naming=unknown
// naming evidence: node type test
bool ScrNode_IsUndefined(ScriptNode *n)
{
    return n->type == SCR_NODE_TYPE_1F;
}

// 0x08072598  size=53  FUN_08072598  status=translated  naming=unknown
// naming evidence: wrapper node whose child is type 0x1f
int ScrNode_IsWrappedUndefined(ScriptNode *n)
{
    int result = 0;
    if (n->type == SCR_NODE_WRAPPER) {
        if (ScrNode_IsUndefined(n->a1.node)) {
            result = 1;
        }
    }
    return result;
}

// 0x080725ce  size=257  FUN_080725ce  status=translated  naming=observed
// naming evidence: strings "not an lvalue" and "$ and self field can only be used in the script debugger"; returns 0 for lvalue kind 4, else 1
int ScrCompile_TryClearAssign(ScriptNode *lvalue, ScriptArg a, ScriptScope *scope)
{
    int type = lvalue->type;
    if (type == SCR_NODE_LVALUE_0F) {
        EmitClearFieldVariable(lvalue->a1.u, lvalue->a2.u, lvalue->a3.u, a.u, scope);
    } else if (type < 0x10) {
        if (type == SCR_NODE_LVALUE_PLAIN) {
            return 0;
        }
        if (type == SCR_NODE_LVALUE_0D) {
            ScrCompile_EmitClearArrayExpr(lvalue->a1.u, lvalue->a2.u, lvalue->a3.u, lvalue->a4.u, scope);
        }
    } else if (type == 0x35 || type == 0x4f) {
        const char *msg;
        if (scrCompileDebuggerMode == 0) {
            msg = "not an lvalue";
        } else {
            msg = "$ and self field can only be used in the script debugger";
        }
        ScrCompile_ErrorInFile(lvalue->a2.i, msg);
    }
    return 1;
}

// 0x080726d0  size=101  FUN_080726d0  status=translated  naming=inferred
// naming evidence: assignment statement: fast path when rhs is a wrapped type-0x1f node, otherwise push rhs and store into the lvalue
void ScrCompile_Assign(ScriptNode *lvalue, ScriptNode *rhs, int srcPos, ScriptArg extra, ScriptScope *scope)
{
    if (ScrNode_IsWrappedUndefined(rhs) == 0 || ScrCompile_TryClearAssign(lvalue, extra, scope) == 0) {
        ScrCompile_ExprPush(rhs, scope);
        ScrCompile_StoreLvalue(lvalue, scope);
        EmitSetVariableField(srcPos);
    }
}

// 0x08072736  size=26  FUN_08072736  status=translated  naming=unknown
// naming evidence: wrapper forwarding (first and third argument) to LvalueVariant
void ScrCompile_LvalueVariantThunkSkipArg2(ScriptNode *lvalue, int unused, ScriptScope *scope)
{
    ScrCompile_LvalueVariant(lvalue, scope);
}

// 0x08072750  size=34  FUN_08072750  status=translated  naming=unknown
// naming evidence: call node whose result is used
void ScrCompile_CallNodeResultUsed(ScriptNode *n, ScriptScope *scope)
{
    ScrCompile_CallNode(n, 1, scope);
}

// 0x08072772  size=67  FUN_08072772  status=translated  naming=unknown
// naming evidence: marks scope state 3 if unset, compiles expression, emits op 0x532-helper
void ScrCompile_Return(ScriptNode *n, int srcPos, ScriptScope *scope)
{
    if (scope->state == 0) {
        scope->state = 3;
    }
    ScrCompile_ExprPush(n, scope);
    EmitReturn();
    ScrDbg_PushLine(srcPos, 0);
}

// 0x080727b6  size=49  FUN_080727b6  status=translated  naming=unknown
// naming evidence: marks scope state 3 if unset, emits helper 0x6e50e then position record
void ScrCompile_ReturnVoid(int srcPos, ScriptScope *scope)
{
    if (scope->state == 0) {
        scope->state = 3;
    }
    EmitEnd();
    ScrDbg_PushLine(srcPos, 1);
}

// 0x080727e8  size=111  FUN_080727e8  status=translated  naming=unknown
// naming evidence: expression then opcode 0x4c with three position records
void ScrCompile_Wait(ScriptNode *n, int srcPosA, int srcPosB, ScriptScope *scope)
{
    ScrCompile_ExprPush(n, scope);
    EmitOpcode(0x4c, -1, 0);
    ScrDbg_PushLine(srcPosB, 0);
    ScrDbg_PushLine(srcPosB, 0);
    ScrDbg_PushLine(srcPosA, 0);
}

// 0x08072858  size=74  FUN_08072858  status=translated  naming=unknown
// naming evidence: opcode 0x4d with two position records
void ScrCompile_WaittillFrameEnd(int srcPos)
{
    EmitOpcode(0x4d, 0, 0);
    ScrDbg_PushLine(srcPos, 1);
    ScrDbg_PushLine(srcPos, 0);
}

// 0x080728a2  size=224  FUN_080728a2  status=translated  naming=inferred
// naming evidence: if statement: condition, opcode 0x5e forward jump, body, patch 16-bit jump offset
void ScrCompile_IfStatement(ScriptNode *cond, ScriptNode *body, int srcPos, char flagA, int flagB, ScriptScope *scope, ScriptScope **bodyScope)
{
    short *patch;
    short startPos;
    short endPos;
    ScrCompile_ExprPush(cond, scope);
    EmitOpcode(0x5e, -1, 0);
    ScrDbg_PushLine(srcPos, 0);
    EmitUnsignedShort(0);
    patch = (short *)scrLastEmitPtr;
    startPos = (short)TempMemory_Reserve(0);
    ScrLocalBlock_RebaseInto(scope, *bodyScope);
    Scr_CompileStmt(body, flagA, flagB, *bodyScope);
    EmitFunctionEpilogue(flagA, flagB, *bodyScope);
    endPos = (short)TempMemory_Reserve(0);
    *patch = endPos - startPos;
}

// 0x08072982  size=72  FUN_08072982  status=translated  naming=unknown
// naming evidence: copies scope, analyses a block against the copy and merges it back
void ScrScope_AnalyzeBlock(ScriptNode *n, ScriptScope *scope, ScriptScope **out)
{
    ScrLocalBlock_Copy(scope, out);
    Scr_ScanStmt(n, *out);
    ScrLocalBlock_ApplyMerged(out, 1, scope);
}

// 0x080729ca  size=575  FUN_080729ca  status=translated  naming=inferred
// naming evidence: if/else statement: opcode 0x5e jump over the if body, opcode 0x62 (or helpers when flagA set) jump over the else body
void ScrCompile_IfElseStatement(ScriptNode *cond, ScriptNode *ifBody, ScriptNode *elseBody, int srcPosIf, int srcPosElse, char flagA, int flagB, ScriptScope *scope, ScriptScope **ifScope, ScriptScope **elseScope)
{
    short *patchIf;
    int startPos;
    ScriptScope *live[2];
    int liveCount;
    int savedCounter;
    int *patchElse;
    int elseStartPos;
    short endPos;

    ScrCompile_ExprPush(cond, scope);
    EmitOpcode(0x5e, -1, 0);
    ScrDbg_PushLine(srcPosIf, 0);
    EmitUnsignedShort(0);
    patchIf = (short *)scrLastEmitPtr;
    startPos = TempMemory_Reserve(0);
    ScrLocalBlock_RebaseInto(scope, *ifScope);
    Scr_CompileStmt(ifBody, flagA, flagB, *ifScope);
    EmitRemoveLocalVars(*ifScope, *ifScope);
    liveCount = ((*ifScope)->state == 0);
    if ((*ifScope)->state == 0) {
        live[0] = *ifScope;
    }
    savedCounter = g_scr_code_hash;
    if (flagA == 0) {
        EmitOpcode(0x62, 0, 0);
        ScrDbg_PushLine(srcPosElse, 1);
        EmitInt32(0);
        patchElse = (int *)scrLastEmitPtr;
        elseStartPos = TempMemory_Reserve(0);
    } else {
        EmitEnd();
        EmitInt32(0);
        ScrDbg_PushLine(flagB, 1);
        patchElse = 0;
        elseStartPos = 0;
    }
    g_scr_code_hash = savedCounter + 1;
    endPos = (short)TempMemory_Reserve(0);
    *patchIf = endPos - (short)startPos;
    ScrLocalBlock_RebaseInto(scope, *elseScope);
    Scr_CompileStmt(elseBody, flagA, flagB, *elseScope);
    EmitFunctionEpilogue(flagA, flagB, *elseScope);
    if ((*elseScope)->state == 0) {
        live[liveCount] = *elseScope;
        liveCount++;
    }
    if (flagA == 0) {
        *patchElse = TempMemory_Reserve(0) - elseStartPos;
    }
    ScrLocalBlock_IntersectInit(live, liveCount, scope);
}

// 0x08072c0a  size=254  FUN_08072c0a  status=translated  naming=unknown
// naming evidence: scope analysis for an if/else pair: merges the fall-through scopes of both branches
void ScrScope_AnalyzeIfElse(ScriptNode *ifBody, ScriptNode *elseBody, ScriptScope *scope, ScriptScope **ifScope, ScriptScope **elseScope)
{
    int liveCount = 0;
    int minState = 3;
    ScriptScope *live[2];

    ScrLocalBlock_Copy(scope, ifScope);
    Scr_ScanStmt(ifBody, *ifScope);
    if ((*ifScope)->state < 4) {
        minState = (*ifScope)->state;
        if (minState == 0) {
            live[0] = *ifScope;
            liveCount = 1;
        }
    }
    ScrLocalBlock_Copy(scope, elseScope);
    Scr_ScanStmt(elseBody, *elseScope);
    if ((*elseScope)->state <= minState) {
        minState = (*elseScope)->state;
        if (minState == 0) {
            live[liveCount] = *elseScope;
            liveCount++;
        }
    }
    if (scope->state == 0) {
        scope->state = minState;
    }
    ScrLocalBlock_MergeVars(live, liveCount, scope);
    ScrLocalBlock_ApplyMerged(live, liveCount, scope);
}

// 0x08072d08  size=77  FUN_08072d08  status=translated  naming=unknown
// naming evidence: appends a fall-through scope to list g_scr_scope_list_primary when recording is active
void ScrScope_PushPrimaryList(ScriptScope *s)
{
    if (s->state == 0 && g_scr_scope_list_primary != 0) {
        Compile_CheckSwitchCaseCount(*g_scr_scope_list_primary_count);
        g_scr_scope_list_primary[*g_scr_scope_list_primary_count] = s;
        *g_scr_scope_list_primary_count = *g_scr_scope_list_primary_count + 1;
    }
}

// 0x08072d56  size=77  FUN_08072d56  status=translated  naming=unknown
// naming evidence: appends a fall-through scope to list g_scr_scope_list_secondary when recording is active
void ScrScope_PushSecondaryList(ScriptScope *s)
{
    if (s->state == 0 && g_scr_scope_list_secondary != 0) {
        Compile_CheckSwitchCaseCount(*g_08206d0c);
        g_scr_scope_list_secondary[*g_08206d0c] = s;
        *g_08206d0c = *g_08206d0c + 1;
    }
}

// 0x08072da4  size=906  FUN_08072da4  status=translated  naming=observed
// naming evidence: string "conditional expression cannot be always false" (loop condition folding); while-loop emission with break/continue patch lists
void ScrCompile_WhileLoop(ScriptNode *cond, ScriptNode *body, int srcPosCond, int srcPosLoop, ScriptScope *scope, ScriptScope **bodyScope)
{
    char savedBreakFlagA = g_scr_can_break;
    char savedBreakFlagB = g_scr_can_break_dev;
    ScriptPatchEntry *savedBreakList = g_scr_break_patch_list;
    char savedContFlagA;
    char savedContFlagB;
    ScriptPatchEntry *savedContList;
    ScriptScope **savedListA;
    int *savedCountA;
    ScriptScope *savedScope;
    ScriptScope **savedListB;
    int *savedCountB;
    int countA;
    ScriptScope **listA;
    short *patchCond;
    int condStartPos;
    int loopStartPos;
    char alwaysTrue;
    ScriptValue cv;
    short endPos;

    g_scr_can_break = 0;
    g_scr_can_break_dev = 0;
    savedContFlagA = g_scr_can_continue;
    savedContFlagB = g_scr_can_continue_dev;
    savedContList = g_scr_continue_patch_list;
    g_scr_can_continue = 0;
    g_scr_can_continue_dev = 0;

    ScrLocalBlock_RebaseInto(scope, *bodyScope);
    ScrLocalBlock_EmitPendingVars(*bodyScope); /* binary also pushes unused srcPosLoop */
    scope->field4 = (*bodyScope)->field4;
    loopStartPos = TempMemory_Reserve(0);

    alwaysTrue = 0;
    if (ScrCompile_Expr(cond, &cv, scope) != 0) {
        if (cv.type == 6 || cv.type == 5) {
            Scr_CastBool((VariableValue *)&cv);
            if (cv.v.i == 0) {
                ScrCompile_ErrorInFile(srcPosCond, "conditional expression cannot be always false");
            }
            alwaysTrue = 1;
        } else {
            FlushDeferredValue(&cv);
        }
    }

    savedListB = g_scr_scope_list_primary;
    savedCountB = g_scr_scope_list_primary_count;
    savedScope = (ScriptScope *)g_scr_current_case_scope;
    savedListA = g_scr_scope_list_secondary;
    savedCountA = g_08206d0c;
    countA = 0;
    g_scr_scope_list_secondary = 0;
    g_scr_current_case_scope = (int *)*bodyScope;
    if (alwaysTrue == 0) {
        EmitOpcode(0x5e, -1, 0);
        ScrDbg_PushLine(srcPosCond, 0);
        EmitUnsignedShort(0);
        patchCond = (short *)scrLastEmitPtr;
        condStartPos = TempMemory_Reserve(0);
        listA = 0;
    } else {
        patchCond = 0;
        condStartPos = 0;
        listA = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
        g_scr_scope_list_primary_count = &countA;
    }
    g_scr_scope_list_primary = listA;
    g_scr_can_break = 1;
    g_scr_can_break_dev = (scrCompileMode != 0);
    g_scr_break_patch_list = 0;
    g_scr_can_continue = 1;
    g_scr_can_continue_dev = (scrCompileMode != 0);
    g_scr_continue_patch_list = 0;
    Scr_CompileStmt(body, 0, 0, *bodyScope);
    if ((*bodyScope)->state != 3) {
        (*bodyScope)->state = 0;
    }
    g_scr_can_break = 0;
    g_scr_can_break_dev = 0;
    g_scr_can_continue = 0;
    g_scr_can_continue_dev = 0;
    ScrCompile_PatchContinueList();
    EmitOpcode(99, 0, 0);
    ScrDbg_PushLine(srcPosLoop, 0);
    if (body->type == SCR_NODE_BLOCK) {
        ScrDbg_PushLine(body->a3.i, 1);
    }
    EmitUnsignedShort(0);
    endPos = (short)TempMemory_Reserve(0);
    *(short *)scrLastEmitPtr = endPos - (short)loopStartPos;
    if (patchCond != 0) {
        endPos = (short)TempMemory_Reserve(0);
        *patchCond = endPos - (short)condStartPos;
    }
    ScrCompile_PatchBreakList();
    g_scr_can_break = savedBreakFlagA;
    g_scr_can_break_dev = savedBreakFlagB;
    g_scr_break_patch_list = savedBreakList;
    g_scr_can_continue = savedContFlagA;
    g_scr_can_continue_dev = savedContFlagB;
    g_scr_continue_patch_list = savedContList;
    if (alwaysTrue != 0) {
        ScrLocalBlock_IntersectInit(listA, countA, scope);
    }
    g_scr_scope_list_primary = savedListB;
    g_scr_scope_list_primary_count = savedCountB;
    g_scr_current_case_scope = (int *)savedScope;
    g_scr_scope_list_secondary = savedListA;
    g_08206d0c = savedCountA;
}

// 0x0807312e  size=407  FUN_0807312e  status=translated  naming=inferred
// naming evidence: scope-analysis pass for a while loop (constant-true condition collects scopes of exits)
void ScrScope_AnalyzeWhile(ScriptNode *cond, ScriptNode *body, ScriptScope *scope, ScriptScope **bodyScope)
{
    char alwaysTrue = 0;
    ScriptValue cv;
    ScriptScope **savedListB;
    int *savedCountB;
    ScriptScope **savedListA;
    int *savedCountA;
    int countB = 0;
    int countA = 0;
    ScriptScope **listA;
    ScriptScope **listB;
    int i;

    if (ScrCompile_FoldConstExpr(cond, &cv) != 0) {
        if ((cv.type == 6 || cv.type == 5) && (Scr_CastBool((VariableValue *)&cv), cv.v.i != 0)) {
            alwaysTrue = 1;
        }
        Scr_RemoveRefToValue((struct VariableValue *)&cv);
    }
    savedCountA = g_08206d0c;
    savedListA = g_scr_scope_list_secondary;
    savedListB = g_scr_scope_list_primary;
    savedCountB = g_scr_scope_list_primary_count;
    listA = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
    g_scr_scope_list_secondary = listA;
    g_08206d0c = &countA;
    if (alwaysTrue == 0) {
        listB = 0;
    } else {
        listB = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
        g_scr_scope_list_primary_count = &countB;
    }
    g_scr_scope_list_primary = listB;
    ScrLocalBlock_Copy(scope, bodyScope);
    Scr_ScanStmt(body, *bodyScope);
    ScrScope_PushSecondaryList(*bodyScope);
    for (i = 0; i < countA; i++) {
        ScrLocalBlock_MergeVars(&listA[i], 1, scope);
    }
    if (alwaysTrue != 0) {
        ScrLocalBlock_MergeVars(listB, countB, scope);
    }
    ScrLocalBlock_ApplyMerged(bodyScope, 1, scope);
    g_scr_scope_list_primary = savedListB;
    g_scr_scope_list_primary_count = savedCountB;
    g_scr_scope_list_secondary = savedListA;
    g_08206d0c = savedCountA;
}

// 0x080732c6  size=1062  FUN_080732c6  status=translated  naming=observed
// naming evidence: string "conditional expression cannot be always false"; for-loop emission (init, condition, body, iterator, back-jump 99)
void ScrCompile_ForLoop(ScriptNode *init, ScriptNode *cond, ScriptNode *iter, ScriptNode *body, int srcPosCond, int srcPosLoop, ScriptScope *scope, ScriptScope **bodyScope, ScriptScope **iterScope)
{
    char savedBreakFlagA = g_scr_can_break;
    char savedBreakFlagB = g_scr_can_break_dev;
    ScriptPatchEntry *savedBreakList = g_scr_break_patch_list;
    char savedContFlagA;
    char savedContFlagB;
    ScriptPatchEntry *savedContList;
    ScriptScope **savedListB;
    int *savedCountB;
    ScriptScope *savedScope;
    ScriptScope **savedListA;
    int *savedCountA;
    int countA;
    int countB;
    ScriptScope **listA;
    ScriptScope **listB;
    short *patchCond;
    int condStartPos;
    int loopStartPos;
    char alwaysTrue;
    ScriptValue cv;
    short endPos;

    g_scr_can_break = 0;
    g_scr_can_break_dev = 0;
    savedContFlagA = g_scr_can_continue;
    savedContFlagB = g_scr_can_continue_dev;
    savedContList = g_scr_continue_patch_list;
    g_scr_can_continue = 0;
    g_scr_can_continue_dev = 0;

    Scr_CompileStmt(init, 0, 0, scope);
    ScrLocalBlock_RebaseInto(scope, *bodyScope);
    ScrLocalBlock_EmitPendingVars(*bodyScope); /* binary also pushes unused srcPosLoop */
    scope->field4 = (*bodyScope)->field4;
    ScrLocalBlock_RebaseInto(scope, *iterScope);
    loopStartPos = TempMemory_Reserve(0);

    if (cond->type == SCR_NODE_COND_STMT) {
        alwaysTrue = 0;
        if (ScrCompile_Expr(cond->a1.node, &cv, scope) != 0) {
            if (cv.type == 6 || cv.type == 5) {
                Scr_CastBool((VariableValue *)&cv);
                if (cv.v.i == 0) {
                    ScrCompile_ErrorInFile(srcPosCond, "conditional expression cannot be always false");
                }
                alwaysTrue = 1;
            } else {
                FlushDeferredValue(&cv);
            }
        }
    } else {
        alwaysTrue = 1;
    }

    savedListB = g_scr_scope_list_primary;
    savedCountB = g_scr_scope_list_primary_count;
    savedScope = (ScriptScope *)g_scr_current_case_scope;
    savedListA = g_scr_scope_list_secondary;
    savedCountA = g_08206d0c;
    countB = 0;
    countA = 0;
    listA = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
    g_scr_scope_list_secondary = listA;
    g_08206d0c = &countA;
    g_scr_current_case_scope = (int *)*bodyScope;
    if (alwaysTrue == 0) {
        EmitOpcode(0x5e, -1, 0);
        ScrDbg_PushLine(srcPosCond, 0);
        EmitUnsignedShort(0);
        patchCond = (short *)scrLastEmitPtr;
        condStartPos = TempMemory_Reserve(0);
        listB = 0;
    } else {
        patchCond = 0;
        condStartPos = 0;
        listB = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
        g_scr_scope_list_primary_count = &countB;
    }
    g_scr_scope_list_primary = listB;
    g_scr_can_break = 1;
    g_scr_can_break_dev = (scrCompileMode != 0);
    g_scr_break_patch_list = 0;
    g_scr_can_continue = 1;
    g_scr_can_continue_dev = (scrCompileMode != 0);
    g_scr_continue_patch_list = 0;
    Scr_CompileStmt(body, 0, 0, *bodyScope);
    ScrScope_PushSecondaryList(*bodyScope);
    g_scr_can_break = 0;
    g_scr_can_break_dev = 0;
    g_scr_can_continue = 0;
    g_scr_can_continue_dev = 0;
    ScrCompile_PatchContinueList();
    ScrLocalBlock_IntersectInit(listA, countA, *iterScope);
    Scr_CompileStmt(iter, 0, 0, *iterScope);
    EmitOpcode(99, 0, 0);
    ScrDbg_PushLine(srcPosLoop, 0);
    if (body->type == SCR_NODE_BLOCK) {
        ScrDbg_PushLine(body->a3.i, 1);
    }
    EmitUnsignedShort(0);
    endPos = (short)TempMemory_Reserve(0);
    *(short *)scrLastEmitPtr = endPos - (short)loopStartPos;
    if (patchCond != 0) {
        endPos = (short)TempMemory_Reserve(0);
        *patchCond = endPos - (short)condStartPos;
    }
    ScrCompile_PatchBreakList();
    g_scr_can_break = savedBreakFlagA;
    g_scr_can_break_dev = savedBreakFlagB;
    g_scr_break_patch_list = savedBreakList;
    g_scr_can_continue = savedContFlagA;
    g_scr_can_continue_dev = savedContFlagB;
    g_scr_continue_patch_list = savedContList;
    if (alwaysTrue != 0) {
        ScrLocalBlock_IntersectInit(listB, countB, scope);
    }
    g_scr_scope_list_primary = savedListB;
    g_scr_scope_list_primary_count = savedCountB;
    g_scr_current_case_scope = (int *)savedScope;
    g_scr_scope_list_secondary = savedListA;
    g_08206d0c = savedCountA;
}

// 0x080736ec  size=534  FUN_080736ec  status=translated  naming=inferred
// naming evidence: scope-analysis pass for a for loop (init, optional constant-true condition, body, iterator)
void ScrScope_AnalyzeFor(ScriptNode *init, ScriptNode *cond, ScriptNode *iter, ScriptNode *body, ScriptScope *scope, ScriptScope **bodyScope, ScriptScope **iterScope)
{
    char alwaysTrue;
    ScriptValue cv;
    ScriptScope **savedListB;
    int *savedCountB;
    ScriptScope **savedListA;
    int *savedCountA;
    int countB = 0;
    int countA = 0;
    ScriptScope **listA;
    ScriptScope **listB;
    int i;

    Scr_ScanStmt(init, scope);
    if (cond->type == SCR_NODE_COND_STMT) {
        alwaysTrue = 0;
        if (ScrCompile_FoldConstExpr(cond->a1.node, &cv) != 0) {
            if ((cv.type == 6 || cv.type == 5) && (Scr_CastBool((VariableValue *)&cv), cv.v.i != 0)) {
                alwaysTrue = 1;
            }
            Scr_RemoveRefToValue((struct VariableValue *)&cv);
        }
    } else {
        alwaysTrue = 1;
    }
    savedCountA = g_08206d0c;
    savedListA = g_scr_scope_list_secondary;
    savedListB = g_scr_scope_list_primary;
    savedCountB = g_scr_scope_list_primary_count;
    listA = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
    g_scr_scope_list_secondary = listA;
    g_08206d0c = &countA;
    if (alwaysTrue == 0) {
        listB = 0;
    } else {
        listB = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
        g_scr_scope_list_primary_count = &countB;
    }
    g_scr_scope_list_primary = listB;
    ScrLocalBlock_Copy(scope, bodyScope);
    ScrLocalBlock_Copy(scope, iterScope);
    Scr_ScanStmt(body, *bodyScope);
    ScrScope_PushSecondaryList(*bodyScope);
    for (i = 0; i < countA; i++) {
        ScrLocalBlock_MergeVars(&listA[i], 1, scope);
    }
    Scr_ScanStmt(iter, *iterScope);
    ScrLocalBlock_MergeVars(iterScope, 1, scope);
    ScrLocalBlock_ApplyMerged(iterScope, 1, scope);
    if (alwaysTrue != 0) {
        ScrLocalBlock_MergeVars(listB, countB, scope);
    }
    ScrLocalBlock_ApplyMerged(bodyScope, 1, scope);
    g_scr_scope_list_primary = savedListB;
    g_scr_scope_list_primary_count = savedCountB;
    g_scr_scope_list_secondary = savedListA;
    g_08206d0c = savedCountA;
}
