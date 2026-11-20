//! CoD2 GSC bytecode execution, scheduling and server-side bindings.
pub mod builtins;
pub mod bytecode;
pub mod effects;
pub mod entity_methods;
pub mod fiber;
pub mod game_entities;
pub mod hud;
pub mod items;
pub mod math;
pub mod messages;
pub mod model_methods;
mod objects;
pub mod precache;
mod scene;
pub mod scheduler;
mod script_files;
pub mod sound;
pub mod spawn_fields;
pub mod tables;
mod trace;
pub mod transition;
pub mod value;

use crate::assets::weapons::WeaponRegistry;
use crate::assets::{GameAssets, models::ModelRegistry};
use crate::level::ConfigStrings;
use crate::{collision::CollisionMap, config::ServerConfig};
use fiber::Host;
use std::{collections::BTreeMap, io};
use value::Value;

fn invalid(text: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, text.into())
}

#[derive(Debug)]
pub struct ScriptWorld {
    pub frame_ms: i32,
    pub(crate) script_animations: std::sync::Arc<crate::animation::script::ScriptAnimations>,
    pub(crate) ignore_player_radius_damage: bool,
    pub script_loading: bool,
    pub vote: crate::vote::Vote,
    pub bots: crate::bots::BotClients,
    pub(crate) interactions: Vec<game_entities::interaction::Action>,
    pub(crate) client_is_spawning: bool,
    pub sound_aliases: std::sync::Arc<crate::assets::SoundAliases>,
    pub animations: std::sync::Arc<crate::animation::PlayerAnimations>,
    pub archive: crate::snapshot::archive::SnapshotArchive,
    pub map_command: Option<transition::MapCommand>,
    pub keep_map_state: bool,
    pub random: math::GameRandom,
    pub native_random: items::NativeRandom,
    pub hud: hud::HudElements,
    pub players: crate::player::Players,
    pub entities: game_entities::GameEntities,
    pub objectives: tables::Objectives,
    script_files: script_files::ScriptFiles,
    objects: objects::Objects,
    game: Value,
}

impl Default for ScriptWorld {
    fn default() -> Self {
        let random = math::GameRandom::default();
        Self {
            frame_ms: 0,
            script_animations: Default::default(),
            ignore_player_radius_damage: false,
            script_loading: false,
            archive: Default::default(),
            vote: Default::default(),
            animations: Default::default(),
            sound_aliases: Default::default(),
            interactions: Vec::new(),
            client_is_spawning: false,
            native_random: items::NativeRandom::new(random.seed()),
            bots: Default::default(),
            random,
            map_command: None,
            keep_map_state: false,
            players: Default::default(),
            entities: Default::default(),
            hud: Default::default(),
            objectives: Default::default(),
            script_files: Default::default(),
            objects: Default::default(),
            game: Value::Array(BTreeMap::new()),
        }
    }
}

impl ScriptWorld {
    /// Scr_QueueEntityRelease changes the pointer type to 0x14 immediately.
    /// Script references to a freed entity retain their ID even after its slot is reused.
    fn mark_dead(&mut self, id: u32) {
        self.objects.release(id, false);
    }

    pub fn free_client_hud(&mut self, slot: u16) {
        for id in self.hud.free_client(slot) {
            self.mark_dead(id);
        }
    }

    pub fn level_object(&self) -> Value {
        Value::Object(0)
    }

    pub fn field(&self, object: u32, name: &str) -> io::Result<Value> {
        if matches!(name, "origin" | "angles")
            && let Some(entity) = self
                .entities
                .slot_for_object(object)
                .and_then(|slot| self.entities.get(slot))
        {
            return Ok(Value::Vector(if name == "origin" {
                entity.origin
            } else {
                entity.angles
            }));
        }
        if let Some(value) = self.players.field(object, name) {
            return Ok(value);
        }
        self.objects.field(object, name)
    }

    /// Engine-side entity health. ClientEndFrame and finishPlayerDamage copy
    /// this value into playerState.stats[0] at their respective synchronization points.
    pub(crate) fn set_entity_health(&mut self, object: u32, health: i32) -> io::Result<()> {
        self.object_fields_mut(object)?
            .insert("health".into(), Value::Int(health));
        Ok(())
    }

