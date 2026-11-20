//! The ten class-zero native GSC fields at 0x0815e820.
use super::{MapEntity, ScriptWorld, Value};
use crate::collision::CollisionMap;
use crate::vm::{entity_methods::relink, invalid};
use std::io;

pub(super) fn set_trajectory(entity: &mut MapEntity, name: &str, xyz: [f32; 3]) {
    let (base, reset) = if name == "origin" {
        entity.origin = xyz;
        (
            ["pos.trBase[0]", "pos.trBase[1]", "pos.trBase[2]"],
            [
                "pos.trType",
                "pos.trTime",
                "pos.trDuration",
                "pos.trDelta[0]",
                "pos.trDelta[1]",
                "pos.trDelta[2]",
            ],
        )
    } else {
        entity.angles = xyz;
        (
            ["apos.trBase[0]", "apos.trBase[1]", "apos.trBase[2]"],
            [
                "apos.trType",
                "apos.trTime",
                "apos.trDuration",
                "apos.trDelta[0]",
                "apos.trDelta[1]",
                "apos.trDelta[2]",
            ],
        )
    };
    for (field, component) in base.into_iter().zip(xyz) {
        entity.state.set_field(field, component.to_bits());
    }
    for field in reset {
        entity.state.set_field(field, 0);
    }
}

impl ScriptWorld {
    pub(in crate::vm) fn set_native_field(
        &mut self,
        object: u32,
        name: &str,
        value: Value,
        collision: &CollisionMap,
    ) -> io::Result<()> {
        if !matches!(
            name,
            "classname"
                | "origin"
                | "model"
                | "spawnflags"
                | "target"
                | "targetname"
                | "count"
                | "health"
                | "dmg"
                | "angles"
        ) {
            return self.set_field(object, name, value);
        }
        let Some((slot, ent)) = self.entities.for_object_mut(object) else {
            return self.set_field(object, name, value);
        };
        let value = match name {
            "classname" | "model" | "spawnflags" => {
                return Err(invalid("Tried to set a read only entity field"));
            }
            "origin" | "angles" => {
                let Value::Vector(xyz) = value else {
                    return Err(invalid("expected entity field vector"));
                };
                if name == "origin" && ent.linked {
                    let old_origin = ent.origin;
                    ent.origin = xyz;
                    if let Err(e) = relink(ent, collision, ent.model_index != 0) {
                        ent.origin = old_origin;
                        return Err(e);
                    }
                    self.objects
                        .live_fields_mut(object)?
                        .insert("angles".into(), Value::Vector(ent.angles));
                }
                set_trajectory(ent, name, xyz);
                Value::Vector(xyz)
            }
            "target" | "targetname" => {
                if value == Value::Undefined {
                    value
                } else {
                    Value::String(value.script_string()?)
                }
            }
            _ => {
                let Value::Int(n) = value else {
                    return Err(invalid("expected integer entity field"));
                };
                if name == "count"
                    && let Some(item) = ent.dropped_item.as_mut()
                {
                    item.reserve = n;
                }
                if name == "health" && self.players.get(slot).is_none() {
                    ent.max_health = n;
                }
                Value::Int(n)
            }
        };
        self.set_field(object, name, value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn vector_assignment_stops_motion_and_updates_native_fields() -> io::Result<()> {
        let collision = crate::collision::tests::map(-1, -2, &[])?;
        let mut world = ScriptWorld::default();
        let (slot, object) = world.spawn_entity(b"script_model", [0.; 3], 0)?;
        let entity = world.entities.get_mut(slot).expect("spawned script model");
        entity.state.set_field("pos.trDelta[0]", 8f32.to_bits());
        entity.state.set_field("pos.trTime", 144);
        entity.state.set_field("apos.trType", 3);
        world.set_native_field(object, "origin", Value::Vector([3., 4., 5.]), &collision)?;
        world.set_native_field(object, "angles", Value::Vector([0., 135., 0.]), &collision)?;
        let e = world.entities.get(slot).expect("spawned script model");
        assert_eq!(e.origin, [3., 4., 5.]);
        assert_eq!(e.state.field("pos.trTime"), Some(0));
        assert_eq!(e.state.field("pos.trDelta[0]"), Some(0));
        assert_eq!(e.state.field("apos.trType"), Some(0));
        assert_eq!(e.state.field("apos.trBase[1]"), Some(135f32.to_bits()));
        assert!(
            world
                .set_native_field(object, "classname", Value::Undefined, &collision)
                .is_err()
        );
        world.set_native_field(object, "target", Value::Int(12), &collision)?;
        assert_eq!(
            world.field(object, "target")?,
            Value::String(b"12".to_vec())
        );
        world.set_native_field(object, "health", Value::Int(92), &collision)?;
        assert_eq!(
            world
                .entities
                .get(slot)
                .expect("spawned script model")
                .max_health,
            92
        );
        Ok(())
    }
}
