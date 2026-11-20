use super::*;
use crate::vm::messages;

fn arg(args: &[Value], index: usize) -> io::Result<&Value> {
    args.get(index)
        .ok_or_else(|| invalid(format!("missing parameter {}", index + 1)))
}

fn message(host: &mut GameHost<'_>, args: &[Value], label: &str) -> io::Result<Vec<u8>> {
    messages::build(args, label, 1024, host.config, |object| {
        let slot = host.world.players.slot_for_object(object)?;
        Some(host.world.players.get(slot)?.name.clone())
    })
}

fn print_command(host: &mut GameHost<'_>, name: &str, args: &[Value]) -> io::Result<Vec<u8>> {
    // va() has two rotating buffers. Each player argument needs one va("%s^7")
    // in GScr_BuildMessageString. The final formatting call clears its output
    // buffer first, even if it still holds the command prefix.
    let mut prefix = vec![if name == "iprintlnbold" { b'g' } else { b'f' }];
    let mut next_reuses_prefix = false;
    let text = messages::build(args, "Game Message", 1024, host.config, |object| {
        let slot = host.world.players.slot_for_object(object)?;
        let name = &host.world.players.get(slot)?.name;
        if next_reuses_prefix {
            prefix.clone_from(name);
            prefix.extend(b"^7");
        }
        next_reuses_prefix = !next_reuses_prefix;
        Some(name.clone())
    })?;
    if next_reuses_prefix {
        prefix.clear();
    }
    Ok([prefix.as_slice(), b" \"", &text, b"\""].concat())
}

/// GScr_Shellshock (0x081162a2) scans the fifteen configstrings, including
/// empty entries, before changing the playerstate or starting an animation.
fn shellshock(slot: u16, args: &[Value], host: &mut GameHost<'_>) -> io::Result<Value> {
    let [name, seconds] = args else {
        return Err(invalid(
            "USAGE: <player> shellshock(<shellshockname>, <duration>)\n",
        ));
    };
    let name = name.script_string()?;
    let index = (1..16)
        .find(|&n| {
            host.strings
                .get(0x48e + n)
                .is_some_and(|s| s.eq_ignore_ascii_case(&name))
        })
        .ok_or_else(|| {
            invalid(format!(
                "shellshock '{}' was not precached\n",
                String::from_utf8_lossy(&name)
            ))
        })?;
    let millis = (seconds.number()? * 1000.0 + 0.5).floor();
    if !(0.0..=60000.0).contains(&millis) {
        return Err(invalid(format!(
            "duration {} should be >= 0 and <= 60",
            crate::vm::value::general_float(millis * 0.001)
        )));
    }
    let object = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("player required"))?
        .object;
    let alive = matches!(host.world.field(object, "health")?, Value::Int(n) if n > 0);
    let player = host.world.players.get_mut(slot)?;
    player.state.set_field("shellshockIndex", index as u32)?;
    player
        .state
        .set_field("shellshockTime", host.time_ms as u32)?;
    player
        .state
        .set_field("shellshockDuration", millis as u32)?;
    if alive {
        host.world.animations.event(
            &mut player.anim_conditions,
            &mut player.state,
            0x12,
            false,
            true,
            &mut host.world.native_random,
        );
        player
            .anim_conditions
            .play_sounds(&mut player.state, host.strings)?;
    }
    Ok(Value::Undefined)
}

pub(crate) fn dispatch(
    slot: u16,
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    if name == "shellshock" {
        return Some(shellshock(slot, args, host));
    }
    if name == "cloneplayer" {
        return Some(crate::vm::game_entities::clone_player(slot, args, host));
    }
    if name == "dropitem" {
        return Some(crate::vm::items::drop_item(slot, args, host));
    }

    if let Some(result) = scores::player_method(host, slot, name, args) {
        return Some(result);
    }
    if let Some(result) = spawn::dispatch(slot, name, args, host) {
        return Some(result);
    }
    let p = host.world.players.get_mut(slot).ok()?;
    if let Some(command) = crate::vm::sound::mix::player_command(name, args, host.strings) {
        return Some(command.map(|command| {
            p.commands.push(command);
            Value::Undefined
        }));
    }
    if let Some(result) = crate::combat::dispatch(
        name,
        args,
        p.session_state == 0,
        &p.name,
        &mut p.state,
        host.weapons,
        &mut p.commands,
    ) {
        return Some(result);
    }
    if let Some(result) = crate::vm::hud::menu_command(name, args, host.strings, p.connected) {
        return Some(result.map(|(ret, command)| {
            if let Some(command) = command {
                p.commands.push(command);
            }
            ret
        }));
    }
    match name {
        "istalking" => {
            let elapsed = host.time_ms.wrapping_sub(p.last_voice_time);
            let duration = host
                .config
                .get("g_voiceChatTalkingDuration")
                .and_then(|v| v.parse::<i32>().ok())
                .unwrap_or(500);
            Some(Ok(Value::Int(i32::from((0..duration).contains(&elapsed)))))
        }
        "playlocalsound" => Some(
            crate::vm::sound::sound_index(args, host.strings).map(|index| {
                p.commands.push(format!("s {index}").into_bytes());
                Value::Undefined
            }),
        ),
        "getentitynumber" => Some(Ok(Value::Int(i32::from(slot)))),
        "getguid" => Some(if args.is_empty() {
            Ok(Value::Int(p.guid))
        } else {
            Err(invalid("USAGE: self getGuid()"))
        }),
        "setentertime" => Some(arg(args, 0).and_then(integer).map(|n| {
            p.enter_time = n;
            Value::Undefined
        })),
        "allowspectateteam" => Some((|| {
            let team = arg(args, 0)?.script_string()?;
            let bit = match team.as_slice() {
                b"none" => 0,
                b"axis" => 1,
                b"allies" => 2,
                b"freelook" => 4,
                _ => return Err(invalid("invalid spectate team")),
            };
            if integer(arg(args, 1)?)? == 0 {
                p.no_spectate |= 1 << bit;
            } else {
                p.no_spectate &= !(1 << bit);
            }
            Ok(Value::Undefined)
        })()),
        "stopshellshock" => Some((|| {
            if !args.is_empty() {
                return Err(invalid("stopshellshock expects no arguments"));
            }
            for key in ["shellshockIndex", "shellshockTime", "shellshockDuration"] {
                p.state.set_field(key, 0)?;
            }
            Ok(Value::Undefined)
        })()),
        // Empty native method on the dedicated server (0x08111e9c).
        "setclientcvar" => Some((|| {
            let key = arg(args, 0)?.script_string()?;
            if !key.iter().all(|b| b.is_ascii_alphanumeric() || *b == b'_') {
                return Err(invalid("invalid client dvar name"));
            }
            let mut value = match arg(args, 1)? {
                Value::LocalizedString(_) => message(host, &args[1..], "Client Dvar Value")?,
                v => v.script_string()?,
            };
            value.truncate(
                value
                    .iter()
                    .position(|&b| b == 0)
                    .unwrap_or(value.len())
                    .min(1023),
            );
            for b in &mut value {
                if matches!(*b, b'"' | 0x92) {
                    *b = b'\'';
                }
            }
            let cmd = [b"v ".as_slice(), &key, b" \"", &value, b"\""].concat();
            host.world.players.get_mut(slot)?.commands.push(cmd);
            Ok(Value::Undefined)
        })()),
        "iprintln" | "iprintlnbold" => Some((|| {
            let cmd = print_command(host, name, args)?;
            host.world.players.get_mut(slot)?.commands.push(cmd);
            Ok(Value::Undefined)
        })()),
        _ => None,
    }
}

