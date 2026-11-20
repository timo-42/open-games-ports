#include "cod2_types.h"
#include "chunk_022_types.h"

/* RAII wrapper for the 0x080ac58e/0x080ac5d8/0x080ac5c2 buffer object; the original
 * releases it on every exit path (including exception unwinding). */
struct ScopedBufferGuard {
    int obj;
    explicit ScopedBufferGuard(int size) { Com_PoolAllocInto2(&obj, size); }
    ~ScopedBufferGuard() { Com_PoolRestoreFrom2(&obj); }
    char *get() { return (char *)Com_PoolPtrFromOffset(&obj); }
};

// 0x080960e2  size=742  FUN_080960e2  status=translated  naming=observed
// String "SV_ReadPackets: fixing up a translated port" names the function (Q3 SV_PacketEvent counterpart).
void SV_ReadPackets(netadr_t from, msg_t *msg)
{
    if (msg->cursize < 4 || *(int *)msg->data != -1) {
        SV_SkelMemoryNewFrame();
        MSG_BeginReading(msg);
        MSG_ReadLong(msg);
        unsigned int qport = MSG_ReadShort(msg);

        client_t *cl = svs.clients;
        for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
            if (cl->state != 0
                && NET_CompareBaseAdr(from, cl->netchan.remoteAddress) != 0
                && (unsigned int)cl->netchan.qport == (qport & 0xffff)) {
                if ((short)cl->netchan.remoteAddress.port != (short)from.port) {
                    Com_Printf("SV_ReadPackets: fixing up a translated port\n");
                    cl->netchan.remoteAddress.port = from.port;
                }
                if (Netchan_Process(&cl->netchan, msg) == 0)
                    return;
                cl->serverId = MSG_ReadByte(msg);
                cl->messageAcknowledge = MSG_ReadLong(msg);
                if (cl->messageAcknowledge < 0) {
                    Com_Printf("Invalid reliableAcknowledge message from %s - reliableAcknowledge is %i\n",
                                cl->name, cl->reliableAcknowledge);
                    return;
                }
                cl->reliableAcknowledge = MSG_ReadLong(msg);
                if (0x7f < cl->reliableSequence - cl->reliableAcknowledge) {
                    Com_Printf("Out of range reliableAcknowledge message from %s - cl->reliableSequence is %i, reliableAcknowledge is %i\n",
                                cl->name, cl->reliableSequence, cl->reliableAcknowledge);
                    cl->reliableAcknowledge = cl->reliableSequence;
                    return;
                }
                SV_Netchan_Decode(cl, msg->data + msg->readcount, msg->cursize - msg->readcount);
                if (cl->state == 1)
                    return;
                cl->lastPacketTime = svs.time;
                SV_ExecuteClientMessage(cl, msg);
                g_gameContextPtr = 0;
                return;
            }
            cl++;
        }
        NET_OutOfBandPrint(1, from, "disconnect");
    } else {
        SV_ConnectionlessPacket(from, msg);
    }
}

// 0x080963c8  size=399  FUN_080963c8  status=translated  naming=inferred
// Strings "Giving %s a 999 ping - ..."; Q3 SV_CalcPings counterpart (averages sent->acked delta over the 32 frames).
void SV_CalcPings(void)
{
    for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
        client_t *cl = &svs.clients[i];
        if (cl->state == 4) {
            if (cl->gentity == 0) {
                Com_DPrintf("Giving %s a 999 ping - not a gentity\n", cl->name);
                cl->ping = 999;
            } else {
                int total = 0;
                int count = 0;
                for (int j = 0; j < 0x20; j++) {
                    if (0 < cl->frames[j].messageAcked) {
                        count++;
                        total += cl->frames[j].messageAcked - cl->frames[j].messageSent;
                    }
                }
                if (count == 0) {
                    Com_DPrintf("Giving %s a 999 ping - !count:\n", cl->name);
                    cl->ping = 999;
                } else {
                    cl->ping = total / count;
                    if (999 < cl->ping) {
                        Com_DPrintf("Giving %s a 999 ping - >999 calculated ping:\n", cl->name);
                        cl->ping = 999;
                    }
                }
            }
        } else {
            cl->ping = 999;
        }
    }
}

// 0x08096558  size=38  FUN_08096558  status=translated  naming=unknown
// Releases the 16-bit handle stored in the client slot (callers are client drop/free paths) and clears it.
void SV_ReleaseClientScriptId(client_t *cl)
{
    Scr_ReleaseVarThunk(cl->scriptId);
    cl->scriptId = 0;
}

// 0x0809657e  size=301  FUN_0809657e  status=translated  naming=inferred
// Strings "Going from CS_ZOMBIE to CS_FREE for %s" and "EXE_TIMEDOUT"; Q3 SV_CheckTimeouts counterpart.
void SV_CheckTimeouts(void)
{
    int dropPoint = svs.time + cv_sv_timeout->current.integer * -1000;
    int zombiePoint = svs.time + cv_sv_zombietime->current.integer * -1000;
    client_t *cl = svs.clients;
    for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (svs.time < cl->lastPacketTime)
            cl->lastPacketTime = svs.time;
        if (cl->bIsTestClient == 0) {
            if (cl->state == 1 && cl->lastPacketTime < zombiePoint) {
                Com_DPrintf("Going from CS_ZOMBIE to CS_FREE for %s\n", cl->name);
                cl->state = 0;
            } else if (cl->state < 2 || dropPoint <= cl->lastPacketTime) {
                cl->timeoutCount = 0;
            } else {
                cl->timeoutCount = cl->timeoutCount + 1;
                if (5 < cl->timeoutCount) {
                    SV_DropClient(cl, "EXE_TIMEDOUT");
                    cl->state = 0;
                }
            }
        }
        cl++;
    }
}

// 0x080966ac  size=165  FUN_080966ac  status=translated  naming=inferred
// Q3 SV_CheckPaused counterpart: pause request honoured only while fewer than two clients are connected.
int SV_CheckPaused(void)
{
    if (com_cl_paused->current.integer == 0)
        return 0;
    int count = 0;
    client_t *cl = svs.clients;
    for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (1 < cl->state)
            count++;
        cl++;
    }
    if (count < 2) {
        Dvar_SetInt(com_sv_paused, 1);
        return 1;
    }
    Dvar_SetInt(com_sv_paused, 0);
    return 0;
}

// 0x08096752  size=26  FUN_08096752  status=translated  naming=unknown
// Calls the pre-frame helper then the game run-frame entry with the current server time.
void SV_RunGameFrame(void)
{
    SV_SkelMemoryNewFrame();
    G_RunFrame(svs.time);
}

// 0x0809676c  size=486  FUN_0809676c  status=translated  naming=inferred
// Builds a random usercmd for a bot-type client (rand-based buttons/moves/angles) and feeds it to SV_ClientThink.
void SV_BotRandomUserCmd(client_t *cl)
{
    if (cl->gentity != 0) {
        usercmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        int clientNum = (int)(cl - svs.clients);
        byte *weaponSource = SV_GameClientNum(clientNum);
        cmd.weapon = weaponSource[0xd4];
        if (G_GetClientArchiveTime(clientNum) == 0) {
            if (Rand_Fraction() < 0.5f)
                cmd.buttons |= 1;
            if (Rand_Fraction() < 0.5f)
                cmd.buttons |= 0x28;
            if (Rand_Fraction() < 0.33f) {
                cmd.forwardmove = 0x7f;
            } else if (Rand_Fraction() < 0.5f) {
                cmd.forwardmove = (signed char)0x81;
            }
            if (Rand_Fraction() < 0.33f) {
                cmd.rightmove = 0x7f;
            } else if (Rand_Fraction() < 0.5f) {
                cmd.rightmove = (signed char)0x81;
            }
            if (Rand_Fraction() < 0.33f)
                cmd.angles[0] = (int)(360.0f * Rand_Signed());
            if (Rand_Fraction() < 0.33f)
                cmd.angles[1] = (int)(360.0f * Rand_Signed());
            if (Rand_Fraction() < 0.33f)
                cmd.angles[2] = (int)(360.0f * Rand_Signed());
        }
        cl->deltaMessage = cl->netchan.outgoingSequence - 1;
        SV_UpdateClientUsercmd(cl, &cmd);
    }
}

