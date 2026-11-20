//! The setmodel entity method (0x08111f12).
mod attachments;
use super::{GameHost, entity_methods, invalid, value::Value};
use crate::assets::models::MODEL_CONFIGSTRING_BASE;
pub use attachments::{ModelAttachment, build_pose};
pub(crate) use attachments::{assemble_pose, dispatch, mirror_client};
use std::io;

impl GameHost<'_> {
    pub(super) fn set_entity_model(&mut self, object: u32, args: &[Value]) -> io::Result<Value> {
        let (slot, entity) = self
            .world
            .entities
            .for_object_mut(object)
            .ok_or_else(|| invalid("not an entity"))?;
        let arg = args
            .first()
            .ok_or_else(|| invalid("expected model name"))?
            .script_string()?;
        let name = arg.split(|&byte| byte == 0).next().unwrap_or_default();
        let name = std::str::from_utf8(name).map_err(|_| invalid("invalid model name"))?;
        let index = self
            .models
            .index(self.assets, self.strings, name, self.initializing)?;
        // Ent_RefreshAttachments only rebuilds the server DObj for non-client entities.
        let (bounds, pose) = if slot < 64 {
            (entity.link.dobj_bounds, None)
        } else {
            attachments::geometry(self.models, index, &entity.attachments)?
        };
        let update_index =
            slot >= 64 && index != 0 && matches!(entity.state.field("eType"), Some(0 | 6 | 9));
        let old_brush_model = entity.brush_model;
        if update_index && entity.bmodel {
            entity.brush_model = u16::from(index);
        }
        let old_bounds = std::mem::replace(&mut entity.link.dobj_bounds, bounds);
        if let Err(error) = entity_methods::relink(entity, self.collision, bounds.is_some()) {
            entity.link.dobj_bounds = old_bounds;
            entity.brush_model = old_brush_model;
            return Err(error);
        }
        // Replacing a non-client DObj invalidates its evaluated skeleton.
        if slot >= 64 {
            entity.dobj_pose = pose;
        }
        entity.model_index = index;
        if update_index {
            entity.state.set_field("index", u32::from(index));
        }
        let registered_name = self
            .strings
            .get(MODEL_CONFIGSTRING_BASE + usize::from(index))
            .expect("model range within server configstrings");
        self.world
            .set_field(object, "model", Value::String(registered_name.to_vec()))?;
        if slot >= 64 {
            super::game_entities::tag_links::update_tags(self.world, slot, self.models)?;
        }
        Ok(Value::Undefined)
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::assets::{GameAssets, weapons::WeaponRegistry};
    use crate::vm::fiber::Host;
    use std::fs;

    pub(crate) fn setup_assets() -> io::Result<(tempfile::TempDir, GameAssets)> {
        let root = tempfile::tempdir()?;
        let main = root.path().join("main");
        fs::create_dir_all(main.join("weapons/mp"))?;
        fs::create_dir_all(main.join("mp"))?;
        fs::write(main.join("mp/playeranimtypes.txt"), "none")?;
        fs::write(main.join("weapons/mp/defaultweapon_mp"), b"WEAPONFILE")?;
        let assets = GameAssets::mount_paths(root.path(), None, "")?;
        Ok((root, assets))
    }

    #[test]
    fn setmodel_relinks_and_clear_preserves_script_model_network_index() -> io::Result<()> {
        let (_root, assets) = setup_assets()?;
        let mut host = GameHost {
            assets: &assets,
            models: &mut Default::default(),
            weapons: &mut WeaponRegistry::load(&assets)?,
            start_time: 0,
            collision: &crate::collision::tests::map(-1, -2, &[])?,
            world: &mut Default::default(),
            config: &mut crate::config::ServerConfig::from_args(&[]),
            strings: &mut Default::default(),
            time_ms: 0,
            initializing: true,
        };
        let name = |bytes: &[u8]| Value::String(bytes.to_vec());
        let Value::Object(object) = host.builtin(
            None,
            "spawn",
            vec![name(b"script_model"), Value::Vector([22., 44., 66.])],
        )?
        else {
            panic!("spawn returned a nonentity")
        };
        assert!(
            host.builtin(Some(0), "setmodel", vec![name(b"xmodel/invalid_entity")])
                .is_err()
        );
        host.builtin(Some(object), "setmodel", vec![name(b"xmodel/test")])?;
        host.initializing = false;
        host.builtin(Some(object), "setmodel", vec![name(b"XMODEL/TEST")])?;
        assert_eq!(host.field(object, "model")?, name(b"xmodel/test"));
        let (_, entity) = host
            .world
            .entities
            .for_object_mut(object)
            .expect("live entity");
        assert_eq!(entity.model_index, 1);
        assert_eq!(entity.state.field("index"), Some(1));
        assert!(entity.link.dobj_bounds.is_some());
        assert!(entity.linked && entity.link.in_collision_world);
        assert_eq!(entity.link.absmin, [21., 43., 65.]);
        assert_eq!(entity.link.absmax, [23., 45., 67.]);
        assert!(
            host.builtin(Some(object), "setmodel", vec![name(b"xmodel/new")])
                .is_err()
        );
        assert_eq!(host.field(object, "model")?, name(b"xmodel/test"));
        host.builtin(Some(object), "setmodel", vec![name(b"")])?;
        let (_, entity) = host
            .world
            .entities
            .for_object_mut(object)
            .expect("live entity");
        assert_eq!(entity.model_index, 0);
        assert_eq!(entity.state.field("index"), Some(1));
        assert!(entity.link.dobj_bounds.is_none());
        assert_eq!(host.field(object, "model")?, name(b""));
        let player = host.world.attach_player(3, 0)?;
        host.builtin(Some(player), "setmodel", vec![name(b"xmodel/test")])?;
        let (_, entity) = host
            .world
            .entities
            .for_object_mut(player)
            .expect("player entity");
        assert_eq!(entity.model_index, 1);
        assert_eq!(entity.state.field("index"), Some(0));
        assert!(entity.link.dobj_bounds.is_none());
        Ok(())
    }
    #[test]
    fn attachments_compact_lowercase_and_mirror_six_of_seven_slots() -> io::Result<()> {
        let (_root, assets) = setup_assets()?;
        let mut host = GameHost {
            assets: &assets,
            models: &mut Default::default(),
            weapons: &mut WeaponRegistry::load(&assets)?,
            start_time: 0,
            collision: &crate::collision::tests::map(-1, -2, &[])?,
            world: &mut Default::default(),
            config: &mut crate::config::ServerConfig::from_args(&[]),
            strings: &mut Default::default(),
            time_ms: 0,
            initializing: true,
        };

        let player = host.world.attach_player(0, 0)?;
        let text = |s: &str| Value::String(s.as_bytes().to_vec());
        for i in 0..7 {
            host.builtin(
                Some(player),
                "attach",
                vec![
                    text(&format!("xmodel/m{i}")),
                    text("TAG_HEAD"),
                    Value::Int(i % 2),
                ],
            )?;
        }
        assert_eq!(
            host.builtin(Some(player), "getattachsize", vec![])?,
            Value::Int(7)
        );
        assert!(
            host.builtin(Some(player), "attach", vec![text("xmodel/overflow")])
                .is_err()
        );
        let mut client = crate::snapshot::ClientState::new(0);
        mirror_client(
            host.world
                .entities
                .get_mut(0)
                .expect("connected fixture entity"),
            &mut client,
            host.strings,
        )?;
        assert_eq!(host.strings.get(0x6f), Some(b"tag_head".as_slice()));
        assert_eq!(client.field("attachModelIndex[5]"), Some(6));
        assert_eq!(client.field("attachTagIndex[5]"), Some(1));
        host.builtin(
            Some(player),
            "detach",
            vec![text("XMODEL/M0"), text("TAG_HEAD")],
        )?;
        assert_eq!(
            host.builtin(Some(player), "getattachmodelname", vec![Value::Int(0)])?,
            text("xmodel/m1")
        );
        assert_eq!(
            host.builtin(Some(player), "getattachtagname", vec![Value::Int(0)])?,
            text("tag_head")
        );
        assert_eq!(
            host.builtin(
                Some(player),
                "getattachignorecollision",
                vec![Value::Int(0)]
            )?,
            Value::Int(1)
        );
        assert!(
            host.builtin(
                Some(player),
                "attach",
                vec![text("XMODEL/M1"), text("TAG_HEAD")]
            )
            .is_err()
        );
        assert_eq!(
            host.builtin(Some(player), "getattachsize", vec![])?,
            Value::Int(5)
        );
        host.builtin(Some(player), "detachall", vec![])?;
        assert_eq!(
            host.builtin(Some(player), "getattachsize", vec![])?,
            Value::Int(0)
        );
        mirror_client(
            host.world
                .entities
                .get_mut(0)
                .expect("connected fixture entity"),
            &mut client,
            host.strings,
        )?;
        assert_eq!(client.field("attachTagIndex[0]"), Some(0));
        assert_eq!(client.field("attachModelIndex[0]"), Some(0));
        Ok(())
    }
}
