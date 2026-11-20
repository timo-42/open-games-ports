/*
 * Chunk 046 (0x080f56f8..0x080fe9ca): game-module client lifecycle (ClientConnect/Begin/
 * Spawn/Disconnect/Think/EndFrame), player-state to entity-state conversion, spectator and
 * intermission frames, the player script field table, the player script method table
 * and the give/score client commands.
 * Layouts are recovered from consistent field offsets in this chunk only; unrecovered
 * regions are explicit pad_XXXX members.
 */
#ifndef COD2_CHUNK_046_TYPES_H
#define COD2_CHUNK_046_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

struct gclient_s;
struct gentity_s;



/* The binary copies/compares these canonical float (or script-float) members as raw integer words
 * (mov/cmp/add on the dword); COD2_INT_BITS reads or writes the same four bytes as an int. */
#define COD2_INT_BITS(x) (*(int *)&(x))

/* ---- recovered structures ---- */

/* usercmd_s: canonical, see cod2_canon.h */

/* playerState_s: canonical, see cod2_canon.h */

/* gclient_s: canonical, see cod2_canon.h */

/* gentity_s: canonical, see cod2_canon.h */

typedef struct clientInfo_s {
	int field_00;
	int field_04;
	int clientNum;
	char name[32];
	int team;
	char pad_0030[0x10];
	char modelName[64];
	char attachModelNames[384];
	char attachTagNames[384];
	char pad_0380[0x60];
	float field_3e0;
	int field_3e4;
	float viewangles[3];
	char pad_03f4[0xb0];
	int field_4a4;
	char pad_04a8[0x10];
} clientInfo_t;
COD2_ASSERT_SIZE(clientInfo_t, 0x4b8);
COD2_ASSERT_OFFSET(clientInfo_t, field_00, 0x0);
COD2_ASSERT_OFFSET(clientInfo_t, field_04, 0x4);
COD2_ASSERT_OFFSET(clientInfo_t, clientNum, 0x8);
COD2_ASSERT_OFFSET(clientInfo_t, name, 0xc);
COD2_ASSERT_OFFSET(clientInfo_t, team, 0x2c);
COD2_ASSERT_OFFSET(clientInfo_t, modelName, 0x40);
COD2_ASSERT_OFFSET(clientInfo_t, attachModelNames, 0x80);
COD2_ASSERT_OFFSET(clientInfo_t, attachTagNames, 0x200);
COD2_ASSERT_OFFSET(clientInfo_t, field_3e0, 0x3e0);
COD2_ASSERT_OFFSET(clientInfo_t, field_3e4, 0x3e4);
COD2_ASSERT_OFFSET(clientInfo_t, viewangles, 0x3e8);
COD2_ASSERT_OFFSET(clientInfo_t, field_4a4, 0x4a4);

/* level_locals_s: canonical, see cod2_canon.h */

/* dvar_s: canonical, see cod2_canon.h */

/* Buffer filled by SV_GetPlayerStateFromCache (archived client lookup): second dword is the team index. */
typedef struct archivedClientInfo_s {
	int field_00;
	int team;
} archivedClientInfo_t;
COD2_ASSERT_SIZE(archivedClientInfo_t, 0x8);

/* Movement context passed to PM_Pmove (Pmove). Only the members touched here are named. */
/* pmove_s: canonical, see cod2_canon.h */

/* Context handed to WeaponView_Accumulate (first of two ps-derived view-effect calculators). */
typedef struct viewCalcA_s {
	playerState_t *ps;
	int field_2840;
	int time;
	int field_2848;
	int field_2844;
	float field_e9528;
	float deltaSeconds;
	int field_287c;
	int *field_2898;
} viewCalcA_t;
COD2_ASSERT_SIZE(viewCalcA_t, 0x24);
COD2_ASSERT_OFFSET(viewCalcA_t, field_e9528, 0x14);
COD2_ASSERT_OFFSET(viewCalcA_t, field_2898, 0x20);

/* Context handed to WeaponView_ComputeOffset (second view-effect calculator). */
typedef struct viewCalcB_s {
	playerState_t *ps;
	float field_e9528;
	float deltaSeconds;
	float field_2870[3];
	int field_287c;
	int time;
	int field_2840;
	int field_2848;
	int field_2844;
	float field_2880[3];
	float field_288c[3];
	float field_2864[3];
	int *field_2898;
} viewCalcB_t;
COD2_ASSERT_SIZE(viewCalcB_t, 0x54);
COD2_ASSERT_OFFSET(viewCalcB_t, field_2870, 0xc);
COD2_ASSERT_OFFSET(viewCalcB_t, field_287c, 0x18);
COD2_ASSERT_OFFSET(viewCalcB_t, field_2880, 0x2c);
COD2_ASSERT_OFFSET(viewCalcB_t, field_2898, 0x50);

/* Weapon definition returned by BG_GetWeaponDef; only two members are read in this chunk. */
/* weaponDef_s: canonical, see cod2_canon.h */

/* Entry of the player script field table at 0x08157ce0 (13 entries, null-name terminated). */
struct playerFieldDef_s;
typedef void (*playerFieldHandler_t)(int obj, const struct playerFieldDef_s *def);
typedef struct playerFieldDef_s {
	const char *name;
	int offset;
	int type;
	playerFieldHandler_t setter;
	playerFieldHandler_t getter;
} playerFieldDef_t;
COD2_ASSERT_SIZE(playerFieldDef_t, 0x14);

/* Entry of the player script method table at 0x08158a40 (59 entries, null-name terminated). */
typedef struct playerMethodDef_s {
	const char *name;
	void *handler;
	int field_08;
} playerMethodDef_t;
COD2_ASSERT_SIZE(playerMethodDef_t, 0xc);

static inline float bits_to_float(int v) {
	union { int i; float f; } u;
	u.i = v;
	return u.f;
}

/* ---- globals (addresses in the original image) ---- */

extern gentity_t g_entities[64];        /* 0x08716400, stride 0x230 */
extern level_locals_t level;            /* 0x0864c380 */
extern uint16 g_scrCallbackMenuResponse;
extern dvar_t *g_087a2458;
extern dvar_t *g_087a2444;
extern dvar_t *g_087a2414;
extern dvar_t *g_087a248c;
extern dvar_t *g_087a2440;
extern dvar_t *g_087a24b4;
extern dvar_t *g_087a2404;
extern dvar_t *g_087a2460;
extern dvar_t *g_087a2464;
extern dvar_t *g_087a245c;
extern dvar_t *dvar_cheats;

/* ---- out-of-chunk callees (placeholders; merge queue renames) ---- */


#endif
