use super::*;
impl Frame<'_> {
    fn offhand(&self) -> &WeaponDefinition {
        self.registry
            .get(self.word(OFFHAND) as usize)
            .expect("verified offhand")
    }
    fn offhand_prime(&mut self) {
        self.set(STATE, 13);
        self.set(TIME, self.offhand().handling.hold_fire_time);
        self.set(DELAY, 0);
        self.set_bits(FLAGS, 16, true);
        self.event_param(0xa5, self.word(OFFHAND));
        self.anim(19);
        self.prone_flag();
    }
    pub(super) fn cookoff(&mut self) -> bool {
        if self.word(FLAGS) & 16 == 0 || self.offhand().weapon_type != 1 || self.word(FUSE) < 1 {
            return false;
        }
        if self.offhand().handling.cook_off_hold {
            self.set(FUSE, self.word(FUSE) - self.ms);
        }
        if self.word(FUSE) > 0 {
            return false;
        }
        let magazine = clip(self.offhand());
        self.set(magazine, self.word(magazine) - 1);
        self.event(0xc5);
        true
    }
    pub(super) fn offhand_input(&mut self) -> io::Result<()> {
        if self.word(FLAGS) & 0x04000000 != 0 || matches!(self.word(STATE),12..=15|17..=22) {
            return Ok(());
        }
        let wanted = self.cmd.offhand as usize;
        if owned(self.ps, wanted) {
            self.set(OFFHAND, wanted as i32);
        }
        let class = if self.cmd.buttons & 0x10000 != 0 {
            1
        } else if self.cmd.buttons & 0x20000 != 0 {
            2
        } else {
            return Ok(());
        };
        let Some((id, def)) = registered(self.registry).find(|(id, def)| {
            def.offhand_class == class
                && owned(self.ps, *id)
                && self.word(clip(def)).wrapping_add(self.word(ammo(def))) != 0
        }) else {
            self.event(0x94);
            return Ok(());
        };
        if def.weapon_type != 1 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("{} cannot be used as an offhand grenade", def.name),
            ));
        }
        self.event_param(0xa7, id as i32);
        self.set(OFFHAND, id as i32);
        if self.weapon() == 0 || self.word(STATE) == 16 {
            self.offhand_prime();
        } else {
            self.set(STATE, 12);
            self.set(TIME, self.props().quick_drop_time);
            self.set(DELAY, 0);
            self.set_bits(FLAGS, 0x50, false);
            self.event(0x95);
            self.anim(17);
        }
        Ok(())
    }
    pub(super) fn melee_input(&mut self, elapsed: bool) -> io::Result<()> {
        let state = self.word(STATE);
        let h = self.props();
        if (12..=22).contains(&state)
            || h.melee_damage == 0
            || elapsed
            || (self.word(DELAY) != 0 && !(5..=9).contains(&state))
        {
            return Ok(());
        }
        if self.cmd.buttons & 4 == 0 {
            self.set_bits(FLAGS, 0x2000, false);
            return Ok(());
        }
        if self.word(FLAGS) & 0x2000 != 0 {
            return Ok(());
        }
        self.set_bits(FLAGS, 0x2000, true);
        if (self.float(FRACTION) > 0. && self.def().overlay_reticle != 0)
            || matches!(state, 1 | 2 | 10 | 11)
        {
            return Ok(());
        }
        (self.animate)(self.ps, 17, false, true)?;
        self.anim(8);
        self.event(0xa3);
        if h.melee_delay == 0 {
            self.melee_strike();
        } else {
            self.set(TIME, h.melee_time);
            self.set(DELAY, h.melee_delay);
            self.set(STATE, 10);
            self.prone_flag();
        }
        Ok(())
    }
    pub(super) fn melee_strike(&mut self) {
        self.set(
            TIME,
            self.word(TIME)
                .max(self.props().melee_time - self.props().melee_delay),
        );
        self.event(0xa4);
        self.set(STATE, 11);
        self.prone_flag();
    }
    fn bino_phase(&mut self, phase: i32) {
        self.set(STATE, phase);
        match phase {
            17 => {
                self.event(0x95);
                self.set_bits(FLAGS, 0x40, false);
                self.set(TIME, 0);
                self.set(
                    DELAY,
                    self.settings.ads_exit_delay + self.props().ads_trans_out_time,
                );
            }
            18 => {
                self.set(TIME, self.props().quick_drop_time);
                self.set(DELAY, 0);
                self.anim(17);
            }
            19 => {
                self.set(
                    TIME,
                    self.binoculars()
                        .map_or(0, |(_, d)| d.handling.ads_trans_in_time),
                );
                self.set(DELAY, 0);
                self.set_bits(FLAGS, 0x20000, true);
                self.event(0xa8);
                self.anim(0);
            }
            20 => {
                self.set(TIME, 0);
                self.set(DELAY, 0);
            }
            21 => {
                self.set(
                    TIME,
                    self.settings.ads_exit_delay
                        + self
                            .binoculars()
                            .map_or(0, |(_, d)| d.handling.ads_trans_out_time),
                );
                self.set(DELAY, 0);
                self.event(0xac);
                self.anim(0);
            }
            22 => {
                self.set_bits(FLAGS, 0x20000, false);
                self.event(0xa9);
                if self.cmd.weapon == 0 {
                    self.set(TIME, 0);
                    self.set(DELAY, 1);
                } else {
                    self.set(TIME, self.props().quick_raise_time);
                    self.set(DELAY, 0);
                    self.anim(18);
                }
            }
            _ => unreachable!(),
        }
    }
    pub(super) fn binocular_input(&mut self) {
        let st = self.word(STATE);
        let active = (17..=22).contains(&st);
        if matches!(st, 1..=16)
            || (!active
                && ((self.float(FRACTION) > 0. && self.def().overlay_reticle != 0)
                    || self.cmd.buttons & 0x30015 != 0))
        {
            return;
        }
        let changed = self.word(0x128 / 4) != 0
            || self.cmd.weapon != self.old.weapon
            || self.cmd.offhand != self.old.offhand
            || self.cmd.buttons & 0x30015 != 0;
        if active && changed {
            force_ready(self.ps);
            if self.cmd.buttons & 1 != 0 {
                self.set(TIME, 1);
            }
            return;
        }
        if self.cmd.buttons & 1 != 0 {
            return;
        }
        if st == 20 {
            match (self.cmd.buttons & 0x28 != 0, self.old.buttons & 0x28 != 0) {
                (true, false) => self.event(0xaa),
                (false, true) => self.event(0xab),
                _ => {}
            }
        }
        let down = self.cmd.buttons & 0x4000 != 0;
        let pressed = down && self.old.buttons & 0x4000 == 0;
        if !active
            && (if self.settings.toggle_binoculars {
                pressed
            } else {
                down
            })
            && self.binoculars().is_some()
        {
            self.bino_phase(if self.cmd.weapon == 0 {
                19
            } else if self.cmd.buttons & 0x1000 == 0 {
                18
            } else {
                17
            });
        } else if (18..=20).contains(&st)
            && (if self.settings.toggle_binoculars {
                pressed
            } else {
                !down
            })
        {
            self.bino_phase(if st == 18 { 22 } else { 21 });
        }
    }
    pub(super) fn equipment_step(&mut self) -> io::Result<()> {
        match self.word(STATE) {
            12 => self.offhand_prime(),
            13 => {
                self.set(STATE, 14);
                self.set(TIME, 0);
                self.set(DELAY, 0);
                self.set_bits(FLAGS, 16, true);
                self.set(FUSE, self.offhand().handling.fuse_time);
            }
            14 => {
                if self.old.buttons & 0x30000 != 0 && self.cmd.buttons & 0x30000 != 0 {
                    self.set(DELAY, 1);
                    return Ok(());
                }
                self.set(STATE, 15);
                self.set(TIME, self.offhand().handling.fire_time);
                self.set(DELAY, 0);
                self.set_bits(FLAGS, 16, true);
                self.event_param(0xa6, self.word(OFFHAND));
                self.anim(2);
                let mag = clip(self.offhand());
                self.set(mag, self.word(mag) - 1);
                (self.animate)(self.ps, 2, false, true)?;
                if self.word(mag) + self.word(ammo(self.offhand())) == 0 {
                    self.event(0x94);
                }
            }
            15 => {
                if self.weapon() == 0 {
                    self.set(TIME, 0);
                    self.set(DELAY, 1);
                } else {
                    self.set(TIME, self.props().quick_raise_time);
                    self.set(DELAY, 0);
                    self.anim(18);
                }
                self.set(STATE, 16);
                self.set_bits(FLAGS, 0x810, false);
            }
            16 | 22 => force_ready(self.ps),
            st @ 17..=19 => self.bino_phase(st + 1),
            20 => {}
            21 => self.bino_phase(22),
            _ => unreachable!(),
        }
        Ok(())
    }
}
