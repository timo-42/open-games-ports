#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_101_types.h"

/* ======================================================================
 * Script debug / string / variable helpers (0x08078536 .. 0x080843ca)
 * ====================================================================== */

// 0x08078536  size=118  FUN_08078536  status=translated  naming=inferred
// Naming: with script debug disabled formats "@ %d" (offset from code base); otherwise looks the code position up in the debug frame table (ScrDbg_LookupLineForKey with flags 4) and formats the frame line via ScrDbg_FormatFrameLine; descriptive name from those callees.
void ScrDbg_FormatCodePos(int codePos, int unused, char *buf, int bufsize)
{
    uint32 found[3];

    if (scr_debug_enabled == 0) {
        Com_sprintf(buf, bufsize, "@ %d", codePos - g_scr_code_base);
    } else {
        ScrDbg_LookupLineForKey((uint32)codePos, 4, found);
        ScrDbg_FormatFrameLine((int)found[0], (int)found[2], buf, bufsize);
    }
}

// 0x08078b1c  size=61  FUN_08078b1c  status=translated  naming=unknown
// Naming: descriptive. Allocates 12 bytes from the high temp hunk, stores three words, returns the box through the hidden struct-return pointer (ret 4). No caller found (FDE-only function).
struct HeapWords_s TempHigh_NewTriple(int a, int b, int c)
{
    struct HeapWords_s box;

    box.words = (int *)Hunk_AllocateTempMemoryHigh(0xc);
    box.words[0] = a;
    box.words[1] = b;
    box.words[2] = c;
    return box;
}

// 0x08078bac  size=71  FUN_08078bac  status=translated  naming=unknown
// Naming: descriptive. Same as TempHigh_NewTriple with a 16-byte, four-word block.
struct HeapWords_s TempHigh_NewQuad(int a, int b, int c, int d)
{
    struct HeapWords_s box;

    box.words = (int *)Hunk_AllocateTempMemoryHigh(0x10);
    box.words[0] = a;
    box.words[1] = b;
    box.words[2] = c;
    box.words[3] = d;
    return box;
}

// 0x08079e1a  size=61  FUN_08079e1a  status=translated  naming=inferred
// Naming: releases the old script string handle in the slot (if any) and interns the new C string; pairs ScrStr_Release / ScrStr_InternCString.
void ScrStr_AssignCString(unsigned short *slot, const char *text)
{
    unsigned short handle;

    if (*slot != 0) {
        ScrStr_Release(*slot);
    }
    handle = (unsigned short)ScrStr_InternCString(text, 0);
    *slot = handle;
}

// 0x0807a006  size=20  FUN_0807a006  status=translated  naming=unknown
// Naming: descriptive thunk: ScrStr_ClearFlagBits(1).
void ScrStr_ClearFlagBit0(void)
{
    ScrStr_ClearFlagBits(1);
}

// 0x0807fda0  size=146  FUN_0807fda0  status=translated  naming=unknown
// Naming: descriptive. Interns the name lower-cased, returns false if the parent already has that variable, else creates it with value {0, type 6} and returns true.
bool ScrVar_DeclareLowerNamed(int parentId, const char *name)
{
    unsigned int nameId;
    int existing;
    unsigned short newId;
    struct VariableValue value;

    nameId = ScrStr_InternLowerCString(name, 0);
    existing = ScrVar_FindVarValueId(parentId, nameId);
    if (existing != 0) {
        ScrStr_Release(nameId);
        return false;
    }
    newId = ScrVar_GetNewVarValueId(parentId, nameId);
    ScrStr_Release(nameId);
    value.type = 6;
    value.u.intValue = 0;
    ScrVar_SetVarValueReleasing(newId, &value);
    return true;
}

// 0x0807fe32  size=19  FUN_0807fe32  status=translated  naming=unknown
// Naming: descriptive thunk to ScrVar_Release.
void ScrVar_ReleaseThunk(unsigned int id)
{
    ScrVar_Release(id);
}

/* ======================================================================
 * Script VM opcode stepping (0x0808261e)
 * ====================================================================== */

// 0x0808261e  size=911  FUN_0808261e  status=translated  naming=unknown
// Naming: descriptive. Given an opcode and the stack top it computes the next code position (skipping operands, evaluating conditional jumps from the top value, handling switch tables); structurally parallel to Scr_SkipBytecode (0x08085658). Inputs: entry = top-of-stack value pair, pc = byte after the opcode start, mode 2 enables the call-depth shortcuts.
char *Scr_ComputeNextCodePos(int *entry, char *pc, int opcode, int mode, uint32 *outLocalId)
{
    char *next;
    int pair[2];
    unsigned int operand;
    int skip;
    int wanted;
    unsigned int count;
    int key;

    *outLocalId = scrVmPub.function_frame->localId;
    do {
        next = pc + 1;
        if (mode == 2) {
            switch (opcode) {
            case 0x4f:
            case 0x50:
            case 0x54:
            L_08082667:
                if (scrVmPub.function_count < 0x20) {
                    *outLocalId = 0;
                    pc = next;
                    return (char *)Cursor_ReadIntAlias2((const char **)&pc);
                }
                break;
            case 0x51:
            case 0x55:
            L_0808269c:
                if (entry[1] == 9 && scrVmPub.function_count < 0x20) {
                    *outLocalId = 0;
                    return (char *)entry[0];
                }
                break;
            case 0x52:
            case 0x56:
                if (entry[1] == 1) {
                    goto L_08082667;
                }
                break;
            case 0x53:
            case 0x57:
                if (entry[-1] == 1) {
                    goto L_0808269c;
                }
            }
        }
        switch (opcode) {
        case 0:
        case 1:
            pc = next;
            return (char *)Scr_PeekCallerOwner(outLocalId);
        case 4:
        case 5:
        case 0x17:
        case 0x1e:
        case 0x1f:
        case 0x22:
        case 0x32:
        case 0x33:
        case 0x37:
        case 0x3d:
        case 0x5a:
        case 0x77:
        case 0x85:
        case 0x86:
            next = pc + 2;
            break;
        case 6:
        case 7:
        case 0x3e:
        case 0x3f:
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4a:
            pc = next;
            Cursor_ReadShortZx((const char **)&pc);
            next = pc;
            break;
        case 8:
        case 9:
        case 0x13:
        case 0x15:
        case 0x4f:
        case 0x50:
        case 0x52:
        case 0x55:
        case 0x57:
            pc = next;
            Cursor_ReadInt((const char **)&pc);
            next = pc;
            break;
        case 10:
        case 0xb:
        case 0x16:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f:
        case 0x30:
        case 0x38:
        case 0x3a:
        case 0x3b:
        case 0x82:
        case 0x83:
        case 0x84:
            pc = next;
            Cursor_ReadShortZx((const char **)&pc);
            next = pc;
            break;
        case 0xc:
            pc = next;
            Cursor_AdvanceVec3((const char **)&pc);
            next = pc;
            break;
        case 0x44:
        case 0x4b:
            pc = pc + 2;
            Cursor_ReadShortZx((const char **)&pc);
            next = pc;
            break;
        case 0x54:
        case 0x56:
        case 0x81:
            pc = next;
            Cursor_ReadInt((const char **)&pc);
            Cursor_ReadInt((const char **)&pc);
            next = pc;
            break;
        case 0x5e:
        case 0x60:
            pair[1] = entry[1];
            pair[0] = entry[0];
            pc = next;
            Scr_AddRefToValue((struct VariableValue *)pair);
            Scr_CastBool((struct VariableValue *)pair);
            operand = Cursor_ReadShortZx((const char **)&pc);
            next = pc;
            if (g_scr_compile_error_message == 0) {
                if (pair[0] == 0) {
                    return pc + (operand & 0xffff);
                }
                return pc;
            }
            break;
        case 0x5f:
        case 0x61:
            pair[1] = entry[1];
            pair[0] = entry[0];
            pc = next;
            Scr_AddRefToValue((struct VariableValue *)pair);
            Scr_CastBool((struct VariableValue *)pair);
            operand = Cursor_ReadShortZx((const char **)&pc);
            next = pc;
            if (g_scr_compile_error_message == 0) {
                if (pair[0] == 0) {
                    return pc;
                }
                return pc + (operand & 0xffff);
            }
            break;
        case 0x62:
            pc = next;
            skip = Cursor_ReadInt((const char **)&pc);
            return pc + skip;
        case 99:
            pc = next;
            operand = Cursor_ReadShortZx((const char **)&pc);
            return pc + -(int)(operand & 0xffff);
        case 0x7c:
            pc = next;
            skip = Cursor_ReadInt((const char **)&pc);
            pc = pc + skip;
            count = Cursor_ReadShortZx((const char **)&pc);
            count = count & 0xffff;
            if (entry[1] == 2) {
                wanted = entry[0];
            } else {
                if (entry[1] != 6) {
                    return pc + count * 8;
                }
                if (Scr_IsSmallNameIndex(entry[0]) == false) {
                    return pc + count * 8;
                }
                wanted = (int)Scr_BiasName24(entry[0]);
            }
            if (count != 0) {
                do {
                    key = Cursor_ReadIntAlias1((const char **)&pc);
                    next = (char *)Cursor_ReadIntAlias2((const char **)&pc);
                    if (key == wanted) {
                        return next;
                    }
                    count = count - 1;
                } while (count != 0);
                if (key == 0) {
                    pc = next;
                }
            }
            return pc;
        case 0x7d:
            pc = next;
            operand = Cursor_ReadShortZx((const char **)&pc);
            Cursor_AdvanceDwords((const char **)&pc, (int)((operand & 0xffff) * 2));
            return pc;
        }
        pc = next;
        Scr_ClearErrorMessage();
        opcode = (int)*pc;
        if (opcode != 0x39) {
            return pc;
        }
    } while (true);
}

