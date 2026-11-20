//! Third-person ledge probes and root-motion traversal (Mantle_FindSurface,
//! Mantle_CheckLedge, Mantle_Start and Mantle_UpdateMove, 0x080dd3dc..0x080de47a).
use super::{
    MovementSettings, MovementState, Trace, UserCmd, Vec3, diff, dot, length, scale, unit, vertical,
};
use crate::{
    animation::pose::Clip, assets::GameAssets, config::ServerConfig, snapshot::PlayerState,
};
use std::{io, sync::Arc};

// Original g_mantleStages @ 0x08184d00 and g_mantleAnimNames @ 0x08184d60.
// Up nodes are 1..=7, over nodes 8..=10. Ties select the first stage.
const HEIGHTS: [f32; 7] = [57., 51., 45., 39., 33., 27., 21.];
const OVER: [usize; 7] = [7, 7, 8, 8, 8, 9, 9];
const CLIPS: [&str; 10] = [
    "mp_mantle_up_57",
    "mp_mantle_up_51",
    "mp_mantle_up_45",
    "mp_mantle_up_39",
    "mp_mantle_up_33",
    "mp_mantle_up_27",
    "mp_mantle_up_21",
    "mp_mantle_over_high",
    "mp_mantle_over_mid",
    "player_mantle_over_low",
];

#[derive(Debug)]
struct Motion {
    clip: Clip,
    duration: i32,
}

#[derive(Debug)]
pub struct MantleAnimations {
    clips: Vec<Motion>,
}
impl MantleAnimations {
    pub fn load(assets: &GameAssets) -> io::Result<Self> {
        Self::from_loader(|name| assets.read_script(&format!("xanim/{name}")))
    }
    fn from_loader(mut load: impl FnMut(&str) -> io::Result<Vec<u8>>) -> io::Result<Self> {
        let mut clips = Vec::with_capacity(CLIPS.len());
        for (index, name) in CLIPS.iter().enumerate() {
            let clip = Clip::parse(&load(name)?)?;
            if clip.fps <= 0 || clip.frames == 0 {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!("invalid mantle animation duration for {name}"),
                ));
            }
            let duration = i32::from(clip.frames) * 1000 / i32::from(clip.fps);
            let expected = if let Some(&height) = HEIGHTS.get(index) {
                [16., 0., height]
            } else {
                [31., 0., -18.]
            };
            if duration == 0
                || clip
                    .end_translation()
                    .iter()
                    .zip(expected)
                    .any(|(&v, target)| !v.is_finite() || (v - target).abs() > 1.)
            {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!("invalid root motion for mantle animation {name}"),
                ));
            }
            clips.push(Motion { clip, duration });
        }
        Ok(Self { clips })
    }
    fn duration(&self, stage: usize, over: bool) -> i32 {
        self.clips[stage].duration
            + if over {
                self.clips[OVER[stage]].duration
            } else {
                0
            }
    }
    fn position(&self, stage: usize, time: i32, yaw: f32) -> Vec3 {
        let up = &self.clips[stage];
        let mut pos = if time > up.duration {
            let over = &self.clips[OVER[stage]];
            let second = over
                .clip
                .translation_at((time - up.duration) as f32 / over.duration as f32);
            let first = up.clip.end_translation();
            std::array::from_fn(|i| first[i] + second[i])
        } else {
            up.clip.translation_at(time as f32 / up.duration as f32)
        };
        let (sin, cos) = (f64::from(yaw).to_radians() as f32).sin_cos();
        let x = f64::from(pos[0]);
        let y = f64::from(pos[1]);
        pos[0] = (x * f64::from(cos) - y * f64::from(sin)) as f32;
        pos[1] = (x * f64::from(sin) + y * f64::from(cos)) as f32;
        pos
    }
}

