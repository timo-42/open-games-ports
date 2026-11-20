//! Read-only, case-insensitive game sources over the `vfs` crate. Installers
//! are data: Inno parses and verifies individual members without running code.
#![forbid(unsafe_code)]

use ::zip::ZipArchive;
use flate2::read::ZlibDecoder;
use inno::Inno;
use md5::Md5;
use open_games_vfs_inno::{self as innoidx, GogPart};
#[cfg(feature = "optical-discs")]
use opticaldiscs::browse::{EntryType, FileEntry, Filesystem, open_disc_filesystem};
#[cfg(feature = "optical-discs")]
use opticaldiscs::detect::DiscImageInfo;
#[cfg(feature = "optical-discs")]
use opticaldiscs::formats::DiscFormat;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::fs::{self, File};
use std::io::{self, Cursor, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use vfs::error::VfsErrorKind;
use vfs::{FileSystem, SeekAndRead, SeekAndWrite, VfsFileType, VfsMetadata, VfsPath, VfsResult};

mod installshield;
mod nsis;
mod pe;
mod rar;
pub mod setup;
mod tar_bz;
mod zip;

pub type Result<T> = std::result::Result<T, String>;
pub type FileStream = Box<dyn SeekAndRead + Send>;
const MAX_MEMBER: u64 = 256 * 1024 * 1024;
static CACHE_TEMP_ID: AtomicU64 = AtomicU64::new(0);

/// Names are Windows-style virtual paths, never backing filesystem paths.
pub fn virtual_path(path: &str) -> Result<String> {
    let path = path.replace('\\', "/");
    if path.is_empty()
        || path.starts_with('/')
        || path.contains([':', '\0'])
        || path
            .split('/')
            .any(|p| p.is_empty() || p == "." || p == ".." || p.eq_ignore_ascii_case(".whiteout"))
    {
        return Err(format!("Invalid virtual path: {path:?}"));
    }
    Ok(path.to_lowercase())
}

fn failure(error: impl std::fmt::Display) -> vfs::VfsError {
    VfsErrorKind::Other(error.to_string()).into()
}

#[derive(Clone, Debug)]
pub struct Artifact {
    pub name: String,
    pub bytes: u64,
    pub sha256: String,
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

fn fingerprint(path: &Path, name: String) -> Result<Artifact> {
    let mut reader = File::open(path).map_err(|e| e.to_string())?;
    let bytes = reader.metadata().map_err(|e| e.to_string())?.len();
    let mut hash = Sha256::new();
    let mut block = [0; 65536];
    loop {
        let n = reader.read(&mut block).map_err(|e| e.to_string())?;
        if n == 0 {
            break;
        }
        hash.update(&block[..n]);
    }
    Ok(Artifact {
        name,
        bytes,
        sha256: hex(&hash.finalize()),
    })
}

fn artifact_identity(artifacts: &[Artifact], version: &str) -> String {
    let mut hash = Sha256::new();
    hash.update((version.len() as u64).to_le_bytes());
    hash.update(version.as_bytes());
    for artifact in artifacts {
        for value in [&artifact.name, &artifact.sha256] {
            hash.update((value.len() as u64).to_le_bytes());
            hash.update(value.as_bytes());
        }
    }
    hex(&hash.finalize())
}

fn cached_rfa_is_valid(path: &Path, manifest_path: &Path, expected_bytes: u64) -> bool {
    let Ok(metadata) = fs::symlink_metadata(path) else {
        return false;
    };
    if metadata.file_type().is_symlink() || !metadata.is_file() || metadata.len() != expected_bytes
    {
        return false;
    }
    let Ok(manifest) = fs::read_to_string(manifest_path) else {
        return false;
    };
    let mut fields = manifest.split_whitespace();
    if fields.next() != Some("1")
        || fields.next().and_then(|size| size.parse::<u64>().ok()) != Some(expected_bytes)
    {
        return false;
    }
    let Some(expected_hash) = fields.next() else {
        return false;
    };
    if fields.next().is_some() {
        return false;
    }
    let Ok(mut file) = File::open(path) else {
        return false;
    };
    let mut hash = Sha256::new();
    let mut block = [0; 64 * 1024];
    loop {
        match file.read(&mut block) {
            Ok(0) => return hex(&hash.finalize()) == expected_hash,
            Ok(count) => hash.update(&block[..count]),
            Err(_) => return false,
        }
    }
}

fn inno_zip_cache_paths(
    installer: &Path,
    index: usize,
    expected_bytes: u64,
    checksum: &str,
) -> Result<(PathBuf, PathBuf)> {
    const MAX_CACHED_CONTAINER: u64 = 2 * 1024 * 1024 * 1024;
    if expected_bytes > MAX_CACHED_CONTAINER {
        return Err(format!(
            "Installer container {index} exceeds the 2 GiB stream-cache limit"
        ));
    }
    let root = if let Some(path) = std::env::var_os("OPEN_GAMES_VFS_CACHE_DIR") {
        PathBuf::from(path)
    } else if cfg!(debug_assertions) {
        PathBuf::from(".local/open-games-vfs-cache")
    } else {
        directories::ProjectDirs::from("org", "open-games", "vfs")
            .ok_or("No platform cache directory is available")?
            .cache_dir()
            .to_path_buf()
    };
    let mut key = Sha256::new();
    let installer_name = installer.to_string_lossy();
    for value in [installer_name.as_ref(), checksum] {
        key.update((value.len() as u64).to_le_bytes());
        key.update(value.as_bytes());
    }
    key.update(index.to_le_bytes());
    key.update(expected_bytes.to_le_bytes());
    let directory = root.join("inno-container-v1").join(hex(&key.finalize()));
    fs::create_dir_all(&directory).map_err(|error| error.to_string())?;
    Ok((
        directory.join("member.bin"),
        directory.join("member.sha256"),
    ))
}

fn save_inno_zip_reader(
    reader: &mut impl Read,
    index: usize,
    expected_bytes: u64,
    cached: &Path,
    manifest: &Path,
) -> Result<()> {
    let suffix = format!(
        "{}.{}",
        std::process::id(),
        CACHE_TEMP_ID.fetch_add(1, Ordering::Relaxed)
    );
    let directory = cached.parent().ok_or("Missing ZIP cache directory")?;
    let temporary = directory.join(format!("member.{suffix}.tmp"));
    let manifest_temporary = directory.join(format!("member.{suffix}.sha256.tmp"));
    let result = (|| {
        let mut output = File::create(&temporary).map_err(|error| error.to_string())?;
        let mut hash = Sha256::new();
        let mut copied = 0u64;
        let mut block = [0u8; 64 * 1024];
        loop {
            let count = reader.read(&mut block).map_err(|error| error.to_string())?;
            if count == 0 {
                break;
            }
            copied = copied
                .checked_add(count as u64)
                .ok_or("Installer container size overflow")?;
            if copied > expected_bytes {
                return Err(format!(
                    "Installer member {index} exceeds its declared size"
                ));
            }
            io::Write::write_all(&mut output, &block[..count])
                .map_err(|error| error.to_string())?;
            hash.update(&block[..count]);
        }
        if copied != expected_bytes {
            return Err(format!("Installer member {index} length mismatch"));
        }
        output.sync_all().map_err(|error| error.to_string())?;
        fs::write(
            &manifest_temporary,
            format!("1 {copied} {}\n", hex(&hash.finalize())),
        )
        .map_err(|error| error.to_string())?;
        fs::rename(&temporary, cached).map_err(|error| error.to_string())?;
        fs::rename(&manifest_temporary, manifest).map_err(|error| error.to_string())?;
        Ok(())
    })();
    if result.is_err() {
        let _ = fs::remove_file(temporary);
        let _ = fs::remove_file(manifest_temporary);
    }
    result
}

/// ZIP containers in split Inno media are seekable through a disposable,
/// verified cache. A member may sit after many gigabytes in a solid stream,
/// so reopening it directly would rescan those bytes on every ZIP lookup.
/// ZIP entries are still decoded individually with their own limits.
fn open_cached_inno_zip(
    installer: &Path,
    index: usize,
    expected_bytes: u64,
    checksum: &str,
) -> Result<File> {
    let (cached, manifest) = inno_zip_cache_paths(installer, index, expected_bytes, checksum)?;
    if cached_rfa_is_valid(&cached, &manifest, expected_bytes) {
        return File::open(cached).map_err(|error| error.to_string());
    }
    let mut inno = Inno::open(installer).map_err(|error| error.to_string())?;
    let mut files = inno.streaming_files(|entry| entry.index() == index);
    let (_, mut reader) = files
        .next()
        .ok_or_else(|| format!("Installer member {index} is unavailable"))?
        .map_err(|error| error.to_string())?;
    save_inno_zip_reader(&mut reader, index, expected_bytes, &cached, &manifest)?;
    File::open(cached).map_err(|error| error.to_string())
}

/// Publish an immutable materialized file into another profile without
/// duplicating its bytes on filesystems supporting hard links. Both paths live
/// in the managed game namespace; clear removes the blobs and profile links.
fn share_cached_file(source: &Path, destination: &Path) -> Result<()> {
    share_cached_file_with_link(source, destination, |from, to| fs::hard_link(from, to))
}

fn share_cached_file_with_link(
    source: &Path,
    destination: &Path,
    link: impl FnOnce(&Path, &Path) -> std::io::Result<()>,
) -> Result<()> {
    let parent = destination.parent().ok_or("cache entry has no parent")?;
    let staged = tempfile::tempdir_in(parent).map_err(|error| error.to_string())?;
    let temporary = staged.path().join("entry");
    if link(source, &temporary).is_err() {
        fs::copy(source, &temporary).map_err(|error| error.to_string())?;
    }
    fs::rename(temporary, destination).map_err(|error| error.to_string())
}

fn same_cached_file(source: &Path, destination: &Path) -> bool {
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        if let (Ok(a), Ok(b)) = (
            fs::symlink_metadata(source),
            fs::symlink_metadata(destination),
        ) {
            return a.is_file() && b.is_file() && a.dev() == b.dev() && a.ino() == b.ino();
        }
    }
    let _ = (source, destination);
    false
}

fn publish_cached_rfa(source: &Path, destination: &Path, expected_bytes: u64) -> Result<()> {
    let manifest = destination.with_extension("sha256");
    let source_manifest = source.with_extension("sha256");
    if (same_cached_file(source, destination) && same_cached_file(&source_manifest, &manifest))
        || cached_rfa_is_valid(destination, &manifest, expected_bytes)
    {
        return Ok(());
    }
    share_cached_file(source, destination)?;
    share_cached_file(&source_manifest, &manifest)
}

/// Cache selected installer RFA files in one ordered streaming pass. Opening
/// each Inno member independently restarts its chunk reader and can repeatedly
/// scan gigabytes of split installer payloads.
fn cache_inno_rfas(
    source: &InstallerRfaSource,
    module_path: &str,
    archives: &[(String, VfsPath)],
    cache: &open_games_cache::Session,
) -> Result<BTreeMap<String, PathBuf>> {
    // Resolved entries remain keyed by the complete ordered source profile.
    // Immutable installer members also have source/decoder-bound backing blobs,
    // shared across profiles so switching a module/update/mod never re-extracts
    // or stores another full copy of unchanged base RFAs.
    let profile_dir = cache.entry("installer-rfa-v1", &[&source.identity, module_path])?;
    let cache_dir = cache
        .with_profile(&[])?
        .entry("installer-rfa-blobs-v1", &[&source.identity])?;

    let mut output_paths = BTreeMap::new();
    let mut missing = BTreeMap::new();
    for (name, _) in archives {
        let full_path = virtual_path(&format!("{module_path}/{name}"))?;
        let Some(member) = source.members.get(&full_path) else {
            continue;
        };
        if member.bytes > MAX_MEMBER {
            return Err(format!(
                "Installer RFA {full_path} exceeds the {} MiB member limit",
                MAX_MEMBER / (1024 * 1024)
            ));
        }
        let path = cache_dir.join(format!("{}.rfa", member.index));
        let manifest_path = cache_dir.join(format!("{}.sha256", member.index));
        // Reuse a valid pre-deduplication entry from this profile as well.
        let previous = profile_dir.join(format!("{}.rfa", member.index));
        let previous_manifest = profile_dir.join(format!("{}.sha256", member.index));
        let mut valid = cached_rfa_is_valid(&path, &manifest_path, member.bytes);
        if !valid && cached_rfa_is_valid(&previous, &previous_manifest, member.bytes) {
            share_cached_file(&previous, &path)?;
            share_cached_file(&previous_manifest, &manifest_path)?;
            valid = true;
        }
        if valid {
            output_paths.insert(name.clone(), path);
        } else {
            let temporary = cache_dir.join(format!(
                "{}.rfa.{}.{}.tmp",
                member.index,
                std::process::id(),
                CACHE_TEMP_ID.fetch_add(1, Ordering::Relaxed)
            ));
            let manifest_temporary = cache_dir.join(format!(
                "{}.sha256.{}.{}.tmp",
                member.index,
                std::process::id(),
                CACHE_TEMP_ID.fetch_add(1, Ordering::Relaxed)
            ));
            missing.insert(
                member.index,
                (
                    name.clone(),
                    member.bytes,
                    path,
                    manifest_path,
                    temporary,
                    manifest_temporary,
                ),
            );
        }
    }

    if !missing.is_empty() {
        let mut inno = Inno::open(&source.path)
            .map_err(|error| format!("Open installer for RFA cache: {error}"))?;
        let selected: BTreeSet<_> = missing.keys().copied().collect();
        let mut files = inno.streaming_files(|entry| selected.contains(&entry.index()));
        while let Some(item) = files.next() {
            let (entry, mut reader) =
                item.map_err(|error| format!("Read installer RFA member: {error}"))?;
            let index = entry.index();
            let Some((_, expected_bytes, path, manifest_path, temporary, manifest_temporary)) =
                missing.get(&index)
            else {
                return Err(format!("Unexpected installer RFA entry {index}"));
            };
            let result = (|| -> Result<()> {
                let mut output = File::create(temporary)
                    .map_err(|error| format!("Create RFA cache file: {error}"))?;
                let mut hash = Sha256::new();
                let mut bytes = 0u64;
                let mut block = [0; 64 * 1024];
                loop {
                    let count = reader
                        .read(&mut block)
                        .map_err(|error| format!("Extract installer RFA member: {error}"))?;
                    if count == 0 {
                        break;
                    }
                    bytes = bytes
                        .checked_add(count as u64)
                        .ok_or("Installer RFA output size overflow")?;
                    if bytes > *expected_bytes {
                        return Err("Installer RFA exceeds its indexed length".into());
                    }
                    std::io::Write::write_all(&mut output, &block[..count])
                        .map_err(|error| format!("Write RFA cache file: {error}"))?;
                    hash.update(&block[..count]);
                }
                if bytes != *expected_bytes {
                    return Err(format!(
                        "Installer RFA length mismatch: expected {expected_bytes}, got {bytes}"
                    ));
                }
                output.sync_all().map_err(|error| error.to_string())?;
                drop(output);
                let digest = hex(&hash.finalize());
                fs::rename(temporary, path).map_err(|error| {
                    format!("Publish installer RFA cache {}: {error}", path.display())
                })?;
                let mut manifest = File::create(manifest_temporary)
                    .map_err(|error| format!("Create RFA cache manifest: {error}"))?;
                std::io::Write::write_all(
                    &mut manifest,
                    format!("1 {bytes} {digest}\n").as_bytes(),
                )
                .map_err(|error| format!("Write RFA cache manifest: {error}"))?;
                manifest.sync_all().map_err(|error| error.to_string())?;
                drop(manifest);
                fs::rename(manifest_temporary, manifest_path)
                    .map_err(|error| format!("Publish RFA cache manifest: {error}"))?;
                Ok(())
            })();
            if let Err(error) = result {
                let _ = fs::remove_file(temporary);
                let _ = fs::remove_file(manifest_temporary);
                return Err(error);
            }
        }
        for (index, (_, expected_bytes, path, manifest_path, _, _)) in &missing {
            if !cached_rfa_is_valid(path, manifest_path, *expected_bytes) {
                return Err(format!(
                    "Installer did not yield a valid RFA cache entry for index {index}"
                ));
            }
        }
        for (name, _, path, _, _, _) in missing.values() {
            output_paths.insert(name.clone(), path.clone());
        }
    }
    for path in output_paths.values_mut() {
        let name = path.file_name().ok_or("cache member has no name")?;
        let resolved = profile_dir.join(name);
        let bytes = fs::metadata(&*path)
            .map_err(|error| error.to_string())?
            .len();
        publish_cached_rfa(path, &resolved, bytes)?;
        *path = resolved;
    }
    Ok(output_paths)
}

#[derive(Clone)]
enum Member {
    Spooled {
        file: Arc<tempfile::NamedTempFile>,
        bytes: u64,
    },
    Nsis {
        archive: Arc<nsis::Archive>,
        entry: nsis::Entry,
        identity: String,
    },
    InstallShield {
        archive: Arc<installshield::Archive>,
        entry: installshield::Entry,
        identity: String,
    },
    Directory {
        path: PathBuf,
        bytes: u64,
    },
    Inno {
        index: usize,
        bytes: u64,
        checksum: String,
    },
    /// A Galaxy-era GOG file: one or more zlib parts, each an Inno member.
    InnoGog {
        parts: Arc<[GogPart]>,
        bytes: u64,
        md5: String,
    },
    PeResource {
        source: PathBuf,
        offset: u64,
        bytes: u64,
    },
    Rar {
        archive: Arc<rar::Archive>,
        index: usize,
        bytes: u64,
    },
    Zip {
        archive: Arc<Mutex<zip::Archive>>,
        disk_backed: bool,
        path: String,
        bytes: u64,
    },
    Virtual {
        path: VfsPath,
        bytes: u64,
    },
    #[cfg(feature = "optical-discs")]
    Disc {
        filesystem: SharedDiscFilesystem,
        entry: FileEntry,
        bytes: u64,
    },
    VfsFile {
        path: VfsPath,
        bytes: u64,
    },
    Rfa {
        archive: Arc<crate::rfa::Archive>,
        entry: crate::rfa::Entry,
    },
}
impl Member {
    fn bytes(&self) -> u64 {
        match self {
            Self::Directory { bytes, .. }
            | Self::Spooled { bytes, .. }
            | Self::Inno { bytes, .. }
            | Self::InnoGog { bytes, .. }
            | Self::PeResource { bytes, .. }
            | Self::Rar { bytes, .. }
            | Self::Zip { bytes, .. }
            | Self::Virtual { bytes, .. } => *bytes,
            #[cfg(feature = "optical-discs")]
            Self::Disc { bytes, .. } => *bytes,
            Self::VfsFile { bytes, .. } => *bytes,
            Self::Nsis { .. } => 0, // Metadata resolves length on demand below.
            Self::Rfa { entry, .. } => entry.bytes(),
            Self::InstallShield { entry, .. } => entry.bytes,
        }
    }
}

struct SourceFs {
    installer: Option<PathBuf>,
    members: BTreeMap<String, Member>,
    directories: BTreeSet<String>,
}

#[derive(Clone, Debug)]
struct InstallerRfaMember {
    index: usize,
    bytes: u64,
    checksum: String,
}

#[derive(Clone, Debug)]
struct InstallerRfaSource {
    path: PathBuf,
    identity: String,
    members: BTreeMap<String, InstallerRfaMember>,
}

impl std::fmt::Debug for SourceFs {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter
            .debug_struct("SourceFs")
            .field("installer", &self.installer)
            .field("member_count", &self.members.len())
            .field("directory_count", &self.directories.len())
            .finish()
    }
}

