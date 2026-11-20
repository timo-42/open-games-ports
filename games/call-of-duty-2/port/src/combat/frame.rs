//! PM_Weapon_Frame and its Pmove timing and predictable-event transitions.
use super::{WeaponHandling, inventory::*};
use crate::{
    assets::weapons::{WeaponDefinition, WeaponRegistry},
    config::ServerConfig,
    snapshot::PlayerState,
    usercmd::UserCmd,
};
use std::io;
mod aim;
mod equipment;
mod reload;

const FLAGS: usize = 0x0c / 4;
const TIME: usize = 0x34 / 4;
const DELAY: usize = 0x38 / 4;
const FUSE: usize = 0x3c / 4;
const KICK: usize = 0x40 / 4;
const STATE: usize = 0xd8 / 4;
const FRACTION: usize = 0xdc / 4;
const ANIM: usize = 0x5d0 / 4;
const SPREAD: usize = 0x5d4 / 4;
const BOLT_BITS: usize = 0x55c / 4;
#[derive(Clone, Copy, Debug)]
pub struct WeaponSettings {
    pub spread_move_speed: f32,
    pub mantle_enabled: bool,
    pub toggle_binoculars: bool,
    pub exit_scope_on_damage: bool,
    pub ads_exit_delay: i32,
    pub breath_hold_ms: i32,
    pub breath_gasp_ms: i32,
    pub breath_fire_delay: i32,
    pub breath_gasp_scale: f32,
    pub breath_hold_lerp: f32,
    pub breath_gasp_lerp: f32,
}
impl Default for WeaponSettings {
    fn default() -> Self {
        Self {
            spread_move_speed: 11.,
            mantle_enabled: true,
            toggle_binoculars: true,
            exit_scope_on_damage: false,
            ads_exit_delay: 0,
            breath_hold_ms: 4500,
            breath_gasp_ms: 1000,
            breath_fire_delay: 0,
            breath_gasp_scale: 4.5,
            breath_hold_lerp: 4.,
            breath_gasp_lerp: 6.,
        }
    }
}
impl WeaponSettings {
    pub fn from_config(config: &ServerConfig) -> Self {
        let get = |name, default| {
            config
                .get(name)
                .and_then(|v| v.parse::<f32>().ok())
                .unwrap_or(default)
        };
        Self {
            spread_move_speed: get("bg_aimspreadmovespeedthreshold", 11.),
            mantle_enabled: get("mantle_enable", 1.) != 0.,
            toggle_binoculars: get("player_togglebinoculars", 1.) != 0.,
            exit_scope_on_damage: get("player_scopeexitondamage", 0.) != 0.,
            ads_exit_delay: get("player_adsexitdelay", 0.) as i32,
            breath_hold_ms: (get("player_breath_hold_time", 4.5) * 1000.) as i32,
            breath_gasp_ms: (get("player_breath_gasp_time", 1.) * 1000.) as i32,
            breath_fire_delay: (get("player_breath_fire_delay", 0.) * 1000.) as i32,
            breath_gasp_scale: get("player_breath_gasp_scale", 4.5),
            breath_hold_lerp: get("player_breath_hold_lerp", 4.),
            breath_gasp_lerp: get("player_breath_gasp_lerp", 6.),
        }
    }
}

fn set_weapon_anim(ps: &mut PlayerState, anim: i32) {
    if (ps.words()[1] as i32) < 6 {
        ps.words_mut()[ANIM] = (ps.words()[ANIM] ^ 0x200) & 0x200 | anim as u32;
    }
}

/// PM_Weapon_ForceReady. Pmove also calls this on every mounted turret step.
pub fn force_ready(ps: &mut PlayerState) {
    let words = ps.words_mut();
    words[FLAGS] &= !0x810;
    if (17..=22).contains(&words[STATE]) {
        words[FRACTION] = 0;
        words[FLAGS] &= !0x20040;
        ps.add_event(0x95, 0);
        ps.add_event(0xa9, 0);
    }
    let words = ps.words_mut();
    words[TIME] = 0;
    words[DELAY] = 0;
    words[STATE] = 0;
    set_weapon_anim(ps, 0);
}

