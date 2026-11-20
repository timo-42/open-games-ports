//! Dedicated-server startup path from Com_ParseCommandLine, Com_StartupVariable,
//! Com_ExecuteServerConfig, Cbuf_Execute and SV_MapRotate_f in the 1.3 reconstruction.
mod dvars;
mod registration;
use crate::query::tokenize;
use dvars::{Definition, Domain, Registered};
pub(crate) use dvars::{SCRIPT_ADVERTISED, SERVER_INFO};
pub(crate) use dvars::{SCRIPT_INFO, float_prefix};
use std::{collections::BTreeMap, io};

const BUFFER_SIZE: usize = 0x10000;
const MAX_COMMAND_LEN: usize = 0xfff;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ServerEvent {
    LoadMap {
        name: String,
        previous_cheats: bool,
        cheats: bool,
    },
    Heartbeat,
    GameCompleteStatus,
    Operator(Vec<String>),
    RestartMap {
        fast: bool,
    },
    Quit,
}

/// Cmd_Exec_f and Cmd_Vstr_f print to the current console and insert text for a later Cbuf_Execute.
#[derive(Debug)]
pub struct CommandInsertion {
    pub message: Option<String>,
    pub text: Option<String>,
    source: String,
}

impl CommandInsertion {
    pub fn expand(
        self,
        sources: &mut Vec<String>,
        run: impl FnOnce(&str, &mut Vec<String>) -> io::Result<()>,
    ) -> io::Result<()> {
        let Some(text) = self.text else { return Ok(()) };
        if sources.contains(&self.source) || sources.len() >= 64 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("recursive command expansion at {}", self.source),
            ));
        }
        sources.push(self.source);
        let result = run(&text, sources);
        sources.pop();
        result
    }
}

/// Configuration commands read from the mounted source profile and write to the user game directory.
pub trait CommandFiles {
    fn read(&mut self, path: &str) -> io::Result<String>;
    fn write(&mut self, path: &str, contents: &[u8]) -> io::Result<()>;
}

impl<F: FnMut(&str) -> io::Result<String>> CommandFiles for F {
    fn read(&mut self, path: &str) -> io::Result<String> {
        self(path)
    }

    fn write(&mut self, _: &str, _: &[u8]) -> io::Result<()> {
        Err(io::ErrorKind::ReadOnlyFilesystem.into())
    }
}

#[derive(Clone, Debug)]
pub struct ServerConfig {
    /// Optional native test seed applied on every map load, outside the dvar table.
    pub game_seed: Option<u32>,
    /// Oracle minimum wall time for a full reload after the initial map.
    pub map_load_ms: Option<u32>,
    /// SV_ValidateGametype uses the running map's saved name for a script
    /// transition that requests persistence, unless map rotation changes it.
    pub retained_gametype: Option<String>,
    /// SV_AddBot's counter persists across maps and dedicated server shutdowns.
    pub(crate) bot_connect_number: i32,
    pub update_package: Option<std::path::PathBuf>,
    info_dirty: bool,
    archive_dirty: bool,
    definitions: BTreeMap<String, Registered>,
    startup_lines: Vec<String>,
    gametypes: Vec<String>,
    pub values: BTreeMap<String, String>,
    latched: BTreeMap<String, String>,
    pub map: Option<String>,
    pub events: Vec<ServerEvent>,
    pub messages: Vec<String>,
}

/// Linux uses the dedicated executable's Sys_DefaultHomePath. The native Win64
/// and Apple Silicon builds use their platform's per-user application data.
pub(super) fn default_homepath() -> String {
    let Some(dirs) = directories::BaseDirs::new() else {
        return String::new();
    };
    #[cfg(any(windows, target_os = "macos"))]
    let path = dirs
        .data_local_dir()
        .join("Open Games")
        .join("Call of Duty 2");
    #[cfg(not(any(windows, target_os = "macos")))]
    let path = dirs.home_dir().join(".callofduty2");
    path.to_string_lossy().into_owned()
}

/// COM_DefaultExtension uses a 64-byte path and examines bytes after the first one.
fn config_filename(name: &str) -> String {
    let mut filename = name[..name.floor_char_boundary(63)].to_owned();
    let has_extension = filename
        .bytes()
        .enumerate()
        .rev()
        .take_while(|&(i, byte)| i > 0 && byte != b'/')
        .any(|(_, byte)| byte == b'.');
    if !has_extension {
        filename.push_str(".cfg");
        filename.truncate(filename.floor_char_boundary(63));
    }
    filename
}

