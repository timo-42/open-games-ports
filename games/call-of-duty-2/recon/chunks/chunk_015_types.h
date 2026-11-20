/*
 * Chunk 015 types: script variable table (VM variable list) and script value operators,
 * cod2_lnxded 1.3, 0x0807cf02..0x0807fd12.
 *
 * Layout evidence: 16-byte entries at 0x08297500 indexed by variable id (id << 4);
 * type name pointer table at 0x0817d960; 12-byte class table at 0x0817d920.
 */
#ifndef COD2_CHUNK_015_TYPES_H
#define COD2_CHUNK_015_TYPES_H

/* ScrVarEntry.u for object/array entries is {refCount u16 @+4, size/aux u16 @+6}. */
#define SCRVAR_RC(e) (((uint16 *)&(e).u)[0])
#define SCRVAR_AUX(e) (((uint16 *)&(e).u)[1])

#define SCR_ADD_REF(v) Scr_AddRefToValue(v)
#define SCR_REMOVE_REF(v) Scr_RemoveRefToValue(v)

/* Type tags of a script stack value (VariableValue.type). Names from the type name table
 * at 0x0817d960 (observed strings); numbering is the index into that table. */
enum ScrValueType {
    SCR_VALUE_UNDEFINED = 0,
    SCR_VALUE_OBJECT = 1,      /* "object": value is a variable id */
    SCR_VALUE_STRING = 2,
    SCR_VALUE_ISTRING = 3,     /* "localized string" */
    SCR_VALUE_VECTOR = 4,      /* value is a pointer to 3 floats */
    SCR_VALUE_FLOAT = 5,
    SCR_VALUE_INT = 6,
    SCR_VALUE_CODEPOS = 7,
    SCR_VALUE_PRECODEPOS = 8,
    SCR_VALUE_FUNCTION = 9,
    SCR_VALUE_STACK = 10,
    SCR_VALUE_ANIMATION = 11
};

/* Type tags of a variable table entry (low 5 bits of ScrVarEntry.flags). */
enum ScrVarEntryType {
    SCR_VAR_FREE_SLOT = 0,     /* untyped / not yet created */
    SCR_VAR_POINTER = 1,
    SCR_VAR_ENTITY_A = 0x13,   /* "thread"/"struct" name slots; handled by the entity sweep */
    SCR_VAR_ENTITY_B = 0x14,
    SCR_VAR_ARRAY = 0x16,
    SCR_VAR_DEAD = 0x17
};

/* Pair returned by Scr_GetVarInfoPair (returned through a hidden pointer, 4 bytes). */
struct ScrVarInfoPair {
    uint16 field_6;            /* entry +0x06 */
    uint16 nameId;             /* flags >> 8 truncated to 16 bits */
};
COD2_ASSERT_SIZE(ScrVarInfoPair, 4);

/* 12-byte snapshot filled by Scr_GetStatSnapshot. */
struct ScrStatSnapshot {
    int value0;                /* g_scr_code_hash */
    int value1;                /* g_scr_code_length */
    int delta;                 /* scrGlobalWindowEnd - g_scr_code_base */
};
COD2_ASSERT_SIZE(ScrStatSnapshot, 12);

/* Globals and out-of-chunk callees come from cod2_globals.h / cod2_prototypes.h. */

#endif
