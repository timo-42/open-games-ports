//! Server commands for global music and the player's environmental sound mix.
use crate::{
    assets::SoundAliases,
    level::ConfigStrings,
    vm::value::{Value, general_float, script_error},
};
use std::io;

const REVERB: [&[u8]; 2] = [
    b"snd_enveffectsprio_level",
    b"snd_enveffectsprio_shellshock",
];
const CHANNEL: [&[u8]; 3] = [
    b"snd_channelvolprio_holdbreath",
    b"snd_channelvolprio_pain",
    b"snd_channelvolprio_shellshock",
];

fn parameter(args: &[Value], which: usize) -> io::Result<&Value> {
    args.get(which)
        .ok_or_else(|| script_error(format!("parameter {} does not exist", which + 1)))
}
fn text(args: &[Value], which: usize) -> io::Result<Vec<u8>> {
    let mut text = parameter(args, which)?.script_string()?;
    if let Some(end) = text.iter().position(|&byte| byte == 0) {
        text.truncate(end);
    }
    Ok(text)
}

fn optional(args: &[Value], which: usize, default: f32) -> io::Result<f32> {
    args.get(which).map_or(Ok(default), Value::number)
}

fn signed_dword(value: f64) -> i32 {
    let value = value.trunc();
    if (-2147483648.0..2147483648.0).contains(&value) {
        value as i32
    } else {
        i32::MIN
    }
}

pub(crate) fn global(
    name: &str,
    args: &[Value],
    bank: &SoundAliases,
    strings: &mut ConfigStrings,
) -> Option<io::Result<Value>> {
    if !matches!(
        name,
        "musicplay" | "musicstop" | "soundfade" | "soundexists"
    ) {
        return None;
    }
    Some((|| {
        let command = match name {
            "musicplay" => [b"o ".as_slice(), &text(args, 0)?].concat(),
            "musicstop" => {
                if args.len() > 1 {
                    return Err(script_error("USAGE: musicStop([fadetime]);\n"));
                }
                let milliseconds = (1000.0 * optional(args, 0, 0.)?) as f64;
                let fade = signed_dword((milliseconds + 0.5).floor());
                if fade < 0 {
                    return Err(script_error("musicStop: fade time must be >= 0\n"));
                }
                format!("p {fade}").into_bytes()
            }
            "soundfade" => {
                let volume = parameter(args, 0)?.number()?;
                let fade = signed_dword(1000.0 * f64::from(optional(args, 1, 0.)?));
                let volume = if volume.is_finite() {
                    format!("{volume:.6}")
                } else {
                    general_float(volume)
                };
                format!("q {volume} {fade}\n").into_bytes()
            }
            _ => {
                let alias = text(args, 0)?;
                let exists = std::str::from_utf8(&alias)
                    .ok()
                    .is_some_and(|name| bank.find(name).is_some());
                return Ok(Value::Int(i32::from(exists)));
            }
        };
        strings.broadcast(command);
        Ok(Value::Undefined)
    })())
}

fn priority(args: &[Value], reverb: bool) -> io::Result<usize> {
    let priorities = if reverb { &REVERB[..] } else { &CHANNEL[..] };
    let requested = text(args, 0)?;
    if let Some(index) = priorities.iter().position(|p| *p == requested) {
        return Ok(index + 1);
    }
    Err(script_error(if reverb {
        "priority must be 'snd_enveffectsprio_level' or 'snd_enveffectsprio_shellshock'\n"
    } else {
        "priority must be 'snd_channelvolprio_holdbreath', 'snd_channelvolprio_pain', or 'snd_channelvolprio_shellshock'\n"
    }))
}

/// G_FindConfigstringIndex with create=0 and a null error prefix returns zero
/// for a missing shellshock name, and stops at the first unused slot.
fn shellshock(strings: &ConfigStrings, name: &[u8]) -> usize {
    if name.is_empty() {
        return 0;
    }
    (1..16)
        .take_while(|&i| strings.get(0x48e + i).is_some_and(|s| !s.is_empty()))
        .find(|&i| {
            strings
                .get(0x48e + i)
                .is_some_and(|s| s.eq_ignore_ascii_case(name))
        })
        .unwrap_or(0)
}

