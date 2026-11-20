//! Read-only asset streams over the upstream `vfs` directory/overlay backends.
//! Source layers are immutable while mounted. No installer or patch is executed.
#![forbid(unsafe_code)]
use sha2::{Digest, Sha256};
use std::collections::HashMap;
use std::io::Read;
use std::path::{Path, PathBuf};
use vfs::error::VfsErrorKind;
use vfs::{
    FileSystem, OverlayFS, PhysicalFS, SeekAndRead, SeekAndWrite, VfsMetadata, VfsPath, VfsResult,
};

pub(crate) mod media;

pub type AssetStream = Box<dyn SeekAndRead + Send>;
pub type Result<T> = std::result::Result<T, String>;

/// Virtual paths use Windows-style ASCII case folding, independent of host OS.
fn virtual_path(path: &str) -> Result<String> {
    let path = path.replace('\\', "/");
    if path.is_empty()
        || path.starts_with('/')
        || path.split('/').any(|p| {
            p.is_empty()
                || p == "."
                || p == ".."
                || p.eq_ignore_ascii_case(".whiteout")
                || p.contains([':', '\0'])
        })
    {
        return Err(format!("invalid virtual asset path: {path:?}"));
    }
    Ok(path.to_ascii_lowercase())
}

#[derive(Debug)]
struct Directory {
    physical: PhysicalFS,
    source: PathBuf,
    paths: HashMap<String, String>,
}

impl Directory {
    fn new(source: &Path) -> Result<Self> {
        let source = source.canonicalize().map_err(|e| e.to_string())?;
        if !source.is_dir() {
            return Err("asset source must be a directory".into());
        }
        let mut paths = HashMap::from([(String::new(), String::new())]);
        let mut pending = vec![source.clone()];
        while let Some(dir) = pending.pop() {
            for entry in std::fs::read_dir(dir).map_err(|e| e.to_string())? {
                let entry = entry.map_err(|e| e.to_string())?;
                let kind = entry.file_type().map_err(|e| e.to_string())?;
                if kind.is_symlink() {
                    return Err(format!(
                        "source symlink refused: {}",
                        entry.path().display()
                    ));
                }
                if !kind.is_file() && !kind.is_dir() {
                    return Err("source contains a special file".into());
                }
                let path = entry.path();
                let relative = path
                    .strip_prefix(&source)
                    .map_err(|e| e.to_string())?
                    .to_str()
                    .ok_or("source paths must be UTF-8")?
                    .to_owned();
                let key = format!("/{}", virtual_path(&relative)?);
                if paths.insert(key, format!("/{relative}")).is_some() {
                    return Err(format!("case-ambiguous source path: {relative}"));
                }
                if kind.is_dir() {
                    pending.push(path);
                }
            }
        }
        Ok(Self {
            physical: PhysicalFS::new(&source),
            source,
            paths,
        })
    }

    fn actual(&self, path: &str) -> VfsResult<&str> {
        let actual = self.paths.get(path).ok_or(VfsErrorKind::FileNotFound)?;
        // Recheck containment at use time. Sources must not change during a run;
        // this is not a sandbox against a concurrent hostile filesystem writer.
        let absolute = self
            .source
            .join(actual.trim_start_matches('/'))
            .canonicalize()?;
        if !absolute.starts_with(&self.source) {
            return Err(VfsErrorKind::NotSupported.into());
        }
        Ok(actual)
    }
}

