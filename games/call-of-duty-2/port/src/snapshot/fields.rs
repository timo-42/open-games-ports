//! Table-driven message field encodings shared by snapshot record types.
use crate::message::MessageWriter;
use std::io;

pub(super) type Field = (&'static str, usize, i8);

pub(super) fn named_offset(fields: &[Field], name: &str) -> Option<usize> {
    fields.iter().find(|f| f.0 == name).map(|f| f.1 / 4)
}

pub(super) fn change_count(fields: &[Field], old: &[u32], new: &[u32]) -> usize {
    fields
        .iter()
        .rposition(|&(_, offset, _)| old[offset / 4] != new[offset / 4])
        .map_or(0, |last| last + 1)
}

/// Original MSG_WriteAngle16 truncates the x87 product to a signed dword
/// before taking the low short; invalid float conversions give integer indefinite.
pub(crate) fn angle16(word: u32) -> i16 {
    let scaled = f64::from(f32::from_bits(word)) * f64::from(182.04445_f32);
    let truncated = scaled.trunc();
    if (f64::from(i32::MIN)..=f64::from(i32::MAX)).contains(&truncated) {
        truncated as i32 as i16
    } else {
        0
    }
}

fn write_compact_float(w: &mut MessageWriter, raw: u32, low_bits: u8) -> io::Result<()> {
    let value = f32::from_bits(raw);
    let bias = 1_i32 << (low_bits + 7);
    let compact = (-(bias as f32)..bias as f32).contains(&value) && value.trunc() == value;
    w.write_bit(!compact)?;
    if compact {
        let encoded = (value as i32 + bias) as u32;
        w.write_bits(encoded, low_bits)?;
        w.write_byte((encoded >> low_bits) as u8)
    } else {
        w.write_long(raw as i32)
    }
}

fn write_integer(w: &mut MessageWriter, word: u32, width: u8) -> io::Result<()> {
    let partial = width % 8;
    if partial != 0 {
        w.write_bits(word, partial)?;
    }
    for shift in (partial..width).step_by(8) {
        w.write_byte((word >> shift) as u8)?;
    }
    Ok(())
}

pub(crate) fn write_struct_field(
    w: &mut MessageWriter,
    old: u32,
    new: u32,
    bits: i8,
) -> io::Result<()> {
    w.write_bit(old != new)?;
    if old == new {
        return Ok(());
    }
    let nonzero = match bits {
        0 | -99 => f32::from_bits(new) != 0.0,
        _ => new != 0,
    };
    w.write_bit(nonzero)?;
    if !nonzero {
        return Ok(());
    }
    match bits {
        0 => write_compact_float(w, new, 5),
        -99 => write_compact_float(w, new, 2),
        -100 => w.write_short(angle16(new)),
        _ => write_integer(w, new, bits.unsigned_abs()),
    }
}

pub(super) fn write_player_field(
    w: &mut MessageWriter,
    old: u32,
    new: u32,
    bits: i8,
) -> io::Result<()> {
    if bits == -100 {
        return write_struct_field(w, old, new, bits);
    }
    w.write_bit(old != new)?;
    if old == new {
        return Ok(());
    }
    if bits == 0 {
        write_compact_float(w, new, 5)
    } else {
        write_integer(w, new, bits.unsigned_abs())
    }
}

pub(super) fn write_fields(
    w: &mut MessageWriter,
    fields: &[Field],
    old: &[u32],
    new: &[u32],
) -> io::Result<()> {
    for &(_, offset, bits) in fields {
        write_struct_field(w, old[offset / 4], new[offset / 4], bits)?;
    }
    Ok(())
}
