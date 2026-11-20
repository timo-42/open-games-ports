#include <ctype.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_013_types.h"

/*
 * chunk_013: cod2_lnxded 1.3, 0x08076d26..0x08079e74 (88 functions).
 * Translated from the binary's observable behaviour (Ghidra pseudocode checked
 * against the disassembly). Helpers with decompiler-visible void returns whose
 * callers consume EAX keep that value as their return (noted per function).
 */

/* Forward declarations of chunk functions used before their definition. */
int ScrDbg_FindFrameIndex(uint32 pc);
void ScrStr_Release(uint32 handle);
void ScrStr_DecRefOrFree(uint32 handle, int bytes);
scr_string_s *ScrStr_Record(uint32 handle);
uint32 ScrStr_FindNulIndex(const scr_string_s *rec);
void ScrStr_FreeNode(uint32 handle, scr_string_s *rec, int bytes);
uint16 ScrStr_Find(const char *text, uint32 len);
uint32 ScrStr_Intern(const char *text, uint32 flags, uint32 len, uint32 extra);
int ScrStr_Hash(const char *text, uint32 len);
int ScrDbg_ScanFrameText(int32 frame_index, int32 nbytes);
int ScrDbg_CountLineBreaks(const char *text, int nbytes, const char **line_start, int *line_len);

/* ===================================================================== */
/* MT_ block allocator                                                    */
/* ===================================================================== */

// 0x08076d26  size=144  FUN_08076d26  status=translated  naming=inferred
// naming: MT_ family (MT_GetSize, MT_AllocIndex strings in neighbours); body resets 17 order free heads, seeds 16 orders via Mem_BucketTree_Insert, resets counters.
void MT_Init(void)
{
    int i;
    scr_string_s *rec0;

    str_pool_base = ((uint8 *)g_08206f80);
    Mem_InitBucketTables();
    for (i = 0; i < 0x11; i++) {
        g_08287280[i] = 0;
    }
    /* Record 0 is cleared as a 4-byte word followed by a 2-byte word: length, flags and refcount. */
    rec0 = (scr_string_s *)((uint8 *)g_08206f80);
    rec0->length = 0;
    rec0->flags = 0;
    rec0->refcount = 0;
    for (i = 0; i < 0x10; i++) {
        Mem_BucketTree_Insert(1u << (i & 0x1f), i);
    }
    g_082872a4 = 0;
    g_082872a8 = 0;
}

// 0x08076db6  size=51  FUN_08076db6  status=translated  naming=observed
// naming: format string "%s: failed memory allocation of %d bytes for script usage" at 0x08076db6 callsite; "failed memory allocation for script usage" string.
void MT_AllocFail(const char *where, int bytes)
{
    Mem_PrintBucketStats();
    Com_Printf("%s: failed memory allocation of %d bytes for script usage\n", where, bytes);
    Scr_ErrorAndDisable("failed memory allocation for script usage");
}

// 0x08076dea  size=128  FUN_08076dea  status=translated  naming=observed
// naming: string "MT_GetSize: max allocation exceeded" passed by this function.
uint32 MT_GetSize(int bytes)
{
    int t;
    int idx;
    uint32 order;

    if (bytes < 0x10000) {
        t = bytes + 7;
        if (t < 0) {
            t = bytes + 0xe;
        }
        idx = (t >> 3) - 1;
        if (idx < 0x100) {
            order = (uint32)g_0828717f[t >> 3];
        } else {
            order = (uint32)g_08287180[idx >> 8] + 8;
        }
        return order;
    }
    MT_AllocFail("MT_GetSize: max allocation exceeded", bytes);
    return 0;
}

// 0x08076e6a  size=183  FUN_08076e6a  status=translated  naming=observed
// naming: string "MT_AllocIndex" passed to MT_AllocFail on exhaustion. Callers pass a second stack argument that this function never reads.
uint32 MT_AllocIndex(int bytes)
{
    int order0;
    int order;
    uint32 handle;

    order0 = (int)MT_GetSize(bytes);
    order = order0;
    for (;;) {
        if (order > 0x10) {
            MT_AllocFail("MT_AllocIndex", bytes);
            return 0;
        }
        handle = (uint32)g_08287280[order];
        if (handle != 0) {
            break;
        }
        order++;
    }
    Mem_BucketTree_PopRoot(order);
    while (order != order0) {
        order--;
        Mem_BucketTree_Insert((1u << ((uint8)order & 0x1f)) + handle, order);
    }
    g_082872a4++;
    g_082872a8 += 1 << ((uint8)order0 & 0x1f);
    return handle;
}

// 0x08076f22  size=125  FUN_08076f22  status=translated  naming=inferred
// naming: MT_ family free counterpart of MT_AllocIndex (same order/size accounting); buddy coalescing via Mem_BucketTree_Remove.
void MT_FreeIndex(uint32 handle, int bytes)
{
    int order;
    uint32 bit;

    order = (int)MT_GetSize(bytes);
    g_082872a4--;
    g_082872a8 -= 1 << ((uint8)order & 0x1f);
    for (; bit = 1u << ((uint8)order & 0x1f), order != 0x10; order++) {
        if (!Mem_BucketTree_Remove(bit ^ handle, order)) {
            break;
        }
        handle &= ~bit;
    }
    Mem_BucketTree_Insert(handle, order);
}

// 0x08076fa0  size=201  FUN_08076fa0  status=translated  naming=inferred
// naming: inserts a handle into the free lists at the largest free aligned order (buddy merge), used by MT_RebuildFreeLists.
void MT_AddFreeHandle(uint32 handle)
{
    int order;
    uint32 h;
    uint32 bit;

    order = 0;
    h = handle;
    for (;;) {
        if (Mem_BucketTree_Remove(h, order)) {
            Mem_BucketTree_Insert(h, order);
            return;
        }
        if (order == 0x10) {
            break;
        }
        h &= ~(1u << ((uint8)order & 0x1f));
        order++;
    }
    order = 0;
    h = handle;
    for (;;) {
        bit = 1u << ((uint8)order & 0x1f);
        if (order == 0x10 || !Mem_BucketTree_Remove(bit ^ h, order)) {
            break;
        }
        h &= ~bit;
        order++;
    }
    Mem_BucketTree_Insert(h, order);
}

// 0x0807706a  size=37  FUN_0807706a  status=translated  naming=inferred
// naming: returns a pointer into the 8-byte string pool record array for a fresh MT block.
uint8 *StrPool_AllocBlock(int bytes, int caller_arg)
{
    uint32 handle;

    (void)caller_arg;
    handle = MT_AllocIndex(bytes);
    return ((uint8 *)g_08206f80) + ((handle & 0xffff) << 3);
}

// 0x08077090  size=34  FUN_08077090  status=translated  naming=inferred
// naming: inverse of StrPool_AllocBlock; converts a pool record pointer back to its MT handle.
void StrPool_FreeBlock(void *blockPtr, int bytes)
{
    uint8 *block = (uint8 *)blockPtr;
    int32 delta;

    delta = (int32)(block - (uint8 *)g_08206f80); /* binary: ptr - 0x08206f80 (the pool base) */
    MT_FreeIndex((uint32)(delta >> 3), bytes);
}

// 0x080770b2  size=40  FUN_080770b2  status=translated  naming=unknown
// naming: no string; resets the MT counters and returns Com_Calloc(0x2000) (8 KiB handle bitmap; caller 0x0807a111 stores EAX).
uint8 *MT_ResetStats(void)
{
    g_082872a4 = 0;
    g_082872a8 = 0;
    /* EAX of the Com_Calloc call is the function's result (bitmap handed to MT_MarkAllocatedBlock). */
    return (uint8 *)Com_Calloc(0x2000);
}

