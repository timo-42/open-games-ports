/*
 * chunk_022 types: server frame / snapshot writer / master heartbeat / net profile
 * (cod2_lnxded 1.3, 0x080960e2 .. 0x0809a408).
 * Link-wave migration: every struct that was local to this chunk is now canonical
 * (cod2_canon.h); externs/prototypes come from cod2_globals.h / cod2_prototypes.h.
 * Only chunk-private declarations remain here.
 */
#ifndef COD2_CHUNK_022_TYPES_H
#define COD2_CHUNK_022_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

/* playerState word indices touched by SV_GetPlayerStateFromCache time rebasing
 * (word == int index into playerState_t; canonical field names not all established). */
enum {
    PS_WORD_TIME_0 = 0x000,
    PS_WORD_TIME_4 = 0x004,
    PS_WORD_ORIGIN = 0x005,       /* vec3 at byte 0x14 (copied by the origin interpolation) */
    PS_WORD_TIME_11 = 0x011,
    PS_WORD_TIME_1C = 0x01c,
    PS_WORD_TIME_3F = 0x03f,
    PS_WORD_TIME_177 = 0x177,
    PS_WORD_TIME_1E9 = 0x1e9,     /* always rebased, even when zero */
    PS_WORD_SLOT_BASE = 0x5d4,    /* 31 slots of 0x20 words */
    PS_SLOT_STRIDE = 0x20,
    PS_SLOT_COUNT = 0x1f,
    PS_SLOT_TIME_A = 0x00,        /* rebased then clamped to svs.time */
    PS_SLOT_TIME_B = 0x08,
    PS_SLOT_TIME_C = 0x0e,
    PS_SLOT_TIME_D = 0x10
};

/* Globals this chunk touches that sit in gaps between symbols.json globals (declared in the
 * chunk JSON "globals" list for the merge lane). */
extern int sv_antilagFrameOffsetA;                  /* 0x0817db80 origin-history window start (frames) */
extern int sv_antilagFrameOffsetB;                  /* 0x0817db84 origin-history window end (frames) */
extern int svc_lastRconTime;                  /* 0x0849fbf4 last rcon time */

#endif /* COD2_CHUNK_022_TYPES_H */