// 0x080841f2  size=177  FUN_080841f2  status=translated  naming=unknown
// Naming: descriptive. Samples script timers, marks the error mode, then calls the script function nameId on selfId (or on a fresh level-parented thread when selfId is 0, removing that thread afterwards) and pops one frame / one value-stack entry.
void Scr_ExecNamedCall(unsigned int nameId, unsigned int selfId)
{
    unsigned int threadId;

    GScr_resettimeout();
    scrVmPub.debugCode = 1;
    if (selfId == 0) {
        ScrVar_AddRef(scrVarPub_levelId);
        threadId = ScrVar_AllocThread((unsigned short)scrVarPub_levelId);
        Scr_CallFunction(threadId, nameId, 0);
        ScrVar_RemoveThread(threadId);
        ScrVar_Release(threadId);
    } else {
        Scr_CallFunction(selfId, nameId, 0);
    }
    scrVmPub.debugCode = 0;
    if (scrVmPub.function_count != 0) {
        scrVmPub.function_count = scrVmPub.function_count - 1;
        scrVmPub.function_frame = scrVmPub.function_frame - 1;
    }
    scrVmPub.top = scrVmPub.top - 1;
    scrVmPub.outparamcount = 0;
}

// 0x080843ca  size=276  FUN_080843ca  status=translated  naming=unknown
// Naming: descriptive. Counts script objects of class classNum: the given object (if its aux/class matches), every child under the time array root, and children of the variable named by 0x1fffe; the first match is stored into *outFirst when non-null.
int Scr_CountObjectsOfClass(int classNum, int *outFirst, int objectId)
{
    int count;
    int cur;
    int found;
    int root;
    int child;

    count = 0;
    if (objectId != 0 && classNum == (int)ScrVar_GetAux(objectId)) {
        if (outFirst != 0) {
            *outFirst = objectId;
        }
        count = 1;
    }
    for (cur = (int)Scr_GetNextSibling(scrVarPub_timeArrayId); cur != 0; cur = (int)Scr_GetNextSibling(cur)) {
        count = Scr_CountMatchingObjects(classNum, cur, count, 1, (uint32)outFirst);
    }
    found = ScrVar_FindVarValueId(classNum, 0x1fffe);
    if (found != 0) {
        root = Scr_GetVarValue(found);
        for (child = (int)Scr_GetNextSibling(root); child != 0; child = (int)Scr_GetNextSibling(child)) {
            count = Scr_CountMatchingObjects(classNum, child, count, 0, (uint32)outFirst);
        }
    }
    return count;
}

// 0x0808529c  size=57  FUN_0808529c  status=translated  naming=inferred
// Naming: structural twin of Scr_AddString (0x08085262, value type 2) but tags the pushed entry with type 3; named after the Scr_AddString/Scr_AddIString pair (type 3 = localized string: inferred).
void Scr_AddIString(const char *text)
{
    struct VariableValue *slot;
    unsigned int id;

    Scr_ReserveStackSlot();
    scrVmPub.top->type = 3;
    slot = scrVmPub.top;
    id = ScrStr_InternCString(text, 0);
    slot->u.stringValue = id;
}

/* ======================================================================
 * Server operator commands (registered from the operator-command table at 0x0808cc..)
 * ====================================================================== */

// 0x0808bd46  size=248  FUN_0808bd46  status=translated  naming=inferred
// Naming: registered for the commands "map" and "devmap" (Cmd_AddCommand at 0x0808cdf0 / 0x0808ce48); Q3 SV_Map_f counterpart; compares argv[0] with "devmap" to set sv_cheats.
void SV_Map_f(void)
{
    char mapname[0x40];
    const char *arg1;
    const char *cmdName;
    const char *normalized;
    char *fileName;
    int cheats;

    arg1 = Cmd_Argv_alt(1);
    if (*arg1 != '\0') {
        normalized = SV_MapNameFromPath((char *)arg1);
        Q_strncpyz(mapname, normalized, 0x40);
        Q_strlwr(mapname);
        fileName = va("maps/mp/%s.%s", mapname, Gfx_GetBspVariant());
        if (FS_ReadFile(fileName, 0) == -1) {
            Com_Printf("Can't find map %s\n", fileName);
        } else {
            cmdName = Cmd_Argv_alt(0);
            cheats = Q_stricmp(cmdName, "devmap");
            FS_NormalizeSeparators(mapname);
            SV_SpawnServer(mapname);
            Dvar_SetBool(cv_sv_cheats, cheats == 0);
        }
    }
}

// 0x0808c55e  size=73  FUN_0808c55e  status=translated  naming=inferred
// Naming: registered for the commands "kick" and "tempBanUser" (0x0808cd14 / 0x0808cd28); resolves a player by name through SV_Kick_f, logs the cheating message and temp-bans the guid.
void SV_TempBanUser_f(void)
{
    char name[0x4c];
    int guid;

    guid = SV_Kick_f(name, 0x40);
    if (guid != 0) {
        Com_Printf("%s (guid %i) was kicked for cheating\n", name, guid);
        SV_TempBanAdd(guid);
    }
}

// 0x0808c5a8  size=84  FUN_0808c5a8  status=translated  naming=inferred
// Naming: registered for the command "banUser" (0x0808ccec); strings "Usage: banUser <player name>".
void SV_BanUser_f(void)
{
    struct client_s *cl;

    if (com_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
    } else {
        if (Cmd_Argc_alt() == 2) {
            cl = SV_GetPlayerByName();
            if (cl != 0) {
                SV_BanClient(cl);
            }
        } else {
            Com_Printf("Usage: banUser <player name>\n");
        }
    }
}

