//! Protocol 118 netchannel as used by the dedicated server.
//!
//! Server fragments carry the offset as a 32-bit little-endian integer. Only
//! client datagrams carry the qport. Each logical payload is keyed once,
//! before splitting for transmission or after reassembling on receipt.
use std::{io, net::SocketAddrV4};

const FRAGMENT: u32 = 1 << 31;
const FRAGMENT_BYTES: usize = 1300;
pub const MAX_MESSAGE: usize = 0x20000;

#[derive(Debug, PartialEq, Eq)]
pub struct ClientPayload {
    pub server_id: u8,
    pub message_ack: i32,
    pub reliable_ack: i32,
    /// XOR-keyed compressed bytes following the nine clear header bytes.
    pub body: Vec<u8>,
}

impl ClientPayload {
    /// SV_Netchan_Decode. The caller selects reliableCommands[reliable_ack & 127].
    pub fn decode(&mut self, challenge: i32, acked_command: &[u8]) {
        let key = challenge as u8 ^ self.server_id ^ self.message_ack as u8;
        apply_key(&mut self.body, key, acked_command);
    }
}

#[derive(Debug)]
struct Pending {
    data: Vec<u8>,
    start: usize,
}

#[derive(Debug)]
pub struct Netchan {
    peer: SocketAddrV4,
    qport: u16,
    challenge: i32,
    incoming: u32,
    outgoing: u32,
    dropped: u32,
    fragment_sequence: u32,
    fragment_buffer: Vec<u8>,
    pending: Option<Pending>,
    reliable_ack: i32,
    message_ack: i32,
}

impl Netchan {
    pub fn new(peer: SocketAddrV4, qport: u16, challenge: i32) -> Self {
        Self {
            peer,
            qport,
            challenge,
            incoming: 0,
            outgoing: 1,
            dropped: 0,
            fragment_sequence: 0,
            fragment_buffer: Vec::new(),
            pending: None,
            reliable_ack: 0,
            message_ack: 0,
        }
    }

    pub(crate) fn translate_port(&mut self, port: u16) {
        self.peer.set_port(port);
    }
    pub fn peer(&self) -> SocketAddrV4 {
        self.peer
    }
    pub fn incoming_sequence(&self) -> u32 {
        self.incoming
    }
    pub fn outgoing_sequence(&self) -> u32 {
        self.outgoing
    }
    pub fn dropped(&self) -> u32 {
        self.dropped
    }
    pub fn reliable_ack(&self) -> i32 {
        self.reliable_ack
    }
    pub fn message_ack(&self) -> i32 {
        self.message_ack
    }
    pub fn pending_bytes(&self) -> usize {
        self.pending.as_ref().map_or(0, |p| p.data.len() - p.start)
    }
    pub fn has_pending_fragments(&self) -> bool {
        self.pending.is_some()
    }

    pub fn matches(&self, from: SocketAddrV4, packet: &[u8]) -> bool {
        from.ip() == self.peer.ip()
            && packet.get(..4) != Some(&[0xff; 4])
            && packet.get(4..6) == Some(self.qport.to_le_bytes().as_slice())
    }

