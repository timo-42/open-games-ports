//! Pack identity and reference lists transmitted by the dedicated server.
use super::{GameAssets, block_checksum, pack_language};
use md4::{Digest, Md4};
use open_games_vfs::layers::{FileStream, Layer};
use std::io::{self, Read, Seek, SeekFrom};
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};

#[derive(Debug)]
pub(super) struct Iwd {
    pub(super) path: PathBuf,
    pub(super) entry_count: usize,
    pub(super) source_layer: usize,
    pub(super) layer_index: Option<usize>,
    game: String,
    basename: String,
    localized: bool,
    contents_crc32: Vec<u8>,
    checksum: i32,
    referenced: AtomicBool,
}

impl Iwd {
    pub(super) fn mount(
        source: &Layer,
        source_layer: usize,
        path: PathBuf,
        game: &str,
        filename: &str,
    ) -> io::Result<(Layer, Self)> {
        // FS_LoadPackFile hashes the little-endian CRCs of nonempty entries
        // in central-directory order. Hashing file names or decompressed data
        // would produce a different handshake value.
        let mut contents_crc32 = Vec::new();
        let layer = source
            .zip_archive_with_entries(filename, filename, |_, size, crc| {
                if size != 0 {
                    contents_crc32.extend_from_slice(&crc.to_le_bytes());
                }
            })
            .map_err(io::Error::other)?;
        let entry_count =
            directory_entry_count(source.open_file(filename).map_err(io::Error::other)?)?;
        let pack = Self {
            path,
            entry_count,
            layer_index: None,
            source_layer,
            game: game.to_owned(),
            basename: filename[..filename.len() - 4].to_owned(),
            localized: pack_language(filename).is_some(),
            checksum: block_checksum(&contents_crc32) as i32,
            contents_crc32,
            referenced: AtomicBool::new(false),
        };
        Ok((layer, pack))
    }

    fn client_referenced(&self) -> bool {
        self.referenced.load(Ordering::Relaxed)
            || !self
                .game
                .get(..4)
                .is_some_and(|p| p.eq_ignore_ascii_case("main"))
    }

    fn pure_checksum(&self, feed: i32) -> i32 {
        let digest = Md4::new()
            .chain_update(feed.to_le_bytes())
            .chain_update(&self.contents_crc32)
            .finalize();
        super::fold_digest(&digest) as i32
    }
}

/// The original minizip reports all ZIP32 directory records, including duplicate
/// empty directories. ZipArchive's lookup inventory folds those duplicates.
fn directory_entry_count(mut source: FileStream) -> io::Result<usize> {
    use zip::unstable::format::blocks::Zip32CentralDirectoryEnd;
    const FOOTER: usize = 22;
    let end = source.seek(SeekFrom::End(0))?;
    let size = end.min((FOOTER + u16::MAX as usize) as u64) as usize;
    source.seek(SeekFrom::End(-(size as i64)))?;
    let mut tail = vec![0; size];
    source.read_exact(&mut tail)?;
    for (offset, mark) in tail.windows(4).enumerate().rev() {
        let record = &tail[offset..];
        if mark == b"PK\x05\x06"
            && record.len() >= FOOTER
            && usize::from(u16::from_le_bytes([record[20], record[21]])) == record.len() - FOOTER
        {
            let footer =
                Zip32CentralDirectoryEnd::parse(&mut &record[..]).map_err(io::Error::other)?;
            return Ok(usize::from(footer.number_of_files));
        }
    }
    Err(io::Error::new(
        io::ErrorKind::InvalidData,
        "IWD has no ZIP32 end record",
    ))
}

impl GameAssets {
    /// FS_ClearPakReferences at a full level start. A failed load retains the
    /// reference table of the still-active map. The exclusive borrow keeps
    /// concurrent asset opens outside the change of level.
    pub fn with_map_references<T>(
        &mut self,
        load: impl FnOnce(&Self) -> io::Result<T>,
    ) -> io::Result<T> {
        self.with_reference_transaction(|assets| {
            for pack in &assets.packs {
                pack.referenced.store(false, Ordering::Relaxed);
            }
            load(assets)
        })
    }

    /// Keep the old active level's references if a batch of map commands fails.
    pub fn with_reference_transaction<T>(
        &mut self,
        apply: impl FnOnce(&mut Self) -> io::Result<T>,
    ) -> io::Result<T> {
        let before = self.unpure_opens.load(Ordering::Relaxed);
        let previous: Vec<_> = self
            .packs
            .iter()
            .map(|p| p.referenced.load(Ordering::Relaxed))
            .collect();
        let result = apply(self);
        if result.is_err() {
            self.unpure_opens.store(before, Ordering::Relaxed);
            for (pack, referenced) in self.packs.iter().zip(previous) {
                pack.referenced.store(referenced, Ordering::Relaxed);
            }
        }
        result
    }

