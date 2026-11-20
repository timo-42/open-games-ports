#include <ctype.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_023_types.h"


/* 0x0809a408  size=1735  FUN_0809a408  status=translated  naming=observed
 * Name evidence: error string "SV_BuildClientSnapshot: bad gEnt" at 0x0814c320 (without the leading 0x15 byte); builds
 * clientSnapshot_s (frames[outgoingSequence & 31]) from svs snapshot ring buffers or the cached snapshot frame. */
void SV_BuildClientSnapshot(client_s *cl)
{
    uint frameIdx = (uint)cl->netchan.outgoingSequence & 0x1f;
    clientSnapshot_s *frame = &cl->frames[frameIdx];
    frame->num_entities = 0;
    frame->num_clients = 0;
    if (cl->gentity == 0 || cl->state == 1)
        return;
    frame->first_entity = g_svs_nextSnapshotEntities;
    frame->first_client = g_svs_nextSnapshotClients;
    if (g_sv_state != 2)
        return;

    entlist_s list;
    float org[3];
    int entityNum;

    list.count = 0;
    entityNum = (int)(cl - svs.clients);                 /* client index */
    int key = G_GetClientArchiveTime(entityNum);
    cachedSnapshot_s *cached = SV_FindCachedSnapshotByAge(&key);
    G_SetClientArchiveTime(entityNum, key);
    int timeDelta;
    if (cached == 0)
        timeDelta = 0;
    else
        timeDelta = svs.time - cached->time;

    playerState_s *srcPs = (playerState_s *)SV_GameClientNum(entityNum);
    memcpy(&frame->ps, srcPs, 0x26a8);
    entityNum = frame->ps.clientNum;
    if (entityNum < 0 || entityNum > 0x3ff)
        Com_Error(1, "\025SV_BuildClientSnapshot: bad gEnt");
    Vec3_Copy_0809bf90(frame->ps.origin, org);
    org[2] = org[2] + frame->ps.viewHeightCurrent;
    BG_ApplyLeanRotationOffset(org, frame->ps.viewangles[1], frame->ps.leanf, 16.0f, 20.0f);

    if (cached == 0) {
        SV_BuildClientListForPoint(org, entityNum, (int *)&list);
        for (int i = 0; i < list.count; i++) {
            gentity_s *ent = SV_GentityNum(list.ents[i]);
            entityState_s *dst = &svs.snapshotEntities[g_svs_nextSnapshotEntities % svs.numSnapshotEntities];
            memcpy(dst, &ent->s, sizeof(entityState_s));
            g_svs_nextSnapshotEntities = g_svs_nextSnapshotEntities + 1;
            if (0x7ffffffd < g_svs_nextSnapshotEntities)
                Com_Error(0, "\025svs.nextSnapshotEntities wrapped");
            frame->num_entities = frame->num_entities + 1;
        }
        client_s *c = svs.clients;
        for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
            if (1 < c->state) {
                clientState_s *dst = &svs.snapshotClients[g_svs_nextSnapshotClients % svs.numSnapshotClients];
                clientState_s *src = G_GetClientStateAddr(i);
                memcpy(dst, src, sizeof(clientState_s));
                if (dst->clientNum == i) {
                    g_svs_nextSnapshotClients = g_svs_nextSnapshotClients + 1;
                    if (0x7ffffffd < g_svs_nextSnapshotClients)
                        Com_Error(0, "\025svs.nextSnapshotClients wrapped");
                    frame->num_clients = frame->num_clients + 1;
                }
            }
            c = (client_s *)((char *)c + sizeof(client_s));
        }
    } else {
        SV_BuildCachedEntityListForPoint(cached->num_entities, cached->first_entity, org, entityNum, (int *)&list);
        for (int i = 0; i < list.count; i++) {
            int idx = cached->first_entity + list.ents[i];
            archivedEntity_s *src = &svs.cachedSnapshotEntities[idx % 0x4000];
            entityState_s *dst = &svs.snapshotEntities[g_svs_nextSnapshotEntities % svs.numSnapshotEntities];
            memcpy(dst, src, sizeof(entityState_s));
            /* four time-like fields are rebased by the age of the cached frame (offsets 0x10,0x34,0x54,0x58) */
            int *w = (int *)dst;
            if (w[4] != 0)
                w[4] = timeDelta + w[4];
            if (w[0xd] != 0)
                w[0xd] = timeDelta + w[0xd];
            if (w[0x15] != 0)
                w[0x15] = timeDelta + w[0x15];
            if (w[0x16] != 0)
                w[0x16] = timeDelta + w[0x16];
            g_svs_nextSnapshotEntities = g_svs_nextSnapshotEntities + 1;
            if (0x7ffffffd < g_svs_nextSnapshotEntities)
                Com_Error(0, "\025svs.nextSnapshotEntities wrapped");
            frame->num_entities = frame->num_entities + 1;
        }
        for (int i = 0; i < cached->num_clients; i++) {
            int idx = cached->first_client + i;
            cachedClient_s *src = &svs.cachedSnapshotClients[idx % 0x1000];
            clientState_s *dst = &svs.snapshotClients[g_svs_nextSnapshotClients % svs.numSnapshotClients];
            memcpy(dst, &src->cs, sizeof(clientState_s));
            g_svs_nextSnapshotClients = g_svs_nextSnapshotClients + 1;
            if (0x7ffffffd < g_svs_nextSnapshotClients)
                Com_Error(0, "\025svs.nextSnapshotClients wrapped");
            frame->num_clients = frame->num_clients + 1;
        }
    }
}

/* 0x0809aad0  size=209  FUN_0809aad0  status=translated  naming=inferred
 * Name evidence: string "It would take %ims to send %i bytes to client %s (rate %i)"; Quake III SV_RateMsec counterpart
 * (clamp 1500, optional max rate dvar, (size+48)*1000/rate). */
int SV_RateMsec(client_s *cl, int messageSize)
{
    if (0x5dc < messageSize)
        messageSize = 0x5dc;
    int rate = cl->rate;
    if (cv_sv_maxRate->current.integer != 0) {
        if (cv_sv_maxRate->current.integer < 1000)
            Dvar_SetInt(cv_sv_maxRate, 1000);
        if (cv_sv_maxRate->current.integer < rate)
            rate = cv_sv_maxRate->current.integer;
    }
    int rateMsec = (messageSize * 1000 + 48000) / rate;
    if (cv_sv_debugRate->current.enabled != '\0')
        Com_Printf("It would take %ims to send %i bytes to client %s (rate %i)\n", rateMsec, messageSize, cl->name, cl->rate);
    return rateMsec;
}

/* 0x0809aba2  size=536  FUN_0809aba2  status=translated  naming=inferred
 * Name evidence: Quake III SV_SendMessageToClient counterpart: compresses the message (first 4 bytes raw), records
 * frame messageSize/Sent/Acked, transmits through the netchan wrapper, schedules nextSnapshotTime from rate.
 * Exception-unwind cleanup of the TempBuffer (catch handler at 0x0809adba) is not modelled. */
void SV_SendMessageToClient(msg_t *msg, client_s *cl)
{
    int tb;
    Com_PoolAllocInto2(&tb, 0x20000);
    byte *buf = (byte *)Com_PoolPtrFromOffset(&tb);
    memcpy(buf, msg->data, 4);
    int len = Huff_CompressBytes(msg->data + 4, buf + 4, msg->cursize - 4);
    len = len + 4;
    if (cl->delayDropMsg != 0)
        SV_DropClient(cl, cl->delayDropMsg);
    clientSnapshot_s *frame = &cl->frames[(uint)cl->netchan.outgoingSequence & 0x1f];
    frame->messageSize = len;
    frame = &cl->frames[(uint)cl->netchan.outgoingSequence & 0x1f];
    frame->messageSent = svs.time;
    frame = &cl->frames[(uint)cl->netchan.outgoingSequence & 0x1f];
    frame->messageAcked = -1;
    SV_Netchan_Transmit(cl, buf, len);
    if (cl->netchan.remoteAddress.type == 2 || Sys_IsLANAddress(cl->netchan.remoteAddress) != 0) {
        cl->nextSnapshotTime = svs.time - 1;
        Com_PoolRestoreFrom2(&tb);
        return;
    }
    int delay = SV_RateMsec(cl, len);
    if (delay < cl->snapshotMsec) {
        delay = cl->snapshotMsec;
        cl->rateDelayed = 0;
    } else {
        cl->rateDelayed = 1;
    }
    cl->nextSnapshotTime = delay + svs.time;
    if (cl->state != 4 && cl->downloadName[0] == 0 && cl->nextSnapshotTime < svs.time + 1000)
        cl->nextSnapshotTime = svs.time + 1000;
    g_0849f68c = g_0849f68c + len;
    Com_PoolRestoreFrom2(&tb);
}

