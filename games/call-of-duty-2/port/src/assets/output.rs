//! Server-generated files go into the writable game directory under fs_homepath.

use open_games_vfs::layers::virtual_path;
use std::{
    fs, io,
    path::{Path, PathBuf},
};

#[derive(Clone, Debug, Default)]
pub struct WritableGame(Option<PathBuf>);

#[cfg(any(windows, test))]
fn valid_win32_component(name: &str) -> bool {
    let stem = name
        .split('.')
        .next()
        .unwrap_or(name)
        .trim_end_matches(' ')
        .to_ascii_uppercase();
    let device = matches!(
        stem.as_str(),
        "CON" | "PRN" | "AUX" | "NUL" | "CONIN$" | "CONOUT$" | "CLOCK$"
    ) || stem
        .strip_prefix("COM")
        .or_else(|| stem.strip_prefix("LPT"))
        .is_some_and(|n| {
            matches!(
                n,
                "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | "¹" | "²" | "³"
            )
        });
    !device
        && !name.ends_with(['.', ' '])
        && !name.chars().any(|c| c < ' ' || "<>:\"|?*".contains(c))
}

impl WritableGame {
    pub fn from_paths(base: &Path, home: Option<&Path>, game: &str) -> io::Result<Self> {
        let Some(home) = home.filter(|p| !p.as_os_str().is_empty()) else {
            return Ok(Self(None));
        };
        let game = if game.is_empty() { "main" } else { game };
        virtual_path(game).map_err(io::Error::other)?;
        let game = game.replace('\\', "/");
        let base = base.canonicalize()?;
        // Check the nearest existing ancestor before creating a new home directory.
        let ancestor = home
            .ancestors()
            .find(|p| p.exists())
            .unwrap_or(Path::new("."));
        if ancestor.canonicalize()?.starts_with(&base) {
            return Ok(Self(None));
        }
        fs::create_dir_all(home)?;
        let mut path = home.canonicalize()?;
        for part in game.split('/') {
            path = Self::component(&path, part)?;
            if path.starts_with(&base) {
                return Ok(Self(None));
            }
            Self::ensure_dir(&path)?;
        }
        if base.starts_with(&path) {
            return Ok(Self(None));
        }
        Ok(Self(Some(path)))
    }

    // Retain the caller's spelling on creation, resolving existing Windows game paths case-insensitively.
    fn component(parent: &Path, name: &str) -> io::Result<PathBuf> {
        #[cfg(windows)]
        if !valid_win32_component(name) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "invalid Win32 output component",
            ));
        }
        let path = parent.join(name);
        match fs::symlink_metadata(&path) {
            Ok(_) => {
                // Preserve exact spelling when it exists. On case-insensitive
                // filesystems metadata lookup may succeed for a differently
                // cased request; resolve that to the directory entry's actual
                // spelling so the later canonical-path check remains valid.
                for entry in fs::read_dir(parent)? {
                    let entry = entry?;
                    if entry.file_name() == name {
                        return Ok(entry.path());
                    }
                }
            }
            Err(err) if err.kind() == io::ErrorKind::NotFound => {}
            Err(err) => return Err(err),
        }
        for entry in fs::read_dir(parent)? {
            let entry = entry?;
            if entry
                .file_name()
                .to_str()
                .is_some_and(|part| part.eq_ignore_ascii_case(name))
            {
                return Ok(entry.path());
            }
        }
        Ok(path)
    }

    fn ensure_dir(path: &Path) -> io::Result<()> {
        match fs::symlink_metadata(path) {
            Ok(meta) if meta.is_dir() && path.canonicalize()? == path => Ok(()),
            Ok(_) => Err(io::Error::other("game output directory is not a directory")),
            Err(e) if e.kind() == io::ErrorKind::NotFound => fs::create_dir(path),
            Err(e) => Err(e),
        }
    }

    pub fn replace(&self, name: &str, bytes: &[u8]) -> io::Result<PathBuf> {
        use std::io::Write;
        let path = self
            .path(name)?
            .ok_or_else(|| io::Error::other("a writable fs_homepath is required"))?;
        let mut output =
            tempfile::NamedTempFile::new_in(path.parent().expect("game output directory"))?;
        output.write_all(bytes)?;
        output.persist(&path)?;
        Ok(path)
    }

    pub fn path(&self, name: &str) -> io::Result<Option<PathBuf>> {
        let Some(mut path) = self.0.clone() else {
            return Ok(None);
        };
        virtual_path(name).map_err(io::Error::other)?;
        let name = name.replace('\\', "/");
        let mut parts = name.split('/').peekable();
        while let Some(part) = parts.next() {
            path = Self::component(&path, part)?;
            if parts.peek().is_some() {
                Self::ensure_dir(&path)?;
            }
        }
        match fs::symlink_metadata(&path) {
            Ok(meta) if !meta.is_file() || path.canonicalize()? != path => {
                return Err(io::Error::other("not a regular game output file"));
            }
            Err(err) if err.kind() != io::ErrorKind::NotFound => return Err(err),
            _ => {}
        }
        Ok(Some(path))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn writable_home_cannot_modify_base_or_follow_links() -> io::Result<()> {
        let temp = TempDir::new()?;
        let base = temp.path().join("base");
        let home = temp.path().join("home");
        fs::create_dir(&base)?;
        let forbidden = WritableGame::from_paths(&base, Some(&base.join("newhome")), "main")?;
        assert!(forbidden.path("game.log")?.is_none());
        assert!(!base.join("newhome").exists());
        assert!(
            WritableGame::from_paths(&base, Some(temp.path()), "base")?
                .path("original.cfg")?
                .is_none()
        );
        for name in [
            "NUL.txt",
            "con",
            "LPT¹.csv",
            "CONOUT$",
            "COM3 .scr",
            ".. ",
            "normal.",
        ] {
            assert!(!valid_win32_component(name), "{name}");
        }
        assert!(valid_win32_component("LaunchSettings.cfg"));
        let writable = WritableGame::from_paths(&base, Some(&home), "main")?;
        assert!(writable.path("../outside").is_err());
        #[cfg(unix)]
        {
            std::os::unix::fs::symlink(&base, home.join("main/alias"))?;
            assert!(writable.path("alias/ban.txt").is_err());
        }
        Ok(())
    }
}
