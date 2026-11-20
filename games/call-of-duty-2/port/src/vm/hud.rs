//! Script HUD element pool, player menu commands and projection to playerState.
//! HudElem_Alloc (0x0810336a) and HudElem_BuildClientLists (0x08104882).
mod fields;
mod methods;

use super::{
    GameHost, ScriptWorld,
    value::{Value, general_float, script_error},
};
use crate::{level::ConfigStrings, snapshot::PlayerState};
use std::{collections::BTreeMap, io};

// Offsets in the 128-byte hudelem_t sent to the client, in dwords.
const KIND: usize = 0;
const X: usize = 1;
const Y: usize = 2;
const Z: usize = 3;
const FONT_SCALE: usize = 4;
const FONT: usize = 5;
const ALIGN_ORIGIN: usize = 6;
const ALIGN_SCREEN: usize = 7;
const COLOR: usize = 8;
const FROM_COLOR: usize = 9;
const FADE_START: usize = 10;
const FADE_TIME: usize = 11;
const LABEL: usize = 12;
const WIDTH: usize = 13;
const MATERIAL: usize = 15;
const FROM_WIDTH: usize = 16;
const SCALE_START: usize = 18;
const SCALE_TIME: usize = 19;
const FROM_X: usize = 20;
const FROM_ALIGN_ORIGIN: usize = 22;
const MOVE_START: usize = 24;
const MOVE_TIME: usize = 25;
const TIME: usize = 26;
const DURATION: usize = 27;
const VALUE: usize = 28;
const TEXT: usize = 29;
const SORT: usize = 30;
const FOREGROUND: usize = 31;

#[derive(Debug, Default)]
struct Element {
    words: [u32; 32],
    owner: Option<u16>,
    team: u8,
    archived: i32,
}

impl Element {
    // HudElem_Init leaves moveStartTime and moveTime in the pool slot across reset/reuse.
    fn reset(&mut self) {
        self.words[..MOVE_START].fill(0);
        self.words[TIME..].fill(0);
        self.words[KIND] = 1;
        self.words[FONT_SCALE] = 1_f32.to_bits();
        self.words[COLOR] = u32::MAX;
        self.archived = 1;
    }

    fn change_type(&mut self, kind: u32) {
        self.words[WIDTH..MOVE_START].fill(0);
        self.words[TIME..SORT].fill(0);
        self.words[KIND] = kind;
    }
}

#[derive(Debug, Default)]
pub struct HudElements {
    elements: Vec<Element>,
    by_object: BTreeMap<u32, usize>,
}

impl HudElements {
    pub(super) fn contains_object(&self, object: u32) -> bool {
        self.by_object.contains_key(&object)
    }

    fn allocate(&mut self, object: u32, owner: Option<u16>, team: u8) {
        let slot = self
            .elements
            .iter()
            .position(|e| e.words[KIND] == 0)
            .unwrap_or(self.elements.len());
        debug_assert!(slot < 1024);
        if slot == self.elements.len() {
            self.elements.push(Element::default());
        }
        let element = &mut self.elements[slot];
        element.reset();
        element.team = team;
        element.owner = owner;
        self.by_object.insert(object, slot);
    }

    fn destroy(&mut self, object: u32) {
        if let Some(slot) = self.by_object.remove(&object) {
            self.elements[slot].words[KIND] = 0;
        }
    }

    pub(super) fn free_client(&mut self, slot: u16) -> Vec<u32> {
        let mut removed = Vec::new();
        self.by_object.retain(|id, index| {
            let element = &mut self.elements[*index];
            if element.owner == Some(slot) {
                removed.push(*id);
                element.words[KIND] = 0;
                false
            } else {
                true
            }
        });
        removed
    }

