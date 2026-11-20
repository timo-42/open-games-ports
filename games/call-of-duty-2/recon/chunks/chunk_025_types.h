/*
 * chunk_025 types, extern globals and prototypes.
 * Target: cod2_lnxded 1.3 (ELF32 i386, GCC 3.3.6 g++), chunk 025 0x080a2a10..0x080a7846.
 *
 * Layouts are recovered only from the offsets this chunk touches. Struct sizes are
 * asserted only where the code fixes them (array stride); other sizes are unknown and
 * the record is partially declared with explicit unknown_* padding.
 *
 * Callees outside this chunk use the placeholder names fn_08xxxxxx (merge lane rewrites).
 * Globals without a recovered name use g_08xxxxxx.
 */
#ifndef COD2_CHUNK_025_TYPES_H
#define COD2_CHUNK_025_TYPES_H

#include <sys/stat.h>
#include "cod2_types.h"
#include "cod2_canon.h"

/* ---- Records ------------------------------------------------------------------------- */

/* (x, y) sample of a piecewise-linear table (Curve_EvalPiecewise). */
struct cod2_curve_point_s {
    float x;
    float y;
};
COD2_ASSERT_SIZE(struct cod2_curve_point_s, 8);

#endif