/* 0x0809adea  size=508  FUN_0809adea  status=translated  naming=inferred
 * Name evidence: Quake III SV_SendClientSnapshot counterpart; strings "WARNING: msg overflowed for %s, trying to recover",
 * "WARNING: client disconnected for msg overflow: %s", "EXE_SERVERMESSAGEOVERFLOW". Message-write callee semantics
 * (MSG_WriteByte writing the constant 7 as terminator) are inferred. */
void SV_SendClientSnapshot(client_s *cl)
{
    msg_t msg;
    int tb;
    Com_PoolAllocInto2(&tb, 0x20000);
    void *buf = Com_PoolPtrFromOffset(&tb);
    if (cl->state == 4 || cl->state == 1)
        SV_BuildClientSnapshot(cl);
    MSG_Init(&msg, (byte *)buf, 0x20000);
    MSG_WriteLong(&msg, cl->lastClientCommand); /* binary: [cl+0x20840] */
    if (cl->state == 4 || cl->state == 1) {
        SV_UpdateServerCommandsToClient(cl, &msg);
        SV_WriteSnapshotToClient(cl, &msg);
    }
    if (cl->state != 1)
        SV_WriteDownloadToClient(cl, &msg);
    MSG_WriteByte(&msg, 7);
    if (msg.overflowed != 0) {
        Com_Printf("WARNING: msg overflowed for %s, trying to recover\n", cl->name);
        if (cl->state == 4 || cl->state == 1) {
            SV_PrintUnackedServerCommands(cl);
            MSG_Init(&msg, (byte *)buf, 0x20000);
            MSG_WriteLong(&msg, cl->lastClientCommand); /* binary: [cl+0x20840] */
            SV_UpdateServerCommandsToClientLimited(cl, &msg, 0x20000);
            MSG_WriteByte(&msg, 7);
        }
        if (msg.overflowed != 0) {
            Com_Printf("WARNING: client disconnected for msg overflow: %s\n", cl->name);
            NET_OutOfBandPrint(1, cl->netchan.remoteAddress, "disconnect");
            SV_DropClient(cl, "EXE_SERVERMESSAGEOVERFLOW");
        }
    }
    SV_SendMessageToClient(&msg, cl);
    Com_PoolRestoreFrom2(&tb);
}

/* 0x0809b016  size=3184  FUN_0809b016  status=translated  naming=observed
 * Name evidence: string "SV_ArchiveSnapshot: ignoring snapshot because it overflowed." and anchor SV_ArchiveSnapshot.
 * Writes a (full or delta) snapshot into the archive ring buffer and caches entity/client state. Exception-unwind
 * cleanup of TempBuffer is not modelled. */
void SV_ArchiveSnapshot(void)
{
    msg_t msg;
    int tb;
    Com_PoolAllocInto2(&tb, 0x20000);
    byte *buf = (byte *)Com_PoolPtrFromOffset(&tb);
    if (g_sv_state != 2) {
        Com_PoolRestoreFrom2(&tb);
        return;
    }
    if (svs.archiveEnabled == 0) {
        Com_PoolRestoreFrom2(&tb);
        return;
    }
    MSG_Init(&msg, (byte *)buf, 0x20000);
    int minFrame = sv_nextCachedSnapshotFrame - 0x200;
    if (minFrame < 0)
        minFrame = 0;
    int ageLimit = g_svs_nextArchivedSnapshotFrames - cv_sv_fps->current.integer;
    int frameIdx = sv_nextCachedSnapshotFrame;
    cachedSnapshot_s *entry;
    int prev;
    bool makeFull = false;
    do {
        prev = frameIdx - 1;
        if (prev < minFrame) {
            makeFull = true;
            break;
        }
        entry = &svs.cachedSnapshotFrames[prev % 0x200];
        frameIdx = prev;
    } while (entry->archivedFrame < ageLimit || entry->usesDelta != 0);
    if (!makeFull) {
        if (entry->first_entity < g_svs_nextCachedSnapshotEntities - 0x4000 || entry->first_client < g_svs_nextCachedSnapshotClients - 0x1000)
            makeFull = true;
    }

    if (makeFull) {
        MSG_WriteBit1(&msg);
        MSG_WriteLong(&msg, svs.time);
        entry = &svs.cachedSnapshotFrames[sv_nextCachedSnapshotFrame % 0x200];
        entry->archivedFrame = g_svs_nextArchivedSnapshotFrames;
        entry->num_entities = 0;
        entry->first_entity = g_svs_nextCachedSnapshotEntities;
        entry->num_clients = 0;
        entry->first_client = g_svs_nextCachedSnapshotClients;
        entry->usesDelta = 0;
        entry->time = svs.time;
        client_s *c = svs.clients;
        for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
            if (1 < c->state) {
                cachedClient_s *slot = &svs.cachedSnapshotClients[g_svs_nextCachedSnapshotClients % 0x1000];
                clientState_s *src = G_GetClientStateAddr(i);
                memcpy(&slot->cs, src, sizeof(clientState_s));
                MSG_WriteDeltaClientState(&msg, 0, &slot->cs, 1);
                slot->playerStateExists = G_CopyClientPlayerState(i, &slot->ps);
                if (slot->playerStateExists == 0) {
                    MSG_WriteBit0(&msg);
                } else {
                    MSG_WriteBit1(&msg);
                    MSG_WriteDeltaPlayerState(&msg, 0, (const unsigned char *)&slot->ps);
                }
                g_svs_nextCachedSnapshotClients = g_svs_nextCachedSnapshotClients + 1;
                if (0x7ffffffd < g_svs_nextCachedSnapshotClients)
                    Com_Error(0, "\025svs.nextCachedSnapshotClients wrapped");
                entry->num_clients = entry->num_clients + 1;
            }
            c = (client_s *)((char *)c + sizeof(client_s));
        }
        MSG_WriteBit0(&msg);
        for (int e = 0; e < g_numGEntities; e++) {
            gentity_s *ent = SV_GentityNum(e);
            bool send = false;
            if (ent->r.linked != 0) {
                if (ent->r.broadcastTime != 0) {
                    send = true;
                } else if ((ent->r.svFlags & 1) == 0) {
                    svEntity_s *se = SV_SvEntityForGentity(ent);
                    if ((ent->r.svFlags & 0x18) != 0 || se->numClusters != 0)
                        send = true;
                }
            }
            if (send) {
                archivedEntity_s *slot = &svs.cachedSnapshotEntities[g_svs_nextCachedSnapshotEntities % 0x4000];
                memcpy(&slot->s, &ent->s, sizeof(entityState_s));
                slot->r.svFlags = (uint)ent->r.svFlags;
                if (ent->r.broadcastTime != 0)
                    slot->r.svFlags = slot->r.svFlags | 8;
                slot->r.clientMask[0] = ent->r.clientMask[0];
                slot->r.clientMask[1] = ent->r.clientMask[1];
                Vec3_Copy_0809bf90(ent->r.absmin, slot->r.absmin);
                Vec3_Copy_0809bf90(ent->r.absmax, slot->r.absmax);
                MSG_WriteDeltaSharedEntity(&msg, &sv_svEntities[ent->s.number].baseline, slot, 1);
                g_svs_nextCachedSnapshotEntities = g_svs_nextCachedSnapshotEntities + 1;
                if (0x7ffffffd < g_svs_nextCachedSnapshotEntities)
                    Com_Error(0, "\025svs.nextCachedSnapshotEntities wrapped");
                entry->num_entities = entry->num_entities + 1;
            }
        }
        sv_nextCachedSnapshotFrame = sv_nextCachedSnapshotFrame + 1;
        if (0x7ffffffd < sv_nextCachedSnapshotFrame)
            Com_Error(0, "\025svs.nextCachedSnapshotFrames wrapped");
    } else {
        MSG_WriteBit0(&msg);
        MSG_WriteLong(&msg, entry->archivedFrame);
        MSG_WriteLong(&msg, svs.time);
        int maxClients = cv_sv_maxclients->current.integer;
        int cachedCount = entry->num_clients;
        cachedClient_s *cc = 0;
        int i = 0;
        int j = 0;
        int cachedNum;
        while (i < maxClients || j < cachedCount) {
            if (i < maxClients && svs.clients[i].state < 2) {
                i = i + 1;
            } else {
                if (j < cachedCount) {
                    int idx = entry->first_client + j;
                    cc = &svs.cachedSnapshotClients[idx % 0x1000];
                    cachedNum = cc->cs.clientNum;
                } else {
                    cachedNum = 9999;
                }
                if (i == cachedNum) {
                    clientState_s *cs = G_GetClientStateAddr(i);
                    MSG_WriteDeltaClientState(&msg, &cc->cs, cs, 1);
                    playerState_s tmpPs;
                    int has = G_CopyClientPlayerState(i, &tmpPs);
                    if (has == 0) {
                        MSG_WriteBit0(&msg);
                    } else {
                        MSG_WriteBit1(&msg);
                        MSG_WriteDeltaPlayerState(&msg, (const unsigned char *)&cc->ps, (const unsigned char *)&tmpPs);
                    }
                    j = j + 1;
                    i = i + 1;
                } else if (i < cachedNum) {
                    clientState_s *cs = G_GetClientStateAddr(i);
                    MSG_WriteDeltaClientState(&msg, 0, cs, 1);
                    playerState_s tmpPs;
                    int has = G_CopyClientPlayerState(i, &tmpPs);
                    if (has == 0) {
                        MSG_WriteBit0(&msg);
                    } else {
                        MSG_WriteBit1(&msg);
                        MSG_WriteDeltaPlayerState(&msg, 0, (const unsigned char *)&tmpPs);
                    }
                    i = i + 1;
                } else if (cachedNum < i) {
                    j = j + 1;
                }
            }
        }
        MSG_WriteBit0(&msg);
        for (int e = 0; e < g_numGEntities; e++) {
            gentity_s *ent = SV_GentityNum(e);
            bool send = false;
            if (ent->r.linked != 0) {
                if (ent->r.broadcastTime != 0) {
                    send = true;
                } else if ((ent->r.svFlags & 1) == 0) {
                    svEntity_s *se = SV_SvEntityForGentity(ent);
                    if ((ent->r.svFlags & 0x18) != 0 || se->numClusters != 0)
                        send = true;
                }
            }
            if (send) {
                archivedEntity_s tmp;
                memcpy(&tmp.s, &ent->s, sizeof(entityState_s));
                tmp.r.svFlags = (uint)ent->r.svFlags;
                if (ent->r.broadcastTime != 0)
                    tmp.r.svFlags = tmp.r.svFlags | 8;
                tmp.r.clientMask[0] = ent->r.clientMask[0];
                tmp.r.clientMask[1] = ent->r.clientMask[1];
                Vec3_Copy_0809bf90(ent->r.absmin, tmp.r.absmin);
                Vec3_Copy_0809bf90(ent->r.absmax, tmp.r.absmax);
                MSG_WriteDeltaSharedEntity(&msg, &sv_svEntities[ent->s.number].baseline, &tmp, 1);
            }
        }
    }

    MSG_WriteBits(&msg, 0x3ff, 10);
    if (msg.overflowed != 0) {
        Com_DPrintf("SV_ArchiveSnapshot: ignoring snapshot because it overflowed.\n");
        Com_PoolRestoreFrom2(&tb);
        return;
    }
    archivedSnapshot_s *af = &svs.archivedSnapshotFrames[g_svs_nextArchivedSnapshotFrames % 0x4b0];
    af->start = sv_nextArchivedSnapshotBuffer;
    af->size = msg.cursize;
    int offset = sv_nextArchivedSnapshotBuffer % 0x2000000;
    sv_nextArchivedSnapshotBuffer = sv_nextArchivedSnapshotBuffer + msg.cursize;
    if (0x7ffffffd < sv_nextArchivedSnapshotBuffer)
        Com_Error(0, "\025svs.nextArchivedSnapshotBuffer wrapped");
    size_t firstPart = 0x2000000 - offset;
    if ((int)firstPart < msg.cursize) {
        memcpy(svs.archivedSnapshotBuffer + offset, msg.data, firstPart);
        memcpy(svs.archivedSnapshotBuffer, msg.data + firstPart, msg.cursize - firstPart);
    } else {
        memcpy(svs.archivedSnapshotBuffer + offset, msg.data, msg.cursize);
    }
    g_svs_nextArchivedSnapshotFrames = g_svs_nextArchivedSnapshotFrames + 1;
    if (0x7ffffffd < g_svs_nextArchivedSnapshotFrames)
        Com_Error(0, "\025svs.nextArchivedSnapshotFrames wrapped");
    Com_PoolRestoreFrom2(&tb);
}

