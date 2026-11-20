//! G_TraceIsClearPoint and G_TraceAccumulateHit pass separate world and dynamic
//! predicates. Bullet queries also test static models and geometric entity DObjs.
use super::{CollisionMap, TraceResult, dobj, trace::dot};
use crate::{assets::models::ModelRegistry, vm::game_entities::GameEntities};
use std::io;
type Vec3 = [f32; 3];

impl CollisionMap {
    #[allow(clippy::too_many_arguments)]
    pub fn point_clear(
        &self,
        entities: &GameEntities,
        models: &ModelRegistry,
        start: Vec3,
        end: Vec3,
        pass: u16,
        mask: u32,
        bullet: bool,
    ) -> io::Result<bool> {
        if self.bsp_point_clear(0, start, end, mask) == Some(false) {
            return Ok(false);
        }
        if bullet && !self.static_models_clear(start, end, mask)? {
            return Ok(false);
        }
        // These two game wrappers explicitly pass ENTITYNUM_NONE as passOwner.
        for (slot, entity) in self.trace_candidates(entities, start, end, None, 1023, mask) {
            if pass != 1023 && (slot == pass || entity.owner_num == pass) {
                continue;
            }
            let mut hit = TraceResult::default();
            if bullet
                && entity.sv_flags & 4 != 0
                && dobj::clip_entity(models, slot, entity, start, end, mask, false, &mut hit)?
            {
                if hit.fraction < 1. {
                    return Ok(false);
                }
                continue;
            }
            let local = |p: Vec3| {
                if entity.bmodel && entity.angles != [0.; 3] {
                    let [forward, right, up] = crate::vm::math::axes(entity.angles);
                    dobj::local(p, entity.origin, [forward, right.map(|v| 0. - v), up])
                } else {
                    std::array::from_fn(|i| p[i] - entity.origin[i])
                }
            };
            let a = local(start);
            let b = local(end);
            let clear = if entity.bmodel {
                self.bsp_point_clear(entity.brush_model, a, b, mask)
                    .ok_or_else(|| super::invalid("missing entity collision model"))?
            } else {
                !capsule_blocks(a, b, entity.mins, entity.maxs)
            };
            if !clear {
                return Ok(false);
            }
        }
        Ok(true)
    }
}

// Sweep_TraceClipToBounds (0x0805c738) for a point moving against the
// temporary entity capsule. The original uses the entire 3D delta length
// even in its horizontal cylinder quadratic.
fn capsule_blocks(start: Vec3, end: Vec3, low: Vec3, high: Vec3) -> bool {
    if (0..3).any(|i| high[i] + 1. < start[i].min(end[i]) || start[i].max(end[i]) < low[i] - 1.) {
        return false;
    }
    let center: Vec3 = std::array::from_fn(|i| (low[i] + high[i]) * 0.5);
    let radius = (high[0] - center[0]).min(high[2] - center[2]);
    let height = high[2] - center[2] - radius;
    let top = center[2] + height;
    let bottom = center[2] - height;
    let delta: Vec3 = std::array::from_fn(|i| end[i] - start[i]);
    let length_sq = dot(delta, delta) as f32;
    let intersects = |z: f32, cylinder: bool| {
        let mut v = [start[0] - center[0], start[1] - center[1], start[2] - z];
        if cylinder {
            v[2] = 0.;
        }
        let r2 = radius * radius;
        let c = (dot(v, v) - f64::from(r2)) as f32;
        if c <= 0. {
            return !cylinder || (start[2] - z).abs() <= height;
        }
        let b = dot(delta, v) as f32;
        if b >= 0. {
            return false;
        }
        let disc = (f64::from(b) * f64::from(b) - f64::from(length_sq) * f64::from(c)) as f32;
        if disc < 0. {
            return false;
        }
        let length = (dot(v, v) as f32).sqrt();
        let bias = 0.125 * f64::from(b) / f64::from(length);
        let t = (-f64::from(b) - f64::from(disc.sqrt())) / f64::from(length_sq);
        if !cylinder {
            return (t + bias) < 1.;
        }
        let bias = bias as f32;
        let hit = (t + f64::from(bias)) as f32;
        hit < 1.
            && ((f64::from(hit - bias) * f64::from(delta[2]) + f64::from(start[2]) - f64::from(z))
                as f32)
                .abs()
                <= height
    };
    if start[2] > top {
        if intersects(top, false) {
            return true;
        }
        if delta[2] >= 0. {
            return false;
        }
    } else if start[2] < bottom {
        if intersects(bottom, false) {
            return true;
        }
        if delta[2] <= 0. {
            return false;
        }
    }
    intersects(center[2], true)
        || (end[2] > top && start[2] <= top && intersects(top, false))
        || (end[2] < bottom && start[2] >= bottom && intersects(bottom, false))
}

#[cfg(test)]
mod tests {
    use super::capsule_blocks;

    #[test]
    fn capsule_sight_includes_its_boundary_and_rejects_rays_above_the_capsule() {
        let low = [-5., -5., -15.];
        let high = [5., 5., 15.];
        assert!(capsule_blocks([5., 0., 0.], [25., 0., 0.], low, high));
        assert!(capsule_blocks([0., 0., 35.], [0., 0., 8.], low, high));
        assert!(!capsule_blocks([-15., 0., 16.], [15., 0., 16.], low, high));
    }
}
