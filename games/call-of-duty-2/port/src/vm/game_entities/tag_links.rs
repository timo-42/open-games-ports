//! Ent_LinkChild, Ent_UnlinkFromParent and the transforms applied by G_RunEntityState.
use super::{GameEntities, MapEntity, ScriptWorld, fields::set_trajectory, frame::basis};
use crate::{
    assets::models::ModelRegistry,
    collision::{CollisionMap, dobj::tag::local_transform},
    vm::{GameHost, effects::axis_angles, entity_methods::relink, invalid, value::Value},
};
use std::io;

type Vec3 = [f32; 3];
#[derive(Clone, Copy, Debug)]
struct Transform {
    axes: [Vec3; 3],
    origin: Vec3,
}

impl Transform {
    fn from_entity(ent: &MapEntity) -> Self {
        Self {
            axes: basis(ent.angles),
            origin: ent.origin,
        }
    }

    fn compose(&self, parent: &Self) -> Self {
        let rotate = |v: Vec3, pos: Vec3| {
            std::array::from_fn(|i| {
                ((0..3)
                    .map(|k| f64::from(v[k]) * f64::from(parent.axes[k][i]))
                    .sum::<f64>()
                    + f64::from(pos[i])) as f32
            })
        };
        Self {
            axes: self.axes.map(|v| rotate(v, [0.; 3])),
            origin: rotate(self.origin, parent.origin),
        }
    }

    fn inverse(&self) -> Self {
        let axes = std::array::from_fn(|i| std::array::from_fn(|j| self.axes[j][i]));
        let origin = self.axes.map(|row| {
            -(0..3)
                .map(|i| f64::from(row[i]) * f64::from(self.origin[i]))
                .sum::<f64>() as f32
        });
        Self { axes, origin }
    }
}

#[derive(Clone, Debug)]
pub struct TagLink {
    pub parent: u16,
    tag: Option<String>,
    relative: Transform,
}

fn parent_transform(
    entity: &MapEntity,
    models: &ModelRegistry,
    tag: Option<&str>,
) -> Option<Transform> {
    let parent = Transform::from_entity(entity);
    match tag {
        None => Some(parent),
        Some(name) => {
            let b = local_transform(entity, models, name.as_bytes())?;
            Some(
                Transform {
                    axes: b.axes(),
                    origin: b.origin,
                }
                .compose(&parent),
            )
        }
    }
}

pub fn unlink(world: &mut ScriptWorld, slot: u16) -> io::Result<()> {
    let Some(child) = world.entities.get_mut(slot) else {
        return Ok(());
    };
    if child.tag_link.take().is_none() {
        return Ok(());
    }
    set_trajectory(child, "origin", child.origin);
    set_trajectory(child, "angles", child.angles);
    if let Ok(p) = world.players.get_mut(slot) {
        let mut angles = ["viewangles[0]", "viewangles[1]", "viewangles[2]"]
            .map(|key| f32::from_bits(p.state.field(key).expect("registered view angle")));
        angles[2] = 0.;
        child.angles = crate::player::spawn::set_angles(&mut p.state, &p.command, angles);
    }
    let (obj, angles) = (child.object, child.angles);
    world.set_field(obj, "angles", Value::Vector(angles))
}

pub(super) fn release(world: &mut ScriptWorld, slot: u16) -> io::Result<()> {
    unlink(world, slot)?;
    let children: Vec<_> = world
        .entities
        .iter()
        .filter(|(_, e)| e.tag_link.as_ref().is_some_and(|l| l.parent == slot))
        .map(|(n, _)| n)
        .collect();
    for child in children {
        unlink(world, child)?;
    }
    Ok(())
}

/// Ent_UpdateChildrenTags after a non-client DObj rebuild.
pub(in crate::vm) fn update_tags(
    world: &mut ScriptWorld,
    slot: u16,
    models: &ModelRegistry,
) -> io::Result<()> {
    let parent = world
        .entities
        .get(slot)
        .ok_or_else(|| invalid("missing parent entity"))?;
    let lost: Vec<_> = world
        .entities
        .iter()
        .filter_map(|(n, e)| {
            let link = e.tag_link.as_ref()?;
            let tag = link.tag.as_ref()?;
            (link.parent == slot && local_transform(parent, models, tag.as_bytes()).is_none())
                .then_some(n)
        })
        .collect();
    for child in lost {
        unlink(world, child)?;
    }
    Ok(())
}

