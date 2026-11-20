#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_104_types.h"

/*
 * Chunk 104: 89 functions recovered from .eh_frame FDE starts (Ghidra missed them) between
 * 0x080fda40 and 0x0811168e. They are GSC method/global handlers (PlayerCmd_*, GScr_*, HudElem_*),
 * spawn/classname callbacks (SP_*), a turret/item placement group, and a few debug helpers.
 * Naming evidence per function is on its provenance line. Callees whose merged symbol is absent or
 * whose merged signature disagrees with the observed call sites use fn_08xxxxxx placeholders.
 * Float literals were read from .rodata; the trace struct and hud table layouts are in
 * chunk_104_types.h.
 */

/* Helpers that are local to this chunk. */

static inline uint32 Chunk104_F2U(float f)
{
    union {
        float f;
        uint32 u;
    } bits;
    bits.f = f;
    return bits.u;
}

static inline float Chunk104_U2F(uint32 u)
{
    union {
        float f;
        uint32 u;
    } bits;
    bits.u = u;
    return bits.f;
}

static inline uint32 Chunk104_P32(const void *p)
{
    return (uint32)(size_t)p;
}

/* g_gentities[ref & 0xffff] with the original stride, no bounds check (the caller's range rule applies). */
static struct gentity_s *Chunk104_EntByRef(uint32 ref)
{
    return (struct gentity_s *)((uint8 *)g_gentities + (size_t)(ref & 0xffff) * 0x230u);
}

/* Shared player-ref validation: a high-word-nonzero ref is "not an entity" (ent becomes NULL);
 * a valid entity without a client fails with "entity %i is not a player". */
static struct gentity_s *Chunk104_CheckPlayerRef(uint32 entref)
{
    struct gentity_s *ent = NULL;

    if ((entref >> 16) == 0) {
        ent = Chunk104_EntByRef(entref);
        if (ent->client == NULL) {
            Scr_ErrorNoParam(va("entity %i is not a player", (int)(uint16)entref));
        }
    } else {
        Scr_ErrorNoParam("not an entity");
        ent = NULL;
    }
    return ent;
}

/* Forward declarations of chunk-local helpers used before their definitions. */
static void PlayerCmd_chatCommon104(uint32 entref, int mode);
static void HudElem_SetEnumFieldAt104(struct game_hudelem_s *he, int field, const char *const *names, int count);

/* ------------------------------------------------------------------------------------------------ */

// 0x080fda40  size=208  FUN_080fda40  status=translated  naming=inferred
// naming: method table entry "sayall" -> 0x080fda40 (script string at .rodata); body sends the chat
//         line through the shared chat sender with mode 0. Function does not reference "sayall" itself.
void PlayerCmd_sayall(uint32 entref)
{
    PlayerCmd_chatCommon104(entref, 0);
}

// 0x080fdb10  size=208  FUN_080fdb10  status=translated  naming=inferred
// naming: method table entry "sayteam" -> 0x080fdb10; identical body to sayall with mode 1.
void PlayerCmd_sayteam(uint32 entref)
{
    PlayerCmd_chatCommon104(entref, 1);
}

static void PlayerCmd_chatCommon104(uint32 entref, int mode)
{
    struct gentity_s *ent;
    char msg[0x400];

    ent = Chunk104_CheckPlayerRef(entref);
    GScr_BuildMessageString(0, Scr_GetNumParams() - 1, "Client Chat Message", msg + 1, 0x3ff);
    msg[0] = 0x14;
    G_Say(ent, NULL, mode, msg);
}

// 0x080fdbe0  size=320  FUN_080fdbe0  status=translated  naming=inferred
// naming: method table entry "allowspectateteam" -> 0x080fdbe0; team names in the error string
//         "team must be axis, allies, none, freelook" are the four handles compared below.
void PlayerCmd_allowspectateteam(uint32 entref)
{
    struct gentity_s *ent;
    uint16 team;
    uint32 mask;

    ent = Chunk104_CheckPlayerRef(entref);
    team = (uint16)Scr_GetAny(0);
    mask = 0;
    if (team == scr_str_axis) {
        mask = 2;   /* axis */
    } else if (team == scr_str_allies) {
        mask = 4;   /* allies */
    } else if (team == scr_str_none) {
        mask = 1;   /* none */
    } else if (team == scr_str_freelook) {
        mask = 0x10; /* freelook */
    } else {
        Scr_ErrorAtParam(0, "team must be \"axis\", \"allies\", \"none\", or \"freelook\"");
    }
    if (Scr_GetInt(1) == 0) {
        ent->client->noSpectate |= mask;
    } else {
        ent->client->noSpectate &= ~mask;
    }
}

// 0x080fdd20  size=139  FUN_080fdd20  status=translated  naming=inferred
// naming: method table entry "getguid" -> 0x080fdd20; string "USAGE: self getGuid()\n" referenced here.
void PlayerCmd_getguid(uint32 entref)
{
    (void)Chunk104_CheckPlayerRef(entref);
    if (Scr_GetNumParams() != 0) {
        Scr_Error("USAGE: self getGuid()\n");
    }
    Scr_AddIndex(SV_GetClientGuid((int)(uint16)entref));
}

// 0x080fe56c  size=28  FUN_080fe56c  status=translated  naming=unknown
// naming: no string or table entry references this function; name is descriptive of the body
//         (concatenates the command arguments from index 1 and parses them as configstring-12 bounds).
void Cmd_ParseConfigstring12Bounds_f(void)
{
    char *text;

    text = ConcatArgs(1);
    G_ParseConfigstring12Bounds(text);
}

// 0x08102a76  size=504  FUN_08102a76  status=translated  naming=unknown
// naming: descriptive. Draws the 12 edges of an oriented box: 8 corners from mins/maxs, rotated about
//         Z by the given angle (degrees; computed as double pi/180 times the float angle), then the
//         edge index pairs at .rodata 0x08159840.
void Debug_DrawOrientedBox(uint32 p1, const float *mins, const float *maxs, float yawDeg,
                           uint32 p5, uint32 p6, uint32 p7)
{
    static const int edgeA[12] = {0, 0, 0, 1, 1, 2, 2, 3, 4, 4, 5, 6};
    static const int edgeB[12] = {1, 2, 4, 3, 5, 3, 6, 7, 5, 6, 7, 7};
    float sinYaw;
    float cosYaw;
    float corners[8][3];
    float xy[2];
    int i;
    int k;

    SinCos_08103092((float)(0.017453292519943295 * (double)yawDeg), &sinYaw, &cosYaw);
    for (i = 0; i < 8; i++) {
        for (k = 0; k < 3; k++) {
            if (((i >> k) & 1) == 0) {
                corners[i][k] = mins[k];
            } else {
                corners[i][k] = maxs[k];
            }
        }
        xy[0] = corners[i][0] * cosYaw - corners[i][1] * sinYaw;
        xy[1] = corners[i][1] * cosYaw + corners[i][0] * sinYaw;
        Vec2Copy_081030c0(xy, corners[i]);   /* copies xy[0..1] into corners[i][0..1]; z kept */
        VectorAdd_08103134(corners[i], (const float *)(size_t)p1, corners[i]);
    }
    for (i = 0; i < 12; i++) {
        G_DebugLine_08102a70(corners[edgeA[i]], corners[edgeB[i]], p5, p6, p7);
    }
}

// 0x08102c6e  size=165  FUN_08102c6e  status=translated  naming=unknown
// naming: descriptive. Builds a 6-float frame from the level's reference point (level+0x14, z offset
//         from level+0xf8) when the flag is zero, otherwise a unit Z frame, then forwards to 0x08102d14.
void Aim_SetupFrame(uint32 p1, uint32 p2, uint32 p3, uint32 p4, int p5, uint32 p6)
{
    float pos[3];
    float frame[6];

    if (p5 == 0) {
        VectorCopy_08103104((const float *)((uint8 *)&level_clients + 0x14), pos);
        pos[2] = pos[2] + *(float *)((uint8 *)&level_clients + 0xf8);
        VectorSubtract_08103178((const float *)(size_t)p1, pos, frame);
    } else {
        VectorSet_081030e0(frame, 0.0f, 0.0f, 1.0f);
    }
    G_DebugCircle_08102d14((const float *)(size_t)p1, Chunk104_U2F(p2), frame, p3, p4, p6);
}