// 0x08096952  size=93  FUN_08096952  status=translated  naming=inferred
// Runs the random-usercmd generator for every in-use client whose remote address type is 0 (bot).
void SV_RunBotClients(void)
{
    SV_SkelMemoryNewFrame();
    client_t *cl = svs.clients;
    for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (cl->state != 0 && cl->netchan.remoteAddress.type == 0)
            SV_BotRandomUserCmd(cl);
        cl++;
    }
}

/* Original repeats this block once per failed counter check: copy the map name,
 * print the restart reason, queue "map <name>". */
static void SV_RestartMapForOverflow(const char *reason)
{
    char mapName[0x40];
    Q_strncpyz(mapName, cv_mapname->current.string, 0x40);
    Com_ServerShutdownThunk(reason);
    Cbuf_AddText(va("map %s\n", mapName));
}

// 0x080969b0  size=1114  FUN_080969b0  status=translated  naming=inferred
// Q3 SV_Frame counterpart: fixed-step server loop with snapshot-ring overflow guards ("EXE_SERVERRESTART*").
void SV_Frame(int msec)
{
    if (((char *)g_0817d838)[0xdd] == '\0') {
        if (com_sv_running->current.enabled != '\0' && SV_CheckPaused() == 0) {
            int frameMsec = (int)(1000 / (long long)cv_sv_fps->current.integer);
            g_sv_timeResidual = g_sv_timeResidual + msec;
            if (frameMsec <= g_sv_timeResidual) {
                if (svs.time >= 0x70000001) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTTIMEWRAP");
                } else if (!(g_svs_nextSnapshotEntities < 0x7ffffffe - svs.numSnapshotEntities)) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTMISC\025numSnapshotEntities");
                } else if (!(g_svs_nextCachedSnapshotEntities < 0x7fffbffe)) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTMISC\025nextCachedSnapshotEntities");
                } else if (!(g_svs_nextCachedSnapshotClients < 0x7fffeffe)) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTMISC\025nextCachedSnapshotClients");
                } else if (!(g_svs_nextArchivedSnapshotFrames < 0x7ffffb4e)) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTMISC\025nextArchivedSnapshotFrames");
                } else if (!(sv_nextArchivedSnapshotBuffer < 0x7dfffffe)) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTMISC\025nextArchivedSnapshotBuffer");
                } else if (!(sv_nextCachedSnapshotFrame < 0x7ffffdfe)) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTMISC\025nextCachedSnapshotFrames");
                } else if (!(g_svs_nextSnapshotClients < 0x7ffffffe - svs.numSnapshotClients)) {
                    SV_RestartMapForOverflow("EXE_SERVERRESTARTMISC\025numSnapshotClients");
                } else {
                    if ((dvar_modifiedFlags & 0x404) != 0) {
                        char *v = Dvar_InfoString(0x404);
                        SV_SetConfigstring(0, v);
                        dvar_modifiedFlags = dvar_modifiedFlags & 0xfffffbfb;
                    }
                    if ((dvar_modifiedFlags & 8) != 0) {
                        char *v = Dvar_InfoString_Big(8);
                        SV_SetConfigstring(1, v);
                        dvar_modifiedFlags = dvar_modifiedFlags & 0xfffffff7;
                    }
                    if ((dvar_modifiedFlags & 0x100) != 0) {
                        Dvar_ForEachWithFlags(0x8e, 0x60, 0x100);
                        dvar_modifiedFlags = dvar_modifiedFlags & 0xfffffeff;
                    }
                    SV_RunBotClients();
                    SV_CalcPings();
                    while (true) {
                        g_sv_timeResidual = g_sv_timeResidual - frameMsec;
                        svs.time = svs.time + frameMsec;
                        SV_RunGameFrame();
                        Scr_SetLoading(0);
                        if (g_sv_timeResidual < frameMsec)
                            break;
                        SV_ArchiveSnapshot();
                    }
                    SV_CheckTimeouts();
                    SV_SendClientMessages();
                    SV_ArchiveSnapshot();
                    SV_MasterHeartbeat("COD-2");
                }
            }
        }
    } else {
        if (((char *)g_0817d838)[0xde] == '\0')
            Com_ServerShutdownThunk("EXE_SERVERKILLED");
        else
            Com_ServerShutdownThunk((const char *)g_0817d838 + 0xde);
        ((char *)g_0817d838)[0xdd] = 0;
    }
}

// 0x08096e0c  size=202  FUN_08096e0c  status=translated  naming=inferred
// String "Resolving %s\n" / "cod2master.activision.com"; resolves once and caches the address in sv_masterAddress.
// Note: the original calls strstr(":", name) with swapped arguments; preserved.
netadr_t *SV_ResolveMasterAddress(void)
{
    if (sv_masterAddress.type == 0) {
        Com_Printf("Resolving %s\n", "cod2master.activision.com");
        if (NET_StringToAdr("cod2master.activision.com", &sv_masterAddress) == 0) {
            Com_Printf("Couldn't resolve address: cod2master.activision.com\n");
        } else {
            char *colon = strstr((char *)":", "cod2master.activision.com");
            if (colon == (char *)0)
                sv_masterAddress.port = (uint16)BigShort(0x50e6);
            short port = BigShort((int)(short)sv_masterAddress.port);
            Com_Printf("cod2master.activision.com resolved to %i.%i.%i.%i:%i\n",
                        sv_masterAddress.ip[0], sv_masterAddress.ip[1], sv_masterAddress.ip[2], sv_masterAddress.ip[3], (int)port);
        }
    }
    return &sv_masterAddress;
}

// 0x08096ed6  size=249  FUN_08096ed6  status=translated  naming=inferred
// Q3 SV_MasterHeartbeat counterpart; strings "Sending heartbeat to cod2master.activision.com" and "heartbeat %s".
void SV_MasterHeartbeat(const char *hbname)
{
    if (com_dedicated != 0 && com_dedicated->current.integer == 2) {
        if (svs.nextHeartbeatTime <= svs.time) {
            svs.nextHeartbeatTime = svs.time + 180000;
            netadr_t *master = SV_ResolveMasterAddress();
            if (master->type != 1) {
                Com_Printf("Sending heartbeat to cod2master.activision.com\n");
                char *text = va("heartbeat %s\n", hbname);
                NET_OutOfBandPrint(1, *master, text);
            }
        }
        if (svs.nextStatusResponseTime <= svs.time) {
            svs.nextStatusResponseTime = svs.time + 600000;
            netadr_t *master = SV_ResolveMasterAddress();
            if (master->type != 1)
                SV_StatusResponse(*master);
        }
    }
}

// 0x08096fd0  size=97  FUN_08096fd0  status=translated  naming=inferred
// String "Sending gameCompleteStatus to cod2master.activision.com".
void SV_MasterGameCompleteStatus(void)
{
    if (com_dedicated != 0 && com_dedicated->current.integer == 2) {
        netadr_t *master = SV_ResolveMasterAddress();
        if (master->type != 1) {
            Com_Printf("Sending gameCompleteStatus to cod2master.activision.com\n");
            SV_GameCompleteStatus(*master);
        }
    }
}

// 0x08097032  size=30  FUN_08097032  status=translated  naming=inferred
// Q3 SV_MasterShutdown counterpart: forces a heartbeat with the "flatline" name.
void SV_MasterShutdown(void)
{
    svs.nextHeartbeatTime = (int)0x80000000;
    SV_MasterHeartbeat("flatline");
}

