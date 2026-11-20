use super::*;

/// PM_SlideMove, 0x080e9c42. Both the midpoint and final gravity velocities
/// must be clipped to every plane touched during the frame.
fn slide(
    ps: &mut MovementState,
    world: &Hull<'_>,
    dt: f32,
    gravity: bool,
    ground: Option<Hit>,
) -> bool {
    let mut end_velocity = ps.velocity;
    let mut primal = ps.velocity;
    if gravity {
        end_velocity[2] -= ps.gravity * dt;
        ps.velocity[2] = (end_velocity[2] + ps.velocity[2]) * 0.5;
        primal[2] = end_velocity[2];
        if let Some(g) = ground {
            ps.velocity = clip(ps.velocity, g.normal);
        }
    }
    let mut planes = Vec::with_capacity(7);
    if let Some(g) = ground {
        planes.push(g.normal);
    }
    planes.push(unit(ps.velocity));
    let mut remaining = dt;
    let mut hit = false;
    for _ in 0..4 {
        let end = ma(ps.origin, remaining, ps.velocity);
        let trace = world.sweep(ps.origin, end);
        if trace.all_solid {
            ps.velocity[2] = 0.;
            return true;
        }
        if trace.fraction > 0. {
            ps.origin = lerp(ps.origin, end, trace.fraction);
        }
        if trace.fraction == 1. {
            break;
        }
        world.contacts.record(trace.entity);
        hit = true;
        remaining =
            (f64::from(remaining) - f64::from(remaining) * f64::from(trace.fraction)) as f32;
        if planes.iter().any(|p| dot(*p, trace.normal) > 0.999) {
            ps.velocity = ma(clip(ps.velocity, trace.normal), 1., trace.normal);
            continue;
        }
        planes.push(trace.normal);
        // Insertion sort in the original inserts an equal dot before its predecessor.
        let mut ordered: Vec<_> = planes.iter().rev().copied().collect();
        ordered.sort_by(|a, b| dot(ps.velocity, *a).total_cmp(&dot(ps.velocity, *b)));
        let first = ordered[0];
        if dot(first, ps.velocity) >= 0.1 {
            continue;
        }
        let mut clipped = clip(ps.velocity, first);
        let mut clipped_end = clip(end_velocity, first);
        for (index, plane) in ordered.iter().copied().enumerate().skip(1) {
            if dot(clipped, plane) >= 0.1 {
                continue;
            }
            clipped = clip(clipped, plane);
            clipped_end = clip(clipped_end, plane);
            if dot(clipped, first) < 0. {
                let edge = unit(cross(first, plane));
                clipped = scale(edge, dot(edge, ps.velocity));
                clipped_end = scale(edge, dot(edge, end_velocity));
                if ordered
                    .iter()
                    .enumerate()
                    .skip(1)
                    .any(|(k, p)| k != index && dot(clipped, *p) < 0.1)
                {
                    ps.velocity = [0.; 3];
                    return true;
                }
            }
        }
        ps.velocity = clipped;
        end_velocity = clipped_end;
    }
    if gravity {
        ps.velocity = end_velocity;
    }
    if ps.pm_time != 0 {
        ps.velocity = primal;
    }
    hit
}

