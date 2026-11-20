//! Protocol 118 initial game state, before Huffman compression and netchannel encoding.
use crate::entity_delta::{EntityState, write_baseline};
use crate::level::ConfigStrings;
use crate::message::MessageWriter;
use crate::netchan::MAX_MESSAGE;
use std::io;

/// SV_SendClientGameState walks both tables in slot order (chunk_020, 0x0808f302).
#[derive(Debug)]
pub struct GameState<'a> {
    pub configstrings: &'a ConfigStrings,
    pub baselines: &'a [EntityState],
}

impl GameState<'_> {
    /// The pending tail of the 128-entry reliable ring is in send order.
    /// Its final command has sequence `reliable_sequence`, even if earlier
    /// commands have already been acknowledged by this client.
    pub fn serialize(
        &self,
        last_client_command: i32,
        reliable_sequence: i32,
        pending_commands: &[Vec<u8>],
        client_slot: i32,
        checksum_feed: i32,
    ) -> io::Result<Vec<u8>> {
        if pending_commands.len() > 128 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "reliable command ring overflow",
            ));
        }
        let mut msg = MessageWriter::new(MAX_MESSAGE);
        msg.write_long(last_client_command)?;

        // SV_UpdateServerCommandsToClient, chunk_022, 0x080988b2.
        let mut sequence = reliable_sequence.wrapping_sub(pending_commands.len() as i32);
        for text in pending_commands {
            sequence = sequence.wrapping_add(1);
            msg.write_byte(4)?;
            msg.write_long(sequence)?;
            msg.write_string(text)?;
        }
        msg.write_byte(1)?;
        msg.write_long(reliable_sequence)?;
        for (index, text) in self.configstrings.populated() {
            msg.write_byte(2)?;
            msg.write_short(index as i16)?;
            msg.write_big_string(text)?;
        }
        if self
            .baselines
            .windows(2)
            .any(|pair| pair[0].number() >= pair[1].number())
            || self
                .baselines
                .last()
                .is_some_and(|state| state.number() >= 1023)
        {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "baselines must be in entity number order",
            ));
        }
        for baseline in self.baselines {
            write_baseline(&mut msg, baseline)?;
        }
        msg.write_byte(7)?; // end of gamestate records
        msg.write_long(client_slot)?;
        msg.write_long(checksum_feed)?;
        msg.write_byte(7)?; // end of server message
        Ok(msg.into_bytes())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reliable_tail_precedes_ordered_configstrings_and_forced_baselines() -> io::Result<()> {
        let mut strings = ConfigStrings::default();
        strings.set(1000, b"s")?;
        strings.set(5, b"a\x92b")?;
        let baselines = [EntityState::new(0), EntityState::new(4)];
        let game = GameState {
            configstrings: &strings,
            baselines: &baselines,
        };
        let message = game.serialize(-2, 11, &[b"a".to_vec(), b"bb".to_vec()], 3, -5)?;
        assert_eq!(
            message,
            [
                254, 255, 255, 255, 4, 10, 0, 0, 0, b'a', 0, 4, 11, 0, 0, 0, b'b', b'b', 0, 1, 11,
                0, 0, 0, 2, 5, 0, b'a', b'\'', b'b', 0, 2, 232, 3, b's', 0, 3, 4, 0, 7, 3, 0, 0, 0,
                251, 255, 255, 255, 7,
            ]
        );
        Ok(())
    }

    #[test]
    fn rejects_overflow_without_emitting_partial_state() -> io::Result<()> {
        let mut strings = ConfigStrings::default();
        for slot in 0..17 {
            strings.set(slot, &[b'x'; 8191])?;
        }
        let game = GameState {
            configstrings: &strings,
            baselines: &[],
        };
        assert!(game.serialize(0, 0, &[], 0, 0).is_err());
        assert!(
            game.serialize(0, 129, &vec![Vec::new(); 129], 0, 0)
                .is_err()
        );
        Ok(())
    }
}
