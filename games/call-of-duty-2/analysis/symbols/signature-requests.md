# Signature-fix requests: resolution log

Source: the 87 request files `.local/cod2/migrate/requests/*.json` (`{addr, current, proposed, evidence}`, filed by the
link-wave lanes that called a function through a cast marked `SIGFIX-REQUEST`, or through the `SIGFIX_ARG` /
`PROVISIONAL_ARG` / `PtrCast` shims). Binary: `cod2_lnxded` 1.3 (SHA-256 `05e774a3...a3a7`), checked with `objdump -d -M intel`
only. Every verdict below was taken from the disassembly (return register use by the callers, argument stack slots and
widths); const-ness is not observable, so the type that lets every caller compile without a cast was chosen.

Result: 87 requests, 0 rejected.

| Class | Count |
| --- | --- |
| Applied in this change (owner JSON + definition changed, requester casts/shims removed): L01-1 L02-1 L05-2 L06-2 L06-3 L06-4 L07-1 L07-2 L07-3 L07-4 L07-6 L07-7 L12-1 L12-4 L13-1 L13-2 L14-1 L14-2 L14-3 L14-4 L15-3 L15-5 L15-6 L15-8 L16-1 L16-3 L16-4 L16-8 L16-9 L17-1 L22-2 L24-2 | 32 |
| Already applied in the tree before this change (23 marked `applied`, plus 27 whose owner JSON/symbol was already correct; only requester casts/shims were left and are now removed) | 50 |
| No signature change (L02-2 informational, L13-4 and L18-2 type promotion deferred, L18-3 obsolete, L19-1 accepted) | 5 |
| Rejected | 0 |

Link result after the change: `link_test.py` exit 0, 0 undefined symbols, 0 duplicate definitions, 71/71 chunks compile
(`recon_merge.py`: compile_pass 71, compile_failures 0).

## Table

"applied-in" names the owner chunk (JSON + definition) first, then the requester chunks whose casts/shims were removed.
Proposal differences are noted in the evidence column.

