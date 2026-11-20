//! Dedicated IPv4 admission with PunkBuster disabled.
//! SV_DirectConnect (0x0808e2aa) selects a slot before ClientConnect
//! (0x080f8e7a) checks the game password and initializes the player.
use crate::challenge::Challenges;
use std::fmt;
use std::net::SocketAddrV4;

#[derive(Clone, Copy, Debug)]
pub struct ClientEndpoint {
    /// NA_BOT has an engine-local port identifier and never receives UDP.
    pub bot_port: Option<u16>,
    pub peer: SocketAddrV4,
    pub qport: i32,
    pub last_connect_ms: i32,
}

pub struct AdmissionRules<'a> {
    pub private_clients: usize,
    pub private_password: &'a [u8],
    pub game_password: &'a [u8],
    pub reconnect_limit_ms: i32,
}

impl fmt::Debug for AdmissionRules<'_> {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter
            .debug_struct("AdmissionRules")
            .field("private_clients", &self.private_clients)
            .field("reconnect_limit_ms", &self.reconnect_limit_ms)
            .finish_non_exhaustive()
    }
}

pub struct EligibleClient<'a> {
    pub is_lan: bool,
    pub slot: usize,
    pub guid: i32,
    pub endpoint: ClientEndpoint,
    pub challenge: i32,
    pub userinfo: &'a [u8],
}

impl fmt::Debug for EligibleClient<'_> {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter
            .debug_struct("EligibleClient")
            .field("slot", &self.slot)
            .field("endpoint", &self.endpoint)
            .finish_non_exhaustive()
    }
}

#[derive(Debug)]
pub enum Admission<'a> {
    Silent,
    Denied(Vec<u8>),
    Eligible(EligibleClient<'a>),
}

fn denied(reason: &[u8]) -> Admission<'static> {
    let mut packet = b"\xff\xff\xff\xfferror\n".to_vec();
    packet.extend_from_slice(reason);
    Admission::Denied(packet)
}

// Info_ValueForKey returns the first case-insensitive match.
pub(crate) fn info_value<'a>(userinfo: &'a [u8], key: &[u8]) -> &'a [u8] {
    let mut fields = userinfo
        .strip_prefix(b"\\")
        .unwrap_or(userinfo)
        .split(|byte| *byte == b'\\');
    while let (Some(name), Some(value)) = (fields.next(), fields.next()) {
        if name.eq_ignore_ascii_case(key) {
            return value;
        }
    }
    &[]
}

// The reference's i386 atoi uses a signed 32-bit long. It reads only the
// initial decimal number, with optional leading whitespace and sign.
pub(crate) fn info_integer(value: &[u8]) -> i32 {
    let value = value.trim_ascii_start();
    let (negative, digits) = match value.split_first() {
        Some((b'-', rest)) => (true, rest),
        Some((b'+', rest)) => (false, rest),
        _ => (false, value),
    };
    let mut result = 0i32;
    for digit in digits.iter().take_while(|digit| digit.is_ascii_digit()) {
        let digit = i32::from(digit - b'0');
        result = result.saturating_mul(10);
        result = if negative {
            result.saturating_sub(digit)
        } else {
            result.saturating_add(digit)
        };
    }
    result
}

pub fn admit_lan<'a>(
    peer: SocketAddrV4,
    userinfo: &'a [u8],
    now: i32,
    challenges: &mut Challenges,
    slots: &[Option<ClientEndpoint>],
    rules: &AdmissionRules<'_>,
) -> Admission<'a> {
    admit(peer, userinfo, now, challenges, slots, rules, None)
}

