//! Multiplayer BG_SetNewAnimation / BG_RunLerpFrame and XAnim blend slots.
#[path = "turret_tree.rs"]
mod turret_tree;
use super::{PlayerSkeleton, PoseNode};
use crate::{
    animation::controllers::{PlayerControllers, ViewInput},
    animation::{Animation, PlayerAnimations},
    vm::GameHost,
};
use std::{collections::BTreeMap, io};

#[derive(Clone, Copy, Debug)]
struct Slot {
    time: f32,
    previous_phase: f32,
    target: f32,
    weight: f32,
    remaining: f32,
    rate: f32,
}
impl Default for Slot {
    fn default() -> Self {
        Self {
            time: 0.,
            previous_phase: 0.,
            target: 0.,
            weight: 0.,
            remaining: 0.,
            rate: 1.,
        }
    }
}
impl Slot {
    fn target(&mut self, weight: f32, blend: f32) {
        if blend == 0. {
            self.weight = weight;
        } else if self.weight == 0. {
            self.weight = weight * 0.001;
        }
        self.remaining = if weight == 0. {
            if self.target != 0. {
                blend
            } else {
                blend.min(self.remaining)
            }
        } else {
            ((f64::from(weight) - f64::from(self.weight)).abs() * f64::from(blend)
                / f64::from(weight.max(self.weight))) as f32
        };
        self.target = weight;
        self.rate = 1.;
    }
}
#[derive(Debug, Clone, Copy, Default)]
struct LerpFrame {
    number: u16,
    previous_ms: i32,
    previous_position: [f32; 3],
}
#[derive(Debug, Default, Clone)]
pub struct PlayerTree {
    pub controllers: PlayerControllers,
    slots: BTreeMap<u16, Slot>,
    parts: [LerpFrame; 2],
    previous_time: Option<i32>,
    move_blend_until: i32,
}