impl ServerConfig {
    /// Native options precede the stock command line. A later token inside a
    /// +set or +exec command is part of that command and retains its spelling.
    pub fn from_command_line(args: &[String]) -> io::Result<Self> {
        let mut seed = None;
        let mut map_load_ms = None;
        let mut update = None;
        let mut commands = args;
        while let Some(option) = commands.first() {
            match option.as_str() {
                "--game-seed" => {
                    seed = Some(
                        commands
                            .get(1)
                            .and_then(|v| v.parse().ok())
                            .ok_or_else(|| {
                                io::Error::new(
                                    io::ErrorKind::InvalidInput,
                                    "--game-seed requires a decimal u32",
                                )
                            })?,
                    );
                }
                "--update" => {
                    let path = commands
                        .get(1)
                        .filter(|v| !v.is_empty() && !v.starts_with('+'))
                        .ok_or_else(|| {
                            io::Error::new(
                                io::ErrorKind::InvalidInput,
                                "--update requires a 1.3 package path",
                            )
                        })?;
                    update = Some(std::path::PathBuf::from(path));
                }
                "--map-load-ms" => {
                    map_load_ms = Some(commands.get(1).and_then(|v| v.parse().ok()).ok_or_else(
                        || {
                            io::Error::new(
                                io::ErrorKind::InvalidInput,
                                "--map-load-ms requires a decimal u32",
                            )
                        },
                    )?);
                }
                _ => break,
            }
            commands = &commands[2..];
        }
        let mut config = Self::from_args(commands);
        config.game_seed = seed;
        config.map_load_ms = map_load_ms;
        config.update_package = update;
        Ok(config)
    }

    /// The Linux executable joins argv[1..] with spaces before splitting at
    /// '+' or LF, even inside quotes. Last of 32 lines retains the unsplit tail.
    pub fn from_args(args: &[String]) -> Self {
        let startup_lines = args
            .join(" ")
            .splitn(32, ['+', '\n'])
            .map(str::to_owned)
            .collect();
        let mut config = Self {
            game_seed: None,
            map_load_ms: None,
            retained_gametype: None,
            bot_connect_number: 0,
            update_package: None,
            info_dirty: true,
            archive_dirty: false,
            definitions: BTreeMap::new(),
            startup_lines,
            gametypes: Vec::new(),
            values: BTreeMap::new(),
            latched: BTreeMap::new(),
            map: None,
            events: Vec::new(),
            messages: Vec::new(),
        };
        // Com_ParseCommandLine -> Com_StartupVariable precedes FS_InitFilesystem.
        config.register(&registration::EARLY[..1]);
        config.startup_variables();
        config.register(&registration::EARLY[1..]);
        if config.get("dedicated") != Some("0") && config.get("viewlog") == Some("0") {
            config.set("viewlog", "1");
        }
        config
    }

    pub fn get(&self, name: &str) -> Option<&str> {
        self.values
            .get(&name.to_ascii_lowercase())
            .map(String::as_str)
    }

    /// Script names enumerated from the active game search path by GScr_LoadGametypeList.
    pub fn set_gametypes(&mut self, names: Vec<String>) {
        self.gametypes = names;
    }

    fn set(&mut self, name: &str, value: &str) {
        let key = name.to_ascii_lowercase();
        let entry = self
            .definitions
            .entry(key.clone())
            .or_insert_with(|| Registered::external(name, value));
        if self.values.get(&key).is_none_or(|v| v != value) {
            self.archive_dirty |= entry.is_archived();
            self.values.insert(key, value.to_owned());
            self.info_dirty = true;
        }
    }

    pub fn take_archive_dirty(&mut self) -> bool {
        std::mem::take(&mut self.archive_dirty)
    }

    pub fn take_info_dirty(&mut self) -> bool {
        std::mem::take(&mut self.info_dirty)
    }

    // SV_AddServerCvars registers these with DVAR_FLAG_LATCH. Starting the
    // next map re-registers them and Dvar_Update applies the pending values.
    fn set_from_command(&mut self, name: &str, value: &str) {
        self.set_typed(name, value, true);
    }

