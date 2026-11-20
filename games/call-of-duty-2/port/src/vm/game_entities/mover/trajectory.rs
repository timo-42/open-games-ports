//! Script mover position and angle trajectories, including acceleration and deceleration.
use crate::entity_delta::EntityState;
use crate::vm::items::motion::{set_vector, vector};

pub(super) type Vec3 = [f32; 3];

pub(super) fn add(a: Vec3, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| a[i] + b[i])
}
pub(super) fn subtract(a: Vec3, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| a[i] - b[i])
}
fn scale(v: Vec3, factor: f32) -> Vec3 {
    v.map(|x| x * factor)
}
fn advance(base: Vec3, amount: f32, direction: Vec3) -> Vec3 {
    std::array::from_fn(|i| {
        (f64::from(base[i]) + f64::from(amount) * f64::from(direction[i])) as f32
    })
}
pub(super) fn length(v: Vec3) -> f32 {
    (v.into_iter()
        .map(|x| f64::from(x) * f64::from(x))
        .sum::<f64>() as f32)
        .sqrt()
}
fn normalize(v: Vec3) -> Vec3 {
    let len = length(v);
    if len == 0. {
        [0.; 3]
    } else {
        scale(v, 1. / len)
    }
}
fn milliseconds(seconds: f32) -> i32 {
    let ms = f64::from(seconds) * 1000.;
    if ms >= 2147483648. || ms < -2147483648. || !ms.is_finite() {
        i32::MIN
    } else {
        ms as i32
    }
}

#[derive(Clone, Copy, Debug)]
pub(crate) enum Channel {
    Position,
    Angles,
}
impl Channel {
    fn fields(self) -> [&'static str; 5] {
        match self {
            Self::Position => [
                "pos.trType",
                "pos.trTime",
                "pos.trDuration",
                "pos.trBase",
                "pos.trDelta",
            ],
            Self::Angles => [
                "apos.trType",
                "apos.trTime",
                "apos.trDuration",
                "apos.trBase",
                "apos.trDelta",
            ],
        }
    }
}

#[derive(Clone, Copy, Debug, Default)]
pub(crate) struct Trajectory {
    pub(super) kind: i32,
    pub(super) start: i32,
    duration: i32,
    base: Vec3,
    delta: Vec3,
}
impl Trajectory {
    pub(crate) fn read(state: &EntityState, channel: Channel) -> Self {
        let [kind, start, duration, base, delta] = channel.fields();
        let word = |key| state.field(key).expect("trajectory field") as i32;
        Self {
            kind: word(kind),
            start: word(start),
            duration: word(duration),
            base: vector(state, base),
            delta: vector(state, delta),
        }
    }
    pub(super) fn write(self, state: &mut EntityState, channel: Channel) {
        let [kind, start, duration, base, delta] = channel.fields();
        state.set_field(kind, self.kind as u32);
        state.set_field(start, self.start as u32);
        state.set_field(duration, self.duration as u32);
        set_vector(state, base, self.base);
        set_vector(state, delta, self.delta);
    }
    pub(super) fn gravity(start: i32, seconds: f32, base: Vec3, delta: Vec3) -> Self {
        Self {
            kind: 5,
            start,
            duration: milliseconds(seconds),
            base,
            delta,
        }
    }
    pub(super) fn due(self, now: i32) -> bool {
        self.kind != 0 && self.start.wrapping_add(self.duration) <= now
    }
    /// BG_EvaluateTrajectory, with the single-precision storage points of the x87 original.
    pub(crate) fn at(self, now: i32) -> Vec3 {
        let end = self.start.wrapping_add(self.duration);
        let time = if matches!(self.kind, 3 | 7 | 8) {
            now.min(end)
        } else {
            now
        };
        let elapsed = time.wrapping_sub(self.start);
        let dt = (f64::from(elapsed) * f64::from(0.001f32)) as f32;
        match self.kind {
            0 | 1 | 6 => self.base,
            2 => advance(self.base, dt, self.delta),
            3 => advance(self.base, dt.max(0.), self.delta),
            4 => {
                let ratio = (f64::from(elapsed) / f64::from(self.duration)) as f32;
                let wave = (f64::from(ratio) * std::f64::consts::TAU).sin() as f32;
                advance(self.base, wave, self.delta)
            }
            5 => {
                let mut result = advance(self.base, dt, self.delta);
                result[2] = (f64::from(result[2]) - f64::from(dt) * 400. * f64::from(dt)) as f32;
                result
            }
            7 | 8 => {
                let ratio = (f64::from(length(self.delta))
                    / (f64::from(0.001f32) * f64::from(self.duration)))
                    as f32;
                let distance = (f64::from(ratio) * 0.5 * f64::from(dt) * f64::from(dt)) as f32;
                let (base, amount) = if self.kind == 7 {
                    (self.base, distance)
                } else {
                    (advance(self.base, dt, self.delta), -distance)
                };
                advance(base, amount, normalize(self.delta))
            }
            _ => unreachable!("registered trajectory type"),
        }
    }
}

