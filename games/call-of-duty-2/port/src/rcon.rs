//! Remote console dispatch and printable operator replies for protocol 118.
use crate::query::tokenize;
use std::io::Write;
use std::net::SocketAddrV4;

#[derive(Debug, Default)]
pub struct Rcon {
    last_request: i32,
}

#[derive(Debug, PartialEq, Eq)]
pub enum RconAction {
    Ignore,
    Print(&'static str),
    Execute(Vec<Vec<u8>>),
}

impl Rcon {
    /// SVC_RemoteCommand limits all RCON requests with one global clock.
    pub fn handle(&mut self, args: &[Vec<u8>], password: &str, now: i32) -> RconAction {
        if args.first().map(Vec::as_slice) != Some(b"rcon")
            || (self.last_request != 0 && now.wrapping_sub(self.last_request) < 500)
        {
            return RconAction::Ignore;
        }
        self.last_request = now;
        if password.is_empty() {
            return RconAction::Print(
                "The server must set 'rcon_password' for clients to use 'rcon'.\n",
            );
        }
        let supplied = args.get(1).map_or(&[][..], Vec::as_slice);
        if supplied.is_empty() {
            return RconAction::Print(
                "You must log in with 'rcon login <password>' before using 'rcon'.\n",
            );
        }
        if supplied != password.as_bytes() {
            return RconAction::Print("Invalid password.\n");
        }

        // The engine reassembles the arguments and calls Cmd_ExecuteString
        // directly. Cbuf's semicolon/newline command splitting does not run.
        let mut line = Vec::new();
        for arg in &args[2..] {
            let quote = arg.is_empty() || arg.iter().any(|byte| (*byte as i8) < 33);
            let length = arg.len() + usize::from(quote) * 2 + 1;
            if line.len() + length >= 1024 {
                return RconAction::Print("");
            }
            if quote {
                line.push(b'"');
            }
            line.extend(arg);
            if quote {
                line.push(b'"');
            }
            line.push(b' ');
        }
        RconAction::Execute(tokenize(&line))
    }
}

/// One SV_FlushRedirect call. The 1300-byte formatting buffer holds
/// six prefix bytes and a trailing NUL, so full 1294-byte chunks send
/// 1293 text bytes. The original advances by all 1294.
pub fn print_packets(output: &[u8]) -> Vec<Vec<u8>> {
    const PREFIX: &[u8] = b"\xff\xff\xff\xffprint\n";
    const CHUNK: usize = 0x50e;
    let text = output.split(|b| *b == 0).next().unwrap_or_default();
    if text.is_empty() {
        return vec![PREFIX.to_vec()];
    }
    text.chunks(CHUNK)
        .map(|chunk| [PREFIX, &chunk[..chunk.len().min(CHUNK - 1)]].concat())
        .collect()
}

pub fn server_info(info: &[u8]) -> Vec<u8> {
    print_info(b"Server info settings:\n", info)
}

pub fn print_info(header: &[u8], info: &[u8]) -> Vec<u8> {
    let mut output = header.to_vec();
    let trimmed = info.strip_prefix(b"\\").unwrap_or(info);
    if trimmed.is_empty() {
        return output;
    }
    let mut parts = trimmed.split(|b| *b == b'\\');
    while let Some(key) = parts.next() {
        output.extend(key);
        output.resize(output.len() + 20usize.saturating_sub(key.len()), b' ');
        if let Some(value) = parts.next() {
            output.extend(value);
            output.push(b'\n');
        } else {
            output.extend(b"MISSING VALUE\n");
        }
    }
    output
}

#[derive(Debug)]
pub struct StatusClient<'a> {
    pub slot: usize,
    pub score: i32,
    /// 1 = zombie, 2 = connected, 3 = primed, 4 = active.
    pub state: i32,
    pub ping: i32,
    pub guid: i32,
    pub name: &'a [u8],
    pub last_message_age_ms: i32,
    pub address: Option<SocketAddrV4>,
    pub bot_port: Option<u16>,
    pub qport: i32,
    pub rate: i32,
}