#[cfg(feature = "optical-discs")]
type SharedDiscFilesystem = Arc<Mutex<Box<dyn Filesystem>>>;

/// Disc track details retained by the source layer.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct DiscTrackMetadata {
    pub number: u32,
    pub format: String,
    pub start_lba: u64,
    pub length_sectors: u64,
}

/// The disc identity and layout observed by the container reader.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct DiscMetadata {
    pub format: String,
    pub filesystem: String,
    pub volume_label: Option<String>,
    pub tracks: Vec<DiscTrackMetadata>,
}

/// A stored track stream. CloneCD main-channel sectors and the separate
/// 96-byte subchannel records are available without exposing backing paths.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TrackChannel {
    MainChannel,
    Subchannel,
}

#[derive(Clone, Debug)]
struct TrackSource {
    number: u32,
    channel: TrackChannel,
    path: PathBuf,
    offset: u64,
    bytes: u64,
}

struct FileSlice {
    file: File,
    offset: u64,
    length: u64,
    cursor: u64,
}

impl Read for FileSlice {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        if self.cursor >= self.length || buffer.is_empty() {
            return Ok(0);
        }
        let remaining = self.length - self.cursor;
        let length = buffer.len().min(remaining as usize);
        self.file.seek(SeekFrom::Start(self.offset + self.cursor))?;
        let count = self.file.read(&mut buffer[..length])?;
        self.cursor += count as u64;
        Ok(count)
    }
}

impl Seek for FileSlice {
    fn seek(&mut self, position: SeekFrom) -> io::Result<u64> {
        let cursor = match position {
            SeekFrom::Start(offset) => i128::from(offset),
            SeekFrom::End(offset) => i128::from(self.length) + i128::from(offset),
            SeekFrom::Current(offset) => i128::from(self.cursor) + i128::from(offset),
        };
        if cursor < 0 || cursor > i128::from(u64::MAX) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "track seek is outside the addressable range",
            ));
        }
        self.cursor = cursor as u64;
        Ok(self.cursor)
    }
}

#[cfg(feature = "optical-discs")]
struct DiscFileStream {
    filesystem: SharedDiscFilesystem,
    entry: FileEntry,
    cursor: u64,
}

#[cfg(feature = "optical-discs")]
impl Read for DiscFileStream {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        if self.cursor >= self.entry.size || buffer.is_empty() {
            return Ok(0);
        }
        let remaining = self.entry.size - self.cursor;
        let length = buffer
            .len()
            .min(usize::try_from(remaining).unwrap_or(usize::MAX));
        let bytes = self
            .filesystem
            .lock()
            .map_err(|_| io::Error::other("disc filesystem lock was poisoned"))?
            .read_file_range(&self.entry, self.cursor, length)
            .map_err(|error| io::Error::other(error.to_string()))?;
        if bytes.is_empty() {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "disc file ended before its declared size",
            ));
        }
        buffer[..bytes.len()].copy_from_slice(&bytes);
        self.cursor += bytes.len() as u64;
        Ok(bytes.len())
    }
}

#[cfg(feature = "optical-discs")]
impl Seek for DiscFileStream {
    fn seek(&mut self, position: SeekFrom) -> io::Result<u64> {
        let cursor = match position {
            SeekFrom::Start(offset) => i128::from(offset),
            SeekFrom::End(offset) => i128::from(self.entry.size) + i128::from(offset),
            SeekFrom::Current(offset) => i128::from(self.cursor) + i128::from(offset),
        };
        if cursor < 0 || cursor > i128::from(u64::MAX) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "disc file seek is outside the addressable range",
            ));
        }
        self.cursor = cursor as u64;
        Ok(self.cursor)
    }
}

#[cfg(feature = "optical-discs")]
fn index_disc_directory(
    filesystem: &SharedDiscFilesystem,
    entry: FileEntry,
    members: &mut BTreeMap<String, Member>,
    directories: &mut BTreeSet<String>,
) -> Result<()> {
    let relative = entry.path.trim_start_matches('/');
    let directory_key = if relative.is_empty() {
        String::new()
    } else {
        format!("/{}", virtual_path(relative)?)
    };
    if entry.entry_type == EntryType::Directory {
        directories.insert(directory_key);
        let children = filesystem
            .lock()
            .map_err(|_| "disc filesystem lock was poisoned")?
            .list_directory(&entry)
            .map_err(|error| format!("List disc directory {}: {error}", entry.path))?;
        for child in children {
            index_disc_directory(filesystem, child, members, directories)?;
        }
        return Ok(());
    }
    insert(
        members,
        virtual_path(relative)?,
        Member::Disc {
            filesystem: filesystem.clone(),
            bytes: entry.size,
            entry,
        },
    )
}

fn insert(members: &mut BTreeMap<String, Member>, name: String, member: Member) -> Result<()> {
    let key = format!("/{name}");
    if let (
        Some(Member::Inno {
            bytes: a,
            checksum: ca,
            ..
        }),
        Member::Inno {
            bytes: b,
            checksum: cb,
            ..
        },
    ) = (members.get(&key), &member)
        && a == b
        && ca == cb
    {
        return Ok(());
    }
    if let (
        Some(Member::InnoGog {
            bytes: a, md5: ma, ..
        }),
        Member::InnoGog {
            bytes: b, md5: mb, ..
        },
    ) = (members.get(&key), &member)
        && a == b
        && ma == mb
    {
        return Ok(());
    }
    if members.insert(key, member).is_some() {
        return Err(format!("Case-ambiguous or duplicate source member: {name}"));
    }
    Ok(())
}

fn scan_directory(
    root: &Path,
    dir: &Path,
    members: &mut BTreeMap<String, Member>,
    artifacts: &mut Vec<Artifact>,
) -> Result<()> {
    for entry in fs::read_dir(dir).map_err(|e| e.to_string())? {
        let path = entry.map_err(|e| e.to_string())?.path();
        // Symlinked trees can cycle or escape the chosen original source.
        if path
            .symlink_metadata()
            .map_err(|e| e.to_string())?
            .file_type()
            .is_symlink()
        {
            return Err(format!("Source contains a symlink: {}", path.display()));
        }
        if path.is_dir() {
            scan_directory(root, &path, members, artifacts)?;
        } else if path.is_file() {
            let relative = path
                .strip_prefix(root)
                .map_err(|e| e.to_string())?
                .to_str()
                .ok_or("Non-UTF8 source filename")?;
            let name = virtual_path(relative)?;
            let artifact = fingerprint(&path, name.clone())?;
            insert(
                members,
                name,
                Member::Directory {
                    path,
                    bytes: artifact.bytes,
                },
            )?;
            artifacts.push(artifact);
        }
    }
    Ok(())
}

fn index_directory_without_hashes(
    root: &Path,
    dir: &Path,
    members: &mut BTreeMap<String, Member>,
) -> Result<()> {
    for entry in fs::read_dir(dir).map_err(|error| error.to_string())? {
        let path = entry.map_err(|error| error.to_string())?.path();
        let metadata = path.symlink_metadata().map_err(|error| error.to_string())?;
        if metadata.file_type().is_symlink() {
            return Err(format!("Source contains a symlink: {}", path.display()));
        }
        if metadata.is_dir() {
            index_directory_without_hashes(root, &path, members)?;
        } else if metadata.is_file() {
            let name = path
                .strip_prefix(root)
                .map_err(|error| error.to_string())?
                .to_str()
                .ok_or("Non-UTF8 source filename")?;
            insert(
                members,
                virtual_path(name)?,
                Member::Directory {
                    path,
                    bytes: metadata.len(),
                },
            )?;
        } else {
            return Err("Source contains a special file".into());
        }
    }
    Ok(())
}

#[cfg(feature = "optical-discs")]
/// Opens a disc descriptor through `opticaldiscs` and indexes its data
/// filesystem. Returns the canonical descriptor, its directory, image info and
/// the indexed filesystem.
fn open_disc_descriptor(
    path: &Path,
    expected: DiscFormat,
    kind: &str,
) -> Result<(PathBuf, PathBuf, DiscImageInfo, SourceFs)> {
    let descriptor = path.canonicalize().map_err(|e| e.to_string())?;
    let root = descriptor
        .parent()
        .ok_or_else(|| format!("{kind} descriptor has no parent directory"))?
        .canonicalize()
        .map_err(|e| e.to_string())?;
    let info = DiscImageInfo::open(&descriptor).map_err(|e| format!("Open {kind} image: {e}"))?;
    if info.format != expected {
        return Err(format!(
            "Expected {kind} descriptor, found {}",
            info.format.display_name()
        ));
    }
    let disc_filesystem = Arc::new(Mutex::new(
        open_disc_filesystem(&info).map_err(|e| format!("Open {kind} filesystem: {e}"))?,
    ));
    let root_entry = disc_filesystem
        .lock()
        .map_err(|_| "disc filesystem lock was poisoned")?
        .root()
        .map_err(|e| format!("Read {kind} root: {e}"))?;

    let mut members = BTreeMap::new();
    let mut directories = BTreeSet::new();
    index_disc_directory(&disc_filesystem, root_entry, &mut members, &mut directories)?;
    if members.is_empty() {
        return Err(format!("{kind} filesystem contains no files"));
    }
    Ok((
        descriptor,
        root,
        info,
        SourceFs {
            installer: None,
            members,
            directories,
        },
    ))
}

