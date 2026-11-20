#include <ctype.h>
#include <time.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_027_types.h"

/* raw 32-bit view of a float (integer moves keep signalling NaNs intact) */
#define W32(x) (*(unsigned int *)&(x))

static const unsigned int HUNK_TEMP_MAGIC = 0x89537892u;
#define HUNK_MB(x) ((x) / 0x100000)

// 0x080ab702  size=27  FUN_080ab702  status=translated  naming=inferred
// naming evidence: calloc(1,size) wrapper; address used as allocator callback for Hunk_AddUserEntry
void *Com_AllocZeroed(size_t size)
{
    return calloc(1, size);
}

// 0x080ab71e  size=34  FUN_080ab71e  status=translated  naming=inferred
// naming evidence: memset(p,0,n) wrapper
void Com_ZeroMemory(void *dest, size_t count)
{
    memset(dest, 0, count);
}

// 0x080ab740  size=52  FUN_080ab740  status=translated  naming=inferred
// naming evidence: strlen+alloc+strcpy: Quake III CopyString pattern; many callers
char *CopyString(const char *src)
{
    int len = ScrStr_Strlen(src);
    char *dest = (char *)Com_MallocZeroed(len + 1);
    strcpy(dest, src);
    return dest;
}

// 0x080ab774  size=117  FUN_080ab774  status=translated  naming=inferred
// naming evidence: reuses *dest if strlen(*dest)>=len else frees and reallocates, then strcpy
void Com_CopyStringInto(char **dest, const char *src)
{
    unsigned int len = (unsigned int)ScrStr_Strlen(src);
    char *buf = *dest;
    if (buf != 0) {
        if (strlen(buf) < len) {
            Com_Free_A(buf);
            buf = 0;
        }
    }
    if (buf == 0) {
        buf = (char *)Com_MallocZeroed(len + 1);
        *dest = buf;
    }
    strcpy(buf, src);
}

// 0x080ab980  size=237  FUN_080ab980  status=translated  naming=observed
// naming evidence: strings com_hunkMegs, "Minimum com_hunkMegs for a dedicated server", universal/com_memory.cpp, meminfo
void Com_InitHunkMemory(void)
{
    dvar_t *hunkMegs;

    if (FS_GetLoadStackCount() != 0) {
        Com_Error(0, "\025Hunk initialization failed. File system load stack not zero");
    }
    hunkMegs = Dvar_RegisterInt("com_hunkMegs", 0xa0, 1, 0x200, 0x1021);
    if (hunkMegs->current.integer < 0x50) {
        Com_Printf("Minimum com_hunkMegs for a dedicated server is %i, allocating %i megs.\n", 0x50, 0x50);
        hunk_totalBytes = 0x5000000;
    } else {
        hunk_totalBytes = hunkMegs->current.integer << 20;
    }
    hunk_base = (unsigned char *)calloc(1, hunk_totalBytes);
    if (hunk_base == 0) {
        Sys_OutOfMemory("universal/com_memory.cpp", 0x365);
    }
    hunk_rawPtr = hunk_base;
    Hunk_Clear();
    Cmd_AddCommand("meminfo", Com_Meminfo_f);
}

// 0x080aba6e  size=107  FUN_080aba6e  status=translated  naming=inferred
// naming evidence: frees calloc block (DAT_085a9818) and zeroes hunk state; counterpart of Com_InitHunkMemory
void Hunk_Shutdown(void)
{
    free(hunk_rawPtr);
    hunk_base = 0;
    hunk_rawPtr = 0;
    hunk_totalBytes = 0;
    memset(&hunk_lowPermanent, 0, 8);
    memset(&hunk_highPermanent, 0, 8);
}

// 0x080abada  size=99  FUN_080abada  status=translated  naming=inferred
// naming evidence: bucket walk comparing type byte and strcasecmp name; returns entry value
int Hunk_FindUserEntry(unsigned int type, const char *name, int bucket)
{
    struct hunk_user_t *e = hunk_userEntryTable[bucket];
    while (1) {
        if (e == 0) {
            return 0;
        }
        if (e->type == type && strcasecmp(e->name, name) == 0) {
            break;
        }
        e = e->next;
    }
    return e->value;
}

// 0x080abb3e  size=55  FUN_080abb3e  status=translated  naming=inferred
// naming evidence: hashes name with FS_HashFileName(name,0x400) then Hunk_FindUserEntry
int Hunk_FindUserEntryByName(int type, const char *name)
{
    int bucket = FS_HashFileName(name, 0x400);
    return Hunk_FindUserEntry((unsigned int)type, name, bucket);
}

// 0x080abb76  size=63  FUN_080abb76  status=translated  naming=inferred
// naming evidence: unsigned range test against hunk base/total
int Hunk_IsInHeap(unsigned int addr)
{
    int result;
    if (addr < (unsigned int)hunk_base) {
        result = 0;
    } else if (addr < (unsigned int)(hunk_totalBytes + (int)hunk_base)) {
        result = 1;
    } else {
        result = 0;
    }
    return result;
}

// 0x080abbb6  size=129  FUN_080abbb6  status=translated  naming=inferred
// naming evidence: allocates strlen+10 via callback, links at bucket head; name at +9
char *Hunk_AddUserEntry(unsigned char type, const char *name, int value, void *allocFnPtr)
{
    hunk_alloc_fn_t allocFn = (hunk_alloc_fn_t)allocFnPtr;
    int bucket = FS_HashFileName(name, 0x400);
    int len = ScrStr_Strlen(name);
    struct hunk_user_t *e = (struct hunk_user_t *)allocFn(len + 10);
    e->value = value;
    e->type = type;
    strcpy(e->name, name);
    e->next = hunk_userEntryTable[bucket];
    hunk_userEntryTable[bucket] = e;
    return e->name;
}

// 0x080abc38  size=60  FUN_080abc38  status=translated  naming=inferred
// naming evidence: allocates 9-byte entry and links into anonymous list hunk_userEntryTable[0x400]
void Hunk_AddAnonymousUserEntry(unsigned char type, int value, void *allocFnPtr)
{
    hunk_alloc_fn_t allocFn = (hunk_alloc_fn_t)allocFnPtr;
    struct hunk_user_t *e = (struct hunk_user_t *)allocFn(9);
    e->value = value;
    e->type = type;
    e->next = hunk_userEntryTable[0x400];
    hunk_userEntryTable[0x400] = e;
}

// 0x080abc74  size=111  FUN_080abc74  status=translated  naming=inferred
// naming evidence: finds entry by type+name and overwrites value
void Hunk_SetUserEntryValue(unsigned int type, const char *name, int value)
{
    int bucket = FS_HashFileName(name, 0x400);
    struct hunk_user_t *e = hunk_userEntryTable[bucket];
    while (1) {
        if (e == 0) {
            return;
        }
        if (e->type == type && strcasecmp(e->name, name) == 0) {
            break;
        }
        e = e->next;
    }
    e->value = value;
}

// 0x080abce4  size=183  FUN_080abce4  status=translated  naming=inferred
// naming evidence: unlinks entries whose address lies in [lo,hi) and releases by type 3/4/5/6
void Hunk_ReleaseEntriesInRange(struct hunk_user_t **link, struct hunk_user_t *lo, struct hunk_user_t *hi)
{
    while (*link != 0) {
        struct hunk_user_t *e = *link;
        if (e < lo || hi <= e) {
            link = &e->next;
        } else {
            unsigned char type;
            int value;
            *link = e->next;
            value = e->value;
            type = e->type;
            if (type == 4) {
                XModel_ReleaseSurfNames((struct XModel *)value);
            } else if (type < 5) {
                if (type == 3) {
                    XModelParts_ReleaseBoneNames((struct XModelParts *)value);
                }
            } else if (type == 5) {
                XAnim_ReleaseHandleTable((struct XAnimParts *)value);
            } else if (type == 6) {
                XAnim_FreeNodeTableNames((struct XAnimNodeTable *)value);
            }
        }
    }
}

