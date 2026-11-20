# `open-games-vfs-inno`: Inno Setup and GOG installer indexing

Game files of Inno Setup installers over any `Read + Seek` image, with no
file-system dependency, so the same code serves `open-games-vfs` (native,
file-backed) and WebAssembly hosts (installer bytes in browser storage). No
installer code or Pascal script runs; GOG's install hooks are parsed as data.

| Layout | Recognized by | Members |
| --- | --- | --- |
| plain Inno | no `goggame-*.ico` in `{app}` | `{app}` entries |
| legacy GOG (2015 "2.0.0.x") | GOG metadata beside `{app}` entries, no `{tmp}/<md5>` payloads | `{app}` entries without `goggame-*` and `webcache.zip` |
| Galaxy GOG | `{tmp}/<md5>` zlib payloads | files from `before_install('<md5>', '<path>', <parts>)` + one `after_install('<md5>', <packed>, <bytes>)` per part (consecutive entries); payloads without hooks are returned for the caller's `.hashdb` mapping |

`index` lists the members (installer spelling, `/`-separated) without
decompressing anything; `read` returns a member's bytes after Inno's
checksum (plain entries) or every part's and the whole file's MD5 (Galaxy)
were verified. Members are bounded to 256 MiB in memory. Split installers
need their `.bin` slices: `Inno::open` finds them beside a file, and
`Inno::with_slices` (a local patch of the vendored `inno` crate, see
[`../vfs/README.md`](../vfs/README.md)) takes readers from the caller.

```sh
cargo test -p open-games-vfs-inno        # synthetic hook lists and zlib parts
```

Real installers must be supplied by the user; no installer media is included.
Galaxy language conditions (`check_if_install`) are not evaluated; other Inno
features (encryption, `[Code]`-driven layouts) are not covered.
