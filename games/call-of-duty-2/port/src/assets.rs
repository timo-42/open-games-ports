//! CoD2 search paths and version 4 multiplayer BSP files.
mod iwd;
mod output;
pub use output::WritableGame;
pub mod models;
mod sound;
pub mod weapons;
use crate::config::ServerConfig;
use crate::entities::{MapEntities, Tokens};
use iwd::Iwd;
pub(crate) use iwd::stock_iwd;
use md4::{Digest, Md4};
use open_games_vfs::MAX_FILE_BYTES;
use open_games_vfs::layers::{Layer, SessionFiles, Sources};
pub use sound::SoundAliases;
use std::fmt;
use std::io;
use std::ops::Range;
use std::path::{Path, PathBuf};

const LANGUAGES: [&str; 15] = [
    "english",
    "french",
    "german",
    "italian",
    "spanish",
    "british",
    "russian",
    "polish",
    "korean",
    "taiwanese",
    "japanese",
    "chinese",
    "thai",
    "leet",
    "czech",
];

#[derive(Debug)]
pub struct GameAssets {
    directories: Vec<PathBuf>,
    active_language: usize,
    available_languages: [bool; LANGUAGES.len()],
    generated: Option<SessionFiles>,
    writable: WritableGame,
    files: Sources,
    packs: Vec<Iwd>,
    /// Each successful non-exempt loose-file open consumes a native random word.
    pub(crate) unpure_opens: std::sync::Arc<std::sync::atomic::AtomicUsize>,
}

impl GameAssets {
    /// FS_PrintPureSearchPaths uses the currently registered language during
    /// the first mount, before Loc_InitLanguages chooses an available language.
    pub fn search_report(&self, requested: Option<usize>) -> String {
        use std::fmt::Write;
        let language = LANGUAGES
            .get(requested.unwrap_or(self.active_language))
            .unwrap_or(&"english");
        let mut report =
            format!("----- FS_Startup -----\nCurrent language: {language}\nCurrent search path:\n");
        let mut localized = Vec::new();
        for path in self.directories.iter().rev() {
            for pack in self
                .packs
                .iter()
                .rev()
                .filter(|p| p.path.parent() == Some(path))
            {
                let name = pack
                    .path
                    .file_name()
                    .and_then(|n| n.to_str())
                    .unwrap_or_default();
                if let Some(pack_language) = pack_language(name) {
                    if pack_language.eq_ignore_ascii_case(language) {
                        localized.push(pack);
                    }
                } else {
                    writeln!(
                        report,
                        "{} ({} files)",
                        pack.path.display(),
                        pack.entry_count
                    )
                    .expect("writing to String is infallible");
                }
            }
            writeln!(report, "{}", path.display()).expect("writing to String is infallible");
        }
        for pack in localized {
            writeln!(
                report,
                "{} ({} files)\n    localized assets iwd file for {language}",
                pack.path.display(),
                pack.entry_count
            )
            .expect("writing to String is infallible");
        }
        let total: usize = self.packs.iter().map(|pack| pack.entry_count).sum();
        writeln!(
            report,
            "\nFile Handles:\n----------------------\n{total} files in iwd files"
        )
        .expect("writing to String is infallible");
        report
    }

    /// The supplied DVD's Setup/Data and a patched server data root both contain main/.
    pub fn mount_paths(base: &Path, home: Option<&Path>, game: &str) -> io::Result<Self> {
        Self::mount_with_update(base, home, game, None)
    }

    pub fn mount_for_server(config: &mut ServerConfig) -> io::Result<Self> {
        let assets = Self::mount_language(
            Path::new(config.get("fs_basepath").unwrap_or(".")),
            config
                .get("fs_homepath")
                .filter(|p| !p.is_empty())
                .map(Path::new),
            config.get("fs_game").unwrap_or(""),
            config.update_package.as_deref(),
            config
                .get("loc_language")
                .and_then(|text| text.parse().ok())
                .unwrap_or(0),
        )?;
        assets.publish_language(config);
        Ok(assets)
    }

    /// A changed loc_language takes effect at the next full filesystem restart.
    /// Reuse the active layers when a requested language falls back to the same set.
    pub fn replacement_for_language(&self, config: &mut ServerConfig) -> io::Result<Option<Self>> {
        let requested = config
            .get("loc_language")
            .and_then(|text| text.parse::<usize>().ok())
            .unwrap_or(0);
        if self.active_language != select_language(&self.available_languages, requested) {
            return Self::mount_for_server(config).map(Some);
        }
        self.publish_language(config);
        Ok(None)
    }

    fn publish_language(&self, config: &mut ServerConfig) {
        config.set_from_script("loc_language", &self.active_language.to_string());
    }

    /// Overlay the official 1.3 multiplayer pack on the retail DVD's base/main.
    pub fn mount_with_update(
        base: &Path,
        home: Option<&Path>,
        game: &str,
        update: Option<&Path>,
    ) -> io::Result<Self> {
        Self::mount_language(base, home, game, update, 0)
    }