// 0x080abd9c  size=138  FUN_080abd9c  status=translated  naming=inferred
// naming evidence: range [low.permanent+base, total+base-high.permanent) swept over all 0x400 buckets plus anonymous list
void Hunk_ReleaseUserEntries(void)
{
    struct hunk_user_t *lo = (struct hunk_user_t *)(hunk_lowPermanent + (int)hunk_base);
    struct hunk_user_t *hi = (struct hunk_user_t *)((hunk_totalBytes + (int)hunk_base) - hunk_highPermanent);
    unsigned int i;
    for (i = 0; i < 0x400; i++) {
        Hunk_ReleaseEntriesInRange(&hunk_userEntryTable[i], lo, hi);
    }
    Hunk_ReleaseEntriesInRange(&hunk_userEntryTable[0x400], lo, hi);
}

// 0x080abe26  size=94  FUN_080abe26  status=translated  naming=inferred
// naming evidence: iterates a chain, calls callback for type 4 entries
void Hunk_ForEachEntryOfType(struct hunk_user_t *list, unsigned int type, void *cbPtr, int arg)
{
    hunk_entry_cb_t cb = (hunk_entry_cb_t)cbPtr;
    for (; list != 0; list = list->next) {
        if (list->type == type && list->type == 4) {
            cb(list->value, arg);
        }
    }
}

// 0x080abe84  size=101  FUN_080abe84  status=translated  naming=inferred
// naming evidence: mode 1 only: iterate all buckets for type 4 entries
void Hunk_ForEachType4Entry(int mode, void *cb, int arg)
{
    unsigned int i;
    if (mode == 1) {
        for (i = 0; i < 0x400; i++) {
            Hunk_ForEachEntryOfType(hunk_userEntryTable[i], 4, cb, arg);
        }
    }
}

// 0x080abeea  size=10  FUN_080abeea  status=translated  naming=inferred
// naming evidence: returns hunk high.permanent hunk_highPermanent
int Hunk_GetHighPermanent(void)
{
    return hunk_highPermanent;
}

// 0x080abef4  size=26  FUN_080abef4  status=translated  naming=inferred
// naming evidence: sets high.temp/high.permanent then releases user entries
void Hunk_SetHighMark(int mark)
{
    hunk_highTemp = mark;
    hunk_highPermanent = mark;
    Hunk_ReleaseUserEntries();
}

// 0x080abf0e  size=10  FUN_080abf0e  status=translated  naming=inferred
// naming evidence: returns hunk low.permanent hunk_lowPermanent
int Hunk_GetLowPermanent(void)
{
    return hunk_lowPermanent;
}

// 0x080abf18  size=26  FUN_080abf18  status=translated  naming=inferred
// naming evidence: sets low.temp/low.permanent then releases user entries
void Hunk_SetLowMark(int mark)
{
    hunk_lowTemp = mark;
    hunk_lowPermanent = mark;
    Hunk_ReleaseUserEntries();
}

// 0x080abf32  size=53  FUN_080abf32  status=translated  naming=inferred
// naming evidence: zeroes four hunk counters then releases user entries; called from Com_InitHunkMemory
void Hunk_Clear(void)
{
    hunk_lowPermanent = 0;
    hunk_lowTemp = 0;
    hunk_highPermanent = 0;
    hunk_highTemp = 0;
    Hunk_ReleaseUserEntries();
}

// 0x080abf68  size=16  FUN_080abf68  status=translated  naming=inferred
// naming evidence: low.permanent + high.permanent
int Hunk_GetUsedBytes(void)
{
    return hunk_highPermanent + hunk_lowPermanent;
}

// 0x080abf78  size=27  FUN_080abf78  status=translated  naming=inferred
// naming evidence: forwards to Hunk_AllocAlign(size,0x20)
void *Hunk_Alloc(int size)
{
    return Hunk_AllocAlign(size, 0x20);
}

// 0x080abf94  size=243  FUN_080abf94  status=translated  naming=observed
// naming evidence: string "Hunk_AllocAlign failed on %i bytes (total %i MB, low %i MB, high %i MB)" at DAT_0814d440
void *Hunk_AllocAlign(size_t size, int alignment)
{
    void *buf;

    hunk_highPermanent = hunk_highPermanent + size;
    hunk_highPermanent = ~(alignment - 1U) & (hunk_highPermanent + (alignment - 1U));
    buf = (void *)((hunk_totalBytes + (int)hunk_base) - hunk_highPermanent);
    hunk_highTemp = hunk_highPermanent;
    if (hunk_totalBytes < (int)(hunk_highPermanent + hunk_lowTemp)) {
        Com_Error(1, "\025Hunk_AllocAlign failed on %i bytes (total %i MB, low %i MB, high %i MB)",
                    size, HUNK_MB(hunk_totalBytes), HUNK_MB(hunk_lowTemp), HUNK_MB(hunk_highPermanent));
    }
    memset(buf, 0, size);
    return buf;
}

// 0x080ac088  size=195  FUN_080ac088  status=translated  naming=observed
// naming evidence: string "Hunk_AllocateTempMemoryHigh: failed on %i bytes" at DAT_0814d4a0
void *Hunk_AllocateTempMemoryHigh(int size)
{
    hunk_highTemp = hunk_highTemp + size;
    hunk_highTemp = (hunk_highTemp + 0xf) & 0xfffffff0;
    if (hunk_totalBytes < (int)(hunk_highTemp + hunk_lowTemp)) {
        Com_Error(1, "\025Hunk_AllocateTempMemoryHigh: failed on %i bytes (total %i MB, low %i MB, high %i MB)",
                    size, HUNK_MB(hunk_totalBytes), HUNK_MB(hunk_lowTemp), HUNK_MB(hunk_highTemp));
    }
    return (void *)((hunk_totalBytes + (int)hunk_base) - hunk_highTemp);
}

// 0x080ac14c  size=15  FUN_080ac14c  status=translated  naming=inferred
// naming evidence: high.temp = high.permanent
void Hunk_ClearTempMemoryHigh(void)
{
    hunk_highTemp = hunk_highPermanent;
}

// 0x080ac15c  size=27  FUN_080ac15c  status=translated  naming=inferred
// naming evidence: forwards to Hunk_AllocLowAlign(size,0x20)
void *Hunk_AllocLow(int size)
{
    return Hunk_AllocLowAlign(size, 0x20);
}

// 0x080ac178  size=237  FUN_080ac178  status=translated  naming=observed
// naming evidence: string "Hunk_AllocLowAlign failed on %i bytes" at DAT_0814d500
void *Hunk_AllocLowAlign(size_t size, int alignment)
{
    unsigned int aligned = ~(alignment - 1U) & (hunk_lowPermanent + (alignment - 1U));
    void *buf = (void *)(aligned + (unsigned int)hunk_base);

    hunk_lowPermanent = aligned + size;
    hunk_lowTemp = hunk_lowPermanent;
    if (hunk_totalBytes < hunk_highTemp + hunk_lowPermanent) {
        Com_Error(1, "\025Hunk_AllocLowAlign failed on %i bytes (total %i MB, low %i MB, high %i MB)",
                    size, HUNK_MB(hunk_totalBytes), HUNK_MB(hunk_lowPermanent), HUNK_MB(hunk_highTemp));
    }
    memset(buf, 0, size);
    return buf;
}