    pub fn write_player_state(&self, client: u16, team: u8, flags: u8, player: &mut PlayerState) {
        for (flag, archived, offset) in [(1, true, 0x1728 / 4), (2, false, 0x7a8 / 4)] {
            if flags & flag == 0 {
                continue;
            }
            let out = &mut player.words_mut()[offset..offset + 31 * 32];
            out.fill(0);
            let visible = self.elements.iter().filter(|element| {
                element.words[KIND] != 0
                    && (element.archived != 0) == archived
                    && (element.team == 0 || element.team == team)
                    && element.owner.is_none_or(|owner| owner == client)
            });
            for (target, element) in out.as_chunks_mut::<32>().0.iter_mut().zip(visible) {
                target.copy_from_slice(&element.words);
            }
        }
    }
}

fn arg(args: &[Value], index: usize) -> io::Result<&Value> {
    args.get(index)
        .ok_or_else(|| script_error(format!("parameter {} does not exist", index + 1)))
}

fn integer(value: &Value) -> io::Result<i32> {
    match value {
        Value::Int(n) => Ok(*n),
        _ => Err(script_error(format!(
            "type {} is not an int",
            value.type_name()
        ))),
    }
}

fn string(value: &Value) -> io::Result<Vec<u8>> {
    Ok(value
        .script_string()?
        .into_iter()
        .take_while(|&c| c != 0)
        .collect())
}

fn localized(value: &Value, strings: &mut ConfigStrings, initializing: bool) -> io::Result<u32> {
    let Value::LocalizedString(name) = value else {
        return Err(script_error("expected a localized string"));
    };
    let name = name.split(|&c| c == 0).next().unwrap_or_default();
    Ok(super::precache::index(
        strings,
        0x51e,
        0x100,
        name,
        initializing,
        "localized string",
    )? as u32)
}

fn player_slot(value: &Value, world: &ScriptWorld) -> io::Result<u16> {
    let Value::Object(id) = value else {
        return Err(script_error("expected a player entity"));
    };
    world
        .entities
        .slot_for_object(*id)
        .filter(|&slot| slot < 64)
        .ok_or_else(|| script_error("not a client"))
}

pub(super) fn dispatch(
    receiver: Option<u32>,
    name: &str,
    args: &[Value],
    host: &mut GameHost<'_>,
) -> Option<io::Result<Value>> {
    if let Some(object) = receiver {
        if !host.world.hud.by_object.contains_key(&object) {
            return None;
        }
        return Some(methods::call(object, name, args, host));
    }
    if !matches!(name, "newhudelem" | "newclienthudelem" | "newteamhudelem") {
        return None;
    }
    Some((|| {
        let mut owner = None;
        let mut team = 0;
        if name == "newclienthudelem" {
            owner = Some(player_slot(arg(args, 0)?, host.world)?);
        } else if name == "newteamhudelem" {
            let name = string(arg(args, 0)?)?;
            team = match name.as_slice() {
                b"axis" => 1,
                b"allies" => 2,
                b"spectator" => 3,
                _ => {
                    return Err(script_error(format!(
                        "team \"{}\" should be \"allies\", \"axis\", or \"spectator\"",
                        String::from_utf8_lossy(&name)
                    )));
                }
            };
        }
        if host.world.hud.by_object.len() == 1024 {
            return Err(script_error("out of hudelems"));
        }
        let id = host.world.allocate_object()?;
        host.world.hud.allocate(id, owner, team);
        Ok(Value::Object(id))
    })())
}

