//! Command-driven CoD2 movement (PM_Pmove, PM_SingleStep and slide collision).
mod feedback;
mod flight;
mod ladder;
pub mod mantle;
pub mod prone;
mod slide;
mod state;
use crate::{
    collision::{CollisionMap, TraceResult as Hit},
    usercmd::UserCmd,
};
pub use state::{MovementSettings, MovementState};
use std::{
    cell::{Cell, RefCell},
    io,
};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MovementAnim {
    Event(u8, bool, bool),
    Move,
    MoveId(u8),
    ProneChange,
    DropTimers,
}

type Vec3 = [f32; 3];
fn dot(a: Vec3, b: Vec3) -> f32 {
    (f64::from(a[0]) * f64::from(b[0])
        + f64::from(a[1]) * f64::from(b[1])
        + f64::from(a[2]) * f64::from(b[2])) as f32
}
fn cross(a: Vec3, b: Vec3) -> Vec3 {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}
fn scale(a: Vec3, k: f32) -> Vec3 {
    a.map(|v| v * k)
}
pub(crate) fn ma(a: Vec3, k: f32, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| (f64::from(a[i]) + f64::from(k) * f64::from(b[i])) as f32)
}
fn diff(a: Vec3, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| a[i] - b[i])
}
pub(crate) fn lerp(a: Vec3, b: Vec3, t: f32) -> Vec3 {
    std::array::from_fn(|i| {
        (f64::from(a[i]) + (f64::from(b[i]) - f64::from(a[i])) * f64::from(t)) as f32
    })
}
fn length(a: Vec3) -> f32 {
    dot(a, a).sqrt()
}
fn unit(a: Vec3) -> Vec3 {
    let l = length(a);
    if l > 0. { scale(a, l.recip()) } else { a }
}
fn vertical(mut a: Vec3, z: f32) -> Vec3 {
    a[2] += z;
    a
}
fn clip(v: Vec3, n: Vec3) -> Vec3 {
    let back = dot(v, n);
    ma(
        v,
        (-f64::from(back) + f64::from(0.001_f32) * f64::from(back.abs())) as f32,
        n,
    )
}

pub type Trace<'a> = dyn Fn(Vec3, Vec3, Vec3, Vec3, u32) -> Hit + 'a;
#[derive(Default)]
struct Contacts {
    slots: RefCell<Vec<u16>>,
    ignore_body: Cell<bool>,
    xy_speed: Cell<f32>,
}
impl Contacts {
    fn record(&self, entity: u16) {
        let mut slots = self.slots.borrow_mut();
        if entity != 1022 && slots.len() < 32 && !slots.contains(&entity) {
            slots.push(entity);
        }
    }
}
struct Hull<'a> {
    contacts: &'a Contacts,
    trace: &'a Trace<'a>,
    mins: Vec3,
    maxs: Vec3,
    mask: u32,
}
impl Hull<'_> {
    fn sweep(&self, start: Vec3, end: Vec3) -> Hit {
        self.bounds_trace(start, end, self.mins, self.maxs)
    }
    fn bounds_trace(&self, start: Vec3, end: Vec3, mins: Vec3, maxs: Vec3) -> Hit {
        let mask = if self.contacts.ignore_body.get() {
            self.mask & !0x2000000
        } else {
            self.mask
        };
        let first = (self.trace)(start, end, mins, maxs, mask);
        if first.start_solid && first.contents & 0x2000000 != 0 {
            self.contacts.record(first.entity);
            self.contacts.ignore_body.set(true);
            (self.trace)(start, end, mins, maxs, mask & !0x2000000)
        } else {
            first
        }
    }
}
pub fn advance(
    ps: &mut MovementState,
    cmd: &UserCmd,
    settings: &MovementSettings,
    map: &CollisionMap,
) -> io::Result<()> {
    advance_steps(ps, cmd, settings, map, |_, _, _, _| Ok(()))
}
/// Runs one callback after each PM_SingleStep. The caller can update weapon
/// state with the exact command interval used by player prediction.
pub fn advance_steps(
    ps: &mut MovementState,
    cmd: &UserCmd,
    settings: &MovementSettings,
    map: &CollisionMap,
    after: impl FnMut(&mut MovementState, &UserCmd, &UserCmd, i32) -> io::Result<()>,
) -> io::Result<()> {
    advance_traced_steps(
        ps,
        cmd,
        settings,
        &|start, end, mins, maxs, mask| map.trace(start, end, mins, maxs, mask),
        after,
    )
}