// 0x080ac266  size=15  FUN_080ac266  status=translated  naming=inferred
// naming evidence: low.permanent = low.temp
void Hunk_CommitLowTemp(void)
{
    hunk_lowPermanent = hunk_lowTemp;
}

// 0x080ac276  size=300  FUN_080ac276  status=translated  naming=observed
// naming evidence: string "Hunk_AllocateTempMemory: failed on %i bytes ... needs %i more hunk bytes" at DAT_0814d560; writes magic 0x89537892 header
int *Hunk_AllocateTempMemory(int size)
{
    int oldLowTemp = hunk_lowTemp;
    int *result;

    if (hunk_base == 0) {
        result = (int *)Com_MallocZeroed(size);
    } else {
        unsigned int aligned = (hunk_lowTemp + 0xfU) & 0xfffffff0;
        int *header = (int *)(aligned + (unsigned int)hunk_base);
        hunk_lowTemp = aligned + size + 0x10;
        if (hunk_totalBytes < hunk_highTemp + hunk_lowTemp) {
            Com_Error(1, "\025Hunk_AllocateTempMemory: failed on %i bytes (total %i MB, low %i MB, high %i MB), needs %i more hunk bytes",
                        size + 0x10, HUNK_MB(hunk_totalBytes), HUNK_MB(hunk_lowTemp), HUNK_MB(hunk_highTemp),
                        (hunk_highTemp + hunk_lowTemp) - hunk_totalBytes);
        }
        result = header + 4;
        header[0] = (int)HUNK_TEMP_MAGIC;
        header[1] = hunk_lowTemp - oldLowTemp;
    }
    return result;
}

// 0x080ac3a2  size=189  FUN_080ac3a2  status=translated  naming=observed
// naming evidence: string "Hunk_ReallocateTempMemory: failed on %i bytes" at DAT_0814d5e0; body is a 32-aligned low.temp allocation (no header, no copy), name kept from the string
int Hunk_ReallocateTempMemory(int size)
{
    unsigned int aligned = (hunk_lowPermanent + 0x1fU) & 0xffffffe0;
    int result = aligned + (int)hunk_base;

    hunk_lowTemp = aligned + size;
    if (hunk_totalBytes < hunk_highTemp + hunk_lowTemp) {
        Com_Error(1, "\025Hunk_ReallocateTempMemory: failed on %i bytes (total %i MB, low %i MB, high %i MB)",
                    size, HUNK_MB(hunk_totalBytes), HUNK_MB(hunk_lowTemp), HUNK_MB(hunk_highTemp));
    }
    return result;
}

// 0x080ac460  size=91  FUN_080ac460  status=translated  naming=observed
// naming evidence: string "Hunk_FreeTempMemory: bad magic" at DAT_0814d640; checks header magic 0x89537892
void Hunk_FreeTempMemory(void *p)
{
    int *ptr = (int *)p;
    if (hunk_base == 0) {
        Com_Free_A(ptr);
    } else {
        if (ptr[-4] != -0x76ac876e) {
            Com_Error(0, "\025Hunk_FreeTempMemory: bad magic");
        }
        ptr[-4] = -0x76ac876d;
        hunk_lowTemp = hunk_lowTemp - ptr[-3];
    }
}

// 0x080ac4bc  size=24  FUN_080ac4bc  status=translated  naming=inferred
// naming evidence: low.temp = low.permanent when hunk exists
void Hunk_ClearTempMemory(void)
{
    if (hunk_base != 0) {
        hunk_lowTemp = hunk_lowPermanent;
    }
}

// 0x080ac4d4  size=29  FUN_080ac4d4  status=translated  naming=inferred
// naming evidence: returns old low.permanent and sets it to low.temp; paired with Hunk_SetLowPermanent in Com_LoadSoundAliases
int Hunk_PushLowPermanent(void)
{
    int old = hunk_lowPermanent;
    hunk_lowPermanent = hunk_lowTemp;
    return old;
}

// 0x080ac4f2  size=13  FUN_080ac4f2  status=translated  naming=inferred
// naming evidence: sets low.permanent
void Hunk_SetLowPermanent(int value)
{
    hunk_lowPermanent = value;
}

// 0x080ac500  size=29  FUN_080ac500  status=translated  naming=inferred
// naming evidence: returns old high.permanent and sets it to high.temp
int Hunk_PushHighPermanent(void)
{
    int old = hunk_highPermanent;
    hunk_highPermanent = hunk_highTemp;
    return old;
}

// 0x080ac51e  size=13  FUN_080ac51e  status=translated  naming=inferred
// naming evidence: sets high.permanent
void Hunk_SetHighPermanent(int value)
{
    hunk_highPermanent = value;
}

// 0x080ac52c  size=40  FUN_080ac52c  status=translated  naming=unknown
// naming evidence: bump allocator on com_poolOffset rounding sizes to 4; returns offset
int Com_PoolAlloc(int size)
{
    int offset = com_poolOffset;
    com_poolOffset = com_poolOffset + ((size + 3U) & 0xfffffffc);
    return offset;
}

// 0x080ac554  size=13  FUN_080ac554  status=translated  naming=unknown
// naming evidence: sets com_poolOffset
void Com_PoolSetOffset(int offset)
{
    com_poolOffset = offset;
}

// 0x080ac562  size=13  FUN_080ac562  status=translated  naming=unknown
// naming evidence: returns com_poolBase+offset
unsigned char *Com_PoolAt(int offset)
{
    return com_poolBase + offset;
}

// 0x080ac570  size=29  FUN_080ac570  status=translated  naming=unknown
// naming evidence: *out = Com_PoolAlloc(size); byte-identical twin of 0x80ac58e
void Com_PoolAllocInto(void *outOffset, int size)
{
    *(int *)outOffset = Com_PoolAlloc(size);
}

// 0x080ac58e  size=29  FUN_080ac58e  status=translated  naming=unknown
// naming evidence: *out = Com_PoolAlloc(size); byte-identical twin of 0x80ac570
void Com_PoolAllocInto2(void *outOffset, int size)
{
    *(int *)outOffset = Com_PoolAlloc(size);
}

// 0x080ac5ac  size=21  FUN_080ac5ac  status=translated  naming=unknown
// naming evidence: Com_PoolSetOffset(*p); twin of 0x80ac5c2
void Com_PoolRestoreFrom(void *savedOffset)
{
    Com_PoolSetOffset(*(int *)savedOffset);
}

// 0x080ac5c2  size=21  FUN_080ac5c2  status=translated  naming=unknown
// naming evidence: Com_PoolSetOffset(*p); twin of 0x80ac5ac
void Com_PoolRestoreFrom2(void *savedOffset)
{
    Com_PoolSetOffset(*(int *)savedOffset);
}

// 0x080ac5d8  size=21  FUN_080ac5d8  status=translated  naming=unknown
// naming evidence: calls Com_PoolAt(*p) and discards result
void *Com_PoolPtrFromOffset(void *offset)
{
    return Com_PoolAt(*(int *)offset);
}

// 0x080ac5ee  size=15  FUN_080ac5ee  status=translated  naming=unknown
// naming evidence: com_poolOffset = 0
void Com_PoolReset(void)
{
    com_poolOffset = 0;
}

