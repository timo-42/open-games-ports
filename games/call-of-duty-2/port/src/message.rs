//! Protocol 118 hybrid message cursor and fixed Huffman codec (recon chunk_008).
//! A bit field can resume in a previously allocated byte after an intervening
//! whole-byte write. Only the outer Huffman stream uses a contiguous bit cursor.

mod codebook;
use bitstream_io::{BitWrite, BitWriter, LittleEndian};
use codebook::CODES;
use std::{fmt, io};

#[derive(Debug)]
struct CapacityExceeded;

impl fmt::Display for CapacityExceeded {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("message exceeds capacity")
    }
}

impl std::error::Error for CapacityExceeded {}

/// Identifies a full output buffer for the command-only snapshot retry.
pub fn capacity_exceeded(err: &io::Error) -> bool {
    err.get_ref()
        .is_some_and(|cause| cause.is::<CapacityExceeded>())
}

fn capacity_error() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, CapacityExceeded)
}

fn width_error() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidInput, "bit field exceeds 32 bits")
}

fn normalize(byte: u8) -> u8 {
    if byte == 0x92 { b'\'' } else { byte }
}

#[derive(Debug)]
pub struct MessageWriter {
    data: Vec<u8>,
    limit: usize,
    bit: usize,
}

impl MessageWriter {
    pub fn new(limit: usize) -> Self {
        Self {
            data: Vec::new(),
            limit,
            bit: 0,
        }
    }

    pub fn as_bytes(&self) -> &[u8] {
        &self.data
    }

    pub fn into_bytes(self) -> Vec<u8> {
        self.data
    }

    fn require(&self, count: usize) -> io::Result<()> {
        if count > self.limit - self.data.len() {
            Err(capacity_error())
        } else {
            Ok(())
        }
    }

    fn push_bit(&mut self, value: bool) {
        if self.bit.is_multiple_of(8) {
            self.bit = self.data.len() * 8;
            self.data.push(0);
        }
        self.data[self.bit / 8] |= u8::from(value) << (self.bit % 8);
        self.bit += 1;
    }

    pub fn write_bit(&mut self, value: bool) -> io::Result<()> {
        self.require(1)?;
        self.push_bit(value);
        Ok(())
    }

    pub fn write_bits(&mut self, value: u32, count: u8) -> io::Result<()> {
        if count > 32 {
            return Err(width_error());
        }
        // MSG_WriteBits reserves room for four bytes regardless of field width.
        self.require(4)?;
        for shift in 0..count {
            self.push_bit(value & (1 << shift) != 0);
        }
        Ok(())
    }

    pub fn write_data(&mut self, value: &[u8]) -> io::Result<()> {
        self.require(value.len())?;
        self.data.extend_from_slice(value);
        Ok(())
    }

    pub fn write_byte(&mut self, value: u8) -> io::Result<()> {
        self.write_data(&[value])
    }

    pub fn write_short(&mut self, value: i16) -> io::Result<()> {
        self.write_data(&value.to_le_bytes())
    }

    pub fn write_long(&mut self, value: i32) -> io::Result<()> {
        self.write_data(&value.to_le_bytes())
    }

    fn write_text(&mut self, text: &[u8], limit: usize) -> io::Result<()> {
        let text = text.split(|&byte| byte == 0).next().unwrap_or_default();
        if text.len() >= limit {
            return self.write_byte(0);
        }
        self.require(text.len() + 1)?;
        self.data.extend(text.iter().copied().map(normalize));
        self.data.push(0);
        Ok(())
    }

    pub fn write_string(&mut self, value: &[u8]) -> io::Result<()> {
        self.write_text(value, 1024)
    }

    pub fn write_big_string(&mut self, value: &[u8]) -> io::Result<()> {
        self.write_text(value, 8192)
    }
}

#[derive(Clone, Debug)]
pub struct MessageReader<'a> {
    data: &'a [u8],
    pos: usize,
    bit: usize,
}

