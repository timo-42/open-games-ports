//! Multiplayer animation rules, playerstate timers and animation restart bits.
pub mod controllers;
mod parser;
pub mod pose;
pub(crate) mod script;
use crate::{assets::weapons::WeaponRegistry, snapshot::PlayerState, usercmd::UserCmd};
use parser::Script;
use std::{collections::BTreeMap, io, sync::Arc};

fn invalid(text: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, text.into())
}

#[derive(Debug, Default, Clone, Copy)]
pub struct Animation {
    pub index: u16,
    pub duration: i32,
    pub looping: bool,
    pub(crate) synchronized_loop: Option<bool>,
    pub flags: u32,
    pub blend_time: i32,
    pub move_types: u64,
    pub move_speed: f32,
}
#[derive(Debug, Clone)]
struct Part {
    body: u8,
    clip: Animation,
    duration: i16,
}
#[derive(Debug, Clone)]
struct Command {
    parts: Vec<Part>,
    sound: Option<Arc<str>>,
}
#[derive(Debug, Clone)]
struct Item {
    conditions: Vec<(usize, u64)>,
    commands: Vec<Command>,
}

#[derive(Debug, Default)]
pub struct Conditions {
    values: [u64; 9],
    turn_end_time: i32,
    pending_sounds: Vec<Arc<str>>,
}

impl Conditions {
    pub(crate) fn play_sounds(
        &mut self,
        ps: &mut PlayerState,
        strings: &mut crate::level::ConfigStrings,
    ) -> io::Result<()> {
        for alias in self.pending_sounds.drain(..) {
            let index = crate::vm::sound::alias_index(alias.as_bytes(), strings)?;
            if index != 0 {
                let w = ps.words_mut();
                let sequence = w[0xa4 / 4];
                let slot = (sequence & 3) as usize;
                w[0xa8 / 4 + slot] = 0xb3;
                w[0xb8 / 4 + slot] = index;
                w[0xa4 / 4] = sequence.wrapping_add(1);
            }
        }
        Ok(())
    }

    pub fn value(&self, index: usize) -> u64 {
        self.values.get(index).copied().unwrap_or(0)
    }

    /// PlayerAnim_SetMoveParams updates low and high movement mask words differently.
    pub fn update_from_entity(
        &mut self,
        entity: &crate::entity_delta::EntityState,
        weapons: &WeaponRegistry,
        animations: &PlayerAnimations,
        view_pitch: f32,
    ) {
        let word = |name: &str| entity.field(name).unwrap_or(0);
        let flags = word("eFlags");
        if let Some(def) = weapons.get(word("weapon") as usize) {
            self.values[0] = 1 << def.player_anim_type;
            self.values[1] = 1 << def.weapon_class;
        }
        self.values[2] = u64::from(flags & 0x300 != 0);
        self.values[4] = u64::from(view_pitch > 0.);
        self.values[5] = u64::from(flags & 4 != 0);
        self.values[6] = u64::from(flags & 0x40 != 0);
        self.values[7] = u64::from(flags & 0x40000 != 0);
        let record = animations
            .animation_for_index((word("legsAnim") & !0x200) as u16)
            .copied()
            .unwrap_or_default();
        if record.move_types != 0 {
            // BG_UpdateConditionValue(checkConversion=0) writes only the low dword.
            self.values[3] =
                (self.values[3] & !u64::from(u32::MAX)) | u64::from(record.move_types as u32);
        }
        self.values[8] = if record.flags & 0x10 != 0 {
            1
        } else if record.flags & 0x20 != 0 {
            2
        } else {
            0
        };
    }

