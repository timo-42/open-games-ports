//! Server download commands and svc_download (protocol 118).
use crate::{
    assets::{GameAssets, stock_iwd},
    config::ServerConfig,
    message::MessageWriter,
    vm::spawn_fields::parse_int,
};
use open_games_vfs::layers::FileStream;
use std::collections::VecDeque;
use std::io::{self, Read, Seek, SeekFrom};

const BLOCK_BYTES: usize = 2048;
const WINDOW: usize = 8;
const UNEXPECTED: &str = "PC_PATCH_1_1_UNEXPECTEDDOWLOADMESSAGE";

#[derive(Debug, PartialEq, Eq)]
pub enum DownloadAction {
    Continue,
    ResendGamestate,
    Drop(&'static str),
}

#[derive(Debug)]
pub struct DownloadClient {
    pub now: i32,
    pub rate: i32,
    pub snapshot_msec: i32,
    pub www_enabled: bool,
}

#[derive(Default)]
pub struct Download {
    name: Vec<u8>,
    transfer: Option<Transfer>,
    redirected: bool,
    client_on_web: bool,
    fallback: bool,
}

impl std::fmt::Debug for Download {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Download")
            .field("name", &String::from_utf8_lossy(&self.name))
            .field("client_on_web", &self.client_on_web)
            .finish_non_exhaustive()
    }
}

struct Transfer {
    file: FileStream,
    size: i32,
    remaining: i32,
    window: VecDeque<Vec<u8>>,
    first: i32,
    next_send: i32,
    sent_at: i32,
    eof: bool,
}

impl Transfer {
    fn fill(&mut self) -> io::Result<()> {
        while self.window.len() < WINDOW && !self.eof {
            let len = BLOCK_BYTES.min(self.remaining as usize);
            let mut bytes = vec![0; len];
            self.file.read_exact(&mut bytes)?;
            self.remaining -= len as i32;
            self.eof = len == 0;
            self.window.push_back(bytes);
        }
        Ok(())
    }

    fn write(&mut self, msg: &mut MessageWriter, now: i32, count: usize) -> io::Result<()> {
        let end = self.first + self.window.len() as i32;
        for _ in 0..count {
            if self.first == end {
                break;
            }
            if self.next_send == end {
                if now.wrapping_sub(self.sent_at) < 1001 {
                    break;
                }
                self.next_send = self.first;
            }
            let block = &self.window[(self.next_send - self.first) as usize];
            msg.write_byte(5)?;
            msg.write_short(self.next_send as i16)?;
            if self.next_send == 0 {
                msg.write_long(self.size)?;
            }
            msg.write_short(block.len() as i16)?;
            msg.write_data(block)?;
            self.next_send += 1;
            self.sent_at = now;
        }
        Ok(())
    }
}

impl Download {
    pub fn active(&self) -> bool {
        !self.name.is_empty()
    }

    fn close(&mut self) {
        self.name.clear();
        self.transfer = None;
    }

    pub fn command(&mut self, command: &[u8], arg: &[u8], now: i32) -> Option<DownloadAction> {
        use DownloadAction::*;
        Some(match command {
            b"download" => {
                self.close();
                self.name.extend_from_slice(&arg[..arg.len().min(63)]);
                Continue
            }
            b"stopdl" => {
                self.close();
                Continue
            }
            b"donedl" => ResendGamestate,
            b"retransdl" => {
                if let Some(t) = &mut self.transfer
                    && parse_int(arg) == t.first
                {
                    t.next_send = t.first;
                }
                Continue
            }
            b"nextdl" => {
                let block = parse_int(arg);
                if let Some(t) = &mut self.transfer {
                    if block != t.first {
                        return Some(Drop("broken download"));
                    }
                    if t.window.pop_front().is_none_or(|bytes| bytes.is_empty()) {
                        self.close();
                    } else {
                        t.first += 1;
                        t.next_send = t.next_send.max(t.first);
                        t.sent_at = now;
                    }
                    Continue
                } else if block == 0 {
                    Continue
                } else {
                    Drop("broken download")
                }
            }
            b"wwwdl" => {
                if !self.redirected {
                    Drop(UNEXPECTED)
                } else if arg.eq_ignore_ascii_case(b"ack") {
                    self.client_on_web = true;
                    Continue
                } else if arg.eq_ignore_ascii_case(b"bbl8r") {
                    Drop("PC_PATCH_1_1_DOWNLOADDISCONNECTED")
                } else if !self.client_on_web {
                    Drop(UNEXPECTED)
                } else if arg.eq_ignore_ascii_case(b"done") {
                    self.client_on_web = false;
                    self.close();
                    Continue
                } else if arg.eq_ignore_ascii_case(b"fail") || arg.eq_ignore_ascii_case(b"chkfail")
                {
                    self.client_on_web = false;
                    self.close();
                    self.fallback = true;
                    ResendGamestate
                } else {
                    Drop(UNEXPECTED)
                }
            }
            _ => return None,
        })
    }

