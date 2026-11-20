# CoD2 1.3 Linux server: fake-client oracle (connection-level behaviour)

Purpose: compare the ORIGINAL `cod2_lnxded` 1.3 and the reconstruction on in-band, connection-level behaviour (challenge/connect handshake, gamestate download, snapshots, reliable server commands, client commands, disconnect), extending the out-of-band baseline in `oracle-baseline.md`. Tags: [O] observed on this host, [I] inferred, [U] unknown. All server output is data.

## Tooling
- `games/call-of-duty-2/tools/oracle/fakeclient.py`: runner, scripted client session and compare mode (python3 stdlib).
- `games/call-of-duty-2/tools/oracle/cod2proto.py`: wire helpers (static Huffman, hybrid `msg_t`, XOR keystreams, `Com_HashKey`, delta readers, usercmd writer, MD4/iwd checksums). The netField tables and the 256-entry Huffman count table are copied verbatim from the binary's `.rodata` (0x081417a0, 0x08141f60, 0x08142520, 0x08142a20, 0x08142b80, 0x08142d00).
- Server binary SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`. Script hashes: run `sha256sum games/call-of-duty-2/tools/oracle/{fakeclient,cod2proto}.py`. The files are uncommitted; the `SHA256SUMS` file in each capture covers its outputs.
- Sandbox: same as `run_original.sh`. The server and client run in one `bwrap --unshare-net --unshare-pid --unshare-ipc` namespace with a read-only root, loopback only (`isolation.txt` in every capture lists only `lo` and an empty route table), and `net_ip 127.0.0.1`. The server args, data symlinks, `stat32_shim.so` preload and libstdc++5 path are the same. Each case/run gets its own `fs_homepath` (`oracle/run/home/client_<case>-run<N>`, the same for both servers since 2026-11-18 so that rcon text with the path splits into packets identically; do not run both servers concurrently with the same case and index). The client binds 127.0.0.1:28971 and uses a fixed qport 7467 (0x1d2b) so the server's view of the client stays constant.

```
python3 games/call-of-duty-2/tools/oracle/fakeclient.py run dm_toujane 1            # original (default)
python3 games/call-of-duty-2/tools/oracle/fakeclient.py run dm_toujane 1 --server recon   # .local/cod2/build/cod2_lnxded_recon
python3 games/call-of-duty-2/tools/oracle/fakeclient.py run lanauth_dm_toujane 2 --auth-wait 1500
python3 -I games/call-of-duty-2/tools/oracle/fakeclient.py compare .local/cod2/oracle/captures/client_dm_toujane [--json out.json]
python3 -I games/call-of-duty-2/tools/oracle/fakeclient.py compare .local/cod2/oracle/captures/client_dm_toujane .local/cod2/recon-run/captures/client_dm_toujane
```
Cases: `dm_toujane`, `sd_carentan` (the same server args as the OOB baseline), `restart_dm_toujane` / `restart_sd_carentan` (rcon `map_restart` while the client is active, then a second activation) and `lanauth_dm_toujane` (adds `+set net_lanauthorize 1`, which forces the authorize-server path for a LAN client). Original captures go to `.local/cod2/oracle/captures/client_<case>/run<N>/` and reconstruction captures to `.local/cod2/recon-run/captures/client_<case>/run<N>/`. Each capture contains:
- `packets.jsonl`: every datagram in either direction, as raw hex with a timestamp.
- `decoded.jsonl`: each server message as its 4 raw bytes plus the XOR-decoded, Huffman-decompressed body.
- `events.jsonl`: the decoded event stream.
- `summary.json`
- `server.log`, `client.log`, `harness.log`, `isolation.txt` and `SHA256SUMS`.

The compare mode flattens `summary.json` and the server log into items: OOB replies, configstrings, baseline hashes, server commands, and for each snapshot index its delta number, flags, entity and client counts, state hashes, relative playerState, byte sizes and ops structure. With one directory it classifies each item across `run*/` as identical or variable. With a second directory it lists every item that is deterministic in the original but differs in the other server.

## Session script (what the client does)
1. `getinfo` until ready, then with 1.1 s gaps: `getchallenge`, `connect` with protocol 117, `connect` with a wrong challenge, `getchallenge` again.
2. `connect "<userinfo>"` (plain text, not Huffman-compressed). The userinfo is `\cg_predictItems\1\cl_anonymous\0\cl_punkbuster\0\cl_voice\0\cl_wwwDownload\0\rate\25000\snaps\20\name\fakeclient\protocol\118\challenge\<N>\qport\7467`. [I] This key set is modelled on a CoD2 client; the server reads protocol, challenge, qport, name, rate, snaps, cl_voice, cl_wwwDownload, cl_punkbuster and password.
3. Pacing (`--pace`, recorded in `summary.json`): `snapshot` (default since 2026-11-18) sends a packet immediately after every in-band server message (100 ms fallback timer); `timer` (the first captures, kept in `client_*-timerpace/`) sends every 50 ms, which makes commandTime depend on the client/server clock phase (see `boot-parity.md`, in-game section). Each packet is `[seq:4][qport:2][serverId:1][messageAck:4][reliableAck:4] XOR(Huffman(payload))`.
   - Before the gamestate: the payload is a 3-bit EOF (3) and serverId is 0, so the server sends the gamestate.
   - After the gamestate: the client sends `cp @ <pure checksums...> <checksumFeed ^ sums ^ n>` together with a usermove. The usermove is 3-bit op 1 (no delta) or 0 (delta, used when the last received message held a valid snapshot), a count byte and keyed usercmds that only advance serverTime. The client also sends unacknowledged reliable client commands in every packet.
   - The pure checksums are those of the iwds listed in `sv_referencedIwdNames`, computed from the same iwd files: MD4 over the CRCs of non-empty zip entries, keyed with checksumFeed. Their unkeyed values match `sv_referencedIwds` exactly ([O]; `cp_iwds` in `summary.json`).
4. While active: `rcon status` at +1 s, `say hi` at +2 s, then the reliable command `disconnect` at +6 s, sent in 3 packets. `restart_*`: `rcon map_restart` at +3 s instead; after the new gamestate the client resends `cp`, and at +1 s / +3 s of the second activation sends `rcon status` / `disconnect`. The client then listens passively for 2 s, sends `rcon status` again and finally `rcon quit`.

## Protocol facts confirmed by the oracle (beyond network.md)
- [O] **`connectResponse` OOB**: after a successful `connect` the original sends `FF FF FF FF "connectResponse"` before any in-band traffic. Disassembly: `SV_DirectConnect` 0x0808eef3..0x0808ef25 pushes string 0x0814a1bc and calls `NET_OutOfBandPrint(1, from, ...)` right after `SV_UserinfoChanged` and `challenges[i].firstPing = 0`, and before `gamestateMessageNum = -1`. **The reconstruction (`recon/chunks/chunk_020.cpp` `SV_DirectConnect`) omits this call.** That is a parity defect for the lane that owns chunk 020.
- [O] Rejections: protocol 117 gives `error\nEXE_SERVER_IS_DIFFERENT_VER\x151.3\n`; a wrong challenge gives `error\nEXE_BAD_CHALLENGE`. Neither allocates a client slot.
- [O] Server message layout: `[seq][frag hdr?]` then a raw long, then `XOR(Huffman(body))`. The raw long is `lastClientCommand`: it tracked our client-command sequence 0→1→3. This confirms network.md naming issue #5 (chunk_023's `reliableAcknowledge` label is wrong).
  - The XOR key is `(seq ^ challenge) & 0xff` with the last executed client-command text.
  - Client→server XOR key: `(challenge ^ serverId ^ messageAck) & 0xff` with the server-command text at `reliableAck`.
  - Usercmd key: `checksumFeed ^ messageAck ^ Com_HashKey(serverCmd[reliableAck], 32)`.
  - All of these were exercised successfully: every server message decoded to exactly its EOF byte (plus at most 1 Huffman pad byte), and the server accepted every client command and usercmd.
- [O] Fragmentation: the gamestate arrives in 1300-byte fragments: 7 for mp_toujane (payload 8512 bytes, 9001 decompressed) and 9 for mp_carentan (about 10470 bytes).
- [O] Gamestate contents:
  - Server commands 1..31 (`v <dvar> "<value>"`), then `reliableSequence` 31.
  - Configstrings: 282 for dm/mp_toujane and 347 for sd/mp_carentan. `[1]` is systeminfo, with `sv_serverid\16`, `sv_pure\1`, `sv_iwds`, `sv_referencedIwds` and the names of the 6 referenced iwds.
  - Baselines: 38 for toujane and 49 for carentan.
  - Then clientNum 0 and checksumFeed, with two EOF bytes as described in network.md.
  - serverId 16 is the byte value 0x10, the first map with no restarts, as `SV_SpawnServer` code predicts.
- [O] Before the gamestate the server sends, every server frame (50 ms), messages that contain only `[long][EOF]`, while the client is in CS_CONNECTED; it does the same in CS_PRIMED until the first usermove.
- [O] State timeline after `connect`:

  | Time after `connect` | Event |
  |---|---|
  | ~0.6 s (carentan ~0.78 s) | Gamestate |
  | ~0.8 s (carentan ~0.95 s) | First snapshot with `snapFlags` 4 (bit 2 clear: active) |
  | Server log | `Going from CS_FREE to CS_CONNECTED for  (num 0 guid 0)`; the name is still empty at that point. A zero GUID is reported because a LAN client is not authorised. |

- [O] Snapshots:
  - The first active snapshot is a full one (deltaNum 0) with 81 changed playerState fields. It holds 27 entities (toujane) or 33 (carentan) and 1 clientState (our name, team 3 = spectator).
  - Later snapshots are deltas with deltaNum 1 at a steady 50 ms serverTime step (sv_fps 20; on LAN the snapshot rate is not capped). Typically one playerState field (commandTime) changes and no entity or client deltas occur.
  - Our usercmds drive `commandTime = serverTime - 50`. The player stays a spectator at the spawn view (`pm_type 4`, origin -170 1008 456 on toujane) and never spawns, so no movement is exercised.
- [O] Server commands while connected, deterministic, in order:
  - Sequence 32: `MP_CONNECTEDfakeclient^7`, with a leading space and no command letter.
  - Sequence 33: `v ui_allow_weaponchange "0"`.
  - Sequence 34: `t 4`.
  - Sequences 35..38: `v` dvars (`cg_objectiveText`, `g_scriptMainMenu "ingame"`, `ui_allow_joinallies`, `ui_allow_joinaxis`).
  - Sequence 39: the chat echo `h "(GAME_SPECTATOR)fakeclient^7: ^7hi"`, logged as `(GAME_SPECTATOR)fakeclient^7: hi`.
  - Sequence 40: `w "EXE_DISCONNECTED"` after our `disconnect`, logged as `0:fakeclient EXE_DISCONNECTED`.
  - After disconnect the client is a zombie (state 1). It keeps receiving full snapshots (deltaNum 0) every 50 ms with 0 entities and 0 clients, and sequence 40 is repeated in each message until it is acknowledged.
- [O] `rcon status` while connected: `  0     0 <ping>      0 fakeclient^7  50 127.0.0.1:28971  7467 25000`. After disconnect plus the zombie time the table is empty.
- [O] **Rate.** The status rate column prints `cl->rate` (`[client+0x6e6b8]`, read at 0x0808c962). It shows 25000, the userinfo value, not the LAN value 99999.
  - `SV_UserinfoChanged` (0x08090833..0x08090854) stores 99999 only if `Sys_IsLANAddress(addr) && com_dedicated->current.integer != 2`.
  - The OOB baseline's `rcon dvarlist` shows `R  L  dedicated "2"` at runtime, although the harness passes `+set dedicated 1`. So the LAN-rate branch is skipped, and master heartbeats are enabled (`Resolving cod2master...` at startup).
  - The reconstruction's logic agrees. A port must reproduce `dedicated` ending up as 2. [U] The mechanism (latched/read-only override) was not traced.
- [O, earlier run, capture not kept] A wrong `cp` (plain `sv_referencedIwds` sums instead of pure sums) gives:
  - server command `w "EXE_UNPURECLIENTDETECTED"` and the log line `0:fakeclient EXE_UNPURECLIENTDETECTED`;
  - the client listed as `ZMBI` in `rcon status`;
  - from about 2 s later (sv_zombietime 2) the server answers every in-band packet from the freed slot with OOB `disconnect`. [I] Mechanism: `SV_ReadPackets` finds no matching client.
- [O] `Client 0 connecting with 1700 challenge ping`: the ping is the time between the last answered `getchallenge` and `connect`, quantised to server frames. It is 1700 or 1750 depending on client timing.

## Authorisation path (CD-key / master) with no network: oracle facts
- [O] Default (LAN client, `net_lanauthorize 0`): `getchallenge` is answered immediately with `challengeResponse <N>`. No authorize request is attempted, `guid` is 0 and `connect` succeeds. There is no `kc` or CD-key exchange on this path. PunkBuster is off (`sv_punkbuster 0`, `cl_punkbuster 0`); `PB_InvokeCb15c` did not veto.
- [O] `net_lanauthorize 1` (`client_lanauth_dm_toujane/run1`): the first `getchallenge` logs `Resolving cod2master.activision.com` / `Couldn't resolve address` and gets **no reply**. A second `getchallenge` about 8 s later is silent, both in the reply and in the log (no new resolve attempt). The client cannot connect.
- [O] Long poll (`client_lanauth_dm_toujane/run2`, `--auth-wait 1500`, `getchallenge` every 30 s):
  - Every poll up to 1184 s after the first `getchallenge` was silent.
  - The poll at 1214.5 s got `challengeResponse 999243735`, after about 20 min of server uptime. This matches the code in `SV_RequestIpAuthorize`: it replies when `svs.time - lastAuthServerPacket > 1200000` ms and the slot is more than 7000 ms old. The "authorize server timed out" message is only a DPrintf and was not logged.
  - The rest of the session then ran exactly as in the LAN case: `connect` accepted with a zero GUID, gamestate, active snapshots, the chat echo, disconnect.
  - So, with the authorize server unreachable, the original refuses challenges for about the first 20 min of uptime and then accepts every client unauthorised (guid 0).

