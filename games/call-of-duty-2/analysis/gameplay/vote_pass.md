# Gameplay lane `vote_pass`: votes that pass and execute

Tags: [O] observed in a run on this host, [I] inferred, [U] unknown. Method and controls: `analysis/gameplay-parity.md`.
Plugin: `tools/oracle/scenarios/vote_pass.py`. Original: `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a3a7`). Complements `social_vote.md`, which covers rejections and failed votes.

## Cases

| Case | Gametype / map | What it exercises |
|---|---|---|
| `vote_pass_tdm_toujane` | tdm toujane, A allies, B axis, C allies, D axis | 1. A `callvote map_restart` passes with B, C: `GAME_VOTEPASSED`, 3 s later `fast_restart` (no new gamestate: `G_InitGame restart=1` at level time 28100, `sv_serverid` 16 -> 17 through `d 1 <systeminfo>`, every player back to spectator and re-picks a team). 2. B `callvote tempBanUser fakeD` passes (A, C yes, D votes no): `tempBanClient "3"` runs, D is removed (`EXE_PLAYERKICKED`, D's stream ends). 3. C `callvote map mp_carentan` passes (quorum 2 of 3 after the kick): `map mp_carentan`, second gamestate (serverid 32). 4. On carentan B `callvote g_gametype dm` passes: `g_gametype dm; map_restart`, third gamestate (serverid 48, dm; players start as spectators). Also `vote` with no vote running (`GAME_NOVOTEINPROGRESS`). |
| `vote_pass_typemap_toujane` | tdm toujane -> dm carentan -> tdm carentan -> dm toujane, A, B, C | A `typemap dm mp_carentan` (-> `g_gametype dm; map mp_carentan`, both parts), B `typemap tdm mp_carentan` on carentan (map name equals `mapname`, so only `g_gametype tdm; map_restart`, a full level reload with new gamestate), A `map_rotate` (`sv_mapRotation "gametype dm map mp_toujane"`): four epochs, three map loads. |
| `vote_pass_script_toujane` | tdm toujane, `g_oldVoting 0` | Script voting: `callvote` (map_restart, kick, map) and `vote yes/no/y` are forwarded to `Scr_NotifyEntityCallVote` / `Scr_NotifyEntityVote` (notify `call_vote` / `vote`); `callvote a;b` still gets `GAME_INVALIDVOTESTRING`; no vote state, no `G_CheckVote`, no config strings 15..18. |

All cases: `g_allowVote 1`, `+map` (no cheats), pinned seed, T0 = 20000. Cases 1 and 2 use `rotate` (end relative to the last level start) and the harness's spawn pad (`SPAWN_PAD_S`, every `G_InitGame` from the 2nd on: the three map loads and the `map_restart`; `fast_restart` is not padded and needs none).

### Facts established from the originals [O]
- A passing vote does not execute at once: `G_CheckVote` sets `pendingVoteExecute = time + 3000` and the command runs 3 s later (`d 15 <end>` configstring cleared at the pass, `d 13` etc. at the restart).
- `fast_restart`: same connection, no gamestate; `d 1` systeminfo carries the new `sv_serverid`; the `mr <serverid> ...` menu responses use the new id (`mr 16` after the restart is silently ignored). The plugin reads the id from the `d 1` command stream (`Scripted.server_id`).
- Teams reset on `fast_restart`, `map` and `map_restart` (clients reconnect through `ClientConnect`); a spectator's `callvote` gives `GAME_NOSPECTATORCALLVOTE`. Menus differ per map: toujane uses `team_britishgerman`/`weapon_british`, carentan `team_americangerman`/`weapon_american`, resolved per gamestate.
- Quorum: yes votes >= `numClients / 2 + 1` (4 clients: 3, 3 clients: 2); the caller counts as yes.
- stock GSC has no `call_vote` / `vote` listener (grep over all `iw_*.iwd` `.gsc`), so script voting is observable only as the absence of replies and of vote state.

## Determinism controls
- Everything keyed to the deciding client's own snapshot (epoch, ms since that epoch's level start). Each client's commands are >= 1000 ms apart (flood protection), and in epochs >= 2 the first command is not sent before 3000 ms: the first snapshot of an epoch is at 2000..2300 ms, and two commands due at once are both sent in one packet and the second is flood-dropped (observed: `mr team` + `mr weapon` at 2100).
- `rotate` must be larger than every intermediate epoch (end triggers on A's time in whatever epoch it is in): 14000 and 12000 here. With 9000 the harness disconnected B and C in the middle of the typemap case.
- Run alone (not 9 sessions in parallel): the map-load pad overran once (`SPAWNPAD rem=-0.020`) under load and moved one epoch.

## Counts
| Case | Original runs | Identical / variable / missing-in-some | Recon runs | Mismatches (deterministic) |
|---|---|---|---|---|
| `vote_pass_tdm_toujane` | 5 | 121506 / 28 / 230 | 3 | 0 |
| `vote_pass_typemap_toujane` | 6 | 76446 / 24 / 186 | 6 (+1 outlier, below) | 0 |
| `vote_pass_script_toujane` | 3 | 39845 / 0 / 0 | 3 | 0 |
| `duel_dm_toujane` regression (`--tag=-vote_pass`) | 3 (existing) | 84560 / 0 | 1 | 0 |

Reconstruction binary SHA-256 `1be05f5e...b105b26` (private build `.local/cod2/lanes/vote_pass/build`, chunks of 2026-11-18 13:07). Scenario `vote_pass.py` SHA-256 `63b2001e...a731a199a` at the time of writing (before this notes file).

### The variable items [O]
All in the first snapshots of an epoch (t+2000..t+2400 after a map load): the first snapshot falls in one frame or the next depending on when the client's `enter` reaches the server (wall clock), so `ps.commandTime` of that snapshot (-50, -100, -200 ...) and the whole first snapshot record are "variable" / "missing in some runs". This is the known map-load boundary family of `rotate_dm_toujane`. In `vote_pass_tdm_toujane` the item `B:e3:t+02300` was identical in the first 3 originals and absent in run 4, so a 3-run comparison can flag it; with 5 originals it is classified correctly.
- Recon run 2 of `vote_pass_typemap_toujane` had the epoch-4 first snapshot one frame late (t+2050/2200/2350 instead of 2000/2100/2200; 1 of 7 recon runs, 0 of 6 originals). Everything after that boundary was identical; the run is kept as `.local/cod2/recon-run/captures/gameplay_vote_pass_typemap_toujane/outlier-run2` and excluded from the 0-mismatch count. [I] same frame-phase race, not a logic defect (no state differs, only which frame the first snapshot occupies).

## Fixes
None: the reconstruction already matches on every item that is deterministic in the original (no chunk edited by this lane).

## Not covered, with evidence
- `GAME_MAXVOTESCALLED`: unreachable. `Cmd_CallVote_f` (0x080ffbb2) tests `client->voteCount > 2` (`cmp DWORD PTR [eax+0x2734],0x2` at 0x080ffc64), and that is the only access to offset 0x2734 of the client structure in the whole binary (`objdump -d | grep 0x2734`: the other hits are unrelated `[ebp-0x2734]` stack slots); nothing increments it, so the limit can never trigger. [O] (static) [I] for the "never" claim: indirect writes through computed pointers were not ruled out by a runtime watch.
- `tempBanUser` is covered; `tempBanClient <n>` as a direct vote (`callvote tempBanClient 3`) takes the same code path with the `target = atoi` branch and was not scripted separately; `kick <name>` and `clientkick <n>` execute are covered by `social_vote`.
- `callvote map_rotate` executes in `vote_pass_typemap_toujane`; with a failing rotation entry or an empty `sv_mapRotation` it is not covered.
- A vote that passes by expiry (abstain weight) and then executes is not covered (the expiry branch of `G_CheckVote` is in `social_vote` for the failing direction).
- Script voting with a mod that handles `call_vote`: the stock data has none; a `fs_game` overlay GSC would be required (not written; the brief says overlays are for stock data only).