#[cfg(feature = "optical-discs")]
fn disc_metadata(info: &DiscImageInfo) -> DiscMetadata {
    DiscMetadata {
        format: info.format.display_name().to_owned(),
        filesystem: format!("{:?}", info.filesystem),
        volume_label: info.volume_label.clone(),
        tracks: info
            .tracks
            .iter()
            .map(|track| DiscTrackMetadata {
                number: track.number,
                format: track.cue_label().to_owned(),
                start_lba: track.start_lba,
                length_sectors: track.length_sectors,
            })
            .collect(),
    }
}

#[cfg(feature = "optical-discs")]
fn source_artifact_name(root: &Path, artifact_path: &Path, kind: &str) -> Result<String> {
    let relative = artifact_path
        .strip_prefix(root)
        .map_err(|_| format!("{kind} source member is outside its descriptor directory"))?;
    Ok(relative
        .to_str()
        .ok_or_else(|| format!("{kind} source names must be UTF-8"))?
        .replace('\\', "/"))
}

/// CUE sheet plus one or more BIN files. Each BIN is fingerprinted and every
/// track's main channel stays addressable through [`Layer::open_track`].
#[cfg(feature = "optical-discs")]
fn cue_layer(path: &Path, label: &str) -> Result<Layer> {
    let (descriptor, root, info, filesystem) =
        open_disc_descriptor(path, DiscFormat::BinCue, "CUE/BIN")?;
    let tracks = opticaldiscs::bincue::parse_cue_tracks(&descriptor)
        .map_err(|e| format!("Read CUE track table: {e}"))?;
    if tracks.is_empty() {
        return Err("CUE sheet contains no tracks".into());
    }
    let mut bins = BTreeSet::new();
    let mut track_sources = Vec::new();
    for track in &tracks {
        let source_metadata = track
            .bin_path
            .symlink_metadata()
            .map_err(|e| e.to_string())?;
        if source_metadata.file_type().is_symlink() || !source_metadata.is_file() {
            return Err("CUE BIN files must be regular files beside their sheet".into());
        }
        let bin = track.bin_path.canonicalize().map_err(|e| e.to_string())?;
        if !bin.starts_with(&root) {
            return Err("CUE BIN files must be regular files beside their sheet".into());
        }
        let physical = source_metadata.len();
        let length_sectors = if track.frame_count == 0 {
            physical
                .saturating_sub(track.file_byte_offset)
                .checked_div(track.sector_size())
                .unwrap_or(0)
        } else {
            track.frame_count
        };
        let bytes = length_sectors
            .checked_mul(track.sector_size())
            .ok_or("CUE track byte length overflow")?;
        let end = track
            .file_byte_offset
            .checked_add(bytes)
            .ok_or("CUE track file range overflow")?;
        if end > physical {
            return Err(format!("CUE track {} exceeds its BIN file", track.track_no));
        }
        track_sources.push(TrackSource {
            number: track.track_no,
            channel: TrackChannel::MainChannel,
            path: bin.clone(),
            offset: track.file_byte_offset,
            bytes,
        });
        bins.insert(bin);
    }
    let mut artifacts = Vec::new();
    for artifact_path in std::iter::once(&descriptor).chain(bins.iter()) {
        artifacts.push(fingerprint(
            artifact_path,
            source_artifact_name(&root, artifact_path, "CUE/BIN")?,
        )?);
    }
    artifacts.sort_by(|a, b| a.name.cmp(&b.name));
    Ok(Layer {
        root: filesystem.into(),
        label: label.into(),
        artifacts,
        disc: Some(disc_metadata(&info)),
        track_sources,
        warnings: Vec::new(),
        installer_rfa_source: None,
        indexed_files: None,
        original_names: BTreeMap::new(),
    })
}

#[cfg(feature = "optical-discs")]
fn clonecd_layer(path: &Path, label: &str) -> Result<Layer> {
    let (descriptor, root, info, filesystem) =
        open_disc_descriptor(path, DiscFormat::CloneCd, "CloneCD")?;

    let tracks = opticaldiscs::ccd::parse_ccd(&descriptor)
        .map_err(|e| format!("Read CloneCD track table: {e}"))?;
    let img_path = tracks
        .first()
        .ok_or("CloneCD descriptor contains no tracks")?
        .bin_path
        .canonicalize()
        .map_err(|e| e.to_string())?;
    if !img_path.starts_with(&root) || !img_path.is_file() {
        return Err("CloneCD image data must be a regular file beside its descriptor".into());
    }
    let sub_path = find_sibling(&root, &descriptor, "sub")?;
    let mut artifacts = Vec::new();
    for artifact_path in std::iter::once(&descriptor)
        .chain(std::iter::once(&img_path))
        .chain(sub_path.iter())
    {
        artifacts.push(fingerprint(
            artifact_path,
            source_artifact_name(&root, artifact_path, "CloneCD")?,
        )?);
    }
    artifacts.sort_by(|a, b| a.name.cmp(&b.name));

    let mut track_sources = Vec::new();
    for track in &tracks {
        let length_sectors = if track.frame_count == 0 {
            let physical = track.bin_path.metadata().map_err(|e| e.to_string())?.len();
            physical
                .saturating_sub(track.file_byte_offset)
                .checked_div(track.sector_size())
                .unwrap_or(0)
        } else {
            track.frame_count
        };
        let main_bytes = length_sectors
            .checked_mul(track.sector_size())
            .ok_or("CloneCD track byte length overflow")?;
        let main_end = track
            .file_byte_offset
            .checked_add(main_bytes)
            .ok_or("CloneCD track file range overflow")?;
        if main_end > img_path.metadata().map_err(|e| e.to_string())?.len() {
            return Err(format!(
                "CloneCD track {} exceeds its IMG file",
                track.track_no
            ));
        }
        track_sources.push(TrackSource {
            number: track.track_no,
            channel: TrackChannel::MainChannel,
            path: img_path.clone(),
            offset: track.file_byte_offset,
            bytes: main_bytes,
        });
        if let Some(sub_path) = &sub_path {
            let sub_bytes = length_sectors
                .checked_mul(96)
                .ok_or("CloneCD subchannel byte length overflow")?;
            let sub_offset = track
                .file_byte_offset
                .checked_div(track.sector_size())
                .and_then(|lba| lba.checked_mul(96))
                .ok_or("CloneCD subchannel offset overflow")?;
            let sub_end = sub_offset
                .checked_add(sub_bytes)
                .ok_or("CloneCD subchannel range overflow")?;
            if sub_end > sub_path.metadata().map_err(|e| e.to_string())?.len() {
                return Err(format!(
                    "CloneCD track {} exceeds its SUB file",
                    track.track_no
                ));
            }
            track_sources.push(TrackSource {
                number: track.track_no,
                channel: TrackChannel::Subchannel,
                path: sub_path.clone(),
                offset: sub_offset,
                bytes: sub_bytes,
            });
        }
    }
    Ok(Layer {
        root: filesystem.into(),
        label: label.into(),
        artifacts,
        disc: Some(disc_metadata(&info)),
        track_sources,
        warnings: Vec::new(),
        installer_rfa_source: None,
        indexed_files: None,
        original_names: BTreeMap::new(),
    })
}

#[cfg(feature = "optical-discs")]
fn find_sibling(root: &Path, descriptor: &Path, extension: &str) -> Result<Option<PathBuf>> {
    let stem = descriptor
        .file_stem()
        .and_then(|s| s.to_str())
        .ok_or("CloneCD descriptor has an invalid name")?;
    for entry in fs::read_dir(root).map_err(|e| e.to_string())? {
        let candidate = entry.map_err(|e| e.to_string())?.path();
        let same_stem = candidate
            .file_stem()
            .and_then(|s| s.to_str())
            .is_some_and(|s| s.eq_ignore_ascii_case(stem));
        let same_extension = candidate
            .extension()
            .and_then(|s| s.to_str())
            .is_some_and(|s| s.eq_ignore_ascii_case(extension));
        if same_stem && same_extension {
            let metadata = candidate.symlink_metadata().map_err(|e| e.to_string())?;
            if metadata.file_type().is_symlink() || !metadata.is_file() {
                return Err(format!(
                    "CloneCD companion must be a regular file: {}",
                    candidate.display()
                ));
            }
            return Ok(Some(candidate.canonicalize().map_err(|e| e.to_string())?));
        }
    }
    Ok(None)
}

/// One explicitly selected base, update or mod layer. Later layers override
/// earlier layers in [`Sources::new`]. Version labels participate in identity.
#[derive(Debug)]
pub struct Layer {
    root: VfsPath,
    pub label: String,
    pub artifacts: Vec<Artifact>,
    pub disc: Option<DiscMetadata>,
    /// Partial source-coverage findings that did not prevent mounting.
    pub warnings: Vec<String>,
    track_sources: Vec<TrackSource>,
    installer_rfa_source: Option<InstallerRfaSource>,
    indexed_files: Option<Arc<Vec<String>>>,
    /// Original spelling of member paths by lower-case virtual path, where
    /// the backend records it (Inno installers). Lookups stay
    /// case-insensitive; this is for games that show or store file names.
    original_names: BTreeMap<String, String>,
}

mod session;
pub use session::SessionFiles;

const MAX_GOG_HASHDB: u64 = 16 * 1024 * 1024;

#[derive(Debug)]
struct GogHashEntry {
    path: String,
    md5: String,
}

fn parse_gog_hashdb(bytes: &[u8]) -> Result<Vec<GogHashEntry>> {
    if bytes.len() < 12 {
        return Err("GOG hash database is shorter than its header".into());
    }
    let header = |offset: usize| {
        u32::from_le_bytes(bytes[offset..offset + 4].try_into().expect("fixed header"))
    };
    let version = header(0);
    let record_type = header(4);
    let count = header(8) as usize;
    if version != 12 || record_type != 1 {
        return Err(format!(
            "Unsupported GOG hash database header ({version}, {record_type})"
        ));
    }
    let expected = 12usize
        .checked_add(
            count
                .checked_mul(1056)
                .ok_or("GOG hash database size overflow")?,
        )
        .ok_or("GOG hash database size overflow")?;
    if bytes.len() != expected {
        return Err(format!(
            "GOG hash database length mismatch: expected {expected}, got {}",
            bytes.len()
        ));
    }
    let mut entries = Vec::with_capacity(count);
    for row in bytes[12..].as_chunks::<1056>().0 {
        let path_end = row[..1024]
            .iter()
            .position(|byte| *byte == 0)
            .unwrap_or(1024);
        if path_end == 0 {
            return Err("GOG hash database contains an empty path".into());
        }
        let path = std::str::from_utf8(&row[..path_end])
            .map_err(|_| "GOG hash database path is not UTF-8")?
            .to_owned();
        let digest = std::str::from_utf8(&row[1024..])
            .map_err(|_| "GOG hash database digest is not ASCII")?;
        if digest.len() != 32 || !digest.bytes().all(|byte| byte.is_ascii_hexdigit()) {
            return Err(format!("Invalid GOG MD5 for {path:?}"));
        }
        entries.push(GogHashEntry {
            path,
            md5: digest.to_ascii_lowercase(),
        });
    }
    Ok(entries)
}

fn hashdb_from_zip(bytes: &[u8]) -> Result<Option<Vec<GogHashEntry>>> {
    let mut archive = ZipArchive::new(Cursor::new(bytes))
        .map_err(|error| format!("Open nested GOG ZIP: {error}"))?;
    for index in 0..archive.len() {
        let mut file = archive
            .by_index(index)
            .map_err(|error| format!("Read nested GOG ZIP entry: {error}"))?;
        let name = file
            .name()
            .map_err(|error| format!("Read nested GOG ZIP name: {error}"))?;
        if !name.to_ascii_lowercase().ends_with(".hashdb") {
            continue;
        }
        if file.size() > MAX_GOG_HASHDB {
            return Err("GOG hash database exceeds 16 MiB".into());
        }
        let mut data = Vec::with_capacity(file.size() as usize);
        file.by_ref()
            .take(MAX_GOG_HASHDB + 1)
            .read_to_end(&mut data)
            .map_err(|error| format!("Read GOG hash database: {error}"))?;
        if data.len() as u64 > MAX_GOG_HASHDB {
            return Err("GOG hash database exceeds 16 MiB".into());
        }
        return parse_gog_hashdb(&data).map(Some);
    }
    Ok(None)
}

fn inspect_gog_payload(outer: &[u8]) -> Result<(String, u64, Option<Vec<GogHashEntry>>)> {
    let mut decoder = ZlibDecoder::new(Cursor::new(outer));
    let mut prefix = [0; 2];
    decoder
        .read_exact(&mut prefix)
        .map_err(|error| format!("Decompress GOG payload: {error}"))?;
    let is_zip = prefix == *b"PK";
    let mut hash = Md5::new();
    hash.update(prefix);
    let mut archive = is_zip.then(|| prefix.to_vec());
    let mut total = 2u64;
    let mut buffer = [0u8; 65536];
    loop {
        let count = decoder
            .read(&mut buffer)
            .map_err(|error| format!("Decompress GOG payload: {error}"))?;
        if count == 0 {
            break;
        }
        total = total
            .checked_add(count as u64)
            .ok_or("GOG payload size overflow")?;
        if total > MAX_MEMBER {
            return Err("GOG payload exceeds 256 MiB limit".into());
        }
        hash.update(&buffer[..count]);
        if archive.is_some() {
            if total <= MAX_GOG_HASHDB {
                archive
                    .as_mut()
                    .expect("archive presence checked")
                    .extend_from_slice(&buffer[..count]);
            } else {
                archive = None;
            }
        }
    }
    let digest = hex(&hash.finalize());
    let hashdb = match archive {
        Some(bytes) => hashdb_from_zip(&bytes)?,
        None => None,
    };
    Ok((digest, total, hashdb))
}

fn companion_paths<R: std::io::Read + std::io::Seek>(
    path: &Path,
    inno: &Inno<R>,
) -> Result<Vec<PathBuf>> {
    let last = inno
        .file_locations()
        .iter()
        .map(|l| l.chunk().last_slice())
        .max()
        .ok_or("Installer has no data locations")?;
    if last >= 10000 {
        return Err("Installer slice count exceeds 10000".into());
    }
    let per_disk = inno.header().slices_per_disk();
    let stem = path
        .file_stem()
        .ok_or("Missing installer stem")?
        .to_string_lossy();
    let alternate = inno
        .header()
        .base_filename()
        .map(|s| s.replace(['/', '\\'], "_"));
    let parent = path.parent().unwrap_or(Path::new("."));
    let mut slices = Vec::new();
    for i in 0..=last {
        let suffix = if per_disk == 1 {
            format!("-{}.bin", i + 1)
        } else {
            format!(
                "-{}{}.bin",
                i / per_disk + 1,
                (b'a' + (i % per_disk) as u8) as char
            )
        };
        let preferred = parent.join(format!("{stem}{suffix}"));
        let selected = if preferred.is_file() {
            preferred
        } else if let Some(name) = &alternate {
            parent.join(format!("{name}{suffix}"))
        } else {
            preferred
        };
        if !selected.is_file() {
            return Err(format!("Missing installer slice: {}", selected.display()));
        }
        slices.push(selected);
    }
    Ok(slices)
}