#[derive(Clone, Debug)]
pub struct Settings {
    pub animations: Option<Arc<MantleAnimations>>,
    enabled: bool,
    debug: bool,
    range: f32,
    radius: f32,
    angle: f32,
    yaw_cap: f32,
}
impl Default for Settings {
    fn default() -> Self {
        Self {
            animations: None,
            enabled: true,
            debug: false,
            range: 20.,
            radius: 0.1,
            angle: 60.,
            yaw_cap: 60.,
        }
    }
}
impl Settings {
    pub fn from_config(config: &ServerConfig) -> Self {
        let mut out = Self::default();
        for (key, field, max) in [
            ("mantle_check_range", &mut out.range, 128.),
            ("mantle_check_radius", &mut out.radius, 15.),
            ("mantle_check_angle", &mut out.angle, 180.),
            ("mantle_view_yawcap", &mut out.yaw_cap, 180.),
        ] {
            if let Some(v) = config
                .get(key)
                .and_then(|s| s.parse::<f32>().ok())
                .filter(|f| f.is_finite())
            {
                *field = v.clamp(0., max);
            }
        }
        out.debug = config.get("mantle_debug").is_some_and(|s| s != "0");
        out.enabled = config.get("mantle_enable").is_none_or(|s| s != "0");
        out
    }
    fn report(&self, message: &str) {
        if self.debug {
            println!("{message}");
        }
    }
    fn clips(&self) -> io::Result<&MantleAnimations> {
        self.animations.as_deref().ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidData, "missing mantle animation tree")
        })
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct MantleState {
    pub yaw: f32,
    pub timer: i32,
    pub stage: i32,
    pub flags: u32,
    /// pmove_t publishes the endpoint to the server when a climb begins.
    pub started: Option<(Vec3, i32)>,
    weapon_state: i32,
}
impl MantleState {
    pub fn from_player(ps: &PlayerState) -> Self {
        let w = ps.words();
        Self {
            yaw: f32::from_bits(w[0x5bc / 4]),
            timer: w[0x5c0 / 4] as i32,
            stage: w[0x5c4 / 4] as i32,
            flags: w[0x5c8 / 4],
            weapon_state: w[0xd8 / 4] as i32,
            started: None,
        }
    }
    pub fn refresh_weapon_state(&mut self, ps: &PlayerState) {
        self.weapon_state = ps.words()[0xd8 / 4] as i32;
    }

    pub fn write_player(&self, ps: &mut PlayerState) {
        ps.words_mut()[0x5bc / 4..0x5cc / 4].copy_from_slice(&[
            self.yaw.to_bits(),
            self.timer as u32,
            self.stage as u32,
            self.flags,
        ]);
    }
}

fn nearest(height: f32) -> usize {
    (0..HEIGHTS.len())
        .min_by(|&a, &b| {
            (height - HEIGHTS[a])
                .abs()
                .total_cmp(&(height - HEIGHTS[b]).abs())
        })
        .expect("seven mantle heights")
}
fn along(start: Vec3, dist: f32, dir: Vec3) -> Vec3 {
    std::array::from_fn(|i| (f64::from(start[i]) + f64::from(dist) * f64::from(dir[i])) as f32)
}

