/*
 * Call of Duty 2 Linux dedicated server 1.3 (cod2_lnxded), chunk 000.
 * Range 0x08049d2c..0x804a704: .init, .plt (PLT0 and 150 import thunks) and CRT
 * startup (_start, call_gmon_start). Binary SHA-256 05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7.
 *
 * The 150 PLT import entries (0x08049d54..0x0804a6a4) are dynamic-linker
 * trampolines: observable behaviour is a tail jump to the imported libc/libstdc++
 * symbol. They carry provenance comments only and no body; reconstructed callers
 * call the imported symbol directly. See chunk_000.json notes.
 */
#include "cod2_types.h"
#include "chunk_000_types.h"

/* Everything below is the C runtime startup (crt1/crti/crtbegin) and the dynamic
 * import PLT of the original binary. The system toolchain provides all of it in the
 * linked build, so it is kept as documentation only; define COD2_RECON_ORIGINAL_CRT
 * to compile it. In the linked build main() (original 0x080d4f7c, Sys_Main in older
 * notes) is the real program entry and is defined by its owning chunk. */
#ifdef COD2_RECON_ORIGINAL_CRT

extern "C" void call_gmon_start(void);

// 0x08049d2c  size=23  _init  status=translated  naming=inferred
// naming: section .init (readelf: .init 0x08049d2c, size 0x17); standard crti.o _init shape (push ebp/sub esp,8, three calls, leave/ret); Ghidra label _DT_INIT
extern "C" void _init(void)
{
    call_gmon_start();
    frame_dummy();
    Crt_RunCtors();
}

// 0x08049d44  size=12  plt0  status=translated  naming=inferred
// naming: section .plt starts at 0x08049d44 with the PLT0 layout: push GOT[1] (0x0817d128), jmp GOT[2] (0x0817d12c); ELF lazy-binding convention; Ghidra auto-label at this address (its pseudocode is an artefact, the disassembly is authoritative)
/* PLT0 lazy-binding trampoline. Reached only by falling from a PLT entry
 * (which pushed its relocation index). Pushes GOT[1] and jumps through GOT[2]
 * (the dynamic linker's resolver). Not callable from C. */
__asm__(".pushsection .plt\n\t"
        "pushl 0x817d128\n\t"
        "jmp *0x817d12c\n\t"
        ".popsection");

