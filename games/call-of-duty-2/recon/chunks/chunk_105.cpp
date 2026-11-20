#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include <ctype.h>
#include "chunk_105_types.h"

/*
 * Chunk 105: GSC entity methods and script functions that Ghidra's auto-analysis missed; recovered
 * from .eh_frame FDE starts and scattered between already-reconstructed chunks. Each entry is
 * reachable only through the script method/function tables in .data (name pointer, function pointer,
 * flag word; stride 0xc). The functions never reference their own name strings, so every name is
 * "inferred" from the table entry. Entity methods take the script entity reference (entref);
 * plain script functions take no C arguments and read their parameters through Scr_Get*.
 */

static struct gentity_s *Ent105(uint32 entref)
{
    return (struct gentity_s *)GScr_GetEntityFromRef(entref);
}

static const char *Scr_GetString105(uint32 param)
{
    return Scr_ReleaseAnyValue(param);
}

static const char *Scr_GetLocalized105(uint32 param)
{
    return Scr_ReleaseLocalizedString(param);
}

/* The binary pushes the float by value; Scr_AddFloatBits takes the same 32 bits. */
static void Scr_AddFloat105(float value)
{
    uint32 bits;

    memcpy(&bits, &value, sizeof(bits));
    Scr_AddFloatBits(bits);
}

static void GetVec105(uint32 param, float *out)
{
    Scr_GetVector(param, out);
}

static void AddVec105(const float *vec)
{
    Scr_AddVector(vec);
}

