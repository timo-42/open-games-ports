/*
 * Chunk 000 (cod2_lnxded 1.3, 0x08049d2c..0x804a704) types and out-of-chunk prototypes.
 * Only out-of-chunk callees/targets are declared here; placeholders fn_08xxxxxx are
 * rewritten by the merge queue.
 */
#ifndef COD2_CHUNK_000_TYPES_H
#define COD2_CHUNK_000_TYPES_H

/* CRT entry arguments passed to __libc_start_main by _start (targets of $symbol immediates). */
extern "C" void libc_csu_init(void);          /* init argument: CRT libc init routine */
extern "C" void Crt_RunFini(void);          /* fini argument: CRT libc fini routine */
#ifdef COD2_RECON_ORIGINAL_CRT
extern "C" int Sys_Main(int argc, char **argv, char **envp); /* main */
#endif

/* Called from _init, in call order. */
extern "C" void frame_dummy(void);          /* CRT frame_dummy (crtbegin) */
extern "C" void Crt_RunCtors(void);          /* CRT constructor driver (__do_global_ctors_aux) */

#endif
