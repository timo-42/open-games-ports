//! MSG_WriteDeltaPlayerState including stats, inventory, objectives and both HUD arrays.
use super::fields::{change_count, named_offset, write_fields, write_player_field};
use super::tables::{HUDELEM_FIELDS, OBJECTIVE_FIELDS, PLAYER_FIELDS};
use crate::message::MessageWriter;
use std::io;

const PLAYER_WORDS: usize = 0x26a8 / 4;

/// Raw IEEE, angle and signed network fields retain their exact dword change semantics.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PlayerState {
    words: Box<[u32; PLAYER_WORDS]>,
}

impl Default for PlayerState {
    fn default() -> Self {
        Self {
            words: Box::new([0; PLAYER_WORDS]),
        }
    }
}

impl PlayerState {
    /// Append an event to the native four-entry player-state ring.
    pub(crate) fn add_event(&mut self, code: i32, param: i32) {
        let words = self.words_mut();
        let sequence = words[0xa4 / 4];
        let slot = (sequence & 3) as usize;
        words[0xa8 / 4 + slot] = code as u32;
        words[0xb8 / 4 + slot] = param as u32;
        words[0xa4 / 4] = sequence.wrapping_add(1);
    }

    pub fn words_mut(&mut self) -> &mut [u32; PLAYER_WORDS] {
        &mut self.words
    }
    pub fn words(&self) -> &[u32; PLAYER_WORDS] {
        &self.words
    }

    pub fn field(&self, name: &str) -> Option<u32> {
        named_offset(PLAYER_FIELDS, name).map(|index| self.words[index])
    }

    pub fn set_field(&mut self, name: &str, word: u32) -> io::Result<()> {
        let index = named_offset(PLAYER_FIELDS, name).ok_or_else(|| {
            io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("unknown player field {name}"),
            )
        })?;
        self.words[index] = word;
        Ok(())
    }
}

fn mask(old: &[u32], new: &[u32]) -> u16 {
    old.iter()
        .zip(new)
        .enumerate()
        .fold(0, |mask, (i, (a, b))| mask | (u16::from(a != b) << i))
}

fn write_stats(w: &mut MessageWriter, old: &[u32], new: &[u32]) -> io::Result<()> {
    let changed = mask(old, new);
    w.write_bit(changed != 0)?;
    if changed == 0 {
        return Ok(());
    }
    w.write_bits(u32::from(changed), 6)?;
    for (i, &word) in new.iter().enumerate() {
        if changed & (1 << i) != 0 {
            match i {
                3 => w.write_bits(word, 6)?,
                5 => w.write_byte(word as u8)?,
                _ => w.write_short(word as i16)?,
            }
        }
    }
    Ok(())
}

fn write_inventory(
    w: &mut MessageWriter,
    old: &[u32],
    new: &[u32],
    outer_flag: bool,
) -> io::Result<()> {
    if outer_flag {
        w.write_bit(old != new)?;
        if old == new {
            return Ok(());
        }
    }
    for (old, new) in old
        .as_chunks::<16>()
        .0
        .iter()
        .zip(new.as_chunks::<16>().0.iter())
    {
        let changed = mask(old, new);
        w.write_bit(changed != 0)?;
        if changed != 0 {
            w.write_short(changed as i16)?;
            for (i, &word) in new.iter().enumerate() {
                if changed & (1 << i) != 0 {
                    w.write_short(word as i16)?;
                }
            }
        }
    }
    Ok(())
}

fn write_objectives(w: &mut MessageWriter, old: &[u32], new: &[u32]) -> io::Result<()> {
    w.write_bit(old != new)?;
    if old != new {
        for (old, new) in old
            .as_chunks::<7>()
            .0
            .iter()
            .zip(new.as_chunks::<7>().0.iter())
        {
            w.write_bits(new[0], 3)?;
            let changed = old[1..] != new[1..];
            w.write_bit(changed)?;
            if changed {
                write_fields(w, OBJECTIVE_FIELDS, old, new)?;
            }
        }
    }
    Ok(())
}

fn write_hud_array(w: &mut MessageWriter, old: &[u32], new: &[u32]) -> io::Result<()> {
    let count = new
        .as_chunks::<32>()
        .0
        .iter()
        .take_while(|element| element[0] != 0)
        .count();
    w.write_bits(count as u32, 5)?;
    for (old, new) in old
        .as_chunks::<32>()
        .0
        .iter()
        .zip(new.as_chunks::<32>().0.iter())
        .take(count)
    {
        let last = change_count(HUDELEM_FIELDS, old, new).saturating_sub(1);
        w.write_bits(last as u32, 5)?;
        write_fields(w, &HUDELEM_FIELDS[..=last], old, new)?;
    }
    Ok(())
}

pub(super) fn write_delta(
    w: &mut MessageWriter,
    old: Option<&PlayerState>,
    new: &PlayerState,
) -> io::Result<()> {
    static ZERO: [u32; PLAYER_WORDS] = [0; PLAYER_WORDS];
    let old = old.map_or(&ZERO, PlayerState::words);
    let new = new.words();
    let count = change_count(PLAYER_FIELDS, old, new);
    w.write_byte(count as u8)?;
    for &(_, offset, bits) in &PLAYER_FIELDS[..count] {
        write_player_field(w, old[offset / 4], new[offset / 4], bits)?;
    }
    let stats = 0x12c / 4..0x144 / 4;
    write_stats(w, &old[stats.clone()], &new[stats])?;
    for (offset, outer_flag) in [(0x144 / 4, true), (0x344 / 4, false)] {
        write_inventory(
            w,
            &old[offset..offset + 64],
            &new[offset..offset + 64],
            outer_flag,
        )?;
    }
    let objectives = 0x5e4 / 4..0x7a4 / 4;
    write_objectives(w, &old[objectives.clone()], &new[objectives])?;
    let hud_start = 0x7a8 / 4;
    let hud_changed = old[hud_start..] != new[hud_start..];
    w.write_bit(hud_changed)?;
    if hud_changed {
        let split = 0x1728 / 4;
        write_hud_array(w, &old[split..], &new[split..])?;
        write_hud_array(w, &old[hud_start..split], &new[hud_start..split])?;
    }
    Ok(())
}
