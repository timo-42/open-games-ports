#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_047_types.h"

// 0x080fe9ca  size=1263  FUN_080fe9ca  status=translated  naming=inferred
// naming: dispatched from ClientCommand for the command string "take" (0x08159405); arguments "health"/"weapons"/"ammo"/"allammo"/"all" mirror the "give" command.
void Cmd_Take_f(gentity_s *ent)
{
    int amount;
    const char *item;
    int takeAll;
    int w;
    int ammoIdx;
    int clipIdx;
    gclient_s *client;

    if (CheatsOk(ent) == 0) {
        return;
    }
    amount = atoi(ConcatArgs(2));
    item = ConcatArgs(1);
    if (item == NULL || *item == '\0') {
        return;
    }
    takeAll = (Q_stricmp(item, "all") == 0);

    if (takeAll || Str_NCaseCmp(item, "health", 6) == 0) {
        if (amount == 0) {
            ent->health = 1;
        } else {
            ent->health = ent->health - amount;
            if (ent->health < 1) {
                ent->health = 1;
            }
        }
        if (!takeAll) {
            return;
        }
    }

    if (takeAll || Q_stricmp(item, "weapons") == 0) {
        for (w = 1; w <= BG_GetNumWeapons(); w++) {
            BG_TakeWeapon(&ent->client->ps, w);
            ammoIdx = BG_GetWeaponAmmoIndex(w);
            ent->client->ps.ammo[ammoIdx] = 0;
            clipIdx = BG_GetWeaponClipIndex(w);
            ent->client->ps.ammoclip[clipIdx] = 0;
        }
        if (ent->client->ps.weapon != 0) {
            ent->client->ps.weapon = 0;
            G_SendCharIndexA(ENTNUM(ent), 0);
        }
        if (!takeAll) {
            return;
        }
    }

    if (takeAll || Str_NCaseCmp(item, "ammo", 4) == 0) {
        if (amount == 0) {
            for (w = 1; w <= BG_GetNumWeapons(); w++) {
                ammoIdx = BG_GetWeaponAmmoIndex(w);
                ent->client->ps.ammo[ammoIdx] = 0;
                clipIdx = BG_GetWeaponClipIndex(w);
                ent->client->ps.ammoclip[clipIdx] = 0;
            }
        } else if (ent->client->ps.weapon != 0) {
            w = ent->client->ps.weapon;
            client = ent->client;
            ammoIdx = BG_GetWeaponAmmoIndex(w);
            client->ps.ammo[ammoIdx] = client->ps.ammo[ammoIdx] - amount;
            if (client->ps.ammo[ammoIdx] < 0) {
                clipIdx = BG_GetWeaponClipIndex(w);
                client->ps.ammoclip[clipIdx] = client->ps.ammo[ammoIdx] + client->ps.ammoclip[clipIdx];
                client->ps.ammo[ammoIdx] = 0;
                if (client->ps.ammoclip[clipIdx] < 0) {
                    client->ps.ammoclip[clipIdx] = 0;
                }
            }
        }
        if (!takeAll) {
            return;
        }
    }

    if (Str_NCaseCmp(item, "allammo", 7) == 0 && amount != 0) {
        for (w = 1; w <= BG_GetNumWeapons(); w++) {
            client = ent->client;
            ammoIdx = BG_GetWeaponAmmoIndex(w);
            client->ps.ammo[ammoIdx] = client->ps.ammo[ammoIdx] - amount;
            if (client->ps.ammo[ammoIdx] < 0) {
                clipIdx = BG_GetWeaponClipIndex(w);
                client->ps.ammoclip[clipIdx] = client->ps.ammo[ammoIdx] + client->ps.ammoclip[clipIdx];
                client->ps.ammo[ammoIdx] = 0;
                if (client->ps.ammoclip[clipIdx] < 0) {
                    client->ps.ammoclip[clipIdx] = 0;
                }
            }
        }
    }
}

// 0x080feeba  size=148  FUN_080feeba  status=translated  naming=inferred
// naming: strings GAME_GODMODE_ON/OFF; dispatched for command "god" (0x0815940a).
void Cmd_God_f(gentity_s *ent)
{
    const char *msg;

    if (CheatsOk(ent) != 0) {
        ent->flags ^= 1;
        if ((ent->flags & 1) == 0) {
            msg = "GAME_GODMODE_OFF";
        } else {
            msg = "GAME_GODMODE_ON";
        }
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"%s\"", 0x65, msg));
    }
}

// 0x080fef4e  size=148  FUN_080fef4e  status=translated  naming=inferred
// naming: strings GAME_DEMI_GODMODE_ON/OFF; dispatched for command "demigod".
void Cmd_DemiGod_f(gentity_s *ent)
{
    const char *msg;

    if (CheatsOk(ent) != 0) {
        ent->flags ^= 2;
        if ((ent->flags & 2) == 0) {
            msg = "GAME_DEMI_GODMODE_OFF";
        } else {
            msg = "GAME_DEMI_GODMODE_ON";
        }
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"%s\"", 0x65, msg));
    }
}

// 0x080fefe2  size=142  FUN_080fefe2  status=translated  naming=inferred
// naming: strings GAME_NOTARGETON/OFF; dispatched for command "notarget".
void Cmd_Notarget_f(gentity_s *ent)
{
    const char *msg;

    if (CheatsOk(ent) != 0) {
        ent->flags ^= 4;
        if ((ent->flags & 4) == 0) {
            msg = "GAME_NOTARGETOFF";
        } else {
            msg = "GAME_NOTARGETON";
        }
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"%s\"", 0x65, msg));
    }
}

// 0x080ff070  size=163  FUN_080ff070  status=translated  naming=inferred
// naming: strings GAME_NOCLIPON/OFF; dispatched for command "noclip". Message is chosen before the toggle.
void Cmd_Noclip_f(gentity_s *ent)
{
    const char *msg;

    if (CheatsOk(ent) != 0) {
        if (ent->client->noclip == 0) {
            msg = "GAME_NOCLIPON";
        } else {
            msg = "GAME_NOCLIPOFF";
        }
        ent->client->noclip = (uint32)(ent->client->noclip == 0);
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"%s\"", 0x65, msg));
    }
}

// 0x080ff114  size=163  FUN_080ff114  status=translated  naming=inferred
// naming: strings GAME_UFOON/OFF; dispatched for command "ufo" (0x08159426).
void Cmd_UFO_f(gentity_s *ent)
{
    const char *msg;

    if (CheatsOk(ent) != 0) {
        if (ent->client->ufo == 0) {
            msg = "GAME_UFOON";
        } else {
            msg = "GAME_UFOOFF";
        }
        ent->client->ufo = (uint32)(ent->client->ufo == 0);
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"%s\"", 0x65, msg));
    }
}

// 0x080ff1b8  size=154  FUN_080ff1b8  status=translated  naming=inferred
// naming: dispatched for command "kill" (0x0815942a); calls player_die with means of death 12 (MOD_SUICIDE in the name table at 0x08187c40).
void Cmd_Kill_f(gentity_s *ent)
{
    if (ent->client->sessionState == 0) {
        ent->flags = ent->flags & 0xfffffffc;
        ent->health = 0;
        ent->client->ps.stats[0] = 0;
        player_die(ent, ent, ent, 100000, 12, 0, 0, 0, 0);
    }
}

