//! GScr_spawnturret, GScr_precacheturret and ScriptEntCmd_useby.
use super::{Turret, occupy};
use crate::{
    assets::{GameAssets, models::ModelRegistry, weapons::WeaponRegistry},
    level::ConfigStrings,
    script::ScriptProgram,
    vm::{
        GameHost,
        entity_methods::relink,
        fiber::FiberVm,
        game_entities::{GameEntities, MapEntity, frame::IdleTurret, touch::TouchHandler},
        invalid, precache,
        value::{Value, script_error},
    },
};
use std::io;

pub(crate) fn resolve(
    registry: &mut WeaponRegistry,
    assets: &GameAssets,
    models: &mut ModelRegistry,
    strings: &mut ConfigStrings,
    weapon_name: &str,
    initializing: bool,
) -> io::Result<u8> {
    let index = registry.resolve(assets, weapon_name, initializing)?;
    if index == 0 {
        return Err(invalid(format!(
            "bad weaponinfo '{weapon_name}' specified for turret"
        )));
    }
    if registry.get(index).is_none_or(|def| def.weapon_class != 7) {
        return Err(script_error(format!(
            "G_SpawnTurret: weapon '{weapon_name}' isn't a turret. This usually indicates that the weapon failed to load."
        )));
    }
    registry.publish(assets, models, strings)?;
    for sound in &registry
        .get(index)
        .expect("resolved weapon definition")
        .turret_sounds
    {
        precache::index(strings, 0x24e, 0x100, sound.as_bytes(), true, "sound")?;
    }
    Ok(index as u8)
}

pub(in crate::vm::game_entities) fn available(entities: &GameEntities) -> io::Result<()> {
    if entities.iter().filter(|(_, e)| e.turret.is_some()).count() >= 32 {
        return Err(invalid(
            "G_SpawnTurret: max number of turrets (32) exceeded",
        ));
    }
    Ok(())
}

pub(in crate::vm::game_entities) fn setup(e: &mut MapEntity, data: Turret, weapon: u8, now: i32) {
    e.turret = Some(Box::new(data));
    e.idle_turret = Some(IdleTurret::new(now));
    e.flags |= 0x1000;
    e.clip_mask = 1;
    e.contents = 0x200004;
    e.linked = true;
    e.mins = [-32., -32., 0.];
    e.maxs = [32., 32., 56.];
    e.state.set_field("eType", 9);
    e.state.set_field("apos.trType", 3);
    e.state.set_field("weapon", u32::from(weapon));
    e.state.set_field("index", u32::from(e.model_index));
    e.update_solid();
}

pub(crate) fn global(name: &str, args: &[Value], host: &mut GameHost<'_>) -> io::Result<Value> {
    let spawning = name == "spawnturret";
    if !spawning && !host.initializing {
        return Err(script_error(
            "precacheTurret must be called before any wait statements in the level script\n",
        ));
    }
    let weapon = args
        .get(if spawning { 2 } else { 0 })
        .ok_or_else(|| script_error("missing turret weapon name"))?
        .script_string()?;
    let weapon = std::str::from_utf8(weapon.split(|&b| b == 0).next().unwrap_or_default())
        .map_err(|_| script_error("invalid turret weapon name"))?;
    if !spawning {
        host.weapons.resolve(host.assets, weapon, true)?;
        host.weapons
            .publish(host.assets, host.models, host.strings)?;
        return Ok(Value::Undefined);
    }
    let [class, Value::Vector(origin), ..] = args else {
        return Err(script_error(
            "spawnturret expects a classname, origin and weapon name",
        ));
    };
    let class = class.script_string()?;
    available(&host.world.entities)?;
    let weapon = resolve(
        host.weapons,
        host.assets,
        host.models,
        host.strings,
        weapon,
        host.initializing,
    )?;
    let damage = host
        .weapons
        .get(usize::from(weapon))
        .expect("resolved turret weapon")
        .shot
        .near_damage
        .max(0);
    let (slot, object) = host.world.spawn_entity(&class, *origin, host.time_ms)?;
    let e = host.world.entities.get_mut(slot).expect("allocated entity");
    setup(e, Turret::default(), weapon, host.time_ms);
    if let Err(error) = relink(e, host.collision, false) {
        host.world.free_entity(slot, host.time_ms)?;
        return Err(error);
    }
    host.world.set_field(object, "health", Value::Int(100))?;
    host.world.set_field(object, "dmg", Value::Int(damage))?;
    Ok(Value::Object(object))
}

pub(crate) fn use_by(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
    object: u32,
    args: &[Value],
) -> io::Result<Value> {
    let target = host
        .world
        .entities
        .slot_for_object(object)
        .ok_or_else(|| script_error("self is not an entity"))?;
    let Some(Value::Object(other)) = args.first() else {
        return Err(script_error("useby expects an entity"));
    };
    let activator = host
        .world
        .entities
        .slot_for_object(*other)
        .ok_or_else(|| script_error("useby expects an entity"))?;
    vm.notify(object, "trigger", &[Value::Object(*other)]);
    let class = host.world.field(object, "classname")?;
    let e = host
        .world
        .entities
        .get_mut(target)
        .expect("verified useby entity");
    if e.turret.is_some() {
        if host.world.players.get(activator).is_none() {
            return Err(script_error("turret occupant must be a player"));
        }
        occupy(activator, target, host, false)?;
    } else if matches!(&class,Value::String(v) if v==b"trigger_hurt") {
        e.touch_handler = match e.touch_handler {
            TouchHandler::Hurt { next_damage } => TouchHandler::HurtDisabled { next_damage },
            TouchHandler::HurtDisabled { next_damage } => TouchHandler::Hurt { next_damage },
            _ => TouchHandler::Hurt { next_damage: 0 },
        };
    } else if matches!(&class,Value::String(v) if v==b"trigger_damage") {
        crate::combat::damage::use_trigger(vm, program, host, target, *other)?;
    }
    Ok(Value::Undefined)
}
