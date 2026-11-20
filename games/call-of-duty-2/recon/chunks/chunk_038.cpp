#include "cod2_types.h"

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <dlfcn.h>
#include <dirent.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdarg.h>
#include "cod2_canon.h"
#include "cod2_globals.h"
/* cod2_prototypes.h (generated) also lists the 132 PLT import thunks of chunk 000 without extern "C";
 * they must not clash with the system headers, so rename them while the shared prototypes are read. */
#define COD2_PLT_SHIM_BEGIN
#include "cod2_prototypes.h"
#include "chunk_038_types.h"

/* ------------------------------------------------------------------ */
/* tty console history                                                 */
/* ------------------------------------------------------------------ */

// 0x080d38d8  size=220  FUN_080d38d8  status=translated  naming=inferred
// Evidence: shifts a 32-entry ring of 0x118-byte field_t lines and stores the new line at [0] (Q3 Hist_Add); called only by Sys_ConsoleInput on '\n'.
void Tty_HistAdd(field_t *field)
{
    int i;

    for (i = 31; i > 0; i--) {
        tty_history[i] = tty_history[i - 1];
    }
    tty_history[0] = *field;
    if (tty_history_count < 32) {
        tty_history_count++;
    }
    tty_hist_current = -1;
}

// 0x080d39b4  size=65  FUN_080d39b4  status=translated  naming=inferred
// Evidence: increments the history cursor while below the stored count; used for the ESC [ A (up arrow) branch of Sys_ConsoleInput (Q3 Hist_Next).
field_t *Tty_HistNext(void)
{
    if (tty_hist_current + 1 < tty_history_count) {
        tty_hist_current++;
        return &tty_history[tty_hist_current];
    }
    return (field_t *)0;
}

// 0x080d39f6  size=63  FUN_080d39f6  status=translated  naming=inferred
// Evidence: decrements the history cursor, returns NULL at -1; used for the ESC [ B (down arrow) branch of Sys_ConsoleInput (Q3 Hist_Prev).
field_t *Tty_HistPrev(void)
{
    if (tty_hist_current > -1) {
        tty_hist_current--;
    }
    if (tty_hist_current == -1) {
        return (field_t *)0;
    }
    return &tty_history[tty_hist_current];
}

// 0x080d3a36  size=67  FUN_080d3a36  status=translated  naming=inferred
// Evidence: Q3 Sys_Exit shape: tty shutdown (0x080d389e), optional delayed process start (see Sys_StartProcess), then exit(code); calls sleep/exit imports.
void Sys_Exit(int code)
{
    Tty_Shutdown();
    if (sys_exit_cmdline[0] != '\0') {
        sleep(1);
        Sys_DoStartProcess(sys_exit_cmdline);
        sleep(1);
    }
    exit(code);
}

// 0x080d3a7a  size=92  FUN_080d3a7a  status=translated  naming=inferred
// Evidence: three shutdown calls, clears O_NONBLOCK (0x800) on stdin, then Sys_Exit(0); called from 0x0806123e (quit path).
void Sys_Quit(void)
{
    unsigned int flags;

    Dvar_Shutdown();
    Cmd_Shutdown();
    Hunk_Shutdown();
    flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags & 0xfffff7ff);
    Sys_Exit(0);
}

// 0x080d3ad6  size=101  FUN_080d3ad6  status=translated  naming=inferred
// Evidence: registers command "in_restart" and cvars "arch"="linux i386" and "username" (Sys_GetCurrentUser); called from 0x080620c0 init path (Q3 Sys_Init).
void Sys_InitCvars(void)
{
    const char *user;

    Cmd_AddCommand("in_restart", Sys_In_Restart_f);
    sys_arch_cvar = Dvar_RegisterString("arch", "linux i386", 0x1040);
    user = Sys_GetCurrentUser();
    sys_username_cvar = Dvar_RegisterString("username", user, 0x1040);
}

// 0x080d3b3c  size=159  FUN_080d3b3c  status=translated  naming=observed
// Evidence: string "Sys_Error: %s\n"; vsprintf into a local buffer then Sys_Exit(1); Q3 Sys_Error.
void Sys_Error(const char *fmt, ...)
{
    unsigned int flags;
    char text[1036];
    va_list ap;

    flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags & 0xfffff7ff);
    if (ttycon_on != 0) {
        Tty_HideInput();
    }
    va_start(ap, fmt);
    vsprintf(text, fmt, ap);
    va_end(ap);
    fprintf(stderr, "Sys_Error: %s\n", text);
    Sys_Exit(1);
}

// 0x080d3c44  size=52  FUN_080d3c44  status=translated  naming=inferred
// Evidence: stat() wrapper returning the st_mtime field (offset 0x40 of the i386 struct stat) or -1 on failure (Q3 Sys_FileTime).
int Sys_FileTime(const char *path)
{
    cod2_stat_t st;

    if (FS_StatPath(path, &st) == -1) {
        return -1;
    }
    return st.st_mtime_sec;
}

// 0x080d3c78  size=28  FUN_080d3c78  status=translated  naming=unknown
// Evidence: re-arms itself as the SIGFPE (8) handler via signal(); descriptive name only.
void Sys_FPEHandler(void)
{
    signal(8, (__sighandler_t)Sys_FPEHandler);
}

// 0x080d3c94  size=370  FUN_080d3c94  status=translated  naming=inferred
// Evidence: strings "ttycon", "Started tty console (use +set ttycon 0 to disable)\n", "stdin is not a tty, tty console mode failed\n"; tcgetattr/tcsetattr (Q3 Sys_ConsoleInputInit).
void Sys_ConsoleInputInit(void)
{
    struct termios tc;
    const unsigned int *src;
    unsigned int *dst;
    int i;

    memset(&tc, 0, 0x3c);
    signal(0x15, (__sighandler_t)1);
    signal(0x16, (__sighandler_t)1);
    ttycon_cvar = Dvar_RegisterBool("ttycon", 1, 0x1010);
    Dvar_SetBool(ttycon_cvar, 0);
    if (Dvar_GetBool("ttycon") == '\0') {
        ttycon_on = 0;
    } else if (isatty(0) == 1) {
        Com_Printf("Started tty console (use +set ttycon 0 to disable)\n");
        Field_Clear(&tty_con);
        tcgetattr(0, (struct termios *)tty_tc);
        /* Quirk preserved: the erase/eof characters are read from the still
           zeroed local termios, before the saved copy is made. */
        tty_erase = (unsigned int)tc.c_cc[2];
        tty_eof = (unsigned int)tc.c_cc[4];
        src = (const unsigned int *)tty_tc;
        dst = (unsigned int *)&tc;
        for (i = 0xf; i != 0; i--) {
            *dst = *src;
            src++;
            dst++;
        }
        tc.c_lflag = tc.c_lflag & 0xfffffff5;
        tc.c_iflag = tc.c_iflag & 0xffffffcf;
        tc.c_cc[6] = 1;
        tc.c_cc[5] = 0;
        tcsetattr(0, 1, &tc);
        ttycon_on = 1;
    } else {
        Com_Printf("stdin is not a tty, tty console mode failed\n");
        Dvar_SetBool(ttycon_cvar, 0);
        ttycon_on = 0;
    }
}