// 0x080ff252  size=602  FUN_080ff252  status=translated  naming=inferred
// naming: counterpart of Quake III StopFollowing; clears the two spectator target slots and repositions the free camera behind the view.
void StopFollowing(gentity_s *ent)
{
    gclient_s *client;
    float org[3];
    float forward[3];
    float up[3];
    float end[3];
    float mins[3];
    float maxs[3];
    float angles[3];
    trace_s tr;

    client = ent->client;
    client->spectatorClient = -1;
    client->lastFollowedClient = -1;
    if ((client->ps.pm_flags & 0x400000) != 0) {
        G_GetPlayerViewOrigin(ent, org);
        G_GetPlayerViewAxis(ent, forward, 0, up);
        VectorCopy_081012cc(client->ps.viewangles, angles);
        angles[0] = angles[0] + 15.0f;
        VectorMA_081012fc(org, -40.0f, forward, end);
        VectorMA_081012fc(end, 10.0f, up, end);
        VectorSet_081012a8(mins, -8.0f, -8.0f, -8.0f);
        VectorSet_081012a8(maxs, 8.0f, 8.0f, 8.0f);
        G_TraceBox(&tr, org, mins, maxs, end, 0x3ff, 0x810011);
        VectorLerp(org, end, tr.fraction, org);
        client->ps.clientNum = ENTNUM(ent);
        client->ps.eFlags = client->ps.eFlags & 0xfffffcff;
        client->ps.viewlocked = 0;
        client->ps.viewlocked_entNum = 0x3ff;
        client->ps.pm_flags = client->ps.pm_flags & 0xffbfffbf;
        client->ps.fWeaponPosFrac = 0;
        G_SetPositionTrajectory(ent, org);
        VectorCopy_081012cc(org, client->ps.origin);
        SetClientViewAngle(ent, angles);
        client->ps.shellshockIndex = 0;
        client->ps.shellshockTime = 0;
        client->ps.shellshockDuration = 0;
    }
}

// 0x080ff4ac  size=359  FUN_080ff4ac  status=translated  naming=observed
// naming: string "Cmd_FollowCycle_f: bad dir %i" at 0x08158ec0; dispatched for "follownext"/"followprev".
int Cmd_FollowCycle_f(gentity_s *ent, int dir)
{
    int next;
    int start;
    char psBuf[9912];
    char csBuf[96];

    if (dir != 1 && dir != -1) {
        Com_Error(1, "\025Cmd_FollowCycle_f: bad dir %i", dir);
    }
    if (ent->client->sessionState == 2 && ent->client->spectatorClient < 0) {
        next = ent->client->lastFollowedClient;
        if (next < 0) {
            next = 0;
        }
        start = next;
        do {
            next = next + dir;
            if (level_maxclients <= next) {
                next = 0;
            }
            if (next < 0) {
                next = level_maxclients - 1;
            }
            if (SV_GetPlayerStateFromCache(next, &ent->client->archiveTime, (playerState_s *)psBuf, (clientState_s *)csBuf) != 0 &&
                Client_SpectateBitClear(ent->client, *(int *)(csBuf + 4)) != 0) {
                ent->client->lastFollowedClient = next;
                ent->client->sessionState = 2;
                return 1;
            }
        } while (next != start);
    }
    return 0;
}

// 0x080ff614  size=27  FUN_080ff614  status=translated  naming=unknown
// naming: no strings; returns whether the client's sessionState field is zero (descriptive name only).
bool G_IsSessionPlaying(gentity_s *ent)
{
    return ent->client->sessionState == 0;
}

// 0x080ff630  size=259  FUN_080ff630  status=translated  naming=inferred
// naming: Quake III G_SayTo counterpart; builds the "%c \"\025%s%c%c%s\"" chat server command (0x08158edf) and sends it to one client.
void G_SayTo(gentity_s *ent, gentity_s *other, int mode, int color, const char *name, const char *text)
{
    int cmdType;

    if (other != 0 && other->r.inuse != '\0' && other->client != 0 && other->client->connected == 2 &&
        (mode != 1 || OnSameTeam(ent, other) != 0) &&
        (g_087a247c->current.enabled != '\0' || G_IsSessionPlaying(ent) != 0 || G_IsSessionPlaying(other) == 0)) {
        if (mode == 1) {
            cmdType = 0x69;
        } else {
            cmdType = 0x68;
        }
        SV_SendServerCommandNum(ENTNUM(other), 0, va("%c \"\025%s%c%c%s\"", cmdType, name, 0x5e, color, text));
    }
}

// 0x080ff734  size=965  FUN_080ff734  status=translated  naming=inferred
// naming: Quake III G_Say counterpart; strings "say;%d;%d;%s;%s\n" / "sayteam;%d;%d;%s;%s\n", GAME_AXIS/GAME_ALLIES.
void G_Say(gentity_s *ent, gentity_s *target, int mode, const char *text)
{
    const char *teamColor;
    const char *teamLabel;
    char name[68];
    char prefix[64];
    char line[128];
    char body[160];
    int color;
    int i;
    int guid;

    if (mode == 1 && ent->client->cs.team != 1 && ent->client->cs.team != 2) {
        mode = 0;
    }
    Q_strncpyz(name, ent->client->cs.name, 0x40);
    Q_CleanStr(name);
    if (ent->client->cs.team == 1) {
        teamColor = "^9";
    } else if (ent->client->cs.team == 2) {
        teamColor = "^8";
    } else {
        teamColor = "";
    }
    if (ent->client->cs.team == 3) {
        Com_sprintf(prefix, 0x40, "\025(\024GAME_SPECTATOR\025)");
    } else if (ent->client->sessionState == 0) {
        Com_sprintf(prefix, 0x40, "\025%s", teamColor);
    } else {
        Com_sprintf(prefix, 0x40, "\025%s(\024GAME_DEAD\025)", teamColor);
    }
    if (mode == 1) {
        if (ent->client->cs.team == 1) {
            teamLabel = "GAME_AXIS";
        } else {
            teamLabel = "GAME_ALLIES";
        }
        guid = SV_GetClientGuid(ent->s.number);
        G_LogPrintf("sayteam;%d;%d;%s;%s\n", guid, ent->s.number, name, text);
        Com_sprintf(line, 0x80, "%s(\024%s\025)%s%s: ", prefix, teamLabel, name, "^7");
        color = 0x35;
    } else if (mode < 2 || mode != 2) {
        guid = SV_GetClientGuid(ent->s.number);
        G_LogPrintf("say;%d;%d;%s;%s\n", guid, ent->s.number, name, text);
        Com_sprintf(line, 0x80, "%s%s%s: ", prefix, name, "^7");
        color = 0x37;
    } else {
        Com_sprintf(line, 0x80, "%s[%s]%s: ", prefix, name, "^7");
        color = 0x33;
    }
    Q_strncpyz(body, text, 0x96);
    if (target == 0) {
        if (g_087a240c->current.integer != 0) {
            Com_Printf("%s%s\n", line, body);
        }
        for (i = 0; i < level_maxclients; i++) {
            G_SayTo(ent, &g_entities[i], mode, color, line, body);
        }
    } else {
        G_SayTo(ent, target, mode, color, line, body);
    }
}

// 0x080ffafa  size=97  FUN_080ffafa  status=translated  naming=inferred
// naming: dispatched for "say" (mode 0) and "say_team" (mode 1); chooses argument 1, or argument 0 when the third parameter is set.
void Cmd_Say_f(gentity_s *ent, int mode, int arg0)
{
    const char *text;

    if (Cmd_Argc_alt() > 1 || arg0 != 0) {
        if (arg0 == 0) {
            text = ConcatArgs(1);
        } else {
            text = ConcatArgs(0);
        }
        G_Say(ent, 0, mode, text);
    }
}

// 0x080ffb5c  size=85  FUN_080ffb5c  status=translated  naming=inferred
// naming: dispatched for command "where"; sends the entity origin text (Vec3_FormatRing on +0x138) as a server print.
void Cmd_Where_f(gentity_s *ent)
{
    const char *where = Vec3_FormatRing(ent->r.currentOrigin);
    const char *msg = va("%c \"\025%s\n\"", 0x65, where);
    SV_SendServerCommandNum(ENTNUM(ent), 0, msg);
}

