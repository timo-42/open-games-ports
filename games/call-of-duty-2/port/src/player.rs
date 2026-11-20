//! Player slots, script callbacks and the connecting → active game-client transition.
mod commands;
mod dev_commands;
mod methods;
mod scores;
pub(crate) use scores::{flush_scores, send_death_scores};
pub mod spawn;

use crate::{
    admission::{info_integer, info_value},
    movement::MovementState,
    script::ScriptProgram,
    snapshot::{ClientState, PlayerState},
    usercmd::UserCmd,
    vm::{GameHost, fiber::FiberVm, value::Value},
};
use std::{collections::BTreeMap, io};

pub(crate) use commands::clean_display_name;
pub use commands::client_command;
pub(crate) use methods::{dispatch, global};

const CALLBACKS: &str = "maps/mp/gametypes/_callbacksetup";
const TEAMS: [&[u8]; 4] = [b"none", b"axis", b"allies", b"spectator"];
const STATES: [&[u8]; 4] = [b"playing", b"dead", b"spectator", b"intermission"];

fn invalid(text: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, text.into())
}

fn integer(value: &Value) -> io::Result<i32> {
    match value {
        Value::Int(n) => Ok(*n),
        _ => Err(invalid("expected an integer")),
    }
}

#[derive(Debug)]
pub struct Player {
    pub object: u32,
    pub connected: bool,
    pub guid: i32,
    pub local: bool,
    pub predict_item_pickup: bool,
    pub name: Vec<u8>,
    pending_name: Vec<u8>,
    pub state: PlayerState,
    pub client_state: ClientState,
    pub anim_conditions: crate::animation::Conditions,
    pub pose_tree: crate::animation::pose::PlayerTree,
    pub movement: MovementState,
    pub command: UserCmd,
    pub view: crate::combat::damage::view::ViewMotion,
    pub damage_feedback: crate::combat::damage::DamageFeedback,
    pub input: spawn::Input,
    pub session_state: u8,
    pub max_health: i32,
    pub spectator_client: i32,
    pub last_followed_client: i32,
    pub no_spectate: u32,
    pub use_hold_entity: i32,
    pub enter_time: i32,
    pub last_voice_time: i32,
    pub archive_time: i32,
    pub score: i32,
    pub deaths: i32,
    pub ping: i32,
    pub status_icon: u8,
    pers: Value,
    pub(crate) commands: crate::level::PlayerCommands,
}

impl Player {
    pub fn team(&self) -> u8 {
        self.client_state.field("team").unwrap_or(0) as u8
    }

    fn apply_userinfo(&mut self, info: &[u8], manual_change: bool) {
        let bytes = &info[..info.len().min(1023)];
        let mut bytes = bytes.split(|&c| c == 0).next().unwrap_or_default();
        if bytes.iter().any(|b| matches!(b, b'"' | b';')) {
            bytes = br"\name\badinfo";
        }
        self.predict_item_pickup = info_integer(info_value(bytes, b"cg_predictItems")) != 0;
        self.pending_name = clean_name(info_value(bytes, b"name"));
        if !self.connected || !manual_change {
            self.publish_name();
        }
    }

    fn publish_name(&mut self) {
        self.name.clone_from(&self.pending_name);
        self.client_state.set_name(&self.name);
    }
}

/// Original ClientCleanName operates on bytes. Color escapes occupy two bytes
/// in the 32-byte name buffer and do not count as visible characters.
pub fn clean_name(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(31);
    let mut visible = false;
    let mut spaces = 0;
    let mut bytes = input.iter().copied().take_while(|&b| b != 0);
    while let Some(byte) = bytes.next() {
        if byte == b'^' {
            let Some(color) = bytes.next() else { break };
            if color != b'0' {
                if out.len() > 29 {
                    break;
                }
                out.extend([byte, color]);
            }
            continue;
        }
        if byte == b' ' {
            if out.is_empty() || spaces == 3 {
                continue;
            }
            spaces += 1;
        } else {
            spaces = 0;
        }
        if out.len() == 31 {
            break;
        }
        out.push(byte);
        visible = true;
    }
    if visible {
        out
    } else {
        b"UnnamedPlayer".to_vec()
    }
}

