//! Original 1.3 body clearance and torso support traces at 0x080e01f4.
use super::{MovementState, Trace, Vec3, delta, elevation};
use crate::movement::{diff, length, lerp, ma, unit, vertical};

#[derive(Clone, Copy)]
pub(super) struct Support {
    pub(super) torso: f32,
    pub(super) waist: f32,
}

fn supported(
    ps: &MovementState,
    trace: &Trace<'_>,
    yaw: f32,
    distance: f32,
    skip_hull: bool,
    grounded: bool,
    normal: Option<Vec3>,
) -> (bool, Option<Support>) {
    let width = ps.maxs[0];
    let origin = ps.origin;
    let mask = 0x810011;
    if !skip_hull
        && trace(
            origin,
            vertical(origin, 10.),
            [-width, -width, 0.],
            [width, width, 30.],
            mask,
        )
        .all_solid
    {
        return (false, None);
    }
    if grounded && normal.is_some_and(|n| n[2] < 0.7) {
        return (false, None);
    }
    let sphere = |a, b| trace(a, b, [-6.; 3], [6.; 3], mask);
    let ray = |a, b| trace(a, b, [0.; 3], [0.; 3], mask);
    let (sin, cos) = (f64::from(yaw - 180.).to_radians()).sin_cos();
    let mut backward = [cos as f32, sin as f32, 0.];
    let mut start = vertical(origin, 24.);
    let mut end = ma(start, distance - 6., backward);
    let mut hit = sphere(start, end);
    let mut edge = hit.fraction < 1.;
    let mut reach = distance;
    if edge {
        if !grounded {
            return (false, None);
        }
        reach = (distance - 6.) * hit.fraction + 6.;
        if reach < width + 2. {
            return (false, None);
        }
        if f64::from(reach) < 24. * 0.7 + 48. {
            end[2] += 22.;
            let slope = diff(end, start);
            backward = unit(slope);
            hit = sphere(start, end);
            edge = hit.fraction < 1.;
            reach = if edge {
                hit.fraction * length(slope) + 6.
            } else {
                distance
            };
            if edge && f64::from(reach) < 24. * 0.7 + 48. {
                return (false, None);
            }
        }
    }
    let mut feet = lerp(start, end, hit.fraction);
    start = vertical(ma(origin, 48., backward), 24.);
    let drop = width * 2.5 + 18.;
    end = vertical(start, -drop);
    hit = sphere(start, end);
    let unsupported = || {
        if grounded {
            (false, None)
        } else {
            (
                true,
                Some(Support {
                    torso: 0.,
                    waist: 0.,
                }),
            )
        }
    };
    if hit.fraction == 1. {
        return unsupported();
    }
    if hit.normal[2] < 0.7 {
        return (false, None);
    }
    let support_depth = drop * hit.fraction + 6.;
    let waist = vertical(lerp(start, end, hit.fraction), -6.);
    if edge {
        if reach - support_depth < -0.75 * support_depth {
            return unsupported();
        }
        let axis = unit(vertical(ma(diff(feet, waist), 6., backward), 6.));
        end = ma(start, distance - 54., axis);
        for i in 0..2 {
            end[i] = ((distance - 6.) * backward[i] + origin[i] + end[i]) * 0.5;
        }
        hit = sphere(start, end);
        if hit.fraction < 1. {
            start = vertical(lerp(start, end, hit.fraction), 18.);
            end = vertical(end, 18.);
            hit = sphere(start, end);
            if hit.fraction < 1. {
                return unsupported();
            }
        }
        feet = lerp(start, end, hit.fraction);
    }
    end = vertical(feet, -((feet[2] - waist[2]) * 2. + width));
    hit = sphere(feet, end);
    if hit.fraction == 1. {
        return unsupported();
    }
    if hit.normal[2] < 0.7 {
        return (false, None);
    }
    feet = vertical(lerp(feet, end, hit.fraction), -6.);
    let angle = (elevation(diff(feet, waist)) - elevation(diff(waist, origin)) + 180.)
        .rem_euclid(360.)
        - 180.;
    let clear = ray(vertical(origin, 5.), vertical(waist, 5.)).fraction == 1.;
    let legs_clear = ray(vertical(waist, 5.), vertical(feet, 5.)).fraction == 1.;
    let result = (-50. ..=70.).contains(&angle) && clear && legs_clear;
    if !result && !grounded {
        return unsupported();
    }
    (
        result,
        Some(Support {
            torso: delta(elevation(diff(origin, waist)), 0.),
            waist: delta(elevation(diff(waist, feet)), 0.),
        }),
    )
}

pub(super) fn check(
    ps: &mut MovementState,
    trace: &Trace<'_>,
    yaw: f32,
    distance: f32,
    skip_hull: bool,
    grounded: bool,
    normal: Option<Vec3>,
) -> bool {
    let (valid, support) = supported(ps, trace, yaw, distance, skip_hull, grounded, normal);
    if let Some(support) = support {
        ps.prone.torso_height = 0.;
        ps.prone.torso_pitch = support.torso;
        ps.prone.waist_pitch = support.waist;
    }
    valid
}

pub(super) fn check_yaw(ps: &MovementState, trace: &Trace<'_>, yaw: f32) -> bool {
    supported(ps, trace, yaw, 45., true, ps.ground_entity != 1023, None).0
}

pub(super) fn check_body(ps: &mut MovementState, trace: &Trace<'_>, yaw: f32) -> bool {
    let turn = delta(yaw, ps.view_angles[1]);
    let weight = turn.abs() / 240.;
    check(
        ps,
        trace,
        (yaw - (1. - weight) * turn).rem_euclid(360.),
        (1. - weight) * 66. + weight * 45.,
        true,
        ps.ground_entity != 1023,
        None,
    )
}
