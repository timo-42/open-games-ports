//! ClientSpawn, view angles and session-dependent ClientEndFrame updates.
mod compass;
mod eject;
mod look;
mod spectator;
mod state;
pub use spectator::follow_cycle;
pub(super) use spectator::stop_following;
pub use state::copy_entity_state;

use super::{integer, invalid};
use crate::{
    movement::{self, MovementSettings, MovementState},
    script::ScriptProgram,
    snapshot::PlayerState,
    usercmd::UserCmd,
    vm::{GameHost, entity_methods::relink, fiber::FiberVm, value::Value},
};
use std::io;

// g_spawnTraceMins and g_spawnTraceMaxs in the 1.3 server image.
#[derive(Debug, Default)]
pub struct Input {
    pub use_hold_time: i32,
    pub next_use_time: i32,
    pub last_command: UserCmd,
    pub last_cmd_time: i32,
    pub buttons: u32,
    pub buttons_since_last_frame: u32,
    pub frozen: bool,
    pub inactivity_deadline: i32,
    pub inactivity_warning: bool,
    pub look_at: Option<u32>,
    pub drop_reason: Option<&'static str>,
    pub noclip: bool,
    pub ufo: bool,
    pub ps_offset_ms: i32,
    pub viewmodel_index: u8,
    pub ping_until: i32,
    pub compass_target: Option<u8>,
    pub view_kick_angles: [f32; 3],
}

const MINS: [f32; 3] = [-15., -15., 0.];
const MAXS: [f32; 3] = [15., 15., 70.];

fn vector(words: &[u32], offset: usize) -> [f32; 3] {
    std::array::from_fn(|i| f32::from_bits(words[offset / 4 + i]))
}

fn write_vector(words: &mut [u32], offset: usize, values: [f32; 3]) {
    words[offset / 4..offset / 4 + 3].copy_from_slice(&values.map(f32::to_bits));
}

pub(crate) fn short_angle(angle: f32) -> u16 {
    let scaled = (f64::from(angle) * f64::from(182.04445_f32)).trunc();
    if scaled < f64::from(i32::MIN) || scaled > f64::from(i32::MAX) || !scaled.is_finite() {
        0
    } else {
        scaled as i32 as u16
    }
}

pub(crate) fn angle_mod(angle: f32) -> f32 {
    f32::from(short_angle(angle)) * (360.0 / 65536.0)
}

pub(crate) fn angle_normalize(angle: f32) -> f32 {
    let a = angle_mod(angle);
    if a > 180. { a - 360. } else { a }
}

pub(crate) fn set_angles(ps: &mut PlayerState, cmd: &UserCmd, mut angles: [f32; 3]) -> [f32; 3] {
    let words = ps.words_mut();
    if words[0x0c / 4] & 1 != 0 && words[0xa0 / 4] & 0x300 == 0 {
        let yaw = f32::from_bits(words[0x584 / 4]);
        let pitch = f32::from_bits(words[0x58c / 4]);
        let yaw_diff = angle_normalize(angle_normalize(yaw - angles[1]));
        if !(-45. ..=45.).contains(&yaw_diff) {
            angles[1] = angle_mod(if yaw_diff > 45. { yaw - 45. } else { yaw + 45. });
        }
        let pitch_diff = angle_normalize(angle_normalize(pitch - angles[0]));
        if !(-15. ..=45.).contains(&pitch_diff) {
            angles[0] = angle_normalize(if pitch_diff > 45. {
                pitch - 45.
            } else {
                pitch + 15.
            });
        }
    }
    for (axis, angle) in angles.iter().enumerate() {
        words[0x54 / 4 + axis] =
            u32::from(short_angle(*angle)).wrapping_sub(u32::from(cmd.angles[axis]));
    }
    write_vector(words, 0xe8, angles);
    angles
}

fn reset_state(ps: &mut PlayerState, slot: u16, now: i32, health_limit: i32, origin: [f32; 3]) {
    let count = ps.words()[0x140 / 4].wrapping_add(1);
    let flags = (ps.words()[0xa0 / 4] & 0x10_0002) ^ 2;
    ps.words_mut().fill(0);
    let words = ps.words_mut();
    words[0] = now.wrapping_sub(100) as u32;
    words[0xc / 4] = 0x1000;
    words[0xa0 / 4] = flags;
    words[0xcc / 4] = u32::from(slot);
    words[0x594 / 4] = 1023;
    words[0xf4 / 4] = 60;
    words[0xf8 / 4] = 60.0_f32.to_bits();
    words[0x140 / 4] = count;
    words[0x134 / 4] = health_limit as u32;
    write_vector(words, 0x14, origin);
    write_vector(words, 0x56c, MINS);
    write_vector(words, 0x578, MAXS);
}

