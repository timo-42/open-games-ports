/*
 * chunk_020: cod2_lnxded 1.3 server connection, authorize, download, client command,
 * usercmd, gentity/svEntity helpers and skeleton-memory functions.
 *
 * Target: Call of Duty 2 Linux dedicated server 1.3 (ELF32 i386, GCC 3.3.6, C-style C++).
 * Functions are in address order. Provenance comments give address, size, the Ghidra
 * label, translation status and naming evidence.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include <ctype.h>
#include "chunk_020_types.h"

/* Ghidra stack ints passed as "a * 0x2c419" style index math: client number from pointer. */
static inline int32 SV_ClientNum(const client_s *cl)
{
    return (int32)((uint32)((((ptrdiff_t)((const char *)cl - (const char *)g_svs_clients)) >> 2)) * 0x2165829u);
}

/* In-chunk prototypes (defined below in address order). */
void SV_InitGameEntityLayout(uint8 *gentities, int32 numEntities, int32 gentitySize,
                             uint8 *gameClients, int32 gameClientSize);

/* ------------------------------------------------------------------------- */

// 0x0808cefc  size=32  FUN_0808cefc  status=translated  naming=unknown
// evidence: descriptive only. Registers the two literal command names "say" and "tell"
//           via one callee. Purpose of the callee is not identified.
void SV_RemoveSayTellCommands(void)
{
    Cmd_RemoveCommand("say");
    Cmd_RemoveCommand("tell");
}

// 0x0808cf1c  size=34  FUN_0808cf1c  status=translated  naming=unknown
// evidence: descriptive only. Prints "%s: %s\n" through the console print callee.
void SV_PrintTagged(const char *tag, const char *text)
{
    Com_Printf("%s: %s\n", tag, text);
}

// 0x0808cf40  size=134  FUN_0808cf40  status=translated  naming=inferred
// evidence: compares the sender against the authorize server address (SV_ResolveMasterAddress) and,
//           on match, stamps the last-authorize time. Name is descriptive of that role.
void SV_NoteAuthorizeServerPacket(netadr_t from)
{
    netadr_t *authServer;
    int32 matched;

    authServer = SV_ResolveMasterAddress();
    matched = NET_CompareBaseAdr(from, *authServer);
    if (matched != 0) {
        sv_lastAuthorizeServerTime = g_svs_time;
    }
}

// 0x0808cfc6  size=456  FUN_0808cfc6  status=translated  naming=inferred
// evidence: format strings "sending getIpAuthorize for %s" and
//           "getIpAuthorize %i %i.%i.%i.%i \"%s\" %i [PB \"%s\"]"; matches the
//           authorize-server request of the IW/Q3 lineage.
void SV_SendGetIpAuthorize(netadr_t from, int32 challenge, const char *pbName)
{
    char fsGame[1035];
    dvar_s *fsGameVar;
    const char *addrText;
    const char *msg;
    uint8 allowAnonymous;

    if (sv_authorizeAddress.type != COD2_NA_BAD) {
        fsGame[0] = '\0';
        fsGameVar = Dvar_RegisterString("fs_game", "", 0x101c);
        if (fsGameVar != NULL && *fsGameVar->current.string != '\0') {
            strcpy(fsGame, fsGameVar->current.string);
        }
        addrText = NET_AdrToString(from);
        Com_DPrintf("sending getIpAuthorize for %s\n", addrText);
        allowAnonymous = (uint8)Dvar_GetBool("sv_allowAnonymous");
        if (pbName == NULL) {
            msg = va("getIpAuthorize %i %i.%i.%i.%i \"%s\" %i",
                              challenge,
                              from.ip[0],
                              from.ip[1],
                              from.ip[2],
                              from.ip[3],
                              fsGame, allowAnonymous);
            NET_OutOfBandPrint(1, sv_authorizeAddress, msg);
        } else {
            msg = va("getIpAuthorize %i %i.%i.%i.%i \"%s\" %i PB \"%s\"",
                              challenge,
                              from.ip[0],
                              from.ip[1],
                              from.ip[2],
                              from.ip[3],
                              fsGame, allowAnonymous, pbName);
            NET_OutOfBandPrint(1, sv_authorizeAddress, msg);
        }
    }
}

// 0x0808d18e  size=1053  FUN_0808d18e  status=translated  naming=inferred
// evidence: string "challengeResponse %i", "Resolving %s", "cod2master.activision.com",
//           "authorize server timed out"; calls SV_SendGetIpAuthorize. Allocates or reuses
//           a challenge slot for the sender.
void SV_RequestIpAuthorize(netadr_t from)
{
    int32 slotLocal;
    int32 oldestTime;
    int32 idx;
    int32 match;
    challenge_s *ch;
    int32 iVar2;
    int32 r1;
    int32 r2;
    const char *msg;
    const char *pbArg;
    int16 sVar1;
    netadr_t *authServer;

    slotLocal = 0;
    oldestTime = 0x7fffffff;
    ch = g_svs_challenges;
    idx = 0;
    while (idx < 0x400 &&
           (ch->connected != 0 || (iVar2 = NET_CompareAdr(from, ch->adr), iVar2 == 0))) {
        if (ch->time < oldestTime) {
            oldestTime = ch->time;
            slotLocal = idx;
        }
        idx = idx + 1;
        ch = ch + 1;
    }
    if (idx == 0x400) {
        ch = &g_svs_challenges[slotLocal];
        r1 = rand();
        r2 = rand();
        ch->challenge = (int32)((uint32)r2 ^ ((uint32)r1 << 0x10) ^ (uint32)g_svs_time);
        ch->adr = from;
        ch->firstTime = g_svs_time;
        ch->firstPing = 0;
        ch->time = g_svs_time;
        ch->connected = 0;
        idx = slotLocal;
    }
    if ((char)cvar_net_lanauthorize->current.integer == '\0' && (match = Sys_IsLANAddress(from), match != 0)) {
        ch->pingTime = g_svs_time;
        msg = va("challengeResponse %i", ch->challenge);
        NET_OutOfBandPrint(1, from, msg);
    } else {
        if ((sv_authorizeAddress.ip[0] == '\0') && (sv_authorizeAddress.type != COD2_NA_BAD)) {
            Com_Printf("Resolving %s\n", "cod2master.activision.com");
            iVar2 = NET_StringToAdr("cod2master.activision.com", &sv_authorizeAddress);
            if (iVar2 == 0) {
                Com_Printf("Couldn't resolve address\n");
                return;
            }
            sv_authorizeAddress.port = (uint16)BigShort(0x50dc);
            sVar1 = (int16)BigShort((int)(int16)sv_authorizeAddress.port);
            Com_Printf("%s resolved to %i.%i.%i.%i:%i\n", "cod2master.activision.com",
                        (int)sv_authorizeAddress.ip[0], /* gameplay-fix(net_extra): movzx byte at 0x0808d3d9..0x0808d3fa; was sign-extended (198 printed as -58) */
                        (int)sv_authorizeAddress.ip[1],
                        (int)sv_authorizeAddress.ip[2],
                        (int)sv_authorizeAddress.ip[3],
                        (int)sVar1);
        }
        if (1200000 < (int)(g_svs_time - sv_lastAuthorizeServerTime) &&
            7000 < (int)(g_svs_time - ch->firstTime)) {
            authServer = SV_ResolveMasterAddress();
            iVar2 = NET_CompareAdr(from, *authServer);
            if (iVar2 == 0) {
                Com_DPrintf("authorize server timed out\n");
                ch->pingTime = g_svs_time;
                msg = va("challengeResponse %i", ch->challenge);
                NET_OutOfBandPrint(1, ch->adr, msg);
                return;
            }
        }
        pbArg = NULL;
        if (Cmd_Argc_alt() == 3) {
            pbArg = Cmd_Argv_alt(2);
            Q_strncpyz(g_svs_challenges[idx].clientPBguid2, pbArg, 0x21);
        }
        SV_SendGetIpAuthorize(from, g_svs_challenges[idx].challenge, pbArg);
    }
}

// 0x0808d5ac  size=131  FUN_0808d5ac  status=translated  naming=inferred
// evidence: caller prints "rejected connection from temporarily banned GUID %i";
//           reads a 16-slot recent-ban table and the cvar timeout in seconds (x1000).
int32 SV_IsTempBannedGuid(int32 guid)
{
    uint32 i;

    if (guid != 0) {
        for (i = 0; i < 0x10; i++) {
            if (g_svs_tempBans[i].guid == guid &&
                (double)(float)(g_svs_time - g_svs_tempBans[i].banTime) <=
                    (double)cv_sv_kickBanTime->current.value * 1000.0) {
                return 1;
            }
        }
    }
    return 0;
}

// 0x0808d630  size=158  FUN_0808d630  status=translated  naming=inferred
// evidence: reads "ban.txt" and compares each line's atoi() against the GUID.
int32 SV_IsGuidPermBanned(int32 guid)
{
    void *fileBuf;
    const char *cursor;
    char *token;
    int32 found;
    int32 len;

    if (guid == 0) {
        return 0;
    }
    fileBuf = NULL;
    len = FS_ReadFile("ban.txt", &fileBuf);
    if (len < 0) {
        return 0;
    }
    cursor = (char *)fileBuf;
    found = 0;
    for (;;) {
        token = Com_Parse(&cursor);
        if (*token == '\0') {
            break;
        }
        if (atoi(token) == guid) {
            found = 1;
            break;
        }
        Com_SkipRestOfLine(&cursor);
    }
    FS_FreeFile(fileBuf);
    return found;
}

