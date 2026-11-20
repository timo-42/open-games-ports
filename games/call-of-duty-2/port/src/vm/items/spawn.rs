//! G_FindItemForClassname and Item_SetupDroppedWeapon for BSP and script spawns.
use super::{DroppedItem, GameHost, Value, invalid, motion::set_vector};
use crate::{
    assets::{GameAssets, weapons::WeaponRegistry},
    vm::{entity_methods::relink, game_entities::MapEntity},
};
use std::io;

pub fn is_item_class(name: &[u8]) -> bool {
    name.starts_with(b"weapon_") || matches!(name, b"item_health_small" | b"item_health_large")
}
pub fn resolve(
    weapons: &mut WeaponRegistry,
    assets: &GameAssets,
    name: &str,
    initializing: bool,
) -> io::Result<usize> {
    if let Some(weapon) = name.strip_prefix("weapon_") {
        return weapons.resolve(assets, weapon, initializing);
    }
    let Some((id, class, alias, _)) = super::health_item(name) else {
        return Ok(0);
    };
    if !initializing && !weapons.item_registered(id) {
        return Err(invalid(format!(
            "game tried to register the item '{alias}' after initialization finished"
        )));
    }
    weapons.precache_item(assets, class)
}
pub fn model(weapons: &WeaponRegistry, id: usize) -> &str {
    match id {
        129 => "xmodel/health_small",
        130 => "xmodel/health_large",
        _ => weapons.get(id).map_or("", |def| def.world_model.as_str()),
    }
}
pub fn setup(
    ent: &mut MapEntity,
    id: usize,
    model: u8,
    flags: i32,
    count: i32,
    finish_at: Option<i32>,
) {
    ent.dropped_item = Some(DroppedItem {
        index: id,
        reserve: count,
        magazine: 0,
        release_owner: 0,
        list_slot: None,
        finish_at,
        suspended: flags & 1 != 0,
    });
    ent.mins = if id < 129 { [-1.; 3] } else { [-1., -1., 0.] };
    ent.maxs = if id < 129 { [1.; 3] } else { [1., 1., 2.] };
    ent.model_index = model;
    ent.contents = 0x407c0108;
    ent.flags |= 0x1000;
    for (name, v) in [("eType", 3), ("index", id as u32), ("clientNum", 1022)] {
        ent.state.set_field(name, v);
    }
    if finish_at.is_none() && flags & 1 == 0 {
        ent.state.set_field("groundEntityNum", 1023);
        if id < 129 {
            ent.angles[2] += 90.;
        }
    }
    set_vector(&mut ent.state, "apos.trBase", ent.angles);
}
pub fn script(args: &[Value], host: &mut GameHost<'_>) -> Option<io::Result<Value>> {
    let [Value::String(name), Value::Vector(pos), rest @ ..] = args else {
        return None;
    };
    if !is_item_class(name) {
        return None;
    }
    Some((|| {
        let class = std::str::from_utf8(name).map_err(|_| invalid("invalid item name"))?;
        let id = resolve(host.weapons, host.assets, class, host.initializing)?;
        if id == 0 {
            return Err(invalid(format!("{class} doesn't have a spawn function")));
        }
        let flags = match rest.first() {
            None => 0,
            Some(Value::Int(n)) => *n,
            _ => return Err(invalid("spawnflags is not an integer")),
        };
        let (_, object) = spawn_at(host, id, name, *pos, flags)?;
        Ok(Value::Object(object))
    })())
}

fn spawn_at(
    host: &mut GameHost<'_>,
    id: usize,
    name: &[u8],
    pos: [f32; 3],
    flags: i32,
) -> io::Result<(u16, u32)> {
    host.weapons
        .publish(host.assets, host.models, host.strings)?;
    let model_name = model(host.weapons, id);
    let model_index =
        host.models
            .index(host.assets, host.strings, model_name, host.initializing)?;
    let (slot, object) = host.world.spawn_entity(name, pos, host.time_ms)?;
    host.world.set_field(
        object,
        "model",
        Value::String(model_name.as_bytes().to_vec()),
    )?;
    host.world
        .set_field(object, "spawnflags", Value::Int(flags))?;
    let ent = host.world.entities.get_mut(slot).expect("spawned entity");
    setup(ent, id, model_index, flags, 0, None);
    relink(ent, host.collision, model_index != 0)?;
    Ok((slot, object))
}

/// Cmd_Give_f temporarily permits item registration and attempts a normal automatic touch.
pub(crate) fn give_named(
    vm: &mut crate::vm::fiber::FiberVm,
    host: &mut GameHost<'_>,
    receiver: u16,
    name: &str,
) -> io::Result<()> {
    let (id, class) = if let Some((id, class, _, _)) = super::health_item(name) {
        (id, class.to_owned())
    } else {
        let id = host.weapons.resolve(host.assets, name, host.initializing)?;
        if id == 0 {
            return Ok(());
        }
        (
            id,
            format!(
                "weapon_{}",
                host.weapons.get(id).expect("resolved definition").name
            ),
        )
    };
    let pos = host
        .world
        .entities
        .get(receiver)
        .ok_or_else(|| invalid("missing player"))?
        .origin;
    let previous = std::mem::replace(&mut host.initializing, true);
    if id >= 129 {
        host.weapons.register_spawned_health(id);
    }
    let spawned = spawn_at(host, id, class.as_bytes(), pos, 0);
    host.initializing = previous;
    let (slot, _) = spawned?;
    let pickup = super::touch(vm, host, receiver, slot, true);
    if host.world.entities.get(slot).is_some() {
        host.world.free_entity(slot, host.time_ms)?;
    }
    pickup
}
