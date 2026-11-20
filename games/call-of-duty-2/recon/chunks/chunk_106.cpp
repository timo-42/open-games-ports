#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_106_types.h"

/*
 * chunk 106 (0x08114bb6..0x08119f22): GSC builtin glue for fx, fog, shellshock,
 * team/winner configstrings, turret limits, model bone queries, map commands and
 * script mover entry points. Bodies were recovered from .eh_frame FDE starts.
 */

/* gentity+0x1b4 = u.mover view +0xc; used as the trigger claim slot (0x3ff none) */
#define TRIG_CLAIM(e) (*(int *)((byte *)(e) + 0x1b4))
#define GAME_ENT(p) ((struct gentity_s *)(p))
#define CS_EFFECTS_BASE 0x34e          /* configstring base of effect names (fx id + base) */
#define CS_SHELLSHOCK_BASE 0x48e       /* configstring base of shellshock names */

static inline gentity_s *ScrEntityParam(uint32 param)
{
    return (gentity_s *)Scr_GetEntityParam(param);
}

/* Scr_GetString: text of the parameter (Scr_GetAny + ScrStr_TextFromHandle). */
static inline const char *Scr_GetStringParam(uint32 param)
{
    return Scr_ReleaseAnyValue(param);
}

/* ===== fx spawn at a point, radius damage, anim sampling ===== */

// 0x08114bb6  size=264  FUN_08114bb6  status=translated  naming=unknown
// Spawns the 0xbc temp event at a script point lifted 1 unit, with up-facing normal,
// and traces 17 units down (mins/maxs zero) to record the surface flags. No identifying strings.
void GScr_grenadeexplosioneffect(void)
{
    trace_t tr;
    float point[3];
    float origin[3];
    float up[3];
    float traceEnd[3];
    gentity_s *ent;

    Scr_GetVec(0, point);
    Vec3_CopyF(point, origin);
    origin[2] = origin[2] + 1.0f;
    ent = (gentity_s *)G_TempEntity(origin, 0xbc);
    Vec3_Set_0805528e(up, 0, 0, 0x3f800000u);
    ent->s.eventParm = Normal_DirToIndex(up) & 0xff;
    Vec3_CopyF(origin, traceEnd);
    traceEnd[2] = traceEnd[2] - 17.0f;
    G_TraceBox(&tr, origin, (float *)vec3_origin, (float *)vec3_origin, traceEnd, 0x3ff, 0x811);
    ent->s.surfType = (int)((unsigned int)tr.surfaceFlags & 0x1f00000u) >> 0x14;
}

// 0x08114cbe  size=156  FUN_08114cbe  status=translated  naming=inferred
// Radius damage builtin: args (origin, radius, damageMax, damageMin); the attacker is the
// world entity g_087a1fa0 and the mod code is 0xe. Calls G_RadiusDamage (name from the shared
// header). Name inferred from the builtin's behaviour (radiusDamage lineage).
void GScr_RadiusDamage(void)
{
    float origin[3];
    float radius;
    float damageMax;
    float damageMin;

    Scr_GetVec(0, origin);
    radius = Scr_GetFloat(1);
    damageMax = Scr_GetFloat(2);
    damageMin = Scr_GetFloat(3);
    g_radiusDamageActiveSource = level_ignorePlayerRadiusDamage;
    G_RadiusDamage(origin, NULL, (struct gentity_s *)g_087a1fa0, damageMax, damageMin, radius, NULL, 0xe);
    g_radiusDamageActiveSource = 0;
}

// 0x08114d5a  size=25  FUN_08114d5a  status=translated  naming=unknown
// Stores script param 0 (int) into the radius-damage source selector level_ignorePlayerRadiusDamage.
void GScr_setplayerignoreradiusdamage(void)
{
    level_ignorePlayerRadiusDamage = Scr_GetInt(0);
}

// 0x08114d74  size=279  FUN_08114d74  status=translated  naming=unknown
// Samples an anim node (tree from Scr_GetAnim, node low 16 bits) between optional start/end
// times in [0,1] and pushes the blended vector. Error strings name the time params only.
void GScr_getmovedelta(void)
{
    int nargs;
    uint32 animRef;
    uint32 treeId;
    float endTime;
    float startTime;
    float blendPos[3];
    float blendVec[3];

    startTime = 0.0f;
    endTime = 1.0f;
    nargs = Scr_GetNumParams();
    if (nargs != 1) {
        if (nargs != 2) {
            endTime = Scr_GetFloat(2);
            if (endTime < 0.0f || 1.0f < endTime) {
                Scr_ErrorAtParam(2, "end time must be between 0 and 1");
            }
        }
        startTime = Scr_GetFloat(1);
        if (startTime < 0.0f || 1.0f < startTime) {
            Scr_ErrorAtParam(1, "start time must be between 0 and 1");
        }
    }
    Scr_GetAnim((struct cod2_scr_anim_s *)&animRef, 0, 0);
    treeId = animRef;
    {
        void *table = (void *)(size_t)Anim_GetTreeSlot((int)(treeId >> 0x10));
        XAnim_NodeBlendSample((struct XAnimNodeTable *)table, (int)(treeId & 0xffff),
                              blendPos, blendVec, startTime, endTime);
    }
    Scr_AddVec(blendVec);
}

// 0x08114e8c  size=287  FUN_08114e8c  status=translated  naming=unknown
// Same sampling as GScr_getmovedelta, but pushes the planar angle (degrees) of the
// sampled blend quaternion as a float. The quaternion read covers the 12-byte blend buffer.
void GScr_getangledelta(void)
{
    int nargs;
    uint32 animRef;
    uint32 treeId;
    float endTime;
    float startTime;
    float blendPos[3];
    float blendVec[3];

    startTime = 0.0f;
    endTime = 1.0f;
    nargs = Scr_GetNumParams();
    if (nargs != 1) {
        if (nargs != 2) {
            endTime = Scr_GetFloat(2);
            if (endTime < 0.0f || 1.0f < endTime) {
                Scr_ErrorAtParam(2, "end time must be between 0 and 1");
            }
        }
        startTime = Scr_GetFloat(1);
        if (startTime < 0.0f || 1.0f < startTime) {
            Scr_ErrorAtParam(1, "start time must be between 0 and 1");
        }
    }
    Scr_GetAnim((struct cod2_scr_anim_s *)&animRef, 0, 0);
    treeId = animRef;
    {
        void *table = (void *)(size_t)Anim_GetTreeSlot((int)(treeId >> 0x10));
        XAnim_NodeBlendSample((struct XAnimNodeTable *)table, (int)(treeId & 0xffff),
                              blendPos, blendVec, startTime, endTime);
    }
    Scr_AddFloat(Quat_PlanarAngleDeg(blendPos));
}

// 0x08114fac  size=60  FUN_08114fac  status=translated  naming=unknown
// Reads configstring 0x0b (up to 0x20 bytes) and pushes it as a float via atof.
void GScr_getnorthyaw(void)
{
    char text[40];

    SV_GetConfigstring(0x0b, text, 0x20);
    Scr_AddFloat((float)atof(text));
}

// 0x08114fe8  size=75  FUN_08114fe8  status=translated  naming=observed
// loadFx builtin. Evidence: string "loadFx must be called before any wait statements in the
// gametype or level script, or on an already loaded effect" referenced here.
void GScr_LoadFx(void)
{
    const char *name;
    int effectId;

    name = Scr_GetStringParam(0);
    effectId = G_EffectIndex(name);
    if (effectId == 0 && level_initializing == 0) {
        Scr_Error("loadFx must be called before any wait statements in the gametype or level script, or on an already loaded effect\n");
    }
    Scr_AddIndex(effectId);
}

