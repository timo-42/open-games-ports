#![forbid(unsafe_code)]

mod console;
mod frame_clock;
mod live_game;
mod operator;
mod restart;
use cod2_server::admission::{Admission, AdmissionRules, admit};
use cod2_server::assets::GameAssets;
use cod2_server::challenge::{Challenges, hostname_addresses, is_lan_address};
use cod2_server::client::{ClientGame, ClientSessions, ClientState, Datagram};
use cod2_server::config::{ServerConfig, ServerEvent};
use cod2_server::level::Level;
use cod2_server::master::{Announcement, Master};
use cod2_server::query::{QueryPlayer, QueryState, oob_tokens};
use cod2_server::rcon::{self, Rcon, RconAction};
use live_game::LiveGame;
use std::io::{self, BufRead};
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4, ToSocketAddrs, UdpSocket};
use std::sync::mpsc;
use std::time::{Duration, Instant};

fn open_socket(interface: &str, port: u16, mut opening: impl FnMut(u16)) -> io::Result<UdpSocket> {
    // NET_OpenIP / NET_IPSocket, chunk_038. "localhost" selects INADDR_ANY.
    let ip = if interface.is_empty() || interface.eq_ignore_ascii_case("localhost") {
        Ipv4Addr::UNSPECIFIED
    } else {
        (interface, port)
            .to_socket_addrs()?
            .find_map(|address| match address {
                SocketAddr::V4(addr) => Some(*addr.ip()),
                _ => None,
            })
            .ok_or_else(|| io::Error::other("net_ip has no IPv4 address"))?
    };
    for offset in 0..10 {
        let candidate = port.wrapping_add(offset);
        opening(candidate);
        match UdpSocket::bind(SocketAddrV4::new(ip, candidate)) {
            Ok(socket) => {
                socket.set_broadcast(true)?;
                return Ok(socket);
            }
            Err(err) if err.kind() == io::ErrorKind::AddrInUse && offset < 9 => continue,
            Err(err) => return Err(err),
        }
    }
    unreachable!()
}

fn publish_dvars(
    browser: &mut QueryState,
    level: Option<&mut Level>,
    config: &mut ServerConfig,
    assets: &GameAssets,
    map_id: u8,
) -> io::Result<()> {
    if let Some(level) = level {
        let packages = assets.iwd_info(config.get("sv_pure") != Some("0"));
        config.set_network_state(&level.name, map_id, &packages);
        browser.publish_config(config);
        level
            .configstrings
            .set(1, &browser.system_info(map_id, &packages))?;
        level.configstrings.set_script_dvars(config)?;
        level.configstrings.set(0, &browser.advertised_info())?;
    } else {
        browser.publish_config(config);
    }
    Ok(())
}

struct Server {
    assets: GameAssets,
    config: ServerConfig,
    map: Option<Level>,
    browser: QueryState,
    challenges: Challenges,
    master: Master,
    local_addresses: Vec<Ipv4Addr>,
    operator_state: operator::OperatorState,
    started: Instant,
    map_id: u8,
    map_socket: UdpSocket,
    frame_serial: u64,
    last_map_frame: Option<u64>,
    max_clients_modified: bool,
    clock: frame_clock::FrameClock,
    rcon: Rcon,
    inserted_commands: Vec<String>,
    quitting: bool,
    sessions: ClientSessions,
    outbound: Vec<Datagram>,
    last_tick_error: Option<String>,
}

impl Server {
    fn new(
        assets: GameAssets,
        config: ServerConfig,
        map_socket: UdpSocket,
        bound_ip: Ipv4Addr,
        local_addresses: Vec<Ipv4Addr>,
        operator_state: operator::OperatorState,
    ) -> io::Result<Self> {
        Ok(Self {
            operator_state,
            assets,
            config,
            map: None,
            browser: QueryState::default(),
            challenges: Challenges::default(),
            master: Master::new(bound_ip),
            local_addresses,
            started: Instant::now(),
            map_id: 0,
            map_socket,
            frame_serial: 0,
            last_map_frame: None,
            max_clients_modified: false,
            clock: frame_clock::FrameClock::new(Instant::now()),
            rcon: Rcon::default(),
            inserted_commands: Vec::new(),
            quitting: false,
            sessions: ClientSessions::new(20),
            outbound: Vec::new(),
            last_tick_error: None,
        })
    }

