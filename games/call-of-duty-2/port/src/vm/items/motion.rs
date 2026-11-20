//! Item and body trajectories (BG_EvaluateTrajectory, G_RunCorpse, Ent_StepMovement).
use crate::{
    collision::{CollisionMap, TraceResult},
    entity_delta::EntityState,
    vm::{
        entity_methods::relink,
        game_entities::{GameEntities, MapEntity},
    },
};
use std::io;
type Vec3 = [f32; 3];

fn components(base: &str) -> [&'static str; 3] {
    match base {
        "pos.trBase" => ["pos.trBase[0]", "pos.trBase[1]", "pos.trBase[2]"],
        "pos.trDelta" => ["pos.trDelta[0]", "pos.trDelta[1]", "pos.trDelta[2]"],
        "apos.trBase" => ["apos.trBase[0]", "apos.trBase[1]", "apos.trBase[2]"],
        "apos.trDelta" => ["apos.trDelta[0]", "apos.trDelta[1]", "apos.trDelta[2]"],
        _ => unreachable!(),
    }
}
pub(crate) fn set_vector(s: &mut EntityState, base: &str, v: Vec3) {
    for (key, component) in components(base).into_iter().zip(v) {
        s.set_field(key, component.to_bits());
    }
}
pub(crate) fn vector(s: &EntityState, base: &str) -> Vec3 {
    components(base).map(|k| f32::from_bits(s.field(k).expect("trajectory field")))
}
fn field(s: &EntityState, key: &str) -> i32 {
    s.field(key).expect("trajectory field") as i32
}
pub(crate) fn position(s: &EntityState, time: i32) -> Vec3 {
    use crate::vm::game_entities::mover::trajectory::{Channel, Trajectory};
    Trajectory::read(s, Channel::Position).at(time)
}
pub(crate) fn stationary(ent: &mut MapEntity, origin: Vec3, kind: u32) {
    for (field, v) in [
        ("pos.trType", kind),
        ("pos.trTime", 0),
        ("pos.trDuration", 0),
    ] {
        ent.state.set_field(field, v);
    }
    set_vector(&mut ent.state, "pos.trBase", origin);
    set_vector(&mut ent.state, "pos.trDelta", [0.; 3]);
    ent.origin = origin;
}
fn lerp(a: Vec3, b: Vec3, f: f32) -> Vec3 {
    std::array::from_fn(|i| {
        (f64::from(a[i]) + (f64::from(b[i]) - f64::from(a[i])) * f64::from(f)) as f32
    })
}
fn align(ent: &mut MapEntity, normal: Vec3, roll: f32) {
    let [forward, _, _] = crate::vm::math::axes(ent.angles);
    let left = crate::vm::effects::cross(normal, forward);
    let forward = crate::vm::effects::cross(left, normal);
    ent.angles = crate::vm::effects::axis_angles(forward, left);
    ent.angles[2] += roll;
    set_vector(&mut ent.state, "apos.trBase", ent.angles);
    set_vector(&mut ent.state, "apos.trDelta", [0.; 3]);
    for field in ["apos.trType", "apos.trTime", "apos.trDuration"] {
        ent.state.set_field(field, 0);
    }
}

/// Each query observes all other linked bodies at their current frame positions.
pub(crate) struct MotionWorld<'a> {
    pub map: &'a CollisionMap,
    pub entities: &'a GameEntities,
    pub owner: u16,
}
impl MotionWorld<'_> {
    fn trace(&self, start: Vec3, end: Vec3, mins: Vec3, maxs: Vec3, mask: u32) -> TraceResult {
        self.map
            .box_trace(self.entities, start, end, mins, maxs, self.owner, mask)
    }
    fn no_drop(&self, pos: Vec3) -> bool {
        self.map
            .point_contents(self.entities, pos, 1023, 0x80000000)
            != 0
    }
}
fn body_think(ent: &mut MapEntity, now: i32) {
    if ent.corpse_think.is_some_and(|at| at > 0 && at <= now) {
        ent.corpse_think = None;
        let flags = ent.state.field("eFlags").unwrap_or_default() & !0x80000;
        ent.state.set_field("eFlags", flags);
        ent.contents = 0x4000000;
        ent.sv_flags = 0;
    }
}