// 0x081152d6  size=400  FUN_081152d6  status=translated  naming=observed
// playFxOnTag builtin. Evidence: string "USAGE: playFxOnTag <effect id from loadFx> <entity> <tag name>".
// The "tag does not exist" error passes the tag string handle (not its text) to va();
// that is reproduced as found in the binary.
void GScr_PlayFxOnTag(void)
{
    int nargs;
    int effectId;
    gentity_s *ent;
    uint32 tagHandle;
    const char *tagText;
    int tagIndex;
    const char *name;
    int fxConfigIndex;

    nargs = Scr_GetNumParams();
    if (nargs != 3) {
        Scr_Error("USAGE: playFxOnTag <effect id from loadFx> <entity> <tag name>");
    }
    effectId = Scr_GetInt(0);
    if (effectId < 1 || 0x3f < effectId) {
        Scr_ErrorAtParam(0, va("effect id %i is invalid\n", effectId));
    }
    ent = ScrEntityParam(1);
    if (ent->model == 0) {
        Scr_ErrorAtParam(1, "cannot play fx on entity with no model");
    }
    tagHandle = Scr_GetLowerString(2);
    tagText = ScrStr_TextFromHandle(tagHandle);
    if (strchr(tagText, '"') != NULL) {
        Scr_ErrorAtParam(2, "cannot use \" characters in tag names\n");
    }
    tagIndex = SV_DObjGetBoneIndexByName(ent, (int)tagHandle);
    if (tagIndex < 0) {
        SV_PrintModelInfo((const int *)ent);
        {
            const char *modelName = G_GetModelName(ent->model);
            const char *msg = va("tag '%s' does not exist on entity with model '%s'",
                                 (const char *)(size_t)tagHandle, modelName);
            Scr_ErrorAtParam(2, msg);
        }
    }
    tagText = ScrStr_TextFromHandle(tagHandle);
    name = va("%02d%s", effectId, tagText);
    fxConfigIndex = G_FindConfigstringIndex(name, 0x38e, 0x100, 1, NULL);
    G_AddEvent(GAME_ENT(ent), 0xc3, fxConfigIndex);
}

// 0x08115466  size=693  FUN_08115466  status=translated  naming=observed
// playLoopedFx builtin. Evidence: string "USAGE: playLoopedFx <effect id from loadFx> <repeat delay> ..."
// Optional forward/up vectors are normalized in place (zero-length is an error); the
// repeat delay is seconds * 1000 rounded. Spawns a looped temp entity (type 8, flag 8).
void GScr_PlayLoopedFx(void)
{
    char effectName[1032];
    float fwd[3];
    float up[3];
    float pos[3];
    float cullDist;
    int repeatMs;
    uint32 effectId;
    int nargs;
    gentity_s *ev;

    nargs = Scr_GetNumParams();
    if (nargs < 3 || 6 < nargs) {
        Scr_Error("USAGE: playLoopedFx <effect id from loadFx> <repeat delay> <vector position of effect> <optional cull distance (0 = never cull)> <optional forward vector> <optional up vector>");
    }
    Vec3_Clear_0811864a(fwd);
    Vec3_Clear_0811864a(up);
    cullDist = 0.0f;
    effectId = (uint32)Scr_GetInt(0);
    nargs = Scr_GetNumParams();
    if (nargs == 6) {
        Scr_GetVec(5, up);
        if (Vec3_Normalize(up) == 0.0f) {
            if (effectId == 0) {
                strcpy(effectName, "not successfully loaded");
            } else {
                SV_GetConfigstring((int)(effectId + CS_EFFECTS_BASE), effectName, 0x400);
            }
            Scr_Error(va("playLoopedFx called with (0 0 0) up direction (effect = %s)\n", effectName));
        }
    }
    if (nargs == 5 || nargs == 6) {
        Scr_GetVec(4, fwd);
        if (Vec3_Normalize(fwd) == 0.0f) {
            if (effectId == 0) {
                strcpy(effectName, "not successfully loaded");
            } else {
                SV_GetConfigstring((int)(effectId + CS_EFFECTS_BASE), effectName, 0x400);
            }
            Scr_Error(va("playLoopedFx called with (0 0 0) forward direction (effect = %s)\n", effectName));
        }
    }
    if (4 <= nargs) {
        cullDist = Scr_GetFloat(3);
    }
    Scr_GetVec(2, pos);
    repeatMs = Float_RoundHalfUp(Scr_GetFloat(1) * 1000.0f);
    if (repeatMs < 1) {
        /* Original passes the int to a %f conversion; reproduced as the same int argument. */
        Scr_Error(va("playLoopedFx called with %f s repeat (should be >= 0.0005s)\n", repeatMs));
    }
    ev = (gentity_s *)G_Spawn();
    ev->s.eType = 8;
    ev->r.svFlags = (byte)(ev->r.svFlags | 8);
    ev->s.scale = (int)(effectId & 0xff);
    G_SetPositionTrajectory(GAME_ENT(ev), pos);
    Vec3_CopyF(fwd, ev->s.origin2);
    ev->s.eventParm = Normal_DirToIndex(up) & 0xff;
    ev->s.angles2[0] = cullDist;
    ev->s.angles2[1] = (float)repeatMs;
    SV_LinkEntity((gentity_s *)ev);
    Scr_AddEntityParam((gentity_s *)ev);
}

// 0x08115910  size=210  FUN_08115910  status=translated  naming=observed
// setExpFog builtin. Evidence: string "setExpFog" and its USAGE text referenced here.
void GScr_SetExpFog(void)
{
    float density;
    float red;
    float green;
    float blue;
    float transition;

    if (Scr_GetNumParams() != 5) {
        Scr_Error("USAGE: setExpFog(density, red, green, blue, transition time);\nDensity must be greater than 0 and less than 1, and typically less than .001.  For example, .0002 means the fog gets .02%% more dense for every 1 unit of distance (about 1%% thicker every 50 units of distance)\n");
    }
    density = Scr_GetFloat(0);
    red = Scr_GetFloat(1);
    green = Scr_GetFloat(2);
    blue = Scr_GetFloat(3);
    transition = Scr_GetFloat(4);
    if (density <= 0.0f || 1.0f <= density) {
        Scr_Error("setExpFog: distance must be greater than 0 and less than 1");
    }
    GScr_SetCullFogParams("setExpFog", 0.0f, 1.0f, density, red, green, blue, transition);
}

// 0x081159e2  size=95  FUN_081159e2  status=translated  naming=unknown
// Pushes 1 when script param 0 is an entity of type 1 with pointer value 0x15 and a client
// record, otherwise 0. Name not recovered from strings.
void GScr_isplayer(void)
{
    gentity_s *ent;

    if (Scr_GetParamType(0) == 1 && Scr_GetPointer(0) == 0x15) {
        ent = ScrEntityParam(0);
        if (ent->client != NULL) {
            Scr_AddIndex(1);
            return;
        }
    }
    Scr_AddIndex(0);
}

// 0x08115a42  size=63  FUN_08115a42  status=translated  naming=unknown
// Pushes 1 when script param 0 is a valid effect id (1..63), otherwise 0.
void GScr_isplayernumber(void)
{
    int id;

    id = Scr_GetInt(0);
    if (id < 0 || 0x3f < id) {
        Scr_AddIndex(0);
    } else {
        Scr_AddIndex(1);
    }
}

// 0x08115a82  size=189  FUN_08115a82  status=translated  naming=unknown
// Writes the "winner" key of configstring 0x16 to the entity number + 1 when it changes.
// Key name "winner" and format "%i" are literals in the binary and used here.
void GScr_setwinningplayer(void)
{
    gentity_s *ent;
    int winnerValue;
    char info[1032];
    const char *newValue;
    const char *current;

    ent = ScrEntityParam(0);
    winnerValue = ent->s.number + 1;
    SV_GetConfigstring(0x16, info, 0x400);
    newValue = va("%i", winnerValue);
    current = Info_ValueForKey(info, "winner");
    if (Q_stricmp(current, newValue) != 0) {
        Info_SetValueForKey(info, "winner", newValue);
        SV_SetConfigstring(0x16, info);
    }
}

