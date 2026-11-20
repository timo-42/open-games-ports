/*
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 048.
 * Range 0x08103618..0x08109096: hud element pool and script field access,
 * float helpers, client ammo bookkeeping, item/weapon pickup and drop, the
 * item registry, small vector helpers, server dvar registration and the
 * per-entity extension pool. Binary SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * Conventions:
 *  - ROUND() in the decompiler output is an x87 fistp with the truncating
 *    control word (or ax,0xc00 at 0x8105a8e and others): every (int)ROUND(x)
 *    here is a C (int) cast.
 *  - Entity numbers are (ent - g_gentities), the gentity stride is 0x230.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_048_types.h"

#define COD2_ENTNUM(ent) ((int)((ent) - g_gentities))
#define COD2_DVAR_INT(d) (*(int *)((char *)(d) + 8))
#define COD2_DVAR_FLOAT(d) (*(float *)((char *)(d) + 8))
#define COD2_DVAR_BOOL(d) (*(unsigned char *)((char *)(d) + 8))

static inline uint32 FLTBITS(float f)
{
    union { float f; uint32 u; } v;
    v.f = f;
    return v.u;
}

/* Forward declarations for functions defined later in this chunk. */
int Float_RoundToInt_08104a34(float v);
int Float_CeilToInt(float v);
float Float_Max_081049ec(float a, float b);
float Float_Min_08104a10(float a, float b);
float Float_SelectIfNonNegative_08104a98(float test, float ifNonNeg, float otherwise);
void Vec3Clear(float *v);
void Vec3Set(float *v, float x, float y, float z);
void Vec3Copy(const float *src, float *dst);
void Vec3Add(const float *a, const float *b, float *out);
void Vec3Sub(const float *a, const float *b, float *out);
void Vec3Average(const float *a, const float *b, float *out);
void Vec3Scale(const float *a, float s, float *out);
void Vec3MulAdd(const float *a, float s, const float *b, float *out);
long double Vec3Dot(const float *a, const float *b);
long double Vec3LengthSquared(const float *a);
unsigned int Ent_GetTraceMask(const struct gentity_s *ent);
int Player_GiveWeaponAmmo(struct gentity_s *ent, int weapon, int amount, int forceFlag);
void Client_FillClipFromAmmo(struct gclient_s *client, int weapon);
int Item_PickupAmmo(struct gentity_s *item, struct gentity_s *player);
int Item_PickupWeapon(struct gentity_s *item, struct gentity_s *player, int *outEvent, int dropFlag);
int Item_PickupHealth(struct gentity_s *item, struct gentity_s *player);
void Item_TouchPickup(struct gentity_s *item, struct gentity_s *player, int pickupFlags);
int DroppedWeapon_FindSlot(void);
void Item_SetOwnerUnset(struct gentity_s *ent);
void Item_SetModelName(const struct gitem_s *def, void *dst);
struct gentity_s *Item_SpawnDropped(const struct gitem_s *def, const float *origin, const float *dir, int owner);
struct gentity_s *Item_ThrowFromPlayer(struct gentity_s *player, const struct gitem_s *def, float forward, int dropMode);
struct gentity_s *Player_DropWeapon(struct gentity_s *player, int weapon, int withAim);
void ItemRegistry_Reset(void);
void ItemRegistry_BuildNameList(void);
void ItemRegistry_BuildBitmask(void);
void ItemRegistry_Register(int index, int setDirty);
int ItemRegistry_IsRegistered(int index);
void Item_SetupDroppedWeapon(struct gentity_s *ent, const struct gitem_s *def);
void G_CorpseApplyTrace(struct gentity_s *ent, struct ent_ext_s *ext, const struct trace_s *tr, const float *vel);
void G_CorpseAlignToTrace(struct gentity_s *ent, const struct trace_s *tr);
void Ext_ReadSlotHandle(struct gentity_s *ent, struct ent_ext_s *ext, void *out);
void G_UpdateCorpseSlot(struct gentity_s *ent);
void Ent_UpdateDObj(struct gentity_s *ent);
void G_RunCorpse(struct gentity_s *ent);
void Ent_StepMovement(struct gentity_s *ent);
void *Hunk_AllocLowCallback(int size);
void Pool_AllocGameObjects(void);
void Pool_FreeGameObjects(void);
void Game_FreeAllEntities(void);
void G_RegisterDvars(void);
long double Float_GetLevelValue(void);
void Level_SetIntValue(int value);
int Level_GetIntValue(void);

/* ---------------------------------------------------------------------- */

// 0x08103618  size=71  HudElem_AddPackedFieldString  status=translated  naming=unknown
// naming: no string or call evidence; descriptive name for a packed-field getter (shift/mask at field +0x10/+0x0c, names table indexed by the extracted value, result pushed with Scr_AddString)
void HudElem_AddPackedFieldString(void *base, struct hudelem_field_s *field, char **names)
{
    int raw = *(int *)((char *)base + field->offset);
    unsigned int index = (unsigned int)(raw >> (field->shift & 0x1f)) & field->mask;
    Scr_AddString(names[index]);
}

// 0x08103b2e  size=119  HudElem_ScriptGetField  status=translated  naming=unknown
// naming: field table entry (stride 0x1c) dispatch; the direction (write vs read) is inferred from the pairing with the setter slot (+0x18) and Scr_AddEntityFieldValue; unverified
void HudElem_ScriptGetField(int elemIndex, int fieldIndex)
{
    const struct hudelem_field_s *field = &g_hudelemFieldTable[fieldIndex];
    struct game_hudelem_s *elem = &g_hudelems[elemIndex];

    if (field->setter == 0) {
        Scr_AddEntityFieldValue(elem, field->type, field->offset);
    } else {
        field->setter(elem, fieldIndex);
    }
}

// 0x08103ba6  size=119  HudElem_ScriptSetField  status=translated  naming=unknown
// naming: field table entry dispatch through the getter slot (+0x14); counterpart of HudElem_ScriptGetField, direction inferred
void HudElem_ScriptSetField(int elemIndex, int fieldIndex)
{
    const struct hudelem_field_s *field = &g_hudelemFieldTable[fieldIndex];
    struct game_hudelem_s *elem = &g_hudelems[elemIndex];

    if (field->getter == 0) {
        Scr_SetEntityFieldGeneric(elem, field->type, field->offset);
    } else {
        field->getter(elem, fieldIndex);
    }
}

// 0x08103c1e  size=67  HudElem_ReleaseStringFields  status=translated  naming=unknown
// naming: walks the hud field table and releases each script-string field (type 3) of one element; descriptive name
void HudElem_ReleaseStringFields(struct game_hudelem_s *elem)
{
    const struct hudelem_field_s *field;

    for (field = g_hudelemFieldTable; field->name != 0; field++) {
        if (field->type == 3) {
            ScrStr_AssignRef((uint16 *)((char *)elem + field->offset), 0);
        }
    }
}

// 0x08103dca  size=78  HudElem_RegisterScriptFields  status=translated  naming=unknown
// naming: registers every hud field name with script class 1 (the class list at g_scrClassInfo names class 1 as "hudelem"); descriptive name
void HudElem_RegisterScriptFields(void)
{
    const struct hudelem_field_s *field;

    for (field = g_hudelemFieldTable; field->name != 0; field++) {
        Scr_AddClassField(1, field->name, (unsigned short)(field - g_hudelemFieldTable));
    }
}

// 0x08103e18  size=57  HudElem_FromHandle  status=translated  naming=observed
// naming: string "not a hud element" at 0x08103e18 callers; handle high word must equal 1, low word indexes the pool
struct game_hudelem_s *HudElem_FromHandle(unsigned int handle)
{
    struct game_hudelem_s *elem;

    if ((handle >> 16) == 1) {
        elem = &g_hudelems[handle & 0xffff];
    } else {
        Scr_ErrorNoParam("not a hud element");
        elem = 0;
    }
    return elem;
}

// 0x08104124  size=179  HudElem_SetTimerCommon  status=translated  naming=inferred
// naming: usage string "USAGE: <hudelem> %s(time_in_seconds);" and "time %g should be > 0" name the builtin; shared body of the one-argument timer builtins (type 7 from setTimer, type 8 from other callers)
void HudElem_SetTimerCommon(unsigned int handle, int type, const char *usage)
{
    struct game_hudelem_s *elem = HudElem_FromHandle(handle);
    int ms;

    if (Scr_GetNumParams() != 1) {
        Scr_Error(va("USAGE: <hudelem> %s(time_in_seconds);\n", usage));
    }
    ms = Float_CeilToInt((float)((long double)1000.0 * (long double)Scr_GetFloat(0)));
    if (ms < 1 && type != 8) {
        Scr_ErrorAtParam(0, va("time %g should be > 0", (double)((float)ms * 0.001)));
    }
    HudElem_ClearFields(elem);
    elem->elem.type = type;
    elem->elem.time = ms + level_time;
}

