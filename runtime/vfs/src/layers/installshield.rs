//! InstallShield 6–10 ANSI cabinets. Layout reference: twogood/unshield (MIT),
//! lib/{cabfile.h,file.c,directory.c,file_group.c}; no installer code is run.
//! Unlike Microsoft CAB or InstallShield Z, these use an external HDR index.
use super::{FileStream, Result, virtual_path};
use flate2::read::DeflateDecoder;
use md5::{Digest, Md5};
use sha2::Sha256;
use std::collections::{BTreeMap, BTreeSet};
use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

const MAX_HEADER: u64 = 16 * 1024 * 1024;
const MAX_OUTPUT: u64 = 2 * 1024 * 1024 * 1024;

#[derive(Clone, Debug)]
pub(super) struct Entry {
    pub path: String,
    pub bytes: u64,
    packed: u64,
    offset: u64,
    volume: u16,
    flags: u16,
    md5: [u8; 16],
}

#[derive(Debug)]
pub(super) struct Archive {
    pub entries: Vec<Entry>,
    pub sources: Vec<PathBuf>,
    volumes: BTreeMap<u16, Span>,
}

#[derive(Debug)]
struct Span {
    path: PathBuf,
    offset: u64,
    bytes: u64,
}

fn field<const N: usize>(data: &[u8], offset: usize) -> Result<[u8; N]> {
    data.get(offset..offset.checked_add(N).ok_or("CAB offset overflow")?)
        .ok_or("Truncated InstallShield index")?
        .try_into()
        .map_err(|_| "Invalid CAB field".into())
}
fn u32_at(data: &[u8], offset: usize) -> Result<usize> {
    Ok(u32::from_le_bytes(field(data, offset)?) as usize)
}
fn text(data: &[u8], offset: usize) -> Result<String> {
    let tail = data
        .get(offset..)
        .ok_or("CAB string offset outside index")?;
    let end = tail
        .iter()
        .take(4097)
        .position(|b| *b == 0)
        .ok_or("Unterminated CAB name")?;
    // Explicit ASCII subset: never silently corrupt an ANSI destination path.
    if !tail[..end].is_ascii() {
        return Err("Non-ASCII InstallShield names unsupported".into());
    }
    Ok(String::from_utf8_lossy(&tail[..end]).into_owned())
}

impl Archive {
    pub fn open(path: &Path) -> Result<Self> {
        if let Some(records) = super::setup::records(path)? {
            if records.iter().any(|r| r.path.ends_with(".rtp")) {
                return Err("InstallShield package contains RTPatch deltas; RTPatch 3.20 reconstruction is unsupported (not a replacement layer)".into());
            }
            let header = records
                .iter()
                .find(|r| r.path == "disk1/data1.hdr")
                .ok_or("Setup package has no data1.hdr")?;
            if header.bytes > MAX_HEADER {
                return Err("InstallShield HDR exceeds 16 MiB".into());
            }
            let mut file = File::open(path).map_err(|e| e.to_string())?;
            file.seek(SeekFrom::Start(header.offset))
                .map_err(|e| e.to_string())?;
            let mut data = Vec::new();
            file.take(header.bytes)
                .read_to_end(&mut data)
                .map_err(|e| e.to_string())?;
            let entries = Self::index(&data)?;
            let mut volumes = BTreeMap::new();
            for entry in &entries {
                let name = format!("disk1/data{}.cab", entry.volume);
                let record = records
                    .iter()
                    .find(|r| r.path == name)
                    .ok_or_else(|| format!("Missing embedded {name}"))?;
                volumes.insert(
                    entry.volume,
                    Span {
                        path: path.to_owned(),
                        offset: record.offset,
                        bytes: record.bytes,
                    },
                );
            }
            return Ok(Self {
                entries,
                sources: vec![path.to_owned()],
                volumes,
            });
        }
        let directory = path.parent().ok_or("CAB has no directory")?;
        let mut companions = BTreeMap::new();
        for item in fs::read_dir(directory).map_err(|e| e.to_string())? {
            let item = item.map_err(|e| e.to_string())?;
            let name = item.file_name().to_string_lossy().to_ascii_lowercase();
            if name == "data1.hdr" || (name.starts_with("data") && name.ends_with(".cab")) {
                if item.file_type().map_err(|e| e.to_string())?.is_symlink() {
                    return Err("InstallShield companion symlinks unsupported".into());
                }
                if companions.insert(name, item.path()).is_some() {
                    return Err("Ambiguous CAB companions".into());
                }
            }
        }
        let hdr = companions
            .get("data1.hdr")
            .ok_or("InstallShield requires data1.hdr")?;
        let mut data = Vec::new();
        File::open(hdr)
            .map_err(|e| e.to_string())?
            .take(MAX_HEADER + 1)
            .read_to_end(&mut data)
            .map_err(|e| e.to_string())?;
        if data.len() as u64 > MAX_HEADER {
            return Err("InstallShield HDR exceeds 16 MiB".into());
        }
        let entries = Self::index(&data)?;
        let mut volumes = BTreeMap::new();
        for entry in &entries {
            let name = format!("data{}.cab", entry.volume);
            let path = companions
                .get(&name)
                .ok_or_else(|| format!("Missing CAB volume {name}"))?;
            volumes.insert(
                entry.volume,
                Span {
                    path: path.clone(),
                    offset: 0,
                    bytes: path.metadata().map_err(|e| e.to_string())?.len(),
                },
            );
        }
        let mut sources = vec![hdr.clone()];
        sources.extend(volumes.values().map(|s| s.path.clone()));
        Ok(Self {
            entries,
            sources,
            volumes,
        })
    }