pub fn dispatch(
    slot: u16,
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    Some(match name {
        "islookingat" => (|| {
            let Some(Value::Object(id)) = args.first() else {
                return Err(invalid("islookingat expects an entity"));
            };
            if host.world.entities.slot_for_object(*id).is_none() {
                return Err(invalid("not an entity"));
            }
            Ok(Value::Int(i32::from(
                host.world.players.get_mut(slot)?.input.look_at == Some(*id),
            )))
        })(),
        "usebuttonpressed" | "attackbuttonpressed" | "meleebuttonpressed" => {
            host.world.players.get_mut(slot).map(|p| {
                let mask = match name {
                    "usebuttonpressed" => 0x28,
                    "attackbuttonpressed" => 1,
                    _ => 4,
                };
                Value::Int(i32::from(
                    (p.input.buttons | p.input.buttons_since_last_frame) & mask != 0,
                ))
            })
        }
        "freezecontrols" => (|| {
            let value = integer(args.first().ok_or_else(|| invalid("missing freeze flag"))?)?;
            host.world.players.get_mut(slot)?.input.frozen = value != 0;
            Ok(Value::Undefined)
        })(),
        "playerads" => host
            .world
            .players
            .get_mut(slot)
            .map(|p| Value::Float(f32::from_bits(p.state.words()[0xdc / 4]))),
        "isonground" => host
            .world
            .players
            .get_mut(slot)
            .map(|p| Value::Int(i32::from(p.state.words()[0x60 / 4] != 1023))),
        "pingplayer" => host.world.players.get_mut(slot).map(|p| {
            p.input.ping_until = host.time_ms.wrapping_add(3000);
            p.state.words_mut()[0xa0 / 4] |= 0x40_0000;
            Value::Undefined
        }),
        "setviewmodel" => (|| {
            let name = args
                .first()
                .ok_or_else(|| invalid("missing viewmodel name"))?
                .script_string()?;
            let text = std::str::from_utf8(&name).map_err(|_| invalid("invalid viewmodel name"))?;
            if text.is_empty() {
                return Err(invalid("setviewmodel expects a non-empty model name"));
            }
            let index = host
                .models
                .index(host.assets, host.strings, text, host.initializing)?;
            host.world.players.get_mut(slot)?.input.viewmodel_index = index;
            Ok(Value::Undefined)
        })(),
        "getviewmodel" => host.world.players.get_mut(slot).map(|p| {
            Value::String(
                host.strings
                    .get(
                        crate::assets::models::MODEL_CONFIGSTRING_BASE
                            + usize::from(p.input.viewmodel_index),
                    )
                    .unwrap_or_default()
                    .to_vec(),
            )
        }),
        "spawn" => match args {
            [Value::Vector(origin), Value::Vector(angles), ..] => {
                spawn(slot, *origin, *angles, host).map(|()| Value::Undefined)
            }
            _ => Err(invalid("spawn expects an origin and angles")),
        },
        "setentertime" => (|| {
            let arg = args
                .first()
                .ok_or_else(|| invalid("missing player enter time"))?;
            host.world.players.get_mut(slot)?.enter_time = integer(arg)?;
            Ok(Value::Undefined)
        })(),
        "getplayerangles" => host
            .world
            .players
            .get_mut(slot)
            .map(|p| Value::Vector(vector(p.state.words(), 0xe8))),
        "setplayerangles" | "setorigin" => (|| {
            let Some(Value::Vector(v)) = args.first() else {
                return Err(invalid("expected a vector"));
            };
            let p = host.world.players.get_mut(slot)?;
            let e = host
                .world
                .entities
                .get_mut(slot)
                .ok_or_else(|| invalid("missing player entity"))?;
            if name == "setorigin" {
                let origin = [v[0], v[1], v[2] + 1.];
                write_vector(p.state.words_mut(), 0x14, origin);
                p.state.words_mut()[0xa0 / 4] ^= 2;
                state::copy_with_recoil(
                    p,
                    &mut e.state,
                    false,
                    host.weapons,
                    &mut host.world.native_random,
                );
                e.origin = origin;
                relink(e, host.collision, e.model_index != 0)?;
                let object = e.object;
                host.world
                    .set_field(object, "origin", Value::Vector(origin))?;
            } else {
                let a = set_angles(&mut p.state, &p.command, *v);
                e.angles = a;
                let object = e.object;
                host.world.set_field(object, "angles", Value::Vector(a))?;
            }
            Ok(Value::Undefined)
        })(),
        _ => return None,
    })
}

