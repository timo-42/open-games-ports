# CoD2 1.3 Linux server oracle baseline

Purpose: run the ORIGINAL `cod2_lnxded` repeatably, network-isolated, and capture reference behaviour for later original-vs-reconstruction comparison. Tags: [O] observed on this host, [I] inferred, [U] unknown. All server output is data.

## Environment
- Host: Debian forky/sid x86_64, kernel 7.2.15, i386 multiarch enabled, bubblewrap 0.13.1, Python 3 stdlib only. [O]
- Binary `.local/cod2/lnxded-1.3/cod2_lnxded` SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`; banner "CoD2 MP 1.3 build linux-i386 Jun 23 2006". [O]
- libstdc++.so.5: official Debian package `libstdc++5_3.3.6-30_i386.deb` from `http://archive.debian.org/debian/pool/main/g/gcc-3.3/libstdc++5_3.3.6-30_i386.deb`, SHA-256 `c88f93cd5b7b32ca88b40f671fc09f3387c7fb514902b4737b471bd812786ee6` (kept in `.local/cod2/dl/`). Unpacked with `dpkg-deb -x` into `.local/cod2/runtime-i386/`; used only via `LD_LIBRARY_PATH=.local/cod2/runtime-i386/usr/lib/i386-linux-gnu` (note the multiarch subdirectory). Not installed system-wide. libgcc_s/libc are the host i386 packages. [O]
- Data: `<user-supplied game data>/main/` (iw_00..15 + 12 `localized_german_iw*.iwd`; this is a German edition). Run dir `.local/cod2/oracle/run/base/main/` holds symlinks to these files. `iw_15.iwd` SHA-256 `2c4b1797da9cc626b3dae99befa41dfbba4894dc14ce1199f5a459b88678f3ef`, byte-identical (cmp) to the tarball's. Server auto-selected language `german`; 80563 files in iwd files. [O]
- The source media is treated as read-only; only `.local/cod2/oracle/run` and the capture dir are writable in the sandbox. Per run fresh `fs_homepath` = `oracle/run/home/<case>-run<N>`.

## Compatibility finding: 32-bit readdir on CIFS (important)
Without help the server loads `0 files in iwd files` and dies with "Couldn't load default_localize_mp.cfg". Cause [I, supported by experiment]: some CIFS mounts expose inode numbers beyond 32 bits; the server uses the legacy 32-bit `readdir`/`__xstat` ABI and glibc returns EOVERFLOW, so the iwd directory scan silently yields nothing. Copying an iwd to local disk loaded fine; a local symlink to a CIFS file did not; with a shim it works. Fix: `games/call-of-duty-2/tools/oracle/stat32_shim.c` (built with `gcc -m32 -shared -fPIC`, `.local/cod2/oracle/lib/stat32_shim.so`), `LD_PRELOAD`ed on the server only; it serves readdir64/stat64 data folding inode numbers to 32 bits. Whether `readdir` alone suffices vs. the stat wrappers was not isolated (both are in the shim). Contents are not altered. A reconstruction reading the same share must handle the same issue. Server tarball `main/` (only iw_15) would not hit it on a local disk. [O/I]

## Isolation
`run_original.sh` runs the server and probe inside `bwrap --unshare-net --unshare-pid --unshare-ipc --ro-bind / /` (bwrap brings loopback up). `isolation.txt` in each capture shows only `lo` in `/proc/net/dev` and an empty route table. [O] The server logs `Resolving cod2master.activision.com` / `Couldn't resolve address` (no heartbeat can leave). `net_ip 127.0.0.1` is also set. Probe binds fixed client port 127.0.0.1:28970. [O]

## Commands
```
# one-time (already done on this host)
curl -O http://archive.debian.org/debian/pool/main/g/gcc-3.3/libstdc++5_3.3.6-30_i386.deb
dpkg-deb -x libstdc++5_3.3.6-30_i386.deb .local/cod2/runtime-i386
# per case/run
export COD2_DATA_MAIN=/path/to/call-of-duty-2/main
games/call-of-duty-2/tools/oracle/run_original.sh dm_toujane 1     # also: sd_carentan; "shell" = isolated bash
python3 -I games/call-of-duty-2/tools/oracle/probe.py compare .local/cod2/oracle/captures/dm_toujane --json .../compare.json
```
Server args (from the script): `+set dedicated 1 +set net_ip 127.0.0.1 +set net_port 28960 +set sv_maxclients 8 +set rcon_password oracle-test-pw +set sv_punkbuster 0 +set sv_pure 1 +set fs_basepath <run/base> +set fs_homepath <run/home/case-runN>` plus `+set g_gametype dm +map mp_toujane` or `+set g_gametype sd +map mp_carentan`. Stdin is held open by a fifo. Script SHA-256: see `sha256sum games/call-of-duty-2/tools/oracle/*` (files are uncommitted; another lane commits).