// 0x081041d8  size=462  HudElem_SetShaderTimerCommon  status=translated  naming=inferred
// naming: usage string "USAGE: <hudelem> %s(time_in_seconds, total_clock_time_in_seconds, shadername[, width, height]);" and the duration/width/height messages; shared body of the shader timer builtin (type 0xc)
void HudElem_SetShaderTimerCommon(unsigned int handle, int type, const char *usage)
{
    struct game_hudelem_s *elem = HudElem_FromHandle(handle);
    int numParams = Scr_GetNumParams();
    int ms;
    int durationMs;
    int shader;
    int width;
    int height;

    if (numParams != 3 && numParams != 5) {
        Scr_Error(va("USAGE: <hudelem> %s(time_in_seconds, total_clock_time_in_seconds, shadername[, width, height]);\n", usage));
    }
    ms = Float_CeilToInt((float)((long double)1000.0 * (long double)Scr_GetFloat(0)));
    if (ms < 1 && type != 0xc) {
        Scr_ErrorAtParam(0, va("time %g should be > 0", (double)((float)ms * 0.001)));
    }
    durationMs = Float_CeilToInt((float)((long double)1000.0 * (long double)Scr_GetFloat(1)));
    if (durationMs < 1) {
        Scr_ErrorAtParam(1, va("duration %g should be > 0", (double)((float)durationMs * 0.001)));
    }
    shader = G_ShaderIndex(Scr_ReleaseAnyValue(2));
    if (numParams == 3) {
        width = 0;
        height = 0;
    } else {
        width = Scr_GetInt(3);
        if (width < 0) {
            Scr_ErrorAtParam(3, va("width %i < 0", width));
        }
        height = Scr_GetInt(4);
        if (height < 0) {
            Scr_ErrorAtParam(4, va("height %i < 0", height));
        }
    }
    HudElem_ClearFields(elem);
    elem->elem.type = type;
    elem->elem.time = ms + level_time;
    elem->elem.duration = durationMs;
    elem->elem.width = width;
    elem->elem.height = height;
    elem->elem.materialIndex = shader;
}

// 0x081043a6  size=35  HudElem_ScrSetTimer  status=translated  naming=observed
// naming: string "setTimer" at 0x081043a6 is passed as the builtin name
void HudElem_ScrSetTimer(unsigned int handle)
{
    HudElem_SetTimerCommon(handle, 7, "setTimer");
}

// 0x081047fe  size=131  HudElem_LookupMethod  status=translated  naming=unknown
// naming: searches the 18-entry hud method table (stride 0x0c, name at +0, handler at +4) by strcmp; returns the handler and canonicalises the caller's name pointer; descriptive name
void *HudElem_LookupMethod(const char **name)
{
    const char *target = *name;
    unsigned int i = 0;

    for (;;) {
        if (i > 0x11) {
            return 0;
        }
        if (strcmp(target, g_hudelemMethodTable[i].name) == 0) {
            break;
        }
        i++;
    }
    *name = (char *)g_hudelemMethodTable[i].name;
    return (void *)g_hudelemMethodTable[i].handler;
}

// 0x08104882  size=362  HudElem_BuildClientLists  status=translated  naming=unknown
// naming: copies the visible hud elements of the 1024-entry pool into the client's two 31-slot snapshot lists (flagged list selected by listSelector); flags bit0 clears/fills the flagged list, bit1 the unflagged list; descriptive name
void HudElem_BuildClientLists(struct gclient_s *client, int targetClient, unsigned char flags)
{
    int unflaggedCount = 0;
    int flaggedCount = 0;
    unsigned int i;
    const struct game_hudelem_s *elem = g_hudelems;

    if ((flags & 1) != 0) {
        memset(client->ps.hudArchival, 0, 0xf80);
    }
    if ((flags & 2) != 0) {
        memset(client->ps.hudCurrent, 0, 0xf80);
    }
    for (i = 0; i < 0x400; i++, elem++) {
        if (elem->elem.type != 0 &&
            (elem->team == 0 || elem->team == client->cs.team) &&
            (elem->clientNum == 0x3ff || elem->clientNum == targetClient)) {
            if (elem->archived == 0) {
                if ((flags & 2) != 0) {
                    struct hudelem_s *dst = &client->ps.hudCurrent[unflaggedCount];
                    unflaggedCount++;
                    if (unflaggedCount < 0x20) {
                        memcpy(dst, elem, 0x80);
                    }
                }
            } else if ((flags & 1) != 0) {
                struct hudelem_s *dst = &client->ps.hudArchival[flaggedCount];
                flaggedCount++;
                if (flaggedCount < 0x20) {
                    memcpy(dst, elem, 0x80);
                }
            }
        }
    }
}

// 0x081049ec  size=36  Float_Max_081049ec  status=translated  naming=inferred
// naming: returns the larger of two floats via Float_SelectIfNonNegative_08104a98(a - b, a, b); the result is left in st0, so the wrapper returns float
float Float_Max_081049ec(float a, float b)
{
    return Float_SelectIfNonNegative_08104a98(a - b, a, b);
}

// 0x08104a10  size=36  Float_Min_08104a10  status=translated  naming=inferred
// naming: returns the smaller of two floats via Float_SelectIfNonNegative_08104a98(b - a, a, b)
float Float_Min_08104a10(float a, float b)
{
    return Float_SelectIfNonNegative_08104a98(b - a, a, b);
}

// 0x08104a34  size=54  Float_RoundToInt_08104a34  status=translated  naming=inferred
// naming: floor(x + 0.5) then truncating integer conversion (x87 fistp with chop); round-half-up to int
int Float_RoundToInt_08104a34(float v)
{
    return (int)floor((double)(v + 0.5f));
}

// 0x08104a6a  size=46  Float_CeilToInt  status=translated  naming=inferred
// naming: ceil() and integer conversion; ceil identity on the rounded value
int Float_CeilToInt(float v)
{
    return (int)ceil((double)v);
}

// 0x08104a98  size=41  Float_SelectIfNonNegative_08104a98  status=translated  naming=unknown
// naming: returns ifNonNeg when test >= 0.0 (fucompp/jae), otherwise otherwise; result in st0 as float
float Float_SelectIfNonNegative_08104a98(float test, float ifNonNeg, float otherwise)
{
    float result;

    if (0.0 <= test) {
        result = ifNonNeg;
    } else {
        result = otherwise;
    }
    return result;
}

// 0x08104ac4  size=208  Client_FillClipFromAmmo  status=translated  naming=unknown
// naming: refills the reserve of a weapon's ammo slot from its clip, up to the slot maximum (BG max-reserve lookup); descriptive name
void Client_FillClipFromAmmo(struct gclient_s *client, int weapon)
{
    int clipSlot = BG_GetWeaponAmmoIndex(weapon);
    int reserveSlot = BG_GetWeaponClipIndex(weapon);

    if (weapon > 0) {
        if (weapon <= BG_GetNumWeapons()) {
            int amount = BG_GetClipTypeSize(reserveSlot) - client->ps.ammoclip[reserveSlot];

            if (client->ps.ammo[clipSlot] < amount) {
                amount = client->ps.ammo[clipSlot];
            }
            if (amount != 0) {
                client->ps.ammo[clipSlot] -= amount;
                client->ps.ammoclip[reserveSlot] += amount;
            }
        }
    }
}

// 0x08104b94  size=720  Player_GiveWeaponAmmo  status=translated  naming=unknown
// naming: adds ammo to a weapon's clip slot, reconciles clip/reserve against slot maxima, applies the weapon's ammo-pool delta (ec514) and returns the total change; descriptive name
int Player_GiveWeaponAmmo(struct gentity_s *ent, int weapon, int amount, int forceFlag)
{
    struct gclient_s *client = ent->client;
    int clipSlot = BG_GetWeaponAmmoIndex(weapon);
    int reserveSlot = BG_GetWeaponClipIndex(weapon);
    int clipBefore = client->ps.ammo[clipSlot];
    int reserveBefore = client->ps.ammoclip[reserveSlot];
    int reserveOnly;
    struct weaponDef_s *info;
    int delta;

    client->ps.ammo[clipSlot] += amount;
    reserveOnly = BG_GetWeaponClipOnly(weapon);
    if (reserveOnly != 0) {
        G_TouchTriggerEntity(client, weapon);
    }
    if (forceFlag != 0 || reserveOnly != 0) {
        Client_FillClipFromAmmo(client, weapon);
    }
    if (reserveOnly != 0) {
        client->ps.ammo[clipSlot] = 0;
    } else if (BG_GetAmmoTypeMax(clipSlot) < client->ps.ammo[clipSlot]) {
        client->ps.ammo[clipSlot] = BG_GetAmmoTypeMax(clipSlot);
    }
    if (BG_GetClipTypeSize(reserveSlot) < client->ps.ammoclip[reserveSlot]) {
        client->ps.ammoclip[reserveSlot] = BG_GetClipTypeSize(reserveSlot);
    }

    info = BG_GetWeaponDef(weapon);
    if (info->iSharedAmmoCapIndex > -1) {
        delta = BG_GetWeaponAmmoRoom(&client->ps, weapon);
        if (delta < 0) {
            if (BG_GetWeaponClipOnly(weapon) == 0) {
                client->ps.ammo[clipSlot] += delta;
                if (client->ps.ammo[clipSlot] < 0) {
                    client->ps.ammo[clipSlot] = 0;
                }
            } else {
                client->ps.ammoclip[reserveSlot] += delta;
                if (client->ps.ammoclip[reserveSlot] < 1) {
                    client->ps.ammoclip[reserveSlot] = 0;
                    BG_TakeWeapon(&client->ps, weapon);
                    return 0;
                }
            }
        }
    }
    return (client->ps.ammo[clipSlot] - clipBefore) + (client->ps.ammoclip[reserveSlot] - reserveBefore);
}