// 0x08049d54  size=6  usleep  status=translated  naming=observed
// naming: ELF dynamic import 'usleep': PLT entry 0x08049d54 (jmp via GOT 0x0817d130); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049d64  size=6  mkdir  status=translated  naming=observed
// naming: ELF dynamic import 'mkdir': PLT entry 0x08049d64 (jmp via GOT 0x0817d134); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049d74  size=6  cos  status=translated  naming=observed
// naming: ELF dynamic import 'cos': PLT entry 0x08049d74 (jmp via GOT 0x0817d138); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049d84  size=6  ntohl  status=translated  naming=observed
// naming: ELF dynamic import 'ntohl': PLT entry 0x08049d84 (jmp via GOT 0x0817d13c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049d94  size=6  std::string::operator=  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsaSEPKc': PLT entry 0x08049d94 (jmp via GOT 0x0817d140); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049da4  size=6  rename  status=translated  naming=observed
// naming: ELF dynamic import 'rename': PLT entry 0x08049da4 (jmp via GOT 0x0817d144); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049db4  size=6  std::string::compare  status=translated  naming=observed
// naming: ELF dynamic import '_ZNKSs7compareERKSs': PLT entry 0x08049db4 (jmp via GOT 0x0817d148); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049dc4  size=6  execl  status=translated  naming=observed
// naming: ELF dynamic import 'execl': PLT entry 0x08049dc4 (jmp via GOT 0x0817d14c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049dd4  size=6  strncat  status=translated  naming=observed
// naming: ELF dynamic import 'strncat': PLT entry 0x08049dd4 (jmp via GOT 0x0817d150); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049de4  size=6  vsprintf  status=translated  naming=observed
// naming: ELF dynamic import 'vsprintf': PLT entry 0x08049de4 (jmp via GOT 0x0817d154); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049df4  size=6  strchr  status=translated  naming=observed
// naming: ELF dynamic import 'strchr': PLT entry 0x08049df4 (jmp via GOT 0x0817d158); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e04  size=6  std::allocator<char>::allocator  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSaIcEC1Ev': PLT entry 0x08049e04 (jmp via GOT 0x0817d15c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e14  size=6  atan  status=translated  naming=observed
// naming: ELF dynamic import 'atan': PLT entry 0x08049e14 (jmp via GOT 0x0817d160); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e24  size=6  __cxa_pure_virtual  status=translated  naming=observed
// naming: ELF dynamic import '__cxa_pure_virtual': PLT entry 0x08049e24 (jmp via GOT 0x0817d164); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e34  size=6  pthread_mutexattr_settype  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutexattr_settype': PLT entry 0x08049e34 (jmp via GOT 0x0817d168); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e44  size=6  write  status=translated  naming=observed
// naming: ELF dynamic import 'write': PLT entry 0x08049e44 (jmp via GOT 0x0817d16c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e54  size=6  localtime  status=translated  naming=observed
// naming: ELF dynamic import 'localtime': PLT entry 0x08049e54 (jmp via GOT 0x0817d170); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e64  size=6  recvfrom  status=translated  naming=observed
// naming: ELF dynamic import 'recvfrom': PLT entry 0x08049e64 (jmp via GOT 0x0817d174); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e74  size=6  fileno  status=translated  naming=observed
// naming: ELF dynamic import 'fileno': PLT entry 0x08049e74 (jmp via GOT 0x0817d178); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e84  size=6  strcmp  status=translated  naming=observed
// naming: ELF dynamic import 'strcmp': PLT entry 0x08049e84 (jmp via GOT 0x0817d17c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049e94  size=6  isspace  status=translated  naming=observed
// naming: ELF dynamic import 'isspace': PLT entry 0x08049e94 (jmp via GOT 0x0817d180); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049ea4  size=6  close  status=translated  naming=observed
// naming: ELF dynamic import 'close': PLT entry 0x08049ea4 (jmp via GOT 0x0817d184); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049eb4  size=6  asin  status=translated  naming=observed
// naming: ELF dynamic import 'asin': PLT entry 0x08049eb4 (jmp via GOT 0x0817d188); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049ec4  size=6  dlsym  status=translated  naming=observed
// naming: ELF dynamic import 'dlsym': PLT entry 0x08049ec4 (jmp via GOT 0x0817d18c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049ed4  size=6  fprintf  status=translated  naming=observed
// naming: ELF dynamic import 'fprintf': PLT entry 0x08049ed4 (jmp via GOT 0x0817d190); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049ee4  size=6  fork  status=translated  naming=observed
// naming: ELF dynamic import 'fork': PLT entry 0x08049ee4 (jmp via GOT 0x0817d194); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049ef4  size=6  getenv  status=translated  naming=observed
// naming: ELF dynamic import 'getenv': PLT entry 0x08049ef4 (jmp via GOT 0x0817d198); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f04  size=6  signal  status=translated  naming=observed
// naming: ELF dynamic import 'signal': PLT entry 0x08049f04 (jmp via GOT 0x0817d19c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f14  size=6  std::string::string  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsC1EPKcRKSaIcE': PLT entry 0x08049f14 (jmp via GOT 0x0817d1a0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f24  size=6  fflush  status=translated  naming=observed
// naming: ELF dynamic import 'fflush': PLT entry 0x08049f24 (jmp via GOT 0x0817d1a4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f34  size=6  seteuid  status=translated  naming=observed
// naming: ELF dynamic import 'seteuid': PLT entry 0x08049f34 (jmp via GOT 0x0817d1a8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f44  size=6  unlink  status=translated  naming=observed
// naming: ELF dynamic import 'unlink': PLT entry 0x08049f44 (jmp via GOT 0x0817d1ac); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f54  size=6  select  status=translated  naming=observed
// naming: ELF dynamic import 'select': PLT entry 0x08049f54 (jmp via GOT 0x0817d1b0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f64  size=6  vsnprintf  status=translated  naming=observed
// naming: ELF dynamic import 'vsnprintf': PLT entry 0x08049f64 (jmp via GOT 0x0817d1b4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f74  size=6  strerror  status=translated  naming=observed
// naming: ELF dynamic import 'strerror': PLT entry 0x08049f74 (jmp via GOT 0x0817d1b8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f84  size=6  tcsetattr  status=translated  naming=observed
// naming: ELF dynamic import 'tcsetattr': PLT entry 0x08049f84 (jmp via GOT 0x0817d1bc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049f94  size=6  isalnum  status=translated  naming=observed
// naming: ELF dynamic import 'isalnum': PLT entry 0x08049f94 (jmp via GOT 0x0817d1c0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049fa4  size=6  __cxa_atexit  status=translated  naming=observed
// naming: ELF dynamic import '__cxa_atexit': PLT entry 0x08049fa4 (jmp via GOT 0x0817d1c4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049fb4  size=6  getpwuid  status=translated  naming=observed
// naming: ELF dynamic import 'getpwuid': PLT entry 0x08049fb4 (jmp via GOT 0x0817d1c8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049fc4  size=6  operator.delete  status=translated  naming=observed
// naming: ELF dynamic import '_ZdlPv': PLT entry 0x08049fc4 (jmp via GOT 0x0817d1cc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049fd4  size=6  __errno_location  status=translated  naming=observed
// naming: ELF dynamic import '__errno_location': PLT entry 0x08049fd4 (jmp via GOT 0x0817d1d0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049fe4  size=6  ftell  status=translated  naming=observed
// naming: ELF dynamic import 'ftell': PLT entry 0x08049fe4 (jmp via GOT 0x0817d1d4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x08049ff4  size=6  std::string::c_str  status=translated  naming=observed
// naming: ELF dynamic import '_ZNKSs5c_strEv': PLT entry 0x08049ff4 (jmp via GOT 0x0817d1d8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a004  size=6  system  status=translated  naming=observed
// naming: ELF dynamic import 'system': PLT entry 0x0804a004 (jmp via GOT 0x0817d1dc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a014  size=6  chmod  status=translated  naming=observed
// naming: ELF dynamic import 'chmod': PLT entry 0x0804a014 (jmp via GOT 0x0817d1e0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a024  size=6  tolower  status=translated  naming=observed
// naming: ELF dynamic import 'tolower': PLT entry 0x0804a024 (jmp via GOT 0x0817d1e4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a034  size=6  access  status=translated  naming=observed
// naming: ELF dynamic import 'access': PLT entry 0x0804a034 (jmp via GOT 0x0817d1e8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a044  size=6  std::string::operator=  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsaSERKSs': PLT entry 0x0804a044 (jmp via GOT 0x0817d1ec); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a054  size=6  malloc  status=translated  naming=observed
// naming: ELF dynamic import 'malloc': PLT entry 0x0804a054 (jmp via GOT 0x0817d1f0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a064  size=6  pthread_mutex_init  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutex_init': PLT entry 0x0804a064 (jmp via GOT 0x0817d1f4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a074  size=6  std::string::~string  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsD1Ev': PLT entry 0x0804a074 (jmp via GOT 0x0817d1f8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a084  size=6  fread  status=translated  naming=observed
// naming: ELF dynamic import 'fread': PLT entry 0x0804a084 (jmp via GOT 0x0817d1fc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a094  size=6  memmove  status=translated  naming=observed
// naming: ELF dynamic import 'memmove': PLT entry 0x0804a094 (jmp via GOT 0x0817d200); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a0a4  size=6  std::string::operator[]  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsixEj': PLT entry 0x0804a0a4 (jmp via GOT 0x0817d204); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a0b4  size=6  sendto  status=translated  naming=observed
// naming: ELF dynamic import 'sendto': PLT entry 0x0804a0b4 (jmp via GOT 0x0817d208); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a0c4  size=6  remove  status=translated  naming=observed
// naming: ELF dynamic import 'remove': PLT entry 0x0804a0c4 (jmp via GOT 0x0817d20c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a0d4  size=6  __cxa_rethrow  status=translated  naming=observed
// naming: ELF dynamic import '__cxa_rethrow': PLT entry 0x0804a0d4 (jmp via GOT 0x0817d210); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a0e4  size=6  rint  status=translated  naming=observed
// naming: ELF dynamic import 'rint': PLT entry 0x0804a0e4 (jmp via GOT 0x0817d214); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a0f4  size=6  std::string::string  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsC1Ev': PLT entry 0x0804a0f4 (jmp via GOT 0x0817d218); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a104  size=6  __xstat  status=translated  naming=observed
// naming: ELF dynamic import '__xstat': PLT entry 0x0804a104 (jmp via GOT 0x0817d21c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a114  size=6  setsockopt  status=translated  naming=observed
// naming: ELF dynamic import 'setsockopt': PLT entry 0x0804a114 (jmp via GOT 0x0817d220); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a124  size=6  fseek  status=translated  naming=observed
// naming: ELF dynamic import 'fseek': PLT entry 0x0804a124 (jmp via GOT 0x0817d224); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a134  size=6  time  status=translated  naming=observed
// naming: ELF dynamic import 'time': PLT entry 0x0804a134 (jmp via GOT 0x0817d228); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a144  size=6  pthread_mutexattr_init  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutexattr_init': PLT entry 0x0804a144 (jmp via GOT 0x0817d22c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a154  size=6  pthread_mutex_lock  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutex_lock': PLT entry 0x0804a154 (jmp via GOT 0x0817d230); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a164  size=6  fputs  status=translated  naming=observed
// naming: ELF dynamic import 'fputs': PLT entry 0x0804a164 (jmp via GOT 0x0817d234); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a174  size=6  strstr  status=translated  naming=observed
// naming: ELF dynamic import 'strstr': PLT entry 0x0804a174 (jmp via GOT 0x0817d238); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a184  size=6  strlen  status=translated  naming=observed
// naming: ELF dynamic import 'strlen': PLT entry 0x0804a184 (jmp via GOT 0x0817d23c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a194  size=6  longjmp  status=translated  naming=observed
// naming: ELF dynamic import 'longjmp': PLT entry 0x0804a194 (jmp via GOT 0x0817d240); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a1a4  size=6  sleep  status=translated  naming=observed
// naming: ELF dynamic import 'sleep': PLT entry 0x0804a1a4 (jmp via GOT 0x0817d244); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a1b4  size=6  __cxa_end_catch  status=translated  naming=observed
// naming: ELF dynamic import '__cxa_end_catch': PLT entry 0x0804a1b4 (jmp via GOT 0x0817d248); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a1c4  size=6  qsort  status=translated  naming=observed
// naming: ELF dynamic import 'qsort': PLT entry 0x0804a1c4 (jmp via GOT 0x0817d24c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a1d4  size=6  atof  status=translated  naming=observed
// naming: ELF dynamic import 'atof': PLT entry 0x0804a1d4 (jmp via GOT 0x0817d250); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a1e4  size=6  atan2  status=translated  naming=observed
// naming: ELF dynamic import 'atan2': PLT entry 0x0804a1e4 (jmp via GOT 0x0817d254); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a1f4  size=6  strncmp  status=translated  naming=observed
// naming: ELF dynamic import 'strncmp': PLT entry 0x0804a1f4 (jmp via GOT 0x0817d258); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a204  size=6  inet_addr  status=translated  naming=observed
// naming: ELF dynamic import 'inet_addr': PLT entry 0x0804a204 (jmp via GOT 0x0817d25c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a214  size=6  floor  status=translated  naming=observed
// naming: ELF dynamic import 'floor': PLT entry 0x0804a214 (jmp via GOT 0x0817d260); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a224  size=6  memcmp  status=translated  naming=observed
// naming: ELF dynamic import 'memcmp': PLT entry 0x0804a224 (jmp via GOT 0x0817d264); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a234  size=6  __gxx_personality_v0  status=translated  naming=observed
// naming: ELF dynamic import '__gxx_personality_v0': PLT entry 0x0804a234 (jmp via GOT 0x0817d268); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a244  size=6  std::string::erase  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSs5eraseEjj': PLT entry 0x0804a244 (jmp via GOT 0x0817d26c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a254  size=6  pthread_self  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_self': PLT entry 0x0804a254 (jmp via GOT 0x0817d270); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a264  size=6  std::string::operator+=  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSspLEc': PLT entry 0x0804a264 (jmp via GOT 0x0817d274); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a274  size=6  dlerror  status=translated  naming=observed
// naming: ELF dynamic import 'dlerror': PLT entry 0x0804a274 (jmp via GOT 0x0817d278); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a284  size=6  __libc_start_main  status=translated  naming=observed
// naming: ELF dynamic import '__libc_start_main': PLT entry 0x0804a284 (jmp via GOT 0x0817d27c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a294  size=6  std::string::find  status=translated  naming=observed
// naming: ELF dynamic import '_ZNKSs4findEPKcj': PLT entry 0x0804a294 (jmp via GOT 0x0817d280); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a2a4  size=6  toupper  status=translated  naming=observed
// naming: ELF dynamic import 'toupper': PLT entry 0x0804a2a4 (jmp via GOT 0x0817d284); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a2b4  size=6  sqrtf  status=translated  naming=observed
// naming: ELF dynamic import 'sqrtf': PLT entry 0x0804a2b4 (jmp via GOT 0x0817d288); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a2c4  size=6  realloc  status=translated  naming=observed
// naming: ELF dynamic import 'realloc': PLT entry 0x0804a2c4 (jmp via GOT 0x0817d28c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a2d4  size=6  strcat  status=translated  naming=observed
// naming: ELF dynamic import 'strcat': PLT entry 0x0804a2d4 (jmp via GOT 0x0817d290); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a2e4  size=6  tan  status=translated  naming=observed
// naming: ELF dynamic import 'tan': PLT entry 0x0804a2e4 (jmp via GOT 0x0817d294); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a2f4  size=6  asctime  status=translated  naming=observed
// naming: ELF dynamic import 'asctime': PLT entry 0x0804a2f4 (jmp via GOT 0x0817d298); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a304  size=6  printf  status=translated  naming=observed
// naming: ELF dynamic import 'printf': PLT entry 0x0804a304 (jmp via GOT 0x0817d29c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a314  size=6  bind  status=translated  naming=observed
// naming: ELF dynamic import 'bind': PLT entry 0x0804a314 (jmp via GOT 0x0817d2a0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a324  size=6  getuid  status=translated  naming=observed
// naming: ELF dynamic import 'getuid': PLT entry 0x0804a324 (jmp via GOT 0x0817d2a4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a334  size=6  fcntl  status=translated  naming=observed
// naming: ELF dynamic import 'fcntl': PLT entry 0x0804a334 (jmp via GOT 0x0817d2a8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a344  size=6  memcpy  status=translated  naming=observed
// naming: ELF dynamic import 'memcpy': PLT entry 0x0804a344 (jmp via GOT 0x0817d2ac); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a354  size=6  sqrt  status=translated  naming=observed
// naming: ELF dynamic import 'sqrt': PLT entry 0x0804a354 (jmp via GOT 0x0817d2b0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a364  size=6  std::string::string  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSsC1ERKSs': PLT entry 0x0804a364 (jmp via GOT 0x0817d2b4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a374  size=6  std::allocator<char>::~allocator  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSaIcED1Ev': PLT entry 0x0804a374 (jmp via GOT 0x0817d2b8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a384  size=6  fclose  status=translated  naming=observed
// naming: ELF dynamic import 'fclose': PLT entry 0x0804a384 (jmp via GOT 0x0817d2bc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a394  size=6  strrchr  status=translated  naming=observed
// naming: ELF dynamic import 'strrchr': PLT entry 0x0804a394 (jmp via GOT 0x0817d2c0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a3a4  size=6  ctime  status=translated  naming=observed
// naming: ELF dynamic import 'ctime': PLT entry 0x0804a3a4 (jmp via GOT 0x0817d2c4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a3b4  size=6  isdigit  status=translated  naming=observed
// naming: ELF dynamic import 'isdigit': PLT entry 0x0804a3b4 (jmp via GOT 0x0817d2c8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a3c4  size=6  rmdir  status=translated  naming=observed
// naming: ELF dynamic import 'rmdir': PLT entry 0x0804a3c4 (jmp via GOT 0x0817d2cc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a3d4  size=6  closedir  status=translated  naming=observed
// naming: ELF dynamic import 'closedir': PLT entry 0x0804a3d4 (jmp via GOT 0x0817d2d0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a3e4  size=6  gettimeofday  status=translated  naming=observed
// naming: ELF dynamic import 'gettimeofday': PLT entry 0x0804a3e4 (jmp via GOT 0x0817d2d4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a3f4  size=6  srand  status=translated  naming=observed
// naming: ELF dynamic import 'srand': PLT entry 0x0804a3f4 (jmp via GOT 0x0817d2d8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a404  size=6  opendir  status=translated  naming=observed
// naming: ELF dynamic import 'opendir': PLT entry 0x0804a404 (jmp via GOT 0x0817d2dc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a414  size=6  snprintf  status=translated  naming=observed
// naming: ELF dynamic import 'snprintf': PLT entry 0x0804a414 (jmp via GOT 0x0817d2e0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a424  size=6  gethostname  status=translated  naming=observed
// naming: ELF dynamic import 'gethostname': PLT entry 0x0804a424 (jmp via GOT 0x0817d2e4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a434  size=6  gethostbyname  status=translated  naming=observed
// naming: ELF dynamic import 'gethostbyname': PLT entry 0x0804a434 (jmp via GOT 0x0817d2e8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a444  size=6  strcasecmp  status=translated  naming=observed
// naming: ELF dynamic import 'strcasecmp': PLT entry 0x0804a444 (jmp via GOT 0x0817d2ec); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a454  size=6  dlclose  status=translated  naming=observed
// naming: ELF dynamic import 'dlclose': PLT entry 0x0804a454 (jmp via GOT 0x0817d2f0); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a464  size=6  exit  status=translated  naming=observed
// naming: ELF dynamic import 'exit': PLT entry 0x0804a464 (jmp via GOT 0x0817d2f4); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a474  size=6  isalpha  status=translated  naming=observed
// naming: ELF dynamic import 'isalpha': PLT entry 0x0804a474 (jmp via GOT 0x0817d2f8); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a484  size=6  atoi  status=translated  naming=observed
// naming: ELF dynamic import 'atoi': PLT entry 0x0804a484 (jmp via GOT 0x0817d2fc); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a494  size=6  calloc  status=translated  naming=observed
// naming: ELF dynamic import 'calloc': PLT entry 0x0804a494 (jmp via GOT 0x0817d300); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a4a4  size=6  sscanf  status=translated  naming=observed
// naming: ELF dynamic import 'sscanf': PLT entry 0x0804a4a4 (jmp via GOT 0x0817d304); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a4b4  size=6  free  status=translated  naming=observed
// naming: ELF dynamic import 'free': PLT entry 0x0804a4b4 (jmp via GOT 0x0817d308); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a4c4  size=6  ioctl  status=translated  naming=observed
// naming: ELF dynamic import 'ioctl': PLT entry 0x0804a4c4 (jmp via GOT 0x0817d30c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a4d4  size=6  htons  status=translated  naming=observed
// naming: ELF dynamic import 'htons': PLT entry 0x0804a4d4 (jmp via GOT 0x0817d310); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a4e4  size=6  getcwd  status=translated  naming=observed
// naming: ELF dynamic import 'getcwd': PLT entry 0x0804a4e4 (jmp via GOT 0x0817d314); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a4f4  size=6  __cxa_begin_catch  status=translated  naming=observed
// naming: ELF dynamic import '__cxa_begin_catch': PLT entry 0x0804a4f4 (jmp via GOT 0x0817d318); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a504  size=6  isatty  status=translated  naming=observed
// naming: ELF dynamic import 'isatty': PLT entry 0x0804a504 (jmp via GOT 0x0817d31c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a514  size=6  acos  status=translated  naming=observed
// naming: ELF dynamic import 'acos': PLT entry 0x0804a514 (jmp via GOT 0x0817d320); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a524  size=6  memset  status=translated  naming=observed
// naming: ELF dynamic import 'memset': PLT entry 0x0804a524 (jmp via GOT 0x0817d324); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a534  size=6  _exit  status=translated  naming=observed
// naming: ELF dynamic import '_exit': PLT entry 0x0804a534 (jmp via GOT 0x0817d328); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a544  size=6  strncasecmp  status=translated  naming=observed
// naming: ELF dynamic import 'strncasecmp': PLT entry 0x0804a544 (jmp via GOT 0x0817d32c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a554  size=6  strncpy  status=translated  naming=observed
// naming: ELF dynamic import 'strncpy': PLT entry 0x0804a554 (jmp via GOT 0x0817d330); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a564  size=6  std::string::operator+=  status=translated  naming=observed
// naming: ELF dynamic import '_ZNSspLEPKc': PLT entry 0x0804a564 (jmp via GOT 0x0817d334); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a574  size=6  fopen  status=translated  naming=observed
// naming: ELF dynamic import 'fopen': PLT entry 0x0804a574 (jmp via GOT 0x0817d338); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a584  size=6  _setjmp  status=translated  naming=observed
// naming: ELF dynamic import '_setjmp': PLT entry 0x0804a584 (jmp via GOT 0x0817d33c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a594  size=6  _Unwind_Resume  status=translated  naming=observed
// naming: ELF dynamic import '_Unwind_Resume': PLT entry 0x0804a594 (jmp via GOT 0x0817d340); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a5a4  size=6  pthread_mutex_unlock  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutex_unlock': PLT entry 0x0804a5a4 (jmp via GOT 0x0817d344); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a5b4  size=6  operator.new[]  status=translated  naming=observed
// naming: ELF dynamic import '_Znaj': PLT entry 0x0804a5b4 (jmp via GOT 0x0817d348); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a5c4  size=6  pthread_mutexattr_destroy  status=translated  naming=observed
// naming: ELF dynamic import 'pthread_mutexattr_destroy': PLT entry 0x0804a5c4 (jmp via GOT 0x0817d34c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a5d4  size=6  operator.delete[]  status=translated  naming=observed
// naming: ELF dynamic import '_ZdaPv': PLT entry 0x0804a5d4 (jmp via GOT 0x0817d350); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a5e4  size=6  sprintf  status=translated  naming=observed
// naming: ELF dynamic import 'sprintf': PLT entry 0x0804a5e4 (jmp via GOT 0x0817d354); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a5f4  size=6  fwrite  status=translated  naming=observed
// naming: ELF dynamic import 'fwrite': PLT entry 0x0804a5f4 (jmp via GOT 0x0817d358); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a604  size=6  sin  status=translated  naming=observed
// naming: ELF dynamic import 'sin': PLT entry 0x0804a604 (jmp via GOT 0x0817d35c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a614  size=6  socket  status=translated  naming=observed
// naming: ELF dynamic import 'socket': PLT entry 0x0804a614 (jmp via GOT 0x0817d360); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a624  size=6  readdir  status=translated  naming=observed
// naming: ELF dynamic import 'readdir': PLT entry 0x0804a624 (jmp via GOT 0x0817d364); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a634  size=6  rand  status=translated  naming=observed
// naming: ELF dynamic import 'rand': PLT entry 0x0804a634 (jmp via GOT 0x0817d368); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a644  size=6  log  status=translated  naming=observed
// naming: ELF dynamic import 'log': PLT entry 0x0804a644 (jmp via GOT 0x0817d36c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a654  size=6  tcgetattr  status=translated  naming=observed
// naming: ELF dynamic import 'tcgetattr': PLT entry 0x0804a654 (jmp via GOT 0x0817d370); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a664  size=6  read  status=translated  naming=observed
// naming: ELF dynamic import 'read': PLT entry 0x0804a664 (jmp via GOT 0x0817d374); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a674  size=6  geteuid  status=translated  naming=observed
// naming: ELF dynamic import 'geteuid': PLT entry 0x0804a674 (jmp via GOT 0x0817d378); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a684  size=6  ceil  status=translated  naming=observed
// naming: ELF dynamic import 'ceil': PLT entry 0x0804a684 (jmp via GOT 0x0817d37c); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a694  size=6  dlopen  status=translated  naming=observed
// naming: ELF dynamic import 'dlopen': PLT entry 0x0804a694 (jmp via GOT 0x0817d380); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a6a4  size=6  strcpy  status=translated  naming=observed
// naming: ELF dynamic import 'strcpy': PLT entry 0x0804a6a4 (jmp via GOT 0x0817d384); import thunk, no body emitted (PLT jmp to the imported symbol).

