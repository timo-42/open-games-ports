use super::*;
const BREATH: usize = 0x5b8 / 4;
const BREATH_SCALE: usize = 0x5b4 / 4;
const ADS_DELAY: usize = 0xe0 / 4;
impl<'a> Frame<'a> {
    fn active_def(&self) -> &'a WeaponDefinition {
        self.registry
            .get(current(self.ps, self.registry))
            .expect("registered player weapon")
    }
    pub(super) fn ads_in(&self, def: &WeaponDefinition) -> f32 {
        if def.handling.ads_trans_in_time < 1 {
            f32::from_bits(0x3b5a740e)
        } else {
            1. / def.handling.ads_trans_in_time as f32
        }
    }
    pub(super) fn view_fraction(&mut self) {
        let def = self.active_def();
        let h = &def.handling;
        let state = self.word(STATE);
        let damaged = self.settings.exit_scope_on_damage
            && self.word(0x128 / 4) != 0
            && def.overlay_reticle != 0;
        if damaged {
            self.event(0x95);
            self.set_bits(FLAGS, 0x40, false);
        }
        if damaged || !h.aim_down_sight {
            self.set_float(FRACTION, 0.);
            self.set(ADS_DELAY, 0);
        } else {
            let reload_lock = if h.segmented_reload {
                matches!(state, 5..=8) || (state == 9 && self.word(TIME) > h.ads_reload_trans_time)
            } else {
                state == 5 && self.word(TIME) > h.ads_reload_trans_time
            };
            let mut active = (!reload_lock
                && (state != 4 || h.rechamber_while_ads)
                && self.word(FLAGS) & 0x40 != 0)
                || (h.ads_fire && self.word(DELAY) != 0 && state == 3);
            let frac = self.float(FRACTION);
            if frac == 1. && !active && self.settings.ads_exit_delay > 0 {
                if self.word(ADS_DELAY) == 0 {
                    self.set(
                        ADS_DELAY,
                        self.cmd
                            .server_time
                            .wrapping_add(self.settings.ads_exit_delay),
                    );
                }
                if self.cmd.server_time < self.word(ADS_DELAY) {
                    active = true;
                } else {
                    self.set(ADS_DELAY, 0);
                }
            } else {
                self.set(ADS_DELAY, 0);
            }
            if (active && frac != 1.) || (!active && frac != 0.) {
                let rate = if active {
                    self.ads_in(def)
                } else if h.ads_trans_out_time < 1 {
                    -f32::from_bits(0x3b03126f)
                } else {
                    -1. / h.ads_trans_out_time as f32
                };
                self.set_float(
                    FRACTION,
                    (f64::from(frac) + f64::from(rate) * f64::from(self.ms)).clamp(0., 1.) as f32,
                );
            }
        }
    }
    pub(super) fn aim(&mut self) {
        self.view_fraction();
        let def = self.active_def();
        let settings = self.settings;
        if settings.breath_hold_ms < 1 {
            self.set_bits(FLAGS, 0x4000, false);
            self.set_float(BREATH_SCALE, 1.);
            self.set(BREATH, 0);
            return;
        }
        if self.float(FRACTION) != 1.
            || def.overlay_reticle == 0
            || def.weapon_class == 9
            || self.cmd.buttons & 0x8000 == 0
        {
            self.set_bits(FLAGS, 0x4000, false);
        } else if self.word(BREATH) == 0 {
            self.set_bits(FLAGS, 0x4000, true);
        }
        let cap = settings.breath_gasp_ms + settings.breath_hold_ms;
        let mut held = self.word(FLAGS) & 0x4000 != 0;
        let mut elapsed = (self.word(BREATH) + if held { self.ms } else { -self.ms }).max(0);
        if held && elapsed > settings.breath_hold_ms {
            held = false;
            elapsed = cap;
            self.set_bits(FLAGS, 0x4000, false);
        }
        self.set(BREATH, elapsed);
        let (scale, rate) = if held {
            (0., settings.breath_hold_lerp)
        } else {
            (
                ((f64::from(settings.breath_gasp_scale) - 1.)
                    * f64::from(elapsed as f32 / cap as f32)
                    + 1.) as f32,
                settings.breath_gasp_lerp,
            )
        };
        let target = ((f64::from(scale) - 1.) * f64::from(self.float(FRACTION)) + 1.) as f32;
        let delta = target - self.float(BREATH_SCALE);
        let change =
            (f64::from(rate) * f64::from(delta) * f64::from(self.ms as f32 * 0.001)) as f32;
        self.set_float(
            BREATH_SCALE,
            if delta.abs() <= 0.001 || delta.abs() < change.abs() {
                target
            } else {
                self.float(BREATH_SCALE) + change
            },
        );
    }
    pub(super) fn breath_fire(&mut self) {
        let def = self.active_def();
        if self.float(FRACTION) == 1. && def.overlay_reticle != 0 && def.weapon_class != 9 {
            if self.word(BREATH) < self.settings.breath_hold_ms {
                self.set(
                    BREATH,
                    (self.word(BREATH) + self.settings.breath_fire_delay)
                        .min(self.settings.breath_hold_ms),
                );
            }
            self.set_bits(FLAGS, 0x4000, false);
        }
    }
}