// 0x080ffbb2  size=3502  FUN_080ffbb2  status=translated  naming=inferred
// naming: Quake III Cmd_CallVote_f counterpart; strings GAME_VOTINGNOTENABLED, GAME_VOTEALREADYINPROGRESS, GAME_CALLEDAVOTE; dispatched for "callvote".
void Cmd_CallVote_f(gentity_s *ent)
{
    char cmd[260];
    char arg2[256];
    char arg3[256];
    char nameBuf[64];
    int target;
    int i;
    const char *kickCmd;
    const char *s;

    if (dvar_g_allowVote->current.enabled == '\0') {
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_VOTINGNOTENABLED\"", 0x65));
        return;
    }
    if (dvar_voteMode->current.enabled != '\0') {
        if (level_voteTime != 0) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_VOTEALREADYINPROGRESS\"", 0x65));
            return;
        }
        if (2 < ent->client->voteCount) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_MAXVOTESCALLED\"", 0x65));
            return;
        }
        if (ent->client->cs.team == 3) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_NOSPECTATORCALLVOTE\"", 0x65));
            return;
        }
    }
    Cmd_ArgvBuffer_alt(1, cmd, 0x100);
    Cmd_ArgvBuffer_alt(2, arg2, 0x100);
    Cmd_ArgvBuffer_alt(3, arg3, 0x100);

    if (strchr(cmd, 0x3b) != 0 || strchr(arg2, 0x3b) != 0 || strchr(arg3, 0x3b) != 0) {
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_INVALIDVOTESTRING\"", 0x65));
        return;
    }
    if (dvar_voteMode->current.enabled == '\0') {
        Scr_NotifyEntityCallVote(ent, cmd, arg2, arg3);
        return;
    }

    if (Q_stricmp(cmd, "map_restart") != 0 && Q_stricmp(cmd, "map_rotate") != 0 &&
        Q_stricmp(cmd, "typemap") != 0 && Q_stricmp(cmd, "map") != 0 &&
        Q_stricmp(cmd, "g_gametype") != 0 && Q_stricmp(cmd, "kick") != 0 &&
        Q_stricmp(cmd, "clientkick") != 0 && Q_stricmp(cmd, "tempBanUser") != 0 &&
        Q_stricmp(cmd, "tempBanClient") != 0) {
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_INVALIDVOTESTRING\"", 0x65));
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_VOTECOMMANDSARE\025 map_restart, map_rotate, map <mapname>, g_gametype <typename>, typemap <typename> <mapname>, kick <player>, clientkick <clientnum>, tempBanUser <player>, tempBanClient <clientNum>\"", 0x65));
        return;
    }

    if (level_pendingVoteExecute != 0) {
        level_pendingVoteExecute = 0;
        Cbuf_ExecuteText(2, va("%s\n", level_voteString));
    }

    if (Q_stricmp(cmd, "typemap") == 0) {
        if (G_IsGametypeListed(arg2) == 0) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_INVALIDGAMETYPE\"", 0x65));
            return;
        }
        if (Q_stricmp(arg2, dvar_gametype->current.string) == 0) {
            arg2[0] = '\0';
        }
        Cmd_ArgvBuffer_alt(3, arg3, 0x100);
        if (SV_MapFileExists(arg3) == 0) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"\025the server doesn't have that map\"", 0x65));
            return;
        }
        if (Q_stricmp(arg3, Dvar_RegisterString("mapname", "", 0x1044)->current.string) == 0) {
            arg3[0] = '\0';
        }
        if (arg2[0] == '\0' && arg3[0] == '\0') {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_TYPEMAP_NOCHANGE\"", 0x65));
            return;
        }
        if (arg3[0] == '\0') {
            Com_sprintf(level_voteString, 0x400, "g_gametype %s; map_restart", arg2);
            s = (const char *)G_FindGametypeDescriptionByName(arg2);
            Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_GAMETYPE\024%s", s);
        } else {
            if (arg2[0] == '\0') {
                Com_sprintf(level_voteString, 0x400, "map %s", arg3);
            } else {
                Com_sprintf(level_voteString, 0x400, "g_gametype %s; map %s", arg2, arg3);
            }
            if (arg2[0] == '\0') {
                Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_MAP\025%s", arg3);
            } else {
                s = (const char *)G_FindGametypeDescriptionByName(arg2);
                Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_GAMETYPE\024%s\025 - \024GAME_VOTE_MAP\025%s", s, arg3);
            }
        }
    } else if (Q_stricmp(cmd, "g_gametype") == 0) {
        if (G_IsGametypeListed(arg2) == 0) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_INVALIDGAMETYPE\"", 0x65));
            return;
        }
        Com_sprintf(level_voteString, 0x400, "%s %s; map_restart", cmd, arg2);
        s = (const char *)G_FindGametypeDescriptionByName(arg2);
        Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_GAMETYPE\024%s", s);
    } else if (Q_stricmp(cmd, "map_restart") == 0) {
        Com_sprintf(level_voteString, 0x400, "fast_restart");
        Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_MAPRESTART");
    } else if (Q_stricmp(cmd, "map_rotate") == 0) {
        Com_sprintf(level_voteString, 0x400, "%s", cmd);
        Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_NEXTMAP");
    } else if (Q_stricmp(cmd, "map") == 0) {
        if (SV_MapFileExists(arg2) == 0) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"\025the server doesn't have that map\"", 0x65));
            return;
        }
        Com_sprintf(level_voteString, 0x400, "%s %s", cmd, arg2);
        Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_MAP\025%s", arg2);
    } else {
        if (Q_stricmp(cmd, "kick") != 0 && Q_stricmp(cmd, "clientkick") != 0 &&
            Q_stricmp(cmd, "tempBanUser") != 0 && Q_stricmp(cmd, "tempBanClient") != 0) {
            return;
        }
        target = 0x40;
        if (Q_stricmp(cmd, "kick") == 0 || Q_stricmp(cmd, "tempBanUser") == 0) {
            for (i = 0; i < 0x40; i++) {
                if (level[i].connected == 2) {
                    Q_strncpyz(nameBuf, level[i].cs.name, 0x40);
                    Q_CleanStr(nameBuf);
                    if (Q_stricmp(nameBuf, arg2) == 0) {
                        target = i;
                    }
                }
            }
        } else {
            target = atoi(arg2);
            if ((target == 0 && Q_stricmp(arg2, "0") != 0) || target < 0 || 0x3f < target ||
                level[target].connected != 2) {
                target = 0x40;
            } else {
                Q_strncpyz(nameBuf, level[target].cs.name, 0x40);
                Q_CleanStr(nameBuf);
            }
        }
        if (target == 0x40) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_CLIENTNOTONSERVER\"", 0x65));
            return;
        }
        if (cmd[0] == 't' || cmd[0] == 'T') {
            kickCmd = "tempBanClient";
        } else {
            kickCmd = "clientkick";
        }
        Com_sprintf(level_voteString, 0x400, "%s \"%d\"", kickCmd, target);
        Com_sprintf(level_voteDisplayString, 0x400, "GAME_VOTE_KICK\025(%i)%s", target, level[target].cs.name);
    }

    SV_SendServerCommandNum(-1, 0, va("%c \"GAME_CALLEDAVOTE\025%s\"", 0x65, ent->client->cs.name));
    level_voteTime = level_time + 30000;
    level_voteYes = 1;
    level_voteNo = 0;
    for (i = 0; i < level_maxclients; i++) {
        level[i].ps.eFlags = level[i].ps.eFlags & 0xffefffff;
    }
    ent->client->ps.eFlags = ent->client->ps.eFlags | 0x100000;
    SV_SetConfigstring(0xf, va("%i", level_voteTime));
    SV_SetConfigstring(0x10, level_voteDisplayString);
    SV_SetConfigstring(0x11, va("%i", level_voteYes));
    SV_SetConfigstring(0x12, va("%i", level_voteNo));
}

