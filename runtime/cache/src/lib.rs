//! Disposable, game-scoped storage shared by native games and container adapters.
#![forbid(unsafe_code)]

use fs4::fs_std::FileExt;
use sha2::{Digest, Sha256};
use std::fs::{self, File, OpenOptions};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};

type Result<T> = std::result::Result<T, String>;
const MARKER: &str = ".open-games-cache-v1";

#[derive(Clone, Debug)]
pub struct Cache {
    root: PathBuf,
    game: String,
}

/// Once activated by a cache consumer, holds a shared lock until all clones drop.
/// A lazy session allows uncached runs without creating or opening cache files.
#[derive(Debug)]
pub struct Session {
    cache: Cache,
    lock: Arc<Mutex<Option<File>>>,
    profile: Vec<String>,
}

impl Cache {
    pub fn new(game: &str, root: Option<&Path>) -> Result<Self> {
        if game.is_empty()
            || !game
                .bytes()
                .all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'-')
        {
            return Err("invalid cache game identifier".into());
        }
        let root = match root {
            Some(path) => std::path::absolute(path).map_err(|e| e.to_string())?,
            None => directories::ProjectDirs::from("org", "open-games", "open-games")
                .ok_or("no platform user cache directory is available; pass --cache-dir")?
                .cache_dir()
                .to_owned(),
        };
        Ok(Self {
            root,
            game: game.into(),
        })
    }

    pub fn directory(&self) -> PathBuf {
        self.root.join(&self.game)
    }

    fn check_directory(&self) -> Result<bool> {
        let directory = self.directory();
        match fs::symlink_metadata(&directory) {
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(false),
            Err(e) => Err(e.to_string()),
            Ok(meta) if meta.is_dir() && !meta.file_type().is_symlink() => {
                let marker = directory.join(MARKER);
                let meta = fs::symlink_metadata(&marker)
                    .map_err(|_| "cache directory is not managed by Open Games")?;
                if !meta.is_file()
                    || meta.file_type().is_symlink()
                    || fs::read_to_string(marker).map_err(|e| e.to_string())? != self.game
                {
                    return Err("refusing an unrecognized cache directory".into());
                }
                Ok(true)
            }
            Ok(_) => Err("cache namespace must be a directory, never a symlink".into()),
        }
    }

    fn lock(&self) -> Result<File> {
        fs::create_dir_all(&self.root).map_err(|e| e.to_string())?;
        let path = self.root.join(format!(".{}.lock", self.game));
        if fs::symlink_metadata(&path).is_ok_and(|m| !m.is_file() || m.file_type().is_symlink()) {
            return Err("cache lock must be a regular file".into());
        }
        OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .open(path)
            .map_err(|e| e.to_string())
    }

    /// Defer all filesystem access until an entry is requested or activate is called.
    pub fn lazy_session(&self) -> Result<Session> {
        Ok(Session {
            cache: self.clone(),
            lock: Arc::new(Mutex::new(None)),
            profile: Vec::new(),
        })
    }

    pub fn session(&self) -> Result<Session> {
        let session = self.lazy_session()?;
        session.activate()?;
        Ok(session)
    }

    fn session_lock(&self) -> Result<File> {
        let lock = self.lock()?;
        loop {
            // Wait for initialization before inspecting the directory/marker pair.
            FileExt::lock_shared(&lock).map_err(|e| e.to_string())?;
            if self.check_directory()? {
                break;
            }
            FileExt::unlock(&lock).map_err(|e| e.to_string())?;
            if FileExt::try_lock_exclusive(&lock).map_err(|e| e.to_string())? {
                if !self.check_directory()? {
                    // Publish the directory and its ownership marker together.
                    // Interrupted setup can leave only an unpublished staging tree,
                    // never an unmanaged game namespace that blocks later startups.
                    let staged = tempfile::Builder::new()
                        .prefix(&format!(".{}.init-", self.game))
                        .tempdir_in(&self.root)
                        .map_err(|e| e.to_string())?;
                    fs::write(staged.path().join(MARKER), &self.game).map_err(|e| e.to_string())?;
                    fs::rename(staged.path(), self.directory()).map_err(|e| e.to_string())?;
                }
                FileExt::lock_shared(&lock).map_err(|e| e.to_string())?;
                break;
            }
            // Another starter may still hold its initial shared lock. Retry
            // without waiting for exclusive access to an already running game.
            std::thread::yield_now();
        }
        Ok(lock)
    }

    /// Remove this game's managed disposable entries. Saves/settings never belong here.
    pub fn clear(&self) -> Result<()> {
        let lock = self.lock()?;
        if !FileExt::try_lock_exclusive(&lock).map_err(|e| e.to_string())? {
            return Err("cache is in use; close this game's processes before clearing it".into());
        }
        if !self.check_directory()? {
            return Ok(());
        }
        for entry in fs::read_dir(self.directory()).map_err(|e| e.to_string())? {
            let entry = entry.map_err(|e| e.to_string())?;
            if entry.file_name() == MARKER {
                continue;
            }
            // remove_dir_all does not follow directory symlinks.
            if entry.file_type().map_err(|e| e.to_string())?.is_dir() {
                fs::remove_dir_all(entry.path()).map_err(|e| e.to_string())?;
            } else {
                fs::remove_file(entry.path()).map_err(|e| e.to_string())?;
            }
        }
        Ok(())
    }

    pub fn size(&self) -> Result<u64> {
        let lock = self.lock()?;
        FileExt::lock_shared(&lock).map_err(|e| e.to_string())?;
        if !self.check_directory()? {
            return Ok(0);
        }
        fn bytes(path: &Path, seen: &mut std::collections::HashSet<(u64, u64)>) -> Result<u64> {
            let mut total = 0u64;
            for entry in fs::read_dir(path).map_err(|e| e.to_string())? {
                let entry = entry.map_err(|e| e.to_string())?;
                let meta = fs::symlink_metadata(entry.path()).map_err(|e| e.to_string())?;
                if meta.is_dir() {
                    total = total.saturating_add(bytes(&entry.path(), seen)?);
                } else if meta.is_file() {
                    #[cfg(unix)]
                    {
                        use std::os::unix::fs::MetadataExt;
                        if !seen.insert((meta.dev(), meta.ino())) {
                            continue;
                        }
                    }
                    total = total.saturating_add(meta.len());
                }
            }
            Ok(total)
        }
        bytes(&self.directory(), &mut std::collections::HashSet::new())
    }
}