// 0x080ac600  size=111  FUN_080ac600  status=translated  naming=inferred
// naming evidence: scans str and its suffixes for one where cmp(suffix,pattern)==0; helper of Com_Filter
char *Com_FindTailMatch(char *str, const char *pattern, void *cmpPtr)
{
    com_strcmp_fn_t cmp = (com_strcmp_fn_t)cmpPtr;
    int strLen = ScrStr_Strlen(str);
    int patLen = ScrStr_Strlen(pattern);
    int i = 0;
    while (1) {
        if (strLen - patLen < i) {
            return 0;
        }
        if (cmp(str, pattern) == 0) {
            break;
        }
        i = i + 1;
        str = str + 1;
    }
    return str;
}

// 0x080ac670  size=831  FUN_080ac670  status=translated  naming=inferred
// naming evidence: Quake III Com_Filter structure: *, ?, [a-z] class matching, case-insensitive via toupper; callers Q_stricmp/Q_strcmp as stricmp/strcmp
int Com_Filter(const char *filter, const char *name, int caseSensitive)
{
    char literal[1028];
    int literalLen;
    int matched;
    com_strcmp_fn_t cmp;
    char *found;

    while (1) {
        while (1) {
            while (1) {
                while (1) {
                    while (1) {
                        if (*filter == '\0') {
                            return 1;
                        }
                        if (*filter != '*') {
                            break;
                        }
                        literalLen = 0;
                        while (filter = filter + 1, *filter != '\0' && *filter != '*' && *filter != '?') {
                            literal[literalLen] = *filter;
                            literalLen = literalLen + 1;
                        }
                        literal[literalLen] = '\0';
                        if (literal[0] != '\0') {
                            if (caseSensitive == 0) {
                                cmp = Q_stricmp;
                            } else {
                                cmp = Q_strcmp;
                            }
                            found = Com_FindTailMatch((char *)name, literal, (void *)cmp);
                            if (found == 0) {
                                return 0;
                            }
                            name = found + strlen(literal);
                        }
                    }
                    if (*filter != '?') {
                        break;
                    }
                    filter = filter + 1;
                    name = name + 1;
                }
                if (*filter != '[' || filter[1] != '[') {
                    break;
                }
                filter = filter + 1;
            }
            if (*filter == '[') {
                break;
            }
            if (caseSensitive == 0) {
                if (toupper((int)*filter) != toupper((int)*name)) {
                    return 0;
                }
            } else if (*filter != *name) {
                return 0;
            }
            filter = filter + 1;
            name = name + 1;
        }
        filter = filter + 1;
        matched = 0;
        while (*filter != '\0' && !matched && (*filter != ']' || filter[1] == ']')) {
            if (filter[1] == '-' && filter[2] != '\0' && (filter[2] != ']' || filter[3] == ']')) {
                if (caseSensitive == 0) {
                    if (toupper((int)*filter) <= toupper((int)*name)) {
                        if (toupper((int)*name) <= toupper((int)filter[2])) {
                            matched = 1;
                        }
                    }
                } else if (*filter <= *name && *name <= filter[2]) {
                    matched = 1;
                }
                filter = filter + 3;
            } else {
                if (caseSensitive == 0) {
                    if (toupper((int)*filter) == toupper((int)*name)) {
                        matched = 1;
                    }
                } else if (*filter == *name) {
                    matched = 1;
                }
                filter = filter + 1;
            }
        }
        if (!matched) {
            break;
        }
        for (; *filter != '\0' && (*filter != ']' || filter[1] == ']'); filter = filter + 1) {
        }
        filter = filter + 1;
        name = name + 1;
    }
    return 0;
}

// 0x080ac9b0  size=243  FUN_080ac9b0  status=translated  naming=inferred
// naming evidence: Quake III Com_FilterPath: normalises \\ and : to / (63 chars max) then Com_Filter
unsigned char Com_FilterPath(const char *filter, const char *name, int caseSensitive)
{
    char nameBuf[64];
    char filterBuf[76];
    int i;

    for (i = 0; i < 0x3f && filter[i] != '\0'; i++) {
        if (filter[i] == '\\' || filter[i] == ':') {
            filterBuf[i] = '/';
        } else {
            filterBuf[i] = filter[i];
        }
    }
    filterBuf[i] = 0;
    for (i = 0; i < 0x3f && name[i] != '\0'; i++) {
        if (name[i] == '\\' || name[i] == ':') {
            nameBuf[i] = '/';
        } else {
            nameBuf[i] = name[i];
        }
    }
    nameBuf[i] = 0;
    return (unsigned char)Com_Filter(filterBuf, nameBuf, caseSensitive);
}

// 0x080acaa4  size=96  FUN_080acaa4  status=translated  naming=inferred
// naming evidence: Quake III Com_HashKey formula (i+119)*c, h ^ h>>10 ^ h>>20
unsigned int Com_HashKey(const char *str, int maxLen)
{
    int i;
    unsigned int hash = 0;
    for (i = 0; i < maxLen && str[i] != '\0'; i++) {
        hash = hash + (i + 0x77) * (int)str[i];
    }
    return hash ^ ((int)hash >> 10) ^ ((int)hash >> 0x14);
}

// 0x080acb04  size=175  FUN_080acb04  status=translated  naming=inferred
// naming evidence: Quake III Com_RealTime: time()+localtime() into 9 ints
time_t Com_RealTime(struct com_qtime_t *qtime)
{
    time_t now;
    time_t copy;
    struct tm *t;

    now = time((time_t *)0);
    if (qtime != 0) {
        copy = now;
        t = localtime(&copy);
        if (t != 0) {
            qtime->tm_sec = t->tm_sec;
            qtime->tm_min = t->tm_min;
            qtime->tm_hour = t->tm_hour;
            qtime->tm_mday = t->tm_mday;
            qtime->tm_mon = t->tm_mon;
            qtime->tm_year = t->tm_year;
            qtime->tm_wday = t->tm_wday;
            qtime->tm_yday = t->tm_yday;
            qtime->tm_isdst = t->tm_isdst;
        }
        now = copy;
    }
    return now;
}

// 0x080acbb4  size=33  FUN_080acbb4  status=translated  naming=inferred
// naming evidence: memcpy wrapper
void Com_Memcpy(void *dest, const void *src, size_t count)
{
    memcpy(dest, src, count);
}

// 0x080acbd6  size=33  FUN_080acbd6  status=translated  naming=inferred
// naming evidence: memset wrapper
void Com_Memset(void *dest, int value, size_t count)
{
    memset(dest, value, count);
}

// 0x080acbf8  size=74  FUN_080acbf8  status=translated  naming=inferred
// naming evidence: tolower hash multiplier 0x7a69 masked to 0x3ff indexes g_soundAliasHash; used by SND_FindAliasList and Com_DuplicateSoundAlias
unsigned int SND_HashAliasName(const char *name)
{
    unsigned int hash = 0;
    for (; *name != '\0'; name = name + 1) {
        int c = tolower((int)*name);
        hash = c + hash * 0x7a69;
    }
    return hash & 0x3ff;
}

// 0x080acc42  size=111  FUN_080acc42  status=translated  naming=inferred
// naming evidence: bucket walk comparing with Q_stricmp (stricmp) against list->aliasName
struct snd_alias_list_t *SND_FindAliasList(const char *name)
{
    struct snd_alias_list_t *e;
    if (name != 0) {
        int bucket = SND_HashAliasName(name);
        for (e = g_soundAliasHash[bucket]; e != 0; e = e->next) {
            if (Q_stricmp(name, e->aliasName) == 0) {
                return e;
            }
        }
    }
    return 0;
}

