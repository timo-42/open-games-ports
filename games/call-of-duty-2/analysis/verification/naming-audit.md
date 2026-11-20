# Naming audit

## Method

ELF .rodata/.data printable NUL-terminated ASCII; observed name substring match, direct reference in pseudocode or bounded objdump disassembly; all checks are literal and heuristic.

The script parses the ELF section table directly. A direct address or Ghidra-style `s_...` label in pseudocode counts as a reference; otherwise the function range from the pseudocode header is searched with `objdump -d`. Missing pseudocode uses a 0x100-byte fallback window. Each count column shows its rate of all functions in that group; PASS rate is also shown among observed functions.

## Per-model

| Model | Functions | PASS (n, total rate) | WEAK (n, total rate) | FAIL (n, total rate) | PASS rate of observed | Inferred (n, rate) | Unknown (n, rate) | Under-claimed | Placeholders (n, rate) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| haiku | 2441 | 241 (9.9%) | 28 (1.1%) | 103 (4.2%) | 64.8% | 1304 (53.4%) | 765 (31.3%) | 9 | 0 (0.0%) |
| sonnet | 2848 | 106 (3.7%) | 38 (1.3%) | 167 (5.9%) | 34.1% | 1850 (65.0%) | 687 (24.1%) | 7 | 2 (0.1%) |

## Per chunk

| Chunk | Model | Functions | PASS (n, rate) | WEAK (n, rate) | FAIL (n, rate) | PASS rate observed | Inferred (n, rate) | Unknown (n, rate) | Under-claimed | Placeholders (n, rate) |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | haiku | 154 | 137 (89.0%) | 0 (0.0%) | 13 (8.4%) | 91.3% | 4 (2.6%) | 0 (0.0%) | 0 | 0 (0.0%) |
| 1 | sonnet | 42 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 42 (100.0%) | 0 (0.0%) | 0 | 0 (0.0%) |
| 2 | sonnet | 68 | 0 (0.0%) | 1 (1.5%) | 0 (0.0%) | 0.0% | 54 (79.4%) | 13 (19.1%) | 0 | 0 (0.0%) |
| 3 | sonnet | 69 | 17 (24.6%) | 3 (4.3%) | 0 (0.0%) | 85.0% | 36 (52.2%) | 13 (18.8%) | 0 | 0 (0.0%) |
| 4 | haiku | 65 | 0 (0.0%) | 0 (0.0%) | 1 (1.5%) | 0.0% | 6 (9.2%) | 58 (89.2%) | 0 | 0 (0.0%) |
| 5 | haiku | 53 | 0 (0.0%) | 0 (0.0%) | 1 (1.9%) | 0.0% | 23 (43.4%) | 29 (54.7%) | 1 | 0 (0.0%) |
| 6 | sonnet | 87 | 4 (4.6%) | 2 (2.3%) | 4 (4.6%) | 40.0% | 38 (43.7%) | 39 (44.8%) | 1 | 0 (0.0%) |
| 7 | sonnet | 80 | 3 (3.8%) | 0 (0.0%) | 13 (16.2%) | 18.8% | 56 (70.0%) | 8 (10.0%) | 0 | 0 (0.0%) |
| 8 | haiku | 77 | 1 (1.3%) | 0 (0.0%) | 0 (0.0%) | 100.0% | 71 (92.2%) | 5 (6.5%) | 0 | 0 (0.0%) |
| 9 | haiku | 59 | 3 (5.1%) | 0 (0.0%) | 5 (8.5%) | 37.5% | 37 (62.7%) | 14 (23.7%) | 4 | 0 (0.0%) |
| 10 | sonnet | 97 | 0 (0.0%) | 0 (0.0%) | 9 (9.3%) | 0.0% | 38 (39.2%) | 50 (51.5%) | 0 | 0 (0.0%) |
| 11 | sonnet | 61 | 0 (0.0%) | 0 (0.0%) | 10 (16.4%) | 0.0% | 26 (42.6%) | 25 (41.0%) | 0 | 0 (0.0%) |
| 12 | haiku | 73 | 0 (0.0%) | 0 (0.0%) | 1 (1.4%) | 0.0% | 27 (37.0%) | 45 (61.6%) | 0 | 0 (0.0%) |
| 13 | haiku | 88 | 2 (2.3%) | 0 (0.0%) | 3 (3.4%) | 40.0% | 70 (79.5%) | 13 (14.8%) | 0 | 0 (0.0%) |
| 14 | sonnet | 105 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 86 (81.9%) | 19 (18.1%) | 0 | 0 (0.0%) |
| 15 | sonnet | 73 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 45 (61.6%) | 28 (38.4%) | 0 | 0 (0.0%) |
| 16 | sonnet | 17 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 13 (76.5%) | 4 (23.5%) | 0 | 0 (0.0%) |
| 17 | haiku | 81 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 27 (33.3%) | 54 (66.7%) | 0 | 0 (0.0%) |
| 18 | sonnet | 17 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 3 (17.6%) | 14 (82.4%) | 0 | 2 (11.8%) |
| 19 | sonnet | 55 | 4 (7.3%) | 0 (0.0%) | 5 (9.1%) | 44.4% | 42 (76.4%) | 4 (7.3%) | 0 | 0 (0.0%) |
| 20 | haiku | 55 | 3 (5.5%) | 2 (3.6%) | 11 (20.0%) | 18.8% | 31 (56.4%) | 8 (14.5%) | 0 | 0 (0.0%) |
| 21 | haiku | 71 | 1 (1.4%) | 6 (8.5%) | 6 (8.5%) | 7.7% | 32 (45.1%) | 26 (36.6%) | 0 | 0 (0.0%) |
| 22 | sonnet | 41 | 0 (0.0%) | 2 (4.9%) | 0 (0.0%) | 0.0% | 25 (61.0%) | 14 (34.1%) | 0 | 0 (0.0%) |
| 23 | sonnet | 51 | 0 (0.0%) | 4 (7.8%) | 0 (0.0%) | 0.0% | 36 (70.6%) | 11 (21.6%) | 0 | 0 (0.0%) |
| 24 | haiku | 54 | 3 (5.6%) | 4 (7.4%) | 1 (1.9%) | 37.5% | 32 (59.3%) | 14 (25.9%) | 0 | 0 (0.0%) |
| 25 | haiku | 86 | 1 (1.2%) | 1 (1.2%) | 1 (1.2%) | 33.3% | 64 (74.4%) | 19 (22.1%) | 0 | 0 (0.0%) |
| 26 | sonnet | 105 | 0 (0.0%) | 0 (0.0%) | 2 (1.9%) | 0.0% | 82 (78.1%) | 21 (20.0%) | 0 | 0 (0.0%) |
| 27 | sonnet | 85 | 1 (1.2%) | 7 (8.2%) | 12 (14.1%) | 5.0% | 55 (64.7%) | 10 (11.8%) | 3 | 0 (0.0%) |
| 28 | haiku | 58 | 2 (3.4%) | 3 (5.2%) | 0 (0.0%) | 40.0% | 50 (86.2%) | 3 (5.2%) | 0 | 0 (0.0%) |
| 29 | haiku | 104 | 0 (0.0%) | 1 (1.0%) | 6 (5.8%) | 0.0% | 79 (76.0%) | 18 (17.3%) | 0 | 0 (0.0%) |
| 30 | sonnet | 86 | 0 (0.0%) | 7 (8.1%) | 3 (3.5%) | 0.0% | 67 (77.9%) | 9 (10.5%) | 0 | 0 (0.0%) |
| 31 | sonnet | 84 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 47 (56.0%) | 37 (44.0%) | 0 | 0 (0.0%) |
| 32 | haiku | 65 | 0 (0.0%) | 0 (0.0%) | 1 (1.5%) | 0.0% | 10 (15.4%) | 54 (83.1%) | 0 | 0 (0.0%) |
| 33 | haiku | 88 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 84 (95.5%) | 4 (4.5%) | 0 | 0 (0.0%) |
| 34 | sonnet | 75 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 36 (48.0%) | 39 (52.0%) | 0 | 0 (0.0%) |
| 35 | sonnet | 35 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 31 (88.6%) | 4 (11.4%) | 2 | 0 (0.0%) |
| 36 | sonnet | 26 | 0 (0.0%) | 1 (3.8%) | 4 (15.4%) | 0.0% | 21 (80.8%) | 0 (0.0%) | 0 | 0 (0.0%) |
| 37 | haiku | 53 | 0 (0.0%) | 0 (0.0%) | 1 (1.9%) | 0.0% | 47 (88.7%) | 5 (9.4%) | 0 | 0 (0.0%) |
| 38 | sonnet | 94 | 5 (5.3%) | 0 (0.0%) | 7 (7.4%) | 41.7% | 50 (53.2%) | 32 (34.0%) | 0 | 0 (0.0%) |
| 39 | sonnet | 55 | 5 (9.1%) | 5 (9.1%) | 0 (0.0%) | 50.0% | 23 (41.8%) | 22 (40.0%) | 0 | 0 (0.0%) |
| 40 | haiku | 80 | 0 (0.0%) | 3 (3.8%) | 8 (10.0%) | 0.0% | 38 (47.5%) | 31 (38.8%) | 0 | 0 (0.0%) |
| 41 | haiku | 53 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 35 (66.0%) | 18 (34.0%) | 0 | 0 (0.0%) |
| 42 | sonnet | 38 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 27 (71.1%) | 11 (28.9%) | 0 | 0 (0.0%) |
| 43 | sonnet | 86 | 0 (0.0%) | 1 (1.2%) | 0 (0.0%) | 0.0% | 49 (57.0%) | 36 (41.9%) | 0 | 0 (0.0%) |
| 44 | haiku | 59 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 15 (25.4%) | 44 (74.6%) | 0 | 0 (0.0%) |
| 45 | haiku | 70 | 0 (0.0%) | 1 (1.4%) | 1 (1.4%) | 0.0% | 66 (94.3%) | 2 (2.9%) | 0 | 0 (0.0%) |
| 46 | sonnet | 56 | 0 (0.0%) | 1 (1.8%) | 6 (10.7%) | 0.0% | 39 (69.6%) | 10 (17.9%) | 0 | 0 (0.0%) |
| 47 | sonnet | 62 | 0 (0.0%) | 1 (1.6%) | 12 (19.4%) | 0.0% | 37 (59.7%) | 12 (19.4%) | 0 | 0 (0.0%) |
| 48 | haiku | 65 | 0 (0.0%) | 0 (0.0%) | 2 (3.1%) | 0.0% | 36 (55.4%) | 27 (41.5%) | 0 | 0 (0.0%) |
| 49 | haiku | 59 | 0 (0.0%) | 2 (3.4%) | 7 (11.9%) | 0.0% | 21 (35.6%) | 29 (49.2%) | 1 | 0 (0.0%) |
| 50 | sonnet | 83 | 0 (0.0%) | 0 (0.0%) | 38 (45.8%) | 0.0% | 42 (50.6%) | 3 (3.6%) | 0 | 0 (0.0%) |
| 51 | sonnet | 92 | 2 (2.2%) | 1 (1.1%) | 12 (13.0%) | 13.3% | 66 (71.7%) | 11 (12.0%) | 0 | 0 (0.0%) |
| 52 | haiku | 92 | 0 (0.0%) | 2 (2.2%) | 3 (3.3%) | 0.0% | 5 (5.4%) | 82 (89.1%) | 2 | 0 (0.0%) |
| 53 | haiku | 82 | 1 (1.2%) | 0 (0.0%) | 0 (0.0%) | 100.0% | 66 (80.5%) | 15 (18.3%) | 1 | 0 (0.0%) |
| 54 | sonnet | 105 | 0 (0.0%) | 0 (0.0%) | 1 (1.0%) | 0.0% | 93 (88.6%) | 11 (10.5%) | 0 | 0 (0.0%) |
| 55 | sonnet | 131 | 0 (0.0%) | 1 (0.8%) | 1 (0.8%) | 0.0% | 115 (87.8%) | 14 (10.7%) | 0 | 0 (0.0%) |
| 56 | haiku | 100 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 89 (89.0%) | 11 (11.0%) | 0 | 0 (0.0%) |
| 57 | sonnet | 75 | 5 (6.7%) | 1 (1.3%) | 12 (16.0%) | 27.8% | 47 (62.7%) | 10 (13.3%) | 1 | 0 (0.0%) |
| 58 | sonnet | 83 | 1 (1.2%) | 0 (0.0%) | 2 (2.4%) | 33.3% | 78 (94.0%) | 2 (2.4%) | 0 | 0 (0.0%) |
| 59 | sonnet | 115 | 0 (0.0%) | 0 (0.0%) | 5 (4.3%) | 0.0% | 25 (21.7%) | 85 (73.9%) | 0 | 0 (0.0%) |
| 60 | haiku | 100 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 100 (100.0%) | 0 (0.0%) | 0 | 0 (0.0%) |
| 61 | sonnet | 116 | 54 (46.6%) | 0 (0.0%) | 9 (7.8%) | 85.7% | 44 (37.9%) | 9 (7.8%) | 0 | 0 (0.0%) |
| 62 | haiku | 89 | 84 (94.4%) | 0 (0.0%) | 5 (5.6%) | 94.4% | 0 (0.0%) | 0 (0.0%) | 0 | 0 (0.0%) |
| 100 | haiku | 67 | 0 (0.0%) | 2 (3.0%) | 0 (0.0%) | 0.0% | 58 (86.6%) | 7 (10.4%) | 0 | 0 (0.0%) |
| 101 | sonnet | 77 | 4 (5.2%) | 0 (0.0%) | 0 (0.0%) | 100.0% | 44 (57.1%) | 29 (37.7%) | 0 | 0 (0.0%) |
| 102 | haiku | 80 | 3 (3.8%) | 0 (0.0%) | 0 (0.0%) | 100.0% | 13 (16.2%) | 64 (80.0%) | 0 | 0 (0.0%) |
| 103 | sonnet | 48 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 48 (100.0%) | 0 (0.0%) | 0 | 0 (0.0%) |
| 104 | haiku | 89 | 0 (0.0%) | 1 (1.1%) | 5 (5.6%) | 0.0% | 67 (75.3%) | 16 (18.0%) | 0 | 0 (0.0%) |
| 105 | sonnet | 86 | 0 (0.0%) | 0 (0.0%) | 0 (0.0%) | n/a | 86 (100.0%) | 0 (0.0%) | 0 | 0 (0.0%) |
| 106 | haiku | 72 | 0 (0.0%) | 0 (0.0%) | 21 (29.2%) | 0.0% | 1 (1.4%) | 50 (69.4%) | 0 | 0 (0.0%) |
| 107 | sonnet | 87 | 1 (1.1%) | 0 (0.0%) | 0 (0.0%) | 100.0% | 58 (66.7%) | 28 (32.2%) | 0 | 0 (0.0%) |