// 0x08097050  size=312  FUN_08097050  status=translated  naming=inferred
// Q3 SV_FlushRedirect counterpart: sends redirected console output as "print\n%s" OOB packets in 0x50e-byte pieces.
void SV_FlushRedirect(char *outputbuf)
{
    char packet[0x514];
    size_t remaining = strlen(outputbuf);
    while (0x50e < (int)remaining) {
        char saved = outputbuf[0x50e];
        outputbuf[0x50e] = '\0';
        Com_sprintf(packet, 0x514, "print\n%s", outputbuf);
        NET_OutOfBandPrint(1, svs.redirectAddress, packet);
        remaining = remaining - 0x50e;
        outputbuf = outputbuf + 0x50e;
        *outputbuf = saved;
    }
    Com_sprintf(packet, 0x514, "print\n%s", outputbuf);
    NET_OutOfBandPrint(1, svs.redirectAddress, packet);
}

// 0x08097188  size=773  FUN_08097188  status=translated  naming=inferred
// Strings "Bad rcon from %s:\n%s\n" / "Rcon from %s:\n%s\n"; Q3 SVC_RemoteCommand counterpart.
void SVC_RemoteCommand(netadr_t from, msg_t *msg)
{
    ScopedBufferGuard outputBuffer(0x1fff0);
    char *redirectBuffer = outputBuffer.get();
    int now = Com_PumpSysEvents();
    if (svc_lastRconTime != 0 && now - svc_lastRconTime < 500)
        return;
    svc_lastRconTime = now;

    const char *password = Cmd_Argv_alt(1);
    int valid;
    if (*cv_rcon_password->current.string == '\0' || strcmp(password, cv_rcon_password->current.string) != 0) {
        valid = 0;
        const char *command = Cmd_Argv_alt(2);
        const char *addr = NET_AdrToString(from);
        Com_Printf("Bad rcon from %s:\n%s\n", addr, command);
    } else {
        valid = 1;
        const char *command = Cmd_Argv_alt(2);
        const char *addr = NET_AdrToString(from);
        Com_Printf("Rcon from %s:\n%s\n", addr, command);
    }
    svs.redirectAddress = from;
    Com_BeginRedirect(redirectBuffer, 0x1fff0, SV_FlushRedirect);
    if (*cv_rcon_password->current.string == '\0') {
        Com_Printf("The server must set 'rcon_password' for clients to use 'rcon'.\n");
    } else if (valid == 0) {
        if (*password == '\0')
            Com_Printf("You must log in with 'rcon login <password>' before using 'rcon'.\n");
        else
            Com_Printf("Invalid password.\n");
    } else {
        char commandLine[0x400 + 4];
        int length = 0;
        int capacity = 0x400;
        for (int arg = 2; arg < Cmd_Argc_alt(); arg++) {
            length = Com_AppendQuotedToken(Cmd_Argv_alt(arg), commandLine, length, capacity, 1);
            length = Com_AppendQuotedToken(" ", commandLine, length, capacity, 0);
        }
        if (length < capacity) {
            commandLine[length] = 0;
            Cmd_ExecuteStringThunk(commandLine);
            if (Q_stricmpn(commandLine, "pb_sv_", 6) == 0)
                PB_QueryServerEventNeg1();
        }
    }
    Com_EndRedirect();
}

// 0x080974d2  size=5  FUN_080974d2  status=translated  naming=unknown
// Empty function (single ret).
void SV_ExitLevelStub(void)
{
}

// 0x080974d8  size=138  FUN_080974d8  status=translated  naming=inferred
// Q3 SV_Netchan_Encode counterpart: XOR keystream from outgoingSequence^challenge and lastClientCommandString.
void SV_Netchan_Encode(client_t *cl, byte *data, int length)
{
    int stringIndex = 0;
    byte key = (byte)((byte)cl->netchan.outgoingSequence ^ (byte)cl->challenge);
    for (int i = 0; i < length; i++) {
        if (cl->lastClientCommandString[stringIndex] == '\0')
            stringIndex = 0;
        key = (byte)(key ^ (cl->lastClientCommandString[stringIndex] << (i & 1)));
        stringIndex++;
        *data = (byte)(key ^ *data);
        data++;
    }
}

// 0x08097562  size=174  FUN_08097562  status=translated  naming=inferred
// Q3 SV_Netchan_Decode counterpart: key from challenge, serverId, messageAcknowledge; string is the acked reliable command.
void SV_Netchan_Decode(client_t *cl, byte *data, int length)
{
    const char *commandString = cl->reliableCommands[(unsigned int)cl->reliableAcknowledge & 0x7f].command;
    int stringIndex = 0;
    byte key = (byte)((byte)cl->challenge ^ (byte)cl->serverId ^ (byte)cl->messageAcknowledge);
    for (int i = 0; i < length; i++) {
        if (commandString[stringIndex] == '\0')
            stringIndex = 0;
        key = (byte)(key ^ (commandString[stringIndex] << (i & 1)));
        stringIndex++;
        *data = (byte)(key ^ *data);
        data++;
    }
}

// 0x08097610  size=22  FUN_08097610  status=translated  naming=inferred
// Thin wrapper around the netchan fragment transmit (Q3 SV_Netchan_TransmitNextFragment counterpart).
unsigned char SV_Netchan_TransmitNextFragment(netchan_t *chan)
{
    return Netchan_TransmitNextFragment(chan);
}

// 0x08097626  size=72  FUN_08097626  status=translated  naming=inferred
// Q3 SV_Netchan_Transmit counterpart: encodes everything after the 4-byte header, then transmits.
unsigned char SV_Netchan_Transmit(client_t *cl, byte *data, int length)
{
    SV_Netchan_Encode(cl, data + 4, length - 4);
    return Netchan_Transmit(&cl->netchan, length, data);
}

// 0x0809766e  size=59  FUN_0809766e  status=translated  naming=unknown
// Records an out-of-band traffic sample in the OOB net profile when profiling is enabled.
void SV_NetProfile_RecordOOB(int bytes)
{
    if (cvar_net_profile->current.integer != 0) {
        NetProfile_Sync(&svs.OOBProf);
        NetProfile_AddSample(&svs.OOBProf->send, bytes, 0); /* send half is at offset 0 */
    }
}

// 0x0809771a  size=172  FUN_0809771a  status=translated  naming=unknown
// Refreshes both halves of the OOB profile and of every in-use client profile.
void SV_NetProfile_Update(void)
{
    if (svs.clients != 0) {
        if (svs.OOBProf != 0) {
            NetProfile_ComputeStats(&svs.OOBProf->send);
            NetProfile_ComputeStats(&svs.OOBProf->recieve);
        }
        client_t *cl = svs.clients;
        for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
            if (cl->state != 0 && cl->netchan.prof != 0) {
                NetProfile_ComputeStats(&cl->netchan.prof->send);
                NetProfile_ComputeStats(&cl->netchan.prof->recieve);
            }
            cl++;
        }
    }
}