// 0x080acd9e  size=126  FUN_080acd9e  status=translated  naming=inferred
// naming evidence: inserts list at bucket head if name absent, returns 1; 0 if present
int SND_InsertAliasList(const char *name, struct snd_alias_list_t *list)
{
    int bucket = SND_HashAliasName(name);
    struct snd_alias_list_t *e = g_soundAliasHash[bucket];
    while (1) {
        if (e == 0) {
            list->next = g_soundAliasHash[bucket];
            g_soundAliasHash[bucket] = list;
            return 1;
        }
        if (Q_stricmp(name, e->aliasName) == 0) {
            break;
        }
        e = e->next;
    }
    return 0;
}

// 0x080ace1c  size=282  FUN_080ace1c  status=translated  naming=observed
// naming evidence: allocation tag string "Com_DuplicateSoundAlias" passed to hunk allocator
void Com_DuplicateSoundAlias(struct snd_alias_list_t *src, const char *name)
{
    int bucket = SND_HashAliasName(name);
    struct snd_alias_list_t *e = g_soundAliasHash[bucket];
    while (1) {
        if (e == 0) {
            struct snd_alias_list_t *copy = (struct snd_alias_list_t *)Com_HunkAllocTagged(0x10, "Com_DuplicateSoundAlias", 0xd);
            char *nameCopy;
            copy->aliasName = src->aliasName;
            copy->head = src->head;
            copy->count = src->count;
            copy->next = src->next;
            nameCopy = (char *)Com_HunkAllocTagged(strlen(name) + 1, "Com_DuplicateSoundAlias", 0xd);
            strcpy(nameCopy, name);
            copy->aliasName = nameCopy;
            copy->next = g_soundAliasHash[bucket];
            g_soundAliasHash[bucket] = copy;
            return;
        }
        if (Q_stricmp(name, e->aliasName) == 0) {
            break;
        }
        e = e->next;
    }
    {
        const char *savedName = e->aliasName;
        e->aliasName = src->aliasName;
        e->head = src->head;
        e->count = src->count;
        e->next = src->next;
        e->aliasName = savedName;
    }
}

// 0x080acf36  size=13  FUN_080acf36  status=translated  naming=inferred
// naming evidence: returns **(holder+0xc) i.e. alias list name
const char *SND_GetAliasListName(struct snd_alias_holder_t *holder)
{
    return holder->aliasList->aliasName;
}

// 0x080acf44  size=14  FUN_080acf44  status=translated  naming=inferred
// naming evidence: returns (*(holder+0xc))->head
struct snd_alias_t *SND_GetAliasListHead(struct snd_alias_holder_t *holder)
{
    return holder->aliasList->head;
}

// 0x080acf52  size=344  FUN_080acf52  status=translated  naming=observed
// naming evidence: strings "soundaliases", "vfcurve", "Snd_Alias Curve initialization: '.vfcurve' file count (%d) exceeds maximum (%d)", "Failed to load sndcurve file"
void SND_InitAliasCurves(void)
{
    int fileCount;
    int i;
    char **files;

    memset(g_soundFalloffCurves, 0, 0x480);
    SndCurve_InitDefault(&g_soundFalloffCurves[0]);
    files = FS_ListFiles("soundaliases", "vfcurve", 0, &fileCount, 10);
    if (0xf < fileCount) {
        Com_Error(1, "\025Snd_Alias Curve initialization: '.vfcurve' file count (%d) exceeds maximum (%d)", fileCount, 0xf);
    }
    for (i = 0; i < fileCount; i++) {
        size_t len = strlen(files[i]);
        char ok;
        Q_strncpyz(g_soundFalloffCurveNames[i], files[i], len - 7);
        ok = (char)Com_LoadSndCurve(g_soundFalloffCurveNames[i], &g_soundFalloffCurves[i + 1]);
        if (ok == '\0') {
            Com_Error(0, "\025Failed to load sndcurve file '%s'", files[i]);
        }
    }
    FS_FreeFileList(files, 10);
    g_soundCurvesInitialised = 1;
}

// 0x080ad0aa  size=457  FUN_080ad0aa  status=translated  naming=inferred
// naming evidence: strings "maps/mp/", "maps/", "soundaliases", "csv", "WARNING: can't find any sound alias files (soundaliases/*.csv)"
void Com_LoadSoundAliases(const char *mapName, const char *loadSpec, int bankIndex)
{
    char name[68];
    int fileCount;
    char **files;
    int i;
    int savedLow;

    if (Str_NCaseCmp(mapName, "maps/mp/", 8) == 0) {
        COM_StripExtension(mapName + 8, name);
    } else if (Str_NCaseCmp(mapName, "maps/", 5) == 0) {
        COM_StripExtension(mapName + 5, name);
    } else {
        strcpy(name, mapName);
    }
    Q_strlwr_EndPtr(name);
    if (bankIndex == 1 && com_sv_running->current.enabled != 0) {
        g_soundAliasBanks[1] = g_soundAliasBanks[2];
        g_soundAliasBankAux[1] = g_soundAliasBankAux[2];
    } else {
        files = FS_ListFiles("soundaliases", "csv", 0, &fileCount, 10);
        if (fileCount == 0) {
            Com_Printf("WARNING: can't find any sound alias files (soundaliases/*.csv)\n");
            return;
        }
        savedLow = Hunk_PushLowPermanent();
        Com_ClearBuildSoundAliases();
        for (i = 0; i < fileCount; i++) {
            Com_LoadSoundAliasFile(name, (uint32)loadSpec, files[i]);
        }
        Com_MakeSoundAliasesPermanent(&g_soundAliasBanks[bankIndex], (struct snd_file_set_s *)&g_soundAliasBankAux[bankIndex]);
        Hunk_ClearTempMemory();
        Hunk_SetLowPermanent(savedLow);
        FS_FreeFileList(files, 10);
    }
    g_soundAliasBankLoaded[bankIndex] = 1;
}

// 0x080ad274  size=109  FUN_080ad274  status=translated  naming=inferred
// naming evidence: clears bank flag/words and the alias hash table; counterpart of Com_LoadSoundAliases
void Com_UnloadSoundAliases(int bankIndex)
{
    if (g_soundAliasBankLoaded[bankIndex] != 0) {
        if (g_soundAliasBanks[bankIndex].word1 != 0) {
            g_soundAliasBanks[bankIndex].word1 = 0;
            g_soundAliasBanks[bankIndex].word2 = 0;
            memset(g_soundAliasHash, 0, 0x1000);
        }
        g_soundAliasBankLoaded[bankIndex] = 0;
    }
}

// 0x080ad312  size=10  FUN_080ad312  status=translated  naming=inferred
// naming evidence: returns g_soundAliasRandSeed LCG seed
unsigned int Com_GetRandSeed(void)
{
    return g_soundAliasRandSeed;
}

// 0x080ad31c  size=13  FUN_080ad31c  status=translated  naming=inferred
// naming evidence: sets g_soundAliasRandSeed
void Com_SetRandSeed(unsigned int seed)
{
    g_soundAliasRandSeed = seed;
}

// 0x080ad32a  size=335  FUN_080ad32a  status=translated  naming=inferred
// naming evidence: weighted random pick over snd_alias_t stride 0x44 using probability +0x34 and sequence +0x10; wrapper at unlisted 0x80ad47a looks the list up by name first
struct snd_alias_t *Com_PickSoundAlias(struct snd_alias_list_t *list)
{
    struct snd_alias_t *base;
    struct snd_alias_t *chosen;
    struct snd_alias_t *cur;
    float accum;
    int maxSequence;
    int i;

