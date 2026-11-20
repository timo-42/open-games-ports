/*
 * Canonical cross-chunk types for the Call of Duty 2 Linux dedicated server 1.3 reconstruction.
 * Target: cod2_lnxded SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7
 *
 * GENERATED from .local/cod2/canon-work/canon_spec.py by gen_canon.py (type-architect lane).
 * Decisions and per-chunk migration: games/call-of-duty-2/analysis/canonical-types.md and
 * canonical-migration.json. Every field comment carries its evidence (address of an access,
 * allocation/memset size or netField table entry). Unrecovered bytes are named unk_0xOFF.
 * Names follow the Quake III / IW-engine lineage where the binary evidence matches; they are
 * naming aids [I] unless the comment cites a string or table.
 * Include after cod2_types.h. Chunk headers must not redefine any tag declared here.
 */
#ifndef COD2_CANON_H
#define COD2_CANON_H

#include "cod2_types.h"

typedef void (*xcommand_t)(void);

/* ---- forward declarations: canonical tags defined below ---- */
struct netadr_s; typedef struct netadr_s netadr_t;
struct msg_s; typedef struct msg_s msg_t;
struct netField_s; typedef struct netField_s netField_t;
struct netProfilePacket_s; typedef struct netProfilePacket_s netProfilePacket_t;
struct netProfileStream_s; typedef struct netProfileStream_s netProfileStream_t;
struct netProfile_s; typedef struct netProfile_s netProfile_t;
struct netchan_s; typedef struct netchan_s netchan_t;
struct usercmd_s; typedef struct usercmd_s usercmd_t;
struct entityState_s; typedef struct entityState_s entityState_t;
struct entityShared_s; typedef struct entityShared_s entityShared_t;
struct archivedEntityShared_s; typedef struct archivedEntityShared_s archivedEntityShared_t;
struct archivedEntity_s; typedef struct archivedEntity_s archivedEntity_t;
struct trace_s; typedef struct trace_s trace_t;
struct hudelem_s; typedef struct hudelem_s hudelem_t;
struct game_hudelem_s; typedef struct game_hudelem_s game_hudelem_t;
struct objective_s; typedef struct objective_s objective_t;
struct MantleState; typedef struct MantleState MantleState;
struct playerState_s; typedef struct playerState_s playerState_t;
struct clientState_s; typedef struct clientState_s clientState_t;
struct pmove_s; typedef struct pmove_s pmove_t;
struct pml_s; typedef struct pml_s pml_t;
struct item_ent_s; typedef struct item_ent_s item_ent_t;
struct mover_ent_s; typedef struct mover_ent_s mover_ent_t;
struct trigger_ent_s; typedef struct trigger_ent_s trigger_ent_t;
union gentity_u; typedef union gentity_u gentity_u;
struct gentity_s; typedef struct gentity_s gentity_t;
struct gclient_s; typedef struct gclient_s gclient_t;
struct level_locals_s; typedef struct level_locals_s level_locals_t;
struct reliableCommand_s; typedef struct reliableCommand_s reliableCommand_t;
struct clientSnapshot_s; typedef struct clientSnapshot_s clientSnapshot_t;
struct voicepacket_s; typedef struct voicepacket_s voicepacket_t;
struct client_s; typedef struct client_s client_t;
struct challenge_s; typedef struct challenge_s challenge_t;
struct tempban_s; typedef struct tempban_s tempban_t;
struct archivedSnapshot_s; typedef struct archivedSnapshot_s archivedSnapshot_t;
struct cachedSnapshot_s; typedef struct cachedSnapshot_s cachedSnapshot_t;
struct cachedClient_s; typedef struct cachedClient_s cachedClient_t;
struct svEntity_s; typedef struct svEntity_s svEntity_t;
struct serverStatic_s; typedef struct serverStatic_s serverStatic_t;
struct server_s; typedef struct server_s server_t;
struct DvarLimitsEnum; typedef struct DvarLimitsEnum DvarLimitsEnum;
struct DvarLimitsInt; typedef struct DvarLimitsInt DvarLimitsInt;
struct DvarLimitsFloat; typedef struct DvarLimitsFloat DvarLimitsFloat;
union DvarValue; typedef union DvarValue DvarValue;
union DvarLimits; typedef union DvarLimits DvarLimits;
struct dvar_s; typedef struct dvar_s dvar_t;
struct cmd_function_s; typedef struct cmd_function_s cmd_function_t;
struct fileHandleData_s; typedef struct fileHandleData_s fileHandleData_t;
struct directory_s; typedef struct directory_s directory_t;
struct iwd_s; typedef struct iwd_s iwd_t;
struct searchpath_s; typedef struct searchpath_s searchpath_t;
union VariableUnion; typedef union VariableUnion VariableUnion;
struct VariableValue; typedef struct VariableValue VariableValue;
struct ScrVarEntry; typedef struct ScrVarEntry ScrVarEntry;
struct ScrVmFrame; typedef struct ScrVmFrame ScrVmFrame;
struct scrVmPub_t; typedef struct scrVmPub_t scrVmPub_t;
struct scrVarPub_t; typedef struct scrVarPub_t scrVarPub_t;
struct scrVmGlob_t; typedef struct scrVmGlob_t scrVmGlob_t;
struct ScrClassInfo; typedef struct ScrClassInfo ScrClassInfo;
struct ScrStackRecord_s; typedef struct ScrStackRecord_s ScrStackRecord;
struct ScrStackBuffer_s; typedef struct ScrStackBuffer_s ScrStackBuffer;
struct weaponDef_s; typedef struct weaponDef_s weaponDef_t;

/* ---- opaque handles owned by single subsystems (defined in the owning chunk headers) ---- */
struct XModel;
struct DObj_s;
struct XAnimTree_s;
struct turret_s;
struct game_attach_node_s;
struct unz_s;
struct fileInIwd_s;
struct snd_alias_list_t;
struct clientInfo_s;
struct huff_s;
struct FxPool;
struct dMaterial_s;
struct accuracy_knot_s;

/* netadr_s: Passed BY VALUE (5 dwords) to NET_AdrToString/NET_CompareAdr/NET_SendPacket/NET_OutOfBand* (signature-fixes.md). */
struct netadr_s {
    int type;                                        /* 0x0000 NA_BOT 0, NA_BAD 1, NA_LOOPBACK 2 ...; NET_StringToAdr failure stores 1 (chunks 20,21,22) */
    byte ip[4];                                      /* 0x0004 octet 0 in low byte; getIpAuthorize formats ip[0] first (chunks 20/21) */
    uint16 port;                                     /* 0x0008 network order; NET_AdrToString movsx eax,WORD PTR [ebp+0x10] (0x0806b207); SV_PacketEvent fix-up mov WORD PTR [edx+0x6e6dc] (0x08094c23). Chunk 101 (Q3 layout, port at +0x12) is wrong */
    byte ipx[10];                                    /* 0x000a IPX branch of NET_AdrToString reads bytes +0xc..+0x12 (0x0806b26f..0x0806b29f) */
};
COD2_ASSERT_SIZE(netadr_t, 0x14);
COD2_ASSERT_OFFSET(netadr_t, type, 0x0);
COD2_ASSERT_OFFSET(netadr_t, ip, 0x4);
COD2_ASSERT_OFFSET(netadr_t, port, 0x8);
COD2_ASSERT_OFFSET(netadr_t, ipx, 0xa);

/* msg_s: Size 0x18 (chunk 8 MSG_Init); chunks 6/21 (0x10) and 9 (0x14) were truncated views, chunk 20 (0x20) over-sized. */
struct msg_s {
    int overflowed;                                  /* 0x0000 tested after writes (chunk 23); chunk 21 called it allowoverflow (wrong) */
    byte * data;                                     /* 0x0004 MSG_Init(msg,data,len) */
    int maxsize;                                     /* 0x0008 MSG_Init */
    int cursize;                                     /* 0x000c all writers */
    int readcount;                                   /* 0x0010 MSG_Read* */
    int bit;                                         /* 0x0014 absolute bit cursor (chunks 8,22,38,100) */
};
COD2_ASSERT_SIZE(msg_t, 0x18);
COD2_ASSERT_OFFSET(msg_t, overflowed, 0x0);
COD2_ASSERT_OFFSET(msg_t, data, 0x4);
COD2_ASSERT_OFFSET(msg_t, maxsize, 0x8);
COD2_ASSERT_OFFSET(msg_t, cursize, 0xc);
COD2_ASSERT_OFFSET(msg_t, readcount, 0x10);
COD2_ASSERT_OFFSET(msg_t, bit, 0x14);

struct netField_s {
    const char * name;                               /* 0x0000 rodata tables 0x081417a0/0x08141ae0/0x08142520 */
    int offset;                                      /* 0x0004 byte offset into the coded struct */
    int bits;                                        /* 0x0008 0 float, -99/-100 special floats, N>0 unsigned bits, N<0 signed */
};
COD2_ASSERT_SIZE(netField_t, 0xc);
COD2_ASSERT_OFFSET(netField_t, name, 0x0);
COD2_ASSERT_OFFSET(netField_t, offset, 0x4);
COD2_ASSERT_OFFSET(netField_t, bits, 0x8);

struct netProfilePacket_s {
    int iTime;                                       /* 0x0000 NetProfile_AddSample ring entry (chunk 9 netprof_sample_s) */
    int iSize;                                       /* 0x0004 NetProfile_AddSample */
    int bFragment;                                   /* 0x0008 NetProfile_AddSample */
};
COD2_ASSERT_SIZE(netProfilePacket_t, 0xc);
COD2_ASSERT_OFFSET(netProfilePacket_t, iTime, 0x0);
COD2_ASSERT_OFFSET(netProfilePacket_t, iSize, 0x4);
COD2_ASSERT_OFFSET(netProfilePacket_t, bFragment, 0x8);

struct netProfileStream_s {
    netProfilePacket_t packets[60];                  /* 0x0000 ring of 60 x 12 bytes (chunk 9) */
    int iCurrPacket;                                 /* 0x02d0 NetProfile_AddSample index */
    int iBytesPerSecond;                             /* 0x02d4 chunk 22 asserts recv.bytesPerSec at netprof+0x5c4 = 0x2f0+0x2d4 */
    int iLastBPSCalcTime;                            /* 0x02d8 NetProfile_ComputeStats */
    int iCountedPackets;                             /* 0x02dc chunk 9 numPackets / chunk 22 fragDenominator */
    int iCountedFragments;                           /* 0x02e0 chunk 9 numFragmented / chunk 22 fragNumerator */
    int iFragmentPercentage;                         /* 0x02e4 chunk 9 percentFragmented / chunk 22 fragPercent */
    int iLargestPacket;                              /* 0x02e8 chunk 9 maxPacketSize / chunk 22 maxBytes */
    int iSmallestPacket;                             /* 0x02ec chunk 9 minPacketSize / chunk 22 minBytes (assert recv.minBytes 0x5dc) */
};
COD2_ASSERT_SIZE(netProfileStream_t, 0x2f0);
COD2_ASSERT_OFFSET(netProfileStream_t, packets, 0x0);
COD2_ASSERT_OFFSET(netProfileStream_t, iCurrPacket, 0x2d0);
COD2_ASSERT_OFFSET(netProfileStream_t, iBytesPerSecond, 0x2d4);
COD2_ASSERT_OFFSET(netProfileStream_t, iLastBPSCalcTime, 0x2d8);
COD2_ASSERT_OFFSET(netProfileStream_t, iCountedPackets, 0x2dc);
COD2_ASSERT_OFFSET(netProfileStream_t, iCountedFragments, 0x2e0);
COD2_ASSERT_OFFSET(netProfileStream_t, iFragmentPercentage, 0x2e4);
COD2_ASSERT_OFFSET(netProfileStream_t, iLargestPacket, 0x2e8);
COD2_ASSERT_OFFSET(netProfileStream_t, iSmallestPacket, 0x2ec);

/* netProfile_s: Allocation 0x5e0 in NetProfile_Sync (malloc/memset). */
struct netProfile_s {
    netProfileStream_t send;                         /* 0x0000 Netchan_ProfileSend uses +0 (chunk 9 outgoing, chunk 22 sent) */
    netProfileStream_t recieve;                      /* 0x02f0 Netchan_ProfileRecv (chunk 9 incoming, chunk 22 recv) */
};
COD2_ASSERT_SIZE(netProfile_t, 0x5e0);
COD2_ASSERT_OFFSET(netProfile_t, send, 0x0);
COD2_ASSERT_OFFSET(netProfile_t, recieve, 0x2f0);

/* netchan_s: Netchan_Setup 0x0806b9d0 memsets 0x40040 bytes (immediate at 0x0806b9d6). Chunk 22 (0x4003c) and 23 (0x2003c) were truncated views. */
struct netchan_s {
    int outgoingSequence;                            /* 0x0000 Netchan_Transmit; client+0x6e6c4 */
    int sock;                                        /* 0x0004 0 client, 1 server; Netchan_Setup(1,...) */
    int dropped;                                     /* 0x0008 Netchan_Process */
    int incomingSequence;                            /* 0x000c Netchan_Process */
    netadr_t remoteAddress;                          /* 0x0010 client+0x6e6d4 */
    int qport;                                       /* 0x0024 client+0x6e6e8, SV_DirectConnect */
    int fragmentSequence;                            /* 0x0028 Netchan_Process */
    int fragmentLength;                              /* 0x002c Netchan_Process */
    byte fragmentBuffer[0x20000];                    /* 0x0030 Netchan_Process memcpy destination */
    int unsentFragments;                             /* 0x20030 client+0x8e6f4; SV_SendClientMessages tests it */
    int unsentFragmentStart;                         /* 0x20034 Netchan_TransmitNextFragment */
    int unsentLength;                                /* 0x20038 Netchan_Transmit */
    byte unsentBuffer[0x20000];                      /* 0x2003c Netchan_Transmit Com_Memcpy destination */
    netProfile_t * prof;                             /* 0x4003c NetProfile_Sync(&chan->prof); = client+0xae700 (chunk 22 client.netProfile) */
};
COD2_ASSERT_SIZE(netchan_t, 0x40040);
COD2_ASSERT_OFFSET(netchan_t, outgoingSequence, 0x0);
COD2_ASSERT_OFFSET(netchan_t, sock, 0x4);
COD2_ASSERT_OFFSET(netchan_t, dropped, 0x8);
COD2_ASSERT_OFFSET(netchan_t, incomingSequence, 0xc);
COD2_ASSERT_OFFSET(netchan_t, remoteAddress, 0x10);
COD2_ASSERT_OFFSET(netchan_t, qport, 0x24);
COD2_ASSERT_OFFSET(netchan_t, fragmentSequence, 0x28);
COD2_ASSERT_OFFSET(netchan_t, fragmentLength, 0x2c);
COD2_ASSERT_OFFSET(netchan_t, fragmentBuffer, 0x30);
COD2_ASSERT_OFFSET(netchan_t, unsentFragments, 0x20030);
COD2_ASSERT_OFFSET(netchan_t, unsentFragmentStart, 0x20034);
COD2_ASSERT_OFFSET(netchan_t, unsentLength, 0x20038);
COD2_ASSERT_OFFSET(netchan_t, unsentBuffer, 0x2003c);
COD2_ASSERT_OFFSET(netchan_t, prof, 0x4003c);

/* usercmd_s: Chunk 20 (w0c[4]) and 46 (int field_18) merged byte fields; chunk 40 UserCmd040 is pml_s, not a usercmd. */
struct usercmd_s {
    int serverTime;                                  /* 0x0000 MSG_ReadDeltaUsercmdKey */
    int buttons;                                     /* 0x0004 SV_BotRandomUserCmd ORs 1 and 0x28 (0x080967ff/0x08096819) */
    byte weapon;                                     /* 0x0008 7-bit; SV_UserMove patches it from ps.weapon (+0xd4) */
    byte offHandIndex;                               /* 0x0009 7-bit; SV_UserMove patches it from ps.offHandIndex (+0xd0) */
    byte unk_0xa[0x2];                               /* 0x000a unrecovered */
    int angles[3];                                   /* 0x000c low 16 bits significant (MSG_ReadDeltaKeyShort) */
    signed char forwardmove;                         /* 0x0018 byte stores 0x7f/0x81 at [cmd+0x18] (0x08096830) */
    signed char rightmove;                           /* 0x0019 byte stores at [cmd+0x19] (0x08096862) */
    byte unk_0x1a[0x2];                              /* 0x001a unrecovered tail */
};
COD2_ASSERT_SIZE(usercmd_t, 0x1c);
COD2_ASSERT_OFFSET(usercmd_t, serverTime, 0x0);
COD2_ASSERT_OFFSET(usercmd_t, buttons, 0x4);
COD2_ASSERT_OFFSET(usercmd_t, weapon, 0x8);
COD2_ASSERT_OFFSET(usercmd_t, offHandIndex, 0x9);
COD2_ASSERT_OFFSET(usercmd_t, unk_0xa, 0xa);
COD2_ASSERT_OFFSET(usercmd_t, angles, 0xc);
COD2_ASSERT_OFFSET(usercmd_t, forwardmove, 0x18);
COD2_ASSERT_OFFSET(usercmd_t, rightmove, 0x19);
COD2_ASSERT_OFFSET(usercmd_t, unk_0x1a, 0x1a);

/* entityState_s: Names/offsets from the 59-entry netField table at 0x081417a0; size 0xf0 = rep movs 0xf0 in SV_ArchiveSnapshot (0x0809b973). */
struct entityState_s {
    int number;                                      /* 0x0000 10-bit entity number written by MSG_WriteDeltaEntity */
    int eType;                                       /* 0x0004 entityStateFields[12] 8 bits */
    int eFlags;                                      /* 0x0008 entityStateFields[13] 24 bits */
    trajectory_t pos;                                /* 0x000c pos.* netfields 0x0c..0x2c */
    trajectory_t apos;                               /* 0x0030 apos.* netfields 0x30..0x50 */
    int time;                                        /* 0x0054 entityStateFields[47] */
    int time2;                                       /* 0x0058 entityStateFields[50] */
    float origin2[3];                                /* 0x005c entityStateFields[53,54,52] */
    float angles2[3];                                /* 0x0068 entityStateFields[24,5,55]; BG_PlayerStateToEntityState stores movementDir as float in angles2[1] */
    int otherEntityNum;                              /* 0x0074 10 bits; game chunks attackerOrTurretEnt */
    int attackerEntityNum;                           /* 0x0078 10 bits */
    int groundEntityNum;                             /* 0x007c 10 bits; ClientSpawn writes 0x3ff */
    int constantLight;                               /* 0x0080 32 bits */
    int loopSound;                                   /* 0x0084 8 bits */
    int surfType;                                    /* 0x0088 8 bits */
    int index;                                       /* 0x008c 10 bits; model/item index (CM_ModelBounds arg in SV_LinkEntity) */
    int clientNum;                                   /* 0x0090 8 bits */
    int iHeadIcon;                                   /* 0x0094 4 bits */
    int iHeadIconTeam;                               /* 0x0098 2 bits */
    int solid;                                       /* 0x009c 24 bits */
    int eventParm;                                   /* 0x00a0 8 bits */
    int eventSequence;                               /* 0x00a4 8 bits */
    int events[4];                                   /* 0x00a8 8 bits each */
    int eventParms[4];                               /* 0x00b8 8 bits each */
    int weapon;                                      /* 0x00c8 7 bits */
    int legsAnim;                                    /* 0x00cc 10 bits */
    int torsoAnim;                                   /* 0x00d0 10 bits */
    float leanf;                                     /* 0x00d4 float; BG_PlayerStateToEntityState copies raw bits */
    int scale;                                       /* 0x00d8 8 bits */
    int dmgFlags;                                    /* 0x00dc 32 bits */
    int animMovetype;                                /* 0x00e0 4 bits */
    float fTorsoHeight;                              /* 0x00e4 float */
    float fTorsoPitch;                               /* 0x00e8 float */
    float fWaistPitch;                               /* 0x00ec float */
};
COD2_ASSERT_SIZE(entityState_t, 0xf0);
COD2_ASSERT_OFFSET(entityState_t, number, 0x0);
COD2_ASSERT_OFFSET(entityState_t, eType, 0x4);
COD2_ASSERT_OFFSET(entityState_t, eFlags, 0x8);
COD2_ASSERT_OFFSET(entityState_t, pos, 0xc);
COD2_ASSERT_OFFSET(entityState_t, apos, 0x30);
COD2_ASSERT_OFFSET(entityState_t, time, 0x54);
COD2_ASSERT_OFFSET(entityState_t, time2, 0x58);
COD2_ASSERT_OFFSET(entityState_t, origin2, 0x5c);
COD2_ASSERT_OFFSET(entityState_t, angles2, 0x68);
COD2_ASSERT_OFFSET(entityState_t, otherEntityNum, 0x74);
COD2_ASSERT_OFFSET(entityState_t, attackerEntityNum, 0x78);
COD2_ASSERT_OFFSET(entityState_t, groundEntityNum, 0x7c);
COD2_ASSERT_OFFSET(entityState_t, constantLight, 0x80);
COD2_ASSERT_OFFSET(entityState_t, loopSound, 0x84);
COD2_ASSERT_OFFSET(entityState_t, surfType, 0x88);
COD2_ASSERT_OFFSET(entityState_t, index, 0x8c);
COD2_ASSERT_OFFSET(entityState_t, clientNum, 0x90);
COD2_ASSERT_OFFSET(entityState_t, iHeadIcon, 0x94);
COD2_ASSERT_OFFSET(entityState_t, iHeadIconTeam, 0x98);
COD2_ASSERT_OFFSET(entityState_t, solid, 0x9c);
COD2_ASSERT_OFFSET(entityState_t, eventParm, 0xa0);
COD2_ASSERT_OFFSET(entityState_t, eventSequence, 0xa4);
COD2_ASSERT_OFFSET(entityState_t, events, 0xa8);
COD2_ASSERT_OFFSET(entityState_t, eventParms, 0xb8);
COD2_ASSERT_OFFSET(entityState_t, weapon, 0xc8);
COD2_ASSERT_OFFSET(entityState_t, legsAnim, 0xcc);
COD2_ASSERT_OFFSET(entityState_t, torsoAnim, 0xd0);
COD2_ASSERT_OFFSET(entityState_t, leanf, 0xd4);
COD2_ASSERT_OFFSET(entityState_t, scale, 0xd8);
COD2_ASSERT_OFFSET(entityState_t, dmgFlags, 0xdc);
COD2_ASSERT_OFFSET(entityState_t, animMovetype, 0xe0);
COD2_ASSERT_OFFSET(entityState_t, fTorsoHeight, 0xe4);
COD2_ASSERT_OFFSET(entityState_t, fTorsoPitch, 0xe8);
COD2_ASSERT_OFFSET(entityState_t, fWaistPitch, 0xec);

/* entityShared_s: Embedded at gentity+0xf0 directly after entityState_s. */
struct entityShared_s {
    byte linked;                                     /* 0x0000 gentity+0xf0; SV_LinkEntity (chunk 23); chunk 20 called it inUse */
    byte bmodel;                                     /* 0x0001 gentity+0xf1; chunk 20 SV_InitEntityBoundsAndLink sets 1; chunk 47 useBoundsFlag */
    byte svFlags;                                    /* 0x0002 gentity+0xf2; movzx eax,BYTE PTR [ent+0xf2] in SV_ArchiveSnapshot (0x0809b989) */
    byte unk_0x3[0x1];                               /* 0x0003 unrecovered */
    int clientMask[2];                               /* 0x0004 gentity+0xf4/+0xf8 copied to archived r.clientMask (0x0809b9c6/0x0809b9db) */
    byte inuse;                                      /* 0x000c gentity+0xfc; game chunks 046/048/049/051/052/103 */
    byte unk_0xd[0x3];                               /* 0x000d unrecovered */
    int broadcastTime;                               /* 0x0010 gentity+0x100; cmp [ent+0x100],0 -> archived svFlags|=8 (0x0809b999) */
    float mins[3];                                   /* 0x0014 gentity+0x104 (chunks 23/46/48/50/52/103) */
    float maxs[3];                                   /* 0x0020 gentity+0x110 */
    int contents;                                    /* 0x002c gentity+0x11c */
    float absmin[3];                                 /* 0x0030 gentity+0x120 -> archived r.absmin (0x0809b9f9) */
    float absmax[3];                                 /* 0x003c gentity+0x12c -> archived r.absmax (0x0809ba18) */
    float currentOrigin[3];                          /* 0x0048 gentity+0x138; script field origin */
    float currentAngles[3];                          /* 0x0054 gentity+0x144; script field angles */
    int ownerNum;                                    /* 0x0060 gentity+0x150; pass entity of G_TraceBox (chunks 23/46/47/50) */
    int eventTime;                                   /* 0x0064 gentity+0x154; level.time when eventSequence changes (chunk 46) */
};
COD2_ASSERT_SIZE(entityShared_t, 0x68);
COD2_ASSERT_OFFSET(entityShared_t, linked, 0x0);
COD2_ASSERT_OFFSET(entityShared_t, bmodel, 0x1);
COD2_ASSERT_OFFSET(entityShared_t, svFlags, 0x2);
COD2_ASSERT_OFFSET(entityShared_t, unk_0x3, 0x3);
COD2_ASSERT_OFFSET(entityShared_t, clientMask, 0x4);
COD2_ASSERT_OFFSET(entityShared_t, inuse, 0xc);
COD2_ASSERT_OFFSET(entityShared_t, unk_0xd, 0xd);
COD2_ASSERT_OFFSET(entityShared_t, broadcastTime, 0x10);
COD2_ASSERT_OFFSET(entityShared_t, mins, 0x14);
COD2_ASSERT_OFFSET(entityShared_t, maxs, 0x20);
COD2_ASSERT_OFFSET(entityShared_t, contents, 0x2c);
COD2_ASSERT_OFFSET(entityShared_t, absmin, 0x30);
COD2_ASSERT_OFFSET(entityShared_t, absmax, 0x3c);
COD2_ASSERT_OFFSET(entityShared_t, currentOrigin, 0x48);
COD2_ASSERT_OFFSET(entityShared_t, currentAngles, 0x54);
COD2_ASSERT_OFFSET(entityShared_t, ownerNum, 0x60);
COD2_ASSERT_OFFSET(entityShared_t, eventTime, 0x64);

struct archivedEntityShared_s {
    int svFlags;                                     /* 0x0000 archived+0xf0 = (byte)ent.r.svFlags | (broadcastTime ? 8 : 0) (0x0809b990) */
    int clientMask[2];                               /* 0x0004 archived+0xf4/+0xf8 (0x0809b9cc/0x0809b9e1) */
    float absmin[3];                                 /* 0x000c archived+0xfc (0x0809b9ed) */
    float absmax[3];                                 /* 0x0018 archived+0x108 (0x0809ba0c) */
};
COD2_ASSERT_SIZE(archivedEntityShared_t, 0x24);
COD2_ASSERT_OFFSET(archivedEntityShared_t, svFlags, 0x0);
COD2_ASSERT_OFFSET(archivedEntityShared_t, clientMask, 0x4);
COD2_ASSERT_OFFSET(archivedEntityShared_t, absmin, 0xc);
COD2_ASSERT_OFFSET(archivedEntityShared_t, absmax, 0x18);