// 0x0808d6ce  size=95  FUN_0808d6ce  status=translated  naming=inferred
// evidence: scans the 16-slot temp-ban table for a free (guid 0) slot, else the
//           smallest time stamp.
uint32 SV_TempBanFindSlot(void)
{
    uint32 best;
    uint32 i;

    best = 0;
    i = 0;
    for (;;) {
        if (i > 0xf) {
            return best;
        }
        if (g_svs_tempBans[i].guid == 0) {
            break;
        }
        if (g_svs_tempBans[i].banTime < g_svs_tempBans[best].banTime) {
            best = i;
        }
        i = i + 1;
    }
    return i;
}

// 0x0808d72e  size=44  FUN_0808d72e  status=translated  naming=inferred
// evidence: stores guid and current svs.time into the slot chosen by SV_TempBanFindSlot.
void SV_TempBanAdd(int32 guid)
{
    uint32 slot;

    slot = SV_TempBanFindSlot();
    g_svs_tempBans[slot].guid = guid;
    g_svs_tempBans[slot].banTime = g_svs_time;
}

// 0x0808d75a  size=299  FUN_0808d75a  status=translated  naming=observed
// evidence: strings "Can't ban user, GUID is 0", "This GUID (%i) is already banned",
//           "EXE_PLAYERKICKED", "ban.txt", "%i %s\r\n".
void SV_BanClient(client_s *cl)
{
    char nameBuf[76];
    int32 handle[3];
    int32 iVar1;

    if (cl->netchan.remoteAddress.type == COD2_NA_LOOPBACK) {
        SV_SendServerCommand(NULL, 0, "%c \"EXE_CANNOTKICKHOSTPLAYER\"", 0x65);
    } else if (cl->guid == 0) {
        Com_Printf("Can't ban user, GUID is 0\n");
    } else {
        iVar1 = SV_IsGuidPermBanned(cl->guid);
        if (iVar1 == 0) {
            iVar1 = FSH_FOpenFile("ban.txt", handle, 2);
            if (iVar1 >= 0) {
                Q_strncpyz(nameBuf, cl->name, 0x40);
                Q_CleanStr(nameBuf);
                FS_Printf(handle[0], "%i %s\r\n", cl->guid, nameBuf);
                FS_FCloseFile(handle[0]);
                SV_DropClient(cl, "EXE_PLAYERKICKED");
                cl->lastPacketTime = g_svs_time;
            }
        } else {
            Com_Printf("This GUID (%i) is already banned\n", cl->guid);
        }
    }
}

// 0x0808d886  size=420  FUN_0808d886  status=translated  naming=observed
// evidence: strings "unbanned %i user(s) named %s", "no banned user has name %s",
//           "ban.txt"; removes matching name lines from ban.txt.
void SV_UnbanUserByName(const char *name)
{
    const char *dest;
    const char *cursor;
    char *token;
    char *base;
    char nameBuf[72];
    int32 fileLen;
    int32 removed;
    size_t nameLen;
    bool matched;
    int32 cmp;

    fileLen = FS_ReadFile("ban.txt", (void **)&base);
    if (fileLen >= 0) {
        Q_strncpyz(nameBuf, name, 0x40);
        Q_CleanStr(nameBuf);
        nameLen = strlen(nameBuf);
        removed = 0;
        cursor = base;
        for (;;) {
            dest = cursor;
            token = Com_Parse(&cursor);
            if (*token == '\0') {
                break;
            }
            while (*cursor != '\0' && *cursor < '!') {
                cursor = cursor + 1;
            }
            matched = false;
            cmp = Str_NCaseCmp_0809163e(cursor, nameBuf, (int32)nameLen);
            if (cmp == 0 && (cursor[nameLen] == '\r' || cursor[nameLen] == '\n')) {
                matched = true;
            }
            Com_SkipRestOfLine(&cursor);
            if (matched) {
                removed = removed + 1;
                memmove((char *)dest, cursor, (size_t)((fileLen - (int32)(cursor - base)) + 1));
                fileLen = fileLen - (int32)(cursor - dest);
                cursor = dest;
            }
        }
        FS_WriteFile("ban.txt", base, fileLen);
        FS_FreeFile(base);
        if (removed == 0) {
            Com_Printf("no banned user has name %s\n", nameBuf);
        } else {
            Com_Printf("unbanned %i user(s) named %s\n", removed, nameBuf);
        }
    }
}

// 0x0808da2a  size=1983  FUN_0808da2a  status=translated  naming=observed
// evidence: string "SV_AuthorizeIpPacket: not from authorize server" (and "challenge not found").
void SV_AuthorizeIpPacket(netadr_t from)
{
    char msgBuf[1024];
    const char *verb;
    const char *keyArg;
    const char *text;
    int32 challenge;
    int32 slot;
    challenge_s *ch;
    int32 iVar2;
    uint8 allowDemo;

    iVar2 = NET_CompareBaseAdr(from, sv_authorizeAddress);
    if (iVar2 == 0) {
        Com_Printf("SV_AuthorizeIpPacket: not from authorize server\n");
        return;
    }
    text = Cmd_Argv_alt(1);
    challenge = atoi(text);
    for (slot = 0; slot < 0x400 && g_svs_challenges[slot].challenge != challenge; slot++) {
    }
    if (slot == 0x400) {
        Com_Printf("SV_AuthorizeIpPacket: challenge not found\n");
        return;
    }
    ch = &g_svs_challenges[slot];
    ch->pingTime = g_svs_time;
    verb = Cmd_Argv_alt(2);
    keyArg = Cmd_Argv_alt(3);
    text = Cmd_Argv(5);
    Q_strncpyz(ch->clientPBguid, text, 0x21);
    if (Q_stricmp(verb, "demo") == 0) {
        allowDemo = (uint8)Dvar_GetBool("fs_restrict");
        if (allowDemo == 0) {
            NET_OutOfBandPrint(1, ch->adr, "error\nEXE_ERR_NOT_A_DEMO_SERVER");
            memset(ch, 0, 0x74);
        } else {
            text = va("challengeResponse %i", ch->challenge);
            NET_OutOfBandPrint(1, ch->adr, text);
        }
    } else if (Q_stricmp(verb, "accept") == 0) {
        text = Cmd_Argv_alt(4);
        ch->guid = atoi(text);
        if (SV_IsGuidPermBanned(ch->guid) == 0) {
            if (SV_IsTempBannedGuid(ch->guid) == 0) {
                if (ch->connected == 0) {
                    text = va("challengeResponse %i", ch->challenge);
                    NET_OutOfBandPrint(1, ch->adr, text);
                }
            } else {
                Com_Printf("rejected connection from temporarily banned GUID %i\n", ch->guid);
                NET_OutOfBandPrint(1, ch->adr, "error\n" "\x15" "You are temporarily banned from this server");
                memset(ch, 0, 0x74);
            }
        } else {
            Com_Printf("rejected connection from permanently banned GUID %i\n", ch->guid);
            NET_OutOfBandPrint(1, ch->adr, "error\n" "\x15" "You are permanently banned from this server");
            memset(ch, 0, 0x74);
        }
    } else if (Q_stricmp(verb, "deny") == 0) {
        if (keyArg == NULL || *keyArg == '\0') {
            NET_OutOfBandPrint(1, ch->adr, "error\nEXE_ERR_CDKEY_IN_USE");
        } else if (Q_stricmp(keyArg, "CLIENT_UNKNOWN_TO_AUTH") == 0) {
            NET_OutOfBandPrint(1, ch->adr, "needcdkey");
        } else if (Q_stricmp(keyArg, "BAD_CDKEY") == 0) {
            NET_OutOfBandPrint(1, ch->adr, "needcdkey");
        } else if (Q_stricmp(keyArg, "INVALID_CDKEY") == 0) {
            NET_OutOfBandPrint(1, ch->adr, "error\nEXE_ERR_CDKEY_IN_USE");
        } else if (Q_stricmp(keyArg, "BANNED_CDKEY") == 0) {
            NET_OutOfBandPrint(1, ch->adr, "error\nEXE_ERR_BAD_CDKEY");
        } else {
            NET_OutOfBandPrint(1, ch->adr, "error\nEXE_ERR_BAD_CDKEY");
        }
        memset(ch, 0, 0x74);
    } else {
        if (keyArg == NULL || *keyArg == '\0') {
            NET_OutOfBandPrint(1, ch->adr, "error\nEXE_ERR_BAD_CDKEY");
        } else {
            sprintf(msgBuf, "error\n%s", keyArg);
            NET_OutOfBandPrint(1, ch->adr, msgBuf);
        }
        memset(ch, 0, 0x74);
    }
}

