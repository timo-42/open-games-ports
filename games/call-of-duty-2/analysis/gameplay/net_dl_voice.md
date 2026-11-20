# Gameplay lane net_dl_voice: downloads, wwwDownload refusal and voice

Original: `cod2_lnxded` 1.3 (SHA-256 `05e774a3...04d5a3a7`). Plugin: `tools/oracle/scenarios/net_dl_voice.py` (SHA-256 prefix `122cbc35`). Reconstruction used: lane build `.local/cod2/lanes/net_dl_voice/build/cod2_lnxded_recon` (prefix `4d1b6174`). Tags: [O] observed, [I] inferred, [U] unknown.

## Result

| Case | Original runs | Identical / variable items | Recon runs | Mismatches on deterministic items |
|---|---|---|---|---|
| `net_dl_dl_mp_toujane` | 3 | 46906 / 1 | 2 | 0 |
| `net_dl_www_mp_toujane` | 3 | 54679 / 0 | 2 | 0 |
| `net_dl_voice_tdm_toujane` | 3 | 153984 / 0 | 2 | 0 |
| `duel_dm_toujane` regression (`--tag=-net_dl_voice`) | existing 3 | 84560 / 0 | 1 | 0 |

The single variable item is `oob:rcon_status_after`: the zombie row of client A (`ZMBI ... lastmsg 2000` vs `1950`) [O]. It is the timing family already seen in the duel: the harness waits about 2.0 s of wall clock after A's disconnect, which is exactly sv_zombietime, so `lastmsg` is one frame more or less. It is excluded by `compare` automatically (variable in the original) and no mask was added. A harness-side fix is listed under `harness_requests`.

## What the cases exercise

Common setup: fs_game `mods/dl`, with a generated `zz_dl.iwd` under the per-run home (`<home>/mods/dl/`, stored ZIP, 2 members, 20269 bytes, fixed timestamps, built from the plugin, never committed). Block size is 2048, so the file is blocks 0..9 plus an empty terminating block 10 [O]. `sv_pure 1` (harness default) is untouched.

Two kinds of clients are used:
- Active clients (A, B, C, D): team and weapon menus staggered by 200 ms of server time per player, as in the duel.
- Connecting-state clients (E, F): join, receive the gamestate, never send a usermove, so the server keeps them in CS_PRIMED, which is the state a real client downloads in. Their packets are fired from A's snapshot times, and their `nextdl` acks are generated while the server message is parsed (a real client acks every block it receives in order), so every decision is still a function of server time.
The harness `fakeclient` stops at `svc_download` ("download op not decoded"); the plugin decodes it with its own copy of `parse_server_message` bound to each client instance (no module patching).

### `net_dl_dl_mp_toujane` (sv_allowDownload 1, sv_wwwDownload 0)
- A (active): full download to the empty block (server: `beginning`, `completed`); a second download acked only for blocks 0 and 1, with `retransdl` (matching and non-matching block), `stopdl`, `stopdl` with nothing running; a restart after setting `rate 90000` through a `userinfo` command; not found; `main/iw_00.iwd` (never acked: only the first window of the 162 MB stock iwd goes out); a 90-byte name (the 64-byte field truncates it); `download` without argument.
- B (active): downloads and acks 3 blocks, then a wrong `nextdl 7` gives `broken download` (client dropped).
- E (connecting): download with automatic `donedl` (the server resends the gamestate: second gamestate, then snapshots with snapFlags 6 while in CS_PRIMED), file not found, `main/iw_01.iwd`, a second full download, `sv_maxRate 500` (clamped to 1000 by the writer) plus `rate 2000` with a download that stalls at block 4 (server retransmits after 1000 ms of server time), `stopdl`, a malformed `userinfo`, a rate-limited download with `retransdl 0`, path variants (`../main/iw_05.iwd`, `mods/dl/../dl/zz_dl.iwd`), and finally a wrong `nextdl` (dropped, `broken download`).

### `net_dl_www_mp_toujane` (sv_allowDownload 0, sv_wwwDownload 1, sv_wwwBaseURL `dl.example.invalid/cod2`, sv_wwwDlDisconnected 1)
(`//` in a command-line value starts a console comment, so the URL has no scheme.)
- A (cl_wwwDownload 0): plain download refused: `EXE_AUTODL_SERVERDISABLED_PURE\x15<name>` (sv_pure 1); a second refused name.
- E (cl_wwwDownload 1 through `userinfo`): redirect of a missing file (`couldn't extract file size`, then the plain path refuses), redirect (`svc_download` block -1 + URL + size + flag), `wwwdl ack`, a duplicate ack (`WARNING: dupe wwwdl ack`), `wwwdl chkfail` (warnings, wwwFallback, gamestate resend), `download` again (www skipped once, plain path refuses), `download` again (redirected), `wwwdl ack`, `wwwdl fail` (fallback + gamestate resend), `wwwdl done` while not downloading (dropped: `PC_PATCH_1_1_UNEXPECTEDDOWLOADMESSAGE`).
- F: redirect, ack, `done`, redirect, ack, unknown subcommand (dropped). B: `wwwdl ack` without a redirect (dropped).