/* archivedEntity_s: Element of svs.cachedSnapshotEntities (imul 0x114 at 0x0809b954) and svEntity_s.baseline; chunks 22/23 cachedEntity_s are this type. */
struct archivedEntity_s {
    entityState_t s;                                 /* 0x0000 rep movs 0xf0 (0x0809b973) */
    archivedEntityShared_t r;                        /* 0x00f0 see archivedEntityShared_s */
};
COD2_ASSERT_SIZE(archivedEntity_t, 0x114);
COD2_ASSERT_OFFSET(archivedEntity_t, s, 0x0);
COD2_ASSERT_OFFSET(archivedEntity_t, r, 0xf0);

/* trace_s: allsolid/startsolid order decided by the brush-trace disassembly (chunks 003/041/045/050 had them swapped). */
struct trace_s {
    float fraction;                                  /* 0x0000 Sweep_TraceBoxInit sets 1.0 and zeroes the rest (memset 0x24) */
    float normal[3];                                 /* 0x0004 hit plane normal (045/048/053 called it endpos: wrong) */
    int surfaceFlags;                                /* 0x0010 materials[n].surfaceFlags (+0x40): mov [ecx+0x10],eax at 0x0805a1b8 (chunk 005 plane_dist: wrong) */
    int contents;                                    /* 0x0014 mov [trace+0x14],brush->contents (0x0805a150) */
    struct dMaterial_s * material;                   /* 0x0018 &cm.materials[side->materialNum]: add eax,ds:0x8188dd0; mov [ecx+0x18],eax (0x0805a1d4); 0 for entity hits */
    uint16 entityNum;                                /* 0x001c 0x3fe world, 0x3ff none */
    uint16 partName;                                 /* 0x001e copied from the DObj trace record (+0x14); chunk 042 */
    uint16 partGroup;                                /* 0x0020 copied from the DObj trace record (+0x16); chunk 042 */
    byte allsolid;                                   /* 0x0022 set together with fraction=0 only when never left the solid: mov BYTE PTR [eax+0x22],0x1 at 0x0805a169 */
    byte startsolid;                                 /* 0x0023 set whenever the start is inside: mov BYTE PTR [eax+0x23],0x1 at 0x0805a15c */
};
COD2_ASSERT_SIZE(trace_t, 0x24);
COD2_ASSERT_OFFSET(trace_t, fraction, 0x0);
COD2_ASSERT_OFFSET(trace_t, normal, 0x4);
COD2_ASSERT_OFFSET(trace_t, surfaceFlags, 0x10);
COD2_ASSERT_OFFSET(trace_t, contents, 0x14);
COD2_ASSERT_OFFSET(trace_t, material, 0x18);
COD2_ASSERT_OFFSET(trace_t, entityNum, 0x1c);
COD2_ASSERT_OFFSET(trace_t, partName, 0x1e);
COD2_ASSERT_OFFSET(trace_t, partGroup, 0x20);
COD2_ASSERT_OFFSET(trace_t, allsolid, 0x22);
COD2_ASSERT_OFFSET(trace_t, startsolid, 0x23);

/* hudelem_s: Field names from the 32-entry netField table at 0x08142b80 (symbols.json recordFields32). */
struct hudelem_s {
    int type;                                        /* 0x0000 hudelemFields[1] 4 bits; 0 = free slot / record-array terminator */
    float x;                                         /* 0x0004 hudelemFields[3]; script field x@4 */
    float y;                                         /* 0x0008 hudelemFields[0] */
    float z;                                         /* 0x000c hudelemFields[14] */
    float fontScale;                                 /* 0x0010 hudelemFields[5]; HudElem_Init stores 1.0 */
    int font;                                        /* 0x0014 hudelemFields[18] */
    int alignOrg;                                    /* 0x0018 hudelemFields[15]; script alignx/aligny */
    int alignScreen;                                 /* 0x001c hudelemFields[4]; horzalign/vertalign */
    int color;                                       /* 0x0020 hudelemFields[2] rgba; HudElem_Init stores -1 */
    int fromColor;                                   /* 0x0024 hudelemFields[10] */
    int fadeStartTime;                               /* 0x0028 hudelemFields[9] */
    int fadeTime;                                    /* 0x002c hudelemFields[11] */
    int label;                                       /* 0x0030 hudelemFields[22] */
    int width;                                       /* 0x0034 hudelemFields[7] */
    int height;                                      /* 0x0038 hudelemFields[8] */
    int materialIndex;                               /* 0x003c hudelemFields[6] */
    int fromWidth;                                   /* 0x0040 hudelemFields[23] */
    int fromHeight;                                  /* 0x0044 hudelemFields[21] */
    int scaleStartTime;                              /* 0x0048 hudelemFields[19] */
    int scaleTime;                                   /* 0x004c hudelemFields[20] */
    float fromX;                                     /* 0x0050 hudelemFields[26] */
    float fromY;                                     /* 0x0054 hudelemFields[27] */
    int fromAlignOrg;                                /* 0x0058 hudelemFields[29] */
    int fromAlignScreen;                             /* 0x005c hudelemFields[28] */
    int moveStartTime;                               /* 0x0060 hudelemFields[24] */
    int moveTime;                                    /* 0x0064 hudelemFields[25] */
    int time;                                        /* 0x0068 hudelemFields[13] */
    int duration;                                    /* 0x006c hudelemFields[30] */
    float value;                                     /* 0x0070 hudelemFields[12] */
    int text;                                        /* 0x0074 hudelemFields[17] */
    float sort;                                      /* 0x0078 hudelemFields[16] */
    int foreground;                                  /* 0x007c hudelemFields[31] 1 bit */
};
COD2_ASSERT_SIZE(hudelem_t, 0x80);
COD2_ASSERT_OFFSET(hudelem_t, type, 0x0);
COD2_ASSERT_OFFSET(hudelem_t, x, 0x4);
COD2_ASSERT_OFFSET(hudelem_t, y, 0x8);
COD2_ASSERT_OFFSET(hudelem_t, z, 0xc);
COD2_ASSERT_OFFSET(hudelem_t, fontScale, 0x10);
COD2_ASSERT_OFFSET(hudelem_t, font, 0x14);
COD2_ASSERT_OFFSET(hudelem_t, alignOrg, 0x18);
COD2_ASSERT_OFFSET(hudelem_t, alignScreen, 0x1c);
COD2_ASSERT_OFFSET(hudelem_t, color, 0x20);
COD2_ASSERT_OFFSET(hudelem_t, fromColor, 0x24);
COD2_ASSERT_OFFSET(hudelem_t, fadeStartTime, 0x28);
COD2_ASSERT_OFFSET(hudelem_t, fadeTime, 0x2c);
COD2_ASSERT_OFFSET(hudelem_t, label, 0x30);
COD2_ASSERT_OFFSET(hudelem_t, width, 0x34);
COD2_ASSERT_OFFSET(hudelem_t, height, 0x38);
COD2_ASSERT_OFFSET(hudelem_t, materialIndex, 0x3c);
COD2_ASSERT_OFFSET(hudelem_t, fromWidth, 0x40);
COD2_ASSERT_OFFSET(hudelem_t, fromHeight, 0x44);
COD2_ASSERT_OFFSET(hudelem_t, scaleStartTime, 0x48);
COD2_ASSERT_OFFSET(hudelem_t, scaleTime, 0x4c);
COD2_ASSERT_OFFSET(hudelem_t, fromX, 0x50);
COD2_ASSERT_OFFSET(hudelem_t, fromY, 0x54);
COD2_ASSERT_OFFSET(hudelem_t, fromAlignOrg, 0x58);
COD2_ASSERT_OFFSET(hudelem_t, fromAlignScreen, 0x5c);
COD2_ASSERT_OFFSET(hudelem_t, moveStartTime, 0x60);
COD2_ASSERT_OFFSET(hudelem_t, moveTime, 0x64);
COD2_ASSERT_OFFSET(hudelem_t, time, 0x68);
COD2_ASSERT_OFFSET(hudelem_t, duration, 0x6c);
COD2_ASSERT_OFFSET(hudelem_t, value, 0x70);
COD2_ASSERT_OFFSET(hudelem_t, text, 0x74);
COD2_ASSERT_OFFSET(hudelem_t, sort, 0x78);
COD2_ASSERT_OFFSET(hudelem_t, foreground, 0x7c);

/* game_hudelem_s: g_hudelems[0x400] at 0x08628f80: 0x400*0x8c ends exactly at the next global 0x0864bf80. */
struct game_hudelem_s {
    hudelem_t elem;                                  /* 0x0000 first 0x80 bytes copied into the playerState hud lists (HudElem_BuildClientLists 0x08104882) */
    int clientNum;                                   /* 0x0080 owner key; 0x3ff = all (HudElem_Alloc) */
    int team;                                        /* 0x0084 second key, compared with the viewer team */
    int archived;                                    /* 0x0088 script field archived@0x88; HudElem_Init stores 1; selects ps.hudArchival */
};
COD2_ASSERT_SIZE(game_hudelem_t, 0x8c);
COD2_ASSERT_OFFSET(game_hudelem_t, elem, 0x0);
COD2_ASSERT_OFFSET(game_hudelem_t, clientNum, 0x80);
COD2_ASSERT_OFFSET(game_hudelem_t, team, 0x84);
COD2_ASSERT_OFFSET(game_hudelem_t, archived, 0x88);

/* objective_s: Table at 0x08142a20 has 6 entries (symbols.json says 3); stride 0x1c observed. */
struct objective_s {
    int state;                                       /* 0x0000 MSG_WriteBits(state,3) before the field group */
    float origin[3];                                 /* 0x0004 objectiveFields[0..2] */
    int entNum;                                      /* 0x0010 objectiveFields[4] 10 bits */
    int teamNum;                                     /* 0x0014 objectiveFields[5] 4 bits */
    int icon;                                        /* 0x0018 objectiveFields[3] 12 bits */
};
COD2_ASSERT_SIZE(objective_t, 0x1c);
COD2_ASSERT_OFFSET(objective_t, state, 0x0);
COD2_ASSERT_OFFSET(objective_t, origin, 0x4);
COD2_ASSERT_OFFSET(objective_t, entNum, 0x10);
COD2_ASSERT_OFFSET(objective_t, teamNum, 0x14);
COD2_ASSERT_OFFSET(objective_t, icon, 0x18);

struct MantleState {
    float yaw;                                       /* 0x0000 netfield mantleState.yaw (ps+0x5bc) */
    int timer;                                       /* 0x0004 netfield mantleState.timer; Mantle_UpdateMove adds pml.msec */
    int transIndex;                                  /* 0x0008 netfield mantleState.transIndex; index into MantleStage[7] @0x08184d00 */
    int flags;                                       /* 0x000c netfield mantleState.flags */
};
COD2_ASSERT_SIZE(MantleState, 0x10);
COD2_ASSERT_OFFSET(MantleState, yaw, 0x0);
COD2_ASSERT_OFFSET(MantleState, timer, 0x4);
COD2_ASSERT_OFFSET(MantleState, transIndex, 0x8);
COD2_ASSERT_OFFSET(MantleState, flags, 0xc);

/* playerState_s: Names from the 105-entry netField table 0x08142520; size 0x26a8 = memcpy in SV_BuildClientSnapshot / memset in ClientSpawn. Only 0x5a0 is unrecovered. Chunks 39-44 declared truncated prefixes (0xec..0x5d8). */
struct playerState_s {
    int commandTime;                                 /* 0x0000 netfield */
    int pm_type;                                     /* 0x0004 netfield 8 bits */
    int bobCycle;                                    /* 0x0008 netfield 8 bits (chunk 40 pmFlags here was an entityState view) */
    int pm_flags;                                    /* 0x000c netfield 27 bits */
    int pm_time;                                     /* 0x0010 netfield 16 bits signed (chunk 40 jumpPhase) */
    float origin[3];                                 /* 0x0014 netfield */
    float velocity[3];                               /* 0x0020 netfield */
    float oldVelocity[2];                            /* 0x002c not networked; pmove smoothed velocity (pmove.md) */
    int weaponTime;                                  /* 0x0034 netfield */
    int weaponDelay;                                 /* 0x0038 netfield */
    int grenadeTimeLeft;                             /* 0x003c netfield */
    int weaponRestrictKickTime;                      /* 0x0040 netfield */
    int foliageSoundTime;                            /* 0x0044 netfield */
    int gravity;                                     /* 0x0048 netfield */
    float leanf;                                     /* 0x004c netfield (chunks 8/41 moveAxis) */
    int speed;                                       /* 0x0050 netfield */
    int delta_angles[3];                             /* 0x0054 netfield */
    int groundEntityNum;                             /* 0x0060 netfield 10 bits (chunk 49 usingTurretIndex: wrong) */
    float vLadderVec[3];                             /* 0x0064 netfield */
    int jumpTime;                                    /* 0x0070 netfield */
    float jumpOriginZ;                               /* 0x0074 netfield */
    int legsTimer;                                   /* 0x0078 netfield */
    int legsAnim;                                    /* 0x007c netfield */
    int torsoTimer;                                  /* 0x0080 netfield */
    int torsoAnim;                                   /* 0x0084 netfield */
    int legsAnimDuration;                            /* 0x0088 not networked (pmove.md) */
    int torsoAnimDuration;                           /* 0x008c not networked (pmove.md) */
    int damageTimer;                                 /* 0x0090 netfield */
    int damageDuration;                              /* 0x0094 netfield */
    int flinchYaw;                                   /* 0x0098 netfield */
    int movementDir;                                 /* 0x009c netfield 8 bits signed */
    int eFlags;                                      /* 0x00a0 netfield 24 bits (chunks 8/41/44 buttonBits: wrong) */
    int eventSequence;                               /* 0x00a4 netfield */
    int events[4];                                   /* 0x00a8 netfield */
    int eventParms[4];                               /* 0x00b8 netfield */
    int oldEventSequence;                            /* 0x00c8 not networked; provisional name (chunk 46 eventSequenceSent) */
    int clientNum;                                   /* 0x00cc netfield */
    int offHandIndex;                                /* 0x00d0 netfield 7 bits */
    int weapon;                                      /* 0x00d4 netfield 7 bits (chunk 41 linkedEntityNum: wrong) */
    int weaponstate;                                 /* 0x00d8 netfield 5 bits */
    float fWeaponPosFrac;                            /* 0x00dc netfield */
    int adsDelayTime;                                /* 0x00e0 netfield */
    int viewmodelIndex;                              /* 0x00e4 netfield 8 bits */
    float viewangles[3];                             /* 0x00e8 netfield angle16 */
    int viewHeightTarget;                            /* 0x00f4 netfield */
    float viewHeightCurrent;                         /* 0x00f8 netfield */
    int viewHeightLerpTime;                          /* 0x00fc netfield */
    int viewHeightLerpTarget;                        /* 0x0100 netfield */
    int viewHeightLerpDown;                          /* 0x0104 netfield */
    float viewHeightLerpPosAdj;                      /* 0x0108 netfield */
    float viewAngleClampBase[2];                     /* 0x010c netfield */
    float viewAngleClampRange[2];                    /* 0x0114 netfield */
    int damageEvent;                                 /* 0x011c netfield */
    int damageYaw;                                   /* 0x0120 netfield */
    int damagePitch;                                 /* 0x0124 netfield */
    int damageCount;                                 /* 0x0128 netfield */
    int stats[6];                                    /* 0x012c MSG_WriteDeltaPlayerState 6-bit mask group; [0] health, [2] maxHealth, [5] spawn count (game chunks) */
    int ammo[128];                                   /* 0x0144 four 16-dword delta groups + non-networked tail (network.md) */
    int ammoclip[128];                               /* 0x0344 four groups at +0x344 */
    unsigned int weapons[4];                         /* 0x0544 owned-weapon bitset; netfields weapons[0],[1] */
    byte weaponslots[8];                             /* 0x0554 netfields weaponslots[0] (+0x554), [4] (+0x558) */
    unsigned int weaponrechamber[4];                 /* 0x055c netfields weaponrechamber[0],[1] */
    float mins[3];                                   /* 0x056c netfield */
    float maxs[3];                                   /* 0x0578 netfield */
    float proneDirection;                            /* 0x0584 netfield */
    float proneDirectionPitch;                       /* 0x0588 netfield */
    float proneTorsoPitch;                           /* 0x058c netfield */
    int viewlocked;                                  /* 0x0590 netfield */
    int viewlocked_entNum;                           /* 0x0594 netfield (chunk 46 linkedEntityNum) */
    int cursorHint;                                  /* 0x0598 netfield */
    int cursorHintString;                            /* 0x059c netfield */
    int cursorHintEntIndex;                          /* 0x05a0 not networked; chunk 053 useTargetNum / 052 lastTarget_5a0 (gclient+0x5a0) [name inferred] */
    int iCompassFriendInfo;                          /* 0x05a4 netfield */
    float fTorsoHeight;                              /* 0x05a8 netfield */
    float fTorsoPitch;                               /* 0x05ac netfield */
    float fWaistPitch;                               /* 0x05b0 netfield */
    float holdBreathScale;                           /* 0x05b4 netfield */
    int holdBreathTimer;                             /* 0x05b8 netfield */
    MantleState mantleState;                         /* 0x05bc netfields mantleState.* */
    int entityEventSequence;                         /* 0x05cc not networked; event replay cursor in BG_PlayerStateToEntityState (pmove.md) */
    int weapAnim;                                    /* 0x05d0 netfield */
    float aimSpreadScale;                            /* 0x05d4 netfield */
    int shellshockIndex;                             /* 0x05d8 netfield */
    int shellshockTime;                              /* 0x05dc netfield */
    int shellshockDuration;                          /* 0x05e0 netfield */
    objective_t objective[16];                       /* 0x05e4 16 x 0x1c delta-coded records */
    int deltaTime;                                   /* 0x07a4 netfield */
    hudelem_t hudCurrent[31];                        /* 0x07a8 unflagged list (game_hudelem.archived == 0) built by HudElem_BuildClientLists; coded second */
    hudelem_t hudArchival[31];                       /* 0x1728 flagged list (archived != 0); coded first */
};
COD2_ASSERT_SIZE(playerState_t, 0x26a8);
COD2_ASSERT_OFFSET(playerState_t, commandTime, 0x0);
COD2_ASSERT_OFFSET(playerState_t, pm_type, 0x4);
COD2_ASSERT_OFFSET(playerState_t, bobCycle, 0x8);
COD2_ASSERT_OFFSET(playerState_t, pm_flags, 0xc);
COD2_ASSERT_OFFSET(playerState_t, pm_time, 0x10);
COD2_ASSERT_OFFSET(playerState_t, origin, 0x14);
COD2_ASSERT_OFFSET(playerState_t, velocity, 0x20);
COD2_ASSERT_OFFSET(playerState_t, oldVelocity, 0x2c);
COD2_ASSERT_OFFSET(playerState_t, weaponTime, 0x34);
COD2_ASSERT_OFFSET(playerState_t, weaponDelay, 0x38);
COD2_ASSERT_OFFSET(playerState_t, grenadeTimeLeft, 0x3c);
COD2_ASSERT_OFFSET(playerState_t, weaponRestrictKickTime, 0x40);
COD2_ASSERT_OFFSET(playerState_t, foliageSoundTime, 0x44);
COD2_ASSERT_OFFSET(playerState_t, gravity, 0x48);
COD2_ASSERT_OFFSET(playerState_t, leanf, 0x4c);
COD2_ASSERT_OFFSET(playerState_t, speed, 0x50);
COD2_ASSERT_OFFSET(playerState_t, delta_angles, 0x54);
COD2_ASSERT_OFFSET(playerState_t, groundEntityNum, 0x60);
COD2_ASSERT_OFFSET(playerState_t, vLadderVec, 0x64);
COD2_ASSERT_OFFSET(playerState_t, jumpTime, 0x70);
COD2_ASSERT_OFFSET(playerState_t, jumpOriginZ, 0x74);
COD2_ASSERT_OFFSET(playerState_t, legsTimer, 0x78);
COD2_ASSERT_OFFSET(playerState_t, legsAnim, 0x7c);
COD2_ASSERT_OFFSET(playerState_t, torsoTimer, 0x80);
COD2_ASSERT_OFFSET(playerState_t, torsoAnim, 0x84);
COD2_ASSERT_OFFSET(playerState_t, legsAnimDuration, 0x88);
COD2_ASSERT_OFFSET(playerState_t, torsoAnimDuration, 0x8c);
COD2_ASSERT_OFFSET(playerState_t, damageTimer, 0x90);
COD2_ASSERT_OFFSET(playerState_t, damageDuration, 0x94);
COD2_ASSERT_OFFSET(playerState_t, flinchYaw, 0x98);
COD2_ASSERT_OFFSET(playerState_t, movementDir, 0x9c);
COD2_ASSERT_OFFSET(playerState_t, eFlags, 0xa0);
COD2_ASSERT_OFFSET(playerState_t, eventSequence, 0xa4);
COD2_ASSERT_OFFSET(playerState_t, events, 0xa8);
COD2_ASSERT_OFFSET(playerState_t, eventParms, 0xb8);
COD2_ASSERT_OFFSET(playerState_t, oldEventSequence, 0xc8);
COD2_ASSERT_OFFSET(playerState_t, clientNum, 0xcc);
COD2_ASSERT_OFFSET(playerState_t, offHandIndex, 0xd0);
COD2_ASSERT_OFFSET(playerState_t, weapon, 0xd4);
COD2_ASSERT_OFFSET(playerState_t, weaponstate, 0xd8);
COD2_ASSERT_OFFSET(playerState_t, fWeaponPosFrac, 0xdc);
COD2_ASSERT_OFFSET(playerState_t, adsDelayTime, 0xe0);
COD2_ASSERT_OFFSET(playerState_t, viewmodelIndex, 0xe4);
COD2_ASSERT_OFFSET(playerState_t, viewangles, 0xe8);
COD2_ASSERT_OFFSET(playerState_t, viewHeightTarget, 0xf4);
COD2_ASSERT_OFFSET(playerState_t, viewHeightCurrent, 0xf8);
COD2_ASSERT_OFFSET(playerState_t, viewHeightLerpTime, 0xfc);
COD2_ASSERT_OFFSET(playerState_t, viewHeightLerpTarget, 0x100);
COD2_ASSERT_OFFSET(playerState_t, viewHeightLerpDown, 0x104);
COD2_ASSERT_OFFSET(playerState_t, viewHeightLerpPosAdj, 0x108);
COD2_ASSERT_OFFSET(playerState_t, viewAngleClampBase, 0x10c);
COD2_ASSERT_OFFSET(playerState_t, viewAngleClampRange, 0x114);
COD2_ASSERT_OFFSET(playerState_t, damageEvent, 0x11c);
COD2_ASSERT_OFFSET(playerState_t, damageYaw, 0x120);
COD2_ASSERT_OFFSET(playerState_t, damagePitch, 0x124);
COD2_ASSERT_OFFSET(playerState_t, damageCount, 0x128);
COD2_ASSERT_OFFSET(playerState_t, stats, 0x12c);
COD2_ASSERT_OFFSET(playerState_t, ammo, 0x144);
COD2_ASSERT_OFFSET(playerState_t, ammoclip, 0x344);
COD2_ASSERT_OFFSET(playerState_t, weapons, 0x544);
COD2_ASSERT_OFFSET(playerState_t, weaponslots, 0x554);
COD2_ASSERT_OFFSET(playerState_t, weaponrechamber, 0x55c);
COD2_ASSERT_OFFSET(playerState_t, mins, 0x56c);
COD2_ASSERT_OFFSET(playerState_t, maxs, 0x578);
COD2_ASSERT_OFFSET(playerState_t, proneDirection, 0x584);
COD2_ASSERT_OFFSET(playerState_t, proneDirectionPitch, 0x588);
COD2_ASSERT_OFFSET(playerState_t, proneTorsoPitch, 0x58c);
COD2_ASSERT_OFFSET(playerState_t, viewlocked, 0x590);
COD2_ASSERT_OFFSET(playerState_t, viewlocked_entNum, 0x594);
COD2_ASSERT_OFFSET(playerState_t, cursorHint, 0x598);
COD2_ASSERT_OFFSET(playerState_t, cursorHintString, 0x59c);
COD2_ASSERT_OFFSET(playerState_t, cursorHintEntIndex, 0x5a0);
COD2_ASSERT_OFFSET(playerState_t, iCompassFriendInfo, 0x5a4);
COD2_ASSERT_OFFSET(playerState_t, fTorsoHeight, 0x5a8);
COD2_ASSERT_OFFSET(playerState_t, fTorsoPitch, 0x5ac);
COD2_ASSERT_OFFSET(playerState_t, fWaistPitch, 0x5b0);
COD2_ASSERT_OFFSET(playerState_t, holdBreathScale, 0x5b4);
COD2_ASSERT_OFFSET(playerState_t, holdBreathTimer, 0x5b8);
COD2_ASSERT_OFFSET(playerState_t, mantleState, 0x5bc);
COD2_ASSERT_OFFSET(playerState_t, entityEventSequence, 0x5cc);
COD2_ASSERT_OFFSET(playerState_t, weapAnim, 0x5d0);
COD2_ASSERT_OFFSET(playerState_t, aimSpreadScale, 0x5d4);
COD2_ASSERT_OFFSET(playerState_t, shellshockIndex, 0x5d8);
COD2_ASSERT_OFFSET(playerState_t, shellshockTime, 0x5dc);
COD2_ASSERT_OFFSET(playerState_t, shellshockDuration, 0x5e0);
COD2_ASSERT_OFFSET(playerState_t, objective, 0x5e4);
COD2_ASSERT_OFFSET(playerState_t, deltaTime, 0x7a4);
COD2_ASSERT_OFFSET(playerState_t, hudCurrent, 0x7a8);
COD2_ASSERT_OFFSET(playerState_t, hudArchival, 0x1728);

/* clientState_s: Table at 0x08141f60 (22 entries); a zero record of 0x5c is used when from==NULL. */
struct clientState_s {
    int clientNum;                                   /* 0x0000 6-bit number on the wire */
    int team;                                        /* 0x0004 clientStateFields[0] 2 bits */
    int modelindex;                                  /* 0x0008 clientStateFields[3] */
    int attachModelIndex[6];                         /* 0x000c clientStateFields[5,4,18..21] */
    int attachTagIndex[6];                           /* 0x0024 clientStateFields[13..17,12] */
    char name[32];                                   /* 0x003c coded as 8 dwords; script field name = gclient+0x2784 */
};
COD2_ASSERT_SIZE(clientState_t, 0x5c);
COD2_ASSERT_OFFSET(clientState_t, clientNum, 0x0);
COD2_ASSERT_OFFSET(clientState_t, team, 0x4);
COD2_ASSERT_OFFSET(clientState_t, modelindex, 0x8);
COD2_ASSERT_OFFSET(clientState_t, attachModelIndex, 0xc);
COD2_ASSERT_OFFSET(clientState_t, attachTagIndex, 0x24);
COD2_ASSERT_OFFSET(clientState_t, name, 0x3c);

