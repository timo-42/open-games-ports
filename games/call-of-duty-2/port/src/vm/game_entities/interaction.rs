//! Player use target selection, button holds, and native/script notifications.
use super::{touch, turret_use};
use crate::{
    script::ScriptProgram,
    vm::{GameHost, fiber::FiberVm, invalid, items, value::Value},
};
use std::io;
type Vec3 = [f32; 3];

#[derive(Debug)]
pub(crate) enum Action {
    Notify {
        object: u32,
        player: u32,
        event: &'static str,
    },
    Item {
        slot: u16,
        target: u16,
    },
    Fire {
        slot: u16,
        turret: u16,
        muzzle: Vec3,
        spread: f32,
    },
}
pub fn dispatch(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
) -> io::Result<()> {
    for action in std::mem::take(&mut host.world.interactions) {
        match action {
            Action::Notify {
                object,
                player,
                event,
            } => vm.notify(object, event, &[Value::Object(player)]),
            Action::Item { slot, target } => items::touch(vm, host, slot, target, false)?,
            Action::Fire {
                slot,
                turret,
                muzzle,
                spread,
            } => {
                crate::combat::damage::turret_shot(vm, program, host, slot, turret, muzzle, spread)?
            }
        }
    }
    Ok(())
}

fn hint_slot(host: &GameHost<'_>, text: &str) -> u32 {
    if text.is_empty() {
        return u32::MAX;
    }
    (0..32)
        .find(|&i| host.strings.get(0x4fe + i) == Some(text.as_bytes()))
        .map_or(u32::MAX, |i| i as u32)
}

#[derive(Clone, Copy)]
struct Candidate {
    score: f32,
    number: u16,
    kind: u32,
    trigger: bool,
    touch_only: bool,
}

// G_CompareKeyedEntity truncates the score difference to int. Its ties are
// non-transitive, so use the original GNU qsort merge order explicitly.
fn rank(entries: &mut [Candidate], scratch: &mut [Candidate]) {
    if entries.len() < 2 {
        return;
    }
    let middle = entries.len() / 2;
    let (left, right) = entries.split_at_mut(middle);
    rank(left, &mut scratch[..middle]);
    rank(right, &mut scratch[middle..]);
    let (mut a, mut b) = (0, 0);
    for out in scratch.iter_mut() {
        if b == right.len() || (a < left.len() && (left[a].score - right[b].score) as i32 <= 0) {
            *out = left[a];
            a += 1;
        } else {
            *out = right[b];
            b += 1;
        }
    }
    entries.copy_from_slice(scratch);
}

