# Windows executable comparison

Read-only examination of the supplied Call of Duty 2 executables. No game or installer executable was run. PE parsing and printable-string extraction were performed with Python 3 standard library in isolated mode (`python3 -I`); helper outputs and the script are under ignored `.local/cod2/win/`.

Evidence labels: **Observed** means directly read from bytes or supplied adjacent metadata; **Inferred** means interpretation; **Unknown** means not established by these checks.

## Artifact hashes and provenance

| Label | Supplied path (relative to game base) | Size | SHA-256 | Evidence |
|---|---|---:|---|---|
| A | `silent_install/game/CoD2MP_s.exe` | 1,974,272 | `8f482e647b250babe098c6e3deb10129f9a935d013e852c99689cf8dff0d12cc` | Observed; agrees with task-provided hash |
| B | `1 install/Setup/Data/CoD2MP_s.exe` | 1,703,936 | `df3642d578e6802826bb4a2e71abfa98c37104d72e4597f2e1c077b1d6c23d7f` | Observed; agrees with task-provided hash |
| C | `silent_install/Call.of.Duty.2.Custom.Patch-LuKeStorm/CoD2MP_s.exe` | 1,974,272 | `9fbadab9fe71a254e5b40b498c6c58069ff62b59e99c1f4f02064ec191a21ad9` | Observed; user supplied abbreviated hash prefix, full hash computed here; third-party modified comparison only |
| D | `2 patch/CallofDuty2Patchv1_3.exe` | 39,195,804 | `158dae73cae81b6d4b28d1294bff517d08e023b0017da2c6bb5a264f6d03d809` | Observed; installer not executed |
| Linux reference | `.local/cod2/lnxded-1.3/cod2_lnxded` | 1,318,204 | `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7` | Observed; supplied reference reports `pc_1.3_1_1`, built 2006-06-23 |
| SP A | `silent_install/game/CoD2SP_s.exe` | 1,761,280 | `2be375ec149c54470a6d2ce3b04b5edf4dd3e7a679ea5db58420a0841c1fa386` | Observed |
| SP B | `1 install/Setup/Data/COD2SP_s.exe` | 3,139,086 | `cf08e440b6147e3b049d47550036766b7f8fd627b4aa5013911432f1d703f918` | Observed |

## Build identity and PE characteristics

- **A — observed:** PE32 i386 (`Machine 0x14c`), COFF timestamp `1146528591` = 2006-05-02 00:09:51 UTC. Embedded strings include `pc_1.3_1_1`, `Call of Duty 2 Multiplayer`, and PDB path `c:\builds\pc_1.3_1_1\pc\cod2\pc\CoD2MP_s.pdb`. Adjacent `version.inf` says `ExtVersion=1.3`, `IntVersion=2.3`, `Language=255`, `QA=4216`.
- **B — observed:** PE32 i386, COFF timestamp `1128846559` = 2005-10-09 08:29:19 UTC. Adjacent `version.inf` says `ExtVersion=1.0`, `IntVersion=15.1`, `Language=2`, `QA=3922`; the supplied label identifies it as German retail. No `pc_1.3_1_1` string was found in B.
- **C — observed:** PE32 i386, same timestamp, section layout, sizes, imports, and selected version/build strings as A. Its SHA differs from A; see byte comparison below. No separate version.inf was associated with C.
- A/B/C each have `.text`, `.rdata`, `.data`, `.rsrc`. A and C section records: `.text` VA `0x1000`, virtual size 1,645,581, raw 1,646,592, entropy 6.695; `.rdata` raw 229,376, entropy 5.894; `.data` raw 77,824, entropy 3.366; `.rsrc` raw 16,384, entropy 4.353. B `.text` raw 1,409,024, entropy 6.695; `.rdata` raw 200,704, entropy 5.917; `.data` raw 73,728, entropy 3.149; `.rsrc` raw 16,384, entropy 4.351.
- **Observed:** A/C import DLLs are `WINMM`, `WSOCK32`, `mss32`, `d3d9`, `DSOUND`, `KERNEL32`, `USER32`, `GDI32`, `ADVAPI32`, `SHELL32`; B has the same set with case differences. This is ordinary-looking uncompressed executable layout; entropy alone is not a packer detector. Sections are named conventionally and there is no UPX section or obvious protector marker among extracted printable strings.
- **Unknown:** PE version-resource fields were not decoded by the small standard-library parser, and Authenticode signature status was not independently validated. No SecuROM/SafeDisc signature marker was observed in A/B/C strings, but absence of printable markers does not exclude protection. There is no basis here to claim cryptographic authenticity or publisher provenance.

