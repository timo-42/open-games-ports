#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_007_types.h"

/* cod2_globals.h types com_extraTimeScale as int; every use here is a float (extra time-scale factor, 1.0 at init). */
static inline uint32 FloatBits(float f)
{
    union { float f; uint32 u; } x;
    x.f = f;
    return x.u;
}
#define g_081a2270_f (*(float *)&com_extraTimeScale)

// 0x08061ccc  size=15  FUN_08061ccc  status=translated  naming=observed
// Registered as the "crash" console command (Com_Init, developer only); writes to address 0.
void Com_Crash_f(void)
{
    *(volatile int *)0 = 0x12345678;
}

// 0x08061cdc  size=94  FUN_08061cdc  status=translated  naming=inferred
// Reads developer/logfile/developer_script dvars and forwards them to Scr_Settings; role unknown.
void Com_ApplyDeveloperOptions(void)
{
    int active = 0;
    if (com_developer->current.integer != 0 || com_logfile->current.integer != 0)
        active = 1;
    Scr_Settings(active, com_developer_script->current.enabled, com_developer->current.integer);
}

// 0x08061d3a  size=86  FUN_08061d3a  status=translated  naming=inferred
// Strings "unskippablecinematic legal" and the cinematic chain; called once at end of Com_Init.
void Com_PlayIntroCinematics(void)
{
    if (com_dedicated->current.integer == 0) {
        Cbuf_AddText("unskippablecinematic legal\n");
        if (com_introPlayed->current.enabled == 0) {
            Dvar_SetString(cv_nextmap,
                        "cinematic atvi; set nextmap \"cinematic IW_logo; set nextmap cinematic cod_intro\"");
        }
        Dvar_SetBool(com_introPlayed, 1);
    }
}

// 0x08061d90  size=639  FUN_08061d90  status=translated  naming=observed
// Registers the common dvars by name ("dedicated", "com_maxfps", ... "com_animCheck").
void Com_InitDvars(void)
{
    com_dedicated = Dvar_RegisterInt("dedicated", 2, 0, 2, 0x1040);
    com_maxfps = Dvar_RegisterInt("com_maxfps", 0x55, 0, 1000, 0x1001);
    com_developer = Dvar_RegisterInt("developer", 0, 0, 2, 0x1000);
    com_developer_script = Dvar_RegisterBool("developer_script", 0, 0x1000);
    com_logfile = Dvar_RegisterInt("logfile", 0, 0, 2, 0x1000);
    com_timescale = Dvar_RegisterFloat("timescale", 1.0f, 0.001f, 1000.0f, 0x1088);
    com_fixedtime = Dvar_RegisterInt("fixedtime", 0, 0, 1000, 0x1080);
    com_viewlog = Dvar_RegisterInt("viewlog", 0, 0, 2, 0x1080);
    com_sv_paused = Dvar_RegisterInt("sv_paused", 0, 0, 2, 0x1040);
    com_cl_paused = Dvar_RegisterInt("cl_paused", 0, 0, 2, 0x1040);
    com_sv_running = Dvar_RegisterBool("sv_running", 0, 0x1040);
    ((int *)g_0817d838)[1] = 0;
    com_introPlayed = Dvar_RegisterBool("com_introPlayed", 0, 0x1001);
    com_animCheck = Dvar_RegisterBool("com_animCheck", 0, 0x1000);
    if (com_dedicated->current.integer != 0 && com_viewlog->current.integer == 0)
        Dvar_SetInt(com_viewlog, 1);
}

// 0x08062010  size=37  FUN_08062010  status=translated  naming=inferred
// Executes the command buffer with the dvar-load flag (Dvar_SetSetterGate) raised around it.
void Cbuf_ExecuteWithLoadFlag(void)
{
    Dvar_SetSetterGate(1);
    Cbuf_ExecuteThunk();
    Dvar_SetSetterGate(0);
}

// 0x08062036  size=118  FUN_08062036  status=translated  naming=inferred
// Strings "exec %s\n", "exec language.cfg\n", "exec safemode_mp_server.cfg\n": Com_ExecuteConfigs.
void Com_ExecuteConfigs(const char *extraCfg)
{
    char *line;

    line = va("exec %s\n", fs_defaultConfigName);
    Cbuf_AddText(line);
    Cbuf_AddText("exec language.cfg\n");
    if (extraCfg != 0) {
        line = va("exec %s\n", extraCfg);
        Cbuf_AddText(line);
    }
    Cbuf_ExecuteThunk();
    Cbuf_ExecuteWithLoadFlag();
    if (Com_SafeMode() != 0)
        Cbuf_AddText("exec safemode_mp_server.cfg\n");
    Cbuf_ExecuteThunk();
}

// 0x080620ac  size=20  FUN_080620ac  status=translated  naming=inferred
// Executes the multiplayer server config set (string "config_mp_server.cfg").
void Com_ExecuteServerConfig(void)
{
    Com_ExecuteConfigs("config_mp_server.cfg");
}

// 0x080620c0  size=597  FUN_080620c0  status=translated  naming=inferred
// Com_Init try-block body: banner "%s %s build %s %s", "--- Common Initialization Complete ---".
void Com_Init_Try_Block_Function(const char *commandLine)
{
    int i;
    char *versionText;
    int seed;

    Com_Printf("%s %s build %s %s\n", "CoD2 MP", "1.3", "linux-i386", "Jun 23 2006");
    Com_InitPushEvent();
    Com_ParseCommandLine((char *)commandLine);
    Swap_Init();
    Cbuf_Init();
    Cmd_Init();
    Com_StartupVariable(0);
    Com_StartupVariable("developer");
    FS_InitFilesystem();
    Com_InitDvars();
    for (i = 0; i < 1; i++) {
        Noop_08060c1a(i);
        Com_ExecuteServerConfig();
    }
    Noop_08060c1a(0);
    Cbuf_ExecuteThunk();
    Com_Noop_08062b64();
    Com_StartupVariable(0);
    Com_InitHunkMemory();
    dvar_modifiedFlags &= 0xfffffffe;
    g_081a2270_f = 1.0f;
    if (com_developer->current.integer != 0) {
        Cmd_AddCommand("error", Com_Error_f);
        Cmd_AddCommand("crash", Com_Crash_f);
        Cmd_AddCommand("freeze", Cmd_Freeze_f);
    }
    Cmd_AddCommand("quit", Com_Quit_f);
    Cmd_AddCommand("writeconfig", Cmd_WriteConfig_f);
    Cmd_AddCommand("writedefaults", Cmd_WriteDefaults_f);
    {
        char *platform = Com_FormatBuildVersion();
        versionText = va("%s %s build %s %s", "CoD2 MP", "1.3", platform, "linux-i386");
    }
    com_version = Dvar_RegisterString("version", "", 0x1040);
    Dvar_SetString(com_version, versionText);
    com_shortversion = Dvar_RegisterString("shortversion", "1.3", 0x1044);
    FxPool_Init();
    Sys_InitCvars();
    seed = Com_PumpSysEvents() & 0xffff;
    NET_InitCvars(seed);
    Scr_Init();
    Com_ApplyDeveloperOptions();
    XAnim_InitSlotPool();
    DObj_InitGlobals();
    SV_AddServerCvars();
    NET_Init();
    com_frameTime = Com_PumpSysEvents();
    Com_AddStartupCommands();
    Com_PlayIntroCinematics();
    com_fullyInitialized = 1;
    Com_Printf("--- Common Initialization Complete ---\n");
    Cbuf_ExecuteThunk();
    if (com_sv_running->current.enabled == 0)
        Com_Noop_080627d6();
}

