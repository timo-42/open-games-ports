#include <ctype.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_046_types.h"


/* C float-to-int conversion: GCC 3.3 emits fldcw (RC = truncate) + fistp around every (int) cast in
 * this chunk (all fistp sites between 0x080f56f8 and 0x080fe9ca are bracketed by fldcw). */
static inline int trunc_to_int(double v)
{
	return (int)v;
}

/* forward declarations for in-chunk functions */
void VectorCopy_080f7c82(const float *src, float *dst);
void VectorCopy_080f9b06(const float *src, float *dst);
void VectorMA_080f9b36(const float *a, float scale, const float *b, float *out);
double Vec2LengthSq(const float *v);
float Vec2Length(const float *v);
void Vec2Subtract(const float *a, const float *b, float *out);
void Vec2Subtract_080f9ada(const float *a, const float *b, float *out);
void Vec2Scale(const float *in, float scale, float *out);
void VectorClear_080f7c34(float *v);
void VectorSet_080f7c5e(float *v, float x, float y, float z);
void VectorAdd_080f7cb2(const float *a, const float *b, float *out);
float Sqrt_float(float x);
double DotProduct_080f7d3a(const float *a, const float *b);
void G_PlayerStateToEntityState(playerState_t *ps, gentity_t *ent, int trTime, int snapOrigin);
void G_SpawnMantleBlocker(const float *origin, int time, pmove_t *pm);
void ClientThink_real(gentity_t *ent, usercmd_t *ucmd);
void ClientEndFrame(gentity_t *ent);
void ClientEndFrame_Intermission(gentity_t *ent);
void ClientEndFrame_Spectator(gentity_t *ent);
int Client_SpectateBitClear(gclient_t *client, byte bit);
int G_UpdateClientAttachments(gentity_t *ent);
int G_PlayerCollisionPush(gentity_t *ent);
void G_HandleClientEvent(int entityNum, int event);
void SetClientViewAngle(gentity_t *ent, const float *angle);
void G_GetPlayerViewOrigin(gentity_t *ent, float *out);
void G_GetPlayerViewAxis(gentity_t *ent, float *forward, float *right, float *up);
void ClientCleanName(const char *in, char *out, int outSize);
void ClientUserinfoChanged(int clientNum);
void ClientSpawn(gentity_t *ent, const float *origin, const float *angles);
unsigned int G_FindTargetPlayerEncoded(gentity_t *ent, const float *origin, int startIndex);
void ScoreboardMessage(gentity_t *ent);
int CheatsOk(gentity_t *ent);
char *ConcatArgs(int start);
void SanitizeString(const char *in, char *out);

// 0x080f56f8  size=1340  FUN_080f56f8  status=translated  naming=inferred
// Name inferred (BG_PlayerStateToEntityState counterpart): copies origin/velocity/viewangles/eFlags/event
// ring from the player state into the entity state; callers pass (ps, ent, ps->commandTime, 1).
void G_PlayerStateToEntityState(playerState_t *ps, gentity_t *ent, int trTime, int snapOrigin)
{
	ent->s.pos.trType = 3;
	VectorCopy_080f7c82(ps->origin, ent->s.pos.trBase);
	VectorCopy_080f7c82(ps->velocity, ent->s.pos.trDelta);
	ent->s.pos.trTime = trTime;
	ent->s.pos.trDuration = 0x32;
	ent->s.apos.trType = 1;
	VectorCopy_080f7c82(ps->viewangles, ent->s.apos.trBase);
	ent->s.angles2[1] = (float)ps->movementDir;
	ent->s.eFlags = ps->eFlags;
	if (ps->entityEventSequence - ps->eventSequence < 0) {
		if (4 < ps->eventSequence - ps->entityEventSequence) {
			ps->entityEventSequence = ps->eventSequence - 4;
		}
		ent->s.eventParm = (int)(byte)ps->eventParms[ps->entityEventSequence & 3];
		ps->entityEventSequence = ps->entityEventSequence + 1;
	} else {
		ent->s.eventParm = 0;
	}
	if (0 < ps->oldEventSequence - ps->eventSequence) {
		ps->oldEventSequence = ps->eventSequence;
	}
	for (unsigned int seq = (unsigned int)ps->oldEventSequence; seq != (unsigned int)ps->eventSequence; seq = seq + 1) {
		byte evt = (byte)ps->events[seq & 3];
		G_HandleClientEvent(ent->s.number, evt);
		int j;
		for (j = 0; 0 < g_pmLocalOnlyEventIds[j] && (unsigned int)g_pmLocalOnlyEventIds[j] != (unsigned int)evt; j = j + 1) {
		}
		if (g_pmLocalOnlyEventIds[j] < 0) {
			ent->s.events[ent->s.eventSequence & 3] = (int)evt;
			ent->s.eventParms[ent->s.eventSequence & 3] = (int)(byte)ps->eventParms[seq & 3];
			ent->s.eventSequence = ent->s.eventSequence + 1;
		}
	}
	ps->oldEventSequence = ps->eventSequence;
	ent->s.weapon = (int)(byte)ps->weapon;
	ent->s.groundEntityNum = (int)(uint16)ps->groundEntityNum; /* movzx word [ps+0x60] (0x080f5941) */
	if ((ps->pm_flags & 0xc00000) == 0) {
		ent->s.eType = 5;
	} else {
		ent->s.eType = 1;
	}
	if (snapOrigin != 0) {
		ent->s.pos.trBase[0] = (float)trunc_to_int(ent->s.pos.trBase[0]);
		ent->s.pos.trBase[1] = (float)trunc_to_int(ent->s.pos.trBase[1]);
		ent->s.pos.trBase[2] = (float)trunc_to_int(ent->s.pos.trBase[2]);
		ent->s.apos.trBase[0] = (float)trunc_to_int(ent->s.apos.trBase[0]);
		ent->s.apos.trBase[1] = (float)trunc_to_int(ent->s.apos.trBase[1]);
		ent->s.apos.trBase[2] = (float)trunc_to_int(ent->s.apos.trBase[2]);
	}
	ent->s.legsAnim = ps->legsAnim;
	ent->s.torsoAnim = ps->torsoAnim;
	ent->s.clientNum = ps->clientNum;
	if ((ps->eFlags & 0x300) != 0) {
		ent->s.otherEntityNum = ps->viewlocked_entNum;
	}
	if (ps->pm_type < 6) {
		ent->s.eFlags = ent->s.eFlags & 0xfffdffff;
	} else {
		ent->s.eFlags = ent->s.eFlags | 0x20000;
	}
	if ((ps->pm_flags & 0x40) == 0) {
		ent->s.eFlags = ent->s.eFlags & 0xfffbffff;
	} else {
		ent->s.eFlags = ent->s.eFlags | 0x40000;
	}
	COD2_INT_BITS(ent->s.leanf) = COD2_INT_BITS(ps->leanf); /* mov dword (0x080f5b02) */
	int stateKind = PM_StanceIndex(ps);
	if (stateKind == 1) {
		int duration = PM_StanceDurationMs(ps, ps->viewHeightLerpTarget, ps->viewHeightLerpDown);
		float frac = (float)(ps->commandTime - ps->viewHeightLerpTime) / (float)duration;
		if (frac < 0.0) {
			frac = 0.0;
		} else if (1.0 < frac) {
			frac = 1.0;
		}
		if (ps->viewHeightLerpDown == 0) {
			frac = 1.0 - frac;
		}
		ent->s.fTorsoHeight = ps->fTorsoHeight * frac;
		ent->s.fTorsoPitch = (float)((double)AngleNormalize180(ps->fTorsoPitch) * (double)frac);
		ent->s.fWaistPitch = (float)((double)AngleNormalize180(ps->fWaistPitch) * (double)frac);
	} else {
		ent->s.fTorsoHeight = 0;
		ent->s.fTorsoPitch = 0;
		ent->s.fWaistPitch = 0;
	}
}

// 0x080f5c34  size=250  FUN_080f5c34  status=translated  naming=unknown
// Spawns a short-lived temp entity owned by the client in pm->ps (contents/clipmask 0x10000, svFlags 0x21,
// eType 5), copying the owner's bounds, placing it at origin and scheduling it (time + level.time + dvar).
void G_SpawnMantleBlocker(const float *origin, int time, pmove_t *pm)
{
	int ownerNum = pm->ps->clientNum;
	gentity_t *temp = G_Spawn();
	temp->parent = &g_entities[ownerNum];
	temp->r.ownerNum = pm->ps->clientNum;
	temp->r.contents = 0x10000;
	temp->clipmask = 0x10000;
	temp->r.svFlags = 0x21;
	temp->s.eType = 5;
	temp->handler = 0x13;
	VectorCopy_080f7c82(g_entities[ownerNum].r.mins, temp->r.mins);
	VectorCopy_080f7c82(g_entities[ownerNum].r.maxs, temp->r.maxs);
	G_SetPositionTrajectory(temp, origin);
	SV_LinkEntity(temp);
	temp->nextthink = time + level.time + g_087a2458->current.integer;
}

