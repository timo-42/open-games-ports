#include <ctype.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_014_types.h"

// 0x08079eba  size=70  FUN_08079eba  status=translated  naming=inferred
// sprintf "%i" into a 136-byte buffer, then interned via ScrStr_InternCStringEx(buf,0,14); returns that call's result
int Scr_IntToStringId(int value)
{
    char buf[136];
    sprintf(buf, "%i", value);
    return ScrStr_InternCStringEx(buf, 0, 14);
}

// 0x08079f00  size=96  FUN_08079f00  status=translated  naming=inferred
// string "(%g, %g, %g)" formatted from three floats (promoted to double), interned via ScrStr_InternCStringEx(buf,0,14)
int Scr_VectorToStringId(const float *vec)
{
    char buf[136];
    sprintf(buf, "(%g, %g, %g)", (double)vec[0], (double)vec[1], (double)vec[2]);
    return ScrStr_InternCStringEx(buf, 0, 14);
}

// 0x08079f60  size=165  FUN_08079f60  status=translated  naming=unknown
// no strings; walks the 0x4000-slot string table at 0x08287400, clears a flag mask on the string headers (byte +1) and calls ScrStr_Release until the changed flag sl_last_link_ptr stays 0
void ScrStr_ClearFlagBits(unsigned char mask)
{
    unsigned int i;
    for (i = 1; i < 0x4000; i++) {
        unsigned char *hdr;
        do {
            if ((sl_hash[i].link & 0xc000) == 0)
                break;
            hdr = (unsigned char *)ScrStr_Record(sl_hash[i].handle);
            if ((hdr[1] & mask) == 0)
                break;
            hdr[1] = (unsigned char)(~mask & hdr[1]);
            sl_last_link_ptr = 0;
            ScrStr_Release(sl_hash[i].handle);
        } while (sl_last_link_ptr != 0);
    }
}

// 0x0807a01a  size=141  FUN_0807a01a  status=translated  naming=unknown
// same walk as ScrStr_ClearFlagBits; for headers having any bit of mask, clears mask and sets replacement bits
void ScrStr_ReplaceFlagBits(unsigned char mask, unsigned char replacement)
{
    unsigned int i;
    for (i = 1; i < 0x4000; i++) {
        if ((sl_hash[i].link & 0xc000) != 0) {
            unsigned char *hdr = (unsigned char *)ScrStr_Record(sl_hash[i].handle);
            if ((hdr[1] & mask) != 0) {
                hdr[1] = (unsigned char)(~mask & hdr[1]);
                hdr[1] = (unsigned char)(replacement | hdr[1]);
            }
        }
    }
}

// 0x0807a0a8  size=299  FUN_0807a0a8  status=translated  naming=unknown
// two passes over the string table (ScrStr_ResetOrFree until sl_last_link_ptr stays 0, then list build for headers with flag 4 using strlen+5); callee MT_ResetStats/MT_MarkAllocatedBlock/MT_RebuildFreeLists behave as list begin/add/end
void ScrStr_ProcessTableAndListFlagged(void)
{
    unsigned int i;
    uint8 *list;
    for (i = 1; i < 0x4000; i++) {
        do {
            if ((sl_hash[i].link & 0xc000) == 0)
                break;
            sl_last_link_ptr = 0;
            ScrStr_ResetOrFree(sl_hash[i].handle);
        } while (sl_last_link_ptr != 0);
    }
    list = MT_ResetStats();
    for (i = 1; i < 0x4000; i++) {
        if ((sl_hash[i].link & 0xc000) != 0) {
            unsigned char *hdr = (unsigned char *)ScrStr_Record(sl_hash[i].handle);
            if ((hdr[1] & 4) != 0) {
                const char *str = ScrStr_TextFromHandle(sl_hash[i].handle);
                unsigned int len = ScrStr_Strlen(str);
                MT_MarkAllocatedBlock(list, sl_hash[i].handle, (int)len + 5);
            }
        }
    }
    MT_RebuildFreeLists(list);
}

// 0x0807a1d4  size=166  FUN_0807a1d4  status=translated  naming=inferred
// error text "\x15Filename '%s' exceeds maximum length of %d" at 0x8144880; lowercases a path, collapses leading and repeated / and \ separators, converts \ to /
void Scr_NormalizeFilename(char *dst, const char *src, int maxLen)
{
    unsigned int c;
    do {
        do {
            do {
                c = (unsigned int)(int)*src;
                src++;
            } while (c == '\\');
        } while (c == '/');
        while (0x1f < c) {
            *dst = (char)tolower((int)c);
            dst++;
            maxLen--;
            if (maxLen == 0)
                Com_Error(1, "\025Filename '%s' exceeds maximum length of %d", src, maxLen);
            if (c == '/')
                break;
            c = (unsigned int)(int)*src;
            src++;
            if (c == '\\')
                c = '/';
        }
        if (c == 0) {
            *dst = 0;
            return;
        }
    } while (true);
}

// 0x0807a27a  size=70  FUN_0807a27a  status=translated  naming=inferred
// normalizes the path into a 1032-byte buffer (limit 0x400) then interns it via ScrStr_InternCStringEx(buf,0,7)
int Scr_NormalizeFilenameToStringId(const char *path)
{
    char buf[1032];
    Scr_NormalizeFilename(buf, path, 0x400);
    return ScrStr_InternCStringEx(buf, 0, 7);
}

// 0x0807a2c0  size=19  FUN_0807a2c0  status=translated  naming=unknown
// tail-calls strlen and returns its result
unsigned int ScrStr_Strlen(const char *s)
{
    return (unsigned int)strlen(s);
}

// 0x0807a2d4  size=15  FUN_0807a2d4  status=translated  naming=unknown
// writes 0 to g_scr_buffer_offset
void TempMemory_Reset(void)
{
    g_scr_buffer_offset = 0;
}

// 0x0807a2e4  size=51  FUN_0807a2e4  status=translated  naming=unknown
// advances g_scr_buffer_offset by delta, calls Hunk_ReallocateTempMemory with the new offset and returns its result plus the (re-read) offset
int TempMemory_Reserve(int delta)
{
    int newOffset;
    int result;
    newOffset = delta + g_scr_buffer_offset;
    result = Hunk_ReallocateTempMemory(newOffset);
    result = result + g_scr_buffer_offset;
    g_scr_buffer_offset = newOffset;
    return result;
}

// 0x0807a318  size=19  FUN_0807a318  status=translated  naming=unknown
// wrapper returning TempMemory_Reserve(delta)
int TempMemory_ReserveAliasA(int delta)
{
    return TempMemory_Reserve(delta);
}

// 0x0807a32c  size=19  FUN_0807a32c  status=translated  naming=unknown
// second identical wrapper returning TempMemory_Reserve(delta)
int TempMemory_ReserveAliasB(int delta)
{
    return TempMemory_Reserve(delta);
}

// 0x0807a340  size=50  FUN_0807a340  status=translated  naming=unknown
// undoes part of TempMemory_Reserve: offset -= (TempMemory_Reserve(0) - mark); returns Hunk_ReallocateTempMemory(offset)
int TempMemory_SetPos(int mark)
{
    int cur = TempMemory_Reserve(0);
    g_scr_buffer_offset = g_scr_buffer_offset - (cur - mark);
    return Hunk_ReallocateTempMemory(g_scr_buffer_offset);
}

// 0x0807a374  size=36  FUN_0807a374  status=translated  naming=inferred
// (info & 0x1f) > 0xe: type is an object-like variable (thread/struct/entity/array)
bool ScrVarEntry_IsObjectType(const ScrVarEntry *entry)
{
    return 0xe < (entry->info & 0x1f);
}

// 0x0807a398  size=38  FUN_0807a398  status=translated  naming=inferred
// Scr_GetVarType(id) > 0xe (signed compare)
bool ScrVar_IsObjectTypeById(unsigned int id)
{
    int type = Scr_GetVarType(id);
    return 0xe < type;
}

// 0x0807a3be  size=127  FUN_0807a3be  status=translated  naming=inferred
// qsort comparator for ScrThreadDumpRec: compares stack[] lexicographically up to the shorter depth, then depth
int ScrThreadDumpRec_Compare(const void *a, const void *b)
{
    const ScrThreadDumpRec *ra = (const ScrThreadDumpRec *)a;
    const ScrThreadDumpRec *rb = (const ScrThreadDumpRec *)b;
    int i;
    for (i = 0; i < ra->depth && i < rb->depth; i++) {
        int va = ra->stack[i];
        int vb = rb->stack[i];
        if (va != vb)
            return va - vb;
    }
    return ra->depth - rb->depth;
}

