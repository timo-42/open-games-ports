//! Server-side first-person view motion used by G_GetViewFrame.
//! ClientThink_real samples both motion channels before running Pmove.
use super::{Vec3, float, shot::number, yaw};
use crate::{
    assets::weapons::{WeaponDefinition, WeaponRegistry},
    combat::inventory,
    config::ServerConfig,
    player::Player,
    snapshot::PlayerState,
    vm::math::axes,
};

#[derive(Clone, Debug, Default)]
pub struct ViewParameters {
    idle_amount: [f32; 2],
    idle_speed: [f32; 2],
    idle_stance: [f32; 3],
    ads_bob_factor: f32,
    ads_view_bob: f32,
    ads_aim_pitch: f32,
    rotation_threshold: [f32; 3],
    rotation: [Vec3; 3],
    rotation_rate: [f32; 2],
    sway: [[f32; 4]; 2],
    spring: [[f32; 4]; 2],
    kick_limit: [f32; 2],
}
impl ViewParameters {
    pub(crate) fn parse<'a>(raw: impl Fn(&str) -> &'a [u8]) -> Self {
        let f = |k: &str| number(raw(k));
        let stance = ["stand", "ducked", "prone"];
        let pair = ["hip", "ads"];
        Self {
            idle_amount: pair.map(|p| f(&format!("{p}idleamount"))),
            idle_speed: pair.map(|p| f(&format!("{p}idlespeed"))),
            idle_stance: [1., f("idlecrouchfactor"), f("idlepronefactor")],
            ads_bob_factor: f("adsbobfactor"),
            ads_view_bob: f("adsviewbobmult"),
            ads_aim_pitch: f("adsaimpitch"),
            rotation_threshold: stance.map(|p| f(&format!("{p}rotminspeed"))),
            rotation: stance.map(|p| ["p", "y", "r"].map(|axis| f(&format!("{p}rot{axis}")))),
            rotation_rate: [f("posrotrate"), f("pospronerotrate")],
            sway: ["", "ads"].map(|p| {
                ["maxangle", "lerpspeed", "pitchscale", "yawscale"]
                    .map(|k| f(&format!("{p}sway{k}")))
            }),
            spring: pair.map(|p| {
                ["accel", "speedmax", "speeddecay", "staticdecay"]
                    .map(|k| f(&format!("{p}gunkick{k}")))
            }),
            kick_limit: [f("gunmaxpitch"), f("gunmaxyaw")],
        }
    }
}
#[derive(Debug, Default)]
pub struct ViewMotion {
    pub gun_angles: [f32; 2],
    pub spread_fraction: f32,
    pub(super) damage_time: i32,
    pub(super) damage_angles: [f32; 2],
    previous_angles: [f32; 2],
    sway_angles: [f32; 2],
    rotation: Vec3,
    gun_kick: [f32; 2],
    idle_factor: f32,
    idle_phase: i32,
}

fn lerp(a: f32, b: f32, t: f32) -> f32 {
    (f64::from(a) + (f64::from(b) - f64::from(a)) * f64::from(t)) as f32
}
fn ease(t: f32) -> f32 {
    (2. - t.abs()) * t
}
fn angular_delta(value: f32, other: f32) -> f32 {
    let delta = (value - other) % 360.;
    if delta > 180. {
        delta - 360.
    } else if delta < -180. {
        delta + 360.
    } else {
        delta
    }
}
fn approach(current: f32, target: f32, step: f32) -> f32 {
    let dist = target - current;
    current + dist.signum() * step.min(dist.abs())
}
fn damage_envelope(elapsed: f32, duration: f32) -> f32 {
    let rise = duration * 100.;
    if elapsed < rise {
        ease(elapsed / rise)
    } else {
        let tail = (elapsed - rise) / (duration * 400.);
        if tail >= 1. { 0. } else { 1. - ease(tail) }
    }
}
fn sin(arg: f32) -> f32 {
    f64::from(arg).sin() as f32
}
fn bob_pair(phase: f32, amplitude: f32) -> f32 {
    (sin(phase + phase) + (f64::from(phase) * 4. + std::f64::consts::FRAC_PI_2).sin() as f32 * 0.2)
        * 0.75
        * amplitude
}