// 0x080977c6  size=2767  FUN_080977c6  status=translated  naming=inferred
// Strings "Server Network Profile:" and the table header; prints (when param != 0) a per-client bandwidth table.
// The original passes two double values to "%5i" conversions in the totals/OOB/client rows (printf misuse); preserved.
void SV_PrintNetworkProfile(int print)
{
    char line[0x400];
    char name16[17];
    int sentBps = 0, recvBps = 0;
    int sentFragDen = 0, sentFragNum = 0;
    int recvFragDen = 0, recvFragNum = 0;
    int maxSent = 0, minSent = 9999;
    int maxRecv = 0, minRecv = 9999;
    int pct;
    int dropPct;
    int gapRun;

    if (svs.clients == 0)
        return;

    SV_NetProfile_Update();
    if (print != 0)
        Com_Printf("\n\n");
    Com_sprintf(line, 0x400, "====================");
    if (print != 0)
        Com_Printf("%s\n", line);
    Com_sprintf(line, 0x400, "Server Network Profile:");
    if (print != 0)
        Com_Printf("%s\n\n", line);
    Com_sprintf(line, 0x400,
                "                    | Sent To                         | Recieved From          | Total Source Traffic   |");
    if (print != 0)
        Com_Printf("%s\n", line);
    Com_sprintf(line, 0x400,
                "              Source|   bps|  max|  min|frag%%|drop%%|ak|   bps|  max|  min|frag%%|   bps|  max|  min|frag%%|");
    if (print != 0)
        Com_Printf("%s\n", line);

    netProfile_t *prof;
    if (svs.OOBProf != 0) {
        prof = svs.OOBProf;
        sentBps += prof->send.iBytesPerSecond;
        sentFragDen += prof->send.iCountedPackets;
        sentFragNum += prof->send.iCountedFragments;
        recvBps += prof->recieve.iBytesPerSecond;
        recvFragDen += prof->recieve.iCountedPackets;
        recvFragNum += prof->recieve.iCountedFragments;
        if (maxSent < prof->send.iLargestPacket) maxSent = prof->send.iLargestPacket;
        if (prof->send.iSmallestPacket < minSent) minSent = prof->send.iSmallestPacket;
        if (maxRecv < prof->recieve.iLargestPacket) maxRecv = prof->recieve.iLargestPacket;
        if (prof->recieve.iSmallestPacket < minRecv) minRecv = prof->recieve.iSmallestPacket;
    }
    client_t *cl = svs.clients;
    for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (cl->state != 0 && cl->netchan.prof != 0) {
            prof = cl->netchan.prof;
            sentBps += prof->send.iBytesPerSecond;
            sentFragDen += prof->send.iCountedPackets;
            sentFragNum += prof->send.iCountedFragments;
            recvBps += prof->recieve.iBytesPerSecond;
            recvFragDen += prof->recieve.iCountedPackets;
            recvFragNum += prof->recieve.iCountedFragments;
            if (maxSent < prof->send.iLargestPacket) maxSent = prof->send.iLargestPacket;
            if (prof->send.iSmallestPacket < minSent) minSent = prof->send.iSmallestPacket;
            if (maxRecv < prof->recieve.iLargestPacket) maxRecv = prof->recieve.iLargestPacket;
            if (prof->recieve.iSmallestPacket < minRecv) minRecv = prof->recieve.iSmallestPacket;
        }
        cl++;
    }

    if (recvFragDen + sentFragDen < 1 || recvFragNum + sentFragNum < 1)
        pct = 0;
    else
        pct = ((sentFragNum + recvFragNum) * 100) / (sentFragDen + recvFragDen);
    dropPct = 0;
    gapRun = 0;
    {
        double minTotal = (double)Float_Min_080982ba((float)minSent, (float)minRecv);
        double maxTotal = (double)Float_Max_08098296((float)maxSent, (float)maxRecv);
        int total = recvBps + sentBps;
        int recvFragPct = (recvFragDen == 0) ? 0 : (recvFragNum * 100) / recvFragDen;
        int sentFragPct = (sentFragDen == 0) ? 0 : (sentFragNum * 100) / sentFragDen;
        Com_sprintf(line, 0x400,
                    "              Totals:%6i|%5i|%5i| %3i%%| %3i%%|%2i|%6i|%5i|%5i| %3i%%|%6i|%5i|%5i| %3i%%|",
                    sentBps, maxSent, minSent, sentFragPct, dropPct, gapRun,
                    recvBps, maxRecv, minRecv, recvFragPct, total, maxTotal, minTotal, pct);
    }
    if (print != 0)
        Com_Printf("%s\n", line);

    if (svs.OOBProf == 0) {
        Com_sprintf(line, 0x400,
                    "  OutOfBand Messages:     0|    0|    0|   - |   - |  |     0|    0|    0|   - |     0|    0|    0|   - |");
        if (print != 0)
            Com_Printf("%s\n", line);
    } else {
        prof = svs.OOBProf;
        if (prof->recieve.iCountedPackets + prof->send.iCountedPackets < 1
            || prof->recieve.iCountedFragments + prof->send.iCountedFragments < 1)
            pct = 0;
        else
            pct = ((prof->send.iCountedFragments + prof->recieve.iCountedFragments) * 100)
                  / (prof->send.iCountedPackets + prof->recieve.iCountedPackets);
        dropPct = 0;
        gapRun = 0;
        double minTotal = (double)Float_Min_080982ba((float)prof->send.iSmallestPacket, (float)prof->recieve.iSmallestPacket);
        double maxTotal = (double)Float_Max_08098296((float)prof->send.iLargestPacket, (float)prof->recieve.iLargestPacket);
        int total = prof->recieve.iBytesPerSecond + prof->send.iBytesPerSecond;
        Com_sprintf(line, 0x400,
                    "  OutOfBand Messages: %5i|%5i|%5i| %3i%%| %3i%%|%2i| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
                    prof->send.iBytesPerSecond, prof->send.iLargestPacket, prof->send.iSmallestPacket, prof->send.iFragmentPercentage,
                    dropPct, gapRun,
                    prof->recieve.iBytesPerSecond, prof->recieve.iLargestPacket, prof->recieve.iSmallestPacket, prof->recieve.iFragmentPercentage,
                    total, maxTotal, minTotal, pct);
        if (print != 0)
            Com_Printf("%s\n", line);
    }

    cl = svs.clients;
    for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (cl->state != 0) {
            strncpy(name16, cl->name, 0x11);
            name16[16] = 0;
            prof = cl->netchan.prof;
            if (prof == 0) {
                Com_sprintf(line, 0x400,
                            "#%2i-%16s:     0|    0|    0|   0%%|     0|    0|    0|   0%%|     0|    0|    0|   0%%|",
                            i, name16);
                if (print != 0)
                    Com_Printf("%s\n", line);
            } else {
                if (prof->recieve.iCountedPackets + prof->send.iCountedPackets < 1
                    || prof->recieve.iCountedFragments + prof->send.iCountedFragments < 1)
                    pct = 0;
                else
                    pct = ((prof->send.iCountedFragments + prof->recieve.iCountedFragments) * 100)
                          / (prof->send.iCountedPackets + prof->recieve.iCountedPackets);
                /* locate the oldest frame: first frame whose messageSent is not larger than its predecessor */
                int oldest = 0;
                int newestSent = 0;
                while (oldest < 0x20 && newestSent < cl->frames[oldest].messageSent) {
                    int prev = oldest;
                    oldest = oldest + 1;
                    newestSent = cl->frames[prev].messageSent;
                }
                int ackedCount = 0;
                int gapTotal = 0;
                gapRun = 0;
                for (int k = 0; k < 0x20; k++) {
                    if (cl->frames[(oldest + k) & 0x1f].messageAcked < 1) {
                        gapRun++;
                    } else {
                        gapTotal += gapRun;
                        gapRun = 0;
                        ackedCount++;
                    }
                }
                if (ackedCount == 0)
                    dropPct = 100;
                else
                    dropPct = (gapTotal * 100) / (gapTotal + ackedCount);
                double minTotal = (double)Float_Min_080982ba((float)prof->send.iSmallestPacket, (float)prof->recieve.iSmallestPacket);
                double maxTotal = (double)Float_Max_08098296((float)prof->send.iLargestPacket, (float)prof->recieve.iLargestPacket);
                int total = prof->recieve.iBytesPerSecond + prof->send.iBytesPerSecond;
                Com_sprintf(line, 0x400,
                            "#%2i-%16s: %5i|%5i|%5i| %3i%%| %3i%%|%2i| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
                            i, name16,
                            prof->send.iBytesPerSecond, prof->send.iLargestPacket, prof->send.iSmallestPacket, prof->send.iFragmentPercentage,
                            dropPct, gapRun,
                            prof->recieve.iBytesPerSecond, prof->recieve.iLargestPacket, prof->recieve.iSmallestPacket, prof->recieve.iFragmentPercentage,
                            total, maxTotal, minTotal, pct);
                if (print != 0)
                    Com_Printf("%s\n", line);
            }
        }
        cl++;
    }
}

// 0x08098296  size=36  FUN_08098296  status=translated  naming=inferred
// Maximum of two floats via the select helper (selects a unless a-b is negative).
float Float_Max_08098296(float a, float b)
{
    return Float_SelectNonNegative_080982de(a - b, a, b);
}

