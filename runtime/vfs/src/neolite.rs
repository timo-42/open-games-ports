//! Static recovery of the NeoLite variant in the fingerprinted RCT executable.
//!
//! This produces a PE analysis carrier, never an executable compatibility claim.
//! The section loader and Neocomp grammar were independently derived from REA
//! `ev_7b738c37edf6f605b1a22b0c7869948a411df7da86c36ea5f172207556cba6ef`
//! and `ev_ad86e7e39c31447a9ee85f759dd748af73773444c48fff684736599882ea19a1`
//! (original stub 0xc47150/0xc471fe). Deflate64 uses the existing safe Rust
//! crate; no suitable Neocomp/NeoLite codec crate was found. This small adapter
//! implements the observed block grammar, bounded to 32 KiB per block.

use crate::{MAX_DECOMPRESSED_BYTES, MAX_FILE_BYTES, sha256_hex};

const BLOCK_LIMIT: usize = 0x8000;

#[derive(Debug)]
pub struct NeoliteImage {
    pub bytes: Vec<u8>,
    pub report: NeoliteReport,
}

#[derive(Debug)]
pub struct NeoliteReport {
    pub original_sha256: String,
    pub analysis_sha256: String,
    pub entry_point_rva: u32,
    pub import_directory_rva: u32,
    pub sections: Vec<NeoliteSection>,
    pub limitations: Vec<String>,
}

#[derive(Debug)]
pub struct NeoliteSection {
    pub name: String,
    pub rva: u32,
    pub file_offset: usize,
    pub decoded_bytes: usize,
    pub sha256: String,
    pub filtered_calls: usize,
}

/// Recover the observed codec-2 layout after verifying the caller's pinned
/// artifact identity. Games own their compatible edition fingerprints; this
/// shared adapter owns only the format and bounded static reconstruction.
pub fn recover_analysis_image(input: &[u8], expected_sha256: &str) -> Result<NeoliteImage, String> {
    if input.len() > MAX_FILE_BYTES {
        return Err("NeoLite source exceeds input budget".into());
    }
    if sha256_hex(input) != expected_sha256 {
        return Err("NeoLite source SHA-256 does not match the pinned executable".into());
    }
    recover(input)
}

fn range(input: &[u8], start: usize, len: usize) -> Result<&[u8], String> {
    input
        .get(start..start.checked_add(len).ok_or("NeoLite offset overflow")?)
        .ok_or_else(|| "Truncated NeoLite input or invalid PE offset".into())
}
fn word(input: &[u8], pos: usize) -> Result<usize, String> {
    Ok(u16::from_le_bytes(range(input, pos, 2)?.try_into().unwrap()) as usize)
}
fn dword(input: &[u8], pos: usize) -> Result<u32, String> {
    Ok(u32::from_le_bytes(
        range(input, pos, 4)?.try_into().unwrap(),
    ))
}
fn set_dword(out: &mut [u8], pos: usize, value: usize) -> Result<(), String> {
    let value = u32::try_from(value).map_err(|_| "NeoLite PE field overflow")?;
    out.get_mut(pos..pos + 4)
        .ok_or("NeoLite PE field outside headers")?
        .copy_from_slice(&value.to_le_bytes());
    Ok(())
}
fn align(size: usize, alignment: usize) -> Result<usize, String> {
    size.checked_add(alignment - 1)
        .map(|v| v & !(alignment - 1))
        .filter(|&v| v <= MAX_DECOMPRESSED_BYTES)
        .ok_or_else(|| "NeoLite analysis image exceeds output budget".into())
}