// 0x08115b40  size=290  FUN_08115b40  status=translated  naming=unknown
// Sets the "winner" key from a team handle: allies = -2, axis = -1, none = 0; other handles
// are a script error. Same configstring update as GScr_setwinningplayer.
void GScr_setwinningteam(void)
{
    short team;
    int winnerValue;
    char info[1028];
    const char *newValue;
    const char *current;

    team = (short)Scr_GetAny(0);
    if (team == scr_str_allies) {
        winnerValue = -2;
    } else if (team == scr_str_axis) {
        winnerValue = -1;
    } else {
        if (team != scr_str_none) {
            const char *text = ScrStr_TextFromHandle((uint32)(int32)team);
            Scr_ErrorAtParam(0, va("Illegal team string '%s'. Must be allies, axis, or none.", text));
            return;
        }
        winnerValue = 0;
    }
    SV_GetConfigstring(0x16, info, 0x400);
    newValue = va("%i", winnerValue);
    current = Info_ValueForKey(info, "winner");
    if (Q_stricmp(current, newValue) != 0) {
        Info_SetValueForKey(info, "winner", newValue);
        SV_SetConfigstring(0x16, info);
    }
}

// 0x08115c62  size=115  FUN_08115c62  status=translated  naming=unknown
// Sends the "Announcement" message built from all script params to every client.
void GScr_announcement(void)
{
    int nargs;
    char message[1032];

    nargs = Scr_GetNumParams();
    GScr_BuildMessageString(0, nargs - 1, "Announcement", message, 0x400);
    SV_SendServerCommandNum(-1, 0, va("%c \"%s\" 2", 'c', message));
}

// 0x08115cd6  size=131  FUN_08115cd6  status=translated  naming=unknown
// Sends the "Announcement" message built from params 1.. to the entity in param 0.
void GScr_clientannouncement(void)
{
    gentity_s *ent;
    int nargs;
    char message[1036];

    ent = ScrEntityParam(0);
    nargs = Scr_GetNumParams();
    GScr_BuildMessageString(1, nargs - 1, "Announcement", message, 0x400);
    SV_SendServerCommandNum(ent->s.number, 0, va("%c \"%s\" 2", 'c', message));
}

// 0x08115d5a  size=128  FUN_08115d5a  status=translated  naming=unknown
// Pushes the team score slot (allies level_teamScoreAllies / axis level_teamScoreAxis) for a team handle.
void GScr_getteamscore(void)
{
    short team;
    int value;

    team = (short)Scr_GetAny(0);
    if (team != scr_str_allies && team != scr_str_axis) {
        const char *text = ScrStr_TextFromHandle((uint32)(int32)team);
        Scr_Error(va("Illegal team string '%s'. Must be allies, or axis.", text));
    }
    if (team == scr_str_allies) {
        value = level_teamScoreAllies;
    } else {
        value = level_teamScoreAxis;
    }
    Scr_AddIndex(value);
}

// 0x08115dda  size=248  FUN_08115dda  status=translated  naming=unknown
// Stores a team score (param 1) for the team handle in param 0, announces it with "H"/"G" and
// sets the team dirty flag level_teamScoresDirty.
void GScr_setteamscore(void)
{
    short team;
    int value;
    const char *text;

    team = (short)Scr_GetAny(0);
    if (team != scr_str_allies && team != scr_str_axis) {
        text = ScrStr_TextFromHandle((uint32)(int32)team);
        Scr_Error(va("Illegal team string '%s'. Must be allies, or axis.", text));
    }
    value = Scr_GetInt(1);
    if (team == scr_str_allies) {
        level_teamScoreAllies = value;
        SV_SendServerCommandNum(-1, 0, va("%c %i", 'H', value));
    } else {
        level_teamScoreAxis = value;
        SV_SendServerCommandNum(-1, 0, va("%c %i", 'G', value));
    }
    level_teamScoresDirty = 1;
}

// 0x08115ed2  size=86  FUN_08115ed2  status=translated  naming=unknown
// Selects the manual-change mode flag level_manualChangeMode from one of two mode handles.
void GScr_setclientnamemode(void)
{
    short mode;

    mode = (short)Scr_GetAny(0);
    if (mode == scr_str_auto_change) {
        level_manualChangeMode = 0;
    } else if (mode == scr_str_manual_change) {
        level_manualChangeMode = 1;
    } else {
        Scr_Error("Unknown mode");
    }
}

// 0x08115f28  size=206  FUN_08115f28  status=translated  naming=unknown
// Only valid in manual-change mode: for each client in state 2 whose pending name differs from its
// name, swap the two names (32-byte copies) and call ClientUserinfoChanged.
void GScr_updateclientnames(void)
{
    char tmp[44];
    int i;
    gclient_s *client;

    if (level_manualChangeMode == 0) {
        Scr_Error("Only works in [manual_change] mode");
    }
    client = level_clients;
    for (i = 0; i < level_maxclients; i = i + 1) {
        if (client->connected == 2 && strcmp(client->cs.name, client->newnetname) != 0) {
            Q_strncpyz(tmp, client->cs.name, 0x20);
            Q_strncpyz(client->cs.name, client->newnetname, 0x20);
            ClientUserinfoChanged(i);
        }
        client = (gclient_s *)((byte *)client + 0x28a4);
    }
}

// 0x08115ff6  size=233  FUN_08115ff6  status=translated  naming=unknown
// Counts live (inUse, health > 0) entities whose client team matches the team handle in param 0
// (allies = 2, axis = 1). Iterates the entity count at G_LEVEL_RECORD[2].
void GScr_getteamplayersalive(void)
{
    short team;
    int teamValue;
    int count;
    int i;

    team = (short)Scr_GetAny(0);
    if (team != scr_str_allies && team != scr_str_axis) {
        const char *text = ScrStr_TextFromHandle((uint32)(int32)team);
        Scr_Error(va("Illegal team string '%s'. Must be allies, or axis.", text));
    }
    if (team == scr_str_allies) {
        teamValue = 2;
    } else {
        teamValue = 1;
    }
    count = 0;
    for (i = 0; i < G_LEVEL_RECORD[2]; i = i + 1) {
        if (g_gentities[i].r.inuse != 0
            && g_gentities[i].client->cs.team == teamValue
            && 0 < g_gentities[i].health) {
            count = count + 1;
        }
    }
    Scr_AddIndex(count);
}

// 0x081160e0  size=50  FUN_081160e0  status=translated  naming=unknown
// Pushes the bone count of the model named by param 0.
void GScr_getnumparts(void)
{
    void *model;

    model = SV_RegisterModel(Scr_GetStringParam(0));
    Scr_AddIndex(XModelRef_GetHeaderShort((const struct XModelRef *)model));
}

// 0x08116112  size=167  FUN_08116112  status=translated  naming=unknown
// Pushes the string id of bone name index param 1 of the model named by param 0; out-of-range
// index and zero bone ids are script errors.
void GScr_getpartname(void)
{
    void *model;
    uint32 index;
    uint32 count;
    int16 boneId;

    model = SV_RegisterModel(Scr_GetStringParam(0));
    index = (uint32)Scr_GetInt(1);
    count = (uint32)XModelRef_GetHeaderShort((const struct XModelRef *)model);
    if (count <= index) {
        Scr_ErrorAtParam(1, va("index out of range (0 - %d)", (int)(count - 1)));
    }
    boneId = (int16)XModel_GetBoneNames((const struct XModel *)model)[index];
    if (boneId == 0) {
        Scr_ErrorAtParam(0, "bad model");
    }
    Scr_AddStringId((uint32)(int32)boneId);
}