// 0x080770da  size=121  FUN_080770da  status=translated  naming=inferred
// naming: marks the bit range of an allocated MT block in a 0x10000-bit handle bitmap and updates the counters.
void MT_MarkAllocatedBlock(uint8 *bitmap, uint32 handle, int bytes)
{
    uint8 order;
    int32 count;

    order = (uint8)MT_GetSize(bytes);
    g_082872a4++;
    count = 1 << (order & 0x1f);
    g_082872a8 += count;
    for (; count != 0; count--) {
        bitmap[handle >> 3] = (uint8)(1 << (handle & 7)) | bitmap[handle >> 3];
        handle++;
    }
}

// 0x08077154  size=87  FUN_08077154  status=translated  naming=inferred
// naming: rebuilds the free lists from a handle bitmap (clear bits are free), then releases the bitmap.
void MT_RebuildFreeLists(uint8 *bitmap)
{
    uint32 i;

    for (i = 1; i < 0x10000; i++) {
        if ((((int)(uint32)bitmap[i >> 3]) >> (i & 7) & 1u) == 0) {
            MT_AddFreeHandle(i);
        }
    }
    Com_Free_B(bitmap);
}

// 0x080771ac  size=48  FUN_080771ac  status=translated  naming=inferred
// naming: compares the MT size classes of two byte sizes (returns order(other) <= order(bytes)).
bool MT_SizeOrderLE(int bytes, int other)
{
    int a;
    int b;

    a = (int)MT_GetSize(bytes);
    b = (int)MT_GetSize(other);
    return b <= a;
}

/* ===================================================================== */
/* Script debug tables                                                    */
/* ===================================================================== */

// 0x080771dc  size=231  FUN_080771dc  status=translated  naming=inferred
// naming: script debug-table init (file, line and frame tables sized 0x10000/0x10000/0x10), gated on scr_debug_enabled.
void ScrDbg_Init(void)
{
    if (scr_debug_enabled != 0) {
        scr_pending_a = -1;
        scr_file_table_cap = 0x10000;
        scr_file_table_count = 0;
        scr_file_table = (scr_file_entry_s *)Com_MallocZeroed(0x140000);
        memset(scr_file_table, 0, scr_file_table_cap * 0x14);
        scr_line_table_cap = 0x10000;
        scr_line_table_count = 0;
        scr_line_table = (scr_line_s *)Com_MallocZeroed(0x80000);
        scr_last_file_key = 0;
        scr_cur_file_line_count = 0;
        scr_frame_table_cap = 0x10;
        scr_frame_count = 0;
        g_08287334 = (scr_frame_s *)Com_MallocZeroed(0x180);
    }
}

// 0x080772c4  size=270  FUN_080772c4  status=translated  naming=inferred
// naming: frees the script debug tables allocated by ScrDbg_Init and the frame/saved-source blocks.
void ScrDbg_Shutdown(void)
{
    uint32 i;

    if (scr_file_table != 0) {
        Com_Free_A(scr_file_table);
        scr_file_table = 0;
    }
    if (scr_line_table != 0) {
        Com_Free_A(scr_line_table);
        scr_line_table = 0;
    }
    if (g_08287334 != 0) {
        for (i = 0; i < (uint32)scr_frame_count; i++) {
            Com_Free_A(g_08287334[i].name);
        }
        Com_Free_A(g_08287334);
        g_08287334 = 0;
    }
    if (scr_saved_src != 0) {
        for (i = 0; (int)i < scr_saved_src_count; i++) {
            if (scr_saved_src[i].text != 0) {
                Com_Free_A(scr_saved_src[i].text);
            }
        }
        Com_Free_A(scr_saved_src);
        scr_saved_src = 0;
    }
}

// 0x080773d2  size=539  FUN_080773d2  status=translated  naming=inferred
// naming: pushes a source line (value, flags) onto the debug file/line tables, growing them when full; -1/-2 values set pending slots.
void ScrDbg_PushLine(int32 line_value, uint32 flags)
{
    scr_file_entry_s *entry;
    scr_line_s *line;
    int32 index;
    int32 bytes;
    void *grown;

    if (scr_debug_enabled == 0 || scrCompileMode == 2) {
        return;
    }
    if (scrOpcodeAtStackBase == 0) {
        flags &= 0xfffffffe;
    }
    if (scr_file_table_cap <= scr_file_table_count) {
        bytes = scr_file_table_cap * 0x28;
        scr_file_table_cap = scr_file_table_cap * 2;
        grown = Com_MallocZeroed(bytes);
        memcpy(grown, scr_file_table, scr_file_table_count * 0x14);
        Com_Free_A(scr_file_table);
        scr_file_table = (scr_file_entry_s *)grown;
    }
    if (scr_line_table_cap <= scr_line_table_count) {
        bytes = scr_line_table_cap << 4;
        scr_line_table_cap = scr_line_table_cap * 2;
        grown = Com_MallocZeroed(bytes);
        memcpy(grown, scr_line_table, scr_line_table_count << 3);
        Com_Free_A(scr_line_table);
        scr_line_table = (scr_line_s *)grown;
    }
    if (scr_last_file_key == (uint32)(size_t)scrLastOpcodePtr) {
        scr_file_table_count--;
        entry = &scr_file_table[scr_file_table_count];
    } else {
        scr_cur_file_line_count = 0;
        scr_last_file_key = (uint32)(size_t)scrLastOpcodePtr;
        entry = &scr_file_table[scr_file_table_count];
        entry->first_line = scr_line_table_count;
        entry->key = scr_last_file_key;
    }
    index = scr_cur_file_line_count + entry->first_line;
    line = &scr_line_table[index];
    line->value = line_value;
    if (line_value == -1) {
        scr_pending_a = index;
    } else if (line_value == -2) {
        scr_pending_b = index;
    } else if (scr_pending_a >= 0 && (flags & 1) != 0) {
        scr_line_table[scr_pending_a].value = line_value;
        scr_pending_a = -1;
    }
    line->flags |= flags;
    scr_cur_file_line_count++;
    entry->line_count = scr_cur_file_line_count;
    scr_file_table_count++;
    scr_line_table_count++;
}

// 0x080775ee  size=103  FUN_080775ee  status=translated  naming=inferred
// naming: pops the most recent source line pushed by ScrDbg_PushLine.
void ScrDbg_PopLine(void)
{
    if (scr_debug_enabled == 0 || scrCompileMode == 2) {
        return;
    }
    scr_line_table_count--;
    scr_file_table_count--;
    scr_cur_file_line_count--;
    if (scr_cur_file_line_count == 0) {
        scr_last_file_key = 0;
    }
    scr_file_table[scr_file_table_count].line_count = scr_cur_file_line_count;
}

// 0x08077656  size=75  FUN_08077656  status=translated  naming=inferred
// naming: fills the pending -2 line slot with a value and marks it with flag 4.
void ScrDbg_SetPendingEntry(int32 value)
{
    scr_line_s *line;

    if (scr_debug_enabled == 0 || scrCompileMode == 2) {
        return;
    }
    line = &scr_line_table[scr_pending_b];
    line->value = value;
    line->flags = 4;
    scr_pending_b = -1;
}

// 0x080776a2  size=465  FUN_080776a2  status=translated  naming=inferred
// naming: finds the best (lowest-key) line of a frame whose value lies in [lo,hi) and whose flags include the mask.
uint32 ScrDbg_FindBestLine(int32 frame_index, uint32 value_lo, uint32 value_hi, uint32 required_flags, uint32 *out_value)
{
    scr_frame_s *next;
    uint32 frame_id;
    uint32 upper;
    uint32 best_key;
    uint32 best_value;
    uint32 i;
    int32 j;
    uint32 key;
    scr_line_s *line;

    if ((uint32)(frame_index + 1) < (uint32)scr_frame_count) {
        next = &g_08287334[frame_index + 1];
    } else {
        next = 0;
    }
    frame_id = g_08287334[frame_index].id;
    if (frame_id == 0) {
        *out_value = 0;
        return 0;
    }
    best_key = 0;
    best_value = 0;
    upper = (next == 0) ? 0xffffffff : next->id - 1;
    for (i = 0; i < (uint32)scr_file_table_count; i++) {
        for (j = 0; j < scr_file_table[i].line_count; j++) {
            key = scr_file_table[i].key;
            line = &scr_line_table[j + scr_file_table[i].first_line];
            if (frame_id <= key && key <= upper && (required_flags & line->flags) == required_flags) {
                if (value_lo <= (uint32)line->value && (uint32)line->value < value_hi) {
                    if (best_key == 0 || key < best_key) {
                        best_value = (uint32)line->value;
                        best_key = key;
                    }
                }
            }
        }
    }
    *out_value = best_value;
    return best_key;
}