pub(crate) fn teleport(
    slot: u16,
    host: &mut GameHost<'_>,
    mut origin: [f32; 3],
    angles: [f32; 3],
) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    let was_linked = e.linked;
    e.linked = false;
    e.link.in_collision_world = false;
    origin[2] += 1.;
    write_vector(p.state.words_mut(), 0x14, origin);
    p.state.words_mut()[0xa0 / 4] ^= 2;
    e.angles = set_angles(&mut p.state, &p.command, angles);
    state::copy_with_recoil(
        p,
        &mut e.state,
        false,
        host.weapons,
        &mut host.world.native_random,
    );
    e.origin = origin;
    if was_linked {
        relink(e, host.collision, e.model_index != 0)?;
    }
    let object = e.object;
    let angles = e.angles;
    host.world
        .set_field(object, "angles", Value::Vector(angles))?;
    host.world
        .set_field(object, "origin", Value::Vector(origin))
}

fn stationary_origin(e: &mut crate::vm::game_entities::MapEntity, origin: [f32; 3]) {
    e.origin = origin;
    for (name, value) in ["pos.trBase[0]", "pos.trBase[1]", "pos.trBase[2]"]
        .into_iter()
        .zip(origin)
    {
        e.state.set_field(name, value.to_bits());
    }
    for field in [
        "pos.trType",
        "pos.trTime",
        "pos.trDuration",
        "pos.trDelta[0]",
        "pos.trDelta[1]",
        "pos.trDelta[2]",
    ] {
        e.state.set_field(field, 0);
    }
}

fn spawn(slot: u16, origin: [f32; 3], angles: [f32; 3], host: &mut GameHost<'_>) -> io::Result<()> {
    crate::vm::game_entities::turret_use::release(slot, host)?;
    crate::vm::game_entities::tag_links::unlink(host.world, slot)?;
    let p = host.world.players.get_mut(slot)?;
    reset_state(&mut p.state, slot, host.time_ms, p.max_health, origin);
    p.input = Input {
        last_command: p.input.last_command,
        ps_offset_ms: p.input.ps_offset_ms,
        viewmodel_index: p.input.viewmodel_index,
        next_use_time: host.time_ms,
        buttons: p.command.buttons,
        inactivity_deadline: host.time_ms.wrapping_add(inactivity_ms(host.config)),
        ..Default::default()
    };
    p.damage_feedback = Default::default();
    p.view = Default::default();
    p.last_followed_client = -1;
    p.last_voice_time = 0;
    p.use_hold_entity = 1023;
    p.command.server_time = host.time_ms;
    let angles = set_angles(&mut p.state, &p.command, angles);
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    e.linked = false;
    e.link.in_collision_world = false;
    e.sv_flags |= 1;
    e.flags = 0x1000;
    e.clip_mask = 0x0281_0011;
    e.mins = MINS;
    e.maxs = MAXS;
    e.angles = angles;
    e.contents = if p.session_state == 1 { 0 } else { 0x0200_0000 };
    e.state.set_field("groundEntityNum", 1023);
    stationary_origin(e, origin);
    let object = p.object;
    host.world
        .set_field(object, "classname", Value::String(b"player".to_vec()))?;
    host.world
        .set_field(object, "origin", Value::Vector(origin))?;
    host.world
        .set_field(object, "angles", Value::Vector(angles))?;
    let was_spawning = std::mem::replace(&mut host.world.client_is_spawning, true);
    let update = end_frame(slot, host).and_then(|()| {
        let cmd = host.world.players.get_mut(slot)?.command;
        think_real(slot, cmd, host, None)
    });
    host.world.client_is_spawning = was_spawning;
    update?;
    let p = host.world.players.get_mut(slot)?;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    state::copy_with_recoil(
        p,
        &mut e.state,
        false,
        host.weapons,
        &mut host.world.native_random,
    );
    let origin = e.origin;
    host.world
        .set_field(object, "origin", Value::Vector(origin))
}

