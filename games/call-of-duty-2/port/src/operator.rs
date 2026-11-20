//! Dedicated operator commands registered by SV_AddOperatorCommands.
mod bans;
mod log;

use self::{bans::BanList, log::Logs};
use super::{LiveGame, Server};
use cod2_server::assets::WritableGame;
use cod2_server::{assets::GameAssets, client::ClientState, config::ServerConfig, rcon};
use std::io;

#[derive(Debug)]
pub(super) struct OperatorState {
    pub bans: BanList,
    pub logs: Logs,
}

impl OperatorState {
    pub fn print_console(&mut self, config: &ServerConfig, message: &[u8]) {
        use io::Write;
        if message.is_empty() {
            return;
        }
        let message = if message.starts_with(b"^") && message.len() >= 2 {
            &message[2..]
        } else {
            message
        };
        let _ = std::io::stdout().lock().write_all(message);
        if let Err(error) = self.logs.console(config, message) {
            eprintln!("console log: {error}");
        }
    }

    pub fn new(assets: &GameAssets) -> io::Result<Self> {
        let files = assets.writable().clone();
        let source = match assets.read_bans() {
            Ok(bytes) => bytes,
            Err(e) if e.kind() == io::ErrorKind::NotFound => Vec::new(),
            Err(e) => return Err(e),
        };
        Ok(Self {
            bans: BanList::new(files.clone(), source),
            logs: Logs::new(files),
        })
    }
}

// Q_CleanStr removes ^0..^9, signed non-ASCII bytes and control bytes.
fn clean(name: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    let mut chars = name.iter().copied().take_while(|b| *b != 0).peekable();
    while let Some(ch) = chars.next() {
        if ch == b'^' && chars.peek().is_some_and(u8::is_ascii_digit) {
            chars.next();
        } else if (32..127).contains(&ch) {
            out.push(ch);
        }
    }
    out
}

fn chat_text(args: &[String]) -> Vec<u8> {
    let joined = args.join(" ");
    let mut text = joined.as_bytes();
    if text.first() == Some(&b'"') {
        text = text
            .get(1..text.len().saturating_sub(1))
            .unwrap_or_default();
    }
    let mut command = b"h \"\x15console: ".to_vec();
    command.extend_from_slice(&text[..text.len().min(1023 - "console: ".len())]);
    command.push(b'"');
    command
}

impl Server {
    fn admin_slot(&self, arg: &str, number: bool) -> Result<usize, String> {
        if !number {
            return self
                .sessions
                .clients()
                .find(|(_, client)| {
                    client.name.eq_ignore_ascii_case(arg.as_bytes())
                        || clean(&client.name).eq_ignore_ascii_case(arg.as_bytes())
                })
                .map(|(slot, _)| slot)
                .ok_or_else(|| format!("Player {arg} is not on the server\n"));
        }
        if !arg.bytes().all(|c| c.is_ascii_digit()) {
            return Err(format!("Bad slot number: {arg}\n"));
        }
        let slot = if arg.is_empty() {
            0
        } else {
            arg.parse::<usize>().unwrap_or(usize::MAX)
        };
        if slot >= self.sessions.capacity() {
            return Err(format!("Bad client slot: {slot}\n"));
        }
        self.sessions
            .clients()
            .find(|(idx, _)| *idx == slot)
            .map(|(slot, _)| slot)
            .ok_or_else(|| format!("Client {slot} is not active\n"))
    }

    fn kick_slot(&mut self, slot: usize, now: i32) -> io::Result<Vec<u8>> {
        let Some(level) = self.map.as_mut() else {
            return Ok(Vec::new());
        };
        let mut game = LiveGame {
            level,
            config: &mut self.config,
            assets: &self.assets,
            map_id: self.map_id,
        };
        // Com_BeginRedirect sends only console lines from this command to the RCON caller.
        let previous = self.sessions.console_lines.len();
        self.sessions.kick_client(slot, now, &mut game)?;
        Ok(self.sessions.console_lines.split_off(previous))
    }