// 0x080f5d2e  size=2249  FUN_080f5d2e  status=translated  naming=inferred
// ClientThink_real counterpart: clamps the command time, runs the session-state specific think
// (intermission/spectator) or Pmove for playing clients, then updates entity state.
void ClientThink_real(gentity_t *ent, usercmd_t *ucmd)
{
	gclient_t *client = ent->client;
	if (client->connected != 2) {
		return;
	}
	if (level.time + 200 < ucmd->serverTime) {
		ucmd->serverTime = level.time + 200;
	}
	if (ucmd->serverTime < level.time + -1000) {
		ucmd->serverTime = level.time + -1000;
	}
	int msec = ucmd->serverTime - client->ps.commandTime;
	if (!(0 < msec || client->ps.clientNum != (int)(ent - g_entities))) {
		return;
	}
	if (200 < msec) {
		msec = 200;
	}
	if (client->bFrozen == 0) {
		client->ps.pm_flags = client->ps.pm_flags & 0xffff7fff;
	} else {
		client->ps.pm_flags = client->ps.pm_flags | 0x8000;
	}
	if (client->sessionState == 3) {
		Client_UpdateButtonEdges(ent, ucmd);
		return;
	}
	if (client->sessionState == 2) {
		Client_ProcessButtonsAndDrop(ent, ucmd);
		return;
	}
	if (Client_CheckInactivity(client) == 0) {
		return;
	}

	int oldEventSequence = client->ps.eventSequence;
	pmove_t pm;
	memset(&pm, 0, 0xf8);
	pm.ps = &client->ps;
	pm.cmd = *ucmd;
	pm.oldcmd = client->oldcmd;
	if (client->ps.pm_type < 6) {
		pm.tracemask = 0x2810011;
	} else {
		pm.tracemask = 0x810011;
	}
	pm.handler = 1;
	VectorCopy_080f7c82(client->ps.origin, client->oldOrigin);
	client->oldbuttons = client->buttons;
	client->buttons = client->cmd.buttons;
	client->latched_buttons = ~client->oldbuttons & client->buttons;
	client->buttonsSinceLastFrame = client->latched_buttons | client->buttonsSinceLastFrame;

	viewCalcA_t ctxA;
	ctxA.ps = &client->ps;
	ctxA.field_2840 = client->recoilStamp;
	ctxA.time = level.time;
	ctxA.field_2848 = COD2_INT_BITS(client->recoilPitchNeg);
	ctxA.field_2844 = COD2_INT_BITS(client->recoilPitchOut);
	ctxA.field_e9528 = PM_GetMoveSpeed(&client->ps, level.time);
	ctxA.deltaSeconds = (float)((double)msec * 0.001);
	ctxA.field_287c = client->viewInt287c;
	ctxA.field_2898 = &client->viewInt2898;

	float viewOffset[3];
	float viewAnglesOut[3];
	WeaponView_Accumulate((struct view_motion_s *)&ctxA /* chunk-private view of view_motion_s */, viewOffset);
	VectorAdd_080f7cb2(client->ps.viewangles, viewOffset, viewAnglesOut);

	weaponDef_t *weaponDef = BG_GetWeaponDef(client->ps.weapon);
	int weaponTimeLeft = (client->ps.shellshockDuration + client->ps.shellshockTime) - ctxA.time;
	float adsScale;
	if (weaponTimeLeft < 1) {
		adsScale = 1.0f;
	} else {
		float t = 1.0f;
		if (weaponTimeLeft < 3000) {
			t = (float)weaponTimeLeft / 3000.0f;
		}
		t = t * (3.0f - (t + t)) * t;
		adsScale = (weaponDef->swayShellShockScale - 1.0f) * t + 1.0f;
	}
	(void)adsScale;

	WeaponView_ApplyAngleSpring(&client->ps, client->viewVec284c, (struct view_spring_s *)client->viewVec2858, client->viewVec2864, 1.0f, msec);

	viewCalcB_t ctxB;
	ctxB.ps = ctxA.ps;
	ctxB.field_e9528 = ctxA.field_e9528;
	ctxB.deltaSeconds = (float)((double)msec * 0.001);
	VectorCopy_080f7c82(client->viewVec2870, ctxB.field_2870);
	ctxB.field_287c = client->viewInt287c;
	ctxB.time = ctxA.time;
	ctxB.field_2840 = client->recoilStamp;
	ctxB.field_2848 = COD2_INT_BITS(client->recoilPitchNeg);
	ctxB.field_2844 = COD2_INT_BITS(client->recoilPitchOut);
	VectorCopy_080f7c82(client->viewVec2880, ctxB.field_2880);
	VectorCopy_080f7c82(client->viewVec288c, ctxB.field_288c);
	VectorCopy_080f7c82(client->viewVec2864, ctxB.field_2864);
	ctxB.field_2898 = &client->viewInt2898;
	WeaponView_ComputeOffset((struct view_motion_s *)&ctxB /* chunk-private view of view_motion_s */, viewOffset);

	if (BG_GetWeaponAimDownSight(ctxB.ps->weapon) != 0 &&
	    (ctxB.ps->fWeaponPosFrac != 0.0f || (ctxB.ps->fWeaponPosFrac != ctxB.ps->fWeaponPosFrac)) &&
	    weaponDef->adsOverlayReticle == 0) {
		float axisA[12];
		float axisB[12];
		float axisC[12];
		AnglesToAxis(viewOffset, axisA);
		AnglesToAxis(viewAnglesOut, axisB);
		Mat3_Multiply(axisA, axisB, axisC);
		AxisToAngles(axisC, viewAnglesOut);
	}
	VectorCopy_080f7c82(ctxB.field_2870, client->viewVec2870);
	client->viewInt287c = ctxB.field_287c;
	VectorCopy_080f7c82(ctxB.field_2880, client->viewVec2880);
	VectorCopy_080f7c82(ctxB.field_288c, client->viewVec288c);
	client->fGunPitch = viewAnglesOut[0];
	client->fGunYaw = viewAnglesOut[1];

	PM_Pmove(&pm);
	if (pm.mantleStarted != 0) {
		G_SpawnMantleBlocker(pm.mantleEndPos, pm.mantleDuration, &pm);
	}
	if ((ent->client->ps.pm_flags & 2) == 0) {
		ent->s.animMovetype = 0;
	} else {
		ent->s.animMovetype = 1;
	}
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
		ent->r.eventTime = level.time;
	}
	if (g_087a248c->current.enabled == 0) {
		BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, 1, 1);
	} else {
		G_PlayerStateToEntityState(&ent->client->ps, ent, ent->client->ps.commandTime, 1);
	}
	VectorCopy_080f7c82(ent->s.pos.trBase, ent->r.currentOrigin);
	VectorCopy_080f7c82(pm.mins, ent->r.mins);
	VectorCopy_080f7c82(pm.maxs, ent->r.maxs);
	Client_DispatchEvents(ent, oldEventSequence);
	SV_LinkEntity(ent);
	if (ent->client->noclip == 0) {
		Entity_TouchLinked(ent);
	}
	VectorCopy_080f7c82(ent->client->ps.origin, ent->r.currentOrigin);
	VectorClear_080f7c34(ent->r.currentAngles);
	ent->r.currentAngles[1] = ent->client->ps.viewangles[1];
	Entity_TouchList(ent, &pm);
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}
	G_UpdateUseState(ent);
}

// 0x080f65f8  size=208  FUN_080f65f8  status=translated  naming=inferred
// ClientThink counterpart: copies the previous usercmd, fetches the new one for the client and runs
// ClientThink_real unless the dvar g_087a2440 defers it to the frame loop.
void ClientThink(int clientNum)
{
	g_gameContextPtr = (uint8 *)g_levelBlock;
	gclient_t *client = g_entities[clientNum].client;
	client->oldcmd = client->cmd;
	SV_GetClientUsercmd(clientNum, &g_entities[clientNum].client->cmd);
	g_entities[clientNum].client->lastCmdTime = level.time;
	if (g_087a2440->current.enabled == 0) {
		ClientThink_real(&g_entities[clientNum], &g_entities[clientNum].client->cmd);
	}
	g_gameContextPtr = 0;
}

// 0x080f66c8  size=335  FUN_080f66c8  status=translated  naming=unknown
// Per-frame client step (called from G_RunEntityState): runs ClientThink_real when g_087a2440 defers it, then
// handles the pm_type 1<->0 / 7<->6 dead/alive step or respawn placement.
void G_ClientRunFrame(gentity_t *ent)
{
	if (g_087a2440->current.enabled != 0) {
		ent->client->cmd.serverTime = level.time;
		ClientThink_real(ent, &ent->client->cmd);
	}
	if (ent->client->noclip == 0) {
		if (ent->tagInfo == 0) {
			if (ent->client->ps.pm_type == 1 || ent->client->ps.pm_type == 7) {
				ent->client->ps.pm_type = ent->client->ps.pm_type + -1;
			}
		} else {
			int newType;
			if (ent->client->sessionState == 1) {
				newType = 7;
			} else {
				newType = 1;
			}
			ent->client->ps.pm_type = newType;
			Ent_SetAttachCurrent(ent, 2);
			G_SetPositionTrajectory(ent, ent->r.currentOrigin);
			G_SetAnglesTrajectory(ent, ent->r.currentAngles);
			ent->s.pos.trType = 1;
			ent->s.apos.trType = 1;
			SV_LinkEntity(ent);
			VectorCopy_080f7c82(ent->r.currentOrigin, ent->client->ps.origin);
		}
	}
}