// 0x08103660  size=74  FUN_08103660  status=translated  naming=unknown
// naming: descriptive. Hud field setter: stores the handle of the named material (script string via
//         0x0811d3fe) at the record's byte offset inside the hud element.
void HudElem_SetMaterialField(struct game_hudelem_s *he, int field)
{
    uint32 handle;

    handle = G_LocalizedStringIndex(Scr_ReleaseLocalizedString(0));
    *(uint32 *)((char *)he + HUDFIELD104(field).offset) = handle;
}

// 0x081036aa  size=66  FUN_081036aa  status=translated  naming=unknown
// naming: descriptive. Hud field setter: stores Scr_GetInt(0) at the record's byte offset.
void HudElem_SetIntField(struct game_hudelem_s *he, int field)
{
    int value;

    value = Scr_GetInt(0);
    *(int *)((char *)he + HUDFIELD104(field).offset) = value;
}

// 0x081036ec  size=209  FUN_081036ec  status=translated  naming=unknown
// naming: descriptive. Reads an RGB vector, clamps each channel to [0,1], stores bytes at color[0..2].
void HudElem_SetColorFromVector(struct game_hudelem_s *he)
{
    float v[3];
    float c;
    int i;

    Scr_GetVector(0, (float *)(v));
    for (i = 0; i < 3; i++) {
        c = (float)Float_Min_08104a10(1.0f, v[i]);
        c = (float)Float_Max_081049ec(0.0f, c);
        ((uint8 *)&he->elem.color)[i] = (uint8)Float_RoundToInt_08104a34(255.0f * c);
    }
}

// 0x081037be  size=97  FUN_081037be  status=translated  naming=unknown
// naming: descriptive. Pushes the RGB bytes as a vector in [0,1] (byte / 255).
void HudElem_GetColorVector(struct game_hudelem_s *he)
{
    float v[3];
    int i;

    for (i = 0; i < 3; i++) {
        v[i] = 0.003921568859368563f * (float)((uint8 *)&he->elem.color)[i];
    }
    Scr_AddVector((const float *)(v));
}

// 0x08103820  size=87  FUN_08103820  status=translated  naming=unknown
// naming: descriptive. Alpha setter: clamps Scr_GetFloat(0) to [0,1] and stores it as a byte at color[3].
void HudElem_SetAlpha(struct game_hudelem_s *he)
{
    float v;

    v = (float)Scr_GetFloat(0);
    v = (float)Float_Min_08104a10(1.0f, v);
    v = (float)Float_Max_081049ec(0.0f, v);
    ((uint8 *)&he->elem.color)[3] = (uint8)Float_RoundToInt_08104a34(255.0f * v);
}

// 0x08103878  size=39  FUN_08103878  status=translated  naming=unknown
// naming: descriptive. Alpha getter: pushes color[3] / 255 as a float.
void HudElem_GetAlpha(struct game_hudelem_s *he)
{
    Scr_AddFloatBits(Chunk104_F2U(0.003921568859368563f * (float)((uint8 *)&he->elem.color)[3]));
}

// 0x081038a0  size=73  FUN_081038a0  status=translated  naming=inferred
// naming: table record "fontscale" (offset 0x10, setter 0x081038a0). Message "font scale was %g; should be > 0".
void HudElem_SetFontScale(struct game_hudelem_s *he)
{
    float v;

    v = (float)Scr_GetFloat(0);
    if (v <= 0.0f) {
        Scr_Error(va("font scale was %g; should be > 0", (double)v));
    }
    he->elem.fontScale = v;
}

// 0x081038ea  size=57  FUN_081038ea  status=translated  naming=inferred
// naming: table record "font" (setter pair 0x081038ea/0x08103924); enum names at .rodata 0x08187c80, count 3.
void HudElem_SetFontEnum(struct game_hudelem_s *he, int field)
{
    HudElem_SetEnumFieldAt104(he, field, g_hudFontEnumNames, 3);
}

// 0x08103924  size=57  FUN_08103924  status=translated  naming=inferred
// naming: alternate setter of the "font" record; same names and count, via the second dispatcher.
void HudElem_SetFontEnumAlt(struct game_hudelem_s *he, int field)
{
    HudElem_AddPackedFieldString((char *)he, (struct hudelem_field_s *)&HUDFIELD104(field), (char **)g_hudFontEnumNames);   /* original caller also pushes the count 3 (ignored by the callee) */
}

// 0x0810395e  size=57  FUN_0810395e  status=translated  naming=inferred
// naming: table record "alignx" (setter 0x0810395e); enum names at .rodata 0x08187c8c, count 3.
void HudElem_SetAlignXEnum(struct game_hudelem_s *he, int field)
{
    HudElem_SetEnumFieldAt104(he, field, g_hudAlignXEnumNames, 3);
}

// 0x08103998  size=57  FUN_08103998  status=translated  naming=inferred
// naming: alternate setter of the "alignx" record.
void HudElem_SetAlignXEnumAlt(struct game_hudelem_s *he, int field)
{
    HudElem_AddPackedFieldString((char *)he, (struct hudelem_field_s *)&HUDFIELD104(field), (char **)g_hudAlignXEnumNames);   /* original caller also pushes the count 3 (ignored by the callee) */
}

// 0x081039d2  size=57  FUN_081039d2  status=translated  naming=inferred
// naming: table record "aligny" (setter 0x081039d2); enum names at .rodata 0x08187c98, count 3.
void HudElem_SetAlignYEnum(struct game_hudelem_s *he, int field)
{
    HudElem_SetEnumFieldAt104(he, field, g_hudAlignYEnumNames, 3);
}

// 0x08103a0c  size=57  FUN_08103a0c  status=translated  naming=inferred
// naming: alternate setter of the "aligny" record.
void HudElem_SetAlignYEnumAlt(struct game_hudelem_s *he, int field)
{
    HudElem_AddPackedFieldString((char *)he, (struct hudelem_field_s *)&HUDFIELD104(field), (char **)g_hudAlignYEnumNames);   /* original caller also pushes the count 3 (ignored by the callee) */
}

// 0x08103a46  size=57  FUN_08103a46  status=translated  naming=inferred
// naming: table record "horzalign" (setter 0x08103a46); enum names at .rodata 0x08187cc0 ("subleft"), count 8.
void HudElem_SetHorzAlignEnum(struct game_hudelem_s *he, int field)
{
    HudElem_SetEnumFieldAt104(he, field, g_hudHorzAlignEnumNames, 8);
}

// 0x08103a80  size=57  FUN_08103a80  status=translated  naming=inferred
// naming: alternate setter of the "horzalign" record.
void HudElem_SetHorzAlignEnumAlt(struct game_hudelem_s *he, int field)
{
    HudElem_AddPackedFieldString((char *)he, (struct hudelem_field_s *)&HUDFIELD104(field), (char **)g_hudHorzAlignEnumNames);   /* original caller also pushes the count 8 (ignored by the callee) */
}

// 0x08103aba  size=57  FUN_08103aba  status=translated  naming=inferred
// naming: vertical alignment record; enum names at .rodata 0x08187ce0 ("subtop"), count 8.
void HudElem_SetVertAlignEnum(struct game_hudelem_s *he, int field)
{
    HudElem_SetEnumFieldAt104(he, field, g_hudVertAlignEnumNames, 8);
}

// 0x08103af4  size=57  FUN_08103af4  status=translated  naming=inferred
// naming: alternate setter of the vertical alignment record.
void HudElem_SetVertAlignEnumAlt(struct game_hudelem_s *he, int field)
{
    HudElem_AddPackedFieldString((char *)he, (struct hudelem_field_s *)&HUDFIELD104(field), (char **)g_hudVertAlignEnumNames);   /* original caller also pushes the count 8 (ignored by the callee) */
}