// 0x08062316  size=82  FUN_08062316  status=translated  naming=inferred
// setjmp wrapper around Com_Init_Try_Block_Function; string "Error during initialization:\n%s\n".
void Com_Init(const char *commandLine)
{
    jmp_buf *env = (jmp_buf *)Sys_GetValue(2);
    if (_setjmp(*env) == 0) {
        Com_Init_Try_Block_Function(commandLine);
    } else {
        char *msg = va("Error during initialization:\n%s\n", com_errorMessage);
        Sys_Error(msg);
    }
}

// 0x08062368  size=90  FUN_08062368  status=translated  naming=inferred
// Writes archived dvars ("seta") to a config file; header "// generated by Call of Duty, do not modify".
void Com_WriteConfigToFile(const char *filename)
{
    int f = FS_FOpenFileWrite(filename);
    if (f == 0) {
        Com_Printf("Couldn't write %s.\n", filename);
    } else {
        FS_Printf(f, "// generated by Call of Duty, do not modify\n");
        Dvar_WriteVariables(f);
        FS_FCloseFile(f);
    }
}

// 0x080623c2  size=90  FUN_080623c2  status=translated  naming=inferred
// Same as Com_WriteConfigToFile but writes the default values via Dvar_WriteDefaults.
void Com_WriteDefaultsToFile(const char *filename)
{
    int f = FS_FOpenFileWrite(filename);
    if (f == 0) {
        Com_Printf("Couldn't write %s.\n", filename);
    } else {
        FS_Printf(f, "// generated by Call of Duty, do not modify\n");
        Dvar_WriteDefaults(f);
        FS_FCloseFile(f);
    }
}

// 0x0806241c  size=81  FUN_0806241c  status=translated  naming=inferred
// If initialised and the archive-modified bit is set, clears it and writes config_mp_server.cfg.
void Com_WriteConfiguration(void)
{
    char filename[0x48];

    if (com_fullyInitialized != 0 && ((dvar_modifiedFlags ^ 1) & 1) == 0) {
        dvar_modifiedFlags &= 0xfffffffe;
        Q_strncpyz(filename, "config_mp_server.cfg", 0x40);
        Com_WriteConfigToFile(filename);
    }
}

// 0x08062566  size=46  FUN_08062566  status=translated  naming=inferred
// Returns fixedtime (as float) when set, else the timescale value.
float Com_GetTimeScale(void)
{
    float scale;
    if (com_fixedtime->current.integer == 0)
        scale = com_timescale->current.value;
    else
        scale = (float)com_fixedtime->current.integer;
    return scale;
}

// 0x08062594  size=281  FUN_08062594  status=translated  naming=inferred
// Com_ModifyMsec: applies fixedtime/timescale, clamps; string "Hitch warning: %i msec frame time\n".
int Com_ModifyMsec(int msec)
{
    int originalMsec = msec;
    int clampMsec;
    bool modified;
    float ratio;

    if (com_fixedtime->current.integer == 0) {
        if (com_timescale->current.value != 1.0f || g_081a2270_f != 1.0f) {
            msec = Com_RoundFloat((float)msec * com_timescale->current.value * g_081a2270_f);
            modified = true;
        } else {
            modified = false;
        }
    } else {
        msec = com_fixedtime->current.integer;
        modified = true;
    }
    if (msec < 1)
        msec = 1;
    if (com_dedicated->current.integer == 0) {
        if (com_sv_running->current.enabled == 0)
            clampMsec = 5000;
        else
            clampMsec = 200;
    } else {
        if (500 < msec && msec < 500000)
            Com_Printf("Hitch warning: %i msec frame time\n", msec);
        clampMsec = 5000;
    }
    if (clampMsec < msec)
        msec = clampMsec;
    if (modified && originalMsec != 0)
        ratio = (float)msec / (float)originalMsec;
    else
        ratio = 1.0f;
    com_frameTimeScale = ratio;
    return msec;
}

// 0x080626ae  size=69  FUN_080626ae  status=translated  naming=inferred
// Handles a modified "viewlog" dvar: shows/hides console on non-dedicated builds, then clears modified.
void Com_CheckViewlog(void)
{
    if (com_viewlog->modified != 0) {
        if (com_dedicated->current.integer == 0)
            ((void (*)(int, int))Sys_NoOp_080d675a)(com_viewlog->current.integer, 0);
        Dvar_ClearModified(com_viewlog);
    }
}

// 0x080626f4  size=225  FUN_080626f4  status=translated  naming=inferred
// Com_Frame try-block body: framerate limiter on com_maxfps, Cbuf_Execute, Com_ModifyMsec.
void Com_Frame_Try_Block_Function(void)
{
    int minMsec;
    int msec;
    int scaled;

    Com_WriteConfiguration();
    Com_CheckViewlog();
    Anim_SetParseFlag(com_animCheck->current.enabled);
    minMsec = 1;
    if (0 < com_maxfps->current.integer && com_dedicated->current.integer == 0) {
        minMsec = (int)(1000LL / (long long)com_maxfps->current.integer);
        if (minMsec == 0)
            minMsec = 1;
    }
    for (;;) {
        com_frameTime = Com_EventLoop();
        if (com_frameTime < com_lastFrameTime)
            com_lastFrameTime = com_frameTime;
        msec = com_frameTime - com_lastFrameTime;
        if (minMsec <= msec)
            break;
        NET_Sleep(0);
    }
    Cbuf_ExecuteThunk();
    com_lastFrameTime = com_frameTime;
    scaled = Com_ModifyMsec(msec);
    Noop_08060c1a(0);
    SV_Frame(scaled);
}

// 0x080627d6  size=5  FUN_080627d6  status=translated  naming=unknown
// Empty function (called at end of Com_Init when sv_running is clear).
void Com_Noop_080627d6(void)
{
}

// 0x080627ee  size=5  FUN_080627ee  status=translated  naming=unknown
// Empty function.
void Com_Noop_080627ee(void)
{
}

// 0x080627f4  size=109  FUN_080627f4  status=translated  naming=inferred
// Com_Frame: setjmp-guarded frame; frame counter increments only on the non-error path.
void Com_Frame(void)
{
    jmp_buf *env;

    Com_Noop_08062b64();
    env = (jmp_buf *)Sys_GetValue(2);
    if (_setjmp(*env) == 0) {
        Com_Frame_Try_Block_Function();
        com_frameCount = com_frameCount + 1;
    }
    Sys_EnterCriticalSection(2);
    if (com_errorEntered == 0) {
        Sys_LeaveCriticalSection(2);
    } else {
        Com_ErrorRecover();
        Sys_LeaveCriticalSection(2);
    }
}

// 0x08062862  size=40  FUN_08062862  status=translated  naming=inferred
// Closes the log file handle (com_logfileHandle) if open.
void Com_CloseLogFile(void)
{
    if (com_logfileHandle != 0) {
        FS_FCloseFile(com_logfileHandle);
        com_logfileHandle = 0;
    }
}

// 0x0806288a  size=43  FUN_0806288a  status=translated  naming=inferred
// Shutdown sequence calling a fixed list of subsystem shutdown functions.
void Com_ShutdownSubsystems(void)
{
    Com_ClearDObjsInitialized();
    DObj_ShutdownGlobals();
    XAnim_ShutdownSlotPool();
    CM_ClearMap();
    Noop_080d6cac();
    Hunk_Clear();
    Scr_Shutdown();
}

// 0x080628b6  size=67  FUN_080628b6  status=translated  naming=unknown
// Initialises a 0x100-byte string buffer structure: clears data, capacity=0x100.
void Field_Clear(field_t *buf)
{
    memset(buf->buffer, 0, 0x100);
    buf->cursor = 0;
    buf->scroll = 0;
    buf->widthInChars = 0x100;
}