// 0x080982ba  size=36  FUN_080982ba  status=translated  naming=inferred
// Minimum of two floats via the select helper.
float Float_Min_080982ba(float a, float b)
{
    return Float_SelectNonNegative_080982de(b - a, a, b);
}

// 0x080982de  size=41  FUN_080982de  status=translated  naming=inferred
// Returns ifNonNegative when selector >= 0.0, otherwise ifNegative.
float Float_SelectNonNegative_080982de(float selector, float ifNonNegative, float ifNegative)
{
    float result;
    if (0.0 <= selector)
        result = ifNonNegative;
    else
        result = ifNegative;
    return result;
}

// 0x08098308  size=372  FUN_08098308  status=translated  naming=inferred
// Q3 SV_EmitPacketEntities counterpart: merge-walks old and new entity lists (sorted by number) and writes deltas;
// terminated by entity number 0x3ff in 10 bits. The first argument is unused.
void SV_EmitPacketEntities(int clientNum, int oldCount, int oldFirst, int newCount, int newFirst, msg_t *msg)
{
    entityState_t *newEnt = 0;
    entityState_t *oldEnt = 0;
    int newIndex = 0;
    int oldIndex = 0;
    while (newIndex < newCount || oldIndex < oldCount) {
        int newNumber;
        int oldNumber;
        if (newIndex < newCount) {
            newEnt = (entityState_t *)(((byte *)svs.snapshotEntities) + ((newIndex + newFirst) % svs.numSnapshotEntities) * 0xf0);
            newNumber = newEnt->number;
        } else {
            newNumber = 9999;
        }
        if (oldIndex < oldCount) {
            oldEnt = (entityState_t *)(((byte *)svs.snapshotEntities) + ((oldIndex + oldFirst) % svs.numSnapshotEntities) * 0xf0);
            oldNumber = oldEnt->number;
        } else {
            oldNumber = 9999;
        }
        if (newNumber == oldNumber) {
            MSG_WriteDeltaEntity(msg, oldEnt, newEnt, 0);
            oldIndex++;
            newIndex++;
        } else if (newNumber < oldNumber) {
            MSG_WriteDeltaEntity(msg, &sv_svEntities[newNumber].baseline.s, newEnt, 1);
            newIndex++;
        } else if (oldNumber < newNumber) {
            MSG_WriteDeltaEntity(msg, oldEnt, 0, 1);
            oldIndex++;
        }
    }
    MSG_WriteBits(msg, 0x3ff, 10);
}

// 0x0809847c  size=332  FUN_0809847c  status=translated  naming=inferred
// Client-state counterpart of SV_EmitPacketEntities (0x5c-byte records, ring size svs.numSnapshotClients); no baseline source.
void SV_EmitPacketClients(int clientNum, int oldCount, int oldFirst, int newCount, int newFirst, msg_t *msg)
{
    clientState_t *newCs = 0;
    clientState_t *oldCs = 0;
    int newIndex = 0;
    int oldIndex = 0;
    while (newIndex < newCount || oldIndex < oldCount) {
        int newNumber;
        int oldNumber;
        if (newIndex < newCount) {
            newCs = (clientState_t *)(((byte *)svs.snapshotClients) + ((newIndex + newFirst) % svs.numSnapshotClients) * 0x5c);
            newNumber = newCs->clientNum;
        } else {
            newNumber = 9999;
        }
        if (oldIndex < oldCount) {
            oldCs = (clientState_t *)(((byte *)svs.snapshotClients) + ((oldIndex + oldFirst) % svs.numSnapshotClients) * 0x5c);
            oldNumber = oldCs->clientNum;
        } else {
            oldNumber = 9999;
        }
        if (newNumber == oldNumber) {
            MSG_WriteDeltaClientState(msg, oldCs, newCs, 0);
            oldIndex++;
            newIndex++;
        } else if (newNumber < oldNumber) {
            MSG_WriteDeltaClientState(msg, 0, newCs, 1);
            newIndex++;
        } else if (oldNumber < newNumber) {
            MSG_WriteDeltaClientState(msg, oldCs, 0, 1);
            oldIndex++;
        }
    }
    MSG_WriteBit0(msg);
}

// 0x080985c8  size=745  FUN_080985c8  status=translated  naming=inferred
// Strings "(writing snapshot) Snapshot delta request from out of date packet."; Q3 SV_WriteSnapshotToClient counterpart.
void SV_WriteSnapshotToClient(client_t *cl, msg_t *msg)
{
    unsigned int frameIndex = (unsigned int)cl->netchan.outgoingSequence & 0x1f;
    clientSnapshot_t *oldFrame;
    int deltaNum;

    if (cl->deltaMessage < 1 || cl->state != 4) {
        oldFrame = 0;
        deltaNum = 0;
    } else if (cl->netchan.outgoingSequence - cl->deltaMessage < 0x1d) {
        oldFrame = &cl->frames[(unsigned int)cl->deltaMessage & 0x1f];
        deltaNum = cl->netchan.outgoingSequence - cl->deltaMessage;
        if (oldFrame->first_entity < g_svs_nextSnapshotEntities - svs.numSnapshotEntities) {
            Com_DPrintf("%s: Delta request from out of date entities.\n", cl->name);
            oldFrame = 0;
            deltaNum = 0;
        }
    } else {
        Com_DPrintf("%s: (writing snapshot) Snapshot delta request from out of date packet.\n", cl->name);
        oldFrame = 0;
        deltaNum = 0;
    }

    MSG_WriteByte(msg, 6);
    MSG_WriteLong(msg, svs.time);
    MSG_WriteByte(msg, deltaNum);
    unsigned int snapFlags = (unsigned int)g_svs_snapFlags;
    if (cl->rateDelayed != 0)
        snapFlags = (unsigned int)g_svs_snapFlags | 1;
    if (cl->state == 4)
        cl->sendAsActive = 1;
    else if (cl->state != 1)
        cl->sendAsActive = 0;
    if (cl->sendAsActive == 0)
        snapFlags = snapFlags | 2;
    MSG_WriteByte(msg, (int)snapFlags);

    int oldNumEntities;
    int oldFirstEntity;
    int oldNumClients;
    int oldFirstClient;
    if (oldFrame == 0) {
        MSG_WriteDeltaPlayerState(msg, 0, (const unsigned char *)&cl->frames[frameIndex].ps);
        oldNumEntities = 0;
        oldFirstEntity = 0;
        oldNumClients = 0;
        oldFirstClient = 0;
    } else {
        MSG_WriteDeltaPlayerState(msg, (const unsigned char *)&oldFrame->ps, (const unsigned char *)&cl->frames[frameIndex].ps);
        oldNumEntities = oldFrame->num_entities;
        oldFirstEntity = oldFrame->first_entity;
        oldNumClients = oldFrame->num_clients;
        oldFirstClient = oldFrame->first_client;
    }
    int clientNum = (int)(cl - svs.clients);
    SV_EmitPacketEntities(clientNum, oldNumEntities, oldFirstEntity,
                          cl->frames[frameIndex].num_entities, cl->frames[frameIndex].first_entity, msg);
    SV_EmitPacketClients(clientNum, oldNumClients, oldFirstClient,
                         cl->frames[frameIndex].num_clients, cl->frames[frameIndex].first_client, msg);
    for (int i = 0; i < cv_sv_padPackets->current.integer; i++)
        MSG_WriteByte(msg, 0);
}

