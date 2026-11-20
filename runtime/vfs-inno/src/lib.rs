//! Game files of Inno Setup installers, including GOG's legacy and
//! Galaxy-era layouts, over any `Read + Seek` installer image.
//!
//! This crate holds the installer indexing that both `open-games-vfs`
//! (native, file-backed) and WebAssembly hosts (installer bytes in browser
//! storage) need. It has no file-system dependency: the caller supplies an
//! [`inno::Inno`] over its own reader (and, for split installers, its own
//! `.bin` slice readers). No installer code or script runs; GOG's install
//! hooks are parsed as data.
//!
//! Three layouts are recognized ([`Layout`]):
//!
//! * plain installers: the `{app}` entries are the game;
//! * legacy GOG installers (the 2015 "2.0.0.x" builds): GOG metadata files
//!   (`goggame-*`) beside ordinary `{app}` members;
//! * Galaxy-era GOG installers: the game is a set of zlib payloads under
//!   `{tmp}/<md5>`. Each file's first payload entry carries
//!   `before_install('<file md5>', '<path>', <parts>)` and every part
//!   `after_install('<part md5>', <packed>, <bytes>)`; the parts of one file
//!   are consecutive entries. Payloads without these hooks are returned for
//!   the caller's `.hashdb` mapping.

use std::io::{Cursor, Read, Seek};
use std::sync::Arc;

use flate2::read::ZlibDecoder;
use inno::Inno;
use md5::{Digest, Md5};

pub type Result<T> = std::result::Result<T, String>;

/// Largest member read into memory.
pub const MAX_MEMBER: u64 = 256 * 1024 * 1024;

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

/// One zlib part of a Galaxy-era GOG installer file: the Inno member holding
/// it, its compressed and decompressed sizes and the MD5 of its bytes.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct GogPart {
    pub index: usize,
    pub packed: u64,
    pub bytes: u64,
    pub md5: String,
}

/// A logical file of a Galaxy-era GOG installer.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct GogFile {
    pub path: String,
    pub md5: String,
    pub bytes: u64,
    pub parts: Vec<GogPart>,
}

/// A `{tmp}/<md5>` payload entry with its Inno install hooks.
#[derive(Clone, Debug)]
pub struct GogEntry {
    pub index: usize,
    pub packed: u64,
    pub before_install: Option<String>,
    pub after_install: Option<String>,
}

/// How an Inno installer stores its game files.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Layout {
    /// No GOG metadata: the `{app}` entries are the game.
    Plain,
    /// Pre-Galaxy GOG installer: GOG metadata files sit in `{app}` beside
    /// ordinary Inno members.
    Legacy,
    /// Galaxy-era GOG installer: zlib payloads under `{tmp}/<md5>`.
    Galaxy,
}

/// Where a member's bytes are.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Payload {
    /// An ordinary Inno file entry (`checksum`: Inno's own, as text).
    Plain {
        index: usize,
        bytes: u64,
        checksum: String,
    },
    /// A Galaxy file of one or more zlib parts.
    Gog {
        parts: Arc<[GogPart]>,
        bytes: u64,
        md5: String,
    },
}

impl Payload {
    pub fn bytes(&self) -> u64 {
        match self {
            Payload::Plain { bytes, .. } | Payload::Gog { bytes, .. } => *bytes,
        }
    }
}

/// A game file: its path below the installation directory in the
/// installer's own spelling (`/`-separated), and its payload.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Member {
    pub path: String,
    pub payload: Payload,
}

/// The game files of an installer.
#[derive(Clone, Debug)]
pub struct Index {
    pub layout: Layout,
    pub members: Vec<Member>,
    /// Galaxy payloads without install hooks: the caller maps them through
    /// the installer's `.hashdb`, or reports them as unsupported.
    pub unmapped_payloads: Vec<GogEntry>,
}

/// Whether `relative` (below `{app}`) is installer metadata, not game data.
fn gog_metadata_file(relative: &str) -> bool {
    !relative.contains('/') && relative.to_ascii_lowercase().starts_with("goggame-")
}