// 0x080d3e06  size=1316  FUN_080d3e06  status=translated  naming=inferred
// Evidence: strings "ttycon", "droping ISCTL sequence: %d, tty_erase: %d\n"; tty line editing, history on ESC [ A/B, tab completion, select()+read() on stdin otherwise (Q3 Sys_ConsoleInput).
char *Sys_ConsoleInput(void)
{
    signed char key;
    int n;
    int i;
    field_t *hist;
    fd_set rfds;
    struct timeval tv;
    int oldCursor;

    key = 0;
    if (Dvar_GetBool("ttycon") != '\0') {
        n = read(0, &key, 1);
        oldCursor = tty_con.cursor;
        if (n != -1) {
            if (key == (signed char)tty_erase || key == 0x7f || key == '\b') {
                if (0 < tty_con.cursor) {
                    tty_con.cursor = tty_con.cursor - 1;
                    tty_con.buffer[oldCursor - 1] = 0;
                    Tty_EraseChar();
                }
                return (char *)0;
            }
            if (key != 0 && key < ' ') {
                if (key == '\n') {
                    Tty_HistAdd(&tty_con);
                    strcpy(sys_console_text, tty_con.buffer);
                    Field_Clear(&tty_con);
                    key = '\n';
                    write(1, &key, 1);
                    return sys_console_text;
                }
                if (key == '\t') {
                    Tty_HideInput();
                    Noop_08060b9c(&tty_con);
                    tty_con.cursor = strlen(tty_con.buffer);
                    if (0 < tty_con.cursor && tty_con.buffer[0] == '\\') {
                        for (i = 0; i <= tty_con.cursor; i++) {
                            tty_con.buffer[i] = tty_con.buffer[i + 1];
                        }
                        tty_con.cursor = tty_con.cursor - 1;
                    }
                    Tty_ShowInput();
                    return (char *)0;
                }
                n = read(0, &key, 1);
                if (n != -1 && (key == '[' || key == 'O')) {
                    n = read(0, &key, 1);
                    if (n != -1) {
                        if (key == 'B') {
                            hist = Tty_HistPrev();
                            Tty_HideInput();
                            if (hist == (field_t *)0) {
                                Field_Clear(&tty_con);
                            } else {
                                tty_con = *hist;
                            }
                            Tty_ShowInput();
                            Tty_DrainStdin();
                            return (char *)0;
                        }
                        if (key < 'C') {
                            if (key == 'A') {
                                hist = Tty_HistNext();
                                if (hist != (field_t *)0) {
                                    Tty_HideInput();
                                    tty_con = *hist;
                                    Tty_ShowInput();
                                }
                                Tty_DrainStdin();
                                return (char *)0;
                            }
                        } else {
                            if (key == 'C') {
                                return (char *)0;
                            }
                            if (key == 'D') {
                                return (char *)0;
                            }
                        }
                    }
                }
                Com_DPrintf("droping ISCTL sequence: %d, tty_erase: %d\n", (int)key, tty_erase);
                Tty_DrainStdin();
                return (char *)0;
            }
            tty_con.buffer[tty_con.cursor] = key;
            tty_con.cursor = tty_con.cursor + 1;
            write(1, &key, 1);
        }
        return (char *)0;
    }
    if (stdin_active == 0) {
        return (char *)0;
    }
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    n = select(1, &rfds, (fd_set *)0, (fd_set *)0, &tv);
    if (n == -1 || !FD_ISSET(0, &rfds)) {
        return (char *)0;
    }
    n = read(0, sys_console_text, 0x100);
    if (n == 0) {
        stdin_active = 0;
        return (char *)0;
    }
    if (n < 1) {
        return (char *)0;
    }
    sys_console_text[n - 1] = 0;
    return sys_console_text;
}

// 0x080d432a  size=72  FUN_080d432a  status=translated  naming=observed
// Evidence: strings "Sys_UnloadDll(NULL)\n" and "Sys_UnloadGame failed on dlclose: \"%s\"!\n"; dlclose/dlerror.
void Sys_UnloadDll(void *handle)
{
    const char *err;

    if (handle == (void *)0) {
        Com_Printf("Sys_UnloadDll(NULL)\n");
    } else {
        dlclose(handle);
        err = dlerror();
        if (err != (const char *)0) {
            Com_Printf("Sys_UnloadGame failed on dlclose: \"%s\"!\n", err);
        }
    }
}

// 0x080d4372  size=5  FUN_080d4372  status=translated  naming=unknown
// Evidence: empty body (push ebp; pop ebp; ret); no strings, no referenced state.
void Sys_NoOp_080d4372(void)
{
}

// 0x080d4378  size=5  FUN_080d4378  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d4378(void)
{
}

// 0x080d437e  size=10  FUN_080d437e  status=translated  naming=unknown
// Evidence: returns constant 0, no side effects.
int Sys_ReturnZero_080d437e(void)
{
    return 0;
}

// 0x080d4388  size=10  FUN_080d4388  status=translated  naming=unknown
// Evidence: returns constant 0, no side effects.
int Sys_ReturnZero_080d4388(void)
{
    return 0;
}

// 0x080d4392  size=53  FUN_080d4392  status=translated  naming=observed
// Evidence: string "OUT OF MEMORY! ABORTING!!! (%s:%d)\n" followed by exit(-1).
void Sys_OutOfMemory(const char *file, int line)
{
    fprintf(stderr, "OUT OF MEMORY! ABORTING!!! (%s:%d)\n", file, line);
    exit(-1);
}

// 0x080d4742  size=5  FUN_080d4742  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d4742(void)
{
}

// 0x080d4748  size=5  FUN_080d4748  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d4748(void)
{
}

// 0x080d474e  size=5  FUN_080d474e  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d474e(void)
{
}

// 0x080d4754  size=5  FUN_080d4754  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d4754(void)
{
}

// 0x080d475a  size=37  FUN_080d475a  status=translated  naming=inferred
// Evidence: fread-shaped (buf,size,count,handle) thunk forwarding (buf,size*count,handle) to the file-handle read at 0x080a052a; returns its result.
int Sys_Fread(int buf, int size, int count, int handle)
{
    return FS_Read((void *)buf, size * count, handle);
}

// 0x080d4780  size=33  FUN_080d4780  status=translated  naming=inferred
// Evidence: fseek-shaped (handle,offset,whence) thunk forwarding to 0x080a078a (which itself calls back here); returns its result.
int Sys_Fseek(int handle, int offset, int whence)
{
    return FS_Seek(handle, offset, whence);
}

