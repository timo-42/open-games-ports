#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_049_types.h"

/*
 * Chunk 049: game-module init/shutdown, entity ordering, frame loop, trace
 * wrappers and the turret entity state machine. Every function below is a
 * behavioural translation of the decompiled body with the disassembly used to
 * resolve float/long-double precision, argument order and stack-buffer layout.
 */

/* ---- forward declarations for chunk-internal functions ---- */
void G_InitGame(int levelTime, uint32 randomSeed, int isRestart, int hasMapLoad);
void G_ShutdownGame(int freeLevel);
void G_FlushPendingTurretUpdates(void);
int G_SortRanks(const void *a, const void *b);
void G_CalculateRanks(void);
void G_ExitLevel(void);
void G_LogPrintf(const char *fmt, ...);
void G_CheckVote(void);
void G_CopyTurretEntries(void);
void G_UpdateClientTurretStates(void);
void G_RunThink(struct gentity_s *ent);
void G_DumpSelectedEntity(void);
void G_StopEntityScripts(struct gentity_s *ent);
void G_RunEntityState(struct gentity_s *ent);
void G_RunFrame(int levelTime);
void G_TraceBox(struct trace_s *results, const float *start, const float *mins, const float *maxs, const float *end,
                int passEntityNum, int contentmask);
int G_TraceIsClearBox(const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum,
                       int contentmask);
void G_TracePointEx(struct trace_s *results, const float *start, const float *end, int passEntityNum, int contentmask, const char *surface);
int G_TraceIsClearPoint(const float *start, const float *end, int passEntityNum, int contentmask);
void G_TraceAccumulateHit(int *hit, float *start, float *end, int passEntityNum, int contentmask);
void G_NoopCallback(const float *origin, const float *rgba, float scale, const char *text);
void G_ReleaseEntityUse(struct gentity_s *ent);
void G_TeleportEntity(struct gentity_s *ent, const float *origin, const float *angles);
void G_ClearTurretSlots(void);
void Turret_ComputeFlashFrame(struct gentity_s *turret, void *viewer, float *frame);
void Turret_UpdateAimState(struct gentity_s *turret, void *viewer);
void Turret_PositionUser(struct gentity_s *user, struct gentity_s *turret);
void Turret_SetInitialAngles(struct gentity_s *turret, struct gentity_s *user);
void Turret_StartReload(struct gentity_s *turret, struct gentity_s *user);
void Turret_UpdateUser(struct gentity_s *turret, struct gentity_s *user);
void Turret_UpdateAmmo(struct gentity_s *turret);
void Turret_ReleaseUser(struct gentity_s *turret);
void Turret_Think(struct gentity_s *turret);
int Turret_ClampAim(struct gentity_s *turret, const float *target, int hasLimit);
void Turret_RestAim(struct gentity_s *turret, int flag);
int Turret_IsAimingAtUser(struct gentity_s *ent, struct gentity_s *other);
void Turret_Reset(struct gentity_s *ent);
int Turret_CanBeUsed(struct gentity_s *ent, struct gentity_s *user);
void G_SpawnTurret(struct gentity_s *ent, const char *weaponName);
void G_SpawnTurretFromWeapon(struct gentity_s *ent);
float Float_Abs_0810ccd8(float v);
float Float_Clamp_0810ccf2(float v, float min, float max);
void Vec2_Add_0810cd30(const float *a, const float *b, float *out);
void Vec3_Clear_0810cd5c(float *v);
void Vec3_Set_0810cd86(float *v, float x, float y, float z);
void Vec3_Copy_0810cdaa(const float *in, float *out);
void Vec3_Add_0810cdda(const float *a, const float *b, float *out);
void Vec3_Sub_0810ce1e(const float *a, const float *b, float *out);
void Vec3_MultiplyAdd(const float *a, float scale, const float *b, float *out);
long double Vec3_Dot_0810ceb6(const float *a, const float *b);
float Float_Select_0810ceea(float a, float b, float c);
void Vec3_Scale_0810cf14(const float *in, float scale, float *out);
void Turret_UpdateTrajectoryAim(struct gentity_s *ent, const float *input, float *out, int isInitial);
int Turret_UpdateFromInput(struct gentity_s *ent, struct turret_input_s *in);
void Turret_ProcessInput(struct gentity_s *ent, struct turret_input_s *in, void *unused, float *out);
void Turret_TraceAimWithFlags(struct trace_s *trace, float *start, float *end, int passEntityNum,
                              int contentmask);
void Turret_TraceAimNoContents(struct trace_s *trace, int index, struct gentity_s *turret, float *end);
float Float_TanDeg(float deg);
void Turret_Advance(struct gentity_s *ent);

/* Reinterpret a 32-bit float slot as its raw bits, and back (turret arcs are stored as bits in weaponinfo). */
static int FloatBits(float f)
{
    union { float f; int i; } u;
    u.f = f;
    return u.i;
}

static float BitsFloat(int i)
{
    union { float f; int i; } u;
    u.i = i;
    return u.f;
}

/* Thin accessor for the entity array base the game uses as level_gentities / g_gentities. */
static struct gentity_s *EntityAt(int index)
{
    return &g_gentities[index];
}

/* ======================================================================== */

// 0x08109096  size=1105  FUN_08109096  status=translated  naming=observed
// strings "------- Game Initialization -------", "InitGame: %s\n", "Jun 23 2006" identify game init
void G_InitGame(int levelTime, uint32 randomSeed, int isRestart, int hasMapLoad)
{
    char levelName[0x400];      /* local_81c */
    char infoBuf[0x408];        /* local_41c */
    int i;

    Com_Printf("------- Game Initialization -------\n");
    Com_Printf("gamename: %s\n", "Call of Duty 2");
    Com_Printf("gamedate: %s\n", "Jun 23 2006");
    Swap_Init();
    /* Zeroes the game-global block that follows level_clients (0x3624 bytes) before re-seeding it. */
    memset(&level_clients, 0, 0x3624);
    level_initializing = 1;
    g_0864c56c = levelTime;
    level_startTime = levelTime;
    g_currentThinkEntNum = -1;
    srand(randomSeed);
    Rand_SetSeed(randomSeed);
    G_SetupWeaponDef();
    if (isRestart == 0 || hasMapLoad == 0) {
        G_RegisterDvars();
    }
    G_ProcessIPBans();
    g_087035ac = (uint32)(size_t)SV_RegisterModel;
    g_087035b0 = (uint32)(size_t)G_ServerDObjCreate;
    g_087035b4 = (uint32)(size_t)Com_ServerDObjFree;
    g_087035b8 = (uint32)(size_t)Hunk_AllocLowCallback;
    g_087035a8 = 1;
    if (*g_087a246c->current.string == '\0') {
        Com_Printf("Not logging to disk.\n");
    } else {
        if (g_087a2470->current.enabled == 0) {
            FSH_FOpenFile(g_087a246c->current.string, &level_logFile, 2);
        } else {
            FSH_FOpenFile(g_087a246c->current.string, &level_logFile, 3);
        }
        if (level_logFile == 0) {
            Com_Printf("WARNING: Couldn't open logfile: %s\n", g_087a246c->current.string);
        } else {
            SV_GetServerinfo(levelName, 0x400);
            G_LogPrintf("------------------------------------------------------------\n");
            G_LogPrintf("InitGame: %s\n", levelName);
        }
    }
    for (i = 0; i < 1; i++) {
        g_scrFileHandles[i] = -1;
        g_0864f98c[i] = 0;
        memset(&g_scrParseSaveRecords[i], 0, 0x14);
    }
    Mantle_ValidateAnims(reinterpret_cast<unsigned int>(&Hunk_AllocLowCallback));
    g_gameContextPtr = g_levelBlock;
    if (isRestart == 0) {
        memset(g_levelBlock, 0, 0xb3bc8);
        *(void **)(g_gameContextPtr + 0xb3bc0) = (void *)SND_FindAliasList;
        *(void **)(g_gameContextPtr + 0xb3bc4) = (void *)Ent_AddAnimEventB3;
        GScr_LoadScripts();
        Player_LoadMultiplayerAnims();
        Pool_AllocGameObjects();
    }
    G_InitScriptStrings();
    SV_GetConfigstring(0x16, infoBuf, 0x400);
    Info_SetValueForKey(infoBuf, "winner", "0");
    SV_SetConfigstring(0x16, infoBuf);
    memset(g_gentities, 0, 0x8c000);
    level_gentities = g_gentities;
    level_maxclients = g_087a2408->current.integer;
    memset(g_gclients, 0, 0xa2900);
    level_clients = g_gclients;
    for (i = 0; i < level_maxclients; i++) {
        g_gentities[i].client = level_clients + i;
    }
    level_numEntities = 0x48;
    level_freeEntityHead = 0;
    level_freeEntityTail = 0;
    SV_InitGameEntityLayout((unsigned char *)level_gentities, 0x48, 0x230, (unsigned char *)level_clients, 0x28a4);
    G_ParseHitLocDmgTable();
    G_ClearTurretSlots();
    G_SpawnEntitiesFromString();
    G_ParseConfigstring12Bounds("0");
    G_ClearObjectives();
    Scr_FlushEntityReleaseChain();
    Com_Printf("-----------------------------------\n");
    Scr_InitVariableRoots(); /* original pushes one ignored dword argument */
    Scr_SetLoading(1);
    Scr_InitPersistentArray();
    G_SpawnScriptStructs();
    G_CallGametypeMain();
    GScr_RunLevelMain();
    G_CallStartGameType();
    for (i = 0; i < 8; i++) {
        *(int *)(g_0884e7bc + i * 0x4c8) = -1;
    }
    g_gameContextPtr = NULL;
    level_initializing = 0;
    ItemRegistry_BuildNameList();
    ItemRegistry_BuildBitmask();
}