// 0x081161ba  size=231  FUN_081161ba  status=translated  naming=unknown
// Spawns a 0xc4 temp event at the origin in param 2 with scale (param 0), duration in ms
// (param 1 * 1000, rounded) and radius (param 3). All checks are script errors.
void GScr_earthquake(void)
{
    float scale;
    int duration;
    float origin[3];
    float radius;
    gentity_s *ent;

    scale = Scr_GetFloat(0);
    duration = Float_RoundHalfUp(1000.0f * Scr_GetFloat(1));
    Scr_GetVec(2, origin);
    radius = Scr_GetFloat(3);
    if (scale <= 0.0f) {
        Scr_ErrorAtParam(0, "Scale must be greater than 0");
    }
    if (duration < 1) {
        Scr_ErrorAtParam(1, "duration must be greater than 0");
    }
    if (radius <= 0.0f) {
        Scr_ErrorAtParam(3, "Radius must be greater than 0");
    }
    ent = (gentity_s *)G_TempEntity(origin, 0xc4);
    ent->s.angles2[0] = scale;
    ent->s.time = duration;
    ent->s.angles2[1] = radius;
}

// 0x081162a2  size=392  FUN_081162a2  status=translated  naming=observed
// shellshock builtin for a player (self ref). Evidence: "USAGE: <player> shellshock(<shellshockname>, <duration>)"
// and "shellshock '%s' was not precached" referenced here.
void GScr_Shellshock(uint32 self)
{
    gentity_s *player;
    int nargs;
    const char *name;
    char cs[1024];
    int index;
    int duration;

    player = GScr_GetPlayerEntity(self);
    nargs = Scr_GetNumParams();
    if (nargs != 2) {
        Scr_Error("USAGE: <player> shellshock(<shellshockname>, <duration>)\n");
    }
    name = Scr_GetStringParam(0);
    index = 1;
    while (true) {
        if (0xf < index) {
            Scr_Error(va("shellshock '%s' was not precached\n", name));
            return;
        }
        SV_GetConfigstring(index + CS_SHELLSHOCK_BASE, cs, 0x400);
        if (strcasecmp(cs, name) == 0) {
            break;
        }
        index = index + 1;
    }
    duration = Float_RoundHalfUp(1000.0f * Scr_GetFloat(1));
    if (duration < 0 || 60000 < duration) {
        Scr_ErrorAtParam(1, va("duration %g should be >= 0 and <= 60", (double)(0.001f * (float)duration)));
    }
    player->client->ps.shellshockIndex = index;
    player->client->ps.shellshockTime = g_0864c56c;
    player->client->ps.shellshockDuration = duration;
    if (player->health < 1) {
        return;
    }
    g_gameContextPtr = g_levelBlock;
    BG_AnimScriptEvent((struct playerState_s *)player->client, 0x12, 0, 1);
}

// 0x0811642a  size=100  FUN_0811642a  status=translated  naming=observed
// stopshellshock builtin for a player (self ref). Evidence: "USAGE: <player> stopshellshock()".
void GScr_StopShellshock(uint32 self)
{
    gentity_s *player;

    player = GScr_GetPlayerEntity(self);
    if (Scr_GetNumParams() != 0) {
        Scr_Error("USAGE: <player> stopshellshock()\n");
    }
    player->client->ps.shellshockIndex = 0;
    player->client->ps.shellshockTime = 0;
    player->client->ps.shellshockDuration = 0;
}

// 0x0811648e  size=230  FUN_0811648e  status=translated  naming=observed
// viewkick builtin for a player (self ref). Evidence: "USAGE: <player> viewkick <force 0-127> <source position>".
// Damage = (force * ent.unk_198 + 50) / 100; the view direction is source minus client origin.
void GScr_ViewKick(uint32 self)
{
    gentity_s *player;
    int nargs;
    int force;
    float source[3];

    player = GScr_GetPlayerEntity(self);
    nargs = Scr_GetNumParams();
    if (nargs != 2) {
        Scr_Error("USAGE: <player> viewkick <force 0-127> <source position>\n");
    }
    force = Scr_GetInt(0);
    player->client->damageTaken = (force * player->maxHealth + 0x32) / 100;
    if (player->client->damageTaken < 0) {
        Scr_Error(va("viewkick: damage %g < 0\n", (double)Scr_GetFloat(0)));
    }
    Scr_GetVec(1, source);
    Vec3_Sub_0811870c(player->client->ps.origin, source, player->client->damageDir);
}

// 0x08116574  size=130  FUN_08116574  status=translated  naming=unknown
// Pushes the world point of local offset param 0 transformed by the entity's angles and origin.
void ScriptEntCmd_localtoworldcoords(uint32 self)
{
    gentity_s *ent;
    float local[3];
    float axis[12];
    float world[3];

    ent = GScr_GetEntityFromRef(self);
    Scr_GetVec(0, local);
    AnglesToAxis(ent->r.currentAngles, axis);
    Vec3_RotateByTranspose(local, axis, world);
    Vec3_Add_081186c8(world, ent->r.currentOrigin, world);
    Scr_AddVec(world);
}

// 0x081165f6  size=116  FUN_081165f6  status=translated  naming=unknown
// Turret setter for field 0x10: stores -param 0 and clamps positive values to 0.
void ScriptEntCmd_setrightarc(uint32 self)
{
    gentity_s *ent;
    turret_s *turret;

    ent = GScr_GetEntityFromRef(self);
    turret = ent->pTurretInfo;
    if (turret == NULL) {
        Scr_Error("entity is not a turret");
    }
    turret->arcRight = -Scr_GetFloat(0);
    if (0.0f < turret->arcRight) {
        turret->arcRight = 0.0f;
    }
}

// 0x0811666a  size=103  FUN_0811666a  status=translated  naming=unknown
// Turret setter for field 0x18: stores param 0 and clamps negative values to 0.
void ScriptEntCmd_setleftarc(uint32 self)
{
    gentity_s *ent;
    turret_s *turret;

    ent = GScr_GetEntityFromRef(self);
    turret = ent->pTurretInfo;
    if (turret == NULL) {
        Scr_Error("entity is not a turret");
    }
    turret->arcLeft = Scr_GetFloat(0);
    if (turret->arcLeft < 0.0f) {
        turret->arcLeft = 0.0f;
    }
}

// 0x081166d2  size=116  FUN_081166d2  status=translated  naming=unknown
// Turret setter for field 0x0c: stores -param 0 and clamps positive values to 0.
void ScriptEntCmd_settoparc(uint32 self)
{
    gentity_s *ent;
    turret_s *turret;

    ent = GScr_GetEntityFromRef(self);
    turret = ent->pTurretInfo;
    if (turret == NULL) {
        Scr_Error("entity is not a turret");
    }
    turret->arcTop = -Scr_GetFloat(0);
    if (0.0f < turret->arcTop) {
        turret->arcTop = 0.0f;
    }
}

// 0x08116746  size=103  FUN_08116746  status=translated  naming=unknown
// Turret setter for field 0x14: stores param 0 and clamps negative values to 0.
void ScriptEntCmd_setbottomarc(uint32 self)
{
    gentity_s *ent;
    turret_s *turret;

    ent = GScr_GetEntityFromRef(self);
    turret = ent->pTurretInfo;
    if (turret == NULL) {
        Scr_Error("entity is not a turret");
    }
    turret->arcBottom = Scr_GetFloat(0);
    if (turret->arcBottom < 0.0f) {
        turret->arcBottom = 0.0f;
    }
}

