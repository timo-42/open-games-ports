//! SV_SpawnServer and SV_MapRestart_f share game initialization and differ in
//! client transitions, retained resources and the generation byte on the wire.
use super::{LiveGame, Server, publish_dvars};
use cod2_server::{
    client::ClientState,
    config::{ServerConfig, ServerEvent},
    level::Level,
};
use std::{
    io,
    time::{Duration, Instant},
};

fn next_id(previous: u8, fast: bool) -> u8 {
    if fast {
        (previous & 0xf0) | (previous.wrapping_add(1) & 15)
    } else {
        let next = previous.wrapping_add(16);
        if next & 0xf0 == 0 {
            next.wrapping_add(16)
        } else {
            next
        }
    }
}

impl Server {
    pub(super) fn apply_map_event(
        &mut self,
        config: &mut ServerConfig,
        event: ServerEvent,
        output: &mut Vec<u8>,
    ) -> io::Result<()> {
        let load_started = Instant::now();
        if let Some(old) = self.map.as_mut() {
            self.sessions.sync_bots(&old.world.bots)?;
            old.world.bots.occupied = self.sessions.occupied_slots();
        }
        let previous = self.map.as_ref();
        let post_spawn_cheats = if let ServerEvent::LoadMap {
            previous_cheats,
            cheats,
            ..
        } = &event
        {
            config.set_map_cheats(*previous_cheats);
            Some(*cheats)
        } else {
            None
        };
        let requested_slots = config
            .get("sv_maxclients")
            .and_then(|v| v.parse().ok())
            .unwrap_or(20);
        let gametype = config.get("g_gametype").unwrap_or("dm");
        let (name, fast, keep_state) = match event {
            ServerEvent::LoadMap { name, .. } => {
                let keep = previous.is_some_and(|old| {
                    old.world.keep_map_state && old.gametype.eq_ignore_ascii_case(gametype)
                });
                (name, false, keep)
            }
            ServerEvent::RestartMap { fast } => {
                let Some(old) = previous else {
                    return Ok(());
                };
                // The original copies validated g_gametype to sv_gametypeName
                // immediately before comparing those two strings (0x0808be65).
                let fast = fast
                    && !self.max_clients_modified
                    && self.sessions.capacity() == requested_slots;
                if fast && self.last_map_frame == Some(self.frame_serial) {
                    return Ok(());
                }
                (old.name.clone(), fast, fast && old.world.keep_map_state)
            }
            ServerEvent::Quit
            | ServerEvent::Heartbeat
            | ServerEvent::GameCompleteStatus
            | ServerEvent::Operator(_) => {
                return Ok(());
            }
        };
        let start_time = previous.map_or(0, |level| level.time_ms);
        let id = next_id(self.map_id, fast);
        let reloading = previous.is_some() && !fast;
        if reloading {
            if !self.assets.map_exists(&name) {
                return Err(io::Error::new(
                    io::ErrorKind::NotFound,
                    format!("Can't find map maps/mp/{name}.d3dbsp"),
                ));
            }
            let notice = format!("loadingnewmap\n{name}\n{gametype}");
            let data = [
                b"\xff\xff\xff\xff".as_slice(),
                &notice.as_bytes()[..notice.len().min(63)],
            ]
            .concat();
            for (_, player) in self.sessions.clients().filter(|(_, p)| {
                !p.is_bot() && matches!(p.state, ClientState::Primed | ClientState::Active)
            }) {
                if let Err(error) = self.map_socket.send_to(&data, player.endpoint.peer) {
                    eprintln!("map notice to {}: {error}", player.endpoint.peer);
                }
            }
            // NET_Sleep(250) wakes as soon as a datagram is waiting, leaving
            // it on the socket for Com_EventLoop after SV_SpawnServer.
            let end = Instant::now() + Duration::from_millis(250);
            let mut peek = [0; 1];
            while let Err(error) = self.map_socket.peek_from(&mut peek) {
                if error.kind() != io::ErrorKind::WouldBlock || Instant::now() >= end {
                    break;
                }
                std::thread::sleep(Duration::from_millis(1));
            }
        }
        // G_InitGame logs serverinfo before any level script can register or
        // modify exported dvars. Save those values while the old level is intact.
        let mut initial_info = cod2_server::query::QueryState::default();
        initial_info.publish_config(config);
        initial_info.set_dvar("mapname", &name);
        config.set_network_state(&name, id, &[]);
        let mut replacement = if fast {
            None
        } else {
            self.assets.replacement_for_language(config)?
        };
        let active_assets = replacement.as_mut().unwrap_or(&mut self.assets);
        if previous.is_some() {
            output.extend_from_slice(b"==== ShutdownGame ====\n");
        }
        if !fast {
            output.extend_from_slice(
                format!("------ Server Initialization ------\nServer: {name}\n").as_bytes(),
            );
            output.extend_from_slice(active_assets.search_report(None).as_bytes());
        }
        output.extend_from_slice(b"------- Game Initialization -------\ngamename: Call of Duty 2\ngamedate: Jun 23 2006\n");
        let mut level = active_assets.with_reference_transaction(|assets| {
            let load = |assets: &_| {
                Level::transition(assets, name, start_time, config, previous, fast, keep_state)
            };
            if fast {
                load(assets)
            } else {
                assets.with_map_references(load)
            }
        })?;
        if let Some(assets) = replacement {
            self.assets = assets;
        }
        level.snapshot_flags = previous.map_or(4, |old| old.snapshot_flags ^ 4);
        let applied_slots = config
            .get("sv_maxclients")
            .and_then(|v| v.parse().ok())
            .unwrap_or(requested_slots);
        self.max_clients_modified = fast && applied_slots != requested_slots;
        let capacity = self.sessions.resize(applied_slots);
        config
            .values
            .insert("sv_maxclients".into(), capacity.to_string());
        self.sessions
            .send_updates(level.configstrings.take_game_updates(), fast);
        if let Err(error) = self.close_game_log() {
            eprintln!("game log: {error}");
        }
        if let Err(error) =
            self.operator_state
                .logs
                .open_game(config, &initial_info.advertised_info(), start_time)
        {
            eprintln!("game log: {error}");
        }
        output.extend_from_slice(b"-----------------------------------\n");
        if let Some(error) = &level.gameplay_error {
            eprintln!("Game initialization: {error}");
        }
        self.map_id = id;
        self.last_map_frame = Some(self.frame_serial);

        {
            let now = level.time_ms;
            let mut game = LiveGame {
                level: &mut level,
                assets: &self.assets,
                config,
                map_id: id,
            };
            self.outbound.extend(
                self.sessions
                    .restart_clients(fast, keep_state, now, &mut game),
            );
        }
        if !fast {
            self.master.map_loaded(&level.name);
        }
        // The new sv_serverid appears in the published systeminfo after the
        // restart command and ClientBegin callbacks for every connected slot.
        publish_dvars(
            &mut self.browser,
            Some(&mut level),
            config,
            &self.assets,
            id,
        )?;
        config.take_info_dirty();
        if let Some(cheats) = post_spawn_cheats {
            // SV_Map_f sets sv_cheats only after SV_SpawnServer publishes the
            // gamestate. A warm client can re-prime before the next SV_Frame.
            config.set_map_cheats(cheats);
        }
        self.sessions
            .send_updates(level.configstrings.take_game_updates(), fast);
        level
            .world
            .bots
            .clients
            .retain(|slot, _| level.world.players.get(*slot).is_some());
        self.sessions.sync_bots(&level.world.bots)?;
        if !fast {
            output.extend_from_slice(b"-----------------------------------\n");
        }
        self.map = Some(level);
        for (slot, error) in self.sessions.tick_errors.drain(..) {
            eprintln!("map change, client {slot}: {error}");
        }
        if reloading && let Some(minimum) = config.map_load_ms {
            // Mirrors the rotation oracle's debugger breakpoint at the return
            // from the second and subsequent SV_SpawnServer calls.
            std::thread::sleep(
                Duration::from_millis(minimum.into()).saturating_sub(load_started.elapsed()),
            );
        }
        let finished = Instant::now();
        if !fast {
            // SV_ResetServerState clears sv.timeResidual. Com_Frame charges
            // time spent loading a running server to the next SV_Frame.
            self.clock = super::frame_clock::FrameClock::new(if reloading {
                load_started
            } else {
                finished
            });
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn generation_byte_keeps_full_and_fast_sequences_separate() {
        assert_eq!(next_id(0, false), 0x10);
        assert_eq!(next_id(0x1f, true), 0x10);
        assert_eq!(next_id(0x13, false), 0x23);
        assert_eq!(next_id(0xf3, false), 0x13);
    }
}