// 0x0807a43e  size=1193  FUN_0807a43e  status=translated  naming=inferred
// strings "Cannot dump script threads: out of memory\n", "count: %d, var usage: %d, endon usage: %d\n", "called from:\n", "ent type '%s'... count: %d, var usage: %d\n"
void Scr_DumpScriptThreads(void)
{
    ScrThreadDumpRec *recs;
    unsigned int id;
    int numRecs;
    int groupStart;
    unsigned int cls;

    recs = (ScrThreadDumpRec *)Com_MallocZeroed_Raw(0x8bfee8);
    if (recs == 0) {
        Com_Printf("Cannot dump script threads: out of memory\n");
        return;
    }
    numRecs = 0;
    for (id = 1; id < 0xfffe; id++) {
        const ScrVarEntry *ent = &g_scrVar[id];
        if ((ent->info & 0x60) != 0 && (ent->info & 0x1f) == SCRVAR_STACK) {
            ScrThreadDumpRec *rec = &recs[numRecs];
            int tmp[32];
            int n = 0;
            char *frame = (char *)ent->u.pointerValue;
            unsigned int count = *(unsigned short *)(frame + 4);
            int topPos = *(int *)frame;
            char *p = frame + 0xb;
            int lastPos = 0;
            int i;
            numRecs++;
            while (count != 0) {
                char kind;
                count--;
                kind = *p;
                lastPos = *(int *)(p + 1);
                p += 5;
                if (kind == 7) {
                    tmp[n] = lastPos;
                    n++;
                }
            }
            tmp[n] = topPos;
            n++;
            rec->varUsage = Scr_GetFrameWeight((ScrStackBuffer *)frame, &rec->endonUsage);
            rec->depth = n;
            n--;
            for (i = 0; i < rec->depth; i++)
                rec->stack[i] = tmp[n - i];
        }
    }
    qsort(recs, (size_t)numRecs, sizeof(ScrThreadDumpRec), ScrThreadDumpRec_Compare);
    Com_Printf("********************************\n");
    groupStart = 0;
    while (groupStart < numRecs) {
        const ScrThreadDumpRec *first = &recs[groupStart];
        int groupCount = 0;
        float sumVar = 0.0f;
        float sumEndon = 0.0f;
        int i;
        do {
            groupCount++;
            sumVar = sumVar + recs[groupStart].varUsage;
            sumEndon = sumEndon + recs[groupStart].endonUsage;
            groupStart++;
            if (numRecs <= groupStart)
                break;
        } while (ScrThreadDumpRec_Compare(first, &recs[groupStart]) == 0);
        Com_Printf("count: %d, var usage: %d, endon usage: %d\n", groupCount, (int)sumVar, (int)sumEndon);
        ScrDbg_PrintLocation(0, (const char *)first->stack[0], 0);
        for (i = 1; i < first->depth; i++) {
            Com_Printf("called from:\n");
            ScrDbg_PrintLocation(0, (const char *)first->stack[i], 0);
        }
    }
    Com_Free_A(recs);
    Com_Printf("********************************\n");
    for (cls = 0; cls < 4; cls++) {
        if (g_scrClassInfo[cls].entArrayId != 0) {
            float sum = 0.0f;
            int entCount = 0;
            unsigned int cur;
            for (cur = Scr_GetNextSibling(g_scrClassInfo[cls].entArrayId); cur != 0; cur = Scr_GetNextSibling(cur)) {
                entCount++;
                if ((g_scrVar[cur].info & 0x1f) == SCRVAR_POINTER)
                    sum = (float)((long double)sum + (long double)Scr_GetVarWeight(g_scrVar[cur].u.pointerValue));
            }
            Com_Printf("ent type '%s'... count: %d, var usage: %d\n", g_scrClassInfo[cls].name, entCount, (int)sum);
        }
    }
    Com_Printf("********************************\n");
}

// 0x0807a8e8  size=5  FUN_0807a8e8  status=translated  naming=unknown
// empty body (single ret)
void Scr_NoOp(void)
{
}

// 0x0807a8ee  size=202  FUN_0807a8ee  status=translated  naming=inferred
// initialises the 0xfffe-entry pool: every entry self-linked and chained into a free list through u.s.refCount (next) and link2 (prev), head in entry 0
void ScrVar_InitFreeList(void)
{
    unsigned short prev = 0;
    unsigned int i;
    unsigned short cur;
    for (i = 1; i < 0xfffe; i++) {
        g_scrVar[i].info = 0;
        cur = (unsigned short)i;
        g_scrVar[i].hashId = cur;
        g_scrVar[i].nextSibling = cur;
        SCRVAR_RC(g_scrVar[prev]) = cur;
        g_scrVar[i].hashPrev = prev;
        prev = cur;
    }
    g_scrVar[0].info = 0;
    g_scrVar[0].hashId = 0;
    g_scrVar[0].nextSibling = 0;
    SCRVAR_RC(g_scrVar[prev]) = 0;
    g_scrVar[0].hashPrev = prev;
}

// 0x0807a9b8  size=77  FUN_0807a9b8  status=translated  naming=inferred
// calls ScrVar_InitFreeList then zeroes fieldObject/entityList of the 4 class entries
void ScrVar_Reset(void)
{
    int i;
    ScrVar_InitFreeList();
    for (i = 0; i < 4; i++) {
        g_scrClassInfo[i].entArrayId = 0;
        g_scrClassInfo[i].id = 0;
    }
}

// 0x0807aa06  size=40  FUN_0807aa06  status=translated  naming=unknown
// if scrVarPub_gameId != 0, releases that id with ScrVar_ReleaseAndFree and clears it
void ScrVar_ReleaseGlobalObject(void)
{
    if (scrVarPub_gameId != 0) {
        ScrVar_ReleaseAndFree(scrVarPub_gameId);
        scrVarPub_gameId = 0;
    }
}

// 0x0807aa2e  size=10  FUN_0807aa2e  status=translated  naming=unknown
// returns 0
unsigned int Scr_ReturnZero(void)
{
    return 0;
}

// 0x0807aa38  size=25  FUN_0807aa38  status=translated  naming=unknown
// (info >> 8) - 0x10000 of the entry
int ScrVar_GetNameIndex(int id)
{
    return (int)((g_scrVar[id].info >> 8) - 0x10000);
}

// 0x0807aa52  size=208  FUN_0807aa52  status=translated  naming=inferred
// hash bucket lookup: slot's variable must be a bucket head (status 0x40), then scans the link12 ring for a variable whose info>>8 equals name; returns the slot or 0
unsigned int ScrVar_FindInBucket(unsigned int name, unsigned int slot)
{
    unsigned int head = g_scrVar[slot].hashId;
    if ((g_scrVar[head].info & 0x60) == 0x40) {
        unsigned short next;
        if ((g_scrVar[head].info >> 8) == name)
            return slot;
        next = g_scrVar[head].nextSibling;
        for (;;) {
            unsigned int cur = next;
            if (&g_scrVar[cur] == &g_scrVar[slot])
                break;
            if ((g_scrVar[g_scrVar[cur].hashId].info >> 8) == name)
                return cur;
            next = g_scrVar[g_scrVar[cur].hashId].nextSibling;
        }
    }
    return 0;
}

// 0x0807ab22  size=66  FUN_0807ab22  status=translated  naming=inferred
// hash = (parent+name) % 0xfffd + 1 as 16 bit, lookup via ScrVar_FindInBucket(name, hash); returns the slot
int ScrVar_FindVariable(int parent, int name)
{
    unsigned short slot = (unsigned short)((unsigned int)(name + parent) % 0xfffd + 1);
    return (int)ScrVar_FindInBucket((unsigned int)name, slot);
}