    /// Reassemble and process the clear client header (SV_ReadPackets).
    /// The caller uses matches() to distinguish an unknown peer from an
    /// incomplete or stale packet on an existing channel.
    pub fn receive(
        &mut self,
        from: SocketAddrV4,
        packet: &[u8],
        reliable_sequence: i32,
    ) -> Option<ClientPayload> {
        if !self.matches(from, packet) {
            return None;
        }
        // A NAT mapping can change the UDP source port between packets.
        self.peer = from;
        let header = u32::from_le_bytes(packet.get(..4)?.try_into().ok()?);
        let sequence = header & !FRAGMENT;
        if sequence <= self.incoming {
            return None;
        }
        self.dropped = sequence - self.incoming - 1;
        let data = packet.get(6..)?;
        let payload = if header & FRAGMENT == 0 {
            if data.len() > MAX_MESSAGE {
                return None;
            }
            data.to_owned()
        } else {
            if sequence != self.fragment_sequence {
                self.fragment_sequence = sequence;
                self.fragment_buffer.clear();
            }
            let offset = u32::from_le_bytes(data.get(..4)?.try_into().ok()?) as usize;
            let length =
                usize::try_from(i16::from_le_bytes(data.get(4..6)?.try_into().ok()?)).ok()?;
            if offset != self.fragment_buffer.len() || offset.checked_add(length)? > MAX_MESSAGE {
                return None;
            }
            self.fragment_buffer.extend(data.get(6..6 + length)?);
            if length == FRAGMENT_BYTES {
                return None;
            }
            std::mem::take(&mut self.fragment_buffer)
        };
        self.incoming = sequence;
        let server_id = *payload.first()?;
        let message_ack = i32::from_le_bytes(payload.get(1..5)?.try_into().ok()?);
        let reliable_ack = i32::from_le_bytes(payload.get(5..9)?.try_into().ok()?);
        // Reliable queues start at zero and refuse to advance past i32::MAX.
        // Only retain acknowledgements for commands that actually exist.
        if message_ack < 0 || !(0..=reliable_sequence).contains(&reliable_ack) {
            return None;
        }
        if reliable_sequence - reliable_ack > 127 {
            self.reliable_ack = reliable_sequence;
            return None;
        }
        self.message_ack = message_ack;
        self.reliable_ack = reliable_ack;
        let mut body = payload;
        body.drain(..9);
        Some(ClientPayload {
            server_id,
            message_ack: self.message_ack,
            reliable_ack: self.reliable_ack,
            body,
        })
    }

    /// SV_Netchan_Transmit. The first four clear bytes acknowledge the last
    /// client command, followed by the keyed compressed server message.
    pub fn transmit(
        &mut self,
        ack_client_cmd: i32,
        compressed_body: &[u8],
        last_client_cmd: &[u8],
    ) -> io::Result<Vec<u8>> {
        if self.pending.is_some() {
            return Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "unsent netchannel fragments",
            ));
        }
        if compressed_body.len() > MAX_MESSAGE - 4 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "netchannel message exceeds 128 KiB",
            ));
        }
        let mut payload = Vec::with_capacity(4 + compressed_body.len());
        payload.extend(ack_client_cmd.to_le_bytes());
        payload.extend(compressed_body);
        apply_key(
            &mut payload[4..],
            self.outgoing as u8 ^ self.challenge as u8,
            last_client_cmd,
        );
        if payload.len() >= FRAGMENT_BYTES {
            self.pending = Some(Pending {
                data: payload,
                start: 0,
            });
            Ok(self.next_fragment().expect("pending first fragment"))
        } else {
            let mut packet = self.outgoing.to_le_bytes().to_vec();
            self.outgoing = self.outgoing.wrapping_add(1);
            packet.extend(payload);
            Ok(packet)
        }
    }

    /// A message exactly divisible by 1300 has a final zero-length fragment.
    pub fn next_fragment(&mut self) -> Option<Vec<u8>> {
        let pending = self.pending.as_mut()?;
        let start = pending.start;
        let length = (pending.data.len() - start).min(FRAGMENT_BYTES);
        let mut packet = Vec::with_capacity(10 + length);
        packet.extend((self.outgoing | FRAGMENT).to_le_bytes());
        packet.extend((start as u32).to_le_bytes());
        packet.extend((length as u16).to_le_bytes());
        packet.extend(&pending.data[start..start + length]);
        pending.start += length;
        if length < FRAGMENT_BYTES {
            self.outgoing = self.outgoing.wrapping_add(1);
            self.pending = None;
        }
        Some(packet)
    }
}

