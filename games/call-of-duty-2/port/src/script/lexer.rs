//! The Flex lexer for GSC distinguishes path identifiers, localized strings,
//! and byte offsets into the unmodified source buffer.

use super::invalid;
use std::io;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(super) enum Kind {
    Word,
    Number,
    Text,
    LocalizedText,
    Symbol,
    End,
}

#[derive(Clone, Copy, Debug)]
pub(super) struct Token<'a> {
    pub kind: Kind,
    pub bytes: &'a [u8],
    pub offset: usize,
}

impl<'a> Token<'a> {
    pub fn ascii(self) -> &'a str {
        std::str::from_utf8(self.bytes).expect("ASCII syntax token")
    }
}

const OPERATORS: &[&[u8]] = &[
    b"<<=", b">>=", b"::", b"++", b"--", b"==", b"!=", b"<=", b">=", b"&&", b"||", b"<<", b">>",
    b"+=", b"-=", b"*=", b"/=", b"%=", b"&=", b"|=", b"^=",
];

pub(super) fn tokenize(source: &[u8]) -> io::Result<Vec<Token<'_>>> {
    let source = source.split(|&byte| byte == 0).next().unwrap_or_default();
    let mut tokens = Vec::new();
    let mut offset = 0;
    let mut developer = false;
    while offset < source.len() {
        let start = offset;
        let rest = &source[start..];
        let byte = rest[0];
        let kind;
        if byte.is_ascii_whitespace() {
            offset += 1;
            continue;
        }
        if rest.starts_with(b"//") {
            offset += rest.iter().position(|&b| b == b'\n').unwrap_or(rest.len());
            continue;
        }
        if let Some(comment) = rest.strip_prefix(b"/*") {
            offset += comment
                .windows(2)
                .position(|p| p == b"*/")
                .map_or(rest.len(), |end| end + 4);
            continue;
        }
        if rest.starts_with(b"/#") || rest.starts_with(b"#/") {
            if developer == (byte == b'/') {
                return Err(invalid(format!(
                    "unmatched developer delimiter at byte {offset}"
                )));
            }
            developer = !developer;
            kind = Kind::Symbol;
            offset += 2;
        } else if byte == b'"' || rest.starts_with(b"&\"") {
            let localized = byte == b'&';
            kind = if localized {
                Kind::LocalizedText
            } else {
                Kind::Text
            };
            offset += if localized { 2 } else { 1 };
            loop {
                match source.get(offset..) {
                    Some([b'"', ..]) => {
                        offset += 1;
                        break;
                    }
                    Some([b'\\', next, ..]) if *next != b'\n' => offset += 2,
                    None | Some([] | [b'\n', ..]) => {
                        return Err(invalid(format!("unterminated string at byte {start}")));
                    }
                    _ => offset += 1,
                }
            }
        } else if byte.is_ascii_digit()
            || (byte == b'.' && rest.get(1).is_some_and(u8::is_ascii_digit))
        {
            kind = Kind::Number;
            while source.get(offset).is_some_and(u8::is_ascii_digit) {
                offset += 1;
            }
            if source.get(offset) == Some(&b'.') {
                offset += 1;
                while source.get(offset).is_some_and(u8::is_ascii_digit) {
                    offset += 1;
                }
            }
            if matches!(source.get(offset), Some(b'e' | b'E')) {
                offset += 1;
                if matches!(source.get(offset), Some(b'+' | b'-')) {
                    offset += 1;
                }
                while source.get(offset).is_some_and(u8::is_ascii_digit) {
                    offset += 1;
                }
            }
        } else if byte.is_ascii_alphabetic() || matches!(byte, b'_' | b'\\') {
            kind = Kind::Word;
            offset += rest
                .iter()
                .position(|c| !(c.is_ascii_alphanumeric() || matches!(c, b'_' | b'\\')))
                .unwrap_or(rest.len());
        } else {
            kind = Kind::Symbol;
            offset += OPERATORS
                .iter()
                .find(|op| rest.starts_with(op))
                .map_or(1, |op| op.len());
        }
        tokens.push(Token {
            kind,
            offset: start,
            bytes: &source[start..offset],
        });
    }
    if developer {
        return Err(invalid("unterminated developer block"));
    }
    tokens.push(Token {
        kind: Kind::End,
        offset,
        bytes: &[],
    });
    Ok(tokens)
}

pub(super) fn decode_string(token: Token<'_>) -> Vec<u8> {
    let start = if token.kind == Kind::Text { 1 } else { 2 };
    let mut source = token.bytes[start..token.bytes.len() - 1].iter().copied();
    let mut result = Vec::new();
    while let Some(byte) = source.next() {
        result.push(if byte == b'\\' {
            match source.next().expect("complete escaped string token") {
                b'n' => b'\n',
                b'r' => b'\r',
                b't' => b'\t',
                other => other,
            }
        } else {
            byte
        });
    }
    result
}