/* 0x0809bcce  size=641  FUN_0809bcce  status=translated  naming=inferred
 * Name evidence: Quake III SV_SendClientMessages counterpart; string "bpspc(%2.0f) bps(%2.0f) pk(%i) ubps(%2.0f) upk(%i)
 * cr(%2.2f) acr(%2.2f)". Also keeps a 20-slot sliding window of sent/unsent byte counts (0849f638/0849f694). */
void SV_SendClientMessages(void)
{
    int numActive = 0;
    g_0849f68c = 0;
    g_0849f6e4 = 0;
    client_s *c = svs.clients;
    for (int i = 0; i < cv_sv_maxclients->current.integer; i++) {
        if (c->state != 0 && c->nextSnapshotTime <= svs.time) {
            numActive = numActive + 1;
            if (c->netchan.unsentFragments == 0) {
                SV_SendClientSnapshot(c);
                SV_SendClientVoiceData(c);
            } else {
                int msec = SV_RateMsec(c, c->netchan.unsentLength - c->netchan.unsentFragmentStart);
                c->nextSnapshotTime = msec + svs.time;
                SV_Netchan_TransmitNextFragment(&c->netchan);
            }
        }
        c = (client_s *)((char *)c + sizeof(client_s));
    }
    if (cv_sv_showAverageBPS->current.enabled != '\0' && 0 < numActive) {
        float sentSum = 0.0f;
        float unsentSum = 0.0f;
        for (int k = 0; k < 0x13; k++) {
            g_sendBytesHistory[k] = g_sendBytesHistory[k + 1];
            sentSum = (float)g_sendBytesHistory[k] + sentSum;
            g_unsentBytesHistory[k] = g_unsentBytesHistory[k + 1];
            unsentSum = (float)g_unsentBytesHistory[k] + unsentSum;
        }
        g_sendBytesHistory[19] = g_0849f68c;
        sentSum = (float)g_0849f68c + sentSum;
        g_unsentBytesHistory[19] = g_0849f6e4;
        unsentSum = (float)g_0849f6e4 + unsentSum;
        if (g_0849f690 <= g_0849f68c)
            g_0849f690 = g_0849f68c;
        if (g_0849f6e8 <= g_0849f6e4)
            g_0849f6e8 = g_0849f6e4;
        g_0849f688 = g_0849f688 + 1;
        if (0x13 < g_0849f688) {
            g_0849f688 = 0;
            sentSum = sentSum / 20.0f;
            unsentSum = unsentSum / 20.0f;
            float ratio = (1.0f - sentSum / unsentSum) * 100.0f;
            g_0849f6ec = g_0849f6ec + ratio;
            g_0849f6f0 = g_0849f6f0 + 1;
            Com_DPrintf("bpspc(%2.0f) bps(%2.0f) pk(%i) ubps(%2.0f) upk(%i) cr(%2.2f) acr(%2.2f)\n",
                        (double)(sentSum / (float)numActive), (double)sentSum, g_0849f690,
                        (double)unsentSum, g_0849f6e8, (double)ratio,
                        (double)(g_0849f6ec / (float)g_0849f6f0));
        }
    }
}

/* 0x0809bf50  size=21  FUN_0809bf50  status=translated  naming=inferred
 * Name evidence: computes b*t + (1-t)*a; linear interpolation. Returns float in st0. */
float Lerp_Float(float a, float b, float t)
{
    return b * t + (1.0f - t) * a;
}

/* 0x0809bf66  size=41  FUN_0809bf66  status=translated  naming=inferred
 * Name evidence: zeroes three consecutive 32-bit words (vec3 clear). */
void Vec3_Clear_0809bf66(float *v)
{
    ((int *)v)[0] = 0;
    ((int *)v)[1] = 0;
    ((int *)v)[2] = 0;
}

/* 0x0809bf90  size=47  FUN_0809bf90  status=translated  naming=inferred
 * Name evidence: copies three 32-bit words from src to dst (VectorCopy). */
void Vec3_Copy_0809bf90(const float *src, float *dst)
{
    ((int *)dst)[0] = ((const int *)src)[0];
    ((int *)dst)[1] = ((const int *)src)[1];
    ((int *)dst)[2] = ((const int *)src)[2];
}

/* 0x0809bfc0  size=225  FUN_0809bfc0  status=translated  naming=inferred
 * Name evidence: writes voicePacketCount then, per queued packet, talker byte, length and payload bytes into a message.
 * The two writer callees (0806800c / 08068044) are unnamed; the length field is written with the same callee as the count. */
void SV_WriteVoicePackets(client_s *cl, msg_t *msg)
{
    MSG_WriteByte(msg, cl->voicePacketCount);
    for (int i = 0; i < cl->voicePacketCount; i++) {
        MSG_WriteByte(msg, cl->voicePackets[i].talker);
        MSG_WriteByte(msg, cl->voicePackets[i].dataSize);
        MSG_WriteData(msg, cl->voicePackets[i].data, cl->voicePackets[i].dataSize);
    }
}

