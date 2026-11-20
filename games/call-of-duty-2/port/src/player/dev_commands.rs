//! ClientCommand developer commands (Cmd_Give_f through Cmd_SetViewpos_f).
use super::*;
use crate::{
    assets::weapons::WeaponRegistry, combat::inventory as inv, vm::spawn_fields::float_prefix,
};

/// ConcatArgs appends a space for each nonfinal argument and stops before an
/// argument would exceed the 1024-byte static buffer.
pub(super) fn concat(args: &[Vec<u8>]) -> Vec<u8> {
    let mut out = Vec::new();
    for (index, arg) in args.iter().enumerate() {
        let arg = &arg[..arg.len().min(1023)];
        if arg.len() + out.len() > 1022 {
            break;
        }
        out.extend_from_slice(arg);
        if index + 1 < args.len() {
            out.push(b' ');
        }
    }
    out
}

fn reply(p: &mut Player, text: &str) {
    p.commands.push(format!("e \"{text}\"").into_bytes());
}

fn toggle(host: &mut GameHost<'_>, slot: u16, cmd: &[u8]) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    let (prefix, enabled) = match cmd {
        b"noclip" => {
            p.input.noclip = !p.input.noclip;
            ("GAME_NOCLIP", p.input.noclip)
        }
        b"ufo" => {
            p.input.ufo = !p.input.ufo;
            ("GAME_UFO", p.input.ufo)
        }
        _ => {
            let (prefix, bit) = match cmd {
                b"god" => ("GAME_GODMODE_", 1),
                b"demigod" => ("GAME_DEMI_GODMODE_", 2),
                _ => ("GAME_NOTARGET", 4),
            };
            let entity = host
                .world
                .entities
                .get_mut(slot)
                .ok_or_else(|| invalid("missing player entity"))?;
            entity.flags ^= bit;
            (prefix, entity.flags & bit != 0)
        }
    };
    reply(
        p,
        &format!("{prefix}{}", if enabled { "ON" } else { "OFF" }),
    );
    Ok(())
}

fn free_held_slot(ps: &PlayerState) -> bool {
    inv::held(ps, 1) == 0 || inv::held(ps, 2) == 0
}

fn all_weapons(p: &mut Player, weapons: &WeaponRegistry, give: bool) {
    if !give {
        for (id, def) in inv::registered(weapons) {
            inv::take(&mut p.state, weapons, id);
            p.state.words_mut()[inv::ammo(def)] = 0;
            p.state.words_mut()[inv::clip(def)] = 0;
        }
        if p.state.words()[0xd4 / 4] != 0 {
            p.state.words_mut()[0xd4 / 4] = 0;
            p.commands.push(b"a 0".to_vec());
        }
        return;
    }
    let last_held = inv::held(&p.state, 2);
    for slot in [1, 2] {
        let old = inv::held(&p.state, slot);
        inv::take(&mut p.state, weapons, old);
    }
    for (id, def) in inv::registered(weapons) {
        if !(1..=2).contains(&def.weapon_slot) || (id > last_held && free_held_slot(&p.state)) {
            inv::give(&mut p.state, weapons, id, &mut p.commands);
        }
    }
    for (id, def) in inv::registered(weapons) {
        if !free_held_slot(&p.state) {
            break;
        }
        if (1..=2).contains(&def.weapon_slot) {
            inv::give(&mut p.state, weapons, id, &mut p.commands);
        }
    }
}

fn ammo(p: &mut Player, reg: &WeaponRegistry, give: bool, id: usize, amount: i32) {
    let Some(def) = reg.get(id) else { return };
    if give {
        inv::give_ammo(
            &mut p.state,
            reg,
            id,
            if amount == 0 { 998 } else { amount },
            true,
            &mut p.commands,
        );
    } else {
        let words = p.state.words_mut();
        let (reserve, clip) = (inv::ammo(def), inv::clip(def));
        if amount == 0 {
            words[reserve] = 0;
            words[clip] = 0;
        } else {
            let left = (words[reserve] as i32).wrapping_sub(amount);
            words[reserve] = left.max(0) as u32;
            if left < 0 {
                words[clip] = left.wrapping_add(words[clip] as i32).max(0) as u32;
            }
        }
    }
}

