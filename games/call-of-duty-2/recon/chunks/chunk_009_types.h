/*
 * chunk_009 types: network message buffer, netchan/loopback/profile records,
 * netfield descriptor tables, cvar prefix, and prototypes for out-of-chunk
 * callees (placeholders fn_08xxxxxx until the merge lane renames them).
 *
 * Target: cod2_lnxded 1.3, ELF32 i386, GNU C++98.
 * Evidence for every layout is listed next to the member.
 */
#ifndef COD2_CHUNK_009_TYPES_H
#define COD2_CHUNK_009_TYPES_H

#include "cod2_types.h"

/* ------------------------------------------------------------------------ */
/* Loopback queue: 16 slots of 0x580 bytes, then get/send counters.          */
struct loopmsg_s {
    byte data[0x578];    /* 0x000 */
    int datalen;         /* 0x578 */
    int source;          /* 0x57c  sock/port tag returned as netadr port */
};
COD2_ASSERT_SIZE(loopmsg_s, 0x580);

struct loopback_s {
    loopmsg_s msgs[16];  /* 0x0000 */
    int get;             /* 0x5800 */
    int send;            /* 0x5804 */
};
COD2_ASSERT_SIZE(loopback_s, 0x5808);

/* ------------------------------------------------------------------------ */
/* Temporary buffer object (0x080ac58e family). Full size not recovered; the chunk's
 * callers use 20-28 byte stack objects, so 32 bytes of opaque storage is reserved here. */
struct netbuf_s {
    unsigned char opaque[32];
};

/* Globals, prototypes and canonical structs (msg_s, netField_s, dvar_s, netadr_s, netchan_s, net profile
 * records) come from cod2_canon.h / cod2_globals.h / cod2_prototypes.h. */

#endif /* COD2_CHUNK_009_TYPES_H */
