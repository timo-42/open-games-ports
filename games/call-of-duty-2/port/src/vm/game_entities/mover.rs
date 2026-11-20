//! Script mover method table, timed phases, collision pushing and completion notifications.
mod push;
pub(crate) mod trajectory;

use super::{MapEntity, ScriptWorld};
use crate::collision::CollisionMap;
use crate::vm::{
    entity_methods::relink,
    value::{Value, script_error},
};
use std::io;
use trajectory::{Channel, Plan, Trajectory, Vec3, subtract};

#[derive(Debug, Default)]
pub(super) struct Mover {
    position: Plan,
    rotation: Plan,
}

pub(crate) fn dispatch(
    object: u32,
    name: &str,
    args: &[Value],
    world: &mut ScriptWorld,
    collision: &CollisionMap,
    now: i32,
) -> Option<io::Result<Value>> {
    let channel = match name {
        "moveto" | "movex" | "movey" | "movez" | "movegravity" => Channel::Position,
        "rotateto" | "rotatepitch" | "rotateyaw" | "rotateroll" | "rotatevelocity" => {
            Channel::Angles
        }
        _ => return None,
    };
    Some((|| {
        let class = world.field(object, "classname")?;
        let (slot, e) = world
            .entities
            .for_object_mut(object)
            .ok_or_else(|| script_error("not an entity"))?;
        if !matches!(class, Value::String(ref c) if matches!(c.as_slice(),b"script_origin"|b"script_brushmodel"|b"script_model"))
        {
            return Err(script_error(format!(
                "entity {slot} is not a script_brushmodel, script_model, or script_origin"
            )));
        }
        let arg = |i: usize| {
            args.get(i)
                .ok_or_else(|| script_error(format!("parameter {} does not exist", i + 1)))
        };
        let number = |i| arg(i)?.number();
        let current = match channel {
            Channel::Position => e.origin,
            Channel::Angles => e.angles,
        };
        let target = match name {
            "movex" | "movey" | "movez" | "rotatepitch" | "rotateyaw" | "rotateroll" => {
                let axis = match name {
                    "movex" | "rotatepitch" => 0,
                    "movey" | "rotateyaw" => 1,
                    _ => 2,
                };
                let mut target = current;
                target[axis] += number(0)?;
                target
            }
            _ => {
                let Value::Vector(target) = arg(0)? else {
                    return Err(script_error("expected a vector"));
                };
                if name == "rotateto" {
                    std::array::from_fn(|i| {
                        let mut d = (target[i] - current[i]) % 360.;
                        if d > 180. {
                            d -= 360.;
                        } else if d < -180. {
                            d += 360.;
                        }
                        current[i] + d
                    })
                } else {
                    *target
                }
            }
        };
        let total = number(1)?;
        let mut tr = Trajectory::read(&e.state, channel);
        let plan = if name == "movegravity" {
            tr = Trajectory::gravity(now, total, current, target);
            Plan::default()
        } else {
            if total <= 0. {
                return Err(script_error("total time must be positive"));
            }
            let accel = args.get(2).map(Value::number).transpose()?.unwrap_or(0.);
            if accel < 0. {
                return Err(script_error("accel time must be nonnegative"));
            }
            let decel = args.get(3).map(Value::number).transpose()?.unwrap_or(0.);
            if decel < 0. {
                return Err(script_error("decel time must be nonnegative"));
            }
            if f64::from(total) < f64::from(accel) + f64::from(decel) {
                return Err(script_error(
                    "accel time plus decel time is greater than total time",
                ));
            }
            Plan::start(
                &mut tr,
                now,
                current,
                target,
                name == "rotatevelocity",
                total,
                accel,
                decel,
            )
        };
        let mover = e.mover.get_or_insert_with(Default::default);
        match channel {
            Channel::Position => {
                e.origin = tr.at(now);
                mover.position = plan;
            }
            Channel::Angles => {
                e.angles = tr.at(now);
                mover.rotation = plan;
            }
        }
        tr.write(&mut e.state, channel);
        relink(e, collision, e.model_index != 0)?;
        Ok(Value::Undefined)
    })())
}