// 0x081167ae  size=500  FUN_081167ae  status=translated  naming=unknown
// Places a spawn point: traces down 128 units up / 262144 units down (mins -15,-15,0; maxs 15,15,70),
// settles the origin at the hit fraction, records the ground entity and warns when the final
// zero-length trace starts solid. Spawn point origin is set through G_SetPositionTrajectory.
void ScriptEntCmd_placespawnpoint(uint32 self)
{
    gentity_s *ent;
    float start[3];
    float end[3];
    trace_t tr;

    ent = GScr_GetEntityFromRef(self);
    Vec3_CopyF(ent->r.currentOrigin, start);
    Vec3_CopyF(ent->r.currentOrigin, end);
    end[2] = end[2] + 128.0f;
    G_TraceBox(&tr, start, (float *)g_spawnTraceMins, (float *)g_spawnTraceMaxs, end, ent->s.number, 0x2810011);
    VectorLerp(start, end, tr.fraction, start);
    Vec3_CopyF(start, end);
    end[2] = end[2] - 262144.0f;
    G_TraceBox(&tr, start, (float *)g_spawnTraceMins, (float *)g_spawnTraceMaxs, end, ent->s.number, 0x2810011);
    ent->s.groundEntityNum = (int)tr.entityNum;
    VectorLerp(start, end, tr.fraction, start);
    G_TraceBox(&tr, start, (float *)g_spawnTraceMins, (float *)g_spawnTraceMaxs, start, ent->s.number, 0x2810011);
    if (tr.allsolid != 0) {
        Com_Printf("WARNING: Spawn point entity %i is in solid at (%i, %i, %i)\n", ent->s.number,
                   Float_RoundHalfUp(ent->r.currentOrigin[0]),
                   Float_RoundHalfUp(ent->r.currentOrigin[1]),
                   Float_RoundHalfUp(ent->r.currentOrigin[2]));
    }
    G_SetPositionTrajectory(GAME_ENT(ent), start);
}

// 0x081169a2  size=154  FUN_081169a2  status=translated  naming=unknown
// Sends the team score configstrings ("H" allies, "G" axis) to the client slot of the entity in param 0.
void ScriptEntCmd_updatescores(uint32 self)
{
    gentity_s *ent;
    int slot;

    ent = GScr_GetEntityFromRef(self);
    slot = (int)(ent - g_gentities);
    SV_SendServerCommandNum(slot, 0, va("%c %i", 'H', level_teamScoreAllies));
    SV_SendServerCommandNum(slot, 0, va("%c %i", 'G', level_teamScoreAxis));
}

// 0x08116a3c  size=119  FUN_08116a3c  status=translated  naming=observed
// map_restart builtin. Evidence: strings "map_restart already called" and "exitlevel already called"
// and the "fast_restart" command text referenced here.
void GScr_Map_Restart(void)
{
    const char *msg;

    if (level_mapCommandState != 0) {
        if (level_mapCommandState == 1) {
            msg = "map_restart already called";
        } else {
            msg = "exitlevel already called";
        }
        Scr_Error(msg);
    }
    level_mapCommandState = 1;
    g_mapChangeKeepStateArg = 0;
    if (Scr_GetNumParams() != 0) {
        g_mapChangeKeepStateArg = Scr_GetInt(0);
    }
    Cbuf_ExecuteText(2, "fast_restart\n");
}

// 0x08116ab4  size=181  FUN_08116ab4  status=translated  naming=observed
// map builtin. Evidence: strings "map already called" and "map %s\n" referenced here.
void GScr_Map(void)
{
    int nargs;
    const char *mapName;
    const char *msg;
    unsigned int nargsU;

    nargs = Scr_GetNumParams();
    if (nargs != 0) {
        mapName = Scr_GetStringParam(0);
        if (SV_MapFileExists(mapName) != 0) {
            if (level_mapCommandState != 0) {
                if (level_mapCommandState == 2) {
                    msg = "map already called";
                } else {
                    msg = "exitlevel already called";
                }
                Scr_Error(msg);
            }
            level_mapCommandState = 2;
            g_mapChangeKeepStateArg = 0;
            nargsU = (unsigned int)Scr_GetNumParams();
            if (1 < nargsU) {
                g_mapChangeKeepStateArg = Scr_GetInt(1);
            }
            Cbuf_ExecuteText(2, va("map %s\n", mapName));
        }
    }
}

// 0x08116b6a  size=109  FUN_08116b6a  status=translated  naming=observed
// exitlevel builtin. Evidence: strings "map_restart already called" and "exitlevel already called" referenced here.
void GScr_ExitLevel(void)
{
    const char *msg;

    if (level_mapCommandState != 0) {
        if (level_mapCommandState == 1) {
            msg = "map_restart already called";
        } else {
            msg = "exitlevel already called";
        }
        Scr_Error(msg);
    }
    level_mapCommandState = 3;
    g_mapChangeKeepStateArg = 0;
    if (Scr_GetNumParams() != 0) {
        g_mapChangeKeepStateArg = Scr_GetInt(0);
    }
    SV_ExitLevelStub();
    G_ExitLevel();
}

// 0x08116bd8  size=33  FUN_08116bd8  status=translated  naming=unknown
// Adds a bot and pushes its entity when one was created.
void GScr_addtestclient(void)
{
    gentity_s *bot;

    bot = SV_AddBot();
    if (bot != NULL) {
        Scr_AddEntityParam(bot);
    }
}

// 0x08116bfa  size=337  FUN_08116bfa  status=translated  naming=unknown
// Dvar setter: when the dvar exists, OR flag 0x100 into it; otherwise register it as a string
// dvar with flags 0x4100. The value is the text of param 1, or a joined message when param 1
// has type 3. A quote-sanitized copy is built, but the original registers the unsanitized text.
void GScr_makecvarserverinfo(void)
{
    const char *name;
    struct dvar_s *dvar;
    int paramType;
    const char *value;
    char message[1024];
    char sanitized[1024];
    char *out;
    unsigned int i;
    int nargs;

    name = Scr_ReleaseAnyValue(0);
    dvar = Dvar_Lookup(name);
    if (dvar == NULL) {
        paramType = (int)Scr_GetParamType(1);
        if (paramType == 3) {
            nargs = Scr_GetNumParams();
            GScr_BuildMessageString(1, nargs - 1, "Dvar Value", message, 0x400);
            value = message;
        } else {
            value = Scr_ReleaseAnyValue(1);
        }
        (void)strlen(value);
        /* The sanitized copy is built but the original registers the unsanitized value. */
        out = sanitized;
        for (i = 0; i < 0x3ff && value[i] != '\0'; i = i + 1) {
            *out = (char)Q_NormalizeApostrophe(value[i]);
            if (*out == '"') {
                *out = '\'';
            }
            out = out + 1;
        }
        *out = '\0';
        Dvar_RegisterString(name, value, 0x4100);
    } else {
        Dvar_OrFlags(dvar, 0x100);
    }
}

// 0x08116d4c  size=85  FUN_08116d4c  status=translated  naming=unknown
// Sends the text of param 0 to every client as a 'e' server command (when any param is given).
void GScr_allclientsprint(void)
{
    const char *text;

    if (Scr_GetNumParams() != 0) {
        text = Scr_ReleaseAnyValue(0);
        SV_SendServerCommandNum(-1, 0, va("%c \"%s\"", 'e', text));
    }
}

// 0x08116da2  size=75  FUN_08116da2  status=translated  naming=unknown
// Pushes 1 when the map file named by param 0 exists, else 0 (nothing pushed without params).
void GScr_mapexists(void)
{
    const char *name;

    if (Scr_GetNumParams() != 0) {
        name = Scr_ReleaseAnyValue(0);
        if (SV_MapFileExists(name) == 0) {
            Scr_AddIndex(0);
        } else {
            Scr_AddIndex(1);
        }
    }
}

// 0x08116dee  size=75  FUN_08116dee  status=translated  naming=unknown
// Pushes 1 when the name in param 0 is present in the named-entry table, else 0.
void GScr_isvalidgametype(void)
{
    const char *name;

    if (Scr_GetNumParams() != 0) {
        name = Scr_ReleaseAnyValue(0);
        if (G_IsGametypeListed(name) == 0) {
            Scr_AddIndex(0);
        } else {
            Scr_AddIndex(1);
        }
    }
}

