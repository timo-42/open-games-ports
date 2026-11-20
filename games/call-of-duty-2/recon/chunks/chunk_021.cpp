/*
 * chunk_021.cpp - cod2_lnxded 1.3 reconstruction, chunk 021.
 * Range 0x08092002..0x0809602e, 71 functions, address order.
 *
 * Behaviour was taken from the disassembly-checked Ghidra pseudocode. Names are
 * observed when a string in the function names it, inferred when a Quake III /
 * IW server counterpart matches the strings, cvars and call shape, and unknown
 * otherwise. Out-of-chunk callees keep placeholder names (fn_08xxxxxx) and
 * unnamed globals keep g_08xxxxxx names, as the merge queue expects.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_021_types.h"

/* 0x08092002  size=136  FUN_08092002  status=translated  naming=unknown */
/* naming: no string; forwards to DObj_AreBonesLocked when the object's cached stamp matches, else allocates skel memory (SV_SkelMemoryAlloc) and stores it. */
int ScrEntity_LockBonesOrCreateSkel(struct gentity_s *ent, int arg)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);
    int result;

    if (DObj_SkelExists(obj, g_skelMemGeneration) != 0) {
        result = DObj_AreBonesLocked(obj, (const uint32 *)arg);
    } else {
        int size = DObj_GetAllocSkelSize(obj);
        unsigned char *mem = SV_SkelMemoryAlloc(size);
        DObj_CreateSkel(obj, (DObjSkel *)mem, g_skelMemGeneration);
        result = 0;
    }
    return result;
}

/* 0x0809208a  size=70  FUN_0809208a  status=translated  naming=unknown */
/* naming: no string; returns XAnim_BindingSetWeight((XAnimBinding *)obj, a, b) or 0 when the handle does not resolve. */
int SV_DObjBindingSetWeight(struct gentity_s *ent, float a, int b)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);
    int result = 0;

    if (obj != 0) {
        result = XAnim_BindingSetWeight((XAnimBinding *)obj, a, b);
    }
    return result;
}

/* 0x080920d0  size=50  FUN_080920d0  status=translated  naming=unknown */
/* naming: no string; calls XAnim_BindingSetTime((XAnimBinding *)obj, dt) when the handle resolves. The argument is the
 * frame time in seconds as a float: the original forwards the raw dword [ebp+0xc] (G_RunFrame passes
 * level.frameTime * 0.001f); an int parameter truncated it to 0 and froze every animation blend. */
void SV_DObjBindingSetTime(int *ref, float a)
{
    DObj *obj = Com_GetServerDObj(*ref);

    if (obj != 0) {
        XAnim_BindingSetTime((XAnimBinding *)obj, a);
    }
}

/* 0x08092102  size=49  FUN_08092102  status=translated  naming=unknown */
/* naming: no string; unconditional forward DObj_GetHierarchyBits(obj, (unsigned)a, (uint32 *)b). */
void SV_DObjGetHierarchyBits(struct gentity_s *ent, int a, int b)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);

    DObj_GetHierarchyBits(obj, (unsigned)a, (uint32 *)b);
}

/* 0x08092134  size=42  FUN_08092134  status=translated  naming=unknown */
/* naming: no string; unconditional forward XAnim_BindingRebuild((XAnimBinding *)obj, (const uint32 *)a). */
void SV_DObjBindingRebuild(struct gentity_s *ent, int a)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);

    XAnim_BindingRebuild((XAnimBinding *)obj, (const uint32 *)a);
}

/* 0x0809215e  size=42  FUN_0809215e  status=translated  naming=unknown */
/* naming: no string; unconditional forward DObj_CalcSkel(obj, (const uint32 *)a). */
void SV_DObjCalcSkel(struct gentity_s *ent, int a)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);

    DObj_CalcSkel(obj, (const uint32 *)a);
}

/* 0x08092188  size=35  FUN_08092188  status=translated  naming=unknown */
/* naming: no string; unconditional forward DObj_GetNumBones(obj). */
void SV_DObjGetNumBones(int *ref)
{
    DObj *obj = Com_GetServerDObj(*ref);

    DObj_GetNumBones(obj);
}

/* 0x080921ac  size=63  FUN_080921ac  status=translated  naming=unknown */
/* naming: no string; returns DObj_GetBoneIndexByName(obj, a), or -1 when the handle does not resolve. */
int SV_DObjGetBoneIndexByName(struct gentity_s *ent, int a)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);
    int result;

    if (obj == 0) {
        result = -1;
    } else {
        result = DObj_GetBoneIndexByName(obj, a);
    }
    return result;
}

/* 0x080921ec  size=35  FUN_080921ec  status=translated  naming=unknown */
/* naming: no string; returns DObj_GetRotTransArray(obj) (tail value in eax; base address of the bone array). */
int ScrEntity_GetRotTransArray(struct gentity_s *ent)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);

    return (int)(size_t)DObj_GetRotTransArray(obj);
}

/* 0x08092210  size=43  FUN_08092210  status=translated  naming=unknown */
/* naming: no string; calls XAnim_DumpTree((XAnimBinding *)obj) when the handle resolves. */
void SV_DObjDumpAnimTree(int *ref)
{
    DObj *obj = Com_GetServerDObj(*ref);

    if (obj != 0) {
        XAnim_DumpTree((XAnimBinding *)obj);
    }
}

/* 0x080922c4  size=56  FUN_080922c4  status=translated  naming=unknown */
/* naming: no string; returns DObj_GetTree(obj), or 0 when the handle does not resolve. */
int SV_DObjGetTree(struct gentity_s *ent)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);
    int result;

    if (obj == 0) {
        result = 0;
    } else {
        result = (int)DObj_GetTree(obj);
    }
    return result;
}

/* 0x080922fc  size=5  FUN_080922fc  status=translated  naming=unknown */
/* naming: empty body (ret only); no string or caller evidence beyond the dispatch table. */
void ScrEntity_Nop(void)
{
}

/* 0x08092302  size=89  FUN_08092302  status=translated  naming=inferred */
/* naming: format string "maps/mp/%s.%s" in this function names the map file path; SV_MapFileExists is the descriptive name. */
int SV_MapFileExists(const char *mapName)
{
    int mapArg = (int)mapName;
    char *mapname = SV_MapNameFromPath((char *)mapArg);
    const char *ext = Gfx_GetBspVariant();
    char *path = va("maps/mp/%s.%s", mapname, ext);
    int found = FS_ReadFile(path, 0);

    /* ~found >> 31 is 1 exactly when found is non-negative. */
    return found >= 0 ? 1 : 0;
}

/* 0x0809235c  size=18  FUN_0809235c  status=translated  naming=unknown */
/* naming: no string; stores CM_EntityString() into the token cursor g_sv_entityParsePoint. */
void SV_BeginEntityParse(void)
{
    g_sv_entityParsePoint = (char *)CM_EntityString();
}

/* 0x0809236e  size=40  FUN_0809236e  status=translated  naming=unknown */
/* naming: no string; reports whether the script-object handle resolves. */
int SV_DObjExists(struct gentity_s *ent)
{
    int *ref = (int *)ent;
    DObj *obj = Com_GetServerDObj(*ref);

    return obj != 0;
}

/* 0x08092396  size=20  FUN_08092396  status=translated  naming=unknown */
/* naming: no string; calls Com_SetPendingHandle(1). Flag meaning not recovered. */
void SV_SetPendingHandleOne(void)
{
    Com_SetPendingHandle(1);
}

/* 0x080923aa  size=20  FUN_080923aa  status=translated  naming=unknown */
/* naming: no string; calls Com_ClearPendingHandle(1). Flag meaning not recovered. */
void SV_ClearPendingHandleOne(void)
{
    Com_ClearPendingHandle(1);
}

/* 0x080923be  size=84  FUN_080923be  status=translated  naming=unknown */
/* naming: no string; copies the next token from the cursor and reports whether the cursor is still active. */
int SV_GetEntityToken(char *dst, int size)
{
    char *token = Com_Parse((const char **)&g_sv_entityParsePoint);
    int result = 0;

    Q_strncpyz(dst, token, size);
    if (g_sv_entityParsePoint != 0 || *token != '\0') {
        result = 1;
    }
    return result;
}

