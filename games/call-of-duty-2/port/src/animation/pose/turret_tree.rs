use super::{PlayerAnimations, PlayerTree};

impl PlayerTree {
    pub(crate) fn turret_clip_time(&self, id: u16) -> f32 {
        self.slots.get(&id).map_or(0., |slot| slot.time)
    }

    /// XAnim_NodeWeightedLength averages the active clip playback rates
    /// within each row, then averages those rows for the synchronized stance.
    pub(super) fn sync_turret_clips(&mut self, bank: &PlayerAnimations, dt: f32) {
        let active: Vec<_> = self
            .slots
            .iter()
            .filter_map(|(&id, slot)| {
                let anim = bank.animation_for_index(id)?;
                let looping = anim.synchronized_loop?;
                (anim.flags & 4 != 0 && slot.weight != 0.).then_some((
                    id,
                    looping,
                    slot.previous_phase,
                    slot.rate,
                ))
            })
            .collect();
        for (id, looping, phase, speed) in active {
            let mut group_rate = 0.;
            let mut group_weight = 0.;
            for row in bank.child_range(id) {
                let Some(r) = self.slots.get(&row).filter(|r| r.weight != 0.) else {
                    continue;
                };
                let mut rate = 0.;
                let mut weight = 0.;
                for leaf in bank.child_range(row) {
                    let Some(s) = self.slots.get(&leaf).filter(|s| s.weight != 0.) else {
                        continue;
                    };
                    if let Some(c) = bank.pose_clip(leaf).filter(|c| c.frames != 0) {
                        rate += (f32::from(c.fps) / f32::from(c.frames)) * s.weight * s.rate;
                        weight += s.weight;
                    }
                }
                if weight != 0. {
                    group_rate += (rate / weight) * r.weight * r.rate;
                    group_weight += r.weight;
                }
            }
            if group_weight == 0. {
                continue;
            }
            let time = phase + (dt * (group_rate / group_weight) * speed);
            let time = if looping { time.fract() } else { time.min(1.) };
            self.slots.get_mut(&id).expect("active stance").time = time;
            for row in bank.child_range(id) {
                if let Some(r) = self.slots.get_mut(&row).filter(|s| s.weight != 0.) {
                    r.previous_phase = phase;
                    r.time = time;
                    for leaf in bank.child_range(row) {
                        if let Some(s) = self.slots.get_mut(&leaf).filter(|s| s.weight != 0.) {
                            s.previous_phase = phase;
                            s.time = time;
                        }
                    }
                }
            }
        }
    }
    /// Turret_PositionUser clears every descendant's current and goal weight.
    /// Active nodes retain animation time across frames while their weights change.
    pub(crate) fn set_turret_weights(
        &mut self,
        bank: &PlayerAnimations,
        stance: u16,
        goals: &[(u16, f32)],
        frame_ms: i32,
    ) {
        for row in bank.child_range(stance) {
            for id in std::iter::once(row).chain(bank.child_range(row)) {
                if let Some(slot) = self.slots.get_mut(&id) {
                    slot.target(0., 0.);
                }
            }
        }
        for &(id, weight) in goals {
            if weight >= 0.001 {
                let blend = (frame_ms as f32 * 0.001) / weight;
                self.slots.entry(id).or_default().target(weight, blend);
            }
        }
    }
}