// 0x08077874  size=385  FUN_08077874  status=translated  naming=inferred
// naming: largest line value not above max_value inside a frame's key range, requiring the flag mask.
uint32 ScrDbg_FindMaxLineValue(int32 frame_index, uint32 max_value, uint32 required_flags)
{
    scr_frame_s *next;
    uint32 frame_id;
    uint32 upper;
    uint32 best;
    uint32 i;
    int32 j;
    uint32 key;
    scr_line_s *line;

    best = 0;
    if ((uint32)(frame_index + 1) < (uint32)scr_frame_count) {
        next = &g_08287334[frame_index + 1];
    } else {
        next = 0;
    }
    frame_id = g_08287334[frame_index].id;
    if (frame_id == 0) {
        return 0;
    }
    upper = (next == 0) ? 0xffffffff : next->id - 1;
    for (i = 0; i < (uint32)scr_file_table_count; i++) {
        for (j = 0; j < scr_file_table[i].line_count; j++) {
            key = scr_file_table[i].key;
            if (frame_id <= key && key <= upper) {
                line = &scr_line_table[j + scr_file_table[i].first_line];
                if ((required_flags & line->flags) == required_flags &&
                    best <= (uint32)line->value && (uint32)line->value <= max_value) {
                    best = (uint32)line->value;
                }
            }
        }
    }
    return best;
}

// 0x080779f6  size=189  FUN_080779f6  status=translated  naming=inferred
// naming: binary search of the file table for the last entry whose key is not above the key (floor lookup); NULL when none.
scr_file_entry_s *ScrDbg_FindFileEntryFloor(uint32 key)
{
    int32 lo;
    int32 hi;
    int32 mid;

    lo = 0;
    hi = scr_file_table_count - 1;
    mid = 0;
    do {
        for (;;) {
            if (hi < lo) {
                return 0;
            }
            mid = (lo + hi) / 2;
            if (scr_file_table[mid].key <= key) {
                break;
            }
            hi = mid - 1;
        }
        lo = mid + 1;
    } while (lo != scr_file_table_count && scr_file_table[lo].key <= key);
    return &scr_file_table[mid];
}

// 0x08077ab4  size=181  FUN_08077ab4  status=translated  naming=inferred
// naming: binary search of the file table for an exact key match; returns the entry or NULL.
scr_file_entry_s *ScrDbg_FindFileEntryExact(uint32 key)
{
    int32 lo;
    int32 hi;
    int32 mid;

    lo = 0;
    hi = scr_file_table_count - 1;
    mid = 0;
    for (;;) {
        for (;;) {
            if (hi < lo) {
                return 0;
            }
            mid = (lo + hi) / 2;
            if (scr_file_table[mid].key <= key) {
                break;
            }
            hi = mid - 1;
        }
        if (key == scr_file_table[mid].key) {
            break;
        }
        lo = mid + 1;
    }
    return &scr_file_table[mid];
}

// 0x08077b6a  size=42  FUN_08077b6a  status=translated  naming=inferred
// naming: line value of the entry at (floor file entry of key).first_line + line_offset.
uint32 ScrDbg_LineValue(uint32 key, int32 line_offset)
{
    scr_file_entry_s *entry;

    entry = ScrDbg_FindFileEntryFloor(key);
    return (uint32)scr_line_table[line_offset + entry->first_line].value;
}

// 0x08077b94  size=81  FUN_08077b94  status=translated  naming=inferred
// naming: scans nbytes of a NUL-separated text block, returning the NUL count and the start/length of the last line.
int ScrDbg_CountLineBreaks(const char *text, int nbytes, const char **line_start, int *line_len)
{
    int count;

    count = 0;
    *line_start = text;
    for (; nbytes != 0; nbytes--) {
        if (*text == '\0') {
            *line_start = text + 1;
            count++;
        }
        text++;
    }
    *line_len = (int)(text - *line_start);
    return count;
}

// 0x08077be6  size=62  FUN_08077be6  status=translated  naming=inferred
// naming: scans the text of a frame; the caller consumes EAX, which carries the return of ScrDbg_CountLineBreaks.
int ScrDbg_ScanFrameText(int32 frame_index, int32 nbytes)
{
    const char *line_start;
    int line_len;

    return ScrDbg_CountLineBreaks(g_08287334[frame_index].source, nbytes, &line_start, &line_len);
}

// 0x08077c24  size=200  FUN_08077c24  status=translated  naming=inferred
// naming: finds the first line of a file key whose flags include the mask; fills out[0]=frame index, out[1]=scan result, out[2]=line value.
int ScrDbg_LookupLineForKey(uint32 key, uint32 required_flags, uint32 *out)
{
    scr_file_entry_s *entry;
    uint32 i;
    scr_line_s *line;

    entry = ScrDbg_FindFileEntryExact(key);
    if (entry != 0) {
        for (i = 0; i < (uint32)entry->line_count; i++) {
            line = &scr_line_table[i + entry->first_line];
            if ((required_flags & line->flags) == required_flags) {
                out[2] = (uint32)line->value;
                out[0] = (uint32)ScrDbg_FindFrameIndex(key);
                out[1] = (uint32)ScrDbg_ScanFrameText((int32)out[0], line->value);
                return 1;
            }
        }
    }
    return 0;
}

// 0x08077cec  size=12  FUN_08077cec  status=translated  naming=inferred
// naming: returns the script debug enable byte.
uint8 ScrDbg_IsEnabled(void)
{
    return scr_debug_enabled;
}

// 0x08077cf8  size=152  FUN_08077cf8  status=translated  naming=inferred
// naming: appends one zeroed frame slot to the frame table, growing it (capacity doubles) when full.
scr_frame_s *ScrDbg_AllocFrame(void)
{
    scr_frame_s *frame;
    int32 bytes;
    void *grown;

    if (scr_frame_table_cap <= scr_frame_count) {
        bytes = scr_frame_table_cap * 0x30;
        scr_frame_table_cap = scr_frame_table_cap * 2;
        grown = Com_MallocZeroed(bytes);
        Com_Memcpy(grown, g_08287334, scr_frame_count * 0x18);
        Com_Free_A(g_08287334);
        g_08287334 = (scr_frame_s *)grown;
    }
    frame = &g_08287334[scr_frame_count];
    scr_frame_count++;
    return frame;
}

// 0x08077d90  size=364  FUN_08077d90  status=translated  naming=inferred
// naming: appends a named frame with optional source text (copied; optionally sanitised of line breaks) and records the last source pointer.
void ScrDbg_AddFrame(const char *name, uint32 id, const char *src, int32 src_len, uint8 sanitize, uint8 is_debug)
{
    size_t name_len;
    char *dest;
    char *dst;
    const char *srcp;
    char *dst_text;
    scr_frame_s *frame;
    int32 i;
    char c;

    if (g_08287334 == 0) {
        scr_last_source_text = 0;
        return;
    }
    name_len = strlen(name);
    dest = (char *)Com_MallocZeroed(src_len + (int32)name_len + 1 + 2);
    strcpy(dest, name);
    if (src == 0) {
        dst_text = 0;
    } else {
        dst_text = dest + name_len + 1;
    }
    srcp = src;
    dst = dst_text;
    if (sanitize == 0) {
        for (i = 0; i <= src_len; i++) {
            *dst = *srcp;
            srcp++;
            dst++;
        }
    } else {
        for (i = 0; i <= src_len; i++) {
            c = *srcp;
            srcp++;
            if (c == '\n' || (c == '\r' && *srcp != '\n')) {
                *dst = '\0';
            } else {
                *dst = c;
            }
            dst++;
        }
    }
    frame = ScrDbg_AllocFrame();
    frame->id = id;
    frame->name = dest;
    frame->source = dst_text;
    frame->source_len = src_len;
    frame->current_line = -1;
    frame->is_debug = is_debug;
    if (dst_text != 0) {
        scr_last_source_text = dst_text;
    }
}

