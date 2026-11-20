//! The server builds a skeleton when a tag or a collision trace requests it.
//! A later request in the same frame retains bones already composed in DObj_CalcSkel.
use super::{Hierarchy, PlayerTree, PoseModel, evaluate_cached};
use crate::{
    animation::{
        PlayerAnimations,
        controllers::{PlayerControllers, ViewInput},
    },
    assets::models::{BoneTransform, ModelRegistry},
    collision::dobj::DObjPose,
    vm::{ScriptWorld, game_entities::MapEntity, model_methods::assemble_pose},
};
use std::{
    cell::{OnceCell, RefCell},
    collections::BTreeMap,
    io,
    sync::Arc,
};

#[derive(Debug, Default)]
struct Cache {
    bones: BTreeMap<usize, BoneTransform>,
    blended: Option<PlayerControllers>,
}

#[derive(Debug)]
pub(crate) struct PlayerSkeleton {
    bank: Arc<PlayerAnimations>,
    tree: PlayerTree,
    input: ViewInput,
    frame_ms: i32,
    models: Vec<(u8, Option<String>, bool)>,
    weapon: u32,
    partial: RefCell<Cache>,
    complete: OnceCell<DObjPose>,
}

impl PlayerSkeleton {
    pub(super) fn record(
        ent: &mut MapEntity,
        tree: &mut PlayerTree,
        bank: &Arc<PlayerAnimations>,
        input: ViewInput,
        frame_ms: i32,
    ) {
        let weapon = if input.flags & 0x300 == 0 {
            ent.state.field("weapon").unwrap_or(0)
        } else {
            0
        };
        let mut defs = vec![(ent.model_index, None, false)];
        defs.extend(
            ent.attachments[..6]
                .iter()
                .filter(|a| a.model_index != 0)
                .map(|a| (a.model_index, a.tag.clone(), a.ignore_collision)),
        );
        if let Some(previous) = ent.player_skeleton.as_mut() {
            previous.copy_blend(tree);
            if previous.models == defs && previous.weapon == weapon {
                previous.refresh_tree(tree);
                previous.input = input;
                previous.frame_ms = frame_ms;
                return;
            }
        }
        ent.player_skeleton = Some(Box::new(Self {
            bank: bank.clone(),
            tree: tree.clone(),
            input,
            frame_ms,
            models: defs,
            weapon,
            partial: RefCell::default(),
            complete: OnceCell::new(),
        }));
    }

    pub(crate) fn refresh_tree(&mut self, tree: &PlayerTree) {
        self.tree = tree.clone();
    }

    fn copy_blend(&self, tree: &mut PlayerTree) {
        if let Some(previous) = &self.partial.borrow().blended {
            tree.controllers.copy_blend(previous);
        }
    }

    fn invalidate(&mut self, tree: &mut PlayerTree, frame_ms: i32) {
        self.copy_blend(tree);
        self.partial.get_mut().bones.clear();
        self.partial.get_mut().blended = None;
        self.complete.take();
        self.frame_ms = frame_ms;
        self.tree = tree.clone();
    }

    fn inputs<'a>(&'a self, models: &'a ModelRegistry) -> io::Result<Vec<PoseModel<'a>>> {
        self.models
            .iter()
            .map(|(id, tag, _)| {
                Ok(PoseModel {
                    parts: &models
                        .get(*id)
                        .ok_or_else(|| io::Error::other("unregistered player model"))?
                        .parts,
                    parent_tag: tag.as_deref().filter(|t| !t.is_empty()),
                })
            })
            .collect()
    }

    fn sample(&self, inputs: &[PoseModel<'_>], advance: bool) -> io::Result<Vec<BoneTransform>> {
        let mut ctrl = self.tree.controllers.clone();
        let mut cache = self.partial.borrow_mut();
        if let Some(prior) = &cache.blended {
            ctrl.copy_blend(prior);
        }
        if advance {
            ctrl.blend(&self.input, self.frame_ms);
        }
        let bones = evaluate_cached(
            inputs,
            &self.tree.node(&self.bank, 0),
            &ctrl.bones(inputs),
            &cache.bones,
        )?;
        if advance {
            cache.blended = Some(ctrl);
        }
        Ok(bones)
    }

    fn assemble(&self, models: &ModelRegistry, advance: bool) -> io::Result<DObjPose> {
        let inputs = self.inputs(models)?;
        let definitions = self
            .models
            .iter()
            .map(|(id, tag, ignore)| (*id, tag.as_deref(), *ignore))
            .collect::<Vec<_>>();
        let bones = self.sample(&inputs, advance)?;
        Ok(assemble_pose(&inputs, &definitions, bones))
    }

    fn full(&self, models: &ModelRegistry) -> io::Result<&DObjPose> {
        if let Some(pose) = self.complete.get() {
            return Ok(pose);
        }
        let pose = self.assemble(models, true)?;
        Ok(self.complete.get_or_init(|| pose))
    }

    pub(crate) fn contains_tag(&self, models: &ModelRegistry, tag: &[u8]) -> bool {
        self.models
            .iter()
            .any(|(id, _, _)| models.bone_index(*id, tag).is_some())
    }

    pub(crate) fn tag(&self, models: &ModelRegistry, tag: &[u8]) -> Option<BoneTransform> {
        let inputs = self.inputs(models).ok()?;
        let hierarchy = Hierarchy::new(&inputs).ok()?;
        let index = *hierarchy
            .names
            .get(std::str::from_utf8(tag).ok()?.to_ascii_lowercase().as_str())?;
        if let Some(pose) = self.complete.get() {
            return pose
                .models
                .iter()
                .flat_map(|part| &part.bones)
                .nth(index)
                .copied();
        }
        if let Some(&bone) = self.partial.borrow().bones.get(&index) {
            return Some(bone);
        }
        let bones = self.sample(&inputs, true).ok()?;
        let mut partial = self.partial.borrow_mut();
        for ancestor in hierarchy.required(index) {
            partial.bones.insert(ancestor, bones[ancestor]);
        }
        Some(bones[index])
    }
}

impl MapEntity {
    pub fn pose(&self, models: &ModelRegistry) -> io::Result<Option<&DObjPose>> {
        match &self.player_skeleton {
            Some(player) => player.full(models).map(Some),
            None => Ok(self.dobj_pose.as_ref()),
        }
    }

    pub(crate) fn clone_body_pose(&self, models: &ModelRegistry) -> io::Result<Option<DObjPose>> {
        match &self.player_skeleton {
            Some(skeleton) => match skeleton.complete.get() {
                Some(complete) => Ok(Some(complete.clone())),
                None => skeleton.assemble(models, false).map(Some),
            },
            None => Ok(self.dobj_pose.clone()),
        }
    }
}

/// G_RunFrame advances the DObj clocks before trigger callbacks and entity think.
pub(crate) fn begin_frame(world: &mut ScriptWorld, now: i32) -> io::Result<()> {
    for (slot, entity) in world.entities.iter_mut() {
        let Some(skeleton) = entity.player_skeleton.as_mut() else {
            continue;
        };
        let tree = &mut world.players.get_mut(slot)?.pose_tree;
        tree.advance_clock(&world.animations, now);
        skeleton.invalidate(tree, world.frame_ms);
    }
    Ok(())
}
