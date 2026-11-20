#ifndef COD2_CHUNK_014_TYPES_H
#define COD2_CHUNK_014_TYPES_H

/* ScrVarEntry.u for object entries is {refCount u16 @+4, aux u16 @+6} (canon: read via refCount/aux accessors). */
#define SCRVAR_RC(e) (((uint16 *)&(e).u)[0])
#define SCRVAR_AUX(e) (((uint16 *)&(e).u)[1])

/*
 * chunk_014 types: script variable system (0x8079eba..0x807cf02).
 *
 * Observed layout: a global pool of 0xfffe 16-byte entries at 0x08297500
 * (g_scrVar), indexed by 16-bit-ish ids; used as hash slots, free list and
 * object/child bookkeeping at once. Field semantics below are inferred from
 * how the code links entries; the +0/+2/+12/+14 link roles are not fully
 * resolved (see open questions).
 */

/* Destination record whose VariableValue lives at +8 (first 8 bytes not used in this chunk). */
struct ScrVarSetTarget {
    int unk0;
    int unk4;
    VariableValue value;
};
COD2_ASSERT_SIZE(ScrVarSetTarget, 0x10);
COD2_ASSERT_OFFSET(ScrVarSetTarget, value, 0x8);

/* String hash node (sl_hash[0x4000] at 0x08287400); layout owned by chunk 013. */
struct sl_hash_node_s {
    uint16 link;     /* +0 (test mask 0xc000) */
    uint16 handle;   /* +2 string id */
};
COD2_ASSERT_SIZE(sl_hash_node_s, 4);

/* Per-thread record built by the thread dump (0x8c bytes). */
struct ScrThreadDumpRec {
    int stack[32];       /* +0x00 code positions, innermost first */
    int depth;           /* +0x80 */
    float varUsage;      /* +0x84 */
    float endonUsage;    /* +0x88 */
};
COD2_ASSERT_SIZE(ScrThreadDumpRec, 0x8c);
COD2_ASSERT_OFFSET(ScrThreadDumpRec, depth, 0x80);
COD2_ASSERT_OFFSET(ScrThreadDumpRec, varUsage, 0x84);
COD2_ASSERT_OFFSET(ScrThreadDumpRec, endonUsage, 0x88);

/* Variable types (low 5 bits of ScrVarEntry::info), from the type-name table at 0x0817d960. */
enum ScrVarType {
    SCRVAR_UNDEFINED = 0, SCRVAR_POINTER = 1, SCRVAR_STRING = 2, SCRVAR_ISTRING = 3,
    SCRVAR_VECTOR = 4, SCRVAR_FLOAT = 5, SCRVAR_INTEGER = 6, SCRVAR_CODEPOS = 7,
    SCRVAR_PRECODEPOS = 8, SCRVAR_FUNCTION = 9, SCRVAR_STACK = 10, SCRVAR_ANIMATION = 11,
    SCRVAR_DEVELOPER_CODEPOS = 12, SCRVAR_INCLUDE_CODEPOS = 13, SCRVAR_THREAD_LIST = 14,
    SCRVAR_THREAD = 15, SCRVAR_NOTIFY_THREAD = 16, SCRVAR_TIME_THREAD = 17, SCRVAR_CHILD_THREAD = 18,
    SCRVAR_STRUCT = 19, SCRVAR_REMOVED_ENTITY = 20, SCRVAR_ENTITY = 21, SCRVAR_ARRAY = 22,
    SCRVAR_REMOVED_THREAD = 23
};

/* Globals and out-of-chunk callees/definitions come from cod2_globals.h / cod2_prototypes.h. */

#endif
