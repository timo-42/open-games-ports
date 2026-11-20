/*
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 062.
 * Placeholder range 0x08a700f8..0x08a7025c: 89 Ghidra EXTERNAL entries for dynamic
 * imports (libc, libstdc++, pthread, dl, socket). Binary SHA-256
 * 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * Each entry has a body of halt_baddata() in the decompiler output and no bytes in
 * the ELF image. The real call target is the PLT entry listed in each provenance line
 * (jmp via GOT), which chunk_000 already records. No body is emitted: the
 * observable behaviour is the dynamic-linker jump to the imported symbol, and
 * reconstructed callers call the imported symbol directly.
 */
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
/* libc names that the generated prototype header (chunk_000 PLT entries) declares with
 * non-system signatures/linkage: hide them from it so the system headers stay authoritative. */
#include "cod2_prototypes.h"
#include "chunk_062_types.h"

// 0x08a700f8  size=1  time  status=translated  naming=observed
// naming: ELF dynamic import 'time' (long time(long *tloc)); Ghidra EXTERNAL placeholder 0x8a700f8 with body halt_baddata(); real PLT entry 0x0804a134 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a700fc  size=1  pthread_mutexattr_init  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutexattr_init' (int pthread_mutexattr_init(void *attr)); Ghidra EXTERNAL placeholder 0x8a700fc with body halt_baddata(); real PLT entry 0x0804a144 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70100  size=1  pthread_mutex_lock  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutex_lock' (int pthread_mutex_lock(void *mutex)); Ghidra EXTERNAL placeholder 0x8a70100 with body halt_baddata(); real PLT entry 0x0804a154 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70104  size=1  fputs  status=translated  naming=observed
// naming: ELF dynamic import 'fputs' (int fputs(const char *s, void *stream)); Ghidra EXTERNAL placeholder 0x8a70104 with body halt_baddata(); real PLT entry 0x0804a164 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70108  size=1  strstr  status=translated  naming=observed
// naming: ELF dynamic import 'strstr' (char *strstr(const char *haystack, const char *needle)); Ghidra EXTERNAL placeholder 0x8a70108 with body halt_baddata(); real PLT entry 0x0804a174 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7010c  size=1  strlen  status=translated  naming=observed
// naming: ELF dynamic import 'strlen' (unsigned int strlen(const char *s)); Ghidra EXTERNAL placeholder 0x8a7010c with body halt_baddata(); real PLT entry 0x0804a184 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70110  size=1  longjmp  status=translated  naming=observed
// naming: ELF dynamic import 'longjmp' (void longjmp(void *env, int val)); Ghidra EXTERNAL placeholder 0x8a70110 with body halt_baddata(); real PLT entry 0x0804a194 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70114  size=1  sleep  status=translated  naming=observed
// naming: ELF dynamic import 'sleep' (unsigned int sleep(unsigned int seconds)); Ghidra EXTERNAL placeholder 0x8a70114 with body halt_baddata(); real PLT entry 0x0804a1a4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70118  size=1  __cxa_end_catch  status=translated  naming=observed
// naming: ELF dynamic import '__cxa_end_catch' (void __cxa_end_catch(void)); Ghidra EXTERNAL placeholder 0x8a70118 with body halt_baddata(); real PLT entry 0x0804a1b4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7011c  size=1  qsort  status=translated  naming=observed
// naming: ELF dynamic import 'qsort' (void qsort(void *base, unsigned int nmemb, unsigned int size, void *compar)); Ghidra EXTERNAL placeholder 0x8a7011c with body halt_baddata(); real PLT entry 0x0804a1c4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70120  size=1  atof  status=translated  naming=observed
// naming: ELF dynamic import 'atof' (double atof(const char *nptr)); Ghidra EXTERNAL placeholder 0x8a70120 with body halt_baddata(); real PLT entry 0x0804a1d4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70124  size=1  atan2  status=translated  naming=observed
// naming: ELF dynamic import 'atan2' (double atan2(double y, double x)); Ghidra EXTERNAL placeholder 0x8a70124 with body halt_baddata(); real PLT entry 0x0804a1e4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70128  size=1  strncmp  status=translated  naming=observed
// naming: ELF dynamic import 'strncmp' (int strncmp(const char *a, const char *b, unsigned int n)); Ghidra EXTERNAL placeholder 0x8a70128 with body halt_baddata(); real PLT entry 0x0804a1f4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7012c  size=1  inet_addr  status=translated  naming=observed
// naming: ELF dynamic import 'inet_addr' (uint32 inet_addr(const char *cp)); Ghidra EXTERNAL placeholder 0x8a7012c with body halt_baddata(); real PLT entry 0x0804a204 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70130  size=1  floor  status=translated  naming=observed
// naming: ELF dynamic import 'floor' (double floor(double x)); Ghidra EXTERNAL placeholder 0x8a70130 with body halt_baddata(); real PLT entry 0x0804a214 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70134  size=1  memcmp  status=translated  naming=observed
// naming: ELF dynamic import 'memcmp' (int memcmp(const void *a, const void *b, unsigned int n)); Ghidra EXTERNAL placeholder 0x8a70134 with body halt_baddata(); real PLT entry 0x0804a224 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70138  size=1  __gxx_personality_v0  status=translated  naming=observed
// naming: ELF dynamic import '__gxx_personality_v0' (int __gxx_personality_v0(int version, int actions, uint64 exception_class, void *exception_object, void *context)); Ghidra EXTERNAL placeholder 0x8a70138 with body halt_baddata(); real PLT entry 0x0804a234 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7013c  size=1  std::string::erase  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSs5eraseEjj' (void *_ZNSs5eraseEjj(void *self, unsigned int pos, unsigned int len)); Ghidra EXTERNAL placeholder 0x8a7013c with body halt_baddata(); real PLT entry 0x0804a244 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70140  size=1  pthread_self  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_self' (unsigned int pthread_self(void)); Ghidra EXTERNAL placeholder 0x8a70140 with body halt_baddata(); real PLT entry 0x0804a254 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70144  size=1  std::string::operator+=  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSspLEc' (void *_ZNSspLEc(void *self, char c)); Ghidra EXTERNAL placeholder 0x8a70144 with body halt_baddata(); real PLT entry 0x0804a264 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70148  size=1  dlerror  status=translated  naming=observed
// naming: ELF dynamic import 'dlerror' (const char *dlerror(void)); Ghidra EXTERNAL placeholder 0x8a70148 with body halt_baddata(); real PLT entry 0x0804a274 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7014c  size=1  __libc_start_main  status=translated  naming=observed
// naming: ELF dynamic import '__libc_start_main' (int __libc_start_main(void *main, int argc, void *argv, void *init, void *fini, void *rtld_fini, void *stack_end)); Ghidra EXTERNAL placeholder 0x8a7014c with body halt_baddata(); real PLT entry 0x0804a284 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70150  size=1  std::string::find  status=translated  naming=observed
// naming: ELF dynamic import '_ZNKSs4findEPKcj' (unsigned int _ZNKSs4findEPKcj(const void *self, const char *s, unsigned int pos)); Ghidra EXTERNAL placeholder 0x8a70150 with body halt_baddata(); real PLT entry 0x0804a294 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70154  size=1  toupper  status=translated  naming=observed
// naming: ELF dynamic import 'toupper' (int toupper(int c)); Ghidra EXTERNAL placeholder 0x8a70154 with body halt_baddata(); real PLT entry 0x0804a2a4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70158  size=1  sqrtf  status=translated  naming=observed
// naming: ELF dynamic import 'sqrtf' (float sqrtf(float x)); Ghidra EXTERNAL placeholder 0x8a70158 with body halt_baddata(); real PLT entry 0x0804a2b4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7015c  size=1  realloc  status=translated  naming=observed
// naming: ELF dynamic import 'realloc' (void *realloc(void *ptr, unsigned int size)); Ghidra EXTERNAL placeholder 0x8a7015c with body halt_baddata(); real PLT entry 0x0804a2c4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70160  size=1  strcat  status=translated  naming=observed
// naming: ELF dynamic import 'strcat' (char *strcat(char *dest, const char *src)); Ghidra EXTERNAL placeholder 0x8a70160 with body halt_baddata(); real PLT entry 0x0804a2d4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70164  size=1  tan  status=translated  naming=observed
// naming: ELF dynamic import 'tan' (double tan(double x)); Ghidra EXTERNAL placeholder 0x8a70164 with body halt_baddata(); real PLT entry 0x0804a2e4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70168  size=1  asctime  status=translated  naming=observed
// naming: ELF dynamic import 'asctime' (char *asctime(const void *tm)); Ghidra EXTERNAL placeholder 0x8a70168 with body halt_baddata(); real PLT entry 0x0804a2f4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7016c  size=1  printf  status=translated  naming=observed
// naming: ELF dynamic import 'printf' (int printf(const char *format, ...)); Ghidra EXTERNAL placeholder 0x8a7016c with body halt_baddata(); real PLT entry 0x0804a304 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70170  size=1  bind  status=translated  naming=observed
// naming: ELF dynamic import 'bind' (int bind(int fd, const void *addr, unsigned int addrlen)); Ghidra EXTERNAL placeholder 0x8a70170 with body halt_baddata(); real PLT entry 0x0804a314 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70174  size=1  getuid  status=translated  naming=observed
// naming: ELF dynamic import 'getuid' (unsigned int getuid(void)); Ghidra EXTERNAL placeholder 0x8a70174 with body halt_baddata(); real PLT entry 0x0804a324 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70178  size=1  fcntl  status=translated  naming=observed
// naming: ELF dynamic import 'fcntl' (int fcntl(int fd, int cmd, ...)); Ghidra EXTERNAL placeholder 0x8a70178 with body halt_baddata(); real PLT entry 0x0804a334 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7017c  size=1  memcpy  status=translated  naming=observed
// naming: ELF dynamic import 'memcpy' (void *memcpy(void *dest, const void *src, unsigned int n)); Ghidra EXTERNAL placeholder 0x8a7017c with body halt_baddata(); real PLT entry 0x0804a344 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70180  size=1  sqrt  status=translated  naming=observed
// naming: ELF dynamic import 'sqrt' (double sqrt(double x)); Ghidra EXTERNAL placeholder 0x8a70180 with body halt_baddata(); real PLT entry 0x0804a354 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70184  size=1  std::string::string  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsC1ERKSs' (void _ZNSsC1ERKSs(void *self, const void *other)); Ghidra EXTERNAL placeholder 0x8a70184 with body halt_baddata(); real PLT entry 0x0804a364 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70188  size=1  std::allocator<char>::~allocator  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSaIcED1Ev' (void _ZNSaIcED1Ev(void *self)); Ghidra EXTERNAL placeholder 0x8a70188 with body halt_baddata(); real PLT entry 0x0804a374 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7018c  size=1  fclose  status=translated  naming=observed
// naming: ELF dynamic import 'fclose' (int fclose(void *stream)); Ghidra EXTERNAL placeholder 0x8a7018c with body halt_baddata(); real PLT entry 0x0804a384 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70190  size=1  strrchr  status=translated  naming=observed
// naming: ELF dynamic import 'strrchr' (char *strrchr(const char *s, int c)); Ghidra EXTERNAL placeholder 0x8a70190 with body halt_baddata(); real PLT entry 0x0804a394 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70194  size=1  ctime  status=translated  naming=observed
// naming: ELF dynamic import 'ctime' (char *ctime(const long *timep)); Ghidra EXTERNAL placeholder 0x8a70194 with body halt_baddata(); real PLT entry 0x0804a3a4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70198  size=1  isdigit  status=translated  naming=observed
// naming: ELF dynamic import 'isdigit' (int isdigit(int c)); Ghidra EXTERNAL placeholder 0x8a70198 with body halt_baddata(); real PLT entry 0x0804a3b4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7019c  size=1  rmdir  status=translated  naming=observed
// naming: ELF dynamic import 'rmdir' (int rmdir(const char *path)); Ghidra EXTERNAL placeholder 0x8a7019c with body halt_baddata(); real PLT entry 0x0804a3c4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701a0  size=1  closedir  status=translated  naming=observed
// naming: ELF dynamic import 'closedir' (int closedir(void *dirp)); Ghidra EXTERNAL placeholder 0x8a701a0 with body halt_baddata(); real PLT entry 0x0804a3d4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701a4  size=1  gettimeofday  status=translated  naming=observed
// naming: ELF dynamic import 'gettimeofday' (int gettimeofday(void *tv, void *tz)); Ghidra EXTERNAL placeholder 0x8a701a4 with body halt_baddata(); real PLT entry 0x0804a3e4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701a8  size=1  srand  status=translated  naming=observed
// naming: ELF dynamic import 'srand' (void srand(unsigned int seed)); Ghidra EXTERNAL placeholder 0x8a701a8 with body halt_baddata(); real PLT entry 0x0804a3f4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701ac  size=1  opendir  status=translated  naming=observed
// naming: ELF dynamic import 'opendir' (void *opendir(const char *name)); Ghidra EXTERNAL placeholder 0x8a701ac with body halt_baddata(); real PLT entry 0x0804a404 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701b0  size=1  snprintf  status=translated  naming=observed
// naming: ELF dynamic import 'snprintf' (int snprintf(char *str, unsigned int size, const char *format, ...)); Ghidra EXTERNAL placeholder 0x8a701b0 with body halt_baddata(); real PLT entry 0x0804a414 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701b4  size=1  gethostname  status=translated  naming=observed
// naming: ELF dynamic import 'gethostname' (int gethostname(char *name, unsigned int len)); Ghidra EXTERNAL placeholder 0x8a701b4 with body halt_baddata(); real PLT entry 0x0804a424 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701b8  size=1  gethostbyname  status=translated  naming=observed
// naming: ELF dynamic import 'gethostbyname' (void *gethostbyname(const char *name)); Ghidra EXTERNAL placeholder 0x8a701b8 with body halt_baddata(); real PLT entry 0x0804a434 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701bc  size=1  strcasecmp  status=translated  naming=observed
// naming: ELF dynamic import 'strcasecmp' (int strcasecmp(const char *a, const char *b)); Ghidra EXTERNAL placeholder 0x8a701bc with body halt_baddata(); real PLT entry 0x0804a444 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701c0  size=1  dlclose  status=translated  naming=observed
// naming: ELF dynamic import 'dlclose' (int dlclose(void *handle)); Ghidra EXTERNAL placeholder 0x8a701c0 with body halt_baddata(); real PLT entry 0x0804a454 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701c4  size=1  exit  status=translated  naming=observed
// naming: ELF dynamic import 'exit' (void exit(int status)); Ghidra EXTERNAL placeholder 0x8a701c4 with body halt_baddata(); real PLT entry 0x0804a464 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701c8  size=1  isalpha  status=translated  naming=observed
// naming: ELF dynamic import 'isalpha' (int isalpha(int c)); Ghidra EXTERNAL placeholder 0x8a701c8 with body halt_baddata(); real PLT entry 0x0804a474 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701cc  size=1  atoi  status=translated  naming=observed
// naming: ELF dynamic import 'atoi' (int atoi(const char *nptr)); Ghidra EXTERNAL placeholder 0x8a701cc with body halt_baddata(); real PLT entry 0x0804a484 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701d0  size=1  calloc  status=translated  naming=observed
// naming: ELF dynamic import 'calloc' (void *calloc(unsigned int nmemb, unsigned int size)); Ghidra EXTERNAL placeholder 0x8a701d0 with body halt_baddata(); real PLT entry 0x0804a494 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701d4  size=1  sscanf  status=translated  naming=observed
// naming: ELF dynamic import 'sscanf' (int sscanf(const char *str, const char *format, ...)); Ghidra EXTERNAL placeholder 0x8a701d4 with body halt_baddata(); real PLT entry 0x0804a4a4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701d8  size=1  free  status=translated  naming=observed
// naming: ELF dynamic import 'free' (void free(void *ptr)); Ghidra EXTERNAL placeholder 0x8a701d8 with body halt_baddata(); real PLT entry 0x0804a4b4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701dc  size=1  ioctl  status=translated  naming=observed
// naming: ELF dynamic import 'ioctl' (int ioctl(int fd, unsigned long request, ...)); Ghidra EXTERNAL placeholder 0x8a701dc with body halt_baddata(); real PLT entry 0x0804a4c4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701e0  size=1  htons  status=translated  naming=observed
// naming: ELF dynamic import 'htons' (uint16 htons(uint16 hostshort)); Ghidra EXTERNAL placeholder 0x8a701e0 with body halt_baddata(); real PLT entry 0x0804a4d4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701e4  size=1  getcwd  status=translated  naming=observed
// naming: ELF dynamic import 'getcwd' (char *getcwd(char *buf, unsigned int size)); Ghidra EXTERNAL placeholder 0x8a701e4 with body halt_baddata(); real PLT entry 0x0804a4e4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701e8  size=1  __cxa_begin_catch  status=translated  naming=observed
// naming: ELF dynamic import '__cxa_begin_catch' (void *__cxa_begin_catch(void *exceptionObject)); Ghidra EXTERNAL placeholder 0x8a701e8 with body halt_baddata(); real PLT entry 0x0804a4f4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701ec  size=1  isatty  status=translated  naming=observed
// naming: ELF dynamic import 'isatty' (int isatty(int fd)); Ghidra EXTERNAL placeholder 0x8a701ec with body halt_baddata(); real PLT entry 0x0804a504 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701f0  size=1  acos  status=translated  naming=observed
// naming: ELF dynamic import 'acos' (double acos(double x)); Ghidra EXTERNAL placeholder 0x8a701f0 with body halt_baddata(); real PLT entry 0x0804a514 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701f4  size=1  memset  status=translated  naming=observed
// naming: ELF dynamic import 'memset' (void *memset(void *s, int c, unsigned int n)); Ghidra EXTERNAL placeholder 0x8a701f4 with body halt_baddata(); real PLT entry 0x0804a524 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701f8  size=1  _exit  status=translated  naming=observed
// naming: ELF dynamic import '_exit' (void _exit(int status)); Ghidra EXTERNAL placeholder 0x8a701f8 with body halt_baddata(); real PLT entry 0x0804a534 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a701fc  size=1  strncasecmp  status=translated  naming=observed
// naming: ELF dynamic import 'strncasecmp' (int strncasecmp(const char *a, const char *b, unsigned int n)); Ghidra EXTERNAL placeholder 0x8a701fc with body halt_baddata(); real PLT entry 0x0804a544 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70200  size=1  strncpy  status=translated  naming=observed
// naming: ELF dynamic import 'strncpy' (char *strncpy(char *dest, const char *src, unsigned int n)); Ghidra EXTERNAL placeholder 0x8a70200 with body halt_baddata(); real PLT entry 0x0804a554 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70204  size=1  std::string::operator+=  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSspLEPKc' (void *_ZNSspLEPKc(void *self, const char *s)); Ghidra EXTERNAL placeholder 0x8a70204 with body halt_baddata(); real PLT entry 0x0804a564 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70208  size=1  fopen  status=translated  naming=observed
// naming: ELF dynamic import 'fopen' (void *fopen(const char *path, const char *mode)); Ghidra EXTERNAL placeholder 0x8a70208 with body halt_baddata(); real PLT entry 0x0804a574 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7020c  size=1  _setjmp  status=translated  naming=observed
// naming: ELF dynamic import '_setjmp' (int _setjmp(void *env)); Ghidra EXTERNAL placeholder 0x8a7020c with body halt_baddata(); real PLT entry 0x0804a584 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70210  size=1  _Unwind_Resume  status=translated  naming=observed
// naming: ELF dynamic import '_Unwind_Resume' (void _Unwind_Resume(void *exception_object)); Ghidra EXTERNAL placeholder 0x8a70210 with body halt_baddata(); real PLT entry 0x0804a594 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70214  size=1  pthread_mutex_unlock  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutex_unlock' (int pthread_mutex_unlock(void *mutex)); Ghidra EXTERNAL placeholder 0x8a70214 with body halt_baddata(); real PLT entry 0x0804a5a4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70218  size=1  operator new[]  status=translated  naming=observed
// naming: ELF dynamic import '_Znaj' (void *_Znaj(unsigned int size)); Ghidra EXTERNAL placeholder 0x8a70218 with body halt_baddata(); real PLT entry 0x0804a5b4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7021c  size=1  pthread_mutexattr_destroy  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutexattr_destroy' (int pthread_mutexattr_destroy(void *attr)); Ghidra EXTERNAL placeholder 0x8a7021c with body halt_baddata(); real PLT entry 0x0804a5c4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70220  size=1  operator delete[]  status=translated  naming=observed
// naming: ELF dynamic import '_ZdaPv' (void _ZdaPv(void *p)); Ghidra EXTERNAL placeholder 0x8a70220 with body halt_baddata(); real PLT entry 0x0804a5d4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70224  size=1  sprintf  status=translated  naming=observed
// naming: ELF dynamic import 'sprintf' (int sprintf(char *str, const char *format, ...)); Ghidra EXTERNAL placeholder 0x8a70224 with body halt_baddata(); real PLT entry 0x0804a5e4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70228  size=1  fwrite  status=translated  naming=observed
// naming: ELF dynamic import 'fwrite' (unsigned int fwrite(const void *ptr, unsigned int size, unsigned int nmemb, void *stream)); Ghidra EXTERNAL placeholder 0x8a70228 with body halt_baddata(); real PLT entry 0x0804a5f4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7022c  size=1  sin  status=translated  naming=observed
// naming: ELF dynamic import 'sin' (double sin(double x)); Ghidra EXTERNAL placeholder 0x8a7022c with body halt_baddata(); real PLT entry 0x0804a604 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70230  size=1  socket  status=translated  naming=observed
// naming: ELF dynamic import 'socket' (int socket(int domain, int type, int protocol)); Ghidra EXTERNAL placeholder 0x8a70230 with body halt_baddata(); real PLT entry 0x0804a614 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70234  size=1  readdir  status=translated  naming=observed
// naming: ELF dynamic import 'readdir' (void *readdir(void *dirp)); Ghidra EXTERNAL placeholder 0x8a70234 with body halt_baddata(); real PLT entry 0x0804a624 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7023c  size=1  rand  status=translated  naming=observed
// naming: ELF dynamic import 'rand' (int rand(void)); Ghidra EXTERNAL placeholder 0x8a7023c with body halt_baddata(); real PLT entry 0x0804a634 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70240  size=1  log  status=translated  naming=observed
// naming: ELF dynamic import 'log' (double log(double x)); Ghidra EXTERNAL placeholder 0x8a70240 with body halt_baddata(); real PLT entry 0x0804a644 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70244  size=1  tcgetattr  status=translated  naming=observed
// naming: ELF dynamic import 'tcgetattr' (int tcgetattr(int fd, void *termios_p)); Ghidra EXTERNAL placeholder 0x8a70244 with body halt_baddata(); real PLT entry 0x0804a654 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70248  size=1  read  status=translated  naming=observed
// naming: ELF dynamic import 'read' (int read(int fd, void *buf, unsigned int n)); Ghidra EXTERNAL placeholder 0x8a70248 with body halt_baddata(); real PLT entry 0x0804a664 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7024c  size=1  geteuid  status=translated  naming=observed
// naming: ELF dynamic import 'geteuid' (unsigned int geteuid(void)); Ghidra EXTERNAL placeholder 0x8a7024c with body halt_baddata(); real PLT entry 0x0804a674 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70250  size=1  __gmon_start__  status=translated  naming=observed
// naming: ELF dynamic-linker symbol 'weak __gmon_start__' (GOT 0x0817d120, R_386_GLOB_DAT); Ghidra EXTERNAL placeholder 0x8a70250 with body halt_baddata(); no PLT entry, import thunk not emitted.

// 0x08a70254  size=1  ceil  status=translated  naming=observed
// naming: ELF dynamic import 'ceil' (double ceil(double x)); Ghidra EXTERNAL placeholder 0x8a70254 with body halt_baddata(); real PLT entry 0x0804a684 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a70258  size=1  dlopen  status=translated  naming=observed
// naming: ELF dynamic import 'dlopen' (void *dlopen(const char *filename, int flags)); Ghidra EXTERNAL placeholder 0x8a70258 with body halt_baddata(); real PLT entry 0x0804a694 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08a7025c  size=1  strcpy  status=translated  naming=observed
// naming: ELF dynamic import 'strcpy' (char *strcpy(char *dest, const char *src)); Ghidra EXTERNAL placeholder 0x8a7025c with body halt_baddata(); real PLT entry 0x0804a6a4 (chunk_000); import thunk, no body emitted (PLT jmp to the imported symbol).
