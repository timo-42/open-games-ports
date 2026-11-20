/*
 * chunk 106 (0x08114bb6..0x08119f22) types, globals and out-of-chunk prototypes.
 * Script-builtin glue (GScr_*) for fx, fog, shellshock, team/winner strings, turret
 * limits, model bone queries, map/exitlevel commands, mover scripting and entity
 * helpers that the chunk touches. The 72 functions were recovered from .eh_frame
 * FDE starts; layouts are taken from consistent offsets in this chunk and agree
 * with chunk 050 where both touch the same field.
 */
#ifndef COD2_CHUNK_106_TYPES_H
#define COD2_CHUNK_106_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

struct XModelRef;
struct XModel;

/* Local copies of trace_s / gclient_s / gentity_s / turret_s / client slot were replaced by the
 * canonical structs in cod2_canon.h; globals come from cod2_globals.h (see canonical-migration.json). */

/* turret_s is only forward-declared canonically; partial view limited to the angle-limit
 * slots the script setters touch (names follow chunk 049's full 0x44-byte layout). */
struct turret_s {
    byte pad_00[0x0c];
    float arcTop;               /* 0x0c: set to min(0, -value) by the script setter */
    float arcRight;             /* 0x10: set to min(0, -value) by the script setter */
    float arcBottom;            /* 0x14: set to max(0, value) by the script setter */
    float arcLeft;              /* 0x18: set to max(0, value) by the script setter */
};
COD2_ASSERT_OFFSET(turret_s, arcTop, 0x0c);
COD2_ASSERT_OFFSET(turret_s, arcLeft, 0x18);

/* Chunk-private view of the globals whose canonical extern type differs from the use here. */
#define G_SCRIPT_FILE_BUF(i) (((char **)g_0864f98c)[(i)])
#define G_LEVEL_RECORD ((int *)g_087a2408)

/* ---- Helpers that wrap shared prototypes with compatible C++98 parameter types ---- */
static inline uint32 Float_Bits(float f)
{
    union { float f; uint32 u; } v;
    v.f = f;
    return v.u;
}

static inline void Vec3_CopyF(const float *src, float *dst)
{
    Vec3_Copy_08118698((const uint32 *)(size_t)src, (uint32 *)(size_t)dst);
}

static inline void Vec3_Set_0805528e(float *v, uint32 x, uint32 y, uint32 z)
{
    Vec3_Set_08118674((uint32 *)v, x, y, z);
}

static inline void Scr_GetVec(uint32 param, float *dst)
{
    Scr_GetVector(param, dst);
}

static inline void Scr_AddVec(const float *v)
{
    Scr_AddVector(v);
}

static inline void Scr_AddFloat(float f)
{
    Scr_AddFloatBits(Float_Bits(f));
}

#endif
