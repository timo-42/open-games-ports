//! CoD2 connectionless voice transport. Voice codec frames are opaque to the server.
use crate::config::ServerConfig;
use crate::message::MessageReader;
use std::io;

const MAX_QUEUED: usize = 40;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct VoicePlayer {
    pub team: u8,
    pub session_state: u8,
}

#[derive(Debug)]
pub(crate) struct VoiceRules {
    pub enabled: bool,
    global: bool,
    echo: bool,
    dead_chat: bool,
}

impl VoiceRules {
    pub(crate) fn from_config(config: &ServerConfig) -> Self {
        Self {
            enabled: config.get("sv_voice") == Some("1"),
            global: config.get("voice_global") == Some("1"),
            echo: config.get("voice_localEcho") == Some("1"),
            dead_chat: config.get("voice_deadChat") == Some("1"),
        }
    }

    // G_SayToEligibleClients, 0x080f9976. Pre-world talkers take a distinct
    // relay path, before the game knows their team or session state.
    pub(crate) fn audible(
        &self,
        speaker: Option<VoicePlayer>,
        listener: Option<VoicePlayer>,
        same_slot: bool,
    ) -> bool {
        let Some(speaker) = speaker else {
            return !same_slot;
        };
        let Some(listener) = listener else {
            return false;
        };
        let compatible_team = listener.session_state == 3
            || self.global
            || speaker.team == 0
            || speaker.team == listener.team;
        let compatible_state = speaker.session_state == listener.session_state
            || (self.dead_chat && (speaker.session_state == 1 || listener.session_state == 1));
        compatible_team && compatible_state && (!same_slot || self.echo)
    }
}

pub(crate) fn payload(packet: &[u8]) -> Option<&[u8]> {
    let body = packet.strip_prefix(&[0xff; 4])?;
    let end = body.iter().position(|b| matches!(*b, 0 | b'\n'))?;
    body.get(end + 1..)
}

#[derive(Debug, Default)]
pub(crate) struct Listener {
    pub enabled: bool,
    muted: u64,
    queue: Vec<(u8, Vec<u8>)>,
}

impl Listener {
    pub(crate) fn mute(&mut self, talker: i32, muted: bool) {
        if (0..64).contains(&talker) {
            let bit = 1_u64 << talker;
            if muted {
                self.muted |= bit;
            } else {
                self.muted &= !bit;
            }
        }
    }

    pub(crate) fn enqueue(&mut self, talker: u8, frame: &[u8]) {
        if self.enabled && self.muted & (1_u64 << talker) == 0 && self.queue.len() < MAX_QUEUED {
            self.queue.push((talker, frame.to_vec()));
        }
    }

    /// SV_SendClientVoiceData sends all queued frames together, after a snapshot.
    pub(crate) fn take_packet(&mut self) -> Option<Vec<u8>> {
        if self.queue.is_empty() {
            return None;
        }
        let mut bytes = b"\xff\xff\xff\xffv\0".to_vec();
        bytes.push(self.queue.len() as u8);
        for (talker, frame) in self.queue.drain(..) {
            bytes.push(talker);
            // A pre-world input may contain 256 bytes. The relay length field
            // is a byte for both input paths, while the full payload is copied.
            bytes.push(frame.len() as u8);
            bytes.extend(frame);
        }
        Some(bytes)
    }
}

/// SV_ParseVoicePacketsLocal takes byte lengths. SV_ParseVoicePackets takes
/// signed shorts. Earlier frames are already queued if a later length fails.
pub(crate) fn read_frames(
    message: &mut MessageReader<'_>,
    active: bool,
    mut consume: impl FnMut(&[u8]),
) -> io::Result<()> {
    let Ok(count) = message.read_byte() else {
        return Ok(());
    };
    for _ in 0..count {
        let length = if active {
            message.read_byte().map(i32::from)
        } else {
            message.read_short().map(i32::from)
        }
        .unwrap_or(-1);
        if !(1..=256).contains(&length) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("invalid voice packet of size {length}"),
            ));
        }
        let frame = message.read_data(length as usize)?;
        consume(frame);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn active_talker_respects_team_session_echo_and_dead_chat() {
        let player = |team, session_state| {
            Some(VoicePlayer {
                team,
                session_state,
            })
        };
        let mut rules = VoiceRules::from_config(&ServerConfig::from_args(&[]));
        let talker = player(2, 0);
        assert!(rules.audible(talker, player(2, 0), false));
        assert!(!rules.audible(talker, player(1, 0), false));
        assert!(!rules.audible(talker, talker, true));
        rules.echo = true;
        assert!(rules.audible(talker, talker, true));
        rules.global = true;
        assert!(rules.audible(talker, player(1, 0), false));
        assert!(!rules.audible(talker, player(1, 1), false));
        rules.dead_chat = true;
        assert!(rules.audible(talker, player(1, 1), false));
        assert!(!rules.audible(talker, player(1, 2), false));
        assert!(rules.audible(None, None, false));
        assert!(!rules.audible(None, None, true));
    }

    #[test]
    fn relay_keeps_order_limits_queue_and_wraps_the_256_byte_length() -> io::Result<()> {
        let mut input = vec![2, 0, 1];
        input.extend(0..=255);
        input.extend([1, 0, 0x55]);
        let mut listener = Listener {
            enabled: true,
            ..Default::default()
        };
        listener.mute(5, true);
        listener.enqueue(5, b"muted");
        listener.mute(5, false);
        read_frames(&mut MessageReader::new(&input), false, |frame| {
            listener.enqueue(5, frame)
        })?;
        let mut expected = b"\xff\xff\xff\xffv\0\x02\x05\0".to_vec();
        expected.extend(0..=255);
        expected.extend([5, 1, 0x55]);
        assert_eq!(listener.take_packet(), Some(expected));
        assert_eq!(listener.take_packet(), None);
        for n in 0..50 {
            listener.enqueue(5, &[n]);
        }
        let frame = listener.take_packet().expect("queued voice frame");
        assert_eq!(frame[6], 40);
        assert_eq!(&frame[frame.len() - 3..], [5, 1, 39]);
        Ok(())
    }

    #[test]
    fn invalid_length_keeps_preceding_frames_and_discards_the_tail() {
        let mut frames = Vec::new();
        let mut packet = MessageReader::new(&[3, 2, 11, 12, 0, 1, 13]);
        assert!(read_frames(&mut packet, true, |frame| frames.push(frame.to_owned())).is_err());
        assert_eq!(frames, [vec![11, 12]]);
    }
}