// 0x08104e64  size=526  Item_PickupAmmo  status=translated  naming=inferred
// naming: "GAME_PICKUP_AMMO" and "GAME_PICKUP_CLIPONLY_AMMO" message formats at 0x0815a282 / 0x0815a260 name an ammo pickup
int Item_PickupAmmo(struct gentity_s *item, struct gentity_s *player)
{
    const struct gitem_s *def = &g_081850c0[item->u.item.index];
    int weapon = def->weaponIndex;
    int amount = (item->count == 0) ? def->defaultAmount : item->count;
    struct weaponDef_s *info;
    char *msg;
    int result;

    if (Player_GiveWeaponAmmo(player, weapon, amount, 0) == 0) {
        result = 0;
    } else {
        if (BG_GetWeaponClipOnly(weapon) == 0) {
            info = BG_GetWeaponDef(weapon);
            msg = va("%c \"GAME_PICKUP_AMMO\x14%s\"", 0x66, info->displayName);
            SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
        } else {
            info = BG_GetWeaponDef(weapon);
            msg = va("%c \"GAME_PICKUP_CLIPONLY_AMMO\x14%s\"", 0x66, info->displayName);
            SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
        }
        Scr_AddEntityParam(player);
        Scr_NotifyEntity(item, scr_str_trigger, 1);
        info = BG_GetWeaponDef(weapon);
        if (info->offhandClass == 0) {
            msg = va("%c \"%i\"", 0x49, 1);
        } else {
            msg = va("%c \"%i\"", 0x49, 4);
        }
        SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
        result = 1;
    }
    return result;
}

// 0x08105072  size=2381  Item_PickupWeapon  status=translated  naming=inferred
// naming: "GAME_CANT_GET_PRIMARY_WEAP_MESSAGE" and the weapon pickup ammo messages name a weapon pickup handler (item type 1); swaps or drops the held primary weapon when the player cannot carry both
int Item_PickupWeapon(struct gentity_s *item, struct gentity_s *player, int *outEvent, int dropFlag)
{
    struct gclient_s *client = player->client;
    const struct gitem_s *def = &g_081850c0[item->u.item.index];
    int weapon = def->weaponIndex;
    struct weaponDef_s *info = BG_GetWeaponDef(weapon);
    struct weaponDef_s *curInfo;
    struct gentity_s *dropped = 0;
    int clipOut;
    int reserveOut;
    int hasWeapon;
    char *msg;
    int rangeLo;
    int rangeHi;
    int slotIdx;

    /* ---- clip ---- */
    if (item->count < 0) {
        clipOut = 0;
    } else {
        if (item->count == 0) {
            rangeLo = info->dropAmmoMin;
            rangeHi = info->dropAmmoMax;
            if (info->dropAmmoMax < rangeLo) {
                rangeLo = info->dropAmmoMax;
                rangeHi = info->dropAmmoMin;
            }
            if (rangeHi == 0 && rangeLo == 0) {
                /* 0x08105072: (Rand_Fraction() + 1) * 0.5f spilled as a float, rounded through 0x081081ee */
                volatile float frac = (float)((Rand_Fraction() + 1.0L) * (long double)0.5f);
                int reserveMax = BG_GetClipTypeSize(BG_GetWeaponClipIndex(weapon));
                item->count = Float_RoundToInt2((float)((long double)(reserveMax - 1) * (long double)frac)) + 1;
            } else if (rangeHi < 0) {
                item->count = 0;
            } else {
                int pick;
                if (rangeHi == rangeLo) {
                    pick = rangeLo;
                } else {
                    pick = rangeLo + rand() % (rangeHi - rangeLo);
                }
                item->count = pick;
                if (item->count < 1) {
                    item->count = 0;
                }
            }
        }
        if (BG_GetAmmoTypeMax(BG_GetWeaponAmmoIndex(weapon)) < item->count) {
            item->count = BG_GetAmmoTypeMax(BG_GetWeaponAmmoIndex(weapon));
        }
        clipOut = item->count;
    }

    /* ---- reserve ---- */
    if (item->u.item.ammoCount < 0) {
        reserveOut = 0;
    } else {
        if (item->u.item.ammoCount == 0) {
            if (item->count < 0) {
                item->u.item.ammoCount = 0;
            } else {
                item->u.item.ammoCount = BG_GetClipTypeSize(BG_GetWeaponClipIndex(weapon));
                if (item->count < item->u.item.ammoCount) {
                    item->u.item.ammoCount = item->count;
                }
                item->count = item->count - item->u.item.ammoCount;
                clipOut = item->count;
            }
        }
        if (BG_GetClipTypeSize(BG_GetWeaponClipIndex(weapon)) < item->u.item.ammoCount) {
            item->u.item.ammoCount = BG_GetClipTypeSize(BG_GetWeaponClipIndex(weapon));
        }
        reserveOut = item->u.item.ammoCount;
    }

    /* ---- weapon ownership and swap ---- */
    hasWeapon = BitSet_Test(client->ps.weapons, weapon);
    if (hasWeapon == 0) {
        if (BG_WeaponFlagA(weapon) == 0) {
            G_TouchTriggerEntity(client, weapon);
        } else {
            if (client->ps.weapon != 0) {
                if (BitSet_Test(client->ps.weapons, client->ps.weapon) == 0) {
                    return 0;
                }
                if (BG_FindWeaponSlot(&client->ps, client->ps.weapon, 1) == 0) {
                    curInfo = BG_GetWeaponDef(client->ps.weapon);
                    if (BG_PickStackableSlot(&client->ps, client->ps.weapon, curInfo->weaponSlot) == 0 &&
                        BG_FindFreeHeldSlot(&client->ps, weapon) == 0) {
                        Com_Printf("WARNING: cannot swap out a debug weapon (can result from too many weapons given to the player)\n");
                        return 0;
                    }
                }
            }
            if (BG_FindFreeHeldSlot(&client->ps, weapon) == 0) {
                curInfo = BG_GetWeaponDef(client->ps.weapon);
                if (BG_PickStackableSlot(&client->ps, weapon, curInfo->weaponSlot) == 0) {
                    curInfo = BG_GetWeaponDef(client->ps.weapon);
                    if (info->weaponSlot == curInfo->weaponSlot) {
                        dropped = Player_DropWeapon(player, client->ps.weapon, 0);
                    } else {
                        for (slotIdx = 1; slotIdx < 3; slotIdx++) {
                            signed char candidate = (signed char)client->ps.weaponslots[slotIdx];
                            int clipSlot = BG_GetWeaponAmmoIndex(weapon);
                            int reserveSlot = BG_GetWeaponClipIndex(weapon);
                            if (client->ps.ammo[clipSlot] == 0 && client->ps.ammoclip[reserveSlot] == 0) {
                                dropped = Player_DropWeapon(player, (int)candidate, 0);
                                break;
                            }
                        }
                        if (2 < slotIdx) {
                            msg = va("%c \"GAME_CANT_GET_PRIMARY_WEAP_MESSAGE\"", 0x66);
                            SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
                            return 0;
                        }
                    }
                    if (dropped == 0) {
                        return 0;
                    }
                    dropped->spawnflags = item->spawnflags;
                    dropped->s.groundEntityNum = item->s.groundEntityNum;
                    G_SetPositionTrajectory(dropped, item->r.currentOrigin);
                    G_SetAnglesTrajectory(dropped, item->r.currentAngles);
                    SV_LinkEntity(dropped);
                }
            }
            G_TouchTriggerEntity(client, weapon);
            if (dropFlag == 0) {
                G_SendCharIndexA(COD2_ENTNUM(player), weapon);
            }
        }
    }

    /* ---- give the ammo ---- */
    if (hasWeapon == 0) {
        if (reserveOut >= 0) {
            if (BG_GetClipTypeSize(BG_GetWeaponClipIndex(weapon)) < reserveOut) {
                clipOut += reserveOut - BG_GetClipTypeSize(BG_GetWeaponClipIndex(weapon));
                reserveOut = BG_GetClipTypeSize(BG_GetWeaponClipIndex(weapon));
            }
            client->ps.ammoclip[BG_GetWeaponClipIndex(weapon)] = reserveOut;
        }
        Player_GiveWeaponAmmo(player, weapon, clipOut, reserveOut == -1);
    } else {
        int given;

        *outEvent = 0x91;
        given = Player_GiveWeaponAmmo(player, weapon, clipOut + reserveOut, 0);
        if (given != 0) {
            if (BG_GetWeaponClipOnly(weapon) == 0) {
                msg = va("%c \"GAME_PICKUP_AMMO\x14%s\"", 0x66, info->displayName);
            } else {
                msg = va("%c \"GAME_PICKUP_CLIPONLY_AMMO\x14%s\"", 0x66, info->displayName);
            }
            SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
        }
        if (given != clipOut + reserveOut) {
            item->count -= given;
            if (item->count < 1) {
                item->u.item.ammoCount = item->count + item->u.item.ammoCount;
                item->count = -1;
                if (item->u.item.ammoCount < 1) {
                    item->u.item.ammoCount = -1;
                }
            }
            if ((item->count > 0 || item->u.item.ammoCount > 0) && COD2_DVAR_BOOL(g_087a2434) != 0) {
                return 0;
            }
        }
    }

    if (dropped == 0) {
        Scr_AddUndefined();
    } else {
        Scr_AddEntityParam(dropped);
    }
    Scr_AddEntityParam(player);
    Scr_NotifyEntity(item, scr_str_trigger, 2);
    if (info->offhandClass == 0) {
        msg = va("%c \"%i\"", 0x49, 1);
    } else {
        msg = va("%c \"%i\"", 0x49, 4);
    }
    SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
    return 1;
}