impl PlayerTree {
    fn goal(&mut self, bank: &PlayerAnimations, id: u16, weight: f32, blend: f32) {
        if weight == 0. && !self.slots.contains_key(&id) {
            return;
        }
        self.slots.entry(id).or_default().target(weight, blend);
        let mut active = id;
        while active != 0 {
            let Some(parent) =
                (0..bank.node_count() as u16).find(|&p| bank.child_range(p).contains(&active))
            else {
                break;
            };
            let slot = self.slots.entry(parent).or_default();
            if slot.target == 0. {
                slot.target(1., if parent == 0 { 0. } else { blend });
            }
            active = parent;
        }
    }
    fn advance_slots(&mut self, bank: &PlayerAnimations, dt: f32) {
        self.slots
            .retain(|_, slot| slot.weight != 0. || slot.target != 0.);
        for (&id, slot) in &mut self.slots {
            slot.previous_phase = slot.time;
            if dt >= slot.remaining {
                slot.weight = slot.target;
                slot.remaining = 0.;
            } else {
                slot.weight = (f64::from(slot.weight)
                    + (f64::from(slot.target) - f64::from(slot.weight)) * f64::from(dt)
                        / f64::from(slot.remaining)) as f32;
                if slot.weight < 0.000001 {
                    slot.weight = slot.target * 0.001;
                }
                slot.remaining -= dt;
            }
            if slot.weight != 0.
                && let Some(clip) = bank.pose_clip(id)
                && clip.frames != 0
            {
                let time =
                    slot.time + dt * slot.rate * (f32::from(clip.fps) / f32::from(clip.frames));
                slot.time = if clip.looping {
                    time.fract()
                } else {
                    time.min(1.)
                };
            }
        }
        self.sync_turret_clips(bank, dt);
    }
    fn switch(
        &mut self,
        bank: &PlayerAnimations,
        number: u16,
        part: usize,
        slot_num: u16,
        now: i32,
        death: bool,
    ) {
        let id = number & !0x200;
        let old = self.parts[part].number & !0x200;
        let new = bank.animation_for_index(id).copied().unwrap_or_default();
        let prev = bank.animation_for_index(old).copied().unwrap_or_default();
        let legs = part == 0;
        if legs
            && [0x180000000c4_u64, 0x308]
                .into_iter()
                .any(|mask| (new.move_types & mask != 0) != (prev.move_types & mask != 0))
        {
            self.move_blend_until = now.saturating_add(400);
        }
        let default_blend = if new.move_speed != 0. {
            120
        } else if prev.move_speed != 0. {
            250
        } else {
            170
        };
        let ms = if old == 0 && legs {
            0
        } else {
            let new_ms = if id == 0 { 200 } else { new.blend_time };
            let min = if id == 0 || new_ms < 1 {
                default_blend
            } else {
                -1
            };
            new_ms
                .max(min)
                .max(self.move_blend_until.saturating_sub(now))
        };
        let blend = ms.max(0) as f32 * 0.001;
        if old != 0
            && let Some(old_slot) = self.slots.get_mut(&old)
        {
            old_slot.remaining = if old_slot.target != 0. {
                blend
            } else {
                blend.min(old_slot.remaining)
            };
            old_slot.target = 0.;
            if blend == 0. {
                old_slot.weight = 0.;
            }
        }
        if id != 0 {
            let reset = self.slots.get(&id).is_none_or(|s| s.weight == 0.);
            let phase = if new.flags & 0x40 != 0 && !death {
                1.
            } else if new.move_speed != 0. && new.looping {
                if old == 0 || prev.move_speed == 0. || !prev.looping {
                    let cycle = bank.pose_clip(old).map_or(1000, |c| {
                        if c.fps > 0 {
                            200 + (1000. * f32::from(c.frames) / f32::from(c.fps)) as i32
                        } else {
                            1000
                        }
                    });
                    (f32::from(slot_num) * 0.36 + (now % cycle) as f32 / cycle as f32).fract()
                } else {
                    self.slots.get(&old).map_or(0., |s| s.time)
                }
            } else {
                0.
            };
            self.goal(bank, id, 1., blend);
            if reset || (!legs && new.move_speed == 0.) || (new.flags & 0x40 != 0 && !death) {
                self.slots.entry(id).or_default().time = phase;
            }
        }
        if !legs {
            if let Some(torso) = bank.clip("torso") {
                self.goal(bank, torso.index, if id == 0 { 0. } else { 1. }, blend);
            }
            if let Some(legs) = bank.clip("legs") {
                self.goal(bank, legs.index, if id == 0 { 1. } else { 0.01 }, blend);
            }
        }
        self.parts[part].number = number;
    }
    pub fn advance(
        &mut self,
        bank: &PlayerAnimations,
        numbers: [u16; 2],
        slot: u16,
        now: i32,
        origin: [f32; 3],
        death: bool,
    ) {
        self.advance_clock(bank, now);
        for (part, number) in numbers.into_iter().enumerate() {
            let id = number & !0x200;
            let old = self.parts[part].number & !0x200;
            let vertical = bank
                .animation_for_index(old)
                .is_some_and(|a| a.flags & 2 != 0);
            if self.parts[part].number != number || (id != 0 && !self.slots.contains_key(&id)) {
                self.switch(bank, number, part, slot, now, death);
            }
            let history = &mut self.parts[part];
            if id != 0
                && let Some(current) = self.slots.get_mut(&id)
                && let Some(anim) = bank.animation_for_index(id)
            {
                if anim.move_speed == 0. || history.previous_ms == 0 {
                    current.rate = 1.;
                } else if now != history.previous_ms {
                    current.rate = movement_rate(
                        anim,
                        history.previous_position,
                        origin,
                        now - history.previous_ms,
                        vertical,
                    );
                }
                history.previous_position = origin;
                history.previous_ms = now;
            }
        }
    }
    pub(super) fn advance_clock(&mut self, bank: &PlayerAnimations, now: i32) {
        if self.previous_time == Some(now) {
            return;
        }
        let dt = self
            .previous_time
            .map_or(0., |last| now.saturating_sub(last).max(0) as f32 * 0.001);
        self.advance_slots(bank, dt);
        self.previous_time = Some(now);
    }

    /// SV_DObjBindingSetTime precedes G_RunCorpse. Only delta tracks with
    /// a nonzero goal weight (+0x1c) contribute to XAnim_BlendRootNode with flag 1.
    pub(crate) fn corpse_displacement(&mut self, bank: &PlayerAnimations, now: i32) -> [f32; 3] {
        let dt = self
            .previous_time
            .map_or(0., |last| now.saturating_sub(last).max(0) as f32 * 0.001);
        self.advance_slots(bank, dt);
        self.previous_time = Some(now);
        self.root_delta(bank, 0).unwrap_or([0.; 3])
    }
    fn root_delta(&self, bank: &PlayerAnimations, id: u16) -> Option<[f32; 3]> {
        if let Some(clip) = bank.pose_clip(id) {
            let slot = self.slots.get(&id)?;
            return clip
                .has_root_track()
                .then(|| clip.translation_delta(slot.previous_phase, slot.time, true));
        }
        let mut sum = [0.; 3];
        let mut first = None;
        let mut children = 0;
        let mut total = 0.;
        for child in bank.child_range(id) {
            let Some(s) = self.slots.get(&child).filter(|s| s.target != 0.) else {
                continue;
            };
            if let Some(v) = self.root_delta(bank, child) {
                first.get_or_insert(v);
                children += 1;
                for (axis, delta) in sum.iter_mut().zip(v) {
                    *axis = (f64::from(*axis) + f64::from(s.target) * f64::from(delta)) as f32;
                }
                total += s.target;
            }
        }
        if children <= 1 {
            first
        } else {
            Some(sum.map(|n| n / total))
        }
    }
    pub fn node<'a>(&self, bank: &'a PlayerAnimations, id: u16) -> PoseNode<'a> {
        if let Some(clip) = bank.pose_clip(id) {
            return PoseNode::Leaf {
                clip,
                time: self.slots.get(&id).map_or(0., |s| s.time),
            };
        }
        PoseNode::Group(
            bank.child_range(id)
                .filter_map(|child| {
                    let slot = self.slots.get(&child)?;
                    (slot.weight != 0.).then(|| (slot.weight, self.node(bank, child)))
                })
                .collect(),
        )
    }
}

