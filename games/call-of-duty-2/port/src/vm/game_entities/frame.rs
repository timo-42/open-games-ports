//! Non-client entity frame work after Scr_AdvanceTime (G_RunEntityState).
use super::{MapEntity, ScriptWorld};
use crate::assets::models::{ModelRegistry, ServerModel};
use crate::collision::CollisionMap;
use crate::entity_delta::EntityState;
use std::io;

type Vec3 = [f32; 3];

#[derive(Debug)]
pub(super) struct IdleTurret {
    next_think: i32,
    rest_pitch: Option<f32>,
}

impl IdleTurret {
    pub(super) fn new(now: i32) -> Self {
        Self {
            next_think: now.wrapping_add(50),
            rest_pitch: None,
        }
    }

    fn due(&self, now: i32) -> bool {
        self.next_think > 0 && self.next_think <= now
    }

    fn think(&mut self, now: i32, state: &mut EntityState, target: Option<f32>) {
        if !self.due(now) {
            return;
        }
        self.next_think = now.wrapping_add(50);
        if let Some(rest) = self.rest_pitch {
            state.set_field("loopSound", 0);
            state.set_field(
                "eFlags",
                state.field("eFlags").expect("entity flags") & !0x40,
            );
            rest_aim(state, rest);
        } else {
            self.rest_pitch = target;
        }
    }
}

pub(super) fn basis(angles: Vec3) -> [Vec3; 3] {
    let [forward, right, up] = crate::vm::math::axes(angles);
    [forward, right.map(|v| -v), up]
}

fn tags(model: Option<&ServerModel>) -> Option<(Vec3, Vec3)> {
    let model = model?;
    let tag = |name: &str| {
        model
            .parts
            .bones
            .iter()
            .find(|bone| bone.name.eq_ignore_ascii_case(name))
            .map(|bone| bone.bind_pose.origin)
    };
    Some((tag("tag_aim")?, tag("tag_butt")?))
}

fn transform(point: Vec3, axis: &[Vec3; 3], origin: Vec3) -> Vec3 {
    std::array::from_fn(|i| {
        (f64::from(point[0]) * f64::from(axis[0][i])
            + f64::from(point[1]) * f64::from(axis[1][i])
            + f64::from(point[2]) * f64::from(axis[2][i])
            + f64::from(origin[i])) as f32
    })
}

/// Turret_FindAimOffset rotates the stock of the gun around tag_aim and
/// stores the first obstructed pitch. The model tag positions are local.
fn find_rest_pitch(
    origin: Vec3,
    angles: Vec3,
    aim: Vec3,
    butt: Vec3,
    mut trace: impl FnMut(Vec3, Vec3) -> io::Result<f32>,
) -> io::Result<f32> {
    let axis = basis(angles);
    let start = transform(aim, &axis, origin);
    let offset = std::array::from_fn(|i| butt[i] - aim[i]);
    for step in 0..=30 {
        let pitch = -3.0 * step as f32;
        let rotated = transform(offset, &basis([pitch, 0., 0.]), [0.; 3]);
        let local = std::array::from_fn(|i| aim[i] + rotated[i]);
        if trace(start, transform(local, &axis, origin))? < 1.0 {
            return Ok(pitch);
        }
    }
    Ok(-90.)
}

fn step_angle(to: f32, from: f32) -> f32 {
    let mut distance = (to - from) % 360.;
    if distance > 180. {
        distance -= 360.;
    } else if distance < -180. {
        distance += 360.;
    }
    from + distance.clamp(-10., 10.)
}

/// Turret_RestAim calls Turret_ClampAim with 200 deg/s on both axes and a
/// fixed 0.05 s step regardless of the server tick interval.
fn rest_aim(state: &mut EntityState, rest_pitch: f32) {
    let angle = |name| f32::from_bits(state.field(name).expect("turret angle field"));
    let prev = angle("angles2[0]");
    let total = step_angle(rest_pitch, prev + angle("angles2[2]"));
    let yaw = step_angle(0., angle("angles2[1]"));
    let pitch = step_angle(total, prev);
    state.set_field("angles2[0]", pitch.to_bits());
    state.set_field("angles2[1]", yaw.to_bits());
    state.set_field("angles2[2]", (total - pitch).to_bits());
}

fn update_flags_and_expiry(entity: &mut MapEntity, is_client: bool, now: i32) -> bool {
    let mut flags = entity.state.field("eFlags").expect("entity flags");
    if !is_client {
        flags = (flags & !0x20) | ((entity.flags & 0x800) >> 6);
        entity.state.set_field("eFlags", flags);
    }
    let time2 = entity.state.field("time2").expect("entity expiration") as i32;
    entity
        .mantle_blocker_free_at
        .is_some_and(|expires| expires > 0 && expires <= now)
        || (flags == 0x10000 && time2 < now)
        || entity
            .free_after_event_at
            .is_some_and(|start| now.wrapping_sub(start) > 300)
}