pub fn advance_traced_steps(
    ps: &mut MovementState,
    cmd: &UserCmd,
    settings: &MovementSettings,
    trace: &Trace<'_>,
    mut after: impl FnMut(&mut MovementState, &UserCmd, &UserCmd, i32) -> io::Result<()>,
) -> io::Result<()> {
    ps.animations.clear();
    ps.touched.clear();
    ps.entity_bounds = [[0.; 3]; 2];
    let contacts = Contacts::default();
    if cmd.server_time <= ps.command_time {
        return Ok(());
    }
    let mut current = *cmd;
    ps.command_time = ps.command_time.max(cmd.server_time.saturating_sub(1000));
    let mut old = UserCmd {
        server_time: ps.command_time,
        buttons: ps.last_buttons,
        ..ps.previous_command
    };
    while ps.command_time < cmd.server_time {
        let ms = (cmd.server_time - ps.command_time).min(66);
        ps.command_time += ms;
        current.server_time = ps.command_time;
        single_step(ps, &mut current, settings, trace, &contacts, ms)?;
        after(ps, &current, &old, ms)?;
        old = current;
        ps.seed_previous_command(current);
    }
    ps.touched = contacts.slots.into_inner();
    Ok(())
}

/// AngleDelta quantizes the difference through the engine's 16-bit AngleMod.
fn signed_angle(degrees: f32) -> f32 {
    let word = (f64::from(degrees) * f64::from(182.04445_f32)) as i32 as u16;
    let degrees = f32::from(word) * (360. / 65536.);
    if degrees > 180. {
        degrees - 360.
    } else {
        degrees
    }
}
fn base_speed(ps: &MovementState) -> f32 {
    if ps.pm_flags & 0x100 != 0 || ps.lean != 0. {
        ps.speed * 0.4
    } else {
        ps.speed
    }
}

fn angles(ps: &mut MovementState, cmd: &UserCmd, s: &MovementSettings) {
    if ps.pm_type >= 5 {
        if ps.pm_type >= 6 && ps.dead_yaw == 999 {
            let yaw = i32::from(ps.delta_angles[1] as i16) + i32::from(cmd.angles[1] as i16);
            ps.dead_yaw = (yaw as f32 * (360. / 65536.)) as i32;
        }
        return;
    }
    for (axis, &input) in cmd.angles.iter().enumerate() {
        let mut angle = i32::from((input.wrapping_add(ps.delta_angles[axis] as u16)) as i16);
        if axis == 0 {
            let clamped = angle.clamp(
                -(s.pitch_up * 182.044_45) as i32,
                (s.pitch_down * 182.044_45) as i32,
            );
            if angle != clamped {
                ps.delta_angles[0] = clamped - i32::from(input);
                angle = clamped;
            }
        }
        ps.view_angles[axis] = angle as f32 * (360. / 65536.);
    }
}
fn basis(angles: Vec3) -> (Vec3, Vec3) {
    let (sp, cp) = (f64::from(angles[0]).to_radians() as f32).sin_cos();
    let (sy, cy) = (f64::from(angles[1]).to_radians() as f32).sin_cos();
    let (sr, cr) = (f64::from(angles[2]).to_radians() as f32).sin_cos();
    (
        [cp * cy, cp * sy, -sp],
        [-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp],
    )
}