    fn mount_language(
        base: &Path,
        home: Option<&Path>,
        game: &str,
        update: Option<&Path>,
        requested_language: usize,
    ) -> io::Result<Self> {
        let patch = update
            .map(|path| {
                Layer::tar_bz2_member(
                    path,
                    "main/iw_15.iwd",
                    "36b6f433ce67dae4d9b03e2013d328c5bd0428aeae525d8bac58b06dbb9c6cdb",
                    "2c4b1797da9cc626b3dae99befa41dfbba4894dc14ce1199f5a459b88678f3ef",
                    "cod2-linux-server-1.3",
                )?
                .select(&[("main/iw_15.iwd", "iw_15.iwd")], "base/main")
            })
            .transpose()
            .map_err(invalid)?;
        Self::mount_layers(base, home, game, patch, requested_language)
    }

    fn mount_layers(
        base: &Path,
        home: Option<&Path>,
        game: &str,
        mut patch: Option<Layer>,
        requested_language: usize,
    ) -> io::Result<Self> {
        // FS_Startup adds base/main, home/main, base/mod, home/mod in this order.
        // Regular nodes override every localized node across the whole search list.
        let mut regular = Vec::new();
        let mut localized = Vec::new();
        let mut mounted = Vec::new();
        let home = home.filter(|path| !path.as_os_str().is_empty() && *path != base);
        let game = if game.is_empty() || game.eq_ignore_ascii_case("main") {
            None
        } else {
            open_games_vfs::layers::virtual_path(game).map_err(invalid)?;
            Some(game.replace('\\', "/"))
        };
        let mut generated = None;
        let writable_game = game.as_deref().unwrap_or("main");
        let mut directories = Vec::new();
        let raw = std::iter::once(base).chain(home).flat_map(|root| {
            ["devraw_shared", "devraw", "raw_shared", "raw"].map(|name| (root, name))
        });
        let games = std::iter::once("main")
            .chain(game.as_deref())
            .flat_map(|name| {
                std::iter::once(base)
                    .chain(home)
                    .map(move |root| (root, name))
            });
        for (root, directory_name) in raw.chain(games) {
            let search_path = root.join(directory_name);
            directories.push(search_path.clone());
            let runtime = if Some(root) == home && directory_name == writable_game {
                let (layer, files) = SessionFiles::mount("generated server files");
                generated = Some(files);
                Some(layer)
            } else {
                None
            };
            let Some(path) = find_directory(root, directory_name)? else {
                if let Some(layer) = runtime {
                    regular.push((None, layer, None));
                }
                continue;
            };
            let path = path.canonicalize()?;
            if mounted.contains(&path) {
                continue;
            }
            mounted.push(path.clone());
            let loose =
                Layer::open(&path, &path.display().to_string()).map_err(io::Error::other)?;
            // Keep the original basenames for the protocol, which sends their case verbatim.
            let mut filenames = Vec::new();
            for file in path.read_dir()? {
                let file = file?;
                let name = file
                    .file_name()
                    .into_string()
                    .map_err(|_| invalid("non-UTF8 IWD source name"))?;
                if name.to_ascii_lowercase().ends_with(".iwd") && file.file_type()?.is_file() {
                    filenames.push(name);
                    if filenames.len() == 1024 {
                        break;
                    }
                }
            }
            let patch_here = if directory_name == "main" && root == base {
                patch.take()
            } else {
                None
            };
            if patch_here.is_some() {
                filenames.retain(|name| !name.eq_ignore_ascii_case("iw_15.iwd"));
                filenames.push("iw_15.iwd".to_owned());
            }
            // FS_CompareLocalizedIwd sorts English before the other
            // localized packs. Regular and localized nodes are separated below.
            filenames.sort_by_cached_key(|name| {
                let english =
                    pack_language(name).is_some_and(|lang| lang.eq_ignore_ascii_case("english"));
                (!english, name.to_ascii_lowercase())
            });
            let mut packs_here = Vec::new();
            let loose_index = regular.len();
            for filename in filenames {
                let (source, index) =
                    if let Some(source) = patch_here.as_ref().filter(|_| filename == "iw_15.iwd") {
                        (source, loose_index + 1)
                    } else {
                        (&loose, loose_index)
                    };
                if let Some(lang) = pack_language(&filename) {
                    if let Some(id) = LANGUAGES
                        .iter()
                        .position(|&name| name.eq_ignore_ascii_case(lang))
                    {
                        let (layer, pack) = Iwd::mount(
                            source,
                            index,
                            search_path.join(&filename),
                            directory_name,
                            &filename,
                        )?;
                        localized.push((Some(id), layer, Some(pack)));
                    }
                } else {
                    let (layer, pack) = Iwd::mount(
                        source,
                        index,
                        search_path.join(&filename),
                        directory_name,
                        &filename,
                    )?;
                    packs_here.push((None, layer, Some(pack)));
                }
            }
            regular.push((None, loose, None));
            if let Some(layer) = runtime {
                regular.push((None, layer, None));
            }
            if let Some(patch) = patch_here {
                regular.push((None, patch, None));
            }
            regular.extend(packs_here);
        }

        if patch.is_some() {
            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                "the 1.3 update requires a base main directory",
            ));
        }
        if mounted.is_empty() {
            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                "no CoD2 game data directory",
            ));
        }
        let mut available_languages = [false; LANGUAGES.len()];
        for (language, _, _) in &localized {
            if let Some(i) = language {
                available_languages[*i] = true;
            }
        }
        let active_language = select_language(&available_languages, requested_language);
        // A pack stores the index of its containing loose layer among regular layers.
        // Localized layers precede these in the final shared VFS.
        let regular_offset = localized
            .iter()
            .filter(|(lang, _, _)| lang.is_none() || *lang == Some(active_language))
            .count();
        let mut layers = Vec::new();
        let mut packs = Vec::new();
        for (language, layer, mut pack) in localized.into_iter().chain(regular) {
            if language.is_none() || language == Some(active_language) {
                if let Some(pack) = &mut pack {
                    pack.layer_index = Some(layers.len());
                }
                layers.push(layer);
            }
            if let Some(mut pack) = pack {
                pack.source_layer += regular_offset;
                packs.push(pack);
            }
        }
        let writable =
            WritableGame::from_paths(base, home, writable_game).unwrap_or_else(|error| {
                eprintln!("Writable game directory unavailable: {error}");
                WritableGame::default()
            });
        Ok(Self {
            directories,
            writable,
            active_language,
            available_languages,
            generated,
            files: Sources::new(layers, "cod2-iwd-1").map_err(io::Error::other)?,
            packs,
            unpure_opens: Default::default(),
        })
    }

    /// MapNameFromPath and SV_MapFileExists, also used by HUD map-name elements.
    pub fn map_exists(&self, name: &str) -> bool {
        let mut name = name.to_ascii_lowercase();
        if let Some(rest) = name.strip_prefix("maps/mp/") {
            name = rest.to_owned();
        }
        if name.ends_with("bsp") && name.len() >= 7 {
            name.truncate(name.floor_char_boundary(name.len() - 7));
        }
        let path = format!("maps/mp/{}.d3dbsp", name.replace('%', "_"));
        self.record_open(&path).is_ok()
    }

    pub fn load_map(&self, name: &str) -> io::Result<ServerMap> {
        let path = format!("maps/mp/{name}.d3dbsp");
        let image = self.read_file(&path)?;
        ServerMap::parse(image)
            .map_err(|error| io::Error::new(error.kind(), format!("{path}: {error}")))
    }

    /// Names admitted by GScr_LoadGametypeList. Description files are optional.
    pub fn gametypes(&self) -> io::Result<Vec<String>> {
        let mut result = Vec::new();
        let mut list_bytes = 0;
        for path in self.files.files().map_err(io::Error::other)? {
            let Some(file) = path.strip_prefix("maps/mp/gametypes/") else {
                continue;
            };
            let Some(stem) = file.strip_suffix(".gsc") else {
                continue;
            };
            if stem.contains('/') {
                continue;
            }
            list_bytes += file.len() + 1;
            if list_bytes + 1 >= 4096 {
                break;
            }
            if !stem.starts_with('_') {
                result.push(stem[..stem.floor_char_boundary(63)].to_owned());
                if result.len() == 32 {
                    break;
                }
            }
        }
        Ok(result)
    }

    pub fn radiant_field_files(&self) -> io::Result<Vec<String>> {
        self.direct_files("radiant", ".txt")
    }

    fn direct_files(&self, directory: &str, suffix: &str) -> io::Result<Vec<String>> {
        let prefix = format!("{directory}/");
        Ok(self
            .files
            .files()
            .map_err(io::Error::other)?
            .into_iter()
            .filter(|path| {
                path.strip_prefix(&prefix)
                    .is_some_and(|name| !name.contains('/') && name.ends_with(suffix))
            })
            .collect())
    }

    pub(crate) fn read_file(&self, path: &str) -> io::Result<Vec<u8>> {
        let bytes = self.files.read_io(path, MAX_FILE_BYTES as u64)?;
        self.record_open(path)?;
        Ok(bytes)
    }

    /// SV_LoadExpectedHunkUsage leaves the previous dvar intact for unknown maps.
    pub(crate) fn expected_hunk_usage(&self, map: &str) -> io::Result<Option<i32>> {
        let bytes = match self.read_file("hunkusage.dat") {
            Ok(bytes) => bytes,
            Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(None),
            Err(error) => return Err(error),
        };
        let key = format!("maps/mp/{map}.d3dbsp");
        let mut tokens = Tokens::new(&bytes);
        while let Some(token) = tokens.next() {
            if token.eq_ignore_ascii_case(key.as_bytes()) {
                return Ok(tokens
                    .next()
                    .filter(|text| !text.is_empty())
                    .map(|n| crate::config::integer_prefix(&String::from_utf8_lossy(&n))));
            }
        }
        Ok(None)
    }

    pub fn read_script(&self, path: &str) -> io::Result<Vec<u8>> {
        self.read_file(path)
    }

    pub fn writable(&self) -> &WritableGame {
        &self.writable
    }

    /// Publish game-generated files at the loose home directory's normal search priority.
    pub fn write_generated(&self, path: &str, bytes: &[u8]) -> io::Result<()> {
        let layer = self
            .generated
            .as_ref()
            .ok_or_else(|| io::Error::other("writable home layer unavailable"))?;
        let backing = self.writable.replace(path, bytes)?;
        layer.publish(path, &backing).map_err(io::Error::other)
    }

    /// FS_FOpenFileAppend only appends to the home file; a packed or base copy is never copied.
    pub fn append_generated(&self, name: &str, bytes: &[u8]) -> io::Result<()> {
        let path = self
            .writable
            .path(name)?
            .ok_or_else(|| io::Error::other("a writable fs_homepath is required"))?;
        use std::io::Write;
        let layer = self
            .generated
            .as_ref()
            .ok_or_else(|| io::Error::other("writable home layer unavailable"))?;
        std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(&path)?
            .write_all(bytes)?;
        layer.publish(name, &path).map_err(io::Error::other)
    }

    pub fn read_bans(&self) -> io::Result<Vec<u8>> {
        self.read_file("ban.txt")
    }

    pub fn read_config(&self, path: &str) -> io::Result<String> {
        let bytes = self.read_file(path)?;
        // Stock configurations use an ASCII-compatible single-byte encoding.
        // Preserve UTF-8 configs verbatim and keep legacy bytes representable.
        Ok(String::from_utf8(bytes)
            .unwrap_or_else(|error| error.into_bytes().into_iter().map(char::from).collect()))
    }
}