// 0x080988b2  size=270  FUN_080988b2  status=translated  naming=inferred
// String "Client %s has the following un-ack'd reliable commands:"; Q3 SV_UpdateServerCommandsToClient counterpart.
void SV_UpdateServerCommandsToClient(client_t *cl, msg_t *msg)
{
    if (cl->reliableAcknowledge + 1 < cl->reliableSequence && cv_sv_debugReliableCmds->current.enabled != '\0')
        Com_Printf("Client %s has the following un-ack'd reliable commands:\n", cl->name);
    unsigned int i = (unsigned int)cl->reliableAcknowledge;
    while (i = i + 1, (int)i <= cl->reliableSequence) {
        MSG_WriteByte(msg, 4);
        MSG_WriteLong(msg, (int)i);
        MSG_WriteString(msg, cl->reliableCommands[i & 0x7f].command);
        if (cv_sv_debugReliableCmds->current.enabled != '\0')
            Com_Printf("%i: %s\n", (int)(i - cl->reliableAcknowledge) - 1, cl->reliableCommands[i & 0x7f].command);
    }
    cl->reliableSent = cl->reliableSequence;
}

// 0x080989c0  size=211  FUN_080989c0  status=translated  naming=inferred
// Size-limited variant: stops when strlen+6+cursize would reach maxSize; reliableSent only moves forward.
void SV_UpdateServerCommandsToClientLimited(client_t *cl, msg_t *msg, int maxSize)
{
    unsigned int last = (unsigned int)cl->reliableAcknowledge;
    unsigned int next;
    while (next = last + 1,
           (int)next <= cl->reliableSequence
           && (int)(strlen(cl->reliableCommands[next & 0x7f].command) + 6 + msg->cursize) < maxSize) {
        MSG_WriteByte(msg, 4);
        MSG_WriteLong(msg, (int)next);
        MSG_WriteString(msg, cl->reliableCommands[next & 0x7f].command);
        last = next;
    }
    if (cl->reliableSent < (int)last)
        cl->reliableSent = (int)last;
}

// 0x08098a94  size=183  FUN_08098a94  status=translated  naming=inferred
// String "-- Unacknowledged Server Commands for client %i:%s --"; debug dump of pending reliable commands.
void SV_PrintUnackedServerCommands(client_t *cl)
{
    unsigned int clientNum = (unsigned int)(cl - svs.clients);
    Com_Printf("-- Unacknowledged Server Commands for client %i:%s --\n", clientNum, cl->name);
    unsigned int i = (unsigned int)cl->reliableAcknowledge;
    int lastTime = 0;
    unsigned int lastSeq = clientNum;
    while (i = i + 1, (int)i <= cl->reliableSequence) {
        lastTime = cl->reliableCommands[i & 0x7f].cmdTime;
        lastSeq = i;
        Com_Printf("cmd %5d: %8d: %s\n", i, lastTime, cl->reliableCommands[i & 0x7f].command);
    }
    Com_Printf("----------", lastSeq, lastTime);
}

// 0x08098b4c  size=38  FUN_08098b4c  status=translated  naming=unknown
// Appends a value to a counted int list (count at [0], max 0x400 entries); used by the client visibility scan.
void SV_AppendToIntList_ClientScan(int value, int *list)
{
    if (*list != 0x400) {
        list[*list + 1] = value;
        *list = *list + 1;
    }
}

// 0x08098b72  size=38  FUN_08098b72  status=translated  naming=unknown
// Identical twin of the function above, used by the cached-entity visibility scan.
void SV_AppendToIntList_EntityScan(int value, int *list)
{
    if (*list != 0x400) {
        list[*list + 1] = value;
        *list = *list + 1;
    }
}

// 0x08098b98  size=665  FUN_08098b98  status=translated  naming=unknown
// For a world point: collects the client numbers (excluding excludeNum) whose gentity is flagged always-visible
// or whose clusters are in the PVS of the point's cluster, optionally limited by a range test.
void SV_BuildClientListForPoint(const float *point, int excludeNum, int *outList)
{
    int leaf = CM_PointLeafnum((float *)point);
    int cluster = CM_LeafCluster(leaf);
    if (-1 < cluster) {
        byte *pvs = (byte *)CM_ClusterPVS(cluster);
        float range = Float_GetLevelValue();
        if (range == 3.4028235e+38f)
            range = 0.0f;
        for (int i = 0; i < g_numGEntities; i++) {
            gentity_t *ent = SV_GentityNum(i);
            if (ent->r.linked == '\0' || i == excludeNum)
                continue;
            bool testVisibility = false;
            if (ent->r.broadcastTime == 0) {
                if ((ent->r.svFlags & 1) == 0
                    && (((ent->r.clientMask[excludeNum >> 5] >> (excludeNum & 0x1f)) & 1U) == 0))
                    testVisibility = true;
            } else {
                if (-1 < ent->r.broadcastTime && ent->r.broadcastTime - svs.time < 0) {
                    ent->r.broadcastTime = 0;
                    testVisibility = true;
                } else {
                    SV_AppendToIntList_ClientScan(i, outList);
                }
            }
            if (!testVisibility)
                continue;
            if ((ent->r.svFlags & 0x18) == 0) {
                svEntity_t *svEnt = (svEntity_t *)SV_SvEntityForGentity(ent);
                if (svEnt->numClusters != 0) {
                    int c = 0;
                    int cl2 = 0;
                    while (c < svEnt->numClusters) {
                        cl2 = svEnt->clusternums[c];
                        if (((int)(unsigned int)pvs[cl2 >> 3] >> (cl2 & 7) & 1U) != 0)
                            break;
                        c++;
                    }
                    bool addCandidate;
                    if (c == svEnt->numClusters) {
                        addCandidate = false;
                        if (svEnt->lastCluster != 0) {
                            while (cl2 <= svEnt->lastCluster
                                   && (((int)(unsigned int)pvs[cl2 >> 3] >> (cl2 & 7) & 1U) == 0))
                                cl2++;
                            if (cl2 != svEnt->lastCluster)
                                addCandidate = true;
                        }
                    } else {
                        addCandidate = true;
                    }
                    if (addCandidate) {
                        if (range == 0.0f
                            || Vec3_SharedSideMinSqExceeds(ent->r.absmin, ent->r.absmax, point, range) == 0)
                            SV_AppendToIntList_ClientScan(i, outList);
                    }
                }
            } else {
                SV_AppendToIntList_ClientScan(i, outList);
            }
        }
    }
}

// 0x08098e32  size=584  FUN_08098e32  status=translated  naming=unknown
// Same scan over a window of the cached-entity ring (count entries starting at first): collects entry indices whose
// box touches a cluster visible from the point, skipping masked/own entities.
void SV_BuildCachedEntityListForPoint(int count, int first, const float *point, int excludeNum, int *outList)
{
    int leafList[0x82];
    int topNode;
    int leaf = CM_PointLeafnum((float *)point);
    int cluster = CM_LeafCluster(leaf);
    if (-1 < cluster) {
        byte *pvs = (byte *)CM_ClusterPVS(cluster);
        float range = Float_GetLevelValue();
        if (range == 3.4028235e+38f)
            range = 0.0f;
        for (int i = 0; i < count; i++) {
            archivedEntity_t *ent = (archivedEntity_t *)(((byte *)svs.cachedSnapshotEntities) + ((first + i) % 0x4000) * 0x114);
            if (((ent->r.clientMask[excludeNum >> 5] >> (excludeNum & 0x1f)) & 1U) == 0
                && ent->s.number != excludeNum) {
                if ((ent->r.svFlags & 0x18U) == 0) {
                    int n = CM_BoxLeafnums(ent->r.absmin, ent->r.absmax, leafList, 0x80, &topNode);
                    if (n != 0) {
                        int k = 0;
                        while (k < n) {
                            int c = CM_LeafCluster(leafList[k]);
                            if (c != -1 && (((int)(unsigned int)pvs[c >> 3] >> (c & 7) & 1U) != 0))
                                break;
                            k++;
                        }
                        if (k != n
                            && (range == 0.0f
                                || Vec3_SharedSideMinSqExceeds(ent->r.absmin, ent->r.absmax, point, range) == 0))
                            SV_AppendToIntList_EntityScan(i, outList);
                    }
                } else {
                    SV_AppendToIntList_EntityScan(i, outList);
                }
            }
        }
    }
}

static cachedClient_t *SV_CachedClientAt(int index)
{
    return (cachedClient_t *)(((byte *)svs.cachedSnapshotClients) + (index % 0x1000) * 0x2708);
}