fn accelerate(ps: &mut MovementState, desired: Vec3, rate: f32, dt: f32, s: &MovementSettings) {
    accelerate_dir(ps, unit(desired), length(desired), rate, dt, s);
}
fn accelerate_dir(
    ps: &mut MovementState,
    dir: Vec3,
    speed: f32,
    rate: f32,
    dt: f32,
    s: &MovementSettings,
) {
    let add = speed - dot(ps.velocity, dir);
    if add <= 0. {
        return;
    }
    let accel = (f64::from(rate) * f64::from(dt) * f64::from(speed.max(s.stop_speed))) as f32;
    let mut amount = accel.min(add);
    let old = [ps.old_velocity[0], ps.old_velocity[1], 0.];
    if ps.pm_type != 2 && amount > s.inertia_max && dot(old, old) >= 0.0001 {
        let mut new = ma(ps.velocity, amount, dir);
        new[2] = 0.;
        if dot(unit(new), unit(old)) < s.inertia_angle {
            amount = s.inertia_max;
        }
    }
    ps.velocity = ma(ps.velocity, amount, dir);
}

fn friction(ps: &mut MovementState, walking: bool, slick: bool, dt: f32, s: &MovementSettings) {
    let mut v = ps.velocity;
    if walking {
        v[2] = 0.;
    }
    let speed = length(v);
    if speed < 1. {
        ps.velocity = [0.; 3];
        return;
    }
    let mut drop = 0.;
    if walking && !slick {
        let factor = if ps.pm_flags & 0x200 != 0 {
            0.3
        } else if ps.pm_flags & 0x80000 != 0 {
            jump_factor(ps, s)
        } else {
            1.
        };
        drop = speed.max(s.stop_speed) * factor * s.friction * dt;
    }
    if ps.pm_type == 4 {
        drop += speed * 5. * dt;
    }
    ps.velocity = scale(ps.velocity, (speed - drop).max(0.) / speed);
}
fn jump_factor(ps: &MovementState, s: &MovementSettings) -> f32 {
    if s.jump_slowdown {
        1. + ps.pm_time.min(1700) as f32 * (1.5 / 1700.)
    } else {
        1.
    }
}
fn ground_trace(ps: &mut MovementState, h: &Hull<'_>, cmd: &UserCmd) -> Option<Hit> {
    let mut ground = h.sweep(vertical(ps.origin, 0.25), vertical(ps.origin, -0.25));
    if ground.all_solid {
        // PM_CorrectAllSolid's 26 candidates at 0x081551e0. Cardinal
        // neighbours across all levels precede the diagonals.
        let mut corrected = None;
        for diagonal in [false, true] {
            for z in [1., 0., -1.] {
                let offsets: &[[f32; 2]] = if diagonal {
                    &[[-1., -1.], [1., -1.], [1., 1.], [-1., 1.]]
                } else {
                    &[[0., 0.], [-1., 0.], [0., -1.], [1., 0.], [0., 1.]]
                };
                for &[x, y] in offsets {
                    if x == 0. && y == 0. && z == 0. {
                        continue;
                    }
                    let v = ma(ps.origin, 1., [x, y, z]);
                    if !h.sweep(v, v).start_solid {
                        corrected = Some(v);
                        break;
                    }
                }
                if corrected.is_some() {
                    break;
                }
            }
            if corrected.is_some() {
                break;
            }
        }
        if let Some(v) = corrected {
            let end = vertical(v, -1.);
            ground = h.sweep(v, end);
            ps.origin = lerp(v, end, ground.fraction);
        } else {
            ps.ground_entity = 1023;
            ps.pm_flags &= !0x80000;
            ps.jump_origin_z = 0.;
            return None;
        }
    }
    if ground.start_solid {
        ground = h.sweep(vertical(ps.origin, -0.001), vertical(ps.origin, -0.25));
    }
    if ground.start_solid || ground.all_solid {
        ps.ground_entity = 1023;
        return None;
    }
    let takeoff =
        ps.pm_flags & 0x20 == 0 && ps.velocity[2] > 0. && dot(ps.velocity, ground.normal) > 10.;
    if ground.fraction == 1. || takeoff {
        let event = if ground.fraction < 1. {
            takeoff
        } else {
            ps.ground_entity != 1023
                && (h.trace)(ps.origin, vertical(ps.origin, -64.), h.mins, h.maxs, h.mask).fraction
                    == 1.
        };
        if event {
            ps.animations.push(MovementAnim::Event(
                if cmd.forward_move < 0 { 4 } else { 3 },
                false,
                ground.fraction == 1.,
            ));
        }
        ps.ground_entity = 1023;
        return None;
    }
    ps.ground_entity = if ground.normal[2] < 0.7 {
        ps.pm_flags &= !0x80000;
        ps.jump_origin_z = 0.;
        1023
    } else {
        h.contacts.record(ground.entity);
        ground.entity
    };
    Some(ground)
}
fn drop_timers(ps: &mut MovementState, ms: i32) {
    ps.animations.push(MovementAnim::DropTimers);
    if ps.pm_time != 0 {
        ps.pm_time = (ps.pm_time - ms).max(0);
        if ps.pm_time == 0 {
            ps.pm_flags &= 0xfff7_f9ff;
        }
    }
}