// 0x081094e8  size=302  FUN_081094e8  status=translated  naming=observed
// string "==== ShutdownGame ====" and "ShutdownGame:" identify the shutdown path
void G_ShutdownGame(int freeLevel)
{
    int i;
    int hasScript;

    Com_Printf("==== ShutdownGame ====\n");
    if (level_logFile != 0) {
        G_LogPrintf("ShutdownGame:\n");
        G_LogPrintf("------------------------------------------------------------\n");
        FS_FCloseFile(level_logFile);
    }
    g_gameContextPtr = NULL;
    Game_FreeAllEntities();
    HudElem_FreeAll();
    hasScript = Scr_IsInitialized(1);
    if (hasScript != 0 && g_mapChangeKeepStateArg == 0) {
        SV_ReallocClientScriptIds();
    }
    Scr_ShutdownVariableRoots(1, g_mapChangeKeepStateArg == 0);
    if (freeLevel != 0) {
        Mantle_ResetTree();
        GScr_ShutdownClasses();
        Scr_ResetAll(1);
        Pool_FreeGameObjects();
        Hunk_SetLowMark(0);
    }
    for (i = 0; i < 1; i++) {
        if (g_0864f98c[i] != 0) {
            Com_Free_A((void *)(size_t)g_0864f98c[i]);
        }
        g_0864f98c[i] = 0;
        if (g_scrFileHandles[i] >= 0) {
            FS_FCloseFile(g_scrFileHandles[i]);
        }
        g_scrFileHandles[i] = -1;
    }
}

// 0x08109616  size=119  FUN_08109616  status=translated  naming=inferred
// clears level_teamScoresDirty after flushing deferred per-entity updates for clients in state 2 with sortState/type 5
void G_FlushPendingTurretUpdates(void)
{
    int i;

    if (level_teamScoresDirty != 0) {
        for (i = 0; i < level_maxclients; i++) {
            struct gclient_s *client = &level_clients[i];
            /* gameplay-fix(gt_sd): original 0x08109616 tests [client+0x4] == 5 (ps.pm_type = PM_INTERMISSION), not commandTime (+0) */
            if (client->connected == 2 && client->ps.pm_type == 5) {
                ScoreboardMessage(&g_gentities[i]);
            }
        }
        level_teamScoresDirty = 0;
    }
}

// 0x0810968e  size=343  FUN_0810968e  status=translated  naming=inferred
// qsort comparator over client indices: sortState, then team 3 (spectator), then score and secondary score
int G_SortRanks(const void *a, const void *b)
{
    const struct gclient_s *ca = &level_clients[*(const int *)a];
    const struct gclient_s *cb = &level_clients[*(const int *)b];

    if (ca->connected == 1) {
        return 1;
    } else if (cb->connected == 1) {
        return -1;
    } else if (ca->cs.team == 3 && cb->cs.team == 3) {
        if (ca < cb) {
            return -1;
        } else if (cb < ca) {
            return 1;
        }
        return 0;
    } else if (ca->cs.team == 3) {
        return 1;
    } else if (cb->cs.team == 3) {
        return -1;
    } else if (cb->score < ca->score) {
        return -1;
    } else if (ca->score < cb->score) {
        return 1;
    } else if (ca->deaths < cb->deaths) {
        return -1;
    } else if (cb->deaths < ca->deaths) {
        return 1;
    }
    return 0;
}

// 0x081097e6  size=210  FUN_081097e6  status=translated  naming=inferred
// fills level_sortedClientIndices with client indices whose sortState is nonzero, counts state-2 non-spectators, then qsorts
void G_CalculateRanks(void)
{
    int i;

    g_0864c598 = 0;
    g_0864ceac = 0;
    for (i = 0; i < level_maxclients; i++) {
        if (level_clients[i].connected != 0) {
            level_sortedClientIndices[g_0864c598] = i;
            g_0864c598 = g_0864c598 + 1;
            if (level_clients[i].cs.team != 3 && level_clients[i].connected == 2) {
                g_0864ceac = g_0864ceac + 1;
            }
        }
    }
    qsort(level_sortedClientIndices, g_0864c598, 4, (int (*)(const void *, const void *))G_SortRanks);
    level_teamScoresDirty = 1;
}

// 0x081098b8  size=218  FUN_081098b8  status=translated  naming=observed
// string "ExitLevel: executed\n" is logged by this function
void G_ExitLevel(void)
{
    int i;

    Cbuf_ExecuteText(2, "map_rotate\n");
    level_teamScoreAxis = 0;
    level_teamScoreAllies = 0;
    for (i = 0; i < g_087a2408->current.integer; i++) {
        if (level_clients[i].connected == 2) {
            level_clients[i].score = 0;
        }
    }
    for (i = 0; i < g_087a2408->current.integer; i++) {
        if (level_clients[i].connected == 2) {
            level_clients[i].connected = 1;
        }
    }
    G_LogPrintf("ExitLevel: executed\n");
}

// 0x08109992  size=330  FUN_08109992  status=translated  naming=inferred
// "%3i:%i%i %s" time-stamped line written to the log handle (G_LogPrintf counterpart)
void G_LogPrintf(const char *fmt, ...)
{
    char message[0x400];        /* local_81c */
    char line[0x40c];           /* local_41c */
    va_list ap;
    int seconds;
    int secTens;
    size_t len;

    if (level_logFile == 0) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(message, 0x400, fmt, ap);
    va_end(ap);
    seconds = (g_0864c56c / 1000) % 60;
    secTens = seconds;
    Com_sprintf(line, 0x400, "%3i:%i%i %s", (g_0864c56c / 1000) / 60, secTens / 10, secTens % 10, message);
    len = strlen(line);
    FS_Write(line, len, level_logFile);
}

// 0x08109adc  size=516  FUN_08109adc  status=translated  naming=observed
// strings "GAME_VOTEPASSED" and "GAME_VOTEFAILED" are emitted here
void G_CheckVote(void)
{
    int tally;
    double ceilVal;

    if (level_pendingVoteExecute != 0 && level_pendingVoteExecute < g_0864c56c) {
        level_pendingVoteExecute = 0;
        Cbuf_ExecuteText(2, va("%s\n", level_voteCommandBuf));
    }
    tally = level_voteYes;
    if (level_voteTime != 0) {
        if (g_0864c56c - level_voteTime < 0) {
            tally = g_0864ceac / 2 + 1;
            if (level_voteYes < tally) {
                if (level_voteNo <= g_0864ceac - tally) {
                    return;
                }
                SV_SendServerCommandNum(-1, 0, va("%c \"GAME_VOTEFAILED\"", 0x65));
            } else {
                SV_SendServerCommandNum(-1, 0, va("%c \"GAME_VOTEPASSED\"", 0x65));
                level_pendingVoteExecute = g_0864c56c + 3000;
            }
        } else {
            ceilVal = ceil((double)((float)(g_0864ceac - (level_voteNo + level_voteYes)) * g_087a24d4->current.value));
            if ((int)(ceilVal + (double)level_voteNo) < tally) {
                SV_SendServerCommandNum(-1, 0, va("%c \"GAME_VOTEPASSED\"", 0x65));
                level_pendingVoteExecute = g_0864c56c + 3000;
            } else {
                SV_SendServerCommandNum(-1, 0, va("%c \"GAME_VOTEFAILED\"", 0x65));
            }
        }
        level_voteTime = 0;
        SV_SetConfigstring(0xf, "");
    }
}

// 0x08109ce0  size=281  FUN_08109ce0  status=translated  naming=unknown
// copies the 16 request entries (team-filtered) into each in-use client's turretEntries table
void G_CopyTurretEntries(void)
{
    int i;
    int k;

    for (i = 0; i < level_maxclients; i++) {
        struct gentity_s *ent = &g_gentities[i];
        if (ent->r.inuse != 0) {
            struct gclient_s *client = ent->client;
            int team = client->cs.team;
            for (k = 0; k < 16; k++) {
                /* level.objectives[k] -> ps.objective[k]: seven dwords (state, origin[3], entNum, teamNum, icon) */
                int *src = (int *)&level_objectives[k];
                int *dst = (int *)&client->ps.objective[k];
                if (src[0] == 0 || (src[5] != 0 && src[5] != team)) {
                    dst[0] = 0;
                } else {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = src[3];
                    dst[4] = src[4];
                    dst[5] = src[5];
                    dst[6] = src[6];
                }
            }
        }
    }
}

// 0x08109dfa  size=101  FUN_08109dfa  status=translated  naming=unknown
// calls HudElem_BuildClientLists(client, entityNum, 3) for every in-use entity
void G_UpdateClientTurretStates(void)
{
    int i;

    for (i = 0; i < level_maxclients; i++) {
        struct gentity_s *ent = &g_gentities[i];
        if (ent->r.inuse != 0) {
            HudElem_BuildClientLists(ent->client, ent->s.number, 3);
        }
    }
}

// 0x08109e60  size=120  FUN_08109e60  status=translated  naming=observed
// string "NULL ent->think" is raised when the dispatch table entry is empty
void G_RunThink(struct gentity_s *ent)
{
    turret_think_fn think;

    if (ent->nextthink > 0 && ent->nextthink <= g_0864c56c) {
        ent->nextthink = 0;
        think = g_entityHandlerTable[ent->handler].think;
        if (think == NULL) {
            Com_Error(1, "\025NULL ent->think");
        }
        (*think)(ent);
    }
}