    if (list == 0) {
        return 0;
    }
    base = list->head;
    chosen = base;
    accum = base->probability;
    maxSequence = base->sequence;
    cur = base;
    i = 0;
    while (i = i + 1, i != list->count) {
        cur = cur + 1;
        accum = accum + cur->probability;
        if ((long double)(int)Com_Rand() * accum < (long double)cur->probability * -2147483648.0f) {
            chosen = cur;
        }
        if (maxSequence < cur->sequence) {
            maxSequence = cur->sequence;
        }
    }
    if (list->count > 2 && maxSequence == chosen->sequence) {
        accum = 0.0f;
        cur = base;
        for (i = 0; i < list->count; i++) {
            if (maxSequence != cur->sequence) {
                accum = accum + cur->probability;
                if ((long double)(int)Com_Rand() * accum < (long double)cur->probability * -2147483648.0f) {
                    chosen = cur;
                }
            }
            cur = cur + 1;
        }
    }
    chosen->sequence = maxSequence + 1;
    return chosen;
}

// 0x080ad49c  size=13  FUN_080ad49c  status=translated  naming=unknown
// naming evidence: ignores both arguments and returns 0 (curve lookup stub); result stored to alias +0x130 for column volumefalloffcurve
struct snd_curve_t *Com_FindFalloffCurve_Stub(const char *curveName, const char *fileName)
{
    return 0;
}

// 0x080ad4aa  size=10  FUN_080ad4aa  status=translated  naming=inferred
// naming evidence: returns &g_soundFalloffCurves[0], the default curve initialised by SndCurve_InitDefault
struct snd_curve_t *Com_GetDefaultFalloffCurve(void)
{
    return &g_soundFalloffCurves[0];
}

// 0x080ad4b4  size=19  FUN_080ad4b4  status=translated  naming=inferred
// naming evidence: Hunk_AllocateTempMemory(size) ignoring the tag argument; caller passes "Com_AddBuildSoundAlias"
void *Com_HunkAllocTempTagged(int size, const char *tag)
{
    return Hunk_AllocateTempMemory(size);
}

// 0x080ad4c8  size=19  FUN_080ad4c8  status=translated  naming=inferred
// naming evidence: Hunk_Alloc(size) ignoring tag and line arguments; callers pass "Com_DuplicateSoundAlias",0xd
void *Com_HunkAllocTagged(int size, const char *tag, int line)
{
    return Hunk_Alloc(size);
}

// 0x080ad4dc  size=152  FUN_080ad4dc  status=translated  naming=inferred
// naming evidence: Quake III Str_NCaseCmp semantics: tolower per char, signed char comparison, -1/0/1
int Str_NCaseCmp(const char *s1, const char *s2, int n)
{
    int c1;
    int c2;
    do {
        if (n == 0 || *s1 == '\0') {
            if (n == 0 || *s1 == *s2) {
                return 0;
            }
            return -1;
        }
        c1 = tolower((int)*s1);
        c2 = tolower((int)*s2);
        n = n - 1;
        s1 = s1 + 1;
        s2 = s2 + 1;
        if ((char)c1 < (char)c2) {
            return -1;
        }
    } while ((char)c1 <= (char)c2);
    return 1;
}

// 0x080ad574  size=39  FUN_080ad574  status=translated  naming=inferred
// naming evidence: MSVC-style LCG 214013*seed+2531011, (seed>>16)&0x7fff
unsigned int Com_Rand(void)
{
    g_soundAliasRandSeed = g_soundAliasRandSeed * 0x343fd + 0x269ec3;
    return (g_soundAliasRandSeed >> 0x10) & 0x7fff;
}

// 0x080ad59c  size=133  FUN_080ad59c  status=translated  naming=observed
// naming evidence: string "Sound alias file %s: Volume Mod Group '%s' not found."; searches g_volumeModGroups (32 x 0x44)
float Com_GetVolumeModGroup(const char *groupName, const char *fileName)
{
    int i;
    for (i = 0; i < 0x20; i++) {
        if (strcasecmp(groupName, g_volumeModGroups[i].name) == 0) {
            return g_volumeModGroups[i].volume;
        }
    }
    Com_Error(1, "\025Sound alias file %s: Volume Mod Group '%s' not found.", fileName, groupName);
    return 0.0f;
}

// 0x080ad622  size=383  FUN_080ad622  status=translated  naming=inferred
// naming evidence: initialises a 0x154 alias-build record with defaults; menu loadspec flag from strcmp(mapName,"menu")
void Com_InitSoundAliasBuild(struct snd_alias_build_t *alias, const char *fileName, const char *mapName)
{
    strcpy(alias->fileName, fileName);
    alias->name[0] = '\0';
    alias->secondaryName[0] = '\0';
    alias->sequence = 0;
    alias->subtitle = 0;
    alias->soundFile[0] = '\0';
    alias->volMin = 1.0f;
    alias->volMax = 1.0f;
    alias->volMod = 1.0f;
    alias->pitchMin = 1.0f;
    alias->pitchMax = 1.0f;
    alias->distMin = 120.0f;
    alias->distMax = 0.0f;
    alias->channel = 0;
    alias->type = 1;
    alias->probability = 1.0f;
    alias->lfePercentage = 0.0f;
    alias->hasError = 0;
    alias->loadSpecPass = (strcmp(mapName, "menu") != 0);
    alias->isMaster = 0;
    alias->isSlave = 0;
    alias->fullDryLevel = 0;
    alias->noWetLevel = 0;
    alias->slavePercentage = 1.0f;
    alias->startDelay = 0;
    alias->looping = 0;
    alias->volumeFalloffCurve = Com_GetDefaultFalloffCurve();
    alias->next = 0;
}

// 0x080ad7a2  size=126  FUN_080ad7a2  status=translated  naming=inferred
// naming evidence: first char >0x1f and alnum or _; rest alnum; no char < 0x20; used to validate alias names
int Com_IsValidAliasName(const char *name)
{
    char c;
    if ('\x1f' < *name) {
        if (isalnum((int)*name) != 0) {
            goto body;
        }
        c = *name;
        while (c == '_') {
        body:
            do {
                name = name + 1;
                if (*name == '\0') {
                    return 1;
                }
                if (*name < ' ') {
                    return 0;
                }
            } while (isalnum((int)*name) != 0);
            c = *name;
        }
    }
    return 0;
}

// 0x080ad820  size=284  FUN_080ad820  status=translated  naming=observed
// naming evidence: string "Unknown sound channel '%s'; should be %s" with channel table at PTR_0817e3a0 (auto, auto2d, menu, weapon, voice, item, body, local, music, announcer, shellshock)
void Com_ParseAliasChannel(const char *value, const char *fileName, struct snd_alias_build_t *alias)
{
    char list[16392];
    int len;
    int i;

    for (i = 0; ; i++) {
        if (10 < i) {
            len = 0;
            for (i = 0; i < 0xb; i++) {
                const char *chan = g_soundChannelNames[i];
                len = len + sprintf(list + len, "%s", chan);
                if (i < 9) {
                    len = len + sprintf(list + len, ", ");
                } else if (i == 9) {
                    len = len + sprintf(list + len, " or ");
                }
            }
            Com_Printf("^1ERROR: Sound alias file %s: Unknown sound channel '%s'; should be %s\n", fileName, value, list);
            alias->hasError = 1;
            return;
        }
        if (Q_stricmp(value, g_soundChannelNames[i]) == 0) {
            break;
        }
    }
    alias->channel = i;
}

// 0x080ad93c  size=158  FUN_080ad93c  status=translated  naming=observed
// naming evidence: strings streamed/primed/loaded and "Unknown sound type"
void Com_ParseAliasType(const char *value, const char *fileName, struct snd_alias_build_t *alias)
{
    if (Q_stricmp(value, "streamed") == 0) {
        alias->type = 2;
    } else if (Q_stricmp(value, "primed") == 0) {
        alias->type = 2;
    } else if (Q_stricmp(value, "loaded") == 0) {
        alias->type = 1;
    } else {
        Com_Printf("^1ERROR: Sound alias file %s: Unknown sound type '%s'; should be primed, streamed or loaded\n", fileName, value);
        alias->hasError = 1;
    }
}