// 0x0808c5fc  size=84  FUN_0808c5fc  status=translated  naming=inferred
// Naming: registered for the command "banClient" (0x0808cd00); strings "Usage: banClient <client number>".
void SV_BanClient_f(void)
{
    struct client_s *cl;

    if (com_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
    } else {
        if (Cmd_Argc_alt() == 2) {
            cl = SV_GetPlayerByNum();
            if (cl != 0) {
                SV_BanClient(cl);
            }
        } else {
            Com_Printf("Usage: banClient <client number>\n");
        }
    }
}

// 0x0808c650  size=52  FUN_0808c650  status=translated  naming=inferred
// Naming: registered for the command "unbanUser" (0x0808cd50); strings "Usage: unban <client name>".
void SV_UnbanUser_f(void)
{
    const char *name;

    if (Cmd_Argc_alt() == 2) {
        name = Cmd_Argv_alt(1);
        SV_UnbanUserByName(name);
    } else {
        Com_Printf("Usage: unban <client name>\n");
    }
}

// 0x0808c684  size=28  FUN_0808c684  status=translated  naming=inferred
// Naming: registered for the command "onlykick" (0x0808ccd8); calls SV_Kick_f(0, 0) (kick without the cheater report).
void SV_OnlyKick_f(void)
{
    SV_Kick_f(0, 0);
}

// 0x0808c6a0  size=28  FUN_0808c6a0  status=translated  naming=inferred
// Naming: registered for the command "clientkick" (0x0808cd64); calls SV_KickNum_f(0, 0).
void SV_ClientKick_f(void)
{
    SV_KickNum_f(0, 0);
}

// 0x0808c6bc  size=73  FUN_0808c6bc  status=translated  naming=inferred
// Naming: registered for the command "tempBanClient" (0x0808cd3c); same shape as SV_TempBanUser_f but resolves the client by number (SV_KickNum_f).
void SV_TempBanClient_f(void)
{
    char name[0x4c];
    int guid;

    guid = SV_KickNum_f(name, 0x40);
    if (guid != 0) {
        Com_Printf("%s (guid %i) was kicked for cheating\n", name, guid);
        SV_TempBanAdd(guid);
    }
}

// 0x0808c706  size=671  FUN_0808c706  status=translated  naming=inferred
// Naming: registered for the command "status" (0x0808cd78); prints the num/score/ping/guid/name/lastmsg/address/qport/rate table, Q3 SV_Status_f counterpart.
void SV_Status_f(void)
{
    struct client_s *cl;
    int i;
    int pad;
    int numPad;
    int pingValue;
    char *adrStr;
    int nameLen;
    int score;

    if (com_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
    } else {
        Com_Printf("map: %s\n", cv_mapname->current.string);
        Com_Printf("num score ping guid   name            lastmsg address               qport rate\n");
        Com_Printf("--- ----- ---- ------ --------------- ------- --------------------- ----- -----\n");
        cl = g_svs_clients;
        for (i = 0; i < cv_sv_maxclients->current.integer; i++) {
            if (cl->state != 0) {
                Com_Printf("%3i ", i);
                SV_GameClientNum(i);
                /* The binary derives the client index with a multiplicative inverse of the stride. */
                score = G_GetClientScore((int)(cl - g_svs_clients));
                Com_Printf("%5i ", score);
                if (cl->state == 2) {
                    Com_Printf("CNCT ");
                } else if (cl->state == 1) {
                    Com_Printf("ZMBI ");
                } else {
                    pingValue = cl->ping;
                    if (pingValue > 9999) {
                        pingValue = 9999;
                    }
                    Com_Printf("%4i ", pingValue);
                }
                Com_Printf("%6i ", cl->guid);
                Com_Printf("%s^7", cl->name);
                nameLen = Q_PrintStrlen(cl->name);
                numPad = 0x10 - nameLen;
                for (pad = 0; pad < numPad; pad++) {
                    Com_Printf(" ");
                }
                Com_Printf("%7i ", g_svs_time - cl->lastPacketTime);
                adrStr = NET_AdrToString(cl->netchan.remoteAddress);
                Com_Printf("%s", adrStr);
                numPad = 0x16 - (int)strlen(adrStr);
                for (pad = 0; pad < numPad; pad++) {
                    Com_Printf(" ");
                }
                Com_Printf("%5i", cl->netchan.qport);
                Com_Printf(" %5i", cl->rate);
                Com_Printf("\n");
            }
            cl++;
        }
        Com_Printf("\n");
    }
}

// 0x0808c9a6  size=197  FUN_0808c9a6  status=translated  naming=inferred
// Naming: registered for the command "say" (0x0808ced2); prefixes "console: " and broadcasts via SV_SendServerCommand; Q3 SV_ConSay_f counterpart.
void SV_ConSay_f(void)
{
    char text[0x40c];
    char *p;
    size_t len;

    if (com_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
    } else {
        if (Cmd_Argc_alt() > 1) {
            strcpy(text, "console: ");
            p = (char *)Cmd_ArgsFrom(1);
            if (*p == '"') {
                p = p + 1;
                len = strlen(p);
                p[len - 1] = '\0';
            }
            I_strncat(text, 0x400, p);
            SV_SendServerCommand(0, 0, "%c \"\x15%s\"", 0x68, text);
        }
    }
}

// 0x0808ca6c  size=299  FUN_0808ca6c  status=translated  naming=inferred
// Naming: registered for the command "tell" (0x0808cee6); sends "console: ..." to one active client; Q3 SV_ConTell_f counterpart.
void SV_ConTell_f(void)
{
    int clientNum;
    struct client_s *cl;
    char text[0x40c];
    char *p;
    size_t len;

    if (com_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
    } else {
        if (Cmd_Argc_alt() > 2) {
            clientNum = atoi(Cmd_Argv_alt(1));
            if (clientNum > -1 && clientNum < cv_sv_maxclients->current.integer) {
                cl = g_svs_clients + clientNum;
                if (cl->state == 4) {
                    strcpy(text, "console: ");
                    p = (char *)Cmd_ArgsFrom(2);
                    if (*p == '"') {
                        p = p + 1;
                        len = strlen(p);
                        p[len - 1] = '\0';
                    }
                    I_strncat(text, 0x400, p);
                    SV_SendServerCommand(cl, 0, "%c \"\x15%s\"", 0x68, text);
                }
            }
        }
    }
}

// 0x0808cba8  size=40  FUN_0808cba8  status=translated  naming=inferred
// Naming: registered for the command "serverinfo" (0x0808cd8c); Q3 SV_Serverinfo_f counterpart.
void SV_ServerInfo_f(void)
{
    Com_Printf("Server info settings:\n");
    Info_Print(Dvar_InfoString(0x404));
}

// 0x0808cbd0  size=40  FUN_0808cbd0  status=translated  naming=inferred
// Naming: registered for the command "systeminfo" (0x0808cda0); Q3 SV_Systeminfo_f counterpart.
void SV_SystemInfo_f(void)
{
    Com_Printf("System info settings:\n");
    Info_Print(Dvar_InfoString(8));
}

// 0x0808cbf8  size=111  FUN_0808cbf8  status=translated  naming=inferred
// Naming: registered for the command "dumpuser" (0x0808cdb4); Q3 SV_DumpUser_f counterpart (prints the userinfo, usage "info <userid>").
void SV_DumpUser_f(void)
{
    struct client_s *cl;

    if (com_sv_running->current.enabled == 0) {
        Com_Printf("Server is not running.\n");
    } else {
        if (Cmd_Argc_alt() == 2) {
            cl = SV_GetPlayerByName();
            if (cl != 0) {
                Com_Printf("userinfo\n");
                Com_Printf("--------\n");
                Info_Print(cl->userinfo);
            }
        } else {
            Com_Printf("Usage: info <userid>\n");
        }
    }
}

// 0x0808cc68  size=20  FUN_0808cc68  status=translated  naming=inferred
// Naming: registered for the command "killserver" (0x0808ce78); Q3 SV_KillServer_f counterpart.
void SV_KillServer_f(void)
{
    Com_ServerShutdownThunk("EXE_SERVERKILLED");
}