    fn index(data: &[u8]) -> Result<Vec<Entry>> {
        if field::<4>(data, 0)? != *b"ISc(" || u32_at(data, 4)? != 0x01009500 {
            return Err("Unsupported InstallShield cabinet version (requires ANSI 10.5)".into());
        }
        let cab = u32_at(data, 12)?;
        let table = cab
            .checked_add(u32_at(data, cab + 12)?)
            .ok_or("CAB table overflow")?;
        let directories = u32_at(data, cab + 28)?;
        let count = u32_at(data, cab + 40)?;
        if count > 100_000 || directories > 100_000 {
            return Err("CAB index budget exceeded".into());
        }
        let descriptors = table
            .checked_add(u32_at(data, cab + 44)?)
            .ok_or("CAB descriptor overflow")?;
        let mut groups = vec![None; count];
        let mut visited = BTreeSet::new();
        for bucket in 0..71 {
            let mut offset = u32_at(data, cab + 0x3e + bucket * 4)?;
            while offset != 0 {
                if !visited.insert(offset) || visited.len() > 100_000 {
                    return Err("Cyclic/oversized CAB groups".into());
                }
                let node = cab.checked_add(offset).ok_or("CAB group overflow")?;
                let desc = cab
                    .checked_add(u32_at(data, node + 4)?)
                    .ok_or("CAB group overflow")?;
                let name = text(
                    data,
                    cab.checked_add(u32_at(data, desc)?)
                        .ok_or("CAB name overflow")?,
                )?;
                let first = u32_at(data, desc + 22)?;
                let last = u32_at(data, desc + 26)?;
                if first != u32::MAX as usize {
                    if last < first || last >= count {
                        return Err("Invalid CAB group range".into());
                    }
                    for group in &mut groups[first..=last] {
                        if group.replace(name.clone()).is_some() {
                            return Err("Overlapping CAB groups".into());
                        }
                    }
                }
                offset = u32_at(data, node + 8)?;
            }
        }
        let mut entries = Vec::new();
        let mut names = BTreeSet::new();
        let mut path_bytes = 0usize;
        for (i, group) in groups.into_iter().enumerate() {
            let o = descriptors
                .checked_add(i * 87)
                .ok_or("CAB descriptor overflow")?;
            let flags = u16::from_le_bytes(field(data, o)?);
            let bytes = u64::from_le_bytes(field(data, o + 2)?);
            let packed = u64::from_le_bytes(field(data, o + 10)?);
            let offset = u64::from_le_bytes(field(data, o + 18)?);
            if flags & 8 != 0
                || offset == 0
                || (bytes == 0 && field::<16>(data, o + 26)? == [0; 16])
            {
                continue;
            }
            if flags & !4 != 0 {
                return Err(format!("Unsupported InstallShield flags {flags:#x}"));
            }
            if bytes > MAX_OUTPUT || packed > MAX_OUTPUT {
                return Err("CAB member exceeds 2 GiB".into());
            }
            let dir = u16::from_le_bytes(field(data, o + 62)?) as usize;
            if dir >= directories {
                return Err("CAB directory index outside table".into());
            }
            let directory = text(
                data,
                table
                    .checked_add(u32_at(data, table + dir * 4)?)
                    .ok_or("CAB name overflow")?,
            )?;
            let name = text(
                data,
                table
                    .checked_add(u32_at(data, o + 58)?)
                    .ok_or("CAB name overflow")?,
            )?;
            let group = group.ok_or("CAB payload has no file group")?;
            // Group names are metadata, never installation destination guesses.
            let path = virtual_path(&format!(
                "groups/{group}/{}{name}",
                if directory.is_empty() {
                    String::new()
                } else {
                    format!("{directory}/")
                }
            ))?;
            path_bytes = path_bytes
                .checked_add(path.len())
                .ok_or("CAB path budget overflow")?;
            if path_bytes > 16 * 1024 * 1024 {
                return Err("CAB paths exceed 16 MiB".into());
            }
            if !names.insert(path.clone()) {
                return Err(format!("Ambiguous CAB member {path}"));
            }
            entries.push(Entry {
                path,
                bytes,
                packed,
                offset,
                flags,
                volume: u16::from_le_bytes(field(data, o + 85)?),
                md5: field(data, o + 26)?,
            });
        }
        Ok(entries)
    }

