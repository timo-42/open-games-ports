use super::{WritableGame, clean};
use std::{fs, io};

#[derive(Debug, Default)]
pub struct BanList {
    temp: [(i32, i32); 16],
    files: WritableGame,
    source: Vec<u8>,
}

impl BanList {
    pub(super) fn new(files: WritableGame, source: Vec<u8>) -> Self {
        Self {
            files,
            source,
            ..Self::default()
        }
    }
    fn contents(&self) -> io::Result<Vec<u8>> {
        if let Some(path) = self.files.path("ban.txt")? {
            match fs::read(path) {
                Ok(bytes) => return Ok(bytes),
                Err(e) if e.kind() == io::ErrorKind::NotFound => {}
                Err(e) => return Err(e),
            }
        }
        Ok(self.source.clone())
    }
    fn has(bytes: &[u8], guid: i32) -> bool {
        bytes.split(|b| *b == b'\n').any(|line| {
            cod2_server::query::tokenize(line)
                .first()
                .is_some_and(|word| {
                    cod2_server::config::integer_prefix(&String::from_utf8_lossy(word)) == guid
                })
        })
    }
    pub fn reject(&self, guid: i32, now: i32, seconds: f32) -> io::Result<Option<&'static str>> {
        if guid == 0 {
            return Ok(None);
        }
        if Self::has(&self.contents()?, guid) {
            return Ok(Some(
                "error\n\x15You are permanently banned from this server",
            ));
        }
        let temporary = self.temp.iter().any(|&(id, time)| {
            id == guid && f64::from(now.wrapping_sub(time) as f32) <= f64::from(seconds) * 1000.
        });
        Ok(temporary.then_some("error\n\x15You are temporarily banned from this server"))
    }
    pub(super) fn temp_ban(&mut self, guid: i32, now: i32) {
        let slot = self
            .temp
            .iter()
            .position(|&(id, _)| id == 0)
            .unwrap_or_else(|| {
                self.temp
                    .iter()
                    .enumerate()
                    .min_by_key(|(_, (_, time))| time)
                    .expect("nonempty ban table")
                    .0
            });
        self.temp[slot] = (guid, now);
    }
    pub(super) fn ban(&self, guid: i32, name: &[u8]) -> io::Result<bool> {
        let mut bytes = self.contents()?;
        if Self::has(&bytes, guid) {
            return Ok(false);
        }
        bytes.extend(guid.to_string().bytes());
        bytes.push(b' ');
        bytes.extend(clean(name));
        bytes.extend(b"\r\n");
        self.files.replace("ban.txt", &bytes)?;
        Ok(true)
    }
    pub(super) fn unban(&self, name: &[u8]) -> io::Result<Vec<u8>> {
        let bytes = self.contents()?;
        if bytes.is_empty() && !self.files.path("ban.txt")?.is_some_and(|p| p.exists()) {
            return Ok(Vec::new());
        }
        let name = clean(&name[..name.len().min(63)]);
        let mut kept = Vec::new();
        let mut removed = 0;
        for line in bytes.split_inclusive(|&b| b == b'\n') {
            let text = line.trim_ascii_start();
            let end = text
                .iter()
                .position(u8::is_ascii_whitespace)
                .unwrap_or(text.len());
            let rest = text[end..].trim_ascii_start();
            let end = rest.iter().position(|b| matches!(b, b'\r' | b'\n'));
            if end.is_some_and(|end| rest[..end].eq_ignore_ascii_case(&name)) {
                removed += 1;
            } else {
                kept.extend_from_slice(line);
            }
        }
        self.files.replace("ban.txt", &kept)?;
        let name = String::from_utf8_lossy(&name);
        Ok(if removed == 0 {
            format!("no banned user has name {name}\n")
        } else {
            format!("unbanned {removed} user(s) named {name}\n")
        }
        .into_bytes())
    }
}