// 0x080f6818  size=253  FUN_080f6818  status=translated  naming=inferred
// Intermission end-frame (dispatched from ClientEndFrame when sessionState == 3): non-solid spectator-style
// pm_type 5, clears flag bits and publishes two level values as configstrings 5 and 6.
void ClientEndFrame_Intermission(gentity_t *ent)
{
	gclient_t *client = ent->client;
	ent->r.svFlags = ent->r.svFlags & 0xfd;
	ent->r.svFlags = ent->r.svFlags | 1;
	ent->takedamage = 0;
	ent->r.contents = 0;
	client->ps.pm_flags = client->ps.pm_flags & 0xfc7fffff;
	client->ps.pm_type = 5;
	client->ps.eFlags = client->ps.eFlags & 0xffdfffff;
	client->ps.eFlags = client->ps.eFlags & 0xffffffbf;
	client->ps.viewmodelIndex = 0;
	ent->s.eType = 5;
	char *text = va("%i", level.teamScores[0]);
	SV_SetConfigstring(5, text);
	text = va("%i", level.teamScores[1]);
	SV_SetConfigstring(6, text);
}

// 0x080f6916  size=1209  FUN_080f6916  status=translated  naming=inferred
// Spectator end-frame (dispatched when sessionState == 2): resolves the followed client from archived
// player states, copies it over the spectator's player state or falls back to free spectating.
void ClientEndFrame_Spectator(gentity_t *ent)
{
	gclient_t *client = ent->client;
	playerState_t archived;
	clientState_t info;
	int queryTime;

	ent->r.svFlags = ent->r.svFlags & 0xfd;
	ent->r.svFlags = ent->r.svFlags | 1;
	ent->takedamage = 0;
	ent->r.contents = 0;
	client->ps.pm_flags = client->ps.pm_flags & 0xff7fffff;
	ent->s.eType = 5;
	client->ps.viewmodelIndex = 0;
	client->fGunPitch = 0;
	client->fGunYaw = 0;
	if (-1 < client->spectatorClient) {
		client->lastFollowedClient = client->spectatorClient;
		while (true) {
			if (client->archiveTime < 0) {
				client->archiveTime = 0;
			}
			queryTime = client->archiveTime - client->psOffsetTime;
			if (SV_GetPlayerStateFromCache(client->spectatorClient, &queryTime, &archived, &info) != 0 &&
			    Client_SpectateBitClear(client, (byte)info.team) != 0) {
				goto have_state;
			}
			if (client->archiveTime == 0) {
				break;
			}
			client->archiveTime = client->archiveTime + -0x32;
		}
		client->spectatorClient = -1;
		client->lastFollowedClient = -1;
	}
	if (client->lastFollowedClient < 0 && Client_SpectateBitClear(client, 4) == 0) {
		Cmd_FollowCycle_f(ent, 1);
	}
	if (-1 < client->lastFollowedClient) {
		queryTime = client->psOffsetTime + client->archiveTime;
		if (SV_GetPlayerStateFromCache(client->lastFollowedClient, &queryTime, &archived, &info) != 0 &&
		    Client_SpectateBitClear(client, (byte)info.team) != 0) {
have_state:
			uint32 keptFlags = client->ps.eFlags & 0x100000 | archived.eFlags & 0xffefffff;
			memcpy(&client->ps, &archived, 0x26a8);
			HudElem_BuildClientLists(client, ent->s.number, 2);
			client->ps.eFlags = keptFlags;
			client->ps.pm_flags = client->ps.pm_flags & 0xff7fffff;
			client->ps.pm_flags = client->ps.pm_flags | 0x400000;
			if (-1 < client->spectatorClient) {
				client->ps.pm_flags = client->ps.pm_flags & 0xfcffffff;
				return;
			}
			client->ps.pm_flags = client->ps.pm_flags | 0x1000000;
			if (Client_SpectateBitClear(client, 4) == 0) {
				client->ps.pm_flags = client->ps.pm_flags & 0xfdffffff;
				return;
			}
			client->ps.pm_flags = client->ps.pm_flags | 0x2000000;
			return;
		}
	}
	StopFollowing(ent);
	client->ps.pm_flags = client->ps.pm_flags & 0xfdffffff;
	if (Client_SpectateBitClear(client, 2) == 0 && Client_SpectateBitClear(client, 1) == 0 &&
	    Client_SpectateBitClear(client, 0) == 0) {
		client->ps.pm_flags = client->ps.pm_flags & 0xfeffffff;
	} else {
		client->ps.pm_flags = client->ps.pm_flags | 0x1000000;
	}
}

// 0x080f6dd0  size=26  FUN_080f6dd0  status=translated  naming=inferred
// Tests bit `bit` of the client's spectate-permission mask (script method allowspectateteam);
// returns 1 when the bit is clear.
int Client_SpectateBitClear(gclient_t *client, byte bit)
{
	return (client->noSpectate >> (bit & 0x1f) ^ 1U) & 1;
}

// 0x080f6dea  size=180  FUN_080f6dea  status=translated  naming=unknown
// Copies the live player state of clientNum into *out (zeroing the tail 0x7a8..0x2728) when the client
// has pm_flags bit 0x800000; returns 1 on copy, 0 otherwise.
int G_CopyClientPlayerState(int clientNum, playerState_t *out)
{
	gclient_t *client = g_entities[clientNum].client;
	if ((client->ps.pm_flags & 0x800000) == 0) {
		return 0;
	}
	memcpy(out, client, 0x26a8);
	memset((char *)out + 0x7a8, 0, 0xf80);
	return 1;
}

// 0x080f6e9e  size=1019  FUN_080f6e9e  status=translated  naming=unknown
// Pushes two overlapping, living, player-contents clients apart in 2D (sets velocity, pm_time=300 and
// pm_flags|=0x200 on both). Returns 1 when a pair was separated, 0 otherwise.
int G_PlayerCollisionPush(gentity_t *ent)
{
	if ((ent->client->ps.pm_flags & 0x800000) != 0 && ent->client->sessionState == 0 &&
	    (ent->r.contents == 0x2000000 || ent->r.contents == 0x4000000)) {
		gentity_t *other = &g_entities[0];
		for (int i = 0; i < level.maxclients; i = i + 1) {
			if (other->r.inuse != 0 && (other->client->ps.pm_flags & 0x800000) != 0 &&
			    other->client->sessionState == 0 && other != ent && other->client != 0 &&
			    0 < other->health && (other->r.contents == 0x2000000 || other->r.contents == 0x4000000) &&
			    other->r.absmin[0] <= ent->r.absmax[0] && ent->r.absmin[0] <= other->r.absmax[0] &&
			    other->r.absmin[1] <= ent->r.absmax[1] && ent->r.absmin[1] <= other->r.absmax[1] &&
			    other->r.absmin[2] <= ent->r.absmax[2] && ent->r.absmin[2] <= other->r.absmax[2]) {
				float delta[3];
				Vec2Subtract(other->r.currentOrigin, ent->r.currentOrigin, delta);
				float radiusSum = ent->r.maxs[0] + other->r.maxs[0];
				if (Vec2LengthSq(delta) <= (double)radiusSum * (double)radiusSum) {
					Vec2Subtract(other->r.currentOrigin, ent->r.currentOrigin, delta);
					delta[0] = (float)((double)delta[0] + (double)Rand_Signed());
					delta[1] = (float)((double)delta[1] + (double)Rand_Signed());
					Vec2_Normalize(delta);
					float speedOther;
					float speedEnt;
					if (0 < Vec2Length(other->client->ps.velocity)) {
						speedOther = (float)g_087a2444->current.integer;
					} else {
						speedOther = 0.0f;
					}
					float pushOther = speedOther;
					if (0 < Vec2Length(ent->client->ps.velocity)) {
						speedEnt = (float)g_087a2444->current.integer;
					} else {
						speedEnt = 0.0f;
					}
					float pushEnt = speedEnt;
					if (pushOther < 0.0001 && speedEnt < 0.0001) {
						pushOther = (float)other->client->ps.speed;
						pushEnt = (float)ent->client->ps.speed;
					}
					Vec2Scale(delta, pushOther, other->client->ps.velocity);
					other->client->ps.pm_time = 300;
					other->client->ps.pm_flags = other->client->ps.pm_flags | 0x200;
					Vec2Scale(delta, -pushEnt, ent->client->ps.velocity);
					ent->client->ps.pm_time = 300;
					ent->client->ps.pm_flags = ent->client->ps.pm_flags | 0x200;
					return 1;
				}
			}
			other = other + 1;
		}
	}
	return 0;
}