// 0x0808e1ea  size=103  FUN_0808e1ea  status=translated  naming=inferred
// evidence: shared cleanup of the drop and drop-all paths; calls the download-state reset
//           (SV_ClientResetDownload) and the game-side client hooks.
void SV_ClientDisconnectCleanup(client_s *cl)
{
    int32 num;

    num = SV_ClientNum(cl);
    SV_ClientResetDownload(cl);
    if (SV_IsGameRunning() != '\0') {
        ClientDisconnect(num);
    }
    SV_SetUserinfo(num, "");
    SV_ReleaseClientScriptId(cl);
}

// 0x0808e252  size=87  FUN_0808e252  status=translated  naming=inferred
// evidence: walks all client slots, runs the cleanup for every slot with state >= CS_CONNECTED,
//           then releases the client array through Com_Free_B.
void SV_DropAllConnectedClients(void)
{
    client_s *cl;
    int32 i;

    cl = g_svs_clients;
    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (cl->state > 1) {
            SV_ClientDisconnectCleanup(cl);
        }
        cl = cl + 1;
    }
    Com_Free_B(g_svs_clients);
}

// 0x0808e2aa  size=3363  FUN_0808e2aa  status=translated  naming=observed
// evidence: string "SV_DirectConnect()\n" plus connect-path messages ("Going from CS_FREE to
//           CS_CONNECTED", "reconnect rejected : too soon", "EXE_SERVERISFULL").
void SV_DirectConnect(netadr_t from)
{
    char keyA[0x30];
    char keyB[0x30];
    char userinfo[0x404];
    const char *text;
    const char *uVar4;
    const char *errText;
    const char *localAddrText;
    const char *passwordArg;
    int32 protocol;
    int32 challengeArg;
    int32 qportArg;
    int32 loopIdx;
    int32 match;
    int32 pingVal;
    int32 guid;
    int32 punkbuster;
    int32 iVar6;
    int32 reservedLow;
    int32 counted;
    int32 slotNum;
    bool reconnectMatch;
    client_s *cl;
    client_s *newcl;

    memset(keyA, 0, 0x21);
    memset(keyB, 0, 0x21);
    Com_DPrintf("SV_DirectConnect()\n");
    text = Cmd_Argv_alt(1);
    Q_strncpyz(userinfo, text, 0x400);
    text = Info_ValueForKey(userinfo, "protocol");
    protocol = atoi(text);
    if (protocol != 0x76) {
        uVar4 = va("error\nEXE_SERVER_IS_DIFFERENT_VER\x15" "%s\n", "1.3");
        NET_OutOfBandPrint(1, from, uVar4);
        Com_DPrintf("    rejected connect from protocol version %i (should be %i)\n", protocol, 0x76);
        return;
    }
    text = Info_ValueForKey(userinfo, "challenge");
    challengeArg = atoi(text);
    text = Info_ValueForKey(userinfo, "qport");
    qportArg = atoi(text);

    /* Reconnect-too-soon check over all client slots. */
    cl = g_svs_clients;
    for (loopIdx = 0; loopIdx < cv_sv_maxclients->current.integer; loopIdx++) {
        match = 0;
        if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress) != 0 &&
            (cl->netchan.qport == qportArg || (int16)from.port == (int16)cl->netchan.remoteAddress.port)) {
            match = 1;
        }
        if (match != 0) {
            if (g_svs_time - cl->lastConnectTime < cv_sv_reconnectlimit->current.integer * 1000) {
                text = NET_AdrToString(from);
                Com_DPrintf("%s:reconnect rejected : too soon\n", text);
                return;
            }
            break;
        }
        cl = cl + 1;
    }

    guid = 0;
    if (NET_IsLocalAddress(from) == 0) {
        for (loopIdx = 0; loopIdx < 0x400; loopIdx++) {
            if (NET_CompareAdr(from, g_svs_challenges[loopIdx].adr) != 0 &&
                challengeArg == g_svs_challenges[loopIdx].challenge) {
                guid = g_svs_challenges[loopIdx].guid;
                Q_strncpyz(keyA, g_svs_challenges[loopIdx].clientPBguid, 0x21);
                Q_strncpyz(keyB, g_svs_challenges[loopIdx].clientPBguid2, 0x21);
                break;
            }
        }
        if (loopIdx == 0x400) {
            NET_OutOfBandPrint(1, from, "error\nEXE_BAD_CHALLENGE");
            return;
        }
        if (g_svs_challenges[loopIdx].firstPing == 0) {
            pingVal = g_svs_time - g_svs_challenges[loopIdx].pingTime;
            g_svs_challenges[loopIdx].firstPing = pingVal;
        } else {
            pingVal = g_svs_challenges[loopIdx].firstPing;
        }
        text = NET_AdrToString(from);
        Com_Printf("Client %i connecting with %i challenge ping from %s\n", loopIdx, pingVal, text);
        g_svs_challenges[loopIdx].connected = 1;
        if (Sys_IsLANAddress(from) == 0) {
            if (cv_sv_minPing->current.integer != 0 && cv_sv_minPing->current.integer > pingVal) {
                NET_OutOfBandPrint(1, from, "error\nEXE_ERR_HIGH_PING_ONLY");
                Com_DPrintf("Client %i rejected on a too low ping\n", loopIdx);
                return;
            }
            if (cv_sv_maxPing->current.integer != 0 && cv_sv_maxPing->current.integer < pingVal) {
                NET_OutOfBandPrint(1, from, "error\nEXE_ERR_LOW_PING_ONLY");
                Com_DPrintf("Client %i rejected on a too high ping: %i\n", loopIdx, pingVal);
                return;
            }
        }
    }

    text = Info_ValueForKey(userinfo, "cl_punkbuster");
    punkbuster = atoi(text);
    if (NET_IsLocalAddress(from) == 0) {
        localAddrText = NET_AdrToString(from);
    } else {
        localAddrText = "localhost";
    }
    errText = PB_CheckClientConnect(localAddrText, punkbuster, keyA);
    if (errText != NULL) {
        iVar6 = Q_stricmpn(errText, "error\n", 6);
        if (iVar6 == 0) {
            NET_OutOfBandPrint(1, from, errText);
        }
        return;
    }

    /* Reconnect of an existing slot with the same address/qport. */
    reconnectMatch = false;
    newcl = NULL;
    cl = g_svs_clients;
    for (loopIdx = 0; loopIdx < cv_sv_maxclients->current.integer; loopIdx++) {
        if (cl->state != 0) {
            match = 0;
            if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress) != 0 &&
                (cl->netchan.qport == qportArg || (int16)from.port == (int16)cl->netchan.remoteAddress.port)) {
                match = 1;
            }
            if (match != 0) {
                text = NET_AdrToString(from);
                Com_Printf("%s:reconnect\n", text);
                if (cl->state > 1) {
                    SV_ClientDisconnectCleanup(cl);
                }
                reconnectMatch = true;
                newcl = cl;
                break;
            }
        }
        cl = cl + 1;
    }

    if (!reconnectMatch) {
        passwordArg = Info_ValueForKey(userinfo, "password");
        iVar6 = strcmp(passwordArg, cv_sv_privatePassword->current.string);
        if (iVar6 == 0) {
            reservedLow = 0;
        } else {
            reservedLow = cv_sv_privateClients->current.integer;
        }
        for (loopIdx = reservedLow; loopIdx < cv_sv_maxclients->current.integer; loopIdx++) {
            cl = g_svs_clients + loopIdx;
            if (cl->state == 0) {
                newcl = cl;
                break;
            }
        }
        if (newcl == NULL) {
            NET_OutOfBandPrint(1, from, "error\nEXE_SERVERISFULL");
            Com_DPrintf("Rejected a connection.\n");
            return;
        }
        newcl->reliableAcknowledge = 0;
        newcl->reliableSequence = 0;
    }

    memset(newcl, 0, 0xB1064);
    slotNum = SV_ClientNum(newcl);
    newcl->gentity = SV_GentityNum(slotNum);
    newcl->scriptId = ScrVar_AllocArray();
    newcl->challenge = challengeArg;
    if (guid == 0) {
        Com_Printf("Connecting player #%i has a zero GUID\n", slotNum);
    }
    newcl->guid = guid;
    Netchan_Setup(1, &newcl->netchan, from, qportArg);
    newcl->voicePacketCount = 0;
    newcl->sendVoice = 1;
    Q_strncpyz(newcl->userinfo, userinfo, 0x400);
    errText = ClientConnect(slotNum, (int16)newcl->scriptId);
    if (errText == NULL) {
        Com_Printf("Going from CS_FREE to CS_CONNECTED for %s (num %i guid %i)\n",
                    newcl->name, slotNum, newcl->guid);
        newcl->state = CS_CONNECTED;
        newcl->nextSnapshotTime = g_svs_time;
        newcl->lastPacketTime = g_svs_time;
        newcl->lastConnectTime = g_svs_time;
        Q_strncpyz(newcl->authKeyA, keyA, 0x21);
        Q_strncpyz(newcl->authKeyB, keyB, 0x21);
        SV_UserinfoChanged(newcl);
        g_svs_challenges[loopIdx].firstPing = 0;
        NET_OutOfBandPrint(1, from, "connectResponse"); /* 0x0808eef3..0x0808ef25 */
        newcl->gamestateMessageNum = -1;
        counted = 0;
        cl = g_svs_clients;
        for (loopIdx = 0; loopIdx < cv_sv_maxclients->current.integer; loopIdx++) {
            if (cl->state > 1) {
                counted = counted + 1;
            }
            cl = cl + 1;
        }
        if (counted == 1 || counted == cv_sv_maxclients->current.integer) {
            SV_Heartbeat_f();
        }
    } else {
        text = va("error\n%s", errText);
        NET_OutOfBandPrint(1, from, text);
        Com_DPrintf("Game rejected a connection: %s.\n", errText);
        SV_ReleaseClientScriptId(newcl);
    }
}