    fn apply(&mut self, mut config: ServerConfig) -> io::Result<bool> {
        let mut output = Vec::new();
        for message in config.messages.drain(..) {
            output.extend(message.bytes());
            output.push(b'\n');
        }
        let result = self.apply_with_output(config, &mut output);
        self.print_console(&output);
        result
    }

    fn apply_with_output(
        &mut self,
        mut config: ServerConfig,
        output: &mut Vec<u8>,
    ) -> io::Result<bool> {
        // The external i386 PunkBuster module cannot be loaded by this native
        // runtime. Refuse a configuration that would advertise its protection.
        if config.get("sv_punkbuster") == Some("1") {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "PunkBuster is unavailable in the native server (sv_punkbuster 1)",
            ));
        }
        let mut loaded_map = false;
        for event in std::mem::take(&mut config.events) {
            match event {
                ServerEvent::Quit => {
                    output.extend_from_slice(b"quitting...\n");
                    output.extend(self.stop_map("EXE_SERVERQUIT"));
                    return Ok(false);
                }
                ServerEvent::Operator(words) => {
                    if let Some(text) = self.operator_reply(&words) {
                        output.extend(text);
                    }
                    if words
                        .first()
                        .is_some_and(|s| s.eq_ignore_ascii_case("killserver"))
                    {
                        config.map = None;
                        config.values.insert("sv_running".into(), "0".into());
                    }
                }
                ServerEvent::Heartbeat => self.master.force_heartbeat(),
                ServerEvent::GameCompleteStatus => {
                    if config.get("dedicated") == Some("2")
                        && let Some(level) = &self.map
                    {
                        self.browser.players = query_players(&self.sessions, level);
                        self.outbound.extend(
                            self.master
                                .announcement(Announcement::GameComplete, &self.browser),
                        );
                        output.extend_from_slice(self.master.take_console().as_bytes());
                    }
                }
                event => {
                    let previous = self.map_id;
                    self.apply_map_event(&mut config, event, output)?;
                    loaded_map |= self.map_id != previous;
                }
            }
        }
        if loaded_map {
            self.browser.publish_config(&config);
        } else {
            publish_dvars(
                &mut self.browser,
                self.map.as_mut(),
                &mut config,
                &self.assets,
                self.map_id,
            )?;
            config.take_info_dirty();
        }
        let number = |name: &str, default: i32| {
            config
                .get(name)
                .and_then(|v| v.parse().ok())
                .unwrap_or(default)
        };
        self.sessions.timeout_ms = number("sv_timeout", 240).clamp(0, 1800) * 1000;
        self.sessions.zombie_ms = number("sv_zombietime", 2).clamp(0, 1800) * 1000;
        self.sessions.max_rate = number("sv_maxRate", 0);
        self.sessions.flood_protect = number("sv_floodProtect", 1) != 0;
        self.sessions.local_dedicated = number("dedicated", 2) != 2;
        self.config = config;
        if let Some(level) = self.map.as_mut() {
            self.sessions
                .send_updates(level.configstrings.take_game_updates(), true);
        }
        Ok(true)
    }

    fn frame(&mut self, now: Instant) -> io::Result<()> {
        if self.config.take_archive_dirty()
            && self
                .assets
                .write_generated("config_mp_server.cfg", &self.config.archived_cfg(false))
                .is_err()
        {
            self.print_console(b"Couldn't write config_mp_server.cfg.\n");
        }
        self.frame_serial = self.frame_serial.wrapping_add(1);
        if let Some(level) = self.map.as_mut()
            && let Some(command) = level.world.map_command.take()
        {
            let mut config = self.config.clone();
            config.retained_gametype = level.world.keep_map_state.then(|| level.gametype.clone());
            config.execute_tokens_files(&command.tokens(), &self.assets)?;
            if let Err(error) = self.apply(config) {
                eprintln!("map transition: {error}");
            }
        }
        let vote_commands = self
            .map
            .as_mut()
            .map(|level| std::mem::take(&mut level.world.vote.console_queue))
            .unwrap_or_default();
        for command in vote_commands {
            let result = self.operator(&command);
            self.print_console(&result);
        }
        let elapsed = self.clock.elapsed(now);
        if self.map.is_none() || elapsed == 0 {
            return Ok(());
        }
        let fixed = self
            .config
            .get("fixedtime")
            .and_then(|v| v.parse().ok())
            .unwrap_or(0);
        let scale = self
            .config
            .get("timescale")
            .and_then(|v| v.parse().ok())
            .unwrap_or(1.0);
        let dedicated = self.config.get("dedicated") != Some("0");
        let (modified, warning) = frame_clock::modified_msec(elapsed, fixed, scale, dedicated);
        if let Some(msec) = warning {
            self.print_console(format!("Hitch warning: {msec} msec frame time\n").as_bytes());
        }
        if self.config.get("cl_paused").is_some_and(|v| v != "0") {
            let paused = self
                .sessions
                .clients()
                .filter(|(_, s)| s.state != ClientState::Zombie)
                .take(2)
                .count()
                < 2;
            self.config
                .values
                .insert("sv_paused".into(), if paused { "1" } else { "0" }.into());
            if paused {
                return Ok(());
            }
        }
        self.clock.residual += modified;
        let level = self.map.as_mut().expect("running level");
        let fps = self
            .config
            .get("sv_fps")
            .and_then(|value| value.parse::<u32>().ok())
            .unwrap_or(20)
            .clamp(1, 1000);
        let step = 1000 / fps;
        if self.clock.residual < step {
            return Ok(());
        }
        if self.config.take_info_dirty() {
            publish_dvars(
                &mut self.browser,
                Some(level),
                &mut self.config,
                &self.assets,
                self.map_id,
            )?;
        }
        self.sessions.sync_bots(&level.world.bots)?;
        level.world.bots.occupied = self.sessions.occupied_slots();
        // SV_CalcPings refreshes the values exposed by SV_GetClientPing
        // before running the game frame and its scoreboard requests.
        for (slot, session) in self.sessions.clients() {
            if let Ok(player) = level.world.players.get_mut(slot as u16) {
                player.ping = session.ping;
            }
        }

        let mut game = LiveGame {
            level,
            config: &mut self.config,
            assets: &self.assets,
            map_id: self.map_id,
        };
        game.with_host(cod2_server::bots::run)?;
        loop {
            let time = game.level.time_ms.wrapping_add(step as i32);
            game.level.frame(game.assets, game.config, time)?;
            game.level.world.script_loading = false;
            // Recoverable ScrRuntime_Error traces use console channel 4 (logfile only).
            for diagnostic in game.level.vm.diagnostics.drain(..) {
                self.operator_state
                    .logs
                    .console(game.config, format!("{diagnostic}\n").as_bytes())?;
            }
            self.sessions.sync_bots(&game.level.world.bots)?;
            for (line_time, line) in game.level.world.players.game_log.drain(..) {
                self.operator_state.logs.record(line_time, &line);
            }
            self.sessions.send_updates(game.take_updates(), true);
            self.clock.residual -= step;
            let last = self.clock.residual < step;
            if last {
                // SV_Frame sends one snapshot after the final catch-up tick.
                self.outbound.extend(self.sessions.tick(time, &mut game)?);
                self.sessions.send_updates(game.take_updates(), true);
            }
            let world = &mut game.level.world;
            world.archive.record(
                time,
                fps as i32,
                world
                    .players
                    .iter()
                    .map(|(_, p)| (&p.client_state, &p.state)),
                &world.entities,
            )?;
            if last {
                break;
            }
        }
        let time = game.level.time_ms;
        let errors = self
            .sessions
            .tick_errors
            .drain(..)
            .map(|(slot, e)| format!("client {slot}: {e}"))
            .collect::<Vec<_>>()
            .join("\n");
        if errors.is_empty() {
            self.last_tick_error = None;
        } else if self.last_tick_error.as_ref() != Some(&errors) {
            eprintln!("{errors}");
            self.last_tick_error = Some(errors);
        }
        let internet = game.config.get("dedicated") == Some("2");
        self.master.population(
            self.sessions
                .clients()
                .filter(|(_, c)| c.state != ClientState::Zombie)
                .count(),
            game.config
                .get("sv_maxclients")
                .and_then(|v| v.parse().ok())
                .unwrap_or(20),
        );
        if self.master.status_due(time, internet) {
            self.browser.players = query_players(&self.sessions, game.level);
        }
        self.master
            .poll(time, internet, false, &self.browser, &mut self.outbound);
        Ok(())
    }

    fn flush(&mut self, socket: &UdpSocket) {
        if let Some(level) = self.map.as_mut() {
            let lines = std::mem::take(&mut level.world.players.console_lines);
            self.print_console(&lines);
        }
        let master_log = self.master.take_console();
        self.print_console(master_log.as_bytes());
        let lines = std::mem::take(&mut self.sessions.console_lines);
        self.print_console(&lines);
        for peer in self.sessions.take_released_challenges() {
            self.challenges.disconnect(peer);
        }
        for datagram in self.outbound.drain(..) {
            if let Err(e) = socket.send_to(&datagram.bytes, datagram.peer) {
                eprintln!("UDP send to {}: {e}", datagram.peer);
            }
        }
    }

    fn read_packets(&mut self, socket: &UdpSocket, buffer: &mut [u8]) -> io::Result<()> {
        loop {
            let (size, peer) = match socket.recv_from(buffer) {
                Ok(packet) => packet,
                Err(e) if e.kind() == io::ErrorKind::WouldBlock => return Ok(()),
                Err(e)
                    if matches!(
                        e.kind(),
                        io::ErrorKind::Interrupted | io::ErrorKind::ConnectionReset
                    ) =>
                {
                    continue;
                }
                Err(e) => return Err(e),
            };
            let SocketAddr::V4(from) = peer else { continue };
            let replies = match self.packet(&buffer[..size], from) {
                Ok(replies) => replies,
                Err(e) => {
                    eprintln!("client packet from {from}: {e}");
                    continue;
                }
            };
            self.flush_game_log();
            for reply in replies {
                if let Err(error) = socket.send_to(&reply, from) {
                    eprintln!("UDP send to {from}: {error}");
                }
            }
            self.flush(socket);
            if self.quitting {
                return Ok(());
            }
        }
    }

    fn shutdown(&mut self, socket: &UdpSocket) {
        self.flush(socket);
    }

    fn operator_reply(&mut self, words: &[String]) -> Option<Vec<u8>> {
        if let Some(result) = self.admin(words) {
            return Some(result.unwrap_or_else(|e| format!("{e}\n").into_bytes()));
        }
        match words.first().map(|cmd| cmd.to_ascii_lowercase()).as_deref() {
            Some("status") => {
                let Some(level) = self.map.as_mut() else {
                    return Some(b"Server is not running.\n".to_vec());
                };
                let time = level.time_ms;
                let game = LiveGame {
                    level,
                    config: &mut self.config,
                    assets: &self.assets,
                    map_id: self.map_id,
                };
                let rows = self
                    .sessions
                    .clients()
                    .map(|(slot, c)| rcon::StatusClient {
                        slot,
                        score: game.score(slot),
                        state: c.state as i32,
                        ping: c.ping,
                        guid: c.guid,
                        name: &c.name,
                        last_message_age_ms: time.wrapping_sub(c.last_packet_ms),
                        address: Some(c.endpoint.peer),
                        bot_port: c.endpoint.bot_port,
                        qport: c.endpoint.qport,
                        rate: c.rate,
                    })
                    .collect::<Vec<_>>();
                return Some(rcon::status(&game.level.name, &rows));
            }
            Some("serverinfo") => return Some(rcon::server_info(&self.browser.advertised_info())),
            _ => {}
        }
        None
    }

    fn packet(&mut self, packet: &[u8], from: SocketAddrV4) -> io::Result<Vec<Vec<u8>>> {
        let Some(level) = self.map.as_mut() else {
            return Ok(Vec::new());
        };
        self.sessions.sync_bots(&level.world.bots)?;
        level.world.bots.occupied = self.sessions.occupied_slots();
        let Some(args) = oob_tokens(packet) else {
            let Some(level) = self.map.as_mut() else {
                return Ok(Vec::new());
            };
            let time = level.time_ms;
            let mut game = LiveGame {
                level,
                config: &mut self.config,
                assets: &self.assets,
                map_id: self.map_id,
            };
            let replies = self.sessions.packet(from, packet, time, &mut game)?;
            self.sessions.send_updates(game.take_updates(), true);
            return Ok(replies.into_iter().map(|r| r.bytes).collect());
        };
        let now = self.started.elapsed().as_millis() as i32;
        if args.first().map(Vec::as_slice) == Some(b"rcon") {
            let action =
                self.rcon
                    .handle(&args, self.config.get("rcon_password").unwrap_or(""), now);
            if !matches!(action, RconAction::Ignore) {
                let password = self.config.get("rcon_password").unwrap_or("");
                let valid =
                    !password.is_empty() && args.get(1).is_some_and(|a| a == password.as_bytes());
                let prefix = if valid { "Rcon" } else { "Bad rcon" };
                let command = args.get(2).map_or(&[][..], Vec::as_slice);
                self.print_console(
                    &[
                        format!("{prefix} from {from}:\n").as_bytes(),
                        command,
                        b"\n",
                    ]
                    .concat(),
                );
            }
            let text = match action {
                RconAction::Ignore => return Ok(Vec::new()),
                RconAction::Print(message) => message.as_bytes().to_vec(),
                RconAction::Execute(words) => {
                    let words: Vec<_> = words
                        .into_iter()
                        .map(|word| String::from_utf8_lossy(&word).into_owned())
                        .collect();
                    match self.config.command_insertion(&words, &mut &self.assets)? {
                        Some(insertion) => {
                            let mut output = insertion
                                .message
                                .map_or_else(Vec::new, |text| format!("{text}\n").into_bytes());
                            if let Some(text) = insertion.text {
                                let queued: usize =
                                    self.inserted_commands.iter().map(|s| s.len() + 1).sum();
                                if queued.saturating_add(text.len() + 1) <= 0x10000 {
                                    self.inserted_commands.push(text);
                                } else {
                                    output.extend(b"Cbuf_InsertText overflowed\n");
                                }
                            }
                            output
                        }
                        None => self.operator(&words),
                    }
                }
            };
            // SV_Shutdown also clears svs.redirectAddress before the RCON
            // redirect buffer is flushed, including on a killserver command.
            if self.quitting || self.map.is_none() {
                return Ok(Vec::new());
            }
            return Ok(rcon::print_packets(&text));
        }
        let Some(level) = self.map.as_mut() else {
            return Ok(Vec::new());
        };
        let now = level.time_ms;
        let lan = is_lan_address(*from.ip(), &self.local_addresses);
        match args.first().map(Vec::as_slice) {
            Some(b"v") => {
                self.sessions.voice_packet(
                    packet,
                    from,
                    now,
                    &self.config,
                    &mut level.world.players,
                )?;
                Ok(Vec::new())
            }
            Some(b"getinfo" | b"getstatus") => {
                self.master.note_contact(from, now);
                self.browser.players = query_players(&self.sessions, level);
                Ok(self.browser.response(packet).into_iter().collect())
            }
            Some(b"getchallenge")
                if lan && self.config.get("net_lanauthorize").unwrap_or("0") == "0" =>
            {
                self.master.note_contact(from, now);
                let reply = self.challenges.lan_response(from, now)?;
                self.challenges
                    .account_game_random(&mut level.world.native_random);
                Ok(vec![reply])
            }
            Some(b"getchallenge") => {
                self.master.note_contact(from, now);
                let pb_name = (args.len() == 3).then(|| args[2].as_slice());
                self.outbound.extend(self.master.request(
                    from,
                    now,
                    &mut self.challenges,
                    self.config.get("fs_game").unwrap_or(""),
                    self.config.get("sv_allowAnonymous") == Some("1"),
                    pb_name,
                )?);
                self.challenges
                    .account_game_random(&mut level.world.native_random);
                Ok(Vec::new())
            }
            Some(b"ipAuthorize") => {
                let demo = self.config.get("fs_restrict") == Some("1");
                let kick_ban_time = self
                    .config
                    .get("sv_kickBanTime")
                    .and_then(|v| v.parse().ok())
                    .unwrap_or(300.0);
                self.outbound.extend(self.master.authorize(
                    from,
                    now,
                    &args,
                    &mut self.challenges,
                    demo,
                    |guid| self.operator_state.bans.reject(guid, now, kick_ban_time),
                )?);
                Ok(Vec::new())
            }
            Some(b"connect") => {
                let number = |key: &str, default: i32| {
                    self.config
                        .get(key)
                        .and_then(|n| n.parse::<i32>().ok())
                        .unwrap_or(default)
                };
                let rules = AdmissionRules {
                    private_clients: number("sv_privateclients", 0).clamp(0, 64) as usize,
                    private_password: self
                        .config
                        .get("sv_privatepassword")
                        .unwrap_or("")
                        .as_bytes(),
                    game_password: self.config.get("g_password").unwrap_or("").as_bytes(),
                    reconnect_limit_ms: number("sv_reconnectlimit", 3).clamp(0, 1800) * 1000,
                };
                let slots = self.sessions.endpoints();
                let decision = admit(
                    from,
                    args.get(1).map_or(&[], Vec::as_slice),
                    now,
                    &mut self.challenges,
                    &slots,
                    &rules,
                    (!lan).then(|| (number("sv_minPing", 0), number("sv_maxPing", 0))),
                );
                if let Some((index, ping)) = self.challenges.connect_observation.take() {
                    self.sessions.console_lines.extend_from_slice(
                        format!(
                            "Client {index} connecting with {ping} challenge ping from {from}\n"
                        )
                        .as_bytes(),
                    );
                }
                match decision {
                    Admission::Denied(reply) => Ok(vec![reply]),
                    Admission::Silent => Ok(Vec::new()),
                    Admission::Eligible(client) => {
                        let mut game = LiveGame {
                            level,
                            config: &mut self.config,
                            assets: &self.assets,
                            map_id: self.map_id,
                        };
                        let result = self.sessions.connect(client, &mut game);
                        match result {
                            Ok(reply) => Ok(vec![reply]),
                            Err(e) => {
                                eprintln!("ClientConnect from {from}: {e}");
                                Ok(vec![
                                    [
                                        b"\xff\xff\xff\xfferror\n".as_slice(),
                                        b"Server could not initialize this player",
                                    ]
                                    .concat(),
                                ])
                            }
                        }
                    }
                }
            }
            _ => Ok(Vec::new()),
        }
    }
}

