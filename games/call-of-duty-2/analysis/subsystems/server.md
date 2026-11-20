# Subsystem: server (sv_*, client connection lifecycle, snapshots, commands, master/rcon/bans)

Target: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`
(ELF32 i386, GCC 3.3.6 C++ written in C style). Claim tags: **[O]** observed (string, disassembly, table
read from the binary, or consistent offsets in several chunks), **[I]** inferred (structure / id Tech 3
lineage), **[U]** unknown. Addresses are virtual addresses in the ELF. Source chunks used:
008, 009, 019, 020, 021, 022, 023, 029, 038, 100, 101 (finished markers present), plus 045/046/048/051
for the game-side boundary. Chunks 102 and 053 have no finished `symbols/chunk_NNN.json` and were not used
as evidence. Companion layout file: `server.structs.json`.

Function coverage: about 420 functions: tags `server/*` (excluding `server/script*`, which is the
script variable system, see "Tag noise"), `net/*`, `msg/*`, `client/usercmd|records`, the three chunk 100
functions not yet merged into `symbols.json`, and clearly related functions with other tags
(`game/client`, `game/cmds`, `game/script` network helpers, ip-filter in chunk 51).

## 1. Purpose and architecture

The binary is a dedicated-only Quake III / IW 2.x-style server. There is no cgame/client in the Linux
server binary, but `client_s` (server-side per-connection slot) and snapshot code are retained [O]. The
game logic (`G_*`, `Client*`, script VM) lives in the same executable (no `qagame.so`): the server calls
game entry points directly by address (`ClientConnect`, `ClientBegin`, `ClientThink`, `ClientCommand`,
`ClientDisconnect`, `ClientUserinfoChanged`, `fn_0810a13a` = run game frame) [O: direct `call`s in
chunk 20/22 sources and disassembly, e.g. caller of `SV_ArchiveSnapshot` flow].

Layers (all [I] from structure, names [O] where noted):

1. Transport: `NET_*`/`Sys_*` UDP (chunk 38), loopback (chunk 9), `Netchan_*` (chunk 9), OOB helpers.
2. Message layer: `MSG_*` bit buffer (chunk 8). Whole-message adaptive Huffman is applied around
   `SV_SendMessageToClient` (`Huff_CompressBytes`, first 4 bytes raw) and `SV_ExecuteClientMessage`
   (`Huff_DecompressBytes`) [O: chunk 20/23 sources]. Inside a message, fields are raw bit packed
   (`MSG_ReadBits(&msg, 3)` for the client opcode) [O: 0x0809114e].
3. Connection lifecycle and authorize: `SV_DirectConnect`, challenge table, ban files, drop, gamestate.
4. Frame loop: `SV_Frame`, timeouts, pings, bots, game frame, snapshot build/send, heartbeat.
5. Server commands: console `SV_*_f` commands, client command table, reliable server commands.
6. World: entity link/unlink, traces (chunk 23 `SV_LinkEntity`, `SV_Trace`...), shared with collision.
7. Out-of-band services: `getstatus`, `getinfo`, `getchallenge`, `connect`, `ipAuthorize`, `rcon`, `v`.

## 2. Entry points and call flow

External entry from common code [O: call-site search with objdump of the whole binary]:

| Caller (address) | Callee | Meaning |
|---|---|---|
| 0x080622ca (Com_Init) | `SV_AddServerCvars` 0x08093adc | register server dvars + operator commands |
| 0x080627ce (Com_Frame) | `SV_Frame` 0x080969b0 | per-frame server tick, argument msec |
| 0x0806196d, 0x08061a78 (Com_EventLoop) | 0x080960e2 | packet from network, argument (netadr by value, msg*) |
| 0x08060eac, 0x08061271 | `SV_Shutdown` 0x080942f8 | server teardown |
| 0x0808be22, 0x0808befa (map cmds) | `SV_SpawnServer` 0x08093520 | map load |
| 0x08116bde (game script) | `SV_AddBot` 0x080913da | bot spawn |

### 2.1 Packet entry (0x080960e2, name see section 8)

[O: 0x080960e2, source chunk 22] If `msg->cursize >= 4` and the first long is not `-1`: it is a sequenced
packet. Read `long` (sequence, ignored here) then `short` qport; find the client slot with
`state != 0 && NET_CompareBaseAdr(from, netchan.remoteAddress) && netchan.qport == qport`; if the port
differs, print "fixing up a translated port" and update `netchan.remoteAddress.port`. Then
`Netchan_Process`; on success read `serverId` (byte, stored at +0xae710), `messageAcknowledge` (long,
+0x20818, negative = reject), `reliableAcknowledge` (long, +0x20810; if
`reliableSequence - reliableAcknowledge > 0x7f` clamp and return), then `SV_Netchan_Decode` of the
remaining bytes, ignore if `state == CS_ZOMBIE`, set `lastPacketTime = svs.time`, call
`SV_ExecuteClientMessage`. No slot matched: OOB print "disconnect". If the first long is `-1`:
`SV_ConnectionlessPacket` 0x0809594e.

### 2.2 Connectionless packets (0x0809594e) [O]

Tokenizes the line, dispatches on argv[0] (strcmp): `getstatus` -> `SV_StatusResponse`; `getinfo` ->
`SV_InfoResponse`; `getchallenge` -> `SV_RequestIpAuthorize`; `connect` -> PunkBuster callback then
`SV_DirectConnect`; `ipAuthorize` -> `SV_AuthorizeIpPacket`; `rcon` -> `SVC_RemoteCommand`; `v` -> 0x08094b56
(voice relay). Packets whose text starts with `pb_` go to PunkBuster (`PB_DispatchEvent3(0xd, ...)`, except
sub-opcodes 'C','1','J' in data[7]). `getstatus`, `getinfo`, `getchallenge`, `ipAuthorize` call
`SV_NoteAuthorizeServerPacket` first (sets "last time heard from master" if the sender equals the master
address). Cvar `sv_packet_info` prints "SV packet %s : %s". `disconnect` as OOB is a no-op (result of
`Q_stricmp` discarded) [O].

### 2.3 Connect / auth flow [O]

1. `getchallenge` -> `SV_RequestIpAuthorize` 0x0808d18e: picks the slot in the 1024-entry challenge table
   whose `adr` equals the sender and `connecting == 0`, otherwise evicts the oldest `firstTime`;
   new challenge = `rand()<<16 ^ rand() ^ svs.time`. LAN senders (`Sys_IsLANAddress`) when
   `net_lanauthorize` is 0 get `challengeResponse %i` immediately. Otherwise it resolves
   `cod2master.activision.com` once into `svs+0x1d070` (port 20700 = 0x50dc, BigShort), and, unless the
   authorize server has been silent >1,200,000 ms and the slot is >7000 ms old (then it answers
   `challengeResponse` itself: "authorize server timed out"), sends
   `getIpAuthorize <challenge> <a.b.c.d> "<fs_game>" <sv_allowAnonymous> [PB "<pbname>"]` (0x0808cfc6).
2. `ipAuthorize <challenge> <verb> <reason> <guid> <key>` -> `SV_AuthorizeIpPacket` 0x0808da2a. Must come from
   the authorize server (base address compare) else "not from authorize server". Verbs: `accept` (store guid,
   check ban.txt then 16-slot tempban table, then `challengeResponse`), `demo` (needs `fs_restrict`, else
   `error\nEXE_ERR_NOT_A_DEMO_SERVER`), `deny` (reasons CLIENT_UNKNOWN_TO_AUTH/BAD_CDKEY -> `needcdkey`,
   INVALID_CDKEY -> `EXE_ERR_CDKEY_IN_USE`, BANNED_CDKEY -> `EXE_ERR_BAD_CDKEY`), anything else raw error.
   Failures `memset(slot, 0, 0x74)`.
3. `connect "<userinfo>"` -> `SV_DirectConnect` 0x0808e2aa: protocol must be 0x76 (118) else
   `error\nEXE_SERVER_IS_DIFFERENT_VER\x15<1.3>`; reconnect throttle `sv_reconnectlimit` seconds (using
   `lastConnectTime`); challenge must match slot (`EXE_BAD_CHALLENGE`); `sv_minPing`/`sv_maxPing` checked
   against the cached challenge ping (non-LAN only); PunkBuster hook `PB_InvokeCb15c(addr, cl_punkbuster, keyA)`
   may veto; existing slot with same address+qport is reused ("reconnect"); otherwise first free slot at or above
   `sv_privateClients` unless the `password` userinfo equals `sv_privatePassword` (then from 0);
   `EXE_SERVERISFULL`; then `memset(slot, 0, 0xB1064)`, set gentity, script handle (`ScrVar_AllocArray`), challenge,
   guid, `Netchan_Setup(1, ...)`, `voiceEnabled = 1`, userinfo; `ClientConnect(num, scriptHandle)` returns
   an error string or NULL. On success `state = CS_CONNECTED`, times set, authorize keys copied,
   `SV_UserinfoChanged`, `gamestateSeq = -1`; heartbeat forced when the connected count becomes 1 or equals
   `sv_maxclients`.
4. First client packet in CONNECTED state triggers `SV_SendClientGameState` 0x0808f302 from
   `SV_ExecuteClientMessage` when `serverId` does not match the current `svs` serverid counter
   (see 2.5).
5. Gamestate packet [O]: write long `lastClientCommandSeq` (+0x20840), pending reliable commands, byte 1
   (svc_gamestate), long `reliableSequence`, then for each of 0x800 configstrings with non-empty text: byte 2,
   short index, big string; then for each of 1024 sv entities with non-zero baseline number: byte 3 + delta entity
   against zero state; byte 7 (EOF); long client number; long `sv.checksumFeed`; byte 7. `state = CS_PRIMED`,
   `pureStatus = 0`, `gamestateSeq = netchan.outgoingSequence`.
6. `SV_ClientEnterWorld` 0x0808f628: PRIMED -> ACTIVE on the first usermove; copies usercmd into
   `lastUsercmd`, calls `ClientBegin`.

### 2.4 Server-to-client message layout (observed opcodes) [O]

Byte opcodes written by the server: 1 gamestate, 2 configstring, 3 baseline entity, 4 server command
(long seq + string, 0x080988b2), 5 download block (0x0808fdc2: short block no, long size on block 0, short len,
data), 6 snapshot (0x080985c8), 7 end of message. A server message starts with a long (the
`lastClientCommandSeq` echo, see 0x0809adea). Snapshot body: byte 6, long `svs.time`, byte deltaNum, byte
snapFlags, delta playerstate, delta entities terminated by entity number 0x3ff in 10 bits (0x08098308), delta
client states terminated by a 0 bit (0x0809847c), then `sv_padPackets` zero bytes. `snapFlags`: low bits
`svs+8` (bit 4 toggled on each spawn/fast restart = SNAPFLAG_SERVERCOUNT-like, [I]), `|1` when rate delayed,
`|2` when the client is not ACTIVE. Delta basis is `frames[deltaMessage & 31]`, refused (full snapshot) if
`deltaMessage < 1`, client not ACTIVE, older than 28 packets (`outgoingSequence - deltaMessage >= 0x1d`) or
older than the entity ring (`oldFrame->firstEntity < nextSnapshotEntities - numSnapshotEntities`).

Client-to-server (after Huffman decompress into a 0x20000 temp buffer, 0x0809114e) [O]: loop reading 3 bits:
2 = client command (`SV_ClientCommand`: long seq, string), 0 = usermove with delta, 1 = usermove without
delta (`SV_UserMove` deltaFlag 1/0, note: 0 -> deltaFlag 1), 3 = no-op/end; anything else prints
"WARNING: bad command byte %i for client %i". In `SV_UserMove`: byte cmdCount (1..32), usercmds
delta-coded with `MSG_ReadDeltaUsercmdKey`, key = `sv.checksumFeed ^ messageAcknowledge ^ Com_HashKey(
reliableCommands[reliableAcknowledge & 0x7f].text, 32)` [O]. Weapon bytes in each usercmd are validated with
`BG_PlayerHasWeapon` and replaced from playerstate fields +0xd4/+0xd0.

The `SV_ExecuteClientMessage` decision [O]: if `serverId == svs counter (0x08423008)` or a download is in
progress, parse commands; else if the high nibble matches (fast restart), a PRIMED client is entered into
the world with its last usercmd; else if `gamestateSeq < messageAcknowledge`, print "dropped gamestate,
resending" and resend the gamestate. A `sv_pure` client with `pureStatus == 2` is dropped with
`EXE_UNPURECLIENTDETECTED` but still sent one final snapshot.

### 2.5 Server frame (`SV_Frame` 0x080969b0) [O]

`if (record(0x817d838)+0xdd == 0)`: if `sv_running` and not `SV_CheckPaused()`: `frameMsec = 1000/sv_fps`,
accumulate `sv.timeResidual` (0x08440210); when >= frameMsec run overflow guards (any counter near
2^31 triggers `Com_ServerShutdown` + `map <current>`, strings `EXE_SERVERRESTARTTIMEWRAP` when
`svs.time >= 0x70000001`, `EXE_SERVERRESTARTMISC\x15<counter name>` for numSnapshotEntities,
nextCachedSnapshotEntities/Clients/Frames, nextArchivedSnapshotFrames/Buffer, numSnapshotClients), update
serverinfo/systeminfo configstrings 0 and 1 from modified dvar flags (0x404 -> CS 0, 8 -> CS 1, 0x100 ->
iterate), `SV_RunBotClients`, `SV_CalcPings`, then loop: `timeResidual -= frameMsec; svs.time += frameMsec;
SV_RunGameFrame; Scr_SetLoading(0)`, archive a snapshot for every extra step; then `SV_CheckTimeouts`,
`SV_SendClientMessages`, `SV_ArchiveSnapshot`, `SV_MasterHeartbeat("COD-2")`. If the kill flag at +0xdd is
set instead: `Com_ServerShutdown(+0xde string or "EXE_SERVERKILLED")` and clear the flag.
`SV_RunGameFrame` 0x08096752 = `SV_SkelMemoryNewFrame(); fn_0810a13a(svs.time)`.

Timeouts [O] (0x0809657e): clamp `lastPacketTime`; ZOMBIE older than `sv_zombietime` -> FREE; CONNECTED+
older than `sv_timeout` increments `timeoutCount`, `> 5` -> `SV_DropClient(EXE_TIMEDOUT)` and state FREE;
`isBot` slots skipped. Pings (0x080963c8): average over the 32 frames of (`messageAcked - messageSent`)
for ACTIVE clients, 999 otherwise or when no gentity.

### 2.6 Snapshot pipeline [O unless noted]

- `SV_ArchiveSnapshot` 0x0809b016: when `sv.state == 2` and archiving enabled (`svs+0x28`), writes a full or
  delta frame of all connected clients' `clientState` + playerstate and visible entities into the 32 MiB
  archive ring (`svs+0x34`, 0x2000000 B) plus cached frame/entity/client rings; a full frame is forced when no
  usable earlier cached frame within `sv_fps` archive frames exists (cached frames: 0x200 x 0x1c bytes;
  cached entities 0x4000 x 0x114; cached clients 0x1000 x 0x2708). [I] purpose: rewinding for lag
  compensation (`SV_FindCachedSnapshotByAge`, `SV_GetInterpolatedPlayerOrigin`, `G_SnapshotClientPositions`).
- `SV_BuildClientSnapshot` 0x0809a408: copies playerstate (0x26a8) into `frames[outSeq & 31]`, builds the
  entity list by PVS (`SV_BuildClientListForPoint`/cached variant), appends entityStates (0xf0 each) to the
  `svs` entity ring and clientStates (0x5c each) to the client ring; error string
  "SV_BuildClientSnapshot: bad gEnt" when ps.clientNum > 0x3ff. When a cached snapshot is selected, four
  time-like words (entityState words 4, 0xd, 0x15, 0x16) are rebased by the cached frame age.
- `SV_SendClientMessages` 0x0809bcce: for each non-free client with `nextSnapshotTime <= svs.time`:
  if no unsent fragments `SV_SendClientSnapshot` + `SV_SendClientVoiceData`, else rate-limited
  `SV_Netchan_TransmitNextFragment`. `sv_showAverageBPS` keeps 20-sample bps statistics.
- `SV_SendMessageToClient` 0x0809aba2: Huffman-compress (first 4 bytes raw), record
  `frames[outSeq&31].messageSize/messageSent=svs.time/messageAcked=-1`, `SV_Netchan_Transmit`; LAN/loopback
  next snapshot immediately; else `SV_RateMsec = (len*1000+48000)/rate` (rate bounded by `sv_maxRate`),
  `nextSnapshotTime = now + max(rateMsec, snapshotMsec)`, `rateDelayed` flag; non-active clients get >= 1000 ms.
- Overflow of the 0x20000 message: retry with `SV_UpdateServerCommandsToClientLimited`, else drop with
  `EXE_SERVERMESSAGEOVERFLOW`.
- `SV_Netchan_Encode/Decode` 0x080974d8/0x08097562: XOR keystream. Server->client: key0 =
  `outgoingSequence ^ challenge` (bytes), mixed with `lastClientCommandString` (shift by `i&1`). Client->server:
  key0 = `challenge ^ serverId ^ messageAcknowledge`, string = `reliableCommands[reliableAcknowledge & 0x7f]`.
  Encoding applies only after the 4-byte header (`SV_Netchan_Transmit`).

### 2.7 Reliable server commands [O] (chunk 21)

128-slot ring per client (`reliableCommands[128]`, each text[0x400] + time + flags). `SV_SendServerCommand`
formats into a 0x20000 pool buffer; `cl == NULL` broadcasts to every slot with `state > 2`;
`SV_QueueReliableCommand` 0x08094750: skipped for bots (+0xae70c); if more than 0x40 pending or not
ACTIVE it first compacts (`SV_CompactReliableCommands` drops entries with `flags == 0` after `reliableSent`)
and only queues when `flags != 0`; duplicates are merged by `SV_FindReliableCommand` using the leading
command letter (cases 'C','D','a','b','o','p','q','r','t' replace by letter; 'd','v' compare tokens via
`SV_TokenMatch`); overflow at `reliableSequence - reliableAcknowledge == 0x81`: dump, OOB "disconnect", defer
drop `EXE_SERVERCOMMANDOVERFLOW`. Command letters observed: `d` configstring (long form `x`/`y`/`z` chunks for
strings longer than 0x400 - header), `e` print (0x65, used by drop messages), `J` (0x4a, client left id),
`w` (0x77, disconnect with reason), `B`/`n` (0x42/0x6e, sent on fast restart depending on level flag).
`SV_UpdateServerCommandsToClient` writes `4, seq, string` for each seq in (`reliableAcknowledge`,
`reliableSequence`] and sets `reliableSent`.

### 2.8 Client command table (read from binary at 0x0817d9e0, stride 8) [O]

userinfo 0x08090a04, disconnect 0x08090518, cp 0x08090534 (`SV_VerifyPaks_f`), vdr 0x080907ba, download
0x0808f940, nextdl 0x0808f83c, stopdl 0x0808f780, donedl 0x0808f7d0, retransdl 0x0808f7fc, wwwdl 0x0808f97c,
muteplayer 0x08090a5a, unmuteplayer 0x08090aaa. Unknown commands fall through to game `ClientCommand(num)`
when `clientOK` and a game is running. Flood protection (0x08090bac): not for `team `, `score `, `mr `
prefixes; if `sv_floodProtect` and the record field +4 of 0x817d838 is zero and client state > 3 and
`svs.time < textFloodTime` the command is not forwarded ("client text ignored"); each chat-like command sets
`textFloodTime = svs.time + 800`. A command number gap sets `EXE_LOSTRELIABLECOMMANDS`.

### 2.9 Console / operator commands registered by `SV_AddOperatorCommands` 0x0808cca6 [O]

heartbeat, onlykick, banUser, banClient, kick (shares the `tempBanUser` handler), tempBanUser, tempBanClient,
unbanUser, clientkick, status, serverinfo, systeminfo, dumpuser, map_restart, fast_restart, map (aux pair
"maps/mp","d3dbsp"), map_rotate, gameCompleteStatus, devmap, killserver, scriptUsage, stringUsage; `say` and
`tell` (`SV_AddSayTellCommands`) only when `com_dedicated != 0`. Registration happens once (static guard).

### 2.10 Master heartbeat, status, rcon [O]

- `SV_MasterHeartbeat(name)` 0x08096ed6 runs only when `com_dedicated == 2` (internet): every 180,000 ms sends
  OOB `heartbeat COD-2` (name `flatline` from `SV_MasterShutdown`) to `cod2master.activision.com`, port 20710
  (0x50e6), cached at 0x0849fbe0; every 600,000 ms sends a full `statusResponse` packet to the master.
  `SV_MasterGameCompleteStatus` sends `gameCompleteStatus` (also console `gameCompleteStatus`).
  `SV_Heartbeat_f` just sets `nextHeartbeatTime = INT_MIN` to force it.
- `statusResponse\n<infostring>\n<score ping "name" lines>`: infostring from `Dvar_InfoString(0x404)`, plus
  `challenge`, `pswrd`, `mod` (1 when `sv_pure` off or `fs_game` set or referenced iwds not in "main"),
  `sv_keywords` prefixed `demo ` if `fs_restrict`. `infoResponse` keys: challenge, protocol, hostname,
  mapname, clients, sv_maxclients, gametype, pure, minPing, maxPing, game, sv_allowAnonymous, con_disabled,
  pswrd, ff (scr_friendlyfire), kc (scr_killcam), hw=1, mod, voice, pb.
- rcon (`SVC_RemoteCommand` 0x08097188): throttled to one per 500 ms (global last-time 0x0849fbf4, not
  per-address); password compared with `rcon_password`; empty password cvar -> message that rcon is
  disabled; redirect buffer 0x1fff0 bytes with `Com_BeginRedirect(..., SV_FlushRedirect)`; output sent as
  `print\n<text>` in 0x50e-byte OOB pieces to the requester; command line rebuilt with quoting (limit 0x400) and run
  by `Cmd_ExecuteString`; `pb_sv_*` lines also notify PunkBuster.

### 2.11 Bans [O]

Permanent: `ban.txt` in the fs (lines "<guid> <name>\r\n", `SV_BanClient` 0x0808d75a appends, host-player
protected, guid 0 refused); `SV_IsGuidPermBanned` scans first token of each line. Unban by name removes matching
lines (`SV_UnbanUserByName`). Temporary: 16-slot table at 0x0844010c, entries (guid, time); duration
`sv_kickBanTime` seconds (float, default 300); slot choice = first empty else oldest. `kick`/`tempBanUser` call the
shared `SV_Kick_f` which returns the guid. IP bans (`addip`/`removeip`, `g_banIPs`) live in the game module
(chunk 51 `StringToFilter`, `AddIP`, `UpdateIPBans`, `G_ProcessIPBans`) and are not part of the server core.
Ban checks run in the authorize reply (`accept`), not in `SV_DirectConnect`, so loopback/LAN skip them [O: no
call of `SV_IsGuidPermBanned` in the connect path in chunk 20 source].

### 2.12 Downloads [O/I]

`download <file>` (`SV_BeginDownload_f` 0x0808f940) -> `SV_WriteDownloadToClient` 0x0808fdc2 (blocks of
up to 8 queued buffers, window handled with `downloadClientBlock/XmitBlock`); `sv_allowDownload`,
`.iwd` only [I], "cannot download IW iwd files" refusal for `iw_*` pak files [O string]; HTTP redirect with
`sv_wwwDownload`, `sv_wwwBaseURL`, `sv_wwwDlDisconnected` via `SV_WWWDownload`/`SV_RedirectClientDownload`.

### 2.13 Bots [O]

`SV_AddBot` 0x080913da builds a `connect "\cg_predictItems\1\cl_punkbuster\0...\name\bot%d..."` userinfo string,
sets `from.type = 0 (NA_BOT)` with `port = counter`, runs `SV_DirectConnect`, flags the slot `isBot`, sends the
gamestate and enters the world with a zero usercmd. `SV_RunBotClients` generates random usercmds each frame for
slots whose `remoteAddress.type == 0` (`SV_BotRandomUserCmd`; weapon taken from gclient+0xd4).

### 2.14 Voice [O]

`sv_voice` gate. Voice is an OOB `v` string packet sent per client by `SV_SendClientVoiceData`: string "v",
count byte, then per packet talker byte, length byte, data (<= 256). Up to 40 queued packets per client
(`voicePackets` 0x105 bytes each). Inbound: `v` connectionless -> 0x08094b56 -> for slots not ACTIVE
(`state <= 3`) 0x0809c3c6 (count byte, **short** length, fan-out to every other client with `state > 1` that has
voice enabled and has not muted the talker, via mute bytes at +0xb0fe0), for ACTIVE slots 0x0809c2ee (count
byte, **byte** length, hands to `G_SayToEligibleClients`). Mute commands `muteplayer`/`unmuteplayer`.

## 3. Global state (addresses and layout)

`svs` = 0x08423080, size 0x1d10c (`memset` in `SV_Shutdown`) [O]; `sv` = 0x08440200, size 0x5f534 [O].
Reconciled layout is in `server.structs.json` (`serverStatic_t`, `server_t`). Addresses that fall inside them:

| Address | Field | Evidence |
|---|---|---|
| 0x08423084 | svs.time | `lea [..]/mov eax,ds:0x8423084` in SV_UserMove 0x8091038 |
| 0x08423088 | svs.snapFlags (xor 4 on spawn/fast restart) | chunk 19/21/22 |
| 0x0842308c | svs.clients | chunk 21 |
| 0x08423090..9c | numSnapshotEntities, numSnapshotClients, nextSnapshotEntities, nextSnapshotClients | chunk 22 vs 21 (names differ, same offsets) |
| 0x084230a0 / a4 | snapshot entity ring (stride 0xf0) / client ring (0x5c) | chunk 22, 21 allocs |
| 0x084230a8 | archiving enabled | chunk 22/23 |
| 0x084230ac..c4 | archive/cache counters; 0x084230b0 frame table (0x4b0 x 8), 0x084230b4 archive buffer (0x2000000), c8/cc/d0 cached rings | chunk 22, sizes match chunk 21 allocs |
| 0x084230d4 / d8 | next heartbeat time / next status-to-master time | chunk 22 [I for 0x84230d4: `g_svs_nextHeartbeatTime` has no printed address] |
| 0x084230dc | challenges[1024] x 0x74 | chunk 20 |
| 0x084400dc | rcon redirect address (netadr) | chunk 22 |
| 0x084400f0 | authorize-server address (resolved) | chunk 20 |
| 0x08440104 | last time authorize server heard | chunk 20 |
| 0x08440108 | out-of-band net profile pointer | chunk 22 |
| 0x0844010c | tempbans[16] x 8 | chunk 20 |
| 0x08440200..14 | sv.state (0 dead,1 loading,2 game), sv.restarting flag, sv.restartTime, sv.checksumFeed, sv.timeResidual | chunks 19,20,21,22 |
| 0x08440618 | sv.configstrings[0x800] | chunk 20, 21 |
| 0x08442618 | svEntities[1024] x 0x174 (baseline at +4) | chunks 20,22,23 |
| 0x08423008 | server-id counter (high nibble: map spawn count, low nibble: fast restarts); mirrored to dvar `sv_serverid` | chunks 19,20,21 |
| 0x0849f61c..34 | game entities base/stride/count, game clients base/stride, skel generation/offset | chunk 20 |
| 0x0849f734..0x0849f7d8 | server dvar pointers (see section 5) | chunk 21 |
| 0x0849fbe0 | master (heartbeat) address, 0x0849fbf4 last rcon time | chunk 22 |
| 0x0817d838 | pointer to a global record: +4 flag (chat-flood bypass and netprofile gate), +0xdd byte "kill server" flag, +0xde reason string | disasm `mov eax,ds:0x817d838` then byte offsets; chunk 21 declared it as an array (wrong) |
| 0x081a218c | `sv_running` dvar (chunk 6/7 observed) | chunk 21 calls it "dedicated flag": wrong |
| 0x084a8780 | `com_dedicated` dvar "dedicated" (chunk 7 observed) | |
| 0x0817d9e0 | client command table | read from binary |
| 0x083e2f0c | bot counter; 0x083e2f80 demo/record flag; 0x0860b420 misc flag cleared after each processed client packet | chunks 20, 21, 22 |

## 4. Data formats and protocols

- `netadr_t`: 20 bytes: `int type` (0 bot, 1 bad, 2 loopback; Q3 lineage 3 broadcast, 4 ip [I]), `byte ip[4]`, `uint16 port`
  (network order), `byte ipx[10]`. Passed by value as five dwords on the stack [O].
- `msg_t`: 0x18 bytes {overflowed, data, maxsize, cursize, readcount, bit} (chunks 8, 22, 38 agree; chunks 6, 20, 21 are
  truncated views). Delta field tables `netField_t` {name, offset, bits} 12 bytes: entity 59 entries
  (0x081417a0), shared entity 68 (0x08141ae0), playerstate 22 (0x08141f60) [O chunk 8 notes].
- Netchan: header long `sequence` with bit 31 = fragmented; server direction also carries `short qport`;
  fragment fields `int fragIndex, short fragLen`; fragment buffers 0x20000 each; sequence out of order is
  dropped; `showpackets`/`showdrop` cvars for debug (chunk 9). `Netchan_Setup(sock, chan, adr, qport)` zeroes 0x40040.
- OOB packet = `ff ff ff ff` + text (`NET_OutOfBandPrint(netsrc=1, adr, text)` where 1 = server socket).
- Usercmd 0x1c bytes (see structs).
- Challenge table slots 0x74 bytes; ban file `ban.txt`.
- Protocol constant 0x76 (118) in `connect`, `protocol` dvar (min=max=118, flags 0x1044) and `infoResponse`.

## 5. Dvars and commands registered (SV_AddServerCvars 0x08093adc) [O]

Format: name (type default [min,max] flags). `g_gametype` string "dm" 0x1024; `sv_keywords` "" 0x1004; `protocol`
int 118 0x1044; `mapname` "" 0x1044; `sv_privateClients` 0 [0,64] 0x1004; `sv_maxclients` 20 [1,64] 0x1025;
`sv_hostname` "CoD2Host" 0x1005; `sv_punkbuster` bool 0 0x1015; `sv_maxRate` 0 [0,25000] 0x1005; `sv_minPing`,
`sv_maxPing` 0 [0,999] 0x1005; `sv_floodProtect` 1 0x1005; `sv_allowAnonymous` 0 0x1004; `sv_showCommands` 0 0x1000;
`sv_disableClientConsole` 0 0x1008; `sv_voice` 0 0x100d; `sv_voiceQuality` 1 [0,9] 0x1008; `sv_cheats` 0 0x1018;
`sv_serverid` int 0x1048; `sv_pure` 1 0x100c; `sv_iwds`, `sv_iwdNames`, `sv_referencedIwds`,
`sv_referencedIwdNames` strings 0x1048; `rcon_password`, `sv_privatePassword` 0x1000; `sv_fps` 20 [10,1000]; `sv_timeout`
240 [0,1800]; `sv_zombietime` 2 [0,1800]; `sv_allowDownload` 1 0x1001; `sv_reconnectlimit` 3 [0,1800] 0x1001; `sv_padPackets` 0;
`sv_allowedClan1/2`; `sv_packet_info`; `sv_showAverageBPS`; `sv_kickBanTime` float 300.0 [0, 3600.0]
(bits 0x43960000 / 0x45610000); `sv_mapRotation`, `sv_mapRotationCurrent`; `sv_debugRate`; `sv_debugReliableCmds`; `nextmap`;
`com_expectedHunkUsage` 0x1040; `sv_wwwDownload`, `sv_wwwBaseURL`, `sv_wwwDlDisconnected` 0x1001. Also looked
up by name elsewhere: `fs_game`, `fs_restrict`, `g_password`, `net_lanauthorize`, `scr_friendlyfire`, `scr_killcam`.
Chunk 48 `Dvar_RegisterServerDvars` (0x081084a4) registers game-side dvars (outside this report).
Flag meaning (0x1000 = server-info archive class, 0x0004 serverinfo, 0x0001 archive, 0x0008 / 0x0010 ...) is [U]: it is
in the dvar subsystem report.

## 6. Dependencies on other subsystems

Dvar/cmd (`Dvar_*`, `Cmd_*`), filesystem (`FS_ReadFile("ban.txt")`, `FS_Restart(checksumFeed)`, `FS_LoadedIwdChecksums`
for `sv_pure`), collision (`CM_LoadMap`, `CM_ModelBounds`, PVS cluster bit lookups in
`SV_EntityVisibleToClient`), game module (`G_*`, `Client*`), script (client script handle per slot:
`ScrVar_AllocArray`, `Scr_ReleaseVarThunk`), PunkBuster (`PB_InvokeCb15c/158`, `PB_DispatchEvent3`), memory pool
(`Com_PoolAllocInto2`/`Com_PoolTouchAt`/`Com_PoolRestoreFrom2`, `Hunk_Alloc`), localisation
(`Loc_TranslateString` used by `SV_DropClient` to decide the message form), FX (`Fx_Shutdown_Wrap1`, `FX_LoadMissingEffect` in
`SV_SpawnServer` when `com_dedicated != 0` [O]; purpose unknown).

## 7. Behaviours a native port must reproduce

1. Wire compatibility with CoD2 1.3 clients: protocol 118, Huffman whole-message coding with the first 4 bytes raw,
   netchan fragmentation with qport only on the server direction, the XOR keystreams (both directions), the
   usercmd delta key (`checksumFeed ^ messageAcknowledge ^ Com_HashKey(reliable text, 32)`), delta field tables.
2. `connect` handshake exactly as 2.3 including authorize server dance, challenge table size 1024, eviction rule,
   reconnect throttle, private slots, `connecting` flag semantics, bot connection through `NA_BOT`.
3. Server tick `1000/sv_fps` with residual, `svs.time` advanced in fixed steps, three warm-up frames at map load
   (`svs.time += 100` x3), snapshot archive every game step.
4. Reliable command ring semantics including merge-by-letter and compaction after `reliableSent`.
5. Rate limiter formula `(size*1000+48000)/rate`, `rateDelayed` bit in snapshot flags, 1000 ms minimum for non-active.
6. Master `heartbeat COD-2` timers (180 s / 600 s status), `flatline` on shutdown, `getstatus/getinfo` key sets.
7. Ban/authorize semantics (checked on authorize reply, `ban.txt` format with CRLF, temp bans in 16 slots, `kick` == `tempBanUser`).
8. Snapshot cache/rewind behaviour if lag-compensated hit detection is to match (game side uses cached
   playerstates).
9. Game-facing contracts: `ClientConnect(num, scriptHandle)` returns error string or NULL; slot `memset` to zero
   before connect; `gentity` pointer bound at connect (`SV_GentityNum`) and again at enter-world.
10. Overflow guard restarts (`EXE_SERVERRESTART*`), `EXE_*` localisation tokens are passed as strings to clients (prefix
    `\x15` marks a localisation reference in several messages).

## 8. Naming problems (evidence)

| Item | Problem | Evidence / proposed name |
|---|---|---|
| 0x08094b56 vs 0x080960e2 | both named `SV_ReadPackets`; merge lane already renamed the second `SV_ReadPackets_080960e2` (conflicts.json). Only one rodata string "SV_ReadPackets: fixing up a translated port" (file offset 0x103540, vaddr 0x0814b540) is referenced by both. 0x080960e2 is the Q3 `SV_PacketEvent` (called from Com_EventLoop 0x0806196d/0x08061a78, runs Netchan_Process). 0x08094b56 is only called from the `v` branch of `SV_ConnectionlessPacket` (0x08095bde). Propose `SV_PacketEvent` and `SV_VoiceConnectionlessPacket`. | [O] callers, [I] names |
| `client_s` in chunks 51 and 102 | those chunks use `client_s` for the game-side `gclient_s` (name at +0x2784, state at +0x26c4, stride 0x28a4); the server `client_t` has name at +0x20c48, stride 0xb1064. Same type name, different structs | [O] offsets; keep `client_t` (server) vs `gclient_t` (game, with embedded `clientState` at +0x2748, name at +0x2784) |
| 0x081a218c / 0x084a8780 | chunk 21 calls 0x081a218c a "dedicated flag"; chunk 6/7 observed `sv_running` for 0x081a218c and `dedicated` for 0x084a8780 | [O] |
| chunk 20 `COD2_CL_FRAME_TIME_PTR` | base 0x236F0 is wrong; real instruction at 0x08091038 is `lea edx,[eax+0x234f0]` with `eax = (messageAcknowledge & 0x1f)*0x26c4 + cl`, i.e. `frames[messageAcknowledge & 31].messageAcked = svs.time` (as in Q3 `SV_UserMove`) | [O] disassembly |
| client +0x20840 | chunk 23 calls it `reliableAcknowledge` (it is written in the first long of each snapshot); the real `reliableAcknowledge` is +0x20810 (chunks 20-22). Chunk 20 name `lastClientCommandSeq` is correct (compared in `SV_ClientCommand`, written to gamestate and snapshot headers) | [O] |
| client +0x20818 | chunk 20 `messageSeq`, chunk 22 `messageAcknowledge`: same field (read as long after the header in 0x080960e2, used as ack index and in the usercmd key) | [O] |
| client +0x20e28 | chunk 20 `enterWorldTime`, chunk 23 `nextSnapshotTime`; chunk 20 writes it with `svs.time` at connect and enter-world and -1 on unpure drop, chunk 23 compares it in `SV_SendClientMessages`. Treat as `nextSnapshotTime` (Q3 layout puts it after `lastConnectTime`) | [O] usage, [I] name |
| client +0x20e08..0x20e14 | chunk 20 names them `wwwDownload/redirectSent/downloadGate/downloadRetry`; chunk 101 names +0x20e0c..14 `bWWWDl/bWWWing/bFallback` and omits +0x20e08. `cl_wwwDownload` userinfo is written to +0x20e08 (disasm 0x80909ca/0x80909f3) = `bDlOK` | [O] |
| client +0x8 | chunk 20 `pendingDrop` (int), chunk 23 `dropReason` (const char *): same field holding the reason string passed to `SV_DropClient` | [O] |
| client +0xae708..0xae710 | `clientRandomId` (020), `slotHandle` (022), `clientDeniedCheckArg` (019): one uint16 holding the script object handle allocated by `ScrVar_AllocArray` and released by `Scr_ReleaseVarThunk`; the value is the 2nd argument of `ClientConnect`. +0xae70c is `isBot` (020), `noTimeout` (022), "gates reliable queueing" (021): one field. +0xae710 chunk 20 `lastCmdSeq` and chunk 22 `serverId` are one field | [O] |
| client +0x6e6c4 netchan | sizes: chunk 9 0x40040 (includes the `profile` pointer at +0x4003c, `Netchan_Setup` memset 0x40040), chunk 22 0x4003c with `netProfile` at client+0xae700, chunk 23 0x2003c truncated. Same layout (0x6e6c4 + 0x4003c = 0xae700) | [O] |
| client +0x6e6b4 / +0x6e6b8 | chunk 21 `ping` at 0x6e6b4 vs chunk 20 `rate` at 0x6e6b8: both right (adjacent fields); chunk 101 has both | [O] |
| `SVC_RemoteCommand` | chunk 21 calls it with `(from, msg)`, chunk 22 defines it as `(netadr_t from)`. The call at 0x08095e1d pushes 6 dwords (from + msg); callee does not read the 6th (no `[ebp+0x1c]`) | [O]; prototype should be `(netadr_t, msg_t *)` with msg unused |
| chunk 21 `g_0817d838` | declared `void *[]`; binary loads a pointer then uses byte offsets 0xdd, 0xde, dword +4 | [O] |
| chunk 20 `gentity_s` field names | `origin104/origin110/vec138/vec144/linkCount11c/inUse` vs chunk 23/46 `mins/maxs/currentOrigin/currentAngles/contents` and chunk 21/22 `absmin/absmax` at 0x120/0x12c vs chunk 21 `currentOrigin/currentAngles` at 0x120/0x12c. Chunk 23/46 agree on 0x104 mins, 0x110 maxs, 0x11c contents, 0x120 absmin, 0x12c absmax, 0x138 currentOrigin, 0x144 currentAngles. Chunk 21 mislabels 0x120/0x12c as origin/angles; chunk 20 `inUse` at 0xf0 is `linked` | [O] by consistency of 23 + 46 + 22 |
| `sv.archivedEntities` (chunk 21) | is the `baseline` entityState inside `svEntity_t[i]` at +4 (0x0844261c = 0x08442618 + 4); chunk 20 `sv_entity_s` base 0x08442618 and chunk 23 `svEntity_s` agree | [O] |
| chunk 22 `g_08440210 = svs.timeResidual` | address is `sv`+0x10 (sv at 0x08440200), so it is `sv.timeResidual` | [O] |
| chunk 38 json | addresses printed as `0x0080d5160` (extra zero) in `chunk_038.json`; `symbols.json` has normalised `0x080d5160` | [O] |
| `SV_MapRestart_f` (019) vs `SV_MapRestart_cmd/SV_FastRestart_cmd` | `SV_MapRestart_f(int fast)` is a helper; registered handlers are the `_cmd` wrappers | [I] |
| `Cmd_Argv_alt`/`Cmd_Argc_alt`/`Cmd_TokenizeString_alt` vs `Cmd_Argv` | two argument-vector APIs are used (0x0808da2a uses both `Cmd_Argv_alt(1)` and `Cmd_Argv(5)`); the `_alt` suffix is a merge artefact, not evidence of two distinct tokenizers | [U] |
| `SV_StatusResponse(...)` heartbeat reuse | sends full status to master using challenge argv(1) of whatever was last tokenized | [I] |
| voice naming | 0x0809c2ee is named `...Local` and 0x0809c3c6 `SV_ParseVoicePackets`; caller (disasm 0x8094c44) sends `state <= 3` to 0x0809c3c6 and ACTIVE to 0x0809c2ee, which makes "Local" the active-player path (team routing in game code) | [O] |

Tag noise: tags `server/script_*` (85 + others in chunks 13-14), `client/fx*`, `net/huffman` (Huffman coder,
shared with `msg`), `server/game/*` (game code in chunks 46), `server/ipfilter` (game module) do not belong to the
core server; they are listed here only where they touch the protocol.

## 9. Open questions

- [U] What `0x0817d838` points to (looks like a Com/Sys global record with the "kill server" request and a
  "no flood protection" flag at +4); owner subsystem should name it.
- [U] `svs+0x2c/+0x38..` counters' exact roles besides what chunk 22 states; archive purpose (lag compensation
  guessed [I]).
- [U] Whether `SV_ArchiveSnapshot` frames are consumed by anything besides `SV_BuildClientSnapshot`.
- [U] Meaning of command letters 'C','D','a','b','o','p','q','r','t','v' in the reliable merge switch (client
  commands in cgame); only the letters are observed.
- [U] `usercmd.unk09` (+0x09) meaning (offhand weapon, copied from gclient+0xd0).
- [U] Why `SV_SpawnServer` calls FX init/shutdown when `com_dedicated != 0` (server-side effects for e.g. blood/impact
  testing?).
- [U] The `\x15` prefix semantics (CoD2 localisation escape) in error strings: only observed.
- [U] `cl->snapshotActiveFlag` (+0x4) full meaning beyond SNAPFLAG not-active bit.
- [U] The full `gentity_s` / `gclient_s` stride (0x274 observed in chunk 23, runtime `g_0849f620`).
- [U] Checksum verification code of `SV_VerifyPaks_f` (`cp`) and pure handling (chunk 101) was not reviewed.
- [U] Whether `Netchan_Setup`'s `sock` argument `1` can be 0 anywhere in the server (client side absent).
