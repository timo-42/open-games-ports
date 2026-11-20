//! CoD2 1.3 master registration and client IP authorization.
use crate::{admission::info_integer, challenge::Challenges, client::Datagram, query::QueryState};
use std::io;
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4, ToSocketAddrs};

const MASTER_NAME: &str = "cod2master.activision.com";
const MASTER_PORT: u16 = 20710;
const AUTHORIZE_PORT: u16 = 20700;

#[derive(Clone, Copy, Debug)]
pub enum Announcement {
    Heartbeat,
    Status,
    GameComplete,
    Shutdown,
}

#[derive(Debug)]
struct Endpoint {
    port: u16,
    address: Option<Ipv4Addr>,
    lookup_failed: bool,
}

impl Endpoint {
    fn new(port: u16) -> Self {
        Self {
            port,
            address: None,
            lookup_failed: false,
        }
    }

    fn resolve(&mut self, log: &mut String) -> Option<Ipv4Addr> {
        use std::fmt::Write as _;
        if self.lookup_failed || self.address.is_some() {
            return self.address;
        }
        let _ = writeln!(log, "Resolving {MASTER_NAME}");
        self.address = (MASTER_NAME, self.port)
            .to_socket_addrs()
            .ok()
            .and_then(|mut addresses| {
                addresses.find_map(|socket| match socket {
                    SocketAddr::V4(addr) if *addr.ip() != Ipv4Addr::BROADCAST => Some(*addr.ip()),
                    _ => None,
                })
            });
        if let Some(ip) = self.address {
            let _ = writeln!(log, "{MASTER_NAME} resolved to {ip}:{}", self.port);
        } else {
            self.lookup_failed = true;
            if self.port == AUTHORIZE_PORT {
                log.push_str("Couldn't resolve address\n");
            } else {
                let _ = writeln!(log, "Couldn't resolve address: {MASTER_NAME}");
            }
        }
        self.address
    }
}

#[derive(Debug)]
pub struct Master {
    external: bool,
    listing: Endpoint,
    authority: Endpoint,
    console: String,
    next_heartbeat: i32,
    next_status: i32,
    last_contact: i32,
    clients: usize,
    status_challenge: Vec<u8>,
}

impl Master {
    pub fn new(bound: Ipv4Addr) -> Self {
        Self {
            external: !bound.is_loopback(),
            listing: Endpoint::new(MASTER_PORT),
            authority: Endpoint::new(AUTHORIZE_PORT),
            console: String::new(),
            next_heartbeat: 0,
            next_status: 0,
            last_contact: 0,
            clients: 0,
            status_challenge: Vec::new(),
        }
    }

    pub fn take_console(&mut self) -> String {
        std::mem::take(&mut self.console)
    }

    /// The first unsolicited status uses argv[1] of the map command.
    pub fn map_loaded(&mut self, name: &str) {
        self.status_challenge = name.as_bytes().to_vec();
        self.force_heartbeat();
    }

    pub fn force_heartbeat(&mut self) {
        self.next_heartbeat = i32::MIN;
    }

    pub fn population(&mut self, count: usize, capacity: usize) {
        if count != self.clients
            && (count == 0 || count > self.clients && (count == 1 || count == capacity))
        {
            self.force_heartbeat();
        }
        self.clients = count;
    }

    /// Called before constructing roster data, as status is due only every ten minutes.
    pub fn status_due(&self, time: i32, internet: bool) -> bool {
        self.external && internet && time >= self.next_status
    }

    pub fn poll(
        &mut self,
        time: i32,
        internet: bool,
        shutdown: bool,
        browser: &QueryState,
        out: &mut Vec<Datagram>,
    ) {
        if !self.external || !internet {
            return;
        }
        if shutdown {
            self.force_heartbeat();
        }
        if time >= self.next_heartbeat {
            self.next_heartbeat = time.wrapping_add(180_000);
            let kind = if shutdown {
                Announcement::Shutdown
            } else {
                Announcement::Heartbeat
            };
            out.extend(self.announcement(kind, browser));
        }
        if time >= self.next_status {
            self.next_status = time.wrapping_add(600_000);
            out.extend(self.announcement(Announcement::Status, browser));
        }
    }

