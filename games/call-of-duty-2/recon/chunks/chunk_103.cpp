#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_103_types.h"

/* BG_GetWeaponDef is recorded with chunk 043's WeaponDef043_s layout; this chunk views the same record as weapondef103_s. */
static struct weaponDef_s *GetWeaponDef103(int weapon)
{
    return (struct weaponDef_s *)BG_GetWeaponDef(weapon);
}

/*
 * Chunk 103: GSC player method handlers recovered from .eh_frame FDE starts.
 * Every handler takes the script entity reference (entref) and validates it as a player first.
 * Naming evidence for all 48: the script-method table in .rodata (name string, function pointer,
 * flag; stride 0xc) lists each function address next to its script name. The functions do not
 * reference those strings themselves, so naming is "inferred". Prefix PlayerCmd_ follows the
 * Quake III / IW engine method naming lineage.
 */

// 0x080fa2ac  size=246  FUN_080fa2ac  status=translated  naming=inferred
// naming: method table entry "switchtooffhand" -> 0x080fa2ac; string "unknown weapon '%s'" referenced here.
void PlayerCmd_switchtooffhand(uint32 entref)
{
    struct gentity_s *ent;
    const char *weaponName;
    int weapon;
    uint8 owned;

    ent = Scr_GetPlayerEntity103(entref);
    weaponName = Scr_ParamText103(0);
    weapon = G_LoadWeaponDef(weaponName);
    if (weapon == 0) {
        Scr_ErrorAtParam(0, va("unknown weapon '%s'", weaponName));
    }
    owned = BitSet_Test(ent->client->ps.weapons, weapon);
    if (owned == 0) {
        Scr_AddInt(0);
    } else {
        G_SendCharIndexC((uint16)entref, weapon);
        Scr_AddInt(1);
    }
}

// 0x080fa3a2  size=252  FUN_080fa3a2  status=translated  naming=inferred
// naming: method table entry "givestartammo" -> 0x080fa3a2.
void PlayerCmd_givestartammo(uint32 entref)
{
    struct gentity_s *ent;
    int weapon;
    struct weaponDef_s *def;
    int amount;

    ent = Scr_GetPlayerEntity103(entref);
    weapon = G_LoadWeaponDef(Scr_ParamText103(0));
    if (BitSet_Test(ent->client->ps.weapons, weapon) != 0) {
        def = GetWeaponDef103(weapon);
        amount = def->startAmmo - ent->client->ps.ammo[def->iAmmoIndex];
        if (amount > 0) {
            Player_GiveWeaponAmmo(ent, weapon, amount, 0);
        }
    }
}

// 0x080fa49e  size=262  FUN_080fa49e  status=translated  naming=inferred
// naming: method table entry "givemaxammo" -> 0x080fa49e.
void PlayerCmd_givemaxammo(uint32 entref)
{
    struct gentity_s *ent;
    int weapon;
    struct weaponDef_s *def;
    int amount;

    ent = Scr_GetPlayerEntity103(entref);
    weapon = G_LoadWeaponDef(Scr_ParamText103(0));
    if (BitSet_Test(ent->client->ps.weapons, weapon) != 0) {
        def = GetWeaponDef103(weapon);
        amount = BG_GetAmmoTypeMax(def->iAmmoIndex) - ent->client->ps.ammo[def->iAmmoIndex];
        if (amount > 0) {
            Player_GiveWeaponAmmo(ent, weapon, amount, 0);
        }
    }
}

// 0x080fa5a4  size=313  FUN_080fa5a4  status=translated  naming=inferred
// naming: method table entry "getfractionstartammo" -> 0x080fa5a4.
void PlayerCmd_getfractionstartammo(uint32 entref)
{
    struct gentity_s *ent;
    int weapon;
    struct weaponDef_s *def;
    int have;

    ent = Scr_GetPlayerEntity103(entref);
    weapon = G_LoadWeaponDef(Scr_ParamText103(0));
    if (BitSet_Test(ent->client->ps.weapons, weapon) == 0) {
        Scr_AddFloatBits(FloatBits103(1.0f));
    } else {
        def = GetWeaponDef103(weapon);
        if (def->startAmmo < 1) {
            Scr_AddFloatBits(FloatBits103(1.0f));
        } else if (ent->client->ps.ammo[def->iAmmoIndex] < 1) {
            Scr_AddFloatBits(FloatBits103(0.0f));
        } else {
            have = ent->client->ps.ammo[def->iAmmoIndex];
            Scr_AddFloatBits(FloatBits103((float)have / (float)def->startAmmo));
        }
    }
}

// 0x080fa6de  size=345  FUN_080fa6de  status=translated  naming=inferred
// naming: method table entry "getfractionmaxammo" -> 0x080fa6de.
void PlayerCmd_getfractionmaxammo(uint32 entref)
{
    struct gentity_s *ent;
    int weapon;
    struct weaponDef_s *def;
    int have;
    int maxAmmo;

    ent = Scr_GetPlayerEntity103(entref);
    weapon = G_LoadWeaponDef(Scr_ParamText103(0));
    if (BitSet_Test(ent->client->ps.weapons, weapon) == 0) {
        Scr_AddFloatBits(FloatBits103(1.0f));
    } else {
        def = GetWeaponDef103(weapon);
        if (BG_GetAmmoTypeMax(def->iAmmoIndex) < 1) {
            Scr_AddFloatBits(FloatBits103(1.0f));
        } else if (ent->client->ps.ammo[def->iAmmoIndex] < 1) {
            Scr_AddFloatBits(FloatBits103(0.0f));
        } else {
            have = ent->client->ps.ammo[def->iAmmoIndex];
            maxAmmo = BG_GetAmmoTypeMax(def->iAmmoIndex);
            Scr_AddFloatBits(FloatBits103((float)have / (float)maxAmmo));
        }
    }
}

