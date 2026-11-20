//! Read-only Refractor 2 FlatArchive (RFA) access for Battlefield 1942 and
//! related games. The index stays small and file-backed; compressed contents
//! are decoded one LZO segment at a time when a virtual file is read.
#![forbid(unsafe_code)]

use std::collections::BTreeSet;
use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use vfs::{SeekAndRead, VfsPath};

const V11_MAGIC: &[u8; 28] = b"Refractor2 FlatArchive 1.1  ";
const V10_HEADER_BYTES: u64 = 156;
const V11_HEADER_BYTES: u64 = 36;
const MAX_ENTRIES: u32 = 100_000;
const MAX_INDEX_BYTES: u64 = 64 * 1024 * 1024;
const MAX_PATH_BYTES: u32 = 4096;
const MAX_MEMBER_BYTES: u32 = 256 * 1024 * 1024;
const MAX_SEGMENTS: u32 = 65_536;
const MAX_SEGMENT_BYTES: u32 = 1024 * 1024;
const MAX_COMPRESSED_SEGMENT_BYTES: u32 = 2 * MAX_SEGMENT_BYTES;

#[derive(Clone, Debug)]
pub(crate) struct Entry {
    pub(crate) path: String,
    compressed_size: u32,
    uncompressed_size: u32,
    data_offset: u32,
}

#[derive(Debug)]
pub(crate) struct Archive {
    backing: Backing,
    file_bytes: u64,
    compressed: bool,
    entries: Vec<Entry>,
}

#[derive(Debug)]
enum Backing {
    Physical(PathBuf),
    Virtual(VfsPath),
}

impl Backing {
    fn open(&self) -> Result<Box<dyn SeekAndRead + Send>, String> {
        match self {
            Self::Physical(path) => File::open(path)
                .map(|file| Box::new(file) as Box<dyn SeekAndRead + Send>)
                .map_err(|error| error.to_string()),
            Self::Virtual(path) => path.open_file().map_err(|error| error.to_string()),
        }
    }
}

impl Archive {
    pub(crate) fn open(path: &Path) -> Result<Self, String> {
        let path = path.canonicalize().map_err(|error| error.to_string())?;
        let file_bytes = path.metadata().map_err(|error| error.to_string())?.len();
        Self::open_backing(Backing::Physical(path), file_bytes)
    }

    pub(crate) fn open_vfs(path: VfsPath) -> Result<Self, String> {
        let file_bytes = path.metadata().map_err(|error| error.to_string())?.len;
        Self::open_backing(Backing::Virtual(path), file_bytes)
    }

    fn open_backing(backing: Backing, file_bytes: u64) -> Result<Self, String> {
        let mut file = backing.open()?;
        if file_bytes < V11_HEADER_BYTES {
            return Err("RFA is shorter than its minimum header".into());
        }

        let mut prefix = [0; 28];
        file.read_exact(&mut prefix)
            .map_err(|error| error.to_string())?;
        let v11 = &prefix == V11_MAGIC;
        let (header_bytes, table_offset, compressed) = if v11 {
            let table_offset = u64::from(read_u32(&mut file)?);
            let compressed = read_compression_flag(&mut file)?;
            (V11_HEADER_BYTES, table_offset, compressed)
        } else {
            if file_bytes < V10_HEADER_BYTES {
                return Err("RFA 1.0 is shorter than its header".into());
            }
            file.seek(SeekFrom::Start(0))
                .map_err(|error| error.to_string())?;
            let table_offset = u64::from(read_u32(&mut file)?);
            let compressed = read_compression_flag(&mut file)?;
            file.seek(SeekFrom::Current(148))
                .map_err(|error| error.to_string())?;
            (V10_HEADER_BYTES, table_offset, compressed)
        };
        if table_offset < header_bytes
            || table_offset
                .checked_add(4)
                .is_none_or(|end| end > file_bytes)
        {
            return Err("RFA file table offset is outside the archive".into());
        }

        file.seek(SeekFrom::Start(table_offset))
            .map_err(|error| error.to_string())?;
        let count = read_u32(&mut file)?;
        if count > MAX_ENTRIES {
            return Err(format!("RFA entry count exceeds {MAX_ENTRIES}"));
        }
        let mut entries = Vec::with_capacity(count as usize);
        let mut names = BTreeSet::new();
        for _ in 0..count {
            let name_bytes = read_u32(&mut file)?;
            if name_bytes == 0 || name_bytes > MAX_PATH_BYTES {
                return Err(format!("Invalid RFA path length {name_bytes}"));
            }
            let mut name = vec![0; name_bytes as usize];
            file.read_exact(&mut name)
                .map_err(|error| error.to_string())?;
            let path = String::from_utf8(name).map_err(|_| "RFA path is not UTF-8")?;
            let key = crate::layers::virtual_path(&path)?;
            if !names.insert(key) {
                return Err(format!("Case-ambiguous path in RFA: {path:?}"));
            }
            let compressed_size = read_u32(&mut file)?;
            let uncompressed_size = read_u32(&mut file)?;
            let data_offset = read_u32(&mut file)?;
            // The remaining entry words are format metadata not required for
            // read access. Keep their bytes inside the bounded table.
            file.seek(SeekFrom::Current(12))
                .map_err(|error| error.to_string())?;
            if uncompressed_size > MAX_MEMBER_BYTES {
                return Err(format!("RFA member {path:?} exceeds 256 MiB"));
            }
            if u64::from(data_offset) > file_bytes {
                return Err(format!("RFA member {path:?} starts past end of archive"));
            }
            entries.push(Entry {
                path,
                compressed_size,
                uncompressed_size,
                data_offset,
            });
            let index_bytes = file
                .stream_position()
                .map_err(|error| error.to_string())?
                .saturating_sub(table_offset);
            if index_bytes > MAX_INDEX_BYTES {
                return Err("RFA index exceeds 64 MiB".into());
            }
        }

        Ok(Self {
            backing,
            file_bytes,
            compressed,
            entries,
        })
    }