    pub fn set_field(&mut self, object: u32, name: &str, value: Value) -> io::Result<()> {
        if name == "health"
            && let Some(slot) = self.players.slot_for_object(object)
        {
            let Value::Int(health) = &value else {
                return Err(invalid("expected integer health"));
            };
            self.players.get_mut(slot)?.state.words_mut()[0x12c / 4] = *health as u32;
        }
        if let Some(result) = self.players.set_field(object, name, &value) {
            result?;
            if name == "maxhealth"
                && let Some(player) = self
                    .players
                    .slot_for_object(object)
                    .and_then(|s| self.players.get(s))
            {
                let health = player.state.words()[0x12c / 4] as i32;
                self.object_fields_mut(object)?
                    .insert("health".into(), Value::Int(health));
            }
            return Ok(());
        }
        if self.objects.is_released(object) {
            return Ok(());
        }
        let fields = self.object_fields_mut(object)?;
        if matches!(value, Value::Undefined) {
            fields.remove(name);
        } else {
            fields.insert(name.to_owned(), value);
        }
        Ok(())
    }

    pub fn allocate_object(&mut self) -> io::Result<u32> {
        self.objects.allocate()
    }

    fn object_fields_mut(&mut self, object: u32) -> io::Result<&mut objects::Fields> {
        self.objects.live_fields_mut(object)
    }
}

pub struct GameHost<'a> {
    pub weapons: &'a mut WeaponRegistry,
    pub assets: &'a GameAssets,
    pub models: &'a mut ModelRegistry,
    pub start_time: i32,
    pub collision: &'a CollisionMap,
    pub world: &'a mut ScriptWorld,
    pub config: &'a mut ServerConfig,
    pub strings: &'a mut ConfigStrings,
    pub time_ms: i32,
    pub initializing: bool,
}

impl std::fmt::Debug for GameHost<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("GameHost")
            .field("time_ms", &self.time_ms)
            .field("initializing", &self.initializing)
            .finish_non_exhaustive()
    }
}