// 0x0807ab64  size=1279  FUN_0807ab64  status=translated  naming=inferred
// string "exceeded maximum number of script variables" at two allocation points; moves/relinks entries to claim hash slot param_3 for a new variable, increments array element count (type 0x16) and notifies Scr_AddRefToValue
unsigned int ScrVar_AllocateInSlot(int parent, int name, unsigned int slot)
{
    unsigned short freeHead = SCRVAR_RC(g_scrVar[0]);
    ScrVarEntry *slotEnt = &g_scrVar[slot];
    unsigned int occupant = slotEnt->hashId;
    ScrVarEntry *occEnt = &g_scrVar[occupant];
    unsigned int occKind = occEnt->info & 0x60;
    unsigned short slot16 = (unsigned short)slot;
    ScrVarEntry *target = slotEnt;
    unsigned short u1;
    unsigned short u2;
    unsigned short saved;
    unsigned int idx;
    ScrVarEntry *other;
    VariableValue v1;
    VariableValue v2;

    if (occKind == 0) {
        u1 = g_scrVar[slot].nextSibling;
        saved = SCRVAR_RC(g_scrVar[occupant]);
        if (u1 == slotEnt->hashId || (g_scrVar[slot].info & 0x60) != 0) {
            target = occEnt;
        } else {
            g_scrVar[u1].hashId = slotEnt->hashId;
            slotEnt->hashId = slot16;
            g_scrVar[occupant].nextSibling = u1;
            SCRVAR_RC(g_scrVar[occupant]) = SCRVAR_RC(g_scrVar[slot]);
        }
        u1 = g_scrVar[slot].hashPrev;
        SCRVAR_RC(g_scrVar[g_scrVar[u1].hashId]) = saved;
        g_scrVar[saved].hashPrev = u1;
        target->info = 0x40;
        target->nextSibling = slot16;
    } else if (occKind == 0x40) {
        if ((g_scrVar[slot].info & 0x60) == 0) {
            u1 = g_scrVar[slot].nextSibling;
            idx = u1;
            other = &g_scrVar[idx];
            u2 = g_scrVar[idx].hashPrev;
            saved = SCRVAR_RC(g_scrVar[slot]);
            SCRVAR_RC(g_scrVar[g_scrVar[u2].hashId]) = saved;
            g_scrVar[saved].hashPrev = u2;
            other->hashId = slotEnt->hashId;
            slotEnt->hashId = slot16;
            g_scrVar[idx].hashPrev = g_scrVar[slot].hashPrev;
            g_scrVar[g_scrVar[g_scrVar[idx].hashPrev].hashId].prevSibling = u1;
            g_scrVar[g_scrVar[occupant].prevSibling].hashPrev = u1;
            g_scrVar[occupant].info = g_scrVar[occupant].info & 0xffffff9fu;
            g_scrVar[occupant].info = g_scrVar[occupant].info | 0x20;
            g_scrVar[slot].info = 0x40;
        } else {
            slot = SCRVAR_RC(g_scrVar[0]);
            if (slot == 0)
                Scr_ErrorAndDisable("exceeded maximum number of script variables");
            slotEnt = &g_scrVar[slot];
            occupant = slotEnt->hashId;
            target = &g_scrVar[occupant];
            saved = SCRVAR_RC(g_scrVar[occupant]);
            SCRVAR_RC(g_scrVar[0]) = saved;
            g_scrVar[saved].hashPrev = 0;
            g_scrVar[occupant].info = 0x20;
            g_scrVar[occupant].nextSibling = occEnt->nextSibling;
            occEnt->nextSibling = freeHead;
        }
    } else {
        unsigned int newIdx;
        ScrVarEntry *newEnt;
        if ((g_scrVar[slot].info & 0x60) == 0) {
            u1 = g_scrVar[slot].nextSibling;
            newIdx = u1;
            newEnt = &g_scrVar[newIdx];
            u2 = g_scrVar[newIdx].hashPrev;
            saved = SCRVAR_RC(g_scrVar[slot]);
            SCRVAR_RC(g_scrVar[g_scrVar[u2].hashId]) = saved;
            g_scrVar[saved].hashPrev = u2;
        } else {
            u1 = SCRVAR_RC(g_scrVar[0]);
            newIdx = u1;
            if (SCRVAR_RC(g_scrVar[0]) == 0)
                Scr_ErrorAndDisable("exceeded maximum number of script variables");
            newEnt = &g_scrVar[(unsigned int)(newIdx & 0xffff)];
            target = &g_scrVar[newEnt->hashId];
            saved = SCRVAR_RC(g_scrVar[newEnt->hashId]);
            SCRVAR_RC(g_scrVar[0]) = saved;
            g_scrVar[saved].hashPrev = 0;
        }
        u2 = occEnt->prevSibling;
        g_scrVar[g_scrVar[slotEnt->hashPrev].hashId].prevSibling = (unsigned short)newIdx;
        g_scrVar[u2].hashPrev = (unsigned short)newIdx;
        if (occKind == 0x20) {
            unsigned short walk = g_scrVar[occEnt->nextSibling].hashId;
            while ((unsigned int)g_scrVar[walk].nextSibling != slot)
                walk = g_scrVar[g_scrVar[walk].nextSibling].hashId;
            g_scrVar[walk].nextSibling = (unsigned short)newIdx;
        } else {
            occEnt->nextSibling = (unsigned short)newIdx;
        }
        newEnt->hashPrev = slotEnt->hashPrev;
        u1 = newEnt->hashId;
        newEnt->hashId = slotEnt->hashId;
        slotEnt->hashId = u1;
        target->info = 0x40;
        target->nextSibling = slot16;
    }
    target->info = (unsigned int)(unsigned char)target->info;
    target->info = ((unsigned int)name << 8) | target->info;
    if ((g_scrVar[parent].info & 0x1f) == SCRVAR_ARRAY) {
        SCRVAR_AUX(g_scrVar[parent]) = (unsigned short)(SCRVAR_AUX(g_scrVar[parent]) + 1);
        ScrVar_NameToValue(&v1, (unsigned int)name);
        v2.u.intValue = v1.u.intValue;
        v2.type = v1.type;
        Scr_AddRefToValue(&v2);
    }
    return slot;
}

// 0x0807b064  size=171  FUN_0807b064  status=translated  naming=inferred
// calls ScrVar_AllocateInSlot then links the new slot into the parent's child ring after link14
int ScrVar_NewChildA(int parent, int name, unsigned int slot)
{
    int newSlot = (int)ScrVar_AllocateInSlot(parent, name, slot);
    unsigned short oldLink = g_scrVar[parent].prevSibling;
    g_scrVar[g_scrVar[newSlot].hashId].prevSibling = oldLink;
    g_scrVar[oldLink].hashPrev = (unsigned short)newSlot;
    g_scrVar[newSlot].hashPrev = g_scrVar[parent].nextSibling;
    g_scrVar[parent].prevSibling = (unsigned short)newSlot;
    return newSlot;
}

// 0x0807b110  size=213  FUN_0807b110  status=translated  naming=inferred
// like ScrVar_NewChildA but links the new slot on the opposite side of the parent's child ring
int ScrVar_NewChildB(int parent, int name, unsigned int slot)
{
    unsigned short a;
    unsigned short b;
    unsigned short c;
    int newSlot = (int)ScrVar_AllocateInSlot(parent, name, slot);
    a = g_scrVar[g_scrVar[parent].prevSibling].hashPrev;
    b = g_scrVar[a].hashPrev;
    c = g_scrVar[b].hashId;
    g_scrVar[g_scrVar[newSlot].hashId].prevSibling = g_scrVar[parent].nextSibling;
    g_scrVar[a].hashPrev = (unsigned short)newSlot;
    g_scrVar[newSlot].hashPrev = b;
    g_scrVar[c].prevSibling = (unsigned short)newSlot;
    return newSlot;
}

// 0x0807b1e6  size=71  FUN_0807b1e6  status=translated  naming=inferred
// hash = (name+parent) % 0xfffd + 1; ScrVar_NewChildA
int ScrVar_NewChildAByHash(int parent, int name)
{
    return ScrVar_NewChildA(parent, name, (unsigned int)(name + parent) % 0xfffd + 1);
}

// 0x0807b22e  size=71  FUN_0807b22e  status=translated  naming=inferred
// hash = (name+parent) % 0xfffd + 1; ScrVar_NewChildB
int ScrVar_NewChildBByHash(int parent, int name)
{
    return ScrVar_NewChildB(parent, name, (unsigned int)(name + parent) % 0xfffd + 1);
}

// 0x0807b276  size=110  FUN_0807b276  status=translated  naming=inferred
// find-or-create: ScrVar_FindInBucket on hash slot, else ScrVar_NewChildA
int ScrVar_GetOrNewChild(int parent, int name)
{
    int slot = (int)((unsigned int)(name + parent) % 0xfffd + 1);
    int found = (int)ScrVar_FindInBucket((unsigned int)name, (unsigned int)slot);
    if (found == 0)
        found = ScrVar_NewChildA(parent, name, (unsigned int)slot);
    return found;
}