impl Session {
    /// Initialize managed storage and prevent clear while it is in use. Call
    /// before using directory() for a backend that writes its own subdirectories.
    pub fn activate(&self) -> Result<()> {
        let mut held = self
            .lock
            .lock()
            .map_err(|_| "cache session lock poisoned")?;
        if held.is_none() {
            *held = Some(self.cache.session_lock()?);
        }
        Ok(())
    }

    pub fn directory(&self) -> PathBuf {
        self.cache.directory()
    }

    /// Bind every backend entry to the complete ordered base/update/mod profile.
    pub fn with_profile(&self, identities: &[&str]) -> Result<Self> {
        Ok(Self {
            cache: self.cache.clone(),
            lock: Arc::clone(&self.lock),
            profile: identities.iter().map(|value| (*value).to_owned()).collect(),
        })
    }

    /// Length-framed fields bind entries to ordered sources, configuration and decoder.
    pub fn entry(&self, decoder: &str, identities: &[&str]) -> Result<PathBuf> {
        self.activate()?;
        let mut hash = Sha256::new();
        hash.update((self.profile.len() as u64).to_le_bytes());
        for value in self
            .profile
            .iter()
            .map(String::as_str)
            .chain(std::iter::once(decoder))
            .chain(identities.iter().copied())
        {
            hash.update((value.len() as u64).to_le_bytes());
            hash.update(value.as_bytes());
        }
        let key = format!("{:x}", hash.finalize());
        let path = self.directory().join(key);
        match fs::symlink_metadata(&path) {
            Ok(meta) if !meta.is_dir() || meta.file_type().is_symlink() => {
                return Err("invalid cache entry directory".into());
            }
            Err(e) if e.kind() != std::io::ErrorKind::NotFound => return Err(e.to_string()),
            _ => {}
        }
        fs::create_dir_all(&path).map_err(|e| e.to_string())?;
        Ok(path)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[cfg(unix)]
    #[test]
    fn size_counts_shared_materialization_once() {
        let root = tempfile::tempdir().unwrap();
        let cache = Cache::new("game", Some(root.path())).unwrap();
        let session = cache.session().unwrap();
        let baseline = cache.size().unwrap();
        let source = session.entry("v1", &[]).unwrap().join("blob");
        fs::write(&source, b"synthetic").unwrap();
        let profile = session.entry("v1", &["profile"]).unwrap().join("link");
        fs::hard_link(source, profile).unwrap();
        assert_eq!(cache.size().unwrap(), baseline + 9);
    }

    #[test]
    fn unused_sessions_do_not_touch_storage_and_clones_keep_live_entries_locked() {
        let root = tempfile::tempdir().unwrap();
        let blocker = root.path().join("file-not-directory");
        fs::write(&blocker, b"synthetic").unwrap();
        let unavailable = Cache::new("game", Some(&blocker))
            .unwrap()
            .lazy_session()
            .unwrap();
        assert!(unavailable.entry("v1", &[]).is_err());
        let absent = root.path().join("absent");
        let cache = Cache::new("game", Some(&absent)).unwrap();
        let session = cache.lazy_session().unwrap();
        let profile = session.with_profile(&["base", "mod"]).unwrap();
        assert!(!absent.exists());
        let entry = profile.entry("v1", &[]).unwrap();
        drop(profile);
        assert!(cache.clear().is_err());
        drop(session);
        cache.clear().unwrap();
        assert!(!entry.exists());
    }

    #[test]
    fn interrupted_unpublished_initialization_does_not_block_startup() {
        let root = tempfile::tempdir().unwrap();
        let orphan = root.path().join(".game.init-interrupted");
        fs::create_dir(&orphan).unwrap();
        let cache = Cache::new("game", Some(root.path())).unwrap();
        let session = cache.session().unwrap();
        assert!(cache.check_directory().unwrap());
        drop(session);
        cache.clear().unwrap();
        assert!(orphan.is_dir());
    }

    #[test]
    fn concurrent_first_startups_share_a_fully_initialized_namespace() {
        let root = tempfile::tempdir().unwrap();
        let cache = Cache::new("game", Some(root.path())).unwrap();
        let barrier = std::sync::Barrier::new(16);
        let sessions = std::thread::scope(|scope| {
            let starters: Vec<_> = (0..16)
                .map(|_| {
                    scope.spawn(|| {
                        barrier.wait();
                        cache.session().unwrap()
                    })
                })
                .collect();
            starters
                .into_iter()
                .map(|task| task.join().unwrap())
                .collect::<Vec<_>>()
        });
        assert!(cache.check_directory().unwrap());
        assert!(cache.clear().is_err());
        drop(sessions);
        cache.clear().unwrap();
    }

    #[test]
    fn clear_is_scoped_and_cannot_remove_live_entries() {
        let root = tempfile::tempdir().unwrap();
        let a = Cache::new("game-a", Some(root.path())).unwrap();
        let b = Cache::new("game-b", Some(root.path())).unwrap();
        let session = a.session().unwrap();
        let second_session = a.session().unwrap();
        let entry = session.entry("v1", &["base", "update"]).unwrap();
        fs::write(entry.join("asset"), b"synthetic").unwrap();
        assert_ne!(entry, session.entry("v1", &["update", "base"]).unwrap());
        assert_ne!(entry, session.entry("v2", &["base", "update"]).unwrap());
        let profile = session.with_profile(&["base", "update", "mod"]).unwrap();
        let reordered = session.with_profile(&["base", "mod", "update"]).unwrap();
        assert_ne!(
            profile.entry("v1", &["member"]).unwrap(),
            reordered.entry("v1", &["member"]).unwrap()
        );
        drop(profile);
        drop(reordered);
        assert!(a.clear().is_err());
        let other = b.session().unwrap();
        let preserved = other.entry("v1", &["base"]).unwrap();
        drop(session);
        assert!(a.clear().is_err());
        drop(second_session);
        a.clear().unwrap();
        assert!(!entry.exists());
        assert!(preserved.exists());
    }

    #[test]
    fn clear_refuses_an_unmanaged_directory() {
        let root = tempfile::tempdir().unwrap();
        fs::create_dir(root.path().join("game")).unwrap();
        fs::write(root.path().join("game/original"), b"preserve").unwrap();
        assert!(
            Cache::new("game", Some(root.path()))
                .unwrap()
                .clear()
                .is_err()
        );
        assert!(root.path().join("game/original").exists());
    }

    #[cfg(unix)]
    #[test]
    fn clear_does_not_follow_symlinks() {
        let root = tempfile::tempdir().unwrap();
        let original = tempfile::tempdir().unwrap();
        fs::write(original.path().join("asset"), b"preserve").unwrap();
        let cache = Cache::new("game", Some(root.path())).unwrap();
        let session = cache.session().unwrap();
        std::os::unix::fs::symlink(original.path(), session.directory().join("link")).unwrap();
        drop(session);
        cache.clear().unwrap();
        assert!(original.path().join("asset").exists());
    }
}
