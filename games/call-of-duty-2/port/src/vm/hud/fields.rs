//! The fifteen script field descriptors at 0x08159ee0, including their
//! packed alignment masks. Enum names come from 0x08187c80..0x08187d00.
use super::*;
struct EnumField {
    offset: usize,
    mask: u32,
    shift: u32,
    names: &'static [&'static str],
}

fn enumeration(name: &str) -> Option<EnumField> {
    let (offset, mask, shift, names): (usize, u32, u32, &[&str]) = match name {
        "font" => (FONT, u32::MAX, 0, &["default", "bigfixed", "smallfixed"]),
        "alignx" => (ALIGN_ORIGIN, 3, 2, &["left", "center", "right"]),
        "aligny" => (ALIGN_ORIGIN, 3, 0, &["top", "middle", "bottom"]),
        "horzalign" => (
            ALIGN_SCREEN,
            7,
            3,
            &[
                "subleft",
                "left",
                "center",
                "right",
                "fullscreen",
                "noscale",
                "alignto640",
                "center_safearea",
            ],
        ),
        "vertalign" => (
            ALIGN_SCREEN,
            7,
            0,
            &[
                "subtop",
                "top",
                "middle",
                "bottom",
                "fullscreen",
                "noscale",
                "alignto480",
                "center_safearea",
            ],
        ),
        _ => return None,
    };
    Some(EnumField {
        offset,
        mask,
        shift,
        names,
    })
}

fn float_offset(name: &str) -> Option<usize> {
    Some(match name {
        "x" => X,
        "y" => Y,
        "z" => Z,
        "fontscale" => FONT_SCALE,
        "sort" => SORT,
        _ => return None,
    })
}

fn channel(value: f32) -> u8 {
    (255.0 * value.clamp(0.0, 1.0) + 0.5).floor() as u8
}

impl HudElements {
    pub(in crate::vm) fn field(&self, id: u32, name: &str) -> Option<Value> {
        let element = self.elements.get(*self.by_object.get(&id)?)?;
        let words = &element.words;
        if let Some(index) = float_offset(name) {
            return Some(Value::Float(f32::from_bits(words[index])));
        }
        if let Some(field) = enumeration(name) {
            let index = (words[field.offset] >> field.shift) & field.mask;
            return Some(Value::String(
                field.names[index as usize].as_bytes().to_vec(),
            ));
        }
        let rgba = words[COLOR]
            .to_le_bytes()
            .map(|c| f32::from(c) * (1.0 / 255.0));
        Some(match name {
            "color" => Value::Vector([rgba[0], rgba[1], rgba[2]]),
            "alpha" => Value::Float(rgba[3]),
            "label" => Value::Int(words[LABEL] as i32),
            "foreground" => Value::Int(words[FOREGROUND] as i32),
            "archived" => Value::Int(element.archived),
            _ => return None,
        })
    }

    pub(in crate::vm) fn set_field(
        &mut self,
        id: u32,
        name: &str,
        value: &Value,
        strings: &mut ConfigStrings,
        initializing: bool,
    ) -> Option<io::Result<()>> {
        let element = self.elements.get_mut(*self.by_object.get(&id)?)?;
        if let Some(index) = float_offset(name) {
            return Some(value.number().and_then(|number| {
                if name == "fontscale" && number <= 0.0 {
                    return Err(script_error(format!(
                        "font scale was {}; should be > 0",
                        general_float(number)
                    )));
                }
                element.words[index] = number.to_bits();
                Ok(())
            }));
        }
        if let Some(field) = enumeration(name) {
            return Some(string(value).and_then(|text| {
                let index = field
                    .names
                    .iter()
                    .position(|v| v.as_bytes().eq_ignore_ascii_case(&text))
                    .ok_or_else(|| script_error(format!("invalid hudelem {name}")))?;
                let word = &mut element.words[field.offset];
                *word = (*word & !(field.mask << field.shift)) | ((index as u32) << field.shift);
                Ok(())
            }));
        }
        let words = &mut element.words;
        Some(match name {
            "color" => match value {
                Value::Vector(v) => {
                    let mut rgba = words[COLOR].to_le_bytes();
                    rgba[..3].copy_from_slice(&v.map(channel));
                    words[COLOR] = u32::from_le_bytes(rgba);
                    Ok(())
                }
                _ => Err(script_error("expected a color vector")),
            },
            "alpha" => value.number().map(|alpha| {
                words[COLOR] = (words[COLOR] & 0xff_ffff) | (u32::from(channel(alpha)) << 24)
            }),
            "label" => localized(value, strings, initializing).map(|index| words[LABEL] = index),
            "foreground" => integer(value).map(|n| words[FOREGROUND] = n as u32),
            "archived" => integer(value).map(|n| element.archived = n),
            _ => return None,
        })
    }
}
