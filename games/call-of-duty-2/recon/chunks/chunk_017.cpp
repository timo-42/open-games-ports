/*
 * chunk_017: cod2_lnxded 1.3 script value stack, frame stack, variable-list
 * and typed-argument accessors (0x80825d4..0x8085658).
 *
 * Reconstruction notes:
 *  - scrVmPub_top is the value stack top, 8-byte entries {value, type}.
 *  - scrVmPub_function_frame is the frame stack cursor, 24-byte cells addressed as words.
 *  - Variable lists are packed blocks (ScrStackBuffer_s header plus 5-byte
 *    records). Their layout is recovered from the offsets used in the
 *    pseudocode; see chunk_017_types.h.
 *  - Where the original reads a field through a second pointer, the access
 *    is kept literal so that side effects and evaluation order match.
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_017_types.h"

/* Sentinel owner value used for unused thread cells (address of scrVm_noOwnerSentinel). */
#define COD2_NO_OWNER ((const char *)scrVm_noOwnerSentinel)

/* Address of a 32-bit word-sized handle, used where Ghidra typed a pointer as int. */
#define COD2_U32(p) ((uint32)(size_t)(p))
#define COD2_PTR(T, v) ((T *)(size_t)(v))

/* Unaligned little-endian 32-bit helpers for packed records. */
static uint32 cod2_load_u32(const uint8 *p)
{
    uint32 v;
    memcpy(&v, p, 4);
    return v;
}

static void cod2_store_u32(uint8 *p, uint32 v)
{
    memcpy(p, &v, 4);
}

/* ---------------------------------------------------------------------------
 * 0x080825d4 */
// 0x080825d4  size=73  FUN_080825d4  status=translated  naming=unknown
// naming: no strings; returns the owner word of the top frame cell and its value (descriptive name only).
void *Scr_PeekCallerOwner(uint32 *valueOut)
{
    void *owner;

    if (scr_call_depth < 2) {
        return (void *)0;
    }
    owner = COD2_PTR(void, scrVmPub_function_frame[-6]);
    if (owner == (void *)scrVm_noOwnerSentinel) {
        return (void *)0;
    }
    *valueOut = scrVmPub_function_frame[-5];
    return owner;
}

/* ---------------------------------------------------------------------------
 * 0x080829ae */
// 0x080829ae  size=104  FUN_080829ae  status=translated  naming=unknown
// naming: no strings; links an object into the two list heads and the chain tail (descriptive name only).
void Scr_LinkObjectLists(uint32 headId, uint32 objId, uint32 prevId, uint32 nextId, uint32 linkValue)
{
    ScrVar_ReleaseNameAndSetThread(objId);
    ScrVar_RemoveVarBias(nextId, objId);
    if (Scr_GetArraySize(nextId) == 0) {
        ScrVar_RemoveVar(prevId, linkValue);
        if (Scr_GetArraySize(prevId) == 0) {
            ScrVar_RemoveVar(headId, COD2_OBJ_HEAD_SENTINEL);
        }
    }
}

/* ---------------------------------------------------------------------------
 * 0x08082a16 */
// 0x08082a16  size=123  FUN_08082a16  status=translated  naming=unknown
// naming: no strings; wraps Scr_LinkObjectLists for a pair of object ids (descriptive name only).
void Scr_LinkObjectPair(uint32 objId, uint32 otherId)
{
    uint32 listA;
    int16 linkB;
    uint32 listC;
    uint32 listD;

    listA = ScrVar_FindVarValueId(objId, COD2_OBJ_HEAD_SENTINEL);
    listA = Scr_GetVarValue(listA);
    linkB = ScrVar_GetNameLow16(otherId);
    listC = ScrVar_FindVarValueId(listA, linkB);
    listD = Scr_GetVarValue(listC);
    Scr_LinkObjectLists(objId, otherId, listA, listD, (uint32)linkB);
}

/* ---------------------------------------------------------------------------
 * 0x08082a92 */
// 0x08082a92  size=295  FUN_08082a92  status=translated  naming=unknown
// naming: no strings; allocates a packed variable-list block from the value stack, popping frame cells for type-7 entries (descriptive name only).
ScrStackBuffer_s *Scr_SaveStack(int32 count, const char *owner, VariableValue *entries, int32 sideSlots, uint32 *headIo)
{
    int32 size;
    ScrStackBuffer_s *list;
    uint32 head;
    uint8 *end;
    VariableValue *entry;
    int32 remaining;
    uint32 value;

    size = count * 5 + 0x0b;
    list = (ScrStackBuffer_s *)StrPool_AllocBlock(size, 1);
    head = *headIo;
    list->localId = (uint16)head;
    list->size = (uint16)count;
    list->bufLen = (uint16)size;
    list->pos = owner;
    list->time = (uint8)scrVarPub_time;

    scrVmPub.localVars = scrVmPub.localVars - sideSlots;
    end = (uint8 *)list + COD2_VARLIST_HDR_SIZE + count * 5;
    remaining = count;
    entry = entries;
    while (remaining != 0) {
        if (entry->type == COD2_SCR_TYPE_FRAMEREF) {
            scr_call_depth = scr_call_depth - 1;
            scrVmPub_function_frame = scrVmPub_function_frame - 6;
            value = scrVmPub_function_frame[0];
            scrVmPub.localVars = scrVmPub.localVars - (int32)scrVmPub_function_frame[2];
            head = ScrVar_GetInfoHighB(head);
        } else {
            value = entry->u.intValue;
        }
        cod2_store_u32(end - 4, value);
        end = end - 5;
        *end = (uint8)entry->type;
        entry = entry - 1;
        remaining = remaining - 1;
    }
    scr_call_depth = scr_call_depth - 1;
    scrVmPub_function_frame = scrVmPub_function_frame - 6;
    ScrVar_AddRef(head);
    *headIo = head;
    return list;
}

/* ---------------------------------------------------------------------------
 * 0x08082bba */
// 0x08082bba  size=79  FUN_08082bba  status=translated  naming=unknown
// naming: no strings; walks a link chain, appending each id to the side stack and returning the count (descriptive name only).
int32 Scr_PushChainIds(uint32 head)
{
    int32 count;
    uint32 id;

    count = 0;
    for (id = Scr_GetSiblingViaPrevLinks(head); id != 0; id = Scr_GetSiblingViaPrevLinks(id)) {
        scrVmPub.localVars = scrVmPub.localVars + 1;
        *scrVmPub.localVars = id;
        count = count + 1;
    }
    return count;
}

/* ---------------------------------------------------------------------------
 * 0x08082c0a */
// 0x08082c0a  size=401  FUN_08082c0a  status=translated  naming=unknown
// naming: no strings; expands a packed variable list into an active working list and per-thread table cells (descriptive name only).
void Scr_RestoreStack(uint32 objId, uint32 *active, ScrStackBuffer_s *list)
{
    uint32 remaining;
    uint8 *rec;
    uint32 *cursor;
    uint32 head;
    int32 k;
    uint32 value;
    uint32 count;

    *scrVmPub_function_frame = (uint32)(size_t)list->pos;
    scr_call_depth = scr_call_depth + 1;
    scrVmPub_function_frame = scrVmPub_function_frame + 6;

    remaining = list->size;
    rec = (uint8 *)list + COD2_VARLIST_HDR_SIZE;
    cursor = COD2_PTR(uint32, active[4]);
    while (remaining != 0) {
        remaining = remaining - 1;
        cursor[3] = (uint32)*rec;
        value = cod2_load_u32(rec + 1);
        if (cursor[3] == COD2_SCR_TYPE_FRAMEREF) {
            *scrVmPub_function_frame = value;
            scr_call_depth = scr_call_depth + 1;
            scrVmPub_function_frame = scrVmPub_function_frame + 6;
        } else {
            cursor[2] = value;
        }
        rec = rec + 5;
        cursor = cursor + 2;
    }

    active[0] = (uint32)(size_t)list->pos;
    active[3] = COD2_U32(cursor);
    head = (uint32)list->localId;
    active[1] = head;
    ScrVar_SetThreadTypeB(objId);

    k = scr_call_depth;
    for (;;) {
        scr_call_stack[k].localId = head;
        k = k - 1;
        if (k == 0) {
            break;
        }
        head = ScrVar_GetInfoHighB(head);
    }

    k = 0;
    for (;;) {
        k = k + 1;
        if (k == scr_call_depth) {
            break;
        }
        count = (uint32)Scr_PushChainIds(scr_call_stack[k].localId);
        scr_call_stack[k].localVarCount = count;
    }

    count = (uint32)Scr_PushChainIds(active[1]);
    active[2] = count;

    if (list->time != (uint8)scrVarPub_time) {
        GScr_resettimeout();
    }
    StrPool_FreeBlock((uint8 *)list, list->bufLen);
}

/* ---------------------------------------------------------------------------
 * 0x08082d9c */
