/*
 * Function-level differential runner for the Call of Duty 2 Linux dedicated server 1.3
 * (cod2_lnxded, ELF32 i386 EXEC, SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7).
 *
 * One process holds both sides:
 *   - the ORIGINAL image, mapped at its fixed PT_LOAD vaddrs (0x08048000..) with
 *     MAP_FIXED_NOREPLACE, .bss zeroed, .rel.plt/.rel.dyn bound to this process's
 *     libc/libm/libdl/libpthread and an i386 libstdc++.so.5 (dlopen) through per-import
 *     recording thunks; unresolved imports trap with their name;
 *   - one RECONSTRUCTED chunk, linked into this executable with its out-of-chunk
 *     callees and globals aliased (linker script) to the original image addresses,
 *     so both sides share one memory layout.
 * The runner is linked non-PIE at 0x60000000 (see differential-harness.md) so it never
 * overlaps the original's 0x08048000..0x08a6f4d8 range.
 *
 * Per case: fork P (case setup), P forks A (original) and B (reconstruction) from the
 * identical pre-call state, compares outcome, return value, every byte of the original
 * .data/.bss segment and of the argument arena, stdout/stderr bytes, and the ordered
 * import-call trace. One JSON line per case is appended to the result file.
 *
 * Usage: hx_runner <cod2_lnxded> <casefile> <resultfile> [libstdc++.so.5 path]
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define PAGE 4096u
#define RW_LO 0x0817d000u /* page-aligned start of the original RW PT_LOAD */
#define RW_HI 0x08a70000u /* page-aligned end (memsz end 0x08a6f4d8) */
#define ARENA_BASE 0x30000000u
#define ARENA_SIZE (8u << 20)
#define MAX_LOG 4096
#define MAX_COPY 768
#define OUT_CAP 65536
#define RETSTR_CAP 4096
#define MAX_WORDS 64
#define MAX_OBJS 64
#define MAX_IMPORTS 512

struct hx_ret {
    uint32_t eax, edx;
    unsigned char st0[16];
};
void hx_call_cdecl(void *fn, const uint32_t *words, int nwords, struct hx_ret *out);
void hx_call_cdecl_fp(void *fn, const uint32_t *words, int nwords, struct hx_ret *out);

/* Generated per chunk (recon_table.c). */
struct hx_recon_entry {
    uint32_t orig;
    void *recon;
    const char *name;
};
extern const struct hx_recon_entry hx_recon_table[];
extern const int hx_recon_count;

/* ------------------------------------------------------------------ shared state */
struct side_result {
    int done;
    int denied;
    int retstr_ok, retstr_len;
    struct hx_ret ret;
    int nlog;
    int log[MAX_LOG];
    int nchanged, overflow;
    uint32_t changed[MAX_COPY];
    char retstr[RETSTR_CAP];
};
struct shm_layout {
    int last_kind; /* 0 compared, 1 discarded, 2 discarded because the original timed out */
    struct side_result side[2];
    uint64_t pre[(RW_HI - RW_LO) / PAGE + ARENA_SIZE / PAGE];
    uint64_t post[2][(RW_HI - RW_LO) / PAGE + ARENA_SIZE / PAGE];
    unsigned char copy[2][MAX_COPY][PAGE];
};
static struct shm_layout *g_shm;
static volatile int g_recording;
static volatile int g_side;

static int g_nimports;
static char *g_import_name[MAX_IMPORTS];
static int g_unresolved[MAX_IMPORTS];
static int g_nunresolved;

/* ------------------------------------------------------------------ utilities */
static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "hx_runner: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(2);
}

__attribute__((force_align_arg_pointer)) static void hx_log_import(int index)
{
    if (g_recording) {
        struct side_result *s = &g_shm->side[g_side];
        if (s->nlog < MAX_LOG)
            s->log[s->nlog] = index;
        s->nlog++;
    }
}

__attribute__((force_align_arg_pointer, noreturn)) static void hx_unresolved_import(int index)
{
    char msg[256];
    int n = snprintf(msg, sizeof msg, "hx: unresolved import called: %s\n",
                     index >= 0 && index < g_nimports ? g_import_name[index] : "?");
    if (write(2, msg, n) < 0) {
    }
    _exit(97);
}

/* Called through the generated trap stubs for reconstruction symbols that have no
 * address (see difftest.py); exits with a distinctive status. */
__attribute__((force_align_arg_pointer, noreturn)) void hx_trap_recon(const char *name)
{
    char msg[256];
    int n = snprintf(msg, sizeof msg, "hx: unresolved reconstruction symbol called: %s\n", name);
    if (write(2, msg, n) < 0) {
    }
    _exit(98);
}

/* Sandbox: imports with host side effects (processes, files, terminal, network,
 * privileges, library loading, sleeping) are denied; a side that calls one stops with
 * exit status 96 and the case is discarded when the original does so. fopen is
 * allowed for reading only (cwd is a scratch directory). */
static const char *const k_denied[] = {
    "system", "execl", "fork", "unlink", "remove", "rmdir", "rename", "mkdir", "chmod", "seteuid",
    "socket", "bind", "sendto", "recvfrom", "setsockopt", "gethostbyname", "gethostname", "select",
    "ioctl", "tcsetattr", "tcgetattr", "fcntl", "dlopen", "dlsym", "dlclose", "sleep", "usleep",
    "signal", "__libc_start_main", NULL,
};
static int g_denied_index = -1;

__attribute__((force_align_arg_pointer, noreturn)) static void hx_denied_import(int index)
{
    if (g_shm)
        g_shm->side[g_side].denied = index;
    _exit(96);
}

