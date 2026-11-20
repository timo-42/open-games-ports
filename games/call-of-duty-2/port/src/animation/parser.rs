//! Line-oriented playeranim.script grammar and multiplayer.atr node numbering.
use super::{Animation, Command, Item, Part, PlayerAnimations, invalid};
use crate::assets::{GameAssets, SoundAliases};
use std::{
    collections::{BTreeMap, BTreeSet},
    io,
};

pub(super) const MOVES: &[&str] = &[
    "",
    "idle",
    "idlecr",
    "idleprone",
    "walk",
    "walkbk",
    "walkcr",
    "walkcrbk",
    "walkprone",
    "walkpronebk",
    "run",
    "runbk",
    "runcr",
    "runcrbk",
    "turnright",
    "turnleft",
    "turnrightcr",
    "turnleftcr",
    "climbup",
    "climbdown",
    "mantle_root",
    "mantle_up_57",
    "mantle_up_51",
    "mantle_up_45",
    "mantle_up_39",
    "mantle_up_33",
    "mantle_up_27",
    "mantle_up_21",
    "mantle_over_high",
    "mantle_over_mid",
    "mantle_over_low",
    "flinch_forward",
    "flinch_backward",
    "flinch_left",
    "flinch_right",
    "stumble_forward",
    "stumble_backward",
    "stumble_walk_forward",
    "stumble_walk_backward",
    "stumble_crouch_forward",
    "stumble_crouch_backward",
];
const EVENTS: &[&str] = &[
    "pain",
    "death",
    "fireweapon",
    "jump",
    "jumpbk",
    "land",
    "dropweapon",
    "raiseweapon",
    "climbmount",
    "climbdismount",
    "reload",
    "crouch_to_prone",
    "prone_to_crouch",
    "stand_to_crouch",
    "crouch_to_stand",
    "stand_to_prone",
    "prone_to_stand",
    "meleeattack",
    "shellshock",
];
const STATES: &[&str] = &["relaxed", "query", "alert", "combat"];
const CONDITIONS: &[&str] = &[
    "playeranimtype",
    "weaponclass",
    "mounted",
    "movetype",
    "underhand",
    "crouching",
    "firing",
    "weapon_position",
    "strafing",
];
const CLASSES: &[&str] = &[
    "rifle",
    "mg",
    "smg",
    "spread",
    "pistol",
    "grenade",
    "rocketlauncher",
    "turret",
    "non-player",
    "item",
];

fn index(names: &[&str], value: &str) -> io::Result<usize> {
    names
        .iter()
        .position(|&name| name == value)
        .ok_or_else(|| invalid(format!("unknown animation token {value}")))
}

#[derive(Debug, Default)]
struct Node {
    text: String,
    children: Option<Vec<Node>>,
}

fn lines(source: &str) -> Vec<String> {
    let mut comment = false;
    let mut result = Vec::new();
    for raw in source.lines() {
        let mut text = String::new();
        let mut chars = raw.chars().peekable();
        while let Some(c) = chars.next() {
            if comment {
                if c == '*' && chars.next_if_eq(&'/').is_some() {
                    comment = false;
                }
            } else if c == '/' && chars.next_if_eq(&'/').is_some() {
                break;
            } else if c == '/' && chars.next_if_eq(&'*').is_some() {
                comment = true;
                text.push(' ');
            } else if matches!(c, '{' | '}') {
                if !text.trim().is_empty() {
                    result.push(text.trim().to_ascii_lowercase());
                }
                text.clear();
                result.push(c.to_string());
            } else {
                text.push(c);
            }
        }
        if !text.trim().is_empty() {
            result.push(text.trim().to_ascii_lowercase());
        }
    }
    result
}

