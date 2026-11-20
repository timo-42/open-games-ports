# Lane net_loss: deterministic packet loss, duplication, reordering and silence

Tags: [O] observed, [I] inferred, [U] unknown. Plugin: `tools/oracle/scenarios/net_loss.py`. Original `cod2_lnxded` 1.3
(SHA-256 `05e774a3…a7`); pinned G_InitGame seed as in `gameplay-parity.md`.

## Cases

| Case | Players | Exercises |
|---|---|---|
| `netloss_play_dm_toujane` (`sv_timeout 3`, `sv_zombietime 1`) | A clean, B lossy, C goes silent | see below |
| `netloss_gamestate_dm_toujane` | A clean, B loses gamestate fragments | see below |

### `netloss_play_dm_toujane` (all times relative to T0 = 20000, by the deciding client's own snapshot time)
- B sends **cl_packetdup-style** packets: current usercmd plus the previous two with their original serverTimes
  (`install_packetdup`, instance-level replacement of `Client.send_packet`; same wire format otherwise).
- **c2s loss (B)**: single drops (every third snapshot, 3000..6000, 40 packets total incl. the burst), a 4-packet burst
  at 6000..6200 (longer than the dup window of 2, so some usercmds really are lost), and **reordering** at 6500..8500:
  two consecutive packets are held back and sent swapped after the next packet's turn (16 packets). The server sees
  seq N+1 before N and must discard N as out of order [O: capture `reordered` packets; server behaviour compared].
- **Reliable client commands over loss**: B queues `say b0..b5` at 3500, 4500, 6050 (inside the burst), 7000 (inside the
  reorder window), 9100 and 13050. They are resent in every packet until a server message acks them [O: `say` log lines
  b0..b5 each once, in order].
- **s2c loss (B)**: 1 in 4 messages dropped 9000..11000 (the next snapshot's `deltaNum` becomes 2: delta from the last
  acked frame), then a 35-message outage from 13000 (1750 ms, more than PACKET_BACKUP-3 = 29), so the first snapshot
  after it is a **full snapshot** (`deltaNum` 0 at serverTime 14800) [O].
- **Reliable server commands over loss**: A says `a0..a3` while B's messages are dropped; B receives the `h` commands
  late and in repeated server messages (7 `(dup)` reliable-command repeats in B's log) [O].
- **Silent client**: C stops sending anything at 16000 (all c2s dropped). With `sv_timeout 3` the server drops it with
  `EXE_TIMEDOUT` (`SV_CheckTimeouts` 0x0809657e: more than 5 consecutive frames past the drop point), logs `Q;0;2;fakeC`,
  and broadcasts `MP_DISCONNECTED` / `e "fakeC^7 EXE_TIMEDOUT"` to A and B [O].
- Movement/fire of all three players keyed to snapshot time, so lost usercmds change B's trajectory as seen in A's
  and C's snapshots.

### `netloss_gamestate_dm_toujane`
- [O] The connect-time gamestate is one 7-fragment message (6 x 1300 + 722 bytes), sent one fragment per ~100 ms.
- B's 3rd fragment (start 2600) is dropped. The client's `msgAck` stays behind; the server answers its next packets with
  more messages and **retransmits the whole gamestate as a new message** (seq 10) once the ack passes the old one.
  The retransmission loses its last fragment (start 7800) too; the third transmission (seq 12) is delivered.
- Compared items include the second/third gamestate content and `reliableSequence`.

## Determinism controls
- No decision uses the wall clock: c2s decisions use the deciding client's latest snapshot time; s2c windows are
  anchored at the first server message after a snapshot-time trigger and counted by sequence number from there;
  gamestate drops are by fragment start offset and by the order of fragment-bearing sequence numbers.
- The harness's wall-clock keep-alive (`resend=True`, after 100 ms of silence) is swallowed for A, B and C, because loss
  creates 100 ms gaps that would race with it (it changes packet counts and sequence numbers, not content).
- `sv_zombietime 1`: the end-of-run `rcon status` otherwise listed A's zombie slot with `lastmsg` 1950 or 2000, right at
  the 2 s zombie boundary (the `ZMBI` row is not matched by the `<LASTMSG>` mask in `gameplay.py`). With 1 s the slot is
  free in every run.
- Items: play 76599, gamestate 19401, all identical across 3 original runs (variable 0; the `rcon_status_after` item was
  the only variable one before `sv_zombietime 1`).

## Results
| Case | Original runs | Recon runs | Identical items | Mismatches |
|---|---|---|---|---|
| `netloss_play_dm_toujane` | 3 | 2 | 76599 | 0 |
| `netloss_gamestate_dm_toujane` | 3 | 2 | 19401 | 0 |
| `duel_dm_toujane` (regression, `--tag=-net_loss`) | 3 (earlier) | 1 | 84560 | 0 |

## Fix
**`WeaponView_IntegrateGunKick` 0x080f1a6a (chunk_045)**: the original moves the dword at `view_motion.+0x08` into a
stack slot and `fld`s it as a float (0x080f1b47..0x080f1b50, no `fild`). The reconstruction used `(float)vm->slot_08`
(int to float conversion), so `remaining` was about 1e9 instead of a denormal and the loop (`while b==0 || a==0`) ran
until both springs came to rest. Effect [O]: every gun-kick frame after firing stalled the recon server for about
400 ms (snapshots 14150 -> 14550 for A and C in recon only; stack samples inside `Spring1D_StepLimited` under
`SV_UserMove -> ClientThink`), and B's resume snapshot slipped from 14800 to 15150. The earlier lanes did not see it
because their runs fire only while the springs are at rest. Class: int-vs-float reinterpretation. Evidence: the
disassembly above and the vanishing of all 8436 mismatches after the fix. The call at 0x080f1ab..: the `fld DWORD PTR
[ebp-0x4]` operand is the raw dword.

## Not covered
- s2c out-of-order delivery (the hook can only drop; the fakeclient discards `seq <= in_seq` itself, which is client
  behaviour, not server behaviour).
- Server reliable-command overflow (more than 64 unacked commands -> `EXE_SERVERCOMMANDOVERFLOW`): needs a long outage
  plus heavy chat; flood protection (`sv_floodProtect`) limits the chat rate. [U]
- Loss during a map change (second gamestate) and rate limiting (`rate`/`sv_maxRate`, `rateDelayed` frames) were not varied.
- Loss while the keep-alive is active: swallowed on purpose (see Determinism controls).
