//! G_PlayerCollisionPush separates the first overlapping playing client in slot order.
use super::{invalid, vector};
use crate::vm::GameHost;
use std::io;

pub(super) fn update(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let players = &mut host.world.players;
    let p = players
        .get(slot)
        .ok_or_else(|| invalid("missing player for collision"))?;
    let eligible = |p: &crate::player::Player| {
        p.session_state == 0
            && p.state.words()[3] & 0x80_0000 != 0
            && (p.state.words()[0x12c / 4] as i32) > 0
    };
    let collision_contents = |n: i32| matches!(n, 0x0200_0000 | 0x0400_0000);
    let speed = |p: &crate::player::Player| (p.state.words()[0x50 / 4] as i32) as f32;
    let moving =
        |p: &crate::player::Player| vector(p.state.words(), 0x20)[..2].iter().any(|v| *v != 0.);
    let ent = host
        .world
        .entities
        .get(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    if !eligible(p) || !collision_contents(ent.contents) {
        return Ok(());
    }
    let other = players.iter().find_map(|(s, other)| {
        if slot == s || !eligible(other) {
            return None;
        }
        let e = host.world.entities.get(s)?;
        if !collision_contents(e.contents)
            || (0..3).any(|a| {
                e.link.absmin[a] > ent.link.absmax[a] || ent.link.absmin[a] > e.link.absmax[a]
            })
        {
            return None;
        }
        let delta = [e.origin[0] - ent.origin[0], e.origin[1] - ent.origin[1]];
        let radius = f64::from(ent.maxs[0] + e.maxs[0]);
        if f64::from(delta[0]).powi(2) + f64::from(delta[1]).powi(2) > radius * radius {
            return None;
        }
        Some((s, delta, other))
    });
    let Some((other_slot, delta, other)) = other else {
        return Ok(());
    };
    let eject_speed = host
        .config
        .get("g_playerCollisionEjectSpeed")
        .map(crate::config::integer_prefix)
        .unwrap_or(25) as f32;
    let mut speeds = [
        if moving(p) { eject_speed } else { 0. },
        if moving(other) { eject_speed } else { 0. },
    ];
    if speeds[0] < 0.0001 && speeds[1] < 0.0001 {
        speeds = [speed(p), speed(other)];
    }
    let mut axis = delta.map(|d| (f64::from(d) + host.world.native_random.signed()) as f32);
    let length = ((f64::from(axis[0]).powi(2) + f64::from(axis[1]).powi(2)) as f32).sqrt();
    if length != 0. {
        axis = axis.map(|v| v * (1. / length));
    }
    for (number, multiplier) in [(other_slot, speeds[1]), (slot, -speeds[0])] {
        let w = players.get_mut(number)?.state.words_mut();
        w[0x20 / 4] = (axis[0] * multiplier).to_bits();
        w[0x24 / 4] = (axis[1] * multiplier).to_bits();
        w[0x10 / 4] = 300;
        w[3] |= 0x200;
    }
    host.world
        .entities
        .get_mut(slot)
        .expect("linked player")
        .contents = 0x0400_0000;
    Ok(())
}