// 0x08100960  size=551  FUN_08100960  status=translated  naming=inferred
// naming: Quake III Cmd_Vote_f counterpart; strings GAME_NOVOTEINPROGRESS, GAME_VOTEALREADYCAST, GAME_VOTECAST; dispatched for command "vote" (0x08159454).
void Cmd_Vote_f(gentity_s *ent)
{
    char answer[64];

    if (dvar_voteMode->current.enabled != '\0') {
        if (level_voteTime == 0) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_NOVOTEINPROGRESS\"", 0x65));
            return;
        }
        if ((ent->client->ps.eFlags & 0x100000) != 0) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_VOTEALREADYCAST\"", 0x65));
            return;
        }
        if (ent->client->cs.team == 3) {
            SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_NOSPECTATORVOTE\"", 0x65));
            return;
        }
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_VOTECAST\"", 0x65));
        ent->client->ps.eFlags = ent->client->ps.eFlags | 0x100000;
    }
    Cmd_ArgvBuffer_alt(1, answer, 0x40);
    if (answer[0] == 'y' || answer[1] == 'Y' || answer[1] == '1') {
        if (dvar_voteMode->current.enabled == '\0') {
            Scr_NotifyEntityVote(ent, "yes");
        } else {
            level_voteYes = level_voteYes + 1;
            SV_SetConfigstring(0x11, va("%i", level_voteYes));
        }
    } else if (dvar_voteMode->current.enabled == '\0') {
        Scr_NotifyEntityVote(ent, "no");
    } else {
        level_voteNo = level_voteNo + 1;
        SV_SetConfigstring(0x12, va("%i", level_voteNo));
    }
}

// 0x08100b88  size=356  FUN_08100b88  status=translated  naming=inferred
// naming: Quake III Cmd_SetViewpos_f counterpart; command "setviewpos", usage string GAME_USAGE: setviewpos x y z yaw, requires the cheat dvar.
void Cmd_SetViewpos_f(gentity_s *ent)
{
    int i;
    char buf[1024];
    float angles[3];
    float origin[3];

    if (dvar_cheats->current.enabled == '\0') {
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_CHEATSNOTENABLED\"", 0x65));
    } else if (Cmd_Argc_alt() == 5) {
        VectorClear_0810127e(angles);
        for (i = 0; i < 3; i++) {
            Cmd_ArgvBuffer_alt(i + 1, buf, 0x400);
            origin[i] = (float)atof(buf);
        }
        Cmd_ArgvBuffer_alt(4, buf, 0x400);
        angles[1] = (float)atof(buf);
        origin[2] = origin[2] - ent->client->ps.viewHeightCurrent;
        G_TeleportEntity(ent, origin, angles);
    } else {
        SV_SendServerCommandNum(ENTNUM(ent), 0, va("%c \"GAME_USAGE\025: setviewpos x y z yaw\"", 0x65));
    }
}

// 0x08100cec  size=42  FUN_08100cec  status=translated  naming=inferred
// naming: string "entity count = %i\n"; dispatched for command "entitycount" (gated by the cheat dvar).
void Cmd_EntityCount_f(void)
{
    if (dvar_cheats->current.enabled != '\0') {
        Com_Printf("entity count = %i\n", level_num_entities);
    }
}

// 0x08100d16  size=318  FUN_08100d16  status=translated  naming=inferred
// naming: dispatched for command "mr" (0x081593fd, menu response); string "sv_serverId" checked against argument 1, "bad" is the fallback payload.
void Cmd_MenuResponse_f(gentity_s *ent)
{
    int menuIndex;
    char serverId[1024];
    char menuName[1024];
    char response[1024];

    menuIndex = -1;
    if (Cmd_Argc_alt() == 4) {
        Cmd_ArgvBuffer_alt(1, serverId, 0x400);
        if (atoi(serverId) != Dvar_GetInt("sv_serverId")) {
            return;
        }
        Cmd_ArgvBuffer_alt(2, menuName, 0x400);
        menuIndex = atoi(menuName);
        if (-1 < menuIndex && menuIndex < 0x20) {
            SV_GetConfigstring(menuIndex + 0x4de, menuName, 0x400);
        }
        Cmd_ArgvBuffer_alt(3, response, 0x400);
    } else {
        menuName[0] = '\0';
        strcpy(response, "bad");
    }
    Scr_AddString(response);
    Scr_AddString(menuName);
    Scr_NotifyEntity(ent, g_scrCallbackMenuResponse, 2);
}

// 0x08100e54  size=13  FUN_08100e54  status=translated  naming=inferred
// naming: dispatched for command "printentities"; thin wrapper over G_PrintEntities.
void Cmd_PrintEntities_f(void)
{
    G_PrintEntities();
}

// 0x08100e62  size=1052  FUN_08100e62  status=translated  naming=inferred
// naming: Quake III ClientCommand counterpart: reads command name with Cmd_ArgvBuffer, falls through string comparisons, replies GAME_UNKNOWNCLIENTCOMMAND.
void ClientCommand(int clientNum)
{
    gentity_s *ent;
    char cmd[1036];
    char reply[1088];

    ent = &g_entities[clientNum];
    if (ent->client != 0) {
        Cmd_ArgvBuffer_alt(0, cmd, 0x400);
        if (Q_stricmp(cmd, "say") == 0) {
            Cmd_Say_f(ent, 0, 0);
        } else if (Q_stricmp(cmd, "say_team") == 0) {
            Cmd_Say_f(ent, 1, 0);
        } else if (Q_stricmp(cmd, "score") == 0) {
            Cmd_Score_f(ent);
        } else if (ent->client->ps.pm_type != 5) {
            if (Q_stricmp(cmd, "mr") == 0) {
                Cmd_MenuResponse_f(ent);
            } else if (Q_stricmp(cmd, "give") == 0) {
                Cmd_Give_f(ent);
            } else if (Q_stricmp(cmd, "take") == 0) {
                Cmd_Take_f(ent);
            } else if (Q_stricmp(cmd, "god") == 0) {
                Cmd_God_f(ent);
            } else if (Q_stricmp(cmd, "demigod") == 0) {
                Cmd_DemiGod_f(ent);
            } else if (Q_stricmp(cmd, "notarget") == 0) {
                Cmd_Notarget_f(ent);
            } else if (Q_stricmp(cmd, "noclip") == 0) {
                Cmd_Noclip_f(ent);
            } else if (Q_stricmp(cmd, "ufo") == 0) {
                Cmd_UFO_f(ent);
            } else if (Q_stricmp(cmd, "kill") == 0) {
                Cmd_Kill_f(ent);
            } else if (Q_stricmp(cmd, "follownext") == 0) {
                Cmd_FollowCycle_f(ent, 1);
            } else if (Q_stricmp(cmd, "followprev") == 0) {
                Cmd_FollowCycle_f(ent, -1);
            } else if (Q_stricmp(cmd, "where") == 0) {
                Cmd_Where_f(ent);
            } else if (Q_stricmp(cmd, "callvote") == 0) {
                Cmd_CallVote_f(ent);
            } else if (Q_stricmp(cmd, "vote") == 0) {
                Cmd_Vote_f(ent);
            } else if (Q_stricmp(cmd, "setviewpos") == 0) {
                Cmd_SetViewpos_f(ent);
            } else if (Q_stricmp(cmd, "entitycount") == 0) {
                Cmd_EntityCount_f();
            } else if (Q_stricmp(cmd, "printentities") == 0) {
                Cmd_PrintEntities_f();
            } else {
                Com_sprintf(reply, 0x440, "%c \"GAME_UNKNOWNCLIENTCOMMAND\025%s\"", 0x65, cmd);
                SV_SendServerCommandNum(clientNum, 0, reply);
            }
        }
    }
}

