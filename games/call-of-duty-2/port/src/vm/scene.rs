//! Native GSC model, spatial and transient-event bindings.
use super::{
    GameHost, ScriptWorld,
    effects::temp_entity,
    entity_methods::relink,
    game_entities::MapEntity,
    value::{Value, script_error},
};
use std::io;

fn argument(args: &[Value], i: usize) -> io::Result<&Value> {
    args.get(i)
        .ok_or_else(|| script_error(format!("parameter {} does not exist", i + 1)))
}

fn integer(v: &Value) -> io::Result<i32> {
    match v {
        Value::Int(n) => Ok(*n),
        other => Err(script_error(format!(
            "type {} is not an int",
            other.type_name()
        ))),
    }
}

fn vector(v: &Value) -> io::Result<[f32; 3]> {
    match v {
        Value::Vector(p) => Ok(*p),
        other => Err(script_error(format!(
            "type {} is not a vector",
            other.type_name()
        ))),
    }
}

fn entity(world: &ScriptWorld, object: u32) -> io::Result<&MapEntity> {
    world
        .entities
        .slot_for_object(object)
        .and_then(|n| world.entities.get(n))
        .ok_or_else(|| script_error("not an entity"))
}

fn entity_argument<'a>(
    world: &'a ScriptWorld,
    args: &[Value],
    i: usize,
) -> io::Result<&'a MapEntity> {
    match argument(args, i)? {
        Value::Object(object) => entity(world, *object),
        _ => Err(script_error(format!(
            "parameter {} is not an entity",
            i + 1
        ))),
    }
}

pub(super) fn dispatch(
    host: &mut GameHost<'_>,
    receiver: Option<u32>,
    name: &str,
    args: &[Value],
) -> Option<io::Result<Value>> {
    match (receiver, name) {
        (None, "worldentnumber") => Some(Ok(Value::Int(1022))),
        (None, "getnorthyaw") => {
            let text = host.strings.get(11).unwrap_or_default();
            let prefix = super::spawn_fields::float_prefix(&text[..text.len().min(31)]);
            let yaw = prefix
                .and_then(|(number, _)| number.parse::<f64>().ok())
                .unwrap_or(0.);
            Some(Ok(Value::Float(yaw as f32)))
        }
        (None, "getbrushmodelcenter") => Some(entity_argument(host.world, args, 0).map(|ent| {
            Value::Vector(std::array::from_fn(|i| {
                (ent.link.absmin[i] + ent.link.absmax[i]) * 0.5
            }))
        })),
        (Some(object), "localtoworldcoords") => Some((|| {
            let ent = entity(host.world, object)?;
            let local = vector(argument(args, 0)?)?;
            let [forward, right, up] = super::math::axes(ent.angles);
            // Vec3_RotateByTranspose stores the rotation as f32 before Vec3_Add.
            Ok(Value::Vector(std::array::from_fn(|i| {
                let rotated = (f64::from(local[0]) * f64::from(forward[i])
                    - f64::from(local[1]) * f64::from(right[i])
                    + f64::from(local[2]) * f64::from(up[i])) as f32;
                rotated + ent.origin[i]
            })))
        })()),
        (None, "getnumparts" | "getpartname") => Some(model_parts(host, name, args)),
        (None, "playfxontag") => Some(play_fx_on_tag(host, args)),
        (None, "earthquake") => Some(earthquake(host, args)),
        _ => None,
    }
}

fn model_parts(host: &mut GameHost<'_>, name: &str, args: &[Value]) -> io::Result<Value> {
    let path = argument(args, 0)?.script_string()?;
    let path = std::str::from_utf8(path.split(|&b| b == 0).next().unwrap_or_default())
        .map_err(|_| script_error("bad model name"))?;
    let model = host.models.register(host.assets, path)?;
    let bones = &model.parts.bones;
    if name == "getnumparts" {
        return Ok(Value::Int(bones.len() as i32));
    }
    let index = integer(argument(args, 1)?)? as u32;
    let bone = bones.get(index as usize).ok_or_else(|| {
        script_error(format!(
            "index out of range (0 - {})",
            bones.len() as i32 - 1
        ))
    })?;
    if bone.name.is_empty() {
        return Err(script_error("bad model"));
    }
    Ok(Value::String(bone.name.as_bytes().to_vec()))
}

fn play_fx_on_tag(host: &mut GameHost<'_>, args: &[Value]) -> io::Result<Value> {
    if args.len() != 3 {
        return Err(script_error(
            "USAGE: playFxOnTag <effect id from loadFx> <entity> <tag name>",
        ));
    }
    let id = integer(&args[0])?;
    if !(1..=63).contains(&id) {
        return Err(script_error(format!("effect id {id} is invalid\n")));
    }
    let ent = entity_argument(host.world, args, 1)?;
    if ent.model_index == 0 {
        return Err(script_error("cannot play fx on entity with no model"));
    }
    let bytes = args[2].script_string()?;
    let tag = bytes
        .split(|&b| b == 0)
        .next()
        .unwrap_or_default()
        .to_ascii_lowercase();
    if tag.contains(&b'"') {
        return Err(script_error("cannot use \" characters in tag names\n"));
    }
    if !crate::collision::dobj::tag::contains(ent, host.models, &tag) {
        let path = host
            .strings
            .get(crate::assets::models::MODEL_CONFIGSTRING_BASE + usize::from(ent.model_index))
            .unwrap_or_default();
        return Err(script_error(format!(
            "tag '{}' does not exist on entity with model '{}'",
            String::from_utf8_lossy(&tag),
            String::from_utf8_lossy(path)
        )));
    }
    let slot = ent.state.number();
    let mut name = format!("{id:02}").into_bytes();
    name.extend_from_slice(&tag);
    let param = super::precache::index(host.strings, 0x38e, 256, &name, true, "effect tag")? as u32;
    let ent = host.world.entities.get_mut(slot).expect("resolved entity");
    if host.world.players.get(slot).is_some() {
        host.world
            .players
            .get_mut(slot)?
            .state
            .add_event(0xc3, param as i32);
        ent.stamp_event(host.time_ms);
    } else {
        ent.add_event(0xc3, param, host.time_ms);
    }
    Ok(Value::Undefined)
}

