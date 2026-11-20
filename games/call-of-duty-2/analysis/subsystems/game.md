# Subsystem: game logic (g_* game module, statically linked)

Target: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`.
Status: read-only analysis of reconstructed chunks plus Ghidra pseudocode and ELF data tables.

Evidence tags: **[O]** observed (directly in disassembly, decompile, a string, or an ELF data table I dumped), **[I]** inferred (consistent with several call sites but not proven), **[U]** unknown.
Lineage names (Q3 `g_*`, IW `scr_*`) are used only as naming hints. Everything below describes this binary.

## 0. Scope, inputs and coverage limits

* Inputs: `analysis/symbols/symbols.json` plus the per-chunk jsons, `recon/chunks/chunk_NNN.cpp` and `_types.h`, `.local/cod2/decomp/functions*`, and the ELF (`.data`/`.rodata` dumped with a small Python ELF reader; no REA/Ghidra session opened on the binary).
* `symbols.json` `chunks_done` is stale. Per-chunk jsons exist for 033, 045, 049, 053, 100, 101, 102, 105 as well as the listed ones, but these are not merged into `symbols.json`. I used them where they cover game code (see 0.1).
* Selection: tags `game*, entity*, combat*, item*, hud*, level*, mp*, configstring*` plus clearly related functions with other tags (`player/*`, `script/player*`, `weapon*`, `bg/items`, `bg/trajectory`, `server/game*`, `server/entity`, `server/config`, `server/script-entity`, `pool/objects`, `game/use-trigger`, `game/script-*`). That is roughly 770 functions. Per chunk: 105: 86, 052: 78, 051: 66, 050: 64, 043: 49, 103: 48, 044: 46, 049: 46, 048: 44, 047: 43, 046: 38, 045: 36, 102: 30, 040: 28, 053: 27, 021: 19, 020: 15, 101: 9.
* Pure math and vector helpers that were duplicated in each chunk (e.g. `Vec3_Copy_0810ebae`, `VectorMA_081029be`) are tagged `math/*` and are excluded. The duplicates are a naming issue (section 9).
* Tag noise found: `Cmd_*` game commands appear as `common/cmd` only for the engine ones in chunk 006, and as `game/cmds` for the game ones. `PlayerCmd_*` appear as `game/script-player-methods`. `BG_*` weapon functions are `game/bg_weapon`. `PM_Weapon_*` are `player/weapon` or `player/pmove`. These belong to the pmove/weapon-state lane, not to the g_* entity logic, but they are listed because the game drives them.

### 0.1 Gaps (what this document cannot describe)

* [O] `.local/cod2/missing_functions.json` lists 341 undecompiled ranges (57,267 bytes) inside 0x080f5000..0x08122000. Most are filled by chunks 102 and 105 (player script setters/getters, give/take weapon, `ScriptEntCmd_*`), but the symbol table still has no name for 73 of the 145 script builtin handlers and 21 of the 59 entity-method handlers (list in section 8.3).
* [O] `G_ClientIsPlaying`, `ClientEndFrame`, `G_PlayerStateToEntityState` bodies were skimmed only. `BG_PlayerStateToEntityState` (the pmove-side converter) is out of my chunk set.
* [U] Where in the binary `G_RunFrame` is told which map entity string to parse (`g_0864d6c8`, `KeyValue_ParseBlock`): the spawn-vars source is the BSP entity lump handled by the server/collision lane.
* [U] Trigger entity classes (`trigger_multiple`, `trigger_hurt`, `trigger_damage`, ...) are at 0x0811c69a..0x0811ce16 in the spawn table; only `Ent_*` helpers around them were read.
* Real-data validation: none done here (analysis only, no oracle runs).

## 1. Purpose and architecture as implemented in this binary

The game module is not a separate DLL. [O] `G_InitGame`/`G_RunFrame`/`ClientConnect`/... are called directly by the server code with plain calls (e.g. `SV_InitMapState` 0x080925e6 calls `G_InitGame`; `SV_RunGameFrame` 0x08096752 calls `G_RunFrame(svs.time)`; `SV_UserMove`/`SV_ClientThink` call `ClientThink`; `SV_ExecuteClientCommand` fallback calls `ClientCommand`). There is no import/export vtable. The ex-"trap_" functions are the normal server functions (`SV_LinkEntity` 0x0809c63a, `SV_Trace` 0x0809d884, `SV_SetConfigstring` 0x08092780, ...).

It is a Quake-III-lineage (IW "MP" flavour) game module with these characteristics, all [O] unless tagged:

* One static entity array `g_entities[1024]`, stride 0x230, at 0x08716400 (memset 0x8c000 in `G_InitGame` 0x08109096). One static client array `g_clients[64]`, stride 0x28a4, at 0x087a2500 (memset 0xa2900). A separate static world entity at 0x087a1fa0 ("g_dummyEntity", number 0x3fe) is not part of `g_entities`.
* Entities 0..63 are reserved for clients, 64..71 are an 8-slot temp-entity ring (`G_AllocTempEntity` 0x0811eaa4), and `G_Spawn` grows from 72 up to 0x3fe (1022). The sentinel entity number is 0x3ff (ENTITYNUM_NONE) and 0x3fe is ENTITYNUM_WORLD.
* The module is script-driven: gametype logic lives in GSC (`maps/mp/gametypes/*.gsc`), reached through `CodeCallback_*` entry points; the C side supplies built-ins, entity fields and methods, and physical simulation.
* The per-class behaviour (think, trajectory end, blocked, touch, use, pain, die, kind callback) is a 0x28-byte record table at 0x08187d00 indexed by `gentity+0x166` (section 4.2).
* Weapons: weapon definitions are loaded on demand from `weapons/mp/<name>` (`BG_RegisterWeapon` 0x080ebc24 -> `BG_LoadWeaponDef("mp", name)`); the BG_* registry (chunk 043) is shared with the (absent) client game.

## 2. Entry points and call flow

### 2.1 Level load / unload

Observed order in `SV_InitMapState` 0x080925e6 [O]: `FX_ResetBlockers` -> `G_InitGame(svs.time, Sys_MillisecondsRaw(), spawnFlag, param)`. After `G_InitGame` the server (`SV_SpawnServer` area, chunk 021 ~line 840) runs 3 game frames at +100 ms each (`SV_RunGameFrame`), calls `SV_ArchiveLinkedEntities`, and re-runs `ClientConnect` for retained clients (state>1) [O].

`G_InitGame` 0x08109096 (chunk 049) [O]:
1. Prints "------- Game Initialization -------", gamename "Call of Duty 2", gamedate "Jun 23 2006".
2. `memset(0x0864c380, 0, 0x3624)` (level block, section 3.1); `level+0x1c` (0x0864c39c) = 1 (precache/create flag); level times seeded; `srand`/`Rand_SetSeed(randomSeed)`.
3. If not (restart with map load) `Dvar_RegisterServerDvars` 0x081084a4 (really `G_RegisterDvars`; section 9).
4. `G_ProcessIPBans`; opens log file `g_log` (default `games_mp.log`, mode 2, or 3 if `g_logSync`); writes `InitGame: <serverinfo>`.
5. If not a restart: clear 0xb3bc8 bytes at 0x0864f9c0, `GScr_LoadScripts`, `Player_LoadMultiplayerAnims`, `Pool_AllocGameObjects`.
6. `fn_081224f8` = `G_InitScriptStrings` (chunk 053, interns the fixed script strings such as playing/dead/spectator/intermission, allies/axis/none, "player").
7. configstring 0x16 gets `winner\0`. Entity/client arrays cleared; `level.clients=g_087a2500; level.gentities=0x08716400`; `level.num_entities = 0x48`; free list empty; client `i`'s entity gets `client = &level.clients[i]`; `SV_InitGameEntityLayout(gentities, 0x48, 0x230, clients, 0x28a4)` (0x08091dac) publishes the layout to the server (`g_0849f61c/620/624/628/62c`).
8. `G_ParseHitLocDmgTable`, `G_ClearTurretSlots`, `G_SpawnEntitiesFromString`, `G_ParseConfigstring12Bounds("0")`, `G_ClearObjectives`, `Scr_FlushEntityReleaseChain`.
9. Script phase: `Scr_Init`, `Scr_SetLoading(1)`, `Scr_InitPersistentArray`, `G_SpawnScriptStructs` (runs `codescripts/struct::initstructs`, creates `script_struct` objects), then **gametype `main`** (`G_CallLevelScriptA` 0x081182f4), then **level `main`** (`GScr_RunLevelMain` 0x081101d0), then **`CodeCallback_StartGameType`** (`G_CallLevelScriptB` 0x08118322). [O: callee offsets g_0884d708 / g_0884d700 / g_0884d70c set in `GScr_LoadGametypeScript`/`GScr_LoadLevelScript`.]
10. Corpse slot table reset (8 slots, stride 0x4c8 at 0x0884e7b8, entity number -1), `level+0x1c = 0`, `ItemRegistry_BuildNameList` + `ItemRegistry_BuildBitmask` (configstrings 7 and 8).

`G_ShutdownGame(freeLevel)` 0x081094e8 [O]: logs, `Game_FreeAllEntities`, `HudElem_FreeAll`, `Scr_Shutdown`, optional `RefreshClientRandomIds`, and if `freeLevel`: `Mantle_ResetTree`, `GScr_ShutdownClasses`, `Scr_ResetAll`, `Pool_FreeGameObjects`, `Hunk_SetLowMark(0)`.

Script loading `GScr_LoadScripts` 0x0811041e [O]: `codescripts/delete::main` (offset 0x0884e7ac, used by `G_SendEntityEvent` 0x0811ef90), `codescripts/struct::initstructs` (0x0884e7b0) and `::createstruct` (0x0884e7b4), gametype script `maps/mp/gametypes/<g_gametype>::main` (0x0884d708), `maps/mp/gametypes/_callbacksetup::CodeCallback_{StartGameType,PlayerConnect,PlayerDisconnect,PlayerDamage,PlayerKilled}` (0x0884d70c, 710, 714, 718, 71c), optional `maps/mp/<mapname>::main` (0x0884d700; no error if missing), then `GScr_InitClasses` (4 script classes; class 0 = entity, class 1 = hudelem [O: `Scr_AddClassField(1,...)` in `HudElem_RegisterScriptFields`]).

### 2.2 Per-frame

`SV_Frame` 0x080969b0 [O]: fixed-step loop; for each `1000/sv_fps` ms step: `SV_RunBotClients`, `SV_CalcPings`, then loop { `svs.time += frameMsec; SV_RunGameFrame(); Scr_SetLoading(0); ... SV_ArchiveSnapshot }`.

`G_RunFrame(levelTime)` 0x0810a13a (chunk 049) [O]:
1. `frameNum++` (0x0864c568), `previousTime`, `time = levelTime` (0x0864c56c), `frameTime` (0x0864c574); `g_0860b420 = g_0864f9c0` (the game context pointer, cleared at the end).
2. For each in-use entity: `ScrEntity_CallVoidArgA(&ent, frameTime*0.001)`. Despite its name this calls `XAnim_BindingSetTime(Com_GetServerDObj(num), dt)`, i.e. advances the server-side DObj animation time [O: chunk 021 0x080920d0].
3. Pair queue (0x0864e168, up to 0x100 entries of 12 bytes: entityA, entityB, serialA, serialB; filled by `G_QueueEntityPairEvent` 0x0811c45c): copied to 0x0864ed68, then drained in rounds: if both generation counters (`gentity+0x228`) still match, the first not-yet-touched entity gets a script notify (string 0x08853274) with the other entity as argument; pairs that cannot yet fire stay and loop again. [O; the string value is the only [U]: likely "touch"/"trigger"; script string ids are interned in `G_InitScriptStrings`.]
4. `G_StopEntityScripts` for each entity, `Scr_BeginNested`, then for `g_0864f984 = 0..num_entities-1` (current entity index, -1 outside): if in use: run `driverRef`'s entity first (attached parent), then `G_RunEntityState(ent)`.
5. `G_CopyTurretEntries`, `G_UpdateClientTurretStates`, `ClientEndFrame` for each in-use client slot, `Client_UpdateAllCrosshairTargets`, `G_CheckVote` (only when dvar 0x087a24d0 is set; by registration order this is `g_oldVoting`, section 9 item 22), `G_FlushPendingTurretUpdates`; `g_listEntity` debug print; rebuilds configstrings 7/8 if flags 0x0864f97c/0x0864f980 were dirtied by `ItemRegistry_Register`; `G_DumpSelectedEntity` (`g_dumpAnims`).

`G_RunEntityState` 0x08109f5c [O] (once per frame per entity via `lastFrame` = frameNum): dispatch on `eType`:
* 4 -> `G_RunMissile` 0x0810e0ce (grenades/rockets, trace + `G_AddImpactEvent` 0x0810e024),
* 3 -> `Ent_StepMovement` 0x08107de6 (dropped items falling/sliding; or `G_RunThink` when parented),
* 2 -> `Ent_UpdateProjectile` 0x08107dbc, which is the **player corpse** update (see section 9),
* `occupied` (+0x160) set -> `Ent_StepMovement`; 6 -> `G_RunMoverEntity` 0x0810fdfe,
* else if `client == NULL`: `G_RunThink` 0x08109e60 (calls `g_08187d00[ent->entClass].think` when `nextThink` in (0, level.time]; "NULL ent->think" error otherwise),
* else `G_ClientRunFrame` 0x080f66c8.
Pre-step: entities with `eFlags == 0x10000` and `field058 < time` are freed; entities with `time - ent+0x178 > 300` are freed (if +0x17c set) or unlinked (if +0x180 set).

### 2.3 Client lifecycle (server -> game)

All [O], chunk 046 unless noted; callers are in chunk 019/020/021.

| Server caller | Game entry | Behaviour |
|---|---|---|
| `SV_DirectConnect` (chunk 020; also `SV_SpawnServer` re-connect loop, chunk 021 ~line 847) | `ClientConnect(clientNum, pers)` 0x080f8e7a | memset client 0x28a4 and `g_clientInfos[n]` (0x4b8, keeps +0x4a4), `connected=1`, `team=3` (spectator), `sessionState=2`, follow/spectator = -1, `G_InitEntityRecord(ent)`, `ent->client=...`, `ClientUserinfoChanged`, password check against `g_password` (`none` ignored; bots skip) -> returns `"GAME_INVALIDPASSWORD"`; else calls script `CodeCallback_PlayerConnect` on the entity (`G_CallEntityScriptA` 0x08118350), `G_BuildEntityOrder` |
| `SV_ClientEnterWorld` 0x0808f628 (chunk 020:906) | `ClientBegin(clientNum)` 0x080f90ae | `connected=2`, `pm_type=4`, resort, script notify (string 0x0885328c, 0 args). Spawning itself is script-driven |
| script `spawn()` / `PlayerCmd_spawn` 0x080fcb24 | `ClientSpawn(ent, origin, angles)` 0x080f910e | releases turret use, unlinks, sets clipmask `0x2810011`, svFlags|=1, `entClass=10`, `flags=0x1000`, saves session block `[+0x26a8, +0x27a8)` (0x100 bytes), zeroes the client, restores session block, `spawnCount+1`, `viewHeight=60`, `pm_flags|=0x1000`, `commandTime=level.time-100`, runs `ClientEndFrame` and `ClientThink_real`, then `BG_PlayerStateToEntityState` |
| `SV_UpdateClientUsercmd` 0x08090dac (chunk 020:1302) | `ClientThink(clientNum)` 0x080f65f8 | copies cmd to oldcmd, `SV_GetClientUsercmd`, runs `ClientThink_real` immediately unless `g_synchronousClients` (0x087a2440), in which case `G_ClientRunFrame` runs it in the frame loop with `serverTime=level.time` |
| `SV_ClientCmd_UserInfo` 0x08090a04 (chunk 020:1203) | `ClientUserinfoChanged` 0x080f8c5e | validates infostring (fallback `\name\badinfo`), `isBot = SV_CheckClientNetAddress()`, `cg_predictItems`, name cleaning into the 32-byte buffers, mirrors name/team into `g_clientInfos` |
| `SV_ExecuteClientCommand` (chunk 020:1224) | `ClientCommand(clientNum)` 0x08100e62 | command table, section 6.2 |
| `SV_ClientDisconnectCleanup` (chunk 020:512) | `ClientDisconnect(clientNum)` 0x080f94ce | script notify ("disconnect" with arg "-1" via the menuresponse string id), stops spectators following, `HudElem_FreeAllForOwner`, `CodeCallback_PlayerDisconnect` (`G_CallEntityScriptB`), `G_FreeEntity`, `connected=0`, clears `[+0x2748, +0x27a4)`, resort |
| `SV_ConsoleCommand` (chunk 021:314, only when sv.state == 2) | `G_ConsoleCommand` 0x0811bf64 | `entitylist`, `addip`, `removeip`, `listip` (-> `Cbuf` "g_banIPs"), and `say` when `dedicated` != 0 |

`ClientThink_real` 0x080f5d2e [O]: clamps `ucmd->serverTime` to `[level.time-1000, level.time+200]`, `msec` capped at 200; drops (`return`) when `connected != 2`; `sessionState==3` (intermission) -> `Client_UpdateButtonEdges`; `sessionState==2` (spectator) -> `Client_ProcessButtonsAndDrop`; playing/dead -> `Client_CheckInactivity` (g_inactivity), builds a `pmove_t` (size 0xf8, `tracemask = 0x2810011` when `pm_type<6` else `0x810011`), runs `WeaponView_*` (chunk 045) view springs, `PM_Pmove`, `G_SpawnPmoveEventEntity` if `pm.spawnEvent`, then `BG_PlayerStateToEntityState` (if `g_smoothClients`==0) or `G_PlayerStateToEntityState` 0x080f56f8 (smoothed, `trTime=commandTime`), `SV_LinkEntity`, touch scan (`Entity_TouchLinked` 0x080f4c1a, `Entity_TouchList` 0x080f4ab6), `Client_DispatchEvents`, and `G_UpdateUseState` (use/lookat, chunk 053).

Session states [O: `Player_SetSessionState` 0x080f7efc]: 0 playing, 1 dead, 2 spectator, 3 intermission (setting "intermission" toggles `stateFlags` bit 1... see 9). Session teams [O: `Player_SetSessionTeam` 0x080f7db6]: 0 none, 1 allies, 2 axis, 3 spectator. `connected` [O]: 0 free, 1 connecting, 2 connected.

### 2.4 Script -> game and game -> script

* Game -> script callbacks: `G_CallPlayerDamageCallback` 0x081183b8 (10 args pushed in reverse: inflictor, attacker, damage, dflags, MOD name string or "badMOD", weapon name string, point, dir, hitloc id string, timeOffset), `G_CallPlayerKilledCallback` 0x08118492 (9 args: inflictor, attacker, damage, MOD string, weapon name, dir, hitloc id, timeOffset, deathAnimDuration), plus `Scr_NotifyEntity(ent, stringId, argc)` 0x0811b2de for "damage" (g_scrCallbackDamage, args attacker, damage), "death" (g_scrCallbackDeath), "menuresponse", touch/trigger.
* Script -> game: 145 builtins (table 0x08188040, 12-byte entries {name, handler, developerOnly}), 59 entity methods in `g_0815e3a0`/`0x08188720` (0x3b entries; section 8.3), 59 player methods (table 0x08158a40, {name, handler, flag}), 15 hudelem fields (0x08159ee0, stride 0x1c) and hudelem methods, entity script fields (0x0815e820, 10 entries) and player script fields (0x08157ce0, 13 entries).
* Method lookup order [O: `Scr_GetMethod` 0x08117dea]: player methods, `GScr_FindEntityMethod` (0x0811a1fa, script_mover helpers), entity methods, hudelem methods; all four lookups always run, first non-null wins.

## 3. Global state

### 3.1 `level` (0x0864c380, block of 0x3624 zeroed bytes in `G_InitGame`)

[O] means the address+offset is used in code; names are mine. Chunk 046's `level_locals_t` (size 0x31c) is a prefix only.

| Off | Addr | Name | Evidence |
|---|---|---|---|
| +0x000 | 0x0864c380 | `clients` (ptr to 0x087a2500) | `G_InitGame`; client i = `+i*0x28a4` [O] |
| +0x004 | 0x0864c384 | `gentities` (ptr to 0x08716400) | [O] |
| +0x00c | 0x0864c38c | `num_entities` (init 0x48, max 0x3fe) | `G_Spawn` 0x0811e9d6 [O] |
| +0x010/+0x014 | 0x0864c390/394 | free-list head/tail (entity addresses; link at `gentity+0x22c`) | `G_Spawn`, `G_FreeEntity` [O] |
| +0x018 | 0x0864c398 | log file handle (`g_log`) | `G_InitGame`/`G_ShutdownGame` [O] |
| +0x01c | 0x0864c39c | "creating/precaching" flag: 1 during `G_InitGame`, used as `create` argument of config-string lookups | [O] |
| +0x020 | 0x0864c3a0 | `level.field_20`, set to 1 around `ClientSpawn`; `Item_TouchPickup` skips when nonzero | [O], meaning [I] "spawning" |
| +0x1e4 | 0x0864c564 | `maxclients` | [O] |
| +0x1e8 | 0x0864c568 | `frameNum` | `G_RunFrame` [O] |
| +0x1ec | 0x0864c56c | `time` | [O] |
| +0x1f0 | 0x0864c570 | `previousTime` | [O] |
| +0x1f4 | 0x0864c574 | `frameTime` | [O] |
| +0x1f8 | 0x0864c578 | `startTime` (also configstring 0xd) | `SP_worldspawn` [O] |
| +0x200/+0x204 | 0x0864c580/584 | two counters; zeroed in `G_ExitLevel`; written to configstrings 5 and 6 in `ClientEndFrame_Intermission` | [O]; "team scores" [I] |
| +0x214 | 0x0864c594 | flag that freezes name changes for connected clients | `ClientUserinfoChanged` [O/I] |
| +0x218/+0x21c | 0x0864c598/59c | `numSortedClients`, `sortedClients[64]` | `G_BuildEntityOrder` [O] |
| +0xb1c..+0xb28 | 0x0864ce9c.. | vote: time, pending-execute, yes, no; display string at 0x0864ca9c (0x400) | `G_CheckVote`, `Cmd_CallVote_f` [O] |
| +0xb2c | 0x0864ceac | count of state-2 non-spectator clients | [O] |
| +0x1d54 | 0x0864e0d4 | "restart/keep-state" int (`Level_Get/SetIntValue` 0x08108dee/f8); consulted by `G_ShutdownGame` and `SV_ValidateGametype` | [O] |
| +0x1d58 | 0x0864e0d8 | 32-entry pointer table cleared by `G_ClearEntityReferences` | [O] |
| +0x1dd8/+0x1ddc | 0x0864e158/15c | cull-fog/bounds (float, float^2), default FLT_MAX, from configstring 12 | `G_ParseConfigstring12Bounds` [O] |
| +0x1de4 | 0x0864e164 | temp-entity ring index 0..7 | [O] |
| +0x1de8.. | 0x0864e168 | pair queue (0x100 x 12 bytes); count at 0x0864f968; copy at 0x0864ed68; copy count 0x0864f96c | [O] |
| 0x0864f974 | | flag: when set, `G_RadiusDamage` ignores clients | [O] |
| 0x0864f984 | | current-entity index during `G_RunFrame` (-1 otherwise) | [O] |

Other globals [O]: hudelem pool `g_hudelems` 0x08628f80 (1024 x 0x8c = 0x23000); hit-location multipliers 19 floats at 0x08628ee0 (and ids `g_hitLocIds`); `g_clientInfos` 0x087035bc (64 x 0x4b8; also the per-seat turret bank; pose pointer at +0x4a4 = 0x08703a60 base); corpse/tracked slots 0x0884e7b8 (8 x 0x4c8, entity number at +4); weapon registry `bg_weaponDefs` 0x086270a0, `bg_numWeapons` 0x08627080, ammo/clip/shared tables 0x086272c0/0x086274e0/0x08627700 (128 pointers each); item table `bg_itemList` 0x081850c0 (stride 0x2c, 0x81 entries); entity class table 0x08187d00; spawn table 0x08188a00; MOD names 0x08187c40 (15); game context pointer `g_0860b420`; model handle table `g_08852e20[0x100]`; gametype list 0x0884d72c (0x20 x 0x84); objectives (0x1c each, `G_ClearObjectives` 0x08112796).

## 4. Data formats

### 4.1 Entity (`gentity_t`, 0x230) and client

See `game.structs.json` for the reconciled layout. Key facts [O]: `number` at +0 is derived from the address (`(addr-0x08716400)/0x230`, compiled as `>>4 * 0x8af8af8b`); `inuse` byte at +0xfc; `client` at +0x158; `entClass` byte at +0x166; `classname` script-string handle at +0x168; `health` +0x194; free-list link +0x22c; generation counter +0x228 (incremented in `G_FreeEntity`, compared in pair queue); `freeNext/time178` (+0x178) set to `level.time` at free and `G_Spawn` reuses a free-list head only if `level.time - time178 >= 500` (or the pool is full).

### 4.2 Entity class callback table (0x08187d00, stride 0x28)

[O] raw dump of the table plus 5 independent chunk readers (049, 050, 047, 045, 053, 052). Layout: `+0 think`, `+4 onTrajectoryEnd`, `+8 onBlocked`, `+0xc touch`, `+0x10 use`, `+0x14 pain`, `+0x18 die`, `+0x1c kindCallback` (called by `Ent_RunKindCallback` 0x0811e3c0, 0x0811e394), `+0x20 mode` (3 tested by the missile runner; turret mode), `+0x24 field24`. Raw rows 0..13 show e.g. row 5/6: `onTrajectoryEnd = G_ScriptMover_Think`; row 9/10: `die = player_die`; row 12: `think = G_SetEntityBodyContents`; rows 3/4: `touch/use = Ent_ToggleKind`. `ClientSpawn` sets class 10 (living player); `player_die` sets class 0xb (11, dead player); `cloneplayer` sets class 0xc (corpse).

### 4.3 Entity types (`eType`) seen in code [O: numeric only]

0 general/player; 2 player corpse (`PlayerCmd_cloneplayer` sets 2, `G_FreeEntity` clears the corpse slot for type 2); 3 dropped item (moved by `Ent_StepMovement`; use-candidate handling in `G_UpdateUseTarget`); 4 missile; 5 invisible/intermission (ClientEndFrame_Intermission sets `eType=5`); 6 mover/script_mover; 9 turret (`G_SpawnTurret`, `Turret_*`, `player_die` redirects damage of type-9 attackers to the owner). Names are [I].

### 4.4 Configstring layout used by the game

Writers: `G_FindConfigstringIndex` 0x0811d300 (case-insensitive scan, create when `level+0x1c`, error text "`%s \"%s\" not precached`"), `G_ModelIndex` 0x0811d49c ("model '%s' not precached", also stores an XModel handle in `g_08852e20[i]` via `SV_ValidateModelName`).

| Index / range | Count | Content | Evidence |
|---|---|---|---|
| 0 / 1 | | serverinfo / systeminfo built by `SV_Frame` (`Dvar_InfoString(0x404)`, big string flag 8) | chunk 022 [O] |
| 2 | | `"cod"` | `SP_worldspawn` [O] |
| 3 | | `n\<ambienttrack>` or empty | [O] |
| 4 | | worldspawn `message` | [O] |
| 5, 6 | | `level+0x200`, `level+0x204` as decimal | `ClientEndFrame_Intermission` [O] |
| 7 | | space-joined weapon names (`ItemRegistry_BuildNameList`) | chunk 048 [O] |
| 8 | | hex bitmask of registered items (`ItemRegistry_BuildBitmask`) | [O] |
| 0xb | | `northyaw` or "0" | [O] |
| 0xc | | 7 floats (cull fog; parsed to fog bounds) | `G_ParseConfigstring12Bounds` [O] |
| 0xd | | level start time | [O] |
| 0xe | | `g_motd` | [O] |
| 0xf..0x12 | | vote time, vote display string, yes, no | `G_CheckVote` area [O] |
| 0x16 | | info string with `winner` | `G_InitGame` [O] |
| 0x17..0x1e | 8 | status icons (`precachestatusicon`) | [O] |
| 0x1f..0x2d | 15 | head icons (`precacheheadicon`) | [O] |
| 0x6e | 0x20 | "items" via `G_ItemIndex` (no error text) | [O range; use [I]] |
| 0x14e | 0x100 | models | [O] |
| 0x24e | 0x100 | anim names (`G_AnimIndex`, byte result) | [O] |
| 0x34e | 0x40 | effects (`G_EffectIndex`, error label "effect"; `GScr_playFx` reads it) | [O] |
| 0x48e | 0x10 | **shellshock** names. `G_SoundIndex` 0x0811d634 uses this range, but `PlayerCmd_shellshock` (chunk 103 ~line 1142) looks names up here; see section 9 | [O range], naming [I] |
| 0x4de | 0x20 | menus (`G_MenuIndex` 0x0811344a, "Menu '%s' was not precached") | [O] |
| 0x4fe | 0x20 | hint strings (`G_FindOrAddHintString` 0x08112432) | [O] |
| 0x51e | 0x100 | localized strings (`G_LocalizedStringIndex`, label "localized string") | [O] |
| 0x61e | 0x80 | shaders (`G_ShaderIndex`, lowercased, label "shader") | [O] |

Ranges not covered by game code I read: 0x13..0x15 and 0x2e..0x6d (client/server shared; `SV_SetupMapConfigstrings` 0x08093486 and the client side own them), 0x38e..0x48d, 0x6a0+. [U]

### 4.5 Script field/method tables (ELF dumps)

* Entity fields (0x0815e820, `{name, offset, type, setter}` stride 0x10), types 0 int, 1 float, 2 string, 3 scr-string, 4 vec3, 5 entity, 6 yaw, 7 u16 object, 8 model [chunk 051 comment, [I]]: `classname`@0x168 (read-only setter), `origin`@0x138 (setter 0x08117e62), `model`@0x164, `spawnflags`@0x170, `target`@0x16a, `targetname`@0x16c, `count`@0x1a0, `health`@0x194 (setter 0x08117ed4), `dmg`@0x19c, `angles`@0x144 (setter 0x08117ea6). [O]
* Player fields (0x08157ce0, stride 0x14 `{name, offset, type, setter, getter}`): `name`@0x2784 (read-only), `sessionteam`, `sessionstate`, `maxhealth`@0x2728, `score`@0x26b8, `deaths`@0x26bc, `statusicon`, `headicon`, `headiconteam`, `spectatorclient`@0x26ac, `archivetime`@0x26b4, `psoffsettime`@0x27a4, `pers`@0x26c0. Field ids are `0xc000|index`. [O]
* Hudelem fields (0x08159ee0, stride 0x1c, 15 entries): x@4, y@8, z@0xc, fontscale@0x10, font@0x14, alignx/aligny@0x18 (mask 3, shift 2/0), horzalign/vertalign@0x1c (mask 7, shift 3/0), color/alpha@0x20, label@0x30, sort@0x78, foreground@0x7c, archived@0x88. [O]
* Spawn table (0x08188a00, `{classname, fn}`, 21 entries): `info_null`/`func_group` (no-op `G_ReleaseEntityUse`), `info_notnull`, `trigger_multiple/radius/disk/hurt/once/use/use_touch/damage/lookat`, `light`, `misc_model`, `misc_mg42`/`misc_turret` (`G_SpawnTurretFromWeapon`), `corona`, `script_brushmodel`, `script_model`, `script_origin`, `script_struct` (handler is `G_FreeEntity`: `G_CallSpawn` returns before creating an entity). Item classnames are matched first through `bg_itemList` (`G_FindItemForClassname` 0x0811a9ba). [O]

## 5. Dvars registered by this subsystem

`Dvar_RegisterServerDvars` 0x081084a4 (chunk 048; true name is a `G_RegisterDvars`), flags in hex [O]:
`sv_cheats` (bool 0, 0x1000), `gamename`="Call of Duty 2" (0x1044), `gamedate`="Jun 23 2006" (0x1040), `sv_mapname` (0x1044), `g_gametype`="dm" (0x1024), `sv_maxclients` (int 20, 1..64, 0x1025), `g_synchronousClients` (0x1008), `g_log`="games_mp.log", `g_logSync`, `g_password`, `g_banIPs`, `dedicated` (0..2, 0x1020; re-registered 0x1040 when nonzero), `g_speed`=190, `g_gravity`=800, `g_knockback`=1000, `g_weaponAmmoPools`, `g_maxDroppedWeapons`=16 (1..32), `g_inactivity`, `g_debugDamage`, `g_debugBullets` (-3..6), `g_motd`, `g_playerCollisionEjectSpeed`=25, `g_dropForwardSpeed`=10, `g_dropUpSpeedBase`=10, `g_dropUpSpeedRand`=5, `g_clonePlayerMaxVelocity`=80, `voice_global`, `voice_localEcho`, `voice_deadChat`, `g_allowVote`=1, `g_listEntity`, `g_deadChat`, `g_voiceChatTalkingDuration`=500, `g_ScoresBanner_{Allies,Axis,None,Spectators}`, `g_TeamName_{Allies,Axis}`, `g_TeamColor_{Allies,Axis}` (color), `g_smoothClients`=1, `g_antilag`=1 (0x1005), `g_oldVoting`=1, `g_voteAbstainWeight`=0.5, `g_no_script_spam`, `g_debugLocDamage`, `g_friendlyfireDist`=256, `g_friendlyNameDist`=15000, `player_meleeRange`=64, `player_meleeWidth`=10, `player_meleeHeight`=10, `g_dumpAnims` (-1..1023), `g_useholdtime`, `g_useholdspawndelay`=1 (0..10), `g_mantleBlockTimeBuffer`=500. It ends with `Player_RegisterDvars` (pmove/weapon dvars, chunk 040/043: `player_scopeExitOnDamage`, `player_adsExitDelay`, `player_breath_*` ...; owned by the pmove lane). Other game-time dvars: `mapname` (0x1044, `GScr_LoadLevelScript`), `g_gametype` re-registered in `SV_ValidateGametype`.

Console / svcmds [O]: `entitylist`, `addip`, `removeip`, `listip` (`G_ConsoleCommand`); `Cmd_PrintEntities_f`, `Cmd_EntityCount_f` are client commands.

## 6. Behaviours a native port must reproduce

### 6.1 Entities and the free list [O]
* Pool/free-list rules: indices 0..63 clients, 64..71 temp ring (`G_AllocTempEntity`: frees the slot first when in use, preserves bit 1 of `eFlags` toggled: `(saved&2)^2`; ring cursor `(i+1) mod 8`), real spawns from 72. `G_FreeEntity` zeroes 0x230 bytes, sets `time178=level.time`, increments `generation`, and appends to the free-list tail only if entity number > 0x47. `G_Spawn` reuses the list head only when 500 ms old (or when `num_entities == 0x3fe`); at 0x3fe it prints entities and `Com_Error`s (string at 0x0815ed01).
* `G_FreeEntity` side effects: unlink from parent and children (`Ent_UnlinkFromParent`), `SV_UnlinkEntity`, reset server DObj (`Com_ServerDObjFree`), `G_ClearEntityReferences` (clears `lockedBy`, `parentNum`, `targetNum`, client `+0x282c/+0x2830/+0x5a0` references and the 32-entry table), corpse slot release, script entity free (`G_FreeScriptEntity`).
* Entity-script-string handles (`classname`, `target`, `targetname`) are refcounted (`ScrStr_AssignRef`) and must be released on free.

### 6.2 Client commands [O: `ClientCommand` 0x08100e62]
`say`, `say_team`, `score` always; when `pm_type != 5`: `mr` (menu response), `give`, `take`, `god`, `demigod`, `notarget`, `noclip`, `ufo`, `kill`, `follownext`, `followprev`, `where`, `callvote`, `vote`, `setviewpos`, `entitycount`, `printentities`. Unknown command: server command `e "GAME_UNKNOWNCLIENTCOMMAND\x15<cmd>"` (0x65 = 'e', 0x15 separator). Cheat commands go through `CheatsOk` 0x080fe102.

### 6.3 Damage [O: chunk 047]
* `G_Damage` 0x08101c58: non-clients: substitutes `g_dummyEntity` for NULL inflictor/attacker, damage min 1, god flag `flags&1` ignores, demigod `flags&2` clamps to leave 1 hp, notifies script "damage"(attacker, damage), if `health<1` (floor -999) notifies "death"(attacker) and calls the class `die`, else class `pain`. Clients go to `G_DamageClient` 0x08101b40: requires `takedamage`, no noclip/ufo, `connected == 2`; weapon taken from inflictor `+0xc8` else attacker; multiplies by `G_GetHitLocMultiplier` (per-weapon table at weaponDef+0x5b0 unless `weaponDef+0x78 != 0`, else global); if result > 0 calls `CodeCallback_PlayerDamage` (10 args). All health logic for players is therefore in GSC (`finishPlayerDamage`, `PlayerCmd_finishplayerdamage` 0x080fb170, 0xa43 bytes).
* Hit locations: 19 names from `info/mp_lochit_dmgtable` (file must start with `LOCDMGTABLE`, < 0x2000 bytes after the header, infostring format); the last (index 18) multiplier defaults to 0.0, others 1.0; ids are interned script strings (`G_GetHitLocId` 0x08102876).
* MOD names (15, 0x08187c40): `MOD_UNKNOWN, MOD_PISTOL_BULLET, MOD_RIFLE_BULLET, MOD_GRENADE, MOD_GRENADE_SPLASH, MOD_PROJECTILE, MOD_PROJECTILE_SPLASH, MOD_MELEE, MOD_HEAD_SHOT, MOD_CRUSH, MOD_TELEFRAG, MOD_FALLING, MOD_SUICIDE, MOD_TRIGGER_HURT, MOD_EXPLOSIVE`; out-of-range -> string "badMOD".
* `G_RadiusDamage` 0x081022a0: attacker must be non-NULL; box = origin +- radius*sqrt(2); `CM_AreaEntities` up to 1024; per victim with `takedamage`: distance (point to AABB if the byte at +0xf1 is set, else to `currentOrigin`); `damage = min + (1 - d/r)*(max-min)`; visibility `G_CanDamage` (5 trace points, mask 0x802891: 1.0 if any clear point for non-clients; fraction `hits/3` for 1..3 clear of 5, 1.0 for >=4 for clients); if visible: `G_Damage(..., dir+24z, dflags 1, mod, hitloc 0)` with `(int)(damage*visibility)`; else if a box trace (mask 0x811) from origin to the victim's centre hits and the distance is `< 0.2*radius`, applies `damage*0.1`. Returns whether any victim was "hit" (`G_ClientCanTargetEntity` 0x08120752 for attacker credit).
* `player_die` 0x0810175a: requires server DObj and `pm_type` 0/1 and not `pm_flags & 0x400000`; attacker of type 9 redirected to its owner; notifies "death"; spawns a dropped grenade `G_SpawnGrenade` when client+0x3c (`field_3c`, an offhand/grenade count) is nonzero with random velocity scaled 160; `pm_type` 1->7 else ->6; `BG_AnimScriptEvent(client, 1, 0, 1)` result is passed as death-animation duration to `CodeCallback_PlayerKilled`; spectators following the victim are refreshed; sets `takedamage=1`, `contents=0x4000000`, `entClass=11`, `health=0`.
* `g_debugDamage`/`g_debugLocDamage` print `target:%i health:%i damage:%i`.

### 6.4 Weapons and items [O/I]
* Registry: slot 0 = default "no weapon" def; `BG_RegisterWeaponDef` pre-increments `bg_numWeapons`, fills ammo-type/shared-cap/clip tables (dedup by name), item entry and alt-weapon link recursively. `bg_itemList[i]` for weapons is `{field00, damage?, ..., name@0x14, ..., itemType@0x1c=1, weaponIndex@0x20, ammoIndex@0x24, clipIndex@0x28}`; ammo entries have `itemType=2` (`BG_FillInAmmoItems`). Max 0x80 weapons (`bg_itemList[1..0x80]` cleared).
* Player weapon storage in `playerState`: ammo[128] +0x144, clip[128] +0x344, owned-bitset +0x544, held slots `weaponSlot[1..2]` at +0x555/+0x556 (primary/secondary), `weapon` +0xd4, offhand +0xd0. `BG_GetCurrentWeapon` 0x080ebd22: `pm_flags & 0x20000` -> first type-3 weapon (binoculars/turret), `pm_flags & 0x10` -> offhand, else `weapon`.
* Pickup: `Item_TouchPickup` 0x08105cac (skipped while `level+0x20` or when the item's `touchPending` is 0 or the player dead): `BG_CanItemBeGrabbed` 0x080df39e gate; messages `GAME_CANT_GET_PRIMARY_WEAP_MESSAGE`, `GAME_PICKUP_CANTCARRYMOREAMMO\x14<weapon>`; log lines `Weapon;%d;%d;%s;%s` / `Item;%d;%d;%s;%s`; dispatch by item type 1 weapon (`Item_PickupWeapon`, 2381 bytes: random clip/reserve fill when the dropped item has 0, swapping with held weapon, stackable slots), 2 ammo, 3 health; success adds pickup event (0x90) and frees the item.
* Drops: `g_maxDroppedWeapons` cap (`DroppedWeapon_FindSlot`), `g_dropForwardSpeed/UpSpeedBase/UpSpeedRand`.
* Firing: `PM_Weapon_*` state machine (chunk 044) inside pmove; server-side shot creation is `G_FireWeaponByMode` 0x08120870 (chunk 053): `fireType` 0 hitscan (`G_FireWeaponShot` -> `Bullet_FireFromEntity`/`Bullet_Fire_Extended`, range 8192.0 [0x46000000]; objType 3 -> shotgun pellets), 1 missile (`G_LaunchWeaponMissile`), 2 melee/FOV (`G_ComputeFovViewOffset`), else prints `Unknown weapon type %i for %s.`. Antilag via `G_SnapshotClientPositions`/`G_RestoreClientPositions` (0x0811fc76/0x0811fdec) when `g_antilag`.

### 6.5 HUD elements [O: chunks 047/048]
Pool of 1024 elements, 0x8c each; `HudElem_Alloc(owner, second)` returns the first element with `type==0` (first word) after `HudElem_Init` (type=1, +0x10 = 1.0, +0x20 = -1, +0x88 = 1) and stores owner key at +0x80, second key at +0x84; `HudElem_FreeAllForOwner` frees by +0x80; client lists are built by `HudElem_BuildClientLists` 0x08104882 into two 31-slot snapshots (each slot = first 0x80 bytes of the element) stored inside `playerState` at +0x7a8 (unflagged) and +0x1728 (flagged, selected by element +0x88 != 0), filtered by viewer team (+0x84 vs client team) and target client (+0x80, 0x3ff = all). Script fields: 15 table entries above; timers via `HudElem_SetTimerCommon` (type 7) / `SetShaderTimerCommon` (type 0xc); enum errors `"%s" is not a valid value for hudelem field "%s"`.

### 6.6 Spawning [O: chunk 051]
`G_SpawnEntitiesFromString`: first entity must be `worldspawn` ("SP_worldspawn: The first entity isn't 'worldspawn'"), sets configstrings (4.4), `g_gravity` from key `gravity` (default 800), and initialises the static world entity (number 0x3fe, in-use at 0x087a209c, classname string at +0x168, spawnflags at +0x170). Then each following block -> `G_CallSpawn` (classname -> item or spawn table; unknown classnames still create and fill an entity but call nothing; `script_struct` is skipped here because it was consumed by `G_SpawnScriptStructs`). Spawn variable parsing `G_ParseField` handles the entity field table types.

### 6.7 Movers [O: chunks 050/051]
`G_RunMover`/`G_MoverPush`/`G_TryPushingEntity` (Q3-style push with blocked callback), and the script mover family (`G_ScriptMover_*`: accel/cruise/decel phase planner, `moveto/rotateto/movex/rotatepitch/...` script methods): fields at +0x1a8..+0x208 (decel times, speeds, cruise/accel end times, destination). The planner is time based on `trType` values (trajectory type 5 "linear stop"/gravity in use for corpses [I]).

### 6.8 Use/look-at triggers [O: chunk 053]
Per client frame (`G_UpdateUseState` 0x08121314 and `G_UpdateUseTarget` 0x08121bc6): candidates collected by `G_CollectUseCandidates`, first valid wins; buttons 0x20/0x28 (use) drive `client->flags` bit 8; the use target handle/slot/entity is stored in the client (+0x2830 area), `g_useholdtime`/`g_useholdspawndelay` apply; classnames compared are interned script strings (g_08853276/g_08853278).

## 7. Inter-subsystem dependencies

* Server (chunks 019-023): `G_InitGame/ShutdownGame/RunFrame`, `Client*`, `G_ConsoleCommand` called from server; game calls back `SV_LinkEntity/UnlinkEntity`, `SV_Trace`, `SV_PointContents`, `SV_SetConfigstring/GetConfigstring(OrEmpty)`, `SV_SendServerCommandNum` (command prefix byte 0x65 'e' errors, 0x66 'f' messages, 0x49 'I' give-weapon notice [O]), `SV_GetUserinfo`, `SV_GetClientUsercmd`, `SV_DropClient`, `SV_GameClientNum`, `SV_GentityNum`, `SV_InitGameEntityLayout`.
* Script VM (chunks 011-018): `Scr_Add*/Scr_Get*`, `Scr_NotifyEntity`, `Scr_CallEntityMethod`, `Scr_AddClassField`, script-string intern/refcount (`ScrStr_*`).
* BG/pmove (040-045): `PM_Pmove`, `BG_*Weapon*`, `BG_EvaluateTrajectory` 0x080df5d4, `BG_PlayerStateToEntityState`, anim scripts (`BG_AnimScriptEvent`), `Mantle_*`.
* XAnim/DObj (033, 102, 105): corpse trees (`XAnim_CopyTreeSlots`), per-entity DObj (`Com_GetServerDObj`, `Com_ServerDObjFree`), `Pool_AllocGameObjects` XAnim blocks.
* FX/sound (054..): `GScr_playFx` reads configstring 0x34e; `playsound`, `playloopsound`, `musicplay`.
* File system / dvar / net: hitloc table (`FSH_FOpenFile`), `g_log`, dvars, `Cbuf` (map_rotate on `G_ExitLevel`).

## 8. Detailed inventories

### 8.1 Function groups (address ranges)
* 0x080dafa6..0x080df948 (chunk 040): player view/anim/jump helpers, `BG_GetItemByIndex` 0x080df20a, `BG_FindItemByName` 0x080df226, `BG_CanItemBeGrabbed` 0x080df39e, trajectories.
* 0x080eb014..0x080ed440 (043): weapon registry. 0x080ed5c0..0x080f11b0 (044): `PM_Weapon_*` state machine. 0x080f14aa..0x080f5682 (045): weapon view springs, weapon-def parser (`BG_LoadWeaponDef` 0x080f2844), accuracy graphs, key/value block parser (`KeyValue_ParseBlock` 0x080f3fd2: used by `G_SpawnEntitiesFromString`), touch scans, client button/inactivity helpers.
* 0x080f56f8..0x080fe9ca (046): client lifecycle, ps-to-es, scoreboard, give.
* 0x080fe9ca..0x08103210 (047): god/noclip/ufo/kill, follow, say, vote, damage, debug draw, hudelem pool.
* 0x08103618..0x08109082 (048): hudelem fields, ammo/items/pickups, corpse physics, dvar registration, pools.
* 0x08109096..0x0810e024 (049): init/shutdown, ordering, frame, turrets, trace wrappers (`G_TraceBox` 0x0810a528 etc.).
* 0x0810e024..0x08118260 (050): missiles, movers, script load, GScr builtins, objectives.
* 0x08118260..0x0811c0d0 (051): script callbacks, script_mover, spawn, ip bans, svcmds.
* 0x0811c0d0..0x08120000 (052): team/crosshair, entity helpers, configstring indexing, attachments/links, pool, events, bullets.
* 0x081203d4..0x08124xxx (053): weapon fire, use/look-at, script strings, message streams (not game), fx curves (not game).

### 8.2 Entity attachment/links [O offsets from chunk 052 access patterns, semantics I]
A linked child owns a 0x70-byte node at `gentity+0x208` (`attachLink`): `owner` parent entity (+0), `next` sibling (+4), tag name script-string (+8), `tagIndex` (+0xc, -1 if unresolved), origin (+0x10), angles (+0x34); the parent keeps the list head at `+0x20c` (`attachChildren`). `Ent_LinkChild` 0x0811dbb0, `Ent_LinkChildAndReset`, `Ent_LinkChildWithOffsets`, `Ent_UnlinkFromParent` 0x0811ddb8, `Ent_UpdateTagIndex`, `Ent_UpdateChildrenTags`, `Ent_GetAttachTransform` 0x0811dfca, `Ent_SyncAttachTransform` 0x0811e118. This is the same pointer chunk 049 calls `driverRef` (`ent->driverRef->ent` = parent entity run before the child in `G_RunFrame`). Attached models: `attachModel[]` bytes at +0x210 and `attachName[]` u16 at +0x218 (chunk 052 says 7 slots; chunk 046 says 6; `gclient` has 6-element `attachModelIndex/attachTagIndex` arrays), `attachFlags` byte at +0x165 (bit k = slot k linked). `Ent_AddAttachment` 0x0811d900 / `Ent_RemoveAttachment` / `Ent_ClearAttachments` / `Ent_RefreshAttachments` 0x0811d69e maintain them (script methods attach/detach/detachall).

### 8.3 Script builtin handlers not named in the symbol table
Builtins (name@handler) with no symbol name: iprintln@0x08110ad2, iprintlnbold@0x08110afe, print3d@0x08110b2a, getent@0x0811b312, getentarray@0x0811b40e, spawn@0x081112bc, spawnturret@0x0811138c, precacheturret@0x0811141a, assert/assertex/assertmsg@0x08110c08/2e/6c, isdefined@0x08110c98, isstring@0x08110cfe, isalive@0x08110d24, getcvar/int/float@0x08110da0/dd0/e08, setcvar@0x08110e46, gettime@0x08111012, getentbynum@0x08111028, getweaponmodel@0x08111078, getanimlength@0x0811116c, animhasnotetrack@0x081111ee, getbrushmodelcenter@0x0811125c, getmovedelta/getangledelta/getnorthyaw@0x08114d74/e8c/fac, loadfx@0x08114fe8, playfxontag@0x081152d6, playloopedfx@0x08115466, setexpfog@0x08115910, grenadeexplosioneffect@0x08114bb6, radiusdamage@0x08114cbe, setplayerignoreradiusdamage@0x08114d5a, getnumparts/getpartname@0x081160e0/112, earthquake@0x081161ba, newhudelem/newclienthudelem/newteamhudelem@0x08103c62/c9e/d0a, isplayer/isplayernumber@0x081159e2/a42, setwinningplayer/team@0x08115a82/b40, announcement/clientannouncement@0x08115c62/cd6, getteamscore/setteamscore@0x08115d5a/dda, setclientnamemode/updateclientnames@0x08115ed2/f28, getteamplayersalive@0x08115ff6, map_restart@0x08116a3c, exitlevel@0x08116b6a, addtestclient@0x08116bd8, makecvarserverinfo@0x08116bfa, setarchive@0x081178ea, allclientsprint@0x08116d4c, clientprint@0x081170ae, mapexists@0x08116da2, isvalidgametype@0x08116dee, setvotestring/time/yescount/nocount@0x08116e3a/ee2/f72/fdc, openfile/closefile/fprintln/freadln/fgetarg@0x08117120/3b4/49c/5c0/778, kick@0x08117022, ban@0x08117068, map@0x08116ab4, issplitscreen@0x08117918.
Entity methods (those 21 with no name): attach/detach/detachall/getattachsize@0x08111452/542/66c/68e, getammocount@0x08111104, getclandescription/url@0x081105fc/0x08110610, shellshock/stopshellshock/viewkick@0x081162a2/42b/48e, localtoworldcoords@0x08116574, setright/left/top/bottomarc@0x081165f6/66a/6d2/746, placespawnpoint@0x081167ae, updatescores@0x081169a2, setteamfortrigger@0x08117944, clientclaimtrigger/clientreleasetrigger/releaseclaimedtrigger@0x08117a72/b5a/c28.
(The remaining entity methods and 72 builtins have symbol names from chunks 050, 051, 103, 105; names in chunk 105 follow the `ScriptEntCmd_*`/`GScr_*` convention.)

## 9. Naming problems found

1. **`Dvar_RegisterServerDvars` (0x081084a4, chunk 048)** registers `g_*`, `voice_*`, `player_melee*`, `sv_cheats`, `sv_maxclients`, `dedicated`; it is the game's dvar registration (`G_RegisterDvars`), not a generic server dvar init [O: strings]. `G_InitGame` calls it only when not (restart with map load).
2. **`G_BuildEntityOrder` / `G_CompareEntityOrder` (0x081097e6/0x0810968e, chunk 049)** sort *clients* by sessionTeam 3, score, secondary score into `sortedClients[]`; they are `CalculateRanks`/`SortRanks` equivalents. Also the field they read at +0x26c4 is named `sortState` in 049, `connected` in 046/047/048, `sessionState` in 052 and `field26c4` in 103; the evidence (values 0/1/2, set to 2 in `ClientBegin`, `G_ExitLevel` uses `==2`) supports `connected`. Offset +0x26a8 is the real `sessionState` (0..3) in 046/047 but is shown as `unk26a8` in 049/052.
3. **`ScrEntity_*` (chunk 021, 0x08092002..0x0809236e)** are server DObj wrappers keyed by entity number (`XAnim_BindingSetTime`, `DObj_GetHierarchyBits`, ...), not script entity functions. Names in `G_RunFrame`/`G_DumpSelectedEntity` (`ScrEntity_CallVoidArgA`, `ScrEntity_CallVoidC`) inherit that.
4. **`SV_ForwardCallToGame` (0x08090afa)** actually forwards to `FX_GetAttenuation` [O: chunk 020 lines 1206-1211].
5. **`G_SoundIndex` (0x0811d634)** indexes configstring range 0x48e (16) and `PlayerCmd_shellshock` looks up shellshock names there; likely `G_ShellShockIndex` [I]. If so, sounds are not in a game-managed configstring range at all [U].
6. **`Ent_UpdateProjectile`/`Ent_UpdateMissileSlot`/`Ext_ReadSlotHandle`/`Ent_ApplyTraceResult`/`Ent_UpdateAnglesFromTrace` (chunk 048)** operate on corpse entities (`eType 2`, tracked slot table at 0x0884e7b8, `G_FindTrackedSlot`): `cloneplayer` (chunk 103) creates the entity with `eType=2`, assigns `G_PickTrackedSlot`, copies the 0x4b8 clientInfo and the XAnim tree. They are corpse/body physics, not missile code. Real missiles are `G_RunMissile` (eType 4).
7. **`Item_PickupHealth`/`Item_*` use `itemdef_s` (048), `BGItem` (040), `bgItem043_s` (043), `gitem_s` (051)** for the same 0x2c-byte `bg_itemList` entry (0x081850c0); only 043 shows how entries are written.
8. **Entity class table**: named `turret_mode_s` (049, indexed by `turretType`), `entity_handler_s` (047, base shifted +0x14), `entity_class_s` (045, base +0xc), `impact_entry_s` (053, base +0xc), `cod2_entclass_s` (050), `kind callback table` (052, base +0x1c). All are the same table at 0x08187d00 (section 4.2); the byte at +0x166 is `handlerIndex` (047), `turretType` (049), `entClass` (050/103), `entity_class` (045), `impactType` (053).
9. **gentity +0x174** is `spawnFlags` in chunks 049/103 but `flags` (FL_* style, `flags & 1` god, `& 2` demigod) in 047/050/051 and the *map* spawnflags field is +0x170 (script field table `spawnflags` -> 0x170). So `+0x174` = flags, `+0x170` = spawnflags [O from field table + `G_Damage`].
10. **gentity +0x168** is a script-string handle (`classname`, [O: field table, `G_PrintEntities`, `G_RunFrame` debug print]); chunk 049 calls it `modelIndex`, chunk 051 `classname`, 053 `classname`. gentity +0x164 is the `model` field (field table type 8), named `modelName`/`field_164` elsewhere.
11. **gclient/ps +0x60** is `groundEntityNum` (0x3ff = none; chunk 042/043/044 and `Turret_CanBeUsed` requiring it != 0x3ff) but chunk 049 calls it `usingTurretIndex`, chunk 046 `field_60`, chunk 040 `jumpBits60`. ps +0x594 is `linkedEntityNum`/viewlock entity (`ClientSpawn` sets 0x3ff and uses it as turret entity) but 049 calls it `turretEntityNum`, 053 `weaponEntityNum`.
12. **gclient +0x2708 `name` vs +0x2784 `cleanName` (046)**: the script field `name` reads +0x2784 and chunks 047/048/050 call +0x2784 `name`; `ClientUserinfoChanged` writes the cleaned name into +0x2784 first and copies into +0x2708 only when not locked. Treat +0x2784 as the script-visible name and +0x2708 as the "pending/pre-lock" name [I].
13. **`level` global**: declared as `gclient_s *level` (047), `game_client_s *`/`int level` "client array base address value" (052), and `level_locals_t level` (046, size 0x31c). It is a struct at 0x0864c380 whose first word is the client array pointer; 052/047 index `level[i]` as if it were the array [I: they used the pointer value, not the struct]. `Client_GetUnk26b8(i)` etc. in chunk 048 are `level.clients[i].score`/`archiveTime` (+0x26b8 `score`, +0x26b4 `archiveTime` in 046) [O].
14. **`G_ClientIsPlaying` (0x080f9f4e) / `G_IsSessionPlaying` (0x080ff614)** duplicate checks of `sessionState`/`connected`; neither was verified.
15. **chunk 102 types use `struct client_s` for the game client (0x28a4)**; chunk 019-023 use `struct client_s` for the server client (0xb1064). Same name, different structs. Chunk 053 uses `gclient_s`.
16. **`g_dummyEntity` (0x087a1fa0)** is the static world entity (number 0x3fe), not a dummy. `SP_worldspawn` initialises it as `int` (chunk 051 type `int g_dummyEntity`) while 047/048 type it `struct gentity_s` [O sizes]. `Game_FreeAllEntities` frees it when `g_087a209c` (its `inuse` byte, +0xfc) is nonzero.
17. **`G_CallLevelScriptA/B`, `G_CallEntityScriptA/B` (chunk 051)**: A = gametype `main` (0x0884d708), B = `CodeCallback_StartGameType` (0x0884d70c); entity A = `CodeCallback_PlayerConnect` (0x0884d710), B = `CodeCallback_PlayerDisconnect` (0x0884d714). Suggested names: `G_CallGametypeMain`, `G_CallStartGameType`, `G_CallPlayerConnect`, `G_CallPlayerDisconnect`.
18. **`GScr_getclanid/clanname/clanmotto` (0x081105c0..)**, `Scr_RaiseError6`: kept; `GScr_print/println/line` are the `print`, `println`, `line` builtins.
19. **`SV_CheckClientNetAddress` (0x08092472)** is declared `void` and "discards the result" in chunk 021 but `ClientUserinfoChanged` stores its return in `client->isBot`; the binary returns a value (chunk 046 [O], chunk 020 `SV_RunBotClients` uses `remoteAddress.type == 0` [O]).
20. **Duplicated math helpers with address suffixes** (`VectorCopy_080f7c82`, `Vec3_Add_081186c8`, ...) appear in every chunk; they should be one set of inline helpers when merged.
21. **`G_RunThink` (049) uses `ent->nextThink`** at +0x190 and calls the table's `think`; chunk 050 calls +0x190 `expireTime`, 048 `nextThink`. [O: `G_RunThink`].
22. **gentity +0x7c** is `groundEntityNum`-like (0x3ff = none; `ClientSpawn` writes 0x3ff, `cloneplayer` writes 0x3ff, chunk 050 `G_TryPushingEntity` compares it with the pusher's `number`) but chunk 052 names it `targetNum` and chunk 048 `targetClient`. **gentity +0x150** is `ownerNum` (046/047/050; passed as the pass-through entity to `G_TraceBox`), `parentNum` (052), `aimTarget` (049). Ownership vs attachment parent are probably the same field (052 clears it from `G_ClearEntityReferences`) [I].
23. **`g_087a24d0`** (gates `G_CheckVote` in `G_RunFrame` 0x0810a13a [O]) is the dvar registered right after `g_antilag` (0x087a24cc) in `Dvar_RegisterServerDvars`, i.e. `g_oldVoting` (chunk 047 names that variable `dvar_voteMode`, "selects script-vote path when clear") [I from registration order]. `g_allowVote` is a different dvar (0x087a2474, gates `GAME_VOTINGNOTENABLED`).

## 10. Open questions

* [U] Exact meaning of `level+0x200/+0x204` (CS5/6) and `level+0x214`; gametype scripts own the semantics (team scores? name lock?).
* [U] ps +0x5d4..+0x5e0 and the `ps` weapon-view fields (chunk 045/046) overlap with the pmove lane (field names conflict).
* [U] Whether `PlayerCmd_*` weapon-slot functions (0x080fbf60..0x080fc91e) index `weaponSlot` or a larger structure (ammo/clip arrays at +0x144/+0x344 hold 128 ints each; 0x544 owned bitset is 16 bytes).
* [U] The unnamed 73 builtins and 21 entity methods (section 8.3) have not been read; the `Scr_GetFunction` table is the only evidence for their names.
* [U] Item classnames in `bg_itemList` beyond weapons (health, ammo) and the "pickup alias" at +0x14.
* [U] `pm_flags` bit meanings used here: 0x1000 (set on spawn), 0x10 (offhand current), 0x20000 (type-3 weapon current), 0x400000 (dead flag in `player_die`), 0x800000 (turret use), 0x8000 (set when `+0x27b4` set; `freezecontrols`) [O for use, names unknown].
* [U] Behaviour of `G_FireShotgunPellets` (spread patterns) and `Bullet_Fire_Extended` penetration; chunk 052 reading only.
* [U] How `G_PlayerStateToEntityState` smoothing differs from `BG_PlayerStateToEntityState` quantitatively (`g_smoothClients`).
