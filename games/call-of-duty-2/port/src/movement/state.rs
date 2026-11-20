use super::{MovementAnim, UserCmd, Vec3};
use super::{mantle, prone};
use crate::{config::ServerConfig, snapshot::PlayerState};

/// Movement fields in playerState_t, plus the previous command's button latch.
#[derive(Clone, Debug, PartialEq)]
pub struct MovementState {
    pub command_time: i32,
    pub bob_cycle: u8,
    pub ladder_normal: Vec3,
    pub(super) animations: Vec<MovementAnim>,
    pub(super) touched: Vec<u16>,
    pub(super) events: Vec<(u8, u8)>,
    pub pm_type: i32,
    pub ads: crate::combat::AdsInput,
    pub damage_timer: i32,
    pub dead_yaw: i32,
    pub pending_spread: f32,
    pub mantle: mantle::MantleState,
    pub prone: prone::ProneState,
    pub clamp_base: [f32; 2],
    pub clamp_range: [f32; 2],
    pub pm_flags: u32,
    pub pm_time: i32,
    pub foliage_sound_time: i32,
    pub origin: Vec3,
    pub velocity: Vec3,
    pub view_angles: Vec3,
    pub delta_angles: [i32; 3],
    pub old_velocity: [f32; 2],
    pub ground_entity: u16,
    pub entity_flags: u32,
    pub speed: f32,
    pub gravity: f32,
    pub lean: f32,
    pub movement_dir: i8,
    pub jump_time: i32,
    pub jump_origin_z: f32,
    pub last_buttons: u32,
    pub(super) previous_command: UserCmd,
    pub view_height: f32,
    pub view_height_target: i32,
    pub height_lerp_time: i32,
    pub height_lerp_target: i32,
    pub height_lerp_down: bool,
    /// pmove_t mins and maxs are the last stance hull used by ClientThink.
    pub entity_bounds: [Vec3; 2],
    pub mins: Vec3,
    pub maxs: Vec3,
}
impl Default for MovementState {
    fn default() -> Self {
        Self {
            ground_entity: 1023,
            speed: 190.,
            gravity: 800.,
            mins: [-15., -15., 0.],
            maxs: [15., 15., 70.],
            ..Self::from_player(&PlayerState::default())
        }
    }
}
impl MovementState {
    pub fn take_animations(&mut self) -> Vec<MovementAnim> {
        std::mem::take(&mut self.animations)
    }
    pub fn take_touched(&mut self) -> Vec<u16> {
        std::mem::take(&mut self.touched)
    }
    pub fn seed_previous_command(&mut self, cmd: UserCmd) {
        self.last_buttons = cmd.buttons;
        self.previous_command = cmd;
    }
    pub fn from_player(ps: &PlayerState) -> Self {
        let w = ps.words();
        let i = |off: usize| w[off / 4] as i32;
        let f = |off| f32::from_bits(i(off) as u32);
        let vec = |off: usize| std::array::from_fn(|j| f(off + j * 4));
        Self {
            command_time: i(0),
            entity_bounds: [[0.; 3]; 2],
            bob_cycle: i(8) as u8,
            events: Vec::new(),
            animations: Vec::new(),
            touched: Vec::new(),
            ladder_normal: vec(0x64),
            pm_type: i(4),
            ads: crate::combat::AdsInput {
                weapon_state: i(0xd8),
                ..Default::default()
            },
            damage_timer: i(0x90),
            dead_yaw: i(0x130),
            pending_spread: 0.,
            mantle: mantle::MantleState::from_player(ps),
            prone: prone::ProneState::from_player(ps),
            clamp_base: [f(0x10c), f(0x110)],
            clamp_range: [f(0x114), f(0x118)],
            pm_flags: w[3],
            pm_time: i(0x10),
            foliage_sound_time: i(0x44),
            origin: vec(0x14),
            velocity: vec(0x20),
            old_velocity: [f(0x2c), f(0x30)],
            gravity: i(0x48) as f32,
            lean: f(0x4c),
            speed: i(0x50) as f32,
            delta_angles: std::array::from_fn(|j| i(0x54 + j * 4)),
            ground_entity: i(0x60) as u16,
            jump_time: i(0x70),
            jump_origin_z: f(0x74),
            movement_dir: i(0x9c) as i8,
            entity_flags: w[0xa0 / 4],
            view_angles: vec(0xe8),
            view_height_target: i(0xf4),
            view_height: f(0xf8),
            height_lerp_time: i(0xfc),
            height_lerp_target: i(0x100),
            height_lerp_down: i(0x104) != 0,
            mins: vec(0x56c),
            maxs: vec(0x578),
            last_buttons: 0,
            previous_command: UserCmd::default(),
        }
    }