static void HudElem_SetEnumFieldAt104(struct game_hudelem_s *he, int field, const char *const *names, int count)
{
    HudElem_SetEnumField((char *)he, (struct hudelem_field_s *)&HUDFIELD104(field), names, count);
}

// 0x08103c62  size=60  FUN_08103c62  status=translated  naming=inferred
// naming: method table entry "newhudelem" -> 0x08103c62; string "out of hudelems" referenced here.
void GScr_newhudelem(void)
{
    struct game_hudelem_s *he;

    he = (struct game_hudelem_s *)HudElem_Alloc(0x3ff, 0);
    if (he == NULL) {
        Scr_Error("out of hudelems");
    }
    Scr_AddHudElemObject(he);
}

// 0x08103c9e  size=108  FUN_08103c9e  status=translated  naming=inferred
// naming: method table entry "newclienthudelem" -> 0x08103c9e; string "not a client" referenced here.
void GScr_newclienthudelem(void)
{
    struct gentity_s *ent;
    struct game_hudelem_s *he;

    ent = Scr_GetEntityParam(0);
    if (ent->client == NULL) {
        Scr_ErrorAtParam(0, "not a client");
    }
    he = (struct game_hudelem_s *)HudElem_Alloc((int)ent->s.number, 0);
    if (he == NULL) {
        Scr_Error("out of hudelems");
    }
    Scr_AddHudElemObject(he);
}

// 0x08103d0a  size=192  FUN_08103d0a  status=translated  naming=inferred
// naming: method table entry "newteamhudelem" -> 0x08103d0a; message "team \"%s\" should be ..." here.
void GScr_newteamhudelem(void)
{
    uint16 teamHandle;
    int team;
    struct game_hudelem_s *he;

    teamHandle = (uint16)Scr_GetAny(0);
    if (teamHandle == scr_str_allies) {
        team = 2;   /* allies */
    } else if (teamHandle == scr_str_axis) {
        team = 1;   /* axis */
    } else if (teamHandle == scr_str_spectator) {
        team = 3;   /* spectator */
    } else {
        const char *text;

        text = Scr_ReleaseAnyValue(0);
        Scr_ErrorAtParam(0, va("team \"%s\" should be \"allies\", \"axis\", or \"spectator\"", text));
        team = 0;
    }
    he = (struct game_hudelem_s *)HudElem_Alloc(0x3ff, team);
    if (he == NULL) {
        Scr_Error("out of hudelems");
    }
    Scr_AddHudElemObject(he);
}

// 0x08103e52  size=79  FUN_08103e52  status=translated  naming=inferred
// naming: method table entry "settext" -> 0x08103e52.
void HudElem_SetText(uint32 entref)
{
    struct game_hudelem_s *he;
    const char *text;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    text = Scr_ReleaseLocalizedString(0);
    HudElem_ClearFields(he);
    he->elem.type = 1;
    he->elem.text = (int)G_LocalizedStringIndex(text);
}

// 0x08103ea2  size=114  FUN_08103ea2  status=translated  naming=inferred
// naming: method table entry "setplayernamestring" -> 0x08103ea2; message text references it.
void HudElem_SetPlayerNameString(uint32 entref)
{
    struct game_hudelem_s *he;
    struct gentity_s *ent;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    ent = Scr_GetEntityParam(0);
    if (ent == NULL) {
        Com_Printf("Invalid entity passed to hudelem setplayernamestring()\n");
    } else if (ent->client == NULL) {
        Com_Printf("Invalid entity passed to hudelem setplayernamestring(), entity is not a client\n");
    } else {
        HudElem_ClearFields(he);
        he->elem.type = 3;
        he->elem.value = (float)(int)ent->s.number;
    }
}

// 0x08103f14  size=143  FUN_08103f14  status=translated  naming=inferred
// naming: method table entry "setgametypestring" -> 0x08103f14.
void HudElem_SetGametypeString(uint32 entref)
{
    struct game_hudelem_s *he;
    const char *name;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    name = Scr_ReleaseAnyValue(0);
    if (name == NULL) {
        Com_Printf("Invalid entity passed to hudelem setgametypestring()\n");
    } else if (G_FindGametypeDescriptionByName(name) == NULL) {
        Com_Printf("Invalid gametype '%s'\n", name);
    } else {
        SV_SetConfigstring(0x14, (char *)name);
        HudElem_ClearFields(he);
        he->elem.type = 5;
        he->elem.value = 20.0f;   /* 0x41a00000 */
    }
}

// 0x08103fa4  size=125  FUN_08103fa4  status=translated  naming=inferred
// naming: method table entry "setmapnamestring" -> 0x08103fa4.
void HudElem_SetMapnameString(uint32 entref)
{
    struct game_hudelem_s *he;
    const char *name;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    name = Scr_ReleaseAnyValue(0);
    if (name == NULL) {
        Com_Printf("Invalid mapname passed to hudelem setmapnamestring()\n");
    } else if (SV_MapFileExists(name) == 0) {
        Com_Printf("Invalid map name passed to hudelem setmapnamestring(), map not found\n");
    } else {
        SV_SetConfigstring(0x13, (char *)name);
        HudElem_ClearFields(he);
        he->elem.type = 4;
    }
}

// 0x08104022  size=258  FUN_08104022  status=translated  naming=inferred
// naming: method table entry "setshader" -> 0x08104022.
void HudElem_SetShader(uint32 entref)
{
    struct game_hudelem_s *he;
    int argc;
    uint32 shader;
    int width;
    int height;
    const char *name;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    argc = Scr_GetNumParams();
    if (argc != 1 && argc != 3) {
        Scr_Error("USAGE: <hudelem> setShader(\"shadername\"[, optional_width, optional_height]);");
    }
    name = Scr_ReleaseAnyValue(0);
    shader = G_ShaderIndex(name);
    if (argc == 1) {
        width = 0;
        height = 0;
    } else {
        width = Scr_GetInt(1);
        if (width < 0) {
            Scr_ErrorAtParam(1, va("width %i < 0", width));
        }
        height = Scr_GetInt(2);
        if (height < 0) {
            Scr_ErrorAtParam(2, va("height %i < 0", height));
        }
    }
    HudElem_ClearFields(he);
    he->elem.type = 6;
    he->elem.materialIndex = (int)shader;
    he->elem.width = width;
    he->elem.height = height;
}

// 0x081043ca  size=35  FUN_081043ca  status=translated  naming=observed
// naming: literal "setTimerUp" is passed by this function (method name, script-visible).
void HudElem_SetTimerUp(uint32 entref)
{
    HudElem_SetTimerCommon(entref, 8, "setTimerUp");
}

// 0x081043ee  size=35  FUN_081043ee  status=translated  naming=observed
// naming: literal "setTenthsTimer" is passed by this function.
void HudElem_SetTenthsTimer(uint32 entref)
{
    HudElem_SetTimerCommon(entref, 9, "setTenthsTimer");
}

// 0x08104412  size=35  FUN_08104412  status=translated  naming=observed
// naming: literal "setTenthsTimerUp" is passed by this function.
void HudElem_SetTenthsTimerUp(uint32 entref)
{
    HudElem_SetTimerCommon(entref, 10, "setTenthsTimerUp");
}

// 0x08104436  size=35  FUN_08104436  status=translated  naming=observed
// naming: literal "setClock" is passed by this function.
void HudElem_SetClock(uint32 entref)
{
    HudElem_SetShaderTimerCommon(entref, 0xb, "setClock");
}

// 0x0810445a  size=35  FUN_0810445a  status=translated  naming=observed
// naming: literal "setClockUp" is passed by this function.
void HudElem_SetClockUp(uint32 entref)
{
    HudElem_SetShaderTimerCommon(entref, 0xc, "setClockUp");
}

// 0x0810447e  size=66  FUN_0810447e  status=translated  naming=inferred
// naming: method table entry "setvalue" -> 0x0810447e.
void HudElem_SetValue(uint32 entref)
{
    struct game_hudelem_s *he;
    float v;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    v = (float)Scr_GetFloat(0);
    HudElem_ClearFields(he);
    he->elem.type = 2;
    he->elem.value = v;
}