    pub(crate) fn entries(&self) -> &[Entry] {
        &self.entries
    }

    pub(crate) fn open_file(&self, entry: &Entry) -> Result<AssetStream, String> {
        let mut file = self.backing.open()?;
        let segments = if self.compressed {
            read_segments(&mut file, self.file_bytes, entry)?
        } else {
            let end = u64::from(entry.data_offset)
                .checked_add(u64::from(entry.uncompressed_size))
                .ok_or("RFA member range overflow")?;
            if end > self.file_bytes {
                return Err(format!(
                    "RFA member {:?} exceeds archive bounds",
                    entry.path
                ));
            }
            Vec::new()
        };
        Ok(Box::new(RfaStream {
            file,
            entry: entry.clone(),
            compressed: self.compressed,
            segments,
            position: 0,
            cached_segment: None,
            cached_bytes: Vec::new(),
        }))
    }
}

type AssetStream = Box<dyn vfs::SeekAndRead + Send>;

impl Entry {
    pub(crate) fn bytes(&self) -> u64 {
        u64::from(self.uncompressed_size)
    }
}

fn read_compression_flag(file: &mut impl Read) -> Result<bool, String> {
    match read_u32(file)? {
        0 => Ok(false),
        1 => Ok(true),
        value => Err(format!("Unsupported RFA compression flag {value}")),
    }
}

fn read_u32(file: &mut impl Read) -> Result<u32, String> {
    let mut bytes = [0; 4];
    file.read_exact(&mut bytes)
        .map_err(|error| error.to_string())?;
    Ok(u32::from_le_bytes(bytes))
}

#[derive(Clone, Debug)]
struct Segment {
    file_offset: u64,
    compressed_bytes: u32,
    uncompressed_bytes: u32,
    logical_offset: u64,
}

fn read_segments(
    file: &mut (impl Read + Seek),
    file_bytes: u64,
    entry: &Entry,
) -> Result<Vec<Segment>, String> {
    if entry.uncompressed_size == 0 {
        match entry.compressed_size {
            0 => return Ok(Vec::new()),
            4 => {
                file.seek(SeekFrom::Start(u64::from(entry.data_offset)))
                    .map_err(|error| error.to_string())?;
                if read_u32(file)? != 0 {
                    return Err(format!("Invalid empty RFA member {:?}", entry.path));
                }
            }
            _ => return Err(format!("Invalid empty RFA member {:?}", entry.path)),
        }
        return Ok(Vec::new());
    }
    file.seek(SeekFrom::Start(u64::from(entry.data_offset)))
        .map_err(|error| error.to_string())?;
    let count = read_u32(file)?;
    if count == 0 || count > MAX_SEGMENTS {
        return Err(format!(
            "Invalid segment count {count} for {:?}",
            entry.path
        ));
    }
    let table_bytes = u64::from(count)
        .checked_mul(12)
        .and_then(|bytes| bytes.checked_add(4))
        .ok_or("RFA segment table size overflow")?;
    let data_start = u64::from(entry.data_offset)
        .checked_add(table_bytes)
        .ok_or("RFA segment data offset overflow")?;
    if data_start > file_bytes {
        return Err(format!(
            "RFA segment table for {:?} exceeds archive",
            entry.path
        ));
    }

    let mut segments = Vec::with_capacity(count as usize);
    let mut logical_offset = 0u64;
    let mut compressed_total = table_bytes;
    for _ in 0..count {
        let compressed_bytes = read_u32(file)?;
        let uncompressed_bytes = read_u32(file)?;
        let relative_offset = u64::from(read_u32(file)?);
        if compressed_bytes == 0
            || compressed_bytes > MAX_COMPRESSED_SEGMENT_BYTES
            || uncompressed_bytes == 0
            || uncompressed_bytes > MAX_SEGMENT_BYTES
        {
            return Err(format!("Invalid RFA segment size for {:?}", entry.path));
        }
        let file_offset = data_start
            .checked_add(relative_offset)
            .ok_or("RFA segment offset overflow")?;
        let end = file_offset
            .checked_add(u64::from(compressed_bytes))
            .ok_or("RFA segment range overflow")?;
        if end > file_bytes {
            return Err(format!("RFA segment for {:?} exceeds archive", entry.path));
        }
        compressed_total = compressed_total
            .checked_add(u64::from(compressed_bytes))
            .ok_or("RFA compressed size overflow")?;
        segments.push(Segment {
            file_offset,
            compressed_bytes,
            uncompressed_bytes,
            logical_offset,
        });
        logical_offset = logical_offset
            .checked_add(u64::from(uncompressed_bytes))
            .ok_or("RFA uncompressed size overflow")?;
    }
    if logical_offset != u64::from(entry.uncompressed_size) {
        return Err(format!("RFA segment lengths do not match {:?}", entry.path));
    }
    if compressed_total != u64::from(entry.compressed_size) {
        return Err(format!(
            "RFA compressed length does not match {:?}",
            entry.path
        ));
    }
    Ok(segments)
}