/* 0x0809c0a2  size=266  FUN_0809c0a2  status=translated  naming=inferred
 * Name evidence: string "WARNING: voice msg overflowed for %s"; sends an out-of-band packet beginning with the string "v"
 * (rodata 0x0814c5e0) followed by the queued voice packets, then clears the queue. */
void SV_SendClientVoiceData(client_s *cl)
{
    msg_t msg;
    int tb;
    Com_PoolAllocInto2(&tb, 0x20000);
    void *buf = Com_PoolPtrFromOffset(&tb);
    if (cl->state == 4 && cl->voicePacketCount != 0) {
        MSG_Init(&msg, (byte *)buf, 0x20000);
        MSG_WriteString(&msg, "v");
        SV_WriteVoicePackets(cl, &msg);
        if (msg.overflowed == 0) {
            NET_OutOfBandVoice(1, cl->netchan.remoteAddress, msg.data, msg.cursize);
            cl->voicePacketCount = 0;
        } else {
            Com_Printf("WARNING: voice msg overflowed for %s\n", cl->name);
        }
    }
    Com_PoolRestoreFrom2(&tb);
}

/* 0x0809c1dc  size=29  FUN_0809c1dc  status=translated  naming=unknown
 * Reads byte at client[n]+0xb1020 (voiceFlag); meaning of the flag inferred only from its use as a send gate. */
byte SV_GetClientSendVoice(int clientNum)
{
    return svs.clients[clientNum].sendVoice;
}

/* 0x0809c1fa  size=33  FUN_0809c1fa  status=translated  naming=unknown
 * Reads byte muteFlags[talker] of client[listener] (client+0xb0fe0); used as "skip when set". */
byte SV_GetClientMuteListEntry(int clientNum, int talker)
{
    return svs.clients[clientNum].muteList[talker];
}

/* 0x0809c21c  size=210  FUN_0809c21c  status=translated  naming=inferred
 * Appends a received voice packet to listener's queue (max 40), tagging the talker. */
void SV_QueueVoicePacket(byte talker, int listener, voicepacket_s *pkt)
{
    client_s *cl = &svs.clients[listener];
    if (cl->voicePacketCount < 0x28) {
        cl->voicePackets[cl->voicePacketCount].dataSize = pkt->dataSize;
        memcpy(cl->voicePackets[cl->voicePacketCount].data, pkt->data, (size_t)pkt->dataSize);
        cl->voicePackets[cl->voicePacketCount].talker = talker;
        cl->voicePacketCount = cl->voicePacketCount + 1;
    }
}

/* 0x0809c2ee  size=216  FUN_0809c2ee  status=translated  naming=inferred
 * Name evidence: string "Received invalid voice packet of size %i from %s"; reads packet count then each packet and
 * hands it to G_SayToEligibleClients with the client's gentity. The talker byte of the local packet is never written (uninitialised). */
void SV_ParseVoicePacketsLocal(client_s *cl, msg_t *msg)
{
    voicepacket_s pkt;
    if (cv_sv_voice->current.enabled != '\0') {
        int count = MSG_ReadByte(msg);
        for (int i = 0; i < count; i++) {
            pkt.dataSize = MSG_ReadByte(msg);
            if (pkt.dataSize < 1 || 0x100 < pkt.dataSize) {
                Com_Printf("Received invalid voice packet of size %i from %s\n", pkt.dataSize, cl->name);
                return;
            }
            MSG_ReadData(msg, pkt.data, pkt.dataSize);
            G_SayToEligibleClients(cl->gentity, &pkt);
        }
    }
}

/* 0x0809c3c6  size=380  FUN_0809c3c6  status=translated  naming=inferred
 * Name evidence: string "Received invalid voice packet of size %i from %s". Reads voice packets from a client and queues
 * them to every other connected client (state > 1) that does not mute the talker and has the voice flag set. The length is
 * read with MSG_ReadShort (differs from the count reader MSG_ReadByte). */
void SV_ParseVoicePackets(client_s *cl, msg_t *msg)
{
    voicepacket_s pkt;
    if (cv_sv_voice->current.enabled != '\0') {
        int talker = (int)(cl - svs.clients);
        int count = MSG_ReadByte(msg);
        for (int i = 0; i < count; i++) {
            pkt.dataSize = MSG_ReadShort(msg);
            if (pkt.dataSize < 1 || 0x100 < pkt.dataSize) {
                Com_Printf("Received invalid voice packet of size %i from %s\n", pkt.dataSize, cl->name);
                return;
            }
            MSG_ReadData(msg, pkt.data, pkt.dataSize);
            for (int j = 0; j < 0x40; j++) {
                if (j != talker && 1 < svs.clients[j].state
                    && SV_GetClientMuteListEntry(j, talker) == '\0'
                    && SV_GetClientSendVoice(j) != '\0') {
                    SV_QueueVoicePacket((byte)talker, j, &pkt);
                }
            }
        }
    }
}

/* 0x0809c544  size=81  FUN_0809c544  status=translated  naming=inferred
 * Q3 SV_ClipHandleForEntity counterpart: brush models (bmodel) return a stored handle (entityState +0x8c), otherwise a
 * temporary box model built from mins/maxs/contents (CM_TempBoxModel). */
int SV_ClipHandleForEntity(gentity_s *ent)
{
    if (ent->r.bmodel == 0)
        return CM_TempBoxModel(ent->r.mins, ent->r.maxs, ent->r.contents);
    return ent->s.index;
}

/* 0x0809c596  size=43  FUN_0809c596  status=translated  naming=inferred
 * Q3 SV_UnlinkEntity counterpart: clears linked and removes the svEntity from its world sector. */
void SV_UnlinkEntity(gentity_s *ent)
{
    svEntity_s *se = SV_SvEntityForGentity(ent);
    ent->r.linked = 0;
    Area_UnlinkEntity(se);
}

/* 0x0809c5c2  size=119  FUN_0809c5c2  status=translated  naming=unknown
 * For each of 3 components, snaps to the nearest integer when the squared difference is below 1.0000001e-06. */
void SV_SnapNearIntegers(float *v)
{
    for (int i = 0; i < 3; i++) {
        int r = Round_Float(v[i]);
        float d = (float)r - v[i];
        if (d * d < 1.0000001e-06f)
            v[i] = (float)r;
    }
}

/* 0x0809c63a  size=1833  FUN_0809c63a  status=translated  naming=inferred
 * Q3 SV_LinkEntity counterpart: packs entityState.solid from mins/maxs (bmodel -> 0xffffff), computes absmin/absmax
 * (with rotation handling and 1-unit padding), recomputes clusters, adds the entity to the world sector tree. */