## A versus C and patch installer D

A and C have identical length (1,974,272) and matching PE header/section/import metadata. A bytewise comparison found **7 difference regions**, all short, at file offsets `0x33ee0` (1 byte), `0x33fc2` (5), `0x65190` (2), `0x65209` (2), `0x664cd` (2), `0xb3b7c` (2), and `0xb8dd0` (1). Combined changed-byte count is 15. No section boundaries or sizes differ. The distribution is consistent with a lightly altered executable; it does not identify the semantic purpose of each modification. **Inferred:** C appears derived from the same 1.3 executable lineage as A. **Unknown:** whether these bytes implement a crack, what they change, or whether A is publisher-original.

D is a PE32 installer with `.text/.rdata/.data/.rsrc`; strings identify `Call of Duty(R) 2 Patch 1.3.msi`. `7z` and `bsdtar` were not available in this environment, and no standard listing/extraction path was available without executing it. Therefore its embedded payload was not extracted and no hash comparison between a D payload and A was possible. **Unknown:** whether D contains byte-identical A, or another official 1.3 MP executable.

## Windows MP versus Linux dedicated server strings

The raw printable ASCII set was extracted from A and the Linux server (`strings`-equivalent minimum length 5). Exact, case-sensitive line-set comparison:

- A unique strings: **9,617**; Linux server unique strings: **3,853**.
- Exact shared strings: **3,512**.
- Jaccard overlap: **35.27%** (`3512 / 9958` union strings).
- Coverage: **36.52%** of A’s set and **91.15%** of the Linux server’s set.

Shared anchors include `SV_BuildClientSnapshot: bad gEnt`, `SV_ArchiveSnapshot: ignoring snapshot because it overflowed.`, `SV_SvEntityForGentity: bad gEnt`, `G_Spawn: no free entities`, `G_CallSpawn: NULL classname`, `sv_mapRotation`, `sv_maxclients`, `sv_allowDownload`, `g_gametype`, `g_allowVote`, `g_friendlyfireDist`, and `g_synchronousClients`. A contains the `pc_1.3_1_1` build marker; the Linux reference is identified as `pc_1.3_1_1` and built June 23, 2006. **Inferred:** A and the Linux dedicated server plausibly derive from the same 1.3.1.1 engine/source revision, with expected platform/MP dedicated-server string-set differences. String presence is not proof of identical code or a complete build match.

## Conclusions and limitations

**Observed:** A is a Windows MP executable carrying explicit 1.3.1.1 build-path/version markers and its adjacent manifest says 1.3; B is the earlier 1.0 German-retail build. A and C are the same size and structurally identical, with only 15 differing bytes across seven regions. A’s substantial exact server-side string overlap with the Linux 1.3.1.1 dedicated server supports a shared revision lineage. **Inferred:** A is very likely a 1.3.1.1 executable, and C is a minimally modified derivative. **Not established:** A’s official/unmodified status, exact semantics of C’s byte edits, whether the official 1.3 installer D embeds A, decoded PE version resources/signatures, or code-level identity with the Linux server. The analysis is static and string/metadata based; it did not execute binaries or perform full disassembly/decompilation.
