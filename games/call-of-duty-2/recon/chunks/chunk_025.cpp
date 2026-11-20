#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_025_types.h"

/* raw 32-bit view of a float (the original copies floats with integer moves, preserving signalling NaNs) */
#define W32(x) (*(uint32 *)&(x))

/* ---------------------------------------------------------------------------------------
 * Filesystem: pack lists, dvars, startup and restart.
 * ------------------------------------------------------------------------------------- */

// 0x080a2a10  size=97  FUN_080a2a10  status=translated  naming=inferred
// naming evidence: Q3 FS_AddGameDirectoryAllLanguages shape: loads numbered iwd slots 14..0, then the directory itself; callers pass fs_basepath/homepath and a fixed subdirectory
void FS_AddGameDirectoryAllLanguages(const char *basePath, const char *gameDir)
{
    int32 pakIndex;

    for (pakIndex = 14; pakIndex >= 0; pakIndex--) {
        FS_AddGameDirectory(basePath, gameDir, 1, pakIndex);
    }
    FS_AddGameDirectory(basePath, gameDir, 0, 0);
}

// 0x080a2a72  size=130  FUN_080a2a72  status=translated  naming=inferred
// naming evidence: descriptive: frees a singly linked list of pak nodes and their iwd records; called from FS_Shutdown with fs_searchpaths
void FS_FreePakList(struct searchpath_s *node)
{
    struct searchpath_s *next;

    while (node != NULL) {
        next = node->next;
        if (node->iwd != NULL) {
            unzClose(node->iwd->handle);
            Com_Free_A(node->iwd->buildBuffer);
            Com_Free_A(node->iwd);
        }
        if (node->dir != NULL) {
            Com_Free_A(node->dir);
        }
        Com_Free_A(node);
        node = next;
    }
}

// 0x080a2af4  size=90  FUN_080a2af4  status=translated  naming=inferred
// naming evidence: descriptive: frees each pointer in fs_serverIwdNames and resets the count; table size 1024 inferred from the adjacent globals
void FS_ClearListA(void)
{
    int32 i;

    for (i = 0; i < fs_numServerIwds; i++) {
        if (fs_serverIwdNames[i] != NULL) {
            Com_Free_A(fs_serverIwdNames[i]);
        }
        fs_serverIwdNames[i] = NULL;
    }
    fs_numServerIwds = 0;
}

// 0x080a2b4e  size=90  FUN_080a2b4e  status=translated  naming=inferred
// naming evidence: descriptive: same teardown as FS_ClearListA for fs_referencedIwdNames
void FS_ClearListB(void)
{
    int32 i;

    for (i = 0; i < fs_numReferencedIwds; i++) {
        if (fs_referencedIwdNames[i] != NULL) {
            Com_Free_A(fs_referencedIwdNames[i]);
        }
        fs_referencedIwdNames[i] = NULL;
    }
    fs_numReferencedIwds = 0;
}

// 0x080a2ba8  size=147  FUN_080a2ba8  status=translated  naming=inferred
// naming evidence: Q3 FS_Shutdown shape: closes open handles, frees pack list, removes the fs commands path/fullpath/dir/fdir/touchFile (rodata strings)
void FS_Shutdown(void)
{
    int32 handle;

    LocTable_DestroyThunk();
    for (handle = 1; handle < 0x40; handle++) {
        if (fsh[handle].fileSize != 0) {
            FS_FCloseFile(handle);
        }
    }
    FS_FreePakList(fs_searchpaths);
    fs_searchpaths = NULL;
    Cmd_RemoveCommand("path");
    Cmd_RemoveCommand("fullpath");
    Cmd_RemoveCommand("dir");
    Cmd_RemoveCommand("fdir");
    Cmd_RemoveCommand("touchFile");
}

// 0x080a2c3c  size=420  FUN_080a2c3c  status=translated  naming=inferred
// naming evidence: dvar names (fs_debug, fs_copyfiles, fs_cdpath, fs_basepath, fs_basegame, fs_useOldAssets, fs_homepath, fs_game, fs_restrict, fs_ignoreLocalized) in rodata; registration runs once (guard is fs_debug)
int32 FS_RegisterDvars(void)
{
    const char *defaultCdPath;
    const char *defaultBasePath;
    const char *homeDefault;

    if (fs_debug == NULL) {
        fs_debug = Dvar_RegisterInt("fs_debug", 0, 0, 2, 0x1000);
        fs_copyfiles = Dvar_RegisterBool("fs_copyfiles", 0, 0x1010);
        defaultCdPath = Sys_DefaultCDPath();
        fs_cdpath = Dvar_RegisterString("fs_cdpath", defaultCdPath, 0x1010);
        defaultBasePath = Sys_DefaultBasePath();
        fs_basepath = Dvar_RegisterString("fs_basepath", defaultBasePath, 0x1010);
        fs_basegameDvar = Dvar_RegisterString("fs_basegame", "", 0x1010);
        fs_useOldAssetsDvar = Dvar_RegisterBool("fs_useOldAssets", 0, 0x1000);
        homeDefault = Sys_DefaultHomePath();
        if (homeDefault == NULL || *homeDefault == '\0') {
            homeDefault = fs_basepath->current.string;
        }
        fs_homepath = Dvar_RegisterString("fs_homepath", homeDefault, 0x1010);
        fs_gameDvar = Dvar_RegisterString("fs_game", "", 0x101c);
        fs_restrict = Dvar_RegisterBool("fs_restrict", 0, 0x1010);
        fs_ignoreLocalized = Dvar_RegisterBool("fs_ignoreLocalized", 0, 0x10a0);
        return 1;
    }
    return 0;
}

// 0x080a2de0  size=1090  FUN_080a2de0  status=translated  naming=observed
// naming evidence: string "----- FS_Startup -----" names this function; the "%d files in iwd files" message follows the load
void FS_Startup(const char *gameName)
{
    Com_Printf("----- FS_Startup -----\n");
    fs_iwdFileCount = 0;
    FS_RegisterDvars();

    if (fs_useOldAssetsDvar->current.enabled != 0) {
        if (*fs_basepath->current.string != '\0') {
            FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, "tempcod");
        }
        if (*fs_homepath->current.string != '\0') {
            FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, "tempcod");
        }
    }
    if (*fs_basepath->current.string != '\0') {
        FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, "devraw_shared");
        FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, "devraw");
        FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, "raw_shared");
        FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, "raw");
    }
    if (*fs_homepath->current.string != '\0') {
        FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, "devraw_shared");
        FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, "devraw");
        FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, "raw_shared");
        FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, "raw");
    }
    if (*fs_cdpath->current.string != '\0') {
        FS_AddGameDirectoryAllLanguages(fs_cdpath->current.string, "devraw_shared");
        FS_AddGameDirectoryAllLanguages(fs_cdpath->current.string, "devraw");
        FS_AddGameDirectoryAllLanguages(fs_cdpath->current.string, "raw_shared");
        FS_AddGameDirectoryAllLanguages(fs_cdpath->current.string, "raw");
        FS_AddGameDirectoryAllLanguages(fs_cdpath->current.string, gameName);
    }
    if (*fs_basepath->current.string != '\0') {
        FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, gameName);
    }
    if (*fs_basepath->current.string != '\0') {
        if (Q_stricmp(fs_homepath->current.string, fs_basepath->current.string) != 0) {
            FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, gameName);
        }
    }

    /* Base-game override ("fs_basegame") applies when the requested game is "main". */
    if (*fs_basegameDvar->current.string != '\0') {
        if (Q_stricmp(gameName, "main") == 0) {
            if (Q_stricmp(fs_basegameDvar->current.string, gameName) != 0) {
                if (*fs_cdpath->current.string != '\0') {
                    FS_AddGameDirectoryAllLanguages(fs_cdpath->current.string, fs_basegameDvar->current.string);
                }
                if (*fs_basepath->current.string != '\0') {
                    FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, fs_basegameDvar->current.string);
                }
                if (*fs_homepath->current.string != '\0') {
                    if (Q_stricmp(fs_homepath->current.string, fs_basepath->current.string) != 0) {
                        FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, fs_basegameDvar->current.string);
                    }
                }
            }
        }
    }

    /* Mod directory ("fs_game") applies when the requested game is "main". */
    if (*fs_gameDvar->current.string != '\0') {
        if (Q_stricmp(gameName, "main") == 0) {
            if (Q_stricmp(fs_gameDvar->current.string, gameName) != 0) {
                if (*fs_cdpath->current.string != '\0') {
                    FS_AddGameDirectoryAllLanguages(fs_cdpath->current.string, fs_gameDvar->current.string);
                }
                if (*fs_basepath->current.string != '\0') {
                    FS_AddGameDirectoryAllLanguages(fs_basepath->current.string, fs_gameDvar->current.string);
                }
                if (*fs_homepath->current.string != '\0') {
                    if (Q_stricmp(fs_homepath->current.string, fs_basepath->current.string) != 0) {
                        FS_AddGameDirectoryAllLanguages(fs_homepath->current.string, fs_gameDvar->current.string);
                    }
                }
            }
        }
    }

    FS_AddCommands();
    FS_PrintPureSearchPaths();
    Dvar_ClearModified(fs_gameDvar);
    Com_Printf("----------------------\n");
    Com_Printf("%d files in iwd files\n", fs_iwdFileCount);
}