void SV_LinkEntity(gentity_s *ent)
{
    svEntity_s *se = SV_SvEntityForGentity(ent);
    if (ent->r.bmodel == 0) {
        if ((ent->r.contents & 0x2000001) == 0) {
            ent->s.solid = 0;
        } else {
            int x = (int)ent->r.maxs[0];
            if (x < 1)
                x = 1;
            if (0xff < x)
                x = 0xff;
            int zdown = (int)(1.0f - ent->r.mins[2]);
            if (zdown < 1)
                zdown = 1;
            if (0xff < zdown)
                zdown = 0xff;
            int zup = (int)(ent->r.maxs[2] + 32.0f);
            if (zup < 1)
                zup = 1;
            if (0xff < zup)
                zup = 0xff;
            ent->s.solid = zdown << 8 | zup << 0x10 | x;
        }
    } else {
        ent->s.solid = 0xffffff;
    }
    float *origin = ent->r.currentOrigin;
    float *angles = ent->r.currentAngles;
    SV_SnapNearIntegers(angles);
    if (ent->r.bmodel == 0 || (angles[0] == 0.0f && angles[1] == 0.0f && angles[2] == 0.0f)) {
        Vec3_Add_0809e502(origin, ent->r.mins, ent->r.absmin);
        Vec3_Add_0809e502(origin, ent->r.maxs, ent->r.absmax);
    } else if (angles[0] != 0.0f || angles[2] != 0.0f) {
        float radius = Vec3_MaxAbsLength(ent->r.mins, ent->r.maxs);
        for (int i = 0; i < 3; i++) {
            ent->r.absmin[i] = origin[i] - radius;
            ent->r.absmax[i] = origin[i] + radius;
        }
    } else {
        float radius = Vec2_MaxAbsLength(ent->r.mins, ent->r.maxs);
        for (int i = 0; i < 2; i++) {
            ent->r.absmin[i] = origin[i] - radius;
            ent->r.absmax[i] = origin[i] + radius;
        }
        ent->r.absmin[2] = origin[2] + ent->r.mins[2];
        ent->r.absmax[2] = origin[2] + ent->r.maxs[2];
    }
    ent->r.absmin[0] = ent->r.absmin[0] - 1.0f;
    ent->r.absmin[1] = ent->r.absmin[1] - 1.0f;
    ent->r.absmin[2] = ent->r.absmin[2] - 1.0f;
    ent->r.absmax[0] = ent->r.absmax[0] + 1.0f;
    ent->r.absmax[1] = ent->r.absmax[1] + 1.0f;
    ent->r.absmax[2] = ent->r.absmax[2] + 1.0f;
    se->numClusters = 0;
    se->lastCluster = 0;
    if ((ent->r.svFlags & 0x19) == 0) {
        int leafs[128];
        int lastLeaf;
        int numLeafs = CM_BoxLeafnums(ent->r.absmin, ent->r.absmax, leafs, 0x80, &lastLeaf);
        if (numLeafs == 0) {
            Area_UnlinkEntity(se);
            return;
        }
        int i;
        for (i = 0; i < numLeafs; i++) {
            int cluster = CM_LeafCluster(leafs[i]);
            if (cluster != -1) {
                se->clusternums[se->numClusters] = cluster;
                se->numClusters = se->numClusters + 1;
                if (se->numClusters == 0x10)
                    break;
            }
        }
        if (i != numLeafs)
            se->lastCluster = CM_LeafCluster(lastLeaf);
    }
    ent->r.linked = 1;
    if (ent->r.contents == 0) {
        Area_UnlinkEntity(se);
        return;
    }
    int clipHandle = SV_ClipHandleForEntity(ent);
    DObj *model = Com_GetServerDObj(ent->s.number);
    if (model == 0 || (ent->r.svFlags & 6) == 0) {
        Area_LinkEntity(se, ent->r.absmin, ent->r.absmax, clipHandle);
    } else {
        float boxMin[3];
        float boxMax[3];
        if ((ent->r.svFlags & 2) == 0) {
            DObj_GetBounds((const struct DObj *)model, boxMin, boxMax);
            Vec2_Add(origin, boxMin, boxMin);
            Vec2_Add(origin, boxMax, boxMax);
        } else {
            Vec2_Add(origin, g_0817db88, boxMin);   /* z components of boxMin/boxMax stay uninitialised in the original */
            Vec2_Add(origin, g_0817db94, boxMax);
        }
        Area_LinkEntity(se, boxMin, boxMax, clipHandle);
    }
}

/* 0x0809cd64  size=420  FUN_0809cd64  status=translated  naming=inferred
 * Q3 SV_ClipMoveToEntity counterpart (box version): tests the swept box against one entity and updates the trace. */
void SV_ClipMoveToEntity_Box(struct BoxHitTrace *clipArg, svEntity_s *touch, trace_s *tr)
{
    boxclip_a_s *clip = (boxclip_a_s *)clipArg; /* chunk 006 tag BoxHitTrace; this chunk's view of the same object */
    int entityNum = (int)(touch - sv_svEntities);
    gentity_s *ent = SV_GentityNum(entityNum);
    if ((ent->r.contents & (uint)clip->contentmask) != 0
        && (clip->passEntityNum == 0x3ff
            || (entityNum != clip->passEntityNum
                && ent->r.ownerNum != clip->passEntityNum
                && ent->r.ownerNum != clip->passOwnerNum))) {
        float bmin[3];
        float bmax[3];
        Vec3_Add_0809e502(ent->r.absmin, clip->mins, bmin);
        Vec3_Add_0809e502(ent->r.absmax, clip->maxs, bmax);
        if (Box_SegmentSlabTest((const struct cod2_box_s *)clip->start, bmin, bmax, tr->fraction) == 0) {
            int clipHandle = SV_ClipHandleForEntity(ent);
            float *angles = ent->r.currentAngles;
            if (ent->r.bmodel == 0)
                angles = vec3_origin;
            float oldFraction = tr->fraction;
            Sweep_TraceBoxWithOffset(tr, clip->start, clip->end, clip->mins, clip->maxs, clipHandle, clip->contentmask,
                        ent->r.currentOrigin, angles);
            if (tr->fraction < oldFraction)
                tr->entityNum = (uint16)ent->s.number;
        }
    }
}

/* 0x0809cf08  size=1139  FUN_0809cf08  status=translated  naming=inferred
 * Line-trace counterpart of SV_ClipMoveToEntity (point trace, optional rotated brush-model / capsule path).
 * Local cmtrace_s is at ebp-0x68; its pseudocode locals were mis-split by the decompiler. */
void SV_ClipMoveToEntity_Line(struct SegHitTrace *clipArg, svEntity_s *touch, trace_s *tr)
{
    lineclip_a_s *clip = (lineclip_a_s *)clipArg; /* chunk 006 tag SegHitTrace; this chunk's view of the same object */
    int entityNum = (int)(touch - sv_svEntities);
    gentity_s *ent = SV_GentityNum(entityNum);
    if ((ent->r.contents & (uint)clip->contentmask) == 0)
        return;
    if (clip->passEntityNum != 0x3ff) {
        if (entityNum == clip->passEntityNum)
            return;
        if (ent->r.ownerNum == clip->passEntityNum)
            return;
        if (ent->r.ownerNum == clip->passOwnerNum)
            return;
    }
    DObj *model = 0;
    bool simple = false;
    if (clip->param30 == 0) {
        simple = true;
    } else {
        model = Com_GetServerDObj(ent->s.number);
        if (model == 0 || (ent->r.svFlags & 6) == 0)
            simple = true;
    }
    if (simple) {
        if ((touch->linkcontents & (uint)clip->contentmask) == 0)
            return;
        if (Box_SegmentSlabTest((const struct cod2_box_s *)clip->start, ent->r.absmin, ent->r.absmax, tr->fraction) != 0)
            return;
        int clipHandle = SV_ClipHandleForEntity(ent);
        float *angles = ent->r.currentAngles;
        if (ent->r.bmodel == 0)
            angles = vec3_origin;
        float oldFraction = tr->fraction;
        Sweep_TraceBoxWithOffset(tr, clip->start, clip->end, vec3_origin, vec3_origin, clipHandle, clip->contentmask,
                    ent->r.currentOrigin, angles);
        if (oldFraction <= tr->fraction)
            return;
        tr->surfaceFlags = 0.0f;
        tr->partName = 0;
        tr->partGroup = 0;
    } else {
        float boxMin[3];
        float boxMax[3];
        /* the original's 4x3 axis matrix ([ebp-0xc8]) holds currentOrigin in its origin row (+0x24); the bounds
           are offset from that row and Vec3_WorldToLocal subtracts it (xform + 9) */
        float axis[12];
        float *org = axis + 9;
        if ((ent->r.svFlags & 4) == 0) {
            if (clip->param34 == 0) {
                /* falls back to the simple path above */
                if ((touch->linkcontents & (uint)clip->contentmask) == 0)
                    return;
                if (Box_SegmentSlabTest((const struct cod2_box_s *)clip->start, ent->r.absmin, ent->r.absmax, tr->fraction) != 0)
                    return;
                int clipHandle = SV_ClipHandleForEntity(ent);
                float *angles = ent->r.currentAngles;
                if (ent->r.bmodel == 0)
                    angles = vec3_origin;
                float oldFraction = tr->fraction;
                Sweep_TraceBoxWithOffset(tr, clip->start, clip->end, vec3_origin, vec3_origin, clipHandle, clip->contentmask,
                            ent->r.currentOrigin, angles);
                if (oldFraction <= tr->fraction)
                    return;
                tr->surfaceFlags = 0.0f;
                tr->partName = 0;
                tr->partGroup = 0;
                tr->entityNum = (uint16)ent->s.number;
                tr->contents = ent->r.contents;
                tr->material = 0;
                return;
            }
            Vec3_Copy2(ent->r.currentOrigin, org);
            Vec3_Add_0809e502(org, g_0817db88, boxMin);
            Vec3_Add_0809e502(org, g_0817db94, boxMax);
        } else {
            if (DObj_HasContents((const struct DObj *)model, clip->contentmask) == 0)
                return;
            Vec3_Copy2(ent->r.currentOrigin, org);
            DObj_GetBounds((const struct DObj *)model, boxMin, boxMax);
            Vec3_Add_0809e502(org, boxMin, boxMin);
            Vec3_Add_0809e502(org, boxMax, boxMax);
        }
        if (Box_SegmentSlabTest((const struct cod2_box_s *)clip->start, boxMin, boxMax, tr->fraction) != 0)
            return;
        float localStart[4];
        float localEnd[4];
        cmtrace_s t;
        Ent_RunKindCallback(ent);
        AnglesToAxis(ent->r.currentAngles, axis);
        Vec3_WorldToLocal(clip->start, axis, localStart);
        Vec3_WorldToLocal(clip->end, axis, localEnd);
        t.fraction = tr->fraction;
        if ((ent->r.svFlags & 4) == 0)
            DObj_Traceline((struct DObj *)model, localStart, localEnd, (const uint8 *)clip->param34, (struct DObjTraceResult *)&t);
        else
            DObj_GeomTraceline((struct DObj *)model, localStart, localEnd, (const void *)clip->contentmask, (struct DObjTraceResult *)&t);
        if (tr->fraction <= t.fraction)
            return;
        tr->fraction = t.fraction;
        /* gameplay-fix(net_clients): 0x0809d20d..0x0809d210: raw dword copy of the DObj trace record's +4 (surface flags), not a float-to-int conversion */
        memcpy(&tr->surfaceFlags, &t.dist, sizeof(tr->surfaceFlags));
        tr->partName = t.field_14;
        tr->partGroup = t.field_16;
        Vec3_RotateByTranspose(t.normal, axis, tr->normal);
    }
    tr->entityNum = (uint16)ent->s.number;
    tr->contents = ent->r.contents;
    tr->material = 0;
}