/* 0x08092412  size=68  FUN_08092412  status=translated  naming=unknown */
/* naming: no string; per-client int at +0xae704, bounded by sv_maxclients. */
int SV_GetClientGuid(int clientNum)
{
    int result;

    if (clientNum < 0 || clientNum >= cv_sv_maxclients->current.integer) {
        result = 0;
    } else {
        result = svs.clients[clientNum].guid;
    }
    return result;
}

/* 0x08092456  size=28  FUN_08092456  status=translated  naming=inferred */
/* naming: the field is printed as "ping=%d" in SV_PB_GetClientScoreText (0x08095f64); no bounds check here. */
int SV_GetClientPing(int clientNum)
{
    return svs.clients[clientNum].ping;
}

/* 0x08092472  size=67  FUN_08092472  status=translated  naming=unknown */
/* naming: no string; passes the client's remote address to NET_IsLocalAddress and returns its result (tail call, eax untouched at 0x080924ae). */
int SV_IsClientAddressLocal(int clientNum)
{
    return NET_IsLocalAddress(svs.clients[clientNum].netchan.remoteAddress);
}

/* 0x080924b6  size=68  FUN_080924b6  status=translated  naming=inferred */
/* naming: clears sv.state and the demo-related flag; inferred from the server-state reset shape. */
void SV_ClearServerState(void)
{
    sv.state = 0;
    Com_UnloadSoundAliases(2);
    if (g_083e2f80 != 0) {
        G_ShutdownGame(1);
        SV_ClearPendingHandleOne();
        g_083e2f80 = 0;
    }
}

/* 0x080924fa  size=236  FUN_080924fa  status=translated  naming=inferred */
/* naming: string "g_gametype %s is not a valid gametype, defaulting to dm" names the validation; SV_ValidateGametype is inferred. */
void SV_ValidateGametype(void)
{
    char gametype[76];
    char *p;

    Dvar_RegisterString("g_gametype", "dm", 0x1024);
    if (cv_081a218c->current.enabled != 0 && Level_GetIntValue() != 0) {
        Q_strncpyz(gametype, sv_gametypeName, 0x40);
    } else {
        Q_strncpyz(gametype, cv_g_gametype->current.string, 0x40);
    }
    for (p = gametype; *p != '\0'; p++) {
        *p = (char)tolower((int)*p);
    }
    if (G_IsGametypeListed(gametype) == 0) {
        Com_Printf("g_gametype %s is not a valid gametype, defaulting to dm\n", gametype);
        strcpy(gametype, "dm");
    }
    Dvar_SetString(cv_g_gametype, gametype);
}

/* 0x080925e6  size=151  FUN_080925e6  status=translated  naming=inferred */
/* naming: no string; resets per-map client state and runs the map-load hooks; SV_InitMapState is inferred. */
void SV_InitMapState(int spawnFlag, int param)
{
    int i;
    int v;

    FX_ResetBlockers();
    g_sv_entityParsePoint = (char *)CM_EntityString();
    Sys_NoOp_080d4372();
    v = Sys_MillisecondsRaw();
    G_InitGame(svs.time, v, spawnFlag, param);
    Sys_NoOp_080d4372();
    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        svs.clients[i].gentity = 0;
    }
    if (cv_084a8780->current.integer != 0) {
        Dvar_Dump(4);
    }
}

/* 0x0809267e  size=49  FUN_0809267e  status=translated  naming=unknown */
/* naming: no string; calls G_ShutdownGame(0) then SV_InitMapState(1, param). */
void SV_RestartGameProgs(int param)
{
    G_ShutdownGame(0);
    g_081a21c4 = 0;
    SV_InitMapState(1, param);
}

/* 0x080926b0  size=37  FUN_080926b0  status=translated  naming=unknown */
/* naming: no string; sets g_083e2f80 then SV_InitMapState(0, param). */
void SV_InitGameProgs(int param)
{
    g_083e2f80 = 1;
    SV_InitMapState(0, param);
}

/* 0x080926d6  size=37  FUN_080926d6  status=translated  naming=inferred */
/* naming: returns G_ConsoleCommand() only while sv.state is SS_GAME (2). */
int SV_GameConsoleCommand(void)
{
    int result;

    if (sv.state == 2) {
        result = G_ConsoleCommand();
    } else {
        result = 0;
    }
    return result;
}

/* 0x080926fc  size=44  FUN_080926fc  status=translated  naming=unknown */
/* naming: no string; two-component float add. */
void Vec2_Add_080926fc(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
}

/* 0x08092728  size=37  FUN_08092728  status=translated  naming=unknown */
/* naming: no string; two-component float scale (scalar first in the argument list). */
void Vec2_Scale_08092728(const float *a, float s, float *out)
{
    out[0] = s * a[0];
    out[1] = s * a[1];
}

/* 0x0809274e  size=47  FUN_0809274e  status=translated  naming=unknown */
/* naming: no string; copies three dwords (a vec3 bit pattern). */
void Vec3_CopyBits_0809274e(int *src, int *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* 0x08092780  size=529  FUN_08092780  status=translated  naming=observed */
/* naming: "SV_SetConfigstring: bad index %i" in the same binary names the function (string at 0x0814adc0). */
void SV_SetConfigstring(int index, const char *value)
{
    char chunk_buf[1027];
    size_t valueLen;
    int headerLen;
    int chunkMax;
    int i;

    if (index < 0 || index > 0x7ff) {
        Com_Error(1, "\x15SV_SetConfigstring: bad index %i\n", index);
    }
    if (value == 0) {
        value = "";
    }
    if (sv.configstrings[index] != 0 && strcmp(value, sv.configstrings[index]) != 0) {
        Com_CopyStringInto(&sv.configstrings[index], value);
        if (SV_IsGameRunning() != 0 || sv.restarting != 0) {
            valueLen = strlen(value);
            sprintf(chunk_buf, "%i", index);
            headerLen = (int)strlen(chunk_buf) + 4;
            chunkMax = 0x400 - headerLen;
            for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
                struct client_s *cl = &svs.clients[i];

                if (cl->state > 2) {
                    if (chunkMax < (int)valueLen) {
                        int offset = 0;
                        int remaining;

                        for (remaining = (int)valueLen; remaining > 0; remaining -= chunkMax) {
                            char mark;

                            if (offset == 0) {
                                mark = 'x';
                            } else if (chunkMax < remaining) {
                                mark = 'y';
                            } else {
                                mark = 'z';
                            }
                            Q_strncpyz(chunk_buf, value + offset, chunkMax + 1);
                            SV_SendServerCommand(cl, 1, "%c %i %s", (int)mark, index, chunk_buf);
                            offset += chunkMax;
                        }
                    } else {
                        SV_SendServerCommand(cl, 1, "%c %i %s", 100, index, value);
                    }
                }
            }
        }
    }
}

/* 0x08092992  size=138  FUN_08092992  status=translated  naming=observed */
/* naming: "SV_GetConfigstring: bad index %i" in the same binary names the function (string at 0x0814ae40). */
void SV_GetConfigstring(int index, char *buffer, int bufferSize)
{
    if (bufferSize < 1) {
        Com_Error(1, "\x15SV_GetConfigstring: bufferSize == %i", bufferSize);
    }
    if (index < 0 || index > 0x7ff) {
        Com_Error(1, "\x15SV_GetConfigstring: bad index %i\n", index);
    }
    if (sv.configstrings[index] == 0) {
        buffer[0] = '\0';
    } else {
        Q_strncpyz(buffer, sv.configstrings[index], bufferSize);
    }
}

/* 0x08092a1c  size=44  FUN_08092a1c  status=translated  naming=inferred */
/* naming: no string; configstring lookup that maps an empty slot to "". */
char *SV_GetConfigstringOrEmpty(int index)
{
    char *value = sv.configstrings[index];

    if (sv.configstrings[index] == 0) {
        value = "";
    }
    return value;
}

/* 0x08092a48  size=118  FUN_08092a48  status=translated  naming=inferred */
/* naming: key/value pairs stored as (start+i) keys and (start+count+i) values; SV_SetConfigValueForKey (observed) is the writer. */
char *SV_FindConfigstringPairValue(int start, int count, char *key)
{
    int i = 0;

    for (;;) {
        if (count <= i) {
            return "";
        }
        if (*sv.configstrings[start + i] == '\0') {
            break;
        }
        if (strcasecmp(key, sv.configstrings[start + i]) == 0) {
            return sv.configstrings[start + count + i];
        }
        i++;
    }
    return "";
}