fn nodes(lines: &mut std::slice::Iter<'_, String>, depth: usize) -> io::Result<Vec<Node>> {
    if depth > 32 {
        return Err(invalid("animation tree nesting exceeds 32 levels"));
    }
    let mut result: Vec<Node> = Vec::new();
    while let Some(line) = lines.next() {
        match line.as_str() {
            "{" => {
                let last = result
                    .last_mut()
                    .ok_or_else(|| invalid("animation block has no name"))?;
                if last.children.is_some() {
                    return Err(invalid("duplicate animation block"));
                }
                last.children = Some(nodes(lines, depth + 1)?);
            }
            "}" if depth > 0 => return Ok(result),
            "}" => return Err(invalid("unmatched animation block terminator")),
            line => result.push(Node {
                text: line.to_owned(),
                children: None,
            }),
        }
    }
    if depth > 0 {
        Err(invalid("unterminated animation block"))
    } else {
        Ok(result)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Ord, PartialOrd)]
pub(super) enum Script {
    Move(usize, usize),
    Canned(usize, usize),
    Transition(usize, usize),
    Event(usize),
}

fn vocab<'a>(condition: usize, anim_types: &'a [&'a str]) -> &'a [&'a str] {
    match condition {
        0 => anim_types,
        1 => CLASSES,
        2 => &["", "mg42"],
        3 => MOVES,
        7 => &["hip", "ads"],
        8 => &["not", "left", "right"],
        _ => &[],
    }
}

type Defines = BTreeMap<(usize, String), u64>;

fn bits(id: usize, tokens: &[&str], types: &[&str], defines: &Defines) -> io::Result<u64> {
    let mut result = 0;
    let mut accumulated = 0;
    let mut negate = false;
    let mut word = Vec::new();
    for &tok in tokens.iter().chain(std::iter::once(&"")) {
        if !matches!(tok, "" | "and" | "minus" | "not") {
            word.push(tok);
            continue;
        }
        if !word.is_empty() {
            let name = word.join(" ");
            if name == "all" {
                accumulated = u64::MAX
            } else if name == "none" {
                result |= 1;
            } else if let Some(&value) = defines.get(&(id, name.clone())) {
                accumulated = value
            } else {
                accumulated |= 1 << index(vocab(id, types), &name)?;
            }
            if negate {
                result &= !accumulated
            } else {
                result |= accumulated
            }
            word.clear();
        }
        if matches!(tok, "minus" | "not") {
            negate = true
        }
    }
    Ok(result)
}

fn conditions(line: &str, types: &[&str], defs: &Defines) -> io::Result<Vec<(usize, u64)>> {
    let mut result = Vec::new();
    for term in line.split(',').map(str::trim).filter(|t| !t.is_empty()) {
        if term == "default" {
            break;
        }
        let mut tokens = term.split_whitespace();
        let id = index(
            CONDITIONS,
            tokens
                .next()
                .ok_or_else(|| invalid("missing animation condition"))?,
        )?;
        let rest: Vec<_> = tokens.collect();
        let value = if matches!(id, 0 | 1 | 3) {
            bits(id, &rest, types, defs)?
        } else if matches!(id, 4..=6) {
            1
        } else {
            index(vocab(id, types), &rest.join(" "))? as u64
        };
        result.push((id, value));
    }
    Ok(result)
}

#[derive(Debug)]
struct TreeNode {
    name: String,
    group: bool,
    synchronized_loop: Option<bool>,
    children: Vec<TreeNode>,
}
fn prune(nodes: Vec<Node>, used: &BTreeSet<String>, all: bool) -> io::Result<Vec<TreeNode>> {
    let mut children = Vec::new();
    // Child variables are inserted at the head of the original sibling list.
    for node in nodes.into_iter().rev() {
        let name = node.text.split(':').next().unwrap_or("").trim().to_owned();
        let referenced = all || used.contains(&name);
        if let Some(inner) = node.children {
            let props: Vec<_> = node
                .text
                .split(':')
                .nth(1)
                .unwrap_or("")
                .split_whitespace()
                .collect();
            let mut kept = prune(
                inner,
                used,
                all || (referenced && props.contains(&"complete")),
            )?;
            if kept.is_empty() && referenced {
                kept.push(TreeNode {
                    name: if props.contains(&"loopsync") {
                        "void_loop"
                    } else {
                        "void"
                    }
                    .into(),
                    group: false,
                    synchronized_loop: None,
                    children: vec![],
                });
            }
            if !kept.is_empty() {
                children.push(TreeNode {
                    name,
                    group: true,
                    synchronized_loop: if props.contains(&"loopsync") {
                        Some(true)
                    } else if props.contains(&"nonloopsync") {
                        Some(false)
                    } else {
                        None
                    },
                    children: kept,
                });
            }
        } else if referenced {
            children.push(TreeNode {
                name,
                group: false,
                synchronized_loop: None,
                children: vec![],
            })
        }
    }
    Ok(children)
}

pub(super) type NumberedNode = (usize, String, bool, std::ops::Range<u16>, Option<bool>);
fn number_tree(nodes: &[TreeNode], next: &mut usize, output: &mut Vec<NumberedNode>) {
    let start = *next;
    *next += nodes.len();
    for (i, n) in nodes.iter().enumerate() {
        output.push((
            start + i,
            n.name.clone(),
            n.group,
            *next as u16..(*next + n.children.len()) as u16,
            n.synchronized_loop,
        ));
        number_tree(&n.children, next, output);
    }
}

fn commands(
    tree: &[Node],
    animations: &mut BTreeMap<String, Animation>,
    key: Script,
    conditions: &[(usize, u64)],
    sounds: &SoundAliases,
) -> io::Result<Vec<Command>> {
    tree.iter()
        .map(|n| {
            let mut words = n.text.split_whitespace();
            let mut parts: Vec<Part> = Vec::new();
            let mut last = None;
            let mut sound = None;
            while let Some(token) = words.next() {
                match token {
                    "both" | "legs" | "torso" => {
                        let name = words
                            .next()
                            .ok_or_else(|| invalid("missing animation clip"))?;
                        last = Some(name);
                        let clip = animations
                            .get_mut(name)
                            .ok_or_else(|| invalid(format!("unknown animation {name}")))?;
                        if let Script::Move(_, movement) = key
                            && token != "torso"
                        {
                            clip.move_types |= 1 << movement;
                            if matches!(movement, 18 | 19) && clip.move_speed != 0. {
                                clip.flags |= 2;
                            }
                            if let Some((_, strafe)) = conditions.iter().find(|(id, _)| *id == 8) {
                                clip.flags |= match strafe {
                                    1 => 0x10,
                                    2 => 0x20,
                                    _ => 0,
                                };
                            }
                        }
                        match key {
                            Script::Event(2) => {
                                clip.flags |= 8;
                                clip.blend_time = 30;
                            }
                            Script::Event(1) => {
                                clip.flags |= 0x40;
                                clip.move_speed = 0.;
                            }
                            Script::Event(10) | Script::Move(_, 21..=30) => {
                                clip.move_speed = 0.;
                            }
                            _ => {}
                        }
                        parts.push(Part {
                            body: index(&["", "legs", "torso", "both"], token)? as u8,
                            clip: *clip,
                            duration: clip.duration as i16,
                        });
                    }
                    "duration" => {
                        let part = parts
                            .last_mut()
                            .ok_or_else(|| invalid("duration without clip"))?;
                        part.duration = words
                            .next()
                            .and_then(|s| s.parse::<i32>().ok())
                            .ok_or_else(|| invalid("bad animation duration"))?
                            as i16;
                    }
                    "blendtime" => {
                        let time = words
                            .next()
                            .and_then(|s| s.parse::<i32>().ok())
                            .ok_or_else(|| invalid("invalid blendtime"))?;
                        animations
                            .get_mut(last.ok_or_else(|| invalid("blendtime without clip"))?)
                            .expect("registered animation")
                            .blend_time = time;
                    }
                    "turretanim" => {
                        animations
                            .get_mut(last.ok_or_else(|| invalid("turretanim without clip"))?)
                            .expect("registered animation")
                            .flags |= 4;
                    }
                    "sound" => {
                        let alias = words
                            .next()
                            .ok_or_else(|| invalid("missing animation sound alias"))?;
                        if alias.contains(".wav") {
                            return Err(invalid("player animations require a sound alias"));
                        }
                        sound = sounds
                            .find(alias)
                            .and_then(|variants| variants.first())
                            .map(|variant| std::sync::Arc::from(variant.name.as_str()));
                    }
                    _ => return Err(invalid(format!("unknown player animation command {token}"))),
                }
            }
            if parts.len() > 2 {
                return Err(invalid("too many animation body parts"));
            }
            Ok(Command { parts, sound })
        })
        .collect()
}

pub(super) fn read_tree(
    assets: &GameAssets,
    tree_name: &str,
    used: &BTreeSet<String>,
) -> io::Result<Vec<NumberedNode>> {
    let source = assets.read_config(&format!("animtrees/{tree_name}.atr"))?;
    let tree = prune(nodes(&mut lines(&source).iter(), 0)?, used, false)?;
    let mut ordered = vec![(0, "root".into(), true, 1..1 + tree.len() as u16, None)];
    number_tree(&tree, &mut 1, &mut ordered);
    Ok(ordered)
}
pub(super) fn read_clip(assets: &GameAssets, name: &str) -> io::Result<super::pose::Clip> {
    Ok(
        match assets
            .read_script(&format!("xanim/{name}"))
            .and_then(|b| super::pose::Clip::parse(&b))
        {
            Ok(value) => value,
            Err(e)
                if matches!(
                    e.kind(),
                    io::ErrorKind::NotFound | io::ErrorKind::InvalidData
                ) =>
            {
                super::pose::Clip::parse(&assets.read_script("xanim/void")?)?
            }
            Err(e) => return Err(e),
        },
    )
}

impl PlayerAnimations {
    pub fn load(assets: &GameAssets, types: &[String], sounds: &SoundAliases) -> io::Result<Self> {
        Self::load_with_references(assets, types, sounds, None)
    }
    pub(crate) fn load_with_references(
        assets: &GameAssets,
        types: &[String],
        sounds: &SoundAliases,
        extra: Option<&BTreeSet<String>>,
    ) -> io::Result<Self> {
        let source = assets.read_config("mp/playeranim.script")?;
        let script = nodes(&mut lines(&source).iter(), 0)?;
        let mut used: BTreeSet<String> = ["root", "torso", "legs", "turning"]
            .map(String::from)
            .into();
        if let Some(extra) = extra {
            used.extend(extra.iter().cloned());
        }
        for line in lines(&source) {
            let words: Vec<_> = line.split_whitespace().collect();
            for pair in words.windows(2) {
                if matches!(pair[0], "legs" | "torso" | "both") {
                    used.insert(pair[1].to_string());
                }
            }
        }
        let ordered = read_tree(assets, "multiplayer", &used)?;
        if ordered.len() > 512 {
            return Err(invalid("player animation tree exceeds 512 nodes"));
        }
        let mut names = vec![String::new(); ordered.len()];
        let mut children = vec![0..0; ordered.len()];
        let mut clips = BTreeMap::new();
        let mut animations = BTreeMap::new();
        for (id, name, group, range, synchronized_loop) in ordered {
            names[id] = name.clone();
            children[id] = range;
            let mut record = Animation {
                index: id as u16,
                synchronized_loop,
                blend_time: -1,
                ..Default::default()
            };
            if group {
                record.flags = if id == 0 { 0x101 } else { 1 };
                if id == 0 {
                    record.blend_time = 0;
                }
            } else {
                let clip = read_clip(assets, &name)?;
                if clip.fps <= 0 {
                    return Err(invalid("invalid animation frame rate"));
                }
                let seconds = f32::from(clip.frames) / f32::from(clip.fps);
                record.duration = ((f64::from(seconds) * 1000.) as i32).max(500);
                record.looping = clip.looping;
                if clip.looping {
                    record.flags |= 0x80;
                }
                if seconds > 0. {
                    let movement = clip.relative_translation(0., 1.);
                    let sum = movement
                        .into_iter()
                        .map(|v| f64::from(v).powi(2))
                        .sum::<f64>();
                    record.move_speed = (sum as f32).sqrt() / seconds;
                }
                clips.insert(id as u16, std::sync::Arc::new(clip));
            }
            animations.insert(name, record);
        }
        let types: Vec<_> = types.iter().map(String::as_str).collect();
        let mut section = "defines";
        let mut defs = Defines::new();
        let mut scripts = BTreeMap::new();
        for node in &script {
            let words: Vec<_> = node.text.split_whitespace().collect();
            if let Some(children) = &node.children {
                let mut add = |key, items: &[Node]| -> io::Result<()> {
                    let parsed = items
                        .iter()
                        .map(|item| {
                            let conditions = conditions(&item.text, &types, &defs)?;
                            let commands = commands(
                                item.children
                                    .as_deref()
                                    .ok_or_else(|| invalid("missing command block"))?,
                                &mut animations,
                                key,
                                &conditions,
                                sounds,
                            )?;
                            Ok(Item {
                                conditions,
                                commands,
                            })
                        })
                        .collect::<io::Result<Vec<_>>>()?;
                    scripts.insert(key, parsed);
                    Ok(())
                };
                match section {
                    "animations" | "canned_animations" => {
                        let ["state", state] = words.as_slice() else {
                            return Err(invalid("expected animation state"));
                        };
                        let state = index(STATES, state)?;
                        for movement in children {
                            let id = index(MOVES, &movement.text)?;
                            add(
                                if section == "animations" {
                                    Script::Move(state, id)
                                } else {
                                    Script::Canned(state, id)
                                },
                                movement
                                    .children
                                    .as_deref()
                                    .ok_or_else(|| invalid("missing movement rules"))?,
                            )?;
                        }
                    }
                    "events" => add(Script::Event(index(EVENTS, &node.text)?), children)?,
                    "statechanges" => {
                        let ["statechange", from, to] = words.as_slice() else {
                            return Err(invalid("invalid animation statechange"));
                        };
                        add(
                            Script::Transition(index(STATES, from)?, index(STATES, to)?),
                            children,
                        )?;
                    }
                    _ => return Err(invalid("unexpected player animation block")),
                }
            } else if let ["set", cond, name, "=", rest @ ..] = words.as_slice() {
                let id = index(CONDITIONS, cond)?;
                defs.insert((id, name.to_string()), bits(id, rest, &types, &defs)?);
            } else {
                section = &node.text;
            }
        }
        Ok(Self {
            scripts,
            animations,
            nodes: names,
            mantle: Some(std::sync::Arc::new(
                crate::movement::mantle::MantleAnimations::load(assets)?,
            )),
            children,
            clips,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::animation::{Conditions, execute};
    use crate::level::ConfigStrings;
    use crate::snapshot::PlayerState;

    #[test]
    fn sound_only_commands_resolve_aliases_and_register_on_playback() -> io::Result<()> {
        let mut sounds = SoundAliases::default();
        let aliases = sounds.aliases.entry("GEAR".into()).or_default();
        aliases.push(Default::default());
        aliases[0].name = "Gear".into();
        let parse = |text: &str| {
            commands(
                &[Node {
                    text: text.into(),
                    children: None,
                }],
                &mut BTreeMap::new(),
                Script::Event(1),
                &[],
                &sounds,
            )
        };
        assert!(parse("sound file.wav").is_err());
        let mut ps = PlayerState::default();
        let mut conditions = Conditions::default();
        let mut strings = ConfigStrings::default();
        for command in [
            parse("sound missing")?,
            parse("sound gear")?,
            parse("sound gear")?,
        ]
        .concat()
        {
            assert_eq!(
                execute(&mut conditions, &mut ps, &command, true, false, false),
                -1
            );
            conditions.play_sounds(&mut ps, &mut strings)?;
        }
        assert_eq!(strings.get(0x24f), Some(b"Gear".as_slice()));
        assert_eq!(ps.words()[0xa4 / 4], 2);
        assert_eq!(&ps.words()[0xa8 / 4..0xa8 / 4 + 2], &[0xb3; 2]);
        assert_eq!(&ps.words()[0xb8 / 4..0xb8 / 4 + 2], &[1; 2]);
        Ok(())
    }
}
