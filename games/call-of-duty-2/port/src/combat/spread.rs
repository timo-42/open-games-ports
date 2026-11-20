//! Firing flags and hip spread at the start of PM_SingleStep, before movement.
use super::{
    WeaponSettings,
    inventory::{WEAPON, clip},
};
use crate::{assets::weapons::WeaponRegistry, snapshot::PlayerState, usercmd::UserCmd};
pub fn prepare_move(
    ps: &mut PlayerState,
    registry: &WeaponRegistry,
    cmd: &UserCmd,
    old: &UserCmd,
    ms: i32,
    settings: &WeaponSettings,
) {
    let Some(def) = registry.get(ps.words()[WEAPON] as usize) else {
        return;
    };
    let w = ps.words_mut();
    let flags = w[3];
    let pm_type = w[1] as i32;
    let mut entity_flags = w[0xa0 / 4];
    entity_flags &= !0x20_0040;
    if cmd.buttons & 0x40000 != 0 {
        entity_flags |= 0x20_0000;
    }
    if pm_type != 5
        && flags & 0x1000 == 0
        && matches!(w[0xd8 / 4], 0 | 3)
        && w[clip(def)] != 0
        && cmd.buttons & 1 != 0
    {
        entity_flags |= 0x40;
    }
    w[0xa0 / 4] = entity_flags;
    let stance = entity_flags;
    if pm_type < 6 && cmd.buttons & 0x4001 == 0 {
        w[3] &= !0x1000;
    }
    let h = &def.handling;
    let seconds = ms.clamp(1, 200) as f32 * 0.001;
    let airborne = w[0x60 / 4] == 1023 && pm_type != 1;
    let (decay, growth) = if h.hip_spread_decay_rate == 0. {
        (1., 0.)
    } else {
        let posture = if airborne {
            0.5
        } else if stance & 8 != 0 {
            h.hip_spread_prone_decay
        } else if stance & 4 != 0 {
            h.hip_spread_ducked_decay
        } else {
            1.
        };
        let mut growth = 0.;
        if f32::from_bits(w[0xdc / 4]) != 1. {
            if h.hip_spread_turn_add != 0. {
                for (next, prev) in cmd.angles[..2].iter().zip(&old.angles) {
                    let delta = next.wrapping_sub(*prev) as i16;
                    let degrees = (f32::from(delta) * 0.005493164).abs();
                    growth = (f64::from(growth)
                        + f64::from(0.01f32)
                            * f64::from(degrees)
                            * f64::from(h.hip_spread_turn_add)
                            / f64::from(seconds)) as f32;
                }
            }
            if h.hip_spread_move_add != 0. && (cmd.forward_move != 0 || cmd.right_move != 0) {
                let vx = f64::from(f32::from_bits(w[0x20 / 4]));
                let vy = f64::from(f32::from_bits(w[0x24 / 4]));
                if (vx * vx + vy * vy) as f32
                    > settings.spread_move_speed * settings.spread_move_speed
                {
                    growth += h.hip_spread_move_add;
                }
            }
            if airborne {
                growth += 1.28;
                growth += 1.28;
            }
        }
        (
            h.hip_spread_decay_rate * posture * seconds,
            growth * seconds,
        )
    };
    let scale = f64::from(f32::from_bits(w[0x5d4 / 4]));
    w[0x5d4 / 4] =
        ((scale + (f64::from(growth) - f64::from(decay)) * 255.).clamp(0., 255.) as f32).to_bits();
}
