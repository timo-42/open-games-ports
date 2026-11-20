# Shared signature fixes (final merge)

Evidence is from `objdump -d -M intel` of `.local/cod2/lnxded-1.3/cod2_lnxded` (SHA-256
`05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`). Status: observed =
verified in the disassembly; the owning chunk's JSON signature and definition were changed and
the reporting chunks' casts and placeholder declarations were removed where safe.

## Duplicate addresses removed

Chunk 006 and gap chunk 100 both translated 0x0805fc62 (Cmd_Wait_f), 0x080600f6 (Cmd_Vstr_f),
0x08060196 (Cmd_PrintArgs_f) and 0x080609e0 (Cmd_List_f). Chunk 100's definitions were kept (equally
complete, evidence text better); the definitions and records were removed from chunk 006.

## Verified and fixed

| Address | Function | Fix | Evidence |
| --- | --- | --- | --- |
| 0x08084bb2 | Scr_ReleaseAnyValue | `const char *` | body is `call Scr_GetAny; call ScrStr_TextFromHandle; leave; ret`, so eax carries the text pointer back. |
| 0x08084c10 | Scr_ReleaseAnyValueCopy | `const char *` | byte-identical body (same pattern, not in the report). |
| 0x08084d24 | Scr_ReleaseLocalizedString | `const char *` | `call 0x8084c96; call 0x8078ee6; leave; ret` (tail value is the text). |
| 0x08091e48 | SV_ValidateModelName | `struct XModel *` | ends with `call XModel_FindOrLoad(name+7,..); leave; ret`, eax passes through. |
| 0x080ac5d8 | Com_PoolTouchAt | `void *` | tail value of Com_PoolAt: `mov eax,0x84a9800; add eax,[ebp+8]`. |
| 0x08079914, 0x080798e0 | ScrStr_InternCString(Ex) | `uint32` | both `call` then `leave; ret`; final callee ScrStr_Intern returns the handle. |
| 0x080799ee, 0x08079a22 | ScrStr_InternLowerCString(Ex) | `uint32` | same tail-return pattern (not in report). |
| 0x080a1cee | FS_FreeFileList | 2 args `(char **list, int tag)` | all 9 call sites store an immediate (3 or 10) at `[esp+4]` before the call; the callee itself never reads it. |
| 0x080b0374 | Com_LocalizeSoundAliasFile | 3 args `(name, zone, tmpPath)` | the single call site (0x80b1182) pushes three words; the callee reads only two (observed), the third is unused. |
| 0x0806b1d4 NET_AdrToString, 0x0806b9d0 Netchan_Setup, 0x0806c424 NET_CompareBaseAdr, 0x0806c54c NET_CompareAdr, 0x0806c57a NET_IsLocalAddress, 0x0806c7ec NET_SendPacket, 0x0806c8cc/ca3e/cb5c/cc7a NET_OutOfBand* | `struct netadr_s` (20 bytes) by value | argument slots span `[ebp+0x8..]` in 5-word groups (e.g. Netchan_Setup reads +0x10..+0x20 and qport at +0x24; NET_CompareBaseAdr takes `lea eax,[ebp+8]` and `[ebp+0x1c]`); callers copy five dwords onto the stack. NET_IsLocalAddress receives all five words although it reads only the type. The `...Signed` helpers genuinely take pointers and stay unchanged. |
| 0x0808bd32 | Wrapper_080660a2 | `char *` | `call MapNameFromPath; leave; ret` (tail value). |
| 0x08090afa | SV_ForwardCallToGame | `float (const float *, const float *)` | tail call to FX_GetAttenuation (ends `fld ...; ret`); caller 0x8113bde passes two `lea` pointers and reads `st0` (`fld; fucompp`). |
| 0x080a40ea | Vec3_DeltaProcess | `float` | tail call to Vec3_Length; caller 0x80da561 does `fstp DWORD PTR [ebp-0x1c]` right after. |
| 0x084a74c0 | g_084a74c0 | `char *[1024]` | filled with the return of CopyString (0x080ab740) at 0x806606c and freed through 0x80ab5b8 at 0x80a2b82. |

## Further mismatches found while compiling (same tail-return/consumption evidence)

| Address | Function | Fix |
| --- | --- | --- |
| 0x08062948 | Com_Call_0807706a_With5 | returns `uint8 *` (tail value of StrPool_AllocBlock, which returns a table pointer). |
| 0x0811d44a | G_ShaderIndex | returns `int` (tail-calls the configstring lookup 0x0811d300). |
| 0x080ebcc0 | BG_RegisterWeaponNoCallback | returns `int` (tail value of BG_RegisterWeapon). |

## Reported but not changed

* Chunk 105: names G_AnimIndex / G_SoundIndex look shifted relative to callers. Naming question, not a
  signature defect; left as is.
* Chunk 100 Com_PumpSysEvents returning a millisecond clock: naming question, left as is.
* Chunk 49/105 report that AnglesToForward_ProjectedNormalized and similar call sites pass a float yaw:
  not verified here; belongs to the struct/overload unification pass.
* DotProduct/Vec2LengthSq etc. return x87 `long double` in the records; not touched.

## Leftover workarounds

Chunks that do not include `cod2_prototypes.h` (047, 048, 050) keep a local declaration of
`Scr_ReleaseAnyValue` / `Scr_ReleaseLocalizedString`, now matching the corrected shared signature.
Chunk 102 keeps the inline `BG_GetWeaponDef102` cast because its `weaponDef_s` layout differs from the
shared `WeaponDef043_s` (struct unification is a separate pass).