// 0x080a3222  size=56  FUN_080a3222  status=translated  naming=inferred
// naming evidence: descriptive: clears the per-pack reference flag (+0x310) of every loaded pack
void FS_ClearPakReferences(void)
{
    struct searchpath_s *node;

    for (node = fs_searchpaths; node != NULL; node = node->next) {
        if (node->iwd != NULL) {
            node->iwd->referenced = 0;
        }
    }
}

// 0x080a325a  size=288  FUN_080a325a  status=translated  naming=inferred
// naming evidence: Q3 FS_InitFilesystem shape: resets fs dvars, starts "main", loads default_localize_mp.cfg or default_mp.cfg, saves base path and game
void FS_InitFilesystem(void)
{
    Com_StartupVariable("fs_cdpath");
    Com_StartupVariable("fs_basepath");
    Com_StartupVariable("fs_homepath");
    Com_StartupVariable("fs_game");
    Com_StartupVariable("fs_copyfiles");
    Com_StartupVariable("fs_restrict");
    Com_StartupVariable("loc_language");
    Loc_RegisterDvars();
    FS_Startup("main");
    Loc_CreateStringTable();
    Loc_InitLanguages();
    FS_SetRestrictedDemoMode();

    if (FS_ReadFile("default_localize_mp.cfg", 0) < 1) {
        if (FS_ReadFile("default_mp.cfg", 0) < 1) {
            Com_Error(0, "Couldn't load %s.  Make sure Call of Duty is run from the correct folder.",
                        "default_localize_mp.cfg");
        } else {
            fs_defaultConfigName = "default_mp.cfg";
        }
    } else {
        fs_defaultConfigName = "default_localize_mp.cfg";
    }
    Q_strncpyz(fs_savedBasePath, fs_basepath->current.string, 0x100);
    Q_strncpyz(fs_savedGame, fs_gameDvar->current.string, 0x100);
}

// 0x080a337a  size=388  FUN_080a337a  status=translated  naming=inferred
// naming evidence: Q3 FS_Restart shape (takes a checksum, stores it as the feed); "Invalid game folder" error path restores the saved base path and recurses once
void FS_Restart(int32 checksum)
{
    FS_Shutdown();
    fs_checksumFeed = checksum;
    FS_ClearPakReferences();
    FS_Startup("main");
    Loc_CreateStringTable();
    Loc_InitLanguages();
    FS_SetRestrictedDemoMode();

    if (FS_ReadFile(fs_defaultConfigName, 0) < 1) {
        if (fs_savedBasePath[0] != '\0') {
            FS_PureServerSetLoadedIwds("", "");
            Dvar_SetString(fs_basepath, fs_savedBasePath);
            Dvar_SetString(fs_gameDvar, fs_savedGame);
            fs_savedBasePath[0] = '\0';
            fs_savedGame[0] = '\0';
            Dvar_SetBool(fs_restrict, 0);
            FS_Restart(checksum);
            Com_Error(1, "Invalid game folder\n");
        }
        Com_Error(0, "Couldn't load %s.  Make sure Call of Duty is run from the correct folder.",
                    fs_defaultConfigName);
    }

    if (Q_stricmp(fs_gameDvar->current.string, fs_savedGame) != 0) {
        if (Com_SafeMode() == 0) {
            const char *execCommand = va("exec %s\n", "config_mp_server.cfg");
            Cbuf_AddText(execCommand);
        }
    }
    Q_strncpyz(fs_savedBasePath, fs_basepath->current.string, 0x100);
    Q_strncpyz(fs_savedGame, fs_gameDvar->current.string, 0x100);
}

// 0x080a34fe  size=100  FUN_080a34fe  status=translated  naming=inferred
// naming evidence: descriptive: restart only when fs_game was modified or the checksum changed; returns 1 when a restart ran
int32 FS_ConditionalRestart(int32 checksum)
{
    if (com_sv_running->current.enabled != 0) {
        return 0;
    }
    if (fs_gameDvar->modified == 0) {
        if (checksum == fs_checksumFeed) {
            return 0;
        }
    }
    FS_Restart(checksum);
    return 1;
}

// 0x080a3562  size=303  FUN_080a3562  status=translated  naming=observed
// naming evidence: string "FSH_FOpenFile: bad mode" names this function; modes 0 read, 1 write, 2 append, 3 append-sync (Q3 fsMode_t order)
int32 FSH_FOpenFile(const char *filename, int32 *fileHandle, int32 mode)
{
    int32 result = 0x1b39;
    qboolean handleSync = 0;

    if (mode == 1) {
        *fileHandle = FS_FOpenFileWrite(filename);
        result = 0;
        if (*fileHandle == 0) {
            result = -1;
        }
    } else if (mode == 0) {
        result = FS_OpenFileRead(filename, fileHandle, 1);
    } else if (mode == 2 || mode == 3) {
        if (mode == 3) {
            handleSync = 1;
        }
        *fileHandle = FS_FOpenFileAppend(filename);
        result = 0;
        if (*fileHandle == 0) {
            result = -1;
        }
    } else {
        Com_Error(0, "FSH_FOpenFile: bad mode");
    }

    if (fileHandle != NULL) {
        if (*fileHandle != 0) {
            fsh[*fileHandle].fileSize = result;
            fsh[*fileHandle].streamed = 0;
        }
        fsh[*fileHandle].handleSync = handleSync;
    }
    return result;
}

// 0x080a36e2  size=27  FUN_080a36e2  status=translated  naming=inferred
// naming evidence: descriptive: flushes the stdio stream behind a file handle
void FS_Flush(int32 fileHandle)
{
    FILE *stream = (FILE *)FS_FileForHandle(fileHandle);
    fflush(stream);
}

// 0x080a36fe  size=73  FUN_080a36fe  status=translated  naming=inferred
// naming evidence: strings "gfx_driver", "d3dbsp", "%sbsp": returns "d3dbsp" when gfx_driver is empty, else "<driver>bsp"
const char *Gfx_GetBspVariant(void)
{
    const char *driver = Dvar_GetString("gfx_driver");

    if (*driver == '\0') {
        return va("d3dbsp");
    }
    return va("%sbsp", driver);
}

// 0x080a3748  size=8  FUN_080a3748  status=translated  naming=unknown
// naming evidence: returns its argument unchanged; no string or call evidence of intent
int32 Int_Identity_080a3748(int32 value)
{
    return value;
}

// 0x080a3750  size=34  FUN_080a3750  status=translated  naming=inferred
// naming evidence: __xstat(3, ...) is the glibc i386 stat() entry (_STAT_VER 3); result is discarded by the caller
extern "C" int __xstat(int ver, const char *path, struct stat *buf);
int FS_StatPath(const char *path, void *statBuf)
{
    struct stat *buf = (struct stat *)statBuf;
    /* the original calls glibc's __xstat(3, ...) directly (i386 _STAT_VER_LINUX) and returns its result */
    return __xstat(3, path, buf);
}

/* Returns rand() / 2^31 without rounding to float: the x87 st(0) result is kept unrounded. */
// 0x080a3774  size=29  FUN_080a3774  status=translated  naming=unknown
// naming evidence: rand() divided by the float constant 2^31 (rodata 0x0814cfe4); descriptive name only
long double Rand_Fraction(void)
{
    int32 r = rand();
    return (long double)r / (long double)2147483648.0f;
}

// 0x080a3792  size=19  FUN_080a3792  status=translated  naming=unknown
// naming evidence: 2 * Rand_Fraction() - 1 (unrounded x87 chain)
long double Rand_Signed(void)
{
    long double v = Rand_Fraction();
    return (v + v) - 1.0L;
}

// 0x080a3bca  size=156  FUN_080a3bca  status=translated  naming=unknown
// naming evidence: steps current toward target by rate*frameTime (sign from target-current); returns target when within one step or when |diff| <= 0.001
float Float_StepTowards(float target, float current, float rate, float frameTime)
{
    float diff = target - current;
    float step;
    float result;

    if (diff <= 0.0) {
        rate = -rate;
    }
    step = rate * frameTime;
    if (0.001 < Float_Abs_080aaea4(diff)) {
        if (Float_Abs_080aaea4(diff) < Float_Abs_080aaea4(step)) {
            result = target;
        } else {
            result = current + step;
        }
    } else {
        result = target;
    }
    return result;
}