// 0x08082d9c  size=361  FUN_08082d9c  status=translated  naming=unknown
// naming: no strings; unwinds a packed variable list, calling back per record until a type-7 record and object match stop id (descriptive name only).
void Scr_FreeSavedStack(uint32 stopId, uint32 objId, ScrStackBuffer_s *list)
{
    uint32 count;
    uint32 head;
    uint8 *cursor;
    uint32 value;
    uint32 next;
    int32 type;
    VariableValue entry;
    uint32 handle;

    count = (uint32)list->size;
    head = (uint32)list->localId;
    cursor = (uint8 *)list + COD2_VARLIST_HDR_SIZE + count * 5;
    value = 0;
    next = 0;

    for (;;) {
        for (;;) {
            if (count == 0) {
                ScrVar_RemoveThread(head);
                ScrVar_Release(head);
                StrPool_FreeBlock((uint8 *)list, list->bufLen);
                return;
            }
            value = cod2_load_u32(cursor - 4);
            cursor = cursor - 5;
            type = (int32)(*cursor);
            count = count - 1;
            if (type == COD2_SCR_TYPE_FRAMEREF) {
                break;
            }
            ScrVar_ReleaseValue(type, value);
        }
        next = ScrVar_GetInfoHighB(head);
        ScrVar_RemoveThread(head);
        ScrVar_Release(head);
        if (head == stopId) {
            break;
        }
        head = next;
    }

    count = count + 1;
    *cursor = 0;
    ScrVar_SetTimeThread(objId, scrVarPub_time);
    list->pos = (const char *)(size_t)value;
    list->localId = (uint16)next;
    list->size = (uint16)count;

    entry.u.stackValue = list;
    entry.type = COD2_SCR_TYPE_HANDLE;
    handle = ScrVar_GetNewVarValueId(scrVarPub_timeArrayId, scrVarPub_time);
    handle = Scr_GetOrCreateArrayId(handle);
    handle = ScrVar_NewChildAVarIdBias(handle, objId);
    ScrVar_SetVarValue(handle, &entry);
}

/* ---------------------------------------------------------------------------
 * 0x08082f06 */
// 0x08082f06  size=395  FUN_08082f06  status=translated  naming=unknown
// naming: no strings; detaches a variable list from its object, re-linking the remaining head and optionally clearing the owner (descriptive name only).
void Scr_DetachSavedStack(uint32 objId, ScrStackBuffer_s *list, int8 keepTail)
{
    uint32 count;
    uint32 head;
    uint8 *cursor;
    uint32 value;
    uint32 next;
    int32 type;
    VariableValue entry;
    uint32 handle;

    count = (uint32)list->size;
    head = (uint32)list->localId;
    cursor = (uint8 *)list + COD2_VARLIST_HDR_SIZE + count * 5;
    value = 0;
    next = 0;

    for (;;) {
        for (;;) {
            if (count == 0) {
                if (keepTail != 0) {
                    ScrVar_RemoveVar(objId, COD2_OBJ_TAIL_SENTINEL);
                }
                ScrVar_RemoveThread(objId);
                ScrVar_Release(objId);
                StrPool_FreeBlock((uint8 *)list, list->bufLen);
                return;
            }
            value = cod2_load_u32(cursor - 4);
            cursor = cursor - 5;
            type = (int32)(*cursor);
            count = count - 1;
            if (type == COD2_SCR_TYPE_FRAMEREF) {
                break;
            }
            ScrVar_ReleaseValue(type, value);
        }
        if (ScrVar_FindVarValueIdBias(scrVarPub_notifyArrayId, head) != 0) {
            break;
        }
        next = ScrVar_GetInfoHighB(head);
        ScrVar_RemoveThread(head);
        ScrVar_Release(head);
        head = next;
    }

    count = count + 1;
    list->localId = (uint16)head;
    list->size = (uint16)count;
    ScrVar_ResetAuxToGlobal(head);
    if (keepTail == 0) {
        ScrVar_SetNotifyThread(objId, 0);
        list->pos = 0;
        entry.u.stackValue = list;
        entry.type = COD2_SCR_TYPE_HANDLE;
        handle = ScrVar_NewChildAVarId(objId, COD2_OBJ_TAIL_SENTINEL);
        ScrVar_SetVarValue(handle, &entry);
    }
}

/* ---------------------------------------------------------------------------
 * 0x08083092 */
// 0x08083092  size=118  FUN_08083092  status=translated  naming=unknown
// naming: no strings; marks the trailing per-thread table cells as unused after searching for an id (descriptive name only).
void Scr_PruneThreadRange(uint32 id)
{
    int32 k;
    int32 last;
    uint32 prevId;

    k = scr_call_depth;
    last = scr_call_depth;
    while (scr_call_stack[k].localId != id) {
        prevId = scr_call_stack[k].localId;
        k = k - 1;
        if (ScrVar_GetChildThreadParent(prevId) == 0) {
            last = k;
        }
    }
    for (; k <= last; last = last - 1) {
        scr_call_stack[last].pos = COD2_NO_OWNER;
    }
}

/* ---------------------------------------------------------------------------
 * 0x08083108 */
// 0x08083108  size=190  FUN_08083108  status=translated  naming=unknown
// naming: no strings; releases the variable list of an object from the root table and calls Scr_RestoreStack-style unwinding (descriptive name only).
void Scr_ReleaseObjectVars(uint32 stopId, uint32 objId)
{
    uint32 idA;
    uint32 handleA;
    uint32 handleB;
    uint32 *slot;
    uint32 listPtr;
    uint32 kept;

    idA = ScrVar_GetInfoHigh(objId);
    ScrVar_SetThreadTypeB(objId);
    handleA = ScrVar_FindVarValueId(scrVarPub_timeArrayId, idA);
    handleB = Scr_GetVarValue(handleA);
    handleA = ScrVar_FindVarValueIdBias(handleB, objId);
    slot = ScrVar_GetValuePtr(handleA);
    listPtr = *slot;
    ScrVar_RemoveVarBias(handleB, objId);
    kept = Scr_GetArraySize(handleB);
    if (kept == 0 && idA != scrVarPub_time) {
        ScrVar_RemoveVar(scrVarPub_timeArrayId, idA);
    }
    Scr_FreeSavedStack(stopId, objId, COD2_PTR(ScrStackBuffer_s, listPtr));
}

/* ---------------------------------------------------------------------------
 * 0x080831c6 */
// 0x080831c6  size=155  FUN_080831c6  status=translated  naming=unknown
// naming: no strings; detaches an object from the secondary root and re-links its variable list (descriptive name only).
void Scr_DetachObjectVars(uint32 objId)
{
    uint32 idA;
    uint32 handleA;
    uint32 handleB;
    uint32 *slot;
    int32 empty;

    idA = ScrVar_GetAux(objId);
    handleA = ScrVar_FindVarValueIdBias(scrVarPub_notifyArrayId, idA);
    handleA = Scr_GetVarValue(handleA);
    handleB = ScrVar_FindVarValueIdBias(handleA, objId);
    slot = ScrVar_GetValuePtr(handleB);
    Scr_LinkObjectPair(slot[0], objId);
    ScrVar_RemoveVarBias(handleA, objId);
    empty = Scr_GetArraySize(handleA);
    if (empty == 0) {
        ScrVar_RemoveVarBias(scrVarPub_notifyArrayId, idA);
    }
}

/* ---------------------------------------------------------------------------
 * 0x08083262 */
// 0x08083262  size=383  FUN_08083262  status=translated  naming=unknown
// naming: no strings; sets the variable list of an object, dispatching on the object's link id (descriptive name only).
void Scr_SetObjectVars(uint32 stopId, uint32 objId)
{
    int16 linkId;
    uint32 handleA;
    uint32 handleB;
    uint32 handleC;
    uint32 handleD;
    uint32 handleE;
    uint32 handleF;
    uint32 handleG;
    uint32 *slot;
    uint32 listPtr;
    uint32 listTmp;
    uint32 owner;
    int32 empty;

    linkId = ScrVar_GetNameLow16(objId);
    if (linkId == 0) {
        handleA = ScrVar_FindVarValueId(objId, COD2_OBJ_TAIL_SENTINEL);
        slot = ScrVar_GetValuePtr(handleA);
        listPtr = *slot;
        ScrVar_RemoveVar(objId, COD2_OBJ_TAIL_SENTINEL);
    } else {
        owner = ScrVar_GetAux(objId);
        handleA = ScrVar_FindVarValueIdBias(scrVarPub_notifyArrayId, owner);
        handleB = Scr_GetVarValue(handleA);
        handleA = ScrVar_FindVarValueIdBias(handleB, objId);
        slot = ScrVar_GetValuePtr(handleA);
        listTmp = *slot;
        handleC = ScrVar_FindVarValueId(listTmp, COD2_OBJ_HEAD_SENTINEL);
        handleD = Scr_GetVarValue(handleC);
        handleE = ScrVar_FindVarValueId(handleD, (int32)linkId);
        handleF = Scr_GetVarValue(handleE);
        handleG = ScrVar_FindVarValueIdBias(handleF, objId);
        slot = ScrVar_GetValuePtr(handleG);
        listPtr = *slot;
        Scr_LinkObjectLists(listTmp, objId, handleD, handleF, (uint32)linkId);
        ScrVar_RemoveVarBias(handleB, objId);
        empty = Scr_GetArraySize(handleB);
        if (empty == 0) {
            ScrVar_RemoveVarBias(scrVarPub_notifyArrayId, owner);
        }
    }
    Scr_FreeSavedStack(stopId, objId, COD2_PTR(ScrStackBuffer_s, listPtr));
}

/* ---------------------------------------------------------------------------
 * 0x080833e2 */