// 0x080f72f8  size=541  FUN_080f72f8  status=translated  naming=unknown
// Mirrors the entity's model and six attachment model/tag names into the per-client info block and
// player-state indices; returns true when any name changed.
int G_UpdateClientAttachments(gentity_t *ent)
{
	gclient_t *client = ent->client;
	clientInfo_t *info = &g_clientInfos[ent->s.clientNum];
	const char *name = G_GetModelName(ent->model);
	client->cs.modelindex = (int)ent->model;
	bool changed = strcmp(info->modelName, name) != 0;
	if (changed) {
		Q_strncpyz(info->modelName, name, 0x40);
	}
	for (int i = 0; i < 6; i = i + 1) {
		if (ent->attachModelNames[i] == 0) {
			info->attachModelNames[i * 0x40] = 0;
			info->attachTagNames[i * 0x40] = 0;
			client->cs.attachModelIndex[i] = 0;
			client->cs.attachTagIndex[i] = 0;
		} else {
			name = G_GetModelName(ent->attachModelNames[i]);
			client->cs.attachModelIndex[i] = (int)ent->attachModelNames[i];
			if (strcmp(&info->attachModelNames[i * 0x40], name) != 0) {
				changed = true;
				Q_strncpyz(&info->attachModelNames[i * 0x40], name, 0x40);
			}
			name = ScrStr_TextFromHandle(ent->attachTagNames[i]);
			client->cs.attachTagIndex[i] = G_ItemIndex(name);
			if (strcmp(&info->attachTagNames[i * 0x40], name) != 0) {
				changed = true;
				Q_strncpyz(&info->attachTagNames[i * 0x40], name, 0x40);
			}
		}
	}
	return changed;
}

// 0x080f7516  size=1570  FUN_080f7516  status=translated  naming=inferred
// ClientEndFrame counterpart: dispatches on sessionState (intermission/spectator/playing), refreshes the
// playing client's pm_type, gravity/speed, entity state, view origin target and per-client info.
void ClientEndFrame(gentity_t *ent)
{
	gclient_t *client = ent->client;
	ent->handler = 10;
	client->ps.deltaTime = 0;
	int attachmentsChanged = G_UpdateClientAttachments(ent);
	if (client->connected == 2) {
		if (client->sessionState == 3) {
			ClientEndFrame_Intermission(ent);
			ent->client->buttonsSinceLastFrame = 0;
		} else if (client->sessionState == 2) {
			ClientEndFrame_Spectator(ent);
			ent->client->buttonsSinceLastFrame = 0;
		} else if (client->ps.clientNum == ent->s.number) {
			ent->r.svFlags = ent->r.svFlags | 2;
			ent->r.svFlags = ent->r.svFlags & 0xfe;
			ent->takedamage = 1;
			client->ps.pm_flags = client->ps.pm_flags | 0x800000;
			client->ps.pm_flags = client->ps.pm_flags & 0xfcffffff;
			client->ps.viewmodelIndex = client->viewmodelIndex;
			Entity_SetContentsForClientFlags(ent);
			client->unk_0x281c = 0;
			if (client->pingTime <= level.time) {
				client->ps.eFlags = client->ps.eFlags & 0xffbfffff;
			}
			if (client->noclip == 0) {
				if (client->ufo == 0) {
					if (client->sessionState == 1) {
						int deadType;
						if (ent->tagInfo == 0) {
							deadType = 6;
						} else {
							deadType = 7;
						}
						client->ps.pm_type = deadType;
						ent->r.svFlags = ent->r.svFlags | 1;
						ent->r.svFlags = ent->r.svFlags & 0xfd;
						ent->takedamage = 0;
					} else {
						client->ps.pm_type = (uint32)(ent->tagInfo != 0);
					}
				} else {
					client->ps.pm_type = 3;
				}
			} else {
				client->ps.pm_type = 2;
			}
			client->ps.gravity = trunc_to_int(g_087a2414->current.value);
			client->ps.speed = dvar_g_speed->current.integer;
			client->currentAimSpreadScale = (float)client->ps.aimSpreadScale / 255.0f;
			G_UpdateLookAtTrigger(ent);
			G_UpdateUseTarget(ent);
			Client_ApplyRecoilKick(ent);
			if (level.time - client->lastCmdTime < 0x3e9) {
				ent->s.eFlags = ent->s.eFlags & 0xffffff7f;
			} else {
				ent->s.eFlags = ent->s.eFlags | 0x80;
			}
			client->ps.stats[0] = ent->health;
			Entity_ClearField84(ent);
			if (g_087a248c->current.enabled == 0) {
				BG_PlayerStateToEntityState(&client->ps, &ent->s, 1, 1);
			} else {
				G_PlayerStateToEntityState(&client->ps, ent, client->ps.commandTime, 1);
			}
			if (0 < ent->health) {
				if (G_PlayerCollisionPush(ent) != 0) {
					ent->r.contents = 0x4000000;
				}
			}
			float viewOrigin[3];
			G_GetPlayerViewOrigin(ent, viewOrigin);
			client->ps.iCompassFriendInfo = (int)G_FindTargetPlayerEncoded(ent, viewOrigin, client->useButtonDone);
			if (client->ps.iCompassFriendInfo == 0) {
				client->useButtonDone = 0x3ff;
			} else {
				client->useButtonDone = client->ps.iCompassFriendInfo & 0x3f;
				if ((g_entities[client->useButtonDone].s.eFlags & 0x400000) == 0) {
					client->ps.eFlags = client->ps.eFlags & 0xff7fffff;
				} else {
					client->ps.eFlags = client->ps.eFlags | 0x800000;
				}
			}
			if (ent->s.eType == 1) {
				byte eventType;
				if (ent->health < 1) {
					eventType = 0xb;
				} else {
					eventType = 9;
				}
				ent->handler = eventType;
				int infoIndex = ent->s.clientNum;
				clientInfo_t *info = &g_clientInfos[infoIndex];
				info->field_3e0 = ent->s.angles2[1];
				info->field_3e4 = COD2_INT_BITS(ent->s.leanf);
				VectorCopy_080f7c82(client->ps.viewangles, info->viewangles);
				if (attachmentsChanged != 0) {
					Ent_UnlinkEntity(ent);
				}
				struct DObj *handle = Com_GetServerDObj(ent->s.number);
				PlayerAnim_UpdateTree((int)handle, &ent->s, info, ent->attachIgnoreCollision);
				PlayerView_Update((unsigned int)handle, &ent->s, info);
				if ((client->ps.pm_flags & 0x800000) != 0 && (client->ps.eFlags & 0x300) != 0) {
					Turret_Think(&level.gentities[client->ps.viewlocked_entNum]);
				}
				if (g_087a24b4->current.enabled != 0) {
					if (SV_DObjExists(ent) != 0) {
						Ent_RunKindCallback(ent);
						ScrEntity_Nop();
					}
				}
				ent->client->buttonsSinceLastFrame = 0;
			} else {
				ent->client->buttonsSinceLastFrame = 0;
			}
		} else {
			float spawnOrigin[3];
			float spawnAngles[3];
			VectorCopy_080f7c82(client->ps.origin, spawnOrigin);
			VectorSet_080f7c5e(spawnAngles, 0, client->ps.viewangles[1], 0);
			ClientSpawn(ent, spawnOrigin, spawnAngles);
			ent->client->buttonsSinceLastFrame = 0;
		}
	} else {
		ent->client->buttonsSinceLastFrame = 0;
	}
}

// 0x080f7b38  size=91  FUN_080f7b38  status=translated  naming=unknown
// Per-event hook invoked for each pending player-state event; only event ids 0x9e..0xa0 and 0xaf do work.
void G_HandleClientEvent(int entityNum, int event)
{
	float scratch[6];
	if (0x9d < event && (event < 0xa1 || event == 0xaf)) {
		BG_WeaponRandomFireKick(&g_entities[entityNum].client->ps, g_entities[entityNum].client->viewVec288c, scratch);
	}
}

// 0x080f7b94  size=44  FUN_080f7b94  status=translated  naming=inferred
// Vec2Subtract: out.xy = a.xy - b.xy.
void Vec2Subtract(const float *a, const float *b, float *out)
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
}

// 0x080f7bc0  size=37  FUN_080f7bc0  status=translated  naming=inferred
// Vec2Scale: out.xy = scale * in.xy.
void Vec2Scale(const float *in, float scale, float *out)
{
	out[0] = scale * in[0];
	out[1] = scale * in[1];
}

// 0x080f7be6  size=33  FUN_080f7be6  status=translated  naming=inferred
// Vec2LengthSq: x*x + y*y (x87 extended result).
double Vec2LengthSq(const float *v)
{
	return (double)v[1] * (double)v[1] + (double)v[0] * (double)v[0];
}

// 0x080f7c08  size=44  FUN_080f7c08  status=translated  naming=inferred
// Vec2Length: sqrt(x*x + y*y) via Sqrt_float.
float Vec2Length(const float *v)
{
	return Sqrt_float(v[1] * v[1] + v[0] * v[0]);
}

// 0x080f7c34  size=41  FUN_080f7c34  status=translated  naming=inferred
// VectorClear_080f7c34.
void VectorClear_080f7c34(float *v)
{
	v[0] = 0;
	v[1] = 0;
	v[2] = 0;
}

// 0x080f7c5e  size=35  FUN_080f7c5e  status=translated  naming=inferred
// VectorSet_080f7c5e.
void VectorSet_080f7c5e(float *v, float x, float y, float z)
{
	/* the binary stores the three argument words with integer movs (no x87 load/store, so signalling NaN
	 * payloads are preserved) */
	((uint32 *)(void *)v)[0] = *(volatile const uint32 *)(volatile const void *)&x; /* volatile: keep integer movs, no fld/fstp */
	((uint32 *)(void *)v)[1] = *(volatile const uint32 *)(volatile const void *)&y;
	((uint32 *)(void *)v)[2] = *(volatile const uint32 *)(volatile const void *)&z;
}

