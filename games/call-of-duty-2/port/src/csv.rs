//! Field scanner shared by CoD2 sound aliases and script file parsing.
use std::io;

/// Com_ParseCSV accepts quoting anywhere in a field. The returned token fits
/// the engine's 1024-byte C buffer. The cursor stops on the next comma or LF.
pub(crate) fn field(input: &mut &[u8]) -> io::Result<Vec<u8>> {
    let mut token = Vec::new();
    let mut quoted = false;
    while let Some((&byte, rest)) = input.split_first() {
        if !quoted && matches!(byte, b',' | b'\n') {
            break;
        }
        *input = rest;
        match byte {
            b'"' if quoted && input.starts_with(b"\"") => {
                *input = &input[1..];
                if token.len() < 1023 {
                    token.push(b'"');
                }
            }
            b'"' => quoted = !quoted,
            b'\r' if !quoted => (),
            _ if token.len() < 1023 => token.push(byte),
            _ => (),
        }
    }
    if quoted {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "unterminated quoted CSV field",
        ));
    }
    Ok(token)
}