// 0x0808efce  size=96  FUN_0808efce  status=translated  naming=unknown
// evidence: descriptive only. For each connected slot runs SV_ReleaseClientScriptId and reassigns the
//           per-client 16-bit id from ScrVar_AllocArray.
void SV_ReallocClientScriptIds(void)
{
    client_s *cl;
    int32 i;

    cl = g_svs_clients;
    for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (cl->state > 1) {
            SV_ReleaseClientScriptId(cl);
            cl->scriptId = ScrVar_AllocArray();
        }
        cl = cl + 1;
    }
}

// 0x0808f02e  size=650  FUN_0808f02e  status=translated  naming=observed
// evidence: strings "Going to CS_ZOMBIE for %s\n", "EXE_DISCONNECTED", "%i:%s %s\n".
void SV_DropClient(client_s *cl, const char *reason)
{
    char name[43];
    challenge_s *entry;
    int32 i;
    int32 isPb;
    const char *suffix;

    if (cl->state == CS_ZOMBIE) {
        return;
    }
    cl->delayDropMsg = 0;
    strcpy(name, cl->name);
    SV_ClientDisconnectCleanup(cl);
    Com_DPrintf("Going to CS_ZOMBIE for %s\n", name);
    cl->state = CS_ZOMBIE;
    if (cl->gentity == NULL) {
        entry = g_svs_challenges;
        for (i = 0; i < 0x400; i++) {
            if (NET_CompareAdr(cl->netchan.remoteAddress, entry->adr) != 0) {
                entry->connected = 0;
                break;
            }
            entry = entry + 1;
        }
    }
    isPb = Loc_TranslateString((char *)reason) != 0;
    if (Q_stricmp(reason, "EXE_DISCONNECTED") != 0) {
        if (isPb == 0) {
            suffix = "";
        } else {
            suffix = "\x14";
        }
        SV_SendServerCommand(NULL, 0, "%c \"\x15" "%s^7 %s%s\"", 0x65, name, suffix, reason);
    }
    Com_Printf("%i:%s %s\n", SV_ClientNum(cl), name, reason);
    SV_SendServerCommand(NULL, 1, "%c %d", 0x4a, SV_ClientNum(cl));
    if (isPb == 0) {
        SV_SendServerCommand(cl, 1, "%c \"%s^7 %s\" PB", 0x77, name, reason);
    } else {
        SV_SendServerCommand(cl, 1, "%c \"%s\"", 0x77, reason);
    }
    for (i = 0; i < cv_sv_maxclients->current.integer && g_svs_clients[i].state < 2; i++) {
    }
    if (i == cv_sv_maxclients->current.integer) {
        SV_Heartbeat_f();
    }
}

// 0x0808f2b8  size=35  FUN_0808f2b8  status=translated  naming=unknown
// evidence: descriptive only. Stores a reason word into an empty pending-drop slot of a
//           non-zombie client.
void SV_SetPendingDropReason(client_s *cl, const char *reason)
{
    if (cl->state != CS_ZOMBIE && cl->delayDropMsg == 0) {
        cl->delayDropMsg = reason;
    }
}

// 0x0808f2dc  size=38  FUN_0808f2dc  status=translated  naming=inferred
// evidence: index wrapper over SV_DropClient (client array base + num * stride).
void SV_DropClientByNum(int32 clientNum, const char *reason)
{
    SV_DropClient(g_svs_clients + clientNum, reason);
}

// 0x0808f302  size=737  FUN_0808f302  status=translated  naming=observed
// evidence: strings "SV_SendClientGameState() for %s", "Going from CS_CONNECTED to CS_PRIMED",
//           "Sending %i bytes in gamestate to client: %i". msg.cursize at +0x0c is the
//           printed byte count (the stack slot ebp-0x120 is msg+0x0c).
void SV_SendClientGameState(client_s *cl)
{
    int32 tmp[4]; /* 16-byte pool-offset save area (was tempbuf_s) */
    msg_t msg;
    uint8 cfgBuf[0xf0];
    uint8 *dataBuf;
    int32 i;

    Com_PoolAllocInto2(tmp, 0x20000);
    dataBuf = (uint8 *)Com_PoolPtrFromOffset(tmp);
    while (cl->state != CS_FREE && cl->netchan.unsentFragments != 0) {
        SV_Netchan_TransmitNextFragment(&cl->netchan);
    }
    Com_DPrintf("SV_SendClientGameState() for %s\n", cl->name);
    Com_DPrintf("Going from CS_CONNECTED to CS_PRIMED for %s\n", cl->name);
    cl->state = CS_PRIMED;
    cl->pureAuthentic = 0;
    cl->gamestateMessageNum = cl->netchan.outgoingSequence;
    MSG_Init(&msg, dataBuf, 0x20000);
    MSG_WriteLong(&msg, cl->lastClientCommand);
    SV_UpdateServerCommandsToClient(cl, &msg);
    MSG_WriteByte(&msg, 1);
    MSG_WriteLong(&msg, cl->reliableSequence);
    for (i = 0; i < 0x800; i++) {
        if (*sv_configstrings[i] != '\0') {
            MSG_WriteByte(&msg, 2);
            MSG_WriteShort(&msg, i);
            MSG_WriteBigString(&msg, sv_configstrings[i]);
        }
    }
    memset(cfgBuf, 0, 0xf0);
    for (i = 0; i < 0x400; i++) {
        if (sv_svEntities[i].baseline.s.number != 0) {
            MSG_WriteByte(&msg, 3);
            MSG_WriteDeltaEntity(&msg, cfgBuf, &sv_svEntities[i].baseline.s.number, 1);
        }
    }
    MSG_WriteByte(&msg, 7);
    MSG_WriteLong(&msg, SV_ClientNum(cl));
    MSG_WriteLong(&msg, sv_checksumFeed);
    MSG_WriteByte(&msg, 7);
    Com_DPrintf("Sending %i bytes in gamestate to client: %i\n", msg.cursize, SV_ClientNum(cl));
    SV_SendMessageToClient(&msg, cl);
    Com_PoolRestoreFrom2(tmp);
}

// 0x0808f628  size=205  FUN_0808f628  status=translated  naming=inferred
// evidence: string "Going from CS_PRIMED to CS_ACTIVE for %s"; copies the 7-dword usercmd
//           into the client and binds the game entity (ent->number = slot).
void SV_ClientEnterWorld(client_s *cl, const int32 *ucmd)
{
    int32 num;
    gentity_s *ent;
    int32 k;

    Com_DPrintf("Going from CS_PRIMED to CS_ACTIVE for %s\n", cl->name);
    cl->state = CS_ACTIVE;
    num = SV_ClientNum(cl);
    ent = SV_GentityNum(num);
    ent->s.number = num;
    cl->gentity = ent;
    cl->deltaMessage = -1;
    cl->nextSnapshotTime = g_svs_time;
    for (k = 0; k < 7; k++) {
        ((int32 *)&cl->lastUsercmd)[k] = ucmd[k];
    }
    ClientBegin(num);
}

// 0x0808f6f6  size=137  FUN_0808f6f6  status=translated  naming=inferred
// evidence: releases the download file handle, clears the download name and frees the eight
//           download block buffers.
void SV_ClientResetDownload(client_s *cl)
{
    int32 i;

    if (cl->download != 0) {
        FS_FCloseFile(cl->download);
    }
    cl->download = 0;
    cl->downloadName[0] = '\0';
    for (i = 0; i < 8; i++) {
        if (cl->downloadBlocks[i] != NULL) {
            Com_Free_A(cl->downloadBlocks[i]);
            cl->downloadBlocks[i] = NULL;
        }
    }
}

// 0x0808fc0c  size=343  FUN_0808fc0c  status=translated  naming=observed
// evidence: "Redirecting client '%s' to %s" and "%s/%s" built from the www base cvar.
// The "couldn't extract file size" message is passed a single argument in the binary;
// the second %s has no argument there and is reproduced as-is.
bool SV_RedirectClientDownload(client_s *cl, msg_t *msg)
{
    int32 iVar1;
    int32 handle;
    const char *uVar2;

    iVar1 = FS_SV_FOpenFileRead(cl->downloadName, &handle);
    if (iVar1 == 0) {
        /* gameplay-fix(net_dl_voice): 0x0808fd4b passes only the name; the second %s reads leftover stack at [esp+8], which
         * the original prints as an empty string in 3/3 oracle runs (the recon read a NULL there and printed "(null)") */
        Com_Printf("ERROR: Client '%s': couldn't extract file size for %s\n", cl->downloadName, "");
    } else {
        FS_FCloseFile(handle);
        uVar2 = va("%s/%s", cv_sv_wwwBaseURL->current.string, cl->downloadName);
        Q_strncpyz(cl->downloadURL, uVar2, 0x100);
        Com_Printf("Redirecting client '%s' to %s\n", cl->name, cl->downloadURL);
        cl->downloadingWWW = 1;
        MSG_WriteByte(msg, 5);
        MSG_WriteShort(msg, -1);
        MSG_WriteString(msg, cl->downloadURL);
        MSG_WriteLong(msg, iVar1);
        MSG_WriteLong(msg, (char)cv_sv_wwwDlDisconnected->current.integer != '\0');
        cl->downloadName[0] = '\0';
    }
    return iVar1 != 0;
}