// 0x080ad9da  size=114  FUN_080ad9da  status=translated  naming=observed
// naming evidence: strings looping/nonlooping and "Unknown sound looping type"
void Com_ParseAliasLooping(const char *value, const char *fileName, struct snd_alias_build_t *alias)
{
    if (Q_stricmp(value, "looping") == 0) {
        alias->looping = 1;
    } else if (Q_stricmp(value, "nonlooping") == 0) {
        alias->looping = 0;
    } else {
        Com_Printf("^1ERROR: Sound alias file %s: Unknown sound looping type '%s'; should be looping or nonlooping\n", fileName, value);
        alias->hasError = 1;
    }
}

// 0x080ada4c  size=366  FUN_080ada4c  status=translated  naming=observed
// naming evidence: string "loadspec is > %i characters"; tokenised search of mapName inside loadspec value
unsigned char Com_ParseAliasLoadSpec(const char *mapName, const char *loadSpec, const char *value, const char *fileName)
{
    char buf[0x4000];
    size_t mapLen = strlen(mapName);
    unsigned char result;
    char *searchFrom;
    char *found;

    buf[0x3fff] = '\0';
    strncpy(buf, value, 0x4000);
    if (buf[0x3fff] == '\0') {
        Q_strlwr_EndPtr(buf);
        searchFrom = buf;
        result = 0;
        if (buf[0] != '!' || strcmp(mapName, "menu") == 0) {
            result = 1;
        }
        while (found = strstr(searchFrom, mapName), found != 0) {
            if ((found == buf || found[-1] < '"') && found[mapLen] < '!') {
                return result;
            }
            searchFrom = found + 1;
        }
        if (strcmp(mapName, "menu") == 0 || strcmp(searchFrom, loadSpec) != 0) {
            result = result ^ 1;
        }
    } else {
        Com_Printf("^1ERROR: Sound alias file %s: loadspec is > %i characters\n", fileName, 0x3fff);
        result = 0;
    }
    return result;
}

// 0x080adbba  size=195  FUN_080adbba  status=translated  naming=observed
// naming evidence: string "master" and "SlavePercentage ... is not within the range"
void Com_ParseAliasMasterSlave(const char *value, const char *fileName, struct snd_alias_build_t *alias)
{
    if (strcasecmp(value, "master") == 0) {
        alias->isMaster = 1;
        alias->isSlave = 0;
    } else {
        alias->isMaster = 0;
        alias->isSlave = 1;
        alias->slavePercentage = (float)atof(value);
        if (alias->slavePercentage < 0.0f || 1.0f < alias->slavePercentage) {
            Com_Printf("^1ERROR: Sound alias file %s: SlavePercentage'%f' is not within the range of '%f'-'%f'.\n",
                        fileName, (double)alias->slavePercentage, 0.0, 1.0);
            alias->hasError = 1;
        }
    }
}

// 0x080adc7e  size=74  FUN_080adc7e  status=translated  naming=observed
// naming evidence: strings fulldrylevel and nowetlevel
void Com_ParseAliasReverb(const char *value, struct snd_alias_build_t *alias)
{
    if (strstr(value, "fulldrylevel") != 0) {
        alias->fullDryLevel = 1;
    }
    if (strstr(value, "nowetlevel") != 0) {
        alias->noWetLevel = 1;
    }
}

// 0x080adcc8  size=1466  FUN_080adcc8  status=translated  naming=observed
// naming evidence: csv column switch with error strings "Duplicate entries for the '%s' column"; column names table at 0x817e340
void Com_ParseAliasColumn(const char *mapName, const char *loadSpec, const char *fileName, const char *value,
                          int column, unsigned char *seen, struct snd_alias_build_t *alias)
{
    size_t i;

    if (column == 0) {
        return;
    }
    if (seen[column] != 0) {
        Com_Printf("^1ERROR: Sound alias file %s: Duplicate entries for the '%s' column\n", fileName, g_sndAliasColumnNames[column]);
        alias->hasError = 1;
        return;
    }
    seen[column] = 1;
    switch (column) {
    case SNDCOL_NAME:
        if (strlen(value) < 0x3f) {
            if (Com_IsValidAliasName(value) == 0) {
                Com_Printf("^1ERROR: Sound alias file %s: Alias name '%s' is invalid\n", fileName, value);
                alias->hasError = 1;
            } else {
                strcpy(alias->name, value);
            }
        } else {
            Com_Printf("^1ERROR: Sound alias file %s: Alias name '%s' is longer than %i characters\n", fileName, value, 0x3f);
            alias->hasError = 1;
        }
        break;
    case SNDCOL_SEQUENCE:
        alias->sequence = atoi(value);
        break;
    case SNDCOL_FILE:
        if (strlen(value) < 0x3f) {
            strcpy(alias->soundFile, value);
        } else {
            Com_Printf("^1ERROR: Sound alias file %s: Sound file '%s' is longer than %i characters\n", fileName, value, 0x3f);
            alias->hasError = 1;
        }
        break;
    case SNDCOL_SUBTITLE:
        for (i = 0; value[i] != '\0'; i++) {
            if (value[i] < '\0') {
                Com_Printf("^1ERROR: Sound alias file %s: Subtitle '%s' has invalid character '%c' ascii %i\n",
                            fileName, value, (int)value[i], value[i]);
                alias->hasError = 1;
                return;
            }
        }
        alias->subtitle = (char *)Hunk_AllocateTempMemory(i + 1);
        memcpy(alias->subtitle, value, i);
        alias->subtitle[i] = '\0';
        break;
    case SNDCOL_VOL_MIN:
        alias->volMin = (float)atof(value);
        if (alias->volMin < 0.0f || 1.0f < alias->volMin) {
            Com_Printf("^1ERROR: Sound alias file %s: MinVolume '%f' is not within the range of '%f'-'%f'.\n",
                        fileName, (double)alias->volMin, 0.0, 1.0);
            alias->hasError = 1;
        } else if (seen[SNDCOL_VOL_MAX] == 0) {
            alias->volMax = alias->volMin;
        }
        break;
    case SNDCOL_VOL_MAX:
        alias->volMax = (float)atof(value);
        if (alias->volMax < 0.0f || 1.0f < alias->volMax) {
            Com_Printf("^1ERROR: Sound alias file %s: MaxVolume '%f' is not within the range of '%f'-'%f'.\n",
                        fileName, (double)alias->volMax, 0.0, 1.0);
            alias->hasError = 1;
        }
        break;
    case SNDCOL_VOL_MOD:
        alias->volMod = Com_GetVolumeModGroup(value, fileName);
        break;
    case SNDCOL_PITCH_MIN:
        alias->pitchMin = (float)atof(value);
        if (seen[SNDCOL_PITCH_MAX] == 0) {
            alias->pitchMax = alias->pitchMin;
        }
        break;
    case SNDCOL_PITCH_MAX:
        alias->pitchMax = (float)atof(value);
        break;
    case SNDCOL_DIST_MIN:
        alias->distMin = (float)atof(value);
        break;
    case SNDCOL_DIST_MAX:
        alias->distMax = (float)atof(value);
        break;
    case SNDCOL_CHANNEL:
        Com_ParseAliasChannel(value, fileName, alias);
        break;
    case SNDCOL_TYPE:
        Com_ParseAliasType(value, fileName, alias);
        break;
    case SNDCOL_LOOP:
        Com_ParseAliasLooping(value, fileName, alias);
        break;
    case SNDCOL_PROBABILITY:
        alias->probability = (float)atof(value);
        break;
    case SNDCOL_LOADSPEC:
        alias->loadSpecPass = Com_ParseAliasLoadSpec(mapName, loadSpec, value, fileName);
        break;
    case SNDCOL_MASTERSLAVE:
        Com_ParseAliasMasterSlave(value, fileName, alias);
        break;
    case SNDCOL_SECONDARY_ALIAS:
        if (strlen(value) < 0x3f) {
            if (Com_IsValidAliasName(value) == 0) {
                Com_Printf("^1ERROR: Sound alias file %s: Secondary Alias name '%s' is invalid\n", fileName, value);
                alias->hasError = 1;
            } else {
                strcpy(alias->secondaryName, value);
            }
        } else {
            Com_Printf("^1ERROR: Sound alias file %s: Secondary Alias name '%s' is longer than %i characters\n", fileName, value, 0x3f);
            alias->hasError = 1;
        }
        break;
    case SNDCOL_VOLUME_FALLOFF_CURVE:
        alias->volumeFalloffCurve = Com_FindFalloffCurve_Stub(value, fileName);
        break;
    case SNDCOL_START_DELAY:
        alias->startDelay = atoi(value);
        break;
    case SNDCOL_REVERB:
        Com_ParseAliasReverb(value, alias);
        break;
    case SNDCOL_LFE_PERCENTAGE:
        alias->lfePercentage = Float_Clamp_080b11e8((float)atof(value), 0.0f, 1.0f);
        break;
    }
}

