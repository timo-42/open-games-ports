//! Client sessions for protocol 118. Game-owned player state and
//! snapshot records come from ClientGame. Only connected/primed clients
//! receive the small pre-world svc_EOF message.
use crate::download::{Download, DownloadAction, DownloadClient};
use crate::level::ServerUpdate;
use crate::{
    admission::{ClientEndpoint, EligibleClient, info_integer, info_value},
    gamestate::GameState,
    message::{self, MessageReader, MessageWriter},
    netchan::{MAX_MESSAGE, Netchan},
    query::tokenize,
    usercmd::{PlayerInputState, UserCmd, read_moves},
};
use std::{io, net::SocketAddrV4};

fn unsupported(name: &str) -> io::Error {
    io::Error::new(
        io::ErrorKind::Unsupported,
        format!("client game binding unavailable: {name}"),
    )
}

pub trait ClientGame {
    /// ClientConnect has finished successfully before the connectResponse is emitted.
    /// Return reliable game commands in their original send order.
    fn connect(&mut self, slot: usize, userinfo: &[u8]) -> io::Result<Vec<Vec<u8>>>;
    fn connect_authenticated(
        &mut self,
        slot: usize,
        userinfo: &[u8],
        _guid: i32,
    ) -> io::Result<Vec<Vec<u8>>> {
        self.connect(slot, userinfo)
    }
    fn gamestate(&self) -> io::Result<GameState<'_>>;
    fn server_id(&self) -> u8;
    fn checksum_feed(&self) -> i32;
    fn pure_checksums(&self) -> Option<&[i32]>;
    fn command(&mut self, _slot: usize, _command: &[u8]) -> io::Result<Vec<Vec<u8>>> {
        Err(unsupported("ClientCommand"))
    }
    fn write_download(
        &mut self,
        _download: &mut Download,
        _msg: &mut MessageWriter,
        _client: DownloadClient,
    ) -> io::Result<()> {
        Err(unsupported("SV_WriteDownloadToClient"))
    }
    fn input_state(&self, _slot: usize) -> io::Result<PlayerInputState> {
        Err(unsupported("playerState"))
    }
    fn begin(&mut self, _slot: usize, _cmd: &UserCmd) -> io::Result<Vec<Vec<u8>>> {
        Err(unsupported("ClientBegin"))
    }
    fn think(&mut self, _slot: usize, _cmd: &UserCmd) -> io::Result<Vec<Vec<u8>>> {
        Err(unsupported("ClientThink"))
    }
    /// Append the live snapshot including its svc_snapshot opcode, without svc_EOF.
    fn snapshot(
        &mut self,
        _slot: usize,
        _sequence: i32,
        _delta: Option<i32>,
        _client_flags: u8,
        _msg: &mut MessageWriter,
    ) -> io::Result<()> {
        Err(unsupported("SV_WriteSnapshotToClient"))
    }
    /// The assembled snapshot was committed to the netchannel at this sequence.
    fn snapshot_sent(&mut self, _slot: usize, _sequence: i32) {}
    fn take_updates(&mut self) -> Vec<ServerUpdate> {
        Vec::new()
    }
    fn pending_drops(&mut self) -> Vec<(usize, String)> {
        Vec::new()
    }
    fn score(&self, _slot: usize) -> i32 {
        0
    }
    fn disconnect(&mut self, slot: usize, reason: &str) -> io::Result<()>;
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Datagram {
    pub peer: SocketAddrV4,
    pub bytes: Vec<u8>,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum ClientState {
    Zombie = 1,
    Connected = 2,
    Primed = 3,
    Active = 4,
}

#[derive(Debug, Clone)]
struct Reliable {
    commands: [Vec<u8>; 128],
    optional: [bool; 128],
    exhausted: bool,
    sequence: i32,
    sent: i32,
}
impl Default for Reliable {
    fn default() -> Self {
        Self {
            commands: std::array::from_fn(|_| Vec::new()),
            optional: [false; 128],
            exhausted: false,
            sequence: 0,
            sent: 0,
        }
    }
}

impl Reliable {
    fn at(&self, sequence: i32) -> &[u8] {
        &self.commands[(sequence & 127) as usize]
    }

    // SV_FindReliableCommand only coalesces commands which have never been sent.
    fn queue(&mut self, ack: i32, text: &[u8]) -> io::Result<()> {
        self.queue_with_priority(ack, text, false, true)
    }

    fn queue_with_priority(
        &mut self,
        ack: i32,
        text: &[u8],
        optional: bool,
        active: bool,
    ) -> io::Result<()> {
        if !active || self.sequence.wrapping_sub(ack) >= 64 {
            // SV_CompactReliableCommands drops unsent optional messages while
            // preserving the sequence and key of every transmitted command.
            let mut destination = self.sent;
            for sequence in self.sent.saturating_add(1)..=self.sequence {
                let index = (sequence & 127) as usize;
                if self.optional[index] {
                    continue;
                }
                destination += 1;
                if destination != sequence {
                    let target = (destination & 127) as usize;
                    self.commands[target] = std::mem::take(&mut self.commands[index]);
                    self.optional[target] = false;
                }
            }
            self.sequence = destination;
            if optional {
                return Ok(());
            }
        }
        let text: Vec<_> = text
            .iter()
            .take(1023)
            .take_while(|b| **b != 0)
            .map(|&b| match b {
                b'%' => b'.',
                0x92 => b'\'',
                _ => b,
            })
            .collect();
        let matches = |old: &[u8]| {
            let Some(&op) = text.first() else {
                return false;
            };
            if old.first() != Some(&op) || matches!(op, b'x'..=b'z') {
                return false;
            }
            if old == text {
                return true;
            }
            match op {
                b'C' | b'D' | b'a' | b'b' | b'o' | b'p' | b'q' | b'r' | b't' => true,
                b'd' | b'v' => {
                    old.get(2..).and_then(|s| s.split(|&b| b == b' ').next())
                        == text.get(2..).and_then(|s| s.split(|&b| b == b' ').next())
                }
                _ => false,
            }
        };
        if let Some(found) = (self.sent.saturating_add(1)..=self.sequence)
            .find(|&i| !self.optional[(i & 127) as usize] && matches(self.at(i)))
        {
            for seq in found..self.sequence {
                self.commands[(seq & 127) as usize] =
                    std::mem::take(&mut self.commands[((seq + 1) & 127) as usize]);
                self.optional[(seq & 127) as usize] = self.optional[((seq + 1) & 127) as usize];
            }
        } else {
            if self.sequence.wrapping_sub(ack) >= 128 {
                self.exhausted = true;
                return Err(io::Error::other("EXE_SERVERCOMMANDOVERFLOW"));
            }
            self.sequence = self
                .sequence
                .checked_add(1)
                .ok_or_else(|| io::Error::other("reliable sequence wrapped"))?;
        }
        self.commands[(self.sequence & 127) as usize] = text;
        self.optional[(self.sequence & 127) as usize] = optional;
        Ok(())
    }
    fn write_pending(
        &self,
        ack: i32,
        msg: &mut MessageWriter,
        limit: Option<usize>,
    ) -> io::Result<i32> {
        let mut last = ack;
        for sequence in ack.saturating_add(1)..=self.sequence {
            let command = self.at(sequence);
            if limit.is_some_and(|limit| msg.as_bytes().len() + command.len() + 6 >= limit) {
                break;
            }
            msg.write_byte(4)?;
            msg.write_long(sequence)?;
            msg.write_string(command)?;
            last = sequence;
        }
        Ok(last)
    }

    fn pending(&self, ack: i32) -> Vec<Vec<u8>> {
        (ack.saturating_add(1)..=self.sequence)
            .map(|i| self.at(i).to_vec())
            .collect()
    }
}

#[derive(Debug)]
pub struct Session {
    pub guid: i32,
    pub endpoint: ClientEndpoint,
    pub state: ClientState,
    pub name: Vec<u8>,
    userinfo: Vec<u8>,
    voice: crate::voice::Listener,
    pub last_packet_ms: i32,
    pub rate: i32,
    is_lan: bool,
    rate_delayed: bool,
    pub ping: i32,
    channel: Netchan,
    challenge: i32,
    reliable: Reliable,
    last_command: i32,
    last_command_text: Vec<u8>,
    gamestate_sequence: i32,
    download: Download,
    snapshot_msec: i32,
    www_enabled: bool,
    pure: Option<bool>,
    delta: Option<i32>,
    send_as_active: bool,
    entity_attached: bool,
    next_send: i32,
    next_command: i32,
    missed_timeouts: u8,
    last_move: UserCmd,
    frames: [Option<(i32, Option<i32>)>; 32],
}

impl Session {
    fn from_admission(
        client: &EligibleClient<'_>,
        local_dedicated: bool,
        console: &mut Vec<u8>,
    ) -> Self {
        let now = client.endpoint.last_connect_ms;
        let mut session = Self {
            guid: client.guid,
            endpoint: client.endpoint,
            channel: Netchan::new(
                client.endpoint.peer,
                client.endpoint.qport as u16,
                client.challenge,
            ),
            challenge: client.challenge,
            state: ClientState::Connected,
            name: Vec::new(),
            userinfo: Vec::new(),
            voice: Default::default(),
            last_packet_ms: now,
            rate: 5000,
            is_lan: client.is_lan,
            rate_delayed: false,
            ping: 999,
            last_command: 0,
            last_command_text: Vec::new(),
            reliable: Reliable::default(),
            gamestate_sequence: -1,
            download: Download::default(),
            snapshot_msec: 50,
            www_enabled: false,
            pure: None,
            delta: None,
            send_as_active: false,
            entity_attached: true,
            next_send: now,
            next_command: 0,
            missed_timeouts: 0,
            last_move: UserCmd::default(),
            frames: [None; 32],
        };
        console.extend_from_slice(
            format!(
                "Going from CS_FREE to CS_CONNECTED for {} (num {} guid {})\n",
                String::from_utf8_lossy(&session.name),
                client.slot,
                client.guid
            )
            .as_bytes(),
        );
        session.identify(client.userinfo, local_dedicated);
        session
    }

    pub fn is_bot(&self) -> bool {
        self.endpoint.bot_port.is_some()
    }
    pub fn userinfo(&self) -> &[u8] {
        &self.userinfo
    }

    fn identify(&mut self, info: &[u8], local_dedicated: bool) {
        let name = info_value(info, b"name");
        self.name = name[..name.len().min(31)].to_vec();
        self.userinfo = info.to_vec();
        self.www_enabled = info_integer(info_value(info, b"cl_wwwDownload")) != 0;
        let snaps = info_value(info, b"snaps");
        self.snapshot_msec = if snaps.is_empty() {
            50
        } else {
            1000 / info_integer(snaps).clamp(1, 30)
        };
        let rate = info_value(info, b"rate");
        self.rate = if local_dedicated && self.is_lan {
            99999
        } else if rate.is_empty() {
            5000
        } else {
            info_integer(rate).clamp(1000, 90000)
        };
        self.voice.enabled = self.rate >= 5000 && info_integer(info_value(info, b"cl_voice")) > 0;
    }

    fn queue(&mut self, commands: impl AsRef<[Vec<u8>]>) -> io::Result<()> {
        self.queue_for_state(commands.as_ref(), self.state == ClientState::Active)
    }

    fn queue_for_state(&mut self, commands: &[Vec<u8>], active: bool) -> io::Result<()> {
        if self.is_bot() {
            return Ok(());
        }
        let ack = self.channel.reliable_ack();
        let queue = |ring: &mut Reliable, command: &[u8]| {
            // Gameplay notices, pickup hints, chat and local sounds use command type 0.
            let optional = matches!(
                command,
                [b'c' | b'e'..=b'i' | b's' | b'G'..=b'I', b' ', ..] | [b' ', b'"', ..]
            );
            ring.queue_with_priority(ack, command, optional, active)
        };
        match commands {
            [] => Ok(()),
            [single] => queue(&mut self.reliable, single),
            _ => {
                let mut staged = self.reliable.clone();
                for command in commands {
                    if let Err(error) = queue(&mut staged, command) {
                        self.reliable.exhausted |= staged.exhausted;
                        return Err(error);
                    }
                }
                self.reliable = staged;
                Ok(())
            }
        }
    }

    fn rate_msec(&self, size: usize, max_rate: i32) -> i32 {
        let rate = if max_rate == 0 {
            self.rate
        } else {
            self.rate.min(max_rate.max(1000))
        };
        (size.min(1500) as i32 * 1000 + 48000) / rate
    }

    fn schedule_send(&mut self, now: i32, length: usize, max_rate: i32) {
        if self.is_lan {
            self.next_send = now.wrapping_sub(1);
            return;
        }
        let delay = self.rate_msec(length, max_rate);
        self.rate_delayed = delay >= self.snapshot_msec;
        let mut interval = delay.max(self.snapshot_msec);
        if self.state != ClientState::Active && !self.download.active() {
            interval = interval.max(1000);
        }
        self.next_send = now.wrapping_add(interval);
    }

    fn send(&mut self, raw: &[u8], now: i32, max_rate: i32) -> io::Result<Datagram> {
        let ack = i32::from_le_bytes(raw.get(..4).and_then(|b| b.try_into().ok()).ok_or_else(
            || io::Error::other("server message lacks client command acknowledgement"),
        )?);
        let compressed = message::compress(&raw[4..], MAX_MESSAGE - 4)?;
        let sequence = self.channel.outgoing_sequence();
        let bytes = self
            .channel
            .transmit(ack, &compressed, &self.last_command_text)?;
        self.frames[(sequence & 31) as usize] = Some((now, None));
        self.schedule_send(now, 4 + compressed.len(), max_rate);
        Ok(Datagram {
            peer: self.channel.peer(),
            bytes,
        })
    }

    fn send_gamestate(
        &mut self,
        slot: usize,
        now: i32,
        max_rate: i32,
        game: &impl ClientGame,
    ) -> io::Result<Vec<Datagram>> {
        let raw = game.gamestate()?.serialize(
            self.last_command,
            self.reliable.sequence,
            &self.reliable.pending(self.channel.reliable_ack()),
            slot as i32,
            game.checksum_feed(),
        )?;
        let mut packets = Vec::new();
        while let Some(bytes) = self.channel.next_fragment() {
            packets.push(Datagram {
                peer: self.channel.peer(),
                bytes,
            });
        }
        let sequence = self.channel.outgoing_sequence() as i32;
        packets.push(self.send(&raw, now, max_rate)?);
        self.gamestate_sequence = sequence;
        self.reliable.sent = self.reliable.sequence;
        self.state = ClientState::Primed;
        self.pure = None;
        Ok(packets)
    }

    fn write_snapshot(
        &mut self,
        slot: usize,
        now: i32,
        msg: &mut MessageWriter,
        game: &mut impl ClientGame,
    ) -> io::Result<()> {
        let active = matches!(self.state, ClientState::Active | ClientState::Zombie);
        let sequence = self.channel.outgoing_sequence() as i32;
        msg.write_long(self.last_command)?;
        if active {
            self.reliable
                .write_pending(self.channel.reliable_ack(), msg, None)?;
            let send_as_active = self.state == ClientState::Active || self.send_as_active;
            let delta = if self.state == ClientState::Active {
                self.delta
            } else {
                None
            };
            game.snapshot(
                slot,
                sequence,
                delta,
                (u8::from(!send_as_active) << 1) | u8::from(self.rate_delayed),
                msg,
            )?;
        }
        if self.state != ClientState::Zombie && self.download.active() {
            game.write_download(
                &mut self.download,
                msg,
                DownloadClient {
                    now,
                    rate: self.rate,
                    snapshot_msec: self.snapshot_msec,
                    www_enabled: self.www_enabled,
                },
            )?;
        }
        msg.write_byte(7)
    }

    fn snapshot(
        &mut self,
        slot: usize,
        now: i32,
        max_rate: i32,
        game: &mut impl ClientGame,
    ) -> io::Result<Datagram> {
        let mut msg = MessageWriter::new(MAX_MESSAGE);
        let active = matches!(self.state, ClientState::Active | ClientState::Zombie);
        let sequence = self.channel.outgoing_sequence() as i32;
        match self.write_snapshot(slot, now, &mut msg, game) {
            Ok(()) => {
                let reply = self.send(msg.as_bytes(), now, max_rate)?;
                if active {
                    self.send_as_active |= self.state == ClientState::Active;
                    game.snapshot_sent(slot, sequence);
                    self.reliable.sent = self.reliable.sequence;
                }
                Ok(reply)
            }
            Err(error) if active && message::capacity_exceeded(&error) => {
                // SV_SendClientSnapshot recovers with just the commands that fit.
                let mut msg = MessageWriter::new(MAX_MESSAGE);
                msg.write_long(self.last_command)?;
                let sent = self.reliable.write_pending(
                    self.channel.reliable_ack(),
                    &mut msg,
                    Some(MAX_MESSAGE),
                )?;
                msg.write_byte(7)?;
                let reply = self.send(msg.as_bytes(), now, max_rate)?;
                self.reliable.sent = self.reliable.sent.max(sent);
                Ok(reply)
            }
            Err(error) => Err(error),
        }
    }

    fn become_active(
        &mut self,
        slot: usize,
        cmd: &UserCmd,
        now: i32,
        game: &mut impl ClientGame,
    ) -> io::Result<()> {
        let commands = game.begin(slot, cmd)?;
        // SV_ClientEnterWorld treats ClientBegin's commands as active even on the first move.
        self.queue_for_state(&commands, true)?;
        self.last_move = *cmd;
        self.state = ClientState::Active;
        self.entity_attached = true;
        self.next_send = now;
        self.delta = None;
        Ok(())
    }
}

#[derive(Debug)]
pub struct ClientSessions {
    pub console_lines: Vec<u8>,
    slots: Vec<Option<Session>>,
    released_challenges: Vec<SocketAddrV4>,
    pub timeout_ms: i32,
    pub zombie_ms: i32,
    pub max_rate: i32,
    pub local_dedicated: bool,
    pub flood_protect: bool,
    /// Per-slot tick errors are separate so that failed game output for one
    /// slot cannot consume datagrams which another slot has already produced.
    pub tick_errors: Vec<(usize, io::Error)>,
}

impl ClientSessions {
    pub fn new(max_clients: usize) -> Self {
        Self {
            console_lines: Vec::new(),
            slots: (0..max_clients.min(64)).map(|_| None).collect(),
            released_challenges: Vec::new(),
            timeout_ms: 240000,
            zombie_ms: 2000,
            max_rate: 0,
            local_dedicated: false,
            flood_protect: true,
            tick_errors: Vec::new(),
        }
    }
    /// Match the connectionless sender by its base address and signed qport.
    pub fn voice_packet(
        &mut self,
        packet: &[u8],
        peer: SocketAddrV4,
        now: i32,
        config: &crate::config::ServerConfig,
        players: &mut crate::player::Players,
    ) -> io::Result<()> {
        use crate::voice::{VoicePlayer, VoiceRules, payload, read_frames};
        let Some(body) = payload(packet) else {
            return Ok(());
        };
        let mut message = MessageReader::new(body);
        let Ok(qport) = message.read_short() else {
            return Ok(());
        };
        let rules = VoiceRules::from_config(config);
        // Match SV_VoiceConnectionlessPacket's port fixup even on a disabled server.
        let slot = self.slots.iter().position(|entry| {
            entry.as_ref().is_some_and(|c| {
                !c.is_bot()
                    && c.endpoint.peer.ip() == peer.ip()
                    && c.endpoint.qport == i32::from(qport)
            })
        });
        let Some(slot) = slot else { return Ok(()) };
        let talker = self.slots[slot].as_mut().expect("matched voice sender");
        talker.channel.translate_port(peer.port());
        talker.endpoint.peer = peer;
        if talker.state == ClientState::Zombie {
            return Ok(());
        }
        talker.last_packet_ms = now;
        if !rules.enabled {
            return Ok(());
        }
        let active = talker.state == ClientState::Active;
        let describe = |p: &crate::player::Player| VoicePlayer {
            team: p.team(),
            session_state: p.session_state,
        };
        let speaker = players.get(slot as u16).map(describe).filter(|_| active);
        if active && speaker.is_none() {
            return Ok(());
        }
        read_frames(&mut message, active, |frame| {
            if active && let Ok(speaker) = players.get_mut(slot as u16) {
                speaker.last_voice_time = now;
            }
            for (listener_slot, entry) in self.slots.iter_mut().enumerate() {
                let Some(listener) = entry else { continue };
                if listener.is_bot() || listener.state == ClientState::Zombie {
                    continue;
                }
                let target = players.get(listener_slot as u16).map(describe);
                if rules.audible(speaker, target, listener_slot == slot) {
                    listener.voice.enqueue(slot as u8, frame);
                }
            }
        })
    }
    pub fn occupied_slots(&self) -> u64 {
        self.clients()
            .fold(0, |mask, (slot, _)| mask | (1u64 << slot))
    }

    /// GSC creates its player synchronously. Attach a CS_ACTIVE transport record before
    /// processing subsequent packets, preserving the engine's single shared slot namespace.
    pub fn sync_bots(&mut self, bots: &crate::bots::BotClients) -> io::Result<()> {
        for (&number, bot) in &bots.clients {
            let slot = usize::from(number);
            let entry = self
                .slots
                .get_mut(slot)
                .ok_or_else(|| io::Error::other("test client exceeds session capacity"))?;
            if let Some(session) = entry {
                if !session.is_bot() {
                    return Err(io::Error::other("test client overlaps a network client"));
                }
                session.last_move = bot.last_command;
                continue;
            }
            let client = EligibleClient {
                slot,
                endpoint: ClientEndpoint {
                    bot_port: Some(bot.number as u16),
                    peer: SocketAddrV4::new(std::net::Ipv4Addr::UNSPECIFIED, 0),
                    qport: 0,
                    last_connect_ms: bot.connected_at,
                },
                userinfo: &bot.userinfo,
                challenge: 0,
                guid: 0,
                is_lan: false,
            };
            self.console_lines.extend_from_slice(
                format!("Connecting player #{slot} has a zero GUID\n").as_bytes(),
            );
            let mut session =
                Session::from_admission(&client, self.local_dedicated, &mut self.console_lines);
            session.state = ClientState::Active;
            *entry = Some(session);
        }
        Ok(())
    }

    pub fn take_released_challenges(&mut self) -> Vec<SocketAddrV4> {
        std::mem::take(&mut self.released_challenges)
    }

    pub fn endpoints(&self) -> Vec<Option<ClientEndpoint>> {
        self.slots
            .iter()
            .map(|c| c.as_ref().map(|c| c.endpoint))
            .collect()
    }
    pub fn clients(&self) -> impl Iterator<Item = (usize, &Session)> {
        self.slots
            .iter()
            .enumerate()
            .filter_map(|(i, c)| Some((i, c.as_ref()?)))
    }

    pub fn connect(
        &mut self,
        client: EligibleClient<'_>,
        game: &mut impl ClientGame,
    ) -> io::Result<Vec<u8>> {
        if self.slots.get(client.slot).is_none() {
            return Err(io::Error::other("invalid admission slot"));
        }
        if self.slots[client.slot]
            .as_ref()
            .is_some_and(|old| old.state != ClientState::Zombie)
        {
            game.disconnect(client.slot, "reconnect")?;
            self.send_updates(game.take_updates(), true);
        }
        self.slots[client.slot] = None;
        if client.guid == 0 {
            self.console_lines.extend_from_slice(
                format!("Connecting player #{} has a zero GUID\n", client.slot).as_bytes(),
            );
        }
        let commands = game.connect_authenticated(client.slot, client.userinfo, client.guid)?;
        let mut session =
            Session::from_admission(&client, self.local_dedicated, &mut self.console_lines);
        if let Err(error) = session.queue(commands) {
            game.disconnect(client.slot, "EXE_SERVERCOMMANDOVERFLOW")?;
            return Err(error);
        }
        self.slots[client.slot] = Some(session);
        self.send_updates(game.take_updates(), true);
        Ok(b"\xff\xff\xff\xffconnectResponse".to_vec())
    }

    /// SV_ResizeClients keeps all occupied high slots even when maxclients is lowered.
    pub fn resize(&mut self, requested: usize) -> usize {
        let occupied = self
            .slots
            .iter()
            .rposition(|slot| {
                slot.as_ref()
                    .is_some_and(|c| c.state != ClientState::Zombie)
            })
            .map_or(0, |i| i + 1);
        let size = requested.clamp(1, 64).max(occupied);
        if size != self.slots.len() {
            for entry in &mut self.slots {
                if entry
                    .as_ref()
                    .is_some_and(|c| c.state == ClientState::Zombie)
                {
                    *entry = None;
                }
            }
            self.slots.resize_with(size, || None);
        }
        size
    }

    pub fn capacity(&self) -> usize {
        self.slots.len()
    }

    /// A full map load re-primes clients. A fast restart keeps the channel,
    /// pure state and already acknowledged gamestate; active clients reenter.
    pub fn restart_clients(
        &mut self,
        fast: bool,
        keep: bool,
        now: i32,
        game: &mut impl ClientGame,
    ) -> Vec<Datagram> {
        let mut packets = Vec::new();
        for slot in 0..self.slots.len() {
            let entry = &mut self.slots[slot];
            let Some(client) = entry else {
                continue;
            };
            if client.state == ClientState::Zombie {
                continue;
            }
            client.entity_attached = false;
            let result = (|| {
                if fast {
                    client.queue(vec![if keep { b"n".to_vec() } else { b"B".to_vec() }])?;
                }
                let commands = game.connect_authenticated(slot, &client.userinfo, client.guid)?;
                client.queue(commands)?;
                if fast {
                    if client.state == ClientState::Active {
                        let last = client.last_move;
                        client.become_active(slot, &last, now, game)?;
                    }
                } else {
                    client.state = ClientState::Connected;
                    client.send_as_active = false;
                    client.delta = None;
                    client.gamestate_sequence = -1;
                    client.pure = None;
                }
                Ok(())
            })();
            match result {
                Ok(()) => (),
                Err(error) => {
                    self.tick_errors.push((slot, error));
                    if !client.is_bot() {
                        packets.push(Datagram {
                            peer: client.endpoint.peer,
                            bytes: b"\xff\xff\xff\xffdisconnect".to_vec(),
                        });
                    }
                    *entry = None;
                }
            }
            self.send_updates(game.take_updates(), fast);
        }
        packets
    }

    /// Broadcast changed configstrings in setter order. Long values use x/y/z
    /// so that retransmission can rebuild a value larger than one command.
    pub fn send_updates(&mut self, changes: Vec<ServerUpdate>, send_configstrings: bool) {
        for change in changes {
            let (index, value) = match change {
                ServerUpdate::Targeted(slot, cmd) => {
                    let index = usize::from(slot);
                    if let Some(client) = self.slots.get_mut(index).and_then(Option::as_mut)
                        && client.state != ClientState::Zombie
                        && let Err(error) = client.queue([cmd])
                    {
                        self.tick_errors.push((index, error));
                    }
                    continue;
                }
                ServerUpdate::Broadcast(cmd) => {
                    self.broadcast_commands(&[cmd]);
                    continue;
                }
                ServerUpdate::ConfigString(index, value) if send_configstrings => (index, value),
                ServerUpdate::ConfigString(_, _) => continue,
            };
            let prefix = format!(" {index} ");
            let chunk_size = 1023 - prefix.len() - 1;
            let mut commands = Vec::new();
            if value.len() <= chunk_size {
                commands.push([b"d".as_slice(), prefix.as_bytes(), &value].concat());
            } else {
                let mut chunks = value.chunks(chunk_size).peekable();
                while let Some(chunk) = chunks.next() {
                    let letter = if commands.is_empty() {
                        b'x'
                    } else if chunks.peek().is_none() {
                        b'z'
                    } else {
                        b'y'
                    };
                    commands.push([&[letter], prefix.as_bytes(), chunk].concat());
                }
            }
            self.broadcast_commands(&commands);
        }
    }
    fn broadcast_commands(&mut self, commands: &[Vec<u8>]) {
        for (slot, c) in self.slots.iter_mut().enumerate() {
            let Some(c) = c else { continue };
            if matches!(c.state, ClientState::Primed | ClientState::Active)
                && let Err(error) = c.queue(commands)
            {
                self.tick_errors.push((slot, error));
            }
        }
    }
    pub fn queue_command(&mut self, slot: usize, command: &[u8]) -> io::Result<()> {
        let client = self
            .slots
            .get_mut(slot)
            .and_then(Option::as_mut)
            .ok_or_else(|| io::Error::other("inactive client slot"))?;
        if client.is_bot() {
            return Ok(());
        }
        client
            .reliable
            .queue(client.channel.reliable_ack(), command)
    }

    /// SV_SendServerCommand(cl, 0, ...) is optional when the reliable queue is busy.
    pub fn queue_notice(&mut self, slot: usize, text: &[u8]) -> io::Result<()> {
        let client = self
            .slots
            .get_mut(slot)
            .and_then(Option::as_mut)
            .ok_or_else(|| io::Error::other("inactive client slot"))?;
        if client.is_bot() {
            return Ok(());
        }
        client.reliable.queue_with_priority(
            client.channel.reliable_ack(),
            text,
            true,
            client.state == ClientState::Active,
        )
    }

    /// SV_DropAllClients sends two final snapshots while the game is live.
    pub fn final_snapshots(
        &mut self,
        now: i32,
        reason: &str,
        game: &mut impl ClientGame,
    ) -> Vec<Datagram> {
        let notice = format!("e \"{reason}\"");
        let close = format!("w \"{reason}\"");
        let mut packets = Vec::new();
        for _ in 0..2 {
            for (slot, entry) in self.slots.iter_mut().enumerate() {
                let Some(client) = entry else { continue };
                if client.is_bot() || client.state == ClientState::Zombie {
                    continue;
                }
                let ack = client.channel.reliable_ack();
                let active = client.state == ClientState::Active;
                let send = (|| {
                    client
                        .reliable
                        .queue_with_priority(ack, notice.as_bytes(), true, active)?;
                    client
                        .reliable
                        .queue_with_priority(ack, close.as_bytes(), false, active)?;
                    while let Some(bytes) = client.channel.next_fragment() {
                        packets.push(Datagram {
                            peer: client.endpoint.peer,
                            bytes,
                        });
                    }
                    client.snapshot(slot, now, self.max_rate, game)
                })();
                match send {
                    Ok(packet) => packets.push(packet),
                    Err(error) => self.tick_errors.push((slot, error)),
                }
            }
        }
        packets
    }

    /// SV_KickClient and SV_BanClient start a fresh zombie interval after the drop.
    pub fn kick_client(
        &mut self,
        slot: usize,
        now: i32,
        game: &mut impl ClientGame,
    ) -> io::Result<()> {
        self.drop_client(slot, "EXE_PLAYERKICKED", now, game)?;
        if let Some(client) = self.slots.get_mut(slot).and_then(Option::as_mut) {
            client.last_packet_ms = now;
        }
        Ok(())
    }

    pub fn drop_client(
        &mut self,
        slot: usize,
        reason: &str,
        _now: i32,
        game: &mut impl ClientGame,
    ) -> io::Result<()> {
        let Some(client) = self.slots.get(slot).and_then(Option::as_ref) else {
            return Ok(());
        };
        if client.state == ClientState::Zombie {
            return Ok(());
        }
        let name = client.name.clone();
        game.disconnect(slot, reason)?;
        self.console_lines
            .extend_from_slice(format!("{slot}:").as_bytes());
        self.console_lines.extend_from_slice(&name);
        self.console_lines
            .extend_from_slice(format!(" {reason}\n").as_bytes());
        self.send_updates(game.take_updates(), true);
        let session = self.slots[slot].as_mut().expect("occupied slot");
        if !session.entity_attached {
            self.released_challenges.push(session.endpoint.peer);
        }
        session.state = ClientState::Zombie;
        session.download = Download::default();
        session.name.clear();
        session.userinfo.clear();
        session.delta = None;
        // Keys present in the stock localized strings. The original WWW protocol error
        // has a misspelling and receives the ordinary unlocalized drop text.
        let localized = matches!(
            reason,
            "EXE_DISCONNECTED"
                | "GAME_DROPPEDFORINACTIVITY"
                | "EXE_TIMEDOUT"
                | "EXE_LOSTRELIABLECOMMANDS"
                | "EXE_UNPURECLIENTDETECTED"
                | "EXE_CANNOTVALIDATEPURECLIENT"
                | "EXE_PLAYERKICKED"
                | "EXE_SERVERCOMMANDOVERFLOW"
                | "PC_PATCH_1_1_DOWNLOADDISCONNECTED"
        );
        let command = if localized {
            format!("w \"{reason}\"").into_bytes()
        } else {
            [
                b"w \"".as_slice(),
                &name,
                b"^7 ",
                reason.as_bytes(),
                b"\" PB",
            ]
            .concat()
        };
        if session.reliable.exhausted {
            // The OOB disconnect has no reliable-window dependency. Where an
            // unsent slot exists, the terminal command can replace that slot
            // without changing the key of any packet previously transmitted.
            if session.reliable.sent < session.reliable.sequence {
                let last = (session.reliable.sequence & 127) as usize;
                session.reliable.commands[last] = command;
                session.reliable.optional[last] = false;
            }
        } else if let Err(e) = session.queue([command]) {
            self.tick_errors.push((slot, e));
        }
        let leave = format!("J {slot}");
        let notice = [
            b"e \"\x15".as_slice(),
            &name,
            b"^7 ",
            if localized { b"\x14" } else { b"" },
            reason.as_bytes(),
            b"\"",
        ]
        .concat();
        for (index, entry) in self.slots.iter_mut().enumerate() {
            let Some(other) = entry else { continue };
            if !other.is_bot() && matches!(other.state, ClientState::Primed | ClientState::Active) {
                let ack = other.channel.reliable_ack();
                let active = other.state == ClientState::Active;
                if reason != "EXE_DISCONNECTED"
                    && let Err(e) = other
                        .reliable
                        .queue_with_priority(ack, &notice, true, active)
                {
                    self.tick_errors.push((index, e));
                }
                if let Err(e) =
                    other
                        .reliable
                        .queue_with_priority(ack, leave.as_bytes(), false, active)
                {
                    self.tick_errors.push((index, e));
                }
            }
        }
        Ok(())
    }

    pub fn tick(&mut self, now: i32, game: &mut impl ClientGame) -> io::Result<Vec<Datagram>> {
        let mut packets = Vec::new();
        for (slot, reason) in game.pending_drops() {
            if let Err(error) = self.drop_client(slot, &reason, now, game) {
                self.tick_errors.push((slot, error));
            }
        }
        for slot in 0..self.slots.len() {
            let Some(client) = self.slots[slot].as_mut() else {
                continue;
            };
            if client.is_bot() {
                continue;
            }
            if client.reliable.exhausted && client.state != ClientState::Zombie {
                let peer = client.endpoint.peer;
                match self.drop_client(slot, "EXE_SERVERCOMMANDOVERFLOW", now, game) {
                    Ok(()) => packets.push(Datagram {
                        peer,
                        bytes: b"\xff\xff\xff\xffdisconnect".to_vec(),
                    }),
                    Err(error) => self.tick_errors.push((slot, error)),
                }
                continue;
            }
            client.last_packet_ms = client.last_packet_ms.min(now);
            let age = now.wrapping_sub(client.last_packet_ms);
            if client.state == ClientState::Zombie && age > self.zombie_ms {
                self.slots[slot] = None;
                continue;
            }
            if client.state != ClientState::Zombie && age > self.timeout_ms {
                client.missed_timeouts = client.missed_timeouts.saturating_add(1);
                if client.missed_timeouts > 5 {
                    match self.drop_client(slot, "EXE_TIMEDOUT", now, game) {
                        Ok(()) => self.slots[slot] = None,
                        Err(e) => self.tick_errors.push((slot, e)),
                    }
                }
            } else {
                client.missed_timeouts = 0;
            }
        }
        for (slot, entry) in self.slots.iter_mut().enumerate() {
            let Some(client) = entry else { continue };
            if client.is_bot() || client.next_send > now {
                continue;
            }
            let pending = client.channel.pending_bytes();
            if let Some(bytes) = client.channel.next_fragment() {
                let delay = client.rate_msec(pending, self.max_rate);
                client.next_send = now.wrapping_add(delay);
                packets.push(Datagram {
                    peer: client.channel.peer(),
                    bytes,
                });
            } else {
                match client.snapshot(slot, now, self.max_rate, game) {
                    Ok(packet) => packets.push(packet),
                    Err(e) => self.tick_errors.push((slot, e)),
                }
                if client.state == ClientState::Active
                    && let Some(bytes) = client.voice.take_packet()
                {
                    packets.push(Datagram {
                        peer: client.channel.peer(),
                        bytes,
                    });
                }
            }
        }
        Ok(packets)
    }

    pub fn packet(
        &mut self,
        peer: SocketAddrV4,
        packet: &[u8],
        now: i32,
        game: &mut impl ClientGame,
    ) -> io::Result<Vec<Datagram>> {
        let Some((slot, client)) = self.slots.iter_mut().enumerate().find_map(|(i, c)| {
            let c = c.as_mut()?;
            (!c.is_bot() && c.channel.matches(peer, packet)).then_some((i, c))
        }) else {
            return Ok(vec![Datagram {
                peer,
                bytes: b"\xff\xff\xff\xffdisconnect".to_vec(),
            }]);
        };
        client.endpoint.peer = peer;
        let Some(mut payload) = client
            .channel
            .receive(peer, packet, client.reliable.sequence)
        else {
            return Ok(vec![]);
        };
        if client.state == ClientState::Zombie {
            return Ok(vec![]);
        }
        client.last_packet_ms = now;
        if payload.server_id != game.server_id() && !client.download.active() {
            if payload.server_id & 0xf0 == game.server_id() & 0xf0 {
                if client.state == ClientState::Primed {
                    let previous = client.last_move;
                    client.become_active(slot, &previous, now, game)?;
                }
            } else if client.gamestate_sequence < payload.message_ack {
                return client.send_gamestate(slot, now, self.max_rate, game);
            }
            return Ok(vec![]);
        }
        payload.decode(client.challenge, client.reliable.at(payload.reliable_ack));
        let body = message::decompress(&payload.body, MAX_MESSAGE)?;
        let mut msg = MessageReader::new(&body);
        let mut outgoing = Vec::new();
        loop {
            let client = self.slots[slot].as_mut().expect("matched client");
            let opcode = msg.read_bits(3)?;
            if opcode == 2 {
                let sequence = msg.read_long()?;
                let text = msg.read_string()?;
                if sequence <= client.last_command {
                    continue;
                }
                if sequence != client.last_command.wrapping_add(1) {
                    self.drop_client(slot, "EXE_LOSTRELIABLECOMMANDS", now, game)?;
                    break;
                }
                let tokens = tokenize(&text);
                let first = tokens.first().map_or(&[][..], Vec::as_slice);
                let exempt = [b"team ".as_slice(), b"score ", b"mr "]
                    .iter()
                    .any(|p| text.starts_with(p));
                let throttled = self.flood_protect
                    && client.state == ClientState::Active
                    && !exempt
                    && now < client.next_command;
                if !exempt {
                    client.next_command = now.wrapping_add(800);
                }
                if let Some(action) = client.download.command(
                    first,
                    tokens.get(1).map_or(&[][..], Vec::as_slice),
                    now,
                ) {
                    match action {
                        DownloadAction::Continue => {}
                        DownloadAction::ResendGamestate => outgoing.extend(client.send_gamestate(
                            slot,
                            now,
                            self.max_rate,
                            game,
                        )?),
                        DownloadAction::Drop(reason) => {
                            client.last_command = sequence;
                            client.last_command_text = text;
                            self.drop_client(slot, reason, now, game)?;
                            break;
                        }
                    }
                    client.last_command = sequence;
                    client.last_command_text = text;
                    continue;
                }
                match first {
                    b"disconnect" => {
                        client.last_command = sequence;
                        client.last_command_text = text;
                        self.drop_client(slot, "EXE_DISCONNECTED", now, game)?;
                        break;
                    }
                    b"cp" => {
                        if let Some(checksums) = game.pure_checksums() {
                            client.pure =
                                Some(valid_report(&tokens, checksums, game.checksum_feed()));
                        }
                    }
                    b"vdr" => client.pure = None,
                    b"muteplayer" | b"unmuteplayer" => {
                        let talker = info_integer(tokens.get(1).map_or(&[], Vec::as_slice));
                        client.voice.mute(talker, first == b"muteplayer");
                    }
                    b"userinfo" => {
                        let updated = game.command(slot, &text)?;
                        client.identify(
                            tokens.get(1).map_or(&[][..], Vec::as_slice),
                            self.local_dedicated,
                        );
                        client.queue(updated)?;
                    }
                    _ if !throttled => client.queue(game.command(slot, &text)?)?,
                    _ => {}
                }
                client.last_command = sequence;
                client.last_command_text = text;
                // The next opcode can enter the world. Dispatch this command's
                // game output while its original slot state is still in effect.
                self.send_updates(game.take_updates(), true);
                continue;
            }
            if game.pure_checksums().is_some() && client.pure == Some(false) {
                self.drop_client(slot, "EXE_UNPURECLIENTDETECTED", now, game)?;
                break;
            }
            if opcode <= 1 {
                client.delta = (opcode == 0).then_some(payload.message_ack);
                let state = game.input_state(slot)?;
                let commands = read_moves(
                    &mut msg,
                    game.checksum_feed(),
                    payload.message_ack,
                    client.reliable.at(payload.reliable_ack),
                    &state,
                )?;
                let Some(first) = commands.first() else { break };
                if let Some((_, ack)) = &mut client.frames[(payload.message_ack & 31) as usize] {
                    *ack = Some(now);
                }
                if client.state == ClientState::Primed {
                    client.become_active(slot, first, now, game)?;
                }
                if game.pure_checksums().is_some() && client.pure != Some(true) {
                    self.drop_client(slot, "EXE_CANNOTVALIDATEPURECLIENT", now, game)?;
                    break;
                }
                if client.state == ClientState::Active {
                    let end = commands.last().expect("nonempty").server_time;
                    for cmd in commands {
                        if cmd.server_time > client.last_move.server_time && cmd.server_time <= end
                        {
                            client.queue(game.think(slot, &cmd)?)?;
                            client.last_move = cmd;
                        }
                    }
                    let (total, count) = client
                        .frames
                        .iter()
                        .filter_map(|f| match *f {
                            Some((time, Some(ack))) if ack > 0 => Some(ack.wrapping_sub(time)),
                            _ => None,
                        })
                        .fold((0i64, 0i64), |(sum, n), d| (sum + i64::from(d), n + 1));
                    client.ping = if count == 0 {
                        999
                    } else {
                        (total / count).min(999) as i32
                    };
                }
            }
            break;
        }
        Ok(outgoing)
    }
}

fn valid_report(tokens: &[Vec<u8>], server: &[i32], checksum_feed: i32) -> bool {
    if !tokens.get(1).is_some_and(|v| v.starts_with(b"@")) {
        return false;
    }
    let Some((reported, entries)) = tokens[2..].split_last() else {
        return false;
    };
    let mut seen = std::collections::BTreeSet::new();
    let mut check = checksum_feed ^ entries.len() as i32;
    for token in entries {
        let value = info_integer(token);
        if !seen.insert(value) || !server.contains(&value) {
            return false;
        }
        check ^= value;
    }
    check == info_integer(reported)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::entity_delta::EntityState;
    use crate::level::ConfigStrings;

    #[derive(Default)]
    struct TestGame {
        strings: ConfigStrings,
        baselines: Vec<EntityState>,
        disconnected: Vec<String>,
        download_assets: Option<(crate::assets::GameAssets, crate::config::ServerConfig)>,
    }
    impl ClientGame for TestGame {
        fn write_download(
            &mut self,
            download: &mut Download,
            message: &mut MessageWriter,
            client: DownloadClient,
        ) -> io::Result<()> {
            let (assets, config) = self.download_assets.as_mut().expect("download fixture");
            download.write(assets, config, message, client)
        }

        fn connect(&mut self, _slot: usize, _info: &[u8]) -> io::Result<Vec<Vec<u8>>> {
            Ok(vec![b"v x 1".to_vec()])
        }
        fn gamestate(&self) -> io::Result<GameState<'_>> {
            Ok(GameState {
                configstrings: &self.strings,
                baselines: &self.baselines,
            })
        }
        fn server_id(&self) -> u8 {
            16
        }
        fn begin(&mut self, _slot: usize, _cmd: &UserCmd) -> io::Result<Vec<Vec<u8>>> {
            Ok(vec![])
        }
        fn checksum_feed(&self) -> i32 {
            42
        }
        fn pure_checksums(&self) -> Option<&[i32]> {
            Some(&[1, 2])
        }
        fn disconnect(&mut self, _slot: usize, reason: &str) -> io::Result<()> {
            self.disconnected.push(reason.into());
            Ok(())
        }
    }
    fn peer() -> SocketAddrV4 {
        "127.0.0.1:32000".parse().expect("IPv4")
    }
    fn connect(game: &mut TestGame) -> io::Result<ClientSessions> {
        let mut sessions = ClientSessions::new(4);
        let eligible = EligibleClient {
            is_lan: true,
            guid: 0,
            slot: 2,
            endpoint: ClientEndpoint {
                bot_port: None,
                peer: peer(),
                qport: 0xbeef,
                last_connect_ms: 100,
            },
            challenge: 123,
            userinfo: br"\name\test",
        };
        assert_eq!(
            sessions.connect(eligible, game)?,
            b"\xff\xff\xff\xffconnectResponse"
        );
        Ok(sessions)
    }
    #[test]
    fn test_client_reserves_a_slot_without_sending_datagrams_or_timing_out() -> io::Result<()> {
        let mut sessions = ClientSessions::new(2);
        let mut bots = crate::bots::BotClients::default();
        bots.clients.insert(
            1,
            crate::bots::TestClient {
                number: 5,
                userinfo: br"\name\bot5\rate\5000".to_vec(),
                connected_at: 4000,
                last_command: Default::default(),
            },
        );
        sessions.sync_bots(&bots)?;
        assert_eq!(sessions.occupied_slots(), 2);
        let mut game = TestGame::default();
        for t in 0..8 {
            assert!(sessions.tick(300_000 + t, &mut game)?.is_empty());
        }
        let client = sessions.clients().next().expect("bot").1;
        assert_eq!(client.state, ClientState::Active);
        assert_eq!(client.rate, 5000);
        assert_eq!(client.name, b"bot5");
        assert!(
            sessions
                .final_snapshots(302_000, "EXE_SERVERQUIT", &mut game)
                .is_empty()
        );
        assert!(game.disconnected.is_empty());
        Ok(())
    }
    #[test]
    fn preworld_voice_tracks_qport_and_listener_userinfo() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        let mut players = crate::player::Players::default();
        let mut config = crate::config::ServerConfig::from_args(&[]);
        let talker = EligibleClient {
            is_lan: true,
            guid: 0,
            slot: 1,
            endpoint: ClientEndpoint {
                bot_port: None,
                peer: peer(),
                qport: 17,
                last_connect_ms: 100,
            },
            challenge: 321,
            userinfo: br"\name\talker",
        };
        sessions.connect(talker, &mut game)?;
        let translated = SocketAddrV4::new(*peer().ip(), peer().port() + 1);
        let packet = b"\xff\xff\xff\xffv\0\x11\0\x01\x02\0\x92\xff";
        sessions.voice_packet(packet, translated, 500, &config, &mut players)?;
        let sender = sessions.slots[1].as_ref().expect("connected fixture slot");
        assert_eq!(sender.last_packet_ms, 500);
        assert_eq!(sender.channel.peer(), translated);
        config.execute("set sv_voice 1", |_| Ok(String::new()))?;
        sessions.slots[2]
            .as_mut()
            .expect("connected fixture slot")
            .identify(br"\cl_voice\1\rate\4999", false);
        sessions.voice_packet(packet, translated, 550, &config, &mut players)?;
        assert!(
            sessions.slots[2]
                .as_mut()
                .expect("connected fixture slot")
                .voice
                .take_packet()
                .is_none()
        );
        sessions.slots[2]
            .as_mut()
            .expect("connected fixture slot")
            .identify(br"\cl_voice\1\rate\5000", false);
        sessions.voice_packet(packet, translated, 600, &config, &mut players)?;
        assert_eq!(
            sessions.slots[2]
                .as_mut()
                .expect("connected fixture slot")
                .voice
                .take_packet()
                .as_deref(),
            Some(b"\xff\xff\xff\xffv\0\x01\x01\x02\x92\xff".as_slice())
        );
        Ok(())
    }