fn add_bob(
    out: &mut Vec3,
    phase: f32,
    speed: f32,
    stance_scale: f32,
    fraction: f32,
    bob_factor: f32,
    view_blend: f32,
) {
    let phase = (f64::from(phase) + std::f64::consts::FRAC_PI_4 + std::f64::consts::TAU) as f32;
    let amplitude = speed * 0.16;
    let amp = (amplitude * stance_scale).min(10.);
    let roll_phase = (f64::from(phase) - 0.4712389167638204) as f32;
    let angles = [
        -bob_pair(phase, amp),
        -sin(phase) * amp,
        (sin(roll_phase) * (amplitude * 1.5 * stance_scale).min(10.)).min(0.),
    ];
    let blend = (1. - (1. - bob_factor) * fraction) * view_blend;
    for (v, angle) in out.iter_mut().zip(angles) {
        *v += angle * blend;
    }
}

fn add_idle(out: &mut Vec3, phase: i32, amount: f32, scoped: bool) {
    let rates = [0.001f32, 0.0007, 0.0005];
    for i in 0..if scoped { 2 } else { 3 } {
        let wave = (f64::from(phase) * f64::from(rates[i])).sin() as f32;
        out[i] =
            (f64::from(out[i]) + f64::from(amount) * f64::from(wave) * f64::from(0.01f32)) as f32;
    }
}
fn spring_step(
    position: &mut f32,
    velocity: &mut f32,
    dt: f32,
    limit: f32,
    [accel, max_speed, friction, decel]: [f32; 4],
) -> bool {
    if position.abs() < 0.25 && velocity.abs() < 1. {
        *position = 0.;
        *velocity = 0.;
        return true;
    }
    let candidate = (f64::from(*position) + f64::from(*velocity) * f64::from(dt)) as f32;
    *position = candidate.min(limit).max(-limit);
    if candidate != *position && velocity.signum() == candidate.signum() {
        *velocity = 0.;
    }
    if *position != 0. {
        *velocity -= position.signum() * accel * dt;
    }
    *velocity =
        (f64::from(*velocity) - f64::from(*velocity) * f64::from(friction) * f64::from(dt)) as f32;
    *velocity = approach(*velocity, 0., decel * dt)
        .min(max_speed)
        .max(-max_speed);
    false
}
fn combine_angles(offset: Vec3, view: Vec3) -> [f32; 2] {
    let component = axes(offset)[0];
    let [forward, right, up] = axes(view);
    let dir = std::array::from_fn(|i| {
        (f64::from(component[0]) * f64::from(forward[i])
            - f64::from(component[1]) * f64::from(right[i])
            + f64::from(component[2]) * f64::from(up[i])) as f32
    });
    [direction_pitch(dir), yaw(dir)]
}
pub(super) fn direction_pitch(direction: Vec3) -> f32 {
    let horizontal = (f64::from(direction[0])
        .mul_add(f64::from(direction[0]), f64::from(direction[1]).powi(2))
        as f32)
        .sqrt();
    if horizontal == 0. {
        if direction[2] > 0. { 270. } else { 90. }
    } else {
        (-f64::from(direction[2])
            .atan2(f64::from(horizontal))
            .to_degrees() as f32)
            .rem_euclid(360.)
    }
}
pub(super) fn move_speed(ps: &PlayerState, time: i32) -> f32 {
    if ps.words()[3] & 0x20 == 0 {
        float(ps, "velocity[0]").hypot(float(ps, "velocity[1]"))
    } else if time.wrapping_sub(ps.field("jumpTime").expect("registered network field") as i32)
        < 500
    {
        0.
    } else {
        float(ps, "velocity[2]")
    }
}
pub(super) fn bob_scale(ps: &PlayerState, config: &ServerConfig) -> f32 {
    let (key, default) = match ps.field("viewHeightTarget") {
        Some(11) => ("bg_bobAmplitudeProne", 0.03),
        Some(40) => ("bg_bobAmplitudeDucked", 0.0075),
        _ => ("bg_bobAmplitudeStanding", 0.007),
    };
    config
        .get(key)
        .and_then(|v| v.parse().ok())
        .unwrap_or(default)
}
pub(crate) fn update(
    p: &mut Player,
    weapons: &WeaponRegistry,
    config: &ServerConfig,
    now: i32,
    command_time: i32,
) {
    p.view.advance(
        &p.state,
        &mut p.input.view_kick_angles,
        weapons,
        config,
        now,
        command_time,
    );
}
impl ViewMotion {
    pub fn advance(
        &mut self,
        ps: &PlayerState,
        kick_velocity: &mut Vec3,
        weapons: &WeaponRegistry,
        config: &ServerConfig,
        now: i32,
        command_time: i32,
    ) {
        let base = ["viewangles[0]", "viewangles[1]", "viewangles[2]"].map(|k| float(ps, k));
        let ms = command_time.wrapping_sub(ps.words()[0] as i32).min(200);
        let dt = (f64::from(ms) * 0.001) as f32;
        let blend = float(ps, "fWeaponPosFrac");
        let speed = move_speed(ps, now);
        let bob = bob_scale(ps, config);
        let motion = self;
        motion.gun_angles = [base[0], base[1]];
        let Some(def) = weapons.get(inventory::current(ps, weapons)) else {
            return;
        };
        let params = &def.view;
        let current_ads = def.handling.aim_down_sight;
        let scoped = def.overlay_reticle != 0;
        let flags = ps.words()[0xa0 / 4];
        let stance = if flags & 8 != 0 {
            2
        } else {
            usize::from(flags & 4 != 0)
        };
        let phase = ((1. + f64::from(ps.words()[2] as u8) / 255.) * std::f64::consts::TAU) as f32;
        let factor = params.idle_stance[stance];
        let (idle_amount, idle_speed) = if current_ads {
            (
                lerp(params.idle_amount[0], params.idle_amount[1], blend),
                lerp(params.idle_speed[0], params.idle_speed[1], blend),
            )
        } else if params.idle_amount[0] == 0. {
            (80., 1.)
        } else {
            (params.idle_amount[0], params.idle_speed[0])
        };
        let elapsed = now.wrapping_sub(motion.damage_time) as f32;
        let mut base_motion = [0.; 3];
        if motion.damage_time != 0 {
            let mut kick = 1. - blend * 0.5;
            if scoped {
                kick *= blend * 0.5 + 1.;
            }
            kick *= damage_envelope(elapsed, 1.);
            base_motion[0] = kick * motion.damage_angles[0];
            base_motion[2] = kick * motion.damage_angles[1];
        }
        if scoped {
            let breath = f32::from_bits(ps.words()[0x5b4 / 4]);
            let idle = if blend != 0. {
                approach(motion.idle_factor, factor, dt * 0.5)
            } else {
                motion.idle_factor
            };
            motion.idle_phase = motion.idle_phase.wrapping_add(
                (f64::from(breath) * f64::from(dt) * 1000. * f64::from(idle_speed)) as i32,
            );
            add_idle(
                &mut base_motion,
                motion.idle_phase,
                idle_amount * idle * blend * breath,
                true,
            );
            add_bob(
                &mut base_motion,
                phase,
                speed,
                bob,
                blend,
                params.ads_bob_factor,
                blend,
            );
        }
        if flags & 0x300 == 0 && blend != 0. && params.ads_view_bob != 0. {
            let amplitude = (speed * bob).min(45.);
            base_motion[0] -= bob_pair(phase, amplitude) * blend * params.ads_view_bob;
            base_motion[1] -= sin(phase) * amplitude * blend * params.ads_view_bob;
        }
        let view = std::array::from_fn(|i| base[i] + base_motion[i]);
        // BG updates angle/rotation/idle/gun springs even while their contribution is hidden.
        if ms != 0 && !(current_ads && blend > 0. && scoped) {
            let t = if current_ads { blend } else { 0. };
            let sway: [f32; 4] =
                std::array::from_fn(|i| lerp(params.sway[0][i], params.sway[1][i], t));
            let slew_dt = ms as f32 * 0.001;
            for i in 0..2 {
                let angle = angular_delta(base[i], motion.previous_angles[i])
                    * (1. / (f64::from(slew_dt) * 60.)) as f32;
                let target = angle.min(sway[0]).max(-sway[0]) * sway[i + 2];
                let current = motion.sway_angles[i];
                let diff = angular_delta(target, current);
                let step = (f64::from(diff) * f64::from(sway[1]) * f64::from(slew_dt)) as f32;
                let next = current
                    + if diff.abs() <= 0.001 || step.abs() > diff.abs() {
                        diff
                    } else {
                        step
                    };
                let quantized = (f64::from(next) * f64::from(182.04445f32)) as i32 as u16;
                motion.sway_angles[i] = angular_delta(f32::from(quantized) * (360. / 65536.), 0.);
                motion.previous_angles[i] = base[i];
            }
        }
        let mut offset = [
            if current_ads {
                blend * params.ads_aim_pitch
            } else {
                0.
            },
            0.,
            -2. * ease(float(ps, "leanf")),
        ];
        let threshold = params.rotation_threshold[stance];
        let varying = speed > threshold && ps.words()[0xd8 / 4] != 5;
        let moving = if varying {
            ((speed - threshold) / (ps.words()[0x50 / 4] as i32 as f32 - threshold)).clamp(0., 1.)
        } else {
            0.
        };
        let rotation_rate =
            params.rotation_rate[usize::from(float(ps, "viewHeightCurrent") == 11.)];
        for (i, angle) in offset.iter_mut().enumerate() {
            let target = params.rotation[stance][i] * moving * (1. - blend);
            let current = motion.rotation[i];
            motion.rotation[i] = approach(
                current,
                target,
                ((target - current).abs() * dt * rotation_rate).max(dt * 0.1),
            );
            if blend < 0.5 {
                *angle += motion.rotation[i] * (1. - 2. * blend);
            }
        }
        if !scoped || blend == 0. {
            motion.idle_factor = approach(motion.idle_factor, factor, dt * 0.5);
        }
        motion.idle_phase = motion
            .idle_phase
            .wrapping_add((f64::from(dt) * 1000. * f64::from(idle_speed)) as i32);
        add_idle(
            &mut offset,
            motion.idle_phase,
            idle_amount * motion.idle_factor * if scoped { 1. - blend } else { 1. },
            false,
        );
        add_bob(
            &mut offset,
            phase,
            speed,
            bob,
            blend,
            params.ads_bob_factor,
            if scoped { 1. - blend } else { 1. },
        );
        if motion.damage_time != 0 {
            let duration = blend * 0.5 + 0.5;
            let kick = damage_envelope(elapsed, duration)
                * duration
                * if scoped { 1. - blend * 0.75 } else { 1. };
            offset[0] += kick * motion.damage_angles[0] * 0.5;
            offset[1] -= kick * motion.damage_angles[1];
            offset[2] += kick * motion.damage_angles[1] * 0.5;
        }
        if current_ads {
            let spring =
                std::array::from_fn(|i| lerp(params.spring[0][i], params.spring[1][i], blend));
            let mut remaining = dt;
            while remaining > 0. {
                let step = remaining.min(0.005);
                let mut at_rest = true;
                for (i, velocity) in kick_velocity.iter_mut().enumerate().take(2) {
                    at_rest &= spring_step(
                        &mut motion.gun_kick[i],
                        velocity,
                        step,
                        params.kick_limit[i],
                        spring,
                    );
                }
                if at_rest {
                    break;
                }
                remaining = (f64::from(remaining) - 0.005) as f32;
            }
            offset[0] += motion.gun_kick[0];
            offset[1] += motion.gun_kick[1];
        }
        for (i, angle) in offset.iter_mut().enumerate().take(2) {
            *angle = angular_delta(*angle, motion.sway_angles[i]);
        }
        let weapon = weapons.get(ps.words()[0xd4 / 4] as usize);
        motion.gun_angles = if blend != 0.
            && weapon.is_some_and(|w: &WeaponDefinition| {
                w.handling.aim_down_sight && w.overlay_reticle == 0
            }) {
            combine_angles(offset, view)
        } else {
            [view[0], view[1]]
        };
    }
}