// 0x081044c0  size=55  FUN_081044c0  status=translated  naming=inferred
// naming: method table entry "setwaypoint" -> 0x081044c0. Note: no ClearFields in this path.
void HudElem_SetWaypoint(uint32 entref)
{
    struct game_hudelem_s *he;
    int value;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    value = Scr_GetInt(0);
    he->elem.type = 0xd;
    he->elem.value = (float)value;
}

// 0x081044f8  size=196  FUN_081044f8  status=translated  naming=inferred
// naming: method table entry "fadeovertime" -> 0x081044f8; messages "fade time %g <= 0" / "> 60" here.
void HudElem_FadeOverTime(uint32 entref)
{
    struct game_hudelem_s *he;
    float t;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    t = (float)Scr_GetFloat(0);
    if (t <= 0.0f) {
        Scr_ErrorAtParam(0, va("fade time %g <= 0", (double)t));
    } else if (60.0f < t) {
        Scr_ErrorAtParam(0, va("fade time %g > 60", (double)t));
    }
    he->elem.fadeStartTime = g_0864c56c;
    he->elem.fadeTime = Float_RoundToInt_08104a34(1000.0f * t);
    he->elem.fromColor = he->elem.color;
}

// 0x081045bc  size=278  FUN_081045bc  status=translated  naming=inferred
// naming: method table entry "scaleovertime" -> 0x081045bc; usage text "hudelem scaleOverTime(...)" here.
void HudElem_ScaleOverTime(uint32 entref)
{
    struct game_hudelem_s *he;
    int argc;
    float t;
    int newWidth;
    int newHeight;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    argc = Scr_GetNumParams();
    if (argc != 3) {
        Scr_Error("hudelem scaleOverTime(time_in_seconds, new_width, new_height)");
    }
    t = (float)Scr_GetFloat(0);
    if (t <= 0.0f) {
        Scr_ErrorAtParam(0, va("scale time %g <= 0", (double)t));
    } else if (60.0f < t) {
        Scr_ErrorAtParam(0, va("scale time %g > 60", (double)t));
    }
    newWidth = Scr_GetInt(1);
    newHeight = Scr_GetInt(2);
    he->elem.scaleStartTime = g_0864c56c;
    he->elem.scaleTime = Float_RoundToInt_08104a34(1000.0f * t);
    he->elem.fromWidth = he->elem.width;
    he->elem.fromHeight = he->elem.height;
    he->elem.width = newWidth;
    he->elem.height = newHeight;
}

// 0x081046d2  size=232  FUN_081046d2  status=translated  naming=inferred
// naming: method table entry "moveovertime" -> 0x081046d2; messages "move time %g <= 0" / "> 60" here.
void HudElem_MoveOverTime(uint32 entref)
{
    struct game_hudelem_s *he;
    float t;

    he = (struct game_hudelem_s *)HudElem_FromHandle(entref);
    t = (float)Scr_GetFloat(0);
    if (t <= 0.0f) {
        Scr_ErrorAtParam(0, va("move time %g <= 0", (double)t));
    } else if (60.0f < t) {
        Scr_ErrorAtParam(0, va("move time %g > 60", (double)t));
    }
    he->elem.moveStartTime = g_0864c56c;
    he->elem.moveTime = Float_RoundToInt_08104a34(1000.0f * t);
    memcpy(&he->elem.fromX, &he->elem.x, 4);
    memcpy(&he->elem.fromY, &he->elem.y, 4);
    memcpy(&he->elem.fromAlignOrg, &he->elem.alignOrg, 4);
    memcpy(&he->elem.fromAlignScreen, &he->elem.alignScreen, 4);
}

// 0x081047ba  size=33  FUN_081047ba  status=translated  naming=inferred
// naming: method table entry "reset" -> 0x081047ba.
void HudElem_Reset(uint32 entref)
{
    HudElem_Init((struct game_hudelem_s *)HudElem_FromHandle(entref));
}

// 0x081047dc  size=33  FUN_081047dc  status=translated  naming=inferred
// naming: method table entry "destroy" -> 0x081047dc.
void HudElem_Destroy(uint32 entref)
{
    HudElem_Free((struct game_hudelem_s *)HudElem_FromHandle(entref));
}

// 0x08105c80  size=43  FUN_08105c80  status=translated  naming=unknown
// naming: descriptive. Sets the turret pending flag (gentity+0x162) and forwards to 0x08105cac.
void Turret_SetPendingAndCall(struct gentity_s *ent, uint32 a, uint32 b)
{
    ent->active = 1;
    Item_TouchPickup(ent, (struct gentity_s *)(size_t)a, (int)b);
}

// 0x08106a90  size=1026  FUN_08106a90  status=translated  naming=observed
// naming: string "FinishSpawningItem: %s startsolid at %s\n" is referenced by this function.
void FinishSpawningItem(struct gentity_s *ent)
{
    struct trace_s tr;
    float mins[3];
    float maxs[3];
    float origin[3];
    float end[3];
    float dest[3];
    float axis[3][3];
    float angles[3];
    int mask;

    ent->handler = 0x11;
    if ((ent->spawnflags & 1) == 0) {
        if (BG_ITEM104_TYPE(ent->u.item.index) == 1) {
            Vec3Set(mins, -1.0f, -1.0f, -1.0f);
            Vec3Set(maxs, 1.0f, 1.0f, 1.0f);
        } else {
            Vec3Set(mins, -1.0f, -1.0f, 0.0f);
            Vec3Set(maxs, 1.0f, 1.0f, 2.0f);
        }
        mask = Ent_GetTraceMask(ent);
        Vec3Copy(ent->r.currentOrigin, origin);
        Vec3Set(end, ent->r.currentOrigin[0], ent->r.currentOrigin[1],
                    ent->r.currentOrigin[2] - 4096.0f);
        G_TraceBox(&tr, origin, mins, maxs, end, (int)ent->s.number, mask);
        if (tr.startsolid != 0) {
            Vec3Copy(ent->r.currentOrigin, origin);
            origin[2] = origin[2] - 15.0f;
            Vec3Set(end, ent->r.currentOrigin[0], ent->r.currentOrigin[1],
                        ent->r.currentOrigin[2] - 4096.0f);
            G_TraceBox(&tr, origin, mins, maxs, end, (int)ent->s.number, mask);
        }
        if (tr.startsolid != 0) {
            const char *where;
            const char *classText;

            where = Vec3_FormatRing(ent->r.currentOrigin);
            classText = ScrStr_TextFromHandle((uint16)ent->classname);
            Com_Printf("FinishSpawningItem: %s startsolid at %s\n", classText, where);
            G_FreeEntity(ent);
            return;
        }
        ent->s.groundEntityNum = (int)tr.entityNum;
        VectorLerp(origin, end, tr.fraction, dest);
        G_SetPositionTrajectory(ent, dest);
        if (tr.fraction < 1.0f) {
            Vec3Copy(tr.normal, axis[2]);
            Angles_ToVectors(ent->r.currentAngles, axis[0], NULL, NULL);
            Vec3_Cross(axis[2], axis[0], axis[1]);
            Vec3_Cross(axis[1], axis[2], axis[0]);
            AxisToAngles(&axis[0][0], angles);
            if (BG_ITEM104_TYPE(ent->s.index) == 1) {
                angles[2] = 90.0f + angles[2];
            }
            G_SetAnglesTrajectory(ent, angles);
        }
    } else {
        G_SetPositionTrajectory(ent, ent->r.currentOrigin);
    }
    SV_LinkEntity(ent);
}