impl<'a> MessageReader<'a> {
    pub fn new(data: &'a [u8]) -> Self {
        Self {
            data,
            pos: 0,
            bit: 0,
        }
    }

    pub fn byte_position(&self) -> usize {
        self.pos
    }

    pub fn read_bit(&mut self) -> io::Result<bool> {
        if self.bit.is_multiple_of(8) {
            self.bit = self.pos * 8;
            self.read_data(1)?;
        }
        let value = self.data[self.bit / 8] & (1 << (self.bit % 8)) != 0;
        self.bit += 1;
        Ok(value)
    }

    pub fn read_bits(&mut self, count: u8) -> io::Result<u32> {
        if count > 32 {
            return Err(width_error());
        }
        let mut value = 0;
        for shift in 0..count {
            value |= u32::from(self.read_bit()?) << shift;
        }
        Ok(value)
    }

    pub fn read_data(&mut self, count: usize) -> io::Result<&'a [u8]> {
        if count > self.data.len() - self.pos {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "end of message",
            ));
        }
        let start = self.pos;
        self.pos += count;
        Ok(&self.data[start..self.pos])
    }

    pub fn read_byte(&mut self) -> io::Result<u8> {
        Ok(self.read_data(1)?[0])
    }

    pub fn read_short(&mut self) -> io::Result<i16> {
        let bytes = self.read_data(2)?;
        Ok(i16::from_le_bytes([bytes[0], bytes[1]]))
    }

    pub fn read_long(&mut self) -> io::Result<i32> {
        let bytes = self.read_data(4)?;
        Ok(i32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]))
    }

    fn read_text(&mut self, limit: usize, replace_percent: bool) -> io::Result<Vec<u8>> {
        let mut output = Vec::new();
        loop {
            match self.read_byte()? {
                0 => return Ok(output),
                byte if output.len() < limit - 1 => {
                    output.push(if replace_percent && byte == b'%' {
                        b'.'
                    } else {
                        normalize(byte)
                    });
                }
                _ => {}
            }
        }
    }

    pub fn read_string(&mut self) -> io::Result<Vec<u8>> {
        self.read_text(1024, false)
    }

    pub fn read_big_string(&mut self) -> io::Result<Vec<u8>> {
        self.read_text(8192, true)
    }
}

/// Huff_CompressBytes (0x08067f4e). Stock padding is zero up to the final byte.
pub fn compress(data: &[u8], max_output: usize) -> io::Result<Vec<u8>> {
    let bits = data
        .iter()
        .try_fold(0usize, |bits, &byte| {
            bits.checked_add(usize::from(CODES[usize::from(byte)].1))
        })
        .ok_or_else(capacity_error)?;
    let size = bits.div_ceil(8);
    if size > max_output {
        return Err(capacity_error());
    }
    let mut writer = BitWriter::endian(Vec::with_capacity(size), LittleEndian);
    for &byte in data {
        let (bits, width) = CODES[usize::from(byte)];
        writer.write_var(u32::from(width), bits)?;
    }
    writer.byte_align()?;
    Ok(writer.into_writer())
}

// The deepest leaf has 11 bits. The single unmatched 11-bit prefix is NYT
// (256), which the reference decoder casts to an output byte of zero.
const DECODE: [(u8, u8); 2048] = {
    let mut table = [(0, 11); 2048];
    let mut symbol = 0;
    while symbol < 256 {
        let (code, width) = CODES[symbol];
        let mut prefix = code as usize;
        while prefix < table.len() {
            table[prefix] = (symbol as u8, width);
            prefix += 1 << width;
        }
        symbol += 1;
    }
    table
};

