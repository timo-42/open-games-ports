//! BG and GScript weapon inventory. Inventory lives in the same player state
//! consumed by SV_UserMove and MSG_WriteDeltaPlayerState.
pub mod damage;
pub(crate) mod inventory;
pub mod projectile;
pub use inventory::selectable_weapons;
mod ads;
pub use ads::AdsInput;
mod frame;
mod methods;
mod recoil;
mod spread;
#[cfg(test)]
mod tests;
pub use frame::{
    WeaponSettings, force_ready, step, step_configured, step_with_animations, update_view_fraction,
};
pub use methods::dispatch;
pub use recoil::fire_kick;
pub use spread::prepare_move;

use crate::vm::spawn_fields::{float_prefix, parse_int};

#[derive(Clone, Debug, Default)]
pub struct WeaponHandling {
    pub kick: [recoil::KickRanges; 2],
    pub move_speed_scale: f32,
    pub fire_time: i32,
    pub fire_delay: i32,
    pub drop_time: i32,
    pub raise_time: i32,
    pub alt_drop_time: i32,
    pub alt_raise_time: i32,
    pub quick_drop_time: i32,
    pub quick_raise_time: i32,
    pub hold_fire_time: i32,
    pub fuse_time: i32,
    pub reload_time: i32,
    pub reload_empty_time: i32,
    pub reload_add_time: i32,
    pub reload_start_time: i32,
    pub reload_start_add_time: i32,
    pub reload_end_time: i32,
    pub rechamber_time: i32,
    pub rechamber_bolt_time: i32,
    pub melee_time: i32,
    pub melee_delay: i32,
    pub ads_trans_in_time: i32,
    pub ads_trans_out_time: i32,
    pub ads_reload_trans_time: i32,
    pub reload_ammo_add: i32,
    pub reload_start_add: i32,
    pub hip_gun_kick_reduced_kick_bullets: i32,
    pub ads_gun_kick_reduced_kick_bullets: i32,
    pub melee_damage: i32,
    pub semi_auto: bool,
    pub bolt_action: bool,
    pub segmented_reload: bool,
    pub no_partial_reload: bool,
    pub cook_off_hold: bool,
    pub ads_fire: bool,
    pub aim_down_sight: bool,
    pub rechamber_while_ads: bool,
    pub hip_spread_decay_rate: f32,
    pub hip_spread_ducked_decay: f32,
    pub hip_spread_prone_decay: f32,
    pub hip_spread_turn_add: f32,
    pub hip_spread_move_add: f32,
    pub hip_spread_fire_add: f32,
    pub clip_only: bool,
    pub shared_ammo_cap: i32,
}

impl WeaponHandling {
    pub(crate) fn parse<'a>(raw: impl Fn(&str) -> &'a [u8]) -> Self {
        let decimal = |key| {
            float_prefix(raw(key))
                .and_then(|(s, _)| s.parse::<f64>().ok())
                .unwrap_or(0.)
        };
        let time = |key| (f64::from(decimal(key) as f32) * 1000.) as i32;
        Self {
            kick: ["hip", "ads"].map(|prefix| {
                recoil::KickRanges::parse(prefix, &|key| {
                    float_prefix(raw(key))
                        .and_then(|(s, _)| s.parse::<f64>().ok())
                        .unwrap_or(0.) as f32
                })
            }),
            move_speed_scale: decimal("movespeedscale") as f32,
            fire_time: time("firetime"),
            fire_delay: time("firedelay"),
            drop_time: time("droptime"),
            raise_time: time("raisetime"),
            alt_drop_time: time("altdroptime"),
            alt_raise_time: time("altraisetime"),
            quick_drop_time: time("quickdroptime"),
            quick_raise_time: time("quickraisetime"),
            hold_fire_time: time("holdfiretime"),
            fuse_time: time("fusetime"),
            reload_time: time("reloadtime"),
            reload_empty_time: time("reloademptytime"),
            reload_add_time: time("reloadaddtime"),
            reload_start_time: time("reloadstarttime"),
            reload_start_add_time: time("reloadstartaddtime"),
            reload_end_time: time("reloadendtime"),
            rechamber_time: time("rechambertime"),
            rechamber_bolt_time: time("rechamberbolttime"),
            melee_time: time("meleetime"),
            melee_delay: time("meleedelay"),
            ads_trans_in_time: time("adstransintime"),
            ads_trans_out_time: time("adstransouttime"),
            ads_reload_trans_time: time("adsreloadtranstime"),
            reload_ammo_add: parse_int(raw("reloadammoadd")),
            reload_start_add: parse_int(raw("reloadstartadd")),
            hip_gun_kick_reduced_kick_bullets: parse_int(raw("hipgunkickreducedkickbullets")),
            ads_gun_kick_reduced_kick_bullets: parse_int(raw("adsgunkickreducedkickbullets")),
            melee_damage: parse_int(raw("meleedamage")),
            semi_auto: parse_int(raw("semiauto")) != 0,
            bolt_action: parse_int(raw("boltaction")) != 0,
            segmented_reload: parse_int(raw("segmentedreload")) != 0,
            no_partial_reload: parse_int(raw("nopartialreload")) != 0,
            cook_off_hold: parse_int(raw("cookoffhold")) != 0,
            ads_fire: parse_int(raw("adsfire")) != 0,
            aim_down_sight: parse_int(raw("aimdownsight")) != 0,
            rechamber_while_ads: parse_int(raw("rechamberwhileads")) != 0,
            hip_spread_decay_rate: decimal("hipspreaddecayrate") as f32,
            hip_spread_ducked_decay: decimal("hipspreadduckeddecay") as f32,
            hip_spread_prone_decay: decimal("hipspreadpronedecay") as f32,
            hip_spread_turn_add: decimal("hipspreadturnadd") as f32,
            hip_spread_move_add: decimal("hipspreadmoveadd") as f32,
            hip_spread_fire_add: decimal("hipspreadfireadd") as f32,

            clip_only: parse_int(raw("cliponly")) != 0,
            shared_ammo_cap: parse_int(raw("sharedammocap")),
        }
    }
}
