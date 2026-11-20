//! Dropped items, inventory transfers, proximity checks, and server item events.
pub(crate) mod motion;
mod pickup;
mod random;
pub(crate) mod spawn;
use super::{GameHost, entity_methods::relink, invalid, value::Value};
use crate::{combat::inventory, entity_delta::EntityState};
pub use pickup::{can_grab, touch};
pub use random::NativeRandom;
pub(crate) use spawn::give_named;
use std::io;

#[derive(Debug, Clone, Copy)]
pub(crate) struct DroppedItem {
    pub index: usize,
    pub reserve: i32,
    pub magazine: i32,
    pub release_owner: i32,
    pub list_slot: Option<usize>,
    pub finish_at: Option<i32>,
    pub suspended: bool,
}

use crate::assets::weapons::health_item;

fn float_dvar(host: &GameHost<'_>, name: &str, default: f32) -> f32 {
    host.config
        .get(name)
        .and_then(|v| v.parse::<f32>().ok())
        .unwrap_or(default)
        .clamp(0., 1000.)
}

fn dropped_list_slot(host: &mut GameHost<'_>) -> io::Result<usize> {
    let max = host
        .config
        .get("g_maxDroppedWeapons")
        .and_then(|v| v.parse::<usize>().ok())
        .unwrap_or(16)
        .clamp(1, 32);
    let mut list = [None; 32];
    for (slot, ent) in host.world.entities.iter() {
        if let Some(index) = ent.dropped_item.and_then(|item| item.list_slot) {
            list[index] = Some((slot, ent.origin));
        }
    }
    if let Some(free) = list[..max].iter().position(Option::is_none) {
        return Ok(free);
    }
    let players = host
        .world
        .players
        .iter()
        .filter(|(_, p)| p.connected && p.session_state == 0)
        .filter_map(|(n, _)| host.world.entities.get(n).map(|e| e.origin))
        .collect::<Vec<_>>();
    let mut selected = 0;
    let mut farthest = -1.;
    for (i, entry) in list[..max].iter().enumerate() {
        let (_, pos) = entry.expect("occupied list");
        let nearest = players
            .iter()
            .map(|p| (0..3).map(|j| (pos[j] - p[j]).powi(2)).sum::<f32>())
            .fold(9.99998e11f32, f32::min);
        if nearest > farthest {
            farthest = nearest;
            selected = i;
        }
    }
    host.world
        .free_entity(list[selected].expect("chosen entry").0, host.time_ms)?;
    Ok(selected)
}

/// PlayerCmd_dropitem accepts either a weapon name or one of the fixed health items.
pub(crate) fn drop_item(slot: u16, args: &[Value], host: &mut GameHost<'_>) -> io::Result<Value> {
    let name = args
        .first()
        .ok_or_else(|| invalid("dropitem expects an item name"))?
        .script_string()?;
    let name = std::str::from_utf8(&name).map_err(|_| invalid("invalid item name"))?;
    let id = host.weapons.resolve(host.assets, name, host.initializing)?;
    if id != 0 {
        let tag = args
            .get(1)
            .map(Value::script_string)
            .transpose()?
            .unwrap_or_else(|| b"tag_weapon_right".to_vec());
        return drop_weapon(slot, id, Some(&tag), host);
    }
    let Some((id, class, alias, model)) = health_item(name) else {
        return Ok(Value::Undefined);
    };
    if !host.initializing && !host.weapons.item_registered(id) {
        return Err(invalid(format!(
            "game tried to register the item '{alias}' after initialization finished"
        )));
    }
    host.weapons.precache_item(host.assets, class)?;
    host.weapons
        .publish(host.assets, host.models, host.strings)?;
    let (_, obj) = spawn_drop(slot, id, class, model, host)?;
    Ok(Value::Object(obj))
}