// 0x080fa838  size=299  FUN_080fa838  status=translated  naming=inferred
// naming: method table entry "setorigin" -> 0x080fa838.
void PlayerCmd_setorigin(uint32 entref)
{
    struct gentity_s *ent;
    vec3_t origin;

    ent = Scr_GetPlayerEntity103(entref);
    Scr_GetVec3_103(0, origin);
    SV_UnlinkEntity(ent);
    VectorCopy_080fde5a(origin, ent->client->ps.origin);
    ent->client->ps.origin[2] = ent->client->ps.origin[2] + 1.0f; /* client+0x1c */
    ent->client->ps.eFlags = ent->client->ps.eFlags ^ 2;
    BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, 1, 1);
    VectorCopy_080fde5a(ent->client->ps.origin, ent->r.currentOrigin);
    SV_LinkEntity(ent);
}

// 0x080fa964  size=135  FUN_080fa964  status=translated  naming=inferred
// naming: method table entry "setplayerangles" -> 0x080fa964.
void PlayerCmd_setplayerangles(uint32 entref)
{
    struct gentity_s *ent;
    vec3_t angles;

    ent = Scr_GetPlayerEntity103(entref);
    Scr_GetVec3_103(0, angles);
    SetClientViewAngle(ent, angles);
}

// 0x080fa9ec  size=120  FUN_080fa9ec  status=translated  naming=inferred
// naming: method table entry "getplayerangles" -> 0x080fa9ec.
void PlayerCmd_getplayerangles(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    Scr_AddVector(&ent->client->ps.viewangles[0]);
}

// 0x080faa64  size=161  FUN_080faa64  status=translated  naming=inferred
// naming: method table entry "usebuttonpressed" -> 0x080faa64.
void PlayerCmd_usebuttonpressed(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    if (((ent->client->buttons | ent->client->buttonsSinceLastFrame) & 0x28) == 0) {
        Scr_AddIndex(0);
    } else {
        Scr_AddIndex(1);
    }
}

// 0x080fab06  size=161  FUN_080fab06  status=translated  naming=inferred
// naming: method table entry "attackbuttonpressed" -> 0x080fab06.
void PlayerCmd_attackbuttonpressed(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    if ((((uint8)ent->client->buttons | (uint8)ent->client->buttonsSinceLastFrame) & 1) == 0) {
        Scr_AddIndex(0);
    } else {
        Scr_AddIndex(1);
    }
}

// 0x080faba8  size=161  FUN_080faba8  status=translated  naming=inferred
// naming: method table entry "meleebuttonpressed" -> 0x080faba8.
void PlayerCmd_meleebuttonpressed(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    if (((ent->client->buttons | ent->client->buttonsSinceLastFrame) & 4) == 0) {
        Scr_AddIndex(0);
    } else {
        Scr_AddIndex(1);
    }
}

// 0x080fac4a  size=121  FUN_080fac4a  status=translated  naming=inferred
// naming: method table entry "playerads" -> 0x080fac4a.
void PlayerCmd_playerads(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    Scr_AddFloatBits(FloatBits103(ent->client->ps.fWeaponPosFrac));
}

// 0x080facc4  size=142  FUN_080facc4  status=translated  naming=inferred
// naming: method table entry "isonground" -> 0x080facc4.
void PlayerCmd_isonground(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    if (ent->client->ps.groundEntityNum == 0x3ff) {
        Scr_AddIndex(0);
    } else {
        Scr_AddIndex(1);
    }
}

// 0x080fad52  size=158  FUN_080fad52  status=translated  naming=inferred
// naming: method table entry "pingplayer" -> 0x080fad52.
void PlayerCmd_pingplayer(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    ent->client->ps.eFlags = ent->client->ps.eFlags | 0x400000;
    ent->client->pingTime = level_time + 3000;
}

// 0x080fadf0  size=179  FUN_080fadf0  status=translated  naming=inferred
// naming: method table entry "setviewmodel" -> 0x080fadf0; string "usage: setviewmodel(<model name>)" referenced here.
void PlayerCmd_setviewmodel(uint32 entref)
{
    struct gentity_s *ent;
    const char *modelName;

    ent = Scr_GetPlayerEntity103(entref);
    modelName = Scr_ParamText103(0);
    if (modelName == 0 || *modelName == '\0') {
        Scr_ErrorAtParam(0, "usage: setviewmodel(<model name>)");
    }
    ent->client->viewmodelIndex = G_ModelIndex(modelName);
}

// 0x080faea4  size=135  FUN_080faea4  status=translated  naming=inferred
// naming: method table entry "getviewmodel" -> 0x080faea4.
void PlayerCmd_getviewmodel(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    Scr_AddString(G_GetModelName(ent->client->viewmodelIndex));
}

// 0x080faf2c  size=109  FUN_080faf2c  status=translated  naming=inferred
// naming: method table entry "showscoreboard" -> 0x080faf2c.
void PlayerCmd_showscoreboard(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    Cmd_Score_f(ent);
}

// 0x080faf9a  size=192  FUN_080faf9a  status=translated  naming=inferred
// naming: method table entry "setspawnweapon" -> 0x080faf9a.
void PlayerCmd_setspawnweapon(uint32 entref)
{
    struct gentity_s *ent;
    int weapon;

    ent = Scr_GetPlayerEntity103(entref);
    weapon = G_LoadWeaponDef(Scr_ParamText103(0));
    if (BG_PlayerHasWeapon((struct playerState_s *)&ent->client->ps, weapon) != 0) {
        ent->client->ps.weapon = weapon;
        ent->client->ps.weaponstate = 0;
    }
}

// 0x080fb05a  size=278  FUN_080fb05a  status=translated  naming=inferred
// naming: method table entry "dropitem" -> 0x080fb05a.
void PlayerCmd_dropitem(uint32 entref)
{
    struct gentity_s *ent;
    const char *itemName;
    int weapon;
    struct gitem_s *item;
    uint32 tagId;
    struct gentity_s *dropped;

    ent = Scr_GetPlayerEntity103(entref);
    itemName = Scr_ParamText103(0);
    weapon = G_LoadWeaponDef(itemName);
    if (weapon == 0) {
        item = BG_FindItemByName(itemName);
        if (item == 0) {
            dropped = 0;
        } else {
            dropped = Item_ThrowFromPlayer(ent, (struct gitem_s *)item, 0, 0);
        }
    } else {
        if ((uint32)Scr_GetNumParams() < 2) {
            tagId = (uint32)scr_str_tag_weapon_right;
        } else {
            tagId = Scr_GetLowerString(1);
        }
        dropped = Player_DropWeapon(ent, weapon, tagId);
    }
    Scr_AddEntityOrUndefined((struct gentity_s *)dropped);
}