## Deterministic vs variable (3 runs each of dm_toujane and sd_carentan, original server)
`fakeclient.py compare` reports:
- dm_toujane: 2768 identical and 211 variable items.
- sd_carentan: 2821 identical and 240 variable items.

| Class | Items | Evidence / normalisation |
|---|---|---|
| Identical | OOB replies (challenge replaced by `<CHALLENGE>`); `connectResponse`; all configstrings and baseline hashes; serverId, clientNum, reliableSequence, fragment count (dm); all server commands 1..40; collapsed message op structure; for every snapshot index: deltaNum, snapFlags, entity/client counts and delta counts, entity and clientState hashes, playerState hash with commandTime made relative to serverTime, snapshot bit-length; serverTime step 50; client-command ack; normalised server.log apart from one line (below) | compare.json in each case dir |
| Variable: random per run | challenge (stable within a run, as in the OOB baseline); `checksumFeed`, and therefore the `cp` arguments and the pure checksums; the compressed gamestate length (sd: 10469/10470, Huffman length of the feed long) | feed values differ in all runs |
| Variable: client timing | absolute serverTime / commandTime (13000-14150 at the first active snapshot); number of empty messages before the gamestate and before the first snapshot (3 vs 4); total message count (165-167); wire length of some snapshots (Huffman length of the serverTime long); `challenge ping` 1700/1750; rcon status ping column; `timing` | normalised in compare: relative times, collapsed ops structure, `<PING>` |