// 0x080a3c66  size=132  FUN_080a3c66  status=translated  naming=unknown
// naming evidence: wraps target-current into [-180, 180], steps with Float_StepTowards, stores through AngleNormalize180
float Angle_StepTowardsWrapped(float target, float current, float rate, float frameTime)
{
    /* x87: the original keeps target in its argument slot, so every step rounds to float */
    volatile float tgt = target;
    for (; 180.0 < tgt - current; tgt = tgt - 360.0) {
    }
    for (; tgt - current < -180.0; tgt = tgt + 360.0) {
    }
    return AngleNormalize180(Float_StepTowards(tgt, current, rate, frameTime));
}

// 0x080a3cea  size=124  FUN_080a3cea  status=translated  naming=unknown
// naming evidence: step = gain * (target-current) * frameTime; same clamp rule as Float_StepTowards
float Float_ApproachProportional(float target, float current, float gain, float frameTime)
{
    volatile float diff = target - current;   /* x87: stored as float, like the original */
    volatile float step = gain * diff * frameTime;
    float result;

    if (0.001 < Float_Abs_080aaea4(diff)) {
        if (Float_Abs_080aaea4(diff) < Float_Abs_080aaea4(step)) {
            result = target;
        } else {
            result = current + step;
        }
    } else {
        result = target;
    }
    return result;
}

// 0x080a3d66  size=132  FUN_080a3d66  status=translated  naming=unknown
// naming evidence: wraps the difference into [-180, 180], then Float_ApproachProportional, stored through AngleNormalize180
float Angle_ApproachProportionalWrapped(float target, float current, float gain, float frameTime)
{
    /* x87: the original keeps target in its argument slot, so every step rounds to float */
    volatile float tgt = target;
    for (; 180.0 < tgt - current; tgt = tgt - 360.0) {
    }
    for (; tgt - current < -180.0; tgt = tgt + 360.0) {
    }
    return AngleNormalize180(Float_ApproachProportional(tgt, current, gain, frameTime));
}

// 0x080a3dea  size=187  FUN_080a3dea  status=translated  naming=inferred
// naming evidence: piecewise-linear lookup over (x, y) samples; -1 when x is past the last sample
float Curve_EvalPiecewise(int32 count, const struct cod2_curve_point_s *table, float x)
{
    volatile float result = -1.0f;
    int32 i = 1;

    for (;;) {
        if (count <= i) {
            return result;
        }
        if (x <= table[i].x) {
            /* x87: the original stores the ratio as a 32-bit float before the multiply */
            volatile float t = (x - table[i - 1].x) / (table[i].x - table[i - 1].x);
            result = table[i - 1].y + (table[i].y - table[i - 1].y) * t;
            return result;
        }
        i++;
    }
}

// 0x080a3ea6  size=37  FUN_080a3ea6  status=translated  naming=unknown
// naming evidence: number of right shifts before the value reaches zero (floor log2 for positive input)
int32 Int_Log2Floor(int32 value)
{
    int32 count = 0;

    while (value = value >> 1, value != 0) {
        count++;
    }
    return count;
}

// 0x080a3ecc  size=91  FUN_080a3ecc  status=translated  naming=inferred
// naming evidence: acos clamped to +-pi; the lower-bound branch stores +pi (rodata 0x0814d040 in both branches), preserved as observed
float Math_AcosClamped(float x)
{
    double angle = acos((double)x);
    float result = (float)angle;

    if (result > 3.141592653589793) {
        result = 3.1415927f;
    } else if (result < -3.141592653589793) {
        result = 3.1415927f;
    }
    return result;
}

// 0x080a3f28  size=48  FUN_080a3f28  status=translated  naming=unknown
// naming evidence: clamps an int to the signed 8-bit range [-128, 127]
int32 Clamp_Int8(int32 value)
{
    int32 result;

    if (value < -0x80) {
        result = -0x80;
    } else if (value < 0x80) {
        result = (int32)(int8)value;
    } else {
        result = 0x7f;
    }
    return result;
}

// 0x080a3f58  size=54  FUN_080a3f58  status=translated  naming=unknown
// naming evidence: clamps an int to the signed 16-bit range
int32 Clamp_Int16(int32 value)
{
    int32 result;

    if (value < -0x8000) {
        result = -0x8000;
    } else if (value < 0x8000) {
        result = (int32)(int16)value;
    } else {
        result = 0x7fff;
    }
    return result;
}

// 0x080a3f8e  size=130  FUN_080a3f8e  status=translated  naming=inferred
// naming evidence: best-dot-product index into the 162-entry unit-normal table bytedirs; returns 0 for a NULL direction
uint8 Normal_DirToIndex(const float *dir)
{
    uint8 bestIndex = 0;
    volatile float bestDot = 0.0f;   /* x87: the original keeps these in 32-bit stack slots */
    uint8 i;

    if (dir == NULL) {
        return 0;
    }
    bestDot = 0.0f;
    for (i = 0; i < 0xa2; i++) {
        volatile float d = Vec3_Dot_080ab260(dir, &bytedirs[(uint32)i * 3]);
        if (bestDot < d) {
            bestIndex = i;
            bestDot = d;
        }
    }
    return bestIndex;
}

// 0x080a4010  size=78  FUN_080a4010  status=translated  naming=inferred
// naming evidence: copies unit normal `index` from bytedirs (index 0..161), or the zero vector out of range
void Normal_IndexToDir(int32 index, float *dir)
{
    if (index < 0 || 0xa1 < index) {
        Vec3_Copy_080ab0d8(vec3_origin, dir);
    } else {
        Vec3_Copy_080ab0d8(&bytedirs[index * 3], dir);
    }
}

// 0x080a405e  size=139  FUN_080a405e  status=translated  naming=inferred
// naming evidence: returns 1 when every component difference of a and b over `count` components lies within radius
qboolean Vec_AllComponentsWithin(const float *a, const float *b, float radius, int32 count)
{
    int32 i = 0;

    for (;;) {
        float d;
        if (count <= i) {
            return 1;
        }
        d = a[i] - b[i];
        if (radius * radius < d * d) {
            return 0;
        }
        i++;
    }
}

// 0x080a40ea  size=44  FUN_080a40ea  status=translated  naming=unknown
// naming evidence: delta = p2 - p1 (3 components), then passed to Vec3_Length; semantics of Vec3_Length unknown
float Vec3_Distance(const float *p1, const float *p2)
{
    float delta[6];   /* original scratch is 24 bytes; callee may read beyond xyz */

    Vec3_Sub(p2, p1, delta);
    return Vec3_Length(delta);
}

// 0x080a4116  size=55  FUN_080a4116  status=translated  naming=unknown
// naming evidence: squared length of (p2 - p1) in 3D, summed x*x + y*y + z*z (ascending, verified against objdump 0x080a4116)
float Vec3_DistanceSquared(const float *p1, const float *p2)
{
    float delta[6];

    Vec3_Sub(p2, p1, delta);
    return delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
}

// 0x080a414e  size=146  FUN_080a414e  status=translated  naming=inferred
// naming evidence: squared distance from point to origin + clamp(dot(point-origin, dir), 0, maxT) * dir
float Ray_ClampedDistanceSq(const float *point, const float *origin, const float *dir, float maxT)
{
    float delta[6];
    float clamped;
    float probe[4];

    Vec3_Sub(point, origin, delta);
    clamped = Vec3_Dot_080ab260(delta, dir);
    clamped = Float_Select2(clamped, 0.0f, maxT);
    Vec3_MA(origin, clamped, dir, probe);
    return Vec3_DistanceSquared(point, probe);
}

// 0x080a41e0  size=44  FUN_080a41e0  status=translated  naming=unknown
// naming evidence: 2D counterpart of Vec3_Distance
void Vec2_Distance(const float *p1, const float *p2)
{
    float delta[4];

    Vec2_Sub(p2, p1, delta);
    Vec2_Length(delta);
}

// 0x080a420c  size=47  FUN_080a420c  status=translated  naming=unknown
// naming evidence: squared length of a 2D difference
float Vec2_DistanceSquared(const float *p1, const float *p2)
{
    float delta[2];

    Vec2_Sub(p2, p1, delta);
    return delta[1] * delta[1] + delta[0] * delta[0];
}

// 0x080a423c  size=116  FUN_080a423c  status=translated  naming=inferred
// naming evidence: standard 3D cross product out = a x b (writes in element order, aliasing-faithful)
void Vec3_Cross(const float *a, const float *b, float *out)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