// 0x080833e2  size=218  FUN_080833e2  status=translated  naming=unknown
// naming: no strings; looks up the link handle of an object in the root tables (descriptive name only).
uint32 Scr_LookupObjectLink(uint32 unusedOwner, uint32 objId)
{
    int16 linkId;
    uint32 handle;
    uint32 owner;
    uint32 *slot;
    uint32 result;

    (void)unusedOwner;
    linkId = ScrVar_GetNameLow16(objId);
    if (linkId == 0) {
        result = ScrVar_FindVarValueId(objId, COD2_OBJ_TAIL_SENTINEL);
    } else {
        owner = ScrVar_GetAux(objId);
        handle = ScrVar_FindVarValueIdBias(scrVarPub_notifyArrayId, owner);
        handle = Scr_GetVarValue(handle);
        handle = ScrVar_FindVarValueIdBias(handle, objId);
        slot = ScrVar_GetValuePtr(handle);
        handle = ScrVar_FindVarValueId(*slot, COD2_OBJ_HEAD_SENTINEL);
        handle = Scr_GetVarValue(handle);
        handle = ScrVar_FindVarValueId(handle, (int32)linkId);
        handle = Scr_GetVarValue(handle);
        result = ScrVar_FindVarValueIdBias(handle, objId);
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x080834bc */
// 0x080834bc  size=109  FUN_080834bc  status=translated  naming=unknown
// naming: no strings; predicate that tests whether an object is an unparented class-0x10 singleton (descriptive name only).
bool Scr_IsSingletonObject(uint32 objId)
{
    int32 cls;
    uint32 found;
    bool result;

    cls = Scr_GetVarType(objId);
    if (cls == 0x10) {
        if (ScrVar_ResolveChildThreadRoot(objId) == objId) {
            found = Scr_LookupObjectLink(objId, objId);
            cls = Scr_GetVarType(found);
            result = (cls == 0);
        } else {
            result = false;
        }
    } else {
        result = false;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x0808352a */
// 0x0808352a  size=115  FUN_0808352a  status=translated  naming=unknown
// naming: no strings; dispatches object release by class (0x10, 0x0f, 0x11) to the matching variable-list cleanup (descriptive name only).
void Scr_DispatchObjectRelease(uint32 objId)
{
    uint32 handle;
    int32 cls;

    handle = ScrVar_ResolveChildThreadRoot(objId);
    cls = Scr_GetVarType(handle);
    if (cls == 0x10) {
        Scr_SetObjectVars(objId, handle);
    } else if (cls < 0x11) {
        if (cls == 0x0f) {
            Scr_PruneThreadRange(objId);
        }
    } else if (cls == 0x11) {
        Scr_ReleaseObjectVars(objId, handle);
    }
}


/* ---------------------------------------------------------------------------
 * 0x0808359e */
// 0x0808359e  size=1232  FUN_0808359e  status=translated  naming=unknown
// naming: header tag 'w' (byte before the owner string) selects the parameter-matching path; no direct name string (descriptive name only).
void Scr_NotifyObject(uint32 objId, uint32 nameId, VariableValue *args)
{
    uint32 headObj;
    uint32 listObj;
    uint32 node;
    uint32 fnObj;
    uint32 fnSlot;
    uint32 slotHandle;
    uint32 handle;
    int32 kind;
    ScrStackBuffer_s *vl;
    uint32 *slotVal;
    const char *hdr;
    uint8 *rec;
    uint32 nparams;
    uint32 remaining;
    uint32 linkCount;
    uint32 sizeNeeded;
    uint32 bytesOfParams;
    uint32 inserted;
    bool skipAssign;
    ScrStackBuffer_s *newList;
    uint8 *dst;
    VariableValue *argPtr;
    VariableValue *scan;
    VariableValue recEntry;
    VariableValue argEntry;
    VariableValue newEntry;

    headObj = ScrVar_FindVarValueId(objId, COD2_OBJ_HEAD_SENTINEL);
    if (headObj == 0) {
        return;
    }
    headObj = Scr_GetVarValue(headObj);
    listObj = ScrVar_FindVarValueId(headObj, nameId);
    if (listObj == 0) {
        return;
    }
    listObj = Scr_GetVarValue(listObj);
    ScrVar_AddRef(listObj);
    scr_compile_error_deferred = 1;
    node = listObj;

top:
    node = Scr_GetSiblingViaPrevLinks(node);
    if (node == 0) {
        goto done;
    }
    fnObj = ScrVar_GetNameIndex(node);
    fnSlot = ScrVar_GetAux(fnObj);
    handle = ScrVar_FindVarValueIdBias(scrVarPub_notifyArrayId, fnSlot);
    slotHandle = Scr_GetVarValue(handle);
    kind = Scr_GetVarType(node);
    if (kind == 0) {
        Scr_LinkObjectLists(objId, fnObj, headObj, listObj, nameId);
        ScrVar_ReleaseAuxOnly(fnObj);
        ScrVar_RemoveVarBias(slotHandle, fnObj);
        if (Scr_GetArraySize(slotHandle) == 0) {
            ScrVar_RemoveVarBias(scrVarPub_notifyArrayId, fnSlot);
        }
        Scr_DispatchObjectRelease(fnSlot);
        node = listObj;
        goto top;
    }

    slotVal = ScrVar_GetValuePtr(node);
    vl = COD2_PTR(ScrStackBuffer_s, *slotVal);
    hdr = COD2_PTR(const char, vl->pos);
    if (hdr[-1] == 'w') {
        nparams = (uint32)(int32)hdr[0];
        rec = (uint8 *)vl + COD2_VARLIST_HDR_SIZE + ((uint32)vl->size - nparams) * 5;
        argPtr = args;
        remaining = nparams;
        while (remaining != 0) {
            if (argPtr->type == COD2_SCR_TYPE_MARKER) {
                goto top;
            }
            remaining = remaining - 1;
            recEntry.type = (uint32)*rec;
            if (recEntry.type == COD2_SCR_TYPE_MARKER) {
                break;
            }
            recEntry.u.intValue = cod2_load_u32(rec + 1);
            rec = rec + 5;
            Scr_AddRefToValue((struct VariableValue *)(&recEntry));
            argEntry.type = argPtr->type;
            argEntry.u.intValue = argPtr->u.intValue;
            Scr_AddRefToValue((struct VariableValue *)(&argEntry));
            Scr_EvalEquals(&recEntry, &argEntry);
            if (g_scr_compile_error_message != 0) {
                ScrRuntime_Error(vl->pos, ((int32)(int8)hdr[0] - (int32)remaining) + 3, g_scr_compile_error_message, scrVmGlob_dialog_error_message);
                Scr_ClearErrorMessage();
                goto top;
            }
            if (recEntry.u.intValue == 0) {
                goto top;
            }
            argPtr = argPtr - 1;
        }
        vl->pos = vl->pos + 1;
        skipAssign = true;
    } else {
        skipAssign = (args->type == COD2_SCR_TYPE_MARKER);
    }

    newEntry.u.stackValue = vl;
    newEntry.type = COD2_SCR_TYPE_HANDLE;
    handle = ScrVar_GetNewVarValueId(scrVarPub_timeArrayId, scrVarPub_time);
    handle = Scr_GetOrCreateArrayId(handle);
    handle = ScrVar_NewChildAVarIdBias(handle, fnObj);
    ScrVar_SetVarValue(handle, &newEntry);
    slotVal = ScrVar_GetValuePtr(handle);
    Scr_LinkObjectLists(objId, fnObj, headObj, listObj, nameId);
    ScrVar_RemoveVarBias(slotHandle, fnObj);
    if (Scr_GetArraySize(slotHandle) == 0) {
        ScrVar_RemoveVarBias(scrVarPub_notifyArrayId, fnSlot);
    }
    ScrVar_SetTimeThread(fnObj, scrVarPub_time);

    if (!skipAssign) {
        remaining = (uint32)vl->size;
        linkCount = remaining;
        scan = args;
        do {
            linkCount = linkCount + 1;
            scan = scan - 1;
        } while (scan->type != COD2_SCR_TYPE_MARKER);
        sizeNeeded = linkCount * 5 + 0x0b;
        bytesOfParams = remaining * 5;
        newList = vl;
        if (MT_SizeOrderLE(vl->bufLen, (int32)sizeNeeded) == 0) {
            newList = (ScrStackBuffer_s *)StrPool_AllocBlock((int32)sizeNeeded, 1);
            newList->bufLen = (uint16)sizeNeeded;
            newList->pos = vl->pos;
            newList->localId = vl->localId;
            memcpy((uint8 *)newList + COD2_VARLIST_HDR_SIZE, (uint8 *)vl + COD2_VARLIST_HDR_SIZE, bytesOfParams);
            StrPool_FreeBlock((uint8 *)vl, vl->bufLen);
            vl = newList;
            *slotVal = COD2_U32(newList);
        }
        vl->size = (uint16)linkCount;
        dst = (uint8 *)vl + COD2_VARLIST_HDR_SIZE + bytesOfParams;
        inserted = linkCount - remaining;
        do {
            scan = scan + 1;
            Scr_AddRefToValue((struct VariableValue *)(scan));
            *dst = (uint8)scan->type;
            cod2_store_u32(dst + 1, scan->u.intValue);
            dst = dst + 5;
            inserted = inserted - 1;
        } while (inserted != 0);
    }
    node = listObj;
    goto top;

done:
    ScrVar_Release(listObj);
    scr_compile_error_deferred = 0;
}

/* ---------------------------------------------------------------------------
 * 0x08083a6e */
// 0x08083a6e  size=181  FUN_08083a6e  status=translated  naming=unknown
// naming: no strings; runs a nested call against the top argument block, marking the base entry as a separator (descriptive name only).
void Scr_NotifyNum(uint32 selfId, uint32 nameId, uint32 arg3, int32 popCount)
{
    VariableValue *base;
    int32 newCount;
    uint32 savedType;
    uint32 handle;

    Scr_ClearInParams();
    base = scrVmPub_top - popCount;
    newCount = scrVmPub_inparamcount - popCount;
    handle = Scr_FindClassEntityVar(selfId, nameId);
    if (handle != 0) {
        savedType = base->type;
        base->type = COD2_SCR_TYPE_MARKER;
        scrVmPub_inparamcount = 0;
        Scr_NotifyObject(handle, arg3, scrVmPub_top);
        base->type = savedType;
    }
    while (scrVmPub_top != base) {
        Scr_RemoveRefToValue((struct VariableValue *)(scrVmPub_top));
        scrVmPub_top = scrVmPub_top - 1;
    }
    scrVmPub_inparamcount = newCount;
}

/* ---------------------------------------------------------------------------
 * 0x08083b24 */
// 0x08083b24  size=355  FUN_08083b24  status=translated  naming=unknown
// naming: no strings; drains object chains under a root, detaching variable lists by link class 10 or otherwise (descriptive name only).
void Scr_ClearObjectChains(uint32 rootId)
{
    uint32 cell;
    uint32 next;
    uint32 owner;
    uint32 listVal;
    uint32 *slot;
    int32 cls;
    uint32 inner;
    uint32 handle;

    for (;;) {
        cell = ScrVar_FindVarValueId(rootId, COD2_OBJ_HEAD_SENTINEL);
        if (cell == 0) {
            return;
        }
        next = Scr_GetVarValue(cell);
        cell = Scr_GetNextSibling(next);
        if (cell == 0) {
            break;
        }
        next = Scr_GetVarValue(cell);
        cell = Scr_GetNextSibling(next);
        if (cell == 0) {
            return;
        }
        owner = ScrVar_GetNameIndex(cell);
        cls = Scr_GetVarType(cell);
        if (cls == COD2_SCR_TYPE_HANDLE) {
            slot = ScrVar_GetValuePtr(cell);
            listVal = *slot;
            Scr_DetachObjectVars(owner);
            Scr_DetachSavedStack(owner, COD2_PTR(ScrStackBuffer_s, listVal), 0);
        } else {
            ScrVar_AddRef(owner);
            Scr_DetachObjectVars(owner);
            inner = ScrVar_GetAux(owner);
            inner = ScrVar_ResolveChildThreadRoot(inner);
            handle = ScrVar_FindVarValueId(inner, COD2_OBJ_TAIL_SENTINEL);
            if (handle != 0) {
                slot = ScrVar_GetValuePtr(handle);
                Scr_DetachSavedStack(inner, COD2_PTR(ScrStackBuffer_s, *slot), 1);
            }
            ScrVar_ReleaseAuxOnly(owner);
            ScrVar_DropRef(owner);
        }
    }
}

/* ---------------------------------------------------------------------------
 * 0x08083c88 */
// 0x08083c88  size=138  FUN_08083c88  status=translated  naming=unknown
// naming: no strings; releases each variable list hanging off a thread root (descriptive name only).
void Scr_ClearThreadVars(uint32 rootId)
{
    uint32 cell;
    uint32 owner;
    uint32 *slot;
    uint32 listVal;

    ScrVar_AddRef(rootId);
    for (;;) {
        cell = Scr_GetNextSibling(rootId);
        if (cell == 0) {
            break;
        }
        owner = ScrVar_GetNameIndex(cell);
        slot = ScrVar_GetValuePtr(cell);
        listVal = *slot;
        ScrVar_RemoveVarBias(rootId, owner);
        ScrVar_SetThreadTypeB(owner);
        Scr_FreeSavedStack(owner, owner, COD2_PTR(ScrStackBuffer_s, listVal));
    }
    ScrVar_Release(rootId);
}

/* ---------------------------------------------------------------------------
 * 0x08083d12 */
// 0x08083d12  size=229  FUN_08083d12  status=translated  naming=unknown
// naming: no strings; restores each thread's variable list into the value-stack base and releases the thread handle (descriptive name only).
void Scr_ResumeSavedStacks(uint32 rootId)
{
    uint32 cell;
    uint32 owner;
    uint32 *slot;
    uint32 listVal;
    uint32 active[5];
    uint32 result;
    VariableValue *base;

    GScr_resettimeout();
    ScrVar_AddRef(rootId);
    base = scrVmStack;
    active[4] = COD2_U32(base);
    for (;;) {
        cell = Scr_GetNextSibling(rootId);
        if (cell == 0) {
            break;
        }
        owner = ScrVar_GetNameIndex(cell);
        slot = ScrVar_GetValuePtr(cell);
        listVal = *slot;
        ScrVar_RemoveVarBias(rootId, owner);
        Scr_RestoreStack(owner, active, COD2_PTR(ScrStackBuffer_s, listVal));
        result = VM_ExecuteInternal((const char *)(size_t)active[0], active[1], (int)active[2], (VariableValue *)(size_t)active[3], base);
        ScrVar_Release(result);
        Scr_RemoveRefToValue((struct VariableValue *)(base + 1));
    }
    ScrVar_Release(rootId);
    ScrVar_ClearValue(scrVarPub_tempVariable);
    scrVmPub_top = scrVmStack;
}

/* ---------------------------------------------------------------------------
 * 0x08083df8 */
// 0x08083df8  size=59  FUN_08083df8  status=translated  naming=inferred
// naming: reserves one value-stack slot; overflow message 'script stack overflow' is in the sibling Scr_CallFunction. Overflow here reports via Com_Error.
void Scr_ReserveStackSlot(void)
{
    Scr_ClearInParams();
    if (scrVmPub_top == scrVmPub_maxstack) {
        Com_Error(1, g_08145aa0);
    }
    scrVmPub_top = scrVmPub_top + 1;
    scrVmPub_inparamcount = scrVmPub_inparamcount + 1;
}

/* ---------------------------------------------------------------------------
 * 0x08083e34 */
// 0x08083e34  size=417  FUN_08083e34  status=translated  naming=inferred
// naming: string 'script stack overflow (too many embedded function calls)' at this function; depth limit 0x1e.
uint32 Scr_CallFunction(uint32 selfId, uint32 nameId, int32 argc)
{
    uint32 *savedFrame;
    VariableValue *base;
    int32 remaining;
    uint32 *cell;
    uint32 savedType;
    uint32 result;
    int32 count;
    VariableValue *slot;

    Scr_ClearInParams();
    savedFrame = scrVmPub_function_frame;
    base = scrVmPub_top - argc;
    remaining = scrVmPub_inparamcount - argc;
    if (scr_call_depth < 0x1e) {
        if (scr_call_depth != 0) {
            scr_call_depth = scr_call_depth + 1;
            scrVmPub_function_frame = scrVmPub_function_frame + 6;
            savedFrame[7] = 0;
        }
        *scrVmPub_function_frame = nameId;
        cell = scrVmPub_function_frame;
        scr_call_depth = scr_call_depth + 1;
        scrVmPub_function_frame = scrVmPub_function_frame + 6;
        cell[7] = selfId;
        savedType = base->type;
        base->type = COD2_SCR_TYPE_MARKER;
        scrVmPub_inparamcount = 0;
        result = VM_ExecuteInternal((const char *)(size_t)nameId, selfId, 0, scrVmPub_top, base);
        base->type = savedType;
        scrVmPub_top = base + 1;
        scrVmPub_inparamcount = remaining + 1;
        ScrVar_ClearValue(scrVarPub_tempVariable);
        if (scr_call_depth != 0) {
            scr_call_depth = scr_call_depth - 1;
            scrVmPub_function_frame = scrVmPub_function_frame - 6;
        }
    } else {
        ScrVar_RemoveThread(selfId);
        scrVmPub_inparamcount = remaining + 1;
        count = remaining;
        slot = scrVmPub_top;
        for (;;) {
            slot = scrVmPub_top;
            if (count == 0) {
                break;
            }
            Scr_RemoveRefToValue((struct VariableValue *)(scrVmPub_top));
            scrVmPub_top = scrVmPub_top - 1;
            count = count - 1;
        }
        scrVmPub_top = scrVmPub_top + 1;
        *(uint32 *)((uint8 *)slot + 0xc) = 0;
        ScrRuntime_Error((const char *)(size_t)nameId, 0, "script stack overflow (too many embedded function calls)", (const char *)0);
        result = selfId;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08083fd6 */
// 0x08083fd6  size=140  FUN_08083fd6  status=translated  naming=unknown
// naming: no strings; calls a script function by code offset (relative to g_scr_code_base) on the global handle, then pops the call slot (descriptive name only).
uint16 Scr_CallScriptOffset(int32 codeOffset, int32 argc)
{
    uint32 handle;
    uint16 result;

    codeOffset = codeOffset + g_scr_code_base;
    if (scr_call_depth == 0) {
        GScr_resettimeout();
    }
    Scr_IsInCodeRange(codeOffset);
    ScrVar_AddRef(scrVarPub_levelId);
    handle = ScrVar_AllocThread(scrVarPub_levelId);
    result = (uint16)Scr_CallFunction(handle, (uint32)codeOffset, argc);
    Scr_RemoveRefToValue((struct VariableValue *)(scrVmPub_top));
    cod2_store_u32((uint8 *)scrVmPub_top + 4, 0);
    scrVmPub_top = scrVmPub_top - 1;
    scrVmPub_inparamcount = scrVmPub_inparamcount - 1;
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08084062 */
// 0x08084062  size=146  FUN_08084062  status=translated  naming=unknown
// naming: no strings; calls a script method by code offset on an object handle resolved from the id pair (descriptive name only).
uint16 Scr_CallMethod(uint32 ownerId, uint32 nameId, int32 codeOffset, int32 argc)
{
    uint32 handle;
    uint16 result;

    codeOffset = codeOffset + g_scr_code_base;
    if (scr_call_depth == 0) {
        GScr_resettimeout();
    }
    handle = Scr_GetOrCreateClassEntityVar(ownerId, nameId);
    ScrVar_AddRef(handle);
    handle = ScrVar_AllocThread(handle);
    result = (uint16)Scr_CallFunction(handle, (uint32)codeOffset, argc);
    Scr_RemoveRefToValue((struct VariableValue *)(scrVmPub_top));
    cod2_store_u32((uint8 *)scrVmPub_top + 4, 0);
    scrVmPub_top = scrVmPub_top - 1;
    scrVmPub_inparamcount = scrVmPub_inparamcount - 1;
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x080840f4 */
// 0x080840f4  size=104  FUN_080840f4  status=translated  naming=unknown
// naming: no strings; spawns a thread on the global handle by code offset and releases the returned handle (descriptive name only).
void Scr_SpawnThread(int32 codeOffset, int32 argc)
{
    uint32 handle;

    codeOffset = codeOffset + g_scr_code_base;
    if (scr_call_depth == 0) {
        GScr_resettimeout();
    }
    ScrVar_AddRef(scrVarPub_levelId);
    handle = ScrVar_AllocThread(scrVarPub_levelId);
    handle = (uint32)Scr_CallFunction(handle, (uint32)codeOffset, argc);
    ScrVar_Release(handle);
    scrVmPub_outparamcount = scrVmPub_outparamcount + 1;
    scrVmPub_inparamcount = scrVmPub_inparamcount - 1;
}

/* ---------------------------------------------------------------------------
 * 0x0808415c */
// 0x0808415c  size=121  FUN_0808415c  status=translated  naming=unknown
// naming: no strings; spawns a method thread on an object handle resolved from the id pair (descriptive name only).
void Scr_SpawnMethodThread(uint32 ownerId, uint32 nameId, int32 codeOffset, int32 argc)
{
    uint32 handle;

    codeOffset = codeOffset + g_scr_code_base;
    if (scr_call_depth == 0) {
        GScr_resettimeout();
    }
    handle = Scr_GetOrCreateClassEntityVar(ownerId, nameId);
    ScrVar_AddRef(handle);
    handle = ScrVar_AllocThread(handle);
    handle = (uint32)Scr_CallFunction(handle, (uint32)codeOffset, argc);
    ScrVar_Release(handle);
    scrVmPub_outparamcount = scrVmPub_outparamcount + 1;
    scrVmPub_inparamcount = scrVmPub_inparamcount - 1;
}

/* ---------------------------------------------------------------------------
 * 0x080841d6 */
// 0x080841d6  size=27  FUN_080841d6  status=translated  naming=unknown
// naming: no strings; releases one 16-bit handle (descriptive name only).
void Scr_ReleaseHandle(uint16 handle)
{
    ScrVar_Release((uint32)handle);
}

/* ---------------------------------------------------------------------------
 * 0x080842a4 */
// 0x080842a4  size=93  FUN_080842a4  status=translated  naming=unknown
// naming: no strings; tears down the current thread entry if the root is live (descriptive name only).
void Scr_ResumeTickThreads(void)
{
    int32 entry;
    uint32 handle;

    if (scrVarPub_timeArrayId != 0) {
        entry = (int32)ScrVar_FindVarValueId(scrVarPub_timeArrayId, scrVarPub_time);
        if (entry != 0) {
            handle = Scr_GetVarValue((uint32)entry);
            Scr_ResumeSavedStacks(handle);
            ScrVar_RemoveVarIfPresent(scrVarPub_timeArrayId, scrVarPub_time);
        }
    }
}

/* ---------------------------------------------------------------------------
 * 0x08084302 */
// 0x08084302  size=199  FUN_08084302  status=translated  naming=unknown
// naming: no strings; counts (and optionally records) object ids under a root whose class matches (descriptive name only).
int32 Scr_CountMatchingObjects(int32 wantedClass, uint32 rootId, int32 count, int8 filterByClass, uint32 outIds)
{
    uint32 root;
    uint32 cell;
    int32 cls;
    uint32 *slot;
    uint32 id;
    ScrStackBuffer_s *list;

    root = Scr_GetVarValue(rootId);
    cell = Scr_GetNextSibling(root);
    for (;;) {
        if (cell == 0) {
            return count;
        }
        cls = Scr_GetVarType(cell);
        if (cls == COD2_SCR_TYPE_HANDLE) {
            slot = ScrVar_GetValuePtr(cell);
            list = COD2_PTR(ScrStackBuffer_s, *slot);
            for (id = (uint32)list->localId; id != 0; id = ScrVar_GetChildThreadParent(id)) {
                if (filterByClass == 0 || wantedClass == (int32)ScrVar_GetAux(id)) {
                    if (outIds != 0) {
                        cod2_store_u32(COD2_PTR(uint8, outIds + (uint32)count * 4), id);
                    }
                    count = count + 1;
                    break;
                }
            }
        }
        cell = Scr_GetNextSibling(cell);
    }
}

/* ---------------------------------------------------------------------------
 * 0x080844de */
// 0x080844de  size=68  FUN_080844de  status=translated  naming=unknown
// naming: no strings; initialises the four root handles and clears the generation tag (descriptive name only).
void Scr_InitVariableRoots(void)
{
    scrVarPub_timeArrayId = ScrVar_AllocStruct();
    scrVarPub_notifyArrayId = ScrVar_AllocArray();
    scrVarPub_levelId = ScrVar_AllocStruct();
    scrVarPub_animId = ScrVar_AllocStruct();
    scrVarPub_time = 0;
    g_script_error_level = -1;
}

/* ---------------------------------------------------------------------------
 * 0x08084522 */
// 0x08084522  size=341  FUN_08084522  status=translated  naming=unknown
// naming: no strings; tears down the root handles, draining thread and object chains (descriptive name only).
void Scr_ShutdownVariableRoots(uint32 unusedArg, uint32 keepId)
{
    int32 cell;
    uint32 handle;
    uint32 *slot;
    uint32 owner;

    (void)unusedArg;
    Scr_FreeIncludeBlocks();
    Scr_FlushEntityReleaseChain();
    if (scrVarPub_timeArrayId != 0) {
        Scr_ShutdownPersistentArray(keepId);
        for (cell = (int32)Scr_GetNextSibling(scrVarPub_timeArrayId); cell != 0; cell = (int32)Scr_GetNextSibling((uint32)cell)) {
            handle = Scr_GetVarValue((uint32)cell);
            Scr_ClearThreadVars(handle);
        }
        for (;;) {
            cell = (int32)Scr_GetNextSibling(scrVarPub_notifyArrayId);
            if (cell == 0) {
                break;
            }
            handle = Scr_GetVarValue((uint32)cell);
            handle = Scr_GetNextSibling(handle);
            slot = ScrVar_GetValuePtr(handle);
            owner = *slot;
            ScrVar_AddRef(owner);
            Scr_ClearObjectChains(owner);
            ScrVar_Release(owner);
        }
        ScrVar_DestroyObject(scrVarPub_levelId);
        ScrVar_DropRef(scrVarPub_levelId);
        scrVarPub_levelId = 0;
        ScrVar_DestroyObject(scrVarPub_animId);
        ScrVar_DropRef(scrVarPub_animId);
        scrVarPub_animId = 0;
        ScrVar_DestroyObject(scrVarPub_timeArrayId);
        ScrVar_DropRef(scrVarPub_timeArrayId);
        scrVarPub_timeArrayId = 0;
        ScrVar_DropRef(scrVarPub_notifyArrayId);
        scrVarPub_notifyArrayId = 0;
        Scr_SweepEntityVars();
    }
}

/* ---------------------------------------------------------------------------
 * 0x08084678 */
// 0x08084678  size=27  FUN_08084678  status=translated  naming=unknown
// naming: no strings; reports whether the primary root handle is live (descriptive name only).
bool Scr_IsInitialized(bool flag)
{
    (void)flag; /* observed: byte argument stored to a local and otherwise unused */
    return scrVarPub_timeArrayId != 0;
}

/* ---------------------------------------------------------------------------
 * 0x08084694 */
// 0x08084694  size=142  FUN_08084694  status=translated  naming=inferred
// naming: strings 'type %s is not an int' and 'parameter %d does not exist' at this function.
int32 Scr_GetInt(uint32 param)
{
    VariableValue *e;
    const char *msg;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_INT) {
            return (int32)e->u.intValue;
        }
        scrVarPub_error_index = (int32)(param + 1);
        msg = va("type %s is not an int", g_scrTypeNames[e->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x08084722 */
// 0x08084722  size=356  FUN_08084722  status=translated  naming=inferred
// naming: strings 'type %s is not an anim', 'anim \'%s\' in animtree ...' and 'parameter %d does not exist' at this function.
cod2_scr_anim_s *Scr_GetAnim(cod2_scr_anim_s *out, uint32 param, int32 treeId)
{
    VariableValue *e;
    const char *msg;
    int32 treeA;
    int32 treeB;
    uint32 n1;
    uint32 n2;
    uint32 n3;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_ANIM) {
            memcpy(out, &e->u, 4);
            if (treeId == 0) {
                return out;
            }
            treeA = Anim_GetTreeSlot(out->tree);
            treeB = XAnim_GetBlockOwnerWord((const uint32 *)(size_t)treeId);
            if (treeA == treeB) {
                return out;
            }
            n1 = XAnim_GetBlockOwnerWord((const uint32 *)(size_t)treeId);
            n1 = XAnim_LoadU32((const void *)(size_t)n1);
            n2 = (uint32)Anim_GetTreeSlot(out->tree);
            n2 = XAnim_LoadU32((const void *)(size_t)n2);
            n3 = (uint32)Anim_GetTreeSlot(out->tree);
            n3 = (uint32)(size_t)XAnim_NodeDisplayName((struct XAnimNodeTable *)(size_t)n3, out->index);
            g_scr_compile_error_message = va("anim '%s' in animtree '%s' does not belong to the entity's animtree '%s'",
                                     (const char *)(size_t)n3, (const char *)(size_t)n2, (const char *)(size_t)n1);
        } else {
            g_scr_compile_error_message = va("type %s is not an anim", g_scrTypeNames[e->type]);
        }
        Scr_RemoveRefToValue((struct VariableValue *)(e));
        e->type = 0;
        scrVarPub_error_index = (int32)(param + 1);
        Scr_ErrorInternal();
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    out->index = 0;
    out->tree = 0;
    return out;
}

/* ---------------------------------------------------------------------------
 * 0x08084886 */
// 0x08084886  size=225  FUN_08084886  status=translated  naming=inferred
// naming: strings 'bad anim tree', 'type %s is not an animtree' and 'parameter %d does not exist' at this function.
uint32 *Scr_GetAnimTree(uint32 *out, uint32 param)
{
    VariableValue *e;
    const char *msg;
    uint32 idx;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_INT) {
            idx = e->u.intValue;
            if (idx <= g_08205a50 && g_0820584c[idx] != 0) {
                *out = g_0820584c[idx];
                return out;
            }
            g_scr_compile_error_message = "bad anim tree";
        } else {
            g_scr_compile_error_message = va("type %s is not an animtree", g_scrTypeNames[e->type]);
        }
        Scr_RemoveRefToValue((struct VariableValue *)(e));
        e->type = 0;
        scrVarPub_error_index = (int32)(param + 1);
        Scr_ErrorInternal();
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    *out = g_0820584c[0];
    return out;
}

/* ---------------------------------------------------------------------------
 * 0x08084968 */
// 0x08084968  size=159  FUN_08084968  status=translated  naming=inferred
// naming: strings 'type %s is not a float' and 'parameter %d does not exist' at this function.
float Scr_GetFloat(uint32 param)
{
    VariableValue *e;
    const char *msg;
    float result;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_FLOAT) {
            memcpy(&result, &e->u, 4);
            return result;
        }
        if (e->type == COD2_SCR_TYPE_INT) {
            result = (float)(int32)e->u.intValue;
            return result;
        }
        scrVarPub_error_index = (int32)(param + 1);
        msg = va("type %s is not a float", g_scrTypeNames[e->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    result = 0.0f;
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08084a08 */
// 0x08084a08  size=116  FUN_08084a08  status=translated  naming=unknown
// naming: no strings; returns the value of any defined parameter, otherwise raises the parameter error (descriptive name only).
uint32 Scr_GetAny(uint32 param)
{
    VariableValue *e;
    const char *msg;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (Scr_CastString(e) != 0) {
            return e->u.stringValue;
        }
        scrVarPub_error_index = (int32)(param + 1);
        Scr_ErrorInternal();
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x08084a7c */
// 0x08084a7c  size=309  FUN_08084a7c  status=translated  naming=inferred
// naming: call to tolower import and string 'parameter %d does not exist'. Copies into an 8196-byte buffer without bound (original behaviour kept).
uint32 Scr_GetLowerString(uint32 param)
{
    VariableValue *e;
    const char *msg;
    uint32 old;
    uint32 lowerId;
    const char *text;
    int32 i;
    int32 ch;
    char lower[8196];
    uint32 result;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (Scr_CastString(e) != 0) {
            old = e->u.stringValue;
            text = ScrStr_TextFromHandle(old);
            i = 0;
            for (;;) {
                ch = tolower((int)text[i]);
                lower[i] = (char)ch;
                if (text[i] == '\0') {
                    break;
                }
                i = i + 1;
            }
            lowerId = ScrStr_InternCString(lower, 0);
            e->u.stringValue = lowerId;
            ScrStr_Release(old);
            result = e->u.stringValue;
            return result;
        }
        scrVarPub_error_index = (int32)(param + 1);
        Scr_ErrorInternal();
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x08084bb2 */
// 0x08084bb2  size=27  FUN_08084bb2  status=translated  naming=unknown
// naming: no strings; reads any parameter and releases its string reference (descriptive name only).
const char *Scr_ReleaseAnyValue(uint32 param)
{
    uint32 value;

    value = Scr_GetAny(param);
    return ScrStr_TextFromHandle(value);
}

/* ---------------------------------------------------------------------------
 * 0x08084bce */
// 0x08084bce  size=66  FUN_08084bce  status=translated  naming=unknown
// naming: no strings; returns zero for an undefined-typed parameter, otherwise delegates to Scr_GetAny (descriptive name only).
uint32 Scr_GetOptionalAny(uint32 param)
{
    uint32 result;

    if (param < (uint32)scrVmPub_outparamcount && (scrVmPub_top - param)->type == 0) {
        result = 0;
    } else {
        result = Scr_GetAny(param);
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08084c10 */
// 0x08084c10  size=27  FUN_08084c10  status=translated  naming=unknown
// naming: identical body to Scr_ReleaseAnyValue; kept as a separate entry point (descriptive name only).
const char *Scr_ReleaseAnyValueCopy(uint32 param)
{
    uint32 value;

    value = Scr_GetAny(param);
    return ScrStr_TextFromHandle(value);
}

/* ---------------------------------------------------------------------------
 * 0x08084c2c */
// 0x08084c2c  size=106  FUN_08084c2c  status=translated  naming=inferred
// naming: string 'parameter %d does not exist' at this function; returns the string text for a string parameter.
uint32 Scr_GetString(uint32 param)
{
    VariableValue *e;
    const char *msg;
    uint32 result;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        Scr_ConvertToDisplayString(e);
        result = (uint32)(size_t)ScrStr_TextFromHandle(e->u.stringValue);
    } else {
        msg = va("parameter %d does not exist", (int)(param + 1));
        Scr_Error(msg);
        result = 0;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08084c96 */
// 0x08084c96  size=142  FUN_08084c96  status=translated  naming=inferred
// naming: strings 'type %s is not a localized string' and 'parameter %d does not exist' at this function.
uint32 Scr_GetLocalizedString(uint32 param)
{
    VariableValue *e;
    const char *msg;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_LOCSTRING) {
            return e->u.stringValue;
        }
        scrVarPub_error_index = (int32)(param + 1);
        msg = va("type %s is not a localized string", g_scrTypeNames[e->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x08084d24 */
// 0x08084d24  size=27  FUN_08084d24  status=translated  naming=unknown
// naming: no strings; reads a localized-string parameter and releases its reference (descriptive name only).
const char *Scr_ReleaseLocalizedString(uint32 param)
{
    uint32 value;

    value = Scr_GetLocalizedString(param);
    return ScrStr_TextFromHandle(value);
}

/* ---------------------------------------------------------------------------
 * 0x08084d40 */
// 0x08084d40  size=144  FUN_08084d40  status=translated  naming=inferred
// naming: strings 'type %s is not a vector' and 'parameter %d does not exist' at this function.
void Scr_GetVector(uint32 param, float *dst)
{
    VariableValue *e;
    const char *msg;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_VECTOR) {
            Vec3_CopyBits_0808577a((const uint32 *)e->u.vectorValue, (uint32 *)dst);
            return;
        }
        scrVarPub_error_index = (int32)(param + 1);
        msg = va("type %s is not a vector", g_scrTypeNames[e->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
}

/* ---------------------------------------------------------------------------
 * 0x08084dd0 */
// 0x08084dd0  size=150  FUN_08084dd0  status=translated  naming=inferred
// naming: strings 'type %s is not a function' and 'parameter %d does not exist' at this function.
int32 Scr_GetFunction(uint32 param)
{
    VariableValue *e;
    const char *msg;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_FUNCTION) {
            return (int32)e->u.intValue - g_scr_code_base;
        }
        scrVarPub_error_index = (int32)(param + 1);
        msg = va("type %s is not a function", g_scrTypeNames[e->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x08084e66 */
// 0x08084e66  size=252  FUN_08084e66  status=translated  naming=inferred
// naming: strings 'type %s is not an entity' and 'parameter %d does not exist' at this function.
uint32 *Scr_GetEntity(uint32 *out, uint32 param)
{
    VariableValue *e;
    const char *msg;
    uint32 entity;
    int32 cls;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_OBJECT) {
            entity = e->u.pointerValue;
            cls = Scr_GetVarType(entity);
            if (cls == 0x15) {
                *(struct ScrVarInfoPair *)out = Scr_GetVarInfoPair(entity);
                return out;
            }
            scrVarPub_error_index = (int32)(param + 1);
            cls = Scr_GetVarType(entity);
            msg = va("type %s is not an entity", g_scrTypeNames[cls]);
            Scr_Error(msg);
        }
        scrVarPub_error_index = (int32)(param + 1);
        msg = va("type %s is not an entity", g_scrTypeNames[e->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    *out = 0;
    return out;
}

/* ---------------------------------------------------------------------------
 * 0x08084f62 */
// 0x08084f62  size=142  FUN_08084f62  status=translated  naming=inferred
// naming: strings 'type %s is not an object' and 'parameter %d does not exist' at this function.
uint32 Scr_GetObject(uint32 param)
{
    VariableValue *e;
    const char *msg;

    if (param < (uint32)scrVmPub_outparamcount) {
        e = scrVmPub_top - param;
        if (e->type == COD2_SCR_TYPE_OBJECT) {
            return e->u.pointerValue;
        }
        scrVarPub_error_index = (int32)(param + 1);
        msg = va("type %s is not an object", g_scrTypeNames[e->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x08084ff0 */
// 0x08084ff0  size=80  FUN_08084ff0  status=translated  naming=unknown
// naming: no strings; returns the type tag of a parameter (descriptive name only).
uint32 Scr_GetParamType(uint32 param)
{
    uint32 result;

    if (param < (uint32)scrVmPub_outparamcount) {
        result = (scrVmPub_top - param)->type;
    } else {
        Scr_Error(va("parameter %d does not exist", (int)(param + 1)));
        result = 0;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08085040 */
// 0x08085040  size=87  FUN_08085040  status=translated  naming=inferred
// naming: indexes the type-name table g_scrTypeNames by the parameter tag; error string 'parameter %d does not exist'.
const char *Scr_GetParamTypeName(uint32 param)
{
    const char *result;

    if (param < (uint32)scrVmPub_outparamcount) {
        result = g_scrTypeNames[(scrVmPub_top - param)->type];
    } else {
        Scr_Error(va("parameter %d does not exist", (int)(param + 1)));
        result = 0;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08085098 */
// 0x08085098  size=157  FUN_08085098  status=translated  naming=inferred
// naming: strings 'type %s is not a pointer' and 'parameter %d does not exist' at this function.
int32 Scr_GetPointer(uint32 param)
{
    const char *msg;

    if (param < (uint32)scrVmPub_outparamcount) {
        if ((scrVmPub_top - param)->type == COD2_SCR_TYPE_OBJECT) {
            return Scr_GetVarType((scrVmPub_top - param)->u.pointerValue);
        }
        msg = va("type %s is not a pointer", g_scrTypeNames[(scrVmPub_top - param)->type]);
        Scr_Error(msg);
    }
    msg = va("parameter %d does not exist", (int)(param + 1));
    Scr_Error(msg);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x08085136 */
// 0x08085136  size=10  FUN_08085136  status=translated  naming=unknown
// naming: no strings; returns the active parameter count (descriptive name only).
int32 Scr_GetNumParams(void)
{
    return scrVmPub_outparamcount;
}

/* ---------------------------------------------------------------------------
 * 0x08085140 */
// 0x08085140  size=36  FUN_08085140  status=translated  naming=inferred
// naming: pushes an integer (type 6), the type Scr_GetInt accepts.
void Scr_AddInt(int32 value)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_INT;
    scrVmPub_top->u.intValue = (int32)value;
}

/* ---------------------------------------------------------------------------
 * 0x08085164 */
// 0x08085164  size=36  FUN_08085164  status=translated  naming=unknown
// naming: body identical to Scr_AddInt; kept as a separate entry point (descriptive name only).
void Scr_AddIndex(int32 value)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_INT;
    scrVmPub_top->u.intValue = (int32)value;
}

/* ---------------------------------------------------------------------------
 * 0x08085188 */
// 0x08085188  size=36  FUN_08085188  status=translated  naming=inferred
// naming: pushes raw float bits with type 5, the type Scr_GetFloat accepts.
void Scr_AddFloatBits(uint32 bits)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_FLOAT;
    scrVmPub_top->u.intValue = (int32)bits;
}

/* ---------------------------------------------------------------------------
 * 0x080851ac */
// 0x080851ac  size=36  FUN_080851ac  status=translated  naming=inferred
// naming: pushes a packed anim with type 11, the type Scr_GetAnim accepts.
void Scr_AddAnim(uint32 packed)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_ANIM;
    scrVmPub_top->u.intValue = (int32)packed;
}

/* ---------------------------------------------------------------------------
 * 0x080851d0 */
// 0x080851d0  size=25  FUN_080851d0  status=translated  naming=inferred
// naming: pushes an undefined-typed (type 0) value with its value word left as is.
void Scr_AddUndefined(void)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_UNDEFINED;
}

/* ---------------------------------------------------------------------------
 * 0x080851ea */
// 0x080851ea  size=47  FUN_080851ea  status=translated  naming=inferred
// naming: pushes an object handle (type 1) and takes a reference on it.
void Scr_AddObject(uint32 handle)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_OBJECT;
    scrVmPub_top->u.pointerValue = handle;
    ScrVar_AddRef(handle);
}

/* ---------------------------------------------------------------------------
 * 0x0808521a */
// 0x0808521a  size=34  FUN_0808521a  status=translated  naming=unknown
// naming: no strings; looks up an object from an id pair and pushes it (descriptive name only).
void Scr_AddObjectByLookup(uint32 a, uint32 b)
{
    uint32 handle;

    handle = Scr_GetOrCreateClassEntityVar(a, b);
    Scr_AddObject(handle);
}

/* ---------------------------------------------------------------------------
 * 0x0808523c */
// 0x0808523c  size=38  FUN_0808523c  status=translated  naming=unknown
// naming: no strings; allocates a new object, pushes it and releases the allocation reference (descriptive name only).
void GScr_spawnstruct(void)
{
    uint32 handle;

    handle = ScrVar_AllocStruct();
    Scr_AddObject(handle);
    ScrVar_Release(handle);
}

/* ---------------------------------------------------------------------------
 * 0x08085262 */
// 0x08085262  size=57  FUN_08085262  status=translated  naming=inferred
// naming: pushes a string (type 2) interned from text.
void Scr_AddString(const char *text)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_STRING;
    scrVmPub_top->u.stringValue = ScrStr_InternCString(text, 0);
}

/* ---------------------------------------------------------------------------
 * 0x080852d6 */
// 0x080852d6  size=47  FUN_080852d6  status=translated  naming=unknown
// naming: pushes a string id (type 2) and calls the string-id hook (descriptive name only).
void Scr_AddStringId(uint32 id)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_STRING;
    scrVmPub_top->u.stringValue = id;
    ScrStr_AddRef(id);
}

/* ---------------------------------------------------------------------------
 * 0x08085306 */
// 0x08085306  size=49  FUN_08085306  status=translated  naming=inferred
// naming: pushes a vector reference (type 4), the type Scr_GetVector accepts.
void Scr_AddVector(const float *vec)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_VECTOR;
    scrVmPub_top->u.vectorValue = (const float *)Scr_CloneVector((const unsigned int *)vec);
}

/* ---------------------------------------------------------------------------
 * 0x08085338 */
// 0x08085338  size=43  FUN_08085338  status=translated  naming=unknown
// naming: no strings; pushes a fresh entity handle with type 1 (descriptive name only).
void Scr_AddEntity(void)
{
    Scr_ReserveStackSlot();
    scrVmPub_top->type = COD2_SCR_TYPE_OBJECT;
    scrVmPub_top->u.pointerValue = ScrVar_AllocArray();
}

/* ---------------------------------------------------------------------------
 * 0x08085364 */
// 0x08085364  size=81  FUN_08085364  status=translated  naming=unknown
// naming: no strings; pops the top value and links it through the list helpers (descriptive name only).
void Scr_PopAndLink(void)
{
    uint32 handle;

    scrVmPub_top = scrVmPub_top - 1;
    scrVmPub_inparamcount = scrVmPub_inparamcount - 1;
    handle = Scr_GetArraySize(scrVmPub_top->u.pointerValue);
    handle = ScrVar_NewChildAVarId24(scrVmPub_top->u.pointerValue, handle);
    ScrVar_SetVarValue(handle, scrVmPub_top + 1);
}

/* ---------------------------------------------------------------------------
 * 0x080853b6 */
// 0x080853b6  size=69  FUN_080853b6  status=translated  naming=unknown
// naming: no strings; pops the top value and releases it against the given handle (descriptive name only).
void Scr_PopAndRelease(uint32 param1)
{
    uint32 handle;

    scrVmPub_top = scrVmPub_top - 1;
    scrVmPub_inparamcount = scrVmPub_inparamcount - 1;
    handle = ScrVar_NewChildAVarId(scrVmPub_top->u.pointerValue, param1);
    ScrVar_SetVarValue(handle, scrVmPub_top + 1);
}

/* ---------------------------------------------------------------------------
 * 0x080853fc */
// 0x080853fc  size=30  FUN_080853fc  status=translated  naming=inferred
// naming: records the first pending error message and unwinds through Scr_ErrorInternal; the error mechanism is the same as the strings in sibling readers.
void Scr_Error(const char *msg)
{
    if (g_scr_compile_error_message == 0) {
        g_scr_compile_error_message = msg;
    }
    Scr_ErrorInternal();
}

/* ---------------------------------------------------------------------------
 * 0x0808541a */
// 0x0808541a  size=29  FUN_0808541a  status=translated  naming=inferred
// naming: sets the pending error message and its numeric payload, then unwinds (sibling of Scr_Error).
void Scr_ErrorWithCode(const char *msg, const char *code)
{
    g_scr_compile_error_message = msg;
    scrVmGlob_dialog_error_message = code;
    Scr_ErrorInternal();
}

/* ---------------------------------------------------------------------------
 * 0x08085438 */
// 0x08085438  size=36  FUN_08085438  status=translated  naming=inferred
// naming: tears down script state (Scr_DumpScriptThreads, Scr_NoOp), sets the disable flag and raises the error.
void Scr_ErrorAndDisable(const char *msg)
{
    Scr_DumpScriptThreads();
    Scr_NoOp();
    scr_error_fatal = 1;
    Scr_Error(msg);
}

/* ---------------------------------------------------------------------------
 * 0x0808545c */
// 0x0808545c  size=28  FUN_0808545c  status=translated  naming=inferred
// naming: records the 1-based parameter index of the error, then raises it.
void Scr_ErrorAtParam(int32 param, const char *msg)
{
    scrVarPub_error_index = param + 1;
    Scr_Error(msg);
}

/* ---------------------------------------------------------------------------
 * 0x08085478 */
// 0x08085478  size=29  FUN_08085478  status=translated  naming=inferred
// naming: marks the error as having no parameter index (-1), then raises it.
void Scr_ErrorNoParam(const char *msg)
{
    scrVarPub_error_index = -1;
    Scr_Error(msg);
}

/* ---------------------------------------------------------------------------
 * 0x08085496 */
// 0x08085496  size=123  FUN_08085496  status=translated  naming=unknown
// naming: no strings; sets a single-entry argument stack, runs Scr_SetObjectField and pops the entry when the call fails (descriptive name only).
uint32 Scr_ExecuteCall(uint32 a, uint32 b, uint32 c, VariableValue *top)
{
    int32 status;
    uint32 result;

    scrVmPub_outparamcount = 1;
    scrVmPub_top = top;
    status = Scr_SetObjectField(a, b, c);
    if (status == 0) {
        scrVmPub_outparamcount = 0;
        result = 0;
    } else {
        if (scrVmPub_outparamcount != 0) {
            Scr_RemoveRefToValue((struct VariableValue *)(scrVmPub_top));
            scrVmPub_top = scrVmPub_top - 1;
            scrVmPub_outparamcount = 0;
        }
        result = 1;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x08085512 */
// 0x08085512  size=94  FUN_08085512  status=translated  naming=unknown
// naming: no strings; runs Scr_GetObjectField on a static one-entry stack and returns the two result words through the out pointer (descriptive name only).
VariableValue *Scr_RunSetup(VariableValue *out, uint32 a, uint32 b, uint32 c)
{
    uint32 second;

    scrVmPub_top = (VariableValue *)scrVmGlob_setupStack;
    scrVmGlob_setupStack[3] = 0;
    Scr_GetObjectField(a, b, c);
    second = scrVmGlob_setupStack[3];
    scrVmPub_inparamcount = 0;
    out->u.intValue = (int)scrVmGlob_setupStack[2];
    out->type = (int)second;
    return out;
}

/* ---------------------------------------------------------------------------
 * 0x08085570 */
// 0x08085570  size=66  FUN_08085570  status=translated  naming=unknown
// naming: no strings; finishes a call by storing the top value under a handle and popping the stack (descriptive name only).
void Scr_FinishCallAt(uint32 a, uint32 b)
{
    uint32 handle;

    handle = ScrVar_GetVarIdForSet(a, b);
    scrVmPub_inparamcount = 0;
    ScrVar_SetVarValueOrPending(handle, scrVmPub_top);
    scrVmPub_top = scrVmPub_top - 1;
}

/* ---------------------------------------------------------------------------
 * 0x080855b2 */
// 0x080855b2  size=47  FUN_080855b2  status=translated  naming=unknown
// naming: no strings; resolves an object from an id pair and forwards to Scr_FinishCallAt (descriptive name only).
void Scr_FinishCallForObject(uint32 a, uint32 b, uint32 c)
{
    uint32 handle;

    handle = Scr_GetOrCreateClassEntityVar(a, b);
    Scr_FinishCallAt(handle, c);
}

/* ---------------------------------------------------------------------------
 * 0x080855e2 */
// 0x080855e2  size=34  FUN_080855e2  status=translated  naming=unknown
// naming: no strings; enters a nested script level, resetting the current thread and advancing the 24-bit tag (descriptive name only).
void Scr_AdvanceTime(void)
{
    Scr_RunCurrentThreads();
    Scr_FlushEntityReleaseChain();
    scrVarPub_time = (scrVarPub_time + 1) & 0xffffff;
}

/* ---------------------------------------------------------------------------
 * 0x08085604 */
// 0x08085604  size=21  FUN_08085604  status=translated  naming=unknown
// naming: no strings; leaves a nested script level, decrementing the 24-bit tag (descriptive name only).
void Scr_RewindTime(void)
{
    scrVarPub_time = (scrVarPub_time - 1) & 0xffffff;
}

/* ---------------------------------------------------------------------------
 * 0x0808561a */
// 0x0808561a  size=13  FUN_0808561a  status=translated  naming=unknown
// naming: forwards to the thread-entry teardown (descriptive name only).
void Scr_RunCurrentThreads(void)
{
    Scr_ResumeTickThreads();
}

/* ---------------------------------------------------------------------------
 * 0x08085628 */
// 0x08085628  size=31  FUN_08085628  status=translated  naming=unknown
// naming: no strings; samples two timing words, the first shifted to its high half (descriptive name only).
void GScr_resettimeout(void)
{
    scrVmGlob_timestampHigh = Sys_ReadTimestampLow();
    scrVmGlob_timestampHigh = scrVmGlob_timestampHigh >> 0x10;
    scrVmGlob_starttime = Sys_Milliseconds();
}

/* ---------------------------------------------------------------------------
 * 0x08085648 */
// 0x08085648  size=15  FUN_08085648  status=translated  naming=unknown
// naming: resets the value-stack top to its base (descriptive name only).
void Scr_ResetStack(void)
{
    scrVmPub_top = scrVmStack;
}

/* ---------------------------------------------------------------------------
 * 0x08085658 */
// 0x08085658  size=236  FUN_08085658  status=translated  naming=unknown
// naming: no strings; walks a bytecode stream, skipping operands by opcode class until an unlisted opcode (descriptive name only).
void Scr_SkipBytecode(const char *pc)
{
    const char *next;
    const char *op;
    uint32 n;

    next = pc;
    do {
        op = next;
        next = op + 1;
        switch (*(const byte *)op) {
        case 0x00: case 0x01: case 0x02: case 0x03:
        case 0x0d: case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12: case 0x14:
        case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d:
        case 0x20: case 0x21: case 0x23: case 0x24: case 0x25: case 0x26:
        case 0x31: case 0x34: case 0x35: case 0x36: case 0x39: case 0x3c:
        case 0x4c: case 0x4d: case 0x4e: case 0x51: case 0x53: case 0x58: case 0x59:
        case 0x5b: case 0x5c: case 0x5d: case 0x64:
        case 0x65: case 0x66: case 0x67: case 0x68: case 0x69: case 0x6a: case 0x6b:
        case 0x6c: case 0x6d: case 0x6e: case 0x6f: case 0x70: case 0x71: case 0x72:
        case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: case 0x78: case 0x79:
        case 0x7a: case 0x7b: case 0x7e:
            break;
        case 0x04: case 0x05: case 0x17: case 0x1e: case 0x1f: case 0x22:
        case 0x32: case 0x33: case 0x37: case 0x3d: case 0x5a: case 0x85: case 0x86:
            next = op + 2;
            break;
        case 0x06: case 0x07: case 0x16:
        case 0x27: case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d:
        case 0x2e: case 0x2f: case 0x30: case 0x38: case 0x3a: case 0x3b: case 0x3e:
        case 0x3f: case 0x40: case 0x41: case 0x42: case 0x43: case 0x45: case 0x46:
        case 0x47: case 0x48: case 0x49: case 0x4a: case 0x5e: case 0x5f: case 0x60:
        case 0x61: case 0x63:
            op = next;
            Cursor_ReadShortZx(&op);
            next = op;
            break;
        case 0x08: case 0x55: case 0x57: case 0x62: case 0x7c:
            op = next;
            Cursor_ReadInt(&op);
            next = op;
            break;
        case 0x09:
            op = next;
            Cursor_ReadFloat(&op);
            next = op;
            break;
        case 0x0a: case 0x0b:
            op = next;
            Cursor_ReadShortZx(&op);
            next = op;
            break;
        case 0x0c:
            op = next;
            Cursor_AdvanceVec3(&op);
            next = op;
            break;
        case 0x13:
            op = next;
            Cursor_ReadInt(&op);
            next = op;
            break;
        case 0x15: case 0x4f: case 0x50: case 0x52:
            op = next;
            Cursor_ReadIntAlias2(&op);
            next = op;
            break;
        case 0x44: case 0x4b:
            op = op + 2;
            Cursor_ReadShortZx(&op);
            next = op;
            break;
        case 0x54: case 0x56:
            op = next;
            Cursor_ReadIntAlias2(&op);
            Cursor_ReadInt(&op);
            next = op;
            break;
        case 0x7d:
            op = next;
            n = Cursor_ReadShortZx(&op);
            Cursor_AdvanceDwords(&op, (n & 0xffff) * 2);
            next = op;
            break;
        default:
            return;
        }
    } while (true);
}
