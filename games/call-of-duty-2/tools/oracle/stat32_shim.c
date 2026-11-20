/* LD_PRELOAD shim for the 32-bit original CoD2 server.
 * The server calls the legacy 32-bit __xstat/__lxstat/__fxstat ABI (struct stat with 32-bit st_ino).
 * On CIFS mounts with serverino, inode numbers exceed 32 bits and glibc returns EOVERFLOW,
 * so iwd files on some network filesystems silently fail to open. This shim answers with the 64-bit stat and
 * folds st_ino into 32 bits. It changes no file contents and writes nothing. Build: gcc -m32 -shared -fPIC */
#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

extern int __xstat64(int, const char *, struct stat64 *);
extern int __lxstat64(int, const char *, struct stat64 *);
extern int __fxstat64(int, int, struct stat64 *);

static void conv(const struct stat64 *s, struct stat *o) {
    memset(o, 0, sizeof *o);
    o->st_dev = s->st_dev;
    o->st_ino = (ino_t)((s->st_ino ^ (s->st_ino >> 32)) & 0xffffffffu);
    o->st_mode = s->st_mode; o->st_nlink = s->st_nlink;
    o->st_uid = s->st_uid; o->st_gid = s->st_gid; o->st_rdev = s->st_rdev;
    o->st_size = (off_t)s->st_size; o->st_blksize = s->st_blksize; o->st_blocks = (blkcnt_t)s->st_blocks;
    o->st_atim = s->st_atim; o->st_mtim = s->st_mtim; o->st_ctim = s->st_ctim;
}
#define WRAP(name, call64, ARGS, CALL) \
int name ARGS { struct stat64 s; int r = call64 CALL; if (r) return r; \
    if (s.st_size > 0x7fffffffLL) { errno = EOVERFLOW; return -1; } conv(&s, buf); return 0; }
WRAP(__xstat,  __xstat64,  (int v, const char *p, struct stat *buf), (v, p, &s))
WRAP(__lxstat, __lxstat64, (int v, const char *p, struct stat *buf), (v, p, &s))
WRAP(__fxstat, __fxstat64, (int v, int fd, struct stat *buf), (v, fd, &s))

/* Legacy 32-bit readdir(): glibc fails with EOVERFLOW when d_ino does not fit. Serve readdir64 data
 * through a per-call static legacy-layout record (struct dirent, 32-bit d_ino/d_off). */
#include <dirent.h>
struct dirent *readdir(DIR *d) {
    static struct dirent out;
    struct dirent64 *e = readdir64(d);
    if (!e) return 0;
    memset(&out, 0, sizeof out);
    out.d_ino = (ino_t)((e->d_ino ^ (e->d_ino >> 32)) & 0xffffffffu);
    out.d_off = (off_t)e->d_off;
    out.d_reclen = sizeof out;
    out.d_type = e->d_type;
    strncpy(out.d_name, e->d_name, sizeof out.d_name - 1);
    return &out;
}
