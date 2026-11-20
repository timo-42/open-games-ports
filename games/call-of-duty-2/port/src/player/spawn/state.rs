//! BG_PlayerStateToEntityState and its extrapolating g_smoothClients variant.
use super::{angle_normalize, vector};
use crate::{entity_delta::EntityState, snapshot::PlayerState};

pub fn copy_entity_state(
    ps: &mut PlayerState,
    entity: &mut EntityState,
    smooth: bool,
    mut notify: impl FnMut(u8),
) {
    let words = ps.words_mut();
    let origin = vector(words, 0x14);
    let view_angles = vector(words, 0xe8);
    for (axis, ((pos, angle), velocity)) in origin
        .into_iter()
        .zip(view_angles)
        .zip(vector(words, 0x20))
        .enumerate()
    {
        entity.set_field(
            ["pos.trBase[0]", "pos.trBase[1]", "pos.trBase[2]"][axis],
            ((pos as i32) as f32).to_bits(),
        );
        entity.set_field(
            ["apos.trBase[0]", "apos.trBase[1]", "apos.trBase[2]"][axis],
            ((angle as i32) as f32).to_bits(),
        );
        if smooth {
            entity.set_field(
                ["pos.trDelta[0]", "pos.trDelta[1]", "pos.trDelta[2]"][axis],
                velocity.to_bits(),
            );
        }
    }
    entity.set_field("pos.trType", if smooth { 3 } else { 1 });
    entity.set_field("apos.trType", 1);
    if smooth {
        entity.set_field("pos.trTime", words[0]);
        entity.set_field("pos.trDuration", 50);
    }
    let pm_flags = words[0xc / 4];
    entity.set_field("eType", if pm_flags & 0xc0_0000 != 0 { 1 } else { 5 });
    entity.set_field("angles2[1]", (words[0x9c / 4] as i32 as f32).to_bits());
    for (name, offset) in [
        ("legsAnim", 0x7c),
        ("torsoAnim", 0x84),
        ("clientNum", 0xcc),
        ("leanf", 0x4c),
    ] {
        entity.set_field(name, words[offset / 4]);
    }
    entity.set_field("weapon", words[0xd4 / 4] & 0xff);
    entity.set_field("groundEntityNum", words[0x60 / 4] & 0xffff);
    let flags = words[0xa0 / 4];
    if flags & 0x300 != 0 {
        entity.set_field("otherEntityNum", words[0x594 / 4]);
    }
    let dead = (words[1] as i32) >= 6;
    entity.set_field(
        "eFlags",
        (flags & !0x6_0000) | (u32::from(dead) << 17) | (u32::from(pm_flags & 0x40 != 0) << 18),
    );
    let prone = words[0xf4 / 4] == 11;
    let blend = if !prone {
        0.0
    } else if words[0xfc / 4] == 0 && !smooth {
        1.0
    } else {
        let target = words[0x100 / 4];
        let down = words[0x104 / 4] != 0;
        let duration = if target == 11 || (target == 40 && !down) {
            400.0
        } else {
            200.0
        };
        let t = (words[0].wrapping_sub(words[0xfc / 4]) as i32 as f32 / duration).clamp(0., 1.);
        if down { t } else { 1. - t }
    };
    for (name, offset) in [
        ("fTorsoHeight", 0x5a8),
        ("fTorsoPitch", 0x5ac),
        ("fWaistPitch", 0x5b0),
    ] {
        let v = f32::from_bits(words[offset / 4]);
        let value = if !prone {
            0.0
        } else if offset != 0x5a8 {
            angle_normalize(v) * blend
        } else {
            v * blend
        };
        entity.set_field(name, value.to_bits());
    }
    let end = words[0xa4 / 4];
    let mut replay = words[0x5cc / 4];
    let lag = end.wrapping_sub(replay) as i32;
    let event_param = if lag > 0 {
        if lag > 4 {
            replay = end.wrapping_sub(4);
        }
        words[0x5cc / 4] = replay.wrapping_add(1);
        words[0xb8 / 4 + (replay as usize & 3)] & 255
    } else {
        0
    };
    entity.set_field("eventParm", event_param);
    let mut seq = words[0xc8 / 4];
    let mut sent = entity
        .field("eventSequence")
        .expect("defined entity event sequence");
    while (end.wrapping_sub(seq) as i32) > 0 {
        let pos = seq as usize & 3;
        let event = words[0xa8 / 4 + pos] as u8;
        notify(event);
        // g_pmLocalOnlyEventIds at 0x0815486c, terminated by -1.
        if !matches!(event, 0x8c..=0x8f | 0xb9 | 0xba) {
            let dst = sent as usize & 3;
            entity.set_field(
                ["events[0]", "events[1]", "events[2]", "events[3]"][dst],
                event.into(),
            );
            entity.set_field(
                [
                    "eventParms[0]",
                    "eventParms[1]",
                    "eventParms[2]",
                    "eventParms[3]",
                ][dst],
                words[0xb8 / 4 + pos] & 255,
            );
            sent = sent.wrapping_add(1);
        }
        seq = seq.wrapping_add(1);
    }
    entity.set_field("eventSequence", sent);
    words[0xc8 / 4] = end;
}

/// Server dispatch row 1 plays the random fire kick for weapon-fire events.
pub(super) fn copy_with_recoil(
    p: &mut crate::player::Player,
    entity: &mut EntityState,
    smooth: bool,
    weapons: &crate::assets::weapons::WeaponRegistry,
    rng: &mut crate::vm::items::NativeRandom,
) {
    let mut kicks = 0;
    copy_entity_state(&mut p.state, entity, smooth, |event| {
        if matches!(event, 0x9e..=0xa0 | 0xaf) {
            kicks += 1;
        }
    });
    for _ in 0..kicks {
        crate::combat::fire_kick(&p.state, weapons, &mut p.input.view_kick_angles, || {
            rng.draw()
        });
    }
}
