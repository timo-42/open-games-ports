//! Stock script function-table entries in chunks 104–106. Object lifetime,
//! entity methods, and asset precaching are handled by the engine host.
use super::value::{ArrayKey, Value, error, script_error};
use crate::config::{ServerConfig, integer_prefix};
use std::collections::BTreeMap;
use std::io;

fn arg(args: &[Value], i: usize) -> io::Result<&Value> {
    args.get(i)
        .ok_or_else(|| script_error(format!("parameter {} does not exist", i + 1)))
}

fn int(args: &[Value], i: usize) -> io::Result<i32> {
    match arg(args, i)? {
        Value::Int(n) => Ok(*n),
        v => Err(script_error(format!(
            "type {} is not an int",
            v.type_name()
        ))),
    }
}

fn vector(args: &[Value], i: usize) -> io::Result<[f32; 3]> {
    match arg(args, i)? {
        Value::Vector(v) => Ok(*v),
        value => Err(script_error(format!(
            "type {} is not a vector",
            value.type_name()
        ))),
    }
}

fn bytes(args: &[Value], i: usize) -> io::Result<Vec<u8>> {
    arg(args, i)?.script_string()
}

fn utf8(bytes: Vec<u8>) -> io::Result<String> {
    String::from_utf8(bytes).map_err(|_| error("dvar text is not representable as UTF-8"))
}

fn dvar_value(
    args: &[Value],
    config: &mut ServerConfig,
    players: Option<&crate::player::Players>,
) -> io::Result<String> {
    if matches!(arg(args, 1)?, Value::LocalizedString(_)) {
        return utf8(super::messages::build(
            &args[1..],
            "Dvar Value",
            1024,
            config,
            |id| {
                let players = players?;
                let slot = players.slot_for_object(id)?;
                Some(players.get(slot)?.name.clone())
            },
        )?);
    }
    utf8(bytes(args, 1)?)
}

fn dvar_call(
    name: &str,
    args: &[Value],
    config: &mut ServerConfig,
    players: Option<&crate::player::Players>,
) -> io::Result<Value> {
    let key = utf8(bytes(args, 0)?)?;
    match name {
        "getcvar" => Ok(Value::String(
            config.get(&key).unwrap_or_default().as_bytes().to_vec(),
        )),
        "getcvarint" => Ok(Value::Int(integer_prefix(
            config.get(&key).unwrap_or_default(),
        ))),
        "getcvarfloat" => Ok(Value::Float(
            crate::config::float_prefix(config.get(&key).unwrap_or_default()).0,
        )),
        "makecvarserverinfo" => {
            if config.get(&key).is_none() {
                let value = dvar_value(args, config, players)?;
                config.set_from_script(&key, &value);
            }
            // Flag 0x100 registers a named configstring separately from getstatus.
            config.add_flags(&key, crate::config::SCRIPT_INFO);
            Ok(Value::Undefined)
        }
        "setcvar" => {
            let value = dvar_value(args, config, players)?;
            if !key.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_') {
                return Err(script_error(format!("Dvar {key} has an invalid dvar name")));
            }
            let public = args.len() > 2 && int(args, 2)? != 0;
            config.set_from_script(&key, &value);
            if public {
                config.add_flags(&key, crate::config::SCRIPT_ADVERTISED);
            }
            Ok(Value::Undefined)
        }
        _ => unreachable!(),
    }
}

fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

fn squared_distance(a: [f32; 3], b: [f32; 3]) -> f32 {
    let delta = std::array::from_fn(|i| a[i] - b[i]);
    dot(delta, delta)
}

/// None lets the engine dispatch its own bindings. Returns Undefined for a
/// handled GSC function that returns no script value.
pub fn dispatch(
    name: &str,
    args: &[Value],
    config: &mut ServerConfig,
    time_ms: i32,
) -> io::Result<Option<Value>> {
    dispatch_with_players(name, args, config, time_ms, None)
}