// 0x08107216  size=283  FUN_08107216  status=translated  naming=inferred
// naming: descriptive for the body (precaches a weapon's hint strings and xmodels). The Com_Error
//         format at .rodata 0x0815a4c0 names the hintstring limit; the function itself references it.
void G_PrecacheWeapon(int weapon)
{
    uint8 *def;
    int found;

    g_itemRegisteredFlags[weapon] = 1;
    g_0864f980 = 1;
    g_0864f97c = 1;
    def = (uint8 *)BG_GetWeaponDef(weapon);
    if (**(char **)(def + 0x56c) != '\0') {
        found = G_FindOrAddHintString((int *)(def + 0x574), *(const char **)(def + 0x56c));
        if (found == 0) {
            Com_Error(1, "\x15Too many different hintstring values on weapons. Max allowed is %i different strings", 0x20);
        }
    }
    if (**(char **)(def + 0x570) != '\0') {
        found = G_FindOrAddHintString((int *)(def + 0x578), *(const char **)(def + 0x570));
        if (found == 0) {
            Com_Error(1, "\x15Too many different hintstring values on weapons. Max allowed is %i different strings", 0x20);
        }
    }
    {
        uint32 model;
        int dummy;

        model = G_ModelIndex(*(const char **)(def + 0x1b4));
        if (model != 0) {
            dummy = G_ModelIsDefault((int)model);
            if (dummy != 0) {
                G_SetModelHandle((int)model, "xmodel/defaultweapon");
            }
        }
    }
    G_ModelIndex(*(const char **)(def + 0x38c));
}

// 0x08108f0c  size=48  FUN_08108f0c  status=translated  naming=inferred
// naming: forwards to the server dobj creator (Com_ServerDObjCreate); name follows that callee.
void G_ServerDObjCreate(void *models, uint16 numModels, void *tree, int handle)
{
    Com_ServerDObjCreate(models, (int)numModels, tree, handle);
}

// 0x0810a6d4  size=31  FUN_0810a6d4  status=translated  naming=inferred
// naming: classname table entry "info_notnull" -> 0x0810a6d4 (spawn callback); sets origin from currentOrigin.
void SP_info_notnull(struct gentity_s *ent)
{
    G_SetPositionTrajectory(ent, ent->r.currentOrigin);
}

// 0x0810a6f4  size=19  FUN_0810a6f4  status=translated  naming=inferred
// naming: classname table entry "light" -> 0x0810a6f4 (spawn callback); body frees the entity via 0x0811ee50.
void SP_light(struct gentity_s *ent)
{
    G_FreeEntity(ent);
}

// 0x0810a7ec  size=19  FUN_0810a7ec  status=translated  naming=inferred
// naming: classname table entry "misc_model" -> 0x0810a7ec; same body as SP_light.
void SP_misc_model(struct gentity_s *ent)
{
    G_FreeEntity(ent);
}

// 0x0810a800  size=54  FUN_0810a800  status=translated  naming=unknown
// naming: descriptive. Link state update: unlinks when gentity+0xf0 is set, else clears the pending
//         flag and links the entity.
void Entity_SetLinkState(struct gentity_s *ent)
{
    if (ent->r.linked == 0) {
        ent->active = 0;
        SV_LinkEntity(ent);
    } else {
        SV_UnlinkEntity(ent);
    }
}

// 0x0810a836  size=19  FUN_0810a836  status=translated  naming=inferred
// naming: classname table entry "corona" -> 0x0810a836 (spawn callback); same body as SP_light.
void SP_corona(struct gentity_s *ent)
{
    G_FreeEntity(ent);
}

// 0x0810be6e  size=146  FUN_0810be6e  status=translated  naming=unknown
// naming: descriptive. Turret refresh: schedules the next think 50 ms ahead, runs the driver path when
//         a driver ref exists, and clears the 0x40 flag when the aim target has no client.
void Turret_Refresh(struct gentity_s *ent)
{
    ent->nextthink = g_0864c56c + 0x32;
    if (ent->tagInfo != NULL) {
        Ent_ResetAttachState(ent);
    }
    if (g_gentities[(uint32)ent->r.ownerNum].client == NULL) {
        Turret_UpdateAmmo(ent);
        ent->s.eFlags = ent->s.eFlags & 0xffffffbf;
        Turret_RestAim(ent, 0);
    }
}

// 0x0810bf00  size=526  FUN_0810bf00  status=translated  naming=unknown
// naming: descriptive. Searches up to 31 yaw steps (-3 degrees each) for a clear aim point between two
//         attach points; the first clear step is stored at turret+0x1c (aimOffset).
void Turret_FindAimOffset(struct gentity_s *ent)
{
    struct turret_s *turret;
    uint8 *attachA;
    uint8 *attachB;
    float axis[12]; /* [ebp-0x48]: AnglesToAxis rows 0..2 + currentOrigin as row 3 (affine 4x3) */
    float local_9c[3];
    float local_bc[3];
    float local_cc[3];
    float local_ac[3];
    float stepAngles[3];
    float stepAxis[9];
    trace_t trace; /* [ebp-0xf8] */
    int step;

    turret = ent->pTurretInfo;
    ent->handler = 0x0e;
    ent->nextthink = g_0864c56c + 0x32;
    attachA = (uint8 *)Ent_GetSlotBase(ent, scrStr_tag_aim);
    if (attachA != NULL) {
        attachB = (uint8 *)Ent_GetSlotBase(ent, scrStr_tag_butt);
        if (attachB != NULL) {
            AnglesToAxis(ent->r.currentAngles, axis);
            Vec3_Copy_0810cdaa(ent->r.currentOrigin, &axis[9]);
            Vec3_Sub_0810ce1e((float *)(attachB + 0x10), (float *)(attachA + 0x10), local_9c);
            Vec3_TransformAffine((float *)(attachA + 0x10), axis, local_bc);
            for (step = 0; step < 0x1f; step++) {
                stepAngles[0] = -3.0f * (float)step;
                stepAngles[1] = 0.0f;
                stepAngles[2] = 0.0f;
                AnglesToAxis(stepAngles, stepAxis);
                Vec3_RotateByTranspose(local_9c, stepAxis, local_ac);
                Vec3_Add_0810cdda((float *)(attachA + 0x10), local_ac, local_ac);
                Vec3_TransformAffine(local_ac, axis, local_cc);
                G_TracePointEx(&trace, local_bc, local_cc, (int)ent->s.number, 0x811, (const char *)g_08187c0c);
                if (trace.fraction < 1.0f) {
                    turret->aimOffset = stepAngles[0];
                    return;
                }
            }
        }
    }
}
// 0x0810c10e  size=175  FUN_0810c10e  status=translated  naming=unknown
// naming: descriptive. Applies the turret control angles (ctrl[0..1], then ctrl[2]) to three named
//         bones of the server dobj through DObj_SetControlBoneByName.
void Turret_SetControlBones(struct gentity_s *ent, const uint32 *partBits)
{
    struct DObj *obj;
    float angles[3];

    obj = Com_GetServerDObj((int)ent->s.number);
    angles[0] = ent->s.angles2[0];
    angles[1] = ent->s.angles2[1];
    angles[2] = 0.0f;
    DObj_SetControlBoneByName((struct DObj *)obj, partBits, (int)scrStr_tag_aim, angles);
    DObj_SetControlBoneByName((struct DObj *)obj, partBits, (int)scrStr_tag_aim_animated, angles);
    angles[0] = ent->s.angles2[2];
    angles[1] = 0.0f;
    DObj_SetControlBoneByName((struct DObj *)obj, partBits, (int)scrStr_tag_flash, angles);
}

