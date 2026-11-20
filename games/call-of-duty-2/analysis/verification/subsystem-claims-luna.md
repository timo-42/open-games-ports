# Adversarial subsystem claim verification

Target: `.local/cod2/lnxded-1.3/cod2_lnxded`, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`. Checks below use Intel-syntax `objdump -d`; conclusions are limited to the cited instruction windows. UNSETTLED means the inspected window did not settle the claim, not that the claim is false.

| Doc | Claim | Verdict | Evidence |
|---|---|---|---|
| network | Datagram receive buffer is 0x20000 bytes; exact full receive is oversize | UNSETTLED | `Sys_GetPacket` starts 0x080d5330. Its opening instructions through 0x080d534f only establish socket selection. The receive call/length comparison was not captured in the bounded window. |
| network | Fragment payload chunk is 0x514 (1300) bytes | UNSETTLED | `Netchan_TransmitNextFragment` 0x0806ba4c initializes a 0x578 output message (0x0806ba65), but this excerpt does not show the fragment-length constant/comparison. |
| network | Netchan transmit rejects lengths greater than 0x20000 | CONFIRMED | At 0x0806bc75–0x0806bc7c: `cmp [ebp+0xc],0x20000; jle ...`; larger lengths take the error path beginning 0x0806bc7e. |
| network | Fragment output buffer maxsize is 0x578 | CONFIRMED | 0x0806ba65–0x0806ba6c loads immediate `0x578` into the third argument before `MSG_Init`. |
| network | Client reliable acknowledgement is masked to 7 bits for ring lookup | CONFIRMED | `SV_Netchan_Decode` at 0x0809756b–0x08097574 loads client field +0x20810 and applies `and edx,0x7f`; subsequent arithmetic computes the ring entry offset. |
| network | `SV_ExecuteClientMessage` allocates a 0x20000-byte decompression buffer | CONFIRMED | At 0x08091155–0x08091163, it passes `0x20000` as second argument to the decompression helper. |
| network | Netchan header has no ack field | UNSETTLED | Transmit/process field layout requires full entry-path inspection. These selected windows do not establish absence of an ack field. |
| network | Challenge table has 1024 entries of 0x74 bytes | UNSETTLED | Not checked with a table traversal window or `.data` dump in this bounded pass. |
| script-vm | VM has 32-slot call-frame array | UNSETTLED | `VM_ExecuteInternal` prologue 0x08080158 reserves 0x11c bytes and increments a counter, but this alone does not establish frame count/layout. |
| script-vm | Variable pool entries span IDs 1..0xfffd | CONFIRMED | `ScrVar_Reset` at 0x0807a8fa initializes ID 1; 0x0807a901–0x0807a908 loops while ID `<= 0xfffd`. |
| script-vm | Variable hash is `(name + parent) % 0xfffd + 1` | UNSETTLED | Reset routine 0x0807a8ee does not contain the hash calculation; hash implementation window was not inspected. |
| script-vm | `Scr_GetArrayForWrite` special-cases reserved ID 0xfffe | CONFIRMED | At 0x0807eeaf–0x0807eeb6 it compares first argument with `0xfffe`, branching to a distinct path on equality. Copy-on-write behavior beyond this branch is not established by this excerpt. |
| script-vm | Script tick increments and wraps at 24 bits | CONFIRMED | 0x080855f2 increments global `0x08397518`; 0x080855f8–0x080855ff ANDs it with `0xffffff`. |
| script-vm | End-frame rewind decrements and wraps at 24 bits | CONFIRMED | 0x08085607 decrements `0x08397518`; 0x0808560d–0x08085614 masks with `0xffffff`. |
| script-vm | Developer settings store boolean bytes | CONFIRMED | `Scr_Settings` 0x0807ffdb–0x0807ffe2 and 0x0807ffe7–0x0807ffee compare arguments to zero and store `setne` results at 0x0839750a and 0x0839750b. Third value similarly stored at 0x083d8a95 (0x0807fff3–0x0807fffa). |
| script-vm | `Scr_CountObjectsOfClass` uses third argument as object id lookup and compares returned value to first argument | CONFIRMED | 0x080843d7–0x080843e3 tests/passes `[ebp+0x10]` to lookup helper; 0x080843e8–0x080843eb compares first argument `[ebp+8]` to returned `eax`. |

## Totals

- CONFIRMED: 10
- REFUTED: 0
- UNSETTLED: 6

No selected claim was refuted in this bounded disassembly pass; therefore there are no corrected values to report. Several unsettled claims need a wider function window or a `.data`/`.rodata` inspection to adjudicate.
