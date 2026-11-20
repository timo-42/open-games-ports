# cod2_lnxded naming pass 2

Source: `analysis/subsystems/*.names2.json` (454 function and 613 global proposal rows over 437 / 478 distinct addresses; the same address is often proposed by several subsystem files). Mechanical renames only; selection by `tools/plan_naming_pass.py`, function records by `tools/apply_function_proposals.py` + `tools/apply_renames.py` + `tools/recon_merge.py`, globals by `tools/apply_global_renames.py`.

## Counts

| | applied | skipped |
|---|---|---|
| functions (437 addresses) | 311 | 126 |
| globals (478 addresses) | 467 | 11 |

Disagreeing proposals for one address were resolved in favour of the better-evidenced one (confidence observed > inferred > descriptive, then longer evidence, then file order): 5 functions, 112 globals. Rejected alternatives are listed in the table at the end.

## Rules applied

* `old` must equal the current name at that address (symbols.json); no mismatches were found.
* New name: valid C++ identifier, not a keyword, unique over all function and global names after the pass (swaps allowed), not already a word in recon sources/headers (comments ignored), not a libc/libstdc++ export or a system-header identifier.
* Function confidence: `observed`/`inferred` kept; proposal confidence `descriptive` (role/position labels) recorded as `unknown` with evidence prefixed `naming pass 2 (descriptive name)`.
* Global types: no `type_hint` was applied. Rewriting a declared type changes signedness/float conversions in the chunk code, which is not a mechanical rename (`SUPPORTED_RETYPE` in apply_global_renames.py is empty); hints remain in the names2 files.
* Five globals now carry the same name as an existing static alias at the same address (`entityStateFields`, `clientStateFields`, `scrVarPub_notifyArrayId`, `scrVarPub_freeEntList`, `level_clients`); the duplicate alias symbols collapse into one (same address, same size).

## Skipped functions

### C++ member functions (123)

recon_merge.py now accepts qualified `Class::method` names (stored in symbols.json, skipped in cod2_prototypes.h because members are declared in the class definitions). 115 of the 123 were already descriptive; the 8 placeholder-style or role-corrected members are renamed in chunk_054.cpp, chunk_054_types.h, .local/cod2/globals/cod2_data.S (mangled vtable symbols) and chunk_054.json. The other 115 keep their names; their naming tags are in the chunk records (mostly inferred from RTTI class hierarchy plus bodies; `fx_primitive_s::ParseMaterials/Models/*FxStrings` observed from .efx key strings). Unchanged unknowns now tagged inferred in chunk_054.json: `Effect::CopyFromTemplate` (slot 11), `Effect::SubmitToRenderer` (callers pass modes 1/4/6/7/8/9). `Particle::GetRayVisibility` (0x0812532a, slot 6, chunk_107) stays unknown in role beyond its caller Fx_GetRayAttenuation.

| addr | old | new | naming | evidence |
|---|---|---|---|---|
| 0x08124738 | Effect::vslot2 | Effect::OnExpire | inferred | vtable slot 2 (+8) per fx-anim.md 4.1; empty body; .rodata vtable 0x0815f548 |
| 0x0812473e | Effect::vslot4 | Effect::IsCulled | inferred | slot 4 (+0x10); returns 0 |
| 0x08124748 | Effect::vslot5 | Effect::Draw | inferred | slot 5 (+0x14); empty body |
| 0x0812540c | Particle::AppendDrawEntry | Particle::AppendBlocker | inferred | slot 7 (+0x1c) AddBlocker; appends {origin,size^2,1-alpha/255} to the blocker list read by FX_GetAttenuation (fx-anim.md section 10 correction) |
| 0x081271c2 | Cloud::vslot2 | Cloud::OnExpire | inferred | slot 2; empty override, vtable 0x0815f488 |
| 0x08127764 | Line::vslot2 | Line::OnExpire | inferred | slot 2; empty override, vtable 0x0815f448 |
| 0x08128192 | Emitter::vslot4 | Emitter::IsCulled | inferred | slot 4; returns 0, vtable 0x0815f388 |
| 0x0812819c | Emitter::vslot5 | Emitter::Draw | inferred | slot 5; submits mode 1 when flag 0x10 and prevSize != 0 |