/* 0x0809d37c  size=365  FUN_0809d37c  status=translated  naming=inferred
 * Boolean box test against a single entity (returns -1 on hit, 0 otherwise); uses the boxclip_b layout. */
int SV_ClipBoxHitsEntity(struct BoxQueryTrace *clipArg, svEntity_s *touch)
{
    boxclip_b_s *clip = (boxclip_b_s *)clipArg; /* chunk 006 tag BoxQueryTrace; this chunk's view of the same object */
    int entityNum = (int)(touch - sv_svEntities);
    gentity_s *ent = SV_GentityNum(entityNum);
    if (((uint)ent->r.contents & (uint)clip->contentmask) == 0)
        return 0;
    if (clip->passEntityNum != 0x3ff) {
        if (entityNum == clip->passEntityNum)
            return 0;
        if (ent->r.ownerNum == clip->passEntityNum)
            return 0;
    }
    if (clip->passOwnerNum != 0x3ff) {
        if (entityNum == clip->passOwnerNum)
            return 0;
        if (ent->r.ownerNum == clip->passOwnerNum)
            return 0;
    }
    int clipHandle = SV_ClipHandleForEntity(ent);
    float *angles = ent->r.currentAngles;
    if (ent->r.bmodel == 0)
        angles = vec3_origin;
    int hit = Sweep_TraceEntityWithOffset(0, clip->start, clip->end, clip->mins, clip->maxs, clipHandle, clip->contentmask,
                          ent->r.currentOrigin, angles);
    if (hit == 0)
        return 0;
    return -1;
}

/* 0x0809d4ea  size=921  FUN_0809d4ea  status=translated  naming=inferred
 * Boolean line test against a single entity (-1 on hit, 0 otherwise); lineclip_b layout, with the rotated brush-model
 * path when the entity has svFlags bit 4 and flag24 is set. */
int SV_ClipLineHitsEntity(struct SegQueryA *clipArg, svEntity_s *touch)
{
    lineclip_b_s *clip = (lineclip_b_s *)clipArg; /* chunk 006 tag SegQueryA; this chunk's view of the same object */
    int entityNum = (int)(touch - sv_svEntities);
    gentity_s *ent = SV_GentityNum(entityNum);
    if (((uint)ent->r.contents & (uint)clip->contentmask) == 0)
        return 0;
    if (clip->passEntityNum != 0x3ff) {
        if (entityNum == clip->passEntityNum)
            return 0;
        if (ent->r.ownerNum == clip->passEntityNum)
            return 0;
    }
    if (clip->passOwnerNum != 0x3ff) {
        if (entityNum == clip->passOwnerNum)
            return 0;
        if (ent->r.ownerNum == clip->passOwnerNum)
            return 0;
    }
    DObj *model = 0;
    bool simple = false;
    if (clip->flag24 == 0) {
        simple = true;
    } else {
        model = Com_GetServerDObj(ent->s.number);
        if (model == 0 || (ent->r.svFlags & 4) == 0)
            simple = true;
    }
    if (simple) {
        int clipHandle = SV_ClipHandleForEntity(ent);
        float *angles = ent->r.currentAngles;
        if (ent->r.bmodel == 0)
            angles = vec3_origin;
        int hit = Sweep_TraceEntityWithOffset(0, clip->start, clip->end, vec3_origin, vec3_origin, clipHandle, clip->contentmask,
                              ent->r.currentOrigin, angles);
        if (hit != 0)
            return -1;
    } else {
        float boxMin[3];
        float boxMax[3];
        float axis[12]; /* [ebp-0xf8]: 4x3, currentOrigin copied into the origin row (+0x24) */
        float *org = axis + 9;
        float localStart[4];
        float localEnd[4];
        lineclip_a_s seg;
        float tr[8];
        if (DObj_HasContents((const struct DObj *)model, clip->contentmask) == 0)
            return 0;
        Vec3_Copy2(ent->r.currentOrigin, org);
        DObj_GetBounds((const struct DObj *)model, boxMin, boxMax);
        Vec3_Add_0809e502(org, boxMin, boxMin);
        Vec3_Add_0809e502(org, boxMax, boxMax);
        Vec3_Copy2(clip->start, seg.start);
        Vec3_Copy2(clip->end, seg.end);
        Box_ComputeInverseExtents((float *)&seg);
        if (Box_SegmentSlabTest((const struct cod2_box_s *)seg.start, boxMin, boxMax, 1.0f) != 0)
            return 0;
        Ent_RunKindCallback(ent);
        AnglesToAxis(ent->r.currentAngles, axis);
        Vec3_WorldToLocal(seg.start, axis, localStart);
        Vec3_WorldToLocal(seg.end, axis, localEnd);
        tr[0] = 1.0f;
        DObj_GeomTraceline((struct DObj *)model, localStart, localEnd, (const void *)clip->contentmask, (struct DObjTraceResult *)tr);
        if (tr[0] < 1.0f)
            return -1;
    }
    return 0;
}

/* 0x0809d884  size=826  FUN_0809d884  status=translated  naming=inferred
 * Q3 SV_Trace counterpart: world trace via CM_BoxTrace, then clips against entities (line or box clip structs). */
void SV_Trace(trace_s *results, const float *start, const float *mins, const float *maxs, const float *end, int passEntityNum, int contentmask, int param8, int param9, int extraCheck)
{
    CM_BoxTrace(results, start, end, mins, maxs, 0, contentmask);
    if (results->fraction != 1.0f)
        results->entityNum = 0x3fe;
    else
        results->entityNum = 0x3ff;
    if (results->fraction != 0.0f
        && (extraCheck == 0 || (Area_SweepBoxFromPoints((float *)results, start, end, contentmask), results->fraction != 0.0f))) {
        if (((((maxs[0] - mins[0]) + maxs[1]) - mins[1]) + maxs[2]) - mins[2] == 0.0f) {
            lineclip_a_s lc;
            Vec3_Copy2(start, lc.start);
            Vec3_Copy2(end, lc.end);
            Box_ComputeInverseExtents((float *)&lc);
            lc.passEntityNum = passEntityNum;
            lc.contentmask = contentmask;
            lc.param30 = param8;
            lc.param34 = param9;
            if (passEntityNum == 0x3ff) {
                lc.passOwnerNum = -1;
            } else {
                gentity_s *pe = SV_GentityNum(passEntityNum);
                lc.passOwnerNum = pe->r.ownerNum;
                if (lc.passOwnerNum == 0x3ff)
                    lc.passOwnerNum = -1;
            }
            SpatialTree_TraceSegmentHitsFromRoot((struct SegHitTrace *)&lc, (float *)results);
        } else {
            boxclip_a_s bc;
            float half[3];
            float center[3];
            bc.contentmask = contentmask;
            bc.passEntityNum = passEntityNum;
            if (passEntityNum == 0x3ff) {
                bc.passOwnerNum = -1;
            } else {
                gentity_s *pe = SV_GentityNum(passEntityNum);
                bc.passOwnerNum = pe->r.ownerNum;
                if (bc.passOwnerNum == 0x3ff)
                    bc.passOwnerNum = -1;
            }
            Vec3_Subtract_0809e546(maxs, mins, half);
            Vec3_Scale(half, 0.5f, half);
            Vec3_Copy2(half, bc.maxs);
            Vec3_Scale(half, -1.0f, bc.mins);
            half[0] = half[0] + 1.0f;
            half[1] = half[1] + 1.0f;
            half[2] = half[2] + 1.0f;
            Vec3_Copy_0809bf90(half, bc.extentPlus1);
            Vec3_Add_0809e502(maxs, mins, center);
            Vec3_Scale(center, 0.5f, center);
            Vec3_Add_0809e502(start, center, bc.start);
            Vec3_Add_0809e502(end, center, bc.end);
            Box_ComputeInverseExtents(bc.start);
            SpatialTree_TraceBoxHitsFromRoot((struct BoxHitTrace *)&bc, (float *)results);
        }
    }
}

