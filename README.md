# Open Games Ports

This repository is being phased in with selected, native game-server ports. Game data is not included; users must provide their own compatible original data.

## Call of Duty 2 dedicated server

The initial project is an experimental Rust implementation of the Call of Duty 2 1.3 multiplayer dedicated server. See [`games/call-of-duty-2/README.md`](games/call-of-duty-2/README.md) for status, [`games/call-of-duty-2/port/README.md`](games/call-of-duty-2/port/README.md) for build and run instructions, [`games/call-of-duty-2/analysis/`](games/call-of-duty-2/analysis/) for evidence and test notes, and [`games/call-of-duty-2/recon/`](games/call-of-duty-2/recon/) for the C++ behavioral reconstruction.

The server requires an existing Call of Duty 2 installation. The C++ reconstruction is not a standalone playable server and also depends on user-supplied original artifacts for its differential harness. Do not commit or distribute original game files, installers, binaries, maps, archives, or generated data images in this repository.

## Releases

Releases are published from the default branch by running the **Release** workflow (Actions → Release → Run workflow) and choosing which version component to increment. The workflow uses [`vergit-action`](https://github.com/timo-42/vergit-action) to derive the next version, creates and pushes the tag, builds `cod2-server` for Linux (x86_64), macOS (Apple silicon) and Windows (x86_64), and attaches one `.zip` per platform to the GitHub release.

## Licensing

This repository contains components with different licenses. The existing top-level `LICENSE` is MIT. The Call of Duty 2 project (including its C++ reconstruction and project documentation), `cod2-server`, `open-games-vfs`, and `open-games-vfs-inno` are GPL-3.0-only; see [`games/call-of-duty-2/LICENSE`](games/call-of-duty-2/LICENSE) and [`LICENSES/GPL-3.0-only.txt`](LICENSES/GPL-3.0-only.txt). `open-games-vfs-core` and `open-games-cache` declare MIT. The vendored Inno Setup library retains its upstream `MIT OR Apache-2.0` license. Package manifests and component license files identify exceptions.