/* pmove_s: Size 0xf8 from the memset in ClientThink_real (chunk 46). */
struct pmove_s {
    playerState_t * ps;                              /* 0x0000 first word (all pmove chunks) */
    usercmd_t cmd;                                   /* 0x0004 chunks 41/42/46 embed usercmd at +4 */
    usercmd_t oldcmd;                                /* 0x0020 chunks 42/46 */
    int tracemask;                                   /* 0x003c PM_Trace; ClientThink sets 0x2810011 */
    int numtouch;                                    /* 0x0040 PM_AddTouchEnt; cleared in PM_Pmove 0x080e9464 */
    int touchents[32];                               /* 0x0044 PM_AddTouchEnt (32 slots) */
    float mins[3];                                   /* 0x00c4 PM_CheckDuck */
    float maxs[3];                                   /* 0x00d0 PM_CheckDuck */
    float xyspeed;                                   /* 0x00dc PM_Footsteps (chunk 42) */
    int proneChange;                                 /* 0x00e0 PM_CheckDuck sets 1 on stance change (chunk 42 stanceChanged) */
    byte handler;                                    /* 0x00e4 row index of the trace dispatch table 0x081869c0 (PM_TraceRaw 0x080e1308) */
    byte mantleStarted;                              /* 0x00e5 chunk 46 spawnEvent / chunk 40 mantleActive: same byte */
    byte unk_0xe6[0x2];                              /* 0x00e6 unrecovered */
    float mantleEndPos[3];                           /* 0x00e8 chunk 46 eventOrigin / chunk 40 mantleTarget */
    int mantleDuration;                              /* 0x00f4 chunk 46 eventTime / chunk 40 mantleEndHeight (a duration in ms) */
};
COD2_ASSERT_SIZE(pmove_t, 0xf8);
COD2_ASSERT_OFFSET(pmove_t, ps, 0x0);
COD2_ASSERT_OFFSET(pmove_t, cmd, 0x4);
COD2_ASSERT_OFFSET(pmove_t, oldcmd, 0x20);
COD2_ASSERT_OFFSET(pmove_t, tracemask, 0x3c);
COD2_ASSERT_OFFSET(pmove_t, numtouch, 0x40);
COD2_ASSERT_OFFSET(pmove_t, touchents, 0x44);
COD2_ASSERT_OFFSET(pmove_t, mins, 0xc4);
COD2_ASSERT_OFFSET(pmove_t, maxs, 0xd0);
COD2_ASSERT_OFFSET(pmove_t, xyspeed, 0xdc);
COD2_ASSERT_OFFSET(pmove_t, proneChange, 0xe0);
COD2_ASSERT_OFFSET(pmove_t, handler, 0xe4);
COD2_ASSERT_OFFSET(pmove_t, mantleStarted, 0xe5);
COD2_ASSERT_OFFSET(pmove_t, unk_0xe6, 0xe6);
COD2_ASSERT_OFFSET(pmove_t, mantleEndPos, 0xe8);
COD2_ASSERT_OFFSET(pmove_t, mantleDuration, 0xf4);

/* pml_s: PM_SingleStep memset(&pml,0,0x7c). */
struct pml_s {
    float forward[3];                                /* 0x0000 PM_SingleStep AngleVectors */
    float right[3];                                  /* 0x000c PM_SingleStep */
    float up[3];                                     /* 0x0018 PM_SingleStep */
    float frametime;                                 /* 0x0024 msec*0.001 */
    int msec;                                        /* 0x0028 clamped 1..200 (chunk 40 UserCmd040.deltaMsec) */
    int walking;                                     /* 0x002c chunks 41/42/43 */
    int groundPlane;                                 /* 0x0030 chunks 41/42/43 */
    int almostGroundPlane;                           /* 0x0034 chunk 41 groundTraceHit */
    trace_t groundTrace;                             /* 0x0038 PM_CorrectAllSolid copies 9 dwords to +0x38 (0x080e34c6) */
    float impactSpeed;                               /* 0x005c PM_SlideMove (0x080e9c42) */
    float previous_origin[3];                        /* 0x0060 PM_SingleStep */
    float previous_velocity[3];                      /* 0x006c PM_SingleStep */
    int previous_waterlevel;                         /* 0x0078 tail of memset(&pml,0,0x7c); no access seen, name from Q3 lineage [I] */
};
COD2_ASSERT_SIZE(pml_t, 0x7c);
COD2_ASSERT_OFFSET(pml_t, forward, 0x0);
COD2_ASSERT_OFFSET(pml_t, right, 0xc);
COD2_ASSERT_OFFSET(pml_t, up, 0x18);
COD2_ASSERT_OFFSET(pml_t, frametime, 0x24);
COD2_ASSERT_OFFSET(pml_t, msec, 0x28);
COD2_ASSERT_OFFSET(pml_t, walking, 0x2c);
COD2_ASSERT_OFFSET(pml_t, groundPlane, 0x30);
COD2_ASSERT_OFFSET(pml_t, almostGroundPlane, 0x34);
COD2_ASSERT_OFFSET(pml_t, groundTrace, 0x38);
COD2_ASSERT_OFFSET(pml_t, impactSpeed, 0x5c);
COD2_ASSERT_OFFSET(pml_t, previous_origin, 0x60);
COD2_ASSERT_OFFSET(pml_t, previous_velocity, 0x6c);
COD2_ASSERT_OFFSET(pml_t, previous_waterlevel, 0x78);

struct item_ent_s {
    int ammoCount;                                   /* 0x0000 gentity+0x1a8 (chunk 48 reserveAmmo) */
    uint16 index;                                    /* 0x0004 gentity+0x1ac weapon/item index (048 weaponDef, 053 weaponIndex, 104 itemIndex) */
    byte unk_0x6[0x5a];                              /* 0x0006 unrecovered tail */
};
COD2_ASSERT_SIZE(item_ent_t, 0x60);
COD2_ASSERT_OFFSET(item_ent_t, ammoCount, 0x0);
COD2_ASSERT_OFFSET(item_ent_t, index, 0x4);
COD2_ASSERT_OFFSET(item_ent_t, unk_0x6, 0x6);

struct mover_ent_s {
    float moveDecelTime;                             /* 0x0000 gentity+0x1a8 script_mover (chunk 51) */
    float rotDecelTime;                              /* 0x0004 +0x1ac (51) */
    float moveSpeed;                                 /* 0x0008 +0x1b0 (51) */
    float rotSpeed;                                  /* 0x000c +0x1b4 (51) */
    float moveCruiseTime;                            /* 0x0010 +0x1b8 (51) */
    float rotCruiseTime;                             /* 0x0014 +0x1bc (51) */
    float moveAccelEnd[3];                           /* 0x0018 +0x1c0 (51) */
    float moveCruiseEnd[3];                          /* 0x0024 +0x1cc (51) */
    float moveDest[3];                               /* 0x0030 +0x1d8 (51) */
    float rotAccelEnd[3];                            /* 0x003c +0x1e4 (51) */
    float rotCruiseEnd[3];                           /* 0x0048 +0x1f0 (51) */
    float rotDest[3];                                /* 0x0054 +0x1fc (51) */
};
COD2_ASSERT_SIZE(mover_ent_t, 0x60);
COD2_ASSERT_OFFSET(mover_ent_t, moveDecelTime, 0x0);
COD2_ASSERT_OFFSET(mover_ent_t, rotDecelTime, 0x4);
COD2_ASSERT_OFFSET(mover_ent_t, moveSpeed, 0x8);
COD2_ASSERT_OFFSET(mover_ent_t, rotSpeed, 0xc);
COD2_ASSERT_OFFSET(mover_ent_t, moveCruiseTime, 0x10);
COD2_ASSERT_OFFSET(mover_ent_t, rotCruiseTime, 0x14);
COD2_ASSERT_OFFSET(mover_ent_t, moveAccelEnd, 0x18);
COD2_ASSERT_OFFSET(mover_ent_t, moveCruiseEnd, 0x24);
COD2_ASSERT_OFFSET(mover_ent_t, moveDest, 0x30);
COD2_ASSERT_OFFSET(mover_ent_t, rotAccelEnd, 0x3c);
COD2_ASSERT_OFFSET(mover_ent_t, rotCruiseEnd, 0x48);
COD2_ASSERT_OFFSET(mover_ent_t, rotDest, 0x54);

struct trigger_ent_s {
    int threshold;                                   /* 0x0000 gentity+0x1a8 spawn key "threshold" (chunk 107) */
    int accumulate;                                  /* 0x0004 +0x1ac spawn key "accumulate" (107) */
    int timestamp;                                   /* 0x0008 +0x1b0 trigger_hurt re-arm time (107 nextHurtTime) */
    byte unk_0xc[0x54];                              /* 0x000c unrecovered tail */
};
COD2_ASSERT_SIZE(trigger_ent_t, 0x60);
COD2_ASSERT_OFFSET(trigger_ent_t, threshold, 0x0);
COD2_ASSERT_OFFSET(trigger_ent_t, accumulate, 0x4);
COD2_ASSERT_OFFSET(trigger_ent_t, timestamp, 0x8);
COD2_ASSERT_OFFSET(trigger_ent_t, unk_0xc, 0xc);

union gentity_u {
    item_ent_t item;                                 /* 0x0000 items (chunks 48/53/104) */
    mover_ent_t mover;                               /* 0x0000 script_mover (chunk 51) */
    trigger_ent_t trigger;                           /* 0x0000 trigger_* (chunk 107) */
    float turretRateMs;                              /* 0x0000 turrets: +0x1a8 rateMs (chunks 49/104) [view only] */
    byte raw[0x60];                                  /* 0x0000 other classes (045 box_a/box_b at +0x1ec/+0x1f8, 053 ownerNum at +0x1b4, 052 limit/count) [unknown] */
};
COD2_ASSERT_SIZE(gentity_u, 0x60);
COD2_ASSERT_OFFSET(gentity_u, item, 0x0);
COD2_ASSERT_OFFSET(gentity_u, mover, 0x0);
COD2_ASSERT_OFFSET(gentity_u, trigger, 0x0);
COD2_ASSERT_OFFSET(gentity_u, turretRateMs, 0x0);
COD2_ASSERT_OFFSET(gentity_u, raw, 0x0);

/* gentity_s: Size 0x230 observed: 165 x imul reg,0x230 (e.g. 0x080f4b3a) and memset 0x8c000 = 1024 x 0x230 (0x0810935b). Chunk 023 gentity_s (0x274) is refuted: no 0x274 stride exists in the binary. */
struct gentity_s {
    entityState_t s;                                 /* 0x0000 number = (e-0x08716400)/0x230 (G_InitEntityRecord 0x0811e85c); SV copies 0xf0 bytes */
    entityShared_t r;                                /* 0x00f0 see entityShared_s */
    gclient_s * client;                              /* 0x0158 every game chunk */
    struct turret_s * pTurretInfo;                   /* 0x015c 049 turret_s pointer; 052 frees it via fn_0810c2ec in G_FreeEntity */
    byte physicsObject;                              /* 0x0160 049 occupied / 103 field160 / 053 isPlayer [semantics unknown] */
    byte takedamage;                                 /* 0x0161 G_Damage (047); player_die sets 1 */
    byte active;                                     /* 0x0162 048 touchPending / 053 useState / 049 turretPending [semantics unknown] */
    byte unk_0x163;                                  /* 0x0163 not named by any chunk */
    byte model;                                      /* 0x0164 script field model type 8 (table 0x0815e820) */
    byte attachIgnoreCollision;                      /* 0x0165 052 attachFlags / 105 */
    byte handler;                                    /* 0x0166 index into the entity class table 0x08187d00 (045/047/049/050/052/053/103) */
    byte team;                                       /* 0x0167 053 teamNum / 106 triggerTeam */
    uint16 classname;                                /* 0x0168 script string; field table classname->0x168 */
    uint16 target;                                   /* 0x016a field table target->0x16a */
    uint16 targetname;                               /* 0x016c field table targetname->0x16c */
    byte unk_0x16e[0x2];                             /* 0x016e unrecovered */
    int spawnflags;                                  /* 0x0170 field table spawnflags->0x170 */
    int flags;                                       /* 0x0174 G_Damage flags&1 god, &2 demigod (047) */
    int eventTime;                                   /* 0x0178 G_FreeEntity sets level.time */
    int freeAfterEvent;                              /* 0x017c 049; 052 active17c */
    int unlinkAfterEvent;                            /* 0x0180 049 field180 */
    int clipmask;                                    /* 0x0184 046 clipmask; 048 traceMask default 0x491 */
    int framenum;                                    /* 0x0188 compared with level.framenum (049) */
    gentity_s * parent;                              /* 0x018c 050 parent; 052 lockedBy; 046 field_18c */
    int nextthink;                                   /* 0x0190 G_RunThink (048/049/103) */
    int health;                                      /* 0x0194 046/047/048/051/102/103/104/105/106/107 */
    int maxHealth;                                   /* 0x0198 105 maxHealth; 049 field198 */
    int damage;                                      /* 0x019c field table dmg->0x19c */
    int count;                                       /* 0x01a0 field table count->0x1a0; items: clip ammo (048 clipAmmo) */
    int unk_0x1a4;                                   /* 0x01a4 not named */
    gentity_u u;                                     /* 0x01a8 per-class union 0x1a8..0x207 (item/mover/trigger/turret views) */
    struct game_attach_node_s * tagInfo;             /* 0x0208 052 attachLink (owner at +0); 049 driverRef */
    gentity_s * tagChildren;                         /* 0x020c 052 attachChildren */
    byte attachModelNames[7];                        /* 0x0210 052 7 slots (046 assumed 6) */
    byte unk_0x217[0x1];                             /* 0x0217 unrecovered */
    uint16 attachTagNames[7];                        /* 0x0218 052; 051 scrStrings released by G_FreeEntityFields */
    byte unk_0x226[0x2];                             /* 0x0226 unrecovered */
    int spawnSerial;                                 /* 0x0228 052 generation / 049 spawnSerial */
    gentity_s * nextFree;                            /* 0x022c G_Spawn / G_FreeEntity free list (052 freeNext) */
};
COD2_ASSERT_SIZE(gentity_t, 0x230);
COD2_ASSERT_OFFSET(gentity_t, s, 0x0);
COD2_ASSERT_OFFSET(gentity_t, r, 0xf0);
COD2_ASSERT_OFFSET(gentity_t, client, 0x158);
COD2_ASSERT_OFFSET(gentity_t, pTurretInfo, 0x15c);
COD2_ASSERT_OFFSET(gentity_t, physicsObject, 0x160);
COD2_ASSERT_OFFSET(gentity_t, takedamage, 0x161);
COD2_ASSERT_OFFSET(gentity_t, active, 0x162);
COD2_ASSERT_OFFSET(gentity_t, unk_0x163, 0x163);
COD2_ASSERT_OFFSET(gentity_t, model, 0x164);
COD2_ASSERT_OFFSET(gentity_t, attachIgnoreCollision, 0x165);
COD2_ASSERT_OFFSET(gentity_t, handler, 0x166);
COD2_ASSERT_OFFSET(gentity_t, team, 0x167);
COD2_ASSERT_OFFSET(gentity_t, classname, 0x168);
COD2_ASSERT_OFFSET(gentity_t, target, 0x16a);
COD2_ASSERT_OFFSET(gentity_t, targetname, 0x16c);
COD2_ASSERT_OFFSET(gentity_t, unk_0x16e, 0x16e);
COD2_ASSERT_OFFSET(gentity_t, spawnflags, 0x170);
COD2_ASSERT_OFFSET(gentity_t, flags, 0x174);
COD2_ASSERT_OFFSET(gentity_t, eventTime, 0x178);
COD2_ASSERT_OFFSET(gentity_t, freeAfterEvent, 0x17c);
COD2_ASSERT_OFFSET(gentity_t, unlinkAfterEvent, 0x180);
COD2_ASSERT_OFFSET(gentity_t, clipmask, 0x184);
COD2_ASSERT_OFFSET(gentity_t, framenum, 0x188);
COD2_ASSERT_OFFSET(gentity_t, parent, 0x18c);
COD2_ASSERT_OFFSET(gentity_t, nextthink, 0x190);
COD2_ASSERT_OFFSET(gentity_t, health, 0x194);
COD2_ASSERT_OFFSET(gentity_t, maxHealth, 0x198);
COD2_ASSERT_OFFSET(gentity_t, damage, 0x19c);
COD2_ASSERT_OFFSET(gentity_t, count, 0x1a0);
COD2_ASSERT_OFFSET(gentity_t, unk_0x1a4, 0x1a4);
COD2_ASSERT_OFFSET(gentity_t, u, 0x1a8);
COD2_ASSERT_OFFSET(gentity_t, tagInfo, 0x208);
COD2_ASSERT_OFFSET(gentity_t, tagChildren, 0x20c);
COD2_ASSERT_OFFSET(gentity_t, attachModelNames, 0x210);
COD2_ASSERT_OFFSET(gentity_t, unk_0x217, 0x217);
COD2_ASSERT_OFFSET(gentity_t, attachTagNames, 0x218);
COD2_ASSERT_OFFSET(gentity_t, unk_0x226, 0x226);
COD2_ASSERT_OFFSET(gentity_t, spawnSerial, 0x228);
COD2_ASSERT_OFFSET(gentity_t, nextFree, 0x22c);

/* gclient_s: Size 0x28a4 observed: 41 x 0x28a4 immediates (e.g. 0x080f8ea5) and memset 0xa2900 = 64 x 0x28a4 (0x0810938e). Chunks 51/102 called this struct client_s (collides with the server client_s, 0xb1064). */
struct gclient_s {
    playerState_t ps;                                /* 0x0000 SV_GameClientNum returns this base; memcpy 0x26a8 */
    int sessionState;                                /* 0x26a8 Player_SetSessionState 0x080f7efc: 0 playing,1 dead,2 spectator,3 intermission */
    int spectatorClient;                             /* 0x26ac script field spectatorclient->0x26ac */
    int killCamEntity;                               /* 0x26b0 chunk 46 field_26b0 [name inferred] */
    int32 archiveTime;                               /* 0x26b4 script field archivetime: stored by fistp (1000 * float), read by fild/int (Player_SetScaledValue 0x080f8420; requests/L22-2) */
    int score;                                       /* 0x26b8 script field score->0x26b8 */
    int deaths;                                      /* 0x26bc script field deaths->0x26bc */
    uint16 pers;                                     /* 0x26c0 script field pers->0x26c0 (object id) */
    byte unk_0x26c2[0x2];                            /* 0x26c2 unrecovered */
    int connected;                                   /* 0x26c4 0 free,1 connecting,2 connected (046/047/048) */
    usercmd_t cmd;                                   /* 0x26c8 chunk 46 */
    usercmd_t oldcmd;                                /* 0x26e4 chunk 46 */
    int localClient;                                 /* 0x2700 chunk 46 isBot (ClientUserinfoChanged) */
    int predictItemPickup;                           /* 0x2704 chunks 46/48 unk_2704 */
    char newnetname[32];                             /* 0x2708 chunk 46 name (userinfo name before cleaning) */
    int maxHealth;                                   /* 0x2728 script field maxhealth->0x2728 */
    int enterTime;                                   /* 0x272c chunk 103 setentertime */
    byte unk_0x2730[0x4];                            /* 0x2730 unrecovered */
    int voteCount;                                   /* 0x2734 chunk 47 */
    byte unk_0x2738[0x4];                            /* 0x2738 unrecovered */
    int viewmodelIndex;                              /* 0x273c chunk 103; 046 field_273c */
    int noSpectate;                                  /* 0x2740 chunk 46 spectateMask */
    byte unk_0x2744[0x4];                            /* 0x2744 unrecovered */
    clientState_t cs;                                /* 0x2748 Client_GetUnk2748Addr (048); ClientDisconnect clears 0x2748..0x27a3; name at +0x2784 */
    int psOffsetTime;                                /* 0x27a4 script field psoffsettime->0x27a4 */
    int lastFollowedClient;                          /* 0x27a8 chunk 46 followClient / 047 */
    int noclip;                                      /* 0x27ac ClientThink_real skips touch (046/047) */
    int ufo;                                         /* 0x27b0 chunk 47 */
    int bFrozen;                                     /* 0x27b4 chunk 103 freezecontrols -> pm_flags 0x8000 (046) */
    int lastCmdTime;                                 /* 0x27b8 chunk 46 lastThinkTime */
    int buttons;                                     /* 0x27bc chunks 46/103 */
    int oldbuttons;                                  /* 0x27c0 chunk 46 */
    int latched_buttons;                             /* 0x27c4 chunk 46 buttonsPressed */
    int buttonsSinceLastFrame;                       /* 0x27c8 chunk 46 buttonsLatched / 103 oldButtons */
    float oldOrigin[3];                              /* 0x27cc chunk 46 */
    float fGunPitch;                                 /* 0x27d8 chunk 46 viewAnglesOut[0] [name inferred] */
    float fGunYaw;                                   /* 0x27dc chunk 46 viewAnglesOut[1] [name inferred] */
    int damageTaken;                                 /* 0x27e0 chunk 103 damageTaken / 106 viewKickDamage / 045 recoil_samples [semantics unresolved] */
    float damageDir[3];                              /* 0x27e4 chunks 103 damageDir / 106 viewKickDir / 045 recoil_angles */
    int damageDirIsSelf;                             /* 0x27f0 chunk 103 [inferred] */
    byte unk_0x27f4[0xc];                            /* 0x27f4 unrecovered */
    int inactivityTime;                              /* 0x2800 ClientSpawn: g_inactivity*1000+time (045/046) */
    int inactivityWarning;                           /* 0x2804 chunk 045 inactivity_warned */
    int lastVoiceTime;                               /* 0x2808 chunk 46 lastSayTime / 103 lastTalkTime */
    byte unk_0x280c[0x4];                            /* 0x280c unrecovered */
    float currentAimSpreadScale;                     /* 0x2810 chunks 46/53 lerpFraction [name inferred] */
    byte unk_0x2814[0x8];                            /* 0x2814 unrecovered */
    int unk_0x281c;                                  /* 0x281c mov DWORD PTR [eax+0x281c],0x0 (chunk 46 field_281c) */
    byte unk_0x2820[0xc];                            /* 0x2820 unrecovered */
    gentity_s * pLookatEnt;                          /* 0x282c chunk 52 ptr_282c / 103 lookAtEnt */
    int useHoldEntity;                               /* 0x2830 ClientConnect writes 0x3ff (046 field_2830, 053 useEntityNum) */
    int useHoldTime;                                 /* 0x2834 chunk 53 useStartTime */
    int useButtonDone;                               /* 0x2838 chunk 46 targetIndex [unknown] */
    int pingTime;                                    /* 0x283c chunk 103 pingTime; mov load/store only (046 field_283c) */
    int recoilStamp;                                 /* 0x2840 chunk 045 recoil_stamp / 046 field_2840 */
    float recoilPitchOut;                            /* 0x2844 fstp DWORD PTR [ebx+0x2844] (045 float; 046 typed int) */
    float recoilPitchNeg;                            /* 0x2848 fstp DWORD PTR [ebx+0x2848] (045 float; 046 typed int) */
    float viewVec284c[3];                            /* 0x284c chunk 46 field_284c (viewCalcB context) [semantics unknown] */
    float viewVec2858[3];                            /* 0x2858 chunk 46 field_2858 [semantics unknown] */
    float viewVec2864[3];                            /* 0x2864 chunk 46 field_2864 [semantics unknown] */
    float viewVec2870[3];                            /* 0x2870 chunk 46 field_2870 [semantics unknown] */
    int viewInt287c;                                 /* 0x287c chunk 46 field_287c [semantics unknown] */
    float viewVec2880[3];                            /* 0x2880 chunk 46 field_2880 [semantics unknown] */
    float viewVec288c[3];                            /* 0x288c chunk 46 field_288c [semantics unknown] */
    int viewInt2898;                                 /* 0x2898 chunk 46 field_2898 (declared int[3], overlaps the next fields); no direct access in the disassembly */
    int lastActivityTime;                            /* 0x289c chunk 045 last_activity_time; cmp eax,[edx+0x289c] */
    int nextUseTime;                                 /* 0x28a0 chunk 053 nextUseTime */
};
COD2_ASSERT_SIZE(gclient_t, 0x28a4);
COD2_ASSERT_OFFSET(gclient_t, ps, 0x0);
COD2_ASSERT_OFFSET(gclient_t, sessionState, 0x26a8);
COD2_ASSERT_OFFSET(gclient_t, spectatorClient, 0x26ac);
COD2_ASSERT_OFFSET(gclient_t, killCamEntity, 0x26b0);
COD2_ASSERT_OFFSET(gclient_t, archiveTime, 0x26b4);
COD2_ASSERT_OFFSET(gclient_t, score, 0x26b8);
COD2_ASSERT_OFFSET(gclient_t, deaths, 0x26bc);
COD2_ASSERT_OFFSET(gclient_t, pers, 0x26c0);
COD2_ASSERT_OFFSET(gclient_t, unk_0x26c2, 0x26c2);
COD2_ASSERT_OFFSET(gclient_t, connected, 0x26c4);
COD2_ASSERT_OFFSET(gclient_t, cmd, 0x26c8);
COD2_ASSERT_OFFSET(gclient_t, oldcmd, 0x26e4);
COD2_ASSERT_OFFSET(gclient_t, localClient, 0x2700);
COD2_ASSERT_OFFSET(gclient_t, predictItemPickup, 0x2704);
COD2_ASSERT_OFFSET(gclient_t, newnetname, 0x2708);
COD2_ASSERT_OFFSET(gclient_t, maxHealth, 0x2728);
COD2_ASSERT_OFFSET(gclient_t, enterTime, 0x272c);
COD2_ASSERT_OFFSET(gclient_t, unk_0x2730, 0x2730);
COD2_ASSERT_OFFSET(gclient_t, voteCount, 0x2734);
COD2_ASSERT_OFFSET(gclient_t, unk_0x2738, 0x2738);
COD2_ASSERT_OFFSET(gclient_t, viewmodelIndex, 0x273c);
COD2_ASSERT_OFFSET(gclient_t, noSpectate, 0x2740);
COD2_ASSERT_OFFSET(gclient_t, unk_0x2744, 0x2744);
COD2_ASSERT_OFFSET(gclient_t, cs, 0x2748);
COD2_ASSERT_OFFSET(gclient_t, psOffsetTime, 0x27a4);
COD2_ASSERT_OFFSET(gclient_t, lastFollowedClient, 0x27a8);
COD2_ASSERT_OFFSET(gclient_t, noclip, 0x27ac);
COD2_ASSERT_OFFSET(gclient_t, ufo, 0x27b0);
COD2_ASSERT_OFFSET(gclient_t, bFrozen, 0x27b4);
COD2_ASSERT_OFFSET(gclient_t, lastCmdTime, 0x27b8);
COD2_ASSERT_OFFSET(gclient_t, buttons, 0x27bc);
COD2_ASSERT_OFFSET(gclient_t, oldbuttons, 0x27c0);
COD2_ASSERT_OFFSET(gclient_t, latched_buttons, 0x27c4);
COD2_ASSERT_OFFSET(gclient_t, buttonsSinceLastFrame, 0x27c8);
COD2_ASSERT_OFFSET(gclient_t, oldOrigin, 0x27cc);
COD2_ASSERT_OFFSET(gclient_t, fGunPitch, 0x27d8);
COD2_ASSERT_OFFSET(gclient_t, fGunYaw, 0x27dc);
COD2_ASSERT_OFFSET(gclient_t, damageTaken, 0x27e0);
COD2_ASSERT_OFFSET(gclient_t, damageDir, 0x27e4);
COD2_ASSERT_OFFSET(gclient_t, damageDirIsSelf, 0x27f0);
COD2_ASSERT_OFFSET(gclient_t, unk_0x27f4, 0x27f4);
COD2_ASSERT_OFFSET(gclient_t, inactivityTime, 0x2800);
COD2_ASSERT_OFFSET(gclient_t, inactivityWarning, 0x2804);
COD2_ASSERT_OFFSET(gclient_t, lastVoiceTime, 0x2808);
COD2_ASSERT_OFFSET(gclient_t, unk_0x280c, 0x280c);
COD2_ASSERT_OFFSET(gclient_t, currentAimSpreadScale, 0x2810);
COD2_ASSERT_OFFSET(gclient_t, unk_0x2814, 0x2814);
COD2_ASSERT_OFFSET(gclient_t, unk_0x281c, 0x281c);
COD2_ASSERT_OFFSET(gclient_t, unk_0x2820, 0x2820);
COD2_ASSERT_OFFSET(gclient_t, pLookatEnt, 0x282c);
COD2_ASSERT_OFFSET(gclient_t, useHoldEntity, 0x2830);
COD2_ASSERT_OFFSET(gclient_t, useHoldTime, 0x2834);
COD2_ASSERT_OFFSET(gclient_t, useButtonDone, 0x2838);
COD2_ASSERT_OFFSET(gclient_t, pingTime, 0x283c);
COD2_ASSERT_OFFSET(gclient_t, recoilStamp, 0x2840);
COD2_ASSERT_OFFSET(gclient_t, recoilPitchOut, 0x2844);
COD2_ASSERT_OFFSET(gclient_t, recoilPitchNeg, 0x2848);
COD2_ASSERT_OFFSET(gclient_t, viewVec284c, 0x284c);
COD2_ASSERT_OFFSET(gclient_t, viewVec2858, 0x2858);
COD2_ASSERT_OFFSET(gclient_t, viewVec2864, 0x2864);
COD2_ASSERT_OFFSET(gclient_t, viewVec2870, 0x2870);
COD2_ASSERT_OFFSET(gclient_t, viewInt287c, 0x287c);
COD2_ASSERT_OFFSET(gclient_t, viewVec2880, 0x2880);
COD2_ASSERT_OFFSET(gclient_t, viewVec288c, 0x288c);
COD2_ASSERT_OFFSET(gclient_t, viewInt2898, 0x2898);
COD2_ASSERT_OFFSET(gclient_t, lastActivityTime, 0x289c);
COD2_ASSERT_OFFSET(gclient_t, nextUseTime, 0x28a0);

