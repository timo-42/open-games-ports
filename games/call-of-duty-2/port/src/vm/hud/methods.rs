use super::*;

fn dimensions(args: &[Value]) -> io::Result<[u32; 2]> {
    if args.is_empty() {
        return Ok([0, 0]);
    }
    let [width, height] = args else {
        return Err(script_error("expected width and height"));
    };
    let width = integer(width)?;
    if width < 0 {
        return Err(script_error(format!("width {width} < 0")));
    }
    let height = integer(height)?;
    if height < 0 {
        return Err(script_error(format!("height {height} < 0")));
    }
    Ok([width as u32, height as u32])
}

fn shader(value: &Value, strings: &mut ConfigStrings, initializing: bool) -> io::Result<u32> {
    Ok(super::super::tables::shader(strings, &string(value)?, initializing)? as u32)
}

fn millis(value: &Value) -> io::Result<i32> {
    let t = (1000.0 * value.number()?).ceil();
    Ok(if (-2147483648.0..2147483648.0).contains(&t) {
        t as i32
    } else {
        i32::MIN
    })
}

pub(super) fn call(
    id: u32,
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> io::Result<Value> {
    if name == "destroy" {
        host.world.hud.destroy(id);
        host.world.mark_dead(id);
        return Ok(Value::Undefined);
    }
    let player = if name == "setplayernamestring" {
        let Value::Object(target) = arg(args, 0)? else {
            return Err(script_error("expected entity"));
        };
        let slot = host
            .world
            .entities
            .slot_for_object(*target)
            .ok_or_else(|| script_error("expected entity"))?;
        if slot >= 64 {
            eprintln!(
                "Invalid entity passed to hudelem setplayernamestring(), entity is not a client"
            );
        }
        (slot < 64).then_some(slot)
    } else {
        None
    };
    let index = host.world.hud.by_object[&id];
    let elem = &mut host.world.hud.elements[index];
    let now = host.time_ms;
    match name {
        "reset" => elem.reset(),
        "settext" => {
            let text = arg(args, 0)?;
            if !matches!(text, Value::LocalizedString(_)) {
                return Err(script_error("expected localized string"));
            }
            elem.change_type(1);
            elem.words[TEXT] = localized(text, host.strings, host.initializing)?;
        }
        "setvalue" => {
            let n = arg(args, 0)?.number()?;
            elem.change_type(2);
            elem.words[VALUE] = n.to_bits();
        }
        "setplayernamestring" => {
            if let Some(client) = player {
                elem.change_type(3);
                elem.words[VALUE] = f32::from(client).to_bits();
            }
        }
        "setgametypestring" | "setmapnamestring" => {
            let text = string(arg(args, 0)?)?;
            let name_text =
                std::str::from_utf8(&text).map_err(|_| script_error("invalid game or map name"))?;
            if name == "setgametypestring" {
                if host.config.has_gametype(name_text) {
                    host.strings.set(20, &text)?;
                    elem.change_type(5);
                    elem.words[VALUE] = 20_f32.to_bits();
                } else {
                    eprintln!("Invalid gametype '{name_text}'");
                }
            } else if host.assets.map_exists(name_text) {
                host.strings.set(19, &text)?;
                elem.change_type(4);
            } else {
                eprintln!("Invalid map name passed to hudelem setmapnamestring(), map not found");
            }
        }
        "setshader" => {
            if !matches!(args.len(), 1 | 3) {
                return Err(script_error(
                    "USAGE: <hudelem> setShader(\"shadername\"[, optional_width, optional_height]);",
                ));
            }
            let material = shader(&args[0], host.strings, host.initializing)?;
            let size = dimensions(&args[1..])?;
            elem.change_type(6);
            elem.words[WIDTH..MATERIAL].copy_from_slice(&size);
            elem.words[MATERIAL] = material;
        }
        "settimer" | "settimerup" | "settenthstimer" | "settenthstimerup" => {
            let (kind, method) = match name {
                "settimer" => (7, "setTimer"),
                "settimerup" => (8, "setTimerUp"),
                "settenthstimer" => (9, "setTenthsTimer"),
                _ => (10, "setTenthsTimerUp"),
            };
            let [time] = args else {
                return Err(script_error(format!(
                    "USAGE: <hudelem> {method}(time_in_seconds);\n"
                )));
            };
            let time = millis(time)?;
            if time < 1 && kind != 8 {
                return Err(script_error(format!(
                    "time {} should be > 0",
                    general_float(time as f32 * 0.001)
                )));
            }
            elem.change_type(kind);
            elem.words[TIME] = now.wrapping_add(time) as u32;
        }
        "setclock" | "setclockup" => {
            if !matches!(args.len(), 3 | 5) {
                let method = if name == "setclock" {
                    "setClock"
                } else {
                    "setClockUp"
                };
                return Err(script_error(format!(
                    "USAGE: <hudelem> {method}(time_in_seconds, total_clock_time_in_seconds, shadername[, width, height]);\n"
                )));
            }
            let time = millis(&args[0])?;
            if time < 1 && name == "setclock" {
                return Err(script_error(format!(
                    "time {} should be > 0",
                    general_float(time as f32 * 0.001)
                )));
            }
            let duration = millis(&args[1])?;
            if duration < 1 {
                return Err(script_error(format!(
                    "duration {} should be > 0",
                    general_float(duration as f32 * 0.001)
                )));
            }
            let material = shader(&args[2], host.strings, host.initializing)?;
            let size = dimensions(&args[3..])?;
            elem.change_type(if name == "setclock" { 11 } else { 12 });
            elem.words[WIDTH..MATERIAL].copy_from_slice(&size);
            elem.words[MATERIAL] = material;
            elem.words[TIME] = now.wrapping_add(time) as u32;
            elem.words[DURATION] = duration as u32;
        }
        "setwaypoint" => {
            let n = integer(arg(args, 0)?)?;
            elem.words[KIND] = 13;
            elem.words[VALUE] = (n as f32).to_bits();
        }
        "fadeovertime" | "scaleovertime" | "moveovertime" => {
            if name == "scaleovertime" && args.len() != 3 {
                return Err(script_error(
                    "hudelem scaleOverTime(time_in_seconds, new_width, new_height)",
                ));
            }
            let t = arg(args, 0)?.number()?;
            if t <= 0.0 || t > 60.0 {
                let method = name.trim_end_matches("overtime");
                let limit = if t <= 0.0 { "<= 0" } else { "> 60" };
                return Err(script_error(format!(
                    "{method} time {} {limit}",
                    general_float(t)
                )));
            }
            let duration = (1000.0 * t + 0.5).floor() as u32;
            match name {
                "fadeovertime" => {
                    elem.words[FROM_COLOR] = elem.words[COLOR];
                    elem.words[FADE_START] = now as u32;
                    elem.words[FADE_TIME] = duration;
                }
                "scaleovertime" => {
                    let size = [integer(&args[1])? as u32, integer(&args[2])? as u32];
                    elem.words.copy_within(WIDTH..MATERIAL, FROM_WIDTH);
                    elem.words[WIDTH..MATERIAL].copy_from_slice(&size);
                    elem.words[SCALE_START] = now as u32;
                    elem.words[SCALE_TIME] = duration;
                }
                _ => {
                    elem.words.copy_within(X..Z, FROM_X);
                    elem.words
                        .copy_within(ALIGN_ORIGIN..COLOR, FROM_ALIGN_ORIGIN);
                    elem.words[MOVE_START] = now as u32;
                    elem.words[MOVE_TIME] = duration;
                }
            }
        }
        _ => {
            return Err(super::super::value::error(format!(
                "unknown hudelem method {name}"
            )));
        }
    }
    Ok(Value::Undefined)
}
