//! Server objective table and its GSC accessors (chunks 049, 050 and 105).
use super::game_entities::GameEntities;
use super::value::{Value, error, script_error};
use crate::level::ConfigStrings;
use crate::snapshot::PlayerState;
use std::io;

const ENTITY_NONE: u16 = 1023;

#[derive(Debug, Clone, Copy, PartialEq)]
struct Objective {
    state: u32,
    origin: [f32; 3],
    entity: u16,
    team: u32,
    icon: u32,
}

impl Default for Objective {
    fn default() -> Self {
        Self {
            state: 0,
            origin: [0.; 3],
            entity: ENTITY_NONE,
            team: 0,
            icon: 0,
        }
    }
}

impl Objective {
    fn words(self) -> [u32; 7] {
        let [x, y, z] = self.origin.map(f32::to_bits);
        [
            self.state,
            x,
            y,
            z,
            self.entity.into(),
            self.team,
            self.icon,
        ]
    }

    fn detach(&mut self, entities: &mut GameEntities) {
        if let Some(ent) = entities.get_mut(self.entity) {
            ent.sv_flags &= !0x10;
        }
        self.entity = ENTITY_NONE;
    }
}

#[derive(Debug, Default)]
pub struct Objectives([Objective; 16]);

impl Objectives {
    /// G_CopyTurretEntries at 0x08109ce0 updates persistent playerstate.
    /// When an objective disappears, its other six words retain the last visible values.
    pub fn apply(&self, player: &mut PlayerState, team: i32) {
        let slots = &mut player.words_mut()[0x5e4 / 4..0x7a4 / 4];
        for (objective, dest) in self.0.iter().zip(slots.as_chunks_mut::<7>().0.iter_mut()) {
            if objective.state == 0 || (objective.team != 0 && objective.team != team as u32) {
                dest[0] = 0;
            } else {
                dest.copy_from_slice(&objective.words());
            }
        }
    }

    fn current(&mut self, args: &[Value]) -> io::Result<()> {
        let mut selected = [false; 16];
        for arg in args {
            selected[index(arg)?] = true;
        }
        for (objective, selected) in self.0.iter_mut().zip(selected) {
            if selected {
                objective.state = 4;
            } else if objective.state == 4 {
                objective.state = 1;
            }
        }
        Ok(())
    }
}

fn parameter(args: &[Value], n: usize) -> io::Result<&Value> {
    args.get(n)
        .ok_or_else(|| script_error(format!("parameter {} does not exist", n + 1)))
}

fn index(value: &Value) -> io::Result<usize> {
    match *value {
        Value::Int(i @ 0..16) => Ok(i as usize),
        Value::Int(i) => Err(script_error(format!(
            "index {i} is an illegal objective index. Valid indexes are 0 to 15"
        ))),
        _ => Err(script_error("objective index must be an int")),
    }
}

fn state(value: &Value) -> io::Result<u32> {
    match value.script_string()?.as_slice() {
        b"empty" => Ok(0),
        b"invisible" => Ok(2),
        b"current" => Ok(4),
        _ => Err(script_error(
            "illegal objective state, expected empty, invisible, or current",
        )),
    }
}

fn position(value: &Value) -> io::Result<[f32; 3]> {
    let Value::Vector(v) = value else {
        return Err(script_error("objective position must be a vector"));
    };
    Ok(v.map(|x| {
        if (-2147483648.0..2147483648.0).contains(&x) {
            (x as i32) as f32
        } else {
            i32::MIN as f32
        }
    }))
}

pub fn shader(strings: &mut ConfigStrings, name: &[u8], initializing: bool) -> io::Result<i32> {
    super::precache::index(
        strings,
        0x61e,
        0x80,
        &name.to_ascii_lowercase(),
        initializing,
        "shader",
    )
}

fn icon(value: &Value, strings: &mut ConfigStrings, initializing: bool) -> io::Result<u32> {
    let name = value.script_string()?;
    let name = name.split(|&b| b == 0).next().unwrap_or_default();
    if name.len() > 63 || name.iter().any(|&b| !(0x20..=0x7e).contains(&b)) {
        return Err(script_error("invalid objective icon name"));
    }
    Ok(shader(strings, name, initializing)? as u32)
}