pub(crate) fn run_body(
    ent: &mut MapEntity,
    now: i32,
    world: &MotionWorld<'_>,
    root: Vec3,
) -> io::Result<bool> {
    let falling = field(&ent.state, "pos.trType") == 5;
    let moving = !falling && root.into_iter().map(|v| v * v).sum::<f32>() > 1.;
    if falling || moving {
        let mut dest = position(&ent.state, now);
        let [forward, right, _] = crate::vm::math::axes(ent.angles);
        let normalize = |v: Vec3| {
            let length = (v
                .into_iter()
                .map(|x| f64::from(x) * f64::from(x))
                .sum::<f64>() as f32)
                .sqrt();
            if length == 0. {
                [0.; 3]
            } else {
                v.map(|x| x * (1. / length))
            }
        };
        let forward = normalize(forward);
        let left = normalize(right.map(|x| -x));
        let move_from = |base: Vec3| {
            std::array::from_fn(|i| {
                let along =
                    (f64::from(base[i]) + f64::from(root[0]) * f64::from(forward[i])) as f32;
                (f64::from(along) + f64::from(root[1]) * f64::from(left[i])) as f32
            })
        };
        if moving {
            dest = move_from(dest);
        }
        let mut trace = world.trace(ent.origin, dest, ent.mins, ent.maxs, 0x10001);
        let mut stop = lerp(ent.origin, dest, trace.fraction);
        ent.origin = stop;
        relink(ent, world.map, true)?;
        body_think(ent, now);
        if !trace.start_solid && trace.fraction == 1. && moving {
            stationary(ent, stop, 1);
            let mut below = dest;
            below[2] -= 1.;
            let support = world.trace(stop, below, ent.mins, ent.maxs, 0x10001);
            if support.fraction == 1. && !support.start_solid {
                ent.state.set_field("pos.trType", 5);
                ent.state.set_field("pos.trTime", now as u32);
                set_vector(&mut ent.state, "pos.trDelta", move_from([0.; 3]));
            }
        } else if trace.start_solid || trace.fraction < 1. {
            if world.no_drop(stop) {
                return Ok(true);
            }
            if !falling {
                return Ok(false);
            }
            if trace.all_solid {
                let start = [stop[0], stop[1], stop[2] + 64.];
                trace = world.trace(start, stop, ent.mins, ent.maxs, 1);
                if !trace.all_solid {
                    stop = lerp(start, stop, trace.fraction);
                }
            }
            if trace.all_solid || trace.normal[2] > 0. {
                stationary(ent, stop, 1);
                ent.state
                    .set_field("groundEntityNum", u32::from(trace.entity));
                if !trace.all_solid {
                    align(ent, trace.normal, 0.);
                }
                relink(ent, world.map, true)?;
            } else {
                ent.origin = std::array::from_fn(|i| stop[i] + trace.normal[i]);
                set_vector(&mut ent.state, "pos.trBase", ent.origin);
                set_vector(&mut ent.state, "pos.trDelta", [0.; 3]);
                ent.state.set_field("pos.trTime", now as u32);
            }
        }
    }
    body_think(ent, now);
    Ok(false)
}

