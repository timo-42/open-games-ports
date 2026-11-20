//! Shared safe stream boundary, including adapters for imported native engines.
//! Container codecs implement Source; upstream vfs owns paths and overlays.
#![forbid(unsafe_code)]

use std::io::{self, Read, Seek, SeekFrom};
use std::sync::Arc;
use vfs::{
    FileSystem, SeekAndWrite, VfsError, VfsFileType, VfsMetadata, VfsPath, VfsResult,
    error::VfsErrorKind,
};

pub type Stream = Box<dyn vfs::SeekAndRead + Send>;

/// Game-specific archive adapters expose only virtual names and streams.
pub trait Source: Send + Sync {
    fn open(&self, path: &str) -> io::Result<Stream>;
    fn size(&self, path: &str) -> io::Result<u64>;
    fn files(&self) -> io::Result<Vec<String>>;
}

struct Adapter(Arc<dyn Source>);
impl std::fmt::Debug for Adapter {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("ReadOnlySource")
    }
}
fn error(error: io::Error) -> VfsError {
    if error.kind() == io::ErrorKind::NotFound {
        VfsErrorKind::FileNotFound.into()
    } else {
        VfsErrorKind::Other(error.to_string()).into()
    }
}

fn io_error(error: VfsError) -> io::Error {
    match error.kind() {
        VfsErrorKind::FileNotFound => io::Error::new(io::ErrorKind::NotFound, "asset not found"),
        VfsErrorKind::IoError(cause) => io::Error::new(cause.kind(), cause.to_string()),
        VfsErrorKind::Other(message) => io::Error::other(message.clone()),
        _ => io::Error::other("asset I/O failed"),
    }
}
impl FileSystem for Adapter {
    fn open_file(&self, path: &str) -> VfsResult<Stream> {
        self.0.open(path.trim_start_matches('/')).map_err(error)
    }
    fn metadata(&self, path: &str) -> VfsResult<VfsMetadata> {
        let relative = path.trim_start_matches('/');
        match self.0.size(relative) {
            Ok(len) => Ok(VfsMetadata {
                file_type: VfsFileType::File,
                len,
                created: None,
                modified: None,
                accessed: None,
            }),
            Err(e) if e.kind() == io::ErrorKind::NotFound => {
                let prefix = format!("{relative}/");
                if relative.is_empty()
                    || self
                        .0
                        .files()
                        .map_err(error)?
                        .iter()
                        .any(|p| p.starts_with(&prefix))
                {
                    Ok(VfsMetadata {
                        file_type: VfsFileType::Directory,
                        len: 0,
                        created: None,
                        modified: None,
                        accessed: None,
                    })
                } else {
                    Err(VfsErrorKind::FileNotFound.into())
                }
            }
            Err(e) => Err(error(e)),
        }
    }
    fn exists(&self, path: &str) -> VfsResult<bool> {
        match self.metadata(path) {
            Ok(_) => Ok(true),
            Err(e) if matches!(e.kind(), VfsErrorKind::FileNotFound) => Ok(false),
            Err(e) => Err(e),
        }
    }
    fn read_dir(&self, path: &str) -> VfsResult<Box<dyn Iterator<Item = String> + Send>> {
        let path = path.trim_start_matches('/');
        let prefix = if path.is_empty() {
            String::new()
        } else {
            format!("{path}/")
        };
        let names: std::collections::BTreeSet<_> = self
            .0
            .files()
            .map_err(error)?
            .into_iter()
            .filter_map(|name| {
                name.strip_prefix(&prefix)
                    .map(|tail| tail.split('/').next().unwrap_or("").to_owned())
            })
            .collect();
        Ok(Box::new(names.into_iter()))
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

#[derive(Clone, Debug)]
pub struct Vfs {
    root: VfsPath,
}
impl Vfs {
    pub fn new(source: impl Source + 'static) -> Self {
        Self::from_root(Adapter(Arc::new(source)).into())
    }
    pub fn from_root(root: VfsPath) -> Self {
        Self { root }
    }
    pub fn open(&self, path: &str) -> io::Result<Stream> {
        self.root
            .join(normalize(path)?)
            .and_then(|p| p.open_file())
            .map_err(io_error)
    }
    pub fn metadata(&self, path: &str) -> io::Result<VfsMetadata> {
        self.root
            .join(normalize(path)?)
            .and_then(|path| path.metadata())
            .map_err(io_error)
    }
    pub fn files(&self) -> io::Result<Vec<String>> {
        fn walk(root: &VfsPath, out: &mut Vec<String>) -> VfsResult<()> {
            for path in root.read_dir()? {
                if path.is_dir()? {
                    walk(&path, out)?;
                } else {
                    out.push(path.as_str().trim_start_matches('/').to_owned());
                }
            }
            Ok(())
        }
        let mut files = Vec::new();
        walk(&self.root, &mut files).map_err(io_error)?;
        files.sort();
        Ok(files)
    }
    pub fn size(&self, path: &str) -> io::Result<u64> {
        let metadata = self.metadata(path)?;
        if metadata.file_type != VfsFileType::File {
            return Err(io::Error::other("asset is not a file"));
        }
        Ok(metadata.len)
    }
    pub fn read(&self, path: &str, limit: u64) -> io::Result<Vec<u8>> {
        let size = self.size(path)?;
        if size > limit {
            return Err(io::Error::other("asset is over read limit"));
        }
        let mut file = self.open(path)?;
        if file.seek(SeekFrom::End(0))? != size {
            return Err(io::Error::other("source changed"));
        }
        file.seek(SeekFrom::Start(0))?;
        let mut bytes = Vec::new();
        bytes
            .try_reserve_exact(usize::try_from(size).map_err(io::Error::other)?)
            .map_err(io::Error::other)?;
        file.take(limit.saturating_add(1)).read_to_end(&mut bytes)?;
        if bytes.len() as u64 != size {
            return Err(io::Error::other("source changed"));
        }
        Ok(bytes)
    }
}

pub fn normalize(path: &str) -> io::Result<String> {
    let path = path.replace('\\', "/");
    if path.is_empty()
        || path.starts_with('/')
        || path.contains([':', '\0'])
        || path
            .split('/')
            .any(|p| p.is_empty() || p == "." || p == ".." || p.eq_ignore_ascii_case(".whiteout"))
    {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "invalid virtual path",
        ));
    }
    Ok(path.to_ascii_lowercase())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    struct Fixture {
        opens: Arc<AtomicUsize>,
        content: &'static [u8],
    }
    impl Source for Fixture {
        fn open(&self, path: &str) -> io::Result<Stream> {
            self.size(path)?;
            self.opens.fetch_add(1, Ordering::Relaxed);
            Ok(Box::new(io::Cursor::new(self.content)))
        }
        fn size(&self, path: &str) -> io::Result<u64> {
            if path == "data/asset" {
                Ok(self.content.len() as u64)
            } else {
                Err(io::ErrorKind::NotFound.into())
            }
        }
        fn files(&self) -> io::Result<Vec<String>> {
            Ok(vec!["data/asset".into()])
        }
    }

