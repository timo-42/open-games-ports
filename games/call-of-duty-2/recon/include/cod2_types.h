/*
 * Shared types for the Call of Duty 2 Linux dedicated server 1.3 reconstruction.
 * Target: cod2_lnxded SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7
 * ABI: ELF32 i386, System V cdecl, GCC 3.3.6 (g++), 4-byte pointers.
 *
 * Owned by the merge-queue lane. Workers propose types in their chunk header
 * (chunks/chunk_NNN_types.h); the merge lane promotes types used by several
 * chunks here once their layouts agree.
 */
#ifndef COD2_TYPES_H
#define COD2_TYPES_H

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

typedef unsigned char byte;
typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef int qboolean;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

/* Compile-time layout checks usable in C++98: COD2_ASSERT_SIZE(client_s, 0x...) */
#define COD2_CAT2(a, b) a##b
#define COD2_CAT(a, b) COD2_CAT2(a, b)
#define COD2_STATIC_ASSERT(cond) typedef char COD2_CAT(cod2_static_assert_, __LINE__)[(cond) ? 1 : -1]
#define COD2_ASSERT_SIZE(type, size) COD2_STATIC_ASSERT(sizeof(type) == (size))
#define COD2_ASSERT_OFFSET(type, field, off) COD2_STATIC_ASSERT(offsetof(type, field) == (off))

/* ---- Promoted by the merge lane (identical layout in >= 2 finished chunks) ---- */
typedef struct cplane_s {
    float normal[3];   /* 0x00 */
    float dist;        /* 0x0c */
    byte type;         /* 0x10 */
    byte signbits;     /* 0x11 */
    byte pad[2];       /* 0x12 */
} cplane_t;
COD2_ASSERT_SIZE(cplane_t, 0x14);


typedef struct sysEvent_s {
    int evTime;        /* 0x00 */
    int evType;        /* 0x04 */
    int evValue;       /* 0x08 */
    int evValue2;      /* 0x0c */
    int evPtrLength;   /* 0x10 */
    void *evPtr;       /* 0x14 */
} sysEvent_t;
COD2_ASSERT_SIZE(sysEvent_t, 0x18);

typedef struct field_s {
    int cursor;        /* 0x00 */
    int scroll;        /* 0x04 */
    int widthInChars;  /* 0x08 */
    int unk0c;         /* 0x0c */
    int unk10;         /* 0x10 */
    int unk14;         /* 0x14 */
    char buffer[256];  /* 0x18 */
} field_t;
COD2_ASSERT_SIZE(field_t, 0x118);

typedef struct trajectory_s {
    int trType;        /* 0x00 */
    int trTime;        /* 0x04 */
    int trDuration;    /* 0x08 */
    vec3_t trBase;     /* 0x0c */
    vec3_t trDelta;    /* 0x18 */
} trajectory_t;
COD2_ASSERT_SIZE(trajectory_t, 0x24);

/* zlib 1.x */
typedef void *(*zalloc_fn)(void *opaque, uint32 items, uint32 size);
typedef void (*zfree_fn)(void *opaque, void *address);
struct z_stream_s {
    byte *next_in;       /* 0x00 */
    uint32 avail_in;     /* 0x04 */
    uint32 total_in;     /* 0x08 */
    byte *next_out;      /* 0x0c */
    uint32 avail_out;    /* 0x10 */
    uint32 total_out;    /* 0x14 */
    const char *msg;     /* 0x18 */
    void *state;         /* 0x1c */
    zalloc_fn zalloc;    /* 0x20 */
    zfree_fn zfree;      /* 0x24 */
    void *opaque;        /* 0x28 */
    int data_type;       /* 0x2c */
    uint32 adler;        /* 0x30 */
    uint32 reserved;     /* 0x34 */
};
COD2_ASSERT_SIZE(z_stream_s, 0x38);

/* The binary is ELF32; reconstruction must be compiled with -m32. */
COD2_STATIC_ASSERT(sizeof(void *) == 4);

#endif