pub(crate) fn run_item(ent: &mut MapEntity, now: i32, world: &MotionWorld<'_>) -> io::Result<bool> {
    let item = ent.dropped_item.expect("item state");
    let weapon = item.index < 129;
    if let Some(at) = item.finish_at {
        if now < at {
            return Ok(false);
        }
        ent.dropped_item.as_mut().expect("map item").finish_at = None;
        if item.suspended {
            stationary(ent, ent.origin, 0);
        } else {
            let mut start = ent.origin;
            let end = [start[0], start[1], start[2] - 4096.];
            let trace_to_ground = |start| {
                world.map.box_trace(
                    world.entities,
                    start,
                    end,
                    ent.mins,
                    ent.maxs,
                    ent.state.number(),
                    0x491,
                )
            };
            let mut trace = trace_to_ground(start);
            if trace.start_solid {
                start[2] -= 15.;
                trace = trace_to_ground(start);
            }
            if trace.start_solid {
                return Ok(true);
            }
            stationary(ent, lerp(start, end, trace.fraction), 0);
            ent.state
                .set_field("groundEntityNum", u32::from(trace.entity));
            if trace.fraction < 1. {
                align(ent, trace.normal, if weapon { 90. } else { 0. });
            }
        }
        relink(ent, world.map, ent.model_index != 0)?;
        return Ok(false);
    }
    if now >= item.release_owner {
        ent.state.set_field("clientNum", 1022);
    }
    let ground = field(&ent.state, "groundEntityNum") as u16;
    let moving_ground = ground == 1023
        || world
            .entities
            .get(ground)
            .is_some_and(|e| field(&e.state, "pos.trType") != 0);
    if !item.suspended && moving_ground && field(&ent.state, "pos.trType") != 5 {
        let origin = ent.origin;
        stationary(ent, origin, 5);
        ent.state.set_field("pos.trTime", now as u32);
    }
    if matches!(field(&ent.state, "pos.trType"), 0 | 6) || ent.tag_link.is_some() {
        return Ok(false);
    }
    let previous = ent.origin;
    let mut dest = position(&ent.state, now.wrapping_add(50));
    let distance = (0..3).map(|i| (dest[i] - previous[i]).powi(2)).sum::<f32>();
    if distance < 0.1 {
        dest[2] -= 1.;
    }
    let mut trace = world.trace(previous, dest, ent.mins, ent.maxs, 0x491);
    let mut stop = dest;
    if trace.fraction < 1. {
        stop = lerp(previous, dest, trace.fraction);
        if !trace.start_solid && trace.fraction < 0.01 && trace.normal[2] < 0.5 {
            let dot = (0..3)
                .map(|i| (dest[i] - previous[i]) * trace.normal[i])
                .sum::<f32>();
            dest = std::array::from_fn(|i| dest[i] + (1. - dot) * trace.normal[i]);
            trace = world.trace(stop, dest, ent.mins, ent.maxs, 0x491);
            stop = lerp(stop, dest, trace.fraction);
        }
        set_vector(&mut ent.state, "pos.trBase", previous);
        set_vector(
            &mut ent.state,
            "pos.trDelta",
            std::array::from_fn(|i| (stop[i] - previous[i]) * 20.),
        );
        for (f, v) in [
            ("pos.trType", 3),
            ("pos.trTime", now as u32),
            ("pos.trDuration", 50),
        ] {
            ent.state.set_field(f, v);
        }
    }
    ent.origin = stop;
    relink(ent, world.map, ent.model_index != 0)?;
    if trace.fraction < 0.01 {
        if trace.normal[2] <= 0. || world.no_drop(stop) {
            return Ok(true);
        }
        align(ent, trace.normal, if weapon { 90. } else { 0. });
        stationary(ent, stop, 0);
        ent.state
            .set_field("groundEntityNum", u32::from(trace.entity));
        relink(ent, world.map, ent.model_index != 0)?;
    }
    Ok(false)
}

pub(super) fn place(ent: &mut MapEntity, pos: Vec3, angles: Vec3, ground: u32) {
    stationary(ent, pos, 0);
    ent.angles = angles;
    set_vector(&mut ent.state, "apos.trBase", angles);
    set_vector(&mut ent.state, "apos.trDelta", [0.; 3]);
    for field in ["apos.trType", "apos.trTime", "apos.trDuration"] {
        ent.state.set_field(field, 0);
    }
    ent.state.set_field("groundEntityNum", ground);
}
