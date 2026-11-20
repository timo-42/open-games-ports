/*
 * chunk_007 types, globals and out-of-chunk prototypes.
 * Covers cod2_lnxded 1.3 0x08061ccc..0x08065d9a: Com_Init/Com_Frame, dvar commands
 * (toggle/set/seta/dvardump), DObj slot tables, FS_SV_* and iwd checksum lists.
 * Naming of placeholders follows WORKER_BRIEF (fn_08xxxxxx / g_08xxxxxx); the comment next to
 * each placeholder states the role inferred from this chunk's use (not a merged name).
 */
#ifndef COD2_CHUNK_007_TYPES_H
#define COD2_CHUNK_007_TYPES_H

#include "cod2_types.h"
#include <setjmp.h>

struct DObj_s;                  /* 100 (0x64) bytes per pool slot */

/* Globals defined by the data image (recon/globals/symbol_map.json) that cod2_globals.h does not declare.
 * They sit inside container/array aliases of cod2_globals.h (g_dobjPool, g_dobjServerHandleToSlot) or in gaps. */
extern int g_081a4f8c;                /* pending handle set by Com_SetPendingHandle */
extern int g_081d7880;                /* free DObj slot counter */
extern int g_081d8a00;                /* last allocated DObj slot */
extern int g_081d8a04;                /* DObj system initialised flag */
extern unsigned char g_081d7080[0x800]; /* DObj slot in-use flags */
extern dvar_s *dvar_iterCursor;       /* 0x081daf00 Dvar_GetNext cursor */
extern char g_081d8b00[];             /* Dvar_InfoString buffer (0x400) */
extern char g_081d8f00[];             /* Dvar_InfoString_Big buffer */
extern char g_081e9100[];             /* Com_ShiftString result buffer */
extern char g_081db100[];             /* FS_LoadedIwdChecksums buffer */
extern char g_081dd100[];             /* FS_LoadedIwdNames */
extern char g_081df100[];             /* FS_LoadedIwdPureChecksums */
extern char g_081e1100[];             /* FS_ReferencedIwdChecksums */
extern char g_081e3100[];             /* FS_ReferencedIwdPureChecksums */
extern char g_081e7100[];             /* FS_ReferencedIwdNames */

/* Canonical structs (dvar_s, fileHandleData_s, iwd_s, searchpath_s, field_s), globals and prototypes come from
 * cod2_canon.h / cod2_globals.h / cod2_prototypes.h. */

#endif