/* 0x08092abe  size=155  FUN_08092abe  status=translated  naming=observed */
/* naming: "SV_SetConfigValueForKey: overflow" (string at 0x0814ae80) names the function. */
void SV_SetConfigValueForKey(int start, int count, char *key, char *value)
{
    int i = 0;

    for (;;) {
        if (count <= i) {
            break;
        }
        if (*sv.configstrings[start + i] == '\0') {
            SV_SetConfigstring(start + i, key);
            break;
        }
        if (strcasecmp(key, sv.configstrings[start + i]) == 0) {
            break;
        }
        i++;
    }
    if (i == count) {
        Com_Error(1, "\x15SV_SetConfigValueForKey: overflow");
    }
    SV_SetConfigstring(start + count + i, value);
}

/* 0x08092b5a  size=170  FUN_08092b5a  status=translated  naming=observed */
/* naming: "SV_SetUserinfo: bad index %i" (string at 0x0814aec0) names the function. */
void SV_SetUserinfo(int clientNum, char *userinfo)
{
    struct client_s *cl;
    char *nameValue;

    if (clientNum < 0 || clientNum >= cv_sv_maxclients->current.integer) {
        Com_Error(1, "\x15SV_SetUserinfo: bad index %i\n", clientNum);
    }
    if (userinfo == 0) {
        userinfo = "";
    }
    cl = &svs.clients[clientNum];
    Q_strncpyz(cl->userinfo, userinfo, 0x400);
    nameValue = Info_ValueForKey(userinfo, "name");
    Q_strncpyz(cl->name, nameValue, 0x20);
}

/* 0x08092c04  size=130  FUN_08092c04  status=translated  naming=observed */
/* naming: "SV_GetUserinfo: bad index %i" (string at 0x0814af40) names the function. */
void SV_GetUserinfo(int clientNum, char *buffer, int bufferSize)
{
    if (bufferSize < 1) {
        Com_Error(1, "\x15SV_GetUserinfo: bufferSize == %i", bufferSize);
    }
    if (clientNum < 0 || clientNum >= cv_sv_maxclients->current.integer) {
        Com_Error(1, "\x15SV_GetUserinfo: bad index %i\n", clientNum);
    }
    Q_strncpyz(buffer, svs.clients[clientNum].userinfo, bufferSize);
}

/* 0x08092c86  size=267  FUN_08092c86  status=translated  naming=inferred */
/* naming: no string; archives each linked entity into sv.archivedEntities for snapshot delta use. */
void SV_ArchiveLinkedEntities(void)
{
    int n;

    for (n = 1; n < g_numGEntities; n++) {
        struct gentity_s *ent = (struct gentity_s *)SV_GentityNum(n);

        if (ent->r.linked != 0) {
            archivedEntity_t *arch = &sv.svEntities[n].baseline;
            int *src = (int *)ent;
            int *dst = (int *)arch;
            int k;

            ent->s.number = n;
            for (k = 0; k < 0x3c; k++) {
                dst[k] = src[k];
            }
            arch->r.svFlags = (unsigned int)ent->r.svFlags;
            arch->r.clientMask[0] = ent->r.clientMask[0];
            arch->r.clientMask[1] = ent->r.clientMask[1];
            Vec3_CopyBits_080943ea((int *)ent->r.absmin, (int *)arch->r.absmin);
            Vec3_CopyBits_080943ea((int *)ent->r.absmax, (int *)arch->r.absmax);
        }
    }
}

/* 0x08092d92  size=103  FUN_08092d92  status=translated  naming=inferred */
/* naming: registers sv_maxclients (string at 0x0814b5c0 region) and raises it to a minimum; SV_EnsureMaxClients is inferred. */
void SV_EnsureMaxClients(int minimum)
{
    cv_sv_maxclients = Dvar_RegisterInt("sv_maxclients", 0x14, 1, 0x40, 0x1025);
    Dvar_ClearModified(cv_sv_maxclients);
    if (cv_sv_maxclients->current.integer < minimum) {
        Dvar_SetInt(cv_sv_maxclients, minimum);
    }
}

/* 0x08092dfa  size=243  FUN_08092dfa  status=translated  naming=observed */
/* naming: "SV_Startup: svs.initialized" and "SV_Startup: unable to allocate svs.clients" (strings at 0x0814af6d and 0x0814afa0) name the function. */
void SV_Startup(void)
{
    if (svs.initialized != 0) {
        Com_Error(0, "\x15SV_Startup: svs.initialized");
    }
    SV_EnsureMaxClients(1);
    svs.clients = (struct client_s *)Com_Calloc(cv_sv_maxclients->current.integer * 0xb1064);
    if (svs.clients == 0) {
        Com_Error(0, "\x15SV_Startup: unable to allocate svs.clients");
    }
    if (cv_084a8780->current.integer == 0) {
        svs.numSnapshotEntities = cv_sv_maxclients->current.integer << 8;
        svs.numSnapshotClients = cv_sv_maxclients->current.integer * cv_sv_maxclients->current.integer * 4;
    } else {
        svs.numSnapshotEntities = cv_sv_maxclients->current.integer << 0xb;
        svs.numSnapshotClients = cv_sv_maxclients->current.integer * cv_sv_maxclients->current.integer * 0x20;
    }
    sv_lastAuthorizeServerTime = svs.time;
    svs.initialized = 1;
    Dvar_SetBool(cv_081a218c, 1);
}

/* 0x08092eee  size=697  FUN_08092eee  status=translated  naming=inferred */
/* naming: no string; grows svs.clients to the highest connected slot + 1 and migrates slot data; SV_ResizeClients is inferred. */
void SV_ResizeClients(void)
{
    int needed = 0;
    int i;
    int oldMax;
    struct client_s *tmp;

    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (svs.clients[i].state > 1 && needed < i) {
            needed = i;
        }
    }
    needed = needed + 1;
    oldMax = cv_sv_maxclients->current.integer;
    SV_EnsureMaxClients(needed);
    if (cv_sv_maxclients->current.integer != oldMax) {
        tmp = (struct client_s *)Hunk_AllocateTempMemory(needed * 0xb1064);
        for (i = 0; i < needed; i++) {
            if (svs.clients[i].state < 2) {
                memset(&tmp[i], 0, 0xb1064);
            } else {
                memcpy(&tmp[i], &svs.clients[i], 0xb1064);
            }
        }
        Com_Free_B(svs.clients);
        svs.clients = (struct client_s *)Com_Calloc(cv_sv_maxclients->current.integer * 0xb1064);
        if (svs.clients == 0) {
            Com_Error(0, "\x15SV_Startup: unable to allocate svs.clients");
        }
        Com_Memset(svs.clients, 0, cv_sv_maxclients->current.integer * 0xb1064);
        for (i = 0; i < needed; i++) {
            if (tmp[i].state > 1) {
                memcpy(&svs.clients[i], &tmp[i], 0xb1064);
            }
        }
        Hunk_FreeTempMemory((int *)tmp);
        if (cv_084a8780->current.integer == 0) {
            svs.numSnapshotEntities = cv_sv_maxclients->current.integer << 8;
            svs.numSnapshotClients = cv_sv_maxclients->current.integer * cv_sv_maxclients->current.integer * 4;
        } else {
            svs.numSnapshotEntities = cv_sv_maxclients->current.integer << 0xb;
            svs.numSnapshotClients = cv_sv_maxclients->current.integer * cv_sv_maxclients->current.integer * 0x20;
        }
    }
}

/* 0x080931a8  size=270  FUN_080931a8  status=translated  naming=inferred */
/* naming: reads "hunkusage.dat" and sets com_expectedHunkUsage from the token after the matching key; SV_LoadExpectedHunkUsage is inferred from the file name and cvar. */
void SV_LoadExpectedHunkUsage(char *key)
{
    int handle = 0;
    int len;
    char *buf;
    const char *cursor;
    char *tok;

    len = FSH_FOpenFile("hunkusage.dat", &handle, 0);
    if (len > -1) {
        buf = (char *)Com_MallocZeroed(len + 1);
        memset(buf, 0, len + 1);
        FS_Read(buf, len, handle);
        FS_FCloseFile(handle);
        cursor = buf;
        for (;;) {
            tok = Com_Parse(&cursor);
            if (tok == 0 || *tok == '\0') {
                Com_Free_A(buf);
                return;
            }
            if (Q_stricmp(tok, key) != 0) {
                continue;
            }
            tok = Com_Parse(&cursor);
            if (tok != 0 && *tok != '\0') {
                break;
            }
        }
        Dvar_SetInt(cv_com_expectedHunkUsage, atoi(tok));
        Com_Free_A(buf);
    }
}

