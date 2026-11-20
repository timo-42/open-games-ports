//! PM_CheckDuck, PM_UpdateStanceHeight and the prone branches of PM_UpdateViewAngles.
mod clearance;
mod facing;
use super::{
    Hull, MovementAnim, MovementSettings, MovementState, Trace, UserCmd, Vec3, dot, lerp, ma,
    vertical,
};
use crate::snapshot::PlayerState;
pub(super) use facing::{smooth, view};

#[derive(Clone, Debug, Default, PartialEq)]
pub struct ProneState {
    pub direction: f32,
    pub direction_pitch: f32,
    pub view_torso_pitch: f32,
    pub torso_height: f32,
    pub torso_pitch: f32,
    pub waist_pitch: f32,
    height_position_adjustment: f32,
}

impl ProneState {
    pub(super) fn from_player(ps: &PlayerState) -> Self {
        let field = |index| f32::from_bits(ps.words()[index / 4]);
        Self {
            direction: field(0x584usize),
            direction_pitch: field(0x588),
            view_torso_pitch: field(0x58c),
            torso_height: field(0x5a8),
            torso_pitch: field(0x5ac),
            waist_pitch: field(0x5b0),
            height_position_adjustment: field(0x108),
        }
    }
    pub(super) fn write_player(&self, ps: &mut PlayerState) {
        for (offset, value) in [
            (0x584, self.direction),
            (0x588, self.direction_pitch),
            (0x58c, self.view_torso_pitch),
            (0x5a8, self.torso_height),
            (0x5ac, self.torso_pitch),
            (0x5b0, self.waist_pitch),
            (0x108, self.height_position_adjustment),
        ] {
            ps.words_mut()[offset / 4] = value.to_bits();
        }
    }
}

fn signed(angle: f32) -> f32 {
    if angle <= -180. || angle > 180. {
        180. - (180. - angle).rem_euclid(360.)
    } else {
        angle
    }
}
fn short(angle: f32) -> i32 {
    (f64::from(angle) * f64::from(182.04445_f32)) as i32 & 65535
}
pub(super) fn delta(a: f32, b: f32) -> f32 {
    signed(short(a - b) as f32 * (360. / 65536.))
}
fn elevation(v: Vec3) -> f32 {
    if v[0] == 0. && v[1] == 0. {
        return if v[2] > 0. { 270. } else { 90. };
    }
    let pitch = (-f64::from(v[2])
        .atan2(f64::from((v[0] * v[0] + v[1] * v[1]).sqrt()))
        .to_degrees()) as f32;
    if pitch < 0. { pitch + 360. } else { pitch }
}
fn slope_pitch(yaw: f32, normal: Vec3) -> f32 {
    let (s, c) = yaw.to_radians().sin_cos();
    let forward = [c, s, 0.];
    elevation(ma(
        forward,
        -dot(forward, normal) / dot(normal, normal),
        normal,
    ))
}

pub(super) fn step_probe_or_restore(
    ps: &mut MovementState,
    trace: &Trace<'_>,
    origin: Vec3,
    velocity: Vec3,
) -> bool {
    if ps.pm_flags & 1 == 0
        || clearance::check(ps, trace, ps.prone.direction, 66., true, true, None)
    {
        return true;
    }
    ps.origin = origin;
    ps.velocity = velocity;
    false
}

const STAND_TO_CROUCH: &[(i32, f32)] = &[
    (0, 60.),
    (1, 59.5),
    (4, 58.5),
    (30, 56.),
    (80, 44.),
    (90, 41.5),
    (95, 40.5),
    (100, 40.),
];
const CROUCH_TO_STAND: &[(i32, f32)] = &[
    (0, 40.),
    (5, 40.5),
    (10, 41.5),
    (20, 44.),
    (70, 56.),
    (96, 58.5),
    (99, 59.5),
    (100, 60.),
];
const CROUCH_TO_PRONE: &[(i32, f32)] = &[
    (0, 40.),
    (11, 38.),
    (22, 33.),
    (34, 25.),
    (45, 16.),
    (50, 15.),
    (55, 16.),
    (70, 18.),
    (90, 17.),
    (100, 11.),
];
const PRONE_TO_CROUCH: &[(i32, f32)] = &[
    (0, 11.),
    (5, 10.),
    (30, 21.),
    (50, 25.),
    (67, 31.),
    (83, 34.),
    (100, 40.),
];