    pub(crate) fn set_from_script(&mut self, name: &str, value: &str) {
        self.set_typed(name, value, false);
    }

    pub(crate) fn has_gametype(&self, name: &str) -> bool {
        self.gametypes
            .iter()
            .any(|kind| kind.eq_ignore_ascii_case(name))
    }

    fn startup_variables(&mut self) {
        for index in 0..self.startup_lines.len() {
            let words = command_words(self.startup_lines[index].as_bytes());
            if words.first().is_some_and(|cmd| {
                cmd.eq_ignore_ascii_case("set") || cmd.eq_ignore_ascii_case("seta")
            }) {
                self.assign(&words);
            }
        }
    }

    fn assign(&mut self, words: &[String]) {
        if words.len() < 3 {
            self.messages
                .push(format!("USAGE: {} <variable> <value>", words[0]));
            return;
        }
        let name = &words[1];
        if !name.bytes().all(|c| c.is_ascii_alphanumeric() || c == b'_') {
            self.messages.push(format!("invalid variable name: {name}"));
            return;
        }
        self.set_from_command(name, &words[2..].join(" "));
        match words[0].to_ascii_lowercase().as_str() {
            "seta" => self.add_flags(name, 1),
            "setu" => self.add_flags(name, 2),
            "sets" => self.add_flags(name, 4),
            _ => {}
        }
    }

    /// File names handed to the callback are virtual game paths. At least one
    /// of the two stock default configurations is required by FS_InitFilesystem.
    pub fn startup(&mut self, read: impl FnMut(&str) -> io::Result<String>) -> io::Result<()> {
        self.startup_files(read)
    }

    pub fn startup_files(&mut self, mut read: impl CommandFiles) -> io::Result<()> {
        let commands = self.initialize_common(&mut read)?;
        self.execute_text(&commands, &mut read, &mut Vec::new())
    }

    /// Execute the early configuration while initializing the common subsystem.
    /// The remaining command line runs after networking and the live server start.
    pub fn initialize_common(&mut self, read: &mut impl CommandFiles) -> io::Result<String> {
        let mut sources = Vec::new();
        for filename in ["default_localize_mp.cfg", "default_mp.cfg"] {
            if let Some(text) = read_optional(read, filename)?
                && !text.is_empty()
            {
                self.messages.push(format!("execing {filename}"));
                self.execute_text(&text, read, &mut sources)?;
                break;
            }
            if filename == "default_mp.cfg" {
                return Err(io::Error::new(
                    io::ErrorKind::NotFound,
                    "missing stock default_localize_mp.cfg and default_mp.cfg",
                ));
            }
        }
        for filename in ["language.cfg", "config_mp_server.cfg"] {
            self.exec_file(filename, read, &mut sources)?;
        }
        if let Some(line) = self.startup_lines.iter_mut().find(|line| {
            command_words(line.as_bytes()).first().is_some_and(|cmd| {
                cmd.eq_ignore_ascii_case("safe") || cmd.eq_ignore_ascii_case("dvar_restart")
            })
        }) {
            line.clear();
            self.exec_file("safemode_mp_server.cfg", read, &mut sources)?;
        }
        self.startup_variables();
        self.archive_dirty = false; // Com_InitHunkMemory clears the archive bit.
        self.register_server();
        Ok(self.startup_lines.join("\n"))
    }

    /// Execute command-buffer text, also used by local operator commands after startup.
    pub fn execute(
        &mut self,
        text: &str,
        mut read: impl FnMut(&str) -> io::Result<String>,
    ) -> io::Result<()> {
        if !self.registered_native("sv_serverid") {
            self.register_game();
        }
        self.execute_text(text, &mut read, &mut Vec::new())
    }