// 0x0810c3de  size=724  FUN_0810c3de  status=translated  naming=unknown
// naming: descriptive. Seats a player (other) on a turret (ent): sets occupant state on both entities and
//         the client, copies the turret origin, sets the control-mode bits, and computes clamped control
//         angles and their limits.
void Turret_Occupy(struct gentity_s *ent, struct gentity_s *other)
{
    struct turret_s *turret;
    struct gclient_s *client;
    float value;

    turret = ent->pTurretInfo;
    client = other->client;
    other->active = 1;
    ent->active = 1;
    ent->r.ownerNum = (int)other->s.number;
    client->ps.viewlocked = 1;
    client->ps.viewlocked_entNum = (int)ent->s.number;
    turret->flags04 = turret->flags04 | 0x800;
    Vec3_Copy_0810cdaa(other->r.currentOrigin, turret->origin);
    other->s.otherEntityNum = (int)ent->s.number;
    ent->s.otherEntityNum = (int)other->s.number;
    if ((client->ps.pm_flags & 1) == 0) {
        if ((client->ps.pm_flags & 2) == 0) {
            turret->aimType = 0;
        } else {
            turret->aimType = 1;
        }
    } else {
        turret->aimType = 2;
    }
    if (turret->mode == 2) {
        client->ps.eFlags = client->ps.eFlags | 0x100;
        client->ps.eFlags = client->ps.eFlags & 0xfffffdff;
    } else if (turret->mode == 1) {
        client->ps.eFlags = client->ps.eFlags | 0x200;
        client->ps.eFlags = client->ps.eFlags & 0xfffffeff;
    } else {
        client->ps.eFlags = client->ps.eFlags | 0x300;
    }
    value = AngleSubtract(client->ps.viewangles[0], ent->r.currentAngles[0]);
    ent->s.angles2[0] = value;
    ent->s.angles2[0] = Float_Clamp_0810ccf2(ent->s.angles2[0], turret->arc0c, turret->off14);
    /* gameplay-fix(wpn_mantle_turret): 0x0810c593/0x0810c67a read currentAngles[1] (+0x148), the decompile used [2] */
    value = AngleSubtract(client->ps.viewangles[1], ent->r.currentAngles[1]);
    ent->s.angles2[1] = value;
    ent->s.angles2[1] = Float_Clamp_0810ccf2(ent->s.angles2[1], turret->arc10, turret->off18);
    ent->s.angles2[2] = 0.0f;
    client->ps.viewAngleClampRange[0] = 0.5f * AngleSubtract(turret->off14, turret->arc0c);
    client->ps.viewAngleClampBase[0] = ent->r.currentAngles[0] + turret->off14;
    client->ps.viewAngleClampBase[0] = AngleNormalize360_Loop(client->ps.viewAngleClampBase[0] - client->ps.viewAngleClampRange[0]);
    client->ps.viewAngleClampRange[1] = 0.5f * AngleSubtract(turret->off18, turret->arc10);
    client->ps.viewAngleClampBase[1] = ent->r.currentAngles[1] + turret->off18;
    client->ps.viewAngleClampBase[1] = AngleNormalize360_Loop(client->ps.viewAngleClampBase[1] - client->ps.viewAngleClampRange[1]);
}
// 0x0810d970  size=955  FUN_0810d970  status=translated  naming=unknown
// naming: descriptive. Turret setup at spawn: uses the weapon-def fields, the truncated trajectory
//         origin (FPU rounding control 0xc00 is truncation), a downward start-solid trace, the surface
//         flags of that trace, and the per-class callback 0x081022a0 when the def requests it.
void Turret_Setup(struct gentity_s *ent)
{
    uint8 *def;
    float pos[3];
    float origin[3];
    struct trace_s tr;
    uint8 dirIndex;
    uint32 surface;
    int hit;

    def = (uint8 *)BG_GetWeaponDef(ent->s.weapon);
    if (WD104_FIELD(def, int, 0x84) == 2 && ent->s.groundEntityNum == 0x3ff) {
        ent->nextthink = 0x32;
        return;
    }
    BG_EvaluateTrajectory(&ent->s.pos, g_0864c56c, pos);
    pos[0] = (float)(int)pos[0];
    pos[1] = (float)(int)pos[1];
    pos[2] = (float)(int)pos[2];
    G_SetPositionTrajectory(ent, pos);
    ent->s.eType = 0;
    ent->s.eFlags = ent->s.eFlags | 0x20;
    ent->flags = ent->flags | 0x800;
    ent->r.svFlags = ent->r.svFlags | 8;
    Vec3_Copy_0810ebae(ent->r.currentOrigin, origin);
    origin[2] = origin[2] - 16.0f;
    G_TraceBox(&tr, ent->r.currentOrigin, vec3_origin, vec3_origin, origin, (int)ent->s.number, 0x811);
    if (WD104_FIELD(def, int, 0x390) == 2) {
        dirIndex = Normal_DirToIndex(tr.normal);
        G_AddEvent(ent, 0xbf, dirIndex);
    } else {
        dirIndex = Normal_DirToIndex(tr.normal);
        G_AddEvent(ent, 0xbc, dirIndex);
    }
    hit = (int)SV_PointContents(ent->r.currentOrigin, -1, 0x20);
    surface = *(uint32 *)((uint8 *)&tr + 0x10);
    if (hit == 0) {
        ent->s.surfType = (int)((surface & 0x1f00000) >> 0x14);
    } else {
        ent->s.surfType = 0x14;
    }
    if (WD104_FIELD(def, const char *, 0x394) == NULL || *WD104_FIELD(def, const char *, 0x394) == '\0') {
        ent->freeAfterEvent = 1;
    } else {
        uint32 handle;
        float lifetime;
        int whole;

        ent->s.eFlags = ent->s.eFlags | 0x10000;
        Fx_CheckClientSlot0();
        handle = (uint32)(size_t)FX_GetEffectByName(WD104_FIELD(def, const char *, 0x394));
        ent->s.time = g_0864c56c;
        lifetime = FX_GetEffectDuration((struct FxEffectTemplate *)(size_t)handle);
        whole = (int)(1.0f + lifetime);
        ent->s.time2 = whole + g_0864c56c;
    }
    if (WD104_FIELD(def, int, 0x37c) != 0) {
        void *handler;

        handler = *(void **)((uint8 *)g_entityHandlerTable + (size_t)ent->handler * 0x28 + 0x24);
        G_RadiusDamage(ent->r.currentOrigin, ent, ent->parent,
                    (float)WD104_FIELD(def, int, 0x37c),
                    (float)WD104_FIELD(def, int, 0x380),
                    (float)WD104_FIELD(def, int, 0x378),
                    ent, (int)(size_t)handler);
    }
    SV_LinkEntity(ent);
}
// 0x0811003e  size=19  FUN_0811003e  status=translated  naming=inferred
// naming: classname table entry "trigger_use" -> 0x0811003e; forwards to SP_trigger_use.
void SP_trigger_use_spawn(struct gentity_s *ent)
{
    SP_trigger_use(ent);
}

// 0x08110052  size=19  FUN_08110052  status=translated  naming=inferred
// naming: classname table entry "trigger_use_touch" -> 0x08110052; forwards to SP_trigger_use.
void SP_trigger_use_touch_spawn(struct gentity_s *ent)
{
    SP_trigger_use(ent);
}

// 0x081105fc  size=20  FUN_081105fc  status=translated  naming=inferred
// naming: method table entry "getclandescription" -> 0x081105fc; pushes the empty string.
void GScr_getclandescription(void)
{
    Scr_AddString(g_0815b06e);
}

// 0x08110610  size=20  FUN_08110610  status=translated  naming=inferred
// naming: method table entry "getclanurl" -> 0x08110610; pushes the empty string.
void GScr_getclanurl(void)
{
    Scr_AddString(g_0815b06e);
}

// 0x08110ad2  size=44  FUN_08110ad2  status=translated  naming=inferred
// naming: method table entry "iprintln" -> 0x08110ad2; message type byte 'f' (0x66) via format "%c".
void GScr_iprintln(void)
{
    const char *text;

    text = va("%c", 0x66);
    GScr_SendGameMessage(-1, text);
}

// 0x08110afe  size=44  FUN_08110afe  status=translated  naming=inferred
// naming: method table entry "iprintlnbold" -> 0x08110afe; message type byte 'g' (0x67).
void GScr_iprintlnbold(void)
{
    const char *text;

    text = va("%c", 0x67);
    GScr_SendGameMessage(-1, text);
}