### Other skipped

| kind | addr | old | proposed | reason |
|---|---|---|---|---|
| g | 0x087a2414 | g_087a2414 | dvar_g_gravity | address not in symbols.json |
| g | 0x087a2440 | g_087a2440 | dvar_g_synchronousClients | address not in symbols.json |
| g | 0x087a2444 | g_087a2444 | dvar_g_playerCollisionEjectSpeed | address not in symbols.json |
| g | 0x087a2458 | g_087a2458 | dvar_g_mantleBlockTimeBuffer | address not in symbols.json |
| g | 0x087a248c | g_087a248c | dvar_g_smoothClients | address not in symbols.json |
| g | 0x08a6f2c0 | g_08a6f2c0 | punkbuster | punkbuster: identifier already used in recon sources/headers |
| g | 0x0860b3ec | g_0860b3ec | snd_errorOnMissing | snd_errorOnMissing: identifier already used in recon sources/headers |
| g | 0x083deecc | g_083deecc | g_lex_param | g_lex_param: identifier already used in recon sources/headers |
| g | 0x0864c56c | g_0864c56c | level_time | level_time: identifier already used in recon sources/headers |
| f | 0x08124fd4 | fn_08124fd4 | Particle_SetBasis | Particle_SetBasis: identifier already used in recon sources/headers |
| f | 0x08126252 | fn_08126252 | Particle_GetStartVelocity | Particle_GetStartVelocity: identifier already used in recon sources/headers |
| g | 0x081850c0 | g_081850c0 | bg_itemList | bg_itemList: identifier already used in recon sources/headers |
| f | 0x081279e4 | fn_081279e4 | Tail_UpdateLengthAndPoint | Tail_UpdateLengthAndPoint: identifier already used in recon sources/headers |
| g | 0x0849f61c | g_0849f61c | g_gentities | g_gentities: collides with another selected proposal at 0x08716400 |

## Resolved disagreements (chosen vs. rejected)