// 0x0808fd64  size=94  FUN_0808fd64  status=translated  naming=inferred
// evidence: writes a download-command record (svc 5, handle -1 with error text) and clears
//           the download name.
void SV_SendDownloadDenied(client_s *cl, msg_t *msg, const char *text)
{
    MSG_WriteByte(msg, 5);
    MSG_WriteShort(msg, 0);
    MSG_WriteLong(msg, -1);
    MSG_WriteString(msg, text);
    cl->downloadName[0] = '\0';
}

// 0x0808fdc2  size=1878  FUN_0808fdc2  status=translated  naming=observed
// evidence: strings "clientDownload: %d : writing block %d", "download disabled",
//           "beginning", "file not found on server", "cannot download IW iwd files".
void SV_WriteDownloadToClient(client_s *cl, msg_t *msg)
{
    char errText[0x400];
    int32 uVar2;
    int32 iwdFlag;
    int32 slot;
    int32 rateLocal;
    int32 blocksToSend;
    int32 rateBytes;
    int32 rateRounded;
    bool reachedEnd;

    if (cl->downloadName[0] == '\0' || cl->clientDownloadingWWW != 0) {
        return;
    }
    if (cl->download == 0) {
        if ((char)cv_sv_wwwDownload->current.integer != '\0' && cl->wwwOk != 0) {
            if (cl->wwwFallback == 0) {
                if (SV_RedirectClientDownload(cl, msg)) {
                    return;
                }
            } else {
                cl->wwwFallback = 0;
            }
        }
        cl->downloadingWWW = 0;
        if ((char)cv_sv_allowDownload->current.integer == '\0') {
            Com_Printf("clientDownload: %d : \"%s\" download disabled",
                        SV_ClientNum(cl), cl->downloadName);
            if ((char)cv_sv_pure->current.integer == '\0') {
                Com_sprintf(errText, 0x400, "EXE_AUTODL_SERVERDISABLED\x15%s", cl->downloadName);
            } else {
                Com_sprintf(errText, 0x400, "EXE_AUTODL_SERVERDISABLED_PURE\x15%s", cl->downloadName);
            }
            SV_SendDownloadDenied(cl, msg, errText);
            return;
        }
        iwdFlag = FS_iwIwd(cl->downloadName, "main");
        if (iwdFlag != 0) {
            Com_Printf("clientDownload: %d : \"%s\" cannot download IW iwd files\n",
                        SV_ClientNum(cl), cl->downloadName);
            Com_sprintf(errText, 0x400, "EXE_CANTAUTODLGAMEPAK\x15%s", cl->downloadName);
            SV_SendDownloadDenied(cl, msg, errText);
            return;
        }
        Com_Printf("clientDownload: %d : beginning \"%s\"\n", SV_ClientNum(cl), cl->downloadName);
        uVar2 = FS_SV_FOpenFileRead(cl->downloadName, &cl->download);
        cl->downloadSize = uVar2;
        if (cl->downloadSize <= 0) {
            Com_Printf("clientDownload: %d : \"%s\" file not found on server\n",
                        SV_ClientNum(cl), cl->downloadName);
            Com_sprintf(errText, 0x400, "EXE_AUTODL_FILENOTONSERVER\x15%s", cl->downloadName);
            SV_SendDownloadDenied(cl, msg, errText);
            return;
        }
        cl->downloadXmitBlock = 0;
        cl->downloadClientBlock = 0;
        cl->downloadCurrentBlock = 0;
        cl->downloadCount = 0;
        cl->downloadEOF = 0;
    }

    /* Fill the block window from the open file. */
    reachedEnd = false;
    for (;;) {
        if (7 < cl->downloadCurrentBlock - cl->downloadClientBlock ||
            cl->downloadSize == cl->downloadCount) {
            reachedEnd = true;
            break;
        }
        slot = cl->downloadCurrentBlock % 8;
        if (cl->downloadBlocks[slot] == NULL) {
            cl->downloadBlocks[slot] = (uint8 *)Com_MallocZeroed(0x800);
        }
        uVar2 = FS_Read(cl->downloadBlocks[slot], 0x800, cl->download);
        cl->downloadBlockSize[slot] = uVar2;
        if (cl->downloadBlockSize[slot] < 0) {
            break;
        }
        cl->downloadCount = cl->downloadCount + cl->downloadBlockSize[slot];
        cl->downloadCurrentBlock = cl->downloadCurrentBlock + 1;
    }
    if (!reachedEnd) {
        cl->downloadCount = cl->downloadSize;
    }
    if (cl->downloadCount == cl->downloadSize && cl->downloadEOF == 0 &&
        cl->downloadCurrentBlock - cl->downloadClientBlock < 8) {
        slot = cl->downloadCurrentBlock % 8;
        cl->downloadBlockSize[slot] = 0;
        cl->downloadCurrentBlock = cl->downloadCurrentBlock + 1;
        cl->downloadEOF = 1;
    }

    /* Blocks allowed this frame from the client rate and snapshot interval. */
    rateLocal = cl->rate;
    if (cv_sv_maxRate->current.integer != 0) {
        if (cv_sv_maxRate->current.integer < 1000) {
            Dvar_SetInt(cv_sv_maxRate, 1000);
        }
        if (cv_sv_maxRate->current.integer < rateLocal) {
            rateLocal = cv_sv_maxRate->current.integer;
        }
    }
    if (rateLocal == 0) {
        blocksToSend = 1;
    } else {
        rateBytes = (rateLocal * cl->snapshotMsec) / 1000;
        rateRounded = rateBytes + 0x800;
        if (rateRounded < 0) {
            rateRounded = rateBytes + 0xfff;
        }
        blocksToSend = rateRounded >> 0xb;
    }
    if (blocksToSend < 0) {
        blocksToSend = 1;
    }
    for (;;) {
        blocksToSend = blocksToSend - 1;
        if (blocksToSend == -1) {
            return;
        }
        if (cl->downloadClientBlock == cl->downloadCurrentBlock) {
            break;
        }
        if (cl->downloadXmitBlock == cl->downloadCurrentBlock) {
            if (g_svs_time - cl->downloadSendTime < 0x3e9) {
                return;
            }
            cl->downloadXmitBlock = cl->downloadClientBlock;
        }
        slot = cl->downloadXmitBlock % 8;
        MSG_WriteByte(msg, 5);
        MSG_WriteShort(msg, cl->downloadXmitBlock);
        if (cl->downloadXmitBlock == 0) {
            MSG_WriteLong(msg, cl->downloadSize);
        }
        MSG_WriteShort(msg, cl->downloadBlockSize[slot]);
        if (cl->downloadBlockSize[slot] != 0) {
            MSG_WriteData(msg, cl->downloadBlocks[slot], cl->downloadBlockSize[slot]);
        }
        Com_DPrintf("clientDownload: %d : writing block %d\n", SV_ClientNum(cl), cl->downloadXmitBlock);
        cl->downloadXmitBlock = cl->downloadXmitBlock + 1;
        cl->downloadSendTime = g_svs_time;
    }
}

// 0x08090518  size=27  FUN_08090518  status=translated  naming=observed
// evidence: ucmd table entry "disconnect" (0x0817d9e8) points here.
void SV_ClientCmd_Disconnect(client_s *cl)
{
    SV_DropClient(cl, "EXE_DISCONNECTED");
}

// 0x080907ba  size=18  FUN_080907ba  status=translated  naming=observed
// evidence: ucmd table entry "vdr" (0x0817d9f8) points here.
void SV_ClientCmd_Vdr(client_s *cl)
{
    cl->pureAuthentic = 0;
}

