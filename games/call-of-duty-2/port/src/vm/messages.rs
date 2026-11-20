//! GScr_BuildMessageString (0x08110720). The client resolves the localized
//! references from its own language files when it receives the marked text.
use super::value::{Value, error};
use crate::config::{ServerConfig, integer_prefix};
use std::io;

const LOCALIZED: u8 = 0x14;
const LITERAL: u8 = 0x15;
const ESCAPES: std::ops::RangeInclusive<u8> = 0x14..=0x16;

pub fn build(
    args: &[Value],
    label: &str,
    capacity: usize,
    config: &mut ServerConfig,
    mut player_name: impl FnMut(u32) -> Option<Vec<u8>>,
) -> io::Result<Vec<u8>> {
    let mut out = Vec::new();
    let mut start_literal = true;
    for value in args {
        let (text, marker) = match value {
            Value::LocalizedString(text) => {
                let text = cstr(text);
                if text.len() > 1 && !text.iter().all(|c| c.is_ascii_alphanumeric() || *c == b'_') {
                    return Err(error("illegal localized string reference"));
                }
                start_literal = true;
                (text.to_vec(), (!out.is_empty()).then_some(LOCALIZED))
            }
            Value::Object(id) => {
                let name = player_name(*id).ok_or_else(|| error("Entity is not a player"))?;
                let text = [cstr(&name), b"^7"].concat();
                let marker = start_literal.then_some(LITERAL);
                start_literal = false;
                (text, marker)
            }
            _ => {
                let text = value.script_string()?;
                let text = cstr(&text);
                // The binary stops inspecting the string at the first letter or escape.
                if let Some(&byte) = text
                    .iter()
                    .find(|&b| b.is_ascii_alphabetic() || ESCAPES.contains(b))
                {
                    if ESCAPES.contains(&byte) {
                        return Err(error(format!(
                            "bad escape character ({byte}) present in string"
                        )));
                    }
                    let text = String::from_utf8_lossy(text);
                    if config
                        .get("loc_warningsAsErrors")
                        .is_some_and(|s| integer_prefix(s) != 0)
                    {
                        return Err(error(format!(
                            "non-localized {label} strings are not allowed to have letters in them: \"{text}\""
                        )));
                    }
                    config.messages.push(format!("^3WARNING: Non-localized {label} string is not allowed to have letters in it. Must be changed over to a localized string: \"{text}\""));
                }
                let marker = start_literal.then_some(LITERAL);
                start_literal = false;
                (text.to_vec(), marker)
            }
        };
        // The original reserves both a terminator and a marker even when this
        // particular argument does not actually need a marker.
        if text.len().saturating_add(out.len()).saturating_add(1) >= capacity {
            return Err(error(format!(
                "{label} is too long. Max length is {capacity}"
            )));
        }
        out.extend(marker);
        out.extend(
            text.into_iter()
                .map(|b| if ESCAPES.contains(&b) { b'.' } else { b }),
        );
    }
    Ok(out)
}

fn cstr(bytes: &[u8]) -> &[u8] {
    bytes.split(|&b| b == 0).next().unwrap_or_default()
}

/// The original debug function table marks these commands as developer-only.
pub(super) fn developer_call(
    name: &str,
    args: &[Value],
    config: &ServerConfig,
    entities: &super::game_entities::GameEntities,
) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "print"
            | "println"
            | "print3d"
            | "line"
            | "assert"
            | "assertex"
            | "assertmsg"
            | "getentbynum"
    ) {
        return None;
    }
    let arg = |n: usize| {
        args.get(n)
            .ok_or_else(|| error(format!("parameter {} does not exist", n + 1)))
    };
    let int = |n: usize| match arg(n)? {
        Value::Int(value) => Ok(*value),
        value => Err(error(format!("type {} is not an int", value.type_name()))),
    };
    let vector = |n: usize| match arg(n)? {
        Value::Vector(_) => Ok(()),
        value => Err(error(format!("type {} is not a vector", value.type_name()))),
    };
    Some((|| {
        match name {
            "print" | "println" => {
                if config
                    .get("g_no_script_spam")
                    .is_none_or(|s| integer_prefix(s) == 0)
                {
                    use std::io::Write;
                    let mut stdout = io::stdout().lock();
                    for value in args {
                        stdout.write_all(cstr(&value.script_string()?))?;
                    }
                    if name == "println" {
                        stdout.write_all(b"\n")?;
                    }
                }
            }
            "print3d" => {
                if !matches!(args.len(), 2..=4) {
                    arg(4)?.number()?;
                }
                if args.len() >= 4 {
                    arg(3)?.number()?;
                }
                if args.len() >= 3 {
                    vector(2)?;
                }
                arg(1)?.script_string()?;
                vector(0)?;
            }
            "line" => {} // GScr_line is empty on the dedicated server.
            "assert" if int(0)? == 0 => return Err(error("assert fail")),
            "assertex" if int(0)? == 0 => {
                return Err(error(format!(
                    "assert fail: {}",
                    String::from_utf8_lossy(&arg(1)?.script_string()?)
                )));
            }
            "assertmsg" => {
                return Err(error(format!(
                    "assert fail: {}",
                    String::from_utf8_lossy(&arg(0)?.script_string()?)
                )));
            }
            "getentbynum" => {
                return Ok(u16::try_from(int(0)?)
                    .ok()
                    .and_then(|i| entities.object_for_slot(i))
                    .map_or(Value::Undefined, Value::Object));
            }
            _ => {}
        }
        Ok(Value::Undefined)
    })())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn mixed_localized_player_and_literal_runs_preserve_color_and_boundaries() -> io::Result<()> {
        let mut config = ServerConfig::from_args(&[]);
        let args = [
            Value::LocalizedString(b"MP_JOIN".to_vec()),
            Value::Object(3),
            Value::String(b"42".to_vec()),
            Value::LocalizedString(b"TIME".to_vec()),
            Value::String(b"ok\x14".to_vec()),
        ];
        assert_eq!(
            build(&args, "Game Message", 1024, &mut config, |id| (id == 3)
                .then(|| b"^1A\x16B".to_vec()))?,
            b"MP_JOIN\x15^1A.B^742\x14TIME\x15ok."
        );
        assert_eq!(config.messages.len(), 1);
        assert!(
            build(
                &[Value::String(b"42\x14a".to_vec())],
                "Game Message",
                1024,
                &mut config,
                |_| None
            )
            .is_err()
        );
        let key = Value::LocalizedString(b"ABC".to_vec());
        assert!(build(std::slice::from_ref(&key), "", 4, &mut config, |_| None).is_err());
        assert_eq!(build(&[key], "", 5, &mut config, |_| None)?, b"ABC");
        Ok(())
    }
}