/// `{tmp}/.../<32 hex digits>`: a Galaxy payload.
pub fn gog_temp_path(destination: &str) -> bool {
    let normalized = destination.replace('\\', "/");
    let Some(relative) = normalized.strip_prefix("{tmp}/") else {
        return false;
    };
    let Some(name) = relative.rsplit('/').next() else {
        return false;
    };
    name.len() == 32 && name.bytes().all(|byte| byte.is_ascii_hexdigit())
}

/// Classify an installer from its GOG metadata, its `{tmp}/<md5>` payload
/// count and the number of `{app}` game members.
pub fn layout(
    has_gog_metadata: bool,
    galaxy_payloads: usize,
    app_members: usize,
) -> Result<Layout> {
    match (has_gog_metadata, galaxy_payloads, app_members) {
        (false, _, _) => Ok(Layout::Plain),
        (true, 0, 0) => Err(
            "GOG installer has neither content-addressed temporary payloads nor {app} game files"
                .into(),
        ),
        (true, 0, _) => Ok(Layout::Legacy),
        // Any `{app}` members of a Galaxy installer stay indexed beside the
        // payloads.
        (true, _, _) => Ok(Layout::Galaxy),
    }
}

/// The game files of `inno`. Only file entries are read: no installer
/// condition, script or payload is run or decompressed.
pub fn index<R: Read + Seek>(inno: &Inno<R>) -> Result<Index> {
    let mut members = Vec::new();
    let mut payloads = Vec::new();
    let mut has_gog_metadata = false;
    for (index, file) in inno.file_entries().iter().enumerate() {
        let Some(destination) = file.destination() else {
            continue;
        };
        let Some(location) = inno.file_locations().get(file.location() as usize) else {
            continue;
        };
        if gog_temp_path(destination) {
            payloads.push(GogEntry {
                index,
                packed: location.uncompressed_size(),
                before_install: file.condition().before_install().map(str::to_owned),
                after_install: file.condition().after_install().map(str::to_owned),
            });
            continue;
        }
        // Only game payloads; system, temporary and installer support files
        // never become assets. No installer conditions are run.
        let normalized = destination.replace('\\', "/");
        let Some(relative) = normalized.strip_prefix("{app}/") else {
            continue;
        };
        let relative = relative.trim_start_matches('/');
        if gog_metadata_file(relative) {
            has_gog_metadata |= relative.to_ascii_lowercase().ends_with(".ico");
            continue;
        }
        if relative.eq_ignore_ascii_case("webcache.zip") {
            continue;
        }
        members.push(Member {
            path: relative.to_owned(),
            payload: Payload::Plain {
                index,
                bytes: location.uncompressed_size(),
                checksum: format!("{:?}", location.file().checksum()),
            },
        });
    }
    let layout = layout(has_gog_metadata, payloads.len(), members.len())?;
    let mut unmapped_payloads = Vec::new();
    if layout == Layout::Galaxy {
        match gog_files_from_install_hooks(&payloads)? {
            Some(files) => {
                for file in files {
                    let path = file.path.replace('\\', "/");
                    let path = path.trim_start_matches('/');
                    if gog_metadata_file(path) {
                        continue;
                    }
                    members.push(Member {
                        path: path.to_owned(),
                        payload: Payload::Gog {
                            parts: file.parts.into(),
                            bytes: file.bytes,
                            md5: file.md5,
                        },
                    });
                }
            }
            None => unmapped_payloads = payloads,
        }
    }
    Ok(Index {
        layout,
        members,
        unmapped_payloads,
    })
}

/// The bytes of a member, verified: Inno's own checksum for plain entries,
/// part and whole-file MD5s for GOG files.
pub fn read<R: Read + Seek>(inno: &mut Inno<R>, payload: &Payload) -> Result<Vec<u8>> {
    match payload {
        Payload::Plain { index, bytes, .. } => member_bytes(inno, *index, *bytes),
        Payload::Gog { parts, bytes, md5 } => read_gog_parts(inno, parts, *bytes, md5),
    }
}