// 0x080ae282  size=687  FUN_080ae282  status=translated  naming=observed
// naming evidence: strings "sound alias '%s' has pitch_min %g <= 0", vol_min, dist_min/dist_max
int Com_ValidateSoundAlias(struct snd_alias_build_t *alias)
{
    volatile float scaled;   /* x87: float stack slot, rounded before the clamp compares */

    if (alias->pitchMax < alias->pitchMin) {
        float tmp;   /* integer moves in the original (signalling NaNs survive the swap) */
        W32(tmp) = W32(alias->pitchMax);
        W32(alias->pitchMax) = W32(alias->pitchMin);
        W32(alias->pitchMin) = W32(tmp);
    }
    if (alias->pitchMin <= 0.0f) {
        Com_Printf("^1ERROR: sound alias '%s' has pitch_min %g <= 0\n", alias->name, (double)alias->pitchMin);
        return 0;
    }
    if (alias->volMax < alias->volMin) {
        float tmp;
        W32(tmp) = W32(alias->volMax);
        W32(alias->volMax) = W32(alias->volMin);
        W32(alias->volMin) = W32(tmp);
    }
    if (alias->volMin < 0.0f) {
        Com_Printf("^1ERROR: sound alias '%s' has vol_min %g < 0\n", alias->name, (double)alias->volMin);
        return 0;
    }
    if (alias->distMax == 0.0f) {
        alias->distMax = alias->distMin * 5.0f;
    }
    if (alias->distMax < alias->distMin) {
        Com_Printf("^1ERROR: sound alias '%s' has dist_min %g <= dist_max %g\n", alias->name,
                    (double)alias->distMin, (double)alias->distMax);
        return 0;
    }
    if (alias->distMin <= 0.0f) {
        Com_Printf("^1ERROR: sound alias '%s' has dist_min %g <= 0\n", alias->name, (double)alias->distMin);
        return 0;
    }
    if (alias->volMod != 1.0f) {
        scaled = alias->volMin * alias->volMod;
        if (scaled < 0.0f) {
            scaled = 0.0f;
        } else if (1.0f < scaled) {
            scaled = 1.0f;
        }
        alias->volMin = scaled;
        scaled = alias->volMax * alias->volMod;
        if (scaled < 0.0f) {
            scaled = 0.0f;
        } else if (1.0f < scaled) {
            scaled = 1.0f;
        }
        alias->volMax = scaled;
    }
    return 1;
}

// 0x080ae532  size=132  FUN_080ae532  status=translated  naming=observed
// naming evidence: allocation tag string "Com_AddBuildSoundAlias"
void Com_AddBuildSoundAlias(struct snd_alias_build_t *src)
{
    struct snd_alias_build_t *copy = (struct snd_alias_build_t *)Com_HunkAllocTempTagged(0x154, "Com_AddBuildSoundAlias");
    *copy = *src;
    copy->next = g_soundAliasBuildHead;
    g_soundAliasBuildHead = copy;
    g_soundAliasBuildCount = g_soundAliasBuildCount + 1;
}

// 0x080ae5b6  size=25  FUN_080ae5b6  status=translated  naming=inferred
// naming evidence: clears build list head g_soundAliasBuildHead and count g_soundAliasBuildCount
void Com_ClearBuildSoundAliases(void)
{
    g_soundAliasBuildHead = 0;
    g_soundAliasBuildCount = 0;
}

// 0x080ae5d0  size=816  FUN_080ae5d0  status=translated  naming=observed
// naming evidence: strings VOLUMEMODGROUPS, soundaliases/volumemodgroups.def, volumemodgroups parse failure errors
void Com_LoadVolumeModGroups(struct snd_volmod_group_t *groups)
{
    size_t headerLen = strlen("VOLUMEMODGROUPS");
    char path[64];
    char buf[8200];
    const char *text;
    char *token;
    int handle;
    int len;
    int groupCount;

    strcpy(path, "soundaliases/volumemodgroups.def");
    len = FS_OpenFileRead(path, &handle, 1);
    if (len < 0) {
        Com_Error(1, "ERROR: Could not find '%s'\n", path);
    } else if (len == 0) {
        FS_FCloseFile(handle);
        Com_Error(1, "ERROR: '%s' is empty\n", path);
    } else {
        FS_Read(buf, headerLen, handle);
        buf[headerLen] = '\0';
        if (strncmp(buf, "VOLUMEMODGROUPS", headerLen) == 0) {
            if ((int)(len - headerLen) < 0x2000) {
                memset(buf, 0, 0x2000);
                FS_Read(buf, len - headerLen, handle);
                buf[len - headerLen] = '\0';
                FS_FCloseFile(handle);
                Com_BeginParseSession(path);
                text = buf;
                groupCount = 0;
                while (token = Com_Parse(&text), *token != '\0' && *token != '}') {
                    if (0x1f < groupCount) {
                        Com_EndParseSession();
                        Com_Error(1, "ERROR: volumemodgroups parse failure on file \"%s\": groups parsed (%d) is greater than or equal to maxGroups(%d)\n",
                                    path, groupCount, 0x20);
                        return;
                    }
                    strcpy(groups[groupCount].name, token);
                    token = Com_Parse(&text);
                    if (*token == '\0' || *token == '}') {
                        Com_EndParseSession();
                        Com_Error(1, "ERROR: volumemodgroups parse failure on file \"%s\": groupname '%s' missing a matching value\n",
                                    path, groups[groupCount].name);
                        return;
                    }
                    groups[groupCount].volume = (float)atof(token);
                    groupCount = groupCount + 1;
                }
                Com_EndParseSession();
            } else {
                FS_FCloseFile(handle);
                Com_Error(1, "ERROR: \"%s\" Is too long of a volumemodgroups file to parse\n", path);
            }
        } else {
            FS_FCloseFile(handle);
            Com_Error(1, "ERROR: \"%s\" does not appear to be a volumemodgroups file\n", path);
        }
    }
}