/// Non-LAN admission applies the configured lower and upper ping bounds.
pub fn admit<'a>(
    peer: SocketAddrV4,
    userinfo: &'a [u8],
    now: i32,
    challenges: &mut Challenges,
    slots: &[Option<ClientEndpoint>],
    rules: &AdmissionRules<'_>,
    ping_bounds: Option<(i32, i32)>,
) -> Admission<'a> {
    // Q_strncpyz(userinfo, argv[1], 0x400).
    let userinfo = &userinfo[..userinfo.len().min(1023)];
    let userinfo = userinfo.split(|byte| *byte == 0).next().unwrap_or_default();
    if info_integer(info_value(userinfo, b"protocol")) != 118 {
        return denied(b"EXE_SERVER_IS_DIFFERENT_VER\x151.3\n");
    }
    let qport = info_integer(info_value(userinfo, b"qport"));
    let returning_slot = slots.iter().enumerate().find_map(|(slot, endpoint)| {
        let endpoint = endpoint.as_ref()?;
        (endpoint.bot_port.is_none()
            && endpoint.peer.ip() == peer.ip()
            && (endpoint.qport == qport || endpoint.peer.port() == peer.port()))
        .then_some((slot, endpoint))
    });
    if let Some((_, endpoint)) = returning_slot
        && now.wrapping_sub(endpoint.last_connect_ms) < rules.reconnect_limit_ms
    {
        return Admission::Silent;
    }

    let challenge = info_integer(info_value(userinfo, b"challenge"));
    let Some((ping, guid)) = challenges.connect_identity(peer, challenge, now) else {
        return denied(b"EXE_BAD_CHALLENGE");
    };
    if ping_bounds.is_some_and(|(min, _)| min != 0 && ping < min) {
        return denied(b"EXE_ERR_HIGH_PING_ONLY");
    }
    if ping_bounds.is_some_and(|(_, max)| max != 0 && ping > max) {
        return denied(b"EXE_ERR_LOW_PING_ONLY");
    }

    let password = info_value(userinfo, b"password");
    let slot = if let Some((slot, _)) = returning_slot {
        slot
    } else {
        let first_slot = if password == rules.private_password {
            0
        } else {
            rules.private_clients
        };
        let Some((free_slot, _)) = slots
            .iter()
            .enumerate()
            .skip(first_slot)
            .find(|(_, endpoint)| endpoint.is_none())
        else {
            return denied(b"EXE_SERVERISFULL");
        };
        free_slot
    };
    if !rules.game_password.is_empty()
        && !rules.game_password.eq_ignore_ascii_case(b"none")
        && password != rules.game_password
    {
        return denied(b"GAME_INVALIDPASSWORD");
    }
    Admission::Eligible(EligibleClient {
        slot,
        guid,
        is_lan: ping_bounds.is_none(),
        endpoint: ClientEndpoint {
            bot_port: None,
            peer,
            qport,
            last_connect_ms: now,
        },
        challenge,
        userinfo,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::Ipv4Addr;

    fn rules() -> AdmissionRules<'static> {
        AdmissionRules {
            private_clients: 1,
            private_password: b"reserve",
            game_password: b"NoNe",
            reconnect_limit_ms: 3000,
        }
    }

    fn assert_denied(decision: Admission, expected: &[u8]) {
        let Admission::Denied(packet) = decision else {
            panic!("expected rejection")
        };
        assert_eq!(packet, expected);
    }

    #[test]
    fn protocol_precedes_challenge_and_capacity() {
        let mut challenges = Challenges::default();
        let from = SocketAddrV4::new(Ipv4Addr::LOCALHOST, 1024);
        assert_denied(
            admit_lan(from, b"", 10, &mut challenges, &[], &rules()),
            b"\xff\xff\xff\xfferror\nEXE_SERVER_IS_DIFFERENT_VER\x151.3\n",
        );
        assert_denied(
            admit_lan(
                from,
                br"\Protocol\118suffix",
                10,
                &mut challenges,
                &[],
                &rules(),
            ),
            b"\xff\xff\xff\xfferror\nEXE_BAD_CHALLENGE",
        );
    }

    #[test]
    fn reserved_slots_precede_game_password_check() -> std::io::Result<()> {
        let mut challenges = Challenges::default();
        let from = SocketAddrV4::new(Ipv4Addr::LOCALHOST, 3030);
        let response = challenges.lan_response(from, 100)?;
        let token = std::str::from_utf8(
            response
                .split(|&b| b == b' ')
                .next_back()
                .unwrap_or_default(),
        )
        .expect("decimal challenge");
        let userinfo = format!(r"\protocol\118\qport\32678\challenge\{token}");
        let slots = [None];
        let mut rules = rules();
        rules.game_password = b"secret";
        assert_denied(
            admit_lan(
                from,
                userinfo.as_bytes(),
                110,
                &mut challenges,
                &slots,
                &rules,
            ),
            b"\xff\xff\xff\xfferror\nEXE_SERVERISFULL",
        );
        let with_password = format!(r"{userinfo}\password\reserve");
        assert_denied(
            admit_lan(
                from,
                with_password.as_bytes(),
                111,
                &mut challenges,
                &slots,
                &rules,
            ),
            b"\xff\xff\xff\xfferror\nGAME_INVALIDPASSWORD",
        );
        rules.game_password = b"NONE";
        let Admission::Eligible(client) = admit_lan(
            from,
            with_password.as_bytes(),
            120,
            &mut challenges,
            &slots,
            &rules,
        ) else {
            panic!("expected private slot")
        };
        assert_eq!(client.slot, 0);
        assert_eq!(client.endpoint.qport, 32678);
        assert_eq!(client.userinfo, with_password.as_bytes());
        assert_eq!(client.challenge.to_string(), token);
        let occupied = [Some(client.endpoint)];
        assert!(matches!(
            admit_lan(
                from,
                userinfo.as_bytes(),
                121,
                &mut challenges,
                &occupied,
                &rules
            ),
            Admission::Silent
        ));
        assert!(matches!(
            admit_lan(
                from,
                userinfo.as_bytes(),
                3120,
                &mut challenges,
                &occupied,
                &rules
            ),
            Admission::Eligible(_)
        ));
        Ok(())
    }
}