// 0x0808cc7c  size=13  FUN_0808cc7c  status=translated  naming=inferred
// Naming: registered for the command "gameCompleteStatus" (0x0808ce34); thunk to SV_MasterGameCompleteStatus.
void SV_GameCompleteStatus_f(void)
{
    SV_MasterGameCompleteStatus();
}

// 0x0808cc8a  size=13  FUN_0808cc8a  status=translated  naming=inferred
// Naming: registered for the command "scriptUsage" (0x0808ce9c); thunk to Scr_DumpScriptThreads.
void SV_ScriptUsage_f(void)
{
    Scr_DumpScriptThreads();
}

// 0x0808cc98  size=13  FUN_0808cc98  status=translated  naming=inferred
// Naming: registered for the command "stringUsage" (0x0808ceb0); thunk to Mem_PrintBucketStats.
void SV_StringUsage_f(void)
{
    Mem_PrintBucketStats();
}

/* ======================================================================
 * Client download / pure / mute commands (0x0808f780 .. 0x08090aaa)
 * ====================================================================== */

// 0x0808f780  size=80  FUN_0808f780  status=translated  naming=inferred
// Naming: debug-prints the aborted download then resets the download state; Q3 SV_StopDownload_f counterpart (client command "stopdl").
void SV_StopDownload_f(struct client_s *cl)
{
    if (cl->downloadName[0] != '\0') {
        Com_DPrintf("clientDownload: %d : file \"%s\" aborted\n", (int)(cl - g_svs_clients), cl->downloadName);
    }
    SV_ClientResetDownload(cl);
}

// 0x0808f7d0  size=43  FUN_0808f7d0  status=translated  naming=inferred
// Naming: debug-prints "clientDownload: %s Done" and re-sends the gamestate; Q3 SV_DoneDownload_f counterpart (client command "donedl").
void SV_DoneDownload_f(struct client_s *cl)
{
    Com_DPrintf("clientDownload: %s Done\n", cl->name);
    SV_SendClientGameState(cl);
}

// 0x0808f7fc  size=63  FUN_0808f7fc  status=translated  naming=inferred
// Naming: when the acknowledged block equals downloadClientBlock the transmit block is rewound to it (retransmit); counterpart of ioq3 SV_RetransmitDownload handling; descriptive.
void SV_RetransmitDownload_f(struct client_s *cl)
{
    int block;

    block = atoi(Cmd_Argv_alt(1));
    if (block == cl->downloadClientBlock) {
        cl->downloadXmitBlock = cl->downloadClientBlock;
    }
}

// 0x0808f83c  size=260  FUN_0808f83c  status=translated  naming=inferred
// Naming: Q3 SV_NextDownload_f counterpart (client command "nextdl"): acknowledges block downloadClientBlock, finishes the download when the block size slot is 0, otherwise advances; wrong block drops the client with "broken download".
void SV_NextDownload_f(struct client_s *cl)
{
    int block;
    int slotBase;

    block = atoi(Cmd_Argv_alt(1));
    if (block == cl->downloadClientBlock) {
        Com_DPrintf("clientDownload: %d : client acknowledge of block %d\n", (int)(cl - g_svs_clients), block);
        block = cl->downloadClientBlock;
        slotBase = block;
        if (block < 0) {
            slotBase = block + 7;
        }
        if (cl->downloadBlockSize[block + (slotBase >> 3) * -8] == 0) {
            Com_Printf("clientDownload: %d : file \"%s\" completed\n", (int)(cl - g_svs_clients), cl->downloadName);
            SV_ClientResetDownload(cl);
        } else {
            cl->downloadSendTime = g_svs_time;
            cl->downloadClientBlock = cl->downloadClientBlock + 1;
        }
    } else {
        SV_DropClient(cl, "broken download");
    }
}

// 0x0808f940  size=59  FUN_0808f940  status=translated  naming=inferred
// Naming: resets any running download then stores argv[1] as the new download name (64 bytes); Q3 SV_BeginDownload_f counterpart (client command "download").
void SV_BeginDownload_f(struct client_s *cl)
{
    const char *name;

    SV_ClientResetDownload(cl);
    name = Cmd_Argv_alt(1);
    Q_strncpyz(cl->downloadName, name, 0x40);
}

// 0x0808f97c  size=655  FUN_0808f97c  status=translated  naming=observed
// Naming: strings "SV_WWWDownload: unexpected wwwdl '%s' for client '%s'" and "SV_WWWDownload: unknown wwwdl subcommand" name the function and are referenced here.
void SV_WWWDownload(struct client_s *cl)
{
    const char *subcmd;

    subcmd = Cmd_Argv(1);
    if (cl->downloadingWWW == 0) {
        Com_Printf("SV_WWWDownload: unexpected wwwdl '%s' for client '%s'\n", subcmd, cl->name);
        SV_DropClient(cl, "PC_PATCH_1_1_UNEXPECTEDDOWLOADMESSAGE");
    } else {
        if (Q_stricmp(subcmd, "ack") == 0) {
            if (cl->clientDownloadingWWW != 0) {
                Com_Printf("WARNING: dupe wwwdl ack from client '%s'\n", cl->name);
            }
            cl->clientDownloadingWWW = 1;
        } else {
            if (Q_stricmp(subcmd, "bbl8r") == 0) {
                SV_DropClient(cl, "PC_PATCH_1_1_DOWNLOADDISCONNECTED");
            } else if (cl->clientDownloadingWWW == 0) {
                Com_Printf("SV_WWWDownload: unexpected wwwdl '%s' for client '%s'\n", subcmd, cl->name);
                SV_DropClient(cl, "PC_PATCH_1_1_UNEXPECTEDDOWLOADMESSAGE");
            } else {
                if (Q_stricmp(subcmd, "done") == 0) {
                    cl->download = 0;
                    cl->downloadName[0] = '\0';
                    cl->clientDownloadingWWW = 0;
                } else {
                    if (Q_stricmp(subcmd, "fail") == 0) {
                        cl->download = 0;
                        cl->downloadName[0] = '\0';
                        cl->clientDownloadingWWW = 0;
                        cl->wwwFallback = 1;
                        Com_Printf("Client '%s' reported that the http download of '%s' failed, falling back to a server download\n", cl->name, cl->downloadName);
                        SV_SendClientGameState(cl);
                    } else {
                        if (Q_stricmp(subcmd, "chkfail") == 0) {
                            Com_Printf("WARNING: client '%s' reports that the redirect download for '%s' had wrong checksum.\n", cl->name, cl->downloadName);
                            Com_Printf("         you should check your download redirect configuration.\n");
                            cl->download = 0;
                            cl->downloadName[0] = '\0';
                            cl->clientDownloadingWWW = 0;
                            cl->wwwFallback = 1;
                            SV_SendClientGameState(cl);
                        } else {
                            Com_Printf("SV_WWWDownload: unknown wwwdl subcommand '%s' for client '%s'\n", subcmd, cl->name);
                            SV_DropClient(cl, "PC_PATCH_1_1_UNEXPECTEDDOWLOADMESSAGE");
                        }
                    }
                }
            }
        }
    }
}

