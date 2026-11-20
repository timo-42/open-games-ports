//! EA BIGF/BIG4 indexes retaining the original names, ranges and size warnings.
//!
//! Physical bounds govern reads when files exclude final alignment bytes from
//! the declared size. Parsing preserves duplicate names;
//! case-insensitive member lookup rejects ambiguity instead of choosing one.
use crate::{MAX_DECOMPRESSED_BYTES, MAX_FILE_BYTES, normalize, refpack};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry {
    pub name: String,
    pub offset: usize,
    pub size: usize,
}

#[derive(Debug, Clone)]
pub struct Archive {
    pub magic: String,
    pub declared_size: usize,
    pub directory_end: usize,
    pub entries: Vec<Entry>,
    pub warnings: Vec<String>,
    physical_size: usize,
}

impl Archive {
    pub fn parse(data: &[u8]) -> Result<Self, String> {
        if data.len() < 16 || !matches!(&data[..4], b"BIGF" | b"BIG4") {
            return Err("Expected an EA BIGF/BIG4 archive".to_owned());
        }
        if data.len() > MAX_FILE_BYTES {
            return Err(format!("BIG archive exceeds {MAX_FILE_BYTES} bytes"));
        }
        let size_bytes: [u8; 4] = data[4..8].try_into().map_err(|_| "Truncated BIG size")?;
        let declared_size = if &data[..4] == b"BIG4" {
            u32::from_le_bytes(size_bytes)
        } else {
            u32::from_be_bytes(size_bytes)
        } as usize;
        let count = word(data, 8)?;
        let directory_end = word(data, 12)?;
        if !(16..=data.len()).contains(&directory_end) {
            return Err("BIG directory end is outside the input".to_owned());
        }
        if count > 100_000 || count > (directory_end - 16) / 9 {
            return Err("BIG entry count cannot fit in the directory".to_owned());
        }
        let mut entries = Vec::with_capacity(count);
        let mut cursor = 16usize;
        let mut name_bytes = 0usize;
        for _ in 0..count {
            if cursor.checked_add(9).is_none_or(|end| end > directory_end) {
                return Err("Truncated BIG directory entry".to_owned());
            }
            let offset = word(data, cursor)?;
            let size = word(data, cursor + 4)?;
            let tail = &data[cursor + 8..directory_end];
            let length = tail
                .iter()
                .position(|byte| *byte == 0)
                .ok_or("Unterminated BIG entry name")?;
            if length == 0 || length > 4096 {
                return Err("BIG entry name must contain 1..4096 bytes".to_owned());
            }
            name_bytes = name_bytes
                .checked_add(length)
                .ok_or("BIG name budget overflow")?;
            if name_bytes > 16 * 1024 * 1024 {
                return Err("BIG entry names exceed 16 MiB".to_owned());
            }
            // The Python evidence reader uses Latin-1. Preserve every byte.
            let name: String = tail[..length].iter().copied().map(char::from).collect();
            if offset < directory_end || offset.checked_add(size).is_none_or(|end| end > data.len())
            {
                return Err(format!("BIG member {name:?} is outside its payload"));
            }
            entries.push(Entry { name, offset, size });
            cursor += 9 + length;
        }
        let mut ranges: Vec<_> = entries
            .iter()
            .filter(|entry| entry.size != 0)
            .map(|entry| (entry.offset, entry.offset + entry.size))
            .collect();
        ranges.sort_unstable();
        for pair in ranges.windows(2) {
            if pair[1].0 < pair[0].1 && pair[1] != pair[0] {
                return Err("BIG member byte ranges overlap".to_owned());
            }
        }
        let warnings = if declared_size == data.len() {
            Vec::new()
        } else {
            vec![format!(
                "Header declares {declared_size} bytes; actual input is {} bytes",
                data.len()
            )]
        };
        Ok(Self {
            magic: String::from_utf8_lossy(&data[..4]).into_owned(),
            declared_size,
            directory_end,
            entries,
            warnings,
            physical_size: data.len(),
        })
    }

    pub fn read(&self, data: &[u8], member: &str) -> Result<Vec<u8>, String> {
        self.read_with_limit(data, member, MAX_DECOMPRESSED_BYTES)
    }

    pub fn read_with_limit(
        &self,
        data: &[u8],
        member: &str,
        max_output: usize,
    ) -> Result<Vec<u8>, String> {
        if data.len() != self.physical_size {
            return Err("BIG data length differs from the parsed archive".to_owned());
        }
        let requested = normalize(member)?;
        let mut found = None;
        for entry in &self.entries {
            if normalize(&entry.name).is_ok_and(|name| name == requested) {
                if found.is_some() {
                    return Err(format!("Ambiguous case-insensitive BIG member: {member:?}"));
                }
                found = Some(entry);
            }
        }
        let entry = found.ok_or_else(|| format!("BIG member was not found: {member:?}"))?;
        let bytes = data
            .get(
                entry.offset
                    ..entry
                        .offset
                        .checked_add(entry.size)
                        .ok_or("BIG member range overflow")?,
            )
            .ok_or("BIG member is outside its payload")?;
        if refpack::is_compressed(bytes) {
            refpack::decompress(bytes, max_output)
        } else if bytes.len() > max_output {
            Err(format!("BIG member exceeds {max_output} bytes"))
        } else {
            Ok(bytes.to_vec())
        }
    }
}

fn word(data: &[u8], offset: usize) -> Result<usize, String> {
    let bytes = data
        .get(offset..offset.checked_add(4).ok_or("BIG integer offset overflow")?)
        .ok_or("Truncated BIG integer")?;
    Ok(u32::from_be_bytes(bytes.try_into().map_err(|_| "Truncated BIG integer")?) as usize)
}
