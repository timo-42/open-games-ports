//! Script controls for grenade collisions, stable rockets, radius damage and explosion effects.
use super::*;
use crate::vm::value::{Value, script_error};

fn integer(v: Option<&Value>) -> io::Result<i32> {
    match v {
        Some(Value::Int(i)) => Ok(*i),
        _ => Err(script_error("expected integer")),
    }
}
fn vector_arg(v: Option<&Value>) -> io::Result<Vec3> {
    match v {
        Some(Value::Vector(v)) => Ok(*v),
        _ => Err(script_error("expected vector")),
    }
}
pub(crate) fn builtin(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    receiver: Option<u32>,
    name: &str,
    args: &[Value],
) -> Option<io::Result<Value>> {
    if receiver.is_none() {
        return Some(match name {
            "grenadeexplosioneffect" => vector_arg(args.first()).and_then(|mut origin| {
                origin[2] += 1.;
                let mut end = origin;
                end[2] -= 17.;
                let surface = host
                    .collision
                    .box_trace(
                        &host.world.entities,
                        origin,
                        end,
                        [0.; 3],
                        [0.; 3],
                        1023,
                        0x811,
                    )
                    .surface_flags;
                let e = damage::temp(host, origin, 0xbc)?;
                e.state.set_field(
                    "eventParm",
                    crate::vm::effects::direction_index([0., 0., 1.]),
                );
                e.state.set_field("surfType", (surface >> 20) & 31);
                Ok(Value::Undefined)
            }),
            "setplayerignoreradiusdamage" => integer(args.first()).map(|enabled| {
                host.world.ignore_player_radius_damage = enabled != 0;
                Value::Undefined
            }),
            "radiusdamage" => (|| {
                let origin = vector_arg(args.first())?;
                let num = |i| {
                    args.get(i)
                        .ok_or_else(|| script_error("missing radiusdamage argument"))
                        .and_then(Value::number)
                };
                radius::Blast {
                    origin,
                    radius: num(1)?,
                    inner: num(2)?,
                    outer: num(3)?,
                    inflictor: None,
                    attacker: 1022,
                    weapon: 0,
                    means: 14,
                    ignore: None,
                    ignore_players: host.world.ignore_player_radius_damage,
                }
                .apply(vm, program, host)?;
                Ok(Value::Undefined)
            })(),
            _ => return None,
        });
    }
    let object = receiver?;
    let (flag, enabled) = match name {
        "setstablemissile" => (0x20000, integer(args.first()).map(|i| i != 0)),
        "enablegrenadetouchdamage" => (0x4000, Ok(true)),
        "disablegrenadetouchdamage" => (0x4000, Ok(false)),
        "enablegrenadebounce" => (0x8000, Ok(false)),
        "disablegrenadebounce" => (0x8000, Ok(true)),
        _ => return None,
    };
    Some((|| {
        let enabled = enabled?;
        if flag == 0x4000
            && host.world.field(object, "classname")? != Value::String(b"trigger_damage".to_vec())
        {
            return Err(script_error("Currently on supported on damage triggers"));
        }
        let (_, ent) = host
            .world
            .entities
            .for_object_mut(object)
            .ok_or_else(|| script_error("not an entity"))?;
        if flag == 0x20000 && field(&ent.state, "eType") != 1 {
            return Err(script_error("Type should be a player"));
        }
        if enabled {
            ent.flags |= flag;
        } else {
            ent.flags &= !flag;
        }
        Ok(Value::Undefined)
    })())
}