/// Apply the current session classification before assembling snapshots.
pub fn end_frame(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let current = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    if current.connected
        && current.session_state < 2
        && current.state.words()[0xcc / 4] != u32::from(slot)
    {
        let origin = vector(current.state.words(), 0x14);
        let yaw = f32::from_bits(current.state.words()[0xec / 4]);
        spawn(slot, origin, [0., yaw, 0.], host)?;
        host.world
            .players
            .get_mut(slot)?
            .input
            .buttons_since_last_frame = 0;
        return Ok(());
    }
    let object = current.object;
    let health = super::integer(&host.world.field(object, "health")?)?;
    let p = host.world.players.get_mut(slot)?;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    crate::vm::model_methods::mirror_client(e, &mut p.client_state, host.strings)?;
    let words = p.state.words_mut();
    words[0x7a4 / 4] = 0;
    if !p.connected {
        p.input.buttons_since_last_frame = 0;
        return Ok(());
    }
    e.take_damage =
        p.session_state == 0 || (p.session_state == 1 && (p.input.noclip || p.input.ufo));
    match p.session_state {
        3 => {
            e.sv_flags = (e.sv_flags & !2) | 1;
            e.contents = 0;
            e.state.set_field("eType", 5);
            words[0xc / 4] &= 0xfc7f_ffff;
            words[1] = 5;
            words[0xa0 / 4] &= 0xffdf_ffbf;
            words[0xe4 / 4] = 0;
            for index in 1..=2 {
                host.strings.set(
                    4 + index,
                    host.world.players.team_scores[index - 1]
                        .to_string()
                        .as_bytes(),
                )?;
            }
        }
        2 => {
            p.view.gun_angles = [0.; 2];
            e.sv_flags = (e.sv_flags & !2) | 1;
            e.contents = 0;
            e.state.set_field("eType", 5);
            words[0xe4 / 4] = 0;
            words[0xc / 4] &= !0x80_0000;
            p.view.gun_angles = [0.; 2];
            spectator::follow(slot, host)?;
        }
        _ => {
            e.sv_flags = (e.sv_flags | 2) & !1;
            words[0xc / 4] = (words[0xc / 4] | 0x80_0000) & 0xfcff_ffff;
            e.contents = if p.session_state == 1 { 0 } else { 0x0200_0000 };
            words[0xe4 / 4] = u32::from(p.input.viewmodel_index);
            if p.input.ping_until <= host.time_ms {
                words[0xa0 / 4] &= !0x40_0000;
            }
            if p.input.noclip {
                e.contents = 0;
                words[1] = 2;
            } else if p.input.ufo {
                e.contents = 0;
                words[1] = 3;
            } else if p.session_state == 1 {
                e.sv_flags = (e.sv_flags & !2) | 1;
                words[1] = 6 + u32::from(e.tag_link.is_some());
            } else {
                words[1] = u32::from(e.tag_link.is_some());
            }
            words[0x48 / 4] = host
                .config
                .get("g_gravity")
                .and_then(|v| v.parse::<f32>().ok())
                .unwrap_or(800.) as i32 as u32;
            words[0x50 / 4] = host
                .config
                .get("g_speed")
                .and_then(|v| v.parse::<i32>().ok())
                .unwrap_or(190) as u32;
            p.view.spread_fraction = f32::from_bits(words[0x5d4 / 4]) / 255.;
            look::update(slot, host)?;
            crate::vm::game_entities::interaction::update_hint(slot, host)?;
            let p = host.world.players.get_mut(slot)?;
            let e = host
                .world
                .entities
                .get_mut(slot)
                .ok_or_else(|| invalid("missing player entity"))?;
            crate::combat::damage::apply_feedback(p, host.time_ms)?;
            let words = p.state.words_mut();
            words[0x12c / 4] = health as u32;
            e.state.set_field("loopSound", 0);
            state::copy_with_recoil(
                p,
                &mut e.state,
                host.config.get("g_smoothclients") != Some("0"),
                host.weapons,
                &mut host.world.native_random,
            );
            eject::update(slot, host)?;
            compass::update(slot, host)?;
            crate::animation::pose::update_player(slot, host)?;
        }
    }
    if host
        .world
        .players
        .get(slot)
        .is_some_and(|p| p.connected && matches!(p.session_state, 0 | 1))
    {
        crate::vm::game_entities::turret_use::update_user(slot, host)?;
        if host.config.get("g_debugLocDamage") == Some("1")
            && let Some(entity) = host.world.entities.get(slot)
            && entity.state.field("eType") == Some(1)
        {
            entity.pose(host.models)?;
        }
    }
    Ok(())
}

