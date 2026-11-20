//! Per-peer challenge allocation for the dedicated server branches of SV_RequestIpAuthorize (0x0808d18e).

use std::io;
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4, ToSocketAddrs};

const CHALLENGE_COUNT: usize = 1024;

#[derive(Clone, Copy, Debug)]
pub(crate) struct Challenge {
    pub(crate) peer: SocketAddrV4,
    pub(crate) value: i32,
    pub(crate) issued_at: i32,
    pub(crate) last_response_at: i32,
    first_ping: i32,
    pub(crate) connected: bool,
    pub(crate) authorized: bool,
    pub(crate) guid: i32,
}

#[derive(Debug)]
pub struct Challenges {
    draws_pending: usize,
    pub connect_observation: Option<(usize, i32)>,
    slots: Box<[Option<Challenge>]>,
}

impl Default for Challenges {
    fn default() -> Self {
        Self {
            draws_pending: 0,
            connect_observation: None,
            slots: vec![None; CHALLENGE_COUNT].into_boxed_slice(),
        }
    }
}

/// NET_GetLocalAddress resolves up to 15 IPv4 addresses for the machine's
/// hostname at startup. An explicitly bound interface is already known.
pub fn local_addresses(bound: Ipv4Addr) -> Vec<Ipv4Addr> {
    let mut addresses = vec![bound];
    if bound.is_loopback() {
        return addresses;
    }
    if let Some((_, local)) = hostname_addresses() {
        addresses.extend(local);
    }
    addresses
}

/// NET_GetLocalAddress uses the lookup both for its startup report and for
/// recognizing clients on the same LAN.
pub fn hostname_addresses() -> Option<(String, Vec<Ipv4Addr>)> {
    let name = hostname::get().ok()?.into_string().ok()?;
    let resolved = (name.as_str(), 0).to_socket_addrs().ok()?;
    let mut addresses = Vec::new();
    for address in resolved {
        if let SocketAddr::V4(v4) = address
            && !addresses.contains(v4.ip())
        {
            addresses.push(*v4.ip());
            if addresses.len() == 15 {
                break;
            }
        }
    }
    Some((name, addresses))
}

/// Sys_IsLANAddress also treats a peer with the same first three octets as
/// a local interface as LAN, even when that interface has a public address.
pub fn is_lan_address(peer: Ipv4Addr, local_addresses: &[Ipv4Addr]) -> bool {
    peer.is_private()
        || peer.is_loopback()
        || peer.is_link_local()
        || local_addresses
            .iter()
            .any(|local| local.octets()[..3] == peer.octets()[..3])
}

impl Challenges {
    /// The original spends two libc rand() draws on each newly allocated challenge.
    /// Preserve subsequent gameplay draw order while generating the wire challenge from OS entropy.
    pub fn account_game_random(&mut self, rng: &mut crate::vm::items::NativeRandom) {
        for _ in 0..std::mem::take(&mut self.draws_pending) {
            rng.draw();
        }
    }
    pub fn lan_response(&mut self, from: SocketAddrV4, server_time_ms: i32) -> io::Result<Vec<u8>> {
        let challenge = self.request(from, server_time_ms, random_words)?;
        challenge.authorized = true;
        Ok(challenge.reply(server_time_ms))
    }
}

fn random_words() -> io::Result<[u32; 2]> {
    let mut bytes = [0; 8];
    getrandom::fill(&mut bytes).map_err(|error| io::Error::other(error.to_string()))?;
    let [a, b, c, d, e, f, g, h] = bytes;
    Ok([
        u32::from_le_bytes([a, b, c, d]),
        u32::from_le_bytes([e, f, g, h]),
    ])
}

impl Challenge {
    pub(crate) fn reply(&mut self, now: i32) -> Vec<u8> {
        self.last_response_at = now;
        let mut response = vec![0xff; 4];
        response.extend_from_slice(format!("challengeResponse {}", self.value).as_bytes());
        response
    }
}
impl Challenges {
    pub(crate) fn request_remote(
        &mut self,
        from: SocketAddrV4,
        now: i32,
    ) -> io::Result<&mut Challenge> {
        self.request(from, now, random_words)
    }
    pub(crate) fn by_value(&mut self, value: i32) -> Option<&mut Option<Challenge>> {
        self.slots
            .iter_mut()
            .find(|entry| entry.is_some_and(|c| c.value == value))
    }

