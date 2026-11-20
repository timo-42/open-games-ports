//! Adapts protocol client slots to the game state for the current map.
use cod2_server::{
    assets::GameAssets,
    client::ClientGame,
    config::ServerConfig,
    gamestate::GameState,
    level::Level,
    message::MessageWriter,
    player::{self, Connect},
    query::tokenize,
    script::ScriptProgram,
    snapshot::{Snapshot, world::build_live},
    usercmd::{PlayerInputState, UserCmd},
    vm::{GameHost, fiber::FiberVm, precache::fog_bounds, value::Value},
};
use std::io;

pub struct LiveGame<'a> {
    pub level: &'a mut Level,
    pub config: &'a mut ServerConfig,
    pub assets: &'a GameAssets,
    pub map_id: u8,
}

impl LiveGame<'_> {
    pub(super) fn with_host<T>(
        &mut self,
        operation: impl FnOnce(&mut FiberVm, &ScriptProgram, &mut GameHost<'_>) -> io::Result<T>,
    ) -> io::Result<T> {
        let level = &mut self.level;
        let mut host = GameHost {
            weapons: &mut level.weapons,
            assets: self.assets,
            models: &mut level.models,
            start_time: level.start_time,
            collision: &level.collision,
            world: &mut level.world,
            config: self.config,
            strings: &mut level.configstrings,
            time_ms: level.time_ms,
            initializing: false,
        };
        let checkpoint = host.strings.begin_callback_output();
        let result = operation(&mut level.vm, &level.scripts, &mut host);
        host.strings.end_callback_output(checkpoint, result.is_ok());
        result
    }
}
impl ClientGame for LiveGame<'_> {
    fn pending_drops(&mut self) -> Vec<(usize, String)> {
        self.level.world.players.take_drops()
    }
    fn write_download(
        &mut self,
        download: &mut cod2_server::download::Download,
        msg: &mut MessageWriter,
        client: cod2_server::download::DownloadClient,
    ) -> io::Result<()> {
        download.write(self.assets, self.config, msg, client)
    }

    fn connect(&mut self, slot: usize, userinfo: &[u8]) -> io::Result<Vec<Vec<u8>>> {
        self.connect_authenticated(slot, userinfo, 0)
    }
    fn connect_authenticated(
        &mut self,
        slot: usize,
        userinfo: &[u8],
        guid: i32,
    ) -> io::Result<Vec<Vec<u8>>> {
        if let Some(error) = &self.level.gameplay_error {
            return Err(io::Error::other(error.clone()));
        }
        let slot = slot as u16;
        self.level.client_snapshots.remove(&slot);
        let persistent = self
            .level
            .persisted_players
            .remove(&slot)
            .unwrap_or_else(|| Value::Array(Default::default()));
        self.with_host(|vm, program, host| {
            // SV_DirectConnect allocates an empty pers array. Unauthenticated LAN
            // clients have guid 0; the dedicated UDP path never uses NA_LOOPBACK.
            let local = host.world.bots.clients.contains_key(&slot);
            player::connect(
                vm,
                program,
                host,
                Connect {
                    slot,
                    userinfo,
                    persistent,
                    guid,
                    local,
                },
            )?;
            Ok(Vec::new())
        })
    }
    fn gamestate(&self) -> io::Result<GameState<'_>> {
        Ok(GameState {
            configstrings: &self.level.configstrings,
            baselines: &self.level.baselines,
        })
    }
    fn server_id(&self) -> u8 {
        self.map_id
    }
    fn checksum_feed(&self) -> i32 {
        self.level.checksum_feed
    }
    fn pure_checksums(&self) -> Option<&[i32]> {
        (self.config.get("sv_pure") != Some("0")).then_some(&self.level.pure_checksums)
    }
    fn score(&self, slot: usize) -> i32 {
        self.level
            .world
            .players
            .get(slot as u16)
            .map_or(0, |player| player.score)
    }
    fn take_updates(&mut self) -> Vec<cod2_server::level::ServerUpdate> {
        self.level.configstrings.take_game_updates()
    }
    fn input_state(&self, slot: usize) -> io::Result<PlayerInputState> {
        let player = self
            .level
            .world
            .players
            .get(slot as u16)
            .ok_or_else(|| io::Error::other("missing player state"))?;
        let state = &player.state;
        let field = |name: &str| state.field(name).expect("registered player netfield");
        Ok(PlayerInputState {
            weapon: field("weapon") as u8,
            offhand: field("offHandIndex") as u8,
            view_angles: ["viewangles[0]", "viewangles[1]", "viewangles[2]"]
                .map(|name| f32::from_bits(field(name))),
            delta_angles: ["delta_angles[0]", "delta_angles[1]", "delta_angles[2]"]
                .map(|name| field(name) as i32),
            pm_flags: field("pm_flags"),
            entity_flags: field("eFlags"),
            lean: f32::from_bits(field("leanf")),
            weapon_position: f32::from_bits(field("fWeaponPosFrac")),
            selectable_weapons: cod2_server::combat::selectable_weapons(state, &self.level.weapons),
        })
    }
    fn command(&mut self, slot: usize, command: &[u8]) -> io::Result<Vec<Vec<u8>>> {
        let slot = slot as u16;
        let args = tokenize(command);
        let server_id = self.map_id;
        self.with_host(|vm, program, host| {
            if args
                .first()
                .is_some_and(|name| name.eq_ignore_ascii_case(b"userinfo"))
            {
                player::userinfo_changed(host, slot, args.get(1).map_or(&[], Vec::as_slice))?;
            } else {
                player::client_command(vm, program, host, slot, server_id, &args)?;
            }
            Ok(Vec::new())
        })
    }
    fn begin(&mut self, slot: usize, cmd: &UserCmd) -> io::Result<Vec<Vec<u8>>> {
        let slot = slot as u16;
        self.with_host(|vm, _, host| {
            host.world.players.get_mut(slot)?.command = *cmd;
            player::begin(vm, host, slot)?;
            Ok(Vec::new())
        })
    }
    fn think(&mut self, slot: usize, cmd: &UserCmd) -> io::Result<Vec<Vec<u8>>> {
        let slot = slot as u16;
        self.with_host(|vm, program, host| {
            player::think(vm, program, host, slot, cmd)?;
            Ok(Vec::new())
        })
    }
    fn snapshot(
        &mut self,
        slot: usize,
        sequence: i32,
        delta: Option<i32>,
        client_flags: u8,
        msg: &mut MessageWriter,
    ) -> io::Result<()> {
        let slot = slot as u16;
        let level = &mut self.level;
        let world = &mut level.world;
        let flags = level.snapshot_flags | client_flags;
        let frame = if let Some(player) = world.players.get(slot) {
            let mut age = player.archive_time;
            let fps = self
                .config
                .get("sv_fps")
                .and_then(|v| v.parse().ok())
                .unwrap_or(20);
            let cached = world.archive.lookup(&mut age, fps);
            let range = fog_bounds(&level.configstrings).1;
            let frame = if let Some(cached) = cached {
                cached.build(level.time_ms, flags, &player.state, &level.collision, range)?
            } else {
                let clients = world
                    .players
                    .iter()
                    .map(|(_, p)| p.client_state.clone())
                    .collect();
                build_live(
                    level.time_ms,
                    flags,
                    &player.state,
                    clients,
                    &mut world.entities,
                    &level.collision,
                    range,
                )?
            };
            world.players.get_mut(slot)?.archive_time = age;
            frame
        } else {
            // A zombie has no game entity. SV_BuildClientSnapshot clears its two
            // lists and retains the playerstate last occupying this ring slot.
            let mut frame = Snapshot {
                server_time: level.time_ms,
                flags,
                ..Default::default()
            };
            if let Some(previous) = level
                .client_snapshots
                .get(&slot)
                .and_then(|history| history.sent.ring_player(sequence))
            {
                frame.player = previous.clone();
            }
            frame
        };
        let history = level.client_snapshots.entry(slot).or_default();
        let clients = self
            .config
            .get("sv_maxclients")
            .and_then(|v| v.parse::<u64>().ok())
            .unwrap_or(20)
            .clamp(1, 64);
        let capacity = clients
            * if self.config.get("dedicated") == Some("0") {
                256
            } else {
                2048
            };
        history.first_entity[sequence as usize & 31] = level.next_snapshot_entity;
        level.next_snapshot_entity += frame.entities.len() as u64;
        let oldest = level.next_snapshot_entity.saturating_sub(capacity);
        let delta = delta.filter(|ack| history.first_entity[*ack as usize & 31] >= oldest);
        history
            .sent
            .write(msg, sequence, delta, &frame, &level.baselines)?;
        let mut padding = self
            .config
            .get("sv_padPackets")
            .and_then(|v| v.parse::<usize>().ok())
            .unwrap_or(0);
        let zeros = [0; 256];
        while padding > 0 {
            let size = padding.min(zeros.len());
            msg.write_data(&zeros[..size])?;
            padding -= size;
        }
        history.pending = Some((sequence, frame));
        Ok(())
    }
    fn snapshot_sent(&mut self, slot: usize, sequence: i32) {
        if let Some(history) = self.level.client_snapshots.get_mut(&(slot as u16))
            && let Some((pending, frame)) = history.pending.take()
            && sequence == pending
        {
            history.sent.record(sequence, frame);
        }
    }
    fn disconnect(&mut self, slot: usize, _reason: &str) -> io::Result<()> {
        self.level.world.bots.clients.remove(&(slot as u16));
        self.with_host(|vm, program, host| player::disconnect(vm, program, host, slot as u16))
    }
}