fn query_players(sessions: &ClientSessions, level: &Level) -> Vec<QueryPlayer> {
    sessions
        .clients()
        .filter(|(_, c)| c.state != ClientState::Zombie)
        .map(|(slot, c)| QueryPlayer {
            slot,
            score: level
                .world
                .players
                .get(slot as u16)
                .map_or(0, |player| player.score),
            ping: c.ping,
            name: c.name.clone(),
        })
        .collect()
}

fn run(args: &[String]) -> io::Result<()> {
    let mut config = ServerConfig::from_command_line(args)?;
    let requested_language = config
        .get("loc_language")
        .and_then(|s| s.parse().ok())
        .unwrap_or(0);
    let assets = GameAssets::mount_for_server(&mut config)?;
    let initial_search = assets.search_report(Some(requested_language));
    config.set_gametypes(assets.gametypes()?);
    let mut operator_state = operator::OperatorState::new(&assets)?;
    operator_state.print_console(
        &config,
        format!(
            "CoD2 MP 1.3 build {}-{} (Rust)\n",
            std::env::consts::OS,
            std::env::consts::ARCH
        )
        .as_bytes(),
    );
    operator_state.print_console(&config, initial_search.as_bytes());
    let startup_commands = config.initialize_common(&mut &assets)?;
    let startup_messages = std::mem::take(&mut config.messages);
    let port = config
        .get("net_port")
        .unwrap_or("28960")
        .parse::<u16>()
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "invalid net_port"))?;
    for message in startup_messages {
        operator_state.print_console(&config, format!("{message}\n").as_bytes());
    }
    let interface = config
        .get("net_ip")
        .filter(|name| !name.is_empty())
        .unwrap_or("localhost");
    let socket = open_socket(interface, port, |port| {
        operator_state.print_console(
            &config,
            format!("Opening IP socket: {interface}:{port}\n").as_bytes(),
        );
    })?;
    socket.set_nonblocking(true)?;
    let SocketAddr::V4(bound) = socket.local_addr()? else {
        unreachable!()
    };
    let mut local_addresses = vec![*bound.ip()];
    if let Some((name, addresses)) = hostname_addresses() {
        operator_state.print_console(&config, format!("Hostname: {name}\n").as_bytes());
        for address in addresses {
            operator_state.print_console(&config, format!("IP: {address}\n").as_bytes());
            if !bound.ip().is_loopback() && !local_addresses.contains(&address) {
                local_addresses.push(address);
            }
        }
    }
    let mut server = Server::new(
        assets,
        config.clone(),
        socket.try_clone()?,
        *bound.ip(),
        local_addresses,
        operator_state,
    )?;
    server.print_console(b"--- Common Initialization Complete ---\n");
    if !server.apply(config)? {
        return Ok(());
    }
    server.execute_console_text(&startup_commands, &mut Vec::new())?;
    if server.quitting {
        server.shutdown(&socket);
        return Ok(());
    }
    server.config.init_console_input();
    let (sender, console) = mpsc::sync_channel(32);
    std::thread::Builder::new()
        .name("console".into())
        .spawn(move || {
            for line in io::stdin().lock().lines().map_while(Result::ok) {
                if sender.send(line).is_err() {
                    break;
                }
            }
        })?;
    let mut buffer = [0u8; 65535];
    loop {
        // The dedicated outer loop sleeps for 5 ms. Com_EventLoop drains every
        // pending datagram before Cbuf_Execute and the fixed-step SV_Frame.
        std::thread::sleep(Duration::from_millis(5));
        server.read_packets(&socket, &mut buffer)?;
        if server.quitting {
            server.shutdown(&socket);
            return Ok(());
        }
        // Inserted RCON scripts run after Com_EndRedirect, at the head of the next console buffer.
        let inserted = std::mem::take(&mut server.inserted_commands);
        for line in inserted
            .into_iter()
            .rev()
            .chain(console.try_iter().take(32))
        {
            if let Err(error) = server.execute_console_text(&line, &mut Vec::new()) {
                server.print_console(format!("{error}\n").as_bytes());
            }
            if server.quitting {
                server.shutdown(&socket);
                return Ok(());
            }
        }
        server.frame(Instant::now())?;
        server.flush(&socket);
    }
}

fn main() -> std::process::ExitCode {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if args.iter().any(|arg| arg == "--help" || arg == "-h") {
        println!(
            "cod2-server [--update <1.3 tar.bz2>] [--game-seed <u32>] [--map-load-ms <u32>] +set fs_basepath <directory containing main> [+exec server.cfg] +map <mapname>"
        );
        return std::process::ExitCode::SUCCESS;
    }
    match run(&args) {
        Ok(()) => std::process::ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("cod2-server: {error}");
            std::process::ExitCode::FAILURE
        }
    }
}
