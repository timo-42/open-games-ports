use super::{MovementState, Trace, UserCmd, Vec3, clearance, delta, short, signed, slope_pitch};

fn nudge(ps: &mut MovementState, axis: usize, amount: f32) {
    ps.delta_angles[axis] = ps.delta_angles[axis].wrapping_add(short(amount));
}

pub(in crate::movement) fn view(
    ps: &mut MovementState,
    cmd: &UserCmd,
    trace: &Trace<'_>,
    old_yaw: f32,
    ms: i32,
    yaw_cap: f32,
) {
    if ps.pm_flags & 1 == 0
        || ps.pm_flags & 4 != 0
        || ps.entity_flags & 0x300 != 0
        || ps.pm_type >= 5
    {
        return;
    }
    let cmd_yaw = ps.view_angles[1];
    let mut adjusted = false;
    let turn = delta(ps.prone.direction, cmd_yaw);
    if turn.abs() > yaw_cap - 5. || ((cmd.forward_move != 0 || cmd.right_move != 0) && turn != 0.) {
        let rate = 0.055 * ms as f32;
        let mut candidate = if turn.abs() < rate {
            cmd_yaw
        } else {
            ps.prone.direction - turn.signum() * rate
        };
        let mut step = true;
        loop {
            if clearance::check_body(ps, trace, candidate) {
                if clearance::check_yaw(ps, trace, cmd_yaw)
                    && clearance::check_yaw(ps, trace, candidate)
                {
                    ps.prone.direction = candidate;
                } else {
                    adjusted = true;
                }
                break;
            }
            if !step {
                break;
            }
            let shift = delta(ps.prone.direction, candidate);
            step = shift.abs() > 1.;
            if !step {
                adjusted = true;
            }
            candidate = (candidate + shift.clamp(-1., 1.)).rem_euclid(360.);
        }
    }
    let mut turn = delta(ps.prone.direction, cmd_yaw);
    if turn != 0. {
        let mut candidate = ps.prone.direction;
        let mut step = true;
        loop {
            let clear = clearance::check_yaw(ps, trace, candidate);
            if clear && clearance::check_body(ps, trace, candidate) {
                ps.prone.direction = candidate;
                break;
            }
            if !step {
                break;
            }
            step = turn.abs() > 1.;
            adjusted = true;
            let shift = turn.clamp(-1., 1.);
            nudge(ps, 1, shift);
            ps.view_angles[1] = (ps.view_angles[1] + shift).rem_euclid(360.);
            turn = delta(ps.prone.direction, ps.view_angles[1]);
            if !clear {
                candidate = (candidate + turn).rem_euclid(360.);
            }
        }
    }
    if turn.abs() > yaw_cap {
        let sign = turn.signum();
        nudge(ps, 1, turn - sign * yaw_cap);
        ps.view_angles[1] = (ps.prone.direction - sign * yaw_cap).rem_euclid(360.);
    }
    if adjusted {
        ps.pm_flags |= 0x10000;
        let slack = delta(old_yaw, ps.view_angles[1]);
        if slack.abs() <= 1. && slack * delta(cmd_yaw, ps.view_angles[1]) > 0. {
            ps.view_angles[1] = (ps.view_angles[1] + slack * 0.98).rem_euclid(360.);
            nudge(ps, 1, slack * 0.98);
        }
    }
    let pitch = delta(ps.prone.view_torso_pitch, ps.view_angles[0]);
    if pitch.abs() > 45. {
        let bound = pitch.signum() * 45.;
        nudge(ps, 0, pitch - bound);
        ps.view_angles[0] = signed(ps.prone.view_torso_pitch - bound);
    }
}

pub(in crate::movement) fn smooth(
    ps: &mut MovementState,
    trace: &Trace<'_>,
    ground: Option<Vec3>,
    dt: f32,
) {
    if ps.pm_flags & 1 == 0 {
        return;
    }
    if ps.ground_entity == 1023 {
        if !clearance::check(ps, trace, ps.prone.direction, 66., true, false, ground) {
            ps.events.push((0x8d, 0));
            ps.pm_flags |= 0x10000;
        }
    } else if ground.is_some_and(|n| n[2] < 0.7) {
        ps.events.push((0x8d, 0));
    }
    for (pitch, yaw) in [
        (&mut ps.prone.direction_pitch, ps.prone.direction),
        (&mut ps.prone.view_torso_pitch, ps.view_angles[1]),
    ] {
        let target = ground.map_or(0., |n| slope_pitch(yaw, n));
        let turn = delta(target, *pitch);
        if turn != 0. {
            *pitch = signed(*pitch + turn.clamp(-dt * 70., dt * 70.));
        }
    }
}
