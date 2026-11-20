use super::inventory::*;
use crate::{
    assets::weapons::WeaponRegistry,
    snapshot::PlayerState,
    vm::value::{Value, script_error},
};
use std::io;

const METHODS: &[&str] = &[
    "getcurrentweapon",
    "getcurrentoffhand",
    "hasweapon",
    "getammocount",
    "giveweapon",
    "takeweapon",
    "takeallweapons",
    "switchtoweapon",
    "switchtooffhand",
    "setspawnweapon",
    "givestartammo",
    "givemaxammo",
    "getfractionstartammo",
    "getfractionmaxammo",
    "getweaponslotweapon",
    "setweaponslotweapon",
    "getweaponslotammo",
    "getweaponslotclipammo",
    "setweaponslotammo",
    "setweaponslotclipammo",
    "setweaponclipammo",
    "disableweapon",
    "enableweapon",
];
pub fn dispatch(
    name: &str,
    args: &[Value],
    playing: bool,
    player_name: &[u8],
    ps: &mut PlayerState,
    weapons: &WeaponRegistry,
    commands: &mut impl crate::level::CommandSink,
) -> Option<io::Result<Value>> {
    METHODS
        .contains(&name)
        .then(|| apply(name, args, playing, player_name, ps, weapons, commands))
}

fn text(args: &[Value], index: usize) -> io::Result<Vec<u8>> {
    args.get(index)
        .ok_or_else(|| script_error(format!("missing parameter {index}")))?
        .script_string()
        .map_err(|err| script_error(err.to_string()))
}
fn integer(args: &[Value], index: usize) -> io::Result<i32> {
    match args.get(index) {
        Some(Value::Int(n)) => Ok(*n),
        _ => Err(script_error(format!("parameter {index} is not an integer"))),
    }
}
fn slot_arg(args: &[Value]) -> io::Result<usize> {
    let name = text(args, 0)?;
    if name.eq_ignore_ascii_case(b"primary") {
        Ok(1)
    } else if name.eq_ignore_ascii_case(b"primaryb") {
        Ok(2)
    } else {
        Err(script_error(format!(
            "Unknown weaponslot name {}. Valid weaponslots are \"primary\" and \"primaryb\"",
            String::from_utf8_lossy(&name)
        )))
    }
}

