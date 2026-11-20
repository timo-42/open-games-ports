/*
 * chunk 061 types: PunkBuster server bridge (0x0813bf4c..0x0813ce98 range), red-black tree
 * node helpers (old-ABI std::map layout), PunkBuster path/DLL helpers, MD5 context and
 * crt init/fini glue, 0x0813b8fe..0x0813e310.
 *
 * Layouts are recovered only from offsets used in this chunk. Fields touched only by other
 * chunks are left as padding. Names are inferred from call structure and rodata strings
 * unless noted in the chunk provenance comments.
 */
#ifndef COD2_CHUNK_061_TYPES_H
#define COD2_CHUNK_061_TYPES_H

#include "cod2_types.h"
#include <strings.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

/* ---- libc / C++ runtime entry points used by this chunk ---- */
extern "C" int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle);

/* std::string handle (struct cod2_string_s), the std::map node (struct cod2_rb_node_s) and the
 * std::map object (struct cod2_str_map_s) are shared with chunk 060 (same old-ABI libstdc++ layouts). */
#include "chunk_060_types.h"

void cod2_string_copy_ctor(struct cod2_string_s *dst, const struct cod2_string_s *src)
    __asm__("_ZNSsC1ERKSs");
void cod2_string_dtor(struct cod2_string_s *str) __asm__("_ZNSsD1Ev");
void *cod2_operator_new_array(size_t size) __asm__("_Znaj");
void cod2_operator_delete(void *ptr) __asm__("_ZdlPv");

/* std::pair<std::string, std::string> as stored in a node at +0x10 (= cod2_rb_node_s key, value). */
struct rb_pair_s {
    struct cod2_string_s first;  /* +0x00 */
    struct cod2_string_s second; /* +0x04 */
};
COD2_ASSERT_SIZE(rb_pair_s, 0x8);

/* ---- PunkBuster server object (global g_08a6f2c0, .bss) ---- */
struct punkbuster_s;
typedef int (*pb_query_fn_t)(struct punkbuster_s *pb, int event);                     /* +0x150 "sa" */
typedef int (*pb_dispatch_fn_t)(struct punkbuster_s *pb, int event, int a, int b,
                                int c, int d);                                        /* +0x14c "sb" */
typedef int (*pb_cmd_fn_t)(char *cmd, char *args);                                    /* +0x13c */
typedef void (*pb_cvar_query_fn_t)(int key, char *buf);                              /* +0x140 */
typedef void (*pb_cb158_fn_t)(struct punkbuster_s *pb, int a, int b);                /* +0x158 */
typedef int (*pb_cb15c_fn_t)(struct punkbuster_s *pb, int a, int b, int c);          /* +0x15c */
typedef void (*pb_cb160_fn_t)(struct punkbuster_s *pb, char *text, int n);           /* +0x160 */
typedef void (*pb_void_fn_t)(void);

struct punkbuster_s {
    unsigned int magic;          /* +0x00 0x357afe24 */
    int state_04;                /* +0x04 cleared on unload */
    void *dll_sv;                /* +0x08 pbsv.so handle */
    void *dll_cl;                /* +0x0c pbcl.so handle */
    void *dll_ag;                /* +0x10 pbag.so handle */
    char name[32];               /* +0x14 "PunkBuster Server" */
    char pb_dir[256];            /* +0x34 base directory with trailing "pb/" */
    unsigned char pad134[4];
    int dll_pending;             /* +0x138 1 = needs (re)load */
    pb_cmd_fn_t cmd_handler;     /* +0x13c NULL = disabled */
    pb_cvar_query_fn_t cvar_query; /* +0x140 */
    pb_void_fn_t cb_144;         /* +0x144 */
    pb_void_fn_t cb_148;         /* +0x148 */
    pb_dispatch_fn_t proc_sb;    /* +0x14c */
    pb_query_fn_t proc_sa;       /* +0x150 */
    pb_void_fn_t cb_154;         /* +0x154 */
    pb_cb158_fn_t cb_158;        /* +0x158 */
    pb_cb15c_fn_t cb_15c;        /* +0x15c */
    pb_cb160_fn_t cb_160;        /* +0x160 */
    int field_164;               /* +0x164 cleared by the pbag unload path */
    char *con_cap_buf;           /* +0x168 console capture buffer */
    int con_cap_len;             /* +0x16c console capture limit (stores pointer bits) */
};
COD2_ASSERT_OFFSET(punkbuster_s, name, 0x14);
COD2_ASSERT_OFFSET(punkbuster_s, pb_dir, 0x34);
COD2_ASSERT_OFFSET(punkbuster_s, dll_pending, 0x138);
COD2_ASSERT_OFFSET(punkbuster_s, cmd_handler, 0x13c);
COD2_ASSERT_OFFSET(punkbuster_s, cvar_query, 0x140);
COD2_ASSERT_OFFSET(punkbuster_s, proc_sb, 0x14c);
COD2_ASSERT_OFFSET(punkbuster_s, proc_sa, 0x150);
COD2_ASSERT_OFFSET(punkbuster_s, cb_158, 0x158);
COD2_ASSERT_OFFSET(punkbuster_s, cb_15c, 0x15c);
COD2_ASSERT_OFFSET(punkbuster_s, cb_160, 0x160);
COD2_ASSERT_OFFSET(punkbuster_s, con_cap_buf, 0x168);
COD2_ASSERT_OFFSET(punkbuster_s, con_cap_len, 0x16c);
COD2_ASSERT_SIZE(punkbuster_s, 0x170);


/* ---- MD5 context (RFC 1321 layout used by the binary) ---- */
struct md5_ctx_s {
    unsigned int count[2];       /* +0x00 bit count low, high */
    unsigned int state[4];       /* +0x08 A, B, C, D */
    byte buffer[64];             /* +0x18 pending input */
    byte digest[16];             /* +0x58 written by MD5_Final */
};
COD2_ASSERT_OFFSET(md5_ctx_s, state, 0x8);
COD2_ASSERT_OFFSET(md5_ctx_s, buffer, 0x18);
COD2_ASSERT_OFFSET(md5_ctx_s, digest, 0x58);
COD2_ASSERT_SIZE(md5_ctx_s, 0x68);

/* ---- static data referenced by this chunk ---- */

/* ---- out-of-chunk callees (placeholders; the merge lane rewrites fn_08xxxxxx names) ---- */
extern "C" void _init(void); /* defined with C linkage by chunk 000 */
void __do_global_dtors_aux(void);

#endif