    pub fn write_player(&mut self, ps: &mut PlayerState) {
        self.mantle.write_player(ps);
        self.prone.write_player(ps);
        let w = ps.words_mut();
        if self.pending_spread != 0. {
            w[0x5d4 / 4] = (f32::from_bits(w[0x5d4 / 4]) + self.pending_spread)
                .min(255.)
                .to_bits();
            self.pending_spread = 0.;
        }
        for (event, param) in self.events.drain(..) {
            let ring = w[0xa4 / 4] as usize & 3;
            w[0xa8 / 4 + ring] = u32::from(event);
            w[0xb8 / 4 + ring] = u32::from(param);
            w[0xa4 / 4] = w[0xa4 / 4].wrapping_add(1);
        }
        for (off, value) in [
            (0, self.command_time),
            (8, i32::from(self.bob_cycle)),
            (4, self.pm_type),
            (0xc, self.pm_flags as i32),
            (0x10, self.pm_time),
            (0x90, self.damage_timer),
            (0x130, self.dead_yaw),
            (0x44, self.foliage_sound_time),
            (0x48, self.gravity as i32),
            (0x50, self.speed as i32),
            (0x60, i32::from(self.ground_entity)),
            (0x70, self.jump_time),
            (0x9c, i32::from(self.movement_dir)),
            (0xa0, self.entity_flags as i32),
            (0xf4, self.view_height_target),
            (0xfc, self.height_lerp_time),
            (0x100, self.height_lerp_target),
            (0x104, i32::from(self.height_lerp_down)),
        ] {
            w[off / 4] = value as u32;
        }
        for (off, value) in [
            (0x2c, self.old_velocity[0]),
            (0x30, self.old_velocity[1]),
            (0x4c, self.lean),
            (0x74, self.jump_origin_z),
            (0xf8, self.view_height),
        ] {
            w[off / 4] = value.to_bits();
        }
        for (off, value) in [
            (0x14, self.origin),
            (0x64, self.ladder_normal),
            (0x20, self.velocity),
            (0xe8, self.view_angles),
            (0x56c, self.mins),
            (0x578, self.maxs),
        ] {
            w[off / 4..off / 4 + 3].copy_from_slice(&value.map(f32::to_bits));
        }
        w[0x54 / 4..0x60 / 4].copy_from_slice(&self.delta_angles.map(|v| v as u32));
    }
}