struct Cursor<'a> {
    input: &'a [u8],
    pos: usize,
    nibbles: u32,
    nibble_count: u8,
    bits: u32,
    bit_count: u8,
}
impl<'a> Cursor<'a> {
    fn new(input: &'a [u8]) -> Self {
        Self {
            input,
            pos: 0,
            nibbles: 0,
            nibble_count: 0,
            bits: 0,
            bit_count: 0,
        }
    }
    fn take(&mut self, len: usize) -> Result<&'a [u8], String> {
        let bytes = range(self.input, self.pos, len)?;
        self.pos += len;
        Ok(bytes)
    }
    fn byte(&mut self) -> Result<usize, String> {
        Ok(self.take(1)?[0] as usize)
    }
    fn nibble(&mut self) -> Result<usize, String> {
        if self.nibble_count == 0 {
            self.nibbles = u32::from_le_bytes(self.take(4)?.try_into().unwrap());
            self.nibble_count = 8;
        }
        let value = self.nibbles & 15;
        self.nibbles >>= 4;
        self.nibble_count -= 1;
        Ok(value as usize)
    }
    fn bit(&mut self) -> Result<usize, String> {
        if self.bit_count == 0 {
            self.bits = u32::from_le_bytes(self.take(4)?.try_into().unwrap());
            self.bit_count = 32;
        }
        let value = self.bits & 1;
        self.bits >>= 1;
        self.bit_count -= 1;
        Ok(value as usize)
    }
    fn two_bits(&mut self) -> Result<usize, String> {
        Ok(self.bit()? * 2 + self.bit()?)
    }
    fn reference(&mut self, extended: bool) -> Result<usize, String> {
        let high = if extended {
            let high = self.nibble()?;
            if high == 0 { self.nibble()? + 16 } else { high }
        } else {
            0
        };
        Ok(high * 256 + self.byte()?)
    }
}

fn neocomp_block(input: &[u8], limit: usize) -> Result<Vec<u8>, String> {
    let mut source = Cursor::new(input);
    let mut out = Vec::new();
    let mut history = Vec::new();
    loop {
        let code = source.nibble()?;
        let (length, reference) = match code {
            0 => (3, Some(source.byte()?)),
            1 => (4, Some(source.byte()?)),
            2 => (5, Some(source.byte()?)),
            3 => (3, Some(256 + source.byte()?)),
            4 => {
                let value = source.nibble()?;
                if value < 2 {
                    (3 + value, Some(0))
                } else {
                    (3, Some(value * 256 + source.byte()?))
                }
            }
            5 | 6 => (if code == 5 { 4 } else { 5 }, Some(source.reference(true)?)),
            7 => {
                let extended = source.bit()? != 0;
                (6, Some(source.reference(extended)?))
            }
            8 => {
                let length = 7 + source.bit()?;
                let extended = source.bit()? != 0;
                (length, Some(source.reference(extended)?))
            }
            9 => {
                let length = 9 + source.two_bits()?;
                let extended = source.bit()? != 0;
                (length, Some(source.reference(extended)?))
            }
            10 => (1, None),
            11 => (2, None),
            12 => (3 + source.bit()?, None),
            13 => (5 + source.two_bits()?, None),
            14 => (9 + source.nibble()?, None),
            15 => {
                if source.bit()? == 0 {
                    let length = 13 + source.nibble()?;
                    let extended = source.bit()? != 0;
                    (length, Some(source.reference(extended)?))
                } else if source.bit()? == 0 {
                    (25 + source.byte()?, None)
                } else {
                    let extra = source.byte()?;
                    if extra == 255 {
                        break;
                    }
                    let extended = source.bit()? != 0;
                    (29 + extra, Some(source.reference(extended)?))
                }
            }
            _ => unreachable!(),
        };
        if length > limit.saturating_sub(out.len()) {
            return Err("Neocomp block exceeds declared output budget".into());
        }
        let start = out.len();
        if let Some(index) = reference {
            let entry = history
                .len()
                .checked_sub(index + 1)
                .ok_or("Invalid Neocomp history reference")?;
            let offset = history[entry];
            if offset >= start {
                return Err("Invalid Neocomp forward reference".into());
            }
            history.push(start);
            for offset in offset..offset + length {
                let value = *out.get(offset).ok_or("Invalid Neocomp match")?;
                out.push(value);
            }
        } else {
            out.extend_from_slice(source.take(length)?);
            history.extend(start..out.len());
        }
    }
    if source.pos != input.len() {
        return Err("Neocomp compressed block has trailing bytes".into());
    }
    Ok(out)
}