// 0x080f7c82  size=47  FUN_080f7c82  status=translated  naming=inferred
// VectorCopy_080f7c82.
void VectorCopy_080f7c82(const float *src, float *dst)
{
	/* integer word copy as in the binary (preserves signalling NaN payloads) */
	((uint32 *)(void *)dst)[0] = ((const uint32 *)(const void *)src)[0];
	((uint32 *)(void *)dst)[1] = ((const uint32 *)(const void *)src)[1];
	((uint32 *)(void *)dst)[2] = ((const uint32 *)(const void *)src)[2];
}

// 0x080f7cb2  size=68  FUN_080f7cb2  status=translated  naming=inferred
// VectorAdd_080f7cb2.
void VectorAdd_080f7cb2(const float *a, const float *b, float *out)
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

// 0x080f7cf6  size=68  FUN_080f7cf6  status=translated  naming=inferred
// VectorSubtract_080f7cf6.
void VectorSubtract_080f7cf6(const float *a, const float *b, float *out)
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

// 0x080f7d3a  size=51  FUN_080f7d3a  status=translated  naming=inferred
// DotProduct (x87 extended result).
double DotProduct_080f7d3a(const float *a, const float *b)
{
	return (double)a[2] * (double)b[2] + (double)a[1] * (double)b[1] + (double)a[0] * (double)b[0];
}

// 0x080f7d6e  size=31  FUN_080f7d6e  status=translated  naming=inferred
// Sqrt wrapper: float(sqrt((double)x)).
float Sqrt_float(float x)
{
	double r = sqrt((double)x);
	return (float)r;
}

// 0x080f84be  size=85  FUN_080f84be  status=translated  naming=inferred
// Registers every entry of the player script field table (g_playerFields) with the script system,
// id = 0xc000 | table index.
void Scr_RegisterPlayerFields(void)
{
	for (const playerFieldDef_t *def = g_playerFields; def->name != 0; def = def + 1) {
		int index = (int)(def - g_playerFields);
		Scr_AddClassField(0, def->name, (int)((index | 0xffffc000U) & 0xffff));
	}
}

// 0x080f8514  size=98  FUN_080f8514  status=translated  naming=inferred
// Script field setter dispatch: uses the entry's custom handler (field +0xc) or the generic setter
// Scr_SetEntityFieldGeneric(obj, type, offset) when none is set.
void Scr_SetPlayerField(struct gclient_s *client, int fieldIndex)
{
    int obj = (int)client;
	if (g_playerFields[fieldIndex].setter == 0) {
		Scr_SetEntityFieldGeneric(client, g_playerFields[fieldIndex].type, g_playerFields[fieldIndex].offset);
	} else {
		g_playerFields[fieldIndex].setter(obj, &g_playerFields[fieldIndex]);
	}
}

// 0x080f8576  size=98  FUN_080f8576  status=translated  naming=inferred
// Script field getter dispatch: custom handler (field +0x10) or generic Scr_AddEntityFieldValue(obj, type, offset).
void Scr_GetPlayerField(struct gclient_s *client, int fieldIndex)
{
    int obj = (int)client;
	if (g_playerFields[fieldIndex].getter == 0) {
		Scr_AddEntityFieldValue(client, g_playerFields[fieldIndex].type, g_playerFields[fieldIndex].offset);
	} else {
		g_playerFields[fieldIndex].getter(obj, &g_playerFields[fieldIndex]);
	}
}

// 0x080f85d8  size=829  FUN_080f85d8  status=translated  naming=inferred
// SetClientViewAngle counterpart: updates delta_angles so the client view matches `angle`, limiting the
// per-call yaw/pitch correction relative to the stored view angles when pm_flags bit 1 is set and the
// player is not on a mounted entity; then stores the angles in currentAngles and ps->viewangles.
void SetClientViewAngle(gentity_t *ent, const float *angle)
{
	float a[3];
	float diff;

	VectorCopy_080f9b06(angle, a);
	if ((ent->client->ps.pm_flags & 1) != 0 && (ent->client->ps.eFlags & 0x300) == 0) {
		diff = AngleNormalize180(AngleDelta(ent->client->ps.proneDirection, a[1]));
		if (45.0 < diff || diff < -45.0) {
			if (45.0 < diff) {
				diff = diff - 45.0;
			} else {
				diff = diff + 45.0;
			}
			ent->client->ps.delta_angles[1] = (int)((trunc_to_int(diff * 182.04445f) & 0xffffU) + ent->client->ps.delta_angles[1]);
			if (0.0 < diff) {
				a[1] = AngleMod((float)(ent->client->ps.proneDirection - 45.0));
			} else {
				a[1] = AngleMod((float)(ent->client->ps.proneDirection + 45.0));
			}
		}
		diff = AngleNormalize180(AngleDelta(ent->client->ps.proneTorsoPitch, a[0]));
		if (45.0 < diff || diff < -15.0) {
			if (45.0 < diff) {
				diff = diff - 45.0;
			} else {
				diff = diff + 15.0;
			}
			ent->client->ps.delta_angles[0] = (int)((trunc_to_int(diff * 182.04445f) & 0xffffU) + ent->client->ps.delta_angles[0]);
			if (0.0 < diff) {
				a[0] = AngleNormalize180((float)(ent->client->ps.proneTorsoPitch - 45.0));
			} else {
				a[0] = AngleNormalize180((float)(ent->client->ps.proneTorsoPitch + 15.0));
			}
		}
	}
	for (int i = 0; i < 3; i = i + 1) {
		ent->client->ps.delta_angles[i] = (int)((trunc_to_int(a[i] * 182.04445f) & 0xffffU) - (unsigned int)ent->client->cmd.angles[i]);
	}
	VectorCopy_080f9b06(a, ent->r.currentAngles);
	VectorCopy_080f9b06(ent->r.currentAngles, ent->client->ps.viewangles);
}

// 0x080f8916  size=482  FUN_080f8916  status=translated  naming=observed
// G_GetPlayerViewOrigin (string "G_GetPlayerViewOrigin: Couldn't find [tag_player] on turret"): eye position
// from origin + view height + bob offsets, or the turret's tag_player when on a mounted entity.
void G_GetPlayerViewOrigin(gentity_t *ent, float *out)
{
	gclient_t *client = ent->client;
	if ((client->ps.eFlags & 0x300) == 0) {
		VectorCopy_080f9b06(client->ps.origin, out);
		out[2] = out[2] + client->ps.viewHeightCurrent;
		float bobA = BG_BobCyclePhase(&client->ps);
		float bobB = PM_GetMoveSpeed(&client->ps, level.time);
		float zOffset = BG_BobSinePair(&client->ps, bobA, bobB, dvar_bg_bobMax->current.value);
		out[2] = out[2] + zOffset;
		float sideOffset = BG_BobSine(&client->ps, bobA, bobB, dvar_bg_bobMax->current.value);
		float right[3];
		G_GetPlayerViewAxis(ent, 0, right, 0);
		VectorMA_080f9b36(out, sideOffset, right, out);
		BG_ApplyLeanRotationOffset(out, client->ps.viewangles[1], client->ps.leanf, 16.0f, 20.0f);
		if (out[2] < client->ps.origin[2] + 8.0) {
			out[2] = client->ps.origin[2] + 8.0f;
		}
	} else {
		gentity_t *turret = &g_entities[client->ps.viewlocked_entNum];
		if (Ent_SetNodeOrigin(turret, (int)scrStr_tag_player, out) == 0) {
			Com_Error(1, "\x15" "G_GetPlayerViewOrigin: Couldn't find [tag_player] on turret\n");
		}
	}
}

// 0x080f8af8  size=51  FUN_080f8af8  status=translated  naming=inferred
// AngleVectors on the player's view angles (ps->viewangles): fills any non-null of forward/right/up.
void G_GetPlayerViewAxis(gentity_t *ent, float *forward, float *right, float *up)
{
	Angles_ToVectors(ent->client->ps.viewangles, forward, right, up);
}

// 0x080f8b2c  size=305  FUN_080f8b2c  status=translated  naming=observed
// ClientCleanName: strips leading spaces, collapses runs of more than three spaces, keeps ^<colour> pairs
// whose second char ColorIndex accepts, drops other ^x pairs, truncates to outSize, and falls back to the
// string "UnnamedPlayer" when empty.
void ClientCleanName(const char *in, char *out, int outSize)
{
	char *outStart = out;
	int written = 0;
	int len = 0;
	int spaces = 0;
	const char *p = in;
	*out = 0;
	while (true) {
		char c = *p;
		p = p + 1;
		if (c == 0) {
			break;
		}
		if (*outStart == 0 && c == ' ') {
			continue;
		}
		if (c != '^') {
			if (c == ' ') {
				spaces = spaces + 1;
				if (3 < spaces) {
					continue;
				}
			} else {
				spaces = 0;
			}
			if (outSize + -2 < len) {
				break;
			}
			*out = c;
			out = out + 1;
			written = written + 1;
			len = len + 1;
			continue;
		}
		if (*p == 0) {
			break;
		}
		if (ColorIndex(*p) != 0) {
			if (outSize + -3 < len) {
				break;
			}
			*out = '^';
			out[1] = *p;
			out = out + 2;
			len = len + 2;
		}
		p = p + 1;
	}
	*out = 0;
	if (*outStart == 0 || written == 0) {
		Q_strncpyz(outStart, "UnnamedPlayer", outSize + -1);
	}
}

