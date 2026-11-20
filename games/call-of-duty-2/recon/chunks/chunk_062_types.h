/*
 * Chunk 062 (cod2_lnxded 1.3, placeholder range 0x08a700f8..0x08a7025c) types and
 * out-of-chunk prototypes.
 *
 * This range holds no program code. Every entry is a Ghidra EXTERNAL placeholder for
 * a dynamic import; the binary's bytes at these addresses do not exist (the ELF
 * image has no bytes at these addresses). Each import is resolved to a real PLT entry in
 * chunk_000 (0x0804a134..0x0804a6a4) and its body is Ghidra halt_baddata().
 * No types or prototypes are needed: callers reference the imported C or C++ symbol
 * directly, as chunk_000 specifies.
 */
#ifndef COD2_CHUNK_062_TYPES_H
#define COD2_CHUNK_062_TYPES_H

#endif