static cachedSnapshot_t *SV_CachedFrameAt(int index)
{
    return &svs.cachedSnapshotFrames[index % 0x200];
}

// 0x0809907a  size=2898  FUN_0809907a  status=translated  naming=observed
// String "SV_GetCachedSnapshot: end of message" names the function. Rebuilds (and caches) an archived snapshot frame,
// recursively rebuilding the delta base first. Returns 0 when the frame has been overwritten in the ring buffers.
cachedSnapshot_t *SV_GetCachedSnapshot(int archivedFrame)
{
    ScopedBufferGuard scratch(0x20000);
    byte *scratchData = (byte *)scratch.get();

    archivedSnapshot_t *archive = &svs.archivedSnapshotFrames[archivedFrame % 0x4b0];
    if (archive->start < sv_nextArchivedSnapshotBuffer - 0x2000000)
        return 0;

    /* look for an already cached frame among the newest 0x200 */
    int searchLimit = sv_nextCachedSnapshotFrame - 0x200;
    int searchIndex = sv_nextCachedSnapshotFrame;
    cachedSnapshot_t *found = 0;
    cachedSnapshot_t *slot = 0;
    if (searchLimit < 0)
        searchLimit = 0;
    bool needBuild = false;
    for (;;) {
        int current = searchIndex - 1;
        if (current < searchLimit) {
            needBuild = true;
            break;
        }
        found = SV_CachedFrameAt(current);
        slot = found;
        searchIndex = current;
        if (found->archivedFrame == archivedFrame)
            break;
    }
    if (!needBuild) {
        if (found->first_entity < g_svs_nextCachedSnapshotEntities - 0x4000 || found->first_client < g_svs_nextCachedSnapshotClients - 0x1000)
            needBuild = true;
        else
            return found;
    }

    /* rebuild from the archive ring */
    msg_t msg;
    MSG_Init(&msg, scratchData, 0x20000);
    msg.cursize = archive->size;
    int ringOffset = archive->start % 0x2000000;
    int firstPart = 0x2000000 - ringOffset;
    if (firstPart < msg.cursize) {
        memcpy(msg.data, svs.archivedSnapshotBuffer + ringOffset, firstPart);
        memcpy(msg.data + firstPart, svs.archivedSnapshotBuffer, msg.cursize - firstPart);
    } else {
        memcpy(msg.data, svs.archivedSnapshotBuffer + ringOffset, msg.cursize);
    }

    /* SV_ArchiveSnapshot writes bit 1 + time for a full frame and bit 0 + base frame + time for a delta frame; the
       original tests the bit with "jne full" at 0x080992b5 (bit 0 falls through to the delta path) */
    if (MSG_ReadBit(&msg) != 0) {
        /* full (non-delta) frame */
        slot = SV_CachedFrameAt(sv_nextCachedSnapshotFrame);
        slot->archivedFrame = archivedFrame;
        slot->num_entities = 0;
        slot->first_entity = g_svs_nextCachedSnapshotEntities;
        slot->num_clients = 0;
        slot->first_client = g_svs_nextCachedSnapshotClients;
        slot->usesDelta = 0;
        slot->time = MSG_ReadLong(&msg);
        while (MSG_ReadBit(&msg) != 0) {
            int clientNum = MSG_ReadBits(&msg, 6);
            if (msg.cursize < msg.readcount)
                Com_Error(1, "\025SV_GetCachedSnapshot: end of message");
            cachedClient_t *newCl = SV_CachedClientAt(g_svs_nextCachedSnapshotClients);
            MSG_ReadDeltaClientState(&msg, 0, &newCl->cs, clientNum);
            newCl->playerStateExists = MSG_ReadBit(&msg);
            if (newCl->playerStateExists != 0)
                MSG_ReadDeltaPlayerState(&msg, 0, (unsigned char *)&newCl->ps);
            g_svs_nextCachedSnapshotClients = g_svs_nextCachedSnapshotClients + 1;
            if (0x7ffffffd < g_svs_nextCachedSnapshotClients)
                Com_Error(0, "\025svs.nextCachedSnapshotClients wrapped");
            slot->num_clients = slot->num_clients + 1;
        }
    } else {
        /* delta frame: base is another archived frame */
        int baseFrame = MSG_ReadLong(&msg);
        if (baseFrame < g_svs_nextArchivedSnapshotFrames - 0x4b0)
            return 0;
        archivedSnapshot_t *baseArchive = &svs.archivedSnapshotFrames[baseFrame % 0x4b0];
        if (baseArchive->start < sv_nextArchivedSnapshotBuffer - 0x2000000)
            return 0;
        cachedSnapshot_t *base = SV_GetCachedSnapshot(baseFrame);
        if (base == 0)
            return 0;
        slot = SV_CachedFrameAt(sv_nextCachedSnapshotFrame);
        slot->archivedFrame = archivedFrame;
        slot->num_entities = 0;
        slot->first_entity = g_svs_nextCachedSnapshotEntities;
        slot->num_clients = 0;
        slot->first_client = g_svs_nextCachedSnapshotClients;
        slot->usesDelta = 1;
        slot->time = MSG_ReadLong(&msg);

        int baseIndex = 0;
        cachedClient_t *baseCl = 0;
        int baseNumber;
        if (base->num_clients < 1) {
            baseNumber = 99999;
        } else {
            baseCl = SV_CachedClientAt(base->first_client);
            baseNumber = baseCl->cs.clientNum;
        }
        while (MSG_ReadBit(&msg) != 0) {
            int clientNum = MSG_ReadBits(&msg, 6);
            if (msg.cursize < msg.readcount)
                Com_Error(1, "\025SV_GetCachedSnapshot: end of message");
            while (baseNumber < clientNum) {
                baseIndex = baseIndex + 1;
                if (baseIndex < base->num_clients) {
                    baseCl = SV_CachedClientAt(base->first_client + baseIndex);
                    baseNumber = baseCl->cs.clientNum;
                } else {
                    baseNumber = 99999;
                }
            }
            cachedClient_t *newCl = SV_CachedClientAt(g_svs_nextCachedSnapshotClients);
            if (baseNumber == clientNum) {
                MSG_ReadDeltaClientState(&msg, &baseCl->cs, &newCl->cs, clientNum);
                newCl->playerStateExists = MSG_ReadBit(&msg);
                if (newCl->playerStateExists != 0)
                    MSG_ReadDeltaPlayerState(&msg, (const unsigned char *)&baseCl->ps, (unsigned char *)&newCl->ps);
                g_svs_nextCachedSnapshotClients = g_svs_nextCachedSnapshotClients + 1;
                if (0x7ffffffd < g_svs_nextCachedSnapshotClients)
                    Com_Error(0, "\025svs.nextCachedSnapshotClients wrapped");
                slot->num_clients = slot->num_clients + 1;
                baseIndex = baseIndex + 1;
                if (baseIndex < base->num_clients) {
                    baseCl = SV_CachedClientAt(base->first_client + baseIndex);
                    baseNumber = baseCl->cs.clientNum;
                } else {
                    baseNumber = 99999;
                }
            } else {
                MSG_ReadDeltaClientState(&msg, 0, &newCl->cs, clientNum);
                newCl->playerStateExists = MSG_ReadBit(&msg);
                if (newCl->playerStateExists != 0)
                    MSG_ReadDeltaPlayerState(&msg, 0, (unsigned char *)&newCl->ps);
                g_svs_nextCachedSnapshotClients = g_svs_nextCachedSnapshotClients + 1;
                if (0x7ffffffd < g_svs_nextCachedSnapshotClients)
                    Com_Error(0, "\025svs.nextCachedSnapshotClients wrapped");
                slot->num_clients = slot->num_clients + 1;
            }
        }
    }

    /* entities (both paths): deltas from the entity baselines */
    int entityNum;
    while ((entityNum = MSG_ReadBits(&msg, 10)) != 0x3ff) {
        if (msg.cursize < msg.readcount)
            Com_Error(1, "\025SV_GetCachedSnapshot: end of message");
        archivedEntity_t *newEnt = (archivedEntity_t *)(((byte *)svs.cachedSnapshotEntities) + (g_svs_nextCachedSnapshotEntities % 0x4000) * 0x114);
        MSG_ReadDeltaSharedEntity(&msg, &sv_svEntities[entityNum].baseline.s, &newEnt->s, entityNum);
        g_svs_nextCachedSnapshotEntities = g_svs_nextCachedSnapshotEntities + 1;
        if (0x7ffffffd < g_svs_nextCachedSnapshotEntities)
            Com_Error(0, "\025svs.nextCachedSnapshotEntities wrapped");
        slot->num_entities = slot->num_entities + 1;
    }
    sv_nextCachedSnapshotFrame = sv_nextCachedSnapshotFrame + 1;
    if (0x7ffffffd < sv_nextCachedSnapshotFrame)
        Com_Error(0, "\025svs.nextCachedSnapshotFrames wrapped");
    return slot;
}