#[derive(Clone, Debug)]
pub struct MovementSettings {
    pub mantle: mantle::Settings,
    pub friction: f32,
    pub ladder_yawcap: f32,
    pub ladder_push: f32,
    pub prone_yawcap: f32,
    pub jump_step: f32,
    pub stop_speed: f32,
    pub damage_max_time: f32,
    pub damage_min_scale: f32,
    pub jump_spread_add: f32,
    pub fall_min: f32,
    pub foliage_min: f32,
    pub foliage_max: f32,
    pub foliage_slow: i32,
    pub foliage_fast: i32,
    pub foliage_reset: i32,
    pub fall_max: f32,
    pub move_threshold: f32,
    pub footsteps_threshold: f32,
    pub inertia_max: f32,
    pub inertia_angle: f32,
    pub strafe_scale: f32,
    pub back_scale: f32,
    pub spectator_scale: f32,
    pub jump_height: f32,
    pub jump_slowdown: bool,
    pub pitch_up: f32,
    pub pitch_down: f32,
}
impl Default for MovementSettings {
    fn default() -> Self {
        Self {
            mantle: mantle::Settings::default(),
            friction: 5.5,
            ladder_yawcap: 100.,
            ladder_push: 128.,
            prone_yawcap: 85.,
            jump_step: 18.,
            stop_speed: 100.,
            damage_max_time: 750.,
            damage_min_scale: 0.,
            jump_spread_add: 64.,
            fall_min: 256.,
            foliage_min: 40.,
            foliage_max: 180.,
            foliage_slow: 1500,
            foliage_fast: 500,
            foliage_reset: 500,
            fall_max: 480.,
            move_threshold: 10.,
            footsteps_threshold: 0.,
            inertia_max: 50.,
            inertia_angle: 0.,
            strafe_scale: 0.8,
            back_scale: 0.7,
            spectator_scale: 2.,
            jump_height: 39.,
            jump_slowdown: true,
            pitch_up: 85.,
            pitch_down: 85.,
        }
    }
}
impl MovementSettings {
    pub fn from_config(c: &ServerConfig) -> Self {
        let mut s = Self::default();
        for (name, value, min, max) in [
            ("bg_fallDamageMinHeight", &mut s.fall_min, 1., f32::MAX),
            ("bg_fallDamageMaxHeight", &mut s.fall_max, 1., f32::MAX),
            ("player_moveThreshhold", &mut s.move_threshold, 1e-8, 20.),
            (
                "player_footstepsThreshhold",
                &mut s.footsteps_threshold,
                0.,
                50000.,
            ),
            ("bg_ladder_yawcap", &mut s.ladder_yawcap, 0., 360.),
            ("jump_ladderPushVel", &mut s.ladder_push, 0., 1024.),
            ("bg_prone_yawcap", &mut s.prone_yawcap, 0., 360.),
            ("jump_stepSize", &mut s.jump_step, 0., 64.),
            ("bg_foliagesnd_minspeed", &mut s.foliage_min, 0., f32::MAX),
            ("bg_foliagesnd_maxspeed", &mut s.foliage_max, 0., f32::MAX),
            (
                "player_dmgtimer_maxTime",
                &mut s.damage_max_time,
                0.,
                f32::MAX,
            ),
            ("player_dmgtimer_minScale", &mut s.damage_min_scale, 0., 1.),
            ("jump_spreadAdd", &mut s.jump_spread_add, 0., 512.),
            ("friction", &mut s.friction, 0., 100.),
            ("stopspeed", &mut s.stop_speed, 0., 1000.),
            ("inertiaMax", &mut s.inertia_max, 0., 1000.),
            ("inertiaAngle", &mut s.inertia_angle, -1., 1.),
            ("player_strafeSpeedScale", &mut s.strafe_scale, 0., 20.),
            ("player_backSpeedScale", &mut s.back_scale, 0., 20.),
            ("player_spectateSpeedScale", &mut s.spectator_scale, 0., 20.),
            ("jump_height", &mut s.jump_height, 0., 128.),
            ("player_view_pitch_up", &mut s.pitch_up, 0., 90.),
            ("player_view_pitch_down", &mut s.pitch_down, 0., 90.),
        ] {
            if let Some(parsed) = c
                .get(name)
                .and_then(|x| x.parse::<f32>().ok())
                .filter(|v| v.is_finite())
            {
                *value = parsed.clamp(min, max);
            }
        }
        s.mantle = mantle::Settings::from_config(c);
        for (name, int) in [
            ("bg_foliagesnd_slowinterval", &mut s.foliage_slow),
            ("bg_foliagesnd_fastinterval", &mut s.foliage_fast),
            ("bg_foliagesnd_resetinterval", &mut s.foliage_reset),
        ] {
            if let Some(v) = c.get(name).and_then(|x| x.parse::<i32>().ok()) {
                *int = v.max(0);
            }
        }
        s.jump_slowdown = c.get("jump_slowdownEnable").is_none_or(|s| s != "0");
        s
    }
}