// 0x0804a6c0  size=34  _start  status=translated  naming=inferred
// naming: ELF e_entry = 0x0804a6c0 (readelf -h); body pops argc, computes argv, aligns esp, pushes stack_end/rtld_fini/fini/init/argv/argc/main and calls __libc_start_main (crt1 convention); Ghidra label 'entry'
/* ELF entry point. The kernel enters with argc at (%esp). */
__asm__(".pushsection .text\n\t"
        ".globl _start\n"
        "_start:\n\t"
        "xorl %ebp, %ebp\n\t"
        "popl %esi\n\t"              /* argc */
        "movl %esp, %ecx\n\t"        /* argv */
        "andl $-16, %esp\n\t"
        "pushl %eax\n\t"             /* alignment padding */
        "pushl %esp\n\t"             /* stack_end */
        "pushl %edx\n\t"             /* rtld_fini (edx from the kernel) */
        "pushl $Crt_RunFini\n\t"     /* fini */
        "pushl $libc_csu_init\n\t"     /* init */
        "pushl %ecx\n\t"             /* argv */
        "pushl %esi\n\t"             /* argc */
        "pushl $main\n\t"     /* main */
        "call __libc_start_main\n\t"
        "hlt\n\t"
        ".popsection");

// 0x0804a6e4  size=33  call_gmon_start  status=translated  naming=inferred
// naming: crtbegin.o call_gmon_start shape: saves ebx, loads GOT slot 0x0817d120 for weak __gmon_start__, calls it only if non-null; Ghidra auto-label at this address (its unconditional call in pseudocode is an artefact; disassembly shows the null test)
extern "C" void __gmon_start__(void) __attribute__((weak));

extern "C" void call_gmon_start(void)
{
    if (__gmon_start__ != 0) {
        __gmon_start__();
    }
}

#endif /* COD2_RECON_ORIGINAL_CRT */