fn duration(ps: &MovementState) -> i32 {
    if ps.height_lerp_target == 11 || (ps.height_lerp_target == 40 && !ps.height_lerp_down) {
        400
    } else {
        200
    }
}
fn curve(ps: &MovementState, percent: i32) -> f32 {
    let keys = match (ps.height_lerp_target, ps.height_lerp_down) {
        (11, _) => CROUCH_TO_PRONE,
        (40, false) => PRONE_TO_CROUCH,
        (40, true) => STAND_TO_CROUCH,
        _ => CROUCH_TO_STAND,
    };
    for points in keys.windows(2) {
        let [(a, from), (b, to)] = [points[0], points[1]];
        if percent <= b {
            let blend = (percent - a) as f32 / (b - a) as f32;
            return from + (to - from) * blend;
        }
    }
    keys[keys.len() - 1].1
}

pub(super) fn walk_scale(ps: &MovementState, time: i32) -> f32 {
    let blend = if ps.height_lerp_time == 0 {
        0.
    } else {
        ((time - ps.height_lerp_time) as f32 / duration(ps) as f32).clamp(0., 1.)
    };
    if blend != 0. {
        if ps.height_lerp_target == 11 {
            return (1. - blend) * 0.65 + blend * 0.15;
        }
        if ps.height_lerp_target == 40 && !ps.height_lerp_down {
            return (1. - blend) * 0.15 + blend * 0.65;
        }
    }
    match ps.view_height_target {
        11 => 0.15,
        40 => 0.65,
        _ => 1.,
    }
}
fn update_height(ps: &mut MovementState, h: &Hull<'_>, dt: f32, s: &MovementSettings) {
    let target = ps.view_height_target;
    if target == 0 || ps.view_height == 0. {
        ps.view_height = target as f32;
        return;
    }
    if !matches!(target, 11 | 40 | 60) {
        ps.height_lerp_time = 0;
        ps.view_height += (target as f32 - ps.view_height).clamp(-dt * 180., dt * 180.);
        return;
    }
    let mut pct = 0;
    if ps.height_lerp_time != 0 {
        pct = ((ps.command_time - ps.height_lerp_time) * 100 / duration(ps)).clamp(0, 100);
        if pct == 100 {
            ps.view_height = ps.height_lerp_target as f32;
            ps.height_lerp_time = 0;
            ps.prone.height_position_adjustment = 0.;
        } else {
            ps.view_height = curve(ps, pct);
            // All four stock curves have zero forward displacement.
            let offset = ps.prone.height_position_adjustment;
            if offset.abs() > 0.05 {
                let velocity = ps.velocity;
                let (sine, c) = ps.view_angles[1].to_radians().sin_cos();
                let displacement = if ps.ground_entity == 1023 {
                    -0.5 * offset
                } else {
                    -offset
                };
                ps.velocity = [c * displacement / dt, sine * displacement / dt, 0.];
                super::slide::step_slide(ps, h, dt, true, None, s);
                ps.velocity = velocity;
                ps.prone.height_position_adjustment = 0.;
            }
        }
    }
    if ps.height_lerp_time == 0 {
        if ps.view_height != target as f32 {
            ps.height_lerp_time = ps.command_time;
            ps.height_lerp_down = ps.view_height > target as f32;
            ps.height_lerp_target = match target {
                11 if ps.view_height > 40. => 40,
                60 if ps.view_height < 40. => 40,
                n => n,
            };
        }
    } else if (ps.height_lerp_down && target > ps.height_lerp_target)
        || (!ps.height_lerp_down && target < ps.height_lerp_target)
    {
        pct = 100 - pct;
        ps.height_lerp_down = !ps.height_lerp_down;
        ps.height_lerp_target = match (ps.height_lerp_down, ps.height_lerp_target) {
            (true, 60) | (false, 11) => 40,
            (true, 40) => 11,
            (false, 40) => 60,
            (_, n) => n,
        };
        if pct == 100 {
            ps.view_height = ps.height_lerp_target as f32;
            ps.height_lerp_time = 0;
        } else {
            ps.height_lerp_time = ps.command_time - duration(ps) * pct / 100;
        }
        ps.prone.height_position_adjustment = 0.;
    }
}