// 0x08090534  size=645  FUN_08090534  status=translated  naming=inferred
// Naming: Q3 SV_VerifyPaks_f counterpart (client command "cp"): argv[1] must be "@", the following pure checksums must be unique, present in the server's pure checksum list and XOR (with the checksum feed and the count) to the trailing value; result stored in pureAuthentic (1 ok, 2 failed).
void SV_VerifyPaks_f(struct client_s *cl)
{
    bool good;
    int dummy;
    unsigned int checksum;
    unsigned int nClientPaks;
    unsigned int serverChecksums[1024];
    unsigned int clientChecksums[1025];
    int nCurArg;
    int i;
    int j;
    int nServerPaks;
    const char *arg;
    const char *pureList;

    good = true;
    dummy = 0;
    checksum = 0;
    nClientPaks = (unsigned int)Cmd_Argc_alt();
    if ((int)nClientPaks < 2) {
        good = false;
    } else {
        nCurArg = 2;
        arg = Cmd_Argv_alt(1);
        if (*arg == '@') {
            i = 0;
            while (nCurArg < (int)nClientPaks) {
                j = nCurArg;
                nCurArg = nCurArg + 1;
                arg = Cmd_Argv_alt(j);
                clientChecksums[i] = (unsigned int)atoi(arg);
                i = i + 1;
            }
            nClientPaks = (unsigned int)(i - 1);
            for (i = 0; i < (int)nClientPaks; i++) {
                for (j = 0; j < (int)nClientPaks; j++) {
                    if (i != j && clientChecksums[i] == clientChecksums[j]) {
                        good = false;
                        break;
                    }
                }
                if (!good) {
                    break;
                }
            }
            if (good) {
                pureList = FS_LoadedIwdPureChecksums();
                Cmd_TokenizeString_alt((char *)pureList);
                nServerPaks = Cmd_Argc_alt();
                if (nServerPaks > 0x400) {
                    nServerPaks = 0x400;
                }
                for (i = 0; i < nServerPaks; i++) {
                    arg = Cmd_Argv_alt(i);
                    serverChecksums[i] = (unsigned int)atoi(arg);
                }
                for (i = 0; i < (int)nClientPaks; i++) {
                    for (j = 0; j < nServerPaks && clientChecksums[i] != serverChecksums[j]; j++) {
                    }
                    if (nServerPaks <= j) {
                        good = false;
                        break;
                    }
                }
                if (good) {
                    checksum = (unsigned int)sv_checksumFeed;
                    for (i = 0; i < (int)nClientPaks; i++) {
                        checksum = checksum ^ clientChecksums[i];
                    }
                    if ((checksum ^ nClientPaks) != clientChecksums[nClientPaks]) {
                        good = false;
                    }
                }
            }
        } else {
            good = false;
        }
    }
    if (good) {
        cl->pureAuthentic = 1;
    } else {
        cl->pureAuthentic = 2;
    }
}

// 0x08090a5a  size=80  FUN_08090a5a  status=translated  naming=inferred
// Naming: client command "mute <client>": sets muted[n] = 1 (range check n<0 || n>0x40 prints "Invalid mute client %i").
void SV_Mute_f(struct client_s *cl)
{
    int clientNum;

    clientNum = atoi(Cmd_Argv_alt(1));
    if (clientNum < 0 || clientNum > 0x40) {
        Com_Printf("Invalid mute client %i\n", clientNum);
    } else {
        cl->muteList[clientNum] = 1;
    }
}

// 0x08090aaa  size=80  FUN_08090aaa  status=translated  naming=inferred
// Naming: client command "unmute <client>": clears muted[n] (same range check, "Invalid unmute client %i").
void SV_Unmute_f(struct client_s *cl)
{
    int clientNum;

    clientNum = atoi(Cmd_Argv_alt(1));
    if (clientNum < 0 || clientNum > 0x40) {
        Com_Printf("Invalid unmute client %i\n", clientNum);
    } else {
        cl->muteList[clientNum] = 0;
    }
}

/* ======================================================================
 * Game-import wrappers, OOB packets, entity trace (0x08091d1c .. 0x0809e2aa)
 * ====================================================================== */

// 0x08091d1c  size=48  FUN_08091d1c  status=translated  naming=inferred
// Naming: game-module import wrapper around Hunk_Alloc; raises Com_Error(1) with "Hunk_Alloc can only be called in G_InitGame and the first few frames of G_RunFrame" unless the server state variable equals 1.
void *G_Hunk_Alloc(int size)
{
    if (g_sv_state != 1) {
        Com_Error(1, "\x15Hunk_Alloc can only be called in G_InitGame and the first few frames of G_RunFrame\n");
    }
    return Hunk_Alloc(size);
}

// 0x08091d4c  size=55  FUN_08091d4c  status=translated  naming=inferred
// Naming: game-module import wrapper around Hunk_AllocAlign with the same state gate and Com_Error message "Hunk_AllocAlign can only be called in G_InitGame and the first few frames of G_RunFrame".
void *G_Hunk_AllocAlign(int size, int alignment)
{
    if (g_sv_state != 1) {
        Com_Error(1, "\x15Hunk_AllocAlign can only be called in G_InitGame and the first few frames of G_RunFrame\n");
    }
    return Hunk_AllocAlign((size_t)size, alignment);
}

// 0x08091d84  size=19  FUN_08091d84  status=translated  naming=inferred
// Naming: game-module import thunk to Hunk_AllocateTempMemory (returns its result).
int *G_Hunk_AllocateTempMemory(int size)
{
    return Hunk_AllocateTempMemory(size);
}

// 0x08091d98  size=19  FUN_08091d98  status=translated  naming=inferred
// Naming: game-module import thunk to Hunk_FreeTempMemory.
void G_Hunk_FreeTempMemory(int *ptr)
{
    Hunk_FreeTempMemory(ptr);
}

// 0x08091e20  size=19  FUN_08091e20  status=translated  naming=unknown
// Naming: referenced as a callback pointer at 0x08091e80 (next to the string "bad model name '%s'"); thunk to Hunk_Alloc returning its result.
void *SV_HunkAllocCallbackA(int size)
{
    return Hunk_Alloc(size);
}

// 0x08091e34  size=19  FUN_08091e34  status=translated  naming=unknown
// Naming: referenced as a callback pointer at 0x08091e78 (next to the string "bad model name '%s'"); thunk to Hunk_Alloc returning its result.
void *SV_HunkAllocCallbackB(int size)
{
    return Hunk_Alloc(size);
}

// 0x0809223c  size=35  FUN_0809223c  status=translated  naming=inferred
// Naming: resolves the server DObj for the handle at *handle and returns its rot/trans array (DObj_GetRotTransArray(Com_GetServerDObj(*handle))).
struct DObjBone *SV_DObjGetRotTransArray(int *handle)
{
    struct DObj *obj;

    obj = Com_GetServerDObj(*handle);
    return DObj_GetRotTransArray((const struct DObj *)obj);
}

// 0x08092260  size=49  FUN_08092260  status=translated  naming=inferred
// Naming: server DObj handle wrapper for DObj_SetRotTransIndex.
int SV_DObjSetRotTransIndex(int *handle, const uint32 *partBits, int boneIndex)
{
    struct DObj *obj;

    obj = Com_GetServerDObj(*handle);
    return DObj_SetRotTransIndex((struct DObj *)obj, partBits, boneIndex);
}

// 0x08092292  size=49  FUN_08092292  status=translated  naming=inferred
// Naming: server DObj handle wrapper for DObj_SetControlRotTransIndex.
int SV_DObjSetControlRotTransIndex(int *handle, const uint32 *partBits, int boneIndex)
{
    struct DObj *obj;

    obj = Com_GetServerDObj(*handle);
    return DObj_SetControlRotTransIndex((struct DObj *)obj, partBits, boneIndex);
}

// 0x080976aa  size=112  FUN_080976aa  status=translated  naming=observed
// Naming: string "SV_Netchan_SendOOBPacket used to send non-OOB packet." names the function and is referenced here. Sends an out-of-band packet (length, data starting with -1, destination address by value) on socket 1 after syncing the OOB net profile.
void SV_Netchan_SendOOBPacket(int length, const void *data, struct netadr_s to)
{
    if (*(const int *)data != -1) {
        Com_Printf("SV_Netchan_SendOOBPacket used to send non-OOB packet.\n");
    }
    NetProfile_Sync(&g_svs_oobNetProfile);
    NET_SendPacket(1, length, data, to);
    SV_NetProfile_RecordOOB(length);
}