impl Layer {
    /// Build selected installer ZIP caches in one forward pass through solid
    /// chunks. Directory/disc layers have nothing to prefetch.
    pub fn prefetch_installer_zips(&self, paths: &[String]) -> Result<()> {
        let Some(source) = &self.installer_rfa_source else {
            return Ok(());
        };
        let mut missing = BTreeMap::new();
        for path in paths {
            let name = virtual_path(path)?;
            if !name.ends_with(".zip") {
                continue;
            }
            let Some(member) = source.members.get(&name) else {
                continue;
            };
            let (cached, manifest) =
                inno_zip_cache_paths(&source.path, member.index, member.bytes, &member.checksum)?;
            if !cached_rfa_is_valid(&cached, &manifest, member.bytes) {
                missing.insert(member.index, (member.bytes, cached, manifest));
            }
        }
        if missing.is_empty() {
            return Ok(());
        }
        let mut inno = Inno::open(&source.path).map_err(|error| error.to_string())?;
        let mut files = inno.streaming_files(|entry| missing.contains_key(&entry.index()));
        let mut completed = 0;
        while let Some(file) = files.next() {
            let (entry, mut reader) = file.map_err(|error| error.to_string())?;
            let (bytes, cached, manifest) = missing
                .get(&entry.index())
                .ok_or("Unexpected installer cache entry")?;
            save_inno_zip_reader(&mut reader, entry.index(), *bytes, cached, manifest)?;
            completed += 1;
        }
        if completed != missing.len() {
            return Err(format!(
                "Installer cache selected {} ZIPs but read {completed}",
                missing.len()
            ));
        }
        Ok(())
    }

    /// Index a nested ZIP without reading its complete payload into memory.
    /// Source identities are inherited; this layer's label identifies the
    /// selected archive and participates in the ordered source identity.
    pub fn zip_archive(&self, path: &str, label: &str) -> Result<Self> {
        self.zip_archive_mode(path, label, false, &mut |_, _, _| {})
    }
    pub fn zip_archive_disk_backed(&self, path: &str, label: &str) -> Result<Self> {
        self.zip_archive_mode(path, label, true, &mut |_, _, _| {})
    }
    /// Observe file names, uncompressed sizes and CRC32s in ZIP directory order
    /// while mounting. Legacy game protocols derive pack checksums from these fields.
    pub fn zip_archive_with_entries(
        &self,
        path: &str,
        label: &str,
        mut visit: impl FnMut(&str, u64, u32),
    ) -> Result<Self> {
        self.zip_archive_mode(path, label, false, &mut visit)
    }

    fn zip_archive_mode(
        &self,
        path: &str,
        label: &str,
        disk_backed: bool,
        visit: &mut dyn FnMut(&str, u64, u32),
    ) -> Result<Self> {
        let stream = self
            .root
            .join(virtual_path(path)?)
            .and_then(|path| path.open_file())
            .map_err(|error| error.to_string())?;
        let mut layer = Self::zip_stream_mode(
            stream,
            &format!("{label}:zip:{}", virtual_path(path)?),
            disk_backed,
            visit,
        )?;
        layer.label = format!("{}|{}", self.label, layer.label);
        layer.artifacts = self.artifacts.clone();
        layer.warnings = self.warnings.clone();
        Ok(layer)
    }

    /// Mount an already identified source stream as ZIP members. Callers which
    /// have a source layer should use `zip_archive` to retain its fingerprints.
    pub fn zip_stream(stream: FileStream, label: &str) -> Result<Self> {
        Self::zip_stream_mode(stream, label, false, &mut |_, _, _| {})
    }
    /// Seekable large-resource streams with bounded RAM. ZIP resources are
    /// verified into anonymous disposable files before exposing them. No PCM
    /// or asset decoder runs here; originals remain untouched.
    pub fn zip_stream_disk_backed(stream: FileStream, label: &str) -> Result<Self> {
        Self::zip_stream_mode(stream, label, true, &mut |_, _, _| {})
    }
    fn zip_stream_mode(
        stream: FileStream,
        label: &str,
        disk_backed: bool,
        visit: &mut dyn FnMut(&str, u64, u32),
    ) -> Result<Self> {
        let archive = zip::Archive::open(stream, visit)?;
        let entries = archive.entries().to_vec();
        let archive = Arc::new(Mutex::new(archive));
        let mut members = BTreeMap::new();
        for entry in entries {
            insert(
                &mut members,
                virtual_path(&entry.name)?,
                Member::Zip {
                    archive: archive.clone(),
                    disk_backed,
                    path: entry.name,
                    bytes: entry.bytes,
                },
            )?;
        }
        Self::from_members(members, label.to_owned(), Vec::new(), Vec::new())
    }

    /// Select and rename explicit virtual members. Installer helper files are
    /// excluded by the caller's mapping; no backing file or cache is exposed.
    pub fn select(&self, mappings: &[(&str, &str)], label: &str) -> Result<Self> {
        let mut members = BTreeMap::new();
        let available: BTreeSet<_> = self.files()?.into_iter().collect();
        let mut identity = label.to_string();
        for (source, destination) in mappings {
            let source = virtual_path(source)?;
            let destination = virtual_path(destination)?;
            let path = self.root.join(&source).map_err(|error| error.to_string())?;
            if !available.contains(&source) {
                return Err(format!("Selected member is not a file: {source}"));
            }
            identity.push_str(&format!("|{source}>{destination}"));
            insert(
                &mut members,
                destination,
                Member::Virtual { path, bytes: 0 },
            )?;
        }
        self.derived(members, format!("{}|{identity}", self.label))
    }

    fn derived(&self, members: BTreeMap<String, Member>, label: String) -> Result<Self> {
        Self::from_members(
            members,
            label,
            self.artifacts.clone(),
            self.warnings.clone(),
        )
    }

    fn from_members(
        members: BTreeMap<String, Member>,
        label: String,
        artifacts: Vec<Artifact>,
        warnings: Vec<String>,
    ) -> Result<Self> {
        let mut directories = BTreeSet::from([String::new()]);
        for path in members.keys() {
            let mut parent = path.as_str();
            while let Some((p, _)) = parent.rsplit_once('/') {
                directories.insert(p.to_string());
                parent = p;
            }
        }
        if directories.iter().any(|p| members.contains_key(p)) {
            return Err("Source path is both file and directory".into());
        }
        Ok(Self {
            indexed_files: Some(Arc::new(
                members
                    .keys()
                    .map(|s| s.trim_start_matches('/').to_owned())
                    .collect(),
            )),
            original_names: BTreeMap::new(),
            root: SourceFs {
                installer: None,
                members,
                directories,
            }
            .into(),
            label,
            artifacts,
            disc: None,
            track_sources: Vec::new(),
            warnings,
            installer_rfa_source: None,
        })
    }

    /// Open a Battlefield/Refractor module directory. Loose files are indexed
    /// together with the contents of sorted `Archives/**/*.rfa` files. Later
    /// archive filenames override earlier ones; loose files override archive
    /// members. The archive order matches common `_001` patch naming but still
    /// needs validation against original runtime search behavior.
    pub fn refractor_mod(path: &Path, label: &str) -> Result<Self> {
        let root = path.canonicalize().map_err(|e| e.to_string())?;
        if !root.is_dir() {
            return Err("Refractor module source must be a directory".into());
        }
        let mut loose_members = BTreeMap::new();
        let mut artifacts = Vec::new();
        scan_directory(&root, &root, &mut loose_members, &mut artifacts)?;
        artifacts.sort_by(|a, b| {
            a.name
                .to_ascii_lowercase()
                .cmp(&b.name.to_ascii_lowercase())
        });
        let archives = artifacts
            .iter()
            .filter(|artifact| {
                let normalized = artifact.name.replace('\\', "/");
                normalized
                    .split('/')
                    .next()
                    .is_some_and(|first| first.eq_ignore_ascii_case("Archives"))
                    && Path::new(&normalized)
                        .extension()
                        .is_some_and(|extension| extension.eq_ignore_ascii_case("rfa"))
            })
            .cloned()
            .collect::<Vec<_>>();

        let mut archive_members = BTreeMap::new();
        let mut warnings = Vec::new();
        for artifact in &archives {
            // `Artifact::name` is a normalized VFS path. Recover the original
            // physical path from the directory member so mixed-case archive
            // names still open on case-sensitive filesystems.
            let archive_key = format!("/{}", virtual_path(&artifact.name)?);
            let archive_path = match loose_members.get(&archive_key) {
                Some(Member::Directory { path, .. }) => path.clone(),
                _ => {
                    return Err(format!(
                        "Missing physical source path for archive {}",
                        artifact.name
                    ));
                }
            };
            let archive = Arc::new(crate::rfa::Archive::open(&archive_path)?);
            let archive_label = artifact.name.clone();
            for entry in archive.entries() {
                let name = virtual_path(&entry.path)?;
                let key = format!("/{name}");
                if archive_members
                    .insert(
                        key,
                        Member::Rfa {
                            archive: archive.clone(),
                            entry: entry.clone(),
                        },
                    )
                    .is_some()
                {
                    warnings.push(format!(
                        "{archive_label} overrides an earlier RFA member at {name}"
                    ));
                }
            }
            // Archive containers are source artifacts, not game-visible files.
            loose_members.remove(&archive_key);
        }
        // Explicit loose files in a module take precedence over its archives.
        for (name, member) in loose_members {
            if archive_members.insert(name.clone(), member).is_some() {
                warnings.push(format!(
                    "A loose module file overrides archived member {name}"
                ));
            }
        }
        if archive_members.is_empty() {
            return Err("Refractor module contains no loose files or RFA members".into());
        }
        let mut directories = BTreeSet::from([String::new()]);
        for path in archive_members.keys() {
            let mut parent = path.as_str();
            while let Some((directory, _)) = parent.rsplit_once('/') {
                directories.insert(directory.to_owned());
                parent = directory;
            }
        }
        if directories
            .iter()
            .any(|directory| archive_members.contains_key(directory))
        {
            return Err("Refractor module path is both file and directory".into());
        }
        Ok(Self {
            root: SourceFs {
                installer: None,
                members: archive_members,
                directories,
            }
            .into(),
            label: label.into(),
            artifacts,
            disc: None,
            warnings,
            track_sources: Vec::new(),
            installer_rfa_source: None,
            indexed_files: None,
            original_names: BTreeMap::new(),
        })
    }

    /// Build a Refractor module layer from any already-mounted source layer,
    /// such as an Inno installer, disc filesystem, or directory source.
    /// `module_path` is virtual and relative to the source root.
    pub fn refractor_mod_from(&self, module_path: &str, label: &str) -> Result<Self> {
        let cache = open_games_cache::Cache::new("battlefield-1942", None)?.lazy_session()?;
        self.refractor_mod_from_cached(module_path, label, &cache)
    }

    pub fn refractor_mod_from_cached(
        &self,
        module_path: &str,
        label: &str,
        cache: &open_games_cache::Session,
    ) -> Result<Self> {
        let module_path = virtual_path(module_path)?;
        let module_root = self
            .root
            .join(&module_path)
            .map_err(|error| format!("Open module source {module_path}: {error}"))?;
        if !module_root.is_dir().map_err(|error| error.to_string())? {
            return Err(format!(
                "Refractor module path is not a directory: {module_path}"
            ));
        }

        fn walk(
            module_root: &VfsPath,
            directory: &VfsPath,
            members: &mut BTreeMap<String, Member>,
            archives: &mut Vec<(String, VfsPath)>,
        ) -> Result<()> {
            for child in directory
                .read_dir()
                .map_err(|error| format!("List {}: {error}", directory.as_str()))?
            {
                if child.is_dir().map_err(|error| error.to_string())? {
                    walk(module_root, &child, members, archives)?;
                    continue;
                }
                let relative = child
                    .as_str()
                    .strip_prefix(module_root.as_str())
                    .and_then(|path| path.strip_prefix('/'))
                    .ok_or("VFS module member is outside its module directory")?;
                let name = virtual_path(relative)?;
                let metadata = child.metadata().map_err(|error| error.to_string())?;
                let key = format!("/{name}");
                if name
                    .split('/')
                    .next()
                    .is_some_and(|first| first.eq_ignore_ascii_case("archives"))
                    && Path::new(&name)
                        .extension()
                        .is_some_and(|extension| extension.eq_ignore_ascii_case("rfa"))
                {
                    archives.push((name, child));
                } else if members
                    .insert(
                        key,
                        Member::VfsFile {
                            path: child,
                            bytes: metadata.len,
                        },
                    )
                    .is_some()
                {
                    return Err(format!("Duplicate virtual module file: {name}"));
                }
            }
            Ok(())
        }

        let mut loose_members = BTreeMap::new();
        let mut archive_files = Vec::new();
        walk(
            &module_root,
            &module_root,
            &mut loose_members,
            &mut archive_files,
        )?;
        archive_files.sort_by(|left, right| {
            left.0
                .to_ascii_lowercase()
                .cmp(&right.0.to_ascii_lowercase())
        });

        let cached_archives = self
            .installer_rfa_source
            .as_ref()
            .map(|source| cache_inno_rfas(source, &module_path, &archive_files, cache))
            .transpose()?
            .unwrap_or_default();

        let mut members = BTreeMap::new();
        let mut warnings = Vec::new();
        for (name, path) in archive_files {
            let archive = Arc::new(if let Some(cached) = cached_archives.get(&name) {
                crate::rfa::Archive::open(cached)?
            } else {
                crate::rfa::Archive::open_vfs(path)?
            });
            for entry in archive.entries() {
                let virtual_name = virtual_path(&entry.path)?;
                if members
                    .insert(
                        format!("/{virtual_name}"),
                        Member::Rfa {
                            archive: archive.clone(),
                            entry: entry.clone(),
                        },
                    )
                    .is_some()
                {
                    warnings.push(format!(
                        "{} overrides an earlier RFA member at {}",
                        name, virtual_name
                    ));
                }
            }
        }
        for (name, member) in loose_members {
            if members.insert(name.clone(), member).is_some() {
                warnings.push(format!(
                    "A loose module file overrides archived member {name}"
                ));
            }
        }
        if members.is_empty() {
            return Err("Refractor module contains no loose files or RFA members".into());
        }
        let mut directories = BTreeSet::from([String::new()]);
        for path in members.keys() {
            let mut parent = path.as_str();
            while let Some((directory, _)) = parent.rsplit_once('/') {
                directories.insert(directory.to_owned());
                parent = directory;
            }
        }
        if directories
            .iter()
            .any(|directory| members.contains_key(directory))
        {
            return Err("Refractor module path is both file and directory".into());
        }
        Ok(Self {
            root: SourceFs {
                installer: None,
                members,
                directories,
            }
            .into(),
            label: label.into(),
            artifacts: self.artifacts.clone(),
            disc: self.disc.clone(),
            warnings,
            track_sources: self.track_sources.clone(),
            installer_rfa_source: None,
            indexed_files: None,
            original_names: BTreeMap::new(),
        })
    }