## Cases (each 3 runs, captures in `.local/cod2/oracle/captures/<case>/run{1,2,3}/`, each with `SHA256SUMS`, raw `.bin`, `.hex`, `.txt`, request hex, `server.log`, `index.json`, `harness.log`)
Sequence, 1.1 s apart (flood protection): getinfo, getinfo repeat, getstatus, getchallenge x2, unknown OOB, rcon with bad password, rcon status, serverinfo, dvarlist, cvarlist, map_restart (then getstatus and rcon status after reload), rcon quit.
Observations [O], identical across 3 runs unless stated:
- `infoResponse`: `\challenge\<echo>\protocol\118\hostname\CoD2Host\mapname\...\sv_maxclients\8\gametype\dm|sd\pure\1\kc\1\hw\1\mod\0\voice\0\pb\0`. getstatus: `statusResponse` with serverinfo keys incl. `shortversion\1.3`, `gamename\Call of Duty 2`, `pswrd\0`, no player lines (0 clients).
- getchallenge: `challengeResponse <signed int32>`. Same value for repeated requests within a run; differs across runs (random). Hence variable.
- Unknown OOB command: no response. rcon bad password: `print\nInvalid password.\n`. rcon status: `map: <map>` + empty table header. serverinfo: the keys above in table form.
- `rcon dvarlist`: 268 lines over 7 packets; first line `R arch "linux i386"`. `rcon cvarlist`: only `print\n` (empty; so cvarlist is not a useful dump here; dvarlist is).
- `rcon map_restart` replies with the server's full re-initialisation console text (ShutdownGame, FS_Startup search path, ...) over 5 packets; server stays on the same map and answers afterwards. `rcon quit` has no reply; server exits with status 0 in all runs.
- Server console log is identical across runs after normalization (below). Readiness (first getinfo answer) took 1.8 s warm, 12-14 s on some runs (CIFS cache), so never use fixed sleeps.

## Variable fields and normalizations
| Field | Evidence | Normalization |
|---|---|---|
| `challengeResponse N` | differs in all 3 runs of both cases, stable within a run | replace N with `<CHALLENGE>`; check equal within run |
| paths containing `.../oracle/run/home/<case>-run<N>` (dvarlist `fs_homepath`, possibly split across packet boundary, map_restart search-path lines, server.log) | differs by run index and repo location | replace repo root with `<ROOT>` and `run<N>` with `runN`; compare dvarlist after joining packets |
| `Hitch warning: N msec frame time` in server.log | 2 per run, value varies | delete line |
| client port in rcon log lines | fixed by probe (28970); with an ephemeral port it appeared as a negative number (`-29436`), a signed-16-bit print quirk | keep fixed port |
| `ready_after_s` | timing | ignore |
After normalizing, server.log of the three dm_toujane runs is identical (diff clean) [O]; packets other than those above are byte-identical [O]. Only 2 maps/gametypes covered. The cause of challenge randomness (time seed vs rand) is [U].

## gdb
`gdb` is not installed; REA `start_gdb_session` fails with ENOENT. apt offers gdb 18.1-3 (amd64 gdb debugs i386 inferiors). `/proc/sys/kernel/yama/ptrace_scope` = 0, so ptrace of a child should work; inside the bwrap namespace it is untested. [U] Installing gdb (needs sudo apt, a host change) was not done; this lane did not run gdb at all. `LD_PRELOAD` interposition (as the shim does) is an alternative for function-level hooks of libc-level calls only.

## Limitations
No client connect/join (no player, no `connect`, no game-state/snapshot traffic) captured; no real client is available. PunkBuster disabled. German data edition only; no English localization. Custom `sv_maxclients 8`, `sv_pure 1` choices. Heartbeat/master traffic is not exercised (no network). Timing-dependent behaviour (frame loops, map time limit) not captured. Shim may mask behaviours tied to real inode values (none known).