// 0x080a42b0  size=86  FUN_080a42b0  status=translated  naming=unknown
// naming evidence: index (0..2) of the largest absolute component, ties resolved toward the lower index
int32 Vec3_LargestAxis(const float *v)
{
    uint32 best;
    float absv[3];

    Vec3_Mul(v, v, absv);
    best = (uint32)(absv[0] < absv[1]);
    if (absv[best] < absv[2]) {
        best = 2;
    }
    return (int32)best;
}

// 0x080a4306  size=285  FUN_080a4306  status=translated  naming=unknown
// naming evidence: orders the two axes other than the dominant one by sign and magnitude
void Vec3_DominantAxisOrder(const float *v, int32 *first, int32 *second)
{
    float absv[3];

    Vec3_Mul(v, v, absv);
    /* positive (>=) tests: with NaN components the original's jae/ja (fucompp+sahf) falls through to the next case */
    if (absv[2] >= absv[0] && absv[2] >= absv[1]) {
        if (0.0f < v[2]) {
            *first = 0;
            *second = 1;
        } else {
            *first = 1;
            *second = 0;
        }
    } else if (absv[1] >= absv[0] && absv[1] >= absv[2]) {
        if (0.0f < v[1]) {
            *first = 2;
            *second = 0;
        } else {
            *first = 0;
            *second = 2;
        }
    } else if (0.0f < v[0]) {
        *first = 1;
        *second = 2;
    } else {
        *first = 2;
        *second = 1;
    }
}

