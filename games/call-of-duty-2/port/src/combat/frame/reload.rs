use super::*;
impl Frame<'_> {
    fn needs_reload(&self) -> bool {
        let def = self.def();
        let h = self.props();
        let mag = self.word(clip(def));
        self.word(ammo(def)) != 0
            && mag < def.clip_size
            && (!h.no_partial_reload
                || if h.reload_ammo_add == 0 || h.reload_ammo_add >= def.clip_size {
                    mag == 0
                } else {
                    h.reload_ammo_add <= def.clip_size - mag
                })
    }
    fn add_time(&self) -> i32 {
        let h = self.props();
        if matches!(self.word(STATE), 7 | 8) {
            return h.reload_start_add_time.min(h.reload_start_time);
        }
        let full = if self.word(clip(self.def())) == 0 && self.def().weapon_type == 0 {
            h.reload_empty_time
        } else {
            h.reload_time
        };
        if h.reload_add_time != 0 {
            full.min(h.reload_add_time)
        } else {
            full
        }
    }
    fn reload_delay(&mut self) {
        let mut delay = self.add_time();
        if self.props().bolt_action && self.pending_bolt() {
            if delay == 0 {
                delay = self.word(TIME);
            }
            delay = delay.min(self.props().rechamber_bolt_time).max(1);
        }
        if delay != 0 {
            self.set(DELAY, delay);
        }
    }
    fn reload_mag(&mut self) {
        if self.def().weapon_type == 0 && self.word(clip(self.def())) == 0 {
            self.anim(12);
            self.set(TIME, self.props().reload_empty_time);
            self.event(0x97);
        } else {
            self.anim(11);
            self.set(TIME, self.props().reload_time);
            self.event(0x96);
        }
        self.set(STATE, if self.word(STATE) == 8 { 6 } else { 5 });
        self.reload_delay();
    }
    pub(super) fn begin_reload(&mut self) -> io::Result<()> {
        if self.weapon() == 0 || !matches!(self.word(STATE), 0 | 3 | 4) {
            return Ok(());
        }
        if !self.props().clip_only {
            (self.animate)(self.ps, 10, false, true)?;
        }
        self.event(0x95);
        if self.props().segmented_reload && self.props().reload_start_time != 0 {
            self.anim(13);
            self.set(TIME, self.props().reload_start_time);
            self.set(STATE, 7);
            self.event(0x98);
            self.reload_delay();
        } else {
            self.reload_mag();
        }
        Ok(())
    }
    pub(super) fn reload_input(&mut self) -> io::Result<()> {
        let state = self.word(STATE);
        if (12..=22).contains(&state) {
            return Ok(());
        }
        let latched = self.word(FLAGS) & 8 != 0;
        self.set_bits(FLAGS, 8, false);
        if self.props().segmented_reload
            && matches!(state, 5 | 7)
            && self.cmd.buttons & 1 != 0
            && self.old.buttons & 1 == 0
        {
            self.set(STATE, state + 1);
        }
        if matches!(state, 1 | 2 | 5..=11) {
            return Ok(());
        }
        let requested = (latched || self.cmd.buttons & 0x10 != 0) && self.needs_reload();
        let moving_prone =
            self.word(FLAGS) & 1 != 0 && (self.cmd.forward_move != 0 || self.cmd.right_move != 0);
        let empty = self.word(clip(self.def())) == 0
            && self.word(ammo(self.def())) != 0
            && state != 3
            && !moving_prone;
        if requested || empty {
            self.begin_reload()?;
        }
        Ok(())
    }
    fn reload_transfer(&mut self) {
        let def = self.def();
        let opening = matches!(self.word(STATE), 7 | 8);
        let mut count = (def.clip_size - self.word(clip(def))).min(self.word(ammo(def)));
        let limit = if opening {
            def.handling.reload_start_add
        } else {
            def.handling.reload_ammo_add
        };
        if opening || limit != 0 {
            count = count.min(limit);
        }
        self.set(ammo(def), self.word(ammo(def)) - count);
        self.set(clip(def), self.word(clip(def)) + count);
    }
    fn apply_reload(&mut self) {
        if !self.props().bolt_action || !self.pending_bolt() {
            self.reload_transfer();
            return;
        }
        self.bolt(false);
        self.event(0xa2);
        if matches!(self.word(STATE), 7 | 8) && self.props().reload_start_add_time == 0 {
            return;
        }
        let total = self.add_time();
        let next = if self.props().rechamber_bolt_time < total {
            total - self.props().rechamber_bolt_time
        } else {
            total - 1
        };
        if self.word(TIME) == 0 || next < 1 {
            self.reload_transfer();
        } else {
            self.set(DELAY, next);
        }
    }
    pub(super) fn reload_step(&mut self, expired: bool) {
        if expired {
            self.apply_reload();
        }
        if self.word(TIME) != 0 {
            return;
        }
        let state = self.word(STATE);
        let opening = matches!(state, 7 | 8);
        if !opening {
            self.bolt(false);
        }
        let h = self.props();
        if self.needs_reload()
            && (if opening {
                state != 8 || self.word(clip(self.def())) == 0
            } else {
                h.segmented_reload && state != 6
            })
        {
            self.reload_mag();
            return;
        }
        if h.segmented_reload && h.reload_end_time != 0 {
            self.set(STATE, 9);
            self.anim(14);
            self.set(TIME, h.reload_end_time);
            self.event(0x99);
        } else {
            self.set(STATE, 0);
            self.anim(0);
        }
    }
    pub(super) fn rechamber(&mut self, expired: bool) -> bool {
        let st = self.word(STATE);
        if (12..=22).contains(&st) || !self.props().bolt_action || !self.pending_bolt() {
            return false;
        }
        if st == 4 && expired {
            self.bolt(false);
            self.event(0xa2);
            if self.word(TIME) != 0 {
                return true;
            }
        }
        if self.word(TIME) == 0 || (!matches!(st, 3 | 4 | 10 | 11) && self.word(DELAY) == 0) {
            if st == 4 {
                self.idle_anim();
                self.set(STATE, 0);
            } else if st == 0 {
                let h = self.props();
                self.anim(if self.float(FRACTION) > 0.75 { 7 } else { 4 });
                self.set(STATE, 4);
                self.set(TIME, h.rechamber_time);
                self.set(
                    DELAY,
                    if h.rechamber_bolt_time == 0 || h.rechamber_time <= h.rechamber_bolt_time {
                        1
                    } else {
                        h.rechamber_bolt_time
                    },
                );
                self.event(0xa1);
            }
        }
        false
    }
}
