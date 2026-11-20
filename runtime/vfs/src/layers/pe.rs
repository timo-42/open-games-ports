//! PE32 resource indexing through pelite. Resource payloads remain ranges in the
//! original file and are opened as seekable streams by the layer backend.
#![forbid(unsafe_code)]

use pelite::FileMap;
use pelite::pe32::{Pe, PeFile};
use pelite::resources::{Directory, Entry, Name};
use std::collections::BTreeSet;
use std::path::Path;

const MAX_CONTAINER_BYTES: u64 = 512 * 1024 * 1024;
const MAX_ENTRIES: usize = 200_000;
const MAX_DEPTH: usize = 8;
const MAX_RESOURCE_BYTES: usize = 256 * 1024 * 1024;

#[derive(Debug)]
pub(super) struct Resource {
    pub path: String,
    pub offset: u64,
    pub bytes: u64,
}

/// Index a PE32 resource tree without copying resource payloads into memory.
pub(super) fn index(path: &Path) -> Result<Vec<Resource>, String> {
    let metadata = std::fs::metadata(path).map_err(|e| e.to_string())?;
    if !metadata.is_file() {
        return Err("PE resource source must be a regular file".into());
    }
    if metadata.len() > MAX_CONTAINER_BYTES {
        return Err(format!(
            "PE resource source exceeds {MAX_CONTAINER_BYTES} bytes"
        ));
    }
    let mapping = FileMap::open(path).map_err(|e| format!("Map PE resource source: {e}"))?;
    let bytes = mapping.as_ref();
    let pe = PeFile::from_bytes(bytes).map_err(|e| format!("Parse PE32 source: {e}"))?;
    let resources = pe
        .resources()
        .map_err(|e| format!("Read PE resources: {e}"))?;
    let root = resources
        .root()
        .map_err(|e| format!("Read PE resource root: {e}"))?;

    let mut indexed = Vec::new();
    let mut names = BTreeSet::new();
    let mut visited = 0usize;
    visit(
        pe,
        root,
        bytes,
        &mut Vec::new(),
        0,
        &mut visited,
        &mut names,
        &mut indexed,
    )?;
    if indexed.is_empty() {
        return Err("PE contains no readable resource payloads".into());
    }
    indexed.sort_by(|a, b| a.path.cmp(&b.path));
    Ok(indexed)
}

#[allow(clippy::too_many_arguments)]
fn visit(
    pe: PeFile<'_>,
    directory: Directory<'_>,
    mapping: &[u8],
    components: &mut Vec<String>,
    depth: usize,
    visited: &mut usize,
    names: &mut BTreeSet<String>,
    indexed: &mut Vec<Resource>,
) -> Result<(), String> {
    if depth > MAX_DEPTH {
        return Err(format!("PE resource tree exceeds depth {MAX_DEPTH}"));
    }
    for child in directory.entries() {
        *visited = visited
            .checked_add(1)
            .ok_or("PE resource entry count overflow")?;
        if *visited > MAX_ENTRIES {
            return Err(format!("PE resource tree exceeds {MAX_ENTRIES} entries"));
        }
        let name = component(
            child
                .name()
                .map_err(|e| format!("Read PE resource name: {e}"))?,
        )?;
        components.push(name);
        match child
            .entry()
            .map_err(|e| format!("Read PE resource entry: {e}"))?
        {
            Entry::Directory(next) => {
                visit(
                    pe,
                    next,
                    mapping,
                    components,
                    depth + 1,
                    visited,
                    names,
                    indexed,
                )?;
            }
            Entry::DataEntry(data) => {
                let bytes = data
                    .bytes()
                    .map_err(|e| format!("Read PE resource payload: {e}"))?;
                if bytes.len() > MAX_RESOURCE_BYTES {
                    return Err(format!("PE resource exceeds {MAX_RESOURCE_BYTES} bytes"));
                }
                let offset = pe
                    .rva_to_file_offset(data.image().OffsetToData)
                    .map_err(|e| format!("Map PE resource payload: {e}"))?;
                let end = offset
                    .checked_add(bytes.len())
                    .ok_or("PE resource file range overflow")?;
                if end > mapping.len() {
                    return Err("PE resource range exceeds source file".into());
                }
                let mut path = components.join("/");
                path.push_str(".bin");
                if path.len() > 4096 {
                    return Err("PE resource path exceeds 4096 bytes".into());
                }
                let key = path.to_lowercase();
                if !names.insert(key) {
                    return Err(format!("Case-ambiguous PE resource path: {path}"));
                }
                indexed.push(Resource {
                    path,
                    offset: offset as u64,
                    bytes: bytes.len() as u64,
                });
            }
        }
        components.pop();
    }
    Ok(())
}

fn component(name: Name<'_>) -> Result<String, String> {
    let value = match name {
        Name::Id(value) => value.to_string(),
        Name::Wide(value) => String::from_utf16(value)
            .map_err(|_| "PE resource name contains invalid UTF-16".to_owned())?,
        Name::Str(value) => value.to_owned(),
    };
    if value.is_empty()
        || value == "."
        || value == ".."
        || value.len() > 4096
        || value.contains(['/', '\\', ':', '\0'])
    {
        return Err(format!("Invalid PE resource path component: {value:?}"));
    }
    Ok(value)
}
