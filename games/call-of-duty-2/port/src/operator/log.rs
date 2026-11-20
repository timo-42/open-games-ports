use super::WritableGame;
use cod2_server::config::ServerConfig;
use std::{
    fs::{File, OpenOptions},
    io::{self, BufWriter, Write},
};

const SEPARATOR: &[u8] = b"------------------------------------------------------------\n";

#[derive(Debug, Default)]
pub(crate) struct Logs {
    files: WritableGame,
    console: Option<BufWriter<File>>,
    game: Option<BufWriter<File>>,
    sync_game: bool,
}

impl Logs {
    pub(super) fn new(files: WritableGame) -> Self {
        Self {
            files,
            ..Self::default()
        }
    }
    pub fn open_game(&mut self, config: &ServerConfig, info: &[u8], time: i32) -> io::Result<()> {
        let name = config.get("g_log").unwrap_or("games_mp.log");
        if name.is_empty() {
            return Ok(());
        }
        let Some(path) = self.files.path(name)? else {
            return Ok(());
        };
        self.game = Some(BufWriter::new(
            OpenOptions::new().create(true).append(true).open(path)?,
        ));
        self.sync_game = config.get("g_logsync") == Some("1");
        self.game_line(time, SEPARATOR)?;
        self.game_line(time, &[b"InitGame: ", info, b"\n"].concat())
    }
    pub fn game_line(&mut self, time: i32, message: &[u8]) -> io::Result<()> {
        let Some(writer) = &mut self.game else {
            return Ok(());
        };
        let seconds = time / 1000;
        let prefix = format!("{:3}:{}{} ", seconds / 60, seconds % 60 / 10, seconds % 10);
        let message = message.split(|&b| b == 0).next().unwrap_or_default();
        writer.write_all(prefix.as_bytes())?;
        writer.write_all(&message[..message.len().min(1023 - prefix.len())])?;
        if self.sync_game {
            writer.flush()?;
        }
        Ok(())
    }
    pub fn record(&mut self, time: i32, message: &[u8]) {
        if let Err(error) = self.game_line(time, message) {
            self.game = None;
            eprintln!("game log: {error}");
        }
    }

    pub fn close_game(&mut self, time: i32) -> io::Result<()> {
        self.game_line(time, b"ShutdownGame:\n")?;
        self.game_line(time, SEPARATOR)?;
        if let Some(mut file) = self.game.take() {
            file.flush()?;
        }
        Ok(())
    }
    pub fn console(&mut self, config: &ServerConfig, message: &[u8]) -> io::Result<()> {
        let mode = config.get("logfile").unwrap_or("0");
        if mode == "0" {
            return Ok(());
        }
        if self.console.is_none() {
            let Some(path) = self.files.path("console_mp_server.log")? else {
                return Ok(());
            };
            let mut log = BufWriter::new(File::create(path)?);
            let time = chrono::Local::now();
            writeln!(log, "logfile opened on {}\n", time.format("%a %b %e %T %Y"))?;
            self.console = Some(log);
        }
        let log = self.console.as_mut().expect("console logfile open");
        log.write_all(message)?;
        if mode == "2" {
            log.flush()?;
        }
        Ok(())
    }
}
