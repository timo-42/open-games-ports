//! Server-side GSC asset names, ambient track and fog configstrings.
use super::value::{Value, error};
use crate::level::ConfigStrings;
use std::io;

fn parameter(args: &[Value], index: usize) -> io::Result<&Value> {
    args.get(index)
        .ok_or_else(|| error(format!("parameter {} does not exist", index + 1)))
}

fn string(args: &[Value], index: usize) -> io::Result<Vec<u8>> {
    let mut text = parameter(args, index)?.script_string()?;
    if let Some(nul) = text.iter().position(|byte| *byte == 0) {
        text.truncate(nul);
    }
    Ok(text)
}

// G_FindConfigstringIndex (0x0811d300) reserves index zero in each table.
pub(crate) fn index(
    strings: &mut ConfigStrings,
    base: usize,
    count: usize,
    value: &[u8],
    create: bool,
    kind: &str,
) -> io::Result<i32> {
    if value.is_empty() {
        return Ok(0);
    }
    for offset in 1..count {
        let current = strings
            .get(base + offset)
            .expect("known configstring range");
        if current.eq_ignore_ascii_case(value) {
            return Ok(offset as i32);
        }
        if current.is_empty() {
            if !create {
                break;
            }
            strings.set(base + offset, value)?;
            return Ok(offset as i32);
        }
    }
    if !create {
        return Err(error(format!(
            "{kind} \"{}\" not precached",
            String::from_utf8_lossy(value)
        )));
    }
    Err(error(format!("{kind} configstring table full")))
}

// GScr_precachemenu, GScr_PrecacheStatusIcon and GScr_PrecacheHeadIcon first
// look for duplicates in the full table, then fill the first empty slot.
fn menu_or_icon(
    strings: &mut ConfigStrings,
    args: &[Value],
    base: usize,
    count: usize,
    full: &str,
) -> io::Result<Value> {
    let value = string(args, 0)?;
    let mut vacant = None;
    for slot in base..base + count {
        let text = strings.get(slot).expect("known configstring range");
        if text[..text.len().min(1023)].eq_ignore_ascii_case(&value) {
            return Ok(Value::Undefined);
        }
        if text.is_empty() && vacant.is_none() {
            vacant = Some(slot);
        }
    }
    strings.set(vacant.ok_or_else(|| error(full))?, &value)?;
    Ok(Value::Undefined)
}

fn precache_string(
    name: &str,
    args: &[Value],
    strings: &mut ConfigStrings,
    initializing: bool,
) -> io::Result<Value> {
    if !initializing {
        return Err(error(format!(
            "{name} must be called before any wait statements in the gametype or level script\n"
        )));
    }
    match name {
        "precacheString" => {
            let arg = parameter(args, 0)?;
            let Value::LocalizedString(text) = arg else {
                return Err(error(format!(
                    "type {} is not a localized string",
                    arg.type_name()
                )));
            };
            let value = text.split(|b| *b == 0).next().unwrap_or_default();
            index(strings, 0x51e, 0x100, value, true, "localized string")?;
        }
        "precacheShader" => {
            let mut name = string(args, 0)?;
            if name.is_empty() {
                return Err(error("Shader name string is empty"));
            }
            name.make_ascii_lowercase();
            index(strings, 0x61e, 0x80, &name, true, "shader")?;
        }
        _ => {
            index(strings, 0x48e, 0x10, &string(args, 0)?, true, "shellshock")?;
        }
    }
    Ok(Value::Undefined)
}

fn load_fx(args: &[Value], strings: &mut ConfigStrings, initializing: bool) -> io::Result<Value> {
    let id = index(
        strings,
        0x34e,
        0x40,
        &string(args, 0)?,
        initializing,
        "effect",
    )?;
    if id == 0 && !initializing {
        return Err(error(
            "loadFx must be called before any wait statements in the gametype or level script, or on an already loaded effect",
        ));
    }
    Ok(Value::Int(id))
}