pub(super) type Notices = Vec<(u32, &'static str)>;

fn reached(
    entity: &mut MapEntity,
    now: i32,
    map: &CollisionMap,
    notices: &mut Notices,
) -> io::Result<()> {
    let mover = entity.mover.as_deref().expect("active script mover");
    let phases = [
        (Channel::Position, mover.position),
        (Channel::Angles, mover.rotation),
    ];
    for (channel, phase) in phases {
        let mut tr = Trajectory::read(&entity.state, channel);
        if !tr.due(now) {
            continue;
        }
        let finished = phase.next(&mut tr, now);
        tr.write(&mut entity.state, channel);
        let value = tr.at(now);
        match channel {
            Channel::Position => entity.origin = value,
            Channel::Angles => entity.angles = value,
        }
        relink(entity, map, entity.model_index != 0)?;
        if finished {
            let event = match channel {
                Channel::Position => "movedone",
                Channel::Angles => {
                    let [pitch, yaw, roll] = entity.angles;
                    entity.angles = [
                        crate::player::spawn::angle_normalize(pitch),
                        crate::player::spawn::angle_mod(yaw),
                        crate::player::spawn::angle_normalize(roll),
                    ];
                    "rotatedone"
                }
            };
            notices.push((entity.object, event));
        }
    }
    Ok(())
}

pub(super) fn step(
    world: &mut ScriptWorld,
    slot: u16,
    map: &CollisionMap,
    now: i32,
    notices: &mut Notices,
) -> io::Result<()> {
    let mover = world.entities.get(slot).expect("current mover");
    let mut pos = Trajectory::read(&mover.state, Channel::Position);
    let mut rot = Trajectory::read(&mover.state, Channel::Angles);
    if pos.kind == 0 && rot.kind == 0 {
        return Ok(());
    }
    let move_by = subtract(pos.at(now), mover.origin);
    let turn_by = subtract(rot.at(now), mover.angles);
    let blocked = push::move_and_push(world, slot, map, move_by, turn_by)?;
    let mover = world.entities.get_mut(slot).expect("current mover");
    if blocked {
        pos.start = pos.start.wrapping_add(world.frame_ms);
        rot.start = rot.start.wrapping_add(world.frame_ms);
        pos.write(&mut mover.state, Channel::Position);
        rot.write(&mut mover.state, Channel::Angles);
        mover.origin = pos.at(now);
        mover.angles = rot.at(now);
        return relink(mover, map, mover.model_index != 0);
    }
    if pos.due(now) {
        reached(mover, now, map, notices)?;
    }
    if Trajectory::read(&mover.state, Channel::Angles).due(now) {
        reached(mover, now, map, notices)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn late_frame_starts_next_phase_and_notifies_position_before_rotation() -> io::Result<()> {
        let map = crate::collision::tests::map(-1, -2, &[])?;
        let mut world = ScriptWorld::default();
        let (slot, object) = world.spawn_entity(b"script_origin", [0.; 3], 0)?;
        let times = [Value::Float(1.), Value::Float(0.25), Value::Float(0.5)];
        for (op, value) in [("movex", 80.), ("rotateyaw", 160.)] {
            let mut args = vec![Value::Float(value)];
            args.extend_from_slice(&times);
            dispatch(object, op, &args, &mut world, &map, 0).expect("mover method")?;
        }
        let mut notices = vec![];
        for (now, kind, start) in [(200, 7, 0), (300, 3, 300), (550, 8, 550), (1050, 0, 1050)] {
            step(&mut world, slot, &map, now, &mut notices)?;
            let e = world.entities.get(slot).expect("script mover");
            assert_eq!(e.state.field("pos.trType"), Some(kind));
            assert_eq!(e.state.field("pos.trTime"), Some(start));
            assert_eq!(e.state.field("apos.trType"), Some(kind));
            if now < 1050 {
                assert!(notices.is_empty());
            }
        }
        let mover = world.entities.get(slot).expect("script mover");
        assert_eq!(mover.origin, [80., 0., 0.]);
        assert_eq!(mover.angles[1], crate::player::spawn::angle_mod(160.));
        assert_eq!(notices, [(object, "movedone"), (object, "rotatedone")]);
        let state = mover.state.clone();
        assert!(
            dispatch(
                object,
                "movex",
                &[Value::Int(23), Value::Float(0.), Value::Int(1)],
                &mut world,
                &map,
                1100
            )
            .expect("mover method")
            .is_err()
        );
        assert_eq!(world.entities.get(slot).expect("script mover").state, state);
        Ok(())
    }
}