static void *g_real_fopen;
static int g_fopen_index;
__attribute__((force_align_arg_pointer)) static FILE *hx_fopen_guard(const char *path, const char *mode)
{
    if (!mode || strpbrk(mode, "wa+"))
        hx_denied_import(g_fopen_index);
    return ((FILE * (*)(const char *, const char *)) g_real_fopen)(path, mode);
}

static int is_denied(const char *name)
{
    for (int i = 0; k_denied[i]; i++)
        if (!strcmp(k_denied[i], name))
            return 1;
    return 0;
}

/* ------------------------------------------------------------------ ELF loader */
static unsigned char *g_file;
static size_t g_file_size;
static void *g_libstdcxx5;
static unsigned char *g_thunks;

static uint32_t rd32(uint32_t addr) { return *(uint32_t *)(uintptr_t)addr; }

static const char *version_name(const Elf32_Dyn *dyn, uint32_t verneed, int nverneed, uint16_t want,
                                uint32_t strtab)
{
    uint32_t p = verneed;
    (void)dyn;
    for (int i = 0; i < nverneed && p; i++) {
        const Elf32_Verneed *vn = (const Elf32_Verneed *)(uintptr_t)p;
        uint32_t a = p + vn->vn_aux;
        for (int j = 0; j < vn->vn_cnt; j++) {
            const Elf32_Vernaux *va = (const Elf32_Vernaux *)(uintptr_t)a;
            if ((va->vna_other & 0x7fff) == want)
                return (const char *)(uintptr_t)(strtab + va->vna_name);
            a += va->vna_next;
        }
        p += vn->vn_next;
    }
    return NULL;
}

static void *resolve_import(const char *name, const char *ver)
{
    void *p = NULL;
    int cxx = ver && (!strncmp(ver, "GLIBCPP", 7) || !strncmp(ver, "CXXABI", 6));
    if (cxx && g_libstdcxx5) {
        p = dlvsym(g_libstdcxx5, name, ver);
        if (!p)
            p = dlsym(g_libstdcxx5, name);
        if (p)
            return p;
    }
    if (ver && !cxx)
        p = dlvsym(RTLD_DEFAULT, name, ver);
    if (!p)
        p = dlsym(RTLD_DEFAULT, name);
    if (!p && g_libstdcxx5)
        p = dlsym(g_libstdcxx5, name);
    return p;
}

static unsigned char *emit_thunk(unsigned char *t, int index, void *real)
{
    if (real) {
        *t++ = 0x50; /* push eax */
        *t++ = 0x51; /* push ecx */
        *t++ = 0x52; /* push edx */
        *t++ = 0x68; /* push imm32 index */
        memcpy(t, &index, 4);
        t += 4;
        *t++ = 0xb8; /* mov eax, hx_log_import */
        uint32_t f = (uint32_t)(uintptr_t)hx_log_import;
        memcpy(t, &f, 4);
        t += 4;
        *t++ = 0xff; /* call eax */
        *t++ = 0xd0;
        *t++ = 0x83; /* add esp, 4 */
        *t++ = 0xc4;
        *t++ = 0x04;
        *t++ = 0x5a; /* pop edx */
        *t++ = 0x59; /* pop ecx */
        *t++ = 0x58; /* pop eax */
        *t++ = 0x68; /* push real ; ret */
        uint32_t r = (uint32_t)(uintptr_t)real;
        memcpy(t, &r, 4);
        t += 4;
        *t++ = 0xc3;
    } else {
        *t++ = 0x68;
        memcpy(t, &index, 4);
        t += 4;
        *t++ = 0xb8;
        uint32_t f = (uint32_t)(uintptr_t)hx_unresolved_import;
        memcpy(t, &f, 4);
        t += 4;
        *t++ = 0xff;
        *t++ = 0xd0;
        *t++ = 0xf4; /* hlt (unreachable) */
    }
    while ((uintptr_t)t & 31)
        *t++ = 0xcc;
    return t;
}

static unsigned char *emit_denied(unsigned char *t, int index)
{
    *t++ = 0x68;
    memcpy(t, &index, 4);
    t += 4;
    *t++ = 0xb8;
    uint32_t f = (uint32_t)(uintptr_t)hx_denied_import;
    memcpy(t, &f, 4);
    t += 4;
    *t++ = 0xff;
    *t++ = 0xd0;
    *t++ = 0xf4;
    while ((uintptr_t)t & 31)
        *t++ = 0xcc;
    return t;
}