// 0x080628fa  size=78  FUN_080628fa  status=translated  naming=inferred
// Restart sequence: shutdown-like subsystem calls then DObj reinit (Com_InitDObj).
void Com_Restart_Subsystems(void)
{
    SV_ClearServerState();
    Com_ClearDObjsInitialized();
    DObj_ShutdownGlobals();
    XAnim_ShutdownSlotPool();
    CM_ClearMap();
    Noop_080d6cac();
    Hunk_Clear();
    Scr_Init();
    Com_ApplyDeveloperOptions();
    g_081a21c4 = 0;
    XAnim_InitSlotPool();
    DObj_InitGlobals();
    Com_InitDObj();
}

// 0x08062948  size=27  FUN_08062948  status=translated  naming=unknown
// Thin wrapper: calls StrPool_AllocBlock(arg, 5).
uint8 *Com_Call_0807706a_With5(int arg)
{
    return StrPool_AllocBlock(arg, 5);
}

// 0x0806299c  size=13  FUN_0806299c  status=translated  naming=unknown
// Stores the pending handle value in g_081a4f8c.
void Com_SetPendingHandle(int handle)
{
    g_081a4f8c = handle;
}

// 0x080629aa  size=36  FUN_080629aa  status=translated  naming=unknown
// Clears the pending handle and notifies BG_ResetWeaponCount when the argument matches it.
void Com_ClearPendingHandle(int handle)
{
    if (handle == g_081a4f8c) {
        g_081a4f8c = 0;
        BG_ResetWeaponCount();
    }
}

// 0x080629ce  size=218  FUN_080629ce  status=translated  naming=inferred
// Appends str to dest[pos..limit) wrapped in double quotes when quoteIfNeeded and the
// string is empty or contains a char below '!' within the available range. Returns new pos.
int Com_AppendQuotedToken(const char *str, char *dest, int pos, int limit, int quoteIfNeeded)
{
    bool quote = false;
    int i;

    if (quoteIfNeeded != 0) {
        if (*str == '\0') {
            quote = true;
        } else {
            i = 0;
            for (;;) {
                if (limit - pos <= i || str[i] == '\0')
                    break;
                if (str[i] < '!') {
                    quote = true;
                    break;
                }
                i++;
            }
        }
    }
    if (quote && pos < limit) {
        dest[pos] = '"';
        pos++;
    }
    for (i = 0; pos < limit && str[i] != '\0'; i++) {
        dest[pos] = str[i];
        pos++;
    }
    if (quote && pos < limit) {
        dest[pos] = '"';
        pos++;
    }
    return pos;
}

// 0x08062aa8  size=10  FUN_08062aa8  status=translated  naming=inferred
// Returns '.' (locale decimal-point character; constant in this build).
int Sys_GetDecimalPoint(void)
{
    return 0x2e;
}

// 0x08062ab2  size=124  FUN_08062ab2  status=translated  naming=inferred
// Formats a float with a precision and replaces the first '.' by the decimal point char.
void Com_FloatToString(float value, char *dest, unsigned int size, int precision)
{
    char point;
    unsigned int i;

    snprintf(dest, size - 1, "%.*f", precision, (double)value);
    dest[size - 1] = '\0';
    point = (char)Sys_GetDecimalPoint();
    if (point != '.') {
        for (i = 0; i < size; i++) {
            if (dest[i] == '.') {
                dest[i] = point;
                return;
            }
        }
    }
}

// 0x08062b2e  size=54  FUN_08062b2e  status=translated  naming=inferred
// floor(x + 0.5) converted with the x87 truncating fistp.
int Com_RoundFloat(float value)
{
    double f = floor((double)value + 0.5);
    return (int)f;
}

// 0x08062b64  size=5  FUN_08062b64  status=translated  naming=unknown
// Empty function (called from Com_Init and Com_Frame).
void Com_Noop_08062b64(void)
{
}

// 0x08062b6c  size=88  FUN_08062b6c  status=translated  naming=inferred
// Client DObj handle (+ 0x480 per local client) -> pool slot pointer, or NULL.
DObj_s *Com_GetClientDObj(int handle, int localClientNum)
{
    int index = handle + localClientNum * 0x480;
    if (g_dobjClientHandleToSlot[index] == 0)
        return 0;
    return (DObj_s *)(g_dobjPool + g_dobjClientHandleToSlot[index] * 100);
}

// 0x08062bc4  size=72  FUN_08062bc4  status=translated  naming=inferred
// Server DObj handle -> pool slot pointer, or NULL.
struct DObj *Com_GetServerDObj(int handle)
{
    if (g_dobjServerHandleToSlot[handle] == 0)
        return 0;
    return (struct DObj *)(g_dobjPool + g_dobjServerHandleToSlot[handle] * 100);
}

// 0x08062c0c  size=164  FUN_08062c0c  status=translated  naming=inferred
// Allocates a free DObj pool slot (1..0x7ff) scanning forward from the last one; 0 when none.
int Com_AllocDObjSlot(void)
{
    int cur = g_081d8a00;
    int prev;
    int idx;

    do {
        prev = cur;
        idx = prev + 1;
        if (0x7ff < idx) {
            idx = 1;
            for (;;) {
                if (g_081d8a00 < idx)
                    return 0;
                if (g_081d7080[idx] == 0)
                    break;
                idx++;
            }
            g_081d8a00 = idx;
            g_081d7080[idx] = 1;
            g_081d7880 = g_081d7880 - 1;
            return idx;
        }
        cur = idx;
    } while (g_081d7080[prev + 1] != 0);
    g_081d8a00 = idx;
    g_081d7080[prev + 1] = 1;
    g_081d7880 = g_081d7880 - 1;
    return idx;
}

// 0x08062cb0  size=131  FUN_08062cb0  status=translated  naming=inferred
// Com_SafeClientDObjCreate: allocates slot, builds the DObj, errors when the pool is exhausted.
void Com_ClientDObjCreate(void *models, int numModels, void *tree, int handle)
{
    int slot = Com_AllocDObjSlot();
    g_dobjClientHandleToSlot[handle] = (int16)slot;
    DObj_Create((DObjModelDef *)models, (uint16)numModels, (XAnimTree *)tree, (DObj *)(g_dobjPool + slot * 100), 0);
    if (g_081d7880 == 0)
        Com_Error(1, "\025No free DObjs");
}

// 0x08062d8e  size=134  FUN_08062d8e  status=translated  naming=inferred
// Server variant; passes (handle + 1) & 0xffff as the last DObjCreate argument.
void Com_ServerDObjCreate(void *models, int numModels, void *tree, int handle)
{
    int slot = Com_AllocDObjSlot();
    g_dobjServerHandleToSlot[handle] = (int16)slot;
    DObj_Create((DObjModelDef *)models, (uint16)numModels, (XAnimTree *)tree, (DObj *)(g_dobjPool + slot * 100),
                (int)((handle + 1U) & 0xffff));
    if (g_081d7880 == 0)
        Com_Error(1, "\025No free DObjs");
}

// 0x08062e14  size=95  FUN_08062e14  status=translated  naming=inferred
// Frees the client DObj for handle (slot released, counter incremented).
void Com_ClientDObjFree(int handle)
{
    int slot = (int)g_dobjClientHandleToSlot[handle];
    if (slot != 0) {
        g_dobjClientHandleToSlot[handle] = 0;
        g_081d7080[slot] = 0;
        g_081d7880 = g_081d7880 + 1;
        DObj_Free((DObj *)(g_dobjPool + slot * 100));
    }
}