pub(super) fn try_start(
    ps: &mut MovementState,
    cmd: &UserCmd,
    config: &MovementSettings,
    trace: &Trace<'_>,
    forward: Vec3,
    mask: u32,
) -> io::Result<()> {
    let s = &config.mantle;
    s.report("====== Mantle Debug ======");
    let failure = if !s.enabled {
        Some("Mantle Failed: Not enabled")
    } else if ps.pm_type >= 6 {
        Some("Mantle Failed: Player is dead")
    } else if ps.pm_flags & 4 != 0 {
        Some("Mantle Failed: Player already mantling")
    } else if ps.entity_flags & 12 != 0 {
        Some("Mantle Failed: Player not standing")
    } else if (17..=22).contains(&ps.mantle.weapon_state) {
        Some("Mantle Failed: Player using binoculars")
    } else {
        None
    };
    if let Some(message) = failure {
        s.report(message);
        return Ok(());
    }
    let facing = unit([forward[0], forward[1], 0.]);
    let inset = ps.maxs[0] - s.radius;
    let surface = trace(
        along(ps.origin, -inset, facing),
        along(ps.origin, s.range + inset, facing),
        [-s.radius, -s.radius, ps.mins[2]],
        [s.radius, s.radius, ps.maxs[2]],
        0x100_0000,
    );
    if surface.start_solid || surface.all_solid {
        s.report("Mantle Failed: Mantle brush is too thick");
        return Ok(());
    }
    if surface.fraction == 1. {
        s.report("Mantle Failed: No mantle surface found");
        return Ok(());
    }
    if surface.surface_flags & 0x600_0000 == 0 {
        s.report("Mantle Failed: No mantle surface with MANTLE_ON or MANTLE_OVER found");
        return Ok(());
    }
    let toward = [-surface.normal[0], -surface.normal[1], 0.];
    if length(toward) < 0.0001 {
        s.report("Mantle Failed: Mantle surface has vertical normal");
        return Ok(());
    }
    let dir = unit(toward);
    if f64::from(dot(dir, facing)).acos().to_degrees() > f64::from(s.angle) {
        s.report("Mantle Failed: Player is not facing mantle surface");
        return Ok(());
    }
    let r = ps.maxs[0];
    let mins = [-r, -r, 0.];
    let maxs = [r, r, r + r];
    for h in [60., 40., 20.] {
        if s.debug {
            println!("Checking for ledge at {h:.6} units");
        }
        let start = vertical(ps.origin, h);
        let above = along(start, 16., dir);
        let ahead = trace(start, above, mins, maxs, mask);
        if ahead.start_solid || ahead.fraction < 1. {
            s.report("Mantle Failed: Ledge is too far away");
            continue;
        }
        let mut bottom = above;
        bottom[2] = ps.origin[2] + 18.;
        let down = trace(above, bottom, mins, maxs, mask);
        if down.start_solid || down.fraction == 1. {
            s.report("Mantle Failed: Can't find ledge");
            continue;
        }
        if down.normal[2] < 0.7 {
            s.report("Mantle Failed: Ledge is too steep");
            continue;
        }
        let mut ledge = bottom;
        ledge[2] = (f64::from(bottom[2] - above[2]) * f64::from(down.fraction)
            + f64::from(above[2])) as f32;
        if trace(ledge, ledge, mins, maxs, mask).start_solid {
            s.report("Mantle Failed: Player can't fit crouched on ledge");
            continue;
        }
        s.report("Mantle available!");
        ps.mantle.flags |= 8;
        if cmd.buttons & 0x400 == 0 {
            return Ok(());
        }
        let mut flags = 8;
        let mut end = ledge;
        if surface.surface_flags & 0x400_0000 != 0 {
            let far = along(ledge, 31., dir);
            let drop = vertical(far, -18.);
            let mut crouched = ps.maxs;
            crouched[2] = 50.;
            let hit = trace(far, drop, ps.mins, crouched, mask);
            if !hit.start_solid && hit.fraction >= 1. {
                end = drop;
                flags |= 1;
            }
        }
        if ps.entity_flags & 4 == 0 {
            if trace(ledge, ledge, ps.mins, ps.maxs, mask).start_solid {
                flags |= 2;
            }
            if !trace(end, end, ps.mins, ps.maxs, mask).start_solid {
                flags |= 4;
            }
        }
        let clips = s.clips()?;
        let stage = nearest(ledge[2] - ps.origin[2]);
        let duration = clips.duration(stage, flags & 1 != 0);
        let yaw = (f64::from(dir[1]).atan2(f64::from(dir[0])).to_degrees() as f32).rem_euclid(360.);
        ps.mantle.yaw = yaw;
        ps.mantle.stage = stage as i32;
        ps.mantle.timer = 0;
        ps.mantle.flags = flags;
        ps.mantle.started = Some((end, duration));
        ps.origin = diff(end, clips.position(stage, duration, yaw));
        ps.pm_flags |= 4;
        ps.entity_flags |= 0x4000;
        break;
    }
    Ok(())
}

