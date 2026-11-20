//! Local server-owned test clients (SV_AddBot, 0x080913da and SV_RunBotClients,
//! 0x08096952). Their script callbacks use the same VM and player slots as UDP clients.
mod command;
use crate::{
    player::{self, Connect},
    script::ScriptProgram,
    usercmd::UserCmd,
    vm::{GameHost, fiber::FiberVm, value::Value},
};
pub use command::random_command;
use std::{collections::BTreeMap, io};

#[derive(Clone, Debug)]
pub struct TestClient {
    pub number: i32,
    pub connected_at: i32,
    pub userinfo: Vec<u8>,
    pub last_command: UserCmd,
}

#[derive(Clone, Debug, Default)]
pub struct BotClients {
    pub clients: BTreeMap<u16, TestClient>,
    /// Includes network clients whose entities have already been released (CS_ZOMBIE).
    pub occupied: u64,
}

impl BotClients {
    fn available(&self, max: u16, private_slots: u16, players: &player::Players) -> Option<u16> {
        (private_slots..max).find(|&slot| {
            self.occupied & (1u64 << slot) == 0
                && !self.clients.contains_key(&slot)
                && players.get(slot).is_none()
        })
    }
}

pub fn add(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
) -> io::Result<Value> {
    let max = host
        .config
        .get("sv_maxclients")
        .and_then(|s| s.parse::<u16>().ok())
        .unwrap_or(20)
        .clamp(1, 64);
    let private = if host
        .config
        .get("sv_privatePassword")
        .is_none_or(str::is_empty)
    {
        0
    } else {
        host.config
            .get("sv_privateClients")
            .and_then(|s| s.parse::<u16>().ok())
            .unwrap_or(0)
    };
    if host
        .world
        .bots
        .available(max, 0, &host.world.players)
        .is_none()
    {
        return Ok(Value::Undefined);
    }
    // SV_AddBot increments its counter after its preliminary capacity check, even when
    // SV_DirectConnect subsequently cannot use any of the remaining private slots.
    let number = host.config.bot_connect_number;
    host.config.bot_connect_number = number.wrapping_add(1);
    let Some(slot) = host.world.bots.available(max, private, &host.world.players) else {
        return Ok(Value::Undefined);
    };
    // Freshly zeroed free slots compare equal to NA_BOT with port 0 in SV_DirectConnect.
    // The first bot's reconnect-too-soon check can therefore fail early on a fresh map.
    let reconnect_ms = host
        .config
        .get("sv_reconnectlimit")
        .and_then(|v| v.parse::<i32>().ok())
        .unwrap_or(3)
        .saturating_mul(1000);
    if number as u16 == 0 && host.time_ms < reconnect_ms {
        return Ok(Value::Undefined);
    }
    let userinfo=format!(r"\cg_predictItems\1\cl_punkbuster\0\cl_anonymous\0\color\4\head\default\model\multi\snaps\20\rate\5000\name\bot{number}\protocol\118").into_bytes();
    player::connect(
        vm,
        program,
        host,
        Connect {
            slot,
            userinfo: &userinfo,
            persistent: Value::Array(Default::default()),
            guid: 0,
            local: true,
        },
    )?;
    player::begin(vm, host, slot)?;
    let object = host.world.players.get_mut(slot)?.object;
    host.world.bots.clients.insert(
        slot,
        TestClient {
            number,
            connected_at: host.time_ms,
            userinfo,
            last_command: UserCmd::default(),
        },
    );
    Ok(Value::Object(object))
}

pub fn run(vm: &mut FiberVm, program: &ScriptProgram, host: &mut GameHost<'_>) -> io::Result<()> {
    // Callbacks can add/drop a later client while an earlier slot is thinking.
    if host.world.bots.clients.is_empty() {
        return Ok(());
    }
    for slot in 0..64 {
        if !host.world.bots.clients.contains_key(&slot) {
            continue;
        }
        let Some(player) = host.world.players.get(slot) else {
            continue;
        };
        let cmd = random_command(
            &mut host.world.native_random,
            player.state.words()[0xd4 / 4] as u8,
            player.archive_time != 0,
        );
        let active = player.connected;
        host.world
            .bots
            .clients
            .get_mut(&slot)
            .expect("test client")
            .last_command = cmd;
        if active {
            player::think(vm, program, host, slot, &cmd)?;
        }
    }
    Ok(())
}