fn inventory(
    vm: &mut FiberVm,
    host: &mut GameHost<'_>,
    slot: u16,
    give: bool,
    args: &[Vec<u8>],
) -> io::Result<()> {
    let amount = args.get(1..).map_or(0, |rest| info_integer(&concat(rest)));
    let name = concat(args);
    if name.is_empty() {
        return Ok(());
    }
    let lower = name.to_ascii_lowercase();
    let all = lower == b"all";
    if all || lower.starts_with(b"health") {
        let p = host.world.players.get_mut(slot)?;
        let object = p.object;
        let max = p.state.words()[0x134 / 4] as i32;
        let current = integer(&host.world.field(object, "health")?)?;
        let next = match (give, amount) {
            (true, 0) => max,
            (true, n) => current.wrapping_add(n),
            (false, 0) => 1,
            (false, n) => current.wrapping_sub(n).max(1),
        };
        host.world.set_field(object, "health", Value::Int(next))?;
        if !all {
            return Ok(());
        }
    }
    let p = host.world.players.get_mut(slot)?;
    if all || lower == b"weapons" {
        all_weapons(p, host.weapons, give);
        if !all {
            return Ok(());
        }
    }
    if all || lower.starts_with(b"ammo") {
        if amount == 0 {
            for (id, _) in inv::registered(host.weapons) {
                ammo(p, host.weapons, give, id, 0);
            }
        } else {
            let current = p.state.words()[0xd4 / 4] as usize;
            if current != 0 {
                ammo(p, host.weapons, give, current, amount);
            }
        }
        return Ok(());
    }
    if lower.starts_with(b"allammo") && amount != 0 {
        for (id, _) in inv::registered(host.weapons) {
            ammo(p, host.weapons, give, id, amount);
        }
        return Ok(());
    }
    if give && let Ok(name) = std::str::from_utf8(&name) {
        crate::vm::items::give_named(vm, host, slot, name)?;
    }
    Ok(())
}

fn print_entities(host: &mut GameHost<'_>) -> io::Result<()> {
    for i in 0..host.world.entities.num_entities() as u16 {
        let entity = host.world.entities.get(i);
        let name = if let Some(e) = entity {
            match host.world.field(e.object, "classname")? {
                Value::String(name) => name,
                _ => Vec::new(),
            }
        } else {
            Vec::new()
        };
        let [x, y, z] = entity.map_or([0.; 3], |e| e.origin);
        host.config.messages.push(format!(
            "{i:4}: '{}', origin: {x:.6} {y:.6} {z:.6}",
            String::from_utf8_lossy(&name)
        ));
    }
    Ok(())
}

pub(super) fn execute(
    vm: &mut FiberVm,
    host: &mut GameHost<'_>,
    slot: u16,
    cmd: &[u8],
    args: &[Vec<u8>],
) -> io::Result<()> {
    if cmd == b"printentities" {
        return print_entities(host);
    }
    let cheats = host
        .config
        .get("sv_cheats")
        .is_some_and(|v| info_integer(v.as_bytes()) != 0);
    if cmd == b"entitycount" {
        if cheats {
            host.config.messages.push(format!(
                "entity count = {}",
                host.world.entities.num_entities()
            ));
        }
        return Ok(());
    }
    let p = host.world.players.get_mut(slot)?;
    if !cheats {
        reply(p, "GAME_CHEATSNOTENABLED");
        return Ok(());
    }
    if cmd == b"setviewpos" {
        if args.len() != 4 {
            reply(p, "GAME_USAGE\x15: setviewpos x y z yaw");
            return Ok(());
        }
        let v = |i: usize| {
            float_prefix(&args[i])
                .and_then(|(s, _)| s.parse::<f32>().ok())
                .unwrap_or(0.)
        };
        let height = f32::from_bits(p.state.words()[0xf8 / 4]);
        return spawn::teleport(slot, host, [v(0), v(1), v(2) - height], [0., v(3), 0.]);
    }
    let object = p.object;
    if integer(&host.world.field(object, "health")?)? < 1 {
        reply(host.world.players.get_mut(slot)?, "GAME_MUSTBEALIVECOMMAND");
        return Ok(());
    }
    match cmd {
        b"give" | b"take" => inventory(vm, host, slot, cmd == b"give", args),
        _ => toggle(host, slot, cmd),
    }
}