// 0x080d47a2  size=172  FUN_080d47a2  status=translated  naming=observed
// Evidence: string "Sys_QueEvent: overflow\n"; 256-entry ring of 0x18-byte events (Q3 Sys_QueEvent).
void Sys_QueEvent(int time, int type, int value, int value2, int ptrLength, void *ptr)
{
    sysEvent_t *ev;

    ev = &eventQue[eventHead & 0xff];
    if (0xff < eventHead - eventTail) {
        Com_Printf("Sys_QueEvent: overflow\n");
        if (ev->evPtr != (void *)0) {
            Com_Free_A(ev->evPtr);
        }
        eventTail = eventTail + 1;
    }
    eventHead = eventHead + 1;
    if (time == 0) {
        time = Sys_Milliseconds();
    }
    ev->evTime = time;
    ev->evType = type;
    ev->evValue = value;
    ev->evValue2 = value2;
    ev->evPtrLength = ptrLength;
    ev->evPtr = ptr;
}

// 0x080d484e  size=632  FUN_080d484e  status=translated  naming=inferred
// Evidence: pops the event ring, else polls Sys_ConsoleInput (queues type 4) and Sys_GetPacket (queues type 5 with a 20-byte netadr prefix) (Q3 Sys_GetEvent).
sysEvent_t Sys_GetEvent(void)
{
    sysEvent_t ev;
    msg_t netmsg;
    netadr_t adr;
    char *s;
    size_t len;
    char *b;
    int total;
    byte *pkt;
    int idx;

    if (eventTail < eventHead) {
        idx = eventTail;
        eventTail = eventTail + 1;
        ev = eventQue[idx & 0xff];
        return ev;
    }
    s = Sys_ConsoleInput();
    if (s != (char *)0) {
        len = strlen(s);
        b = (char *)Com_MallocZeroed(len + 1);
        strcpy(b, s);
        Sys_QueEvent(0, SE_CONSOLE_CODE, 0, 0, len + 1, b);
    }
    MSG_Init(&netmsg, sys_packetReceived, 0x20000);
    if (Sys_GetPacket(&adr, &netmsg) != 0) {
        total = netmsg.cursize + 0x14;
        pkt = (byte *)Com_MallocZeroed(total);
        memcpy(pkt, &adr, 0x14);
        memcpy(pkt + 0x14, netmsg.data, netmsg.cursize);
        Sys_QueEvent(0, SE_PACKET_CODE, 0, 0, total, pkt);
    }
    if (eventTail < eventHead) {
        idx = eventTail;
        eventTail = eventTail + 1;
        ev = eventQue[idx & 0xff];
    } else {
        memset(&ev, 0, 0x18);
        ev.evTime = Sys_Milliseconds();
    }
    return ev;
}

// 0x080d4ad0  size=5  FUN_080d4ad0  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d4ad0(void)
{
}

// 0x080d4ad6  size=10  FUN_080d4ad6  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d4ad6(void)
{
    return 0;
}

// 0x080d4ae0  size=56  FUN_080d4ae0  status=translated  naming=inferred
// Evidence: hides the tty prompt, fputs(msg, stderr), shows it again (Q3 Sys_Print).
void Sys_Print(const char *msg)
{
    if (ttycon_on != 0) {
        Tty_HideInput();
    }
    fputs(msg, stderr);
    if (ttycon_on != 0) {
        Tty_ShowInput();
    }
}

// 0x080d4b18  size=18  FUN_080d4b18  status=translated  naming=unknown
// Evidence: executes fnstcw into a dead local initialised to 0; no observable result besides the instruction.
void Sys_ConfigureFPU(void)
{
    int word = 0;
    __asm__ volatile("fnstcw %0" : "=m"(word));
}

// 0x080d4b2a  size=148  FUN_080d4b2a  status=translated  naming=inferred
// Evidence: banner strings "Linux Quake3 Dedicated Server [%s %s]\n", build date "Jun 23 2006" "12:25:18", " local install: %s\n"; called by Sys_ParseArgs for --version.
void Sys_PrintBinVersion(const char *name)
{
    fprintf(stdout, "\n\n%s\n", "==============================================================");
    fprintf(stdout, "Linux Quake3 Dedicated Server [%s %s]\n", "Jun 23 2006", "12:25:18");
    fprintf(stdout, " local install: %s\n", name);
    fprintf(stdout, "%s\n\n", "==============================================================");
}

// 0x080d4bbe  size=152  FUN_080d4bbe  status=translated  naming=inferred
// Evidence: strings "chmod('%s', %d) failed: errno %d\n", "stat('%s')  failed: errno %d\n"; adds mode bits to existing st_mode (Q3 Sys_Chmod-like).
void Sys_Chmod(const char *path, unsigned int mode)
{
    cod2_stat_t st;
    unsigned int newmode;

    if (FS_StatPath(path, &st) == 0) {
        newmode = mode | st.st_mode;
        if (chmod(path, newmode) != 0) {
            Com_Printf("chmod('%s', %d) failed: errno %d\n", path, newmode, *__errno_location());
        }
        /* Quirk preserved: two conversions, one argument. */
        Com_DPrintf("chmod +%d '%s'\n", path);
    } else {
        Com_Printf("stat('%s')  failed: errno %d\n", path, *__errno_location());
    }
}

// 0x080d4c56  size=104  FUN_080d4c56  status=translated  naming=inferred
// Evidence: fork(); child runs execl(cmd,cmd,NULL) if there is no space in cmd, else system(cmd), then _exit(0) (Q3 Sys_DoStartProcess).
void Sys_DoStartProcess(const char *cmd)
{
    pid_t pid;

    pid = fork();
    if (pid == -1 || pid != 0) {
        return;
    }
    if (strchr(cmd, 0x20) == (char *)0) {
        execl(cmd, cmd, (char *)0);
    } else {
        system(cmd);
    }
    _exit(0);
}

// 0x080d4cbe  size=112  FUN_080d4cbe  status=translated  naming=observed
// Evidence: strings "Sys_StartProcess %s\n" and "Sys_StartProcess %s (delaying to final exit)\n"; queues "quit\n".
void Sys_StartProcess(const char *cmd, int doexit)
{
    if (doexit == 0) {
        Com_DPrintf("Sys_StartProcess %s\n", cmd);
        Sys_DoStartProcess(cmd);
    } else {
        Com_DPrintf("Sys_StartProcess %s (delaying to final exit)\n", cmd);
        Q_strncpyz(sys_exit_cmdline, cmd, 0x400);
        Cbuf_ExecuteText(2, "quit\n");
    }
}