/* 0x0809dbbe  size=633  FUN_0809dbbe  status=translated  naming=unknown
 * Returns 1 when the swept line/box is clear of the world and entities (0 on obstruction). The stack layout matches
 * boxclip_b_s (mins/maxs/extent+1/start/end then pass/owner/mask). */
int SV_TraceIsClear(float *start, float *mins, float *maxs, float *end, int passEntityNum, int passOwnerNum,
                    int contentmask, int flag, int extraCheck)
{
    if (Sweep_TraceEntityDispatch(0, start, end, mins, maxs, 0, contentmask) != 0)
        return 0;
    if (extraCheck != 0 && Collision_IsSegmentClear(start, end, contentmask) == 0)
        return 0;
    if (((((maxs[0] - mins[0]) + maxs[1]) - mins[1]) + maxs[2]) - mins[2] == 0.0f) {
        lineclip_b_s lc;
        lc.contentmask = contentmask;
        Vec3_Copy2(start, lc.start);
        Vec3_Copy2(end, lc.end);
        lc.passEntityNum = passEntityNum;
        lc.passOwnerNum = passOwnerNum;
        lc.flag24 = flag;
        if (SpatialTree_QuerySegmentFromRoot((struct SegQueryA *)&lc) != 0)
            return 0;
    } else {
        boxclip_b_s bc;
        float center[3];
        bc.contentmask = contentmask;
        bc.passEntityNum = passEntityNum;
        bc.passOwnerNum = passOwnerNum;
        Vec3_Subtract_0809e546(maxs, mins, bc.extentPlus1);
        Vec3_Scale(bc.extentPlus1, 0.5f, bc.extentPlus1);
        Vec3_Copy2(bc.extentPlus1, bc.maxs);
        Vec3_Scale(bc.extentPlus1, -1.0f, bc.mins);
        bc.extentPlus1[0] = bc.extentPlus1[0] + 1.0f;
        bc.extentPlus1[1] = bc.extentPlus1[1] + 1.0f;
        bc.extentPlus1[2] = bc.extentPlus1[2] + 1.0f;
        Vec3_Add_0809e502(maxs, mins, center);
        Vec3_Scale(center, 0.5f, center);
        Vec3_Add_0809e502(start, center, bc.start);
        Vec3_Add_0809e502(end, center, bc.end);
        if (SpatialTree_TraceBoxFirstHitFromRoot((struct BoxQueryTrace *)&bc) != 0)
            return 0;
    }
    return 1;
}

/* 0x0809de38  size=550  FUN_0809de38  status=translated  naming=unknown
 * Like SV_TraceIsClear but accumulates into *hit: first the world test result (previous value passed in), then, if still
 * zero, the entity line/box test. */
void SV_TraceAccumulateHit(int *hit, float *start, float *mins, float *maxs, float *end, int passEntityNum,
                           int passOwnerNum, int contentmask)
{
    *hit = Sweep_TraceEntityDispatch(*hit, start, end, mins, maxs, 0, contentmask);
    if (*hit == 0) {
        if (((((maxs[0] - mins[0]) + maxs[1]) - mins[1]) + maxs[2]) - mins[2] == 0.0f) {
            lineclip_b_s lc;
            lc.contentmask = contentmask;
            Vec3_Copy2(start, lc.start);
            Vec3_Copy2(end, lc.end);
            lc.passEntityNum = passEntityNum;
            lc.passOwnerNum = passOwnerNum;
            lc.flag24 = 0;
            *hit = SpatialTree_QuerySegmentFromRoot((struct SegQueryA *)&lc);
        } else {
            boxclip_b_s bc;
                float center[3];
            bc.contentmask = contentmask;
            bc.passEntityNum = passEntityNum;
            bc.passOwnerNum = passOwnerNum;
            Vec3_Subtract_0809e546(maxs, mins, bc.extentPlus1);
            Vec3_Scale(bc.extentPlus1, 0.5f, bc.extentPlus1);
            Vec3_Copy2(bc.extentPlus1, bc.maxs);
            Vec3_Scale(bc.extentPlus1, -1.0f, bc.mins);
            bc.extentPlus1[0] = bc.extentPlus1[0] + 1.0f;
            bc.extentPlus1[1] = bc.extentPlus1[1] + 1.0f;
            bc.extentPlus1[2] = bc.extentPlus1[2] + 1.0f;
            Vec3_Add_0809e502(maxs, mins, center);
            Vec3_Scale(center, 0.5f, center);
            Vec3_Add_0809e502(start, center, bc.start);
            Vec3_Add_0809e502(end, center, bc.end);
            *hit = SpatialTree_TraceBoxFirstHitFromRoot((struct BoxQueryTrace *)&bc);
        }
    }
}

/* 0x0809e05e  size=587  FUN_0809e05e  status=translated  naming=unknown
 * Tests the swept box (points start/end, extents mins/maxs) against entity entityNum: contents mask, expanded-bounds
 * overlap against absmin/absmax, then the clip-model test. Returns -1 on hit, 0 otherwise. */
int SV_BoxEntityHit(const float *start, const float *mins, const float *maxs, const float *end, int entityNum, uint contentmask)
{
    gentity_s *ent = SV_GentityNum(entityNum);
    if (((uint)ent->r.contents & contentmask) == 0)
        return 0;
    float lo[3];
    float hi[3];
    for (int i = 0; i < 3; i++) {
        if (start[i] < end[i]) {
            lo[i] = (start[i] + mins[i]) - 1.0f;
            hi[i] = end[i] + maxs[i] + 1.0f;
        } else {
            lo[i] = (end[i] + mins[i]) - 1.0f;
            hi[i] = start[i] + maxs[i] + 1.0f;
        }
    }
    if (hi[0] < ent->r.absmin[0] || hi[1] < ent->r.absmin[1] || hi[2] < ent->r.absmin[2]
        || ent->r.absmax[0] < lo[0] || ent->r.absmax[1] < lo[1] || ent->r.absmax[2] < lo[2])
        return 0;
    int clipHandle = SV_ClipHandleForEntity(ent);
    float *angles = ent->r.currentAngles;
    if (ent->r.bmodel == 0)
        angles = vec3_origin;
    int hit = Sweep_TraceEntityWithOffset(0, start, end, mins, maxs, clipHandle, (int)contentmask, ent->r.currentOrigin, angles);
    if (hit == 0)
        return 0;
    return -1;
}

/* 0x0809e368  size=264  FUN_0809e368  status=translated  naming=inferred
 * Q3 SV_PointContents counterpart: world contents at p ORed with contents of every entity (except passEntityNum)
 * found in a zero-size area query, masked by contentmask. */
uint SV_PointContents(float *p, int passEntityNum, uint contentmask)
{
    int list[1026];
    uint contents = CM_PointContents(p, 0);
    int n = CM_AreaEntities(p, p, list, 0x400, (int)contentmask);
    for (int i = 0; i < n; i++) {
        if (list[i] != passEntityNum) {
            gentity_s *ent = SV_GentityNum(list[i]);
            int clipHandle = SV_ClipHandleForEntity(ent);
            uint c = CM_TransformedPointContents(p, clipHandle, ent->r.currentOrigin, ent->r.currentAngles);
            contents = contents | c;
        }
    }
    return contentmask & contents;
}

/* 0x0809e470  size=54  FUN_0809e470  status=translated  naming=inferred
 * floor(x + 0.5) converted to int by truncation (fistp with truncating control word). */
int Round_Float(float x)
{
    double d = floor((double)x + 0.5);
    return (int)d;
}

/* 0x0809e4a6  size=44  FUN_0809e4a6  status=translated  naming=inferred
 * Adds only the first two components (x,y). */
void Vec2_Add(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
}

/* 0x0809e4d2  size=47  FUN_0809e4d2  status=translated  naming=inferred
 * Copies three words (duplicate of Vec3_Copy_0809bf90 at 0809bf90). */
void Vec3_Copy2(const float *src, float *dst)
{
    ((int *)dst)[0] = ((const int *)src)[0];
    ((int *)dst)[1] = ((const int *)src)[1];
    ((int *)dst)[2] = ((const int *)src)[2];
}