/// ClientThink stores the newest usercmd even when the server defers its simulation.
pub(super) fn think(
    slot: u16,
    cmd: &UserCmd,
    host: &mut GameHost<'_>,
    vm: &mut FiberVm,
    program: &ScriptProgram,
) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    p.input.last_command = p.command;
    p.input.last_cmd_time = host.time_ms;
    p.command = *cmd;
    if host.config.get("g_synchronousclients") != Some("1") {
        think_real(slot, *cmd, host, Some((vm, program)))?;
    }
    Ok(())
}

fn inactivity_ms(config: &crate::config::ServerConfig) -> i32 {
    config
        .get("g_inactivity")
        .map(crate::config::integer_prefix)
        .unwrap_or(0)
        .max(0)
        .saturating_mul(1000)
}

fn active(p: &mut super::Player, cmd: &UserCmd, now: i32, limit: i32) -> bool {
    if limit == 0 || cmd.forward_move != 0 || cmd.right_move != 0 || cmd.buttons & 0x401 != 0 {
        p.input.inactivity_deadline = now.wrapping_add(if limit == 0 { 60000 } else { limit });
        p.input.inactivity_warning = false;
    } else if !p.local {
        if p.input.inactivity_deadline < now {
            p.input.drop_reason = Some("GAME_DROPPEDFORINACTIVITY");
            return false;
        }
        if p.input.inactivity_deadline.saturating_sub(10_000) < now && !p.input.inactivity_warning {
            p.input.inactivity_warning = true;
            p.commands.push(b"c \"GAME_INACTIVEDROPWARNING\"".to_vec());
        }
    }
    true
}