fn movement_rate(
    anim: &Animation,
    previous: [f32; 3],
    origin: [f32; 3],
    millis: i32,
    vertical: bool,
) -> f32 {
    let distance = if vertical {
        (previous[2] - origin[2]).abs()
    } else {
        previous
            .into_iter()
            .zip(origin)
            .map(|(a, b)| (a - b).powi(2))
            .sum::<f32>()
            .sqrt()
    };
    let travelled_per_second =
        (f64::from(distance) / (f64::from(millis) * f64::from(0.001_f32))) as f32;
    let speed = travelled_per_second / anim.move_speed;
    if speed < 0.1 {
        return if speed < 0.01 && vertical { 0. } else { 0.1 };
    }
    let max = if anim.flags & 2 != 0 {
        4.
    } else {
        (3. - (anim.move_speed - 20.) / 130.).clamp(2., 3.)
    };
    speed.min(max)
}

/// Update the server DObj after the entity state's animation numbers change.
pub fn update_player(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let world = &mut host.world;
    let player = world.players.get_mut(slot)?;
    let entity = world
        .entities
        .get_mut(slot)
        .ok_or_else(|| io::Error::other("missing player entity"))?;
    if entity.model_index == 0 {
        player.pose_tree = PlayerTree::default();
        entity.dobj_pose = None;
        entity.player_skeleton = None;
        return Ok(());
    }
    let bank = &world.animations;
    let numbers = ["legsAnim", "torsoAnim"].map(|key| entity.state.field(key).unwrap_or(0) as u16);
    let origin = ["pos.trBase[0]", "pos.trBase[1]", "pos.trBase[2]"]
        .map(|key| f32::from_bits(entity.state.field(key).unwrap_or(0)));
    let frame_ms = world.frame_ms;
    let view = ["viewangles[0]", "viewangles[1]", "viewangles[2]"]
        .map(|key| f32::from_bits(player.state.field(key).expect("registered view angle")));
    let mut input = ViewInput::from_state(
        &entity.state,
        view,
        player.anim_conditions.value(3) as u32,
        player.anim_conditions.value(6) != 0,
        bank.animation_for_index(numbers[0] & !0x200)
            .map_or(0, |a| a.flags),
    );
    let swing = host
        .config
        .get("bg_swingSpeed")
        .and_then(|s| s.parse::<f32>().ok())
        .unwrap_or(0.2)
        .clamp(0., 1.);
    player
        .pose_tree
        .controllers
        .update_angles(&input, frame_ms, swing);
    player
        .anim_conditions
        .update_from_entity(&entity.state, host.weapons, bank, view[0]);
    input.movement_bits = player.anim_conditions.value(3) as u32;
    input.firing = player.anim_conditions.value(6) != 0;
    player.pose_tree.advance(
        bank,
        numbers,
        slot,
        host.time_ms,
        origin,
        entity.state.field("eFlags").unwrap_or(0) & 0x80000 != 0,
    );
    PlayerSkeleton::record(entity, &mut player.pose_tree, bank, input, frame_ms);
    Ok(())
}

#[cfg(test)]
mod corpse_tests {
    use super::*;
    use crate::animation::pose::Clip;

    fn root_clip(translation: f32) -> io::Result<Clip> {
        // Two-key non-looping v14 XAnim with a translation-only delta track.
        let mut data = vec![14, 0, 2, 0, 0, 0, 2, 20, 0, 0, 0, 2, 0];
        for x in [0., translation] {
            for v in [x, 0., 0.] {
                data.extend(v.to_le_bytes());
            }
        }
        data.push(0);
        Clip::parse(&data)
    }

    #[test]
    fn corpse_uses_goal_weights_during_the_visual_crossfade() -> io::Result<()> {
        let bank = PlayerAnimations {
            children: vec![1..3, 0..0, 0..0],
            clips: [(1, root_clip(12.)?.into()), (2, root_clip(-8.)?.into())].into(),
            ..Default::default()
        };
        let mut tree = PlayerTree::default();
        for (id, goal, weight) in [(1, 0., 0.7), (2, 1., 0.3)] {
            tree.slots.insert(
                id,
                Slot {
                    time: 1.,
                    target: goal,
                    weight,
                    ..Default::default()
                },
            );
        }
        assert_eq!(tree.root_delta(&bank, 0), Some([-8., 0., 0.]));
        Ok(())
    }
}