// 0x080fb170  size=2627  FUN_080fb170  status=translated  naming=inferred
// naming: method table entry "finishplayerdamage" -> 0x080fb170. Script params: 0 inflictor, 1 attacker,
//   2 damage, 3 damage flags, 4 means of death, 5 weapon, 6 point, 7 direction, 8 hit location, 9 time offset.
//   Float-to-int conversions are truncating (x87 control word 0xc00 around every fistp).
void PlayerCmd_finishplayerdamage(uint32 entref)
{
    struct gentity_s *ent;
    struct gentity_s *inflictor;
    struct gentity_s *attacker;
    const float *pointArg;
    const float *dirArg;
    vec3_t point;
    vec3_t dirIn;
    vec3_t dir;
    vec3_t kick;
    int damage;
    uint32 dflags;
    int mod;
    int weapon;
    int hitLoc;
    int psOffsetTime;
    int kickDamage;
    float kickScale;
    int kickTime;
    struct gentity_s *temp;
    float yaw;
    float damageCountMax;
    float damageCountScale;
    float damageCountScaled;
    int id;

    inflictor = &g_gentities[1022];
    attacker = &g_gentities[1022];
    dirArg = 0;
    pointArg = 0;

    ent = Scr_GetPlayerEntity103(entref);
    damage = Scr_GetInt(2);
    if (damage <= 0) {
        return;
    }
    if (Scr_GetParamType(0) != 0 && Scr_GetPointer(0) == 0x15) {
        inflictor = Scr_GetEntityParam(1);
    }
    if (Scr_GetParamType(1) != 0 && Scr_GetPointer(1) == 0x15) {
        attacker = Scr_GetEntityParam(1);
    }
    dflags = (uint32)Scr_GetInt(3);
    mod = G_MeansOfDeathFromString(Scr_ParamText103(4));
    weapon = G_LoadWeaponDef(Scr_ParamText103(5));
    if (Scr_GetParamType(6) != 0) {
        Scr_GetVec3_103(6, point);
        pointArg = point;
    }
    if (Scr_GetParamType(7) != 0) {
        Scr_GetVec3_103(7, dirIn);
        dirArg = dirIn;
    }
    hitLoc = G_GetHitLocIndexFromId((uint16)Scr_GetAny(8));
    psOffsetTime = Scr_GetInt(9);
    if (dirArg == 0) {
        VectorClear_080fde30(dir);
    } else {
        Vec3_NormalizeCopy(dirArg, dir);
    }

    if ((ent->flags & 8) == 0 && (dflags & 4) == 0) {
        kickScale = 0.3f;
        if ((ent->client->ps.pm_flags & 1) == 0) {
            if ((ent->client->ps.pm_flags & 2) != 0) {
                kickScale = 0.15f;
            }
        } else {
            kickScale = 0.02f;
        }
        kickDamage = (int)((float)damage * kickScale);
        if (kickDamage > 0x3c) {
            kickDamage = 0x3c;
        }
        if (kickDamage != 0 && (ent->client->ps.eFlags & 0x300) == 0) {
            VectorScale_080fdece(dir, ((float)kickDamage * g_087a241c->current.value) / 250.0f, kick);
            VectorAdd_080fde8a(ent->client->ps.velocity, kick, ent->client->ps.velocity);
            if (ent->client->ps.pm_time == 0) {
                kickTime = kickDamage * 2;
                if (kickTime < 0x32) {
                    kickTime = 0x32;
                }
                if (kickTime > 200) {
                    kickTime = 200;
                }
                ent->client->ps.pm_time = kickTime;
                ent->client->ps.pm_flags = ent->client->ps.pm_flags | 0x400;
            }
        }
    } else {
        kickDamage = 0;
    }

    if ((ent->flags & 1) != 0) {
        return;
    }

    if (weapon != 0 && GetWeaponDef103(weapon)->weaponType == 0) {
        if (GetWeaponDef103(weapon)->rifleBullet == 0) {
            id = 0xb6;
        } else {
            id = 0xb7;
        }
        temp = (struct gentity_s *)G_TempEntity(point, id);
        temp->s.eventParm = (int)(uint32)Normal_DirToIndex(dir);
        temp->s.scale = (int)(uint32)Normal_DirToIndex(dir);
        temp->s.surfType = 7;
        temp->s.otherEntityNum = attacker->s.number;
        temp->r.clientMask[ent->client->ps.clientNum >> 5] =
            (1 << (ent->client->ps.clientNum & 0x1f)) | temp->r.clientMask[ent->client->ps.clientNum >> 5];

        if (GetWeaponDef103(weapon)->rifleBullet == 0) {
            id = 0xb9;
        } else {
            id = 0xba;
        }
        temp = (struct gentity_s *)G_TempEntity(point, id);
        temp->s.surfType = 7;
        temp->s.otherEntityNum = attacker->s.number;
        temp->s.clientNum = ent->client->ps.clientNum;
        temp->r.clientMask[0] = 0xffffffff;
        temp->r.clientMask[1] = 0xffffffff;
        temp->r.clientMask[ent->client->ps.clientNum >> 5] =
            ~(1 << (ent->client->ps.clientNum & 0x1f)) & temp->r.clientMask[ent->client->ps.clientNum >> 5];
    }

    ent->client->damageTaken = damage + ent->client->damageTaken;
    if (dirArg == 0) {
        VectorCopy_080fde5a(ent->r.currentOrigin, ent->client->damageDir);
        ent->client->damageDirIsSelf = 1;
    } else {
        VectorCopy_080fde5a(dir, ent->client->damageDir);
        ent->client->damageDirIsSelf = 0;
    }

    if ((ent->flags & 2) != 0 && ent->health - damage < 1) {
        damage = ent->health - 1;
    }

    damageCountMax = dvar_player_dmgtimer_maxTime->current.value;
    damageCountScale = dvar_player_dmgtimer_timePerPoint->current.value;
    damageCountScaled = (float)damage * damageCountScale;
    ent->client->ps.damageTimer = (int)damageCountScaled + ent->client->ps.damageTimer;
    if (dirArg == 0) {
        ent->client->ps.flinchYaw = 0;
    } else {
        ent->client->ps.flinchYaw = (int)Vec2_AngleDegrees(dirArg);
    }
    yaw = ent->client->ps.viewangles[1];
    if (yaw < 0.0f) {
        yaw = 360.0f + yaw;
    }
    ent->client->ps.flinchYaw = ent->client->ps.flinchYaw - (int)yaw;
    if (damageCountMax < (float)ent->client->ps.damageTimer) {
        ent->client->ps.damageTimer = (int)damageCountMax;
    }
    ent->client->ps.damageDuration = ent->client->ps.damageTimer;
    ent->health = ent->health - damage;

    Scr_AddEntityParam(attacker);
    Scr_AddIndex(damage);
    Scr_NotifyEntity(ent, (uint32)g_scrCallbackDamage, 2);

    if (ent->health < 1) {
        if (ent->health < -999) {
            ent->health = -999;
        }
        if (ENT_CLASS103(ent->handler).die != 0) {
            ENT_CLASS103(ent->handler).die(ent, inflictor, attacker, damage, mod, weapon, dir, hitLoc, psOffsetTime);
        }
        if (ent->r.inuse == 0) {
            return;
        }
    } else if (ENT_CLASS103(ent->handler).pain != 0) {
        ENT_CLASS103(ent->handler).pain(ent, attacker, damage, pointArg, mod, dir, hitLoc);
    }
    ent->client->ps.stats[0] = ent->health;
}