fn select(slot: u16, host: &GameHost<'_>) -> io::Result<(u16, u32, u32)> {
    let none = (1023, 0, u32::MAX);
    let p = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing player"))?;
    let w = p.state.words();
    if !matches!(host.world.field(p.object, "health")?, Value::Int(h) if h > 0)
        || (17..=22).contains(&w[0xd8 / 4])
    {
        return Ok(none);
    }
    if w[0xa0 / 4] & 0x300 != 0 {
        let number = w[0x594 / 4] as u16;
        if let Some(id) = host
            .world
            .entities
            .get(number)
            .and_then(|t| t.state.field("weapon"))
            && let Some(def) = host.weapons.get(id as usize)
            && !def.drop_hint_string.is_empty()
        {
            return Ok((1023, id + 4, hint_slot(host, &def.drop_hint_string)));
        }
        return Ok(none);
    }
    if w[3] & 4 != 0 {
        return Ok(none);
    }
    let origin: Vec3 = std::array::from_fn(|i| f32::from_bits(w[0x14 / 4 + i]));
    let low = std::array::from_fn(|i| origin[i] + f32::from_bits(w[0x56c / 4 + i]));
    let high = std::array::from_fn(|i| origin[i] + f32::from_bits(w[0x578 / 4 + i]));
    let eye = crate::player::spawn::view_origin(host, &p.state)?;
    let angles = std::array::from_fn(|i| f32::from_bits(w[0xe8 / 4 + i]));
    let fwd = super::frame::basis(angles)[0];
    let extent = [192., 192., 96.];
    let candidates = touch::area_entities(
        &host.world.entities,
        host.collision,
        std::array::from_fn(|i| eye[i] - extent[i]),
        std::array::from_fn(|i| eye[i] + extent[i]),
        0x20_0000,
    );
    let mut ranked = Vec::new();
    let mut rejected = 0;
    for target in candidates.into_iter().filter(|n| *n != slot) {
        let e = host.world.entities.get(target).expect("area entity");
        let class = host.world.field(e.object, "classname")?;
        let is_trigger =
            matches!(&class,Value::String(s) if s==b"trigger_use" || s==b"trigger_use_touch");
        let touch_only = matches!(&class,Value::String(s) if s==b"trigger_use_touch");
        let kind = e.state.field("eType").expect("etype");
        let key = if touch_only {
            if !(0..3).all(|i| e.link.absmin[i] <= high[i] && low[i] <= e.link.absmax[i])
                || !touch::entity_contact(host.collision, e, low, high)
            {
                continue;
            }
            -256.
        } else {
            let center: Vec3 = std::array::from_fn(|i| (e.link.absmin[i] + e.link.absmax[i]) * 0.5);
            let d: Vec3 = std::array::from_fn(|i| center[i] - eye[i]);
            let len = d
                .into_iter()
                .map(|v| f64::from(v).powi(2))
                .sum::<f64>()
                .sqrt() as f32;
            if len > 128. {
                continue;
            }
            let dot = if len > 0. {
                (0..3)
                    .map(|i| f64::from(d[i] / len) * f64::from(fwd[i]))
                    .sum::<f64>() as f32
            } else {
                0.
            };
            let mut score = (1. - (dot + 1.) * 0.5) * 256.;
            if is_trigger {
                score -= 256.;
            }
            if kind == 3 && !items::can_grab(&e.state, &p.state, host.weapons, false) {
                score += 10000.;
                rejected += 1;
            }
            score + len
        };
        ranked.push(Candidate {
            score: key,
            number: target,
            kind,
            trigger: is_trigger,
            touch_only,
        });
    }
    let mut scratch = ranked.clone();
    rank(&mut ranked, &mut scratch);
    ranked.truncate(ranked.len() - rejected);
    let mut blocked = 0;
    for c in ranked.iter_mut().filter(|c| !c.touch_only) {
        let e = host
            .world
            .entities
            .get(c.number)
            .expect("collected use candidate");
        let center = std::array::from_fn(|i| (e.link.absmin[i] + e.link.absmax[i]) * 0.5);
        let point = if c.kind == 9 {
            turret_use::tag_world(e, host.models, "tag_aim").unwrap_or(center)
        } else {
            center
        };
        if !host.collision.point_clear(
            &host.world.entities,
            host.models,
            eye,
            point,
            slot,
            0x11,
            false,
        )? {
            c.score += 10000.;
            blocked += 1;
        }
    }
    scratch.truncate(ranked.len());
    rank(&mut ranked, &mut scratch);
    ranked.truncate(ranked.len() - blocked);
    for Candidate {
        number: target,
        kind,
        trigger,
        ..
    } in ranked
    {
        let e = host
            .world
            .entities
            .get(target)
            .expect("collected use candidate");
        match kind {
            3 => {
                let id = e.state.field("index").expect("registered network field") as usize;
                if host.weapons.get(id).is_some_and(|d| d.weapon_type != 1)
                    && !crate::combat::inventory::owned(&p.state, id)
                {
                    return Ok((target, id as u32 + 4, u32::MAX));
                }
            }
            9 => {
                let id = e.state.field("weapon").expect("registered network field");
                if let Some(def) = host.weapons.get(id as usize)
                    && turret_use::usable(e, p, origin, def)
                {
                    return Ok((target, id + 4, hint_slot(host, &def.use_hint_string)));
                }
            }
            0 if !trigger => return Ok(none),
            0 if (e.trigger_team == 0 || e.trigger_team == p.team())
                && (e.trigger_claim == 1023 || e.trigger_claim == slot) =>
            {
                let hint = e.state.field("dmgFlags").expect("registered network field");
                let hint_string = e.state.field("scale").expect("registered network field");
                return Ok((
                    if hint != 0 { target } else { 1023 },
                    hint,
                    if hint != 0 && hint_string != 255 {
                        hint_string
                    } else {
                        u32::MAX
                    },
                ));
            }
            _ => {}
        }
    }
    Ok(none)
}
pub fn update_hint(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let (target, hint, string) = select(slot, host)?;
    let w = host.world.players.get_mut(slot)?.state.words_mut();
    w[0x598 / 4] = hint;
    w[0x59c / 4] = string;
    w[0x5a0 / 4] = u32::from(target);
    Ok(())
}

pub fn use_buttons(slot: u16, old: u32, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    let w = p.state.words_mut();
    w[3] &= !8;
    if (17..=22).contains(&w[0xd8 / 4]) {
        return Ok(());
    }
    let buttons = p.input.buttons;
    let edge = buttons & !old;
    if p.use_hold_entity != 1023 && old & 0x20 != 0 && buttons & 0x20 == 0 {
        w[3] |= 8;
        return Ok(());
    }
    let mut handled = false;
    if edge & 0x28 != 0 {
        p.use_hold_entity = 1023;
        if w[0xa0 / 4] & 0x300 != 0 {
            if let Some(t) = host
                .world
                .entities
                .get_mut(w[0x594 / 4] as u16)
                .and_then(|e| e.turret.as_mut())
            {
                t.release_requested = true;
            }
            handled = true;
        } else if w[3] & 4 != 0 {
            handled = true;
        } else if w[0x5a0 / 4] != 1023 {
            p.use_hold_entity = w[0x5a0 / 4] as i32;
            p.input.use_hold_time = host.time_ms;
            handled = true;
        }
    }
    if p.use_hold_entity == 1023 && !handled {
        if edge & 0x20 != 0 {
            w[3] |= 8;
        }
        return Ok(());
    }
    let delay = |name, default| {
        host.config
            .get(name)
            .and_then(|s| s.parse::<i32>().ok())
            .unwrap_or(default)
    };
    if buttons & 0x28 == 0
        || p.use_hold_entity == 1023
        || host.time_ms.wrapping_sub(p.input.next_use_time) < delay("g_useholdspawndelay", 1)
        || host.time_ms.wrapping_sub(p.input.use_hold_time) < delay("g_useholdtime", 0)
    {
        return Ok(());
    }
    let target = p.use_hold_entity as u16;
    let player = p.object;
    p.use_hold_entity = 1023;
    let Some(e) = host.world.entities.get(target) else {
        return Ok(());
    };
    let object = e.object;
    match e.state.field("eType") {
        Some(3) => {
            host.world.interactions.push(Action::Notify {
                object,
                player,
                event: "touch",
            });
            if !host.world.client_is_spawning {
                host.world.interactions.push(Action::Item { slot, target });
            }
        }
        Some(9) if !turret_use::occupy(slot, target, host, true)? => {}
        _ => host.world.interactions.push(Action::Notify {
            object,
            player,
            event: "trigger",
        }),
    }
    Ok(())
}
