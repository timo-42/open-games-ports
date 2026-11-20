# cod2_lnxded 1.3 — source inventory and language analysis

Checked 2026-11-17 using a read-only original-data source. The source path is machine-specific and is not included here.

## Source set

Base path: `<user-supplied Call of Duty 2 source root>/`

| Artifact | SHA-256 | Notes |
|---|---|---|
| `cod2-lnxded-1.3-06232006.tar.bz2` | `36b6f433ce67dae4d9b03e2013d328c5bd0428aeae525d8bac58b06dbb9c6cdb` | Official 1.3 Linux dedicated server (owner `icculus`, 2006-06-23) |
| ↳ `cod2_lnxded` | `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7` | Reconstruction target |
| ↳ `main/iw_15.iwd` | `2c4b1797da9cc626b3dae99befa41dfbba4894dc14ce1199f5a459b88678f3ef` | Identical to `silent_install/game/main/iw_15.iwd` |
| ↳ `pb/pbsv.so` | `91b6585be25934e330d14cbf11ff2d17fa01022dab9560b977d74e991f90a62a` | PunkBuster (third-party, out of scope) |
| ↳ `pb/pbcl.so` = `pbcls.so` | `aa7375f5d5132e4289034fd6260145dc216911ef67409075701d8be458ba2bb5` | PunkBuster |
| ↳ `pb/pbag.so` = `pbags.so` | `f4e67a38e3e3db7b5b21ee1d3dd6d337b470fb7aa18253b00b25e37d1ae07397` | PunkBuster |
| `2 patch/CallofDuty2Patchv1_3.exe` | `158dae73cae81b6d4b28d1294bff517d08e023b0017da2c6bb5a264f6d03d809` | Windows 1.3 patch |
| `silent_install/game/CoD2MP_s.exe` | `8f482e647b250babe098c6e3deb10129f9a935d013e852c99689cf8dff0d12cc` | Patched Windows tree (`version.inf` ExtVersion 1.3); originality unconfirmed |
| `1 install/Setup/Data/CoD2MP_s.exe` | `df3642d578e6802826bb4a2e71abfa98c37104d72e4597f2e1c077b1d6c23d7f` | Loose German retail 1.0 layout (`version.inf` 1.0, Language=2) |

Game data: `iw_00..iw_14.iwd` + `localized_german_iw00..11.iwd` (base, German), `iw_15.iwd` (1.3).
`.iwd` files are ZIP archives (deflate, method 8). `iw_15.iwd` holds 27 `.gsc` scripts
(`maps/mp/gametypes/{tdm,sd,hq,...}.gsc`, map scripts); `iw_07.iwd` holds 341 `.gsc`.

## Binary facts (observed)

- ELF32 i386 `EXEC`, entry `0x804a6c0`, stripped (no `.symtab`); 160 imported dynamic symbols.
- `.comment`: 159 × `GCC 3.3.6 (Ubuntu 1:3.3.6-8ubuntu1)`, 4 × `GCC 3.4.5 20050809` (crt objects).
- NEEDED: `libdl.so.2 libpthread.so.0 libstdc++.so.5 libm.so.6 libgcc_s.so.1 libc.so.6`.
- `.text` 998,480 B; `.rodata` 142,044 B; `.data` 47,104 B; `.bss` 9,332,952 B.
- REA/Ghidra 12.2 inventory: 4,830 procedures (151 external), 4,265 defined strings
  (Evidence `ev_c1739b023716be341638dbe441e4da4a64d2406c1cc69a4aed7b4e7ef06651b5`).
  Full pseudocode export: Evidence `ev_1ab5b9fe…`, `ev_9ed02e57…` (4,679 written, 0 failed),
  kept under `.local/cod2/decomp/`.
- Version strings: `pc_1.3_1_1`, `Jun 23 2006`, `linux-i386`, `CoD2 MP`,
  `Linux Quake3 Dedicated Server [%s %s]`; master `cod2master.activision.com`.

## Language conclusion

**C++ (GNU g++ 3.3.6), written predominantly in C style.** Evidence:

- every `.eh_frame` CIE (21) has augmentation `zP`/`zPL` (personality routine), which GCC
  emits only for C++ translation units: 2,453 FDEs without LSDA (624,889 B) and 509 with LSDA
  (136,451 B);
- libstdc++ imports (`std::string`, `operator new/delete`, `__cxa_*`, `__gxx_personality_v0`);
- surviving source path `universal/com_memory.cpp`;
- RTTI type names for the effects/particle classes (`Effect`, `Emitter`, `Particle`,
  `OrientedParticle`, `Cloud`, `Flash`, `Light`, `Line`, `Tail`, `Cylinder`, `Ccursorhint`).

Engine lineage (inferred): id Tech 3 / IW engine — `SV_`, `FS_`, `NET_`, `Netchan_`, `MSG_`,
`Cbuf_`, `Cmd_`, `Hunk_`, `Com_`, `CM_`, `G_`, `BG_`, `Scr_` prefixes, `CodeCallback_*`
script entry points. Game logic is in GSC scripts loaded from the `.iwd` packs.

## Limitations

- Function names are not present; names in the reconstruction are observed (from strings),
  inferred (structure/lineage) or unknown, as tagged per function in `symbols/`.
- No Windows executable has been compared function-by-function yet.
