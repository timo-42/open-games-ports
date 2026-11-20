//! PlayerCmd_cloneplayer and the eight body slots (64–71), separate from G_Spawn.
use super::{MapEntity, ScriptWorld, default_fields};
use crate::vm::{GameHost, entity_methods::relink, invalid, items::motion, value::Value};
use std::io;

impl ScriptWorld {
    fn allocate_body(&mut self, now: i32) -> io::Result<(u16, u32, u32)> {
        let slot = 64 + self.entities.corpse_ring as u16;
        self.entities.corpse_ring = (self.entities.corpse_ring + 1) & 7;
        let mut old_flags = 0;
        if let Some(ent) = self.entities.get_mut(slot) {
            old_flags = ent.state.field("eFlags").expect("entity flags");
            self.free_entity(slot, now)?;
        }
        let object = self.allocate_object()?;
        *self.object_fields_mut(object)? = default_fields();
        self.entities.slots[usize::from(slot)] = Some(MapEntity::new(slot, object));
        Ok((slot, object, (old_flags & 2) ^ 2))
    }
}

pub(crate) fn clone_player(
    slot: u16,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> io::Result<Value> {
    let Some(Value::Int(delay)) = args.first() else {
        return Err(invalid("cloneplayer requires a delay in milliseconds"));
    };
    let player = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("cloneplayer requires a player"))?;
    let field = |name| {
        player
            .state
            .field(name)
            .expect("registered player netfield")
    };
    let origin = ["origin[0]", "origin[1]", "origin[2]"].map(|n| f32::from_bits(field(n)));
    let mut velocity =
        ["velocity[0]", "velocity[1]", "velocity[2]"].map(|n| f32::from_bits(field(n)));
    let flags = field("eFlags");
    let anim = [field("legsAnim"), field("torsoAnim")];
    let client = field("clientNum");
    let tree = player.pose_tree.clone();
    let player_ent = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    let (angles, mins, maxs) = (player_ent.angles, player_ent.mins, player_ent.maxs);
    let pose = player_ent.clone_body_pose(host.models)?;
    let animation = pose
        .as_ref()
        .map(|pose| Box::new(BodyAnimation::new(tree, pose, host.models)));
    let max_speed = host
        .config
        .get("g_clonePlayerMaxVelocity")
        .and_then(|n| n.parse().ok())
        .unwrap_or(80.0f32)
        .max(0.);
    for component in &mut velocity[..2] {
        *component = component.min(max_speed);
    }
    let (number, object, teleport) = host.world.allocate_body(host.time_ms)?;
    host.world
        .set_field(object, "origin", Value::Vector(origin))?;
    host.world
        .set_field(object, "angles", Value::Vector(angles))?;
    let corpse = host.world.entities.get_mut(number).expect("allocated body");
    corpse.dobj_pose = pose;
    corpse.corpse_animation = animation;
    corpse.origin = origin;
    corpse.angles = angles;
    corpse.mins = mins;
    corpse.maxs = maxs;
    corpse.contents = 0x4002000;
    corpse.clip_mask = 0x10001;
    corpse.sv_flags = 2;
    corpse.corpse_think = Some(host.time_ms.wrapping_add(*delay));
    for (field, value) in [
        ("clientNum", client),
        ("eType", 2),
        ("eFlags", teleport | (flags & !2) | 0xa0000),
        ("pos.trType", 5),
        ("pos.trTime", host.time_ms as u32),
        ("groundEntityNum", 1023),
        ("legsAnim", anim[0]),
        ("torsoAnim", anim[1]),
    ] {
        corpse.state.set_field(field, value);
    }
    motion::set_vector(&mut corpse.state, "pos.trBase", origin);
    motion::set_vector(&mut corpse.state, "apos.trBase", angles);
    motion::set_vector(&mut corpse.state, "pos.trDelta", velocity);
    relink(corpse, host.collision, true)?;
    Ok(Value::Object(object))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn body_ring_recycles_outside_the_game_entity_free_list() -> io::Result<()> {
        let mut world = ScriptWorld::default();
        for index in 0..8 {
            let (slot, _, toggle) = world.allocate_body(index)?;
            assert_eq!(slot, 64 + index as u16);
            assert_eq!(toggle, 2);
            world
                .entities
                .get_mut(slot)
                .expect("allocated corpse")
                .state
                .set_field("eFlags", 0x20002);
        }
        assert_eq!(world.allocate_body(300)?.2, 0);
        assert_eq!(world.spawn_entity(b"test", [0.; 3], 1234)?.0, 72);
        Ok(())
    }
}

use crate::{
    animation::{
        PlayerAnimations,
        pose::{self, PlayerTree, PoseModel},
    },
    assets::models::ModelRegistry,
    collision::dobj::DObjPose,
};

/// ClonePlayer copies the current blend tree, hierarchy and attachments to
/// the ring entry, allowing it to outlive the source player's next respawn.
#[derive(Debug)]
pub(crate) struct BodyAnimation {
    tree: PlayerTree,
    parent_tags: Vec<Option<String>>,
}
impl BodyAnimation {
    fn new(tree: PlayerTree, pose: &DObjPose, registry: &ModelRegistry) -> Self {
        let mut names = vec![];
        let mut parent_tags = vec![];
        for part in &pose.models {
            parent_tags.push(
                part.parent_bone
                    .and_then(|n| names.get(usize::from(n)).cloned()),
            );
            if let Some(model) = registry.get(part.model_index) {
                names.extend(model.parts.bones.iter().map(|b| b.name.clone()));
            }
        }
        Self { tree, parent_tags }
    }
    pub(crate) fn displacement(&mut self, bank: &PlayerAnimations, now: i32) -> [f32; 3] {
        self.tree.corpse_displacement(bank, now)
    }
    pub(crate) fn update(
        ent: &mut MapEntity,
        bank: &PlayerAnimations,
        registry: &ModelRegistry,
        now: i32,
    ) -> io::Result<()> {
        let (Some(animation), Some(pose)) = (&mut ent.corpse_animation, &mut ent.dobj_pose) else {
            return Ok(());
        };
        let numbers = ["legsAnim", "torsoAnim"].map(|n| ent.state.field(n).unwrap_or(0) as u16);
        animation.tree.advance(
            bank,
            numbers,
            ent.state.field("clientNum").unwrap_or(0) as u16,
            now,
            ent.origin,
            ent.state.field("eFlags").unwrap_or(0) & 0x80000 != 0,
        );
        let tree = animation.tree.node(bank, 0);
        let models = pose
            .models
            .iter()
            .zip(&animation.parent_tags)
            .map(|(part, tag)| {
                Ok(PoseModel {
                    parts: &registry
                        .get(part.model_index)
                        .ok_or_else(|| invalid("missing corpse model"))?
                        .parts,
                    parent_tag: tag.as_deref(),
                })
            })
            .collect::<io::Result<Vec<_>>>()?;
        let bones = pose::evaluate(&models, &tree, &[])?;
        let mut offset = 0;
        for (part, model) in pose.models.iter_mut().zip(models) {
            let end = offset + model.parts.bones.len();
            part.bones.clone_from_slice(&bones[offset..end]);
            offset = end;
        }
        Ok(())
    }
}
