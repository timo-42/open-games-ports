/*
 * chunk_012: Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), 73 functions,
 * 0x08073902..0x08076d25. Script (GSC) compiler code generation and front end, plus the
 * 16-bit free-bucket tree allocator and its statistics dump.
 *
 * Translation is from the binary's behaviour (decompiler pseudocode checked against
 * disassembly). Placeholder callees are fn_08xxxxxx with unresolved arity; they are
 * rewritten by the merge lane.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_012_types.h"

/* Chunk-internal forward declarations. */

// 0x08073902  size=98  FUN_08073902  status=translated  naming=inferred
// evidence: caller Scr_CompileStmt dispatch case 0x29; emits opcode 100 with (1,0) and patches label.
void Scr_CompileIncrement(struct ScriptNode *expr, int label, struct ScriptScope *ctx)
{
    scrCompileUninitIsError = 1;
    ScrCompile_StoreLvalue(expr, ctx);
    scrCompileUninitIsError = 0;
    EmitOpcode(100, 1, 0);
    ScrDbg_PushLine(label, 0);
    EmitSetVariableField(label);
}

// 0x08073964  size=26  FUN_08073964  status=translated  naming=inferred
// evidence: scan-pass partner of Scr_CompileIncrement/Op101 (cases 0x29/0x2a in Scr_ScanStmt).
void Scr_ScanExprStmt(struct ScriptNode *expr, struct ScriptScope *state)
{
    ScrCompile_LvalueVariant(expr, state);
}

// 0x0807397e  size=98  FUN_0807397e  status=translated  naming=inferred
// evidence: same shape as Scr_CompileIncrement but emits opcode 101 (case 0x2a).
void Scr_CompileDecrement(struct ScriptNode *expr, int label, struct ScriptScope *ctx)
{
    scrCompileUninitIsError = 1;
    ScrCompile_StoreLvalue(expr, ctx);
    scrCompileUninitIsError = 0;
    EmitOpcode(0x65, 1, 0);
    ScrDbg_PushLine(label, 0);
    EmitSetVariableField(label);
}

// 0x080739e0  size=66  FUN_080739e0  status=translated  naming=unknown
// evidence: walks a list of (first, second) pairs and calls EmitSafeSetVariableFieldCached on each; no string.
void Scr_EmitParamSets(struct ScriptListItem *head, struct ScriptScope *ctx)
{
    struct ScriptListItem *cell;
    for (cell = head->next; cell != 0; cell = cell->next) {
        struct ScriptNodeRef *pair = (struct ScriptNodeRef *)cell->item;
        EmitSafeSetVariableFieldCached(pair->first, pair->second, ctx);
    }
}

// 0x08073a22  size=66  FUN_08073a22  status=translated  naming=unknown
// evidence: same walk as Scr_EmitParamSets calling ScrLocalBlock_AddVarThunk1.
void Scr_AddVarList(struct ScriptListItem *head, struct ScriptScope *ctx)
{
    struct ScriptListItem *cell;
    for (cell = head->next; cell != 0; cell = cell->next) {
        struct ScriptNodeRef *pair = (struct ScriptNodeRef *)cell->item;
        ScrLocalBlock_AddVarThunk1(pair->first, pair->second, ctx);
    }
}

// 0x08073a64  size=66  FUN_08073a64  status=translated  naming=unknown
// evidence: same walk as Scr_EmitParamSets calling EmitSafeSetWaittillVariableFieldCached.
void Scr_EmitWaittillSets(struct ScriptListItem *head, struct ScriptScope *ctx)
{
    struct ScriptListItem *cell;
    for (cell = head->next; cell != 0; cell = cell->next) {
        struct ScriptNodeRef *pair = (struct ScriptNodeRef *)cell->item;
        EmitSafeSetWaittillVariableFieldCached(pair->first, pair->second, ctx);
    }
}

// 0x08073aa6  size=218  FUN_08073aa6  status=translated  naming=inferred
// evidence: loop-statement codegen (opcode 0x78, closing 0x34); no string.
void Scr_CompileWaittill(int label_head, int *seq_ref, int cont_label, int brk_label, struct ScriptScope *ctx)
{
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *first = seq->list;
    struct ScriptNodeRef *pair = (struct ScriptNodeRef *)first->item;

    ScrCompile_ExprPush((ScriptNode *)pair->first, ctx);
    ScrCompile_PrimaryExprPush((ScriptNode *)label_head, ctx);
    EmitOpcode(0x78, 0xfffffffe, 0);
    ScrDbg_PushLine(brk_label, 0);
    ScrDbg_PushLine(brk_label, 0);
    ScrDbg_PushLine(cont_label, 0);
    ScrDbg_PushLine(pair->second, 0);
    Scr_EmitWaittillSets(first, ctx);
    EmitOpcode(0x34, 0, 0);
}

// 0x08073b80  size=39  FUN_08073b80  status=translated  naming=unknown
// evidence: scan pass for Scr_CompileWaittill; forwards the seq list to Scr_AddVarList.
void Scr_ScanWaittill(int *seq_ref, struct ScriptScope *state)
{
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    Scr_AddVarList(seq->list, (ScriptScope *)state);
}

// 0x08073ba8  size=331  FUN_08073ba8  status=translated  naming=inferred
// evidence: multi-target loop codegen (opcode 0x77, closing 0x34), no string.
void Scr_CompileWaittillmatch(int label_head, int *seq_ref, int cont_label, int brk_label, struct ScriptScope *ctx)
{
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *cell;
    struct ScriptListItem *last;
    uint32 count = 0;

    cell = seq->list;
    while ((cell = cell->next) != 0) {
        struct ScriptNodeRef *pair = (struct ScriptNodeRef *)cell->item;
        ScrCompile_ExprPush((ScriptNode *)pair->first, ctx);
        count++;
    }

    cell = seq->list;
    ScrCompile_ExprPush((ScriptNode *)(((struct ScriptNodeRef *)cell->item)->first), ctx);
    ScrCompile_PrimaryExprPush((ScriptNode *)label_head, ctx);
    EmitOpcode(0x77, -2 - (int)count, 0);
    ScrDbg_PushLine(brk_label, 0);
    ScrDbg_PushLine(brk_label, 0);
    ScrDbg_PushLine(cont_label, 0);
    ScrDbg_PushLine(((struct ScriptNodeRef *)cell->item)->second, 0);
    last = cell;
    while ((last = last->next) != 0) {
        ScrDbg_PushLine(((struct ScriptNodeRef *)last->item)->second, 0);
    }
    EmitByte((int)(count & 0xff));
    EmitOpcode(0x34, 0, 0);
}

// 0x08073cf4  size=245  FUN_08073cf4  status=translated  naming=inferred
// evidence: list-of-cases codegen (opcode 0x7b, closing 0x79), no string.
void Scr_CompileNotify(int label_head, struct ScriptListItem **head_ref, int label_b, int label_c, struct ScriptScope *ctx)
{
    struct ScriptListItem *cell;
    struct ScriptListItem *last = 0;
    int count = 0;

    EmitOpcode(0x7b, 1, 0);
    ScrDbg_PushLine(label_b, 1);
    for (cell = *head_ref; cell != 0; cell = cell->next) {
        last = cell;
        ScrCompile_ExprPush((ScriptNode *)(((struct ScriptNodeRef *)cell->item)->first), ctx);
        count++;
    }
    ScrCompile_PrimaryExprPush((ScriptNode *)label_head, ctx);
    EmitOpcode(0x79, -2 - count, 0);
    ScrDbg_PushLine(label_c, 0);
    /* The binary dereferences the last cell even when the list is empty. Kept. */
    ScrDbg_PushLine(((struct ScriptNodeRef *)last->item)->second, 0);
    ScrDbg_PushLine(label_b, 0);
}

// 0x08073dea  size=110  FUN_08073dea  status=translated  naming=inferred
// evidence: single-expression statement codegen (opcode 0x7a), no string.
void Scr_CompileEndon(int a, int b, int c, int d, struct ScriptScope *ctx)
{
    ScrCompile_ExprPush((ScriptNode *)b, ctx);
    ScrCompile_PrimaryExprPush((ScriptNode *)a, ctx);
    EmitOpcode(0x7a, 0xfffffffe, 0);
    ScrDbg_PushLine(d, 0);
    ScrDbg_PushLine(c, 0);
}

// 0x08073e58  size=60  FUN_08073e58  status=translated  naming=unknown
// evidence: qsort comparator over 8-byte (value, label) records; unsigned, descending.
int Scr_CompareCaseKeyDesc(const void *a, const void *b)
{
    const uint32 *lhs = (const uint32 *)a;
    const uint32 *rhs = (const uint32 *)b;

    if (*rhs < *lhs) {
        return -1;
    }
    if (*lhs < *rhs) {
        return 1;
    }
    return 0;
}