pub(super) fn dispatch_with_players(
    name: &str,
    args: &[Value],
    config: &mut ServerConfig,
    time_ms: i32,
    players: Option<&crate::player::Players>,
) -> io::Result<Option<Value>> {
    use Value::{Float, Int};
    let result = match name {
        "getcvar" | "getcvarint" | "getcvarfloat" | "setcvar" | "makecvarserverinfo" => {
            dvar_call(name, args, config, players)?
        }
        "gettime" => Int(time_ms),
        // GScr_issplitscreen 0x08117918 unconditionally returns 0 on lnxded.
        "issplitscreen" => Int(0),
        // lnxded registers the console rumble and matchmaking calls, but their
        // native handlers return immediately (chunk_050, 0x08117906..0x0811793e).
        "playrumbleonpos"
        | "playlooprumbleonpos"
        | "stopallrumbles"
        | "matchend"
        | "setplayerteamrank"
        | "sendranks"
        | "endparty" => Value::Undefined,
        "isvalidgametype" if args.is_empty() => Value::Undefined,
        "isvalidgametype" => Int(i32::from(config.has_gametype(&utf8(bytes(args, 0)?)?))),
        "isstring" => Int(i32::from(matches!(arg(args, 0)?, Value::String(_)))),
        "int" => Int(match arg(args, 0)? {
            Int(i) => *i,
            Float(f) if (-2147483648.0..2147483648.0).contains(f) => f.trunc() as i32,
            Float(_) => i32::MIN,
            Value::String(b) => integer_prefix(&String::from_utf8_lossy(b)),
            v => {
                return Err(script_error(format!(
                    "cannot cast {} to int",
                    v.type_name()
                )));
            }
        }),
        "tolower" => {
            let mut text = bytes(args, 0)?;
            if text.len() > 1023 {
                return Err(script_error("string too long"));
            }
            text.make_ascii_lowercase();
            Value::String(text)
        }
        "issubstr" => {
            let needle = bytes(args, 1)?;
            let haystack = bytes(args, 0)?;
            Int(i32::from(
                needle.is_empty() || haystack.windows(needle.len()).any(|s| s == needle),
            ))
        }
        "getsubstr" => {
            let text = bytes(args, 0)?;
            let start = int(args, 1)?;
            if start < 0 {
                return Err(error("negative substring start"));
            }
            let start = (start as usize).min(text.len());
            let end = if args.len() > 2 {
                int(args, 2)?.max(0) as usize
            } else {
                text.len()
            };
            let slice = &text[start..end.min(text.len()).max(start)];
            if slice.len() > 1024 || (slice.len() == 1024 && end > start + 1024) {
                return Err(script_error("string too long"));
            }
            Value::String(slice.to_vec())
        }
        "strtok" => {
            let source = bytes(args, 0)?;
            let separators = bytes(args, 1)?;
            let mut result = BTreeMap::new();
            for word in source
                .split(|c| separators.contains(c))
                .filter(|s| !s.is_empty())
            {
                if word.len() > 1023 {
                    return Err(script_error("string too long"));
                }
                result.insert(
                    ArrayKey::Int(result.len() as i32),
                    Value::String(word.to_vec()),
                );
            }
            Value::Array(result)
        }
        "sin" | "cos" | "tan" | "asin" | "acos" | "atan" => {
            let x = arg(args, 0)?.number()?;
            Float(match name {
                "sin" => (x as f64).to_radians().sin() as f32,
                "cos" => (x as f64).to_radians().cos() as f32,
                "tan" => {
                    let (s, c) = ((x as f64).to_radians() as f32).sin_cos();
                    if c == 0.0 {
                        return Err(script_error("divide by 0"));
                    }
                    s / c
                }
                "asin" | "acos" if x.abs() > 1. => {
                    return Err(script_error(format!("{x} out of range")));
                }
                _ => {
                    let radians = match name {
                        "acos" => (x as f64).acos(),
                        "asin" => (x as f64).asin(),
                        _ => (x as f64).atan(),
                    } as f32;
                    (radians as f64).to_degrees() as f32
                }
            })
        }
        "vectordot" => Float(dot(vector(args, 0)?, vector(args, 1)?)),
        "lengthsquared" | "length" | "vectornormalize" => {
            let v = vector(args, 0)?;
            let len2 = dot(v, v);
            match name {
                "lengthsquared" => Float(len2),
                "length" => Float(len2.sqrt()),
                _ => {
                    let length = len2.sqrt();
                    Value::Vector(if length != 0. {
                        let inv = 1. / length;
                        v.map(|c| c * inv)
                    } else {
                        v
                    })
                }
            }
        }
        "distance" | "distancesquared" => {
            let d = squared_distance(vector(args, 0)?, vector(args, 1)?);
            Float(if name == "distance" { d.sqrt() } else { d })
        }
        "closer" => {
            let origin = vector(args, 0)?;
            Int(i32::from(
                squared_distance(origin, vector(args, 1)?)
                    < squared_distance(origin, vector(args, 2)?),
            ))
        }
        _ => return Ok(None),
    };
    Ok(Some(result))
}