## FAIL items

- `0x08049d94` `std::string::operator=` (chunk 0): ELF dynamic import '_ZNSsaSEPKc': PLT entry 0x08049d94 (jmp via GOT 0x0817d140); no matching binary string.
- `0x08049db4` `std::string::compare` (chunk 0): ELF dynamic import '_ZNKSs7compareERKSs': PLT entry 0x08049db4 (jmp via GOT 0x0817d148); no matching binary string.
- `0x08049f14` `std::string::string` (chunk 0): ELF dynamic import '_ZNSsC1EPKcRKSaIcE': PLT entry 0x08049f14 (jmp via GOT 0x0817d1a0); no matching binary string.
- `0x08049ff4` `std::string::c_str` (chunk 0): ELF dynamic import '_ZNKSs5c_strEv': PLT entry 0x08049ff4 (jmp via GOT 0x0817d1d8); no matching binary string.
- `0x0804a044` `std::string::operator=` (chunk 0): ELF dynamic import '_ZNSsaSERKSs': PLT entry 0x0804a044 (jmp via GOT 0x0817d1ec); no matching binary string.
- `0x0804a074` `std::string::~string` (chunk 0): ELF dynamic import '_ZNSsD1Ev': PLT entry 0x0804a074 (jmp via GOT 0x0817d1f8); no matching binary string.
- `0x0804a0a4` `std::string::operator[]` (chunk 0): ELF dynamic import '_ZNSsixEj': PLT entry 0x0804a0a4 (jmp via GOT 0x0817d204); no matching binary string.
- `0x0804a0f4` `std::string::string` (chunk 0): ELF dynamic import '_ZNSsC1Ev': PLT entry 0x0804a0f4 (jmp via GOT 0x0817d218); no matching binary string.
- `0x0804a244` `std::string::erase` (chunk 0): ELF dynamic import '_ZNSs5eraseEjj': PLT entry 0x0804a244 (jmp via GOT 0x0817d26c); no matching binary string.
- `0x0804a264` `std::string::operator+=` (chunk 0): ELF dynamic import '_ZNSspLEc': PLT entry 0x0804a264 (jmp via GOT 0x0817d274); no matching binary string.
- `0x0804a294` `std::string::find` (chunk 0): ELF dynamic import '_ZNKSs4findEPKcj': PLT entry 0x0804a294 (jmp via GOT 0x0817d280); no matching binary string.
- `0x0804a364` `std::string::string` (chunk 0): ELF dynamic import '_ZNSsC1ERKSs': PLT entry 0x0804a364 (jmp via GOT 0x0817d2b4); no matching binary string.
- `0x0804a564` `std::string::operator+=` (chunk 0): ELF dynamic import '_ZNSspLEPKc': PLT entry 0x0804a564 (jmp via GOT 0x0817d334); no matching binary string.
- `0x080585fe` `cm_load_static_models` (chunk 4): string "CM_CreateStaticModel" (alloc tag, line 0x17); misc_model entity parse; Ghidra FUN_080585fe, size 1254; no matching binary string.
- `0x0805e76e` `CM_AreaEntities_r` (chunk 5): string 'CM_AreaEntities: MAXCOUNT\n' at 0x805e76e; no matching binary string.
- `0x080600f6` `Cmd_Vstr_f` (chunk 6): Strings 'vstr <variablename> : execute a variable command\n', "%s doesn't exist\n", '%s is not a string-based dvar\n'; registered as 'vstr' by Cmd_Init.; no matching binary string.
- `0x080609e0` `Cmd_List_f` (chunk 6): String '%i commands\n' at 0x0814046c; registered as 'cmdlist' by Cmd_Init. Optional filter in arg 1.; no matching binary string.
- `0x08060f44` `Com_RegisterErrorDvars` (chunk 6): Strings 'com_errorMessage' and 'com_errorTitle': registers both string dvars (default "", flags 0x1040) and sets com_errorMessage to msg.; no matching binary string.
- `0x08061124` `Com_Error` (chunk 6): String 'recursive error after: %s'. Q3 Com_Error: formats into the 0x1000-byte error buffer, maps error codes 4, 5 and 6 to 1, stores the code and longjmps to the handler registered in slot 2. Never returns.; no matching binary string.
- `0x08061ccc` `Com_Crash_f` (chunk 7): Registered as the "crash" console command (Com_Init, developer only); writes to address 0.; no matching binary string.
- `0x08061d90` `Com_InitDvars` (chunk 7): Registers the common dvars by name ("dedicated", "com_maxfps", ... "com_animCheck").; no matching binary string.
- `0x080633b0` `Dvar_Toggle_f` (chunk 7): "toggle" command: "USAGE: %s <variable> <optional value sequence>" / "toggle failed: dvar '%s' not found."; no matching binary string.
- `0x080635b0` `Cmd_Set_f` (chunk 7): "set" command: "USAGE: set <variable> <value>", "invalid variable name: %s".; no matching binary string.
- `0x08063a3c` `Cmd_SetA_f` (chunk 7): "seta" command: runs Cmd_Set_f then adds flag 1 (archive) to the dvar.; no matching binary string.
- `0x08063e2c` `Dvar_Dump` (chunk 7): Dvar dump ("=== DVAR DUMP ==="), optional name filter from Cmd_Argv(1), to file/console handle.; no matching binary string.
- `0x08063fe8` `Dvar_Dump_f` (chunk 7): "dvardump" command handler.; no matching binary string.
- `0x0806424c` `Dvar_AddCommands` (chunk 7): Registers dvar console commands ("toggle", "togglep", "set", "sets", "seta", "setfromdvar",; no matching binary string.
- `0x080650d4` `FS_Dir_f` (chunk 7): "dir" command: "usage: dir <directory> [extension]", "Directory of %s %s".; no matching binary string.
- `0x080651e0` `FS_NewDir_f` (chunk 7): "fdir" command: "usage: fdir <filter>", "%d files listed".; no matching binary string.
- `0x080652f0` `FS_TouchFile_f` (chunk 7): "touchFile" command: "Usage: touchFile <file>".; no matching binary string.
- `0x080657dc` `FS_AddCommands` (chunk 7): Registers "path", "fullpath", "dir", "fdir", "touchFile".; no matching binary string.
- `0x08065d9a` `FS_PureServerSetLoadedIwds` (chunk 7): FS_PureServerSetLoadedIwds: string "iwd sum/name mismatch", "Connected to a pure server.\n".; no matching binary string.
- `0x0806b892` `NET_DumpProfile_f` (chunk 9): naming: observed. Registered as command "net_dumpprofile" in NET_InitCvars; message text in rodata.; no matching binary string.
- `0x0806b8be` `NET_InitCvars` (chunk 9): naming: observed. Registers cvars showpackets, showdrop, packetDebug, net_profile, net_showprofile, net_lanauthorize and command net_dumpprofile (string evidence, rodata).; no matching binary string.
- `0x0806cc7a` `NET_OutOfBandVoice` (chunk 9): naming: observed. Evidence: "OOB Voice Packet is %i bytes - too large to send" (rodata).; no matching binary string.
- `0x0806d0dc` `Anim_AddNodeToCurrentTree` (chunk 9): naming: observed. Error text "#using_animtree was not specified" (rodata).; no matching binary string.
- `0x0806d924` `Anim_ReportUnresolved` (chunk 9): naming: observed. Error text "animation '%s' not defined in anim tree '%s'" (rodata).; no matching binary string.
- `0x0806e8fc` `EmitGetAnimTree` (chunk 10): string "#using_animtree was not specified"; otherwise emits the registered tree index as an integer; no matching binary string.
- `0x0806e9b2` `Compile_LookupLocalVar` (chunk 10): strings "uninitialised variable '%s'" and "unreachable code"; returns stack offset of the local from the top; no matching binary string.
- `0x0806ec34` `Compile_CheckSwitchCaseCount` (chunk 10): string "MAX_SWITCH_CASES exceeded"; no matching binary string.
- `0x0806ec5c` `Compile_CheckLocalVarCount` (chunk 10): string "LOCAL_VAR_STACK_SIZE exceeded"; no matching binary string.
- `0x0806f50e` `PushDeferredValue` (chunk 10): string "VALUE_STACK_SIZE exceeded"; no matching binary string.
- `0x0806fbd4` `EmitDebuggerVariable` (chunk 10): strings "bad expression" and "$ can only be used in the script debugger"; parses a $tNNN or $<letter>NNN token; no matching binary string.
- `0x0806fe86` `EmitAssignTarget` (chunk 10): strings "self field in assignment expression not currently supported" / "self field can only be used in the script debugger"; dispatch on node->type; no matching binary string.
- `0x08070124` `EmitFunctionRef` (chunk 10): strings "unknown function" (0x8143ad3) and "normal script cannot reference a function in a /# ... #/ comment"; resolves a function name (optionally file::func), emits its code position and records a fixup; no matching binary string.
- `0x080704c6` `Scr_FindOrAddFuncTableEntry` (chunk 10): string "SCR_FUNC_TABLE_SIZE exceeded" (prefixed by a 0x15 byte in rodata); linear search in a 0x400-entry table, appending on miss; no matching binary string.
- `0x080711ce` `ScrCompile_DefineFunction` (chunk 11): strings "function '%s' already defined" and "... already defined in '%s'" (duplicate function definition check); no matching binary string.
- `0x08071608` `ScrCompile_VectorExpr` (chunk 11): string "expression list must have 1 or 3 parameters"; no matching binary string.
- `0x08071772` `ScrCompile_ObjectFromList` (chunk 11): string "not an object"; no matching binary string.
- `0x080721e8` `ScrCompile_StoreLvalue` (chunk 11): strings "not an lvalue" and "$ and self field can only be used in the script debugger"; no matching binary string.
- `0x08072352` `ScrCompile_StoreLvalueChecked` (chunk 11): string "not an lvalue"; no matching binary string.
- `0x080723da` `ScrCompile_ObjectExpr` (chunk 11): string "not an object"; no matching binary string.
- `0x080724b2` `ScrCompile_ObjectRefNode` (chunk 11): string "not an object"; no matching binary string.
- `0x080725ce` `ScrCompile_TryFastLvalue` (chunk 11): strings "not an lvalue" and "$ and self field can only be used in the script debugger"; returns 0 for lvalue kind 4, else 1; no matching binary string.
- `0x08072da4` `ScrCompile_WhileLoop` (chunk 11): string "conditional expression cannot be always false" (loop condition folding); while-loop emission with break/continue patch lists; no matching binary string.
- `0x080732c6` `ScrCompile_ForLoop` (chunk 11): string "conditional expression cannot be always false"; for-loop emission (init, condition, body, iterator, back-jump 99); no matching binary string.
- `0x08076680` `Mem_PrintBucketStats` (chunk 12): strings "%d subtree has %d * %d = %d free buckets", "total memory alloc buckets: %d (%d instances)".; no matching binary string.
- `0x08076db6` `MT_AllocFail` (chunk 13): format string "%s: failed memory allocation of %d bytes for script usage" at 0x08076db6 callsite; "failed memory allocation for script usage" string.; no matching binary string.
- `0x080787dc` `ScrRuntime_ReportTrace` (chunk 13): string "script runtime error" and the "called from:" / "started from:" labels (observed).; no matching binary string.
- `0x080788d2` `ScrRuntime_Error` (chunk 13): string "\025script runtime error\n(see console for details)" (observed); reports a runtime error through the trace or message path by the scr_error_* flags.; no matching binary string.
- `0x8089308` `Lex_StoreEscapedString` (chunk 19): String "max string length exceeded: \"%s\"" is printed here; backslash escapes n/r/t decoded, other escapes pass the char.; no matching binary string.
- `0x808b9dd` `Parse_ReportSyntaxError` (chunk 19): Strings 'unexpected end of file found' and 'bad syntax'; token 0x102 is the lexer's error token. Always returns 0.; no matching binary string.
- `0x808c0a8` `SV_MapRestart_cmd` (chunk 19): "map_restart" command callback (registered in SV_AddOperatorCommands): SV_MapRestart_f(0).; no matching binary string.
- `0x808c0bc` `SV_FastRestart_cmd` (chunk 19): "fast_restart" command callback: SV_MapRestart_f(1).; no matching binary string.
- `0x808c132` `SV_MapRotate_f` (chunk 19): Strings 'map_rotate...' and the sv_mapRotation / sv_mapRotationCurrent dumps; command "map_rotate".; no matching binary string.
- `0x0808d75a` `SV_BanClient` (chunk 20): FUN_0808d75a (299 bytes): strings "Can't ban user, GUID is 0", "This GUID (%i) is already banned",; no matching binary string.
- `0x0808d886` `SV_UnbanUserByName` (chunk 20): FUN_0808d886 (420 bytes): strings "unbanned %i user(s) named %s", "no banned user has name %s",; no matching binary string.
- `0x0808f02e` `SV_DropClient` (chunk 20): FUN_0808f02e (650 bytes): strings "Going to CS_ZOMBIE for %s\n", "EXE_DISCONNECTED", "%i:%s %s\n".; no matching binary string.
- `0x0808fc0c` `SV_RedirectClientDownload` (chunk 20): FUN_0808fc0c (343 bytes): "Redirecting client '%s' to %s" and "%s/%s" built from the www base cvar.; no matching binary string.
- `0x0808fdc2` `SV_WriteDownloadToClient` (chunk 20): FUN_0808fdc2 (1878 bytes): strings "clientDownload: %d : writing block %d", "download disabled",; no matching binary string.
- `0x08090518` `SV_ClientCmd_Disconnect` (chunk 20): FUN_08090518 (27 bytes): ucmd table entry "disconnect" (0x0817d9e8) points here.; no matching binary string.
- `0x080907ba` `SV_ClientCmd_Vdr` (chunk 20): FUN_080907ba (18 bytes): ucmd table entry "vdr" (0x0817d9f8) points here.; no matching binary string.
- `0x08090a04` `SV_ClientCmd_UserInfo` (chunk 20): FUN_08090a04 (85 bytes): ucmd table entry "userinfo" (0x0817d9e0) points here.; no matching binary string.
- `0x08090bac` `SV_ClientCommand` (chunk 20): FUN_08090bac (512 bytes): strings "clientCommand: %i : %s", "Client %s lost %i clientCommands",; no matching binary string.
- `0x08091e48` `SV_ValidateModelName` (chunk 20): FUN_08091e48 (80 bytes): string "\x15" "bad model name '%s'" at 0x0814acfa.; no matching binary string.
- `0x08091f0e` `SV_SkelMemoryAlloc` (chunk 20): FUN_08091f0e (108 bytes): string "^3WARNING: SV_SKEL_MEMORY_SIZE exceeded" from the pool allocator.; no matching binary string.
- `0x08093520` `SV_SpawnServer` (chunk 21): strings '------ Server Initialization ------', 'Server: %s', 'loadingnewmap' in body; no matching binary string.
- `0x080942f8` `SV_Shutdown` (chunk 21): strings '----- Server Shutdown -----' and '---------------------------' in body; no matching binary string.
- `0x08094c84` `SV_StatusResponse` (chunk 21): string 'statusResponse\n%s\n%s' in body; no matching binary string.
- `0x080950d0` `SV_GameCompleteStatus` (chunk 21): string 'gameCompleteStatus\n%s\n%s' in body; no matching binary string.
- `0x0809537c` `SV_InfoResponse` (chunk 21): string 'infoResponse\n' in body; no matching binary string.
- `0x0809594e` `SV_ConnectionlessPacket` (chunk 21): string 'SV packet %s : %s' in body; commands getstatus, getinfo, getchallenge, connect, ipAuthorize, rcon; no matching binary string.
- `0x0809f958` `FS_FOpenFileReadInternal` (chunk 24): rodata 'FS_FOpenFileRead: %s (found in ...)', 'Can\'t find %s', 'Couldn\'t reopen %s' in body; no matching binary string.
- `0x080a57c2` `Mat4_CopyIdentity` (chunk 25): copies the 16-float identity constant at rodata 0x0814d100 (verified bytes); no matching binary string.
- `0x080ab5f4` `Com_AllocFailed_228` (chunk 26): calls error handler with "universal/com_memory.cpp", line 0xe4 (string observed); no matching binary string.
- `0x080ab6ce` `Com_Calloc` (chunk 26): calloc; on NULL calls error handler with "universal/com_memory.cpp", line 0x218 (string observed); no matching binary string.
- `0x080ab980` `Com_InitHunkMemory` (chunk 27): strings com_hunkMegs, "Minimum com_hunkMegs for a dedicated server", universal/com_memory.cpp, meminfo; no matching binary string.
- `0x080acf52` `SND_InitAliasCurves` (chunk 27): strings "soundaliases", "vfcurve", "Snd_Alias Curve initialization: '.vfcurve' file count (%d) exceeds maximum (%d)", "Failed to load sndcurve file"; no matching binary string.
- `0x080ad59c` `Com_GetVolumeModGroup` (chunk 27): string "Sound alias file %s: Volume Mod Group '%s' not found."; searches g_085ab168 (32 x 0x44); no matching binary string.
- `0x080ad820` `Com_ParseAliasChannel` (chunk 27): string "Unknown sound channel '%s'; should be %s" with channel table at PTR_0817e3a0 (auto, auto2d, menu, weapon, voice, item, body, local, music, announcer, shellshock); no matching binary string.
- `0x080ad93c` `Com_ParseAliasType` (chunk 27): strings streamed/primed/loaded and "Unknown sound type"; no matching binary string.
- `0x080ad9da` `Com_ParseAliasLooping` (chunk 27): strings looping/nonlooping and "Unknown sound looping type"; no matching binary string.
- `0x080ada4c` `Com_ParseAliasLoadSpec` (chunk 27): string "loadspec is > %i characters"; tokenised search of mapName inside loadspec value; no matching binary string.
- `0x080adbba` `Com_ParseAliasMasterSlave` (chunk 27): string "master" and "SlavePercentage ... is not within the range"; no matching binary string.
- `0x080adc7e` `Com_ParseAliasReverb` (chunk 27): strings fulldrylevel and nowetlevel; no matching binary string.
- `0x080adcc8` `Com_ParseAliasColumn` (chunk 27): csv column switch with error strings "Duplicate entries for the '%s' column"; column names table at 0x817e340; no matching binary string.
- `0x080ae282` `Com_ValidateSoundAlias` (chunk 27): strings "sound alias '%s' has pitch_min %g <= 0", vol_min, dist_min/dist_max; no matching binary string.
- `0x080ae5d0` `Com_LoadVolumeModGroups` (chunk 27): strings VOLUMEMODGROUPS, soundaliases/volumemodgroups.def, volumemodgroups parse failure errors; no matching binary string.
- `0x080b2afa` `Dvar_SetVariant` (chunk 29): format string "      dvar set %s %s\n" in this function, plus "%s is read only.", "%s is write protected.", "%s is cheat protected.", "%s will be changed upon restarting." and the invalid-value message.; no matching binary string.
- `0x080b3c48` `Dvar_Create` (chunk 29): "Can't create dvar '%s': %i dvars already exist" (limit 0x500). Allocates the next pool entry, copies the name when the flags request ownership, stores the per-type value words, inserts into the name-sorted list and the hash bucket.; no matching binary string.
- `0x080b4ada` `Dvar_SetFromStringEx` (chunk 29): "'%s' is not a valid value for dvar '%s'" in this function. Parses text against the dvar domain; an enum parse failure (-0x539) reports the error and falls back to the reset value.; no matching binary string.
- `0x080b5138` `Dvar_InitSvCheats` (chunk 29): registers the "sv_cheats" dvar (flags 0x1018, bool 0), keeps the pointer in g_dvar_sv_cheats and sets g_085b7654.; no matching binary string.
- `0x080b5bfe` `MsgBuf_WriteData` (chunk 29): "Couldn't write %i bytes to %i-byte buffer (only %i bytes free)". Appends len bytes; on overflow sets the flag and optionally warns.; no matching binary string.
- `0x080b5d5e` `MsgBuf_ReadData` (chunk 29): "Couldn't read %i bytes from %i-byte buffer (only %i bytes left)". Reads len bytes from the cursor into dst; on overflow sets the flag.; no matching binary string.
- `0x080b636a` `Com_UngetToken` (chunk 30): string "UngetToken called twice" (Q3 Com_UngetToken counterpart); no matching binary string.
- `0x080b6ea8` `Com_MatchToken` (chunk 30): string "MatchToken: %s != %s\n"; parameter 3 selects Com_ParseError (0) or Com_ParseWarning (non-zero); no matching binary string.
- `0x080b8802` `Info_SetValueForKey_Big` (chunk 30): big-buffer variant of Info_SetValueForKey (0x2000 buffers); note it still limits the result to strlen sum < 0x401 and calls Info_RemoveKey_Big; no matching binary string.
- `0x080bc986` `XAnim_FindOrLoad` (chunk 32): string/callee evidence: string "^3WARNING: Couldn't find xanim '%s', using default xanim '%s' instead" at 0x0814ffa0; default name "void" at 0x0814ff84; no matching binary string.
- `0x080cbda5` `inflate_codes` (chunk 36): zlib counterpart by structure, struct offsets and constants: strings 'invalid literal/length code','invalid distance code'; calls inflate_fast and inflate_flush; no matching binary string.
- `0x080ccb34` `inflate_fast` (chunk 36): zlib counterpart by structure, struct offsets and constants: same error strings as inflate_codes; called with m>=258,n>=10 guard; no matching binary string.
- `0x080ce566` `inflate_trees_bits` (chunk 36): zlib counterpart by structure, struct offsets and constants: strings 'oversubscribed dynamic bit lengths tree','incomplete dynamic bit lengths tree'; no matching binary string.
- `0x080ce64f` `inflate_trees_dynamic` (chunk 36): zlib counterpart by structure, struct offsets and constants: strings for literal/length and distance tree errors; no matching binary string.
- `0x080d389e` `Tty_Shutdown` (chunk 37): string "Shutdown tty console\n" at 0x80d389e's call site identifies the shutdown; restores the saved termios with TCSADRAIN when the console is active; no matching binary string.
- `0x0080d4392` `Sys_OutOfMemory` (chunk 38): Evidence: string "OUT OF MEMORY! ABORTING!!! (%s:%d)\n" followed by exit(-1).; no matching binary string.
- `0x0080d5330` `Sys_GetPacket` (chunk 38): Evidence: strings "NET_GetPacket: %s from %s\n" and "Oversize packet from %s\n"; recvfrom on ip then ipx sockets (Q3 NET_GetPacket).; no matching binary string.
- `0x0080d54a4` `Sys_SendPacket` (chunk 38): Evidence: strings "NET_SendPacket: bad address type" and "NET_SendPacket ERROR: %s to %s\n"; sendto (Q3 NET_SendPacket).; no matching binary string.
- `0x0080d597e` `NET_OpenIP` (chunk 38): Evidence: strings "net_ip", "localhost", "net_port", "Couldn't allocate IP port"; tries 10 consecutive ports (Q3 NET_OpenIP).; no matching binary string.
- `0x0080d5a5c` `NET_Init` (chunk 38): Evidence: string "net_noudp"; opens the IP socket when UDP is allowed and none is open (Q3 NET_Config/NET_Init shape).; no matching binary string.
- `0x0080d5aa8` `NET_IPSocket` (chunk 38): Evidence: strings "Opening IP socket: %s:%i\n", "ERROR: UDP_OpenSocket: socket/ioctl FIONBIO/setsockopt SO_BROADCAST/bind"; socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP) (Q3 NET_IPSocket).; no matching binary string.
- `0x0080d6668` `Sys_DefaultHomePath` (chunk 38): Evidence: strings "HOME", "/.callofduty2", "Unable to create directory \"%s\", error is %s(%d)\n"; mkdir 0777 ignoring EEXIST (Q3 Sys_DefaultHomePath).; no matching binary string.
- `0x080dc1ae` `Player_LookupAnimTree` (chunk 40): string "Could not find animation tree '%s'" at 0x081532a0; no matching binary string.
- `0x080dc60c` `Player_RegisterJumpDvars` (chunk 40): strings jump_height, jump_stepSize, jump_slowdownEnable, jump_ladderPushVel, jump_spreadAdd at registration; no matching binary string.
- `0x080dcf8c` `Mantle_RegisterDvars` (chunk 40): strings mantle_enable, mantle_debug, mantle_check_range, mantle_check_radius, mantle_check_angle, mantle_view_yawcap; no matching binary string.
- `0x080dd65a` `Mantle_CheckLedge` (chunk 40): strings "Checking for ledge at %f units", "Mantle Failed: Ledge is too far away", "...too steep", "Mantle available!", "...can't fit crouched on ledge", "...Can't find ledge"; no matching binary string.
- `0x080ddaa4` `Mantle_ValidateAnims` (chunk 40): strings "PLAYER_MANTLE" and "Mantle anim [%s] has {X,Y,Z} translation %f, should be %f\n"; no matching binary string.
- `0x080dddfe` `Mantle_FindSurface` (chunk 40): strings "Mantle Failed: No mantle surface found", "...MANTLE_ON or MANTLE_OVER found", "...vertical normal", "...not facing mantle surface", "...brush is too thick"; no matching binary string.
- `0x080de088` `Mantle_Debug` (chunk 40): strings "====== Mantle Debug ======", "Mantle Failed: Not enabled", "...using binoculars", "...not standing", "...already mantling", "...is dead"; no matching binary string.
- `0x080de830` `Player_RegisterDvars` (chunk 40): registration strings player_view_pitch_*, bg_*, inertia*, friction, stopspeed, player_breath_*, player_*Threshhold, player_dmgtimer_* (all observed); no matching binary string.
- `0x080f2b74` `BG_WeaponTypeName` (chunk 45): index into the 8-entry string table at 0x081869e0 (bullet, grenade, projectile...).; no matching binary string.
- `0x080f8b2c` `ClientCleanName` (chunk 46): string "UnnamedPlayer" fallback; Q3 ClientCleanName; no matching binary string.
- `0x080f8e7a` `ClientConnect` (chunk 46): returns string GAME_INVALIDPASSWORD on password key mismatch; reads userinfo key password; no matching binary string.
- `0x080f94ce` `ClientDisconnect` (chunk 46): string "disconnect" passed to script notify; clears connected and 0x2748..0x27a4; no matching binary string.
- `0x080fe102` `CheatsOk` (chunk 46): strings GAME_CHEATSNOTENABLED, GAME_MUSTBEALIVECOMMAND; no matching binary string.
- `0x080fe2ce` `ClientNumberFromString` (chunk 46): strings GAME_BADCLIENTSLOT, GAME_CLIENTNOTACTIVE, GAME_USERNOTONSERVER; no matching binary string.
- `0x080fe588` `Cmd_Give_f` (chunk 46): strings health, weapons, ammo, allammo; no matching binary string.
- `0x080feeba` `Cmd_God_f` (chunk 47): strings GAME_GODMODE_ON/OFF; ClientCommand 'god'; no matching binary string.
- `0x080fef4e` `Cmd_DemiGod_f` (chunk 47): strings GAME_DEMI_GODMODE_ON/OFF; ClientCommand 'demigod'; no matching binary string.
- `0x080fefe2` `Cmd_Notarget_f` (chunk 47): strings GAME_NOTARGETON/OFF; ClientCommand 'notarget'; no matching binary string.
- `0x080ff070` `Cmd_Noclip_f` (chunk 47): strings GAME_NOCLIPON/OFF; ClientCommand 'noclip'; no matching binary string.
- `0x080ff114` `Cmd_UFO_f` (chunk 47): strings GAME_UFOON/OFF; ClientCommand 'ufo'; no matching binary string.
- `0x080ffbb2` `Cmd_CallVote_f` (chunk 47): strings GAME_VOTINGNOTENABLED/GAME_CALLEDAVOTE/map_restart/typemap etc.; ClientCommand 'callvote'; no matching binary string.
- `0x08100960` `Cmd_Vote_f` (chunk 47): strings GAME_NOVOTEINPROGRESS/GAME_VOTECAST; ClientCommand 'vote'; no matching binary string.
- `0x08100b88` `Cmd_SetViewpos_f` (chunk 47): string GAME_CHEATSNOTENABLED/GAME_USAGE: setviewpos x y z yaw; ClientCommand 'setviewpos'; no matching binary string.
- `0x08100cec` `Cmd_EntityCount_f` (chunk 47): string 'entity count = %i\n'; ClientCommand 'entitycount'; no matching binary string.
- `0x0810136a` `G_ParseHitLocDmgTable` (chunk 47): strings 'info/mp_lochit_dmgtable', 'LOCDMGTABLE', hitloc damage table errors; no matching binary string.
- `0x081016fa` `G_MeansOfDeathFromString` (chunk 47): string 'Unknown means of death string'; no matching binary string.
- `0x081034de` `HudElem_SetEnumField` (chunk 47): string 'is not a valid value for hudelem field'; no matching binary string.
- `0x08103e18` `HudElem_FromHandle` (chunk 48): string "not a hud element" at 0x08103e18 callers; handle high word must equal 1, low word indexes the pool [string 'not a hud element' at 0x08103e18 callers]; no matching binary string.
- `0x081043a6` `HudElem_ScrSetTimer` (chunk 48): string "setTimer" at 0x081043a6 is passed as the builtin name [string 'setTimer' at 0x081043a6 passed as builtin name]; no matching binary string.
- `0x081094e8` `G_ShutdownGame` (chunk 49): string '==== ShutdownGame ====' and 'ShutdownGame:' in this body; no matching binary string.
- `0x081098b8` `G_ExitLevel` (chunk 49): string 'ExitLevel: executed\n' is logged here; Cbuf command 'map_rotate'; no matching binary string.
- `0x08109adc` `G_CheckVote` (chunk 49): strings '%c "GAME_VOTEPASSED"' and '%c "GAME_VOTEFAILED"'; no matching binary string.
- `0x08109e60` `G_RunThink` (chunk 49): string 'NULL ent->think' raised when the think table entry is empty; no matching binary string.
- `0x0810a880` `Turret_ComputeFlashFrame` (chunk 49): string 'tag_flash' in the 'Couldn't find %s on turret' error raised here; no matching binary string.
- `0x0810aa50` `Turret_PositionUser` (chunk 49): string "WARNING: aborting player positioning on turret since 'tag_weapon' does not exist"; no matching binary string.
- `0x0810ceea` `Float_Select` (chunk 49): disassembly: returns b when a >= 0 else c (fucompp/jae); no matching binary string.
- `0x08110208` `GScr_GetFunctionOffset` (chunk 50): Resolves script+label to a function offset; optionally reports "Could not find script/label" via Com_Error(1). Strings observed.; no matching binary string.
- `0x08110286` `GScr_LoadGametypeScript` (chunk 50): Loads the gametype script maps/mp/gametypes/<g_gametype> main and the CodeCallback_* entry points from _callbacksetup.; no matching binary string.
- `0x0811037a` `GScr_LoadLevelScript` (chunk 50): Loads maps/mp/<mapname> main (optional; no error if missing). Dvar "mapname" registered with flags 0x1044.; no matching binary string.
- `0x0811041e` `GScr_LoadScripts` (chunk 50): Script load sequence: compile state, codescripts/delete + codescripts/struct entry points, gametype and level scripts, then class init.; no matching binary string.
- `0x081104d2` `GScr_GetEntityFromRef` (chunk 50): Entity reference (low 16 bits = entity number, high 16 bits must be 0) to entity pointer; "not an entity" script error otherwise.; no matching binary string.
- `0x0811050c` `GScr_GetPlayerEntity` (chunk 50): Like GScr_GetEntityFromRef but the entity must be a player (has a client); otherwise Scr_Error with entity number, origin, classname and targetname.; no matching binary string.
- `0x081105c0` `GScr_getclanid` (chunk 50): Script method getclanid (builtin method table at 0x08188720): returns the string "0".; no matching binary string.
- `0x081105d4` `GScr_getclanname` (chunk 50): Script method getclanname (method table): returns an empty string.; no matching binary string.
- `0x081105e8` `GScr_getclanmotto` (chunk 50): Script method getclanmotto (method table): returns an empty string.; no matching binary string.
- `0x08110624` `GScr_print` (chunk 50): Script builtin print (function table at 0x08188040): prints each argument with "%s" when the gating dvar byte is zero.; no matching binary string.
- `0x08110674` `GScr_println` (chunk 50): Script builtin println: print plus newline under the same dvar gate.; no matching binary string.
- `0x081106b6` `GScr_CheckLocalizedRef` (chunk 50): Validates a localized-string reference: every character must be alphanumeric or '_' (only checked when length > 1).; no matching binary string.
- `0x08110720` `GScr_BuildMessageString` (chunk 50): Concatenates script parameters first..last into `out` with 0x14 (localized separator) / 0x15 (literal/player-name marker) prefixes; control bytes inside text become '.'.; no matching binary string.
- `0x08110a60` `GScr_SendGameMessage` (chunk 50): Builds a "Game Message" string from all script arguments and sends `<cmd> "<text>"` to the client (iprintln-style helper; sender is fn_080917aa).; no matching binary string.
- `0x08110c02` `GScr_line` (chunk 50): Script builtin "line" (function table entry): empty body.; no matching binary string.
- `0x08111e90` `GScr_playrumble` (chunk 50): Script method playrumble (method table): empty body.; no matching binary string.
- `0x08111e96` `GScr_playlooprumble` (chunk 50): Script method playlooprumble (method table): empty body.; no matching binary string.
- `0x08111e9c` `GScr_stoprumble` (chunk 50): Script method stoprumble (method table): empty body.; no matching binary string.
- `0x081128ce` `G_SetObjectiveIcon` (chunk 50): objective_icon helper: validates the icon name argument (printable, < 64 chars; errors reported against param 3) and stores its index in objective+0x18.; no matching binary string.
- `0x0811344a` `G_MenuIndex` (chunk 50): Menu config-string index (0x4de..0x4fd) for a precached menu name; script error "Menu '%s' was not precached" otherwise (returns 0).; no matching binary string.
- `0x081134dc` `GScr_PrecacheStatusIcon` (chunk 50): Script builtin precachestatusicon (function table): registers a status icon name in config strings 0x17..0x1e (max 8); duplicates only log a developer message.; no matching binary string.
- `0x081135d2` `G_StatusIconIndex` (chunk 50): 1-based status-icon index for a precached name; 0 for an empty name; script error "Status icon '%s' was not precached" if missing.; no matching binary string.
- `0x0811367a` `GScr_PrecacheHeadIcon` (chunk 50): Script builtin precacheheadicon (function table): registers a head icon in config strings 0x1f..0x2d (max 15).; no matching binary string.
- `0x08113770` `G_HeadIconIndex` (chunk 50): 1-based head-icon index for a precached name; 0 for an empty name; script error "Head icon '%s' was not precached" if missing.; no matching binary string.
- `0x08113818` `GScr_bulletTrace` (chunk 50): Script builtin bullettrace (function table): params start, end, hitCharacters, ignoreEntity; returns an object with fraction, position, entity, normal and surfacetype.; no matching binary string.
- `0x08114a36` `GScr_precacheRumble` (chunk 50): Script builtin precacherumble (function table): empty body.; no matching binary string.
- `0x08115034` `GScr_FxError` (chunk 50): Script error "<msg> (effect = <name>)" where the effect name is config string 0x34e+index, or "not successfully loaded" for index 0.; no matching binary string.
- `0x081150a4` `GScr_playFx` (chunk 50): Script builtin playfx (function table): params fx, origin[, forward[, up]]; spawns temp entity event 0xc2 with the orientation angles in apos.trBase.; no matching binary string.
- `0x0811571c` `GScr_SetCullFogParams` (chunk 50): Validates cull-fog parameters (near >= 0, near < far, rgb in [0,1], time >= 0) and sends "%g %g %g %g %g %g %.0f" (time in ms) via fn_080fe4bc.; no matching binary string.
- `0x08115858` `GScr_setCullFog` (chunk 50): Script builtin setcullfog (function table): six float params near, far, r, g, b, time; passes 1.0 as the constant fourth value.; no matching binary string.
- `0x08117906` `GScr_playRumbleOnPos` (chunk 50): Script builtin playrumbleonpos (function table): empty body.; no matching binary string.
- `0x0811790c` `GScr_playLoopRumbleOnPos` (chunk 50): Script builtin playlooprumbleonpos (function table): empty body.; no matching binary string.
- `0x08117912` `GScr_stopAllRumbles` (chunk 50): Script builtin stopallrumbles (function table): empty body.; no matching binary string.
- `0x0811792c` `GScr_matchend` (chunk 50): Script builtin matchend (function table): empty body.; no matching binary string.
- `0x08117932` `GScr_setPlayerTeamRank` (chunk 50): Script builtin setplayerteamrank (function table): empty body.; no matching binary string.
- `0x08117938` `GScr_sendRanks` (chunk 50): Script builtin sendranks (function table): empty body.; no matching binary string.
- `0x0811793e` `GScr_endParty` (chunk 50): Script builtin endparty (function table): empty body.; no matching binary string.
- `0x08117f70` `GScr_LoadGametypeList` (chunk 50): Builds the gametype list from maps/mp/gametypes/*.gsc (skipping names starting with '_'): name, description and team flag from <name>.txt (second token "team"). Caps at 0x20 entries.; no matching binary string.
- `0x081197a6` `SP_script_brushmodel` (chunk 51): Spawn function for classname "script_brushmodel" (spawn table 0x08188a00 entry): brush model setup (fn_08091858), common; no matching binary string.
- `0x081197dc` `SP_script_model` (chunk 51): Spawn function for classname "script_model" (spawn table entry): model setup (fn_0811d69e), common setup, svFlags |= 4,; no matching binary string.
- `0x08119c3c` `GScr_MoveX` (chunk 51): Script entity method "movex" (method table 0x0815e3a0 entry points here).; no matching binary string.
- `0x08119ece` `GScr_RotatePitch` (chunk 51): Script entity method "rotatepitch" (method table entry).; no matching binary string.
- `0x0811a4dc` `Scr_ReadOnlyEntityField` (chunk 51): Field setter used for read-only entity fields (classname, model, spawnflags): raises the script error.; no matching binary string.
- `0x0811ac96` `G_LoadRadiantFieldFiles` (chunk 51): Loads the "radiant" field description files with extension "txt".; no matching binary string.
- `0x0811b16e` `Scr_GetEntityParam` (chunk 51): Reads script param as an entity reference; high half nonzero -> error "not an entity" and NULL, else the entity from the array at 0x08716400.; no matching binary string.
- `0x0811bad2` `AddIP` (chunk 51): AddIP: reuses the first free filter slot (compare == -1) or appends (max 0x400, "IP filter list is full\n"), parses the; no matching binary string.
- `0x0811bc02` `Svcmd_AddIP_f` (chunk 51): Svcmd_AddIP_f ("Usage:  addip <ip-mask>\n"): takes console arg 1.; no matching binary string.
- `0x0811bc52` `Svcmd_RemoveIP_f` (chunk 51): Svcmd_RemoveIP_f ("Usage:  sv removeip <ip-mask>\n", "Removed.\n", "Didn't find %s.\n"): frees the exact mask/compare match and republishes g_banIPs.; no matching binary string.
- `0x0811bd22` `Svcmd_EntityList_f` (chunk 51): Svcmd_EntityList_f (strings "ET_GENERAL", "ET_PLAYER", ...): lists all in-use entities from index 1 with their type name; no matching binary string.
- `0x0811bf64` `G_ConsoleCommand` (chunk 51): ConsoleCommand (strings "entitylist", "addip", ...): dispatches entitylist / addip / removeip / listip (runs "g_banIPs\n"; no matching binary string.
- `0x0811d44a` `G_ShaderIndex` (chunk 52): label string "shader" passed to the configstring lookup in this function.; no matching binary string.
- `0x0811d600` `G_EffectIndex` (chunk 52): label string "effect" passed to the configstring lookup here. Ghidra prints void; the binary returns the callee's eax.; no matching binary string.
- `0x0811e8d4` `G_PrintEntities` (chunk 52): format "%4i: '%s', origin: %f %f %f\n" in the function.; no matching binary string.
- `0x0812411a` `FxPool_AllocBlock` (chunk 54): string '^1Out of effects memory!\n' printed via Com_DPrintf when no block is free; no matching binary string.
- `0x0812ac64` `FxScheduler_ReportInvalidEffect` (chunk 55): prints "FxScheduler::PlayEffect called with invalid effect\n" through fn_081314e2 [REA decompile sub_0812ac64; Ghidra 12.2 pseudocode, cross-checked against disassembly where x87/float semantics mattered]; no matching binary string.
- `0x0812f1e4` `PrimitiveTemplate::ParseSequenceStartFrameMode` (chunk 57): Evidence: key string "sequenceStartFrameMode" selects this handler in 0x08130248; accepts 0..2.; no matching binary string.
- `0x0812f228` `PrimitiveTemplate::ParseSequenceFixedFrameValue` (chunk 57): Evidence: key string "sequenceFixedFrameValue"; stores unconditionally, succeeds if > 0.; no matching binary string.
- `0x0812f268` `PrimitiveTemplate::ParseSequencePlayRateMode` (chunk 57): Evidence: key string "sequencePlayRateMode"; accepts 0..1.; no matching binary string.
- `0x0812f2ac` `PrimitiveTemplate::ParseSequenceFixedFpsValue` (chunk 57): Evidence: key string "sequenceFixedFpsValue"; stores unconditionally, succeeds if >= 0.; no matching binary string.
- `0x0812f2f4` `PrimitiveTemplate::ParseSequenceLoopMode` (chunk 57): Evidence: key string "sequenceLoopMode"; accepts 0..1.; no matching binary string.
- `0x0812f338` `PrimitiveTemplate::ParseSequenceLoopTimes` (chunk 57): Evidence: key string "sequenceLoopTimes"; stores unconditionally, succeeds if >= 0.; no matching binary string.
- `0x0812f378` `PrimitiveTemplate::ParseSpawnFrustumCullRadius` (chunk 57): Evidence: key string "spawnFrustumCullRadius"; stores only if >= 0.; no matching binary string.
- `0x0812fd3c` `PrimitiveTemplate::ParseCurveVec3` (chunk 57): Evidence: format "%f %f %f %f" (time + rgb) per child; called for key "curve" on channels 0/1; stores the curve at +0x100+channel*12.; no matching binary string.
- `0x0812fe54` `PrimitiveTemplate::ParseCurveFloat` (chunk 57): Evidence: format "%f %f" (time + value) per child; called for key "curve" on scalar channels.; no matching binary string.
- `0x0812ff4c` `PrimitiveTemplate::ParseChannelScale` (chunk 57): Evidence: key "scale" in ParseChannel; stores a range at channel slot +4 (+0x104+channel*12).; no matching binary string.
- `0x0812ffa2` `PrimitiveTemplate::ParseChannel` (chunk 57): Evidence: strings "curve","scale","start","end","parm","parms","flags","flag" and "Unknown key parsing a channel: %s\n".; no matching binary string.
- `0x0813021e` `PrimitiveTemplate::ParseShaderImpact` (chunk 57): Evidence: key "shaderImpact" copies up to 0x20 bytes into +0x20.; no matching binary string.
- `0x081330ea` `ConfigTree_Init1` (chunk 58): ConfigTree constructor (variant 1): root block named "Top Level" (string at 0x08160640).; no matching binary string.
- `0x0813311e` `ConfigTree_Init2` (chunk 58): ConfigTree constructor (variant 2).; no matching binary string.
- `0x08138162` `Loc_RegisterDvars` (chunk 59): string dvar names loc_language, loc_forceEnglish, loc_translate, loc_warnings, loc_warningsAsErrors; no matching binary string.
- `0x08138224` `Loc_InitLanguages` (chunk 59): string "^1ERROR: No languages available because no localized assets were found"; re-registers loc dvars by name, scans 15 language pak ids; no matching binary string.
- `0x0813840a` `Loc_LoadLanguage` (chunk 59): strings "Could not load localization strings for %s: %s" (warning and error variants); no matching binary string.
- `0x08138522` `Loc_TranslateExeString` (chunk 59): string "Could not translate exe string \"%s\"" and the "^1UNLOCALIZED(^7" wrapper; no matching binary string.
- `0x081385fe` `Loc_TranslatePart` (chunk 59): string "Could not translate part of %s: \"%s\""; "%s" format at 0x8160a09 (rodata dump); no matching binary string.
- `0x0813c938` `PB_Construct` (chunk 61): rodata string "PunkBuster Server" copied to object +0x14 by this function; magic 0x357afe24 at +0; no matching binary string.
- `0x08a70010` `std::string::operator=` (chunk 61): Ghidra EXTERNAL::std::string::operator= placeholder at 0x08a70010 outside the loaded image (no code bytes); no matching binary string.
- `0x08a70018` `std::string::compare` (chunk 61): Ghidra EXTERNAL::std::string::compare placeholder at 0x08a70018 outside the loaded image (no code bytes); no matching binary string.
- `0x08a70070` `std::string::string` (chunk 61): Ghidra EXTERNAL::std::string::string placeholder at 0x08a70070 outside the loaded image (no code bytes); no matching binary string.
- `0x08a700a8` `std::string::c_str` (chunk 61): Ghidra EXTERNAL::std::string::c_str placeholder at 0x08a700a8 outside the loaded image (no code bytes); no matching binary string.
- `0x08a700bc` `std::string::operator=` (chunk 61): Ghidra EXTERNAL::std::string::operator= placeholder at 0x08a700bc outside the loaded image (no code bytes); no matching binary string.
- `0x08a700c8` `std::string::~string` (chunk 61): Ghidra EXTERNAL::std::string::~string placeholder at 0x08a700c8 outside the loaded image (no code bytes); no matching binary string.
- `0x08a700d4` `std::string::operator[]` (chunk 61): Ghidra EXTERNAL::std::string::operator[] placeholder at 0x08a700d4 outside the loaded image (no code bytes); no matching binary string.
- `0x08a700e8` `std::string::string` (chunk 61): Ghidra EXTERNAL::std::string::string placeholder at 0x08a700e8 outside the loaded image (no code bytes); no matching binary string.
- `0x08a7013c` `std::string::erase` (chunk 62): ELF dynamic import '_ZNSs5eraseEjj': PLT entry 0x0804a244 (chunk_000); Ghidra EXTERNAL placeholder 0x8a7013c; no matching binary string.
- `0x08a70144` `std::string::operator+=` (chunk 62): ELF dynamic import '_ZNSspLEc': PLT entry 0x0804a264 (chunk_000); Ghidra EXTERNAL placeholder 0x8a70144; no matching binary string.
- `0x08a70150` `std::string::find` (chunk 62): ELF dynamic import '_ZNKSs4findEPKcj': PLT entry 0x0804a294 (chunk_000); Ghidra EXTERNAL placeholder 0x8a70150; no matching binary string.
- `0x08a70184` `std::string::string` (chunk 62): ELF dynamic import '_ZNSsC1ERKSs': PLT entry 0x0804a364 (chunk_000); Ghidra EXTERNAL placeholder 0x8a70184; no matching binary string.
- `0x08a70204` `std::string::operator+=` (chunk 62): ELF dynamic import '_ZNSspLEPKc': PLT entry 0x0804a564 (chunk_000); Ghidra EXTERNAL placeholder 0x8a70204; no matching binary string.
- `0x081043ca` `HudElem_SetTimerUp` (chunk 104): literal "setTimerUp" is passed by this function (method name, script-visible).; no matching binary string.
- `0x081043ee` `HudElem_SetTenthsTimer` (chunk 104): literal "setTenthsTimer" is passed by this function.; no matching binary string.
- `0x08104412` `HudElem_SetTenthsTimerUp` (chunk 104): literal "setTenthsTimerUp" is passed by this function.; no matching binary string.
- `0x08104436` `HudElem_SetClock` (chunk 104): literal "setClock" is passed by this function.; no matching binary string.
- `0x0810445a` `HudElem_SetClockUp` (chunk 104): literal "setClockUp" is passed by this function.; no matching binary string.
- `0x08114fe8` `GScr_LoadFx` (chunk 106): loadFx builtin. Evidence: string "loadFx must be called before any wait statements in the; no matching binary string.
- `0x081152d6` `GScr_PlayFxOnTag` (chunk 106): playFxOnTag builtin. Evidence: string "USAGE: playFxOnTag <effect id from loadFx> <entity> <tag name>".; no matching binary string.
- `0x08115466` `GScr_PlayLoopedFx` (chunk 106): playLoopedFx builtin. Evidence: string "USAGE: playLoopedFx <effect id from loadFx> <repeat delay> ..."; no matching binary string.
- `0x08115910` `GScr_SetExpFog` (chunk 106): setExpFog builtin. Evidence: string "setExpFog" and its USAGE text referenced here.; no matching binary string.
- `0x081162a2` `GScr_Shellshock` (chunk 106): shellshock builtin for a player (self ref). Evidence: "USAGE: <player> shellshock(<shellshockname>, <duration>)"; no matching binary string.
- `0x0811642a` `GScr_StopShellshock` (chunk 106): stopshellshock builtin for a player (self ref). Evidence: "USAGE: <player> stopshellshock()".; no matching binary string.
- `0x0811648e` `GScr_ViewKick` (chunk 106): viewkick builtin for a player (self ref). Evidence: "USAGE: <player> viewkick <force 0-127> <source position>".; no matching binary string.
- `0x08116a3c` `GScr_Map_Restart` (chunk 106): map_restart builtin. Evidence: strings "map_restart already called" and "exitlevel already called"; no matching binary string.
- `0x08116ab4` `GScr_Map` (chunk 106): map builtin. Evidence: strings "map already called" and "map %s\n" referenced here.; no matching binary string.
- `0x08116b6a` `GScr_ExitLevel` (chunk 106): exitlevel builtin. Evidence: strings "map_restart already called" and "exitlevel already called" referenced here.; no matching binary string.
- `0x08117022` `GScr_TempBanClient` (chunk 106): tempBanClient builtin. Evidence: format string "tempBanClient %i\n" referenced here; the text is; no matching binary string.
- `0x08117068` `GScr_BanClient` (chunk 106): banClient builtin. Evidence: format string "banClient %i\n" referenced here.; no matching binary string.
- `0x08117120` `GScr_OpenFile` (chunk 106): openfile builtin: opens scriptdata/<name> in read/write/append mode into the file slot table.; no matching binary string.
- `0x081173b4` `GScr_CloseFile` (chunk 106): closefile builtin. Evidence: "CloseFile failed, invalid file number %i" and; no matching binary string.
- `0x0811749c` `GScr_FPrintln` (chunk 106): fprintln builtin. Evidence: "fprintln requires at least 2 parameters (file, output)" and; no matching binary string.
- `0x081175c0` `GScr_FReadln` (chunk 106): freadln builtin. Evidence: "freadln requires a parameter - the file to read from" and; no matching binary string.
- `0x08117778` `GScr_FReadline` (chunk 106): freadline builtin. Evidence: "freadline requires at least 2 parameters (file, string)" and; no matching binary string.
- `0x08117944` `GScr_SetTeamForTrigger` (chunk 106): setteamfortrigger builtin. Evidence: "setteamfortrigger: trigger entity must be of type %s or %s" and; no matching binary string.
- `0x08117a72` `GScr_ClientClaimTrigger` (chunk 106): clientclaimtrigger builtin. Evidence: "clientclaimtrigger: claimer must be a client." and; no matching binary string.
- `0x08117b5a` `GScr_ClientReleaseTrigger` (chunk 106): clientreleasetrigger builtin. Evidence: "clientreleasetrigger: releaser must be a client." and; no matching binary string.
- `0x08117c28` `GScr_ReleaseClaimedTrigger` (chunk 106): releaseclaimedtrigger builtin. Evidence: "releaseclaimedtrigger: trigger entity must be of type %s or %s"; no matching binary string.

## WEAK items

- `0x080521a2` `CM_LoadMap` (chunk 2): Name evidence: string "\x15CM_LoadMap: NULL name" passed to the error function (fn_08061124) when name is NULL or empty.; matched string(s): `CM_LoadMap: NULL name` @ `0x0813f5e1`, `CM_LoadMapFromBsp` @ `0x0813ff8a`
- `0x08052d16` `CMod_PartionLeafBrushes_r` (chunk 3): String "CMod_PartionLeafBrushes_r" (sic) is the temp-allocation tag; recursive split of a brush index list into a cbrushnode_t tree.; matched string(s): `CMod_PartionLeafBrushes_r: leafBrushCount exceeded` @ `0x0813f7e1`, `CMod_PartionLeafBrushes_r` @ `0x0813f814`, `CMod_PartionLeafBrushes_r: child exceeded` @ `0x0813f841`
- `0x08053404` `CMod_LoadSubmodelBrushNodes` (chunk 3): String "CMod_LoadSubmodelBrushNodes" tags the allocation; builds per-submodel (index >= 1) brush lists and trees.; matched string(s): `>CMod_LoadSubmodelBrushNodes` @ `0x0813f86f`, `CMod_LoadSubmodelBrushNodes: leafBrushes exceeded` @ `0x0813f8a1`
- `0x080536bc` `CMod_LoadBrushes` (chunk 3): Strings "CMod_LoadBrushes" / "CMod_LoadBrushSides" tag the allocations. File brushes are (short numSides, short materialNum); the side lump holds 6 axial (float, material) pairs per brush followed by the extra sides.; matched string(s): `CMod_LoadBrushes: funny lump size` @ `0x0813f981`, `CMod_LoadBrushes: bad side count` @ `0x0813f9c1`, `CMod_LoadBrushes` @ `0x0813f9f6`, `CMod_LoadBrushes: cm.numBrushes exceeded` @ `0x0813fa21`, `CMod_LoadBrushes: brush has less than 6 sides` @ `0x0813fa61`, `CMod_LoadBrushes: bad materialNum: %i` @ `0x0813faa1`, `CMod_LoadBrushes: axialMaterialNum exceeded` @ `0x0813fae1`
- `0x0805fe3c` `Cbuf_ExecuteText` (chunk 6): String '\025Cbuf_ExecuteText: bad exec_when' at 0x08140320. 0 = now, 1 = insert, 2 = append.; matched string(s): `Cbuf_ExecuteText: bad exec_when` @ `0x08140321`
- `0x0806182a` `Com_EventLoop` (chunk 6): String 'Com_EventLoop: oversize packet\n'; "\025Com_EventLoop: bad event type %i". Q3 Com_EventLoop: an empty event first drains loopback packets for socket 1 (into a 0x20000-byte temporary message buffer) and returns that event's time. Type 4 = console command text, appended to the command buffer plus "\n"; type 5 = packet whose payload follows a 0x14-byte netadr. Packets are delivered via fn_080960e2 only while com_sv_running is set. Returns the time of the terminating empty event.; matched string(s): `Com_EventLoop: bad event type %i` @ `0x081406c1`, `Com_EventLoop: oversize packet
` @ `0x08140700`
- `0x08091734` `SV_SvEntityForGentity` (chunk 20): FUN_08091734 (71 bytes): string "\x15SV_SvEntityForGentity: bad gEnt" at 0x0814abc0.; matched string(s): `SV_SvEntityForGentity: bad gEnt` @ `0x0814abc1`
- `0x08091cd0` `SV_GetServerinfo` (chunk 20): FUN_08091cd0 (75 bytes): string "\x15SV_GetServerinfo: bufferSize == %i" at 0x0814ac00.; matched string(s): `SV_GetServerinfo: bufferSize == %i` @ `0x0814ac01`
- `0x08092780` `SV_SetConfigstring` (chunk 21): string 'SV_SetConfigstring: bad index %i' (0x0814adc0) in body; matched string(s): `SV_SetConfigstring: bad index %i
` @ `0x0814adc1`
- `0x08092992` `SV_GetConfigstring` (chunk 21): string 'SV_GetConfigstring: bad index %i' (0x0814ae40) in body; matched string(s): `SV_GetConfigstring: bufferSize == %i` @ `0x0814ae01`, `SV_GetConfigstring: bad index %i
` @ `0x0814ae41`
- `0x08092abe` `SV_SetConfigValueForKey` (chunk 21): string 'SV_SetConfigValueForKey: overflow' (0x0814ae80) in body; matched string(s): `SV_SetConfigValueForKey: overflow` @ `0x0814ae81`
- `0x08092b5a` `SV_SetUserinfo` (chunk 21): string 'SV_SetUserinfo: bad index %i' (0x0814aec0) in body; matched string(s): `SV_SetUserinfo: bad index %i
` @ `0x0814aec1`
- `0x08092c04` `SV_GetUserinfo` (chunk 21): strings 'SV_GetUserinfo: bufferSize == %i' and 'bad index %i' in body; matched string(s): `SV_GetUserinfo: bufferSize == %i` @ `0x0814af01`, `SV_GetUserinfo: bad index %i
` @ `0x0814af41`
- `0x08092dfa` `SV_Startup` (chunk 21): strings 'SV_Startup: svs.initialized' and 'SV_Startup: unable to allocate svs.clients' in body; matched string(s): `SV_Startup: svs.initialized` @ `0x0814af6e`, `SV_Startup: unable to allocate svs.clients` @ `0x0814afa1`
- `0x080960e2` `SV_ReadPackets` (chunk 22): string 'SV_ReadPackets: fixing up a translated port'; Q3 SV_PacketEvent shape; matched string(s): `SV_ReadPackets: fixing up a translated port
` @ `0x0814b540`
- `0x0809907a` `SV_GetCachedSnapshot` (chunk 22): string 'SV_GetCachedSnapshot: end of message'; matched string(s): `SV_GetCachedSnapshot: end of message` @ `0x0814c221`
- `0x0809a408` `SV_BuildClientSnapshot` (chunk 23): * Name evidence: error string "SV_BuildClientSnapshot: bad gEnt" at 0x0814c320 (without the leading 0x15 byte); builds * client_frame_s (frames[outgoingSequence & 31]) from svs snapshot ring buffers or the cached snapshot frame.; matched string(s): `SV_BuildClientSnapshot: bad gEnt` @ `0x0814c321`
- `0x0809b016` `SV_ArchiveSnapshot` (chunk 23): * Name evidence: string "SV_ArchiveSnapshot: ignoring snapshot because it overflowed." and anchor SV_ArchiveSnapshot. * Writes a (full or delta) snapshot into the archive ring buffer and caches entity/client state. Exception-unwind * cleanup of TempBuffer is not modelled.; matched string(s): `SV_ArchiveSnapshot: ignoring snapshot because it overflowed.
` @ `0x0814c4c0`
- `0x0809e7d4` `FS_HandleForFile` (chunk 23): * Name evidence: error string "FS_HandleForFile: none free" and the "FILE %2i: '%s'" dump. Slots 1..0x32 for * non-zip, 0x33..0x33+0xc for zip (zipFile != 0).; matched string(s): `FS_HandleForFile: none free` @ `0x0814c6b1`
- `0x0809e9b6` `FS_BuildOSPath` (chunk 23): * Name evidence: error string "FS_BuildOSPath: os path length exceeded"; builds base/game/qpath into out (max 255 chars), * with failSilently returning an empty string instead of raising the error.; matched string(s): `FS_BuildOSPath: os path length exceeded
` @ `0x0814c6e1`
- `0x0809eb76` `FS_CopyFile` (chunk 24): rodata 'Short read in FS_CopyFile()' and 'Short write in FS_CopyFile()' in body; callers FS_Rename (0809ed5a) and 0809f958; matched string(s): `Short read in FS_CopyFile()
` @ `0x0814c754`, `Short write in FS_CopyFile()
` @ `0x0814c781`
- `0x0809f3c8` `FS_FileCompare` (chunk 24): rodata 'FS_FileCompare: %s does not exist' and 'Short read in FS_FileCompare()' in body; matched string(s): `FS_FileCompare: %s does not exist
` @ `0x0814c801`, `Short read in FS_FileCompare()
` @ `0x0814c841`
- `0x080a052a` `FS_Read` (chunk 24): rodata 'FS_Read: -1 bytes read' in body; pack handles delegate to 80d3094; matched string(s): `FS_Read: -1 bytes read` @ `0x0814c97c`, `FS_ReadFile with empty name
` @ `0x0814c994`
- `0x080a0a9c` `FS_ReadFile` (chunk 24): rodata 'FS_ReadFile with empty name' in body; fs_loadStack increment; matched string(s): `FS_ReadFile with empty name
` @ `0x0814c994`
- `0x080a3562` `FSH_FOpenFile` (chunk 25): string "FSH_FOpenFile: bad mode" names this function; modes 0 read, 1 write, 2 append, 3 append-sync (Q3 fsMode_t order); matched string(s): `FSH_FOpenFile: bad mode` @ `0x0814cf7e`
- `0x080abf94` `Hunk_AllocAlign` (chunk 27): string "Hunk_AllocAlign failed on %i bytes (total %i MB, low %i MB, high %i MB)" at DAT_0814d440; matched string(s): `Hunk_AllocAlign can only be called in G_InitGame and the first few frames of G_RunFrame
` @ `0x0814aca1`, `Hunk_AllocAlign failed on %i bytes (total %i MB, low %i MB, high %i MB)` @ `0x0814d441`
- `0x080ac088` `Hunk_AllocateTempMemoryHigh` (chunk 27): string "Hunk_AllocateTempMemoryHigh: failed on %i bytes" at DAT_0814d4a0; matched string(s): `Hunk_AllocateTempMemoryHigh: failed on %i bytes (total %i MB, low %i MB, high %i MB)` @ `0x0814d4a1`
- `0x080ac178` `Hunk_AllocLowAlign` (chunk 27): string "Hunk_AllocLowAlign failed on %i bytes" at DAT_0814d500; matched string(s): `Hunk_AllocLowAlign failed on %i bytes (total %i MB, low %i MB, high %i MB)` @ `0x0814d501`
- `0x080ac276` `Hunk_AllocateTempMemory` (chunk 27): string "Hunk_AllocateTempMemory: failed on %i bytes ... needs %i more hunk bytes" at DAT_0814d560; writes magic 0x89537892 header; matched string(s): `Hunk_AllocateTempMemoryHigh: failed on %i bytes (total %i MB, low %i MB, high %i MB)` @ `0x0814d4a1`, `Hunk_AllocateTempMemory: failed on %i bytes (total %i MB, low %i MB, high %i MB), needs %i more hunk bytes` @ `0x0814d561`
- `0x080ac3a2` `Hunk_ReallocateTempMemory` (chunk 27): string "Hunk_ReallocateTempMemory: failed on %i bytes" at DAT_0814d5e0; body is a 32-aligned low.temp allocation (no header, no copy), name kept from the string; matched string(s): `Hunk_ReallocateTempMemory: failed on %i bytes (total %i MB, low %i MB, high %i MB)` @ `0x0814d5e1`
- `0x080ac460` `Hunk_FreeTempMemory` (chunk 27): string "Hunk_FreeTempMemory: bad magic" at DAT_0814d640; checks header magic 0x89537892; matched string(s): `Hunk_FreeTempMemory: bad magic` @ `0x0814d641`
- `0x080ae532` `Com_AddBuildSoundAlias` (chunk 27): allocation tag string "Com_AddBuildSoundAlias"; matched string(s): `@Com_AddBuildSoundAlias` @ `0x0814e033`
- `0x080af224` `Com_MakeSoundAliasesPermanent` (chunk 28): allocation tags 'Com_MakeSoundAliasesPermanent:aliases|soundFiles|strings|aliasList'; anchor name; matched string(s): `Com_MakeSoundAliasesPermanent:aliases` @ `0x0814e3c0`, `Com_MakeSoundAliasesPermanent:soundFiles` @ `0x0814e400`, `Com_MakeSoundAliasesPermanent:strings` @ `0x0814e440`, `Com_MakeSoundAliasesPermanent:aliasList` @ `0x0814e480`
- `0x080b0e9e` `COM_WriteFinalStringEdFile` (chunk 28): 'Short read/write in COM_WriteFinalStringEdFile()' error strings; matched string(s): `Short read in COM_WriteFinalStringEdFile()
` @ `0x0814ec01`, `Short write in COM_WriteFinalStringEdFile()
` @ `0x0814ec41`
- `0x080b1274` `generateHashValue` (chunk 28): error string 'null name in generateHashValue' names the function; matched string(s): `null name in generateHashValue` @ `0x0814ee21`
- `0x080b5f32` `Com_BeginParseSession` (chunk 29): "\x15Com_BeginParseSession: session overflow trying to parse %s\n" (0x0814f600) and the "Already parsing:" listing. Pushes a named slot. At 15 active slots it lists them and reports overflow, then pushes anyway (the binary does not return), writing past the struct as the original does.; matched string(s): `Com_BeginParseSession: session overflow trying to parse %s
` @ `0x0814f601`
- `0x080b6004` `Com_EndParseSession` (chunk 30): string "Com_EndParseSession: session underflow" at 0x814f640 is raised here; matched string(s): `Com_EndParseSession: session underflow` @ `0x0814f641`
- `0x080b7d98` `I_strncat` (chunk 30): string "I_strncat: already overflowed" (error code 0 when strlen(dest) >= size); matched string(s): `I_strncat: already overflowed` @ `0x0814f861`
- `0x080b7fa6` `va` (chunk 30): string "Attempted to overrun string in call to va()"; two rotating 0x400 buffers in per-thread block slot 1; matched string(s): `invalid define parameter` @ `0x0813e685`, `out of value space
` @ `0x0813e8c2`, `! or ~ after value in #if/#elif` @ `0x0813e980`, `invalid operator %s in #if/#elif` @ `0x0813ea00`, `mising values in #if/#elif` @ `0x0813ea97`, `no value after #if/#elif` @ `0x0813ec38`, `can't evaluate %s, not defined` @ `0x0813ec60`, `can't evaluate %s` @ `0x0813ec7f`, `no leading ( after $evalint/$evalfloat` @ `0x0813eca0`, `nothing to evaluate` @ `0x0813ecc7`, `eval` @ `0x0813ed8f`, `evalfloat` @ `0x0813ed94`, `evalint` @ `0x0813ee01`, `too large value in escape character` @ `0x0813f080`, `expected float value, found %s
` @ `0x0813f500`, `expected integer value, found %s
` @ `0x0813f520`, `Invalid static model name
` @ `0x08140121`, `vstr <variablename> : execute a variable command
` @ `0x081403a0`, `%s is not a string-based dvar
` @ `0x08140400`, `dvar_restart` @ `0x08140650`, `'toggle' with no arguments makes no sense for dvar '%s'
` @ `0x08140b40`, `USAGE: %s <variable> <optional value sequence>
` @ `0x08140ba0`, `toggle failed: dvar '%s' not found.
` @ `0x08140be0`, `USAGE: set <variable> <value>
` @ `0x08140c20`, `invalid variable name: %s
` @ `0x08140c3f`, `dvar '%s' is not a boolean dvar
` @ `0x08140c80`, `dvar %s: min %i should not be greater than max %i
` @ `0x08140d00`, `dvar '%s' is not an integer dvar
` @ `0x08140d40`, `dvar %s: min %g should not be greater than max %g
` @ `0x08140d80`, `USAGE: setu <variable> <value>
` @ `0x08140dc0`, `USAGE: sets <variable> <value>
` @ `0x08140de0`, `USAGE: seta <variable> <value>
` @ `0x08140e00`, `USAGE: setfromdvar <dest_dvar> <source_dvar>
` @ `0x08140e20`, `dvar '%s' doesn't exist
` @ `0x08140e4e`, `USAGE: reset <variable>
` @ `0x08140e67`, `
%i total dvars
` @ `0x08140ebc`, `
%i total dvars
%i dvar indexes
` @ `0x08140f80`, `setfromdvar` @ `0x08141032`, `dvarlist` @ `0x08141044`, `dvardump` @ `0x0814104d`, `dvar_bool` @ `0x08141056`, `dvar_int` @ `0x08141060`, `dvar_float` @ `0x08141069`, `value` @ `0x08142ad3`, `uninitialised variable '%s'` @ `0x08143982`, `return value of developer command can not be accessed if not in a /# ... #/ comment` @ `0x08143b60`, `not an lvalue` @ `0x08143f2c`, `count: %d, var usage: %d, endon usage: %d
` @ `0x08144a40`, `ent type '%s'... count: %d, var usage: %d
` @ `0x08144a80`, `exceeded maximum number of script variables` @ `0x08144ac0`, `first parameter of waittill must evaluate to a string` @ `0x081451c0`, `first parameter of notify must evaluate to a string` @ `0x08145200`, `first parameter of endon must evaluate to a string` @ `0x08145240`, `cannot create a new local variable in the debugger` @ `0x081452c0`, `Invalid mute client %i
` @ `0x0814a90f`, `Invalid unmute client %i
` @ `0x0814a927`, `g_gametype %s is not a valid gametype, defaulting to dm
` @ `0x0814ad80`, `sv_privateClients` @ `0x0814b0f8`, `sv_privatePassword` @ `0x0814b20b`, `Invalid reliableAcknowledge message from %s - reliableAcknowledge is %i
` @ `0x0814b740`, `Invalid password.
` @ `0x0814bcc0`, `Received invalid voice packet of size %i from %s
` @ `0x0814c640`, `WARNING: Localized assets iwd file %s/%s/%s has invalid name (no language specified). Proper naming convention is: localized_[language]_iwd#.iwd
` @ `0x0814cba0`, `WARNING: Localized assets iwd file %s/%s/%s has invalid name (bad language name specified). Proper naming convention is: localized_[language]_iwd#.iwd
` @ `0x0814cc40`, `Invalid game folder
` @ `0x0814cf4a`, `^1ERROR: Sound alias file %s: Alias name '%s' is invalid
` @ `0x0814dc60`, `^1ERROR: Sound alias file %s: Secondary Alias name '%s' is invalid
` @ `0x0814dd00`, `^1ERROR: Sound alias file %s: Subtitle '%s' has invalid character '%c' ascii %i
` @ `0x0814ddc0`, `ERROR: volumemodgroups parse failure on file "%s": groupname '%s' missing a matching value
` @ `0x0814e1c0`, `      dvar set %s %s
` @ `0x0814f288`, `'%s' is not a valid value for dvar '%s'
` @ `0x0814f2a0`, `Can't create dvar '%s': %i dvars already exist` @ `0x0814f360`, `^3WARNING: unknown dvar '%s' in file '%s'
` @ `0x0814f3c0`, `^1ERROR: the following dvars were not specified in file '%s'
` @ `0x0814f400`, `Attempted to overrun string in call to va()` @ `0x0814f881`, `Can't use keys with a \
key: '%s'
value: '%s'` @ `0x0814f9c1`, `Can't use keys with a semicolon
key: '%s'
value: '%s'` @ `0x0814fa01`, `Can't use keys with a "
key: '%s'
value: '%s'` @ `0x0814fa41`, `Info buffer length exceeded, not including key/value pair in response
` @ `0x0814fa81`, `Info string length exceeded
key: '%s'
value: '%s'
Info string:
%s
` @ `0x0814fae1`, `invalid block type` @ `0x081509cc`, `invalid stored block lengths` @ `0x081509df`, `invalid bit length repeat` @ `0x08150a24`, `invalid literal/length code` @ `0x08150a68`, `invalid distance code` @ `0x08150a84`, `invalid distance code` @ `0x08150ac4`, `invalid literal/length code` @ `0x08150ada`, `invalid window size` @ `0x08150b13`, `BG_ParseConditions: expected condition value, found end of line` @ `0x081529c0`, `BG_ParseCommands: expected duration value` @ `0x08152ac0`, `BG_ParseCommands: expected blendtime value` @ `0x08152b60`, `Mantle available!` @ `0x08153592`, `bg_foliagesnd_slowinterval` @ `0x08154903`, `bg_foliagesnd_fastinterval` @ `0x0815491e`, `bg_foliagesnd_resetinterval` @ `0x08154939`, `BG_EvaluateTrajectory: unknown trType: %i` @ `0x08154d01`, `BG_EvaluateTrajectoryDelta: unknown trType: %i` @ `0x08154d81`, `^3WARNING: "%s" is not a valid weapon file
` @ `0x08157600`, `spectatorclient can only be set to -1, or a valid client number` @ `0x08157ba0`, `Client Dvar Value` @ `0x0815809c`, `Dvar %s has an invalid dvar name` @ `0x081580c0`, `USAGE: player deactivateReverb("priority", fadetime = 0);
Valid priorities are "snd_enveffectsprio_level" or "snd_enveffectsprio_shellshock", fadetime is the time spent fading to the next lowest active reverb priority level in seconds
` @ `0x08158300`, `USAGE: player deactivatechannelvolumes("priority", fadetime = 0);
Valid priorities are "snd_channelvolprio_holdbreath", "snd_channelvolprio_pain", or "snd_channelvolprio_shellshock", fadetime is the time spent fading to the next lowest active reverb priority level in seconds
` @ `0x08158560`, `deactivatereverb` @ `0x081588f3`, `deactivatechannelvolumes` @ `0x08158916`, `setclientcvar` @ `0x081589ee`, `"%s" is not a valid hitloc damage table
` @ `0x08159681`, `"%s" is not a valid value for hudelem field "%s"
Should be one of:` @ `0x08159960`, `Invalid entity passed to hudelem setplayernamestring()
` @ `0x08159a60`, `Invalid entity passed to hudelem setplayernamestring(), entity is not a client
` @ `0x08159aa0`, `Invalid entity passed to hudelem setgametypestring()
` @ `0x08159b00`, `Invalid gametype '%s'
` @ `0x08159b36`, `Invalid mapname passed to hudelem setmapnamestring()
` @ `0x08159b60`, `Invalid map name passed to hudelem setmapnamestring(), map not found
` @ `0x08159ba0`, `setvalue` @ `0x0815a131`, `Too many different hintstring values on weapons. Max allowed is %i different strings` @ `0x0815a4c1`, `Too many different hintstring key values on trigger_use entities. Max allowed is %i` @ `0x0815af01`, `only valid on players; called on entity %i at %.0f %.0f %.0f classname %s targetname %s
` @ `0x0815b0e0`, `Dvar Value` @ `0x0815b34f`, `Dvar %s has an invalid dvar name` @ `0x0815b360`, `failed to link entity since parent model '%s' is invalid` @ `0x0815b600`, `List of valid hint type strings
` @ `0x0815b8c0`, `%s is not a valid hint type. See above for list of valid hint types
` @ `0x0815b960`, `Too many different hintstring values. Max allowed is %i different strings` @ `0x0815ba20`, `effect id %i is invalid
` @ `0x0815c75f`, `CloseFile failed, invalid file number %i
` @ `0x0815d120`, `FPrintln failed, invalid file number %i
` @ `0x0815d1e0`, `freadln failed, invalid file number %i
` @ `0x0815d2a0`, `freadline failed, invalid file number %i
` @ `0x0815d360`, `freadline failed, invalid argument number %i
` @ `0x0815d3a0`, `setteamfortrigger: invalid team used must be %s, %s or %s` @ `0x0815d4c0`, `getcvar` @ `0x0815d6e5`, `getcvarint` @ `0x0815d6ed`, `getcvarfloat` @ `0x0815d6f8`, `setcvar` @ `0x0815d705`, `makecvarserverinfo` @ `0x0815dbf4`, `isvalidgametype` @ `0x0815dc38`, `FxScheduler::PlayEffect called with invalid effect
` @ `0x0815f660`, `^1FX Error, invalid primitive template for effect '%s'
` @ `0x0815f940`, `variance` @ `0x08160285`, `^1FX bounding box mins / maxs invalid for effect '%s'
` @ `0x08160420`, `^1ERROR: No languages available because no localized assets were found
` @ `0x08160800`, `Cvar_Set` @ `0x08160d06`, `Dvar_Set` @ `0x08160d0f`
- `0x080b8108` `Info_ValueForKey` (chunk 30): string "Info_ValueForKey: oversize infostring"; walks \key\value pairs, copies the value into one of two 0x2000 static buffers; matched string(s): `Info_ValueForKey: oversize infostring` @ `0x0814f8c1`
- `0x080b8318` `Info_RemoveKey` (chunk 30): string "Info_RemoveKey: oversize infostring" (0x400 limit); removes "\key\value" with matching key (case-sensitive strcmp) in place; matched string(s): `Info_RemoveKey: oversize infostring` @ `0x0814f901`, `Info_RemoveKey_Big: oversize infostring` @ `0x0814f941`
- `0x080b844a` `Info_RemoveKey_Big` (chunk 30): string "Info_RemoveKey_Big: oversize infostring" (0x2000 limit); same as Info_RemoveKey with 8 KiB buffers; matched string(s): `Info_RemoveKey_Big: oversize infostring` @ `0x0814f941`
- `0x080b85ce` `Info_SetValueForKey` (chunk 30): string "Info_SetValueForKey: oversize infostring" and key rejection messages; removes the key, then appends "\key\value" (value stripped of '\\', ';', '"'); matched string(s): `Info_SetValueForKey: oversize infostring` @ `0x0814f981`
- `0x080cd515` `inflate` (chunk 36): zlib counterpart by structure, struct offsets and constants: strings 'unknown compression method','invalid window size','incorrect header check','need dictionary','incorrect data check'; matched string(s): ` inflate 1.1.4 Copyright 1995-2002 Mark Adler ` @ `0x08150ba0`
- `0x080d6f56` `BG_AnimationForString` (chunk 39): Returns the animation_t whose name matches, or raises an error and returns 0. Evidence: string "BG_AnimationForString: unknown player animation '%s'" at 0x08152820.; matched string(s): `BG_AnimationForString: unknown player animation '%s'` @ `0x08152821`
- `0x080d75f8` `BG_ParseConditionBits` (chunk 39): Parses one comma-terminated condition expression ("a AND b NOT c", "none", "all", "MINUS"/"NOT") from the current line into a 64-bit mask. Words resolve first against the script-defined values (g_0860dba0/g_0860e020) and then against the condition's string table. Only the first dword of result is cleared on entry; the local mask persists between terms (quirks preserved). Evidence: strings "BG_ParseConditionBits: unexpected end of condition" and "...: unexpected '%s'".; matched string(s): `BG_ParseConditionBits: unexpected end of condition` @ `0x08152940`, `BG_ParseConditionBits: unexpected '%s'` @ `0x08152980`
- `0x080d7974` `BG_ParseConditions` (chunk 39): Parses the condition list of one script item up to "default" or end of line. Each condition name resolves through g_08184c00; bit-flag conditions go to BG_ParseConditionBits, value conditions read one more token. The two value dwords persist across conditions (stale value[1] for value type). Evidence: strings "BG_ParseConditions: no conditions found" and "...expected condition value...".; matched string(s): `BG_ParseConditions: expected condition value, found end of line` @ `0x081529c0`, `BG_ParseConditions: no conditions found` @ `0x08152a00`
- `0x080d82b8` `BG_AnimParseAnimScript` (chunk 39): Parses mp/playeranim.script (loaded once into g_0860e4c0) into scriptData: sections "defines" (set <type> <name> = <bits>), "animations", "canned_animations", "statechanges" and "events". Depth/stack bookkeeping: depth 0 = section level, then state / move type / item / commands. The "unexpected end of file" error is raised with a format that has %s but no argument (as in the binary). While parsing, g_0860b424/g_0860e4a8/g_0860e4ac are set; g_08184860 is cleared at the end. Evidence: strings "BG_AnimParseAnimScript", "mp/playeranim.script" (g_08184860), many "BG_AnimParseAnimScript: ..." errors.; matched string(s): `BG_AnimParseAnimScript` @ `0x08152ccb`, `BG_AnimParseAnimScript: unexpected end of file: %s` @ `0x08152d00`, `BG_AnimParseAnimScript: unexpected '%s'` @ `0x08152d40`, `BG_AnimParseAnimScript: expected condition type string` @ `0x08152d80`, `BG_AnimParseAnimScript: can not make a define of type '%s'` @ `0x08152dc0`, `BG_AnimParseAnimScript: expected condition define string` @ `0x08152e00`, `BG_AnimParseAnimScript: expected '=', found end of line` @ `0x08152e40`, `BG_AnimParseAnimScript: expected '=', found '%s'` @ `0x08152e80`, `BG_AnimParseAnimScript: expected 'state'` @ `0x08152ec0`, `BG_AnimParseAnimScript: expected state type` @ `0x08152f00`, `BG_AnimParseAnimScript: expected '{'` @ `0x08152f40`, `BG_AnimParseAnimScript: internal error` @ `0x08152f80`, `BG_AnimParseAnimScript: exceeded maximum items per script (%i)` @ `0x08152fc0`, `BG_AnimParseAnimScript: exceeded maximum global items (%i)` @ `0x08153000`, `BG_AnimParseAnimScript: expected 'statechange', got '%s'` @ `0x08153060`, `BG_AnimParseAnimScript: expected <state type>` @ `0x081530a0`, `BG_AnimParseAnimScript` @ `0x0815715f`
- `0x080d99dc` `BG_GetAnimationForIndex` (chunk 39): Returns animations[index] of the parse target (g_0860b424); raises an error when out of range. Evidence: string "BG_GetAnimationForIndex: index out of bounds" at 0x08153140.; matched string(s): `BG_GetAnimationForIndex: index out of bounds` @ `0x08153141`
- `0x080df39e` `BG_CanItemBeGrabbed` (chunk 40): strings "BG_CanItemBeGrabbed: index out of range (index is %i, eType is %i)" and "BG_CanItemBeGrabbed: IT_BAD"; matched string(s): `BG_CanItemBeGrabbed: index out of range (index is %i, eType is %i)` @ `0x08154c81`, `BG_CanItemBeGrabbed: IT_BAD` @ `0x08154cc5`
- `0x080df5d4` `BG_EvaluateTrajectory` (chunk 40): string "BG_EvaluateTrajectory: unknown trType: %i"; matched string(s): `BG_EvaluateTrajectory: unknown trType: %i` @ `0x08154d01`, `BG_EvaluateTrajectoryDelta: unknown trType: %i` @ `0x08154d81`
- `0x080df948` `BG_EvaluateTrajectoryDelta` (chunk 40): string "BG_EvaluateTrajectoryDelta: unknown trType: %i"; matched string(s): `BG_EvaluateTrajectoryDelta: unknown trType: %i` @ `0x08154d81`
- `0x080eb0dc` `BG_FillInAmmoItems` (chunk 43): String "\025BG_FillInAmmoItems: weapon def for default weapon could not be found!" at 0x081553c0. For item slots 0x81..0x82 with itemType 2, looks up the weapon named by the first word of item->name and stores its index/ammo/clip; falls back to "defaultweapon_mp" (index 0 def when missing).; matched string(s): `BG_FillInAmmoItems: weapon def for default weapon could not be found!` @ `0x081553c1`
- `0x080f2844` `BG_LoadWeaponDef` (chunk 45): string "BG_LoadWeaponDef: Could not find default weapon" (rodata 0x08155960), falls back to "defaultweapon_mp".; matched string(s): `BG_LoadWeaponDef: Could not find default weapon` @ `0x08155961`
- `0x080f8916` `G_GetPlayerViewOrigin` (chunk 46): string "G_GetPlayerViewOrigin: Couldn't find [tag_player] on turret" at 0x8157e20; matched string(s): `G_GetPlayerViewOrigin: Couldn't find [tag_player] on turret
` @ `0x08157e21`
- `0x080ff4ac` `Cmd_FollowCycle_f` (chunk 47): string 'Cmd_FollowCycle_f: bad dir %i'; matched string(s): `Cmd_FollowCycle_f: bad dir %i` @ `0x08158ec1`
- `0x08109096` `G_InitGame` (chunk 49): strings '------- Game Initialization -------', 'InitGame: %s\n', 'Jun 23 2006' at 0x0815xxxx in this body; matched string(s): `Hunk_Alloc can only be called in G_InitGame and the first few frames of G_RunFrame
` @ `0x0814ac41`, `Hunk_AllocAlign can only be called in G_InitGame and the first few frames of G_RunFrame
` @ `0x0814aca1`
- `0x0810c6b2` `G_SpawnTurret` (chunk 49): strings 'G_SpawnTurret: weapon ... isn't a turret', 'turret '%s' not precached', 'G_SpawnTurret: max number of turrets (%d) exceeded'; matched string(s): `G_SpawnTurret: max number of turrets (%d) exceeded` @ `0x0815ac81`, `G_SpawnTurret: weapon '%s' isn't a turret. This usually indicates that the weapon failed to load.` @ `0x0815ad00`
- `0x0811b56a` `SP_worldspawn` (chunk 51): SP_worldspawn (string "SP_worldspawn: The first entity isn't 'worldspawn'"): validates classname, then sets configstrings; matched string(s): `SP_worldspawn: The first entity isn't 'worldspawn'` @ `0x0815e741`
- `0x0811d49c` `G_ModelIndex` (chunk 52): format "model '%s' not precached" in the function.; matched string(s): `G_ModelIndex: overflow` @ `0x0815ec80`
- `0x0811fe90` `Bullet_Fire_Extended` (chunk 52): format string "Bullet_Fire_Extended: Too many resursions, bullet aborted\n" in the function (the misspelling is in the binary); scr_friendlyfire check.; matched string(s): `?Bullet_Fire_Extended: Too many resursions, bullet aborted
` @ `0x0815ed5f`
- `0x0812aeec` `FxScheduler::PlayEffect` (chunk 55): stores a helper seed, falls back to "fx/error.efx" (and reports) when the effect is NULL, then for every primitive template spawns count = (int)(range + 0.5) records: delay <= 0 spawns now via fn_0812b8a8, else queues a 0x50-byte FxScheduledEffect [REA decompile sub_0812aeec; Ghidra 12.2 pseudocode, cross-checked against disassembly where x87/float semantics mattered]; matched string(s): `FxScheduler::PlayEffect called with invalid effect
` @ `0x0815f660`
- `0x0812f6f4` `PrimitiveTemplate::ParseMaterials` (chunk 57): Evidence: string "PrimitiveTemplate::ParseMaterials called with an empty list!\n"; keys "shaders"/"shader".; matched string(s): `:PrimitiveTemplate::ParseMaterials called with an empty list!
` @ `0x0815fe3f`
- `0x0805785a` `BaseWindingForPlane` (chunk 100): string 'BaseWindingForPlane: no axis found' (0x08140060) referenced by this function; matched string(s): `BaseWindingForPlane: no axis found` @ `0x08140061`
- `0x08057b1e` `ClipWinding` (chunk 100): strings 'ClipWinding: points exceeded estimate' (0x081400a0), 'ClipWinding: MAX_POINTS_ON_WINDING' (0x081400e0); matched string(s): `ClipWinding: points exceeded estimate` @ `0x081400a1`, `ClipWinding: MAX_POINTS_ON_WINDING` @ `0x081400e1`
- `0x08106a90` `FinishSpawningItem` (chunk 104): string "FinishSpawningItem: %s startsolid at %s\n" is referenced by this function.; matched string(s): `FinishSpawningItem: %s startsolid at %s
` @ `0x0815a400`

## Naming hygiene

Duplicate names: 223; placeholders: 2; generic decompiler-style names: 0.