// 0x08077efc  size=225  FUN_08077efc  status=translated  naming=inferred
// naming: loads a source file through the filesystem (FSH_FOpenFile) into a heap buffer and registers it as a frame; the first argument is forwarded by callers and unused here.
char *ScrDbg_LoadFrameFile(int unused_channel, const char *name, uint32 id, uint8 is_debug)
{
    int32 buf;
    int32 len;
    char *out;

    (void)unused_channel;
    buf = 0;
    len = FSH_FOpenFile(name, &buf, 0);
    if (len < 0) {
        ScrDbg_AddFrame(name, id, 0, -1, 1, is_debug);
        return 0;
    }
    out = (char *)Hunk_AllocateTempMemoryHigh(len + 1);
    FS_Read(out, len, buf);
    out[len] = '\0';
    FS_FCloseFile(buf);
    ScrDbg_AddFrame(name, id, out, len, 1, is_debug);
    return out;
}

// 0x08077fde  size=330  FUN_08077fde  status=translated  naming=inferred
// naming: when is_savegame is set and a saved-source stack exists, pops the top saved text (NUL bytes turned to newlines) as a frame; otherwise loads the file.
char *ScrDbg_LoadFrameFileOrSaved(int unused_channel, const char *name, uint32 id, uint8 is_savegame)
{
    char *result;
    scr_saved_src_s *entry;
    int32 len;
    char *buf;
    char *dst;
    char *src;
    int32 i;
    char ch;

    if (is_savegame == 0 || scr_saved_src == 0) {
        result = ScrDbg_LoadFrameFile(unused_channel, name, id, is_savegame);
        return result;
    }
    scr_saved_src_count--;
    entry = &scr_saved_src[scr_saved_src_count];
    len = entry->length;
    buf = 0;
    if (len >= 0) {
        buf = (char *)Hunk_AllocateTempMemoryHigh(len + 1);
        src = entry->text;
        dst = buf;
        for (i = 0; i < len; i++) {
            ch = *src;
            src++;
            if (ch == '\0') {
                ch = '\n';
            }
            *dst = ch;
            dst++;
        }
        *dst = '\0';
        if (entry->text != 0) {
            Com_Free_A(entry->text);
        }
    }
    ScrDbg_AddFrame(name, id, buf, len, 1, is_savegame);
    result = buf;
    return result;
}

// 0x08078128  size=173  FUN_08078128  status=translated  naming=inferred
// naming: extracts the last line of a text window into out_line (tabs to spaces, trailing CR removed); returns the NUL count.
int ScrDbg_ExtractLine(const char *text, int nbytes, int *line_len, char *out_line)
{
    const char *line;
    int count;
    size_t len;
    int i;
    char c;

    count = ScrDbg_CountLineBreaks(text, nbytes, &line, line_len);
    len = strlen(line);
    if (0x3ff < (int)len) {
        len = 0x3ff;
    }
    for (i = 0; i <= (int)len; i++) {
        c = line[i];
        if (c == '\t') {
            c = ' ';
        }
        out_line[i] = c;
    }
    if (out_line[(int)len - 1] == '\r') {
        out_line[(int)len - 1] = '\0';
    }
    return count;
}

// 0x080781d6  size=252  FUN_080781d6  status=translated  naming=inferred
// naming: prints a source location header, the source line and a caret line to a channel; " (savegame)" tag when a saved source is active.
void ScrDbg_PrintSourceContext(int channel, const char *file_name, const char *text, int nbytes)
{
    char line_text[1032];
    int caret;
    int count;
    const char *suffix;
    char *msg;
    int i;

    count = ScrDbg_ExtractLine(text, nbytes, &caret, line_text);
    suffix = (scr_saved_src == 0) ? "" : " (savegame)";
    msg = va("(file '%s'%s, line %d)\n", file_name, suffix, count + 1);
    Com_PrintMessage(channel, msg);
    msg = va("%s\n", line_text);
    Com_PrintMessage(channel, msg);
    for (i = 0; i < caret; i++) {
        Com_PrintMessage(channel, " ");
    }
    Com_PrintMessage(channel, "*\n");
}

// 0x080782d2  size=200  FUN_080782d2  status=translated  naming=inferred
// naming: formats "<line> // <file><savegame>, line N" for a frame into a caller buffer; returns the NUL count.
int ScrDbg_FormatFrameLine(int frame_index, int nbytes, char *buf, int bufsize)
{
    char line_text[1032];
    int caret;
    int count;
    const char *suffix;

    count = ScrDbg_ExtractLine(g_08287334[frame_index].source, nbytes, &caret, line_text);
    suffix = (scr_saved_src == 0) ? "" : " (savegame)";
    Com_sprintf(buf, bufsize, "%s // %s%s, line %d", line_text, g_08287334[frame_index].name, suffix, count + 1);
    return count;
}

// 0x0807839a  size=95  FUN_0807839a  status=translated  naming=inferred
// naming: frame index lookup; the largest index >= 1 with a nonzero id not above the code key, else a value below 1.
int ScrDbg_FindFrameIndex(uint32 pc)
{
    int32 i;

    i = scr_frame_count;
    do {
        i--;
        if (i < 1) {
            return i;
        }
    } while (g_08287334[i].id == 0 || pc < g_08287334[i].id);
    return i;
}

// 0x080783fa  size=315  FUN_080783fa  status=translated  naming=inferred
// naming: prints a code location (frozen/removed thread, "@ offset", or the source context when debug is enabled).
void ScrDbg_PrintLocation(int channel, const char *pc, int32 line_bias)
{
    int idx;
    uint32 line;
    char *msg;

    if (pc == 0) {
        Com_PrintMessage(channel, "<frozen thread>\n");
        return;
    }
    if (pc == (const char *)scrVm_noOwnerSentinel) { /* binary compares with the address 0x083d7600 */
        Com_PrintMessage(channel, "<removed thread>\n");
        return;
    }
    if (scr_debug_enabled == 0) {
        if (Scr_IsInCodeRange((int)(size_t)(pc - 1)) != 0) {
            msg = va("@ %d\n", (int)((uint32)(size_t)pc - (uint32)g_scr_code_base));
            Com_PrintMessage(channel, msg);
            return;
        }
    } else if (g_scr_code_base != 0 && Scr_IsInCodeRange((int)(size_t)pc) != 0) {
        idx = ScrDbg_FindFrameIndex((uint32)(size_t)(pc - 1));
        line = ScrDbg_LineValue((uint32)(size_t)(pc - 1), line_bias);
        ScrDbg_PrintSourceContext(channel, g_08287334[idx].name, g_08287334[idx].source, (int)line);
        return;
    }
    msg = va("%s\n\n", pc);
    Com_PrintMessage(channel, msg);
}

// 0x080785ac  size=145  FUN_080785ac  status=translated  naming=inferred
// naming: maps a code key to its file name and 1-based line number (floor file entry, frame lookup).
void ScrDbg_GetFileAndLine(uint32 key, const char **out_file, int *out_line)
{
    scr_file_entry_s *entry;
    uint32 value;
    int idx;
    int count;

    entry = ScrDbg_FindFileEntryFloor(key);
    if (entry == 0) {
        *out_line = 0;
        *out_file = "";
    } else {
        value = (uint32)scr_line_table[entry->first_line].value;
        idx = ScrDbg_FindFrameIndex(key);
        count = ScrDbg_ScanFrameText(idx, (int32)value);
        *out_line = count + 1;
        *out_file = g_08287334[idx].name;
    }
}