// 0x08109ed8  size=59  FUN_08109ed8  status=translated  naming=inferred
// debug dump of the entity selected by the g_087a24d8 dvar, labelled "server:"
void G_DumpSelectedEntity(void)
{
    if (g_087a24d8->current.integer > -1) {
        Com_Printf("server:\n");
        SV_DObjDumpAnimTree((int *)&g_gentities[g_087a24d8->current.integer]);
    }
}

// 0x08109f14  size=71  FUN_08109f14  status=translated  naming=unknown
// clears script threads of an entity while it is in use, not flagged 0x2000, and Ent_UpdateTimeScaled reports work
void G_StopEntityScripts(struct gentity_s *ent)
{
    while (ent->r.inuse != 0 && (ent->flags & 0x2000) == 0 && Ent_UpdateTimeScaled(ent, 1) != 0) {
        Scr_RunCurrentThreads();
    }
}

// 0x08109f5c  size=478  FUN_08109f5c  status=translated  naming=unknown
// per-entity frame update dispatched on eType (4,3,2,6,0) and the occupied flag
void G_RunEntityState(struct gentity_s *ent)
{
    if (ent->framenum != level_frameNum) {
        ent->framenum = level_frameNum;
        if (ent->client == NULL) {
            if ((ent->flags & 0x800) == 0) {
                ent->s.eFlags &= 0xffffffdfu;
            } else {
                ent->s.eFlags |= 0x20u;
            }
        }
        if (ent->s.eFlags == 0x10000 && ent->s.time2 < g_0864c56c) {
            G_FreeEntity(ent);
        } else {
            if (300 < g_0864c56c - ent->eventTime) {
                if (ent->freeAfterEvent != 0) {
                    G_FreeEntity(ent);
                    return;
                }
                if (ent->unlinkAfterEvent != 0) {
                    ent->unlinkAfterEvent = 0;
                    SV_UnlinkEntity(ent);
                }
            }
            if (ent->freeAfterEvent == 0) {
                if (ent->s.eType == 4) {
                    G_RunMissile(ent);
                } else if (ent->s.eType == 3) {
                    if (ent->tagInfo == NULL) {
                        Ent_StepMovement(ent);
                    } else {
                        Ent_ResetAttachState(ent);
                        G_RunThink(ent);
                    }
                } else if (ent->s.eType == 2) {
                    G_RunCorpse(ent);
                } else if (ent->physicsObject == 0) {
                    if (ent->s.eType == 6) {
                        G_RunMoverEntity(ent);
                    } else if (ent->client == NULL) {
                        if (ent->s.eType == 0 && ent->tagInfo != NULL) {
                            Ent_ResetAttachState(ent);
                        }
                        G_RunThink(ent);
                    } else {
                        G_ClientRunFrame(ent);
                    }
                } else {
                    Ent_StepMovement(ent);
                }
            }
        }
    }
}

// 0x0810a13a  size=1005  FUN_0810a13a  status=translated  naming=inferred
// per-frame driver: advances g_0864c56c, runs think/update passes until no pending pair changes, then post-frame hooks
void G_RunFrame(int levelTime)
{
    int prevTime;
    char touched[0x400];        /* local_42c */
    char stamp;                 /* local_1d */
    int changed;                /* local_1c */
    int i;
    struct entity_pair_s *pair;
    struct gentity_s *entA;

    prevTime = g_0864c56c;
    level_frameNum = level_frameNum + 1;
    level_previousTime = g_0864c56c;
    g_0864c56c = levelTime;
    level_frameTime = levelTime - prevTime;
    g_0870359c = levelTime;
    g_087035a0 = levelTime;
    g_gameContextPtr = g_levelBlock;
    g_087035a4 = level_frameTime;
    for (i = 0; i < level_numEntities; i++) {
        if (g_gentities[i].r.inuse != 0) {
            SV_DObjBindingSetTime((int *)&g_gentities[i], (float)level_frameTime * 0.001f);
        }
    }
    memset(touched, 0, 0x400);
    stamp = 0;
    Com_Memcpy(g_0864ed68, g_entityPairQueue, g_entityPairQueueCount * 12);
    g_0864f96c = g_entityPairQueueCount;
    g_entityPairQueueCount = 0;
    for (;;) {
        changed = 0;
        stamp = (char)(stamp + 1);
        for (i = 0; i < g_0864f96c; i++) {
            unsigned int idxA;
            pair = &g_0864ed68[i];
            idxA = pair->entityA;
            entA = &g_gentities[idxA];
            if (g_gentities[idxA].spawnSerial == pair->serialA &&
                g_gentities[pair->entityB].spawnSerial == pair->serialB) {
                if (touched[idxA] != stamp) {
                    touched[idxA] = stamp;
                    Scr_AddEntityParam(&g_gentities[pair->entityB]);
                    Scr_NotifyEntity(entA, scr_str_trigger, 1);
                    goto remove_pair;
                }
                changed = 1;
            } else {
remove_pair:
                g_0864f96c = g_0864f96c - 1;
                i = i - 1;
                *pair = g_0864ed68[g_0864f96c];
            }
        }
        Scr_RunCurrentThreads();
        if (changed == 0) {
            break;
        }
    }
    for (i = 0; i < level_numEntities; i++) {
        G_StopEntityScripts(&g_gentities[i]);
    }
    Scr_AdvanceTime();
    for (g_currentThinkEntNum = 0; g_currentThinkEntNum < level_numEntities; g_currentThinkEntNum++) {
        struct gentity_s *ent = &g_gentities[g_currentThinkEntNum];
        if (ent->r.inuse != 0) {
            if (ent->tagInfo != NULL) {
                G_RunEntityState(*(gentity_s **)ent->tagInfo); /* node owner at +0 */
            }
            G_RunEntityState(ent);
        }
    }
    g_currentThinkEntNum = -1;
    G_CopyTurretEntries();
    G_UpdateClientTurretStates();
    for (i = 0; i < level_maxclients; i++) {
        if (g_gentities[i].r.inuse != 0) {
            ClientEndFrame(&g_gentities[i]);
        }
    }
    Client_UpdateAllCrosshairTargets();
    if (dvar_g_oldVoting->current.enabled != 0) {
        G_CheckVote();
    }
    G_FlushPendingTurretUpdates();
    if (g_087a2480->current.enabled != 0) {
        for (i = 0; i < 0x400; i++) {
            const char *name = ScrStr_TextFromHandle(g_gentities[i].classname);
            Com_Printf("%4i: %s\n", i, name);
        }
        Dvar_SetBool(g_087a2480, 0);
    }
    if (g_0864f97c != 0) {
        ItemRegistry_BuildNameList();
    }
    if (g_0864f980 != 0) {
        ItemRegistry_BuildBitmask();
    }
    G_DumpSelectedEntity();
    g_gameContextPtr = NULL;
}

// 0x0810a528  size=85  FUN_0810a528  status=translated  naming=inferred
// seven-argument SV_Trace with zero extra-check parameters (generic box trace)
void G_TraceBox(struct trace_s *results, const float *start, const float *mins, const float *maxs, const float *end,
                int passEntityNum, int contentmask)
{
    SV_Trace(results, (float *)start, (float *)mins, (float *)maxs, (float *)end, passEntityNum, contentmask, 0, 0, 0);
}

// 0x0810a57e  size=78  FUN_0810a57e  status=translated  naming=inferred
// SV_TraceIsClear wrapper: passOwnerNum fixed to 0x3ff (none); result discarded
int G_TraceIsClearBox(const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum,
                       int contentmask)
{
    return SV_TraceIsClear((float *)start, (float *)mins, (float *)maxs, (float *)end, passEntityNum, 0x3ff, contentmask, 0, 0);
}

// 0x0810a5cc  size=86  FUN_0810a5cc  status=translated  naming=unknown
// point SV_Trace with flag=1 and an extra parameter taken from the caller (table pointer)
void G_TracePointEx(struct trace_s *results, const float *start, const float *end, int passEntityNum, int contentmask, const char *surface)
{
    int param9 = (int)surface;
    float zero[3];
    zero[0] = vec3_origin[0];
    zero[1] = vec3_origin[1];
    zero[2] = vec3_origin[2];
    SV_Trace(results, start, zero, zero, end, passEntityNum, contentmask, 1, param9, 1);
}

// 0x0810a622  size=80  FUN_0810a622  status=translated  naming=unknown
// point SV_TraceIsClear wrapper with passOwnerNum 0x3ff and flags 1/1
int G_TraceIsClearPoint(const float *start, const float *end, int passEntityNum, int contentmask)
{
    float zero[3];
    zero[0] = vec3_origin[0];
    zero[1] = vec3_origin[1];
    zero[2] = vec3_origin[2];
    return SV_TraceIsClear((float *)start, zero, zero, (float *)end, passEntityNum, 0x3ff, contentmask, 1, 1);
}

// 0x0810a672  size=71  FUN_0810a672  status=translated  naming=unknown
// point SV_TraceAccumulateHit wrapper with passOwnerNum 0x3ff
void G_TraceAccumulateHit(int *hit, float *start, float *end, int passEntityNum, int contentmask)
{
    float zero[3];
    zero[0] = vec3_origin[0];
    zero[1] = vec3_origin[1];
    zero[2] = vec3_origin[2];
    SV_TraceAccumulateHit(hit, start, zero, zero, end, passEntityNum, 0x3ff, contentmask);
}

// 0x0810a6ba  size=5  FUN_0810a6ba  status=translated  naming=unknown
// empty callback (returns immediately); used as a no-op dispatch slot
void G_NoopCallback(const float *origin, const float *rgba, float scale, const char *text)
{
    (void)origin;
    (void)rgba;
    (void)scale;
    (void)text;
}

// 0x0810a6c0  size=19  FUN_0810a6c0  status=translated  naming=unknown
// forwards to G_FreeEntity (entity use release)
void G_ReleaseEntityUse(struct gentity_s *ent)
{
    G_FreeEntity(ent);
}