| kind | addr | old | chosen | rejected |
|---|---|---|---|---|
| f | 0x0807a318 | ScrBuf_AdvanceA | TempMemory_ReserveAliasA (descriptive) | ScrBuf_AdvanceThunk1 |
| f | 0x0807a32c | ScrBuf_AdvanceB | TempMemory_ReserveAliasB (descriptive) | ScrBuf_AdvanceThunk2 |
| f | 0x08117cb2 | Scr_GetFunction_08117cb2 | GScr_GetBuiltinFunction (inferred) | GScr_GetFunction, Scr_LookupBuiltinFunction |
| f | 0x0811b1ca | G_FreeClassObject1 | HudElem_FreeScriptObject (inferred) | HudElem_ReleaseScriptObject |
| f | 0x081383ee | Loc_CallA | Loc_CreateStringTable (descriptive) | LocTable_CreateThunk |
| g | 0x081417a0 | g_081417a0 | entityStateFields (inferred) | g_netFieldsEntityState |
| g | 0x08141ae0 | g_08141ae0 | sharedEntityFields (inferred) | g_netFieldsSharedEntity |
| g | 0x08141f60 | g_08141f60 | clientStateFields (inferred) | g_netFieldsClientState |
| g | 0x0814cfc8 | g_0814cfc8 | vec3_origin (inferred) | g_zeroVec3 |
| g | 0x08150b9c | g_08150b9c | inflateSync_mark (observed) | zlib_inflate_mark |
| g | 0x08150be0 | g_08150be0 | cplens (observed) | zlib_cplens |
| g | 0x08150c60 | g_08150c60 | cplext (observed) | zlib_cplext |
| g | 0x08150ce0 | g_08150ce0 | cpdist (observed) | zlib_cpdist |
| g | 0x08150d60 | g_08150d60 | cpdext (observed) | zlib_cpdext |
| g | 0x08150f40 | g_08150f40 | extra_lbits (observed) | zlib_extra_lbits |
| g | 0x08150fc0 | g_08150fc0 | extra_dbits (observed) | zlib_extra_dbits |
| g | 0x0815108c | g_0815108c | bl_order (observed) | zlib_bl_order |
| g | 0x081510a0 | g_081510a0 | static_ltree (inferred) | zlib_static_ltree |
| g | 0x08151520 | g_08151520 | static_dtree (inferred) | zlib_static_dtree |
| g | 0x081518a0 | g_081518a0 | base_length (observed) | zlib_base_length |
| g | 0x08151920 | g_08151920 | base_dist (observed) | zlib_base_dist |
| g | 0x0817d9e0 | g_0817d9e0 | clientCommandTable (observed) | g_clientCommandTable |
| g | 0x0817db80 | g_0817db80 | sv_antilagFrameOffsetA (inferred) | g_originHistoryStartFrames |
| g | 0x0817db84 | g_0817db84 | sv_antilagFrameOffsetB (inferred) | g_originHistoryEndFrames |
| g | 0x0817e338 | g_0817e338 | g_randSeed (inferred) | rand_seed |
| g | 0x08183240 | g_08183240 | fixed_bl (observed) | zlib_fixed_bl |
| g | 0x08183244 | g_08183244 | fixed_bd (observed) | zlib_fixed_bd |
| g | 0x08183260 | g_08183260 | fixed_tl (inferred) | zlib_fixed_tl |
| g | 0x08184260 | g_08184260 | fixed_td (inferred) | zlib_fixed_td |
| g | 0x081843a4 | g_081843a4 | static_l_desc (observed) | zlib_static_l_desc |
| g | 0x081843b8 | g_081843b8 | static_d_desc (observed) | zlib_static_d_desc |
| g | 0x081843cc | g_081843cc | static_bl_desc (observed) | zlib_static_bl_desc |
| g | 0x08184860 | g_08184860 | g_playerAnimScriptPath (observed) | g_animScriptFileName |
| g | 0x08184cc0 | g_08184cc0 | g_animScriptSectionNames (observed) | g_animScriptKeywords |
| g | 0x081e9544 | g_081e9544 | huff_bitCursor (inferred) | g_huffBitCursor |
| g | 0x081e9c60 | g_081e9c60 | msgHuff (inferred) | g_msgHuff |
| g | 0x081f0c7c | g_081f0c7c | msgHuffDecompressor (inferred) | g_huffSecondTree |
| g | 0x081f7c98 | g_081f7c98 | msg_huffmanInitialized (inferred) | g_huffInitialized |
| g | 0x082055a0 | g_082055a0 | net_adrToStringBuf (inferred) | g_netAdrToStringBuf |
| g | 0x08205c88 | g_08205c88 | g_scr_loaded_scripts_table (observed) | scrCompileFileCacheArrayId |
| g | 0x08205c8c | g_08205c8c | g_scr_file_function_tables (observed) | scrCompileFuncRefArrayId |
| g | 0x08205c90 | g_08205c90 | scrCompileFuncCallArrayId (observed) | g_scr_builtin_function_cache |
| g | 0x08205c94 | g_08205c94 | g_scr_builtin_method_cache (observed) | scrCompileMethodCallArrayId |
| g | 0x08205cb0 | g_08205cb0 | g_scr_code_length (observed) | scrCodeWindowLen |
| g | 0x08297484 | g_08297484 | g_scr_buffer_offset (observed) | tempMemoryOffset |
| g | 0x0839753c | g_0839753c | g_scr_code_hash (observed) | g_animTreeHash, scrCompileOpcodeHash |
| g | 0x08397548 | g_08397548 | g_scr_code_base (observed) | scrCodeWindowBase |
| g | 0x0839754c | g_0839754c | scrGlobalWindowEnd (inferred) | g_scr_global_window_end |
| g | 0x083e3000 | g_083e3000 | g_skelPoolBase (inferred) | g_svSkelMemoryPool |
| g | 0x08423000 | g_08423000 | g_skelPoolPtr (inferred) | g_svSkelMemoryPtr |
| g | 0x08423004 | g_08423004 | g_svSkelOverflowWarnGeneration (inferred) | g_skelOverflowWarnGeneration |
| g | 0x08423008 | g_08423008 | g_sv_serverIdCounter (inferred) | sv_serverId |
| g | 0x08423088 | g_08423088 | g_svs_snapFlags (inferred) | sv_snapFlagServerBit |
| g | 0x08423098 | g_08423098 | g_svs_nextSnapshotEntities (observed) | sv_nextSnapshotEntities |
| g | 0x084230ac | g_084230ac | g_svs_nextArchivedSnapshotFrames (inferred) | sv_nextArchivedSnapshotFrame |
| g | 0x084230b8 | g_084230b8 | sv_nextArchivedSnapshotBuffer (inferred) | g_svs_nextArchivedSnapshotBuffer |
| g | 0x084230bc | g_084230bc | g_svs_nextCachedSnapshotEntities (inferred) | sv_nextCachedSnapshotEntity |
| g | 0x084230c0 | g_084230c0 | g_svs_nextCachedSnapshotClients (inferred) | sv_nextCachedSnapshotClient |
| g | 0x084230c4 | g_084230c4 | sv_nextCachedSnapshotFrame (inferred) | g_svs_nextCachedSnapshotFrames |
| g | 0x084230dc | g_084230dc | g_svs_challenges (inferred) | svs_challenges |
| g | 0x084400f0 | g_084400f0 | sv_authorizeAddress (inferred) | g_svs_authorizeAddress |
| g | 0x08440104 | g_08440104 | sv_lastAuthorizeServerTime (inferred) | g_svs_lastAuthorizeTime |
| g | 0x08440108 | g_08440108 | g_svs_oobNetProfile (inferred) | svs_OOBProf |
| g | 0x0844010c | g_0844010c | g_svs_tempBans (inferred) | sv_tempBans |
| g | 0x0844020c | g_0844020c | sv_checksumFeed (inferred) | g_sv_checksumFeed |
| g | 0x08440618 | g_08440618 | sv_configstrings (inferred) | g_sv_configstrings |
| g | 0x08442618 | g_08442618 | sv_svEntities (inferred) | g_sv_svEntities |
| g | 0x0849f630 | g_0849f630 | g_skelMemGeneration (inferred) | g_svSkelGeneration |
| g | 0x0849f634 | g_0849f634 | g_skelPoolOffset (inferred) | g_svSkelPoolOffset |
| g | 0x0849fbe0 | g_0849fbe0 | sv_masterAddress (inferred) | g_masterAddress |
| g | 0x0849fbf4 | g_0849fbf4 | svc_lastRconTime (inferred) | g_lastRconTime |
| g | 0x085a9844 | g_085a9844 | g_soundAliasRandSeed (inferred) | com_randSeed |
| g | 0x085b7660 | g_085b7660_slots | g_085b7660_cmTraceSlots (inferred) | cm_scratchSlots |
| g | 0x085bc800 | g_085bc800 | pfnBigShort (inferred) | swap_bigShortFn |
| g | 0x085bc804 | g_085bc804 | pfnLittleShort (inferred) | swap_littleShortFn |
| g | 0x085bc808 | g_085bc808 | swap_bigLongFn (inferred) | pfnBigLong |
| g | 0x085bc80c | g_085bc80c | pfnLittleLong (inferred) | swap_littleLongFn |
| g | 0x0860b424 | g_0860b424 | g_curAnimScriptData (inferred) | g_animScriptData |
| g | 0x0860dba0 | g_0860dba0 | g_animDefineStrings (inferred) | g_animConditionDefineStrings |
| g | 0x0860e020 | g_0860e020 | g_animDefineValues (inferred) | g_animConditionDefineBits |
| g | 0x08626b80 | g_08626b80 | g_playerAnimTypeNameTable (inferred) | g_playerAnimTypeNames |
| g | 0x0864e0d4 | g_0864e0d4 | g_mapChangeKeepStateArg (observed) | level_mapCommandArg |
| g | 0x0864f970 | g_0864f970 | level_mapCommandState (observed) | g_mapChangeCallState |
| g | 0x0864f974 | g_0864f974 | g_radiusDamageActiveSource (observed) | level_radiusDamageSkipClients |
| g | 0x0864f978 | g_0864f978 | level_ignorePlayerRadiusDamage (observed) | g_radiusDamageSourceSelector |
| g | 0x08852e20 | g_08852e20 | g_modelHandleTable (observed) | g_modelHandles |
| g | 0x08853220 | g_08853220 | scr_str_emptystring (observed) | scrStr_emptyString |
| g | 0x08853222 | g_08853222 | scr_str_allies (observed) | scrStr_allies |
| g | 0x08853224 | g_08853224 | scr_str_axis (observed) | scrStr_axis |
| g | 0x08853226 | g_08853226 | scr_str_current (observed) | scrStr_current |
| g | 0x0885322a | g_0885322a | scr_str_death (observed) | scrStr_death |
| g | 0x08853230 | g_08853230 | scr_str_empty (observed) | scrStr_empty |
| g | 0x08853232 | g_08853232 | scr_str_entity (observed) | scrStr_entity |
| g | 0x08853236 | g_08853236 | scr_str_fraction (observed) | scrStr_fraction |
| g | 0x0885323e | g_0885323e | scr_str_invisible (observed) | scrStr_invisible |
| g | 0x0885324c | g_0885324c | scr_str_normal (observed) | scrStr_normal |
| g | 0x08853254 | g_08853254 | scr_str_position (observed) | scrStr_position |
| g | 0x08853264 | g_08853264 | scr_str_script_model (observed) | scrStr_script_model |
| g | 0x08853266 | g_08853266 | scr_str_script_origin (observed) | scrStr_script_origin |
| g | 0x08853268 | g_08853268 | scr_str_spectator (observed) | scrStr_spectator |
| g | 0x0885326c | g_0885326c | scr_str_surfacetype (observed) | scrStr_surfacetype |
| g | 0x08853274 | g_08853274 | scr_str_trigger (observed) | scrStr_trigger |
| g | 0x08853276 | g_08853276 | scr_str_trigger_use (observed) | scrStr_trigger_use |
| g | 0x08853278 | g_08853278 | scr_str_trigger_use_touch (observed) | scrStr_trigger_use_touch |
| g | 0x0885328e | g_0885328e | scr_str_intermission (observed) | scrStr_intermission |
| g | 0x08853292 | g_08853292 | scr_str_playing (observed) | scrStr_playing |
| g | 0x08853294 | g_08853294 | scr_str_none (observed) | scrStr_none |
| g | 0x08853296 | g_08853296 | scr_str_dead (observed) | scrStr_dead |
| g | 0x08853298 | g_08853298 | scr_str_auto_change (observed) | scrStr_auto_change |
| g | 0x0885329a | g_0885329a | scr_str_manual_change (observed) | scrStr_manual_change |
| g | 0x0885329c | g_0885329c | scr_str_freelook (observed) | scrStr_freelook |
| g | 0x088532a2 | g_088532a2 | scr_str_snd_enveffectsprio_level (observed) | scrStr_snd_enveffectsprio_level |
| g | 0x088532a4 | g_088532a4 | scr_str_snd_enveffectsprio_shellshock (observed) | scrStr_snd_enveffectsprio_shellshock |
| g | 0x088532a6 | g_088532a6 | scr_str_snd_channelvolprio_holdbreath (observed) | scrStr_snd_channelvolprio_holdbreath |
| g | 0x088532a8 | g_088532a8 | scr_str_snd_channelvolprio_pain (observed) | scrStr_snd_channelvolprio_pain |
| g | 0x088532aa | g_088532aa | scr_str_snd_channelvolprio_shellshock (observed) | scrStr_snd_channelvolprio_shellshock |
| g | 0x088532c6 | g_088532c6 | scr_str_tag_weapon_right (observed) | scrStr_tag_weapon_right |

## Verification

* `link_test.py`: exit 0, link_ok, 0 undefined globals/symbols, 0 duplicate definitions, image verification ok (1284 aliases, 2969 pointers, 0 bad).
* .text/.rodata/.data of the linked executable are byte-identical to the pre-pass build; the per-object symbol multiset (type,size) is identical (5310). Executable symbol list differs only by the five collapsed aliases above.
* Oracle capture dm_toujane (mp_toujane), recon run 7 (renamed) and run 8 (pre-pass) vs oracle run1: verdict `match`, server.log identical.