// 0x08062e74  size=95  FUN_08062e74  status=translated  naming=inferred
// Frees the server DObj for handle.
void Com_ServerDObjFree(int handle)
{
    int slot = (int)g_dobjServerHandleToSlot[handle];
    if (slot != 0) {
        g_dobjServerHandleToSlot[handle] = 0;
        g_081d7080[slot] = 0;
        g_081d7880 = g_081d7880 + 1;
        DObj_Free((DObj *)(g_dobjPool + slot * 100));
    }
}

// 0x08062ed4  size=122  FUN_08062ed4  status=translated  naming=inferred
// Resets the DObj pool tables: 0x7ff free slots, handle maps cleared, initialised flag set.
void Com_InitDObj(void)
{
    Com_Memset(g_081d7080, 0, 0x800);
    g_081d7880 = 0x7ff;
    Com_Memset(g_dobjClientHandleToSlot, 0, 0x900);
    Com_Memset(g_dobjServerHandleToSlot, 0, 0x800);
    g_081d8a00 = 1;
    g_081d8a04 = 1;
}

// 0x08062f4e  size=26  FUN_08062f4e  status=translated  naming=inferred
// Clears the DObj-initialised flag if set.
void Com_ClearDObjsInitialized(void)
{
    if (g_081d8a04 != 0)
        g_081d8a04 = 0;
}

// 0x08062f68  size=15  FUN_08062f68  status=translated  naming=inferred
// Unconditionally clears the DObj-initialised flag.
void Com_ResetDObjsInitialized(void)
{
    g_081d8a04 = 0;
}

// 0x08062fa8  size=171  FUN_08062fa8  status=translated  naming=inferred
// Joins Cmd_Argv(start..argc-1) into buf (0x1000), space separated; stops before overflow.
void Cmd_ArgsFromBuffer(char *buf, int start)
{
    int argc = Cmd_Argc();
    int total = 0;
    int i;
    size_t len;

    *buf = 0;
    for (i = start; i < argc; i++) {
        const char *arg = Cmd_Argv(i);
        len = strlen(arg + 1);
        if (0xffd < (int)(len + total))
            break;
        I_strncat(buf, 0x1000, Cmd_Argv(i));
        if (i != argc - 1)
            I_strncat(buf, 0x1000, " ");
        total = total + (int)len;
    }
}

// 0x08063054  size=288  FUN_08063054  status=translated  naming=inferred
// Dvar_Command: prints dvar state for one argument, otherwise sets it to the joined arguments.
int Dvar_Command(void)
{
    dvar_s *dvar;
    char buf[4108];
    const char *def;
    const char *cur;

    dvar = Dvar_Lookup(Cmd_Argv(0));
    if (dvar == 0)
        return 0;
    if (Cmd_Argc() == 1) {
        def = Dvar_FormatResetValue(dvar);
        cur = Dvar_FormatCurrentValue(dvar);
        Com_Printf("\"%s\" is: \"%s^7\" default: \"%s^7\"\n", dvar->name, cur, def);
        if (Dvar_HasPendingLatch(dvar) != 0)
            Com_Printf("latched: \"%s\"\n", Dvar_FormatLatchedValue(dvar));
        Dvar_PrintDomain(dvar->type, dvar->domain.integer.min, dvar->domain.integer.max);
        return 1;
    }
    Cmd_ArgsFromBuffer(buf, 1);
    Dvar_SetFromUserInput(Cmd_Argv(0), buf);
    return 1;
}

// 0x08063174  size=572  FUN_08063174  status=translated  naming=inferred
// Dvar_Toggle with no value list; string "'toggle' with no arguments makes no sense for dvar '%s'".
int Dvar_ToggleDvar(dvar_s *dvar)
{
    switch (dvar->type) {
    case 0:
        Dvar_SetBoolEx(dvar, (uint8)dvar->current.enabled ^ 1, 1);
        return 1;
    case 1:
        if (0.0f < dvar->domain.decimal.min || dvar->domain.decimal.max < 1.0f) {
            if (dvar->current.value == dvar->domain.decimal.min)
                Dvar_SetFloatEx(dvar, dvar->domain.decimal.max, 1);
            else
                Dvar_SetFloatEx(dvar, dvar->domain.decimal.min, 1);
        } else if (dvar->current.value != 0.0f) {
            Dvar_SetFloatEx(dvar, 0.0f, 1);
        } else {
            Dvar_SetFloatEx(dvar, 1.0f, 1);
        }
        return 1;
    case 2:
    case 3:
    case 4:
    case 7:
    case 8:
        Com_Printf("'toggle' with no arguments makes no sense for dvar '%s'\n", dvar->name);
        return 0;
    case 5:
        if (dvar->domain.integer.min < 1 && 0 < dvar->domain.integer.max) {
            if (dvar->current.integer == 0)
                Dvar_SetIntEx(dvar, 1, 1);
            else
                Dvar_SetIntEx(dvar, 0, 1);
        } else if (dvar->current.integer == dvar->domain.integer.min) {
            Dvar_SetIntEx(dvar, dvar->domain.integer.max, 1);
        } else {
            Dvar_SetIntEx(dvar, dvar->domain.integer.min, 1);
        }
        return 1;
    case 6:
        if (dvar->domain.enumeration.stringCount != 0)
            Dvar_SetIntEx(dvar, (dvar->current.integer + 1) % dvar->domain.enumeration.stringCount, 1);
        return 1;
    default:
        return 0;
    }
}

// 0x080633b0  size=410  FUN_080633b0  status=translated  naming=observed
// "toggle" command: "USAGE: %s <variable> <optional value sequence>" / "toggle failed: dvar '%s' not found."
unsigned char Dvar_Toggle_f(void)
{
    dvar_s *dvar;
    char *name;
    const char *curValue;
    char *candidate;
    char *resolved;
    int i;

    if (Cmd_Argc() < 2) {
        Com_Printf("USAGE: %s <variable> <optional value sequence>\n", Cmd_Argv(0));
        return 0;
    }
    name = (char *)Cmd_Argv(1);
    dvar = Dvar_Lookup(name);
    if (dvar == 0) {
        Com_Printf("toggle failed: dvar '%s' not found.\n", name);
        return 0;
    }
    if (Cmd_Argc() == 2)
        return (unsigned char)Dvar_ToggleDvar(dvar);

    curValue = Dvar_FormatCurrentValue(dvar);
    for (i = 2; i + 1 < Cmd_Argc(); i++) {
        candidate = (char *)Cmd_Argv(i);
        if (dvar->type == 6) {
            resolved = (char *)Dvar_EnumStringFromIndexText(dvar, candidate);
            if (*resolved != '\0')
                candidate = resolved;
        }
        if (strcasecmp(curValue, candidate) == 0) {
            Dvar_SetFromUserInput(name, Cmd_Argv(i + 1));
            return 1;
        }
    }
    candidate = (char *)Cmd_Argv(2);
    if (dvar->type == 6) {
        resolved = (char *)Dvar_EnumStringFromIndexText(dvar, candidate);
        if (*resolved != '\0')
            candidate = resolved;
    }
    Dvar_SetFromUserInput(name, candidate);
    return 1;
}

// 0x0806354a  size=13  FUN_0806354a  status=translated  naming=inferred
// Registered as the "toggle" handler; forwards to Dvar_Toggle_f.
void Cmd_Toggle_f(void)
{
    Dvar_Toggle_f();
}

// 0x080635b0  size=159  FUN_080635b0  status=translated  naming=observed
// "set" command: "USAGE: set <variable> <value>", "invalid variable name: %s".
void Cmd_Set_f(void)
{
    char buf[4108];

    if (Cmd_Argc() < 3) {
        Com_Printf("USAGE: set <variable> <value>\n");
    } else if (Com_IsIdentifierName(Cmd_Argv(1)) == 0) {
        Com_Printf("invalid variable name: %s\n", Cmd_Argv(1));
    } else {
        Cmd_ArgsFromBuffer(buf, 2);
        Dvar_SetFromUserInput(Cmd_Argv(1), buf);
    }
}