fn ambient(play: bool, args: &[Value], strings: &mut ConfigStrings, now: i32) -> io::Result<Value> {
    let required = usize::from(play);
    if args.len() < required || args.len() > required + 1 {
        return Err(error(if play {
            "USAGE: ambientPlay(alias_name, <fadetime>);"
        } else {
            "USAGE: ambientStop(<fadetime>);"
        }));
    }
    let mut fade = 0;
    if let Some(arg) = args.get(required) {
        // Float_RoundHalfUp, then x87 signed dword conversion (including overflow).
        let millis = (arg.number()? * 1000.0 + 0.5).floor();
        fade = if (-2147483648.0..2147483648.0).contains(&millis) {
            millis as i32
        } else {
            i32::MIN
        };
    }
    let mut value = Vec::new();
    if play {
        let alias = string(args, 0)?;
        if alias.is_empty() {
            return Err(error(
                "ambientPlay: alias name cannot be the empty string... use stop or fade version",
            ));
        }
        value.extend_from_slice(b"n\\");
        value.extend(alias);
        value.push(b'\\');
    }
    if fade < 0 {
        return Err(error(if play {
            "ambientPlay: fade time must be >= 0\n"
        } else {
            "ambientStop: fade time must be >= 0\n"
        }));
    }
    value.extend_from_slice(format!("t\\{}", now.wrapping_add(fade)).as_bytes());
    strings.set(3, &value)?;
    Ok(Value::Undefined)
}

fn fog(exponential: bool, args: &[Value], strings: &mut ConfigStrings) -> io::Result<Value> {
    let function = if exponential {
        "setExpFog"
    } else {
        "setCullFog"
    };
    let first_color = if exponential { 1 } else { 2 };
    if args.len() != first_color + 4 {
        return Err(error(if exponential {
            "USAGE: setExpFog(density, red, green, blue, transition time);\nDensity must be greater than 0 and less than 1, and typically less than .001.  For example, .0002 means the fog gets .02%% more dense for every 1 unit of distance (about 1%% thicker every 50 units of distance)\n"
        } else {
            "USAGE: setCullFog(near distance, far distance, red, green, blue, transition time);\n"
        }));
    }
    let (near, far, density) = if exponential {
        (0.0, 1.0, args[0].number()?)
    } else {
        (args[0].number()?, args[1].number()?, 1.0)
    };
    let r = args[first_color].number()?;
    let g = args[first_color + 1].number()?;
    let b = args[first_color + 2].number()?;
    let time = args[first_color + 3].number()?;
    if exponential && (density <= 0.0 || density >= 1.0) {
        return Err(error(
            "setExpFog: distance must be greater than 0 and less than 1",
        ));
    }
    if near < 0.0 {
        return Err(error(format!("{function}: near distance must be >= 0")));
    }
    if far <= near {
        return Err(error(format!(
            "{function}: near distance must be less than far distance"
        )));
    }
    if [r, g, b]
        .iter()
        .any(|v| !v.is_nan() && !(0.0..=1.0).contains(v))
    {
        return Err(error(format!(
            "{function}: red/green/blue color components must be in the range [0, 1]"
        )));
    }
    if time < 0.0 {
        return Err(error(format!(
            "{function}: transition time must be >= 0 seconds"
        )));
    }
    let mut text = Vec::new();
    for value in [near, far, density, r, g, b] {
        text.extend(Value::Float(value).script_string()?);
        text.push(b' ');
    }
    text.extend_from_slice(
        format!("{:.0}", time * 1000.0)
            .to_ascii_lowercase()
            .as_bytes(),
    );
    strings.set(12, &text)?;
    Ok(Value::Undefined)
}

/// G_ParseConfigstring12Bounds parses the formatted configstring after assignment.
pub fn fog_bounds(strings: &ConfigStrings) -> (f32, f32) {
    let parse = || {
        let mut tokens = std::str::from_utf8(strings.get(12)?)
            .ok()?
            .split_ascii_whitespace();
        let mut values = [0_f32; 7];
        for value in &mut values {
            *value = tokens.next()?.parse().ok()?;
        }
        let [near, far, density, ..] = values;
        if density >= 1.0 {
            let distance = ((f64::from(far) - f64::from(near)) + f64::from(near)) as f32;
            return Some((distance, distance * distance));
        }
        None
    };
    parse().unwrap_or((f32::MAX, f32::MAX))
}

