/*
 * Chunk 028 of cod2_lnxded 1.3: 0x080ae900..0x080b245c (58 functions).
 *
 * Covers sound alias file loading and merging, sndcurve parsing, subtitle
 * localization of soundaliases/<name>.csv files, and the dvar value/domain helpers.
 * Behaviour is translated from the binary; decompiler artefacts were resolved
 * against the disassembly (value words, integer sentinel compares, pointer
 * results stored as 4-byte words).
 */
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_028_types.h"

#include <math.h>
#include <strings.h>

/* The shared prototypes take const char ** cursors and void ** buffer out-params; the
 * chunk's own variables are char *, so the calls below go through these argument casts. */
#define Com_Parse(c) Com_Parse((const char **)(c))
#define Com_ParseOnLine(c) Com_ParseOnLine((const char **)(c))
#define Com_SkipRestOfLine(c) Com_SkipRestOfLine((const char **)(c))
#define FS_ReadFile(path, buf) FS_ReadFile((path), (void **)(buf))

/* ---------------------------------------------------------------------------
 * Chunk-local helpers.
 * ------------------------------------------------------------------------- */

/* Dvar value words hold float bit patterns, int values, or pointers. */
typedef union chunk028_word_u {
    uint32 bits;
    float f;
    int32 i;
} chunk028_word_t;

static float w_float(uint32 w)
{
    chunk028_word_t u;
    u.bits = w;
    return u.f;
}

static int32 w_int(uint32 w)
{
    chunk028_word_t u;
    u.bits = w;
    return u.i;
}

static char *w_str(uint32 w)
{
    return (char *)(uintptr_t)w;
}

static void *w_ptr(uint32 w)
{
    return (void *)(uintptr_t)w;
}

/* Frame of Com_LoadSoundAliasFile: the alias callback context followed by two flag bytes. */
struct chunk028_alias_ctx_s {
    uint8 bytes[329];
    uint8 flagA;     /* byte just past the context (local_443) */
    uint8 flagB;     /* next byte (local_442) */
};

static const char *chunk028_fs_base(void)
{
    return fs_basepath->current.string;
}

/* In-chunk prototypes. */
void snd_refreshVolumeModGroups(void);
void Com_LoadSoundAliasFile(const char *group, uint32 param2, const char *fileName);
int SndAlias_CompareName(struct snd_alias_build_t *a, struct snd_alias_build_t *b);
struct snd_alias_build_t *SndAlias_MergeSortList(struct snd_alias_build_t *list, int32 *count,
                                                 int (*cmp)(struct snd_alias_build_t *, struct snd_alias_build_t *),
                                                 uint32 uniq);
int SndAlias_CompareFileAndType(struct snd_alias_build_t *a, struct snd_alias_build_t *b);
void SndAlias_WarnFileTypeMismatch(struct snd_alias_build_t *a, struct snd_alias_build_t *b);
void SndFile_Init(struct snd_file_s *file, const char *name, uint32 loadType);
void Com_AddSoundAlias(struct snd_alias_build_t *entry, struct snd_alias_t *alias, const char *name,
                       struct snd_file_s *soundFile, const char *subtitle);
void Com_MakeSoundAliasesPermanent(struct snd_alias_bank_t *aliasSet, struct snd_file_set_s *fileSet);
int Com_ParseSndCurve(char *text, const char *curveName, struct snd_curve_t *curve);
int Com_LoadSndCurve(const char *curveName, struct snd_curve_t *curve);
void SndCurve_InitDefault(struct snd_curve_t *curve);
int SndSubtitle_HasReference(const char *key);
const char *SndSubtitle_FindEnglishText(const char *key);
void SndSubtitle_WriteReferenceBlock(const char *key, const char *text, int handle);
void SndSubtitle_WriteReference(const char *key, const char *text);
void Com_LocalizeSoundAliasFile(const char *fileName, const char *zone, char *tmpPath);
void COM_WriteFinalStringEdFile(const char *srcPath, const char *dstPath);
float Float_Clamp_080b11e8(float a, float b, float c);
float Float_SelectIfNonNegative(float test, float ifNonNegative, float ifNegative);
void Dvar_SetSetterGate(uint8 value);
uint8 Dvar_GetSvCheatsReady(void);
uint32 generateHashValue(const char *name);
int Com_IsIdentifierName(const char *s);
float *Dvar_ScratchAlloc(int count);
void Dvar_AllocValueBlock(struct dvar_s *dvar);
void Dvar_FreeValueBlock(struct dvar_s *dvar);
const char *Dvar_InternString(const char *s);
void Dvar_ReleasePoolString(const char *s);
const char *Dvar_CanonicalString(const char *s);
void Dvar_ReleaseValueString(const char *s);
void Dvar_ReleaseCurrentValue(struct dvar_s *dvar);
void Dvar_ReleaseLatchedValue(struct dvar_s *dvar);
void Dvar_ReleaseResetValue(struct dvar_s *dvar);
void Dvar_SetCurrentString(struct dvar_s *dvar, const char *s);
void Dvar_SetLatchedString(struct dvar_s *dvar, const char *s);
void Dvar_SetResetString(struct dvar_s *dvar, const char *s);
const char *Dvar_CurrentEnumString(const struct dvar_s *dvar);
const char *Dvar_EnumStringFromIndexText(const struct dvar_s *dvar, const char *text);
const char *Dvar_FormatValue(const struct dvar_s *dvar, uint32 value);
bool Dvar_ParseBool(const char *s);
int Dvar_ParseInt(const char *s);
float Dvar_ParseFloat(const char *s);
float *Dvar_ParseVec2(const char *s);
float *Dvar_ParseVec3(const char *s);
float *Dvar_ParseVec4(const char *s);
int Dvar_ParseEnumIndex(const struct DvarLimitsEnum *desc, const char *s);
void Dvar_ParseColorRGBA(const char *s, uint8 *rgba);
DvarValue Dvar_ParseValueWord(uint8 type, DvarLimits domain, const char *text);
const char *Dvar_FormatCurrentValue(const struct dvar_s *dvar);
const char *Dvar_FormatResetValue(const struct dvar_s *dvar);
const char *Dvar_FormatLatchedValue(const struct dvar_s *dvar);
void Dvar_ClampFloatArray(float *values, int count, float lo, float hi);
int Dvar_AllFloatsInRange(const float *values, int count, float lo, float hi);
DvarValue Dvar_ClampValue(uint8 type, uint32 value, uint32 fallback, DvarLimits limits);
int Dvar_ValueInDomain(uint8 type, uint32 value, uint32 lo, uint32 hi);
void Dvar_DomainVectorText(int dims, float lo, float hi, char *buf, size_t size);
char *Dvar_DomainText(uint8 type, uint32 lo, uint32 hi, char *buf, size_t size, int32 *lineCount);

/* ---------------------------------------------------------------------------
 * 0x080ae900  size=12
 * ------------------------------------------------------------------------- */
// 0x080ae900  size=12  snd_refreshVolumeModGroups  status=translated  naming=observed
// naming: the string 'snd_refreshVolumeModGroups' is registered as this command handler by 0x080ae90c.
void snd_refreshVolumeModGroups(void)
{
    g_sndVolModGroupsDirty = 0;
}

/* ---------------------------------------------------------------------------
 * 0x080ae90c  size=766
 * Loads one soundaliases/<fileName> CSV file. The header row maps columns to
 * column ids; each data row is turned into aliases through Com_ParseAliasColumn.
 * ------------------------------------------------------------------------- */
// 0x080ae90c  size=766  Com_LoadSoundAliasFile  status=translated  naming=inferred
// naming: builds the 'soundaliases/%s' path, registers snd_refreshVolumeModGroups and reports 'Sound alias file %s' errors.
void Com_LoadSoundAliasFile(const char *group, uint32 param2, const char *fileName)
{
    char pathBuf[64];
    struct chunk028_alias_ctx_s aliasCtx;
    int32 columnIds[257];
    uint8 rowFlags[24];
    char *fileText[3];
    char *cursor;
    char *token;
    int32 columnCount = 0;
    int32 index;
    int rc;
    int hasName;
    int hasFile;

    Com_sprintf(pathBuf, 0x40, "soundaliases/%s", fileName);
    rc = FS_ReadFile(pathBuf, fileText);
    if (rc < 0) {
        return;
    }
    if (g_sndVolModGroupsDirty == 0) {
        if (g_sndVolModCmdRegistered == 0) {
            Cmd_AddCommand("snd_refreshVolumeModGroups", snd_refreshVolumeModGroups);
            g_sndVolModCmdRegistered = 1;
        }
        Com_LoadVolumeModGroups(g_volumeModGroups);
        g_sndVolModGroupsDirty = 1;
    }
    Com_BeginParseSession(pathBuf);
    Com_SetCSVMode(1);
    cursor = fileText[0];

    for (;;) {
        /* Skip blank and comment rows. */
        for (;;) {
            token = Com_Parse(&cursor);
            if (cursor == NULL) {
                Com_EndParseSession();
                return;
            }
            if (*token != '\0' && *token != '#') {
                break;
            }
            Com_SkipRestOfLine(&cursor);
        }

        if (columnCount == 0) {
            /* Header row: map each cell to a column id (0 when unknown). */
            hasName = 0;
            hasFile = 0;
            do {
                columnIds[columnCount] = 0;
                for (index = 1; index < 0x18; index++) {
                    if (Q_stricmp(g_sndAliasColumnNames[index], token) == 0) {
                        columnIds[columnCount] = index;
                        if (index == 1) {
                            hasName = 1;
                        } else if (index == 3) {
                            hasFile = 1;
                        }
                        break;
                    }
                }
                columnCount++;
                if (columnCount == 0x100 || cursor == NULL || *cursor == '\n') {
                    break;
                }
                token = Com_ParseOnLine(&cursor);
            } while (1);
            if (!hasName || !hasFile) {
                Com_Printf("^1ERROR: Sound alias file %s: missing 'name' and/or 'file' columns\n", fileName);
                Com_EndParseSession();
                return;
            }
        } else {
            /* Data row: feed every cell to the alias builder. */
            memset(rowFlags, 0, 0x18);
            Com_InitSoundAliasBuild((struct snd_alias_build_t *)aliasCtx.bytes, fileName, group);
            index = 0;
            for (;;) {
                if (*token != '\0') {
                    Com_ParseAliasColumn(group, (const char *)(size_t)param2, fileName, token, columnIds[index], rowFlags,
                                         (struct snd_alias_build_t *)aliasCtx.bytes);
                }
                index++;
                if (index == columnCount) {
                    break;
                }
                token = Com_ParseOnLine(&cursor);
            }
            if (rowFlags[1] == 0 || rowFlags[3] == 0) {
                Com_Printf("^1ERROR: Sound alias file %s: alias entry missing name and/or file\n", fileName);
                Com_EndParseSession();
                return;
            }
            if (aliasCtx.flagB != 0 && aliasCtx.flagA == 0 && Com_ValidateSoundAlias((struct snd_alias_build_t *)aliasCtx.bytes) != 0) {
                Com_AddBuildSoundAlias((struct snd_alias_build_t *)aliasCtx.bytes);
            }
        }
        Com_SkipRestOfLine(&cursor);
    }
}

