//! Old InstallShield Setup Player's uncompressed, four-NUL-string records.
//! Indexes PE overlay ranges with seeks; never loads or executes the wrapper.
use super::Result;
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;

#[derive(Clone, Debug)]
pub struct Record {
    pub path: String,
    pub offset: u64,
    pub bytes: u64,
}
fn string(file: &mut File) -> Result<String> {
    let mut bytes = Vec::new();
    for _ in 0..4096 {
        let mut byte = [0];
        file.read_exact(&mut byte).map_err(|e| e.to_string())?;
        if byte[0] == 0 {
            return String::from_utf8(bytes).map_err(|e| e.to_string());
        }
        bytes.push(byte[0]);
    }
    Err("InstallShield wrapper string exceeds 4096 bytes".into())
}
pub fn records(path: &Path) -> Result<Option<Vec<Record>>> {
    let mut file = File::open(path).map_err(|e| e.to_string())?;
    let length = file.metadata().map_err(|e| e.to_string())?.len();
    let mut dos = [0; 64];
    if file.read_exact(&mut dos).is_err() || &dos[..2] != b"MZ" {
        return Ok(None);
    }
    let pe = u32::from_le_bytes(dos[60..64].try_into().unwrap()) as u64;
    if pe > 1024 * 1024 {
        return Ok(None);
    }
    file.seek(SeekFrom::Start(pe)).map_err(|e| e.to_string())?;
    let mut coff = [0; 24];
    file.read_exact(&mut coff).map_err(|e| e.to_string())?;
    if &coff[..4] != b"PE\0\0" {
        return Ok(None);
    }
    let count = u16::from_le_bytes(coff[6..8].try_into().unwrap());
    let optional = u16::from_le_bytes(coff[20..22].try_into().unwrap());
    if count > 96 {
        return Err("PE section budget exceeded".into());
    }
    file.seek(SeekFrom::Current(optional as i64))
        .map_err(|e| e.to_string())?;
    let mut overlay = 0u64;
    for _ in 0..count {
        let mut section = [0; 40];
        file.read_exact(&mut section).map_err(|e| e.to_string())?;
        let bytes = u32::from_le_bytes(section[16..20].try_into().unwrap()) as u64;
        let offset = u32::from_le_bytes(section[20..24].try_into().unwrap()) as u64;
        overlay = overlay.max(offset + bytes);
    }
    if overlay == 0 || overlay >= length {
        return Ok(None);
    }
    file.seek(SeekFrom::Start(overlay))
        .map_err(|e| e.to_string())?;
    // A strict signature probe avoids treating arbitrary PE overlays as records.
    let first = match string(&mut file) {
        Ok(s) => s,
        Err(_) => return Ok(None),
    };
    if !["data1.cab", "PATCH.RTD"]
        .iter()
        .any(|n| first.eq_ignore_ascii_case(n))
    {
        return Ok(None);
    }
    file.seek(SeekFrom::Start(overlay))
        .map_err(|e| e.to_string())?;
    let mut records = Vec::new();
    let mut names = std::collections::BTreeSet::new();
    while file.stream_position().map_err(|e| e.to_string())? < length {
        if records.len() >= 100_000 {
            return Err("Setup record budget exceeded".into());
        }
        let name = string(&mut file)?;
        let path = super::virtual_path(&string(&mut file)?)?;
        let _version = string(&mut file)?;
        let bytes = string(&mut file)?
            .parse::<u64>()
            .map_err(|_| "Invalid setup record size")?;
        if !path
            .rsplit('/')
            .next()
            .is_some_and(|s| s.eq_ignore_ascii_case(&name))
            || !names.insert(path.clone())
        {
            return Err("Invalid/duplicate setup record name".into());
        }
        let offset = file.stream_position().map_err(|e| e.to_string())?;
        let end = offset
            .checked_add(bytes)
            .filter(|end| *end <= length)
            .ok_or("Setup record outside executable")?;
        records.push(Record {
            path,
            offset,
            bytes,
        });
        file.seek(SeekFrom::Start(end)).map_err(|e| e.to_string())?;
    }
    Ok(Some(records))
}

#[cfg(test)]
mod tests {
    use super::*;
    fn wrapper(records: &[(&str, &[u8])]) -> Vec<u8> {
        let mut bytes = vec![0; 512];
        bytes[..2].copy_from_slice(b"MZ");
        bytes[60..64].copy_from_slice(&64u32.to_le_bytes());
        bytes[64..68].copy_from_slice(b"PE\0\0");
        bytes[70..72].copy_from_slice(&1u16.to_le_bytes());
        bytes[104..108].copy_from_slice(&512u32.to_le_bytes());
        for (name, data) in records {
            bytes.extend(format!("{name}\0Disk1\\{name}\x000.0.0.0\0{}\0", data.len()).as_bytes());
            bytes.extend_from_slice(data);
        }
        bytes
    }
    #[test]
    fn setup_records_use_ranges_and_reject_malformed_lengths_and_paths() {
        let temp = tempfile::tempdir().unwrap();
        let path = temp.path().join("patch.exe");
        let data = wrapper(&[("data1.cab", b"cab"), ("data1.hdr", b"hdr")]);
        std::fs::write(&path, &data).unwrap();
        let entries = records(&path).unwrap().unwrap();
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[1].path, "disk1/data1.hdr");
        assert_eq!(
            &data[entries[0].offset as usize..entries[0].offset as usize + 3],
            b"cab"
        );
        let mut truncated = data.clone();
        truncated.pop();
        std::fs::write(&path, truncated).unwrap();
        assert!(records(&path).is_err());
        let mut traversal = wrapper(&[("data1.cab", b"cab"), ("../data1.hdr", b"hdr")]);
        std::fs::write(&path, &traversal).unwrap();
        assert!(records(&path).is_err());
        traversal[0] = 0;
        std::fs::write(&path, traversal).unwrap();
        assert!(records(&path).unwrap().is_none());
    }
}