// 0x080fbbb4  size=224  FUN_080fbbb4  status=translated  naming=inferred
// naming: method table entry "suicide" -> 0x080fbbb4.
void PlayerCmd_suicide(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    ent->flags = ent->flags & 0xfffffffc;
    ent->health = 0;
    ent->client->ps.stats[0] = 0;
    player_die(ent, ent, ent, 100000, 0xc, 0, 0, 0, 0);
}

// 0x080fbc94  size=216  FUN_080fbc94  status=translated  naming=inferred
// naming: method table entry "openmenu" -> 0x080fbc94; format "%c %i" (0x74 't') referenced here.
void PlayerCmd_openmenu(uint32 entref)
{
    struct gentity_s *ent;
    int menu;

    ent = Scr_GetPlayerEntity103(entref);
    if (ent->client->connected == 2) {
        menu = G_MenuIndex(Scr_ParamText103(0));
        SV_SendServerCommandNum((uint16)entref, 1, va("%c %i", 0x74, menu));
        Scr_AddIndex(1);
    } else {
        Scr_AddIndex(0);
    }
}

// 0x080fbd6c  size=216  FUN_080fbd6c  status=translated  naming=inferred
// naming: method table entry "openmenunomouse" -> 0x080fbd6c; format "%c %i 1" referenced here.
void PlayerCmd_openmenunomouse(uint32 entref)
{
    struct gentity_s *ent;
    int menu;

    ent = Scr_GetPlayerEntity103(entref);
    if (ent->client->connected == 2) {
        menu = G_MenuIndex(Scr_ParamText103(0));
        SV_SendServerCommandNum((uint16)entref, 1, va("%c %i 1", 0x74, menu));
        Scr_AddIndex(1);
    } else {
        Scr_AddIndex(0);
    }
}

// 0x080fbe44  size=142  FUN_080fbe44  status=translated  naming=inferred
// naming: method table entry "closemenu" -> 0x080fbe44; sends command byte 0x75 ('u').
void PlayerCmd_closemenu(uint32 entref)
{
    Scr_GetPlayerEntity103(entref);
    SV_SendServerCommandNum((uint16)entref, 1, va("%c", 0x75));
}

// 0x080fbed2  size=142  FUN_080fbed2  status=translated  naming=inferred
// naming: method table entry "closeingamemenu" -> 0x080fbed2; sends command byte 0x4b ('K').
void PlayerCmd_closeingamemenu(uint32 entref)
{
    Scr_GetPlayerEntity103(entref);
    SV_SendServerCommandNum((uint16)entref, 1, va("%c", 0x4b));
}

// 0x080fbf60  size=296  FUN_080fbf60  status=translated  naming=inferred
// naming: method table entry "getweaponslotweapon" -> 0x080fbf60; string "Unknown weaponslot name %s..." referenced here.
void PlayerCmd_getweaponslotweapon(uint32 entref)
{
    struct gentity_s *ent;
    uint16 slotId;
    int slot;
    int weapon;

    ent = Scr_GetPlayerEntity103(entref);
    if (G_ClientIsPlaying(ent) == 0) {
        Scr_AddStringId((uint32)scr_str_none);
    } else {
        slotId = (uint16)Scr_GetAny(0);
        slot = BG_WeaponSlotFromName(ScrStr_TextFromHandle(slotId));
        if (slot == 0) {
            Scr_ErrorAtParam(0, va("Unknown weaponslot name %s. Valid weaponslots are \"primary\" and \"primaryb\"",
                                   ScrStr_TextFromHandle(slotId)));
        }
        weapon = (int)ent->client->ps.weaponslots[slot];
        if (weapon == 0) {
            Scr_AddStringId((uint32)scr_str_none);
        } else {
            Scr_AddString(GetWeaponDef103(weapon)->szInternalName);
        }
    }
}