static void load_original(const char *path, const char *stdcxx5)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        die("open %s: %s", path, strerror(errno));
    struct stat st;
    fstat(fd, &st);
    g_file_size = st.st_size;
    g_file = malloc(g_file_size);
    if (read(fd, g_file, g_file_size) != (ssize_t)g_file_size)
        die("short read");
    close(fd);
    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)g_file;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) || eh->e_ident[EI_CLASS] != ELFCLASS32 || eh->e_machine != EM_386 ||
        eh->e_type != ET_EXEC)
        die("%s is not an ELF32 i386 EXEC", path);
    const Elf32_Phdr *ph = (const Elf32_Phdr *)(g_file + eh->e_phoff);
    uint32_t dynaddr = 0;
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC)
            dynaddr = ph[i].p_vaddr;
        if (ph[i].p_type != PT_LOAD)
            continue;
        uint32_t lo = ph[i].p_vaddr & ~(PAGE - 1);
        uint32_t hi = (ph[i].p_vaddr + ph[i].p_memsz + PAGE - 1) & ~(PAGE - 1);
        void *m = mmap((void *)(uintptr_t)lo, hi - lo, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (m == MAP_FAILED || (uint32_t)(uintptr_t)m != lo)
            die("cannot map PT_LOAD at 0x%08x-0x%08x (%s)", lo, hi, strerror(errno));
        memcpy((void *)(uintptr_t)ph[i].p_vaddr, g_file + ph[i].p_offset, ph[i].p_filesz);
        /* remaining memsz (.bss) stays zero from the anonymous mapping */
    }
    if (!dynaddr)
        die("no PT_DYNAMIC");

    if (stdcxx5 && *stdcxx5) {
        g_libstdcxx5 = dlopen(stdcxx5, RTLD_NOW | RTLD_LOCAL);
        if (!g_libstdcxx5)
            fprintf(stderr, "hx_runner: warning: %s\n", dlerror());
    }

    uint32_t symtab = 0, strtab = 0, jmprel = 0, pltrelsz = 0, rel = 0, relsz = 0, versym = 0, verneed = 0;
    int nverneed = 0;
    for (const Elf32_Dyn *d = (const Elf32_Dyn *)(uintptr_t)dynaddr; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
        case DT_SYMTAB: symtab = d->d_un.d_ptr; break;
        case DT_STRTAB: strtab = d->d_un.d_ptr; break;
        case DT_JMPREL: jmprel = d->d_un.d_ptr; break;
        case DT_PLTRELSZ: pltrelsz = d->d_un.d_val; break;
        case DT_REL: rel = d->d_un.d_ptr; break;
        case DT_RELSZ: relsz = d->d_un.d_val; break;
        case DT_VERSYM: versym = d->d_un.d_ptr; break;
        case DT_VERNEED: verneed = d->d_un.d_ptr; break;
        case DT_VERNEEDNUM: nverneed = d->d_un.d_val; break;
        }
    }
    int nplt = pltrelsz / sizeof(Elf32_Rel);
    if (nplt > MAX_IMPORTS)
        die("too many imports");
    g_thunks = mmap(NULL, (nplt + 1) * 64, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    unsigned char *t = g_thunks;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t base = pass == 0 ? rel : jmprel;
        uint32_t size = pass == 0 ? relsz : pltrelsz;
        for (uint32_t off = 0; off < size; off += sizeof(Elf32_Rel)) {
            const Elf32_Rel *r = (const Elf32_Rel *)(uintptr_t)(base + off);
            int type = ELF32_R_TYPE(r->r_info);
            int si = ELF32_R_SYM(r->r_info);
            const Elf32_Sym *sym = (const Elf32_Sym *)(uintptr_t)(symtab + si * sizeof(Elf32_Sym));
            const char *name = (const char *)(uintptr_t)(strtab + sym->st_name);
            uint16_t vs = versym ? ((const uint16_t *)(uintptr_t)versym)[si] & 0x7fff : 0;
            const char *ver = vs >= 2 ? version_name(NULL, verneed, nverneed, vs, strtab) : NULL;
            void *real = si ? resolve_import(name, ver) : NULL;
            uint32_t *where = (uint32_t *)(uintptr_t)r->r_offset;
            switch (type) {
            case R_386_JMP_SLOT: {
                int idx = g_nimports++;
                g_import_name[idx] = strdup(name);
                if (!real)
                    g_unresolved[g_nunresolved++] = idx;
                *where = (uint32_t)(uintptr_t)t;
                if (real && is_denied(name)) {
                    t = emit_denied(t, idx);
                } else {
                    if (real && !strcmp(name, "fopen")) {
                        g_real_fopen = real;
                        g_fopen_index = idx;
                        real = (void *)hx_fopen_guard;
                    }
                    t = emit_thunk(t, idx, real);
                }
                break;
            }
            case R_386_GLOB_DAT:
                *where = (uint32_t)(uintptr_t)real; /* weak __gmon_start__ -> 0 */
                break;
            case R_386_32:
                *where += (uint32_t)(uintptr_t)real;
                break;
            case R_386_COPY:
                if (!real)
                    die("COPY relocation for unresolved %s", name);
                memcpy(where, real, sym->st_size);
                break;
            case R_386_NONE:
            case R_386_RELATIVE: /* EXEC at its link address: nothing to add */
                break;
            default:
                die("unsupported relocation type %d for %s", type, name);
            }
        }
    }
    mprotect(g_thunks, (nplt + 1) * 64, PROT_READ | PROT_EXEC);
    /* final protections: text segment R+X, data stays R+W */
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || (ph[i].p_flags & PF_W))
            continue;
        uint32_t lo = ph[i].p_vaddr & ~(PAGE - 1);
        uint32_t hi = (ph[i].p_vaddr + ph[i].p_memsz + PAGE - 1) & ~(PAGE - 1);
        mprotect((void *)(uintptr_t)lo, hi - lo, PROT_READ | PROT_EXEC);
    }
}

/* ------------------------------------------------------------------ case file */
enum ret_kind { RK_VOID, RK_INT, RK_U64, RK_PTR, RK_STR, RK_F32, RK_F64 };
struct obj {
    uint32_t size;
    uint32_t len;
    unsigned char *data;
    uint32_t addr;
};
struct call {
    uint32_t addr;
    int nwords;
    uint32_t words[MAX_WORDS];
};

static uint32_t g_target;
static void *g_recon_fn;
static const char *g_recon_name;
static enum ret_kind g_rk = RK_VOID;
static int g_ret_bits = 32, g_ret_signed;
static int g_timeout = 2;

static int hexval(int c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; }
static unsigned char *unhex(const char *s, uint32_t *len)
{
    if (!strcmp(s, "-")) {
        *len = 0;
        return NULL;
    }
    size_t n = strlen(s) / 2;
    unsigned char *b = malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        b[i] = hexval(s[2 * i]) << 4 | hexval(s[2 * i + 1]);
    *len = n;
    return b;
}

static uint32_t recon_for(uint32_t orig, const char **name)
{
    for (int i = 0; i < hx_recon_count; i++)
        if (hx_recon_table[i].orig == orig) {
            if (name)
                *name = hx_recon_table[i].name;
            return (uint32_t)(uintptr_t)hx_recon_table[i].recon;
        }
    return 0;
}