fn think_real(
    slot: u16,
    mut cmd: UserCmd,
    host: &mut GameHost<'_>,
    mut scripts: Option<(&mut FiberVm, &ScriptProgram)>,
) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    if !p.connected {
        return Ok(());
    }
    cmd.server_time = cmd.server_time.clamp(
        host.time_ms.saturating_sub(1000),
        host.time_ms.saturating_add(200),
    );
    p.command.server_time = cmd.server_time;
    if cmd.server_time <= p.state.words()[0] as i32 && p.state.words()[0xcc / 4] == u32::from(slot)
    {
        return Ok(());
    }
    let words = p.state.words_mut();
    words[0x0c / 4] = (words[0x0c / 4] & !0x8000) | (u32::from(p.input.frozen) << 15);
    let spectator = p.session_state == 2;
    if p.session_state <= 1 && !active(p, &cmd, host.time_ms, inactivity_ms(host.config)) {
        return Ok(());
    }
    let old_buttons = p.input.buttons;
    p.input.buttons = cmd.buttons;
    p.input.buttons_since_last_frame |= cmd.buttons & !old_buttons;
    if p.session_state == 3 {
        return Ok(());
    }
    if spectator {
        if p.spectator_client < 0
            && p.no_spectate & 16 == 0
            && p.last_followed_client >= 0
            && (cmd.buttons ^ old_buttons) & 4 != 0
        {
            stop_following(slot, host)?;
        }
        let edge = cmd.buttons & !old_buttons;
        if edge & 1 != 0 || edge & 0x1000 != 0 {
            follow_cycle(slot, if edge & 1 != 0 { 1 } else { -1 }, host)?;
        }
    }
    let p = host.world.players.get_mut(slot)?;
    if spectator {
        if p.state.words()[0xc / 4] & 0x40_0000 != 0 {
            return Ok(());
        }
        p.state.words_mut()[1] = 4;
        p.state.words_mut()[0x50 / 4] = if p.no_spectate & 16 == 0 { 400 } else { 0 };
    }
    if !spectator {
        crate::combat::damage::view::update(
            p,
            host.weapons,
            host.config,
            host.time_ms,
            cmd.server_time,
        );
    }
    let previous_sequence = p.state.words()[0xa4 / 4] as i32;
    p.movement = MovementState::from_player(&p.state);
    p.movement.ads = crate::combat::AdsInput::from_player(&p.state, host.weapons);
    if !spectator {
        p.movement.seed_previous_command(p.input.last_command);
    }
    let mut settings = MovementSettings::from_config(host.config);
    settings.mantle.animations = host.world.animations.mantle.clone();
    let handling = crate::combat::WeaponSettings::from_config(host.config);
    let footsteps = crate::animation::FootstepSettings {
        move_threshold: host
            .config
            .get("player_moveThreshhold")
            .and_then(|s| s.parse().ok())
            .unwrap_or(10.),
        stumble_ms: host
            .config
            .get("player_dmgtimer_stumbleTime")
            .and_then(|s| s.parse().ok())
            .unwrap_or(500),
        turn_animations: host
            .config
            .get("player_turnAnims")
            .is_some_and(|s| s != "0"),
        time_ms: host.time_ms,
    };
    movement::advance_traced_steps(
        &mut p.movement,
        &cmd,
        &settings,
        &|start, end, mins, maxs, mask| {
            host.collision
                .box_trace(&host.world.entities, start, end, mins, maxs, slot, mask)
        },
        |movement, current, old, ms| {
            crate::combat::prepare_move(&mut p.state, host.weapons, current, old, ms, &handling);
            movement.entity_flags =
                (movement.entity_flags & !0x20_0040) | (p.state.words()[0xa0 / 4] & 0x20_0040);
            movement.pm_flags = (movement.pm_flags & !0x1000) | (p.state.words()[3] & 0x1000);
            p.anim_conditions.refresh(&p.state, current, host.weapons);
            movement.write_player(&mut p.state);
            for anim in movement.take_animations() {
                use crate::movement::MovementAnim;
                match anim {
                    MovementAnim::ProneChange => {
                        let w = p.state.words_mut();
                        if w[0x84 / 4] & !0x200 != 0 {
                            w[0x84 / 4] = (w[0x84 / 4] & 0x200) ^ 0x200;
                            w[0x8c / 4] = 50;
                        }
                    }
                    MovementAnim::DropTimers => {
                        crate::animation::PlayerAnimations::drop_timers(&mut p.state, ms)
                    }
                    MovementAnim::Move => host.world.animations.move_player(
                        &mut p.anim_conditions,
                        &mut p.state,
                        current,
                        &footsteps,
                        &mut p.pose_tree.controllers,
                    ),
                    MovementAnim::MoveId(id) => {
                        host.world
                            .animations
                            .move_id(&mut p.anim_conditions, &mut p.state, id);
                    }
                    MovementAnim::Event(id, cont, force) => {
                        host.world.animations.event(
                            &mut p.anim_conditions,
                            &mut p.state,
                            i32::from(id),
                            cont,
                            force,
                            &mut host.world.native_random,
                        );
                    }
                }
                p.anim_conditions.play_sounds(&mut p.state, host.strings)?;
            }
            if !matches!(movement.pm_type, 2..=5)
                && (movement.entity_flags & 0x300 == 0 || matches!(movement.pm_type, 1 | 7))
            {
                crate::combat::step_with_animations(
                    &mut p.state,
                    host.weapons,
                    current,
                    old,
                    ms,
                    &handling,
                    |ps, event, cont, force| {
                        if event == 7 {
                            p.anim_conditions
                                .set_weapon(ps.words()[0xd4 / 4] as usize, host.weapons);
                        }
                        host.world.animations.event(
                            &mut p.anim_conditions,
                            ps,
                            event,
                            cont,
                            force,
                            &mut host.world.native_random,
                        );
                        p.anim_conditions.play_sounds(ps, host.strings)
                    },
                )?;
                movement.mantle.refresh_weapon_state(&p.state);
                movement.pm_flags = p.state.words()[0xc / 4];
                movement.entity_flags = p.state.words()[0xa0 / 4];
            } else if !matches!(movement.pm_type, 1..=5 | 7) {
                crate::combat::force_ready(&mut p.state);
                movement.pm_flags = p.state.words()[0xc / 4];
            } else if matches!(movement.pm_type, 2..=5) {
                crate::combat::update_view_fraction(
                    &mut p.state,
                    host.weapons,
                    current,
                    ms,
                    &handling,
                );
            }
            movement.ads = crate::combat::AdsInput::from_player(&p.state, host.weapons);
            movement.mantle.refresh_weapon_state(&p.state);
            Ok(())
        },
    )?;
    if let Some((end, duration)) = p.movement.mantle.started.take() {
        crate::movement::mantle::spawn_blocker(host, slot, end, duration)?;
    }
    let p = host.world.players.get_mut(slot)?;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    let origin = p.movement.origin;
    if spectator {
        e.linked = false;
        e.link.in_collision_world = false;
    } else {
        e.state
            .set_field("animMovetype", u32::from(p.movement.pm_flags & 2 != 0));
        state::copy_with_recoil(
            p,
            &mut e.state,
            host.config.get("g_smoothclients") != Some("0"),
            host.weapons,
            &mut host.world.native_random,
        );
        e.origin = origin.map(|n| n.trunc());
        [e.mins, e.maxs] = p.movement.entity_bounds;
    }
    let touched = p.movement.take_touched();
    if !spectator {
        if let Some((vm, program)) = scripts.as_mut() {
            crate::combat::damage::dispatch_events(vm, program, host, slot, previous_sequence)?;
        }
        let e = host
            .world
            .entities
            .get_mut(slot)
            .ok_or_else(|| invalid("missing player entity"))?;
        relink(e, host.collision, e.model_index != 0)?;
        if !host.world.players.get_mut(slot)?.input.noclip {
            crate::vm::game_entities::touch::touch_linked(slot, host)?;
            if let Some((vm, program)) = scripts.as_mut() {
                crate::vm::game_entities::touch::flush_contacts(vm, program, host)?;
            }
        }
    }
    let p = host.world.players.get_mut(slot)?;
    let yaw = f32::from_bits(p.state.words()[0xec / 4]);
    let origin = vector(p.state.words(), 0x14);
    let object = p.object;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    if !spectator {
        e.angles = [0., yaw, 0.];
    }
    e.origin = origin;
    let angles = e.angles;
    host.world
        .set_field(object, "origin", Value::Vector(origin))?;
    host.world
        .set_field(object, "angles", Value::Vector(angles))?;
    if !spectator {
        crate::vm::game_entities::touch::touch_list(slot, &touched, host)?;
        if let Some((vm, program)) = scripts.as_mut() {
            crate::vm::game_entities::touch::flush_contacts(vm, program, host)?;
        }
        crate::vm::game_entities::interaction::use_buttons(slot, old_buttons, host)?;
        if let Some((vm, program)) = scripts {
            crate::vm::game_entities::interaction::dispatch(vm, program, host)?;
        }
    }
    Ok(())
}

