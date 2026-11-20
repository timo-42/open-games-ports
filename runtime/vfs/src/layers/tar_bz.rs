//! Mount a selected member of a pinned bzip2-compressed tar. The bzip2 stream
//! is sequential, while a nested archive needs independent seekable readers.
use super::{Layer, MAX_MEMBER, Member, Result, fingerprint, hex, insert, virtual_path};
use bzip2_rust::read::BzDecoder;
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    fs::File,
    io::{BufReader, Read, Write},
    path::Path,
    sync::Arc,
};

impl Layer {
    /// Verify the package identity and expose one regular file under its original
    /// virtual name. Only the selected member is spooled to disposable storage.
    pub fn tar_bz2_member(
        path: &Path,
        member: &str,
        archive_sha256: &str,
        member_sha256: &str,
        label: &str,
    ) -> Result<Self> {
        let name = virtual_path(member)?;
        let filename = path
            .file_name()
            .ok_or("Missing tar filename")?
            .to_string_lossy();
        let artifact = fingerprint(path, filename.into_owned())?;
        if !artifact.sha256.eq_ignore_ascii_case(archive_sha256) {
            return Err(format!("Unexpected SHA-256 for {}", artifact.name));
        }
        let file = File::open(path).map_err(|e| e.to_string())?;
        let decoder = BzDecoder::new(BufReader::new(file));
        let mut archive = tar::Archive::new(decoder);
        for entry in archive.entries().map_err(|e| e.to_string())? {
            let mut entry = entry.map_err(|e| e.to_string())?;
            let entry_name = entry.path_bytes();
            if entry_name.strip_prefix(b"./").unwrap_or(&entry_name) != member.as_bytes() {
                continue;
            }
            if !entry.header().entry_type().is_file() || entry.size() > MAX_MEMBER {
                return Err(format!("Invalid tar member {name}"));
            }
            let mut spool = tempfile::NamedTempFile::new().map_err(|e| e.to_string())?;
            let mut digest = Sha256::new();
            let mut buf = [0u8; 65536];
            let mut bytes = 0u64;
            loop {
                let count = entry.read(&mut buf).map_err(|e| e.to_string())?;
                if count == 0 {
                    break;
                }
                spool.write_all(&buf[..count]).map_err(|e| e.to_string())?;
                digest.update(&buf[..count]);
                bytes += count as u64;
            }
            if bytes != entry.size() || !hex(&digest.finalize()).eq_ignore_ascii_case(member_sha256)
            {
                return Err(format!("Tar member {name} failed verification"));
            }
            let mut members = BTreeMap::new();
            insert(
                &mut members,
                name.clone(),
                Member::Spooled {
                    file: Arc::new(spool),
                    bytes,
                },
            )?;
            return Self::from_members(
                members,
                format!("{label}|tar.bz2:{name}"),
                vec![artifact],
                Vec::new(),
            );
        }
        Err(format!("Missing tar member {name}"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Seek;

    const MEMBER_SHA: &str = "a4895eb44afc336fecbba6e520cd67e178dace0276655d102fceffa8e5f70570";

    #[test]
    fn selected_tar_member_has_verified_identity_and_independent_cursors() -> Result<()> {
        let dir = tempfile::tempdir().map_err(|e| e.to_string())?;
        let path = dir.path().join("fixture.tar.bz2");
        let encoder =
            bzip2_rust::write::BzEncoder::new(Vec::new(), bzip2_rust::Compression::fast());
        let mut tar = tar::Builder::new(encoder);
        for (name, data) in [("other", b"junk".as_slice()), ("main/update", b"patch")] {
            let mut header = tar::Header::new_gnu();
            header.set_size(data.len() as u64);
            header.set_mode(0o644);
            tar.append_data(&mut header, name, data)
                .map_err(|e| e.to_string())?;
        }
        let bytes = tar
            .into_inner()
            .and_then(|encoder| encoder.finish())
            .map_err(|e| e.to_string())?;
        let archive_sha = hex(&Sha256::digest(&bytes));
        std::fs::write(&path, bytes).map_err(|e| e.to_string())?;
        let layer = Layer::tar_bz2_member(&path, "main/update", &archive_sha, MEMBER_SHA, "test")?;
        assert_eq!(layer.files()?, ["main/update"]);
        assert_eq!(layer.artifacts[0].sha256, archive_sha);
        let mut first = layer.open_file("main/update")?;
        let second = layer.open_file("MAIN/UPDATE")?;
        first
            .seek(std::io::SeekFrom::Start(2))
            .map_err(|e| e.to_string())?;
        assert_eq!(
            std::io::read_to_string(second).map_err(|e| e.to_string())?,
            "patch"
        );
        assert_eq!(
            std::io::read_to_string(first).map_err(|e| e.to_string())?,
            "tch"
        );
        assert!(Layer::tar_bz2_member(&path, "missing", &archive_sha, MEMBER_SHA, "test").is_err());
        assert!(
            Layer::tar_bz2_member(&path, "main/update", &archive_sha, &archive_sha, "test")
                .is_err()
        );
        assert!(
            Layer::tar_bz2_member(&path, "main/update", MEMBER_SHA, MEMBER_SHA, "test").is_err()
        );
        Ok(())
    }
}
