//! Bounded ZIP member access over a source-backed VFS stream.

use super::FileStream;
use std::collections::{BTreeMap, BTreeSet};
use std::io::{BufReader, Read, Seek, SeekFrom};
use zip::ZipArchive;

const MAX_ARCHIVE_ENTRIES: usize = 20_000;
const MAX_RESOURCE_BYTES: u64 = 256 * 1024 * 1024;

use super::Result;

/// Metadata for one resource entry. Names are retained exactly as stored.
#[derive(Clone, Debug, PartialEq, Eq)]
pub(super) struct Entry {
    pub name: String,
    pub bytes: u64,
}

/// An indexed resource archive backed by a VFS stream. The compressed archive
/// remains on its original source; only the requested resource is decoded.
pub(super) struct Archive {
    archive: ZipArchive<BufReader<FileStream>>,
    entries: Vec<Entry>,
    by_name: BTreeMap<String, usize>,
}

fn key(name: &str) -> Result<String> {
    let name = name.replace('\\', "/");
    if name.is_empty()
        || name.starts_with('/')
        || name.contains([':', '\0'])
        || name
            .split('/')
            .any(|component| component.is_empty() || component == "." || component == "..")
    {
        return Err(format!("unsafe resource path: {name:?}"));
    }
    Ok(name.to_ascii_lowercase())
}

impl Archive {
    /// Index ZIP metadata without decompressing resource payloads.
    pub fn open(stream: FileStream, visit: &mut dyn FnMut(&str, u64, u32)) -> Result<Self> {
        let archive = ZipArchive::new(BufReader::new(stream))
            .map_err(|e| format!("Open resource ZIP: {e}"))?;
        if archive.len() > MAX_ARCHIVE_ENTRIES {
            return Err(format!(
                "resource ZIP contains more than {MAX_ARCHIVE_ENTRIES} entries"
            ));
        }
        let mut indexed = Vec::with_capacity(archive.len());
        let mut seen_names = BTreeSet::new();
        for index in 0..archive.len() {
            let entry = archive
                .by_index_data(index)
                .map_err(|e| format!("Read resource ZIP directory: {e}"))?;
            let name = entry
                .name()
                .map_err(|e| format!("Resource ZIP name is not UTF-8: {e}"))?
                .into_owned();
            let lookup_name = if entry.is_dir() {
                name.strip_suffix('/')
                    .or_else(|| name.strip_suffix('\\'))
                    .ok_or_else(|| {
                        format!("resource directory lacks trailing separator: {name:?}")
                    })?
            } else {
                name.as_str()
            };
            let lookup_key = key(lookup_name)?;
            if entry.is_symlink() || entry.encrypted() {
                return Err(format!(
                    "resource ZIP links and encrypted entries are unsupported: {name:?}"
                ));
            }
            if entry.is_dir() {
                continue;
            }
            if !seen_names.insert(lookup_key.clone()) {
                return Err(format!("case-ambiguous resource ZIP path: {name}"));
            }
            visit(&name, entry.size(), entry.crc32());
            indexed.push((
                Entry {
                    name,
                    bytes: entry.size(),
                },
                lookup_key,
                index,
            ));
        }
        // Keep the archive as the source of truth and sort inventory output for
        // deterministic comparisons.
        indexed.sort_by(|a, b| a.0.name.cmp(&b.0.name));
        let entries = indexed.iter().map(|(entry, _, _)| entry.clone()).collect();
        let by_name = indexed
            .into_iter()
            .map(|(_, name, index)| (name, index))
            .collect();
        Ok(Self {
            archive,
            entries,
            by_name,
        })
    }

    pub fn entries(&self) -> &[Entry] {
        &self.entries
    }

    pub fn stream(&mut self, path: &str) -> Result<FileStream> {
        let index = *self
            .by_name
            .get(&key(path)?)
            .ok_or("resource was not found")?;
        let mut entry = self.archive.by_index(index).map_err(|e| e.to_string())?;
        let size = entry.size();
        if size > MAX_RESOURCE_BYTES {
            return Err("resource stream exceeds 256 MiB".into());
        }
        let root = if let Some(path) = std::env::var_os("OPEN_GAMES_VFS_CACHE_DIR") {
            std::path::PathBuf::from(path)
        } else if cfg!(debug_assertions) {
            std::path::PathBuf::from(".local/open-games-vfs-cache")
        } else {
            directories::ProjectDirs::from("org", "open-games", "vfs")
                .ok_or("No platform cache directory")?
                .cache_dir()
                .to_path_buf()
        };
        std::fs::create_dir_all(&root).map_err(|e| e.to_string())?;
        let mut file = tempfile::tempfile_in(root).map_err(|e| e.to_string())?;
        let copied = std::io::copy(&mut (&mut entry).take(size + 1), &mut file)
            .map_err(|e| format!("Decode streamed resource: {e}"))?;
        if copied != size
            || entry
                .read(&mut [0; 1])
                .map_err(|e| format!("Verify streamed resource: {e}"))?
                != 0
        {
            return Err("streamed resource size differs from ZIP record".into());
        }
        file.seek(SeekFrom::Start(0)).map_err(|e| e.to_string())?;
        Ok(Box::new(file))
    }

    pub fn read(&mut self, path: &str) -> Result<Vec<u8>> {
        let lookup_key = key(path)?;
        let index = *self
            .by_name
            .get(&lookup_key)
            .ok_or_else(|| format!("resource was not found: {path:?}"))?;
        let mut entry = self
            .archive
            .by_index(index)
            .map_err(|e| format!("Open resource {path:?}: {e}"))?;
        let declared_size = entry.size();
        if declared_size > MAX_RESOURCE_BYTES {
            return Err(format!(
                "resource {path:?} exceeds {MAX_RESOURCE_BYTES} byte limit"
            ));
        }
        let capacity = usize::try_from(declared_size)
            .map_err(|_| format!("resource {path:?} does not fit in memory"))?;
        let mut bytes = Vec::new();
        bytes
            .try_reserve_exact(capacity)
            .map_err(|e| format!("Allocate resource {path:?}: {e}"))?;
        (&mut entry)
            .take(declared_size.saturating_add(1))
            .read_to_end(&mut bytes)
            .map_err(|e| format!("Decode resource {path:?}: {e}"))?;
        if bytes.len() as u64 != declared_size {
            return Err(format!(
                "resource {path:?} size differs from its ZIP record"
            ));
        }
        // Force the decoder through EOF so the upstream ZIP reader can verify
        // the member checksum before exposing the bytes to game code.
        if entry
            .read(&mut [0; 1])
            .map_err(|e| format!("Verify resource {path:?}: {e}"))?
            != 0
        {
            return Err(format!("resource {path:?} exceeds its declared size"));
        }
        Ok(bytes)
    }
}
