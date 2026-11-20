//! Visibility and physical link state of a server game entity.
#[derive(Clone, Debug, Default)]
pub struct SpatialLink {
    pub absmin: [f32; 3],
    pub absmax: [f32; 3],
    pub clusters: Vec<i16>,
    pub last_cluster: i16,
    /// Membership in the dynamic collision broad phase. SV_LinkEntity can
    /// leave an entity linked for snapshots while removing it from collision.
    pub in_collision_world: bool,
    pub area_contents: u32,
    /// Sum of server XModel bounds for the current DObj (DObj_ComputeBounds).
    pub dobj_bounds: Option<[[f32; 3]; 2]>,
    pub area_mins: [f32; 2],
    pub area_maxs: [f32; 2],
}

use super::{ScriptWorld, game_entities::MapEntity, value::Value};
use crate::assets::models::ModelRegistry;
use crate::collision::CollisionMap;
use std::io;

fn invalid(s: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, s.into())
}

fn snapped_angle(value: f32) -> f32 {
    let rounded = (f64::from(value) + 0.5).floor();
    if rounded < f64::from(i32::MIN) || rounded > f64::from(i32::MAX) {
        return value;
    }
    let snapped = rounded as f32;
    let error = snapped - value;
    if error * error < 1.0000001e-6 {
        snapped
    } else {
        value
    }
}

fn absolute_bounds(entity: &MapEntity) -> ([f32; 3], [f32; 3]) {
    let mut mins = entity.mins;
    let mut maxs = entity.maxs;
    if entity.bmodel && entity.angles != [0.; 3] {
        // A yaw-only rotation expands the XY square. Pitch/roll expand all axes.
        let axes = if entity.angles[0] != 0. || entity.angles[2] != 0. {
            3
        } else {
            2
        };
        let sum: f64 = (0..axes)
            .map(|axis| {
                let extent = f64::from(mins[axis].abs().max(maxs[axis].abs()));
                extent * extent
            })
            .sum();
        let radius = (sum as f32).sqrt();
        mins[..axes].fill(-radius);
        maxs[..axes].fill(radius);
    }
    (
        std::array::from_fn(|i| (mins[i] + entity.origin[i]) - 1.),
        std::array::from_fn(|i| (maxs[i] + entity.origin[i]) + 1.),
    )
}

/// SV_LinkEntity (0x0809c63a) separates snapshot linkage from collision membership.
/// The collision membership and bounds are stored on the entity. The dynamic
/// world-sector tree and shape tracing are separate consumers of that state.
pub fn relink(entity: &mut MapEntity, collision: &CollisionMap, has_dobj: bool) -> io::Result<()> {
    let model_area_bounds = if entity.contents != 0 && has_dobj && entity.sv_flags & 6 != 0 {
        if entity.sv_flags & 2 != 0 {
            // XY components of the constants at 0x0817db88 and 0x0817db94.
            Some([[-64.; 3], [64.; 3]])
        } else {
            Some(entity.link.dobj_bounds.ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::Unsupported,
                    "SV_LinkEntity requires server DObj collision bounds",
                )
            })?)
        }
    } else {
        None
    };
    let area_contents = if entity.bmodel {
        let model = collision
            .inline_model(entity.brush_model)
            .ok_or_else(|| invalid("invalid inline collision model"))?;
        entity.mins = model.mins;
        entity.maxs = model.maxs;
        model.contents
    } else {
        // CM_TempBoxModel returns the scratch model whose brush contents are all bits.
        u32::MAX
    };
    entity.update_solid();
    entity.angles = entity.angles.map(snapped_angle);
    let (mins, maxs) = absolute_bounds(entity);
    entity.link.absmin = mins;
    entity.link.absmax = maxs;
    let (clusters, last) = if entity.sv_flags & 0x19 == 0 {
        collision.box_clusters(mins, maxs)
    } else {
        (Vec::new(), 0)
    };
    entity.link.clusters = clusters;
    entity.link.last_cluster = last;
    entity.linked = true;
    entity.link.in_collision_world = entity.contents != 0 && area_contents != 0;
    if entity.link.in_collision_world {
        entity.link.area_contents = area_contents;
        if let Some([low, high]) = model_area_bounds {
            entity.link.area_mins = std::array::from_fn(|i| entity.origin[i] + low[i]);
            entity.link.area_maxs = std::array::from_fn(|i| entity.origin[i] + high[i]);
        } else {
            entity.link.area_mins = [mins[0], mins[1]];
            entity.link.area_maxs = [maxs[0], maxs[1]];
        }
    }
    Ok(())
}

/// Complete geometry-dependent linkage after the stock map's models have been registered.
/// Spawned scripted models have one model and no attachments at this stage.
pub fn link_spawned(
    world: &mut ScriptWorld,
    collision: &CollisionMap,
    models: &ModelRegistry,
) -> io::Result<()> {
    let objects: Vec<_> = world
        .entities
        .iter()
        .filter(|(_, entity)| entity.linked)
        .map(|(_, entity)| entity.object)
        .collect();
    for object in objects {
        let (_, entity) = world
            .entities
            .for_object_mut(object)
            .expect("live map object");
        let has_dobj = entity.model_index != 0;
        if has_dobj {
            entity.link.dobj_bounds = models.get(entity.model_index).map(|model| model.bounds);
        }
        relink(entity, collision, has_dobj)?;
    }
    Ok(())
}