impl FileSystem for Directory {
    fn read_dir(&self, path: &str) -> VfsResult<Box<dyn Iterator<Item = String> + Send>> {
        Ok(Box::new(
            self.physical
                .read_dir(self.actual(path)?)?
                .map(|name| name.to_ascii_lowercase()),
        ))
    }
    fn open_file(&self, path: &str) -> VfsResult<AssetStream> {
        self.physical.open_file(self.actual(path)?)
    }
    fn metadata(&self, path: &str) -> VfsResult<VfsMetadata> {
        self.physical.metadata(self.actual(path)?)
    }
    fn exists(&self, path: &str) -> VfsResult<bool> {
        if !self.paths.contains_key(path) {
            return Ok(false);
        }
        self.physical.exists(self.actual(path)?)
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

#[derive(Debug)]
pub struct AssetVfs {
    root: open_games_vfs_core::Vfs,
    source_fingerprints: Vec<(String, String)>,
}
impl AssetVfs {
    /// Mount original source sets and ordered replacement layers through the
    /// same container resolver used by every native game.
    pub fn sources(
        base: &[PathBuf],
        updates: &[(String, PathBuf)],
        mods: &[(String, PathBuf)],
        decoder: &str,
    ) -> Result<Self> {
        let sources = crate::layers::Sources::profile(base, updates, mods, decoder)?;
        let fingerprints = sources
            .layers
            .iter()
            .flat_map(|layer| layer.artifacts.iter())
            .map(|a| (a.name.clone(), a.sha256.clone()))
            .collect();
        let mut assets = sources.assets;
        assets.source_fingerprints = fingerprints;
        Ok(assets)
    }

    /// Shared mount boundary for container adapters; first root wins duplicates.
    pub(crate) fn from_roots(roots: &[VfsPath]) -> Self {
        Self {
            root: open_games_vfs_core::Vfs::from_root(OverlayFS::new(roots).into()),
            source_fingerprints: Vec::new(),
        }
    }

    pub(crate) fn files(&self) -> Result<Vec<String>> {
        self.root.files().map_err(|e| e.to_string())
    }

    /// Merge ISO discs in supplied order (first disc wins duplicates), then
    /// loose replacement layers. Disc metadata/track-stream APIs remain future work.
    pub fn iso_discs(discs: &[PathBuf], layers: &[PathBuf]) -> Result<Self> {
        if discs.is_empty() {
            return Err("at least one ISO disc is required".into());
        }
        let mut roots = layers
            .iter()
            .rev()
            .map(|p| Directory::new(p).map(VfsPath::from))
            .collect::<Result<Vec<_>>>()?;
        let mut source_fingerprints = Vec::new();
        for disc in discs {
            let mut file = std::fs::File::open(disc).map_err(|e| e.to_string())?;
            source_fingerprints.push((disc.display().to_string(), digest(&mut file)?));
            roots.push(media::IsoMedia::new(disc)?.into());
        }
        Ok(Self {
            root: open_games_vfs_core::Vfs::from_root(OverlayFS::new(&roots).into()),
            source_fingerprints,
        })
    }
    /// Mount base, then updates in dependency order, then mods in override order.
    /// Only loose-file replacement layers are supported; binary deltas are not.
    pub fn directories(layers: &[PathBuf]) -> Result<Self> {
        if layers.is_empty() {
            return Err("a base source is required".into());
        }
        let roots = layers
            .iter()
            .rev()
            .map(|p| Directory::new(p).map(VfsPath::from))
            .collect::<Result<Vec<_>>>()?;
        Ok(Self {
            root: open_games_vfs_core::Vfs::from_root(OverlayFS::new(&roots).into()),
            source_fingerprints: Vec::new(),
        })
    }
    pub fn source_fingerprints(&self) -> &[(String, String)] {
        &self.source_fingerprints
    }
    pub(crate) fn metadata(&self, path: &str) -> Result<VfsMetadata> {
        self.root
            .metadata(&virtual_path(path)?)
            .map_err(|e| e.to_string())
    }

    pub fn open(&self, path: &str) -> Result<AssetStream> {
        self.root
            .open(&virtual_path(path)?)
            .map_err(|e| e.to_string())
    }
    pub(crate) fn file_size(&self, path: &str) -> Result<u64> {
        self.root
            .size(&virtual_path(path)?)
            .map_err(|e| e.to_string())
    }
    pub fn read(&self, path: &str, limit: u64) -> Result<Vec<u8>> {
        self.root
            .read(&virtual_path(path)?, limit)
            .map_err(|e| e.to_string())
    }
    pub(crate) fn read_io(&self, path: &str, limit: u64) -> std::io::Result<Vec<u8>> {
        self.root.read(path, limit)
    }
    pub fn sha256(&self, path: &str) -> Result<String> {
        let mut stream = self.open(path)?;
        digest(&mut stream)
    }
}
fn digest(stream: &mut impl Read) -> Result<String> {
    let mut digest = Sha256::new();
    let mut buffer = [0; 65536];
    loop {
        let count = stream.read(&mut buffer).map_err(|e| e.to_string())?;
        if count == 0 {
            break;
        }
        digest.update(&buffer[..count]);
    }
    Ok(format!("{:x}", digest.finalize()))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_escapes_and_reserved_paths() {
        for path in [
            "",
            "../x",
            "/x",
            "c:\\x",
            "a//b",
            "a/./b",
            "a\0b",
            ".whiteout/x",
        ] {
            assert!(virtual_path(path).is_err(), "{path:?}");
        }
        assert_eq!(
            virtual_path("CARS\\240SX/Geometry.BIN").unwrap(),
            "cars/240sx/geometry.bin"
        );
    }
    #[test]
    fn reads_case_insensitively_and_later_layers_win() {
        let temp = std::env::temp_dir().join(format!("og-vfs-{}", std::process::id()));
        std::fs::create_dir(&temp).unwrap();
        for layer in ["base", "update", "mod"] {
            std::fs::create_dir(temp.join(layer)).unwrap();
            std::fs::write(temp.join(layer).join("DATA.bin"), layer).unwrap();
        }
        let layers: Vec<_> = ["base", "update", "mod"]
            .iter()
            .map(|s| temp.join(s))
            .collect();
        std::fs::write(layers[0].join("base-only"), b"original").unwrap();
        let fs = AssetVfs::directories(&layers).unwrap();
        assert_eq!(fs.read("Data.BIN", 3).unwrap(), b"mod");
        assert_eq!(fs.read("BASE-ONLY", 8).unwrap(), b"original");
        assert!(fs.read("data.bin", 2).is_err());
        assert!(fs.open("../base/DATA.bin").is_err());
        assert!(
            Directory::new(&layers[0])
                .unwrap()
                .create_file("/new")
                .is_err()
        );
        assert_eq!(std::fs::read(layers[0].join("DATA.bin")).unwrap(), b"base");
        std::os::unix::fs::symlink("/etc/passwd", layers[0].join("escape")).unwrap();
        assert!(AssetVfs::directories(&layers).is_err());
        std::fs::remove_dir_all(temp).unwrap();
    }
}