// 0x081059c0  size=703  Item_PickupHealth  status=translated  naming=inferred
// naming: "GAME_PICKUP_HEALTH" message format at 0x0815a34c names a health pickup (item type 3)
int Item_PickupHealth(struct gentity_s *item, struct gentity_s *player)
{
    const struct gitem_s *def = &g_081850c0[item->u.item.index];
    struct gclient_s *client = player->client;
    int capHealth;
    int amount;
    int oldHealth;
    int newPct;
    int pct;

    if (def->defaultAmount == 5 || def->defaultAmount == 100) {
        capHealth = client->maxHealth * 2;
    } else {
        capHealth = client->maxHealth;
    }
    amount = (item->count == 0) ? def->defaultAmount : item->count;

    oldHealth = player->health;
    player->health = (int)((float)client->maxHealth * (float)amount * 0.01) + player->health;
    if (capHealth < player->health) {
        player->health = capHealth;
    } else {
        newPct = (int)((long double)(player->health * 100) / (long double)client->maxHealth);
        if (newPct < 1) {
            newPct = 1;
        } else if (100 < newPct) {
            newPct = 100;
        }
        pct = (int)((long double)(oldHealth * 100) / (long double)client->maxHealth);
        if (pct < 1) {
            pct = 1;
        }
        pct = pct + amount;
        if (100 < pct) {
            pct = 100;
        }
        if (newPct != pct) {
            player->health = (pct * client->maxHealth) / 100;
        }
    }
    client->ps.stats[0] = player->health;

    SV_SendServerCommandNum(COD2_ENTNUM(player), 0, va("%c \"GAME_PICKUP_HEALTH\x15%i\"", 0x66, amount));
    SV_SendServerCommandNum(COD2_ENTNUM(player), 0, va("%c \"%i\"", 0x49, 0));
    Scr_AddEntityParam(player);
    Scr_NotifyEntity(item, scr_str_trigger, 1);
    return 1;
}

// 0x08105cac  size=845  Item_TouchPickup  status=translated  naming=inferred
// naming: "Weapon;%d;%d;%s;%s" and "Item;%d;%d;%s;%s" log formats and the "GAME_PICKUP_CANTCARRYMOREAMMO" message name the touch-pickup dispatcher
void Item_TouchPickup(struct gentity_s *item, struct gentity_s *player, int pickupFlags)
{
    int outEvent = 0x90;
    const struct gitem_s *def;
    struct weaponDef_s *info;
    char name[72];
    int handled;
    char *msg;
    int res;

    if (item->active != 0) {
        item->active = 0;
        if (player->client != 0 && player->health > 0 && g_0864c3a0 == 0) {
            def = &g_081850c0[item->u.item.index];
            handled = BG_CanItemBeGrabbed(&item->s, &player->client->ps, pickupFlags);
            if (handled == 0) {
                if (pickupFlags == 0 && item->s.clientNum != player->s.number && def->itemType == 1) {
                    if (BitSet_Test(player->client->ps.weapons, def->weaponIndex) == 0) {
                        info = BG_GetWeaponDef(def->weaponIndex);
                        if ((unsigned int)(info->weaponSlot - 1) < 2) {
                            msg = va("%c \"GAME_CANT_GET_PRIMARY_WEAP_MESSAGE\"", 0x66);
                            SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
                        }
                    } else {
                        info = BG_GetWeaponDef(def->weaponIndex);
                        msg = va("%c \"GAME_PICKUP_CANTCARRYMOREAMMO\x14%s\"", 0x66, info->displayName);
                        SV_SendServerCommandNum(COD2_ENTNUM(player), 0, msg);
                    }
                }
            } else {
                Q_strncpyz(name, player->client->cs.name, 0x40);
                Q_CleanStr(name);
                if (def->itemType == 1) {
                    info = BG_GetWeaponDef(def->weaponIndex);
                    G_LogPrintf("Weapon;%d;%d;%s;%s\n", SV_GetClientGuid(player->s.number),
                                player->s.number, name, info->szInternalName);
                } else {
                    G_LogPrintf("Item;%d;%d;%s;%s\n", SV_GetClientGuid(player->s.number),
                                player->s.number, name, def->itemName);
                }
                if (def->itemType == 2) {
                    res = Item_PickupAmmo(item, player);
                } else if (def->itemType < 3) {
                    if (def->itemType != 1) {
                        return;
                    }
                    res = Item_PickupWeapon(item, player, &outEvent, pickupFlags);
                } else {
                    if (def->itemType != 3) {
                        return;
                    }
                    res = Item_PickupHealth(item, player);
                }
                if (res != 0) {
                    if (player->client->predictItemPickup == 0) {
                        G_AddEvent(player, outEvent, item->s.index);
                    } else {
                        Ent_ForwardToClient(player, outEvent, item->s.index);
                    }
                    G_FreeEntity(item);
                }
            }
        }
    }
}

// 0x08105ffa  size=309  DroppedWeapon_FindSlot  status=translated  naming=unknown
// naming: returns the first free slot of the dropped-weapon list (g_droppedWeapons) or, when the list is full, evicts the entry farthest from every connected client and returns its slot; descriptive name
int DroppedWeapon_FindSlot(void)
{
    int maxCount = COD2_DVAR_INT(g_087a2438);
    int bestSlot = 0;
    float bestDistance = -1.0f;
    int i;

    for (i = 0; i < maxCount; i++) {
        struct gentity_s *dropped = g_droppedWeapons[i];
        float nearest = 9.99998e+11f;
        int c;

        if (dropped == 0) {
            return i;
        }
        for (c = 0; c < level_maxclients; c++) {
            const struct gclient_s *cl = &level[c];

            if (cl->connected == 2 && cl->sessionState == 0) {
                float d = Vec3_DistanceSquared(g_gentities[c].r.currentOrigin, dropped->r.currentOrigin);
                if (d < nearest) {
                    nearest = d;
                }
            }
        }
        if (bestDistance < nearest) {
            bestDistance = nearest;
            bestSlot = i;
        }
    }
    G_FreeEntity(g_droppedWeapons[bestSlot]);
    g_droppedWeapons[bestSlot] = 0;
    return bestSlot;
}

// 0x08106130  size=18  Item_SetOwnerUnset  status=translated  naming=inferred
// naming: stores 0x3fe (the unowned marker used by Item_SetupDroppedWeapon) in the entity owner field at +0x90
void Item_SetOwnerUnset(struct gentity_s *ent)
{
    ent->s.clientNum = 0x3fe;
}

// 0x08106142  size=137  Item_SetModelName  status=translated  naming=inferred
// naming: builds "weapon_<name>" for weapon-type definitions (index beyond the weapon count uses the definition's own string); the destination is the model field at entity +0x168
void Item_SetModelName(const struct gitem_s *def, void *dst)
{
    int index = (int)(def - g_081850c0);

    if (BG_GetNumWeapons() < index) {
        G_SetScrNameRef((uint16 *)dst, (int)def->itemName);
    } else {
        char buf[264];
        const struct weaponDef_s *info = BG_GetWeaponDef(index);

        Com_sprintf(buf, 0x100, "weapon_%s", info->szInternalName);
        G_SetScrNameRef((uint16 *)dst, (int)buf);
    }
}

// 0x081061cc  size=519  Item_SpawnDropped  status=translated  naming=inferred
// naming: "dropped item" spawn: registers the definition, spawns an entity (G_Spawn), records it in the dropped-weapon list and sets its bounds, owner, model and timers
struct gentity_s *Item_SpawnDropped(const struct gitem_s *def, const float *origin, const float *dir, int owner)
{
    int index = (int)(def - g_081850c0);
    struct gentity_s *ent;
    int slot;

    ItemRegistry_Register(index, 1);
    ent = G_Spawn();
    slot = DroppedWeapon_FindSlot();
    g_droppedWeapons[slot] = ent;
    ent->s.eType = 3;
    ent->s.index = index;
    Item_SetModelName(def, &ent->classname);
    ent->u.item.index = (unsigned short)index;
    if (def->itemType == 1) {
        Vec3Set(ent->r.mins, -1.0f, -1.0f, -1.0f);
        Vec3Set(ent->r.maxs, 1.0f, 1.0f, 1.0f);
    } else {
        Vec3Set(ent->r.mins, -1.0f, -1.0f, 0.0f);
        Vec3Set(ent->r.maxs, 1.0f, 1.0f, 2.0f);
    }
    ent->r.contents = 0x405c0108u;
    if (def->itemType != 2) {
        ent->r.contents |= 0x200000u;
    }
    ent->s.clientNum = owner;
    Ent_SetModelByName(ent, def->model);
    Ent_RefreshAttachments(ent);
    ent->handler = 0xf;
    G_SetPositionTrajectory(ent, origin);
    ent->s.pos.trType = 5;
    ent->s.pos.trTime = level_time;
    Vec3Copy(dir, ent->s.pos.trDelta);
    ent->nextthink = level_time + 1000;
    ent->flags = 0x10;
    SV_LinkEntity(ent);
    return ent;
}

// 0x081063d4  size=265  Item_ThrowFromPlayer  status=translated  naming=inferred
// naming: builds the throw direction from the player's view angles (AngleVectors, g_dropForwardSpeed / g_dropUpSpeedBase / g_dropUpSpeedRand dvars) and spawns the dropped item at the player's box top; returns the spawned entity (eax passthrough of Item_SpawnDropped)
struct gentity_s *Item_ThrowFromPlayer(struct gentity_s *player, const struct gitem_s *def, float forward, int dropMode)
{
    float angles[3];
    float dir[3];
    float origin[3];

    Vec3Copy(player->r.currentAngles, angles);
    angles[1] = angles[1] + forward;
    angles[0] = 0.0f;
    angles[2] = 0.0f;

    if (dropMode == 0) {
        Angles_ToVectors(angles, dir, 0, 0);
        Vec3Scale(dir, COD2_DVAR_FLOAT(g_087a2448), dir);
        /* 0x081063d4: Rand_Signed() st(0) * dvar(0x087a2450) + dvar(0x087a244c) is added to dir[2] unrounded */
        dir[2] = (float)((long double)dir[2] +
                         (Rand_Signed() * (long double)COD2_DVAR_FLOAT(g_087a2450) +
                          (long double)COD2_DVAR_FLOAT(g_087a244c)));
    } else {
        Vec3Clear(dir);
    }

