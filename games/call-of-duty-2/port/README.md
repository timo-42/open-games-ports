# Rust dedicated server

## Build and run

Build from the repository root with the pinned Rust toolchain:

```sh
cargo build --release --locked -p cod2-server
```

Point `fs_basepath` at a Call of Duty 2 installation directory containing `main/` and its original IWD archives. Set `fs_homepath` to a writable directory for server configuration and logs:

```sh
GAME_DIR=/path/to/call-of-duty-2
SERVER_HOME="$HOME/.local/share/open-games/cod2"

target/release/cod2-server \
  +set fs_basepath "$GAME_DIR" \
  +set fs_homepath "$SERVER_HOME" \
  +set dedicated 1 \
  +set net_ip 127.0.0.1 \
  +set net_port 28960 \
  +set sv_maxclients 8 \
  +set g_gametype dm \
  +map mp_toujane
```

For LAN clients, bind `net_ip` to the host's LAN address instead of `127.0.0.1`. The current implementation sends master-server heartbeats whenever it binds a non-loopback address; a LAN bind does not disable those outbound announcements. The default home path can contain an empty `main/` directory, so explicitly setting `fs_homepath` is recommended.

The server accepts operator commands on stdin. A configured `rcon_password` enables remote console commands. Use only a trusted network for RCON and choose a strong password.

## Data and compatibility

Original IWD archives, maps, installers, and executables are not included. Supply a compatible game installation externally. The official dedicated-server 1.3 update archive can be supplied with `--update <archive>` when using the matching retail data profile; consult the upstream package documentation for supported update formats.

The Rust server and the original game data are separate. Asset loading and server startup do not imply complete stock-client gameplay compatibility.

## Tests

Run the media-independent server tests from the repository root:

```sh
cargo test --locked -p cod2-server
```
