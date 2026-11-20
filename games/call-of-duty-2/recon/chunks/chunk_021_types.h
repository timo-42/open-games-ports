/*
 * chunk_021_types.h - reconstruction types, globals and prototypes for
 * chunk 021 of cod2_lnxded 1.3 (0x08092002..0x0809602e, 71 functions).
 *
 * Layouts were recovered from consistent offsets in the Ghidra pseudocode and
 * checked against the disassembly. Fields that this chunk never reads keep
 * "unk" names and are padded so that the known offsets and the total size
 * (where it is known) are exact. Layouts proposed here are for the merge lane
 * to reconcile with other chunks; they are not promoted to cod2_types.h.
 */
#ifndef COD2_CHUNK_021_TYPES_H
#define COD2_CHUNK_021_TYPES_H

#include <strings.h>
#include <ctype.h>

/* ------------------------------------------------------------------------ */
/* Network address: 20 bytes, passed by value as five 32-bit words.         */
/* Port is the low 16 bits of word 2 (confirmed by the SV_VoiceConnectionlessPacket port  */
/* fix-up and the connectionless port match).                               */
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Message buffer. Only the offsets used by this chunk are recovered.        */
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Cvar. The value union sits at +0x08 for every cvar kind used here:        */
/* integer (sv_maxclients), boolean (sv_pure), string (sv_hostname).        */
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Server-side shared entity (gentity). Partial: only the offsets this       */
/* chunk reads are recovered.                                              */
/* ------------------------------------------------------------------------ */

/* Archived entity snapshot record, stride 0x174 inside sv.                 */

/* Queued reliable server command: text at +0, time at +0x400, flags at +0x404. */

/* Client slot, stride 0xb1064 inside svs.clients. */

/* Server static data at 0x08423080, size 0x1d10c (cleared on shutdown). */

/* Server state at 0x08440200, size 0x5f534 (cleared by SV_ResetServerState). */

/* ------------------------------------------------------------------------ */
/* Globals. Names follow the cvar or role they hold; g_xxxxxxxx are unnamed. */
/* ------------------------------------------------------------------------ */
/* sv is not a separate global in the data image: server_s overlays it at g_sv_state (0x08440200, size 0x5f534). */
#define sv (*(server_t *)&g_sv_state)

extern struct dvar_s *cv_081a21a0;              /* 0x081a21a0 */
extern struct dvar_s *cv_081a218c;              /* 0x081a218c (dedicated flag, inferred) */
extern struct dvar_s *cv_084a8780;              /* 0x084a8780 */


/* ------------------------------------------------------------------------ */
/* Out-of-chunk callees. Names are placeholders until symbols.json merges.   */
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Chunk-local prototypes (definitions in chunk_021.cpp, address order).     */
/* ------------------------------------------------------------------------ */

#endif