// 0x0809e2aa  size=190  FUN_0809e2aa  status=translated  naming=inferred
// Naming: clears the trace result (fraction 1.0, entityNum 0x3ff), builds a line clip from start/end, and clips it against the server entity of the given gentity number via SV_ClipMoveToEntity_Line.
void SV_TraceToEntity(struct trace_s *tr, const float *start, const float *end, int entNum, int contentmask, int traceFlags)
{
    struct lineclip_a_s clip;
    struct gentity_s *gent;
    struct svEntity_s *svEnt;

    gent = SV_GentityNum(entNum);
    svEnt = SV_SvEntityForGentity(gent);
    memset(tr, 0, 0x24);
    tr->fraction = 1.0f;
    tr->entityNum = 0x3ff;
    Vec3_Copy2(start, clip.start);
    Vec3_Copy2(end, clip.end);
    Box_ComputeInverseExtents(clip.start);
    clip.unk_24 = -1;
    clip.unk_28 = -1;
    clip.contentmask = contentmask;
    clip.unk_30 = 1;
    clip.traceFlags = traceFlags;
    SV_ClipMoveToEntity_Line((struct SegHitTrace *)&clip, svEnt, tr);
}

/* ======================================================================
 * Filesystem
 * ====================================================================== */

// 0x080a0236  size=47  FUN_080a0236  status=translated  naming=inferred
// Naming: calls FS_FOpenFileReadInternal with the trailing quiet argument fixed to 1; thin Q3 FS_FOpenFileRead wrapper shape (returns the size).
int FS_FOpenFileRead_Quiet(const char *filename, int *file, int uniqueFile)
{
    return FS_FOpenFileReadInternal(filename, file, uniqueFile, 1);
}

// 0x080a3692  size=79  FUN_080a3692  status=translated  naming=inferred
// Naming: Q3 FS_FTell counterpart: ftell on the plain FILE, or the zip-handle tell when the handle slot's zipFile flag is set.
long FS_FTell(int handle)
{
    FILE *stream;
    long pos;

    if (fsh[handle].iwd == 0) {
        stream = (FILE *)FS_FileForHandle(handle);
        pos = ftell(stream);
    } else {
        pos = unzTellCurrentFile((struct unz_s *)fsh[handle].handleFiles);
    }
    return pos;
}

/* ======================================================================
 * Random / geometry / rotation helpers (0x080a37a6 .. 0x080aacd6)
 * ====================================================================== */

// 0x080a37a6  size=119  FUN_080a37a6  status=translated  naming=inferred
// Naming: polar (Marsaglia) method: draws two Rand_Signed values until x*x+y*y <= 1, scales both by sqrt(-2*ln(s)/s); returns a normally distributed pair.
void Rand_GaussianPair(float *outA, float *outB)
{
    float x;
    float y;
    float s;
    float scale;

    do {
        x = (float)Rand_Signed();
        y = (float)Rand_Signed();
        s = y * y + x * x;
    } while (1.0f < s);
    scale = Float_Sqrt_080aaebe((float)((-2.0 * log((double)s)) / (double)s));
    *outA = x * scale;
    *outB = y * scale;
}

// 0x080a381e  size=92  FUN_080a381e  status=translated  naming=unknown
// Naming: descriptive. Point on a disk: radius = sqrt(u), angle = 2*pi*v; writes (r*cos, r*sin).
void Vec2_DiskPoint(float u, float v, float *out)
{
    float radius;
    float angle;
    float sinA;
    float cosA;

    radius = Float_Sqrt_080aaebe(u);
    angle = (float)(6.283185307179586 * v);
    SinCos_Float(angle, &sinA, &cosA);
    out[0] = radius * cosA;
    out[1] = radius * sinA;
}

// 0x080a387a  size=118  FUN_080a387a  status=translated  naming=unknown
// Naming: descriptive. Point on the unit sphere: z = 2u - 1, r = sqrt(1 - z*z), angle = 2*pi*v.
void Vec3_SpherePoint(float u, float v, float *out)
{
    float z;
    float radius;
    float angle;
    float sinA;
    float cosA;

    z = (u + u) - 1.0f;
    radius = Float_Sqrt_080aaebe(1.0f - z * z);
    angle = (float)(6.283185307179586 * v);
    SinCos_Float(angle, &sinA, &cosA);
    out[0] = radius * cosA;
    out[1] = radius * sinA;
    out[2] = z;
}

// 0x080a38f0  size=110  FUN_080a38f0  status=translated  naming=unknown
// Naming: descriptive. Point on the unit hemisphere: z = u, r = sqrt(1 - z*z), angle = 2*pi*v.
void Vec3_HemispherePoint(float u, float v, float *out)
{
    float radius;
    float angle;
    float sinA;
    float cosA;

    radius = Float_Sqrt_080aaebe(1.0f - u * u);
    angle = (float)(6.283185307179586 * v);
    SinCos_Float(angle, &sinA, &cosA);
    out[0] = radius * cosA;
    out[1] = radius * sinA;
    out[2] = u;
}

// 0x080a395e  size=184  FUN_080a395e  status=translated  naming=unknown
// Naming: descriptive. Fibonacci/golden-spiral disk distribution: count points written at out + i*stride bytes; radius = sqrt((i+0.5)/count), angle advanced by a fixed rotation (cos -0.7373689, sin -0.6754903).
void Vec2_DiskSpiral(int count, float *out, int stride)
{
    float sinStep;
    float cosStep;
    float step;
    float t;
    float rx;
    float ry;
    float radius;
    float prevY;
    int i;

    sinStep = -0.6754903f;
    cosStep = -0.7373689f;
    step = 1.0f / (float)count;
    t = 0.5f * step;
    ry = 0.0f;
    rx = 1.0f;
    for (i = 0; i < count; i++) {
        radius = Float_Sqrt_080aaebe(t);
        out[0] = radius * rx;
        out[1] = radius * ry;
        t = t + step;
        prevY = ry;
        ry = ry * cosStep + rx * sinStep;
        rx = rx * cosStep - prevY * sinStep;
        out = (float *)((int)out + stride);
    }
}

// 0x080a3a16  size=214  FUN_080a3a16  status=translated  naming=unknown
// Naming: descriptive. Spiral distribution on a hemisphere: z = (i+0.5)/count, radius = sqrt(1 - z*z), same fixed per-step rotation as Vec2_DiskSpiral; writes 3 floats per point at stride bytes.
void Vec3_HemisphereSpiral(int count, float *out, int stride)
{
    float sinStep;
    float cosStep;
    float step;
    float z;
    float rx;
    float ry;
    float radius;
    float prevY;
    int i;

    sinStep = -0.6754903f;
    cosStep = -0.7373689f;
    step = 1.0f / (float)count;
    z = 0.5f * step;
    ry = 0.0f;
    rx = 1.0f;
    for (i = 0; i < count; i++) {
        radius = Float_Sqrt_080aaebe(1.0f - z * z);
        out[0] = radius * rx;
        out[1] = radius * ry;
        out[2] = z;
        z = z + step;
        prevY = ry;
        ry = ry * cosStep + rx * sinStep;
        rx = rx * cosStep - prevY * sinStep;
        out = (float *)((int)out + stride);
    }
}

