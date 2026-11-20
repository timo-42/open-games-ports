//! Dvar console commands (Dvar_Command, Dvar_List_f and the handlers in Dvar_Init).
use super::*;
use std::fmt::Write as _;

impl Domain {
    pub(super) fn description(self) -> String {
        let (kind, lo, hi) = match self {
            Self::Bool => return "  Domain is 0 or 1".into(),
            Self::Text => return "  Domain is any text".into(),
            Self::Color => return "  Domain is any 4-component color, in RGBA format".into(),
            Self::Integer(lo, hi) => (
                "integer",
                (lo != i32::MIN).then(|| lo.to_string()),
                (hi != i32::MAX).then(|| hi.to_string()),
            ),
            Self::Float(lo, hi) => (
                "number",
                (lo != f32::MIN).then(|| general_float(lo)),
                (hi != f32::MAX).then(|| general_float(hi)),
            ),
        };
        let mut text = format!("  Domain is any {kind}");
        match (lo, hi) {
            (Some(lo), Some(hi)) => write!(text, " from {lo} to {hi}"),
            (Some(lo), None) => write!(text, " {lo} or bigger"),
            (None, Some(hi)) => write!(text, " {hi} or smaller"),
            (None, None) => Ok(()),
        }
        .expect("writing to String");
        text
    }
}

// Com_Filter accepts a prefix. A literal run following '*' is located using
// Com_FindTailMatch with Q_stricmp (whole-string equality on each suffix).
pub(crate) fn dvar_filter(pattern: &str, name: &str) -> bool {
    let mut pattern = pattern.as_bytes();
    let mut name = name.as_bytes();
    while let Some((&token, rest)) = pattern.split_first() {
        pattern = rest;
        if token == b'*' {
            let end = pattern
                .iter()
                .position(|c| matches!(c, b'*' | b'?'))
                .unwrap_or(pattern.len());
            if end != 0 {
                let Some(suffix) = name.get(name.len().saturating_sub(end)..) else {
                    return false;
                };
                if !suffix.eq_ignore_ascii_case(&pattern[..end]) {
                    return false;
                }
                name = &[];
            }
            pattern = &pattern[end..];
            continue;
        }
        let Some((&ch, remaining)) = name.split_first() else {
            return false;
        };
        name = remaining;
        match token {
            b'?' => {}
            b'[' => {
                // Com_Filter skips repeated opening brackets before entering the class.
                while pattern.first() == Some(&b'[') {
                    pattern = &pattern[1..];
                }
                let mut matched = false;
                while let Some((&first, rest)) = pattern.split_first() {
                    pattern = rest;
                    if first == b']' && rest.first() != Some(&b']') {
                        break;
                    }
                    if let [b'-', last, tail @ ..] = rest
                        && (*last != b']' || tail.first() == Some(&b']'))
                    {
                        matched |= (first.to_ascii_uppercase()..=last.to_ascii_uppercase())
                            .contains(&ch.to_ascii_uppercase());
                        pattern = tail;
                    } else {
                        matched |= first.eq_ignore_ascii_case(&ch);
                    }
                }
                if !matched {
                    return false;
                }
            }
            literal if literal.eq_ignore_ascii_case(&ch) => {}
            _ => return false,
        }
    }
    true
}

impl ServerConfig {
    pub(in crate::config) fn inspect_dvar(&mut self, name: &str) {
        let key = name.to_ascii_lowercase();
        let entry = &self.definitions[&key];
        let value = &self.values[&key];
        self.messages.push(format!(
            "\"{}\" is: \"{value}^7\" default: \"{}^7\"",
            entry.name, entry.reset
        ));
        if let Some(value) = self.latched.get(&key) {
            self.messages.push(format!("latched: \"{value}\""));
        }
        self.messages.push(entry.domain.description());
    }

    fn list_dvars(&mut self, filter: Option<&String>, dump: bool) {
        if dump {
            if self.get("logfile").unwrap_or("0") == "0" {
                return;
            }
            self.messages.push("=============================== DVAR DUMP ========================================".into());
        }
        for (key, entry) in &self.definitions {
            if filter.is_some_and(|f| !dvar_filter(f, &entry.name)) {
                continue;
            }
            let name = &entry.name;
            let value = &self.values[key];
            let line = if dump {
                let mut line = format!("      {name} \"{value}\"");
                if let Some(pending) = self.latched.get(key) {
                    write!(line, " -- latched \"{pending}\"").expect("writing to String");
                }
                line
            } else {
                let flags: String = [
                    (SERVER_INFO, 'S'),
                    (2, 'U'),
                    (READ_ONLY, 'R'),
                    (INIT, 'I'),
                    (1, 'A'),
                    (LATCH, 'L'),
                    (CHEAT, 'C'),
                ]
                .iter()
                .map(|(mask, c)| if entry.flags & mask != 0 { *c } else { ' ' })
                .collect();
                format!("{flags} {name} \"{value}\"")
            };
            self.messages.push(line);
        }
        self.messages
            .push(format!("\n{} total dvars", self.definitions.len()));
        if dump {
            self.messages
                .push(format!("{} dvar indexes", self.definitions.len()));
            self.messages.push("=============================== END DVAR DUMP =====================================".into());
        }
    }

