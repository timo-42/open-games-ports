//! XModel_TraceLine (0x080c59da). The triangle stores face and barycentric planes.
use super::{
    BoneTransform, ModelHit, ServerModel, Vec3, box_intersects, dot, invalid, local, rotate_back,
    sub,
};
use std::io;

pub fn trace_model<'a>(
    model: &'a ServerModel,
    pose: Option<&[BoneTransform]>,
    start: Vec3,
    end: Vec3,
    mask: u32,
    mut limit: f32,
) -> io::Result<Option<ModelHit<'a>>> {
    if pose.is_some_and(|matrices| matrices.len() != model.parts.bones.len()) {
        return Err(invalid("incomplete XModel skeleton"));
    }
    let mut best = None;
    for surface in &model.collision {
        if mask & surface.contents == 0 {
            continue;
        }
        let index =
            usize::try_from(surface.bone_index).map_err(|_| invalid("invalid collision bone"))?;
        let bone = model
            .parts
            .bones
            .get(index)
            .ok_or_else(|| invalid("invalid collision bone"))?;
        let transform = pose.map_or(bone.bind_pose, |matrices| matrices[index]);
        let axes = transform.axes();
        let from = local(start, transform.origin, axes);
        let to = local(end, transform.origin, axes);
        if !box_intersects(from, to, surface.bounds, limit) {
            continue;
        }
        let direction = sub(to, from);
        for tri in &surface.triangles {
            let plane = |p, i: usize| {
                (dot(p, [tri[i], tri[i + 1], tri[i + 2]]) - f64::from(tri[i + 3])) as f32
            };
            let end_dist = plane(to, 0);
            let start_dist = plane(from, 0);
            if end_dist >= 0. || start_dist <= 0. {
                continue;
            }
            let interval = f64::from(start_dist) - f64::from(end_dist);
            let fraction = (((f64::from(start_dist) - 0.125) / interval) as f32).max(0.);
            if fraction >= limit {
                continue;
            }
            let t = (f64::from(start_dist) / interval) as f32;
            let point = std::array::from_fn(|i| {
                (f64::from(from[i]) + f64::from(t) * f64::from(direction[i])) as f32
            });
            let u = plane(point, 4);
            let v = plane(point, 8);
            if (-0.001..=1.001).contains(&u)
                && v >= -0.001
                && f64::from(u) + f64::from(v) <= f64::from(1.001f32)
            {
                limit = fraction;
                best = Some(ModelHit {
                    fraction,
                    normal: rotate_back([tri[0], tri[1], tri[2]], axes),
                    contents: surface.contents,
                    surface_flags: surface.flags,
                    part_name: &bone.name,
                    part_group: 0,
                });
            }
        }
    }
    Ok(best)
}