/* level_locals_s: G_InitGame memset 0x3624 bytes at 0x0864c380 (immediate at 0x081090d8). Chunk 46 asserted only a 0x31c prefix. Most fields past 0xb2c are unrecovered. */
struct level_locals_s {
    gclient_t * clients;                             /* 0x0000 G_InitGame stores 0x087a2500 */
    gentity_t * gentities;                           /* 0x0004 G_InitGame stores 0x08716400 */
    byte unk_0x8[0x4];                               /* 0x0008 unrecovered */
    int num_entities;                                /* 0x000c 0x0864c38c; init 0x48; G_Spawn increments */
    gentity_t * firstFreeEnt;                        /* 0x0010 0x0864c390 */
    gentity_t * lastFreeEnt;                         /* 0x0014 0x0864c394 */
    int logFile;                                     /* 0x0018 0x0864c398 */
    int initializing;                                /* 0x001c 0x0864c39c (046 field_1c) */
    int clientIsSpawning;                            /* 0x0020 0x0864c3a0 (046 field_20 around ClientSpawn) */
    objective_t objectives[16];                      /* 0x0024 g_0864c3a4 (int[16*7] in symbols.json) = 16 x 0x1c; chunk 050 cod2_objective_s / 105 objective105_s use the objective_s layout */
    int maxclients;                                  /* 0x01e4 level_maxclients 0x0864c564 */
    int framenum;                                    /* 0x01e8 0x0864c568 G_RunFrame */
    int time;                                        /* 0x01ec 0x0864c56c */
    int previousTime;                                /* 0x01f0 0x0864c570 */
    int frameTime;                                   /* 0x01f4 0x0864c574 */
    int startTime;                                   /* 0x01f8 0x0864c578 */
    byte unk_0x1fc[0x4];                             /* 0x01fc unrecovered */
    int teamScores[2];                               /* 0x0200 046 field_200/204 -> configstrings 5/6 (g_0864c580/584) [name inferred] */
    byte unk_0x208[0x4];                             /* 0x0208 unrecovered */
    int lastAimRefreshTime;                          /* 0x020c 0x0864c58c (symbols.json) */
    int unk_0x210;                                   /* 0x0210 0x0864c590 */
    int manualNameChange;                            /* 0x0214 046 field_214 nameLockFlag (0x0864c594) */
    int numConnectedClients;                         /* 0x0218 0x0864c598 (046 numSortedClients) */
    int sortedClients[64];                           /* 0x021c 0x0864c59c */
    char voteString[1024];                           /* 0x031c 0x0864c69c */
    char voteDisplayString[1024];                    /* 0x071c 0x0864ca9c */
    int voteTime;                                    /* 0x0b1c 0x0864ce9c */
    int voteExecuteTime;                             /* 0x0b20 0x0864cea0 level_pendingVoteExecute */
    int voteYes;                                     /* 0x0b24 0x0864cea4 */
    int voteNo;                                      /* 0x0b28 0x0864cea8 */
    byte unk_0xb2c[0x81c];                           /* 0x0b2c unrecovered */
    byte spawnVars[0x208];                           /* 0x1348 g_0864d6c8 spawnVarState_t (chunk 51 size 0x208; parsing flag, count +4, pairs +8) */
    byte unk_0x1550[0x804];                          /* 0x1550 unrecovered */
    int exitLevelArg;                                /* 0x1d54 g_0864e0d4: argument stored by the map/exitlevel builtins */
    gentity_t * droppedWeapons[32];                  /* 0x1d58 g_0864e0d8: dropped weapon list (count g_maxDroppedWeapons); 0x80 bytes to the next global */
    float cs12Bound;                                 /* 0x1dd8 g_0864e158: set in G_ParseConfigstring12Bounds [meaning unknown] */
    float cs12BoundSq;                               /* 0x1ddc g_0864e15c: square of cs12Bound */
    byte unk_0x1de0[0x4];                            /* 0x1de0 unrecovered */
    int tempEntityIndex;                             /* 0x1de4 g_0864e164: ring index 0..7 in G_AllocTempEntity */
    byte pairQueue[0xc00];                           /* 0x1de8 g_0864e168: 0x100 x 12-byte entity_pair_s written by G_QueueEntityPairEvent (subsystem-owned type, bytes here) */
    byte pendingPairs[0xc00];                        /* 0x29e8 g_0864ed68: 0x100 x entity_pair_s processed by G_RunFrame */
    int pairQueueCount;                              /* 0x35e8 g_0864f968: compared with 0x100 in G_QueueEntityPairEvent */
    byte unk_0x35ec[0x4];                            /* 0x35ec unrecovered */
    int exitLevelState;                              /* 0x35f0 g_0864f970: 1 restart, 2 map, 3 exitlevel */
    int radiusDamageSource;                          /* 0x35f4 g_0864f974: set around G_RadiusDamage */
    int radiusDamageTag;                             /* 0x35f8 g_0864f978: GScr_SetRadiusDamageTag */
    int weaponPrecacheFlagA;                         /* 0x35fc g_0864f97c: set to 1 by G_PrecacheWeapon */
    int weaponPrecacheFlagB;                         /* 0x3600 g_0864f980: set to 1 by G_PrecacheWeapon */
    int currentThinkEntity;                          /* 0x3604 g_0864f984: delete refuses to free it */
    int sentinelSlot;                                /* 0x3608 g_0864f988: -1 sentinel, freed in G_ShutdownGame */
    int handleSlot;                                  /* 0x360c g_0864f98c: freed with Com_Free_A in G_ShutdownGame */
    byte parseRecord[0x14];                          /* 0x3610 g_0864f990: ParseSave-sized record (stride 0x14); level ends at 0x3624 */
};
COD2_ASSERT_SIZE(level_locals_t, 0x3624);
COD2_ASSERT_OFFSET(level_locals_t, clients, 0x0);
COD2_ASSERT_OFFSET(level_locals_t, gentities, 0x4);
COD2_ASSERT_OFFSET(level_locals_t, unk_0x8, 0x8);
COD2_ASSERT_OFFSET(level_locals_t, num_entities, 0xc);
COD2_ASSERT_OFFSET(level_locals_t, firstFreeEnt, 0x10);
COD2_ASSERT_OFFSET(level_locals_t, lastFreeEnt, 0x14);
COD2_ASSERT_OFFSET(level_locals_t, logFile, 0x18);
COD2_ASSERT_OFFSET(level_locals_t, initializing, 0x1c);
COD2_ASSERT_OFFSET(level_locals_t, clientIsSpawning, 0x20);
COD2_ASSERT_OFFSET(level_locals_t, objectives, 0x24);
COD2_ASSERT_OFFSET(level_locals_t, maxclients, 0x1e4);
COD2_ASSERT_OFFSET(level_locals_t, framenum, 0x1e8);
COD2_ASSERT_OFFSET(level_locals_t, time, 0x1ec);
COD2_ASSERT_OFFSET(level_locals_t, previousTime, 0x1f0);
COD2_ASSERT_OFFSET(level_locals_t, frameTime, 0x1f4);
COD2_ASSERT_OFFSET(level_locals_t, startTime, 0x1f8);
COD2_ASSERT_OFFSET(level_locals_t, unk_0x1fc, 0x1fc);
COD2_ASSERT_OFFSET(level_locals_t, teamScores, 0x200);
COD2_ASSERT_OFFSET(level_locals_t, unk_0x208, 0x208);
COD2_ASSERT_OFFSET(level_locals_t, lastAimRefreshTime, 0x20c);
COD2_ASSERT_OFFSET(level_locals_t, unk_0x210, 0x210);
COD2_ASSERT_OFFSET(level_locals_t, manualNameChange, 0x214);
COD2_ASSERT_OFFSET(level_locals_t, numConnectedClients, 0x218);
COD2_ASSERT_OFFSET(level_locals_t, sortedClients, 0x21c);
COD2_ASSERT_OFFSET(level_locals_t, voteString, 0x31c);
COD2_ASSERT_OFFSET(level_locals_t, voteDisplayString, 0x71c);
COD2_ASSERT_OFFSET(level_locals_t, voteTime, 0xb1c);
COD2_ASSERT_OFFSET(level_locals_t, voteExecuteTime, 0xb20);
COD2_ASSERT_OFFSET(level_locals_t, voteYes, 0xb24);
COD2_ASSERT_OFFSET(level_locals_t, voteNo, 0xb28);
COD2_ASSERT_OFFSET(level_locals_t, unk_0xb2c, 0xb2c);
COD2_ASSERT_OFFSET(level_locals_t, spawnVars, 0x1348);
COD2_ASSERT_OFFSET(level_locals_t, unk_0x1550, 0x1550);
COD2_ASSERT_OFFSET(level_locals_t, exitLevelArg, 0x1d54);
COD2_ASSERT_OFFSET(level_locals_t, droppedWeapons, 0x1d58);
COD2_ASSERT_OFFSET(level_locals_t, cs12Bound, 0x1dd8);
COD2_ASSERT_OFFSET(level_locals_t, cs12BoundSq, 0x1ddc);
COD2_ASSERT_OFFSET(level_locals_t, unk_0x1de0, 0x1de0);
COD2_ASSERT_OFFSET(level_locals_t, tempEntityIndex, 0x1de4);
COD2_ASSERT_OFFSET(level_locals_t, pairQueue, 0x1de8);
COD2_ASSERT_OFFSET(level_locals_t, pendingPairs, 0x29e8);
COD2_ASSERT_OFFSET(level_locals_t, pairQueueCount, 0x35e8);
COD2_ASSERT_OFFSET(level_locals_t, unk_0x35ec, 0x35ec);
COD2_ASSERT_OFFSET(level_locals_t, exitLevelState, 0x35f0);
COD2_ASSERT_OFFSET(level_locals_t, radiusDamageSource, 0x35f4);
COD2_ASSERT_OFFSET(level_locals_t, radiusDamageTag, 0x35f8);
COD2_ASSERT_OFFSET(level_locals_t, weaponPrecacheFlagA, 0x35fc);
COD2_ASSERT_OFFSET(level_locals_t, weaponPrecacheFlagB, 0x3600);
COD2_ASSERT_OFFSET(level_locals_t, currentThinkEntity, 0x3604);
COD2_ASSERT_OFFSET(level_locals_t, sentinelSlot, 0x3608);
COD2_ASSERT_OFFSET(level_locals_t, handleSlot, 0x360c);
COD2_ASSERT_OFFSET(level_locals_t, parseRecord, 0x3610);

struct reliableCommand_s {
    char command[1024];                              /* 0x0000 command letter + payload (chunks 20,21,22) */
    int cmdTime;                                     /* 0x0400 svs.time when queued (SV_QueueReliableCommand) */
    int cmdType;                                     /* 0x0404 nonzero = keep (chunk 21 SV_CompactReliableCommands) */
};
COD2_ASSERT_SIZE(reliableCommand_t, 0x408);
COD2_ASSERT_OFFSET(reliableCommand_t, command, 0x0);
COD2_ASSERT_OFFSET(reliableCommand_t, cmdTime, 0x400);
COD2_ASSERT_OFFSET(reliableCommand_t, cmdType, 0x404);

struct clientSnapshot_s {
    playerState_t ps;                                /* 0x0000 memcpy(&frame->ps, SV_GameClientNum(), 0x26a8) (chunks 22/23) */
    int num_entities;                                /* 0x26a8 chunks 22/23 */
    int num_clients;                                 /* 0x26ac chunks 22/23 */
    int first_entity;                                /* 0x26b0 chunks 22/23 */
    int first_client;                                /* 0x26b4 chunks 22/23 */
    int messageSent;                                 /* 0x26b8 SV_CalcPings / SV_SendMessageToClient */
    int messageAcked;                                /* 0x26bc lea edx,[eax+0x234f0] = client+0x20e34+0x26bc (SV_UserMove 0x08091038) */
    int messageSize;                                 /* 0x26c0 SV_SendMessageToClient */
};
COD2_ASSERT_SIZE(clientSnapshot_t, 0x26c4);
COD2_ASSERT_OFFSET(clientSnapshot_t, ps, 0x0);
COD2_ASSERT_OFFSET(clientSnapshot_t, num_entities, 0x26a8);
COD2_ASSERT_OFFSET(clientSnapshot_t, num_clients, 0x26ac);
COD2_ASSERT_OFFSET(clientSnapshot_t, first_entity, 0x26b0);
COD2_ASSERT_OFFSET(clientSnapshot_t, first_client, 0x26b4);
COD2_ASSERT_OFFSET(clientSnapshot_t, messageSent, 0x26b8);
COD2_ASSERT_OFFSET(clientSnapshot_t, messageAcked, 0x26bc);
COD2_ASSERT_OFFSET(clientSnapshot_t, messageSize, 0x26c0);

/* voicepacket_s: Packed: client_s.voicePackets[40] spans 0xae714..0xb0fdc (40 x 0x105). */
struct voicepacket_s {
    byte talker;                                     /* 0x0000 chunk 23 */
    byte data[256];                                  /* 0x0001 chunk 23 */
    int dataSize;                                    /* 0x0101 unaligned dword (packed record) */
} __attribute__((packed));
COD2_ASSERT_SIZE(voicepacket_t, 0x105);
COD2_ASSERT_OFFSET(voicepacket_t, talker, 0x0);
COD2_ASSERT_OFFSET(voicepacket_t, data, 0x1);
COD2_ASSERT_OFFSET(voicepacket_t, dataSize, 0x101);

/* client_s: Size 0xb1064 observed: 83 immediates (stride, memset in SV_DirectConnect, Com_Calloc(maxclients*0xb1064)). Chunk 023 (0xb10dc) laid voicepackets out with a 0x108 stride (unpacked); chunks 051/102 used this tag for gclient_s. */
struct client_s {
    int state;                                       /* 0x0000 CS_FREE 0, ZOMBIE 1, CONNECTED 2, PRIMED 3, ACTIVE 4 */
    int sendAsActive;                                /* 0x0004 chunk 22 snapshotActiveFlag (SV_WriteSnapshotToClient) */
    const char * delayDropMsg;                       /* 0x0008 chunk 23 dropReason, passed to SV_DropClient by SV_SendMessageToClient (chunk 20 typed it int pendingDrop) */
    char userinfo[1024];                             /* 0x000c chunks 20,21,101 */
    reliableCommand_t reliableCommands[128];         /* 0x040c stride 0x408 */
    int reliableSequence;                            /* 0x2080c chunks 20,21,22 */
    int reliableAcknowledge;                         /* 0x20810 SV_UpdateServerCommandsToClient (0x080988bb) */
    int reliableSent;                                /* 0x20814 chunk 22; chunk 21 reliableSearchCursor */
    int messageAcknowledge;                          /* 0x20818 read after the header in SV_PacketEvent (chunk 20 messageSeq) */
    int gamestateMessageNum;                         /* 0x2081c netchan.outgoingSequence at gamestate send; -1 at connect */
    int challenge;                                   /* 0x20820 SV_Netchan_Encode/Decode key */
    usercmd_t lastUsercmd;                           /* 0x20824 chunks 19,20,22 */
    int lastClientCommand;                           /* 0x20840 SV_ClientCommand; first long of gamestate/snapshot (chunk 23 reliableAcknowledge: wrong, 0x0809ae4c) */
    char lastClientCommandString[1024];              /* 0x20844 SV_Netchan_Encode string source */
    gentity_t * gentity;                             /* 0x20c44 chunks 20,22,23 (chunk 21 unk_20c44) */
    char name[32];                                   /* 0x20c48 chunks 20,21,23; downloadName follows at 0x20c68 */
    char downloadName[64];                           /* 0x20c68 chunks 20,101 */
    int download;                                    /* 0x20ca8 file handle (chunk 101) */
    int downloadSize;                                /* 0x20cac chunk 20 */
    int downloadCount;                               /* 0x20cb0 chunk 20 downloadPos */
    int downloadClientBlock;                         /* 0x20cb4 chunk 101 */
    int downloadCurrentBlock;                        /* 0x20cb8 chunk 20 downloadNextBlock */
    int downloadXmitBlock;                           /* 0x20cbc chunks 20,101 */
    byte * downloadBlocks[8];                        /* 0x20cc0 0x800-byte blocks */
    int downloadBlockSize[8];                        /* 0x20ce0 chunks 20,101 */
    int downloadEOF;                                 /* 0x20d00 chunk 20 downloadQueuedEof */
    int downloadSendTime;                            /* 0x20d04 chunks 20,101 */
    char downloadURL[256];                           /* 0x20d08 chunk 20 redirectUrl */
    int wwwOk;                                       /* 0x20e08 cl_wwwDownload userinfo stored here (0x080909ca/0x080909f3) */
    int downloadingWWW;                              /* 0x20e0c chunk 101 bWWWDl (0x0808f994) */
    int clientDownloadingWWW;                        /* 0x20e10 chunk 101 bWWWing (0x0808f9ee) */
    int wwwFallback;                                 /* 0x20e14 chunk 101 bFallback (0x0808fb13) */
    int deltaMessage;                                /* 0x20e18 SV_UserMove stores messageAcknowledge or -1; delta base in SV_WriteSnapshotToClient */
    int nextReliableTime;                            /* 0x20e1c svs.time+800 flood gate (chunk 20 textFloodTime) */
    int lastPacketTime;                              /* 0x20e20 svs.time per packet (chunks 19,21,22,101) */
    int lastConnectTime;                             /* 0x20e24 chunk 20 */
    int nextSnapshotTime;                            /* 0x20e28 chunk 23; chunk 20 enterWorldTime (writes svs.time = send now) */
    int rateDelayed;                                 /* 0x20e2c chunks 22,23 */
    int timeoutCount;                                /* 0x20e30 chunk 22 */
    clientSnapshot_t frames[32];                     /* 0x20e34 32 x 0x26c4 ends exactly at 0x6e6b4 */
    int ping;                                        /* 0x6e6b4 chunks 21,22,101 */
    int rate;                                        /* 0x6e6b8 clamp 1000..90000 (chunks 20,23,101) */
    int snapshotMsec;                                /* 0x6e6bc chunks 20,23 */
    int pureAuthentic;                               /* 0x6e6c0 0 unknown,1 verified,2 failed (chunks 20,101) */
    netchan_t netchan;                               /* 0x6e6c4 remoteAddress at 0x6e6d4, qport at 0x6e6e8, prof at 0xae700 */
    int guid;                                        /* 0xae704 chunks 19,20,101 */
    uint16 scriptId;                                 /* 0xae708 ScrVar_AllocArray result; 2nd arg of ClientConnect; released by Scr_ReleaseVarThunk */
    byte unk_0xae70a[0x2];                           /* 0xae70a unrecovered */
    int bIsTestClient;                               /* 0xae70c SV_AddBot sets 1 (chunk 20 isBot, 22 noTimeout) */
    int serverId;                                    /* 0xae710 byte read from the packet, compared with 0x08423008 (chunk 22) */
    voicepacket_t voicePackets[40];                  /* 0xae714 chunk 23; packed 0x105 records */
    int voicePacketCount;                            /* 0xb0fdc chunk 23 */
    byte muteList[64];                               /* 0xb0fe0 chunk 23 muteFlags */
    byte sendVoice;                                  /* 0xb1020 cl_voice && rate>=5000 (chunk 20 voiceEnabled) */
    char authKeyA[33];                               /* 0xb1021 copied from challenge keyA (chunk 20) */
    char authKeyB[33];                               /* 0xb1042 chunk 20 asserts the offset */
    byte unk_0xb1063[0x1];                           /* 0xb1063 unrecovered tail */
};
COD2_ASSERT_SIZE(client_t, 0xb1064);
COD2_ASSERT_OFFSET(client_t, state, 0x0);
COD2_ASSERT_OFFSET(client_t, sendAsActive, 0x4);
COD2_ASSERT_OFFSET(client_t, delayDropMsg, 0x8);
COD2_ASSERT_OFFSET(client_t, userinfo, 0xc);
COD2_ASSERT_OFFSET(client_t, reliableCommands, 0x40c);
COD2_ASSERT_OFFSET(client_t, reliableSequence, 0x2080c);
COD2_ASSERT_OFFSET(client_t, reliableAcknowledge, 0x20810);
COD2_ASSERT_OFFSET(client_t, reliableSent, 0x20814);
COD2_ASSERT_OFFSET(client_t, messageAcknowledge, 0x20818);
COD2_ASSERT_OFFSET(client_t, gamestateMessageNum, 0x2081c);
COD2_ASSERT_OFFSET(client_t, challenge, 0x20820);
COD2_ASSERT_OFFSET(client_t, lastUsercmd, 0x20824);
COD2_ASSERT_OFFSET(client_t, lastClientCommand, 0x20840);
COD2_ASSERT_OFFSET(client_t, lastClientCommandString, 0x20844);
COD2_ASSERT_OFFSET(client_t, gentity, 0x20c44);
COD2_ASSERT_OFFSET(client_t, name, 0x20c48);
COD2_ASSERT_OFFSET(client_t, downloadName, 0x20c68);
COD2_ASSERT_OFFSET(client_t, download, 0x20ca8);
COD2_ASSERT_OFFSET(client_t, downloadSize, 0x20cac);
COD2_ASSERT_OFFSET(client_t, downloadCount, 0x20cb0);
COD2_ASSERT_OFFSET(client_t, downloadClientBlock, 0x20cb4);
COD2_ASSERT_OFFSET(client_t, downloadCurrentBlock, 0x20cb8);
COD2_ASSERT_OFFSET(client_t, downloadXmitBlock, 0x20cbc);
COD2_ASSERT_OFFSET(client_t, downloadBlocks, 0x20cc0);
COD2_ASSERT_OFFSET(client_t, downloadBlockSize, 0x20ce0);
COD2_ASSERT_OFFSET(client_t, downloadEOF, 0x20d00);
COD2_ASSERT_OFFSET(client_t, downloadSendTime, 0x20d04);
COD2_ASSERT_OFFSET(client_t, downloadURL, 0x20d08);
COD2_ASSERT_OFFSET(client_t, wwwOk, 0x20e08);
COD2_ASSERT_OFFSET(client_t, downloadingWWW, 0x20e0c);
COD2_ASSERT_OFFSET(client_t, clientDownloadingWWW, 0x20e10);
COD2_ASSERT_OFFSET(client_t, wwwFallback, 0x20e14);
COD2_ASSERT_OFFSET(client_t, deltaMessage, 0x20e18);
COD2_ASSERT_OFFSET(client_t, nextReliableTime, 0x20e1c);
COD2_ASSERT_OFFSET(client_t, lastPacketTime, 0x20e20);
COD2_ASSERT_OFFSET(client_t, lastConnectTime, 0x20e24);
COD2_ASSERT_OFFSET(client_t, nextSnapshotTime, 0x20e28);
COD2_ASSERT_OFFSET(client_t, rateDelayed, 0x20e2c);
COD2_ASSERT_OFFSET(client_t, timeoutCount, 0x20e30);
COD2_ASSERT_OFFSET(client_t, frames, 0x20e34);
COD2_ASSERT_OFFSET(client_t, ping, 0x6e6b4);
COD2_ASSERT_OFFSET(client_t, rate, 0x6e6b8);
COD2_ASSERT_OFFSET(client_t, snapshotMsec, 0x6e6bc);
COD2_ASSERT_OFFSET(client_t, pureAuthentic, 0x6e6c0);
COD2_ASSERT_OFFSET(client_t, netchan, 0x6e6c4);
COD2_ASSERT_OFFSET(client_t, guid, 0xae704);
COD2_ASSERT_OFFSET(client_t, scriptId, 0xae708);
COD2_ASSERT_OFFSET(client_t, unk_0xae70a, 0xae70a);
COD2_ASSERT_OFFSET(client_t, bIsTestClient, 0xae70c);
COD2_ASSERT_OFFSET(client_t, serverId, 0xae710);
COD2_ASSERT_OFFSET(client_t, voicePackets, 0xae714);
COD2_ASSERT_OFFSET(client_t, voicePacketCount, 0xb0fdc);
COD2_ASSERT_OFFSET(client_t, muteList, 0xb0fe0);
COD2_ASSERT_OFFSET(client_t, sendVoice, 0xb1020);
COD2_ASSERT_OFFSET(client_t, authKeyA, 0xb1021);
COD2_ASSERT_OFFSET(client_t, authKeyB, 0xb1042);
COD2_ASSERT_OFFSET(client_t, unk_0xb1063, 0xb1063);

