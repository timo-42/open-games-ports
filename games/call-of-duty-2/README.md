# Call of Duty 2 dedicated server

The Rust package in [`port/`](port/) implements a native multiplayer dedicated server for Call of Duty 2 version 1.3. It loads the original game's multiplayer assets from an external installation; no game data is included here. The [`analysis/`](analysis/) directory records binary observations and validation results. [`recon/`](recon/) contains a C++ behavioral reconstruction of the Linux 1.3 dedicated server, with supporting headers and differential harness.

The Rust server can load stock maps and execute game scripts. Stock-client gameplay compatibility remains in development; see the port README for current build and run instructions. The C++ reconstruction is partial and is not, by itself, a supported playable deliverable; its original binary and generated data image are not included.

This project and its documentation are GPL-3.0-only; see [`LICENSE`](LICENSE). Original Call of Duty 2 assets and executables remain separate and are not licensed by this project.
