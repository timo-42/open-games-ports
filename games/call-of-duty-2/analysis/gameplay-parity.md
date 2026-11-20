# CoD2 1.3 Linux server: gameplay parity (oracle and reconstruction)

Lane: gameplay parity. This lane extends the connection-level oracle (`oracle-client.md`, `boot-parity.md`) to spawned players: movement, weapons, damage, kills, killcam, respawn, scoring, several clients, map rotation and server-side test clients. It then drives the reconstruction (`.local/cod2/build/cod2_lnxded_recon`, built by `tools/link_test.py`) to parity with the original `cod2_lnxded` 1.3 (SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`).

Tags: [O] observed in a run on this host, [I] inferred, [U] unknown. All server output is treated as data.

## Archived capture recheck (2026-11-20)

Recomparing the 42 sets of three original and two C++ captures with complete item values gives **40 matching scenarios and 15 differences** across 5,572,274 deterministic items. The 2026-11-18 comparison below used a 200-character display sample as its equality check for longer values. All 15 differences occur after that cutoff. Six are entity trajectory observations in `wpn_grenade_fragsmoke_dm_toujane` and nine in `wpn_grenade_kill_tdm_carentan`. For example, at +8400 ms client A observed Toujane entity 72 with `pos.trDelta[0]` of −279.49115 on the original and −279.491119 on the reconstruction. The oracle now compares the complete value before formatting the diagnostic. This recheck used the saved runs of the previously identified binaries.

## Result (2026-11-18)

All scenarios below are deterministic on the original across 3 runs. On the final reconstruction build they show **0 mismatches** on every deterministic item.

| Case | What it exercises | Original runs: identical / variable items | Reconstruction runs | Mismatches |
|---|---|---|---|---|
| `duel_dm_toujane` | Exercises the following, all keyed to absolute server time:<br>- two clients pick team and weapon through the stock GSC menus (`mr`) and spawn;<br>- forward walk, 90° turn, jump, crouch, prone, fire, reload, weapon switch (thompson → webley → thompson);<br>- `setviewpos` (devmap): the two players face each other;<br>- A aims from snapshot data and fires; B is killed;<br>- killcam (archived snapshots), B respawns with the use button;<br>- `score`, `say`, A `kill` (suicide), `rcon status`, disconnect B then A. | 84560 / 0 | 3 | 0 |
| `rotate_dm_toujane` (re-captured with the built-in spawn pad, see below) | The duel, then:<br>- `scr_dm_timelimit 1.25` ends the map: `endMap`, `W` log line, intermission, `wait 10`;<br>- `exitLevel` → `map_rotate` loads mp_carentan (`sv_mapRotation`);<br>- both clients receive a second gamestate (serverId 32) and are respawned by script;<br>- on the second map they move, jump, request `score` and chat. | 128505 / 2 variable, 24 missing in some runs (map-load boundary) | 2 | 0 |
| `bots_dm_toujane` | Option B:<br>- two `addtestclient` bots join through a test overlay of `dm.gsc`, pick teams/weapons via `TestClient` and play with `SV_BotRandomUserCmd`: they move, fire, create impacts and switch weapons;<br>- two fake spectators observe. | 29355 / 0 | 2 | 0 |
| Regression: `client_dm_toujane`, `client_sd_carentan`, `client_restart_dm_toujane` (fakeclient.py) | Connection-level cases from `oracle-client.md`. | 2759, 2814, 3079 | 1–2 | 0, 0, 0 (the map_restart step is now a timing family in `fakeclient.py`) |

The items are as follows:
- For each client and each snapshot (keyed by server time relative to T0, or to the second level's start), every non-zero playerState field, bit-exact: floats as `%.9g`, including `stats[]`, `ammo[]` and `ammoclip[]`.
- The raw playerState hash with commandTime made relative.
- Entity creation, changes and removals, with every field bit-exact.
- clientState changes and entity count.
- Every reliable server command, per client.
- The client's command log.
- Gamestate 2: serverId, configstrings and baseline hashes.
- `server.log` and `games_mp.log`, normalised.
- The rcon replies.

## Final verification (2026-11-18, all 42 cases)

[O] All 42 gameplay cases were recaptured under one harness version against one merged reconstruction build.
- Build: `.local/cod2/build/cod2_lnxded_recon`, SHA-256 `16f929de9c70561ba7b46ddb11a37bfe49579f4229311b2eae766e1b457ab1cb`, from `link_test.py` with 0 undefined.
- Runs: 3 original and 2 reconstruction runs per case, 8 cases at a time. Scripts: `.local/cod2/verify/recapture_all.sh` and `resume_all.sh`.
- Result: **0 mismatches on deterministic items in all 42 cases**, over 5,572,274 identical items.
- Original determinism: 41 cases are 100% identical across their original runs. `net_clients_dm20_mp_toujane` has 256 variable and 39 missing items (12-client harness jitter; see `gameplay/net_clients.md`).
- Earlier original captures are kept as `gameplay_<case>-h1`; the lanes' map-change captures as `-slowcp`.

Harness changes made during this verification. They affect client timing, which is why every case was recaptured on both servers:
- **iwd checksums are cached.** The CRC blob is parsed once per file and warmed before T0, and `send_cp` computes only the pure checksum. Before this, each gamestate stalled all scripted clients for 0.1–0.3 s while 20k-entry central directories were parsed. At a map change that delayed usercmds by up to 5 frames.
- **Staggered entry.** At the initial connect and after every map change, a client's cp+enter is held until every connecting client has the gamestate. The clients then enter one at a time in player order, each once the previous one is active.
  - Safety valve: 2000 ms of server time, or 10 s of wall time.
  - Without it, the order of ClientBegin (J log lines, `MP_CONNECTED` commands) depended on which socket Python read first.
- **Map-change cutoff in `compare`.** Per client and per epoch ≥ 2, records are compared from one record after the latest first record over all runs (`map_change_cutoffs`). Before that point a client's activation frame is wall-clock dependent, and its first record diffs against the previous epoch.
- **Rotation spawn pad** (see below), now built into `gameplay.py run` for every case with a map rotation.

The 17 round-2 cases (lanes end_sd, end_teams, wpn_rockets, wpn_turret2, vote_pass, net_extra):

| Case | Lane notes | Exercises |
|---|---|---|
| `end_sd_roundlimit_mp_toujane`, `end_sd_roundlimit3_mp_toujane`, `end_sd_timelimit_mp_carentan` | `end_sd.md` | SD round limit, time limit, round-length expiry, last-round wins, final intermission |
| `end_teams_tdm_ctf_mp_toujane`, `end_teams_ctf_hq_mp_downtown`, `end_teams_hq_hold_mp_toujane` | `end_teams.md` | Score and time limits in tdm/ctf/hq; ctf touch-return and 120 s autoReturn (mp_downtown); hq hold cap |
| `wpn_rockets_rocket_dm_toujane`, `wpn_rockets_snipe_tdm_carentan` | `wpn_rockets.md` | Panzerschreck/bazooka missiles and splash kill (test overlay enabling launchers); scoped sniper ADS; grenade pickup; non-lethal melee |
| `wpn_turret2_tdm_toujane`, `wpn_turret2_ladder_toujane`, `wpn_turret2_mantle_toujane` | `wpn_turret2.md` | Two users on one MG42, turret kill and killcam in tdm; ladder top dismount and jump-off; crouched and failed mantles. Overheat does not exist in this binary (evidence in notes). |
| `vote_pass_tdm_toujane`, `vote_pass_typemap_toujane`, `vote_pass_script_toujane` | `vote_pass.md` | Passing votes that execute map, gametype, typemap, map_restart and kick; `g_oldVoting 0`. GAME_MAXVOTESCALLED is unreachable (voteCount is never incremented). |
| `netx_edge_dm_toujane`, `netx_rotloss_dm_toujane`, `netx_rate_dm_toujane` | `net_extra.md` | s2c reordering and delay (`hold_s2c`), reliable-command overflow, loss during a map change, rate/snaps limiting for a non-LAN peer (198.18.0.0/15 source address in the namespace) |

Round-2 reconstruction fixes:
- `MSG_WriteDeltaPlayerState` (0x0806a200): compares raw dwords, not floats.
- `Area_SweepBoxFromPoints` (0x0805ec2c): uses vec4 start/end.
- `WeaponView_ApplyIdleSway` (0x080f1ede): float slots.
- `Player_DropWeapon` (0x081064de): the spread block is inside the aim branch.
- `XAnim_SetSlotWeight` (0x080c20de): float 0.001f.
- The `SV_RequestIpAuthorize` log line.

## Coverage extension (2026-11-18)

Twelve lanes, one scenario plugin each in `tools/oracle/scenarios/`, added the cases below. Each lane recorded its scenarios, determinism controls, fixes and the paths it left uncovered in `analysis/gameplay/<lane>.md`; the rules they followed are in `recon/GAMEPLAY_LANE_BRIEF.md`.

[O] Final verification by the coordinator: all 25 gameplay cases were re-run once each on one merged build of all fixes (`link_test.py`, 0 undefined), 8 cases at a time, and compared against the 3 original runs per case. Result: 0 mismatches on deterministic items in every case. The one exception at first was `rotate_dm_toujane`, which is now padded; see below.

| Case | Lane notes | Exercises | Identical items |
|---|---|---|---:|
| `gt_sd_rounds_mp_toujane`, `gt_sd_rounds_mp_carentan` | `gt_sd.md` | Search & Destroy rounds: bomb plant, explosion/defuse, round end, next round, intermission scoreboard | 164579, 181824 |
| `gt_tdm_teams_mp_toujane`, `gt_tdm_rotate_dm_mp_toujane` | `gt_tdm.md` | TDM: autoassign, enemy kill, team kill with friendly fire, team score; rotation tdm → dm | 221204, 123909 |
| `ctf_flag_cycle_mp_toujane` | `gt_ctf.md` | CTF: pickup, carry, capture, drop on death | 97359 |
| `gt_hq_capture_mp_toujane`, `gt_hq_fight_mp_toujane` | `gt_hq.md` | HQ: radio spawn, capture, hold scoring, destroy, relocation; corpses | 163354, 167735 |
| `wpn_grenade_fragsmoke_dm_toujane`, `wpn_grenade_kill_tdm_carentan` | `wpn_grenade.md` | Frag/smoke throws, cooking, bounce, explosion, grenade kill | 70027, 79972 |
| `wpn_melee_ads_lean_ads_lean_dm_toujane`, `wpn_melee_ads_lean_melee_dm_toujane` | `wpn_melee_ads_lean.md` | ADS, lean left/right, melee kill, weapon sway/bob | 89443, 76768 |
| `wpn_mantle_ladder_toujane`, `wpn_turret_mg42_toujane` | `wpn_mantle_turret.md` | Mantling, ladder, MG42 turret mount/aim/fire/dismount | 113918, 70087 |
| `social_vote_tdm_toujane`, `social_spec_tdm_toujane` | `social_vote.md` | callvote/vote, spectator follow, quick messages, say_team/tell, team change | 221184, 119239 |
| `net_clients_tdm8_mp_toujane`, `net_clients_dm20_mp_toujane` | `net_clients.md` | 8 clients in TDM; 20-slot DM with 12 clients; server-full rejection; anti-lag | 323591, 411327 |
| `netloss_play_dm_toujane`, `netloss_gamestate_dm_toujane` | `net_loss.md` | Deterministic c2s/s2c loss in play; fragment loss in the gamestate; retransmission | 76599, 19401 |
| `net_dl_dl_mp_toujane`, `net_dl_www_mp_toujane`, `net_dl_voice_tdm_toujane` | `net_dl_voice.md` | iwd download blocks, wwwDownload redirect/refusal, voice relay | 46906, 54679, 153984 |

Defects fixed in this round (details, original addresses and evidence are in the lane notes):
- **Scripts:** `Scr_AddEntityFieldValue` (0x0811af3c) read script string ids and object handles sign-extended. With an id ≥ 0x8000, `self.pers` broke: teams got swapped and `pers["team"]` was lost in 8-client runs.
  - [O] Retested afterwards: 4 runs of `net_clients_tdm8` without its `fs_game` workaround (`+set fs_game main`) against 3 new original runs show 0 mismatches.
- **Intermission:** the scoreboard flush (0x08109616) tested `commandTime` instead of `pm_type == PM_INTERMISSION`.
- **Corpses:** the settle trace lerp was missing (0x08107846).
- **Trajectories and float precision:**
  - `BG_EvaluateTrajectory`/`Delta` use a float time scale with extended-precision gravity.
  - Grenade bounce timing truncates with `fistp`.
  - Vector dot/length sum in x, y, z order.
  - `PM_CmdScale` uses float stance constants.
  - The degree-to-radian constant is the double value.
- **Weapon view:** gun-kick and sway slots are floats, not ints. The integer reading made the spring loop stall the server for about 400 ms after each shot.
- **Melee:** the decompiler had split the orientation matrix in `G_UpdateWeaponTargetLock`.
- **Mantle and turrets:**
  - Mantle ledge probes and velocity.
  - `XAnim_NodeDurationMs` truncation.
  - Turret yaw index, aim direction, and the matrices in `Turret_PositionUser`.
  - Turret event surface type.
- **Anti-lag:** the client rewind condition never ran (0x0811fc76).
- **Traces:** the surfaceFlags copy in `SV_ClipMoveToEntity_Line`.
- **Downloads:** a missing `Com_Printf` argument in `SV_RedirectClientDownload`.

Harness changes:
- **Scenario plugins.** Scenarios live in separate plugin files and support N players and deterministic packet-loss hooks.
- **Rotation spawn pad.** Every rotation case pads the second `SV_SpawnServer` to 1.150 s under gdb on both servers (`SPAWN_PAD_S`; `--no-spawn-pad` turns it off).
  - Without it the wall-clock map-load time (original 916–1033 ms; reconstruction about 915 ms after this round's fixes) decides which 50 ms frame the first epoch-2 snapshot falls in.
  - That moved every epoch-2 record: 1154 mismatches, all timing.
  - The unpadded original captures are kept as `gameplay_rotate_dm_toujane-nopad`.
- **Zombie rows.** The lastmsg column of ZMBI rows in rcon status is masked as wall-clock.
- **Disconnect freeze.** Server commands are frozen when each client disconnects.

## Tooling

### `tools/oracle/gameplay.py`
Usage:
```
python3 games/call-of-duty-2/tools/oracle/gameplay.py run duel_dm_toujane 1 [--server recon] [--seed N|none] [--tag=-x] [--gdb-extra FILE]
python3 -I games/call-of-duty-2/tools/oracle/gameplay.py compare .local/cod2/oracle/captures/gameplay_duel_dm_toujane .local/cod2/recon-run/captures/gameplay_duel_dm_toujane [-v] [--json OUT]
```

Sandbox:
- The server and both clients run inside one `bwrap --unshare-net --unshare-pid --unshare-ipc` namespace with a read-only root and loopback only, as in `fakeclient.py`. `isolation.txt` in every capture lists only `lo`.
- Game data is read through symlinks only.
- Each server/case/tag gets its own `fs_homepath` (`oracle/run/home/gameplay_<case><tag>-<server>-run<N>`). The reason is that `g_log` appends to `<home>/<fs_game>/games_mp.log`, and a shared home mixed concurrent runs.

Captures:
- Original: `.local/cod2/oracle/captures/gameplay_<case>/run<N>/`. Reconstruction: `.local/cod2/recon-run/captures/gameplay_<case>/run<N>/`.
- Contents: `summary.json`, `clientA/` and `clientB/` (with `packets.jsonl`, `decoded.jsonl` and `events.jsonl` as in fakeclient), `server.log`, `games_mp.log`, `gdb.log`, `seed.gdb`, `isolation.txt`, `harness.log`, `SHA256SUMS`.

Clients:
- Two `fakeclient.Client` instances: client ports 28971/28972, qports 7467/7468, names `fakeA`/`fakeB`.
- `fakeclient.Client` now accepts a client port, a qport and an explicit usercmd. Its keep-alive mode re-sends the last usercmd serverTime, so a keep-alive never shifts commandTime.

Usercmds:
- `cod2proto.write_usercmd_full` writes every usercmd in the reader's long form with all "changed" bits forced. The server's first delta base is `CL_CreateCmd(ps)` (0x080689a6), which the client cannot know exactly, and with this encoding it does not matter.
- Field order and keys were checked against `MSG_ReadDeltaUsercmdKey` 0x08068f9e:
  - serverTime long; keyed changed/long-form bits; buttons bit 0;
  - angles[0..1] as keyed shorts; move class (4 bits);
  - `key ^= serverTime`; angles[2]; `buttons >> 1` (18 bits); weapon (7); offHandIndex (7).
- `weapon = 0` makes `SV_UserMove` substitute `ps.weapon`.
- Button bits, inferred from pmove and weapon readers [I]:

  | Bit | Meaning |
  |---|---|
  | 0x1 | attack |
  | 0x4 | melee |
  | 0x8 / 0x20 | use (`useButtonPressed` tests 0x28) |
  | 0x10 | reload |
  | 0x40 / 0x80 | lean |
  | 0x100 | prone |
  | 0x200 | crouch |
  | 0x400 | jump |
  | 0x1000 | ADS |

Menu responses:
- The client sends `mr <sv_serverid> <menu index> <response>`, as handled by `Cmd_MenuResponse_f` 0x08100d16.
- The menu index is the configstring index minus 0x4de. For mp_toujane: 1246 `ingame`, 1247 `team_britishgerman`, 1248 `weapon_british`, 1249 `weapon_german`.
- Weapon indices are positions in configstring 7.

Aiming:
- The yaw/pitch toward B's entity `pos.trBase` (+40 z) is computed from A's own snapshot (A's origin + `viewHeightCurrent`).
- The cmd angle is `ANGLE2SHORT(angle) - delta_angles`.
- Every decision uses only the deciding client's own latest snapshot, so it is a function of server state at a fixed server time.

Script timeline:
- The timeline of `duel_dm_toujane` is in `class Duel` (relative to T0 = server time 20000).
- After a map change, both clients re-anchor on configstring 13, the level start time ([O] 85200 in every run).

### `tools/oracle/calltrace.py`
This is the localisation tool used for every fix in this lane. It reads the `// 0xADDR size=` headers in `recon/chunks/*.cpp` to map original entry points to reconstruction symbols (`nm -C`).

Generated probes (one per binary):
- Each traced function gets an entry breakpoint, created disabled. Probes print the original address, the arguments (arity from the reconstruction's demangled signature), the return address, and optional pointer dumps (`--deref ADDR:ARG:COUNT`).
- A window opens at the n-th trigger hit that satisfies a gdb condition and closes at a stop function. Outside the window the traced breakpoints are disabled; without that, the hot XAnim and collision functions slowed the server to a crawl.
- Bss/data addresses in conditions are shifted by +0x2B880 for the reconstruction's data image.

Diff:
- `diff` aligns the two call sequences with difflib, so a helper that the original inlines does not derail the comparison.
- It masks pointer-like words.
- It reports the first matched call whose arguments differ.

Usage:
```
python3 games/call-of-duty-2/tools/oracle/calltrace.py gen --chunks 032 033 --trigger 0x080c1260 --stop 0x080b9dfa --stop-name DObj_CalcSkel \
    --when '*(int*)0x0864c56c == 35100 && ...' --out-dir DIR
python3 games/call-of-duty-2/tools/oracle/gameplay.py run duel_dm_toujane 1 --tag=-probe --gdb-extra DIR/trace_orig.gdb
python3 games/call-of-duty-2/tools/oracle/gameplay.py run duel_dm_toujane 1 --server recon --tag=-probe --gdb-extra DIR/trace_recon.gdb
python3 games/call-of-duty-2/tools/oracle/calltrace.py diff <orig gdb.log> <recon gdb.log>
```

Other techniques:
- Point probes: `gameplay.py --gdb-extra` with hand-written breakpoints, for example `G_TraceUseRay` results and DObj trace fractions.
- Memory dumps: `dump binary memory` of `g_xanimSlotRing` (0x085bc900, 0x28000 bytes) at several level times.

Script hashes (final): `gameplay.py` `4f68e407…1f1f`, `calltrace.py` `e4f8d84a…dd15`, `cod2proto.py` `c01875de…634a`, `fakeclient.py` `d56e363d…cd04`. The reconstruction binary used for the final runs was `e164e42d…a484`.

## Determinism

**Random seed.**
- [O] The seed is time-derived and no dvar controls it.
- `SV_InitMapState` (0x080925e6) calls `G_InitGame(svs.time, Sys_MillisecondsRaw(), ...)` (0x08109096). `G_InitGame` passes that value to both `srand()` and `Rand_SetSeed` (the game LCG, 0x080aaa76: `seed * 0x343fd + 0x269ec3`).
- `Rand_Int`, `Rand_Float` and `Rand_Fraction` feed the following:
  - script `randomInt` (spawn point choice, autoassign);
  - `SV_BotRandomUserCmd`;
  - item drops.
- Pinning: `gameplay.py run` (default `--seed 1234567`) starts the server under `gdb -batch` with `set exec-wrapper env LD_PRELOAD=... [LD_LIBRARY_PATH=...]`. A breakpoint at G_InitGame's first instruction overwrites `[esp+8]` before it is read.
  - The same breakpoint is used for the original (0x08109096) and the reconstruction (`_Z10G_InitGameijii` from `nm`).
  - `gdb.log` records every pin as `SEEDPIN levelTime=… seed=<time-derived> -> 1234567`; there are two after a map rotation.
- Without the pin (`--seed none`, 2 runs in `gameplay_duel_dm_toujane-noseed`), the spawn points differ: (858,742) vs (2932,1148). Hit locations differ as well, and 17076 items vary.

**Client timing.**
- One usercmd is sent per received snapshot, with usercmd serverTime equal to that snapshot's serverTime. The command arrives before the next 50 ms frame, so `commandTime = serverTime - 50` regardless of the client/server clock phase.
- Every scripted action fires at the first snapshot whose serverTime has reached the scheduled time.
- The 100 ms keep-alive used to fire spuriously while traffic was flowing, and its `+1` serverTime made commandTime `-49` in some runs. It now only fires after 100 ms of silence and re-sends the previous serverTime, which the server ignores.

**Remaining variability**, masked or excluded and listed by `compare -v`:
- The `J` line timestamp in `games_mp.log`. A client's "begin" happens at its wall-clock connect time before T0. It is masked as `<CONNECT-TIME>`: the original was at 0:05 or 0:06 depending on the batch; the reconstruction at 0:05.
- Ping and lastmsg columns of `rcon status`, and the ping fields of the scoreboard command `b <count> <t0> <t1> (<client> <score> <ping> <deaths> <killcam>)*`. They are masked as `<PING>` / `<LASTMSG>`.
- `w "EXE_DISCONNECTED"` to A after its own disconnect. Whether it is captured depends on when the client stops listening, so it is missing in some runs and excluded.
- Bot case: client connects consume libc `rand()` (`getchallenge`), and bots consume it every frame. If bots were added at boot (`scr_testclients 2` on the command line), connect timing interleaved with the bots' random usercmds and 2609 items varied. The fix is to start the server with `scr_testclients 0` and have client A send `rcon set scr_testclients 2` at T0-5000. The `addTestClients` poll (`wait 1`) then adds the bots at level 0:15 in every run.

**Pre-T0 history.** The archive ring is pre-T0 history that can differ between the two servers: the frame in which a client connects depends on the wall clock. [O] It is not visible after T0: killcam replays only frames from the last few seconds, and full archived frames recur at least once per `sv_fps` frames.

## Fixes (by error class)

Each fix was located with the probes above and checked against the original disassembly at the cited addresses. Every commit is titled `cod2 gameplay: …`. `difftest.py` was run where the harness can drive the function. For the others it reports `unsupported` (opaque struct pointers need a spec, or the original segfaults on synthetic pointers), and validation is the gameplay oracle.

**1. Locals that the original keeps contiguous, split by the decompiler**
- `SV_ClipMoveToEntity_Line` 0x0809cf08 and `SV_ClipLineHitsEntity` 0x0809d4ea (chunk_023):
  - The original copies `currentOrigin` into the origin row (+0x24) of the 4x3 axis matrix at `[ebp-0xc8]`/`[ebp-0xf8]`, and `Vec3_WorldToLocal` reads `xform + 9`.
  - The reconstruction kept a separate `org[3]` and a 9-float axis, so local ray coordinates were NaN/garbage.
  - Effect: bullets and look-at rays passed through players.
- `XAnim_BuildPartMap` 0x080bce5e (chunk_032):
  - The 128-bit mask at `[ebp-0x98]` is followed directly by the per-part slot map at `[ebp-0x88]`, indexed by part and initialised to 0x7f. `ScrStr_Intern` copies `part_count + 0x10` bytes from the mask.
  - With separate arrays the interned map was zero, so every animated track blended into bone 0 and player poses stayed at identity.
- `Ent_GetAttachTransform`, `Ent_SyncAttachTransform`, `Ent_SetAttachCurrent`, `Ent_SetNodeFromSlot` and `Ent_SetNodeOrigin` (0x0811dfca..0x0811e5fa, chunk_052):
  - One 4x3 matrix at `[ebp-0x48]` with the origin at +0x24.
  - The reconstruction used `angles_tmp[9]` + `origin_tmp[6]`, so `Vec3_TransformAffine` and `Mat34_ConcatRowVector` read stale stack.
  - Effect: dropped weapons spawned at an old position and A's pickup cursor hint (22) never appeared.

**2. Wrong types or signatures**
- `SV_DObjBindingSetTime` 0x080920d0 (chunk_021, prototype):
  - The original forwards the raw float dword `[ebp+0xc]` (G_RunFrame passes `level.frameTime * 0.001f`).
  - An `int` parameter truncated 0.05 to 0, so no animation blend or clip time advanced.
  - Located with a `g_xanimSlotRing` dump diff (first difference between level times 21100 and 21200) and then the call trace (`XAnim_BindingSetTime` arg 0.05 vs 0).
- `MSG_ReadDeltaField` 0x08069960 (chunk_008):
  - Rewritten from the disassembly. Unchanged fields, the long of the float encodings and every integer field are moved as raw dwords. Only the 5/2-bit small-integer floats use `fild/fstp`, and the angle encoding stores `MSG_ReadAngle16`.
  - The float-typed translation turned archived entity integers into float bit patterns (`pos.trTime` 29750 → 0x46E86C00). Killcam snapshots carried garbage.
  - This function is only used server-side for archive reads, which the connection-level oracle never reached.

**3. Inverted branch**
- `SV_GetCachedSnapshot` 0x0809907a (chunk_022):
  - The writer emits bit 1 + time for a full archived frame and bit 0 + base frame + time for a delta frame. The reader branches with `jne full` at 0x080992b5.
  - With the test inverted, killcam playback recursed through garbage base frames and the server hung in `SV_FindCachedSnapshotByAge`. The backtrace came from the runner's hang handler.

**4. Decompiler placeholder left in an argument**
- `Bullet_Fire_Extended` 0x0811fe90 and `Bullet_FireFromEntity` 0x0811f844 (chunk_052):
  - G_Damage's hit location is `movzx word [ebp-0x18]`, which is `trace.partGroup` (trace at `[ebp-0x38]`, +0x20; 0x081202ff / 0x0811f97d).
  - The reconstruction passed `hitLocation = 0 /* left unset by the decompile */`, so every hit was logged and scaled as `none`.

**5. x87 precision (1–2 ulp)**
Located with exact float records and call-trace argument diffs:
- `cm_wedge_sphere_sweep` 0x08056490 (chunk_004): the plane-contact fraction divides by the float spill `diff` at `[ebp-0xcc]` (0x08056782). The reconstruction re-derived `start_dist - end_dist` in extended precision.
- `DotProduct` 0x080562b2 and `VectorLengthSquared` 0x080562e6 (chunk_003, prototypes):
  - The original sums x, y, z on the x87 stack and returns st(0) unrounded.
  - The reconstruction summed z, y, x and spilled through a float return slot (`fstp/fld`).
  - Effect: collision distances were off by an ulp, so spawn heights (B z 72.2032166 vs 72.2032318) and slope steps were wrong in 274 snapshots.
  - Both functions now return `long double`. `difftest`: 2000/2000 pass each.
- Item randomness (chunk_048):
  - `Player_DropWeapon` (0x081069fe, 0x081066c6), `Item_ThrowFromPlayer` (0x081063d4) and `Item_PickupWeapon` (0x08105072) multiply the unrounded `Rand_Signed()`/`Rand_Fraction()` st(0) by float constants (60/40/50, 0.5, 0.25) or dvars before the first float store. They round through 0x081081ee (`Float_RoundToInt2`).
  - The reconstruction rounded each random value to float first, which moved the dropped weapon's `apos.trDelta` by an ulp.

**6. Field confusion**
- `FS_filelength` 0x0809e8a6 (chunk_023): the original tests `fsh.iwd` (+0x14, `cmp [slot+0x849fd94]`). The reconstruction tested +0x04, which is the uniqueFile flag that `FS_FOpenFileReadInternal` stores for loose files too. Loose files opened with uniqueFile=1 got a garbage length, so `fs_game` scripts gave "Could not find script".

## Option B: server-side test clients

[O] The binary has `addtestclient` only as a GSC builtin (`GScr_addtestclient` 0x08116bd8 → `SV_AddBot`). There is no console command. The userinfo template is `connect "\cg_predictItems\1\...\name\bot%d\protocol\%d"`.

[O] Every stock gametype (dm/tdm/sd/ctf/hq in iw_07/iw_15) has `//thread maps\mp\gametypes\_teams::addTestClients();` commented out. Stock data therefore cannot add test clients; `scr_testclients` alone does nothing.

[O] The overlay used here:
- `gameplay.py` writes `<fs_homepath>/mods/gptest/maps/mp/gametypes/dm.gsc` per run: the stock iw_15 `dm.gsc` with that one comment removed (SHA-256 `b8806487…38c8`, recorded in `harness.log`).
- It is derived from game data and is never committed.
- The server runs with `+set fs_game mods/gptest`. The loose file is found through the fs_game search path; this is the path that exposed the `FS_filelength` defect.

[O] Bot behaviour:
- `_teams.gsc` `TestClient` sends `menuresponse` team allies/axis and then weapon `m1carbine_mp`/`mp40_mp`.
- `SV_RunBotClients` gives each bot `SV_BotRandomUserCmd` every frame: `Rand_Fraction` buttons (attack 1, use 0x28), ±127 moves and random angles.
- In 25 s, bot entities 2/3 changed in all 605 snapshots, fired (`events[]`, impact temp entities eType 192) and switched weapons. There were no kills.

## Protocol and game facts confirmed

- [O] Both clients go through `mr` team → `mr` weapon → `spawnPlayer` in the same server frame as the second `mr` (snapshot at T0+1050).
- [O] Damage log lines are `D;guid;num;team;name;aguid;anum;ateam;aname;weapon;damage;mod;hitloc`. A thompson hit at close range is 35 damage, `MOD_PISTOL_BULLET` (sic), on `left_arm_lower` / `torso_lower`. Three hits kill.
- [O] The kill sends B the server commands `d 595 MP_hit_alert` / `s 5`, then `d 596 breathing_hurt`. The scoreboard reply to `score` is `b 2 0 0 0 1 <ping> 0 0 1 0 <ping> 1 0`.
- [O] B's killcam starts at about T0+18750: `pm_type` changes and archived frames are replayed with time fields rebased by `svs.time - snap->time`. Holding use (0x8) respawns B as soon as the killcam ends.
- [O] Looking at an enemy sets `pm_flags` 0x200000 (`G_UpdateLookAtTrigger`, 15000-unit ray, hit-location surface table 0x08187c0c).
- [O] The pickup hint for a dropped weapon is `cursorHint` 22.
- [O] On time limit, dm logs `W;;<guid>;<winner>` and then `ExitLevel: executed`. The second gamestate has serverId 32 and `configstring[13]` 85200.

## Remaining gaps

- The per-lane "not covered" lists are in `analysis/gameplay/*.md`. The main ones:
  - **Gametypes and maps:** sd bombmode 1/2 (hard-wired to 0 in the stock script); flags and radios after a rotation (cheats are off after `map`); misc_turret (no stock map uses one).
  - **Weapons:** panzerfaust; hold-breath on scoped rifles.
  - **Network:** `sv_maxRate` below 1000; a client timeout during `SV_SpawnServer`; addtestclient bots mixed with fake clients.
  - **Voting:** script voting with a mod that handles `call_vote`.
- A real Windows client was tested as a smoke test on the LAN (`lan-client-smoke.md`), not bit-exact.
- Seed pinning needs gdb. Without it, any scenario that consumes `Rand_*` is not reproducible.
- Untested by `difftest`: `MSG_ReadDeltaField`, `XAnim_BuildPartMap`, the clip functions and the chunk_052 transforms need structured inputs, so the harness reports `unsupported`. Their parity evidence is the gameplay oracle only.
- `calltrace.py` takes arity from the reconstruction's signatures. A wrong recon signature can hide a divergence, so compare `--nargs` 0 sequences first.