// 0x0810127e  size=41  FUN_0810127e  status=translated  naming=inferred
// naming: zeroes three floats (Quake III VectorClear shape); duplicated vector helper, address suffix keeps the copies distinct.
void VectorClear_0810127e(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x081012a8  size=35  FUN_081012a8  status=translated  naming=inferred
// naming: stores three floats (VectorSet shape).
void VectorSet_081012a8(float *v, float x, float y, float z)
{
    /* the original moves the three argument dwords with integer movs (no x87 load, so signalling NaN payloads survive) */
    ((volatile uint32 *)v)[0] = *(volatile uint32 *)&x;
    ((volatile uint32 *)v)[1] = *(volatile uint32 *)&y;
    ((volatile uint32 *)v)[2] = *(volatile uint32 *)&z;
}

// 0x081012cc  size=47  FUN_081012cc  status=translated  naming=inferred
// naming: copies three floats (VectorCopy shape).
void VectorCopy_081012cc(const float *in, float *out)
{
    /* integer dword moves, as in the original (no x87 load/store, payload bits preserved) */
    ((volatile uint32 *)out)[0] = ((const volatile uint32 *)in)[0];
    ((volatile uint32 *)out)[1] = ((const volatile uint32 *)in)[1];
    ((volatile uint32 *)out)[2] = ((const volatile uint32 *)in)[2];
}

// 0x081012fc  size=83  FUN_081012fc  status=translated  naming=inferred
// naming: out = a + scale * b (VectorMA shape).
void VectorMA_081012fc(const float *a, float scale, const float *b, float *out)
{
    out[0] = a[0] + scale * b[0];
    out[1] = a[1] + scale * b[1];
    out[2] = a[2] + scale * b[2];
}

// 0x08101350  size=26  FUN_08101350  status=translated  naming=unknown
// naming: no strings; strcpy wrapper passed as the string-copy callback of the info-field parser (descriptive name only).
void G_InfoFieldStrcpyCallback(char *dst, const char *src)
{
    strcpy(dst, src);
}

// 0x0810136a  size=665  FUN_0810136a  status=translated  naming=inferred
// naming: strings "info/mp_lochit_dmgtable", "LOCDMGTABLE" and the "hitloc damage table" error messages; fills the 19-entry multiplier and id tables.
void G_ParseHitLocDmgTable(void)
{
    int i;
    int len;
    int handle;
    InfoField_s fields[19];
    char buf[8192];
    const char *path;
    const char *header;
    size_t headerLen;

    path = "info/mp_lochit_dmgtable";
    header = "LOCDMGTABLE";
    headerLen = strlen("LOCDMGTABLE");
    for (i = 0; i < 0x13; i++) {
        g_hitLocMultiplier[i] = 1.0f;
        fields[i].name = g_hitLocNames[i];
        fields[i].offset = i << 2;
        fields[i].type = 6;
        g_hitLocIds[i] = (short)ScrStr_InternCString(g_hitLocNames[i], 1);
    }
    g_hitLocMultiplier[18] = 0.0f;
    len = FSH_FOpenFile(path, &handle, 0);
    if (len < 1) {
        Com_Error(1, "\025Could not load hitloc damage table %s\n", path);
    }
    FS_Read(buf, (int)headerLen, handle);
    buf[headerLen] = '\0';
    if (strncmp(buf, header, headerLen) != 0) {
        Com_Error(1, "\025\"%s\" does not appear to be a hitloc damage table\n", path);
    }
    if (0x1fff < (int)(len - headerLen)) {
        Com_Error(1, "\025\"%s\" Is too long of a hitloc damage table to parse\n", path);
    }
    FS_Read(buf, (int)(len - headerLen), handle);
    buf[len - headerLen] = '\0';
    FS_FCloseFile(handle);
    if (Info_Validate(buf) == 0) {
        Com_Error(1, "\025\"%s\" is not a valid hitloc damage table\n", path);
    }
    if (Com_ParseInfoFields((char *)g_hitLocMultiplier, (InfoField *)fields, 0x13, buf, 0, 0, (void (*)(void *, const char *))G_InfoFieldStrcpyCallback) == 0) {
        Com_Error(1, "\025Error parsing hitloc damage table %s\n", path);
    }
}

// 0x08101604  size=5  FUN_08101604  status=translated  naming=unknown
// naming: single ret instruction; empty function (descriptive name only).
void NoOp_08101604(void)
{
}

// 0x0810160a  size=239  FUN_0810160a  status=translated  naming=inferred
// naming: Quake III LookAtKiller counterpart: stores the yaw towards the attacker (or inflictor, or own yaw) in client+0x130.
void G_SetKillerYaw(gentity_s *self, gentity_s *inflictor, gentity_s *attacker)
{
    float dir[5];

    if (attacker == 0 || attacker == self) {
        if (inflictor == 0 || inflictor == self) {
            self->client->ps.stats[1] = (int)self->r.currentAngles[1];
            return;
        }
        VectorSubtract_08102942(inflictor->r.currentOrigin, self->r.currentOrigin, dir);
    } else {
        VectorSubtract_08102942(attacker->r.currentOrigin, self->r.currentOrigin, dir);
    }
    self->client->ps.stats[1] = (int)Vec2_AngleDegrees(dir);
    Vec2_AngleDegrees(dir);
}

// 0x081016fa  size=96  FUN_081016fa  status=translated  naming=observed
// naming: string "Unknown means of death string '%s'\n" at this function; looks the name up in the MOD_ table at 0x08187c40.
int G_MeansOfDeathFromString(const char *name)
{
    int i;

    for (i = 0; i <= 0xe; i++) {
        if (Q_stricmp(name, g_modNames[i]) == 0) {
            return i;
        }
    }
    Com_Printf("Unknown means of death string '%s'\n", name);
    return 0;
}

// 0x0810175a  size=904  FUN_0810175a  status=translated  naming=inferred
// naming: Quake III player_die counterpart: sets dead pm_type, drops item, runs script death callback, frees followers, sets death contents.
void player_die(gentity_s *self, gentity_s *inflictor, gentity_s *attacker, int damage, int mod, int weapon, const float *dir, int hitLoc, int extra)
{
    gclient_s *client;
    float vel[3];
    float org[3];
    int i;
    int result;

    client = self->client;
    if (Com_GetServerDObj(client->ps.clientNum) != 0 && (client->ps.pm_type == 0 || client->ps.pm_type == 1) &&
        (client->ps.pm_flags & 0x400000) == 0) {
        g_gameContextPtr = g_levelBlock;
        if (attacker->s.eType == 9 && attacker->r.ownerNum != 0x3ff) {
            attacker = &g_entities[attacker->r.ownerNum];
        }
        Scr_AddEntityParam(attacker);
        Scr_NotifyEntity(self, g_scrCallbackDeath, 1);
        if (weapon != 0 && attacker->client != 0 && (attacker->client->ps.eFlags & 0x300) != 0 &&
            g_entities[attacker->s.otherEntityNum].s.eType == 9) {
            weapon = g_entities[attacker->s.otherEntityNum].s.weapon;
        }
        if (self->client->ps.grenadeTimeLeft != 0) {
            vel[0] = Rand_Signed();
            vel[1] = Rand_Signed();
            vel[2] = Rand_Fraction();
            VectorScale_08102986(vel, 160.0f, vel);
            VectorCopy_081028ce(self->r.currentOrigin, org);
            org[2] = org[2] + 40.0f;
            G_SpawnGrenade(self, org, vel, self->client->ps.offHandIndex, self->client->ps.grenadeTimeLeft);
        }
        if (self->client->ps.pm_type == 1) {
            self->client->ps.pm_type = 7;
        } else {
            self->client->ps.pm_type = 6;
        }
        result = BG_AnimScriptEvent(&self->client->ps, 1, 0, 1);
        G_CallPlayerKilledCallback(self, inflictor, attacker, damage, mod, weapon, dir, hitLoc, extra, result);
        for (i = 0; i < level_maxclients; i++) {
            gclient_s *other = &level[i];
            if (other->connected == 2 && other->sessionState == 2 && other->lastFollowedClient == self->s.number) {
                Cmd_Score_f(&g_entities[i]);
            }
        }
        self->takedamage = 1;
        self->r.contents = 0x4000000;
        self->r.currentAngles[2] = 0.0f;
        G_SetKillerYaw(self, inflictor, attacker);
        VectorCopy_081028ce(self->r.currentAngles, self->client->ps.viewangles);
        self->s.loopSound = 0;
        SV_UnlinkEntity(self);
        self->r.maxs[2] = 30.0f;
        SV_LinkEntity(self);
        self->health = 0;
        self->handler = 0xb;
    }
}

// 0x08101ae2  size=94  FUN_08101ae2  status=translated  naming=inferred
// naming: returns the hit-location damage multiplier: per-weapon table at weaponDef+0x5b0 unless the weapon is missing or flag +0x78 is set, else the global table at 0x08628ee0.
float G_GetHitLocMultiplier(int hitLoc, int weapon)
{
    float mult;
    weaponDef_s *def;

    if (weapon == 0) {
        mult = g_hitLocMultiplier[hitLoc];
    } else {
        def = (weaponDef_s *)BG_GetWeaponDef(weapon);
        if (def == 0 || def->weaponType != 0) {
            mult = g_hitLocMultiplier[hitLoc];
        } else {
            mult = def->locationDamageMultipliers[hitLoc];
        }
    }
    return mult;
}

// 0x08101b40  size=279  FUN_08101b40  status=translated  naming=unknown
// naming: no strings; client branch of G_Damage: scales damage by the hit-location multiplier (truncating cast) and forwards to G_CallPlayerDamageCallback (descriptive name only).
void G_DamageClient(gentity_s *self, gentity_s *inflictor, gentity_s *attacker, const float *dir, const float *point, int damage, int dflags, int mod, int hitLoc, int extra)
{
    int weapon;
    float fdamage;

    if (self->takedamage != 0 && self->client->noclip == 0 && self->client->ufo == 0 &&
        self->client->connected == 2) {
        if (inflictor == 0) {
            if (attacker == 0) {
                weapon = 0;
            } else {
                weapon = attacker->s.weapon;
            }
        } else {
            weapon = inflictor->s.weapon;
        }
        fdamage = (float)damage;
        damage = (int)(G_GetHitLocMultiplier(hitLoc, weapon) * fdamage);
        if (0 < damage) {
            G_CallPlayerDamageCallback(self, inflictor, attacker, damage, dflags, mod, weapon, point, dir, hitLoc, extra);
        }
    }
}

// 0x08101c58  size=653  FUN_08101c58  status=translated  naming=inferred
// naming: Quake III G_Damage counterpart; string "target:%i health:%i damage:%i\n", flag bits 1 (god) / 2 (demigod), script notify with 2 args, die/pain function table.
void G_Damage(gentity_s *self, gentity_s *inflictor, gentity_s *attacker, const float *dir, const float *point, int damage, int dflags, int mod, int hitLoc, int extra)
{
    float dirBuf[6];

    if (self->client == 0) {
        if (self->takedamage != 0) {
            if (inflictor == 0) {
                inflictor = &g_dummyEntity;
            }
            if (attacker == 0) {
                attacker = &g_dummyEntity;
            }
            Vec3_NormalizeCopy(dir, dirBuf);
            if ((self->flags & 1) == 0) {
                if (damage < 1) {
                    damage = 1;
                }
                if ((self->flags & 2) != 0 && self->health - damage < 1) {
                    damage = self->health - 1;
                }
                if (g_087a242c->current.enabled != '\0') {
                    Com_Printf("target:%i health:%i damage:%i\n", self->s.number, self->health, damage);
                }
                self->health = self->health - damage;
                Scr_AddEntityParam(attacker);
                Scr_AddIndex(damage);
                Scr_NotifyEntity(self, g_scrCallbackDamage, 2);
                if (self->health < 1) {
                    if (self->health < -999) {
                        self->health = -999;
                    }
                    Scr_AddEntityParam(attacker);
                    Scr_NotifyEntity(self, g_scrCallbackDeath, 1);
                    if (g_entityHandlers[self->handler].die != 0) {
                        g_entityHandlers[self->handler].die(self, inflictor, attacker, damage, mod, inflictor->s.weapon, dirBuf, hitLoc, extra);
                    }
                } else if (g_entityHandlers[self->handler].pain != 0) {
                    g_entityHandlers[self->handler].pain(self, attacker, damage, point, mod, dirBuf, hitLoc);
                }
            }
        }
    } else {
        G_DamageClient(self, inflictor, attacker, dir, point, damage, dflags, mod, hitLoc, extra);
    }
}

// 0x08101ee6  size=954  FUN_08101ee6  status=translated  naming=inferred
// naming: Quake III CanDamage counterpart returning a visibility fraction: line traces (mask 0x802891) from 5 sample points; no strings.
float G_CanDamage(gentity_s *ent, const float *origin)
{
    float result;
    int hits;
    int i;
    float viewOrigin[3];
    float dir[3];
    float perp[3];
    float pts[5][3];
    float zOffset;

    if (ent->client == 0) {
        VectorAdd_081028fe(ent->r.absmin, ent->r.absmax, pts[0]);
        VectorScale_08102986(pts[0], 0.5f, pts[0]);
        VectorCopy_081028ce(pts[0], pts[1]);
        pts[1][0] = pts[1][0] + 15.0f;
        pts[1][1] = pts[1][1] + 15.0f;
        VectorCopy_081028ce(pts[0], pts[2]);
        pts[2][0] = pts[2][0] + 15.0f;
        pts[2][1] = pts[2][1] - 15.0f;
        VectorCopy_081028ce(pts[0], pts[3]);
        pts[3][0] = pts[3][0] - 15.0f;
        pts[3][1] = pts[3][1] + 15.0f;
        VectorCopy_081028ce(pts[0], pts[4]);
        pts[4][0] = pts[4][0] - 15.0f;
        pts[4][1] = pts[4][1] - 15.0f;
        for (i = 0; i < 5; i++) {
            if (G_TraceIsClearPoint(pts[i], origin, ent->s.number, 0x802891) != 0) {
                return 1.0f;
            }
        }
        result = 0.0f;
    } else {
        G_GetPlayerViewOrigin(ent, viewOrigin);
        zOffset = (viewOrigin[2] - ent->r.currentOrigin[2]) * 0.5f;
        VectorSubtract_08102942(origin, ent->r.currentOrigin, dir);
        dir[2] = 0.0f;
        Vec3_Normalize(dir);
        perp[0] = -dir[1];
        perp[1] = dir[0];
        perp[2] = dir[2];
        VectorAdd_081028fe(viewOrigin, ent->r.currentOrigin, pts[0]);
        VectorScale_08102986(pts[0], 0.5f, pts[0]);
        VectorMA_081029be(pts[0], 15.0f, perp, pts[1]);
        pts[1][2] = pts[1][2] + zOffset;
        VectorMA_081029be(pts[0], 15.0f, perp, pts[2]);
        pts[2][2] = pts[2][2] - zOffset;
        VectorMA_081029be(pts[0], -15.0f, perp, pts[3]);
        pts[3][2] = pts[3][2] + zOffset;
        VectorMA_081029be(pts[0], -15.0f, perp, pts[4]);
        pts[4][2] = pts[4][2] - zOffset;
        hits = 0;
        for (i = 0; i < 5; i++) {
            if (G_TraceIsClearPoint(origin, pts[i], ent->s.number, 0x802891) != 0) {
                hits = hits + 1;
            }
        }
        if (hits == 0) {
            result = 0.0f;
        } else if (hits < 4) {
            result = (float)hits / 3.0f;
        } else {
            result = 1.0f;
        }
    }
    return result;
}

// 0x081022a0  size=1494  FUN_081022a0  status=translated  naming=inferred
// naming: Quake III G_RadiusDamage counterpart; no strings; collects entities in a box of radius*sqrt(2), scales damage linearly with distance and calls G_Damage with dflags 1. Returns 1 when G_ClientCanTargetEntity reports a hit on any victim.
int G_RadiusDamage(const float *origin, gentity_s *inflictor, gentity_s *attacker, float damageMax, float damageMin, float radius, gentity_s *ignore, int mod)
{
    int hit;
    int i;
    int n;
    int num;
    int entNum;
    float boxExtent;
    float dist;
    float damage;
    float visibility;
    float v[3];
    float mins[3];
    float maxs[3];
    float dir[3];
    float mid[3];
    float tmp[3];
    trace_s tr;
    int list[1024];
    gentity_s *victim;

    hit = 0;
    if (attacker == 0) {
        return 0;
    }
    if (radius < 1.0f) {
        radius = 1.0f;
    }
    boxExtent = radius * 1.4142135f;
    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - boxExtent;
        maxs[i] = origin[i] + boxExtent;
    }
    num = CM_AreaEntities(mins, maxs, list, 0x400, -1);
    for (n = 0; n < num; n++) {
        entNum = list[n];
        victim = &g_entities[entNum];
        if (victim != ignore && victim->takedamage != 0) {
            if (victim->r.bmodel == 0) {
                VectorSubtract_08102942(victim->r.currentOrigin, origin, v);
            } else {
                for (i = 0; i < 3; i++) {
                    if (origin[i] < victim->r.absmin[i]) {
                        v[i] = victim->r.absmin[i] - origin[i];
                    } else if (victim->r.absmax[i] < origin[i]) {
                        v[i] = origin[i] - victim->r.absmax[i];
                    } else {
                        v[i] = 0.0f;
                    }
                }
            }
            dist = VectorLength_08102a12(v);
            if (dist < radius && (victim->client == 0 || g_radiusDamageActiveSource == 0)) {
                damage = damageMin + (1.0f - dist / radius) * (damageMax - damageMin);
                visibility = G_CanDamage(victim, origin);
                if (0.0f < visibility) {
                    if (G_ClientCanTargetEntity(victim, attacker) != 0) {
                        hit = 1;
                    }
                    VectorSubtract_08102942(victim->r.currentOrigin, origin, dir);
                    dir[2] = dir[2] + 24.0f;
                    G_Damage(victim, inflictor, attacker, dir, origin, (int)(damage * visibility), 1, mod, 0, 0);
                } else {
                    VectorAdd_081028fe(victim->r.absmin, victim->r.absmax, tmp);
                    VectorScale_08102986(tmp, 0.5f, tmp);
                    VectorCopy_081028ce(tmp, mid);
                    G_TraceBox(&tr, (float *)origin, vec3_origin, vec3_origin, mid, 0x3ff, 0x811);
                    if (tr.fraction < 1.0f) {
                        VectorSubtract_08102942(mid, origin, mid);
                        dist = VectorLength_08102a12(mid);
                        if (dist < radius * 0.2f) {
                            if (G_ClientCanTargetEntity(victim, attacker) != 0) {
                                hit = 1;
                            }
                            VectorSubtract_08102942(victim->r.currentOrigin, origin, dir);
                            dir[2] = dir[2] + 24.0f;
                            G_Damage(victim, inflictor, attacker, dir, origin, (int)(damage * 0.1f), 1, mod, 0, 0);
                        }
                    }
                }
            }
        }
    }
    return hit;
}