// Called on the compressed body only. A newly cleared command buffer supplies
// zero for every byte when no command has yet been acknowledged.
fn apply_key(data: &mut [u8], mut key: u8, command: &[u8]) {
    let command = command.split(|byte| *byte == 0).next().unwrap_or_default();
    if command.is_empty() {
        for byte in data {
            *byte ^= key;
        }
        return;
    }
    for (i, (byte, letter)) in data.iter_mut().zip(command.iter().cycle()).enumerate() {
        key ^= letter << (i & 1);
        *byte ^= key;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn channel() -> Netchan {
        Netchan::new("127.0.0.1:28970".parse().expect("IPv4"), 0x1d2b, 0x12)
    }

    fn client(seq: u32, data: &[u8]) -> Vec<u8> {
        [seq.to_le_bytes().as_slice(), &0x1d2bu16.to_le_bytes(), data].concat()
    }

    fn fragment(seq: u32, offset: u32, bytes: &[u8]) -> Vec<u8> {
        client(
            seq | FRAGMENT,
            &[
                offset.to_le_bytes().as_slice(),
                &(bytes.len() as i16).to_le_bytes(),
                bytes,
            ]
            .concat(),
        )
    }

    #[test]
    fn packet_sequence_and_ack_header() {
        let mut chan = channel();
        let peer = "127.0.0.1:30000".parse().expect("IPv4");
        let packet = client(2, &[0x22, 3, 0, 0, 0, 1, 0, 0, 0, 1, 2, 3, 4, 5]);
        let mut received = chan.receive(peer, &packet, 2).expect("message");
        assert_eq!(chan.peer(), peer);
        assert_eq!(chan.dropped(), 1);
        received.decode(0x12, b"ab");
        assert_eq!(received.body, [0x53, 0x94, 0xf4, 0x37, 0x57]);
        assert!(chan.receive(peer, &packet, 2).is_none());
        assert_eq!(chan.incoming_sequence(), 2);
    }

    #[test]
    fn contiguous_fragments_or_new_sequence() {
        let mut chan = channel();
        let peer = chan.peer();
        let head = vec![0; FRAGMENT_BYTES];
        assert!(chan.receive(peer, &fragment(1, 0, &head), 0).is_none());
        assert!(chan.receive(peer, &fragment(1, 1301, &[3]), 0).is_none());
        let complete = chan
            .receive(peer, &fragment(1, 1300, &[2]), 0)
            .expect("joined");
        assert_eq!(complete.body.len(), 1292);
        assert_eq!(complete.body.last(), Some(&2));
        assert!(chan.receive(peer, &fragment(2, 0, &head), 0).is_none());
        assert!(chan.receive(peer, &fragment(3, 0, &head), 0).is_none());
        assert!(chan.receive(peer, &fragment(3, 1300, &[]), 0).is_some());
    }

    #[test]
    fn outgoing_header_and_terminal_empty_fragment() {
        let mut chan = channel();
        let packet = chan
            .transmit(124, &[1, 2, 3, 4, 5], b"")
            .expect("small packet");
        assert_eq!(
            packet,
            [1, 0, 0, 0, 124, 0, 0, 0, 0x12, 0x11, 0x10, 0x17, 0x16]
        );
        let body = vec![10; 2596];
        let first = chan.transmit(7, &body, b"abcdefgh").expect("fragmented");
        assert_eq!(&first[..10], &[2, 0, 0, 128, 0, 0, 0, 0, 0x14, 5]);
        assert_eq!(chan.outgoing_sequence(), 2);
        assert_eq!(chan.next_fragment().expect("second").len(), 1310);
        assert_eq!(
            chan.next_fragment().expect("terminator"),
            [2, 0, 0, 128, 0x28, 10, 0, 0, 0, 0]
        );
        assert_eq!(chan.outgoing_sequence(), 3);
        assert!(chan.next_fragment().is_none());
    }

    #[test]
    fn rejects_wrong_peer_and_unusable_ack() {
        let mut chan = channel();
        let packet = client(1, &[0; 9]);
        assert!(!chan.matches("10.2.3.4:28970".parse().expect("IPv4"), &packet));
        let mut wrong_qport = packet.clone();
        wrong_qport[4] = 0;
        assert!(!chan.matches(chan.peer(), &wrong_qport));
        assert!(chan.receive(chan.peer(), &packet, 128).is_none());
        assert_eq!(chan.reliable_ack(), 128);
    }
}