Snapshot contents are therefore a strong comparison target once serverTime is normalised: the entity sets of the static maps are byte-identical across runs.

## Reconstruction attempt (for the compare path)
[O] `fakeclient.py run dm_toujane 1 --server recon` with `.local/cod2/build/cod2_lnxded_recon` (build of 2026-11-17 22:59) produced the following:
- The server log stops after `------- Game Initialization -------` / `gamedate: Jun 23 2006`.
- The server never answered `getinfo` within 120 s and was killed.
- The compare run reports every deterministic item as missing, for example configstrings.

The capture is in `.local/cod2/recon-run/captures/client_dm_toujane/run1/`. The compare path itself works end to end.

## Verification against the original binary
- Disassembly checked instruction-by-instruction:
  - `MSG_ReadDeltaField` 0x08069960: changed bit; float 0 / -99 / -100 paths with a zero flag; integer path with a zero flag and sign extension.
  - `MSG_ReadDeltaFieldsAll` 0x08069c98, `MSG_ReadDeltaStruct` 0x08069d4a.
  - `MSG_ReadDeltaPlayerState` 0x0806aaca: no zero flag for float or integer fields, unlike the entity coder.
  - Control flow of `MSG_ReadDeltaRecordArray` 0x0806a0d0.
  - `Com_HashKey` 0x080acaa4 (movsx char, sar).
  - `SV_Netchan_Encode` 0x080974d8 (`netchan+0` = outgoingSequence, `+0x20820` challenge, `+0x20844` string) and `SV_Netchan_Decode` 0x08097562.
  - The `connectResponse` site above.