pub fn run_frame(
    vm: &mut crate::vm::fiber::FiberVm,
    program: &crate::script::ScriptProgram,
    host: &mut GameHost<'_>,
) -> io::Result<()> {
    let slots: Vec<_> = host
        .world
        .players
        .iter()
        .filter(|(_, p)| p.connected)
        .map(|(slot, _)| slot)
        .collect();
    if host.config.get("g_synchronousclients") == Some("1") {
        for &slot in &slots {
            let mut cmd = host.world.players.get_mut(slot)?.command;
            cmd.server_time = host.time_ms;
            think_real(slot, cmd, host, Some((vm, program)))?;
        }
    }
    for &slot in &slots {
        let p = host.world.players.get_mut(slot)?;
        let team = p.team();
        host.world.objectives.apply(&mut p.state, i32::from(team));
        host.world
            .hud
            .write_player_state(slot, team, 3, &mut p.state);
    }
    for &slot in &slots {
        end_frame(slot, host)?;
        crate::vm::game_entities::interaction::dispatch(vm, program, host)?;
    }
    for slot in slots {
        update_crosshair(slot, host)?;
    }
    Ok(())
}

/// G_GetPlayerViewOrigin places the view at tag_player for a mounted turret.
pub fn view_origin(host: &GameHost<'_>, ps: &PlayerState) -> io::Result<[f32; 3]> {
    if ps.words()[0xa0 / 4] & 0x300 != 0 {
        let turret = host
            .world
            .entities
            .get(ps.words()[0x594 / 4] as u16)
            .ok_or_else(|| invalid("missing view turret"))?;
        return crate::vm::game_entities::turret_use::tag_world(turret, host.models, "tag_player")
            .ok_or_else(|| invalid("turret is missing tag_player"));
    }
    Ok(crate::combat::damage::eye(
        host,
        ps,
        vector(ps.words(), 0xe8),
    ))
}