// 0x08110b2a  size=215  FUN_08110b2a  status=translated  naming=inferred
// naming: method table entry "print3d" -> 0x08110b2a. Argument shape: (origin, [rgb], [alpha], [scale], text).
void GScr_print3d(void)
{
    int argc;
    float scale;
    float color[4];
    float rgb[3];
    float origin[3];
    const char *text;

    scale = 1.0f;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    argc = Scr_GetNumParams();
    if (argc != 3) {
        if (argc < 4) {
            if (argc == 2) {
                goto after_color;
            }
            scale = (float)Scr_GetFloat(4);
        } else if (argc != 4) {
            scale = (float)Scr_GetFloat(4);
        }
        color[3] = (float)Scr_GetFloat(3);
    }
    Scr_GetVector(2, (float *)(rgb));
    Vec3_Copy_08118698((const uint32 *)rgb, (uint32 *)color);
after_color:
    text = Scr_ReleaseAnyValue(1);
    Scr_GetVector(0, (float *)(origin));
    G_NoopCallback(origin, color, scale, text);
}
// 0x08110c08  size=38  FUN_08110c08  status=translated  naming=inferred
// naming: method table entry "assert" -> 0x08110c08; message "assert fail" referenced here.
void GScr_assert(void)
{
    if (Scr_GetInt(0) == 0) {
        Scr_Error("assert fail");
    }
}

// 0x08110c2e  size=62  FUN_08110c2e  status=translated  naming=inferred
// naming: method table entry "assertex" -> 0x08110c2e; message "assert fail: %s" referenced here.
void GScr_assertex(void)
{
    int cond;
    const char *msg;

    cond = Scr_GetInt(0);
    if (cond == 0) {
        msg = Scr_ReleaseAnyValue(1);
        Scr_Error(va("assert fail: %s", msg));
    }
}

// 0x08110c6c  size=44  FUN_08110c6c  status=translated  naming=inferred
// naming: method table entry "assertmsg" -> 0x08110c6c; unconditional failure with a message.
void GScr_assertmsg(void)
{
    const char *msg;

    msg = Scr_ReleaseAnyValue(0);
    Scr_Error(va("assert fail: %s", msg));
}

// 0x08110c98  size=101  FUN_08110c98  status=translated  naming=inferred
// naming: method table entry "isdefined" -> 0x08110c98.
void GScr_isdefined(void)
{
    int type;
    int value;
    int result;

    type = (int)Scr_GetParamType(0);
    if (type == 1) {
        value = Scr_GetPointer(0);
        result = 0;
        if (value < 0x17 && value != 0x14) {
            result = 1;
        }
        Scr_AddIndex(result);
    } else {
        Scr_AddIndex(type != 0);
    }
}

// 0x08110cfe  size=37  FUN_08110cfe  status=translated  naming=inferred
// naming: method table entry "isstring" -> 0x08110cfe.
void GScr_isstring(void)
{
    int type;

    type = (int)Scr_GetParamType(0);
    Scr_AddIndex(type == 2);
}

// 0x08110d24  size=123  FUN_08110d24  status=translated  naming=inferred
// naming: method table entry "isalive" -> 0x08110d24.
void GScr_isalive(void)
{
    int type;
    int value;
    struct gentity_s *ent;

    type = (int)Scr_GetParamType(0);
    if (type == 1) {
        value = Scr_GetPointer(0);
        if (value == 0x15) {
            ent = Scr_GetEntityParam(0);
            if (ent->health < 1) {
                Scr_AddIndex(0);
            } else {
                Scr_AddIndex(1);
            }
        } else {
            Scr_AddIndex(0);
        }
    } else {
        Scr_AddIndex(0);
    }
}

// 0x08110da0  size=48  FUN_08110da0  status=translated  naming=inferred
// naming: method table entry "getcvar" -> 0x08110da0.
void GScr_getcvar(void)
{
    const char *name;

    name = Scr_ReleaseAnyValue(0);
    Scr_AddString(Dvar_GetDisplayString(name));
}

// 0x08110dd0  size=56  FUN_08110dd0  status=translated  naming=inferred
// naming: method table entry "getcvarint" -> 0x08110dd0.
void GScr_getcvarint(void)
{
    const char *name;
    int value;

    name = Scr_ReleaseAnyValue(0);
    value = atoi(Dvar_GetDisplayString(name));
    Scr_AddIndex(value);
}

// 0x08110e08  size=62  FUN_08110e08  status=translated  naming=inferred
// naming: method table entry "getcvarfloat" -> 0x08110e08.
void GScr_getcvarfloat(void)
{
    const char *name;
    float value;

    name = Scr_ReleaseAnyValue(0);
    value = (float)atof(Dvar_GetDisplayString(name));
    Scr_AddFloatBits(Chunk104_F2U(value));
}

// 0x08110e46  size=459  FUN_08110e46  status=translated  naming=inferred
// naming: method table entry "setcvar" -> 0x08110e46. Value is an array of parts when the second
//         argument is of type 3. Note: the quote-normalised copy is built but the raw value is what
//         Dvar_SetStringOrCreate receives (preserved from the binary). The copy is bounded to 0x3ff
//         bytes here; the original loop had no such bound (see open_questions).
void GScr_setcvar(void)
{
    const char *name;
    const char *value;
    char raw[0x400];
    char normalized[0x400];
    uint32 paramType;
    int argc;
    int i;
    char *dst;
    int setFlags;
    struct dvar_s *dvar;

    name = Scr_ReleaseAnyValue(0);
    paramType = Scr_GetParamType(1);
    if (paramType == 3) {
        argc = Scr_GetNumParams();
        GScr_BuildMessageString(1, argc - 1, "Dvar Value", raw, 0x400);
        value = raw;
    } else {
        value = Scr_ReleaseAnyValue(1);
    }
    dst = normalized;
    for (i = 0; i < 0x2000 && value[i] != '\0' && i < 0x3ff; i++) {
        *dst = (char)Q_NormalizeApostrophe(value[i]);
        if (*dst == '"') {
            *dst = '\'';
        }
        dst++;
    }
    *dst = '\0';
    if (Com_IsIdentifierName(name) == 0) {
        Scr_Error(va("Dvar %s has an invalid dvar name", name));
    } else {
        setFlags = 0;
        argc = Scr_GetNumParams();
        if (argc > 2 && Scr_GetInt(2) != 0) {
            setFlags = 1;
        }
        Dvar_SetStringOrCreate(name, value);
        if (setFlags != 0) {
            dvar = Dvar_Lookup(name);
            Dvar_OrFlags(dvar, 0x400);
        }
    }
}

// 0x08111012  size=21  FUN_08111012  status=translated  naming=inferred
// naming: method table entry "gettime" -> 0x08111012; pushes level time (ms).
void GScr_gettime(void)
{
    Scr_AddIndex(g_0864c56c);
}

// 0x08111028  size=80  FUN_08111028  status=translated  naming=inferred
// naming: method table entry "getentbynum" -> 0x08111028.
void GScr_getentbynum(void)
{
    uint32 num;

    num = (uint32)Scr_GetInt(0);
    if (num < 0x400 && g_gentities[num].r.inuse != 0) {
        Scr_AddEntityParam(&g_gentities[num]);
    }
}

// 0x08111078  size=140  FUN_08111078  status=translated  naming=inferred
// naming: method table entry "getweaponmodel" -> 0x08111078; message "unknown weapon '%s' in getWeaponModel".
void GScr_getweaponmodel(void)
{
    const char *name;
    int weapon;
    uint8 *def;

    name = Scr_ReleaseAnyValue(0);
    weapon = G_LoadWeaponDef(name);
    if (weapon == 0) {
        if (*name != '\0') {
            if (Q_stricmp(name, g_0815b381) != 0) {
                Com_Printf(va("unknown weapon '%s' in getWeaponModel\n", name));
            }
        }
        Scr_AddString(g_0815b06e);
    } else {
        def = (uint8 *)BG_GetWeaponDef(weapon);
        Scr_AddString((const char *)WD104_FIELD(def, void *, 0x1b4));
    }
}

// 0x08111104  size=103  FUN_08111104  status=translated  naming=inferred
// naming: method table entry "getammocount" -> 0x08111104.
void GScr_getammocount(uint32 entref)
{
    struct gentity_s *ent;
    const char *name;
    int weapon;

    ent = (struct gentity_s *)GScr_GetPlayerEntity(entref);
    name = Scr_ReleaseAnyValue(0);
    weapon = G_LoadWeaponDef(name);
    if (weapon == 0) {
        Scr_AddIndex(0);
    } else {
        Scr_AddIndex(BG_GetWeaponTotalAmmo((struct playerState_s *)ent->client, weapon));
    }
}