    fn moderate(&mut self, words: &[String], command: &str, now: i32) -> io::Result<Vec<u8>> {
        let by_number = matches!(command, "banclient" | "tempbanclient" | "clientkick");
        let permanent = matches!(command, "banuser" | "banclient");
        if words.len() != 2 {
            let text = match command {
                "banuser" => "Usage: banUser <player name>\n".into(),
                "banclient" => "Usage: banClient <client number>\n".into(),
                _ if by_number => format!("Usage: {} <client number>\n", words[0]),
                _ => format!(
                    "Usage: {} <player name>\n{} all = kick everyone\n",
                    words[0], words[0]
                ),
            };
            return Ok(text.into_bytes());
        }
        let slot = match self.admin_slot(&words[1], by_number) {
            Ok(slot) => slot,
            Err(error) => {
                let mut output = error.into_bytes();
                if !permanent && !by_number && words[1].eq_ignore_ascii_case("all") {
                    let slots = self
                        .sessions
                        .clients()
                        .map(|(slot, _)| slot)
                        .collect::<Vec<_>>();
                    for slot in slots {
                        output.extend(self.kick_slot(slot, now)?);
                    }
                }
                return Ok(output);
            }
        };
        let (_, client) = self
            .sessions
            .clients()
            .find(|(id, _)| *id == slot)
            .expect("resolved client slot");
        let guid = client.guid;
        let name = clean(&client.name);
        if permanent {
            if guid == 0 {
                return Ok(b"Can't ban user, GUID is 0\n".to_vec());
            }
            if !self.operator_state.bans.ban(guid, &name)? {
                return Ok(format!("This GUID ({guid}) is already banned\n").into_bytes());
            }
        }
        let mut output = self.kick_slot(slot, now)?;
        if guid != 0 && matches!(command, "tempbanclient" | "tempbanuser" | "kick") {
            self.operator_state.bans.temp_ban(guid, now);
            output.extend(
                format!(
                    "{} (guid {guid}) was kicked for cheating\n",
                    String::from_utf8_lossy(&name)
                )
                .into_bytes(),
            );
        }
        Ok(output)
    }

    pub fn admin(&mut self, words: &[String]) -> Option<io::Result<Vec<u8>>> {
        let command = words.first()?.to_ascii_lowercase();
        let command = command.as_str();
        if matches!(command, "scriptusage" | "stringusage") {
            return Some(Ok(if let Some(level) = &self.map {
                if command == "scriptusage" {
                    level.vm.script_usage()
                } else {
                    level.vm.string_usage(&level.world)
                }
            } else {
                "Server is not running.\n".into()
            }
            .into_bytes()));
        }
        if command == "killserver" {
            return Some(Ok(self.stop_map("EXE_SERVERKILLED")));
        }
        if command == "systeminfo" {
            let iwds = self
                .assets
                .iwd_info(self.config.get("sv_pure") != Some("0"));
            return Some(Ok(rcon::print_info(
                b"System info settings:\n",
                &self.browser.system_info(self.map_id, &iwds),
            )));
        }
        if command == "unbanuser" {
            return Some(if words.len() == 2 {
                self.operator_state.bans.unban(words[1].as_bytes())
            } else {
                Ok(b"Usage: unban <client name>\n".to_vec())
            });
        }
        if !matches!(
            command,
            "kick"
                | "onlykick"
                | "clientkick"
                | "tempbanuser"
                | "tempbanclient"
                | "banuser"
                | "banclient"
                | "dumpuser"
                | "say"
                | "tell"
        ) {
            return None;
        }
        let Some(now) = self.map.as_ref().map(|l| l.time_ms) else {
            return Some(Ok(b"Server is not running.\n".to_vec()));
        };
        if command == "dumpuser" {
            return Some(Ok(if words.len() != 2 {
                b"Usage: info <userid>\n".to_vec()
            } else {
                match self.admin_slot(&words[1], false) {
                    Ok(slot) => rcon::print_info(
                        b"userinfo\n--------\n",
                        self.sessions
                            .clients()
                            .find(|(s, _)| *s == slot)
                            .expect("resolved client slot")
                            .1
                            .userinfo(),
                    ),
                    Err(err) => err.into_bytes(),
                }
            }));
        }
        if command == "say" || command == "tell" {
            if self.config.get("dedicated") == Some("0") {
                return None;
            }
            let tell = command == "tell";
            let skip = if tell { 2 } else { 1 };
            if words.len() > skip {
                let slot = words
                    .get(1)
                    .map_or(0, |w| cod2_server::config::integer_prefix(w));
                let targets = self
                    .sessions
                    .clients()
                    .filter(|(id, c)| {
                        if tell {
                            *id as i32 == slot && c.state == ClientState::Active
                        } else {
                            matches!(c.state, ClientState::Primed | ClientState::Active)
                        }
                    })
                    .map(|(id, _)| id)
                    .collect::<Vec<_>>();
                let text = chat_text(&words[skip..]);
                for id in targets {
                    if let Err(e) = self.sessions.queue_notice(id, &text) {
                        return Some(Err(e));
                    }
                }
            }
            return Some(Ok(vec![]));
        }
        Some(self.moderate(words, command, now))
    }

    pub fn stop_map(&mut self, reason: &str) -> Vec<u8> {
        let mut output = Vec::new();
        if let Some(level) = self.map.as_mut() {
            output.extend_from_slice(b"----- Server Shutdown -----\n");
            let now = level.time_ms;
            let mut game = LiveGame {
                level,
                config: &mut self.config,
                assets: &self.assets,
                map_id: self.map_id,
            };
            self.outbound
                .extend(self.sessions.final_snapshots(now, reason, &mut game));
            self.master.poll(
                now,
                self.config.get("dedicated") == Some("2"),
                true,
                &self.browser,
                &mut self.outbound,
            );
            output.extend_from_slice(self.master.take_console().as_bytes());
            output.extend_from_slice(b"==== ShutdownGame ====\n");
            if let Err(error) = self.close_game_log() {
                eprintln!("game log: {error}");
            }
            self.map = None;
            // SV_ClearServerState shuts the game down before
            // SV_DisconnectAllClients, so there is no ClientDisconnect callback.
            for (slot, client) in self.sessions.clients() {
                if client.state != cod2_server::client::ClientState::Zombie {
                    output.extend_from_slice(format!("{slot}:").as_bytes());
                    output.extend_from_slice(&client.name);
                    output.extend_from_slice(b" EXE_DISCONNECTED\n");
                }
            }
            self.sessions = cod2_server::client::ClientSessions::new(0);
            self.config.map = None;
            self.config.values.insert("sv_running".into(), "0".into());
            self.challenges = Default::default();
            output.extend_from_slice(b"---------------------------\n");
        }
        output
    }