impl Host for GameHost<'_> {
    fn animation(&self, tree: &str, name: &str) -> io::Result<Value> {
        self.world.script_animations.animation(tree, name)
    }
    fn animtree(&self, tree: &str) -> io::Result<Value> {
        self.world.script_animations.tree(tree)
    }
    fn script_loading(&self) -> bool {
        self.world.script_loading
    }

    fn trace_script_errors(&self) -> bool {
        ["developer", "logfile"]
            .iter()
            .any(|name| self.config.get(name).is_some_and(|v| v != "0"))
    }
    fn abort_script_errors(&self) -> bool {
        let enabled = |name| {
            self.config
                .get(name)
                .is_some_and(|v| v != "0" && !v.is_empty())
        };
        enabled("developer") || (self.world.script_loading && enabled("logfile"))
    }
    fn flush_released_objects(&mut self) {
        self.world.objects.flush_released();
    }

    fn global(&self, name: &str) -> io::Result<Value> {
        match name {
            "level" => Ok(self.world.level_object()),
            "game" => Ok(self.world.game.clone()),
            _ => Err(invalid(format!("unknown script global {name}"))),
        }
    }
    fn set_global(&mut self, name: &str, value: Value) -> io::Result<()> {
        if name != "game" {
            return Err(invalid(format!("cannot assign {name}")));
        }
        self.world.game = value;
        Ok(())
    }
    fn field(&self, object: u32, name: &str) -> io::Result<Value> {
        if name == "model"
            && let Some(entity) = self
                .world
                .entities
                .slot_for_object(object)
                .and_then(|slot| self.world.entities.get(slot))
            && entity.dropped_item.is_some()
        {
            let name = if entity.model_index == 0 {
                &[][..]
            } else {
                self.strings
                    .get(
                        crate::assets::models::MODEL_CONFIGSTRING_BASE
                            + usize::from(entity.model_index),
                    )
                    .unwrap_or_default()
            };
            return Ok(Value::String(name.to_vec()));
        }
        if let Some(value) = crate::player::icon_field(self, object, name) {
            return Ok(value);
        }
        if let Some(value) = self.world.hud.field(object, name) {
            return Ok(value);
        }
        self.world.field(object, name)
    }
    fn set_field(&mut self, object: u32, name: &str, value: Value) -> io::Result<()> {
        if matches!(name, "statusicon" | "headicon" | "headiconteam")
            && let Some(slot) = self.world.players.slot_for_object(object)
        {
            return crate::player::set_icon_field(self, slot, name, value);
        }
        if let Some(result) =
            self.world
                .hud
                .set_field(object, name, &value, self.strings, self.initializing)
        {
            return result;
        }
        self.world
            .set_native_field(object, name, value, self.collision)
    }
    fn builtin_with_vm(
        &mut self,
        vm: &mut fiber::FiberVm,
        program: &crate::script::ScriptProgram,
        receiver: Option<u32>,
        name: &str,
        args: Vec<Value>,
    ) -> io::Result<Value> {
        if let Some(object) = receiver
            && name == "useby"
        {
            return game_entities::turret_use::script::use_by(vm, program, self, object, &args);
        }
        if receiver.is_none() && name == "addtestclient" {
            return crate::bots::add(vm, program, self);
        }
        if let Some(object) = receiver
            && name == "delete"
        {
            self.world.check_script_delete(object)?;
            vm.notify(object, "death", &[]);
            if let Some((_, turret)) = self.world.entities.for_object_mut(object)
                && turret.turret.is_some()
                && turret.owner_num < 64
            {
                let owner = turret.owner_num;
                game_entities::turret_use::release(owner, self)?;
            }
            return self.world.delete_from_script(object, self.time_ms);
        }
        if let Some(value) =
            crate::combat::projectile::builtin(vm, program, self, receiver, name, &args)
        {
            return value;
        }
        if let Some(value) =
            crate::combat::damage::builtin(vm, program, self, receiver, name, &args)
        {
            return value;
        }
        self.builtin(receiver, name, args)
    }
    fn builtin(
        &mut self,
        receiver: Option<u32>,
        name: &str,
        args: Vec<Value>,
    ) -> io::Result<Value> {
        if let Some(result) = scene::dispatch(self, receiver, name, &args) {
            return result;
        }
        if let Some(slot) = receiver.and_then(|object| self.world.players.slot_for_object(object))
            && let Some(result) = crate::player::dispatch(slot, name, &args, self)
        {
            return result;
        }
        if receiver.is_none()
            && let Some(result) = self.world.script_files.dispatch(self.assets, name, &args)
        {
            return result;
        }
        if receiver.is_none()
            && let Some(result) = crate::vote::builtin(name, &args, self)
        {
            return result;
        }
        if receiver.is_none()
            && let Some(result) = crate::player::global(name, &args, self)
        {
            return result;
        }
        if let Some(object) = receiver
            && let Some(result) = game_entities::tag_links::dispatch(self, object, name, &args)
        {
            return result;
        }
        if let Some(object) = receiver
            && let Some(result) = game_entities::mover::dispatch(
                object,
                name,
                &args,
                self.world,
                self.collision,
                self.time_ms,
            )
        {
            return result;
        }
        if let Some(object) = receiver
            && let Some(result) = game_entities::turret_use::method(object, name, &args, self)
        {
            return result;
        }
        if let Some(result) = hud::dispatch(receiver, name, &args, self) {
            return result;
        }
        if let Some(object) = receiver
            && let Some(result) =
                game_entities::trigger_methods::dispatch(object, name, &args, self)
        {
            return result;
        }
        if let Some(object) = receiver
            && let Some(result) = sound::dispatch(object, name, &args, self)
        {
            return result;
        }
        if let Some(object) = receiver
            && let Some(result) = model_methods::dispatch(self, object, name, &args)
        {
            return result;
        }
        if let Some(object) = receiver
            && name == "setmodel"
        {
            return self.set_entity_model(object, &args);
        }
        if let Some(object) = receiver
            && let Some(result) =
                entity_methods::dispatch(object, name, &args, self.world, self.collision)
        {
            return result;
        }
        if receiver.is_none() {
            if let Some(result) = self.world.script_animations.dispatch(name, &args) {
                return result;
            }
            if let Some(result) =
                sound::mix::global(name, &args, &self.world.sound_aliases, self.strings)
            {
                return result;
            }
            if matches!(name, "spawnturret" | "precacheturret") {
                return game_entities::turret_use::script::global(name, &args, self);
            }
            if let Some(result) = trace::dispatch(name, &args, self) {
                return result;
            }
            if let Some(result) = transition::dispatch(name, &args, self) {
                return result;
            }
            if let Some(result) =
                messages::developer_call(name, &args, self.config, &self.world.entities)
            {
                return result;
            }
            if let Some(value) = math::dispatch(name, &args, &mut self.world.random)? {
                return Ok(value);
            }
            if let Some(result) = tables::dispatch(
                name,
                &args,
                &mut self.world.objectives,
                &mut self.world.entities,
                self.strings,
                self.initializing,
            ) {
                return result;
            }
            if matches!(name, "precacheitem" | "getweaponmodel") {
                let precache = name == "precacheitem";
                if precache && !self.initializing {
                    return Err(invalid(
                        "precacheItem must be called before any wait statements in the gametype or level script\n",
                    ));
                }
                let bytes = args
                    .first()
                    .ok_or_else(|| invalid("expected item name"))?
                    .script_string()?;
                let name = std::str::from_utf8(bytes.split(|&b| b == 0).next().unwrap_or_default())
                    .map_err(|_| invalid("invalid item name"))?;
                let index = if precache {
                    self.weapons.precache_item(self.assets, name)?
                } else {
                    self.weapons.resolve(self.assets, name, self.initializing)?
                };
                self.weapons
                    .publish(self.assets, self.models, self.strings)?;
                return Ok(if precache {
                    Value::Undefined
                } else {
                    Value::String(
                        self.weapons
                            .get(index)
                            .expect("resolved weapon")
                            .world_model
                            .as_bytes()
                            .to_vec(),
                    )
                });
            }
            if let Some(result) = effects::dispatch(
                name,
                &args,
                self.world,
                self.strings,
                self.time_ms,
                self.collision,
            ) {
                return result;
            }
            if name == "getstarttime" {
                return Ok(Value::Int(self.start_time));
            }
            if name == "precachemodel" {
                if !self.initializing {
                    return Err(invalid(
                        "precacheModel can only be called during level initialization",
                    ));
                }
                let name = args
                    .first()
                    .ok_or_else(|| invalid("expected model name"))?
                    .script_string()?;
                let name = std::str::from_utf8(&name).map_err(|_| invalid("invalid model name"))?;
                self.models.index(self.assets, self.strings, name, true)?;
                return Ok(Value::Undefined);
            }
            if let Some(result) =
                precache::dispatch(name, &args, self.strings, self.initializing, self.time_ms)
            {
                return result;
            }
            if let Some(result) = builtins::dispatch_with_players(
                name,
                &args,
                self.config,
                self.time_ms,
                Some(&self.world.players),
            )? {
                return Ok(result);
            }
        }
        match (receiver, name, args.as_slice()) {
            // The VM validates the receiver before entering the dedicated
            // native methods. Clan lookup returns fixed strings and rumble has no output.
            (
                Some(object),
                "getclanid" | "getclanname" | "getclanmotto" | "getclandescription" | "getclanurl"
                | "playrumble" | "playlooprumble" | "stoprumble",
                _,
            ) => {
                if self.world.entities.slot_for_object(object).is_none()
                    && !self.world.hud.contains_object(object)
                {
                    return Err(value::script_error("not an entity"));
                }
                Ok(match name {
                    "getclanid" => Value::String(b"0".to_vec()),
                    "getclanname" | "getclanmotto" | "getclandescription" | "getclanurl" => {
                        Value::String(Vec::new())
                    }
                    _ => Value::Undefined,
                })
            }
            (None, "setarchive", [Value::Int(enable), ..]) => {
                self.world.archive.enabled = *enable != 0;
                Ok(Value::Undefined)
            }
            (None, "getent", _) => self.world.get_map_entities(&args, false),
            (None, "getentarray", _) => self.world.get_map_entities(&args, true),
            (None, "spawn", _) => {
                if let Some(result) = items::spawn::script(&args, self) {
                    result
                } else {
                    self.world
                        .spawn_linked_from_script(&args, self.time_ms, self.collision)
                }
            }
            (Some(object), "istouching", _) => {
                game_entities::touch::is_touching(object, &args, self)
            }
            (None, "spawnstruct", []) => Ok(Value::Object(self.world.allocate_object()?)),
            (None, "isdefined", [value]) => Ok(Value::Int(i32::from(match value {
                Value::Undefined => false,
                Value::Object(id) => self.world.objects.live_fields(*id).is_some(),
                _ => true,
            }))),
            _ => Err(invalid(format!("unimplemented game binding {name}"))),
        }
    }
}
