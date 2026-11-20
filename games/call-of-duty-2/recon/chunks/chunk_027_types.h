/*
 * chunk 027 (0x080ab702..0x080ae5d0): hunk memory, user-entry registry, small
 * memory pool, Com_Filter, sound alias parsing/lookup (universal/com_memory.cpp
 * and the sound-alias loader).
 */
#ifndef COD2_CHUNK_027_TYPES_H
#define COD2_CHUNK_027_TYPES_H


/* ---- Hunk user-entry registry (hash buckets at hunk_userEntryTable) ---- */
/* Allocated with strlen(name)+10 bytes; name starts at +9 (observed from the
 * strcpy target in Hunk_AddUserEntry). Anonymous entries are 9 bytes. */
struct hunk_user_t {
    int value;                  /* 0x00 handle/pointer released by type */
    struct hunk_user_t *next;   /* 0x04 */
    unsigned char type;         /* 0x08 3,4,5,6 are released by Hunk_ReleaseEntriesInRange */
    char name[1];               /* 0x09 NUL terminated, compared case-insensitively */
};
COD2_ASSERT_OFFSET(hunk_user_t, value, 0x00);
COD2_ASSERT_OFFSET(hunk_user_t, next, 0x04);
COD2_ASSERT_OFFSET(hunk_user_t, type, 0x08);
COD2_ASSERT_OFFSET(hunk_user_t, name, 0x09);

typedef void *(*hunk_alloc_fn_t)(int size);
typedef int (*com_strcmp_fn_t)(const char *a, const char *b);
typedef void (*hunk_entry_cb_t)(int value, int arg);