/// Names are already lowercase in the VM's builtin dispatch table.
pub fn dispatch(
    name: &str,
    args: &[Value],
    strings: &mut ConfigStrings,
    initializing: bool,
    level_time: i32,
) -> Option<io::Result<Value>> {
    Some(match name {
        "loadfx" => load_fx(args, strings, initializing),
        "precachemenu" => menu_or_icon(
            strings,
            args,
            0x4de,
            32,
            "Too many menus precached. Max allowed menus is 32",
        ),
        "precachestatusicon" => menu_or_icon(
            strings,
            args,
            0x17,
            8,
            "Too many player status icons precached. Max allowed is 8",
        ),
        "precacheheadicon" => menu_or_icon(
            strings,
            args,
            0x1f,
            15,
            "Too many player head icons precached. Max allowed is 15",
        ),
        "precachestring" => precache_string("precacheString", args, strings, initializing),
        "precacheshader" => precache_string("precacheShader", args, strings, initializing),
        "precacheshellshock" => precache_string("precacheShellShock", args, strings, initializing),
        "precacherumble" => Ok(Value::Undefined), // GScr_precacheRumble, 0x08114a36, returns immediately.
        "ambientplay" => ambient(true, args, strings, level_time),
        "ambientstop" => ambient(false, args, strings, level_time),
        "setexpfog" => fog(true, args, strings),
        "setcullfog" => fog(false, args, strings),
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn text(s: &[u8]) -> Value {
        Value::String(s.to_vec())
    }

    #[test]
    fn indices_and_loading_gate() -> io::Result<()> {
        let mut cs = ConfigStrings::default();
        let name = text(b"Effect");
        assert_eq!(
            load_fx(std::slice::from_ref(&name), &mut cs, true)?,
            Value::Int(1)
        );
        assert_eq!(load_fx(&[text(b"EFFect")], &mut cs, false)?, Value::Int(1));
        assert!(load_fx(&[text(b"missing")], &mut cs, false).is_err());
        assert_eq!(cs.get(0x34f), Some(b"Effect".as_slice()));
        assert!(precache_string("precacheString", &[name], &mut cs, true).is_err());
        let localized = Value::LocalizedString(b"MP_HINT".to_vec());
        precache_string(
            "precacheString",
            std::slice::from_ref(&localized),
            &mut cs,
            true,
        )?;
        assert!(precache_string("precacheString", &[localized], &mut cs, false).is_err());
        precache_string("precacheShader", &[text(b"ABC")], &mut cs, true)?;
        assert_eq!(cs.get(0x51f), Some(b"MP_HINT".as_slice()));
        assert_eq!(cs.get(0x61f), Some(b"abc".as_slice()));
        for i in 2..64 {
            load_fx(&[text(i.to_string().as_bytes())], &mut cs, true)?;
        }
        assert!(load_fx(&[text(b"one too many")], &mut cs, true).is_err());
        assert_eq!(cs.get(0x38e), Some(b"".as_slice()));
        Ok(())
    }

    #[test]
    fn duplicate_menu_beyond_first_hole_and_icon_limit() -> io::Result<()> {
        let mut cs = ConfigStrings::default();
        cs.set(0x4df, b"second")?;
        dispatch("precachemenu", &[text(b"SECOND")], &mut cs, false, 0).expect("builtin")?;
        assert_eq!(cs.get(0x4de), Some(b"".as_slice()));
        dispatch("precachemenu", &[text(b"first")], &mut cs, false, 0).expect("builtin")?;
        assert_eq!(cs.get(0x4de), Some(b"first".as_slice()));
        for i in 0..8 {
            dispatch("precachestatusicon", &[Value::Int(i)], &mut cs, false, 0)
                .expect("builtin")?;
        }
        assert!(
            dispatch("precachestatusicon", &[text(b"extra")], &mut cs, false, 0)
                .expect("builtin")
                .is_err()
        );
        Ok(())
    }

    #[test]
    fn ambient_fade_uses_level_time_and_fog_bounds_use_rounded_text() -> io::Result<()> {
        let mut cs = ConfigStrings::default();
        ambient(true, &[text(b"alias"), Value::Float(0.0025)], &mut cs, 40)?;
        assert_eq!(cs.get(3), Some(b"n\\alias\\t\\43".as_slice()));
        ambient(false, &[], &mut cs, 64)?;
        assert_eq!(cs.get(3), Some(b"t\\64".as_slice()));
        assert!(ambient(true, &[text(b"")], &mut cs, 0).is_err());
        fog(
            true,
            &[0.00015, 0.9, 0.95, 1.0, 0.0].map(Value::Float),
            &mut cs,
        )?;
        assert_eq!(cs.get(12), Some(b"0 1 0.00015 0.9 0.95 1 0".as_slice()));
        assert_eq!(fog_bounds(&cs), (f32::MAX, f32::MAX));
        fog(
            false,
            &[2.0, 1200.25, 1.0, 0.125, 0.75, 0.0025].map(Value::Float),
            &mut cs,
        )?;
        assert_eq!(cs.get(12), Some(b"2 1200.25 1 1 0.125 0.75 2".as_slice()));
        assert_eq!(fog_bounds(&cs), (1200.25, 1200.25_f32.powi(2)));
        Ok(())
    }
}
