/*
 * chunk_020 types: server client/challenge/ban layouts, netadr, usercmd, gentity views,
 * globals and out-of-chunk callee placeholders for cod2_lnxded 1.3.
 *
 * Layouts are recovered from consistent offsets in the chunk's decompiled code and were
 * spot-checked against the disassembly (SV_UserMove 0x8090e34, SV_SendClientGameState
 * 0x808f302, SV_DirectConnect 0x808e2aa). Fields whose meaning is not proven are named
 * by role only. Out-of-chunk callees use the fn_08xxxxxx placeholder convention with
 * variadic prototypes or prototypes typed by use; the merge lane replaces them.
 */
#ifndef COD2_CHUNK_020_TYPES_H
#define COD2_CHUNK_020_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

/* ---- netadr_t ------------------------------------------------------------
 * Five dwords, passed by value. On i386 cdecl this is the same stack image as five
 * dword arguments. Port is the low half of dword 2. Ip octet 0 is the low byte.
 */
COD2_ASSERT_SIZE(netadr_t, 0x14);
COD2_ASSERT_OFFSET(netadr_t, ip, 0x04);
COD2_ASSERT_OFFSET(netadr_t, port, 0x08);

#define COD2_NA_BOT 0
#define COD2_NA_BAD 1
#define COD2_NA_LOOPBACK 2

/* Client states (observed in "Going from CS_x to CS_y" strings). */
enum {
    CS_FREE = 0,
    CS_ZOMBIE = 1,
    CS_CONNECTED = 2,
    CS_PRIMED = 3,
    CS_ACTIVE = 4
};

/* ---- challenge_s : one of 1024 slots (stride 0x74) at 0x084230dc ---- */

#define COD2_MAX_CHALLENGES 1024

/* ---- tempban_s : 16-slot recent-ban table (stride 8) ---- */

/* ---- usercmd_t : 7-dword client command, 28 bytes ---- */
COD2_ASSERT_SIZE(usercmd_t, 0x1c);
COD2_ASSERT_OFFSET(usercmd_t, weapon, 0x08);

/* ---- dvar_s : value union at +0x08 (string / integer / float views) ---- */

/* ---- client-command table entry (stride 8) at 0x0817d9e0 ---- */
struct client_s;
typedef void (*ucmd_func_t)(struct client_s *cl);
typedef struct ucmd_entry_s {
    const char *name;
    ucmd_func_t func;
} ucmd_entry_s;
COD2_ASSERT_SIZE(ucmd_entry_s, 8);

/* ---- msg_t : bit-buffer handle. Layout from the cursize (+0x0c) and readcount (+0x10)
 * uses in SV_SendClientGameState and SV_ExecuteClientMessage; data at +0x04 is the buffer
 * set by MSG_Init. Tail size is not recovered (kept large enough for the stack frames). */
COD2_ASSERT_OFFSET(msg_t, cursize, 0x0c);
COD2_ASSERT_OFFSET(msg_t, readcount, 0x10);

/* Scratch buffer handle (16 bytes) used with Com_PoolAllocInto2 / Com_PoolPtrFromOffset / Com_PoolRestoreFrom2. */
typedef struct tempbuf_s {
    uint8 raw[16];
} tempbuf_s;

/* ---- svEntity_s : server entity record (stride 0x174), array at 0x08442618 ---- */

/* ---- gentity_s : game entity; stride is runtime (g_gentitySize). Partial layout. ---- */

/* ---- reliable server command ring slot (stride 0x408) ---- */

/* ---- client_s : one slot of svs.clients (stride 0xB1064); canonical layout in cod2_canon.h.
 * Snapshot ring frames[k] (clientSnapshot_t, stride 0x26C4) starts at +0x20E34; messageAcked is
 * +0x234F0 + k * 0x26C4 (SV_UserMove 0x08091038, SV_CalcPings 0x08096476). */

/* ---- globals (unnamed in the binary; g_08xxxxxx placeholders) ---- */



/* ---- out-of-chunk callees (placeholders; merge lane renames) ---- */
void SV_SendMessageToClient(...);

#endif /* COD2_CHUNK_020_TYPES_H */