// 0x0810a708  size=227  FUN_0810a708  status=translated  naming=inferred
// teleport counterpart: unlink, copy origin to the client with +1 z lift, toggle the teleport bit (eFlags ^= 2), relink
void G_TeleportEntity(struct gentity_s *ent, const float *origin, const float *angles)
{
    signed char wasLinked;

    wasLinked = (signed char)ent->r.linked;
    SV_UnlinkEntity(ent);
    Vec3_Copy_0810cdaa(origin, ent->client->ps.origin);
    ent->client->ps.origin[2] = ent->client->ps.origin[2] + 1.0f;
    ent->client->ps.eFlags ^= 2;
    SetClientViewAngle(ent, angles);
    BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, 1, 1);
    Vec3_Copy_0810cdaa(ent->client->ps.origin, ent->r.currentOrigin);
    if (wasLinked != 0) {
        SV_LinkEntity(ent);
    }
}

// 0x0810a84a  size=53  FUN_0810a84a  status=translated  naming=inferred
// frees all 32 turret slots (inUse = 0)
void G_ClearTurretSlots(void)
{
    int i;

    for (i = 0; i < 0x20; i++) {
        g_turrets[i].inUse = 0;
    }
}

// 0x0810a880  size=257  FUN_0810a880  status=translated  naming=observed
// string "tag_flash" is named in the "Couldn't find %s on turret" error raised here
void Turret_ComputeFlashFrame(struct gentity_s *turret, void *viewer, float *frame)
{
    float tag[12];              /* orientation buffer; origin at tag[9] */
    float eye[3];               /* local_4c */
    float dir[3];               /* local_5c */
    float dist;

    if (Ent_SetNodeFromSlot(turret, (int)scrStr_tag_flash, (int)tag) == 0) {
        const char *modelName = ScrStr_TextFromHandle(turret->classname);
        Com_Error(1, "\025Couldn't find %s on turret (entity %d, classname '%s').\n", "tag_flash",
                  turret->s.number, modelName);
    }
    G_GetPlayerViewOrigin((gentity_s *)viewer, eye);
    G_GetPlayerViewAxis((gentity_s *)viewer, frame, frame + 3, frame + 6);
    Vec3_Copy_0810cdaa(frame, frame + 12);
    Vec3_Sub_0810ce1e(tag + 9, eye, dir);
    dist = Vec3_Normalize(dir);
    Vec3_MultiplyAdd(eye, dist, frame, frame + 9);
}

// 0x0810a982  size=205  FUN_0810a982  status=translated  naming=unknown
// computes the flash frame for the viewer, selects the aim path from weaponinfo.fireModeFlag, sends event 0xaf
void Turret_UpdateAimState(struct gentity_s *turret, void *viewer)
{
    float frame[15];            /* local_4c (60 bytes) */
    void *view;
    struct weaponDef_s *w;

    view = (viewer == (void *)g_087a21d0) ? (void *)g_dummyEntity : viewer;
    Turret_ComputeFlashFrame(turret, view, frame);
    w = BG_GetWeaponDef(turret->s.weapon);
    if (w->weaponType == 0) {
        G_FireWeaponShot((gentity_s *)view, FloatBits(turret->pTurretInfo->maxAimRate), (shot_view_s *)frame, turret, g_0864c56c);
    } else {
        G_ComputeFovViewOffset(turret, 0, (shot_view_s *)frame);
    }
    G_AddEvent(turret, 0xaf, *(int *)view);
}

// 0x0810aa50  size=3198  FUN_0810aa50  status=partial  naming=unknown
// positions a seated user on the turret tag_weapon chain; anim-tree helpers fn_080c* are unresolved
// missing: the fn_080c* anim/pose helper contracts (argument meanings) and the g_clientInfos bank layout beyond 0x4a4
void Turret_PositionUser(struct gentity_s *user, struct gentity_s *turret)
{
    int slot = user->s.clientNum;
    struct clientInfo_s *bank = &g_clientInfos[slot];
    struct XAnimNodeTable *animTable = (struct XAnimNodeTable *)(size_t)g_turretAnimTableHandle;
    struct XAnimTree *pose = (struct XAnimTree *)bank->pose;
    float *tagWeapon;
    struct weaponDef_s *w;
    int useMask;
    float tagMatrix[12];        /* local_13c */
    float tagLen;               /* local_34 */
    /* gameplay-fix(wpn_mantle_turret): local_9c[9] + auStack_78[3] are one 4x3 matrix ([ebp-0xa0..]) that Mat34_ConcatRowVector (0x0810b4..) reads as 12 floats */
    float turretAxis[12];       /* local_9c, origin row at +9 (auStack_78) */
    float rel[3];               /* local_16c */
    float upDot;                /* local_a0 */
    float heightRel;            /* local_38 */
    float dirVec[3];            /* local_5c */
    /* gameplay-fix(wpn_mantle_turret): XAnim_BlendRootAll (0x080c15ba) writes a vec3 at [ebp-0x68]; the loop exit test and the segment distance read its [2] ([ebp-0x60], 0x0810aeee/0x0810b159), not [1] */
    float arcRange[3];          /* local_6c */
    float axisYaw[12];          /* local_dc, origin row at +9 (auStack_b8) */
    float outOrient[12];        /* local_10c */
    float startPos[3];          /* local_17c */
    float endPos[3];            /* local_18c */
    float clampOut[3];          /* local_1cc */
    float traceRes[12];         /* local_1bc */
    float frac;                 /* local_48 */
    float prevDist;             /* local_3c */
    float prevFrac;             /* local_40 */
    int prevIdx;                /* local_20 */
    int outer;                  /* local_18 */
    int inner;                  /* local_1c */
    int iter;                   /* local_10 */
    int idx;                    /* local_24 */
    int handle;                 /* local_158 */
    int childA;                 /* local_150 */
    int childB;                 /* local_154 */
    float tmp;
    long double lv;
    float v4c;

    if (bank->useFlags != 0 && bank->turret != NULL && (*(uint32 *)&bank->turret->s.apos.trDelta[2] & 4) != 0) {
        tagWeapon = (float *)Ent_GetSlotBase(turret, (int)scrStr_tag_weapon);
        if (tagWeapon == NULL) {
            Com_Printf("WARNING: aborting player positioning on turret since 'tag_weapon' does not exist\n");
        } else {
            w = BG_GetWeaponDef(turret->s.weapon);
            useMask = (int)(bank->useFlags & 0xfffffdffu);
            QuatTransform_ToMat3((const QuatTransform_s *)tagWeapon, tagMatrix);
            tagLen = (float)Vec2_AngleDegreesRaw(tagMatrix);
            AnglesToAxis(turret->r.currentAngles, turretAxis);
            Vec3_Copy_0810cdaa(turret->r.currentOrigin, (turretAxis + 9));
            Vec3_Sub_0810ce1e(user->r.currentOrigin, (turretAxis + 9), rel);
            upDot = (float)Vec3_Dot_0810ceb6(rel, turretAxis + 6);
            heightRel = upDot - tagWeapon[6];
            XAnim_SetChildrenTime(pose, (uint32)useMask, 0);
            outer = XAnim_NodeChildCount(animTable, useMask);
            prevDist = 0.0f;
            prevFrac = 0.0f;
            prevIdx = 0;
            childB = 0;
            if (outer == 0) {
                Com_Error(1, "\025Player anim '%s' has no children", XAnim_NodeDisplayName(animTable, useMask));
            }
            iter = 0;
            do {
                handle = XAnim_NodeChildId(animTable, useMask, iter);
                XAnim_BlendStepC(pose, handle, 1.0f, 1.0f, 1.0f, 0, 0, 0);
                inner = XAnim_NodeChildCount(animTable, handle);
                if (inner == 0) {
                    Com_Error(1, "\025Player anim '%s' has no children", XAnim_NodeDisplayName(animTable, handle));
                }
                frac = (float)((double)(float)inner * 0.5 - (double)(tagLen / w->animHorRotateInc));
                if (frac < 0.0f) {
                    frac = 0.0f;
                } else if ((float)(inner - 1) <= frac) {
                    frac = (float)(inner - 1);
                }
                /* gameplay-fix(wpn_mantle_turret): 0x0810ad98..0x0810adb9 fistp with control word | 0xc00 = truncation, not lrintf */
                idx = (int)frac;
                frac = frac - (float)idx;
                childA = XAnim_NodeChildId(animTable, handle, idx);
                XAnim_BlendStepC(pose, childA, (float)(1.0 - (double)frac), 1.0f, 1.0f, 0, 0, 0);
                if (frac != 0.0f || frac != frac) {
                    childB = XAnim_NodeChildId(animTable, handle, idx + 1);
                    XAnim_BlendStepC(pose, childB, frac, 1.0f, 1.0f, 0, 0, 0);
                }
                XAnim_BlendRootAll(pose, handle, dirVec, arcRange);
                if (heightRel <= arcRange[2]) {
                    break;
                }
                prevDist = arcRange[2];
                prevIdx = idx;
                prevFrac = frac;
                iter = iter + 1;
            } while (iter < outer);

            XAnim_SetChildrenTime(pose, (uint32)useMask, 0);
            lv = (long double)XAnim_NodeOldWeight(pose, childA);
            lv = (long double)Float_Abs_0810ccd8((float)(lv - ((long double)1 - (long double)frac)));
            v4c = (float)(((long double)1000.0 / (long double)level_frameTime) * lv);
            tmp = (0.0f < v4c) ? (float)(1.0 / (double)v4c) : 0.0f;
            XAnim_BlendStepC(pose, childA, (float)(1.0 - (double)frac), tmp, 1.0f, 0, 0, 0);
            if (frac != 0.0f || frac != frac) {
                lv = (long double)XAnim_NodeOldWeight(pose, childB);
                lv = (long double)Float_Abs_0810ccd8((float)(lv - (long double)frac));
                v4c = (float)(((long double)1000.0 / (long double)level_frameTime) * lv);
                tmp = (0.0f < v4c) ? (float)(1.0 / (double)v4c) : 0.0f;
                XAnim_BlendStepC(pose, childB, frac, tmp, 1.0f, 0, 0, 0);
            }
            if (iter == 0 || iter == outer) {
                lv = (long double)XAnim_NodeOldWeight(pose, handle);
                lv = (long double)Float_Abs_0810ccd8((float)(lv - (long double)1));
                v4c = (float)(((long double)1000.0 / (long double)level_frameTime) * lv);
                tmp = (0.0f < v4c) ? (float)(1.0 / (double)v4c) : 0.0f;
                XAnim_BlendStepC(pose, handle, 1.0f, tmp, 1.0f, 0, 0, 0);
            } else {
                float blend = (heightRel - prevDist) / (arcRange[2] - prevDist);
                lv = (long double)XAnim_NodeOldWeight(pose, handle);
                lv = (long double)Float_Abs_0810ccd8((float)(lv - (long double)blend));
                v4c = (float)(((long double)1000.0 / (long double)level_frameTime) * lv);
                tmp = (0.0f < v4c) ? (float)(1.0 / (double)v4c) : 0.0f;
                XAnim_BlendStepC(pose, handle, blend, tmp, 1.0f, 0, 0, 0);
                handle = XAnim_NodeChildId(animTable, useMask, iter - 1);
                lv = (long double)XAnim_NodeOldWeight(pose, handle);
                lv = (long double)Float_Abs_0810ccd8((float)(lv - ((long double)1 - (long double)blend)));
                v4c = (float)(((long double)1000.0 / (long double)level_frameTime) * lv);
                tmp = (0.0f < v4c) ? (float)(1.0 / (double)v4c) : 0.0f;
                XAnim_BlendStepC(pose, handle, (float)(1.0 - (double)blend), tmp, 1.0f, 0, 0, 0);
                childA = XAnim_NodeChildId(animTable, handle, prevIdx);
                lv = (long double)XAnim_NodeOldWeight(pose, childA);
                lv = (long double)Float_Abs_0810ccd8((float)(lv - ((long double)1 - (long double)prevFrac)));
                v4c = (float)(((long double)1000.0 / (long double)level_frameTime) * lv);
                tmp = (0.0f < v4c) ? (float)(1.0 / (double)v4c) : 0.0f;
                XAnim_BlendStepC(pose, childA, (float)(1.0 - (double)prevFrac), tmp, 1.0f, 0, 0, 0);
                if (prevFrac != 0.0f || prevFrac != prevFrac) {
                    childB = XAnim_NodeChildId(animTable, handle, prevIdx + 1);
                    lv = (long double)XAnim_NodeOldWeight(pose, childB);
                    lv = (long double)Float_Abs_0810ccd8((float)(lv - (long double)prevFrac));
                    v4c = (float)(((long double)1000.0 / (long double)level_frameTime) * lv);
                    tmp = (0.0f < v4c) ? (float)(1.0 / (double)v4c) : 0.0f;
                    XAnim_BlendStepC(pose, childB, prevFrac, tmp, 1.0f, 0, 0, 0);
                }
            }
            XAnim_BlendRootAll(pose, useMask, dirVec, arcRange);
            Vec2_RotateDegrees(arcRange, tagLen);
            Vec2_Add_0810cd30(arcRange, tagWeapon + 4, axisYaw + 9);
            axisYaw[11] = upDot;
            {
                float yawSum;
                lv = (long double)Quat_PlanarAngleDeg(dirVec);
                yawSum = (float)(lv + (long double)tagLen);
                AnglesToAxis_ZUp(yawSum, axisYaw);
            }
            Mat34_ConcatRowVector(axisYaw, turretAxis, outOrient);
            Vec3_Copy_0810cdaa(outOrient + 9, user->client->ps.origin);
            Vec3_Copy_0810cdaa(user->client->ps.origin, startPos);
            Vec3_Copy_0810cdaa(user->client->ps.origin, endPos);
            startPos[2] = startPos[2] + user->client->ps.viewHeightCurrent;
            endPos[2] = endPos[2] - 60.0f;
            G_TraceBox((struct trace_s *)traceRes, startPos, vec3_origin, vec3_origin, endPos,
                       user->s.number, 0x2810011);
            if ((double)traceRes[0] < 1.0) {
                VectorLerp(startPos, endPos, traceRes[0], clampOut);
                user->client->ps.origin[2] = clampOut[2];
            }
            BG_PlayerStateToEntityState(&user->client->ps, &user->s, 1, 1);
            Vec3_Copy_0810cdaa(user->client->ps.origin, user->r.currentOrigin);
            AxisToAngles(outOrient, user->r.currentAngles);
            SV_LinkEntity(user);
        }
    }
}