    /// Open a file inside this layer by virtual path.
    pub fn open_file(&self, path: &str) -> Result<FileStream> {
        self.root
            .join(&virtual_path(path)?)
            .and_then(|path| path.open_file())
            .map_err(|error| error.to_string())
    }

    /// All virtual names without decoding payloads whose size is not indexed.
    pub fn files(&self) -> Result<Vec<String>> {
        if let Some(names) = &self.indexed_files {
            return Ok(names.as_ref().clone());
        }
        crate::AssetVfs::from_roots(std::slice::from_ref(&self.root)).files()
    }

    /// The original spelling of `path` (a virtual path in any case), when
    /// this layer's backend recorded it.
    pub fn original_name(&self, path: &str) -> Option<&str> {
        self.original_names
            .get(&virtual_path(path).ok()?)
            .map(String::as_str)
    }

    /// List direct children of a virtual directory in this source layer.
    pub fn list_dir(&self, path: &str) -> Result<Vec<String>> {
        self.root
            .join(&virtual_path(path)?)
            .and_then(|path| path.read_dir())
            .map(|children| {
                children
                    .map(|child| {
                        child
                            .as_str()
                            .rsplit('/')
                            .next()
                            .unwrap_or_default()
                            .to_owned()
                    })
                    .collect()
            })
            .map_err(|error| error.to_string())
    }

    fn nsis(path: &Path, label: &str, archive: nsis::Archive) -> Result<Self> {
        let artifacts = vec![fingerprint(
            path,
            path.file_name()
                .ok_or("Missing NSIS filename")?
                .to_string_lossy()
                .into_owned(),
        )?];
        let identity = artifact_identity(&artifacts, "nsis2-nonsolid-deflate-v1");
        let archive = Arc::new(archive);
        let mut members = BTreeMap::new();
        let mut directories = BTreeSet::from([String::new()]);
        for entry in &archive.entries {
            let slash_path = format!("/{}", entry.path);
            let mut parent = slash_path.as_str();
            while let Some((p, _)) = parent.rsplit_once('/') {
                directories.insert(p.into());
                parent = p;
            }
            insert(
                &mut members,
                entry.path.clone(),
                Member::Nsis {
                    archive: archive.clone(),
                    entry: entry.clone(),
                    identity: identity.clone(),
                },
            )?;
        }
        Ok(Self { indexed_files: Some(Arc::new(archive.entries.iter().map(|e| e.path.clone()).collect())), root: SourceFs { installer: None, members, directories }.into(), label: label.into(), artifacts, disc: None, warnings: vec!["NSIS static destinations only; script branches, plugins, registry and mod engine logic are not executed. Whole source SHA-256 identifies payloads; cache SHA-256 detects cache corruption.".into()], track_sources: Vec::new(), installer_rfa_source: None, original_names: BTreeMap::new() })
    }

    fn installshield(path: &Path, label: &str, known: Option<Vec<Artifact>>) -> Result<Self> {
        let archive = Arc::new(installshield::Archive::open(path)?);
        let mut artifacts = if let Some(known) = known.filter(|a| !a.is_empty()) {
            known
        } else {
            archive
                .sources
                .iter()
                .map(|p| fingerprint(p, p.file_name().unwrap().to_string_lossy().into_owned()))
                .collect::<Result<Vec<_>>>()?
        };
        artifacts.sort_by(|a, b| a.name.cmp(&b.name));
        let identity = artifact_identity(&artifacts, "installshield-ansi10-v1");
        let mut members = BTreeMap::new();
        let mut directories = BTreeSet::from([String::new()]);
        for entry in &archive.entries {
            let slash_path = format!("/{}", entry.path);
            let mut parent = slash_path.as_str();
            while let Some((p, _)) = parent.rsplit_once('/') {
                directories.insert(p.to_owned());
                parent = p;
            }
            insert(
                &mut members,
                entry.path.clone(),
                Member::InstallShield {
                    archive: archive.clone(),
                    entry: entry.clone(),
                    identity: identity.clone(),
                },
            )?;
        }
        Ok(Self { indexed_files: Some(Arc::new(archive.entries.iter().map(|e| e.path.clone()).collect())), root: SourceFs { installer: None, members, directories }.into(), label: label.into(), artifacts, disc: None, warnings: vec!["InstallShield file groups are exposed under groups/; installation destinations require an explicit game adapter. ANSI 10.5, nonsplit stored/deflate members only.".into()], track_sources: Vec::new(), installer_rfa_source: None, original_names: BTreeMap::new() })
    }

    pub fn open(path: &Path, label: &str) -> Result<Self> {
        Self::open_with_known_artifacts(path, label, None)
    }

    /// Index a directory or installer using previously recorded source hashes.
    /// This avoids rereading large source sets during ordinary game startup.
    /// Recorded hashes are identities, not a claim that every source byte was
    /// rehashed at this mount. The VFS verifies an Inno file's own checksum
    /// when opening that member.
    pub fn open_with_known_artifacts(
        path: &Path,
        label: &str,
        known_artifacts: Option<Vec<Artifact>>,
    ) -> Result<Self> {
        if path
            .extension()
            .and_then(|e| e.to_str())
            .is_some_and(|e| e.eq_ignore_ascii_case("cab") || e.eq_ignore_ascii_case("hdr"))
        {
            return Self::installshield(path, label, known_artifacts);
        }
        if path
            .extension()
            .and_then(|extension| extension.to_str())
            .is_some_and(|extension| extension.eq_ignore_ascii_case("zip"))
        {
            let file = File::open(path).map_err(|error| error.to_string())?;
            let mut layer = Self::zip_stream(Box::new(file), label)?;
            layer.artifacts.push(fingerprint(
                path,
                path.file_name()
                    .ok_or("Missing ZIP filename")?
                    .to_string_lossy()
                    .into(),
            )?);
            return Ok(layer);
        }
        if path
            .extension()
            .and_then(|extension| extension.to_str())
            .is_some_and(|extension| extension.eq_ignore_ascii_case("ccd"))
        {
            #[cfg(feature = "optical-discs")]
            return clonecd_layer(path, label);
            #[cfg(not(feature = "optical-discs"))]
            return Err("CloneCD images need the `optical-discs` feature".into());
        }
        if path
            .extension()
            .and_then(|extension| extension.to_str())
            .is_some_and(|extension| extension.eq_ignore_ascii_case("cue"))
        {
            #[cfg(feature = "optical-discs")]
            return cue_layer(path, label);
            #[cfg(not(feature = "optical-discs"))]
            return Err("CUE/BIN images need the `optical-discs` feature".into());
        }
        if path.is_file()
            && path
                .extension()
                .and_then(|s| s.to_str())
                .is_some_and(|s| s.eq_ignore_ascii_case("iso"))
        {
            return Ok(Self {
                root: crate::assets::media::IsoMedia::new(path)?.into(),
                label: label.into(),
                artifacts: vec![fingerprint(
                    path,
                    path.file_name()
                        .ok_or("Missing ISO filename")?
                        .to_string_lossy()
                        .into(),
                )?],
                disc: Some(DiscMetadata {
                    format: "ISO9660/2048".into(),
                    filesystem: "ISO9660".into(),
                    volume_label: None,
                    tracks: Vec::new(),
                }),
                warnings: Vec::new(),
                track_sources: Vec::new(),
                installer_rfa_source: None,
                indexed_files: None,
                original_names: BTreeMap::new(),
            });
        }
        if path.is_file() && setup::records(path)?.is_some() {
            return Self::installshield(path, label, known_artifacts);
        }
        if path.is_file()
            && let Some(archive) = nsis::Archive::probe(path)?
        {
            return Self::nsis(path, label, archive);
        }
        let mut members = BTreeMap::new();
        let mut artifacts = Vec::new();
        let mut warnings = Vec::new();
        let mut installer_rfa_members = BTreeMap::new();
        let mut original_names = BTreeMap::new();
        let installer = if path.is_dir() {
            let root = path.canonicalize().map_err(|e| e.to_string())?;
            if known_artifacts.is_some() {
                index_directory_without_hashes(&root, &root, &mut members)?;
            } else {
                scan_directory(&root, &root, &mut members, &mut artifacts)?;
            }
            None
        } else {
            if let Ok(mut inno) = Inno::open(path) {
                let indexed = innoidx::index(&inno)?;
                for member in indexed.members {
                    let virtual_name = virtual_path(&member.path)?;
                    original_names.insert(virtual_name.clone(), member.path.clone());
                    let member = match member.payload {
                        innoidx::Payload::Plain {
                            index,
                            bytes,
                            checksum,
                        } => {
                            installer_rfa_members.insert(
                                virtual_name.clone(),
                                InstallerRfaMember {
                                    index,
                                    bytes,
                                    checksum: checksum.clone(),
                                },
                            );
                            Member::Inno {
                                index,
                                bytes,
                                checksum,
                            }
                        }
                        // The installer's own part list: no payload is
                        // decompressed to index it, and multi-part files are
                        // reassembled on open.
                        innoidx::Payload::Gog { parts, bytes, md5 } => {
                            Member::InnoGog { parts, bytes, md5 }
                        }
                    };
                    insert(&mut members, virtual_name, member)?;
                }
                if indexed.layout == innoidx::Layout::Legacy {
                    warnings.push(
                        "Legacy GOG installer: game files are ordinary Inno members under {app}; GOG metadata files are omitted".into(),
                    );
                }
                if !indexed.unmapped_payloads.is_empty() {
                    let gog_temp_members = indexed.unmapped_payloads;
                    let mut hashdb = None;
                    let mut payloads: BTreeMap<String, GogPart> = BTreeMap::new();
                    for entry in gog_temp_members {
                        let (index, size) = (entry.index, entry.packed);
                        let outer = innoidx::member_bytes(&mut inno, index, size)?;
                        let (md5, inner_size, nested_hashdb) = inspect_gog_payload(&outer)
                            .map_err(|error| format!("Inspect GOG payload {index}: {error}"))?;
                        payloads.entry(md5.clone()).or_insert(GogPart {
                            index,
                            packed: size,
                            bytes: inner_size,
                            md5,
                        });
                        if let Some(entries) = nested_hashdb
                            && hashdb.replace(entries).is_some()
                        {
                            return Err("GOG installer contains multiple hash databases".into());
                        }
                    }
                    let hashdb = hashdb.ok_or(
                        "GOG installer metadata is present, but no supported .hashdb was found",
                    )?;
                    let mut mapped = 0usize;
                    for entry in hashdb {
                        let virtual_name = virtual_path(&entry.path)?;
                        original_names.insert(virtual_name.clone(), entry.path.replace('\\', "/"));
                        let Some(part) = payloads.get(&entry.md5).cloned() else {
                            warnings.push(format!(
                                "GOG hash database lists {:?} (MD5 {}), but no installer payload matches; the file is omitted",
                                entry.path, entry.md5
                            ));
                            continue;
                        };
                        insert(
                            &mut members,
                            virtual_name,
                            Member::InnoGog {
                                bytes: part.bytes,
                                parts: Arc::new([part]),
                                md5: entry.md5,
                            },
                        )?;
                        mapped += 1;
                    }
                    if mapped == 0 {
                        return Err("GOG hash database did not match any installer payload".into());
                    }
                }
                if known_artifacts.is_none() {
                    artifacts.push(fingerprint(
                        path,
                        path.file_name()
                            .ok_or("Missing installer filename")?
                            .to_string_lossy()
                            .into(),
                    )?);
                }
                // Match Inno's exact slice resolution, including renamed EXEs,
                // header-name fallbacks and multi-letter split-disc layouts.
                if inno.setup_loader().data_offset() == 0 {
                    for slice in companion_paths(path, &inno)? {
                        if known_artifacts.is_none() {
                            artifacts.push(fingerprint(
                                &slice,
                                slice
                                    .file_name()
                                    .ok_or("Missing slice filename")?
                                    .to_string_lossy()
                                    .into(),
                            )?);
                        }
                    }
                }
                Some(path.to_path_buf())
            } else if let Some(archive) = rar::Archive::probe(path)? {
                let archive = Arc::new(archive);
                for entry in archive.entries() {
                    insert(
                        &mut members,
                        entry.path.clone(),
                        Member::Rar {
                            archive: archive.clone(),
                            index: entry.index,
                            bytes: entry.bytes,
                        },
                    )?;
                }
                artifacts.push(fingerprint(
                    path,
                    path.file_name()
                        .ok_or("Missing RAR filename")?
                        .to_string_lossy()
                        .into(),
                )?);
                None
            } else {
                let indexed = pe::index(path)?;
                for resource in indexed {
                    insert(
                        &mut members,
                        virtual_path(&format!("resources/{}", resource.path))?,
                        Member::PeResource {
                            source: path.to_path_buf(),
                            offset: resource.offset,
                            bytes: resource.bytes,
                        },
                    )?;
                }
                artifacts.push(fingerprint(
                    path,
                    path.file_name()
                        .ok_or("Missing executable filename")?
                        .to_string_lossy()
                        .into(),
                )?);
                None
            }
        };
        if members.is_empty() {
            return Err("Source contains no game files".into());
        }
        let mut directories = BTreeSet::from([String::new()]);
        for path in members.keys() {
            let mut parent = path.as_str();
            while let Some((p, _)) = parent.rsplit_once('/') {
                directories.insert(p.to_string());
                parent = p;
            }
        }
        if directories.iter().any(|p| members.contains_key(p)) {
            return Err("Source path is both file and directory".into());
        }
        if let Some(known) = known_artifacts {
            artifacts = known;
        }
        artifacts.sort_by(|a, b| a.name.cmp(&b.name));
        let installer_rfa_source = installer
            .as_ref()
            .filter(|_| !installer_rfa_members.is_empty())
            .map(|path| InstallerRfaSource {
                path: path.clone(),
                identity: artifact_identity(&artifacts, "battlefield-inno-rfa-cache-v1"),
                members: installer_rfa_members,
            });
        Ok(Self {
            root: SourceFs {
                installer,
                members,
                directories,
            }
            .into(),
            label: label.into(),
            artifacts,
            disc: None,
            warnings,
            track_sources: Vec::new(),
            installer_rfa_source,
            indexed_files: None,
            original_names,
        })
    }

