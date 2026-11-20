//! CM_TempBoxModel (0x080592e2) uses a vertical capsule for its swept trace.
use super::{Sweep, TraceResult, Vec3, dot, sub};
/// Both endpoints and the target bounds are in target-local coordinates.
pub fn trace_capsule(
    start: Vec3,
    end: Vec3,
    mins: Vec3,
    maxs: Vec3,
    target_mins: Vec3,
    target_maxs: Vec3,
    contents: u32,
) -> TraceResult {
    let mut hit = TraceResult::default();
    let moving = Sweep::new(start, end, mins, maxs, contents);
    let center: Vec3 = std::array::from_fn(|i| (target_mins[i] + target_maxs[i]) * 0.5);
    let target_half = sub(target_maxs, center);
    let target_radius = target_half[0].min(target_half[2]);
    let height = target_half[2] - target_radius;
    let radius = target_radius + moving.radius;
    let up = |mut v: Vec3, d: f32| {
        v[2] += d;
        v
    };
    let target_top = up(center, height);
    let target_bottom = up(center, -height);
    let start_top = up(moving.start, moving.offset_z);
    let start_bottom = up(moving.start, -moving.offset_z);
    if moving.stationary {
        let r2 = f64::from(radius * radius);
        let overlap = [target_top, target_bottom].into_iter().any(|p| {
            [start_top, start_bottom].into_iter().any(|q| {
                let d = sub(q, p);
                dot(d, d) < r2
            })
        });
        let mut d = sub(moving.start, center);
        let inside_height = d[2].abs() <= moving.offset_z + height;
        d[2] = 0.;
        if overlap || (inside_height && dot(d, d) < r2) {
            hit.start_solid = true;
            hit.all_solid = true;
            hit.fraction = 0.;
        }
        return hit;
    }
    if (0..3).any(|i| {
        moving.start[i].min(moving.end[i]) - moving.extents[i] > target_maxs[i] + 1.
            || moving.start[i].max(moving.end[i]) + moving.extents[i] < target_mins[i] - 1.
    }) {
        return hit;
    }
    let sphere = |cap, sign, hit: &mut TraceResult| -> bool {
        let start = sub(up(moving.start, sign), cap);
        let end = sub(up(moving.end, sign), cap);
        sweep_round(start, end, moving.delta, radius, None, contents, hit)
    };
    if start_bottom[2] > target_top[2] {
        if sphere(target_top, -moving.offset_z, &mut hit) || moving.delta[2] >= 0. {
            return hit;
        }
    } else if start_top[2] < target_bottom[2]
        && (sphere(target_bottom, moving.offset_z, &mut hit) || moving.delta[2] <= 0.)
    {
        return hit;
    }
    if sweep_round(
        sub(moving.start, center),
        sub(moving.end, center),
        moving.delta,
        radius,
        Some(height + moving.offset_z),
        contents,
        &mut hit,
    ) {
        return hit;
    }
    if moving.end[2] - moving.offset_z > target_top[2] {
        if start_bottom[2] <= target_top[2] {
            sphere(target_top, -moving.offset_z, &mut hit);
        }
    } else if moving.end[2] + moving.offset_z < target_bottom[2] && start_top[2] >= target_bottom[2]
    {
        sphere(target_bottom, moving.offset_z, &mut hit);
    }
    hit
}
// With a height bound the quadratic uses XY and additionally tests the Z
// coordinate at contact. The original computes the normal at the segment start.
fn sweep_round(
    mut start: Vec3,
    end: Vec3,
    mut delta: Vec3,
    radius: f32,
    height: Option<f32>,
    contents: u32,
    hit: &mut TraceResult,
) -> bool {
    let start_z = start[2];
    let delta_z = delta[2];
    if height.is_some() {
        start[2] = 0.;
        delta[2] = 0.;
    }
    let len_sq = dot(start, start);
    let r2 = f64::from(radius * radius);
    let c = (len_sq - r2) as f32;
    let len = (len_sq as f32).sqrt();
    let normal = if len == 0. {
        [0.; 3]
    } else {
        start.map(|n| n * (1. / len))
    };
    if c <= 0. {
        if height.is_some_and(|h| start_z.abs() > h) {
            return false;
        }
        hit.start_solid = true;
        hit.fraction = 0.;
        hit.contents = contents;
        hit.normal = normal;
        hit.all_solid |= height.map_or_else(|| dot(end, end) <= r2, |h| end[2].abs() <= h);
        return true;
    }
    let b = dot(delta, start) as f32;
    if b >= 0. {
        return false;
    }
    let a = dot(delta, delta) as f32;
    let disc = (f64::from(b).powi(2) - f64::from(a) * f64::from(c)) as f32;
    if disc < 0. {
        return false;
    }
    let bias = 0.125 * f64::from(len) / f64::from(b);
    let t0 = (-f64::from(b) - f64::from(disc.sqrt())) / f64::from(a);
    let time = (t0
        + if height.is_some() {
            f64::from(bias as f32)
        } else {
            bias
        }) as f32;
    if time >= hit.fraction {
        return false;
    }
    if let Some(h) = height {
        let z = ((f64::from(time) - f64::from(bias as f32)) * f64::from(delta_z)
            + f64::from(start_z)) as f32;
        if z.abs() > h {
            return false;
        }
    }
    hit.fraction = time.max(0.);
    hit.normal = normal;
    hit.contents = contents;
    true
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn sides_caps_and_position_tests_use_original_solid_fields() {
        let target_mins = [-5., -5., -10.];
        let target_maxs = [5., 5., 10.];
        let ray = |a, b| trace_capsule(a, b, [0.; 3], [0.; 3], target_mins, target_maxs, 17);
        let side = ray([20., 0., 0.], [0.; 3]);
        assert_eq!(side.normal, [1., 0., 0.]);
        assert_eq!(side.contents, 17);
        assert_eq!(side.fraction, 0.74375);
        let top = ray([0., 0., 25.], [0., 0., 5.]);
        assert_eq!(top.normal, [0., 0., 1.]);
        assert_eq!(top.fraction, side.fraction);
        // Moving from inside the cylinder sets allsolid when the end is in
        // its vertical interval, even if the movement exits horizontally.
        let escape = ray([2., 0., 0.], [20., 0., 0.]);
        assert!(escape.start_solid && escape.all_solid);
        let position = ray([2., 0., 0.], [2., 0., 0.]);
        assert!(position.start_solid && position.all_solid);
        assert_eq!(position.contents, 0);
    }
}