    #[test]
    fn primed_download_commands_transfer_a_mounted_pack_and_resend_gamestate() -> io::Result<()> {
        use std::io::{Cursor, Write};
        let tmp = tempfile::tempdir()?;
        let main = tmp.path().join("main");
        std::fs::create_dir(&main)?;
        let mut archive = zip::ZipWriter::new(Cursor::new(Vec::new()));
        archive.start_file("map.cfg", zip::write::SimpleFileOptions::default())?;
        archive.write_all(b"set name example")?;
        let archive = archive.finish()?.into_inner();
        std::fs::write(main.join("extra.iwd"), &archive)?;
        let mut config = crate::config::ServerConfig::from_args(&[]);
        config.register_game();
        let mut game = TestGame {
            download_assets: Some((
                crate::assets::GameAssets::mount_paths(tmp.path(), None, "")?,
                config,
            )),
            ..Default::default()
        };
        let mut sessions = connect(&mut game)?;
        sessions.slots[2]
            .as_mut()
            .expect("connected fixture slot")
            .state = ClientState::Primed;
        let command_packet = |seq: i32, command: &[u8]| -> io::Result<Vec<u8>> {
            let mut writer = MessageWriter::new(1024);
            writer.write_bits(2, 3)?;
            writer.write_long(seq)?;
            writer.write_string(command)?;
            writer.write_bits(3, 3)?;
            let mut body = crate::netchan::ClientPayload {
                server_id: 16,
                message_ack: 0,
                reliable_ack: 0,
                body: message::compress(writer.as_bytes(), 1024)?,
            };
            body.decode(123, b""); // Symmetric command key.
            let mut bytes = seq.to_le_bytes().to_vec();
            bytes.extend_from_slice(&0xbeefu16.to_le_bytes());
            bytes.extend_from_slice(&[16, 0, 0, 0, 0, 0, 0, 0, 0]);
            bytes.extend_from_slice(&body.body);
            Ok(bytes)
        };
        let begin = b"download main/extra.iwd";
        sessions.packet(peer(), &command_packet(1, begin)?, 101, &mut game)?;
        let packets = sessions.tick(102, &mut game)?;
        let mut expected = MessageWriter::new(1024);
        expected.write_byte(5)?;
        expected.write_short(0)?;
        expected.write_long(archive.len() as i32)?;
        expected.write_short(archive.len() as i16)?;
        expected.write_data(&archive)?;
        expected.write_byte(7)?;
        let mut peer_channel = Netchan::new(peer(), 0xbeef, 123);
        let packet =
            peer_channel.transmit(1, &message::compress(expected.as_bytes(), 1024)?, begin)?;
        assert_eq!(packets[0].bytes, packet);
        sessions.packet(peer(), &command_packet(2, b"nextdl 0")?, 103, &mut game)?;
        sessions.tick(104, &mut game)?;
        sessions.packet(peer(), &command_packet(3, b"nextdl 1")?, 105, &mut game)?;
        assert!(
            !sessions.slots[2]
                .as_ref()
                .expect("connected fixture slot")
                .download
                .active()
        );
        let replies = sessions.packet(peer(), &command_packet(4, b"donedl")?, 106, &mut game)?;
        assert_eq!(replies.len(), 1);
        assert_eq!(&replies[0].bytes[4..8], &3i32.to_le_bytes());
        Ok(())
    }

