//! Script trace bindings resolve entity references through the live game world.
use super::{
    GameHost,
    value::{ArrayKey, Value, script_error},
};
use crate::{collision::TraceResult, movement::lerp};
use std::{collections::BTreeMap, io};

const NO_ENTITY: u16 = 1023;
const WORLD: u16 = 1022;
const PLAYER_CONTENTS: u32 = 0x02000000;

fn vector(args: &[Value], index: usize) -> io::Result<[f32; 3]> {
    match args.get(index) {
        Some(Value::Vector(v)) => Ok(*v),
        Some(other) => Err(script_error(format!(
            "type {} is not a vector",
            other.type_name()
        ))),
        None => Err(script_error(format!(
            "parameter {} does not exist",
            index + 1
        ))),
    }
}
fn result(trace: TraceResult, start: [f32; 3], end: [f32; 3], entity: Value) -> Value {
    let (normal, surface) = if trace.fraction < 1. {
        let surface = ((trace.surface_flags >> 20) & 31) as usize;
        (
            trace.normal,
            *crate::combat::projectile::SURFACES
                .get(surface)
                .unwrap_or(&"default"),
        )
    } else {
        let mut direction: [f32; 3] = std::array::from_fn(|i| end[i] - start[i]);
        let length = (direction.iter().map(|&n| f64::from(n).powi(2)).sum::<f64>() as f32).sqrt();
        if length != 0. {
            let inv = length.recip();
            direction = direction.map(|v| v * inv);
        }
        (direction, "none")
    };
    Value::Array(BTreeMap::from([
        (
            ArrayKey::String(b"fraction".to_vec()),
            Value::Float(trace.fraction),
        ),
        (
            ArrayKey::String(b"position".to_vec()),
            Value::Vector(lerp(start, end, trace.fraction)),
        ),
        (ArrayKey::String(b"normal".to_vec()), Value::Vector(normal)),
        (ArrayKey::String(b"entity".to_vec()), entity),
        (
            ArrayKey::String(b"surfacetype".to_vec()),
            Value::String(surface.as_bytes().to_vec()),
        ),
    ]))
}

pub fn dispatch(name: &str, args: &[Value], host: &GameHost<'_>) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "physicstrace" | "bullettrace" | "bullettracepassed" | "sighttracepassed"
    ) {
        return None;
    }
    Some(trace(name, args, host))
}

fn trace(name: &str, args: &[Value], host: &GameHost<'_>) -> io::Result<Value> {
    let start = vector(args, 0)?;
    let end = vector(args, 1)?;
    let entities = &host.world.entities;
    if name == "physicstrace" {
        let tr = host
            .collision
            .box_trace(entities, start, end, [0.; 3], [0.; 3], NO_ENTITY, 0x820011);
        return Ok(Value::Vector(lerp(start, end, tr.fraction)));
    }
    let mut mask = if name == "sighttracepassed" {
        0x2801803
    } else {
        0x2802831
    };
    match args.get(2) {
        Some(Value::Int(0)) => mask &= !PLAYER_CONTENTS,
        Some(Value::Int(_)) => {}
        Some(v) => {
            return Err(script_error(format!(
                "type {} is not an int",
                v.type_name()
            )));
        }
        None => return Err(script_error("parameter 3 does not exist")),
    }
    let pass = match args.get(3) {
        Some(Value::Object(id)) => entities.slot_for_object(*id).unwrap_or(NO_ENTITY),
        Some(_) => NO_ENTITY,
        None => return Err(script_error("parameter 4 does not exist")),
    };
    if name == "bullettrace" {
        let tr = host.collision.trace_point(
            entities,
            host.models,
            start,
            end,
            pass,
            mask,
            None::<bool>,
        )?;
        let hit_entity = if tr.entity < WORLD {
            entities
                .object_for_slot(tr.entity)
                .map_or(Value::Undefined, Value::Object)
        } else {
            Value::Undefined
        };
        Ok(result(tr, start, end, hit_entity))
    } else {
        let clear = host.collision.point_clear(
            entities,
            host.models,
            start,
            end,
            pass,
            mask,
            name == "bullettracepassed",
        )?;
        Ok(Value::Int(i32::from(clear)))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn trace_result_keys_distinguish_world_and_miss() {
        let start = [1., 2., 3.];
        let end = [1., 6., 6.];
        let read = |record: &Value, name: &str| {
            record
                .clone()
                .index(Value::String(name.as_bytes().to_vec()))
                .expect("trace field")
        };
        let record = result(TraceResult::default(), start, end, Value::Undefined);
        assert_eq!(read(&record, "normal"), Value::Vector([0., 0.8, 0.6]));
        assert_eq!(
            read(&record, "surfacetype"),
            Value::String(b"none".to_vec())
        );
        let hit = result(
            TraceResult {
                fraction: 0.5,
                normal: [0., 0., 1.],
                surface_flags: 5 << 20,
                ..TraceResult::default()
            },
            start,
            end,
            Value::Object(9),
        );
        assert_eq!(read(&hit, "position"), Value::Vector([1., 4., 4.5]));
        assert_eq!(read(&hit, "entity"), Value::Object(9));
        assert_eq!(
            read(&hit, "surfacetype"),
            Value::String(b"concrete".to_vec())
        );
    }
}