| addr | function | requester | verdict | evidence | applied-in |
| --- | --- | --- | --- | --- | --- |
| 0x080d4f7c | Sys_Main -> main | L00-1 | already applied | `_start` pushes it as the `__libc_start_main` main argument | 102 |
| 0x080ac460 | Hunk_FreeTempMemory | L00-2 (003) | owner already `void *`; cast removed | callers pass `cmod_lumpBuffer` and other pointers | 003 |
| 0x08a700f8.. | libc/libstdc++ import stubs | L00-3, L07-9, L14-5, L18-1, L19-2 | already applied; shims removed | `recon_merge.py` skips >= 0x08a70000 and chunk 062 signatures are blank; the generated header declares no libc name. Removed the `#define cod2_plt_*` / `cod2_proto_*` macro blocks (038/039/040: 132 names each, 059-062, 001, 037, 102) and the `tolower` workaround in 046; stale include-order comments in 023/024 | 001, 023, 024, 037-040, 046, 059-062, 102 |
| 0x080561d6.. | VectorAdd/Sub/MA/Dot/Length/Scale | L00-4 | already applied | conflicts.json suffixes the chunk 041 copies; link: 0 duplicates | - |
| 0x0809177c | SV_GentityNumFromSvEntity | L01-1 (005) | applied | 0x0809177c-0x080917a9: computes the index, calls 0x080916f8 and returns with eax untouched (`call; leave; ret`) -> `struct gentity_s *` | 020; 005 |
| 0x0806c700 | NET_PollLoopPacket | L01-2 (006) | already applied; cm6 shim removed | `int`, tail call returns eax | 009; 006 |
| 0x080b408c | Dvar_RegisterFloat | L02-1 (007) | applied | Com_InitDvars pushes the float bit patterns as immediates; float and uint32 stack words are identical in cdecl. Float literals replace the `FloatBits`/`FLTBITS`/hex-word workarounds (every converted literal round-trip checked bit-exact) | 029; 007, 021, 040, 048, 100 |
| 0x080b49aa | Dvar_Register* (info) | L02-2 | no change | informational: Dvar_RegisterBool leaves bytes 1..3 of the dvar value as stack residue in the original; harness limitation, not a signature | - |
| 0x08079316 | ScrStr_LookupCString | L03-1 (012), L10-1 | already applied; casts removed | returns the string id in eax | 013; 012, 031 |
| 0x080ac088 | Hunk_AllocateTempMemoryHigh | L04-5 | owner already `void *`; casts removed | returns a buffer pointer in eax | 027; 013, 015 |
| 0x08075dd4,0x08075df4 | Scr_AddRefToValue/Scr_RemoveRefToValue | L05-1 | already applied | - | 012 |
| 0x0808b9dd | yyerror | L05-2 (018) | applied | 0x0808b9dd never reads `[ebp+8]`, but the bison call sites in yyparse push the message; `int yyerror(const char *msg)` | 019; 018 (`COD2_YYERROR` removed) |
| 0x08059186 | CM_ClusterPVS | L06-1 (020) | owner already `const unsigned char *`; cast removed | 0x080591b0..0x080591c7 returns a pointer into the cluster table | 004; 020 |
| 0x0813c11c | PB_InvokeCb15c | L06-2 (020) | applied | forwards three stack args to the callback and returns its eax; caller passes `(addr text, atoi(cl_punkbuster), char[0x30])` and compares the result with `"error\n"` | 061; 020 |
| 0x08097188 | SVC_RemoteCommand | L06-3 (021) | applied | the call at 0x08095df4..0x08095e1d pushes six dwords: netadr_t (5) + `msg_t *` | 022; 021 |
| 0x08062bc4 | Com_GetServerDObj | L06-4 (020/021) | applied | result consumed by `DObj_*` prototypes taking `struct DObj *`; two tags named the same pool slot. Locals typed `DObj_s *` in 023/046/048/101/102/104 retyped | 007; 020, 021, 023, 046, 048, 101, 102, 104 |
| 0x08090dac | SV_UpdateClientUsercmd | L07-1 (022) | applied | 7-dword usercmd copy; callers pass `usercmd_s *` (`const` accepts all) | 020; 022 |
| 0x080a1d4a | FS_GetModList_080a1d4a | L07-10 (050) | already applied | symbols.json names it FS_GetFileList; chunk 050 already calls that | 024 |
| 0x0805d552, 0x080982de | Float_SelectNonNegative duplicate | L07-11 | already applied | symbol is `Float_SelectNonNegative_0805d552`; link: 0 duplicates | 005 |
| 0x08060ba2 | Com_BeginRedirect | L07-2 (022) | applied | flush argument stored as the redirect callback; `void (*)(char *)` | 006; 022 |
| 0x080b8c98 | Rotate_Eased | L07-3 (023), L13-2 (042), L14-4 (046) | applied | 0x080b8c98..0x080b8d2e: `[ebp+8]` = float *dst, `[ebp+0xc]` is forwarded unchanged as the middle float of the rotation triple (a float yaw, viewangles[1]), `[ebp+0x10]` = t (fldz compare), `[ebp+0x14/0x18]` = scales. Merged the three proposals: `(float *dst, float yaw, float t, float angleScale, float blendScale)` | 030; 023, 042, 046 |
| 0x08108e72 | Client_GetUnk2748Addr | L07-4 (022/023) | applied | `imul 0x28a4; add [0x864c380]; add 0x2748; ret` -> pointer to the embedded clientState; callers memcpy 0x5c bytes from it | 048; 022, 023 |
| 0x0805d328 | Sweep_TraceEntityWithOffset | L07-5 (023) | owner already `int`; shim removed | eax tested after the call at 0x0809d4c6 | 005; 023 |
| 0x08059098 | CM_TransformedPointContents | L07-6 (023), L14-3 (045) | owner already `uint32`; now `const float *` for point/origin/angles | epilogue `mov eax,[ebp-4]`; result OR-ed into the contents mask; both callers pass const vectors | 004; 023, 045 |
| 0x080a3750 | FS_StatPath | L07-7 (024), L12-4 (038) | applied (differs from proposal) | `push 3; call __xstat; leave; ret` returns the __xstat result. Callers pass either the i386 `struct stat` (024) or the chunk-private 0x58-byte `cod2_stat_t` (038), so the buffer is `void *statBuf` instead of a type only one chunk can name | 025; 024, 038 |
| 0x080f9976 | G_SayToEligibleClients | L07-8, L14-6 | already applied | forwards the voicepacket pointer to SV_QueueVoicePacket | 046 |
| 0x080b11e8 | Float_Clamp_080b11e8 | L08-1 (027) | owner already `float`; cast removed | second select-helper call is followed by `leave; ret`, st0 returned | 028; 027 |
| 0x08060b0a | Com_Fwrite | L09-1 (028) | owner already `size_t`; cast removed | `call fwrite@plt; leave; ret`, eax returned | 006; 028 |
| 0x080bc2b0 | Vec4_Set duplicate | L09-2 | already applied | symbol is `Vec4_Set_080bc2b0` | 031 |
| 0x08104a98 | Float_SelectIfNonNegative duplicate | L09-3 | already applied | symbol is `Float_SelectIfNonNegative_08104a98` | 048 |
| 0x080b1e18 | Dvar_ParseValueWord | L09-4 | already applied | - | 029 |
| 0x080606b6 | Cmd_AddCommand | L12-1 | applied | defined with `void (*)(void)`; 58 call sites used `(void *)` or the `COD2_CMD_FN` adapter (removed) | 006; 007, 009, 019, 027, 028, 038 |
| 0x080ebd22, 0x080eb9a4, 0x080ec514 | BG_GetCurrentWeapon/BG_GetWeaponDef/BG_GetWeaponAmmoRoom | L12-3 (039/040) | owner already canonical; shims removed | chunk 043 JSON uses `playerState_s` / `weaponDef_s` | 043; 039, 040, 051, 053 |
| 0x080d484e | Sys_GetEvent | L12-5 | already applied | hidden sret, `ret 4` | 038 |
| 0x080d520a.. | NET_BaseAdrToString/Sys_SendPacket/Sys_IsLANAddress | L12-6 | already applied | `netadr_s` passed by value (5 dwords) | 038 |
| 0x080de088 | Mantle_TryStart | L13-1 (042) | applied | PM_SingleStep (0x080e8934) passes `lea [ebp-0xa8]`, the same pml block later given to Mantle_UpdateMove; the body only reads it as the float eye block | 040; 042 |
| 0x080e69f4 | PM_UpdateLean prototype | L13-3 | already applied | explicit function-pointer parameter; present in cod2_prototypes.h | 042 |
| 0x08627080.. | private view types (clientTurnView, pm_dispatch, bgItem043) | L13-4 | no change (deferred) | type promotion that needs owner-chunk layouts; no signature impact | - |
| 0x08091dda | SV_GetClientUsercmd | L14-1 (046) | applied | 7-dword copy into `&client->cmd` (usercmd_t, 0x1c bytes) | 020; 046 |
| 0x08092472 | SV_CheckClientNetAddress | L14-2 (046) | applied | 0x080924ae `call 0x0806c57a; leave; ret` returns NET_IsLocalAddress; ClientUserinfoChanged stores eax into gclient+0x2700 | 021; 046 |
| 0x080f0bde | PM_Weapon_SwayPhase | L14-7 | already applied | argument is `playerState_s *` (byte at +8 is bobCycle) | 044 |
| 0x08092302 | SV_MapFileExists | L15-3 (047) | applied | `[ebp+8]` goes to 0x0808bd32 and into `"maps/mp/%s.%s"`: a map name | 021; 047, 104, 106 |
| 0x080765b6 | Scr_ResetAll | L15-5 (049) | applied | `mov eax,[ebp+8]; mov [ebp-1],al` then unused | 012; 049 |
| 0x0811e394 | Ent_UpdateTimeScaled | L15-6 (049) | applied | 0x0811e3b9 `call 0x0809208a; leave; ret`: tail-returns the SV_DObjBindingSetWeight result; second stack word forwarded unchanged | 052; 049 |
| 0x087a2428, 0x087a24cc | globals typed `byte *` | L15-8 (048) | applied | Dvar_RegisterInt/Bool results; readers use +8. Changed the `cod2_globals.h` declarations (generated by `.local/cod2/canon-work/gen_globals_h.py`, which must pick the type up from chunk_045.json) | include/cod2_globals.h, 045 JSON; 045, 046, 048, 052 |
| 0x080f41c8 | Print_UnescapeAndEmit | L16-1 (051) | applied | 0x080f42c2..0x080f42d9 ends with `call ScrStr_InternCString; leave; ret` | 045; 051 |
| 0x080921ec | ScrEntity_CallVoidB | L16-3 (052) | applied (`int`, as proposed) | 0x080921ec..0x0809220d returns the DObj_GetRotTransArray result (bone array base, used as `base + tagIndex*0x20`) | 021; 052 (`c52_ScrEntity_CallVoidB` removed) |
| 0x08092780 | SV_SetConfigstring | L16-4 | applied | only reads the value; `const char *` | 021; 050, 051, 052 |
| 0x08079de0 | ScrStr_AssignRef | L16-8 | applied | `movzx eax,WORD PTR [eax]` at 0x08079e03 and a 16-bit store: `uint16 *` (body no longer sign-extends) | 013; 046, 048, 050, 051, 052, 104 |
| 0x0811a3dc | G_SpawnString | L16-9 | applied | `out` points into the spawn-var table, never written through: `const char **` | 051; 049, 050, 104, 107 |
| 0x081101b4 | G_NewScriptString | L17-1 (053) | applied | 0x081101b4 pushes the string and a constant 1 and returns eax of 0x08079e58; callers store a 16-bit handle | 050; 053 |
| 0x08131e8a | ParseNode layouts | L18-2 | no change (deferred) | three chunk-private layouts of one tag; type promotion, no signature impact | - |
| 0x081314a0.. | PrimitiveTemplate function map names | L18-3 | no change (obsolete) | `recon/globals/{symbol_map.json,cod2_aliases.ld,cod2_data.S}` contain no PrimitiveTemplate names; link test resolves the `fx_primitive_s` names | - |
| 0x08124fd4.. | Particle/Tail member functions | L19-1 (059) | no change (accepted) | names stay; chunk 059 reaches them through asm-labelled declarations | 054 |
| 0x08a5c9e0 | g_08a5c9e0 | L19-3 | already applied (data image) | present in `.local/cod2/globals/symbol_map.json`; the extern stays in chunk_059_types.h (cod2_globals.h is generated) | - |
| 0x08092002.. | SV_DObj*/ScrEntity_CachedSkelOp/SV_GetSkelForModel (`int *ref`) | none filed (SIGFIX_ARG sites in 050/052) | applied | every body starts `Com_GetServerDObj(*ref)`, i.e. reads the entity number at +0 of a `gentity_s`; callers pass entities | 021, 020; 052, 105, 106 |
| 0x0811e5fa | Ent_SetNodeOrigin (`float *out`) | none filed (046 PROVISIONAL_ARG) | applied | callers pass the origin buffer | 052; 046 |
| 0x08133756 | FX_GetBoneOrientation | none filed (059 note, 055 adapter) | applied | both call sites (0x0812afbf, 0x08136f9b) push `(bolt, FxPlacement *)`; the stub ignores them; chunk 059 had called it with no arguments | 058; 059, 055 |
| 0x08060b9c | Noop_08060b9c | none filed (038 cast) | applied | empty body, caller pushes the field_t pointer | 006; 038 |
| 0x0810a5cc, 0x0809d884, 0x080591f8, 0x08121f4e | G_TracePointEx (`const char *surface`, const vectors), SV_Trace (const vectors), cm_vec3_sub_c (const), G_TraceUseRay (`const char *surface`) | none filed (050/052/053/104 shims) | applied | const-ness and the surface-name argument (g_08187c0c is a string) | 049, 023, 004, 053 |
| 0x0811ad7e, 0x0811af3c | Scr_SetEntityFieldGeneric/Scr_AddEntityFieldValue | none filed (046/048 shims) | applied | called with a gclient or hudelem base, so the first argument is the object base (`void *`) | 051; 046, 048 |
| 0x080f8514, 0x080f8576 | Scr_SetPlayerField/Scr_GetPlayerField | none filed (051 shims) | applied | argument is the `gclient_s *` | 046; 051 |
| 0x08103414 | HudElem_FreeAllForOwner | none filed (046 shim) | applied | compares `clientNum` with `*owner` (the entity number at +0) | 047; 046 |
| 0x081047fe | HudElem_LookupMethod | none filed (050 shim) | applied | only reads the name (`const char **`) | 048; 050 |
| 0x0810a6ba | G_NoopCallback | L24-2 (104) | applied | empty body (`push ebp; mov ebp,esp; pop ebp; ret`); GScr_print3d pushes `(origin, rgba, scale, text)` | 049; 104 |
| 0x0811f232.. | G_AddEvent/G_FreeEntity/Ent_* (`game_entity_s`) | L24-3 (104) | already applied; `GE104` shim removed | chunk 052 JSON already uses `gentity_s *` | 052; 104 |
| 0x08103618 | HudElem_AddPackedFieldString etc. | L24-4 (104) | already consistent | JSON and definition agree (link: 0 signature mismatches); the 4th count argument pushed by the wrappers is ignored by the callee | 048 |
| 0x081104d2, 0x0811050c | GScr_GetEntityFromRef/GScr_GetPlayerEntity | L26-1 (106) | already applied; macros removed | owner JSON returns `gentity_s *` | 050; 106 |
| 0x0810a622, 0x0810a57e | G_TraceIsClearPoint/Box | L15-1, L15-2, L25-1 | already applied; `_I` macros removed | return `int` | 049; 047, 048, 105 |
| 0x08084678 | Scr_IsInitialized | L15-4, L17-2 | already applied; `_I` macro and 053 adapter removed | byte argument stored to a local | 017; 049, 053 |
| 0x08079e58 | ScrStr_InternCStringFlag1 | L15-7 | already applied | - | 013 |
| 0x0810a528, 0x08120ff6, 0x08084d40, 0x08085306, 0x080c2d0a, 0x081063d4, 0x08117f50, 0x0811eaa4, 0x080c2c18, 0x0811d598 | G_TraceBox, Vec3_LengthDiscard, Scr_GetVector, Scr_AddVector, XAnim_InitSlotRecord, Item_ThrowFromPlayer, Scr_AddEntityOrUndefined, G_AllocTempEntity, XAnim_CopyTreeSlots, G_ModelIsDefault | L16-5, L16-2, L16-6, L16-7, L22-1, L23-1, L23-3, L23-4, L23-5, L24-1 | already applied; remaining requester casts removed (102 declaration, 104 `G_ModelIsDefault` cast) | `XAnim_CopyTreeSlots` second argument: chunk 103 keeps a plain cast because its record stores the start as an `int` handle | 102, 103, 104 |
| gclient_s+0x26b4 | archiveTime field | L22-2 | applied | 0x080f8420 stores with `fistp DWORD PTR [ebx+0x26b4]`, 0x080f8460 loads with `fild`, 0x080f84a4 pushes it as an integer index: the canonical field is `int32` | include/cod2_canon.h; 046, 047, 048, 102 |
| 0x080ebf24, 0x080ebffe | BG_PlayerHasWeapon/BG_TakeWeapon | L23-2 (103) | already applied | chunk 043 JSON canonical | 043; 047, 048 |