pub(super) fn frame_order(entities: &GameEntities) -> Vec<u16> {
    let mut seen = vec![false; entities.num_entities()];
    let mut order = Vec::with_capacity(seen.len());
    for (slot, e) in entities.iter() {
        for current in e
            .tag_link
            .as_ref()
            .map(|l| l.parent)
            .into_iter()
            .chain([slot])
        {
            if let Some(visited) = seen.get_mut(usize::from(current))
                && !std::mem::replace(visited, true)
            {
                order.push(current);
            }
        }
    }
    order
}

pub(super) fn update(
    world: &mut ScriptWorld,
    slot: u16,
    collision: &CollisionMap,
    models: &ModelRegistry,
) -> io::Result<()> {
    let Some(ent) = world.entities.get(slot) else {
        return Ok(());
    };
    let Some(link) = ent.tag_link.as_ref() else {
        return Ok(());
    };
    let is_player = slot < 64;
    if is_player {
        if world.players.get(slot).is_none_or(|p| p.input.noclip) {
            return Ok(());
        }
    } else if !matches!(ent.state.field("eType"), Some(0 | 3 | 6 | 9)) {
        return Ok(());
    }
    let parent = world
        .entities
        .get(link.parent)
        .and_then(|e| parent_transform(e, models, link.tag.as_deref()));
    let Some(parent) = parent else {
        return unlink(world, slot);
    };
    let current = link.relative.compose(&parent);
    let ent = world.entities.get_mut(slot).expect("live child");
    let angles = if is_player {
        ent.angles
    } else {
        axis_angles(current.axes[0], current.axes[1])
    };
    set_trajectory(ent, "origin", current.origin);
    set_trajectory(ent, "angles", angles);
    ent.state.set_field("pos.trType", 1);
    ent.state.set_field("apos.trType", 1);
    relink(
        ent,
        collision,
        ent.model_index != 0 || ent.dobj_pose.is_some(),
    )?;
    let (object, angles) = (ent.object, ent.angles);
    if is_player {
        let p = world.players.get_mut(slot)?;
        p.state
            .set_field("pm_type", if p.session_state == 1 { 7 } else { 1 })?;
        for (i, v) in current.origin.into_iter().enumerate() {
            p.state.words_mut()[0x14 / 4 + i] = v.to_bits();
        }
    }
    world.set_field(object, "origin", Value::Vector(current.origin))?;
    world.set_field(object, "angles", Value::Vector(angles))
}