/* Function pointer equivalence: an aligned word holding original address X on side A
 * and the reconstructed entry of X on side B is treated as equal. */
static int fn_equiv(uint32_t a, uint32_t b)
{
    if (a == b)
        return 1;
    for (int i = 0; i < hx_recon_count; i++)
        if (hx_recon_table[i].orig == a && (uint32_t)(uintptr_t)hx_recon_table[i].recon == b)
            return 1;
    return 0;
}

static int is_shared(uint32_t a)
{
    return (a >= RW_LO && a < RW_HI) || (a >= ARENA_BASE && a < ARENA_BASE + ARENA_SIZE);
}

#define ORIG_RODATA_LO 0x0813e340u
#define ORIG_RODATA_HI 0x08160e1cu
extern char __executable_start[], etext[];
static int read_retstr(uint32_t ptr, char *dst, int cap, int *len);

/* Constant-literal equivalence: the original points into its .rodata, the reconstruction
 * into the runner's own read-only image, and both strings are equal (string literals
 * cannot share addresses between the two builds). */
static int literal_equiv(uint32_t a, uint32_t b)
{
    static char sa[RETSTR_CAP], sb[RETSTR_CAP];
    int la, lb;
    if (a < ORIG_RODATA_LO || a >= ORIG_RODATA_HI)
        return 0;
    if (b < (uint32_t)(uintptr_t)__executable_start || is_shared(b))
        return 0;
    if (!read_retstr(a, sa, sizeof sa, &la) || !read_retstr(b, sb, sizeof sb, &lb))
        return 0;
    return la == lb && la < (int)sizeof sa && !memcmp(sa, sb, la);
}

/* ------------------------------------------------------------------ hashing */
static const uint32_t n_rw_pages = (RW_HI - RW_LO) / PAGE;
static uint32_t g_arena_pages; /* pages of arena included in comparisons */

static inline uint32_t page_addr(uint32_t idx)
{
    return idx < n_rw_pages ? RW_LO + idx * PAGE : ARENA_BASE + (idx - n_rw_pages) * PAGE;
}
static inline uint64_t hash_page(const uint32_t *p)
{
    uint32_t a = 0x811c9dc5u, b = 0x9e3779b9u;
    for (int i = 0; i < (int)(PAGE / 4); i += 2) {
        a = (a ^ p[i]) * 0x01000193u;
        b = (b ^ p[i + 1]) * 0x85ebca6bu;
        b = (b << 13) | (b >> 19);
    }
    return (uint64_t)a << 32 | b;
}
static void hash_all(uint64_t *out)
{
    uint32_t n = n_rw_pages + g_arena_pages;
    for (uint32_t i = 0; i < n; i++)
        out[i] = hash_page((const uint32_t *)(uintptr_t)page_addr(i));
}

/* ------------------------------------------------------------------ execution */
static int read_retstr(uint32_t ptr, char *dst, int cap, int *len)
{
    struct iovec local = {dst, cap}, remote = {(void *)(uintptr_t)ptr, cap};
    /* read page-wise to tolerate strings ending near an unmapped page */
    ssize_t got = 0;
    while (got < cap) {
        uint32_t chunk = PAGE - ((ptr + got) & (PAGE - 1));
        if (chunk > (uint32_t)(cap - got))
            chunk = cap - got;
        local.iov_base = dst + got;
        local.iov_len = chunk;
        remote.iov_base = (void *)(uintptr_t)(ptr + got);
        remote.iov_len = chunk;
        ssize_t r = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
        if (r <= 0)
            break;
        got += r;
        if (memchr(dst + got - r, 0, r))
            break;
    }
    if (got == 0)
        return 0;
    void *z = memchr(dst, 0, got);
    *len = z ? (int)((char *)z - dst) : (int)got;
    return 1;
}

static void run_side(int side, const struct call *c, int outfd)
{
    struct side_result *s = &g_shm->side[side];
    memset(s, 0, offsetof(struct side_result, retstr));
    s->denied = -1;
    g_side = side;
    alarm(g_timeout);
    dup2(outfd, 1);
    dup2(outfd, 2);
    /* the functions under test must not reach the runner's result/case files */
    if (syscall(SYS_close_range, 3u, ~0u, 0u) != 0)
        for (int fd = 3; fd < 1024; fd++)
            close(fd);
    void *fn = side == 0 ? (void *)(uintptr_t)c->addr : g_recon_fn;
    struct hx_ret r;
    memset(&r, 0, sizeof r);
    g_recording = 1;
    if (g_rk == RK_F32 || g_rk == RK_F64)
        hx_call_cdecl_fp(fn, c->words, c->nwords, &r);
    else
        hx_call_cdecl(fn, c->words, c->nwords, &r);
    g_recording = 0;
    fflush(NULL);
    s->ret = r;
    if (g_rk == RK_STR && r.eax)
        s->retstr_ok = read_retstr(r.eax, s->retstr, RETSTR_CAP, &s->retstr_len);
    uint32_t n = n_rw_pages + g_arena_pages;
    uint64_t *post = g_shm->post[side];
    hash_all(post);
    for (uint32_t i = 0; i < n; i++) {
        if (post[i] == g_shm->pre[i])
            continue;
        if (s->nchanged < MAX_COPY) {
            s->changed[s->nchanged] = i;
            memcpy(g_shm->copy[side][s->nchanged], (const void *)(uintptr_t)page_addr(i), PAGE);
        } else {
            s->overflow = 1;
        }
        s->nchanged++;
    }
    s->done = 1;
    _exit(0);
}