    /// Complete contents of Com_WriteConfigToFile / Com_WriteDefaultsToFile.
    pub fn archived_cfg(&self, defaults: bool) -> Vec<u8> {
        let mut text = String::from("// generated by Call of Duty, do not modify\n");
        for (key, entry) in &self.definitions {
            if key == "cl_cdkey" {
                continue;
            }
            if defaults {
                if entry.flags & (EXTERNAL | READ_ONLY | CHEAT) == 0 {
                    writeln!(text, "set {} \"{}\"", entry.name, entry.reset)
                        .expect("writing to String");
                }
            } else if entry.flags & 1 != 0 {
                let value = self.latched.get(key).unwrap_or(&self.values[key]);
                writeln!(text, "seta {} \"{value}\"", entry.name).expect("writing to String");
            }
        }
        text.into_bytes()
    }

    fn toggle(&mut self, words: &[String], print: bool) {
        let Some(name) = words.get(1) else {
            self.messages.push(format!(
                "USAGE: {} <variable> <optional value sequence>",
                words[0]
            ));
            return;
        };
        let key = name.to_ascii_lowercase();
        let Some(entry) = self.definitions.get(&key) else {
            self.messages
                .push(format!("toggle failed: dvar '{name}' not found."));
            return;
        };
        let current = &self.values[&key];
        let value = if let Some(first) = words.get(2) {
            words[2..]
                .windows(2)
                .find(|pair| current.eq_ignore_ascii_case(&pair[0]))
                .map_or(first, |p| &p[1])
                .clone()
        } else {
            match entry.domain {
                Domain::Bool => i32::from(current == "0").to_string(),
                Domain::Integer(lo, hi) => {
                    let n = integer_prefix(current);
                    if lo <= 0 && hi >= 1 {
                        i32::from(n == 0)
                    } else if n == lo {
                        hi
                    } else {
                        lo
                    }
                    .to_string()
                }
                Domain::Float(lo, hi) => {
                    let n = float_prefix(current).0;
                    let next = if lo <= 0. && hi >= 1. {
                        if n == 0. { 1. } else { 0. }
                    } else if n == lo {
                        hi
                    } else {
                        lo
                    };
                    general_float(next)
                }
                _ => {
                    self.messages.push(format!(
                        "'toggle' with no arguments makes no sense for dvar '{}'",
                        entry.name
                    ));
                    return;
                }
            }
        };
        self.set_from_command(name, &value);
        if print {
            self.messages
                .push(format!("{name} toggled to {}", self.values[&key]));
        }
    }

    fn register_from_command(&mut self, command: &str, words: &[String]) {
        let boolean = command == "dvar_bool";
        if words.len() != if boolean { 3 } else { 5 } {
            let bounds = if boolean { "" } else { " <min> <max>" };
            self.messages
                .push(format!("USAGE: {} <name> <default>{bounds}", words[0]));
            return;
        }
        let name = &words[1];
        let (domain, reset, bounds) = match command {
            "dvar_int" => {
                let lo = integer_prefix(&words[3]);
                let hi = integer_prefix(&words[4]);
                (
                    Domain::Integer(lo, hi),
                    integer_prefix(&words[2]).to_string(),
                    (lo > hi).then(|| (lo.to_string(), hi.to_string())),
                )
            }
            "dvar_float" => {
                let lo = float_prefix(&words[3]).0;
                let hi = float_prefix(&words[4]).0;
                (
                    Domain::Float(lo, hi),
                    general_float(float_prefix(&words[2]).0),
                    (lo > hi).then(|| (general_float(lo), general_float(hi))),
                )
            }
            _ => (
                Domain::Bool,
                i32::from(integer_prefix(&words[2]) != 0).to_string(),
                None,
            ),
        };
        if let Some((lo, hi)) = bounds {
            self.messages.push(format!(
                "dvar {name}: min {lo} should not be greater than max {hi}"
            ));
            return;
        }
        let key = name.to_ascii_lowercase();
        if let Some(entry) = self.definitions.get(&key)
            && !(matches!(entry.domain, Domain::Text) && entry.flags & EXTERNAL != 0)
        {
            if std::mem::discriminant(&entry.domain) != std::mem::discriminant(&domain) {
                let kind = if boolean { "a boolean" } else { "an integer" };
                self.messages
                    .push(format!("dvar '{}' is not {kind} dvar", entry.name));
            }
            return;
        }
        let value = self
            .get(name)
            .and_then(|s| domain.parse(s, true))
            .unwrap_or_else(|| reset.clone());
        self.set(name, &value);
        let entry = self
            .definitions
            .get_mut(&key)
            .expect("set creates registration");
        entry.reset = reset.into();
        entry.domain = domain;
    }