- Taken from the reconstruction and validated by successful end-to-end decoding of every message in all runs: Huffman build and code (`huffman_code_sha256` 4777c5a2...bd88), `msg_t` cursor rules, netchan fragments, the gamestate and snapshot writers' layouts, `SV_UserMove` / `SV_ExecuteClientMessage` / `SV_VerifyPaks_f` / `SV_ClientCommand` rules.
- Not cross-checked: the client→server usercmd encoding beyond the "no change" path. The server applied the commands, and commandTime followed our serverTime.

## Limitations
- Gameplay (spawned players, weapons, damage, killcam, map rotation, test clients) is covered by `gameplay-parity.md` (`tools/oracle/gameplay.py`).
- The client is a model, not the real CoD2 client:
  - the userinfo keys and the timing (50 ms packets, 1 usercmd per packet) are chosen;
  - the usercmds never change buttons, angles or moves, so the player stays a spectator; no team/class selection, spawn, movement, weapon fire or damage is exercised;
  - client→server packets are never fragmented;
  - reliable client commands are resent until acknowledged, as Q3 does.
- Not covered:
  - downloads (`svc` 5 is not decoded: the client stops parsing that message);
  - voice;
  - PunkBuster;
  - map change (`map`) while connected (`map_restart` is covered by `restart_*`);
  - multiple clients;
  - packet loss or reordering;
  - `sv_pure 0`;
  - timeouts;
  - the authorize server answering;
  - hudelem and objective delta paths: they are decoded, but values are not checked against independent evidence.
