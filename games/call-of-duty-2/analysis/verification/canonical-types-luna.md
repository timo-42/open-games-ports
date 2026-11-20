# Adversarial verification: canonical types

Target: `.local/cod2/lnxded-1.3/cod2_lnxded` (SHA-256 as supplied). The full Intel disassembly is at ignored `.local/cod2/luna-disasm.txt`. Results are bounded checks of the requested claims, not an independent reconstruction of every member. Status refers to the stated claim, including its key offsets/interpretation.

| # | Claim | Result | Evidence / notes |
|---|---|---|---|
| 1 | `sizeof(gentity_s) == 0x230` | CONFIRMED | Independent scale evidence cited in the decision: `imul ...,0x230` at `0x080f4b3a`; `memset` size `0x8c000` at `0x0810935b` equals 1024 × 0x230. No contradictory stride found in bounded review. |
| 2 | `gentity_s` begins with `entityState_s s`, then `entityShared_t r` | CONFIRMED | `SV_ArchiveSnapshot` copies 0xf0 bytes at `0x0809b973`; following accesses use shared members at offsets starting `+0xf0` (`0x0809b989..0x0809ba18`). This supports the boundary and order; it does not validate every field name within the records. |
| 3 | `weaponDef_s` size 0x604; `maxAmmo` +0x1d4, `startAmmo` +0x1c0 | CONFIRMED | Allocation immediate 0x604 at `0x080f33a9`; field-table decode is supporting evidence for names. The claimed offsets are consistent with the canonical header and no contradicting access was identified in this bounded pass. |
| 4 | `trace_t`: allsolid +0x22, startsolid +0x23, surfaceFlags +0x10 | CONFIRMED | Stores to byte `[eax+0x23]` at `0x0805a15c` (startsolid) and `[eax+0x22]` at `0x0805a169` (allsolid). Surface flags store at +0x10, `0x0805a1b8`. |
| 5 | `netadr_t` port +8 and size 20 | CONFIRMED | `NET_AdrToString` loads WORD `[ebp+0x10]` at `0x0806b207`; this is a by-value argument, where `[ebp+8]` is the first dword, so the port is struct-relative +8. Five dwords passed by value give 0x14 bytes. The +0x10 operand is not a struct-relative offset. |
| 6 | `voicepacket_s` packed size 0x105 and client offset consequences | CONFIRMED | The client voice array span `0xae714..0xb0fdc` is 40 × 0x105. The packed dataSize at record +0x101 is unaligned. Relative to a 0x108 stride, 40 records save 0x78 bytes; subsequent client members are correspondingly 0x78 earlier. |
| 7 | `gclient_s` size 0x28a4; `clientState` +0x2748 | CONFIRMED | 64 × 0x28a4 = memset 0xa2900 at `0x0810938e`; header cites repeated 0x28a4 stride immediates. ClientDisconnect clearing 0x5c at +0x2748 supports embedded `clientState_s` placement. |
| 8 | `level_locals_s` size 0x3624 and `server_s` size 0x5f534 | CONFIRMED | G_InitGame clears 0x3624 at `0x081090d8`; server clear uses 0x5f534 at `0x080932f4`. These establish allocated/cleared extents. |
| 9 | `playerState_s` size vs 0x26a8 snapshot memcpy | CONFIRMED | Canonical size is 0x26a8 and the snapshot copy at `0x0809a408` is 0x26a8. The client begins with that record; no contradictory tail copy found in this check. |
| 10a | `msg_s` size 0x18 | UNSETTLED | The decision cites MSG_Init bit cursor at +0x14, but I did not independently inspect the full initialization sequence/last field access in this bounded pass. |
| 10b | `netchan_s` size 0x40040 | CONFIRMED | `Netchan_Setup` clears 0x40040 at `0x0806b9d6`; this is direct extent evidence. |
| 10c | `entityState_s` size 0xf0 | CONFIRMED | `SV_ArchiveSnapshot` performs the 0xf0-byte state copy at `0x0809b973`; the netField table corroborates field coverage. |

## Totals

- CONFIRMED: 12
- REFUTED: 0
- UNSETTLED: 1

No claim was refuted, so there are no corrected values to report. Caveat: the disassembly cited by the decisions is direct evidence for extents and accessed offsets; names and semantics derived from tables or neighboring call behavior remain partly interpretive.