// 0x08063a3c  size=87  FUN_08063a3c  status=translated  naming=observed
// "seta" command: runs Cmd_Set_f then adds flag 1 (archive) to the dvar.
void Cmd_SetA_f(void)
{
    dvar_s *dvar;

    if (Cmd_Argc() < 3) {
        Com_Printf("USAGE: seta <variable> <value>\n");
    } else {
        Cmd_Set_f();
        dvar = Dvar_Lookup(Cmd_Argv(1));
        if (dvar != 0)
            Dvar_OrFlags(dvar, 1);
    }
}

// 0x08063b6c  size=122  FUN_08063b6c  status=translated  naming=inferred
// Writes "seta name "latched"" lines for archive-flag dvars except cl_cdkey.
void Dvar_WriteVariables(int file)
{
    dvar_s *dvar;

    for (dvar = dvar_vars; dvar != 0; dvar = dvar->next) {
        if (Q_stricmp(dvar->name, "cl_cdkey") != 0 && (((uint8)dvar->flags ^ 1) & 1) == 0)
            FS_Printf(file, "seta %s \"%s\"\n", dvar->name, Dvar_FormatLatchedValue(dvar));
    }
}

// 0x08063be6  size=123  FUN_08063be6  status=translated  naming=inferred
// Writes "set name "default"" lines for dvars with none of flags 0x40c0 set, except cl_cdkey.
void Dvar_WriteDefaults(int file)
{
    dvar_s *dvar;

    for (dvar = dvar_vars; dvar != 0; dvar = dvar->next) {
        if (Q_stricmp(dvar->name, "cl_cdkey") != 0 && (dvar->flags & 0x40c0) == 0)
            FS_Printf(file, "set %s \"%s\"\n", dvar->name, Dvar_FormatResetValue(dvar));
    }
}

// 0x08063e2c  size=444  FUN_08063e2c  status=translated  naming=observed
// Dvar dump ("=== DVAR DUMP ==="), optional name filter from Cmd_Argv(1), to file/console handle.
void Dvar_Dump(int handle)
{
    char line[0x2000];
    const char *filter;
    int count;
    dvar_s *dvar;

    if (Cmd_Argc() < 2)
        filter = 0;
    else
        filter = Cmd_Argv(1);
    if (handle != 0 || (com_logfile != 0 && com_logfile->current.integer != 0)) {
        count = 0;
        Com_PrintMessage(handle,
                    "=============================== DVAR DUMP ========================================\n");
        for (dvar = dvar_vars; dvar != 0; dvar = dvar->next) {
            if (filter == 0 || Com_Filter(filter, dvar->name, 0) != 0) {
                if (Dvar_HasPendingLatch(dvar) == 0) {
                    Com_sprintf(line, 0x2000, "      %s \"%s\"\n", dvar->name, Dvar_FormatCurrentValue(dvar));
                } else {
                    const char *latched = Dvar_FormatLatchedValue(dvar);
                    const char *shown = Dvar_FormatCurrentValue(dvar);
                    Com_sprintf(line, 0x2000, "      %s \"%s\" -- latched \"%s\"\n", dvar->name, shown, latched);
                }
                Com_PrintMessage(handle, line);
            }
            count = count + 1;
        }
        Com_sprintf(line, 0x2000, "\n%i total dvars\n%i dvar indexes\n", count, dvar_indexCount);
        Com_PrintMessage(handle, line);
        Com_PrintMessage(handle,
                    "=============================== END DVAR DUMP =====================================\n");
    }
}

// 0x08063fe8  size=20  FUN_08063fe8  status=translated  naming=observed
// "dvardump" command handler.
void Dvar_Dump_f(void)
{
    Dvar_Dump(0);
}

// 0x08063ffc  size=48  FUN_08063ffc  status=translated  naming=inferred
// Sets an existing dvar from a string (no-op if it does not exist).
void Dvar_SetIfExists(const char *name, const char *value)
{
    if (Dvar_Lookup(name) != 0)
        Dvar_SetStringOrCreate(name, value);
}

// 0x0806402c  size=137  FUN_0806402c  status=translated  naming=inferred
// Iterator over the dvar list using a static cursor; returns false at the end.
bool Dvar_GetNext(const char **name, const char **value, unsigned int *flags, const char **defaultValue)
{
    bool more;

    if (dvar_iterCursor == 0)
        dvar_iterCursor = dvar_vars;
    else
        dvar_iterCursor = dvar_iterCursor->next;
    more = dvar_iterCursor != 0;
    if (more) {
        *name = dvar_iterCursor->name;
        *value = Dvar_FormatCurrentValue(dvar_iterCursor);
        *flags = (unsigned int)dvar_iterCursor->flags;
        *defaultValue = Dvar_FormatResetValue(dvar_iterCursor);
    }
    return more;
}

// 0x0806412c  size=91  FUN_0806412c  status=translated  naming=inferred
// Calls SV_SetConfigValueForKey(a, b, name, displayValue) for every dvar whose flags intersect the mask.
void Dvar_ForEachWithFlags(int a, int b, uint16 mask)
{
    dvar_s *dvar;

    for (dvar = dvar_vars; dvar != 0; dvar = dvar->next) {
        if ((dvar->flags & mask) != 0)
            SV_SetConfigValueForKey(a, b, (char *)dvar->name, (char *)Dvar_FormatCurrentValue(dvar));
    }
}

// 0x08064188  size=97  FUN_08064188  status=translated  naming=inferred
// Dvar_InfoString: builds an info string of name/value for dvars matching the flag mask.
char *Dvar_InfoString(uint16 mask)
{
    dvar_s *dvar;

    g_081d8b00[0] = 0;
    for (dvar = dvar_vars; dvar != 0; dvar = dvar->next) {
        if ((dvar->flags & mask) != 0)
            Info_SetValueForKey(g_081d8b00, dvar->name, Dvar_FormatCurrentValue(dvar));
    }
    return g_081d8b00;
}

// 0x080641ea  size=97  FUN_080641ea  status=translated  naming=inferred
// Dvar_InfoString_Big: same with the big info-string writer into the larger buffer.
char *Dvar_InfoString_Big(uint16 mask)
{
    dvar_s *dvar;

    g_081d8f00[0] = 0;
    for (dvar = dvar_vars; dvar != 0; dvar = dvar->next) {
        if ((dvar->flags & mask) != 0)
            Info_SetValueForKey_Big(g_081d8f00, dvar->name, Dvar_FormatCurrentValue(dvar));
    }
    return g_081d8f00;
}

// 0x0806424c  size=268  FUN_0806424c  status=translated  naming=observed
// Registers dvar console commands ("toggle", "togglep", "set", "sets", "seta", "setfromdvar",
// "reset", "dvarlist", "dvardump", "dvar_bool", "dvar_int", "dvar_float", "setu").
void Dvar_AddCommands(void)
{
    Cmd_AddCommand("toggle", Cmd_Toggle_f);
    Cmd_AddCommand("togglep", Cmd_ToggleDvar_f);
    Cmd_AddCommand("set", Cmd_Set_f);
    Cmd_AddCommand("sets", Cmd_SetServer_f);
    Cmd_AddCommand("seta", Cmd_SetA_f);
    Cmd_AddCommand("setfromdvar", Cmd_SetFromDvar_f);
    Cmd_AddCommand("reset", Cmd_ResetDvar_f);
    Cmd_AddCommand("dvarlist", Dvar_List_f);
    Cmd_AddCommand("dvardump", Dvar_Dump_f);
    Cmd_AddCommand("dvar_bool", Cmd_RegisterBool_f);
    Cmd_AddCommand("dvar_int", Cmd_RegisterInt_f);
    Cmd_AddCommand("dvar_float", Cmd_RegisterFloat_f);
    Cmd_AddCommand("setu", Cmd_SetUser_f);
}