// 0x0807b2e4  size=549  FUN_0807b2e4  status=translated  naming=inferred
// unlinks one child slot from its parent's ring and returns it to the free state (status 0x60); for array parents (type 0x16) decrements the element count and calls Scr_RemoveRefToValue
void ScrVar_UnlinkChild(ScrVarEntry *slotEnt, ScrVarEntry *parent)
{
    int slotIdx = (int)(slotEnt - g_scrVar);
    unsigned short varId = slotEnt->hashId;
    ScrVarEntry *varEnt = &g_scrVar[varId];
    VariableValue v1;
    VariableValue v2;

    if ((parent->info & 0x1f) == SCRVAR_ARRAY) {
        unsigned int nm;
        SCRVAR_AUX(*parent) = (unsigned short)(SCRVAR_AUX(*parent) - 1);
        nm = g_scrVar[varId].info >> 8;
        ScrVar_NameToValue(&v1, nm);
        v2.u.intValue = v1.u.intValue;
        v2.type = v1.type;
        Scr_RemoveRefToValue(&v2);
    }
    if ((varEnt->info & 0x60) == 0x40) {
        unsigned short l12 = varEnt->nextSibling;
        ScrVarEntry *other = &g_scrVar[l12];
        unsigned int otherVar = other->hashId;
        if (other != slotEnt) {
            unsigned short slotLink2;
            unsigned short varLink14;
            unsigned short otherLink2;
            unsigned int tmp;
            g_scrVar[otherVar].info = g_scrVar[otherVar].info & 0xffffff9fu;
            g_scrVar[otherVar].info = g_scrVar[otherVar].info | 0x40;
            slotLink2 = slotEnt->hashPrev;
            varLink14 = varEnt->prevSibling;
            otherLink2 = g_scrVar[l12].hashPrev;
            g_scrVar[g_scrVar[otherVar].prevSibling].hashPrev = (unsigned short)slotIdx;
            g_scrVar[g_scrVar[otherLink2].hashId].prevSibling = (unsigned short)slotIdx;
            g_scrVar[varLink14].hashPrev = l12;
            g_scrVar[g_scrVar[slotLink2].hashId].prevSibling = l12;
            tmp = *(unsigned int *)slotEnt;
            *(unsigned int *)slotEnt = *(unsigned int *)other;
            *(unsigned int *)other = tmp;
            slotIdx = l12;
        }
    } else {
        ScrVarEntry *prev = slotEnt;
        ScrVarEntry *walk = varEnt;
        ScrVarEntry *next = slotEnt;
        do {
            prev = next;
            next = &g_scrVar[walk->nextSibling];
            walk = &g_scrVar[next->hashId];
        } while (next != slotEnt);
        g_scrVar[prev->hashId].nextSibling = varEnt->nextSibling;
    }
    varEnt->info = varEnt->info & 0xffffff9fu;
    varEnt->info = varEnt->info | 0x60;
    varEnt->nextSibling = (unsigned short)slotIdx;
}

// 0x0807b50a  size=199  FUN_0807b50a  status=translated  naming=inferred
// unlinks every child of the object (ScrVar_UnlinkChild), then releases each child id (ScrVar_ReleaseAndFree)
void ScrVar_FreeChildren(unsigned int id)
{
    ScrVarEntry *slotEnt = &g_scrVar[g_scrVar[id].prevSibling];
    unsigned short next = slotEnt->hashId;
    unsigned int cur;
    while (cur = next, cur != id) {
        ScrVar_UnlinkChild(slotEnt, &g_scrVar[id]);
        slotEnt = &g_scrVar[g_scrVar[cur].prevSibling];
        next = slotEnt->hashId;
    }
    next = g_scrVar[g_scrVar[id].prevSibling].hashId;
    while (cur = next, cur != id) {
        next = g_scrVar[g_scrVar[cur].prevSibling].hashId;
        ScrVar_ReleaseAndFree((int)cur);
    }
}

// 0x0807b5d2  size=41  FUN_0807b5d2  status=translated  naming=inferred
// addref, free all children, drop ref
void ScrVar_DestroyObject(unsigned int id)
{
    ScrVar_AddRef((int)id);
    ScrVar_FreeChildren(id);
    ScrVar_DropRef((int)id);
}

// 0x0807b5fc  size=87  FUN_0807b5fc  status=translated  naming=inferred
// sets type 0x10 (notify thread) keeping only the low byte of info, then stores value<<8
void ScrVar_SetNotifyThread(int id, int value)
{
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = (unsigned int)(unsigned char)g_scrVar[id].info;
    g_scrVar[id].info = g_scrVar[id].info | 0x10;
    g_scrVar[id].info = ((unsigned int)value << 8) | g_scrVar[id].info;
}

// 0x0807b654  size=74  FUN_0807b654  status=translated  naming=inferred
// if the first child slot is not the object itself frees the children; then releases the object stored in aux
void ScrVar_ClearChildrenAndReleaseAux(unsigned int id)
{
    if ((unsigned int)g_scrVar[g_scrVar[id].prevSibling].hashId != id)
        ScrVar_FreeChildren(id);
    ScrVar_Release(SCRVAR_AUX(g_scrVar[id]));
}

// 0x0807b69e  size=54  FUN_0807b69e  status=translated  naming=inferred
// clears/releases as above, then stores scrVarPub_levelId into aux and adds a reference to it
void ScrVar_ResetAuxToGlobal(int id)
{
    ScrVar_ClearChildrenAndReleaseAux((unsigned int)id);
    SCRVAR_AUX(g_scrVar[id]) = (unsigned short)scrVarPub_levelId;
    ScrVar_AddRef((int)scrVarPub_levelId);
}

// 0x0807b6d4  size=83  FUN_0807b6d4  status=translated  naming=inferred
// releases the string id in info bits 8..23 (ScrStr_Release) and sets the type to 0xf (thread)
void ScrVar_ReleaseNameAndSetThread(int id)
{
    unsigned int strId = ScrVar_GetNameLow16(id);
    ScrStr_Release(strId);
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = g_scrVar[id].info | 0xf;
}

// 0x0807b728  size=56  FUN_0807b728  status=translated  naming=inferred
// sets type 0xf
void ScrVar_SetThreadType(int id)
{
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = g_scrVar[id].info | 0xf;
}

// 0x0807b760  size=23  FUN_0807b760  status=translated  naming=unknown
// (info >> 8) & 0xffff
unsigned int ScrVar_GetNameLow16(int id)
{
    return (g_scrVar[id].info >> 8) & 0xffff;
}

// 0x0807b778  size=103  FUN_0807b778  status=translated  naming=inferred
// sets type 0x11 (time thread) keeping low byte of info, then stores value<<8
void ScrVar_SetTimeThread(int id, int value)
{
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = (unsigned int)(unsigned char)g_scrVar[id].info;
    g_scrVar[id].info = g_scrVar[id].info | 0x11;
    g_scrVar[id].info = ((unsigned int)value << 8) | g_scrVar[id].info;
}

// 0x0807b7e0  size=56  FUN_0807b7e0  status=translated  naming=inferred
// sets type 0xf (duplicate of ScrVar_SetThreadType)
void ScrVar_SetThreadTypeB(int id)
{
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = g_scrVar[id].info | 0xf;
}

// 0x0807b818  size=20  FUN_0807b818  status=translated  naming=unknown
// info >> 8
unsigned int ScrVar_GetInfoHigh(int id)
{
    return g_scrVar[id].info >> 8;
}

// 0x0807b82c  size=20  FUN_0807b82c  status=translated  naming=unknown
// info >> 8 (duplicate)
unsigned int ScrVar_GetInfoHighB(int id)
{
    return g_scrVar[id].info >> 8;
}

// 0x0807b840  size=58  FUN_0807b840  status=translated  naming=inferred
// for type 0x12 (child thread) returns info>>8 (parent), else 0
unsigned int ScrVar_GetChildThreadParent(int id)
{
    unsigned int result;
    if ((g_scrVar[id].info & 0x1f) == SCRVAR_CHILD_THREAD)
        result = g_scrVar[id].info >> 8;
    else
        result = 0;
    return result;
}

// 0x0807b87a  size=51  FUN_0807b87a  status=translated  naming=inferred
// follows type-0x12 links through info>>8 until a non child-thread variable
unsigned int ScrVar_ResolveChildThreadRoot(unsigned int id)
{
    while ((g_scrVar[id].info & 0x1f) == SCRVAR_CHILD_THREAD)
        id = g_scrVar[id].info >> 8;
    return id;
}