    pub fn command_insertion(
        &self,
        words: &[String],
        files: &mut impl CommandFiles,
    ) -> io::Result<Option<CommandInsertion>> {
        let Some(command) = words.first() else {
            return Ok(None);
        };
        let lower = command.to_ascii_lowercase();
        if !matches!(lower.as_str(), "exec" | "vstr") {
            return Ok(None);
        }
        let mut insertion = CommandInsertion {
            message: None,
            text: None,
            source: String::new(),
        };
        if words.len() != 2 {
            insertion.message = Some(
                if lower == "exec" {
                    "exec <filename> : execute a script file"
                } else {
                    "vstr <variablename> : execute a variable command"
                }
                .into(),
            );
        } else if lower == "exec" {
            let filename = config_filename(&words[1]);
            insertion.source = format!("exec {}", filename.to_ascii_lowercase());
            insertion.text = read_optional(files, &filename)?;
            insertion.message = Some(if insertion.text.is_some() {
                format!("execing {filename}")
            } else {
                format!("couldn't exec {}", words[1])
            });
        } else {
            let key = words[1].to_ascii_lowercase();
            insertion.source = format!("vstr {key}");
            if let Some(definition) = self.definitions.get(&key) {
                if definition.is_string() {
                    insertion.text = self.values.get(&key).cloned();
                } else {
                    insertion.message =
                        Some(format!("{} is not a string-based dvar", definition.name()));
                }
            } else {
                insertion.message = Some(format!("{} doesn't exist", words[1]));
            }
        }
        Ok(Some(insertion))
    }

    fn exec_file<F>(
        &mut self,
        filename: &str,
        read: &mut F,
        sources: &mut Vec<String>,
    ) -> io::Result<()>
    where
        F: CommandFiles,
    {
        self.dispatch(&["exec".into(), filename.to_owned()], read, sources)
    }

    fn execute_text<F>(
        &mut self,
        text: &str,
        read: &mut F,
        sources: &mut Vec<String>,
    ) -> io::Result<()>
    where
        F: CommandFiles,
    {
        let Some(commands) = command_buffer(text) else {
            self.messages.push("Cbuf_InsertText overflowed".into());
            return Ok(());
        };
        for words in commands {
            if self.events.last() == Some(&ServerEvent::Quit) {
                break;
            }
            self.dispatch(&words, read, sources)?;
        }
        Ok(())
    }

    /// Dispatch already-tokenized input as one command (SV_RemoteCommand).
    pub fn execute_tokens(
        &mut self,
        words: &[String],
        read: impl FnMut(&str) -> io::Result<String>,
    ) -> io::Result<()> {
        self.execute_tokens_files(words, read)
    }

    pub fn execute_tokens_files(
        &mut self,
        words: &[String],
        mut read: impl CommandFiles,
    ) -> io::Result<()> {
        if !self.registered_native("sv_serverid") {
            self.register_game();
        }
        self.dispatch(words, &mut read, &mut Vec::new())
    }

    fn dispatch<F>(
        &mut self,
        words: &[String],
        read: &mut F,
        sources: &mut Vec<String>,
    ) -> io::Result<()>
    where
        F: CommandFiles,
    {
        let Some(command) = words.first() else {
            return Ok(());
        };
        let lower = command.to_ascii_lowercase();
        if self.dvar_command(&lower, words) {
            return Ok(());
        }
        match lower.as_str() {
            "set" | "seta" | "sets" | "setu" => self.assign(words),
            "exec" | "vstr" => {
                if let Some(mut insertion) = self.command_insertion(words, read)? {
                    if let Some(message) = insertion.message.take() {
                        self.messages.push(message);
                    }
                    insertion.expand(sources, |text, sources| {
                        self.execute_text(text, read, sources)
                    })?;
                }
            }
            "map" | "devmap" => {
                if let Some(name) = words.get(1).filter(|s| !s.is_empty()) {
                    self.select_map(name, command.eq_ignore_ascii_case("devmap"));
                }
            }
            "heartbeat" => self.events.push(ServerEvent::Heartbeat),
            "gamecompletestatus" => self.events.push(ServerEvent::GameCompleteStatus),
            "map_rotate" => self.rotate(),
            "map_restart" | "fast_restart" => {
                self.restart(command.eq_ignore_ascii_case("fast_restart"))
            }
            "writeconfig" | "writedefaults" => {
                if words.len() == 2 {
                    let filename = config_filename(&words[1]);
                    let contents = self.archived_cfg(command.eq_ignore_ascii_case("writedefaults"));
                    self.messages.push(format!("Writing {filename}."));
                    if read.write(&filename, &contents).is_err() {
                        self.messages.push(format!("Couldn't write {filename}."));
                    }
                } else {
                    self.messages.push(format!(
                        "Usage: {} <filename>",
                        command.to_ascii_lowercase()
                    ));
                }
            }
            "kick" | "onlykick" | "clientkick" | "tempbanuser" | "tempbanclient" | "banuser"
            | "banclient" | "unbanuser" | "dumpuser" | "systeminfo" | "status" | "serverinfo"
            | "say" | "tell" | "killserver" | "scriptusage" | "stringusage" => {
                self.events.push(ServerEvent::Operator(words.to_vec()));
            }
            "quit" => {
                self.events.push(ServerEvent::Quit);
                return Ok(());
            }
            // These client key bindings in default_mp.cfg have no handler
            // in the dedicated server command table.
            "bind" | "unbindall" => {}
            _ if self.get(command).is_some() => {
                if words.len() > 1 {
                    self.set_from_command(command, &words[1..].join(" "));
                } else {
                    self.inspect_dvar(command);
                }
            }
            _ => self
                .messages
                .push(format!("Unsupported dedicated-server command: {command}")),
        }
        Ok(())
    }