- Huffman decoding: when the trailing pad bits run past the input, the binary reads beyond the buffer; the client treats missing bits as 0, which is only relevant for the final pad symbol.
- Empty XOR key strings: the binary reads `buf[1]` of the 0x400-byte command buffer; it is modelled as 0, which is true for freshly zeroed buffers.
- Decoded float fields are kept as raw IEEE bits, and integer-coded floats as `(float)v`. Comparisons are of decoded state, not of in-server memory.
- Only 2 maps, German data edition, `sv_maxclients 8`, `sv_pure 1`.
- Reconstruction comparison: see `boot-parity.md`, in-game section (0 deterministic mismatches for dm_toujane, sd_carentan and restart_dm_toujane as of 2026-11-18).

## map_restart while connected (`client_restart_dm_toujane`, 3 runs, 2026-11-18) [O]
- The rcon reply is 5 print packets of re-initialisation text (FS_Startup, Game Initialization), as in the OOB baseline.
- The client keeps receiving delta snapshots of the old level until the restart completes (no `map_restart` server command was observed). Then a new gamestate arrives without first being requested: serverId 16 -> 32 (0x10 -> 0x20), reliableSequence 39, the same 282 configstrings except `[3]`/`[13]` (level start time, 16750/16850: varies per run) and 38 baselines, in 6 fragments.
- Server commands 40..70 repeat the 31 `v` dvars of the first gamestate's command block with values, 71 is `MP_CONNECTEDfakeclient^7`, 72..77 the same dvar/team commands as 33..38, 78 `w "EXE_DISCONNECTED"`.
- The second activation follows about 4.3 s after the rcon command; `rcon status` then shows the client again with ping and rate 25000. After the restart, the relative playerState hash varies per run in about 100 snapshot indices (timing-dependent fields after the level restart; not analysed further), so those items are excluded as variable by `compare`.
- The reconstruction matches all 3079 deterministic items (2 runs).
- `fakeclient.py` SHA-256 `857f85d29f59acc276fca1d38c4550ba4199e0530abb3422ed2635429f03b44f`, `cod2proto.py` `821efcf28655f04bb7358fb522e674c8bffebd3143d8f2fe1a6e20facfff718d`.