// 0x08073e94  size=92  FUN_08073e94  status=translated  naming=unknown
// evidence: returns 1 for an empty list or one whose nodes all have kind 0x2d (only when g_scr_developer_script is 0).
int Scr_ListTailAllType2D(struct ScriptListItem *list)
{
    if (list == 0) {
        return 1;
    }
    if (g_scr_developer_script == 0) {
        for (; list != 0; list = list->next) {
            if (((struct ScriptNode *)list->item)->kind != 0x2d) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

// 0x08073ef0  size=188  FUN_08073ef0  status=translated  naming=inferred
// evidence: strings "case index %d out of range", "case expression must be an int or string".
void Scr_CompileCaseLabel(struct ScriptNode *label, int error_ctx)
{
    int case_value;

    if (label->kind == 7) {
        char in_range = (char)Scr_IsSmallNameIndex(label->op[0]);
        if (in_range == 0) {
            const char *msg = va("case index %d out of range", label->op[0]);
            ScrCompile_ErrorInFile(error_ctx, msg);
            return;
        }
        case_value = Scr_BiasName24(label->op[0]);
    } else {
        if (label->kind != 0xb) {
            ScrCompile_ErrorInFile(error_ctx, "case expression must be an int or string");
            return;
        }
        case_value = label->op[0];
        EmitStringRef(label->op[0], 1);
    }
    Scr_AddCaseEntry(case_value, error_ctx);
}

// 0x08073fac  size=27  FUN_08073fac  status=translated  naming=inferred
// evidence: default label registers a case entry with value 0 (see Scr_AddCaseEntry).
void Scr_AddDefaultCase(int error_ctx)
{
    Scr_AddCaseEntry(0, error_ctx);
}

// 0x08073fc8  size=584  FUN_08073fc8  status=translated  naming=inferred
// evidence: strings "missing case statement"; walks switch body, saves/restores pool and label slots.
void Scr_CompileSwitchBody(int *seq_ref, uint8 flag, int ctx, struct ScriptScope *label_state)
{
    int saved_pool = (int)g_scr_scope_list_primary;
    int *saved_counter = g_scr_scope_list_primary_count;
    int *saved_label = g_scr_current_case_scope;
    int pool_count = 0;
    uint8 has_default = 0;
    int pool_base;
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *cell;
    struct ScriptListItem *next;

    g_scr_scope_list_primary = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
    g_scr_scope_list_primary_count = &pool_count;
    g_scr_current_case_scope = 0;
    pool_base = (int)g_scr_scope_list_primary;
    cell = seq->list;

    for (;;) {
        if (cell == 0) {
            if (g_scr_current_case_scope != 0) {
                g_scr_can_break = 0;
                EmitRemoveLocalVars((ScriptScope *)g_scr_current_case_scope, (ScriptScope *)g_scr_current_case_scope);
            }
            if (has_default != 0) {
                if (g_scr_current_case_scope != 0) {
                    ScrScope_PushPrimaryList((ScriptScope *)g_scr_current_case_scope);
                }
                ScrLocalBlock_IntersectInit((ScriptScope **)pool_base, pool_count, label_state);
            }
            g_scr_current_case_scope = saved_label;
            g_scr_scope_list_primary_count = saved_counter;
            g_scr_scope_list_primary = (ScriptScope **)saved_pool;
            return;
        }

        next = cell->next;
        struct ScriptNode *node = (struct ScriptNode *)cell->item;

        if (node->kind == 0x3d || node->kind == 0x3e) {
            if (g_scr_current_case_scope != 0) {
                g_scr_can_break = 0;
                EmitRemoveLocalVars((ScriptScope *)g_scr_current_case_scope, (ScriptScope *)g_scr_current_case_scope);
            }
            if (node->kind == 0x3d) {
                g_scr_current_case_scope = (int *)node->op[2];
                Scr_CompileCaseLabel((struct ScriptNode *)node->op[0], node->op[1]);
            } else {
                g_scr_current_case_scope = (int *)node->op[1];
                has_default = 1;
                Scr_AddDefaultCase(node->op[0]);
            }
            ScrLocalBlock_RebaseInto(label_state, (ScriptScope *)g_scr_current_case_scope);
            g_scr_can_break = 1;
        } else {
            if (g_scr_current_case_scope == 0) {
                ScrCompile_ErrorInFile(ctx, "missing case statement");
                return;
            }
            uint8 tail_flag = 0;
            if (flag != 0 && Scr_ListTailAllType2D(next)) {
                tail_flag = 1;
            }
            Scr_CompileStmt(node, tail_flag, ctx, (ScriptScope *)g_scr_current_case_scope);
            if (g_scr_current_case_scope != 0 && *g_scr_current_case_scope != 0) {
                g_scr_current_case_scope = 0;
                g_scr_can_break = 0;
            }
        }
        cell = next;
    }
}

// 0x08074210  size=500  FUN_08074210  status=translated  naming=unknown
// evidence: builds the switch jump table from the case labels; scan helper for 0x3c nodes.
void Scr_BuildSwitchTable(int *seq_ref, struct ScriptScope *state)
{
    int min_value = 3;
    int saved_pool = (int)g_scr_scope_list_primary;
    int *saved_counter = g_scr_scope_list_primary_count;
    int counter = 0;
    int *pending = 0;
    uint8 saw_default = 0;
    int count = 0;
    int pool_base;
    int *table;
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *cell;

    g_scr_scope_list_primary = (ScriptScope **)Hunk_AllocateTempMemoryHigh(0x1000);
    g_scr_scope_list_primary_count = &counter;
    pool_base = (int)g_scr_scope_list_primary;
    table = (int *)Hunk_AllocateTempMemoryHigh(0x1000);

    for (cell = seq->list; cell != 0; cell = cell->next) {
        struct ScriptNode *node = (struct ScriptNode *)cell->item;

        if (node->kind == 0x3d || node->kind == 0x3e) {
            pending = 0;
            ScrLocalBlock_Copy(state, (ScriptScope **)(&pending));
            if (node->kind == 0x3d) {
                node->op[2] = (int)pending;
            } else {
                node->op[1] = (int)pending;
                saw_default = 1;
            }
        } else if (pending != 0) {
            Scr_ScanStmt(node, (ScriptScope *)pending);
            if (*pending != 0) {
                if (*pending == 2) {
                    *pending = 0;
                    min_value = 0;
                    Compile_CheckSwitchCaseCount(count);
                    table[count] = (int)pending;
                    count++;
                } else if (*pending <= min_value) {
                    min_value = *pending;
                }
                pending = 0;
            }
        }
    }

    if (saw_default) {
        if (pending != 0) {
            ScrScope_PushPrimaryList((ScriptScope *)pending);
            Compile_CheckSwitchCaseCount(count);
            table[count] = (int)pending;
            count++;
        }
        if (state->state == 0) {
            state->state = min_value;
        }
        ScrLocalBlock_MergeVars((ScriptScope **)pool_base, counter, state);
        ScrLocalBlock_ApplyMerged((ScriptScope **)table, count, state);
    }

    g_scr_scope_list_primary = (ScriptScope **)saved_pool;
    g_scr_scope_list_primary_count = saved_counter;
}

// 0x08074404  size=612  FUN_08074404  status=translated  naming=inferred
// evidence: string "duplicate case expression"; switch statement codegen with qsort of case values.
void Scr_CompileSwitchStmt(struct ScriptNode *expr, int body_ref, int end_label, uint8 flag, int loc, struct ScriptScope *ctx)
{
    uint8 saved_ce6 = g_scr_in_switch;
    struct cod2_case_entry *saved_ce8 = g_scr_switch_case_list;
    uint8 saved_cec = g_scr_can_break;
    uint8 saved_ced = g_scr_can_break_dev;
    struct ScriptPatchEntry *saved_cf0 = g_scr_break_patch_list;
    int *cursor;
    int *cursor2;
    int base;
    int *sorted;
    uint32 count;
    struct cod2_case_entry *entry;

    g_scr_in_switch = 0;
    g_scr_can_break = 0;
    g_scr_can_break_dev = 0;
    ScrCompile_ExprPush(expr, ctx);
    EmitOpcode(0x7c, 0xffffffff, 0);
    EmitInt32(0);
    cursor = (int *)scrLastEmitPtr;
    base = TempMemory_Reserve(0);
    g_scr_in_switch = (scrCompileMode != 0);
    g_scr_switch_case_list = 0;
    g_scr_can_break_dev = (scrCompileMode != 0);
    g_scr_break_patch_list = 0;
    Scr_CompileSwitchBody((int *)body_ref, flag, loc, ctx);
    g_scr_in_switch = 0;
    g_scr_can_break_dev = 0;
    EmitOpcode(0x7d, 0, 0);
    ScrDbg_PushLine(end_label, 0);
    EmitShort(0);
    cursor2 = (int *)scrLastEmitPtr;
    *cursor = (int)scrLastEmitPtr - base;

    sorted = (int *)TempMemory_ReserveAliasB(0);
    count = 0;
    for (entry = g_scr_switch_case_list; entry != 0; entry = entry->next) {
        EmitInt32(entry->value);
        EmitCodePos(entry->label);
        count++;
    }
    *(short *)cursor2 = (short)count;
    qsort(sorted, count, 8, Scr_CompareCaseKeyDesc);

    for (;;) {
        if ((int)count < 2) {
            ScrCompile_PatchBreakList();
            g_scr_break_patch_list = saved_cf0;
            g_scr_can_break_dev = saved_ced;
            g_scr_can_break = saved_cec;
            g_scr_switch_case_list = saved_ce8;
            g_scr_in_switch = saved_ce6;
            return;
        }
        if (sorted[0] == sorted[2]) {
            for (entry = g_scr_switch_case_list; entry != 0; entry = entry->next) {
                if (entry->value == sorted[0]) {
                    ScrCompile_ErrorInFile(entry->location, "duplicate case expression");
                    return;
                }
            }
        }
        count--;
        sorted += 2;
    }
}

// 0x08074668  size=93  FUN_08074668  status=translated  naming=unknown
// evidence: prepends a 0x10-byte case record to the active switch list.
void Scr_AddCaseEntry(int value, int location)
{
    if (scrCompileMode != 2) {
        struct cod2_case_entry *entry = (struct cod2_case_entry *)Hunk_AllocateTempMemoryHigh(0x10);
        entry->value = value;
        entry->label = TempMemory_Reserve(0);
        entry->location = location;
        entry->next = g_scr_switch_case_list;
        g_scr_switch_case_list = entry;
    }
}

// 0x080746c6  size=217  FUN_080746c6  status=translated  naming=inferred
// evidence: string "illegal break statement".
void Scr_CompileBreakStmt(int loc, struct ScriptScope *state)
{
    if (g_scr_can_break == 0 || state->state != 0) {
        ScrCompile_ErrorInFile(loc, "illegal break statement");
        return;
    }
    ScrScope_PushPrimaryList(state);
    EmitRemoveLocalVars(state, (ScriptScope *)g_scr_current_case_scope);
    state->state = 2;
    EmitOpcode(0x62, 0, 0);
    ScrDbg_PushLine(loc, 1);
    EmitInt32(0);
    {
        struct ScriptPatchEntry *jump = (struct ScriptPatchEntry *)Hunk_AllocateTempMemoryHigh(0xc);
        jump->patch_site = (int)scrLastEmitPtr;
        jump->label = TempMemory_Reserve(0);
        jump->next = g_scr_break_patch_list;
        g_scr_break_patch_list = jump;
    }
}

// 0x080747a0  size=215  FUN_080747a0  status=translated  naming=inferred
// evidence: string "illegal continue statement".
void Scr_CompileContinueStmt(int loc, struct ScriptScope *state)
{
    if (g_scr_can_continue == 0 || state->state != 0) {
        ScrCompile_ErrorInFile(loc, "illegal continue statement");
        return;
    }
    ScrScope_PushSecondaryList(state);
    EmitRemoveLocalVars(state, state);
    state->state = 1;
    EmitOpcode(0x62, 0, 0);
    ScrDbg_PushLine(loc, 1);
    EmitInt32(0);
    {
        struct ScriptPatchEntry *jump = (struct ScriptPatchEntry *)Hunk_AllocateTempMemoryHigh(0xc);
        jump->patch_site = (int)scrLastEmitPtr;
        jump->label = TempMemory_Reserve(0);
        jump->next = g_scr_continue_patch_list;
        g_scr_continue_patch_list = jump;
    }
}

// 0x08074878  size=5  FUN_08074878  status=translated  naming=unknown
// evidence: empty handler (kind 0x4b); the single operand is ignored.
void Scr_CompileNop(int unused)
{
    (void)unused;
}

// 0x0807487e  size=87  FUN_0807487e  status=translated  naming=unknown
// evidence: marker statement; emits op (with a trailing 0) only in dev mode (g_scr_developer_script != 0).
void Scr_CompileMarker(int a, int b, uint8 op)
{
    (void)b;
    if (g_scr_developer_script == 0) {
        Compile_ReleaseStringRef(a);
    } else {
        Compile_ReleaseStringRef(a);
        EmitOpcode(op, 0, 0);
        EmitByte(0);
    }
}

// 0x080748d6  size=34  FUN_080748d6  status=translated  naming=unknown
// evidence: Scr_CompileMarker with opcode 0x85 (kind 0x4c).
void Scr_CompileProfBegin(int a, int b)
{
    Scr_CompileMarker(a, b, 0x85);
}

// 0x080748f8  size=34  FUN_080748f8  status=translated  naming=unknown
// evidence: Scr_CompileMarker with opcode 0x86 (kind 0x4d).
void Scr_CompileProfEnd(int a, int b)
{
    Scr_CompileMarker(a, b, 0x86);
}

// 0x0807491a  size=1392  FUN_0807491a  status=translated  naming=inferred
// evidence: strings "illegal case statement", "illegal default statement"; statement dispatch on kind.
void Scr_CompileStmt(struct ScriptNode *node, uint8 flag, int ctx, struct ScriptScope *label_state)
{
    int *p = node->op;

    switch (node->kind) {
    case 2:
        { ScriptArg extra; extra.i = p[3]; ScrCompile_Assign((ScriptNode *)p[0], (ScriptNode *)p[1], p[2], extra, label_state); }
        break;
    case 0x19:
        ScrCompile_CallNodeResultUsed((ScriptNode *)p[0], label_state);
        break;
    case 0x1b:
        ScrCompile_Return((ScriptNode *)p[0], p[1], label_state);
        break;
    case 0x1c:
        ScrCompile_ReturnVoid(p[0], label_state);
        break;
    case 0x1d:
        ScrCompile_Wait((ScriptNode *)p[0], p[1], p[2], label_state);
        break;
    case 0x25:
        ScrCompile_IfStatement((ScriptNode *)p[0], (ScriptNode *)p[1], p[2], flag, ctx, label_state, (ScriptScope **)(&p[3]));
        break;
    case 0x26:
        ScrCompile_IfElseStatement((ScriptNode *)p[0], (ScriptNode *)p[1], (ScriptNode *)p[2], p[3], p[4], flag, ctx, label_state, (ScriptScope **)(&p[5]), (ScriptScope **)(&p[6]));
        break;
    case 0x27:
        ScrCompile_WhileLoop((ScriptNode *)p[0], (ScriptNode *)p[1], p[2], p[3], label_state, (ScriptScope **)(&p[4]));
        break;
    case 0x28:
        ScrCompile_ForLoop((ScriptNode *)p[0], (ScriptNode *)p[1], (ScriptNode *)p[2], (ScriptNode *)p[3], p[4], p[5], label_state, (ScriptScope **)(&p[6]), (ScriptScope **)(&p[7]));
        break;
    case 0x29:
        Scr_CompileIncrement((ScriptNode *)p[0], p[1], label_state);
        break;
    case 0x2a:
        Scr_CompileDecrement((ScriptNode *)p[0], p[1], label_state);
        break;
    case 0x2b:
        ScrCompile_CompoundAssign((ScriptNode *)p[0], (ScriptNode *)p[1], p[2], p[3], label_state);
        break;
    case 0x2c:
        Scr_CompileStmtList((int *)p[0], flag, ctx, label_state);
        break;
    case 0x2d:
        Scr_CompileDevBlock(p[0], p[1], label_state, &p[2]);
        break;
    case 0x37:
        Scr_CompileWaittill(p[0], (int *)p[1], p[2], p[3], label_state);
        break;
    case 0x38:
        Scr_CompileWaittillmatch(p[0], (int *)p[1], p[2], p[3], label_state);
        break;
    case 0x39:
        ScrCompile_WaittillFrameEnd(p[0]);
        break;
    case 0x3a:
        Scr_CompileNotify(p[0], (struct ScriptListItem **)p[1], p[2], p[3], label_state);
        break;
    case 0x3b:
        Scr_CompileEndon(p[0], p[1], p[2], p[3], label_state);
        break;
    case 0x3c:
        Scr_CompileSwitchStmt((ScriptNode *)p[0], p[1], p[2], flag, ctx, label_state);
        break;
    case 0x3d:
        ScrCompile_ErrorInFile(p[1], "illegal case statement");
        break;
    case 0x3e:
        ScrCompile_ErrorInFile(p[0], "illegal default statement");
        break;
    case 0x3f:
        Scr_CompileBreakStmt(p[0], ((ScriptScope *)(int *)label_state));
        break;
    case 0x40:
        Scr_CompileContinueStmt(p[0], ((ScriptScope *)(int *)label_state));
        break;
    case 0x4b:
        Scr_CompileNop(p[0]);
        break;
    case 0x4c:
        Scr_CompileProfBegin(p[0], p[1]);
        break;
    case 0x4d:
        Scr_CompileProfEnd(p[0], p[1]);
        break;
    default:
        break;
    }
}

// 0x08074e8a  size=567  FUN_08074e8a  status=translated  naming=unknown
// evidence: pre-pass over statements (state 1/2/3 markers); mirror of Scr_CompileStmt.
void Scr_ScanStmt(struct ScriptNode *node, struct ScriptScope *state)
{
    int *p = node->op;

    switch (node->kind) {
    case 2:
        ScrCompile_LvalueVariantThunkSkipArg2((ScriptNode *)p[0], p[1], state);
        break;
    case 0x1b:
    case 0x1c:
        if (state->state == 0) {
            state->state = 3;
        }
        break;
    case 0x25:
        ScrScope_AnalyzeBlock((ScriptNode *)p[1], state, (ScriptScope **)(&p[3]));
        break;
    case 0x26:
        ScrScope_AnalyzeIfElse((ScriptNode *)p[1], (ScriptNode *)p[2], state, (ScriptScope **)(&p[5]), (ScriptScope **)(&p[6]));
        break;
    case 0x27:
        ScrScope_AnalyzeWhile((ScriptNode *)p[0], (ScriptNode *)p[1], state, (ScriptScope **)(&p[4]));
        break;
    case 0x28:
        ScrScope_AnalyzeFor((ScriptNode *)p[0], (ScriptNode *)p[1], (ScriptNode *)p[2], (ScriptNode *)p[3], state, (ScriptScope **)(&p[6]), (ScriptScope **)(&p[7]));
        break;
    case 0x29:
    case 0x2a:
        Scr_ScanExprStmt((ScriptNode *)p[0], state);
        break;
    case 0x2b:
        ScrCompile_LvalueVariantThunk((ScriptNode *)p[0], state);
        break;
    case 0x2c:
        Scr_ScanStmtList((int *)p[0], state);
        break;
    case 0x2d:
        Scr_ScanStmtListTail((int *)p[0], state, &p[2]);
        break;
    case 0x37:
        Scr_ScanWaittill((int *)p[1], state);
        break;
    case 0x3c:
        Scr_BuildSwitchTable((int *)p[1], state);
        break;
    case 0x3f:
        ScrScope_PushPrimaryList(state);
        if (state->state == 0) {
            state->state = 2;
        }
        break;
    case 0x40:
        ScrScope_PushSecondaryList(state);
        if (state->state == 0) {
            state->state = 1;
        }
        break;
    default:
        break;
    }
}

// 0x080750c2  size=123  FUN_080750c2  status=translated  naming=unknown
// evidence: compiles a statement list; the flag is set when the remaining tail is all kind 0x2d.
void Scr_CompileStmtList(int *seq_ref, uint8 flag, int ctx, struct ScriptScope *label_state)
{
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *cell = seq->list;

    while (cell != 0) {
        struct ScriptListItem *next = cell->next;
        uint8 tail_flag = 0;
        if (flag != 0 && Scr_ListTailAllType2D(next)) {
            tail_flag = 1;
        }
        Scr_CompileStmt((struct ScriptNode *)cell->item, tail_flag, ctx, label_state);
        cell = next;
    }
}

// 0x0807513e  size=62  FUN_0807513e  status=translated  naming=unknown
// evidence: scan pass over a statement list (calls Scr_ScanStmt per item).
void Scr_ScanStmtList(int *seq_ref, struct ScriptScope *state)
{
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *cell;

    for (cell = seq->list; cell != 0; cell = cell->next) {
        Scr_ScanStmt((struct ScriptNode *)cell->item, state);
    }
}

// 0x0807517c  size=72  FUN_0807517c  status=translated  naming=unknown
// evidence: scan of a guarded block: saves state into slot, scans the list, then restores with flag 1.
void Scr_ScanStmtListTail(int *seq_ref, struct ScriptScope *state, int *slot)
{
    ScrLocalBlock_Copy(state, (ScriptScope **)slot);
    Scr_ScanStmtList(seq_ref, ((ScriptScope *)(int *)*slot));
    ScrLocalBlock_ApplyMerged((ScriptScope **)slot, 1, state);
}

// 0x080751c4  size=238  FUN_080751c4  status=translated  naming=inferred
// evidence: string "cannot recurse /#".
void Scr_CompileDevBlock(int seq_arg, int loc, struct ScriptScope *ctx, int *slot)
{
    int saved_b753c = g_scr_code_hash;

    if (scrCompileMode == 0) {
        ScrLocalBlock_RebaseInto(ctx, (ScriptScope *)(*slot));
        if (g_scr_developer_script == 0) {
            int pos = TempMemory_Reserve(0);
            scrCompileMode = 2;
            /* The binary passes a fifth stack word here that Scr_CompileStmtList ignores. */
            Scr_CompileStmtList((int *)seq_arg, 0, 0, (ScriptScope *)(*slot));
            TempMemory_SetPos(pos);
        } else {
            scrCompileMode = 1;
            Scr_CompileStmtList((int *)seq_arg, 0, 0, (ScriptScope *)(*slot));
            EmitRemoveLocalVars((ScriptScope *)(*slot), (ScriptScope *)*slot);
        }
        scrCompileMode = 0;
        g_scr_code_hash = saved_b753c;
    } else {
        ScrCompile_ErrorInFile(loc, "cannot recurse /#");
    }
}

// 0x080752b2  size=75  FUN_080752b2  status=translated  naming=unknown
// evidence: parameter list codegen (opcode 0x35) calling Scr_EmitParamSets.
void Scr_CompileParamList(struct ScriptListItem **list_ref, int loc, struct ScriptScope *ctx)
{
    Scr_EmitParamSets(*list_ref, ctx);
    EmitOpcode(0x35, 0, 0);
    ScrDbg_PushLine(loc, 0);
}

// 0x080752fe  size=28  FUN_080752fe  status=translated  naming=unknown
// evidence: scan pass over a parameter list (Scr_AddVarList).
void Scr_ScanParamList(struct ScriptListItem **list_ref, struct ScriptScope *ctx)
{
    Scr_AddVarList(*list_ref, ctx);
}

// 0x0807531a  size=283  FUN_0807531a  status=translated  naming=inferred
// evidence: strings "cannot recurse /#", "#/ has no matching /#"; handles dev-comment markers.
void Scr_PreScanDevComment(struct ScriptNode *node)
{
    int *p = node->op;
    int kind = node->kind;

    if (kind == 0x45) {
        if (g_scr_in_dev_block == 0) {
            g_scr_in_dev_block = 1;
            g_scr_dev_block_open_pos = p[0];
        } else {
            ScrCompile_ErrorInFile(p[0], "cannot recurse /#");
        }
    } else if (kind < 0x46) {
        if (kind == 0x44 && (g_scr_in_dev_block == 0 || g_scr_developer_script != 0)) {
            int id = ScrVar_GetNewVarValueId((int)g_scr_current_file_symtab, p[0]);
            int slot = Scr_GetOrCreateObjectId(id);
            int mode = (g_scr_in_dev_block == 0) ? 7 : 0xc;
            ScrCompile_DefineFunction((int)slot, p[0], p[3], mode);
        }
    } else if (kind == 0x46) {
        if (g_scr_in_dev_block == 0) {
            ScrCompile_ErrorInFile(p[0], "#/ has no matching /#");
        } else {
            g_scr_in_dev_block = 0;
        }
    }
}

// 0x08075436  size=233  FUN_08075436  status=translated  naming=inferred
// evidence: string "function exceeds operand stack size"; compiles params and body of one function.
void Scr_CompileFunctionBody(int name, struct ScriptNode *fn, int loc, int ctx, struct ScriptScope *out)
{
    /* Writes g_scr_current_function_varid, which overlaps the upper 3 bytes of g_scr_current_file_symtab in the binary. */
    g_scr_current_function_varid = name;
    ScrDbg_SetPendingEntry(loc);
    scrCompileStackDepth = 0;
    scrCompileMaxStackDepth = 0;
    scrCompileMaxCallStackDepth = 0;
    EmitStringRef(fn->op[0], 2);
    Scr_CompileParamList((struct ScriptListItem **)fn->op[1], loc, (ScriptScope *)out);
    Scr_CompileStmtList((int *)fn->op[2], 1, ctx, out);
    EmitEnd();
    ScrDbg_PushLine(ctx, 1);
    ScrDbg_PushLine(0xfffffffe, 0);
    if (0x7ff < scrCompileMaxCallStackDepth * 0x20 + scrCompileMaxStackDepth) {
        ScrCompile_ErrorInFile(loc, "function exceeds operand stack size");
    }
}

// 0x08075520  size=156  FUN_08075520  status=translated  naming=unknown
// evidence: allocates the 0x118-byte function compile context and runs the parameter and body scans.
void Scr_CreateFunctionContext(struct ScriptListItem **params, int *body_ref, int *out)
{
    struct ScriptScope *ctx;

    scrCompileUninitIsError = 0;
    ctx = (struct ScriptScope *)Hunk_AllocateTempMemoryHigh(0x118);
    *out = (int)ctx;
    ctx->state = 0;
    ctx->field_4 = 0;
    ctx->field_c = 0;
    ctx->field_8 = 0;
    memset(ctx->field_10, 0, 8);
    Scr_ScanParamList(params, (ScriptScope *)ctx);
    Scr_ScanStmtList(body_ref, ((ScriptScope *)(int *)ctx));
}

// 0x080755bc  size=166  FUN_080755bc  status=translated  naming=unknown
// evidence: clears the per-function frame; closes an open frame (flag at g_scr_frame_open_flags[idx]).
void Scr_ResetCompileFrame(int idx)
{
    g_scr_in_switch = 0;
    g_scr_switch_case_list = 0;
    g_scr_can_break = 0;
    g_scr_can_break_dev = 0;
    g_scr_break_patch_list = 0;
    g_scr_can_continue = 0;
    g_scr_can_continue_dev = 0;
    g_scr_continue_patch_list = 0;
    g_scr_scope_list_primary = 0;
    g_scr_scope_list_secondary = 0;
    if (g_scr_frame_open_flags[idx] != 0) {
        g_scr_frame_open_flags[idx] = 0;
        EmitEnd();
        ScrDbg_PushLine(0, 0);
        ScrDbg_PushLine(0xfffffffe, 0);
    }
}

// 0x08075662  size=130  FUN_08075662  status=translated  naming=unknown
// evidence: compiles a plain (non-dev) function node after symbol resolution.
void Scr_CompileFunctionNode(struct ScriptNode *node, int *out)
{
    int id;
    int fn_index;

    Scr_ResetCompileFrame(0);
    id = ScrVar_FindVarValueId((int)g_scr_current_file_symtab, node->op[0]);
    fn_index = Scr_GetVarValue(id);
    ScrCompile_RecordFunctionStart(fn_index); /* binary also pushes node->op[4]-offset arg (unused by callee) */
    Scr_CompileFunctionBody(fn_index, node, node->op[3], node->op[4], (ScriptScope *)*out);
}

// 0x080756e4  size=280  FUN_080756e4  status=translated  naming=unknown
// evidence: compiles a dev-mode function node (g_scr_developer_script selects the path).
void Scr_CompileFunctionNodeDev(struct ScriptNode *node, int *out)
{
    int saved_b753c;

    if (g_scr_developer_script == 0) {
        int pos = TempMemory_Reserve(0);
        saved_b753c = g_scr_code_hash;
        scrCompileMode = 2;
        Scr_ResetCompileFrame(1);
        Scr_CompileFunctionBody(0, node, node->op[3], node->op[4], (ScriptScope *)*out);
        TempMemory_SetPos(pos);
        g_scr_code_hash = saved_b753c;
    } else {
        int id;
        int fn_index;
        scrCompileMode = 1;
        Scr_ResetCompileFrame(1);
        id = ScrVar_FindVarValueId((int)g_scr_current_file_symtab, node->op[0]);
        fn_index = Scr_GetVarValue(id);
        ScrCompile_RecordFunctionStart(fn_index); /* binary also pushes node->op[4]-offset arg (unused by callee) */
        Scr_CompileFunctionBody(fn_index, node, node->op[3], node->op[4], (ScriptScope *)*out);
    }
    scrCompileMode = 0;
}

// 0x080757fc  size=262  FUN_080757fc  status=translated  naming=inferred
// evidence: string "cannot put #using_animtree inside /# ... #/ comment".
void Scr_CompileTopLevelItem(struct ScriptNode *node)
{
    int *p = node->op;
    int kind = node->kind;

    if (kind == 0x45) {
        g_scr_in_dev_block = 1;
    } else if (kind < 0x46) {
        if (kind == 0x44) {
            Scr_CreateFunctionContext((struct ScriptListItem **)p[1], (int *)p[2], &p[5]);
            if (g_scr_in_dev_block == 0) {
                Scr_CompileFunctionNode(node, &p[5]);
            } else {
                Scr_CompileFunctionNodeDev(node, &p[5]);
            }
        }
    } else if (kind == 0x46) {
        g_scr_in_dev_block = 0;
    } else if (kind == 0x47) {
        if (g_scr_in_dev_block == 0) {
            const char *h = ScrStr_TextFromHandle((int)p[0]);
            Scr_UsingAnimTree((const char *)h, p[2]);
            Compile_ReleaseStringRef(p[0]);
        } else {
            ScrCompile_ErrorInFile(p[1], "cannot put #using_animtree inside /# ... #/ comment");
        }
    }
}

// 0x08075902  size=153  FUN_08075902  status=translated  naming=inferred
// evidence: string "/# has no matching #/"; two passes over the top-level list.
void Scr_CompileTopLevelList(int *seq_ref)
{
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *cell;

    g_scr_in_dev_block = 0;
    for (cell = seq->list; cell != 0; cell = cell->next) {
        Scr_PreScanDevComment((struct ScriptNode *)cell->item);
    }
    if (g_scr_in_dev_block != 0) {
        ScrCompile_ErrorInFile(g_scr_dev_block_open_pos, "/# has no matching #/");
    }
    g_scr_frame_open_flags[0] = 1;
    g_scr_frame_open_flags[1] = 1;
    for (cell = seq->list; cell != 0; cell = cell->next) {
        Scr_CompileTopLevelItem((struct ScriptNode *)cell->item);
    }
}

// 0x0807599c  size=82  FUN_0807599c  status=translated  naming=unknown
// evidence: registers one include reference through the symbol lookup and Compile_AddFuncRecord.
void Scr_RegisterIncludeRef(struct ScriptNode *node)
{
    const char *text = ScrStr_TextFromHandle((int)(node->op[0]));
    int h = Scr_NormalizeFilenameToStringId(text);
    Compile_ReleaseStringRef(node->op[0]);
    Compile_AddFuncRecord(h, node->op[1], 1);
}

// 0x080759ee  size=55  FUN_080759ee  status=translated  naming=unknown
// evidence: walks an include list and calls Scr_RegisterIncludeRef for each item.
void Scr_RegisterIncludeList(int *seq_ref)
{
    struct cod2_scr_seq *seq = (struct cod2_scr_seq *)*seq_ref;
    struct ScriptListItem *cell;

    for (cell = seq->list; cell != 0; cell = cell->next) {
        Scr_RegisterIncludeRef((struct ScriptNode *)cell->item);
    }
}

// 0x08075a26  size=817  FUN_08075a26  status=translated  naming=inferred
// evidence: strings "Duplicate #include", "Could not find script '%s'"; include resolution and compile.
void Scr_CompileScriptUnit(int *unit, int symtab, struct ScriptScope *ctx)
{
    struct ScrFuncRecord *recs;
    int count;
    int i;
    int finish_args[2];   /* [1] is written as 6 before the final call; [0] is unset */
    int pos;

    g_scr_current_file_symtab = (void *)symtab;
    scrCompileKeepStringRefs = 0;
    scrUsingAnimTreeIndex = 0;
    scrCompileMode = 0;
    if (g_scr_dependency_record_count == 0) {
        recs = 0;
    } else {
        recs = (struct ScrFuncRecord *)Com_MallocZeroed(g_scr_dependency_record_count * 12);
    }
    scrFuncRecordPtr = (ScrFuncRecord *)recs;
    if (recs != 0) {
        recs->chain = (int)g_scr_include_block_chain;
        g_scr_include_block_chain = recs;
    }

    Scr_RegisterIncludeList((int *)unit[0]);
    Scr_CompileTopLevelList((int *)unit[1]);
    pos = TempMemory_Reserve(0);
    g_scr_code_length = pos - g_scr_code_base;
    Hunk_ClearTempMemoryHigh();

    count = g_scr_dependency_record_count;
    for (i = 0; i < count; i++) {
        struct ScrFuncRecord *rec = &recs[i];
        int id = rec->script_id;
        const char *name = ScrStr_TextFromHandle((int)id);
        int loaded = Scr_LoadScriptFile(name);
        int j;

        if (loaded == 0) {
            const char *name2 = ScrStr_TextFromHandle((int)id);
            ScrCompile_ErrorInFile(rec->location, "Could not find script '%s'", name2);
            return;
        }
        ScrStr_Release(id);

        j = i;
        if (rec->pending != 0) {
            struct ScrFuncRecord *other;
            int next_index;
            while ((next_index = j + 1) < count &&
                   (other = &recs[next_index], other->pending != 0)) {
                j = next_index;
                if (other->script_id == id) {
                    ScrCompile_ErrorInFile(other->location, "Duplicate #include");
                    return;
                }
            }
            rec->pending = 0;
            {
                int stmt = Scr_GetNextSibling(loaded);
                while (stmt != 0) {
                    int kind_r = Scr_GetVarType(stmt);
                    if (kind_r == 1) {
                        int sym = Scr_GetVarValue(stmt);
                        int sym_id = ScrVar_FindVarValueId(sym, 1);
                        if (sym_id != 0) {
                            int pair[2];
                            int copy[2];
                            ScrVar_ReadVarValue((VariableValue *)pair, sym_id);
                            copy[0] = pair[0];
                            copy[1] = pair[1];
                            if (pair[1] != 0xd) {
                                uint16 sid = (uint16)Scr_GetVarNameId(stmt);
                                int idx_name = ScrVar_GetNewVarValueId(symtab, sid);
                                int slot_val = Scr_GetOrCreateObjectId(idx_name);
                                int rv = ScrCompile_DefineFunction(slot_val, sid, rec->location, 0xd);
                                int *dst = (int *)ScrVar_GetValuePtr(rv);
                                int *src = (int *)ScrVar_GetValuePtr(sym_id);
                                *dst = *src;
                                ScrCompile_ResolveFunctionRefs((int)slot_val, (VariableValue *)copy, 0);
                            }
                        }
                    }
                    stmt = Scr_GetNextSibling(stmt);
                }
            }
        }
    }

    if (recs != 0) {
        g_scr_include_block_chain = (struct ScrFuncRecord *)recs->chain;
        Com_Free_A(recs);
    }
    ScrCompile_ResolveAllFunctionRefs((int)symtab);
    finish_args[1] = 6;
    ScrVar_SetVarValueReleasing((int)ctx, (VariableValue *)finish_args);
}

// 0x08075da0  size=52  FUN_08075da0  status=translated  naming=unknown
// evidence: frees the include-block chain (g_scr_include_block_chain), following each record's chain link.
void Scr_FreeIncludeBlocks(void)
{
    struct ScrFuncRecord *blk;

    while ((blk = g_scr_include_block_chain) != 0) {
        g_scr_include_block_chain = (struct ScrFuncRecord *)blk->chain;
        Com_Free_A(blk);
    }
}

// 0x08075dd4  size=31  FUN_08075dd4  status=translated  naming=unknown
// evidence: releases one symbol reference through ScrVar_AddRefValue.
void Scr_AddRefToValue(struct VariableValue *value)
{
    ScrVar_AddRefValue(value->type, (unsigned int)value->u.intValue);
}

// 0x08075df4  size=31  FUN_08075df4  status=translated  naming=unknown
// evidence: releases one symbol reference through ScrVar_ReleaseValue.
void Scr_RemoveRefToValue(struct VariableValue *value)
{
    ScrVar_ReleaseValue(value->type, (unsigned int)value->u.intValue);
}

// 0x08075e14  size=43  FUN_08075e14  status=translated  naming=unknown
// evidence: range test against the code window [g_scr_code_base, g_scr_code_base + g_scr_code_length).
bool Scr_IsInCodeRange(int p)
{
    return (uint32)(p - g_scr_code_base) < (uint32)g_scr_code_length;
}

// 0x08075e40  size=47  FUN_08075e40  status=translated  naming=unknown
// evidence: range test against [g_scr_code_base, scrGlobalWindowEnd).
int Scr_IsInGlobalRange(uint32 p)
{
    int result = 0;
    if ((uint32)g_scr_code_base <= p && p < (uint32)scrGlobalWindowEnd) {
        result = 1;
    }
    return result;
}

// 0x08075e70  size=69  FUN_08075e70  status=translated  naming=unknown
// evidence: identifier check (alnum or underscore) over a NUL-terminated string.
int Scr_IsIdentifier(const char *s)
{
    for (;;) {
        int ok;
        if (*s == 0) {
            return 1;
        }
        ok = isalnum((int)*s);
        if (ok == 0 && *s != '_') {
            break;
        }
        s++;
    }
    return 0;
}

// 0x08075eb6  size=306  FUN_08075eb6  status=translated  naming=inferred
// evidence: resolves a script-function reference to a code offset via symbol tables.
int Scr_ResolveFunctionOffset(const char *script, const char *label)
{
    int name_id = (int)script;
    int ref = (int)label;
    int sym;
    int tab;
    int tab_rec;
    int idx;
    int c;
    int tgt;
    int tgt_sym;
    int rec[2];

    sym = Scr_NormalizeFilenameToStringId((const char *)name_id);
    tab = ScrVar_FindVarValueId(g_scr_file_function_tables, sym);
    ScrStr_Release(sym);
    if (tab == 0) {
        return 0;
    }
    tab_rec = Scr_GetVarValue(tab);
    idx = ScrStr_LookupLowerCString((const char *)ref);
    if (idx == 0) {
        return 0;
    }
    c = ScrVar_FindVarValueId(tab_rec, idx);
    if (c == 0) {
        return 0;
    }
    if (Scr_GetVarType(c) != 1) {
        return 0;
    }
    tgt = Scr_GetVarValue(c);
    tgt_sym = ScrVar_FindVarValueId(tgt, 1);
    ScrVar_ReadVarValue((VariableValue *)rec, tgt_sym);
    if (!Scr_IsInCodeRange(rec[0])) {
        return 0;
    }
    return rec[0] - g_scr_code_base;
}

// 0x08075fe8  size=44  FUN_08075fe8  status=translated  naming=unknown
// evidence: creates the symbol pool and its 0x20000-byte table.
void Scr_InitSymbolPools(void)
{
    g_scr_symbol_pool_mark = Hunk_GetHighPermanent();
    g_scr_slot_table = (int)Hunk_Alloc(0x20000);
    g_scr_next_slot_number = 0;
}

// 0x08076014  size=31  FUN_08076014  status=translated  naming=unknown
// evidence: releases the symbol pool and its table.
void Scr_FreeSymbolPools(void)
{
    g_scr_slot_table = 0;
    Hunk_SetHighMark(g_scr_symbol_pool_mark);
}

// 0x08076034  size=109  FUN_08076034  status=translated  naming=unknown
// evidence: assigns the next slot number to an id in the short table.
short Scr_AssignSlot(int id)
{
    short slot;

    ScrStr_SetFlagOrDecRef(id, 2);
    slot = *(short *)(g_scr_slot_table + id * 2);
    if (slot == 0) {
        g_scr_next_slot_number = g_scr_next_slot_number + 1;
        *(short *)(g_scr_slot_table + id * 2) = (short)g_scr_next_slot_number;
        slot = (short)g_scr_next_slot_number;
    }
    return slot;
}

// 0x080760a2  size=89  FUN_080760a2  status=translated  naming=unknown
// evidence: returns the slot for a reference, assigning one when it has none.
uint32 Scr_GetOrAssignSlot(int ref)
{
    int idx;
    uint32 slot;

    idx = ScrStr_LookupCString((const char *)ref);
    slot = (uint32)*(uint16 *)(g_scr_slot_table + idx * 2);
    if (slot == 0) {
        int id = ScrStr_InternCStringEx((const char *)ref, 0, 0xf);
        slot = (uint32)Scr_AssignSlot(id);
    }
    return slot;
}

// 0x080760fc  size=154  FUN_080760fc  status=translated  naming=unknown
// evidence: one-time compile-state initialisation (pools, tables, ranges).
void Scr_InitCompileState(void)
{
    scrCompileDebuggerMode = 1;
    ScrDbg_Init();
    g_scr_loaded_scripts_table = ScrVar_AllocArray();
    g_scr_file_function_tables = ScrVar_AllocArray();
    scrCompileFuncCallArrayId = (void *)ScrVar_AllocArray();
    g_scr_builtin_method_cache = (void *)ScrVar_AllocArray();
    g_scr_code_base = (int)Hunk_AllocLow(0);
    g_scr_code_length = 0;
    scrGlobalWindowEnd = 0;
    Scr_InitSymbolPools();
    g_scr_field_table = 0;
    scrValueStackCount = 0;
    Scr_ClearErrorMessage();
    scrFuncTableCount = 0;
    Scr_InitAnimTrees(1);
    TempMemory_Reset();
}

// 0x08076196  size=30  FUN_08076196  status=translated  naming=unknown
// evidence: reduced initialisation used when only the symbol table is needed.
void Scr_InitCompileStateMin(void)
{
    scrCompileDebuggerMode = 1;
    ScrDbg_Init();
    g_scr_loaded_scripts_table = ScrVar_AllocArray();
}

// 0x080761b4  size=75  FUN_080761b4  status=translated  naming=unknown
// evidence: resets one table slot (scrAnimTreeCount[idx], 0x200-byte block) and sets its table.
void Scr_InitAnimTrees(int idx)
{
    g_scr_animtree_groups_active = 1;
    scrAnimTreeCount[idx] = 0;
    *(int *)(scrAnimTreeObjects + idx * 0x200) = 0;
    scrAnimTreeRoot = ScrVar_AllocArray();
    scrAnimTreeScratchObject = 0;
    scrCompileMode = 0;
}

// 0x08076200  size=150  FUN_08076200  status=translated  naming=unknown
// evidence: reads one line of source text (at most max bytes) from g_scr_source_cursor, handling the end of the buffer.
int Scr_ReadSourceLine(char *buf, int max)
{
    char c = '*';
    int i;

    for (i = 0; i < max; i++) {
        c = *g_scr_source_cursor;
        g_scr_source_cursor = g_scr_source_cursor + 1;
        if (c == 0 || c == '\n') {
            break;
        }
        buf[i] = c;
    }
    if (c == '\n') {
        buf[i] = 10;
        i = i + 1;
    } else if (c == 0) {
        if (g_scr_source_text == 0) {
            g_scr_source_cursor = g_scr_source_cursor - 1;
        } else {
            g_scr_source_cursor = g_scr_source_text;
            g_scr_source_text = 0;
        }
    }
    return i;
}

// 0x08076296  size=444  FUN_08076296  status=translated  naming=inferred
// evidence: string "%s.gsc" and "Could not find script '%s'"; loads and parses a script by name.
int Scr_LoadScriptFile(const char *script)
{
    int name_id = (int)script;
    char path[68];
    int sym;
    int tab;
    int ret;

    sym = Scr_NormalizeFilenameToStringId((const char *)name_id);
    tab = ScrVar_FindVarValueId(g_scr_loaded_scripts_table, sym);
    if (tab == 0) {
        int slot = ScrVar_NewChildAVarId(g_scr_loaded_scripts_table, sym);
        int saved_unit = (int)scr_last_source_text;
        int saved_pos;
        const char *nm;
        char *src;
        char *saved_name;

        ScrStr_Release(sym);
        nm = ScrStr_TextFromHandle((int)sym);
        Com_sprintf(path, 0x40, "%s.gsc", nm);
        saved_unit = (int)scr_last_source_text;
        saved_pos = TempMemory_Reserve(0);
        nm = ScrStr_TextFromHandle((int)sym);
        /* binary: Scr_AddSourceBuffer-style call (name, "<name>.gsc", codePos, 1) at 0x08076395 */
        src = ScrDbg_LoadFrameFileOrSaved((int)(long)nm, path, saved_pos, 1);
        saved_name = g_scr_current_source_name;
        if (src == 0) {
            ret = 0;
            g_scr_current_source_name = saved_name;
        } else {
            int unit;
            int symtab;
            g_08205648 = 0;
            g_scr_dependency_record_count = 0;
            g_scr_current_source_name = path;
            g_scr_source_cursor = g_0814445f;
            g_scr_source_text = (char *)src;
            Lex_RunParser(&unit, 0);
            symtab = ScrVar_GetNewVarValueId(g_scr_file_function_tables, sym);
            ret = Scr_GetOrCreateObjectId(symtab);
            Scr_CompileScriptUnit((int *)unit, ret, (ScriptScope *)slot);
            g_scr_current_source_name = saved_name;
            scr_last_source_text = (char *)saved_unit;
        }
    } else {
        int t2;
        ScrStr_Release(sym);
        t2 = ScrVar_FindVarValueId(g_scr_file_function_tables, sym);
        if (t2 == 0) {
            ret = 0;
        } else {
            ret = Scr_GetVarValue(t2);
        }
    }
    return ret;
}

// 0x08076452  size=13  FUN_08076452  status=translated  naming=unknown
// evidence: releases the parser pools.
void Scr_ReleasePools(void)
{
    Hunk_CommitLowTemp();
}

// 0x08076460  size=176  FUN_08076460  status=translated  naming=unknown
// evidence: tears down the four symbol tables and the symbol pool.
void Scr_ShutdownCompileState(void)
{
    Scr_FreeSymbolPools();
    ScrStr_ClearFlagBits(2);
    scrCompileDebuggerMode = 0;
    ScrVar_DestroyObject(g_scr_loaded_scripts_table);
    ScrVar_Release(g_scr_loaded_scripts_table);
    g_scr_loaded_scripts_table = 0;
    ScrVar_DestroyObject(g_scr_file_function_tables);
    ScrVar_Release(g_scr_file_function_tables);
    g_scr_file_function_tables = 0;
    ScrVar_DestroyObject((unsigned int)scrCompileFuncCallArrayId);
    ScrVar_Release((unsigned int)scrCompileFuncCallArrayId);
    scrCompileFuncCallArrayId = 0;
    ScrVar_DestroyObject((unsigned int)g_scr_builtin_method_cache);
    ScrVar_Release((unsigned int)g_scr_builtin_method_cache);
    g_scr_builtin_method_cache = 0;
}

// 0x08076510  size=64  FUN_08076510  status=translated  naming=unknown
// evidence: visits entries 1..scrAnimTreeCount[idx] of one table slot.
void Scr_LoadAnimTrees(int arg, int idx)
{
    int i;
    for (i = 1; i <= scrAnimTreeCount[idx]; i++) {
        Scr_LoadAnimTree(i, arg, idx);
    }
}

// 0x08076550  size=102  FUN_08076550  status=translated  naming=unknown
// evidence: tears down one table slot (scrAnimTreeRoot/scrAnimTreeScratchObject).
void Scr_ShutdownAnimTrees(void)
{
    ScrVar_DestroyObject(scrAnimTreeRoot);
    ScrVar_Release(scrAnimTreeRoot);
    scrAnimTreeRoot = 0;
    if (scrAnimTreeScratchObject != 0) {
        ScrVar_Release(scrAnimTreeScratchObject);
    }
    ScrStr_ClearFlagBits(2);
    scrGlobalWindowEnd = (int)Hunk_AllocLow(0);
    g_scr_animtree_groups_active = 0;
}

// 0x080765b6  size=113  FUN_080765b6  status=translated  naming=unknown
// evidence: full reset of the compile state, the table slot and the symbol ranges.
void Scr_ResetAll(uint8 flag)
{
    (void)flag; /* byte argument stored to a local and otherwise unused (0x080765bc) */
    if (scrCompileDebuggerMode != 0) {
        scrCompileDebuggerMode = 0;
        Scr_ShutdownCompileState();
    }
    if (g_scr_animtree_groups_active != 0) {
        g_scr_animtree_groups_active = 0;
        Scr_ShutdownAnimTrees();
    }
    ScrStr_ClearFlagBits(1);
    ScrDbg_Shutdown();
    g_scr_code_base = 0;
    g_scr_code_length = 0;
    scrGlobalWindowEnd = 0;
    g_scr_code_hash = 0;
}

// 0x08076628  size=88  FUN_08076628  status=translated  naming=inferred
// evidence: counts the nodes of one bucket subtree (used by the stats dump).
int Bucket_TreeNodeCount(int idx)
{
    int left;
    int right;

    if (idx == 0) {
        return 0;
    }
    left = Bucket_TreeNodeCount(g_08206f80[idx].child[0]);
    right = Bucket_TreeNodeCount(g_08206f80[idx].child[1]);
    return left + right + 1;
}

// 0x08076680  size=233  FUN_08076680  status=translated  naming=observed
// evidence: strings "%d subtree has %d * %d = %d free buckets", "total memory alloc buckets: %d (%d instances)".
void Mem_PrintBucketStats(void)
{
    int k;

    Com_Printf("********************************\n");
    for (k = 0; k < 0x11; k++) {
        int n = Bucket_TreeNodeCount(g_08287280[k]);
        Com_Printf("%d subtree has %d * %d = %d free buckets\n", k, n, 1 << (k & 0x1f),
                    n << (k & 0x1f));
    }
    Com_Printf("********************************\n");
    Com_Printf("********************************\n");
    Com_Printf("total memory alloc buckets: %d (%d instances)\n", g_082872a8, g_082872a4);
    Com_Printf("total memory free buckets: %d\n", 0xffff - g_082872a8);
    Com_Printf("********************************\n");
}

// 0x0807676a  size=10  FUN_0807676a  status=translated  naming=inferred
// evidence: returns the allocated-bucket counter printed by Mem_PrintBucketStats.
int Mem_GetAllocatedBuckets(void)
{
    return g_082872a8;
}

// 0x08076774  size=194  FUN_08076774  status=translated  naming=unknown
// evidence: fills the popcount, trailing-zero (capped at 8) and bit-length tables for 0..255.
void Mem_InitBucketTables(void)
{
    uint32 v;

    for (v = 0; (int)v < 0x100; v = v + 1) {
        uint32 t;
        char pop = 0;
        uint8 tz;
        char bits = 0;

        for (t = v; t != 0; t = (int)t >> 1) {
            if ((t & 1) != 0) {
                pop = pop + 1;
            }
        }
        g_08287080[v] = (uint8)pop;

        tz = 8;
        while ((((1u << (tz & 0x1f)) - 1u) & v) != 0) {
            tz = tz - 1;
        }
        g_08286f80[v] = tz;

        for (t = v; t != 0; t = (int)t >> 1) {
            bits = bits + 1;
        }
        g_08287180[v] = (uint8)bits;
    }
}

// 0x08076836  size=115  FUN_08076836  status=translated  naming=unknown
// evidence: subtree-size key for a 16-bit position: lowbit size plus the count of the complement's unset bits.
int Mem_BucketKey(int idx)
{
    uint32 u2 = 0x10000u - (uint32)idx;
    uint32 lo = u2 & 0xff;
    uint32 hi = (u2 >> 8) & 0xff;
    uint8 bv = g_08286f80[lo];

    if (lo == 0) {
        bv = (uint8)(bv + g_08286f80[hi]);
    }
    return (int)((1u << (bv & 0x1f)) + (u2 - ((uint32)g_08287080[lo] + (uint32)g_08287080[hi])));
}

// 0x080768aa  size=352  FUN_080768aa  status=translated  naming=inferred
// evidence: bucket-tree insert (heap-ordered by Mem_BucketKey, search by pool index); no string.
void Mem_BucketTree_Insert(uint32 node, int klass)
{
    uint16 *slot = &g_08287280[klass];
    uint32 cur = *slot;

    if (cur != 0) {
        int new_key = Mem_BucketKey((int)node);
        int pos = 0;
        int step = 0x10000;

        do {
            int cur_key = Mem_BucketKey((int)cur);
            if (cur_key < new_key) {
                for (;;) {
                    uint32 aux;
                    *slot = (uint16)node;
                    aux = g_08206f80[cur].aux;
                    g_08206f80[node].child[0] = g_08206f80[cur].child[0];
                    g_08206f80[node].child[1] = g_08206f80[cur].child[1];
                    g_08206f80[node].aux = aux;
                    if (cur == 0) {
                        break;
                    }
                    step = step >> 1;
                    if ((int)cur < pos) {
                        slot = &g_08206f80[node].child[0];
                        pos += -step;
                    } else {
                        slot = &g_08206f80[node].child[1];
                        pos += step;
                    }
                    node = cur;
                    cur = *slot;
                }
                return;
            }
            step = step >> 1;
            if ((int)node < pos) {
                slot = &g_08206f80[cur].child[0];
                pos += -step;
            } else {
                slot = &g_08206f80[cur].child[1];
                pos += step;
            }
            cur = *slot;
        } while (cur != 0);
    }
    *slot = (uint16)node;
    g_08206f80[node].child[0] = 0;
    g_08206f80[node].child[1] = 0;
}

// 0x08076a0a  size=470  FUN_08076a0a  status=translated  naming=inferred
// evidence: bucket-tree remove of a node by pool index; returns 1 when removed, 0 when absent.
int Mem_BucketTree_Remove(uint32 node, int klass)
{
    uint32 pos = 0;
    int step = 0x10000;
    uint16 *slot = &g_08287280[klass];
    uint32 cur = *slot;
    uint16 pair[2];
    uint32 aux_val;

    for (;;) {
        int delta;
        cur = *slot;
        if (cur == 0) {
            return 0;
        }
        if (node == cur) {
            break;
        }
        if (node == pos) {
            return 0;
        }
        step = step >> 1;
        if ((int)node < (int)pos) {
            slot = &g_08206f80[cur].child[0];
            delta = -step;
        } else {
            slot = &g_08206f80[cur].child[1];
            delta = step;
        }
        pos += (uint32)delta;
    }

    pair[0] = g_08206f80[node].child[0];
    pair[1] = g_08206f80[node].child[1];
    aux_val = g_08206f80[node].aux;
    for (;;) {
        uint32 next_aux;
        uint16 next_pair[2];

        if (pair[0] == 0) {
            node = pair[1];
            *slot = pair[1];
            if (node == 0) {
                return 1;
            }
            slot = &g_08206f80[node].child[1];
        } else if (pair[1] == 0) {
            node = pair[0];
            *slot = pair[0];
            slot = &g_08206f80[node].child[0];
        } else {
            int k0 = Mem_BucketKey(pair[0]);
            int k1 = Mem_BucketKey(pair[1]);
            if (k0 < k1) {
                node = pair[1];
                *slot = pair[1];
                slot = &g_08206f80[node].child[1];
            } else {
                node = pair[0];
                *slot = pair[0];
                slot = &g_08206f80[node].child[0];
            }
        }
        next_aux = g_08206f80[node].aux;
        next_pair[0] = g_08206f80[node].child[0];
        next_pair[1] = g_08206f80[node].child[1];
        g_08206f80[node].child[0] = pair[0];
        g_08206f80[node].child[1] = pair[1];
        g_08206f80[node].aux = aux_val;
        pair[0] = next_pair[0];
        pair[1] = next_pair[1];
        aux_val = next_aux;
    }
}

// 0x08076be0  size=325  FUN_08076be0  status=translated  naming=inferred
// evidence: removes the root of a size-class bucket tree and sifts its children up.
void Mem_BucketTree_PopRoot(int klass)
{
    uint16 *slot = &g_08287280[klass];
    uint16 pair[2];
    uint32 aux_val;

    pair[0] = g_08206f80[g_08287280[klass]].child[0];
    pair[1] = g_08206f80[g_08287280[klass]].child[1];
    aux_val = g_08206f80[g_08287280[klass]].aux;
    for (;;) {
        uint32 cur;
        uint32 next_aux;
        uint16 next_pair[2];

        if (pair[0] == 0) {
            cur = pair[1];
            *slot = pair[1];
            if (cur == 0) {
                return;
            }
            slot = &g_08206f80[cur].child[1];
        } else if (pair[1] == 0) {
            cur = pair[0];
            *slot = pair[0];
            slot = &g_08206f80[cur].child[0];
        } else {
            int k0 = Mem_BucketKey(pair[0]);
            int k1 = Mem_BucketKey(pair[1]);
            if (k0 < k1) {
                cur = pair[1];
                *slot = pair[1];
                slot = &g_08206f80[cur].child[1];
            } else {
                cur = pair[0];
                *slot = pair[0];
                slot = &g_08206f80[cur].child[0];
            }
        }
        next_aux = g_08206f80[cur].aux;
        next_pair[0] = g_08206f80[cur].child[0];
        next_pair[1] = g_08206f80[cur].child[1];
        g_08206f80[cur].child[0] = pair[0];
        g_08206f80[cur].child[1] = pair[1];
        g_08206f80[cur].aux = aux_val;
        pair[0] = next_pair[0];
        pair[1] = next_pair[1];
        aux_val = next_aux;
    }
}