/// The stored bytes of Inno file entry `index` (for a Galaxy payload: the
/// zlib stream). Reading to the end verifies Inno's per-file checksum.
pub fn member_bytes<R: Read + Seek>(
    inno: &mut Inno<R>,
    index: usize,
    expected_size: u64,
) -> Result<Vec<u8>> {
    if expected_size > MAX_MEMBER {
        return Err(format!("Installer member {index} exceeds 256 MiB limit"));
    }
    let mut files = inno.streaming_files(|entry| entry.index() == index);
    let (_, mut reader) = files
        .next()
        .ok_or_else(|| format!("Installer member {index} is unavailable"))?
        .map_err(|error| error.to_string())?;
    let mut data = Vec::new();
    reader
        .by_ref()
        .take(MAX_MEMBER + 1)
        .read_to_end(&mut data)
        .map_err(|error| error.to_string())?;
    if data.len() as u64 > MAX_MEMBER {
        return Err(format!("Installer member {index} exceeds 256 MiB limit"));
    }
    let mut eof = [0];
    if reader.read(&mut eof).map_err(|error| error.to_string())? != 0
        || data.len() as u64 != expected_size
    {
        return Err(format!("Installer member {index} length mismatch"));
    }
    Ok(data)
}

/// Arguments of a Pascal Script call such as `name('a''b', 'c', 3)`:
/// quoted strings (with `''` escapes) or bare tokens. `None` unless the
/// whole text is one call of `name`.
pub fn pascal_call(text: &str, name: &str) -> Option<Vec<String>> {
    let rest = text.trim().strip_prefix(name)?.trim_start();
    let mut chars = rest.strip_prefix('(')?.chars().peekable();
    let mut args = Vec::new();
    loop {
        while chars.next_if(|c| c.is_whitespace()).is_some() {}
        let mut arg = String::new();
        if chars.next_if_eq(&'\'').is_some() {
            loop {
                match chars.next()? {
                    '\'' if chars.next_if_eq(&'\'').is_some() => arg.push('\''),
                    '\'' => break,
                    c => arg.push(c),
                }
            }
            while chars.next_if(|c| c.is_whitespace()).is_some() {}
        } else {
            while let Some(c) = chars.next_if(|c| *c != ',' && *c != ')') {
                arg.push(c);
            }
            arg = arg.trim().to_owned();
        }
        args.push(arg);
        match chars.next()? {
            ',' => {}
            ')' => break,
            _ => return None,
        }
    }
    chars.all(char::is_whitespace).then_some(args)
}

fn md5_arg(value: &str) -> Result<String> {
    if value.len() == 32 && value.bytes().all(|b| b.is_ascii_hexdigit()) {
        Ok(value.to_ascii_lowercase())
    } else {
        Err(format!("GOG install hook has a malformed MD5: {value:?}"))
    }
}

fn number_arg(value: &str) -> Result<u64> {
    value
        .parse()
        .map_err(|_| format!("GOG install hook has a malformed number: {value:?}"))
}