// 0x080907cc  size=567  FUN_080907cc  status=translated  naming=inferred
// evidence: reads userinfo keys "name", "rate", "snaps", "cl_voice", "cl_wwwDownload";
//           called from the userinfo command and SV_DirectConnect.
void SV_UserinfoChanged(client_s *cl)
{
    const char *value;
    int32 iVar3;
    int32 snaps;
    bool lanRate;

    value = Info_ValueForKey(cl->userinfo, "name");
    Q_strncpyz(cl->name, value, 0x20);
    lanRate = false;
    iVar3 = Sys_IsLANAddress(cl->netchan.remoteAddress);
    if (iVar3 != 0 && com_dedicated->current.integer != 2) {
        lanRate = true;
    }
    if (lanRate) {
        cl->rate = 99999;
    } else {
        value = Info_ValueForKey(cl->userinfo, "rate");
        if (*value == '\0') {
            cl->rate = 5000;
        } else {
            iVar3 = atoi(value);
            cl->rate = iVar3;
            if (cl->rate < 1000) {
                cl->rate = 1000;
            } else if (90000 < cl->rate) {
                cl->rate = 90000;
            }
        }
    }
    value = Info_ValueForKey(cl->userinfo, "snaps");
    if (*value == '\0') {
        cl->snapshotMsec = 0x32;
    } else {
        snaps = atoi(value);
        if (snaps < 1) {
            snaps = 1;
        } else if (0x1e < snaps) {
            snaps = 0x1e;
        }
        cl->snapshotMsec = (int32)(1000 / (int64)snaps);
    }
    value = Info_ValueForKey(cl->userinfo, "cl_voice");
    iVar3 = atoi(value);
    cl->sendVoice = (uint8)(0 < iVar3);
    if (cl->rate < 5000) {
        cl->sendVoice = 0;
    }
    value = Info_ValueForKey(cl->userinfo, "cl_wwwDownload");
    cl->wwwOk = 0;
    if (*value != '\0') {
        iVar3 = atoi(value);
        if (iVar3 != 0) {
            cl->wwwOk = 1;
        }
    }
}

// 0x08090a04  size=85  FUN_08090a04  status=translated  naming=observed
// evidence: ucmd table entry "userinfo" (0x0817d9e0) points here.
void SV_ClientCmd_UserInfo(client_s *cl)
{
    const char *value;

    value = Cmd_Argv_alt(1);
    Q_strncpyz(cl->userinfo, value, 0x400);
    SV_UserinfoChanged(cl);
    ClientUserinfoChanged(SV_ClientNum(cl));
}

// 0x08090afa  size=26  FUN_08090afa  status=translated  naming=unknown
// evidence: descriptive only. Forwards two words to FX_GetAttenuation.
float FX_GetAttenuationThunk(const float *a, const float *b)
{
    return FX_GetAttenuation(a, b);
}

// 0x08090b14  size=151  FUN_08090b14  status=translated  naming=inferred
// evidence: walks the NULL-terminated client-command table at 0x0817d9e0 matching Cmd_Argv(0);
//           falls back to the game client command hook when clientOK is set.
void SV_ExecuteClientCommand(client_s *cl, const char *s, int32 clientOK)
{
    const ucmd_entry_s *u;

    Cmd_TokenizeString_alt((char *)s);
    for (u = clientCommandTable;; u++) {
        if (u->name == NULL) {
            if (clientOK != 0 && SV_IsGameRunning() != '\0') {
                ClientCommand(SV_ClientNum(cl));
            }
            return;
        }
        if (strcmp(Cmd_Argv_alt(0), u->name) == 0) {
            u->func(cl);
            return;
        }
    }
}

// 0x08090bac  size=512  FUN_08090bac  status=translated  naming=observed
// evidence: strings "clientCommand: %i : %s", "Client %s lost %i clientCommands",
//           "EXE_LOSTRELIABLECOMMANDS", "client text ignored for %s: %s".
int32 SV_ClientCommand(client_s *cl, msg_t *msg)
{
    int32 clientOK;
    bool chatAllowed;
    int32 seq;
    const char *cmdText;
    int32 iVar4;

    clientOK = 1;
    chatAllowed = true;
    seq = MSG_ReadLong(msg);
    cmdText = MSG_ReadString(msg);
    if (seq <= cl->lastClientCommand) {
        return 1;
    }
    if ((char)cv_sv_showCommands->current.integer != '\0') {
        Com_Printf("clientCommand: %i : %s\n", seq, cmdText);
    }
    if (cl->lastClientCommand + 1 < seq) {
        Com_Printf("Client %s lost %i clientCommands\n", cl->name,
                    (seq - cl->lastClientCommand) + 1);
        SV_DropClient(cl, "EXE_LOSTRELIABLECOMMANDS");
        return 0;
    }
    iVar4 = Q_strncmp("team ", cmdText, 5);
    if (iVar4 == 0) {
        chatAllowed = false;
    } else {
        iVar4 = Q_strncmp("score ", cmdText, 6);
        if (iVar4 == 0) {
            chatAllowed = false;
        } else {
            iVar4 = Q_strncmp("mr ", cmdText, 3);
            if (iVar4 == 0) {
                chatAllowed = false;
            }
        }
    }
    if (((int *)g_0817d838)[1] == 0 && cl->state > 3 &&
        (char)cv_sv_floodProtect->current.integer != '\0' &&
        g_svs_time < cl->nextReliableTime && chatAllowed) {
        clientOK = 0;
        Com_DPrintf("client text ignored for %s: %s\n", cl->name, Cmd_Argv_alt(0));
    }
    if (chatAllowed) {
        cl->nextReliableTime = g_svs_time + 800;
    }
    SV_ExecuteClientCommand(cl, cmdText, clientOK);
    cl->lastClientCommand = seq;
    Com_sprintf(cl->lastClientCommandString, 0x400, "%s", cmdText);
    return 1;
}

// 0x08090dac  size=135  FUN_08090dac  status=translated  naming=inferred
// evidence: stores a 7-dword usercmd and, for active clients, notifies the game hooks.
void SV_UpdateClientUsercmd(client_s *cl, const struct usercmd_s *cmd)
{
    const int32 *ucmd = (const int32 *)cmd;
    int32 k;

    for (k = 0; k < 7; k++) {
        ((int32 *)&cl->lastUsercmd)[k] = ucmd[k];
    }
    if (cl->state == CS_ACTIVE) {
        Client_SetLastActivityTime(SV_ClientNum(cl), ucmd[0]);
        ClientThink(SV_ClientNum(cl));
    }
}

// 0x08090e34  size=794  FUN_08090e34  status=partial  naming=inferred
// evidence: q3-style SV_UserMove structure (cmdCount bound 32, reliable-window check, ack
//           mirror, playerstate-patched bytes, rate of applied commands). Helper argument
//           layout is verified in the disassembly (MSG_ReadDeltaUsercmdKey takes msg, key, prev, cmd).
// open: the playerstate helper CL_CreateCmd receives a 36-byte baseline scratch (ps, &base[1]);
//       the bit meaning of BG_PlayerHasWeapon(ps, byte) and the field at ps+0xd0/0xd4 are unnamed.
void SV_UserMove(client_s *cl, msg_t *msg, int32 deltaFlag)
{
    int32 cmdCount;
    int32 key;
    int32 i;
    uint32 checksum;
    uint8 *ps;
    usercmd_t baseline; /* [ebp-0x38]: CL_CreateCmd target and the first delta base */
    usercmd_t cmds[32];
    usercmd_t *prev;
    usercmd_t *cmd;

    if (deltaFlag != 0) {
        cl->deltaMessage = cl->messageAcknowledge;
    } else {
        cl->deltaMessage = -1;
    }
    if (cl->reliableSequence - cl->reliableAcknowledge >= 0x80) {
        return;
    }
    cmdCount = MSG_ReadByte(msg);
    if (cmdCount < 1) {
        Com_Printf("cmdCount < 1\n");
        return;
    }
    if (cmdCount > 0x20) {
        Com_Printf("cmdCount > MAX_PACKET_USERCMDS\n");
        return;
    }
    key = sv_checksumFeed ^ cl->messageAcknowledge;
    checksum = Com_HashKey(cl->reliableCommands[cl->reliableAcknowledge & 0x7f].command, 0x20);
    key = key ^ (int32)checksum;
    ps = SV_GameClientNum(SV_ClientNum(cl));
    CL_CreateCmd((const playerState_s *)ps, (usercmd_s *)&baseline);
    prev = &baseline;
    for (i = 0; i < cmdCount; i++) {
        cmd = &cmds[i];
        MSG_ReadDeltaUsercmdKey(msg, key, prev, cmd);
        if (BG_PlayerHasWeapon((playerState_s *)ps, cmd->weapon) == 0) {
            cmd->weapon = ps[0xd4];
        }
        if (BG_PlayerHasWeapon((playerState_s *)ps, cmd->offHandIndex) == 0) {
            cmd->offHandIndex = ps[0xd0];
        }
        prev = cmd;
    }
    cl->frames[cl->messageAcknowledge & 0x1f].messageAcked = g_svs_time; /* client+0x234f0+k*0x26c4 (0x08091038) */
    if (cl->state == CS_PRIMED) {
        SV_ClientEnterWorld(cl, (const int32 *)&cmds[0]);
    }
    if ((char)cv_sv_pure->current.integer == '\0' || cl->pureAuthentic != 0) {
        if (cl->state == CS_ACTIVE) {
            for (i = 0; i < cmdCount; i++) {
                if (cmds[i].serverTime <= cmds[cmdCount - 1].serverTime &&
                    cl->lastUsercmd.serverTime < cmds[i].serverTime) {
                    SV_UpdateClientUsercmd(cl, &cmds[i]);
                }
            }
        } else {
            cl->deltaMessage = -1;
        }
    } else {
        SV_DropClient(cl, "EXE_CANNOTVALIDATEPURECLIENT");
    }
}

