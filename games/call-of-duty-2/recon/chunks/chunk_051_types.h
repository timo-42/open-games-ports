/*
 * Chunk 051 types: 0x08118260..0x0811bf64 (script mover / spawn / entity fields / IP filter).
 * Layouts are recovered from consistent offsets in this chunk only; the merge lane may widen them.
 */
#ifndef COD2_CHUNK_051_TYPES_H
#define COD2_CHUNK_051_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ---- tables ---- */
typedef void (*entFieldSetFn)(gentity_t *ent, int fieldIndex);
/* g_entityScriptFieldTable: entity field table, terminated by name == NULL (stride 0x10) */
typedef struct entityFieldDef_s {
    const char *name;
    int ofs;
    int type;                      /* 0 int,1 float,2 string,3 script str,4 vec3,5 entity,6 yaw,7 u16 object,8 model */
    entFieldSetFn setter;
} entityFieldDef_t;
COD2_ASSERT_SIZE(entityFieldDef_t, 0x10);

/* g_scriptMoverMethodTable: script entity method table, 12 entries (stride 0xc) */
typedef struct entityMethodDef_s {
    const char *name;
    void *handler;
    int unused;
} entityMethodDef_t;
COD2_ASSERT_SIZE(entityMethodDef_t, 0xc);

typedef void (*spawnFn)(gentity_t *ent);
/* g_spawnFuncTable: classname -> spawn function, terminated by name == NULL */
typedef struct spawnDef_s {
    const char *name;
    spawnFn spawn;
} spawnDef_t;
COD2_ASSERT_SIZE(spawnDef_t, 8);

/* Item table entry (stride 0x2c); only classname is read here. Table at 0x081850c0. */
typedef struct gitem_s {
    const char *classname;
    byte rest[0x2c - 4];
} gitem_t;
COD2_ASSERT_SIZE(gitem_t, 0x2c);

/* Spawn-variable parser state at 0x0864d6c8 */
typedef struct spawnVar_s {
    char *key;
    char *value;
} spawnVar_t;
typedef struct spawnVarState_s {
    byte parsing;                  /* 0x0 nonzero while an entity is being parsed */
    byte pad_01[3];
    int numSpawnVars;              /* 0x4 */
    spawnVar_t spawnVars[64];      /* 0x8 (capacity assumed) */
} spawnVarState_t;
COD2_ASSERT_OFFSET(spawnVarState_t, numSpawnVars, 4);
COD2_ASSERT_OFFSET(spawnVarState_t, spawnVars, 8);

/* Gametype list entry (0x84 bytes) at 0x0884d72c; the payload at +0x40 is what G_FindGametypeDescriptionByName returns. */
typedef struct cod2_gametype_s {
    char name[0x40];            /* 0x00 */
    char description[0x40];     /* 0x40 */
    int teamBased;              /* 0x80 */
} cod2_gametype_s;
COD2_ASSERT_SIZE(cod2_gametype_s, 0x84);

/* IP filter pair at 0x08850e00 (1024 entries); compare == 0xffffffff marks a free slot */
typedef struct ipFilter_s {
    uint32 mask;
    uint32 compare;
} ipFilter_t;
COD2_ASSERT_SIZE(ipFilter_t, 8);

/* ---- globals absent from cod2_globals.h under these names (the data image defines them at the original addresses) ---- */
extern int g_0884d70c, g_0884d710, g_0884d714, g_0884d718, g_0884d71c;
extern int g_0884e7b0, g_0884e7b4;
extern uint16 g_087a2108;
extern int g_087a2110, g_dummyEntity, g_087a209c;
extern struct dvar_s *g_087a2414, *g_087a243c, *g_087a2484;
extern int g_08852e00;             /* number of IP filters */

#endif