// 0x0807863e  size=255  FUN_0807863e  status=translated  naming=inferred
// naming: body prints the string "******* script compile error *******" (string evidence for the error path; function name inferred); formats the message, prints it with source context, or defers it when scr_compile_error_deferred is set.
void ScrCompile_ErrorInFile(int nbytes, const char *fmt, ...)
{
    char buf[1036];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, 0x400, fmt, ap);
    va_end(ap);
    if (scr_compile_error_deferred == 0) {
        Com_Printf("\n");
        Com_Printf("******* script compile error *******\n");
        if (scr_debug_enabled == 0) {
            Com_Printf("%s\n", buf);
        } else {
            Com_Printf("%s: ", buf);
            ScrDbg_PrintSourceContext(0, g_scr_current_source_name, scr_last_source_text, nbytes);
        }
        Com_Printf("************************************\n");
        Com_Error(5, "\025script compile error\n(see console for details)");
    } else if (g_scr_compile_error_message == 0) {
        g_scr_compile_error_message = va("%s", buf);
    }
}

// 0x0807873e  size=158  FUN_0807873e  status=translated  naming=inferred
// naming: body prints "******* script compile error *******" (string evidence; function name inferred); compile-error report with a location at pc.
void ScrCompile_ErrorAt(const char *pc, const char *fmt, ...)
{
    char buf[1036];
    va_list ap;

    Com_Printf("\n");
    Com_Printf("******* script compile error *******\n");
    va_start(ap, fmt);
    vsnprintf(buf, 0x400, fmt, ap);
    va_end(ap);
    Com_Printf("%s: ", buf);
    ScrDbg_PrintLocation(0, pc, 0);
    Com_Printf("************************************\n");
    Com_Error(5, "\025script compile error\n(see console for details)");
}

// 0x080787dc  size=246  FUN_080787dc  status=translated  naming=observed
// naming: string "script runtime error" and the "called from:" / "started from:" labels (observed).
void ScrRuntime_ReportTrace(int channel, const char *pc, int32 extra, const char *msg)
{
    char *text;
    int32 i;

    text = va("\n******* script runtime error *******\n%s: ", msg);
    Com_PrintMessage(channel, text);
    ScrDbg_PrintLocation(channel, pc, extra);
    i = scr_call_depth;
    if (scr_call_depth != 0) {
        while (--i > 0) {
            Com_PrintMessage(channel, "called from:\n");
            ScrDbg_PrintLocation(channel, scr_call_stack[i].pos, scr_call_stack[i].localId == 0);
        }
        Com_PrintMessage(channel, "started from:\n");
        ScrDbg_PrintLocation(channel, scr_call_stack[0].pos, 1);
    }
    Com_PrintMessage(channel, "************************************\n");
}

// 0x080788d2  size=270  FUN_080788d2  status=translated  naming=observed
// naming: string "\025script runtime error\n(see console for details)" (observed); reports a runtime error through the trace or message path by the scr_error_* flags.
void ScrRuntime_Error(const char *pc, int32 extra, const char *msg, const char *detail)
{
    bool recoverable;
    int channel;
    const char *line_sep;
    int level;

    if (scr_debug_enabled == 0 && scr_error_fatal == 0) {
        return;
    }
    if (scr_error_plain == 0) {
        recoverable = false;
        if (scr_error_recoverable != 0 || scr_error_fatal != 0) {
            recoverable = true;
        }
        channel = recoverable ? 0 : 4;
        ScrRuntime_ReportTrace(channel, pc, extra, msg);
        if (!recoverable) {
            return;
        }
    } else {
        Com_Printf("%s\n", msg);
        if (scr_error_fatal == 0) {
            return;
        }
    }
    if (detail == 0) {
        line_sep = "";
        detail = "";
    } else {
        line_sep = "\n";
    }
    level = (scr_error_fatal == 0) ? 4 : 5;
    Com_Error(level, "\025script runtime error\n(see console for details)\n%s%s%s", msg, line_sep, detail);
}

/* ===================================================================== */
/* Small script value / list boxes                                        */
/* ===================================================================== */

// 0x080789e0  size=17  FUN_080789e0  status=translated  naming=unknown
// naming: stores one word through a pointer and returns it; body is byte-identical to ScrVal_StoreWordAlt.
uint32 *ScrVal_StoreWord(uint32 *dst, uint32 value)
{
    *dst = value;
    return dst;
}

// 0x080789f2  size=17  FUN_080789f2  status=translated  naming=unknown
// naming: duplicate of ScrVal_StoreWord (identical code at a second address).
uint32 *ScrVal_StoreWordAlt(uint32 *dst, uint32 value)
{
    *dst = value;
    return dst;
}

// 0x08078a04  size=41  FUN_08078a04  status=translated  naming=unknown
// naming: allocates a 4-byte box holding one word; out receives the box.
void **ScrBox_Make1(void **out, uint32 a)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(4);
    *out = box;
    *(uint32 *)*out = a;
    return out;
}

// 0x08078a2e  size=51  FUN_08078a2e  status=translated  naming=unknown
// naming: allocates an 8-byte box holding two words.
void **ScrBox_Make2(void **out, uint32 a, uint32 b)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(8);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    return out;
}

// 0x08078a62  size=61  FUN_08078a62  status=translated  naming=unknown
// naming: allocates a 12-byte box holding three words.
void **ScrBox_Make3(void **out, uint32 a, uint32 b, uint32 c)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(0xc);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    ((uint32 *)*out)[2] = c;
    return out;
}

// 0x08078aa0  size=51  FUN_08078aa0  status=translated  naming=unknown
// naming: duplicate of ScrBox_Make2 (identical code at a second address).
void **ScrBox_Make2Alt(void **out, uint32 a, uint32 b)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(8);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    return out;
}

// 0x08078ad4  size=71  FUN_08078ad4  status=translated  naming=unknown
// naming: allocates a 16-byte box holding four words.
void **ScrBox_Make4(void **out, uint32 a, uint32 b, uint32 c, uint32 d)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(0x10);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    ((uint32 *)*out)[2] = c;
    ((uint32 *)*out)[3] = d;
    return out;
}

// 0x08078b5a  size=81  FUN_08078b5a  status=translated  naming=unknown
// naming: allocates a 20-byte box holding five words.
void **ScrBox_Make5(void **out, uint32 a, uint32 b, uint32 c, uint32 d, uint32 e)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(0x14);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    ((uint32 *)*out)[2] = c;
    ((uint32 *)*out)[3] = d;
    ((uint32 *)*out)[4] = e;
    return out;
}

// 0x08078bf4  size=91  FUN_08078bf4  status=translated  naming=unknown
// naming: allocates a 24-byte box holding six words.
void **ScrBox_Make6(void **out, uint32 a, uint32 b, uint32 c, uint32 d, uint32 e, uint32 f)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(0x18);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    ((uint32 *)*out)[2] = c;
    ((uint32 *)*out)[3] = d;
    ((uint32 *)*out)[4] = e;
    ((uint32 *)*out)[5] = f;
    return out;
}

// 0x08078c50  size=101  FUN_08078c50  status=translated  naming=unknown
// naming: allocates a 28-byte box holding seven words.
void **ScrBox_Make7(void **out, uint32 a, uint32 b, uint32 c, uint32 d, uint32 e, uint32 f, uint32 g)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(0x1c);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    ((uint32 *)*out)[2] = c;
    ((uint32 *)*out)[3] = d;
    ((uint32 *)*out)[4] = e;
    ((uint32 *)*out)[5] = f;
    ((uint32 *)*out)[6] = g;
    return out;
}

