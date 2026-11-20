//! Pmove reads these weapon properties before stance and movement simulation.
use super::inventory::{WEAPON, clip, current};
use crate::{assets::weapons::WeaponRegistry, snapshot::PlayerState, usercmd::UserCmd};

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct AdsInput {
    pub move_speed_scale: f32,
    pub has_clip_ammo: bool,
    pub weapon_state: i32,
    pub aim_down_sight: bool,
}
impl AdsInput {
    pub fn from_player(ps: &PlayerState, registry: &WeaponRegistry) -> Self {
        let weapon = registry.get(ps.words()[WEAPON] as usize);
        Self {
            move_speed_scale: weapon.map_or(0., |def| def.handling.move_speed_scale),
            has_clip_ammo: weapon.is_some_and(|def| ps.words()[clip(def)] != 0),
            weapon_state: ps.words()[0xd8 / 4] as i32,
            aim_down_sight: registry
                .get(current(ps, registry))
                .is_some_and(|def| def.handling.aim_down_sight),
        }
    }
    /// PM_UpdateAdsFlag followed by PM_UpdateAdsSlowMoveFlag. PM_INTERMISSION
    /// does not call the latter routine.
    pub fn update(
        self,
        mut flags: u32,
        pm_type: i32,
        almost_ground: bool,
        cmd: &UserCmd,
        old_buttons: u32,
    ) -> u32 {
        flags &= !0x40;
        let st = self.weapon_state;
        let allowed = self.aim_down_sight
            && !(2..8).contains(&pm_type)
            && !(pm_type == 1 && almost_ground)
            && !matches!(st, 1 | 2 | 10..=18 | 21 | 22);
        let wanted = cmd.buttons & 0x1000 != 0 || matches!(st, 19 | 20);
        if allowed && wanted {
            if flags & 1 == 0 {
                flags |= 0x40;
            } else if old_buttons & 0x1000 == 0 || cmd.forward_move == 0 && cmd.right_move == 0 {
                flags |= 0x840;
            }
        }
        if pm_type != 5 {
            flags &= !0x100;
            if pm_type < 6
                && cmd.buttons & 0x1000 != 0
                && flags & 1 == 0
                && flags & 0x40 != 0
                && !(5..=9).contains(&st)
            {
                flags |= 0x100;
            }
        }
        flags
    }
}