// 0x08102876  size=16  FUN_08102876  status=translated  naming=unknown
// naming: no strings; returns entry i of the 19-entry hit-location id table at 0x08628f40 (descriptive name only).
short G_GetHitLocId(int index)
{
    return g_hitLocIds[index];
}

// 0x08102886  size=72  FUN_08102886  status=translated  naming=unknown
// naming: no strings; reverse lookup in the hit-location id table, returns 0 when absent (descriptive name only).
int G_GetHitLocIndexFromId(short id)
{
    int i;

    for (i = 0; i <= 0x12; i++) {
        if (g_hitLocIds[i] == id) {
            return i;
        }
    }
    return 0;
}

// 0x081028ce  size=47  FUN_081028ce  status=translated  naming=inferred
// naming: copies three floats (VectorCopy shape); duplicate of the helper at 0x081012cc.
void VectorCopy_081028ce(const float *in, float *out)
{
    /* integer dword moves, as in the original (no x87 load/store, payload bits preserved) */
    ((volatile uint32 *)out)[0] = ((const volatile uint32 *)in)[0];
    ((volatile uint32 *)out)[1] = ((const volatile uint32 *)in)[1];
    ((volatile uint32 *)out)[2] = ((const volatile uint32 *)in)[2];
}

// 0x081028fe  size=68  FUN_081028fe  status=translated  naming=inferred
// naming: out = a + b (VectorAdd shape).
void VectorAdd_081028fe(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x08102942  size=68  FUN_08102942  status=translated  naming=inferred
// naming: out = a - b (VectorSubtract shape).
void VectorSubtract_08102942(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x08102986  size=56  FUN_08102986  status=translated  naming=inferred
// naming: out = scale * v (VectorScale shape).
void VectorScale_08102986(const float *v, float scale, float *out)
{
    out[0] = scale * v[0];
    out[1] = scale * v[1];
    out[2] = scale * v[2];
}

// 0x081029be  size=83  FUN_081029be  status=translated  naming=inferred
// naming: out = a + scale * b (VectorMA shape); duplicate of 0x081012fc.
void VectorMA_081029be(const float *a, float scale, const float *b, float *out)
{
    out[0] = a[0] + scale * b[0];
    out[1] = a[1] + scale * b[1];
    out[2] = a[2] + scale * b[2];
}

// 0x08102a12  size=62  FUN_08102a12  status=translated  naming=inferred
// naming: sqrt(z*z + y*y + x*x) of a vec3 (VectorLength shape); the decompiler shows void but the x87 result of the callee is returned.
float VectorLength_08102a12(const float *v)
{
    return Sqrt_Float_08102a50(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]);
}

// 0x08102a50  size=31  FUN_08102a50  status=translated  naming=unknown
// naming: no strings; float -> double sqrt -> float (descriptive name only).
float Sqrt_Float_08102a50(float value)
{
    double r = sqrt((double)value);
    return (float)r;
}

// 0x08102a70  size=5  FUN_08102a70  status=translated  naming=unknown
// naming: single ret; debug line draw stub called with two points and three pass-through words by the circle/arc helpers (descriptive name only).
void G_DebugLine_08102a70(const float *a, const float *b, uint32 c1, uint32 c2, uint32 c3)
{
}

// 0x08102d14  size=470  FUN_08102d14  status=translated  naming=unknown
// naming: no strings; builds 16 points on a circle around a centre in the plane perpendicular to a normal and draws the 16 segments (descriptive name only).
void G_DebugCircle_08102d14(const float *center, float radius, const float *normal, uint32 c1, uint32 c2, uint32 c3)
{
    float sinv;
    float cosv;
    float angle;
    float axisA[3];
    float axisB[3];
    float axisC[3];
    float pts[16][3];
    uint32 i;

    Vec3_NormalizeCopy(normal, axisA);
    Vec3_PerpendicularVector(axisA, axisB);
    Vec3_Cross(axisA, axisB, axisC);
    for (i = 0; i < 0x10; i++) {
        angle = (float)((double)(int)i * 0.39269908169872414);
        SinCos_08103092(angle, &sinv, &cosv);
        sinv = sinv * radius;
        cosv = cosv * radius;
        VectorMA_081031bc(center, sinv, axisC, pts[i]);
        VectorMA_081031bc(pts[i], cosv, axisB, pts[i]);
    }
    for (i = 0; i < 0x10; i++) {
        G_DebugLine_08102a70(pts[i], pts[(i + 1) & 0xf], c1, c2, c3);
    }
}

// 0x08102eea  size=424  FUN_08102eea  status=translated  naming=unknown
// naming: no strings; builds 16 points along an arc between two angles in degrees (wraps by 360 when the span is negative) and draws 15 segments (descriptive name only).
void G_DebugArc_08102eea(const float *center, float radius, float startAngle, float endAngle, uint32 c1, uint32 c2, uint32 c3)
{
    float sinv;
    float cosv;
    float angle;
    float step;
    float pts[16][3];
    uint32 i;

    step = (endAngle - startAngle) / 15.0f;
    if (step < 0.0f) {
        startAngle = startAngle - 360.0f;
        step = (endAngle - startAngle) / 15.0f;
    }
    for (i = 0; i < 0x10; i++) {
        angle = (float)(((float)(int)i * step + startAngle) * 0.017453292519943295);
        SinCos_08103092(angle, &sinv, &cosv);
        pts[i][0] = center[0] + cosv * radius;
        pts[i][1] = center[1] + sinv * radius;
        pts[i][2] = center[2];
    }
    for (i = 0; i < 0xf; i++) {
        G_DebugLine_08102a70(pts[i], pts[i + 1], c1, c2, c3);
    }
}

// 0x08103092  size=45  FUN_08103092  status=translated  naming=unknown
// naming: no strings; sin/cos of a float angle (radians) stored through two float pointers (descriptive name only).
void SinCos_08103092(float angle, float *sinOut, float *cosOut)
{
    double r;

    r = sin((double)angle);
    *sinOut = (float)r;
    r = cos((double)angle);
    *cosOut = (float)r;
}

// 0x081030c0  size=31  FUN_081030c0  status=translated  naming=unknown
// naming: copies two floats (Vec2Copy shape).
void Vec2Copy_081030c0(const float *in, float *out)
{
    ((volatile uint32 *)out)[0] = ((const volatile uint32 *)in)[0];
    ((volatile uint32 *)out)[1] = ((const volatile uint32 *)in)[1];
}

// 0x081030e0  size=35  FUN_081030e0  status=translated  naming=inferred
// naming: stores three floats (VectorSet shape); duplicate of 0x081012a8.
void VectorSet_081030e0(float *v, float x, float y, float z)
{
    /* the original moves the three argument dwords with integer movs (no x87 load, so signalling NaN payloads survive) */
    ((volatile uint32 *)v)[0] = *(volatile uint32 *)&x;
    ((volatile uint32 *)v)[1] = *(volatile uint32 *)&y;
    ((volatile uint32 *)v)[2] = *(volatile uint32 *)&z;
}

// 0x08103104  size=47  FUN_08103104  status=translated  naming=inferred
// naming: copies three floats (VectorCopy shape); duplicate.
void VectorCopy_08103104(const float *in, float *out)
{
    /* integer dword moves, as in the original (no x87 load/store, payload bits preserved) */
    ((volatile uint32 *)out)[0] = ((const volatile uint32 *)in)[0];
    ((volatile uint32 *)out)[1] = ((const volatile uint32 *)in)[1];
    ((volatile uint32 *)out)[2] = ((const volatile uint32 *)in)[2];
}

// 0x08103134  size=68  FUN_08103134  status=translated  naming=inferred
// naming: out = a + b (VectorAdd shape); duplicate.
void VectorAdd_08103134(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x08103178  size=68  FUN_08103178  status=translated  naming=inferred
// naming: out = a - b (VectorSubtract shape); duplicate.
void VectorSubtract_08103178(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x081031bc  size=83  FUN_081031bc  status=translated  naming=inferred
// naming: out = a + scale * b (VectorMA shape); duplicate.
void VectorMA_081031bc(const float *a, float scale, const float *b, float *out)
{
    out[0] = a[0] + scale * b[0];
    out[1] = a[1] + scale * b[1];
    out[2] = a[2] + scale * b[2];
}

// 0x08103210  size=158  FUN_08103210  status=translated  naming=inferred
// naming: zeroes the dynamic fields of a hudelem pool entry (used by HudElem_Init); pool identified by the "hudelem field" string in HudElem_SetEnumField.
void HudElem_ClearFields(game_hudelem_s *he)
{
    he->elem.width = 0;
    he->elem.height = 0;
    he->elem.materialIndex = 0;
    he->elem.fromX = 0;
    he->elem.fromY = 0;
    he->elem.fromAlignOrg = 0;
    he->elem.fromAlignScreen = 0;
    he->elem.fromWidth = 0;
    he->elem.fromHeight = 0;
    he->elem.scaleStartTime = 0;
    he->elem.scaleTime = 0;
    he->elem.time = 0;
    he->elem.duration = 0;
    he->elem.value = 0;
    he->elem.text = 0;
}

// 0x081032ae  size=187  FUN_081032ae  status=translated  naming=inferred
// naming: initialises a hudelem pool entry (inUse=1, 1.0f at +0x10, -1 at +0x20) then clears dynamic fields.
void HudElem_Init(game_hudelem_s *he)
{
    he->elem.type = 1;
    he->elem.x = 0;
    he->elem.y = 0;
    he->elem.z = 0;
    he->elem.fontScale = 1.0f;
    he->elem.font = 0;
    he->elem.alignOrg = 0;
    he->elem.alignScreen = 0;
    he->elem.color = -1;
    he->elem.fromColor = 0;
    he->elem.fadeStartTime = 0;
    he->elem.fadeTime = 0;
    he->elem.label = 0;
    he->elem.sort = 0;
    he->elem.foreground = 0;
    he->archived = 1;
    HudElem_ClearFields(he);
}

// 0x0810336a  size=142  FUN_0810336a  status=translated  naming=inferred
// naming: first-free allocation from the 0x400-entry hudelem pool at 0x08628f80 (entry size 0x8c); stores two key words at +0x80/+0x84; returns NULL when full.
game_hudelem_s *HudElem_Alloc(int ownerKey, int secondKey)
{
    uint32 i;

    for (i = 0; i <= 0x3ff; i++) {
        if (g_hudelems[i].elem.type == 0) {
            break;
        }
    }
    if (0x3ff < i) {
        return 0;
    }
    HudElem_Init(&g_hudelems[i]);
    g_hudelems[i].clientNum = ownerKey;
    g_hudelems[i].team = secondKey;
    return &g_hudelems[i];
}

// 0x081033f8  size=28  FUN_081033f8  status=translated  naming=inferred
// naming: releases script references of a hudelem (HudElem_FreeScriptObject) then marks the pool entry unused.
void HudElem_Free(game_hudelem_s *he)
{
    HudElem_FreeScriptObject(he);
    he->elem.type = 0;
}

// 0x08103414  size=97  FUN_08103414  status=translated  naming=inferred
// naming: frees every in-use hudelem whose key word at +0x80 equals the first word of the given owner record.
void HudElem_FreeAllForOwner(const struct gentity_s *owner)
{
    uint32 i;

    for (i = 0; i < 0x400; i++) {
        if (g_hudelems[i].elem.type != 0 && g_hudelems[i].clientNum == owner->s.number) {
            HudElem_Free(&g_hudelems[i]);
        }
    }
}

// 0x08103476  size=103  FUN_08103476  status=translated  naming=inferred
// naming: frees every in-use hudelem then zeroes the whole pool (0x23000 bytes).
void HudElem_FreeAll(void)
{
    uint32 i;

    for (i = 0; i < 0x400; i++) {
        if (g_hudelems[i].elem.type != 0) {
            HudElem_Free(&g_hudelems[i]);
        }
    }
    memset(g_hudelems, 0, 0x23000);
}

// 0x081034de  size=314  FUN_081034de  status=translated  naming=observed
// naming: string "\"%s\" is not a valid value for hudelem field \"%s\"\nShould be one of:" at this function; stores a matched enum index into a bit field.
void HudElem_SetEnumField(char *base, hudelem_field_s *field, const char *const *names, int count)
{
    uint32 *target;
    const char *value;
    int i;
    char msg[2048];

    target = (uint32 *)(base + field->offset);
    value = Scr_ReleaseAnyValue(0);
    i = 0;
    while (true) {
        if (count <= i) {
            sprintf(msg, "\"%s\" is not a valid value for hudelem field \"%s\"\nShould be one of:", value, field->name);
            for (i = 0; i < count; i++) {
                strncat(msg, va(" %s", names[i]), 0x800);
                msg[2047] = '\0';
            }
            Scr_Error(msg);
            return;
        }
        if (Q_stricmp(value, names[i]) == 0) {
            break;
        }
        i = i + 1;
    }
    *target = ~(field->mask << (field->shift & 0x1f)) & *target;
    *target = (uint32)i << (field->shift & 0x1f) | *target;
}
