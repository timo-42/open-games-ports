/*
 * Chunk 041 (cod2_lnxded 1.3, 0x080dfb76..0x080e3eb0) types, globals and
 * out-of-chunk prototypes. Owned by the chunk worker; the merge lane promotes
 * shared layouts into cod2_types.h.
 *
 * Layouts were recovered from consistent offsets in the pseudocode and checked
 * against the disassembly (x87 int/float conversions, sign-bit constants,
 * trace_t field order). Where a field is only used by this chunk its name is
 * provisional (see the notes in chunk_041.json).
 */
#ifndef COD2_CHUNK_041_TYPES_H
#define COD2_CHUNK_041_TYPES_H

/* Trace dispatch record: 12-byte rows at 0x081869c0. Row index = pmove.traceMode
 * for the trace function; the same row's +8 word is the event-notify hook. */
typedef void (*cod2_pm_trace_fn)(trace_t *trace, const float *start, const float *mins,
                                 const float *maxs, const float *end, int passEntity,
                                 int contentMask);
typedef void (*cod2_pm_notify_fn)(int entityNum, int eventValue);
struct cod2_pm_dispatch_s {
    cod2_pm_trace_fn trace;    /* +0 */
    uint32 field_04;           /* +4 */
    cod2_pm_notify_fn notify;  /* +8 */
};
COD2_ASSERT_SIZE(cod2_pm_dispatch_s, 12);
COD2_ASSERT_OFFSET(cod2_pm_dispatch_s, notify, 8);

/* Stance/interpolation curve row: 12-byte rows terminated by key == -1. */
struct cod2_pm_curve_key_s {
    int32 key;                 /* +0 */
    float value;               /* +4 */
    int32 scalar;              /* +8 */
};
COD2_ASSERT_SIZE(cod2_pm_curve_key_s, 12);

/* Color fade used by LerpColorOverTime: from/to RGBA bytes, start time and duration. */
struct cod2_color_fade_s {
    uint8 pad00[0x20];
    uint8 from[4];             /* 0x20 */
    uint8 to[4];               /* 0x24 */
    int32 startTime;           /* 0x28 */
    int32 duration;            /* 0x2c */
};
COD2_ASSERT_OFFSET(cod2_color_fade_s, from, 0x20);
COD2_ASSERT_OFFSET(cod2_color_fade_s, to, 0x24);
COD2_ASSERT_OFFSET(cod2_color_fade_s, startTime, 0x28);
COD2_ASSERT_OFFSET(cod2_color_fade_s, duration, 0x2c);
typedef struct cod2_color_fade_s cod2_color_fade_t;

#endif