static const unsigned char *page_bytes(int side, uint32_t idx)
{
    struct side_result *s = &g_shm->side[side];
    int n = s->nchanged < MAX_COPY ? s->nchanged : MAX_COPY;
    for (int i = 0; i < n; i++)
        if (s->changed[i] == idx)
            return g_shm->copy[side][i];
    if (g_shm->post[side][idx] == g_shm->pre[idx])
        return (const unsigned char *)(uintptr_t)page_addr(idx); /* P still holds the pre-call bytes */
    return NULL;
}

/* JSON output helpers */
static char g_line[1 << 16];
static int g_lp;
static void jp(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_lp += vsnprintf(g_line + g_lp, sizeof g_line - g_lp, fmt, ap);
    va_end(ap);
    if (g_lp > (int)sizeof g_line - 1)
        g_lp = sizeof g_line - 1;
}
static void jstr(const unsigned char *p, int n)
{
    jp("\"");
    for (int i = 0; i < n && g_lp < (int)sizeof g_line - 16; i++) {
        unsigned char ch = p[i];
        if (ch == '"' || ch == '\\')
            jp("\\%c", ch);
        else if (ch < 0x20 || ch >= 0x7f)
            jp("\\u%04x", ch);
        else
            jp("%c", ch);
    }
    jp("\"");
}
static void jhex(const unsigned char *p, int n)
{
    jp("\"");
    for (int i = 0; i < n; i++)
        jp("%02x", p[i]);
    jp("\"");
}

static const char *sig_name(int sig)
{
    switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGABRT: return "SIGABRT";
    case SIGALRM: return "timeout";
    case SIGTRAP: return "SIGTRAP";
    default: return "signal";
    }
}

static void describe_status(int st, const struct side_result *s, char *buf, size_t n)
{
    if (WIFSIGNALED(st))
        snprintf(buf, n, "%s(%d)", sig_name(WTERMSIG(st)), WTERMSIG(st));
    else if (WIFEXITED(st) && s->done && WEXITSTATUS(st) == 0)
        snprintf(buf, n, "returned");
    else if (WIFEXITED(st) && WEXITSTATUS(st) == 97)
        snprintf(buf, n, "unresolved_import");
    else if (WIFEXITED(st) && WEXITSTATUS(st) == 96)
        snprintf(buf, n, "sandbox_denied:%s", s->denied >= 0 && s->denied < g_nimports ? g_import_name[s->denied] : "?");
    else if (WIFEXITED(st) && WEXITSTATUS(st) == 98)
        snprintf(buf, n, "unresolved_recon_symbol");
    else
        snprintf(buf, n, "exit(%d)", WEXITSTATUS(st));
}

static int out_read(int fd, unsigned char *buf, int cap)
{
    off_t len = lseek(fd, 0, SEEK_END);
    if (len > cap)
        len = cap;
    if (pread(fd, buf, len, 0) != len)
        return 0;
    return (int)len;
}

static uint64_t ret_masked(const struct hx_ret *r)
{
    switch (g_rk) {
    case RK_INT: {
        uint32_t v = r->eax;
        if (g_ret_bits < 32)
            v &= (1u << g_ret_bits) - 1;
        return v;
    }
    case RK_U64: return (uint64_t)r->edx << 32 | r->eax;
    case RK_PTR:
    case RK_STR: return r->eax;
    case RK_F32: {
        long double ld;
        memcpy(&ld, r->st0, 10);
        float f = (float)ld;
        uint32_t b;
        memcpy(&b, &f, 4);
        return b;
    }
    case RK_F64: {
        long double ld;
        memcpy(&ld, r->st0, 10);
        double d = (double)ld;
        uint64_t b;
        memcpy(&b, &d, 8);
        return b;
    }
    default: return 0;
    }
}