/* Shared objective index range check; the binary inlines it at every call site. */
static void CheckObjectiveIndex105(int idx, int param)
{
    if (idx < 0 || 0xf < idx) {
        Scr_ErrorAtParam(param, va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", idx, 0xf));
    }
}

// 0x081116d8  size=109  FUN_081116d8  status=translated  naming=inferred
// naming: script method table entry "getattachmodelname" at 0x08188754 points to this function; the function does not reference the string.
void ScriptEntCmd_getattachmodelname(uint32 entref)
{
    struct gentity_s *ent;
    uint32 slot;
    uint32 modelName;

    ent = Ent105(entref);
    slot = (uint32)Scr_GetInt(0);
    if (6 < slot || ent->attachModelNames[slot] == 0) {
        Scr_ErrorAtParam(0, "bad index");
    }
    Scr_AddString(G_GetModelName(ent->attachModelNames[slot]));
}

// 0x08111746  size=101  FUN_08111746  status=translated  naming=inferred
// naming: script method table entry "getattachtagname" at 0x08188760 points to this function; the function does not reference the string.
void ScriptEntCmd_getattachtagname(uint32 entref)
{
    struct gentity_s *ent;
    uint32 slot;

    ent = Ent105(entref);
    slot = (uint32)Scr_GetInt(0);
    if (6 < slot || ent->attachModelNames[slot] == 0) {
        Scr_ErrorAtParam(0, "bad index");
    }
    Scr_AddStringId(ent->attachTagNames[slot]);
}

// 0x081117ac  size=109  FUN_081117ac  status=translated  naming=inferred
// naming: script method table entry "getattachignorecollision" at 0x0818876c points to this function; the function does not reference the string.
void ScriptEntCmd_getattachignorecollision(uint32 entref)
{
    struct gentity_s *ent;
    uint32 slot;

    ent = Ent105(entref);
    slot = (uint32)Scr_GetInt(0);
    if (6 < slot || ent->attachModelNames[slot] == 0) {
        Scr_ErrorAtParam(0, "bad index");
    }
    Scr_AddInt((int)ent->attachIgnoreCollision >> ((uint8)slot & 0x1f) & 1);
}

// 0x0811181a  size=534  FUN_0811181a  status=translated  naming=inferred
// naming: script method table entry "linkto" at 0x081887c0 points to this function; the function does not reference the string.
void ScriptEntCmd_linkto(uint32 entref)
{
    struct gentity_s *ent;
    struct gentity_s *parent;
    uint32 tag;
    int numParams;
    bool linked;
    float angles[3];
    float offset[3];

    ent = Ent105(entref);
    if (Scr_GetParamType(0) != 1 || Scr_GetPointer(0) != 0x15) {
        Scr_ErrorAtParam(0, "not an entity");
    }
    if ((ent->flags & 0x1000) == 0) {
        Scr_ErrorNoParam(va("entity (classname: '%s') does not currently support linkTo",
                            ScrStr_TextFromHandle(ent->classname)));
    }
    parent = (struct gentity_s *)Scr_GetEntityParam(0);
    numParams = Scr_GetNumParams();
    tag = 0;
    if (1 < numParams) {
        tag = Scr_GetLowerString(1);
        if (*ScrStr_TextFromHandle(tag) == '\0') {
            tag = 0;
        }
    }
    if (numParams < 3) {
        linked = Ent_LinkChildAndReset((struct gentity_s *)ent, (struct gentity_s *)parent, (int32)tag);
    } else {
        GetVec105(2, angles);
        GetVec105(3, offset);
        linked = Ent_LinkChildWithOffsets((struct gentity_s *)ent, (struct gentity_s *)parent, (int32)tag,
                                          angles, offset);
    }
    if (!linked) {
        if (SV_DObjExists(parent) == 0) {
            if (parent->model == 0) {
                Scr_Error("failed to link entity since parent has no model");
            }
            Scr_Error(va("failed to link entity since parent model '%s' is invalid", G_GetModelName(parent->model)));
        }
        if (tag != 0 && SV_DObjGetBoneIndexByName(parent, (int)tag) < 0) {
            const char *modelName;
            const char *tagText;

            SV_PrintModelInfo((const int *)parent);
            modelName = G_GetModelName(parent->model);
            tagText = ScrStr_TextFromHandle(tag);
            Scr_Error(va("failed to link entity since tag '%s' does not exist in parent model '%s'", tagText, modelName));
        }
        Scr_Error("failed to link entity due to link cycle");
    }
}

// 0x08111a30  size=33  FUN_08111a30  status=translated  naming=inferred
// naming: script method table entry "unlink" at 0x081887cc points to this function; the function does not reference the string.
void ScriptEntCmd_unlink(uint32 entref)
{
    Ent_UnlinkFromParent((struct gentity_s *)Ent105(entref));
}

// 0x08111a52  size=141  FUN_08111a52  status=translated  naming=inferred
// naming: script method table entry "enablelinkto" at 0x081887d8 points to this function; the function does not reference the string.
void ScriptEntCmd_enablelinkto(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    if ((ent->flags & 0x1000) != 0) {
        Scr_ErrorNoParam("entity already has linkTo enabled");
    }
    if (ent->s.eType != 0 || ent->physicsObject != 0) {
        Scr_ErrorNoParam(va("entity (classname: '%s') does not currently support enableLinkTo",
                            ScrStr_TextFromHandle(ent->classname)));
    }
    ent->flags = ent->flags | 0x1000;
}

// 0x08111ae0  size=56  FUN_08111ae0  status=translated  naming=inferred
// naming: script method table entry "getorigin" at 0x081887e4 points to this function; the function does not reference the string.
void ScriptEntCmd_getorigin(uint32 entref)
{
    struct gentity_s *ent;
    float out[3];

    ent = Ent105(entref);
    Vec3_Copy_08118698((const uint32 *)ent->r.currentOrigin, (uint32 *)out);
    AddVec105(out);
}

// 0x08111b18  size=70  FUN_08111b18  status=translated  naming=inferred
// naming: script method table entry "geteye" at 0x081887f0 points to this function; the function does not reference the string.
void ScriptEntCmd_geteye(uint32 entref)
{
    struct gentity_s *ent;
    float out[3];

    ent = Ent105(entref);
    Vec3_Copy_08118698((const uint32 *)ent->r.currentOrigin, (uint32 *)out);
    out[2] = 40.0f + out[2];
    AddVec105(out);
}

// 0x08111b5e  size=138  FUN_08111b5e  status=translated  naming=inferred
// naming: script method table entry "useby" at 0x081887fc points to this function; the function does not reference the string.
void ScriptEntCmd_useby(uint32 entref)
{
    struct gentity_s *ent;
    struct gentity_s *other;
    void (*handler)(struct gentity_s *, struct gentity_s *, struct gentity_s *);

    ent = Ent105(entref);
    other = (struct gentity_s *)Scr_GetEntityParam(0);
    Scr_AddEntityParam((struct gentity_s *)other);
    Scr_NotifyEntity((struct gentity_s *)ent, scr_str_trigger, 1);
    handler = g_08187d10[ent->handler].use;
    if (handler != 0) {
        handler(ent, other, other);
    }
}

// 0x08111be8  size=274  FUN_08111be8  status=translated  naming=inferred
// naming: script method table entry "istouching" at 0x08188814 points to this function; the function does not reference the string. Note: Ghidra shows the SV_EntityContact result stored as a full dword; the unsigned char return is widened here.
void ScriptEntCmd_istouching(uint32 entref)
{
    struct gentity_s *ent;
    struct gentity_s *saved;
    struct gentity_s *other;
    float mins[3];
    float maxs[3];

    ent = Ent105(entref);
    if (ent->r.bmodel == '\0' && (ent->r.svFlags & 0x60) == 0) {
        other = (struct gentity_s *)Scr_GetEntityParam(0);
    } else {
        saved = ent;
        ent = (struct gentity_s *)Scr_GetEntityParam(0);
        if (ent->r.bmodel != '\0' || (ent->r.svFlags & 0x60) != 0) {
            Scr_Error("istouching cannot be called on 2 brush/cylinder entities");
        }
        other = saved;
    }
    Vec3_Add_081186c8(ent->r.currentOrigin, ent->r.mins, mins);
    Vec3_Add_081186c8(ent->r.currentOrigin, ent->r.maxs, maxs);
    Vec3Pair_PushZApart(mins, maxs);
    Scr_AddIndex((int32)SV_EntityContact(mins, maxs, (const struct gentity_s *)other));
}

// 0x08111cfa  size=50  FUN_08111cfa  status=translated  naming=inferred
// naming: script function table entry "soundexists" at 0x081886f8 points to this function; the function does not reference the string.
void GScr_soundexists(void)
{
    const char *alias;

    alias = Scr_GetString105(0);
    Scr_AddInt(SND_FindAliasList(alias) != 0);
}

// 0x08111d2c  size=112  FUN_08111d2c  status=translated  naming=inferred
// naming: script method table entry "playsound" at 0x08188820 points to this function; the function does not reference the string. Note: Callee merged under the name G_SoundIndex (configstring base 0x24e); used here as a sound alias index.
void ScriptEntCmd_playsound(uint32 entref)
{
    struct gentity_s *ent;
    struct gentity_s *temp;
    uint8 soundIndex;

    ent = Ent105(entref);
    soundIndex = G_SoundIndex(Scr_GetString105(0));
    temp = (struct gentity_s *)G_TempEntity(ent->r.currentOrigin, 0xb3);
    temp->r.svFlags = temp->r.svFlags | 8;
    temp->s.eventParm = (uint32)soundIndex;
}

// 0x08111d9c  size=112  FUN_08111d9c  status=translated  naming=inferred
// naming: script method table entry "playsoundasmaster" at 0x0818882c points to this function; the function does not reference the string. Note: Same as playsound with event type 0xb4.
void ScriptEntCmd_playsoundasmaster(uint32 entref)
{
    struct gentity_s *ent;
    struct gentity_s *temp;
    uint8 soundIndex;

    ent = Ent105(entref);
    soundIndex = G_SoundIndex(Scr_GetString105(0));
    temp = (struct gentity_s *)G_TempEntity(ent->r.currentOrigin, 0xb4);
    temp->r.svFlags = temp->r.svFlags | 8;
    temp->s.eventParm = (uint32)soundIndex;
}

// 0x08111e0c  size=77  FUN_08111e0c  status=translated  naming=inferred
// naming: script method table entry "playloopsound" at 0x08188838 points to this function; the function does not reference the string.
void ScriptEntCmd_playloopsound(uint32 entref)
{
    struct gentity_s *ent;
    uint8 soundIndex;

    ent = Ent105(entref);
    soundIndex = G_SoundIndex(Scr_GetString105(0));
    ent->r.broadcastTime = -1;
    ent->s.loopSound = (int)(uint32)soundIndex;
}

// 0x08111e5a  size=54  FUN_08111e5a  status=translated  naming=inferred
// naming: script method table entry "stoploopsound" at 0x08188844 points to this function; the function does not reference the string.
void ScriptEntCmd_stoploopsound(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    ent->r.broadcastTime = g_0864c56c + 300;
    ent->s.loopSound = 0;
}

// 0x08111ea2  size=111  FUN_08111ea2  status=translated  naming=inferred
// naming: script method table entry "delete" at 0x08188874 points to this function; the function does not reference the string.
void ScriptEntCmd_delete(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    if (ent->client != 0) {
        Scr_Error("Cannot delete a client entity");
    }
    if (g_currentThinkEntNum == ent->s.number) {
        Scr_Error("Cannot delete entity during its think");
    }
    Scr_NotifyEntity((struct gentity_s *)ent, scr_str_death, 0);
    G_FreeEntity((struct gentity_s *)ent);
}

// 0x08111f12  size=77  FUN_08111f12  status=translated  naming=inferred
// naming: script method table entry "setmodel" at 0x08188880 points to this function; the function does not reference the string.
void ScriptEntCmd_setmodel(uint32 entref)
{
    struct gentity_s *ent;
    const char *modelName;

    ent = Ent105(entref);
    modelName = Scr_GetString105(0);
    Ent_SetModelByName((struct gentity_s *)ent, modelName);
    Ent_RefreshAttachments((struct gentity_s *)ent);
    SV_LinkEntity((struct gentity_s *)ent);
}

// 0x08111f60  size=114  FUN_08111f60  status=translated  naming=inferred
// naming: script method table entry "getnormalhealth" at 0x0818888c points to this function; the function does not reference the string.
void ScriptEntCmd_getnormalhealth(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    if (ent->client == 0) {
        Scr_AddFloat105((float)ent->health);
    } else if (ent->health == 0) {
        Scr_AddFloat105(0.0f);
    } else {
        Scr_AddFloat105((float)ent->health / (float)ent->client->maxHealth);
    }
}

// 0x08111fd2  size=278  FUN_08111fd2  status=translated  naming=inferred
// naming: script method table entry "setnormalhealth" at 0x08188898 points to this function; the function does not reference the string. Note: x87 float*int products are computed in extended precision; double is used here.
void ScriptEntCmd_setnormalhealth(uint32 entref)
{
    struct gentity_s *ent;
    float fraction;
    int newHealth;

    ent = Ent105(entref);
    fraction = Scr_GetFloat(0);
    if (1.0f < fraction) {
        fraction = 1.0f;
    }
    if (ent->client == 0) {
        if (ent->maxHealth == 0) {
            newHealth = (int)fraction;
        } else {
            newHealth = (int)((double)ent->maxHealth * (double)fraction);
        }
    } else {
        newHealth = Float_RoundHalfUp((float)((double)ent->client->maxHealth * (double)fraction));
        SV_SendServerCommandNum((int)(ent - g_gentities), 0, va("%c \"%i\"", 0x49, 0));
    }
    if (newHealth < 1) {
        Com_Printf("ERROR: Cannot setnormalhealth to 0 or below.\n");
    } else {
        ent->health = newHealth;
    }
}

// 0x081120e8  size=71  FUN_081120e8  status=translated  naming=inferred
// naming: script method table entry "show" at 0x081888a4 points to this function; the function does not reference the string.
void ScriptEntCmd_show(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    ent->flags = ent->flags & 0xfffff7ff;
    ent->r.clientMask[0] = 0;
    ent->r.clientMask[1] = 0;
}

// 0x08112130  size=71  FUN_08112130  status=translated  naming=inferred
// naming: script method table entry "hide" at 0x081888b0 points to this function; the function does not reference the string.
void ScriptEntCmd_hide(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    ent->flags = ent->flags | 0x800;
    ent->r.clientMask[0] = 0xffffffff;
    ent->r.clientMask[1] = 0xffffffff;
}

// 0x08112178  size=148  FUN_08112178  status=translated  naming=inferred
// naming: script method table entry "showtoplayer" at 0x081888bc points to this function; the function does not reference the string.
void ScriptEntCmd_showtoplayer(uint32 entref)
{
    struct gentity_s *ent;
    struct gentity_s *player;

    ent = Ent105(entref);
    player = (struct gentity_s *)Scr_GetEntityParam(0);
    if (player->s.number < 0x40) {
        ent->flags = ent->flags & 0xfffff7ff;
        ent->r.clientMask[player->s.number >> 5] =
            ~(1u << ((uint8)player->s.number & 0x1f)) & ent->r.clientMask[player->s.number >> 5];
    } else {
        Scr_Error("showToClient error: param must be a client entity\n");
    }
}

// 0x0811220c  size=77  FUN_0811220c  status=translated  naming=inferred
// naming: script method table entry "setcontents" at 0x081888c8 points to this function; the function does not reference the string.
void ScriptEntCmd_setcontents(uint32 entref)
{
    struct gentity_s *ent;
    int contents;

    ent = Ent105(entref);
    contents = Scr_GetInt(0);
    Scr_AddIndex(ent->r.contents);
    ent->r.contents = contents;
    SV_LinkEntity((struct gentity_s *)ent);
}

// 0x0811225a  size=114  FUN_0811225a  status=translated  naming=inferred
// naming: script method table entry "setstablemissile" at 0x08188808 points to this function; the function does not reference the string.
void ScriptEntCmd_setstablemissile(uint32 entref)
{
    struct gentity_s *ent;
    int enable;

    ent = Ent105(entref);
    enable = Scr_GetInt(0);
    if (ent->s.eType != 1) {
        Scr_Error("Type should be a player");
    }
    if (enable == 0) {
        ent->flags = ent->flags & 0xfffdffff;
    } else {
        ent->flags = ent->flags | 0x20000;
    }
}

// 0x081122cc  size=357  FUN_081122cc  status=translated  naming=inferred
// naming: script method table entry "setcursorhint" at 0x081888d4 points to this function; the function does not reference the string.
void ScriptEntCmd_setcursorhint(uint32 entref)
{
    struct gentity_s *ent;
    const char *hintName;
    int i;

    ent = Ent105(entref);
    hintName = Scr_GetString105(0);
    if ((ent->classname == scr_str_trigger_use || ent->classname == scr_str_trigger_use_touch) &&
        Q_stricmp(hintName, "HINT_INHERIT") == 0) {
        ent->s.dmgFlags = -1;
    } else {
        for (i = 1; i < 0x85 && g_cursorHintNames[i] != 0; i = i + 1) {
            if (Q_stricmp(hintName, g_cursorHintNames[i]) == 0) {
                ent->s.dmgFlags = i;
                return;
            }
        }
        Com_Printf("List of valid hint type strings\n");
        if (ent->classname == scr_str_trigger_use || ent->classname == scr_str_trigger_use_touch) {
            Com_Printf("HINT_INHERIT (for trigger_use or trigger_use_touch entities only)\n");
        }
        for (i = 1; i < 0x85 && g_cursorHintNames[i] != 0; i = i + 1) {
            Com_Printf("%s\n", g_cursorHintNames[i]);
        }
        Scr_Error(va("%s is not a valid hint type. See above for list of valid hint types\n", hintName));
    }
}

// 0x081124f6  size=259  FUN_081124f6  status=translated  naming=inferred
// naming: script method table entry "sethintstring" at 0x081888e0 points to this function; the function does not reference the string. Note: G_FindOrAddHintString writes into a 12-byte local; only its first byte is read back.
void ScriptEntCmd_sethintstring(uint32 entref)
{
    struct gentity_s *ent;
    int paramType;
    int numParams;
    char text[0x400];
    uint8 hintIndex[12];

    ent = Ent105(entref);
    if (ent->classname != scr_str_trigger_use && ent->classname != scr_str_trigger_use_touch) {
        Scr_Error("The setHintString command only works on trigger_use or trigger_use_touch entities.\n");
    }
    paramType = (int)Scr_GetParamType(0);
    if (paramType == 2) {
        if (Q_stricmp(Scr_GetString105(0), "") == 0) {
            ent->s.scale = 0xff;
            return;
        }
    }
    numParams = Scr_GetNumParams();
    GScr_BuildMessageString(0, numParams - 1, "Hint String", text, 0x400);
    if (G_FindOrAddHintString((int *)hintIndex, text) == 0) {
        Scr_Error(va("Too many different hintstring values. Max allowed is %i different strings", 0x20));
    }
    ent->s.scale = (uint32)hintIndex[0];
}

// 0x081125fa  size=35  FUN_081125fa  status=translated  naming=inferred
// naming: script method table entry "getentitynumber" at 0x0818894c points to this function; the function does not reference the string.
void ScriptEntCmd_getentitynumber(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    Scr_AddIndex(ent->s.number);
}

// 0x0811261e  size=77  FUN_0811261e  status=translated  naming=inferred
// naming: script method table entry "enablegrenadetouchdamage" at 0x08188958 points to this function; the function does not reference the string.
void ScriptEntCmd_enablegrenadetouchdamage(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    if (ent->classname != scrStr_trigger_damage) {
        Scr_Error("Currently on supported on damage triggers");
    }
    ent->flags = ent->flags | 0x4000;
}

// 0x0811266c  size=77  FUN_0811266c  status=translated  naming=inferred
// naming: script method table entry "disablegrenadetouchdamage" at 0x08188964 points to this function; the function does not reference the string.
void ScriptEntCmd_disablegrenadetouchdamage(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    if (ent->classname != scrStr_trigger_damage) {
        Scr_Error("Currently on supported on damage triggers");
    }
    ent->flags = ent->flags & 0xffffbfff;
}

// 0x081126ba  size=45  FUN_081126ba  status=translated  naming=inferred
// naming: script method table entry "enablegrenadebounce" at 0x08188970 points to this function; the function does not reference the string.
void ScriptEntCmd_enablegrenadebounce(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    ent->flags = ent->flags & 0xffff7fff;
}

// 0x081126e8  size=45  FUN_081126e8  status=translated  naming=inferred
// naming: script method table entry "disablegrenadebounce" at 0x0818897c points to this function; the function does not reference the string.
void ScriptEntCmd_disablegrenadebounce(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    ent->flags = ent->flags | 0x8000;
}

// 0x08112716  size=64  FUN_08112716  status=translated  naming=inferred
// naming: script method table entry "enableaimassist" at 0x08188988 points to this function; the function does not reference the string.
void ScriptEntCmd_enableaimassist(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    if (ent->r.bmodel == '\0') {
        Scr_Error("Currently only supported on entities with brush models");
    }
    ent->s.eFlags = ent->s.eFlags | 0x800;
}

// 0x08112756  size=64  FUN_08112756  status=translated  naming=inferred
// naming: script method table entry "disableaimassist" at 0x08188994 points to this function; the function does not reference the string.
void ScriptEntCmd_disableaimassist(uint32 entref)
{
    struct gentity_s *ent;

    ent = Ent105(entref);
    if (ent->r.bmodel == '\0') {
        Scr_Error("Currently only supported on entities with brush models");
    }
    ent->s.eFlags = ent->s.eFlags & 0xfffff7ff;
}

// 0x0811299c  size=417  FUN_0811299c  status=translated  naming=inferred
// naming: script function table entry "objective_add" at 0x08188194 points to this function; the function does not reference the string. Note: Index range check is shared via CheckObjectiveIndex105; the binary inlines it with the same Scr_ErrorAtParam call.
void GScr_objective_add(void)
{
    int numParams;
    int idx;
    struct objective_s *obj;
    uint16 stateHandle;
    int state;

    numParams = Scr_GetNumParams();
    if (numParams < 2) {
        Scr_Error("objective_add needs at least the first two parameters out of its parameter list of: index state [string] [position]\n");
    }
    idx = Scr_GetInt(0);
    CheckObjectiveIndex105(idx, 0);
    obj = &level_objectives[idx];
    G_ClearObjectiveEntity((struct objective_s *)obj);
    stateHandle = (uint16)Scr_GetAny(1);
    if (G_ScriptStringToIndex(&state, stateHandle) == 0) {
        Scr_ErrorAtParam(1, va("Illegal objective state \"%s\". Valid states are \"empty\", \"invisible\", \"current\"\n",
                               ScrStr_TextFromHandle(stateHandle)));
    }
    obj->state = state;
    if (2 < numParams) {
        GetVec105(2, obj->origin);
        obj->origin[0] = (float)(int)obj->origin[0];
        obj->origin[1] = (float)(int)obj->origin[1];
        obj->origin[2] = (float)(int)obj->origin[2];
        obj->entNum = 0x3ff;
        if (3 < numParams) {
            G_SetObjectiveIcon((struct objective_s *)obj, 3);
        }
    }
    obj->teamNum = 0;
}

// 0x08112b3e  size=132  FUN_08112b3e  status=translated  naming=inferred
// naming: script function table entry "objective_delete" at 0x081881a0 points to this function; the function does not reference the string.
void GScr_objective_delete(void)
{
    int idx;

    idx = Scr_GetInt(0);
    CheckObjectiveIndex105(idx, 0);
    G_ClearObjectiveEntity((struct objective_s *)&level_objectives[idx]);
    G_ClearObjective((struct objective_s *)&level_objectives[idx]);
}

// 0x08112bc2  size=217  FUN_08112bc2  status=translated  naming=inferred
// naming: script function table entry "objective_state" at 0x081881ac points to this function; the function does not reference the string.
void GScr_objective_state(void)
{
    int idx;
    struct objective_s *obj;
    uint16 stateHandle;
    int state;

    idx = Scr_GetInt(0);
    CheckObjectiveIndex105(idx, 0);
    obj = &level_objectives[idx];
    stateHandle = (uint16)Scr_GetAny(1);
    if (G_ScriptStringToIndex(&state, stateHandle) == 0) {
        Scr_ErrorAtParam(1, va("Illegal objective state \"%s\". Valid states are \"empty\", \"invisible\", \"current\"\n",
                               Scr_GetString105(1)));
    }
    obj->state = state;
    if (state == 0 || state == 2) {
        G_ClearObjectiveEntity((struct objective_s *)obj);
    }
}

// 0x08112c9c  size=114  FUN_08112c9c  status=translated  naming=inferred
// naming: script function table entry "objective_icon" at 0x081881b8 points to this function; the function does not reference the string.
void GScr_objective_icon(void)
{
    int idx;

    idx = Scr_GetInt(0);
    CheckObjectiveIndex105(idx, 0);
    G_SetObjectiveIcon((struct objective_s *)&level_objectives[idx], 1);
}

// 0x08112d0e  size=245  FUN_08112d0e  status=translated  naming=inferred
// naming: script function table entry "objective_position" at 0x081881c4 points to this function; the function does not reference the string.
void GScr_objective_position(void)
{
    int idx;
    struct objective_s *obj;

    idx = Scr_GetInt(0);
    CheckObjectiveIndex105(idx, 0);
    obj = &level_objectives[idx];
    G_ClearObjectiveEntity((struct objective_s *)obj);
    GetVec105(1, obj->origin);
    obj->origin[0] = (float)(int)obj->origin[0];
    obj->origin[1] = (float)(int)obj->origin[1];
    obj->origin[2] = (float)(int)obj->origin[2];
}

// 0x08112e04  size=159  FUN_08112e04  status=translated  naming=inferred
// naming: script function table entry "objective_onentity" at 0x081881d0 points to this function; the function does not reference the string.
void GScr_objective_onentity(void)
{
    int idx;
    struct gentity_s *ent;

    idx = Scr_GetInt(0);
    CheckObjectiveIndex105(idx, 0);
    G_ClearObjectiveEntity((struct objective_s *)&level_objectives[idx]);
    ent = (struct gentity_s *)Scr_GetEntityParam(1);
    ent->r.svFlags = ent->r.svFlags | 0x10;
    level_objectives[idx].entNum = ent->s.number;
}

// 0x08112ea4  size=229  FUN_08112ea4  status=translated  naming=inferred
// naming: script function table entry "objective_current" at 0x081881dc points to this function; the function does not reference the string. Note: The original stack buffer is 17 ints with only the first 16 cleared and used.
void GScr_objective_current(void)
{
    int selected[16];
    int numParams;
    int i;
    int idx;
    struct objective_s *obj;

    numParams = Scr_GetNumParams();
    memset(selected, 0, sizeof(selected));
    for (i = 0; i < numParams; i = i + 1) {
        idx = Scr_GetInt(i);
        CheckObjectiveIndex105(idx, i);
        selected[idx] = 1;
    }
    for (idx = 0; idx < 0x10; idx = idx + 1) {
        obj = &level_objectives[idx];
        if (selected[idx] == 0) {
            if (obj->state == 4) {
                obj->state = 1;
            }
        } else {
            obj->state = 4;
        }
    }
}

// 0x08112f8a  size=236  FUN_08112f8a  status=translated  naming=inferred
// naming: script function table entry "objective_team" at 0x0818853c points to this function; the function does not reference the string.
void GScr_objective_team(void)
{
    int idx;
    uint16 teamHandle;

    idx = Scr_GetInt(0);
    CheckObjectiveIndex105(idx, 0);
    teamHandle = (uint16)Scr_GetAny(1);
    if (teamHandle == scr_str_allies) {
        level_objectives[idx].teamNum = 2;
    } else if (teamHandle == scr_str_axis) {
        level_objectives[idx].teamNum = 1;
    } else if (teamHandle == scr_str_none) {
        level_objectives[idx].teamNum = 0;
    } else {
        Scr_ErrorAtParam(1, va("Illegal team string '%s'. Must be allies, axis, or none.",
                               ScrStr_TextFromHandle(teamHandle)));
    }
}

// 0x08113076  size=158  FUN_08113076  status=translated  naming=inferred
// naming: script function table entry "logprint" at 0x08188548 points to this function; the function does not reference the string.
void GScr_logprint(void)
{
    char text[0x400];
    int total;
    int numParams;
    int i;
    const char *piece;
    size_t len;

    text[0] = 0;
    total = 0;
    numParams = Scr_GetNumParams();
    for (i = 0; i < numParams; i = i + 1) {
        piece = Scr_GetString105(i);
        len = strlen(piece);
        if (0x3ff < (int)(len + total)) {
            break;
        }
        I_strncat(text, 0x400, piece);
        total = total + len;
    }
    G_LogPrintf(text);
}

// 0x08113114  size=20  FUN_08113114  status=translated  naming=inferred
// naming: script function table entry "worldentnumber" at 0x08188554 points to this function; the function does not reference the string.
void GScr_worldentnumber(void)
{
    Scr_AddIndex(0x3fe);
}

// 0x08113128  size=260  FUN_08113128  status=translated  naming=inferred
// naming: script function table entry "obituary" at 0x08188560 points to this function; the function does not reference the string. Note: G_LoadWeaponDef (weapon name to index) is also used by chunk 103's switchtooffhand; G_LoadWeaponDef has no merged name.
void GScr_obituary(void)
{
    int weapon;
    uint32 meansOfDeath;
    struct gentity_s *victim;
    struct gentity_s *attacker;
    struct gentity_s *temp;

    weapon = G_LoadWeaponDef(Scr_GetString105(2));
    meansOfDeath = (uint32)G_MeansOfDeathFromString(Scr_GetString105(3));
    victim = (struct gentity_s *)Scr_GetEntityParam(0);
    temp = (struct gentity_s *)G_TempEntity(vec3_origin, 0xc6);
    temp->s.otherEntityNum = victim->s.number;
    if (Scr_GetParamType(1) == 1 && Scr_GetPointer(1) == 0x15) {
        attacker = (struct gentity_s *)Scr_GetEntityParam(1);
        temp->s.attackerEntityNum = attacker->s.number;
    } else {
        temp->s.attackerEntityNum = 0x3fe;
    }
    temp->r.svFlags = 8;
    if (meansOfDeath == 7 || meansOfDeath == 8 || meansOfDeath == 0xc || meansOfDeath == 0xb || meansOfDeath == 9) {
        temp->s.eventParm = meansOfDeath | 0x80;
    } else {
        temp->s.eventParm = (uint32)weapon;
    }
}

// 0x0811322c  size=264  FUN_0811322c  status=translated  naming=inferred
// naming: script function table entry "positionwouldtelefrag" at 0x0818856c points to this function; the function does not reference the string.
void GScr_positionwouldtelefrag(void)
{
    float pos[3];
    float mins[3];
    float maxs[3];
    int list[0x400];
    int count;
    int i;
    struct gentity_s *ent;

    GetVec105(0, pos);
    Vec3_Add_081186c8(pos, g_spawnTraceMins, mins);
    Vec3_Add_081186c8(pos, g_spawnTraceMaxs, maxs);
    count = CM_AreaEntities(mins, maxs, list, 0x400, 0x2000000);
    for (i = 0; i < count; i = i + 1) {
        ent = &g_gentities[list[i]];
        if (ent->client != 0 && ent->client->ps.pm_type < 6) {
            Scr_AddIndex(1);
            return;
        }
    }
    Scr_AddIndex(0);
}

// 0x08113334  size=21  FUN_08113334  status=translated  naming=inferred
// naming: script function table entry "getstarttime" at 0x08188578 points to this function; the function does not reference the string.
void GScr_getstarttime(void)
{
    Scr_AddIndex(level_startTime);
}

// 0x0811334a  size=255  FUN_0811334a  status=translated  naming=inferred
// naming: script function table entry "precachemenu" at 0x08188584 points to this function; the function does not reference the string.
void GScr_precachemenu(void)
{
    const char *menuName;
    char current[0x408];
    int i;

    menuName = Scr_GetString105(0);
    for (i = 0; i < 0x20; i = i + 1) {
        SV_GetConfigstring(i + 0x4de, current, 0x400);
        if (Q_stricmp(current, menuName) == 0) {
            Com_DPrintf("Script tried to precache the menu '%s' more than once\n", menuName);
            return;
        }
    }
    i = 0;
    while (i < 0x20) {
        SV_GetConfigstring(i + 0x4de, current, 0x400);
        if (current[0] == '\0') {
            break;
        }
        i = i + 1;
    }
    if (i == 0x20) {
        Scr_Error(va("Too many menus precached. Max allowed menus is %i", 0x20));
    }
    SV_SetConfigstring(i + 0x4de, (char *)menuName);
}

// 0x08113a4a  size=194  FUN_08113a4a  status=translated  naming=inferred
// naming: script function table entry "bullettracepassed" at 0x081881f4 points to this function; the function does not reference the string.
void GScr_bullettracepassed(void)
{
    float start[3];
    float end[3];
    int ignoreEntNum;
    uint32 contentMask;

    ignoreEntNum = 0x3ff;
    contentMask = 0x2802831;
    GetVec105(0, start);
    GetVec105(1, end);
    if (Scr_GetInt(2) == 0) {
        contentMask = 0x802831;
    }
    if (Scr_GetParamType(3) == 1 && Scr_GetPointer(3) == 0x15) {
        ignoreEntNum = ((struct gentity_s *)Scr_GetEntityParam(3))->s.number;
    }
    Scr_AddInt(G_TraceIsClearPoint(start, end, ignoreEntNum, (int)contentMask));
}

// 0x08113b0c  size=257  FUN_08113b0c  status=translated  naming=inferred
// naming: script function table entry "sighttracepassed" at 0x08188200 points to this function; the function does not reference the string. Note: FX_GetAttenuationThunk is the merged name of 0x08090afa, but the binary uses it as a float-returning (start,end) distance helper; called through a cast.
void GScr_sighttracepassed(void)
{
    float start[3];
    float end[3];
    int ignoreEntNum;
    uint32 contentMask;
    int hit;
    float dist;

    ignoreEntNum = 0x3ff;
    contentMask = 0x2801803;
    GetVec105(0, start);
    GetVec105(1, end);
    if (Scr_GetInt(2) == 0) {
        contentMask = contentMask & 0xfdffffff;
    }
    if (Scr_GetParamType(3) == 1 && Scr_GetPointer(3) == 0x15) {
        ignoreEntNum = ((struct gentity_s *)Scr_GetEntityParam(3))->s.number;
    }
    G_TraceAccumulateHit(&hit, start, end, ignoreEntNum, contentMask);
    if (hit == 0) {
        dist = ((float (*)(const float *, const float *))FX_GetAttenuationThunk)(start, end);
        if (dist < 0.2f) {
            hit = 1;
        }
    }
    Scr_AddInt(hit == 0);
}

// 0x08113c0e  size=149  FUN_08113c0e  status=translated  naming=inferred
// naming: script function table entry "physicstrace" at 0x0818820c points to this function; the function does not reference the string.
void GScr_physicstrace(void)
{
    float start[3];
    float end[3];
    float hitPos[3];
    struct trace_s trace;

    GetVec105(0, start);
    GetVec105(1, end);
    G_TraceBox(&trace, start, vec3_origin, vec3_origin, end, 0x3ff, 0x820011);
    VectorLerp(start, end, trace.fraction, hitPos);
    AddVec105(hitPos);
}

// 0x08113ca4  size=89  FUN_08113ca4  status=translated  naming=inferred
// naming: script function table entry "randomint" at 0x0818823c points to this function; the function does not reference the string.
void GScr_randomint(void)
{
    int limit;

    limit = Scr_GetInt(0);
    if (limit < 1) {
        Com_Printf("RandomInt parm: %d  ", limit);
        Scr_Error("RandomInt parm must be positive integer.\n");
    } else {
        Scr_AddIndex(Rand_Int(0, limit));
    }
}

// 0x08113cfe  size=51  FUN_08113cfe  status=translated  naming=inferred
// naming: script function table entry "randomfloat" at 0x08188248 points to this function; the function does not reference the string.
void GScr_randomfloat(void)
{
    Scr_AddFloat105(Rand_Float(0.0f, Scr_GetFloat(0)));
}

// 0x08113d32  size=110  FUN_08113d32  status=translated  naming=inferred
// naming: script function table entry "randomintrange" at 0x08188254 points to this function; the function does not reference the string.
void GScr_randomintrange(void)
{
    int lo;
    int hi;

    lo = Scr_GetInt(0);
    hi = Scr_GetInt(1);
    if (hi <= lo) {
        Com_Printf("RandomIntRange parms: %d %d ", lo, hi);
        Scr_Error("RandomIntRange range must be positive integer.\n");
    }
    Scr_AddIndex(Rand_Int(lo, hi));
}

// 0x08113da0  size=117  FUN_08113da0  status=translated  naming=inferred
// naming: script function table entry "randomfloatrange" at 0x08188260 points to this function; the function does not reference the string. Note: The first message's format string uses %d with double arguments, as in the binary.
void GScr_randomfloatrange(void)
{
    float lo;
    float hi;

    lo = Scr_GetFloat(0);
    hi = Scr_GetFloat(1);
    if (hi <= lo) {
        Com_Printf("Scr_RandomFloatRange parms: %d %d ", (double)lo, (double)hi);
        Scr_Error("Scr_RandomFloatRange range must be positive float.\n");
    }
    Scr_AddFloat105(Rand_Float(lo, hi));
}

// 0x08113e16  size=50  FUN_08113e16  status=translated  naming=inferred
// naming: script function table entry "sin" at 0x0818826c points to this function; the function does not reference the string.
void GScr_sin(void)
{
    double r;

    r = sin(0.017453292519943295 * (double)Scr_GetFloat(0));
    Scr_AddFloat105((float)r);
}

// 0x08113e48  size=50  FUN_08113e48  status=translated  naming=inferred
// naming: script function table entry "cos" at 0x08188278 points to this function; the function does not reference the string.
void GScr_cos(void)
{
    double r;

    r = cos(0.017453292519943295 * (double)Scr_GetFloat(0));
    Scr_AddFloat105((float)r);
}

// 0x08113e7a  size=100  FUN_08113e7a  status=translated  naming=inferred
// naming: script function table entry "tan" at 0x08188284 points to this function; the function does not reference the string.
void GScr_tan(void)
{
    float sinValue;
    float cosValue;

    Float_SinCos((float)(0.017453292519943295 * (double)Scr_GetFloat(0)), &sinValue, &cosValue);
    if (cosValue == 0.0f) {
        Scr_Error("divide by 0");
    }
    Scr_AddFloat105(sinValue / cosValue);
}

// 0x08113ede  size=121  FUN_08113ede  status=translated  naming=inferred
// naming: script function table entry "asin" at 0x08188290 points to this function; the function does not reference the string.
void GScr_asin(void)
{
    float x;
    float r;

    x = Scr_GetFloat(0);
    if (x < -1.0f || 1.0f < x) {
        Scr_Error(va("%g out of range", (double)x));
    }
    r = (float)asin((double)x);
    Scr_AddFloat105((float)((double)r * 57.29577951308232));
}

// 0x08113f58  size=121  FUN_08113f58  status=translated  naming=inferred
// naming: script function table entry "acos" at 0x0818829c points to this function; the function does not reference the string.
void GScr_acos(void)
{
    float x;
    float r;

    x = Scr_GetFloat(0);
    if (x < -1.0f || 1.0f < x) {
        Scr_Error(va("%g out of range", (double)x));
    }
    r = (float)acos((double)x);
    Scr_AddFloat105((float)((double)r * 57.29577951308232));
}

// 0x08113fd2  size=56  FUN_08113fd2  status=translated  naming=inferred
// naming: script function table entry "atan" at 0x081882a8 points to this function; the function does not reference the string.
void GScr_atan(void)
{
    float r;

    r = (float)atan((double)Scr_GetFloat(0));
    Scr_AddFloat105((float)((double)r * 57.29577951308232));
}

// 0x0811400a  size=196  FUN_0811400a  status=translated  naming=inferred
// naming: script function table entry "int" at 0x081882b4 points to this function; the function does not reference the string.
void GScr_int(void)
{
    uint32 type;

    type = Scr_GetParamType(0);
    if (type == 5) {
        Scr_AddIndex((int)Scr_GetFloat(0));
    } else {
        if (type < 6) {
            if (type == 2) {
                Scr_AddIndex(atoi(Scr_GetString105(0)));
                return;
            }
        } else if (type == 6) {
            Scr_AddIndex(Scr_GetInt(0));
            return;
        }
        Scr_ErrorAtParam(0, va("cannot cast %s to int", Scr_GetParamTypeName(0)));
    }
}

// 0x081140ce  size=72  FUN_081140ce  status=translated  naming=inferred
// naming: script function table entry "distance" at 0x081882c0 points to this function; the function does not reference the string. Note: Vec3_Distance is the merged name of 0x080a40ea; it returns a float distance here, called through a cast.
void GScr_distance(void)
{
    float a[3];
    float b[3];

    GetVec105(0, a);
    GetVec105(1, b);
    Scr_AddFloat105(((float (*)(const float *, const float *))Vec3_Distance)(a, b));
}

// 0x08114116  size=72  FUN_08114116  status=translated  naming=inferred
// naming: script function table entry "distancesquared" at 0x081882cc points to this function; the function does not reference the string.
void GScr_distancesquared(void)
{
    float a[3];
    float b[3];

    GetVec105(0, a);
    GetVec105(1, b);
    Scr_AddFloat105(Vec3_DistanceSquared(a, b));
}

// 0x0811415e  size=46  FUN_0811415e  status=translated  naming=inferred
// naming: script function table entry "length" at 0x081882d8 points to this function; the function does not reference the string.
void GScr_length(void)
{
    float v[3];

    GetVec105(0, v);
    Scr_AddFloat105(Vec3_Length_081187f0(v));
}

// 0x0811418c  size=46  FUN_0811418c  status=translated  naming=inferred
// naming: script function table entry "lengthsquared" at 0x081882e4 points to this function; the function does not reference the string.
void GScr_lengthsquared(void)
{
    float v[3];

    GetVec105(0, v);
    Scr_AddFloat105(Vec3_LengthSquared_081187bc(v));
}

// 0x081141ba  size=134  FUN_081141ba  status=translated  naming=inferred
// naming: script function table entry "closer" at 0x081882f0 points to this function; the function does not reference the string.
void GScr_closer(void)
{
    float origin[3];
    float a[3];
    float b[3];
    float distA;
    float distB;

    GetVec105(0, origin);
    GetVec105(1, a);
    GetVec105(2, b);
    distA = Vec3_DistanceSquared(a, origin);
    distB = Vec3_DistanceSquared(b, origin);
    Scr_AddIndex(distA < distB);
}

// 0x08114240  size=72  FUN_08114240  status=translated  naming=inferred
// naming: script function table entry "vectordot" at 0x081882fc points to this function; the function does not reference the string.
void GScr_vectordot(void)
{
    float a[3];
    float b[3];

    GetVec105(0, a);
    GetVec105(1, b);
    Scr_AddFloat105(Vec3_Dot_08118788(a, b));
}

// 0x08114288  size=69  FUN_08114288  status=translated  naming=inferred
// naming: script function table entry "vectornormalize" at 0x08188308 points to this function; the function does not reference the string.
void GScr_vectornormalize(void)
{
    float in[3];
    float out[3];

    GetVec105(0, in);
    Vec3_Copy_08118698((const uint32 *)in, (uint32 *)out);
    Vec3_Normalize(out);
    AddVec105(out);
}

// 0x081142ce  size=56  FUN_081142ce  status=translated  naming=inferred
// naming: script function table entry "vectortoangles" at 0x08188314 points to this function; the function does not reference the string.
void GScr_vectortoangles(void)
{
    float v[3];
    float angles[3];

    GetVec105(0, v);
    Vec3_ToAngles(v, angles);
    AddVec105(angles);
}

// 0x08114306  size=72  FUN_08114306  status=translated  naming=inferred
// naming: script function table entry "anglestoup" at 0x08188320 points to this function; the function does not reference the string.
void GScr_anglestoup(void)
{
    float angles[3];
    float up[3];

    GetVec105(0, angles);
    Angles_ToVectors(angles, 0, 0, up);
    AddVec105(up);
}

// 0x0811434e  size=72  FUN_0811434e  status=translated  naming=inferred
// naming: script function table entry "anglestoright" at 0x0818832c points to this function; the function does not reference the string.
void GScr_anglestoright(void)
{
    float angles[3];
    float right[3];

    GetVec105(0, angles);
    Angles_ToVectors(angles, 0, right, 0);
    AddVec105(right);
}

// 0x08114396  size=72  FUN_08114396  status=translated  naming=inferred
// naming: script function table entry "anglestoforward" at 0x08188338 points to this function; the function does not reference the string.
void GScr_anglestoforward(void)
{
    float angles[3];
    float forward[3];

    GetVec105(0, angles);
    Angles_ToVectors(angles, forward, 0, 0);
    AddVec105(forward);
}

// 0x081143de  size=67  FUN_081143de  status=translated  naming=inferred
// naming: script function table entry "issubstr" at 0x08188344 points to this function; the function does not reference the string.
void GScr_issubstr(void)
{
    const char *needle;
    const char *haystack;

    needle = Scr_GetString105(1);
    haystack = Scr_GetString105(0);
    Scr_AddInt(strstr(haystack, needle) != 0);
}

// 0x08114422  size=263  FUN_08114422  status=translated  naming=inferred
// naming: script function table entry "getsubstr" at 0x08188350 points to this function; the function does not reference the string.
void GScr_getsubstr(void)
{
    const char *src;
    int start;
    int end;
    int len;
    char buf[0x408];

    src = Scr_GetString105(0);
    start = Scr_GetInt(1);
    if ((uint32)Scr_GetNumParams() < 3) {
        end = 0x7fffffff;
    } else {
        end = Scr_GetInt(2);
    }
    len = 0;
    for (; start < end; start = start + 1) {
        if (0x3ff < len) {
            Scr_Error("string too long");
        }
        if (src[start] == '\0') {
            break;
        }
        buf[len] = src[start];
        len = len + 1;
    }
    buf[len] = '\0';
    Scr_AddString(buf);
}

// 0x0811452a  size=154  FUN_0811452a  status=translated  naming=inferred
// naming: script function table entry "tolower" at 0x0818835c points to this function; the function does not reference the string.
void GScr_tolower(void)
{
    const char *src;
    int len;
    char c;
    char buf[0x408];

    src = Scr_GetString105(0);
    len = 0;
    while (true) {
        if (0x3ff < len) {
            Scr_Error("string too long");
            return;
        }
        c = (char)tolower((int)*src);
        buf[len] = c;
        if (c == '\0') {
            break;
        }
        len = len + 1;
        src = src + 1;
    }
    Scr_AddString(buf);
}

// 0x081145c4  size=367  FUN_081145c4  status=translated  naming=inferred
// naming: script function table entry "strtok" at 0x08188368 points to this function; the function does not reference the string. Note: Scr_AddEntity/Scr_PopAndLink are the merged names of 0x08085338/0x08085364; here they behave as array-begin and array-append.
void GScr_strtok(void)
{
    const char *src;
    const char *delims;
    size_t delimCount;
    int len;
    int pos;
    int d;
    char c;
    char buf[0x408];

    src = Scr_GetString105(0);
    delims = Scr_GetString105(1);
    delimCount = strlen(delims);
    len = 0;
    Scr_AddEntity();
    pos = 0;
    while (true) {
        c = src[pos];
        if (c == '\0') {
            if (len != 0) {
                buf[len] = '\0';
                Scr_AddString(buf);
                Scr_PopAndLink();
            }
            return;
        }
        for (d = 0; d < (int)delimCount; d = d + 1) {
            if (c == delims[d]) {
                if (len != 0) {
                    buf[len] = '\0';
                    Scr_AddString(buf);
                    Scr_PopAndLink();
                    len = 0;
                }
                goto next_char;
            }
        }
        buf[len] = c;
        len = len + 1;
        if (0x3ff < len) {
            Scr_Error("string too long");
        }
    next_char:
        pos = pos + 1;
    }
}

// 0x08114734  size=68  FUN_08114734  status=translated  naming=inferred
// naming: script function table entry "musicplay" at 0x08188374 points to this function; the function does not reference the string.
void GScr_musicplay(void)
{
    SV_SendServerCommandNum(-1, 1, va("%c %s", 0x6f, Scr_GetString105(0)));
}

// 0x08114778  size=171  FUN_08114778  status=translated  naming=inferred
// naming: script function table entry "musicstop" at 0x08188380 points to this function; the function does not reference the string.
void GScr_musicstop(void)
{
    int numParams;
    int fadeTime;

    numParams = Scr_GetNumParams();
    if (numParams == 0) {
        fadeTime = 0;
    } else {
        if (numParams != 1) {
            Scr_Error(va("USAGE: musicStop([fadetime]);\n"));
            return;
        }
        fadeTime = Float_RoundHalfUp((float)(1000.0 * (double)Scr_GetFloat(0)));
    }
    if (fadeTime < 0) {
        Scr_Error(va("musicStop: fade time must be >= 0\n"));
    }
    SV_SendServerCommandNum(-1, 1, va("%c %i", 0x70, fadeTime));
}

// 0x08114824  size=144  FUN_08114824  status=translated  naming=inferred
// naming: script function table entry "soundfade" at 0x0818838c points to this function; the function does not reference the string.
void GScr_soundfade(void)
{
    float volume;
    int fadeTime;

    volume = Scr_GetFloat(0);
    if ((uint32)Scr_GetNumParams() < 2) {
        fadeTime = 0;
    } else {
        fadeTime = (int)(1000.0 * (double)Scr_GetFloat(1));
    }
    SV_SendServerCommandNum(-1, 1, va("%c %f %i\n", 0x71, (double)volume, fadeTime));
}

// 0x081148b4  size=49  FUN_081148b4  status=translated  naming=inferred
// naming: script function table entry "precachemodel" at 0x081883b0 points to this function; the function does not reference the string.
void GScr_precachemodel(void)
{
    if (level_initializing == 0) {
        Scr_Error("precacheModel must be called before any wait statements in the gametype or level script\n");
    }
    G_ModelIndex(Scr_GetString105(0));
}

// 0x081148e6  size=49  FUN_081148e6  status=translated  naming=inferred
// naming: script function table entry "precacheshellshock" at 0x081883bc points to this function; the function does not reference the string. Note: Callee is merged as G_ShellShockIndex (configstring base 0x48e, 0x10 slots); shell-shock precache semantic is not confirmed.
void GScr_precacheshellshock(void)
{
    if (level_initializing == 0) {
        Scr_Error("precacheShellShock must be called before any wait statements in the gametype or level script\n");
    }
    G_ShellShockIndex(Scr_GetString105(0));
}

// 0x08114918  size=138  FUN_08114918  status=translated  naming=inferred
// naming: script function table entry "precacheitem" at 0x081883c8 points to this function; the function does not reference the string.
void GScr_precacheitem(void)
{
    const char *itemName;
    struct gitem_s *item;

    if (level_initializing == 0) {
        Scr_Error("precacheItem must be called before any wait statements in the gametype or level script\n");
    }
    itemName = Scr_GetString105(0);
    item = BG_FindItemByName(itemName);
    if (item == 0) {
        Scr_ErrorAtParam(0, va("unknown item '%s'", itemName));
    }
    ItemRegistry_Register((int)(((uint8 *)item - (uint8 *)g_081850c0) / 0x2c), 1);
}

// 0x081149a2  size=83  FUN_081149a2  status=translated  naming=inferred
// naming: script function table entry "precacheshader" at 0x081883d4 points to this function; the function does not reference the string.
void GScr_precacheshader(void)
{
    const char *shaderName;

    if (level_initializing == 0) {
        Scr_Error("precacheShader must be called before any wait statements in the gametype or level script\n");
    }
    shaderName = Scr_GetString105(0);
    if (*shaderName == '\0') {
        Scr_ErrorAtParam(0, "Shader name string is empty");
    }
    G_ShaderIndex(shaderName);
}

// 0x081149f6  size=63  FUN_081149f6  status=translated  naming=inferred
// naming: script function table entry "precachestring" at 0x081883e0 points to this function; the function does not reference the string.
void GScr_precachestring(void)
{
    const char *text;

    if (level_initializing == 0) {
        Scr_Error("precacheString must be called before any wait statements in the gametype or level script\n");
    }
    text = Scr_GetLocalized105(0);
    if (*text != '\0') {
        G_LocalizedStringIndex(text);
    }
}

// 0x08114a3c  size=216  FUN_08114a3c  status=translated  naming=inferred
// naming: script function table entry "ambientplay" at 0x08188398 points to this function; the function does not reference the string.
void GScr_ambientplay(void)
{
    int fadeTime;
    int numParams;
    const char *alias;

    fadeTime = 0;
    numParams = Scr_GetNumParams();
    if (numParams != 1) {
        if (numParams != 2) {
            Scr_Error(va("USAGE: ambientPlay(alias_name, <fadetime>);\n"));
            return;
        }
        fadeTime = Float_RoundHalfUp((float)(1000.0 * (double)Scr_GetFloat(1)));
    }
    alias = Scr_GetString105(0);
    if (*alias == '\0') {
        Scr_Error(va("ambientPlay: alias name cannot be the empty string... use stop or fade version\n"));
    }
    if (fadeTime < 0) {
        Scr_Error(va("ambientPlay: fade time must be >= 0\n"));
    }
    SV_SetConfigstring(3, va("n\\%s\\t\\%i", alias, fadeTime + g_0864c56c));
}

// 0x08114b14  size=161  FUN_08114b14  status=translated  naming=inferred
// naming: script function table entry "ambientstop" at 0x081883a4 points to this function; the function does not reference the string.
void GScr_ambientstop(void)
{
    int numParams;
    int fadeTime;

    numParams = Scr_GetNumParams();
    if (numParams == 0) {
        fadeTime = 0;
    } else {
        if (numParams != 1) {
            Scr_Error(va("USAGE: ambientStop(<fadetime>);\n"));
            return;
        }
        fadeTime = Float_RoundHalfUp((float)(1000.0 * (double)Scr_GetFloat(0)));
    }
    if (fadeTime < 0) {
        Scr_Error(va("ambientStop: fade time must be >= 0\n"));
    }
    SV_SetConfigstring(3, va("t\\%i", fadeTime + g_0864c56c));
}