// 0x080f8c5e  size=540  FUN_080f8c5e  status=translated  naming=inferred
// ClientUserinfoChanged counterpart: reads the userinfo (falling back to "\\name\\badinfo" when invalid),
// refreshes isBot/predictItems and the cleaned name, and mirrors name/team into the client-info block.
void ClientUserinfoChanged(int clientNum)
{
	char userinfo[1036];
	char oldName[1032];
	gclient_t *client = g_entities[clientNum].client;
	SV_GetUserinfo(clientNum, userinfo, 0x400);
	if (Info_Validate(userinfo) == 0) {
		strcpy(userinfo, "\\name\\badinfo");
	}
	client->localClient = SV_IsClientAddressLocal(clientNum);
	char *value = Info_ValueForKey(userinfo, "cg_predictItems");
	if (atoi(value) == 0) {
		client->predictItemPickup = 0;
	} else {
		client->predictItemPickup = 1;
	}
	if (client->connected == 2 && level.manualNameChange != 0) {
		value = Info_ValueForKey(userinfo, "name");
		ClientCleanName(value, client->newnetname, 0x20);
	} else {
		Q_strncpyz(oldName, client->cs.name, 0x400);
		value = Info_ValueForKey(userinfo, "name");
		ClientCleanName(value, client->cs.name, 0x20);
		Q_strncpyz(client->newnetname, client->cs.name, 0x20);
	}
	clientInfo_t *info = &g_clientInfos[clientNum];
	info->clientNum = clientNum;
	Q_strncpyz(info->name, client->cs.name, 0x20);
	info->team = client->cs.team;
}

// 0x080f8e7a  size=563  FUN_080f8e7a  status=translated  naming=observed
// ClientConnect (returns the string "GAME_INVALIDPASSWORD" on password mismatch, else NULL): resets the
// client slot and info block, marks it connecting/spectator team 3, and checks the server password.
const char *ClientConnect(int clientNum, uint16 pers)
{
	char userinfo[1028];
	gentity_t *ent = &g_entities[clientNum];
	gclient_t *client = (gclient_t *)(clientNum * 0x28a4 + (int)level.clients);
	memset(client, 0, 0x28a4);
	clientInfo_t *info = &g_clientInfos[clientNum];
	int savedField = info->field_4a4;
	memset(info, 0, 0x4b8);
	info->field_4a4 = savedField;
	info->field_00 = 1;
	info->field_04 = 1;
	client->connected = 1;
	client->pers = pers;
	client->cs.team = 3;
	client->sessionState = 2;
	client->lastFollowedClient = -1;
	client->spectatorClient = -1;
	G_InitEntityRecord(ent);
	*((byte *)&g_entities[0] + clientNum * 0x230 + 0x166) = 0;
	g_entities[clientNum].client = client;
	client->useHoldEntity = 0x3ff;
	client->cs.clientNum = clientNum;
	client->ps.clientNum = clientNum;
	ClientUserinfoChanged(clientNum);
	SV_GetUserinfo(clientNum, userinfo, 0x400);
	if (client->localClient == 0) {
		char *password = Info_ValueForKey(userinfo, "password");
		if (*g_087a2404->current.string != 0 && Q_stricmp(g_087a2404->current.string, "none") != 0 &&
		    strcmp(g_087a2404->current.string, password) != 0) {
			G_FreeEntity(ent);
			return "GAME_INVALIDPASSWORD";
		}
	}
	G_CallPlayerConnect(ent);
	G_CalculateRanks();
	return 0;
}

// 0x080f90ae  size=95  FUN_080f90ae  status=translated  naming=inferred
// ClientBegin counterpart: marks the client connected (2) with pm_type 4, then notifies the script layer.
void ClientBegin(int clientNum)
{
	gclient_t *client = (gclient_t *)(clientNum * 0x28a4 + (int)level.clients);
	client->connected = 2;
	client->ps.pm_type = 4;
	G_CalculateRanks();
	Scr_NotifyEntity(&g_entities[clientNum], scrStr_begin, 0);
}

// 0x080f910e  size=959  FUN_080f910e  status=translated  naming=inferred
// ClientSpawn counterpart: resets the entity and client for a (re)spawn at origin/angles, preserving the
// session block (0x26a8..0x27a8) and bumping spawnCount, then runs a think and entity-state update.
void ClientSpawn(gentity_t *ent, const float *origin, const float *angles)
{
	int savedSession[0x40];
	int entIndex = (int)(ent - g_entities);
	gclient_t *client = ent->client;
	if ((client->ps.pm_flags & 0x800000U) != 0 && (client->ps.eFlags & 0x300U) != 0) {
		Turret_ReleaseUser(&level.gentities[client->ps.viewlocked_entNum]);
	}
	Ent_UnlinkFromParent(ent);
	if (ent->r.linked != 0) {
		SV_UnlinkEntity(ent);
	}
	ent->s.groundEntityNum = 0x3ff;
	ScrStr_AssignRef(&ent->classname, scrStr_player);
	ent->clipmask = 0x2810011;
	ent->r.svFlags = ent->r.svFlags | 1;
	ent->takedamage = 0;
	Entity_SetContentsForClientFlags(ent);
	ent->handler = 10;
	ent->flags = 0x1000;
	VectorCopy_080f9b06(g_spawnTraceMins, ent->r.mins);
	VectorCopy_080f9b06(g_spawnTraceMaxs, ent->r.maxs);
	uint32 keptFlags = client->ps.eFlags & 0x100002;
	memcpy(savedSession, &client->sessionState, 0x100);
	int spawnCount = client->ps.stats[5];
	memset(client, 0, 0x28a4);
	memcpy(&client->sessionState, savedSession, 0x100);
	client->lastFollowedClient = -1;
	client->useHoldEntity = 0x3ff;
	client->ps.stats[5] = spawnCount + 1;
	client->ps.stats[2] = client->maxHealth;
	client->ps.eFlags = keptFlags;
	client->cs.clientNum = entIndex;
	client->ps.clientNum = entIndex;
	client->ps.viewlocked_entNum = 0x3ff;
	SV_GetClientUsercmd((int)(client - level.clients), &client->cmd);
	client->ps.eFlags = client->ps.eFlags ^ 2;
	VectorCopy_080f9b06(ent->r.mins, client->ps.mins);
	VectorCopy_080f9b06(ent->r.maxs, client->ps.maxs);
	client->ps.viewHeightTarget = 0x3c;
	client->ps.viewHeightCurrent = 60.0f;
	client->ps.viewHeightLerpTime = 0;
	client->ps.viewHeightLerpPosAdj = 0;
	G_SetPositionTrajectory(ent, origin);
	VectorCopy_080f9b06(origin, client->ps.origin);
	client->ps.pm_flags = client->ps.pm_flags | 0x1000;
	SetClientViewAngle(ent, angles);
	client->inactivityTime = g_087a2428->current.integer * 1000 + level.time;
	client->buttons = client->cmd.buttons;
	level.clientIsSpawning = 1;
	client->nextUseTime = level.time;
	client->cmd.serverTime = level.time;
	client->ps.commandTime = level.time + -100;
	ClientEndFrame(ent);
	ClientThink_real(ent, &client->cmd);
	level.clientIsSpawning = 0;
	BG_PlayerStateToEntityState(&client->ps, &ent->s, 1, 1);
}

// 0x080f94ce  size=347  FUN_080f94ce  status=translated  naming=observed
// ClientDisconnect (script notify "disconnect"): notifies scripts, stops spectators following this
// client, runs the entity teardown calls and clears connected plus the session tail 0x2748..0x27a4.
void ClientDisconnect(int clientNum)
{
	gclient_t *client = (gclient_t *)(clientNum * 0x28a4 + (int)level.clients);
	gentity_t *ent = &g_entities[clientNum];
	if (Scr_IsInitialized(1) != 0) {
		Scr_AddString("disconnect");
		Scr_AddString("-1");
		Scr_NotifyEntity(ent, g_scrCallbackMenuResponse, 2);
	}
	for (int i = 0; i < level.maxclients; i = i + 1) {
		if (level.clients[i].connected != 0 && level.clients[i].sessionState == 2 &&
		    level.clients[i].lastFollowedClient == clientNum) {
			StopFollowing(&g_entities[i]);
		}
	}
	HudElem_FreeAllForOwner(ent);
	if (Scr_IsInitialized(1) != 0) {
		G_CallPlayerDisconnect(ent);
	}
	G_FreeEntity(ent);
	client->connected = 0;
	memset((char *)client + 0x2748, 0, 0x5c);
	G_CalculateRanks();
}