// 0x0810b6ce  size=274  FUN_0810b6ce  status=translated  naming=unknown
// sets the turret's pitch/yaw from the user's view angles, clamped by the turret arcs; clears pending steer flag
void Turret_SetInitialAngles(struct gentity_s *turret, struct gentity_s *user)
{
    struct turret_s *t = turret->pTurretInfo;
    struct gclient_s *client = user->client;

    client->ps.viewlocked = 1;
    client->ps.viewlocked_entNum = turret->s.number;
    turret->s.angles2[0] = AngleSubtract(client->ps.viewangles[0], turret->r.currentAngles[0]);
    turret->s.angles2[0] = Float_Clamp_0810ccf2(turret->s.angles2[0], t->arcTop, t->arcBottom);
    turret->s.angles2[1] = AngleSubtract(client->ps.viewangles[1], turret->r.currentAngles[1]);
    turret->s.angles2[1] = Float_Clamp_0810ccf2(turret->s.angles2[1], t->arcRight, t->arcLeft);
    turret->s.angles2[2] = 0.0f;
    if ((t->flags & 0x800) != 0) {
        t->flags = t->flags & 0xfffff7ffu;
        turret->s.eFlags = turret->s.eFlags ^ 2;
    }
}

// 0x0810b7e0  size=111  FUN_0810b7e0  status=translated  naming=unknown
// starts the reload timer from weaponinfo.fireInterval*3 and forces the user aim state to 2
void Turret_StartReload(struct gentity_s *turret, struct gentity_s *user)
{
    struct turret_s *t = turret->pTurretInfo;
    struct weaponDef_s *w = BG_GetWeaponDef(turret->s.weapon);

    t->timer = w->fireTime * 3;
    if (user->client != NULL) {
        /* decompiler passes a third argument here that the callee ignores */
        Turret_UpdateAimState(turret, user);
        user->client->ps.viewlocked = 2;
    }
}

// 0x0810b850  size=214  FUN_0810b850  status=translated  naming=unknown
// full per-frame update of a seated user: angles, positioning, aim state, cooldown and reload restart
void Turret_UpdateUser(struct gentity_s *turret, struct gentity_s *user)
{
    struct turret_s *t = turret->pTurretInfo;
    struct weaponDef_s *w;

    Turret_SetInitialAngles(turret, user);
    Turret_PositionUser(user, turret);
    w = BG_GetWeaponDef(turret->s.weapon);
    user->client->ps.viewlocked = 1;
    turret->s.eFlags &= 0xffffffbfu;
    t->cooldown08 = t->cooldown08 - 50;
    if (t->cooldown08 < 1) {
        t->cooldown08 = 0;
        if ((user->client->buttons & 1) != 0) {
            t->cooldown08 = w->fireTime;
            Turret_StartReload(turret, user);
            turret->s.eFlags |= 0x40u;
        }
    }
}

// 0x0810b926  size=126  FUN_0810b926  status=translated  naming=unknown
// counts the reload timer down by 50 and reports the tag event when it expires
void Turret_UpdateAmmo(struct gentity_s *turret)
{
    struct turret_s *t = turret->pTurretInfo;

    turret->s.loopSound = 0;
    if (t->timer > 0) {
        turret->s.loopSound = t->tag0;
        t->timer = t->timer - 50;
        if (t->timer < 1 && t->tag2 != 0) {
            turret->s.loopSound = 0;
            Ent_AddEventB3IfNonzero(turret, t->tag2);
        }
    }
}

// 0x0810b9a4  size=358  FUN_0810b9a4  status=translated  naming=unknown
// releases the seated user: resets turret/client/target flags, sends the tag event and teleports the user
void Turret_ReleaseUser(struct gentity_s *turret)
{
    struct turret_s *t = turret->pTurretInfo;
    int target = turret->r.ownerNum;
    struct gentity_s *ent = &g_gentities[target];

    t->timer = 0;
    turret->s.loopSound = 0;
    if (t->tagSlot != -1) {
        if (t->tagSlot == 2) {
            G_AddEvent(ent, 0x8e, 0);
        } else if (t->tagSlot == 1) {
            G_AddEvent(ent, 0x8d, 0);
        } else {
            G_AddEvent(ent, 0x8c, 0);
        }
        t->tagSlot = -1;
    }
    G_TeleportEntity(ent, t->aimOrigin, ent->r.currentAngles);
    g_gentities[target].client->ps.eFlags &= 0xfffffcffu;
    g_gentities[target].client->ps.viewlocked = 0;
    g_gentities[target].client->ps.viewlocked_entNum = 0x3ff;
    ent->active = 0;
    ent->s.otherEntityNum = 0;
    turret->active = 0;
    turret->r.ownerNum = 0x3ff;
    t->flags = t->flags & 0xfffff7ffu;
}

