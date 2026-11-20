use super::*;

pub(super) fn fly(ps: &mut MovementState, cmd: &UserCmd, s: &MovementSettings, dt: f32) {
    ps.view_height_target = 60;
    let (mut fwd, side) = basis(ps.view_angles);
    let mut up = cross(side, fwd);
    let f = f32::from(cmd.forward_move);
    let r = f32::from(cmd.right_move);
    let z = 127.
        * (f32::from(u8::from(cmd.buttons & 0x80 != 0))
            - f32::from(u8::from(cmd.buttons & 0x40 != 0)));
    let input = [f, r, z];
    if ps.pm_type == 3 {
        up = [0., 0., 1.];
        fwd = cross(up, side);
    }
    let speed = length(ps.velocity);
    if speed < 1. || (ps.pm_type == 3 && input == [0.; 3]) {
        ps.velocity = [0.; 3];
    } else {
        let loss = speed.max(s.stop_speed) * s.friction * 1.5 * dt;
        ps.velocity = scale(ps.velocity, (speed - loss).max(0.) / speed);
    }
    let axis = length(input);
    if axis > 0. {
        let mut speed = ps.speed * if ps.pm_type == 2 { 3. } else { 6. };
        if ps.pm_flags & 0x100 != 0 || ps.lean != 0. {
            speed *= 0.4;
        }
        let wish = ma(ma(scale(fwd, f), r, side), z, up);
        let factor = speed * f.abs().max(r.abs()).max(z.abs()) / (127. * axis);
        accelerate(ps, scale(wish, factor), 9., dt, s);
    }
    ps.origin = ma(ps.origin, dt, ps.velocity);
}

pub(super) fn lean(ps: &mut MovementState, cmd: &UserCmd, ms: i32, trace: &Trace<'_>) {
    let mut direction = 0;
    if ps.pm_type < 6
        && ps.pm_flags & 0x8000 == 0
        && ps.entity_flags & 0x300 == 0
        && (ps.ground_entity != 1023 || ps.pm_type == 1)
    {
        direction = i32::from(cmd.buttons & 0x80 != 0) - i32::from(cmd.buttons & 0x40 != 0);
    }
    let limit = if ps.view_height_target == 11 {
        0.25
    } else {
        0.5
    };
    if direction == 0 {
        ps.lean = ps.lean.signum() * (ps.lean.abs() - ms as f32 / 280. * limit).max(0.);
    } else {
        ps.lean = (ps.lean + direction as f32 * ms as f32 / 350. * limit).clamp(-limit, limit);
    }
    if ps.lean != 0. {
        let sign = ps.lean.signum();
        let start = vertical(ps.origin, ps.view_height);
        let (_, side) = basis([0., ps.view_angles[1], sign * 16.]);
        let end = ma(start, sign * 20., side);
        let hit = trace(start, end, [-8.; 3], [8.; 3], 0x2810011);
        let limit = 1. - (1. - hit.fraction).sqrt();
        ps.lean = sign * ps.lean.abs().min(limit);
    }
}