#[derive(Debug, Default)]
pub struct Players {
    slots: BTreeMap<u16, Player>,
    manual_name_change: bool,
    pub scores_dirty: bool,
    pub team_scores: [i32; 2],
    pub game_log: Vec<(i32, Vec<u8>)>,
    pub console_lines: Vec<u8>,
}

impl Players {
    pub(crate) fn exit_level(&mut self) {
        self.team_scores = [0; 2];
        for player in self.slots.values_mut().filter(|p| p.connected) {
            player.score = 0;
            player.connected = false;
        }
    }

    pub fn get(&self, slot: u16) -> Option<&Player> {
        self.slots.get(&slot)
    }
    pub fn get_mut(&mut self, slot: u16) -> io::Result<&mut Player> {
        self.slots
            .get_mut(&slot)
            .ok_or_else(|| invalid("client slot is empty"))
    }
    pub fn iter(&self) -> impl Iterator<Item = (u16, &Player)> {
        self.slots.iter().map(|(&slot, p)| (slot, p))
    }
    pub(crate) fn iter_mut(&mut self) -> impl Iterator<Item = (u16, &mut Player)> {
        self.slots.iter_mut().map(|(&slot, p)| (slot, p))
    }
    pub fn slot_for_object(&self, object: u32) -> Option<u16> {
        self.iter()
            .find_map(|(slot, p)| (p.object == object).then_some(slot))
    }
    pub fn ranked_slots(&self) -> Vec<u16> {
        let mut slots = self.slots.keys().copied().collect::<Vec<_>>();
        slots.sort_by_key(|slot| {
            let player = &self.slots[slot];
            let spectator = player.team() == 3;
            let scores = if spectator {
                (0, 0)
            } else {
                (player.score, player.deaths)
            };
            (
                !player.connected,
                spectator,
                std::cmp::Reverse(scores.0),
                scores.1,
            )
        });
        slots
    }
    pub fn take_drops(&mut self) -> Vec<(usize, String)> {
        self.slots
            .iter_mut()
            .filter_map(|(&slot, p)| {
                p.input
                    .drop_reason
                    .take()
                    .map(|reason| (usize::from(slot), reason.to_owned()))
            })
            .collect()
    }
    pub fn take_commands(&mut self, slot: u16) -> Vec<Vec<u8>> {
        self.slots
            .get_mut(&slot)
            .map(|p| p.commands.take())
            .unwrap_or_default()
    }
    pub(crate) fn field(&self, object: u32, name: &str) -> Option<Value> {
        let player = self.get(self.slot_for_object(object)?)?;
        Some(match name {
            "name" => Value::String(player.name.clone()),
            "pers" => player.pers.clone(),
            "sessionteam" => Value::String(TEAMS[player.team() as usize].to_vec()),
            "sessionstate" => Value::String(STATES[player.session_state as usize].to_vec()),
            "score" => Value::Int(player.score),
            "deaths" => Value::Int(player.deaths),
            "spectatorclient" => Value::Int(player.spectator_client),
            "maxhealth" => Value::Int(player.max_health),
            // Player_GetPsOffsetTime reads archiveTime (0x26b4) at 0x080f84a4.
            "psoffsettime" => Value::Int(player.archive_time),
            "archivetime" => Value::Float(player.archive_time as f32 * 0.001),
            _ => return None,
        })
    }