// 0x08078cb6  size=111  FUN_08078cb6  status=translated  naming=unknown
// naming: allocates a 32-byte box holding eight words.
void **ScrBox_Make8(void **out, uint32 a, uint32 b, uint32 c, uint32 d, uint32 e, uint32 f, uint32 g, uint32 h)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(0x20);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    ((uint32 *)*out)[2] = c;
    ((uint32 *)*out)[3] = d;
    ((uint32 *)*out)[4] = e;
    ((uint32 *)*out)[5] = f;
    ((uint32 *)*out)[6] = g;
    ((uint32 *)*out)[7] = h;
    return out;
}

// 0x08078d26  size=121  FUN_08078d26  status=translated  naming=unknown
// naming: allocates a 36-byte box holding nine words.
void **ScrBox_Make9(void **out, uint32 a, uint32 b, uint32 c, uint32 d, uint32 e, uint32 f, uint32 g, uint32 h, uint32 i)
{
    uint32 *box;

    box = (uint32 *)Hunk_AllocateTempMemoryHigh(0x24);
    *out = box;
    ((uint32 *)*out)[0] = a;
    ((uint32 *)*out)[1] = b;
    ((uint32 *)*out)[2] = c;
    ((uint32 *)*out)[3] = d;
    ((uint32 *)*out)[4] = e;
    ((uint32 *)*out)[5] = f;
    ((uint32 *)*out)[6] = g;
    ((uint32 *)*out)[7] = h;
    ((uint32 *)*out)[8] = i;
    return out;
}

// 0x08078da0  size=86  FUN_08078da0  status=translated  naming=inferred
// naming: creates a list holding one value (head and tail set to the new node).
void **ScrList_Create(void **out, uint32 value)
{
    scr_list_node_s *node;
    scr_list_s *list;

    node = (scr_list_node_s *)Hunk_AllocateTempMemoryHigh(8);
    node->value = value;
    node->next = 0;
    list = (scr_list_s *)Hunk_AllocateTempMemoryHigh(8);
    *out = list;
    list->head = node;
    list->tail = node;
    return out;
}

// 0x08078df6  size=69  FUN_08078df6  status=translated  naming=inferred
// naming: pushes a value on the front of a list (head at offset 0); out receives the list.
void **ScrList_PushFront(void **out, uint32 value, scr_list_s *list)
{
    scr_list_node_s *node;

    node = (scr_list_node_s *)Hunk_AllocateTempMemoryHigh(8);
    node->value = value;
    node->next = list->head;
    list->head = node;
    *out = list;
    return out;
}

// 0x08078e3c  size=87  FUN_08078e3c  status=translated  naming=inferred
// naming: appends a value at the tail of a list (tail at offset 4); out receives the list.
void **ScrList_Append(void **out, scr_list_s *list, uint32 value)
{
    scr_list_node_s *node;

    node = (scr_list_node_s *)Hunk_AllocateTempMemoryHigh(8);
    node->value = value;
    node->next = 0;
    list->tail->next = node;
    list->tail = node;
    *out = list;
    return out;
}

// 0x08078e94  size=51  FUN_08078e94  status=translated  naming=inferred
// naming: splices the chain described by other (head, tail) onto the tail of list; out receives the list.
void **ScrList_Splice(void **out, scr_list_s *list, scr_list_s *other)
{
    list->tail->next = other->head;
    list->tail = other->tail;
    *out = list;
    return out;
}

/* ===================================================================== */
/* String pool (ScrStr_*)                                                 */
/* ===================================================================== */

// 0x08078ec8  size=17  FUN_08078ec8  status=translated  naming=inferred
// naming: handle to record address (handle * 8 + pool base).
scr_string_s *ScrStr_Record(uint32 handle)
{
    return (scr_string_s *)(str_pool_base + handle * 8);
}

// 0x08078eda  size=11  FUN_08078eda  status=translated  naming=inferred
// naming: text pointer to record address (text sits 4 bytes into the record).
scr_string_s *ScrStr_RecordFromText(const char *text)
{
    return (scr_string_s *)((char *)text - 4);
}

// 0x08078ee6  size=43  FUN_08078ee6  status=translated  naming=inferred
// naming: handle to text pointer; handle 0 yields NULL.
const char *ScrStr_TextFromHandle(uint32 handle)
{
    if (handle == 0) {
        return 0;
    }
    return (const char *)ScrStr_Record(handle) + 4;
}

// 0x08078f12  size=58  FUN_08078f12  status=translated  naming=inferred
// naming: scans the record's text in 256-byte steps from (length - 1) for the NUL terminator; returns its index.
uint32 ScrStr_FindNulIndex(const scr_string_s *rec)
{
    const uint8 *bytes;
    uint32 idx;

    bytes = (const uint8 *)rec;
    idx = (uint32)(rec->length - 1) & 0xff;
    while (bytes[idx + 4] != 0) {
        idx += 0x100;
    }
    return idx;
}

// 0x08078f4c  size=33  FUN_08078f4c  status=translated  naming=inferred
// naming: NUL index of a handle's record; the caller consumes EAX, the return of ScrStr_FindNulIndex.
int ScrStr_NulIndexOfHandle(uint32 handle)
{
    scr_string_s *rec;

    rec = ScrStr_Record(handle);
    return (int)ScrStr_FindNulIndex(rec);
}

// 0x08078f6e  size=39  FUN_08078f6e  status=translated  naming=inferred
// naming: record address to pool handle (offset from pool base divided by 8).
int ScrStr_HandleOfRecord(const scr_string_s *rec)
{
    int local;

    local = (int)((size_t)rec - (size_t)str_pool_base);
    if (local < 0) {
        local += 7;
    }
    return local >> 3;
}

// 0x08078f96  size=27  FUN_08078f96  status=translated  naming=inferred
// naming: text pointer to pool handle; EAX of ScrStr_HandleOfRecord is the result (used by the script field-table code).
int ScrStr_HandleOfText(const char *text)
{
    scr_string_s *rec;

    rec = ScrStr_RecordFromText(text);
    return ScrStr_HandleOfRecord(rec);
}

// 0x08078fb2  size=194  FUN_08078fb2  status=translated  naming=inferred
// naming: string hash to a bucket index in 1..0x3fff (multiplier 0x1f; long strings hash their length quarter).
int ScrStr_Hash(const char *text, uint32 len)
{
    uint32 h;
    const char *p;

    if (len < 0x100) {
        h = 0;
        p = text;
        for (; len != 0; len--) {
            h = h * 0x1f + (uint32)(int)*p;
            p++;
        }
    } else {
        h = len >> 2;
    }
    return (int)(h % 0x3fff + 1);
}

// 0x08079074  size=141  FUN_08079074  status=translated  naming=inferred
// naming: initialises the string pool (MT_Init), the hash node chain and the ready flag.
void ScrStr_Init(void)
{
    uint32 prev;
    uint32 i;

    MT_Init();
    sl_hash[0].link = 0;
    prev = 0;
    for (i = 1; i < 0x4000; i++) {
        sl_hash[i].link = 0;
        sl_hash[prev].link = (uint16)i | sl_hash[prev].link;
        sl_hash[i].handle = (uint16)(int16)prev;
        prev = i;
    }
    sl_hash[0].handle = (uint16)(int16)prev;
    sl_hash_ready = 1;
}

// 0x08079102  size=29  FUN_08079102  status=translated  naming=inferred
// naming: lazy string pool initialisation; otherwise runs the ScrStr_ProcessTableAndListFlagged hook.
void ScrStr_EnsureInit(void)
{
    if (sl_hash_ready == 0) {
        ScrStr_Init();
    } else {
        ScrStr_ProcessTableAndListFlagged();
    }
}

// 0x08079120  size=23  FUN_08079120  status=translated  naming=inferred
// naming: clears the string pool ready flag.
void ScrStr_ClearReady(void)
{
    if (sl_hash_ready != 0) {
        sl_hash_ready = 0;
    }
}