/* ---------------------------------------------------------------------------
 * 0x080aec0a  size=32
 * ------------------------------------------------------------------------- */
// 0x080aec0a  size=32  SndAlias_CompareName  status=translated  naming=inferred
// naming: compares the alias-name field (+0x40) of two entries; passed as the comparator to the sort at 0x080aec2a.
int SndAlias_CompareName(struct snd_alias_build_t *a, struct snd_alias_build_t *b)
{
    return Q_stricmp(a->name, b->name);
}

/* ---------------------------------------------------------------------------
 * 0x080aec2a  size=596
 * Recursive merge sort over the entry list (linked by `next`). When `uniq` is
 * set, equal entries (same comparator result and same sequence) are checked
 * for duplicates in the same source file.
 * ------------------------------------------------------------------------- */
// 0x080aec2a  size=596  SndAlias_MergeSortList  status=translated  naming=inferred
// naming: recursive merge sort on a count-carrying list; contains the 'duplicate alias' diagnostic.
struct snd_alias_build_t *SndAlias_MergeSortList(struct snd_alias_build_t *list, int32 *count,
                                                 int (*cmp)(struct snd_alias_build_t *, struct snd_alias_build_t *),
                                                 uint32 uniq)
{
    uint8 uniqFlag = (uint8)uniq;
    struct snd_alias_build_t *head = NULL;
    struct snd_alias_build_t **tail;
    struct snd_alias_build_t *left;
    struct snd_alias_build_t *right;
    int32 leftCount;
    int32 rightCount;
    int32 order;
    int32 step;

    if (*count == 1) {
        list->next = NULL;
        return list;
    }
    leftCount = *count / 2;
    rightCount = *count - leftCount;
    right = list;
    for (step = 0; step < leftCount; step++) {
        right = right->next;
    }
    left = SndAlias_MergeSortList(list, &leftCount, cmp, uniq & 0xff);
    right = SndAlias_MergeSortList(right, &rightCount, cmp, uniqFlag);

    *count = 0;
    tail = &head;
    while (leftCount != 0 && rightCount != 0) {
        order = cmp(left, right);
        if (uniqFlag == 0 || order != 0 || (order = (int32)left->sequence - (int32)right->sequence, order != 0)) {
            if (order < 1) {
                *tail = left;
                left = left->next;
                leftCount--;
            } else {
                *tail = right;
                right = right->next;
                rightCount--;
            }
            (*count)++;
            tail = &(*tail)->next;
        } else {
            order = Q_stricmp(left->sourceFile, right->sourceFile);
            if (order == 0) {
                Com_Printf("^1ERROR: sound alias file %s: duplicate alias '%s'\n", left->sourceFile, left->name);
                left = left->next;
                leftCount--;
                right = right->next;
                rightCount--;
            } else if (order < 0) {
                left = left->next;
                leftCount--;
            } else {
                right = right->next;
                rightCount--;
            }
        }
    }
    if (leftCount == 0) {
        *tail = right;
        *count = rightCount + *count;
    } else {
        *tail = left;
        *count = leftCount + *count;
    }
    return head;
}

/* ---------------------------------------------------------------------------
 * 0x080aee7e  size=114
 * ------------------------------------------------------------------------- */