/// Saved G_ScriptMover_PlanMove/PlanRotate data for the remaining phases.
#[derive(Clone, Copy, Debug, Default)]
pub(super) struct Plan {
    speed: f32,
    cruise: f32,
    decel: f32,
    accel_end: Vec3,
    cruise_end: Vec3,
    destination: Vec3,
}

impl Plan {
    #[allow(clippy::too_many_arguments)]
    pub(super) fn start(
        tr: &mut Trajectory,
        now: i32,
        current: Vec3,
        target: Vec3,
        velocity: bool,
        total: f32,
        accel: f32,
        decel: f32,
    ) -> Self {
        let distance = subtract(target, current);
        let base = if tr.kind == 0 { current } else { tr.at(now) };
        let cruise = ((f64::from(total) - f64::from(accel)) - f64::from(decel)) as f32;
        let mut p = Self {
            cruise,
            decel,
            destination: target,
            ..Self::default()
        };
        if accel == 0. && decel == 0. {
            let duration = milliseconds(total);
            let delta = if velocity {
                target
            } else {
                scale(distance, 1000. / duration as f32)
            };
            *tr = Trajectory {
                kind: 3,
                start: now,
                duration,
                base,
                delta,
            };
            if velocity {
                p.destination = tr.at(now.wrapping_add(duration));
            }
            return p;
        }
        let delta = if velocity {
            p.speed = length(target);
            target
        } else {
            p.speed = (2. * f64::from(length(distance))
                / (2. * f64::from(total) - f64::from(accel) - f64::from(decel)))
                as f32;
            scale(normalize(distance), p.speed)
        };
        let (kind, duration, initial_delta) = if accel != 0. {
            (7, milliseconds(accel), delta)
        } else if cruise != 0. {
            let duration = milliseconds(cruise);
            (
                3,
                duration,
                if velocity {
                    delta
                } else {
                    scale(scale(delta, cruise), 1000. / duration as f32)
                },
            )
        } else {
            (8, milliseconds(decel), delta)
        };
        *tr = Trajectory {
            kind,
            start: now,
            duration,
            base,
            delta: initial_delta,
        };
        p.accel_end = if accel != 0. {
            tr.at(now.wrapping_add(duration))
        } else {
            base
        };
        p.cruise_end = advance(p.accel_end, cruise, delta);
        if velocity {
            p.destination = if decel != 0. {
                let tail = Trajectory {
                    kind: 8,
                    start: now,
                    duration: milliseconds(decel),
                    base: p.cruise_end,
                    delta,
                };
                tail.at(now.wrapping_add(tail.duration))
            } else {
                p.cruise_end
            };
        }
        p
    }

    /// Phase starts are relative to the actual frame that ended the preceding phase.
    pub(super) fn next(self, tr: &mut Trajectory, now: i32) -> bool {
        let cruise_ms = milliseconds(self.cruise);
        if tr.kind == 7 && cruise_ms > 0 {
            *tr = Trajectory {
                kind: 3,
                start: now,
                duration: cruise_ms,
                base: self.accel_end,
                delta: scale(
                    subtract(self.cruise_end, self.accel_end),
                    1000. / cruise_ms as f32,
                ),
            };
        } else if matches!(tr.kind, 7 | 3) && self.decel > 0. {
            *tr = Trajectory {
                kind: 8,
                start: now,
                duration: milliseconds(self.decel),
                base: self.cruise_end,
                delta: scale(
                    normalize(subtract(self.destination, self.cruise_end)),
                    self.speed,
                ),
            };
        } else {
            tr.base = if tr.kind == 5 {
                tr.at(now)
            } else {
                self.destination
            };
            tr.kind = 0;
            tr.start = now;
            return true;
        }
        false
    }
}