pub(super) fn step_slide(
    ps: &mut MovementState,
    world: &Hull<'_>,
    dt: f32,
    gravity: bool,
    ground: Option<Hit>,
    settings: &MovementSettings,
) {
    let start = ps.origin;
    let velocity = ps.velocity;
    let ladder = ps.pm_flags & 0x20 != 0;
    let can_step_down = !ladder && ground.is_some();
    if ladder || (!can_step_down && ps.pm_flags & 0x80000 != 0 && ps.pm_time != 0) {
        ps.pm_flags &= !0x80000;
        ps.jump_origin_z = 0.;
    }
    let blocked = slide(ps, world, dt, gravity, ground);
    let down_origin = ps.origin;
    let down_velocity = ps.velocity;
    let mut step_up = 0.;
    let mut step_height = if ps.pm_flags & 1 != 0 { 10. } else { 18. };
    let mut launched = false;
    if ps.ground_entity == 1023 {
        let room = ps.jump_origin_z + settings.jump_height - start[2];
        if blocked && ps.pm_flags & 0x80000 != 0 && room > 0. {
            step_height = settings.jump_step.min(room);
            if step_height < 1. {
                return;
            }
            launched = true;
        }
        if !launched && (!ladder || ps.velocity[2] <= 0.) {
            return;
        }
    }
    if blocked {
        let up = vertical(start, step_height + 1.);
        let ceiling = world.sweep(start, up);
        let available = (step_height + 1.) * ceiling.fraction - 1.;
        if available >= 1. {
            step_up = available;
            ps.origin = vertical(start, step_up);
            ps.velocity = velocity;
            slide(ps, world, dt, gravity, ground);
        }
    }
    let down = if can_step_down { 9. } else { 0. };
    if down != 0. || step_up != 0. {
        let down_end = vertical(ps.origin, -step_up - down);
        let floor = world.sweep(ps.origin, down_end);
        if floor.entity < 64 || (floor.fraction < 1. && floor.normal[2] < 0.3) {
            ps.origin = down_origin;
            ps.velocity = down_velocity;
            return;
        }
        if floor.fraction < 1. {
            ps.origin = lerp(ps.origin, down_end, floor.fraction);
            ps.velocity = clip(ps.velocity, floor.normal);
        } else {
            ps.origin[2] -= step_up;
        }
    }
    let along =
        |pos: Vec3| (pos[0] - start[0]) * ps.velocity[0] + (pos[1] - start[1]) * ps.velocity[1];
    if along(ps.origin) <= 0.001 + along(down_origin)
        || (launched && ps.origin[2] >= ps.jump_origin_z + settings.jump_height)
    {
        ps.origin = down_origin;
        ps.velocity = down_velocity;
        if can_step_down {
            let end = vertical(ps.origin, -9.);
            let floor = world.sweep(ps.origin, end);
            if floor.fraction < 1. {
                ps.origin = lerp(ps.origin, end, floor.fraction);
                ps.velocity = clip(ps.velocity, floor.normal);
            }
        }
    }
    if launched && ps.origin[2] > down_origin[2] {
        let remaining = ps.jump_origin_z + settings.jump_height - ps.origin[2];
        if remaining < 0.1 {
            ps.velocity[2] = 0.;
        } else {
            ps.velocity[2] = ps.velocity[2].min((2. * ps.gravity * remaining).sqrt());
        }
    }
    if can_step_down
        && ps.pm_type < 6
        && prone::step_probe_or_restore(ps, world.trace, start, velocity)
        && (ps.origin[2] - down_origin[2]).abs() > 0.5
    {
        let delta = (ps.origin[2] - down_origin[2] + 0.5)
            .floor()
            .clamp(-16., 24.) as i32;
        if delta == 0 {
            return;
        }
        ps.events.push((0x8f, (delta + 128) as u8));
        if delta.abs() > 3
            && ps.ground_entity != 1023
            && ps.view_height_target == 60
            && ps.pm_flags & 0x100 == 0
            && world.contacts.xy_speed.get() >= settings.footsteps_threshold
        {
            let old = ps.bob_cycle;
            ps.bob_cycle =
                (f32::from(old) + (delta.abs() / 2).min(4) as f32 * 1.25 + 7.) as i32 as u8;
            let surface = ground.map_or(0, feedback::surface);
            if surface != 0 && (old.wrapping_add(64) ^ ps.bob_cycle.wrapping_add(64)) & 128 != 0 {
                ps.events
                    .push((surface + if ps.lean != 0. { 0x18 } else { 1 }, 0));
            }
        }
        ps.velocity = scale(
            ps.velocity,
            (1. - (ps.origin[2] - start[2]).abs() / step_height) * 0.8 + 0.199_999_99,
        );
    }
}
