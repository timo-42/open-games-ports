//! Free spectators and archived killcam targets.
use super::{invalid, set_angles, vector, view_origin, write_vector};
use crate::{
    snapshot::PlayerState,
    vm::{GameHost, value::Value},
};
use std::io;

fn cached_player(target: i32, age: &mut i32, host: &mut GameHost<'_>) -> Option<(PlayerState, u8)> {
    let slot = u8::try_from(target).ok().filter(|&s| s < 64)?;
    let fps = host
        .config
        .get("sv_fps")
        .and_then(|v| v.parse().ok())
        .unwrap_or(20);
    if let Some(frame) = host.world.archive.lookup(age, fps) {
        let (ps, cs) = frame.player_state(slot, host.time_ms)?;
        return Some((ps, cs.field("team")? as u8));
    }
    if *age > 0 {
        return None;
    }
    let p = host
        .world
        .players
        .get(u16::from(slot))
        .filter(|p| p.connected)?;
    Some((crate::snapshot::archive::copy_player(&p.state)?, p.team()))
}

pub(super) fn follow(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing spectator"))?;
    let mut manual = p.spectator_client >= 0;
    let mut target = if manual {
        p.spectator_client
    } else {
        p.last_followed_client
    };
    let mask = p.no_spectate;
    let allowed = |team: u8| mask & (1 << team) == 0;
    let mut age = p.archive_time;
    let offset = p.input.ps_offset_ms;
    let mut selected = None;
    if manual {
        loop {
            age = age.max(0);
            let mut query = age.saturating_sub(offset);
            if let Some((ps, team)) = cached_player(target, &mut query, host)
                && allowed(team)
            {
                selected = Some((target, ps));
                break;
            }
            if age == 0 {
                break;
            }
            age = age.saturating_sub(50);
        }
        let p = host.world.players.get_mut(slot)?;
        p.archive_time = age;
        if selected.is_none() {
            p.spectator_client = -1;
            p.last_followed_client = -1;
            target = -1;
            manual = false;
        }
    }
    if selected.is_none() {
        if target < 0 && !allowed(4) {
            follow_cycle(slot, 1, host)?;
            let p = host.world.players.get_mut(slot)?;
            target = p.last_followed_client;
            age = p.archive_time;
        }
        if target >= 0 {
            let mut query = age.saturating_add(offset);
            selected = cached_player(target, &mut query, host)
                .filter(|(_, team)| allowed(*team))
                .map(|(ps, _)| (target, ps));
        }
    }
    if let Some((target, mut ps)) = selected {
        let p = host.world.players.get_mut(slot)?;
        let kept = p.state.words()[0xa0 / 4] & 0x10_0000;
        let words = ps.words_mut();
        words[0xa0 / 4] = (words[0xa0 / 4] & !0x10_0000) | kept;
        words[0xc / 4] = (words[0xc / 4] & !0x80_0000) | 0x40_0000;
        words[0xc / 4] &= !0x0300_0000;
        if !manual {
            words[0xc / 4] |= 0x0100_0000;
            if allowed(4) {
                words[0xc / 4] |= 0x0200_0000;
            }
        }
        p.last_followed_client = target;
        host.world
            .hud
            .write_player_state(slot, p.team(), 2, &mut ps);
        p.state = ps;
    } else {
        stop_following(slot, host)?;
        let p = host.world.players.get_mut(slot)?;
        let words = p.state.words_mut();
        words[0xc / 4] &= !0x0300_0000;
        if mask & 7 != 7 {
            words[0xc / 4] |= 0x0100_0000;
        }
    }
    Ok(())
}

pub(in crate::player) fn stop_following(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing spectator"))?;
    let start = if p.state.words()[3] & 0x40_0000 != 0 {
        Some(view_origin(host, &p.state)?)
    } else {
        None
    };
    let p = host.world.players.get_mut(slot)?;
    p.spectator_client = -1;
    p.last_followed_client = -1;
    let words = p.state.words_mut();
    if let Some(start) = start {
        let mut angles = vector(words, 0xe8);
        let [forward, _, up] = crate::vm::math::axes(angles);
        let behind = crate::movement::ma(start, -40., forward);
        let end = crate::movement::ma(behind, 10., up);
        let fraction = host
            .collision
            .box_trace(
                &host.world.entities,
                start,
                end,
                [-8.; 3],
                [8.; 3],
                1023,
                0x810011,
            )
            .fraction;
        let origin = crate::movement::lerp(start, end, fraction);
        write_vector(words, 0x14, origin);
        words[0xcc / 4] = u32::from(slot);
        words[0xa0 / 4] &= !0x300;
        words[0x590 / 4] = 0;
        words[0x594 / 4] = 1023;
        words[0xc / 4] &= !0x40_0040;
        words[0xdc / 4] = 0;
        words[0x5d8 / 4..=0x5e0 / 4].fill(0);
        angles[0] += 15.;
        let angles = set_angles(&mut p.state, &p.command, angles);
        if let Some(e) = host.world.entities.get_mut(slot) {
            super::stationary_origin(e, origin);
            e.angles = angles;
            let object = e.object;
            host.world
                .set_field(object, "origin", Value::Vector(origin))?;
            host.world
                .set_field(object, "angles", Value::Vector(angles))?;
        }
    }
    Ok(())
}

pub fn follow_cycle(slot: u16, direction: i32, host: &mut GameHost<'_>) -> io::Result<bool> {
    let p = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing spectator"))?;
    if !matches!(direction, 1 | -1) {
        return Err(invalid("invalid follow direction"));
    }
    if p.session_state != 2 || p.spectator_client >= 0 {
        return Ok(false);
    }
    let mask = p.no_spectate;
    let count = host
        .config
        .get("sv_maxclients")
        .and_then(|v| v.parse::<i32>().ok())
        .unwrap_or(20)
        .clamp(1, 64);
    let first = p.last_followed_client.max(0);
    let mut age = p.archive_time;
    for i in 1..=count {
        let next = (first + i * direction).rem_euclid(count);
        if let Some((_, team)) = cached_player(next, &mut age, host)
            && mask & (1 << team) == 0
        {
            let p = host.world.players.get_mut(slot)?;
            p.last_followed_client = next;
            p.archive_time = age;
            return Ok(true);
        }
    }
    host.world.players.get_mut(slot)?.archive_time = age;
    Ok(false)
}