// 0x0809114e  size=603  FUN_0809114e  status=translated  naming=inferred
// evidence: q3-style SV_ExecuteClientMessage. Decodes the netchan payload into a 0x20000 buffer,
//           then dispatches bytes 0 (usermove, delta), 1 (usermove), 2 (clientCommand
//           loop), 3 (no-op); reports "dropped gamestate, resending" and EXE_UNPURECLIENTDETECTED.
void SV_ExecuteClientMessage(client_s *cl, msg_t *netmsg)
{
    int32 tmp[4]; /* 16-byte pool-offset save area (was tempbuf_s) */
    msg_t msg;
    uint8 *buf;
    int32 cmdByte;
    int32 ok;

    Com_PoolAllocInto2(tmp, 0x20000);
    buf = (uint8 *)Com_PoolPtrFromOffset(tmp);
    MSG_Init(&msg, buf, 0x20000);
    msg.cursize = Huff_DecompressBytes(netmsg->data + netmsg->readcount, buf,
                              netmsg->cursize - netmsg->readcount);
    if (cl->serverId == g_sv_serverIdCounter || cl->downloadName[0] != '\0') {
        do {
            cmdByte = MSG_ReadBits(&msg, 3);
            if (cmdByte == 3 || cmdByte != 2) {
                if ((char)cv_sv_pure->current.integer != '\0' && cl->pureAuthentic == 2) {
                    cl->nextSnapshotTime = -1;
                    SV_DropClient(cl, "EXE_UNPURECLIENTDETECTED");
                    cl->state = CS_ACTIVE;
                    SV_SendClientSnapshot(cl);
                    cl->state = CS_ZOMBIE;
                }
                if (cmdByte == 0) {
                    SV_UserMove(cl, &msg, 1);
                } else if (cmdByte == 1) {
                    SV_UserMove(cl, &msg, 0);
                } else if (cmdByte != 3) {
                    Com_Printf("WARNING: bad command byte %i for client %i\n",
                                cmdByte, SV_ClientNum(cl));
                }
                Com_PoolRestoreFrom2(tmp);
                return;
            }
            ok = SV_ClientCommand(cl, &msg);
            if (ok == 0) {
                Com_PoolRestoreFrom2(tmp);
                return;
            }
        } while (cl->state != CS_ZOMBIE);
        Com_PoolRestoreFrom2(tmp);
    } else if ((cl->serverId & 0xf0) == (g_sv_serverIdCounter & 0xf0)) {
        if (cl->state == CS_PRIMED) {
            SV_ClientEnterWorld(cl, (const int32 *)&cl->lastUsercmd);
        }
        Com_PoolRestoreFrom2(tmp);
    } else {
        if (cl->gamestateMessageNum < cl->messageAcknowledge) {
            Com_DPrintf("%s : dropped gamestate, resending\n", cl->name);
            SV_SendClientGameState(cl);
        }
        Com_PoolRestoreFrom2(tmp);
    }
}

// 0x080913da  size=611  FUN_080913da  status=translated  naming=inferred
// evidence: builds a "connect ... name\bot%d ... protocol\%d" command, runs SV_DirectConnect
//           from a NA_BOT address, then sends the gamestate and enters the bot into the world.
gentity_s *SV_AddBot(void)
{
    char userinfo[1032];
    client_s *cl;
    int32 num;
    netadr_t from;
    gentity_s *result;

    num = 0;
    cl = g_svs_clients;
    while (num < cv_sv_maxclients->current.integer && cl->state != CS_FREE) {
        num++;
        cl = cl + 1;
    }
    if (num == cv_sv_maxclients->current.integer) {
        return NULL;
    }
    sprintf(userinfo,
            "connect \"\\cg_predictItems\\1\\cl_punkbuster\\0\\cl_anonymous\\0\\color\\4\\head\\default\\model\\multi\\snaps\\20\\rate\\5000\\name\\bot%d\\protocol\\%d\"",
            g_botConnectCounter, 0x76);
    Cmd_TokenizeString_alt(userinfo);
    memset(&from, 0, sizeof(from));
    from.port = (uint16)g_botConnectCounter;
    g_botConnectCounter = g_botConnectCounter + 1;
    SV_DirectConnect(from);

    num = 0;
    cl = g_svs_clients;
    while (num < cv_sv_maxclients->current.integer &&
           (cl->state == CS_FREE || NET_CompareBaseAdr(from, cl->netchan.remoteAddress) == 0)) {
        num++;
        cl = cl + 1;
    }
    if (num == cv_sv_maxclients->current.integer) {
        return NULL;
    }
    cl->bIsTestClient = 1;
    SV_SendClientGameState(cl);
    {
        usercmd_t zeroCmd;
        memset(&zeroCmd, 0, 0x1c);
        SV_ClientEnterWorld(cl, (const int32 *)&zeroCmd);
    }
    result = SV_GentityNum(num);
    return result;
}

// 0x0809163e  size=152  FUN_0809163e  status=translated  naming=unknown
// evidence: descriptive only. Case-insensitive bounded string compare (tolower on both sides,
//           raw byte equality at the end) with -1 / 0 / 1 results.
int32 Str_NCaseCmp_0809163e(const char *a, const char *b, int32 n)
{
    int32 la;
    int32 lb;

    do {
        if (n == 0 || *a == '\0') {
            if (n == 0 || *a == *b) {
                return 0;
            }
            return -1;
        }
        la = tolower((int)*a);
        lb = tolower((int)*b);
        n = n - 1;
        a = a + 1;
        b = b + 1;
        if ((char)la < (char)lb) {
            return -1;
        }
    } while ((char)la <= (char)lb);
    return 1;
}

// 0x080916d8  size=32  FUN_080916d8  status=translated  naming=inferred
// evidence: inverse of SV_GentityNum; entity index from pointer (base and stride are game globals).
int32 SV_GentityIndex(const gentity_s *ent)
{
    return (int32)(((ptrdiff_t)((const uint8 *)ent - g_0849f61c)) / g_gentitySize);
}

// 0x080916f8  size=29  FUN_080916f8  status=translated  naming=inferred
// evidence: q3 SV_GentityNum counterpart; base + num * stride from the game entity layout.
gentity_s *SV_GentityNum(int32 num)
{
    return (gentity_s *)(g_0849f61c + (ptrdiff_t)g_gentitySize * num);
}

// 0x08091716  size=29  FUN_08091716  status=translated  naming=inferred
// evidence: q3 SV_GameClientNum counterpart; returns the playerstate for a client number.
uint8 *SV_GameClientNum(int32 num)
{
    return g_gameClients + (ptrdiff_t)g_gameClientSize * num;
}

// 0x08091734  size=71  FUN_08091734  status=translated  naming=observed
// evidence: string "\x15SV_SvEntityForGentity: bad gEnt" at 0x0814abc0.
svEntity_s *SV_SvEntityForGentity(const gentity_s *ent)
{
    if (ent == NULL || ent->s.number < 0 || 0x3ff < ent->s.number) {
        Com_Error(1, "\x15" "SV_SvEntityForGentity: bad gEnt");
    }
    return &sv_svEntities[ent->s.number];
}

// 0x0809177c  size=46  FUN_0809177c  status=translated  naming=inferred
// evidence: converts an svEntity pointer back to a game entity index and resolves it; the
//           resolved pointer is returned (call; leave; ret, eax untouched; requests/L01-1).
struct gentity_s *SV_GentityNumFromSvEntity(svEntity_s *sv)
{
    return SV_GentityNum((int32)((uint32)((((ptrdiff_t)((uint8 *)sv - (uint8 *)sv_svEntities)) >> 2)) *
                                0x3f4fd3f5u));
}

// 0x080917aa  size=115  FUN_080917aa  status=translated  naming=inferred
// evidence: server command with format "%s" to one client (-1 broadcasts).
void SV_SendServerCommandNum(int32 clientNum, int32 reliable, const char *text)
{
    if (clientNum == -1) {
        SV_SendServerCommand(NULL, reliable, "%s", text);
    } else if (-1 < clientNum && clientNum < cv_sv_maxclients->current.integer) {
        SV_SendServerCommand(g_svs_clients + clientNum, reliable, "%s", text);
    }
}

// 0x0809181e  size=58  FUN_0809181e  status=translated  naming=inferred
// evidence: range-checked wrapper over SV_DropClient.
void SV_DropClientNumChecked(int32 clientNum, const char *reason)
{
    if (-1 < clientNum && clientNum < cv_sv_maxclients->current.integer) {
        SV_DropClient(g_svs_clients + clientNum, reason);
    }
}

// 0x08091858  size=119  FUN_08091858  status=translated  naming=unknown
// evidence: descriptive only. Computes the entity bounds from the model field, stores them in
//           the entity, marks it linked and calls the link helper.
void SV_SetBrushModel(gentity_s *ent)
{
    float boundsA[6];
    float boundsB[4];

    CM_ModelBounds(ent->s.index, boundsA, boundsB);
    Vec3_CopyBits_0809274e((int *)boundsA, (int *)ent->r.mins);
    Vec3_CopyBits_0809274e((int *)boundsB, (int *)ent->r.maxs);
    ent->r.bmodel = 1;
    ent->r.contents = -1;
    SV_LinkEntity(ent);
}