    // SV_ValidateGametype re-registers g_gametype (applying pending changes),
    // folds it to lower case, and compares it to the script registry.
    fn apply_map_settings(&mut self, fast: bool) {
        if fast {
            // SV_ValidateGametype re-registers only g_gametype on the fast path.
            self.register(&registration::SPAWN[..1]);
        } else {
            self.register_game();
        }
        let retained = self.retained_gametype.take();
        let requested = retained
            .as_deref()
            .or_else(|| self.get("g_gametype"))
            .unwrap_or("dm");
        let kind = requested[..requested.floor_char_boundary(63)].to_ascii_lowercase();
        if self.has_gametype(&kind) {
            self.set("g_gametype", &kind);
        } else {
            self.messages.push(format!(
                "g_gametype {kind} is not a valid gametype, defaulting to dm"
            ));
            self.set("g_gametype", "dm");
        }
    }

    fn select_map(&mut self, argument: &str, cheats: bool) {
        let previous_cheats = self.get("sv_cheats") == Some("1");
        let lower = argument.to_ascii_lowercase();
        let mut name = lower.strip_prefix("maps/mp/").unwrap_or(&lower);
        if name.ends_with("bsp") && name.len() >= 7 {
            name = &name[..name.floor_char_boundary(name.len() - 7)];
        }
        let name = name[..name.floor_char_boundary(63)]
            .replace('%', "_")
            .replace(['\\', ':'], "/");
        self.apply_map_settings(false);
        self.clear_script_serverinfo();
        self.set("nextmap", "map_restart");
        self.events.push(ServerEvent::LoadMap {
            name: name.clone(),
            previous_cheats,
            cheats,
        });
        self.set("mapname", &name);
        self.set("sv_running", "1");
        self.register(registration::AFTER_MAP);
        self.map = Some(name);
        self.set_map_cheats(cheats);
    }

    pub fn set_map_cheats(&mut self, enabled: bool) {
        self.set("sv_cheats", if enabled { "1" } else { "0" });
    }

    fn restart(&mut self, fast: bool) {
        if self.map.is_none() {
            self.messages.push("Server is not running.".into());
            return;
        }
        self.apply_map_settings(fast);
        if !fast {
            self.clear_script_serverinfo();
        }
        self.events.push(ServerEvent::RestartMap { fast });
    }