// 0x0807b8ae  size=257  FUN_0807b8ae  status=translated  naming=inferred
// clears children, removes the thread's notify table (Scr_LinkObjectPair per notify entry), then marks the variable as type 0x17 (removed thread)
void ScrVar_RemoveThread(int id)
{
    int notifyRoot;
    ScrVar_ClearChildrenAndReleaseAux((unsigned int)id);
    notifyRoot = ScrVar_FindVarValueIdBias(scrVarPub_notifyArrayId, id);
    if (notifyRoot != 0) {
        unsigned int inner = Scr_GetVarValue((unsigned int)notifyRoot);
        for (;;) {
            int child = (int)Scr_GetNextSibling(inner);
            unsigned int nameLow;
            unsigned int pairId;
            unsigned int *valuePtr;
            if (child == 0)
                break;
            nameLow = (g_scrVar[child].info >> 8) & 0xffff;
            pairId = ScrVar_FindVarValueIdBias((int)inner, (int)nameLow);
            valuePtr = ScrVar_GetValuePtr((int)pairId);
            Scr_LinkObjectPair(*valuePtr, nameLow);
            ScrVar_ReleaseAuxOnly((int)nameLow);
            ScrVar_RemoveVarBias((int)inner, (int)nameLow);
        }
        ScrVar_RemoveVarBias(scrVarPub_notifyArrayId, id);
    }
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = g_scrVar[id].info | 0x17;
}

// 0x0807b9b0  size=71  FUN_0807b9b0  status=translated  naming=inferred
// releases the aux object and marks the variable as type 0x17 (removed thread)
void ScrVar_ReleaseAuxOnly(int id)
{
    ScrVar_Release(SCRVAR_AUX(g_scrVar[id]));
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = g_scrVar[id].info | 0x17;
}

// 0x0807b9f8  size=248  FUN_0807b9f8  status=translated  naming=inferred
// string "exceeded maximum number of script variables" at 0x8144... when the free list head (entry 0 refCount) is empty; pops the head, relinks hash/ring fields and returns the entry's var id
unsigned short ScrVar_AllocVariable(void)
{
    unsigned short head = SCRVAR_RC(g_scrVar[0]);
    unsigned int headIdx;
    ScrVarEntry *headEnt;
    unsigned int occupant;
    ScrVarEntry *occEnt;

    if (head == 0)
        Scr_ErrorAndDisable("exceeded maximum number of script variables");
    headIdx = head;
    headEnt = &g_scrVar[headIdx];
    occupant = headEnt->hashId;
    occEnt = &g_scrVar[occupant];
    SCRVAR_RC(g_scrVar[0]) = SCRVAR_RC(g_scrVar[occupant]);
    if (headEnt != occEnt && (g_scrVar[headIdx].info & 0x60) == 0) {
        unsigned short l12 = g_scrVar[headIdx].nextSibling;
        g_scrVar[l12].hashId = headEnt->hashId;
        headEnt->hashId = head;
        g_scrVar[occupant].nextSibling = l12;
        SCRVAR_RC(g_scrVar[occupant]) = SCRVAR_RC(g_scrVar[headIdx]);
        occEnt = headEnt;
    }
    g_scrVar[SCRVAR_RC(g_scrVar[0])].hashPrev = 0;
    occEnt->nextSibling = head;
    occEnt->prevSibling = head;
    g_scrVar[headIdx].hashPrev = head;
    return headEnt->hashId;
}

// 0x0807baf0  size=193  FUN_0807baf0  status=translated  naming=inferred
// returns an entry to the free list (info = 0, refCount = old head, head = entry link12)
void ScrVar_FreeVariable(int id)
{
    unsigned short l12 = g_scrVar[id].nextSibling;
    unsigned short l12prev = g_scrVar[l12].hashPrev;
    unsigned short l14 = g_scrVar[id].prevSibling;
    g_scrVar[l14].hashPrev = l12prev;
    g_scrVar[g_scrVar[l12prev].hashId].prevSibling = l14;
    g_scrVar[id].info = 0;
    SCRVAR_RC(g_scrVar[id]) = SCRVAR_RC(g_scrVar[0]);
    g_scrVar[l12].hashPrev = 0;
    g_scrVar[SCRVAR_RC(g_scrVar[0])].hashPrev = l12;
    SCRVAR_RC(g_scrVar[0]) = l12;
}

// 0x0807bbb2  size=62  FUN_0807bbb2  status=translated  naming=inferred
// allocates a variable and sets info = 0x60 (no type)
unsigned int ScrVar_AllocValue(void)
{
    unsigned int id = ScrVar_AllocVariable() & 0xffff;
    g_scrVar[id].info = 0x60;
    g_scrVar[id].info = g_scrVar[id].info;
    return id;
}

// 0x0807bbf0  size=74  FUN_0807bbf0  status=translated  naming=inferred
// allocates a variable of type 0x13 (struct) with refCount 0
unsigned int ScrVar_AllocStruct(void)
{
    unsigned int id = ScrVar_AllocVariable() & 0xffff;
    g_scrVar[id].info = 0x60;
    g_scrVar[id].info = g_scrVar[id].info | 0x13;
    SCRVAR_RC(g_scrVar[id]) = 0;
    return id;
}

// 0x0807bc3a  size=110  FUN_0807bc3a  status=translated  naming=inferred
// allocates type 0x15 (entity): class number in info>>8, entity number in aux
unsigned int ScrVar_AllocEntity(int classNum, unsigned short entNum)
{
    unsigned int id = ScrVar_AllocVariable() & 0xffff;
    g_scrVar[id].info = 0x60;
    g_scrVar[id].info = g_scrVar[id].info | 0x15;
    g_scrVar[id].info = ((unsigned int)classNum << 8) | g_scrVar[id].info;
    SCRVAR_RC(g_scrVar[id]) = 0;
    SCRVAR_AUX(g_scrVar[id]) = entNum;
    return id;
}

// 0x0807bca8  size=83  FUN_0807bca8  status=translated  naming=inferred
// allocates type 0x16 (array) with refCount 0 and aux 0
unsigned int ScrVar_AllocArray(void)
{
    unsigned int id = ScrVar_AllocVariable() & 0xffff;
    g_scrVar[id].info = 0x60;
    g_scrVar[id].info = g_scrVar[id].info | 0x16;
    SCRVAR_RC(g_scrVar[id]) = 0;
    SCRVAR_AUX(g_scrVar[id]) = 0;
    return id;
}

// 0x0807bcfc  size=84  FUN_0807bcfc  status=translated  naming=inferred
// allocates type 0xf (thread) with refCount 0 and aux = argument
unsigned int ScrVar_AllocThread(unsigned short aux)
{
    unsigned int id = ScrVar_AllocVariable() & 0xffff;
    g_scrVar[id].info = 0x60;
    g_scrVar[id].info = g_scrVar[id].info | 0xf;
    SCRVAR_RC(g_scrVar[id]) = 0;
    SCRVAR_AUX(g_scrVar[id]) = aux;
    return id;
}

// 0x0807bd50  size=102  FUN_0807bd50  status=translated  naming=inferred
// allocates type 0x12 (child thread): parent in info>>8, aux = first argument
unsigned int ScrVar_AllocChildThread(unsigned short aux, int parent)
{
    unsigned int id = ScrVar_AllocVariable() & 0xffff;
    g_scrVar[id].info = 0x60;
    g_scrVar[id].info = g_scrVar[id].info | 0x12;
    g_scrVar[id].info = ((unsigned int)parent << 8) | g_scrVar[id].info;
    SCRVAR_RC(g_scrVar[id]) = 0;
    SCRVAR_AUX(g_scrVar[id]) = aux;
    return id;
}

// 0x0807bdb6  size=18  FUN_0807bdb6  status=translated  naming=unknown
// returns aux (u16 at +6)
unsigned short ScrVar_GetAux(int id)
{
    return SCRVAR_AUX(g_scrVar[id]);
}

// 0x0807bdc8  size=66  FUN_0807bdc8  status=translated  naming=inferred
// releases the payload by type (ScrVar_ReleaseValue) then frees the entry
void ScrVar_ReleaseAndFree(int id)
{
    ScrVar_ReleaseValue((int)(g_scrVar[id].info & 0x1f), g_scrVar[id].u.pointerValue);
    ScrVar_FreeVariable(id);
}

// 0x0807be0a  size=34  FUN_0807be0a  status=translated  naming=inferred
// refCount++ (u16)
void ScrVar_AddRef(int id)
{
    SCRVAR_RC(g_scrVar[id]) = (unsigned short)(SCRVAR_RC(g_scrVar[id]) + 1);
}