// 0x080918d0  size=518  FUN_080918d0  status=translated  naming=inferred
// evidence: decides whether an entity is sent to a client using its visibility flags and the
//           client's cluster bit vector; name is descriptive of that role.
bool SV_EntityVisibleToClient(float *origin, int32 entNum)
{
    gentity_s *ent;
    svEntity_s *sv;
    int32 uVar3;
    int32 bitsRet;
    int32 iVar4;
    int32 localCluster;
    int32 localBit;
    const uint8 *clusterBits;
    float limit;
    int32 rel;
    bool result;

    ent = SV_GentityNum(entNum);
    if (ent->r.linked == 0) {
        return false;
    }
    if (ent->r.broadcastTime != 0) {
        return true;
    }
    if ((ent->r.svFlags & 1) != 0) {
        return false;
    }
    if ((ent->r.svFlags & 0x18) != 0) {
        return true;
    }
    sv = SV_SvEntityForGentity(ent);
    uVar3 = CM_PointLeafnum(origin);
    if (sv->numClusters == 0) {
        return false;
    }
    bitsRet = CM_LeafCluster(uVar3);
    clusterBits = CM_ClusterPVS(bitsRet);
    localBit = 0;
    localCluster = 0;
    while (localCluster < sv->numClusters &&
           (localBit = sv->clusternums[localCluster],
            ((clusterBits[localBit >> 3] >> (localBit & 7)) & 1) == 0)) {
        localCluster = localCluster + 1;
    }
    if (localCluster == sv->numClusters) {
        if (sv->lastCluster == 0) {
            return false;
        }
        while (localBit <= sv->lastCluster &&
               ((clusterBits[localBit >> 3] >> (localBit & 7)) & 1) == 0) {
            localBit = localBit + 1;
        }
        if (localBit == sv->lastCluster) {
            return false;
        }
    }
    limit = Float_GetLevelValue();
    if (limit == 3.4028235e+38f) {
        return true;
    }
    iVar4 = Vec3_SharedSideMinSqExceeds(ent->r.absmin, ent->r.absmax, origin, limit);
    rel = iVar4;
    result = (rel == 0);
    return result;
}

// 0x08091ad6  size=505  FUN_08091ad6  status=partial  naming=unknown
// evidence: descriptive only. Three-way relevance test (trace when flags 0x60 are clear, then
//           float distance tests against two radii). The x87 long-double comparisons are done in
//           float here; the trace result byte is trace[35] (ebp-0x35 in the disassembly).
uint8 SV_EntityContact(const float *view, const float *origin, const gentity_s *ent)
{
    uint8 trace[36];
    float local_1c[3];
    float local_20;
    float local_24;
    float a;
    int32 cmpResult;

    if ((ent->r.svFlags & 0x60) == 0) {
        Sweep_TraceBoxWithOffsetInit((trace_s *)trace, vec3_origin, vec3_origin, view, origin,
                    SV_ClipHandleForEntity((gentity_s *)ent), -1, ent->r.currentOrigin, (float *)ent->r.currentAngles);
        return trace[35];
    }
    if ((ent->r.svFlags & 0x20) == 0) {
        Vec2_Add_080926fc(view, origin, local_1c);
        Vec2_Scale_08092728(local_1c, 0.5f, local_1c);
        local_20 = ((origin[0] - local_1c[0]) + ent->r.maxs[0]) - 64.0f;
        local_24 = local_20 * local_20;
        a = (float)Vec2_DistanceSquared(ent->r.currentOrigin, local_1c);
        return (uint8)((float)local_24 <= a);
    }
    if (origin[2] <= ent->r.currentOrigin[2]) {
        return 0;
    }
    if (ent->r.currentOrigin[2] + ent->r.maxs[2] <= view[2]) {
        return 0;
    }
    Vec2_Add_080926fc(view, origin, local_1c);
    Vec2_Scale_08092728(local_1c, 0.5f, local_1c);
    local_20 = ent->r.maxs[0] + (origin[0] - local_1c[0]);
    local_24 = local_20 * local_20;
    a = (float)Vec2_DistanceSquared(ent->r.currentOrigin, local_1c);
    cmpResult = (a < (float)local_24);
    return (uint8)cmpResult;
}

// 0x08091cd0  size=75  FUN_08091cd0  status=translated  naming=observed
// evidence: string "\x15SV_GetServerinfo: bufferSize == %i" at 0x0814ac00.
void SV_GetServerinfo(char *buffer, int32 bufferSize)
{
    if (bufferSize < 1) {
        Com_Error(1, "\x15" "SV_GetServerinfo: bufferSize == %i", bufferSize);
    }
    Q_strncpyz(buffer, Dvar_InfoString(0x404), bufferSize);
}

// 0x08091dac  size=45  FUN_08091dac  status=translated  naming=inferred
// evidence: stores the game entity and game client array layout (base, count, stride).
void SV_InitGameEntityLayout(uint8 *gentities, int32 numEntities, int32 gentitySize,
                             uint8 *gameClients, int32 gameClientSize)
{
    g_0849f61c = gentities;
    g_gentitySize = gentitySize;
    g_numGEntities = numEntities;
    g_gameClients = gameClients;
    g_gameClientSize = gameClientSize;
}

// 0x08091dda  size=69  FUN_08091dda  status=translated  naming=inferred
// evidence: copies the 7-dword usercmd of a client slot out to the caller.
void SV_GetClientUsercmd(int32 clientNum, struct usercmd_s *cmdOut)
{
    int32 *out = (int32 *)cmdOut;
    const int32 *src;
    int32 k;

    src = (const int32 *)&(g_svs_clients + clientNum)->lastUsercmd;
    for (k = 0; k < 7; k++) {
        out[k] = src[k];
    }
}

// 0x08091e48  size=80  FUN_08091e48  status=translated  naming=observed
// evidence: string "\x15" "bad model name '%s'" at 0x0814acfa.
struct XModel *SV_RegisterModel(const char *name)
{
    if (String_IsXModelPath(name) == '\0') {
        Com_Error(1, "\x15" "bad model name '%s'", name);
    }
    return XModel_FindOrLoad(name + 7, (void *)SV_HunkAllocCallbackA, (void *)SV_HunkAllocCallbackB);
}

// 0x08091e98  size=68  FUN_08091e98  status=translated  naming=inferred
// evidence: prints "no model.\n" when the model lookup fails; otherwise hands the model to the
//           debug draw helper. Enabled by the cvar at 0x081a2174.
void SV_PrintModelInfo(const int32 *entity)
{
    DObj *model;

    if (com_developer->current.integer != 0) {
        model = Com_GetServerDObj(*entity);
        if (model == 0) {
            Com_Printf("no model.\n");
        } else {
            DObj_DumpInfo(model);
        }
    }
}

// 0x08091edc  size=49  FUN_08091edc  status=translated  naming=inferred
// evidence: advances the skeleton-memory generation (skips 0 on wrap) and resets the pool.
void SV_SkelMemoryNewFrame(void)
{
    int32 previous;

    previous = g_skelMemGeneration;
    g_skelMemGeneration = (int32)((uint32)g_skelMemGeneration + 1u);
    if (g_skelMemGeneration == 0) {
        g_skelMemGeneration = previous + 2;
    }
    g_skelPoolPtr = g_skelPoolBase;
    g_skelPoolOffset = 0;
}

// 0x08091f0e  size=108  FUN_08091f0e  status=translated  naming=observed
// evidence: string "^3WARNING: SV_SKEL_MEMORY_SIZE exceeded" from the pool allocator.
uint8 *SV_SkelMemoryAlloc(int32 size)
{
    uint8 *block;

    for (;;) {
        block = g_skelPoolPtr + g_skelPoolOffset;
        g_skelPoolOffset = g_skelPoolOffset + (int32)(((uint32)size + 0xfu) & 0xfffffff0u);
        if (g_skelPoolOffset < 0x3fff1) {
            break;
        }
        if (g_svSkelOverflowWarnGeneration != g_skelMemGeneration) {
            g_svSkelOverflowWarnGeneration = g_skelMemGeneration;
            Com_Printf("^3WARNING: SV_SKEL_MEMORY_SIZE exceeded\n");
        }
        SV_SkelMemoryNewFrame();
    }
    return block;
}

// 0x08091f7a  size=136  FUN_08091f7a  status=translated  naming=inferred
// evidence: per-generation skeleton cache keyed by the model handle; allocates from the pool
//           on a miss and returns the cached value on a hit.
int32 SV_GetSkelForModel(const struct gentity_s *ent, int32 param2)
{
    const int32 *modelHolder = (const int32 *)ent;
    DObj *model;
    int32 cached;
    int32 size;
    uint8 *mem;
    int32 result;

    model = (DObj *)Com_GetServerDObj(*modelHolder);
    cached = DObj_SkelExists(model, g_skelMemGeneration);
    if (cached == 0) {
        size = DObj_GetAllocSkelSize(model);
        mem = SV_SkelMemoryAlloc(size);
        DObj_CreateSkel(model, (DObjSkel *)mem, g_skelMemGeneration);
        result = 0;
    } else {
        result = DObj_IsBoneLocked(model, param2);
    }
    return result;
}