## Notes on deviations from the proposals

* `FS_StatPath`: the proposal named `cod2_stat_t`, a chunk-private type; the shared prototype takes `void *`.
* `ScrEntity_CallVoidB`: returns the bone-array base address; kept `int` (the proposal) so the `tagIndex * 0x20 + result`
  arithmetic in chunk 052 compiles unchanged. The faithful type would be `struct DObjBone *`.
* `Rotate_Eased`: three requests (L07-3, L13-2, L14-4) differ only in parameter names; one signature was applied.
* `SVC_RemoteCommand`: the message pointer is unused in the body but is part of the call (six dwords).

## Residual adapters (not covered by any request; unchanged)

These are call-site adapter macros/casts for prototypes that no request file addresses. They no longer carry a
`SIGFIX-REQUEST` marker, because there is no request to track.

* chunk_053_types.h: `FxRange_Set`, `FX_GetEffectByName`, `Bullet_ApplySpread`, `Bullet_Fire_Extended`, `FxRange_Lerp`,
  `Model_RegisterByName`, `FX_ReturnZeroA`, `XModel_GetName`, `Bullet_FireFromEntity`.
* chunk_054_types.h: `IntPair_Init`, `FxRange_Random`, `Orient_VecWorldToLocal`, `FX_InitSampleRecord`, `FX_NoopA`,
  `FX_ReturnZeroB`, `FX_IsSegmentCulled`, `MsgStream_TransferBytes/Value`, `FXTEMPLATE`.
* chunk_055_types.h: `MsgStream_*`, `Curve_*`, `FxPool_FreeElement`, `FX_NoopB`, `FX_IsCulledByPlanes2`, `FX_DrawPrimitive`,
  `FxCurve_InitFromTemplate`.
* Plain C casts that remain because the requester holds the value as an `int`/private-view type, not because a prototype
  is wrong: chunk 051 `(kv_table_s *)&g_0864d6c8` (spawnVarState_s and kv_table_s are two views of one global),
  chunk 050 `(turret_input_s *)&tr` (turret_input_s is a view of trace_t), chunk 103 `XAnim_CopyTreeSlots` second argument,
  chunk 051 `(gitem_t *)BG_GetItemByIndex`, `(int)handle` first argument of `PlayerAnim_UpdateTree`/`PlayerView_Update`
  (chunks 046/048; the callee's first parameter is a DObj pointer typed `int`).