    fn rotate(&mut self) {
        self.messages.push(format!(
            "map_rotate...\n\n\"sv_mapRotation\" is:\"{}\"\n\n\"sv_mapRotationCurrent\" is:\"{}\"\n",
            self.get("sv_maprotation").unwrap_or_default(),
            self.get("sv_maprotationcurrent").unwrap_or_default(),
        ));
        let mut rotation = self
            .get("sv_maprotationcurrent")
            .unwrap_or_default()
            .trim_start()
            .to_owned();
        if rotation.is_empty() {
            rotation = self.get("sv_maprotation").unwrap_or_default().to_owned();
        }
        let mut rest = rotation.as_str();
        while let Some(keyword) = rotation_token(&mut rest) {
            if keyword.eq_ignore_ascii_case("map") {
                if let Some(map) = rotation_token(&mut rest) {
                    self.messages.push(format!("Setting map: {map}."));
                    self.select_map(map, false);
                    self.set("sv_maprotationcurrent", rest);
                    return;
                }
                self.messages.push(
                    "No map specified after 'map' keyword in sv_mapRotation - forcing map_restart."
                        .into(),
                );
                self.set("sv_maprotationcurrent", "");
                self.restart(true);
                return;
            }
            if keyword.eq_ignore_ascii_case("gametype") {
                if let Some(kind) = rotation_token(&mut rest) {
                    if self
                        .get("g_gametype")
                        .is_some_and(|old| !old.eq_ignore_ascii_case(kind))
                    {
                        self.retained_gametype = None;
                    }
                    self.messages.push(format!("Setting g_gametype: {kind}."));
                    self.latched.remove("g_gametype");
                    self.set("g_gametype", kind);
                } else {
                    self.messages.push("No gametype specified after 'gametype' keyword in sv_mapRotation - forcing map_restart.".into());
                    self.set("sv_maprotationcurrent", "");
                    self.restart(true);
                    return;
                }
            } else {
                self.messages
                    .push(format!("Unknown keyword '{keyword}' in sv_mapRotation."));
            }
        }
        self.set("sv_maprotationcurrent", "");
        self.messages
            .push("No map specified in sv_mapRotation - forcing map_restart.".into());
        self.restart(true);
    }
}

/// Cbuf command splitting runs before tokenization and comment handling.
pub fn command_buffer(text: &str) -> Option<impl Iterator<Item = Vec<String>> + '_> {
    let mut remaining = text
        .as_bytes()
        .split(|b| *b == 0)
        .next()
        .unwrap_or_default();
    if remaining.len() >= BUFFER_SIZE {
        return None;
    }
    Some(std::iter::from_fn(move || {
        if remaining.is_empty() {
            return None;
        }
        let mut quoted = false;
        let end = remaining
            .iter()
            .take(MAX_COMMAND_LEN)
            .position(|&c| {
                if c == b'"' {
                    quoted = !quoted;
                }
                c == b'\r' || c == b'\n' || (c == b';' && !quoted)
            })
            .unwrap_or(remaining.len().min(MAX_COMMAND_LEN));
        let words = command_words(&remaining[..end]);
        remaining = remaining.get(end + 1..).unwrap_or_default();
        Some(words)
    }))
}

pub fn integer_prefix(text: &str) -> i32 {
    let text = text.trim_ascii_start();
    let negative = text.starts_with('-');
    let digits = text.strip_prefix(['-', '+']).unwrap_or(text);
    digits
        .bytes()
        .take_while(u8::is_ascii_digit)
        .fold(0i32, |n, digit| {
            let digit = i32::from(digit - b'0');
            n.saturating_mul(10)
                .saturating_add(if negative { -digit } else { digit })
        })
}

fn command_words(line: &[u8]) -> Vec<String> {
    tokenize(line)
        .into_iter()
        .map(|word| String::from_utf8_lossy(&word).into_owned())
        .collect()
}

fn rotation_token<'a>(rest: &mut &'a str) -> Option<&'a str> {
    let text = rest.trim_start_matches(|ch: char| ch.is_ascii_whitespace());
    if text.is_empty() {
        return None;
    }
    if let Some(quoted) = text.strip_prefix('"') {
        let (value, tail) = quoted.split_once('"').unwrap_or((quoted, ""));
        *rest = tail;
        return Some(value);
    }
    let end = text
        .find(|ch: char| ch.is_ascii_whitespace())
        .unwrap_or(text.len());
    *rest = &text[end..];
    Some(&text[..end])
}

