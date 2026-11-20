use super::*;

pub(super) fn leave(ps: &mut MovementState) {
    if ps.pm_flags & 0x20 != 0 {
        ps.pm_flags |= 0x40000;
        ps.pm_flags &= !0x20;
    }
}
fn facing_delta(ps: &MovementState) -> f32 {
    let facing = 180. + ps.ladder_normal[1].atan2(ps.ladder_normal[0]).to_degrees();
    signed_angle(facing - ps.view_angles[1])
}
pub(super) fn cap_yaw(ps: &mut MovementState, s: &MovementSettings) {
    if ps.pm_flags & 0x20 == 0 || ps.ground_entity != 1023 || s.ladder_yawcap == 0. {
        return;
    }
    let delta = facing_delta(ps);
    let adjust = delta - delta.clamp(-s.ladder_yawcap, s.ladder_yawcap);
    if adjust != 0. {
        ps.delta_angles[1] = ps.delta_angles[1].wrapping_add((adjust * 182.044_45) as i32 & 65535);
        ps.view_angles[1] = (ps.view_angles[1] + adjust).rem_euclid(360.);
    }
}
pub(super) fn check(ps: &mut MovementState, cmd: &UserCmd, h: &Hull<'_>, forward: Vec3) {
    let walking = ps.ground_entity != 1023;
    if walking {
        ps.pm_flags &= !0x40000;
    }
    if ps.pm_time != 0 && ps.pm_flags & 0x20 == 0 && ps.pm_flags & 0x600 != 0 {
        return;
    }
    let climbing = ps.pm_flags & 0x20 != 0 && !walking;
    if ps.pm_type >= 6 {
        ps.ground_entity = 1023;
        leave(ps);
        return;
    }
    if ps.pm_flags & 0x40000 != 0
        || ps.view_height_target == 11
        || cmd.server_time.saturating_sub(ps.jump_time) < 300
    {
        leave(ps);
        return;
    }
    let dir = if climbing {
        scale(ps.ladder_normal, -1.)
    } else {
        unit([forward[0], forward[1], 0.])
    };
    let dist = if walking { 8. } else { 30. };
    let mins = [h.mins[0] + 6., h.mins[1] + 6., 8.];
    let maxs = [h.maxs[0] - 6., h.maxs[1] - 6., h.maxs[2].max(8.)];
    let probe = |d| h.bounds_trace(ps.origin, ma(ps.origin, dist, d), mins, maxs);
    let hit = probe(dir);
    if hit.fraction < 1. && hit.surface_flags & 8 != 0 && (!walking || cmd.forward_move > 0) {
        if ps.pm_flags & 0x20 != 0 {
            return;
        }
        ps.ladder_normal = hit.normal;
        let second = probe(scale(hit.normal, -1.));
        if second.fraction < 1. && second.surface_flags & 8 != 0 {
            ps.pm_flags |= 0x20;
            return;
        }
    }
    leave(ps);
    if climbing {
        ps.animations.push(MovementAnim::Event(3, false, true));
    }
}
pub(super) fn jump_push(ps: &mut MovementState, forward: Vec3, s: &MovementSettings) {
    ps.velocity[2] *= 0.75;
    let flat = unit([forward[0], forward[1], 0.]);
    let n = ps.ladder_normal;
    let v = if dot(n, forward) < 0. {
        unit(ma(flat, -2. * dot(flat, n), n))
    } else {
        flat
    };
    ps.velocity[0] = v[0] * s.ladder_push;
    ps.velocity[1] = v[1] * s.ladder_push;
    ps.pm_flags &= !0x20;
}
pub(super) fn climb(
    ps: &mut MovementState,
    cmd: &UserCmd,
    h: &Hull<'_>,
    g: Option<Hit>,
    s: &MovementSettings,
    dt: f32,
) {
    let (fwd, right) = basis(ps.view_angles);
    let side = unit([right[0], right[1], 0.]);
    let n = ps.ladder_normal;
    let side = ma(side, -dot(side, n), n);
    let f = f32::from(cmd.forward_move);
    let r = f32::from(cmd.right_move);
    let speed = if ps.pm_flags & 0x100 != 0 || ps.lean != 0. {
        ps.speed * 0.4
    } else {
        ps.speed
    };
    let mut desired = [0.; 3];
    if f != 0. || r != 0. {
        let factor = speed * f.abs().max(r.abs()) / (127. * f.hypot(r));
        let climb = ((fwd[2] + 0.25) * 2.5).clamp(-1., 1.);
        desired = ma([0., 0., f * climb * 0.5 * factor], r * factor * 0.2, side);
    }
    let change = diff(desired, ps.velocity);
    let step = (9. * dt * length(desired)).min(length(change));
    ps.velocity = ma(ps.velocity, step, unit(change));
    if f == 0. {
        ps.velocity[2] = ps.velocity[2].signum() * (ps.velocity[2].abs() - ps.gravity * dt).max(0.);
    }
    if r == 0. {
        let dir = unit([side[0], side[1], 0.]);
        let rate = dot(dir, ps.velocity);
        if rate != 0. {
            let decel = rate * dt * 16.;
            let amount = if decel.abs() < rate.abs() {
                decel.signum() * decel.abs().max(1.)
            } else {
                rate
            };
            ps.velocity = ma(ps.velocity, -amount, dir);
        }
    }
    if ps.ground_entity == 1023 {
        let n = [n[0], n[1], 0.];
        ps.velocity = ma(ps.velocity, -dot(n, ps.velocity), n);
        if ps.velocity[0].powi(2) + ps.velocity[1].powi(2) <= ps.velocity[2].powi(2) {
            ps.velocity = ma(ps.velocity, -50., n);
        }
    }
    slide::step_slide(ps, h, dt, false, g, s);
    ps.movement_dir = facing_delta(ps).clamp(-75., 75.) as i8;
}