// 0x080a3aec  size=222  FUN_080a3aec  status=translated  naming=unknown
// Naming: descriptive. Spiral distribution on the full sphere: z = 2*((i+0.5)/count) - 1, radius = sqrt(1 - z*z), same fixed per-step rotation; 3 floats per point at stride bytes.
void Vec3_SphereSpiral(int count, float *out, int stride)
{
    float sinStep;
    float cosStep;
    float step;
    float t;
    float z;
    float rx;
    float ry;
    float radius;
    float prevY;
    int i;

    sinStep = -0.6754903f;
    cosStep = -0.7373689f;
    step = 1.0f / (float)count;
    t = 0.5f * step;
    ry = 0.0f;
    rx = 1.0f;
    for (i = 0; i < count; i++) {
        z = (t + t) - 1.0f;
        radius = Float_Sqrt_080aaebe(1.0f - z * z);
        out[0] = radius * rx;
        out[1] = radius * ry;
        out[2] = z;
        t = t + step;
        prevY = ry;
        ry = ry * cosStep + rx * sinStep;
        rx = rx * cosStep - prevY * sinStep;
        out = (float *)((int)out + stride);
    }
}

// 0x080a4c3c  size=134  FUN_080a4c3c  status=translated  naming=unknown
// Naming: descriptive. Builds an orthonormal 3-vector set in place from a forward vector at axis[0..2]: up (axis+3) from Vec3_PerpendicularVector, optionally rotated about forward by rollDegrees, then axis+6 = cross(forward, up).
void Axis_FromForwardAndRoll(float *axis, float rollDegrees)
{
    float tmp[6];

    Vec3_PerpendicularVector(axis, axis + 3);
    if (rollDegrees != 0.0f) {
        Vec3_Copy_080ab0d8(axis + 3, tmp);
        Vec3_RotateAroundAxis(axis + 3, axis, tmp, rollDegrees);
    }
    Vec3_Cross(axis, axis + 3, axis + 6);
}

// 0x080a7b4c  size=76  FUN_080a7b4c  status=translated  naming=unknown
// Naming: descriptive. Quaternion (x,y,z,w) for a rotation about +Y by `degrees`: (0, sin(a/2), 0, cos(a/2)).
void Quat_FromAngleY(float degrees, float *q)
{
    degrees = (float)(degrees * 0.008726646259971648);
    q[0] = 0.0f;
    q[2] = 0.0f;
    SinCos_Float(degrees, q + 1, q + 3);
}

// 0x080a7b98  size=76  FUN_080a7b98  status=translated  naming=unknown
// Naming: descriptive. Quaternion for a rotation about +Z by `degrees`: (0, 0, sin(a/2), cos(a/2)).
void Quat_FromAngleZ(float degrees, float *q)
{
    degrees = (float)(degrees * 0.008726646259971648);
    q[0] = 0.0f;
    q[1] = 0.0f;
    SinCos_Float(degrees, q + 2, q + 3);
}

// 0x080a7be4  size=76  FUN_080a7be4  status=translated  naming=unknown
// Naming: descriptive. Quaternion for a rotation about +X by `degrees`: (sin(a/2), 0, 0, cos(a/2)).
void Quat_FromAngleX(float degrees, float *q)
{
    degrees = (float)(degrees * 0.008726646259971648);
    q[1] = 0.0f;
    q[2] = 0.0f;
    SinCos_Float(degrees, q, q + 3);
}

// 0x080a7c7c  size=152  FUN_080a7c7c  status=translated  naming=unknown
// Naming: descriptive. 3x3 row-major rotation matrix about X by `degrees`.
void Mat3_RotationX(float *m, float degrees)
{
    float s;
    float c;

    SinCos_Float((float)(degrees * 0.017453292519943295), &s, &c);
    m[0] = 1.0f;
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;
    m[4] = c;
    m[5] = -s;
    m[6] = 0.0f;
    m[7] = s;
    m[8] = c;
}

// 0x080a7d14  size=152  FUN_080a7d14  status=translated  naming=unknown
// Naming: descriptive. 3x3 row-major rotation matrix about Y by `degrees`.
void Mat3_RotationY(float *m, float degrees)
{
    float s;
    float c;

    SinCos_Float((float)(degrees * 0.017453292519943295), &s, &c);
    m[0] = c;
    m[1] = 0.0f;
    m[2] = s;
    m[3] = 0.0f;
    m[4] = 1.0f;
    m[5] = 0.0f;
    m[6] = -s;
    m[7] = 0.0f;
    m[8] = c;
}

// 0x080a7dac  size=152  FUN_080a7dac  status=translated  naming=unknown
// Naming: descriptive. 3x3 row-major rotation matrix about Z by `degrees`.
void Mat3_RotationZ(float *m, float degrees)
{
    float s;
    float c;

    SinCos_Float((float)(degrees * 0.017453292519943295), &s, &c);
    m[0] = c;
    m[1] = -s;
    m[2] = 0.0f;
    m[3] = s;
    m[4] = c;
    m[5] = 0.0f;
    m[6] = 0.0f;
    m[7] = 0.0f;
    m[8] = 1.0f;
}

// 0x080aa04a  size=154  FUN_080aa04a  status=translated  naming=inferred
// Naming: Q3 PlaneFromPoints counterpart: normal = normalize(cross(c - a, b - a)), distance = dot(a, normal); returns false for a degenerate (zero-length) normal.
bool Plane_FromPoints(float *plane, const float *a, const float *b, const float *c)
{
    float ab[3];
    float ac[3];
    float len;

    Vec3_Sub(b, a, ab);
    Vec3_Sub(c, a, ac);
    Vec3_Cross(ac, ab, plane);
    len = Vec3_Normalize(plane);
    if (len != 0.0f) {
        plane[3] = Vec3_Dot(a, plane);
    }
    return len != 0.0f;
}

// 0x080aa55c  size=292  FUN_080aa55c  status=translated  naming=unknown
// Naming: descriptive. True when point a lies in an annular arc sector around point b: horizontal distance within tolerance of `radius`, |dz| within zRange, and the heading of (a - b) inside (arcStart, arcEnd) (wrapping when arcStart >= arcEnd).
int Point_InArcSector(const float *a, float radius, const float *b, float tolerance,
                      float arcStart, float arcEnd, float zRange)
{
    float delta[3];
    float dist;
    float diff;
    float angle;

    Vec3_Sub(a, b, delta);
    dist = Vec2_Normalize(delta);
    diff = dist - radius;
    if (diff * diff > tolerance * tolerance) {
        return 0;
    }
    if (b[2] - zRange > a[2] || a[2] > b[2] + zRange) {
        return 0;
    }
    angle = Vec2_AngleDegrees(delta);
    angle = AngleMod(angle);
    if (arcEnd > arcStart) {
        if (arcEnd > angle && angle > arcStart) {
            return 1;
        }
    } else if (arcEnd > angle || angle > arcStart) {
        return 1;
    }
    return 0;
}

// 0x080aa974  size=83  FUN_080aa974  status=translated  naming=unknown
// Naming: descriptive. Rotates (p - center) by the angle set and adds center back: out = center + RotateByAngles(p - center, angles).
void Vec3_RotateAboutPoint(const float *p, const float *angles, const float *center, float *out)
{
    float rel[3];
    float rotated[3];

    Vec3_Sub(p, center, rel);
    Vec3_RotateByAngles(rel, angles, rotated);
    Vec3_Add_080ab108(rotated, center, out);
}

// 0x080aa9c8  size=103  FUN_080aa9c8  status=translated  naming=unknown
// Naming: descriptive. Spherical-to-Cartesian with a single angle used for both azimuth and elevation (the binary passes the same argument to SinCos_Float twice): out = radius * (cos*cos, sin*cos, sin).
void Vec3_FromRadiusAngle(float *out, float radius, float angle)
{
    float sinA;
    float cosA;
    float sinB;
    float cosB;

    SinCos_Float(angle, &sinA, &cosA);
    SinCos_Float(angle, &sinB, &cosB);
    out[0] = radius * cosA * cosB;
    out[1] = radius * sinA * cosB;
    out[2] = radius * sinB;
}