// Loc_InitLanguages keeps the requested language if installed, otherwise
// walks IDs from English upwards. No localized packs leaves the dvar at zero.
fn select_language(available: &[bool; LANGUAGES.len()], requested: usize) -> usize {
    if available.get(requested) == Some(&true) {
        requested
    } else {
        available.iter().position(|v| *v).unwrap_or(0)
    }
}

impl crate::config::CommandFiles for &GameAssets {
    fn read(&mut self, path: &str) -> io::Result<String> {
        self.read_config(path)
    }

    fn write(&mut self, filename: &str, contents: &[u8]) -> io::Result<()> {
        self.write_generated(filename, contents)
    }
}

/// Select a game source directory using the same ASCII-insensitive names on all hosts.
/// A newly created home path may have no main/ or mod directory yet.
fn find_directory(root: &Path, relative: &str) -> io::Result<Option<std::path::PathBuf>> {
    let mut path = root.to_owned();
    for component in relative.split('/') {
        let entries = match path.read_dir() {
            Ok(entries) => entries,
            Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(None),
            Err(error) => return Err(error),
        };
        let mut selected = None;
        for entry in entries {
            let entry = entry?;
            let matches = entry
                .file_name()
                .to_str()
                .is_some_and(|name| name.eq_ignore_ascii_case(component));
            if !matches {
                continue;
            }
            if !entry.file_type()?.is_dir() || selected.is_some() {
                return Err(invalid(format!(
                    "invalid or ambiguous game directory {component:?}"
                )));
            }
            selected = Some(entry.path());
        }
        let Some(next) = selected else {
            return Ok(None);
        };
        path = next;
    }
    Ok(Some(path))
}

