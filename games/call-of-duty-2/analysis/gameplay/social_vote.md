# Gameplay lane `social_vote`: voting, spectating/follow, quick messages, chat, team changes

Tags: [O] observed in a run on this host, [I] inferred, [U] unknown. Method and controls: `analysis/gameplay-parity.md`.
Plugin: `tools/oracle/scenarios/social_vote.py`. Original: `cod2_lnxded` 1.3 (SHA-256 `05e774a3...5a3a7`).

## Cases

Both cases: `g_gametype tdm`, `+map mp_toujane` (no cheats), `g_allowVote 1`, five fake clients A (allies, thompson), B (axis, mp40), C (allies, m1carbine), D (axis, kar98k), E (spectator). T0 = 20000, pinned seed.

| Case | Length | What it exercises |
|---|---|---|
| `social_vote_tdm_toujane` | 57 s | Every `Cmd_CallVote_f` / `Cmd_Vote_f` reply and `G_CheckVote` outcome, see below. |
| `social_spec_tdm_toujane` | 24 s | Spectator follow, quick messages, chat, team changes mid-game. |

### Vote case
- `rcon set g_allowVote 0` from A's own socket, then `callvote` gives `GAME_VOTINGNOTENABLED`; `set g_allowVote 1` restores it. [O]
- Rejections: spectator `callvote` (`GAME_NOSPECTATORCALLVOTE`), unknown command (`GAME_INVALIDVOTESTRING` plus the `GAME_VOTECOMMANDSARE` list), `kick nobody` and `clientkick 99` (`GAME_CLIENTNOTONSERVER`), unknown map (`the server doesn't have that map`), `typemap xyz` and `g_gametype nogt` (`GAME_INVALIDGAMETYPE`), `typemap tdm mp_toujane` (`GAME_TYPEMAP_NOCHANGE`), `a;b` (`;` check), `vote` with no vote (`GAME_NOVOTEINPROGRESS`), `callvote` during a vote (`GAME_VOTEALREADYINPROGRESS`), second `vote yes` (`GAME_VOTEALREADYCAST`), spectator `vote` (`GAME_NOSPECTATORVOTE`). [O]
- Vote 1: A `callvote kick fakeE`; configstrings 15..18 set; B and C vote yes (`vote y` form); `GAME_VOTEPASSED`; 3 s later `clientkick 4` runs (`EXE_PLAYERKICKED`, E is removed). [O]
- Vote 2: B `callvote map_restart`, D and A vote no: `GAME_VOTEFAILED` as soon as no-votes exceed the remaining quorum. Vote 3: C `callvote g_gametype dm`, B and D vote no: failed. [O]
- Vote 4: D `callvote map_restart`, nobody votes: expires 30 s later through the abstain branch of `G_CheckVote` (`g_voteAbstainWeight`). C's `callvote clientkick 3` in between gives `GAME_VOTEALREADYINPROGRESS`. [O]

### Spec case
- Spectator E: usercmd attack edge = `Cmd_FollowCycle_f(+1)`, ADS (0x1000) edge = `(-1)`, plus `follownext`/`followprev` commands. `ps.clientNum` of E steps 1,2,3,2,1,... while following and returns to 4 (free) for a melee press; follow continues through B going spectator and rejoining. [O]
- `say`, `say_team` from allies, axis and spectator (the spectator's `say_team` becomes `say`), `tell` (unknown client command, `GAME_UNKNOWNCLIENTCOMMAND`). [O]
- Quick messages through `mr <menu> <n>`: `quickcommands`, `quickstatements`, `quickresponses` for allies and axis (sound alias server commands `d` and team chat `i`), the 2 s `spamdelay` drop, a spectator's ignored message. [O]
- Team changes mid-game: A allies -> axis (suicide, `MOD_SUICIDE`, weapon menu), B axis -> spectator -> allies, D `autoassign`, A `kill`. [O]

## Determinism controls
- Each client's reliable commands are at least 1000 ms apart: `sv_floodProtect 1` drops a client command that arrives within about 800 ms of the previous one. A first version with 100 ms spacing lost most commands (identically in every original run, but pointless). [O]
- The `rcon` calls for `g_allowVote` are spaced 1.1 s apart for the same reason (rcon rate limit); the second one was dropped at 400 ms. [O]
- Everything else is the Duel contract (decisions keyed to the deciding client's own snapshot time, pinned seed, full usercmds).

## Counts
| Case | Original runs | Identical / variable items | Recon runs | Mismatches |
|---|---|---|---|---|
| `social_vote_tdm_toujane` | 3 | 221184 / 0 | 2 | 0 |
| `social_spec_tdm_toujane` | 3 | 119239 / 0 | 2 | 0 |
| `duel_dm_toujane` regression (`--tag=-social_vote`) | 3 (existing) | 84560 / 0 | 1 | 0 |

Reconstruction binary SHA-256 `4d1b6174...633e0` (private build `.local/cod2/lanes/social_vote/build`, chunks as of 09:32 on 2026-11-18).

## Fixes
None by this lane. The first reconstruction runs were nondeterministic under CPU load (about one run in three: A and C stayed spectators, `ui_allow_joinauto "1"` instead of `"2"`, 4 of 5 players' scoreboards differ). Evidence [O]:
- `g_scrVar` dumps every 50 ms around level time 7.5 s: in a bad run one player's `pers` array stays empty (`pers["team"] = "spectator"` is lost) and the pers array ids of the bad players are above 0x8000 (33499, 55547).
- The cause is the `movsx` instead of `movzx` of the u16 script object id in `Scr_AddEntityFieldValue` case 7 (0x0811af3c, client `pers`), fixed concurrently by lane `gt_tdm` (`gameplay-fix(gt_tdm)` in chunk_051). After rebuilding with that fix, 24 consecutive concurrent recon runs were stable and both cases match.
- Layout of `g_scrVar` varies run to run before T0 (the original's too), because clients `begin` at wall-clock-dependent frames (level time 7.4 - 8.8 s); an id above 0x8000 for a pers array therefore appears in a fraction of runs, which is why the defect was flaky.

## Not covered / limits
- `callvote map`/`typemap`/`g_gametype`/`map_restart` passing and executing (map change, `fast_restart`, rotation) is not exercised: a passing vote that changes the map would need the second-gamestate path, which `rotate_dm_toujane` already covers for `map_rotate`. [U] whether the vote-triggered `map` command differs.
- `tempBanUser`/`tempBanClient`, `g_oldVoting 0` (script vote mode: `Scr_NotifyEntityCallVote`) and `voteCount` limit (`GAME_MAXVOTESCALLED`) are not covered (voteCount is not incremented in the code paths reached).
- `sv_floodProtect` drops are tested only implicitly; the scoreboard `score` command is not used here.
- Spectator follow of a dead/killcam player and `scr_spectateenemy 0` / `scr_spectatefree 0` permission variants are not covered.
