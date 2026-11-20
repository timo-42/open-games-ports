//! Protocol 118 client movement commands (CL_CreateCmd, MSG_ReadDeltaUsercmdKey,
//! SV_UserMove). Each packet starts from the server's current player state.
use crate::message::MessageReader;
use std::io;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct UserCmd {
    pub server_time: i32,
    pub buttons: u32,
    pub weapon: u8,
    pub offhand: u8,
    pub angles: [u16; 3],
    pub forward_move: i8,
    pub right_move: i8,
}

#[derive(Clone, Debug, Default)]
pub struct PlayerInputState {
    pub weapon: u8,
    pub offhand: u8,
    pub view_angles: [f32; 3],
    pub delta_angles: [i32; 3],
    pub pm_flags: u32,
    pub entity_flags: u32,
    pub lean: f32,
    pub weapon_position: f32,
    /// Cached BG_PlayerHasWeapon results (valid index, ownership bit, and
    /// offhand class, equipped slot, or alternate weapon qualification).
    pub selectable_weapons: [u32; 4],
}

impl PlayerInputState {
    pub fn baseline(&self) -> UserCmd {
        let mut cmd = UserCmd {
            weapon: self.weapon,
            offhand: self.offhand,
            ..Default::default()
        };
        for axis in 0..2 {
            // The i386 server multiplies two single-precision operands in x87
            // registers, then truncates to i32 before masking to 16 bits.
            let scaled = (f64::from(self.view_angles[axis]) * f64::from(182.044_45_f32)).trunc();
            let quantized = if (f64::from(i32::MIN)..=f64::from(i32::MAX)).contains(&scaled) {
                scaled as i32
            } else {
                i32::MIN
            };
            cmd.angles[axis] = quantized.wrapping_sub(self.delta_angles[axis]) as u16;
        }
        if self.pm_flags & 0x80_0000 != 0 {
            if self.entity_flags & 8 != 0 {
                cmd.buttons |= 0x100;
            } else if self.entity_flags & 4 != 0 {
                cmd.buttons |= 0x200;
            }
            if self.lean > 0. {
                cmd.buttons |= 0x80;
            } else if self.lean < 0. {
                cmd.buttons |= 0x40;
            }
            if self.weapon_position != 0. {
                cmd.buttons |= 0x1000;
            }
        }
        cmd
    }

    fn can_select(&self, index: u8) -> bool {
        self.selectable_weapons
            .get(usize::from(index / 32))
            .is_some_and(|mask| mask & (1 << (index % 32)) != 0)
    }
}

/// Com_HashKey hashes at most 32 signed characters of the acknowledged
/// reliable server command, stopping at its first NUL.
pub fn command_key(checksum_feed: i32, message_ack: i32, reliable_command: &[u8]) -> u32 {
    let mut hash = 0i32;
    for (i, &byte) in reliable_command
        .iter()
        .take(32)
        .take_while(|&&b| b != 0)
        .enumerate()
    {
        hash += (i as i32 + 119) * i32::from(byte as i8);
    }
    (checksum_feed ^ message_ack ^ hash ^ (hash >> 10) ^ (hash >> 20)) as u32
}

fn keyed(reader: &mut MessageReader<'_>, key: u32, width: u8) -> io::Result<u32> {
    Ok(reader.read_bits(width)? ^ (key & ((1 << width) - 1)))
}

fn delta(reader: &mut MessageReader<'_>, key: u32, old: u32, width: u8) -> io::Result<u32> {
    if reader.read_bit()? {
        keyed(reader, key, width)
    } else {
        Ok(old)
    }
}

fn angle(reader: &mut MessageReader<'_>, key: u32, old: u16) -> io::Result<u16> {
    if reader.read_bit()? {
        Ok(reader.read_short()? as u16 ^ key as u16)
    } else {
        Ok(old)
    }
}

fn movement_flags(value: i8) -> u32 {
    if value > 10 {
        1
    } else if value < -10 {
        2
    } else {
        0
    }
}

fn movement_axis(flags: u32) -> i8 {
    if flags & 1 != 0 {
        127
    } else if flags & 2 != 0 {
        -127
    } else {
        0
    }
}

pub fn read_delta(
    reader: &mut MessageReader<'_>,
    key: u32,
    previous: UserCmd,
) -> io::Result<UserCmd> {
    let mut cmd = previous;
    cmd.server_time = if reader.read_bit()? {
        previous
            .server_time
            .wrapping_add(i32::from(reader.read_byte()?))
    } else {
        reader.read_long()?
    };
    if keyed(reader, key, 1)? == 0 {
        return Ok(cmd);
    }
    let full = keyed(reader, key, 1)? != 0;
    let time_key = key ^ cmd.server_time as u32;
    let movement_key = if full { key } else { time_key };
    cmd.buttons = (cmd.buttons & !1) | keyed(reader, movement_key, 1)?;
    cmd.angles[0] = angle(reader, movement_key, previous.angles[0])?;
    cmd.angles[1] = angle(reader, movement_key, previous.angles[1])?;
    let old = movement_flags(previous.forward_move) | (movement_flags(previous.right_move) << 2);
    let movement = delta(reader, movement_key, old, 4)?;
    cmd.forward_move = movement_axis(movement);
    cmd.right_move = movement_axis(movement >> 2);
    if full {
        cmd.angles[2] = angle(reader, time_key, previous.angles[2])?;
        cmd.buttons =
            (cmd.buttons & 1) | (delta(reader, time_key, previous.buttons >> 1, 18)? << 1);
        cmd.weapon = delta(reader, time_key, u32::from(previous.weapon), 7)? as u8;
        cmd.offhand = delta(reader, time_key, u32::from(previous.offhand), 7)? as u8;
    }
    Ok(cmd)
}