fn neocomp(input: &[u8], size: usize) -> Result<(Vec<u8>, usize), String> {
    if size > MAX_DECOMPRESSED_BYTES {
        return Err("Neocomp exceeds output budget".into());
    }
    let mut source = Cursor::new(input);
    let mut out = Vec::with_capacity(size);
    while out.len() < size {
        let header = u16::from_le_bytes(source.take(2)?.try_into().unwrap());
        if header & 0x8000 == 0 {
            let length = usize::from(header) + 1;
            if length > size - out.len() {
                return Err("Neocomp literal block exceeds declared output".into());
            }
            out.extend_from_slice(source.take(length)?);
        } else {
            let packed = source.take(usize::from(header & 0x7fff))?;
            let block = neocomp_block(packed, (size - out.len()).min(BLOCK_LIMIT))?;
            if block.is_empty() {
                return Err("Neocomp block made no progress".into());
            }
            out.extend_from_slice(&block);
        }
    }
    Ok((out, source.pos))
}

fn deflate64(input: &[u8], size: usize) -> Result<Vec<u8>, String> {
    if size > MAX_DECOMPRESSED_BYTES {
        return Err("Deflate64 exceeds output budget".into());
    }
    // One extra byte detects streams that expand beyond the declared size.
    let mut out = vec![0; size + 1];
    let mut inflater = Box::new(deflate64::InflaterManaged::new());
    let (mut read, mut written) = (0, 0);
    loop {
        let result = inflater.inflate(&input[read..], &mut out[written..]);
        read += result.bytes_consumed;
        written += result.bytes_written;
        if result.data_error || inflater.errored() {
            return Err("Invalid NeoLite Deflate64 stream".into());
        }
        if written > size {
            return Err("Deflate64 exceeds declared output".into());
        }
        if inflater.finished() {
            break;
        }
        if result.bytes_consumed == 0 && result.bytes_written == 0 {
            return Err("Truncated NeoLite Deflate64 stream".into());
        }
    }
    if written != size || read != input.len() {
        return Err("Deflate64 input/output size mismatch".into());
    }
    out.truncate(size);
    Ok(out)
}

/// Undo the observed section-local x86 E8 transform. Positions refer to the
/// operand, and the scan skips four operand bytes even for unchanged calls.
fn undo_calls(bytes: &mut [u8]) -> usize {
    let size = bytes.len() as i64;
    let (mut pos, mut count) = (0, 0);
    while pos + 4 < bytes.len() {
        if bytes[pos] != 0xe8 {
            pos += 1;
            continue;
        }
        pos += 1;
        let value = i32::from_le_bytes(bytes[pos..pos + 4].try_into().unwrap()) as i64;
        if value >= -(pos as i64) && value < size {
            let value = if value >= 0 {
                value - pos as i64
            } else {
                value + size
            };
            bytes[pos..pos + 4].copy_from_slice(&(value as i32).to_le_bytes());
            count += 1;
        }
        pos += 4;
    }
    count
}