// 0x080d4d2e  size=493  FUN_080d4d2e  status=translated  naming=observed
// Evidence: strings "Sys_OpenURL %s\n", "openurl.sh", "%s not found\n", "URL script: %s\n"; searches cwd, fs_homepath, fs_basepath.
void Sys_OpenURL(const char *url, int doexit)
{
    char command[0x400];
    char path[0x100];
    char script[0x24];
    const char *dir;

    Com_Printf("Sys_OpenURL %s\n", url);
    Q_strncpyz(script, "openurl.sh", 0x14);
    dir = Sys_Cwd();
    Com_sprintf(path, 0x100, "%s/%s", dir, script);
    if (access(path, 1) == -1) {
        Com_DPrintf("%s not found\n", path);
        dir = Dvar_GetString("fs_homepath");
        Com_sprintf(path, 0x100, "%s/%s", dir, script);
        if (access(path, 1) == -1) {
            Com_DPrintf("%s not found\n", path);
            dir = Dvar_GetString("fs_basepath");
            Com_sprintf(path, 0x100, "%s/%s", dir, script);
            if (access(path, 1) == -1) {
                Com_DPrintf("%s not found\n", path);
                Com_Printf("Can't find script '%s' to open requested URL (use +set developer 1 for more verbosity)\n", script);
                return;
            }
        }
    }
    Com_DPrintf("URL script: %s\n", path);
    Com_sprintf(command, 0x400, "%s '%s' &", path, url);
    Sys_StartProcess(command, doexit);
}

// 0x080d4f1c  size=95  FUN_080d4f1c  status=translated  naming=inferred
// Evidence: strings "--version" and "-v"; prints the banner via Sys_PrintBinVersion and exits.
void Sys_ParseArgs(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "--version") != 0 && strcmp(argv[1], "-v") != 0) {
            return;
        }
        Sys_PrintBinVersion(argv[0]);
        Sys_Exit(0);
    }
}

/* ------------------------------------------------------------------ */
/* UDP networking                                                      */
/* ------------------------------------------------------------------ */

// 0x080d5160  size=125  FUN_080d5160  status=translated  naming=inferred
// Evidence: converts netadr type 3 (broadcast, INADDR_BROADCAST) / 4 (IP) into a 16-byte sockaddr_in (Q3 NetadrToSockadr).
void NetadrToSockadr(const netadr_t *a, struct sockaddr_in *s)
{
    memset(s, 0, 0x10);
    if (a->type == 3) {
        s->sin_family = 2;
        s->sin_port = a->port;
        s->sin_addr.s_addr = 0xffffffff;
    } else if (a->type == 4) {
        s->sin_family = 2;
        memcpy(&s->sin_addr.s_addr, a->ip, 4);
        s->sin_port = a->port;
    }
}

// 0x080d51de  size=44  FUN_080d51de  status=translated  naming=inferred
// Evidence: copies sin_addr/sin_port into a netadr and sets type 4 (Q3 SockadrToNetadr).
void SockadrToNetadr(const struct sockaddr_in *s, netadr_t *a)
{
    memcpy(a->ip, &s->sin_addr.s_addr, 4);
    a->port = s->sin_port;
    a->type = 4;
}

// 0x080d520a  size=73  FUN_080d520a  status=translated  naming=inferred
// Evidence: formats the 4 ip bytes (second dword) with "%i.%i.%i.%i" into a static 0x40 buffer; first dword is the (unused) address type.
const char *NET_BaseAdrToString(netadr_t adr)
{
    Com_sprintf(sys_ipstring, 0x40, "%i.%i.%i.%i", adr.ip[0], adr.ip[1], adr.ip[2], adr.ip[3]);
    return sys_ipstring;
}

// 0x080d5254  size=151  FUN_080d5254  status=translated  naming=inferred
// Evidence: digit-first strings go through inet_addr, others through gethostbyname (Q3 Sys_StringToSockaddr).
int Sys_StringToSockaddr(const char *s, struct sockaddr_in *sadr)
{
    struct hostent *h;

    memset(sadr, 0, 0x10);
    sadr->sin_family = 2;
    sadr->sin_port = 0;
    if (*s < '0' || '9' < *s) {
        h = gethostbyname(s);
        if (h == (struct hostent *)0) {
            return 0;
        }
        sadr->sin_addr.s_addr = *(unsigned int *)h->h_addr_list[0];
    } else {
        sadr->sin_addr.s_addr = inet_addr(s);
    }
    return 1;
}

// 0x080d52ec  size=67  FUN_080d52ec  status=translated  naming=inferred
// Evidence: resolves a string to a sockaddr and converts it to a netadr (Q3 Sys_StringToAdr).
int Sys_StringToAdr(const char *s, netadr_t *a)
{
    struct sockaddr_in sadr;
    int ok;

    ok = Sys_StringToSockaddr(s, &sadr);
    if (ok != 0) {
        SockadrToNetadr(&sadr, a);
    }
    return ok != 0;
}

// 0x080d5330  size=371  FUN_080d5330  status=translated  naming=observed
// Evidence: strings "NET_GetPacket: %s from %s\n" and "Oversize packet from %s\n"; recvfrom on ip then ipx sockets (Q3 NET_GetPacket).
int Sys_GetPacket(netadr_t *net_from, msg_t *net_message)
{
    int which;
    int sock;
    socklen_t fromlen;
    struct sockaddr_in from;
    int ret;
    const char *adrstr;
    const char *errstr;

    which = 0;
    for (;;) {
        if (1 < which) {
            return 0;
        }
        if (which == 0) {
            sock = ip_socket;
        } else {
            sock = ipx_socket;
        }
        if (sock != 0) {
            fromlen = 0x10;
            ret = recvfrom(sock, net_message->data, net_message->maxsize, 0, (struct sockaddr *)&from, &fromlen);
            SockadrToNetadr(&from, net_from);
            net_message->readcount = 0;
            if (ret == -1) {
                if (*__errno_location() != 0xb && *__errno_location() != 0x6f) {
                    adrstr = NET_AdrToString(*net_from);
                    errstr = NET_ErrorString();
                    Com_Printf("NET_GetPacket: %s from %s\n", errstr, adrstr);
                }
            } else {
                if (ret != net_message->maxsize) {
                    net_message->cursize = ret;
                    return 1;
                }
                adrstr = NET_AdrToString(*net_from);
                Com_Printf("Oversize packet from %s\n", adrstr);
            }
        }
        which = which + 1;
    }
}