// 0x08116e3a  size=167  FUN_08116e3a  status=translated  naming=unknown
// Sets configstring 0x10 to param 0 and configstrings 0x0f, 0x11, 0x12 to the integer values of
// the three stored server config values.
void GScr_setvotestring(void)
{
    const char *text;

    if (Scr_GetNumParams() != 0) {
        text = Scr_ReleaseAnyValue(0);
        SV_SetConfigstring(0x10, (char *)text);
        SV_SetConfigstring(0xf, (char *)va("%i", level_voteTime));
        SV_SetConfigstring(0x11, (char *)va("%i", level_voteYes));
        SV_SetConfigstring(0x12, (char *)va("%i", level_voteNo));
    }
}

// 0x08116ee2  size=143  FUN_08116ee2  status=translated  naming=unknown
// Sets configstring 0x0f from param 0 (integer) and refreshes configstrings 0x11 and 0x12.
void GScr_setvotetime(void)
{
    int value;

    if (Scr_GetNumParams() != 0) {
        value = Scr_GetInt(0);
        SV_SetConfigstring(0xf, (char *)va("%i", value));
        SV_SetConfigstring(0x11, (char *)va("%i", level_voteYes));
        SV_SetConfigstring(0x12, (char *)va("%i", level_voteNo));
    }
}

// 0x08116f72  size=106  FUN_08116f72  status=translated  naming=unknown
// Sets configstring 0x11 from param 0 (integer) and refreshes configstring 0x12.
void GScr_setvoteyescount(void)
{
    int value;

    if (Scr_GetNumParams() != 0) {
        value = Scr_GetInt(0);
        SV_SetConfigstring(0x11, (char *)va("%i", value));
        SV_SetConfigstring(0x12, (char *)va("%i", level_voteNo));
    }
}

// 0x08116fdc  size=69  FUN_08116fdc  status=translated  naming=unknown
// Sets configstring 0x12 from param 0 (integer).
void GScr_setvotenocount(void)
{
    int value;

    if (Scr_GetNumParams() != 0) {
        value = Scr_GetInt(0);
        SV_SetConfigstring(0x12, (char *)va("%i", value));
    }
}

// 0x08117022  size=69  FUN_08117022  status=translated  naming=observed
// tempBanClient builtin. Evidence: format string "tempBanClient %i\n" referenced here; the text is
// queued with Cbuf_ExecuteText (append).
void GScr_TempBanClient(void)
{
    int clientNum;

    if (Scr_GetNumParams() != 0) {
        clientNum = Scr_GetInt(0);
        Cbuf_ExecuteText(2, va("tempBanClient %i\n", clientNum));
    }
}

// 0x08117068  size=69  FUN_08117068  status=translated  naming=observed
// banClient builtin. Evidence: format string "banClient %i\n" referenced here.
void GScr_BanClient(void)
{
    int clientNum;

    if (Scr_GetNumParams() != 0) {
        clientNum = Scr_GetInt(0);
        Cbuf_ExecuteText(2, va("banClient %i\n", clientNum));
    }
}

// 0x081170ae  size=113  FUN_081170ae  status=translated  naming=unknown
// Sends param 1 as an 'e' server command to the client slot of the entity in param 0.
void GScr_clientprint(void)
{
    gentity_s *ent;
    const char *text;

    if (Scr_GetNumParams() != 0) {
        ent = ScrEntityParam(0);
        text = Scr_ReleaseAnyValue(1);
        SV_SendServerCommandNum((int)(ent - g_gentities), 0, va("%c \"%s\"", 'e', text));
    }
}

// 0x08117120  size=659  FUN_08117120  status=translated  naming=observed
// openfile builtin: opens scriptdata/<name> in read/write/append mode into the file slot table.
// Evidence: "OpenFile failed.  %i files already open" and "Valid openfile modes are ..." referenced here.
// Only slot 0 is searched (the original loop bound is 1).
void GScr_OpenFile(void)
{
    uint32 nargs;
    int slot;
    int *freeSlot;
    const char *name;
    const char *mode;
    const char *path;
    int handle;
    int len;

    freeSlot = NULL;
    slot = 0;
    nargs = (uint32)Scr_GetNumParams();
    if (1 < nargs) {
        name = Scr_ReleaseAnyValue(0);
        mode = Scr_ReleaseAnyValue(1);
        for (slot = 0; slot < 1; slot = slot + 1) {
            if (g_scrFileHandles[slot] < 0) {
                freeSlot = &g_scrFileHandles[slot];
                break;
            }
        }
        if (freeSlot == NULL) {
            Com_Printf("OpenFile failed.  %i files already open\n", 1);
            Scr_AddIndex(-1);
        } else if (strcmp(mode, "read") == 0) {
            path = va("%s/%s", "scriptdata", name);
            len = FSH_FOpenFile(path, &handle, 0);
            if (len < 0) {
                Scr_AddIndex(-1);
            } else {
                G_SCRIPT_FILE_BUF(slot) = (char *)Com_MallocZeroed((unsigned int)(len + 1));
                FS_Read(G_SCRIPT_FILE_BUF(slot), len, handle);
                FS_FCloseFile(handle);
                G_SCRIPT_FILE_BUF(slot)[len] = '\0';
                Com_BeginParseSession(name);
                Com_SetCSVMode(1);
                g_scrParseSaveRecords[slot * 5] = 0;
                Scr_AddIndex(slot);
            }
        } else if (strcmp(mode, "write") == 0) {
            path = va("%s/%s", "scriptdata", name);
            len = FSH_FOpenFile(path, freeSlot, 1);
            if (len < 0) {
                Scr_AddIndex(-1);
            } else {
                Scr_AddIndex(slot);
            }
        } else if (strcmp(mode, "append") == 0) {
            path = va("%s/%s", "scriptdata", name);
            len = FSH_FOpenFile(path, freeSlot, 2);
            if (len < 0) {
                Scr_AddIndex(-1);
            } else {
                Scr_AddIndex(slot);
            }
        } else {
            Com_Printf("Valid openfile modes are 'write', 'read', and 'append'\n");
            Scr_AddIndex(-1);
        }
    }
}

// 0x081173b4  size=231  FUN_081173b4  status=translated  naming=observed
// closefile builtin. Evidence: "CloseFile failed, invalid file number %i" and
// "CloseFile failed, file number %i was not open" referenced here.
void GScr_CloseFile(void)
{
    int fileNum;

    if (Scr_GetNumParams() != 0) {
        fileNum = Scr_GetInt(0);
        if (fileNum < 0 || 1 < fileNum) {
            Com_Printf("CloseFile failed, invalid file number %i\n", fileNum);
            Scr_AddIndex(-1);
        } else {
            if (g_scrFileHandles[fileNum] < 1) {
                if (G_SCRIPT_FILE_BUF(fileNum) == NULL) {
                    Com_Printf("CloseFile failed, file number %i was not open\n", fileNum);
                    Scr_AddIndex(-1);
                    return;
                }
                Com_EndParseSession();
                Com_Free_A(G_SCRIPT_FILE_BUF(fileNum));
                G_SCRIPT_FILE_BUF(fileNum) = NULL;
            } else {
                FS_FCloseFile(g_scrFileHandles[fileNum]);
                g_scrFileHandles[fileNum] = -1;
            }
            Scr_AddIndex(1);
        }
    }
}

// 0x08117918  size=20  FUN_08117918  status=translated  naming=unknown
// Stub builtin: pushes 0.
void GScr_issplitscreen(void)
{
    Scr_AddIndex(0);
}

// 0x081178ea  size=28  FUN_081178ea  status=translated  naming=unknown
// Passes param 0 (integer) to SV_AllocServerPools.
void GScr_setarchive(void)
{
    int enable;

    enable = Scr_GetInt(0);
    SV_AllocServerPools(enable);
}

