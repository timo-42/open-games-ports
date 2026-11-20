use super::*;

pub(super) fn surface(hit: Hit) -> u8 {
    if hit.surface_flags & 0x2000 == 0 {
        ((hit.surface_flags >> 20) & 31) as u8
    } else {
        0
    }
}
pub(super) fn detect_landing(
    ps: &mut MovementState,
    hull: &Hull<'_>,
    start: Vec3,
    vz: f32,
    s: &MovementSettings,
    cmd: &UserCmd,
) -> Option<Hit> {
    let airborne = ps.ground_entity == 1023;
    let hit = ground_trace(ps, hull, cmd)?;
    if airborne && ps.ground_entity != 1023 && ps.gravity > 0. {
        // PM_CrashLand solves v² = v0² + 2g(h0-h1) to recover drop height.
        let height = vz * vz / (2. * ps.gravity) + (start[2] - ps.origin[2]);
        let material = surface(hit);
        if height > 12. {
            ps.animations.push(MovementAnim::Event(5, false, true));
        }
        let damage = if s.fall_max > s.fall_min
            && height > s.fall_min
            && hit.surface_flags & 1 == 0
            && ps.pm_type < 6
        {
            (100. * (height - s.fall_min) / (s.fall_max - s.fall_min)).clamp(0., 100.) as u8
        } else {
            0
        };
        if damage != 0 {
            let damping = if damage < 100 && hit.surface_flags & 2 == 0 {
                ps.pm_time = (i32::from(damage) * 35 + 500).min(2000);
                ps.pm_flags |= 0x200;
                if ps.pm_time < 1500 {
                    0.5 - (ps.pm_time - 500) as f32 * 0.0003
                } else {
                    0.2
                }
            } else {
                0.67
            };
            ps.velocity = scale(ps.velocity, damping);
            ps.events.push((material + 0x74, damage));
        } else if height >= 12. {
            let band = if height == 12. {
                0
            } else {
                (4. + (height - 12.) * (4. / 26.)).min(24.) as u8
            };
            ps.velocity = scale(ps.velocity, 0.67);
            ps.events.push((material + 0x5d, band));
        } else if height > 4. && material != 0 {
            ps.events
                .push((material + if height < 8. { 0x18 } else { 1 }, 0));
        }
    }
    Some(hit)
}

pub(super) fn footsteps(
    ps: &mut MovementState,
    cmd: &UserCmd,
    g: Hit,
    s: &MovementSettings,
    ms: i32,
) {
    if ps.pm_type >= 6 || ps.entity_flags & 0x300 != 0 {
        return;
    }
    let speed = length([ps.velocity[0], ps.velocity[1], 0.]);
    if speed < s.move_threshold || ps.pm_type == 1 {
        if speed < 1. {
            ps.bob_cycle = 0;
        }
        return;
    }
    let mut reference = ps.speed;
    let back = cmd.forward_move < 0;
    if cmd.right_move != 0 {
        let strafe = (f64::from(s.strafe_scale) - 1.) * 0.75 + 1.;
        let scale = if cmd.forward_move == 0 {
            strafe
        } else {
            (strafe + 1.) * 0.5
        };
        reference = (f64::from(reference) * scale) as f32;
    }
    if back {
        let scale = if cmd.right_move == 0 {
            f64::from(s.back_scale)
        } else {
            (f64::from(s.back_scale) + 1.) * 0.5
        };
        reference = (f64::from(reference) * scale) as f32;
    }
    let lean = ps.pm_flags & 0x100 != 0 || ps.lean != 0.;
    if lean {
        reference *= 0.4;
    }
    reference *= prone::walk_scale(ps, cmd.server_time);
    let step: f32 = if ps.view_height_target == 11 {
        if lean { 0.24 } else { 0.25 }
    } else if ps.view_height_target == 40 {
        if lean { 0.315 } else { 0.34 }
    } else if ps.pm_flags & 0x80 != 0 {
        // Backward gait persists while releasing both movement axes.
        if lean { 0.325 } else { 0.36 }
    } else if lean {
        0.305
    } else {
        0.335
    };
    if reference <= 0. {
        return;
    }
    let old = ps.bob_cycle;
    let bob_scale = (f64::from(speed) / f64::from(reference) * f64::from(step)) as f32;
    ps.bob_cycle = (f64::from(old) + f64::from(ms) * f64::from(bob_scale)) as i32 as u8;
    let crossing = (old.wrapping_add(64) ^ ps.bob_cycle.wrapping_add(64)) & 128 != 0;
    if crossing
        && (cmd.forward_move != 0 || cmd.right_move != 0)
        && ps.view_height_target == 60
        && ps.pm_flags & 0x100 == 0
        && speed >= s.footsteps_threshold
    {
        let material = surface(g);
        if material != 0 {
            ps.events.push((material + if lean { 0x18 } else { 1 }, 0));
        }
    }
}

pub(super) fn ladder_step(ps: &mut MovementState, h: &Hull<'_>, ms: i32) {
    if ps.command_time.saturating_sub(ps.jump_time) < 300 {
        return;
    }
    let lean = ps.pm_flags & 0x100 != 0 || ps.lean != 0.;
    let bob = if lean {
        ps.velocity[2] / 38.100_002 * 0.35
    } else {
        ps.velocity[2] / 95.25 * 0.45
    };
    let old = ps.bob_cycle;
    ps.bob_cycle = (f32::from(old) + ms as f32 * bob) as i32 as u8;
    if (old.wrapping_add(64) ^ ps.bob_cycle.wrapping_add(64)) & 128 == 0 {
        return;
    }
    let mins = [h.mins[0] + 6., h.mins[1] + 6., 8.];
    let maxs = [h.maxs[0] - 6., h.maxs[1] - 6., h.maxs[2].max(8.)];
    let end = ma(ps.origin, -31., ps.ladder_normal);
    let trace = (h.trace)(ps.origin, end, mins, maxs, h.mask & 0xfdfeffff);
    let material = ((trace.surface_flags >> 20) & 31) as u8;
    let sound = if trace.fraction == 1. || material == 0 {
        21
    } else {
        material
    };
    ps.events.push((sound + 1, 0));
}
pub(super) fn foliage(ps: &mut MovementState, h: &Hull<'_>, s: &MovementSettings) {
    let speed = if ps.pm_type >= 6 {
        0.
    } else {
        ps.velocity[0].hypot(ps.velocity[1])
    };
    if speed < s.foliage_min {
        if ps.foliage_sound_time.saturating_add(s.foliage_reset) < ps.command_time {
            ps.foliage_sound_time = 0;
        }
    } else {
        let frac = ((speed - s.foliage_min) / (s.foliage_max - s.foliage_min)).min(1.);
        let delay =
            (s.foliage_slow as f32 + (s.foliage_fast - s.foliage_slow) as f32 * frac) as i32;
        if ps.foliage_sound_time.saturating_add(delay) < ps.command_time {
            let mut top = scale(h.maxs, 0.75);
            top[2] = h.maxs[2] * 0.9;
            let trace = (h.trace)(ps.origin, ps.origin, scale(h.mins, 0.75), top, 2);
            if trace.start_solid {
                ps.foliage_sound_time = ps.command_time;
                ps.events.push((0x8b, 0));
            }
        }
    }
}
