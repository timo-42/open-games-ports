//! DObj_Traceline (0x080bb27e), including inherited classifications and priority overrides.
use super::{BoneTransform, ModelHit, ModelView, Vec3, dot, invalid, local, sub};
use std::{collections::BTreeMap, io};

const PISTOL: [u8; 19] = [1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0];
const RIFLE: [u8; 19] = [1, 9, 9, 9, 8, 7, 6, 6, 6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 0];

enum BoxHit {
    Enter(f32, usize),
    Inside,
}

fn clip_box(
    start: Vec3,
    end: Vec3,
    bounds: [Vec3; 2],
    mut limit: f32,
    sign_out: &mut f32,
) -> Option<BoxHit> {
    let mut near = 0.;
    let mut axis = None;
    let mut exits = false;
    for (face, sign) in bounds.into_iter().zip([-1., 1.]) {
        for i in 0..3 {
            let from = (start[i] - face[i]) * sign;
            let to = (end[i] - face[i]) * sign;
            let span = f64::from(from - to);
            if from > 0. {
                if to > 0. {
                    return None;
                }
                if f64::from(near) * span < f64::from(from) {
                    let fraction = f64::from(from) / span;
                    if f64::from(limit) <= fraction {
                        return None;
                    }
                    near = fraction as f32;
                    *sign_out = sign;
                    axis = Some(i);
                }
            } else if to > 0. {
                exits = true;
                if f64::from(limit) * span < f64::from(from) {
                    let fraction = f64::from(from) / span;
                    if fraction <= f64::from(near) {
                        return None;
                    }
                    limit = fraction as f32;
                }
            }
        }
    }
    match axis {
        Some(axis) => Some(BoxHit::Enter(near, axis)),
        None if !exits => Some(BoxHit::Inside),
        None => None,
    }
}

pub(super) fn trace<'a>(
    models: &[ModelView<'a>],
    start: Vec3,
    end: Vec3,
    rifle: bool,
    initial_fraction: f32,
) -> io::Result<Option<ModelHit<'a>>> {
    let priorities = if rifle { RIFLE } else { PISTOL };
    let dir = sub(end, start);
    let length_sq = dot(dir, dir) as f32;
    if length_sq == 0. {
        return Ok(None);
    }
    let inv_length = length_sq.recip();
    let mut groups = Vec::<u8>::new();
    let mut names = BTreeMap::<&str, usize>::new();
    let mut threshold = 2;
    let mut best_fraction = initial_fraction;
    let mut best = None;
    let mut normal_axis = None;
    // The original carries the last accepted clipping-plane sign across bones.
    let mut last_sign = 0.;
    for (index, view) in models.iter().enumerate() {
        let first = groups.len();
        if first + view.model.parts.bones.len() > 127 {
            return Err(invalid("DObj exceeds 127 bones"));
        }
        for (b, (bone, bounds)) in view
            .model
            .parts
            .bones
            .iter()
            .zip(&view.model.bone_bounds)
            .enumerate()
        {
            let alias = if index != 0 && view.parent_bone.is_none() {
                names.get(bone.name.as_str()).copied()
            } else {
                None
            };
            let mut group = bone.classification;
            let priority = |group: u8| {
                priorities
                    .get(usize::from(group))
                    .copied()
                    .ok_or_else(|| invalid("invalid XModel bone classification"))
            };
            if priority(group)? == 1 {
                let parent = alias
                    .or_else(|| bone.parent.map(|p| first + usize::from(p)))
                    .or(view.parent_bone.map(usize::from));
                group = match parent {
                    Some(p) => *groups
                        .get(p)
                        .ok_or_else(|| invalid("invalid DObj bone parent"))?,
                    None => 0,
                };
            }
            names.entry(&bone.name).or_insert(groups.len());
            groups.push(group);
            let priority = priority(group)?;
            if view.ignore_collision || bounds.radius_squared == 0. || priority < threshold {
                continue;
            }
            let transform = view.pose.map_or(bone.bind_pose, |p| p[b]);
            let center = transform.transform_point(bounds.center);
            let to_start = sub(start, center);
            let closest_t = (-dot(to_start, dir) * f64::from(inv_length)) as f32;
            let closest = if closest_t >= 1. {
                sub(end, center)
            } else if closest_t <= 0. {
                to_start
            } else {
                std::array::from_fn(|i| {
                    (f64::from(to_start[i]) + f64::from(closest_t) * f64::from(dir[i])) as f32
                })
            };
            let overlap = bounds.radius_squared - dot(closest, closest) as f32;
            if overlap <= 0.
                || (priority == threshold
                    && closest_t - (overlap * inv_length).sqrt() >= best_fraction)
            {
                continue;
            }
            let axes = transform.axes();
            let from = local(start, transform.origin, axes);
            let to = local(end, transform.origin, axes);
            let limit = if threshold == priority {
                best_fraction
            } else {
                initial_fraction
            };
            let hit = ModelHit {
                fraction: 0.,
                normal: [0.; 3],
                surface_flags: 0,
                contents: 0,
                part_name: &bone.name,
                part_group: group,
            };
            match clip_box(from, to, bounds.bounds, limit, &mut last_sign) {
                Some(BoxHit::Enter(fraction, axis))
                    if threshold != priority || fraction < best_fraction =>
                {
                    best_fraction = fraction;
                    threshold = priority;
                    best = Some(ModelHit { fraction, ..hit });
                    normal_axis = Some((transform, axis));
                }
                Some(BoxHit::Inside)
                    if f64::from(dir[0]) * f64::from(start[0])
                        + f64::from(dir[1]) * f64::from(start[1])
                        <= 0. =>
                {
                    let normal = if dir[0] == 0. && dir[1] == 0. {
                        [0., 0., if dir[2] < 0. { 1. } else { -1. }]
                    } else {
                        let length = (f64::from(start[0]).powi(2) + f64::from(start[1]).powi(2))
                            .sqrt() as f32;
                        if length == 0. {
                            [0.; 3]
                        } else {
                            [start[0] / length, start[1] / length, 0.]
                        }
                    };
                    return Ok(Some(ModelHit { normal, ..hit }));
                }
                _ => {}
            }
        }
    }
    if let (Some(hit), Some((transform, axis))) = (&mut best, normal_axis) {
        let transform: BoneTransform = transform;
        hit.normal = transform.axes()[axis].map(|v| v * last_sign);
    }
    Ok(best)
}