    /// Open one stored track channel through this source layer. CloneCD track
    /// main-channel and subchannel bytes are bounded, seekable streams.
    pub fn open_track(&self, number: u32, channel: TrackChannel) -> Result<FileStream> {
        let source = self
            .track_sources
            .iter()
            .find(|source| source.number == number && source.channel == channel)
            .ok_or_else(|| {
                format!(
                    "track {number} {channel:?} stream is unavailable in layer {}",
                    self.label
                )
            })?;
        let metadata = source.path.symlink_metadata().map_err(|e| e.to_string())?;
        if metadata.file_type().is_symlink() || !metadata.is_file() {
            return Err("Disc track source changed type after mounting".into());
        }
        let end = source
            .offset
            .checked_add(source.bytes)
            .ok_or("Track stream range overflow")?;
        let file = File::open(&source.path).map_err(|e| e.to_string())?;
        if end > file.metadata().map_err(|e| e.to_string())?.len() {
            return Err("Disc track source became shorter after mounting".into());
        }
        Ok(Box::new(FileSlice {
            file,
            offset: source.offset,
            length: source.bytes,
            cursor: 0,
        }))
    }
}

/// The game sees only virtual files and streams. Backend choice and layer
/// precedence are kept out of all game-specific asset decoders.
#[derive(Debug)]
pub struct Sources {
    pub(crate) assets: crate::AssetVfs,
    pub layers: Vec<Layer>,
    pub identity: String,
    precedence: Vec<usize>,
}
impl Sources {
    /// Base media is a source set, not a patch stack: earlier discs win
    /// duplicate virtual files; updates and mods then override in listed order.
    pub fn profile(
        base: &[PathBuf],
        updates: &[(String, PathBuf)],
        mods: &[(String, PathBuf)],
        decoder: &str,
    ) -> Result<Self> {
        if base.is_empty() {
            return Err("a base source is required".into());
        }
        let mut layers = Vec::new();
        for (index, path) in base.iter().enumerate() {
            layers.push(Layer::open(path, &format!("base-disc-{}", index + 1))?);
        }
        for (label, path) in updates {
            layers.push(Layer::open(path, &format!("update:{label}"))?);
        }
        for (label, path) in mods {
            layers.push(Layer::open(path, &format!("mod:{label}"))?);
        }
        let mut sources = Self::new(layers, decoder)?;
        sources.precedence = (base.len()..sources.layers.len())
            .rev()
            .chain(0..base.len())
            .collect();
        let roots = sources
            .precedence
            .iter()
            .map(|&index| sources.layers[index].root.clone())
            .collect::<Vec<_>>();
        sources.assets = crate::AssetVfs::from_roots(&roots);
        Ok(sources)
    }

    pub fn new(layers: Vec<Layer>, decoder_version: &str) -> Result<Self> {
        if layers.is_empty() {
            return Err("A source profile needs a base layer".into());
        }
        let mut names = BTreeSet::new();
        for layer in &layers {
            names.extend(layer.files()?);
        }
        for name in &names {
            let mut parent = name.as_str();
            while let Some((p, _)) = parent.rsplit_once('/') {
                if names.contains(p) {
                    return Err(format!(
                        "Layer stack path is both a file and a directory: {p}"
                    ));
                }
                parent = p;
            }
        }
        let mut hash = Sha256::new();
        let mut field = |s: &str| {
            hash.update((s.len() as u64).to_le_bytes());
            hash.update(s.as_bytes());
        };
        field(decoder_version);
        for layer in &layers {
            field(&layer.label);
            for artifact in &layer.artifacts {
                field(&artifact.name);
                field(&artifact.sha256);
            }
        }
        let roots = layers
            .iter()
            .rev()
            .map(|l| l.root.clone())
            .collect::<Vec<_>>();
        let assets = crate::AssetVfs::from_roots(&roots);
        Ok(Self {
            assets,
            precedence: (0..layers.len()).rev().collect(),
            layers,
            identity: hex(&hash.finalize()),
        })
    }
    /// Resolved file metadata and the winning layer index. Resolution uses the
    /// same overlay as open(); no adapter needs to choose a backing container.
    pub fn file_info(&self, path: &str) -> Result<(u64, usize)> {
        let path = virtual_path(path)?;
        let metadata = self.assets.metadata(&path)?;
        if metadata.file_type != VfsFileType::File {
            return Err(format!("Not a virtual file: {path}"));
        }
        for &index in &self.precedence {
            let entry = self.layers[index]
                .root
                .join(&path)
                .map_err(|e| e.to_string())?;
            if entry.exists().map_err(|e| e.to_string())? {
                return Ok((metadata.len, index));
            }
        }
        Err(format!("No source layer for virtual file: {path}"))
    }

    pub fn open(&self, path: &str) -> Result<FileStream> {
        self.assets.open(&virtual_path(path)?)
    }
    pub fn read(&self, path: &str) -> Result<Vec<u8>> {
        let mut bytes = Vec::new();
        self.open(path)?
            .take(MAX_MEMBER + 1)
            .read_to_end(&mut bytes)
            .map_err(|e| e.to_string())?;
        if bytes.len() as u64 > MAX_MEMBER {
            return Err("Asset exceeds 256 MiB limit".into());
        }
        Ok(bytes)
    }
    /// Bounded read that distinguishes missing optional assets from decode and I/O errors.
    pub fn read_io(&self, path: &str, limit: u64) -> io::Result<Vec<u8>> {
        let name = virtual_path(path)
            .map_err(|error| io::Error::new(io::ErrorKind::InvalidInput, error))?;
        self.assets.read_io(&name, limit.min(MAX_MEMBER))
    }
    pub fn files(&self) -> Result<Vec<String>> {
        let mut names = BTreeSet::new();
        for layer in &self.layers {
            names.extend(layer.files()?);
        }
        Ok(names.into_iter().collect())
    }

    pub fn file_size(&self, path: &str) -> Result<u64> {
        self.assets.file_size(&virtual_path(path)?)
    }

    /// The original spelling of `path` from the layer that provides it (the
    /// highest-precedence layer listing it), when its backend recorded one.
    pub fn original_name(&self, path: &str) -> Option<String> {
        let key = virtual_path(path).ok()?;
        for layer in self.precedence.iter().filter_map(|&i| self.layers.get(i)) {
            if let Some(name) = layer.original_names.get(&key) {
                return Some(name.clone());
            }
            if layer
                .root
                .join(&key)
                .and_then(|p| p.is_file())
                .unwrap_or(false)
            {
                return None;
            }
        }
        None
    }

    /// Track numbers are addressed within an explicit ordered source layer.
    /// The returned stream hides all container and backing-file details.
    pub fn open_track(
        &self,
        layer_index: usize,
        number: u32,
        channel: TrackChannel,
    ) -> Result<FileStream> {
        self.layers
            .get(layer_index)
            .ok_or_else(|| format!("source layer {layer_index} does not exist"))?
            .open_track(number, channel)
    }
}

impl FileSystem for SourceFs {
    fn open_file(&self, path: &str) -> VfsResult<FileStream> {
        match self.members.get(path).ok_or(VfsErrorKind::FileNotFound)? {
            Member::InstallShield {
                archive,
                entry,
                identity,
            } => archive.open_member(entry, identity).map_err(failure),
            Member::Nsis {
                archive,
                entry,
                identity,
            } => archive.open_member(entry, identity).map_err(failure),
            Member::Directory { path, .. } => Ok(Box::new(File::open(path)?)),
            Member::Spooled { file, .. } => Ok(Box::new(file.reopen()?)),
            Member::Inno {
                index,
                bytes,
                checksum,
            } => {
                if path.to_ascii_lowercase().ends_with(".zip") {
                    return open_cached_inno_zip(
                        self.installer
                            .as_ref()
                            .ok_or_else(|| failure("Missing installer"))?,
                        *index,
                        *bytes,
                        checksum,
                    )
                    .map(|file| Box::new(file) as FileStream)
                    .map_err(failure);
                }
                if *bytes > MAX_MEMBER {
                    return Err(failure("Installer member exceeds 256 MiB limit"));
                }
                let mut inno = Inno::open(
                    self.installer
                        .as_ref()
                        .ok_or_else(|| failure("Missing installer"))?,
                )
                .map_err(failure)?;
                let mut files = inno.streaming_files(|entry| entry.index() == *index);
                let (_, mut reader) = files
                    .next()
                    .ok_or(VfsErrorKind::FileNotFound)?
                    .map_err(failure)?;
                let mut data = Vec::new();
                reader
                    .by_ref()
                    .take(MAX_MEMBER + 1)
                    .read_to_end(&mut data)?;
                if data.len() as u64 > MAX_MEMBER {
                    return Err(failure("Decompressed member exceeds limit"));
                }
                // EOF verifies Inno's per-file checksum; no unverified bytes
                // are exposed to a decoder.
                let mut eof = [0];
                if reader.read(&mut eof)? != 0 || data.len() as u64 != *bytes {
                    return Err(failure("Installer member length mismatch"));
                }
                Ok(Box::new(Cursor::new(data)))
            }
            Member::InnoGog { parts, bytes, md5 } => {
                let installer = self
                    .installer
                    .as_ref()
                    .ok_or_else(|| failure("Missing installer"))?;
                let mut inno = Inno::open(installer).map_err(failure)?;
                let data =
                    innoidx::read_gog_parts(&mut inno, parts, *bytes, md5).map_err(failure)?;
                Ok(Box::new(Cursor::new(data)))
            }
            Member::PeResource {
                source,
                offset,
                bytes,
            } => {
                if *bytes > MAX_MEMBER {
                    return Err(failure("PE resource exceeds 256 MiB limit"));
                }
                let file = File::open(source)?;
                let size = file.metadata()?.len();
                let end = offset
                    .checked_add(*bytes)
                    .ok_or_else(|| failure("PE resource range overflow"))?;
                if end > size {
                    return Err(failure("PE resource range exceeds its source file"));
                }
                Ok(Box::new(FileRange {
                    file,
                    offset: *offset,
                    length: *bytes,
                    position: 0,
                }))
            }
            Member::Rar { archive, index, .. } => archive.open_member(*index).map_err(failure),
            Member::Zip {
                archive,
                path,
                disk_backed,
                ..
            } => {
                if *disk_backed {
                    return archive
                        .lock()
                        .map_err(|_| failure("ZIP lock was poisoned"))?
                        .stream(path)
                        .map_err(failure);
                }
                let bytes = archive
                    .lock()
                    .map_err(|_| failure("ZIP lock was poisoned"))?
                    .read(path)
                    .map_err(failure)?;
                Ok(Box::new(Cursor::new(bytes)))
            }
            Member::Virtual { path, .. } => path.open_file(),
            #[cfg(feature = "optical-discs")]
            Member::Disc {
                filesystem, entry, ..
            } => Ok(Box::new(DiscFileStream {
                filesystem: filesystem.clone(),
                entry: entry.clone(),
                cursor: 0,
            })),
            Member::VfsFile { path, .. } => path.open_file().map_err(failure),
            Member::Rfa { archive, entry } => archive.open_file(entry).map_err(failure),
        }
    }
    fn read_dir(&self, path: &str) -> VfsResult<Box<dyn Iterator<Item = String> + Send>> {
        if !self.directories.contains(path) {
            return Err(VfsErrorKind::FileNotFound.into());
        }
        let prefix = format!("{path}/");
        let children: BTreeSet<_> = self
            .members
            .keys()
            .chain(self.directories.iter())
            .filter_map(|p| p.strip_prefix(&prefix))
            .filter(|p| !p.is_empty() && !p.contains('/'))
            .map(String::from)
            .collect();
        Ok(Box::new(children.into_iter()))
    }
    fn metadata(&self, path: &str) -> VfsResult<VfsMetadata> {
        let (file_type, len) = if let Some(m) = self.members.get(path) {
            (
                VfsFileType::File,
                if let Member::Nsis {
                    archive,
                    entry,
                    identity,
                } = m
                {
                    archive
                        .open_member(entry, identity)
                        .map_err(failure)?
                        .seek(SeekFrom::End(0))?
                } else if let Member::Virtual { path, .. } = m {
                    path.metadata()?.len
                } else {
                    m.bytes()
                },
            )
        } else if self.directories.contains(path) {
            (VfsFileType::Directory, 0)
        } else {
            return Err(VfsErrorKind::FileNotFound.into());
        };
        Ok(VfsMetadata {
            file_type,
            len,
            created: None,
            modified: None,
            accessed: None,
        })
    }
    fn exists(&self, path: &str) -> VfsResult<bool> {
        Ok(self.members.contains_key(path) || self.directories.contains(path))
    }
    fn create_dir(&self, _: &str) -> VfsResult<()> {
        Err(VfsErrorKind::NotSupported.into())
    }
    fn create_file(&self, _: &str) -> VfsResult<Box<dyn SeekAndWrite + Send>> {
        Err(VfsErrorKind::NotSupported.into())
    }
    fn append_file(&self, _: &str) -> VfsResult<Box<dyn SeekAndWrite + Send>> {
        Err(VfsErrorKind::NotSupported.into())
    }
    fn remove_file(&self, _: &str) -> VfsResult<()> {
        Err(VfsErrorKind::NotSupported.into())
    }
    fn remove_dir(&self, _: &str) -> VfsResult<()> {
        Err(VfsErrorKind::NotSupported.into())
    }
}