    #[test]
    fn connected_heartbeat_and_first_gamestate() -> io::Result<()> {
        let mut game = TestGame::default();
        game.strings.set(5, b"example")?;
        let mut sessions = connect(&mut game)?;
        let first = sessions.tick(100, &mut game)?;
        let first = first.first().expect("heartbeat");
        assert_eq!(&first.bytes[..8], &[1, 0, 0, 0, 0, 0, 0, 0]);
        assert_eq!(message::decompress(&[first.bytes[8] ^ 122], 16)?[0], 7);
        let request = [1, 0, 0, 0, 0xef, 0xbe, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0];
        let replies = sessions.packet(peer(), &request, 120, &mut game)?;
        let game_packet = &replies.first().expect("gamestate").bytes;
        let body: Vec<_> = game_packet[8..].iter().map(|b| b ^ 121).collect();
        let mut raw = message::decompress(&body, MAX_MESSAGE)?;
        let expected = game
            .gamestate()?
            .serialize(0, 1, &[b"v x 1".to_vec()], 2, 42)?;
        raw.truncate(expected.len() - 4);
        assert_eq!(raw, expected[4..]);
        assert_eq!(
            sessions.clients().next().expect("session").1.state,
            ClientState::Primed
        );
        assert!(
            sessions
                .packet(peer(), &request, 121, &mut game)?
                .is_empty()
        );
        Ok(())
    }

