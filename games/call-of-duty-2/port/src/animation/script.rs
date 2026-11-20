//! Script animation references pack the registered tree slot and ATR node in one word.
use super::{PlayerAnimations, parser, pose::Clip};
use crate::{
    assets::GameAssets,
    script::ScriptProgram,
    vm::value::{Value, script_error},
};
use std::{collections::BTreeMap, io, sync::Arc};

#[derive(Debug, Default)]
pub(crate) struct ScriptAnimations {
    names: BTreeMap<String, u16>,
    trees: Vec<Tree>,
}

#[derive(Debug, Default)]
struct Tree {
    nodes: BTreeMap<String, u16>,
    clips: BTreeMap<u16, Arc<Clip>>,
}

impl ScriptAnimations {
    pub(crate) fn load(
        assets: &GameAssets,
        program: &ScriptProgram,
        player: &PlayerAnimations,
    ) -> io::Result<Self> {
        let mut result = Self::default();
        for (name, used) in &program.animation_trees {
            let mut tree = Tree::default();
            if name == "multiplayer" {
                for (index, name) in player.nodes.iter().enumerate() {
                    tree.nodes.insert(name.clone(), index as u16);
                }
                tree.clips = player.clips.clone();
            } else {
                for (index, name, group, _, _) in parser::read_tree(assets, name, used)? {
                    let index = u16::try_from(index)
                        .map_err(|_| super::invalid("animation tree exceeds 65536 nodes"))?;
                    if !group {
                        tree.clips
                            .insert(index, Arc::new(parser::read_clip(assets, &name)?));
                    }
                    tree.nodes.insert(name, index);
                }
            }
            for node in used {
                if !tree.nodes.contains_key(node) {
                    return Err(super::invalid(format!(
                        "animation '{node}' not defined in anim tree '{name}'"
                    )));
                }
            }
            let id = u16::try_from(result.trees.len() + 1)
                .map_err(|_| super::invalid("too many animation trees"))?;
            result.names.insert(name.clone(), id);
            result.trees.push(tree);
        }
        Ok(result)
    }
    fn id(&self, tree: &str) -> io::Result<u16> {
        self.names
            .get(tree)
            .copied()
            .ok_or_else(|| script_error(format!("unknown anim tree '{tree}'")))
    }
    pub(crate) fn tree(&self, tree: &str) -> io::Result<Value> {
        Ok(Value::Int(i32::from(self.id(tree)?)))
    }
    pub(crate) fn animation(&self, tree: &str, node: &str) -> io::Result<Value> {
        let id = self.id(tree)?;
        let index = self.trees[usize::from(id) - 1]
            .nodes
            .get(node)
            .ok_or_else(|| {
                script_error(format!(
                    "animation '{node}' not defined in anim tree '{tree}'"
                ))
            })?;
        Ok(Value::Animation((u32::from(id) << 16) | u32::from(*index)))
    }
    pub(crate) fn dispatch(&self, name: &str, args: &[Value]) -> Option<io::Result<Value>> {
        if !matches!(
            name,
            "getanimlength" | "animhasnotetrack" | "getmovedelta" | "getangledelta"
        ) {
            return None;
        }
        Some(self.call(name, args))
    }

