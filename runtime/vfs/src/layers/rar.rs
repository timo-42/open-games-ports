//! Source-backed RAR/SFX access through `rars`; no installer code is executed.
//! Solid members replay required predecessors into a sink. Only the selected
//! member is retained; decoder output, metadata and workspace have finite caps.
#![forbid(unsafe_code)]

use super::{FileStream, Result, virtual_path};
use rars::{ArchiveReadOptions, ArchiveReader};
use std::collections::BTreeSet;
use std::io::Cursor;
use std::path::Path;

const MAX_MEMBER: u64 = 256 * 1024 * 1024;
const MAX_REPLAY: u64 = 512 * 1024 * 1024;
const MAX_WORKSPACE: u64 = 128 * 1024 * 1024;
const MAX_HEADERS: u64 = 20_000;
const MAX_HEADER_BYTES: u64 = 64 * 1024 * 1024;

#[derive(Clone, Debug)]
pub(super) struct Entry {
    pub path: String,
    pub index: usize,
    pub bytes: u64,
}

pub(super) struct Archive {
    archive: rars::Archive,
    entries: Vec<Entry>,
}

impl std::fmt::Debug for Archive {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter
            .debug_struct("RarArchive")
            .field("entries", &self.entries.len())
            .finish()
    }
}

fn options() -> ArchiveReadOptions<'static> {
    ArchiveReadOptions::new()
        .with_max_header_count(MAX_HEADERS)
        .with_max_header_bytes(MAX_HEADER_BYTES)
        .with_max_member_output_bytes(MAX_MEMBER)
        .with_max_total_output_bytes(MAX_REPLAY)
        .with_max_reader_workspace_bytes(MAX_WORKSPACE)
}

impl Archive {
    pub(super) fn probe(path: &Path) -> Result<Option<Self>> {
        let file = std::fs::File::open(path).map_err(|e| format!("Open RAR source: {e}"))?;
        Self::from_stream(Box::new(file))
    }

    fn from_stream(stream: FileStream) -> Result<Option<Self>> {
        let archive = match ArchiveReader::read_reader_with_options(stream, options()) {
            Ok(archive) => archive,
            Err(rars::Error::UnsupportedSignature) => return Ok(None),
            Err(error) => return Err(format!("Index RAR source: {error}")),
        };
        let mut entries = Vec::new();
        let mut names = BTreeSet::new();
        for (index, member) in archive.members().enumerate() {
            let meta = member.meta;
            let raw = std::str::from_utf8(&meta.name)
                .map_err(|_| "RAR member names must be UTF-8 or ASCII")?;
            let normalized = raw.replace('\\', "/");
            let name = if meta.is_directory {
                normalized.trim_end_matches('/')
            } else {
                &normalized
            };
            let path = virtual_path(name)?;
            if !names.insert(path.clone()) {
                return Err(format!("Case-ambiguous RAR member: {raw:?}"));
            }
            // RAR4 Unix symlinks store their target in an ordinary payload.
            // Reject nonregular Unix modes and Windows reparse attributes too.
            let unix_kind = meta.file_attr & 0o170_000;
            let unix_link = meta.host_os == Some(3)
                && unix_kind != 0
                && unix_kind != 0o100_000
                && unix_kind != 0o040_000;
            if meta.is_redirection || unix_link || meta.file_attr & 0x400 != 0 {
                return Err(format!(
                    "RAR links and redirections are unsupported: {raw:?}"
                ));
            }
            if meta.is_encrypted || meta.is_split_before || meta.is_split_after {
                return Err(format!(
                    "Encrypted and split-volume RAR members are unsupported: {raw:?}"
                ));
            }
            if meta.unpacked_size > MAX_MEMBER {
                return Err(format!("RAR member exceeds {MAX_MEMBER} bytes: {raw:?}"));
            }
            if !meta.is_directory {
                entries.push(Entry {
                    path,
                    index,
                    bytes: meta.unpacked_size,
                });
            }
        }
        if entries.is_empty() {
            return Err("RAR source contains no regular files".into());
        }
        Ok(Some(Self { archive, entries }))
    }

    pub(super) fn entries(&self) -> &[Entry] {
        &self.entries
    }

    pub(super) fn open_member(&self, index: usize) -> Result<FileStream> {
        let entry = self
            .entries
            .iter()
            .find(|entry| entry.index == index)
            .ok_or("RAR member index is not a regular file")?;
        // Upstream verifies selected member and required solid predecessors;
        // a decode/checksum error never exposes a partial buffer.
        let bytes = self
            .archive
            .read_member_at_with_options(index, options())
            .map_err(|e| format!("Decode RAR member {:?}: {e}", entry.path))?
            .ok_or("RAR member disappeared after indexing")?;
        if bytes.len() as u64 != entry.bytes || bytes.len() as u64 > MAX_MEMBER {
            return Err("RAR member output size differs from indexed size".into());
        }
        Ok(Box::new(Cursor::new(bytes)))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Read, Seek, SeekFrom};

    // Synthetic uncompressed RAR4 fixtures. Production compression and CRC
    // decoding belongs entirely to the upstream crate.
    fn crc(bytes: &[u8]) -> u32 {
        let mut value = !0u32;
        for &byte in bytes {
            value ^= u32::from(byte);
            for _ in 0..8 {
                value = (value >> 1) ^ (0xedb8_8320 & (0u32.wrapping_sub(value & 1)));
            }
        }
        !value
    }