    #[test]
    fn active_fast_restart_keeps_channel_and_pure_while_full_restart_reprimes() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        let c = sessions.slots[2].as_mut().expect("connected fixture slot");
        c.state = ClientState::Active;
        c.pure = Some(true);
        c.last_move.server_time = 1000;
        c.delta = Some(97);
        c.gamestate_sequence = 5;
        let channel = c.channel.outgoing_sequence();
        sessions.restart_clients(true, true, 1450, &mut game);
        assert!(sessions.tick_errors.is_empty());
        let c = sessions.slots[2].as_ref().expect("connected fixture slot");
        assert_eq!(c.state, ClientState::Active);
        assert_eq!(c.channel.outgoing_sequence(), channel);
        assert_eq!(c.pure, Some(true));
        assert_eq!(c.delta, None);
        assert_eq!(c.gamestate_sequence, 5);
        assert_eq!(c.last_move.server_time, 1000);
        assert_eq!(c.next_send, 1450);
        assert!(c.reliable.pending(0).iter().any(|text| text == b"n"));
        sessions.restart_clients(false, false, 2200, &mut game);
        let c = sessions.slots[2].as_ref().expect("connected fixture slot");
        assert_eq!(c.state, ClientState::Connected);
        assert_eq!(c.channel.outgoing_sequence(), channel);
        assert_eq!(c.pure, None);
        assert_eq!(c.gamestate_sequence, -1);
        Ok(())
    }

    #[test]
    fn exhausted_reliable_queue_closes_only_its_session() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        sessions.slots[2]
            .as_mut()
            .expect("occupied")
            .reliable
            .exhausted = true;
        let packets = sessions.tick(101, &mut game)?;
        assert!(
            packets.iter().any(
                |packet| packet.peer == peer() && packet.bytes == b"\xff\xff\xff\xffdisconnect"
            )
        );
        assert_eq!(
            sessions.slots[2].as_ref().expect("occupied").state,
            ClientState::Zombie
        );
        assert_eq!(game.disconnected, ["EXE_SERVERCOMMANDOVERFLOW"]);
        Ok(())
    }

    #[test]
    fn free_after_six_timeout_checks() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        sessions.timeout_ms = 1000;
        for frame in 0..5 {
            sessions.tick(1101 + frame, &mut game)?;
            assert!(sessions.endpoints()[2].is_some());
        }
        sessions.tick(1106, &mut game)?;
        assert!(sessions.endpoints()[2].is_none());
        assert_eq!(game.disconnected, ["EXE_TIMEDOUT"]);
        Ok(())
    }

    #[test]
    fn coalesces_unsent_commands_and_retains_acknowledged_keys() -> io::Result<()> {
        let mut ring = Reliable::default();
        for text in ["v light 2", "v name one", "v light 3", "x echo", "x echo"] {
            ring.queue(0, text.as_bytes())?;
        }
        assert_eq!(
            ring.pending(0),
            ["v name one", "v light 3", "x echo", "x echo"].map(str::as_bytes)
        );
        ring.sent = 4;
        ring.queue(4, b"v name two")?;
        assert_eq!(ring.at(1), b"v name one");
        assert_eq!(ring.pending(4), [b"v name two"]);
        Ok(())
    }

    #[test]
    fn broadcasts_whole_or_split_configstrings_to_primed_clients() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        sessions.send_updates(
            vec![ServerUpdate::ConfigString(10, b"before".to_vec())],
            true,
        );
        let client = sessions.slots[2].as_mut().expect("occupied");
        assert_eq!(client.reliable.sequence, 1);
        client.state = ClientState::Primed;
        let capacity = 1024 - (2 + 4); // decimal "10" and command framing
        let complete = vec![b'a'; capacity];
        let long = vec![b'b'; capacity * 2 + 1];
        sessions.send_updates(
            vec![
                ServerUpdate::ConfigString(10, complete.clone()),
                ServerUpdate::ConfigString(10, long.clone()),
            ],
            true,
        );
        let ring = &sessions.slots[2].as_ref().expect("occupied").reliable;
        assert_eq!(ring.sequence, 5);
        assert_eq!(ring.at(2), [b"d 10 ".as_slice(), &complete].concat());
        for (sequence, prefix) in [(3, b"x 10 "), (4, b"y 10 "), (5, b"z 10 ")] {
            assert!(ring.at(sequence).starts_with(prefix));
        }
        assert_eq!(
            [&ring.at(3)[5..], &ring.at(4)[5..], &ring.at(5)[5..]].concat(),
            long
        );
        Ok(())
    }

    #[test]
    fn remote_rate_uses_snaps_and_message_size() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        let client = sessions.slots[2].as_mut().expect("occupied");
        client.is_lan = false;
        client.identify(br"\rate\10000\snaps\30", true);
        assert_eq!(client.rate, 10000);
        client.schedule_send(500, 10, 0);
        assert_eq!(client.next_send, 1500);
        client.state = ClientState::Active;
        client.schedule_send(500, 10, 0);
        assert_eq!((client.next_send, client.rate_delayed), (533, false));
        client.schedule_send(500, 2000, 1000);
        assert_eq!((client.next_send, client.rate_delayed), (2048, true));
        Ok(())
    }

    #[test]
    fn join_prints_only_reach_active_slots() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        let print = b" \"MP_CONNECTED\x15player^7\"".to_vec();
        sessions.slots[2].as_mut().expect("occupied").state = ClientState::Primed;
        sessions.send_updates(vec![ServerUpdate::Broadcast(print.clone())], true);
        assert_eq!(
            sessions.slots[2]
                .as_ref()
                .expect("occupied")
                .reliable
                .sequence,
            1
        );
        sessions.slots[2].as_mut().expect("occupied").state = ClientState::Active;
        sessions.send_updates(vec![ServerUpdate::Broadcast(print.clone())], true);
        assert_eq!(
            sessions.slots[2].as_ref().expect("occupied").reliable.at(2),
            print
        );
        Ok(())
    }

    #[test]
    fn giveweapon_pickup_notifications_retain_duplicates() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        let client = sessions.slots[2].as_mut().expect("occupied");
        client.state = ClientState::Active;
        let pickup = br#"I "1""#.to_vec();
        client.queue([pickup.clone(), pickup.clone(), pickup.clone()])?;
        assert_eq!(
            client.reliable.pending(1),
            [pickup.clone(), pickup.clone(), pickup]
        );
        Ok(())
    }

    #[test]
    fn optional_local_sounds_require_an_active_slot_and_room() -> io::Result<()> {
        let mut game = TestGame::default();
        let mut sessions = connect(&mut game)?;
        let client = sessions.slots[2].as_mut().expect("occupied");
        client.queue([b"s 5".to_vec()])?;
        assert_eq!(client.reliable.sequence, 1);
        client.state = ClientState::Active;
        for index in 0..63 {
            client.queue([format!("s {index}").into_bytes()])?;
        }
        assert_eq!(client.reliable.sequence, 64);
        client.queue([b"d 33 value".to_vec()])?;
        assert_eq!(client.reliable.sequence, 2);
        assert_eq!(client.reliable.at(2), b"d 33 value");
        Ok(())
    }

    #[test]
    fn verifies_pure_reports_against_loaded_packs_and_feed() {
        let server = [1, 4, 7];
        assert!(valid_report(&tokenize(b"cp @ 1 7 46"), &server, 42));
        assert!(!valid_report(&tokenize(b"cp @ 1 1 40"), &server, 42));
        assert!(!valid_report(&tokenize(b"cp @ 1 6 47"), &server, 42));
        assert!(!valid_report(&tokenize(b"cp @ 1 7 42"), &server, 42));
    }
}
