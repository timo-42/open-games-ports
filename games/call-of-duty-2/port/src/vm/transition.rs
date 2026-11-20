//! Script-requested server commands run after the current game frame.
use super::{GameHost, ScriptWorld, value::Value};
use std::collections::BTreeMap;
use std::io;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MapCommand {
    Restart,
    Load(String),
    Exit,
}

impl MapCommand {
    pub fn tokens(&self) -> Vec<String> {
        match self {
            Self::Restart => vec!["fast_restart".into()],
            Self::Load(name) => vec!["map".into(), name.clone()],
            Self::Exit => vec!["map_rotate".into()],
        }
    }
}

pub(super) fn dispatch(
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    let command = match name {
        "map_restart" => MapCommand::Restart,
        "exitlevel" => MapCommand::Exit,
        "map" => {
            let Some(value) = args.first() else {
                return Some(Ok(Value::Undefined));
            };
            let map = match value
                .script_string()
                .and_then(|v| String::from_utf8(v).map_err(|_| super::invalid("invalid map name")))
            {
                Ok(map) => map,
                Err(e) => return Some(Err(e)),
            };
            if !host.assets.map_exists(&map) {
                return Some(Ok(Value::Undefined));
            }
            MapCommand::Load(map)
        }
        _ => return None,
    };
    if let Some(previous) = &host.world.map_command {
        let message = match (previous, &command) {
            (MapCommand::Restart, MapCommand::Restart | MapCommand::Exit) => {
                "map_restart already called"
            }
            (MapCommand::Load(_), MapCommand::Load(_)) => "map already called",
            _ => "exitlevel already called",
        };
        return Some(Err(super::invalid(message)));
    }
    let keep_arg = usize::from(matches!(command, MapCommand::Load(_)));
    host.world.keep_map_state = match args.get(keep_arg) {
        None => false,
        Some(Value::Int(value)) => *value != 0,
        Some(_) => return Some(Err(super::invalid("map state argument must be an int"))),
    };
    if command == MapCommand::Exit {
        host.world.players.exit_level();
    }
    host.world.map_command = Some(command);
    Some(Ok(Value::Undefined))
}

/// Scr_PruneArrayChildren drops handles to level-scoped objects and code.
fn persistent_value(value: &Value) -> Option<Value> {
    match value {
        Value::Object(_) | Value::Function { .. } | Value::Animation(_) => None,
        Value::Array(entries) => Some(Value::Array(
            entries
                .iter()
                .filter_map(|(k, v)| Some((k.clone(), persistent_value(v)?)))
                .collect(),
        )),
        value => Some(value.clone()),
    }
}

impl ScriptWorld {
    /// Unlike the game array, client persistent arrays survive without pruning.
    /// Scr_SweepEntityVars clears the fields in surviving struct references.
    /// A freed entity remains an invalid entity handle, even if its slot is reused.
    fn restore_player_value(
        &mut self,
        value: &Value,
        old: &Self,
        objects: &mut BTreeMap<u32, u32>,
    ) -> io::Result<Value> {
        match value {
            Value::Object(id) => {
                if let Some(restored) = objects.get(id) {
                    return Ok(Value::Object(*restored));
                }
                let restored = self.allocate_object()?;
                if old.objects.is_released(*id)
                    || old.entities.slot_for_object(*id).is_some()
                    || old.hud.contains_object(*id)
                {
                    self.mark_dead(restored);
                }
                objects.insert(*id, restored);
                Ok(Value::Object(restored))
            }
            Value::Array(entries) => {
                let mut array = BTreeMap::new();
                for (key, value) in entries {
                    array.insert(key.clone(), self.restore_player_value(value, old, objects)?);
                }
                Ok(Value::Array(array))
            }
            value => Ok(value.clone()),
        }
    }

    pub(crate) fn restore_map_state(&mut self, old: &Self) -> io::Result<BTreeMap<u16, Value>> {
        self.game = persistent_value(&old.game).unwrap_or_else(|| Value::Array(Default::default()));
        let mut objects = BTreeMap::new();
        let mut clients = BTreeMap::new();
        for (slot, player) in old.players.iter() {
            let value = old.field(player.object, "pers")?;
            clients.insert(slot, self.restore_player_value(&value, old, &mut objects)?);
        }
        Ok(clients)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vm::value::ArrayKey;
    #[test]
    fn player_pers_retains_struct_identity_and_invalidates_old_entity_references() -> io::Result<()>
    {
        let mut old = ScriptWorld::default();
        let object = old.allocate_object()?;
        old.set_field(object, "foo", Value::Int(73))?;
        let (_, entity) = old.spawn_entity(b"script_origin", [0.; 3], 100)?;
        let mut new = ScriptWorld::default();
        let mut remap = BTreeMap::new();
        let copy = new.restore_player_value(&Value::Object(object), &old, &mut remap)?;
        assert_eq!(
            copy,
            new.restore_player_value(&Value::Object(object), &old, &mut remap)?
        );
        let Value::Object(copy) = copy else {
            unreachable!()
        };
        assert_eq!(new.field(copy, "foo")?, Value::Undefined);
        assert!(new.objects.live_fields(copy).is_some());
        let Value::Object(freed) =
            new.restore_player_value(&Value::Object(entity), &old, &mut remap)?
        else {
            unreachable!()
        };
        assert!(new.objects.is_released(freed));
        Ok(())
    }

    #[test]
    fn level_only_objects_and_code_are_removed_from_nested_persistent_data() {
        let scalar = ArrayKey::String(b"score".to_vec());
        let nested = ArrayKey::String(b"round".to_vec());
        let data = Value::Array(BTreeMap::from([
            (scalar.clone(), Value::Int(2)),
            (
                nested.clone(),
                Value::Array(BTreeMap::from([
                    (ArrayKey::Int(1), Value::Object(42)),
                    (ArrayKey::Int(2), Value::Int(100)),
                    (
                        ArrayKey::Int(3),
                        Value::Function {
                            module: None,
                            name: "run".into(),
                        },
                    ),
                ])),
            ),
        ]));
        let pruned = Value::Array(BTreeMap::from([
            (scalar, Value::Int(2)),
            (
                nested,
                Value::Array(BTreeMap::from([(ArrayKey::Int(2), Value::Int(100))])),
            ),
        ]));
        assert_eq!(persistent_value(&data), Some(pruned));
    }
}