    Vec3Copy(player->r.currentOrigin, origin);
    origin[2] = (float)(origin[2] +
                        ((float)player->r.maxs[2] - (float)player->r.mins[2]) * 0.5);
    return Item_SpawnDropped(def, origin, dir, player->s.number);
}

// 0x081064de  size=1457  Player_DropWeapon  status=translated  naming=unknown
// naming: drops a weapon from the player (or builds a dropped entity for a client-less player) carrying the clip and reserve amounts; optionally places it at the aim point (Ent_SetNodeFromSlot aim record); descriptive name
struct gentity_s *Player_DropWeapon(struct gentity_s *player, int weapon, int withAim)
{
    const struct gitem_s *def = &g_081850c0[weapon];
    struct gclient_s *client = player->client;
    int clip = 0;
    int reserve = 0;
    int clipSlot;
    int reserveSlot;
    struct weaponDef_s *info;
    struct gentity_s *ent;

    if (client != 0 && BitSet_Test(client->ps.weapons, weapon) == 0) {
        BG_TakeWeapon(&client->ps, weapon);
        return 0;
    }

    clipSlot = BG_GetWeaponAmmoIndex(weapon);
    reserveSlot = BG_GetWeaponClipIndex(weapon);
    info = BG_GetWeaponDef(weapon);
    if (info->clipOnly != 0 && client->ps.ammoclip[reserveSlot] == 0) {
        BG_TakeWeapon(&client->ps, weapon);
        return 0;
    }

    ent = Item_ThrowFromPlayer(player, def, 0.0f, 0);
    if (client == 0) {
        int hi = info->dropAmmoMax;
        int lo = info->dropAmmoMin;

        if (hi < lo) {
            hi = lo;
            lo = info->dropAmmoMax;
        }
        if (hi == 0 && lo == 0) {
            /* 0x081066c6..0x08106720: (Rand_Fraction() + 1) * 0.5f is spilled as a float; the second fraction stays
             * on the x87 stack ((r * 0.5f + 0.25f) * clip); both roundings call 0x081081ee */
            volatile float frac = (float)((Rand_Fraction() + 1.0L) * (long double)0.5f);
            int reserveMax = BG_GetClipTypeSize(reserveSlot);

            clip = Float_RoundToInt2((float)((long double)(reserveMax - 1) * (long double)frac)) + 1;
            reserve = Float_RoundToInt2((float)((Rand_Fraction() * (long double)0.5f + (long double)0.25f) * (long double)clip));
            clip = clip - reserve;
        } else if (hi < 0) {
            clip = 0;
            reserve = 0;
        } else {
            int pick;
            int dc;
            int maxReserve;

            if (hi == lo) {
                pick = lo;
            } else {
                pick = lo + rand() % (hi - lo);
            }
            clip = pick;
            if (clip < 1) {
                clip = 0;
                reserve = 0;
            } else {
                maxReserve = BG_GetClipTypeSize(reserveSlot);
                if (maxReserve == 0) {
                    dc = 0;
                } else {
                    dc = rand();
                    dc = dc % maxReserve;
                }
                reserve = dc;
                if (dc < clip) {
                    clip = clip - dc;
                } else {
                    reserve = clip;
                    clip = 0;
                }
            }
        }
    } else {
        clip = client->ps.ammo[clipSlot];
        client->ps.ammo[clipSlot] = 0;
        reserve = client->ps.ammoclip[reserveSlot];
        client->ps.ammoclip[reserveSlot] = 0;
        BG_TakeWeapon(&client->ps, weapon);
    }

    ent->count = clip;
    ent->u.item.ammoCount = reserve;
    if (ent->count == 0) {
        ent->count = -1;
    }
    if (ent->u.item.ammoCount == 0) {
        ent->u.item.ammoCount = -1;
    }
    if (clip == 0 && reserve == 0) {
        ent->r.contents &= ~0x200000u;
    }

    if (withAim != 0) {
        float aim[12];          /* axis[3][3] followed by the aim end point (aim + 9) */
        float angles[3];
        float center[3];
        struct trace_s tr;

        if (Ent_SetNodeFromSlot(player, withAim, (int)aim) == 0) {
            Vec3Copy(player->r.currentAngles, angles);
        } else {
            Vec3Average(player->r.mins, player->r.maxs, center);
            Vec3Add(center, player->r.currentOrigin, center);
            G_TraceBox(&tr, center, ent->r.mins, ent->r.maxs, aim + 9, player->s.number,
                        Ent_GetTraceMask(ent));
            VectorLerp(center, aim + 9, tr.fraction, ent->s.pos.trBase);
            Vec3Copy(ent->s.pos.trBase, ent->r.currentOrigin);
            ent->s.pos.trTime = level_time;
            AxisToAngles(aim, angles);
        }
        angles[2] = (float)(angles[2] + 90.0);
        G_SetAnglesTrajectory(ent, player->r.currentAngles);
    /* gameplay-fix(wpn_rockets): 0x08106890 (cmp [ebp+0x10],0; je 0x08106a7e) skips the angular-spread block too when withAim == 0; it was outside the if and burned three rand() calls per pickup swap */
    {
        float spread[3];
        /* 0x081069fe..0x08106a45: each Rand_Signed() st(0) result is multiplied by a float constant before any
         * store; z (60) and y (40) are spilled as floats, x (50) stays on the x87 stack until the call */
        volatile float z = (float)(Rand_Signed() * (long double)60.0f);
        volatile float y = (float)(Rand_Signed() * (long double)40.0f);
        long double x = Rand_Signed() * (long double)50.0f;

        Vec3Set(spread, (float)x, y, z);
        ent->s.apos.trType = 2;
        ent->s.apos.trTime = level_time;
        Vec3Copy(spread, ent->s.apos.trDelta);
    }
    }
    return ent;
}

// 0x08106e92  size=46  ItemRegistry_Reset  status=translated  naming=inferred
// naming: clears the 256-entry item registration table and marks entry 0 as registered
void ItemRegistry_Reset(void)
{
    memset(g_itemRegisteredFlags, 0, 0x400);
    g_itemRegisteredFlags[0] = 1;
}

// 0x08106ec0  size=188  ItemRegistry_BuildNameList  status=translated  naming=inferred
// naming: concatenates the weapon names (space separated, indices 1..count) into a 0x2000-byte buffer and sends it as configstring 7
void ItemRegistry_BuildNameList(void)
{
    char buf[0x2008];
    const struct weaponDef_s *prev = 0;
    int i;

    g_0864f97c = 0;
    buf[0] = 0;
    for (i = 1; i <= BG_GetNumWeapons(); i++) {
        if (prev != 0) {
            I_strncat(buf, 0x2000, " ");
        }
        prev = BG_GetWeaponDef(i);
        I_strncat(buf, 0x2000, prev->szInternalName);
    }
    SV_SetConfigstring(7, buf);
}

// 0x08106f7c  size=408  ItemRegistry_BuildBitmask  status=translated  naming=inferred
// naming: encodes the 131 item registration flags as a hex string (4 flags per digit, digits 0-9 then lowercase letters) and sends it as configstring 8
void ItemRegistry_BuildBitmask(void)
{
    char out[280];
    int outLen = 0;
    int nibble = 0;
    int bitPos = 0;
    int i;

    g_0864f980 = 0;
    for (i = 0; i < 0x83; i++) {
        if (g_itemRegisteredFlags[i] != 0) {
            nibble += 1 << (bitPos & 0x1f);
        }
        bitPos++;
        if (bitPos == 4) {
            out[outLen++] = (nibble < 10) ? (char)(nibble + '0') : (char)(nibble + 'W');
            nibble = 0;
            bitPos = 0;
        }
    }
    if (bitPos != 0) {
        out[outLen++] = (nibble < 10) ? (char)(nibble + '0') : (char)(nibble + 'W');
    }
    out[outLen] = 0;
    SV_SetConfigstring(8, out);
}

// 0x08107114  size=257  ItemRegistry_Register  status=translated  naming=inferred
// naming: "game tried to register the item '%s' after initialization finished" names the registration check; sets the registry flag and takes references on the definition's model slots
void ItemRegistry_Register(int index, int setDirty)
{
    if (g_itemRegisteredFlags[index] == 0) {
        if (level_initializing == 0) {
            const char *name = g_081850c0[index].name;

            if (name == 0 || *name == '\0') {
                name = "<<unknown>>";
            }
            Scr_Error(va("game tried to register the item '%s' after initialization finished\n", name));
        }
        g_itemRegisteredFlags[index] = 1;
        if (g_081850c0[index].model != 0) {
            G_ModelIndex(g_081850c0[index].model);
        }
        if (g_081850c0[index].model2 != 0) {
            G_ModelIndex(g_081850c0[index].model2);
        }
        if (setDirty != 0) {
            g_0864f980 = 1;
        }
    }
}

// 0x08107332  size=15  ItemRegistry_IsRegistered  status=translated  naming=inferred
// naming: returns the registration flag of one item index
int ItemRegistry_IsRegistered(int index)
{
    return g_itemRegisteredFlags[index];
}