// 0x080fc088  size=742  FUN_080fc088  status=translated  naming=inferred
// naming: method table entry "setweaponslotweapon" -> 0x080fc088; strings "Unknown weapon %s." and "Weapon %s goes in the %s weaponslot..." referenced here.
void PlayerCmd_setweaponslotweapon(uint32 entref)
{
    bool swapSecondary;
    struct gentity_s *ent;
    uint16 slotId;
    int slot;
    const char *weaponName;
    int weapon;
    struct weaponDef_s *def;
    int oldWeapon;
    uint8 alreadyOwned;
    int amount;
    const char *slotNameWanted;
    const char *slotNameDef;

    swapSecondary = false;
    ent = Scr_GetPlayerEntity103(entref);
    slotId = (uint16)Scr_GetAny(0);
    slot = BG_WeaponSlotFromName(ScrStr_TextFromHandle(slotId));
    if (slot == 0) {
        Scr_ErrorAtParam(0, va("Unknown weaponslot name %s. Valid weaponslots are \"primary\" and \"primaryb\"",
                               ScrStr_TextFromHandle(slotId)));
    }
    weaponName = Scr_ParamText103(1);
    if (Q_stricmp(weaponName, "none") == 0) {
        weapon = 0;
        def = 0;
    } else {
        weapon = G_LoadWeaponDef(weaponName);
        if (weapon == 0) {
            Scr_ErrorAtParam(1, va("Unknown weapon %s.", weaponName));
        }
        def = GetWeaponDef103(weapon);
        if (def->weaponSlot != slot &&
            ((def->weaponSlot != 1 && def->weaponSlot != 2) || (slot != 1 && slot != 2))) {
            slotNameWanted = BG_WeaponSlotName(slot);
            slotNameDef = BG_WeaponSlotName(def->weaponSlot);
            Scr_ErrorAtParam(1, va("Weapon %s goes in the %s weaponslot, not the %s weaponslot.",
                                   weaponName, slotNameDef, slotNameWanted));
        }
    }
    oldWeapon = (int)ent->client->ps.weaponslots[slot];
    if (oldWeapon != 0) {
        BG_TakeWeapon((struct playerState_s *)&ent->client->ps, oldWeapon);
    }
    if (weapon != 0) {
        if (slot == 2 && ent->client->ps.weaponslots[1] == 0) {
            swapSecondary = true;
        }
        alreadyOwned = BitSet_Test(ent->client->ps.weapons, weapon);
        G_TouchTriggerEntity(ent->client, weapon);
        if (swapSecondary) {
            ent->client->ps.weaponslots[2] = ent->client->ps.weaponslots[1];
            ent->client->ps.weaponslots[1] = 0;
        }
        amount = def->startAmmo - ent->client->ps.ammo[def->iAmmoIndex];
        if (amount > 0) {
            Player_GiveWeaponAmmo(ent, weapon, amount, alreadyOwned == 0);
        }
    }
}

// 0x080fc36e  size=362  FUN_080fc36e  status=translated  naming=inferred
// naming: method table entry "getweaponslotammo" -> 0x080fc36e; string "Unknown weaponslot name %s..." referenced here.
void PlayerCmd_getweaponslotammo(uint32 entref)
{
    struct gentity_s *ent;
    uint16 slotId;
    int slot;
    int weapon;
    int idx;

    ent = Scr_GetPlayerEntity103(entref);
    if (G_ClientIsPlaying(ent) == 0) {
        Scr_AddIndex(0);
    } else {
        slotId = (uint16)Scr_GetAny(0);
        slot = BG_WeaponSlotFromName(ScrStr_TextFromHandle(slotId));
        if (slot == 0) {
            Scr_ErrorAtParam(0, va("Unknown weaponslot name %s. Valid weaponslots are \"primary\" and \"primaryb\"",
                                   ScrStr_TextFromHandle(slotId)));
        }
        weapon = (int)ent->client->ps.weaponslots[slot];
        if (weapon == 0) {
            Scr_AddIndex(0);
        } else if (BG_GetWeaponClipOnly(weapon) == 0) {
            idx = BG_GetWeaponAmmoIndex(weapon);
            Scr_AddIndex(ent->client->ps.ammo[idx]);
        } else {
            idx = BG_GetWeaponClipIndex(weapon);
            Scr_AddIndex(ent->client->ps.ammoclip[idx]);
        }
    }
}

// 0x080fc4d8  size=435  FUN_080fc4d8  status=translated  naming=inferred
// naming: method table entry "setweaponslotammo" -> 0x080fc4d8; string "Unknown weaponslot name %s..." referenced here.
void PlayerCmd_setweaponslotammo(uint32 entref)
{
    struct gentity_s *ent;
    uint16 slotId;
    int slot;
    int amount;
    int weapon;
    int idx;

    ent = Scr_GetPlayerEntity103(entref);
    slotId = (uint16)Scr_GetAny(0);
    slot = BG_WeaponSlotFromName(ScrStr_TextFromHandle(slotId));
    if (slot == 0) {
        Scr_ErrorAtParam(0, va("Unknown weaponslot name %s. Valid weaponslots are \"primary\" and \"primaryb\"",
                               ScrStr_TextFromHandle(slotId)));
    }
    amount = Scr_GetInt(1);
    weapon = (int)ent->client->ps.weaponslots[slot];
    if (weapon != 0) {
        if (BG_GetWeaponClipOnly(weapon) == 0) {
            idx = BG_GetWeaponAmmoIndex(weapon);
            if (idx != 0) {
                if (amount < 0) {
                    amount = 0;
                } else if (BG_GetAmmoTypeMax(idx) < amount) {
                    amount = BG_GetAmmoTypeMax(idx);
                }
                ent->client->ps.ammo[idx] = amount;
            }
        } else {
            idx = BG_GetWeaponClipIndex(weapon);
            if (idx != 0) {
                if (amount < 0) {
                    amount = 0;
                } else if (BG_GetClipTypeSize(idx) < amount) {
                    amount = BG_GetClipTypeSize(idx);
                }
                ent->client->ps.ammoclip[idx] = amount;
            }
        }
    }
}

// 0x080fc68c  size=324  FUN_080fc68c  status=translated  naming=inferred
// naming: method table entry "getweaponslotclipammo" -> 0x080fc68c; string "Unknown weaponslot name %s..." referenced here.
void PlayerCmd_getweaponslotclipammo(uint32 entref)
{
    struct gentity_s *ent;
    uint16 slotId;
    int slot;
    int weapon;
    int idx;

    ent = Scr_GetPlayerEntity103(entref);
    if (G_ClientIsPlaying(ent) == 0) {
        Scr_AddIndex(0);
    } else {
        slotId = (uint16)Scr_GetAny(0);
        slot = BG_WeaponSlotFromName(ScrStr_TextFromHandle(slotId));
        if (slot == 0) {
            Scr_ErrorAtParam(0, va("Unknown weaponslot name %s. Valid weaponslots are \"primary\" and \"primaryb\"",
                                   ScrStr_TextFromHandle(slotId)));
        }
        weapon = (int)ent->client->ps.weaponslots[slot];
        if (weapon == 0) {
            Scr_AddIndex(0);
        } else {
            idx = BG_GetWeaponClipIndex(weapon);
            if (idx == 0) {
                Scr_AddIndex(0);
            } else {
                Scr_AddIndex(ent->client->ps.ammoclip[idx]);
            }
        }
    }
}

