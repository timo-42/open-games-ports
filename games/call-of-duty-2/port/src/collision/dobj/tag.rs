//! Named bone transforms used for game entity tags.
use crate::{
    assets::models::{BoneTransform, ModelRegistry},
    vm::{game_entities::MapEntity, math::axes},
};

/// SV_DObjGetBoneIndexByName only searches the DObj model list; it does not evaluate animations.
pub(crate) fn contains(ent: &MapEntity, models: &ModelRegistry, tag: &[u8]) -> bool {
    if let Some(player) = &ent.player_skeleton {
        return player.contains_tag(models, tag);
    }
    if let Some(pose) = &ent.dobj_pose {
        return pose
            .models
            .iter()
            .any(|part| models.bone_index(part.model_index, tag).is_some());
    }
    ent.state.number() >= 64 && models.bone_index(ent.model_index, tag).is_some()
}

pub fn local_transform(
    ent: &MapEntity,
    models: &ModelRegistry,
    tag: &[u8],
) -> Option<BoneTransform> {
    if let Some(player) = &ent.player_skeleton {
        return player.tag(models, tag);
    }
    let lowered;
    let tag = if tag.iter().any(u8::is_ascii_uppercase) {
        lowered = tag.to_ascii_lowercase();
        lowered.as_slice()
    } else {
        tag
    };
    let find = |index| models.bone_index(index, tag);
    if let Some(pose) = &ent.dobj_pose {
        for submodel in &pose.models {
            if let Some(index) = find(submodel.model_index) {
                return submodel.bones.get(index).copied();
            }
        }
        return None;
    }
    if ent.state.number() < 64 {
        return None;
    }
    let model = models.get(ent.model_index)?;
    Some(model.parts.bones[find(ent.model_index)?].bind_pose)
}

pub fn world_origin(ent: &MapEntity, models: &ModelRegistry, tag: &[u8]) -> Option<[f32; 3]> {
    let local = local_transform(ent, models, tag)?.origin;
    let [forward, right, up] = axes(ent.angles);
    Some(std::array::from_fn(|i| {
        (f64::from(ent.origin[i]) + f64::from(forward[i]) * f64::from(local[0])
            - f64::from(right[i]) * f64::from(local[1])
            + f64::from(up[i]) * f64::from(local[2])) as f32
    }))
}