    fn call(&self, name: &str, args: &[Value]) -> io::Result<Value> {
        let arg = |i: usize| {
            args.get(i)
                .ok_or_else(|| script_error(format!("parameter {} does not exist", i + 1)))
        };
        let mut times = [0., 1.];
        if matches!(name, "getmovedelta" | "getangledelta") && args.len() != 1 {
            for (index, label) in [(1, "end"), (0, "start")] {
                if index == 1 && args.len() == 2 {
                    continue;
                }
                let t = arg(index + 1)?.number()?;
                if !(0. ..=1.).contains(&t) {
                    return Err(script_error(format!(
                        "{label} time must be between 0 and 1"
                    )));
                }
                times[index] = t;
            }
        }
        let value = arg(0)?;
        let Value::Animation(handle) = value else {
            return Err(script_error(format!(
                "type {} is not an anim",
                value.type_name()
            )));
        };
        let slot = (handle >> 16) as usize;
        let tree = slot
            .checked_sub(1)
            .and_then(|i| self.trees.get(i))
            .ok_or_else(|| script_error("unknown animation tree slot"))?;
        let clip = tree.clips.get(&(*handle as u16));
        let [start, end] = times;
        match name {
            "getanimlength" => {
                let clip = clip.ok_or_else(|| {
                    script_error("non-primitive animation has no concept of length")
                })?;
                Ok(Value::Float(f32::from(clip.frames) / f32::from(clip.fps)))
            }
            "animhasnotetrack" => {
                let note = arg(1)?.script_string()?;
                Ok(Value::Int(i32::from(clip.is_some_and(|c| {
                    c.notifies.iter().any(|(name, _)| name.as_bytes() == note)
                }))))
            }
            "getmovedelta" => Ok(Value::Vector(
                clip.map_or([0.; 3], |c| c.relative_translation(start, end)),
            )),
            "getangledelta" => Ok(Value::Float(
                clip.map_or(0., |c| c.relative_yaw(start, end)),
            )),
            _ => unreachable!(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn references_bind_to_tree_and_sample_clip_channels() -> io::Result<()> {
        let dir = tempfile::tempdir()?;
        let main = dir.path().join("main");
        for folder in ["animtrees", "xanim"] {
            std::fs::create_dir_all(main.join(folder))?;
        }
        std::fs::write(
            main.join("animtrees/sample.atr"),
            "branch\n{\ncurve\nunused\n}\n",
        )?;
        // One second of root translation and a named notetrack at the halfway point.
        let mut data = vec![14, 0, 3, 0, 0, 0, 2, 2, 0, 0, 0, 3, 0];
        for x in [0f32, 4., 8.] {
            for value in [x, 0., 0.] {
                data.extend(value.to_le_bytes());
            }
        }
        data.extend(b"\x01Step\0\x01\0");
        std::fs::write(main.join("xanim/curve"), data)?;
        let assets = GameAssets::mount_paths(dir.path(), None, "")?;
        let program = ScriptProgram::discover("dm", "mp_test", false, |path| {
            let src = match path {
                "codescripts/delete.gsc" => "main(){}",
                "codescripts/struct.gsc" => "initstructs(){} createstruct(){}",
                "maps/mp/gametypes/dm.gsc" => "main(){}",
                "maps/mp/gametypes/_callbacksetup.gsc" => {
                    "codecallback_startgametype(){} codecallback_playerconnect(){} codecallback_playerdisconnect(){} codecallback_playerdamage(){} codecallback_playerkilled(){}"
                }
                "maps/mp/mp_test.gsc" => {
                    r#"#using_animtree("sample"); main(){ a=%curve; b=%branch; c=#animtree; }"#
                }
                _ => return Err(io::ErrorKind::NotFound.into()),
            };
            Ok(src.as_bytes().into())
        })?;
        let registry = ScriptAnimations::load(&assets, &program, &PlayerAnimations::default())?;
        let curve = registry.animation("sample", "curve")?;
        let group = registry.animation("sample", "branch")?;
        assert_eq!(registry.tree("sample")?, Value::Int(1));
        assert_eq!(curve, Value::Animation(0x10002));
        assert_eq!(
            registry.call("getanimlength", std::slice::from_ref(&curve))?,
            Value::Float(1.)
        );
        assert_eq!(
            registry.call(
                "animhasnotetrack",
                &[curve.clone(), Value::String(b"Step".to_vec())]
            )?,
            Value::Int(1)
        );
        assert_eq!(
            registry.call(
                "animhasnotetrack",
                &[curve.clone(), Value::String(b"step".to_vec())]
            )?,
            Value::Int(0)
        );
        assert_eq!(
            registry.call(
                "getmovedelta",
                &[curve.clone(), Value::Float(0.25), Value::Float(0.75)]
            )?,
            Value::Vector([4., 0., 0.])
        );
        assert!(
            registry
                .call("getmovedelta", &[curve, Value::Int(2)])
                .is_err()
        );
        assert_eq!(
            registry.call("getangledelta", std::slice::from_ref(&group))?,
            Value::Float(0.)
        );
        assert!(registry.call("getanimlength", &[group]).is_err());
        Ok(())
    }
}
