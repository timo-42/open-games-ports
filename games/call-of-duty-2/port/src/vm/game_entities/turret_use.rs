//! Mounted weapon occupation, control angles, shot cadence and dismount.
pub(crate) mod script;
mod seat;
use super::{MapEntity, interaction::Action};
use crate::{
    assets::weapons::WeaponDefinition,
    entities::SpawnVars,
    player::{Player, spawn::teleport},
    snapshot::PlayerState,
    vm::{GameHost, invalid, value::Value},
};
use std::io;
type Vec3 = [f32; 3];

#[derive(Debug, Default)]
pub(crate) struct Turret {
    overrides: [Option<f32>; 4],
    spread: Option<f32>,
    pub(super) release_requested: bool,
    first_frame: bool,
    original_stance: u32,
    original_position: Vec3,
    cooldown: i32,
    sound_time: i32,
}
impl Turret {
    pub(super) fn from_spawn(vars: &SpawnVars) -> Self {
        let read = |key: &[u8]| {
            vars.get(key).map(|v| {
                crate::vm::spawn_fields::float_prefix(v)
                    .and_then(|(prefix, _)| prefix.parse().ok())
                    .unwrap_or(0.)
            })
        };
        Self {
            overrides: [b"toparc".as_slice(), b"rightarc", b"bottomarc", b"leftarc"].map(read),
            spread: read(b"playerSpread"),
            ..Self::default()
        }
    }
    fn arcs(&self, def: &WeaponDefinition) -> [f32; 4] {
        std::array::from_fn(|i| {
            self.overrides[i].unwrap_or(def.turret_arcs[i]).max(0.) * if i < 2 { -1. } else { 1. }
        })
    }
}
pub fn method(
    object: u32,
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    let index = ["settoparc", "setrightarc", "setbottomarc", "setleftarc"]
        .iter()
        .position(|&v| v == name)?;
    Some((|| {
        let turret = host
            .world
            .entities
            .for_object_mut(object)
            .and_then(|(_, e)| e.turret.as_mut())
            .ok_or_else(|| invalid("entity is not a turret"))?;
        turret.overrides[index] = Some(
            args.first()
                .ok_or_else(|| invalid("missing arc"))?
                .number()?,
        );
        Ok(Value::Undefined)
    })())
}
fn subtract(a: f32, b: f32) -> f32 {
    let mut angle = (a - b) % 360.;
    if angle > 180. {
        angle -= 360.;
    } else if angle < -180. {
        angle += 360.;
    }
    angle
}
pub(super) fn usable(
    ent: &MapEntity,
    user: &Player,
    player_origin: Vec3,
    def: &WeaponDefinition,
) -> bool {
    let Some(t) = ent.turret.as_ref() else {
        return false;
    };
    if ent.owner_num != 1023
        || user.state.words()[0x3c / 4] != 0
        || user.state.words()[0x60 / 4] == 1023
    {
        return false;
    }
    let [_, right, _, left] = t.arcs(def);
    let half = (right.abs() + left.abs()) * 0.5;
    let yaw = f64::from(crate::player::spawn::angle_normalize(
        ent.angles[1] + right + half,
    ))
    .to_radians();
    let x = f64::from(ent.origin[0] - player_origin[0]);
    let y = f64::from(ent.origin[1] - player_origin[1]);
    let len = x.hypot(y);
    let dot = if len == 0. {
        0.
    } else {
        (x * yaw.cos() + y * yaw.sin()) / len
    };
    dot.clamp(-1., 1.).acos().to_degrees() <= f64::from(half)
}
fn initial_angles(t: &mut MapEntity, p: &PlayerState, arcs: [f32; 4]) {
    let view = std::array::from_fn::<_, 2, _>(|i| f32::from_bits(p.words()[0xe8 / 4 + i]));
    for i in 0..2 {
        t.state.set_field(
            ["angles2[0]", "angles2[1]"][i],
            subtract(view[i], t.angles[i])
                .clamp(arcs[i], arcs[i + 2])
                .to_bits(),
        );
    }
    t.state.set_field("angles2[2]", 0);
}
fn set_view_clamp(ps: &mut PlayerState, angles: Vec3, arcs: [f32; 4]) -> io::Result<()> {
    for (i, (base_field, range_field)) in [
        ("viewAngleClampBase[0]", "viewAngleClampRange[0]"),
        ("viewAngleClampBase[1]", "viewAngleClampRange[1]"),
    ]
    .into_iter()
    .enumerate()
    {
        let range = subtract(arcs[i + 2], arcs[i]) * 0.5;
        let base = (angles[i] + arcs[i + 2] - range).rem_euclid(360.);
        ps.set_field(base_field, base.to_bits())?;
        ps.set_field(range_field, range.to_bits())?;
    }
    Ok(())
}
pub(super) fn occupy(
    slot: u16,
    target: u16,
    host: &mut GameHost<'_>,
    check_use: bool,
) -> io::Result<bool> {
    let p = host.world.players.get_mut(slot)?;
    let origin = host
        .world
        .entities
        .get(slot)
        .ok_or_else(|| invalid("missing player"))?
        .origin;
    let e = host
        .world
        .entities
        .get_mut(target)
        .ok_or_else(|| invalid("missing turret"))?;
    let def = host
        .weapons
        .get(e.state.field("weapon").unwrap_or(0) as usize)
        .ok_or_else(|| invalid("missing turret weapon"))?;
    if check_use && !usable(e, p, origin, def) {
        return Ok(false);
    }
    let t = e.turret.as_mut().expect("verified turret");
    t.first_frame = true;
    t.release_requested = false;
    t.original_position = origin;
    t.original_stance = if p.state.words()[3] & 1 != 0 {
        2
    } else {
        (p.state.words()[3] & 2) >> 1
    };
    let arcs = t.arcs(def);
    let w = p.state.words_mut();
    w[0x590 / 4] = 1;
    w[0x594 / 4] = u32::from(target);
    w[0xa0 / 4] = (w[0xa0 / 4] & !0x300)
        | match def.stance {
            2 => 0x100,
            1 => 0x200,
            _ => 0x300,
        };
    set_view_clamp(&mut p.state, e.angles, arcs)?;
    e.owner_num = slot;
    e.state.set_field("otherEntityNum", u32::from(slot));
    initial_angles(e, &p.state, arcs);
    host.world
        .entities
        .get_mut(slot)
        .expect("live occupant")
        .state
        .set_field("otherEntityNum", u32::from(target));
    Ok(true)
}
pub fn release(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    if p.state.words()[0xa0 / 4] & 0x300 == 0 {
        return Ok(());
    }
    let number = p.state.words()[0x594 / 4] as u16;
    let Some(turret) = host
        .world
        .entities
        .get_mut(number)
        .filter(|t| t.owner_num == slot)
    else {
        return Ok(());
    };
    let t = turret
        .turret
        .as_mut()
        .ok_or_else(|| invalid("mounted entity has no turret"))?;
    t.sound_time = 0;
    t.first_frame = false;
    t.release_requested = false;
    turret.state.set_field("loopSound", 0);
    turret.owner_num = 1023;
    let w = p.state.words_mut();
    let seq = w[0xa4 / 4];
    w[0xa8 / 4 + (seq as usize & 3)] = 0x8c + t.original_stance;
    w[0xb8 / 4 + (seq as usize & 3)] = 0;
    w[0xa4 / 4] = seq.wrapping_add(1);
    let origin = t.original_position;
    let angles = host
        .world
        .entities
        .get(slot)
        .ok_or_else(|| invalid("missing player"))?
        .angles;
    teleport(slot, host, origin, angles)?;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player"))?;
    e.state.set_field("otherEntityNum", 0);
    let w = host.world.players.get_mut(slot)?.state.words_mut();
    w[0xa0 / 4] &= !0x300;
    w[0x590 / 4] = 0;
    w[0x594 / 4] = 1023;
    Ok(())
}
pub fn update_user(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    if p.state.words()[0xa0 / 4] & 0x300 == 0 {
        return Ok(());
    }
    let number = p.state.words()[0x594 / 4] as u16;
    let e = host
        .world
        .entities
        .get_mut(number)
        .ok_or_else(|| invalid("mounted turret missing"))?;
    let t = e
        .turret
        .as_ref()
        .ok_or_else(|| invalid("invalid mounted entity"))?;
    if t.release_requested || p.session_state != 0 {
        return release(slot, host);
    }
    let def = host
        .weapons
        .get(e.state.field("weapon").expect("turret weapon") as usize)
        .ok_or_else(|| invalid("missing turret definition"))?;
    let arcs = t.arcs(def);
    // The first tag query in ClientEndFrame caches the previous control angles.
    seat::pose_turret(e, host.models)?;
    initial_angles(e, &p.state, arcs);
    let t = e.turret.as_mut().expect("verified mounted turret");
    let mut flags = e.state.field("eFlags").expect("registered network field") & !0x40;
    if t.first_frame {
        t.first_frame = false;
        flags ^= 2;
    }
    p.state.words_mut()[0x590 / 4] = 1;
    t.cooldown = (t.cooldown - 50).max(0);
    let firing = t.cooldown == 0 && p.input.buttons & 1 != 0;
    if firing {
        flags |= 0x40;
        t.cooldown = def.handling.fire_time;
        t.sound_time = t.cooldown * 3;
        p.state.words_mut()[0x590 / 4] = 2;
    }
    e.state.set_field("eFlags", flags);
    let spread = t.spread.unwrap_or(def.turret_spread).max(0.);
    let sound = |name: &str| {
        (1..256)
            .find(|i| {
                host.strings
                    .get(0x24e + i)
                    .is_some_and(|s| s.eq_ignore_ascii_case(name.as_bytes()) && !s.is_empty())
            })
            .unwrap_or(0) as u32
    };
    let mut loop_sound = 0;
    let mut stop = 0;
    if t.sound_time > 0 {
        loop_sound = sound(&def.turret_sounds[0]);
        t.sound_time -= 50;
        if t.sound_time <= 0 {
            stop = sound(&def.turret_sounds[2]);
            if stop != 0 {
                loop_sound = 0;
            }
        }
    }
    e.state.set_field("loopSound", loop_sound);
    if firing {
        e.add_event(0xaf, u32::from(slot), host.time_ms);
    }
    if stop != 0 {
        e.add_event(0xb3, stop, host.time_ms);
    }
    seat::update(slot, number, def.turret_anim_increment, host)?;
    if firing {
        let e = host
            .world
            .entities
            .get(number)
            .ok_or_else(|| invalid("missing mounted turret"))?;
        let eye = seat::tag_world(e, host.models, "tag_player")
            .ok_or_else(|| invalid("missing tag_player"))?;
        let flash = seat::tag_world(e, host.models, "tag_flash")
            .ok_or_else(|| invalid("missing tag_flash"))?;
        let p = host
            .world
            .players
            .get(slot)
            .ok_or_else(|| invalid("missing occupant"))?;
        let view = std::array::from_fn(|i| f32::from_bits(p.state.words()[0xe8 / 4 + i]));
        let dir = super::frame::basis(view)[0];
        let len = (0..3)
            .map(|i| f64::from(flash[i] - eye[i]).powi(2))
            .sum::<f64>()
            .sqrt() as f32;
        let muzzle = std::array::from_fn(|i| eye[i] + len * dir[i]);
        host.world.interactions.push(Action::Fire {
            slot,
            turret: number,
            muzzle,
            spread,
        });
    }
    Ok(())
}
pub(crate) use seat::tag_world;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn seated_axis_limits_replicate_in_separate_pitch_and_yaw_fields() -> io::Result<()> {
        let mut ps = PlayerState::default();
        set_view_clamp(&mut ps, [0., 82.9, 0.], [-15., -45., 15., 45.])?;
        for (name, expected) in [
            ("viewAngleClampBase[0]", 0.0_f32),
            ("viewAngleClampBase[1]", 82.9),
            ("viewAngleClampRange[0]", 15.),
            ("viewAngleClampRange[1]", 45.),
        ] {
            assert_eq!(ps.field(name), Some(expected.to_bits()));
        }
        Ok(())
    }
}
