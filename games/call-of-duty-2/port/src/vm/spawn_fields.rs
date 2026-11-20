//! Typed spawn keys registered by Scr_LoadFieldFiles and applied by G_ParseScrFieldValue.
use super::value::Value;
use crate::assets::GameAssets;
use crate::entities::Tokens;
use std::collections::BTreeMap;
use std::io;

#[derive(Clone, Copy, Debug)]
enum FieldType {
    Int,
    Float,
    Vector,
    String,
}

#[derive(Debug, Default)]
pub struct SpawnFields {
    fields: BTreeMap<String, FieldType>,
}

impl SpawnFields {
    /// G_InitGame enumerates radiant/*.txt through the current game search path.
    pub fn load(assets: &GameAssets) -> io::Result<Self> {
        let mut fields = Self::default();
        for path in assets.radiant_field_files()? {
            fields.add_file(&path, &assets.read_script(&path)?)?;
        }
        Ok(fields)
    }

    fn add_file(&mut self, path: &str, contents: &[u8]) -> io::Result<()> {
        let invalid = |detail: String| {
            io::Error::new(io::ErrorKind::InvalidData, format!("{detail} in '{path}'"))
        };
        let mut tokens = Tokens::new(contents);
        while let Some(word) = tokens.next() {
            let kind = match word.as_ref() {
                b"int" => FieldType::Int,
                b"float" => FieldType::Float,
                b"string" => FieldType::String,
                b"vector" => FieldType::Vector,
                _ => {
                    return Err(invalid(format!(
                        "unknown type '{}'",
                        String::from_utf8_lossy(&word)
                    )));
                }
            };
            let key = tokens
                .next()
                .ok_or_else(|| invalid("missing field name".to_owned()))?;
            let canonical = canonical_name(&key);
            if self.fields.insert(canonical.clone(), kind).is_some() {
                return Err(invalid(format!("duplicate key '{canonical}'")));
            }
        }
        Ok(())
    }

    /// Scr_FindFieldDef is case insensitive and leaves unregistered keys unset.
    pub fn parse(&self, key: &[u8], text: &[u8]) -> Option<(&str, Value)> {
        let (name, kind) = self.fields.get_key_value(&canonical_name(key))?;
        let value = match kind {
            FieldType::String => Value::String(text.to_owned()),
            FieldType::Int => Value::Int(parse_int(text)),
            FieldType::Float => Value::Float(
                float_prefix(text)
                    .and_then(|(word, _)| word.parse::<f64>().ok())
                    .unwrap_or(0.0) as f32,
            ),
            FieldType::Vector => {
                let mut result = [0.0; 3];
                let mut rest = text;
                for coordinate in &mut result {
                    let Some((word, next)) = float_prefix(rest) else {
                        break;
                    };
                    let Ok(parsed) = word.parse::<f32>() else {
                        break;
                    };
                    *coordinate = parsed;
                    rest = next;
                }
                Value::Vector(result)
            }
        };
        Some((name, value))
    }
}

fn canonical_name(key: &[u8]) -> String {
    key.iter()
        .map(|c| char::from(c.to_ascii_lowercase()))
        .collect()
}

// The reference uses i386 atoi (signed 32-bit strtol), including a numeric prefix.
pub(crate) fn parse_int(text: &[u8]) -> i32 {
    let text = text.trim_ascii_start();
    let negative = text.starts_with(b"-");
    let digits = text
        .strip_prefix(b"-")
        .or_else(|| text.strip_prefix(b"+"))
        .unwrap_or(text);
    let mut value = 0_i32;
    for digit in digits.iter().take_while(|b| b.is_ascii_digit()) {
        let n = i32::from(*digit - b'0');
        value = value.saturating_mul(10);
        value = if negative {
            value.saturating_sub(n)
        } else {
            value.saturating_add(n)
        };
    }
    value
}

// Accept a decimal prefix like C's atof/sscanf, leaving the first unmatched
// byte for the next vector conversion. Parse directly as f32 for "%f", and
// through f64 for atof so their rounding matches the two source call sites.
pub(crate) fn float_prefix(text: &[u8]) -> Option<(&str, &[u8])> {
    let text = text.trim_ascii_start();
    let mut end = usize::from(matches!(text.first(), Some(b'+' | b'-')));
    let number = &text[end..];
    for special in [b"infinity".as_slice(), b"inf", b"nan"] {
        if number
            .get(..special.len())
            .is_some_and(|prefix| prefix.eq_ignore_ascii_case(special))
        {
            end += special.len();
            return Some((std::str::from_utf8(&text[..end]).ok()?, &text[end..]));
        }
    }
    let before = number.iter().take_while(|c| c.is_ascii_digit()).count();
    end += before;
    let mut digits = before;
    if text.get(end) == Some(&b'.') {
        end += 1;
        let after = text[end..]
            .iter()
            .take_while(|c| c.is_ascii_digit())
            .count();
        end += after;
        digits += after;
    }
    if digits == 0 {
        return None;
    }
    if matches!(text.get(end), Some(b'e' | b'E')) {
        let start = end + 1 + usize::from(matches!(text.get(end + 1), Some(b'+' | b'-')));
        let exponent = text[start..]
            .iter()
            .take_while(|c| c.is_ascii_digit())
            .count();
        if exponent > 0 {
            end = start + exponent;
        }
    }
    Some((std::str::from_utf8(&text[..end]).ok()?, &text[end..]))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn types_are_loaded_from_files_and_unlisted_keys_are_skipped() -> io::Result<()> {
        let mut fields = SpawnFields::default();
        fields.add_file(
            "radiant/keys.txt",
            b"// registry\nvector Origin float script_wait",
        )?;
        fields.add_file("radiant/mod.txt", b"int COUNT /* mod */ string Label")?;
        assert_eq!(
            fields.parse(b"ORIGIN", b" .5 -2e1 invalid"),
            Some(("origin", Value::Vector([0.5, -20.0, 0.0])))
        );
        assert_eq!(
            fields.parse(b"origin", b"1+2-3"),
            Some(("origin", Value::Vector([1., 2., -3.])))
        );
        assert_eq!(
            fields.parse(b"script_wait", b"1.25suffix"),
            Some(("script_wait", Value::Float(1.25)))
        );
        assert_eq!(
            fields.parse(b"count", b"-99999999999"),
            Some(("count", Value::Int(i32::MIN)))
        );
        assert_eq!(
            fields.parse(b"label", b"\xffHello"),
            Some(("label", Value::String(b"\xffHello".to_vec())))
        );
        assert_eq!(fields.parse(b"classname", b"script_struct"), None);
        Ok(())
    }

    #[test]
    fn rejects_unknown_types_missing_names_and_duplicate_keys() {
        for bad in [b"integer x".as_slice(), b"float", b"string a int A"] {
            assert!(
                SpawnFields::default()
                    .add_file("radiant/test.txt", bad)
                    .is_err()
            );
        }
    }
}