// 0x08099c04  size=299  FUN_08099c04  status=translated  naming=unknown
// Finds the oldest rebuildable cached snapshot at or after (now - *msecAge); clamps/zeroes *msecAge as the archive
// range requires. Returns 0 (and zeroes *msecAge) when none can be rebuilt.
cachedSnapshot_t *SV_FindCachedSnapshotByAge(int *msecAge)
{
    if (svs.archiveEnabled != 0 && 0 < *msecAge) {
        int first = g_svs_nextArchivedSnapshotFrames - (*msecAge * cv_sv_fps->current.integer) / 1000;
        if (first < g_svs_nextArchivedSnapshotFrames - 0x4b0) {
            first = g_svs_nextArchivedSnapshotFrames - 0x4b0;
            *msecAge = ((g_svs_nextArchivedSnapshotFrames - first) * 1000) / cv_sv_fps->current.integer;
        }
        if (first < 0) {
            first = 0;
            *msecAge = (g_svs_nextArchivedSnapshotFrames * 1000) / cv_sv_fps->current.integer;
        }
        for (; first < g_svs_nextArchivedSnapshotFrames; first++) {
            cachedSnapshot_t *snap = SV_GetCachedSnapshot(first);
            if (snap != 0)
                return snap;
        }
        *msecAge = 0;
    }
    return 0;
}

// 0x08099d30  size=124  FUN_08099d30  status=translated  naming=unknown
// For an active client (state 4): fetches the live player state through the game and copies its client state record.
int SV_GetCurrentPlayerAndClientState(int clientNum, playerState_t *ps, clientState_t *cs)
{
    if (svs.clients[clientNum].state == 4) {
        if (G_CopyClientPlayerState(clientNum, ps) == 0)
            return 0;
        *cs = *G_GetClientStateAddr(clientNum);
        return 1;
    }
    return 0;
}

// 0x08099dac  size=777  FUN_08099dac  status=translated  naming=unknown
// Interpolated player origin at (now - msecAge): searches up to 10 server frames forward/backward for stored player
// states, then lerps their origins (missing side contributes a zero vector with weight 0/1). Returns 0 when neither exists.
int SV_GetInterpolatedPlayerOrigin(int clientNum, int msecAge, float *originOut)
{
    playerState_t ps;
    clientState_t cs;
    vec3_t originA;
    vec3_t originB;

    int frameMsec = (int)(1000 / (long long)cv_sv_fps->current.integer);
    int baseTime = (svs.time / frameMsec) * frameMsec;
    int timeCursor = ((baseTime - msecAge) / frameMsec + sv_antilagFrameOffsetA) * frameMsec;
    int timeA = timeCursor;
    int timeB = ((baseTime - msecAge) / frameMsec + sv_antilagFrameOffsetB) * frameMsec;
    char haveB = '\0';
    char haveA = '\0';

    for (int i = 0; i < 10; i++) {
        if (SV_GetPlayerStateFromCache(clientNum, &timeCursor, &ps, &cs) != 0) {
            haveA = '\1';
            timeA = timeCursor;
            memcpy(originA, &((int *)&ps)[PS_WORD_ORIGIN], 0xc);
            break;
        }
        timeCursor = timeCursor + frameMsec;
    }

    timeCursor = timeB;
    for (int i = 0; i < 10; i++) {
        if (SV_GetPlayerStateFromCache(clientNum, &timeCursor, &ps, &cs) != 0) {
            haveB = '\1';
            timeB = timeCursor;
            memcpy(originB, &((int *)&ps)[PS_WORD_ORIGIN], 0xc);
            break;
        }
        timeCursor = timeCursor - frameMsec;
    }

    float fraction;
    if (haveA == '\0' || haveB == '\0') {
        if (haveA == '\0') {
            if (haveB == '\0')
                return 0;
            fraction = 1.0f;
            Vec3_Clear_0809bf66(originA);
        } else {
            fraction = 0.0f;
            Vec3_Clear_0809bf66(originB);
        }
    } else {
        fraction = (float)(msecAge % frameMsec) / (float)(timeA - timeB);
    }
    for (int i = 0; i < 3; i++)
        originOut[i] = Lerp_Float(originA[i], originB[i], fraction);
    return 1;
}

// 0x0809a0b6  size=849  FUN_0809a0b6  status=translated  naming=unknown
// Fetches the stored player state + client state of clientNum from the snapshot cache entry found for *msecAge, rebasing
// its time-valued words by (svs.time - snapshot time); falls back to the live state when *msecAge < 1.
int SV_GetPlayerStateFromCache(int clientNum, int *msecAge, playerState_t *ps, clientState_t *cs)
{
    cachedSnapshot_t *snap = SV_FindCachedSnapshotByAge(msecAge);
    if (snap == 0) {
        if (*msecAge < 1)
            return SV_GetCurrentPlayerAndClientState(clientNum, ps, cs);
        return 0;
    }
    int delta = svs.time - snap->time;
    for (int i = 0; i < snap->num_clients; i++) {
        cachedClient_t *cc = SV_CachedClientAt(snap->first_client + i);
        if (cc->cs.clientNum == clientNum) {
            if (cc->playerStateExists != 0) {
                memcpy(ps, &cc->ps, 0x26a8);
                memcpy(cs, &cc->cs, 0x5c);
                int *w = (int *)ps;
                if (w[PS_WORD_TIME_0] != 0) w[PS_WORD_TIME_0] += delta;
                if (w[PS_WORD_TIME_4] != 0) w[PS_WORD_TIME_4] += delta;
                if (w[PS_WORD_TIME_11] != 0) w[PS_WORD_TIME_11] += delta;
                if (w[PS_WORD_TIME_1C] != 0) w[PS_WORD_TIME_1C] += delta;
                if (w[PS_WORD_TIME_3F] != 0) w[PS_WORD_TIME_3F] += delta;
                if (w[PS_WORD_TIME_177] != 0) w[PS_WORD_TIME_177] += delta;
                for (unsigned int slotIdx = 0; slotIdx < (unsigned int)PS_SLOT_COUNT; slotIdx++) {
                    int base = PS_WORD_SLOT_BASE + (int)slotIdx * PS_SLOT_STRIDE;
                    if (w[base + PS_SLOT_TIME_D] != 0)
                        w[base + PS_SLOT_TIME_D] += delta;
                    if (w[base + PS_SLOT_TIME_A] != 0) {
                        w[base + PS_SLOT_TIME_A] += delta;
                        if (svs.time < w[base + PS_SLOT_TIME_A])
                            w[base + PS_SLOT_TIME_A] = svs.time;
                    }
                    if (w[base + PS_SLOT_TIME_B] != 0)
                        w[base + PS_SLOT_TIME_B] += delta;
                    if (w[base + PS_SLOT_TIME_C] != 0)
                        w[base + PS_SLOT_TIME_C] += delta;
                }
                w[PS_WORD_TIME_1E9] = delta + w[PS_WORD_TIME_1E9];
                return 1;
            }
            break;
        }
    }
    return 0;
}