// 0x08079138  size=477  FUN_08079138  status=translated  naming=inferred
// naming: looks up an existing interned string (exact bytes and length) in its bucket; a hit moves the node to the bucket head.
uint16 ScrStr_Find(const char *text, uint32 len)
{
    uint32 bucket;
    uint32 cur;
    uint32 next;
    uint16 *head;
    uint16 *slot;
    uint16 link_val;
    uint16 tmp;
    scr_string_s *rec;

    bucket = (uint32)ScrStr_Hash(text, len);
    cur = bucket;
    head = &sl_hash[bucket].link;
    if ((*head & 0xc000) == 0x8000) {
        rec = ScrStr_Record(sl_hash[bucket].handle);
        if ((uint32)rec->length == (len & 0xff) && memcmp(rec->text, text, len) == 0) {
            return sl_hash[bucket].handle;
        }
        link_val = *head;
        for (;;) {
            next = link_val & 0x3fff;
            slot = &sl_hash[next].link;
            if (slot == head) {
                break;
            }
            rec = ScrStr_Record(sl_hash[next].handle);
            if ((uint32)rec->length == (len & 0xff) && memcmp(rec->text, text, len) == 0) {
                sl_hash[cur].link = (uint16)((*slot & 0x3fff) | (sl_hash[cur].link & 0xc000));
                *slot = (uint16)((*head & 0x3fff) | (*slot & 0xc000));
                *head = (uint16)(next | (*head & 0xc000));
                tmp = sl_hash[next].handle;
                sl_hash[next].handle = sl_hash[bucket].handle;
                sl_hash[bucket].handle = tmp;
                return tmp;
            }
            cur = next;
            link_val = *slot;
        }
    }
    return 0;
}

// 0x08079316  size=35  FUN_08079316  status=translated  naming=inferred
// naming: looks up a C string as an interned string; result discarded.
uint32 ScrStr_LookupCString(const char *text)
{
    int len;

    len = ScrStr_Strlen(text);
    return ScrStr_Find(text, (uint32)(len + 1));
}

// 0x0807933a  size=169  FUN_0807933a  status=translated  naming=inferred
// naming: looks up the lowercase form of a C string (tolower over the bytes including the NUL).
uint32 ScrStr_LookupLowerCString(const char *text)
{
    int32 n;
    char lower[0x2004];
    int32 i;
    uint32 result;

    n = ScrStr_Strlen(text) + 1;
    if (n < 0x2001) {
        for (i = 0; i < n; i++) {
            lower[i] = (char)tolower((int)text[i]);
        }
        result = (uint32)ScrStr_Find(lower, (uint32)n);
    } else {
        result = 0;
    }
    return result;
}

// 0x080793e4  size=52  FUN_080793e4  status=translated  naming=inferred
// naming: sets a usage flag bit on a record; the first setting of a bit adds one reference.
void ScrStr_SetRefFlag(scr_string_s *rec, uint8 flag)
{
    if ((rec->flags & flag) == 0) {
        rec->flags = (uint8)(rec->flags | flag);
        rec->refcount++;
    }
}

// 0x08079418  size=36  FUN_08079418  status=translated  naming=inferred
// naming: handle form of ScrStr_SetRefFlag.
void ScrStr_SetRefFlagHandle(uint32 handle, uint8 flag)
{
    scr_string_s *rec;

    rec = ScrStr_Record(handle);
    ScrStr_SetRefFlag(rec, flag);
}

// 0x0807943c  size=1187  FUN_0807943c  status=translated  naming=inferred
// naming: interns a byte string into the pool (bucket search, then node and MT block allocation); the extra argument is passed to MT_AllocIndex, which ignores it.
uint32 ScrStr_Intern(const char *text, uint32 flags, uint32 len, uint32 extra)
{
    uint32 bucket;
    uint16 free_head0;
    uint32 cur;
    uint32 next;
    uint16 link_val;
    uint16 *head;
    uint16 *slot;
    uint16 tmp_free;
    uint16 result;
    uint16 old_head;
    uint32 free_idx;
    uint32 cursor;
    uint32 next_free;
    uint16 bucket_handle;
    scr_string_s *rec;

    (void)extra;
    bucket = (uint32)ScrStr_Hash(text, len);
    free_head0 = sl_hash[0].link;
    head = &sl_hash[bucket].link;
    result = 0;
    if ((*head & 0xc000) == 0x8000) {
        rec = ScrStr_Record(sl_hash[bucket].handle);
        if ((uint32)rec->length == (len & 0xff) && memcmp(rec->text, text, len) == 0) {
            ScrStr_SetRefFlag(rec, (uint8)flags);
            return sl_hash[bucket].handle;
        }
        link_val = *head;
        cur = bucket;
        for (;;) {
            tmp_free = sl_hash[0].link;
            next = link_val & 0x3fff;
            slot = &sl_hash[next].link;
            if (slot == head) {
                break;
            }
            rec = ScrStr_Record(sl_hash[next].handle);
            if ((uint32)rec->length == (len & 0xff) && memcmp(rec->text, text, len) == 0) {
                sl_hash[cur].link = (uint16)((*slot & 0x3fff) | (sl_hash[cur].link & 0xc000));
                *slot = (uint16)((*head & 0x3fff) | (*slot & 0xc000));
                *head = (uint16)(next | (*head & 0xc000));
                result = sl_hash[next].handle;
                sl_hash[next].handle = sl_hash[bucket].handle;
                sl_hash[bucket].handle = result;
                ScrStr_SetRefFlag(rec, (uint8)flags);
                return result;
            }
            cur = next;
            link_val = *slot;
        }
        free_idx = sl_hash[0].link;
        if (free_idx == 0) {
            Scr_DumpScriptThreads();
            Scr_NoOp();
            Com_Error(1, "\025exceeded maximum number of script strings\n");
        }
        result = (uint16)MT_AllocIndex((int)(len + 4));
        next_free = sl_hash[free_idx].link & 0x3fff;
        sl_hash[0].link = (uint16)next_free;
        sl_hash[next_free].handle = 0;
        sl_hash[free_idx].link = (uint16)((*head & 0x3fff) | 0x4000);
        *head = (uint16)((tmp_free & 0x3fff) | (*head & 0xc000));
        sl_hash[free_idx].handle = sl_hash[bucket].handle;
    } else {
        if ((*head & 0xc000) == 0) {
            result = (uint16)MT_AllocIndex((int)(len + 4));
            bucket_handle = sl_hash[bucket].handle;
            old_head = *head;
            sl_hash[bucket_handle].link = (uint16)((old_head & 0x3fff) | (sl_hash[bucket_handle].link & 0xc000));
            sl_hash[old_head & 0x3fff].handle = bucket_handle;
        } else {
            old_head = *head;
            cursor = old_head & 0x3fff;
            while ((sl_hash[cursor].link & 0x3fff) != bucket) {
                cursor = sl_hash[cursor].link & 0x3fff;
            }
            free_idx = sl_hash[0].link;
            if (free_idx == 0) {
                Scr_DumpScriptThreads();
                Scr_NoOp();
                Com_Error(1, "\025exceeded maximum number of script strings\n");
            }
            result = (uint16)MT_AllocIndex((int)(len + 4));
            next_free = sl_hash[free_idx].link & 0x3fff;
            sl_hash[0].link = (uint16)next_free;
            sl_hash[next_free].handle = 0;
            sl_hash[cursor].link = (uint16)(free_head0 | (sl_hash[cursor].link & 0xc000));
            sl_hash[free_idx].link = (uint16)((old_head & 0x3fff) | 0x4000);
            sl_hash[free_idx].handle = sl_hash[bucket].handle;
        }
        *head = (uint16)(bucket | 0x8000);
    }
    sl_hash[bucket].handle = result;
    rec = ScrStr_Record(result);
    memcpy(rec->text, text, len);
    rec->flags = (uint8)flags;
    rec->refcount = 1;
    rec->length = (uint8)len;
    return result;
}