fn pack_language(pack: &str) -> Option<&str> {
    let rest = pack.strip_prefix("localized_")?;
    let end = rest.bytes().take_while(u8::is_ascii_alphabetic).count();
    Some(&rest[..end])
}

const LUMP_COUNT: usize = 39;
const HEADER_SIZE: usize = 8 + LUMP_COUNT * 8;
const ENTITY_LUMP: usize = 37;

/// The original image is retained so collision and game code can read bounded lump slices.
pub struct ServerMap {
    image: Vec<u8>,
    lumps: [Range<usize>; LUMP_COUNT],
    pub checksum: u32,
    pub entities: MapEntities,
}

impl fmt::Debug for ServerMap {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ServerMap")
            .field("byte_len", &self.byte_len())
            .field("checksum", &self.checksum)
            .finish_non_exhaustive()
    }
}

impl ServerMap {
    fn parse(image: Vec<u8>) -> io::Result<Self> {
        let header = image
            .get(..HEADER_SIZE)
            .ok_or_else(|| invalid("short BSP header"))?;
        if &header[..4] != b"IBSP" || header[4..8] != 4_u32.to_le_bytes() {
            return Err(invalid("expected IBSP version 4"));
        }

        let mut lumps = std::array::from_fn(|_| 0..0);
        for (index, entry) in header[8..].as_chunks::<8>().0.iter().enumerate() {
            let length = u32::from_le_bytes(entry[..4].try_into().expect("length field")) as usize;
            let start = u32::from_le_bytes(entry[4..].try_into().expect("offset field")) as usize;
            let end = start
                .checked_add(length)
                .ok_or_else(|| invalid("BSP lump overflow"))?;
            if end > image.len() || (length != 0 && start < HEADER_SIZE) {
                return Err(invalid(format!("invalid extent for BSP lump {index}")));
            }
            lumps[index] = start..end;
        }
        validate_collision_lumps(&lumps)?;
        let entities = MapEntities::parse(&image[lumps[ENTITY_LUMP].clone()])?;

        Ok(Self {
            checksum: block_checksum(&image),
            image,
            lumps,
            entities,
        })
    }

