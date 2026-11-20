/*
 * chunk_038 types: Linux system layer (tty console, event queue, UDP sockets,
 * file listing, paths, mutexes) of cod2_lnxded 1.3, 0x080d38d8..0x080d6c32.
 * Layouts are recovered from consistent offsets in the disassembly.
 */
#ifndef COD2_CHUNK_038_TYPES_H
#define COD2_CHUNK_038_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include <pthread.h>

struct sockaddr_in;

/* Console input line (Q3 field_t lineage). 0x46 dwords copied by history code. */
/* field_s: promoted to cod2_types.h */
COD2_ASSERT_SIZE(field_t, 0x118);
COD2_ASSERT_OFFSET(field_t, buffer, 0x18);

/* sysEvent_s: promoted to cod2_types.h */
COD2_ASSERT_SIZE(sysEvent_t, 0x18);
COD2_ASSERT_OFFSET(sysEvent_t, evPtr, 0x14);

enum {
    SE_CONSOLE_CODE = 4,
    SE_PACKET_CODE = 5
};

/* i386 glibc 2.3 'struct stat' as filled by __xstat(3, ...) (0x80a3750). */
typedef struct cod2_stat_s {
    uint32 st_dev_lo;    /* 0x00 */
    uint32 st_dev_hi;    /* 0x04 */
    uint32 pad_08;       /* 0x08 */
    uint32 st_ino;       /* 0x0c */
    uint32 st_mode;      /* 0x10 */
    uint32 st_nlink;     /* 0x14 */
    uint32 st_uid;       /* 0x18 */
    uint32 st_gid;       /* 0x1c */
    uint32 st_rdev_lo;   /* 0x20 */
    uint32 st_rdev_hi;   /* 0x24 */
    uint32 pad_28;       /* 0x28 */
    int32 st_size;       /* 0x2c */
    int32 st_blksize;    /* 0x30 */
    int32 st_blocks;     /* 0x34 */
    int32 st_atime_sec;  /* 0x38 */
    uint32 st_atime_nsec;/* 0x3c */
    int32 st_mtime_sec;  /* 0x40 */
    uint32 st_mtime_nsec;/* 0x44 */
    int32 st_ctime_sec;  /* 0x48 */
    uint32 st_ctime_nsec;/* 0x4c */
    uint32 unused4;      /* 0x50 */
    uint32 unused5;      /* 0x54 */
} cod2_stat_t;
COD2_ASSERT_SIZE(cod2_stat_t, 0x58);
COD2_ASSERT_OFFSET(cod2_stat_t, st_mode, 0x10);
COD2_ASSERT_OFFSET(cod2_stat_t, st_mtime_sec, 0x40);

#endif