// 0x0811749c  size=291  FUN_0811749c  status=translated  naming=observed
// fprintln builtin. Evidence: "fprintln requires at least 2 parameters (file, output)" and
// "FPrintln failed, invalid file number %i" referenced here. Writes each further param followed by a ','
// to the file slot, then pushes the number of written params.
void GScr_FPrintln(void)
{
    uint32 nargs;
    int file;
    uint32 i;
    const char *text;

    nargs = (uint32)Scr_GetNumParams();
    if (nargs < 2) {
        Com_Printf("fprintln requires at least 2 parameters (file, output)\n");
        Scr_AddIndex(-1);
    } else {
        file = Scr_GetInt(0);
        if (file < 0 || 1 < file) {
            Com_Printf("FPrintln failed, invalid file number %i\n", file);
            Scr_AddIndex(-1);
        } else if (g_scrFileHandles[file] < 0) {
            Com_Printf("FPrintln failed, file number %i was not open for writing\n", file);
            Scr_AddIndex(-1);
        } else {
            i = 1;
            while (true) {
                if ((uint32)Scr_GetNumParams() <= i) {
                    break;
                }
                text = Scr_ReleaseAnyValue(i);
                FS_Write(text, (int)strlen(text), g_scrFileHandles[file]);
                FS_Write(",", 1, g_scrFileHandles[file]);
                i = i + 1;
            }
            Scr_AddIndex(Scr_GetNumParams() - 1);
        }
    }
}

// 0x081175c0  size=439  FUN_081175c0  status=translated  naming=observed
// freadln builtin. Evidence: "freadln requires a parameter - the file to read from" and
// "freadln failed, invalid file number %i" referenced here. Reads the next line token count
// (state 0) or the remaining token count after the first token (state 1) from the file slot.
void GScr_FReadln(void)
{
    int file;
    const char *data;
    struct ParseSave *save;
    const char *tok;
    int atEnd;

    if (Scr_GetNumParams() == 0) {
        Com_Printf("freadln requires a parameter - the file to read from\n");
        Scr_AddIndex(-1);
    } else {
        file = Scr_GetInt(0);
        if (file < 0 || 1 < file) {
            Com_Printf("freadln failed, invalid file number %i\n", file);
            Scr_AddIndex(-1);
        } else if (G_SCRIPT_FILE_BUF(file) == NULL) {
            Com_Printf("freadln failed, file number %i was not open for reading\n", file);
            Scr_AddIndex(-1);
        } else {
            data = G_SCRIPT_FILE_BUF(file);
            save = (struct ParseSave *)&g_scrParseSaveRecords[file * 5];
            if (g_scrParseSaveRecords[file * 5] == 0) {
                Com_SaveParseOffset(&data, save);
                Scr_AddIndex(Com_CountTokensOnLine(&data));
            } else {
                Com_RestoreParseOffset(&data, save);
                Com_SkipRestOfLine(&data);
                Com_SaveParseOffset(&data, save);
                tok = Com_Parse(&data);
                atEnd = (*tok == '\0');
                Com_RestoreParseOffset(&data, save);
                if (atEnd == 0) {
                    Scr_AddIndex(Com_CountTokensOnLine(&data));
                } else {
                    Scr_AddIndex(-1);
                }
            }
        }
    }
}

// 0x08117778  size=369  FUN_08117778  status=translated  naming=observed
// freadline builtin. Evidence: "freadline requires at least 2 parameters (file, string)" and
// "freadline failed, invalid file number %i" referenced here. Pushes the argument token
// (param 1 index on the line) or an empty string on any error.
void GScr_FReadline(void)
{
    uint32 nargs;
    int file;
    int argNum;
    const char *data;
    struct ParseSave *save;
    const char *tok;
    int i;

    nargs = (uint32)Scr_GetNumParams();
    if (nargs < 2) {
        Com_Printf("freadline requires at least 2 parameters (file, string)\n");
        Scr_AddString("");
    } else {
        file = Scr_GetInt(0);
        argNum = Scr_GetInt(1);
        if (file < 0 || 1 < file) {
            Com_Printf("freadline failed, invalid file number %i\n", file);
            Scr_AddString("");
        } else if (argNum < 0) {
            Com_Printf("freadline failed, invalid argument number %i\n", argNum);
            Scr_AddString("");
        } else if (G_SCRIPT_FILE_BUF(file) == NULL) {
            Com_Printf("freadline failed, file number %i was not open for reading\n", file);
            Scr_AddString("");
        } else {
            tok = "";
            data = G_SCRIPT_FILE_BUF(file);
            save = (struct ParseSave *)&g_scrParseSaveRecords[file * 5];
            Com_RestoreParseOffset(&data, save);
            for (i = 0; i <= argNum; i = i + 1) {
                tok = Com_ParseOnLine(&data);
                if (*tok == '\0') {
                    Com_Printf("freadline failed, there aren't %i arguments on this line, there are only %i arguments\n",
                               argNum + 1, i);
                    Scr_AddString("");
                    return;
                }
            }
            Scr_AddString(tok);
        }
    }
}

// 0x08117944  size=302  FUN_08117944  status=translated  naming=observed
// setteamfortrigger builtin. Evidence: "setteamfortrigger: trigger entity must be of type %s or %s" and
// "setteamfortrigger: invalid team used must be %s, %s or %s" referenced here. Stores the trigger team
// code (allies 2, axis 1, none 0) at gentity +0x167.
void GScr_SetTeamForTrigger(uint32 self)
{
    gentity_s *trig;
    short team;

    trig = GScr_GetEntityFromRef(self);
    if ((short)trig->classname != scr_str_trigger_use && (short)trig->classname != scr_str_trigger_use_touch) {
        Scr_Error(va("setteamfortrigger: trigger entity must be of type %s or %s",
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use),
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use_touch)));
    }
    team = (short)Scr_GetAny(0);
    if (team == scr_str_allies) {
        trig->team = 2;
    } else if (team == scr_str_axis) {
        trig->team = 1;
    } else if (team == scr_str_none) {
        trig->team = 0;
    } else {
        Scr_Error(va("setteamfortrigger: invalid team used must be %s, %s or %s",
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_allies),
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_axis),
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_none)));
    }
}

// 0x08117a72  size=231  FUN_08117a72  status=translated  naming=observed
// clientclaimtrigger builtin. Evidence: "clientclaimtrigger: claimer must be a client." and
// "clientclaimtrigger: trigger entity must be of type %s or %s" referenced here.
void GScr_ClientClaimTrigger(uint32 self)
{
    gentity_s *claimer;
    gentity_s *trig;

    claimer = GScr_GetEntityFromRef(self);
    if (claimer->client == NULL) {
        Scr_Error("clientclaimtrigger: claimer must be a client.");
    }
    trig = ScrEntityParam(0);
    if ((short)trig->classname != scr_str_trigger_use && (short)trig->classname != scr_str_trigger_use_touch) {
        Scr_Error(va("clientclaimtrigger: trigger entity must be of type %s or %s",
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use),
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use_touch)));
    }
    if (TRIG_CLAIM(trig) == 0x3ff || TRIG_CLAIM(trig) == claimer->client->ps.clientNum) {
        TRIG_CLAIM(trig) = claimer->client->ps.clientNum;
    }
}

// 0x08117b5a  size=205  FUN_08117b5a  status=translated  naming=observed
// clientreleasetrigger builtin. Evidence: "clientreleasetrigger: releaser must be a client." and
// "clientreleasetrigger: trigger entity must be of type %s or %s" referenced here.
void GScr_ClientReleaseTrigger(uint32 self)
{
    gentity_s *releaser;
    gentity_s *trig;

    releaser = GScr_GetEntityFromRef(self);
    if (releaser->client == NULL) {
        Scr_Error("clientreleasetrigger: releaser must be a client.");
    }
    trig = ScrEntityParam(0);
    if ((short)trig->classname != scr_str_trigger_use && (short)trig->classname != scr_str_trigger_use_touch) {
        Scr_Error(va("clientreleasetrigger: trigger entity must be of type %s or %s",
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use),
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use_touch)));
    }
    if (TRIG_CLAIM(trig) == releaser->client->ps.clientNum) {
        TRIG_CLAIM(trig) = 0x3ff;
    }
}

