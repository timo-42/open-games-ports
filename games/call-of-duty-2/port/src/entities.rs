//! Spawn-variable records from the collision BSP's entity lump.

use std::io;

/// Spawn script and entity initialization both visit the records in file order.
#[derive(Debug)]
pub struct MapEntities {
    records: Vec<SpawnVars>,
}

#[derive(Debug)]
pub struct SpawnVars {
    pairs: Vec<SpawnVar>,
}

#[derive(Debug)]
struct SpawnVar {
    name: Box<[u8]>,
    value: Box<[u8]>,
}

impl SpawnVars {
    pub fn pairs(&self) -> impl Iterator<Item = (&[u8], &[u8])> {
        self.pairs
            .iter()
            .map(|pair| (pair.name.as_ref(), pair.value.as_ref()))
    }

    /// G_SpawnString uses strcmp and returns the first occurrence of a key.
    pub fn get(&self, key: &[u8]) -> Option<&[u8]> {
        self.pairs
            .iter()
            .find(|pair| pair.name.as_ref() == key)
            .map(|pair| pair.value.as_ref())
    }
}

fn invalid(message: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message)
}

/// Com_ParseExt in the default space-delimited mode. Tokens are capped at
/// 1023 bytes while the scanner still advances through the complete token.
pub(crate) struct Tokens<'a> {
    rest: &'a [u8],
}

impl<'a> Tokens<'a> {
    pub(crate) fn new(input: &'a [u8]) -> Self {
        Self {
            rest: input.split(|&c| c == 0).next().unwrap_or_default(),
        }
    }
    pub(crate) fn next(&mut self) -> Option<Box<[u8]>> {
        loop {
            let start = self.rest.iter().position(|&c| (c as i8) > 32)?;
            self.rest = &self.rest[start..];
            if self.rest.starts_with(b"//") {
                let newline = self.rest.iter().position(|&c| c == b'\n')?;
                self.rest = &self.rest[newline + 1..];
            } else if self.rest.starts_with(b"/*") {
                let end = self.rest[2..].windows(2).position(|pair| pair == b"*/")?;
                self.rest = &self.rest[end + 4..];
            } else {
                break;
            }
        }
        if let Some(contents) = self.rest.strip_prefix(b"\"") {
            let mut token = Vec::new();
            let mut bytes = contents.iter();
            while let Some(&byte) = bytes.next() {
                let mut c = byte;

                if c == b'"' {
                    break;
                }
                if c == b'\\' && matches!(bytes.as_slice().first(), Some(b'"' | b'\\')) {
                    c = *bytes.next().expect("matched escaped byte");
                }
                if token.len() < 1023 {
                    token.push(c);
                }
            }
            self.rest = bytes.as_slice();
            Some(token.into_boxed_slice())
        } else {
            let end = self
                .rest
                .iter()
                .position(|&c| (c as i8) <= 32)
                .unwrap_or(self.rest.len());
            let (token, rest) = self.rest.split_at(end);
            self.rest = rest;
            Some(token[..end.min(1023)].into())
        }
    }

    fn in_record(&mut self) -> io::Result<Box<[u8]>> {
        self.next()
            .ok_or_else(|| invalid("G_ParseSpawnVars: EOF without closing brace"))
    }
}

impl MapEntities {
    /// KeyValue_ParseBlock, followed by SP_worldspawn's classname check.
    pub fn parse(lump: &[u8]) -> io::Result<Self> {
        let mut tokens = Tokens::new(lump);
        let mut records = Vec::new();
        while let Some(open) = tokens.next() {
            if !open.starts_with(b"{") {
                return Err(invalid("G_ParseSpawnVars: expected opening brace"));
            }
            let mut pairs = Vec::new();
            let mut pool_used = 0;
            loop {
                let key = tokens.in_record()?;
                if key.starts_with(b"}") {
                    break;
                }
                let value = tokens.in_record()?;
                if value.starts_with(b"}") {
                    return Err(invalid("G_ParseSpawnVars: closing brace without data"));
                }
                pool_used += key.len() + value.len() + 2;
                if pairs.len() == 64 || pool_used > 2048 {
                    return Err(invalid("G_ParseSpawnVars: MAX_SPAWN_VARS"));
                }
                pairs.push(SpawnVar { name: key, value });
            }
            let record = SpawnVars { pairs };
            if records.is_empty()
                && !record
                    .get(b"classname")
                    .is_some_and(|class| class.eq_ignore_ascii_case(b"worldspawn"))
            {
                return Err(invalid(
                    "SP_worldspawn: The first entity isn't 'worldspawn'",
                ));
            }
            records.push(record);
        }
        if records.is_empty() {
            return Err(invalid("SpawnEntities: no entities"));
        }
        Ok(Self { records })
    }

    pub fn records(&self) -> &[SpawnVars] {
        &self.records
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ordered_spawn_vars_and_byte_values() {
        let mut text = br#"// map header
{ "classname" "Worldspawn" "message" "a\"b\\c\n" "message" "ignored duplicate" }
 /* second record */ { "classname" "mp_dm_spawn" "origin" "1 2 3" "annotation" "" "#
            .to_vec();
        text.extend_from_slice(b"\"key\" \"\xff\" }\0extra bytes");
        let map = MapEntities::parse(&text).expect("valid entity lump");
        let records = map.records();
        assert_eq!(records.len(), 2);
        assert_eq!(records[0].get(b"message"), Some(br#"a"b\c\n"#.as_slice()));
        assert_eq!(records[0].get(b"Message"), None);
        assert_eq!(records[1].get(b"origin"), Some(b"1 2 3".as_slice()));
        assert_eq!(records[1].get(b"annotation"), Some(b"".as_slice()));
        assert_eq!(records[1].get(b"key"), Some([0xff].as_slice()));
    }

    #[test]
    fn validates_world_and_block_structure() {
        for lump in [
            b"".as_slice(),
            b"{ CLASSNAME worldspawn }",
            b"unexpected",
            b"{ classname }",
            b"{ classname worldspawn",
            b"{ classname worldspawn } {",
        ] {
            assert_eq!(
                MapEntities::parse(lump)
                    .expect_err("bad entity lump")
                    .kind(),
                io::ErrorKind::InvalidData
            );
        }
    }

    #[test]
    fn enforces_pair_and_pool_limits_per_record_after_token_truncation() {
        let world = "{ classname worldspawn } ";
        for (pairs, ok) in [(64, true), (65, false)] {
            let text = format!("{world}{{ {} }}", "k v ".repeat(pairs));
            assert_eq!(MapEntities::parse(text.as_bytes()).is_ok(), ok);
        }
        for (value_len, ok) in [(1019, true), (1020, false)] {
            let text = format!(
                "{world}{{ a {} b {} }}",
                "x".repeat(2000),
                "y".repeat(value_len)
            );
            assert_eq!(MapEntities::parse(text.as_bytes()).is_ok(), ok);
        }
    }
}