    pub(crate) fn set_field(
        &mut self,
        object: u32,
        name: &str,
        value: &Value,
    ) -> Option<io::Result<()>> {
        let slot = self.slot_for_object(object)?;
        let player = self.slots.get_mut(&slot)?;
        Some(match name {
            "pers" => {
                player.pers = value.clone();
                Ok(())
            }
            "name" => Err(invalid("player field name is read-only")),
            "sessionteam" => value.script_string().and_then(|text| {
                let team = TEAMS
                    .iter()
                    .position(|v| *v == text)
                    .ok_or_else(|| invalid("illegal sessionteam string"))?;
                self.scores_dirty = true;
                player.client_state.set_field("team", team as u32)
            }),
            "sessionstate" => value.script_string().and_then(|text| {
                let state = STATES
                    .iter()
                    .position(|v| *v == text)
                    .ok_or_else(|| invalid("illegal sessionstate string"))?;
                if state == 3 {
                    player
                        .state
                        .set_field("eFlags", player.state.field("eFlags").unwrap_or(0) ^ 2)?;
                }
                player.session_state = state as u8;
                Ok(())
            }),
            "score" => integer(value).map(|n| {
                player.score = n;
                self.scores_dirty = true;
            }),
            "deaths" => integer(value).map(|n| player.deaths = n),
            "maxhealth" => integer(value).map(|n| {
                player.max_health = n.max(1);
                let stats = &mut player.state.words_mut()[0x12c / 4..][..6];
                stats[0] = (stats[0] as i32).min(player.max_health) as u32;
                stats[2] = player.max_health as u32;
            }),
            "psoffsettime" => integer(value).map(|n| player.input.ps_offset_ms = n),
            "archivetime" => value
                .number()
                .map(|v| player.archive_time = (f64::from(v) * 1000.) as i32),
            "spectatorclient" => integer(value).and_then(|n| {
                if !(-1..64).contains(&n) {
                    return Err(invalid("invalid spectatorclient"));
                }
                player.spectator_client = n;
                Ok(())
            }),
            _ => return None,
        })
    }
}

#[derive(Debug)]
pub struct Connect<'a> {
    pub slot: u16,
    pub userinfo: &'a [u8],
    pub persistent: Value,
    pub guid: i32,
    pub local: bool,
}

pub fn connect(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
    request: Connect<'_>,
) -> io::Result<()> {
    let Connect {
        slot,
        userinfo,
        persistent,
        guid,
        local,
    } = request;
    if !local
        && let Some(password) = host.config.get("g_password")
        && !password.is_empty()
        && !password.eq_ignore_ascii_case("none")
        && password.as_bytes() != info_value(userinfo, b"password")
    {
        return Err(invalid("GAME_INVALIDPASSWORD"));
    }
    let mut state = PlayerState::default();
    state.set_field("clientNum", u32::from(slot))?;
    let mut client_state =
        ClientState::new(u8::try_from(slot).map_err(|_| invalid("bad client slot"))?);
    client_state.set_field("team", 3)?;
    let object = host.world.attach_player(slot, host.time_ms)?;
    let mut p = Player {
        state,
        client_state,
        object,
        guid,
        local,
        pers: persistent,
        connected: false,
        max_health: 0,
        enter_time: 0,
        last_voice_time: 0,
        no_spectate: 0,
        use_hold_entity: 1023,
        predict_item_pickup: false,
        name: Vec::new(),
        pending_name: Vec::new(),
        view: Default::default(),
        damage_feedback: Default::default(),
        input: Default::default(),
        anim_conditions: Default::default(),
        pose_tree: Default::default(),
        movement: Default::default(),
        command: Default::default(),
        session_state: 2,
        spectator_client: -1,
        last_followed_client: -1,
        archive_time: 0,
        score: 0,
        deaths: 0,
        ping: 0,
        status_icon: 0,
        commands: host.strings.player_commands(slot),
    };
    p.apply_userinfo(userinfo, host.world.players.manual_name_change);
    host.world.players.slots.insert(slot, p);
    if let Err(e) = vm.invoke(
        program,
        host,
        CALLBACKS,
        "codecallback_playerconnect",
        object,
        vec![],
    ) {
        vm.notify(object, "disconnect", &[]);
        host.world.players.slots.remove(&slot);
        host.world.detach_player(slot, host.time_ms)?;
        return Err(e);
    }
    host.world.players.scores_dirty = true;
    Ok(())
}

pub fn userinfo_changed(host: &mut GameHost<'_>, slot: u16, userinfo: &[u8]) -> io::Result<()> {
    let manual_change = host.world.players.manual_name_change;
    host.world
        .players
        .get_mut(slot)?
        .apply_userinfo(userinfo, manual_change);
    Ok(())
}