    pub fn set_weapon(&mut self, index: usize, registry: &WeaponRegistry) {
        if let Some(def) = registry.get(index) {
            self.values[0] = 1 << def.player_anim_type;
            self.values[1] = 1 << def.weapon_class;
        }
    }
    pub fn refresh(&mut self, ps: &PlayerState, cmd: &UserCmd, registry: &WeaponRegistry) {
        let w = ps.words();
        let weapon = if w[3] & 0x20000 != 0 {
            (1..128)
                .find(|&n| registry.get(n).is_some_and(|d| d.weapon_type == 3))
                .unwrap_or(0)
        } else if w[3] & 0x10 != 0 {
            w[0xd0 / 4] as usize
        } else {
            w[0xd4 / 4] as usize
        };
        if let Some(def) = registry.get(weapon) {
            self.values[0] = 1 << def.player_anim_type;
            self.values[1] = 1 << def.weapon_class;
        }
        self.values[2] = u64::from(w[0xa0 / 4] & 0x300 != 0);
        self.values[4] = u64::from(f32::from_bits(w[0xe8 / 4]) > 0.);
        self.values[6] = u64::from(cmd.buttons & 1 != 0);
        self.values[7] = u64::from(w[0xa0 / 4] & 0x40000 != 0);
    }

    fn matches(&self, item: &Item) -> bool {
        item.conditions.iter().all(|&(slot, value)| {
            if matches!(slot, 0 | 1 | 3) {
                self.values[slot] & value != 0
            } else {
                self.values[slot] == value
            }
        })
    }
}

#[derive(Debug)]
pub struct FootstepSettings {
    pub move_threshold: f32,
    pub stumble_ms: i32,
    pub turn_animations: bool,
    pub time_ms: i32,
}

#[derive(Debug, Default, Clone)]
pub struct PlayerAnimations {
    pub mantle: Option<std::sync::Arc<crate::movement::mantle::MantleAnimations>>,
    scripts: BTreeMap<Script, Vec<Item>>,
    animations: BTreeMap<String, Animation>,
    nodes: Vec<String>,
    children: Vec<std::ops::Range<u16>>,
    clips: BTreeMap<u16, std::sync::Arc<pose::Clip>>,
}

// Offsets in the 1.3 playerState, including two durations absent from netfields.
fn play(
    ps: &mut PlayerState,
    part: &Part,
    duration: i32,
    timer: bool,
    continuing: bool,
    force: bool,
) -> bool {
    let w = ps.words_mut();
    let mut started = false;
    for (body, animation, timer_offset, duration_offset) in
        [(1, 0x7c, 0x78, 0x88), (2, 0x84, 0x80, 0x8c)]
    {
        if part.body != body && part.body != 3 {
            continue;
        }
        let a = animation / 4;
        let t = timer_offset / 4;
        if w[t] as i32 >= 50 && !force {
            continue;
        }
        let index = if body == 2 && part.body == 3 {
            0
        } else {
            u32::from(part.clip.index)
        };
        if !continuing || (w[a] & !0x200) != index {
            w[a] = (w[a] & 0x200 ^ 0x200) | index;
            w[duration_offset / 4] = duration as u32;
            if timer {
                w[t] = duration as u32;
            }
            if body == 1 {
                started = true;
            }
        } else if timer && index != 0 && part.clip.looping {
            w[t] = duration as u32;
        }
    }
    started
}

fn execute(
    c: &mut Conditions,
    ps: &mut PlayerState,
    cmd: &Command,
    timer: bool,
    continuing: bool,
    force: bool,
) -> i32 {
    if let Some(alias) = &cmd.sound {
        c.pending_sounds.push(Arc::clone(alias));
    }
    let Some(first) = cmd.parts.first() else {
        return -1;
    };
    let duration = i32::from(first.duration) + 50;
    let mut legs = false;
    for part in &cmd.parts {
        let started = play(ps, part, duration, timer, continuing, force);
        if matches!(first.body, 1 | 3) {
            legs = started
        }
    }
    if legs { duration } else { -1 }
}