    fn request(
        &mut self,
        from: SocketAddrV4,
        server_time_ms: i32,
        random_words: impl FnOnce() -> io::Result<[u32; 2]>,
    ) -> io::Result<&mut Challenge> {
        let mut oldest = 0;
        let mut oldest_time = i32::MAX;
        let mut existing = None;
        for (index, slot) in self.slots.iter().enumerate() {
            if slot.is_some_and(|challenge| !challenge.connected && challenge.peer == from) {
                existing = Some(index);
                break;
            }
            // Empty slots in the original table have an issue time of zero.
            let issued_at = slot.map_or(0, |challenge| challenge.issued_at);
            if issued_at < oldest_time {
                oldest_time = issued_at;
                oldest = index;
            }
        }
        let challenge = if let Some(existing) = existing {
            self.slots[existing]
                .as_mut()
                .expect("matched an occupied challenge slot")
        } else {
            let [first, second] = random_words()?;
            self.draws_pending += 2;
            let value = ((first << 16) ^ second ^ server_time_ms as u32) as i32;
            self.slots[oldest].insert(Challenge {
                peer: from,
                value,
                issued_at: server_time_ms,
                last_response_at: server_time_ms,
                first_ping: 0,
                connected: false,
                authorized: false,
                guid: 0,
            })
        };
        Ok(challenge)
    }

    #[cfg(test)]
    fn lan_response_with(
        &mut self,
        from: SocketAddrV4,
        now: i32,
        random: impl FnOnce() -> io::Result<[u32; 2]>,
    ) -> io::Result<Vec<u8>> {
        let challenge = self.request(from, now, random)?;
        challenge.authorized = true;
        Ok(challenge.reply(now))
    }

    /// A client dropped before entering the world releases its challenge for reuse.
    pub fn disconnect(&mut self, peer: SocketAddrV4) {
        if let Some(challenge) = self
            .slots
            .iter_mut()
            .flatten()
            .find(|slot| slot.peer == peer)
        {
            challenge.connected = false;
        }
    }

    /// SV_DirectConnect (0x0808e2aa) measures ping on the first successful
    /// address/value lookup. Subsequent connects reuse the first nonzero ping.
    pub fn connect_ping(&mut self, peer: SocketAddrV4, value: i32, now: i32) -> Option<i32> {
        self.connect_identity(peer, value, now)
            .map(|(ping, _)| ping)
    }
    pub(crate) fn connect_identity(
        &mut self,
        peer: SocketAddrV4,
        value: i32,
        now: i32,
    ) -> Option<(i32, i32)> {
        let (index, slot) = self
            .slots
            .iter_mut()
            .enumerate()
            .find_map(|(index, entry)| {
                let slot = entry.as_mut()?;
                (slot.authorized && slot.peer == peer && slot.value == value)
                    .then_some((index, slot))
            })?;
        if slot.first_ping == 0 {
            slot.first_ping = now.wrapping_sub(slot.last_response_at);
        }
        slot.connected = true;
        self.connect_observation = Some((index, slot.first_ping));
        Some((slot.first_ping, slot.guid))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn client(port: u16) -> SocketAddrV4 {
        SocketAddrV4::new(Ipv4Addr::LOCALHOST, port)
    }

    #[test]
    fn signed_decimal_challenge_and_ping_follow_last_reply() -> io::Result<()> {
        let mut table = Challenges::default();
        let first = table.lan_response_with(client(3000), 100, || Ok([0x8000, 100]))?;
        assert_eq!(first, b"\xff\xff\xff\xffchallengeResponse -2147483648");
        assert_eq!(
            table.lan_response_with(client(3000), 117, || panic!("reused challenge"))?,
            first
        );
        assert_eq!(table.connect_ping(client(3001), i32::MIN, 119), None);
        assert_eq!(table.connect_ping(client(3000), i32::MIN, 119), Some(2));
        assert_eq!(table.connect_ping(client(3000), i32::MIN, 121), Some(2));
        assert_ne!(
            table.lan_response_with(client(3000), 122, || Ok([1, 2]))?,
            first
        );
        Ok(())
    }

    #[test]
    fn replacement_uses_issue_time_including_ties() -> io::Result<()> {
        let mut table = Challenges::default();
        for port in 1..=CHALLENGE_COUNT as u16 {
            table.lan_response_with(client(port), 100, || Ok([0, port.into()]))?;
        }
        table.lan_response_with(client(1), 300, || panic!("existing"))?;
        table.lan_response_with(client(2000), 301, || Ok([0, 0]))?;
        assert_eq!(table.connect_ping(client(1), 101, 310), None);
        assert_eq!(table.connect_ping(client(2), 102, 310), Some(210));
        Ok(())
    }

    #[test]
    fn public_local_interfaces_match_only_the_same_24_bit_prefix() {
        let interfaces = [Ipv4Addr::new(198, 51, 100, 8)];
        assert!(is_lan_address(Ipv4Addr::new(198, 51, 100, 3), &interfaces));
        assert!(!is_lan_address(Ipv4Addr::new(198, 51, 101, 3), &interfaces));
        assert!(is_lan_address(Ipv4Addr::new(172, 31, 1, 2), &[]));
        assert!(!is_lan_address(Ipv4Addr::new(172, 32, 1, 2), &[]));
    }
}
