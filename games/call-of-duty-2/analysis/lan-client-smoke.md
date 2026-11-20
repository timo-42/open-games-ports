# Real Windows client against the reconstructed server (LAN smoke test, 2026-11-18)

The gameplay oracle (`gameplay-parity.md`) uses scripted protocol clients. This test checks that the stock CoD2
1.3 Windows client also works against the reconstruction (`.local/cod2/build/cod2_lnxded_recon`, built from commit
`32a4dc5fe`, SHA-256 `1be05f5e55a61f30e904abc2403cf32664a16d938dcb4b5e836c8b093b105b26`). It runs the same steps
against the original `cod2_lnxded` 1.3 (SHA-256 `05e774a3…a3a7`) for comparison.

Tags: [O] observed, [I] inferred.

## Setup

- **Server.** It ran on a Linux host and listened on its LAN address (`SERVER_LAN_IP:28960`; address redacted).
  - Settings: `dedicated 1`, `sv_pure 1`, `sv_punkbuster 0`, dm on mp_toujane.
  - The data came from the supplied installer tree (read-only), with a random rcon password.
  - Launcher: `.local/cod2/lanclient/server.sh`.
- **No outside contact.** The binary hard-codes `cod2master.activision.com` for master heartbeats and key authorization.
  - The server runs in `bwrap` with a private `/etc/hosts` that maps that name to `127.0.0.2`, so no packet leaves the LAN.
  - [O] The server log shows `resolved to 127.0.0.2:20710`.
  - A first attempt without this mapping sent master heartbeats to the public master server for a few seconds before it was stopped.
- **Client.**
  - Machine: a Windows client on the same LAN (address redacted).
  - Install: a compatible Windows 1.3 game tree. Its `main\iw_15.iwd` is identical to the supplied one (SHA-256 `2c4b1797…f3ef`).
  - Launch: `CoD2MP_s.exe +set cl_punkbuster 0 +connect SERVER_LAN_IP:28960`, through a scheduled task in the user's session.
  - Input: mouse and key events (`keybd_event`/`mouse_event`) from a PowerShell helper.
  - Screenshots: `CopyFromScreen`.
  - Afterwards: the player's `config_mp.cfg` was backed up and restored, and the helper files and tasks were deleted.

## Result

| Step | Reconstruction | Original |
|---|---|---|
| Connect, challenge, gamestate, pure check | [O] `Client 0 connecting ... from CLIENT_LAN_IP`, `CS_CONNECTED`, `J;0;0;<name>` in games_mp.log | [O] same |
| Gametype briefing (Deathmatch, score limit 50, time limit 30) | [O] shown | [O] shown |
| Team menu → British, weapon menu → first weapon, spawn | [O] spawned at the same spawn point, same view, HUD shows `32 \| 192`, 1 smoke + 1 frag | [O] same |
| Walk 1.5 s, jump, 5 shots, strafe, reload | [O] same view of the wall with bullet impacts, `32 \| 187` | [O] same |

Screenshots are kept under `.local/cod2/lanclient/{recon-2,orig-1}/shot*.png`. They show game imagery and are not committed.

The only visible difference is the round clock, which depends on how long the steps took on the wall clock. [I] The
same spawn point is expected: dm spawn selection with one player is deterministic, so it does not show the RNG is identical.

## Limits

- Human-paced input means this is a smoke test, not a bit-exact comparison. Bit-exact parity is covered by the 25 scripted cases.
- One client only. No second human player, no kill against a real client, no PunkBuster, no internet master or authorize server (blocked on purpose).