impl PlayerAnimations {
    pub fn clip(&self, name: &str) -> Option<Animation> {
        self.animations.get(&name.to_ascii_lowercase()).copied()
    }
    pub fn node(&self, index: u16) -> Option<&str> {
        self.nodes.get(usize::from(index)).map(String::as_str)
    }
    pub fn child_range(&self, index: u16) -> std::ops::Range<u16> {
        self.children
            .get(usize::from(index))
            .cloned()
            .unwrap_or_default()
    }
    pub fn pose_clip(&self, index: u16) -> Option<&pose::Clip> {
        self.clips.get(&index).map(std::sync::Arc::as_ref)
    }
    pub fn animation_for_index(&self, index: u16) -> Option<&Animation> {
        self.animations.get(self.node(index)?)
    }
    pub fn node_count(&self) -> usize {
        self.nodes.len()
    }

    fn movement(&self, c: &mut Conditions, ps: &mut PlayerState, movement: usize) -> Option<bool> {
        for state in (0..4).rev() {
            if let Some(item) = self
                .scripts
                .get(&Script::Move(state, movement))
                .and_then(|items| items.iter().find(|i| c.matches(i)))
            {
                if item.commands.is_empty() {
                    return None;
                }
                let command = &item.commands[(ps.words()[0xcc / 4] as usize) % item.commands.len()];
                c.values[3] = 1 << movement;
                return Some(execute(c, ps, command, false, true, false) >= 0);
            }
        }
        None
    }

    pub fn event(
        &self,
        c: &mut Conditions,
        ps: &mut PlayerState,
        event: i32,
        continuing: bool,
        force: bool,
        random: &mut crate::vm::items::NativeRandom,
    ) -> i32 {
        if event != 1 && ps.words()[1] >= 6 {
            return -1;
        }
        let Some(item) = self
            .scripts
            .get(&Script::Event(event as usize))
            .and_then(|items| items.iter().find(|i| c.matches(i)))
        else {
            return -1;
        };
        if item.commands.is_empty() {
            return -1;
        }
        let command = &item.commands[random.draw() as usize % item.commands.len()];
        execute(c, ps, command, true, continuing, force)
    }

    pub fn move_id(&self, c: &mut Conditions, ps: &mut PlayerState, id: u8) -> Option<bool> {
        self.movement(c, ps, usize::from(id))
    }
    pub fn drop_timers(ps: &mut PlayerState, ms: i32) {
        let w = ps.words_mut();
        for index in [0x78 / 4, 0x80 / 4] {
            if w[index] as i32 > 0 {
                w[index] = (w[index] as i32 - ms).max(0) as u32;
            }
        }
    }