/* challenge_s: 1024 entries at svs+0x5c (0x084230dc); loop bound 0x400. */
struct challenge_s {
    netadr_t adr;                                    /* 0x0000 SV_RequestIpAuthorize/NET_CompareAdr */
    int challenge;                                   /* 0x0014 rand()<<16 ^ rand() ^ svs.time */
    int time;                                        /* 0x0018 oldest is evicted (chunk 20 firstTime) */
    int pingTime;                                    /* 0x001c svs.time at authorize reply (chunk 20 lastTime) */
    int firstTime;                                   /* 0x0020 authorize-timeout test (chunk 20 createdTime) */
    int firstPing;                                   /* 0x0024 chunk 20 pingCache */
    int connected;                                   /* 0x0028 set while SV_DirectConnect runs (chunk 20 connecting) */
    int guid;                                        /* 0x002c from the ipAuthorize accept */
    char clientPBguid[33];                           /* 0x0030 chunk 20 keyA */
    char clientPBguid2[33];                          /* 0x0051 chunk 20 keyB */
    byte unk_0x72[0x2];                              /* 0x0072 unrecovered tail */
};
COD2_ASSERT_SIZE(challenge_t, 0x74);
COD2_ASSERT_OFFSET(challenge_t, adr, 0x0);
COD2_ASSERT_OFFSET(challenge_t, challenge, 0x14);
COD2_ASSERT_OFFSET(challenge_t, time, 0x18);
COD2_ASSERT_OFFSET(challenge_t, pingTime, 0x1c);
COD2_ASSERT_OFFSET(challenge_t, firstTime, 0x20);
COD2_ASSERT_OFFSET(challenge_t, firstPing, 0x24);
COD2_ASSERT_OFFSET(challenge_t, connected, 0x28);
COD2_ASSERT_OFFSET(challenge_t, guid, 0x2c);
COD2_ASSERT_OFFSET(challenge_t, clientPBguid, 0x30);
COD2_ASSERT_OFFSET(challenge_t, clientPBguid2, 0x51);
COD2_ASSERT_OFFSET(challenge_t, unk_0x72, 0x72);

struct tempban_s {
    int guid;                                        /* 0x0000 SV_IsTempBannedGuid */
    int banTime;                                     /* 0x0004 svs.time */
};
COD2_ASSERT_SIZE(tempban_t, 0x8);
COD2_ASSERT_OFFSET(tempban_t, guid, 0x0);
COD2_ASSERT_OFFSET(tempban_t, banTime, 0x4);

/* archivedSnapshot_s: svs.archivedSnapshotFrames: 0x4b0 x 8 (chunk 21 alloc). */
struct archivedSnapshot_s {
    int start;                                       /* 0x0000 byte offset into the 0x2000000 archive buffer (chunk 22) */
    int size;                                        /* 0x0004 chunk 22 */
};
COD2_ASSERT_SIZE(archivedSnapshot_t, 0x8);
COD2_ASSERT_OFFSET(archivedSnapshot_t, start, 0x0);
COD2_ASSERT_OFFSET(archivedSnapshot_t, size, 0x4);

/* cachedSnapshot_s: 0x200 x 0x1c ring (chunk 21 alloc). */
struct cachedSnapshot_s {
    int archivedFrame;                               /* 0x0000 chunk 22 */
    int time;                                        /* 0x0004 chunk 22 serverTime */
    int num_entities;                                /* 0x0008 chunks 22/23 */
    int first_entity;                                /* 0x000c chunks 22/23 */
    int num_clients;                                 /* 0x0010 chunks 22/23 */
    int first_client;                                /* 0x0014 chunks 22/23 */
    int usesDelta;                                   /* 0x0018 chunk 23 flag18 */
};
COD2_ASSERT_SIZE(cachedSnapshot_t, 0x1c);
COD2_ASSERT_OFFSET(cachedSnapshot_t, archivedFrame, 0x0);
COD2_ASSERT_OFFSET(cachedSnapshot_t, time, 0x4);
COD2_ASSERT_OFFSET(cachedSnapshot_t, num_entities, 0x8);
COD2_ASSERT_OFFSET(cachedSnapshot_t, first_entity, 0xc);
COD2_ASSERT_OFFSET(cachedSnapshot_t, num_clients, 0x10);
COD2_ASSERT_OFFSET(cachedSnapshot_t, first_client, 0x14);
COD2_ASSERT_OFFSET(cachedSnapshot_t, usesDelta, 0x18);

/* cachedClient_s: 0x1000 x 0x2708 ring (chunk 21 alloc; imul 0x2708 at 0x08099480). */
struct cachedClient_s {
    int playerStateExists;                           /* 0x0000 chunks 22/23 */
    clientState_t cs;                                /* 0x0004 chunks 22/23 */
    playerState_t ps;                                /* 0x0060 chunks 22/23 */
};
COD2_ASSERT_SIZE(cachedClient_t, 0x2708);
COD2_ASSERT_OFFSET(cachedClient_t, playerStateExists, 0x0);
COD2_ASSERT_OFFSET(cachedClient_t, cs, 0x4);
COD2_ASSERT_OFFSET(cachedClient_t, ps, 0x60);

/* svEntity_s: sv.svEntities[1024] at 0x08442618 (stride 0x174). Chunk 21 archivedEntities = &svEntities[i].baseline; chunks 05/06 index 1-based from 0x084424a4 = &svEntities[-1]. */
struct svEntity_s {
    uint16 worldSector;                              /* 0x0000 area node index, 0 = unlinked (chunks 05/06 Area_*) */
    uint16 nextEntityInWorldSector;                  /* 0x0002 1-based next index (chunks 05/06) */
    archivedEntity_t baseline;                       /* 0x0004 delta base: imul 0x174; add 0x844261c (0x0809ba3c) */
    int numClusters;                                 /* 0x0118 chunks 20,22,23 */
    int clusternums[16];                             /* 0x011c chunks 20,22,23 */
    int lastCluster;                                 /* 0x015c chunks 20,22,23 */
    int linkcontents;                                /* 0x0160 chunk 23 contents160; chunk 05 raw +0x160 */
    float linkmin[2];                                /* 0x0164 Vec2_Copy in Area_LinkEntity (chunk 05) */
    float linkmax[2];                                /* 0x016c Vec2_Copy in Area_LinkEntity (chunk 05) */
};
COD2_ASSERT_SIZE(svEntity_t, 0x174);
COD2_ASSERT_OFFSET(svEntity_t, worldSector, 0x0);
COD2_ASSERT_OFFSET(svEntity_t, nextEntityInWorldSector, 0x2);
COD2_ASSERT_OFFSET(svEntity_t, baseline, 0x4);
COD2_ASSERT_OFFSET(svEntity_t, numClusters, 0x118);
COD2_ASSERT_OFFSET(svEntity_t, clusternums, 0x11c);
COD2_ASSERT_OFFSET(svEntity_t, lastCluster, 0x15c);
COD2_ASSERT_OFFSET(svEntity_t, linkcontents, 0x160);
COD2_ASSERT_OFFSET(svEntity_t, linkmin, 0x164);
COD2_ASSERT_OFFSET(svEntity_t, linkmax, 0x16c);

/* serverStatic_s: svs at 0x08423080; memset(&svs,0,0x1d10c) in SV_Shutdown (immediate at 0x08094374). */
struct serverStatic_s {
    int initialized;                                 /* 0x0000 SV_Startup (chunk 21) */
    int time;                                        /* 0x0004 0x08423084 (mov eax,ds:0x8423084 in SV_UserMove) */
    int snapFlagServerBit;                           /* 0x0008 0x08423088; xor 4 on spawn/fast restart */
    client_t * clients;                              /* 0x000c Com_Calloc(maxclients*0xb1064) (chunk 21) */
    int numSnapshotEntities;                         /* 0x0010 0x08423090 */
    int numSnapshotClients;                          /* 0x0014 0x08423094 */
    int nextSnapshotEntities;                        /* 0x0018 0x08423098 (error string svs.nextSnapshotEntities) */
    int nextSnapshotClients;                         /* 0x001c 0x0842309c */
    entityState_t * snapshotEntities;                /* 0x0020 stride 0xf0 */
    clientState_t * snapshotClients;                 /* 0x0024 stride 0x5c */
    int archiveEnabled;                              /* 0x0028 0x084230a8 */
    int nextArchivedSnapshotFrames;                  /* 0x002c 0x084230ac */
    archivedSnapshot_t * archivedSnapshotFrames;     /* 0x0030 0x4b0 x 8 */
    byte * archivedSnapshotBuffer;                   /* 0x0034 0x2000000 bytes */
    int nextArchivedSnapshotBuffer;                  /* 0x0038 0x084230b8 */
    int nextCachedSnapshotEntities;                  /* 0x003c 0x084230bc (inc at 0x0809ba59) */
    int nextCachedSnapshotClients;                   /* 0x0040 0x084230c0 */
    int nextCachedSnapshotFrames;                    /* 0x0044 0x084230c4 (inc at 0x0809ba92) */
    archivedEntity_t * cachedSnapshotEntities;       /* 0x0048 0x084230c8: imul 0x114; add eax,ds:0x84230c8 (0x0809b95a) */
    cachedClient_t * cachedSnapshotClients;          /* 0x004c 0x1000 x 0x2708 */
    cachedSnapshot_t * cachedSnapshotFrames;         /* 0x0050 0x200 x 0x1c */
    int nextHeartbeatTime;                           /* 0x0054 0x084230d4 (g_svs_nextHeartbeatTime) */
    int nextStatusResponseTime;                      /* 0x0058 0x084230d8 (+600000) */
    challenge_t challenges[1024];                    /* 0x005c 0x084230dc */
    netadr_t redirectAddress;                        /* 0x1d05c 0x084400dc rcon redirect (chunk 22) */
    netadr_t authorizeAddress;                       /* 0x1d070 0x084400f0 (chunk 20) */
    int sv_lastTimeMasterServerCommunicated;         /* 0x1d084 0x08440104 (chunk 20) */
    netProfile_t * OOBProf;                          /* 0x1d088 0x08440108 (chunk 22) */
    tempban_t tempBans[16];                          /* 0x1d08c 0x0844010c (chunk 20) */
};
COD2_ASSERT_SIZE(serverStatic_t, 0x1d10c);
COD2_ASSERT_OFFSET(serverStatic_t, initialized, 0x0);
COD2_ASSERT_OFFSET(serverStatic_t, time, 0x4);
COD2_ASSERT_OFFSET(serverStatic_t, snapFlagServerBit, 0x8);
COD2_ASSERT_OFFSET(serverStatic_t, clients, 0xc);
COD2_ASSERT_OFFSET(serverStatic_t, numSnapshotEntities, 0x10);
COD2_ASSERT_OFFSET(serverStatic_t, numSnapshotClients, 0x14);
COD2_ASSERT_OFFSET(serverStatic_t, nextSnapshotEntities, 0x18);
COD2_ASSERT_OFFSET(serverStatic_t, nextSnapshotClients, 0x1c);
COD2_ASSERT_OFFSET(serverStatic_t, snapshotEntities, 0x20);
COD2_ASSERT_OFFSET(serverStatic_t, snapshotClients, 0x24);
COD2_ASSERT_OFFSET(serverStatic_t, archiveEnabled, 0x28);
COD2_ASSERT_OFFSET(serverStatic_t, nextArchivedSnapshotFrames, 0x2c);
COD2_ASSERT_OFFSET(serverStatic_t, archivedSnapshotFrames, 0x30);
COD2_ASSERT_OFFSET(serverStatic_t, archivedSnapshotBuffer, 0x34);
COD2_ASSERT_OFFSET(serverStatic_t, nextArchivedSnapshotBuffer, 0x38);
COD2_ASSERT_OFFSET(serverStatic_t, nextCachedSnapshotEntities, 0x3c);
COD2_ASSERT_OFFSET(serverStatic_t, nextCachedSnapshotClients, 0x40);
COD2_ASSERT_OFFSET(serverStatic_t, nextCachedSnapshotFrames, 0x44);
COD2_ASSERT_OFFSET(serverStatic_t, cachedSnapshotEntities, 0x48);
COD2_ASSERT_OFFSET(serverStatic_t, cachedSnapshotClients, 0x4c);
COD2_ASSERT_OFFSET(serverStatic_t, cachedSnapshotFrames, 0x50);
COD2_ASSERT_OFFSET(serverStatic_t, nextHeartbeatTime, 0x54);
COD2_ASSERT_OFFSET(serverStatic_t, nextStatusResponseTime, 0x58);
COD2_ASSERT_OFFSET(serverStatic_t, challenges, 0x5c);
COD2_ASSERT_OFFSET(serverStatic_t, redirectAddress, 0x1d05c);
COD2_ASSERT_OFFSET(serverStatic_t, authorizeAddress, 0x1d070);
COD2_ASSERT_OFFSET(serverStatic_t, sv_lastTimeMasterServerCommunicated, 0x1d084);
COD2_ASSERT_OFFSET(serverStatic_t, OOBProf, 0x1d088);
COD2_ASSERT_OFFSET(serverStatic_t, tempBans, 0x1d08c);

/* server_s: sv at 0x08440200; clear size 0x5f534 (immediate at 0x080932f4). 0x014..0x417 and 0x5f438..0x5f4f3 unrecovered; the tail fields were separate globals in symbols.json. */
struct server_s {
    int state;                                       /* 0x0000 0 dead,1 loading,2 game (0x08440200) */
    int restarting;                                  /* 0x0004 0x08440204 */
    int start_frameTime;                             /* 0x0008 0x08440208 (com_frameTime at spawn) */
    int checksumFeed;                                /* 0x000c 0x0844020c */
    int timeResidual;                                /* 0x0010 0x08440210 (chunk 22 called it svs.timeResidual) */
    byte unk_0x14[0x404];                            /* 0x0014 unrecovered */
    char * configstrings[0x800];                     /* 0x0418 0x08440618 */
    svEntity_t svEntities[1024];                     /* 0x2418 0x08442618 */
    char * entityParsePoint;                         /* 0x5f418 g_0849f618: token cursor passed as char ** to the parser (fn_080b6db2) [name inferred] */
    gentity_t * gentities;                           /* 0x5f41c g_0849f61c: game entity base (SV_InitGameEntityLayout, chunk 20) */
    int gentitySize;                                 /* 0x5f420 g_0849f620: game entity stride */
    int num_entities;                                /* 0x5f424 g_0849f624: game entity count */
    playerState_t * gameClients;                     /* 0x5f428 g_0849f628: game client (playerState) base */
    int gameClientSize;                              /* 0x5f42c g_0849f62c: game client stride */
    int skelTimeStamp;                               /* 0x5f430 g_0849f630: skeleton memory generation */
    int skelMemPos;                                  /* 0x5f434 g_0849f634: skeleton pool offset */
    byte unk_0x5f438[0xbc];                          /* 0x5f438 unrecovered */
    char gametype[64];                               /* 0x5f4f4 sv_gametypeName: copied from g_gametype in SV_SpawnServer; ends exactly at the 0x5f534 clear size */
};
COD2_ASSERT_SIZE(server_t, 0x5f534);
COD2_ASSERT_OFFSET(server_t, state, 0x0);
COD2_ASSERT_OFFSET(server_t, restarting, 0x4);
COD2_ASSERT_OFFSET(server_t, start_frameTime, 0x8);
COD2_ASSERT_OFFSET(server_t, checksumFeed, 0xc);
COD2_ASSERT_OFFSET(server_t, timeResidual, 0x10);
COD2_ASSERT_OFFSET(server_t, unk_0x14, 0x14);
COD2_ASSERT_OFFSET(server_t, configstrings, 0x418);
COD2_ASSERT_OFFSET(server_t, svEntities, 0x2418);
COD2_ASSERT_OFFSET(server_t, entityParsePoint, 0x5f418);
COD2_ASSERT_OFFSET(server_t, gentities, 0x5f41c);
COD2_ASSERT_OFFSET(server_t, gentitySize, 0x5f420);
COD2_ASSERT_OFFSET(server_t, num_entities, 0x5f424);
COD2_ASSERT_OFFSET(server_t, gameClients, 0x5f428);
COD2_ASSERT_OFFSET(server_t, gameClientSize, 0x5f42c);
COD2_ASSERT_OFFSET(server_t, skelTimeStamp, 0x5f430);
COD2_ASSERT_OFFSET(server_t, skelMemPos, 0x5f434);
COD2_ASSERT_OFFSET(server_t, unk_0x5f438, 0x5f438);
COD2_ASSERT_OFFSET(server_t, gametype, 0x5f4f4);

struct DvarLimitsEnum {
    int stringCount;                                 /* 0x0000 Dvar_CurrentEnumString tests the enum count (+0x14 of dvar) */
    const char ** strings;                           /* 0x0004 chunk 028 enumNames@0x18 */
};
COD2_ASSERT_SIZE(DvarLimitsEnum, 0x8);
COD2_ASSERT_OFFSET(DvarLimitsEnum, stringCount, 0x0);
COD2_ASSERT_OFFSET(DvarLimitsEnum, strings, 0x4);

struct DvarLimitsInt {
    int min;                                         /* 0x0000 Dvar_PrintDomain */
    int max;                                         /* 0x0004 */
};
COD2_ASSERT_SIZE(DvarLimitsInt, 0x8);
COD2_ASSERT_OFFSET(DvarLimitsInt, min, 0x0);
COD2_ASSERT_OFFSET(DvarLimitsInt, max, 0x4);

struct DvarLimitsFloat {
    float min;                                       /* 0x0000 Dvar_PrintDomain */
    float max;                                       /* 0x0004 */
};
COD2_ASSERT_SIZE(DvarLimitsFloat, 0x8);
COD2_ASSERT_OFFSET(DvarLimitsFloat, min, 0x0);
COD2_ASSERT_OFFSET(DvarLimitsFloat, max, 0x4);

union DvarValue {
    byte enabled;                                    /* 0x0000 bool dvars (type 0) */
    int integer;                                     /* 0x0000 int/enum dvars (types 5/6) */
    unsigned int unsignedInt;                        /* 0x0000 raw bits */
    float value;                                     /* 0x0000 float dvars (type 1) */
    float * vector;                                  /* 0x0000 vec2/3/4 dvars: pointer into Dvar_AllocValueBlock storage */
    const char * string;                             /* 0x0000 string dvars (type 7) */
    byte color[4];                                   /* 0x0000 color dvars (type 8) */
};
COD2_ASSERT_SIZE(DvarValue, 0x4);
COD2_ASSERT_OFFSET(DvarValue, enabled, 0x0);
COD2_ASSERT_OFFSET(DvarValue, integer, 0x0);
COD2_ASSERT_OFFSET(DvarValue, unsignedInt, 0x0);
COD2_ASSERT_OFFSET(DvarValue, value, 0x0);
COD2_ASSERT_OFFSET(DvarValue, vector, 0x0);
COD2_ASSERT_OFFSET(DvarValue, string, 0x0);
COD2_ASSERT_OFFSET(DvarValue, color, 0x0);

union DvarLimits {
    DvarLimitsEnum enumeration;                      /* 0x0000 {stringCount, strings} */
    DvarLimitsInt integer;                           /* 0x0000 {min, max} */
    DvarLimitsFloat decimal;                         /* 0x0000 {min, max} */
};
COD2_ASSERT_SIZE(DvarLimits, 0x8);
COD2_ASSERT_OFFSET(DvarLimits, enumeration, 0x0);
COD2_ASSERT_OFFSET(DvarLimits, integer, 0x0);
COD2_ASSERT_OFFSET(DvarLimits, decimal, 0x0);

/* dvar_s: Pool g_085abe20[0x500]: 0x085abe20 + 0x500*0x24 = 0x085b7220 (hash table). Views of 0xc/0x1c/0x20 bytes were truncated; the tags cvar_s/cod2_dvar_s/dvar043_s/dvar103_s/dvar107_s/cvar_ref_s denote this type. */
struct dvar_s {
    const char * name;                               /* 0x0000 Dvar_Create stores the name (0x080b3c48) */
    uint16 flags;                                    /* 0x0004 Dvar_SetVariant tests 0x10/0x20/0x40/0x80 (0x080b2afa) */
    byte type;                                       /* 0x0006 0 bool,1 float,2 vec2,3 vec3,4 vec4,5 int,6 enum,7 string,8 color */
    byte modified;                                   /* 0x0007 Dvar_SetVariant sets 1 */
    DvarValue current;                               /* 0x0008 Dvar_GetInt/Float/Bool read +8 */
    DvarValue latched;                               /* 0x000c Dvar_SetLatched (0x080b29f0) */
    DvarValue reset;                                 /* 0x0010 Dvar_ResetToDefault (0x080b5092) */
    DvarLimits domain;                               /* 0x0014 Dvar_PrintDomain (0x08063054) */
    dvar_s * next;                                   /* 0x001c name-sorted list (Dvar_Create) */
    dvar_s * hashNext;                               /* 0x0020 Dvar_FindHashed chain (0x080b2e54) */
};
COD2_ASSERT_SIZE(dvar_t, 0x24);
COD2_ASSERT_OFFSET(dvar_t, name, 0x0);
COD2_ASSERT_OFFSET(dvar_t, flags, 0x4);
COD2_ASSERT_OFFSET(dvar_t, type, 0x6);
COD2_ASSERT_OFFSET(dvar_t, modified, 0x7);
COD2_ASSERT_OFFSET(dvar_t, current, 0x8);
COD2_ASSERT_OFFSET(dvar_t, latched, 0xc);
COD2_ASSERT_OFFSET(dvar_t, reset, 0x10);
COD2_ASSERT_OFFSET(dvar_t, domain, 0x14);
COD2_ASSERT_OFFSET(dvar_t, next, 0x1c);
COD2_ASSERT_OFFSET(dvar_t, hashNext, 0x20);

/* cmd_function_s: Com_MallocZeroed(0x14). */
struct cmd_function_s {
    cmd_function_s * next;                           /* 0x0000 Cmd_AddCommand pushes at head (0x080606b6) */
    const char * name;                               /* 0x0004 CopyString(name) */
    const char * autoCompleteDir;                    /* 0x0008 Cmd_SetCommandAuxPair (0x0806079c); chunk 100 path */
    const char * autoCompleteExt;                    /* 0x000c chunk 100 extension */
    xcommand_t function;                             /* 0x0010 Cmd_ExecuteString calls cmd->function (0x080608a6) */
};
COD2_ASSERT_SIZE(cmd_function_t, 0x14);
COD2_ASSERT_OFFSET(cmd_function_t, next, 0x0);
COD2_ASSERT_OFFSET(cmd_function_t, name, 0x4);
COD2_ASSERT_OFFSET(cmd_function_t, autoCompleteDir, 0x8);
COD2_ASSERT_OFFSET(cmd_function_t, autoCompleteExt, 0xc);
COD2_ASSERT_OFFSET(cmd_function_t, function, 0x10);

/* fileHandleData_s: fsh[] at 0x0849fd80, stride 0x11c. Chunk 025 cod2_fsh_s was based 8 bytes late (fsh_table 0x0849fd88). */
struct fileHandleData_s {
    void * handleFiles;                              /* 0x0000 FILE* or unz_s*; cmp [slot+0x849fd80] in FS_HandleForFile (0x0809e7d4) */
    int zipFile;                                     /* 0x0004 FS_FOpenFileReadInternal stores at 0x0849fd84 (0x0809f958) */
    int handleSync;                                  /* 0x0008 FSH_FOpenFile stores at 0x0849fd88 */
    int fileSize;                                    /* 0x000c FS_Shutdown tests 0x0849fd8c (0x080a2ba8) */
    int zipFilePos;                                  /* 0x0010 FS_Seek (chunks 24/101) */
    struct iwd_s * iwd;                              /* 0x0014 non-NULL for pack entries (chunk 24 pack) */
    int streamed;                                    /* 0x0018 FS_FCloseFile 0x0809ee68 / FS_ReadThroughHandle test 0x0849fd98 */
    char name[256];                                  /* 0x001c Q_strncpyz(name, qpath, 0x100); base+0x1c = 0x0849fd9c */
};
COD2_ASSERT_SIZE(fileHandleData_t, 0x11c);
COD2_ASSERT_OFFSET(fileHandleData_t, handleFiles, 0x0);
COD2_ASSERT_OFFSET(fileHandleData_t, zipFile, 0x4);
COD2_ASSERT_OFFSET(fileHandleData_t, handleSync, 0x8);
COD2_ASSERT_OFFSET(fileHandleData_t, fileSize, 0xc);
COD2_ASSERT_OFFSET(fileHandleData_t, zipFilePos, 0x10);
COD2_ASSERT_OFFSET(fileHandleData_t, iwd, 0x14);
COD2_ASSERT_OFFSET(fileHandleData_t, streamed, 0x18);
COD2_ASSERT_OFFSET(fileHandleData_t, name, 0x1c);

struct directory_s {
    char path[256];                                  /* 0x0000 chunk 24 fs_directory_s */
    char gamedir[256];                               /* 0x0100 chunk 24 */
};
COD2_ASSERT_SIZE(directory_t, 0x200);
COD2_ASSERT_OFFSET(directory_t, path, 0x0);
COD2_ASSERT_OFFSET(directory_t, gamedir, 0x100);

/* iwd_s: Single allocation: 0x320 header + hashSize*4 hash table. */
struct iwd_s {
    char iwdFilename[256];                           /* 0x0000 FS_LoadPackFile Q_strncpyz */
    char iwdBasename[256];                           /* 0x0100 ".iwd" suffix stripped */
    char iwdGamename[256];                           /* 0x0200 FS_AddIwdFiles strcpy(gamedir) */
    struct unz_s * handle;                           /* 0x0300 unzOpen result */
    int checksum;                                    /* 0x0304 Com_BlockChecksum (chunk 23 fsEntry_s.checksum) */
    int pure_checksum;                               /* 0x0308 Com_BlockChecksumKey(.., fs_checksumFeed) */
    int numfiles;                                    /* 0x030c FS_PrintSearchPathInfo "(%i files)" */
    byte referenced;                                 /* 0x0310 FS_FOpenFileReadInternal sets 1 */
    byte unk_0x311[0x3];                             /* 0x0311 unrecovered */
    int hashSize;                                    /* 0x0314 power of two, <= 0x800 */
    struct fileInIwd_s ** hashTable;                 /* 0x0318 = (char*)iwd + 0x320 */
    struct fileInIwd_s * buildBuffer;                /* 0x031c entry array; FS_FreePakList frees it */
};
COD2_ASSERT_SIZE(iwd_t, 0x320);
COD2_ASSERT_OFFSET(iwd_t, iwdFilename, 0x0);
COD2_ASSERT_OFFSET(iwd_t, iwdBasename, 0x100);
COD2_ASSERT_OFFSET(iwd_t, iwdGamename, 0x200);
COD2_ASSERT_OFFSET(iwd_t, handle, 0x300);
COD2_ASSERT_OFFSET(iwd_t, checksum, 0x304);
COD2_ASSERT_OFFSET(iwd_t, pure_checksum, 0x308);
COD2_ASSERT_OFFSET(iwd_t, numfiles, 0x30c);
COD2_ASSERT_OFFSET(iwd_t, referenced, 0x310);
COD2_ASSERT_OFFSET(iwd_t, unk_0x311, 0x311);
COD2_ASSERT_OFFSET(iwd_t, hashSize, 0x314);
COD2_ASSERT_OFFSET(iwd_t, hashTable, 0x318);
COD2_ASSERT_OFFSET(iwd_t, buildBuffer, 0x31c);