/* 0x0809e502  size=68  FUN_0809e502  status=translated  naming=inferred */
void Vec3_Add_0809e502(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

/* 0x0809e546  size=68  FUN_0809e546  status=translated  naming=inferred */
void Vec3_Subtract_0809e546(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

/* 0x0809e58a  size=56  FUN_0809e58a  status=translated  naming=inferred */
void Vec3_Scale(const float *v, float s, float *out)
{
    out[0] = s * v[0];
    out[1] = s * v[1];
    out[2] = s * v[2];
}

/* 0x0809e5c4  size=69  FUN_0809e5c4  status=translated  naming=inferred
 * Name evidence: strings "QuitOnError" and "r_vc_compile": caches whether errors should quit (command-line flag or
 * r_vc_compile==2) once the gate callee Dvar_GetSvCheatsReady reports true. */
void Com_UpdateQuitOnErrorFlag(void)
{
    if (Dvar_GetSvCheatsReady() != '\0') {
        char flag = 0;
        if (Dvar_GetBool("QuitOnError") != '\0' || Dvar_GetInt("r_vc_compile") == 2)
            flag = 1;
        com_quitOnErrorFlag = flag;
    }
}

/* 0x0809e60a  size=20  FUN_0809e60a  status=translated  naming=inferred */
char Com_GetQuitOnErrorFlag(void)
{
    Com_UpdateQuitOnErrorFlag();
    return com_quitOnErrorFlag;
}

/* 0x0809e620  size=18  FUN_0809e620  status=translated  naming=inferred
 * True when the search path list head (0849fd68) is non-null. */
bool FS_HasSearchPaths(void)
{
    return fs_searchpaths != 0;
}

/* 0x0809e632  size=5  FUN_0809e632  status=translated  naming=unknown
 * Empty function (called from FS_filelength). */
void FS_Nop(void)
{
}

/* 0x0809e638  size=93  FUN_0809e638  status=translated  naming=unknown
 * Returns 1 if the allow-list (084a4480 entries at 084a44a0) is empty or contains entry->checksum (+0x304). */
int FS_ChecksumAllowed(iwd_s *entry)
{
    if (fs_numServerIwds == 0)
        return 1;
    for (int i = 0; i < fs_numServerIwds; i++) {
        if (entry->checksum == fs_serverIwds[i])
            return 1;
    }
    return 0;
}

/* 0x0809e696  size=10  FUN_0809e696  status=translated  naming=unknown
 * Getter for global 0849fc24. */
int FS_GetLoadStackCount(void)
{
    return fs_loadStack;
}

/* 0x0809e6a0  size=85  FUN_0809e6a0  status=translated  naming=unknown
 * Search path acceptance test: rejects when field_c is set and the policy dvar is on; otherwise, for field_c != 0,
 * requires field_10 to equal Loc_GetLanguage(). */
int FS_SearchPathAllowed(searchpath_s *sp)
{
    if (sp->bLocalized == 0 || fs_ignoreLocalized->current.enabled == '\0') {
        if (sp->bLocalized != 0 && sp->language != Loc_GetLanguage())
            return 0;
        return 1;
    }
    return 0;
}

/* 0x0809e6f6  size=73  FUN_0809e6f6  status=translated  naming=unknown
 * Walks the search path list for a node with field_c != 0 and field_10 == id. */
int FS_HasLoadedPakWithId(int id)
{
    searchpath_s *sp = fs_searchpaths;
    while (true) {
        if (sp == 0)
            return 0;
        if (sp->bLocalized != 0 && sp->language == id)
            break;
        sp = sp->next;
    }
    return 1;
}

/* 0x0809e740  size=147  FUN_0809e740  status=translated  naming=inferred
 * Q3 FS_HashFileName counterpart: lowercased, '\\' -> '/', stops at '.', hash += (i+119)*c, folded with >>20 and >>10. */
uint FS_HashFileName(const char *fname, int hashSize)
{
    uint hash = 0;
    int i = 0;
    while (fname[i] != '\0') {
        int letter = tolower((int)fname[i]);
        if (letter == 0x2e)
            break;
        if (letter == 0x5c)
            letter = 0x2f;
        if (letter == 0x2f)
            letter = 0x2f;
        hash = hash + (uint)((i + 0x77) * letter);
        i = i + 1;
    }
    return (uint)(((int)hash >> 0x14) ^ ((int)hash >> 10) ^ (int)hash) & (uint)(hashSize - 1);
}

/* 0x0809e7d4  size=189  FUN_0809e7d4  status=translated  naming=observed
 * Name evidence: error string "FS_HandleForFile: none free" and the "FILE %2i: '%s'" dump. Slots 1..0x32 for
 * non-zip, 0x33..0x33+0xc for zip (zipFile != 0). */
int FS_HandleForFile(int zipFile)
{
    int base;
    int count;
    if (zipFile == 0) {
        base = 1;
        count = 0x32;
    } else {
        base = 0x33;
        count = 0xd;
    }
    int i = 0;
    while (true) {
        if (count <= i) {
            for (i = 1; i < 0x40; i++)
                Com_Printf("FILE %2i: '%s'\n", i, fsh[i].name);
            Com_Error(1, "\025FS_HandleForFile: none free");
            return -1;
        }
        if (fsh[i + base].handleFiles == 0)
            break;
        i = i + 1;
    }
    return i + base;
}

/* 0x0809e892  size=20  FUN_0809e892  status=translated  naming=inferred
 * Q3 FS_FileForHandle counterpart: returns the stored file pointer of the handle slot. */
void *FS_FileForHandle(int handle)
{
    return fsh[handle].handleFiles;
}

/* 0x0809e8a6  size=164  FUN_0809e8a6  status=translated  naming=inferred
 * Q3 FS_filelength counterpart: non-zip handles seek to the end and back; zip handles read the size at +0x44 of the
 * unzip state. */
long FS_filelength(int handle)
{
    long result;
    FS_Nop();
    /* 0x0809e8b6: cmp [slot+0x849fd94] (fsh.iwd, +0x14) -- pack entries report the unz state's size; +0x04 is the
     * uniqueFile flag, set for loose files opened with uniqueFile=1 as well */
    if (fsh[handle].iwd == NULL) {
        FILE *f = (FILE *)FS_FileForHandle(handle);
        long pos = ftell(f);
        Com_Fseek(f, 0, 2);
        result = ftell(f);
        Com_Fseek(f, pos, 0);
    } else {
        result = *(long *)((char *)fsh[handle].handleFiles + 0x44);
    }
    return result;
}

/* 0x0809e94a  size=108  FUN_0809e94a  status=translated  naming=inferred
 * Replaces runs of '/' and '\\' with a single '/' in place. */
void FS_ReplaceSeparators(char *path)
{
    bool inSep = false;
    char *dst = path;
    for (char *src = path; *src != '\0'; src++) {
        if (*src == '/' || *src == '\\') {
            if (!inSep) {
                inSep = true;
                *dst = '/';
                dst = dst + 1;
            }
        } else {
            inSep = false;
            *dst = *src;
            dst = dst + 1;
        }
    }
    *dst = '\0';
}

/* 0x0809e9b6  size=249  FUN_0809e9b6  status=translated  naming=observed
 * Name evidence: error string "FS_BuildOSPath: os path length exceeded"; builds base/game/qpath into out (max 255 chars),
 * with failSilently returning an empty string instead of raising the error. */
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *out, int failSilently)
{
    if (game == 0 || *game == '\0')
        game = fs_gamedir;
    size_t baseLen = ScrStr_Strlen(base);
    size_t gameLen = ScrStr_Strlen(game);
    int qpathLen = (int)ScrStr_Strlen(qpath);
    if (0xff < (int)(gameLen + baseLen + qpathLen + 2)) {
        if (failSilently != 0) {
            *out = 0;
            return;
        }
        Com_Error(0, "\025FS_BuildOSPath: os path length exceeded\n");
    }
    memcpy(out, base, baseLen);
    out[baseLen] = 0x2f;
    memcpy(out + baseLen + 1, game, gameLen);
    out[gameLen + baseLen + 1] = 0x2f;
    memcpy(out + gameLen + baseLen + 2, qpath, qpathLen + 1);
    FS_ReplaceSeparators(out);
}

/* 0x0809eab0  size=48  FUN_0809eab0  status=translated  naming=inferred
 * Wrapper of FS_BuildOSPath with failSilently = 0. */
void FS_BuildOSPath_Fatal(const char *base, const char *game, const char *qpath, char *out)
{
    FS_BuildOSPath(base, game, qpath, out, 0);
}

/* 0x0809eae0  size=149  FUN_0809eae0  status=translated  naming=inferred
 * Q3 FS_CreatePath counterpart: refuses paths containing ".." or "::", otherwise creates each parent directory. */
int FS_CreatePath(char *path)
{
    if (strstr(path, "..") != 0 || strstr(path, "::") != 0) {
        Com_Printf("WARNING: refusing to create relative path \"%s\"\n", path);
        return 1;
    }
    char *ofs = path;
    while (ofs = ofs + 1, *ofs != '\0') {
        if (*ofs == '/') {
            *ofs = '\0';
            Sys_Mkdir(path);
            *ofs = '/';
        }
    }
    return 0;
}