// 0x080f962a  size=843  FUN_080f962a  status=translated  naming=unknown
// Searches (starting after startIndex, wrapping over 64 entities) for the first living same-team client other
// than ent that passes the SV_EntityVisibleToClient test against `origin`, and returns it packed:
// yaw*0.7111 << 24 | y-bucket << 15 | x-bucket << 6 | entity number (6 bits). 0 if none or team is 0/3.
unsigned int G_FindTargetPlayerEncoded(gentity_t *ent, const float *origin, int startIndex)
{
	int team = ent->client->cs.team;
	if (team != 0 && team != 3) {
		int start;
		if (startIndex == 0x3ff) {
			start = 0;
		} else {
			start = startIndex + 1;
		}
		for (int step = 0; step < 0x40; step = step + 1) {
			int idx = start + step;
			int adj = idx;
			if (idx < 0) {
				adj = idx + 0x3f;
			}
			idx = idx + (adj >> 6) * -0x40;
			gentity_t *cand = &g_entities[idx];
			if (cand->r.inuse != 0 && cand->client != 0 && cand->client->sessionState == 0 &&
			    cand->client->cs.team == team && ent != cand &&
			    SV_EntityVisibleToClient((float *)origin, cand->s.number) == 0) {
				unsigned int number = (unsigned int)cand->s.number;
				float diff[3];
				Vec2Subtract_080f9ada(cand->r.currentOrigin, origin, diff);
				int dx = trunc_to_int(diff[0] + 0.5);
				int dy = trunc_to_int(diff[1] + 0.5);
				float scaleX = 1.0f;
				float scaleY = 1.0f;
				if (dx < 0x401) {
					if (dx < -0x3fe) {
						scaleX = -1022.0f / (float)dx;
					}
				} else {
					scaleX = 1024.0f / (float)dx;
				}
				if (dy < 0x401) {
					if (dy < -0x3fe) {
						scaleY = -1022.0f / (float)dy;
					}
				} else {
					scaleY = 1024.0f / (float)dy;
				}
				if (scaleX < 1.0f || scaleY < 1.0f) {
					if (scaleX < scaleY) {
						dy = trunc_to_int((float)dy * scaleX);
					} else if (scaleY < scaleX) {
						dx = trunc_to_int((float)dx * scaleY);
					}
				}
				if (dx < 0x401) {
					if (dx < -0x3fe) {
						dx = -0x3fe;
					}
				} else {
					dx = 0x400;
				}
				if (dy < 0x401) {
					if (dy < -0x3fe) {
						dy = -0x3fe;
					}
				} else {
					dy = 0x400;
				}
				int bucketX = dx + 2;
				if (bucketX < 0) {
					bucketX = dx + 5;
				}
				int bucketY = dy + 2;
				if (bucketY < 0) {
					bucketY = dy + 5;
				}
				return (unsigned int)trunc_to_int(cand->r.currentAngles[1] * 0.7111111f) << 0x18 |
				       (((unsigned int)(bucketY >> 2) + 0xffU) & 0x1ff) << 0xf |
				       (((unsigned int)(bucketX >> 2) + 0xffU) & 0x1ff) << 6 | number & 0x3f;
			}
		}
	}
	return 0;
}

// 0x080f9976  size=355  FUN_080f9976  status=translated  naming=inferred
// Say-style fan-out (counterpart of G_Say's recipient loop): records the speaker's lastSayTime and sends
// the voice packet `pkt` via SV_QueueVoicePacket to every eligible in-use client (team/spectator/dvar rules).
void G_SayToEligibleClients(gentity_t *ent, voicepacket_t *pkt)
{
	ent->client->lastVoiceTime = level.time;
	for (int i = 0; i < 0x40; i = i + 1) {
		gentity_t *other = &g_entities[i];
		if (other->r.inuse != 0 && other->client != 0 &&
		    (other->client->sessionState == 3 ||
		     (g_087a2460->current.enabled != 0 || OnSameTeam(ent, other) != 0 || ent->client->cs.team == 0)) &&
		    (other->client->sessionState == ent->client->sessionState ||
		     ((other->client->sessionState == 1 || ent->client->sessionState == 1) && g_087a2464->current.enabled != 0)) &&
		    (ent != other || g_087a245c->current.enabled != 0) &&
		    SV_GetClientMuteListEntry(i, ent->s.number) == 0 && SV_GetClientSendVoice(i) != 0) {
			SV_QueueVoicePacket((byte)ent->s.number, i, pkt);
		}
	}
}

// 0x080f9ada  size=44  FUN_080f9ada  status=translated  naming=inferred
// Vec2Subtract (second copy).
void Vec2Subtract_080f9ada(const float *a, const float *b, float *out)
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
}

// 0x080f9b06  size=47  FUN_080f9b06  status=translated  naming=inferred
// VectorCopy_080f7c82 (second copy).
void VectorCopy_080f9b06(const float *src, float *dst)
{
	/* integer word copy as in the binary (preserves signalling NaN payloads) */
	((uint32 *)(void *)dst)[0] = ((const uint32 *)(const void *)src)[0];
	((uint32 *)(void *)dst)[1] = ((const uint32 *)(const void *)src)[1];
	((uint32 *)(void *)dst)[2] = ((const uint32 *)(const void *)src)[2];
}

// 0x080f9b36  size=83  FUN_080f9b36  status=translated  naming=inferred
// VectorMA: out = a + scale * b.
void VectorMA_080f9b36(const float *a, float scale, const float *b, float *out)
{
	out[0] = a[0] + scale * b[0];
	out[1] = a[1] + scale * b[1];
	out[2] = a[2] + scale * b[2];
}

// 0x080f9f4e  size=27  FUN_080f9f4e  status=translated  naming=unknown
// True when the entity's client has sessionState 0 (playing).
bool G_ClientIsPlaying(gentity_t *ent)
{
	return ent->client->sessionState == 0;
}

// 0x080fcc48  size=45  FUN_080fcc48  status=translated  naming=unknown
// Clears eFlags bit 0x80000, sets contents to 0x4000000 and svFlags to 0.
void G_SetEntityBodyContents(gentity_t *ent)
{
	ent->s.eFlags = ent->s.eFlags & 0xfff7ffff;
	ent->r.contents = 0x4000000;
	ent->r.svFlags = 0;
}

// 0x080fddac  size=131  FUN_080fddac  status=translated  naming=inferred
// Looks up a player script method by name in g_playerMethods (59 entries), rewrites *name to the table's
// string and returns the handler, or NULL when not found.
void *Scr_FindPlayerMethod(const char **name)
{
	const char *wanted = *name;
	unsigned int i = 0;
	while (true) {
		if (0x3a < i) {
			return 0;
		}
		if (strcmp(wanted, g_playerMethods[i].name) == 0) {
			break;
		}
		i = i + 1;
	}
	*name = g_playerMethods[i].name;
	return g_playerMethods[i].handler;
}

// 0x080fde30  size=41  FUN_080fde30  status=translated  naming=inferred
// VectorClear_080f7c34 (third copy).
void VectorClear_080fde30(float *v)
{
	v[0] = 0;
	v[1] = 0;
	v[2] = 0;
}

// 0x080fde5a  size=47  FUN_080fde5a  status=translated  naming=inferred
// VectorCopy_080f7c82 (third copy).
void VectorCopy_080fde5a(const float *src, float *dst)
{
	/* integer word copy as in the binary (preserves signalling NaN payloads) */
	((uint32 *)(void *)dst)[0] = ((const uint32 *)(const void *)src)[0];
	((uint32 *)(void *)dst)[1] = ((const uint32 *)(const void *)src)[1];
	((uint32 *)(void *)dst)[2] = ((const uint32 *)(const void *)src)[2];
}

// 0x080fde8a  size=68  FUN_080fde8a  status=translated  naming=inferred
// VectorAdd_080f7cb2 (second copy).
void VectorAdd_080fde8a(const float *a, const float *b, float *out)
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

// 0x080fdece  size=56  FUN_080fdece  status=translated  naming=inferred
// VectorScale: out = scale * in.
void VectorScale_080fdece(const float *in, float scale, float *out)
{
	out[0] = scale * in[0];
	out[1] = scale * in[1];
	out[2] = scale * in[2];
}

// 0x080fdf08  size=485  FUN_080fdf08  status=translated  naming=inferred
// ScoreboardMessage counterpart: builds " <client> <score> <ping> <deaths> <field_26b0>" for up to 64
// sorted clients (stopping before the 0x400-byte limit) and sends "b <count> <level200> <level204><list>".
void ScoreboardMessage(gentity_t *ent)
{
	char list[1408];
	char entry[1032];
	int len = 0;
	int i;
	int count = level.numConnectedClients;

	list[0] = 0;
	if (0x40 < count) {
		count = 0x40;
	}
	for (i = 0; i < count; i = i + 1) {
		int cn = level.sortedClients[i];
		gclient_t *cl = &level.clients[cn];
		int ping;
		if (cl->connected == 1) {
			ping = -1;
		} else {
			ping = SV_GetClientPing(cn);
		}
		Com_sprintf(entry, 0x400, " %i %i %i %i %i", level.sortedClients[i], cl->score, ping, cl->deaths, cl->killCamEntity);
		size_t entryLen = strlen(entry);
		if (0x400 < (int)(entryLen + len)) {
			break;
		}
		strcpy(list + len, entry);
		len = len + entryLen;
	}
	char *command = va("%c %i %i %i%s", 0x62, i, level.teamScores[0], level.teamScores[1], list);
	SV_SendServerCommandNum((int)(ent - g_entities), 1, command);
}

