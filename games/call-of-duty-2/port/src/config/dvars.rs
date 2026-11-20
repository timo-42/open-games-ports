//! Registrations, value domains and write protection from Dvar_Register / Dvar_SetVariant.
mod commands;
use super::{ServerConfig, integer_prefix, registration};
use crate::vm::value::general_float;
use std::borrow::Cow;

pub(crate) const SERVER_INFO: u16 = 0x404;
pub(crate) const SCRIPT_ADVERTISED: u16 = 0x400;
const EXTERNAL: u16 = 0x4000;
pub(crate) const SYSTEM_INFO: u16 = 8;
const INIT: u16 = 0x10;
const LATCH: u16 = 0x20;
const READ_ONLY: u16 = 0x40;
const CHEAT: u16 = 0x80;
pub(crate) const SCRIPT_INFO: u16 = 0x100;

#[derive(Clone, Copy, Debug)]
pub(super) enum Domain {
    Bool,
    Text,
    Integer(i32, i32),
    Float(f32, f32),
    Color,
}

#[derive(Clone, Copy, Debug)]
pub(super) struct Definition(
    pub(super) &'static str,
    pub(super) &'static str,
    pub(super) Domain,
    pub(super) u16,
);

#[derive(Clone, Debug)]
pub(super) struct Registered {
    name: Cow<'static, str>,
    reset: Cow<'static, str>,
    domain: Domain,
    flags: u16,
}

impl Registered {
    pub(super) fn name(&self) -> &str {
        &self.name
    }
    pub(super) fn is_string(&self) -> bool {
        matches!(self.domain, Domain::Text)
    }

    pub(super) fn is_archived(&self) -> bool {
        self.flags & 1 != 0
    }

    pub(super) fn external(name: &str, value: &str) -> Self {
        Self {
            name: Cow::Owned(name.to_owned()),
            reset: Cow::Owned(value.to_owned()),
            domain: Domain::Text,
            flags: EXTERNAL,
        }
    }
}

impl Domain {
    fn parse(self, value: &str, clamp: bool) -> Option<String> {
        match self {
            Self::Text => Some(value.into()),
            Self::Bool => Some(i32::from(integer_prefix(value) != 0).to_string()),
            Self::Integer(min, max) => {
                let n = integer_prefix(value);
                (clamp || (min..=max).contains(&n)).then(|| n.clamp(min, max).to_string())
            }
            Self::Float(min, max) => {
                let (n, _) = float_prefix(value);
                (clamp || (min..=max).contains(&n)).then(|| general_float(n.clamp(min, max)))
            }
            Self::Color => {
                // sscanf("%g %g %g %g") starts with four zeroes and stops at
                // the first failed conversion. The type stores four bytes.
                let mut channels = Vec::with_capacity(4);
                let mut rest = value;
                for _ in 0..4 {
                    let (n, next) = float_prefix(rest);
                    rest = next;
                    let byte = (n.clamp(0., 1.) * 255.).round() as u8;
                    channels.push(general_float(f32::from(byte) * (1. / 255.)));
                }
                Some(channels.join(" "))
            }
        }
    }
}

// atof/sscanf consume leading whitespace and a decimal prefix.
pub(crate) fn float_prefix(value: &str) -> (f32, &str) {
    let text = value.trim_ascii_start();
    let b = text.as_bytes();
    let sign = usize::from(matches!(b.first(), Some(b'+' | b'-')));
    let mut i = sign;
    while b.get(i).is_some_and(u8::is_ascii_digit) {
        i += 1;
    }
    let integer = i > sign;
    if b.get(i) == Some(&b'.') {
        i += 1;
        let start = i;
        while b.get(i).is_some_and(u8::is_ascii_digit) {
            i += 1;
        }
        if !integer && i == start {
            return (0., text);
        }
    } else if !integer {
        return (0., text);
    }
    if matches!(b.get(i), Some(b'e' | b'E')) {
        let marker = i;
        i += 1;
        if matches!(b.get(i), Some(b'+' | b'-')) {
            i += 1;
        }
        let start = i;
        while b.get(i).is_some_and(u8::is_ascii_digit) {
            i += 1;
        }
        if i == start {
            i = marker;
        }
    }
    (text[..i].parse::<f64>().unwrap_or(0.) as f32, &text[i..])
}

fn default_value(def: &Definition) -> String {
    match def.0 {
        "fs_basepath" => std::env::current_dir()
            .ok()
            .map(|p| p.to_string_lossy().into_owned())
            .unwrap_or_else(|| ".".into()),
        "fs_homepath" => super::default_homepath(),
        "username" => std::env::var("USER")
            .or_else(|_| std::env::var("USERNAME"))
            .unwrap_or_else(|_| "player".into()),
        "arch" => format!("{} {}", std::env::consts::OS, std::env::consts::ARCH),
        _ => def.1.to_owned(),
    }
}