// 0x080fc7d0  size=333  FUN_080fc7d0  status=translated  naming=inferred
// naming: method table entry "setweaponslotclipammo" -> 0x080fc7d0; string "Unknown weaponslot name %s..." referenced here.
void PlayerCmd_setweaponslotclipammo(uint32 entref)
{
    struct gentity_s *ent;
    uint16 slotId;
    int slot;
    int amount;
    int weapon;
    int idx;

    ent = Scr_GetPlayerEntity103(entref);
    slotId = (uint16)Scr_GetAny(0);
    slot = BG_WeaponSlotFromName(ScrStr_TextFromHandle(slotId));
    if (slot == 0) {
        Scr_ErrorAtParam(0, va("Unknown weaponslot name %s. Valid weaponslots are \"primary\" and \"primaryb\"",
                               ScrStr_TextFromHandle(slotId)));
    }
    amount = Scr_GetInt(1);
    weapon = (int)ent->client->ps.weaponslots[slot];
    if (weapon == 0) {
        Scr_AddIndex(0);
    } else {
        idx = BG_GetWeaponClipIndex(weapon);
        if (idx != 0) {
            if (amount < 0) {
                amount = 0;
            }
            if (BG_GetClipTypeSize(idx) < amount) {
                amount = BG_GetClipTypeSize(idx);
            }
            ent->client->ps.ammoclip[idx] = amount;
        }
    }
}

// 0x080fc91e  size=249  FUN_080fc91e  status=translated  naming=inferred
// naming: method table entry "setweaponclipammo" -> 0x080fc91e.
void PlayerCmd_setweaponclipammo(uint32 entref)
{
    struct gentity_s *ent;
    const char *weaponName;
    int amount;
    int weapon;
    int idx;

    ent = Scr_GetPlayerEntity103(entref);
    weaponName = Scr_ParamText103(0);
    amount = Scr_GetInt(1);
    weapon = G_LoadWeaponDef(weaponName);
    if (weapon == 0) {
        Scr_AddIndex(0);
    } else {
        idx = BG_GetWeaponClipIndex(weapon);
        if (idx != 0) {
            if (amount < 0) {
                amount = 0;
            }
            if (BG_GetClipTypeSize(idx) < amount) {
                amount = BG_GetClipTypeSize(idx);
            }
            ent->client->ps.ammoclip[idx] = amount;
        }
    }
}

// 0x080fca18  size=134  FUN_080fca18  status=translated  naming=inferred
// naming: method table entry "iprintln" -> 0x080fca18; sends command byte 0x66 ('f').
void PlayerCmd_iprintln(uint32 entref)
{
    Scr_GetPlayerEntity103(entref);
    GScr_SendGameMessage((uint16)entref, va("%c", 0x66));
}

// 0x080fca9e  size=134  FUN_080fca9e  status=translated  naming=inferred
// naming: method table entry "iprintlnbold" -> 0x080fca9e; sends command byte 0x67 ('g').
void PlayerCmd_iprintlnbold(uint32 entref)
{
    Scr_GetPlayerEntity103(entref);
    GScr_SendGameMessage((uint16)entref, va("%c", 0x67));
}

// 0x080fcb24  size=161  FUN_080fcb24  status=translated  naming=inferred
// naming: method table entry "spawn" -> 0x080fcb24.
void PlayerCmd_spawn(uint32 entref)
{
    struct gentity_s *ent;
    vec3_t origin;
    vec3_t angles;

    ent = Scr_GetPlayerEntity103(entref);
    Scr_GetVec3_103(0, origin);
    Scr_GetVec3_103(1, angles);
    ClientSpawn(ent, origin, angles);
}

// 0x080fcbc6  size=130  FUN_080fcbc6  status=translated  naming=inferred
// naming: method table entry "setentertime" -> 0x080fcbc6.
void PlayerCmd_setentertime(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    ent->client->enterTime = Scr_GetInt(0);
}

// 0x080fcc76  size=790  FUN_080fcc76  status=translated  naming=inferred
// naming: method table entry "cloneplayer" -> 0x080fcc76.
void PlayerCmd_cloneplayer(uint32 entref)
{
    struct gentity_s *ent;
    struct gclient_s *client;
    struct gentity_s *clone;
    struct XAnimTree *tree;
    struct corpse103_s *corpse;
    int delay;
    int i;
    int slot;

    ent = Scr_GetPlayerEntity103(entref);
    delay = Scr_GetInt(0);
    client = ent->client;
    clone = (struct gentity_s *)G_AllocTempEntity();
    clone->s.clientNum = client->ps.clientNum;
    clone->s.eFlags = (clone->s.eFlags & 2) | (client->ps.eFlags & 0xfffffffd) | 0xa0000;
    G_SetPositionTrajectory(clone, client->ps.origin);
    G_SetAnglesTrajectory(clone, ent->r.currentAngles);
    clone->s.pos.trType = 5;
    clone->s.pos.trTime = level_time;
    VectorCopy_080fde5a(client->ps.velocity, clone->s.pos.trDelta);
    clone->s.eType = 2;
    clone->physicsObject = 1;
    tree = DObj_GetTree((const struct DObj *)Com_GetServerDObj(client->ps.clientNum));
    for (i = 0; i < 2; i = i + 1) {
        if (g_087a2454->current.value < clone->s.pos.trDelta[i]) {
            clone->s.pos.trDelta[i] = g_087a2454->current.value;
        }
    }
    clone->u.item.ammoCount = level_time;
    slot = G_PickTrackedSlot();
    corpse = (struct corpse103_s *)g_trackedEntitySlots + slot;
    corpse->entNum = clone->s.number;
    corpse->time = level_time;
    corpse->active = 1;
    memcpy(corpse->data, (uint8 *)g_clientInfos + client->ps.clientNum * 0x4b8, 0x4b8);
    *(int *)(corpse->data + 0x4a4) = corpse->handle; /* copy of record start lands at +0x4b0 of the entry */
    XAnim_CopyTreeSlots(tree, (struct XAnimTree *)corpse->handle /* the record keeps its start as an int handle */);
    clone->s.groundEntityNum = 0x3ff;
    clone->r.svFlags = 2;
    VectorCopy_080fde5a(ent->r.mins, clone->r.mins);
    VectorCopy_080fde5a(ent->r.maxs, clone->r.maxs);
    VectorCopy_080fde5a(ent->r.absmin, clone->r.absmin);
    VectorCopy_080fde5a(ent->r.absmax, clone->r.absmax);
    clone->s.legsAnim = client->ps.legsAnim;
    clone->s.torsoAnim = client->ps.torsoAnim;
    clone->clipmask = 0x10001;
    clone->r.contents = 0x4002000;
    SV_LinkEntity(clone);
    clone->nextthink = delay + level_time;
    clone->handler = 0xc;
    Scr_AddEntityOrUndefined(clone);
}

