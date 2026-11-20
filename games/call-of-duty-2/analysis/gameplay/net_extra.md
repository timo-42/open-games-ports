# Gameplay lane net_extra: s2c reordering/delay, reliable-command overflow, loss across a map change, non-LAN rate/snaps

Tags: [O] observed, [I] inferred, [U] unknown. Plugin: `tools/oracle/scenarios/net_extra.py`. Original `cod2_lnxded` 1.3
(SHA-256 `05e774a3...a7`); pinned G_InitGame seed as in `gameplay-parity.md`. Related: `net_loss.md`, `net_clients.md`.

## Cases

| Case | Players | What it exercises | Original runs | Recon runs | Identical items | Mismatches |
|---|---|---|---|---|---|---|
| `netx_edge_dm_toujane` | A clean, B s2c delay/reorder, C deaf c2s | (1) generic `hold_s2c` hook: delay bursts and reordering of server->client packets; (2) reliable-command overflow | 3 (0 variable) | 2 | 64923 | 0 |
| `netx_rotloss_dm_toujane` | A clean, B lossy around the map change | (3) the second gamestate (after `scr_dm_timelimit` ends the map, rotation to mp_carentan, spawn pad) loses fragments and B's first c2s packets are lost | 3 (0 variable) | 2 | 123714 | 0 |
| `netx_rate_dm_toujane` | A LAN, B/C/D non-LAN 198.18.0.2/3/4 | (4) `rate` 1000..200000 and `snaps` 5..30 limiting, `sv_maxRate 6000`, run-time `userinfo` changes | 3 (0 variable) | 2 | 97015 | 0 |
| `duel_dm_toujane` (regression, `--tag=-net_extra`) | | | 3 (earlier) | 1 | 84560 | 0 |
| `netloss_play_dm_toujane`, `netloss_gamestate_dm_toujane` (regression) | | | 3 (earlier) | 1 | 76599 / 19401 | 0 |

## Harness additions (the only lane allowed to edit `gameplay.py`; everything is opt-in, existing cases are byte-for-byte unaffected)

- **`hold_s2c(p, raw)`** scenario hook (generic). Return `None` to deliver, or `("seq", S[, "after"])` / `("time", T[, "after"])`.
  The packet is held; it is released when a later in-band packet arrives with sequence >= S (`"seq"`), or when the player's latest
  snapshot server time is >= T (`"time"`). Held packets are delivered before the releasing packet in sequence order (a pure delay:
  the burst arrives together) or, with `"after"`, after it (reordering: the fakeclient then logs `out_of_order` and discards the
  older packet, as a Q3-style client does). Decisions use only sequence numbers and server time. Events `held_s2c` / `released_s2c`
  are logged in the client's `events.jsonl`. The loop body that processes one in-band packet became the nested function `inband()`.
- **`client_addrs`** (plugin spec, `{tag: ip}`): the fake client's socket binds to that source address. `run()` adds the addresses to `lo`
  inside the namespace (`ip addr add X/32 dev lo`) and, only for such cases, starts bwrap with `--unshare-user --uid 0 --gid 0 --cap-add CAP_NET_ADMIN`.
  [O] `Sys_IsLANAddress` 0x080d5638 treats 127.0.0.1 and private ranges as LAN; 198.18.0.0/15 (benchmark range) is not.
