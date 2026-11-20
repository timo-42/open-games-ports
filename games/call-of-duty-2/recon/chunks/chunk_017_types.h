/*
 * chunk_017 types: cod2_lnxded 1.3 script value stack, frame stack and
 * object/variable-list layouts used by functions 0x80825d4..0x8085658.
 * Target: ELF32 i386, GNU C++98, 4-byte pointers.
 *
 * Layouts recovered from consistent offsets in the pseudocode and checked
 * against the disassembly (stride of scrVmPub, scrVmPub_function_frame, scrVmPub_top).
 */
#ifndef COD2_CHUNK_017_TYPES_H
#define COD2_CHUNK_017_TYPES_H

/* <ctype.h> cannot be included: the generated cod2_prototypes.h declares toupper/isalpha with C++
 * linkage (generator issue). tolower is the libc import used by Scr_GetLowerString. */
extern "C" int tolower(int c);

/* Value type codes as observed in the type checks of this chunk. */
#define COD2_SCR_TYPE_UNDEFINED   0   /* pushed by Scr_AddUndefined */
#define COD2_SCR_TYPE_OBJECT      1   /* object/entity handle, accepted by Scr_GetObject / Scr_GetEntity */
#define COD2_SCR_TYPE_STRING      2   /* interned string id */
#define COD2_SCR_TYPE_LOCSTRING   3   /* localized string */
#define COD2_SCR_TYPE_VECTOR      4   /* vector reference */
#define COD2_SCR_TYPE_FLOAT       5   /* raw float bits, accepted by Scr_GetFloat */
#define COD2_SCR_TYPE_INT         6   /* integer, also animtree index */
#define COD2_SCR_TYPE_FRAMEREF    7   /* frame-cell reference inside variable lists */
#define COD2_SCR_TYPE_MARKER      8   /* argument-list separator written by Scr_NotifyNum */
#define COD2_SCR_TYPE_FUNCTION    9   /* code offset relative to g_scr_code_base */
#define COD2_SCR_TYPE_HANDLE     10   /* object/variable-list link stored by Scr_SaveStack users */
#define COD2_SCR_TYPE_ANIM       11   /* packed anim (index | tree << 16) */

/* Anim operand as written by Scr_GetAnim: low 16 bits anim, high 16 bits tree. */
typedef struct cod2_scr_anim_s {
    uint16 index;   /* +0x00 */
    uint16 tree;    /* +0x02 */
} cod2_scr_anim_s;
COD2_ASSERT_SIZE(cod2_scr_anim_s, 0x4);


/* Sentinel object value (scrVm_noOwnerSentinel) and object ids used as list links. */
#define COD2_OBJ_HEAD_SENTINEL 0x1fffe   /* object link value 0x1fffe */
#define COD2_OBJ_TAIL_SENTINEL 0x1ffff   /* object link value 0x1ffff */

/* Packed variable-list block (ScrStackBuffer_s header, 0xb bytes) followed by `size` 5-byte
 * ScrStackRecord_s records. */
#define COD2_VARLIST_HDR_SIZE   ((uint32)sizeof(ScrStackBuffer_s))
#define COD2_VARLIST_RECS(list) ((ScrStackRecord_s *)((uint8 *)(list) + COD2_VARLIST_HDR_SIZE))

/* Expanded working list used by Scr_RestoreStack (param_2). Words:
 * [0] owner, [1] head id, [2] count, [3] end cursor, [4] start cursor. */
#define COD2_SCR_ACTIVE_WORDS 5

/* Pair returned through a hidden pointer by Scr_GetVarInfoPair (same layout as chunk 15's). */
struct ScrVarInfoPair {
    uint16 field_6;            /* entry +0x06 */
    uint16 nameId;             /* flags >> 8 truncated to 16 bits */
};
COD2_ASSERT_SIZE(ScrVarInfoPair, 4);

/* Callee owned by chunk 16 with the bytecode position as const char * (canonical pos type). */
unsigned int VM_ExecuteInternal(const char *codepos, unsigned int localId, int localVarCount,
                                VariableValue *top, VariableValue *startTop);

/* Chunk 14 callee with the canonical value type (owner not migrated yet). */
void ScrVar_SetVarValue(int id, VariableValue *v);

#endif /* COD2_CHUNK_017_TYPES_H */