    pub fn announcement(&mut self, kind: Announcement, browser: &QueryState) -> Option<Datagram> {
        if !self.external {
            return None;
        }
        let ip = self.listing.resolve(&mut self.console)?;
        match kind {
            Announcement::Heartbeat | Announcement::Shutdown => {
                self.console
                    .push_str("Sending heartbeat to cod2master.activision.com\n");
            }
            Announcement::GameComplete => {
                self.console
                    .push_str("Sending gameCompleteStatus to cod2master.activision.com\n");
            }
            Announcement::Status => {}
        }
        let bytes = match kind {
            Announcement::Heartbeat => b"\xff\xff\xff\xffheartbeat COD-2\n".to_vec(),
            Announcement::Shutdown => b"\xff\xff\xff\xffheartbeat flatline\n".to_vec(),
            Announcement::Status => {
                browser.master_status(false, &std::mem::take(&mut self.status_challenge))
            }
            Announcement::GameComplete => browser.master_status(true, b""),
        };
        Some(Datagram {
            peer: SocketAddrV4::new(ip, self.listing.port),
            bytes,
        })
    }

    pub fn note_contact(&mut self, peer: SocketAddrV4, time: i32) {
        if self.external && self.listing.resolve(&mut self.console).as_ref() == Some(peer.ip()) {
            self.last_contact = time;
        }
    }

    pub fn request(
        &mut self,
        peer: SocketAddrV4,
        now: i32,
        challenges: &mut Challenges,
        fs_game: &str,
        allow_anonymous: bool,
        pb_name: Option<&[u8]>,
    ) -> io::Result<Option<Datagram>> {
        if !self.external {
            return Ok(None);
        }
        let authority = self.authority.resolve(&mut self.console);
        let challenge = challenges.request_remote(peer, now)?;
        // SV_RequestIpAuthorize permits independent play after sustained master unavailability.
        if now.wrapping_sub(self.last_contact) > 1_200_000
            && now.wrapping_sub(challenge.issued_at) > 7_000
            && self
                .listing
                .resolve(&mut self.console)
                .is_none_or(|ip| peer != SocketAddrV4::new(ip, self.listing.port))
        {
            challenge.authorized = true;
            return Ok(Some(Datagram {
                peer,
                bytes: challenge.reply(now),
            }));
        }
        let Some(ip) = authority else {
            return Ok(None);
        };
        challenge.authorized = false;
        let mut bytes = vec![0xff; 4];
        bytes.extend(
            format!(
                "getIpAuthorize {} {} \"{}\" {}",
                challenge.value,
                peer.ip(),
                fs_game,
                u8::from(allow_anonymous)
            )
            .bytes(),
        );
        if let Some(name) = pb_name {
            bytes.extend_from_slice(b" PB \"");
            bytes.extend_from_slice(name);
            bytes.push(b'"');
        }
        Ok(Some(Datagram {
            peer: SocketAddrV4::new(ip, self.authority.port),
            bytes,
        }))
    }