impl ScriptWorld {
    pub fn run_entity_frame(
        &mut self,
        now: i32,
        collision: &CollisionMap,
        models: &ModelRegistry,
    ) -> io::Result<Vec<(u32, &'static str)>> {
        let mut notices = Vec::new();
        for slot in super::tag_links::frame_order(&self.entities) {
            super::tag_links::update(self, slot, collision, models)?;
            let Some(entity) = self.entities.get_mut(slot) else {
                continue;
            };
            if update_flags_and_expiry(entity, slot < 64, now) {
                self.free_entity(slot, now)?;
            } else if (64..72).contains(&slot) || entity.dropped_item.is_some() {
                // The moving entity's own contents never match its movement mask.
                // Keep the rest of the live entity pool available for the trace.
                let mut entity = self.entities.slots[usize::from(slot)]
                    .take()
                    .expect("moving entity");
                let world = crate::vm::items::motion::MotionWorld {
                    map: collision,
                    entities: &self.entities,
                    owner: entity.owner_num,
                };
                let result = if entity.dropped_item.is_some() {
                    crate::vm::items::motion::run_item(&mut entity, now, &world)
                } else {
                    let delta = entity
                        .corpse_animation
                        .as_mut()
                        .map_or([0.; 3], |a| a.displacement(&self.animations, now));
                    crate::vm::items::motion::run_body(&mut entity, now, &world, delta).and_then(
                        |free| {
                            if !free {
                                super::corpses::BodyAnimation::update(
                                    &mut entity,
                                    &self.animations,
                                    models,
                                    now,
                                )?;
                            }
                            Ok(free)
                        },
                    )
                };
                self.entities.slots[usize::from(slot)] = Some(entity);
                if result? {
                    self.free_entity(slot, now)?;
                }
            } else if entity.mover.is_some() && entity.tag_link.is_none() {
                super::mover::step(self, slot, collision, now, &mut notices)?;
            } else if entity.owner_num == 1023
                && let Some(turret) = &entity.idle_turret
                && turret.due(now)
            {
                let idx = entity.model_index;
                let origin = entity.origin;
                let angles = entity.angles;
                let rest = if turret.rest_pitch.is_none() {
                    Some(if let Some((aim, butt)) = tags(models.get(idx)) {
                        find_rest_pitch(origin, angles, aim, butt, |from, to| {
                            collision
                                .trace_point(&self.entities, models, from, to, slot, 0x811, false)
                                .map(|hit| hit.fraction)
                        })?
                    } else {
                        -90.
                    })
                } else {
                    None
                };
                let entity = self.entities.get_mut(slot).expect("current turret");
                entity.idle_turret.as_mut().expect("current turret").think(
                    now,
                    &mut entity.state,
                    rest,
                );
            }
        }
        Ok(notices)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rest_search_uses_model_space_and_stops_at_first_obstruction() {
        let mut probes = 0;
        let pitch = find_rest_pitch(
            [1., 2., 3.],
            [0., 90., 0.],
            [2., 0., 0.],
            [-2., 0., 0.],
            |start, end| {
                assert!((start[0] - 1.).abs() < 0.00001 && (start[1] - 4.).abs() < 0.00001);
                if probes == 0 {
                    assert_eq!(end[2], 3.);
                }
                probes += 1;
                Ok(if end[2] < 2. { 0.75 } else { 1. })
            },
        );
        assert_eq!(probes, 6);
        assert_eq!(pitch.expect("valid trace"), -15.);
    }

    #[test]
    fn late_frames_run_one_turret_think_and_hidden_flags_follow_native_flags() {
        let mut e = MapEntity::new(73, 4);
        let mut turret = IdleTurret::new(0);
        for (now, want) in [(49, 0.), (100, 0.), (200, -10.), (300, -20.), (301, -20.)] {
            turret.think(now, &mut e.state, Some(-90.));
            assert_eq!(
                f32::from_bits(
                    e.state
                        .field("angles2[0]")
                        .expect("registered turret angle field")
                ),
                want
            );
        }
        e.flags = 0x800;
        assert!(!update_flags_and_expiry(&mut e, false, 300));
        assert_eq!(e.state.field("eFlags"), Some(0x20));
        e.flags = 0;
        e.state.set_field("eFlags", 0x10000);
        e.state.set_field("time2", 300);
        assert!(!update_flags_and_expiry(&mut e, false, 300));
        assert!(update_flags_and_expiry(&mut e, false, 301));
    }
}
