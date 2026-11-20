//! Protocol 118 entityState_t deltas and gamestate baselines (MSG_WriteDeltaEntity).
use crate::message::MessageWriter;
use crate::snapshot::fields::write_struct_field as write_field;
use std::io;

// The 59 (name, byte offset, encoding) entries at 0x081417a0 in cod2_lnxded
// 1.3, in wire order. A raw u32 preserves IEEE payloads and signed zero for
// the original instantaneous change test.
const FIELDS: [(&str, usize, i8); 59] = [
    ("pos.trTime", 0x10, 32),
    ("pos.trBase[1]", 0x1c, 0),
    ("pos.trBase[0]", 0x18, 0),
    ("pos.trDelta[0]", 0x24, 0),
    ("pos.trDelta[1]", 0x28, 0),
    ("angles2[1]", 0x6c, 0),
    ("apos.trBase[1]", 0x40, -100),
    ("pos.trDelta[2]", 0x2c, 0),
    ("pos.trBase[2]", 0x20, 0),
    ("apos.trBase[0]", 0x3c, -100),
    ("eventSequence", 0xa4, 8),
    ("legsAnim", 0xcc, 10),
    ("eType", 0x04, 8),
    ("eFlags", 0x08, 24),
    ("otherEntityNum", 0x74, 10),
    ("surfType", 0x88, 8),
    ("eventParm", 0xa0, 8),
    ("scale", 0xd8, 8),
    ("clientNum", 0x90, 8),
    ("torsoAnim", 0xd0, 10),
    ("groundEntityNum", 0x7c, 10),
    ("events[0]", 0xa8, 8),
    ("events[1]", 0xac, 8),
    ("events[2]", 0xb0, 8),
    ("angles2[0]", 0x68, 0),
    ("events[3]", 0xb4, 8),
    ("apos.trBase[2]", 0x44, -100),
    ("pos.trType", 0x0c, 8),
    ("fWaistPitch", 0xec, 0),
    ("fTorsoPitch", 0xe8, 0),
    ("apos.trTime", 0x34, 32),
    ("solid", 0x9c, 24),
    ("apos.trDelta[0]", 0x48, 0),
    ("apos.trType", 0x30, 8),
    ("animMovetype", 0xe0, 4),
    ("fTorsoHeight", 0xe4, 0),
    ("apos.trDelta[2]", 0x50, 0),
    ("weapon", 0xc8, 7),
    ("index", 0x8c, 10),
    ("apos.trDelta[1]", 0x4c, 0),
    ("eventParms[0]", 0xb8, 8),
    ("eventParms[1]", 0xbc, 8),
    ("eventParms[2]", 0xc0, 8),
    ("eventParms[3]", 0xc4, 8),
    ("iHeadIcon", 0x94, 4),
    ("pos.trDuration", 0x14, 32),
    ("iHeadIconTeam", 0x98, 2),
    ("time", 0x54, 32),
    ("leanf", 0xd4, 0),
    ("attackerEntityNum", 0x78, 10),
    ("time2", 0x58, 32),
    ("loopSound", 0x84, 8),
    ("origin2[2]", 0x64, 0),
    ("origin2[0]", 0x5c, 0),
    ("origin2[1]", 0x60, 0),
    ("angles2[2]", 0x70, 0),
    ("constantLight", 0x80, 32),
    ("apos.trDuration", 0x38, 32),
    ("dmgFlags", 0xdc, 32),
];

/// The 0xf0-byte entityState_t layout. The first dword is the entity number.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct EntityState {
    words: [u32; 60],
}

impl EntityState {
    pub fn new(number: u16) -> Self {
        let mut state = Self { words: [0; 60] };
        state.words[0] = u32::from(number);
        state
    }

    pub(crate) fn words(&self) -> &[u32; 60] {
        &self.words
    }

    pub fn from_words(words: [u32; 60]) -> Self {
        Self { words }
    }

    pub fn number(&self) -> u16 {
        self.words[0] as u16
    }

    pub fn field(&self, name: &str) -> Option<u32> {
        FIELDS
            .iter()
            .find(|field| field.0 == name)
            .map(|field| self.words[field.1 / 4])
    }

    pub fn set_field(&mut self, name: &str, raw_word: u32) -> bool {
        if let Some(&(_, offset, _)) = FIELDS.iter().find(|field| field.0 == name) {
            self.words[offset / 4] = raw_word;
            true
        } else {
            false
        }
    }
}