// 0x080aacd6  size=244  FUN_080aacd6  status=translated  naming=inferred
// Naming: exact sin/cos for multiples of 90 degrees (after wrapping negatives by +360), otherwise SinCos_Float(deg * pi/180); outputs *outSin, *outCos. Named after the SinCos_Float helper it wraps.
void SinCos_Degrees(float degrees, float *outSin, float *outCos)
{
    if (degrees < 0.0f) {
        degrees = 360.0f + degrees;
    }
    if (degrees == 0.0f) {
        *outCos = 1.0f;
        *outSin = 0.0f;
    } else if (degrees == 90.0f) {
        *outCos = 0.0f;
        *outSin = 1.0f;
    } else if (degrees == 180.0f) {
        *outCos = -1.0f;
        *outSin = 0.0f;
    } else if (degrees == 270.0f) {
        *outCos = 0.0f;
        *outSin = -1.0f;
    } else {
        SinCos_Float((float)(degrees * 0.017453292519943295), outSin, outCos);
    }
}

/* ======================================================================
 * Hunk diagnostics, sound aliases, subtitle localization
 * ====================================================================== */

// 0x080ab7ea  size=214  FUN_080ab7ea  status=translated  naming=inferred
// Naming: registered for the command "meminfo" (Cmd_AddCommand at 0x080aba5f); Q3 Com_Meminfo_f counterpart, prints the hunk totals.
void Com_Meminfo_f(void)
{
    Com_Printf("%8i bytes total hunk\n", hunk_totalBytes);
    Com_Printf("\n");
    Com_Printf("%8i low permanent\n", hunk_lowPermanent);
    if (hunk_lowTemp != hunk_lowPermanent) {
        Com_Printf("%8i low temp\n", hunk_lowTemp);
    }
    Com_Printf("\n");
    Com_Printf("%8i high permanent\n", hunk_highPermanent);
    if (hunk_highTemp != hunk_highPermanent) {
        Com_Printf("%8i high temp\n", hunk_highTemp);
    }
    Com_Printf("\n");
    Com_Printf("%8i total hunk in use\n", hunk_highPermanent + hunk_lowPermanent);
    Com_Printf("\n");
}

// 0x080ab8c0  size=192  FUN_080ab8c0  status=translated  naming=observed
// Naming: string "Com_TouchMemory: %i msec. Using sum: %d" names the function and is referenced here. Touches every 256th byte of the permanent low and high hunk regions and sums the words; the second loop bound uses high.permanent as in the binary.
void Com_TouchMemory(void)
{
    int start;
    int end;
    int sum;
    int i;

    start = Sys_Milliseconds();
    sum = 0;
    for (i = 0; i < (hunk_lowPermanent >> 2); i = i + 0x40) {
        sum = sum + *(int *)(hunk_base + i * 4);
    }
    for (i = (hunk_totalBytes - hunk_highPermanent) >> 2; i < (hunk_highPermanent >> 2); i = i + 0x40) {
        sum = sum + *(int *)(hunk_base + i * 4);
    }
    end = Sys_Milliseconds();
    Com_Printf("Com_TouchMemory: %i msec. Using sum: %d\n", end - start, sum);
}

// 0x080accb2  size=92  FUN_080accb2  status=translated  naming=unknown
// Naming: descriptive. Index of an alias inside its alias list (list found by the alias's own name); returns 0 when the pointer is not in the list.
int SND_GetAliasIndex(struct snd_alias_t *alias)
{
    struct snd_alias_list_t *list;
    struct snd_alias_t *cur;
    int i;

    list = SND_FindAliasList(alias->aliasName);
    cur = list->head;
    i = 0;
    while (true) {
        if (list->count <= i) {
            return 0;
        }
        if (cur == alias) {
            break;
        }
        cur = cur + 1;
        i = i + 1;
    }
    return i;
}

// 0x080acd0e  size=144  FUN_080acd0e  status=translated  naming=observed
// Naming: string "SND_GetAliasWithOffset: could not find sound alias '%s' with offset %d" names the function and is referenced here. Returns the alias at the offset, the first alias when the offset is out of range, or 0 (after Com_Error) when the list is empty.
struct snd_alias_t *SND_GetAliasWithOffset(const char *name, int offset)
{
    struct snd_alias_list_t *list;
    struct snd_alias_t *cur;
    struct snd_alias_t *result;
    int i;

    list = SND_FindAliasList(name);
    cur = list->head;
    i = 0;
    while (true) {
        if (list->count <= i) {
            if (list->count == 0) {
                Com_Error(1, "SND_GetAliasWithOffset: could not find sound alias '%s' with offset %d", name, offset);
                result = 0;
            } else {
                result = list->head;
            }
            return result;
        }
        if (i == offset) {
            break;
        }
        cur = cur + 1;
        i = i + 1;
    }
    return cur;
}

// 0x080ad2e2  size=48  FUN_080ad2e2  status=translated  naming=unknown
// Naming: descriptive. Returns the first word (alias name pointer) of the alias list found by name, or 0 when no list exists.
const char *SND_GetAliasListName_080ad2e2(const char *name)
{
    struct snd_alias_list_t *list;
    const char *result;

    list = SND_FindAliasList(name);
    if (list == 0) {
        result = 0;
    } else {
        result = list->aliasName;
    }
    return result;
}

// 0x080ad47a  size=33  FUN_080ad47a  status=translated  naming=unknown
// Naming: descriptive. Looks up the alias list by name and picks one alias via Com_PickSoundAlias; returns the pick (eax passes through).
struct snd_alias_t *SND_PickAliasByName(const char *name)
{
    struct snd_alias_list_t *list;

    list = SND_FindAliasList(name);
    return Com_PickSoundAlias(list);
}

// 0x080b0fd8  size=528  FUN_080b0fd8  status=translated  naming=inferred
// Naming: strings "Localizing sound alias subtitle text..." / "Writing to StringEd file %s" identify it; builds the subtitle.st OS path, makes a local copy, runs Com_LocalizeSoundAliasFile over every soundaliases/*.csv inside a low-permanent hunk mark, then writes the final StringEd file.
void Com_LocalizeSoundAliasSubtitles(void)
{
    char tmpPath[0x100];
    char stringEdPath[0x104];
    void *stream;
    int numFiles;
    char **fileList;
    int mark;
    int i;
    size_t len;

    FS_BuildOSPath_Fatal(fs_homepath->current.string, "../source_data/string_resources/subtitle.st", "", stringEdPath);
    len = strlen(stringEdPath);
    stringEdPath[len - 1] = '\0';
    stream = Com_Fopen(stringEdPath, "r+");
    if (stream == 0) {
        Com_Printf("WARNING: Can not write to StringEd file %s\n", stringEdPath);
    } else {
        Com_Fclose(stream);
        FS_BuildOSPath_Fatal(fs_basepath->current.string, fs_gamedir, "soundaliases/subtitle.st", tmpPath);
        FS_CopyFile(stringEdPath, tmpPath);
        if (FS_FileExists("soundaliases/subtitle.st") == 0) {
            Com_Printf("WARNING: Could not make local copy of StringEd file %s\n", "soundaliases/subtitle.st");
        } else {
            Com_Printf("Localizing sound alias subtitle text...\n");
            Com_Printf("Writing to StringEd file %s\n", stringEdPath);
            fileList = FS_ListFiles("soundaliases", "csv", 0, &numFiles, 10);
            if (numFiles == 0) {
                Com_Printf("WARNING: can't find any sound alias files (soundaliases/*.csv)\n");
            } else {
                mark = Hunk_PushLowPermanent();
                for (i = 0; i < numFiles; i++) {
                    Com_LocalizeSoundAliasFile(fileList[i], "all_mp", tmpPath);
                    Hunk_ClearTempMemory();
                }
                Hunk_SetLowPermanent(mark);
                FS_FreeFileList(fileList, 10);
                COM_WriteFinalStringEdFile(tmpPath, stringEdPath);
                FS_Remove(tmpPath);
                Com_Printf("done\n");
            }
        }
    }
}