/* 0x080932b6  size=92  FUN_080932b6  status=translated  naming=inferred */
/* naming: no string; frees all configstrings and clears the whole server block. */
void SV_ResetServerState(void)
{
    int i;

    for (i = 0; i < 0x800; i++) {
        if (sv.configstrings[i] != 0) {
            Com_Free_A(sv.configstrings[i]);
        }
    }
    Com_Memset(&sv, 0, 0x5f534);
}

/* 0x08093312  size=123  FUN_08093312  status=translated  naming=inferred */
/* naming: no string; allocates the five server buffers when the enable flag is set; SV_AllocServerPools is inferred. */
void SV_AllocServerPools(int enable)
{
    svs.archiveEnabled = enable;
    if (enable != 0 && svs.archivedSnapshotFrames == 0) {
        svs.cachedSnapshotEntities = (archivedEntity_t *)Com_MallocZeroed(0x450000);
        svs.cachedSnapshotClients = (cachedClient_t *)Com_MallocZeroed(0x2708000);
        svs.archivedSnapshotFrames = (archivedSnapshot_t *)Com_MallocZeroed(0x2580);
        svs.archivedSnapshotBuffer = (byte *)Com_MallocZeroed(0x2000000);
        svs.cachedSnapshotFrames = (cachedSnapshot_t *)Com_MallocZeroed(0x3800);
    }
}

/* 0x0809338e  size=65  FUN_0809338e  status=translated  naming=inferred */
/* naming: no string; zeroes the pool state words. */
void SV_ClearPoolState(void)
{
    svs.archiveEnabled = 0;
    svs.nextArchivedSnapshotFrames = 0;
    svs.nextArchivedSnapshotBuffer = 0;
    svs.nextCachedSnapshotEntities = 0;
    svs.nextCachedSnapshotClients = 0;
    svs.nextCachedSnapshotFrames = 0;
}

/* 0x080933d0  size=168  FUN_080933d0  status=translated  naming=inferred */
/* naming: no string; frees the five server buffers and clears their pointers. */
void SV_FreeServerPools(void)
{
    if (svs.cachedSnapshotEntities != 0) {
        Com_Free_A(svs.cachedSnapshotEntities);
        svs.cachedSnapshotEntities = 0;
    }
    if (svs.cachedSnapshotClients != 0) {
        Com_Free_A(svs.cachedSnapshotClients);
        svs.cachedSnapshotClients = 0;
    }
    if (svs.archivedSnapshotFrames != 0) {
        Com_Free_A(svs.archivedSnapshotFrames);
        svs.archivedSnapshotFrames = 0;
    }
    if (svs.archivedSnapshotBuffer != 0) {
        Com_Free_A(svs.archivedSnapshotBuffer);
        svs.archivedSnapshotBuffer = 0;
    }
    if (svs.cachedSnapshotFrames != 0) {
        Com_Free_A(svs.cachedSnapshotFrames);
        svs.cachedSnapshotFrames = 0;
    }
}

/* 0x08093478  size=13  FUN_08093478  status=translated  naming=unknown */
/* naming: no string; calls Dvar_ClearUnknownFlag0400(). */
void SV_ClearAllDvarFlag0400(void)
{
    Dvar_ClearUnknownFlag0400();
}

/* 0x08093486  size=154  FUN_08093486  status=translated  naming=inferred */
/* naming: writes configstrings 1 and 0 from temporary buffers under flag word dvar_modifiedFlags; SV_SetupMapConfigstrings is inferred. */
void SV_SetupMapConfigstrings(void)
{
    char buf[8200];
    char *src;

    src = Dvar_InfoString_Big(8);
    Q_strncpyz(buf, src, 0x2000);
    dvar_modifiedFlags = dvar_modifiedFlags & 0xfffffff7u;
    SV_SetConfigstring(1, buf);
    src = Dvar_InfoString(0x404);
    SV_SetConfigstring(0, src);
    dvar_modifiedFlags = dvar_modifiedFlags & 0xfffffbfbu;
    Dvar_ForEachWithFlags(0x8e, 0x60, 0x100);
    dvar_modifiedFlags = dvar_modifiedFlags & 0xfffffeffu;
}

/* 0x08093520  size=1398  FUN_08093520  status=translated  naming=observed */
/* naming: "------ Server Initialization ------" and "Server: %s" strings in the body name SV_SpawnServer. */
void SV_SpawnServer(char *mapname)
{
    char mapbuf[64];
    int out4;
    const char *text;
    int local1c;
    int i;
    unsigned int seedWord;
    unsigned char bVar1;

    GScr_LoadGametypeList();
    SV_ValidateGametype();
    if (cv_081a218c->current.enabled == 0) {
        local1c = 0;
    } else {
        struct client_s *cl;

        local1c = Level_GetIntValue();
        for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
            cl = &svs.clients[i];
            if (cl->state > 2) {
                Com_sprintf(mapbuf, 0x40, "loadingnewmap\n%s\n%s", mapname, cv_g_gametype->current.string);
                NET_OutOfBandPrint(1, cl->netchan.remoteAddress, mapbuf);
            }
        }
        NET_Sleep(0xfa);
    }
    Dvar_SetOrCreateString("mapname", mapname);
    SV_ClearServerState();
    Com_Printf("------ Server Initialization ------\n");
    Com_Printf("Server: %s\n", mapname);
    SV_ResetServerState();
    if (cv_084a8780->current.integer != 0) {
        FX_ShutdownSystem_Full();
    }
    ((void (*)(int))FS_Shutdown)(1); /* the original pushes 1; FS_Shutdown (0x080a2ba8) never reads it (shared prototype is void) */
    FS_ClearPakReferences();
    Com_Restart_Subsystems();
    if (cv_081a218c->current.enabled == 0) {
        SV_Startup();
    } else {
        SV_ResizeClients();
    }
    Q_strncpyz(sv_gametypeName, cv_g_gametype->current.string, 0x40);
    seedWord = (unsigned int)Sys_MillisecondsRaw();
    srand(seedWord);
    {
        int r1 = rand();
        int r2 = rand();
        unsigned int r3 = (unsigned int)Sys_Milliseconds();

        sv.checksumFeed = (int)(r3 ^ ((unsigned int)r1 << 16) ^ (unsigned int)r2);
    }
    FS_Restart(sv.checksumFeed);
    text = Gfx_GetBspVariant();
    Com_sprintf(mapbuf, 0x40, "maps/mp/%s.%s", mapname, text);
    SV_LoadExpectedHunkUsage(mapbuf);
    for (i = 0; i < 0x800; i++) {
        sv.configstrings[i] = CopyString("");
    }
    SV_ClearAllDvarFlag0400();
    svs.snapshotEntities = (entityState_t *)Hunk_Alloc(svs.numSnapshotEntities * 0xf0);
    svs.nextSnapshotEntities = 0;
    svs.snapshotClients = (clientState_t *)Hunk_Alloc(svs.numSnapshotClients * 0x5c);
    svs.nextSnapshotClients = 0;
    SV_ClearPoolState();
    svs.snapFlagServerBit = svs.snapFlagServerBit ^ 4;
    Dvar_SetString(cv_nextmap, "map_restart");
    Dvar_SetInt(cv_081a21a0, 0);
    text = Gfx_GetBspVariant();
    Com_sprintf(mapbuf, 0x40, "maps/mp/%s.%s", mapname, text);
    Com_LoadBsp(mapbuf);
    CM_LoadMap(mapbuf, &out4);
    Com_UnloadBsp();
    Area_ResetAndRelinkAll();
    bVar1 = (unsigned char)((unsigned char)g_sv_serverIdCounter + 0x10);
    g_sv_serverIdCounter = bVar1;
    if ((bVar1 & 0xf0) == 0) {
        g_sv_serverIdCounter = g_sv_serverIdCounter + 0x10;
    }
    Dvar_SetInt(cv_sv_serverid, (int)g_sv_serverIdCounter);
    sv.start_frameTime = com_frameTime;
    sv.state = 1;
    text = Gfx_GetBspVariant();
    Com_sprintf(mapbuf, 0x40, "maps/mp/%s.%s", mapname, text);
    Com_LoadSoundAliases(mapbuf, "all_mp", 2);
    SV_InitGameProgs(local1c);
    if (cv_084a8780->current.integer != 0) {
        FX_InitThunk(0);
        FX_LoadMissingEffect();
    }
    for (i = 0; i < 3; i++) {
        svs.time = svs.time + 100;
        SV_RunGameFrame();
    }
    SV_ArchiveLinkedEntities();
    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        struct client_s *cl = &svs.clients[i];

        if (cl->state > 1) {
            const char *reason = ClientConnect(i, (short)cl->scriptId);

            if (reason == 0) {
                cl->state = 2;
            } else {
                SV_DropClient(cl, reason);
            }
        }
    }
    if (cv_sv_pure->current.enabled == 0) {
        Dvar_SetString(cv_sv_iwds, "");
        Dvar_SetString(cv_sv_iwdNames, "");
    } else {
        char *iwds = FS_LoadedIwdChecksums();

        Dvar_SetString(cv_sv_iwds, iwds);
        if (*iwds == '\0') {
            Com_Printf("WARNING: sv_pure set but no IWD files loaded\n");
        }
        Dvar_SetString(cv_sv_iwdNames, FS_LoadedIwdNames());
    }
    Dvar_SetString(cv_sv_referencedIwds, FS_ReferencedIwdChecksums());
    Dvar_SetString(cv_sv_referencedIwdNames, FS_ReferencedIwdNames());
    SV_SetupMapConfigstrings();
    sv.state = 2;
    SV_Heartbeat_f();
    Com_Printf("-----------------------------------\n");
    if (Dvar_GetBool("sv_punkbuster") == 0) {
        PB_SendMapStartEventDisabled();
    } else {
        PB_SendMapStartEventEnabled();
    }
}