// 0x08107342  size=588  Item_SetupDroppedWeapon  status=translated  naming=inferred
// naming: initialises an entity as a dropped weapon from its definition (registration, bounds, owner marker 0x3fe, angles roll +90 for weapon-type items) and links it
void Item_SetupDroppedWeapon(struct gentity_s *ent, const struct gitem_s *def)
{
    int index = (int)(def - g_081850c0);

    ItemRegistry_Register(index, 0);
    ent->u.item.index = (unsigned short)index;
    Ent_SetModelByName(ent, def->model);
    if (def->itemType == 1) {
        Vec3Set(ent->r.mins, -1.0f, -1.0f, -1.0f);
        Vec3Set(ent->r.maxs, 1.0f, 1.0f, 1.0f);
    } else {
        Vec3Set(ent->r.mins, -1.0f, -1.0f, 0.0f);
        Vec3Set(ent->r.maxs, 1.0f, 1.0f, 2.0f);
    }
    ent->r.contents = 0x405c0108u;
    if (def->itemType != 2) {
        ent->r.contents |= 0x200000u;
    }
    ent->s.eType = 3;
    ent->s.index = (int)ent->u.item.index;
    Ent_RefreshAttachments(ent);
    Item_SetOwnerUnset(ent);
    ent->flags |= 0x1000u;
    if (*(byte *)&g_spawnVars == 0) {
        ent->handler = 0x11;
        if ((ent->spawnflags & 1) == 0) {
            ent->s.groundEntityNum = 0x3ff;
            if (def->itemType == 1) {
                ent->r.currentAngles[2] = (float)(ent->r.currentAngles[2] + 90.0);
            }
        }
        G_SetAnglesTrajectory(ent, ent->r.currentAngles);
        G_SetPositionTrajectory(ent, ent->r.currentOrigin);
        SV_LinkEntity(ent);
    } else {
        G_SetAnglesTrajectory(ent, ent->r.currentAngles);
        ent->nextthink = level_time + 100;
        ent->handler = 0x10;
    }
}

// 0x0810758e  size=428  G_CorpseApplyTrace  status=partial  naming=unknown
// naming: applies a trace result to a moving entity: either a grounded/blocked reset (state 1, velocity stored, angles from the trace normal) or a step to the trace end point; trace record layout partial (endpos at +0x04, allsolid at +0x22)
void G_CorpseApplyTrace(struct gentity_s *ent, struct ent_ext_s *ext, const struct trace_s *tr, const float *vel)
{
    /* the three vectors are one contiguous 3x3 axis in the original frame ([ebp-0x38]: fwd, +0xc cross, +0x18 normal); AxisToAngles reads all nine floats */
    float axis[9];
    float *fwd = axis;
    float *cross = axis + 3;
    float *endpos = axis + 6;
    float ang[3];

    Vec3Clear(ent->s.pos.trDelta);
    if (tr->allsolid != 0 || 0.0 < tr->normal[2]) {
        ext->doneFlag = 0;
        ent->s.pos.trType = 1;
        Vec3Copy(vel, ent->s.pos.trBase);
        ent->s.pos.trTime = 0;
        ent->s.pos.trDuration = 0;
        Vec3Clear(ent->s.pos.trDelta);
        ent->s.groundEntityNum = (unsigned int)tr->entityNum;
        if (tr->allsolid == 0) {
            Vec3Copy(tr->normal, endpos);
            Angles_ToVectors(ent->r.currentAngles, fwd, 0, 0);
            Vec3_Cross(endpos, fwd, cross);
            Vec3_Cross(cross, endpos, fwd);
            AxisToAngles(fwd, ang);
            G_SetAnglesTrajectory(ent, ang);
        } else {
            G_SetAnglesTrajectory(ent, ent->r.currentAngles);
        }
        SV_LinkEntity(ent);
    } else {
        Vec3Add(ent->r.currentOrigin, tr->normal, ent->r.currentOrigin);
        Vec3Copy(ent->r.currentOrigin, ent->s.pos.trBase);
        ent->s.pos.trTime = level_time;
    }
}

// 0x0810773a  size=215  G_CorpseAlignToTrace  status=translated  naming=unknown
// naming: derives the entity's new angles from the trace end point and its forward axis (cross products, AxisToAngles); weapon-type items (itemType 1) get roll +90
void G_CorpseAlignToTrace(struct gentity_s *ent, const struct trace_s *tr)
{
    /* one contiguous 3x3 axis in the original frame ([ebp-0x38]: fwd, +0xc cross, +0x18 normal) */
    float axis[9];
    float *fwd = axis;
    float *cross = axis + 3;
    float *vec = axis + 6;
    float ang[3];

    Vec3Copy(tr->normal, vec);
    Angles_ToVectors(ent->r.currentAngles, fwd, 0, 0);
    Vec3_Cross(vec, fwd, cross);
    Vec3_Cross(cross, vec, fwd);
    AxisToAngles(fwd, ang);
    if (g_081850c0[ent->s.index].itemType == 1) {
        ang[2] = (float)(ang[2] + 90.0);
    }
    G_SetAnglesTrajectory(ent, ang);
}

// 0x08107812  size=51  Ext_ReadSlotHandle  status=translated  naming=unknown
// naming: queries the slot's stored handle through XAnim_BlendRootNode and writes the result to the caller's out pointer; descriptive name
void Ext_ReadSlotHandle(struct gentity_s *ent, struct ent_ext_s *ext, void *out)
{
    char scratch[24];

    XAnim_BlendRootNode((XAnimTree *)ext->handle_00, 0, (float *)scratch, (float *)out, 1);
}

// 0x08107846  size=1257  G_UpdateCorpseSlot  status=partial  naming=unknown
// naming: per-slot projectile update: reads the slot's velocity (Ext_ReadSlotHandle), steps the entity through a trace, and resolves impacts (G_CorpseApplyTrace / G_CorpseAlignToTrace); trace layout inferred from the decompiler's out-buffer offsets, partial
void G_UpdateCorpseSlot(struct gentity_s *ent)
{
    int slotIndex = G_FindTrackedSlot(ent);
    struct ent_ext_s *ext = &g_trackedEntitySlots[slotIndex];
    int moving = 0;
    float vel[3];
    float pos[3];
    float fwd[3];
    float right[3];
    float upDir[3];
    float out6c[3];
    struct trace_s tr;
    int hit;
    unsigned int mask;

    Ext_ReadSlotHandle(ent, ext, vel);
    if (ext->doneFlag == 0 && 1.0L < Vec3LengthSquared(vel)) {
        moving = 1;
    }
    if (ext->doneFlag != 0 || moving != 0) {
        BG_EvaluateTrajectory(&ent->s.pos, level_time, pos);
        if (moving != 0) {
            Angles_ToVectors(ent->r.currentAngles, fwd, right, 0);
            Vec3Scale(right, -1.0f, upDir);
            Vec3_Normalize(fwd);
            Vec3_Normalize(upDir);
            Vec3MulAdd(pos, vel[0], fwd, pos);
            Vec3MulAdd(pos, vel[1], upDir, pos);
        }
        mask = Ent_GetTraceMask(ent);
        G_TraceBox(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, pos, ent->r.ownerNum, mask);
        VectorLerp(ent->r.currentOrigin, pos, tr.fraction, out6c);
        Vec3Copy(out6c, ent->r.currentOrigin);
        if (tr.startsolid != 0) {
            tr.fraction = 0.0f;
        }
        SV_LinkEntity(ent);
        G_RunThink(ent);
        if (ent->r.inuse != 0) {
            if (tr.fraction == 1.0f) {
                if (ext->doneFlag == 0 && moving != 0) {
                    ent->s.pos.trType = 1;
                    Vec3Copy(out6c, ent->s.pos.trBase);
                    ent->s.pos.trTime = 0;
                    ent->s.pos.trDuration = 0;
                    Vec3Clear(ent->s.pos.trDelta);
                    pos[2] = pos[2] - 1.0f;
                    if (G_TraceIsClearBox(ent->r.currentOrigin, ent->r.mins, ent->r.maxs, pos, ent->r.ownerNum, mask) != 0) {
                        ext->doneFlag = 1;
                        ent->s.pos.trType = 5;
                        Vec3Copy(out6c, ent->s.pos.trBase);
                        Vec3Clear(ent->s.pos.trDelta);
                        Vec3MulAdd(ent->s.pos.trDelta, vel[0], fwd, ent->s.pos.trDelta);
                        Vec3MulAdd(ent->s.pos.trDelta, vel[1], upDir, ent->s.pos.trDelta);
                        ent->s.pos.trTime = level_time;
                        ent->s.pos.trDuration = 0;
                    }
                }
            } else {
                hit = SV_PointContents(ent->r.currentOrigin, -1, 0x80000000u);
                if (hit == 0) {
                    if (ext->doneFlag != 0) {
                        if (tr.allsolid != 0) {
                            float start[3];
                            Vec3Copy(ent->r.currentOrigin, start);
                            start[2] = start[2] + 64.0f;
                            G_TraceBox(&tr, start, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, ent->r.ownerNum,
                                        mask & 0xfffeffffu);
                            /* gameplay-fix(gt_hq): 0x08107cce..0x08107d08 after the lifted retrace (!allsolid), the original lerps start->currentOrigin by tr.fraction into out6c and copies it to currentOrigin */
                            if (tr.allsolid == 0) {
                                VectorLerp(start, ent->r.currentOrigin, tr.fraction, out6c);
                                Vec3Copy(out6c, ent->r.currentOrigin);
                            }
                        }
                        G_CorpseApplyTrace(ent, ext, &tr, out6c);
                    }
                } else {
                    G_FreeEntity(ent);
                }
            }
        }
    }
}

// 0x08107d30  size=139  Ent_UpdateDObj  status=translated  naming=unknown
// naming: refreshes the entity's server DObj (Com_GetServerDObj) and its per-slot animation block (slot record +0x0c)
void Ent_UpdateDObj(struct gentity_s *ent)
{
    int slot = G_FindTrackedSlot(ent);
    struct DObj *obj = Com_GetServerDObj(ent->s.number);

    PlayerAnim_UpdateTree((int)obj, &ent->s, (struct clientInfo_s *)((char *)&g_trackedEntitySlots[slot] + 0x0c), 0);
    obj = Com_GetServerDObj(ent->s.number);
    if (obj != 0) {
        PlayerView_Update((unsigned int)obj, &ent->s, (struct clientInfo_s *)((char *)&g_trackedEntitySlots[slot] + 0x0c));
    }
}