// 0x08064358  size=144  FUN_08064358  status=translated  naming=inferred
// FS_SV_FileExists: builds the fs_homepath OS path, fopen "rb", closes it, returns existence.
bool FS_SV_FileExists(const char *qpath)
{
    char ospath[256];
    FILE *fp;
    size_t len;

    FS_BuildOSPath_Fatal(fs_homepath->current.string, qpath, "", ospath);
    len = strlen(ospath);
    ospath[len - 1] = '\0';
    fp = (FILE *)Com_Fopen(ospath, "rb");
    if (fp != 0)
        Com_Fclose(fp);
    return fp != 0;
}

// 0x080643e8  size=368  FUN_080643e8  status=translated  naming=observed
// FS_SV_FOpenFileWrite: string "FS_SV_FOpenFileWrite: %s\n", "writing to: %s\n".
int FS_SV_FOpenFileWrite(const char *filename)
{
    char ospath[260];
    size_t len;
    int handle;
    int failed;

    FS_Nop();
    FS_BuildOSPath_Fatal(fs_homepath->current.string, filename, "", ospath);
    len = strlen(ospath);
    ospath[len - 1] = '\0';
    handle = FS_HandleForFile(0);
    fsh[handle].iwd = 0;
    if (fs_debug->current.integer != 0)
        Com_Printf("FS_SV_FOpenFileWrite: %s\n", ospath);
    failed = FS_CreatePath(ospath);
    if (failed != 0)
        return 0;
    Com_DPrintf("writing to: %s\n", ospath);
    fsh[handle].handleFiles = Com_Fopen(ospath, "wb");
    Q_strncpyz(fsh[handle].name, filename, 0x100);
    fsh[handle].handleSync = 0;
    if (fsh[handle].handleFiles == 0)
        handle = 0;
    return handle;
}

// 0x08064558  size=800  FUN_08064558  status=translated  naming=observed
// FS_SV_FOpenFileRead: tries fs_homepath, fs_basepath (if different), then fs_cdpath. Returns length.
// Quirk preserved: after a failed fs_basepath attempt the handle becomes 0, and the following
// checks/stores then operate on fsh[0].
int FS_SV_FOpenFileRead(const char *filename, int *fp)
{
    char ospath[260];
    size_t len;
    int handle = 0;

    FS_Nop();
    handle = FS_HandleForFile(0);
    fsh[handle].iwd = 0;
    Q_strncpyz(fsh[handle].name, filename, 0x100);
    FS_BuildOSPath_Fatal(fs_homepath->current.string, filename, "", ospath);
    len = strlen(ospath);
    ospath[len - 1] = '\0';
    if (fs_debug->current.integer != 0)
        Com_Printf("FS_SV_FOpenFileRead (fs_homepath): %s\n", ospath);
    fsh[handle].handleFiles = Com_Fopen(ospath, "rb");
    fsh[handle].handleSync = 0;
    if (fsh[handle].handleFiles == 0) {
        if (Q_stricmp(fs_homepath->current.string, fs_basepath->current.string) != 0) {
            FS_BuildOSPath_Fatal(fs_basepath->current.string, filename, "", ospath);
            len = strlen(ospath);
            ospath[len - 1] = '\0';
            if (fs_debug->current.integer != 0)
                Com_Printf("FS_SV_FOpenFileRead (fs_basepath): %s\n", ospath);
            fsh[handle].handleFiles = Com_Fopen(ospath, "rb");
            fsh[handle].handleSync = 0;
            if (fsh[handle].handleFiles == 0)
                handle = 0;
        }
    }
    if (fsh[handle].handleFiles == 0) {
        FS_BuildOSPath_Fatal(fs_cdpath->current.string, filename, "", ospath);
        len = strlen(ospath);
        ospath[len - 1] = '\0';
        if (fs_debug->current.integer != 0)
            Com_Printf("FS_SV_FOpenFileRead (fs_cdpath) : %s\n", ospath);
        fsh[handle].handleFiles = Com_Fopen(ospath, "rb");
        fsh[handle].handleSync = 0;
        if (fsh[handle].handleFiles == 0)
            handle = 0;
    }
    *fp = handle;
    if (handle == 0)
        return 0;
    return FS_filelength(handle);
}

// 0x08064878  size=251  FUN_08064878  status=translated  naming=observed
// FS_SV_Rename: string "FS_SV_Rename: %s --> %s\n"; falls back to copy + remove if rename fails.
void FS_SV_Rename(const char *from, const char *to)
{
    char fromPath[256];
    char toPath[256];
    size_t len;

    FS_Nop();
    FS_BuildOSPath_Fatal(fs_homepath->current.string, from, "", fromPath);
    FS_BuildOSPath_Fatal(fs_homepath->current.string, to, "", toPath);
    len = strlen(fromPath);
    fromPath[len - 1] = '\0';
    len = strlen(toPath);
    toPath[len - 1] = '\0';
    if (fs_debug->current.integer != 0)
        Com_Printf("FS_SV_Rename: %s --> %s\n", fromPath, toPath);
    if (rename(fromPath, toPath) != 0) {
        FS_CopyFile(fromPath, toPath);
        FS_Remove(fromPath);
    }
}

// 0x08064974  size=87  FUN_08064974  status=translated  naming=unknown
// Adds a signed char offset to each byte of src into a static buffer (0x081e9100) and returns it.
char *Com_ShiftString(const char *src, char shift)
{
    int len = (int)strlen(src);
    int i;

    for (i = 0; i < len; i++)
        g_081e9100[i] = (char)(shift + src[i]);
    g_081e9100[i] = 0;
    return g_081e9100;
}

// 0x08064a6e  size=45  FUN_08064a6e  status=translated  naming=inferred
// Length of a NULL-terminated pointer list (0 for NULL).
int Com_ListLength(char **list)
{
    int count = 0;
    if (list != 0) {
        for (; *list != 0; list++)
            count = count + 1;
    }
    return count;
}

// 0x08064a9c  size=303  FUN_08064a9c  status=translated  naming=inferred
// Concatenates three NULL-terminated pointer lists into a new allocation and frees the inputs.
char **FS_ConcatenateLists(char **a, char **b, char **c)
{
    int na = Com_ListLength(a);
    int nb = Com_ListLength(b);
    int nc = Com_ListLength(c);
    char **result = (char **)Com_MallocZeroed((na + nb + nc) * 4 + 4);
    char **out = result;
    char **p;

    if (a != 0) {
        for (p = a; *p != 0; p++) {
            *out = *p;
            out++;
        }
    }
    if (b != 0) {
        for (p = b; *p != 0; p++) {
            *out = *p;
            out++;
        }
    }
    if (c != 0) {
        for (p = c; *p != 0; p++) {
            *out = *p;
            out++;
        }
    }
    *out = 0;
    if (a != 0)
        Com_Free_A(a);
    if (b != 0)
        Com_Free_A(b);
    if (c != 0)
        Com_Free_A(c);
    return result;
}