/* 0x08093a96  size=18  FUN_08093a96  status=translated  naming=inferred */
/* naming: no string; true while the server is in the game state (sv.state == 2). */
int SV_IsGameRunning(void)
{
    return sv.state == 2;
}

/* 0x08093aa8  size=51  FUN_08093aa8  status=translated  naming=inferred */
/* naming: sets sv_punkbuster from a string value ("0" disables); SV_ApplyPunkbusterCvar is inferred. */
void SV_ApplyPunkbusterCvar(char *value)
{
    int iVar1 = Q_stricmp(value, "0");

    Dvar_SetOrCreateBool("sv_punkbuster", iVar1 != 0);
}

/* 0x08093adc  size=1776  FUN_08093adc  status=translated  naming=inferred */
/* naming: registers the server cvars by their observed names (sv_maxclients, sv_hostname, ...); SV_AddServerCvars is inferred. */
void SV_AddServerCvars(void)
{
    SV_AddOperatorCommands();
    cv_g_gametype = Dvar_RegisterString("g_gametype", "dm", 0x1024);
    Dvar_RegisterString("sv_keywords", "", 0x1004);
    Dvar_RegisterInt("protocol", 0x76, 0x76, 0x76, 0x1044);
    cv_mapname = Dvar_RegisterString("mapname", "", 0x1044);
    cv_sv_privateClients = Dvar_RegisterInt("sv_privateClients", 0, 0, 0x40, 0x1004);
    cv_sv_maxclients = Dvar_RegisterInt("sv_maxclients", 0x14, 1, 0x40, 0x1025);
    cv_sv_hostname = Dvar_RegisterString("sv_hostname", "CoD2Host", 0x1005);
    cv_sv_punkbuster = Dvar_RegisterBool("sv_punkbuster", 0, 0x1015);
    cv_sv_maxRate = Dvar_RegisterInt("sv_maxRate", 0, 0, 25000, 0x1005);
    cv_sv_minPing = Dvar_RegisterInt("sv_minPing", 0, 0, 999, 0x1005);
    cv_sv_maxPing = Dvar_RegisterInt("sv_maxPing", 0, 0, 999, 0x1005);
    cv_sv_floodProtect = Dvar_RegisterBool("sv_floodProtect", 1, 0x1005);
    cv_sv_allowAnonymous = Dvar_RegisterBool("sv_allowAnonymous", 0, 0x1004);
    cv_sv_showCommands = Dvar_RegisterBool("sv_showCommands", 0, 0x1000);
    cv_sv_disableClientConsole = Dvar_RegisterBool("sv_disableClientConsole", 0, 0x1008);
    cv_sv_voice = Dvar_RegisterBool("sv_voice", 0, 0x100d);
    cv_sv_voiceQuality = Dvar_RegisterInt("sv_voiceQuality", 1, 0, 9, 0x1008);
    cv_sv_cheats = Dvar_RegisterBool("sv_cheats", 0, 0x1018);
    cv_sv_serverid = Dvar_RegisterInt("sv_serverid", 0, (int)0x80000000u, 0x7fffffff, 0x1048);
    cv_sv_pure = Dvar_RegisterBool("sv_pure", 1, 0x100c);
    cv_sv_iwds = Dvar_RegisterString("sv_iwds", "", 0x1048);
    cv_sv_iwdNames = Dvar_RegisterString("sv_iwdNames", "", 0x1048);
    cv_sv_referencedIwds = Dvar_RegisterString("sv_referencedIwds", "", 0x1048);
    cv_sv_referencedIwdNames = Dvar_RegisterString("sv_referencedIwdNames", "", 0x1048);
    cv_rcon_password = Dvar_RegisterString("rcon_password", "", 0x1000);
    cv_sv_privatePassword = Dvar_RegisterString("sv_privatePassword", "", 0x1000);
    cv_sv_fps = Dvar_RegisterInt("sv_fps", 0x14, 10, 1000, 0x1000);
    cv_sv_timeout = Dvar_RegisterInt("sv_timeout", 0xf0, 0, 0x708, 0x1000);
    cv_sv_zombietime = Dvar_RegisterInt("sv_zombietime", 2, 0, 0x708, 0x1000);
    cv_sv_allowDownload = Dvar_RegisterBool("sv_allowDownload", 1, 0x1001);
    cv_sv_reconnectlimit = Dvar_RegisterInt("sv_reconnectlimit", 3, 0, 0x708, 0x1001);
    cv_sv_padPackets = Dvar_RegisterInt("sv_padPackets", 0, 0, 0x7fffffff, 0x1000);
    ((int *)g_0817d838)[0xdd] = 0;
    cv_sv_allowedClan1 = Dvar_RegisterString("sv_allowedClan1", "", 0x1000);
    cv_sv_allowedClan2 = Dvar_RegisterString("sv_allowedClan2", "", 0x1000);
    cv_sv_packet_info = Dvar_RegisterBool("sv_packet_info", 0, 0x1000);
    cv_sv_showAverageBPS = Dvar_RegisterBool("sv_showAverageBPS", 0, 0x1000);
    cv_sv_kickBanTime = Dvar_RegisterFloat("sv_kickBanTime", 300.0f, 0.0f, 3600.0f, 0x1000);
    cv_sv_mapRotation = Dvar_RegisterString("sv_mapRotation", "", 0x1000);
    cv_sv_mapRotationCurrent = Dvar_RegisterString("sv_mapRotationCurrent", "", 0x1000);
    cv_sv_debugRate = Dvar_RegisterBool("sv_debugRate", 0, 0x1000);
    cv_sv_debugReliableCmds = Dvar_RegisterBool("sv_debugReliableCmds", 0, 0x1000);
    cv_nextmap = Dvar_RegisterString("nextmap", "", 0x1000);
    cv_com_expectedHunkUsage = Dvar_RegisterInt("com_expectedHunkUsage", 0, 0, 0x7fffffff, 0x1040);
    cv_sv_wwwDownload = Dvar_RegisterBool("sv_wwwDownload", 0, 0x1001);
    cv_sv_wwwBaseURL = Dvar_RegisterString("sv_wwwBaseURL", "", 0x1001);
    cv_sv_wwwDlDisconnected = Dvar_RegisterBool("sv_wwwDlDisconnected", 0, 0x1001);
}

/* 0x080941cc  size=217  FUN_080941cc  status=translated  naming=inferred */
/* naming: sends "e"/"w" disconnect commands with the reason to each connected client; SV_DropAllClients is inferred. */
void SV_DropAllClients(char *reason)
{
    int pass;
    int i;

    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
            struct client_s *cl = &svs.clients[i];

            if (cl->state > 1) {
                if (cl->netchan.remoteAddress.type != 2) {
                    SV_SendServerCommand(cl, 0, "%c \"%s\"", 0x65, reason);
                    SV_SendServerCommand(cl, 1, "%c \"%s\"", 0x77, reason);
                }
                cl->nextSnapshotTime = -1;
                SV_SendClientSnapshot(cl);
            }
        }
    }
}