// 0x080d54a4  size=287  FUN_080d54a4  status=translated  naming=observed
// Evidence: strings "NET_SendPacket: bad address type" and "NET_SendPacket ERROR: %s to %s\n"; sendto (Q3 NET_SendPacket).
int Sys_SendPacket(int length, const void *data, netadr_t to)
{
    int sock;
    struct sockaddr_in addr;
    int ret;
    const char *adrstr;
    const char *errstr;

    if (to.type == 3) {
        sock = ip_socket;
    } else if (to.type == 4) {
        sock = ip_socket;
    } else if (to.type == 5) {
        sock = ipx_socket;
    } else {
        if (to.type != 6) {
            Com_Error(0, "NET_SendPacket: bad address type");
            return 0;
        }
        sock = ipx_socket;
    }
    if (sock == 0) {
        return 0;
    }
    NetadrToSockadr(&to, &addr);
    ret = sendto(sock, data, length, 0, (struct sockaddr *)&addr, 0x10);
    if (ret == -1) {
        adrstr = NET_AdrToString(to);
        errstr = NET_ErrorString();
        Com_Printf("NET_SendPacket ERROR: %s to %s\n", errstr, adrstr);
        return 0;
    }
    return 1;
}

// 0x080d55c4  size=115  FUN_080d55c4  status=translated  naming=inferred
// Evidence: resolves host string to a type-4 netadr, overrides the port with htons(port) and sends (Q3 NET_SendPacket-to-string helper).
void Sys_SendPacketToHost(const char *host, uint16 port, int length, const void *data)
{
    netadr_t adr;
    uint16 nport;

    /* adr.type is set before the resolve; bytes 0x0a..0x13 stay uninitialised as in the binary. */
    adr.type = 4;
    if (Sys_StringToAdr(host, &adr) != 0) {
        nport = htons(port);
        adr.port = nport;
        Sys_SendPacket(length, data, adr);
    }
}