    pub(in crate::config) fn dvar_command(&mut self, command: &str, words: &[String]) -> bool {
        match command {
            "dvar_bool" | "dvar_int" | "dvar_float" => self.register_from_command(command, words),
            "dvarlist" | "dvardump" => self.list_dvars(words.get(1), command == "dvardump"),
            "toggle" | "togglep" => self.toggle(words, command == "togglep"),
            "reset" if words.len() == 2 => {
                if let Some(reset) = self
                    .definitions
                    .get(&words[1].to_ascii_lowercase())
                    .map(|entry| entry.reset.clone())
                {
                    self.set_from_command(&words[1], reset.as_ref());
                }
            }
            "reset" => self.messages.push("USAGE: reset <variable>".into()),
            "setfromdvar" if words.len() == 3 => {
                if let Some(value) = self.get(&words[2]).map(str::to_owned) {
                    self.set_from_command(&words[1], &value);
                } else {
                    self.messages
                        .push(format!("dvar '{}' doesn't exist", words[2]));
                }
            }
            "setfromdvar" => self
                .messages
                .push("USAGE: setfromdvar <dest_dvar> <source_dvar>".into()),
            _ => return false,
        }
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cmd(cfg: &mut ServerConfig, text: &str) {
        cfg.execute(text, |_| Err(std::io::ErrorKind::NotFound.into()))
            .expect("execute fixture console commands");
    }

    #[test]
    fn typed_console_registration_toggles_and_inspection() {
        let mut cfg = ServerConfig::from_args(&[]);
        cmd(
            &mut cfg,
            "seta ProbeA 900; dvar_int probEa 13 4 20; ProbeA; togglep ProbeA; togglep ProbeA; set ProbeA 30junk",
        );
        assert_eq!(
            cfg.messages,
            [
                "\"ProbeA\" is: \"20^7\" default: \"13^7\"",
                "  Domain is any integer from 4 to 20",
                "ProbeA toggled to 4",
                "ProbeA toggled to 20",
                "'30' is not a valid value for dvar 'ProbeA'",
                "  Domain is any integer from 4 to 20",
            ]
        );
        cmd(
            &mut cfg,
            "dvar_float ProbeB 1.2 1.1 1.8; togglep ProbeB; togglep ProbeB; dvar_bool ProbeC 0; togglep ProbeC; setfromdvar ProbeD ProbeA; togglep ProbeD 20 next; reset ProbeD",
        );
        assert_eq!(cfg.get("probeB"), Some("1.8"));
        assert_eq!(cfg.get("probeC"), Some("1"));
        assert_eq!(cfg.get("probeD"), Some("20"));
        cfg.messages.clear();
        cmd(&mut cfg, "dvarlist Probe[a-c]");
        assert_eq!(
            &cfg.messages[..3],
            [
                "    A   ProbeA \"20\"",
                "        ProbeB \"1.8\"",
                "        ProbeC \"1\""
            ]
        );
        assert_eq!(cfg.messages.len(), 4);
        assert!(!dvar_filter("*obe*", "ProbeA"));
        assert!(dvar_filter("*obeA", "ProbeA"));
    }

    #[test]
    fn config_files_preserve_latches_spelling_and_reset_flags() {
        let mut cfg = ServerConfig::from_args(&[]);
        cmd(
            &mut cfg,
            "seta MyName Third; seta cl_cdkey secret; set sv_maxclients 12",
        );
        let archive = String::from_utf8(cfg.archived_cfg(false)).expect("UTF-8 archived settings");
        let defaults = String::from_utf8(cfg.archived_cfg(true)).expect("UTF-8 default settings");
        assert!(!cfg.take_archive_dirty()); // pending latch and flags alone do not autosave
        cmd(&mut cfg, "seta MyName fourth");
        assert!(cfg.take_archive_dirty());
        assert!(!cfg.take_archive_dirty());
        assert!(archive.starts_with("// generated by Call of Duty, do not modify\n"));
        assert!(archive.contains("seta MyName \"Third\"\n"));
        assert!(archive.contains("seta sv_maxclients \"12\"\n"));
        assert!(!archive.contains("cl_cdkey"));
        assert!(defaults.contains("set sv_maxclients \"20\"\n"));
        assert!(!defaults.contains("MyName"));
        cmd(&mut cfg, "reset sv_maxclients");
        assert!(!cfg.latched.contains_key("sv_maxclients"));
    }
}