/// The caller supplies occupied clients in ascending slot order.
pub fn status(map: &str, clients: &[StatusClient<'_>]) -> Vec<u8> {
    let mut output = format!("map: {map}\n").into_bytes();
    output.extend(
        b"num score ping guid   name            lastmsg address               qport rate\n",
    );
    output.extend(
        b"--- ----- ---- ------ --------------- ------- --------------------- ----- -----\n",
    );
    for client in clients {
        let ping = match client.state {
            0 => continue,
            1 => "ZMBI".to_owned(),
            2 => "CNCT".to_owned(),
            _ => format!("{:4}", client.ping.min(9999)),
        };
        write!(
            output,
            "{:3} {:5} {} {:6} ",
            client.slot, client.score, ping, client.guid
        )
        .expect("in-memory write");
        output.extend(client.name);
        output.extend(b"^7");
        let mut name = client.name;
        let mut name_width = 0;
        while !name.is_empty() {
            if name[0] == b'^' && name.get(1).is_some_and(u8::is_ascii_digit) {
                name = &name[2..];
            } else {
                name_width += 1;
                name = &name[1..];
            }
        }
        output.resize(output.len() + 16usize.saturating_sub(name_width), b' ');
        let address = if let Some(port) = client.bot_port {
            format!("00000000.000000000000:{}", port.swap_bytes() as i16)
        } else {
            client.address.map_or_else(
                || "loopback".to_owned(),
                |addr| format!("{}:{}", addr.ip(), addr.port() as i16),
            )
        };
        writeln!(
            output,
            "{:7} {:22}{:5} {:5}",
            client.last_message_age_ms, address, client.qport, client.rate
        )
        .expect("in-memory write");
    }
    output.push(b'\n');
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rcon_throttles_globally_and_validates_before_dispatch() {
        let mut rcon = Rcon::default();
        let command = tokenize(br#"rcon TEST set sv_hostname "Test Server""#);
        assert!(
            matches!(rcon.handle(&command, "", 1), RconAction::Print(s) if s.starts_with("The server"))
        );
        assert_eq!(rcon.handle(&command, "TEST", 500), RconAction::Ignore);
        assert_eq!(
            rcon.handle(&command, "test", 501),
            RconAction::Print("Invalid password.\n")
        );
        assert!(
            matches!(rcon.handle(&tokenize(b"rcon"), "TEST", 1001), RconAction::Print(s) if s.starts_with("You must log"))
        );
        assert_eq!(
            rcon.handle(&command, "TEST", 1501),
            RconAction::Execute(vec![
                b"set".to_vec(),
                b"sv_hostname".to_vec(),
                b"Test Server".to_vec()
            ])
        );
    }

    #[test]
    fn redirect_preserves_stock_chunk_boundaries() {
        let mut message = vec![b'x'; 1293];
        message.extend(b"YZ");
        let chunks = print_packets(&message);
        assert_eq!(chunks[0].len(), 1303);
        assert_eq!(chunks[1], b"\xff\xff\xff\xffprint\nZ");
        assert_eq!(print_packets(b"")[0], b"\xff\xff\xff\xffprint\n");
    }

    #[test]
    fn renders_operator_tables() {
        assert_eq!(
            server_info(br"\sv_hostname\Test"),
            b"Server info settings:\nsv_hostname         Test\n"
        );
        let client = StatusClient {
            slot: 0,
            score: 5,
            state: 2,
            ping: 78,
            guid: 2,
            name: b"^1Bob",
            last_message_age_ms: 10,
            bot_port: None,
            address: Some("127.0.0.1:60000".parse().expect("address")),
            qport: 1234,
            rate: 25000,
        };
        let result = String::from_utf8(status("mp_test", &[client])).expect("ASCII");
        assert!(result.ends_with("  0     5 CNCT      2 ^1Bob^7                  10 127.0.0.1:-5536        1234 25000\n\n"), "{result}");
    }
}