/// Apply one active client usercmd and deliver game events before processing the next slot.
pub fn think(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
    slot: u16,
    cmd: &UserCmd,
) -> io::Result<()> {
    spawn::think(slot, cmd, host, vm, program)
}

pub fn begin(vm: &mut FiberVm, host: &mut GameHost<'_>, slot: u16) -> io::Result<()> {
    let player = host.world.players.get_mut(slot)?;
    player.connected = true;
    player.state.set_field("pm_type", 4)?;
    vm.notify(player.object, "begin", &[]);
    host.world.players.scores_dirty = true;
    Ok(())
}

pub fn disconnect(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
    slot: u16,
) -> io::Result<()> {
    let object = host.world.players.get_mut(slot)?.object;
    vm.notify(
        object,
        "menuresponse",
        &[
            Value::String(b"-1".to_vec()),
            Value::String(b"disconnect".to_vec()),
        ],
    );
    let spectators: Vec<_> = host
        .world
        .players
        .iter()
        .filter(|(_, p)| p.session_state == 2 && p.last_followed_client == i32::from(slot))
        .map(|(slot, _)| slot)
        .collect();
    for spectator in spectators {
        spawn::stop_following(spectator, host)?;
    }
    host.world.free_client_hud(slot);
    let callback = vm.invoke(
        program,
        host,
        CALLBACKS,
        "codecallback_playerdisconnect",
        object,
        vec![],
    );
    host.world.detach_player(slot, host.time_ms)?;
    host.world.players.slots.remove(&slot);
    host.world.players.scores_dirty = true;
    callback.map(|_| ())
}

pub(crate) fn icon_field(host: &GameHost<'_>, object: u32, name: &str) -> Option<Value> {
    if !matches!(name, "statusicon" | "headicon" | "headiconteam") {
        return None;
    }
    let slot = host.world.players.slot_for_object(object)?;
    let ent = host.world.entities.get(slot)?;
    let (base, index) = match name {
        "statusicon" => (0x16, u32::from(host.world.players.get(slot)?.status_icon)),
        "headicon" => (0x1e, ent.state.field("iHeadIcon")?),
        _ => {
            let team = ent.state.field("iHeadIconTeam")? as usize;
            return Some(Value::String(
                TEAMS.get(team).unwrap_or(&b"none".as_slice()).to_vec(),
            ));
        }
    };
    let value = if index == 0 {
        &[]
    } else {
        host.strings.get(base + index as usize)?
    };
    Some(Value::String(value.to_vec()))
}

pub(crate) fn set_icon_field(
    host: &mut GameHost<'_>,
    slot: u16,
    field: &str,
    value: Value,
) -> io::Result<()> {
    let name = value.script_string()?;
    if field == "headiconteam" {
        // Player_SetHeadIconTeam at 0x080f82b6 branches to Scr_Error on an
        // exact spectator match. Other unrecognized string IDs select team 3.
        let team = match name.as_slice() {
            b"none" => 0,
            b"axis" => 1,
            b"allies" => 2,
            b"spectator" => return Err(invalid("illegal head icon team string")),
            _ => 3,
        };
        host.world
            .entities
            .get_mut(slot)
            .ok_or_else(|| invalid("missing player entity"))?
            .state
            .set_field("iHeadIconTeam", team);
        return Ok(());
    }
    let (base, count) = if field == "statusicon" {
        (0x16, 8)
    } else {
        (0x1e, 15)
    };
    let index = if name.is_empty() {
        0
    } else {
        (1..=count)
            .find(|i| {
                host.strings
                    .get(base + i)
                    .is_some_and(|s| s.eq_ignore_ascii_case(&name))
            })
            .ok_or_else(|| {
                invalid(format!(
                    "{field} '{}' was not precached",
                    String::from_utf8_lossy(&name)
                ))
            })?
    };
    if field == "statusicon" {
        host.world.players.get_mut(slot)?.status_icon = index as u8;
    } else {
        host.world
            .entities
            .get_mut(slot)
            .ok_or_else(|| invalid("missing player entity"))?
            .state
            .set_field("iHeadIcon", index as u32);
    }
    Ok(())
}

#[cfg(test)]
mod tests;