struct searchpath_s {
    searchpath_s * next;                             /* 0x0000 list link */
    iwd_t * iwd;                                     /* 0x0004 non-NULL for iwd nodes */
    directory_t * dir;                               /* 0x0008 non-NULL for directory nodes */
    int bLocalized;                                  /* 0x000c cmp [eax+0xc] in FS_LoadedIwdChecksums (0x08065935); chunk 007 "dir" here was wrong */
    int language;                                    /* 0x0010 compared with Loc_GetLanguage() */
};
COD2_ASSERT_SIZE(searchpath_t, 0x14);
COD2_ASSERT_OFFSET(searchpath_t, next, 0x0);
COD2_ASSERT_OFFSET(searchpath_t, iwd, 0x4);
COD2_ASSERT_OFFSET(searchpath_t, dir, 0x8);
COD2_ASSERT_OFFSET(searchpath_t, bLocalized, 0xc);
COD2_ASSERT_OFFSET(searchpath_t, language, 0x10);

union VariableUnion {
    int intValue;                                    /* 0x0000 VAR_INTEGER */
    float floatValue;                                /* 0x0000 VAR_FLOAT */
    unsigned int stringValue;                        /* 0x0000 VAR_STRING/ISTRING (string id) */
    const float * vectorValue;                       /* 0x0000 VAR_VECTOR */
    const char * codePosValue;                       /* 0x0000 VAR_CODEPOS/PRECODEPOS */
    unsigned int pointerValue;                       /* 0x0000 VAR_POINTER (object id) */
    struct ScrStackBuffer_s * stackValue;            /* 0x0000 VAR_STACK (suspended frame, chunk 16) */
    int entityOffset;                                /* 0x0000 pending entity field */
};
COD2_ASSERT_SIZE(VariableUnion, 0x4);
COD2_ASSERT_OFFSET(VariableUnion, intValue, 0x0);
COD2_ASSERT_OFFSET(VariableUnion, floatValue, 0x0);
COD2_ASSERT_OFFSET(VariableUnion, stringValue, 0x0);
COD2_ASSERT_OFFSET(VariableUnion, vectorValue, 0x0);
COD2_ASSERT_OFFSET(VariableUnion, codePosValue, 0x0);
COD2_ASSERT_OFFSET(VariableUnion, pointerValue, 0x0);
COD2_ASSERT_OFFSET(VariableUnion, stackValue, 0x0);
COD2_ASSERT_OFFSET(VariableUnion, entityOffset, 0x0);

/* VariableValue: scrVmStack[2048] at 0x083d8da0. Chunk 18 yy_stype_t is a different 8-byte pair. */
struct VariableValue {
    VariableUnion u;                                 /* 0x0000 OP_* handlers write value then type (chunks 16/17) */
    int type;                                        /* 0x0004 0 undefined,1 object,2 string,3 istring,4 vector,5 float,6 int,7 codepos,8 precodepos,9 function,10 stack,11 animation; names at 0x0817d960 */
};
COD2_ASSERT_SIZE(VariableValue, 0x8);
COD2_ASSERT_OFFSET(VariableValue, u, 0x0);
COD2_ASSERT_OFFSET(VariableValue, type, 0x4);

/* ScrVarEntry: Pool g_scrVar at 0x08297500: 0x10000 x 0x10 bytes (ids 0..0xfffd valid). */
struct ScrVarEntry {
    uint16 hashId;                                   /* 0x0000 variable id stored in this hash slot (014 var, 015 link_a) */
    uint16 hashPrev;                                 /* 0x0002 slot back link / free-list prev (ScrVar_InitFreeList) */
    VariableUnion u;                                 /* 0x0004 value; for objects {refCount u16 @+4, aux u16 @+6} (read via ScrVarEntry_RefCount) */
    unsigned int info;                               /* 0x0008 bits0-4 type, bits5-6 status (0x20 chained, 0x40 head, 0x60 value), bits8-31 name key */
    uint16 nextSibling;                              /* 0x000c slot ring link (014 link12; 015 pad_0c) */
    uint16 prevSibling;                              /* 0x000e child ring link (014 link14; 015 link_slot) */
};
COD2_ASSERT_SIZE(ScrVarEntry, 0x10);
COD2_ASSERT_OFFSET(ScrVarEntry, hashId, 0x0);
COD2_ASSERT_OFFSET(ScrVarEntry, hashPrev, 0x2);
COD2_ASSERT_OFFSET(ScrVarEntry, u, 0x4);
COD2_ASSERT_OFFSET(ScrVarEntry, info, 0x8);
COD2_ASSERT_OFFSET(ScrVarEntry, nextSibling, 0xc);
COD2_ASSERT_OFFSET(ScrVarEntry, prevSibling, 0xe);

/* ScrVmFrame: function_frame_start[32] at 0x083d8aa0 (013 scr_call_entry_s, 017 cod2_scr_frame_s/cod2_scr_thread_s). */
struct ScrVmFrame {
    const char * pos;                                /* 0x0000 resume code pointer (printed as pc) */
    unsigned int localId;                            /* 0x0004 thread local-variable object (013 flags) */
    int localVarCount;                               /* 0x0008 local ids pushed on scrVmPub.localVars */
    VariableValue * callMarker;                      /* 0x000c chunk 16 */
    VariableValue * startTop;                        /* 0x0010 saved caller startTop (chunk 16) */
    int callMarkerType;                              /* 0x0014 chunk 16 */
};
COD2_ASSERT_SIZE(ScrVmFrame, 0x18);
COD2_ASSERT_OFFSET(ScrVmFrame, pos, 0x0);
COD2_ASSERT_OFFSET(ScrVmFrame, localId, 0x4);
COD2_ASSERT_OFFSET(ScrVmFrame, localVarCount, 0x8);
COD2_ASSERT_OFFSET(ScrVmFrame, callMarker, 0xc);
COD2_ASSERT_OFFSET(ScrVmFrame, startTop, 0x10);
COD2_ASSERT_OFFSET(ScrVmFrame, callMarkerType, 0x14);

/* scrVmPub_t: scrVmPub at 0x083d8a80; 0x20 + 32*0x18 = 0x320. Chunk 101 (0x20) declared the header only. */
struct scrVmPub_t {
    unsigned int * localVars;                        /* 0x0000 local variable id stack top (base 0x083dce9c) */
    VariableValue * maxstack;                        /* 0x0004 &scrVmStack[2047] */
    int function_count;                              /* 0x0008 0x083d8a88 (013/017 scr_call_depth) */
    ScrVmFrame * function_frame;                     /* 0x000c 0x083d8a8c next free frame */
    VariableValue * top;                             /* 0x0010 0x083d8a90 value stack top */
    byte debugCode;                                  /* 0x0014 0x083d8a94 (013 scr_error_plain) */
    byte abort_on_error;                             /* 0x0015 0x083d8a95 (013 scr_error_recoverable) */
    byte terminal_error;                             /* 0x0016 0x083d8a96 (013/017 scr_error_fatal) */
    byte unk_0x17[0x1];                              /* 0x0017 unrecovered */
    unsigned int outparamcount;                      /* 0x0018 0x083d8a98 Scr_ReserveStackSlot */
    unsigned int inparamcount;                       /* 0x001c 0x083d8a9c Scr_GetInt param check */
    ScrVmFrame function_frame_start[32];             /* 0x0020 0x083d8aa0 */
};
COD2_ASSERT_SIZE(scrVmPub_t, 0x320);
COD2_ASSERT_OFFSET(scrVmPub_t, localVars, 0x0);
COD2_ASSERT_OFFSET(scrVmPub_t, maxstack, 0x4);
COD2_ASSERT_OFFSET(scrVmPub_t, function_count, 0x8);
COD2_ASSERT_OFFSET(scrVmPub_t, function_frame, 0xc);
COD2_ASSERT_OFFSET(scrVmPub_t, top, 0x10);
COD2_ASSERT_OFFSET(scrVmPub_t, debugCode, 0x14);
COD2_ASSERT_OFFSET(scrVmPub_t, abort_on_error, 0x15);
COD2_ASSERT_OFFSET(scrVmPub_t, terminal_error, 0x16);
COD2_ASSERT_OFFSET(scrVmPub_t, unk_0x17, 0x17);
COD2_ASSERT_OFFSET(scrVmPub_t, outparamcount, 0x18);
COD2_ASSERT_OFFSET(scrVmPub_t, inparamcount, 0x1c);
COD2_ASSERT_OFFSET(scrVmPub_t, function_frame_start, 0x20);

/* scrVarPub_t: Block 0x08397500..0x0839754f; every member also exists as a separate global in symbols.json (interior aliases). */
struct scrVarPub_t {
    char * fieldBuffer;                              /* 0x0000 g_08397500 radiant field records (chunk 15) */
    int unk_0x4;                                     /* 0x0004 g_08397504 written by Scr_InitSymbolPools [unknown] */
    uint16 canonicalStrCount;                        /* 0x0008 g_08397508 read with movzx word (0x0807c428) [name inferred] */
    byte developer;                                  /* 0x000a scr_debug_enabled 0x0839750a */
    byte developer_script;                           /* 0x000b g_0839750b Scr_Settings */
    byte evaluate;                                   /* 0x000c scr_compile_error_deferred 0x0839750c */
    byte unk_0xd[0x3];                               /* 0x000d unrecovered */
    const char * error_message;                      /* 0x0010 g_08397510 */
    int error_index;                                 /* 0x0014 scrVarPub_error_index */
    unsigned int time;                               /* 0x0018 scrVarPub_time (masked 0xffffff) */
    unsigned int timeArrayId;                        /* 0x001c scrVarPub_timeArrayId */
    unsigned int pauseArrayId;                       /* 0x0020 g_08397520 thread->notify reverse index [name inferred] */
    unsigned int levelId;                            /* 0x0024 scrVarPub_levelId */
    unsigned int gameId;                             /* 0x0028 scrVarPub_gameId */
    unsigned int animId;                             /* 0x002c scrVarPub_animId */
    unsigned int freeEntList;                        /* 0x0030 g_08397530 */
    unsigned int tempVariable;                       /* 0x0034 scrVarPub_tempVariable */
    byte bInited;                                    /* 0x0038 scrVarPub_inited */
    byte unk_0x39[0x3];                              /* 0x0039 unrecovered */
    unsigned int numScriptValues;                    /* 0x003c g_0839753c compile-side counter [unknown] */
    unsigned int entId;                              /* 0x0040 g_08397540 pending field object (014) */
    unsigned int entFieldName;                       /* 0x0044 g_08397544 pending field name (014) */
    const char * programBuffer;                      /* 0x0048 g_08397548 code base */
    const char * endScriptBuffer;                    /* 0x004c g_0839754c Scr_IsInCodeRange */
};
COD2_ASSERT_SIZE(scrVarPub_t, 0x50);
COD2_ASSERT_OFFSET(scrVarPub_t, fieldBuffer, 0x0);
COD2_ASSERT_OFFSET(scrVarPub_t, unk_0x4, 0x4);
COD2_ASSERT_OFFSET(scrVarPub_t, canonicalStrCount, 0x8);
COD2_ASSERT_OFFSET(scrVarPub_t, developer, 0xa);
COD2_ASSERT_OFFSET(scrVarPub_t, developer_script, 0xb);
COD2_ASSERT_OFFSET(scrVarPub_t, evaluate, 0xc);
COD2_ASSERT_OFFSET(scrVarPub_t, unk_0xd, 0xd);
COD2_ASSERT_OFFSET(scrVarPub_t, error_message, 0x10);
COD2_ASSERT_OFFSET(scrVarPub_t, error_index, 0x14);
COD2_ASSERT_OFFSET(scrVarPub_t, time, 0x18);
COD2_ASSERT_OFFSET(scrVarPub_t, timeArrayId, 0x1c);
COD2_ASSERT_OFFSET(scrVarPub_t, pauseArrayId, 0x20);
COD2_ASSERT_OFFSET(scrVarPub_t, levelId, 0x24);
COD2_ASSERT_OFFSET(scrVarPub_t, gameId, 0x28);
COD2_ASSERT_OFFSET(scrVarPub_t, animId, 0x2c);
COD2_ASSERT_OFFSET(scrVarPub_t, freeEntList, 0x30);
COD2_ASSERT_OFFSET(scrVarPub_t, tempVariable, 0x34);
COD2_ASSERT_OFFSET(scrVarPub_t, bInited, 0x38);
COD2_ASSERT_OFFSET(scrVarPub_t, unk_0x39, 0x39);
COD2_ASSERT_OFFSET(scrVarPub_t, numScriptValues, 0x3c);
COD2_ASSERT_OFFSET(scrVarPub_t, entId, 0x40);
COD2_ASSERT_OFFSET(scrVarPub_t, entFieldName, 0x44);
COD2_ASSERT_OFFSET(scrVarPub_t, programBuffer, 0x48);
COD2_ASSERT_OFFSET(scrVarPub_t, endScriptBuffer, 0x4c);

/* scrVmGlob_t: Proposal only (low confidence): 0x00..0x07 and 0x10..0x17 untouched. */
struct scrVmGlob_t {
    unsigned int unk_0x0[2];                         /* 0x0000 g_083dce78 [unknown] */
    VariableValue eval_stack[2];                     /* 0x0008 0x083dce80 static result stack used by Scr_RunSetup */
    const char * dialog_error_message;               /* 0x0018 0x083dce90 */
    int loading;                                     /* 0x001c 0x083dce94 Scr_SetLoading */
    int starttime;                                   /* 0x0020 0x083dce98 = rdtsc>>16 (Scr_SampleTimers) */
    unsigned int localVarsStack[1];                  /* 0x0024 0x083dce9c base of scrVmPub.localVars (the stack grows past the struct) */
};
COD2_ASSERT_SIZE(scrVmGlob_t, 0x28);
COD2_ASSERT_OFFSET(scrVmGlob_t, unk_0x0, 0x0);
COD2_ASSERT_OFFSET(scrVmGlob_t, eval_stack, 0x8);
COD2_ASSERT_OFFSET(scrVmGlob_t, dialog_error_message, 0x18);
COD2_ASSERT_OFFSET(scrVmGlob_t, loading, 0x1c);
COD2_ASSERT_OFFSET(scrVmGlob_t, starttime, 0x20);
COD2_ASSERT_OFFSET(scrVmGlob_t, localVarsStack, 0x24);

/* ScrClassInfo: g_scrClassInfo[4] at 0x0817d920. */
struct ScrClassInfo {
    uint16 id;                                       /* 0x0000 field map array object (014 fieldObject, 015 fieldMapId) */
    uint16 entArrayId;                               /* 0x0002 live entity objects array (016 classnum: wrong) */
    char charId;                                     /* 0x0004 low byte 'e','h','p','v' */
    byte unk_0x5[0x3];                               /* 0x0005 unrecovered */
    const char * name;                               /* 0x0008 "entity","hudelem","pathnode","vehiclenode" */
};
COD2_ASSERT_SIZE(ScrClassInfo, 0xc);
COD2_ASSERT_OFFSET(ScrClassInfo, id, 0x0);
COD2_ASSERT_OFFSET(ScrClassInfo, entArrayId, 0x2);
COD2_ASSERT_OFFSET(ScrClassInfo, charId, 0x4);
COD2_ASSERT_OFFSET(ScrClassInfo, unk_0x5, 0x5);
COD2_ASSERT_OFFSET(ScrClassInfo, name, 0x8);

struct ScrStackRecord_s {
    byte type;                                       /* 0x0000 value type; 7 marks a nested call frame (Scr_RestoreVarList) */
    unsigned int value;                              /* 0x0001 unaligned (cod2_load_u32) */
} __attribute__((packed));
COD2_ASSERT_SIZE(ScrStackRecord, 0x5);
COD2_ASSERT_OFFSET(ScrStackRecord, type, 0x0);
COD2_ASSERT_OFFSET(ScrStackRecord, value, 0x1);

/* ScrStackBuffer_s: Variable length: followed by size x ScrStackRecord. */
struct ScrStackBuffer_s {
    const char * pos;                                /* 0x0000 resume code pointer; pos[-1] = suspending opcode */
    uint16 size;                                     /* 0x0004 number of 5-byte records (Scr_BuildVarList) */
    uint16 bufLen;                                   /* 0x0006 0x0b + 5*size (StrPool_FreeBlock size) */
    uint16 localId;                                  /* 0x0008 innermost thread local id */
    byte time;                                       /* 0x000a scrVarPub.time & 0xff at suspend */
} __attribute__((packed));
COD2_ASSERT_SIZE(ScrStackBuffer, 0xb);
COD2_ASSERT_OFFSET(ScrStackBuffer, pos, 0x0);
COD2_ASSERT_OFFSET(ScrStackBuffer, size, 0x4);
COD2_ASSERT_OFFSET(ScrStackBuffer, bufLen, 0x6);
COD2_ASSERT_OFFSET(ScrStackBuffer, localId, 0x8);
COD2_ASSERT_OFFSET(ScrStackBuffer, time, 0xa);