fn desired(ps: &mut MovementState, cmd: &mut UserCmd, h: &mut Hull<'_>) -> u32 {
    let was = ps.pm_flags & 3;
    if ps.entity_flags & 0x300 != 0 {
        return match ps.entity_flags & 0x300 {
            0x100 => 1,
            0x200 => 2,
            _ => 0,
        };
    }
    if ps.pm_flags & 0x8000 != 0 {
        return was;
    }
    if ps.pm_flags & 0x20 != 0 && cmd.buttons & 0x300 != 0 {
        cmd.buttons &= !0x300;
        ps.events.push((0x8c, 0));
    }
    let mut clear = |height| {
        h.maxs[2] = height;
        !(h.trace)(ps.origin, ps.origin, h.mins, h.maxs, h.mask & !0x2000000).all_solid
    };
    let report = cmd.buttons & 0x2000 == 0;
    if cmd.buttons & 0x100 != 0 {
        if was == 1 {
            return 1;
        }
        if ps.ground_entity == 1023 {
            return was;
        }
        if clearance::check(ps, h.trace, ps.view_angles[1], 66., false, true, None) {
            return 1;
        }
        ps.pm_flags |= 0x10000;
        if report {
            ps.events.push((if was == 2 { 0x8d } else { 0x8c }, 0));
        }
    } else if cmd.buttons & 0x200 != 0 {
        if was != 1 || clear(50.) {
            ps.animations.push(MovementAnim::Event(
                if was == 1 { 0xc } else { 0xd },
                false,
                false,
            ));
            return 2;
        }
        if report {
            ps.events.push((0x8e, 2));
        }
    } else if was != 0 {
        if clear(ps.maxs[2]) {
            ps.animations.push(MovementAnim::Event(
                if was == 1 { 0x10 } else { 0xe },
                false,
                false,
            ));
            return 0;
        }
        if was == 1 && clear(50.) {
            return 2;
        }
        if report {
            ps.events.push((if was == 1 { 0x8e } else { 0x8d }, 1));
        }
    }
    was
}

pub(super) fn update_stance(
    ps: &mut MovementState,
    cmd: &mut UserCmd,
    h: &mut Hull<'_>,
    dt: f32,
    s: &MovementSettings,
) {
    if ps.pm_type >= 6 {
        ps.entity_bounds = [h.mins, h.maxs];
        ps.view_height_target = 8;
        update_height(ps, h, dt, s);
        return;
    }
    let was_prone = ps.pm_flags & 1 != 0;
    let stance = desired(ps, cmd, h);
    ps.pm_flags = (ps.pm_flags & !3) | stance;
    if ps.height_lerp_time == 0 {
        let previous = ps.view_height_target;
        ps.view_height_target = match (stance, previous) {
            (1, 60) | (0 | 2, 11) => 40,
            (1, _) => 11,
            (2, _) => 40,
            _ => 60,
        };
        if (previous == 11) != (ps.view_height_target == 11) {
            ps.animations.push(MovementAnim::ProneChange);
            if ps.view_height_target == 11 && ps.pm_time == 0 {
                ps.pm_time = 1800;
                ps.pm_flags |= 0x80000;
            }
        }
    }
    update_height(ps, h, dt, s);
    ps.pm_flags &= !3;
    ps.entity_flags &= !12;
    h.maxs[2] = match ps.view_height_target {
        11 => {
            ps.pm_flags |= 1;
            ps.entity_flags |= 8;
            30.
        }
        40 => {
            ps.pm_flags |= 2;
            ps.entity_flags |= 4;
            50.
        }
        _ => ps.maxs[2],
    };
    ps.entity_bounds = [h.mins, h.maxs];
    if !was_prone && ps.pm_flags & 1 != 0 {
        if cmd.forward_move != 0 || cmd.right_move != 0 {
            ps.pm_flags &= !0x840;
            ps.events.push((0x95, 0));
        }
        let sweep = |a, b| (h.trace)(a, b, h.mins, h.maxs, h.mask & !0x2000000);
        let above = vertical(ps.origin, 10.);
        let top = lerp(ps.origin, above, sweep(ps.origin, above).fraction);
        ps.origin = lerp(top, ps.origin, sweep(top, ps.origin).fraction);
        ps.prone.direction = ps.view_angles[1];
        let ground = sweep(ps.origin, vertical(ps.origin, -0.25));
        ps.prone.direction_pitch = if ground.start_solid || ground.fraction >= 1. {
            0.
        } else {
            slope_pitch(ps.prone.direction, ground.normal)
        };
        let amount = delta(ps.prone.direction_pitch, ps.view_angles[0]);
        ps.prone.view_torso_pitch = if amount.abs() > 45. {
            ps.view_angles[0] + 45. * amount.signum()
        } else {
            ps.prone.direction_pitch
        };
    }
}