pub(super) fn drop_weapon(
    slot: u16,
    id: usize,
    tag: Option<&[u8]>,
    host: &mut GameHost<'_>,
) -> io::Result<Value> {
    let def = host
        .weapons
        .get(id)
        .ok_or_else(|| invalid("unknown weapon"))?;
    let ps = &mut host.world.players.get_mut(slot)?.state;
    let ammo = inventory::ammo(def);
    let clip = inventory::clip(def);
    if !inventory::owned(ps, id) || def.handling.clip_only && ps.words()[clip] == 0 {
        inventory::take(ps, host.weapons, id);
        return Ok(Value::Undefined);
    }
    let name = format!("weapon_{}", def.name);
    let model = def.world_model.clone();
    let (number, object) = spawn_drop(slot, id, &name, &model, host)?;
    let ps = &mut host.world.players.get_mut(slot)?.state;
    let reserve = std::mem::take(&mut ps.words_mut()[ammo]) as i32;
    let magazine = std::mem::take(&mut ps.words_mut()[clip]) as i32;
    inventory::take(ps, host.weapons, id);
    let player_ent = host.world.entities.get_mut(slot).expect("player entity");
    let angles = player_ent.angles;
    let center: [f32; 3] = std::array::from_fn(|i| {
        player_ent.origin[i] + (player_ent.mins[i] + player_ent.maxs[i]) * 0.5
    });
    let tag_origin = tag
        .and_then(|name| crate::collision::dobj::tag::world_origin(player_ent, host.models, name));
    let tag_position = tag_origin.map(|end| {
        let entity = host.world.entities.get(number).expect("dropped weapon");
        let hit = host.collision.box_trace(
            &host.world.entities,
            center,
            end,
            entity.mins,
            entity.maxs,
            slot,
            0x491,
        );
        std::array::from_fn(|i| {
            (f64::from(center[i])
                + (f64::from(end[i]) - f64::from(center[i])) * f64::from(hit.fraction))
                as f32
        })
    });
    let entity = host.world.entities.get_mut(number).expect("dropped weapon");
    let drop = entity.dropped_item.as_mut().expect("dropped item");
    drop.reserve = if reserve == 0 { -1 } else { reserve };
    drop.magazine = if magazine == 0 { -1 } else { magazine };
    if reserve == 0 && magazine == 0 {
        entity.contents &= !0x200000;
    }
    if tag.is_some() {
        // When there is no server tag matrix the original keeps the throw origin.
        // G_SetAnglesTrajectory receives the player's angles in either branch.
        if let Some(origin) = tag_position {
            entity.origin = origin;
            motion::set_vector(&mut entity.state, "pos.trBase", entity.origin);
        }
        entity.angles = angles;
        motion::set_vector(&mut entity.state, "apos.trBase", angles);
        let z = (host.world.native_random.signed() * 60.) as f32;
        let y = (host.world.native_random.signed() * 40.) as f32;
        let x = (host.world.native_random.signed() * 50.) as f32;
        entity.state.set_field("apos.trType", 2);
        entity.state.set_field("apos.trTime", host.time_ms as u32);
        motion::set_vector(&mut entity.state, "apos.trDelta", [x, y, z]);
    }
    host.world.set_field(
        object,
        "count",
        Value::Int(if reserve == 0 { -1 } else { reserve }),
    )?;
    Ok(Value::Object(object))
}