    fn block(kind: u8, flags: u16, fields: &[u8]) -> Vec<u8> {
        let mut bytes = vec![0, 0, kind];
        bytes.extend_from_slice(&flags.to_le_bytes());
        bytes.extend_from_slice(&(7u16 + fields.len() as u16).to_le_bytes());
        bytes.extend_from_slice(fields);
        let checksum = crc(&bytes[2..]) as u16;
        bytes[..2].copy_from_slice(&checksum.to_le_bytes());
        bytes
    }

    fn fixture(files: &[(&str, &[u8])], stub: bool) -> Vec<u8> {
        let mut bytes = if stub {
            b"MZ synthetic SFX stub".to_vec()
        } else {
            Vec::new()
        };
        bytes.extend_from_slice(b"Rar!\x1a\x07\0");
        bytes.extend(block(0x73, 0, &[0; 6]));
        for &(name, payload) in files {
            let mut fields = Vec::new();
            fields.extend_from_slice(&(payload.len() as u32).to_le_bytes());
            fields.extend_from_slice(&(payload.len() as u32).to_le_bytes());
            fields.push(2); // Windows
            fields.extend_from_slice(&crc(payload).to_le_bytes());
            fields.extend_from_slice(&0u32.to_le_bytes());
            fields.extend_from_slice(&[20, 0x30]);
            fields.extend_from_slice(&(name.len() as u16).to_le_bytes());
            fields.extend_from_slice(&0x20u32.to_le_bytes());
            fields.extend_from_slice(name.as_bytes());
            bytes.extend(block(0x74, 0x8000, &fields));
            bytes.extend_from_slice(payload);
        }
        bytes.extend(block(0x7b, 0x4000, &[]));
        bytes
    }

    fn open(bytes: Vec<u8>) -> Result<Archive> {
        Archive::from_stream(Box::new(Cursor::new(bytes)))?.ok_or("Not a RAR fixture".into())
    }

    #[test]
    fn reads_sfx_members_case_insensitively_as_seekable_streams() {
        let archive = open(fixture(
            &[
                ("Ships\\Enterprise.DAT", b"ship"),
                ("planets.bin", b"planet"),
            ],
            true,
        ))
        .unwrap();
        assert_eq!(archive.entries()[0].path, "ships/enterprise.dat");
        let mut stream = archive.open_member(1).unwrap();
        stream.seek(SeekFrom::Start(2)).unwrap();
        let mut tail = String::new();
        stream.read_to_string(&mut tail).unwrap();
        assert_eq!(tail, "anet");
        let mut first = Vec::new();
        archive
            .open_member(0)
            .unwrap()
            .read_to_end(&mut first)
            .unwrap();
        assert_eq!(first, b"ship");
    }

    #[test]
    fn distinguishes_missing_signature_from_corrupt_recognized_archives() {
        assert!(
            Archive::from_stream(Box::new(Cursor::new(b"MZ opaque PE".to_vec())))
                .unwrap()
                .is_none()
        );
        let mut bytes = fixture(&[("file", b"payload")], false);
        bytes[7] ^= 1; // corrupt the main header's stored CRC
        assert!(Archive::from_stream(Box::new(Cursor::new(bytes))).is_err());
    }

    #[test]
    fn rejects_escaping_and_ambiguous_paths() {
        for name in ["../escape", "/root", "a//b", "a/./b", "C:\\file"] {
            assert!(open(fixture(&[(name, b"x")], false)).is_err(), "{name}");
        }
        assert!(open(fixture(&[("File", b"a"), ("file", b"b")], false)).is_err());
    }

    #[test]
    fn rejects_encryption_split_links_and_oversized_outputs() {
        for policy in 0..5 {
            let mut bytes = fixture(&[("asset", b"x")], false);
            let start = 20; // marker plus minimum main header
            let length =
                u16::from_le_bytes(bytes[start + 5..start + 7].try_into().unwrap()) as usize;
            match policy {
                0 => bytes[start + 3] |= 4, // encrypted payload
                1 => bytes[start + 3] |= 1, // continued from another volume
                2 => bytes[start + 28..start + 32].copy_from_slice(&0x400u32.to_le_bytes()),
                3 => {
                    bytes[start + 15] = 3; // Unix symlink stored as file data
                    bytes[start + 28..start + 32].copy_from_slice(&0o120_777u32.to_le_bytes());
                }
                4 => bytes[start + 11..start + 15]
                    .copy_from_slice(&((MAX_MEMBER + 1) as u32).to_le_bytes()),
                _ => unreachable!(),
            }
            let checksum = crc(&bytes[start + 2..start + length]) as u16;
            bytes[start..start + 2].copy_from_slice(&checksum.to_le_bytes());
            assert!(open(bytes).is_err(), "policy {policy}");
        }
    }

    #[test]
    fn archive_can_be_shared_by_safe_vfs_backends() {
        fn assert_send_sync<T: Send + Sync>() {}
        assert_send_sync::<Archive>();
    }

    #[test]
    fn verifies_requested_payload_before_returning_bytes() {
        let mut bytes = fixture(&[("first", b"one"), ("second", b"two")], false);
        let offset = bytes.windows(3).position(|part| part == b"two").unwrap();
        bytes[offset] ^= 1;
        let archive = open(bytes).unwrap();
        assert!(archive.open_member(0).is_ok());
        assert!(archive.open_member(1).is_err());
        assert!(archive.open_member(2).is_err());
    }
}