// 0x080d5638  size=266  FUN_080d5638  status=translated  naming=inferred
// Evidence: loopback/IPX true; IPv4 private ranges 10/8, 127/8, 169.254/16, 172.16/12, 192.168/16, then first 3 bytes against localIP[] (Q3 Sys_IsLANAddress).
int Sys_IsLANAddress(netadr_t adr)
{
    int i;

    if (adr.type == 2) {
        return 1;
    }
    if (adr.type == 5) {
        return 1;
    }
    if (adr.type == 4) {
        if (adr.ip[0] == 10) {
            return 1;
        }
        if (adr.ip[0] == 127) {
            return 1;
        }
        if (adr.ip[0] == 169 && adr.ip[1] == 254) {
            return 1;
        }
        if (adr.ip[0] == 172 && (adr.ip[1] & 0xf0) == 0x10) {
            return 1;
        }
        if (adr.ip[0] == 192 && adr.ip[1] == 168) {
            return 1;
        }
        for (i = 0; i < numIP; i++) {
            if (adr.ip[0] == localIP[i][0] && adr.ip[1] == localIP[i][1] && adr.ip[2] == localIP[i][2]) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// 0x080d5742  size=107  FUN_080d5742  status=translated  naming=inferred
// Evidence: string "IP: %i.%i.%i.%i\n" printed for each localIP entry (Q3 NET_ShowIP).
void NET_ShowIP(void)
{
    int i;

    for (i = 0; i < numIP; i++) {
        Com_Printf("IP: %i.%i.%i.%i\n", localIP[i][0], localIP[i][1], localIP[i][2], localIP[i][3]);
    }
}

// 0x080d57ae  size=464  FUN_080d57ae  status=translated  naming=inferred
// Evidence: strings "Hostname: %s\n", "Alias: %s\n", "IP: %i.%i.%i.%i\n"; gethostname/gethostbyname fills localIP (Q3 NET_GetLocalAddress).
void NET_GetLocalAddress(void)
{
    char hostname[0x100];
    struct hostent *h;
    char **alias;
    unsigned int *addr;
    unsigned int hostip;
    int ai;

    if (gethostname(hostname, 0x100) == -1) {
        return;
    }
    h = gethostbyname(hostname);
    if (h == (struct hostent *)0) {
        return;
    }
    Com_Printf("Hostname: %s\n", h->h_name);
    ai = 0;
    for (;;) {
        alias = h->h_aliases + ai;
        ai = ai + 1;
        if (*alias == (char *)0) {
            break;
        }
        Com_Printf("Alias: %s\n", *alias);
    }
    if (h->h_addrtype == 2) {
        numIP = 0;
        for (;;) {
            addr = (unsigned int *)h->h_addr_list[numIP];
            numIP = numIP + 1;
            if (addr == (unsigned int *)0 || 0xf < numIP) {
                break;
            }
            hostip = ntohl(*addr);
            /* Quirk preserved: numIP is incremented before the store, so slot 0 is never filled. */
            localIP[numIP][0] = (byte)*addr;
            localIP[numIP][1] = ((byte *)addr)[1];
            localIP[numIP][2] = ((byte *)addr)[2];
            localIP[numIP][3] = ((byte *)addr)[3];
            Com_Printf("IP: %i.%i.%i.%i\n", (int)hostip >> 0x18 & 0xff, (int)hostip >> 0x10 & 0xff, (int)hostip >> 8 & 0xff, hostip & 0xff);
        }
    }
}

// 0x080d597e  size=221  FUN_080d597e  status=translated  naming=observed
// Evidence: strings "net_ip", "localhost", "net_port", "Couldn't allocate IP port"; tries 10 consecutive ports (Q3 NET_OpenIP).
void NET_OpenIP(void)
{
    dvar_t *portVar;
    int basePort;
    const char *netIp;
    int i;

    Dvar_RegisterString("net_ip", "localhost", 0x1010);
    portVar = Dvar_RegisterInt("net_port", 0x7120, 0, 999999, 0x1010);
    basePort = Dvar_GetInt("net_port");
    netIp = Dvar_GetString("net_ip");
    i = 0;
    for (;;) {
        if (9 < i) {
            Com_Error(0, "Couldn't allocate IP port");
            return;
        }
        ip_socket = NET_IPSocket(netIp, i + basePort);
        if (ip_socket != 0) {
            break;
        }
        i = i + 1;
        ip_socket = 0;
    }
    Dvar_SetInt(portVar, i + basePort);
    NET_GetLocalAddress();
}

// 0x080d5a5c  size=76  FUN_080d5a5c  status=translated  naming=observed
// Evidence: string "net_noudp"; opens the IP socket when UDP is allowed and none is open (Q3 NET_Config/NET_Init shape).
void NET_Init(void)
{
    net_noudp_cvar = Dvar_RegisterBool("net_noudp", 0, 0x1010);
    if (Dvar_GetBool("net_noudp") == '\0' && ip_socket == 0) {
        NET_OpenIP();
    }
}

// 0x080d5aa8  size=470  FUN_080d5aa8  status=translated  naming=observed
// Evidence: strings "Opening IP socket: %s:%i\n", "ERROR: UDP_OpenSocket: socket/ioctl FIONBIO/setsockopt SO_BROADCAST/bind"; socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP) (Q3 NET_IPSocket).
int NET_IPSocket(const char *net_interface, int port)
{
    int newsocket;
    int on_fionbio;
    int on_broadcast;
    struct sockaddr_in addr;
    const char *err;

    on_fionbio = 1;
    on_broadcast = 1;
    if (net_interface == (const char *)0) {
        Com_Printf("Opening IP socket: localhost:%i\n", port);
    } else {
        Com_Printf("Opening IP socket: %s:%i\n", net_interface, port);
    }
    newsocket = socket(2, 2, 0x11);
    if (newsocket == -1) {
        err = NET_ErrorString();
        Com_Printf("ERROR: UDP_OpenSocket: socket: %s", err);
        return 0;
    }
    if (ioctl(newsocket, 0x5421, &on_fionbio) == -1) {
        err = NET_ErrorString();
        Com_Printf("ERROR: UDP_OpenSocket: ioctl FIONBIO:%s\n", err);
        return 0;
    }
    if (setsockopt(newsocket, 1, 6, &on_broadcast, 4) == -1) {
        err = NET_ErrorString();
        Com_Printf("ERROR: UDP_OpenSocket: setsockopt SO_BROADCAST:%s\n", err);
        return 0;
    }
    if (net_interface == (const char *)0 || *net_interface == '\0' || Q_stricmp(net_interface, "localhost") == 0) {
        addr.sin_addr.s_addr = 0;
    } else {
        Sys_StringToSockaddr(net_interface, &addr);
    }
    if (port == -1) {
        addr.sin_port = 0;
    } else {
        addr.sin_port = htons((uint16)port);
    }
    addr.sin_family = 2;
    if (bind(newsocket, (struct sockaddr *)&addr, 0x10) == -1) {
        err = NET_ErrorString();
        Com_Printf("ERROR: UDP_OpenSocket: bind: %s\n", err);
        close(newsocket);
        return 0;
    }
    return newsocket;
}

// 0x080d5c7e  size=40  FUN_080d5c7e  status=translated  naming=inferred
// Evidence: close() on the IP socket global and clear it (Q3 NET_CloseIP).
void NET_CloseIP(void)
{
    if (ip_socket != 0) {
        close(ip_socket);
        ip_socket = 0;
    }
}

// 0x080d5ca6  size=29  FUN_080d5ca6  status=translated  naming=inferred
// Evidence: returns strerror(errno) (eax is left holding the strerror result; Ghidra shows void) (Q3 NET_ErrorString).
const char *NET_ErrorString(void)
{
    return strerror(*__errno_location());
}

// 0x080d5cc4  size=210  FUN_080d5cc4  status=translated  naming=inferred
// Evidence: select() on stdin (when active) and the IP socket with a millisecond timeout (Q3 NET_Sleep).
void NET_Sleep(int msec)
{
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    if (stdin_active != 0) {
        FD_SET(0, &rfds);
    }
    FD_SET(ip_socket, &rfds);
    tv.tv_sec = msec / 1000;
    tv.tv_usec = (msec % 1000) * 1000;
    select(ip_socket + 1, &rfds, (fd_set *)0, (fd_set *)0, &tv);
}

/* ------------------------------------------------------------------ */
/* time, math, files, paths                                            */
/* ------------------------------------------------------------------ */

// 0x080d5d98  size=135  FUN_080d5d98  status=translated  naming=inferred
// Evidence: gettimeofday with a first-call time base in seconds (Q3 Sys_Milliseconds); first call returns usec/1000 without storing curtime.
int Sys_Milliseconds(void)
{
    struct timeval tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);
    if (sys_timeBase == 0) {
        sys_timeBase = tp.tv_sec;
        return tp.tv_usec / 1000;
    }
    sys_curtime = tp.tv_usec / 1000 + (tp.tv_sec - sys_timeBase) * 1000;
    return sys_curtime;
}

// 0x080d5e20  size=77  FUN_080d5e20  status=translated  naming=unknown
// Evidence: gettimeofday converted to absolute milliseconds into the shared curtime global, no time base.
int Sys_MillisecondsRaw(void)
{
    struct timeval tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);
    sys_curtime = tp.tv_usec / 1000 + tp.tv_sec * 1000;
    return sys_curtime;
}

// 0x080d5e6e  size=38  FUN_080d5e6e  status=translated  naming=inferred
// Evidence: x87 fistp with control word RC=truncate (or ax,0xc00), i.e. (int)f (Q3 Q_ftol).
int Q_ftol(float f)
{
    return (int)f;
}

// 0x080d5e94  size=79  FUN_080d5e94  status=translated  naming=inferred
// Evidence: rint() on each of three floats via double (Q3 Sys_SnapVector shape).
void Sys_SnapVector(float *v)
{
    double d;

    d = rint((double)v[0]);
    v[0] = (float)d;
    d = rint((double)v[1]);
    v[1] = (float)d;
    d = rint((double)v[2]);
    v[2] = (float)d;
}

// 0x080d5ee4  size=27  FUN_080d5ee4  status=translated  naming=inferred
// Evidence: mkdir(path, 0777) (Q3 Sys_Mkdir).
void Sys_Mkdir(const char *path)
{
    mkdir(path, 0x1ff);
}

// 0x080d5f00  size=663  FUN_080d5f00  status=translated  naming=inferred
// Evidence: recursive opendir/readdir walk with "%s/%s" joins, directory test (mode & 0x4000), skips "." and "..", filter via 0x080ac9b0, cap 0xfff entries (Q3 Sys_ListFilteredFiles).
void Sys_ListFilteredFiles(const char *basedir, const char *subdirs, const char *filter, char **list, int *numfiles)
{
    char search[0x100];
    char newsubdirs[0x100];
    char filename[0x100];
    cod2_stat_t st;
    DIR *dir;
    struct dirent *d_entry;

    if (*numfiles < 0xfff) {
        if (*subdirs == '\0') {
            Com_sprintf(search, 0x100, "%s", basedir);
        } else {
            Com_sprintf(search, 0x100, "%s/%s", basedir, subdirs);
        }
        dir = opendir(search);
        if (dir != (DIR *)0) {
            while ((d_entry = readdir(dir)) != (struct dirent *)0) {
                Com_sprintf(filename, 0x100, "%s/%s", search, d_entry->d_name);
                if (FS_StatPath(filename, &st) != -1) {
                    if ((st.st_mode & 0x4000) != 0 && Q_stricmp(d_entry->d_name, ".") != 0 && Q_stricmp(d_entry->d_name, "..") != 0) {
                        if (*subdirs == '\0') {
                            Com_sprintf(newsubdirs, 0x100, "%s", d_entry->d_name, d_entry->d_name);
                        } else {
                            Com_sprintf(newsubdirs, 0x100, "%s/%s", subdirs, d_entry->d_name);
                        }
                        Sys_ListFilteredFiles(basedir, newsubdirs, filter, list, numfiles);
                    }
                    if (0xffe < *numfiles) {
                        break;
                    }
                    Com_sprintf(filename, 0x100, "%s/%s", subdirs, d_entry->d_name);
                    if (Com_FilterPath(filter, filename, 0) != '\0') {
                        list[*numfiles] = CopyString(filename);
                        *numfiles = *numfiles + 1;
                    }
                }
            }
            closedir(dir);
        }
    }
}

// 0x080d6198  size=911  FUN_080d6198  status=translated  naming=inferred
// Evidence: opendir/readdir directory listing with extension suffix match (Q_stricmp on the name tail), "/" extension meaning directories only, NULL-terminated copied list (Q3 Sys_ListFiles).
char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int *numfiles, int wantsubs)
{
    char *list[4099];
    char search[0x104];
    cod2_stat_t st;
    DIR *dir;
    struct dirent *d_entry;
    int nfiles;
    int dironly;
    char **result;
    int i;

    dironly = wantsubs;
    if (filter == (const char *)0) {
        if (extension == (const char *)0) {
            extension = "";
        }
        if (*extension == '/' && extension[1] == '\0') {
            extension = "";
            dironly = 1;
        }
        (void)strlen(extension);
        nfiles = 0;
        dir = opendir(directory);
        if (dir == (DIR *)0) {
            *numfiles = 0;
            return (char **)0;
        }
        while ((d_entry = readdir(dir)) != (struct dirent *)0) {
            Com_sprintf(search, 0x100, "%s/%s", directory, d_entry->d_name);
            if (FS_StatPath(search, &st) == -1) {
                continue;
            }
            if (dironly != 0 && (st.st_mode & 0x4000) == 0) {
                continue;
            }
            if (dironly == 0 && (st.st_mode & 0x4000) != 0) {
                continue;
            }
            if (*extension != '\0') {
                if (strlen(d_entry->d_name) < strlen(extension)) {
                    continue;
                }
                if (Q_stricmp(d_entry->d_name + (strlen(d_entry->d_name) - strlen(extension)), extension) != 0) {
                    continue;
                }
            }
            if (nfiles == 0xfff) {
                break;
            }
            list[nfiles] = CopyString(d_entry->d_name);
            nfiles = nfiles + 1;
        }
        list[nfiles] = (char *)0;
        closedir(dir);
    } else {
        nfiles = 0;
        Sys_ListFilteredFiles(directory, "", filter, list, &nfiles);
        list[nfiles] = (char *)0;
    }
    *numfiles = nfiles;
    if (nfiles == 0) {
        return (char **)0;
    }
    result = (char **)Com_MallocZeroed(nfiles * 4 + 4);
    for (i = 0; i < nfiles; i++) {
        result[i] = list[i];
    }
    result[i] = (char *)0;
    return result;
}

