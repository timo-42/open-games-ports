//! ISO9660 and nested ZIP adaptation. Upstream crates own both container formats.
//! Only one requested ZIP member is decompressed, bounded to 256 MiB. No installer
//! runs, no extracted tree/cache is created, and source containers stay on disk.
use super::{AssetStream, Result, virtual_path};
use iso9660::{DirectoryEntry, ISO9660, ISODirectory};
use std::{
    collections::{HashMap, HashSet},
    fs::File,
    io::{self, Cursor, Read, Seek, SeekFrom},
    path::{Path, PathBuf},
};
use vfs::{FileSystem, SeekAndWrite, VfsFileType, VfsMetadata, VfsResult, error::VfsErrorKind};

#[derive(Clone, Copy, Debug)]
struct Extent {
    start: u64,
    len: u64,
}
struct Slice {
    file: File,
    extent: Extent,
    position: u64,
}
impl Slice {
    fn new(source: &Path, extent: Extent) -> io::Result<Self> {
        let mut file = File::open(source)?;
        let size = file.metadata()?.len();
        if extent.start > size || extent.len > size - extent.start {
            return Err(io::Error::other("ISO extent exceeds source"));
        }
        file.seek(SeekFrom::Start(extent.start))?;
        Ok(Self {
            file,
            extent,
            position: 0,
        })
    }
}
impl Read for Slice {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        let len = buffer
            .len()
            .min(self.extent.len.saturating_sub(self.position) as usize);
        let count = self.file.read(&mut buffer[..len])?;
        self.position += count as u64;
        Ok(count)
    }
}
impl Seek for Slice {
    fn seek(&mut self, from: SeekFrom) -> io::Result<u64> {
        let target = match from {
            SeekFrom::Start(n) => i128::from(n),
            SeekFrom::Current(n) => i128::from(self.position) + i128::from(n),
            SeekFrom::End(n) => i128::from(self.extent.len) + i128::from(n),
        };
        if target < 0 || target > i128::from(self.extent.len) {
            return Err(io::Error::other("seek outside ISO file extent"));
        }
        self.file
            .seek(SeekFrom::Start(self.extent.start + target as u64))?;
        self.position = target as u64;
        Ok(self.position)
    }
}
#[derive(Debug)]
enum Member {
    File(Extent),
    Zip {
        archive: Extent,
        index: usize,
        size: u64,
    },
}
#[derive(Debug)]
pub(crate) struct IsoMedia {
    source: PathBuf,
    members: HashMap<String, Member>,
    directories: HashSet<String>,
}
impl IsoMedia {
    pub(crate) fn new(path: &Path) -> Result<Self> {
        let source = path.canonicalize().map_err(|e| e.to_string())?;
        let mut file = File::open(&source).map_err(|e| e.to_string())?;
        // Bound the upstream volume-descriptor scan, before opening the parser.
        let mut terminated = false;
        for block in 16..272 {
            file.seek(SeekFrom::Start(block * 2048))
                .map_err(|e| e.to_string())?;
            let mut h = [0; 7];
            file.read_exact(&mut h).map_err(|e| e.to_string())?;
            if &h[1..6] != b"CD001" || h[6] != 1 {
                return Err("not a supported ISO9660 image".into());
            }
            if h[0] == 255 {
                terminated = true;
                break;
            }
        }
        if !terminated {
            return Err("ISO descriptor count exceeds budget".into());
        }
        let iso = ISO9660::new(file).map_err(|e| e.to_string())?;
        let mut members = HashMap::new();
        let mut visited = HashSet::new();
        fn walk(
            dir: ISODirectory<File>,
            prefix: &str,
            depth: u32,
            visited: &mut HashSet<(u32, u32)>,
            out: &mut HashMap<String, Member>,
        ) -> Result<()> {
            if depth > 32 || out.len() > 100_000 {
                return Err("ISO directory inventory exceeds budget".into());
            }
            for entry in dir.contents() {
                let entry = entry.map_err(|e| e.to_string())?;
                if matches!(entry.identifier(), "." | "..") {
                    continue;
                }
                let name = format!("{prefix}{}", entry.identifier());
                let key = format!("/{}", virtual_path(&name)?);
                let h = entry.header();
                if h.file_unit_size != 0
                    || h.interleave_gap_size != 0
                    || h.file_flags.bits() & 0x80 != 0
                    || h.extended_attribute_record_length != 0
                {
                    return Err(
                        "interleaved/multi-extent/extended-attribute ISO entries unsupported"
                            .into(),
                    );
                }
                let extent = Extent {
                    start: u64::from(h.extent_loc) * 2048,
                    len: u64::from(h.extent_length),
                };
                let identity = (h.extent_loc, h.extent_length);
                match entry {
                    DirectoryEntry::File(_) => {
                        if out.insert(key, Member::File(extent)).is_some() {
                            return Err("case-ambiguous ISO path".into());
                        }
                    }
                    DirectoryEntry::Directory(child) => {
                        if !visited.insert(identity) {
                            return Err("cyclic/shared ISO directory extent unsupported".into());
                        }
                        walk(child, &format!("{name}/"), depth + 1, visited, out)?;
                    }
                }
            }
            Ok(())
        }
        walk(iso.root, "", 0, &mut visited, &mut members)?;
        // An explicit ISO COMPRESS.ZIP payload is a virtual source layer, with
        // its archived game files taking precedence over same-named disc files.
        let archives: Vec<_> = members
            .iter()
            .filter_map(|(name, member)| match member {
                Member::File(extent) if name.ends_with("/compress.zip") => Some(*extent),
                _ => None,
            })
            .collect();
        let mut archive_paths = HashSet::new();
        for extent in archives {
            let reader = Slice::new(&source, extent).map_err(|e| e.to_string())?;
            let zip = zip::ZipArchive::new(reader).map_err(|e| e.to_string())?;
            if zip.len() > 100_000 {
                return Err("ZIP entry count exceeds budget".into());
            }
            for index in 0..zip.len() {
                let entry = zip.by_index_data(index).map_err(|e| e.to_string())?;
                if entry.is_dir() {
                    continue;
                }
                if entry.encrypted() || entry.is_symlink() {
                    return Err("encrypted/symlink ZIP members unsupported".into());
                }
                let name = entry.name().map_err(|e| e.to_string())?;
                let key = format!("/{}", virtual_path(&name)?);
                if !archive_paths.insert(key.clone()) {
                    return Err("case-ambiguous ZIP member".into());
                }
                members.insert(
                    key,
                    Member::Zip {
                        archive: extent,
                        index,
                        size: entry.size(),
                    },
                );
            }
        }
        let mut directories = HashSet::from([String::new()]);
        for path in members.keys() {
            let mut prefix = path.as_str();
            while let Some((parent, _)) = prefix.rsplit_once('/') {
                directories.insert(parent.into());
                prefix = parent;
            }
        }
        if directories.iter().any(|p| members.contains_key(p)) {
            return Err("ISO/ZIP file-directory collision".into());
        }
        Ok(Self {
            source,
            members,
            directories,
        })
    }
}
fn fail(error: impl std::fmt::Display) -> vfs::VfsError {
    VfsErrorKind::Other(error.to_string()).into()
}
impl FileSystem for IsoMedia {
    fn read_dir(&self, path: &str) -> VfsResult<Box<dyn Iterator<Item = String> + Send>> {
        if !self.directories.contains(path) {
            return Err(VfsErrorKind::FileNotFound.into());
        }
        let prefix = format!("{path}/");
        let entries: HashSet<_> = self
            .members
            .keys()
            .filter_map(|key| {
                key.strip_prefix(&prefix)
                    .and_then(|p| p.split('/').next())
                    .map(str::to_owned)
            })
            .collect();
        Ok(Box::new(entries.into_iter()))
    }
    fn open_file(&self, path: &str) -> VfsResult<AssetStream> {
        let member = self.members.get(path).ok_or(VfsErrorKind::FileNotFound)?;
        match member {
            Member::File(extent) => Ok(Box::new(Slice::new(&self.source, *extent)?)),
            Member::Zip {
                archive,
                index,
                size,
            } => {
                if *size > 256 * 1024 * 1024 {
                    return Err(fail("ZIP asset exceeds 256 MiB budget"));
                }
                let mut zip =
                    zip::ZipArchive::new(Slice::new(&self.source, *archive)?).map_err(fail)?;
                let mut entry = zip.by_index(*index).map_err(fail)?;
                let mut data = Vec::new();
                (&mut entry).take(size + 1).read_to_end(&mut data)?;
                if data.len() as u64 != *size {
                    return Err(fail("ZIP asset size mismatch"));
                }
                // Read to EOF to require the upstream decompressor's CRC check.
                if entry.read(&mut [0; 1])? != 0 {
                    return Err(fail("ZIP member exceeds declared size"));
                }
                Ok(Box::new(Cursor::new(data)))
            }
        }
    }
    fn metadata(&self, path: &str) -> VfsResult<VfsMetadata> {
        let (file_type, len) = if self.directories.contains(path) {
            (VfsFileType::Directory, 0)
        } else {
            let member = self.members.get(path).ok_or(VfsErrorKind::FileNotFound)?;
            (
                VfsFileType::File,
                match member {
                    Member::File(e) => e.len,
                    Member::Zip { size, .. } => *size,
                },
            )
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