/* 0x080942a6  size=82  FUN_080942a6  status=translated  naming=inferred */
/* naming: disconnects every connected client with "EXE_DISCONNECTED"; SV_DisconnectAllClients is inferred from that string. */
void SV_DisconnectAllClients(void)
{
    int i;

    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        struct client_s *cl = &svs.clients[i];

        if (cl->state > 1) {
            SV_DropClient(cl, "EXE_DISCONNECTED");
        }
    }
}

/* 0x080942f8  size=241  FUN_080942f8  status=translated  naming=observed */
/* naming: "----- Server Shutdown -----" in the body names SV_Shutdown. */
void SV_Shutdown(char *reason)
{
    int wasLoading;

    if (cv_081a218c != 0 && cv_081a218c->current.enabled != 0) {
        Com_Printf("----- Server Shutdown -----\n");
        wasLoading = (sv.state == 1);
        if (svs.clients != 0) {
            SV_DropAllClients(reason);
        }
        SV_ShutdownStub();
        SV_MasterShutdown();
        SV_ClearServerState();
        SV_DisconnectAllClients();
        SV_ResetServerState();
        if (svs.clients != 0) {
            SV_DropAllConnectedClients();
        }
        SV_FreeServerPools();
        memset(&svs, 0, 0x1d10c);
        if (cv_084a8780->current.integer != 0) {
            FX_ShutdownSystem_Full();
        }
        Dvar_SetBool(cv_081a218c, 0);
        Com_Printf("---------------------------\n");
        if (wasLoading) {
            Com_ResetDObjsInitialized();
            DObj_ClearGlobals();
            XAnim_ClearEndNotifyString();
            Scr_ClearInitState();
            g_gameContextPtr = 0;
        }
    }
}

/* 0x080943ea  size=47  FUN_080943ea  status=translated  naming=unknown */
/* naming: no string; copies three dwords (same shape as Vec3_CopyBits_0809274e). */
void Vec3_CopyBits_080943ea(int *src, int *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* 0x0809441c  size=142  FUN_0809441c  status=translated  naming=inferred */
/* naming: no string; escapes newlines as "\\n" and drops 0x14/0x15 control bytes into g_escapedServerTextBuf; SV_EscapeServerText is inferred. */
char *SV_EscapeServerText(char *text)
{
    unsigned int n = 0;

    for (;;) {
        if (*text == '\0' || n > 0x3fc) {
            g_escapedServerTextBuf[n] = '\0';
            return g_escapedServerTextBuf;
        }
        if (*text == '\n') {
            g_escapedServerTextBuf[n] = '\\';
            g_escapedServerTextBuf[n + 1] = 'n';
            n += 2;
            text++;
            continue;
        }
        if (*text != 0x14 && *text != 0x15) {
            g_escapedServerTextBuf[n] = *text;
            n++;
        }
        text++;
    }
}

/* 0x080944aa  size=134  FUN_080944aa  status=translated  naming=inferred */
/* naming: no string; compares two space-delimited tokens for equality. */
int SV_TokenMatch(char *a, char *b)
{
    while (*a != '\0' && *b != '\0' && *a != ' ' && *b != ' ') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    if ((*a == '\0' || *a == ' ') && (*b == '\0' || *b == ' ')) {
        return 1;
    }
    return 0;
}

/* 0x08094530  size=304  FUN_08094530  status=translated  naming=inferred */
/* naming: no string; searches the reliable-command ring for a matching command after the cursor; SV_FindReliableCommand is inferred. */
int SV_FindReliableCommand(struct client_s *cl, char *text)
{
    unsigned int seq = (unsigned int)cl->reliableSent;

    for (;;) {
        unsigned int slot;
        struct reliableCommand_s *c;

        seq = seq + 1;
        if (cl->reliableSequence < (int)seq) {
            return -1;
        }
        slot = seq & 0x7f;
        c = &cl->reliableCommands[slot];
        if (c->cmdType != 0 && text[0] == c->command[0] && (unsigned char)(text[0] + 0x88) > 2) {
            if (strcmp(text + 1, c->command + 1) == 0) {
                return (int)seq;
            }
            switch (text[0]) {
            case 'C':
            case 'D':
            case 'a':
            case 'b':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 't':
                return (int)seq;
            case 'd':
            case 'v':
                if (SV_TokenMatch(text + 2, c->command + 2) != 0) {
                    return (int)seq;
                }
                break;
            default:
                break;
            }
        }
    }
}

/* 0x08094660  size=240  FUN_08094660  status=translated  naming=inferred */
/* naming: no string; compacts acknowledged reliable commands in front of the cursor; SV_CompactReliableCommands is inferred. */
void SV_CompactReliableCommands(struct client_s *cl)
{
    unsigned int dst = (unsigned int)cl->reliableSent + 1;
    unsigned int src;

    for (src = dst; (int)src <= cl->reliableSequence; src++) {
        unsigned int slot = src & 0x7f;

        if (cl->reliableCommands[slot].cmdType != 0) {
            if ((dst & 0x7f) != slot) {
                memmove(&cl->reliableCommands[dst & 0x7f], &cl->reliableCommands[slot], sizeof(struct reliableCommand_s));
            }
            dst = dst + 1;
        }
    }
    cl->reliableSequence = (int)(dst - 1);
}

/* 0x08094750  size=703  FUN_08094750  status=translated  naming=inferred */
/* naming: its overflow path prints "===== pending server commands =====" and "EXE_SERVERCOMMANDOVERFLOW", so the queueing role is named from that; SV_QueueReliableCommand is inferred. */
void SV_QueueReliableCommand(struct client_s *cl, int flags, char *text)
{
    if (cl->bIsTestClient == 0) {
        int ok;

        if (cl->reliableSequence - cl->reliableAcknowledge < 0x40 && cl->state == 4) {
            ok = 1;
        } else {
            SV_CompactReliableCommands(cl);
            ok = (flags != 0);
        }
        if (ok) {
            int found = SV_FindReliableCommand(cl, text);
            unsigned int dstSeq;
            unsigned int srcSeq;
            unsigned int slot;

            if (found < 0) {
                cl->reliableSequence = cl->reliableSequence + 1;
            } else {
                dstSeq = (unsigned int)found;
                srcSeq = (unsigned int)found;
                while (srcSeq = srcSeq + 1, (int)srcSeq <= cl->reliableSequence) {
                    memmove(&cl->reliableCommands[dstSeq & 0x7f], &cl->reliableCommands[srcSeq & 0x7f],
                            sizeof(struct reliableCommand_s));
                    dstSeq = dstSeq + 1;
                }
            }
            if (cl->reliableSequence - cl->reliableAcknowledge == 0x81) {
                unsigned int ackSeq;

                Com_Printf("===== pending server commands =====\n");
                ackSeq = (unsigned int)cl->reliableAcknowledge;
                while (ackSeq = ackSeq + 1, (int)ackSeq <= cl->reliableSequence) {
                    Com_Printf("cmd %5d: %8d: %s\n", (int)ackSeq,
                                cl->reliableCommands[ackSeq & 0x7f].cmdTime,
                                cl->reliableCommands[ackSeq & 0x7f].command);
                }
                Com_Printf("cmd %5d: %8d: %s\n", (int)ackSeq, svs.time, text);
                NET_OutOfBandPrint(1, cl->netchan.remoteAddress, "disconnect");
                SV_SetPendingDropReason(cl, "EXE_SERVERCOMMANDOVERFLOW");
                flags = 1;
                text = va("%c \"EXE_SERVERCOMMANDOVERFLOW\"", 0x77);
            }
            slot = (unsigned int)cl->reliableSequence & 0x7f;
            CopyReliableCommand(text, cl->reliableCommands[slot].command, 0x400);
            cl->reliableCommands[slot].cmdTime = svs.time;
            cl->reliableCommands[slot].cmdType = flags;
        }
    }
}

/* 0x08094a10  size=277  FUN_08094a10  status=translated  naming=inferred */
/* naming: formats a server command and queues it to one client, or to all connected clients when cl is NULL (broadcast); SV_SendServerCommand is inferred. */
void SV_SendServerCommand(struct client_s *cl, int flags, char *fmt, ...)
{
    int scratch[7]; /* 28-byte pool-offset save area */
    char *text;
    va_list ap;

    Com_PoolAllocInto2(scratch, 0x20000);
    text = (char *)Com_PoolPtrFromOffset(scratch);
    va_start(ap, fmt);
    vsnprintf(text, 0x20000, fmt, ap);
    va_end(ap);
    if (cl == 0) {
        if (cv_084a8780->current.integer != 0 && strncmp(text, "print", 5) == 0) {
            char *escaped = SV_EscapeServerText(text);

            Com_Printf("broadcast: %s\n", escaped);
        }
        {
            int i;

            for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
                if (svs.clients[i].state > 2) {
                    SV_QueueReliableCommand(&svs.clients[i], flags, text);
                }
            }
        }
        Com_PoolRestoreFrom2(scratch);
    } else {
        SV_QueueReliableCommand(cl, flags, text);
        Com_PoolRestoreFrom2(scratch);
    }
}