// 0x0807be2c  size=247  FUN_0807be2c  status=translated  naming=inferred
// refcount release: at 0 frees the entry (destroying children first when needed); otherwise decrements and, when an entity (type 0x15) hits 0 as bucket head, turns it into type 0x14 and removes it from its class entity list
void ScrVar_Release(unsigned int id)
{
    if (SCRVAR_RC(g_scrVar[id]) == 0) {
        if ((unsigned int)g_scrVar[g_scrVar[id].prevSibling].hashId != id)
            ScrVar_DestroyObject(id);
        ScrVar_FreeVariable((int)id);
    } else {
        SCRVAR_RC(g_scrVar[id]) = (unsigned short)(SCRVAR_RC(g_scrVar[id]) - 1);
        if (SCRVAR_RC(g_scrVar[id]) == 0 && (g_scrVar[id].info & 0x1f) == SCRVAR_ENTITY &&
            (unsigned int)g_scrVar[g_scrVar[id].prevSibling].hashId == id) {
            g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
            g_scrVar[id].info = g_scrVar[id].info | 0x14;
            ScrVar_RemoveVar24(g_scrClassInfo[g_scrVar[id].info >> 8].entArrayId, SCRVAR_AUX(g_scrVar[id]));
        }
    }
}

// 0x0807bf24  size=64  FUN_0807bf24  status=translated  naming=inferred
// refCount == 0 -> ScrVar_FreeVariable, else refCount--
void ScrVar_DropRef(int id)
{
    if (SCRVAR_RC(g_scrVar[id]) == 0)
        ScrVar_FreeVariable(id);
    else
        SCRVAR_RC(g_scrVar[id]) = (unsigned short)(SCRVAR_RC(g_scrVar[id]) - 1);
}

// 0x0807bf64  size=33  FUN_0807bf64  status=translated  naming=unknown
// returns refCount
unsigned short ScrVar_GetRefCount(int id)
{
    return SCRVAR_RC(g_scrVar[id]);
}

// 0x0807bf86  size=49  FUN_0807bf86  status=translated  naming=inferred
// allocates 0x10 bytes (tag 2) via StrPool_AllocBlock, zeroes the first dword (header) and returns pointer + 4
unsigned int *Scr_AllocVectorBlock(void)
{
    unsigned int *block = (unsigned int *)StrPool_AllocBlock(0x10, 2);
    *block = 0;
    return block + 1;
}

// 0x0807bfb8  size=61  FUN_0807bfb8  status=translated  naming=inferred
// allocates a vector block and copies three dwords from the source
unsigned int *Scr_CloneVector(const unsigned int *src)
{
    unsigned int *dst = Scr_AllocVectorBlock();
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    return dst;
}

// 0x0807bff6  size=35  FUN_0807bff6  status=translated  naming=inferred
// if the byte before the vector is 0, increments the u16 at vector-4 (reference count)
void Scr_VectorAddRef(char *vec)
{
    if (*(vec - 1) == '\0')
        *(unsigned short *)(vec - 4) = (unsigned short)(*(unsigned short *)(vec - 4) + 1);
}

// 0x0807c01a  size=74  FUN_0807c01a  status=translated  naming=inferred
// if the byte before the vector is 0: when the u16 at vector-4 is 0 frees the block (StrPool_FreeBlock(vec-4,0x10)), else decrements it
void Scr_VectorRelease(char *vec)
{
    if (*(vec - 1) == '\0') {
        if (*(unsigned short *)(vec - 4) == 0)
            StrPool_FreeBlock((uint8 *)(vec - 4), 0x10);
        else
            *(unsigned short *)(vec - 4) = (unsigned short)(*(unsigned short *)(vec - 4) - 1);
    }
}

// 0x0807c064  size=79  FUN_0807c064  status=translated  naming=inferred
// add-ref by value type: 1 -> ScrVar_AddRef, 2/3 -> ScrStr_AddRef (string), 4 -> Scr_VectorAddRef
void ScrVar_AddRefValue(int type, unsigned int value)
{
    if (type < 4) {
        if (type < 2) {
            if (type == 1)
                ScrVar_AddRef((int)value);
        } else {
            ScrStr_AddRef(value);
        }
    } else if (type == 4) {
        Scr_VectorAddRef((char *)value);
    }
}

// 0x0807c0b4  size=79  FUN_0807c0b4  status=translated  naming=inferred
// release by value type: 1 -> ScrVar_Release, 2/3 -> ScrStr_Release (string), 4 -> Scr_VectorRelease
void ScrVar_ReleaseValue(int type, unsigned int value)
{
    if (type < 4) {
        if (type < 2) {
            if (type == 1)
                ScrVar_Release(value);
        } else {
            ScrStr_Release(value);
        }
    } else if (type == 4) {
        Scr_VectorRelease((char *)value);
    }
}

// 0x0807c104  size=38  FUN_0807c104  status=translated  naming=inferred
// (unsigned)(x + 0x7e0002) < 0xfe0002
bool Scr_IsSmallNameIndex(int x)
{
    return (unsigned int)(x + 0x7e0002) < 0xfe0002u;
}

// 0x0807c12a  size=18  FUN_0807c12a  status=translated  naming=inferred
// (x + 0x800000) & 0xffffff
unsigned int Scr_BiasName24(int x)
{
    return ((unsigned int)x + 0x800000u) & 0xffffff;
}

// 0x0807c13c  size=36  FUN_0807c13c  status=translated  naming=inferred
// ScrVar_FindVariable(parent, (x + 0x800000) & 0xffffff)
int ScrVar_FindVariable24(int parent, int x)
{
    return ScrVar_FindVariable(parent, (int)(((unsigned int)x + 0x800000u) & 0xffffff));
}

// 0x0807c160  size=36  FUN_0807c160  status=translated  naming=inferred
// var id of ScrVar_FindVariable24
unsigned short ScrVar_FindVarValueId24(int parent, int x)
{
    int slot = ScrVar_FindVariable24(parent, x);
    return g_scrVar[slot].hashId;
}

// 0x0807c184  size=36  FUN_0807c184  status=translated  naming=inferred
// var id of ScrVar_FindVariable
unsigned short ScrVar_FindVarValueId(int parent, int name)
{
    int slot = ScrVar_FindVariable(parent, name);
    return g_scrVar[slot].hashId;
}

// 0x0807c1a8  size=41  FUN_0807c1a8  status=translated  naming=inferred
// var id of ScrVar_FindVariable(parent, x + 0x10000)
unsigned short ScrVar_FindVarValueIdBias(int parent, int x)
{
    int slot = ScrVar_FindVariable(parent, x + 0x10000);
    return g_scrVar[slot].hashId;
}

// 0x0807c1d2  size=36  FUN_0807c1d2  status=translated  naming=inferred
// ScrVar_GetOrNewChild(parent, (x + 0x800000) & 0xffffff)
int ScrVar_GetOrNewChild24(int parent, int x)
{
    return ScrVar_GetOrNewChild(parent, (int)(((unsigned int)x + 0x800000u) & 0xffffff));
}

// 0x0807c1f6  size=36  FUN_0807c1f6  status=translated  naming=inferred
// ScrVar_NewChildAByHash(parent, (x + 0x800000) & 0xffffff)
int ScrVar_NewChildAByHash24(int parent, int x)
{
    return ScrVar_NewChildAByHash(parent, (int)(((unsigned int)x + 0x800000u) & 0xffffff));
}

// 0x0807c21a  size=177  FUN_0807c21a  status=translated  naming=inferred
// string "cannot set field of %s" with the type-name table at 0x0817d960; plain objects use ScrVar_GetNewVarValueId, entities (type 0x15) use field lookup and the pending-field globals with sentinel 0xfffe
int ScrVar_GetVarIdForSet(int id, int name)
{
    unsigned int type = g_scrVar[id].info & 0x1f;
    int result;
    if (type < 0x14) {
        result = ScrVar_GetNewVarValueId(id, name);
    } else if (type == 0x15) {
        result = ScrVar_FindVarValueId(id, name);
        if (result == 0) {
            scrVarPub_pendingFieldObjectId = id;
            scrVarPub_pendingFieldNameId = name;
            result = 0xfffe;
        }
    } else {
        const char *msg = va("cannot set field of %s", g_scrTypeNames[type]);
        Scr_Error(msg);
        result = 0;
    }
    return result;
}

// 0x0807c2cc  size=146  FUN_0807c2cc  status=translated  naming=inferred
// looks the field up; if absent and the object is an entity reads the class field (ScrVar_ReadClassField), else returns type 0 (value left unset as in the binary)
VariableValue *ScrVar_ReadFieldValue(VariableValue *out, int id, int name)
{
    int found;
    int unsetValue;
    found = ScrVar_FindVarValueId(id, name);
    if (found == 0) {
        if ((g_scrVar[id].info & 0x1f) == SCRVAR_ENTITY) {
            ScrVar_ReadClassField(out, id, name);
        } else {
            out->u.intValue = unsetValue;
            out->type = 0;
        }
    } else {
        ScrVar_ReadVarValue(out, found);
    }
    return out;
}