pub(super) fn update(
    ps: &mut MovementState,
    config: &MovementSettings,
    msec: i32,
) -> io::Result<()> {
    if !config.mantle.enabled {
        return Ok(());
    }
    let clips = config.mantle.clips()?;
    let stage = usize::try_from(ps.mantle.stage)
        .ok()
        .filter(|&i| i < HEIGHTS.len())
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "invalid mantle transition"))?;
    ps.mantle.flags &= !8;
    if ps.mantle.flags & 2 != 0 {
        ps.events.push((0x8d, 0));
    }
    let duration = clips.duration(stage, ps.mantle.flags & 1 != 0);
    let old = ps.mantle.timer;
    let now = old.saturating_add(msec).min(duration);
    ps.mantle.timer = now;
    let delta = diff(
        clips.position(stage, now, ps.mantle.yaw),
        clips.position(stage, old, ps.mantle.yaw),
    );
    let node = if now > clips.clips[stage].duration {
        OVER[stage]
    } else {
        stage
    };
    ps.animations
        .push(super::MovementAnim::MoveId((node + 21) as u8));
    ps.origin = std::array::from_fn(|i| ps.origin[i] + delta[i]);
    ps.velocity = scale(
        delta,
        (1. / (f64::from(now - old) * f64::from(0.001f32))) as f32,
    );
    if now == duration {
        ps.pm_flags &= !4;
        ps.mantle.started = None;
        if ps.mantle.flags & 1 != 0 {
            ps.animations
                .push(super::MovementAnim::Event(3, false, true));
        }
        if ps.mantle.flags & 4 != 0 {
            ps.events.push((0x8c, 0));
            ps.entity_flags &= !0x4000;
        }
    }
    Ok(())
}

pub(super) fn apply_yaw_cap(ps: &mut MovementState, config: &MovementSettings) {
    let s = &config.mantle;
    if !s.enabled || ps.pm_flags & 4 == 0 {
        return;
    }
    let shorts = (f64::from(ps.mantle.yaw - ps.view_angles[1]) * f64::from(182.04445_f32)) as i32;
    let mut delta = (shorts & 0xffff) as f32 * (360. / 65536.);
    if delta > 180. {
        delta -= 360.;
    }
    if delta.abs() <= s.yaw_cap {
        return;
    }
    // A zero cap requests a locked view. The original loop cannot advance in
    // that case; folding the full delta avoids hanging the server on this dvar.
    if s.yaw_cap > 0. {
        let old = delta;
        if delta.abs() / s.yaw_cap <= 4096. {
            while delta < -s.yaw_cap {
                delta += s.yaw_cap;
            }
            while delta > s.yaw_cap {
                delta -= s.yaw_cap;
            }
        } else {
            delta %= s.yaw_cap;
            if delta == 0. {
                delta = s.yaw_cap.copysign(old);
            }
        }
    }
    ps.delta_angles[1] = ps.delta_angles[1]
        .wrapping_add((f64::from(delta) * f64::from(182.04445_f32)) as i32 & 0xffff);
    ps.view_angles[1] =
        (ps.mantle.yaw + if delta > 0. { -s.yaw_cap } else { s.yaw_cap }).rem_euclid(360.);
}