/* 0x08094b56  size=302  FUN_08094b56  status=translated  naming=observed */
/* naming: "SV_VoiceConnectionlessPacket: fixing up a translated port" (string in this function) names the function. */
void SV_VoiceConnectionlessPacket(struct netadr_s from, struct msg_s *msg)
{
    int qport = MSG_ReadShort(msg);
    int i;

    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        struct client_s *cl = &svs.clients[i];

        if (cl->state != 0 && NET_CompareBaseAdr(from, cl->netchan.remoteAddress) != 0 && cl->netchan.qport == qport) {
            if ((short)cl->netchan.remoteAddress.port != (short)from.port) {
                Com_Printf("SV_VoiceConnectionlessPacket: fixing up a translated port\n");
                cl->netchan.remoteAddress.port = (unsigned short)(short)from.port;
            }
            if (cl->state != 1) {
                cl->lastPacketTime = svs.time;
                if (cl->state < 4) {
                    SV_ParseVoicePackets(cl, msg);
                } else {
                    SV_ParseVoicePacketsLocal(cl, msg);
                }
            }
        }
    }
}

/* 0x08094c84  size=1030  FUN_08094c84  status=translated  naming=observed */
/* naming: "statusResponse\n%s\n%s" (string in the body) names the function as the getstatus reply. */
void SV_StatusResponse(struct netadr_s from)
{
    char info[8200];
    int poolSave[4]; /* pool offset save area (ebp-0x418 in the original) */
    char *players;   /* pool block returned by Com_PoolPtrFromOffset (ebp-0x41c) */
    char keyBuf[1036];
    char response[8192];
    int pos = 0;
    int i;
    int needPassword = 0;
    const char *fsGame;
    const char *refNames;
    const char *value;

    Com_PoolAllocInto2(poolSave, 0x20000);
    players = (char *)Com_PoolPtrFromOffset(poolSave);
    needPassword = 0;
    strcpy(info, Dvar_InfoString(0x404));
    Info_SetValueForKey(info, "challenge", Cmd_Argv_alt(1));
    if (Dvar_GetBool("fs_restrict") != 0) {
        value = Info_ValueForKey(info, "sv_keywords");
        Com_sprintf(keyBuf, 0x400, "demo %s", value);
        Info_SetValueForKey(info, "sv_keywords", keyBuf);
    }
    *players = '\0';
    pos = 0;
    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        struct client_s *cl = &svs.clients[i];

        if (cl->state > 1) {
            int score = 0;
            size_t len;
            char line[1028];

            (void)SV_GameClientNum(i);
            if (g_083e2f80 != 0) {
                score = G_GetClientScore((int)(((unsigned int)((char *)cl - (char *)svs.clients) >> 2) * 0x2165829u));
            }
            Com_sprintf(line, 0x400, "%i %i \"%s\"\n", score, cl->ping, cl->name);
            len = strlen(line);
            if (0x1ffffu < len + (size_t)pos) {
                break;
            }
            strcpy(players + pos, line);
            pos += (int)len;
        }
    }
    value = Dvar_GetString("g_password");
    if (value == 0 || *value == '\0') {
        Info_SetValueForKey(info, "pswrd", "0");
    } else {
        Info_SetValueForKey(info, "pswrd", "1");
    }
    fsGame = Dvar_GetString("fs_game");
    if (cv_sv_pure->current.enabled == 0 || (fsGame != 0 && *fsGame != '\0')) {
        needPassword = 1;
    } else {
        refNames = Dvar_GetString("sv_referencedIwdNames");
        if (*refNames != '\0') {
            int argc;

            Cmd_TokenizeString_alt((char *)refNames);
            argc = Cmd_Argc_alt();
            for (i = 0; i < argc; i++) {
                const char *arg = Cmd_Argv_alt(i);

                if (FS_iwIwd((char *)arg, "main") == 0) {
                    needPassword = 1;
                    break;
                }
            }
        }
    }
    Info_SetValueForKey(info, "mod", va("%i", needPassword));
    Com_sprintf(response, 0x2000, "statusResponse\n%s\n%s", info, players);
    NET_OutOfBandPrint(1, from, response);
    Com_PoolRestoreFrom2(poolSave);
}

/* 0x080950d0  size=615  FUN_080950d0  status=translated  naming=observed */
/* naming: "gameCompleteStatus\n%s\n%s" (string in the body) names the function. */
void SV_GameCompleteStatus(struct netadr_s from)
{
    char info[1032];
    char keyBuf[1024];
    int poolSave[4]; /* pool offset save area (ebp-0x418 in the original) */
    char *players;   /* pool block returned by Com_PoolPtrFromOffset (ebp-0x41c) */
    char line[1028];
    int pos = 0;
    int i;
    const char *value;

    Com_PoolAllocInto2(poolSave, 0x20000);
    players = (char *)Com_PoolPtrFromOffset(poolSave);
    strcpy(info, Dvar_InfoString(0x404));
    Info_SetValueForKey(info, "challenge", Cmd_Argv_alt(1));
    if (Dvar_GetBool("fs_restrict") != 0) {
        value = Info_ValueForKey(info, "sv_keywords");
        Com_sprintf(keyBuf, 0x400, "demo %s", value);
        Info_SetValueForKey(info, "sv_keywords", keyBuf);
    }
    *players = '\0';
    pos = 0;
    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        struct client_s *cl = &svs.clients[i];

        if (cl->state > 1) {
            size_t len;

            (void)SV_GameClientNum(i);
            Com_sprintf(line, 0x400, "%i %i \"%s\"\n",
                        G_GetClientScore((int)(((unsigned int)((char *)cl - (char *)svs.clients) >> 2) * 0x2165829u)),
                        cl->ping, cl->name);
            len = strlen(line);
            if (0x1ffffu < len + (size_t)pos) {
                break;
            }
            strcpy(players + pos, line);
            pos += (int)len;
        }
    }
    value = va("gameCompleteStatus\n%s\n%s", info, players);
    NET_OutOfBandPrint(1, from, value);
    Com_PoolRestoreFrom2(poolSave);
}

