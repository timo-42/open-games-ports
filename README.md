# Open Games Ports

This repository is being phased in with selected, native game-server ports. Game data is not included; users must provide their own compatible original data.

## Call of Duty 2 dedicated server

The initial project is an experimental Rust implementation of the Call of Duty 2 1.3 multiplayer dedicated server. See [`games/call-of-duty-2/README.md`](games/call-of-duty-2/README.md) for status and [`games/call-of-duty-2/port/README.md`](games/call-of-duty-2/port/README.md) for build and run instructions.

The server requires an existing Call of Duty 2 installation. Do not commit or distribute original game files, installers, maps, or archives in this repository.

## Licensing

This repository contains components with different licenses. The existing top-level `LICENSE` is MIT. The `cod2-server`, `open-games-vfs`, and `open-games-vfs-inno` packages declare GPL-3.0-only; the GPL text is in [`LICENSES/GPL-3.0-only.txt`](LICENSES/GPL-3.0-only.txt). `open-games-vfs-core` and `open-games-cache` declare MIT. The vendored Inno Setup library retains its upstream `MIT OR Apache-2.0` license. Package manifests are authoritative for each package's license.