struct RfaStream {
    file: Box<dyn SeekAndRead + Send>,
    entry: Entry,
    compressed: bool,
    segments: Vec<Segment>,
    position: u64,
    cached_segment: Option<usize>,
    cached_bytes: Vec<u8>,
}

impl RfaStream {
    fn load_segment(&mut self, index: usize) -> io::Result<()> {
        let segment = &self.segments[index];
        let mut compressed = vec![0; segment.compressed_bytes as usize];
        self.file.seek(SeekFrom::Start(segment.file_offset))?;
        self.file.read_exact(&mut compressed)?;
        let mut bytes = vec![0; segment.uncompressed_bytes as usize];
        let written = lzo::decompress_into(&compressed, &mut bytes)
            .map_err(|error| io::Error::new(io::ErrorKind::InvalidData, error))?;
        if written != bytes.len() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "RFA LZO output length does not match its segment header",
            ));
        }
        self.cached_segment = Some(index);
        self.cached_bytes = bytes;
        Ok(())
    }
}

impl Read for RfaStream {
    fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        if buffer.is_empty() || self.position >= u64::from(self.entry.uncompressed_size) {
            return Ok(0);
        }
        if !self.compressed {
            let remaining = u64::from(self.entry.uncompressed_size) - self.position;
            let count = buffer
                .len()
                .min(usize::try_from(remaining).unwrap_or(usize::MAX));
            let offset = u64::from(self.entry.data_offset)
                .checked_add(self.position)
                .ok_or_else(|| {
                    io::Error::new(io::ErrorKind::InvalidInput, "RFA offset overflow")
                })?;
            self.file.seek(SeekFrom::Start(offset))?;
            let read = self.file.read(&mut buffer[..count])?;
            self.position += read as u64;
            return Ok(read);
        }

        let mut written = 0usize;
        while written < buffer.len() && self.position < u64::from(self.entry.uncompressed_size) {
            let index = self
                .segments
                .iter()
                .position(|segment| {
                    let end = segment.logical_offset + u64::from(segment.uncompressed_bytes);
                    self.position >= segment.logical_offset && self.position < end
                })
                .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "RFA segment gap"))?;
            if self.cached_segment != Some(index) {
                self.load_segment(index)?;
            }
            let segment = &self.segments[index];
            let within = (self.position - segment.logical_offset) as usize;
            let count = (self.cached_bytes.len() - within).min(buffer.len() - written);
            buffer[written..written + count]
                .copy_from_slice(&self.cached_bytes[within..within + count]);
            written += count;
            self.position += count as u64;
        }
        Ok(written)
    }
}

impl Seek for RfaStream {
    fn seek(&mut self, from: SeekFrom) -> io::Result<u64> {
        let target = match from {
            SeekFrom::Start(value) => i128::from(value),
            SeekFrom::Current(delta) => i128::from(self.position) + i128::from(delta),
            SeekFrom::End(delta) => i128::from(self.entry.uncompressed_size) + i128::from(delta),
        };
        if !(0..=i128::from(self.entry.uncompressed_size)).contains(&target) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "seek outside RFA member",
            ));
        }
        self.position = target as u64;
        Ok(self.position)
    }
}