/* 0x0809537c  size=1489  FUN_0809537c  status=translated  naming=observed */
/* naming: "infoResponse\n" (string in the body) names the function as the getinfo reply. */
void SV_InfoResponse(struct netadr_s from)
{
    char info[1036];
    char response[1024];
    int needPassword = 0;
    int privateConnected = 0;
    int connected = 0;
    int i;
    int maxOpen;
    const char *value;
    const char *fsGame;

    info[0] = '\0';
    for (i = 0; i < cv_sv_privateClients->current.integer; i++) {
        if (svs.clients[i].state > 1) {
            privateConnected++;
        }
    }
    connected = privateConnected;
    for (i = cv_sv_privateClients->current.integer; i < cv_sv_maxclients->current.integer; i++) {
        if (svs.clients[i].state > 1) {
            connected++;
        }
    }
    Info_SetValueForKey(info, "challenge", Cmd_Argv_alt(1));
    Info_SetValueForKey(info, "protocol", va("%i", 0x76));
    Info_SetValueForKey(info, "hostname", cv_sv_hostname->current.string);
    Info_SetValueForKey(info, "mapname", cv_mapname->current.string);
    if (connected != 0) {
        Info_SetValueForKey(info, "clients", va("%i", connected));
    }
    maxOpen = cv_sv_maxclients->current.integer - (cv_sv_privateClients->current.integer - privateConnected);
    if (maxOpen > 0) {
        Info_SetValueForKey(info, "sv_maxclients", va("%i", maxOpen));
    }
    Info_SetValueForKey(info, "gametype", cv_g_gametype->current.string);
    if (cv_sv_pure->current.enabled != 0 || fs_numServerIwds != 0) {
        Info_SetValueForKey(info, "pure", "1");
    }
    if (cv_sv_minPing->current.integer != 0) {
        Info_SetValueForKey(info, "minPing", va("%i", cv_sv_minPing->current.integer));
    }
    if (cv_sv_maxPing->current.integer != 0) {
        Info_SetValueForKey(info, "maxPing", va("%i", cv_sv_maxPing->current.integer));
    }
    fsGame = Dvar_GetString("fs_game");
    if (*fsGame != '\0') {
        Info_SetValueForKey(info, "game", fsGame);
    }
    if (cv_sv_allowAnonymous->current.enabled != 0) {
        Info_SetValueForKey(info, "sv_allowAnonymous", va("%i", (int)cv_sv_allowAnonymous->current.enabled));
    }
    if (cv_sv_disableClientConsole->current.enabled != 0) {
        Info_SetValueForKey(info, "con_disabled", va("%i", (int)cv_sv_disableClientConsole->current.enabled));
    }
    value = Dvar_GetString("g_password");
    if (value != 0 && *value != '\0') {
        Info_SetValueForKey(info, "pswrd", "1");
    }
    {
        int friendlyFire = Dvar_GetInt("scr_friendlyfire");
        int killcam;

        if (friendlyFire != 0) {
            Info_SetValueForKey(info, "ff", va("%i", friendlyFire));
        }
        killcam = Dvar_GetInt("scr_killcam");
        if (killcam != 0) {
            Info_SetValueForKey(info, "kc", va("%i", killcam));
        }
    }
    Info_SetValueForKey(info, "hw", va("%i", 1));
    if (cv_sv_pure->current.enabled == 0 || (fsGame != 0 && *fsGame != '\0')) {
        needPassword = 1;
    } else {
        const char *refNames = Dvar_GetString("sv_referencedIwdNames");

        if (*refNames != '\0') {
            int argc;

            Cmd_TokenizeString_alt((char *)refNames);
            argc = Cmd_Argc_alt();
            for (i = 0; i < argc; i++) {
                const char *arg = Cmd_Argv_alt(i);

                if (FS_iwIwd((char *)arg, "main") == 0) {
                    needPassword = 1;
                    break;
                }
            }
        }
    }
    Info_SetValueForKey(info, "mod", va("%i", needPassword));
    Info_SetValueForKey(info, "voice", va("%i", (int)cv_sv_voice->current.enabled));
    Info_SetValueForKey(info, "pb", va("%i", (int)cv_sv_punkbuster->current.enabled));
    Q_strncpyz(response, "infoResponse\n", 0x400);
    I_strncat(response, 0x400, info);
    NET_OutOfBandPrint(1, from, response);
}

/* 0x0809594e  size=1259  FUN_0809594e  status=translated  naming=observed */
/* naming: "SV packet %s : %s" in the body and the connectionless command names (getstatus, getinfo, connect, rcon) identify it. */
void SV_ConnectionlessPacket(struct netadr_s from, struct msg_s *msg)
{
    int clientIndex = -1;
    int i;
    char *cmd;
    const char *argv0;

    MSG_BeginReading(msg);
    MSG_ReadLong(msg);
    SV_NetProfile_RecordOOB(msg->cursize);
    if (Q_stricmpn((char *)msg->data + 4, "pb_", 3) == 0) {
        for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
            struct client_s *cl = &svs.clients[i];

            if (cl->state != 0 && NET_CompareBaseAdr(from, cl->netchan.remoteAddress) != 0
                && (short)cl->netchan.remoteAddress.port == (short)from.port) {
                clientIndex = i;
                break;
            }
        }
        if (msg->data[7] != 'C' && msg->data[7] != '1' && msg->data[7] != 'J') {
            PB_DispatchEvent3(0xd, clientIndex, msg->cursize - 4, (int)(msg->data + 4));
        }
    } else {
        cmd = MSG_ReadStringLine(msg);
        Cmd_TokenizeString_alt(cmd);
        argv0 = Cmd_Argv_alt(0);
        if (cv_sv_packet_info->current.enabled != 0) {
            char *addr = NET_AdrToString(from);

            Com_Printf("SV packet %s : %s\n", addr, argv0);
        }
        if (strcmp(argv0, "getstatus") == 0) {
            SV_NoteAuthorizeServerPacket(from);
            SV_StatusResponse(from);
        } else if (strcmp(argv0, "v") == 0) {
            SV_VoiceConnectionlessPacket(from, msg);
        } else if (strcmp(argv0, "getinfo") == 0) {
            SV_NoteAuthorizeServerPacket(from);
            SV_InfoResponse(from);
        } else if (strcmp(argv0, "getchallenge") == 0) {
            SV_NoteAuthorizeServerPacket(from);
            SV_RequestIpAuthorize(from);
        } else if (strcmp(argv0, "connect") == 0) {
            char *who;

            if (NET_IsLocalAddress(from) == 0) {
                who = NET_AdrToString(from);
            } else {
                who = "localhost";
            }
            PB_OnConnectPacket((int)who, (int)msg->data);
            SV_DirectConnect(from);
        } else if (strcmp(argv0, "ipAuthorize") == 0) {
            SV_NoteAuthorizeServerPacket(from);
            SV_AuthorizeIpPacket(from);
        } else if (strcmp(argv0, "rcon") == 0) {
            SVC_RemoteCommand(from, msg);
        } else {
            Q_stricmp(argv0, "disconnect");
        }
    }
}

/* 0x08095e3a  size=13  FUN_08095e3a  status=translated  naming=inferred */
/* naming: no string; returns the sv_maxclients value. */
int SV_PB_GetMaxClients(void)
{
    return cv_sv_maxclients->current.integer;
}

/* 0x08095e48  size=284  FUN_08095e48  status=translated  naming=inferred */
/* naming: no string; fills a 0x68-byte status record (name, second string, address) for a client in state >= 4; SV_PB_GetClientInfo is inferred. */
int SV_PB_GetClientInfo(int clientNum, char *out)
{
    int result;

    memset(out, 0, 0x68);
    if (clientNum < 0 || clientNum >= cv_sv_maxclients->current.integer) {
        result = 0;
    } else if (svs.clients == 0) {
        result = 0;
    } else if (svs.clients[clientNum].state < 4) {
        result = 0;
    } else {
        struct client_s *cl = &svs.clients[clientNum];
        char *addr;

        strcpy(out, cl->name);
        strcpy(out + 0x21, cl->authKeyA);
        addr = NET_AdrToString(cl->netchan.remoteAddress);
        strcpy(out + 0x42, addr);
        result = 1;
    }
    return result;
}

/* 0x08095f64  size=202  FUN_08095f64  status=translated  naming=inferred */
/* naming: format string "ping=%d score=%d" (in the body) names the score text. */
int SV_PB_GetClientScoreText(int clientNum, char *out)
{
    int result;

    *out = '\0';
    if (clientNum < 0 || clientNum >= cv_sv_maxclients->current.integer) {
        result = 0;
    } else if (svs.clients == 0) {
        result = 0;
    } else if (svs.clients[clientNum].state < 4) {
        result = 0;
    } else {
        struct client_s *cl = &svs.clients[clientNum];
        int score = 0;

        if (g_083e2f80 != 0) {
            score = G_GetClientScore((int)(((unsigned int)((char *)cl - (char *)svs.clients) >> 2) * 0x2165829u));
        }
        sprintf(out, "ping=%d score=%d", cl->ping, score);
        result = 1;
    }
    return result;
}

/* 0x0809602e  size=179  FUN_0809602e  status=translated  naming=inferred */
/* naming: no string; forwards a message to each connected client (optionally one slot) while svs.initialized == 1; SV_PB_SendOOBToClients is inferred. */
void SV_PB_SendOOBToClients(int arg1, int arg2, int clientNum)
{
    int i;

    if (svs.initialized == 1) {
        for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
            if ((clientNum < 0 || clientNum == i) && svs.clients[i].state > 1) {
                struct client_s *cl = &svs.clients[i];

                NET_OutOfBandSend(1, cl->netchan.remoteAddress, (const void *)arg2, arg1);
            }
        }
    }
}