pub(in crate::vm) fn dispatch(
    host: &mut GameHost<'_>,
    object: u32,
    name: &str,
    args: &[Value],
) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "linkto" | "unlink" | "enablelinkto" | "getorigin" | "geteye"
    ) {
        return None;
    }
    Some((|| {
        let (slot, child) = host
            .world
            .entities
            .for_object_mut(object)
            .ok_or_else(|| invalid("not an entity"))?;
        match name {
            "geteye" | "getorigin" => {
                let mut p = child.origin;
                if name == "geteye" {
                    p[2] += 40.;
                }
                return Ok(Value::Vector(p));
            }
            "unlink" => {
                unlink(host.world, slot)?;
                return Ok(Value::Undefined);
            }
            "enablelinkto" => {
                if child.flags & 0x1000 != 0 {
                    return Err(invalid("entity already has linkTo enabled"));
                }
                if child.state.field("eType") != Some(0) || child.dropped_item.is_some() {
                    return Err(invalid("entity does not currently support enableLinkTo"));
                }
                child.flags |= 0x1000;
                return Ok(Value::Undefined);
            }
            _ => {}
        }
        let Some(Value::Object(parent_obj)) = args.first() else {
            return Err(invalid("not an entity"));
        };
        let child_supports = child.flags & 0x1000 != 0;
        let parent_slot = host
            .world
            .entities
            .slot_for_object(*parent_obj)
            .ok_or_else(|| invalid("not an entity"))?;
        if !child_supports {
            return Err(invalid("entity does not currently support linkTo"));
        }
        let tag = match args.get(1) {
            None => None,
            Some(v) => {
                let b = v.script_string()?;
                let s = std::str::from_utf8(b.split(|c| *c == 0).next().unwrap_or_default())
                    .map_err(|_| invalid("invalid tag name"))?;
                (!s.is_empty()).then(|| s.to_ascii_lowercase())
            }
        };
        let explicit = if args.len() < 3 {
            None
        } else {
            let (Some(Value::Vector(origin)), Some(Value::Vector(angles))) =
                (args.get(2), args.get(3))
            else {
                return Err(invalid("expected link offset and angles vectors"));
            };
            Some(Transform {
                axes: basis(*angles),
                origin: *origin,
            })
        };
        unlink(host.world, slot)?;
        let mut ancestor = Some(parent_slot);
        while let Some(n) = ancestor {
            if n == slot {
                return Err(invalid("failed to link entity due to link cycle"));
            }
            ancestor = host
                .world
                .entities
                .get(n)
                .and_then(|e| e.tag_link.as_ref())
                .map(|l| l.parent);
        }
        let parent = host
            .world
            .entities
            .get(parent_slot)
            .expect("validated parent");
        let matrix = parent_transform(parent, host.models, tag.as_deref()).ok_or_else(|| {
            if parent.model_index == 0 {
                invalid("failed to link entity since parent has no model")
            } else {
                invalid(format!(
                    "failed to link entity since tag '{}' does not exist in parent model",
                    tag.as_deref().unwrap_or_default()
                ))
            }
        })?;
        let child = host.world.entities.get_mut(slot).expect("validated child");
        let relative =
            explicit.unwrap_or_else(|| Transform::from_entity(child).compose(&matrix.inverse()));
        child.tag_link = Some(TagLink {
            parent: parent_slot,
            tag,
            relative,
        });
        Ok(Value::Undefined)
    })())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        assets::weapons::WeaponRegistry,
        vm::{fiber::Host, model_methods::tests::setup_assets},
    };

    #[test]
    fn parent_rotation_explicit_offset_then_preserved_relative_transform() -> io::Result<()> {
        let (_root, assets) = setup_assets()?;
        let mut host = GameHost {
            assets: &assets,
            models: &mut Default::default(),
            weapons: &mut WeaponRegistry::load(&assets)?,
            start_time: 0,
            collision: &crate::collision::tests::map(-1, -2, &[])?,
            world: &mut Default::default(),
            config: &mut crate::config::ServerConfig::from_args(&[]),
            strings: &mut Default::default(),
            time_ms: 0,
            initializing: true,
        };
        let (ps, p) = host
            .world
            .spawn_entity(b"script_origin", [100., 200., 300.], 0)?;
        let (cs, c) = host.world.spawn_entity(b"script_model", [1., 2., 3.], 0)?;
        for slot in [ps, cs] {
            let e = host.world.entities.get_mut(slot).expect("spawned entity");
            e.flags |= 0x1000;
            e.state.set_field("eType", 6);
        }
        host.set_field(p, "angles", Value::Vector([0., 90., 0.]))?;
        let explicit = vec![
            Value::Object(p),
            Value::String(vec![]),
            Value::Vector([3., 4., 5.]),
            Value::Vector([12., 21., 6.]),
        ];
        host.builtin(Some(c), "linkto", explicit)?;
        assert_eq!(
            host.builtin(Some(c), "getorigin", vec![])?,
            Value::Vector([1., 2., 3.])
        );
        host.world
            .run_entity_frame(50, host.collision, host.models)?;
        let child = host.world.entities.get(cs).expect("child");
        assert_eq!(child.origin, [96., 203., 305.]);
        assert_eq!(child.angles, [12., 111., 6.]);
        assert_eq!(child.state.field("pos.trType"), Some(1));
        host.builtin(Some(c), "unlink", vec![])?;
        assert_eq!(
            host.world
                .entities
                .get(cs)
                .expect("child")
                .state
                .field("apos.trType"),
            Some(0)
        );
        host.builtin(Some(c), "linkto", vec![Value::Object(p)])?;
        host.set_field(p, "origin", Value::Vector([115., 209., 290.]))?;
        host.set_field(p, "angles", Value::Vector([0., 135., 0.]))?;
        host.world
            .run_entity_frame(100, host.collision, host.models)?;
        let child = host.world.entities.get(cs).expect("child");
        assert_eq!(
            child.origin.map(f32::to_bits),
            [0x42dc19bb, 0x43504afb, 0x43938000]
        );
        assert_eq!(child.angles, [12., 156., 6.]);
        assert!(
            host.builtin(Some(p), "linkto", vec![Value::Object(c)])
                .is_err()
        );
        host.world.free_entity(ps, 110)?;
        assert!(
            host.world
                .entities
                .get(cs)
                .expect("child")
                .tag_link
                .is_none()
        );
        Ok(())
    }
}