fn spawn_drop(
    player: u16,
    id: usize,
    class: &str,
    model: &str,
    host: &mut GameHost<'_>,
) -> io::Result<(u16, u32)> {
    let speed = float_dvar(host, "g_dropForwardSpeed", 10.);
    let up = float_dvar(host, "g_dropUpSpeedBase", 10.);
    let spread = float_dvar(host, "g_dropUpSpeedRand", 5.);
    let src = host
        .world
        .entities
        .get_mut(player)
        .ok_or_else(|| invalid("missing dropping player"))?;
    let yaw = f64::from(f64::from(src.angles[1]).to_radians() as f32);
    let mut origin = src.origin;
    origin[2] += (src.maxs[2] - src.mins[2]) * 0.5;
    let velocity = [
        speed * yaw.cos() as f32,
        speed * yaw.sin() as f32,
        (f64::from(up) + host.world.native_random.signed() * f64::from(spread)) as f32,
    ];
    let model_index = host
        .models
        .index(host.assets, host.strings, model, host.initializing)?;
    let (slot, object) = host
        .world
        .spawn_entity(class.as_bytes(), origin, host.time_ms)?;
    let list_slot = dropped_list_slot(host)?;
    host.world
        .set_field(object, "model", Value::String(model.as_bytes().to_vec()))?;
    let ent = host.world.entities.get_mut(slot).expect("new item");
    ent.dropped_item = Some(DroppedItem {
        index: id,
        reserve: 0,
        magazine: 0,
        release_owner: host.time_ms.wrapping_add(1000),
        list_slot: Some(list_slot),
        finish_at: None,
        suspended: false,
    });
    ent.model_index = model_index;
    ent.mins = if id < 129 { [-1.; 3] } else { [-1., -1., 0.] };
    ent.maxs = if id < 129 { [1.; 3] } else { [1., 1., 2.] };
    ent.contents = 0x407c0108;
    ent.flags = 0x10;
    for (field, v) in [
        ("eType", 3),
        ("index", id as u32),
        ("clientNum", u32::from(player)),
        ("pos.trType", 5),
        ("pos.trTime", host.time_ms as u32),
    ] {
        ent.state.set_field(field, v);
    }
    motion::set_vector(&mut ent.state, "pos.trDelta", velocity);
    relink(ent, host.collision, model_index != 0)?;
    Ok((slot, object))
}