/// A seekable view of one PE resource. The source executable stays file-backed;
/// only bytes requested by the game decoder are read into memory.
#[derive(Debug)]
struct FileRange {
    file: File,
    offset: u64,
    length: u64,
    position: u64,
}
impl Read for FileRange {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        let remaining = self.length.saturating_sub(self.position);
        let count = buffer
            .len()
            .min(usize::try_from(remaining).unwrap_or(usize::MAX));
        if count == 0 {
            return Ok(0);
        }
        let absolute = self.offset.checked_add(self.position).ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidInput, "PE resource offset overflow")
        })?;
        self.file.seek(SeekFrom::Start(absolute))?;
        let read = self.file.read(&mut buffer[..count])?;
        self.position += read as u64;
        Ok(read)
    }
}
impl Seek for FileRange {
    fn seek(&mut self, from: SeekFrom) -> io::Result<u64> {
        let target = match from {
            SeekFrom::Start(value) => i128::from(value),
            SeekFrom::Current(delta) => i128::from(self.position) + i128::from(delta),
            SeekFrom::End(delta) => i128::from(self.length) + i128::from(delta),
        };
        if !(0..=i128::from(self.length)).contains(&target) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "seek outside PE resource",
            ));
        }
        self.position = target as u64;
        Ok(self.position)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use vfs::MemoryFS;

    #[test]
    fn streamed_installer_zip_cache_checks_length_and_digest() {
        let root = std::env::temp_dir().join(format!(
            "og-inno-zip-cache-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        fs::create_dir(&root).unwrap();
        let cached = root.join("member.bin");
        let manifest = root.join("member.sha256");
        let mut source = Cursor::new(b"archive bytes".to_vec());
        save_inno_zip_reader(&mut source, 3, 13, &cached, &manifest).unwrap();
        assert!(cached_rfa_is_valid(&cached, &manifest, 13));
        fs::write(&cached, b"tampered data").unwrap();
        assert!(!cached_rfa_is_valid(&cached, &manifest, 13));
        let mut too_long = Cursor::new(b"too long".to_vec());
        assert!(save_inno_zip_reader(&mut too_long, 3, 7, &cached, &manifest).is_err());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn copy_fallback_reuses_a_valid_profile_on_later_launches() {
        let temporary = tempfile::tempdir().unwrap();
        let source = temporary.path().join("blob.rfa");
        let destination = temporary.path().join("profile.rfa");
        let payload = b"synthetic immutable materialization";
        fs::write(&source, payload).unwrap();
        fs::write(
            source.with_extension("sha256"),
            format!("1 {} {}\n", payload.len(), hex(&Sha256::digest(payload))),
        )
        .unwrap();
        let unavailable =
            |_: &Path, _: &Path| Err(std::io::Error::from(std::io::ErrorKind::Unsupported));
        share_cached_file_with_link(&source, &destination, unavailable).unwrap();
        share_cached_file_with_link(
            &source.with_extension("sha256"),
            &destination.with_extension("sha256"),
            unavailable,
        )
        .unwrap();
        let before = fs::metadata(&destination).unwrap();
        publish_cached_rfa(&source, &destination, payload.len() as u64).unwrap();
        assert_eq!(fs::read(&destination).unwrap(), payload);
        #[cfg(unix)]
        {
            use std::os::unix::fs::MetadataExt;
            assert_eq!(before.ino(), fs::metadata(&destination).unwrap().ino());
        }
        let _ = before;
        fs::write(&destination, b"stale").unwrap();
        publish_cached_rfa(&source, &destination, payload.len() as u64).unwrap();
        assert!(cached_rfa_is_valid(
            &destination,
            &destination.with_extension("sha256"),
            payload.len() as u64
        ));
        assert_eq!(fs::read(&source).unwrap(), payload);
    }

    #[test]
    fn materialized_files_share_storage_without_sharing_profile_keys() {
        let temporary = tempfile::tempdir().unwrap();
        let cache = open_games_cache::Cache::new("game", Some(temporary.path())).unwrap();
        let session = cache.lazy_session().unwrap();
        let first = session.with_profile(&["base", "mod-one"]).unwrap();
        let second = session.with_profile(&["base", "mod-two"]).unwrap();
        let first_dir = first.entry("decoder", &["source"]).unwrap();
        let second_dir = second.entry("decoder", &["source"]).unwrap();
        assert_ne!(first_dir, second_dir);
        let blob_dir = session.entry("blob-decoder", &["source"]).unwrap();
        let blob = blob_dir.join("synthetic.rfa");
        fs::write(&blob, b"synthetic immutable materialization").unwrap();
        let a = first_dir.join("synthetic.rfa");
        let b = second_dir.join("synthetic.rfa");
        share_cached_file(&blob, &a).unwrap();
        share_cached_file(&blob, &b).unwrap();
        assert_eq!(fs::read(&a).unwrap(), fs::read(&b).unwrap());
        #[cfg(unix)]
        {
            use std::os::unix::fs::MetadataExt;
            if fs::hard_link(&blob, temporary.path().join("probe")).is_ok() {
                assert_eq!(
                    fs::metadata(&a).unwrap().ino(),
                    fs::metadata(&b).unwrap().ino()
                );
            }
        }
        // Replacing a stale profile link is atomic and cannot change the blob.
        fs::remove_file(&a).unwrap();
        fs::write(&a, b"stale").unwrap();
        share_cached_file(&blob, &a).unwrap();
        assert_eq!(fs::read(&a).unwrap(), fs::read(&blob).unwrap());
        drop(first);
        drop(second);
        drop(session);
        cache.clear().unwrap();
        assert!(!blob.exists() && !a.exists() && !b.exists());
    }

    #[test]
    fn profiles_keep_disc_order_then_apply_updates_and_mods() {
        let temporary = tempfile::tempdir().unwrap();
        let paths: Vec<_> = ["disc-one", "disc-two-longer", "patch", "mod"]
            .iter()
            .map(|name| {
                let path = temporary.path().join(name);
                fs::create_dir(&path).unwrap();
                fs::write(path.join("asset.dat"), name.as_bytes()).unwrap();
                path
            })
            .collect();
        let base = paths[..2].to_vec();
        let original = Sources::profile(&base, &[], &[], "v1").unwrap();
        assert_eq!(original.layers[0].label, "base-disc-1");
        assert_eq!(original.read("ASSET.DAT").unwrap(), b"disc-one");
        assert_eq!(original.file_info("ASSET.DAT").unwrap(), (8, 0));
        let updates = vec![("1".into(), paths[2].clone())];
        let mods = vec![("custom".into(), paths[3].clone())];
        let patched = Sources::profile(&base, &updates, &[], "v1").unwrap();
        assert_eq!(patched.read("asset.dat").unwrap(), b"patch");
        assert_eq!(patched.file_info("asset.dat").unwrap(), (5, 2));
        let modified = Sources::profile(&base, &updates, &mods, "v1").unwrap();
        assert_eq!(modified.read("asset.dat").unwrap(), b"mod");
        assert_eq!(modified.file_info("asset.dat").unwrap(), (3, 3));
        assert_ne!(original.identity, patched.identity);
        assert_ne!(patched.identity, modified.identity);
        assert_ne!(
            modified.identity,
            Sources::profile(&base, &updates, &mods, "v2")
                .unwrap()
                .identity
        );
        assert_eq!(fs::read(paths[0].join("asset.dat")).unwrap(), b"disc-one");
    }

    fn layer(label: &str, content: &str) -> Layer {
        let root: VfsPath = MemoryFS::new().into();
        root.join("data").unwrap().create_dir().unwrap();
        root.join("data/file")
            .unwrap()
            .create_file()
            .unwrap()
            .write_all(content.as_bytes())
            .unwrap();
        Layer {
            root,
            label: label.into(),
            artifacts: vec![Artifact {
                name: "file".into(),
                bytes: content.len() as u64,
                sha256: hex(&Sha256::digest(content)),
            }],
            disc: None,
            warnings: Vec::new(),
            track_sources: Vec::new(),
            installer_rfa_source: None,
            indexed_files: None,
            original_names: BTreeMap::new(),
        }
    }
    #[test]
    fn overlays_are_ordered_and_identity_includes_order() {
        let sources = Sources::new(
            vec![
                layer("base", "old"),
                layer("update", "new"),
                layer("mod", "custom"),
            ],
            "1",
        )
        .unwrap();
        assert_eq!(sources.read("DATA\\File").unwrap(), b"custom");
        assert_eq!(sources.files().unwrap(), ["data/file"]);
        let reversed = Sources::new(
            vec![
                layer("mod", "custom"),
                layer("update", "new"),
                layer("base", "old"),
            ],
            "1",
        )
        .unwrap();
        assert_eq!(reversed.read("data/file").unwrap(), b"old");
        assert_ne!(sources.identity, reversed.identity);
        assert!(sources.read("missing").is_err());
    }
    #[test]
    fn disk_backed_zip_stream_is_seekable_and_matches_verified_member() {
        let mut zip = ::zip::ZipWriter::new(Cursor::new(Vec::new()));
        let options = ::zip::write::SimpleFileOptions::default()
            .compression_method(::zip::CompressionMethod::Deflated);
        zip.start_file("music.ogg", options).unwrap();
        let bytes: Vec<_> = (0..100_000).map(|i| (i % 251) as u8).collect();
        zip.write_all(&bytes).unwrap();
        let zip = zip.finish().unwrap().into_inner();
        let layer =
            Layer::zip_stream_disk_backed(Box::new(Cursor::new(zip)), "synthetic-stream").unwrap();
        let sources = Sources::new(vec![layer], "stream-test").unwrap();
        let mut stream = sources.open("music.ogg").unwrap();
        assert_eq!(stream.seek(SeekFrom::End(0)).unwrap(), bytes.len() as u64);
        stream.seek(SeekFrom::Start(99_000)).unwrap();
        let mut actual = Vec::new();
        stream.read_to_end(&mut actual).unwrap();
        assert_eq!(actual, bytes[99_000..]);
        stream.rewind().unwrap();
        actual.clear();
        stream.read_to_end(&mut actual).unwrap();
        assert_eq!(actual, bytes);
    }

    #[test]
    fn disk_backed_zip_stream_rejects_corrupt_payload_before_exposure() {
        let mut zip = ::zip::ZipWriter::new(Cursor::new(Vec::new()));
        let options = ::zip::write::SimpleFileOptions::default()
            .compression_method(::zip::CompressionMethod::Stored);
        zip.start_file("music.ogg", options).unwrap();
        let payload = b"synthetic recording with a trusted ZIP checksum";
        zip.write_all(payload).unwrap();
        let mut bytes = zip.finish().unwrap().into_inner();
        let offset = bytes
            .windows(payload.len())
            .position(|w| w == payload)
            .unwrap();
        bytes[offset] ^= 1;
        let layer =
            Layer::zip_stream_disk_backed(Box::new(Cursor::new(bytes)), "corrupt-stream").unwrap();
        let sources = Sources::new(vec![layer], "stream-test").unwrap();
        assert!(sources.open("music.ogg").is_err());
    }

    #[test]
    fn nested_zip_and_explicit_update_mapping_share_overlay_precedence() {
        let root: VfsPath = MemoryFS::new().into();
        let mut zip = ::zip::ZipWriter::new(Cursor::new(Vec::new()));
        let options = ::zip::write::SimpleFileOptions::default()
            .compression_method(::zip::CompressionMethod::Deflated);
        for (name, bytes) in [("Assets/SHIP.DAT", b"base"), ("other.dat", b"keep")] {
            zip.start_file(name, options).unwrap();
            zip.write_all(bytes).unwrap();
        }
        let packed = zip.finish().unwrap().into_inner();
        root.join("data.res")
            .unwrap()
            .create_file()
            .unwrap()
            .write_all(&packed)
            .unwrap();
        let parent = Layer {
            root,
            label: "base:1.0".into(),
            artifacts: vec![Artifact {
                name: "disc.img".into(),
                bytes: packed.len() as u64,
                sha256: hex(&Sha256::digest(&packed)),
            }],
            disc: None,
            track_sources: Vec::new(),
            warnings: vec!["source diagnostic".into()],
            installer_rfa_source: None,
            indexed_files: None,
            original_names: BTreeMap::new(),
        };
        let nested = parent.zip_archive("DATA.RES", "english").unwrap();
        assert!(nested.label.contains("base:1.0"));
        assert_eq!(nested.artifacts[0].sha256, parent.artifacts[0].sha256);
        assert_eq!(nested.warnings, parent.warnings);
        let mut update = layer("update:1.0.2", "patched");
        update.warnings.push("update diagnostic".into());
        let selected = update
            .select(&[("data/file", "assets/ship.dat")], "patch.lst")
            .unwrap();
        assert_eq!(selected.warnings, update.warnings);
        let sources = Sources::new(vec![nested, selected], "decoder-1").unwrap();
        assert_eq!(sources.files().unwrap(), ["assets/ship.dat", "other.dat"]);
        assert_eq!(sources.file_size("assets/ship.dat").unwrap(), 7);
        assert_eq!(sources.read("ASSETS\\SHIP.DAT").unwrap(), b"patched");
        assert_eq!(sources.read("other.dat").unwrap(), b"keep");
        assert!(sources.read("data/file").is_err());
        assert!(update.select(&[("data/file", "../escape")], "bad").is_err());
        assert!(
            update
                .select(&[("data/file", "same"), ("data/file", "SAME")], "bad")
                .is_err()
        );
        assert!(update.select(&[("data", "dir")], "bad").is_err());
    }

    #[test]
    fn rejects_escaping_virtual_paths() {
        for p in [
            "../file",
            "a/../b",
            "/etc/passwd",
            "C:\\file",
            "a\0b",
            "a//b",
            "a/./b",
            "a/.whiteout/b",
        ] {
            assert!(virtual_path(p).is_err(), "{p:?}");
        }
    }

    #[test]
    fn directory_adapter_preserves_sources_and_rejects_aliases() {
        let nonce = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let path = std::env::temp_dir().join(format!("og-vfs-{}-{nonce}", std::process::id()));
        fs::create_dir_all(path.join("Data")).unwrap();
        fs::write(path.join("Data/ORIGINAL.DAT"), b"original bytes").unwrap();
        let layer = Layer::open(&path, "base-1").unwrap();
        assert!(layer.root.join("new").unwrap().create_file().is_err());
        let sources = Sources::new(vec![layer], "decoder-1").unwrap();
        assert_eq!(
            sources.read("DATA/original.dat").unwrap(),
            b"original bytes"
        );
        let mut stream = sources.open("data/original.dat").unwrap();
        use std::io::{Seek, SeekFrom};
        stream.seek(SeekFrom::Start(9)).unwrap();
        let mut tail = String::new();
        stream.read_to_string(&mut tail).unwrap();
        assert_eq!(tail, "bytes");
        std::os::unix::fs::symlink("/etc/passwd", path.join("Data/escape")).unwrap();
        assert!(Layer::open(&path, "bad").is_err());
        fs::remove_file(path.join("Data/escape")).unwrap();
        if let Ok(mut alias) = fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(path.join("Data/original.dat"))
        {
            alias.write_all(b"different case").unwrap();
            assert!(Layer::open(&path, "ambiguous").is_err());
        }
        assert_eq!(
            fs::read(path.join("Data/ORIGINAL.DAT")).unwrap(),
            b"original bytes"
        );
        fs::remove_dir_all(path).unwrap();
    }

    #[cfg(feature = "optical-discs")]
    fn both_endian(bytes: &mut [u8], offset: usize, value: u32) {
        bytes[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
        bytes[offset + 4..offset + 8].copy_from_slice(&value.to_be_bytes());
    }

    #[cfg(feature = "optical-discs")]
    fn iso_record(lba: u32, size: u32, flags: u8, name: &[u8]) -> Vec<u8> {
        let length = 33 + name.len() + usize::from(name.len().is_multiple_of(2));
        let mut record = vec![0; length];
        record[0] = length as u8;
        both_endian(&mut record, 2, lba);
        both_endian(&mut record, 10, size);
        record[18..25].copy_from_slice(&[100, 1, 1, 0, 0, 0, 0]);
        record[25] = flags;
        record[28..30].copy_from_slice(&1u16.to_le_bytes());
        record[30..32].copy_from_slice(&1u16.to_be_bytes());
        record[32] = name.len() as u8;
        record[33..33 + name.len()].copy_from_slice(name);
        record
    }

    #[cfg(feature = "optical-discs")]
    fn raw_mode1(user_data: &[u8; 2048]) -> [u8; 2352] {
        let mut sector = [0; 2352];
        sector[0] = 0;
        sector[1..11].fill(0xff);
        sector[11] = 0;
        sector[15] = 1;
        sector[16..16 + user_data.len()].copy_from_slice(user_data);
        sector
    }

    #[cfg(feature = "optical-discs")]
    fn synthetic_iso_sectors() -> Vec<[u8; 2048]> {
        let mut sectors = vec![[0; 2048]; 24];
        let mut pvd = [0; 2048];
        pvd[0] = 1;
        pvd[1..6].copy_from_slice(b"CD001");
        pvd[6] = 1;
        pvd[40..44].copy_from_slice(b"BOTF");
        both_endian(&mut pvd, 80, 24);
        pvd[120..124].copy_from_slice(&[1, 0, 0, 1]);
        pvd[124..128].copy_from_slice(&[1, 0, 0, 1]);
        for offset in [813, 830, 847, 864] {
            pvd[offset..offset + 16].copy_from_slice(b"2000010100000000");
        }
        pvd[881] = 1;
        pvd[128..130].copy_from_slice(&2048u16.to_le_bytes());
        pvd[130..132].copy_from_slice(&2048u16.to_be_bytes());
        let root_record = iso_record(20, 2048, 2, &[0]);
        pvd[156..156 + root_record.len()].copy_from_slice(&root_record);
        sectors[16] = pvd;
        let mut terminator = [0; 2048];
        terminator[0] = 255;
        terminator[1..6].copy_from_slice(b"CD001");
        terminator[6] = 1;
        sectors[17] = terminator;
        let mut directory = [0; 2048];
        let dot = iso_record(20, 2048, 2, &[0]);
        let parent = iso_record(20, 2048, 2, &[1]);
        let file = iso_record(21, 12, 0, b"HELLO.TXT;1");
        directory[..dot.len()].copy_from_slice(&dot);
        directory[dot.len()..dot.len() + parent.len()].copy_from_slice(&parent);
        let offset = dot.len() + parent.len();
        directory[offset..offset + file.len()].copy_from_slice(&file);
        sectors[20] = directory;
        sectors[21][..12].copy_from_slice(b"hello, botf\n");
        sectors
    }

    fn cue_text(bin: &str, tracks: &str) -> String {
        format!("FILE \"{bin}\" BINARY\n{tracks}")
    }

    #[cfg(feature = "optical-discs")]
    #[test]
    fn cue_bin_mode1_streams_files_fingerprints_and_metadata() {
        let nonce = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let root = std::env::temp_dir().join(format!("og-cue-{}-{nonce}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let cue = root.join("disc.cue");
        let bin = root.join("disc.bin");
        let sectors = synthetic_iso_sectors();
        let image: Vec<u8> = sectors.iter().flat_map(raw_mode1).collect();
        fs::write(&bin, &image).unwrap();
        fs::write(
            &cue,
            cue_text("disc.bin", " TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n"),
        )
        .unwrap();
        let layer = Layer::open(&cue, "synthetic-cue").unwrap();
        assert_eq!(layer.artifacts.len(), 2);
        let cue_artifact = layer
            .artifacts
            .iter()
            .find(|a| a.name == "disc.cue")
            .unwrap();
        let bin_artifact = layer
            .artifacts
            .iter()
            .find(|a| a.name == "disc.bin")
            .unwrap();
        assert_eq!(
            cue_artifact.sha256,
            crate::sha256_hex(&fs::read(&cue).unwrap())
        );
        assert_eq!(bin_artifact.sha256, crate::sha256_hex(&image));
        let disc = layer.disc.as_ref().unwrap();
        assert_eq!(disc.format, "BIN/CUE");
        assert_eq!(disc.volume_label.as_deref(), Some("BOTF"));
        assert_eq!(disc.tracks.len(), 1);
        assert_eq!(disc.tracks[0].number, 1);
        assert_eq!(disc.tracks[0].length_sectors, 24);
        let sources = Sources::new(vec![layer], "cue-test").unwrap();
        assert_eq!(sources.files().unwrap(), ["hello.txt"]);
        assert_eq!(sources.read("HELLO.TXT").unwrap(), b"hello, botf\n");
        fs::remove_dir_all(root).unwrap();
    }

    #[cfg(feature = "optical-discs")]
    #[test]
    fn cue_bin_mode1_cooked_sectors_are_readable() {
        let nonce = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let root =
            std::env::temp_dir().join(format!("og-cue-cooked-{}-{nonce}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let cue = root.join("disc.cue");
        let bin = root.join("disc.bin");
        let sectors = synthetic_iso_sectors();
        let image: Vec<u8> = sectors.iter().flatten().copied().collect();
        fs::write(&bin, &image).unwrap();
        fs::write(
            &cue,
            cue_text("disc.bin", " TRACK 01 MODE1/2048\n  INDEX 01 00:00:00\n"),
        )
        .unwrap();
        let sources =
            Sources::new(vec![Layer::open(&cue, "cooked-cue").unwrap()], "cue-test").unwrap();
        assert_eq!(sources.read("hello.txt").unwrap(), b"hello, botf\n");
        fs::remove_dir_all(root).unwrap();
    }

    #[cfg(feature = "optical-discs")]
    #[test]
    fn cue_bin_audio_track_and_invalid_sources() {
        use std::io::Read;
        let nonce = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let root =
            std::env::temp_dir().join(format!("og-cue-errors-{}-{nonce}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let cue = root.join("disc.cue");
        let bin = root.join("disc.bin");
        let sectors = synthetic_iso_sectors();
        let mut image: Vec<u8> = sectors.iter().flat_map(raw_mode1).collect();
        let audio = vec![0xa5; 2352];
        image.extend_from_slice(&audio);
        fs::write(&bin, &image).unwrap();
        fs::write(
            &cue,
            cue_text(
                "disc.bin",
                " TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n TRACK 02 AUDIO\n  INDEX 01 00:00:24\n",
            ),
        )
        .unwrap();
        let sources =
            Sources::new(vec![Layer::open(&cue, "multi-track").unwrap()], "cue-test").unwrap();
        let mut stream = sources.open_track(0, 2, TrackChannel::MainChannel).unwrap();
        let mut read = Vec::new();
        stream.read_to_end(&mut read).unwrap();
        assert_eq!(read, audio);

        let missing_cue = root.join("missing.cue");
        fs::write(
            &missing_cue,
            cue_text("missing.bin", " TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n"),
        )
        .unwrap();
        assert!(Layer::open(&missing_cue, "missing").is_err());
        let subdir = root.join("subdir");
        fs::create_dir(&subdir).unwrap();
        let outside = root.parent().unwrap().join(format!("outside-{nonce}.bin"));
        fs::write(&outside, &image).unwrap();
        let outside_cue = subdir.join("outside.cue");
        fs::write(
            &outside_cue,
            cue_text(
                &format!("../outside-{nonce}.bin"),
                " TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n",
            ),
        )
        .unwrap();
        assert!(Layer::open(&outside_cue, "outside").is_err());
        fs::remove_file(&outside).unwrap();
        #[cfg(unix)]
        {
            std::os::unix::fs::symlink(&bin, root.join("alias.bin")).unwrap();
            fs::write(
                &cue,
                cue_text("alias.bin", " TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n"),
            )
            .unwrap();
            assert!(Layer::open(&cue, "symlink").is_err());
        }
        fs::write(
            &cue,
            cue_text("disc.bin", " TRACK 01 MODE1/2352\n  INDEX 01 00:00:25\n"),
        )
        .unwrap();
        assert!(Layer::open(&cue, "past-end").is_err());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn cue_bin_rejects_empty_filesystem() {
        let nonce = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let root =
            std::env::temp_dir().join(format!("og-cue-empty-{}-{nonce}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let cue = root.join("empty.cue");
        let bin = root.join("empty.bin");
        fs::write(&bin, vec![0; 24 * 2352]).unwrap();
        fs::write(
            &cue,
            cue_text("empty.bin", " TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n"),
        )
        .unwrap();
        assert!(Layer::open(&cue, "empty").is_err());
        fs::remove_dir_all(root).unwrap();
    }

    #[cfg(feature = "optical-discs")]
    #[test]
    fn clonecd_vfs_streams_files_and_both_track_channels_without_extraction() {
        use std::io::{Read, Seek, SeekFrom};

        let nonce = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let scratch = std::env::var_os("RUNNER_TEMP")
            .map(PathBuf::from)
            .unwrap_or_else(std::env::temp_dir);
        let root = scratch.join(format!("og-ccd-{}-{nonce}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let ccd = root.join("TEST.ccd");
        let img = root.join("TEST.img");
        let sub = root.join("TEST.sub");

        let mut sectors = vec![[0; 2048]; 24];
        let mut pvd = [0; 2048];
        pvd[0] = 1;
        pvd[1..6].copy_from_slice(b"CD001");
        pvd[6] = 1;
        pvd[40..44].copy_from_slice(b"BOTF");
        both_endian(&mut pvd, 80, 24);
        pvd[120..124].copy_from_slice(&[1, 0, 0, 1]); // volume set size
        pvd[124..128].copy_from_slice(&[1, 0, 0, 1]); // volume sequence
        for offset in [813, 830, 847, 864] {
            pvd[offset..offset + 16].copy_from_slice(b"2000010100000000");
        }
        pvd[881] = 1; // file structure version
        pvd[128..130].copy_from_slice(&2048u16.to_le_bytes());
        pvd[130..132].copy_from_slice(&2048u16.to_be_bytes());
        let root_record = iso_record(20, 2048, 2, &[0]);
        pvd[156..156 + root_record.len()].copy_from_slice(&root_record);
        sectors[16] = pvd;

        let mut terminator = [0; 2048];
        terminator[0] = 255;
        terminator[1..6].copy_from_slice(b"CD001");
        terminator[6] = 1;
        sectors[17] = terminator;

        let mut directory = [0; 2048];
        let dot = iso_record(20, 2048, 2, &[0]);
        let parent = iso_record(20, 2048, 2, &[1]);
        let file = iso_record(21, 12, 0, b"HELLO.TXT;1");
        directory[..dot.len()].copy_from_slice(&dot);
        directory[dot.len()..dot.len() + parent.len()].copy_from_slice(&parent);
        let file_start = dot.len() + parent.len();
        directory[file_start..file_start + file.len()].copy_from_slice(&file);
        sectors[20] = directory;
        sectors[21][..12].copy_from_slice(b"hello, botf\n");

        let mut image = Vec::new();
        for data in &sectors {
            image.extend_from_slice(&raw_mode1(data));
        }
        fs::write(&img, &image).unwrap();
        fs::write(&sub, vec![0x5a; sectors.len() * 96]).unwrap();
        fs::write(
            &ccd,
            "[CloneCD]\nVersion=3\n[Disc]\nTocEntries=4\nSessions=1\n\
             [Entry 2]\nPoint=0xa2\nPLBA=24\n\
             [TRACK 1]\nMODE=1\nINDEX 1=0\n",
        )
        .unwrap();

        // Exercise the same synthetic filesystem through the ISO Sources seam.
        let iso = root.join("TEST.iso");
        let iso_bytes: Vec<u8> = sectors.iter().flatten().copied().collect();
        fs::write(&iso, &iso_bytes).unwrap();
        let iso_layer = Layer::open(&iso, "synthetic-iso-base").unwrap();
        assert_eq!(iso_layer.artifacts.len(), 1);
        assert_eq!(iso_layer.artifacts[0].sha256, crate::sha256_hex(&iso_bytes));
        let iso_sources = Sources::new(vec![iso_layer], "iso-seam-test").unwrap();
        assert_eq!(iso_sources.files().unwrap(), ["hello.txt"]);
        assert_eq!(iso_sources.file_info("HELLO.TXT").unwrap(), (12, 0));
        assert_eq!(iso_sources.read("hello.txt").unwrap(), b"hello, botf\n");
        assert!(iso_sources.file_info("missing").is_err());

        let layer = Layer::open(&ccd, "synthetic-clonecd-base").unwrap();
        let disc = layer.disc.as_ref().unwrap();
        assert_eq!(disc.volume_label.as_deref(), Some("BOTF"));
        assert_eq!(disc.tracks.len(), 1);
        assert_eq!(disc.tracks[0].length_sectors, 24);
        assert_eq!(layer.artifacts.len(), 3);
        let sources = Sources::new(vec![layer], "clonecd-reader-test").unwrap();
        assert_eq!(sources.read("hello.txt").unwrap(), b"hello, botf\n");

        let mut main = sources.open_track(0, 1, TrackChannel::MainChannel).unwrap();
        assert_eq!(main.seek(SeekFrom::End(0)).unwrap(), 24 * 2352);
        main.seek(SeekFrom::Start(21 * 2352 + 16)).unwrap();
        let mut text = [0; 12];
        main.read_exact(&mut text).unwrap();
        assert_eq!(&text, b"hello, botf\n");

        let mut subchannel = sources.open_track(0, 1, TrackChannel::Subchannel).unwrap();
        assert_eq!(subchannel.seek(SeekFrom::End(0)).unwrap(), 24 * 96);
        let mut sample = [0; 96];
        subchannel.seek(SeekFrom::Start(0)).unwrap();
        subchannel.read_exact(&mut sample).unwrap();
        assert!(sample.iter().all(|byte| *byte == 0x5a));
        assert_eq!(fs::read(&img).unwrap(), image);
        fs::remove_dir_all(root).unwrap();
    }
}