    pub fn open_member(&self, entry: &Entry, identity: &str) -> Result<FileStream> {
        let (cached, manifest) = super::inno_zip_cache_paths(
            Path::new(identity),
            0,
            entry.bytes,
            &super::hex(&entry.md5),
        )?;
        if super::cached_rfa_is_valid(&cached, &manifest, entry.bytes) {
            return Ok(Box::new(File::open(cached).map_err(|e| e.to_string())?));
        }
        let span = self
            .volumes
            .get(&entry.volume)
            .ok_or("Missing CAB volume")?;
        let path = &span.path;
        let mut input = File::open(path).map_err(|e| e.to_string())?;
        if entry
            .offset
            .checked_add(entry.packed)
            .is_none_or(|end| end > span.bytes)
        {
            return Err("CAB member range outside volume (split files unsupported)".into());
        }
        input
            .seek(SeekFrom::Start(
                span.offset
                    .checked_add(entry.offset)
                    .ok_or("CAB offset overflow")?,
            ))
            .map_err(|e| e.to_string())?;
        let mut input = input.take(entry.packed);
        let mut output =
            tempfile::NamedTempFile::new_in(cached.parent().ok_or("Missing cache directory")?)
                .map_err(|e| e.to_string())?;
        let mut hash = Md5::new();
        let mut cache_hash = Sha256::new();
        let mut written = 0u64;
        let mut emit = |data: &[u8]| -> Result<()> {
            written = written
                .checked_add(data.len() as u64)
                .ok_or("CAB size overflow")?;
            if written > entry.bytes {
                return Err("CAB output exceeds declared size".into());
            }
            hash.update(data);
            cache_hash.update(data);
            output.write_all(data).map_err(|e| e.to_string())
        };
        let mut block = [0; 65536];
        if entry.flags & 4 != 0 {
            while input.limit() != 0 {
                let mut size = [0; 2];
                input.read_exact(&mut size).map_err(|e| e.to_string())?;
                let size = u16::from_le_bytes(size) as usize;
                if size == 0 || size as u64 > input.limit() {
                    return Err("Invalid CAB deflate block size".into());
                }
                let mut compressed = vec![0; size];
                input
                    .read_exact(&mut compressed)
                    .map_err(|e| e.to_string())?;
                let mut decoder = DeflateDecoder::new(compressed.as_slice());
                let mut chunk = Vec::new();
                decoder
                    .by_ref()
                    .take(65537)
                    .read_to_end(&mut chunk)
                    .map_err(|e| e.to_string())?;
                if chunk.len() > 65536 {
                    return Err("CAB deflate block exceeds 64 KiB".into());
                }
                emit(&chunk)?;
            }
        } else {
            loop {
                let n = input.read(&mut block).map_err(|e| e.to_string())?;
                if n == 0 {
                    break;
                }
                emit(&block[..n])?;
            }
        }
        if written != entry.bytes || hash.finalize().as_slice() != entry.md5 {
            return Err(format!("InstallShield size/MD5 mismatch: {}", entry.path));
        }
        output.flush().map_err(|e| e.to_string())?;
        output.persist(&cached).map_err(|e| e.to_string())?;
        fs::write(
            &manifest,
            format!("1 {written} {}\n", super::hex(&cache_hash.finalize())),
        )
        .map_err(|e| e.to_string())?;
        Ok(Box::new(File::open(cached).map_err(|e| e.to_string())?))
    }
}

#[cfg(test)]
#[path = "installshield_tests.rs"]
mod tests;