/// Huff_DecompressBytes (0x08067fa2). There is no decoded-length prefix.
/// Decode every symbol that starts within the payload; final pad bits can
/// yield extra bytes after the protocol terminator. Match the oracle's zero
/// extension for the last codeword without reading outside the datagram.
pub fn decompress(data: &[u8], max_output: usize) -> io::Result<Vec<u8>> {
    let mut output = Vec::new();
    let mut offset = 0usize;
    while offset / 8 < data.len() {
        if output.len() >= max_output {
            return Err(capacity_error());
        }
        let tail = &data[offset / 8..];
        let window = u32::from_le_bytes([
            tail[0],
            tail.get(1).copied().unwrap_or(0),
            tail.get(2).copied().unwrap_or(0),
            0,
        ]);
        let index = (window >> (offset % 8)) & 0x7ff;
        let (byte, width) = DECODE[index as usize];
        output.push(byte);
        offset += usize::from(width);
    }
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn interleaved_bits_and_raw_fields() -> io::Result<()> {
        let mut out = MessageWriter::new(128);
        out.write_bits(5, 3)?;
        out.write_long(-500)?;
        out.write_bits(0x27, 6)?;
        out.write_short(-32768)?;
        out.write_bit(true)?;
        assert_eq!(out.as_bytes(), &[0x3d, 0x0c, 0xfe, 0xff, 0xff, 3, 0, 0x80]);
        let mut input = MessageReader::new(out.as_bytes());
        assert_eq!(input.read_bits(3)?, 5);
        assert_eq!(input.read_long()?, -500);
        assert_eq!(input.read_bits(6)?, 0x27);
        assert_eq!(input.read_short()?, -32768);
        assert!(input.read_bit()?);
        assert_eq!(input.byte_position(), 8);
        Ok(())
    }

    #[test]
    fn writer_capacity_guards_match_each_primitive() -> io::Result<()> {
        let mut writer = MessageWriter::new(4);
        writer.write_bits(1, 1)?;
        assert!(capacity_exceeded(
            &writer.write_bits(0, 1).expect_err("insufficient capacity")
        ));
        assert!(!capacity_exceeded(
            &writer.write_bits(0, 33).expect_err("invalid bit width")
        ));
        writer.write_bit(true)?;
        assert!(writer.write_data(&[3; 4]).is_err());
        assert_eq!(writer.as_bytes(), [3]);
        assert!(!capacity_exceeded(
            &MessageReader::new(&[1])
                .read_short()
                .expect_err("truncated short")
        ));
        Ok(())
    }

    #[test]
    fn text_normalization_and_size_limits() -> io::Result<()> {
        let mut writer = MessageWriter::new(2048);
        writer.write_string(b"a%\x92\0unused")?;
        writer.write_big_string(b"a%\x92")?;
        writer.write_string(&[b'x'; 1024])?;
        assert_eq!(writer.as_bytes(), b"a%'\0a%'\0\0");
        let mut reader = MessageReader::new(writer.as_bytes());
        assert_eq!(reader.read_string()?, b"a%'");
        assert_eq!(reader.read_big_string()?, b"a.'");
        assert_eq!(reader.read_string()?, b"");
        Ok(())
    }

    #[test]
    fn oracle_huffman_padding_and_limits() -> io::Result<()> {
        let data = [0, 42, 17, 99, 255, 13];
        let packed = [0xce, 0x80, 0xe5, 0x31, 0xd9, 0x02];
        assert_eq!(compress(&data, packed.len())?, packed);
        assert_eq!(decompress(&packed, 7)?, [0, 42, 17, 99, 255, 13, 208]);
        assert!(compress(&data, 5).is_err());
        assert!(decompress(&packed, 6).is_err());
        assert!(compress(&[], 0)?.is_empty());
        assert!(decompress(&[], 0)?.is_empty());
        Ok(())
    }

    // cod2proto.Huffman().compress(bytes(range(256))) exercises every leaf.
    #[test]
    fn full_alphabet_matches_oracle() -> io::Result<()> {
        #[rustfmt::skip]
        let expected = [
            0x6e, 0x24, 0x3b, 0x34, 0x02, 0xf2, 0x57, 0x5a, 0x7a, 0x31, 0x69, 0x73, 0x58, 0x95, 0xb0, 0xc0,
            0xc7, 0xdf, 0xdd, 0xfc, 0x2b, 0x8e, 0x4c, 0xaf, 0xb1, 0x55, 0xc6, 0x5e, 0x6f, 0xe8, 0x09, 0xf5,
            0x3d, 0x0d, 0x91, 0x66, 0x8b, 0xcf, 0xbf, 0xe1, 0xe6, 0x29, 0xb1, 0x36, 0x03, 0x05, 0x71, 0xc2,
            0x4b, 0x1f, 0xef, 0x34, 0xfc, 0x7e, 0x79, 0x2a, 0xb6, 0x41, 0xd8, 0x03, 0xf1, 0x32, 0x47, 0x36,
            0x12, 0x2f, 0xa2, 0xf3, 0x3d, 0x43, 0xf9, 0x84, 0x90, 0x28, 0x40, 0x70, 0x7a, 0x8e, 0x71, 0x6d,
            0xdc, 0x9d, 0x57, 0x3d, 0x6e, 0x66, 0x96, 0x9d, 0xac, 0xb5, 0xf6, 0x74, 0x71, 0x8d, 0x37, 0xe3,
            0x35, 0xcb, 0xc2, 0x45, 0x75, 0xac, 0xe0, 0x3a, 0xec, 0x79, 0x3c, 0xab, 0x1f, 0x5f, 0x82, 0x05,
            0x3d, 0xe5, 0x50, 0x7a, 0x11, 0xc3, 0x7c, 0xc4, 0x30, 0x5b, 0xc8, 0x78, 0x94, 0x97, 0x05, 0x5e,
            0x64, 0x2c, 0x85, 0x8f, 0xce, 0xbd, 0xa1, 0x11, 0xe5, 0x2f, 0xc5, 0xec, 0x3f, 0x0c, 0x06, 0x1c,
            0x9e, 0x9d, 0x5f, 0xd1, 0x2d, 0x27, 0x1e, 0xa8, 0x49, 0xcc, 0xb1, 0x00, 0x5f, 0xbf, 0x42, 0x51,
            0x6b, 0xf4, 0xb0, 0x72, 0x3e, 0xe3, 0x14, 0xb9, 0x52, 0x0c, 0xeb, 0xe7, 0x46, 0x31, 0xbf, 0x99,
            0x58, 0xe8, 0x36, 0xb8, 0x8e, 0xa3, 0xc3, 0x19, 0xc6, 0xb8, 0x45, 0xaf, 0xd9, 0xcc, 0x32, 0x4d,
            0xe1, 0x08, 0xbe, 0x47, 0xde, 0x8b, 0x61, 0x7d, 0xfc, 0x4c, 0xa7, 0xa7, 0x70, 0x40, 0xde, 0x3a,
            0x0d, 0x7c, 0xcc, 0x46, 0xa8, 0x06, 0x76, 0x81, 0x89, 0xe3, 0x0f, 0xc6, 0x8d, 0xcf, 0x62, 0x11,
            0x03, 0x74, 0x54, 0x5c, 0xd3, 0x1d, 0x00, 0xc8, 0xbd, 0x9b, 0xc0, 0x1a, 0xca, 0xa5, 0x39, 0xca,
            0x7b, 0xa3, 0xac, 0x71, 0xac, 0x7a, 0x0f, 0xa6, 0x7b, 0x7c, 0xa2, 0x45, 0xca, 0x04, 0xdf, 0x11,
            0xe2, 0x98, 0x36, 0xf2, 0x64, 0x8c, 0x07, 0x85, 0x39, 0x22, 0x46, 0x81, 0xc1, 0x5e, 0x07, 0x4f,
            0xb1, 0x00, 0x47, 0x8f, 0x7a, 0x87, 0x00, 0x2d, 0x00, 0xeb, 0x7c, 0x51, 0xcb, 0xb5, 0xd1, 0x17,
            0x09,

        ];
        let alphabet: Vec<u8> = (0..=255).collect();
        assert_eq!(compress(&alphabet, expected.len())?, expected);
        let mut decoded = decompress(&expected, 257)?;
        assert_eq!(decoded.pop(), Some(208));
        assert_eq!(decoded, alphabet);
        Ok(())
    }
}
