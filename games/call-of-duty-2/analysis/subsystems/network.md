# Subsystem: network (cod2_lnxded 1.3)

Binary: `cod2_lnxded`, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`, ELF32 i386, GNU C++98.
Scope: UDP sockets, netchan, message bit streams, Huffman, out-of-band (OOB) packets, server-side connection/snapshot protocol glue, master/authorize traffic, PunkBuster (PB) glue.
Method: finished `analysis/symbols/chunk_NNN.json` (chunks 000-062, 100-105 present), reconstructed `recon/chunks/chunk_NNN.cpp` + `_types.h`, `objdump -d` of the binary, and `.rodata` reads of the ELF (no REA/Ghidra session opened). Tags: **[O]** observed (read from binary bytes/disassembly/strings, or directly from a reconstruction whose offsets/strings I re-checked), **[I]** inferred, **[U]** unknown. Lineage (Quake III / IW) is used only to choose names; every behaviour below is this binary's.

Selection note: the worker `subsystem` tags are noisy. I selected `net/*`, `msg/*`, `pb/*`, `crypto/*`, `net`-adjacent `common/checksum`, `common/endian|byteorder` (endian wrappers only), plus the clearly network `server/{net,netchan,netprofile,master,authorize,rcon,download,snapshot,snapshot-cache,voice,clientcmd,usercmd,gamestate,connect,ban,ipfilter,status,client}` functions, the `import/libc` socket thunks, and the tag-less functions I found by call-graph (main loop, PB callbacks, chunk 101 download/pure/mute commands, chunk 100 Huffman/NET wrappers). `MsgBuf_*` (tag `msg/buffer`) is **not** network (section 9).

---------------------------------------------------------------------

## 1. Purpose and architecture

This is a **dedicated server only** binary [O: `Sys_GetPacket`/`Sys_SendPacket` have a single `ip_socket`, `SV_*` everywhere, `main` at 0x080d4f7c has no client/renderer path]. The network stack is a Quake III descendant with Call of Duty 2 specific changes:

1. **Transport** (`net/udp`, chunk 038): one non-blocking IPv4 UDP socket, `NET_Sleep` via `select`, no IPX socket ever opened (`ipx_socket` stays 0) [O].
2. **Packet dispatch** (`Sys_GetEvent` -> `Com_EventLoop` -> `SV_ReadPackets_080960e2`): datagram becomes an `SE_PACKET` event carrying a 0x14-byte `netadr_t` prefix, then either connectionless (first dword `0xFFFFFFFF`) or in-band netchan traffic [O].
3. **Netchan** (`Netchan_*`, chunk 009): sequence number + optional qport + optional fragment header. **There is no ack field in the netchan header** (acks live in the payload) [O].
4. **Payload obfuscation + compression**: server->client payload (after a 4-byte raw header) is **static-Huffman compressed then XOR-keystream encoded**; client->server payload (after serverId/ack fields) is XOR-decoded then Huffman-decompressed [O]. The Huffman tree is built once from a fixed 256-entry frequency table (not adaptive during play) [O].
5. **Message layer** (`MSG_*`, chunk 008/009): a *hybrid* byte/bit buffer (section 5.2), delta coders driven by `netField_t` tables for entityState, a second table for "shared entity" (server-internal snapshot cache/baselines), clientState, playerState, objectives and hudelems.
6. **Server protocol glue** (chunks 020-023, 101): challenge/authorize against `cod2master.activision.com`, `connect`, gamestate, snapshots, reliable server commands (128-entry ring with de-dup/compaction), downloads (2048-byte blocks, 8-block window, optional www redirect), voice, rcon, status/info responses, net profiling.
7. **PunkBuster glue** (chunk 061): a C++ global object that `dlopen`s `pb/pbsv.so` and brokers events/callbacks; the engine only forwards `pb_*` OOB packets and a few lifecycle events.

Protocol number 0x76 (118) is hard-coded in `SV_DirectConnect`, `SV_InfoResponse`, `SV_AddBot` and the read-only `protocol` dvar; version string in the reject message is `"1.3"` [O: 0x0808e2aa, 0x0809537c, 0x080913da, 0x08093adc].

---------------------------------------------------------------------

## 2. Entry points and call flow

### 2.1 Start-up / shutdown [O unless noted]

| Step | Function (addr) | Notes |
|---|---|---|
| process entry | `main` 0x080d4f7c (**no symbol in any finished chunk**) | geteuid/seteuid, `Sys_InitMutexes`, `Sys_ParseArgs`, `Com_Init` (0x08062316), `Sys_ConsoleInputInit`, `PB_ServerInit` (0x0813c02c) at 0x080d50fd, then loop: `Sys_StoreFPUControlWord`, `usleep`, `Com_Frame` (0x080627f4), `PB_QueryServerEvent0` (0x0813c07a) at 0x080d5118 each iteration [I from call list; bodies not reconstructed] |
| qport seed | `Com_Init` -> `NET_InitCvars(Com_PumpSysEvents() & 0xffff)` 0x0806b8be | qport = low 16 bits of the ms clock at init [O: chunk_007 line 139] |
| cvars | `SV_AddServerCvars` 0x08093adc, then `NET_Init` 0x080d5a5c | `net_noudp`; if 0 and no socket, `NET_OpenIP` |
| socket | `NET_OpenIP` 0x080d597e -> `NET_IPSocket` 0x080d5aa8 | tries `net_port`..`net_port+9`; on success writes the chosen port back into `net_port`; `NET_GetLocalAddress` fills `localIP[]` |
| PB | `PB_GlobalCtorEntry` 0x0813c632 (in `.ctors`, slot 4 of 0x0817d00c) -> `PB_Construct` + `__cxa_atexit(PB_AtexitShutdown)` | [O: .ctors words `0x813c632`] |
| shutdown | `SV_Shutdown` 0x080942f8 -> `SV_MasterShutdown` 0x08097032 (heartbeat name `"flatline"`) | `NET_CloseIP` exists (0x080d5c7e) |

### 2.2 Receive path

```
Com_Frame (0x080627f4)
  -> Com_EventLoop 0x0806182a
       -> Com_GetEvent 0x0806179c -> Sys_GetEvent 0x080d484e
            -> Sys_GetPacket 0x080d5330 (recvfrom, 0x20000 byte buffer sys_packetReceived 0x085e6880)
            -> Sys_QueEvent(type 5, ptr = netadr(0x14) + payload)
       -> type 5: if com_sv_running -> SV_ReadPackets_080960e2(from,msg)   [0x080960e2, in chunk 022]
       -> type 0 (empty): drains loopback queue 1 via NET_PollLoopPacket 0x0806c700 -> same handler
SV_ReadPackets_080960e2:
   cursize<4 or first dword != -1  -> in-band client packet (below)
   else                              -> SV_ConnectionlessPacket 0x0809594e
```

[O] The socket is polled once per `Sys_GetEvent` call; `Com_EventLoop` loops until the empty event, so all pending datagrams are drained each frame. `NET_Sleep(0)` (select with 0 timeout on stdin and `ip_socket`) is used only in the frame-throttle loop (`Com_Frame`, minMsec) and `NET_Sleep(0xfa)` in `SV_SpawnServer` after sending `loadingnewmap` [O: chunk_007:300, chunk_021:772].

In-band path (`SV_ReadPackets_080960e2`) [O]:
1. `MSG_BeginReading`, `MSG_ReadLong` (sequence), `MSG_ReadShort` (qport).
2. Linear scan of `svs.clients[0..sv_maxclients)` for `state != 0 && NET_CompareBaseAdr(from, cl->netchan.remoteAddress) && cl->netchan.qport == qport&0xffff`. On match: if the port differs, print `"SV_ReadPackets_080960e2: fixing up a translated port"` (the original text is `SV_ReadPackets: ...` - merge renamed the string in the reconstruction only; verify against binary before reproducing the exact string) and overwrite the stored port.
3. `Netchan_Process` (0x0806be8a); on 0 return -> drop.
4. Read `serverId` (byte, stored at client+0xae710), `messageAcknowledge` (long, client+0x20818; negative -> print "Invalid reliableAcknowledge message ..." and return), `reliableAcknowledge` (long, client+0x20810; if `reliableSequence - reliableAcknowledge > 0x7f` clamp to `reliableSequence` and return).
5. `SV_Netchan_Decode(cl, data+readcount, remaining)` (0x08097562) XOR-decodes the rest in place.
6. state 1 (zombie) -> return; else `lastPacketTime = svs.time`, `SV_ExecuteClientMessage` (0x0809114e), then a global (0x0860b420) is zeroed.
7. No match: `NET_OutOfBandPrint(1, from, "disconnect")`.

`SV_ExecuteClientMessage` [O: chunk_020 + objdump callers]: `Huff_DecompressBytes(data+readcount -> 0x20000 temp)`; then if `cl->0xae710 (serverId) == svs.serverId (0x08423008)` or a download is active: loop `MSG_ReadBits(3)`: `2` = client command (`SV_ClientCommand`), `0` = usermove with delta, `1` = usermove no-delta, `3` = EOF/no-op; anything else prints `"WARNING: bad command byte %i for client %i"`. If serverId mismatches but high nibble matches (`& 0xf0`) and state is PRIMED, the client enters the world; else if `gamestateSeq < messageAcknowledge` the gamestate is resent ("dropped gamestate, resending").

### 2.3 Connectionless commands (SV_ConnectionlessPacket 0x0809594e) [O]

`MSG_BeginReading; MSG_ReadLong; SV_NetProfile_RecordOOB(cursize)`. If bytes `data+4` start with `pb_` (3 chars, case-insensitive): find the client by address+port (state != 0), and unless `data[7]` is `'C'`, `'1'` or `'J'`, call `PB_DispatchEvent3(0x0d, clientIndex_or_-1, cursize-4, data+4)`. Otherwise `MSG_ReadStringLine` (maps `%` to `.`, stops at `\n`), tokenize, optional log `"SV packet %s : %s"` when `sv_packet_info`, then `strcmp` chain:

| Command (argv0, case-sensitive `strcmp`) | Handler | Reply |
|---|---|---|
| `getstatus` | `SV_NoteAuthorizeServerPacket`, `SV_StatusResponse` 0x08094c84 | `statusResponse\n<info>\n<player lines>` |
| `getinfo` | `SV_NoteAuthorizeServerPacket`, `SV_InfoResponse` 0x0809537c | `infoResponse\n<info>` |
| `getchallenge` | `SV_NoteAuthorizeServerPacket`, `SV_RequestIpAuthorize` 0x0808d18e | `challengeResponse %i` (immediately for LAN unless `net_lanauthorize`; else after master `ipAuthorize` reply) |
| `connect` | `PB_InvokeCb158(addrString, msg->data)` then `SV_DirectConnect` 0x0808e2aa | none directly (client proceeds by in-band traffic) or `error\n<EXE_...>` |
| `ipAuthorize` | `SV_NoteAuthorizeServerPacket`, `SV_AuthorizeIpPacket` 0x0808da2a | `challengeResponse %i`, `error\n...`, `needcdkey` |
| `rcon` | `SVC_RemoteCommand` 0x08097188 | `print\n...` pieces |
| `v` | function at 0x08094b56 (symbol `SV_ReadPackets`, **misnamed**, section 10) | voice relay |
| `disconnect` | `Q_stricmp` result discarded | none [O: no-op] |

Heartbeat/outbound OOBs: `heartbeat COD-2\n` (`SV_MasterHeartbeat` 0x08096ed6, called from `SV_Frame` with name `"COD-2"`), `gameCompleteStatus\n...` (0x08096fd0), unsolicited `statusResponse` to the master every 600000 ms [O].

### 2.4 Send path

```
SV_Frame 0x080969b0 -> SV_SendClientMessages 0x0809bcce
   per client (state != 0, nextSnapshotTime <= svs.time):
     if netchan.unsentFragments == 0: SV_SendClientSnapshot 0x0809adea ; SV_SendClientVoiceData 0x0809c0a2
     else: nextSnapshotTime = now + SV_RateMsec(remaining) ; SV_Netchan_TransmitNextFragment 0x08097610 -> Netchan_TransmitNextFragment 0x0806ba4c
SV_SendClientSnapshot:
   MSG_Init(0x20000); MSG_WriteLong(lastClientCommandSeq @+0x20840)
   [state 4 or 1] SV_UpdateServerCommandsToClient 0x080988b2 ; SV_WriteSnapshotToClient 0x080985c8
   [state != 1]   SV_WriteDownloadToClient 0x0808fdc2
   MSG_WriteByte(7)  (svc EOF)
   overflow -> retry with SV_UpdateServerCommandsToClientLimited; still overflowed -> "disconnect" OOB + SV_DropClient("EXE_SERVERMESSAGEOVERFLOW")
   SV_SendMessageToClient 0x0809aba2:
       copy first 4 bytes raw; Huff_CompressBytes(rest) ; record frames[outgoingSequence&31].{messageSize,messageSent,messageAcked=-1}
       SV_Netchan_Transmit 0x08097626: SV_Netchan_Encode(data+4, len-4) ; Netchan_Transmit 0x0806bc6c ; NET_SendPacket 0x0806c7ec -> Sys_SendPacket 0x080d54a4 (sendto)
       LAN/loopback client -> nextSnapshotTime = svs.time-1 ; else delay = max(SV_RateMsec(len), snapMsec) (rateDelayed flag)
```

All OOB sends funnel through `NET_OutOfBandPrint` 0x0806c8cc (35 call sites; sock is always 1 on the server) which prefixes `FF FF FF FF`, and `SV_NetProfile_RecordOOB` when sock==1. Voice goes through `NET_OutOfBandVoice` 0x0806cc7a; PB uses `NET_OutOfBandSend` 0x0806cb5c (via `SV_SendToClients` 0x0809602e) and raw `Sys_SendPacketToHost` 0x080d55c4 (via PB callback 0x0813c5a8) [O].

---------------------------------------------------------------------

## 3. Global state

| Address | Name (proposed/used) | Layout / meaning | Tag |
|---|---|---|---|
| 0x08608e34 | `ip_socket` | int, 0 = none | O |
| 0x08608e38 | `ipx_socket` | never opened | O |
| 0x08608e3c | `net_noudp_cvar` | | O |
| 0x08608e40 / 0x08608e60 | `numIP` / `localIP[16][4]` | filled by `NET_GetLocalAddress`; quirk: `numIP` is incremented **before** the store, so slot 0 is never filled and a 15th address overflows into index 16 | O (chunk_038 comment; asm not re-verified) |
| 0x0860aee0 | `sys_ipstring[0x40]` | static buffer of `NET_BaseAdrToString` | O |
| 0x085e5060 / 0x085e6860 / 0x085e6864 | `eventQue[256]` (0x18 each) / head / tail | `Sys_QueEvent` ring; overflow drops oldest | O |
| 0x085e6880 | `sys_packetReceived[0x20000]` | recv buffer; max datagram 131071 (an exactly full read is treated as `"Oversize packet from %s"`) | O |
| 0x082055a0 | `g_082055a0[0x40]` | static buffer of `NET_AdrToString` (not thread safe) | O |
| 0x081e9c60 | Huffman `huff_s` {compressor 0x701c, decompressor 0x701c} = 0xe038 | decompressor half at 0x081f0c7c | O |
| 0x081e9544 | Huffman global bit cursor used by all `Huff_*Bit` helpers | not re-entrant | O |
| 0x081f7c98 | one-shot guard for `MSG_InitHuffmanOnce` | set to 1 on first `MSG_Init` | O |
| 0x08142d00 | `huff_symbol_counts[256]` | **fixed table**, sum 1,053,340, min 740 (every symbol present), `[0]=250315` | O (read from binary) |
| 0x081f7ca0 / 0x081f80a0 / 0x081fa0a0 | static buffers of `MSG_ReadString` (1024) / `MSG_ReadBigString` (8192) / `MSG_ReadStringLine` (1024) | returned pointers are overwritten by the next call of the same function | O |
| 0x0817d880 | `(1<<n)-1` mask table (32 dwords) | used by keyed readers | O |
| `net_loopbacks[]` (address not recovered; `index*0x5808`) | `loopback_s` 16 x 0x580 slots + get/send | queue index 1 = server | I |
| 0x081fa518 | `net_qport` (u16) | | O (chunk_009 types comment) |
| `net_profile_mode` | 0 off / 1 client1 / 2 server | `NetProfile_Sync` | O |
| 0x08423080 | `svs` (size 0x1d10c) | `svs.clients` pointer (`g_svs_clients`), `svs.time` (`g_svs_time`) | I (chunk_021 comment) |
| 0x08423008 | server id / message sequence compared with `client+0xae710` | | I |
| 0x084230dc | `challenge_s g_084230dc[1024]` (0x74 each) | challenge/authorize table | O (0x400 loop bound, 0x74 memset) |
| 0x0844010c | `tempban_s[16]` (8 each) | | O |
| 0x084400f0 | authorize-server `netadr_t` | resolved lazily to `cod2master.activision.com`, port 0x50dc = **20700** | O |
| 0x0849fbe0 | master-heartbeat `netadr_t` | `SV_ResolveMasterAddress`, port forced to 0x50e6 = **20710** | O |
| 0x08440104 | last time an authorize-server packet was seen | | I |
| 0x084400dc | rcon redirect target `netadr_t` | | O |
| 0x08440108 | OOB `netprof_s*` | | O |
| 0x0844020c | checksum feed (sent in gamestate, mixed into usercmd key) | | O |
| 0x0849f68c.. | per-frame sent/unsent byte counters and 20-slot window used by `sv_showAverageBPS` | | O |
| 0x08a6f2c0 | `punkbuster_s` global (0x170 bytes) | magic 0x357afe24; name `"PunkBuster Server"` | O |
| 0x0817d9e0 | client command table (12 `{name,fn}`) | userinfo, disconnect, cp(`SV_VerifyPaks_f`), vdr, download, nextdl, stopdl, donedl, retransdl, wwwdl, muteplayer, unmuteplayer | O (read from binary) |
| 0x08440618 | `char *configstrings[0x800]` | | O |
| 0x08442618 | `sv_entity_s[1024]` (0x174); baseline `entityState` at +4 | | O |

---------------------------------------------------------------------

## 4. Types shared across the subsystem (see `network.structs.json`)

Key size facts verified by cross-chunk consistency [O unless noted]:
* `msg_s` 0x18 (`MSG_Init` memset 0x18).
* `netadr_s` 0x14 (type, ip[4], port(u16, network order), ipx[10]); passed **by value** as five stack dwords.
* `netchan_s` 0x40040; embedded in `client_s` at +0x6e6c4; `client_s` stride 0xb1064; the next client field (`guid`) sits at exactly netchan+0x40040 = 0xae704.
* `entityState` 0xf0 = 59*4+4 (`MSG_ReadDeltaStruct` memcpy length); shared/cached entity 0x114 = 68*4+4; clientState 0x5c = 22*4+4; playerState 0x26a8; hudelem 0x80; objective 0x1c.

---------------------------------------------------------------------

## 5. Data formats and protocols

### 5.1 Datagram framing

* Connectionless: bytes `FF FF FF FF` + text (no NUL counted on send: `NET_OutOfBandPrint` sends `strlen(buf)` which stops at the first NUL after the prefix; the terminating NUL is not sent) [O]. Receive side reads the first dword (`MSG_ReadLong`) then a `\n`/NUL-terminated line.
* `NET_OutOfBandPrint` max text: `strlen+1 < 0x1fffd`, else `"OOB Packet is %i bytes - too large to send"` (debug print) [O]. `NET_OutOfBandSend`/`Voice`: payload `< 0x20001` [O].
* In-band client->server: `[seq:4 LE][qport:2][fragment header?][payload]`; in-band server->client: `[seq:4][fragment header?][payload]` (no qport) [O: `Netchan_Transmit` writes qport only when `sock < 1`; `Netchan_Process` skips a short only when `sock == 1`]. Sockets: 0 = client1, 1 = server (`net_sock_names`) [O].
* Fragment bit: top bit of seq (`0x80000000`). Fragment header: `fragStart:i32`, `fragLen:i16`. Fragment size 0x514 (1300). Unfragmented if `length < 0x514`; reject if `length > 0x20000` (`Com_Error(1,"Netchan_Transmit: length = %i")`). Out-buffer for a fragment datagram is 0x578 (`MSG_Init(…,0x580,0x578)`). Last fragment is the first with `fragLen != 0x514` (so an exact multiple of 1300 emits a final empty fragment). `Netchan_Process` accepts a fragment only if `fragStart == fragmentLength`, `fragLen` sane, total `<= 0x20000`; on completion it rewrites `msg->data[0..3]` with the sequence and copies the reassembled payload after it, then re-reads the long [O].
* Sequence handling: `seq <= incomingSequence` -> drop ("Out of order packet"); `dropped = seq - incomingSequence - 1`; then `incomingSequence = seq` [O]. A fragment series is not committed to `incomingSequence` until complete.
* **No ack field** in the netchan header [O]; acknowledgement is carried by the `messageAcknowledge`/`reliableAcknowledge` payload fields (client->server) and the leading `lastClientCommand` long (server->client).

### 5.2 `msg_t` bit/byte buffer [O]

Fields: `overflowed@0, data@4, maxsize@8, cursize@0xc, readcount@0x10, bit@0x14`. This is **not** the Q3 Huffman-per-bit design:
* Raw writers (`WriteByte/Short/Long/Data/String`) append at `cursize` as whole bytes, little-endian, length checked; overflow sets `overflowed=1`.
* `MSG_WriteBits(value,bits)` writes LSB-first into a bit cursor `msg->bit`; when `(bit&7)==0` it allocates a new byte at `cursize` and sets `bit = cursize<<3`. `MSG_WriteBit0/1` same. Overflow test in `WriteBits` is a fixed "fewer than 4 bytes free". Raw byte writes after partial-byte bit writes append a **new** byte after the partial one and do not move `bit`; the next `WriteBits` continues in the earlier partial byte. Readers mirror this (`readcount` advances by whole bytes; `MSG_ReadBits` re-syncs `bit = readcount<<3` when `(bit&7)==0`). A native port must reproduce this exact hybrid cursor logic, not a conventional bit stream.
* `MSG_ReadBits/ReadBit` return -1 and set overflow on exhaustion; `MSG_ReadByte/Short/Long` return -1, `ReadData` fills 0xFF.
* Strings: `MSG_WriteString` (limit `< 0x400`, else prints `"MSG_WriteString: MAX_STRING_CHARS"` and writes an empty string), `WriteBigString` (`< 0x2000`, `"MSG_WriteString: BIG_INFO_STRING"`); both pass every byte through `Q_NormalizeApostrophe`. Readers: `ReadString` (1024, apostrophe-normalised, `-1` -> NUL), `ReadBigString` and `ReadStringLine` additionally map `%` -> `.`; `ReadStringLine` ends at `\n` too.
* Angles: `MSG_WriteAngle16 = (int)(deg*182.04445f)&0xffff` (const 0x0814142c), `ReadAngle16 = (short)v * 0.0054931640625f` (0x08141430) - note `ReadShort` is sign-extended so angles > 180 read back negative [O].
* Endianness wrappers `Net_SignExtend16` (0x0806b1b8), `Net_Identity32` (0x0806b1cc), `Netchan_SequenceToWire` (0x0806cf00) are identity on this little-endian build [O]; `BigLong64` is used for the 64-bit read/write (never called: 0 call sites for WriteInt64/ReadInt64) [O].

### 5.3 Keyed bit helpers and usercmd delta [O; client-side writer has no call sites]

`MSG_WriteDeltaKey(key,old,new,bits)`: bit 0 if equal else bit 1 + `key^new` in `bits` bits; `ReadDeltaKey` uses `mask[bits]&key`. `MSG_WriteDeltaUsercmdKey` (0x08068bc0) has **zero callers** in this binary (client code left in); the reader `MSG_ReadDeltaUsercmdKey` (0x08068f9e) is used by `SV_UserMove`. Wire layout read by the server: bit (0 -> full `serverTime` long, 1 -> byte delta); keyed bit "something changed"; further keyed bits select (A) angles0/1 + move class path or (B) full path (buttons>>1 in 18 bits, weapon 7 bits, offhand 7 bits, angles2). Move axes are quantised to a 4-bit class (`UserCmd_MoveClass` 0x08068b20: >10 -> +127 flag, < -10 -> -127 flag; `MoveFromClass` expands to +127/-127/0) [O].

`SV_UserMove` key = `checksumFeed(0x0844020c) ^ messageAcknowledge ^ Com_HashKey(reliableCommands[reliableAcknowledge & 0x7f].text, 32)`; limits: `cmdCount` 1..32; refuses when `reliableSequence - reliableAcknowledge >= 0x80`; weapon bytes patched from playerstate (`ps+0xd4`, `ps+0xd0`) when `BG_PlayerHasWeapon` fails; applies only commands with `serverTime` newer than `lastUsercmd`; with `sv_pure` and `pureStatus == 0` drops with `EXE_CANNOTVALIDATEPURECLIENT` [O].

### 5.4 netField tables and delta structs [O tables, I wire interpretation]

Table entry `{name, offset, bits}`. `bits`: `0` = float (value `(int)v` truncated by x87 `fistp` with RC=chop; if integral and `-0x1000 <= v < 0x1000`: bit 0, 5 low bits, byte `(v+0x1000)>>5`; else bit 1 + raw 32-bit float bits); `-99` = float, same idea with `+0x200`, 2 low bits + byte; `-100` = float, zero flag then `MSG_WriteAngle16`; `N>0` / `N<0` = N-bit unsigned/signed integer, written as `N&7` bits then whole bytes, LSB first, sign-extended on read. Before any value: 1 changed bit. Full tables are in the appendix.

Differences found in the code vs. the binary's asm (see section 10): the changed test is an integer dword compare (`cmp eax,[edx]`), the zero test for integer fields is an integer compare, and the `bits==0` path zero-flag exists in the entity coder (0x080692dc) but the playerState coder (0x0806a200) has no zero flag [I from reconstruction; changed-test and chop mode O from asm].

Struct-level coders [O]:
* `MSG_WriteDeltaStruct(from,to,force,numFields,numBits,fields,print)` (0x080696bc): `to==NULL` -> (`[bit1 if print]`, `number`(numBits), bit1 = remove). Else compute `lastChanged` = 1 + index of last differing dword; none and !force -> nothing; none and force -> `[bit1] number, 0, 0`; else `[bit1] number, 0, 1, byte lastChanged, then fields[0..lastChanged)` each `MSG_WriteDeltaField`. Reader `MSG_ReadDeltaStruct` (0x08069d4a): bit1 -> removed; bit0 then bit0 -> copy `numFields*4+4` bytes from baseline; else `byte count` (`> numFields` -> overflow) + fields, rest copied from baseline.
* `MSG_WriteDeltaEntity` = struct coder (59 fields, 10 bits, print 0); entity numbers end with sentinel `0x3ff` in 10 bits (`SV_EmitPacketEntities`). `MSG_WriteDeltaSharedEntity` = 68 fields, 10 bits (used only for the internal snapshot archive and baselines inside it). `MSG_WriteDeltaPlayerstate` (0x080698e8, **actually clientState**) = 22 fields, 6 bits, print 1 (each entry preceded by bit 1, list terminated by `MSG_WriteBit0` in `SV_EmitPacketClients`), `from==NULL` uses a zero 0x5c record.
* `MSG_WriteDeltaPlayerState` (0x0806a200, size 0x26a8): `byte lastChanged` over the 105-entry table (per field: changed bit + payload as above), then: 6 dwords at +0x12c with a 6-bit mask (shorts/bits/byte per mask bit: +0x12c,+0x130,+0x134 short; +0x138 6 bits; +0x13c short; +0x140 byte); four 16-dword groups at +0x144 with an outer "any" bit, per group bit, 16-bit mask, shorts for set bits; four groups at +0x344 with per-group bit only; 16 objective records of 0x1c at +0x5e4 (3-bit state word + `MSG_WriteDeltaFieldGroup` over the 6 objective fields); finally one bit then two hudelem arrays (31 x 0x80) at +0x1728 and +0x7a8 via `MSG_WriteDeltaRecordArray` (5-bit count, per record 5-bit last index, then fields of `hudelemFields`). Arrays are scanned until a record whose first dword is 0 (`type == 0`).
* Snapshot body (`SV_WriteSnapshotToClient` 0x080985c8): `byte 6`, `long svs.time`, `byte deltaNum`, `byte snapFlags` (`g_08423088` | 1 if `rateDelayed` | 2 if not active), playerState delta (vs old frame or NULL -> zero base), entity list, clientState list, then `sv_padPackets` zero bytes. Delta frame valid only if `outgoingSequence - deltaMessage < 0x1d` (29) and the old frame's entities are still in the ring; else full snapshot [O].
* Gamestate (`SV_SendClientGameState` 0x0808f302): `long lastClientCommandSeq`, queued server commands, `byte 1`, `long reliableSequence`, per non-empty configstring (0x800 slots) `byte 2, short idx, bigstring`, per linked entity `byte 3, MSG_WriteDeltaEntity(zero baseline, force)`, `byte 7`, `long clientNum`, `long checksumFeed`, `byte 7` (two EOF bytes in total) [O].
* Server->client opcodes seen: 1 gamestate, 2 configstring, 3 baseline, 4 server command (`byte 4, long seq, string`), 5 download, 6 snapshot, 7 EOF. Client->server 3-bit codes: 0 move(delta), 1 move(no delta), 2 clientCommand (`long seq, string`), 3 EOF [O].

### 5.5 Obfuscation and compression [O]

* `SV_Netchan_Encode` (0x080974d8, server->client): `key = (u8)(outgoingSequence ^ challenge)`; for each byte i: if the cursor char in `lastClientCommand` (client+0x20844) is NUL reset to 0; `key ^= ch << (i & 1)`; advance cursor; `byte ^= key`.
* `SV_Netchan_Decode` (0x08097562, client->server): `key = (u8)(challenge ^ serverId ^ messageAcknowledge)`; string = `reliableCommands[reliableAcknowledge & 0x7f].command`; same loop.
* Server->client message = `[raw long lastClientCommandSeq][Huffman(payload)]` then encode skips the first 4 bytes. Client->server = `[serverId byte][messageAck long][reliableAck long][XOR(Huffman(payload))]`.
* Huffman: `huff_s` with two `huffman_s` (0x701c each: blocNode, blocPtrs, tree, lhead, ltail, loc[257], freelist, nodeList[768], nodePtrs[768]); `MSG_InitHuffmanTables` 0x0806b12c: `Huff_Init` then for sym 0..255, `counts[sym]` times `Huff_addRef` on **both** halves. `Huff_CompressBytes` 0x08067f4e uses `Huff_offsetTransmitKnown` (path from root; never emits NYT; no adaptation), `Huff_DecompressBytes` 0x08067fa2 decodes until `compressedLen*8` bits consumed (no adaptation; trailing pad bits < 8 can decode as extra symbols, last-symbol behaviour must be reproduced bit-exactly). A native port can either re-run the exact adaptive build (1.05M `addRef` steps, deterministic, incl. `Huff_swap*`/`Huff_increment` tie-breaking) or dump the 256 code words once; the tree is a pure function of the 256-entry table at 0x08142d00 [O: build procedure; I: equivalence claim].
* Adaptive per-message `Huff_Compress`/`Huff_Decompress` (chunk 100, 0x08066cf8 / 0x08066a4e: 16-bit BE length + adaptive code) have **no direct call sites** in the binary [O: `objdump` search], so they are presumed dead (client leftovers).
* MD4: `Com_BlockChecksum` (0x08067b02) / `Com_BlockChecksumKey` (0x08067b5e) = XOR-fold of the four MD4 digest words (key mixed as a leading 4-byte block). Used 3x / 1x (pak checksums, outside this subsystem). MD5 (chunk 061: `MD5_InitSeeded` 0x0813df1c with state `IV + seed*{0xb,0x47,0x25,0x61}`, `MD5_Update`, `MD5_Final`) is only reachable from PB-library glue; no external caller found [O].

### 5.6 Reliable server commands (client+0x40c ring of 128 x 0x408) [O]

Fields: `reliableSequence@0x2080c`, `reliableAcknowledge@0x20810`, `reliableSent@0x20814` (also called `reliableSearchCursor`/`unk20814`). Entry = `{text[0x400], time, flags}`. `SV_QueueReliableCommand` 0x08094750: skipped for bots (`+0xae70c != 0`); if `(reliableSequence - reliableAcknowledge) < 0x40 && state == 4` queue, else compact (`SV_CompactReliableCommands` drops entries with `flags==0`) and queue only if `flags != 0`; duplicate suppression by `SV_FindReliableCommand` (same first char and identical text, and per-type rules: for `C D a b o p q r t` same type collapses; for `d v` `SV_TokenMatch`); overflow at exactly 0x81 pending: dumps `"===== pending server commands ====="`, sends OOB `disconnect`, defers drop `EXE_SERVERCOMMANDOVERFLOW`. Text is sanitised by `CopyReliableCommand` (`%` -> `.`, apostrophe normalised, warnings when >= 0x400 or empty). `SV_SendServerCommand(cl=NULL)` broadcasts to clients with `state > 2` only.

### 5.7 Download protocol [O]

Client commands `download <name>`, `nextdl <block>`, `stopdl`, `donedl`, `retransdl <block>`, `wwwdl ack|bbl8r|done|fail|chkfail`. Server sends `byte 5, short block, [long totalSize if block 0], short len, data` with 0x800-byte blocks, 8-block window; a zero-length block marks EOF; denial = `byte 5, short 0, long -1, string(EXE_... \x15 name)`; redirect = `byte 5, short -1, string url, long size, long disconnectFlag`. Blocks per frame = `(rate*snapMsec/1000 + 0x800) >> 11`; `sv_maxRate` floor 1000; resend a stalled block after 1000 ms; denies IW paks via `FS_iwIwd(name,"main")`.

### 5.8 Voice [O]

Outbound `NET_OutOfBandVoice(1, addr, buf)` where buf = `"v\0"` + count byte + per packet {talker byte, length byte, data}: queue 40 packets x 0x105 per client, `sv_voice` gate, only to `state == 4`; inbound `v` OOB = `qport:short` then per packet (count byte; length is **short** in `SV_ParseVoicePackets`, **byte** in `SV_ParseVoicePacketsLocal`), valid 1..0x100.

### 5.9 Master / authorize / rcon / status [O]

* Master host `cod2master.activision.com`; heartbeat every 180000 ms (only if `com_dedicated == 2`), unsolicited `statusResponse` to master every 600000 ms, `getIpAuthorize <challenge> <a.b.c.d> "<fs_game>" <allowAnonymous> [PB "<key>"]` to the auth address; replies `ipAuthorize <challenge> accept|deny|demo|... <keyArg> <guid> <key>`; challenge table 1024 entries, victim = oldest `firstTime`, new challenge = `rand()<<16 ^ rand() ^ svs.time`; auth timeout rule: if `svs.time - lastAuthReply > 1,200,000` and `svs.time - createdTime > 7000` and sender isn't the master -> answer `challengeResponse` anyway ("authorize server timed out"). GUID bans checked in `SV_AuthorizeIpPacket` (perm via `SV_IsGuidPermBanned`, temp via 16-slot `SV_IsTempBannedGuid`).
* `SV_DirectConnect`: protocol check -> `error\nEXE_SERVER_IS_DIFFERENT_VER\x151.3`; reconnect throttle (`sv_reconnectlimit` s) per address+(qport or port); challenge lookup (non-local); ping gate `sv_minPing/sv_maxPing` (non-LAN); `PB_InvokeCb15c(addr, cl_punkbuster, keyA)` may veto with `error\n...`; reuse slot on same address/qport else first free slot at/after `sv_privateClients` (0 if `password == sv_privatePassword`); `EXE_SERVERISFULL`; `memset(client, 0, 0xB1064)`; `Netchan_Setup(1, ...)`; `ClientConnect`; state CS_CONNECTED; heartbeat when connected count is 1 or maxclients.
* `rcon`: global 500 ms throttle (silent drop), compares `rcon_password` exactly, logs `"Bad rcon from %s:\n%s\n"` / `"Rcon from %s:\n%s\n"`, captures console output via `Com_BeginRedirect` and `SV_FlushRedirect` (`print\n` + 0x50e-byte slices); re-quotes argv[2..] into a 0x400 buffer; `pb_sv_*` commands also trigger `PB_QueryServerEventNeg1`.
* `statusResponse` info string = `Dvar_InfoString(0x404)` + `challenge`, `pswrd`, `mod`; `infoResponse` includes `protocol 118, hostname, mapname, clients, sv_maxclients (open slots), gametype, pure, minPing, maxPing, game, sv_allowAnonymous, con_disabled, pswrd, ff, kc, hw=1, mod, voice, pb`.

### 5.10 Net profiling [O]

`netprof_s` 0x5e0 = two `netprof_half_s` (0x2f0): 60 x {time,bytes,fragmented} ring, sampleIndex, bytesPerSecond, lastUpdateTime, numPackets, numFragmented, percentFragmented, max, min. Allocated by `NetProfile_Sync` only when `net_profile != 0`; `NET_DumpProfile_f` / `SV_PrintNetworkProfile` 0x080977c6 prints the table; per-packet traces with `net_showprofile & 2`.

---------------------------------------------------------------------

## 6. Dvars and commands registered

[O unless noted] flags are raw hex.

| Registered by | Dvar / command | Default / range / flags |
|---|---|---|
| `NET_InitCvars` 0x0806b8be | `showpackets` (bool, 0x1000), `showdrop` (bool), `packetDebug` (bool), `net_profile` (int 0..2), `net_showprofile` (int 0..3), `net_lanauthorize` (bool); command `net_dumpprofile` | defaults NULL string/0 |
| `NET_Init` 0x080d5a5c | `net_noudp` bool 0, 0x1010 | |
| `NET_OpenIP` 0x080d597e | `net_ip` "localhost" 0x1010; `net_port` 28960 (0x7120), 0..999999, 0x1010 | `"localhost"`/empty = bind INADDR_ANY |
| `SV_AddServerCvars` 0x08093adc | `protocol` 118 (min=max=118, 0x1044); `sv_hostname` "CoD2Host" 0x1005; `sv_maxclients` 20, 1..64, 0x1025; `sv_privateClients` 0, 0..64; `sv_punkbuster` bool 0, 0x1015; `sv_maxRate` 0..25000; `sv_minPing`/`sv_maxPing` 0..999; `sv_floodProtect` 1; `sv_allowAnonymous`; `sv_showCommands`; `sv_disableClientConsole`; `sv_voice` 0 (0x100d); `sv_voiceQuality` 1, 0..9; `sv_pure` 1; `sv_iwds`, `sv_iwdNames`, `sv_referencedIwds`, `sv_referencedIwdNames` (0x1048); `rcon_password`; `sv_privatePassword`; `sv_fps` 20, 10..1000; `sv_timeout` 240, 0..1800; `sv_zombietime` 2, 0..1800; `sv_allowDownload` 1; `sv_reconnectlimit` 3, 0..1800; `sv_padPackets` 0; `sv_allowedClan1/2`; `sv_packet_info`; `sv_showAverageBPS`; `sv_kickBanTime` float 300.0 (0..3600); `sv_debugRate`; `sv_debugReliableCmds`; `sv_wwwDownload`; `sv_wwwBaseURL`; `sv_wwwDlDisconnected`; also `sv_serverid`, `sv_keywords`, `mapname`, `g_gametype`, `sv_mapRotation*`, `nextmap`, `sv_cheats`, `com_expectedHunkUsage` | see chunk_021:891-945 for exact values/flags |
| other reads | `g_password`, `fs_restrict`, `fs_game`, `scr_friendlyfire`, `scr_killcam`, `sv_allowAnonymous` | |
| Server operator commands (chunk 019/101, not re-derived) | `status`, `kick`, `clientkick`, `onlykick`, `ban*`/`tempban*`/`unban*`, `heartbeat`, `serverinfo`, `systeminfo`, `dumpuser`, `map`, `map_restart`, `say`/`tell`, ... | `SV_AddOperatorCommands` 0x0808cca6 |
| Client commands (table 0x0817d9e0) | userinfo, disconnect, cp, vdr, download, nextdl, stopdl, donedl, retransdl, wwwdl, muteplayer, unmuteplayer | |

---------------------------------------------------------------------

## 7. Inter-subsystem dependencies

* Dvar/Cmd: `Dvar_Register*`, `Cmd_Argv_alt`/`Cmd_TokenizeString_alt` (second tokenizer used for network text; `Cmd_*_alt` duplicates are separate functions), `Info_*` (infostring), `va`, `Com_sprintf`, `Com_Printf/DPrintf/Error`, `Com_BeginRedirect`.
* Memory: `Com_PoolAllocInto2/Com_PoolTouchAt/Com_PoolRestoreFrom2` (stack-like scratch pool used for every 0x20000 network buffer).
* Game module: `ClientConnect`, `ClientBegin`, `ClientThink`, `ClientUserinfoChanged`, `ClientCommand`, `G_SayToEligibleClients`, `BG_PlayerHasWeapon`, `Client_GetUnk26b8` (score).
* Filesystem: `FS_LoadedIwd*`, `FS_ReferencedIwd*`, `FS_iwIwd`, `FS_SV_FOpenFileRead`, `FS_Read` for pure/downloads; `FS_SetIwdChecksums` (0x08065f94) lives in the network chunk but is filesystem code (iwd sum/name parser, `Com_Error(1,"iwd sum/name mismatch")`, up to 0x400 entries).
* Script/clients: `ScrVar_AllocArray` handle stored at client+0xae708; snapshot code reads playerState/entityState from game memory.
* Loader/Sys: `Sys_Milliseconds`, `Sys_IsLANAddress`, event queue.
* PB: `libdl` (`dlopen/dlsym/dlclose`), `chmod/mkdir/rename/remove`.

---------------------------------------------------------------------

## 8. Behaviours a native port must reproduce

1. [O] Hybrid byte/bit `msg_t` semantics (5.2), including the fixed "4 bytes free" overflow test in `WriteBits` and `-1`/0xFF fill behaviour on read overflow.
2. [O] Exact framing (5.1): 4-byte LE seq, qport only on client->server, fragment threshold 1300, reassembly rules, no ack.
3. [O] XOR keystreams (5.5) with the *cross-keying* (server encode uses client's last command string and outgoing sequence; decode uses last acked server command and serverId/messageAck).
4. [O/I] Static Huffman from the 256-entry table; compressor never emits NYT; decoder consumption rule `bitOffset < len*8`.
5. [O] Snapshot/gamestate structure and opcode values (5.4), 10-bit entity numbers with `0x3ff` terminator, 6-bit client numbers, delta ring depth 32 (`& 0x1f`, delta valid if < 0x1d old), `snaps` clamp 1..30, `rate` clamp 1000..90000 (default 5000, LAN 99999), `SV_RateMsec = (min(len,1500)*1000+48000)/rate`.
6. [O] netField tables and their *order* (wire order = table order), the playerState sub-groups and hudelem/objective encodings, and the integer-vs-float treatment per `bits` value.
7. [O] Connection state machine strings and error codes (`EXE_*`), challenge table semantics, `SV_DirectConnect` ordering of checks, reconnect throttle, private-slot logic, bot connect via `NA_BOT` (type 0) with port = bot counter.
8. [O] OOB command set and reply formats (2.3, 5.9), `FF FF FF FF` prefix, rcon 500 ms throttle, rcon output slicing.
9. [O] Reliable-command ring semantics (5.6) including de-dup/compaction and overflow disconnect.
10. [O] Download block protocol (5.7) incl. 0x800 block size and denial strings.
11. [O] Master heartbeat cadence/names (`COD-2`, `flatline`) and addresses/ports (20710 heartbeat target, 20700 authorize target). Whether a port should contact the Activision master at all is a product decision; behaviour is documented here.
12. [O] `NET_StringToAdr` default port 28960; `localhost` -> loopback type 2.
13. [I] PB: a port can run with PB disabled; `PB_Dispatch` returns 0 when `cmd_handler == NULL`; `sv_punkbuster 0` reports `pb=0`. The `pb_` OOB prefix check and event ids are listed in 2.3/section 9.
14. [O] Quirks worth keeping bit-compatible only if oracle parity is required: `numIP` off-by-one, `SV_Mute_f` accepts index 64 which writes the byte after `muteFlags[64]` (= `voiceFlag` at client+0xb1020), `NET_OutOfBandPrint` truncates at first NUL, `Netchan_Process` prints `Net_CurrentClientNum()+1` while `Netchan_TransmitNextFragment` prints it without +1 (always 0).

---------------------------------------------------------------------

## 9. PunkBuster and non-network `msg` items

### 9.1 PB object (`punkbuster_s` 0x170 at 0x08a6f2c0) [O unless noted]

`magic 0x357afe24; state_04; dll_sv/dll_cl/dll_ag handles (+8,+0xc,+0x10); name[32] "PunkBuster Server" (+0x14); pb_dir[256] (+0x34); dll_pending (+0x138); cmd_handler (+0x13c = PB_HandleCommand); cvar_query (+0x140 = 0x0813c43a); cb_144 (0x0813c524); cb_148 (0x0813c582); proc_sb (+0x14c, "sb" export); proc_sa (+0x150, "sa" export); cb_154 (0x0813c5a8); cb_158; cb_15c; cb_160; field_164; con_cap_buf (+0x168); con_cap_len (+0x16c)`.

* `PB_LoadServerDll` 0x0813ca88: path `fs_homepath/pb/` (via `PB_BuildPath`, falling back to `getcwd`), copies `pbsv.so`, `pbcl.so`, `pbag.so` from `fs_basepath/pb/` when missing; if `pbsvnew.so` exists rotates `pbsv.so` -> `pbsvold.so`, installs the new one; `dlopen(...,1)`; resolves `sa` and `sb`; errors `"PB Error: Server DLL Load Failure"`, `"PB Error: Server DLL Get Procedure Failure"`.
* `PB_Dispatch` 0x0813c7c8(event,a,b,c,d) lazily loads, calls `sb(pb, event, a, b, c, d)`; `PB_QueryServer` 0x0813c888 calls `sa(pb, event)`.
* Event ids seen: `0x0d` OOB `pb_` packet (a=client index or -1, b=len, c=data), `0x0f` joined text (**no callers**), `0x10` server init (`PB_ServerInit`), `0x33` (**no callers**), `0x71/0x72` (errors suppressed), `0x75` / `0x76` (sv_punkbuster on / off, sent at the end of `SV_SpawnServer`); query 0 each frame from `main`, query -1 after `pb_*` console commands.
* Engine -> PB callbacks (installed by `PB_InitHandlers` 0x0813c774; **these four bodies are not in any finished symbol chunk**, I disassembled them): `0x0813c43a` query(key, buf): `0x65` -> `SV_GetMaxClients` as decimal string; `0x66` -> `SV_GetClientInfo(atoi(buf), buf)` (0x68-byte record: name@0, string@0x21, address string@0x42; failure returns `"PB Error: Query Failed"`); `0x67` -> `Dvar_GetString`-by-name copied with `strncpy(...,0xff)` (used for `fs_basepath`/`fs_homepath`); `0x72` -> `SV_GetClientScoreText` (`"ping=%d score=%d"`); `0x0813c524` console print (`"%s: %s\n"`); `0x0813c582` `SV_SendToClients(len,data,clientNum)` -> OOB send to client(s) (`clientNum -1` = all in state>1); `0x0813c5a8` `Sys_SendPacketToHost(host, port, len, data)`.
* PB -> engine: `PB_HandleCommand` 0x0813c28c (`set_sv_punkbuster`, `ConCapBufLen`, `ConCapBuf`, `Cmd_Exec`, `DropClient`, `Cvar_Set`/`Dvar_Set`); `PB_ConsoleCapture` is called from `Com_PrintMessage` (chunk_006:1039).
* Connect hooks: `PB_InvokeCb158(addrString, packetData)` for `connect`; `PB_InvokeCb15c(addrString, cl_punkbuster, keyA)` returns an error string or NULL.
* Binary-only dependency: without `pbsv.so` every `PB_Dispatch` returns an error string; the server still runs (`PB_ServerInit` sets `sv_punkbuster 0` when `proc_sb == NULL`).

### 9.2 `MsgBuf_*` (0x080b5b06..) [O/I]

`msgbuf_s` {data, maxsize, cursor, warn_on_overflow(byte), overflowed(byte), transfer fn ptr}: a direction-tagged byte serialiser (`MsgBuf_InitRead`/`InitWrite`, `MsgBuf_WriteData`/`ReadData` which call `Com_Error(1,"Couldn't write %i bytes to %i-byte buffer ...")` when warn is set). No call sites for `InitRead/InitWrite` by direct call (function pointers stored); the only direct users of `MsgBuf_WriteData/ReadData` besides its own helpers are two call sites at 0x08123563/0x08123585 (game/save region). I classify it as **not network** (likely savegame/`G_` serialisation) [I].

---------------------------------------------------------------------

## 10. Naming problems found

| # | Address / name in symbols | Problem | Evidence |
|---|---|---|---|
| 1 | 0x08094b56 `SV_ReadPackets` (chunk 021) | This is the handler for the connectionless **voice** command `v` (reads qport, matches a client, calls `SV_ParseVoicePackets[Local]`), not the packet loop. The real in-band packet entry is 0x080960e2 (merged as `SV_ReadPackets_080960e2`; Q3 `SV_PacketEvent`). The merge kept the wrong function under the plain name. | `SV_ConnectionlessPacket` dispatches `"v"` to it (`strcmp(argv0,"v")`); disasm calls 0x809c3c6/0x809c2ee; conflicts.json entry |
| 2 | 0x080698e8 `MSG_WriteDeltaPlayerstate` vs 0x0806a200 `MSG_WriteDeltaPlayerState` | Case-only difference; 0x080698e8 encodes the **clientState** table (0x08141f60: team, name[0..28], modelindex, attach*), 22 fields/6 bits/0x5c bytes. chunk_008 header and comments call it "player state". Reader sibling is already named `MSG_ReadDeltaClientState` (0x08069f14). | rodata table dump; `SV_EmitPacketClients` passes 0x5c-byte `clientState_t` |
| 3 | `originFields` (0x08142a20, symbol says 3 entries) | Has **6** entries and describes the 0x1c-byte *objective* record (origin[3], icon, entNum, teamNum; plus the leading state dword written as 3 bits); caller uses count 6 and stride 0x1c, 16 records at playerState+0x5e4. | rodata; `MSG_WriteDeltaPlayerState` loop |
| 4 | `recordFields32` (0x08142b80), `MSG_*DeltaRecordArray` | Table fields are x,y,z,type,color,font,fontScale,alignOrg,alignScreen,text,label,fade/move/scale times, foreground...: a **hudelem** (0x80) table; two arrays of 31 at playerState+0x7a8 and +0x1728. Names say "record". | rodata |
| 5 | chunk_023 `client_s.reliableAcknowledge` at +0x20840 | Wrong: +0x20840 is `lastClientCommandSeq` (chunk_020 name). Real `reliableAcknowledge` is +0x20810 (chunk_022). `SV_SendClientSnapshot` writes the *last client command sequence* as the leading long, not `reliableAcknowledge`. | `objdump` 0x0809ae4c/0x0809af20 read [+0x20840]; chunk_020 `SV_ClientCommand` assigns +0x20840 |
| 6 | client+0x20814 | `unk20814` (020), `reliableSent` (022), `reliableSearchCursor` (021): one dword; `SV_UpdateServerCommandsToClient` stores `reliableSequence` into it. | chunk_022:866ff |
| 7 | client+0xae710 | `lastCmdSeq` (020), `serverId` (022). 022's reading (`cl->serverId = MSG_ReadByte` right after `Netchan_Process`, used in `SV_Netchan_Decode`) is the correct one. | chunk_022 `SV_ReadPackets_080960e2` |
| 8 | client+0xae70c | `unk_ae70c` gates queueing (021), `isBot` (020 sets 1 in `SV_AddBot`), `noTimeout` (022 reads in `SV_CheckTimeouts`): same dword; semantics are "bot / no timeout / no reliable commands". | three chunks |
| 8b | client+0x20e28, +0x20e20, +0x20e18 | +0x20e28: `enterWorldTime` (020) vs `nextSnapshotTime` (023), same dword (Q3 order lastPacketTime/lastConnectTime/nextSnapshotTime favours the latter) [I]; +0x20e20 `lastActivityTime` (020) = `lastPacketTime` (021/022/101) [O]; +0x20e18 `usercmdAckSeq` (020) = `deltaMessage` (022; read as the delta base in `SV_WriteSnapshotToClient`) [I]. | chunk_020/022/023 |
| 9 | client+0x20c68 | `flag20c68` byte (023, tested `== 0` in `SV_SendMessageToClient`) is `downloadName[0]` (020, 101). | offsets |
| 10 | client+0x20e08..0x20e14 | 020: `wwwDownload`@e08 (client capability from `cl_wwwDownload`), `redirectSent`@e0c, `downloadGate`@e10, `downloadRetry`@e14. 101: `bWWWDl`@e0c, `bWWWing`@e10, `bFallback`@e14 and nothing at e08. Same offsets, different names. | `objdump` 0x0808f994 (cmp [+0x20e0c]); 0x080909ca/f3 store [+0x20e08] |
| 11 | chunk_101 `netadr_s` | places `ipx[10]` at +8 and `port` at +0x12; every other chunk and the code (`Sys_GetPacket`, `NET_AdrToString`) use `port` at +8, `ipx` at +0xa. | chunk_009/038 asserts, `SockadrToNetadr` |
| 12 | `netchan_t` sizes | chunk_022 asserts 0x4003c (omits profile pointer which it keeps in `client_s` at +0xae700), chunk_023 asserts 0x2003c (truncated), chunk_009 0x40040 (correct: `guid` at +0xae704 = +0x6e6c4+0x40040). | offsets |
| 13 | `NET_*` type enum | chunk_009 comment lists 0 bot, 1 bad, 2 loopback, 4 IP, 5 IPX only; chunk_038 adds 3 broadcast and 6 broadcast IPX. `NET_CompareBaseAdrSigned` has no case 3/6 (prints `"bad address type"`). | code |
| 14 | `msg_s` partial definitions | 006, 009, 020, 021 give 4-5 fields (020 even a 0x20 size); real size 0x18 (`MSG_Init` memset 0x18; `bit` at +0x14 in 008/038/100). 009 has no `bit`. | chunk_008 `MSG_Init` |
| 15 | chunk_008 `MSG_WriteDeltaField` 0x080692dc / `MSG_ReadDeltaField` 0x08069960 | Reconstruction treats integer fields as `float` (compares `*from == *to` as float, converts with `(int)`/`(float)`), while the asm does an **integer** dword compare for "changed" and integer bit shifts/`sar` for the `bits>0` path. Float-typed Ghidra output was kept. Likely wrong for integer fields containing patterns that are NaN/-0.0 and for sign/rounding behaviour; the playerState variant in chunk_009 uses raw 32-bit loads and is closer. | asm 0x080692fa-0x08069302 (`cmp eax,[edx]`), 0x08069584+ (`sar`) |
| 16 | chunk_009 `MSG_WriteDeltaPlayerState` | uses `round()`; asm uses `fistp` with control word `|0xc00` (round toward zero), same as the entity coder. | `objdump` 0x0806a366-0x0806a37d |
| 17 | 0x0809602e `SV_SendToClients` | PB engine callback (cb_148) that OOB-sends `(len,data)` to one/all clients; naming "inferred" with no role. Suggest `SV_PB_SendOOBToClients`. | 0x0813c582 body |
| 18 | `SV_GetClientInfo` 0x08095e48, `SV_GetClientScoreText` 0x08095f64, `SV_GetMaxClients` 0x08095e3a | PB query-callback services, not generic server accessors. | 0x0813c43a body |
| 19 | chunk_061 `cvar_query` typedef `void(int,char*)` | returns `char*` (error string `"PB Error: Query Failed"`); key `0x67` is a dvar-string lookup. Name `cvar_query` hides that keys 0x65/0x66/0x72 are not cvars. | disasm |
| 20 | `SV_NoteAuthorizeServerPacket` 0x0808cf40 | Compares sender with `SV_ResolveMasterAddress()` (0x0849fbe0, port 20710) while `SV_AuthorizeIpPacket`/`SV_RequestIpAuthorize` use a separate authorize address 0x084400f0 (port 20700). Two caches for the same host. Name vs role may be wrong, or this is a real binary inconsistency. | chunk_020 + chunk_022 |
| 21 | `SV_ResolveMasterAddress` 0x08096e0c | Calls `strstr(":", name)` (swapped args), so the "no explicit port" branch always runs and overwrites the port with 0x50e6; harmless but any port in the host string is ignored. | chunk_022 comment (asm not re-checked) |
| 22 | `SV_ParseVoicePackets` 0x0809c3c6 vs `...Local` 0x0809c2ee | Dispatch is `state <= 3 -> ParseVoicePackets (relay to all, short length)`, `state > 3 -> Local (byte length, game-level talk)`; names suggest the opposite. | `objdump` 0x08094c44 `cmp [eax],3; jg` |
| 23 | `SV_Mute_f` 0x08090a5a | bound `n > 0x40` is off by one (allows 64 -> overlays `voiceFlag`); chunk_101 type declares `muted[0x41]` hiding that the 0x41st byte is `voiceFlag`. | `objdump`; chunk_023 layout |
| 24 | Dead code with ordinary names | `MSG_WriteDeltaUsercmdKey`, `MSG_ReadDeltaEntity`, `MSG_ReadInt64/WriteInt64`, `MSG_WriteAngle8`, `MSG_ReadBigString`, `NET_OutOfBandData`, `NET_DumpProfile_f` (command registered, so reachable), `Huff_Compress/Decompress` (adaptive), `NET_ReceivePacket`, `PB_SendJoinedText`, `PB_SendEvent33`: 0 direct call sites. | `objdump` search of `call 0x...` |
| 25 | 0x0806b1b8 `Net_SignExtend16`, 0x0806b1cc `Net_Identity32`, 0x0806cf00 `Netchan_SequenceToWire` | Endian-swap placeholders (LittleShort/LittleLong inlined as identity); chunk_002/030 define the real `LittleLong`/`LittleShort` at 0x0805208a/0x080b75ec/0x080b75ce. | bodies |
| 26 | chunk_038 `chunk_038.json` addresses | Written as 9-hex-digit `0x0080d5160`; merged `symbols.json` normalises to `0x080d5160` (checked). Anyone consuming per-chunk JSON directly will mis-key 38. | json |
| 27 | Missing symbols | `main` 0x080d4f7c, PB callbacks 0x0813c43a/0x0813c524/0x0813c582/0x0813c5a8, the 0x080d3xxx gap functions: no finished chunk covers them. | symbols grep |
| 28 | `NET_OutOfBandPrint` vs `NET_OutOfBandSend` | Both prefix `FF FF FF FF`; `Print` = text (strlen), `Send` = binary with explicit length and 0x20004 scratch, `Data` is an unused twin. | code |

---------------------------------------------------------------------

## 11. Open questions

* [U] `SV_ReadPackets_080960e2` string text: reconstruction renamed the literal to `...080960e2: fixing up...`; need the binary's exact string (original likely `SV_ReadPackets: fixing up a translated port`).
* [U] Meaning of `svs.serverId` at 0x08423008 vs `cv_sv_serverid`; whether `serverId & 0xf0` relates to map-restart counters.
* [U] `PB_InvokeCb158` (0x0813c0ec) purpose (called for `connect` with address text + raw packet) and `cb_144/cb_154/cb_160`; `pbsv.so` ABI of `sa`/`sb` (event ids beyond those listed). `0x08a6f2e0` in the console-print callback.
* [U] Who calls the `MsgBuf_*` writers/readers (0x08123563 region) and what they serialise.
* [U] Voice dispatch state split (section 10 #22): is the "relay" path pre-game chat or a mistaken branch?
* [U] Exact flags semantics of dvar flag words (0x1000 etc.) - out of subsystem scope.
* [U] Whether `Sys_GetPacket` `ECONNREFUSED/EAGAIN` suppression matches the original: `errno 0xb` and `0x6f` hard-coded [O].
* [U] `SV_UserMove` baseline for the first command (`CL_CreateCmd(ps, &baseline[1])`) - CL_CreateCmd builds a usercmd from playerState; chunk_020 marks it partial.
* [U] `ReadDeltaField` integer path correctness (section 10 #15): needs oracle replay with a real packet capture before the coder is trusted for bit-exactness.
* [I] Net `loopback_s` array address and count (only queue 1 is polled by the server; index by sock/port).

## 12. Appendix: netField tables (read from .rodata of the binary)

[observed] Each entry is 12 bytes `{const char *name; int offset; int bits}`; list index = wire order. Format below: `index:name@offset/bits`. Offsets are into the in-memory struct; bits semantics are in section 5.4.

### entityStateFields (MSG_WriteDeltaEntity / MSG_ReadDeltaEntity), 59 entries, base 0x081417a0, struct 0xf0

```
0:pos.trTime@0x10/32; 1:pos.trBase[1]@0x1c/0; 2:pos.trBase[0]@0x18/0; 3:pos.trDelta[0]@0x24/0; 4:pos.trDelta[1]@0x28/0; 5:angles2[1]@0x6c/0; 6:apos.trBase[1]@0x40/-100; 7:pos.trDelta[2]@0x2c/0; 8:pos.trBase[2]@0x20/0; 9:apos.trBase[0]@0x3c/-100; 10:eventSequence@0xa4/8; 11:legsAnim@0xcc/10; 12:eType@0x4/8; 13:eFlags@0x8/24; 14:otherEntityNum@0x74/10; 15:surfType@0x88/8; 16:eventParm@0xa0/8; 17:scale@0xd8/8; 18:clientNum@0x90/8; 19:torsoAnim@0xd0/10; 20:groundEntityNum@0x7c/10; 21:events[0]@0xa8/8; 22:events[1]@0xac/8; 23:events[2]@0xb0/8; 24:angles2[0]@0x68/0; 25:events[3]@0xb4/8; 26:apos.trBase[2]@0x44/-100; 27:pos.trType@0xc/8; 28:fWaistPitch@0xec/0; 29:fTorsoPitch@0xe8/0; 30:apos.trTime@0x34/32; 31:solid@0x9c/24; 32:apos.trDelta[0]@0x48/0; 33:apos.trType@0x30/8; 34:animMovetype@0xe0/4; 35:fTorsoHeight@0xe4/0; 36:apos.trDelta[2]@0x50/0; 37:weapon@0xc8/7; 38:index@0x8c/10; 39:apos.trDelta[1]@0x4c/0; 40:eventParms[0]@0xb8/8; 41:eventParms[1]@0xbc/8; 42:eventParms[2]@0xc0/8; 43:eventParms[3]@0xc4/8; 44:iHeadIcon@0x94/4; 45:pos.trDuration@0x14/32; 46:iHeadIconTeam@0x98/2; 47:time@0x54/32; 48:leanf@0xd4/0; 49:attackerEntityNum@0x78/10; 50:time2@0x58/32; 51:loopSound@0x84/8; 52:origin2[2]@0x64/0; 53:origin2[0]@0x5c/0; 54:origin2[1]@0x60/0; 55:angles2[2]@0x70/0; 56:constantLight@0x80/32; 57:apos.trDuration@0x38/32; 58:dmgFlags@0xdc/32
```

### sharedEntityFields (MSG_*DeltaSharedEntity), 68 entries, base 0x08141ae0, struct 0x114

```
0:absmin[1]@0x100/0; 1:absmax[1]@0x10c/0; 2:absmin[0]@0xfc/0; 3:absmax[0]@0x108/0; 4:absmin[2]@0x104/0; 5:absmax[2]@0x110/0; 6:pos.trBase[1]@0x1c/0; 7:pos.trBase[0]@0x18/0; 8:eType@0x4/8; 9:eFlags@0x8/24; 10:pos.trBase[2]@0x20/0; 11:svFlags@0xf0/32; 12:groundEntityNum@0x7c/10; 13:apos.trBase[1]@0x40/0; 14:clientNum@0x90/8; 15:apos.trBase[0]@0x3c/0; 16:index@0x8c/10; 17:apos.trBase[2]@0x44/0; 18:eventSequence@0xa4/8; 19:events[0]@0xa8/8; 20:legsAnim@0xcc/10; 21:events[1]@0xac/8; 22:events[2]@0xb0/8; 23:events[3]@0xb4/8; 24:weapon@0xc8/7; 25:pos.trType@0xc/8; 26:pos.trTime@0x10/32; 27:apos.trType@0x30/8; 28:solid@0x9c/24; 29:pos.trDuration@0x14/32; 30:eventParms[0]@0xb8/8; 31:torsoAnim@0xd0/10; 32:pos.trDelta[0]@0x24/0; 33:pos.trDelta[1]@0x28/0; 34:angles2[1]@0x6c/0; 35:angles2[0]@0x68/0; 36:animMovetype@0xe0/4; 37:pos.trDelta[2]@0x2c/0; 38:otherEntityNum@0x74/10; 39:eventParms[1]@0xbc/8; 40:surfType@0x88/8; 41:eventParm@0xa0/8; 42:eventParms[2]@0xc0/8; 43:scale@0xd8/8; 44:eventParms[3]@0xc4/8; 45:fTorsoHeight@0xe4/0; 46:fWaistPitch@0xec/0; 47:fTorsoPitch@0xe8/0; 48:apos.trTime@0x34/32; 49:apos.trDelta[0]@0x48/0; 50:apos.trDelta[2]@0x50/0; 51:clientMask[0]@0xf4/32; 52:clientMask[1]@0xf8/32; 53:leanf@0xd4/0; 54:apos.trDelta[1]@0x4c/0; 55:loopSound@0x84/8; 56:attackerEntityNum@0x78/10; 57:iHeadIcon@0x94/4; 58:iHeadIconTeam@0x98/2; 59:apos.trDuration@0x38/32; 60:time@0x54/32; 61:time2@0x58/32; 62:origin2[0]@0x5c/0; 63:origin2[1]@0x60/0; 64:origin2[2]@0x64/0; 65:angles2[2]@0x70/0; 66:constantLight@0x80/32; 67:dmgFlags@0xdc/32
```

### clientStateFields (MSG_WriteDeltaPlayerstate [sic] / MSG_ReadDeltaClientState), 22 entries, base 0x08141f60, struct 0x5c

```
0:team@0x4/2; 1:name[0]@0x3c/32; 2:name[4]@0x40/32; 3:modelindex@0x8/8; 4:attachModelIndex[1]@0x10/8; 5:attachModelIndex[0]@0xc/8; 6:name[8]@0x44/32; 7:name[12]@0x48/32; 8:name[16]@0x4c/32; 9:name[20]@0x50/32; 10:name[24]@0x54/32; 11:name[28]@0x58/32; 12:attachTagIndex[5]@0x38/5; 13:attachTagIndex[0]@0x24/5; 14:attachTagIndex[1]@0x28/5; 15:attachTagIndex[2]@0x2c/5; 16:attachTagIndex[3]@0x30/5; 17:attachTagIndex[4]@0x34/5; 18:attachModelIndex[2]@0x14/8; 19:attachModelIndex[3]@0x18/8; 20:attachModelIndex[4]@0x1c/8; 21:attachModelIndex[5]@0x20/8
```

### playerStateFields (MSG_WriteDeltaPlayerState / MSG_ReadDeltaPlayerState), 105 entries, base 0x08142520, struct 0x26a8

```
0:commandTime@0x0/32; 1:origin[1]@0x18/0; 2:origin[0]@0x14/0; 3:bobCycle@0x8/8; 4:viewangles[1]@0xec/-100; 5:origin[2]@0x1c/0; 6:velocity[1]@0x24/0; 7:velocity[0]@0x20/0; 8:viewangles[0]@0xe8/-100; 9:movementDir@0x9c/-8; 10:velocity[2]@0x28/0; 11:eventSequence@0xa4/8; 12:legsAnim@0x7c/10; 13:aimSpreadScale@0x5d4/0; 14:weaponTime@0x34/-16; 15:pm_flags@0xc/27; 16:events[0]@0xa8/8; 17:events[1]@0xac/8; 18:events[2]@0xb0/8; 19:events[3]@0xb4/8; 20:weapAnim@0x5d0/10; 21:viewHeightCurrent@0xf8/0; 22:torsoTimer@0x80/16; 23:torsoAnim@0x84/10; 24:eFlags@0xa0/24; 25:fWeaponPosFrac@0xdc/0; 26:holdBreathScale@0x5b4/0; 27:weaponstate@0xd8/5; 28:viewHeightTarget@0xf4/-8; 29:weaponDelay@0x38/-16; 30:legsTimer@0x78/16; 31:viewHeightLerpTarget@0x100/-8; 32:groundEntityNum@0x60/10; 33:pm_time@0x10/-16; 34:eventParms[3]@0xc4/8; 35:eventParms[1]@0xbc/8; 36:eventParms[0]@0xb8/8; 37:eventParms[2]@0xc0/8; 38:weapon@0xd4/7; 39:weapons[0]@0x544/32; 40:viewHeightLerpDown@0x104/1; 41:weaponslots[0]@0x554/32; 42:delta_angles[0]@0x54/16; 43:delta_angles[1]@0x58/16; 44:cursorHintString@0x59c/-8; 45:offHandIndex@0xd0/7; 46:clientNum@0xcc/8; 47:viewlocked_entNum@0x594/16; 48:viewmodelIndex@0xe4/8; 49:viewHeightLerpTime@0xfc/32; 50:speed@0x50/16; 51:mins[1]@0x570/0; 52:mins[0]@0x56c/0; 53:maxs[2]@0x580/0; 54:maxs[1]@0x57c/0; 55:maxs[0]@0x578/0; 56:gravity@0x48/16; 57:damageTimer@0x90/16; 58:cursorHint@0x598/8; 59:mantleState.flags@0x5c8/4; 60:flinchYaw@0x98/16; 61:fWaistPitch@0x5b0/0; 62:mantleState.timer@0x5c0/32; 63:fTorsoPitch@0x5ac/0; 64:proneTorsoPitch@0x58c/0; 65:holdBreathTimer@0x5b8/16; 66:jumpTime@0x70/32; 67:viewangles[2]@0xf0/-100; 68:foliageSoundTime@0x44/32; 69:weapons[1]@0x548/32; 70:damageEvent@0x11c/8; 71:damageDuration@0x94/16; 72:damageYaw@0x120/8; 73:proneDirection@0x584/0; 74:proneDirectionPitch@0x588/0; 75:mantleState.yaw@0x5bc/0; 76:mantleState.transIndex@0x5c4/4; 77:fTorsoHeight@0x5a8/0; 78:damagePitch@0x124/8; 79:jumpOriginZ@0x74/0; 80:pm_type@0x4/8; 81:viewlocked@0x590/8; 82:weaponrechamber[0]@0x55c/32; 83:vLadderVec[0]@0x64/0; 84:weaponslots[4]@0x558/32; 85:weaponRestrictKickTime@0x40/-16; 86:vLadderVec[1]@0x68/0; 87:viewAngleClampRange[1]@0x118/0; 88:viewAngleClampRange[0]@0x114/0; 89:viewAngleClampBase[1]@0x110/0; 90:weaponrechamber[1]@0x560/32; 91:leanf@0x4c/0; 92:damageCount@0x128/7; 93:grenadeTimeLeft@0x3c/-16; 94:deltaTime@0x7a4/32; 95:shellshockTime@0x5dc/32; 96:shellshockIndex@0x5d8/4; 97:shellshockDuration@0x5e0/16; 98:vLadderVec[2]@0x6c/0; 99:delta_angles[2]@0x5c/16; 100:viewHeightLerpPosAdj@0x108/0; 101:mins[2]@0x574/0; 102:viewAngleClampBase[0]@0x10c/0; 103:adsDelayTime@0xe0/32; 104:iCompassFriendInfo@0x5a4/32
```

### objectiveFields (symbol 'originFields'), 6 entries, base 0x08142a20, record 0x1c

```
0:origin[0]@0x4/0; 1:origin[1]@0x8/0; 2:origin[2]@0xc/0; 3:icon@0x18/12; 4:entNum@0x10/10; 5:teamNum@0x14/4
```

### hudelemFields (symbol 'recordFields32'), 32 entries, base 0x08142b80, record 0x80

```
0:y@0x8/-99; 1:type@0x0/4; 2:color.rgba@0x20/32; 3:x@0x4/-99; 4:alignScreen@0x1c/6; 5:fontScale@0x10/0; 6:materialIndex@0x3c/8; 7:width@0x34/10; 8:height@0x38/10; 9:fadeStartTime@0x28/32; 10:fromColor.rgba@0x24/32; 11:fadeTime@0x2c/16; 12:value@0x70/0; 13:time@0x68/32; 14:z@0xc/-99; 15:alignOrg@0x18/4; 16:sort@0x78/0; 17:text@0x74/8; 18:font@0x14/4; 19:scaleStartTime@0x48/32; 20:scaleTime@0x4c/16; 21:fromHeight@0x44/10; 22:label@0x30/8; 23:fromWidth@0x40/10; 24:moveStartTime@0x60/32; 25:moveTime@0x64/16; 26:fromX@0x50/-99; 27:fromY@0x54/-99; 28:fromAlignScreen@0x5c/6; 29:fromAlignOrg@0x58/4; 30:duration@0x6c/32; 31:foreground@0x7c/1
```