/// The file list of a Galaxy-era GOG installer from its install hooks (see
/// the crate documentation). Redistributables (`before_install_dependency`)
/// are skipped. `None` when a payload has no hooks (older Galaxy builds,
/// which the `.hashdb` maps).
pub fn gog_files_from_install_hooks(entries: &[GogEntry]) -> Result<Option<Vec<GogFile>>> {
    let mut files = Vec::new();
    let mut open: Option<(GogFile, u64)> = None;
    let mut dependency_parts = 0u64;
    for entry in entries {
        let before = entry.before_install.as_deref().unwrap_or("");
        let after = entry.after_install.as_deref().unwrap_or("");
        if let Some(args) = pascal_call(before, "before_install_dependency") {
            if open.is_some() || dependency_parts > 0 {
                return Err(format!(
                    "GOG part list of entry {} is interrupted",
                    entry.index
                ));
            }
            let [_, _, parts] = args.as_slice() else {
                return Err(format!(
                    "GOG dependency hook of entry {} is malformed",
                    entry.index
                ));
            };
            dependency_parts = number_arg(parts)?.saturating_sub(1);
            continue;
        }
        if dependency_parts > 0 && pascal_call(after, "after_install_dependency").is_some() {
            dependency_parts -= 1;
            continue;
        }
        if let Some(args) = pascal_call(before, "before_install") {
            if open.is_some() {
                return Err(format!(
                    "GOG part list of entry {} is interrupted",
                    entry.index
                ));
            }
            let [md5, path, parts] = args.as_slice() else {
                return Err(format!(
                    "GOG install hook of entry {} is malformed",
                    entry.index
                ));
            };
            let parts = number_arg(parts)?;
            if parts == 0 {
                return Err(format!("GOG file {path:?} declares no parts"));
            }
            let file = GogFile {
                path: path.clone(),
                md5: md5_arg(md5)?,
                bytes: 0,
                parts: Vec::new(),
            };
            open = Some((file, parts));
        } else if !before.trim().is_empty() || open.is_none() {
            // A payload outside the hook scheme: let the hash database map it.
            return Ok(None);
        }
        let Some(args) = pascal_call(after, "after_install") else {
            return Err(format!(
                "GOG payload entry {} has no after_install part record",
                entry.index
            ));
        };
        let [md5, packed, bytes] = args.as_slice() else {
            return Err(format!(
                "GOG part record of entry {} is malformed",
                entry.index
            ));
        };
        let part = GogPart {
            index: entry.index,
            packed: number_arg(packed)?,
            bytes: number_arg(bytes)?,
            md5: md5_arg(md5)?,
        };
        if part.packed != entry.packed {
            return Err(format!(
                "GOG part record of entry {} says {} packed bytes, the installer stores {}",
                entry.index, part.packed, entry.packed
            ));
        }
        let Some((file, remaining)) = open.as_mut() else {
            return Err(format!("GOG part {} belongs to no file", entry.index));
        };
        file.bytes = file
            .bytes
            .checked_add(part.bytes)
            .filter(|total| *total <= MAX_MEMBER)
            .ok_or_else(|| format!("GOG file {:?} exceeds 256 MiB", file.path))?;
        file.parts.push(part);
        *remaining -= 1;
        if *remaining == 0
            && let Some((file, _)) = open.take()
        {
            files.push(file);
        }
    }
    if let Some((file, _)) = open {
        return Err(format!("GOG file {:?} is missing parts", file.path));
    }
    if dependency_parts > 0 {
        return Err("GOG dependency is missing parts".into());
    }
    Ok((!files.is_empty()).then_some(files))
}

/// Decompress and verify `parts` (one zlib stream each) and the whole file.
pub fn read_gog_parts<R: Read + Seek>(
    inno: &mut Inno<R>,
    parts: &[GogPart],
    bytes: u64,
    md5: &str,
) -> Result<Vec<u8>> {
    let mut sources = Vec::with_capacity(parts.len());
    for part in parts {
        sources.push(member_bytes(inno, part.index, part.packed)?);
    }
    join_gog_parts(parts, &sources, bytes, md5)
}

/// Decompress the stored zlib streams of `parts` (`stored[i]` for
/// `parts[i]`) and verify each part's and the whole file's MD5.
pub fn join_gog_parts(
    parts: &[GogPart],
    stored: &[Vec<u8>],
    bytes: u64,
    md5: &str,
) -> Result<Vec<u8>> {
    if bytes > MAX_MEMBER {
        return Err("GOG member exceeds 256 MiB limit".into());
    }
    if parts.len() != stored.len() {
        return Err("GOG part count mismatch".into());
    }
    let mut data = Vec::with_capacity(bytes as usize);
    let mut whole = Md5::new();
    for (part, outer) in parts.iter().zip(stored) {
        let start = data.len();
        ZlibDecoder::new(Cursor::new(outer))
            .take(part.bytes + 1)
            .read_to_end(&mut data)
            .map_err(|error| format!("Decompress GOG part {}: {error}", part.index))?;
        let decoded = &data[start..];
        if decoded.len() as u64 != part.bytes {
            return Err(format!("GOG part {} length mismatch", part.index));
        }
        let actual = hex(&Md5::digest(decoded));
        if actual != part.md5 {
            return Err(format!(
                "GOG part {} MD5 mismatch: expected {}, got {actual}",
                part.index, part.md5
            ));
        }
        whole.update(decoded);
    }
    if data.len() as u64 != bytes {
        return Err("GOG member length changed after indexing".into());
    }
    let actual = hex(&whole.finalize());
    if actual != md5 {
        return Err(format!(
            "GOG member MD5 mismatch: expected {md5}, got {actual}"
        ));
    }
    Ok(data)
}

#[cfg(test)]
mod tests;