    /// PM_Footsteps selects the movement script after ground tracing. Command-dependent
    /// state is updated on the same branch that consumes it in the original.
    pub fn move_player(
        &self,
        c: &mut Conditions,
        ps: &mut PlayerState,
        cmd: &UserCmd,
        settings: &FootstepSettings,
        controllers: &mut controllers::PlayerControllers,
    ) {
        let w = ps.words();
        let mode = w[1];
        if mode >= 6 || (2..=5).contains(&mode) {
            return;
        }
        let flags = w[3];
        let stance = match w[0xf4 / 4] {
            11 => 1,
            40 => 2,
            _ => 0,
        };
        let idle = [1, 3, 2][stance];
        if w[0xa0 / 4] & 0x300 != 0 {
            let movement = if flags & 1 != 0 {
                3
            } else if flags & 2 != 0 {
                2
            } else {
                1
            };
            self.movement(c, ps, movement);
            return;
        }
        if w[0x60 / 4] == 1023 && mode != 1 {
            if flags & 0x20 != 0 {
                if cmd.server_time.wrapping_sub(w[0x70 / 4] as i32) < 300 {
                    return;
                }
                let climb = if f32::from_bits(w[0x28 / 4]) >= 0. {
                    18
                } else {
                    19
                };
                self.movement(c, ps, climb);
            }
            if stance == (flags as usize & 3) {
                return;
            }
        }
        let w = ps.words();
        let stumble = (w[0x94 / 4] as i32)
            .saturating_sub(settings.stumble_ms)
            .max(0)
            < w[0x90 / 4] as i32;
        let xy = [
            f64::from(f32::from_bits(w[0x20 / 4])),
            f64::from(f32::from_bits(w[0x24 / 4])),
        ];
        let speed = ((xy[0] * xy[0] + xy[1] * xy[1]) as f32).sqrt();
        if speed < settings.move_threshold || mode == 1 {
            let mut move_id = idle;
            if settings.turn_animations {
                let (previous, active) = controllers.legs_yaw();
                let torso = controllers.torso_yaw();
                if !active {
                    if settings.time_ms < c.turn_end_time {
                        controllers.set_legs_yaw(torso);
                        return;
                    }
                    if c.turn_end_time != 0 {
                        c.turn_end_time = 0;
                        controllers.set_legs_yaw(torso);
                    }
                } else {
                    if stance != 1 {
                        move_id =
                            14 + usize::from(torso >= previous) + 2 * usize::from(stance == 2);
                    }
                    controllers.set_legs_yaw(torso);
                    if c.turn_end_time < settings.time_ms {
                        c.turn_end_time = 0;
                    }
                }
            }
            if move_id == 1 && stumble {
                let mut yaw = w[0x98 / 4] as i32;
                if yaw < 0 {
                    yaw = yaw.wrapping_add(360);
                }
                let flinch = match yaw {
                    45..135 => 33,
                    135..225 => 32,
                    225..315 => 34,
                    _ => 31,
                };
                self.movement(c, ps, flinch);
                return;
            }
            let started = self.movement(c, ps, move_id);
            if (14..=17).contains(&move_id) {
                if started == Some(true) && c.turn_end_time == 0 {
                    c.turn_end_time = settings.time_ms.wrapping_add(ps.words()[0x88 / 4] as i32);
                }
                if started.is_none() {
                    self.movement(c, ps, if stance == 0 && stumble { 31 } else { idle });
                }
            }
            return;
        }
        if cmd.forward_move != 0 {
            c.values[8] = 0
        } else if cmd.right_move != 0 {
            c.values[8] = if cmd.right_move < 0 { 1 } else { 2 };
        }
        let backwards = usize::from(flags & 0x80 != 0);
        let slow = flags & 0x100 != 0 || f32::from_bits(w[0x4c / 4]) != 0.;
        let base = match (stance, slow, stumble) {
            (1, _, _) => 8,
            (2, _, true) => 39,
            (2, true, false) => 6,
            (2, false, false) => 12,
            (_, true, true) => 37,
            (_, false, true) => 35,
            (_, true, false) => 4,
            (_, false, false) => 10,
        };
        let mut result = self.movement(c, ps, base + backwards);
        if cmd.forward_move == 0 && cmd.right_move == 0 {
            if speed <= 120. {
                if stance != 0 {
                    result = self.movement(c, ps, idle);
                }
                if result.is_none() {
                    self.movement(c, ps, if stumble { 31 } else { 1 });
                }
            }
        } else if result.is_none() {
            self.movement(c, ps, 1);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn combined_body_toggles_and_timer_restarts_only_legs() {
        let mut ps = PlayerState::default();
        let mut c = Conditions::default();
        let cmd = Command {
            sound: None,
            parts: vec![Part {
                body: 3,
                clip: Animation {
                    index: 42,
                    looping: true,
                    duration: 800,
                    ..Default::default()
                },
                duration: 800,
            }],
        };
        assert_eq!(execute(&mut c, &mut ps, &cmd, true, false, false), 850);
        assert_eq!(ps.words()[0x7c / 4], 554);
        assert_eq!(ps.words()[0x84 / 4], 512);
        ps.words_mut()[0x78 / 4] = 0;
        ps.words_mut()[0x80 / 4] = 0;
        assert_eq!(execute(&mut c, &mut ps, &cmd, true, true, false), -1);
        assert_eq!(ps.words()[0x78 / 4], 850);
        assert_eq!(ps.words()[0x80 / 4], 0);
    }
}
