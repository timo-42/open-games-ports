//! Server script methods for use-trigger interaction, ownership and cursor hints.
use crate::{
    level::ConfigStrings,
    vm::{
        GameHost, invalid, messages,
        value::{Value, script_error},
    },
};
use std::io;

pub(super) fn cursor_hint(bytes: &[u8], inherit: bool) -> Option<u32> {
    if inherit && bytes.eq_ignore_ascii_case(b"HINT_INHERIT") {
        return Some(u32::MAX);
    }
    [
        b"HINT_NONE".as_slice(),
        b"HINT_ACTIVATE",
        b"HINT_HEALTH",
        b"HINT_FRIENDLY",
    ]
    .iter()
    .position(|hint| bytes.eq_ignore_ascii_case(hint))
    .map(|i| i as u32 + 1)
}

pub(crate) fn hint_index(strings: &mut ConfigStrings, text: &[u8]) -> io::Result<u8> {
    for slot in 0x4fe..0x51e {
        let current = strings.get(slot).expect("hint string range");
        if current.is_empty() {
            strings.set(slot, text)?;
            return Ok((slot - 0x4fe) as u8);
        }
        if current == text {
            return Ok((slot - 0x4fe) as u8);
        }
    }
    Err(invalid(
        "Too many different hintstring values. Max allowed is 32 different strings",
    ))
}

pub fn dispatch(
    receiver: u32,
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "setteamfortrigger"
            | "clientclaimtrigger"
            | "clientreleasetrigger"
            | "releaseclaimedtrigger"
            | "setcursorhint"
            | "sethintstring"
    ) {
        return None;
    }
    Some(apply(receiver, name, args, host))
}

fn apply(receiver: u32, name: &str, args: &[Value], host: &mut GameHost<'_>) -> io::Result<Value> {
    let arg = || {
        args.first()
            .ok_or_else(|| script_error("parameter 1 does not exist"))
    };
    let client_method = matches!(name, "clientclaimtrigger" | "clientreleasetrigger");
    let (target, client) = if client_method {
        let player = host
            .world
            .players
            .slot_for_object(receiver)
            .and_then(|s| host.world.players.get(s))
            .ok_or_else(|| script_error(format!("{name}: caller must be a client.")))?;
        let Value::Object(target) = arg()? else {
            return Err(script_error("expected trigger entity"));
        };
        (
            *target,
            player.state.field("clientNum").expect("clientNum") as u16,
        )
    } else {
        (receiver, 1023)
    };
    let slot = host
        .world
        .entities
        .slot_for_object(target)
        .ok_or_else(|| script_error("not a live entity"))?;
    let kind = host.world.field(target, "classname")?;
    let use_trigger =
        matches!(&kind, Value::String(s) if s==b"trigger_use" || s==b"trigger_use_touch");
    if !use_trigger && name != "setcursorhint" {
        return Err(script_error(format!(
            "{name}: trigger entity must be of type trigger_use or trigger_use_touch"
        )));
    }
    let processing = host.world.entities.get_mut(slot).expect("live trigger");
    match name {
        "setteamfortrigger" => {
            processing.trigger_team = match arg()?.script_string()?.as_slice() {
                b"axis" => 1,
                b"allies" => 2,
                b"none" => 0,
                _ => {
                    return Err(script_error(
                        "setteamfortrigger: invalid team used must be allies, axis or none",
                    ));
                }
            }
        }
        "clientclaimtrigger" => {
            if processing.trigger_claim == 1023 || processing.trigger_claim == client {
                processing.trigger_claim = client;
            }
        }
        "clientreleasetrigger" => {
            if processing.trigger_claim == client {
                processing.trigger_claim = 1023;
            }
        }
        "releaseclaimedtrigger" => processing.trigger_claim = 1023,
        "setcursorhint" => {
            let value = arg()?.script_string()?;
            let id = cursor_hint(&value, use_trigger).ok_or_else(|| {
                script_error(format!(
                    "{} is not a valid hint type",
                    String::from_utf8_lossy(&value)
                ))
            })?;
            processing.state.set_field("dmgFlags", id);
        }
        "sethintstring" => {
            let index = if matches!(arg()?,Value::String(text) if text.is_empty()) {
                255
            } else {
                let players = &host.world.players;
                let text = messages::build(args, "Hint String", 1024, host.config, |id| {
                    players
                        .slot_for_object(id)
                        .and_then(|s| players.get(s))
                        .map(|p| p.name.clone())
                })?;
                hint_index(host.strings, &text).map_err(|error| script_error(error.to_string()))?
            };
            processing.state.set_field("scale", u32::from(index));
        }
        _ => unreachable!(),
    }
    Ok(Value::Undefined)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn hint_indices_share_the_live_weapon_table_and_deduplicate() -> io::Result<()> {
        let mut strings = ConfigStrings::default();
        strings.set(0x4fe, b"USE_WEAPON")?;
        assert_eq!(hint_index(&mut strings, b"MP_TARGET")?, 1);
        assert_eq!(hint_index(&mut strings, b"MP_TARGET")?, 1);
        assert_eq!(hint_index(&mut strings, b"USE_WEAPON")?, 0);
        assert_eq!(cursor_hint(b"hint_inherit", true), Some(u32::MAX));
        assert_eq!(cursor_hint(b"HINT_INHERIT", false), None);
        assert_eq!(cursor_hint(b"HINT_ACTIVATE", false), Some(2));
        Ok(())
    }
}
