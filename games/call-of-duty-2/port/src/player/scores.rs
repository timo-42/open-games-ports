use super::*;
use crate::{query::InfoString, vm::messages};

pub(crate) fn send_death_scores(players: &mut Players, victim: u16) {
    let packet = commands::scoreboard(players);
    for (_, p) in players.iter_mut().filter(|(_, p)| {
        p.connected && p.session_state == 2 && p.last_followed_client == i32::from(victim)
    }) {
        p.commands.push(packet.clone());
    }
}

pub(crate) fn flush_scores(players: &mut Players) {
    if !std::mem::take(&mut players.scores_dirty) {
        return;
    }
    let packet = commands::scoreboard(players);
    for p in players
        .slots
        .values_mut()
        .filter(|p| p.connected && p.state.field("pm_type") == Some(5))
    {
        p.commands.push(packet.clone());
    }
}

fn message(
    host: &mut GameHost<'_>,
    args: &[Value],
    label: &str,
    limit: usize,
) -> io::Result<Vec<u8>> {
    messages::build(args, label, limit, host.config, |obj| {
        let slot = host.world.players.slot_for_object(obj)?;
        Some(host.world.players.get(slot)?.name.clone())
    })
}

pub(super) fn player_method(
    host: &mut GameHost<'_>,
    slot: u16,
    name: &str,
    args: &[Value],
) -> Option<io::Result<Value>> {
    match name {
        "showscoreboard" => Some((|| {
            let packet = commands::scoreboard(&host.world.players);
            host.world.players.get_mut(slot)?.commands.push(packet);
            Ok(Value::Undefined)
        })()),
        "updatescores" => Some((|| {
            let [axis, allies] = host.world.players.team_scores;
            let p = host.world.players.get_mut(slot)?;
            p.commands.push(format!("H {allies}").into_bytes());
            p.commands.push(format!("G {axis}").into_bytes());
            Ok(Value::Undefined)
        })()),
        "sayall" | "sayteam" => Some((|| {
            let text = [
                b"\x14".as_slice(),
                &message(host, args, "Client Chat Message", 1023)?,
            ]
            .concat();
            let dead_chat = host
                .config
                .get("g_deadChat")
                .is_some_and(|v| info_integer(v.as_bytes()) != 0);
            commands::say(
                &mut host.world.players,
                slot,
                name == "sayteam",
                dead_chat,
                host.config.get("dedicated") != Some("0"),
                host.time_ms,
                &text,
            )?;
            Ok(Value::Undefined)
        })()),
        _ => None,
    }
}

fn entity_arg(host: &GameHost<'_>, args: &[Value]) -> io::Result<u16> {
    let Some(Value::Object(object)) = args.first() else {
        return Err(invalid("expected entity"));
    };
    host.world
        .entities
        .slot_for_object(*object)
        .ok_or_else(|| invalid("expected entity"))
}

pub(super) fn global(
    host: &mut GameHost<'_>,
    name: &str,
    args: &[Value],
) -> Option<io::Result<Value>> {
    match name {
        "announcement" | "clientannouncement" => Some((|| {
            let (recipient, parts) = if name == "announcement" {
                (None, args)
            } else {
                (Some(entity_arg(host, args)?), &args[1..])
            };
            let cmd = [
                b"c \"".as_slice(),
                &message(host, parts, "Announcement", 1024)?,
                b"\" 2",
            ]
            .concat();
            if let Some(slot) = recipient {
                if let Some(p) = host.world.players.slots.get_mut(&slot) {
                    p.commands.push(cmd);
                }
            } else {
                host.strings.broadcast(cmd);
            }
            Ok(Value::Undefined)
        })()),
        "allclientsprint" => Some((|| {
            if let Some(value) = args.first() {
                host.strings
                    .broadcast([b"e \"".as_slice(), &value.script_string()?, b"\""].concat());
            }
            Ok(Value::Undefined)
        })()),
        "setwinningplayer" | "setwinningteam" => Some((|| {
            let winner = if name == "setwinningplayer" {
                i32::from(entity_arg(host, args)?) + 1
            } else {
                match args
                    .first()
                    .ok_or_else(|| invalid("expected team"))?
                    .script_string()?
                    .as_slice()
                {
                    b"allies" => -2,
                    b"axis" => -1,
                    b"none" => 0,
                    _ => return Err(invalid("illegal winning team")),
                }
            }
            .to_string();
            let current = host.strings.get(0x16).unwrap_or_default();
            if !info_value(current, b"winner").eq_ignore_ascii_case(winner.as_bytes()) {
                let mut info = InfoString(current.to_vec());
                info.set("winner", winner);
                host.strings.set(0x16, &info.0)?;
            }
            Ok(Value::Undefined)
        })()),
        "getteamplayersalive" => Some((|| {
            let team = match args
                .first()
                .ok_or_else(|| invalid("expected team"))?
                .script_string()?
                .as_slice()
            {
                b"axis" => 1,
                b"allies" => 2,
                _ => return Err(invalid("expected axis or allies")),
            };
            let mut count = 0;
            for (_, p) in host.world.players.iter() {
                if p.team() == team
                    && matches!(host.world.field(p.object,"health")?,Value::Int(n) if n>0)
                {
                    count += 1;
                }
            }
            Ok(Value::Int(count))
        })()),
        _ => None,
    }
}
