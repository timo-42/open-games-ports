//! Positional script sound methods from the 1.3 native table in chunk_105.
pub(crate) mod mix;
use super::{
    GameHost, ScriptWorld,
    effects::temp_entity,
    entity_methods::relink,
    game_entities::MapEntity,
    precache,
    value::{Value, error},
};
use crate::level::ConfigStrings;
use std::io;

/// G_SoundIndex creates an entry on first use even after map initialization.
pub(crate) fn sound_index(args: &[Value], strings: &mut ConfigStrings) -> io::Result<u32> {
    let name = args
        .first()
        .ok_or_else(|| error("parameter 1 does not exist"))?
        .script_string()?;
    let name = name.split(|&b| b == 0).next().unwrap_or_default();
    alias_index(name, strings)
}

pub(crate) fn alias_index(name: &[u8], strings: &mut ConfigStrings) -> io::Result<u32> {
    precache::index(strings, 0x24e, 0x100, name, true, "sound").map(|n| n as u32)
}

pub fn dispatch(
    object: u32,
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "playsound" | "playsoundasmaster" | "playloopsound" | "stoploopsound"
    ) {
        return None;
    }
    Some(positional(
        object,
        name,
        args,
        host.world,
        host.strings,
        host.time_ms,
        |entity| relink(entity, host.collision, false),
    ))
}

fn positional(
    object: u32,
    name: &str,
    args: &[Value],
    world: &mut ScriptWorld,
    strings: &mut ConfigStrings,
    now: i32,
    link: impl FnOnce(&mut MapEntity) -> io::Result<()>,
) -> io::Result<Value> {
    let (_, emitter) = world
        .entities
        .for_object_mut(object)
        .ok_or_else(|| error("not an entity"))?;
    if name == "stoploopsound" {
        emitter.broadcast_time = now.wrapping_add(300);
        emitter.state.set_field("loopSound", 0);
        return Ok(Value::Undefined);
    }
    let index = sound_index(args, strings)?;
    if name == "playloopsound" {
        emitter.broadcast_time = -1;
        emitter.state.set_field("loopSound", index);
    } else {
        let origin = emitter.origin;
        let event_type = if name == "playsound" { 0xb3 } else { 0xb4 };
        let event = temp_entity(world, origin, event_type, now, link)?;
        event.sv_flags |= 8;
        event.state.set_field("eventParm", index);
    }
    Ok(Value::Undefined)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn positional_sound_updates_loop_and_transient_broadcast_state() -> io::Result<()> {
        let mut world = ScriptWorld::default();
        let (slot, object) = world.spawn_entity(b"script_origin", [1.75, -2.75, 3.], 0)?;
        let mut strings = ConfigStrings::default();
        let args = [Value::String(b"Rumble".to_vec())];
        let link = |e: &mut MapEntity| {
            e.linked = true;
            Ok(())
        };
        positional(
            object,
            "playloopsound",
            &args,
            &mut world,
            &mut strings,
            100,
            link,
        )?;
        let e = world.entities.get_mut(slot).expect("emitter");
        assert_eq!(e.state.field("loopSound"), Some(1));
        assert_eq!(e.broadcast_time, -1);
        assert_eq!(strings.get(0x24f), Some(b"Rumble".as_slice()));
        positional(
            object,
            "stoploopsound",
            &[],
            &mut world,
            &mut strings,
            150,
            link,
        )?;
        let e = world.entities.get_mut(slot).expect("emitter");
        assert_eq!(e.state.field("loopSound"), Some(0));
        assert_eq!(e.broadcast_time, 450);
        let args = [Value::String(b"rumble".to_vec())];
        for (method, kind) in [("playsound", 0xb3), ("playsoundasmaster", 0xb4)] {
            positional(object, method, &args, &mut world, &mut strings, 575, link)?;
            let (_, event) = world.entities.iter().last().expect("transient entity");
            assert_eq!(event.state.field("eType"), Some(kind + 10));
            assert_eq!(event.state.field("eventParm"), Some(1));
            assert_eq!(event.sv_flags, 8);
            assert_eq!(event.free_after_event_at, Some(575));
            assert_eq!(event.origin, [1., -2., 3.]);
            assert!(event.linked);
        }
        assert_eq!(strings.populated().count(), 1);
        Ok(())
    }
}