    pub fn authorize(
        &mut self,
        from: SocketAddrV4,
        now: i32,
        args: &[Vec<u8>],
        challenges: &mut Challenges,
        demo_server: bool,
        banned: impl FnOnce(i32) -> io::Result<Option<&'static str>>,
    ) -> io::Result<Option<Datagram>> {
        // NET_CompareBaseAdr checks the resolved IPv4 address; replies can originate
        // from a different source port than the request destination.
        self.note_contact(from, now);
        if self.authority.address.as_ref() != Some(from.ip()) {
            return Ok(None);
        }
        let arg = |i| args.get(i).map_or(&[][..], Vec::as_slice);
        let Some(entry) = challenges.by_value(info_integer(arg(1))) else {
            return Ok(None);
        };
        let ch = entry.as_mut().expect("matched challenge");
        ch.last_response_at = now;
        let peer = ch.peer;
        let verb = arg(2);
        let reason = arg(3);
        let denial = if verb.eq_ignore_ascii_case(b"accept") {
            ch.guid = info_integer(arg(4));
            if let Some(message) = banned(ch.guid)? {
                Some(message.as_bytes().to_vec())
            } else if ch.connected {
                return Ok(None);
            } else {
                None
            }
        } else if verb.eq_ignore_ascii_case(b"demo") {
            (!demo_server).then(|| b"error\nEXE_ERR_NOT_A_DEMO_SERVER".to_vec())
        } else if verb.eq_ignore_ascii_case(b"deny") {
            let message: &[u8] =
                if reason.is_empty() || reason.eq_ignore_ascii_case(b"INVALID_CDKEY") {
                    b"error\nEXE_ERR_CDKEY_IN_USE".as_slice()
                } else if reason.eq_ignore_ascii_case(b"CLIENT_UNKNOWN_TO_AUTH")
                    || reason.eq_ignore_ascii_case(b"BAD_CDKEY")
                {
                    b"needcdkey"
                } else {
                    b"error\nEXE_ERR_BAD_CDKEY"
                };
            Some(message.to_vec())
        } else if reason.is_empty() {
            Some(b"error\nEXE_ERR_BAD_CDKEY".to_vec())
        } else {
            Some([b"error\n".as_slice(), reason].concat())
        };
        let bytes = if let Some(reason) = denial {
            *entry = None;
            [b"\xff\xff\xff\xff".as_slice(), &reason].concat()
        } else {
            ch.authorized = true;
            ch.reply(now)
        };
        Ok(Some(Datagram { peer, bytes }))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::admission::{Admission, AdmissionRules, admit};
    use std::net::UdpSocket;
    use std::time::Duration;

    fn mock() -> io::Result<(Master, UdpSocket)> {
        let socket = UdpSocket::bind((Ipv4Addr::LOCALHOST, 0))?;
        socket.set_read_timeout(Some(Duration::from_secs(1)))?;
        let mut master = Master::new(Ipv4Addr::UNSPECIFIED);
        for endpoint in [&mut master.listing, &mut master.authority] {
            endpoint.address = Some(Ipv4Addr::LOCALHOST);
            endpoint.port = socket.local_addr()?.port();
        }
        Ok((master, socket))
    }
    fn exchange(
        datagram: &Datagram,
        server: &UdpSocket,
        recipient: &UdpSocket,
    ) -> io::Result<(Vec<u8>, SocketAddrV4)> {
        server.send_to(&datagram.bytes, datagram.peer)?;
        let mut data = [0u8; 8192];
        let (size, addr) = recipient.recv_from(&mut data)?;
        let SocketAddr::V4(from) = addr else {
            unreachable!()
        };
        Ok((data[..size].to_vec(), from))
    }

    #[test]
    fn loopback_master_authenticates_and_preserves_guid_for_public_admission() -> io::Result<()> {
        let (mut master, authority) = mock()?;
        let server = UdpSocket::bind((Ipv4Addr::LOCALHOST, 0))?;
        let client = UdpSocket::bind((Ipv4Addr::LOCALHOST, 0))?;
        let SocketAddr::V4(peer) = client.local_addr()? else {
            unreachable!()
        };
        let mut challenges = Challenges::default();
        let request = master
            .request(peer, 100, &mut challenges, "test", false, Some(b"probe"))?
            .expect("mock master reply");
        let (wire, source) = exchange(&request, &server, &authority)?;
        let args = crate::query::oob_tokens(&wire).expect("connectionless packet");
        let value = info_integer(&args[1]);
        assert_eq!(
            wire,
            [
                b"\xff\xff\xff\xff".as_slice(),
                format!("getIpAuthorize {value} 127.0.0.1 \"test\" 0 PB \"probe\"").as_bytes()
            ]
            .concat()
        );
        assert_eq!(challenges.connect_ping(peer, value, 105), None);

        let mut reply = vec![0xff; 4];
        reply.extend(format!("ipAuthorize {value} accept 0 12345 x").bytes());
        let (raw, from) = exchange(
            &Datagram {
                bytes: reply,
                peer: source,
            },
            &authority,
            &server,
        )?;
        let args = crate::query::oob_tokens(&raw).expect("connectionless packet");
        let packet = master
            .authorize(from, 110, &args, &mut challenges, false, |_| Ok(None))?
            .expect("mock master reply");
        assert_eq!(
            exchange(&packet, &server, &client)?.0,
            [
                b"\xff\xff\xff\xff".as_slice(),
                format!("challengeResponse {value}").as_bytes()
            ]
            .concat()
        );
        let rules = AdmissionRules {
            private_clients: 0,
            private_password: b"",
            game_password: b"",
            reconnect_limit_ms: 0,
        };
        let info = format!("\\protocol\\118\\challenge\\{value}");
        assert!(matches!(
            admit(
                peer,
                info.as_bytes(),
                120,
                &mut challenges,
                &[None],
                &rules,
                Some((11, 100))
            ),
            Admission::Denied(_)
        ));
        let Admission::Eligible(slot) = admit(
            peer,
            info.as_bytes(),
            200,
            &mut challenges,
            &[None],
            &rules,
            Some((0, 10)),
        ) else {
            panic!("authorized player")
        };
        assert_eq!(slot.guid, 12345);
        assert!(!slot.is_lan);
        Ok(())
    }

    #[test]
    fn reason_codes_and_server_bans_clear_the_challenge() -> io::Result<()> {
        let (mut master, authority) = mock()?;
        let SocketAddr::V4(from) = authority.local_addr()? else {
            unreachable!()
        };
        let peer = SocketAddrV4::new(Ipv4Addr::LOCALHOST, 20501);
        let mut challenges = Challenges::default();
        for (verb, key, msg) in [
            ("deny", "BAD_CDKEY", "needcdkey"),
            ("deny", "CLIENT_UNKNOWN_TO_AUTH", "needcdkey"),
            ("deny", "INVALID_CDKEY", "error\nEXE_ERR_CDKEY_IN_USE"),
            ("deny", "BANNED_CDKEY", "error\nEXE_ERR_BAD_CDKEY"),
            ("demo", "", "error\nEXE_ERR_NOT_A_DEMO_SERVER"),
            (
                "accept",
                "0",
                "error\n\x15You are permanently banned from this server",
            ),
            ("pending", "Checking", "error\nChecking"),
        ] {
            let request = master
                .request(peer, 9, &mut challenges, "", false, None)?
                .expect("mock master reply");
            let val =
                crate::query::oob_tokens(&request.bytes).expect("connectionless packet")[1].clone();
            let args = vec![
                b"ipAuthorize".to_vec(),
                val.clone(),
                verb.as_bytes().to_vec(),
                key.as_bytes().to_vec(),
                b"123".to_vec(),
            ];
            let datagram = master
                .authorize(from, 10, &args, &mut challenges, false, |_| {
                    Ok(Some(
                        "error\n\x15You are permanently banned from this server",
                    ))
                })?
                .expect("mock master reply");
            assert_eq!(&datagram.bytes[4..], msg.as_bytes());
            assert!(challenges.by_value(info_integer(&val)).is_none());
        }
        Ok(())
    }

    #[test]
    fn wire_heartbeat_status_gamecomplete_and_shutdown() -> io::Result<()> {
        let (mut master, listener) = mock()?;
        let server = UdpSocket::bind((Ipv4Addr::LOCALHOST, 0))?;
        let browser = QueryState::default();
        let mut out = Vec::new();
        master.poll(0, false, false, &browser, &mut out);
        assert!(out.is_empty());
        master.poll(0, true, false, &browser, &mut out);
        assert_eq!(out.len(), 2);
        assert_eq!(
            exchange(&out[0], &server, &listener)?.0,
            b"\xff\xff\xff\xffheartbeat COD-2\n"
        );
        assert_eq!(
            exchange(&out[1], &server, &listener)?.0,
            browser
                .response(b"\xff\xff\xff\xffgetstatus")
                .expect("mock master reply")
        );
        out.clear();
        master.poll(179999, true, false, &browser, &mut out);
        assert!(out.is_empty());
        master.poll(180000, true, false, &browser, &mut out);
        assert_eq!(out.len(), 1);
        out.clear();
        master.population(1, 8);
        master.poll(180001, true, false, &browser, &mut out);
        assert_eq!(out.len(), 1);
        out.clear();
        master.poll(180002, true, true, &browser, &mut out);
        assert_eq!(
            exchange(&out[0], &server, &listener)?.0,
            b"\xff\xff\xff\xffheartbeat flatline\n"
        );
        let complete = master
            .announcement(Announcement::GameComplete, &browser)
            .expect("mock master reply");
        let (wire, _) = exchange(&complete, &server, &listener)?;
        assert!(wire.starts_with(b"\xff\xff\xff\xffgameCompleteStatus\n"));
        assert!(!wire.windows(7).any(|s| s == b"\\pswrd\\"));
        let mut local = Master::new(Ipv4Addr::LOCALHOST);
        out.clear();
        local.poll(0, true, false, &browser, &mut out);
        assert!(out.is_empty());
        assert!(local.listing.address.is_none());
        Ok(())
    }
}
