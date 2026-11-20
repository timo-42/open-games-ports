# Original-data VFS

`open-games-vfs` provides read-only access to original game directories and supported source containers.
It does not run the original executable or unpack files into the source tree.

The shared crate also exports `AssetVfs` and `AssetStream` for seekable directory,
ISO9660 and nested ZIP asset access, with ordered loose-file overlays. Underground
2 uses this streaming interface. The crate also retains bounded readers for EA
BIG archives and RefPack-compressed payloads, used by supported Need for Speed
asset formats.

```rust
use open_games_vfs::AssetVfs;

let game = AssetVfs::iso_discs(&["CD1.iso".into(), "CD2.iso".into()], &[])?;
let mut geometry = game.open("CARS/240SX/GEOMETRY.BIN")?;
```

ISO files remain on disk. A requested ZIP member is decompressed into a seekable
memory stream, bounded to 256 MiB and checked against its ZIP CRC. ISO source
members are fingerprinted with SHA-256. Selected Inno ZIP containers may be
copied into a disposable cache as described below; no installer executes.
Only primary ISO9660 volumes with 2048-byte sectors and
stored/Deflate `COMPRESS.ZIP` payloads have been validated. Other disc layouts,
installers, binary deltas and native mod logic remain outstanding; see
[Underground 2's media coverage](../../games/nfs-underground-2/port/README.md).

`archive::Archive::parse` reads EA BIGF/BIG4 indexes, retaining member names,
byte ranges and declared-size warnings. Lookup rejects ambiguous names and unsafe
paths; physical bounds govern reads when the declared size differs from the actual
input size. Partial payload overlap and truncated entries are rejected, while
identical shared ranges are allowed. `Archive::read` also decodes bounded
five-byte `0x10FB` RefPack member wrappers. Other RefPack profiles are
unsupported, and member names are never written to the filesystem.

`sha256` and `sha256_hex` fingerprint raw resource bytes for evidence and
multiplayer content identity. They do not authenticate an untrusted peer.

Run the synthetic fixtures independently with:

```sh
cargo test --locked -p open-games-vfs
cargo clippy --locked -p open-games-vfs --all-targets -- -D warnings
```

The `layers` API mounts its sources through the same `AssetVfs` stream abstraction and adds read-only, case-insensitive directory and ordinary Inno installer mounts with ordered whole-file update/mod overlays, SHA-256 source identities and seekable file streams. RCT uses this API directly. `Layer::open_with_known_artifacts` indexes a directory or installer without rehashing its full payload set; supplied manifest hashes remain recorded identities, while each opened Inno member is verified against its installer checksum. A caller must record separately whether those manifest hashes were rechecked. ISO9660 and nested ZIP mounts are supported through `AssetVfs`; other disc formats, Galaxy logical file reconstruction and binary patch reconstruction remain unsupported.

`Layer::tar_bz2_member` mounts one named regular file from a SHA-256-pinned tar.bz2 package and verifies the member digest. The tar reader and bzip2 decoder provide sequential access; the VFS spools the selected member to a disposable seekable file, so nested ZIP readers can seek independently while memory use stays bounded. CoD2 uses this path to mount its official 1.3 multiplayer data update. [tar archive reader](https://docs.rs/tar/0.4.46/tar/struct.Archive.html) · [bzip2 streaming decoder](https://docs.rs/bzip2/0.6.1/bzip2/read/struct.BzDecoder.html)

The pinned `inno` 0.7.0 crate lacked the `idskb32` disk-slice variant used by the supplied Battlefield 2 repack. A narrow vendored patch in `runtime/vfs/vendor/inno` accepts its 64-bit slice size and 16-byte header, following [Inno Setup's slice structure](https://github.com/jrsoftware/issrc/blob/main/Projects/Src/Shared.Struct.pas). Inno ZIP containers requested through `Layer` are copied on demand to a disposable, checksum-verified cache so the ZIP index has a seekable stream without loading a 641 MB archive into RAM. `prefetch_installer_zips` fills several requested ZIPs in one forward pass over solid installer chunks. The cache uses `.local/open-games-vfs-cache` in debug builds, the platform user-cache directory in release builds, or `OPEN_GAMES_VFS_CACHE_DIR` when set. It is bounded to 2 GiB per container; decoded ZIP members retain the 256 MiB limit. The original media is not modified.

GOG installers are indexed by the wasm-capable
[`open-games-vfs-inno`](../vfs-inno/README.md) crate, which the Anno 1602
browser host also uses. Legacy GOG installers (the 2015 "2.0.0.x" builds:
`goggame-*` metadata beside ordinary `{app}` members) are read as plain Inno
installers; they were rejected before. Galaxy-era installers (`{tmp}/<md5>`
zlib payloads) are mapped from the installer's own `before_install`/
`after_install` hooks: no payload is decompressed at mount, files split into
several parts are joined on open, and every part MD5 and the whole-file MD5
are checked before bytes are exposed. Installers whose payloads carry no hooks
keep the earlier `.hashdb` mapping (whole-file MD5, decompressing each payload
at mount; multi-part files are then omitted with a warning).
Redistributables (`before_install_dependency`, `__redist/`) and `goggame-*`
metadata are not exposed. No `check_if_install` language condition is
evaluated; a path listed twice with different content is rejected. Validated
on the four supplied Anno 1602 GOG installers
([gog-editions.md](../../games/anno-1602/analysis/gog-editions.md)); the
supplied Rollercoaster Tycoon 77749 Galaxy installer now lists 504 files
(game adapter not revalidated). `Layer::original_name` and
`Sources::original_name` give the installer's own spelling of a member path
(lookups stay case-insensitive), for games that show or store file names.

A second local patch to the vendored `inno` 0.7.0 keeps each file entry's
condition (`File::condition`: components, tasks, languages, check and the
install hooks above; upstream reads and discards it) and lets a caller supply
`.bin` slice readers instead of files beside the installer
(`Inno::with_slices`, `Inno::slice_file_name`, `SliceOpener`), for installers
held in browser storage.

`layers::Layer::open` also supports PE32 resources using the `pelite` crate. It
indexes the resource directory and exposes each payload at
`resources/<type>/<name>/<language>.bin`; opening a payload reads from its
file-backed byte range, without extracting or loading the complete executable.
PE32+ resources and unusual PE resource layouts have not been validated.

The [inspection tools](../vfs-tools/README.md) expose the resolved `Sources` view
through an extraction CLI, a versioned JSON Lines protocol and optional
read-only FUSE. `Sources::file_info` reports size and the winning layer through
the same overlay used for reads. `Layer::open` also accepts ISO9660/2048 images
using the existing ISO/nested ZIP backend; this adds no new disc-format coverage.

The vendored Inno directory retains `LICENSE-MIT.md` and `LICENSE-APACHE.md`.
These are byte-for-byte copies of the upstream 0.7.0 revision
`373a35233ebd0ce5fb6b517e83c687353532ceba`, verified against the SHA-256s in
`games/battlefield-2/docs/licenses/infantry-engine-fallback-notices.json`.
The `idskb32` modification, the kept file conditions and the slice readers
described above remain local patches.
