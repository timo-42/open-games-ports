use super::{DroppedItem, drop_weapon};
use crate::{
    assets::weapons::WeaponRegistry,
    combat::inventory as inv,
    entity_delta::EntityState,
    snapshot::PlayerState,
    vm::{GameHost, fiber::FiberVm, invalid, value::Value},
};
use std::io;

pub fn can_grab(
    item: &EntityState,
    ps: &PlayerState,
    weapons: &WeaponRegistry,
    automatic: bool,
) -> bool {
    let id = item.field("index").unwrap_or(0) as usize;
    if item.field("clientNum") == ps.field("clientNum") {
        return false;
    }
    if matches!(id, 129 | 130) {
        return (ps.words()[0x12c / 4] as i32) < ps.words()[0x134 / 4] as i32;
    }
    let Some(def) = weapons.get(id).filter(|_| id > 0) else {
        return false;
    };
    if automatic && def.offhand_class == 0 && def.weapon_class != 9 && !inv::owned(ps, id) {
        return false;
    }
    let room = if def.shared_ammo_cap_index.is_some() {
        inv::shared_room(ps, weapons, def)
    } else if def.handling.clip_only {
        def.clip_size - ps.words()[inv::clip(def)] as i32
    } else {
        def.max_ammo - ps.words()[inv::ammo(def)] as i32
    };
    room > 0
}

fn health(amount: i32, current: i32, max: i32) -> i32 {
    // Both registered health pickups cap at the live player-state maximum.
    let increment = (f64::from(max) * f64::from(amount) * f64::from(0.01f32)) as i32;
    let next = current.wrapping_add(increment);
    if next > max {
        return max;
    }
    if max <= 0 {
        return next;
    }
    let expected = ((current * 100 / max).max(1) + amount).min(100);
    if (next * 100 / max).clamp(1, 100) == expected {
        next
    } else {
        expected * max / 100
    }
}

fn weapon_message(key: &[u8], display: &str) -> Vec<u8> {
    [b"f \"".as_slice(), key, b"\x14", display.as_bytes(), b"\""].concat()
}

pub fn touch(
    vm: &mut FiberVm,
    host: &mut GameHost<'_>,
    player: u16,
    item: u16,
    automatic: bool,
) -> io::Result<()> {
    if host.world.client_is_spawning {
        return Ok(());
    }
    let Some(ent) = host.world.entities.get_mut(item) else {
        return Ok(());
    };
    let Some(mut drop) = ent.dropped_item else {
        return Ok(());
    };
    let id = drop.index;
    let object = ent.object;
    let owner = ent.state.field("clientNum");
    let Some(p) = host.world.players.get(player) else {
        return Ok(());
    };
    let ps = &p.state;
    if ps.words()[0x12c / 4] as i32 <= 0 {
        return Ok(());
    }
    if !can_grab(&ent.state, ps, host.weapons, automatic) {
        if !automatic
            && owner != Some(u32::from(player))
            && let Some(def) = host.weapons.get(id)
        {
            let cmd = if inv::owned(ps, id) {
                Some(weapon_message(
                    b"GAME_PICKUP_CANTCARRYMOREAMMO",
                    &def.display_name,
                ))
            } else if (1..=2).contains(&def.weapon_slot) {
                Some(b"f \"GAME_CANT_GET_PRIMARY_WEAP_MESSAGE\"".to_vec())
            } else {
                None
            };
            if let Some(cmd) = cmd {
                host.world.players.get_mut(player)?.commands.push(cmd);
            }
        }
        return Ok(());
    }
    let player_object = p.object;
    let (kind, name) = match id {
        129 => ("Item", "item_health_small"),
        130 => ("Item", "item_health_large"),
        _ => (
            "Weapon",
            host.weapons
                .get(id)
                .expect("valid item weapon")
                .name
                .as_str(),
        ),
    };
    let log = [
        format!("{kind};{};{player};", p.guid).as_bytes(),
        &crate::player::clean_display_name(&p.name),
        b";",
        name.as_bytes(),
        b"\n",
    ]
    .concat();
    host.world.players.game_log.push((host.time_ms, log));
    let mut event = 0x90;
    let mut args = vec![Value::Object(player_object)];
    if matches!(id, 129 | 130) {
        let amount = if drop.reserve != 0 {
            drop.reserve
        } else if id == 129 {
            10
        } else {
            50
        };
        let player = host.world.players.get_mut(player)?;
        let new_health = health(
            amount,
            player.state.words()[0x12c / 4] as i32,
            player.state.words()[0x134 / 4] as i32,
        );
        player.state.words_mut()[0x12c / 4] = new_health as u32;
        player
            .commands
            .push(format!("f \"GAME_PICKUP_HEALTH\x15{amount}\"").into_bytes());
        player.commands.push(b"I \"0\"".to_vec());
        host.world
            .set_field(player_object, "health", Value::Int(new_health))?;
    } else {
        let Some((new_event, swapped)) = weapon_pickup(host, player, item, &mut drop, automatic)?
        else {
            return Ok(());
        };
        event = new_event;
        args.push(swapped);
    }
    vm.notify(object, "trigger", &args);
    let ps = &mut host.world.players.get_mut(player)?.state;
    let words = ps.words_mut();
    let sequence = words[0xa4 / 4];
    words[0xa8 / 4 + (sequence & 3) as usize] = event;
    words[0xb8 / 4 + (sequence & 3) as usize] = id as u32;
    words[0xa4 / 4] = sequence.wrapping_add(1);
    host.world.free_entity(item, host.time_ms)
}

