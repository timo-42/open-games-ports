//! Read-only, bounded access to original game data without extracting it.
//!
//! Paths use DOS-style ASCII case folding on every platform. Original paths and
//! archive member names remain available for evidence and diagnostics. Symlinks
//! and ambiguous case variants are rejected. The mount is not a security sandbox
//! against another process concurrently replacing its directories.
#![forbid(unsafe_code)]

pub mod archive;
mod assets;
pub mod layers;
pub use assets::{AssetStream, AssetVfs};

pub mod neolite;
pub mod refpack;
pub mod rfa;
mod sha256;

pub use sha256::{sha256, sha256_hex};

pub const MAX_FILE_BYTES: usize = 256 * 1024 * 1024;
pub const MAX_DECOMPRESSED_BYTES: usize = 64 * 1024 * 1024;
const MAX_PATH_BYTES: usize = 4096;

pub(crate) fn normalize(path: &str) -> Result<String, String> {
    if path.is_empty() || path.len() > MAX_PATH_BYTES || path.contains(['\0', ':']) {
        return Err(format!("Invalid VFS relative path: {path:?}"));
    }
    let path = path.replace('\\', "/");
    if path.starts_with('/') {
        return Err(format!("Absolute VFS paths are not allowed: {path:?}"));
    }
    let mut components = Vec::new();
    for component in path.split('/') {
        match component {
            ".." => return Err(format!("VFS path traversal is not allowed: {path:?}")),
            "" | "." => {}
            _ => components.push(component.to_ascii_lowercase()),
        }
    }
    if components.is_empty() {
        return Err(format!("Invalid VFS relative path: {path:?}"));
    }
    Ok(components.join("/"))
}

#[cfg(test)]
mod tests;