type MenuResult = io::Result<(Value, Option<Vec<u8>>)>;
pub fn menu_command(
    name: &str,
    args: &[Value],
    strings: &ConfigStrings,
    connected: bool,
) -> Option<MenuResult> {
    Some(match name {
        "openmenu" | "openmenunomouse" if !connected => Ok((Value::Int(0), None)),
        "openmenu" | "openmenunomouse" => (|| {
            let menu = string(arg(args, 0)?)?;
            let index = (0..32)
                .find(|i| {
                    strings
                        .get(0x4de + i)
                        .is_some_and(|s| s[..1023.min(s.len())].eq_ignore_ascii_case(&menu))
                })
                .ok_or_else(|| {
                    script_error(format!(
                        "Menu '{}' was not precached\n",
                        String::from_utf8_lossy(&menu)
                    ))
                })?;
            let extra = if name == "openmenunomouse" { " 1" } else { "" };
            Ok((
                Value::Int(1),
                Some(format!("t {index}{extra}").into_bytes()),
            ))
        })(),
        "closemenu" => Ok((Value::Undefined, Some(b"u".to_vec()))),
        "closeingamemenu" => Ok((Value::Undefined, Some(b"K".to_vec()))),
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn projection_filters_by_owner_and_team_and_caps_each_list_independently() {
        let mut pool = HudElements::default();
        for id in 0..40 {
            pool.allocate(id, None, 0);
            pool.elements[id as usize].words[VALUE] = id;
        }
        pool.elements[1].team = 1;
        pool.elements[2].owner = Some(5);
        pool.elements[3].archived = 0;
        let mut ps = PlayerState::default();
        ps.words_mut()[0x7a8 / 4] = 13;
        pool.write_player_state(0, 2, 1, &mut ps);
        assert_eq!(ps.words()[0x7a8 / 4], 13);
        let archival = &ps.words()[0x1728 / 4..];
        assert_eq!(archival[VALUE], 0);
        assert_eq!(archival[32 + VALUE], 4);
        assert_eq!(archival[30 * 32 + VALUE], 33);
        pool.write_player_state(0, 2, 2, &mut ps);
        assert_eq!(ps.words()[0x7a8 / 4 + VALUE], 3);
        assert_eq!(ps.words()[0x7a8 / 4 + 32], 0);
        assert_eq!(pool.free_client(5), [2]);
        pool.allocate(40, Some(6), 0);
        assert_eq!(pool.by_object.get(&40), Some(&2));
        assert!(!pool.by_object.contains_key(&2));
    }

    #[test]
    fn alignment_and_color_use_independent_packed_channels() -> io::Result<()> {
        let mut pool = HudElements::default();
        let mut strings = ConfigStrings::default();
        pool.allocate(7, None, 0);
        for (field, value) in [
            ("alignx", Value::String(b"RIGHT".to_vec())),
            ("aligny", Value::String(b"middle".to_vec())),
            ("alpha", Value::Float(0.2)),
            ("color", Value::Vector([0.25, 0.5, 1.0])),
        ] {
            pool.set_field(7, field, &value, &mut strings, true)
                .expect("registered HUD binding")?;
        }
        assert_eq!(pool.elements[0].words[ALIGN_ORIGIN], 9);
        assert_eq!(pool.elements[0].words[COLOR], 0x33ff8040);
        assert_eq!(
            pool.field(7, "alignx"),
            Some(Value::String(b"right".to_vec()))
        );
        for (field, value) in [
            ("alignx", Value::String(b"unknown".to_vec())),
            ("fontscale", Value::Float(0.0)),
        ] {
            assert!(
                pool.set_field(7, field, &value, &mut strings, true)
                    .expect("registered HUD binding")
                    .is_err()
            );
        }
        Ok(())
    }

    #[test]
    fn menu_sends_zero_based_precache_index_after_connect() -> io::Result<()> {
        let mut strings = ConfigStrings::default();
        strings.set(0x4de, b"team")?;
        strings.set(0x4df, b"weapon")?;
        assert_eq!(
            menu_command("openmenu", &[], &strings, false).expect("registered HUD binding")?,
            (Value::Int(0), None)
        );
        assert_eq!(
            menu_command(
                "openmenunomouse",
                &[Value::String(b"WEAPON".to_vec())],
                &strings,
                true
            )
            .expect("registered HUD binding")?,
            (Value::Int(1), Some(b"t 1 1".to_vec()))
        );
        assert!(
            menu_command(
                "openmenu",
                &[Value::String(b"missing".to_vec())],
                &strings,
                true
            )
            .expect("registered HUD binding")
            .is_err()
        );
        Ok(())
    }
}
