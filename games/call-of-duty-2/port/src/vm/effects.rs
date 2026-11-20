//! Server-side effect entities from GScr_playFx and GScr_PlayLoopedFx.
use super::{ScriptWorld, entity_methods::relink, game_entities::MapEntity, value::Value};
use crate::collision::CollisionMap;
use crate::level::ConfigStrings;
use std::io;

type Vec3 = [f32; 3];

fn invalid(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

fn vector(value: &Value) -> io::Result<Vec3> {
    match value {
        Value::Vector(v) => Ok(*v),
        _ => Err(invalid(format!("{} is not a vector", value.type_name()))),
    }
}

fn effect_id(value: &Value) -> io::Result<i32> {
    match value {
        Value::Int(id) => Ok(*id),
        _ => Err(invalid(format!("{} is not an int", value.type_name()))),
    }
}

fn effect_error(reason: &str, id: i32, strings: &ConfigStrings) -> io::Error {
    let name = if id == 0 {
        b"not successfully loaded".as_slice()
    } else {
        strings
            .get(id.wrapping_add(0x34e) as usize)
            .unwrap_or_default()
    };
    invalid(format!(
        "{reason} (effect = {})",
        String::from_utf8_lossy(name)
    ))
}

fn dot(a: Vec3, b: Vec3) -> f32 {
    (f64::from(a[0]) * f64::from(b[0])
        + f64::from(a[1]) * f64::from(b[1])
        + f64::from(a[2]) * f64::from(b[2])) as f32
}

pub(crate) fn cross(a: Vec3, b: Vec3) -> Vec3 {
    let a = a.map(f64::from);
    let b = b.map(f64::from);
    [
        (a[1] * b[2] - a[2] * b[1]) as f32,
        (a[2] * b[0] - a[0] * b[2]) as f32,
        (a[0] * b[1] - a[1] * b[0]) as f32,
    ]
}

fn normalized(value: &Value, message: &str, id: i32, strings: &ConfigStrings) -> io::Result<Vec3> {
    let v = vector(value)?;
    let length = dot(v, v).sqrt();
    if length == 0.0 {
        return Err(effect_error(message, id, strings));
    }
    let inv = 1.0 / length;
    Ok(v.map(|n| n * inv))
}

pub(crate) fn direction_index(dir: Vec3) -> u32 {
    let mut best = 0.0;
    let mut index = 0;
    for (i, &normal) in NORMALS.iter().enumerate() {
        let score = dot(dir, normal);
        if score > best {
            best = score;
            index = i;
        }
    }
    index as u32
}

fn elevation(dir: Vec3) -> f32 {
    if dir[0] == 0.0 && dir[1] == 0.0 {
        if dir[2] > 0.0 { -90.0 } else { 90.0 }
    } else {
        let horizontal = dot([dir[0], dir[1], 0.0], [dir[0], dir[1], 0.0]).sqrt();
        (-f64::from(dir[2]).atan2(f64::from(horizontal)).to_degrees()) as f32
    }
}

pub(crate) fn angles(forward: Vec3) -> Vec3 {
    let yaw = if forward[0] == 0.0 && forward[1] == 0.0 {
        0.0
    } else {
        f64::from(forward[1])
            .atan2(f64::from(forward[0]))
            .to_degrees() as f32
    };
    let pitch = elevation(forward);
    [
        if pitch < 0.0 { pitch + 360.0 } else { pitch },
        if yaw < 0.0 { yaw + 360.0 } else { yaw },
        0.0,
    ]
}

// AxisToAngles (0x080a9558) rotates the left vector into the forward frame
// before computing roll. Using atan2(up.y,up.z) loses the vertical case.
fn play_angles(params: &[Value], id: i32, strings: &ConfigStrings) -> io::Result<Vec3> {
    let Some(fwd) = params.first() else {
        return Ok([-90.0, 0.0, 0.0]);
    };
    let forward = normalized(
        fwd,
        "playFx called with (0 0 0) forward direction",
        id,
        strings,
    )?;
    let Some(up) = params.get(1) else {
        return Ok(angles(forward));
    };
    let up = normalized(up, "playFx called with (0 0 0) up direction", id, strings)?;
    let mut left = cross(up, forward);
    let length = dot(left, left).sqrt();
    if length < 0.001 {
        return Err(effect_error(
            "playFx called an up direction 0 or 180 degrees from forward",
            id,
            strings,
        ));
    }
    if length < 0.999 {
        left = left.map(|n| n * (1.0 / length));
    }
    Ok(axis_angles(forward, left))
}

pub(crate) fn axis_angles(forward: Vec3, left: Vec3) -> Vec3 {
    let mut result = angles(forward);
    let sin_cos = |degrees: f32| {
        let radians = f64::from(-degrees).to_radians() as f32;
        let (sin, cos) = f64::from(radians).sin_cos();
        (sin as f32, cos as f32)
    };
    let (sy, cy) = sin_cos(result[1]);
    let (sp, cp) = sin_cos(result[0]);
    let [sy, cy, sp, cp] = [sy, cy, sp, cp].map(f64::from);
    let [lx, ly, lz] = left.map(f64::from);
    let x = f64::from((cy * lx - sy * ly) as f32);
    let y = (cy * ly + sy * lx) as f32;
    let roll = elevation([(cp * x + sp * lz) as f32, y, (cp * lz - sp * x) as f32]);
    result[2] = if y < 0.0 {
        if roll < 0.0 {
            roll + 180.0
        } else {
            roll - 180.0
        }
    } else {
        -roll
    };
    result
}

/// G_TempEntity links an event at a truncated position before its caller
/// fills the event parameters or sets additional broadcast flags.
pub(super) fn temp_entity(
    world: &mut ScriptWorld,
    origin: [f32; 3],
    event_type: u32,
    now: i32,
    link: impl FnOnce(&mut MapEntity) -> io::Result<()>,
) -> io::Result<&mut MapEntity> {
    let origin = origin.map(|n| {
        // Out-of-range x87 FISTP produces integer indefinite.
        if (-2147483648.0..2147483648.0).contains(&n) {
            (n as i32) as f32
        } else {
            i32::MIN as f32
        }
    });
    let (slot, _) = world.spawn_entity(b"tempEntity", origin, now)?;
    let entity = world.entities.get_mut(slot).expect("new entity");
    entity.free_after_event_at = Some(now);
    entity.stamp_event(now);
    entity.state.set_field("eType", event_type + 10);
    link(entity)?;
    Ok(entity)
}

fn play(
    args: &[Value],
    world: &mut ScriptWorld,
    strings: &ConfigStrings,
    now: i32,
    link: impl FnOnce(&mut MapEntity) -> io::Result<()>,
) -> io::Result<Value> {
    if !(2..=4).contains(&args.len()) {
        return Err(invalid("Incorrect number of parameters"));
    }
    let id = effect_id(&args[0])?;
    let entity = temp_entity(world, vector(&args[1])?, 0xc2, now, link)?;
    entity.state.set_field("eventParm", id as u32 & 255);
    let orientation = play_angles(&args[2..], id, strings)?;
    for (field, angle) in ["apos.trBase[0]", "apos.trBase[1]", "apos.trBase[2]"]
        .into_iter()
        .zip(orientation)
    {
        entity.state.set_field(field, angle.to_bits());
    }
    Ok(Value::Undefined)
}

fn looped(
    args: &[Value],
    world: &mut ScriptWorld,
    strings: &ConfigStrings,
    now: i32,
    link: impl FnOnce(&mut MapEntity) -> io::Result<()>,
) -> io::Result<Value> {
    if !(3..=6).contains(&args.len()) {
        return Err(invalid("playLoopedFx requires 3 to 6 arguments"));
    }
    let id = effect_id(&args[0])?;
    let up = match args.get(5) {
        Some(v) => normalized(
            v,
            "playLoopedFx called with (0 0 0) up direction",
            id,
            strings,
        )?,
        None => [0.; 3],
    };
    let forward = match args.get(4) {
        Some(v) => normalized(
            v,
            "playLoopedFx called with (0 0 0) forward direction",
            id,
            strings,
        )?,
        None => [0.; 3],
    };
    let cull = args.get(3).map(Value::number).transpose()?.unwrap_or(0.0);
    let origin = vector(&args[2])?;
    let repeat = (f64::from(args[1].number()? * 1000.) + 0.5).floor();
    if !(1.0..=f64::from(i32::MAX)).contains(&repeat) {
        return Err(invalid(
            "playLoopedFx repeat delay must be at least 0.0005 seconds and fit in milliseconds",
        ));
    }
    let (slot, object) = world.spawn_entity(b"noclass", origin, now)?;
    let entity = world.entities.get_mut(slot).expect("new entity");
    entity.sv_flags |= 8;
    entity.state.set_field("eType", 8);
    entity.state.set_field("scale", id as u32 & 255);
    entity.state.set_field("angles2[0]", cull.to_bits());
    entity
        .state
        .set_field("angles2[1]", (repeat as f32).to_bits());
    entity.state.set_field("eventParm", direction_index(up));
    for (field, v) in ["origin2[0]", "origin2[1]", "origin2[2]"]
        .into_iter()
        .zip(forward)
    {
        entity.state.set_field(field, v.to_bits());
    }
    link(entity)?;
    Ok(Value::Object(object))
}

pub fn dispatch(
    name: &str,
    args: &[Value],
    world: &mut ScriptWorld,
    strings: &ConfigStrings,
    now_ms: i32,
    collision: &CollisionMap,
) -> Option<io::Result<Value>> {
    let link = |entity: &mut MapEntity| relink(entity, collision, false);
    match name {
        "playfx" => Some(play(args, world, strings, now_ms, link)),
        "playloopedfx" => Some(looped(args, world, strings, now_ms, link)),
        _ => None,
    }
}

// Quake III Arena code/game/q_math.c bytedirs, id Software, GPL-2.0-or-later.
// Identical in value and order to the protocol normals at 0x0817dba0.
#[rustfmt::skip]
const NORMALS: [Vec3;162] = [
    [-0.525731, 0.0, 0.850651], [-0.442863, 0.238856, 0.864188], [-0.295242, 0.0, 0.955423],
    [-0.309017, 0.5, 0.809017], [-0.16246, 0.262866, 0.951056], [0.0, 0.0, 1.0],
    [0.0, 0.850651, 0.525731], [-0.147621, 0.716567, 0.681718], [0.147621, 0.716567, 0.681718],
    [0.0, 0.525731, 0.850651], [0.309017, 0.5, 0.809017], [0.525731, 0.0, 0.850651],
    [0.295242, 0.0, 0.955423], [0.442863, 0.238856, 0.864188], [0.16246, 0.262866, 0.951056],
    [-0.681718, 0.147621, 0.716567], [-0.809017, 0.309017, 0.5], [-0.587785, 0.425325, 0.688191],
    [-0.850651, 0.525731, 0.0], [-0.864188, 0.442863, 0.238856], [-0.716567, 0.681718, 0.147621],
    [-0.688191, 0.587785, 0.425325], [-0.5, 0.809017, 0.309017], [-0.238856, 0.864188, 0.442863],
    [-0.425325, 0.688191, 0.587785], [-0.716567, 0.681718, -0.147621], [-0.5, 0.809017, -0.309017],
    [-0.525731, 0.850651, 0.0], [0.0, 0.850651, -0.525731], [-0.238856, 0.864188, -0.442863],
    [0.0, 0.955423, -0.295242], [-0.262866, 0.951056, -0.16246], [0.0, 1.0, 0.0],
    [0.0, 0.955423, 0.295242], [-0.262866, 0.951056, 0.16246], [0.238856, 0.864188, 0.442863],
    [0.262866, 0.951056, 0.16246], [0.5, 0.809017, 0.309017], [0.238856, 0.864188, -0.442863],
    [0.262866, 0.951056, -0.16246], [0.5, 0.809017, -0.309017], [0.850651, 0.525731, 0.0],
    [0.716567, 0.681718, 0.147621], [0.716567, 0.681718, -0.147621], [0.525731, 0.850651, 0.0],
    [0.425325, 0.688191, 0.587785], [0.864188, 0.442863, 0.238856], [0.688191, 0.587785, 0.425325],
    [0.809017, 0.309017, 0.5], [0.681718, 0.147621, 0.716567], [0.587785, 0.425325, 0.688191],
    [0.955423, 0.295242, 0.0], [1.0, 0.0, 0.0], [0.951056, 0.16246, 0.262866],
    [0.850651, -0.525731, 0.0], [0.955423, -0.295242, 0.0], [0.864188, -0.442863, 0.238856],
    [0.951056, -0.16246, 0.262866], [0.809017, -0.309017, 0.5], [0.681718, -0.147621, 0.716567],
    [0.850651, 0.0, 0.525731], [0.864188, 0.442863, -0.238856], [0.809017, 0.309017, -0.5],
    [0.951056, 0.16246, -0.262866], [0.525731, 0.0, -0.850651], [0.681718, 0.147621, -0.716567],
    [0.681718, -0.147621, -0.716567], [0.850651, 0.0, -0.525731], [0.809017, -0.309017, -0.5],
    [0.864188, -0.442863, -0.238856], [0.951056, -0.16246, -0.262866], [0.147621, 0.716567, -0.681718],
    [0.309017, 0.5, -0.809017], [0.425325, 0.688191, -0.587785], [0.442863, 0.238856, -0.864188],
    [0.587785, 0.425325, -0.688191], [0.688191, 0.587785, -0.425325], [-0.147621, 0.716567, -0.681718],
    [-0.309017, 0.5, -0.809017], [0.0, 0.525731, -0.850651], [-0.525731, 0.0, -0.850651],
    [-0.442863, 0.238856, -0.864188], [-0.295242, 0.0, -0.955423], [-0.16246, 0.262866, -0.951056],
    [0.0, 0.0, -1.0], [0.295242, 0.0, -0.955423], [0.16246, 0.262866, -0.951056],
    [-0.442863, -0.238856, -0.864188], [-0.309017, -0.5, -0.809017], [-0.16246, -0.262866, -0.951056],
    [0.0, -0.850651, -0.525731], [-0.147621, -0.716567, -0.681718], [0.147621, -0.716567, -0.681718],
    [0.0, -0.525731, -0.850651], [0.309017, -0.5, -0.809017], [0.442863, -0.238856, -0.864188],
    [0.16246, -0.262866, -0.951056], [0.238856, -0.864188, -0.442863], [0.5, -0.809017, -0.309017],
    [0.425325, -0.688191, -0.587785], [0.716567, -0.681718, -0.147621], [0.688191, -0.587785, -0.425325],
    [0.587785, -0.425325, -0.688191], [0.0, -0.955423, -0.295242], [0.0, -1.0, 0.0],
    [0.262866, -0.951056, -0.16246], [0.0, -0.850651, 0.525731], [0.0, -0.955423, 0.295242],
    [0.238856, -0.864188, 0.442863], [0.262866, -0.951056, 0.16246], [0.5, -0.809017, 0.309017],
    [0.716567, -0.681718, 0.147621], [0.525731, -0.850651, 0.0], [-0.238856, -0.864188, -0.442863],
    [-0.5, -0.809017, -0.309017], [-0.262866, -0.951056, -0.16246], [-0.850651, -0.525731, 0.0],
    [-0.716567, -0.681718, -0.147621], [-0.716567, -0.681718, 0.147621], [-0.525731, -0.850651, 0.0],
    [-0.5, -0.809017, 0.309017], [-0.238856, -0.864188, 0.442863], [-0.262866, -0.951056, 0.16246],
    [-0.864188, -0.442863, 0.238856], [-0.809017, -0.309017, 0.5], [-0.688191, -0.587785, 0.425325],
    [-0.681718, -0.147621, 0.716567], [-0.442863, -0.238856, 0.864188], [-0.587785, -0.425325, 0.688191],
    [-0.309017, -0.5, 0.809017], [-0.147621, -0.716567, 0.681718], [-0.425325, -0.688191, 0.587785],
    [-0.16246, -0.262866, 0.951056], [0.442863, -0.238856, 0.864188], [0.16246, -0.262866, 0.951056],
    [0.309017, -0.5, 0.809017], [0.147621, -0.716567, 0.681718], [0.0, -0.525731, 0.850651],
    [0.425325, -0.688191, 0.587785], [0.587785, -0.425325, 0.688191], [0.688191, -0.587785, 0.425325],
    [-0.955423, 0.295242, 0.0], [-0.951056, 0.16246, 0.262866], [-1.0, 0.0, 0.0],
    [-0.850651, 0.0, 0.525731], [-0.955423, -0.295242, 0.0], [-0.951056, -0.16246, 0.262866],
    [-0.864188, 0.442863, -0.238856], [-0.951056, 0.16246, -0.262866], [-0.809017, 0.309017, -0.5],
    [-0.864188, -0.442863, -0.238856], [-0.951056, -0.16246, -0.262866], [-0.809017, -0.309017, -0.5],
    [-0.681718, 0.147621, -0.716567], [-0.681718, -0.147621, -0.716567], [-0.850651, 0.0, -0.525731],
    [-0.688191, 0.587785, -0.425325], [-0.587785, 0.425325, -0.688191], [-0.425325, 0.688191, -0.587785],
    [-0.425325, -0.688191, -0.587785], [-0.587785, -0.425325, -0.688191], [-0.688191, -0.587785, -0.425325],
];

#[cfg(test)]
mod tests {
    use super::*;
    use crate::entity_delta::EntityState;

    fn mark_linked(entity: &mut MapEntity) -> io::Result<()> {
        entity.linked = true;
        Ok(())
    }

    #[test]
    fn loop_sets_raw_network_fields_and_returns_an_entity() -> io::Result<()> {
        let mut world = ScriptWorld::default();
        let args = [
            Value::Int(257),
            Value::Float(0.1255),
            Value::Vector([0.5, -4.5, 8.25]),
            Value::Int(25),
            Value::Vector([2., 0., 0.]),
            Value::Vector([0., 0., 4.]),
        ];
        let entity = looped(
            &args,
            &mut world,
            &ConfigStrings::default(),
            1350,
            mark_linked,
        )?;
        let (slot, data) = world.entities.iter().next().expect("looped FX");
        assert_eq!(entity, Value::Object(data.object));
        assert!(data.linked);
        assert_eq!(data.sv_flags, 8);
        assert_eq!(data.free_after_event_at, None);
        let mut words = [0u32; 60];
        words[0] = slot.into();
        words[1] = 8;
        words[6..9].copy_from_slice(&[0.5f32, -4.5, 8.25].map(f32::to_bits));
        words[0xd8 / 4] = 1;
        words[0x5c / 4] = 1f32.to_bits();
        words[0x68 / 4] = 25f32.to_bits();
        // 0.1255f32 * 1000 is slightly below the 125.5 ms boundary.
        words[0x6c / 4] = 125f32.to_bits();
        words[0xa0 / 4] = 5; // Normal_DirToIndex(0,0,1)
        assert_eq!(data.state, EntityState::from_words(words));
        Ok(())
    }

    #[test]
    fn temporary_fx_snaps_origin_and_expires_after_300_ms() -> io::Result<()> {
        let mut world = ScriptWorld::default();
        let args = [Value::Int(258), Value::Vector([-1.9, 1.9, 0.99])];
        assert_eq!(
            play(
                &args,
                &mut world,
                &ConfigStrings::default(),
                100,
                mark_linked
            )?,
            Value::Undefined
        );
        let (slot, data) = world.entities.iter().next().expect("temporary FX");
        let mut words = [0; 60];
        words[0] = slot.into();
        words[1] = 204;
        words[6] = (-1.0f32).to_bits();
        words[7] = 1.0f32.to_bits();
        words[0x3c / 4] = (-90f32).to_bits();
        words[0xa0 / 4] = 2;
        assert_eq!(data.state, EntityState::from_words(words));
        assert_eq!(data.free_after_event_at, Some(100));
        world.expire_event_entities(400)?;
        assert!(world.entities.object_for_slot(slot).is_some());
        world.expire_event_entities(401)?;
        assert!(world.entities.object_for_slot(slot).is_none());
        Ok(())
    }

    #[test]
    fn invalid_forward_raises_after_temporary_entity_spawn() {
        let mut world = ScriptWorld::default();
        let mut strings = ConfigStrings::default();
        strings.set(0x34f, b"test").expect("effect slot");
        let args = [
            Value::Int(1),
            Value::Vector([0.; 3]),
            Value::Vector([0.; 3]),
        ];
        let error =
            play(&args, &mut world, &strings, 1100, mark_linked).expect_err("invalid direction");
        assert!(error.to_string().contains("effect = test"));
        assert_eq!(world.entities.iter().count(), 1);
    }

    #[test]
    fn orientation_normalizes_axes_and_rejects_parallel_up() -> io::Result<()> {
        let strings = ConfigStrings::default();
        assert_eq!(
            play_angles(&[Value::Vector([0., 0., 2.])], 0, &strings)?,
            [270., 0., 0.]
        );
        let forward = Value::Vector([1., 0., 0.]);
        assert!(play_angles(&[forward.clone(), forward.clone()], 0, &strings).is_err());
        assert_eq!(
            play_angles(&[forward, Value::Vector([0., 1., 0.])], 0, &strings)?,
            [0., 0., -90.]
        );
        Ok(())
    }
}

#[cfg(test)]
mod slope_tests {
    #[test]
    fn fallen_body_alignment_matches_carentan_contact() {
        let normal = [0xbe5ae860, 0x3bab16ee, 0x3f7a140e].map(f32::from_bits);
        let [forward, _, _] = crate::vm::math::axes([0., -180., 0.]);
        let left = super::cross(normal, forward);
        let forward = super::cross(left, normal);
        assert_eq!(
            super::axis_angles(forward, left).map(f32::to_bits),
            [0x41457f6f, 0x43341128, 0x3ea08103]
        );
    }
}