/* Runs one prepared case inside process P; prints one JSON line to resfd. */
static void run_case(int id, const struct call *c, int resfd, int strict_imports)
{
    int outfd[2];
    unsigned char *outbuf[2];
    int outlen[2];
    int st[2];
    char desc[2][64];

    g_shm->side[0].done = g_shm->side[1].done = 0;
    hash_all(g_shm->pre);
    for (int side = 0; side < 2; side++) {
        outfd[side] = memfd_create(side ? "recon_out" : "orig_out", 0);
        if (side == 1 && (WIFSIGNALED(st[0]) || (WIFEXITED(st[0]) && WEXITSTATUS(st[0]) >= 96 && WEXITSTATUS(st[0]) <= 98))) {
            /* the case is discarded anyway; do not spend time on the reconstruction */
            st[1] = st[0];
            snprintf(desc[1], sizeof desc[1], "not run");
            break;
        }
        fflush(NULL);
        pid_t pid = fork();
        if (pid == 0)
            run_side(side, c, outfd[side]);
        waitpid(pid, &st[side], 0);
        describe_status(st[side], &g_shm->side[side], desc[side], sizeof desc[side]);
    }
    for (int side = 0; side < 2; side++) {
        outbuf[side] = malloc(OUT_CAP);
        outlen[side] = out_read(outfd[side], outbuf[side], OUT_CAP);
    }
    struct side_result *A = &g_shm->side[0], *B = &g_shm->side[1];
    g_lp = 0;
    jp("{\"case\":%d,", id);
    int orig_ok = WIFEXITED(st[0]) && A->done;
    int orig_crash = WIFSIGNALED(st[0]) ||
                     (WIFEXITED(st[0]) && (WEXITSTATUS(st[0]) == 96 || WEXITSTATUS(st[0]) == 97 || WEXITSTATUS(st[0]) == 98));
    g_shm->last_kind = orig_crash ? (WIFSIGNALED(st[0]) && WTERMSIG(st[0]) == SIGALRM ? 2 : 1) : 0;
    if (orig_crash) {
        jp("\"status\":\"discard\",\"why\":\"original %s\",\"recon\":\"%s\"}\n", desc[0], desc[1]);
        goto out;
    }
    if (strcmp(desc[0], desc[1])) {
        jp("\"status\":\"fail\",\"what\":\"outcome\",\"orig\":\"%s\",\"recon\":\"%s\"", desc[0], desc[1]);
        if (outlen[1] && !(WIFEXITED(st[1]) && B->done)) {
            jp(",\"recon_output\":");
            jstr(outbuf[1], outlen[1] > 300 ? 300 : outlen[1]);
        }
        jp("}\n");
        goto out;
    }
    /* same outcome; for exit() outcomes only output and imports can be compared */
    if (orig_ok) {
        uint64_t ra = ret_masked(&A->ret), rb = ret_masked(&B->ret);
        int ret_eq = ra == rb;
        int literal_note = 0;
        if (!ret_eq && g_rk == RK_PTR)
            ret_eq = fn_equiv((uint32_t)ra, (uint32_t)rb) || literal_equiv((uint32_t)ra, (uint32_t)rb);
        if (!ret_eq && g_rk == RK_STR && A->retstr_ok && B->retstr_ok && !is_shared((uint32_t)ra) &&
            !is_shared((uint32_t)rb) && A->retstr_len == B->retstr_len &&
            !memcmp(A->retstr, B->retstr, A->retstr_len))
            ret_eq = literal_note = 1; /* both return a constant string literal of the own image */
        if (!ret_eq) {
            jp("\"status\":\"fail\",\"what\":\"return\",\"orig\":\"0x%llx\",\"recon\":\"0x%llx\"",
               (unsigned long long)ra, (unsigned long long)rb);
            if (g_rk == RK_STR) {
                jp(",\"orig_str\":");
                jstr((unsigned char *)A->retstr, A->retstr_ok ? (A->retstr_len > 200 ? 200 : A->retstr_len) : 0);
                jp(",\"recon_str\":");
                jstr((unsigned char *)B->retstr, B->retstr_ok ? (B->retstr_len > 200 ? 200 : B->retstr_len) : 0);
            }
            jp("}\n");
            goto out;
        }
        if (g_rk == RK_STR && (A->retstr_ok != B->retstr_ok || A->retstr_len != B->retstr_len ||
                               memcmp(A->retstr, B->retstr, A->retstr_len))) {
            jp("\"status\":\"fail\",\"what\":\"return_string\",\"orig\":");
            jstr((unsigned char *)A->retstr, A->retstr_len > 200 ? 200 : A->retstr_len);
            jp(",\"recon\":");
            jstr((unsigned char *)B->retstr, B->retstr_len > 200 ? 200 : B->retstr_len);
            jp("}\n");
            goto out;
        }
        /* memory: union of changed pages, ascending address */
        uint32_t n = n_rw_pages + g_arena_pages;
        long diffbytes = 0;
        int first = 1;
        for (uint32_t i = 0; i < n; i++) {
            if (g_shm->post[0][i] == g_shm->post[1][i] && g_shm->post[0][i] == g_shm->pre[i])
                continue;
            const unsigned char *pa = page_bytes(0, i), *pb = page_bytes(1, i);
            if (!pa || !pb) {
                if (g_shm->post[0][i] != g_shm->post[1][i]) {
                    if (first) {
                        jp("\"status\":\"fail\",\"what\":\"memory\",\"addr\":\"0x%08x\",\"detail\":\"page hash differs (copy overflow)\"",
                           page_addr(i));
                        first = 0;
                    }
                    diffbytes += 1;
                }
                continue;
            }
            for (uint32_t o = 0; o < PAGE; o++) {
                if (pa[o] == pb[o])
                    continue;
                uint32_t wo = o & ~3u;
                uint32_t wa, wb;
                memcpy(&wa, pa + wo, 4);
                memcpy(&wb, pb + wo, 4);
                if (fn_equiv(wa, wb) || literal_equiv(wa, wb)) {
                    o = wo + 3;
                    continue;
                }
                diffbytes++;
                if (first) {
                    uint32_t addr = page_addr(i) + o;
                    uint32_t w0 = o >= 8 ? o - 8 : 0;
                    uint32_t wl = (w0 + 32 <= PAGE) ? 32 : PAGE - w0;
                    jp("\"status\":\"fail\",\"what\":\"memory\",\"addr\":\"0x%08x\",\"region\":\"%s\",\"orig_byte\":\"0x%02x\",\"recon_byte\":\"0x%02x\",\"window_addr\":\"0x%08x\",\"orig_window\":",
                       addr, i < n_rw_pages ? "image_rw" : "arena", pa[o], pb[o], page_addr(i) + w0);
                    jhex(pa + w0, wl);
                    jp(",\"recon_window\":");
                    jhex(pb + w0, wl);
                    first = 0;
                }
            }
        }
        if (!first) {
            jp(",\"diff_bytes\":%ld}\n", diffbytes);
            goto out;
        }
    }
    if (outlen[0] != outlen[1] || memcmp(outbuf[0], outbuf[1], outlen[0])) {
        jp("\"status\":\"fail\",\"what\":\"stdout\",\"orig\":");
        jstr(outbuf[0], outlen[0] > 300 ? 300 : outlen[0]);
        jp(",\"recon\":");
        jstr(outbuf[1], outlen[1] > 300 ? 300 : outlen[1]);
        jp("}\n");
        goto out;
    }
    {
        int la = A->nlog < MAX_LOG ? A->nlog : MAX_LOG, lb = B->nlog < MAX_LOG ? B->nlog : MAX_LOG;
        int k = 0;
        while (k < la && k < lb && A->log[k] == B->log[k])
            k++;
        int same = A->nlog == B->nlog && k == la;
        if (!same) {
            jp("\"status\":\"%s\",\"what\":\"imports\",\"index\":%d,\"orig\":\"%s\",\"recon\":\"%s\",\"orig_n\":%d,\"recon_n\":%d",
               strict_imports ? "fail" : "pass", k, k < la ? g_import_name[A->log[k]] : "<end>",
               k < lb ? g_import_name[B->log[k]] : "<end>", A->nlog, B->nlog);
            jp(",\"orig_trace\":[");
            for (int i = 0; i < la && i < 12; i++)
                jp("%s\"%s\"", i ? "," : "", g_import_name[A->log[i]]);
            jp("],\"recon_trace\":[");
            for (int i = 0; i < lb && i < 12; i++)
                jp("%s\"%s\"", i ? "," : "", g_import_name[B->log[i]]);
            jp("]}\n");
            goto out;
        }
    }
    {
        /* x87 excess precision note: declared-type value equal, raw 80-bit differs */
        int note = (g_rk == RK_F32 || g_rk == RK_F64) && orig_ok && memcmp(A->ret.st0, B->ret.st0, 10);
        int lit = orig_ok && g_rk == RK_STR && A->ret.eax != B->ret.eax;
        jp("\"status\":\"pass\",\"outcome\":\"%s\"%s%s}\n", desc[0],
           note ? ",\"note\":\"x87 80-bit return differs before rounding\"" : "",
           lit ? ",\"literal\":\"returned pointers differ but both point to equal constant strings outside shared memory\"" : "");
    }
out:
    if (write(resfd, g_line, g_lp) < 0) {
    }
    close(outfd[0]);
    close(outfd[1]);
}

