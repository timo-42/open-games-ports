//! A read-only layer populated with individually registered, user-generated files.
use super::*;
use std::sync::RwLock;

#[derive(Debug, Clone)]
pub struct SessionFiles {
    // MemoryFS holds directory entries only. File contents stream from the registered backing file.
    entries: VfsPath,
    backing: Arc<RwLock<BTreeMap<String, PathBuf>>>,
}

impl SessionFiles {
    pub fn mount(label: &str) -> (Layer, Self) {
        let files = Self {
            entries: vfs::MemoryFS::new().into(),
            backing: Default::default(),
        };
        let layer = Layer {
            root: files.clone().into(),
            label: label.to_owned(),
            artifacts: Vec::new(),
            disc: None,
            warnings: Vec::new(),
            track_sources: Vec::new(),
            installer_rfa_source: None,
            indexed_files: None,
            original_names: BTreeMap::new(),
        };
        (layer, files)
    }

    /// Register a completed file from the game's writable directory with the overlay.
    pub fn publish(&self, name: &str, backing: &Path) -> Result<()> {
        let name = virtual_path(name)?;
        let backing = backing.canonicalize().map_err(|e| e.to_string())?;
        let marker = self.entries.join(&name).map_err(|e| e.to_string())?;
        let mut paths = self.backing.write().map_err(|e| e.to_string())?;
        let key = format!("/{name}");
        if paths.get(&key) == Some(&backing) {
            return Ok(());
        }
        marker
            .parent()
            .create_dir_all()
            .map_err(|e| e.to_string())?;
        marker.create_file().map_err(|e| e.to_string())?;
        paths.insert(key, backing);
        Ok(())
    }

    fn backing_file(&self, path: &str) -> VfsResult<PathBuf> {
        let paths = self.backing.read().map_err(failure)?;
        let file = paths.get(path).ok_or(VfsErrorKind::FileNotFound)?;
        if file.canonicalize()? != *file {
            return Err(VfsErrorKind::NotSupported.into());
        }
        Ok(file.clone())
    }
}

impl FileSystem for SessionFiles {
    fn read_dir(&self, path: &str) -> VfsResult<Box<dyn Iterator<Item = String> + Send>> {
        Ok(Box::new(
            self.entries
                .join(path)?
                .read_dir()?
                .map(|entry| entry.filename()),
        ))
    }
    fn open_file(&self, path: &str) -> VfsResult<FileStream> {
        Ok(Box::new(File::open(self.backing_file(path)?)?))
    }
    fn metadata(&self, path: &str) -> VfsResult<VfsMetadata> {
        let mut metadata = self.entries.join(path)?.metadata()?;
        if metadata.file_type == VfsFileType::File {
            metadata.len = fs::metadata(self.backing_file(path)?)?.len();
        }
        Ok(metadata)
    }
    fn exists(&self, path: &str) -> VfsResult<bool> {
        self.entries.join(path)?.exists()
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