// 0x08107dbc  size=41  G_RunCorpse  status=translated  naming=unknown
// naming: one projectile think step: slot update, DObj refresh, then the common entity pass
void G_RunCorpse(struct gentity_s *ent)
{
    G_UpdateCorpseSlot(ent);
    Ent_UpdateDObj(ent);
    G_RunThink(ent);
}

// 0x08107de6  size=1032  Ent_StepMovement  status=partial  naming=unknown
// naming: per-frame movement of a moving entity: follows the target entity, steps toward the next position, slides along surfaces (second trace) and settles or frees the entity; trace record layout inferred, partial
void Ent_StepMovement(struct gentity_s *ent)
{
    float pos[3];
    float out6c[3];
    float toPos[3];
    struct trace_s tr;
    int hit;
    unsigned int mask;

    if ((ent->s.groundEntityNum == 0x3ff || level_gentities[ent->s.groundEntityNum].s.pos.trType != 0) &&
        ent->s.pos.trType != 5 && (ent->spawnflags & 1) == 0) {
        ent->s.pos.trType = 5;
        ent->s.pos.trTime = level_time;
        Vec3Copy(ent->r.currentOrigin, ent->s.pos.trBase);
        Vec3Clear(ent->s.pos.trDelta);
    }

    if (ent->s.pos.trType == 0 || ent->s.pos.trType == 6 || ent->tagInfo != 0) {
        G_RunThink(ent);
    } else {
        BG_EvaluateTrajectory(&ent->s.pos, level_time + 0x32, pos);
        mask = Ent_GetTraceMask(ent);
        if ((long double)Vec3_DistanceSquared(ent->r.currentOrigin, pos) < (long double)0.1) {
            pos[2] = pos[2] - 1.0f;
        }
        G_TraceBox(&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, pos, ent->r.ownerNum, mask);
        if (tr.fraction < 1.0f) {
            VectorLerp(ent->r.currentOrigin, pos, tr.fraction, out6c);
            if (tr.startsolid == 0 && tr.fraction < 0.01f && tr.normal[2] < 0.5f) {
                long double dot;

                Vec3Sub(pos, ent->r.currentOrigin, toPos);
                dot = Vec3Dot(toPos, tr.normal);
                Vec3MulAdd(pos, (float)(1.0L - dot), tr.normal, pos);
                G_TraceBox(&tr, out6c, ent->r.mins, ent->r.maxs, pos, ent->r.ownerNum, mask);
                VectorLerp(out6c, pos, tr.fraction, out6c);
            }
            ent->s.pos.trType = 3;
            ent->s.pos.trTime = level_time;
            ent->s.pos.trDuration = 0x32;
            Vec3Copy(ent->r.currentOrigin, ent->s.pos.trBase);
            Vec3Sub(out6c, ent->r.currentOrigin, ent->s.pos.trDelta);
            Vec3Scale(ent->s.pos.trDelta, 20.0f, ent->s.pos.trDelta);
            Vec3Copy(out6c, ent->r.currentOrigin);
        } else {
            Vec3Copy(pos, ent->r.currentOrigin);
        }

        SV_LinkEntity(ent);
        G_RunThink(ent);
        if (ent->r.inuse != 0 && tr.fraction < 0.01f) {
            if (tr.normal[2] <= 0.0f) {
                G_FreeEntity(ent);
            } else {
                hit = SV_PointContents(ent->r.currentOrigin, -1, 0x80000000u);
                if (hit == 0) {
                    G_CorpseAlignToTrace(ent, &tr);
                    G_SetPositionTrajectory(ent, out6c);
                    ent->s.groundEntityNum = (unsigned int)tr.entityNum;
                    SV_LinkEntity(ent);
                } else {
                    G_FreeEntity(ent);
                }
            }
        }
    }
}

// 0x081081ee  size=54  Float_RoundToInt2  status=translated  naming=inferred
// naming: identical body to Float_RoundToInt_08104a34 (0x08104a34): floor(x + 0.5) truncated to int
int Float_RoundToInt2(float v)
{
    return (int)floor((double)(v + 0.5f));
}