/* ---- qtime (9 ints written by Com_RealTime) ---- */
struct com_qtime_t {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
COD2_ASSERT_SIZE(com_qtime_t, 0x24);

/* ---- runtime sound alias structures ---- */
/* Runtime alias entry: stride 0x44. Only sequence and probability are used here. */
struct snd_alias_t {
    unsigned char unk00[0x10];
    int sequence;               /* 0x10 */
    unsigned char unk14[0x20];
    float probability;          /* 0x34 */
    unsigned char unk38[0x0c];
};
COD2_ASSERT_OFFSET(snd_alias_t, sequence, 0x10);
COD2_ASSERT_OFFSET(snd_alias_t, probability, 0x34);
COD2_ASSERT_SIZE(snd_alias_t, 0x44);

/* Hash-bucket element (g_soundAliasHash[0x400]). */
struct snd_alias_list_t {
    const char *aliasName;      /* 0x00 */
    struct snd_alias_t *head;   /* 0x04 array of count aliases */
    int count;                  /* 0x08 */
    struct snd_alias_list_t *next; /* 0x0c hash chain */
};
COD2_ASSERT_OFFSET(snd_alias_list_t, head, 0x04);
COD2_ASSERT_OFFSET(snd_alias_list_t, count, 0x08);
COD2_ASSERT_OFFSET(snd_alias_list_t, next, 0x0c);
COD2_ASSERT_SIZE(snd_alias_list_t, 0x10);

/* Some owner structure holding a pointer to its alias list at +0x0c. */
struct snd_alias_holder_t {
    int unk0;
    int unk4;
    int unk8;
    struct snd_alias_list_t *aliasList; /* 0x0c */
};
COD2_ASSERT_OFFSET(snd_alias_holder_t, aliasList, 0x0c);

/* Falloff curve slot (16 slots of 0x48 at g_soundFalloffCurves; slot 0 is the default). */
struct snd_curve_t {
    unsigned char data[0x48];
};
COD2_ASSERT_SIZE(snd_curve_t, 0x48);

/* Alias bank descriptor (3 of 0x10 at g_soundAliasBanks); only words 1 and 2 are touched here. */
struct snd_alias_bank_t {
    int word0;
    int word1;
    int word2;
    int word3;
};
COD2_ASSERT_SIZE(snd_alias_bank_t, 0x10);
struct snd_alias_bank_aux_t {
    int word0;
    int word1;
};
COD2_ASSERT_SIZE(snd_alias_bank_aux_t, 0x08);

/* Volume mod group (32 of 0x44 at g_volumeModGroups). */
struct snd_volmod_group_t {
    char name[0x40];            /* 0x00 */
    float volume;               /* 0x40 */
};
COD2_ASSERT_OFFSET(snd_volmod_group_t, volume, 0x40);
COD2_ASSERT_SIZE(snd_volmod_group_t, 0x44);

/* Alias being built from one csv row (hunk temp allocation, 0x154 bytes). */
struct snd_alias_build_t {
    char fileName[0x40];        /* 0x000 csv file the row came from (strcpy of 2nd init arg) */
    char name[0x40];            /* 0x040 'name' column */
    char secondaryName[0x40];   /* 0x080 'secondaryaliasname' column */
    char *subtitle;             /* 0x0c0 'subtitle' column, temp-hunk copy */
    int sequence;               /* 0x0c4 'sequence' column */
    char soundFile[0x40];       /* 0x0c8 'file' column */
    int unk108;                 /* 0x108 never written here */
    float volMin;               /* 0x10c */
    float volMax;               /* 0x110 */
    float volMod;               /* 0x114 */
    float pitchMin;             /* 0x118 */
    float pitchMax;             /* 0x11c */
    float distMin;              /* 0x120 */
    float distMax;              /* 0x124 */
    int channel;                /* 0x128 index into channel name table */
    int type;                   /* 0x12c 1 = loaded, 2 = streamed/primed */
    struct snd_curve_t *volumeFalloffCurve; /* 0x130 */
    float slavePercentage;      /* 0x134 */
    float probability;          /* 0x138 */
    float lfePercentage;        /* 0x13c */
    int startDelay;             /* 0x140 */
    unsigned char looping;      /* 0x144 */
    unsigned char isMaster;     /* 0x145 */
    unsigned char isSlave;      /* 0x146 */
    unsigned char fullDryLevel; /* 0x147 */
    unsigned char noWetLevel;   /* 0x148 */
    unsigned char hasError;     /* 0x149 */
    unsigned char loadSpecPass; /* 0x14a */
    unsigned char unk14b;       /* 0x14b */
    int unk14c;                 /* 0x14c */
    struct snd_alias_build_t *next; /* 0x150 build list link */
};
COD2_ASSERT_OFFSET(snd_alias_build_t, name, 0x40);
COD2_ASSERT_OFFSET(snd_alias_build_t, secondaryName, 0x80);
COD2_ASSERT_OFFSET(snd_alias_build_t, subtitle, 0xc0);
COD2_ASSERT_OFFSET(snd_alias_build_t, sequence, 0xc4);
COD2_ASSERT_OFFSET(snd_alias_build_t, soundFile, 0xc8);
COD2_ASSERT_OFFSET(snd_alias_build_t, volMin, 0x10c);
COD2_ASSERT_OFFSET(snd_alias_build_t, distMax, 0x124);
COD2_ASSERT_OFFSET(snd_alias_build_t, channel, 0x128);
COD2_ASSERT_OFFSET(snd_alias_build_t, volumeFalloffCurve, 0x130);
COD2_ASSERT_OFFSET(snd_alias_build_t, slavePercentage, 0x134);
COD2_ASSERT_OFFSET(snd_alias_build_t, startDelay, 0x140);
COD2_ASSERT_OFFSET(snd_alias_build_t, looping, 0x144);
COD2_ASSERT_OFFSET(snd_alias_build_t, hasError, 0x149);
COD2_ASSERT_OFFSET(snd_alias_build_t, loadSpecPass, 0x14a);
COD2_ASSERT_OFFSET(snd_alias_build_t, next, 0x150);
COD2_ASSERT_SIZE(snd_alias_build_t, 0x154);

/* csv column indices (index into g_sndAliasColumnNames and the per-row "seen" byte array). */
enum snd_alias_column_t {
    SNDCOL_NONE = 0,
    SNDCOL_NAME = 1,
    SNDCOL_SEQUENCE = 2,
    SNDCOL_FILE = 3,
    SNDCOL_SUBTITLE = 4,
    SNDCOL_VOL_MIN = 5,
    SNDCOL_VOL_MAX = 6,
    SNDCOL_VOL_MOD = 7,
    SNDCOL_PITCH_MIN = 8,
    SNDCOL_PITCH_MAX = 9,
    SNDCOL_DIST_MIN = 10,
    SNDCOL_DIST_MAX = 11,
    SNDCOL_CHANNEL = 12,
    SNDCOL_TYPE = 13,
    SNDCOL_LOOP = 14,
    SNDCOL_PROBABILITY = 15,
    SNDCOL_LOADSPEC = 16,
    SNDCOL_MASTERSLAVE = 17,
    SNDCOL_SECONDARY_ALIAS = 18,
    SNDCOL_VOLUME_FALLOFF_CURVE = 19,
    SNDCOL_START_DELAY = 20,
    SNDCOL_SPEAKERMAP = 21,
    SNDCOL_REVERB = 22,
    SNDCOL_LFE_PERCENTAGE = 23
};

#endif /* COD2_CHUNK_027_TYPES_H */