/* weaponDef_s: Allocation: mov [esp],0x604; call Hunk_Alloc at 0x080f33a9 (weapon file loader); g_08627940 also spans 0x604 bytes up to the next global. Chunk 047 (0x5fc = end of the hitloc array) was a lower bound. Field names/types are taken from chunk 045, which decoded the 366-entry weapon file field table at 0x08186a60 (names are rodata strings: observed). startAmmo is +0x1c0, maxAmmo +0x1d4. */
struct weaponDef_s {
    const char * szInternalName;                     /* 0x0000 weapon file name (WeaponDef_SetString); 043/044 diagnostics */
    const char * displayName;                        /* 0x0004 field table 0x08186a60: type 0 */
    const char * AIOverlayDescription;               /* 0x0008 field table 0x08186a60: type 0 */
    const char * gunModel;                           /* 0x000c field table 0x08186a60: type 0 */
    const char * handModel;                          /* 0x0010 field table 0x08186a60: type 0 */
    int unk_0x14;                                    /* 0x0014 chunk 102 field14 [unknown] */
    const char * idleAnim;                           /* 0x0018 field table 0x08186a60: type 0 */
    const char * emptyIdleAnim;                      /* 0x001c field table 0x08186a60: type 0 */
    const char * fireAnim;                           /* 0x0020 field table 0x08186a60: type 0 */
    const char * holdFireAnim;                       /* 0x0024 field table 0x08186a60: type 0 */
    const char * lastShotAnim;                       /* 0x0028 field table 0x08186a60: type 0 */
    const char * rechamberAnim;                      /* 0x002c field table 0x08186a60: type 0 */
    const char * meleeAnim;                          /* 0x0030 field table 0x08186a60: type 0 */
    const char * reloadAnim;                         /* 0x0034 field table 0x08186a60: type 0 */
    const char * reloadEmptyAnim;                    /* 0x0038 field table 0x08186a60: type 0 */
    const char * reloadStartAnim;                    /* 0x003c field table 0x08186a60: type 0 */
    const char * reloadEndAnim;                      /* 0x0040 field table 0x08186a60: type 0 */
    const char * raiseAnim;                          /* 0x0044 field table 0x08186a60: type 0 */
    const char * dropAnim;                           /* 0x0048 field table 0x08186a60: type 0 */
    const char * altRaiseAnim;                       /* 0x004c field table 0x08186a60: type 0 */
    const char * altDropAnim;                        /* 0x0050 field table 0x08186a60: type 0 */
    const char * quickRaiseAnim;                     /* 0x0054 field table 0x08186a60: type 0 */
    const char * quickDropAnim;                      /* 0x0058 field table 0x08186a60: type 0 */
    const char * adsFireAnim;                        /* 0x005c field table 0x08186a60: type 0 */
    const char * adsLastShotAnim;                    /* 0x0060 field table 0x08186a60: type 0 */
    const char * adsRechamberAnim;                   /* 0x0064 field table 0x08186a60: type 0 */
    const char * adsUpAnim;                          /* 0x0068 field table 0x08186a60: type 0 */
    const char * adsDownAnim;                        /* 0x006c field table 0x08186a60: type 0 */
    const char * modeName;                           /* 0x0070 field table 0x08186a60: type 0 */
    int playerAnimType;                              /* 0x0074 field table 0x08186a60: type 15 */
    int weaponType;                                  /* 0x0078 field table 0x08186a60: type 8 */
    int weaponClass;                                 /* 0x007c field table 0x08186a60: type 9 */
    int weaponSlot;                                  /* 0x0080 field table 0x08186a60: type 11 */
    int offhandClass;                                /* 0x0084 field table 0x08186a60: type 14 */
    int slotStackable;                               /* 0x0088 field table 0x08186a60: type 5 */
    int stance;                                      /* 0x008c field table 0x08186a60: type 12 */
    const char * viewFlashEffect;                    /* 0x0090 field table 0x08186a60: type 0 */
    const char * worldFlashEffect;                   /* 0x0094 field table 0x08186a60: type 0 */
    const char * pickupSound;                        /* 0x0098 field table 0x08186a60: type 0 */
    const char * ammoPickupSound;                    /* 0x009c field table 0x08186a60: type 0 */
    const char * projectileSound;                    /* 0x00a0 field table 0x08186a60: type 0 */
    const char * pullbackSound;                      /* 0x00a4 field table 0x08186a60: type 0 */
    const char * fireSound;                          /* 0x00a8 field table 0x08186a60: type 0 */
    const char * fireSoundPlayer;                    /* 0x00ac field table 0x08186a60: type 0 */
    const char * loopFireSound;                      /* 0x00b0 field table 0x08186a60: type 0 */
    const char * loopFireSoundPlayer;                /* 0x00b4 field table 0x08186a60: type 0 */
    const char * stopFireSound;                      /* 0x00b8 field table 0x08186a60: type 0 */
    const char * stopFireSoundPlayer;                /* 0x00bc field table 0x08186a60: type 0 */
    const char * lastShotSound;                      /* 0x00c0 field table 0x08186a60: type 0 */
    const char * lastShotSoundPlayer;                /* 0x00c4 field table 0x08186a60: type 0 */
    const char * meleeSwipeSound;                    /* 0x00c8 field table 0x08186a60: type 0 */
    const char * rechamberSound;                     /* 0x00cc field table 0x08186a60: type 0 */
    const char * rechamberSoundPlayer;               /* 0x00d0 field table 0x08186a60: type 0 */
    const char * reloadSound;                        /* 0x00d4 field table 0x08186a60: type 0 */
    const char * reloadSoundPlayer;                  /* 0x00d8 field table 0x08186a60: type 0 */
    const char * reloadEmptySound;                   /* 0x00dc field table 0x08186a60: type 0 */
    const char * reloadEmptySoundPlayer;             /* 0x00e0 field table 0x08186a60: type 0 */
    const char * reloadStartSound;                   /* 0x00e4 field table 0x08186a60: type 0 */
    const char * reloadStartSoundPlayer;             /* 0x00e8 field table 0x08186a60: type 0 */
    const char * reloadEndSound;                     /* 0x00ec field table 0x08186a60: type 0 */
    const char * reloadEndSoundPlayer;               /* 0x00f0 field table 0x08186a60: type 0 */
    const char * raiseSound;                         /* 0x00f4 field table 0x08186a60: type 0 */
    const char * altSwitchSound;                     /* 0x00f8 field table 0x08186a60: type 0 */
    const char * putawaySound;                       /* 0x00fc field table 0x08186a60: type 0 */
    const char * noteTrackSoundA;                    /* 0x0100 field table 0x08186a60: type 0 */
    const char * noteTrackSoundB;                    /* 0x0104 field table 0x08186a60: type 0 */
    const char * noteTrackSoundC;                    /* 0x0108 field table 0x08186a60: type 0 */
    const char * noteTrackSoundD;                    /* 0x010c field table 0x08186a60: type 0 */
    const char * shellEjectEffect;                   /* 0x0110 field table 0x08186a60: type 0 */
    const char * lastShotEjectEffect;                /* 0x0114 field table 0x08186a60: type 0 */
    const char * reticleCenter;                      /* 0x0118 field table 0x08186a60: type 0 */
    const char * reticleSide;                        /* 0x011c field table 0x08186a60: type 0 */
    int reticleCenterSize;                           /* 0x0120 field table 0x08186a60: type 4 */
    int reticleSideSize;                             /* 0x0124 field table 0x08186a60: type 4 */
    int reticleMinOfs;                               /* 0x0128 field table 0x08186a60: type 4 */
    float standMoveF;                                /* 0x012c field table 0x08186a60: type 6 */
    float standMoveR;                                /* 0x0130 field table 0x08186a60: type 6 */
    float standMoveU;                                /* 0x0134 field table 0x08186a60: type 6 */
    float standRotP;                                 /* 0x0138 field table 0x08186a60: type 6 */
    float standRotY;                                 /* 0x013c field table 0x08186a60: type 6 */
    float standRotR;                                 /* 0x0140 field table 0x08186a60: type 6 */
    float duckedOfsF;                                /* 0x0144 field table 0x08186a60: type 6 */
    float duckedOfsR;                                /* 0x0148 field table 0x08186a60: type 6 */
    float duckedOfsU;                                /* 0x014c field table 0x08186a60: type 6 */
    float duckedMoveF;                               /* 0x0150 field table 0x08186a60: type 6 */
    float duckedMoveR;                               /* 0x0154 field table 0x08186a60: type 6 */
    float duckedMoveU;                               /* 0x0158 field table 0x08186a60: type 6 */
    float duckedRotP;                                /* 0x015c field table 0x08186a60: type 6 */
    float duckedRotY;                                /* 0x0160 field table 0x08186a60: type 6 */
    float duckedRotR;                                /* 0x0164 field table 0x08186a60: type 6 */
    float proneOfsF;                                 /* 0x0168 field table 0x08186a60: type 6 */
    float proneOfsR;                                 /* 0x016c field table 0x08186a60: type 6 */
    float proneOfsU;                                 /* 0x0170 field table 0x08186a60: type 6 */
    float proneMoveF;                                /* 0x0174 field table 0x08186a60: type 6 */
    float proneMoveR;                                /* 0x0178 field table 0x08186a60: type 6 */
    float proneMoveU;                                /* 0x017c field table 0x08186a60: type 6 */
    float proneRotP;                                 /* 0x0180 field table 0x08186a60: type 6 */
    float proneRotY;                                 /* 0x0184 field table 0x08186a60: type 6 */
    float proneRotR;                                 /* 0x0188 field table 0x08186a60: type 6 */
    float posMoveRate;                               /* 0x018c field table 0x08186a60: type 6 */
    float posProneMoveRate;                          /* 0x0190 field table 0x08186a60: type 6 */
    float standMoveMinSpeed;                         /* 0x0194 field table 0x08186a60: type 6 */
    float duckedMoveMinSpeed;                        /* 0x0198 field table 0x08186a60: type 6 */
    float proneMoveMinSpeed;                         /* 0x019c field table 0x08186a60: type 6 */
    float posRotRate;                                /* 0x01a0 field table 0x08186a60: type 6 */
    float posProneRotRate;                           /* 0x01a4 field table 0x08186a60: type 6 */
    float standRotMinSpeed;                          /* 0x01a8 field table 0x08186a60: type 6 */
    float duckedRotMinSpeed;                         /* 0x01ac field table 0x08186a60: type 6 */
    float proneRotMinSpeed;                          /* 0x01b0 field table 0x08186a60: type 6 */
    const char * worldModel;                         /* 0x01b4 field table 0x08186a60: type 0 */
    const char * hudIcon;                            /* 0x01b8 field table 0x08186a60: type 0 */
    const char * modeIcon;                           /* 0x01bc field table 0x08186a60: type 0 */
    int startAmmo;                                   /* 0x01c0 field table 0x08186a60: type 4 */
    const char * ammoName;                           /* 0x01c4 field table 0x08186a60: type 0 */
    int iAmmoIndex;                                  /* 0x01c8 runtime index filled by BG_FillInAmmoIndex 0x080eb24a (043/102/103) */
    const char * clipName;                           /* 0x01cc field table 0x08186a60: type 0 */
    int iClipIndex;                                  /* 0x01d0 runtime index (043) */
    int maxAmmo;                                     /* 0x01d4 field table 0x08186a60: type 4 */
    int clipSize;                                    /* 0x01d8 field table 0x08186a60: type 4 */
    int shotCount;                                   /* 0x01dc field table 0x08186a60: type 4 */
    const char * sharedAmmoCapName;                  /* 0x01e0 field table 0x08186a60: type 0 */
    int iSharedAmmoCapIndex;                         /* 0x01e4 runtime index (043; 102 called it weaponClass) */
    int sharedAmmoCap;                               /* 0x01e8 field table 0x08186a60: type 4 */
    int damage;                                      /* 0x01ec field table 0x08186a60: type 4 */
    int playerDamage;                                /* 0x01f0 field table 0x08186a60: type 4 */
    int meleeDamage;                                 /* 0x01f4 field table 0x08186a60: type 4 */
    byte unk_0x1f8[0x4];                             /* 0x01f8 unrecovered */
    int fireDelay;                                   /* 0x01fc field table 0x08186a60: type 7 */
    int meleeDelay;                                  /* 0x0200 field table 0x08186a60: type 7 */
    int fireTime;                                    /* 0x0204 field table 0x08186a60: type 7 */
    int rechamberTime;                               /* 0x0208 field table 0x08186a60: type 7 */
    int rechamberBoltTime;                           /* 0x020c field table 0x08186a60: type 7 */
    int holdFireTime;                                /* 0x0210 field table 0x08186a60: type 7 */
    int meleeTime;                                   /* 0x0214 field table 0x08186a60: type 7 */
    int reloadTime;                                  /* 0x0218 field table 0x08186a60: type 7 */
    int reloadEmptyTime;                             /* 0x021c field table 0x08186a60: type 7 */
    int reloadAddTime;                               /* 0x0220 field table 0x08186a60: type 7 */
    int reloadStartTime;                             /* 0x0224 field table 0x08186a60: type 7 */
    int reloadStartAddTime;                          /* 0x0228 field table 0x08186a60: type 7 */
    int reloadEndTime;                               /* 0x022c field table 0x08186a60: type 7 */
    int dropTime;                                    /* 0x0230 field table 0x08186a60: type 7 */
    int raiseTime;                                   /* 0x0234 field table 0x08186a60: type 7 */
    int altDropTime;                                 /* 0x0238 field table 0x08186a60: type 7 */
    int altRaiseTime;                                /* 0x023c field table 0x08186a60: type 7 */
    int quickDropTime;                               /* 0x0240 field table 0x08186a60: type 7 */
    int quickRaiseTime;                              /* 0x0244 field table 0x08186a60: type 7 */
    int fuseTime;                                    /* 0x0248 field table 0x08186a60: type 7 */
    float autoAimRange;                              /* 0x024c field table 0x08186a60: type 6 */
    float slowdownAimRange;                          /* 0x0250 field table 0x08186a60: type 6 */
    float slowdownAimRangeAds;                       /* 0x0254 field table 0x08186a60: type 6 */
    float lockonAimRange;                            /* 0x0258 field table 0x08186a60: type 6 */
    float lockonAimRangeAds;                         /* 0x025c field table 0x08186a60: type 6 */
    float enemyCrosshairRange;                       /* 0x0260 field table 0x08186a60: type 6 */
    float moveSpeedScale;                            /* 0x0264 field table 0x08186a60: type 6 */
    float adsZoomFov;                                /* 0x0268 field table 0x08186a60: type 6 */
    float adsZoomInFrac;                             /* 0x026c field table 0x08186a60: type 6 */
    float adsZoomOutFrac;                            /* 0x0270 field table 0x08186a60: type 6 */
    const char * adsOverlayShader;                   /* 0x0274 field table 0x08186a60: type 0 */
    int adsOverlayReticle;                           /* 0x0278 field table 0x08186a60: type 10 */
    float adsOverlayWidth;                           /* 0x027c field table 0x08186a60: type 6 */
    float adsOverlayHeight;                          /* 0x0280 field table 0x08186a60: type 6 */
    float adsBobFactor;                              /* 0x0284 field table 0x08186a60: type 6 */
    float adsViewBobMult;                            /* 0x0288 field table 0x08186a60: type 6 */
    float hipSpreadStandMin;                         /* 0x028c field table 0x08186a60: type 6 */
    float hipSpreadDuckedMin;                        /* 0x0290 field table 0x08186a60: type 6 */
    float hipSpreadProneMin;                         /* 0x0294 field table 0x08186a60: type 6 */
    float hipSpreadMax;                              /* 0x0298 field table 0x08186a60: type 6 */
    float hipSpreadDuckedMax;                        /* 0x029c field table 0x08186a60: type 6 */
    float hipSpreadProneMax;                         /* 0x02a0 field table 0x08186a60: type 6 */
    float hipSpreadDecayRate;                        /* 0x02a4 field table 0x08186a60: type 6 */
    float hipSpreadFireAdd;                          /* 0x02a8 field table 0x08186a60: type 6 */
    float hipSpreadTurnAdd;                          /* 0x02ac field table 0x08186a60: type 6 */
    float hipSpreadMoveAdd;                          /* 0x02b0 field table 0x08186a60: type 6 */
    float hipSpreadDuckedDecay;                      /* 0x02b4 field table 0x08186a60: type 6 */
    float hipSpreadProneDecay;                       /* 0x02b8 field table 0x08186a60: type 6 */
    float hipReticleSidePos;                         /* 0x02bc field table 0x08186a60: type 6 */
    int adsTransInTime;                              /* 0x02c0 field table 0x08186a60: type 7 */
    int adsTransOutTime;                             /* 0x02c4 field table 0x08186a60: type 7 */
    float adsIdleAmount;                             /* 0x02c8 field table 0x08186a60: type 6 */
    float hipIdleAmount;                             /* 0x02cc field table 0x08186a60: type 6 */
    float adsIdleSpeed;                              /* 0x02d0 field table 0x08186a60: type 6 */
    float hipIdleSpeed;                              /* 0x02d4 field table 0x08186a60: type 6 */
    float idleCrouchFactor;                          /* 0x02d8 field table 0x08186a60: type 6 */
    float idleProneFactor;                           /* 0x02dc field table 0x08186a60: type 6 */
    float gunMaxPitch;                               /* 0x02e0 field table 0x08186a60: type 6 */
    float gunMaxYaw;                                 /* 0x02e4 field table 0x08186a60: type 6 */
    float swayMaxAngle;                              /* 0x02e8 field table 0x08186a60: type 6 */
    float swayLerpSpeed;                             /* 0x02ec field table 0x08186a60: type 6 */
    float swayPitchScale;                            /* 0x02f0 field table 0x08186a60: type 6 */
    float swayYawScale;                              /* 0x02f4 field table 0x08186a60: type 6 */
    float swayHorizScale;                            /* 0x02f8 field table 0x08186a60: type 6 */
    float swayVertScale;                             /* 0x02fc field table 0x08186a60: type 6 */
    float swayShellShockScale;                       /* 0x0300 field table 0x08186a60: type 6 */
    float adsSwayMaxAngle;                           /* 0x0304 field table 0x08186a60: type 6 */
    float adsSwayLerpSpeed;                          /* 0x0308 field table 0x08186a60: type 6 */
    float adsSwayPitchScale;                         /* 0x030c field table 0x08186a60: type 6 */
    float adsSwayYawScale;                           /* 0x0310 field table 0x08186a60: type 6 */
    float adsSwayHorizScale;                         /* 0x0314 field table 0x08186a60: type 6 */
    float adsSwayVertScale;                          /* 0x0318 field table 0x08186a60: type 6 */
    int rifleBullet;                                 /* 0x031c field table 0x08186a60: type 5 */
    int armorPiercing;                               /* 0x0320 field table 0x08186a60: type 5 */
    int semiAuto;                                    /* 0x0324 field table 0x08186a60: type 5 */
    int boltAction;                                  /* 0x0328 field table 0x08186a60: type 5 */
    int aimDownSight;                                /* 0x032c field table 0x08186a60: type 5 */
    int rechamberWhileAds;                           /* 0x0330 field table 0x08186a60: type 5 */
    float adsViewErrorMin;                           /* 0x0334 field table 0x08186a60: type 6 */
    float adsViewErrorMax;                           /* 0x0338 field table 0x08186a60: type 6 */
    int cookOffHold;                                 /* 0x033c field table 0x08186a60: type 5 */
    int clipOnly;                                    /* 0x0340 field table 0x08186a60: type 5 */
    int wideListIcon;                                /* 0x0344 field table 0x08186a60: type 5 */
    int adsFire;                                     /* 0x0348 field table 0x08186a60: type 5 */
    const char * killIcon;                           /* 0x034c field table 0x08186a60: type 0 */
    int wideKillIcon;                                /* 0x0350 field table 0x08186a60: type 5 */
    int flipKillIcon;                                /* 0x0354 field table 0x08186a60: type 5 */
    int noPartialReload;                             /* 0x0358 field table 0x08186a60: type 5 */
    int segmentedReload;                             /* 0x035c field table 0x08186a60: type 5 */
    int reloadAmmoAdd;                               /* 0x0360 field table 0x08186a60: type 4 */
    int reloadStartAdd;                              /* 0x0364 field table 0x08186a60: type 4 */
    const char * altWeapon;                          /* 0x0368 field table 0x08186a60: type 0 */
    int altWeaponIndex;                              /* 0x036c runtime index of altWeapon (043/044) */
    int dropAmmoMin;                                 /* 0x0370 field table 0x08186a60: type 4 */
    int dropAmmoMax;                                 /* 0x0374 field table 0x08186a60: type 4 */
    int explosionRadius;                             /* 0x0378 field table 0x08186a60: type 4 */
    int explosionInnerDamage;                        /* 0x037c field table 0x08186a60: type 4 */
    int explosionOuterDamage;                        /* 0x0380 field table 0x08186a60: type 4 */
    int projectileSpeed;                             /* 0x0384 field table 0x08186a60: type 4 */
    int projectileSpeedUp;                           /* 0x0388 field table 0x08186a60: type 4 */
    const char * projectileModel;                    /* 0x038c field table 0x08186a60: type 0 */
    int projExplosionType;                           /* 0x0390 field table 0x08186a60: type 13 */
    const char * projExplosionEffect;                /* 0x0394 field table 0x08186a60: type 0 */
    const char * projExplosionSound;                 /* 0x0398 field table 0x08186a60: type 0 */
    int projImpactExplode;                           /* 0x039c field table 0x08186a60: type 5 */
    float parallelDefaultBounce;                     /* 0x03a0 field table 0x08186a60: type 6 */
    float parallelBarkBounce;                        /* 0x03a4 field table 0x08186a60: type 6 */
    float parallelBrickBounce;                       /* 0x03a8 field table 0x08186a60: type 6 */
    float parallelCarpetBounce;                      /* 0x03ac field table 0x08186a60: type 6 */
    float parallelClothBounce;                       /* 0x03b0 field table 0x08186a60: type 6 */
    float parallelConcreteBounce;                    /* 0x03b4 field table 0x08186a60: type 6 */
    float parallelDirtBounce;                        /* 0x03b8 field table 0x08186a60: type 6 */
    float parallelFleshBounce;                       /* 0x03bc field table 0x08186a60: type 6 */
    float parallelFoliageBounce;                     /* 0x03c0 field table 0x08186a60: type 6 */
    float parallelGlassBounce;                       /* 0x03c4 field table 0x08186a60: type 6 */
    float parallelGrassBounce;                       /* 0x03c8 field table 0x08186a60: type 6 */
    float parallelGravelBounce;                      /* 0x03cc field table 0x08186a60: type 6 */
    float parallelIceBounce;                         /* 0x03d0 field table 0x08186a60: type 6 */
    float parallelMetalBounce;                       /* 0x03d4 field table 0x08186a60: type 6 */
    float parallelMudBounce;                         /* 0x03d8 field table 0x08186a60: type 6 */
    float parallelPaperBounce;                       /* 0x03dc field table 0x08186a60: type 6 */
    float parallelPlasterBounce;                     /* 0x03e0 field table 0x08186a60: type 6 */
    float parallelRockBounce;                        /* 0x03e4 field table 0x08186a60: type 6 */
    float parallelSandBounce;                        /* 0x03e8 field table 0x08186a60: type 6 */
    float parallelSnowBounce;                        /* 0x03ec field table 0x08186a60: type 6 */
    float parallelWaterBounce;                       /* 0x03f0 field table 0x08186a60: type 6 */
    float parallelWoodBounce;                        /* 0x03f4 field table 0x08186a60: type 6 */
    float parallelAsphaltBounce;                     /* 0x03f8 field table 0x08186a60: type 6 */
    float perpendicularDefaultBounce;                /* 0x03fc field table 0x08186a60: type 6 */
    float perpendicularBarkBounce;                   /* 0x0400 field table 0x08186a60: type 6 */
    float perpendicularBrickBounce;                  /* 0x0404 field table 0x08186a60: type 6 */
    float perpendicularCarpetBounce;                 /* 0x0408 field table 0x08186a60: type 6 */
    float perpendicularClothBounce;                  /* 0x040c field table 0x08186a60: type 6 */
    float perpendicularConcreteBounce;               /* 0x0410 field table 0x08186a60: type 6 */
    float perpendicularDirtBounce;                   /* 0x0414 field table 0x08186a60: type 6 */
    float perpendicularFleshBounce;                  /* 0x0418 field table 0x08186a60: type 6 */
    float perpendicularFoliageBounce;                /* 0x041c field table 0x08186a60: type 6 */
    float perpendicularGlassBounce;                  /* 0x0420 field table 0x08186a60: type 6 */
    float perpendicularGrassBounce;                  /* 0x0424 field table 0x08186a60: type 6 */
    float perpendicularGravelBounce;                 /* 0x0428 field table 0x08186a60: type 6 */
    float perpendicularIceBounce;                    /* 0x042c field table 0x08186a60: type 6 */
    float perpendicularMetalBounce;                  /* 0x0430 field table 0x08186a60: type 6 */
    float perpendicularMudBounce;                    /* 0x0434 field table 0x08186a60: type 6 */
    float perpendicularPaperBounce;                  /* 0x0438 field table 0x08186a60: type 6 */
    float perpendicularPlasterBounce;                /* 0x043c field table 0x08186a60: type 6 */
    float perpendicularRockBounce;                   /* 0x0440 field table 0x08186a60: type 6 */
    float perpendicularSandBounce;                   /* 0x0444 field table 0x08186a60: type 6 */
    float perpendicularSnowBounce;                   /* 0x0448 field table 0x08186a60: type 6 */
    float perpendicularWaterBounce;                  /* 0x044c field table 0x08186a60: type 6 */
    float perpendicularWoodBounce;                   /* 0x0450 field table 0x08186a60: type 6 */
    float perpendicularAsphaltBounce;                /* 0x0454 field table 0x08186a60: type 6 */
    const char * projTrailEffect;                    /* 0x0458 field table 0x08186a60: type 0 */
    int projectileDLight;                            /* 0x045c field table 0x08186a60: type 4 */
    float projectileRed;                             /* 0x0460 field table 0x08186a60: type 6 */
    float projectileGreen;                           /* 0x0464 field table 0x08186a60: type 6 */
    float projectileBlue;                            /* 0x0468 field table 0x08186a60: type 6 */
    float adsAimPitch;                               /* 0x046c field table 0x08186a60: type 6 */
    float adsCrosshairInFrac;                        /* 0x0470 field table 0x08186a60: type 6 */
    float adsCrosshairOutFrac;                       /* 0x0474 field table 0x08186a60: type 6 */
    int adsGunKickReducedKickBullets;                /* 0x0478 field table 0x08186a60: type 4 */
    float adsGunKickReducedKickPercent;              /* 0x047c field table 0x08186a60: type 6 */
    float adsGunKickPitchMin;                        /* 0x0480 field table 0x08186a60: type 6 */
    float adsGunKickPitchMax;                        /* 0x0484 field table 0x08186a60: type 6 */
    float adsGunKickYawMin;                          /* 0x0488 field table 0x08186a60: type 6 */
    float adsGunKickYawMax;                          /* 0x048c field table 0x08186a60: type 6 */
    float adsGunKickAccel;                           /* 0x0490 field table 0x08186a60: type 6 */
    float adsGunKickSpeedMax;                        /* 0x0494 field table 0x08186a60: type 6 */
    float adsGunKickSpeedDecay;                      /* 0x0498 field table 0x08186a60: type 6 */
    float adsGunKickStaticDecay;                     /* 0x049c field table 0x08186a60: type 6 */
    float adsViewKickPitchMin;                       /* 0x04a0 field table 0x08186a60: type 6 */
    float adsViewKickPitchMax;                       /* 0x04a4 field table 0x08186a60: type 6 */
    float adsViewKickYawMin;                         /* 0x04a8 field table 0x08186a60: type 6 */
    float adsViewKickYawMax;                         /* 0x04ac field table 0x08186a60: type 6 */
    float adsViewKickCenterSpeed;                    /* 0x04b0 field table 0x08186a60: type 6 */
    byte unk_0x4b4[0x8];                             /* 0x04b4 unrecovered */
    float adsSpread;                                 /* 0x04bc field table 0x08186a60: type 6 */
    int hipGunKickReducedKickBullets;                /* 0x04c0 field table 0x08186a60: type 4 */
    float hipGunKickReducedKickPercent;              /* 0x04c4 field table 0x08186a60: type 6 */
    float hipGunKickPitchMin;                        /* 0x04c8 field table 0x08186a60: type 6 */
    float hipGunKickPitchMax;                        /* 0x04cc field table 0x08186a60: type 6 */
    float hipGunKickYawMin;                          /* 0x04d0 field table 0x08186a60: type 6 */
    float hipGunKickYawMax;                          /* 0x04d4 field table 0x08186a60: type 6 */
    float hipGunKickAccel;                           /* 0x04d8 field table 0x08186a60: type 6 */
    float hipGunKickSpeedMax;                        /* 0x04dc field table 0x08186a60: type 6 */
    float hipGunKickSpeedDecay;                      /* 0x04e0 field table 0x08186a60: type 6 */
    float hipGunKickStaticDecay;                     /* 0x04e4 field table 0x08186a60: type 6 */
    float hipViewKickPitchMin;                       /* 0x04e8 field table 0x08186a60: type 6 */
    float hipViewKickPitchMax;                       /* 0x04ec field table 0x08186a60: type 6 */
    float hipViewKickYawMin;                         /* 0x04f0 field table 0x08186a60: type 6 */
    float hipViewKickYawMax;                         /* 0x04f4 field table 0x08186a60: type 6 */
    float hipViewKickCenterSpeed;                    /* 0x04f8 field table 0x08186a60: type 6 */
    byte unk_0x4fc[0x8];                             /* 0x04fc unrecovered */
    float fightDist;                                 /* 0x0504 field table 0x08186a60: type 6 */
    float maxDist;                                   /* 0x0508 field table 0x08186a60: type 6 */
    const char * aiVsAiAccuracyGraph;                /* 0x050c field table 0x08186a60: type 0 */
    const char * aiVsPlayerAccuracyGraph;            /* 0x0510 field table 0x08186a60: type 0 */
    struct accuracy_knot_s * accuracy_knots[2];      /* 0x0514 field table 0x08186a60: parsed AI accuracy graphs (0: aivsai, 1: aivsplayer) */
    int accuracy_knot_count[2];                      /* 0x051c field table 0x08186a60: knot counts for the two accuracy graphs */
    int adsReloadTransTime;                          /* 0x0524 field table 0x08186a60: type 7 */
    float leftArc;                                   /* 0x0528 field table 0x08186a60: type 6 */
    float rightArc;                                  /* 0x052c field table 0x08186a60: type 6 */
    float topArc;                                    /* 0x0530 field table 0x08186a60: type 6 */
    float bottomArc;                                 /* 0x0534 field table 0x08186a60: type 6 */
    float accuracy;                                  /* 0x0538 field table 0x08186a60: type 6 */
    float aiSpread;                                  /* 0x053c field table 0x08186a60: type 6 */
    float playerSpread;                              /* 0x0540 field table 0x08186a60: type 6 */
    float minVertTurnSpeed;                          /* 0x0544 field table 0x08186a60: type 6 */
    float minHorTurnSpeed;                           /* 0x0548 field table 0x08186a60: type 6 */
    float maxVertTurnSpeed;                          /* 0x054c field table 0x08186a60: type 6 */
    float maxHorTurnSpeed;                           /* 0x0550 field table 0x08186a60: type 6 */
    float pitchConvergenceTime;                      /* 0x0554 field table 0x08186a60: type 6 */
    float yawConvergenceTime;                        /* 0x0558 field table 0x08186a60: type 6 */
    float suppressionTime;                           /* 0x055c field table 0x08186a60: type 6 */
    float maxRange;                                  /* 0x0560 field table 0x08186a60: type 6 */
    float animHorRotateInc;                          /* 0x0564 field table 0x08186a60: type 6 */
    float playerPositionDist;                        /* 0x0568 field table 0x08186a60: type 6 */
    const char * useHintString;                      /* 0x056c field table 0x08186a60: type 0 */
    const char * dropHintString;                     /* 0x0570 field table 0x08186a60: type 0 */
    int unk_0x574;                                   /* 0x0574 chunk 053 field574 [unknown] */
    int unk_0x578;                                   /* 0x0578 chunk 053 field578 [unknown] */
    float horizViewJitter;                           /* 0x057c field table 0x08186a60: type 6 */
    float vertViewJitter;                            /* 0x0580 field table 0x08186a60: type 6 */
    const char * script;                             /* 0x0584 field table 0x08186a60: type 0 */
    float adsTransInTimeInv;                         /* 0x0588 field table 0x08186a60: reciprocal of adsTransInTime, default 1/300 */
    float adsTransOutTimeInv;                        /* 0x058c field table 0x08186a60: reciprocal of adsTransOutTime, default 1/500 */
    int minDamage;                                   /* 0x0590 field table 0x08186a60: type 4 */
    int minPlayerDamage;                             /* 0x0594 field table 0x08186a60: type 4 */
    float maxDamageRange;                            /* 0x0598 field table 0x08186a60: type 6 */
    float minDamageRange;                            /* 0x059c field table 0x08186a60: type 6 */
    float destabilizationBaseTime;                   /* 0x05a0 field table 0x08186a60: type 6 */
    float destabilizationTimeReductionRatio;         /* 0x05a4 field table 0x08186a60: type 6 */
    float destabilizationAngleMax;                   /* 0x05a8 field table 0x08186a60: type 6 */
    int destabilizeDistance;                         /* 0x05ac field table 0x08186a60: type 4 */
    float locationDamageMultipliers[19];             /* 0x05b0 G_GetHitLocMultiplier: mov eax,[eax+edx*4+0x5b0] (0x08101b31); table names locNone.. (chunk 045) */
    const char * fireRumble;                         /* 0x05fc field table 0x08186a60: type 0 */
    const char * meleeImpactRumble;                  /* 0x0600 field table 0x08186a60: type 0 */
};
COD2_ASSERT_SIZE(weaponDef_t, 0x604);
COD2_ASSERT_OFFSET(weaponDef_t, szInternalName, 0x0);
COD2_ASSERT_OFFSET(weaponDef_t, displayName, 0x4);
COD2_ASSERT_OFFSET(weaponDef_t, AIOverlayDescription, 0x8);
COD2_ASSERT_OFFSET(weaponDef_t, gunModel, 0xc);
COD2_ASSERT_OFFSET(weaponDef_t, handModel, 0x10);
COD2_ASSERT_OFFSET(weaponDef_t, unk_0x14, 0x14);
COD2_ASSERT_OFFSET(weaponDef_t, idleAnim, 0x18);
COD2_ASSERT_OFFSET(weaponDef_t, emptyIdleAnim, 0x1c);
COD2_ASSERT_OFFSET(weaponDef_t, fireAnim, 0x20);
COD2_ASSERT_OFFSET(weaponDef_t, holdFireAnim, 0x24);
COD2_ASSERT_OFFSET(weaponDef_t, lastShotAnim, 0x28);
COD2_ASSERT_OFFSET(weaponDef_t, rechamberAnim, 0x2c);
COD2_ASSERT_OFFSET(weaponDef_t, meleeAnim, 0x30);
COD2_ASSERT_OFFSET(weaponDef_t, reloadAnim, 0x34);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEmptyAnim, 0x38);
COD2_ASSERT_OFFSET(weaponDef_t, reloadStartAnim, 0x3c);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEndAnim, 0x40);
COD2_ASSERT_OFFSET(weaponDef_t, raiseAnim, 0x44);
COD2_ASSERT_OFFSET(weaponDef_t, dropAnim, 0x48);
COD2_ASSERT_OFFSET(weaponDef_t, altRaiseAnim, 0x4c);
COD2_ASSERT_OFFSET(weaponDef_t, altDropAnim, 0x50);
COD2_ASSERT_OFFSET(weaponDef_t, quickRaiseAnim, 0x54);
COD2_ASSERT_OFFSET(weaponDef_t, quickDropAnim, 0x58);
COD2_ASSERT_OFFSET(weaponDef_t, adsFireAnim, 0x5c);
COD2_ASSERT_OFFSET(weaponDef_t, adsLastShotAnim, 0x60);
COD2_ASSERT_OFFSET(weaponDef_t, adsRechamberAnim, 0x64);
COD2_ASSERT_OFFSET(weaponDef_t, adsUpAnim, 0x68);
COD2_ASSERT_OFFSET(weaponDef_t, adsDownAnim, 0x6c);
COD2_ASSERT_OFFSET(weaponDef_t, modeName, 0x70);
COD2_ASSERT_OFFSET(weaponDef_t, playerAnimType, 0x74);
COD2_ASSERT_OFFSET(weaponDef_t, weaponType, 0x78);
COD2_ASSERT_OFFSET(weaponDef_t, weaponClass, 0x7c);
COD2_ASSERT_OFFSET(weaponDef_t, weaponSlot, 0x80);
COD2_ASSERT_OFFSET(weaponDef_t, offhandClass, 0x84);
COD2_ASSERT_OFFSET(weaponDef_t, slotStackable, 0x88);
COD2_ASSERT_OFFSET(weaponDef_t, stance, 0x8c);
COD2_ASSERT_OFFSET(weaponDef_t, viewFlashEffect, 0x90);
COD2_ASSERT_OFFSET(weaponDef_t, worldFlashEffect, 0x94);
COD2_ASSERT_OFFSET(weaponDef_t, pickupSound, 0x98);
COD2_ASSERT_OFFSET(weaponDef_t, ammoPickupSound, 0x9c);
COD2_ASSERT_OFFSET(weaponDef_t, projectileSound, 0xa0);
COD2_ASSERT_OFFSET(weaponDef_t, pullbackSound, 0xa4);
COD2_ASSERT_OFFSET(weaponDef_t, fireSound, 0xa8);
COD2_ASSERT_OFFSET(weaponDef_t, fireSoundPlayer, 0xac);
COD2_ASSERT_OFFSET(weaponDef_t, loopFireSound, 0xb0);
COD2_ASSERT_OFFSET(weaponDef_t, loopFireSoundPlayer, 0xb4);
COD2_ASSERT_OFFSET(weaponDef_t, stopFireSound, 0xb8);
COD2_ASSERT_OFFSET(weaponDef_t, stopFireSoundPlayer, 0xbc);
COD2_ASSERT_OFFSET(weaponDef_t, lastShotSound, 0xc0);
COD2_ASSERT_OFFSET(weaponDef_t, lastShotSoundPlayer, 0xc4);
COD2_ASSERT_OFFSET(weaponDef_t, meleeSwipeSound, 0xc8);
COD2_ASSERT_OFFSET(weaponDef_t, rechamberSound, 0xcc);
COD2_ASSERT_OFFSET(weaponDef_t, rechamberSoundPlayer, 0xd0);
COD2_ASSERT_OFFSET(weaponDef_t, reloadSound, 0xd4);
COD2_ASSERT_OFFSET(weaponDef_t, reloadSoundPlayer, 0xd8);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEmptySound, 0xdc);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEmptySoundPlayer, 0xe0);
COD2_ASSERT_OFFSET(weaponDef_t, reloadStartSound, 0xe4);
COD2_ASSERT_OFFSET(weaponDef_t, reloadStartSoundPlayer, 0xe8);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEndSound, 0xec);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEndSoundPlayer, 0xf0);
COD2_ASSERT_OFFSET(weaponDef_t, raiseSound, 0xf4);
COD2_ASSERT_OFFSET(weaponDef_t, altSwitchSound, 0xf8);
COD2_ASSERT_OFFSET(weaponDef_t, putawaySound, 0xfc);
COD2_ASSERT_OFFSET(weaponDef_t, noteTrackSoundA, 0x100);
COD2_ASSERT_OFFSET(weaponDef_t, noteTrackSoundB, 0x104);
COD2_ASSERT_OFFSET(weaponDef_t, noteTrackSoundC, 0x108);
COD2_ASSERT_OFFSET(weaponDef_t, noteTrackSoundD, 0x10c);
COD2_ASSERT_OFFSET(weaponDef_t, shellEjectEffect, 0x110);
COD2_ASSERT_OFFSET(weaponDef_t, lastShotEjectEffect, 0x114);
COD2_ASSERT_OFFSET(weaponDef_t, reticleCenter, 0x118);
COD2_ASSERT_OFFSET(weaponDef_t, reticleSide, 0x11c);
COD2_ASSERT_OFFSET(weaponDef_t, reticleCenterSize, 0x120);
COD2_ASSERT_OFFSET(weaponDef_t, reticleSideSize, 0x124);
COD2_ASSERT_OFFSET(weaponDef_t, reticleMinOfs, 0x128);
COD2_ASSERT_OFFSET(weaponDef_t, standMoveF, 0x12c);
COD2_ASSERT_OFFSET(weaponDef_t, standMoveR, 0x130);
COD2_ASSERT_OFFSET(weaponDef_t, standMoveU, 0x134);
COD2_ASSERT_OFFSET(weaponDef_t, standRotP, 0x138);
COD2_ASSERT_OFFSET(weaponDef_t, standRotY, 0x13c);
COD2_ASSERT_OFFSET(weaponDef_t, standRotR, 0x140);
COD2_ASSERT_OFFSET(weaponDef_t, duckedOfsF, 0x144);
COD2_ASSERT_OFFSET(weaponDef_t, duckedOfsR, 0x148);
COD2_ASSERT_OFFSET(weaponDef_t, duckedOfsU, 0x14c);
COD2_ASSERT_OFFSET(weaponDef_t, duckedMoveF, 0x150);
COD2_ASSERT_OFFSET(weaponDef_t, duckedMoveR, 0x154);
COD2_ASSERT_OFFSET(weaponDef_t, duckedMoveU, 0x158);
COD2_ASSERT_OFFSET(weaponDef_t, duckedRotP, 0x15c);
COD2_ASSERT_OFFSET(weaponDef_t, duckedRotY, 0x160);
COD2_ASSERT_OFFSET(weaponDef_t, duckedRotR, 0x164);
COD2_ASSERT_OFFSET(weaponDef_t, proneOfsF, 0x168);
COD2_ASSERT_OFFSET(weaponDef_t, proneOfsR, 0x16c);
COD2_ASSERT_OFFSET(weaponDef_t, proneOfsU, 0x170);
COD2_ASSERT_OFFSET(weaponDef_t, proneMoveF, 0x174);
COD2_ASSERT_OFFSET(weaponDef_t, proneMoveR, 0x178);
COD2_ASSERT_OFFSET(weaponDef_t, proneMoveU, 0x17c);
COD2_ASSERT_OFFSET(weaponDef_t, proneRotP, 0x180);
COD2_ASSERT_OFFSET(weaponDef_t, proneRotY, 0x184);
COD2_ASSERT_OFFSET(weaponDef_t, proneRotR, 0x188);
COD2_ASSERT_OFFSET(weaponDef_t, posMoveRate, 0x18c);
COD2_ASSERT_OFFSET(weaponDef_t, posProneMoveRate, 0x190);
COD2_ASSERT_OFFSET(weaponDef_t, standMoveMinSpeed, 0x194);
COD2_ASSERT_OFFSET(weaponDef_t, duckedMoveMinSpeed, 0x198);
COD2_ASSERT_OFFSET(weaponDef_t, proneMoveMinSpeed, 0x19c);
COD2_ASSERT_OFFSET(weaponDef_t, posRotRate, 0x1a0);
COD2_ASSERT_OFFSET(weaponDef_t, posProneRotRate, 0x1a4);
COD2_ASSERT_OFFSET(weaponDef_t, standRotMinSpeed, 0x1a8);
COD2_ASSERT_OFFSET(weaponDef_t, duckedRotMinSpeed, 0x1ac);
COD2_ASSERT_OFFSET(weaponDef_t, proneRotMinSpeed, 0x1b0);
COD2_ASSERT_OFFSET(weaponDef_t, worldModel, 0x1b4);
COD2_ASSERT_OFFSET(weaponDef_t, hudIcon, 0x1b8);
COD2_ASSERT_OFFSET(weaponDef_t, modeIcon, 0x1bc);
COD2_ASSERT_OFFSET(weaponDef_t, startAmmo, 0x1c0);
COD2_ASSERT_OFFSET(weaponDef_t, ammoName, 0x1c4);
COD2_ASSERT_OFFSET(weaponDef_t, iAmmoIndex, 0x1c8);
COD2_ASSERT_OFFSET(weaponDef_t, clipName, 0x1cc);
COD2_ASSERT_OFFSET(weaponDef_t, iClipIndex, 0x1d0);
COD2_ASSERT_OFFSET(weaponDef_t, maxAmmo, 0x1d4);
COD2_ASSERT_OFFSET(weaponDef_t, clipSize, 0x1d8);
COD2_ASSERT_OFFSET(weaponDef_t, shotCount, 0x1dc);
COD2_ASSERT_OFFSET(weaponDef_t, sharedAmmoCapName, 0x1e0);
COD2_ASSERT_OFFSET(weaponDef_t, iSharedAmmoCapIndex, 0x1e4);
COD2_ASSERT_OFFSET(weaponDef_t, sharedAmmoCap, 0x1e8);
COD2_ASSERT_OFFSET(weaponDef_t, damage, 0x1ec);
COD2_ASSERT_OFFSET(weaponDef_t, playerDamage, 0x1f0);
COD2_ASSERT_OFFSET(weaponDef_t, meleeDamage, 0x1f4);
COD2_ASSERT_OFFSET(weaponDef_t, unk_0x1f8, 0x1f8);
COD2_ASSERT_OFFSET(weaponDef_t, fireDelay, 0x1fc);
COD2_ASSERT_OFFSET(weaponDef_t, meleeDelay, 0x200);
COD2_ASSERT_OFFSET(weaponDef_t, fireTime, 0x204);
COD2_ASSERT_OFFSET(weaponDef_t, rechamberTime, 0x208);
COD2_ASSERT_OFFSET(weaponDef_t, rechamberBoltTime, 0x20c);
COD2_ASSERT_OFFSET(weaponDef_t, holdFireTime, 0x210);
COD2_ASSERT_OFFSET(weaponDef_t, meleeTime, 0x214);
COD2_ASSERT_OFFSET(weaponDef_t, reloadTime, 0x218);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEmptyTime, 0x21c);
COD2_ASSERT_OFFSET(weaponDef_t, reloadAddTime, 0x220);
COD2_ASSERT_OFFSET(weaponDef_t, reloadStartTime, 0x224);
COD2_ASSERT_OFFSET(weaponDef_t, reloadStartAddTime, 0x228);
COD2_ASSERT_OFFSET(weaponDef_t, reloadEndTime, 0x22c);
COD2_ASSERT_OFFSET(weaponDef_t, dropTime, 0x230);
COD2_ASSERT_OFFSET(weaponDef_t, raiseTime, 0x234);
COD2_ASSERT_OFFSET(weaponDef_t, altDropTime, 0x238);
COD2_ASSERT_OFFSET(weaponDef_t, altRaiseTime, 0x23c);
COD2_ASSERT_OFFSET(weaponDef_t, quickDropTime, 0x240);
COD2_ASSERT_OFFSET(weaponDef_t, quickRaiseTime, 0x244);
COD2_ASSERT_OFFSET(weaponDef_t, fuseTime, 0x248);
COD2_ASSERT_OFFSET(weaponDef_t, autoAimRange, 0x24c);
COD2_ASSERT_OFFSET(weaponDef_t, slowdownAimRange, 0x250);
COD2_ASSERT_OFFSET(weaponDef_t, slowdownAimRangeAds, 0x254);
COD2_ASSERT_OFFSET(weaponDef_t, lockonAimRange, 0x258);
COD2_ASSERT_OFFSET(weaponDef_t, lockonAimRangeAds, 0x25c);
COD2_ASSERT_OFFSET(weaponDef_t, enemyCrosshairRange, 0x260);
COD2_ASSERT_OFFSET(weaponDef_t, moveSpeedScale, 0x264);
COD2_ASSERT_OFFSET(weaponDef_t, adsZoomFov, 0x268);
COD2_ASSERT_OFFSET(weaponDef_t, adsZoomInFrac, 0x26c);
COD2_ASSERT_OFFSET(weaponDef_t, adsZoomOutFrac, 0x270);
COD2_ASSERT_OFFSET(weaponDef_t, adsOverlayShader, 0x274);
COD2_ASSERT_OFFSET(weaponDef_t, adsOverlayReticle, 0x278);
COD2_ASSERT_OFFSET(weaponDef_t, adsOverlayWidth, 0x27c);
COD2_ASSERT_OFFSET(weaponDef_t, adsOverlayHeight, 0x280);
COD2_ASSERT_OFFSET(weaponDef_t, adsBobFactor, 0x284);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewBobMult, 0x288);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadStandMin, 0x28c);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadDuckedMin, 0x290);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadProneMin, 0x294);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadMax, 0x298);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadDuckedMax, 0x29c);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadProneMax, 0x2a0);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadDecayRate, 0x2a4);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadFireAdd, 0x2a8);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadTurnAdd, 0x2ac);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadMoveAdd, 0x2b0);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadDuckedDecay, 0x2b4);
COD2_ASSERT_OFFSET(weaponDef_t, hipSpreadProneDecay, 0x2b8);
COD2_ASSERT_OFFSET(weaponDef_t, hipReticleSidePos, 0x2bc);
COD2_ASSERT_OFFSET(weaponDef_t, adsTransInTime, 0x2c0);
COD2_ASSERT_OFFSET(weaponDef_t, adsTransOutTime, 0x2c4);
COD2_ASSERT_OFFSET(weaponDef_t, adsIdleAmount, 0x2c8);
COD2_ASSERT_OFFSET(weaponDef_t, hipIdleAmount, 0x2cc);
COD2_ASSERT_OFFSET(weaponDef_t, adsIdleSpeed, 0x2d0);
COD2_ASSERT_OFFSET(weaponDef_t, hipIdleSpeed, 0x2d4);
COD2_ASSERT_OFFSET(weaponDef_t, idleCrouchFactor, 0x2d8);
COD2_ASSERT_OFFSET(weaponDef_t, idleProneFactor, 0x2dc);
COD2_ASSERT_OFFSET(weaponDef_t, gunMaxPitch, 0x2e0);
COD2_ASSERT_OFFSET(weaponDef_t, gunMaxYaw, 0x2e4);
COD2_ASSERT_OFFSET(weaponDef_t, swayMaxAngle, 0x2e8);
COD2_ASSERT_OFFSET(weaponDef_t, swayLerpSpeed, 0x2ec);
COD2_ASSERT_OFFSET(weaponDef_t, swayPitchScale, 0x2f0);
COD2_ASSERT_OFFSET(weaponDef_t, swayYawScale, 0x2f4);
COD2_ASSERT_OFFSET(weaponDef_t, swayHorizScale, 0x2f8);
COD2_ASSERT_OFFSET(weaponDef_t, swayVertScale, 0x2fc);
COD2_ASSERT_OFFSET(weaponDef_t, swayShellShockScale, 0x300);
COD2_ASSERT_OFFSET(weaponDef_t, adsSwayMaxAngle, 0x304);
COD2_ASSERT_OFFSET(weaponDef_t, adsSwayLerpSpeed, 0x308);
COD2_ASSERT_OFFSET(weaponDef_t, adsSwayPitchScale, 0x30c);
COD2_ASSERT_OFFSET(weaponDef_t, adsSwayYawScale, 0x310);
COD2_ASSERT_OFFSET(weaponDef_t, adsSwayHorizScale, 0x314);
COD2_ASSERT_OFFSET(weaponDef_t, adsSwayVertScale, 0x318);
COD2_ASSERT_OFFSET(weaponDef_t, rifleBullet, 0x31c);
COD2_ASSERT_OFFSET(weaponDef_t, armorPiercing, 0x320);
COD2_ASSERT_OFFSET(weaponDef_t, semiAuto, 0x324);
COD2_ASSERT_OFFSET(weaponDef_t, boltAction, 0x328);
COD2_ASSERT_OFFSET(weaponDef_t, aimDownSight, 0x32c);
COD2_ASSERT_OFFSET(weaponDef_t, rechamberWhileAds, 0x330);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewErrorMin, 0x334);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewErrorMax, 0x338);
COD2_ASSERT_OFFSET(weaponDef_t, cookOffHold, 0x33c);
COD2_ASSERT_OFFSET(weaponDef_t, clipOnly, 0x340);
COD2_ASSERT_OFFSET(weaponDef_t, wideListIcon, 0x344);
COD2_ASSERT_OFFSET(weaponDef_t, adsFire, 0x348);
COD2_ASSERT_OFFSET(weaponDef_t, killIcon, 0x34c);
COD2_ASSERT_OFFSET(weaponDef_t, wideKillIcon, 0x350);
COD2_ASSERT_OFFSET(weaponDef_t, flipKillIcon, 0x354);
COD2_ASSERT_OFFSET(weaponDef_t, noPartialReload, 0x358);
COD2_ASSERT_OFFSET(weaponDef_t, segmentedReload, 0x35c);
COD2_ASSERT_OFFSET(weaponDef_t, reloadAmmoAdd, 0x360);
COD2_ASSERT_OFFSET(weaponDef_t, reloadStartAdd, 0x364);
COD2_ASSERT_OFFSET(weaponDef_t, altWeapon, 0x368);
COD2_ASSERT_OFFSET(weaponDef_t, altWeaponIndex, 0x36c);
COD2_ASSERT_OFFSET(weaponDef_t, dropAmmoMin, 0x370);
COD2_ASSERT_OFFSET(weaponDef_t, dropAmmoMax, 0x374);
COD2_ASSERT_OFFSET(weaponDef_t, explosionRadius, 0x378);
COD2_ASSERT_OFFSET(weaponDef_t, explosionInnerDamage, 0x37c);
COD2_ASSERT_OFFSET(weaponDef_t, explosionOuterDamage, 0x380);
COD2_ASSERT_OFFSET(weaponDef_t, projectileSpeed, 0x384);
COD2_ASSERT_OFFSET(weaponDef_t, projectileSpeedUp, 0x388);
COD2_ASSERT_OFFSET(weaponDef_t, projectileModel, 0x38c);
COD2_ASSERT_OFFSET(weaponDef_t, projExplosionType, 0x390);
COD2_ASSERT_OFFSET(weaponDef_t, projExplosionEffect, 0x394);
COD2_ASSERT_OFFSET(weaponDef_t, projExplosionSound, 0x398);
COD2_ASSERT_OFFSET(weaponDef_t, projImpactExplode, 0x39c);
COD2_ASSERT_OFFSET(weaponDef_t, parallelDefaultBounce, 0x3a0);
COD2_ASSERT_OFFSET(weaponDef_t, parallelBarkBounce, 0x3a4);
COD2_ASSERT_OFFSET(weaponDef_t, parallelBrickBounce, 0x3a8);
COD2_ASSERT_OFFSET(weaponDef_t, parallelCarpetBounce, 0x3ac);
COD2_ASSERT_OFFSET(weaponDef_t, parallelClothBounce, 0x3b0);
COD2_ASSERT_OFFSET(weaponDef_t, parallelConcreteBounce, 0x3b4);
COD2_ASSERT_OFFSET(weaponDef_t, parallelDirtBounce, 0x3b8);
COD2_ASSERT_OFFSET(weaponDef_t, parallelFleshBounce, 0x3bc);
COD2_ASSERT_OFFSET(weaponDef_t, parallelFoliageBounce, 0x3c0);
COD2_ASSERT_OFFSET(weaponDef_t, parallelGlassBounce, 0x3c4);
COD2_ASSERT_OFFSET(weaponDef_t, parallelGrassBounce, 0x3c8);
COD2_ASSERT_OFFSET(weaponDef_t, parallelGravelBounce, 0x3cc);
COD2_ASSERT_OFFSET(weaponDef_t, parallelIceBounce, 0x3d0);
COD2_ASSERT_OFFSET(weaponDef_t, parallelMetalBounce, 0x3d4);
COD2_ASSERT_OFFSET(weaponDef_t, parallelMudBounce, 0x3d8);
COD2_ASSERT_OFFSET(weaponDef_t, parallelPaperBounce, 0x3dc);
COD2_ASSERT_OFFSET(weaponDef_t, parallelPlasterBounce, 0x3e0);
COD2_ASSERT_OFFSET(weaponDef_t, parallelRockBounce, 0x3e4);
COD2_ASSERT_OFFSET(weaponDef_t, parallelSandBounce, 0x3e8);
COD2_ASSERT_OFFSET(weaponDef_t, parallelSnowBounce, 0x3ec);
COD2_ASSERT_OFFSET(weaponDef_t, parallelWaterBounce, 0x3f0);
COD2_ASSERT_OFFSET(weaponDef_t, parallelWoodBounce, 0x3f4);
COD2_ASSERT_OFFSET(weaponDef_t, parallelAsphaltBounce, 0x3f8);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularDefaultBounce, 0x3fc);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularBarkBounce, 0x400);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularBrickBounce, 0x404);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularCarpetBounce, 0x408);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularClothBounce, 0x40c);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularConcreteBounce, 0x410);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularDirtBounce, 0x414);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularFleshBounce, 0x418);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularFoliageBounce, 0x41c);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularGlassBounce, 0x420);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularGrassBounce, 0x424);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularGravelBounce, 0x428);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularIceBounce, 0x42c);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularMetalBounce, 0x430);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularMudBounce, 0x434);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularPaperBounce, 0x438);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularPlasterBounce, 0x43c);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularRockBounce, 0x440);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularSandBounce, 0x444);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularSnowBounce, 0x448);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularWaterBounce, 0x44c);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularWoodBounce, 0x450);
COD2_ASSERT_OFFSET(weaponDef_t, perpendicularAsphaltBounce, 0x454);
COD2_ASSERT_OFFSET(weaponDef_t, projTrailEffect, 0x458);
COD2_ASSERT_OFFSET(weaponDef_t, projectileDLight, 0x45c);
COD2_ASSERT_OFFSET(weaponDef_t, projectileRed, 0x460);
COD2_ASSERT_OFFSET(weaponDef_t, projectileGreen, 0x464);
COD2_ASSERT_OFFSET(weaponDef_t, projectileBlue, 0x468);
COD2_ASSERT_OFFSET(weaponDef_t, adsAimPitch, 0x46c);
COD2_ASSERT_OFFSET(weaponDef_t, adsCrosshairInFrac, 0x470);
COD2_ASSERT_OFFSET(weaponDef_t, adsCrosshairOutFrac, 0x474);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickReducedKickBullets, 0x478);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickReducedKickPercent, 0x47c);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickPitchMin, 0x480);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickPitchMax, 0x484);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickYawMin, 0x488);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickYawMax, 0x48c);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickAccel, 0x490);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickSpeedMax, 0x494);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickSpeedDecay, 0x498);
COD2_ASSERT_OFFSET(weaponDef_t, adsGunKickStaticDecay, 0x49c);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewKickPitchMin, 0x4a0);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewKickPitchMax, 0x4a4);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewKickYawMin, 0x4a8);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewKickYawMax, 0x4ac);
COD2_ASSERT_OFFSET(weaponDef_t, adsViewKickCenterSpeed, 0x4b0);
COD2_ASSERT_OFFSET(weaponDef_t, unk_0x4b4, 0x4b4);
COD2_ASSERT_OFFSET(weaponDef_t, adsSpread, 0x4bc);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickReducedKickBullets, 0x4c0);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickReducedKickPercent, 0x4c4);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickPitchMin, 0x4c8);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickPitchMax, 0x4cc);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickYawMin, 0x4d0);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickYawMax, 0x4d4);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickAccel, 0x4d8);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickSpeedMax, 0x4dc);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickSpeedDecay, 0x4e0);
COD2_ASSERT_OFFSET(weaponDef_t, hipGunKickStaticDecay, 0x4e4);
COD2_ASSERT_OFFSET(weaponDef_t, hipViewKickPitchMin, 0x4e8);
COD2_ASSERT_OFFSET(weaponDef_t, hipViewKickPitchMax, 0x4ec);
COD2_ASSERT_OFFSET(weaponDef_t, hipViewKickYawMin, 0x4f0);
COD2_ASSERT_OFFSET(weaponDef_t, hipViewKickYawMax, 0x4f4);
COD2_ASSERT_OFFSET(weaponDef_t, hipViewKickCenterSpeed, 0x4f8);
COD2_ASSERT_OFFSET(weaponDef_t, unk_0x4fc, 0x4fc);
COD2_ASSERT_OFFSET(weaponDef_t, fightDist, 0x504);
COD2_ASSERT_OFFSET(weaponDef_t, maxDist, 0x508);
COD2_ASSERT_OFFSET(weaponDef_t, aiVsAiAccuracyGraph, 0x50c);
COD2_ASSERT_OFFSET(weaponDef_t, aiVsPlayerAccuracyGraph, 0x510);
COD2_ASSERT_OFFSET(weaponDef_t, accuracy_knots, 0x514);
COD2_ASSERT_OFFSET(weaponDef_t, accuracy_knot_count, 0x51c);
COD2_ASSERT_OFFSET(weaponDef_t, adsReloadTransTime, 0x524);
COD2_ASSERT_OFFSET(weaponDef_t, leftArc, 0x528);
COD2_ASSERT_OFFSET(weaponDef_t, rightArc, 0x52c);
COD2_ASSERT_OFFSET(weaponDef_t, topArc, 0x530);
COD2_ASSERT_OFFSET(weaponDef_t, bottomArc, 0x534);
COD2_ASSERT_OFFSET(weaponDef_t, accuracy, 0x538);
COD2_ASSERT_OFFSET(weaponDef_t, aiSpread, 0x53c);
COD2_ASSERT_OFFSET(weaponDef_t, playerSpread, 0x540);
COD2_ASSERT_OFFSET(weaponDef_t, minVertTurnSpeed, 0x544);
COD2_ASSERT_OFFSET(weaponDef_t, minHorTurnSpeed, 0x548);
COD2_ASSERT_OFFSET(weaponDef_t, maxVertTurnSpeed, 0x54c);
COD2_ASSERT_OFFSET(weaponDef_t, maxHorTurnSpeed, 0x550);
COD2_ASSERT_OFFSET(weaponDef_t, pitchConvergenceTime, 0x554);
COD2_ASSERT_OFFSET(weaponDef_t, yawConvergenceTime, 0x558);
COD2_ASSERT_OFFSET(weaponDef_t, suppressionTime, 0x55c);
COD2_ASSERT_OFFSET(weaponDef_t, maxRange, 0x560);
COD2_ASSERT_OFFSET(weaponDef_t, animHorRotateInc, 0x564);
COD2_ASSERT_OFFSET(weaponDef_t, playerPositionDist, 0x568);
COD2_ASSERT_OFFSET(weaponDef_t, useHintString, 0x56c);
COD2_ASSERT_OFFSET(weaponDef_t, dropHintString, 0x570);
COD2_ASSERT_OFFSET(weaponDef_t, unk_0x574, 0x574);
COD2_ASSERT_OFFSET(weaponDef_t, unk_0x578, 0x578);
COD2_ASSERT_OFFSET(weaponDef_t, horizViewJitter, 0x57c);
COD2_ASSERT_OFFSET(weaponDef_t, vertViewJitter, 0x580);
COD2_ASSERT_OFFSET(weaponDef_t, script, 0x584);
COD2_ASSERT_OFFSET(weaponDef_t, adsTransInTimeInv, 0x588);
COD2_ASSERT_OFFSET(weaponDef_t, adsTransOutTimeInv, 0x58c);
COD2_ASSERT_OFFSET(weaponDef_t, minDamage, 0x590);
COD2_ASSERT_OFFSET(weaponDef_t, minPlayerDamage, 0x594);
COD2_ASSERT_OFFSET(weaponDef_t, maxDamageRange, 0x598);
COD2_ASSERT_OFFSET(weaponDef_t, minDamageRange, 0x59c);
COD2_ASSERT_OFFSET(weaponDef_t, destabilizationBaseTime, 0x5a0);
COD2_ASSERT_OFFSET(weaponDef_t, destabilizationTimeReductionRatio, 0x5a4);
COD2_ASSERT_OFFSET(weaponDef_t, destabilizationAngleMax, 0x5a8);
COD2_ASSERT_OFFSET(weaponDef_t, destabilizeDistance, 0x5ac);
COD2_ASSERT_OFFSET(weaponDef_t, locationDamageMultipliers, 0x5b0);
COD2_ASSERT_OFFSET(weaponDef_t, fireRumble, 0x5fc);
COD2_ASSERT_OFFSET(weaponDef_t, meleeImpactRumble, 0x600);

#endif /* COD2_CANON_H */