// 0x080fe0ee  size=19  FUN_080fe0ee  status=translated  naming=inferred
// Cmd_Score_f counterpart: sends the scoreboard to the calling client.
void Cmd_Score_f(gentity_t *ent)
{
	ScoreboardMessage(ent);
}

// 0x080fe102  size=173  FUN_080fe102  status=translated  naming=observed
// CheatsOk (strings "GAME_CHEATSNOTENABLED", "GAME_MUSTBEALIVECOMMAND"): requires dvar dvar_cheats and
// health > 0, else prints the localized error to the client and returns 0.
int CheatsOk(gentity_t *ent)
{
	int result;
	if (dvar_cheats->current.enabled == 0) {
		char *msg = va("%c \"GAME_CHEATSNOTENABLED\"", 0x65);
		SV_SendServerCommandNum((int)(ent - g_entities), 0, msg);
		result = 0;
	} else if (ent->health < 1) {
		char *msg = va("%c \"GAME_MUSTBEALIVECOMMAND\"", 0x65);
		SV_SendServerCommandNum((int)(ent - g_entities), 0, msg);
		result = 0;
	} else {
		result = 1;
	}
	return result;
}

// 0x080fe1b0  size=198  FUN_080fe1b0  status=translated  naming=inferred
// ConcatArgs counterpart: joins command arguments start..argc-1 with single spaces into the static
// buffer g_08628ae0 (stops when the next argument would overflow 0x3ff bytes) and returns it.
char *ConcatArgs(int start)
{
	char arg[1024];
	int len = 0;
	int argc = Cmd_Argc_alt();
	for (int i = start; i < argc; i = i + 1) {
		Cmd_ArgvBuffer_alt(i, arg, 0x400);
		size_t argLen = strlen(arg);
		if (0x3fe < (int)(argLen + len)) {
			break;
		}
		memcpy(g_08628ae0 + len, arg, argLen);
		len = len + argLen;
		if (i != argc + -1) {
			g_08628ae0[len] = 0x20;
			len = len + 1;
		}
	}
	g_08628ae0[len] = 0;
	return g_08628ae0;
}

// 0x080fe276  size=88  FUN_080fe276  status=translated  naming=inferred
// SanitizeString counterpart: lower-cases, skipping 0x1b escapes (with the following byte) and other control
// characters (signed char < ' ').
void SanitizeString(const char *in, char *out)
{
	while (*in != 0) {
		if (*in == 0x1b) {
			in = in + 2;
		} else if (*in < ' ') {
			in = in + 1;
		} else {
			char c = *in;
			in = in + 1;
			*out = (char)tolower(c);
			out = out + 1;
		}
	}
	*out = 0;
}

// 0x080fe2ce  size=494  FUN_080fe2ce  status=translated  naming=observed
// ClientNumberFromString (strings GAME_BADCLIENTSLOT / GAME_CLIENTNOTACTIVE / GAME_USERNOTONSERVER): resolves a
// slot number or sanitized name of a connected client, else sends an error to ent and returns -1.
int ClientNumberFromString(gentity_t *ent, const char *s)
{
	char cleanName[1024];
	char cleanArg[1032];
	int result;
	int i;

	if (*s < '0' || '9' < *s) {
		SanitizeString(s, cleanArg);
		gclient_t *cl = level.clients;
		for (i = 0; i < level.maxclients; i = i + 1) {
			if (cl->connected == 2) {
				SanitizeString(cl->cs.name, cleanName);
				if (strcmp(cleanName, cleanArg) == 0) {
					return i;
				}
			}
			cl = (gclient_t *)((char *)cl + 0x28a4);
		}
		char *msg = va("%c \"GAME_USERNOTONSERVER\x15" "%s\"", 0x65, s);
		SV_SendServerCommandNum((int)(ent - g_entities), 0, msg);
		result = -1;
	} else {
		result = atoi(s);
		i = result;
		if (result < 0 || level.maxclients <= result) {
			char *msg = va("%c \"GAME_BADCLIENTSLOT\x15 %i\"", 0x65, result);
			SV_SendServerCommandNum((int)(ent - g_entities), 0, msg);
			result = -1;
		} else {
			gclient_t *cl = (gclient_t *)(result * 0x28a4 + (int)level.clients);
			if (cl->connected != 2) {
				char *msg = va("%c \"GAME_CLIENTNOTACTIVE\x15" "%i\"", 0x65, result);
				SV_SendServerCommandNum((int)(ent - g_entities), 0, msg);
				result = -1;
			}
		}
	}
	return result;
}

// 0x080fe4bc  size=175  FUN_080fe4bc  status=translated  naming=unknown
// Stores a 7-float configstring (index 12), parses it with sscanf, and when all 7 parse and the 3rd >= 1.0
// sets g_0864e158 = (f1 - f0) + f0 and g_0864e15c = g_0864e158^2 (both default to FLT_MAX).
void G_ParseConfigstring12Bounds(const char *text)
{
	float v0, v1, v2, v3, v4, v5, v6;
	SV_SetConfigstring(0xc, (char *)text);
	g_0864e158 = 3.4028235e+38f;
	g_0864e15c = 3.4028235e+38f;
	int count = sscanf(text, "%f %f %f %f %f %f %f", &v0, &v1, &v2, &v3, &v4, &v5, &v6);
	if (count == 7 && 1.0 <= v2) {
		g_0864e158 = (float)(((double)v1 - (double)v0) + (double)v0);
		g_0864e15c = g_0864e158 * g_0864e158;
	}
}

// 0x080fe588  size=1089  FUN_080fe588  status=translated  naming=observed
// Cmd_Give_f (strings "health", "weapons", "ammo", "allammo"): cheat command giving health, all weapons,
// ammo, or a named item to the caller. "all" runs every branch except allammo.
void Cmd_Give_f(gentity_t *ent)
{
	if (CheatsOk(ent) == 0) {
		return;
	}
	char *argAmount = ConcatArgs(2);
	int amount = atoi(argAmount);
	char *name = ConcatArgs(1);
	if (name == 0 || *name == 0) {
		return;
	}
	bool giveAll = Q_stricmp(name, "all") == 0;
	if (giveAll || Q_stricmpn(name, "health", 6) == 0) {
		if (amount == 0) {
			ent->health = ent->client->ps.stats[2];
		} else {
			ent->health = amount + ent->health;
		}
		if (!giveAll) {
			return;
		}
	}
	if (giveAll || Q_stricmp(name, "weapons") == 0) {
		level.initializing = 1;
		char slotB = ent->client->ps.weaponslots[2];
		BG_TakeWeapon(&ent->client->ps, (int)ent->client->ps.weaponslots[1]);
		BG_TakeWeapon(&ent->client->ps, (int)ent->client->ps.weaponslots[2]);
		for (int w = 1; w <= BG_GetNumWeapons(); w = w + 1) {
			char owned = BG_IsHeldSlotWeapon(w);
			if (owned == 0) {
				G_TouchTriggerEntity(ent->client, w);
			} else if (slotB < w && PlayerState_HasFreeHeldSlot(&ent->client->ps) != 0) {
				G_TouchTriggerEntity(ent->client, w);
			}
		}
		for (int w = 1; PlayerState_HasFreeHeldSlot(&ent->client->ps) != 0 && w <= BG_GetNumWeapons(); w = w + 1) {
			if (BG_IsHeldSlotWeapon(w) != 0) {
				G_TouchTriggerEntity(ent->client, w);
			}
		}
		level.initializing = 0;
		if (!giveAll) {
			level.initializing = 0;
			return;
		}
	}
	if (giveAll || Q_stricmpn(name, "ammo", 4) == 0) {
		if (amount == 0) {
			for (int w = 1; w <= BG_GetNumWeapons(); w = w + 1) {
				Player_GiveWeaponAmmo(ent, w, 0x3e6, 1);
			}
		} else if (ent->client->ps.weapon != 0) {
			Player_GiveWeaponAmmo(ent, ent->client->ps.weapon, amount, 1);
		}
		if (!giveAll) {
			return;
		}
	}
	if (Q_stricmpn(name, "allammo", 7) == 0 && amount != 0) {
		for (int w = 1; w <= BG_GetNumWeapons(); w = w + 1) {
			Player_GiveWeaponAmmo(ent, w, amount, 1);
		}
		if (!giveAll) {
			return;
		}
	}
	if (!giveAll) {
		struct gitem_s *itemIndex = BG_FindItemByName(name);
		if (itemIndex != NULL) {
			level.initializing = 1;
			gentity_t *item = G_Spawn();
			VectorCopy_081012cc(ent->r.currentOrigin, item->r.currentOrigin);
			Item_SetModelName(itemIndex, &item->classname);
			Item_SetupDroppedWeapon(item, itemIndex);
			item->active = 1;
			Item_TouchPickup(item, ent, 1);
			item->active = 0;
			if (item->r.inuse != 0) {
				G_FreeEntity(item);
			}
			level.initializing = 0;
		}
	}
}