fn apply(
    method: &str,
    args: &[Value],
    playing: bool,
    player_name: &[u8],
    ps: &mut PlayerState,
    weapons: &WeaponRegistry,
    commands: &mut impl crate::level::CommandSink,
) -> io::Result<Value> {
    let named = |name: &[u8]| {
        registered(weapons)
            .find(|(_, def)| def.name.as_bytes().eq_ignore_ascii_case(name))
            .map_or(0, |(id, _)| id)
    };
    let name_for = |id| {
        Value::String(
            weapons
                .get(id)
                .map_or(b"none".as_slice(), |def| def.name.as_bytes())
                .to_vec(),
        )
    };
    match method {
        "enableweapon" => ps.words_mut()[0xc / 4] &= !0x0400_0000,
        "disableweapon" => ps.words_mut()[0xc / 4] |= 0x0400_0000,
        "getcurrentweapon" | "getcurrentoffhand" => {
            let field = if method == "getcurrentweapon" {
                WEAPON
            } else {
                OFFHAND
            };
            return Ok(name_for(if playing {
                ps.words()[field] as usize
            } else {
                0
            }));
        }
        "takeallweapons" => {
            ps.words_mut()[WEAPON] = 0;
            for (id, def) in registered(weapons) {
                ps.words_mut()[ammo(def)] = 0;
                ps.words_mut()[clip(def)] = 0;
                take(ps, weapons, id);
            }
        }
        "getweaponslotweapon"
        | "getweaponslotammo"
        | "getweaponslotclipammo"
        | "setweaponslotweapon"
        | "setweaponslotammo"
        | "setweaponslotclipammo" => {
            if !playing && method.starts_with("get") {
                return Ok(if method == "getweaponslotweapon" {
                    name_for(0)
                } else {
                    Value::Int(0)
                });
            }
            let slot = slot_arg(args)?;
            let current = held(ps, slot);
            let def = weapons.get(current).ok_or_else(|| {
                io::Error::new(io::ErrorKind::InvalidData, "invalid slotted weapon")
            })?;
            if method == "getweaponslotweapon" {
                return Ok(name_for(current));
            }
            if method == "setweaponslotweapon" {
                let weapon_name = text(args, 1)?;
                let id = named(&weapon_name);
                if id == 0 && !weapon_name.eq_ignore_ascii_case(b"none") {
                    return Err(script_error(format!(
                        "Unknown weapon {}.",
                        String::from_utf8_lossy(&weapon_name)
                    )));
                }
                let def = weapons.get(id).expect("registered weapon");
                if id != 0 && !(1..=2).contains(&def.weapon_slot) {
                    return Err(script_error(format!(
                        "Weapon {} goes in the none weaponslot, not the {} weaponslot.",
                        String::from_utf8_lossy(&weapon_name),
                        if slot == 1 { "primary" } else { "primaryb" }
                    )));
                }
                if current != 0 {
                    take(ps, weapons, current);
                }
                if id != 0 {
                    let swap = slot == 2 && held(ps, 1) == 0;
                    let already = owned(ps, id);
                    give(ps, weapons, id, commands);
                    if swap {
                        let first = held(ps, 1);
                        set_slot(ps, 2, first);
                        set_slot(ps, 1, 0);
                    }
                    let remaining = def.start_ammo - ps.words()[ammo(def)] as i32;
                    if remaining > 0 {
                        give_ammo(ps, weapons, id, remaining, !already, commands);
                    }
                }
            } else {
                let clip_ammo = method.ends_with("clipammo") || def.handling.clip_only;
                let (index, limit, group) = if clip_ammo {
                    (clip(def), def.clip_size, def.clip_index)
                } else {
                    (ammo(def), def.max_ammo, def.ammo_index)
                };
                if method.starts_with("get") {
                    return Ok(Value::Int(
                        if current == 0 || (method.ends_with("clipammo") && group == 0) {
                            0
                        } else {
                            ps.words()[index] as i32
                        },
                    ));
                }
                let amount = integer(args, 1)?;
                if current == 0 && method == "setweaponslotclipammo" {
                    return Ok(Value::Int(0));
                }
                if current != 0 && group != 0 {
                    ps.words_mut()[index] = amount.max(0).min(limit) as u32;
                }
            }
        }
        _ => {
            let name = text(args, 0)?;
            let id = named(&name);
            let def = weapons.get(id).expect("registered weapon or sentinel");
            let have = owned(ps, id);
            match method {
                "getammocount" => return Ok(Value::Int(if id == 0 { 0 } else { total(ps, def) })),
                "hasweapon" => return Ok(Value::Int(i32::from(id != 0 && have))),
                "switchtoweapon" | "switchtooffhand" => {
                    if id == 0 {
                        return Err(script_error(format!(
                            "unknown weapon '{}'",
                            String::from_utf8_lossy(&name)
                        )));
                    }
                    if have {
                        let command = if method == "switchtoweapon" { 'a' } else { 'C' };
                        commands.push(format!("{command} {id}").into_bytes());
                    }
                    return Ok(Value::Int(i32::from(have)));
                }
                "setspawnweapon" => {
                    if selectable(ps, id, def) {
                        ps.words_mut()[WEAPON] = id as u32;
                        ps.words_mut()[0xd8 / 4] = 0;
                    }
                }
                "takeweapon" => {
                    ps.words_mut()[ammo(def)] = 0;
                    ps.words_mut()[clip(def)] = 0;
                    take(ps, weapons, id);
                }
                "giveweapon" => {
                    if def.weapon_class != 9
                        && def.offhand_class == 0
                        && (!(1..=2).contains(&def.weapon_slot)
                            || (held(ps, 1) != 0 && held(ps, 2) != 0))
                    {
                        let equipped = |slot| {
                            &weapons
                                .get(held(ps, slot))
                                .expect("slotted weapon")
                                .display_name
                        };
                        return Err(script_error(format!(
                            "Cannot give {} weapon {} without having an empty weapon slot - player currently has a {} and a {}\n",
                            String::from_utf8_lossy(player_name),
                            def.display_name,
                            equipped(1),
                            equipped(2)
                        )));
                    }
                    if give(ps, weapons, id, commands) {
                        commands.push(br#"I "1""#.to_vec());
                    }
                    let remaining = def.start_ammo - ps.words()[ammo(def)] as i32;
                    if remaining > 0 {
                        give_ammo(ps, weapons, id, remaining, !have, commands);
                    }
                }
                "givestartammo" | "givemaxammo" | "getfractionstartammo" | "getfractionmaxammo" => {
                    let target = if method.ends_with("startammo") {
                        def.start_ammo
                    } else {
                        def.max_ammo
                    };
                    let reserve = ps.words()[ammo(def)] as i32;
                    if method.starts_with("get") {
                        return Ok(Value::Float(if !have || target < 1 {
                            1.0
                        } else {
                            reserve.max(0) as f32 / target as f32
                        }));
                    }
                    if have && target > reserve {
                        give_ammo(ps, weapons, id, target - reserve, false, commands);
                    }
                }
                "setweaponclipammo" => {
                    let amount = integer(args, 1)?;
                    if id == 0 {
                        return Ok(Value::Int(0));
                    }
                    if def.clip_index != 0 {
                        ps.words_mut()[clip(def)] = amount.max(0).min(def.clip_size) as u32;
                    }
                }
                _ => unreachable!("recognised method"),
            }
        }
    }
    Ok(Value::Undefined)
}