// 0x08064bcc  size=1288  FUN_08064bcc  status=translated  naming=inferred
// FS_GetModList: lists mod directories containing .iwd files across homepath/basepath/cdpath and
// packs "name\0description\0" pairs into listbuf. Strings "description.txt",
// "Call of Duty 2 Multiplayer", "main". Returns number of mods.
int FS_GetModList(char *listbuf, int bufsize)
{
    int numFiles;
    int nTotal = 0;
    int nMods = 0;
    int nLen;
    int nDescLen;
    int i;
    int j;
    bool dup = false;
    int numIwd;
    int descHandle;
    int descRead;
    char **list;
    char **dirsHome;
    char **dirsBase;
    char **dirsCd = 0;
    char **iwdList;
    char *name;
    char descPath[256];
    char osPath[260];

    *listbuf = '\0';
    dirsHome = Sys_ListFiles(fs_homepath->current.string, 0, 0, &numFiles, 1);
    dirsBase = Sys_ListFiles(fs_basepath->current.string, 0, 0, &numFiles, 1);
    if (fs_cdpath->current.string != 0 && *fs_cdpath->current.string != '\0')
        dirsCd = Sys_ListFiles(fs_cdpath->current.string, 0, 0, &numFiles, 1);
    list = FS_ConcatenateLists(dirsHome, dirsBase, dirsCd);
    numFiles = Com_ListLength(list);
    for (i = 0; i < numFiles; i++) {
        name = list[i];
        if (i != 0) {
            dup = false;
            for (j = 0; j < i; j++) {
                if (Q_stricmp(list[j], name) == 0) {
                    dup = true;
                    break;
                }
            }
        }
        if (!dup && Q_stricmpn(name, ".", 1) != 0) {
            FS_BuildOSPath_Fatal(fs_basepath->current.string, name, "", osPath);
            numIwd = 0;
            iwdList = Sys_ListFiles(osPath, "iwd", 0, &numIwd, 0);
            Sys_FreeFileList(iwdList);
            if (numIwd < 1) {
                FS_BuildOSPath_Fatal(fs_cdpath->current.string, name, "", osPath);
                numIwd = 0;
                iwdList = Sys_ListFiles(osPath, "iwd", 0, &numIwd, 0);
                Sys_FreeFileList(iwdList);
            }
            if (numIwd < 1) {
                FS_BuildOSPath_Fatal(fs_homepath->current.string, name, "", osPath);
                numIwd = 0;
                iwdList = Sys_ListFiles(osPath, "iwd", 0, &numIwd, 0);
                Sys_FreeFileList(iwdList);
            }
            if (0 < numIwd) {
                nLen = (int)strlen(name) + 1;
                strcpy(descPath, name);
                I_strncat(descPath, 0x100, "/description.txt");
                descRead = FS_SV_FOpenFileRead(descPath, &descHandle);
                if (descRead < 1 || descHandle == 0) {
                    if (Q_stricmp(name, "main") == 0)
                        strcpy(descPath, "Call of Duty 2 Multiplayer");
                    else
                        strcpy(descPath, name);
                } else {
                    FILE *descFile = (FILE *)FS_FileForHandle(descHandle);
                    Com_Memset(descPath, 0, 0x100);
                    descRead = Com_Fread(descPath, 1, 0x30, descFile);
                    if (-1 < descRead)
                        descPath[descRead] = '\0';
                    FS_FCloseFile(descHandle);
                }
                nDescLen = (int)strlen(descPath) + 1;
                if (bufsize <= nLen + nTotal + nDescLen + 2)
                    break;
                strcpy(listbuf, name);
                listbuf = listbuf + nLen;
                strcpy(listbuf, descPath);
                listbuf = listbuf + nDescLen;
                nTotal = nTotal + nLen + nDescLen;
                nMods = nMods + 1;
            }
        }
    }
    Sys_FreeFileList(list);
    return nMods;
}

// 0x080650d4  size=268  FUN_080650d4  status=translated  naming=observed
// "dir" command: "usage: dir <directory> [extension]", "Directory of %s %s".
void FS_Dir_f(void)
{
    const char *path;
    const char *extension;
    int numFiles;
    char **files;
    int i;

    if (Cmd_Argc() < 2 || Cmd_Argc() > 3) {
        Com_Printf("usage: dir <directory> [extension]\n");
        return;
    }
    if (Cmd_Argc() == 2) {
        path = Cmd_Argv(1);
        extension = "";
    } else {
        path = Cmd_Argv(1);
        extension = Cmd_Argv(2);
    }
    Com_Printf("Directory of %s %s\n", path, extension);
    Com_Printf("---------------\n");
    files = FS_ListFiles(path, extension, 0, &numFiles, 10);
    for (i = 0; i < numFiles; i++)
        Com_Printf("%s\n", files[i]);
    FS_FreeFileList(files, 10);
}

// 0x080651e0  size=272  FUN_080651e0  status=translated  naming=observed
// "fdir" command: "usage: fdir <filter>", "%d files listed".
void FS_NewDir_f(void)
{
    const char *filter;
    int numFiles;
    char **files;
    int i;

    if (Cmd_Argc() < 2) {
        Com_Printf("usage: fdir <filter>\n");
        Com_Printf("example: fdir *q3dm*.bsp\n");
    } else {
        filter = Cmd_Argv(1);
        Com_Printf("---------------\n");
        files = FS_ListFilteredFilesInList(fs_searchpaths, "", "", filter, 0, &numFiles, 10);
        FS_SortFileList(files, numFiles);
        for (i = 0; i < numFiles; i++) {
            FS_NormalizeSeparators(files[i]);
            Com_Printf("%s\n", files[i]);
        }
        Com_Printf("%d files listed\n", numFiles);
        FS_FreeFileList(files, 10);
    }
}

// 0x080652f0  size=52  FUN_080652f0  status=translated  naming=observed
// "touchFile" command: "Usage: touchFile <file>".
void FS_TouchFile_f(void)
{
    if (Cmd_Argc() == 2)
        FS_SearchFileExists(Cmd_Argv(1));
    else
        Com_Printf("Usage: touchFile <file>\n");
}

// 0x08065324  size=299  FUN_08065324  status=translated  naming=inferred
// FS_iwIwd: true if iwd matches base/iw_00..iw_24 or a localized_ file with an _iwNN marker.
// Uses a 76-byte local buffer and an unbounded strcpy exactly as the binary does.
int FS_iwIwd(char *iwd, const char *base)
{
    char buf[76];
    char *loc;
    int i;

    for (i = 0; i < 0x19; i++) {
        char *pattern = va("%s/iw_%02d", base, i);
        if (FS_FilenameCompare(iwd, pattern) == 0)
            return 1;
    }
    loc = strstr(iwd, "localized_");
    if (loc != 0) {
        strcpy(buf, iwd);
        buf[(loc - iwd) + 10] = '\0';
        if (FS_FilenameCompare(buf, va("%s/localized_", base)) == 0) {
            strcpy(buf, loc + 10);
            Q_strlwr(buf);
            for (i = 0; i < 0x19; i++) {
                char *marker = va("_iw%02d", i);
                if (strstr(buf, marker) != 0)
                    return 1;
            }
        }
    }
    return 0;
}

// 0x08065450  size=79  FUN_08065450  status=translated  naming=inferred
// True if the lower-cased name (copied to a 72-byte buffer) contains "_svr_".
bool FS_IwdNameHasSvrMarker(const char *name)
{
    char buf[72];

    strcpy(buf, name);
    Q_strlwr(buf);
    return strstr(buf, "_svr_") != 0;
}

// 0x080657dc  size=108  FUN_080657dc  status=translated  naming=observed
// Registers "path", "fullpath", "dir", "fdir", "touchFile".
void FS_AddCommands(void)
{
    Cmd_AddCommand("path", FS_PrintPureSearchPaths);
    Cmd_AddCommand("fullpath", FS_PrintSearchPathsCmd);
    Cmd_AddCommand("dir", FS_Dir_f);
    Cmd_AddCommand("fdir", FS_NewDir_f);
    Cmd_AddCommand("touchFile", FS_TouchFile_f);
}