fn update(
    name: &str,
    args: &[Value],
    objectives: &mut Objectives,
    entities: &mut GameEntities,
    strings: &mut ConfigStrings,
    initializing: bool,
) -> io::Result<Value> {
    if name == "objective_current" {
        objectives.current(args)?;
        return Ok(Value::Undefined);
    }
    let obj = &mut objectives.0[index(parameter(args, 0)?)?];
    match name {
        "objective_add" => {
            obj.detach(entities);
            obj.state = state(parameter(args, 1)?)?;
            if let Some(pos) = args.get(2) {
                obj.origin = position(pos)?;
                if let Some(icon_arg) = args.get(3) {
                    obj.icon = icon(icon_arg, strings, initializing)?;
                }
            }
            obj.team = 0;
        }
        "objective_delete" => {
            obj.detach(entities);
            *obj = Objective::default();
        }
        "objective_state" => {
            obj.state = state(parameter(args, 1)?)?;
            if matches!(obj.state, 0 | 2) {
                obj.detach(entities);
            }
        }
        "objective_icon" => obj.icon = icon(parameter(args, 1)?, strings, initializing)?,
        "objective_position" => {
            obj.detach(entities);
            obj.origin = position(parameter(args, 1)?)?;
        }
        "objective_onentity" => {
            obj.detach(entities);
            let Value::Object(id) = parameter(args, 1)? else {
                return Err(script_error("objective target must be an entity"));
            };
            let (slot, ent) = entities
                .for_object_mut(*id)
                .ok_or_else(|| error("objective target must be an entity"))?;
            ent.sv_flags |= 0x10;
            obj.entity = slot;
        }
        "objective_team" => {
            obj.team = match parameter(args, 1)?.script_string()?.as_slice() {
                b"none" => 0,
                b"axis" => 1,
                b"allies" => 2,
                _ => {
                    return Err(script_error(
                        "illegal team string, expected allies, axis, or none",
                    ));
                }
            };
        }
        _ => unreachable!(),
    }
    Ok(Value::Undefined)
}

pub fn dispatch(
    name: &str,
    args: &[Value],
    objectives: &mut Objectives,
    entities: &mut GameEntities,
    strings: &mut ConfigStrings,
    initializing: bool,
) -> Option<io::Result<Value>> {
    match name {
        "objective_add" | "objective_delete" | "objective_state" | "objective_icon"
        | "objective_position" | "objective_onentity" | "objective_current" | "objective_team" => {
            Some(update(
                name,
                args,
                objectives,
                entities,
                strings,
                initializing,
            ))
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn objective_parameter_faults_reach_the_vm_recovery_path() {
        // Both calls return undefined and the next statement runs on lnxded 1.3.
        for (name, args) in [
            (
                "objective_add",
                [Value::Int(23), Value::String(b"current".to_vec())],
            ),
            (
                "objective_team",
                [Value::Int(2), Value::String(b"bad team".to_vec())],
            ),
        ] {
            let err = update(
                name,
                &args,
                &mut Objectives::default(),
                &mut GameEntities::default(),
                &mut ConfigStrings::default(),
                false,
            )
            .expect_err("invalid objective parameter");
            let fault = super::super::value::ScriptFault::from_error(&err)
                .expect("recoverable script error");
            assert_eq!(fault.result, Value::Undefined);
        }
    }

    #[test]
    fn teams_keep_last_visible_objective_payload_and_current_selection_changes_state()
    -> io::Result<()> {
        let mut objectives = Objectives::default();
        let mut entities = GameEntities::default();
        let mut strings = ConfigStrings::default();
        let mut player = PlayerState::default();
        let s = |s: &[u8]| Value::String(s.to_vec());
        let i = Value::Int(3);
        let range = 0x5e4 / 4 + 3 * 7..0x5e4 / 4 + 4 * 7;
        update(
            "objective_add",
            &[
                i.clone(),
                s(b"current"),
                Value::Vector([12.9, -10.7, 0.]),
                s(b"Hud_Bomb"),
            ],
            &mut objectives,
            &mut entities,
            &mut strings,
            true,
        )?;
        update(
            "objective_team",
            &[i.clone(), s(b"axis")],
            &mut objectives,
            &mut entities,
            &mut strings,
            true,
        )?;
        objectives.apply(&mut player, 1);
        assert_eq!(
            player.words()[range.clone()],
            [4, 12f32.to_bits(), (-10f32).to_bits(), 0, 1023, 1, 1]
        );
        assert_eq!(strings.get(0x61f), Some(b"hud_bomb".as_slice()));
        objectives.apply(&mut player, 2);
        assert_eq!(
            player.words()[range.clone()],
            [0, 12f32.to_bits(), (-10f32).to_bits(), 0, 1023, 1, 1]
        );
        objectives.current(&[])?;
        objectives.apply(&mut player, 1);
        assert_eq!(player.words()[range.start], 1);
        update(
            "objective_delete",
            &[i],
            &mut objectives,
            &mut entities,
            &mut strings,
            false,
        )?;
        assert_eq!(objectives.0[3], Objective::default());
        Ok(())
    }
}
