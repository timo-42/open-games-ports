/*
 * Chunk 028 types: sound alias loading, sndcurve parsing, subtitle localization
 * and the dvar value/domain helpers of cod2_lnxded 1.3.
 *
 * Layouts are recovered from consistent field offsets in the pseudocode and
 * disassembly of functions 0x080ae900..0x080b245c. Fields whose meaning is not
 * pinned down are named by their offset and listed in the chunk symbol JSON
 * open_questions. Out-of-chunk callees use the placeholder names fn_08xxxxxx
 * until the merge lane supplies merged names.
 */
#ifndef CHUNK_028_TYPES_H
#define CHUNK_028_TYPES_H

#include <stdint.h>
#include <stdio.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"

/* ---------------------------------------------------------------------------
 * Sound alias parse entry. One row of a soundaliases/<name>.csv file, linked by
 * `next`. Column ids follow the column-name table g_sndAliasColumnNames.
 * Size is the highest observed extent (next pointer at 0x150).
 * ------------------------------------------------------------------------- */
struct snd_file_s;

struct snd_alias_build_t {
    char sourceFile[0x40];            /* 0x000 source alias file path (dup key, diagnostics) */
    char name[0x40];                  /* 0x040 alias name, column 1 "name" */
    char secondaryAliasName[0x40];    /* 0x080 column 18 "secondaryaliasname" (inline) */
    const char *subtitle;             /* 0x0c0 column 4 "subtitle" (pool string or NULL) */
    int32 sequence;                   /* 0x0c4 column 2 "sequence"; tie-breaker in merge sort */
    char soundFileName[0x40];         /* 0x0c8 column 3 "file" */
    struct snd_file_s *soundFile;     /* 0x108 sound file record assigned by Com_MakeSoundAliasesPermanent */
    uint32 volMin;                    /* 0x10c column 5 "vol_min" (raw 32-bit word) */
    uint32 volMax;                    /* 0x110 column 6 "vol_max" */
    uint32 volMod;                    /* 0x114 column 7 "vol_mod" (not copied to runtime alias) */
    uint32 pitchMin;                  /* 0x118 column 8 "pitch_min" */
    uint32 pitchMax;                  /* 0x11c column 9 "pitch_max" */
    uint32 distMin;                   /* 0x120 column 10 "dist_min" */
    uint32 distMax;                   /* 0x124 column 11 "dist_max" */
    uint32 channel;                   /* 0x128 column 12 "channel" (packed as 4 bits) */
    uint32 loadType;                  /* 0x12c column 13 "type" (2 streamed, 3 primed, else loaded) */
    uint32 probability;               /* 0x130 column 15 "probability" */
    uint32 loadSpec;                  /* 0x134 column 16 "loadspec" (unverified) */
    uint32 masterSlave;               /* 0x138 column 17 "masterslave" (unverified) */
    uint32 falloffCurveRef;           /* 0x13c column 19 "volumefalloffcurve" (curve name pointer, unverified) */
    uint32 startDelay;                /* 0x140 column 20 "startdelay" (unverified) */
    uint8 flag144;                    /* 0x144 bool; bit 0 of runtime packedBits */
    uint8 flag145;                    /* 0x145 bool; bit 1 */
    uint8 flag146;                    /* 0x146 bool; bit 2 */
    uint8 flag147;                    /* 0x147 bool; bit 3 */
    uint8 flag148;                    /* 0x148 bool; bit 4 */
    uint8 reserved149[3];             /* 0x149 padding */
    struct snd_alias_build_t *dupOf;  /* 0x14c earlier entry sharing the same file (and type) */
    struct snd_alias_build_t *next;   /* 0x150 list link */
};

COD2_ASSERT_OFFSET(struct snd_alias_build_t, sourceFile, 0x000);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, name, 0x040);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, secondaryAliasName, 0x080);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, subtitle, 0x0c0);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, sequence, 0x0c4);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, soundFileName, 0x0c8);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, soundFile, 0x108);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, volMin, 0x10c);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, volMax, 0x110);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, volMod, 0x114);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, pitchMin, 0x118);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, pitchMax, 0x11c);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, distMin, 0x120);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, distMax, 0x124);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, channel, 0x128);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, loadType, 0x12c);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, probability, 0x130);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, loadSpec, 0x134);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, masterSlave, 0x138);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, falloffCurveRef, 0x13c);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, startDelay, 0x140);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, flag144, 0x144);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, dupOf, 0x14c);
COD2_ASSERT_OFFSET(struct snd_alias_build_t, next, 0x150);
COD2_ASSERT_SIZE(struct snd_alias_build_t, 0x154);

/* ---------------------------------------------------------------------------
 * Runtime sound file record (16 bytes). Array allocated per alias set.
 * ------------------------------------------------------------------------- */
struct snd_file_s {
    const char *name;        /* 0x00 sound file name (pool copy) */
    uint32 useCount;         /* 0x04 zeroed on creation */
    uint32 reserved08;       /* 0x08 not written by the code in this chunk */
    uint32 loadType;         /* 0x0c load type copied from the first alias using the file */
};

