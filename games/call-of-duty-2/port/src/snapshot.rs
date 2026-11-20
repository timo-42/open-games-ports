//! Protocol 118 SV_WriteSnapshotToClient, with per-client acknowledged frame history.
use crate::entity_delta::{self, EntityState};
use crate::message::MessageWriter;
use std::cmp::Ordering;
use std::io;

pub mod archive;
pub(crate) mod fields;
mod player;
mod tables;
pub mod world;
pub use player::PlayerState;
use tables::CLIENT_FIELDS;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ClientState {
    words: [u32; 0x5c / 4],
}

impl ClientState {
    pub fn new(number: u8) -> Self {
        let mut words = [0; 0x5c / 4];
        words[0] = u32::from(number);
        Self { words }
    }

    pub fn from_words(words: [u32; 0x5c / 4]) -> Self {
        Self { words }
    }
    pub fn number(&self) -> u8 {
        self.words[0] as u8
    }

    pub fn field(&self, name: &str) -> Option<u32> {
        fields::named_offset(CLIENT_FIELDS, name).map(|index| self.words[index])
    }

    pub fn set_field(&mut self, name: &str, word: u32) -> io::Result<()> {
        let index = fields::named_offset(CLIENT_FIELDS, name).ok_or_else(|| {
            io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("unknown client field {name}"),
            )
        })?;
        self.words[index] = word;
        Ok(())
    }

    pub fn set_name(&mut self, name: &[u8]) {
        let mut bytes = [0u8; 32];
        let count = name
            .iter()
            .position(|&c| c == 0)
            .unwrap_or(name.len())
            .min(31);
        bytes[..count].copy_from_slice(&name[..count]);
        for (word, chunk) in self.words[0x3c / 4..]
            .iter_mut()
            .zip(bytes.as_chunks::<4>().0)
        {
            *word = u32::from_le_bytes(*chunk);
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Snapshot {
    pub server_time: i32,
    pub flags: u8,
    pub player: PlayerState,
    /// Entities visible to this client, ordered by entity number.
    pub entities: Vec<EntityState>,
    /// Clients in this snapshot, ordered by slot number.
    pub clients: Vec<ClientState>,
}

fn write_client(
    w: &mut MessageWriter,
    old: Option<&ClientState>,
    new: Option<&ClientState>,
    force: bool,
) -> io::Result<()> {
    let Some(new) = new else {
        if let Some(old) = old {
            w.write_bit(true)?;
            w.write_bits(old.words[0], 6)?;
            w.write_bit(true)?;
        }
        return Ok(());
    };
    let old_words = old.map_or(&[0; 0x5c / 4], |state| &state.words);
    let changed = fields::change_count(CLIENT_FIELDS, old_words, &new.words);
    if changed == 0 && !force {
        return Ok(());
    }
    w.write_bit(true)?;
    w.write_bits(new.words[0], 6)?;
    w.write_bit(false)?;
    w.write_bit(changed != 0)?;
    if changed != 0 {
        w.write_byte(changed as u8)?;
        fields::write_fields(w, &CLIENT_FIELDS[..changed], old_words, &new.words)?;
    }
    Ok(())
}

fn ordered<T, K: Ord>(values: &[T], key: impl Fn(&T) -> K, upper: K) -> bool {
    values.last().is_none_or(|last| key(last) < upper)
        && values.windows(2).all(|pair| key(&pair[0]) < key(&pair[1]))
}

fn merge<T, K: Ord>(
    old: &[T],
    new: &[T],
    key: impl Fn(&T) -> K,
    mut emit: impl FnMut(Option<&T>, Option<&T>) -> io::Result<()>,
) -> io::Result<()> {
    let (mut a, mut b) = (old.iter().peekable(), new.iter().peekable());
    while a.peek().is_some() || b.peek().is_some() {
        match (a.peek(), b.peek()) {
            (Some(prev), Some(next)) => match key(prev).cmp(&key(next)) {
                Ordering::Less => emit(a.next(), None)?,
                Ordering::Equal => emit(a.next(), b.next())?,
                Ordering::Greater => emit(None, b.next())?,
            },
            (Some(_), None) => emit(a.next(), None)?,
            (None, Some(_)) => emit(None, b.next())?,
            (None, None) => break,
        }
    }
    Ok(())
}

/// Per-client acknowledgement window. The server checks the shared entity-ring
/// position before passing an acknowledged frame to the delta writer.
#[derive(Clone, Debug, Default)]
pub struct SnapshotHistory {
    frames: [Option<(i32, Snapshot)>; 32],
}

impl SnapshotHistory {
    pub fn ring_player(&self, sequence: i32) -> Option<&PlayerState> {
        self.frames[sequence as usize & 31]
            .as_ref()
            .map(|(_, frame)| &frame.player)
    }

    pub fn record(&mut self, sequence: i32, frame: Snapshot) {
        self.frames[sequence as usize & 31] = Some((sequence, frame));
    }

    fn base(&self, sequence: i32, delta_message: Option<i32>) -> Option<(u8, &Snapshot)> {
        let ack = delta_message.filter(|&ack| ack > 0)?;
        let distance = sequence.checked_sub(ack)?;
        if !(1..29).contains(&distance) {
            return None;
        }
        let (stored, frame) = self.frames[ack as usize & 31].as_ref()?;
        (*stored == ack).then_some((distance as u8, frame))
    }

    /// Writes svc_snapshot, including entity and client list terminators.
    /// The enclosing server message supplies any packet padding and svc_EOF.
    pub fn write(
        &self,
        writer: &mut MessageWriter,
        sequence: i32,
        delta_message: Option<i32>,
        frame: &Snapshot,
        baselines: &[EntityState],
    ) -> io::Result<()> {
        let base = self.base(sequence, delta_message);
        if !ordered(&frame.entities, |e| e.words()[0], 1023)
            || !ordered(&frame.clients, |c| c.words[0], 64)
            || !ordered(baselines, |e| e.words()[0], 1023)
        {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "snapshot records must have unique, ascending protocol indices",
            ));
        }
        writer.write_byte(6)?;
        writer.write_long(frame.server_time)?;
        writer.write_byte(base.map_or(0, |(distance, _)| distance))?;
        writer.write_byte(frame.flags)?;
        let old = base.map(|(_, frame)| frame);
        player::write_delta(writer, old.map(|f| &f.player), &frame.player)?;
        merge(
            old.map_or(&[], |f| f.entities.as_slice()),
            &frame.entities,
            EntityState::number,
            |old, new| {
                if let Some(old) = old {
                    return entity_delta::write_delta(writer, old, new, false);
                }
                if let Some(new) = new {
                    let baseline = baselines
                        .binary_search_by_key(&new.number(), EntityState::number)
                        .ok()
                        .map(|index| &baselines[index]);
                    entity_delta::write_delta(
                        writer,
                        baseline.unwrap_or(&EntityState::new(0)),
                        Some(new),
                        true,
                    )?;
                }
                Ok(())
            },
        )?;
        writer.write_bits(1023, 10)?;
        merge(
            old.map_or(&[], |f| f.clients.as_slice()),
            &frame.clients,
            ClientState::number,
            |old, new| write_client(writer, old, new, old.is_none()),
        )?;
        writer.write_bit(false)
    }
}

#[cfg(test)]
mod tests;