type AnimCallback<'a> = dyn FnMut(&mut PlayerState, i32, bool, bool) -> io::Result<()> + 'a;
struct Frame<'a> {
    animate: &'a mut AnimCallback<'a>,
    ps: &'a mut PlayerState,
    registry: &'a WeaponRegistry,
    cmd: &'a UserCmd,
    old: &'a UserCmd,
    settings: &'a WeaponSettings,
    ms: i32,
}
impl<'a> Frame<'a> {
    fn word(&self, key: usize) -> i32 {
        self.ps.words()[key] as i32
    }
    fn set(&mut self, key: usize, v: i32) {
        self.ps.words_mut()[key] = v as u32;
    }
    fn float(&self, key: usize) -> f32 {
        f32::from_bits(self.ps.words()[key])
    }
    fn set_float(&mut self, key: usize, v: f32) {
        self.ps.words_mut()[key] = v.to_bits();
    }
    fn set_bits(&mut self, key: usize, bits: i32, on: bool) {
        let v = self.word(key);
        self.set(key, if on { v | bits } else { v & !bits });
    }
    fn weapon(&self) -> usize {
        self.word(WEAPON) as usize
    }
    fn def(&self) -> &'a WeaponDefinition {
        self.registry
            .get(self.weapon())
            .expect("verified player weapon")
    }
    fn props(&self) -> &'a WeaponHandling {
        &self.def().handling
    }
    fn binoculars(&self) -> Option<(usize, &'a WeaponDefinition)> {
        registered(self.registry).find(|(_, d)| d.weapon_type == 3)
    }
    fn event_param(&mut self, code: i32, param: i32) {
        self.ps.add_event(code, param);
    }
    fn event(&mut self, code: i32) {
        self.event_param(code, 0);
    }
    fn anim(&mut self, anim: i32) {
        set_weapon_anim(self.ps, anim);
    }
    fn idle_anim(&mut self) {
        if self.word(ANIM) & !0x200 != 0 {
            self.anim(0);
        }
    }
    fn prone_flag(&mut self) {
        if self.word(FLAGS) & 1 != 0 {
            self.set_bits(FLAGS, 0x800, true);
        }
    }
    fn pending_bolt(&self) -> bool {
        self.ps.words()[BOLT_BITS + self.weapon() / 32] & (1 << (self.weapon() % 32)) != 0
    }
    fn bolt(&mut self, pending: bool) {
        let id = self.weapon();
        self.set_bits(BOLT_BITS + id / 32, 1i32 << (id % 32), pending);
    }
    fn timer_step(&mut self) -> bool {
        let state = self.word(STATE);
        if self.word(KICK) > 0 {
            self.set(KICK, (self.word(KICK) - self.ms).max(0));
        }
        if self.word(TIME) != 0 {
            let remaining = self.word(TIME) - self.ms;
            if remaining <= 0
                && !(12..=16).contains(&state)
                && self.props().semi_auto
                && self.cmd.buttons & 1 != 0
                && self.weapon() == self.cmd.weapon as usize
                && self.word(clip(self.def())) != 0
            {
                self.set(TIME, i32::from(!(5..=9).contains(&state)));
                if matches!(state, 3 | 4 | 10 | 11) {
                    self.idle_anim();
                    self.set(STATE, 0);
                }
            } else {
                self.set(TIME, remaining.max(0));
            }
        }
        let delay = self.word(DELAY);
        self.set(DELAY, (delay - self.ms).max(0));
        delay != 0 && delay <= self.ms
    }
    fn must_lower(&self) -> bool {
        let flags = self.word(FLAGS);
        flags & 0x04000020 != 0
            || (self.settings.mantle_enabled
                && flags & 4 != 0
                && (0..5).contains(&self.word(0x5c4 / 4)))
    }
    fn select_slot(&mut self) -> io::Result<()> {
        let state = self.word(STATE);
        if (12..=22).contains(&state)
            || (self.word(TIME) != 0
                && !(4..=9).contains(&state)
                && (matches!(state, 3 | 10 | 11) || self.word(DELAY) != 0))
        {
            return Ok(());
        }
        let id = self.weapon();
        let flags = self.word(FLAGS);
        let requested = self.cmd.weapon as usize;
        let choice = if self.must_lower() {
            0
        } else if id == requested
            || (flags & 0x8000 != 0 && id != 0)
            || (requested != 0
                && !self
                    .registry
                    .get(requested)
                    .is_some_and(|d| selectable(self.ps, requested, d)))
        {
            if id == 0 || owned(self.ps, id) {
                return Ok(());
            }
            0
        } else {
            requested
        };
        if state == 2 || choice == id {
            return Ok(());
        }
        self.set(DELAY, 0);
        let timed = id != 0 && owned(self.ps, id) && self.word(FUSE) <= 0;
        self.set(FUSE, 0);
        self.set(STATE, 2);
        self.prone_flag();
        if !timed {
            self.set(TIME, 0);
            return Ok(());
        }
        let def = self.def();
        if choice != 0 && choice == def.alt_weapon_index {
            self.event(0x9c);
            self.anim(15);
            self.set(TIME, def.handling.alt_drop_time);
            if let Some(slot) = super::inventory::find_slot(self.ps, self.registry, id) {
                if choice != held(self.ps, slot) && choice != id {
                    self.set(WEAPON, 0);
                }
                set_slot(self.ps, slot, choice);
            }
        } else {
            if !def.handling.clip_only || self.word(clip(def)) != 0 {
                self.event(0x9b);
                self.anim(9);
            }
            if self.word(FLAGS) & 4 == 0 {
                (self.animate)(self.ps, 6, false, false)?;
            }
            self.set(TIME, def.handling.drop_time);
        }
        Ok(())
    }
    fn raise(&mut self) -> io::Result<()> {
        let previous = self.weapon();
        let requested = self.cmd.weapon as usize;
        let id = if !self.must_lower()
            && owned(self.ps, requested)
            && self.registry.get(requested).is_some()
        {
            requested
        } else {
            0
        };
        let alt = previous != 0 && id != 0 && id == self.def().alt_weapon_index;
        self.set(WEAPON, id as i32);
        if previous == id {
            self.set(STATE, 0);
            self.anim(0);
            return Ok(());
        }
        self.set(STATE, 1);
        self.prone_flag();
        if alt {
            self.set(TIME, self.props().alt_raise_time);
            self.set_float(SPREAD, self.float(SPREAD).max(128.));
            self.anim(16);
        } else {
            if previous != 0 {
                self.event(0x9a);
                (self.animate)(self.ps, 7, false, false)?;
            }
            self.set(TIME, self.props().raise_time);
            self.set_float(SPREAD, 255.);
            self.anim(10);
        }
        Ok(())
    }
    fn fire(&mut self, delay_elapsed: bool) -> io::Result<()> {
        let def = self.def();
        let h = &def.handling;
        if self.cmd.buttons & 1 == 0 && !delay_elapsed {
            if self.word(STATE) == 3 {
                self.idle_anim();
            }
            self.set(STATE, 0);
            return Ok(());
        }
        if def.weapon_type == 1 {
            if !delay_elapsed {
                if self.word(clip(def)) != 0 {
                    self.set(FUSE, h.fuse_time);
                    self.anim(19);
                    self.event(0x9d);
                }
                self.set(DELAY, h.hold_fire_time);
                self.set(TIME, 0);
            }
        } else {
            self.set(
                DELAY,
                if h.ads_fire {
                    ((1.0 / f64::from(self.ads_in(def))) * (1.0 - f64::from(self.float(FRACTION))))
                        as i32
                } else {
                    h.fire_delay
                },
            );
            self.set(TIME, h.fire_time);
            (self.animate)(self.ps, 2, false, true)?;
            if h.bolt_action {
                self.bolt(true);
            }
            if self.word(STATE) != 3 {
                let reduced = if self.float(FRACTION) >= 1. {
                    h.ads_gun_kick_reduced_kick_bullets
                } else {
                    h.hip_gun_kick_reduced_kick_bullets
                };
                self.set(
                    KICK,
                    h.fire_delay.wrapping_add(h.fire_time.wrapping_mul(reduced)),
                );
            }
        }
        self.set(STATE, 3);
        self.prone_flag();
        if self.word(clip(def)) < 1 {
            if self.word(ammo(def)) < 1 {
                if def.weapon_type != 1 {
                    self.event(0x92);
                }
                self.bolt(false);
                self.idle_anim();
                self.set(TIME, self.word(TIME) + 500);
            } else {
                self.begin_reload()?;
            }
            return Ok(());
        }
        if self.word(DELAY) != 0 {
            return Ok(());
        }
        self.set(clip(def), self.word(clip(def)) - 1);
        if def.weapon_type == 1 {
            self.set(TIME, h.fire_time);
        }
        let empty = self.word(clip(def)) == 0;
        self.anim(if self.float(FRACTION) > 0.75 { 5 } else { 2 } + i32::from(empty));
        self.event(if empty { 0xa0 } else { 0x9e });
        self.breath_fire();
        if self.float(FRACTION) != 1. {
            self.set_float(
                SPREAD,
                (f64::from(self.float(SPREAD)) + f64::from(h.hip_spread_fire_add) * 255.).min(255.)
                    as f32,
            );
        }
        if h.clip_only && empty && self.word(ammo(def)) == 0 {
            let id = self.weapon();
            take(self.ps, self.registry, id);
            self.event(0x92);
        }
        Ok(())
    }
    fn run(&mut self) -> io::Result<()> {
        if self.word(FLAGS) & 0x1000 != 0 {
            return Ok(());
        }
        if self.word(1) >= 6 {
            self.set(WEAPON, 0);
            return Ok(());
        }
        if self.word(0xa0 / 4) & 0x300 != 0 {
            return Ok(());
        }
        self.aim();
        if self.cookoff() {
            return Ok(());
        }
        let expired = self.timer_step();
        self.binocular_input();
        self.offhand_input()?;
        self.select_slot()?;
        self.reload_input()?;
        self.melee_input(expired)?;
        if self.rechamber(expired) {
            return Ok(());
        }
        if (self.word(FLAGS) & 1 != 0 && (self.cmd.forward_move != 0 || self.cmd.right_move != 0))
            || matches!(self.word(STATE), 10 | 11)
        {
            self.set_float(SPREAD, 255.);
        }
        if expired || self.word(TIME) == 0 && self.word(DELAY) == 0 {
            match self.word(STATE) {
                1 | 9 => {
                    self.set(STATE, 0);
                    self.anim(0);
                }
                2 => self.raise()?,
                5..=8 => self.reload_step(expired),
                10 => self.melee_strike(),
                11 => {
                    self.idle_anim();
                    self.set(STATE, 0);
                }
                12..=22 => self.equipment_step()?,
                _ => {
                    if self.weapon() != 0 {
                        self.fire(expired)?;
                    }
                }
            }
        }
        Ok(())
    }
}
/// Flight, spectator and intermission run PM_UpdateWeaponPosFrac without advancing the weapon timers.
pub fn update_view_fraction(
    ps: &mut PlayerState,
    registry: &WeaponRegistry,
    cmd: &UserCmd,
    ms: i32,
    settings: &WeaponSettings,
) {
    Frame {
        ps,
        registry,
        cmd,
        old: cmd,
        ms,
        settings,
        animate: &mut |_, _, _, _| Ok(()),
    }
    .view_fraction();
}
pub fn step_configured(
    ps: &mut PlayerState,
    registry: &WeaponRegistry,
    cmd: &UserCmd,
    old: &UserCmd,
    ms: i32,
    settings: &WeaponSettings,
) -> io::Result<()> {
    step_with_animations(ps, registry, cmd, old, ms, settings, |_, _, _, _| Ok(()))
}
pub fn step_with_animations(
    ps: &mut PlayerState,
    registry: &WeaponRegistry,
    cmd: &UserCmd,
    old: &UserCmd,
    ms: i32,
    settings: &WeaponSettings,
    mut animate: impl FnMut(&mut PlayerState, i32, bool, bool) -> io::Result<()>,
) -> io::Result<()> {
    if registry.get(ps.words()[WEAPON] as usize).is_none()
        || registry.get(ps.words()[OFFHAND] as usize).is_none()
        || ps.words()[WEAPON] >= 128
        || ms < 0
    {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "invalid player weapon state or command interval",
        ));
    }
    Frame {
        animate: &mut animate,
        ps,
        registry,
        cmd,
        old,
        settings,
        ms,
    }
    .run()
}
pub fn step(
    ps: &mut PlayerState,
    registry: &WeaponRegistry,
    cmd: &UserCmd,
    old: &UserCmd,
    ms: i32,
) -> io::Result<()> {
    step_configured(ps, registry, cmd, old, ms, &WeaponSettings::default())
}