#[cfg(test)]
mod tests {
    use super::*;
    fn text(s: &str) -> Value {
        Value::String(s.as_bytes().to_vec())
    }

    #[test]
    fn script_dvar_sources_share_values_but_have_separate_exports() {
        let mut config = ServerConfig::from_args(&[]);
        let mut call = |name, values: &[Value]| {
            dispatch(name, values, &mut config, 450)
                .expect("dispatch")
                .expect("recognized")
        };
        call("makecvarserverinfo", &[text("scr_limit"), text("8")]);
        call("makecvarserverinfo", &[text("scr_limit"), text("777")]);
        assert_eq!(call("getcvarint", &[text("scr_limit")]), Value::Int(8));
        call(
            "setcvar",
            &[text("scr_title"), text("hello"), Value::Int(1)],
        );
        call("setcvar", &[text("scr_scale"), text("-0.5e2 ms")]);
        assert_eq!(
            call("getcvarfloat", &[text("scr_scale")]),
            Value::Float(-50.)
        );
        assert_eq!(call("getcvar", &[text("missing")]), text(""));
        assert_eq!(call("gettime", &[]), Value::Int(450));
        assert!(
            config
                .dvars_with_flags(crate::config::SCRIPT_ADVERTISED)
                .any(|(name, _)| name == "scr_title")
        );
        assert!(
            config
                .dvars_with_flags(crate::config::SCRIPT_INFO)
                .any(|(name, _)| name == "scr_limit")
        );
        assert_eq!(config.get("scr_title"), Some("hello"));
    }

    #[test]
    fn strings_keep_byte_indices_and_empty_tokens_are_discarded() {
        let mut config = ServerConfig::from_args(&[]);
        let mut call = |name, values: &[Value]| {
            dispatch(name, values, &mut config, 0)
                .expect("dispatch")
                .expect("recognized")
        };
        let source = Value::String(vec![0xf3, b'A', b'B']);
        assert_eq!(
            call("tolower", std::slice::from_ref(&source)),
            Value::String(vec![0xf3, b'a', b'b'])
        );
        assert_eq!(
            call("getsubstr", &[source, Value::Int(1), Value::Int(3)]),
            text("AB")
        );
        let result = call("strtok", &[text(" a==b "), text(" =")]);
        assert_eq!(result.size().expect("size"), 2);
        assert_eq!(result.index(Value::Int(1)).expect("second"), text("b"));
        assert_eq!(call("issubstr", &[text("abc"), text("")]), Value::Int(1));
    }

    #[test]
    fn math_uses_degrees_and_vector_domain_and_unknowns_are_explicit() {
        let mut config = ServerConfig::from_args(&[]);
        for (name, input, result) in [
            ("asin", 1., 90.),
            ("acos", -1., 180.),
            ("sin", 90., 1.),
            ("atan", 1., 45.),
        ] {
            assert_eq!(
                dispatch(name, &[Value::Float(input)], &mut config, 10).expect("domain"),
                Some(Value::Float(result))
            );
        }
        assert!(dispatch("acos", &[Value::Float(2.)], &mut config, 10).is_err());
        assert_eq!(
            dispatch(
                "vectornormalize",
                &[Value::Vector([0., 0., 0.])],
                &mut config,
                10
            )
            .expect("domain"),
            Some(Value::Vector([0.; 3]))
        );
        assert_eq!(
            dispatch("other", &[], &mut config, 10).expect("delegate"),
            None
        );
    }
}