fn weapon_pickup(
    host: &mut GameHost<'_>,
    slot: u16,
    item: u16,
    drop: &mut DroppedItem,
    automatic: bool,
) -> io::Result<Option<(u32, Value)>> {
    let id = drop.index;
    let def = host
        .weapons
        .get(id)
        .ok_or_else(|| invalid("invalid weapon item"))?;
    let main_weapon = def.offhand_class == 0 && def.weapon_class != 9;
    let held_slot = def.weapon_slot;
    let offhand = def.offhand_class != 0;
    let clip_index = inv::clip(def);
    let clip_size = def.clip_size;
    let ammo_index = inv::ammo(def);
    let display = def.display_name.clone();
    let clip_only = def.handling.clip_only;
    if drop.reserve == 0 {
        let lo = def.drop_ammo_min.min(def.drop_ammo_max);
        let hi = def.drop_ammo_min.max(def.drop_ammo_max);
        if hi == 0 && lo == 0 {
            let fraction = ((host.world.native_random.fraction() + 1.) * 0.5) as f32;
            drop.reserve = ((f64::from((f64::from(clip_size - 1) * f64::from(fraction)) as f32)
                + 0.5)
                .floor() as i32)
                + 1;
        } else if hi >= 0 {
            drop.reserve = if hi == lo {
                lo
            } else {
                lo + (host.world.native_random.draw() as i32) % (hi - lo)
            };
            drop.reserve = drop.reserve.max(0);
        }
    }
    drop.reserve = drop.reserve.min(def.max_ammo);
    if drop.magazine == 0 && drop.reserve >= 0 {
        drop.magazine = drop.reserve.min(clip_size);
        drop.reserve -= drop.magazine;
    }
    drop.magazine = drop.magazine.min(clip_size);
    let reserve = drop.reserve.max(0);
    let magazine = drop.magazine.max(0);
    let entity = host.world.entities.get_mut(item).expect("pickup entity");
    entity.dropped_item = Some(*drop);
    let object = entity.object;
    host.world
        .set_field(object, "count", Value::Int(drop.reserve))?;
    let p = host.world.players.get_mut(slot)?;
    let mut swapped = Value::Undefined;
    let event;
    if inv::owned(&p.state, id) {
        event = 0x91;
        let received = inv::give_ammo(
            &mut p.state,
            host.weapons,
            id,
            reserve + magazine,
            false,
            &mut p.commands,
        );
        if received != 0 {
            p.commands.push(weapon_message(
                if clip_only {
                    b"GAME_PICKUP_CLIPONLY_AMMO"
                } else {
                    b"GAME_PICKUP_AMMO"
                },
                &display,
            ));
        }
        if received != reserve + magazine {
            drop.reserve -= received;
            if drop.reserve < 1 {
                drop.magazine += drop.reserve;
                if drop.magazine < 1 {
                    drop.magazine = -1;
                }
                drop.reserve = -1;
            }
            if (drop.reserve > 0 || drop.magazine > 0)
                && host.config.get("g_weaponAmmoPools") == Some("1")
            {
                let ent = host.world.entities.get_mut(item).expect("pickup");
                ent.dropped_item = Some(*drop);
                let object = ent.object;
                host.world
                    .set_field(object, "count", Value::Int(drop.reserve))?;
                return Ok(None);
            }
        }
    } else {
        event = 0x90;
        let active = p.state.field("weapon").unwrap_or(0) as usize;
        let slots = [inv::held(&p.state, 1), inv::held(&p.state, 2)];
        let free_slot = (1..=2).contains(&held_slot) && slots.contains(&0);
        let stackable = |id| {
            host.weapons
                .get(id)
                .is_some_and(|w| w.slot_stackable && (1..=2).contains(&w.weapon_slot))
                && slots
                    .iter()
                    .any(|&id| id == 0 || host.weapons.get(id).is_some_and(|w| w.slot_stackable))
        };
        if main_weapon
            && active != 0
            && (!inv::owned(&p.state, active)
                || (inv::find_slot(&p.state, host.weapons, active).is_none()
                    && !stackable(active)
                    && !free_slot))
        {
            return Ok(None);
        }
        if main_weapon && !free_slot && !stackable(id) {
            let active_slot = host.weapons.get(active).map_or(0, |w| w.weapon_slot);
            let swap = if active_slot == held_slot {
                active
            } else if p.state.words()[ammo_index] == 0 && p.state.words()[clip_index] == 0 {
                inv::held(&p.state, 1)
            } else {
                p.commands
                    .push(b"f \"GAME_CANT_GET_PRIMARY_WEAP_MESSAGE\"".to_vec());
                return Ok(None);
            };
            swapped = drop_weapon(slot, swap, None, host)?;
            let Value::Object(object) = swapped else {
                return Ok(None);
            };
            let item = host.world.entities.get_mut(item).expect("picked item");
            let (pos, angles, ground, suspended) = (
                item.origin,
                item.angles,
                item.state.field("groundEntityNum").unwrap_or(1023),
                item.dropped_item.expect("picked item").suspended,
            );
            let (_, new_item) = host
                .world
                .entities
                .for_object_mut(object)
                .expect("dropped item");
            new_item.dropped_item.as_mut().expect("swap").suspended = suspended;
            super::motion::place(new_item, pos, angles, ground);
            crate::vm::entity_methods::relink(new_item, host.collision, new_item.model_index != 0)?;
        }
        let p = host.world.players.get_mut(slot)?;
        inv::give(&mut p.state, host.weapons, id, &mut p.commands);
        if main_weapon && !automatic {
            p.commands.push(format!("a {id}").into_bytes());
        }
        p.state.words_mut()[clip_index] = magazine as u32;
        inv::give_ammo(
            &mut p.state,
            host.weapons,
            id,
            reserve,
            false,
            &mut p.commands,
        );
    }
    host.world.players.get_mut(slot)?.commands.push(if offhand {
        b"I \"4\"".to_vec()
    } else {
        b"I \"1\"".to_vec()
    });
    Ok(Some((event, swapped)))
}

#[cfg(test)]
mod tests {
    use super::health;
    #[test]
    fn health_pickups_match_original_player_and_entity_health() {
        // Item_PickupHealth calls from the Linux 1.3 binary. The source is the
        // maximum in ps.stats[2] even when the session maxHealth differs.
        for (amount, current, max, expected) in [
            (10, 73, 100, 83),
            (50, 73, 100, 100),
            (10, 12, 73, 19),
            (7, 19, 33, 21),
            (100, 19, 30, 30),
        ] {
            assert_eq!(health(amount, current, max), expected);
        }
    }
}