COD2_ASSERT_OFFSET(struct snd_file_s, name, 0x00);
COD2_ASSERT_OFFSET(struct snd_file_s, useCount, 0x04);
COD2_ASSERT_OFFSET(struct snd_file_s, loadType, 0x0c);
COD2_ASSERT_SIZE(struct snd_file_s, 0x10);

/* ---------------------------------------------------------------------------
 * Runtime sound alias (68 bytes = 17 words).
 * ------------------------------------------------------------------------- */
struct snd_alias_t {
    const char *name;               /* 0x00 */
    const char *subtitle;           /* 0x04 */
    char *secondaryAliasName;       /* 0x08 heap copy or NULL */
    struct snd_file_s *soundFile;   /* 0x0c */
    uint32 reserved10;              /* 0x10 always zero */
    uint32 volMin;                  /* 0x14 */
    uint32 volMax;                  /* 0x18 */
    uint32 pitchMin;                /* 0x1c */
    uint32 pitchMax;                /* 0x20 */
    uint32 distMin;                 /* 0x24 */
    uint32 distMax;                 /* 0x28 */
    uint32 packedBits;              /* 0x2c bits 0-4 flags, 5-6 loadType, 7-10 channel */
    uint32 loadSpec;                /* 0x30 */
    uint32 masterSlave;             /* 0x34 */
    uint32 falloffCurveRef;         /* 0x38 */
    uint32 startDelay;              /* 0x3c */
    uint32 probability;             /* 0x40 */
};

COD2_ASSERT_OFFSET(struct snd_alias_t, name, 0x00);
COD2_ASSERT_OFFSET(struct snd_alias_t, subtitle, 0x04);
COD2_ASSERT_OFFSET(struct snd_alias_t, secondaryAliasName, 0x08);
COD2_ASSERT_OFFSET(struct snd_alias_t, soundFile, 0x0c);
COD2_ASSERT_OFFSET(struct snd_alias_t, volMin, 0x14);
COD2_ASSERT_OFFSET(struct snd_alias_t, distMax, 0x28);
COD2_ASSERT_OFFSET(struct snd_alias_t, packedBits, 0x2c);
COD2_ASSERT_OFFSET(struct snd_alias_t, probability, 0x40);
COD2_ASSERT_SIZE(struct snd_alias_t, 0x44);

/* Per-name list node (16 bytes): name, first alias of the name, alias count. */
struct snd_alias_list_t {
    const char *name;                /* 0x00 */
    struct snd_alias_t *aliases;     /* 0x04 first alias with this name */
    int32 aliasCount;                /* 0x08 */
    uint32 reserved0c;               /* 0x0c */
};

COD2_ASSERT_SIZE(struct snd_alias_list_t, 0x10);

/* Destination set passed as param_1 of Com_MakeSoundAliasesPermanent. */
struct snd_alias_bank_t {
    uint32 reserved00;               /* 0x00 */
    struct snd_alias_t *aliases;     /* 0x04 */
    int32 aliasCount;                /* 0x08 */
};

COD2_ASSERT_OFFSET(struct snd_alias_bank_t, aliases, 0x04);
COD2_ASSERT_OFFSET(struct snd_alias_bank_t, aliasCount, 0x08);

/* Destination set passed as param_2 of Com_MakeSoundAliasesPermanent. */
struct snd_file_set_s {
    int32 fileCount;                 /* 0x00 */
    struct snd_file_s *files;        /* 0x04 */
};

COD2_ASSERT_OFFSET(struct snd_file_set_s, files, 0x04);
COD2_ASSERT_SIZE(struct snd_file_set_s, 0x08);

/* ---------------------------------------------------------------------------
 * sndcurve: name, knot count, up to 8 (x, y) knots in [0, 1].
 * ------------------------------------------------------------------------- */
struct snd_curve_knot_s {
    float x;
    float y;
};

struct snd_curve_t {
    const char *name;                       /* 0x00 */
    int32 knotCount;                        /* 0x04 */
    struct snd_curve_knot_s knots[8];       /* 0x08 */
};

COD2_ASSERT_OFFSET(struct snd_curve_t, knotCount, 0x04);
COD2_ASSERT_OFFSET(struct snd_curve_t, knots, 0x08);
COD2_ASSERT_SIZE(struct snd_curve_t, 0x48);

/* Default curve initializer target: name, knot count, two knots (see SndCurve_InitDefault). */
struct snd_range_default_s {
    const char *name;
    int32 mode;                             /* observed value 2 */
    uint32 reserved08;
    float x0;
    float y0;
    float x1;
    float y1;
};

/* Chunk 029 helpers (suffixed names); declared until cod2_prototypes.h is regenerated with them. */
void Vec2_Clear_080b5744(float *v);
void Vec3_Clear_080b57e6(float *v);
float Float_Min_080b56ea(float a, float b);
float Float_Max_080b56c6(float a, float b);
int Float_RoundToInt_080b570e(float v);

/* By-value DvarValue/DvarLimits functions (hidden aggregate-return pointer, callee pops it). */
union DvarValue Dvar_ParseValueWord(uint8 type, union DvarLimits domain, const char *text);
union DvarValue Dvar_ClampValue(uint8 type, uint32 value, uint32 fallback, union DvarLimits limits);

#endif /* CHUNK_028_TYPES_H */