/// Client_UpdateCrosshairTarget stores the other player's slot and health.
fn update_crosshair(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing player"))?;
    let w = p.state.words();
    if w[3] & 0x40_0000 != 0 {
        return Ok(());
    }
    let playing = p.session_state == 0;
    let team = p.team();
    let mut selected = None;
    if !playing || team != 0 {
        let angles = vector(w, 0xe8);
        let mut origin = view_origin(host, &p.state)?;
        let height = f32::from_bits(w[0xf8 / 4]);
        if !playing && height < 8. {
            origin[2] += 8. - height;
        }
        let [forward, _, _] = crate::vm::math::axes(angles);
        let end = movement::ma(origin, 8000., forward);
        let trace = host.collision.box_trace(
            &host.world.entities,
            origin,
            end,
            [0.; 3],
            [0.; 3],
            w[0xcc / 4] as u16,
            0x0200_0001,
        );
        if let Some(other) = host.world.players.get(trace.entity)
            && (!playing || other.team() == team)
        {
            selected = Some((
                u32::from(trace.entity),
                integer(&host.world.field(other.object, "health")?)? as u32,
            ));
        }
    }
    let words = host.world.players.get_mut(slot)?.state.words_mut();
    (words[0x138 / 4], words[0x13c / 4]) = selected.unwrap_or((u32::MAX, 0));
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::entity_delta::EntityState;

    #[test]
    fn respawn_clears_inventory_and_reuses_session_values() {
        let mut ps = PlayerState::default();
        ps.words_mut()[0x544 / 4] = 42;
        ps.words_mut()[0x140 / 4] = 6;
        ps.words_mut()[0xa0 / 4] = 0x10_0202;
        reset_state(&mut ps, 2, 900, 125, [11., -15., 82.]);
        assert_eq!(ps.words()[0], 800);
        assert_eq!(ps.words()[0x544 / 4], 0);
        assert_eq!(ps.words()[0x140 / 4], 7);
        assert_eq!(ps.words()[0x134 / 4], 125);
        assert_eq!(ps.words()[0xa0 / 4], 0x10_0000);
        assert_eq!(vector(ps.words(), 0x14), [11., -15., 82.]);
        assert_eq!(vector(ps.words(), 0x56c), MINS);
        assert_eq!(vector(ps.words(), 0x578), MAXS);
        let cmd = UserCmd {
            angles: [1000, 60000, 100],
            ..Default::default()
        };
        assert_eq!(set_angles(&mut ps, &cmd, [0., 34., -90.]), [0., 34., -90.]);
        assert_eq!(ps.words()[0x54 / 4] as i32, -1000);
        assert_eq!(ps.words()[0x58 / 4] as i32, -53811);
        assert_eq!(ps.words()[0x5c / 4], 49052);
    }

    #[test]
    fn prone_angles_and_local_events_match_native_state_conversion() {
        let mut ps = PlayerState::default();
        let words = ps.words_mut();
        words[0x0c / 4] = 0x80_0040;
        words[0x04 / 4] = 6;
        words[0xf4 / 4] = 11;
        words[0x5a8 / 4] = 8_f32.to_bits();
        words[0x5ac / 4] = 325.28946_f32.to_bits();
        words[0x5b0 / 4] = (-381.26193_f32).to_bits();
        words[0xa4 / 4] = 2;
        words[0xa8 / 4] = 0x8c;
        words[0xac / 4] = 0x47;
        words[0xb8 / 4] = 0x110;
        words[0xbc / 4] = 0x121;
        let mut e = EntityState::new(2);
        let mut notified = Vec::new();
        copy_entity_state(&mut ps, &mut e, false, |event| notified.push(event));
        assert_eq!(notified, [0x8c, 0x47]);
        assert_eq!(e.field("events[0]"), Some(0x47));
        assert_eq!(e.field("eventParms[0]"), Some(0x21));
        assert_eq!(e.field("eventParm"), Some(0x10));
        assert_eq!(e.field("eventSequence"), Some(1));
        assert_eq!(e.field("eFlags"), Some(0x6_0000));
        // Observed calls at 0x080dffeb and 0x080e0008 quantize both pitches.
        assert_eq!(e.field("fTorsoPitch"), Some((-34.711304_f32).to_bits()));
        assert_eq!(e.field("fWaistPitch"), Some((-21.258545_f32).to_bits()));
        copy_entity_state(&mut ps, &mut e, false, |_| panic!("event notified twice"));
        assert_eq!(e.field("eventParm"), Some(0x21));
    }
}