fn change_contents(
    entity: &mut MapEntity,
    value: i32,
    collision: &CollisionMap,
    has_dobj: bool,
) -> io::Result<i32> {
    let previous = entity.contents;
    entity.contents = value;
    if let Err(e) = relink(entity, collision, has_dobj) {
        entity.contents = previous;
        return Err(e);
    }
    Ok(previous)
}

pub fn dispatch(
    receiver: u32,
    name: &str,
    args: &[Value],
    world: &mut ScriptWorld,
    collision: &CollisionMap,
) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "hide" | "show" | "showtoplayer" | "solid" | "notsolid" | "setcontents" | "placespawnpoint"
    ) {
        return None;
    }
    Some(apply(receiver, name, args, world, collision))
}

fn apply(
    receiver: u32,
    name: &str,
    args: &[Value],
    world: &mut ScriptWorld,
    collision: &CollisionMap,
) -> io::Result<Value> {
    let slot = world
        .entities
        .slot_for_object(receiver)
        .ok_or_else(|| invalid("not an entity"))?;
    if name == "placespawnpoint" {
        return place_spawn_point(world, collision, receiver, slot);
    }
    let player = if name == "showtoplayer" {
        let Some(Value::Object(object)) = args.first() else {
            return Err(invalid("parameter 1 is not an entity"));
        };
        let player = world
            .entities
            .slot_for_object(*object)
            .ok_or_else(|| invalid("parameter 1 is not an entity"))?;
        if player >= 64 {
            return Err(invalid(
                "showToClient error: param must be a client entity\n",
            ));
        }
        Some(player)
    } else {
        None
    };
    let class = world.field(receiver, "classname")?;
    let (_, ent) = world
        .entities
        .for_object_mut(receiver)
        .expect("resolved live entity");
    let has_model = ent.model_index != 0;
    match name {
        "hide" => {
            ent.flags |= 0x800;
            ent.client_mask = [u32::MAX; 2];
        }
        "show" => {
            ent.flags &= !0x800;
            ent.client_mask = [0; 2];
        }
        "showtoplayer" => {
            let n = usize::from(player.expect("validated player"));
            ent.flags &= !0x800;
            ent.client_mask[n / 32] &= !(1u32 << (n % 32));
        }
        "solid" | "notsolid" => {
            let Value::String(class) = class else {
                return Err(invalid("invalid entity classname"));
            };
            // GScr_Solid / GScr_NotSolid: script_origin emits a developer message and returns.
            if class == b"script_origin" {
                return Ok(Value::Undefined);
            }
            if class != b"script_model" && class != b"script_brushmodel" {
                return Err(invalid(format!(
                    "entity {slot} is not a script_brushmodel, script_model, or script_origin"
                )));
            }
            let solid = name == "solid";
            let contents = if !solid {
                0
            } else if class == b"script_model" {
                0x2080
            } else {
                1
            };
            change_contents(ent, contents, collision, has_model)?;
            if class == b"script_brushmodel" {
                let flags = ent.state.field("eFlags").expect("eFlags field");
                ent.state
                    .set_field("eFlags", if solid { flags & !1 } else { flags | 1 });
            }
        }
        "setcontents" => {
            let Some(Value::Int(contents)) = args.first() else {
                return Err(invalid("setcontents requires an integer"));
            };
            let previous = change_contents(ent, *contents, collision, has_model)?;
            return Ok(Value::Int(previous));
        }
        _ => unreachable!("checked method"),
    }
    Ok(Value::Undefined)
}

// ScriptEntCmd_placespawnpoint, 0x081167ae. The original places the whole
// standing player hull, including when testing whether the landing is solid.
fn place_spawn_point(
    world: &mut ScriptWorld,
    collision: &CollisionMap,
    object: u32,
    slot: u16,
) -> io::Result<Value> {
    let origin = world
        .entities
        .get(slot)
        .expect("resolved live entity")
        .origin;
    let trace = |start, end| {
        collision.box_trace(
            &world.entities,
            start,
            end,
            [-15., -15., 0.],
            [15., 15., 70.],
            slot,
            0x0281_0011,
        )
    };
    let translated = |p: [f32; 3], dz: f32| [p[0], p[1], p[2] + dz];
    let sample = |a: [f32; 3], b: [f32; 3], fraction: f32| {
        std::array::from_fn(|i| {
            (f64::from(a[i]) + (f64::from(b[i]) - f64::from(a[i])) * f64::from(fraction)) as f32
        })
    };
    let top = translated(origin, 128.);
    let raised = sample(origin, top, trace(origin, top).fraction);
    let bottom = translated(raised, -262144.);
    let ground = trace(raised, bottom);
    let placed = sample(raised, bottom, ground.fraction);
    if trace(placed, placed).all_solid {
        let [x, y, z] = origin.map(|n| (f64::from(n) + 0.5).floor() as i32);
        eprintln!("WARNING: Spawn point entity {slot} is in solid at ({x}, {y}, {z})");
    }
    world.set_field(object, "origin", Value::Vector(placed))?;
    let entity = world.entities.get_mut(slot).expect("resolved live entity");
    entity.origin = placed;
    entity
        .state
        .set_field("groundEntityNum", u32::from(ground.entity));
    for name in [
        "pos.trType",
        "pos.trTime",
        "pos.trDuration",
        "pos.trDelta[0]",
        "pos.trDelta[1]",
        "pos.trDelta[2]",
    ] {
        entity.state.set_field(name, 0);
    }
    for (axis, name) in ["pos.trBase[0]", "pos.trBase[1]", "pos.trBase[2]"]
        .iter()
        .enumerate()
    {
        entity.state.set_field(name, placed[axis].to_bits());
    }
    Ok(Value::Undefined)
}