fn read_optional(read: &mut impl CommandFiles, path: &str) -> io::Result<Option<String>> {
    match read.read(path) {
        Ok(text) => Ok(Some(text)),
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(None),
        Err(error) => Err(error),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn missing(_: &str) -> io::Result<String> {
        Err(io::ErrorKind::NotFound.into())
    }

    #[test]
    fn deterministic_seed_is_separate_from_the_stock_dvar_table() {
        let args = [
            "--game-seed",
            "4294967295",
            "+set",
            "sv_hostname",
            "--game-seed",
        ]
        .map(str::to_owned);
        let config = ServerConfig::from_command_line(&args).expect("valid native seed");
        assert_eq!(config.game_seed, Some(u32::MAX));
        assert_eq!(config.get("sv_hostname"), Some("--game-seed"));
        assert!(config.get("game_seed").is_none());
        assert!(ServerConfig::from_command_line(&args[..1]).is_err());
        assert!(ServerConfig::from_command_line(&["--game-seed".into(), "-1".into()]).is_err());
        assert_eq!(
            ServerConfig::from_command_line(&[])
                .expect("unseeded")
                .game_seed,
            None
        );
    }

    #[test]
    fn update_path_keeps_spaces_and_can_precede_seed() -> io::Result<()> {
        let args = [
            "--update",
            "/game data/patch.tar.bz2",
            "--game-seed",
            "23",
            "--map-load-ms",
            "1150",
            "+set",
            "sv_hostname",
            "local",
        ]
        .map(str::to_owned);
        let config = ServerConfig::from_command_line(&args)?;
        assert_eq!(
            config.update_package.as_deref(),
            Some(std::path::Path::new("/game data/patch.tar.bz2"))
        );
        assert_eq!(config.game_seed, Some(23));
        assert_eq!(config.map_load_ms, Some(1150));
        assert!(config.get("map_load_ms").is_none());
        assert_eq!(config.get("sv_hostname"), Some("local"));
        assert!(ServerConfig::from_command_line(&args[..1]).is_err());
        Ok(())
    }

    #[test]
    fn startup_uses_early_variables_defaults_then_commands_in_order() {
        let mut config = ServerConfig::from_args(&[
            "+set".into(),
            "fs_basepath".into(),
            "/games/cod2".into(),
            "+exec".into(),
            "lan".into(),
            "+set".into(),
            "SV_hostname".into(),
            "final name".into(),
            "+devmap".into(),
            "maps/mp/MP_TOuJANE.d3dbsp".into(),
        ]);
        assert_eq!(config.get("FS_BasePath"), Some("/games/cod2"));
        let mut reads = Vec::new();
        config
            .startup(|name| {
                reads.push(name.to_owned());
                match name {
                    "default_mp.cfg" => Ok("set sv_hostname stock; set language fallback".into()),
                    "language.cfg" => Ok("set language english".into()),
                    "lan.cfg" => Ok("sets MyInfo \"hello; world\"; set sv_hostname lan".into()),
                    _ => missing(name),
                }
            })
            .expect("valid startup files");
        assert_eq!(
            reads,
            [
                "default_localize_mp.cfg",
                "default_mp.cfg",
                "language.cfg",
                "config_mp_server.cfg",
                "lan.cfg"
            ]
        );
        assert_eq!(config.get("SV_HOSTNAME"), Some("final name"));
        assert_eq!(config.get("language"), Some("english"));
        assert_eq!(config.get("myinfo"), Some("hello; world"));
        assert!(
            config
                .dvars_with_flags(SERVER_INFO)
                .any(|(name, value)| name == "MyInfo" && value == "hello; world")
        );
        assert_eq!(config.map.as_deref(), Some("mp_toujane"));
        assert_eq!(config.get("sv_cheats"), Some("1"));
    }

    #[test]
    fn nested_commands_run_in_place_and_rotation_wraps() {
        let mut config = ServerConfig::from_args(&[]);
        config.set_gametypes(vec!["dm".into(), "sd".into()]);
        config.execute("exec first; map_rotate; map_rotate; map_rotate", |path| match path {
            "first.cfg" => Ok("exec nested.cfg; vstr rotationcfg".into()),
            "nested.cfg" => Ok("set rotationcfg \"set sv_mapRotation gametype sd map mp_a gametype dm map mp_b\"".into()),
            _ => missing(path)
        }).expect("valid nested config");
        assert_eq!(
            config.events,
            [
                ServerEvent::LoadMap {
                    name: "mp_a".into(),
                    previous_cheats: false,
                    cheats: false
                },
                ServerEvent::LoadMap {
                    name: "mp_b".into(),
                    previous_cheats: false,
                    cheats: false
                },
                ServerEvent::LoadMap {
                    name: "mp_a".into(),
                    previous_cheats: false,
                    cheats: false
                }
            ]
        );
        assert_eq!(
            config.get("sv_maprotationcurrent"),
            Some(" gametype dm map mp_b")
        );
        assert_eq!(config.get("g_gametype"), Some("sd"));
    }

    #[test]
    fn map_load_validates_configured_and_latched_gametypes_against_script_registry() {
        let mut config = ServerConfig::from_args(&["+map mp_toujane".into()]);
        config.set_gametypes(vec!["dm".into(), "custom_mode".into()]);
        config
            .startup(|path| {
                if path == "default_localize_mp.cfg" {
                    Ok("set g_gametype random".into())
                } else {
                    missing(path)
                }
            })
            .expect("load stock settings");
        assert_eq!(config.get("g_gametype"), Some("dm"));
        assert!(
            config
                .messages
                .iter()
                .any(|s| s == "g_gametype random is not a valid gametype, defaulting to dm")
        );
        config
            .execute("set g_gametype CUSTOM_MODE", missing)
            .expect("latch");
        assert_eq!(config.get("g_gametype"), Some("dm"));
        config
            .execute("map_restart", missing)
            .expect("activate custom mode");
        assert_eq!(config.get("g_gametype"), Some("custom_mode"));
    }

    #[test]
    fn changing_maxclients_after_map_load_waits_for_restart() {
        let mut config = ServerConfig::from_args(&[
            "+set sv_maxclients 8 +map mp_a +set sv_maxclients 16".into(),
        ]);
        config
            .startup(|name| {
                if name == "default_mp.cfg" {
                    Ok("// stock".into())
                } else {
                    missing(name)
                }
            })
            .expect("startup");
        assert_eq!(config.get("sv_maxclients"), Some("8"));
        config.execute("map_restart", missing).expect("restart");
        assert_eq!(config.get("sv_maxclients"), Some("16"));
    }

    #[test]
    fn tokenized_console_commands_preserve_punctuation_and_typed_values() {
        let mut config = ServerConfig::from_args(&[]);
        config
            .execute_tokens(
                &[
                    "set".into(),
                    "sv_hostname".into(),
                    "Honest; +map mp_x".into(),
                ],
                missing,
            )
            .expect("single command");
        config
            .execute(
                "set sv_privateclients +07slots; sv_voice 2; sv_maxclients 65; g_antilag false",
                missing,
            )
            .expect("typed commands");
        assert_eq!(config.get("sv_hostname"), Some("Honest; +map mp_x"));
        assert_eq!(config.get("sv_voice"), Some("1"));
        assert_eq!(config.get("sv_privateclients"), Some("7"));
        assert_eq!(config.get("sv_maxclients"), Some("20"));
        assert_eq!(config.get("g_antilag"), Some("0"));
        assert!(config.map.is_none());
    }

    #[test]
    fn missing_stock_defaults_and_recursion_report_errors() {
        let mut config = ServerConfig::from_args(&[]);
        assert_eq!(
            config
                .startup(missing)
                .expect_err("stock config required")
                .kind(),
            io::ErrorKind::NotFound
        );
        let error = config
            .execute("exec cyclic", |_| Ok("exec cyclic".into()))
            .expect_err("cycle");
        assert_eq!(error.kind(), io::ErrorKind::InvalidData);
    }

    #[test]
    fn malformed_and_unknown_commands_report_diagnostics_and_continue() {
        let mut config = ServerConfig::from_args(&[]);
        config.set_gametypes(vec!["dm".into()]);
        config.execute("devmap mp_a; set bad-name x; unknown; sets Bla hello // comment\r\nmap_restart; fast_restart; quit; map late", missing)
            .expect("recoverable errors");
        assert_eq!(
            config.messages,
            [
                "invalid variable name: bad-name",
                "Unsupported dedicated-server command: unknown"
            ]
        );
        assert_eq!(config.get("Bla"), Some("hello"));
        assert_eq!(
            config.events,
            [
                ServerEvent::LoadMap {
                    name: "mp_a".into(),
                    previous_cheats: false,
                    cheats: true
                },
                ServerEvent::RestartMap { fast: false },
                ServerEvent::RestartMap { fast: true },
                ServerEvent::Quit
            ]
        );
        assert_eq!(config.map.as_deref(), Some("mp_a"));
    }
}