// 0x080fcf8c  size=462  FUN_080fcf8c  status=translated  naming=inferred
// naming: method table entry "setclientcvar" -> 0x080fcf8c; strings "Client Dvar Value" and "Dvar %s has an invalid dvar name" referenced here.
// Quirk preserved: the copy loop is bounded by 0x2000 characters although the destination buffer is 0x400 bytes.
void PlayerCmd_setclientcvar(uint32 entref)
{
    char value[1032];
    char quoted[1024];
    const char *name;
    const char *src;
    char *dst;
    uint32 type;
    int i;

    Scr_GetPlayerEntity103(entref);
    name = Scr_ParamText103(0);
    type = Scr_GetParamType(1);
    if (type == 3) {
        GScr_BuildMessageString(1, Scr_GetNumParams() - 1, "Client Dvar Value", value, 0x400);
        src = value;
    } else {
        src = Scr_ParamText103(1);
    }
    strlen(src);
    if (Com_IsIdentifierName(name) == 0) {
        Scr_Error(va("Dvar %s has an invalid dvar name", name));
    } else {
        dst = quoted;
        memset(quoted, 0, 0x400);
        for (i = 0; i < 0x2000 && src[i] != '\0'; i = i + 1) {
            *dst = (char)Q_NormalizeApostrophe(src[i]);
            if (*dst == '"') {
                *dst = '\'';
            }
            dst = dst + 1;
        }
        SV_SendServerCommandNum((uint16)entref, 1, va("%c %s \"%s\"", 0x76, name, quoted));
    }
}

// 0x080fd15a  size=169  FUN_080fd15a  status=translated  naming=inferred
// naming: method table entry "istalking" -> 0x080fd15a.
void PlayerCmd_istalking(uint32 entref)
{
    struct gentity_s *ent;
    int elapsed;

    ent = Scr_GetPlayerEntity103(entref);
    elapsed = level_time - ent->client->lastVoiceTime;
    if (elapsed < 0 || g_087a2468->current.integer <= elapsed) {
        Scr_AddIndex(0);
    } else {
        Scr_AddIndex(1);
    }
}

// 0x080fd204  size=130  FUN_080fd204  status=translated  naming=inferred
// naming: method table entry "freezecontrols" -> 0x080fd204.
void PlayerCmd_freezecontrols(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    ent->client->bFrozen = Scr_GetInt(0);
}

// 0x080fd286  size=127  FUN_080fd286  status=translated  naming=inferred
// naming: method table entry "disableweapon" -> 0x080fd286.
void PlayerCmd_disableweapon(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    ent->client->ps.pm_flags = ent->client->ps.pm_flags | 0x4000000;
}

// 0x080fd306  size=127  FUN_080fd306  status=translated  naming=inferred
// naming: method table entry "enableweapon" -> 0x080fd306.
void PlayerCmd_enableweapon(uint32 entref)
{
    struct gentity_s *ent;

    ent = Scr_GetPlayerEntity103(entref);
    ent->client->ps.pm_flags = ent->client->ps.pm_flags & 0xfbffffff;
}

// 0x080fd386  size=402  FUN_080fd386  status=translated  naming=inferred
// naming: method table entry "setreverb" -> 0x080fd386; USAGE string "USAGE: player setReverb(..." referenced here.
void PlayerCmd_setreverb(uint32 entref)
{
    float fadeTime;
    float wetLevel;
    float dryLevel;
    uint32 numParams;
    const char *roomType;
    uint16 priorityId;
    int priority;

    Scr_GetPlayerEntity103(entref);
    fadeTime = 0.0f;
    dryLevel = 1.0f;
    wetLevel = 0.5f;
    numParams = (uint32)Scr_GetNumParams();
    if (numParams != 3) {
        if (numParams < 4) {
            if (numParams != 2) {
                goto usage;
            }
            goto haveArgs;
        }
        if (numParams != 4) {
            if (numParams != 5) {
                goto usage;
            }
            fadeTime = Scr_GetFloat(4);
        }
        wetLevel = Scr_GetFloat(3);
    }
    dryLevel = Scr_GetFloat(2);
haveArgs:
    roomType = Scr_ParamText103(1);
    priorityId = (uint16)Scr_GetAny(0);
    priority = 1;
    if (priorityId == scr_str_snd_enveffectsprio_level) {
        priority = 1;
    } else if (priorityId == scr_str_snd_enveffectsprio_shellshock) {
        priority = 2;
    } else {
        Scr_Error("priority must be 'snd_enveffectsprio_level' or 'snd_enveffectsprio_shellshock'\n");
    }
    SV_SendServerCommandNum((uint16)entref, 1,
                            va("%c %i \"%s\" %g %g %g", 0x72, priority, roomType,
                               (double)dryLevel, (double)wetLevel, (double)fadeTime));
    return;
usage:
    Scr_Error("USAGE: player setReverb(\"priority\", \"roomtype\", drylevel = 1.0, wetlevel = 0.5, fadetime = 0);\nValid priorities are \"snd_enveffectsprio_level\" or \"snd_enveffectsprio_shellshock\", dry level is a float from 0 (no source sound) to 1 (full source sound), wetlevel is a float from 0 (no effect) to 1 (full effect), fadetime is in sec and modifies drylevel and wetlevel\n");
}