### `net_dl_voice_tdm_toujane` (tdm, sv_voice 1, sv_maxclients 64)
Voice is not a usercmd op: it is the connectionless packet `ffffffff 'v' 00 <qport:u16> <count:u8> {<len> <data>}*` [O] (`SV_VoiceConnectionlessPacket` 0x08094b56). `len` is a byte when the sender is active (`SV_ParseVoicePacketsLocal` 0x0809c2ee, relayed through `G_SayToEligibleClients`) and a 16-bit short when the sender is not yet active (`SV_ParseVoicePackets` 0x0809c3c6, relay to every client with state > 1). The relay reply to a listener is the OOB packet `'v' 00 <count> {<talker> <len:u8> <data>}*` (`SV_SendClientVoiceData` 0x0809c0a2), captured by the plugin per client in `scenario_notes.oob`.
- A, B, C, D (A and C allies, B and D axis; D keeps cl_voice 0; the others switch cl_voice on with a `userinfo` command): team-only delivery, `voice_global 1`, `voice_localEcho 1` (A hears itself), three packets in one datagram, `muteplayer` / `unmuteplayer` (in range, out of range, no argument), size 0 (`Received invalid voice packet of size 0`), size 255, the dead talker with `voice_deadChat` 0 and 1 after A's `kill`, `rate 4000` (cl_voice 1 but rate < 5000 means no voice), `sv_voice 0` (packets ignored) and back to 1.
- E (connecting-state talker): 16-bit-length packets through the relay path (two packets, sizes 0 and 300 invalid, 256 boundary), delivered to every active client with cl_voice 1.

## Determinism controls

- Seed pinned as in the duel. Menu responses staggered per player (without it the 4-client case showed one reconstruction run with a different join/spawn order: packet arrival order between sockets is not server time) [O].
- `send_cp` in `gameplay.py` reads every referenced iwd central directory in pure Python (0.1-0.3 s per gamestate). One Python thread serves all fake clients, so a gamestate resend to E or F stalled A and B and shifted their `commandTime` by a wall-clock-dependent amount (observed: original -150..-300, recon -50 around T0+7500 and T0+10600, first www runs) [O]. The connecting-state clients therefore keep a working copy of the gamestate without `sv_referencedIwdNames` (the recorded gamestates keep the server's bytes), so their `cp` carries no iwd checksums.
- The userinfo command sends `challenge 0` (the connect challenge is a libc `rand()` value).
- `sv_maxclients 64` in the voice case, see "Original defect" below.

## Fixes

1. `SV_RedirectClientDownload` 0x0808fc0c (chunk_020): `Com_Printf("ERROR: Client '%s': couldn't extract file size for %s\n", name)` has no second argument at 0x0808fd4b; the second `%s` reads stale stack at `[esp+8]`. The original printed an empty string in 3/3 runs; the reconstruction read NULL and printed `(null)`. Evidence: server.log line 138 of the www case, original vs recon. The fix passes `""` explicitly. [I] The residue is not a defined value; it matches in this call sequence only.

Everything else the cases reach (download state machine, block window, rate/sv_maxRate clamp, denial texts, www redirect, wwwdl subcommands, `SV_SendClientGameState`, voice parse, queue, mute list, `G_SayToEligibleClients`, OOB voice send) matched without change.

## Original defect found (not reproduced as a case)

[O] With `sv_maxclients 8` the relay loop of `SV_ParseVoicePackets` (0x0809c3c6, `for j = 0; j < 0x40`) reads `svs.clients[j]` for slots beyond the allocation. In the first voice run a connecting-state talker made the original read garbage as slot 11 (`state=808203577`, `voicePacketCount=-466100`, `sendVoice=118`) and crash with SIGSEGV in `SV_QueueVoicePacket` 0x0809c27f (gdb.log, backtrace 0x0809c27f / 0x0809c526 / 0x08094c5b). The slot contents are heap garbage, so this is neither deterministic nor comparable with the reconstruction. The voice case runs with `sv_maxclients 64`, where the 64 slots exist and the relay is well defined.

## Uncovered

- A real HTTP download (nothing listens; `wwwdl done` after a genuine transfer is only reached as "unexpected" in the cases above, and `done` in the redirect state only for F).
- The `sv_wwwDlDisconnected 0` value and `sv_wwwBaseURL` with a scheme (see the comment handling above).
- Voice from `SV_ParseVoicePackets` with sv_maxclients below 64 (undefined behaviour in the original).
- Muting by a connecting-state client; `voice_global`/`voice_deadChat` combinations with spectators; sv_voiceQuality (only an info dvar here).
- Downloads of an iwd that the server references for sv_pure (the generated iwd is not referenced; the gamestate's `sv_referencedIwdNames` is unchanged), so the real "download, then reconnect with a matching `cp`" client flow is not emulated; E's `cp` lists no iwds.
