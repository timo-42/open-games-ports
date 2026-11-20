//! G_TraceBox and the non-locational path through SV_Trace.
use super::{CollisionMap, TraceResult, dobj, trace_capsule};
use crate::vm::game_entities::{GameEntities, MapEntity, touch::sector_path};
type Vec3 = [f32; 3];

/// SpatialTree_TraceSegmentHits and SpatialTree_TraceBoxHits widen each XY
/// split for the moving hull and visit the near child first. Return the
/// corresponding preorder key only if this segment reaches the entity's node.
fn sector_order(
    path: impl IntoIterator<Item = (usize, f32, u8)>,
    mut start: Vec3,
    mut end: Vec3,
    extents: Option<Vec3>,
) -> Option<Vec<u8>> {
    let mut key = Vec::new();
    for (axis, split, side) in path {
        let a = start[axis] - split;
        let b = end[axis] - split;
        let only = match extents {
            None if a * b >= 0. => Some(u8::from(a.min(b) < 0.)),
            Some(half) if a.min(b) >= half[axis] => Some(0),
            Some(half) if a.max(b) <= -half[axis] => Some(1),
            _ => None,
        };
        if let Some(only) = only {
            if side != only {
                return None;
            }
            key.push(0);
            continue;
        }
        let width = extents.map_or(0., |e| e[axis]);
        let delta = b - a;
        let near = u8::from(delta > 0.);
        let (first, last) = if delta == 0. {
            (0., 1.)
        } else {
            let distance = if delta > 0. { -a } else { a };
            let inv = delta.abs().recip();
            (
                ((distance - width) * inv).max(0.),
                ((distance + width) * inv).min(1.),
            )
        };
        let t = if side == near { last } else { first };
        let mid = std::array::from_fn(|i| {
            (f64::from(start[i]) + (f64::from(end[i]) - f64::from(start[i])) * f64::from(t)) as f32
        });
        if side == near {
            end = mid;
        } else {
            start = mid;
        }
        key.push(side ^ near);
    }
    Some(key)
}

impl CollisionMap {
    /// Sweep a vertical capsule against one transformed inline model or temporary entity capsule.
    pub fn trace_entity(
        &self,
        entity: &MapEntity,
        start: Vec3,
        end: Vec3,
        mins: Vec3,
        maxs: Vec3,
        mask: u32,
    ) -> Option<TraceResult> {
        let center: Vec3 = std::array::from_fn(|i| (mins[i] + maxs[i]) * 0.5);
        let low = std::array::from_fn(|i| mins[i] - center[i]);
        let high = std::array::from_fn(|i| maxs[i] - center[i]);
        let centered = |p: Vec3| std::array::from_fn(|i| p[i] + center[i]);
        let offset = |p: Vec3| std::array::from_fn(|i| p[i] - entity.origin[i]);
        let start = centered(start);
        let end = centered(end);
        if !entity.bmodel {
            if entity.contents as u32 & mask == 0 {
                return Some(TraceResult::default());
            }
            return Some(trace_capsule(
                offset(start),
                offset(end),
                low,
                high,
                entity.mins,
                entity.maxs,
                entity.contents as u32,
            ));
        }
        if entity.angles == [0.; 3] {
            return self.trace_model(
                entity.brush_model,
                offset(start),
                offset(end),
                low,
                high,
                mask,
            );
        }
        let [forward, right, up] = crate::vm::math::axes(entity.angles);
        let axes = [forward, right.map(|v| -v), up];
        let start = dobj::local(start, entity.origin, axes);
        let end = dobj::local(end, entity.origin, axes);
        let mut hit = self.trace_model(entity.brush_model, start, end, low, high, mask)?;
        if hit.fraction < 1. {
            hit.normal = dobj::rotate_back(hit.normal, axes);
        }
        Some(hit)
    }

