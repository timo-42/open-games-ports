/*
 * chunk_024 types: filesystem (FS_*) search paths, pack files, file handles.
 * Target: cod2_lnxded 1.3, ELF32 i386, GNU C++98.
 *
 * Link-wave migration: dvar_s, directory_s, iwd_s, searchpath_s and fileHandleData_s are canonical
 * (cod2_canon.h); globals and prototypes come from cod2_globals.h / cod2_prototypes.h.  Only the
 * chunk-private views of the zip reader remain (canonical unz_s / fileInIwd_s are incomplete tags).
 */
#ifndef COD2_CHUNK_024_TYPES_H
#define COD2_CHUNK_024_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

#include <stdio.h>
#include <strings.h>
#include <sys/stat.h>

#define COD2_FS_MAX_HANDLES 64
#define COD2_FS_HANDLE_SIZE 0x11c
#define COD2_FS_PACK_HEADER_SIZE 0x320
#define COD2_FS_MAX_FOUND_FILES 0x1000
#define COD2_FS_MAX_IWD_FILES 0x400
#define COD2_FS_SEEK_CUR 0
#define COD2_FS_SEEK_END 1
#define COD2_FS_SEEK_SET 2

/* One entry of a pack's file list (also the hash-chain node). */
struct fileInIwd_s {
    int filePos;                       /* +0x00 zip central-directory position */
    char *name;                        /* +0x04 normalised entry name */
    struct fileInIwd_s *next;      /* +0x08 next entry in the hash bucket */
};
COD2_ASSERT_OFFSET(struct fileInIwd_s, name, 0x04);
COD2_ASSERT_OFFSET(struct fileInIwd_s, next, 0x08);
COD2_ASSERT_SIZE(struct fileInIwd_s, 0x0c);

/* Per-open pack entry state (zip read object). Copied as 0x80 bytes. */
struct unz_s {
    int firstWord;                     /* +0x00 kept across the state copy */
    unsigned char opaque_04[0x40];     /* +0x04 */
    int fileSize;                      /* +0x44 uncompressed size */
    unsigned char opaque_48[0x38];     /* +0x48 */
};
COD2_ASSERT_OFFSET(struct unz_s, fileSize, 0x44);
COD2_ASSERT_SIZE(struct unz_s, 0x80);

/* Zip central-directory entry info: 14 x uint32 + the 6-word tm_unz date (0x50 bytes). The original
 * FS_LoadPackFile frame reserves 0x50 bytes for it ([ebp-0x178]..[ebp-0x128]) and unzGetCurrentFileInfo
 * (chunk 037) writes the date at +0x38; a 0x38-byte local let the year (+0x4c) overwrite the next local. */
struct unz_file_info_s {
    uint32 version;
    uint32 versionNeeded;
    uint32 flag;
    uint32 compressionMethod;
    uint32 dosDate;
    uint32 crc;                        /* +0x14 */
    uint32 compressedSize;
    uint32 uncompressedSize;           /* +0x1c */
    uint32 sizeFilename;
    uint32 sizeFileExtra;
    uint32 sizeFileComment;
    uint32 diskNumStart;
    uint32 internalFa;
    uint32 externalFa;
    uint32 tmuDate[6];                 /* +0x38 tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year */
};
COD2_ASSERT_OFFSET(struct unz_file_info_s, crc, 0x14);
COD2_ASSERT_OFFSET(struct unz_file_info_s, uncompressedSize, 0x1c);
COD2_ASSERT_OFFSET(struct unz_file_info_s, tmuDate, 0x38);
COD2_ASSERT_SIZE(struct unz_file_info_s, 0x50);

/* Pending shared prototype: FS_ChecksumAllowed (chunk 023) is not yet in cod2_prototypes.h because the
 * old chunk_023.json signature used a chunk-local tag; the migrated JSON declares exactly this. */
int FS_ChecksumAllowed(struct iwd_s *entry);

#endif
