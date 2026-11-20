//! BG_WeaponRandomFireKick, called by the server's player event handler.
use crate::{assets::weapons::WeaponRegistry, snapshot::PlayerState};

#[derive(Clone, Copy, Debug, Default)]
pub struct KickRanges {
    view_pitch: [f32; 2],
    view_yaw: [f32; 2],
    gun_pitch: [f32; 2],
    gun_yaw: [f32; 2],
    reduced_percent: f32,
}
impl KickRanges {
    pub(super) fn parse(prefix: &str, read: &impl Fn(&str) -> f32) -> Self {
        let range = |key| {
            [
                read(&format!("{prefix}{key}min")),
                read(&format!("{prefix}{key}max")),
            ]
        };
        Self {
            view_pitch: range("viewkickpitch"),
            view_yaw: range("viewkickyaw"),
            gun_pitch: range("gunkickpitch"),
            gun_yaw: range("gunkickyaw"),
            reduced_percent: read(&format!("{prefix}gunkickreducedkickpercent")),
        }
    }
}
/// Advance the level's shared libc random sequence by four draws. The caller
/// retains the accumulated gun angles; the return value is the view kick.
pub fn fire_kick(
    ps: &PlayerState,
    registry: &WeaponRegistry,
    angles: &mut [f32; 3],
    mut draw: impl FnMut() -> u32,
) -> [f32; 3] {
    let id = super::inventory::current(ps, registry);
    let def = registry.get(id).expect("registered player weapon");
    let blend = f32::from_bits(ps.words()[0xdc / 4]);
    let view = &def.handling.kick[usize::from(blend == 1.)];
    let gun = &def.handling.kick[usize::from(blend > 0.)];
    let scale = if ps.words()[0x40 / 4] as i32 > 0 {
        view.reduced_percent * 0.01
    } else {
        1.
    };
    let mut sample = |[low, high]: [f32; 2]| {
        (f64::from(low) + f64::from(high - low) * f64::from(draw()) / 2147483648.) as f32
    };
    let pitch = -sample(view.view_pitch) * scale;
    let yaw = sample(view.view_yaw) * scale;
    for (angle, range) in angles.iter_mut().zip([gun.gun_pitch, gun.gun_yaw]) {
        *angle += sample(range) * scale;
    }
    [pitch, yaw, -0.5 * yaw]
}
