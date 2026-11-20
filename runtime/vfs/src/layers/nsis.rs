//! On-demand NSIS 2 non-solid deflate files. `nsis` parses only a bounded
//! prefix containing the PE stub and complete compressed header. File blocks
//! remain in the original executable; installer script instructions never run.
use super::{FileStream, Result, virtual_path};
use ::nsis::{
    NsisInstaller,
    decompress::{CompressionMethod, CompressionMode},
    opcode,
};
use flate2::{Decompress, FlushDecompress, Status};
use sha2::{Digest, Sha256};
use std::collections::BTreeSet;
use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

const HEADER_BUDGET: usize = 4 * 1024 * 1024;
const OUTPUT_BUDGET: u64 = 2 * 1024 * 1024 * 1024;
#[derive(Clone, Debug)]
pub(super) struct Entry {
    pub path: String,
    offset: u64,
    packed: u64,
    compressed: bool,
}
#[derive(Debug)]
pub(super) struct Archive {
    pub entries: Vec<Entry>,
    source: PathBuf,
}

fn resolve_directory(target: &str, current: &str, saved: Option<&str>) -> String {
    for (variable, value) in [("$OUTDIR", Some(current)), ("$_OUTDIR", saved)] {
        if let Some(suffix) = target.strip_prefix(variable)
            && (suffix.is_empty() || suffix.starts_with('/'))
        {
            return value.map_or_else(|| target.to_owned(), |base| format!("{base}{suffix}"));
        }
    }
    target.to_owned()
}

fn decode_deflate(mut input: impl Read, mut emit: impl FnMut(&[u8]) -> Result<()>) -> Result<()> {
    let mut decoder = Decompress::new(false);
    let mut compressed = [0; 65536];
    let mut block = [0; 65536];
    let mut start = 0;
    let mut end = 0;
    let mut eof = false;
    loop {
        if start == end && !eof {
            end = input.read(&mut compressed).map_err(|e| e.to_string())?;
            start = 0;
            eof = end == 0;
        }
        let before_in = decoder.total_in();
        let before_out = decoder.total_out();
        let status = decoder
            .decompress(
                &compressed[start..end],
                &mut block,
                if eof {
                    FlushDecompress::Finish
                } else {
                    FlushDecompress::None
                },
            )
            .map_err(|e| e.to_string())?;
        let consumed = (decoder.total_in() - before_in) as usize;
        let written = (decoder.total_out() - before_out) as usize;
        start += consumed;
        emit(&block[..written])?;
        if status == Status::StreamEnd {
            let mut extra = [0];
            if start != end || input.read(&mut extra).map_err(|e| e.to_string())? != 0 {
                return Err("Trailing NSIS deflate bytes".into());
            }
            return Ok(());
        }
        if consumed == 0 && written == 0 {
            return Err("Truncated NSIS deflate stream".into());
        }
    }
}