/// MSG_WriteDeltaStruct removal, forced unchanged, and changed entity paths.
pub fn write_delta(
    writer: &mut MessageWriter,
    from: &EntityState,
    to: Option<&EntityState>,
    force: bool,
) -> io::Result<()> {
    let Some(to) = to else {
        writer.write_bits(u32::from(from.number()), 10)?;
        return writer.write_bit(true);
    };
    let count = FIELDS
        .iter()
        .rposition(|&(_, offset, _)| from.words[offset / 4] != to.words[offset / 4])
        .map_or(0, |last| last + 1);
    if count == 0 && !force {
        return Ok(());
    }
    writer.write_bits(u32::from(to.number()), 10)?;
    writer.write_bit(false)?;
    writer.write_bit(count != 0)?;
    if count != 0 {
        writer.write_byte(count as u8)?;
        for &(_, offset, encoding) in &FIELDS[..count] {
            write_field(
                writer,
                from.words[offset / 4],
                to.words[offset / 4],
                encoding,
            )?;
        }
    }
    Ok(())
}

/// SV_SendClientGameState omits entity zero, then writes svc_baseline and
/// a forced delta against a completely zeroed entityState_t.
pub fn write_baseline(writer: &mut MessageWriter, baseline: &EntityState) -> io::Result<()> {
    if baseline.number() == 0 {
        return Ok(());
    }
    writer.write_byte(3)?;
    write_delta(writer, &EntityState::new(0), Some(baseline), true)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::message::MessageReader;
    use crate::snapshot::fields::angle16;

    #[test]
    fn omit_remove_and_force() {
        let mut out = MessageWriter::new(64);
        let state = EntityState::new(741);
        write_baseline(&mut out, &EntityState::new(0)).expect("entity zero");
        write_delta(&mut out, &state, Some(&state), false).expect("unchanged");
        assert!(out.as_bytes().is_empty());
        write_delta(&mut out, &state, None, false).expect("removal");
        write_baseline(&mut out, &state).expect("forced");
        let mut m = MessageReader::new(out.as_bytes());
        assert_eq!(m.read_bits(10).expect("number"), 741);
        assert!(m.read_bit().expect("removed"));
        assert_eq!(m.read_byte().expect("baseline op"), 3);
        assert_eq!(m.read_bits(10).expect("number"), 741);
        assert!(!m.read_bit().expect("retained"));
        assert!(!m.read_bit().expect("unchanged"));
    }

    #[test]
    fn final_field_keeps_previous_unmodified_words_and_all_32_bits() -> io::Result<()> {
        let mut before = EntityState::new(17);
        assert!(before.set_field("pos.trTime", 654));
        let mut after = before.clone();
        assert!(after.set_field("dmgFlags", 0x8000_0001));
        let mut out = MessageWriter::new(64);
        write_delta(&mut out, &before, Some(&after), false)?;
        let mut msg = MessageReader::new(out.as_bytes());
        assert_eq!(msg.read_bits(10)?, 17);
        assert!(!msg.read_bit()?);
        assert!(msg.read_bit()?);
        assert_eq!(msg.read_byte()?, 59);
        for _ in 0..58 {
            assert!(!msg.read_bit()?);
        }
        assert!(msg.read_bit()?);
        assert!(msg.read_bit()?);
        assert_eq!(msg.read_long()? as u32, 0x8000_0001);
        Ok(())
    }

    #[test]
    fn scaled_float_boundaries_and_angle_words() -> io::Result<()> {
        for value in [
            -4097.0_f32,
            -4096.0,
            -1.0,
            -0.0,
            0.5,
            4095.0,
            4096.0,
            f32::INFINITY,
        ] {
            let mut out = MessageWriter::new(64);
            write_field(&mut out, 0, value.to_bits(), 0)?;
            let mut m = MessageReader::new(out.as_bytes());
            assert!(m.read_bit()?);
            if value == 0.0 {
                assert!(!m.read_bit()?);
            } else {
                assert!(m.read_bit()?);
                if m.read_bit()? {
                    assert!(!(-4096.0..=4095.0).contains(&value) || value != value.trunc());
                    assert_eq!(m.read_long()? as u32, value.to_bits());
                } else {
                    let lo = m.read_bits(5)?;
                    let hi = u32::from(m.read_byte()?);
                    assert_eq!(((hi << 5 | lo) as i32 - 4096) as f32, value);
                }
            }
        }
        assert_eq!(angle16((-90.0_f32).to_bits()), -16384);
        assert_eq!(angle16(360.0_f32.to_bits()), 0);
        assert_eq!(angle16(f32::NAN.to_bits()), 0);
        Ok(())
    }
}