fn single_step(
    ps: &mut MovementState,
    cmd: &mut UserCmd,
    s: &MovementSettings,
    trace: &Trace<'_>,
    contacts: &Contacts,
    ms: i32,
) -> io::Result<()> {
    let dt = ms as f32 * 0.001;
    if ps.pm_flags & 0x8000 != 0 {
        cmd.buttons &= 0x2300;
        cmd.forward_move = 0;
        cmd.right_move = 0;
        ps.velocity = [0.; 3];
    }
    if cmd.buttons & 0x40000 != 0 {
        cmd.buttons &= 0x43300;
        cmd.forward_move = 0;
        cmd.right_move = 0;
        ps.entity_flags |= 0x200000;
    } else {
        ps.entity_flags &= !0x200000;
    }
    ps.pm_flags &= !0x10000;
    if ps.pm_type < 6 && cmd.buttons & 0x4001 == 0 {
        ps.pm_flags &= !0x1000;
    }
    if ps.pm_flags & 1 == 0 {
        ps.pm_flags &= !0x800;
    } else {
        let old = ps.previous_command;
        let harder = cmd.forward_move.unsigned_abs() > old.forward_move.unsigned_abs()
            || cmd.right_move.unsigned_abs() > old.right_move.unsigned_abs();
        let st = ps.ads.weapon_state;
        if harder && !matches!(st, 3 | 11..=16) {
            ps.pm_flags &= !0x840;
            ps.events.push((0x95, 0));
        } else if !harder && ps.pm_flags & 0x40 == 0 && matches!(st, 0..=2 | 5) {
            ps.pm_flags &= !0x800;
        }
    }
    ps.entity_flags &= !0x40;
    if ps.pm_type != 5
        && ps.pm_flags & 0x1000 == 0
        && matches!(ps.ads.weapon_state, 0 | 3)
        && ps.ads.has_clip_ammo
        && cmd.buttons & 1 != 0
    {
        ps.entity_flags |= 0x40;
    }
    let old_yaw = ps.view_angles[1];
    angles(ps, cmd, s);
    if ps.pm_type < 5 {
        if ps.entity_flags & 0x300 != 0 {
            for axis in 0..2 {
                let base = ps.clamp_base[axis];
                let limit = ps.clamp_range[axis];
                let delta = signed_angle(base - ps.view_angles[axis]);
                let adjust = delta - delta.clamp(-limit.abs(), limit.abs());
                if adjust != 0. {
                    ps.delta_angles[axis] =
                        ps.delta_angles[axis].wrapping_add((adjust * 182.044_45) as i32 & 65535);
                    ps.view_angles[axis] =
                        (base - if adjust > 0. { limit } else { -limit }).rem_euclid(360.);
                }
            }
        } else if ps.pm_flags & 4 != 0 {
            mantle::apply_yaw_cap(ps, s);
        } else {
            ladder::cap_yaw(ps, s);
            prone::view(ps, cmd, trace, old_yaw, ms, s.prone_yawcap);
        }
    }
    ps.mantle.flags &= !8;
    if ps.pm_type > 5 || (ps.view_height_target == 11 && ps.pm_flags & 0x840 != 0) {
        cmd.forward_move = 0;
        cmd.right_move = 0;
    }
    if !matches!(ps.pm_type, 2..=5) && ps.entity_flags & 0x300 == 0 && ps.pm_flags & 4 == 0 {
        flight::lean(ps, cmd, ms, trace);
    }
    if (1..=5).contains(&ps.pm_type) || ps.pm_type == 7 {
        ladder::leave(ps);
    }
    if cmd.forward_move < 0 {
        ps.pm_flags |= 0x80;
    } else if cmd.forward_move > 0 || cmd.right_move != 0 {
        ps.pm_flags &= !0x80;
    }
    let (mut forward, mut right) = basis(ps.view_angles);
    let fm = f32::from(cmd.forward_move);
    let rm = f32::from(cmd.right_move);
    let axis_length = fm.hypot(rm);
    let mut hull = Hull {
        contacts,
        trace,
        mins: ps.mins,
        maxs: ps.maxs,
        mask: 0x2810011,
    };
    if ps.pm_type != 0 && ps.pm_type != 6 || ps.entity_flags & 0x300 != 0 {
        ps.pm_flags = ps
            .ads
            .update(ps.pm_flags, ps.pm_type, false, cmd, ps.last_buttons);
    }
    match ps.pm_type {
        4 => {
            hull.mins = [-8.; 3];
            hull.maxs = [8., 8., 16.];
            ps.entity_bounds = [hull.mins, hull.maxs];
            hull.mask = 0x800811;
            ps.pm_flags &= !3;
            ps.view_height = 0.;
            ps.view_height_target = 0;
            if cmd.buttons & 0x100 != 0 {
                ps.events.push((0x8c, 0));
            }
            cmd.buttons &= !0x100;
            friction(ps, false, false, dt, s);
            let spec_speed = base_speed(ps) * s.spectator_scale;
            let mut wish = [0.; 3];
            if axis_length > 0. {
                wish = scale(
                    ma(scale(forward, fm), rm, right),
                    spec_speed * fm.abs().max(rm.abs()) / (127. * axis_length),
                );
            }
            if cmd.buttons & 0x40 != 0 {
                wish[2] -= spec_speed;
            }
            if cmd.buttons & 0x80 != 0 {
                wish[2] += spec_speed;
            }
            accelerate(ps, wish, 8., dt, s);
            slide::step_slide(ps, &hull, dt, false, None, s);
            drop_timers(ps, ms);
            return Ok(());
        }
        1 | 7 => {
            ps.velocity = [0.; 3];
            ps.ground_entity = 1023;
            prone::update_stance(ps, cmd, &mut hull, dt, s);
            drop_timers(ps, ms);
            ps.animations.push(MovementAnim::Move);
            return Ok(());
        }
        2 | 3 => {
            flight::fly(ps, cmd, s, dt);
            drop_timers(ps, ms);
            return Ok(());
        }
        5 => return Ok(()),
        0 | 6 => {}
        kind => {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                format!("unimplemented pm_type {kind}"),
            ));
        }
    }
    if ps.pm_type == 6 {
        hull.mask = 0x810011;
    }
    if ps.entity_flags & 0x300 != 0 {
        ps.velocity = [0.; 3];
        ps.ground_entity = 1023;
        ladder::leave(ps);
        prone::update_stance(ps, cmd, &mut hull, dt, s);
        drop_timers(ps, ms);
        ps.animations.push(MovementAnim::Move);
        return Ok(());
    }
    let start = ps.origin;
    let prev_vz = ps.velocity[2];
    let mut ground = None;
    if ps.pm_flags & 4 == 0 {
        prone::update_stance(ps, cmd, &mut hull, dt, s);
        ground = feedback::detect_landing(ps, &hull, start, prev_vz, s, cmd);
    }
    mantle::try_start(ps, cmd, s, trace, forward, hull.mask)?;
    if ps.pm_flags & 4 != 0 {
        ladder::leave(ps);
        ps.ground_entity = 1023;
        prone::update_stance(ps, cmd, &mut hull, dt, s);
        mantle::update(ps, s, ms)?;
        return Ok(());
    }
    ps.pm_flags = ps.ads.update(
        ps.pm_flags,
        ps.pm_type,
        ground.is_some(),
        cmd,
        ps.last_buttons,
    );
    prone::smooth(ps, trace, ground.map(|g| g.normal), dt);
    if ps.pm_type == 6 && ps.ground_entity != 1023 {
        let length = length(ps.velocity);
        ps.velocity = scale(unit(ps.velocity), (length - 20.).max(0.));
    }
    ladder::check(ps, cmd, &hull, forward);
    if ps.pm_type >= 6 && ps.ground_entity == 1023 {
        ground = None;
    }
    drop_timers(ps, ms);
    let mut walking = ps.ground_entity != 1023;
    if walking && ps.pm_flags & 0x20 == 0 && ps.pm_flags & 0x80000 != 0 {
        let damping = if ps.pm_time > 1800 {
            ps.pm_flags &= !0x80000;
            ps.jump_origin_z = 0.;
            0.65
        } else if ps.pm_time == 0 {
            let low = ps.origin[2] < ps.jump_origin_z + 18.;
            ps.pm_time = if low { 1800 } else { 1200 };
            if low { 0.65 } else { 0.5 }
        } else {
            1.
        };
        if s.jump_slowdown {
            ps.velocity = scale(ps.velocity, damping);
        }
    }
    if (walking || ps.pm_flags & 0x20 != 0)
        && ps.pm_type < 6
        && cmd.server_time.saturating_sub(ps.jump_time) >= 500
        && ps.pm_flags & 0x1007 == 0
        && cmd.buttons & 0x400 != 0
    {
        if ps.last_buttons & 0x400 != 0 {
            cmd.buttons &= !0x400;
        } else {
            let factor = if ps.pm_flags & 0x80000 != 0 {
                jump_factor(ps, s)
            } else {
                1.
            };
            ps.velocity[2] = (2. * ps.gravity * s.jump_height / factor).sqrt();
            let material = ground.map_or(0, feedback::surface);
            if ps.pm_flags & 0x20 != 0 {
                ps.events.push((0x5b, 0));
                ladder::jump_push(ps, forward, s);
            } else if material != 0 {
                ps.events.push((material + 0x46, 0));
            }
            ps.animations.push(MovementAnim::Event(
                if cmd.forward_move < 0 { 4 } else { 3 },
                false,
                true,
            ));
            ps.jump_time = cmd.server_time;
            ps.jump_origin_z = ps.origin[2];
            ps.pending_spread += s.jump_spread_add;
            ps.pm_flags |= 0x80000;
            ps.pm_time = 0;
            ps.ground_entity = 1023;
            walking = false;
            ground = None;
        }
    }
    if ps.pm_flags & 0x20 != 0 {
        ladder::climb(ps, cmd, &hull, ground, s, dt);
    } else {
        let slick = ground.is_some_and(|g| g.surface_flags & 2 != 0) || ps.pm_flags & 0x400 != 0;
        friction(ps, walking, slick, dt, s);
        forward[2] = 0.;
        right[2] = 0.;
        let mut multiplier = base_speed(ps);
        let mut max_axis = fm.abs().max(rm.abs());
        if walking {
            let plane = ground.expect("ground entity from trace").normal;
            forward = clip(forward, plane);
            right = clip(right, plane);
            max_axis =
                (if fm < 0. { -fm * s.back_scale } else { fm }).max(rm.abs() * s.strafe_scale);
            multiplier *= prone::walk_scale(ps, cmd.server_time);
            if ps.ads.move_speed_scale > 0. {
                multiplier *= ps.ads.move_speed_scale;
            }
            if ps.damage_timer != 0 && s.damage_max_time != 0. {
                multiplier *= 1. - ps.damage_timer as f32 * s.damage_min_scale / s.damage_max_time;
            }
            ps.damage_timer = (ps.damage_timer - (f64::from(dt) * 1000.) as i32).max(0);
            if cmd.buttons & 0x800 != 0 {
                multiplier *= 0.4;
            }
        }
        if axis_length > 0. && ps.pm_type < 6 {
            let wish = ma(scale(unit(forward), fm), rm, unit(right));
            let mut rate = if !walking || slick {
                1.
            } else if ps.pm_flags & 1 != 0 {
                19.
            } else if ps.pm_flags & 2 != 0 {
                12.
            } else {
                9.
            };
            if walking && ps.pm_flags & 0x200 != 0 {
                rate *= 0.25;
            }
            accelerate_dir(
                ps,
                unit(wish),
                length(wish) * (multiplier * max_axis / (127. * axis_length)),
                rate,
                dt,
                s,
            );
        }
        if walking && slick {
            ps.velocity[2] -= ps.gravity * dt;
        }
        if let Some(g) = ground {
            let before = ps.velocity;
            ps.velocity = clip(before, g.normal);
            if walking && dot(ps.velocity, before) > 0. {
                ps.velocity = scale(unit(ps.velocity), length(before));
            }
        }
        if !walking || ps.velocity[0] != 0. || ps.velocity[1] != 0. {
            slide::step_slide(ps, &hull, dt, !walking, ground, s);
        }
        let moved = diff(ps.origin, start);
        ps.movement_dir = 0;
        if ps.pm_flags & 1 != 0 {
            ps.movement_dir =
                prone::delta(ps.prone.direction, ps.view_angles[1]).clamp(-90., 90.) as i8;
        } else if (fm != 0. || rm != 0.) && ps.ground_entity != 1023 && length(moved) > 5. * dt {
            let dir = unit(moved);
            let mut travel_yaw = f64::from(dir[1]).atan2(f64::from(dir[0])).to_degrees() as f32;
            if travel_yaw < 0. {
                travel_yaw += 360.;
            }
            let yaw = signed_angle(travel_yaw - ps.view_angles[1]);
            let dir = if fm < 0. {
                signed_angle(yaw.trunc() + 180.)
            } else {
                yaw
            };
            ps.movement_dir = dir.clamp(-90., 90.) as i8;
        }
    }
    let moved = diff(ps.origin, start);
    let final_ground = feedback::detect_landing(ps, &hull, start, prev_vz, s, cmd);
    ps.animations.push(MovementAnim::Move);
    if ps.pm_type < 6 {
        contacts.xy_speed.set(ps.velocity[0].hypot(ps.velocity[1]));
    }
    if let Some(g) = final_ground.filter(|_| ps.ground_entity != 1023) {
        feedback::footsteps(ps, cmd, g, s, ms);
    }
    if ps.ground_entity == 1023 && ps.pm_flags & 0x20 != 0 {
        feedback::ladder_step(ps, &hull, ms);
    }
    feedback::foliage(ps, &hull, s);
    let actual_speed_sq = (f64::from(dot(moved, moved)) / f64::from(dt).powi(2)) as f32;
    if actual_speed_sq < 0.25 * dot(ps.velocity, ps.velocity) {
        ps.velocity = scale(moved, dt.recip());
    }
    for (old, v) in ps.old_velocity.iter_mut().zip(ps.velocity) {
        *old += (v - *old) * dt;
    }
    ps.velocity = ps.velocity.map(f32::round_ties_even);
    Ok(())
}

#[cfg(test)]
mod tests;