// 0x0807c35e  size=541  FUN_0807c35e  status=translated  naming=inferred
// enumerates child names of an object: entities (0x15) list class fields not set on the entity (name - 0x800000 <= g_scr_next_slot_number), arrays (0x16) list all child names, objects of type >= 0xf list names excluding 0x1fffe/0x1ffff; writes into out[] when non-null and returns the count
int ScrVar_EnumerateNames(unsigned int id, int *out)
{
    int count = 0;
    unsigned int type = g_scrVar[id].info & 0x1f;
    unsigned int root;
    unsigned short next;
    unsigned int cur;

    if (type == 0x15) {
        root = g_scrClassInfo[g_scrVar[id].info >> 8].id;
        next = g_scrVar[g_scrVar[root].prevSibling].hashId;
        while (cur = next, cur != root) {
            unsigned int nm = (g_scrVar[cur].info >> 8) - 0x800000;
            if (nm <= g_scr_next_slot_number && ScrVar_FindVarValueId((int)id, (int)nm) == 0) {
                if (out != 0)
                    out[count] = (int)nm;
                count++;
            }
            next = g_scrVar[g_scrVar[cur].prevSibling].hashId;
        }
    } else {
        if (0x15 < type) {
            if (type != 0x16)
                return 0;
            next = g_scrVar[g_scrVar[id].prevSibling].hashId;
            while (cur = next, cur != id) {
                if (out != 0)
                    out[count] = (int)(g_scrVar[cur].info >> 8);
                count++;
                next = g_scrVar[g_scrVar[cur].prevSibling].hashId;
            }
            return count;
        }
        if (type < 0xf)
            return 0;
    }
    next = g_scrVar[g_scrVar[id].prevSibling].hashId;
    while (cur = next, cur != id) {
        unsigned int nm = g_scrVar[cur].info >> 8;
        if (nm != 0x1fffe && nm != 0x1ffff) {
            if (out != 0)
                out[count] = (int)nm;
            count++;
        }
        next = g_scrVar[g_scrVar[cur].prevSibling].hashId;
    }
    return count;
}

// 0x0807c57c  size=97  FUN_0807c57c  status=translated  naming=inferred
// maps a biased variable name back to a (value,type) pair: < 0x10000 -> string id (type 2), < 0x1fffe -> (name-0x10000, type 1), else (name-0x800000, type 6)
VariableValue *ScrVar_NameToValue(VariableValue *out, unsigned int name)
{
    if (name < 0x10000) {
        out->type = 2;
        out->u.intValue = (int)(name & 0xffff);
    } else if (name < 0x1fffe) {
        out->type = 1;
        out->u.intValue = (int)(name - 0x10000);
    } else {
        out->type = 6;
        out->u.intValue = (int)(name - 0x800000);
    }
    return out;
}

// 0x0807c5de  size=210  FUN_0807c5de  status=translated  naming=inferred
// if the field is not a plain child and the object is an entity, looks it up in the class field object and calls Scr_ExecuteCall with the destination's VariableValue (type zeroed first); if the child exists calls ScrVar_RemoveVar
void ScrVar_GetFieldIntoTarget(int id, int name, ScrVarSetTarget *dest)
{
    int found = ScrVar_FindVariable(id, name);
    if (found == 0) {
        if ((g_scrVar[id].info & 0x1f) == SCRVAR_ENTITY) {
            unsigned int info = g_scrVar[id].info;
            int fieldVar = ScrVar_FindVarValueId24(g_scrClassInfo[info >> 8].id, name);
            if (fieldVar != 0) {
                dest->value.type = 0;
                Scr_ExecuteCall(info >> 8, SCRVAR_AUX(g_scrVar[id]), g_scrVar[fieldVar].u.pointerValue, &dest->value);
            }
        }
    } else {
        ScrVar_RemoveVar(id, name);
    }
}

// 0x0807c6b0  size=36  FUN_0807c6b0  status=translated  naming=inferred
// var id of ScrVar_GetOrNewChild24
unsigned short ScrVar_GetOrNewVarValueId24(int parent, int x)
{
    int slot = ScrVar_GetOrNewChild24(parent, x);
    return g_scrVar[slot].hashId;
}

// 0x0807c6d4  size=36  FUN_0807c6d4  status=translated  naming=inferred
// var id of ScrVar_NewChildAByHash24
unsigned short ScrVar_NewChildAVarId24(int parent, int x)
{
    int slot = ScrVar_NewChildAByHash24(parent, x);
    return g_scrVar[slot].hashId;
}

// 0x0807c6f8  size=36  FUN_0807c6f8  status=translated  naming=inferred
// var id of ScrVar_GetOrNewChild
unsigned short ScrVar_GetNewVarValueId(int parent, int name)
{
    int slot = ScrVar_GetOrNewChild(parent, name);
    return g_scrVar[slot].hashId;
}

// 0x0807c71c  size=36  FUN_0807c71c  status=translated  naming=inferred
// var id of ScrVar_NewChildAByHash
unsigned short ScrVar_NewChildAVarId(int parent, int name)
{
    int slot = ScrVar_NewChildAByHash(parent, name);
    return g_scrVar[slot].hashId;
}

// 0x0807c740  size=41  FUN_0807c740  status=translated  naming=inferred
// var id of ScrVar_GetOrNewChild(parent, x + 0x10000)
unsigned short ScrVar_GetNewVarValueIdBias(int parent, int x)
{
    int slot = ScrVar_GetOrNewChild(parent, x + 0x10000);
    return g_scrVar[slot].hashId;
}

// 0x0807c76a  size=41  FUN_0807c76a  status=translated  naming=inferred
// var id of ScrVar_NewChildAByHash(parent, x + 0x10000)
unsigned short ScrVar_NewChildAVarIdBias(int parent, int x)
{
    int slot = ScrVar_NewChildAByHash(parent, x + 0x10000);
    return g_scrVar[slot].hashId;
}

// 0x0807c794  size=41  FUN_0807c794  status=translated  naming=inferred
// var id of ScrVar_NewChildBByHash(parent, x + 0x10000)
unsigned short ScrVar_NewChildBVarIdBias(int parent, int x)
{
    int slot = ScrVar_NewChildBByHash(parent, x + 0x10000);
    return g_scrVar[slot].hashId;
}

// 0x0807c7be  size=93  FUN_0807c7be  status=translated  naming=inferred
// finds the child slot (no null check), unlinks it from the parent and releases the variable
void ScrVar_RemoveVar(int parent, int name)
{
    int slot = ScrVar_FindVariable(parent, name);
    unsigned short varId = g_scrVar[slot].hashId;
    ScrVar_UnlinkChild(&g_scrVar[slot], &g_scrVar[parent]);
    ScrVar_ReleaseAndFree(varId);
}

// 0x0807c81c  size=92  FUN_0807c81c  status=translated  naming=inferred
// unlinks and releases the first slot of the parent's child ring
void ScrVar_RemoveFirstChild(int parent)
{
    unsigned short varId = g_scrVar[g_scrVar[parent].prevSibling].hashId;
    ScrVar_UnlinkChild(&g_scrVar[g_scrVar[parent].prevSibling], &g_scrVar[parent]);
    ScrVar_ReleaseAndFree(varId);
}

// 0x0807c878  size=31  FUN_0807c878  status=translated  naming=inferred
// ScrVar_RemoveVar(parent, x + 0x10000)
void ScrVar_RemoveVarBias(int parent, int x)
{
    ScrVar_RemoveVar(parent, x + 0x10000);
}

// 0x0807c898  size=36  FUN_0807c898  status=translated  naming=inferred
// ScrVar_RemoveVarIfPresent(parent, (x + 0x800000) & 0xffffff)
void ScrVar_RemoveVarIfPresent24(int parent, int x)
{
    ScrVar_RemoveVarIfPresent(parent, (int)(((unsigned int)x + 0x800000u) & 0xffffff));
}

// 0x0807c8bc  size=36  FUN_0807c8bc  status=translated  naming=inferred
// ScrVar_RemoveVar(parent, (x + 0x800000) & 0xffffff)
void ScrVar_RemoveVar24(int parent, int x)
{
    ScrVar_RemoveVar(parent, (int)(((unsigned int)x + 0x800000u) & 0xffffff));
}