impl ServerConfig {
    pub(super) fn register(&mut self, definitions: &'static [Definition]) {
        for def in definitions {
            let key = def.0.to_ascii_lowercase();
            let cheats = self.get("sv_cheats") == Some("1");
            if let Some(entry) = self.definitions.get_mut(&key)
                && entry.flags & EXTERNAL == 0
            {
                entry.flags |= def.3;
                if entry.flags & CHEAT != 0 && !cheats {
                    let reset = entry.reset.to_string();
                    self.set(&key, &reset);
                    self.latched.remove(&key);
                } else if entry.flags & LATCH != 0
                    && let Some(value) = self.latched.remove(&key)
                {
                    self.set(&key, &value);
                }
                continue;
            }
            // Dvar_Redefine ORs the incoming flags when a user-created
            // startup string becomes a native dvar. Dvar_Create doesn't.
            if self.definitions.contains_key(&key) && def.3 & 1 != 0 {
                self.archive_dirty = true;
            }
            let default = default_value(def);
            let inherited_flags = self
                .definitions
                .get(&key)
                .map_or(0, |entry| entry.flags & !EXTERNAL);
            let value = if def.3 & READ_ONLY != 0 || def.3 & CHEAT != 0 && !cheats {
                default
            } else {
                self.get(&key)
                    .and_then(|v| def.2.parse(v, true))
                    .unwrap_or(default)
            };
            self.set(&key, &value);
            self.definitions.insert(
                key,
                Registered {
                    name: Cow::Borrowed(def.0),
                    reset: Cow::Owned(default_value(def)),
                    domain: def.2,
                    flags: def.3 | inherited_flags,
                },
            );
        }
    }

    pub(super) fn registered_native(&self, key: &str) -> bool {
        self.definitions
            .get(key)
            .is_some_and(|entry| entry.flags & EXTERNAL == 0)
    }

    pub(crate) fn add_flags(&mut self, name: &str, flags: u16) {
        if let Some(entry) = self.definitions.get_mut(&name.to_ascii_lowercase())
            && entry.flags & flags != flags
        {
            entry.flags |= flags;
            self.info_dirty = true;
        }
    }

    pub(super) fn clear_script_serverinfo(&mut self) {
        for entry in self.definitions.values_mut() {
            entry.flags &= !SCRIPT_ADVERTISED;
        }
    }

    pub(super) fn register_server(&mut self) {
        if self.registered_native("sv_serverid") {
            return;
        }
        self.register(registration::SERVER);
        self.set(
            "version",
            &format!(
                "CoD2 MP 1.3 build {}-{}",
                std::env::consts::OS,
                std::env::consts::ARCH
            ),
        );
    }

    pub fn init_console_input(&mut self) {
        self.register(registration::CONSOLE);
        self.set("ttycon", "0"); // Sys_ConsoleInputInit disables terminal editing in lnxded.
    }

    pub(crate) fn register_game(&mut self) {
        self.register_server();
        self.register(registration::SPAWN);
        self.register(registration::GAME);
    }

    pub(crate) fn register_game_dvars_only(&mut self) {
        self.register(registration::GAME);
    }

    pub(crate) fn register_game_if_needed(&mut self) {
        if !self.registered_native("g_gravity") {
            self.register_game();
        }
    }

    /// Actual dynamic system values are visible to getCvar as well as the wire.
    pub fn set_network_state(&mut self, map: &str, server_id: u8, packages: &[(&str, String)]) {
        self.set("mapname", map);
        self.set("sv_serverid", &server_id.to_string());
        for (name, value) in packages {
            self.set(name, value);
        }
    }

    pub(crate) fn system_defaults() -> impl Iterator<Item = (&'static str, &'static str)> {
        registration::EARLY
            .iter()
            .chain(registration::SERVER)
            .chain(registration::GAME)
            .filter(|def| def.3 & SYSTEM_INFO != 0)
            .map(|def| (def.0, def.1))
    }

    pub fn dvars_with_flags(&self, mask: u16) -> impl Iterator<Item = (&str, &str)> {
        self.definitions
            .iter()
            .filter(move |(_, entry)| entry.flags & mask != 0)
            .filter_map(|(key, entry)| Some((entry.name.as_ref(), self.values.get(key)?.as_str())))
    }

    pub(super) fn set_typed(&mut self, name: &str, value: &str, from_command: bool) {
        let key = name.to_ascii_lowercase();
        let Some(entry) = self.definitions.get(&key) else {
            self.set(name, value);
            return;
        };
        let spelling = &entry.name;
        let Some(value) = entry.domain.parse(value, false) else {
            let value = match entry.domain {
                Domain::Integer(..) => integer_prefix(value).to_string(),
                Domain::Float(..) => general_float(float_prefix(value).0),
                _ => value.to_owned(),
            };
            self.messages.push(format!(
                "'{value}' is not a valid value for dvar '{}'",
                spelling
            ));
            self.messages.push(entry.domain.description());
            return;
        };
        if from_command {
            let reason = if entry.flags & READ_ONLY != 0 {
                Some("read only")
            } else if entry.flags & INIT != 0 {
                Some("write protected")
            } else if entry.flags & CHEAT != 0 && self.get("sv_cheats") != Some("1") {
                Some("cheat protected")
            } else {
                None
            };
            if let Some(reason) = reason {
                self.messages.push(format!("{spelling} is {reason}."));
                return;
            }
            if entry.flags & LATCH != 0 {
                if self.get(name) == Some(&value) {
                    self.latched.remove(&key);
                } else {
                    self.latched.insert(key, value);
                    self.messages
                        .push(format!("{spelling} will be changed upon restarting."));
                }
                return;
            }
        }
        self.latched.remove(&key);
        self.set(name, &value);
    }
}