    #[test]
    fn imported_adapters_share_bounds_paths_streams_and_read_only_overlays() {
        let opens = Arc::new(AtomicUsize::new(0));
        let base = Vfs::new(Fixture {
            opens: Arc::clone(&opens),
            content: b"base",
        });
        assert!(base.read("DATA/asset", 3).is_err());
        assert_eq!(opens.load(Ordering::Relaxed), 0);
        assert_eq!(base.read("DATA\\asset", 4).unwrap(), b"base");
        let mut stream = base.open("data/asset").unwrap();
        stream.seek(SeekFrom::Start(2)).unwrap();
        let mut tail = Vec::new();
        stream.read_to_end(&mut tail).unwrap();
        assert_eq!(tail, b"se");
        for bad in [
            "../data/asset",
            "/data/asset",
            "c:/data/asset",
            "data//asset",
            ".whiteout/asset",
        ] {
            assert!(base.open(bad).is_err());
        }
        let adapter = Adapter(Arc::new(Fixture {
            opens: Arc::clone(&opens),
            content: b"base",
        }));
        assert!(adapter.create_file("/new").is_err());
        let layer = Vfs::new(Fixture {
            opens,
            content: b"mod",
        });
        let overlay =
            Vfs::from_root(vfs::OverlayFS::new(&[layer.root.clone(), base.root.clone()]).into());
        assert_eq!(overlay.read("data/asset", 3).unwrap(), b"mod");
        assert_eq!(base.read("data/asset", 4).unwrap(), b"base");
    }
}
