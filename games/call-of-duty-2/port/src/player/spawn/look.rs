//! G_UpdateLookAtTrigger, including the second ray through a trigger_lookat.
use super::{invalid, vector, view_origin};
use crate::vm::{GameHost, value::Value};
use std::io;

pub(super) fn update(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    p.input.look_at = None;
    p.state.words_mut()[3] &= !0x30_0000;
    let p = host.world.players.get(slot).expect("active player");
    let own_object = p.object;
    let w = p.state.words();
    let origin = view_origin(host, &p.state)?;
    let angles = vector(w, 0xe8);
    let [forward, _, _] = crate::vm::math::axes(angles);
    let end = crate::movement::ma(origin, 15000., forward);
    let own_weapon = w[0xd4 / 4];
    let weapon = if w[0xa0 / 4] & 0x300 == 0 {
        own_weapon
    } else {
        host.world
            .entities
            .get(w[0x594 / 4] as u16)
            .and_then(|e| e.state.field("weapon"))
            .unwrap_or(0)
    };
    let def = host
        .weapons
        .get(weapon as usize)
        .ok_or_else(|| invalid("unknown crosshair weapon"))?;
    let rifle = own_weapon != 0 && def.shot.rifle;
    let max_range = def.enemy_crosshair_range;
    let team = p.team();
    let trace = |host: &GameHost<'_>, mask: u32| {
        host.collision.trace_point(
            &host.world.entities,
            host.models,
            origin,
            end,
            slot,
            mask,
            rifle,
        )
    };
    // The dedicated server does not draw client FX occluders.
    let mut hit = trace(host, 0x2280_2801)?;
    let Some(target) = host
        .world
        .entities
        .get(hit.entity)
        .filter(|_| hit.entity < 1022)
    else {
        return Ok(());
    };
    let mut look_at = None;
    if matches!(host.world.field(target.object,"classname")?,Value::String(name) if name==b"trigger_lookat")
    {
        look_at = Some(target.object);
        host.world.entities.queue_trigger(target.object, own_object);
        hit = trace(host, 0x0280_2801)?;
    }
    let mut flags = 0;
    if let Some(target) = host.world.entities.get(hit.entity)
        && target.state.field("eType") == Some(1)
        && hit.surface_flags & 0x10 == 0
        && let Some(other) = host.world.players.get(hit.entity)
    {
        let dist_sq = (0..3)
            .map(|i| f64::from(target.origin[i] - origin[i]).powi(2))
            .sum::<f64>() as f32;
        let within = |limit: f32| f64::from(dist_sq) < f64::from(limit).powi(2);
        if team != 0 && team == other.team() {
            let limit = |name, default| {
                host.config
                    .get(name)
                    .and_then(|v| v.parse::<f32>().ok())
                    .unwrap_or(default)
            };
            if within(limit("g_friendlyNameDist", 15000.)) {
                look_at.get_or_insert(target.object);
            }
            if within(limit("g_friendlyfireDist", 256.)) {
                flags = 0x10_0000;
            }
        } else if within(max_range) {
            look_at.get_or_insert(target.object);
            flags = 0x20_0000;
        }
    }
    let p = host.world.players.get_mut(slot)?;
    p.input.look_at = look_at;
    p.state.words_mut()[3] |= flags;
    Ok(())
}