// 0x080aee7e  size=114  SndAlias_CompareFileAndType  status=translated  naming=inferred
// naming: comparator on (sound file name, load type, alias name), used by the second sort in 0x080af224.
int SndAlias_CompareFileAndType(struct snd_alias_build_t *a, struct snd_alias_build_t *b)
{
    int32 result;

    result = Q_stricmp(a->soundFileName, b->soundFileName);
    if (result == 0) {
        result = (int32)a->loadType - (int32)b->loadType;
        if (result == 0) {
            result = SndAlias_CompareName(a, b);
        }
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x080aeef0  size=199
 * ------------------------------------------------------------------------- */
static const char *snd_loadTypeName(uint32 loadType)
{
    int32 t = (int32)loadType;

    if (t == 2) {
        return "streamed";
    }
    if (t == 3) {
        return "primed";
    }
    return "loaded";
}

// 0x080aeef0  size=199  SndAlias_WarnFileTypeMismatch  status=translated  naming=inferred
// naming: prints the 'sound file ... used as %s in alias' warning for two entries that share a file with different load types.
void SndAlias_WarnFileTypeMismatch(struct snd_alias_build_t *a, struct snd_alias_build_t *b)
{
    const char *typeA = snd_loadTypeName(a->loadType);
    const char *typeB = snd_loadTypeName(b->loadType);

    Com_Printf("WARNING: sound file '%s' used as %s in alias '%s' and %s in alias '%s'\n",
                a->soundFileName, typeA, a->name, typeB, b->name);
}

/* ---------------------------------------------------------------------------
 * 0x080aefb8  size=32
 * ------------------------------------------------------------------------- */
// 0x080aefb8  size=32  SndFile_Init  status=translated  naming=inferred
// naming: initializes a 16-byte sound file record (name, use count zero, load type); called from 0x080af224.
void SndFile_Init(struct snd_file_s *file, const char *name, uint32 loadType)
{
    file->name = name;
    file->useCount = 0;
    file->loadType = loadType;
}

/* ---------------------------------------------------------------------------
 * 0x080aefd8  size=588
 * ------------------------------------------------------------------------- */
// 0x080aefd8  size=588  Com_AddSoundAlias  status=translated  naming=observed
// naming: heap allocation tag 'Com_AddSoundAlias' names this function.
void Com_AddSoundAlias(struct snd_alias_build_t *entry, struct snd_alias_t *alias, const char *name,
                       struct snd_file_s *soundFile, const char *subtitle)
{
    size_t len;

    alias->name = name;
    if (entry->secondaryAliasName[0] == '\0') {
        alias->secondaryAliasName = NULL;
    } else {
        len = strlen(entry->secondaryAliasName);
        alias->secondaryAliasName = (char *)Com_HunkAllocTagged((uint32)(len + 1), "Com_AddSoundAlias", 0xd);
        strcpy(alias->secondaryAliasName, entry->secondaryAliasName);
    }
    alias->soundFile = soundFile;
    alias->subtitle = subtitle;
    alias->reserved10 = 0;
    alias->volMin = entry->volMin;
    alias->volMax = entry->volMax;
    alias->pitchMin = entry->pitchMin;
    alias->pitchMax = entry->pitchMax;
    alias->distMin = entry->distMin;
    alias->distMax = entry->distMax;
    /* Bits 7-10 hold the channel, bits 5-6 the load type, bits 0-4 the flags. Other
     * bits keep their previous contents, as in the binary. */
    alias->packedBits = (entry->channel << 7) | (alias->packedBits & 0xfffff87fu);
    alias->packedBits = (entry->loadType << 5) | (alias->packedBits & 0xffffff9fu);
    alias->probability = entry->probability;
    alias->loadSpec = entry->loadSpec;
    alias->masterSlave = entry->masterSlave;
    alias->falloffCurveRef = entry->falloffCurveRef;
    alias->startDelay = entry->startDelay;
    if (entry->flag144 == 0) {
        alias->packedBits &= 0xfffffffeu;
    } else {
        alias->packedBits |= 1u;
    }
    if (entry->flag145 == 0) {
        alias->packedBits &= 0xfffffffdu;
    } else {
        alias->packedBits |= 2u;
    }
    if (entry->flag146 == 0) {
        alias->packedBits &= 0xfffffffbu;
    } else {
        alias->packedBits |= 4u;
    }
    if (entry->flag147 == 0) {
        alias->packedBits &= 0xfffffff7u;
    } else {
        alias->packedBits |= 8u;
    }
    if (entry->flag148 == 0) {
        alias->packedBits &= 0xffffffefu;
    } else {
        alias->packedBits |= 0x10u;
    }
}

/* ---------------------------------------------------------------------------
 * 0x080af224  size=1356
 * Sorts the global alias list, deduplicates sound files, and fills the runtime
 * alias, alias-list and sound-file arrays with pool-allocated strings.
 * ------------------------------------------------------------------------- */
// 0x080af224  size=1356  Com_MakeSoundAliasesPermanent  status=translated  naming=observed
// naming: allocation tags 'Com_MakeSoundAliasesPermanent:*' and the anchor name the function.
void Com_MakeSoundAliasesPermanent(struct snd_alias_bank_t *aliasSet, struct snd_file_set_s *fileSet)
{
    struct snd_alias_build_t *entry;
    struct snd_alias_build_t *prevUnique = NULL;
    const char *prevFileName = NULL;
    size_t poolBytes = 0;
    int32 uniqueFileCount = 0;
    struct snd_alias_list_t *listNode = NULL;
    char *strings;
    char *subtitleOut;
    struct snd_alias_t *outAlias;
    struct snd_file_s *fileRec;
    const char *prevName;
    int emit;
    size_t len;
    int sameFile;
    int sameType;
    int sameBoth;

    fileSet->fileCount = 0;
    aliasSet->aliasCount = 0;
    if (g_soundAliasBuildCount == 0) {
        return;
    }

    g_soundAliasBuildHead = SndAlias_MergeSortList(g_soundAliasBuildHead, &g_soundAliasBuildCount, SndAlias_CompareName, 1);
    if (g_soundAliasBuildHead == NULL) {
        return;
    }
    g_soundAliasBuildHead = SndAlias_MergeSortList(g_soundAliasBuildHead, &g_soundAliasBuildCount,
                                                SndAlias_CompareFileAndType, 0);
    if (g_soundAliasBuildHead == NULL) {
        return;
    }

    /* Pass 1: first entry per (file, load type) is unique; later ones point at it. */
    for (entry = g_soundAliasBuildHead; entry != NULL; entry = entry->next) {
        len = strlen(entry->soundFileName);
        sameType = (prevUnique != NULL && entry->loadType == prevUnique->loadType) ? 1 : 0;
        sameFile = (prevFileName != NULL && Q_stricmp(prevFileName, entry->soundFileName) == 0) ? 1 : 0;
        sameBoth = sameFile && sameType;
        if (prevFileName == NULL || !sameBoth) {
            if (sameFile && !sameType) {
                SndAlias_WarnFileTypeMismatch(entry, prevUnique);
            }
            prevUnique = entry;
            prevFileName = entry->soundFileName;
            entry->dupOf = NULL;
            poolBytes += len + 1;
            uniqueFileCount++;
        } else {
            entry->dupOf = prevUnique;
        }
    }

    g_soundAliasBuildHead = SndAlias_MergeSortList(g_soundAliasBuildHead, &g_soundAliasBuildCount, SndAlias_CompareName, 1);
    if (g_soundAliasBuildHead == NULL) {
        return;
    }

    /* Pass 2: size the string pool for names and subtitles. */
    prevName = NULL;
    for (entry = g_soundAliasBuildHead; entry != NULL; entry = entry->next) {
        len = strlen(entry->name);
        if (prevName == NULL || Q_stricmp(prevName, entry->name) != 0) {
            poolBytes += len + 1;
            prevName = entry->name;
        }
        if (entry->subtitle != NULL) {
            poolBytes += strlen(entry->subtitle) + 1;
        }
    }

    aliasSet->aliases = (struct snd_alias_t *)Com_HunkAllocTagged((uint32)g_soundAliasBuildCount * 0x44u,
                                                          "Com_MakeSoundAliasesPermanent:aliases", 0xd);
    fileSet->files = (struct snd_file_s *)Com_HunkAllocTagged((uint32)uniqueFileCount << 4,
                                                      "Com_MakeSoundAliasesPermanent:soundFiles", 0xd);
    strings = (char *)Com_HunkAllocTagged((uint32)poolBytes, "Com_MakeSoundAliasesPermanent:strings", 0xd);

    /* Pass 3: copy strings and emit runtime aliases and list nodes. */
    prevName = NULL;
    for (entry = g_soundAliasBuildHead; entry != NULL; entry = entry->next) {
        if (prevName == NULL || Q_stricmp(prevName, entry->name) != 0) {
            prevName = strings;
            strcpy(strings, entry->name);
            strings += strlen(strings) + 1;
        }
        if (entry->subtitle == NULL) {
            subtitleOut = NULL;
        } else {
            subtitleOut = strings;
            strcpy(strings, entry->subtitle);
            strings += strlen(strings) + 1;
        }
        outAlias = aliasSet->aliases + aliasSet->aliasCount;

        emit = 0;
        if (listNode == NULL || Q_stricmp(listNode->aliases->name, prevName) != 0) {
            listNode = (struct snd_alias_list_t *)Com_HunkAllocTagged(0x10, "Com_MakeSoundAliasesPermanent:aliasList", 0xd);
            if (SND_InsertAliasList(prevName, listNode) != 0) {
                listNode->name = prevName;
                listNode->aliases = outAlias;
                emit = 1;
            } else {
                listNode = NULL;
                Com_Printf("^1ERROR: alias '%s' already added - ignoring\n", prevName);
            }
        } else {
            emit = 1;
        }

        if (emit) {
            if (entry->dupOf == NULL) {
                char *fileNameCopy = strings;
                strcpy(strings, entry->soundFileName);
                len = strlen(strings);
                fileRec = fileSet->files + fileSet->fileCount;
                SndFile_Init(fileRec, fileNameCopy, entry->loadType);
                fileSet->fileCount++;
                strings += len + 1;
            } else {
                fileRec = entry->dupOf->soundFile;
            }
            entry->soundFile = fileRec;
            Com_AddSoundAlias(entry, outAlias, prevName, fileRec, subtitleOut);
            aliasSet->aliasCount++;
            listNode->aliasCount++;
        }
    }
}

/* ---------------------------------------------------------------------------
 * 0x080af770  size=783
 * Parses a sndcurve body: knot count, then up to 8 (x, y) knots in [0, 1].
 * ------------------------------------------------------------------------- */
// 0x080af770  size=783  Com_ParseSndCurve  status=translated  naming=inferred
// naming: parses the 'sndcurve parse failure' diagnostics of a sound curve body.
int Com_ParseSndCurve(char *text, const char *curveName, struct snd_curve_t *curve)
{
    char *token;
    int32 knotIndex;
    int32 last;
    double value;

    Com_BeginParseSession(curveName);
    token = Com_Parse(&text);
    curve->knotCount = atoi(token);

    if (curve->knotCount < 2) {
        Com_EndParseSession();
        Com_Printf("^1ERROR: sndcurve parse failure on file \"%s\": knot count (%d) is less than 2\n",
                    curveName, curve->knotCount);
        return 0;
    }
    if (curve->knotCount >= 9) {
        Com_EndParseSession();
        Com_Printf("^1ERROR: sndcurve parse failure on file \"%s\": knot count (%d) is greater than maxKnots (%d)\n",
                    curveName, curve->knotCount, 8);
        return 0;
    }

    knotIndex = 0;
    for (;;) {
        token = Com_Parse(&text);
        if (!(*token != '\0' && *token != '}')) {
            break;
        }
        if (knotIndex > 7) {
            Com_EndParseSession();
            Com_Printf("^1ERROR: sndcurve parse failure on file \"%s\": knots parsed (%d) is greater than or equal to maxKnots (%d)\n",
                        curveName, knotIndex, 8);
            return 0;
        }
        value = atof(token);
        curve->knots[knotIndex].x = (float)value;
        if (curve->knots[knotIndex].x < 0.0f || 1.0f < curve->knots[knotIndex].x) {
            Com_EndParseSession();
            Com_Printf("^1ERROR: sndcurve parse failure on file \"%s\": knot x-coord '%f' is not in the range 0-1.\n",
                        curveName, (double)curve->knots[knotIndex].x);
            return 0;
        }

        token = Com_Parse(&text);
        if (*token == '\0' || *token == '}') {
            break;
        }
        value = atof(token);
        curve->knots[knotIndex].y = (float)value;
        if (curve->knots[knotIndex].y < 0.0f || 1.0f < curve->knots[knotIndex].y) {
            Com_EndParseSession();
            Com_Printf("^1ERROR: sndcurve parse failure on file \"%s\": knot x-coord '%f' is not in the range 0-1.\n",
                        curveName, (double)curve->knots[knotIndex].y);
            return 0;
        }
        knotIndex++;
    }

    Com_EndParseSession();
    if (knotIndex == curve->knotCount) {
        knotIndex--;
        last = knotIndex;
        if (curve->knots[0].x != 0.0f || curve->knots[0].y != 1.0f ||
            curve->knots[last].x != 1.0f || curve->knots[last].y != 0.0f) {
            curve->knots[0].x = 0.0f;
            curve->knots[0].y = 1.0f;
            curve->knots[last].x = 1.0f;
            curve->knots[last].y = 0.0f;
            Com_Printf("^3WARNING^7: sndcurve parse on file \"%s\": the first point must be '0.0000 1.0000' and the last point must be '1.0000 0.0000'.\nadjusting sndcurve endpoints.\n",
                        curveName);
        }
        return 1;
    }
    Com_Printf("^1ERROR: sndcurve parse failure on file \"%s\": knot count (%d) does not match knots parsed (%d).\n",
                curveName, curve->knotCount, knotIndex);
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x080afa80  size=553
 * Loads soundaliases/<name>.vfcurve, checks the SNDCURVE header, and parses it.
 * ------------------------------------------------------------------------- */
// 0x080afa80  size=553  Com_LoadSndCurve  status=translated  naming=inferred
// naming: loads 'soundaliases/%s.vfcurve' and reports 'Could not load sndcurve file'.
int Com_LoadSndCurve(const char *curveName, struct snd_curve_t *curve)
{
    size_t headerLen = strlen("SNDCURVE");
    char pathBuf[72];
    char textBuf[0x2008];
    int handle = 0;
    int32 size;
    int32 bodyLen;
    int status;

    Com_sprintf(pathBuf, 0x40, "soundaliases/%s.vfcurve", curveName);
    size = FS_OpenFileRead(pathBuf, &handle, 1);
    if (size < 0) {
        Com_Printf("^1ERROR: Could not load sndcurve file '%s'\n", pathBuf);
        return 0;
    }
    if (size == 0) {
        FS_FCloseFile(handle);
        Com_Printf("^1ERROR: sndcurve file '%s' is empty\n", pathBuf);
        return 0;
    }

    FS_Read(textBuf, (int)headerLen, handle);
    textBuf[headerLen] = '\0';
    if (strncmp(textBuf, "SNDCURVE", headerLen) != 0) {
        FS_FCloseFile(handle);
        Com_Printf("^1ERROR: \"%s\" does not appear to be a sndcurve file\n", pathBuf);
        return 0;
    }

    bodyLen = size - (int32)headerLen;
    if (bodyLen >= 0x2000) {
        FS_FCloseFile(handle);
        Com_Printf("^1ERROR: \"%s\" Is too long of a sndcurve file to parse\n", pathBuf);
        return 0;
    }
    memset(textBuf, 0, 0x2000);
    FS_Read(textBuf, bodyLen, handle);
    textBuf[bodyLen] = '\0';
    FS_FCloseFile(handle);

    status = Com_ParseSndCurve(textBuf, pathBuf, curve);
    if (status == 0) {
        return 0;
    }
    curve->name = curveName;
    return 1;
}

/* ---------------------------------------------------------------------------
 * 0x080afcaa  size=68
 * ------------------------------------------------------------------------- */
// 0x080afcaa  size=68  SndCurve_InitDefault  status=translated  naming=inferred
// naming: initializes the default two-knot curve (0,1)-(1,0) that the endpoint warning of 0x080af770 enforces.
void SndCurve_InitDefault(struct snd_curve_t *curve)
{
    curve->name = g_sndEmptyString;
    curve->knotCount = 2;
    curve->knots[0].x = 0.0f;
    curve->knots[0].y = 1.0f;
    curve->knots[1].x = 1.0f;
    curve->knots[1].y = 0.0f;
}

/* ---------------------------------------------------------------------------
 * 0x080afcee  size=262
 * Returns 1 when a SUBTITLE_ key already has a REFERENCE entry in subtitle.st.
 * ------------------------------------------------------------------------- */
// 0x080afcee  size=262  SndSubtitle_HasReference  status=translated  naming=inferred
// naming: checks 'SUBTITLE_' keys against 'soundaliases/subtitle.st' references.
int SndSubtitle_HasReference(const char *key)
{
    int found = 0;
    char *buffer = NULL;
    char *cursor;
    char *token;

    if (Q_strncmp(key, "SUBTITLE_", 9) != 0) {
        return 0;
    }
    if (FS_ReadFile("soundaliases/subtitle.st", &buffer) < 0) {
        Com_Printf("WARNING: Could not read local copy of StringEd file %s\n", "soundaliases/subtitle.st");
        return 0;
    }
    Com_BeginParseSession("soundaliases/subtitle.st");
    cursor = buffer;
    for (;;) {
        token = Com_Parse(&cursor);
        if (cursor == NULL) {
            break;
        }
        if (strcmp(token, "REFERENCE") == 0) {
            token = Com_ParseOnLine(&cursor);
            if (Q_stricmp(key + 9, token) == 0) {
                found = 1;
                break;
            }
        }
        Com_SkipRestOfLine(&cursor);
    }
    Com_EndParseSession();
    FS_FreeFile(buffer);
    return found;
}

/* ---------------------------------------------------------------------------
 * 0x080afdf4  size=351
 * Returns the English text of a REFERENCE entry matching `key`, or NULL.
 * ------------------------------------------------------------------------- */
// 0x080afdf4  size=351  SndSubtitle_FindEnglishText  status=translated  naming=inferred
// naming: looks up the LANG_ENGLISH text of a REFERENCE in 'soundaliases/subtitle.st'.
const char *SndSubtitle_FindEnglishText(const char *key)
{
    char *buffer = NULL;
    char *cursor;
    char *token;
    int readRc;

    readRc = FS_ReadFile("soundaliases/subtitle.st", &buffer);
    if (readRc < 0) {
        Com_Printf("WARNING: Could not read local copy of StringEd file %s\n", "soundaliases/subtitle.st");
        return NULL;
    }
    Com_BeginParseSession("soundaliases/subtitle.st");
    cursor = buffer;
    for (;;) {
        token = Com_Parse(&cursor);
        if (cursor == NULL) {
            break;
        }
        if (strcmp(token, "REFERENCE") == 0) {
            token = Com_ParseOnLine(&cursor);
            strcpy(g_sndReferenceNameBuf, token);
            Com_SkipRestOfLine(&cursor);
            do {
                token = Com_Parse(&cursor);
                if (cursor == NULL) {
                    Com_Error(1, "\x15StringEd file %s has bad syntax", "soundaliases/subtitle.st");
                }
            } while (strcmp(token, "LANG_ENGLISH") != 0);
            token = Com_ParseOnLine(&cursor);
            if (Q_stricmp(key, token) == 0) {
                Com_EndParseSession();
                FS_FreeFile(buffer);
                return g_sndReferenceNameBuf;
            }
        }
        Com_SkipRestOfLine(&cursor);
    }
    Com_EndParseSession();
    FS_FreeFile(buffer);
    return NULL;
}

/* ---------------------------------------------------------------------------
 * 0x080aff54  size=204
 * ------------------------------------------------------------------------- */
// 0x080aff54  size=204  SndSubtitle_WriteReferenceBlock  status=translated  naming=inferred
// naming: writes one REFERENCE / LANG_ENGLISH block of a StringEd file.
void SndSubtitle_WriteReferenceBlock(const char *key, const char *text, int handle)
{
    size_t len;

    len = strlen("REFERENCE           ");
    FS_Write("REFERENCE           ", len, handle);
    len = strlen(key);
    FS_Write(key, len, handle);
    len = strlen("\r\nLANG_ENGLISH        \"");
    FS_Write("\r\nLANG_ENGLISH        \"", len, handle);
    len = strlen(text);
    FS_Write(text, len, handle);
    len = strlen("\"\r\n\r\n");
    FS_Write("\"\r\n\r\n", len, handle);
}

/* ---------------------------------------------------------------------------
 * 0x080b0020  size=851
 * Adds or replaces a REFERENCE entry in subtitle.st through soundaliases/temp.st.
 * ------------------------------------------------------------------------- */
// 0x080b0020  size=851  SndSubtitle_WriteReference  status=translated  naming=inferred
// naming: rewrites soundaliases/subtitle.st via a temp file to add a SUBTITLE_ reference.
void SndSubtitle_WriteReference(const char *key, const char *text)
{
    int found = 0;
    const char *keyName = key + 9;
    int handle;
    char *buffer = NULL;
    char *cursor;
    char *blockStart;
    char *token;
    int readCount;
    int exitToSkip;
    char tempPath[256];
    char subPath[256];

    handle = FS_FOpenFileWrite("soundaliases/temp.st");
    if (handle == 0) {
        Com_Printf("WARNING: Could not open output file %s for writing\n", "soundaliases/temp.st");
        return;
    }
    readCount = FS_ReadFile("soundaliases/subtitle.st", &buffer);
    if (readCount < 0) {
        Com_Printf("WARNING: Could not read local copy of StringEd file %s\n", "soundaliases/subtitle.st");
        FS_FCloseFile(handle);
        return;
    }

    Com_BeginParseSession("soundaliases/subtitle.st");
    cursor = buffer;
    blockStart = buffer;
    for (;;) {
        token = Com_Parse(&cursor);
        if (cursor == NULL) {
            break;
        }
        if (strcmp(token, "ENDMARKER") == 0) {
            if (blockStart < cursor) {
                FS_Write(blockStart, (size_t)((cursor - blockStart) - 11), handle);
            }
            break;
        }
        if (strcmp(token, "REFERENCE") == 0) {
            token = Com_ParseOnLine(&cursor);
            if (strcmp(token, keyName) == 0) {
                if (blockStart < cursor) {
                    FS_Write(blockStart, (size_t)(cursor - blockStart), handle);
                }
                SndSubtitle_WriteReferenceBlock(keyName, text, handle);
                found = 1;
                exitToSkip = 0;
                for (;;) {
                    blockStart = cursor;
                    token = Com_Parse(&cursor);
                    if (cursor == NULL) {
                        blockStart = NULL;
                        exitToSkip = 1;
                        break;
                    }
                    if (strcmp(token, "REFERENCE") == 0 || strcmp(token, "ENDMARKER") == 0) {
                        break;
                    }
                }
                if (!exitToSkip) {
                    Com_UngetToken();
                }
            }
        }
        Com_SkipRestOfLine(&cursor);
    }

    if (!found) {
        SndSubtitle_WriteReferenceBlock(keyName, text, handle);
    }
    Com_EndParseSession();
    FS_FreeFile(buffer);
    FS_Write("\r\nENDMARKER\r\n\r\n\r\n", strlen("\r\nENDMARKER\r\n\r\n\r\n"), handle);
    FS_FCloseFile(handle);

    FS_BuildOSPath_Fatal(chunk028_fs_base(), fs_gamedir, "soundaliases/temp.st", tempPath);
    FS_BuildOSPath_Fatal(chunk028_fs_base(), fs_gamedir, "soundaliases/subtitle.st", subPath);
    FS_CopyFile(tempPath, subPath);
    FS_Remove(tempPath);
}

/* ---------------------------------------------------------------------------
 * 0x080b0374  size=2857
 * Localizes one soundaliases/<fileName> CSV: rewrites subtitle cells to
 * SUBTITLE_ keys and records new REFERENCE entries.
 * ------------------------------------------------------------------------- */
// 0x080b0374  size=2857  Com_LocalizeSoundAliasFile  status=translated  naming=inferred
// naming: 'Processing sound alias file' and 'Localized %i sound alias subtitles' messages describe the pass.
void Com_LocalizeSoundAliasFile(const char *fileName, const char *zone, char *tmpPath)
{
    char pathBuf[256];
    char srcOsPath[264];
    char columnValue[24][1024];
    char keyBuf[1024];
    char rowCtx[352];
    uint8 flags[24];
    int32 columnIds[257];
    char tempOsPath[256];
    char origOsPath[256];
    char *fileBuf = NULL;
    char *cursor;
    char *lineStart;
    char *token;
    char *valueText;
    FILE *probe;
    int tempHandle;
    int rc;
    int32 columnCount = 0;
    int32 localizedCount = 0;
    int32 idx;
    int32 needsLocalize;
    int hasName = 0;
    int hasFile = 0;
    size_t len;
    const char *fmt;
    int hasSeparator;

    Com_sprintf(pathBuf, 0x100, "soundaliases/%s", fileName);
    FS_BuildOSPath_Fatal(chunk028_fs_base(), fs_gamedir, pathBuf, srcOsPath);
    Com_Printf("Processing sound alias file %s..\n", srcOsPath);
    probe = (FILE *)Com_Fopen(srcOsPath, "r+");
    if (probe == NULL) {
        Com_Printf("WARNING: Can not write to sound alias file %s\n", srcOsPath);
        return;
    }
    Com_Fclose(probe);

    rc = FS_ReadFile(pathBuf, &fileBuf);
    if (rc < 0) {
        Com_Printf("WARNING: Could not read sound alias file %s\n", pathBuf);
        return;
    }
    tempHandle = FS_FOpenFileWrite("soundaliases/temp.csv");
    if (tempHandle == 0) {
        Com_Printf("WARNING: Could not open output file %s for writing\n", "soundaliases/temp.csv");
        return;
    }

    Com_BeginParseSession(pathBuf);
    Com_SetCSVMode(1);
    cursor = fileBuf;

    while (cursor != NULL) {
        if (*cursor == '\r') {
            while (*cursor == '\r') {
                cursor++;
            }
        }
        if (*cursor == '\n') {
            cursor++;
            FS_Write("\r\n", 2, tempHandle);
        }
        lineStart = cursor;
        token = Com_Parse(&cursor);
        if (cursor == NULL) {
            break;
        }
        /* The '#Chateau' marker only resets the loop index, which every loop below re-initializes. */

        if (*token == '\0' || *token == '#') {
            Com_SkipRestOfLine(&cursor);
            if (*lineStart == '\n') {
                FS_Write("\r", 1, tempHandle);
            }
            FS_Write(lineStart, (size_t)(cursor - lineStart), tempHandle);
            continue;
        }

        if (columnCount == 0) {
            /* Header row. */
            hasName = 0;
            hasFile = 0;
            do {
                columnIds[columnCount] = 0;
                for (idx = 1; idx < 0x18; idx++) {
                    if (Q_stricmp(g_sndAliasColumnNames[idx], token) == 0) {
                        columnIds[columnCount] = idx;
                        if (idx == 1) {
                            hasName = 1;
                        } else if (idx == 3) {
                            hasFile = 1;
                        }
                        break;
                    }
                }
                columnCount++;
                if (columnCount == 0x100 || cursor == NULL || *cursor == '\n') {
                    break;
                }
                token = Com_ParseOnLine(&cursor);
            } while (1);

            if (!hasName || !hasFile) {
                Com_Error(1, "\x15Sound alias file %s: missing 'name' and/or 'file' columns\n", fileName);
            }
            Com_SkipRestOfLine(&cursor);
            if (*lineStart == '\n') {
                FS_Write("\r", 1, tempHandle);
            }
            FS_Write(lineStart, (size_t)(cursor - lineStart), tempHandle);
            continue;
        }

        /* Data row. */
        memset(flags, 0, 0x18);
        Com_InitSoundAliasBuild((struct snd_alias_build_t *)rowCtx, fileName, "menu");
        idx = 0;
        for (;;) {
            strcpy(columnValue[columnIds[idx]], token);
            if (*token != '\0') {
                Com_ParseAliasColumn("menu", zone, fileName, token, columnIds[idx], flags, (struct snd_alias_build_t *)rowCtx);
            }
            idx++;
            if (idx == columnCount) {
                break;
            }
            token = Com_ParseOnLine(&cursor);
        }

        if (flags[1] == 0 || flags[3] == 0) {
            Com_Error(1, "\x15Sound alias file %s: alias entry missing name and/or file\n", fileName);
        }

        needsLocalize = 0;
        if (flags[4] != 0) {
            len = strlen(columnValue[4]);
            for (idx = 0; idx < (int32)len; idx++) {
                char c = columnValue[4][idx];
                if (!((('@' < c) && (c < '[')) || (('/' < c) && (c < ':')) || (c == '_'))) {
                    break;
                }
            }
            if (idx < (int32)len || Q_strncmp(columnValue[4], "SUBTITLE_", 9) != 0 ||
                SndSubtitle_HasReference(columnValue[4]) == 0) {
                needsLocalize = 1;
            }
        }

        if (needsLocalize == 0) {
            Com_SkipRestOfLine(&cursor);
            FS_Write(lineStart, (size_t)(cursor - lineStart), tempHandle);
            continue;
        }

        for (idx = 0; idx < columnCount; idx++) {
            int32 col = columnIds[idx];
            if (col == 0 || flags[col] == 0) {
                if (idx != columnCount - 1) {
                    FS_Write(",", 1, tempHandle);
                }
            } else if (col == 4) {
                valueText = (char *)SndSubtitle_FindEnglishText(columnValue[4]);
                if (valueText == NULL) {
                    if (flags[2] == 0) {
                        Com_sprintf(keyBuf, 0x400, "%s%s", "SUBTITLE_", columnValue[1]);
                    } else {
                        Com_sprintf(keyBuf, 0x400, "%s%s_%s", "SUBTITLE_", columnValue[1], columnValue[2]);
                    }
                    valueText = Q_strupr(keyBuf);
                    SndSubtitle_WriteReference(keyBuf, columnValue[4]);
                    localizedCount++;
                } else {
                    Com_sprintf(keyBuf, 0x400, "%s%s", "SUBTITLE_", valueText);
                    valueText = Q_strupr(keyBuf);
                }
                len = strlen(valueText);
                FS_Write(valueText, len, tempHandle);
            } else {
                hasSeparator = (strchr(columnValue[col], ',') != NULL || strchr(columnValue[col], ' ') != NULL ||
                                strchr(columnValue[col], '\n') != NULL || strchr(columnValue[col], '\r') != NULL);
                if (idx == columnCount - 1) {
                    fmt = hasSeparator ? "\"%s\"" : "%s";
                } else {
                    fmt = hasSeparator ? "\"%s\"," : "%s,";
                }
                valueText = va(fmt, columnValue[col]);
                len = strlen(valueText);
                FS_Write(valueText, len, tempHandle);
            }
        }
        FS_Write("\r\n", 2, tempHandle);
        Com_SkipRestOfLine(&cursor);
    }

    Com_EndParseSession();
    FS_FCloseFile(tempHandle);
    FS_BuildOSPath_Fatal(chunk028_fs_base(), fs_gamedir, "soundaliases/temp.csv", tempOsPath);
    FS_BuildOSPath_Fatal(chunk028_fs_base(), fs_gamedir, pathBuf, origOsPath);
    if (localizedCount != 0) {
        FS_CopyFile(tempOsPath, origOsPath);
    }
    FS_Remove(tempOsPath);
    Com_Printf("Localized %i sound alias subtitles\n", localizedCount);
}

/* ---------------------------------------------------------------------------
 * 0x080b0e9e  size=313
 * Copies a whole file to another path through memory.
 * ------------------------------------------------------------------------- */
// 0x080b0e9e  size=313  COM_WriteFinalStringEdFile  status=translated  naming=observed
// naming: the 'Short read/write in COM_WriteFinalStringEdFile()' errors name this function.
void COM_WriteFinalStringEdFile(const char *srcPath, const char *dstPath)
{
    FILE *in;
    FILE *out;
    size_t size;
    size_t got;
    void *buf;

    in = (FILE *)Com_Fopen(srcPath, "rb");
    if (in == NULL) {
        return;
    }
    Com_Fseek(in, 0, SEEK_END);
    size = (size_t)ftell(in);
    Com_Fseek(in, 0, SEEK_SET);
    buf = malloc(size);
    got = Com_Fread(buf, 1, size, in);
    if (got != size) {
        Com_Error(0, "\x15Short read in COM_WriteFinalStringEdFile()\n");
    }
    Com_Fclose(in);

    out = (FILE *)Com_Fopen(dstPath, "wb");
    if (out == NULL) {
        free(buf);
        return;
    }
    got = Com_Fwrite(buf, 1, size, out);
    if (got != size) {
        Com_Error(0, "\x15Short write in COM_WriteFinalStringEdFile()\n");
    }
    Com_Fclose(out);
    free(buf);
}

/* ---------------------------------------------------------------------------
 * 0x080b11e8  size=61
 * Evaluates two chained float selections; the result is not stored.
 * ------------------------------------------------------------------------- */
// 0x080b11e8  size=61  Float_Clamp_080b11e8  status=translated  naming=unknown
// naming: no string or callee names it; only calls the selection helper 0x080b1226 and discards the final result.
float Float_Clamp_080b11e8(float a, float b, float c)
{
    float t;

    t = Float_SelectIfNonNegative(a - c, c, a);
    return Float_SelectIfNonNegative(b - a, b, t);
}

/* ---------------------------------------------------------------------------
 * 0x080b1226  size=41
 * ------------------------------------------------------------------------- */
// 0x080b1226  size=41  Float_SelectIfNonNegative  status=translated  naming=inferred
// naming: returns the second argument when the first is non-negative, else the third (select helper).
float Float_SelectIfNonNegative(float test, float ifNonNegative, float ifNegative)
{
    float result;

    if (0.0f <= test) {
        result = ifNonNegative;
    } else {
        result = ifNegative;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x080b1250 / 0x080b1268  size=23 / size=12
 * Byte flag pair. The setter writes dvar_loadingConfig and the getter reads
 * dvar_initialised, as in the binary (see open questions).
 * ------------------------------------------------------------------------- */
// 0x080b1250  size=23  Dvar_SetSetterGate  status=translated  naming=unknown
// naming: stores one byte into dvar_loadingConfig; no string names it.
void Dvar_SetSetterGate(uint8 value)
{
    dvar_loadingConfig = value;
}

// 0x080b1268  size=12  Dvar_GetSvCheatsReady  status=translated  naming=unknown
// naming: returns the byte at dvar_initialised (one byte before the setter's target).
uint8 Dvar_GetSvCheatsReady(void)
{
    return dvar_initialised;
}

/* ---------------------------------------------------------------------------
 * 0x080b1274  size=117
 * ------------------------------------------------------------------------- */
// 0x080b1274  size=117  generateHashValue  status=translated  naming=observed
// naming: error string 'null name in generateHashValue' names this function.
uint32 generateHashValue(const char *name)
{
    uint32 hash = 0;
    int32 i;

    if (name == NULL) {
        Com_Error(1, "\x15null name in generateHashValue");
    }
    for (i = 0; name[i] != '\0'; i++) {
        hash += (uint32)((i + 0x77) * __builtin_tolower((int)name[i]));
    }
    return hash & 0xff;
}

/* ---------------------------------------------------------------------------
 * 0x080b12ea  size=103
 * ------------------------------------------------------------------------- */
// 0x080b12ea  size=103  Com_IsIdentifierName  status=translated  naming=inferred
// naming: tests that a name is non-empty only when every character is alphanumeric or '_'.
int Com_IsIdentifierName(const char *s)
{
    int32 i;

    if (s == NULL) {
        return 0;
    }
    for (i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (__builtin_isalnum((int)c) == 0 && c != '_') {
            return 0;
        }
    }
    return 1;
}

/* ---------------------------------------------------------------------------
 * 0x080b1352  size=66
 * ------------------------------------------------------------------------- */
// 0x080b1352  size=66  Dvar_ScratchAlloc  status=translated  naming=inferred
// naming: allocates a run of float slots from a 12-slot scratch ring used by dvar vector parsing.
float *Dvar_ScratchAlloc(int count)
{
    int32 base;

    if ((uint32)(count + g_dvarScratchIndex) > 0xc) {
        g_dvarScratchIndex = 0;
    }
    base = g_dvarScratchIndex;
    g_dvarScratchIndex += count;
    return &g_dvarScratch[base];
}

/* ---------------------------------------------------------------------------
 * 0x080b1394  size=87
 * ------------------------------------------------------------------------- */
// 0x080b1394  size=87  Dvar_AllocValueBlock  status=translated  naming=inferred
// naming: allocates one block for the three value words (current, latched, reset) of a dvar.
void Dvar_AllocValueBlock(struct dvar_s *dvar)
{
    dvar->current.unsignedInt = (uint32)(uintptr_t)Com_MallocZeroed((uint32)dvar->type * 12u);
    dvar->latched.unsignedInt = dvar->current.unsignedInt + (uint32)dvar->type * 4u;
    dvar->reset.unsignedInt = dvar->latched.unsignedInt + (uint32)dvar->type * 4u;
}

/* ---------------------------------------------------------------------------
 * 0x080b13ec  size=22
 * ------------------------------------------------------------------------- */
// 0x080b13ec  size=22  Dvar_FreeValueBlock  status=translated  naming=inferred
// naming: releases the value block allocated by Dvar_AllocValueBlock.
void Dvar_FreeValueBlock(struct dvar_s *dvar)
{
    Com_Free_A(w_ptr(dvar->current.unsignedInt));
}

/* ---------------------------------------------------------------------------
 * 0x080b1402 / 0x080b1416  size=19 / size=19
 * ------------------------------------------------------------------------- */
// 0x080b1402  size=19  Dvar_InternString  status=translated  naming=inferred
// naming: pool-copies a string (tail call to the string-pool allocator and returns its result).
const char *Dvar_InternString(const char *s)
{
    return CopyString(s);
}

// 0x080b1416  size=19  Dvar_ReleasePoolString  status=translated  naming=inferred
// naming: releases a pool string through the pool free routine.
void Dvar_ReleasePoolString(const char *s)
{
    Com_Free_A((void *)s);
}

/* ---------------------------------------------------------------------------
 * 0x080b142a  size=215
 * Maps a string to its canonical value: "" for empty, one-digit strings to a
 * static table, "off"/"on" to the canonical pointers, otherwise a pool copy.
 * ------------------------------------------------------------------------- */
// 0x080b142a  size=215  Dvar_CanonicalString  status=translated  naming=inferred
// naming: canonicalizes dvar value strings; the 'off'/'on' and one-digit table pointers define the canonical forms.
const char *Dvar_CanonicalString(const char *s)
{
    int32 len;

    if (*s == '\0') {
        return g_emptyString;
    }
    len = ScrStr_Strlen(s);
    if (s[1] == '\0') {
        if ('/' < s[0] && s[0] < ':') {
            return g_digitStringTable + (int)s[0] * 2;
        }
    } else if (s[0] == 'o') {
        if (len == 3 && s[1] == 'f' && s[2] == 'f' && s[3] == '\0') {
            return g_canonicalOff;
        }
        if (len == 2 && s[1] == 'n' && s[2] == '\0') {
            return g_canonicalOn;
        }
    }
    return CopyString(s);
}

/* ---------------------------------------------------------------------------
 * 0x080b1502  size=78
 * Frees a value string unless it is a canonical one-digit, "off" or "on" string.
 * ------------------------------------------------------------------------- */
// 0x080b1502  size=78  Dvar_ReleaseValueString  status=translated  naming=inferred
// naming: frees a dvar value string that is not a canonical form returned by Dvar_CanonicalString.
void Dvar_ReleaseValueString(const char *s)
{
    if (*s != '\0' && (s[1] != '\0' || *s < '0' || '9' < *s) && s != g_canonicalOff && s != g_canonicalOn) {
        Com_Free_A((void *)s);
    }
}

/* ---------------------------------------------------------------------------
 * 0x080b1550 / 0x080b158c / 0x080b15c8  size=60 each
 * ------------------------------------------------------------------------- */
// 0x080b1550  size=60  Dvar_ReleaseCurrentValue  status=translated  naming=inferred
// naming: releases the current value word unless it is shared with latched or reset.
void Dvar_ReleaseCurrentValue(struct dvar_s *dvar)
{
    if (dvar->current.unsignedInt != dvar->latched.unsignedInt && dvar->current.unsignedInt != dvar->reset.unsignedInt) {
        Dvar_ReleaseValueString(w_str(dvar->current.unsignedInt));
    }
    dvar->current.unsignedInt = 0;
}

// 0x080b158c  size=60  Dvar_ReleaseLatchedValue  status=translated  naming=inferred
// naming: releases the latched value word unless it is shared with current or reset.
void Dvar_ReleaseLatchedValue(struct dvar_s *dvar)
{
    if (dvar->latched.unsignedInt != dvar->current.unsignedInt && dvar->latched.unsignedInt != dvar->reset.unsignedInt) {
        Dvar_ReleaseValueString(w_str(dvar->latched.unsignedInt));
    }
    dvar->latched.unsignedInt = 0;
}

// 0x080b15c8  size=60  Dvar_ReleaseResetValue  status=translated  naming=inferred
// naming: releases the reset value word unless it is shared with current or latched.
void Dvar_ReleaseResetValue(struct dvar_s *dvar)
{
    if (dvar->reset.unsignedInt != dvar->current.unsignedInt && dvar->reset.unsignedInt != dvar->latched.unsignedInt) {
        Dvar_ReleaseValueString(w_str(dvar->reset.unsignedInt));
    }
    dvar->reset.unsignedInt = 0;
}

/* ---------------------------------------------------------------------------
 * 0x080b1604 / 0x080b1698 / 0x080b172c  size=148 each
 * Each setter prefers an existing value word that equals the new string, then
 * the canonical form. They differ only in which words they try first.
 * ------------------------------------------------------------------------- */
static uint32 dvar_pick_string_word(const char *s, uint32 firstWord, uint32 secondWord)
{
    if (firstWord != 0 && (s == w_str(firstWord) || strcmp(s, w_str(firstWord)) == 0)) {
        return firstWord;
    }
    if (secondWord == 0 || (s != w_str(secondWord) && strcmp(s, w_str(secondWord)) != 0)) {
        return (uint32)(uintptr_t)Dvar_CanonicalString(s);
    }
    return secondWord;
}

// 0x080b1604  size=148  Dvar_SetCurrentString  status=translated  naming=inferred
// naming: sets the current value word from a string, sharing latched or reset when equal.
void Dvar_SetCurrentString(struct dvar_s *dvar, const char *s)
{
    dvar->current.unsignedInt = dvar_pick_string_word(s, dvar->latched.unsignedInt, dvar->reset.unsignedInt);
}

// 0x080b1698  size=148  Dvar_SetLatchedString  status=translated  naming=inferred
// naming: sets the latched value word from a string, sharing current or reset when equal.
void Dvar_SetLatchedString(struct dvar_s *dvar, const char *s)
{
    dvar->latched.unsignedInt = dvar_pick_string_word(s, dvar->current.unsignedInt, dvar->reset.unsignedInt);
}

// 0x080b172c  size=148  Dvar_SetResetString  status=translated  naming=inferred
// naming: sets the reset value word from a string, sharing current or latched when equal.
void Dvar_SetResetString(struct dvar_s *dvar, const char *s)
{
    dvar->reset.unsignedInt = dvar_pick_string_word(s, dvar->current.unsignedInt, dvar->latched.unsignedInt);
}

/* ---------------------------------------------------------------------------
 * 0x080b17c0 / 0x080b17f6  size=54 / size=133
 * ------------------------------------------------------------------------- */
// 0x080b17c0  size=54  Dvar_CurrentEnumString  status=translated  naming=inferred
// naming: current enum display string (empty when the dvar has no enum domain).
const char *Dvar_CurrentEnumString(const struct dvar_s *dvar)
{
    if (dvar->domain.enumeration.stringCount == 0) {
        return g_emptyString;
    }
    return dvar->domain.enumeration.strings[w_int(dvar->current.unsignedInt)];
}

// 0x080b17f6  size=133  Dvar_EnumStringFromIndexText  status=translated  naming=inferred
// naming: maps decimal index text to an enum display string; non-digit text yields the empty string.
const char *Dvar_EnumStringFromIndexText(const struct dvar_s *dvar, const char *text)
{
    int32 len;
    int32 i;

    if (dvar->domain.enumeration.stringCount == 0) {
        return g_emptyString;
    }
    len = ScrStr_Strlen(text);
    for (i = 0; i < len; i++) {
        if (__builtin_isdigit((int)text[i]) == 0) {
            return g_emptyString;
        }
    }
    return dvar->domain.enumeration.strings[atoi(text)];
}

/* ---------------------------------------------------------------------------
 * 0x080b187c  size=475
 * Formats a dvar value word according to its type.
 * ------------------------------------------------------------------------- */
// 0x080b187c  size=475  Dvar_FormatValue  status=translated  naming=inferred
// naming: formats a typed value with '%g' and '%i' style strings for the dvar display path.
const char *Dvar_FormatValue(const struct dvar_s *dvar, uint32 value)
{
    const float *vec = (const float *)w_ptr(value);
    const char *result;

    switch (dvar->type) {
    case 0:
        result = ((uint8)value == 0) ? "0" : "1";
        break;
    case 1:
        result = va("%g", (double)w_float(value));
        break;
    case 2:
        result = va("%g %g", (double)vec[0], (double)vec[1]);
        break;
    case 3:
        result = va("%g %g %g", (double)vec[0], (double)vec[1], (double)vec[2]);
        break;
    case 4:
        result = va("%g %g %g %g", (double)vec[0], (double)vec[1], (double)vec[2], (double)vec[3]);
        break;
    case 5:
        result = va("%i", (int)value);
        break;
    case 6:
        if (dvar->domain.enumeration.stringCount == 0) {
            result = g_emptyString;
        } else {
            result = dvar->domain.enumeration.strings[w_int(value)];
        }
        break;
    case 7:
        result = va("%s", w_str(value));
        break;
    case 8:
        result = va("%g %g %g %g",
                             (double)((float)(value & 0xff) * 0.003921569f),
                             (double)((float)((value >> 8) & 0xff) * 0.003921569f),
                             (double)((float)((value >> 16) & 0xff) * 0.003921569f),
                             (double)((float)(value >> 24) * 0.003921569f));
        break;
    default:
        result = g_emptyString;
        break;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x080b1a58 / 0x080b1a7e / 0x080b1a92  size=38 / 19 / 31
 * ------------------------------------------------------------------------- */
// 0x080b1a58  size=38  Dvar_ParseBool  status=translated  naming=inferred
// naming: parses a bool dvar value through atoi (nonzero is true).
bool Dvar_ParseBool(const char *s)
{
    int32 v = atoi(s);
    return v != 0;
}

// 0x080b1a7e  size=19  Dvar_ParseInt  status=translated  naming=inferred
// naming: parses an int dvar value through atoi; the binary returns atoi's result in eax.
int Dvar_ParseInt(const char *s)
{
    return atoi(s);
}

// 0x080b1a92  size=31  Dvar_ParseFloat  status=translated  naming=inferred
// naming: parses a float dvar value through atof and narrows to float.
float Dvar_ParseFloat(const char *s)
{
    double v = atof(s);
    return (float)v;
}

/* ---------------------------------------------------------------------------
 * 0x080b1ab2 / 0x080b1afc / 0x080b1b50  size=73 / 83 / 93
 * Parse 2, 3 and 4 floats into scratch storage.
 * ------------------------------------------------------------------------- */
// 0x080b1ab2  size=73  Dvar_ParseVec2  status=translated  naming=inferred
// naming: parses a vec2 dvar value into scratch storage.
float *Dvar_ParseVec2(const char *s)
{
    float *v = Dvar_ScratchAlloc(2);

    Vec2_Clear_080b5744(v);
    sscanf(s, "%g %g", &v[0], &v[1]);
    return v;
}

// 0x080b1afc  size=83  Dvar_ParseVec3  status=translated  naming=inferred
// naming: parses a vec3 dvar value into scratch storage.
float *Dvar_ParseVec3(const char *s)
{
    float *v = Dvar_ScratchAlloc(3);

    Vec3_Clear_080b57e6(v);
    sscanf(s, "%g %g %g", &v[0], &v[1], &v[2]);
    return v;
}

// 0x080b1b50  size=93  Dvar_ParseVec4  status=translated  naming=inferred
// naming: parses a vec4 dvar value into scratch storage.
float *Dvar_ParseVec4(const char *s)
{
    float *v = Dvar_ScratchAlloc(4);

    Vec4_Clear(v);
    sscanf(s, "%g %g %g %g", &v[0], &v[1], &v[2], &v[3]);
    return v;
}

/* ---------------------------------------------------------------------------
 * 0x080b1bae  size=298
 * Matches an enum display name (case-insensitively) or a decimal index.
 * ------------------------------------------------------------------------- */
// 0x080b1bae  size=298  Dvar_ParseEnumIndex  status=translated  naming=inferred
// naming: resolves an enum domain entry by name or by numeric index text; -1337 signals no match.
int Dvar_ParseEnumIndex(const struct DvarLimitsEnum *desc, const char *s)
{
    int32 index;
    const char *cur;
    int32 i;
    uint32 len;

    for (i = 0; i < desc->stringCount; i++) {
        if (strcasecmp(s, desc->strings[i]) == 0) {
            return i;
        }
    }

    index = 0;
    cur = s;
    for (;;) {
        if (*cur == '\0') {
            if (index < 0 || desc->stringCount <= index) {
                len = (uint32)ScrStr_Strlen(s);
                for (i = 0; i < desc->stringCount; i++) {
                    if (Str_NCaseCmpN(s, desc->strings[i], len) == 0) {
                        return i;
                    }
                }
                return -0x539;
            }
            return index;
        }
        if (*cur < '0' || '9' < *cur) {
            break;
        }
        index = (int32)*cur + index * 10 - 0x30;
        cur++;
    }
    return -0x539;
}

/* ---------------------------------------------------------------------------
 * 0x080b1cd8  size=319
 * Parses four floats and stores them as clamped RGBA bytes.
 * ------------------------------------------------------------------------- */
static uint8 dvar_color_channel(float v)
{
    float clamped = Float_Max_080b56c6(0.0f, Float_Min_080b56ea(1.0f, v));

    return Float_RoundToInt_080b570e((float)(255.0 * (double)clamped));
}

// 0x080b1cd8  size=319  Dvar_ParseColorRGBA  status=translated  naming=inferred
// naming: parses an RGBA colour dvar value, clamping each channel to [0, 1] and scaling to bytes.
void Dvar_ParseColorRGBA(const char *s, uint8 *rgba)
{
    float c[4];

    Vec4_Clear(c);
    sscanf(s, "%g %g %g %g", &c[0], &c[1], &c[2], &c[3]);
    rgba[0] = dvar_color_channel(c[0]);
    rgba[1] = dvar_color_channel(c[1]);
    rgba[2] = dvar_color_channel(c[2]);
    rgba[3] = dvar_color_channel(c[3]);
}

/* ---------------------------------------------------------------------------
 * 0x080b1e18  size=238
 * Parses text into one value word according to its type. Type 0 writes a
 * single byte; out-of-range types store zero.
 * ------------------------------------------------------------------------- */
// 0x080b1e18  size=238  Dvar_ParseValueWord  status=translated  naming=inferred
// naming: dispatches a dvar value parse by type code (bool, float, vec2-4, int, enum, string, RGBA).
// Returns the parsed word through the hidden aggregate-return pointer (callee pops it, "ret 4").
// Original stack: sret, type, domain (two words by value), text. For bool dvars only the low
// byte is written; the other bytes keep whatever the result slot held.
DvarValue Dvar_ParseValueWord(uint8 type, DvarLimits domain, const char *text)
{
    DvarValue out;

    switch (type) {
    case 0:
        out.enabled = (uint8)Dvar_ParseBool(text);
        break;
    case 1:
        out.value = Dvar_ParseFloat(text);
        break;
    case 2:
        out.vector = Dvar_ParseVec2(text);
        break;
    case 3:
        out.vector = Dvar_ParseVec3(text);
        break;
    case 4:
        out.vector = Dvar_ParseVec4(text);
        break;
    case 5:
        out.integer = Dvar_ParseInt(text);
        break;
    case 6:
        out.integer = Dvar_ParseEnumIndex(&domain.enumeration, text);
        break;
    case 7:
        out.string = text;
        break;
    case 8:
        Dvar_ParseColorRGBA(text, out.color);
        break;
    default:
        out.unsignedInt = 0;
        break;
    }
    return out;
}

/* ---------------------------------------------------------------------------
 * 0x080b1f06 / 0x080b1f44 / 0x080b1f82  size=61 each
 * ------------------------------------------------------------------------- */
// 0x080b1f06  size=61  Dvar_FormatCurrentValue  status=translated  naming=inferred
// naming: formats the current value word, bracketed by the same scratch-slot calls as its siblings.
const char *Dvar_FormatCurrentValue(const struct dvar_s *dvar)
{
    const char *result;

    Sys_EnterCriticalSection(4);
    result = Dvar_FormatValue(dvar, dvar->current.unsignedInt);
    Sys_LeaveCriticalSection(4);
    return result;
}

// 0x080b1f44  size=61  Dvar_FormatResetValue  status=translated  naming=inferred
// naming: formats the reset value word.
const char *Dvar_FormatResetValue(const struct dvar_s *dvar)
{
    const char *result;

    Sys_EnterCriticalSection(4);
    result = Dvar_FormatValue(dvar, dvar->reset.unsignedInt);
    Sys_LeaveCriticalSection(4);
    return result;
}

// 0x080b1f82  size=61  Dvar_FormatLatchedValue  status=translated  naming=inferred
// naming: formats the latched value word.
const char *Dvar_FormatLatchedValue(const struct dvar_s *dvar)
{
    const char *result;

    Sys_EnterCriticalSection(4);
    result = Dvar_FormatValue(dvar, dvar->latched.unsignedInt);
    Sys_LeaveCriticalSection(4);
    return result;
}

/* ---------------------------------------------------------------------------
 * 0x080b1fc0 / 0x080b2042  size=130 / size=118
 * ------------------------------------------------------------------------- */
// 0x080b1fc0  size=130  Dvar_ClampFloatArray  status=translated  naming=inferred
// naming: clamps each float of a vector to [lo, hi].
void Dvar_ClampFloatArray(float *values, int count, float lo, float hi)
{
    int32 i;

    for (i = 0; i < count; i++) {
        if (values[i] < lo) {
            values[i] = lo;
        } else if (hi < values[i]) {
            values[i] = hi;
        }
    }
}

// 0x080b2042  size=118  Dvar_AllFloatsInRange  status=translated  naming=inferred
// naming: returns 1 when every float of a vector lies within [lo, hi].
int Dvar_AllFloatsInRange(const float *values, int count, float lo, float hi)
{
    int32 i = 0;

    for (;;) {
        if (count <= i) {
            return 1;
        }
        if (values[i] < lo) {
            break;
        }
        if (hi < values[i]) {
            return 0;
        }
        i++;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * 0x080b20b8  size=291
 * Clamps a value word to its domain; type 6 substitutes the fallback for an
 * out-of-range index. Types 0, 7 and 8 are stored unchanged.
 * ------------------------------------------------------------------------- */
// 0x080b20b8  size=291  Dvar_ClampValue  status=translated  naming=inferred
// naming: clamps a dvar value to its domain according to the type code.
// Hidden aggregate-return pointer first (callee pops it); the domain words arrive by value.
DvarValue Dvar_ClampValue(uint8 type, uint32 value, uint32 fallback, DvarLimits limits)
{
    DvarValue out;
    uint32 lo = limits.integer.min;
    uint32 hi = limits.integer.max;

    switch (type) {
    case 1:
        if (w_float(value) < w_float(lo)) {
            value = lo;
        } else if (w_float(hi) < w_float(value)) {
            value = hi;
        }
        break;
    case 2:
        Dvar_ClampFloatArray((float *)w_ptr(value), 2, w_float(lo), w_float(hi));
        break;
    case 3:
        Dvar_ClampFloatArray((float *)w_ptr(value), 3, w_float(lo), w_float(hi));
        break;
    case 4:
        Dvar_ClampFloatArray((float *)w_ptr(value), 4, w_float(lo), w_float(hi));
        break;
    case 5:
        if (w_int(value) < w_int(lo)) {
            value = lo;
        } else if (w_int(hi) < w_int(value)) {
            value = hi;
        }
        break;
    case 6:
        if (w_int(value) < 0 || w_int(lo) <= w_int(value)) {
            value = fallback;
        }
        break;
    default:
        break;
    }
    out.unsignedInt = value;
    return out;
}

/* ---------------------------------------------------------------------------
 * 0x080b21dc  size=383
 * ------------------------------------------------------------------------- */
// 0x080b21dc  size=383  Dvar_ValueInDomain  status=translated  naming=inferred
// naming: tests whether a value word lies in its type's domain.
int Dvar_ValueInDomain(uint8 type, uint32 value, uint32 lo, uint32 hi)
{
    switch (type) {
    case 0:
        return 1;
    case 1:
        if (w_float(value) < w_float(lo)) {
            return 0;
        }
        if (w_float(hi) < w_float(value)) {
            return 0;
        }
        return 1;
    case 2:
        return Dvar_AllFloatsInRange((const float *)w_ptr(value), 2, w_float(lo), w_float(hi));
    case 3:
        return Dvar_AllFloatsInRange((const float *)w_ptr(value), 3, w_float(lo), w_float(hi));
    case 4:
        return Dvar_AllFloatsInRange((const float *)w_ptr(value), 4, w_float(lo), w_float(hi));
    case 5:
        if (w_int(value) < w_int(lo)) {
            return 0;
        }
        if (w_int(hi) < w_int(value)) {
            return 0;
        }
        return 1;
    case 6: {
        int ok = 0;
        if ((-1 < w_int(value) && w_int(value) < w_int(lo)) || w_float(value) == 0.0f) {
            ok = 1;
        }
        return ok;
    }
    case 7:
        return 1;
    case 8:
        return 1;
    default:
        return 0;
    }
}

/* ---------------------------------------------------------------------------
 * 0x080b235c  size=255
 * ------------------------------------------------------------------------- */
// 0x080b235c  size=255  Dvar_DomainVectorText  status=translated  naming=inferred
// naming: writes the 'Domain is any %iD vector' description, with the sentinel bounds of +/-FLT_MAX.
void Dvar_DomainVectorText(int dims, float lo, float hi, char *buf, size_t size)
{
    if (lo != -3.4028235e+38f) {
        if (hi != 3.4028235e+38f) {
            snprintf(buf, size, "Domain is any %iD vector with components from %g to %g", dims,
                     (double)lo, (double)hi);
        } else {
            snprintf(buf, size, "Domain is any %iD vector with components %g or bigger", dims, (double)lo);
        }
    } else if (hi != 3.4028235e+38f) {
        snprintf(buf, size, "Domain is any %iD vector with components %g or smaller", dims, (double)hi);
    } else {
        snprintf(buf, size, "Domain is any %iD vector", dims);
    }
}

/* ---------------------------------------------------------------------------
 * 0x080b245c  size=885
 * Writes a human-readable domain description for a dvar type. Types 5 use
 * integer sentinels (INT_MIN lower, INT_MAX upper) compared on the raw word.
 * ------------------------------------------------------------------------- */
// 0x080b245c  size=885  Dvar_DomainText  status=translated  naming=inferred
// naming: writes the dvar domain description strings ('Domain is ...') for each type.
char *Dvar_DomainText(uint8 type, uint32 lo, uint32 hi, char *buf, size_t size, int32 *lineCount)
{
    char *out = buf;
    int written;
    int32 i;
    const char **names;
    size_t remaining;

    if (lineCount != NULL) {
        *lineCount = 0;
    }
    switch (type) {
    case 0:
        snprintf(out, size, "Domain is 0 or 1");
        break;
    case 1:
        if (w_float(lo) != -3.4028235e+38f) {
            if (w_float(hi) != 3.4028235e+38f) {
                snprintf(out, size, "Domain is any number from %g to %g", (double)w_float(lo), (double)w_float(hi));
            } else {
                snprintf(out, size, "Domain is any number %g or bigger", (double)w_float(lo));
            }
        } else if (w_float(hi) != 3.4028235e+38f) {
            snprintf(out, size, "Domain is any number %g or smaller", (double)w_float(hi));
        } else {
            snprintf(out, size, "Domain is any number");
        }
        break;
    case 2:
        Dvar_DomainVectorText(2, w_float(lo), w_float(hi), out, size);
        break;
    case 3:
        Dvar_DomainVectorText(3, w_float(lo), w_float(hi), out, size);
        break;
    case 4:
        Dvar_DomainVectorText(4, w_float(lo), w_float(hi), out, size);
        break;
    case 5:
        if (lo == 0x80000000u) {
            if (hi == 0x7fffffffu) {
                snprintf(out, size, "Domain is any integer");
            } else {
                snprintf(out, size, "Domain is any integer %i or smaller", w_int(hi));
            }
        } else if (hi == 0x7fffffffu) {
            snprintf(out, size, "Domain is any integer %i or bigger", w_int(lo));
        } else {
            snprintf(out, size, "Domain is any integer from %i to %i", w_int(lo), w_int(hi));
        }
        break;
    case 6:
        names = (const char **)w_ptr(hi);
        written = snprintf(out, size, "Domain is one of the following:");
        if (written >= 0) {
            out += written;
            for (i = 0; i < w_int(lo); i++) {
                /* Remaining space is (buf + size - out), evaluated modulo 2^32 as in the binary. */
                remaining = (size_t)((uint32)(uintptr_t)buf + (uint32)size - (uint32)(uintptr_t)out);
                written = snprintf(out, remaining, "\n  %2i: %s", i, names[i]);
                if (written < 0) {
                    break;
                }
                if (lineCount != NULL) {
                    *lineCount += 1;
                }
                out += written;
            }
        }
        break;
    case 7:
        snprintf(out, size, "Domain is any text");
        break;
    case 8:
        snprintf(out, size, "Domain is any 4-component color, in RGBA format");
        break;
    default:
        *out = '\0';
        break;
    }
    buf[size - 1] = '\0';
    return out;
}