    /// Locational and ordinary traces have different narrow phases and share
    /// their server world-sector traversal and pass-entity ownership rules.
    pub(super) fn trace_candidates<'a>(
        &self,
        entities: &'a GameEntities,
        start: Vec3,
        end: Vec3,
        extents: Option<Vec3>,
        pass: u16,
        mask: u32,
    ) -> impl Iterator<Item = (u16, &'a MapEntity)> {
        let owner = entities
            .get(pass)
            .map(|e| e.owner_num)
            .filter(|&n| n != 1023);
        let mut candidates = Vec::new();
        for (slot, entity) in entities.iter() {
            if !entity.linked
                || !entity.link.in_collision_world
                || entity.contents as u32 & mask == 0
            {
                continue;
            }
            if pass != 1023
                && (slot == pass || entity.owner_num == pass || Some(entity.owner_num) == owner)
            {
                continue;
            }
            if let Some(path) = sector_order(sector_path(entity, self), start, end, extents) {
                candidates.push((path, slot, entity));
            }
        }
        candidates.sort_unstable_by(|a, b| (&a.0, a.1).cmp(&(&b.0, b.1)));
        candidates
            .into_iter()
            .map(|(_, slot, entity)| (slot, entity))
    }

    /// SV_Trace with locational and static-model extra checks disabled (0x0809d884).
    #[allow(clippy::too_many_arguments)]
    pub fn box_trace(
        &self,
        entities: &GameEntities,
        start: Vec3,
        end: Vec3,
        mins: Vec3,
        maxs: Vec3,
        pass_entity: u16,
        mask: u32,
    ) -> TraceResult {
        let mut hit = self.trace(start, end, mins, maxs, mask);
        if hit.fraction == 0. {
            return hit;
        }
        let line = (((maxs[0] - mins[0] + maxs[1]) - mins[1] + maxs[2]) - mins[2]) == 0.;
        let center: Vec3 = if line {
            [0.; 3]
        } else {
            std::array::from_fn(|i| (maxs[i] + mins[i]) * 0.5)
        };
        let half: Vec3 = if line {
            [0.; 3]
        } else {
            std::array::from_fn(|i| (maxs[i] - mins[i]) * 0.5)
        };
        let start = std::array::from_fn(|i| start[i] + center[i]);
        let end = std::array::from_fn(|i| end[i] + center[i]);
        let low = half.map(|x| -x);
        let extents = (!line).then(|| half.map(|v| v + 1.));
        for (slot, entity) in
            self.trace_candidates(entities, start, end, extents, pass_entity, mask)
        {
            if entity.link.area_contents & mask == 0 {
                continue;
            }
            let bounds = [
                std::array::from_fn(|i| entity.link.absmin[i] + low[i]),
                std::array::from_fn(|i| entity.link.absmax[i] + half[i]),
            ];
            if !dobj::box_intersects(start, end, bounds, hit.fraction) {
                continue;
            }
            let Some(mut candidate) = self.trace_entity(entity, start, end, low, half, mask) else {
                continue;
            };
            let start_solid = hit.start_solid || candidate.start_solid;
            let all_solid = hit.all_solid || candidate.all_solid;
            if candidate.fraction < hit.fraction {
                candidate.entity = slot;
                if line {
                    candidate.surface_flags = 0;
                    candidate.contents = entity.contents as u32;
                    candidate.material = None;
                }
                hit = candidate;
            }
            hit.start_solid = start_solid;
            hit.all_solid = all_solid;
        }
        hit
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vm::{ScriptWorld, entity_methods::relink};
    use std::io;

    #[test]
    fn entity_line_hits_choose_the_first_slot_and_honor_pass_ownership() -> io::Result<()> {
        let mut plane = [0; 16];
        plane[..4].copy_from_slice(&1f32.to_le_bytes());
        let mut node = [0; 36];
        node[4..12].fill(255);
        let map = CollisionMap::decode(&plane, &node, &[0; 36], &[])?;
        let mut world = ScriptWorld::default();
        let (pass, _) = world.spawn_entity(b"script_origin", [100.; 3], 0)?;
        let mut targets = Vec::new();
        for _ in 0..2 {
            let (slot, _) = world.spawn_entity(b"script_model", [0., 1., 3.], 0)?;
            let entity = world.entities.get_mut(slot).expect("spawned trace target");
            entity.mins = [-4., -4., -8.];
            entity.maxs = [4., 4., 8.];
            entity.contents = 0x200;
            relink(entity, &map, false)?;
            targets.push(slot);
        }
        let test = |entities: &GameEntities, skip, mask| {
            map.box_trace(
                entities,
                [-20., 1., 3.],
                [20., 1., 3.],
                [0.; 3],
                [0.; 3],
                skip,
                mask,
            )
        };
        let hit = test(&world.entities, 1023, 0x200);
        assert_eq!(
            (hit.entity, hit.contents, hit.material),
            (targets[0], 0x200, None)
        );
        assert!(hit.fraction > 0. && hit.fraction < 0.5);
        assert_eq!(test(&world.entities, targets[0], 0x200).entity, targets[1]);
        world
            .entities
            .get_mut(targets[0])
            .expect("spawned trace target")
            .owner_num = pass;
        assert_eq!(test(&world.entities, pass, 0x200).entity, targets[1]);
        assert_eq!(test(&world.entities, pass, 1), TraceResult::default());
        Ok(())
    }
}