// 0x08117c28  size=138  FUN_08117c28  status=translated  naming=observed
// releaseclaimedtrigger builtin. Evidence: "releaseclaimedtrigger: trigger entity must be of type %s or %s"
// referenced here. Clears the claim slot of the entity itself.
void GScr_ReleaseClaimedTrigger(uint32 self)
{
    gentity_s *trig;

    trig = GScr_GetEntityFromRef(self);
    if ((short)trig->classname != scr_str_trigger_use && (short)trig->classname != scr_str_trigger_use_touch) {
        Scr_Error(va("releaseclaimedtrigger: trigger entity must be of type %s or %s",
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use),
                     ScrStr_TextFromHandle((uint32)(int32)scr_str_trigger_use_touch)));
    }
    TRIG_CLAIM(trig) = 0x3ff;
}

// 0x08117e62  size=68  FUN_08117e62  status=translated  naming=unknown
// Sets the entity origin from param 0 along a position trajectory and relinks it when unk_f0 is set.
void GScr_SetEntityOriginFromVector(gentity_s *ent)
{
    float origin[3];

    Scr_GetVec(0, origin);
    G_SetPositionTrajectory(GAME_ENT(ent), origin);
    if (ent->r.linked != 0) {
        SV_LinkEntity((gentity_s *)ent);
    }
}

// 0x08117ea6  size=45  FUN_08117ea6  status=translated  naming=unknown
// Sets the entity angle trajectory from param 0 (vector).
void GScr_SetEntityAnglesFromVector(gentity_s *ent)
{
    float angles[3];

    Scr_GetVec(0, angles);
    G_SetAnglesTrajectory(GAME_ENT(ent), angles);
}

// 0x08117ed4  size=91  FUN_08117ed4  status=translated  naming=unknown
// Sets the entity health from param 0 (integer). Entities with a client also copy the value to
// the client record at +0x12c; entities without one also set +0x198.
void GScr_SetEntityHealth(gentity_s *ent)
{
    int value;

    value = Scr_GetInt(0);
    if (ent->client == NULL) {
        ent->maxHealth = value;
        ent->health = value;
    } else {
        ent->health = value;
        ent->client->ps.stats[0] = value;
    }
}

// 0x08119828  size=94  FUN_08119828  status=translated  naming=unknown
// Finishes a script mover spawn: common mover setup, clears contents, links the entity, then sets
// the flag at +0xf2 (no unk_80) or the eFlags bit 0x20 (unk_80 set).
void SP_script_mover_linked(gentity_s *ent)
{
    SP_script_mover_common((gentity_s *)ent);
    ent->r.contents = 0;
    SV_LinkEntity((gentity_s *)ent);
    if (ent->s.constantLight == 0) {
        ent->r.svFlags = (byte)(ent->r.svFlags | 1);
    } else {
        ent->s.eFlags = ent->s.eFlags | 0x20;
    }
}

// Helper for the script mover builtins (targets 0x0811998a and the mover setters): resolves the entity from a self ref and
// checks its classname against the script_brushmodel, script_model and script_origin handles.
// Non-entity refs raise "not an entity" and pass NULL on.
static gentity_s *ScriptMoverTarget(uint32 self)
{
    gentity_s *ent;

    if ((self >> 16) == 0) {
        ent = &g_gentities[self & 0xffff];
        if ((short)ent->classname != scrStr_script_brushmodel
            && (short)ent->classname != scr_str_script_model
            && (short)ent->classname != scr_str_script_origin) {
            Scr_ErrorNoParam(va("entity %i is not a script_brushmodel, script_model, or script_origin",
                                (int)(short)self));
        }
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = NULL;
    }
    return ent;
}

// 0x0811998a  size=226  FUN_0811998a  status=translated  naming=unknown
// Script mover move builtin (mover start-move call). Name not recovered from strings; the error strings
// "entity %i is not a script_brushmodel, script_model, or script_origin" and "not an entity" are shared by the mover builtins.
void ScriptMover_moveto(uint32 self)
{
    gentity_s *ent;
    float dest[3];
    float totalTime;
    float accelTime;
    float decelTime;

    ent = ScriptMoverTarget(self);
    Scr_GetVec(0, dest);
    Scr_GetMoverTimeParams(&totalTime, &accelTime, &decelTime);
    G_ScriptMover_StartMove((gentity_s *)ent, dest, totalTime, accelTime, decelTime);
}

// 0x08119a6c  size=202  FUN_08119a6c  status=translated  naming=unknown
// Script mover gravity builtin (same target checks as ScriptMover_moveto); calls G_ScriptMover_StartGravity.
void ScriptMover_movegravity(uint32 self)
{
    gentity_s *ent;
    float velocity[3];
    float seconds;

    ent = ScriptMoverTarget(self);
    Scr_GetVec(0, velocity);
    seconds = Scr_GetFloat(1);
    G_ScriptMover_StartGravity((gentity_s *)ent, velocity, seconds);
}

// 0x08119c58  size=27  FUN_08119c58  status=translated  naming=unknown
// Axis-1 wrapper over GScr_MoveAxis.
void ScriptMover_movey(uint32 self)
{
    GScr_MoveAxis(self, 1);
}

// 0x08119c74  size=27  FUN_08119c74  status=translated  naming=unknown
// Axis-2 wrapper over GScr_MoveAxis.
void ScriptMover_movez(uint32 self)
{
    GScr_MoveAxis(self, 2);
}

// 0x08119c90  size=311  FUN_08119c90  status=translated  naming=unknown
// Script mover rotate-to builtin (same target checks as ScriptMover_moveto). The destination
// angles are current + AngleSubtract(dest, current) per axis.
void ScriptMover_rotateto(uint32 self)
{
    gentity_s *ent;
    float dest[3];
    float target[4];
    float totalTime;
    float accelTime;
    float decelTime;
    int i;

    ent = ScriptMoverTarget(self);
    Scr_GetVec(0, dest);
    Scr_GetMoverTimeParams(&totalTime, &accelTime, &decelTime);
    for (i = 0; i < 3; i = i + 1) {
        target[i] = ent->r.currentAngles[i] + AngleSubtract(dest[i], ent->r.currentAngles[i]);
    }
    G_ScriptMover_StartRotateTo((gentity_s *)ent, target, totalTime, accelTime, decelTime);
}

// 0x08119eea  size=27  FUN_08119eea  status=translated  naming=unknown
// Axis-1 wrapper over GScr_RotateAxis.
void ScriptMover_rotateyaw(uint32 self)
{
    GScr_RotateAxis(self, 1);
}

// 0x08119f06  size=27  FUN_08119f06  status=translated  naming=unknown
// Axis-2 wrapper over GScr_RotateAxis.
void ScriptMover_rotateroll(uint32 self)
{
    GScr_RotateAxis(self, 2);
}

// 0x08119f22  size=226  FUN_08119f22  status=translated  naming=unknown
// Script mover rotate-velocity builtin (same target checks as ScriptMover_moveto); calls G_ScriptMover_StartRotateVelocity.
void ScriptMover_rotatevelocity(uint32 self)
{
    gentity_s *ent;
    float angVel[3];
    float totalTime;
    float accelTime;
    float decelTime;

    ent = ScriptMoverTarget(self);
    Scr_GetVec(0, angVel);
    Scr_GetMoverTimeParams(&totalTime, &accelTime, &decelTime);
    G_ScriptMover_StartRotateVelocity((gentity_s *)ent, angVel, totalTime, accelTime, decelTime);
}