// 0x080fd518  size=297  FUN_080fd518  status=translated  naming=inferred
// naming: method table entry "deactivatereverb" -> 0x080fd518; USAGE string "USAGE: player deactivateReverb(..." referenced here.
// Quirk preserved: the format string is the 6-field setReverb format but only (command, priority, fadetime as double)
// are passed, so the "%s" and the three "%g" read past the supplied varargs.
void PlayerCmd_deactivatereverb(uint32 entref)
{
    float fadeTime;
    int numParams;
    uint16 priorityId;
    int priority;

    Scr_GetPlayerEntity103(entref);
    fadeTime = 0.0f;
    numParams = Scr_GetNumParams();
    if (numParams != 1) {
        if (numParams != 2) {
            Scr_Error("USAGE: player deactivateReverb(\"priority\", fadetime = 0);\nValid priorities are \"snd_enveffectsprio_level\" or \"snd_enveffectsprio_shellshock\", fadetime is the time spent fading to the next lowest active reverb priority level in seconds\n");
            return;
        }
        fadeTime = Scr_GetFloat(1);
    }
    priorityId = (uint16)Scr_GetAny(0);
    priority = 1;
    if (priorityId == scr_str_snd_enveffectsprio_level) {
        priority = 1;
    } else if (priorityId == scr_str_snd_enveffectsprio_shellshock) {
        priority = 2;
    } else {
        Scr_Error("priority must be 'snd_enveffectsprio_level' or 'snd_enveffectsprio_shellshock'\n");
    }
    SV_SendServerCommandNum((uint16)entref, 1,
                            va("%c %i \"%s\" %g %g %g", 0x44, priority, (double)fadeTime));
}

// 0x080fd642  size=381  FUN_080fd642  status=translated  naming=inferred
// naming: method table entry "setchannelvolumes" -> 0x080fd642; USAGE string "USAGE: player setchannelvolumes(..." referenced here.
void PlayerCmd_setchannelvolumes(uint32 entref)
{
    float fadeTime;
    int numParams;
    int shockIndex;
    uint16 priorityId;
    int priority;

    Scr_GetPlayerEntity103(entref);
    fadeTime = 0.0f;
    numParams = Scr_GetNumParams();
    if (numParams != 2) {
        if (numParams != 3) {
            Scr_Error("USAGE: player setchannelvolumes(\"priority\", \"shock name\", fadetime = 0);\nValid priorities are \"snd_channelvolprio_holdbreath\", \"snd_channelvolprio_pain\", or \"snd_channelvolprio_shellshock\", fadetime is in sec\n");
            return;
        }
        fadeTime = Scr_GetFloat(2);
    }
    shockIndex = G_FindConfigstringIndex(Scr_ParamText103(1), 0x48e, 0x10, 0, 0);
    priorityId = (uint16)Scr_GetAny(0);
    priority = 1;
    if (priorityId == scr_str_snd_channelvolprio_holdbreath) {
        priority = 1;
    } else if (priorityId == scr_str_snd_channelvolprio_pain) {
        priority = 2;
    } else if (priorityId == scr_str_snd_channelvolprio_shellshock) {
        priority = 3;
    } else {
        Scr_Error("priority must be 'snd_channelvolprio_holdbreath', 'snd_channelvolprio_pain', or 'snd_channelvolprio_shellshock'\n");
    }
    SV_SendServerCommandNum((uint16)entref, 1, va("%c %i %i %g", 0x45, priority, shockIndex, (double)fadeTime));
}

// 0x080fd7c0  size=319  FUN_080fd7c0  status=translated  naming=inferred
// naming: method table entry "deactivatechannelvolumes" -> 0x080fd7c0; USAGE string "USAGE: player deactivatechannelvolumes(..." referenced here.
// Quirk preserved: reuses the 6-field "%c %i \"%s\" %g %g %g" format with only (command, priority, fadetime as double).
void PlayerCmd_deactivatechannelvolumes(uint32 entref)
{
    float fadeTime;
    int numParams;
    uint16 priorityId;
    int priority;

    Scr_GetPlayerEntity103(entref);
    fadeTime = 0.0f;
    numParams = Scr_GetNumParams();
    if (numParams != 1) {
        if (numParams != 2) {
            Scr_Error("USAGE: player deactivatechannelvolumes(\"priority\", fadetime = 0);\nValid priorities are \"snd_channelvolprio_holdbreath\", \"snd_channelvolprio_pain\", or \"snd_channelvolprio_shellshock\", fadetime is the time spent fading to the next lowest active reverb priority level in seconds\n");
            return;
        }
        fadeTime = Scr_GetFloat(1);
    }
    priorityId = (uint16)Scr_GetAny(0);
    priority = 1;
    if (priorityId == scr_str_snd_channelvolprio_holdbreath) {
        priority = 1;
    } else if (priorityId == scr_str_snd_channelvolprio_pain) {
        priority = 2;
    } else if (priorityId == scr_str_snd_channelvolprio_shellshock) {
        priority = 3;
    } else {
        Scr_Error("priority must be 'snd_channelvolprio_holdbreath', 'snd_channelvolprio_pain', or 'snd_channelvolprio_shellshock'\n");
    }
    SV_SendServerCommandNum((uint16)entref, 1, va("%c %i \"%s\" %g %g %g", 0x46, priority, (double)fadeTime));
}

// 0x080fd900  size=144  FUN_080fd900  status=translated  naming=inferred
// naming: method table entry "islookingat" -> 0x080fd900.
void PlayerCmd_islookingat(uint32 entref)
{
    struct gentity_s *ent;
    struct gclient_s *client;
    struct gentity_s *target;

    ent = Scr_GetPlayerEntity103(entref);
    client = ent->client;
    target = Scr_GetEntityParam(0);
    Scr_AddIndex(client->pLookatEnt == target);
}

// 0x080fd990  size=175  FUN_080fd990  status=translated  naming=inferred
// naming: method table entry "playlocalsound" -> 0x080fd990; command byte 0x73 ('s'), sent unreliably (reliable=0).
void PlayerCmd_playlocalsound(uint32 entref)
{
    const char *soundName;
    uint8 soundIndex;

    Scr_GetPlayerEntity103(entref);
    soundName = Scr_ParamText103(0);
    soundIndex = G_SoundIndex(soundName);
    SV_SendServerCommandNum((uint16)entref, 0, va("%c %i", 0x73, (int)soundIndex));
}