#[derive(Clone)]
struct Section {
    record: usize,
    name: String,
    rva: u32,
    virtual_size: usize,
    raw_size: usize,
    raw: usize,
}
fn recover(input: &[u8]) -> Result<NeoliteImage, String> {
    if range(input, 0, 2)? != b"MZ" {
        return Err("NeoLite input is not an MZ executable".into());
    }
    let pe = dword(input, 0x3c)? as usize;
    if range(input, pe, 4)? != b"PE\0\0" || word(input, pe + 4)? != 0x14c {
        return Err("NeoLite requires an x86 PE executable".into());
    }
    let count = word(input, pe + 6)?;
    let optional_size = word(input, pe + 20)?;
    let opt = pe + 24;
    range(input, opt, optional_size)?;
    if optional_size < 224 || word(input, opt)? != 0x10b || count == 0 || count > 96 {
        return Err("Unsupported NeoLite PE headers".into());
    }
    let table = opt + optional_size;
    range(input, table, count * 40)?;
    let header_size = dword(input, opt + 60)? as usize;
    if header_size < table + count * 40 || header_size > MAX_DECOMPRESSED_BYTES {
        return Err("Invalid NeoLite PE header size".into());
    }
    let alignment = dword(input, opt + 36)? as usize;
    if !(512..=65536).contains(&alignment) || !alignment.is_power_of_two() {
        return Err("Unsupported NeoLite PE file alignment".into());
    }
    let entry = dword(input, opt + 16)?;
    let mut sections = Vec::new();
    for i in 0..count {
        let record = table + i * 40;
        let name = range(input, record, 8)?;
        sections.push(Section {
            record,
            name: String::from_utf8_lossy(name.split(|&v| v == 0).next().unwrap()).into_owned(),
            virtual_size: dword(input, record + 8)? as usize,
            rva: dword(input, record + 12)?,
            raw_size: dword(input, record + 16)? as usize,
            raw: dword(input, record + 20)? as usize,
        });
    }
    let stub = sections
        .iter()
        .position(|s| s.name == ".neolit")
        .ok_or("Missing NeoLite stub section")?;
    let stub_section = &sections[stub];
    let entry_delta = entry
        .checked_sub(stub_section.rva)
        .ok_or("NeoLite entry outside stub")? as usize;
    if entry_delta + 0x19 > stub_section.raw_size {
        return Err("NeoLite entry outside raw stub".into());
    }
    let entry_file = stub_section
        .raw
        .checked_add(entry_delta)
        .ok_or("NeoLite entry offset overflow")?;
    let header = dword(input, entry_file + 0x15)? as usize;
    let mut fields = [0u32; 21];
    for (i, field) in fields.iter_mut().enumerate() {
        *field = dword(input, header + i * 4)?;
    }
    if fields[0] != 0x04181996 || fields[14] != 2 {
        return Err("Unsupported NeoLite header/codec".into());
    }
    let packed_count = fields[7] as usize;
    if packed_count == 0 || packed_count >= count {
        return Err("Invalid NeoLite section count".into());
    }
    let descriptors = header + 84;
    range(input, descriptors, packed_count * 16)?;
    let mut cursor = descriptors + packed_count * 16;
    let (text, used) = neocomp(
        range(input, cursor, input.len() - cursor)?,
        fields[3] as usize,
    )?;
    cursor += used;
    let (data, used) = neocomp(
        range(input, cursor, input.len() - cursor)?,
        fields[5] as usize,
    )?;
    cursor += used;
    cursor = cursor
        .checked_add(fields[6] as usize)
        .ok_or("NeoLite relocation offset overflow")?;
    range(input, cursor, 0)?;
    let mut payloads: Vec<Option<(Vec<u8>, u32, usize)>> = vec![None; count];
    let mut total = header_size;
    for i in 0..packed_count {
        let d = descriptors + i * 16;
        let index = dword(input, d)? as usize;
        let flags = dword(input, d + 4)?;
        let size = dword(input, d + 8)? as usize;
        let packed = dword(input, d + 12)? as usize;
        if index >= count || index == stub || payloads[index].is_some() {
            return Err("Invalid/duplicate NeoLite section index".into());
        }
        total = total
            .checked_add(size)
            .filter(|&v| v <= MAX_DECOMPRESSED_BYTES)
            .ok_or("NeoLite sections exceed output budget")?;
        let mut decoded = deflate64(range(input, cursor, packed)?, size)?;
        cursor += packed;
        let filtered = if flags & 0x20 != 0 {
            undo_calls(&mut decoded)
        } else {
            0
        };
        if index == fields[15] as usize {
            let prefix_size = fields[16] as usize;
            if prefix_size > sections[index].raw_size {
                return Err("NeoLite resource prefix exceeds original section".into());
            }
            let mut prefixed = range(input, sections[index].raw, prefix_size)?.to_vec();
            prefixed.extend_from_slice(&decoded);
            decoded = prefixed;
        }
        if decoded.len() > sections[index].virtual_size {
            return Err("NeoLite payload exceeds PE virtual section size".into());
        }
        payloads[index] = Some((decoded, flags, filtered));
    }
    let mut expanded_stub = range(input, stub_section.raw, stub_section.raw_size)?.to_vec();
    for (relative, decoded) in [(fields[2], text), (fields[4], data)] {
        let start = entry_delta
            .checked_add(relative as usize)
            .ok_or("NeoLite embedded decoder offset overflow")?;
        let end = start
            .checked_add(decoded.len())
            .filter(|&v| v <= stub_section.virtual_size && v <= MAX_DECOMPRESSED_BYTES)
            .ok_or("NeoLite embedded decoder exceeds stub size")?;
        expanded_stub.resize(expanded_stub.len().max(end), 0);
        expanded_stub[start..end].copy_from_slice(&decoded);
    }
    payloads[stub] = Some((expanded_stub, dword(input, stub_section.record + 36)?, 0));
    let mut out = range(input, 0, header_size)?.to_vec();
    let mut reports = Vec::new();
    for (section, payload) in sections.iter().zip(payloads) {
        let (decoded, flags, filtered_calls) = payload.ok_or("Unrecovered NeoLite section")?;
        let file_offset = align(out.len(), alignment)?;
        let raw_size = align(decoded.len(), alignment)?;
        let end = file_offset
            .checked_add(raw_size)
            .filter(|&v| v <= MAX_DECOMPRESSED_BYTES)
            .ok_or("NeoLite analysis image exceeds budget")?;
        out.resize(end, 0);
        out[file_offset..file_offset + decoded.len()].copy_from_slice(&decoded);
        set_dword(&mut out, section.record + 16, raw_size)?;
        set_dword(&mut out, section.record + 20, file_offset)?;
        set_dword(&mut out, section.record + 36, flags as usize)?;
        reports.push(NeoliteSection {
            name: section.name.clone(),
            rva: section.rva,
            file_offset,
            decoded_bytes: decoded.len(),
            sha256: sha256_hex(&decoded),
            filtered_calls,
        });
    }
    // Preserve the preferred-base image and original RVA mapping. Import thunks
    // are static metadata, never bound to host functions or executed here.
    set_dword(&mut out, opt + 16, fields[8] as usize)?;
    set_dword(&mut out, opt + 104, fields[9] as usize)?;
    let mut import_size = 0;
    if fields[9] != 0 {
        let import_section = reports
            .iter()
            .find(|s| fields[9] >= s.rva && (fields[9] - s.rva) < s.decoded_bytes as u32)
            .ok_or("Recovered import directory is outside sections")?;
        let start = import_section.file_offset + (fields[9] - import_section.rva) as usize;
        let remaining = import_section.decoded_bytes - (fields[9] - import_section.rva) as usize;
        for entry in range(&out, start, remaining)?
            .as_chunks::<20>()
            .0
            .iter()
            .take(4096)
        {
            import_size += 20;
            if entry.iter().all(|&b| b == 0) {
                break;
            }
        }
        if import_size == 0
            || !range(&out, start + import_size - 20, 20)?
                .iter()
                .all(|&b| b == 0)
        {
            return Err("Recovered import directory lacks bounded terminator".into());
        }
    }
    set_dword(&mut out, opt + 108, import_size)?;
    // Stale signatures/checksums/bound imports cannot authenticate a derivative.
    set_dword(&mut out, opt + 64, 0)?;
    for offset in [128, 132, 184, 188] {
        set_dword(&mut out, opt + offset, 0)?;
    }
    let report = NeoliteReport {
        original_sha256: sha256_hex(input), analysis_sha256: sha256_hex(&out),
        entry_point_rva: fields[8], import_directory_rva: fields[9], sections: reports,
        limitations: vec![
            "Derived static PE analysis carrier; executable equivalence and game playability are not established.".into(),
            "Only the fingerprinted x86 NeoLite codec-2 variant is validated; other editions and codecs are unsupported.".into(),
            "Original import/resource metadata is recovered statically; host import binding, runtime initialization and relocation execution are not performed.".into(),
        ],
    };
    Ok(NeoliteImage { bytes: out, report })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn stored(bytes: &[u8]) -> Vec<u8> {
        let mut packed = vec![1];
        packed.extend_from_slice(&(bytes.len() as u16).to_le_bytes());
        packed.extend_from_slice(&(!(bytes.len() as u16)).to_le_bytes());
        packed.extend_from_slice(bytes);
        packed
    }
    fn literal(bytes: &[u8]) -> Vec<u8> {
        let mut packed = ((bytes.len() - 1) as u16).to_le_bytes().to_vec();
        packed.extend_from_slice(bytes);
        packed
    }
    fn fixture() -> Vec<u8> {
        let mut bytes = vec![0; 0x800];
        bytes[..2].copy_from_slice(b"MZ");
        set_dword(&mut bytes, 0x3c, 0x80).unwrap();
        bytes[0x80..0x84].copy_from_slice(b"PE\0\0");
        bytes[0x84..0x86].copy_from_slice(&0x14cu16.to_le_bytes());
        bytes[0x86..0x88].copy_from_slice(&2u16.to_le_bytes());
        bytes[0x94..0x96].copy_from_slice(&224u16.to_le_bytes());
        let opt = 0x98;
        bytes[opt..opt + 2].copy_from_slice(&0x10bu16.to_le_bytes());
        for (offset, value) in [(16, 0x2000), (36, 0x200), (60, 0x200)] {
            set_dword(&mut bytes, opt + offset, value).unwrap();
        }
        let table = opt + 224;
        for (i, name, rva, raw) in [
            (0, b".text\0\0\0", 0x1000, 0),
            (1, b".neolit\0", 0x2000, 0x200),
        ] {
            let record = table + i * 40;
            bytes[record..record + 8].copy_from_slice(name);
            for (field, value) in [(8, 0x200), (12, rva), (16, 0x200), (20, raw)] {
                set_dword(&mut bytes, record + field, value).unwrap();
            }
        }
        set_dword(&mut bytes, 0x215, 0x400).unwrap();
        let header = [
            0x04181996, 0, 0x80, 1, 0x90, 1, 0, 1, 0x1000, 0, 0, 0, 0, 0, 2, 0xffff, 0, 0, 0, 0, 2,
        ];
        for (i, value) in header.into_iter().enumerate() {
            set_dword(&mut bytes, 0x400 + i * 4, value).unwrap();
        }
        let section = stored(b"native!");
        for (i, value) in [0, 0x60000020, 7, section.len()].into_iter().enumerate() {
            set_dword(&mut bytes, 0x454 + i * 4, value).unwrap();
        }
        let mut payload = literal(b"x");
        payload.extend(literal(b"y"));
        payload.extend(section);
        bytes[0x464..0x464 + payload.len()].copy_from_slice(&payload);
        bytes
    }

    #[test]
    fn literal_blocks_respect_lengths_and_caps() {
        assert_eq!(
            neocomp(&literal(b"hello"), 5).unwrap(),
            (b"hello".to_vec(), 7)
        );
        assert!(neocomp(&literal(b"hello"), 4).is_err());
        assert!(neocomp(&literal(b"hello")[..6], 5).is_err());
        assert!(neocomp(&[], MAX_DECOMPRESSED_BYTES + 1).is_err());
    }
    #[test]
    fn compressed_literals_and_history_matches() {
        // LSB control nibbles: literal-one, match-three(last history), end.
        // End marker consumes two set bits and byte 255.
        let packed = [0x0a, 0x0f, 0, 0, b'Q', 0, 3, 0, 0, 0, 255];
        assert_eq!(neocomp_block(&packed, 4).unwrap(), b"QQQQ");
        let mut stream = (0x8000u16 | packed.len() as u16).to_le_bytes().to_vec();
        stream.extend(packed);
        assert_eq!(neocomp(&stream, 4).unwrap().0, b"QQQQ");
        assert!(neocomp_block(&packed, 3).is_err());
        let mut invalid = packed;
        invalid[5] = 1;
        assert!(neocomp_block(&invalid, 4).is_err());
        for end in 0..packed.len() {
            assert!(neocomp_block(&packed[..end], 4).is_err());
        }
    }
    #[test]
    fn compressed_literal_runs_and_bit_order() {
        // literal 5+binary10 = 7, then end; bits are LSB encoded but read
        // most-significant first into the two-bit length value.
        let mut packed = vec![0xfd, 0, 0, 0, 13, 0, 0, 0];
        packed.extend(b"1234567");
        packed.push(255);
        assert_eq!(neocomp_block(&packed, 7).unwrap(), b"1234567");
    }
    #[test]
    fn deflate64_requires_complete_bounded_streams() {
        assert_eq!(deflate64(&stored(b"safe"), 4).unwrap(), b"safe");
        // RFC1951 fixed-Huffman literals 'A', 'B', 'C', end; raw framing is
        // also valid Deflate64 for these literal-only symbols.
        assert_eq!(deflate64(&[0x73, 0x74, 0x72, 0x06, 0], 3).unwrap(), b"ABC");
        assert!(deflate64(&stored(b"safe"), 3).is_err());
        assert!(deflate64(&stored(b"safe"), 5).is_err());
        assert!(deflate64(&[1, 4, 0, 0xfb, 0xff, b's'], 4).is_err());
        assert!(deflate64(&[7], 1).is_err());
        assert!(deflate64(&[], MAX_DECOMPRESSED_BYTES + 1).is_err());
    }
    #[test]
    fn filter_positions_negative_operands_and_bounds() {
        let mut bytes = vec![
            0xe8, 7, 0, 0, 0, 0xe8, 0xfa, 0xff, 0xff, 0xff, 0xe8, 100, 0, 0, 0,
        ];
        assert_eq!(undo_calls(&mut bytes), 2);
        assert_eq!(i32::from_le_bytes(bytes[1..5].try_into().unwrap()), 6);
        assert_eq!(i32::from_le_bytes(bytes[6..10].try_into().unwrap()), 9);
        assert_eq!(i32::from_le_bytes(bytes[11..15].try_into().unwrap()), 100);
    }
    #[test]
    fn static_image_preserves_rvas_and_maps_recovered_sections() {
        let image = recover(&fixture()).unwrap();
        assert_eq!(image.report.entry_point_rva, 0x1000);
        let text = &image.report.sections[0];
        assert_eq!(text.rva, 0x1000);
        assert_eq!(
            &image.bytes[text.file_offset..text.file_offset + 7],
            b"native!"
        );
        let stub = &image.report.sections[1];
        assert_eq!(image.bytes[stub.file_offset + 0x80], b'x');
        assert_eq!(image.bytes[stub.file_offset + 0x90], b'y');
        assert_eq!(dword(&image.bytes, 0x98 + 16).unwrap(), 0x1000);
        assert!(recover_analysis_image(&fixture(), &"0".repeat(64)).is_err());
    }
    #[test]
    fn invalid_pe_offsets_and_sizes_are_rejected() {
        for end in [0, 2, 63, 0x100, 0x200, 0x420, 0x464] {
            assert!(recover(&fixture()[..end]).is_err());
        }
        for (field, value) in [
            (0x3c, u32::MAX as usize),
            (0x215, u32::MAX as usize),
            (0x98 + 60, 0x100),
            (0x98 + 36, 3),
            (0x454, 100),
            (0x454 + 8, MAX_DECOMPRESSED_BYTES + 1),
        ] {
            let mut bytes = fixture();
            set_dword(&mut bytes, field, value).unwrap();
            assert!(recover(&bytes).is_err(), "accepted field {field:x}");
        }
    }
    #[test]
    #[ignore = "requires explicitly supplied original media; never used by synthetic CI"]
    fn original_artifact_validation() {
        let path = std::env::var("OPENGAMES_NEOLITE_ORIGINAL").expect("set original-media path");
        let input = std::fs::read(path).unwrap();
        let image = recover_analysis_image(
            &input,
            "bdfebd64383b231de0252fe0726523d1c45aa2c4da919c2d7ed8bfb7aaa05c76",
        )
        .unwrap();
        eprintln!("{:#?}", image.report);
        assert_eq!(
            image.report.analysis_sha256,
            "478df03e8515c89198644f3db5caa46769d21c02ad3f21543ac332b604b8666e"
        );
        assert_eq!(image.report.sections.len(), 7);
        assert_eq!(image.report.entry_point_rva, 0x18e06d);
        assert_eq!(image.report.import_directory_rva, 0x487f08);
        assert_eq!(
            image
                .report
                .sections
                .iter()
                .map(|s| s.filtered_calls)
                .sum::<usize>(),
            9975
        );
    }
}
