/*
 * chunk_043 types: player-movement slide/step code, small vector helpers and the
 * BG weapon-definition tables of cod2_lnxded 1.3 (0x080e982e..0x080ed440).
 * Layouts are recovered from consistent field offsets in this chunk only. Fields that
 * this chunk never touches are padding. Names marked "inferred" follow the Quake III
 * counterpart; names of the form fieldNN are unknown.
 */
#ifndef CHUNK_043_TYPES_H
#define CHUNK_043_TYPES_H

#include <strings.h>

#define PM043_GROUND_NONE 0x3ff /* observed: groundEntityNum compared with 0x3ff */
#define PM043_MAX_WEAPONS_ARRAY 136
#define PM043_MAX_CLIP_PLANES 8 /* observed: bump loop aborts when numplanes >= 8 */

/* ---- item table entry (stride 0x2c, table at 0x081850c0) ---- */
struct bgItem043_s {
    int field00;                 /* 0x00 */
    const char *fromWeapon98;            /* 0x04 <- WeaponDef.field98 */
    const char *fromWeapon1b4;           /* 0x08 <- WeaponDef.field1b4 */
    int zero0c;                  /* 0x0c */
    const char *fromWeapon1b8;           /* 0x10 <- WeaponDef.field1b8 */
    const char *name;            /* 0x14 <- WeaponDef.field04Name */
    int fromWeapon1c0;           /* 0x18 <- WeaponDef.field1c0 */
    int itemType;                /* 0x1c (1 = weapon entry written here, 2 = ammo entry fixed up by BG_FillInAmmoItems) */
    int weaponIndex;             /* 0x20 */
    int ammoIndex;               /* 0x24 */
    int clipIndex;               /* 0x28 */
};
typedef struct bgItem043_s bgItem043_t;
COD2_ASSERT_SIZE(bgItem043_s, 0x2c);
COD2_ASSERT_OFFSET(bgItem043_s, name, 0x14);
COD2_ASSERT_OFFSET(bgItem043_s, itemType, 0x1c);
COD2_ASSERT_OFFSET(bgItem043_s, weaponIndex, 0x20);
COD2_ASSERT_OFFSET(bgItem043_s, clipIndex, 0x28);

typedef void (*BGWeaponLoadedCallback043)(int weaponIndex);

/* BG item table (stride 0x2c) at 0x081850c0; cod2_globals.h declares it as g_081850c0 of the
 * opaque canonical gitem_s, so this chunk views it through its private entry layout. */
#define bg_itemList ((bgItem043_t *)g_081850c0)

/* ---- in-chunk functions used before definition ---- */
weaponDef_t *BG_GetWeaponDef(int weaponIndex);
int BG_GetNumWeapons(void);
int BG_GetClipTypeSize(int clipIndex);
int BG_GetAmmoTypeMax(int ammoIndex);
int BG_GetSharedAmmoCapMax(int index);
int BG_RegisterWeapon(const char *name, BGWeaponLoadedCallback043 callback);
int BG_GetCurrentWeapon(playerState_t *ps);
int BG_GetWeaponClipIndex(int weaponIndex);
int BG_GetWeaponAmmoIndex(int weaponIndex);
int BG_GetWeaponClipOnly(int weaponIndex);
int BG_FindWeaponSlot(playerState_t *ps, int weaponIndex, int followAlt);
int BG_GetWeaponTotalAmmo(playerState_t *ps, int weaponIndex);
int BG_FindWeaponByName(const char *name);
int BG_RegisterWeaponDef(weaponDef_t *def, BGWeaponLoadedCallback043 callback);
void BG_FillInWeaponIndices(int weaponIndex);
float VectorLength_080e99e8(const vec_t *v);
void VectorCopy_080eaede(const vec_t *in, vec_t *out);
void ClearVector3(vec_t *v);
float DotProduct3(const vec_t *a, const vec_t *b);
float DotProduct3_alt(const vec_t *a, const vec_t *b);
void VectorSubtract2D(const vec_t *a, const vec_t *b, vec_t *out);
float DotProduct2D(const vec_t *a, const vec_t *b);
void VectorSet3(vec_t *v, vec_t x, vec_t y, vec_t z);
void VectorAdd3_alt(const vec_t *a, const vec_t *b, vec_t *out);
void VectorScale3_alt(const vec_t *in, float scale, vec_t *out);
void VectorMA3_alt(const vec_t *base, float scale, const vec_t *dir, vec_t *out);
float Q_fabs043(float x);
int Q_roundToInt043(float x);
float PM_SortPlanesByDot(const vec_t *velocity, int numPlanes, const vec_t *planes, int *order);
int PM_SlideMove(pmove_t *pm, pml_t *pml, int gravity);
void PM_StepSlideMove(pmove_t *pm, pml_t *pml, int gravity);

#endif