fn earthquake(host: &mut GameHost<'_>, args: &[Value]) -> io::Result<Value> {
    let scale = argument(args, 0)?.number()?;
    let ms = argument(args, 1)?.number()? * 1000.;
    let rounded = (f64::from(ms) + 0.5).floor();
    let duration = if (f64::from(i32::MIN)..=f64::from(i32::MAX)).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    };
    let origin = vector(argument(args, 2)?)?;
    let radius = argument(args, 3)?.number()?;
    if scale <= 0. {
        return Err(script_error("Scale must be greater than 0"));
    }
    if duration < 1 {
        return Err(script_error("duration must be greater than 0"));
    }
    if radius <= 0. {
        return Err(script_error("Radius must be greater than 0"));
    }
    let ent = temp_entity(host.world, origin, 0xc4, host.time_ms, |e| {
        relink(e, host.collision, false)
    })?;
    ent.state.set_field("angles2[0]", scale.to_bits());
    ent.state.set_field("time", duration as u32);
    ent.state.set_field("angles2[1]", radius.to_bits());
    Ok(Value::Undefined)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{assets::weapons::WeaponRegistry, vm::fiber::Host};

    #[test]
    fn earthquake_checks_arguments_before_emitting_a_timed_snapshot_event() -> io::Result<()> {
        let (_root, assets) = super::super::model_methods::tests::setup_assets()?;
        let mut host = GameHost {
            assets: &assets,
            weapons: &mut WeaponRegistry::load(&assets)?,
            models: &mut Default::default(),
            world: &mut Default::default(),
            config: &mut crate::config::ServerConfig::from_args(&[]),
            collision: &crate::collision::tests::map(-1, -2, &[])?,
            strings: &mut Default::default(),
            start_time: 0,
            time_ms: 795,
            initializing: false,
        };
        let args = vec![
            Value::Float(2.5),
            Value::Float(0.0005),
            Value::Vector([1.75, -2.75, 3.]),
            Value::Int(35),
        ];
        for index in [0, 1, 3] {
            let mut bad = args.clone();
            bad[index] = Value::Int(0);
            assert!(host.builtin(None, "earthquake", bad).is_err());
        }
        assert_eq!(host.world.entities.iter().count(), 0);
        host.builtin(None, "earthquake", args)?;
        let (_, event) = host.world.entities.iter().next().expect("new earthquake");
        assert!(event.linked);
        assert_eq!(
            (
                event.origin,
                event.event_time,
                event.shared_event_time,
                event.free_after_event_at
            ),
            ([1., -2., 3.], 795, 795, Some(795))
        );
        assert_eq!(event.state.field("eType"), Some(0xc4 + 10));
        assert_eq!(event.state.field("time"), Some(1));
        assert_eq!(event.state.field("angles2[0]"), Some(2.5f32.to_bits()));
        assert_eq!(event.state.field("angles2[1]"), Some(35f32.to_bits()));
        let object = event.object;
        assert_eq!(
            host.builtin(None, "getbrushmodelcenter", vec![Value::Object(object)])?,
            Value::Vector([1., -2., 3.])
        );
        host.strings.set(11, b"  -2.5e2 degrees")?;
        assert_eq!(
            host.builtin(None, "getnorthyaw", vec![])?,
            Value::Float(-250.)
        );
        assert_eq!(
            host.builtin(None, "worldentnumber", vec![])?,
            Value::Int(1022)
        );
        for (name, args) in [
            (
                "getpartname",
                vec![Value::String(b"xmodel/test".to_vec()), Value::Int(-1)],
            ),
            (
                "getpartname",
                vec![Value::String(b"xmodel/test".to_vec()), Value::Int(0)],
            ),
            (
                "playfxontag",
                vec![
                    Value::Int(64),
                    Value::Object(object),
                    Value::String(b"tag_origin".to_vec()),
                ],
            ),
            (
                "playfxontag",
                vec![
                    Value::Int(4),
                    Value::Object(object),
                    Value::String(b"tag_origin".to_vec()),
                ],
            ),
        ] {
            assert!(host.builtin(None, name, args).is_err());
        }
        assert_eq!(
            host.builtin(
                None,
                "getnumparts",
                vec![Value::String(b"xmodel/test".to_vec())]
            )?,
            Value::Int(1)
        );
        assert!(
            host.strings
                .get(crate::assets::models::MODEL_CONFIGSTRING_BASE + 1)
                .expect("reserved model configstring")
                .is_empty()
        );
        Ok(())
    }
}
