//! Server DObj triangle and bone hit tests used by SV_ClipMoveToEntity_Line.
mod geometry;
mod hitbox;
pub mod tag;
#[cfg(test)]
mod tests;
pub use geometry::trace_model;

use super::TraceResult;
use crate::assets::models::{BoneTransform, ServerModel};
use crate::vm::game_entities::MapEntity;
use std::io;

type Vec3 = [f32; 3];

#[derive(Clone, Debug, Default)]
pub struct DObjPose {
    pub models: Vec<PosedModel>,
}

/// Absolute bone-to-DObj transforms, in XModelParts order.
#[derive(Clone, Debug)]
pub struct PosedModel {
    pub model_index: u8,
    pub bones: Vec<BoneTransform>,
    /// Global DObj bone index for an explicitly tagged attachment.
    pub parent_bone: Option<u8>,
    pub ignore_collision: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct ModelHit<'a> {
    pub fraction: f32,
    pub normal: Vec3,
    pub surface_flags: u32,
    pub contents: u32,
    pub part_name: &'a str,
    pub part_group: u8,
}

struct ModelView<'a> {
    model: &'a ServerModel,
    pose: Option<&'a [BoneTransform]>,
    parent_bone: Option<u8>,
    ignore_collision: bool,
}

fn invalid(message: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message)
}

fn dot(a: Vec3, b: Vec3) -> f64 {
    (0..3).map(|i| f64::from(a[i]) * f64::from(b[i])).sum()
}

fn sub(a: Vec3, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| a[i] - b[i])
}

pub(super) fn local(point: Vec3, origin: Vec3, axes: [Vec3; 3]) -> Vec3 {
    let delta = sub(point, origin);
    axes.map(|axis| dot(axis, delta) as f32)
}

pub(super) fn rotate_back(dir: Vec3, axes: [Vec3; 3]) -> Vec3 {
    std::array::from_fn(|i| dot(dir, [axes[0][i], axes[1][i], axes[2][i]]) as f32)
}

// Box_SegmentSlabTest tests strict overlap with [0, fraction].
pub(super) fn box_intersects(start: Vec3, end: Vec3, bounds: [Vec3; 2], mut leave: f32) -> bool {
    let mut enter = 0f32;
    let inverse = sub(start, end).map(|x| if x == 0. { 0. } else { x.recip() });
    for (face, sign) in bounds.into_iter().zip([-1., 1.]) {
        for i in 0..3 {
            let from = (start[i] - face[i]) * sign;
            let to = (end[i] - face[i]) * sign;
            if from > 0. {
                let fraction = from * inverse[i] * sign;
                if to > 0. || leave <= fraction {
                    return false;
                }
                enter = enter.max(fraction);
            } else if to > 0. {
                let fraction = from * inverse[i] * sign;
                if fraction <= enter {
                    return false;
                }
                leave = leave.min(fraction);
            }
        }
    }
    true
}

/// Returns false when the original selects the ordinary entity collision hull.
#[allow(clippy::too_many_arguments)]
pub fn clip_entity(
    models: &crate::assets::models::ModelRegistry,
    slot: u16,
    entity: &MapEntity,
    start: Vec3,
    end: Vec3,
    mask: u32,
    rifle: bool,
    hit: &mut TraceResult,
) -> io::Result<bool> {
    if entity.sv_flags & 6 == 0 || (entity.model_index == 0 && entity.dobj_pose.is_none()) {
        return Ok(false);
    }
    let geometric = entity.sv_flags & 4 != 0;
    let model_bounds = if geometric {
        entity
            .link
            .dobj_bounds
            .or_else(|| models.get(entity.model_index).map(|model| model.bounds))
            .ok_or_else(|| invalid("missing DObj model bounds"))?
    } else {
        [[-64., -64., -32.], [64., 64., 72.]]
    };
    let bounds = model_bounds.map(|corner| std::array::from_fn(|i| entity.origin[i] + corner[i]));
    if !box_intersects(start, end, bounds, hit.fraction) {
        return Ok(true);
    }
    let [forward, right, up] = crate::vm::math::axes(entity.angles);
    let axes = [forward, right.map(|v| -v), up];
    let local_start = local(start, entity.origin, axes);
    let local_end = local(end, entity.origin, axes);
    let mut views = Vec::new();
    if let Some(pose) = entity.pose(models)? {
        for part in &pose.models {
            let model = models
                .get(part.model_index)
                .ok_or_else(|| invalid("unregistered DObj model"))?;
            if model.parts.bones.len() != part.bones.len() {
                return Err(invalid("incomplete server skeleton"));
            }
            views.push(ModelView {
                model,
                pose: Some(&part.bones),
                parent_bone: part.parent_bone,
                ignore_collision: part.ignore_collision,
            });
        }
    } else {
        if !geometric && slot < 64 {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "player DObj has no evaluated animation pose",
            ));
        }
        let model = models
            .get(entity.model_index)
            .ok_or_else(|| invalid("unregistered entity model"))?;
        views.push(ModelView {
            model,
            pose: None,
            parent_bone: None,
            ignore_collision: false,
        });
    }
    let best = if geometric {
        let mut best = None;
        let mut limit = hit.fraction;
        for view in &views {
            if let Some(next) =
                trace_model(view.model, view.pose, local_start, local_end, mask, limit)?
            {
                limit = next.fraction;
                best = Some(next);
            }
        }
        best
    } else {
        hitbox::trace(&views, local_start, local_end, rifle, hit.fraction)?
    };
    if let Some(best) = best {
        hit.fraction = best.fraction;
        hit.normal = rotate_back(best.normal, axes);
        hit.contents = entity.contents as u32;
        hit.entity = slot;
        hit.surface_flags = best.surface_flags;
        hit.material = None;
        hit.part_group = u16::from(best.part_group);
        hit.part_name = models.bone_name_handle(best.part_name);
    }
    Ok(true)
}