// 0x080798e0  size=51  FUN_080798e0  status=translated  naming=inferred
// naming: interns a C string (length strlen+1) with the given flags and extra argument.
uint32 ScrStr_InternCStringEx(const char *text, uint32 flags, uint32 extra)
{
    int len;

    len = ScrStr_Strlen(text);
    return ScrStr_Intern(text, flags, (uint32)(len + 1), extra);
}

// 0x08079914  size=34  FUN_08079914  status=translated  naming=inferred
// naming: interns a C string with the default extra argument 6.
uint32 ScrStr_InternCString(const char *text, uint32 flags)
{
    return ScrStr_InternCStringEx(text, flags, 6);
}

// 0x08079936  size=183  FUN_08079936  status=translated  naming=inferred
// naming: format "max string length exceeded: \"%s\"" (observed); interns the lowercase bytes (no NUL) of a string of len bytes.
uint32 ScrStr_InternLowerBytes(const char *text, uint32 flags, uint32 len, uint32 extra)
{
    char lower[0x2004];
    uint32 i;
    uint32 result;

    if (len < 0x2001) {
        for (i = 0; i < len; i++) {
            lower[i] = (char)tolower((int)text[i]);
        }
        result = ScrStr_Intern(lower, flags, len, extra);
    } else {
        Com_Error(1, "max string length exceeded: \"%s\"", text);
        result = 0;
    }
    return result;
}

// 0x080799ee  size=51  FUN_080799ee  status=translated  naming=inferred
// naming: lowercase intern of a C string (length strlen+1) with an extra argument.
uint32 ScrStr_InternLowerCStringEx(const char *text, uint32 flags, uint32 extra)
{
    int32 n;

    n = ScrStr_Strlen(text) + 1;
    return ScrStr_InternLowerBytes(text, flags, (uint32)n, extra);
}

// 0x08079a22  size=34  FUN_08079a22  status=translated  naming=inferred
// naming: lowercase intern of a C string with the default extra argument 6.
uint32 ScrStr_InternLowerCString(const char *text, uint32 flags)
{
    return ScrStr_InternLowerCStringEx(text, flags, 6);
}

// 0x08079a44  size=228  FUN_08079a44  status=translated  naming=inferred
// naming: re-interns an existing handle's text in lowercase, releasing the old handle; returns the handle unchanged when the text is too long.
uint32 ScrStr_ReinternLower(uint32 handle, uint32 flags, uint32 extra)
{
    int nul;
    uint32 n;
    const char *src;
    char lower[0x2004];
    uint32 i;
    uint32 result;

    nul = ScrStr_NulIndexOfHandle(handle);
    n = (uint32)nul + 1;
    if (n < 0x2001) {
        src = ScrStr_TextFromHandle(handle);
        for (i = 0; i < n; i++) {
            lower[i] = (char)tolower((int)src[i]);
        }
        result = ScrStr_Intern(lower, flags, n, extra);
        ScrStr_Release(handle);
    } else {
        result = handle;
    }
    return result;
}

// 0x08079b28  size=69  FUN_08079b28  status=translated  naming=inferred
// naming: sets a flag bit when unset, otherwise decrements the reference count.
void ScrStr_SetFlagOrDecRef(uint32 handle, uint8 flag)
{
    scr_string_s *rec;

    rec = ScrStr_Record(handle);
    if ((rec->flags & flag) == 0) {
        rec->flags = (uint8)(rec->flags | flag);
    } else {
        rec->refcount--;
    }
}

// 0x08079b6e  size=37  FUN_08079b6e  status=translated  naming=inferred
// naming: increments a record's reference count.
void ScrStr_AddRef(uint32 handle)
{
    scr_string_s *rec;

    rec = ScrStr_Record(handle);
    rec->refcount++;
}

// 0x08079b94  size=350  FUN_08079b94  status=translated  naming=inferred
// naming: removes a node from its hash bucket chain, frees its MT block and returns the node to the free list.
void ScrStr_FreeNode(uint32 handle, scr_string_s *rec, int bytes)
{
    uint32 bucket;
    uint32 cur;
    uint32 prev;
    uint16 *head;
    uint16 *node;

    bucket = (uint32)ScrStr_Hash((const char *)rec + 4, bytes);
    head = &sl_hash[bucket].link;
    MT_FreeIndex(handle, bytes + 4);
    cur = *head & 0x3fff;
    node = &sl_hash[cur].link;
    if (sl_hash[bucket].handle == handle) {
        if (node == head) {
            cur = bucket;
            node = head;
        } else {
            *head = (uint16)((*node & 0x3fff) | 0x8000);
            sl_hash[bucket].handle = sl_hash[cur].handle;
            sl_last_link_ptr = head;
        }
    } else {
        prev = bucket;
        while (sl_hash[cur].handle != handle) {
            prev = cur;
            cur = *node & 0x3fff;
            node = &sl_hash[cur].link;
        }
        sl_hash[prev].link = (uint16)((*node & 0x3fff) | (sl_hash[prev].link & 0xc000));
    }
    *node = sl_hash[0].link;
    sl_hash[cur].handle = 0;
    sl_hash[sl_hash[0].link].handle = (uint16)(int16)cur;
    sl_hash[0].link = (uint16)cur;
}

// 0x08079cf2  size=55  FUN_08079cf2  status=translated  naming=inferred
// naming: releases one reference to a pool handle, freeing the node when the count reaches zero.
void ScrStr_Release(uint32 handle)
{
    scr_string_s *rec;
    int nul;

    rec = ScrStr_Record(handle);
    nul = (int)ScrStr_FindNulIndex(rec);
    ScrStr_DecRefOrFree(handle, nul + 1);
}

// 0x08079d2a  size=107  FUN_08079d2a  status=translated  naming=inferred
// naming: permanent (flag 4) records are reset to one reference; others are cleared and freed.
void ScrStr_ResetOrFree(uint32 handle)
{
    scr_string_s *rec;
    int nul;

    rec = ScrStr_Record(handle);
    if ((rec->flags & 4) == 0) {
        rec->refcount = 0;
        rec->flags = 0;
        nul = (int)ScrStr_FindNulIndex(rec);
        ScrStr_FreeNode(handle, rec, nul + 1);
    } else {
        rec->refcount = 1;
        rec->flags = 4;
    }
}

// 0x08079d96  size=74  FUN_08079d96  status=translated  naming=inferred
// naming: decrements a record's reference count and frees the node when it reaches zero.
void ScrStr_DecRefOrFree(uint32 handle, int bytes)
{
    scr_string_s *rec;

    rec = ScrStr_Record(handle);
    rec->refcount--;
    if (rec->refcount == 0) {
        ScrStr_FreeNode(handle, rec, bytes);
    }
}

// 0x08079de0  size=57  FUN_08079de0  status=translated  naming=inferred
// naming: assigns a pool handle to a 16-bit slot, adding a reference to the new handle and releasing the old one.
void ScrStr_AssignRef(uint16 *slot, uint32 handle)
{
    if (handle != 0) {
        ScrStr_AddRef(handle);
    }
    if (*slot != 0) {
        ScrStr_Release((uint32)*slot);
    }
    *slot = (uint16)handle;
}

// 0x08079e58  size=27  FUN_08079e58  status=translated  naming=inferred
// naming: interns a C string with flag 1.
uint32 ScrStr_InternCStringFlag1(const char *text)
{
    return ScrStr_InternCString(text, 1);
}

// 0x08079e74  size=70  FUN_08079e74  status=translated  naming=inferred
// naming: format "%g" (observed in rodata neighbourhood of the call); interns the float's text with extra argument 14.
int ScrStr_InternFloat(float value)
{
    char buf[136];

    sprintf(buf, "%g", (double)value);
    /* EAX of the tail call is the result (callers 0x0807f... use the string id). */
    return ScrStr_InternCStringEx(buf, 0, 0xe);
}