pub fn player_near(origin: [f32; 3], item: &EntityState, now: i32) -> bool {
    let pos = motion::position(item, now);
    (-36.0..=36.).contains(&(origin[0] - pos[0]))
        && (-36.0..=36.).contains(&(origin[1] - pos[1]))
        && (-88.0..=18.).contains(&(origin[2] - pos[2]))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        assets::{GameAssets, weapons::WeaponRegistry},
        config::ServerConfig,
        player::{self, Connect},
        script::ScriptProgram,
        vm::fiber::FiberVm,
    };
    use std::fs;
    #[test]
    fn original_drop_vectors_and_inventory_transfer() -> io::Result<()> {
        let dir = tempfile::tempdir()?;
        let main = dir.path().join("main");
        fs::create_dir_all(main.join("weapons/mp"))?;
        fs::create_dir_all(main.join("mp"))?;
        fs::write(main.join("mp/playeranimtypes.txt"), "none")?;
        fs::write(main.join("weapons/mp/defaultweapon_mp"),br"WEAPONFILE\weaponSlot\primary\ammoName\bullets\clipName\magazine\maxAmmo\96\clipSize\8\dropAmmoMin\32\dropAmmoMax\40")?;
        let assets = GameAssets::mount_paths(dir.path(), None, "")?;
        let weapon = &mut WeaponRegistry::load(&assets)?;
        let mut host = GameHost {
            weapons: weapon,
            assets: &assets,
            models: &mut Default::default(),
            start_time: 0,
            collision: &crate::collision::tests::map(-1, -2, &[])?,
            world: &mut Default::default(),
            config: &mut ServerConfig::from_args(&[]),
            strings: &mut Default::default(),
            time_ms: 300,
            initializing: false,
        };
        let program = ScriptProgram::discover("test", "mp_test", false, |p| {
            Ok(match p {
            "codescripts/delete.gsc"=>b"main() {}".to_vec(),
            "codescripts/struct.gsc"=>b"initStructs() {} createStruct() {}".to_vec(),
            "maps/mp/gametypes/test.gsc"=>b"main() {}".to_vec(),
            "maps/mp/gametypes/_callbacksetup.gsc"=>b"CodeCallback_StartGameType(){} CodeCallback_PlayerConnect(){} CodeCallback_PlayerDisconnect(){} CodeCallback_PlayerDamage(){} CodeCallback_PlayerKilled(){}".to_vec(),
            _=>return Err(io::ErrorKind::NotFound.into())
        })
        })?;
        let mut vm = FiberVm::default();
        for slot in 0..=1 {
            player::connect(
                &mut vm,
                &program,
                &mut host,
                Connect {
                    slot,
                    userinfo: b"",
                    local: false,
                    guid: 0,
                    persistent: Value::default(),
                },
            )?;
            let player = host.world.players.get_mut(slot)?;
            player.state.words_mut()[0x12c / 4] = 100;
            let ent = host
                .world
                .entities
                .get_mut(slot)
                .expect("spawned fixture entity");
            ent.origin = [24., -14., 425.];
            ent.angles = [1., 90., 0.];
            ent.mins = [-15., -15., 0.];
            ent.maxs = [15., 15., 70.];
        }
        let id = 1;
        let (ammo, clip) = {
            let d = host.weapons.get(id).expect("registered fixture weapon");
            (inventory::ammo(d), inventory::clip(d))
        };
        for aim in [None, Some(b"tag_weapon_right".as_slice())] {
            let p = host.world.players.get_mut(0)?;
            inventory::give(&mut p.state, host.weapons, id, &mut p.commands);
            p.state.words_mut()[ammo] = 35;
            p.state.words_mut()[clip] = 3;
            host.world.native_random = NativeRandom::new(1234567);
            let Value::Object(object) = drop_weapon(0, id, aim, &mut host)? else {
                panic!("drop returned undefined")
            };
            let (slot, e) = host
                .world
                .entities
                .for_object_mut(object)
                .expect("spawned fixture entity");
            let mut expected = [0; 60];
            // All 60 original dwords at return from Player_DropWeapon. The
            // slot and weapon index in this generated level use its own tables.
            for (idx, val) in [
                (0, u32::from(slot)),
                (1, 3),
                (3, 5),
                (4, 300),
                (6, 0x41c00000),
                (7, 0xc1600000),
                (8, 0x43e60000),
                (9, 0xb4eaac7a),
                (10, 0x41200000),
                (11, 0x4146d895),
                (35, 1),
            ] {
                expected[idx] = val;
            }
            if aim.is_some() {
                for (idx, val) in [
                    (12, 2),
                    (13, 300),
                    (15, 0x3f800000),
                    (16, 0x42b40000),
                    (18, 0x4228e892),
                    (19, 0xc1f4483b),
                    (20, 0x417cdf11),
                ] {
                    expected[idx] = val;
                }
            }
            assert_eq!(e.state, EntityState::from_words(expected));
            assert_eq!(
                host.world
                    .players
                    .get(0)
                    .expect("connected fixture player")
                    .state
                    .words()[ammo],
                0
            );
            assert_eq!(
                host.world
                    .players
                    .get(0)
                    .expect("connected fixture player")
                    .state
                    .words()[clip],
                0
            );
            touch(&mut vm, &mut host, 0, slot, false)?;
            assert!(host.world.entities.get_mut(slot).is_some());
            if aim.is_some() {
                touch(&mut vm, &mut host, 1, slot, true)?;
                assert!(host.world.entities.get_mut(slot).is_some());
                touch(&mut vm, &mut host, 1, slot, false)?;
                let receiver = host.world.players.get(1).expect("connected fixture player");
                assert_eq!(
                    (receiver.state.words()[ammo], receiver.state.words()[clip]),
                    (35, 3)
                );
            } else {
                host.world.free_entity(slot, 300)?;
            }
        }
        // M1 Garand Item_PickupWeapon calls in the original server, with its
        // stock 96-cartridge reserve, 8-round clip and 32..40 drop range.
        for (owned, inventory_ammo, loaded, item_ammo, item_clip, event, want_ammo, want_clip) in [
            (false, 0, 0, 35, 3, 144, 35, 3),
            (true, 0, 1, 35, 3, 145, 38, 1),
            (true, 94, 3, 35, 3, 145, 96, 3),
            (true, 0, 3, -1, -1, 145, 0, 3),
            (true, 0, 3, 0, 0, 145, 32, 3),
        ] {
            let receiver = host.world.players.get_mut(1)?;
            inventory::take(&mut receiver.state, host.weapons, id);
            if owned {
                inventory::give(
                    &mut receiver.state,
                    host.weapons,
                    id,
                    &mut receiver.commands,
                );
            }
            receiver.state.words_mut()[ammo] = inventory_ammo;
            receiver.state.words_mut()[clip] = loaded;
            let event_index = (receiver.state.words()[0xa4 / 4] & 3) as usize;
            let (slot, _) = host
                .world
                .spawn_entity(b"weapon_defaultweapon_mp", [0.; 3], 300)?;
            let ent = host.world.entities.get_mut(slot).expect("spawned pickup");
            spawn::setup(ent, id, 0, 0, item_ammo, None);
            ent.dropped_item.as_mut().expect("registered item").magazine = item_clip;
            host.world.native_random = NativeRandom::new(1234567);
            touch(&mut vm, &mut host, 1, slot, false)?;
            let receiver = host.world.players.get(1).expect("receiver");
            assert_eq!(
                (
                    receiver.state.words()[0xa8 / 4 + event_index],
                    receiver.state.words()[ammo],
                    receiver.state.words()[clip]
                ),
                (event, want_ammo, want_clip)
            );
            assert!(host.world.entities.get(slot).is_none());
        }
        // Two cloneplayer returns from the original DM/Toujane duel. Source player
        // fields and the signed-zero velocities are recorded before any corpse think.
        for (slot, body, time, delay, raw_origin, raw_yaw, anim, raw_vel) in [
            (
                1,
                64,
                36750,
                1983,
                [0x43ea98c8, 0x4477ff4f, 0x42ee4000],
                0x4333b2a8,
                0x26,
                [0, 0x80000000, 0x80000000],
            ),
            (
                0,
                65,
                56000,
                949,
                [0x43b88ccd, 0x4478199a, 0x42ee4000],
                0xbf4d5000,
                0x29,
                [0; 3],
            ),
        ] {
            host.time_ms = time;
            let p = host.world.players.get_mut(slot)?;
            for (axis, (&position, &velocity)) in raw_origin.iter().zip(&raw_vel).enumerate() {
                p.state.set_field(&format!("origin[{axis}]"), position)?;
                p.state.set_field(&format!("velocity[{axis}]"), velocity)?;
            }
            p.state.set_field("eFlags", 2)?;
            p.state.set_field("legsAnim", anim)?;
            let original = host
                .world
                .entities
                .get_mut(slot)
                .expect("spawned fixture entity");
            original.angles = [0., f32::from_bits(raw_yaw), 0.];
            super::super::game_entities::clone_player(slot, &[Value::Int(delay)], &mut host)?;
            let corpse = host
                .world
                .entities
                .get_mut(body)
                .expect("spawned fixture entity");
            let mut expected = [0u32; 60];
            for (i, word) in [
                (0, u32::from(body)),
                (1, 2),
                (2, 0xa0002),
                (3, 5),
                (4, time as u32),
                (16, raw_yaw),
                (31, 1023),
                (36, u32::from(slot)),
                (51, anim),
            ] {
                expected[i] = word;
            }
            expected[6..9].copy_from_slice(&raw_origin);
            expected[9..12].copy_from_slice(&raw_vel);
            assert_eq!(corpse.state, EntityState::from_words(expected));
            assert_eq!(corpse.corpse_think, Some(time + delay));
        }
        Ok(())
    }
}