// 0x080d6528  size=86  FUN_080d6528  status=translated  naming=inferred
// Evidence: frees each entry of a NULL-terminated list then the list (Q3 Sys_FreeFileList); free is 0x080ab5b8 = Com_Free_A.
void Sys_FreeFileList(char **list)
{
    int i;

    if (list != (char **)0) {
        for (i = 0; list[i] != (char *)0; i++) {
            Com_Free_A(list[i]);
        }
        Com_Free_A(list);
    }
}

// 0x080d657e  size=40  FUN_080d657e  status=translated  naming=inferred
// Evidence: getcwd into a static 0x100 buffer, forces NUL at index 0xff (Q3 Sys_Cwd).
char *Sys_Cwd(void)
{
    getcwd(sys_cwd, 0xff);
    sys_cwd[0xff] = 0;
    return sys_cwd;
}

// 0x080d65a6  size=35  FUN_080d65a6  status=translated  naming=inferred
// Evidence: Q_strncpyz into the static cd-path buffer (Q3 Sys_SetDefaultCDPath).
void Sys_SetDefaultCDPath(const char *path)
{
    Q_strncpyz(sys_cdpath, path, 0x100);
}

// 0x080d65ca  size=10  FUN_080d65ca  status=translated  naming=inferred
// Evidence: returns the static cd-path buffer (Q3 Sys_DefaultCDPath).
char *Sys_DefaultCDPath(void)
{
    return sys_cdpath;
}

// 0x080d65d4  size=37  FUN_080d65d4  status=translated  naming=inferred
// Evidence: returns the install-path buffer when set else Sys_Cwd (Q3 Sys_DefaultInstallPath); byte-identical twin of 0x080d661e.
char *Sys_DefaultInstallPath(void)
{
    if (sys_installpath[0] == '\0') {
        return Sys_Cwd();
    }
    return sys_installpath;
}

// 0x080d661e  size=37  FUN_080d661e  status=translated  naming=unknown
// Evidence: identical body to Sys_DefaultInstallPath (install-path buffer else Sys_Cwd); distinguishing role unknown.
char *Sys_DefaultBasePath(void)
{
    if (sys_installpath[0] == '\0') {
        return Sys_Cwd();
    }
    return sys_installpath;
}

// 0x080d6668  size=221  FUN_080d6668  status=translated  naming=observed
// Evidence: strings "HOME", "/.callofduty2", "Unable to create directory \"%s\", error is %s(%d)\n"; mkdir 0777 ignoring EEXIST (Q3 Sys_DefaultHomePath).
char *Sys_DefaultHomePath(void)
{
    const char *home;
    int *errPtr;
    const char *errText;
    int errCode;

    if (sys_homepath[0] == '\0') {
        home = getenv("HOME");
        if (home == (const char *)0) {
            return (char *)"";
        }
        Q_strncpyz(sys_homepath, home, 0x100);
        I_strncat(sys_homepath, 0x100, "/.callofduty2");
        if (mkdir(sys_homepath, 0x1ff) != 0) {
            if (*__errno_location() != 0x11) {
                errCode = *__errno_location();
                errText = strerror(*__errno_location());
                Sys_Error("Unable to create directory \"%s\", error is %s(%d)\n", sys_homepath, errText, errCode);
            }
        }
        return sys_homepath;
    }
    return sys_homepath;
}

// 0x080d6746  size=10  FUN_080d6746  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6746(void)
{
    return 0;
}

// 0x080d6750  size=10  FUN_080d6750  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6750(void)
{
    return 0;
}

// 0x080d675a  size=5  FUN_080d675a  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d675a(void)
{
}

// 0x080d6760  size=50  FUN_080d6760  status=translated  naming=inferred
// Evidence: getpwuid(getuid())->pw_name else the literal "player"; feeds the "username" cvar in Sys_InitCvars.
const char *Sys_GetCurrentUser(void)
{
    struct passwd *p;

    p = getpwuid(getuid());
    if (p == (struct passwd *)0) {
        return "player";
    }
    return p->pw_name;
}

