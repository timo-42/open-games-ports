//! Stock archive replay reads the same deltas it uses to write the cache.
//! Decoded frames consequently contain quantized network fields.
use super::super::{ClientState, PlayerState, fields::Field, tables::*};
use crate::message::MessageReader;
use std::io;

fn invalid() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, "invalid server archive delta")
}

fn integer(m: &mut MessageReader<'_>, bits: i8) -> io::Result<u32> {
    let width = bits.unsigned_abs();
    let partial = width % 8;
    let mut value = m.read_bits(partial)?;
    for shift in (partial..width).step_by(8) {
        value |= u32::from(m.read_byte()?) << shift;
    }
    if bits < 0 {
        value = (((value << (32 - width)) as i32) >> (32 - width)) as u32;
    }
    Ok(value)
}

fn read_fields(
    m: &mut MessageReader<'_>,
    fields: &[Field],
    words: &mut [u32],
    player: bool,
) -> io::Result<()> {
    for &(_, offset, bits) in fields {
        if !m.read_bit()? {
            continue;
        }
        words[offset / 4] = if (!player || bits == -100) && !m.read_bit()? {
            0
        } else {
            match bits {
                0 | -99 => {
                    if m.read_bit()? {
                        m.read_long()? as u32
                    } else {
                        let low = if bits == 0 { 5 } else { 2 };
                        let biased = m.read_bits(low)? | (u32::from(m.read_byte()?) << low);
                        ((biased as i32 - (1 << (low + 7))) as f32).to_bits()
                    }
                }
                -100 => (f32::from(m.read_short()?) * (360. / 65536.)).to_bits(),
                _ => integer(m, bits)?,
            }
        };
    }
    Ok(())
}

pub(super) fn record(
    m: &mut MessageReader<'_>,
    fields: &[Field],
    words: &mut [u32],
    number: u32,
) -> io::Result<()> {
    if m.read_bit()? {
        return Err(invalid());
    }
    if m.read_bit()? {
        words[0] = number;
        let count = usize::from(m.read_byte()?);
        read_fields(m, fields.get(..count).ok_or_else(invalid)?, words, false)?;
    }
    Ok(())
}

pub(super) fn client(
    m: &mut MessageReader<'_>,
    old: Option<&ClientState>,
    number: u8,
) -> io::Result<ClientState> {
    let mut new = old.cloned().unwrap_or_else(|| ClientState::new(0));
    record(m, CLIENT_FIELDS, &mut new.words, u32::from(number))?;
    Ok(new)
}

fn inventory(m: &mut MessageReader<'_>, words: &mut [u32]) -> io::Result<()> {
    for group in words.as_chunks_mut::<16>().0 {
        if m.read_bit()? {
            let mask = m.read_short()? as u16;
            for (i, w) in group.iter_mut().enumerate() {
                if mask & (1 << i) != 0 {
                    *w = i32::from(m.read_short()?) as u32;
                }
            }
        }
    }
    Ok(())
}

pub(super) fn player(
    m: &mut MessageReader<'_>,
    old: Option<&PlayerState>,
) -> io::Result<PlayerState> {
    let mut new = old.cloned().unwrap_or_default();
    let words = new.words_mut();
    let count = usize::from(m.read_byte()?);
    read_fields(
        m,
        PLAYER_FIELDS.get(..count).ok_or_else(invalid)?,
        words,
        true,
    )?;
    if m.read_bit()? {
        let mask = m.read_bits(6)?;
        for i in 0..6 {
            if mask & (1 << i) != 0 {
                words[0x12c / 4 + i] = match i {
                    3 => m.read_bits(6)?,
                    5 => u32::from(m.read_byte()?),
                    _ => i32::from(m.read_short()?) as u32,
                };
            }
        }
    }
    if m.read_bit()? {
        inventory(m, &mut words[0x144 / 4..0x144 / 4 + 64])?;
    }
    inventory(m, &mut words[0x344 / 4..0x344 / 4 + 64])?;
    if m.read_bit()? {
        for obj in words[0x5e4 / 4..0x7a4 / 4].as_chunks_mut::<7>().0 {
            obj[0] = m.read_bits(3)?;
            if m.read_bit()? {
                read_fields(m, OBJECTIVE_FIELDS, obj, false)?;
            }
        }
    }
    if m.read_bit()? {
        for off in [0x1728 / 4, 0x7a8 / 4] {
            let n = m.read_bits(5)? as usize;
            let elements = &mut words[off..off + 31 * 32];
            for element in elements[..n * 32].as_chunks_mut::<32>().0 {
                let last = m.read_bits(5)? as usize;
                read_fields(m, &HUDELEM_FIELDS[..=last], element, false)?;
            }
            elements[n * 32..].fill(0);
        }
    }
    Ok(new)
}