// 0x08108224  size=41  Vec3Clear  status=translated  naming=inferred
// naming: zeroes a 3-float vector
void Vec3Clear(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

// 0x0810824e  size=35  Vec3Set  status=translated  naming=inferred
// naming: stores three components
void Vec3Set(float *v, float x, float y, float z)
{
    /* integer dword moves as in the original (no x87 load/store: signalling NaN payloads survive) */
    ((volatile uint32 *)v)[0] = *(volatile uint32 *)&x;
    ((volatile uint32 *)v)[1] = *(volatile uint32 *)&y;
    ((volatile uint32 *)v)[2] = *(volatile uint32 *)&z;
}

// 0x08108272  size=47  Vec3Copy  status=translated  naming=inferred
// naming: copies a 3-float vector
void Vec3Copy(const float *src, float *dst)
{
    /* integer dword moves as in the original (no x87 load/store) */
    ((volatile uint32 *)dst)[0] = ((const volatile uint32 *)src)[0];
    ((volatile uint32 *)dst)[1] = ((const volatile uint32 *)src)[1];
    ((volatile uint32 *)dst)[2] = ((const volatile uint32 *)src)[2];
}

// 0x081082a2  size=68  Vec3Add  status=translated  naming=inferred
// naming: componentwise sum
void Vec3Add(const float *a, const float *b, float *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

// 0x081082e6  size=68  Vec3Sub  status=translated  naming=inferred
// naming: componentwise difference a - b
void Vec3Sub(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

// 0x0810832a  size=92  Vec3Average  status=translated  naming=inferred
// naming: componentwise midpoint (a + b) * 0.5 (double multiply, float store)
void Vec3Average(const float *a, const float *b, float *out)
{
    out[0] = (float)((a[0] + b[0]) * 0.5);
    out[1] = (float)((a[1] + b[1]) * 0.5);
    out[2] = (float)((a[2] + b[2]) * 0.5);
}

// 0x08108386  size=56  Vec3Scale  status=translated  naming=inferred
// naming: out = s * a
void Vec3Scale(const float *a, float s, float *out)
{
    out[0] = s * a[0];
    out[1] = s * a[1];
    out[2] = s * a[2];
}

// 0x081083be  size=83  Vec3MulAdd  status=translated  naming=inferred
// naming: out = a + s * b
void Vec3MulAdd(const float *a, float s, const float *b, float *out)
{
    out[0] = a[0] + s * b[0];
    out[1] = a[1] + s * b[1];
    out[2] = a[2] + s * b[2];
}

// 0x08108412  size=51  Vec3Dot  status=translated  naming=inferred
// naming: dot product evaluated in long double, summed z, y, x
long double Vec3Dot(const float *a, const float *b)
{
    return (long double)a[2] * (long double)b[2] +
           (long double)a[1] * (long double)b[1] +
           (long double)a[0] * (long double)b[0];
}

// 0x08108446  size=51  Vec3LengthSquared  status=translated  naming=inferred
// naming: squared length, same evaluation order as Vec3Dot
long double Vec3LengthSquared(const float *a)
{
    return (long double)a[2] * (long double)a[2] +
           (long double)a[1] * (long double)a[1] +
           (long double)a[0] * (long double)a[0];
}

// 0x0810847a  size=42  Ent_GetTraceMask  status=translated  naming=inferred
// naming: returns the entity trace mask, defaulting to 0x491 when unset
unsigned int Ent_GetTraceMask(const struct gentity_s *ent)
{
    unsigned int mask = ent->clipmask;

    if (ent->clipmask == 0) {
        mask = 0x491;
    }
    return mask;
}

// 0x081084a4  size=2377  G_RegisterDvars  status=translated  naming=inferred
// naming: the dvar names registered here (sv_cheats, g_gametype, g_log, g_speed, ...) are the server cvar set; the function is the dvar init block (it ends with the call to Player_RegisterDvars); inferred from the registration strings, which do not name the function
void G_RegisterDvars(void)
{
    g_087a2418 = Dvar_RegisterBool("sv_cheats", 0, 0x1000);
    Dvar_RegisterString("gamename", "Call of Duty 2", 0x1044);
    Dvar_RegisterString("gamedate", "Jun 23 2006", 0x1040);
    Dvar_RegisterString("sv_mapname", "", 0x1044);
    g_087a2400 = Dvar_RegisterString("g_gametype", "dm", 0x1024);
    g_087a2408 = Dvar_RegisterInt("sv_maxclients", 0x14, 1, 0x40, 0x1025);
    g_087a2440 = Dvar_RegisterBool("g_synchronousClients", 0, 0x1008);
    g_087a246c = Dvar_RegisterString("g_log", "games_mp.log", 0x1001);
    g_087a2470 = Dvar_RegisterBool("g_logSync", 0, 0x1001);
    g_087a2404 = Dvar_RegisterString("g_password", "", 0x1000);
    g_087a2484 = Dvar_RegisterString("g_banIPs", "", 0x1001);
    g_087a240c = Dvar_RegisterInt("dedicated", 0, 0, 2, 0x1020);
    if (COD2_DVAR_INT(g_087a240c) != 0) {
        Dvar_RegisterInt("dedicated", 0, 0, 2, 0x1040);
    }
    dvar_g_speed = Dvar_RegisterInt("g_speed", 190, (int)0x80000000u, 0x7fffffff, 0x1000);
    g_087a2414 = Dvar_RegisterFloat("g_gravity", 800.0f, 1.0f, 3.40282346e+38f, 0x1000);
    g_087a241c = Dvar_RegisterFloat("g_knockback", 1000.0f, -3.40282346e+38f, 3.40282346e+38f, 0x1000);
    g_087a2434 = Dvar_RegisterBool("g_weaponAmmoPools", 0, 0x1000);
    g_087a2438 = Dvar_RegisterInt("g_maxDroppedWeapons", 0x10, 1, 0x20, 0x1000);
    g_087a2428 = Dvar_RegisterInt("g_inactivity", 0, 0, 0x7fffffff, 0x1000);
    g_087a242c = Dvar_RegisterBool("g_debugDamage", 0, 0x1080);
    g_087a2430 = Dvar_RegisterInt("g_debugBullets", 0, -3, 6, 0x1080);
    g_087a243c = Dvar_RegisterString("g_motd", "", 0x1000);
    g_087a2444 = Dvar_RegisterInt("g_playerCollisionEjectSpeed", 0x19, 0, 32000, 0x1001);
    g_087a2448 = Dvar_RegisterFloat("g_dropForwardSpeed", 10.0f, 0.0f, 1000.0f, 0x1001);
    g_087a244c = Dvar_RegisterFloat("g_dropUpSpeedBase", 10.0f, 0.0f, 1000.0f, 0x1001);
    g_087a2450 = Dvar_RegisterFloat("g_dropUpSpeedRand", 5.0f, 0.0f, 1000.0f, 0x1001);
    g_087a2454 = Dvar_RegisterFloat("g_clonePlayerMaxVelocity", 80.0f, 0.0f, 3.40282346e+38f, 0x1001);
    g_087a2460 = Dvar_RegisterBool("voice_global", 0, 0x1001);
    g_087a245c = Dvar_RegisterBool("voice_localEcho", 0, 0x1001);
    g_087a2464 = Dvar_RegisterBool("voice_deadChat", 0, 0x1001);
    dvar_g_allowVote = Dvar_RegisterBool("g_allowVote", 1, 0x1000);
    g_087a2480 = Dvar_RegisterBool("g_listEntity", 0, 0x1000);
    g_087a247c = Dvar_RegisterBool("g_deadChat", 0, 0x1001);
    g_087a2468 = Dvar_RegisterInt("g_voiceChatTalkingDuration", 500, 0, 10000, 0x1001);
    g_087a2490 = Dvar_RegisterString("g_ScoresBanner_Allies", "mpflag_american", 0x1100);
    g_087a2494 = Dvar_RegisterString("g_ScoresBanner_Axis", "mpflag_german", 0x1100);
    g_087a2498 = Dvar_RegisterString("g_ScoresBanner_None", "mpflag_none", 0x1100);
    g_087a249c = Dvar_RegisterString("g_ScoresBanner_Spectators", "mpflag_spectator", 0x1100);
    g_087a24a0 = Dvar_RegisterString("g_TeamName_Allies", "GAME_ALLIES", 0x1100);
    g_087a24a4 = Dvar_RegisterString("g_TeamName_Axis", "GAME_AXIS", 0x1100);
    g_087a24a8 = Dvar_RegisterColor("g_TeamColor_Allies", 0.5f, 0.5f, 1.0f, 1.0f, 0x1100);
    g_087a24ac = Dvar_RegisterColor("g_TeamColor_Axis", 1.0f, 0.5f, 0.5f, 1.0f, 0x1100);
    g_087a248c = Dvar_RegisterBool("g_smoothClients", 1, 0x1000);
    cv_g_antilag = Dvar_RegisterBool("g_antilag", 1, 0x1005);
    dvar_voteMode = Dvar_RegisterBool("g_oldVoting", 1, 0x1001);
    g_087a24d4 = Dvar_RegisterFloat("g_voteAbstainWeight", 0.5f, 0.0f, 1.0f, 0x1001);
    g_087a24b0 = Dvar_RegisterBool("g_no_script_spam", 0, 0x1000);
    g_087a24b4 = Dvar_RegisterBool("g_debugLocDamage", 0, 0x1080);
    g_087a24b8 = Dvar_RegisterFloat("g_friendlyfireDist", 256.0f, 0.0f, 15000.0f, 0x1080);
    g_087a24bc = Dvar_RegisterFloat("g_friendlyNameDist", 15000.0f, 0.0f, 15000.0f, 0x1080);
    g_087a24c0 = Dvar_RegisterFloat("player_meleeRange", 64.0f, 0.0f, 1000.0f, 0x1080);
    g_087a24c4 = Dvar_RegisterFloat("player_meleeWidth", 10.0f, 0.0f, 1000.0f, 0x1080);
    g_087a24c8 = Dvar_RegisterFloat("player_meleeHeight", 10.0f, 0.0f, 1000.0f, 0x1080);
    g_087a24d8 = Dvar_RegisterInt("g_dumpAnims", -1, -1, 0x3ff, 0x1080);
    g_087a2420 = Dvar_RegisterInt("g_useholdtime", 0, 0, 0x7fffffff, 0x1000);
    g_087a2424 = Dvar_RegisterInt("g_useholdspawndelay", 1, 0, 10, 0x1081);
    g_087a2458 = Dvar_RegisterInt("g_mantleBlockTimeBuffer", 500, 0, 60000, 0x1080);
    Player_RegisterDvars();
}

// 0x08108dee  size=10  Level_GetIntValue  status=translated  naming=unknown
// naming: getter for a level-global int (g_mapChangeKeepStateArg); descriptive name
int Level_GetIntValue(void)
{
    return g_mapChangeKeepStateArg;
}

// 0x08108df8  size=13  Level_SetIntValue  status=translated  naming=unknown
// naming: setter for the same level-global int
void Level_SetIntValue(int value)
{
    g_mapChangeKeepStateArg = value;
}

// 0x08108e06  size=19  Float_GetLevelValue  status=translated  naming=unknown
// naming: returns a level-global float (g_0864e15c) widened to long double
long double Float_GetLevelValue(void)
{
    return (long double)g_0864e15c;
}

// 0x08108e1a  size=28  G_GetClientScore  status=translated  naming=unknown
// naming: reads an int field of client record i (+0x26b8); descriptive name
int G_GetClientScore(int clientIndex)
{
    return level[clientIndex].score;
}

// 0x08108e36  size=28  G_GetClientArchiveTime  status=translated  naming=unknown
// naming: reads an int field of client record i (+0x26b4)
int G_GetClientArchiveTime(int clientIndex)
{
    return level[clientIndex].archiveTime;
}

// 0x08108e52  size=32  G_SetClientArchiveTime  status=translated  naming=unknown
// naming: writes the field read by G_GetClientArchiveTime
void G_SetClientArchiveTime(int clientIndex, int value)
{
    level[clientIndex].archiveTime = value;
}

// 0x08108e72  size=25  G_GetClientStateAddr  status=translated  naming=unknown
// naming: returns the address of the sub-record at client +0x2748 (clientState_t; memcpy'd by SV_BuildClientSnapshot/SV_ArchiveSnapshot)
struct clientState_s *G_GetClientStateAddr(int clientIndex)
{
    return &level[clientIndex].cs;
}

// 0x08108e8c  size=127  Game_FreeAllEntities  status=translated  naming=inferred
// naming: frees every in-use entity up to level.num_entities, then the special entity at g_dummyEntity, and resets the entity counters
void Game_FreeAllEntities(void)
{
    struct gentity_s *ent = g_gentities;
    int i;

    for (i = 0; i < level_num_entities; i++) {
        if (ent->r.inuse != 0) {
            G_FreeEntity(ent);
        }
        ent++;
    }
    if (g_087a209c != 0) {
        G_FreeEntity(&g_dummyEntity);
    }
    level_num_entities = 0;
    level_freeEntityHead = 0;
    level_freeEntityTail = 0;
}

// 0x08108f3c  size=137  Pool_AllocGameObjects  status=translated  naming=inferred
// naming: fills the 64-entry object pool (stride 0x4b8) and the 8 per-slot records (stride 0x4c8) from pool g_08703588 with Hunk_AllocLowCallback as the release callback
void Pool_AllocGameObjects(void)
{
    unsigned int pool = g_08703588;
    int i;

    for (i = 0; i < 0x40; i++) {
        *(void **)(g_08703a60 + i * 0x4b8) = XAnim_AllocBlock((XAnimNodeTable *)pool, Hunk_AllocLowCallback);
    }
    for (i = 0; i < 8; i++) {
        *(void **)((char *)g_trackedEntitySlots + i * 0x4c8) = XAnim_AllocBlock((XAnimNodeTable *)pool, Hunk_AllocLowCallback);
    }
}

// 0x08108fc6  size=188  Pool_FreeGameObjects  status=translated  naming=inferred
// naming: releases the pool elements filled by Pool_AllocGameObjects and clears their slots
void Pool_FreeGameObjects(void)
{
    int i;

    for (i = 0; i < 0x40; i++) {
        if (*(void **)(g_08703a60 + i * 0x4b8) != 0) {
            XAnim_ReinitBlock((void **)*(void **)(g_08703a60 + i * 0x4b8), 0);
            *(void **)(g_08703a60 + i * 0x4b8) = 0;
        }
    }
    for (i = 0; i < 8; i++) {
        if (*(void **)((char *)g_trackedEntitySlots + i * 0x4c8) != 0) {
            XAnim_ReinitBlock((void **)*(void **)((char *)g_trackedEntitySlots + i * 0x4c8), 0);
            *(void **)((char *)g_trackedEntitySlots + i * 0x4c8) = 0;
        }
    }
}

// 0x08109082  size=19  Hunk_AllocLowCallback  status=partial  naming=unknown
// naming: per-element callback used by the object pool (passed to XAnim_AllocBlock); forwards the element to Hunk_AllocLow. Merged name Hunk_AllocLow does not fit a pointer argument, so the placeholder is kept; partial until the callee is confirmed
void *Hunk_AllocLowCallback(int size)
{
    return Hunk_AllocLow(size);
}