// 0x0807c8e0  size=101  FUN_0807c8e0  status=translated  naming=inferred
// like ScrVar_RemoveVar but does nothing when the child is absent
void ScrVar_RemoveVarIfPresent(int parent, int name)
{
    int slot = ScrVar_FindVariable(parent, name);
    if (slot != 0) {
        unsigned short varId = g_scrVar[slot].hashId;
        ScrVar_UnlinkChild(&g_scrVar[slot], &g_scrVar[parent]);
        ScrVar_ReleaseAndFree(varId);
    }
}

// 0x0807c946  size=357  FUN_0807c946  status=translated  naming=inferred
// deep copy of all children of src into dst (recursing into arrays, add-ref for other values)
void ScrVar_CopyChildren(unsigned int src, unsigned int dst)
{
    unsigned short next = g_scrVar[g_scrVar[src].prevSibling].hashId;
    unsigned int cur;
    while (cur = next, cur != src) {
        unsigned int type = g_scrVar[cur].info & 0x1f;
        int slot = ScrVar_GetOrNewChild((int)dst, (int)(g_scrVar[cur].info >> 8));
        unsigned int newVar = g_scrVar[slot].hashId;
        g_scrVar[newVar].info = type | g_scrVar[newVar].info;
        if (type == 1) {
            if ((g_scrVar[g_scrVar[cur].u.pointerValue].info & 0x1f) == SCRVAR_ARRAY) {
                unsigned int arr = ScrVar_AllocArray();
                g_scrVar[newVar].u.pointerValue = arr;
                ScrVar_CopyChildren(g_scrVar[cur].u.pointerValue, g_scrVar[newVar].u.pointerValue);
            } else {
                g_scrVar[newVar].u.pointerValue = g_scrVar[cur].u.pointerValue;
                ScrVar_AddRef((int)g_scrVar[cur].u.pointerValue);
            }
        } else {
            g_scrVar[newVar].u.pointerValue = g_scrVar[cur].u.pointerValue;
            ScrVar_AddRefValue((int)type, g_scrVar[cur].u.pointerValue);
        }
        next = g_scrVar[g_scrVar[cur].prevSibling].hashId;
    }
}

// 0x0807caac  size=101  FUN_0807caac  status=translated  naming=inferred
// releases the old payload by type, then stores new type bits and value from the (value,type) pair
void ScrVar_SetVarValueReleasing(int id, VariableValue *v)
{
    ScrVar_ReleaseValue((int)(g_scrVar[id].info & 0x1f), g_scrVar[id].u.pointerValue);
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
    g_scrVar[id].info = (unsigned int)v->type | g_scrVar[id].info;
    g_scrVar[id].u.pointerValue = (unsigned int)v->u.intValue;
}

// 0x0807cb12  size=57  FUN_0807cb12  status=translated  naming=inferred
// ORs the type bits into info and stores the value, without releasing the old payload
void ScrVar_SetVarValue(int id, VariableValue *v)
{
    g_scrVar[id].info = (unsigned int)v->type | g_scrVar[id].info;
    g_scrVar[id].u.pointerValue = (unsigned int)v->u.intValue;
}

// 0x0807cb4c  size=32  FUN_0807cb4c  status=translated  naming=unknown
// returns &entry.u.pointerValue
unsigned int *ScrVar_GetValuePtr(int id)
{
    return &g_scrVar[id].u.pointerValue;
}

// 0x0807cb6c  size=198  FUN_0807cb6c  status=translated  naming=inferred
// for an entity: looks up the class field and tries Scr_ExecuteCall; if absent or it returns 0, stores the value as a plain child variable
void ScrVar_SetClassFieldValue(int id, int name, VariableValue *v)
{
    int fieldVar = ScrVar_FindVarValueId24(g_scrClassInfo[g_scrVar[id].info >> 8].id, name);
    if (fieldVar == 0 ||
        (char)Scr_ExecuteCall(g_scrVar[id].info >> 8, SCRVAR_AUX(g_scrVar[id]), g_scrVar[fieldVar].u.pointerValue, v) == '\0') {
        int var = ScrVar_NewChildAVarId(id, name);
        g_scrVar[var].info = (unsigned int)v->type | g_scrVar[var].info;
        g_scrVar[var].u.pointerValue = (unsigned int)v->u.intValue;
    }
}

// 0x0807cc32  size=70  FUN_0807cc32  status=translated  naming=inferred
// releases the payload by type and clears the type bits (keeps value word)
void ScrVar_ClearValue(int id)
{
    ScrVar_ReleaseValue((int)(g_scrVar[id].info & 0x1f), g_scrVar[id].u.pointerValue);
    g_scrVar[id].info = g_scrVar[id].info & 0xffffffe0u;
}

// 0x0807cc78  size=66  FUN_0807cc78  status=translated  naming=inferred
// id 0xfffe is the pending entity-field marker set by ScrVar_GetVarIdForSet: routes to ScrVar_SetClassFieldValue(scrVarPub_pendingFieldObjectId, scrVarPub_pendingFieldNameId, v); else ScrVar_SetVarValueReleasing
void ScrVar_SetVarValueOrPending(int id, VariableValue *v)
{
    if (id == 0xfffe)
        ScrVar_SetClassFieldValue(scrVarPub_pendingFieldObjectId, scrVarPub_pendingFieldNameId, v);
    else
        ScrVar_SetVarValueReleasing(id, v);
}

// 0x0807ccba  size=68  FUN_0807ccba  status=translated  naming=inferred
// out = (type = info & 0x1f, value = u.value), then Scr_AddRefToValue(out)
VariableValue *ScrVar_ReadVarValue(VariableValue *out, int id)
{
    out->type = (int)(g_scrVar[id].info & 0x1f);
    out->u.intValue = (int)g_scrVar[id].u.pointerValue;
    Scr_AddRefToValue(out);
    return out;
}

// 0x0807ccfe  size=126  FUN_0807ccfe  status=translated  naming=inferred
// string "%s is not a field object"; returns the object id when the variable is a pointer (type 1) to an object of type < 0x16, otherwise reports an error and returns 0
int ScrVar_GetFieldObjectId(int id)
{
    unsigned int type = g_scrVar[id].info & 0x1f;
    int result;
    if (type == 1 && (type = g_scrVar[g_scrVar[id].u.pointerValue].info & 0x1f, type < 0x16)) {
        result = (int)g_scrVar[id].u.pointerValue;
    } else {
        const char *msg = va("%s is not a field object", g_scrTypeNames[type]);
        Scr_Error(msg);
        result = 0;
    }
    return result;
}

// 0x0807cd7c  size=272  FUN_0807cd7c  status=translated  naming=inferred
// reads a class field of an entity via Scr_RunSetup; when the result is a pointer to a non-empty array (type 0x16, refCount != 0) it is copied into a fresh array (release, alloc, ScrVar_CopyChildren)
VariableValue *ScrVar_ReadClassField(VariableValue *out, int id, int name)
{
    ScrVarEntry *ent = &g_scrVar[id];
    int fieldVar = ScrVar_FindVarValueId24(g_scrClassInfo[ent->info >> 8].id, name);
    if (fieldVar == 0) {
        out->type = 0;
    } else {
        VariableValue tmp;
        Scr_RunSetup(&tmp, ent->info >> 8, SCRVAR_AUX(*ent), g_scrVar[fieldVar].u.pointerValue);
        out->u.intValue = tmp.u.intValue;
        out->type = tmp.type;
        if (out->type == 1) {
            int arrId = out->u.intValue;
            if ((g_scrVar[arrId].info & 0x1f) == SCRVAR_ARRAY && SCRVAR_RC(g_scrVar[arrId]) != 0) {
                int oldArr = out->u.intValue;
                ScrVar_Release((unsigned int)oldArr);
                out->u.intValue = (int)ScrVar_AllocArray();
                ScrVar_CopyChildren((unsigned int)oldArr, (unsigned int)out->u.intValue);
            }
        }
    }
    return out;
}

// 0x0807ce8c  size=117  FUN_0807ce8c  status=translated  naming=inferred
// id 0xfffe is the pending entity-field marker: ScrVar_ReadClassField(scrVarPub_pendingFieldObjectId, scrVarPub_pendingFieldNameId), else ScrVar_ReadVarValue
VariableValue *ScrVar_ReadVarValueOrPending(VariableValue *out, int id)
{
    VariableValue a;
    VariableValue b;
    if (id == 0xfffe) {
        ScrVar_ReadClassField(&a, scrVarPub_pendingFieldObjectId, scrVarPub_pendingFieldNameId);
        b.u.intValue = a.u.intValue;
        b.type = a.type;
    } else {
        ScrVar_ReadVarValue(&a, id);
        b.u.intValue = a.u.intValue;
        b.type = a.type;
    }
    out->u.intValue = b.u.intValue;
    out->type = b.type;
    return out;
}