// 0x0810bb0a  size=107  FUN_0810bb0a  status=translated  naming=unknown
// per-frame turret think: update the pending user when present, otherwise release it
void Turret_Think(struct gentity_s *turret)
{
    struct gentity_s *ent = &g_gentities[turret->r.ownerNum];

    if (ent->active == 1 && ent->client->sessionState == 0) {
        Turret_UpdateUser(turret, ent);
        Turret_UpdateAmmo(turret);
    } else {
        Turret_ReleaseUser(turret);
    }
}

// 0x0810bb76  size=677  FUN_0810bb76  status=translated  naming=unknown
// advances aim angles toward target by at most the per-frame rate limit; returns 1 when unclamped
int Turret_ClampAim(struct gentity_s *turret, const float *target, int hasLimit)
{
    struct turret_s *t = turret->pTurretInfo;
    int result = 1;
    float prevYaw = turret->s.angles2[0];
    float limits[2];
    float local30;
    float local20;
    int i;

    turret->s.angles2[0] = turret->s.angles2[0] + turret->s.angles2[2];
    if (hasLimit == 0) {
        limits[0] = 200.0f;
        limits[1] = 200.0f;
    } else {
        struct weaponDef_s *w;
        w = BG_GetWeaponDef(turret->s.weapon);
        limits[0] = w->maxVertTurnSpeed;
        w = BG_GetWeaponDef(turret->s.weapon);
        limits[1] = w->maxHorTurnSpeed;
    }
    if ((t->flags & 0x200) != 0 && (t->flags & 0x100) != 0 && limits[0] < 360.0f) {
        limits[0] = 360.0f;
    }
    for (i = 0; i < 2; i++) {
        limits[i] = (float)((double)limits[i] * 0.05);
        local20 = AngleSubtract(target[i], turret->s.angles2[i]);
        if (limits[i] < local20) {
            result = 0;
            local20 = limits[i];
        } else if (local20 < -limits[i]) {
            result = 0;
            local20 = -limits[i];
        }
        turret->s.angles2[i] = turret->s.angles2[i] + local20;
    }
    local30 = turret->s.angles2[0];
    turret->s.angles2[2] = local30;
    if ((t->flags & 0x200) != 0) {
        if ((t->flags & 0x400) == 0) {
            if (t->unk3c < turret->s.angles2[0]) {
                local30 = t->unk3c;
            } else {
                t->flags = t->flags & 0xfffffeffu;
            }
        } else if (turret->s.angles2[0] < t->unk3c) {
            local30 = t->unk3c;
        } else {
            t->flags = t->flags & 0xfffffeffu;
        }
    }
    local20 = AngleSubtract(local30, prevYaw);
    if (limits[0] < local20) {
        result = 0;
        local20 = limits[0];
    } else if (local20 < -limits[0]) {
        result = 0;
        local20 = -limits[0];
    }
    turret->s.angles2[0] = prevYaw + local20;
    turret->s.angles2[2] = turret->s.angles2[2] - turret->s.angles2[0];
    return result;
}

// 0x0810be1c  size=81  FUN_0810be1c  status=translated  naming=unknown
// rest-aim wrapper: target is the turret rest angle (flag==0) or zero, then ClampAim
void Turret_RestAim(struct gentity_s *turret, int flag)
{
    float target[2];
    float rest;

    if (flag == 0) {
        rest = turret->pTurretInfo->restAngle;
    } else {
        rest = 0.0f;
    }
    target[0] = rest;
    target[1] = 0.0f;
    Turret_ClampAim(turret, target, flag);
}

// 0x0810c1be  size=302  FUN_0810c1be  status=translated  naming=unknown
// true when the turret's aim cone (current yaw + mean arc half-width) covers the direction to `other`
int Turret_IsAimingAtUser(struct gentity_s *ent, struct gentity_s *other)
{
    struct turret_s *t = ent->pTurretInfo;
    float yaw = ent->r.currentAngles[1];
    float arcR = t->arcRight;
    float halfArc;
    float axis[3];
    float dir[3];
    long double lv;
    float fyaw;

    lv = (long double)Float_Abs_0810ccd8(t->arcRight);
    lv = (long double)0.5 * (lv + (long double)Float_Abs_0810ccd8(t->arcLeft));
    halfArc = (float)lv;
    fyaw = AngleNormalize180(yaw + arcR + halfArc);
    Angle_YawToForwardRight(fyaw, axis, 0);
    Vec3_Normalize(axis);
    Vec3_Sub_0810ce1e(ent->r.currentOrigin, other->r.currentOrigin, dir);
    /* gameplay-fix(wpn_mantle_turret): 0x0810c26c..0x0810c271 zero dir[2] ([ebp-0x20]) before the normalize */
    dir[2] = 0.0f;
    Vec3_Normalize(dir);
    lv = Vec3_Dot_0810ceb6(axis, dir);
    lv = (long double)Float_Clamp_0810ccf2((float)lv, -1.0f, 1.0f);
    lv = (long double)Math_AcosClamped((float)lv);
    return (float)((long double)57.29577951308232 * lv) <= halfArc;
}

// 0x0810c2ec  size=108  FUN_0810c2ec  status=translated  naming=unknown
// clears the turret link: releases the target if the client is attached, resets pending and the slot pointer
void Turret_Reset(struct gentity_s *ent)
{
    if (g_gentities[ent->r.ownerNum].client != NULL) {
        Turret_ReleaseUser(ent);
    }
    ent->active = 0;
    ent->pTurretInfo->inUse = 0;
    ent->pTurretInfo = NULL;
}