/// Called after the 3-bit move/moveNoDelta opcode. The inventory-constrained
/// result is the base for the *next command in this packet*.
pub fn read_moves(
    reader: &mut MessageReader<'_>,
    checksum_feed: i32,
    message_ack: i32,
    reliable_command: &[u8],
    state: &PlayerInputState,
) -> io::Result<Vec<UserCmd>> {
    let count = reader.read_byte()?;
    if !(1..=32).contains(&count) {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("invalid usercmd count {count}"),
        ));
    }
    let key = command_key(checksum_feed, message_ack, reliable_command);
    let mut moves = Vec::with_capacity(usize::from(count));
    let mut previous = state.baseline();
    for _ in 0..count {
        let mut cmd = read_delta(reader, key, previous)?;
        if !state.can_select(cmd.weapon) {
            cmd.weapon = state.weapon;
        }
        if !state.can_select(cmd.offhand) {
            cmd.offhand = state.offhand;
        }
        moves.push(cmd);
        previous = cmd;
    }
    Ok(moves)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reads_oracle_full_command_and_time_delta_with_inventory_filter() -> io::Result<()> {
        // Constructed once with tools/oracle/cod2proto.py's full and nochange writers.
        let packet = [
            0xf1, 2, 0x59, 2, 0, 0, 0x54, 8, 0xd7, 0xad, 0xf4, 0xed, 9, 0x64, 0x19, 0x15, 0x6f, 6,
            0x13,
        ];
        let mut reader = MessageReader::new(&packet);
        assert_eq!(reader.read_bits(3)?, 1);
        let state = PlayerInputState {
            weapon: 2,
            offhand: 31,
            selectable_weapons: [1 << 7, 0, 0, 0],
            ..Default::default()
        };
        assert_eq!(command_key(37, 499, b"v foo 7\xff"), 0x10bbc);
        let cmds = read_moves(&mut reader, 37, 499, b"v foo 7\xff", &state)?;
        assert_eq!(
            cmds[0],
            UserCmd {
                server_time: 601,
                buttons: 0x2103,
                angles: [1000, 0xff11, 8],
                forward_move: 127,
                right_move: -127,
                weapon: 7,
                offhand: 31
            }
        );
        assert_eq!(
            cmds[1],
            UserCmd {
                server_time: 620,
                ..cmds[0]
            }
        );
        assert_eq!(reader.byte_position(), packet.len());
        Ok(())
    }

    #[test]
    fn short_form_xors_time_before_aim_and_keeps_extended_fields() -> io::Result<()> {
        let mut reader = MessageReader::new(&[0x6b, 8, 0xa1, 0x1a, 3]);
        let old = UserCmd {
            server_time: 4,
            buttons: 0x2200,
            angles: [9, 10, 11],
            weapon: 6,
            offhand: 18,
            ..Default::default()
        };
        let next = read_delta(&mut reader, 0x10bbc, old)?;
        assert_eq!(
            next,
            UserCmd {
                server_time: 12,
                buttons: 0x2201,
                angles: [9, 0x1111, 11],
                forward_move: -127,
                right_move: 127,
                ..old
            }
        );
        Ok(())
    }

    #[test]
    fn baseline_uses_player_flags_and_only_the_first_two_view_angles() {
        let state = PlayerInputState {
            pm_flags: 0x80_0000,
            entity_flags: 12,
            lean: -0.25,
            weapon_position: 0.5,
            weapon: 7,
            offhand: 4,
            view_angles: [90., -45., 100.],
            delta_angles: [5, -2, 7],
            ..Default::default()
        };
        assert_eq!(
            state.baseline(),
            UserCmd {
                weapon: 7,
                offhand: 4,
                angles: [0x3ffb, 0xe002, 0],
                buttons: 0x1140,
                ..Default::default()
            }
        );
        assert_eq!(
            PlayerInputState {
                pm_flags: 0,
                ..state
            }
            .baseline()
            .buttons,
            0
        );
    }

    #[test]
    fn invalid_count_or_incomplete_command_returns_no_partial_batch() {
        let state = PlayerInputState::default();
        for data in [&[0][..], &[33], &[2, 0, 0]] {
            assert!(read_moves(&mut MessageReader::new(data), 0, 0, b"", &state).is_err());
        }
    }
}