    fn deny(&mut self, msg: &mut MessageWriter, reason: &[u8]) -> io::Result<()> {
        msg.write_byte(5)?;
        msg.write_short(0)?;
        msg.write_long(-1)?;
        msg.write_string(&[reason, b"\x15", &self.name].concat())?;
        self.close();
        Ok(())
    }

    pub fn write(
        &mut self,
        assets: &GameAssets,
        config: &mut ServerConfig,
        msg: &mut MessageWriter,
        client: DownloadClient,
    ) -> io::Result<()> {
        if !self.active() || self.client_on_web {
            return Ok(());
        }
        if self.transfer.is_none() {
            let name = std::str::from_utf8(&self.name).unwrap_or("");
            let try_web = config.get("sv_wwwDownload") == Some("1")
                && client.www_enabled
                && !std::mem::take(&mut self.fallback);
            self.redirected = false;
            let allow = config.get("sv_allowDownload") == Some("1");
            let mut file = if allow || try_web {
                match assets.open_download(name) {
                    Ok(file) => file,
                    Err(error) => {
                        eprintln!("Cannot open mounted download '{name}': {error}");
                        None
                    }
                }
            } else {
                None
            };
            let size = if let Some(file) = &mut file {
                let len = file.seek(SeekFrom::End(0))?;
                file.rewind()?;
                i32::try_from(len).unwrap_or(0)
            } else {
                0
            };
            if try_web && size > 0 {
                let mut url = config
                    .get("sv_wwwBaseURL")
                    .unwrap_or("")
                    .as_bytes()
                    .to_vec();
                url.push(b'/');
                url.extend_from_slice(&self.name);
                url.truncate(255);
                msg.write_byte(5)?;
                msg.write_short(-1)?;
                msg.write_string(&url)?;
                msg.write_long(size)?;
                msg.write_long(i32::from(config.get("sv_wwwDlDisconnected") == Some("1")))?;
                self.redirected = true;
                self.close();
                return Ok(());
            }
            if !allow {
                return self.deny(
                    msg,
                    if config.get("sv_pure") == Some("1") {
                        b"EXE_AUTODL_SERVERDISABLED_PURE"
                    } else {
                        b"EXE_AUTODL_SERVERDISABLED"
                    },
                );
            }
            if stock_iwd(name) {
                return self.deny(msg, b"EXE_CANTAUTODLGAMEPAK");
            }
            let Some(file) = file.filter(|_| size > 0) else {
                return self.deny(msg, b"EXE_AUTODL_FILENOTONSERVER");
            };
            self.transfer = Some(Transfer {
                file,
                size,
                remaining: size,
                window: VecDeque::new(),
                first: 0,
                next_send: 0,
                sent_at: 0,
                eof: false,
            });
        }
        let limit = config
            .get("sv_maxRate")
            .and_then(|s| s.parse::<i32>().ok())
            .unwrap_or(0);
        let rate = if limit == 0 {
            client.rate
        } else {
            if limit < 1000 {
                config.set_from_script("sv_maxRate", "1000");
            }
            client.rate.min(limit.max(1000))
        };
        let count = 1
            + (i64::from(rate.max(0)) * i64::from(client.snapshot_msec.max(0)) / 1000) as usize
                / BLOCK_BYTES;
        let transfer = self.transfer.as_mut().expect("opened download");
        if let Err(error) = transfer.fill() {
            eprintln!(
                "Cannot read mounted download '{}': {error}",
                String::from_utf8_lossy(&self.name)
            );
            return self.deny(msg, b"EXE_AUTODL_FILENOTONSERVER");
        }
        transfer.write(msg, client.now, count)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::{Cursor, Write};

    const PACK: &[u8] = b"mods/test/extra.iwd";

    fn archive(byte: u8, len: usize) -> io::Result<Vec<u8>> {
        let mut zip = zip::ZipWriter::new(Cursor::new(Vec::new()));
        zip.start_file(
            "dl/payload",
            zip::write::SimpleFileOptions::default()
                .compression_method(zip::CompressionMethod::Stored),
        )?;
        zip.write_all(&vec![byte; len])?;
        Ok(zip.finish()?.into_inner())
    }

    fn fixture() -> io::Result<(tempfile::TempDir, GameAssets, ServerConfig, Vec<u8>)> {
        let tmp = tempfile::tempdir()?;
        let base = tmp.path().join("base");
        let home = tmp.path().join("home");
        for root in [&base, &home] {
            fs::create_dir_all(root.join("mods/test"))?;
        }
        fs::create_dir_all(base.join("main"))?;
        let file = archive(b'B', 20880)?;
        fs::write(base.join("mods/test/extra.iwd"), archive(b'A', 500)?)?;
        fs::write(base.join("main/extra.iwd"), archive(b'M', 410)?)?;
        fs::write(
            base.join("main/localized_english_iw00.iwd"),
            archive(7, 12)?,
        )?;
        fs::write(home.join("mods/test/extra.iwd"), &file)?;
        fs::write(home.join("mods/test/private_svr_pack.iwd"), archive(1, 12)?)?;
        let assets = GameAssets::mount_paths(&base, Some(&home), "mods/test")?;
        let mut config = ServerConfig::from_args(&[]);
        config.register_game();
        Ok((tmp, assets, config, file))
    }

    fn send(
        download: &mut Download,
        assets: &GameAssets,
        config: &mut ServerConfig,
        now: i32,
        rate: i32,
        www_enabled: bool,
    ) -> io::Result<Vec<u8>> {
        let mut msg = MessageWriter::new(64 * 1024);
        download.write(
            assets,
            config,
            &mut msg,
            DownloadClient {
                now,
                rate,
                www_enabled,
                snapshot_msec: 50,
            },
        )?;
        Ok(msg.into_bytes())
    }

    #[test]
    fn stream_window_retransmits_then_reconstructs_mounted_home_pack() -> io::Result<()> {
        let (_tmp, assets, mut config, source) = fixture()?;
        let mut d = Download::default();
        d.command(b"download", PACK, 0);
        let first = send(&mut d, &assets, &mut config, 0, 1000, false)?;
        assert_eq!(&first[..3], &[5, 0, 0]);
        assert_eq!(
            i32::from_le_bytes(
                first[3..7]
                    .try_into()
                    .expect("fixed-width download block field")
            ),
            source.len() as i32
        );
        assert_eq!(first[7..9], 2048i16.to_le_bytes());
        assert_eq!(first[9..], source[..2048]);
        d.command(b"retransdl", b"0", 5);
        assert_eq!(send(&mut d, &assets, &mut config, 10, 1000, false)?, first);
        // Fill and send the remaining seven blocks in the first window without acknowledging.
        for index in 1..8 {
            assert_eq!(
                send(&mut d, &assets, &mut config, 20, 1000, false)?[1],
                index
            );
        }
        assert!(send(&mut d, &assets, &mut config, 1020, 1000, false)?.is_empty());
        assert_eq!(
            send(&mut d, &assets, &mut config, 1021, 1000, false)?,
            first
        );
        let mut received = first[9..].to_vec();
        assert_eq!(
            d.command(b"nextdl", b"0", 1022),
            Some(DownloadAction::Continue)
        );
        for id in 1.. {
            let bytes = send(&mut d, &assets, &mut config, 1023, 1000, false)?;
            assert_eq!(bytes[0], 5);
            assert_eq!(
                i16::from_le_bytes(
                    bytes[1..3]
                        .try_into()
                        .expect("fixed-width download block field")
                ),
                id
            );
            let len = i16::from_le_bytes(
                bytes[3..5]
                    .try_into()
                    .expect("fixed-width download block field"),
            ) as usize;
            assert_eq!(len, bytes.len() - 5);
            received.extend_from_slice(&bytes[5..]);
            d.command(b"nextdl", id.to_string().as_bytes(), 1023);
            if len == 0 {
                break;
            }
        }
        assert_eq!(received, source);
        assert!(!d.active());
        assert_eq!(
            d.command(b"donedl", b"", 1023),
            Some(DownloadAction::ResendGamestate)
        );
        Ok(())
    }

    #[test]
    fn redirect_precedes_plain_refusal_and_fallback_is_one_request() -> io::Result<()> {
        let (_tmp, assets, mut config, source) = fixture()?;
        config.set_from_script("sv_wwwDownload", "1");
        config.set_from_script("sv_wwwDlDisconnected", "1");
        config.set_from_script("sv_wwwBaseURL", "https://dl.example.invalid/cod2");
        config.set_from_script("sv_allowDownload", "0");
        let mut d = Download::default();
        d.command(b"download", PACK, 0);
        let redirect = send(&mut d, &assets, &mut config, 0, 25000, true)?;
        let mut expected = vec![5, 255, 255];
        expected.extend_from_slice(b"https://dl.example.invalid/cod2/mods/test/extra.iwd\0");
        expected.extend_from_slice(&(source.len() as i32).to_le_bytes());
        expected.extend_from_slice(&1i32.to_le_bytes());
        assert_eq!(redirect, expected);
        assert_eq!(
            d.command(b"wwwdl", b"ack", 0),
            Some(DownloadAction::Continue)
        );
        assert_eq!(
            d.command(b"wwwdl", b"chkfail", 0),
            Some(DownloadAction::ResendGamestate)
        );
        d.command(b"download", PACK, 0);
        let refused = send(&mut d, &assets, &mut config, 0, 25000, true)?;
        assert_eq!(&refused[0..7], &[5, 0, 0, 255, 255, 255, 255]);
        assert_eq!(
            &refused[7..],
            b"EXE_AUTODL_SERVERDISABLED_PURE\x15mods/test/extra.iwd\0"
        );
        d.command(b"download", PACK, 0);
        assert_eq!(
            send(&mut d, &assets, &mut config, 0, 25000, true)?,
            expected
        );
        Ok(())
    }

    #[test]
    fn mounted_unreferenced_packs_are_readable_and_private_files_are_denied() -> io::Result<()> {
        let (_tmp, assets, mut config, _) = fixture()?;
        assert!(
            !assets.iwd_info(true)[3]
                .1
                .split_whitespace()
                .any(|n| n == "main/extra")
        );
        let mut d = Download::default();
        d.command(b"download", b"main/extra.iwd", 0);
        let bytes = send(&mut d, &assets, &mut config, 0, 25000, false)?;
        assert_eq!(&bytes[9..], archive(b'M', 410)?);
        assert!(
            assets
                .open_download("mods/test/private_svr_pack.iwd")?
                .is_none()
        );
        assert!(
            assets
                .open_download("mods/test/config_mp_server.cfg")?
                .is_none()
        );
        let mut d = Download::default();
        d.command(b"download", b"main/iw_00.iwd", 0);
        let denied = send(&mut d, &assets, &mut config, 0, 25000, false)?;
        assert_eq!(&denied[7..], b"EXE_CANTAUTODLGAMEPAK\x15main/iw_00.iwd\0");
        d.command(b"download", b"mods/test/unknown.iwd", 0);
        let denied = send(&mut d, &assets, &mut config, 0, 25000, false)?;
        assert_eq!(
            &denied[7..],
            b"EXE_AUTODL_FILENOTONSERVER\x15mods/test/unknown.iwd\0"
        );
        assert_eq!(
            d.command(b"nextdl", b"8", 0),
            Some(DownloadAction::Drop("broken download"))
        );
        Ok(())
    }
}