// 0x0810c358  size=133  FUN_0810c358  status=translated  naming=unknown
// true when the turret is unlocked, has a slot, the user is aiming at it and the user is not already on a turret
int Turret_CanBeUsed(struct gentity_s *ent, struct gentity_s *user)
{
    if (ent->active == 0 && ent->pTurretInfo != NULL) {
        if (Turret_IsAimingAtUser(ent, user) == 0) {
            return 0;
        }
        if (user->client->ps.grenadeTimeLeft == 0) {
            if (user->client->ps.groundEntityNum == 0x3ff) {
                return 0;
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

// 0x0810c6b2  size=1496  FUN_0810c6b2  status=translated  naming=observed
// strings "G_SpawnTurret: weapon '%s' isn't a turret", "turret '%s' not precached" and "max number of turrets" name it
void G_SpawnTurret(struct gentity_s *ent, const char *weaponName)
{
    int slot;
    struct turret_s *t = NULL;
    struct weaponDef_s *w;
    int arcBits;
    int sprBits;

    slot = 0;
    while (slot < 0x20) {
        t = &g_turrets[slot];
        if (t->inUse == 0) {
            break;
        }
        slot++;
    }
    if (slot == 0x20) {
        Com_Error(1, "\025G_SpawnTurret: max number of turrets (%d) exceeded", 0x20);
    }
    memset(t, 0, 0x44);
    ent->pTurretInfo = (struct turret_s *)t;
    t->inUse = 1;
    ent->s.weapon = G_LoadWeaponDef(weaponName);
    if (ent->s.weapon == 0) {
        Com_Error(1, "\025bad weaponinfo '%s' specified for turret", weaponName);
    }
    w = BG_GetWeaponDef(ent->s.weapon);
    if (w->weaponClass != 7) {
        Scr_Error(va("G_SpawnTurret: weapon '%s' isn't a turret. This usually indicates that the weapon failed to load.", weaponName));
    }
    if (level_initializing == 0 && ItemRegistry_IsRegistered(ent->s.weapon) == 0) {
        Scr_Error(va("turret '%s' not precached", weaponName));
    }
    t->cooldown08 = 0;
    t->weaponParam = w->stance;
    t->tagSlot = -1;
    t->timer = 0;
    if (w->loopFireSound == NULL || *w->loopFireSound == '\0') {
        t->tag0 = 0;
    } else {
        t->tag0 = G_SoundIndex(w->loopFireSound);
    }
    if (w->loopFireSoundPlayer == NULL || *w->loopFireSoundPlayer == '\0') {
        t->tag1 = 0;
    } else {
        t->tag1 = G_SoundIndex(w->loopFireSoundPlayer);
    }
    if (w->stopFireSound == NULL || *w->stopFireSound == '\0') {
        t->tag2 = 0;
    } else {
        t->tag2 = G_SoundIndex(w->stopFireSound);
    }
    if (w->stopFireSoundPlayer == NULL || *w->stopFireSoundPlayer == '\0') {
        t->tag3 = 0;
    } else {
        t->tag3 = G_SoundIndex(w->stopFireSoundPlayer);
    }

    arcBits = FloatBits(w->rightArc);
    if (*(byte *)&g_spawnVars == 0 || G_SpawnFloat("rightarc", "", (float *)&arcBits) == 0) {
        arcBits = FloatBits(w->rightArc);
    }
    arcBits = arcBits ^ (int)0x80000000u;
    if (0.0f < BitsFloat(arcBits)) {
        arcBits = 0;
    }
    t->arcRight = BitsFloat(arcBits);

    arcBits = FloatBits(w->topArc);
    if (*(byte *)&g_spawnVars == 0 || G_SpawnFloat("toparc", "", (float *)&arcBits) == 0) {
        arcBits = FloatBits(w->topArc);
    }
    arcBits = arcBits ^ (int)0x80000000u;
    if (0.0f < BitsFloat(arcBits)) {
        arcBits = 0;
    }
    t->arcTop = BitsFloat(arcBits);

    arcBits = FloatBits(w->leftArc);
    if (*(byte *)&g_spawnVars == 0 || G_SpawnFloat("leftarc", "", (float *)&arcBits) == 0) {
        arcBits = FloatBits(w->leftArc);
    }
    if (BitsFloat(arcBits) < 0.0f) {
        arcBits = 0;
    }
    t->arcLeft = BitsFloat(arcBits);

    arcBits = FloatBits(w->bottomArc);
    if (*(byte *)&g_spawnVars == 0 || G_SpawnFloat("bottomarc", "", (float *)&arcBits) == 0) {
        arcBits = FloatBits(w->bottomArc);
    }
    if (BitsFloat(arcBits) < 0.0f) {
        arcBits = 0;
    }
    t->arcBottom = BitsFloat(arcBits);

    t->restAngle = BitsFloat((int)0xC2B40000u);

    if (ent->health == 0) {
        ent->health = 100;
    }
    if (*(byte *)&g_spawnVars == 0 || G_SpawnInt("damage", "0", &ent->damage) == 0) {
        ent->damage = w->damage;
    }
    if (ent->damage < 0) {
        ent->damage = 0;
    }
    sprBits = FloatBits(w->playerSpread);
    if (*(byte *)&g_spawnVars == 0 || G_SpawnFloat("playerSpread", "1", (float *)&sprBits) == 0) {
        sprBits = FloatBits(w->playerSpread);
    }
    if (BitsFloat(sprBits) < 0.0f) {
        sprBits = 0;
    }
    t->maxAimRate = BitsFloat(sprBits);

    t->flags = 3;
    t->cooldown08 = 0;
    t->weaponParam = w->stance;
    ent->clipmask = 1;
    ent->r.contents = 0x200004;
    ent->r.svFlags = 0;
    ent->s.eType = 9;
    ent->flags = ent->flags | 0x1000u;
    Ent_RefreshAttachments(ent);
    Vec3_Set_0810cd86(ent->r.mins, -32.0f, -32.0f, 0.0f);
    Vec3_Set_0810cd86(ent->r.maxs, 32.0f, 32.0f, 56.0f);
    G_SetPositionTrajectory(ent, ent->r.currentOrigin);
    G_SetAnglesTrajectory(ent, ent->r.currentAngles);
    Vec3_Clear_0810cd5c(ent->s.angles2);
    ent->handler = 0xd;
    ent->nextthink = g_0864c56c + 0x32;
    ent->s.apos.trType = 3;
    ent->takedamage = 0;
    SV_LinkEntity(ent);
}

// 0x0810cc8a  size=77  FUN_0810cc8a  status=translated  naming=inferred
// reads the "weaponinfo" key for a turret spawn and forwards to G_SpawnTurret
void G_SpawnTurretFromWeapon(struct gentity_s *ent)
{
    const char *weaponName = NULL;
    int found;

    found = G_SpawnString("weaponinfo", "", &weaponName);
    if (found == 0) {
        Com_Error(1, "\025no weaponinfo specified for turret");
    }
    G_SpawnTurret(ent, weaponName);
}

// 0x0810ccd8  size=25  FUN_0810ccd8  status=translated  naming=unknown
// absolute value of a float
float Float_Abs_0810ccd8(float v)
{
    float r;
    r = __builtin_fabsf(v); /* the original executes x87 fabs (also clears the sign of -0.0 and NaNs) */
    return r;
}

// 0x0810ccf2  size=61  FUN_0810ccf2  status=translated  naming=unknown
// clamp: value limited to [min, max] (Float_Select_0810ceea ladder as in the disassembly)
float Float_Clamp_0810ccf2(float v, float min, float max)
{
    float r1;
    r1 = Float_Select_0810ceea(v - max, max, v);
    return Float_Select_0810ceea(min - v, min, r1);
}

// 0x0810cd30  size=44  FUN_0810cd30  status=translated  naming=inferred
// 2D add (x, y only)
void Vec2_Add_0810cd30(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
}

// 0x0810cd5c  size=41  FUN_0810cd5c  status=translated  naming=inferred
// zero a 3-vector
void Vec3_Clear_0810cd5c(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x0810cd86  size=35  FUN_0810cd86  status=translated  naming=inferred
// set a 3-vector from three words
void Vec3_Set_0810cd86(float *v, float x, float y, float z)
{
    /* integer dword moves as in the original (no x87 load/store: signalling NaN payloads survive) */
    ((volatile uint32 *)v)[0] = *(volatile uint32 *)&x;
    ((volatile uint32 *)v)[1] = *(volatile uint32 *)&y;
    ((volatile uint32 *)v)[2] = *(volatile uint32 *)&z;
}

// 0x0810cdaa  size=47  FUN_0810cdaa  status=translated  naming=inferred
// copy a 3-vector (src in first argument)
void Vec3_Copy_0810cdaa(const float *in, float *out)
{
    /* integer dword moves as in the original (no x87 load/store) */
    ((volatile uint32 *)out)[0] = ((const volatile uint32 *)in)[0];
    ((volatile uint32 *)out)[1] = ((const volatile uint32 *)in)[1];
    ((volatile uint32 *)out)[2] = ((const volatile uint32 *)in)[2];
}

// 0x0810cdda  size=68  FUN_0810cdda  status=translated  naming=inferred
// out = a + b
void Vec3_Add_0810cdda(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x0810ce1e  size=68  FUN_0810ce1e  status=translated  naming=inferred
// out = a - b
void Vec3_Sub_0810ce1e(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x0810ce62  size=83  FUN_0810ce62  status=translated  naming=inferred
// out = a + scale * b
void Vec3_MultiplyAdd(const float *a, float scale, const float *b, float *out)
{
    out[0] = a[0] + scale * b[0];
    out[1] = a[1] + scale * b[1];
    out[2] = a[2] + scale * b[2];
}

// 0x0810ceb6  size=51  FUN_0810ceb6  status=translated  naming=inferred
// dot product in x87 extended precision, summed as ((z*z + y*y) + x*x)
long double Vec3_Dot_0810ceb6(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2] +
           (long double)a[1] * (long double)b[1] +
           (long double)a[0] * (long double)b[0];
}

// 0x0810ceea  size=41  FUN_0810ceea  status=translated  naming=inferred
// select: returns b when a >= 0, otherwise c
float Float_Select_0810ceea(float a, float b, float c)
{
    return (a >= 0.0f) ? b : c;
}

// 0x0810cf14  size=56  FUN_0810cf14  status=translated  naming=inferred
// out = scale * in
void Vec3_Scale_0810cf14(const float *in, float scale, float *out)
{
    out[0] = scale * in[0];
    out[1] = scale * in[1];
    out[2] = scale * in[2];
}

// 0x0810cf4c  size=603  FUN_0810cf4c  status=translated  naming=unknown
// evaluates the turret's angular trajectory at the input time and steers the spin/yaw (random jitter when not initial)
void Turret_UpdateTrajectoryAim(struct gentity_s *ent, const float *input, float *out, int isInitial)
{
    int time;
    float local2c[3];
    float yawBase;
    float deltaYaw;
    float absDelta;

    /* gameplay-fix(wpn_grenade): fild * float then fistp with the control word forced to truncate (0x0810cf72..0x0810cf93), not round-to-nearest */
    time = (int)((long double)(g_0864c56c - level_previousTime) * (long double)input[0]) + level_previousTime;
    BG_EvaluateTrajectory(&ent->s.apos, time, out);
    Vec3_Copy_0810ebae(out, local2c);
    if (0.1f < input[3]) {
        yawBase = AnglesToForward_ProjectedNormalized(out[1], input + 1);
        deltaYaw = AngleSubtract(yawBase, out[0]);
        absDelta = Abs_Float(deltaYaw);
        if (isInitial == 0) {
            Vec3_Copy_0810ebae(out, ent->s.apos.trBase);
            ent->s.apos.trTime = time;
            if (absDelta < 80.0f) {
                /* gameplay-fix(wpn_grenade): 0.3f/0.85f are float constants (0x815ae14/0x815ae18), not doubles; 0x0810d06c..0x0810d092 */
                ent->s.apos.trDelta[0] = -(float)(((long double)0.85f + (long double)0.3f * (long double)Rand_Fraction()) *
                                             (long double)ent->s.apos.trDelta[0]);
            } else {
                ent->s.apos.trDelta[0] = (float)((long double)ent->s.apos.trDelta[0] *
                                           ((long double)0.85f + (long double)0.3f * (long double)Rand_Fraction()));
            }
        }
        out[0] = AngleNormalize180(out[0]);
        if (isInitial != 0 || absDelta < 45.0f) {
            if ((long double)90.0 < (long double)Abs_Float(out[0])) {
                out[0] = AngleMod((float)((double)yawBase + 180.0));
            } else {
                out[0] = AngleMod(yawBase);
            }
        } else if (absDelta < 80.0f) {
            out[0] = AngleMod((float)((double)deltaYaw * 0.25 + (double)out[0]));
        } else {
            out[0] = AngleMod(out[0]);
        }
    } else if (isInitial == 0) {
        unsigned int r = (unsigned int)rand();
        ent->s.apos.trDelta[0] = AngleMod((float)(int)((r & 0x7f) - 0x3f) + ent->s.apos.trDelta[0]);
    }
}

// 0x0810d1a8  size=873  FUN_0810d1a8  status=translated  naming=unknown
// applies a turret input: evaluates the entity trajectory, updates trDelta from the weapon arc, returns 1 when the shot is blocked
int Turret_UpdateFromInput(struct gentity_s *ent, struct turret_input_s *in)
{
    struct weaponDef_s *w = BG_GetWeaponDef(ent->s.weapon);
    int contents;
    int seat;
    float pos[3];
    float dot;
    float len;
    float localOut[4];
    float local3c[3];
    int ret;

    contents = (int)SV_PointContents(ent->r.currentOrigin, -1, 0x20);
    seat = (int)((in->flags & 0x1f00000u) >> 20);
    /* gameplay-fix(wpn_grenade): truncating fistp of the extended product (0x0810d20b..0x0810d22c) */
    BG_EvaluateTrajectoryDelta(&ent->s.pos, (int)((long double)(g_0864c56c - level_previousTime) * (long double)in->fraction) + level_previousTime, pos);
    dot = Vec3_Dot_0810ecf2(pos, in->aim);
    Vec3_MA_0810ec9e(pos, dot * -2.0f, in->aim, ent->s.pos.trDelta);
    if (0.7f < in->aim[2]) {
        ent->s.groundEntityNum = (uint32)in->entityNum;
    }
    if ((ent->s.eFlags & 0x1000000u) != 0) {
        len = Vec3_Length_0810ed26(pos);
        if (0.0f < len && dot <= 0.0f) {
            Vec3_Scale_0810ec66(ent->s.pos.trDelta,
                        ((&w->perpendicularDefaultBounce)[seat] - (&w->parallelDefaultBounce)[seat]) * (dot / -len) + (&w->parallelDefaultBounce)[seat],
                        ent->s.pos.trDelta);
        }
        if (0.7f < in->aim[2] && Vec3_Length_0810ed26(ent->s.pos.trDelta) < 20.0f) {
            G_SetPositionTrajectory(ent, ent->r.currentOrigin);
            Turret_UpdateTrajectoryAim(ent, (const float *)in, localOut, 1);
            G_SetAnglesTrajectory(ent, localOut);
            return 0;
        }
    }
    Vec3_Scale_0810ec66(in->aim, 0.1f, local3c);
    if (0.0f < local3c[2]) {
        local3c[2] = 0.0f;
    }
    Vec3_Add_0810ebde(ent->r.currentOrigin, local3c, ent->r.currentOrigin);
    Vec3_Copy_0810ebae(ent->r.currentOrigin, ent->s.pos.trBase);
    ent->s.pos.trTime = g_0864c56c;
    Turret_UpdateTrajectoryAim(ent, (const float *)in, localOut, 0);
    Vec3_Copy_0810ebae(localOut, ent->s.apos.trBase);
    ent->s.apos.trTime = g_0864c56c;
    if (contents == 0) {
        Vec3_Subtract_0810ec22(ent->s.pos.trDelta, pos, pos);
        len = Vec3_Length_0810ed26(pos);
        ret = (100.0 < (double)len) ? 1 : 0;
    } else {
        ret = 0;
    }
    return ret;
}

// 0x0810d512  size=1117  FUN_0810d512  status=translated  naming=unknown
// dispatches a turret input: direct aim path for unoccupied targets, or the occupied path with damage/projectile events
void Turret_ProcessInput(struct gentity_s *ent, struct turret_input_s *in, void *unused, float *out)
{
    int localFlag = 0;
    struct gentity_s *target = &g_gentities[in->entityNum];
    struct gentity_s *aimEnt;
    struct weaponDef_s *w;
    int seat;
    int mode;
    int result;
    float origin[3];
    float len;
    void *dst;

    (void)unused;
    seat = (int)((in->flags & 0x1f00000u) >> 20);
    ent->s.surfType = seat;
    if (target->takedamage == 0 && (ent->s.eFlags & 0x1000000u) != 0) {
        result = Turret_UpdateFromInput(ent, in);
        if (result != 0 && in->pendingFlag == 0) {
            G_AddEvent(ent, 0xbb, seat);
        }
        return;
    }
    w = BG_GetWeaponDef(ent->s.weapon);
    mode = g_entityHandlerTable[ent->handler].field20;
    if (target->takedamage != 0) {
        if (ent->damage == 0) {
            if (target->client != NULL && in->flags == 0) {
                in->flags = 0x700000u;
            }
            result = Turret_UpdateFromInput(ent, in);
            if (result == 0) {
                return;
            }
            if (in->pendingFlag == 0) {
                /* gameplay-fix(wpn_grenade): parm is re-read from in->flags after the 0x700000 default was stored (0x0810d752..0x0810d775) */
                G_AddEvent(ent, 0xbb, (int)((in->flags & 0x1f00000u) >> 20) & 0xff);
            }
            return;
        }
        aimEnt = &g_gentities[ent->r.ownerNum];
        if (G_ClientCanTargetEntity(target, aimEnt) != 0) {
            localFlag = 1;
        }
        BG_EvaluateTrajectoryDelta(&ent->s.pos, g_0864c56c, origin);
        len = Vec3_Length_0810ed26(origin);
        if (len == 0.0f) {
            origin[2] = 1.0f;
        }
        G_Damage(target, ent, (ent->r.ownerNum == 0x3ff) ? NULL : aimEnt, origin, ent->r.currentOrigin,
                    ent->damage, 0, mode, 0, 0);
    }
    if (ent->damage != 0) {
        dst = (ent->r.ownerNum == 0x3ff) ? (void *)g_dummyEntity : (void *)&g_gentities[ent->r.ownerNum];
        Ent_AreaEffectApply((gentity_s *)dst, ent->r.currentOrigin, out, ent->damage, mode);
    }
    {
        int hit = (localFlag != 0 || in->seatCmd != 0);
        uint8 aimFlag = Normal_DirToIndex(in->aim);
        G_AddEvent(ent, hit ? 0xbe : 0xbd, aimFlag);
    }
    /* gameplay-fix(wpn_grenade): surfType re-read from in->flags (0x0810d858..0x0810d863) */
    ent->s.surfType = (int)((in->flags & 0x1f00000u) >> 20) & 0xff;
    ent->freeAfterEvent = 1;
    ent->s.eType = 0;
    ent->s.eFlags = ent->s.eFlags ^ 2u;
    ent->s.eFlags = ent->s.eFlags | 0x20u;
    ent->flags = ent->flags | 0x800u;
    Vec3_FloorOrCeilTowards(out, ent->s.pos.trBase);
    G_SetPositionTrajectory(ent, out);
    if (w->explosionInnerDamage != 0) {
        G_RadiusDamage(out, ent, ent->parent, (float)w->explosionInnerDamage, (float)w->explosionOuterDamage, (float)w->explosionRadius,
                    target, g_entityHandlerTable[ent->handler].field24);
    }
    SV_LinkEntity(ent);
}

// 0x0810dd2c  size=122  FUN_0810dd2c  status=translated  naming=unknown
// point trace with the table pointer parameter; zeroes fraction and sets endpos when the flag byte at 0x23 is set
void Turret_TraceAimWithFlags(struct trace_s *trace, float *start, float *end, int passEntityNum,
                              int contentmask)
{
    float dir[3];

    G_TracePointEx(trace, start, end, passEntityNum, contentmask, (const char *)g_08187c0c);
    if (trace->startsolid != 0) {
        trace->fraction = 0.0f;
        Vec3_Sub_0810ce1e(start, end, dir);
        Vec3_NormalizeCopy(dir, trace->normal);
    }
}

// 0x0810dda6  size=122  FUN_0810dda6  status=translated  naming=unknown
// traces with the target entity's contents mask temporarily zeroed, then restores it
void Turret_TraceAimNoContents(struct trace_s *trace, int index, struct gentity_s *turret, float *end)
{
    int saved = g_gentities[index].r.contents;

    g_gentities[index].r.contents = 0;
    Turret_TraceAimWithFlags(trace, turret->r.currentOrigin, end, turret->r.ownerNum, turret->clipmask);
    g_gentities[index].r.contents = saved;
}

// 0x0810de20  size=45  FUN_0810de20  status=translated  naming=inferred
// tangent of an angle given in degrees (computed in double, returned as float)
float Float_TanDeg(float deg)
{
    /* the radian factor is the double 0.017453292519943295 (rodata 0x0815ae48); the product is rounded to float before tan() */
    float rad = (float)((double)deg * 0.017453292519943295);
    double r = tan((double)rad);
    return (float)r;
}

// 0x0810de4e  size=469  FUN_0810de4e  status=translated  naming=unknown
// advances the turret's randomised projectile trajectory once its rate interval has elapsed
void Turret_Advance(struct gentity_s *ent)
{
    struct weaponDef_s *w;
    float dirv[3];
    float rnd[3];
    long double tanv;
    int i;

    if (lrintf(ent->u.turretRateMs) + ent->s.pos.trTime < g_0864c56c) {
        w = BG_GetWeaponDef(ent->s.weapon);
        Vec3_Copy_0810ebae(ent->s.pos.trDelta, dirv);
        Vec3_Normalize(dirv);
        tanv = (long double)Float_TanDeg(w->destabilizationAngleMax);
        for (i = 0; i < 3; i++) {
            rnd[i] = Rand_Float(-1.0f, 1.0f);
        }
        Vec3_Scale_0810ec66(rnd, (float)tanv, rnd);
        Vec3_Add_0810ebde(dirv, rnd, dirv);
        Vec3_Normalize(dirv);
        Vec3_Scale_0810ec66(dirv, (float)w->projectileSpeed, ent->s.pos.trDelta);
        Vec3_Copy_0810ebae(ent->r.currentOrigin, ent->s.pos.trBase);
        Vec3_ToAngles(dirv, ent->r.currentAngles);
        G_SetAnglesTrajectory(ent, ent->r.currentAngles);
        ent->s.pos.trTime = g_0864c56c;
        if ((ent->s.eFlags & 0x10000u) == 0) {
            ent->u.turretRateMs = (float)((double)w->destabilizationBaseTime * 1000.0);
        } else {
            ent->u.turretRateMs = ent->u.turretRateMs * w->destabilizationTimeReductionRatio;
        }
        ent->s.eFlags = ent->s.eFlags | 0x10000u;
    }
}