    pub(super) fn record_open(&self, path: &str) -> io::Result<()> {
        let (_, index) = self.files.file_info(path).map_err(io::Error::other)?;
        let Some(pack) = self.packs.iter().find(|p| p.layer_index == Some(index)) else {
            // FS_FOpenFileReadInternal generates fs_unpureFileToken for each actual
            // loose-file open, including repeated reads of the same mod script.
            let ext = path.rsplit_once('.').map_or("", |(_, ext)| ext);
            if !ext.eq_ignore_ascii_case("cfg") && !ext.eq_ignore_ascii_case("menu") {
                self.unpure_opens.fetch_add(1, Ordering::Relaxed);
            }
            return Ok(());
        };
        if [
            ".hlsl",
            ".txt",
            ".cfg",
            ".levelshots",
            ".menu",
            ".arena",
            ".str",
        ]
        .iter()
        .any(|ext| {
            path.get(path.len().saturating_sub(ext.len())..)
                .is_some_and(|suffix| suffix.eq_ignore_ascii_case(ext))
        }) {
            return Ok(());
        }
        pack.referenced.store(true, Ordering::Relaxed);
        Ok(())
    }

    /// SV_SpawnServer uses regular pack checksums in systeminfo even with sv_pure enabled.
    pub fn iwd_info(&self, pure: bool) -> [(&'static str, String); 4] {
        let mut loaded_names = String::new();
        let mut loaded_checksums = String::new();
        let mut referenced_names = String::new();
        let mut referenced_checksums = String::new();
        for pack in self.packs.iter().rev() {
            if pure && !pack.localized {
                append_name(&mut loaded_names, &pack.basename);
                append(&mut loaded_checksums, &format!("{} ", pack.checksum));
            }
            // Q_stricmpn(..., "main", 4) also recognizes game names beginning with "main".
            if pack.client_referenced() {
                append_name(
                    &mut referenced_names,
                    &format!("{}/{}", pack.game, pack.basename),
                );
                append(&mut referenced_checksums, &format!("{} ", pack.checksum));
            }
        }
        [
            ("sv_iwds", loaded_checksums),
            ("sv_iwdNames", loaded_names),
            ("sv_referencedIwds", referenced_checksums),
            ("sv_referencedIwdNames", referenced_names),
        ]
    }

    /// A client can request a mounted third-party archive before it is referenced. Resolve
    /// by the mounted pack identity so a same-named file in a different game
    /// directory or inside a nested ZIP cannot substitute its contents.
    pub fn open_download(&self, name: &str) -> io::Result<Option<FileStream>> {
        if stock_iwd(name) || name.to_ascii_lowercase().contains("_svr_") {
            return Ok(None);
        }
        let Some(pack) = self.packs.iter().rev().find(|pack| {
            pack.layer_index.is_some()
                && name.eq_ignore_ascii_case(&format!("{}/{}.iwd", pack.game, pack.basename))
        }) else {
            return Ok(None);
        };
        self.files.layers[pack.source_layer]
            .open_file(&format!("{}.iwd", pack.basename))
            .map(Some)
            .map_err(io::Error::other)
    }

    /// Non-localized pure checksums in search order for SV_VerifyPaks_f.
    pub fn loaded_pure_checksums(&self, feed: i32) -> Vec<i32> {
        self.packs
            .iter()
            .rev()
            .filter(|p| !p.localized)
            .map(|p| p.pure_checksum(feed))
            .collect()
    }
}

/// The stock client's missing-file list passes extensionless names to FS_iwIwd.
pub(crate) fn stock_iwd(name: &str) -> bool {
    let normalized = name.to_ascii_lowercase();
    let name = normalized.strip_suffix(".iwd").unwrap_or(&normalized);
    let Some(basename) = name.strip_prefix("main/") else {
        return false;
    };
    (0..25).any(|i| {
        basename == format!("iw_{i:02}")
            || (basename.starts_with("localized_") && basename.contains(&format!("_iw{i:02}")))
    })
}

fn append_name(out: &mut String, name: &str) {
    if !out.is_empty() {
        append(out, " ");
    }
    append(out, name);
}

fn append(out: &mut String, text: &str) {
    // I_strncat reserves one byte for NUL in its 0x2000-byte destination.
    let available = 8191 - out.len();
    out.push_str(&text[..text.floor_char_boundary(available.min(text.len()))]);
}
