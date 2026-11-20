/*
 * chunk 103 types: GSC player method handlers (PlayerCmd_*) recovered from .eh_frame FDE starts,
 * 0x080fa2ac .. 0x080fd990. Link-wave migration: gclient_s, gentity_s, dvar_s and weaponDef_s now come
 * from cod2_canon.h. entity_handler_s and ent_ext_s are only forward-declared there (no canonical
 * layout), so this chunk keeps private views of the two records it touches; the migration map's
 * replacement (entClass103_s -> entity_handler_s, corpse103_s -> ent_ext_s) is applied as a cast of
 * the canonical global (g_entityHandlerTable / g_trackedEntitySlots) to the view. Offsets observed from disassembly of
 * cod2_lnxded 1.3.
 */
#ifndef COD2_CHUNK_103_TYPES_H
#define COD2_CHUNK_103_TYPES_H

struct XAnimTree;

/* ---- per-entity-class callback table: stride 0x28 at 0x08187d00; pain at +0x14, die at +0x18 ---- */
typedef void (*entPain103_t)(struct gentity_s *self, struct gentity_s *attacker, int damage,
                             const float *point, int mod, const float *dir, int hitLoc);
typedef void (*entDie103_t)(struct gentity_s *self, struct gentity_s *inflictor, struct gentity_s *attacker,
                            int damage, int mod, int weapon, const float *dir, int hitLoc, int psOffsetTime);
struct entClass103_s {
    uint8 pad00[0x14];
    entPain103_t pain;                /* 0x14 */
    entDie103_t die;                  /* 0x18 */
    uint8 pad1c[0x28 - 0x1c];
};
COD2_ASSERT_SIZE(struct entClass103_s, 0x28);
COD2_ASSERT_OFFSET(struct entClass103_s, pain, 0x14);
COD2_ASSERT_OFFSET(struct entClass103_s, die, 0x18);

/* ---- clone-player body record: array at 0x0884e7b8, stride 0x4c8 ---- */
struct corpse103_s {
    int handle;                       /* 0x000 */
    int entNum;                       /* 0x004 */
    int time;                         /* 0x008 */
    uint8 data[0x4b8];                /* 0x00c copy of the 0x4b8 per-client record at g_clientInfos[clientNum] */
    uint8 active;                     /* 0x4c4 */
    uint8 pad4c5[3];
};
COD2_ASSERT_SIZE(struct corpse103_s, 0x4c8);
COD2_ASSERT_OFFSET(struct corpse103_s, data, 0x0c);
COD2_ASSERT_OFFSET(struct corpse103_s, active, 0x4c4);

/* ---- globals not (yet) declared in cod2_globals.h; the data image defines them under these names ---- */
extern int level_time;                         /* 0x0864c56c level.time (ms) */
extern dvar_t *g_087a241c;                     /* float: damage kick scale divisor input */
extern dvar_t *g_087a2454;                     /* float: max clone velocity (x,y) */
extern dvar_t *g_087a2468;                     /* int: istalking window (ms) */
extern uint16 g_scrCallbackDamage;             /* script string id notified by finishplayerdamage */

#define ENT_CLASS103(idx) (((struct entClass103_s *)g_entityHandlerTable)[(idx)])

/* ---- helpers shared by the player methods (the originals inline these) ---- */
static inline uint32 FloatBits103(float f)
{
    uint32 bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

static inline const char *Scr_ParamText103(uint32 param)
{
    return Scr_ReleaseAnyValue(param);
}

static inline void Scr_GetVec3_103(uint32 param, float *out)
{
    Scr_GetVector(param, out);
}

/* entref: low 16 bits entity number, high 16 bits class number (0 = plain entity). */
static inline struct gentity_s *Scr_GetPlayerEntity103(uint32 entref)
{
    struct gentity_s *ent;

    if ((entref >> 16) == 0) {
        ent = &g_gentities[entref & 0xffff];
        if (ent->client == 0) {
            Scr_ErrorNoParam(va("entity %i is not a player", (uint16)entref));
        }
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = 0;
    }
    return ent;
}

#endif /* COD2_CHUNK_103_TYPES_H */