// 0x08065848  size=193  FUN_08065848  status=translated  naming=inferred
// Restricted demo mode: string "Running in restricted demo mode", "Corrupted iw0.iwd: %u".
void FS_SetRestrictedDemoMode(void)
{
    searchpath_s *sp;

    if (fs_restrict->current.enabled != 0) {
        Dvar_SetBool(fs_restrict, 1);
        Com_Printf("\nRunning in restricted demo mode.\n\n");
        ((void (*)(int))FS_Shutdown)(0);
        FS_Startup("demomain");
        for (sp = fs_searchpaths; sp != 0; sp = sp->next) {
            if (FS_SearchPathAllowed(sp) != 0 && sp->iwd != 0 && (int)sp->iwd->checksum != -0x4e0a6a0b)
                Com_Error(0, "Corrupted iw0.iwd: %u", sp->iwd->checksum);
        }
    }
}

// 0x0806590a  size=120  FUN_0806590a  status=translated  naming=inferred
// Space-separated "%i " checksums of loaded non-localized iwds.
char *FS_LoadedIwdChecksums(void)
{
    searchpath_s *sp;

    g_081db100[0] = 0;
    for (sp = fs_searchpaths; sp != 0; sp = sp->next) {
        if (sp->iwd != 0 && sp->bLocalized == 0 /* binary: cmp [sp+0xc],0 */)
            I_strncat(g_081db100, 0x2000, va("%i ", sp->iwd->checksum));
    }
    return g_081db100;
}

// 0x08065982  size=140  FUN_08065982  status=translated  naming=inferred
// Space-separated basenames of loaded iwds.
char *FS_LoadedIwdNames(void)
{
    searchpath_s *sp;

    g_081dd100[0] = '\0';
    for (sp = fs_searchpaths; sp != 0; sp = sp->next) {
        if (sp->iwd != 0 && sp->bLocalized == 0 /* binary: cmp [sp+0xc],0 */) {
            if (g_081dd100[0] != '\0')
                I_strncat(g_081dd100, 0x2000, " ");
            I_strncat(g_081dd100, 0x2000, sp->iwd->iwdBasename);
        }
    }
    return g_081dd100;
}

// 0x08065a0e  size=120  FUN_08065a0e  status=translated  naming=inferred
// "%i " pure checksums of loaded iwds.
char *FS_LoadedIwdPureChecksums(void)
{
    searchpath_s *sp;

    g_081df100[0] = 0;
    for (sp = fs_searchpaths; sp != 0; sp = sp->next) {
        if (sp->iwd != 0 && sp->bLocalized == 0 /* binary: cmp [sp+0xc],0 */)
            I_strncat(g_081df100, 0x2000, va("%i ", sp->iwd->pure_checksum));
    }
    return g_081df100;
}

// 0x08065a86  size=171  FUN_08065a86  status=translated  naming=inferred
// Checksums of referenced iwds (referenced flag, or gamename not starting with "main").
char *FS_ReferencedIwdChecksums(void)
{
    searchpath_s *sp;

    g_081e1100[0] = 0;
    for (sp = fs_searchpaths; sp != 0; sp = sp->next) {
        if (sp->iwd != 0 &&
            (sp->iwd->referenced != 0 || Q_stricmpn(sp->iwd->iwdGamename, "main", 4) != 0))
            I_strncat(g_081e1100, 0x2000, va("%i ", sp->iwd->checksum));
    }
    return g_081e1100;
}

// 0x08065b32  size=260  FUN_08065b32  status=translated  naming=inferred
// "gamename/basename" tokens of referenced iwds, space separated.
char *FS_ReferencedIwdNames(void)
{
    searchpath_s *sp;

    g_081e7100[0] = '\0';
    for (sp = fs_searchpaths; sp != 0; sp = sp->next) {
        if (sp->iwd != 0 &&
            (sp->iwd->referenced != 0 || Q_stricmpn(sp->iwd->iwdGamename, "main", 4) != 0)) {
            if (g_081e7100[0] != '\0')
                I_strncat(g_081e7100, 0x2000, " ");
            I_strncat(g_081e7100, 0x2000, sp->iwd->iwdGamename);
            I_strncat(g_081e7100, 0x2000, "/");
            I_strncat(g_081e7100, 0x2000, sp->iwd->iwdBasename);
        }
    }
    return g_081e7100;
}

// 0x08065c36  size=356  FUN_08065c36  status=translated  naming=inferred
// FS_ReferencedIwdPureChecksums: starts with "@ ", appends referenced pure checksums, optional
// fs_unpureFileToken, then the XOR of the feed, all checksums and the count.
char *FS_ReferencedIwdPureChecksums(void)
{
    searchpath_s *sp;
    uint32 checksum;
    uint32 count;
    size_t len;

    g_081e3100[0] = 0;
    checksum = fs_checksumFeed;
    count = 0;
    len = strlen(g_081e3100);
    g_081e3100[len + 1] = 0;
    len = strlen(g_081e3100);
    g_081e3100[len + 2] = 0;
    len = strlen(g_081e3100);
    g_081e3100[len] = 0x40;
    len = strlen(g_081e3100);
    g_081e3100[len] = 0x20;
    for (sp = fs_searchpaths; sp != 0; sp = sp->next) {
        if (sp->iwd != 0 && sp->bLocalized == 0 /* binary: cmp [sp+0xc],0 */ && sp->iwd->referenced != 0) {
            I_strncat(g_081e3100, 0x2000, va("%i ", sp->iwd->pure_checksum));
            checksum = checksum ^ sp->iwd->pure_checksum;
            count = count + 1;
        }
    }
    if (fs_unpureFileToken != 0)
        I_strncat(g_081e3100, 0x2000, va("%i ", fs_unpureFileToken));
    I_strncat(g_081e3100, 0x2000, va("%i ", checksum ^ count));
    return g_081e3100;
}

// 0x08065d9a  size=506  FUN_08065d9a  status=translated  naming=observed
// FS_PureServerSetLoadedIwds: string "iwd sum/name mismatch", "Connected to a pure server.\n".
void FS_PureServerSetLoadedIwds(const char *sums, const char *names)
{
    int checksums[1024];
    char *nameList[1024];
    int numSums;
    int numNames;
    int i;
    int j;

    Cmd_TokenizeString((char *)sums);
    numSums = Cmd_Argc();
    if (0x400 < numSums)
        numSums = 0x400;
    for (i = 0; i < numSums; i++)
        checksums[i] = atoi(Cmd_Argv(i));
    Cmd_TokenizeString((char *)names);
    numNames = Cmd_Argc();
    if (0x400 < numNames)
        numNames = 0x400;
    for (i = 0; i < numNames; i++)
        nameList[i] = CopyString(Cmd_Argv(i));
    if (numSums != numNames)
        Com_Error(1, "iwd sum/name mismatch");
    if (numSums == fs_numServerIwds) {
        for (i = 0; i < numSums; i++) {
            j = 0;
            for (;;) {
                if (fs_numServerIwds <= j)
                    goto reset_list;
                if (checksums[i] == fs_serverIwds[j] &&
                    Q_stricmp(nameList[i], fs_serverIwdNames[j]) == 0)
                    break;
                j++;
            }
        }
        for (i = 0; i < numNames; i++)
            Com_Free_A(nameList[i]);
    } else {
reset_list:
        FS_ClearListA();
        fs_numServerIwds = numSums;
        if (numSums != 0) {
            Com_DPrintf("Connected to a pure server.\n");
            Com_Memcpy(fs_serverIwds, checksums, fs_numServerIwds << 2);
            Com_Memcpy(fs_serverIwdNames, nameList, fs_numServerIwds << 2);
            fs_unpureFileToken = 0;
        }
    }
}