pub(crate) fn player_command(
    name: &str,
    args: &[Value],
    strings: &ConfigStrings,
) -> Option<io::Result<Vec<u8>>> {
    if !matches!(
        name,
        "setreverb" | "deactivatereverb" | "setchannelvolumes" | "deactivatechannelvolumes"
    ) {
        return None;
    }
    Some((|| match name {
        "setreverb" => {
            if !(2..=5).contains(&args.len()) {
                return Err(script_error(
                    "USAGE: player setReverb(\"priority\", \"roomtype\", drylevel = 1.0, wetlevel = 0.5, fadetime = 0);\nValid priorities are \"snd_enveffectsprio_level\" or \"snd_enveffectsprio_shellshock\", dry level is a float from 0 (no source sound) to 1 (full source sound), wetlevel is a float from 0 (no effect) to 1 (full effect), fadetime is in sec and modifies drylevel and wetlevel\n",
                ));
            }
            let fade = general_float(optional(args, 4, 0.)?);
            let wet = general_float(optional(args, 3, 0.5)?);
            let dry = general_float(optional(args, 2, 1.)?);
            let room = text(args, 1)?;
            let priority = priority(args, true)?;
            Ok([
                format!("r {priority} \"").as_bytes(),
                &room,
                format!("\" {dry} {wet} {fade}").as_bytes(),
            ]
            .concat())
        }
        "setchannelvolumes" => {
            if !(2..=3).contains(&args.len()) {
                return Err(script_error(
                    "USAGE: player setchannelvolumes(\"priority\", \"shock name\", fadetime = 0);\nValid priorities are \"snd_channelvolprio_holdbreath\", \"snd_channelvolprio_pain\", or \"snd_channelvolprio_shellshock\", fadetime is in sec\n",
                ));
            }
            let fade = general_float(optional(args, 2, 0.)?);
            let shock = shellshock(strings, &text(args, 1)?);
            let priority = priority(args, false)?;
            Ok(format!("E {priority} {shock} {fade}").into_bytes())
        }
        _ => {
            if !(1..=2).contains(&args.len()) {
                return Err(script_error(if name == "deactivatereverb" {
                    "USAGE: player deactivateReverb(\"priority\", fadetime = 0);\nValid priorities are \"snd_enveffectsprio_level\" or \"snd_enveffectsprio_shellshock\", fadetime is the time spent fading to the next lowest active reverb priority level in seconds\n"
                } else {
                    "USAGE: player deactivatechannelvolumes(\"priority\", fadetime = 0);\nValid priorities are \"snd_channelvolprio_holdbreath\", \"snd_channelvolprio_pain\", or \"snd_channelvolprio_shellshock\", fadetime is the time spent fading to the next lowest active reverb priority level in seconds\n"
                }));
            }
            let reverb = name == "deactivatereverb";
            let fade = general_float(optional(args, 1, 0.)?);
            let priority = priority(args, reverb)?;
            // The Windows multiplayer client accepts exactly opcode, priority and fade.
            // The 1.3 i386 servers pass these three values to setReverb's longer format.
            let opcode = if reverb { 'D' } else { 'F' };
            Ok(format!("{opcode} {priority} {fade}").into_bytes())
        }
    })())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::level::ServerUpdate;
    fn s(bytes: &[u8]) -> Value {
        Value::String(bytes.to_vec())
    }

    #[test]
    fn music_alias_lookup_and_broadcast_bytes() -> io::Result<()> {
        let mut strings = ConfigStrings::default();
        let mut bank = SoundAliases::default();
        bank.aliases.insert("AMBIENT_AFRICA".into(), Vec::new());
        for (name, args, expected) in [
            ("musicplay", vec![s(b"theme")], b"o theme".as_slice()),
            ("musicstop", vec![], b"p 0"),
            ("musicstop", vec![Value::Float(0.0625)], b"p 63"),
            (
                "soundfade",
                vec![Value::Float(0.25), Value::Float(-0.0625)],
                b"q 0.250000 -62\n",
            ),
        ] {
            global(name, &args, &bank, &mut strings).expect("registered sound command")?;
            let changes = strings.take_game_updates();
            assert!(
                matches!(&changes[..], [ServerUpdate::Broadcast(command)] if command==expected)
            );
        }
        assert_eq!(
            global("soundexists", &[s(b"ambient_africa")], &bank, &mut strings)
                .expect("registered sound command")?,
            Value::Int(1)
        );
        assert_eq!(
            global("soundexists", &[s(b"missing")], &bank, &mut strings)
                .expect("registered sound command")?,
            Value::Int(0)
        );
        assert!(
            global("musicstop", &[Value::Float(-1.)], &bank, &mut strings)
                .expect("registered sound command")
                .is_err()
        );
        assert!(strings.take_game_updates().is_empty());
        Ok(())
    }
    #[test]
    fn targeted_mix_uses_original_priority_and_shellshock_tables() -> io::Result<()> {
        let mut cs = ConfigStrings::default();
        cs.set(0x48f, b"grenade")?;
        for (name, args, expected) in [
            (
                "setreverb",
                vec![s(REVERB[0]), s(b"cave")],
                b"r 1 \"cave\" 1 0.5 0".as_slice(),
            ),
            (
                "setreverb",
                vec![
                    s(REVERB[1]),
                    s(b"hall"),
                    Value::Float(0.25),
                    Value::Int(0),
                    Value::Float(0.75),
                ],
                b"r 2 \"hall\" 0.25 0 0.75",
            ),
            (
                "setchannelvolumes",
                vec![s(CHANNEL[2]), s(b"Grenade"), Value::Float(0.125)],
                b"E 3 1 0.125",
            ),
            (
                "setchannelvolumes",
                vec![s(CHANNEL[1]), s(b"missing")],
                b"E 2 0 0",
            ),
            ("deactivatereverb", vec![s(REVERB[0])], b"D 1 0"),
            (
                "deactivatechannelvolumes",
                vec![s(CHANNEL[1]), Value::Float(0.25)],
                b"F 2 0.25",
            ),
        ] {
            assert_eq!(
                player_command(name, &args, &cs).expect("registered sound command")?,
                expected
            );
        }
        assert!(
            player_command("setchannelvolumes", &[s(b"invalid"), s(b"grenade")], &cs)
                .expect("registered sound command")
                .is_err()
        );
        Ok(())
    }
}