impl Archive {
    pub fn probe(path: &Path) -> Result<Option<Self>> {
        let mut input = File::open(path).map_err(|e| e.to_string())?;
        let length = input.metadata().map_err(|e| e.to_string())?.len();
        let mut prefix = Vec::new();
        Read::by_ref(&mut input)
            .take(HEADER_BUDGET as u64)
            .read_to_end(&mut prefix)
            .map_err(|e| e.to_string())?;
        if !prefix
            .windows(16)
            .any(|w| w == b"\xef\xbe\xad\xdeNullsoftInst")
        {
            return Ok(None);
        }
        let magic = prefix
            .windows(16)
            .position(|w| w == b"\xef\xbe\xad\xdeNullsoftInst")
            .ok_or("Missing NSIS signature")?;
        let u32_at = |offset: usize| -> Result<u32> {
            Ok(u32::from_le_bytes(
                prefix
                    .get(offset..offset + 4)
                    .ok_or("Truncated NSIS first header")?
                    .try_into()
                    .map_err(|_| "Invalid NSIS field")?,
            ))
        };
        if u32_at(magic + 16)? as usize > HEADER_BUDGET {
            return Err("NSIS decompressed header exceeds 4 MiB".into());
        }
        let block_size = u32_at(magic + 24)?;
        if block_size & 0x8000_0000 == 0 || (block_size & 0x7fff_ffff) as usize > HEADER_BUDGET {
            return Err(
                "NSIS streaming adapter requires a bounded non-solid compressed header".into(),
            );
        }
        if ::nsis::decompress::detect_compression(
            prefix
                .get(magic + 28..)
                .ok_or("Truncated NSIS header block")?,
        ) != CompressionMethod::Deflate
        {
            return Err("NSIS streaming adapter supports deflate only".into());
        }
        let installer = NsisInstaller::builder(&prefix)
            .max_decompressed_size(HEADER_BUDGET)
            .parse()
            .map_err(|e| format!("NSIS header: {e}"))?;
        if installer.compression_mode() != CompressionMode::NonSolid
            || installer.compression() != CompressionMethod::Deflate
            || installer.version() != ::nsis::NsisVersion::V2
        {
            return Err(
                "Only NSIS 2 non-solid deflate is supported by the streaming adapter".into(),
            );
        }
        if installer.entry_count() > 100_000 {
            return Err("NSIS instruction count exceeds 100,000".into());
        }
        let mut path_bytes = 0usize;
        let mut out_dir = "$INSTDIR".to_owned();
        let mut saved_out_dir = None;
        let saved_index = installer
            .nsis2_sub_version()
            .map_or(31, ::nsis::Nsis2SubVersion::spec_outdir_var_index);
        let mut entries = Vec::new();
        let mut names = BTreeSet::new();
        for instruction in installer.entries() {
            let instruction = instruction.map_err(|e| e.to_string())?;
            match installer.normalize_opcode(instruction.which()) {
                opcode::EW_ASSIGNVAR if instruction.offset(0) == i32::from(saved_index) => {
                    let source = installer
                        .read_string(instruction.offset(1))
                        .map_err(|e| e.to_string())?
                        .to_string();
                    saved_out_dir = (source == "$OUTDIR"
                        && instruction.offset(2) == 0
                        && instruction.offset(3) == 0)
                        .then(|| out_dir.clone());
                }
                opcode::EW_CREATEDIR if instruction.offset(1) != 0 => {
                    let target = installer
                        .read_string(instruction.offset(0))
                        .map_err(|e| e.to_string())?
                        .to_string()
                        .replace('\\', "/");
                    out_dir = resolve_directory(&target, &out_dir, saved_out_dir.as_deref());
                }
                opcode::EW_EXTRACTFILE => {
                    let name = installer
                        .read_string(instruction.offset(1))
                        .map_err(|e| e.to_string())?
                        .to_string()
                        .replace('\\', "/");
                    let name = if name.starts_with('$')
                        || name.starts_with('/')
                        || name.as_bytes().get(1) == Some(&b':')
                    {
                        name
                    } else {
                        format!("{out_dir}/{name}")
                    };
                    let name = name.strip_prefix("$INSTDIR/").unwrap_or(&name);
                    // Unknown script variables, helpers, shell folders and branch
                    // dependent destinations are never guessed or executed.
                    if name.contains('$') {
                        continue;
                    }
                    let path = virtual_path(name)?;
                    path_bytes = path_bytes
                        .checked_add(path.len())
                        .ok_or("NSIS path budget overflow")?;
                    if path_bytes > 16 * 1024 * 1024 {
                        return Err("NSIS paths exceed 16 MiB".into());
                    }
                    if !names.insert(path.clone()) {
                        return Err(format!("Ambiguous NSIS destination: {path}"));
                    }
                    let offset = (installer.data_block_offset() as u64)
                        .checked_add(instruction.offset(2) as u32 as u64)
                        .ok_or("NSIS offset overflow")?;
                    input
                        .seek(SeekFrom::Start(offset))
                        .map_err(|e| e.to_string())?;
                    let mut size = [0; 4];
                    input.read_exact(&mut size).map_err(|e| e.to_string())?;
                    let raw = u32::from_le_bytes(size);
                    let packed = (raw & 0x7fff_ffff) as u64;
                    let offset = offset + 4;
                    if offset.checked_add(packed).is_none_or(|e| e > length) {
                        return Err("NSIS file block outside source".into());
                    }
                    entries.push(Entry {
                        path,
                        offset,
                        packed,
                        compressed: raw & 0x8000_0000 != 0,
                    });
                }
                _ => {}
            }
        }
        if entries.is_empty() {
            return Err("No static NSIS installation destinations".into());
        }
        Ok(Some(Self {
            entries,
            source: path.to_owned(),
        }))
    }
    pub fn open_member(&self, entry: &Entry, identity: &str) -> Result<FileStream> {
        let root = super::inno_zip_cache_paths(
            Path::new(identity),
            entry.offset as usize,
            0,
            "nsis-deflate-v1",
        )?
        .0;
        let directory = root.parent().ok_or("Missing cache directory")?;
        let cached = directory.join("decoded.bin");
        let manifest = directory.join("decoded.sha256");
        if let Ok(size) = fs::metadata(&cached).map(|m| m.len())
            && size <= OUTPUT_BUDGET
            && super::cached_rfa_is_valid(&cached, &manifest, size)
        {
            return Ok(Box::new(File::open(cached).map_err(|e| e.to_string())?));
        }
        let mut input = File::open(&self.source).map_err(|e| e.to_string())?;
        input
            .seek(SeekFrom::Start(entry.offset))
            .map_err(|e| e.to_string())?;
        let input = input.take(entry.packed);
        let mut output = tempfile::NamedTempFile::new_in(directory).map_err(|e| e.to_string())?;
        let mut size = 0u64;
        let mut hash = Sha256::new();
        let mut emit = |data: &[u8]| -> Result<()> {
            size += data.len() as u64;
            if size > OUTPUT_BUDGET {
                return Err("NSIS member exceeds 2 GiB".into());
            }
            output.write_all(data).map_err(|e| e.to_string())?;
            hash.update(data);
            Ok(())
        };
        if entry.compressed {
            decode_deflate(input, &mut emit)?;
        } else {
            let mut input = input;
            let mut block = [0; 65536];
            loop {
                let n = input.read(&mut block).map_err(|e| e.to_string())?;
                if n == 0 {
                    break;
                }
                emit(&block[..n])?;
            }
        }
        output.flush().map_err(|e| e.to_string())?;
        output.persist(&cached).map_err(|e| e.to_string())?;
        fs::write(
            &manifest,
            format!("1 {size} {}\n", super::hex(&hash.finalize())),
        )
        .map_err(|e| e.to_string())?;
        Ok(Box::new(File::open(cached).map_err(|e| e.to_string())?))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn put(bytes: &mut [u8], offset: usize, value: u32) {
        bytes[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
    }
    fn compressed(data: &[u8]) -> Vec<u8> {
        let mut w = flate2::write::DeflateEncoder::new(Vec::new(), flate2::Compression::default());
        w.write_all(data).unwrap();
        w.finish().unwrap()
    }
    fn fixture() -> Vec<u8> {
        let strings = b"\0mods/AIX2\0test.bin\0";
        let mut header = vec![0; 384];
        put(&mut header, 4 + 2 * 8, 300);
        put(&mut header, 8 + 2 * 8, 3);
        put(&mut header, 4 + 3 * 8, 384);
        put(&mut header, 300, opcode::EW_CREATEDIR as u32);
        put(&mut header, 304, 1);
        put(&mut header, 308, 1);
        put(&mut header, 328, opcode::EW_EXTRACTFILE as u32);
        put(&mut header, 336, 11); // string offset
        put(&mut header, 356, opcode::EW_RET as u32);
        header.extend(strings);
        let packed_header = compressed(&header);
        let payload = compressed(b"synthetic mod data");
        let mut exe = vec![0; 1024];
        exe[..2].copy_from_slice(b"MZ");
        put(&mut exe, 60, 128);
        exe[128..132].copy_from_slice(b"PE\0\0");
        exe[132..134].copy_from_slice(&0x14cu16.to_le_bytes());
        exe[134..136].copy_from_slice(&1u16.to_le_bytes());
        exe[148..150].copy_from_slice(&224u16.to_le_bytes());
        exe[152..154].copy_from_slice(&0x10bu16.to_le_bytes());
        put(&mut exe, 152 + 60, 512);
        let section = 152 + 224;
        put(&mut exe, section + 16, 512);
        put(&mut exe, section + 20, 512);
        exe.extend(0u32.to_le_bytes());
        exe.extend(b"\xef\xbe\xad\xdeNullsoftInst");
        exe.extend((header.len() as u32).to_le_bytes());
        exe.extend(0u32.to_le_bytes());
        exe.extend(((packed_header.len() as u32) | 0x80000000).to_le_bytes());
        exe.extend(packed_header);
        exe.extend(((payload.len() as u32) | 0x80000000).to_le_bytes());
        exe.extend(payload);
        let following = (exe.len() - 1024) as u32;
        put(&mut exe, 1024 + 24, following);
        exe
    }
    #[test]
    fn bounded_header_and_on_demand_mod_member() {
        let temp = tempfile::tempdir().unwrap();
        let path = temp.path().join("mod.exe");
        let bytes = fixture();
        fs::write(&path, &bytes).unwrap();
        let layer = super::super::Layer::open(&path, "mod").unwrap();
        assert_eq!(layer.files().unwrap(), ["mods/aix2/test.bin"]);
        let selected = layer
            .select(&[("mods/aix2/test.bin", "mods/aix2/test.bin")], "selected")
            .unwrap();
        let sources = super::super::Sources::new(vec![selected], "nsis-test").unwrap();
        assert_eq!(sources.file_size("mods/aix2/test.bin").unwrap(), 18);
        assert_eq!(
            sources.read("mods/aix2/test.bin").unwrap(),
            b"synthetic mod data"
        );
        let mut huge = bytes;
        put(&mut huge, 1024 + 20, 0x7fffffff);
        fs::write(&path, huge).unwrap();
        assert!(Archive::probe(&path).is_err());
    }
    #[test]
    fn deflate_requires_an_end_marker_and_no_trailing_data() {
        let data = compressed(b"mod data");
        let mut result = Vec::new();
        decode_deflate(data.as_slice(), |b| {
            result.extend_from_slice(b);
            Ok(())
        })
        .unwrap();
        assert_eq!(result, b"mod data");
        for n in 0..data.len() {
            assert!(decode_deflate(&data[..n], |_| Ok(())).is_err());
        }
        let mut extra = data;
        extra.push(0);
        assert!(decode_deflate(extra.as_slice(), |_| Ok(())).is_err());
        assert_eq!(
            resolve_directory("$_OUTDIR/AI", "$TEMP", Some("$INSTDIR/mods/AIX2")),
            "$INSTDIR/mods/AIX2/AI"
        );
        assert_eq!(
            resolve_directory("$OUTDIR/sub", "$INSTDIR/mods/AIX2", None),
            "$INSTDIR/mods/AIX2/sub"
        );
        assert_eq!(
            resolve_directory("$_OUTDIR/sub", "$TEMP", None),
            "$_OUTDIR/sub"
        );
    }
}