// 0x0811116c  size=129  FUN_0811116c  status=translated  naming=inferred
// naming: method table entry "getanimlength" -> 0x0811116c; message "non-primitive animation has no concept of length".
void GScr_getanimlength(void)
{
    uint32 anim[2];
    int treeSlot;
    uint8 primitive;
    float length;

    Scr_GetAnim((struct cod2_scr_anim_s *)anim, 0, 0);
    treeSlot = Anim_GetTreeSlot((int)(anim[0] >> 16));
    primitive = XAnim_NodeIsLeaf((struct XAnimNodeTable *)(size_t)treeSlot, (int)(anim[0] & 0xffff));
    if (primitive == 0) {
        Scr_ErrorNoParam("non-primitive animation has no concept of length");
    }
    length = XAnim_NodeDurationSec((struct XAnimNodeTable *)(size_t)treeSlot, (int)(anim[0] & 0xffff));
    Scr_AddFloatBits(Chunk104_F2U(length));
}

// 0x081111ee  size=109  FUN_081111ee  status=translated  naming=inferred
// naming: method table entry "animhasnotetrack" -> 0x081111ee.
void GScr_animhasnotetrack(void)
{
    uint32 anim[2];
    uint16 noteName;
    int treeSlot;
    uint8 has;

    Scr_GetAnim((struct cod2_scr_anim_s *)anim, 0, 0);
    noteName = (uint16)Scr_GetAny(1);
    treeSlot = Anim_GetTreeSlot((int)(anim[0] >> 16));
    has = XAnim_ClipHasKey((struct XAnimNodeTable *)(size_t)treeSlot, (int)(anim[0] & 0xffff), noteName);
    Scr_AddInt((int)has);
}

// 0x0811125c  size=96  FUN_0811125c  status=translated  naming=inferred
// naming: method table entry "getbrushmodelcenter" -> 0x0811125c; center = (vec120 + vec12c) * 0.5.
void GScr_getbrushmodelcenter(void)
{
    struct gentity_s *ent;
    float center[3];

    ent = Scr_GetEntityParam(0);
    Vec3_Add_081186c8(ent->r.absmin, ent->r.absmax, center);
    Vec3_Scale_08118750(center, 0.5f, center);
    Scr_AddVector((const float *)(center));
}

// 0x081112bc  size=208  FUN_081112bc  status=translated  naming=inferred
// naming: method table entry "spawn" -> 0x081112bc; message "unable to spawn \"%s\" entity".
void GScr_spawn(void)
{
    uint16 classHandle;
    float origin[3];
    int angle;
    struct gentity_s *ent;
    uint32 argc;
    int spawned;

    classHandle = (uint16)Scr_GetAny(0);
    Scr_GetVector(1, (float *)(origin));
    argc = (uint32)Scr_GetNumParams();
    if (argc < 3) {
        angle = 0;
    } else {
        angle = Scr_GetInt(2);
    }
    ent = (struct gentity_s *)G_Spawn();
    ScrStr_AssignRef(&ent->classname, classHandle);
    Vec3_Copy_08118698((const uint32 *)origin, (uint32 *)ent->r.currentOrigin);
    ent->spawnflags = (uint32)angle;
    spawned = G_CallSpawnEntity(ent);
    if (spawned == 0) {
        const char *text;

        text = ScrStr_TextFromHandle(classHandle);
        Scr_Error(va("unable to spawn \"%s\" entity", text));
    } else {
        Scr_AddEntityParam(ent);
    }
}

// 0x0811138c  size=142  FUN_0811138c  status=translated  naming=inferred
// naming: method table entry "spawnturret" -> 0x0811138c.
void GScr_spawnturret(void)
{
    uint16 classHandle;
    float origin[3];
    const char *weaponName;
    struct gentity_s *ent;

    classHandle = (uint16)Scr_GetAny(0);
    Scr_GetVector(1, (float *)(origin));
    weaponName = Scr_ReleaseAnyValue(2);
    ent = (struct gentity_s *)G_Spawn();
    ScrStr_AssignRef(&ent->classname, classHandle);
    Vec3_Copy_08118698((const uint32 *)origin, (uint32 *)ent->r.currentOrigin);
    G_SpawnTurret(ent, weaponName);
    Scr_AddEntityParam(ent);
}

// 0x0811141a  size=55  FUN_0811141a  status=translated  naming=inferred
// naming: method table entry "precacheturret" -> 0x0811141a; message names the wait-statement rule.
void GScr_precacheturret(void)
{
    const char *name;

    if (level_initializing == 0) {
        Scr_Error("precacheTurret must be called before any wait statements in the level script\n");
    }
    name = Scr_ReleaseAnyValue(0);
    G_LoadWeaponDef(name);
}

// 0x08111452  size=239  FUN_08111452  status=translated  naming=inferred
// naming: method table entry "attach" -> 0x08111452.
void GScr_attach(uint32 entref)
{
    struct gentity_s *ent;
    const char *model;
    uint32 tag;
    uint32 flag;
    int argc;
    int already;

    ent = (struct gentity_s *)GScr_GetEntityFromRef(entref);
    model = Scr_ReleaseAnyValue(0);
    argc = Scr_GetNumParams();
    if (argc < 2) {
        tag = scr_str_emptystring;
    } else {
        tag = Scr_GetLowerString(1);
    }
    argc = Scr_GetNumParams();
    if (argc < 3) {
        flag = 0;
    } else {
        flag = (uint32)Scr_GetInt(2);
    }
    already = Ent_RemoveAttachment(ent, model, tag);
    if (already != 0) {
        const char *tagText;

        tagText = ScrStr_TextFromHandle(tag);
        Scr_Error(va("model '%s' already attached to tag '%s'", model, tagText));
    }
    if (Ent_AddAttachment(ent, model, tag, (int)flag) == 0) {
        Scr_Error("maximum attached models exceeded");
    }
}

// 0x08111542  size=298  FUN_08111542  status=translated  naming=inferred
// naming: method table entry "detach" -> 0x08111542; message "failed to detach model ..." here.
void GScr_detach(uint32 entref)
{
    struct gentity_s *ent;
    const char *model;
    uint32 tag;
    int argc;
    int found;

    ent = (struct gentity_s *)GScr_GetEntityFromRef(entref);
    model = Scr_ReleaseAnyValue(0);
    argc = Scr_GetNumParams();
    if (argc < 2) {
        tag = scr_str_emptystring;
    } else {
        tag = Scr_GetLowerString(1);
    }
    found = Ent_RemoveAttachment(ent, model, tag);
    if (found == 0) {
        int i;

        Com_Printf("Current attachments:\n");
        for (i = 0; i < 7; i++) {
            if (ent->attachModelNames[i] != 0 && ent->attachTagNames[i] != 0) {
                const char *modelText;
                const char *tagText;

                modelText = G_GetModelName((uint8)ent->attachModelNames[i]);
                tagText = ScrStr_TextFromHandle((uint32)ent->attachTagNames[i]);
                Com_Printf("model: '%s', tag: '%s'\n", modelText, tagText);
            }
        }
        {
            const char *tagText;

            tagText = ScrStr_TextFromHandle(tag);
            Scr_Error(va("failed to detach model '%s' from tag '%s'", model, tagText));
        }
    }
}

// 0x0811166c  size=33  FUN_0811166c  status=translated  naming=inferred
// naming: method table entry "detachall" -> 0x0811166c.
void GScr_detachall(uint32 entref)
{
    Ent_ClearAttachments(GScr_GetEntityFromRef(entref));
}

// 0x0811168e  size=73  FUN_0811168e  status=translated  naming=inferred
// naming: method table entry "getattachsize" -> 0x0811168e; counts occupied attach slots (max 7).
void GScr_getattachsize(uint32 entref)
{
    struct gentity_s *ent;
    int i;

    ent = (struct gentity_s *)GScr_GetEntityFromRef(entref);
    for (i = 0; i < 7 && ent->attachModelNames[i] != 0; i++) {
    }
    Scr_AddIndex(i);
}