    pub fn print_console(&mut self, message: &[u8]) {
        self.operator_state.print_console(&self.config, message);
    }

    pub fn flush_game_log(&mut self) {
        if let Some(level) = &mut self.map {
            for (line_time, line) in level.world.players.game_log.drain(..) {
                self.operator_state.logs.record(line_time, &line);
            }
        }
    }

    pub fn close_game_log(&mut self) -> io::Result<()> {
        self.flush_game_log();
        if let Some(level) = self.map.as_ref() {
            self.operator_state.logs.close_game(level.time_ms)?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::TempDir;

    fn paths() -> io::Result<(TempDir, WritableGame, ServerConfig)> {
        let temp = TempDir::new()?;
        let base = temp.path().join("base");
        let home = temp.path().join("home");
        fs::create_dir(&base)?;
        let config = ServerConfig::from_args(&[
            "+set".into(),
            "fs_basepath".into(),
            base.to_string_lossy().into_owned(),
            "+set".into(),
            "fs_homepath".into(),
            home.to_string_lossy().into_owned(),
            "+set".into(),
            "g_logSync".into(),
            "1".into(),
            "+set".into(),
            "logfile".into(),
            "2".into(),
        ]);
        let writable = WritableGame::from_paths(&base, Some(&home), "main")?;
        Ok((temp, writable, config))
    }

    #[test]
    fn bans_use_guid_and_remove_all_exact_clean_names() -> io::Result<()> {
        let (temp, files, _) = paths()?;
        let mut bans = BanList::new(files, b"85 Alice\r\n823 Alice Smith\r\n".to_vec());
        assert!(bans.reject(85, 20, 300.)?.is_some());
        assert!(!bans.ban(85, b"Bob")?);
        assert!(bans.ban(404, b"^3Alice")?);
        assert_eq!(
            fs::read(temp.path().join("home/main/ban.txt"))?,
            b"85 Alice\r\n823 Alice Smith\r\n404 Alice\r\n"
        );
        bans.temp_ban(22, 200);
        assert!(bans.reject(22, 1200, 1.)?.is_some());
        assert!(bans.reject(22, 1201, 1.)?.is_none());
        for id in 0..16 {
            bans.temp_ban(id + 300, id + 201);
        }
        assert!(bans.reject(22, 1220, 300.)?.is_none());
        assert_eq!(bans.unban(b"^2aLiCe")?, b"unbanned 2 user(s) named aLiCe\n");
        assert_eq!(
            fs::read(temp.path().join("home/main/ban.txt"))?,
            b"823 Alice Smith\r\n"
        );
        assert!(bans.reject(0, 30, 3600.)?.is_none());
        assert!(bans.reject(823, 30, 3600.)?.is_some());
        Ok(())
    }

    #[test]
    fn logs_use_game_time_and_original_message_bounds() -> io::Result<()> {
        let (temp, files, config) = paths()?;
        let mut logs = Logs::new(files);
        logs.open_game(&config, br"\mapname\mp_test", 62_000)?;
        logs.game_line(125_000, &vec![b'x'; 2000])?;
        logs.game_line(126_000, b"\n")?;
        logs.close_game(130_000)?;
        let line = fs::read(temp.path().join("home/main/games_mp.log"))?;
        assert!(line.starts_with(b"  1:02 ------------------------------------------------------------\n  1:02 InitGame: \\mapname\\mp_test\n"));
        assert!(line.windows(14).any(|s| s == b"  2:05 xxxxxxx"));
        assert_eq!(line.iter().filter(|&&v| v == b'x').count(), 1016);
        assert!(line.ends_with(b"  2:10 ShutdownGame:\n  2:10 ------------------------------------------------------------\n"));
        logs.console(&config, b"test console\n")?;
        let console = fs::read(temp.path().join("home/main/console_mp_server.log"))?;
        assert!(console.starts_with(b"logfile opened on "));
        assert!(console.ends_with(b"\n\ntest console\n"));
        Ok(())
    }

    #[test]
    fn chat_and_cleaning_preserve_protocol_markers() {
        assert_eq!(clean(b"^0Black^a ^^8\x01\x81\x7f ok"), b"Black^a ^ ok");
        assert_eq!(
            chat_text(&["two".into(), "words".into()]),
            b"h \"\x15console: two words\""
        );
    }
}