// 0x080d6792  size=71  FUN_080d6792  status=translated  naming=unknown
// Evidence: lowercases a string in place with tolower(); quirk: returns the pointer advanced to the terminating NUL (not the start); NULL stays NULL.
char *Q_strlwr_EndPtr(char *s)
{
    if (s == (char *)0) {
        return s;
    }
    for (; *s != '\0'; s++) {
        *s = (char)tolower((int)*s);
    }
    return s;
}

// 0x080d67da  size=103  FUN_080d67da  status=translated  naming=inferred
// Evidence: six pthread mutexes of type 1 (recursive) initialised in a static array (stride 0x18).
void Sys_InitMutexes(void)
{
    pthread_mutexattr_t attr;
    int i;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, 1);
    for (i = 0; i < 6; i++) {
        pthread_mutex_init((pthread_mutex_t *)&sys_mutexes[i], &attr);
    }
    pthread_mutexattr_destroy(&attr);
}

// 0x080d6842  size=33  FUN_080d6842  status=translated  naming=inferred
// Evidence: pthread_mutex_lock on sys_mutexes[index]; Com_Error locks index 2.
void Sys_EnterCriticalSection(int index)
{
    pthread_mutex_lock((pthread_mutex_t *)&sys_mutexes[index]);
}

// 0x080d6864  size=33  FUN_080d6864  status=translated  naming=inferred
// Evidence: pthread_mutex_unlock on sys_mutexes[index].
void Sys_LeaveCriticalSection(int index)
{
    pthread_mutex_unlock((pthread_mutex_t *)&sys_mutexes[index]);
}

// 0x080d6886  size=239  FUN_080d6886  status=translated  naming=inferred
// Evidence: copies the path converting '\\' to '/', opendir and returns 1 if any entry other than "." / ".." exists.
int Sys_DirectoryHasContent(const char *path)
{
    char *copy;
    DIR *dir;
    struct dirent *de;
    int i;

    copy = new char[strlen(path) + 1];
    strcpy(copy, path);
    for (i = 0; copy[i] != '\0'; i++) {
        if (copy[i] == '\\') {
            copy[i] = '/';
        }
    }
    i = 0;
    dir = opendir(copy);
    if (dir != (DIR *)0) {
        do {
            de = readdir(dir);
            if (de == (struct dirent *)0) {
                goto done;
            }
        } while (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0);
        i = 1;
done:
        closedir(dir);
    }
    if (copy != (char *)0) {
        delete[] copy;
    }
    return i;
}

// 0x080d6976  size=490  FUN_080d6976  status=translated  naming=inferred
// Evidence: recursive directory removal (opendir/readdir/stat/unlink/rmdir, "%s/%s", '\\' to '/'); quirk: always returns 1 regardless of success.
int Sys_RemoveDirTree(const char *path)
{
    char *copy;
    char *sub;
    size_t subLen;
    DIR *dir;
    struct dirent *de;
    cod2_stat_t st;
    unsigned int status;
    unsigned int i;

    copy = new char[strlen(path) + 1];
    strcpy(copy, path);
    for (i = 0; copy[i] != '\0'; i++) {
        if (copy[i] == '\\') {
            copy[i] = '/';
        }
    }
    status = 1;
    dir = opendir(copy);
    if (dir != (DIR *)0) {
        do {
            do {
                de = readdir(dir);
                if (de == (struct dirent *)0) {
                    goto done;
                }
            } while (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0);
            subLen = strlen(de->d_name) + strlen(copy) + 2;
            sub = new char[subLen];
            snprintf(sub, subLen, "%s/%s", copy, de->d_name);
            if (FS_StatPath(sub, &st) == -1) {
                status = 0;
            } else if ((st.st_mode & 0xf000) == 0x4000) {
                status = Sys_RemoveDirTree(sub);
            } else {
                status = (unlink(sub) != -1);
            }
            if (sub != (char *)0) {
                delete[] sub;
            }
        } while (status != 0);
done:
        closedir(dir);
    }
    if (status == 1) {
        status = (rmdir(copy) != -1);
    }
    if (copy != (char *)0) {
        delete[] copy;
    }
    return 1;
}

// 0x080d6b60  size=30  FUN_080d6b60  status=translated  naming=inferred
// Evidence: records pthread_self() as the main thread and calls Com_SetThreadBlocks(0).
void Sys_SetMainThread(void)
{
    *(pthread_t *)sys_mainthread = pthread_self();
    Com_SetThreadBlocks(0);
}

// 0x080d6b7e  size=10  FUN_080d6b7e  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6b7e(void)
{
    return 0;
}

// 0x080d6b88  size=37  FUN_080d6b88  status=translated  naming=inferred
// Evidence: compares pthread_self() with the recorded main thread.
int Sys_IsMainThread(void)
{
    return pthread_self() == *(pthread_t *)sys_mainthread;
}

// 0x080d6bae  size=18  FUN_080d6bae  status=translated  naming=inferred
// Evidence: stores into a 5-slot array; slot 2 is read back as the longjmp target by Com_Error (0x08061124).
void Sys_SetValue(int index, void *value)
{
    sys_values[index] = value;
}

// 0x080d6bc0  size=15  FUN_080d6bc0  status=translated  naming=inferred
// Evidence: reads the 5-slot array written by Sys_SetValue.
void *Sys_GetValue(int index)
{
    return sys_values[index];
}

// 0x080d6bd0  size=5  FUN_080d6bd0  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6bd0(void)
{
}

// 0x080d6bd6  size=5  FUN_080d6bd6  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6bd6(void)
{
}

// 0x080d6bdc  size=5  FUN_080d6bdc  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6bdc(void)
{
}

// 0x080d6be2  size=10  FUN_080d6be2  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6be2(void)
{
    return 0;
}

// 0x080d6bec  size=10  FUN_080d6bec  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6bec(void)
{
    return 0;
}

// 0x080d6bf6  size=10  FUN_080d6bf6  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6bf6(void)
{
    return 0;
}

// 0x080d6c00  size=10  FUN_080d6c00  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6c00(void)
{
    return 0;
}

// 0x080d6c0a  size=10  FUN_080d6c0a  status=translated  naming=unknown
// Evidence: returns constant 0.
int Sys_ReturnZero_080d6c0a(void)
{
    return 0;
}

// 0x080d6c14  size=5  FUN_080d6c14  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6c14(void)
{
}

// 0x080d6c1a  size=5  FUN_080d6c1a  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6c1a(void)
{
}

// 0x080d6c20  size=5  FUN_080d6c20  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6c20(void)
{
}

// 0x080d6c26  size=5  FUN_080d6c26  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6c26(void)
{
}

// 0x080d6c2c  size=5  FUN_080d6c2c  status=translated  naming=unknown
// Evidence: empty body.
void Sys_NoOp_080d6c2c(void)
{
}