- **Fake authorize server.** [O] A non-LAN `getchallenge` is not answered until the authorize server accepts the client
  (`SV_RequestIpAuthorize` 0x0808d18e; without a reply the original refuses challenges for the first 20 minutes of uptime, see
  `oracle-client.md`). The sandbox has no DNS, so `cod2master.activision.com` is mapped to 198.18.0.100 by a private `/etc/hosts`
  bind mount and a thread in the session answers `getIpAuthorize <challenge> <ip> ...` with `ipAuthorize <challenge> accept 0 <guid> <pbguid>`
  (guid = 1000 + last octet) on UDP port 20700 (the server's `BigShort(0x50dc)`).
- **`userinfo`** (plugin spec, `{tag: {key: value}}`): connect-time userinfo overrides (rate, snaps).
- **`late_connect`** (plugin spec, `{tag: A server ms}`): the player starts its `getchallenge` from the main loop right after A's snapshot
  reaches that server time instead of during the sequential connect phase. [O] Needed for clients whose snapshot interval is a multiple of the
  50 ms frame (`snaps` < 20 or rate-limited): the snapshot phase is the parity of the server tick that handled the connect, which is wall-clock
  luck otherwise (first rate case runs: B's snapshots at x050 or x000, 20621 variable items; with `late_connect` 0 variable).

## 1. Server->client delay and reordering (`netx_edge_dm_toujane`, player B)

Windows are anchored at the first server message after B's snapshot time reaches 5000 / 9000 / 13000 (relative to T0 = 20000) and
counted by sequence number from there (27 held packets in total):
- `lag`: 6 consecutive messages held, delivered together (in order) when message base+6 arrives; `lag2`: 8 messages (offsets 10..17).
  B's ack and its usercmds then arrive late and in a burst; the server answers with deltas from older acked frames.
- `reorder`: every third message is released after its successor (10 packets): the client sees N+1 then N and discards N (13 `out_of_order`
  events incl. the deep ones).
- `deep`: three messages are released three packets late (they are discarded by the client).
- B's `say b0..b6` over these windows (reliable client commands resent until acked) all arrive once and in order [O: games_mp.log].
- [I] Reordering/late delivery is observable by the server only through the client's ack pattern, so server-side behaviour is
  covered by the compared snapshots (deltaNum, entity sets, `reliableSequence`).

## 2. Reliable-command overflow (`netx_edge_dm_toujane`, player C)

- C's c2s packets all vanish from T0+6000 (106 packets dropped), so its `reliableAcknowledge` stops. [O]
- rcon is no flood source: `SVC_RemoteCommand` 0x08097188 ignores any rcon within 500 ms of the previous one using the wall clock
  (`Com_PumpSysEvents`); a first attempt (A sends one `rcon say` per snapshot) delivered about one in ten, non-deterministically.
- Instead a loose-file overlay (`fs_game mods/netxflood`, never committed; the stock `dm.gsc` plus `thread netxFlood();`) sends C one
  `iprintln` (flag 0) and one `setClientCvar` (flag 1, `v nxN "vN"`) per server frame from server time 28000 (T0+8000), 160 iterations.
  Hash of the generated overlay file is in `harness.log`.
- [O] Behaviour in the original (`SV_QueueReliableCommand` 0x08094750): while fewer than 64 commands are pending both kinds queue; from
  64 pending `SV_CompactReliableCommands` runs and only flagged commands are queued (the `iprintln`s are dropped); at
  `reliableSequence - reliableAcknowledge == 0x81` the server prints `===== pending server commands =====` and the list, sends OOB `disconnect`, sets the
  pending drop reason `EXE_SERVERCOMMANDOVERFLOW` and queues `w "EXE_SERVERCOMMANDOVERFLOW"`; the client is dropped with the next message
  (`2:fakeC EXE_SERVERCOMMANDOVERFLOW`), A and B get `e "fakeC^7 EXE_SERVERCOMMANDOVERFLOW"` and `J 2`, `games_mp.log` `Q;0;2;fakeC`.
  C's last received command is `v nx94`. (the server log lists 129 pending commands.) The server's 128-slot ring (`& 0x7f`) and the
  `0x40` / `0x81` constants match the reconstruction.

## 3. Loss across a map change (`netx_rotloss_dm_toujane`)

- `scr_dm_timelimit 1.25` ends the map; `sv_mapRotation "gametype dm map mp_carentan"`; the run uses the same `SV_SpawnServer` pad (1.15 s)
  as `gt_tdm_rotate_dm_mp_toujane` (gdb, original and recon alike).
- B loses fragment 2 (start 1300) of the **second gamestate** (8 fragments, 9566 bytes), then the last fragment (start 9100) of the server's
  retransmission; the third transmission arrives [O: `dropped_s2c` x2, B's second gamestate at 85.73 s wall]. B's `cp` and first usercmds
  therefore start 1.5 s later than A's (B's first epoch-2 snapshot is at +3650 against A's +2100).
- B's c2s packets are dropped every other 50 ms between its relative +3900 and +5300 of the new level (14 packets).
- Both clients `say` on the new map; both disconnect.

## 4. Rate / snaps for non-LAN peers (`netx_rate_dm_toujane`)

- [O] `SV_SendMessageToClient` 0x0809aba2: non-LAN delay = `max(SV_RateMsec(len), snapshotMsec)`; `rateDelayed` = 1 when the rate term wins (visible as
  `snapFlags` bit 1, value 5), connecting/zombie clients wait at least 1000 ms between messages, `sv_maxRate` caps the rate.
- Profiles (connect values, then run-time `userinfo` changes at T0 + ms): B 2500/10 -> 1000/30 (9000) -> 5000/10 (14000) -> 25000/20 (20000);
  C 5000/20 -> 2500/20 (12000); D 5000/10 -> 2500/10 (16000) -> 200000/5 (22000, stored as 90000). `sv_maxRate 6000`.
  [O] `rcon status` at the end lists the stored rates 25000 / 2500 / 90000 for B / C / D.
- [O] Snapshot intervals (run 1): A (LAN) 50 ms always; B 100 ms (rate 2500/snaps 10), 150..450 ms with `rate delayed` flag set while rate 1000, back to 50 ms after 25000/20;
  C 50 ms with isolated 100 ms steps under rate 2500; D 100 ms, then 200 ms after snaps 5.
- The three non-LAN clients and A fight at one place (`setviewpos` to A's mark), so snapshots are large enough for the rate to matter.

## Determinism controls

- All drop/hold/flood decisions depend on the deciding client's own snapshot time, sequence numbers relative to such a trigger, or the
  server's frame clock (`gettime()` in the overlay). `swallow_keepalive` removes the harness's wall-clock resend for the lossy and non-LAN clients.
- `sv_zombietime 1` (as in net_loss) keeps the final `rcon status` free of zombie `lastmsg` values.
- `late_connect` phase-locks non-LAN joins to A's snapshots (above); the fake authorize server answers within the same server tick pair, so the
  `challengeResponse` arrives deterministically a frame later.
- Item counts: see the table; all three cases had 0 variable items over 3 original runs.

## Fixes (original addresses)

1. **`MSG_WriteDeltaPlayerState` 0x0806a200 (chunk_009)**: the reconstruction decided "field unchanged" with a **float** compare
   (`fromValue == toValue` on every netfield, including integer fields) and tested the angle fields for `toValue == 0.0f`. The original compares
   the raw dwords (`cmp eax,[edx]` at 0x0806a316, 0x0806a29b for the last-changed scan) and tests the raw angle dword for zero (0x0806a447);
   the float path truncates (`fistp` with RC=truncate, 0x0806a377) instead of rounding. Effect [O]: a playerstate field whose dword changes while its float value
   compares equal (e.g. -0.0 vs +0.0, or a NaN pattern) was written as changed (and, for ints, as value) in the original but not in the recon, so
   the compressed message was 1-2 bytes shorter (client state identical!). Invisible on LAN peers; for non-LAN peers the length feeds `SV_RateMsec`
   and shifted a rate-limited snapshot by one frame (C at server time 47450 vs 47500, `deltaNum`/flags identical before). Located by comparing C's
   `payload_len` per message sequence (seq 214: 50 vs 49) and decoding the first differing message (`decoded.jsonl`). Class: int-vs-float reinterpretation.
2. **`Area_SweepBoxFromPoints` 0x0805ec2c (chunk_005)**: the segment end points are vec4s (`start[3] = 0` at 0x0805ec8a, `end[3] = *result` at 0x0805eca7);
   the reconstruction declared `float start[3], end[3]`, so `Area_SweepBoxRecords` 0x0805e9ca read stack garbage (the contents mask 0x2802831) as the running
   fraction `seg[3]` in `if (*frac <= seg[3]) break;`. Effect [O]: static-model (xmodel) boxes of the area tree were skipped for some bullet traces
   (D's mp40 bullet at server time 33350 hit the world at fraction 0.18 instead of the static model at fraction 0.096; impact entity position, normal and `eventParm`
   differed; 308 mismatches in `netx_rate`). Located by tracing the first diverging `G_TracePointEx` result (188 bullet traces, #76), then the `Box_SegmentSlabTest`
   / `Area_SweepBoxRecords` call sequence with `calltrace.py` and `seg[3]` read at 0x0805eb00 (0 in the original, 0x2802831 in the recon).
   The earlier lanes never fired at a static model close to the line of fire.
3. **`SV_RequestIpAuthorize` 0x0808d18e (chunk_020)**: the `"%s resolved to %i.%i.%i.%i:%i"` message printed `ip[0]` sign-extended (`-58.18.0.100`); the original
   zero-extends all four bytes (movzx at 0x0808d3d9..0x0808d3fa). Only visible in `server.log`.

## Not covered / limitations

- [U] Server-side effects of s2c delivery order are only observable through the ack pattern; a client that acks a reordered packet individually does not exist
  in the fakeclient (it discards `seq <= in_seq`). Real-time delay (ms of wall clock) is deliberately not modelled.
- [U] Rate limiting of a client that is also downloading (`SV_WriteDownloadToClient` with `downloadName`), `sv_maxRate` below 1000 (clamped by `Dvar_SetInt`), `sv_debugRate` output
  and `cl_maxpackets`-style c2s rate are not driven. The rate case never reaches fragment-bound snapshots for the non-LAN clients (all snapshots stay below one fragment).
- [U] Loss during the *first* part of a map change (the `map_restart`/`disconnect` server commands, the `cp` command) is covered only through B's lost second-gamestate fragments;
  there is no case where the client times out during loading (`sv_timeout` during `SV_SpawnServer`).
- [I] Overflow with only flag-1 commands (no compaction branch) or with 0x81 reached in one frame is not separate; the 0x40 compaction path (flag 0 dropped) and the 0x81 overflow are both in `netx_edge`.
- 100.64.0.0/10 was not tried; 198.18.0.0/15 is non-LAN in the original (`rate` case).