// 0x080a4424  size=157  FUN_080a4424  status=translated  naming=inferred
// naming evidence: normalizes a 3-vector in place, returns the original length; zero vectors are left unchanged
float Vec3_Normalize(float *v)
{
    /* x87: the original stores length and inv as 32-bit floats (fstp/fld) */
    volatile float length = Float_Sqrt_080aaebe(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    volatile float inv;

    if (length != 0.0f || length != length) {
        inv = 1.0 / length;
        v[0] = v[0] * inv;
        v[1] = v[1] * inv;
        v[2] = v[2] * inv;
    }
    return length;
}

// 0x080a44c2  size=120  FUN_080a44c2  status=translated  naming=inferred
// naming evidence: normalizes a 2-vector in place, returns the original length
float Vec2_Normalize(float *v)
{
    /* x87: the original stores length and inv as 32-bit floats (fstp/fld) */
    volatile float length = Float_Sqrt_080aaebe(v[0] * v[0] + v[1] * v[1]);
    volatile float inv;

    if (length != 0.0f || length != length) {
        inv = 1.0 / length;
        v[0] = v[0] * inv;
        v[1] = v[1] * inv;
    }
    return length;
}

// 0x080a453a  size=194  FUN_080a453a  status=translated  naming=inferred
// naming evidence: normalizes a 4-vector in place, returns the original length
float Vec4_Normalize(float *v)
{
    /* x87: the original stores length and inv as 32-bit floats (fstp/fld) */
    volatile float length = Float_Sqrt_080aaebe(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
    volatile float inv;

    if (length != 0.0f || length != length) {
        inv = 1.0 / length;
        v[0] = v[0] * inv;
        v[1] = v[1] * inv;
        v[2] = v[2] * inv;
        v[3] = v[3] * inv;
    }
    return length;
}

// 0x080a45fc  size=170  FUN_080a45fc  status=translated  naming=inferred
// naming evidence: out = normalized(in); zeroes out for a zero-length input; returns the length
float Vec3_NormalizeCopy(const float *in, float *out)
{
    /* x87: the original stores length and inv as 32-bit floats (fstp/fld) */
    volatile float length = Float_Sqrt_080aaebe(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
    volatile float inv;

    if (length != 0.0f || length != length) {
        inv = 1.0 / length;
        out[0] = in[0] * inv;
        out[1] = in[1] * inv;
        out[2] = in[2] * inv;
    } else {
        Vec3_Clear_080ab08a(out);
    }
    return length;
}

// 0x080a46a6  size=133  FUN_080a46a6  status=translated  naming=inferred
// naming evidence: 2-vector form of Vec3_NormalizeCopy
float Vec2_NormalizeCopy(const float *in, float *out)
{
    /* x87: the original stores length and inv as 32-bit floats (fstp/fld) */
    volatile float length = Float_Sqrt_080aaebe(in[0] * in[0] + in[1] * in[1]);
    volatile float inv;

    if (length != 0.0f || length != length) {
        inv = 1.0 / length;
        out[0] = in[0] * inv;
        out[1] = in[1] * inv;
    } else {
        Vec2_Clear(out);
    }
    return length;
}

// 0x080a472c  size=118  FUN_080a472c  status=translated  naming=inferred
// naming evidence: packs three floats scaled by 511 into 10-bit fields (x at 0, y at 10, z at 20)
uint32 Vec3_PackDir10(const float *v)
{
    uint32 x = Float_RoundToInt(v[0] * 511.0);
    uint32 y = Float_RoundToInt(v[1] * 511.0);
    uint32 z = Float_RoundToInt(v[2] * 511.0);

    return ((y & 0x3ff) << 10) | (x & 0x3ff) | ((z & 0x3ff) << 0x14);
}

// 0x080a47a2  size=119  FUN_080a47a2  status=translated  naming=inferred
// naming evidence: inverse of Vec3_PackDir10: sign-extended 10-bit fields times 1/511
void Vec3_UnpackDir10(uint32 packed, float *out)
{
    int32 p = (int32)packed;

    out[0] = (float)((p << 0x16) >> 0x16) * 0.001956947147846222f; /* rodata 0x0814d054 is a float */
    out[1] = (float)(((p >> 10) << 0x16) >> 0x16) * 0.001956947147846222f; /* rodata 0x0814d054 is a float */
    out[2] = (float)(((p >> 0x14) << 0x16) >> 0x16) * 0.001956947147846222f; /* rodata 0x0814d054 is a float */
}

// 0x080a481a  size=103  FUN_080a481a  status=translated  naming=unknown
// naming evidence: maximum of three components (ties keep the earlier one)
float Vec3_MaxComponent(const float *v)
{
    float first;
    float result;

    if (v[1] <= v[0]) {
        first = v[0];
    } else {
        first = v[1];
    }
    if (v[2] <= first) {
        result = first;
    } else {
        result = v[2];
    }
    return result;
}

// 0x080a4882  size=170  FUN_080a4882  status=translated  naming=inferred
// naming evidence: out[j] = sum_k v[k] * m[3j+k] (k summed from 2 down to 0)
void Vec3_MulMat3Rows(const float *v, const float *m, float *out)
{
    out[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2];
    out[1] = v[0] * m[3] + v[1] * m[4] + v[2] * m[5];
    out[2] = v[0] * m[6] + v[1] * m[7] + v[2] * m[8];
}

// 0x080a492c  size=170  FUN_080a492c  status=translated  naming=inferred
// naming evidence: out[j] = sum_k v[k] * m[3k+j] (row vector times 3x3)
void Vec3_MulMat3Cols(const float *v, const float *m, float *out)
{
    out[0] = v[0] * m[0] + v[1] * m[3] + v[2] * m[6];
    out[1] = v[0] * m[1] + v[1] * m[4] + v[2] * m[7];
    out[2] = v[0] * m[2] + v[1] * m[5] + v[2] * m[8];
}

// 0x080a49d6  size=613  FUN_080a49d6  status=translated  naming=inferred
// naming evidence: rotates v about the axis vector by `degrees` using the frame M = [perp | cross | axis] and M*Rz*M^T (Rz = [[c,s,0],[-s,c,0],[0,0,1]])
void Vec3_RotateAroundAxis(float *out, const float *axis, const float *v, float degrees)
{
    float axisCopy[3];
    float perp[3];
    float cross[3];
    float m[9];
    float mt[9];
    float rot[9];
    float mr[9];
    float f[9];
    int32 i;

    /* The original moves these as 32-bit integers (mov), so signalling NaNs are preserved. */
    W32(axisCopy[0]) = W32(axis[0]);
    W32(axisCopy[1]) = W32(axis[1]);
    W32(axisCopy[2]) = W32(axis[2]);
    Vec3_PerpendicularVector(axis, perp);
    Vec3_Cross(perp, axisCopy, cross);

    W32(m[0]) = W32(perp[0]);
    W32(m[3]) = W32(perp[1]);
    W32(m[6]) = W32(perp[2]);
    W32(m[1]) = W32(cross[0]);
    W32(m[4]) = W32(cross[1]);
    W32(m[7]) = W32(cross[2]);
    W32(m[2]) = W32(axisCopy[0]);
    W32(m[5]) = W32(axisCopy[1]);
    W32(m[8]) = W32(axisCopy[2]);

    /* mt = transpose(m); the original copies m (memcpy 0x24) then overwrites six entries with the transposed values. */
    memcpy(mt, m, 0x24);
    W32(mt[1]) = W32(m[3]);
    W32(mt[2]) = W32(m[6]);
    W32(mt[3]) = W32(m[1]);
    W32(mt[5]) = W32(m[7]);
    W32(mt[6]) = W32(m[2]);
    W32(mt[7]) = W32(m[5]);

    memset(rot, 0, 0x24);
    rot[8] = 1.0f;
    rot[4] = 1.0f;
    rot[0] = 1.0f;
    SinCos_Float((float)(degrees * 0.017453292519943295), &rot[1], &rot[0]);
    {
        W32(rot[3]) = W32(rot[1]) ^ 0x80000000u;   /* xor sign bit, as the original */
    }
    W32(rot[4]) = W32(rot[0]);

    Mat3_Multiply(m, rot, mr);
    Mat3_Multiply(mr, mt, f);
    for (i = 0; i < 3; i++) {
        out[i] = f[i * 3] * v[0] + f[i * 3 + 1] * v[1] + f[i * 3 + 2] * v[2];
    }
}

// 0x080a4cc2  size=151  FUN_080a4cc2  status=translated  naming=inferred
// naming evidence: builds a unit vector perpendicular to normal (scratch = (n[2], -n[0], n[1]) reduced by its normal component), then out = scratch x normal
void Vec3_PerpendicularBasis(const float *normal, float *scratch, float *out)
{
    volatile float d;   /* x87: float stack slot */
    float negD;

    /* integer moves / sign-bit xor as in the original (objdump 0x080a4cc2) */
    W32(scratch[1]) = W32(((float *)normal)[0]) ^ 0x80000000u;
    W32(scratch[2]) = W32(((float *)normal)[1]);
    W32(scratch[0]) = W32(((float *)normal)[2]);
    d = Vec3_Dot_080ab260(scratch, normal);
    W32(negD) = *(volatile unsigned int *)&d ^ 0x80000000u;
    Vec3_MA(scratch, negD, normal, scratch);
    Vec3_Normalize(scratch);
    Vec3_Cross(scratch, normal, out);
}

// 0x080a4d5a  size=142  FUN_080a4d5a  status=translated  naming=inferred
// naming evidence: yaw of a 2D vector in degrees, wrapped into [0, 360)
float Vec2_AngleDegrees(const float *v)
{
    double angle;
    float result;

    if (v[1] == 0.0f && v[0] == 0.0f) {
        return 0.0f;
    }
    angle = atan2((double)v[1], (double)v[0]);
    result = (float)(((long double)180.0 * (long double)angle) / (long double)3.141592653589793);
    if (result < 0.0) {
        result = result + 360.0;
    }
    return result;
}

// 0x080a4de8  size=114  FUN_080a4de8  status=translated  naming=inferred
// naming evidence: yaw of a 2D vector in degrees without wrapping
float Vec2_AngleDegreesRaw(const float *v)
{
    double angle;

    if (v[1] == 0.0f && v[0] == 0.0f) {
        return 0.0f;
    }
    angle = atan2((double)v[1], (double)v[0]);
    return (float)(((long double)180.0 * (long double)angle) / (long double)3.141592653589793);
}

// 0x080a4e5a  size=210  FUN_080a4e5a  status=translated  naming=inferred
// naming evidence: elevation of a 3-vector in degrees (-180 scale), wrapped into [0, 360); zero horizontal gives 270 or 90
float Vec3_ElevationDegrees(const float *v)
{
    long double horiz;
    double angle;
    float result;

    if (v[1] == 0.0f && v[0] == 0.0f) {
        if (0.0f < v[2]) {
            return 270.0f;
        }
        return 90.0f;
    }
    horiz = (long double)Float_Sqrt_080aaebe(v[1] * v[1] + v[0] * v[0]);
    angle = atan2((double)v[2], (double)(float)horiz);
    result = (float)(((long double)-180.0 * (long double)angle) / (long double)3.141592653589793);
    if (result < 0.0) {
        result = result + 360.0;
    }
    return result;
}

// 0x080a4f2c  size=182  FUN_080a4f2c  status=translated  naming=inferred
// naming evidence: elevation of a 3-vector in degrees (-180 scale), not wrapped; zero horizontal gives -90 or 90
float Vec3_ElevationDegreesRaw(const float *v)
{
    long double horiz;
    double angle;

    if (v[1] == 0.0f && v[0] == 0.0f) {
        if (0.0f < v[2]) {
            return -90.0f;
        }
        return 90.0f;
    }
    horiz = (long double)Float_Sqrt_080aaebe(v[1] * v[1] + v[0] * v[0]);
    angle = atan2((double)v[2], (double)(float)horiz);
    return (float)(((long double)-180.0 * (long double)angle) / (long double)3.141592653589793);
}

// 0x080a4fe2  size=319  FUN_080a4fe2  status=translated  naming=inferred
// naming evidence: angles {elevation, yaw, 0} with wrapped values (VectorToAngles shape)
void Vec3_ToAngles(const float *v, float *out)
{
    double angle;
    long double horiz;
    float yaw;
    float pitch;

    if (v[1] == 0.0f && v[0] == 0.0f) {
        yaw = 0.0f;
        if (0.0f < v[2]) {
            pitch = 270.0f;
        } else {
            pitch = 90.0f;
        }
    } else {
        angle = atan2((double)v[1], (double)v[0]);
        yaw = (float)(((long double)180.0 * (long double)angle) / (long double)3.141592653589793);
        if (yaw < 0.0) {
            yaw = yaw + 360.0;
        }
        horiz = (long double)Float_Sqrt_080aaebe(v[1] * v[1] + v[0] * v[0]);
        angle = atan2((double)v[2], (double)(float)horiz);
        pitch = (float)(((long double)-180.0 * (long double)angle) / (long double)3.141592653589793);
        if (pitch < 0.0) {
            pitch = pitch + 360.0;
        }
    }
    out[0] = pitch;
    out[1] = yaw;
    out[2] = 0.0f;
}

// 0x080a5122  size=260  FUN_080a5122  status=translated  naming=inferred
// naming evidence: angles {elevation, yaw, 0} without wrapping (VectorToAngles shape)
void Vec3_ToAnglesRaw(const float *v, float *out)
{
    double angle;
    long double horiz;
    float yaw;
    float pitch;

    if (v[1] == 0.0f && v[0] == 0.0f) {
        yaw = 0.0f;
        if (0.0f < v[2]) {
            pitch = -90.0f;
        } else {
            pitch = 90.0f;
        }
    } else {
        angle = atan2((double)v[1], (double)v[0]);
        yaw = (float)(((long double)180.0 * (long double)angle) / (long double)3.141592653589793);
        horiz = (long double)Float_Sqrt_080aaebe(v[1] * v[1] + v[0] * v[0]);
        angle = atan2((double)v[2], (double)(float)horiz);
        pitch = (float)(((long double)-180.0 * (long double)angle) / (long double)3.141592653589793);
    }
    out[0] = pitch;
    out[1] = yaw;
    out[2] = 0.0f;
}

// 0x080a5226  size=375  FUN_080a5226  status=translated  naming=inferred
// naming evidence: AngleVectors shape: forward, right and up from {pitch, yaw, roll} in degrees; each output pointer is optional
void Angles_ToVectors(const float *angles, float *forward, float *right, float *up)
{
    float sinYaw;
    float cosYaw;
    float sinPitch;
    float cosPitch;
    float sinRoll;
    float cosRoll;
    float angle;

    angle = (float)(angles[1] * 0.017453292519943295);
    SinCos_Float(angle, &sinYaw, &cosYaw);
    angle = (float)(angles[0] * 0.017453292519943295);
    SinCos_Float(angle, &sinPitch, &cosPitch);
    if (forward != NULL) {
        forward[0] = cosPitch * cosYaw;
        forward[1] = cosPitch * sinYaw;
        forward[2] = -sinPitch;
    }
    if (right != NULL || up != NULL) {
        angle = (float)(angles[2] * 0.017453292519943295);
        SinCos_Float(angle, &sinRoll, &cosRoll);
        if (right != NULL) {
            volatile float nSR = -sinRoll;   /* keep fchs+fadd (a + -b) from folding into fsub: NaN sign differs */
            volatile float nCR = -cosRoll;
            /* operand order follows the original x87 sequence (matters for NaN propagation) */
            right[0] = nSR * sinPitch * cosYaw + cosRoll * sinYaw;
            right[1] = nSR * sinPitch * sinYaw + nCR * cosYaw;
            right[2] = nSR * cosPitch;
        }
        if (up != NULL) {
            volatile float nSR2 = -sinRoll;
            up[0] = cosRoll * sinPitch * cosYaw + sinRoll * sinYaw;
            up[1] = cosRoll * sinPitch * sinYaw + nSR2 * cosYaw;
            up[2] = cosRoll * cosPitch;
        }
    }
}

// 0x080a539e  size=128  FUN_080a539e  status=translated  naming=inferred
// naming evidence: yaw in degrees to a forward (cos, sin, 0) and right (sin, -cos, 0) pair; both pointers optional
void Angle_YawToForwardRight(float yaw, float *forward, float *right)
{
    float sinYaw;
    float cosYaw;
    float angle = (float)(yaw * 0.017453292519943295);

    SinCos_Float(angle, &sinYaw, &cosYaw);
    if (forward != NULL) {
        forward[0] = cosYaw;
        forward[1] = sinYaw;
        forward[2] = 0.0f;
    }
    if (right != NULL) {
        right[0] = sinYaw;
        right[1] = -cosYaw;
        right[2] = 0.0f;
    }
}

// 0x080a541e  size=210  FUN_080a541e  status=translated  naming=inferred
// naming evidence: Q3 PerpendicularVector shape: picks the smallest-magnitude axis, builds src with that axis shifted by one, normalizes
void Vec3_PerpendicularVector(const float *src, float *dst)
{
    volatile float sq[3];   /* x87: the original rounds each square to a 32-bit float slot */
    int32 axis;

    sq[0] = src[0] * src[0];
    sq[1] = src[1] * src[1];
    sq[2] = src[2] * src[2];
    axis = (int32)(sq[1] < sq[0]);
    if (sq[2] < sq[axis]) {
        axis = 2;
    }
    {
        float negComp;
        W32(negComp) = W32(((float *)src)[axis]) ^ 0x80000000u;   /* integer sign flip, as the original */
        Vec3_Scale_080ab190(src, negComp, dst);
    }
    dst[axis] = dst[axis] + 1.0f;
    Vec3_Normalize(dst);
}

// 0x080a54f0  size=122  FUN_080a54f0  status=translated  naming=inferred
// naming evidence: unit normal of triangle (a, b, c): normalize(normalize(a-b) x normalize(a-c))
void Triangle_Normal(const float *a, const float *b, const float *c, float *normal)
{
    float e1[6];
    float e2[4];

    Vec3_Sub(a, b, e1);
    Vec3_Normalize(e1);
    Vec3_Sub(a, c, e2);
    Vec3_Normalize(e2);
    Vec3_Cross(e1, e2, normal);
    Vec3_Normalize(normal);
}

// 0x080a556a  size=173  FUN_080a556a  status=translated  naming=inferred
// naming evidence: projects point onto the line through origin with direction (end - origin); out = origin when the direction is degenerate
void Line_ProjectPoint(const float *point, const float *origin, const float *end, float *out)
{
    float dir[6];
    float rel[6];
    volatile float dot;    /* x87: the original keeps dot and len2 in 32-bit stack slots */
    volatile float len2;

    Vec3_Sub(end, origin, dir);
    Vec3_Sub(point, origin, rel);
    dot = Vec3_Dot_080ab260(rel, dir);
    len2 = Vec3_Dot_080ab260(dir, dir);
    if (len2 == 0.0f) {
        Vec3_Copy_080ab0d8(origin, out);
    } else {
        Vec3_MA(origin, dot / len2, dir, out);
    }
}

/* Returns the squared length of the component of (point - origin) perpendicular to (end - origin). */
// 0x080a5618  size=164  FUN_080a5618  status=translated  naming=inferred
// naming evidence: dot of the residual (rel - t*dir) with itself; the function returns the dot in st(0) (verified in disassembly)
float Line_ResidualLengthSq(const float *point, const float *origin, const float *end)
{
    float dir[6];
    float rel[6];
    float resid[4];
    volatile float numer;   /* x87: 32-bit stack slots in the original */
    volatile float denom;
    volatile float t;
    float negT;

    Vec3_Sub(end, origin, dir);
    Vec3_Sub(point, origin, rel);
    numer = Vec3_Dot_080ab260(rel, dir);
    denom = Vec3_Dot_080ab260(dir, dir);
    t = numer / denom;
    W32(negT) = W32(t) ^ 0x80000000u;   /* integer sign flip, as the original */
    Vec3_MA(rel, negT, dir, resid);
    return Vec3_Dot_080ab260(resid, resid);
}

// 0x080a56bc  size=190  FUN_080a56bc  status=translated  naming=unknown
// naming evidence: sums squared per-axis excesses of (b-a) and (a-c) over 3 axes; the observable result is only this sum
float Vec3_ExtentDistanceSq(const float *a, const float *b, const float *c)
{
    volatile float sum = 0.0f;   /* x87: sum and d are 32-bit stack slots in the original */
    int32 i;

    for (i = 0; i < 3; i++) {
        volatile float d = b[i] - a[i];
        if (0.0f < d) {
            sum = sum + d * d;
        } else {
            d = a[i] - c[i];
            if (0.0f < d) {
                sum = sum + d * d;
            }
        }
    }
    return sum;
}

// 0x080a577a  size=71  FUN_080a577a  status=translated  naming=inferred
// naming evidence: 3x3 identity (9 floats, row-major): memset 0x24 bytes then 1.0 at elements 0, 4, 8
void Mat3_Identity(float *m)
{
    memset(m, 0, 0x24);
    m[0] = 1.0f;
    m[4] = 1.0f;
    m[8] = 1.0f;
}

// 0x080a57c2  size=35  FUN_080a57c2  status=translated  naming=observed
// naming evidence: copies the 16-float identity constant at rodata 0x0814d100 (verified bytes)
void Mat4_CopyIdentity(float *m)
{
    memcpy(m, identityMatrix4, 0x40);
}

// 0x080a57e6  size=267  FUN_080a57e6  status=translated  naming=inferred
// naming evidence: 4x4 from a 3x3 axis block scaled by `scale` plus origin in the translation row (row-major, w = 1)
void Mat4_FromAxisOrigin(float *out, const float *origin, const float *axes, float scale)
{
    out[0] = axes[0] * scale;
    out[1] = axes[1] * scale;
    out[2] = axes[2] * scale;
    out[3] = 0.0f;
    out[4] = axes[3] * scale;
    out[5] = axes[4] * scale;
    out[6] = axes[5] * scale;
    out[7] = 0.0f;
    out[8] = axes[6] * scale;
    out[9] = axes[7] * scale;
    out[10] = axes[8] * scale;
    out[11] = 0.0f;
    out[12] = origin[0];
    out[13] = origin[1];
    out[14] = origin[2];
    out[15] = 1.0f;
}

// 0x080a58f2  size=524  FUN_080a58f2  status=translated  naming=inferred
// naming evidence: row-major 3x3 product out = a * b (summation order k = 2, 1, 0)
void Mat3_Multiply(const float *a, const float *b, float *out)
{
    out[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
    out[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
    out[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];
    out[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
    out[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
    out[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];
    out[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
    out[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
    out[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];
}

// 0x080a5afe  size=542  FUN_080a5afe  status=translated  naming=inferred
// naming evidence: in-place 3x3 product b = a * b; rows 0 and 1 are computed into temporaries, row 2 is written directly first
void Mat3_MultiplyInPlace(const float *a, float *b)
{
    volatile float row0[3];   /* x87: float stack slots (every result is rounded when stored) */
    volatile float row1[3];

    row0[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
    row0[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
    row0[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];
    row1[0] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
    row1[1] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
    row1[2] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];
    b[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
    b[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
    b[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];
    Vec3_Copy_080ab0d8((const float *)row0, b);
    Vec3_Copy_080ab0d8((const float *)row1, b + 3);
}

// 0x080a5d1c  size=725  FUN_080a5d1c  status=translated  naming=inferred
// naming evidence: row-major 3x4 transform concatenation (translation in column 3 of each row)
void Mat34_Concat(const float *a, const float *b, float *out)
{
    out[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8];
    out[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9];
    out[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10];
    out[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3];
    out[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8];
    out[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9];
    out[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10];
    out[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7];
    out[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8];
    out[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9];
    out[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10];
    out[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11];
}

// 0x080a5ff2  size=725  FUN_080a5ff2  status=translated  naming=inferred
// naming evidence: 3x4 product in the row-vector layout (3x3 block in rows 0..2, translation in row 3 at elements 9..11)
void Mat34_ConcatRowVector(const float *a, const float *b, float *out)
{
    out[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
    out[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
    out[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
    out[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
    out[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
    out[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
    out[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];
    out[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];
    out[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];
    out[9] = a[9] * b[0] + a[10] * b[3] + a[11] * b[6] + b[9];
    out[10] = a[9] * b[1] + a[10] * b[4] + a[11] * b[7] + b[10];
    out[11] = a[9] * b[2] + a[10] * b[5] + a[11] * b[8] + b[11];
}

// 0x080a62c8  size=1226  FUN_080a62c8  status=translated  naming=inferred
// naming evidence: row-major 4x4 product out = a * b (summation order k = 3, 2, 1, 0)
void Mat4_Multiply(const float *a, const float *b, float *out)
{
    out[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
    out[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
    out[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
    out[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];
    out[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
    out[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
    out[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
    out[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];
    out[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
    out[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
    out[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
    out[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];
    out[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
    out[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
    out[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
    out[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];
}

// 0x080a6792  size=143  FUN_080a6792  status=translated  naming=inferred
// naming evidence: 3x3 transpose (out may not alias in; element order preserved)
void Mat3_Transpose(const float *in, float *out)
{
    W32(out[0]) = W32(((float *)in)[0]);
    W32(out[1]) = W32(((float *)in)[3]);
    W32(out[2]) = W32(((float *)in)[6]);
    W32(out[3]) = W32(((float *)in)[1]);
    W32(out[4]) = W32(((float *)in)[4]);
    W32(out[5]) = W32(((float *)in)[7]);
    W32(out[6]) = W32(((float *)in)[2]);
    W32(out[7]) = W32(((float *)in)[5]);
    W32(out[8]) = W32(((float *)in)[8]);
}

// 0x080a6822  size=255  FUN_080a6822  status=translated  naming=inferred
// naming evidence: 4x4 transpose (element order preserved)
void Mat4_Transpose(const float *in, float *out)
{
    W32(out[0]) = W32(((float *)in)[0]);
    W32(out[1]) = W32(((float *)in)[4]);
    W32(out[2]) = W32(((float *)in)[8]);
    W32(out[3]) = W32(((float *)in)[12]);
    W32(out[4]) = W32(((float *)in)[1]);
    W32(out[5]) = W32(((float *)in)[5]);
    W32(out[6]) = W32(((float *)in)[9]);
    W32(out[7]) = W32(((float *)in)[13]);
    W32(out[8]) = W32(((float *)in)[2]);
    W32(out[9]) = W32(((float *)in)[6]);
    W32(out[10]) = W32(((float *)in)[10]);
    W32(out[11]) = W32(((float *)in)[14]);
    W32(out[12]) = W32(((float *)in)[3]);
    W32(out[13]) = W32(((float *)in)[7]);
    W32(out[14]) = W32(((float *)in)[11]);
    W32(out[15]) = W32(((float *)in)[15]);
}

// 0x080a6922  size=544  FUN_080a6922  status=translated  naming=inferred
// naming evidence: 3x3 inverse via cofactors, scaled by 1/determinant (1.0 is a double division, as in the original)
void Mat3_Invert(const float *m, float *inv)
{
    /* x87: the original stores the determinant and its reciprocal as 32-bit floats */
    volatile float det = (m[8] * m[4] - m[7] * m[5]) * m[0] -
                         (m[8] * m[1] - m[7] * m[2]) * m[3] +
                         (m[5] * m[1] - m[4] * m[2]) * m[6];
    volatile float invDet = 1.0 / det;

    inv[0] = (m[8] * m[4] - m[7] * m[5]) * invDet;
    inv[1] = -(m[8] * m[1] - m[7] * m[2]) * invDet;
    inv[2] = (m[5] * m[1] - m[4] * m[2]) * invDet;
    inv[3] = -(m[8] * m[3] - m[6] * m[5]) * invDet;
    inv[4] = (m[8] * m[0] - m[6] * m[2]) * invDet;
    inv[5] = -(m[5] * m[0] - m[3] * m[2]) * invDet;
    inv[6] = (m[7] * m[3] - m[6] * m[4]) * invDet;
    inv[7] = -(m[7] * m[0] - m[6] * m[1]) * invDet;
    inv[8] = (m[4] * m[0] - m[3] * m[1]) * invDet;
}

// 0x080a6b42  size=83  FUN_080a6b42  status=translated  naming=inferred
// naming evidence: inverse of a rigid 3x4 transform: transposed 3x3 block, translation = -(origin) times the transposed block
void Rigid_Invert(const float *src, float *dst)
{
    float negOrigin[6];

    Mat3_Transpose(src, dst);
    Vec3_Sub(vec3_origin, src + 9, negOrigin);
    Vec3_RotateByTranspose(negOrigin, dst, dst + 9);
}

// 0x080a6b96  size=1567  FUN_080a6b96  status=translated  naming=inferred
// naming evidence: 4x4 inverse by 2x2 sub-determinants and 1/det scaling; t[j][i] holds memory element m[4i+j] (Ghidra stride-16 reads)
void Mat4_Invert(const float *m, float *out)
{
    /* x87: the original keeps every intermediate in a 32-bit stack slot or in the output matrix, so each statement
     * below rounds to float; volatile reproduces that (a modern compiler would keep values in x87 registers). */
    volatile float t[4][4];
    volatile float X[12];     /* the original reuses one 12-float block for both pair-product tables */
    volatile float *vo = out;
    volatile float det;
    volatile float invDet;
    int32 i;

    /* transpose copy with integer moves (preserves signalling NaNs) */
    for (i = 0; i < 4; i++) {
        W32(t[0][i]) = W32(((float *)m)[i * 4]);
        W32(t[1][i]) = W32(((float *)m)[i * 4 + 1]);
        W32(t[2][i]) = W32(((float *)m)[i * 4 + 2]);
        W32(t[3][i]) = W32(((float *)m)[i * 4 + 3]);
    }

    X[0] = t[2][2] * t[3][3];
    X[1] = t[2][3] * t[3][2];
    X[2] = t[2][1] * t[3][3];
    X[3] = t[2][3] * t[3][1];
    X[4] = t[2][1] * t[3][2];
    X[5] = t[2][2] * t[3][1];
    X[6] = t[2][0] * t[3][3];
    X[7] = t[2][3] * t[3][0];
    X[8] = t[2][0] * t[3][2];
    X[9] = t[2][2] * t[3][0];
    X[10] = t[2][0] * t[3][1];
    X[11] = t[2][1] * t[3][0];
    vo[0] = X[0] * t[1][1] + X[3] * t[1][2] + X[4] * t[1][3];
    vo[0] = vo[0] - (X[1] * t[1][1] + X[2] * t[1][2] + X[5] * t[1][3]);
    vo[1] = X[1] * t[1][0] + X[6] * t[1][2] + X[9] * t[1][3];
    vo[1] = vo[1] - (X[0] * t[1][0] + X[7] * t[1][2] + X[8] * t[1][3]);
    vo[2] = X[2] * t[1][0] + X[7] * t[1][1] + X[10] * t[1][3];
    vo[2] = vo[2] - (X[3] * t[1][0] + X[6] * t[1][1] + X[11] * t[1][3]);
    vo[3] = X[5] * t[1][0] + X[8] * t[1][1] + X[11] * t[1][2];
    vo[3] = vo[3] - (X[4] * t[1][0] + X[9] * t[1][1] + X[10] * t[1][2]);
    vo[4] = X[1] * t[0][1] + X[2] * t[0][2] + X[5] * t[0][3];
    vo[4] = vo[4] - (X[0] * t[0][1] + X[3] * t[0][2] + X[4] * t[0][3]);
    vo[5] = X[0] * t[0][0] + X[7] * t[0][2] + X[8] * t[0][3];
    vo[5] = vo[5] - (X[1] * t[0][0] + X[6] * t[0][2] + X[9] * t[0][3]);
    vo[6] = X[3] * t[0][0] + X[6] * t[0][1] + X[11] * t[0][3];
    vo[6] = vo[6] - (X[2] * t[0][0] + X[7] * t[0][1] + X[10] * t[0][3]);
    vo[7] = X[4] * t[0][0] + X[9] * t[0][1] + X[10] * t[0][2];
    vo[7] = vo[7] - (X[5] * t[0][0] + X[8] * t[0][1] + X[11] * t[0][2]);
    X[0] = t[0][2] * t[1][3];
    X[1] = t[0][3] * t[1][2];
    X[2] = t[0][1] * t[1][3];
    X[3] = t[0][3] * t[1][1];
    X[4] = t[0][1] * t[1][2];
    X[5] = t[0][2] * t[1][1];
    X[6] = t[0][0] * t[1][3];
    X[7] = t[0][3] * t[1][0];
    X[8] = t[0][0] * t[1][2];
    X[9] = t[0][2] * t[1][0];
    X[10] = t[0][0] * t[1][1];
    X[11] = t[0][1] * t[1][0];
    vo[8] = X[0] * t[3][1] + X[3] * t[3][2] + X[4] * t[3][3];
    vo[8] = vo[8] - (X[1] * t[3][1] + X[2] * t[3][2] + X[5] * t[3][3]);
    vo[9] = X[1] * t[3][0] + X[6] * t[3][2] + X[9] * t[3][3];
    vo[9] = vo[9] - (X[0] * t[3][0] + X[7] * t[3][2] + X[8] * t[3][3]);
    vo[10] = X[2] * t[3][0] + X[7] * t[3][1] + X[10] * t[3][3];
    vo[10] = vo[10] - (X[3] * t[3][0] + X[6] * t[3][1] + X[11] * t[3][3]);
    vo[11] = X[5] * t[3][0] + X[8] * t[3][1] + X[11] * t[3][2];
    vo[11] = vo[11] - (X[4] * t[3][0] + X[9] * t[3][1] + X[10] * t[3][2]);
    vo[12] = X[2] * t[2][2] + X[5] * t[2][3] + X[1] * t[2][1];
    vo[12] = vo[12] - (X[4] * t[2][3] + X[0] * t[2][1] + X[3] * t[2][2]);
    vo[13] = X[8] * t[2][3] + X[0] * t[2][0] + X[7] * t[2][2];
    vo[13] = vo[13] - (X[6] * t[2][2] + X[9] * t[2][3] + X[1] * t[2][0]);
    vo[14] = X[6] * t[2][1] + X[11] * t[2][3] + X[3] * t[2][0];
    vo[14] = vo[14] - (X[10] * t[2][3] + X[2] * t[2][0] + X[7] * t[2][1]);
    vo[15] = X[10] * t[2][2] + X[4] * t[2][0] + X[9] * t[2][1];
    vo[15] = vo[15] - (X[8] * t[2][1] + X[11] * t[2][2] + X[5] * t[2][0]);

    det = t[0][0] * vo[0] + t[0][1] * vo[1] + t[0][2] * vo[2] + t[0][3] * vo[3];
    invDet = 1.0f / det;
    for (i = 0; i < 16; i++) {
        vo[i] = vo[i] * invDet;
    }
}

// 0x080a71b6  size=170  FUN_080a71b6  status=translated  naming=inferred
// naming evidence: identical to Vec3_MulMat3Cols (row vector times 3x3, transposed indexing)
void Vec3_RotateByTranspose(const float *v, const float *m, float *out)
{
    out[0] = v[0] * m[0] + v[1] * m[3] + v[2] * m[6];
    out[1] = v[0] * m[1] + v[1] * m[4] + v[2] * m[7];
    out[2] = v[0] * m[2] + v[1] * m[5] + v[2] * m[8];
}

// 0x080a7260  size=299  FUN_080a7260  status=translated  naming=inferred
// naming evidence: row vector times 4x4: out[j] = sum_k v[k] * m[4k+j] (k summed from 3 down to 0)
void Vec4_MulMat4(const float *v, const float *m, float *out)
{
    out[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + v[3] * m[12];
    out[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + v[3] * m[13];
    out[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + v[3] * m[14];
    out[3] = v[0] * m[3] + v[1] * m[7] + v[2] * m[11] + v[3] * m[15];
}

// 0x080a7436  size=194  FUN_080a7436  status=translated  naming=inferred
// naming evidence: affine point transform out = v * M + t, rows 0..2 of M in the row-vector layout with translation at 9..11
void Vec3_TransformAffine(const float *v, const float *m, float *out)
{
    out[0] = v[0] * m[0] + v[1] * m[3] + v[2] * m[6] + m[9];
    out[1] = v[0] * m[1] + v[1] * m[4] + v[2] * m[7] + m[10];
    out[2] = v[0] * m[2] + v[1] * m[5] + v[2] * m[8] + m[11];
}

// 0x080a74f8  size=165  FUN_080a74f8  status=translated  naming=unknown
// naming evidence: (point - origin) with origin at xform[9..11], then out = xform block times that delta (inverse-rotation style)
void Vec3_WorldToLocal(const float *point, const float *xform, float *out)
{
    float local[6];

    Vec3_Sub(point, xform + 9, local);
    out[0] = xform[0] * local[0] + xform[1] * local[1] + xform[2] * local[2];
    out[1] = xform[3] * local[0] + xform[4] * local[1] + xform[5] * local[2];
    out[2] = xform[6] * local[0] + xform[7] * local[1] + xform[8] * local[2];
}

// 0x080a759e  size=209  FUN_080a759e  status=translated  naming=inferred
// naming evidence: in-place affine transform v = v * M + t; the original temporaries are reproduced so reads see pre-write values
void Vec3_TransformAffineInPlace(float *v, const float *m)
{
    /* x87: the new x and y are float stack slots; the original computes them first, then z, then moves x/y as integers */
    volatile float n0 = v[0] * m[0] + v[1] * m[3] + v[2] * m[6] + m[9];
    volatile float n1 = v[0] * m[1] + v[1] * m[4] + v[2] * m[7] + m[10];

    v[2] = v[0] * m[2] + v[1] * m[5] + v[2] * m[8] + m[11];
    W32(v[0]) = *(volatile unsigned int *)&n0;
    W32(v[1]) = *(volatile unsigned int *)&n1;
}

// 0x080a7670  size=102  FUN_080a7670  status=translated  naming=inferred
// naming evidence: rotates the 2D vector by `degrees`; the angle is passed to SinCos_Float as a single-precision value (fstps in the binary)
void Vec2_RotateDegrees(float *v, float degrees)
{
    float sinA;
    float cosA;
    float tmp;

    SinCos_Float((float)(degrees * 0.017453292519943295), &sinA, &cosA);
    tmp = v[1];
    v[1] = v[0] * sinA + v[1] * cosA;
    v[0] = v[0] * cosA - tmp * sinA;
}

// 0x080a76d6  size=290  FUN_080a76d6  status=translated  naming=inferred
// naming evidence: quaternion product out = a * b in the component order of the binary (w last in the x87 expression)
void Quat_Multiply(const float *a, const float *b, float *out)
{
    /* term order follows the original x87 sequence (objdump 0x080a76d6) */
    out[0] = a[0] * b[3] + a[3] * b[0] + a[2] * b[1] - a[1] * b[2];
    out[1] = a[1] * b[3] - a[2] * b[0] + a[3] * b[1] + a[0] * b[2];
    out[2] = a[2] * b[3] + a[1] * b[0] - a[0] * b[1] + a[3] * b[2];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

// 0x080a77f8  size=78  FUN_080a77f8  status=translated  naming=inferred
// naming evidence: quaternion conjugate: negates x, y, z (sign-bit flip in the binary), keeps w
void Quat_Conjugate(const float *in, float *out)
{
    /* integer moves with a sign-bit xor, as the original (no x87 load, so signalling NaNs survive) */
    W32(out[0]) = W32(((float *)in)[0]) ^ 0x80000000u;
    W32(out[1]) = W32(((float *)in)[1]) ^ 0x80000000u;
    W32(out[2]) = W32(((float *)in)[2]) ^ 0x80000000u;
    W32(out[3]) = W32(((float *)in)[3]);
}

// 0x080a7846  size=384  FUN_080a7846  status=translated  naming=inferred
// naming evidence: quaternion (w, x, y, z) to 3x3 rotation with s = 2/|q|^2; all inputs are read into temporaries before any write
void Quat_ToMat3(const float *q, float *out)
{
    /* x87: every named intermediate below is a 32-bit float stack slot in the original (fstp/fld) */
    volatile float q0q0 = q[0] * q[0];
    volatile float q1q1 = q[1] * q[1];
    volatile float q2q2 = q[2] * q[2];
    volatile float q3q3 = q[3] * q[3];
    volatile float norm = q0q0 + q1q1 + q2q2 + q3q3;
    volatile float s = 2.0f / norm;
    volatile float wwS, xxS, yyS, wS, wxS, wyS, wzS, sx, xyS, xzS, yzS;

    wwS = q0q0 * s;
    xxS = q1q1 * s;
    yyS = q2q2 * s;
    wS = s * q[0];
    wxS = wS * q[1];
    wyS = wS * q[2];
    wzS = wS * q[3];
    sx = s * q[1];
    xyS = sx * q[2];
    xzS = sx * q[3];
    yzS = s * q[2] * q[3];   /* no intermediate store in the original */

    out[0] = 1.0f - (xxS + yyS);
    out[1] = wxS + yzS;
    out[2] = wyS - xzS;
    out[3] = wxS - yzS;
    out[4] = 1.0f - (wwS + yyS);
    out[5] = xyS + wzS;
    out[6] = wyS + xzS;
    out[7] = xyS - wzS;
    out[8] = 1.0f - (wwS + xxS);
}

// 0x080a738c  size=170  FUN_080a738c  status=translated  naming=inferred
// naming evidence: same arithmetic as Vec3_MulMat3Rows (separate copy in the binary): out[j] = sum_k v[k] * m[3j+k]
void Vec3_MulMat3RowsAlt(const float *v, const float *m, float *out)
{
    out[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2];
    out[1] = v[0] * m[3] + v[1] * m[4] + v[2] * m[5];
    out[2] = v[0] * m[6] + v[1] * m[7] + v[2] * m[8];
}