pub(crate) fn global(
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    if let Some(result) = scores::global(host, name, args) {
        return Some(result);
    }
    match name {
        "iprintln" | "iprintlnbold" => Some((|| {
            let cmd = print_command(host, name, args)?;
            host.strings.broadcast(cmd);
            Ok(Value::Undefined)
        })()),
        "logprint" => Some((|| {
            let mut line = Vec::new();
            for v in args {
                let part = v.script_string()?;
                if line.len() + part.len() > 1023 {
                    break;
                }
                line.extend(part);
            }
            host.world.players.game_log.push((host.time_ms, line));
            Ok(Value::Undefined)
        })()),
        "setclientnamemode" => Some((|| {
            host.world.players.manual_name_change = match arg(args, 0)?.script_string()?.as_slice()
            {
                b"auto_change" => false,
                b"manual_change" => true,
                _ => return Err(invalid("Unknown mode")),
            };
            Ok(Value::Undefined)
        })()),
        "updateclientnames" => Some(if !host.world.players.manual_name_change {
            Err(invalid("Only works in [manual_change] mode"))
        } else {
            for p in host
                .world
                .players
                .slots
                .values_mut()
                .filter(|p| p.connected)
            {
                p.publish_name();
            }
            Ok(Value::Undefined)
        }),
        "isalive" => Some((|| {
            let Value::Object(object) = arg(args, 0)? else {
                return Ok(Value::Int(0));
            };
            if host.world.entities.slot_for_object(*object).is_none() {
                return Ok(Value::Int(0));
            }
            let health = host.world.field(*object, "health")?;
            Ok(Value::Int(i32::from(
                matches!(health, Value::Int(n) if n > 0),
            )))
        })()),
        "positionwouldtelefrag" => Some((|| {
            let Value::Vector(origin) = arg(args, 0)? else {
                return Err(invalid("positionWouldTelefrag expects a vector"));
            };
            let [x, y, z] = *origin;
            let slots = crate::vm::game_entities::touch::area_entities(
                &host.world.entities,
                host.collision,
                [x - 15., y - 15., z],
                [x + 15., y + 15., z + 70.],
                0x0200_0000,
            );
            let occupied = slots.iter().any(|&slot| {
                host.world
                    .players
                    .get(slot)
                    .is_some_and(|p| (p.state.field("pm_type").unwrap_or(0) as i32) < 6)
            });
            Ok(Value::Int(i32::from(occupied)))
        })()),
        "isplayer" => Some(Ok(Value::Int(i32::from(
            matches!(args.first(),Some(Value::Object(o)) if host.world.players.slot_for_object(*o).is_some()),
        )))),
        "isplayernumber" => Some(
            arg(args, 0)
                .and_then(integer)
                .map(|n| Value::Int(i32::from((0..64).contains(&n)))),
        ),
        "getteamscore" | "setteamscore" => Some((|| {
            let i = match arg(args, 0)?.script_string()?.as_slice() {
                b"axis" => 0,
                b"allies" => 1,
                _ => return Err(invalid("expected axis or allies")),
            };
            let players = &mut host.world.players;
            if name == "getteamscore" {
                return Ok(Value::Int(players.team_scores[i]));
            }
            let score = integer(arg(args, 1)?)?;
            players.team_scores[i] = score;
            players.scores_dirty = true;
            let cmd = format!("{} {score}", if i == 0 { 'G' } else { 'H' }).into_bytes();
            host.strings.broadcast(cmd);
            Ok(Value::Undefined)
        })()),
        _ => None,
    }
}