/// G_SpawnMantleBlocker reserves the destination for this player until the
/// animation and server buffer have both elapsed. Owner exclusion allows the
/// same player to complete the climb.
pub fn spawn_blocker(
    host: &mut crate::vm::GameHost<'_>,
    owner: u16,
    end: Vec3,
    duration: i32,
) -> io::Result<()> {
    let player = host.world.entities.get(owner).ok_or_else(|| {
        io::Error::new(io::ErrorKind::InvalidInput, "missing mantle blocker owner")
    })?;
    let (mins, maxs) = (player.mins, player.maxs);
    let buffer = host
        .config
        .get("g_mantleBlockTimeBuffer")
        .map(crate::config::integer_prefix)
        .unwrap_or(500)
        .clamp(0, 60000);
    let (slot, _) = host.world.spawn_entity(b"noclass", end, host.time_ms)?;
    let blocker = host
        .world
        .entities
        .get_mut(slot)
        .expect("newly allocated mantle blocker");
    blocker.owner_num = owner;
    blocker.contents = 0x1_0000;
    blocker.sv_flags = 0x21;
    blocker.state.set_field("eType", 5);
    blocker.mins = mins;
    blocker.maxs = maxs;
    blocker.mantle_blocker_free_at = Some(host.time_ms.wrapping_add(duration).wrapping_add(buffer));
    crate::vm::entity_methods::relink(blocker, host.collision, false)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::collision::TraceResult;
    use std::cell::Cell;

    /// Two position keys and no rotation in a version 14 root-motion clip.
    fn clip(translation: Vec3) -> Vec<u8> {
        let mut bytes = vec![14, 0, 2, 0, 0, 0, 2, 10, 0, 0, 0, 2, 0];
        for value in [0., 0., 0.].into_iter().chain(translation) {
            bytes.extend_from_slice(&value.to_le_bytes());
        }
        bytes.push(0);
        bytes
    }
    fn settings() -> io::Result<MovementSettings> {
        let curves = MantleAnimations::from_loader(|name| {
            let id = CLIPS
                .iter()
                .position(|&n| n == name)
                .expect("known mantle animation");
            Ok(clip(if id < 7 {
                [16., 0., HEIGHTS[id]]
            } else {
                [31., 0., -18.]
            }))
        })?;
        let mut settings = MovementSettings::default();
        settings.mantle.animations = Some(Arc::new(curves));
        Ok(settings)
    }

    #[test]
    fn successful_over_preserves_probe_order_and_tracks_two_clips() -> io::Result<()> {
        let settings = settings()?;
        let steps = Cell::new(0);
        let trace = |start: Vec3, end: Vec3, mins: Vec3, maxs: Vec3, mask: u32| {
            let i = steps.replace(steps.get() + 1);
            let mut result = TraceResult::default();
            match i {
                0 => {
                    assert_eq!(mask, 0x100_0000);
                    assert_eq!((mins, maxs), ([-0.1, -0.1, 0.], [0.1, 0.1, 70.]));
                    result.fraction = 0.5;
                    result.normal = [-1., 0., 0.];
                    result.surface_flags = 0x400_0000;
                }
                1 => {
                    assert_eq!((start, end), ([0., 0., 60.], [16., 0., 60.]));
                    assert_eq!(maxs, [15., 15., 30.]);
                }
                2 => {
                    assert_eq!((start[2], end[2]), (60., 18.));
                    result.fraction = 0.5;
                    result.normal = [0., 0., 1.];
                }
                3 => assert_eq!((start, end), ([16., 0., 39.], [16., 0., 39.])),
                4 => {
                    assert_eq!(end, [47., 0., 21.]);
                    assert_eq!(maxs[2], 50.);
                }
                5 | 6 => assert_eq!(start, end),
                _ => panic!("extra trace"),
            }
            result
        };
        let cmd = UserCmd {
            buttons: 0x400,
            ..Default::default()
        };
        let mut ps = MovementState::default();
        try_start(&mut ps, &cmd, &settings, &trace, [1., 0., 0.], 0x2810011)?;
        assert_eq!(steps.get(), 7);
        assert_eq!(ps.mantle.stage, 3);
        assert_eq!(ps.mantle.started, Some(([47., 0., 21.], 200)));
        assert_eq!(ps.origin, [0., 0., 0.]);
        update(&mut ps, &settings, 100)?;
        assert_eq!(ps.origin, [16., 0., 39.]);
        update(&mut ps, &settings, 100)?;
        assert_eq!(ps.origin, [47., 0., 21.]);
        assert_eq!(ps.pm_flags & 4, 0);
        assert_eq!(ps.entity_flags & 0x4000, 0);
        assert_eq!(ps.events, [(0x8c, 0)]);
        Ok(())
    }
    #[test]
    fn stage_ties_choose_taller_and_missing_or_invalid_assets_fail() {
        assert_eq!(nearest(54.), 0);
        assert_eq!(nearest(53.99), 1);
        assert!(MantleAnimations::from_loader(|_| Ok(clip([30., 0., 8.]))).is_err());
    }
    #[test]
    fn zero_yaw_cap_locks_look_without_looping() {
        let mut ps = MovementState {
            pm_flags: 4,
            view_angles: [0., 78., 0.],
            ..Default::default()
        };
        let mut settings = MovementSettings::default();
        settings.mantle.yaw_cap = 0.;
        apply_yaw_cap(&mut ps, &settings);
        assert_eq!(ps.view_angles[1], ps.mantle.yaw);
    }
}