    pub fn byte_len(&self) -> usize {
        self.image.len()
    }

    pub fn lump(&self, index: usize) -> Option<&[u8]> {
        let extent = self.lumps.get(index)?.clone();
        self.image.get(extent)
    }
}

fn invalid(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

// Record sizes checked by CMod_Load* in chunk_003. These checks precede use of
// indices inside records by the collision subsystem.
fn validate_collision_lumps(lumps: &[Range<usize>; LUMP_COUNT]) -> io::Result<()> {
    for (index, stride) in [
        (0, 72),
        (4, 16),
        (5, 8),
        (6, 4),
        (25, 36),
        (26, 36),
        (27, 4),
        (28, 4),
        (29, 16),
        (30, 56),
        (31, 72),
        (32, 28),
        (33, 12),
        (34, 32),
        (35, 48),
    ] {
        if !lumps[index].len().is_multiple_of(stride) {
            return Err(invalid(format!("partial record in BSP lump {index}")));
        }
    }
    for index in [0, 4, 25, 26, 35] {
        if lumps[index].is_empty() {
            return Err(invalid(format!("missing required BSP lump {index}")));
        }
    }
    if lumps[35].len() / 48 > 1023 || lumps[6].len() / 4 > usize::from(u16::MAX) {
        return Err(invalid("BSP exceeds submodel or brush count limit"));
    }
    if lumps[5].len() / 8 < 6 * (lumps[6].len() / 4) {
        return Err(invalid("BSP has fewer than six sides per brush"));
    }
    if (1..8).contains(&lumps[36].len()) {
        return Err(invalid("truncated visibility header"));
    }
    Ok(())
}

/// Com_BlockChecksum (0x08067b02). This value identifies the map in the legacy protocol.
fn block_checksum(bytes: &[u8]) -> u32 {
    fold_digest(&Md4::digest(bytes))
}

fn fold_digest(digest: &md4::digest::Output<Md4>) -> u32 {
    digest
        .as_chunks::<4>()
        .0
        .iter()
        .fold(0, |sum, &word| sum ^ u32::from_le_bytes(word))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;

    fn write_pack(directory: &Path, name: &str, members: &[(&str, &[u8])]) -> io::Result<()> {
        let mut archive = zip::ZipWriter::new(fs::File::create(directory.join(name))?);
        for &(path, bytes) in members {
            archive.start_file(path, zip::write::SimpleFileOptions::default())?;
            archive.write_all(bytes)?;
        }
        archive.finish()?;
        Ok(())
    }

    #[test]
    fn loose_script_reads_consume_native_random_but_packed_scripts_and_configs_do_not()
    -> io::Result<()> {
        let root = tempfile::tempdir()?;
        let main = root.path().join("main");
        fs::create_dir(&main)?;
        fs::write(main.join("loose.gsc"), b"loose")?;
        fs::write(main.join("server.cfg"), b"cfg")?;
        write_pack(&main, "scripts.iwd", &[("packed.gsc", b"packed")])?;
        let assets = GameAssets::mount_paths(root.path(), None, "")?;
        assets.read_script("loose.gsc")?;
        let mut random = crate::vm::items::NativeRandom::new(1);
        random.track_loose_opens(assets.unpure_opens.clone(), 1);
        assets.read_script("packed.gsc")?;
        assets.read_config("server.cfg")?;
        assert_eq!(random.draw(), 1804289383);
        assets.read_script("loose.gsc")?;
        assets.read_script("loose.gsc")?;
        assert_eq!(random.draw(), 1714636915);
        Ok(())
    }

    #[test]
    fn explicit_update_replaces_one_base_pack_and_keeps_mod_precedence() -> io::Result<()> {
        let dvd = tempfile::tempdir()?;
        let base = dvd.path().join("main");
        fs::create_dir(&base)?;
        write_pack(&base, "iw_14.iwd", &[("shared.cfg", b"base")])?;
        write_pack(&base, "iw_15.iwd", &[("shared.cfg", b"previous")])?;
        let update = tempfile::tempdir()?;
        write_pack(update.path(), "iw_15.iwd", &[("shared.cfg", b"update")])?;
        let mount = |game| {
            let source = Layer::open(update.path(), "update").map_err(io::Error::other)?;
            GameAssets::mount_layers(dvd.path(), None, game, Some(source), 0)
        };
        let assets = mount("")?;
        assert_eq!(assets.read_config("shared.cfg")?, "update");
        assert_eq!(assets.iwd_info(true)[1].1, "iw_15 iw_14");
        let mods = dvd.path().join("testmod");
        fs::create_dir(&mods)?;
        write_pack(&mods, "custom.iwd", &[("shared.cfg", b"mod")])?;
        let assets = mount("testmod")?;
        assert_eq!(assets.read_config("shared.cfg")?, "mod");
        Ok(())
    }

    fn map_fixture() -> Vec<u8> {
        let mut image = vec![0; HEADER_SIZE];
        image[..4].copy_from_slice(b"IBSP");
        image[4..8].copy_from_slice(&4u32.to_le_bytes());
        for (index, length) in [(0, 72), (4, 16), (25, 36), (26, 36), (35, 48)] {
            let entry = 8 + index * 8;
            let start = image.len();
            image[entry..entry + 4].copy_from_slice(&(length as u32).to_le_bytes());
            image[entry + 4..entry + 8].copy_from_slice(&(start as u32).to_le_bytes());
            image.resize(start + length, 0);
        }
        let entities = b"{\n\"classname\" \"worldspawn\"\n}\n\0";
        let entry = 8 + ENTITY_LUMP * 8;
        let start = image.len();
        image[entry..entry + 4].copy_from_slice(&(entities.len() as u32).to_le_bytes());
        image[entry + 4..entry + 8].copy_from_slice(&(start as u32).to_le_bytes());
        image.extend_from_slice(entities);
        image
    }

    #[test]
    fn search_report_counts_raw_iwd_records_and_raw_directories_supply_assets() -> io::Result<()> {
        let root = tempfile::tempdir()?;
        let main = root.path().join("main");
        let raw = root.path().join("raw");
        fs::create_dir(&main)?;
        fs::create_dir(&raw)?;
        fs::write(raw.join("fallback.cfg"), b"raw config")?;
        let options = zip::write::SimpleFileOptions::default();
        let mut zip = zip::ZipWriter::new(io::Cursor::new(Vec::new()));
        zip.add_directory("a/", options)?;
        zip.add_directory("b/", options)?;
        zip.start_file("file.cfg", options)?;
        zip.write_all(b"value")?;
        let mut bytes = zip.finish()?.into_inner();
        // Like the retail IWDs, store two directory records with one name.
        for i in 0..bytes.len().saturating_sub(1) {
            if &bytes[i..i + 2] == b"b/" {
                bytes[i] = b'a';
            }
        }
        fs::write(main.join("iw_00.iwd"), bytes)?;
        let assets = GameAssets::mount_paths(root.path(), None, "")?;
        let report = assets.search_report(None);
        assert!(report.contains("iw_00.iwd (3 files)"));
        assert!(report.ends_with("3 files in iwd files\n"));
        assert_eq!(assets.read_config("fallback.cfg")?, "raw config");
        Ok(())
    }

    #[test]
    fn loads_map_and_uses_stock_search_order() -> io::Result<()> {
        let root = tempfile::tempdir()?;
        let main = root.path().join("main");
        fs::create_dir(&main)?;
        fs::write(main.join("shared.cfg"), "loose")?;
        fs::write(main.join("fallback.cfg"), "loose")?;
        let scripts = main.join("MAPS/MP/GAMETYPES");
        fs::create_dir_all(scripts.join("nested"))?;
        for name in ["_common.gsc", "nested/hidden.gsc", "DM.GSC", "DM.txt"] {
            fs::write(scripts.join(name), "fixture")?;
        }
        write_pack(&main, "localized_german_iw00.iwd", &[("only.cfg", b"de")])?;
        write_pack(
            &main,
            "localized_english_iw00.iwd",
            &[("ONLY.cfg", b"en"), ("fallback.cfg", b"local")],
        )?;
        write_pack(&main, "iw_00.iwd", &[("shared.cfg", b"base")])?;
        let image = map_fixture();
        write_pack(
            &main,
            "IW_15.IWD",
            &[("SHARED.cfg", b"patch"), ("maps/mp/MP_TEST.d3dbsp", &image)],
        )?;
        let assets =
            GameAssets::mount_paths(root.path(), Some(&root.path().join("empty-home")), "")?;
        assert_eq!(assets.read_config("shared.cfg")?, "patch");
        assert_eq!(assets.read_config("fallback.cfg")?, "loose");
        assert_eq!(assets.read_config("only.cfg")?, "en");
        assert_eq!(assets.gametypes()?, ["dm"]);
        assert_eq!(
            assets
                .read_config("missing.cfg")
                .expect_err("missing config")
                .kind(),
            io::ErrorKind::NotFound,
        );
        assert!(assets.iwd_info(true)[3].1.is_empty());
        assert!(assets.map_exists("maps/mp/MP_TEST.d3dbsp"));
        assert_eq!(assets.iwd_info(true)[3].1, "main/IW_15");
        let map = assets.load_map("mp_test")?;
        assert_eq!(map.byte_len(), image.len());
        assert_eq!(map.lump(4).map(<[u8]>::len), Some(16));
        assert!(assets.load_map("../outside").is_err());
        Ok(())
    }

    #[test]
    fn full_filesystem_restart_applies_language_and_hunk_usage() -> io::Result<()> {
        let temp = tempfile::tempdir()?;
        let main = temp.path().join("main");
        fs::create_dir(&main)?;
        write_pack(&main, "localized_english_a.iwd", &[("loc.cfg", b"en")])?;
        write_pack(&main, "localized_german_a.iwd", &[("loc.cfg", b"de")])?;
        fs::write(
            main.join("hunkusage.dat"),
            b"// test\nmaps/mp/MP_TEST.d3dbsp 984\n",
        )?;
        let mut config = ServerConfig::from_args(&[
            "+set fs_basepath".into(),
            temp.path().to_string_lossy().into_owned(),
            "+set fs_homepath".into(),
            temp.path().join("home").to_string_lossy().into_owned(),
            "+set loc_language 2".into(),
        ]);
        let assets = GameAssets::mount_for_server(&mut config)?;
        assert_eq!(assets.read_config("loc.cfg")?, "de");
        assert_eq!(config.get("loc_language"), Some("2"));
        config.execute("set loc_language 3; map mp_test", |_| {
            Err(io::ErrorKind::NotFound.into())
        })?;
        let replacement = assets
            .replacement_for_language(&mut config)?
            .expect("fallback to English");
        assert_eq!(replacement.read_config("loc.cfg")?, "en");
        assert_eq!(config.get("loc_language"), Some("0"));
        assert_eq!(replacement.expected_hunk_usage("mp_test")?, Some(984));
        assert_eq!(replacement.expected_hunk_usage("unknown")?, None);
        Ok(())
    }

    #[test]
    fn newly_written_configs_follow_home_pack_precedence() -> io::Result<()> {
        let temp = tempfile::tempdir()?;
        let base = temp.path().join("base");
        let home = temp.path().join("home");
        std::fs::create_dir_all(base.join("main"))?;
        std::fs::write(base.join("main/local.cfg"), b"base")?;
        let assets = GameAssets::mount_paths(&base, Some(&home), "")?;
        assets.write_generated("local.cfg", b"first generated config")?;
        assert_eq!(assets.read_config("LOCAL.CFG")?, "first generated config");
        assert_eq!(
            std::fs::read(home.join("main/local.cfg"))?,
            b"first generated config"
        );
        assets.append_generated("scriptdata/Chat.csv", b"first\n")?;
        assets.append_generated("scriptdata/chat.csv", b"second\n")?;
        assert_eq!(assets.read_file("SCRIPTDATA/CHAT.CSV")?, b"first\nsecond\n");
        assert_eq!(
            std::fs::read(home.join("main/scriptdata/Chat.csv"))?,
            b"first\nsecond\n"
        );
        let mut cfg = ServerConfig::from_args(&[]);
        // The nested exec reads the exact state at the point of the preceding writeconfig.
        cfg.execute_tokens_files(&["set".into(),"cfg_macro".into(),
            "seta TestArchive first; writeconfig ExportMaps/Settings; set TestArchive second; exec ExportMaps/Settings".into()], &assets)?;
        cfg.execute_tokens_files(&["vstr".into(), "cfg_macro".into()], &assets)?;
        assert_eq!(cfg.get("TestArchive"), Some("first"));
        assert!(home.join("main/ExportMaps/Settings.cfg").is_file());

        assets.write_generated("generated/test.cfg", b"second generated config")?;
        assert_eq!(
            assets.read_config("generated/test.cfg")?,
            "second generated config"
        );
        fs::create_dir_all(home.join("main"))?;
        write_pack(
            &home.join("main"),
            "custom.iwd",
            &[("local.cfg", b"packed")],
        )?;
        let assets = GameAssets::mount_paths(&base, Some(&home), "")?;
        assets.write_generated("local.cfg", b"generated")?;
        assert_eq!(assets.read_config("local.cfg")?, "packed");
        Ok(())
    }

    #[test]
    fn home_and_mod_roots_override_in_engine_order() -> io::Result<()> {
        let root = tempfile::tempdir()?;
        let base = root.path().join("base");
        let home = root.path().join("home");
        for (dir, name, value) in [
            (&base, "Main", "base"),
            (&home, "main", "home"),
            (&base, "MyMod", "mod"),
            (&home, "myMOD", "user-mod"),
        ] {
            let dir = dir.join(name);
            fs::create_dir_all(&dir)?;
            fs::write(dir.join("config.cfg"), value)?;
            fs::write(dir.join(format!("{value}.cfg")), value)?;
        }
        write_pack(
            &base.join("MyMod"),
            "update.iwd",
            &[("base.cfg", b"patched")],
        )?;
        write_pack(
            &home.join("myMOD"),
            "localized_german_iw01.iwd",
            &[("home.cfg", b"localized")],
        )?;
        let assets = GameAssets::mount_paths(&base, Some(&home), "mymod")?;
        assert_eq!(assets.read_config("config.cfg")?, "user-mod");
        assert_eq!(assets.read_config("base.cfg")?, "patched");
        assert_eq!(assets.read_config("mod.cfg")?, "mod");
        assert_eq!(assets.read_config("home.cfg")?, "home");
        assert!(GameAssets::mount_paths(&base, Some(&home), "../escape").is_err());
        Ok(())
    }

    #[test]
    fn pack_checksums_and_reference_flags_follow_actual_map_reads() -> io::Result<()> {
        let root = tempfile::tempdir()?;
        let main = root.path().join("main");
        let mod_path = root.path().join("Mod");
        fs::create_dir(&main)?;
        fs::create_dir(&mod_path)?;
        write_pack(
            &main,
            "iw_00.iwd",
            &[
                ("z.cfg", b"not script"),
                ("maps/new.gsc", b"code here"),
                ("empty.gsc", b""),
                ("a.gsc", b"last script"),
            ],
        )?;
        write_pack(&main, "Update.IWD", &[("maps/new.gsc", b"new script")])?;
        write_pack(&mod_path, "clientmod.iwd", &[("model", b"assets")])?;
        let mut assets = GameAssets::mount_paths(root.path(), None, "Mod")?;
        // Expected values from folding OpenSSL MD4 of the nonempty entry CRCs.
        assert_eq!(
            assets.loaded_pure_checksums(-374),
            [1672836226, 893318358, 719883434]
        );
        assert_eq!(
            assets.iwd_info(true)[0].1,
            "-1664753553 -1736588878 -1053808612 "
        );
        assert_eq!(assets.iwd_info(true)[1].1, "clientmod Update iw_00");
        assert_eq!(assets.read_config("z.cfg")?, "not script");
        assert_eq!(assets.iwd_info(true)[3].1, "Mod/clientmod");
        assert_eq!(assets.read_script("maps/new.gsc")?, b"new script");
        assert_eq!(assets.iwd_info(false)[0].1, "");
        assert_eq!(assets.iwd_info(false)[2].1, "-1664753553 -1736588878 ");
        let before = assets.iwd_info(true);
        assert!(
            assets
                .with_map_references(|a| {
                    a.read_script("a.gsc")?;
                    a.read_script("missing.gsc")
                })
                .is_err()
        );
        assert_eq!(assets.iwd_info(true), before);
        let failed_batch = assets.with_reference_transaction(|a| {
            a.with_map_references(|active| active.read_script("a.gsc"))?;
            a.with_map_references(|next| next.read_script("missing-second-map.gsc"))
        });
        assert!(failed_batch.is_err());
        assert_eq!(assets.iwd_info(true), before);
        assets.with_map_references(|a| a.read_script("a.gsc"))?;
        assert_eq!(assets.iwd_info(true)[3].1, "Mod/clientmod main/iw_00");
        Ok(())
    }

    #[test]
    fn rejects_wrong_version_and_invalid_lump_extents_or_sizes() {
        let mut image = map_fixture();
        for (at, value) in [(4, 3u32), (8, 71), (12, 8), (12, u32::MAX), (8, 0)] {
            let original = image[at..at + 4].to_vec();
            image[at..at + 4].copy_from_slice(&value.to_le_bytes());
            assert!(ServerMap::parse(image.clone()).is_err());
            image[at..at + 4].copy_from_slice(&original);
        }
        assert!(ServerMap::parse(image[..HEADER_SIZE - 1].to_vec()).is_err());
    }

    #[test]
    fn folds_md4_digest_in_little_endian_order() {
        // RFC 1320's "abc" digest, folded as four little-endian words.
        assert_eq!(
            block_checksum(b"abc"),
            0xa448017au32.swap_bytes()
                ^ 0xaf21d852u32.swap_bytes()
                ^ 0x5fc10ae8u32.swap_bytes()
                ^ 0x7aa6729du32.swap_bytes()
        );
    }
}
