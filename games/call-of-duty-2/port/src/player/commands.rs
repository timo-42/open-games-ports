use super::*;

pub(super) fn scoreboard(players: &Players) -> Vec<u8> {
    let mut list = String::new();
    let mut count = 0;
    for slot in players.ranked_slots() {
        let p = &players.slots[&slot];
        let ping = if p.connected { p.ping } else { -1 };
        let line = format!(" {slot} {} {ping} {} {}", p.score, p.deaths, p.status_icon);
        if list.len() + line.len() > 1024 {
            break;
        }
        list.push_str(&line);
        count += 1;
    }
    format!(
        "b {count} {} {}{list}",
        players.team_scores[0], players.team_scores[1]
    )
    .into_bytes()
}

pub(crate) fn clean_display_name(name: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    let mut bytes = name.iter().copied().peekable();
    while let Some(b) = bytes.next() {
        if b == b'^' && bytes.peek().is_some_and(u8::is_ascii_digit) {
            bytes.next();
        } else if (32..127).contains(&b) {
            out.push(b);
        }
    }
    out
}

pub(super) fn say(
    players: &mut Players,
    slot: u16,
    team_only: bool,
    dead_chat: bool,
    dedicated: bool,
    time_ms: i32,
    text: &[u8],
) -> io::Result<()> {
    let p = players.get_mut(slot)?;
    let team = p.team();
    let sender_playing = p.session_state == 0;
    let team_only = team_only && (team == 1 || team == 2);
    let name = clean_display_name(&p.name);
    let label = if team_only { "sayteam" } else { "say" };
    let log = [
        format!("{label};{};{slot};", p.guid).as_bytes(),
        &name,
        b";",
        text,
        b"\n",
    ]
    .concat();
    let mut prefix = vec![0x15];
    match team {
        1 => prefix.extend(b"^9"),
        2 => prefix.extend(b"^8"),
        _ => (),
    }
    if team == 3 {
        prefix.extend(b"(\x14GAME_SPECTATOR\x15)");
    } else if !sender_playing {
        prefix.extend(b"(\x14GAME_DEAD\x15)");
    }
    if team_only {
        prefix.extend(if team == 1 {
            b"(\x14GAME_AXIS\x15)".as_slice()
        } else {
            b"(\x14GAME_ALLIES\x15)"
        });
    }
    prefix.extend(name);
    prefix.extend(b"^7: ");
    prefix.truncate(127);
    let body = &text[..text.len().min(149)];
    if dedicated {
        players.console_lines.extend_from_slice(&prefix);
        players.console_lines.extend_from_slice(body);
        players.console_lines.push(b'\n');
    }
    let mut cmd = if team_only {
        b"i \"\x15".to_vec()
    } else {
        b"h \"\x15".to_vec()
    };
    cmd.extend(prefix);
    cmd.extend(if team_only { b"^5" } else { b"^7" });
    cmd.extend_from_slice(body);
    cmd.push(b'"');
    for other in players.slots.values_mut() {
        if other.connected
            && (!team_only || other.team() == team)
            && (dead_chat || sender_playing || other.session_state != 0)
        {
            other.commands.push(cmd.clone());
        }
    }
    players.game_log.push((time_ms, log));
    Ok(())
}

fn menu_response(
    vm: &mut FiberVm,
    host: &GameHost<'_>,
    object: u32,
    server_id: u8,
    args: &[Vec<u8>],
) {
    let (menu, response) = if let [_, id, menu, response] = args {
        if info_integer(id) != i32::from(server_id) {
            return;
        }
        let index = info_integer(menu);
        let menu = if (0..32).contains(&index) {
            host.strings.get(0x4de + index as usize).unwrap_or_default()
        } else {
            menu.as_slice()
        };
        (menu, response.as_slice())
    } else {
        (&[][..], b"bad".as_slice())
    };
    vm.notify(
        object,
        "menuresponse",
        &[
            Value::String(menu.to_vec()),
            Value::String(response.to_vec()),
        ],
    );
}

pub fn client_command(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
    slot: u16,
    server_id: u8,
    args: &[Vec<u8>],
) -> io::Result<()> {
    let Some(command) = args.first() else {
        return Ok(());
    };
    let p = host.world.players.get_mut(slot)?;
    let object = p.object;
    let name = command.to_ascii_lowercase();
    match name.as_slice() {
        b"say" | b"say_team" => {
            let dead_chat = host
                .config
                .get("g_deadChat")
                .is_some_and(|v| info_integer(v.as_bytes()) != 0);
            if args.len() <= 1 {
                return Ok(());
            }
            say(
                &mut host.world.players,
                slot,
                name == b"say_team",
                dead_chat,
                host.config.get("dedicated") != Some("0"),
                host.time_ms,
                &dev_commands::concat(&args[1..]),
            )?;
        }
        b"score" => {
            let cmd = scoreboard(&host.world.players);
            host.world.players.get_mut(slot)?.commands.push(cmd);
        }
        _ if p.state.field("pm_type") == Some(5) => (),
        b"kill" if p.session_state == 0 => {
            crate::combat::damage::suicide(vm, program, host, slot)?;
        }
        b"kill" => (),
        b"callvote" => crate::vote::call(vm, host, slot, args)?,
        b"vote" => crate::vote::cast(vm, host, slot, args)?,
        b"mr" => menu_response(vm, host, object, server_id, args),
        b"follownext" | b"followprev" => {
            let direction = if name == b"follownext" { 1 } else { -1 };
            spawn::follow_cycle(slot, direction, host)?;
        }
        b"god" | b"demigod" | b"noclip" | b"ufo" | b"notarget" | b"give" | b"take"
        | b"setviewpos" | b"entitycount" | b"printentities" => {
            dev_commands::execute(vm, host, slot, &name, &args[1..])?;
        }
        b"where" => {
            let origin = host
                .world
                .entities
                .get_mut(slot)
                .ok_or_else(|| invalid("missing client entity"))?
                .origin;
            let [x, y, z] = origin.map(|v| v as i32);
            p.commands
                .push(format!("e \"\x15({x} {y} {z})\n\"").into_bytes());
        }
        _ => {
            p.commands.push(
                [
                    b"e \"GAME_UNKNOWNCLIENTCOMMAND\x15".as_slice(),
                    command,
                    b"\"",
                ]
                .concat(),
            );
        }
    }
    Ok(())
}
