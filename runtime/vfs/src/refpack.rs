//! Bounded decoder for the five-byte 0x10FB RefPack profile.
//! Other header variants are deliberately unsupported.

pub fn is_compressed(data: &[u8]) -> bool {
    data.starts_with(&[0x10, 0xfb])
}

pub fn decompress(data: &[u8], max_output: usize) -> Result<Vec<u8>, String> {
    if data.len() < 5 || !is_compressed(data) {
        return Err("Expected a five-byte 0x10FB RefPack header".to_owned());
    }
    let expected =
        (usize::from(data[2]) << 16) | (usize::from(data[3]) << 8) | usize::from(data[4]);
    if expected > max_output {
        return Err(format!("RefPack output exceeds {max_output} bytes"));
    }
    let mut output = Vec::new();
    output
        .try_reserve_exact(expected)
        .map_err(|error| format!("Cannot allocate RefPack output: {error}"))?;
    let mut cursor = 5usize;
    while cursor < data.len() {
        let control = take(data, &mut cursor, 1)?[0];
        if control >= 0xfc {
            literals(
                data,
                &mut cursor,
                &mut output,
                usize::from(control & 3),
                expected,
            )?;
            if output.len() != expected {
                return Err("RefPack terminal has an incorrect output size".to_owned());
            }
            return Ok(output);
        }
        if control >= 0xe0 {
            literals(
                data,
                &mut cursor,
                &mut output,
                usize::from(control & 0x1f) * 4 + 4,
                expected,
            )?;
            continue;
        }
        let (literal_count, distance, count) = if control >= 0xc0 {
            let operands = take(data, &mut cursor, 3)?;
            (
                usize::from(control & 3),
                (usize::from(control & 0x10) << 12)
                    + (usize::from(operands[0]) << 8)
                    + usize::from(operands[1])
                    + 1,
                (usize::from(control & 0x0c) << 6) + usize::from(operands[2]) + 5,
            )
        } else if control >= 0x80 {
            let operands = take(data, &mut cursor, 2)?;
            (
                usize::from(operands[0] >> 6),
                (usize::from(operands[0] & 0x3f) << 8) + usize::from(operands[1]) + 1,
                usize::from(control & 0x3f) + 4,
            )
        } else {
            let operand = take(data, &mut cursor, 1)?[0];
            (
                usize::from(control & 3),
                (usize::from(control & 0x60) << 3) + usize::from(operand) + 1,
                (usize::from(control & 0x1c) >> 2) + 3,
            )
        };
        literals(data, &mut cursor, &mut output, literal_count, expected)?;
        if distance > output.len() {
            return Err("RefPack back-reference precedes output".to_owned());
        }
        if count > expected - output.len() {
            return Err("RefPack copy exceeds declared output size".to_owned());
        }
        // Bytewise copies intentionally permit overlapping back-references.
        for _ in 0..count {
            output.push(output[output.len() - distance]);
        }
    }
    Err("RefPack stream has no terminal command".to_owned())
}

fn take<'a>(data: &'a [u8], cursor: &mut usize, count: usize) -> Result<&'a [u8], String> {
    let end = cursor.checked_add(count).ok_or("RefPack cursor overflow")?;
    let result = data
        .get(*cursor..end)
        .ok_or("Truncated RefPack command or literal")?;
    *cursor = end;
    Ok(result)
}

fn literals(
    data: &[u8],
    cursor: &mut usize,
    output: &mut Vec<u8>,
    count: usize,
    expected: usize,
) -> Result<(), String> {
    if count > expected - output.len() {
        return Err("RefPack literal exceeds declared output size".to_owned());
    }
    output.extend_from_slice(take(data, cursor, count)?);
    Ok(())
}