/* ------------------------------------------------------------------ main */
static uint32_t parse_u32(const char *s) { return (uint32_t)strtoul(s, NULL, 16); }

static void call_init(const struct call *c)
{
    struct hx_ret r;
    hx_call_cdecl((void *)(uintptr_t)c->addr, c->words, c->nwords, &r);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s <cod2_lnxded> <casefile> <resultfile> [libstdc++.so.5]\n", argv[0]);
        return 2;
    }
    load_original(argv[1], argc > 4 ? argv[4] : NULL);
    void *arena = mmap((void *)(uintptr_t)ARENA_BASE, ARENA_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (arena == MAP_FAILED || (uint32_t)(uintptr_t)arena != ARENA_BASE)
        die("cannot map arena");
    g_shm = mmap(NULL, sizeof *g_shm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_shm == MAP_FAILED)
        die("shm");
    FILE *cf = fopen(argv[2], "r");
    if (!cf)
        die("open cases %s", argv[2]);
    int resfd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (resfd < 0)
        die("open results");
    signal(SIGPIPE, SIG_IGN);
    {
        const char *sb = getenv("HX_SANDBOX");
        if (sb && chdir(sb) != 0)
            die("chdir %s: %s", sb, strerror(errno));
    }

    /* header line with binding report */
    {
        char hdr[8192];
        int hp = snprintf(hdr, sizeof hdr, "{\"header\":1,\"imports\":%d,\"libstdcxx5\":%s,\"unresolved\":[",
                          g_nimports, g_libstdcxx5 ? "true" : "false");
        for (int i = 0; i < g_nunresolved && hp < (int)sizeof hdr - 128; i++)
            hp += snprintf(hdr + hp, sizeof hdr - hp, "%s\"%s\"", i ? "," : "", g_import_name[g_unresolved[i]]);
        hp += snprintf(hdr + hp, sizeof hdr - hp, "],\"recon_functions\":%d}\n", hx_recon_count);
        if (write(resfd, hdr, hp) < 0) {
        }
    }

    size_t cap = 1 << 20;
    char *line = malloc(cap);
    struct obj objs[MAX_OBJS];
    int nobj = 0;
    struct call c, pre[16];
    int npre = 0;
    int case_id = -1, strict = 0;
    struct {
        uint32_t addr, len;
        unsigned char *data;
    } mems[32];
    int nmem = 0;
    struct {
        int obj;
        uint32_t off;
        int tobj;
        uint32_t toff;
    } fix[64];
    int nfix = 0;
    int argobj[MAX_WORDS];
    int ncases = 0, ncompared = 0, consecutive_timeouts = 0;
    memset(&c, 0, sizeof c);
    g_arena_pages = 64;

    while (fgets(line, cap, cf)) {
        size_t L = strlen(line);
        while (L && (line[L - 1] == '\n' || line[L - 1] == '\r'))
            line[--L] = 0;
        char *save = NULL;
        char *tok = strtok_r(line, " ", &save);
        if (!tok || tok[0] == '#')
            continue;
        if (!strcmp(tok, "target")) {
            g_target = parse_u32(strtok_r(NULL, " ", &save));
            g_recon_fn = (void *)(uintptr_t)recon_for(g_target, &g_recon_name);
            if (!g_recon_fn) {
                dprintf(resfd, "{\"error\":\"0x%08x has no reconstructed entry in this build\"}\n", g_target);
                return 3;
            }
        } else if (!strcmp(tok, "ret")) {
            const char *k = strtok_r(NULL, " ", &save);
            const char *bits = strtok_r(NULL, " ", &save);
            g_rk = !strcmp(k, "v") ? RK_VOID : !strcmp(k, "i") ? RK_INT : !strcmp(k, "q") ? RK_U64
                 : !strcmp(k, "p") ? RK_PTR : !strcmp(k, "s") ? RK_STR : !strcmp(k, "f") ? RK_F32 : RK_F64;
            g_ret_bits = bits ? atoi(bits) : 32;
        } else if (!strcmp(tok, "timeout")) {
            g_timeout = atoi(strtok_r(NULL, " ", &save));
        } else if (!strcmp(tok, "strict_imports")) {
            strict = 1;
        } else if (!strcmp(tok, "init")) {
            /* global prelude: call an ORIGINAL function in the runner before any case */
            struct call ic;
            ic.addr = parse_u32(strtok_r(NULL, " ", &save));
            ic.nwords = 0;
            char *w;
            while ((w = strtok_r(NULL, " ", &save)) && ic.nwords < MAX_WORDS)
                ic.words[ic.nwords++] = parse_u32(w);
            call_init(&ic);
        } else if (!strcmp(tok, "case")) {
            case_id = atoi(strtok_r(NULL, " ", &save));
            nobj = nmem = nfix = npre = 0;
            memset(&c, 0, sizeof c);
            c.addr = g_target;
        } else if (!strcmp(tok, "obj")) {
            int id = atoi(strtok_r(NULL, " ", &save));
            if (id != nobj || nobj >= MAX_OBJS)
                die("bad obj id");
            objs[nobj].size = parse_u32(strtok_r(NULL, " ", &save));
            objs[nobj].data = unhex(strtok_r(NULL, " ", &save), &objs[nobj].len);
            nobj++;
        } else if (!strcmp(tok, "fix")) {
            fix[nfix].obj = atoi(strtok_r(NULL, " ", &save));
            fix[nfix].off = parse_u32(strtok_r(NULL, " ", &save));
            fix[nfix].tobj = atoi(strtok_r(NULL, " ", &save));
            fix[nfix].toff = parse_u32(strtok_r(NULL, " ", &save));
            nfix++;
        } else if (!strcmp(tok, "mem")) {
            mems[nmem].addr = parse_u32(strtok_r(NULL, " ", &save));
            mems[nmem].data = unhex(strtok_r(NULL, " ", &save), &mems[nmem].len);
            nmem++;
        } else if (!strcmp(tok, "pre")) {
            pre[npre].addr = parse_u32(strtok_r(NULL, " ", &save));
            pre[npre].nwords = 0;
            char *w;
            while ((w = strtok_r(NULL, " ", &save)) && pre[npre].nwords < MAX_WORDS)
                pre[npre].words[pre[npre].nwords++] = parse_u32(w);
            npre++;
        } else if (!strcmp(tok, "arg")) {
            const char *k = strtok_r(NULL, " ", &save);
            if (c.nwords >= MAX_WORDS)
                die("too many args");
            if (!strcmp(k, "w"))
                argobj[c.nwords] = -1, c.words[c.nwords++] = parse_u32(strtok_r(NULL, " ", &save));
            else if (!strcmp(k, "o")) {
                int id = atoi(strtok_r(NULL, " ", &save));
                uint32_t off = parse_u32(strtok_r(NULL, " ", &save));
                argobj[c.nwords] = id;
                c.words[c.nwords++] = off; /* resolved to arena address at go */
            } else
                die("bad arg kind %s", k);
        } else if (!strcmp(tok, "go")) {
            /* lay out objects in the arena: 16-byte aligned with a 64-byte 0xA5 gap */
            uint32_t cur = 64;
            for (int i = 0; i < nobj; i++) {
                cur = (cur + 15) & ~15u;
                objs[i].addr = ARENA_BASE + cur;
                cur += objs[i].size + 64;
            }
            if (cur > ARENA_SIZE - 65536)
                die("arena overflow");
            g_arena_pages = (cur + 65536 + PAGE - 1) / PAGE;
            for (int i = 0; i < c.nwords; i++)
                if (argobj[i] >= nobj)
                    die("arg refers to missing obj %d", argobj[i]);
            fflush(NULL);
            pid_t p = fork();
            if (p == 0) {
                memset(arena, 0xA5, g_arena_pages * PAGE);
                for (int i = 0; i < nobj; i++) {
                    unsigned char *d = (unsigned char *)(uintptr_t)objs[i].addr;
                    memset(d, 0, objs[i].size);
                    if (objs[i].len)
                        memcpy(d, objs[i].data, objs[i].len > objs[i].size ? objs[i].size : objs[i].len);
                }
                for (int i = 0; i < nfix; i++) {
                    uint32_t v = objs[fix[i].tobj].addr + fix[i].toff;
                    memcpy((void *)(uintptr_t)(objs[fix[i].obj].addr + fix[i].off), &v, 4);
                }
                for (int i = 0; i < nmem; i++)
                    memcpy((void *)(uintptr_t)mems[i].addr, mems[i].data, mems[i].len);
                for (int i = 0; i < npre; i++)
                    call_init(&pre[i]);
                for (int i = 0; i < c.nwords; i++)
                    if (argobj[i] >= 0)
                        c.words[i] += objs[argobj[i]].addr;
                run_case(case_id, &c, resfd, strict);
                _exit(0);
            }
            int st;
            g_shm->last_kind = 1;
            waitpid(p, &st, 0);
            /* give up early on functions the generated inputs cannot drive */
            ncases++;
            if (g_shm->last_kind == 0)
                ncompared++;
            consecutive_timeouts = g_shm->last_kind == 2 ? consecutive_timeouts + 1 : 0;
            if (consecutive_timeouts >= 5 || (ncases >= 60 && ncompared == 0)) {
                dprintf(resfd, "{\"aborted\":\"%s after %d cases\"}\n",
                        consecutive_timeouts >= 5 ? "5 consecutive original timeouts" : "no comparable case", ncases);
                break;
            }
            if (!WIFEXITED(st) || WEXITSTATUS(st))
                dprintf(resfd, "{\"case\":%d,\"status\":\"error\",\"why\":\"case process status 0x%x\"}\n", case_id, st);
            for (int i = 0; i < nobj; i++)
                free(objs[i].data);
            for (int i = 0; i < nmem; i++)
                free(mems[i].data);
        }
    }
    close(resfd);
    return 0;
}
