//! Map-lifetime server data and the worldspawn configuration strings.
mod gameplay;

use crate::assets::weapons::WeaponRegistry;
use crate::assets::{GameAssets, ServerMap, SoundAliases, models::ModelRegistry};
use crate::collision::CollisionMap;
use crate::config::ServerConfig;
use crate::entities::SpawnVars;
use crate::entity_delta::EntityState;
use crate::script::ScriptProgram;
use crate::snapshot::{Snapshot, SnapshotHistory};
use crate::vm::game_entities::MapResource;
use crate::vm::{GameHost, fiber::FiberVm};
use crate::vm::{ScriptWorld, spawn_fields::SpawnFields, value::Value};
use std::collections::BTreeMap;
use std::io;
use std::sync::Arc;

/// SV_SetConfigstring and SV_GetConfigstring use 2048 slots. The wire order
/// is the index order, including when scripts assign strings out of order.
#[derive(Debug)]
pub struct ConfigStrings {
    slots: Vec<Vec<u8>>,
    changed: std::rc::Rc<std::cell::RefCell<Vec<ServerUpdate>>>,
    before_callback: Option<BTreeMap<usize, Vec<u8>>>,
}

/// Server writes in emission order. Direct commands can also target connecting slots.
#[derive(Clone, Debug)]
pub enum ServerUpdate {
    ConfigString(u16, Vec<u8>),
    Broadcast(Vec<u8>),
    Targeted(u16, Vec<u8>),
}

// Restart staging copies a map's configstrings into an independent journal.
// All player senders created for the staged map consequently target that map.
impl Clone for ConfigStrings {
    fn clone(&self) -> Self {
        Self {
            slots: self.slots.clone(),
            before_callback: None,
            changed: std::rc::Rc::new(std::cell::RefCell::new(self.changed.borrow().clone())),
        }
    }
}

#[derive(Debug)]
pub(crate) struct PlayerCommands {
    slot: u16,
    journal: std::rc::Rc<std::cell::RefCell<Vec<ServerUpdate>>>,
}

pub trait CommandSink {
    fn push(&mut self, command: Vec<u8>);
}

impl CommandSink for Vec<Vec<u8>> {
    fn push(&mut self, command: Vec<u8>) {
        Vec::push(self, command);
    }
}

impl CommandSink for PlayerCommands {
    fn push(&mut self, command: Vec<u8>) {
        self.push(command);
    }
}

impl PlayerCommands {
    pub(crate) fn push(&mut self, command: Vec<u8>) {
        self.journal
            .borrow_mut()
            .push(ServerUpdate::Targeted(self.slot, command));
    }

    pub(crate) fn take(&mut self) -> Vec<Vec<u8>> {
        self.journal
            .borrow_mut()
            .extract_if(
                ..,
                |event| matches!(event, ServerUpdate::Targeted(slot, _) if *slot == self.slot),
            )
            .filter_map(|event| match event {
                ServerUpdate::Targeted(_, command) => Some(command),
                _ => None,
            })
            .collect()
    }
}

impl Default for ConfigStrings {
    fn default() -> Self {
        Self {
            slots: vec![Vec::new(); 0x800],
            changed: Default::default(),
            before_callback: None,
        }
    }
}

impl ConfigStrings {
    pub fn get(&self, index: usize) -> Option<&[u8]> {
        self.slots.get(index).map(Vec::as_slice)
    }

    /// Returns whether the server must notify primed and active clients.
    pub fn set(&mut self, index: usize, value: &[u8]) -> io::Result<bool> {
        let slot = self
            .slots
            .get_mut(index)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "bad configstring index"))?;
        let value = value.split(|&b| b == 0).next().unwrap_or_default();
        if slot == value {
            return Ok(false);
        }
        if let Some(saved) = self.before_callback.as_mut() {
            saved.entry(index).or_insert_with(|| std::mem::take(slot));
        }
        slot.clear();
        slot.extend_from_slice(value);
        self.changed
            .borrow_mut()
            .push(ServerUpdate::ConfigString(index as u16, slot.clone()));
        Ok(true)
    }

    /// SV_SetConfigValueForKey stores a table of names followed by a parallel
    /// table of values. Existing names keep their assigned slot across updates.
    pub fn set_script_dvars(&mut self, config: &ServerConfig) -> io::Result<()> {
        const FIRST: usize = 0x8e;
        const COUNT: usize = 0x60;
        for (name, value) in config.dvars_with_flags(crate::config::SCRIPT_INFO) {
            let slot = (FIRST..FIRST + COUNT)
                .find(|&index| {
                    self.slots[index].is_empty()
                        || self.slots[index].eq_ignore_ascii_case(name.as_bytes())
                })
                .ok_or_else(|| {
                    io::Error::new(
                        io::ErrorKind::InvalidData,
                        "script dvar configstring overflow",
                    )
                })?;
            if self.slots[slot].is_empty() {
                self.set(slot, name.as_bytes())?;
            }
            self.set(slot + COUNT, value.as_bytes())?;
        }
        Ok(())
    }

    pub fn broadcast(&mut self, cmd: Vec<u8>) {
        self.changed.borrow_mut().push(ServerUpdate::Broadcast(cmd));
    }

    pub fn take_changed(&mut self) -> Vec<ServerUpdate> {
        self.changed
            .borrow_mut()
            .extract_if(.., |event| !matches!(event, ServerUpdate::Targeted(..)))
            .collect()
    }

    pub fn take_game_updates(&mut self) -> Vec<ServerUpdate> {
        std::mem::take(&mut *self.changed.borrow_mut())
    }

    pub(crate) fn player_commands(&self, slot: u16) -> PlayerCommands {
        PlayerCommands {
            slot,
            journal: self.changed.clone(),
        }
    }

    pub fn begin_callback_output(&mut self) -> usize {
        debug_assert!(self.before_callback.is_none());
        self.before_callback = Some(BTreeMap::new());
        self.changed.borrow().len()
    }

    pub fn end_callback_output(&mut self, checkpoint: usize, success: bool) {
        let saved = self
            .before_callback
            .take()
            .expect("callback output checkpoint");
        if !success {
            for (index, value) in saved {
                self.slots[index] = value;
            }
            self.changed.borrow_mut().truncate(checkpoint);
        }
    }

    pub fn populated(&self) -> impl Iterator<Item = (u16, &[u8])> {
        self.slots
            .iter()
            .enumerate()
            .filter(|(_, value)| !value.is_empty())
            .map(|(index, value)| (index as u16, value.as_slice()))
    }
}

#[derive(Debug)]
pub struct Level {
    pub scripts: Arc<ScriptProgram>,
    pub world: ScriptWorld,
    pub vm: FiberVm,
    pub name: String,
    pub map: Arc<ServerMap>,
    pub collision: Arc<CollisionMap>,
    pub sounds: Arc<SoundAliases>,
    pub persisted_players: BTreeMap<u16, Value>,
    spawn_fields: Arc<SpawnFields>,
    pub gametype: String,
    script_gametype: String,
    pub models: ModelRegistry,
    pub weapons: WeaponRegistry,
    pub gameplay_error: Option<String>,
    pub baselines: Vec<EntityState>,
    pub client_snapshots: BTreeMap<u16, ClientSnapshots>,
    pub next_snapshot_entity: u64,
    pub checksum_feed: i32,
    pub pure_checksums: Vec<i32>,
    pub snapshot_flags: u8,
    pub configstrings: ConfigStrings,
    pub start_time: i32,
    pub time_ms: i32,
}

#[derive(Debug, Default)]
pub struct ClientSnapshots {
    pub first_entity: [u64; 32],
    pub sent: SnapshotHistory,
    pub pending: Option<(i32, Snapshot)>,
}

impl Level {
    /// SV_ArchiveLinkedEntities runs once, after the three spawn frames.
    pub fn freeze_baselines(&mut self) {
        self.world.archive.freeze_baselines(&self.world.entities);
        self.baselines = self
            .world
            .entities
            .iter()
            .filter(|(slot, entity)| *slot != 0 && entity.linked)
            .map(|(_, entity)| entity.state.clone())
            .collect();
    }

    pub fn load(
        assets: &GameAssets,
        name: String,
        start_time: i32,
        config: &mut ServerConfig,
    ) -> io::Result<Self> {
        Self::transition(assets, name, start_time, config, None, false, false)
    }

    /// Fast restarts retain the level's immutable resources, network baselines,
    /// registration tables and checksum feed. Game entities and threads start afresh.
    pub fn transition(
        assets: &GameAssets,
        name: String,
        start_time: i32,
        config: &mut ServerConfig,
        previous: Option<&Self>,
        fast: bool,
        keep_state: bool,
    ) -> io::Result<Self> {
        if fast && !keep_state {
            config.register_game_dvars_only();
        } else {
            config.register_game_if_needed();
        }
        let restart = previous.filter(|_| fast);
        let gametype = config.get("g_gametype").unwrap_or("dm").to_owned();
        let opens_at_seed;
        let (map, sounds, spawn_fields, scripts, collision) = if let Some(old) = restart {
            opens_at_seed = assets
                .unpure_opens
                .load(std::sync::atomic::Ordering::Relaxed);
            (
                old.map.clone(),
                old.sounds.clone(),
                old.spawn_fields.clone(),
                old.scripts.clone(),
                old.collision.clone(),
            )
        } else {
            if let Some(usage) = assets.expected_hunk_usage(&name)? {
                config.set_from_script("com_expectedHunkUsage", &usage.to_string());
            }
            let map = Arc::new(assets.load_map(&name)?);
            let sounds = Arc::new(assets.load_sound_aliases(&name)?);
            let collision = Arc::new(CollisionMap::load_with_assets(&map, assets)?);
            // SV_SpawnServer loads collision and sounds before G_InitGame calls srand.
            opens_at_seed = assets
                .unpure_opens
                .load(std::sync::atomic::Ordering::Relaxed);
            let spawn_fields = Arc::new(SpawnFields::load(assets)?);
            let scripts = Arc::new(ScriptProgram::discover(
                &gametype,
                &name,
                config.get("developer_script") == Some("1"),
                |path| assets.read_script(path),
            )?);
            (map, sounds, spawn_fields, scripts, collision)
        };
        let world = map
            .entities
            .records()
            .first()
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "empty entity lump"))?;
        // A restart inherits the string table. Pending old-map writes are tied to
        // the old players and stay in their journal until the transition commits.
        let mut configstrings = restart.map_or_else(ConfigStrings::default, |old| ConfigStrings {
            slots: old.configstrings.slots.clone(),
            changed: Default::default(),
            before_callback: None,
        });
        // G_InitGame resets only winner inside this info string.
        let mut info = crate::query::InfoString(configstrings.get(22).unwrap_or_default().to_vec());
        info.set("winner", "0");
        configstrings.set(22, &info.0)?;
        set_worldspawn_strings(
            &mut configstrings,
            world,
            start_time,
            config.get("g_motd").unwrap_or_default().as_bytes(),
        )?;
        // GScr_LoadScripts installs function handles only on a full map load.
        // A fast restart can publish a changed g_gametype while retaining those handles.
        let script_gametype =
            restart.map_or_else(|| gametype.clone(), |old| old.script_gametype.clone());
        let gametype_module = format!("maps/mp/gametypes/{script_gametype}");
        let mut world = ScriptWorld::default();
        world.sound_aliases = sounds.clone();
        // G_InitGame keeps Scr_SetLoading enabled throughout the settling frames.
        world.script_loading = true;
        if let Some(previous) = previous {
            world.bots = previous.world.bots.clone();
        }
        let persisted_players = match previous.filter(|_| keep_state) {
            Some(old) => world.restore_map_state(&old.world)?,
            None => BTreeMap::new(),
        };
        if let Some(seed) = config.game_seed {
            world.random = crate::vm::math::GameRandom::new(seed);
            world.native_random = crate::vm::items::NativeRandom::new(seed);
        }
        world
            .native_random
            .track_loose_opens(assets.unpure_opens.clone(), opens_at_seed);
        let mut vm = FiberVm::default();
        let mut models = restart.map_or_else(ModelRegistry::default, |old| old.models.clone());
        let mut weapons = match restart {
            Some(old) => old.weapons.clone(),
            None => WeaponRegistry::load(assets)?,
        };
        world.animations = match restart {
            Some(old) => old.world.animations.clone(),
            None => crate::animation::PlayerAnimations::load_with_references(
                assets,
                weapons.player_anim_types(),
                &sounds,
                scripts
                    .animation_trees
                    .iter()
                    .find(|(name, _)| name == "multiplayer")
                    .map(|(_, nodes)| nodes),
            )?
            .into(),
        };
        world.script_animations = match restart {
            Some(old) => old.world.script_animations.clone(),
            None => crate::animation::script::ScriptAnimations::load(
                assets,
                &scripts,
                &world.animations,
            )?
            .into(),
        };
        weapons.publish(assets, &mut models, &mut configstrings)?;
        let mut host = GameHost {
            weapons: &mut weapons,
            assets,
            models: &mut models,
            start_time,
            collision: &collision,
            world: &mut world,
            config,
            strings: &mut configstrings,
            time_ms: start_time,
            initializing: true,
        };
        vm.invoke(
            &scripts,
            &mut host,
            "codescripts/struct",
            "initstructs",
            0,
            Vec::new(),
        )?;
        for record in map
            .entities
            .records()
            .iter()
            .filter(|record| record.get(b"classname") == Some(b"script_struct"))
        {
            let Value::Object(id) = vm.invoke(
                &scripts,
                &mut host,
                "codescripts/struct",
                "createstruct",
                0,
                Vec::new(),
            )?
            else {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "CreateStruct must return an object",
                ));
            };
            for (key, bytes) in record.pairs() {
                if let Some((name, value)) = spawn_fields.parse(key, bytes) {
                    host.world.set_field(id, name, value)?;
                }
            }
        }
        host.world.spawn_map_with_resources(
            &map.entities,
            &spawn_fields,
            start_time,
            &mut |request| {
                let name = match request {
                    MapResource::HintString(text) => {
                        return crate::vm::game_entities::trigger_methods::hint_index(
                            host.strings,
                            text,
                        );
                    }
                    MapResource::Model(name)
                    | MapResource::TurretWeapon(name)
                    | MapResource::ItemIndex(name)
                    | MapResource::ItemModel(name) => std::str::from_utf8(name).map_err(|_| {
                        io::Error::new(io::ErrorKind::InvalidData, "invalid asset name")
                    })?,
                };
                match request {
                    MapResource::HintString(_) => unreachable!("map hint handled above"),
                    MapResource::ItemIndex(_) | MapResource::ItemModel(_) => {
                        let id =
                            crate::vm::items::spawn::resolve(host.weapons, assets, name, true)?;
                        host.weapons.publish(assets, host.models, host.strings)?;
                        if matches!(request, MapResource::ItemModel(_)) {
                            host.models.index(
                                assets,
                                host.strings,
                                crate::vm::items::spawn::model(host.weapons, id),
                                true,
                            )
                        } else {
                            Ok(id as u8)
                        }
                    }
                    MapResource::Model(_) => host.models.index(assets, host.strings, name, true),
                    MapResource::TurretWeapon(_) => {
                        crate::vm::game_entities::turret_use::script::resolve(
                            host.weapons,
                            assets,
                            host.models,
                            host.strings,
                            name,
                            true,
                        )
                    }
                }
            },
        )?;
        crate::vm::entity_methods::link_spawned(host.world, &collision, host.models)?;
        host.strings.set(12, b"0")?;
        let checksum_feed = match restart {
            Some(old) => old.checksum_feed,
            None => {
                let mut feed_bytes = [0u8; 4];
                getrandom::fill(&mut feed_bytes).map_err(io::Error::other)?;
                i32::from_le_bytes(feed_bytes)
            }
        };
        let mut level = Self {
            vm,
            world,
            scripts,
            name,
            map,
            collision,
            sounds,
            models,
            weapons,
            gameplay_error: None,
            gametype,
            script_gametype,
            persisted_players,
            spawn_fields,
            baselines: Vec::new(),
            client_snapshots: BTreeMap::new(),
            next_snapshot_entity: 0,
            checksum_feed,
            pure_checksums: assets.loaded_pure_checksums(checksum_feed),
            snapshot_flags: 4,
            configstrings,
            time_ms: start_time,
            start_time,
        };
        if let Err(error) = level.start_gameplay(assets, config, start_time, &gametype_module) {
            level.gameplay_error = Some(error.to_string());
        }
        if let Some(old) = restart {
            level.baselines.clone_from(&old.baselines);
            level.world.archive.reuse_baselines(&old.world.archive);
        }
        Ok(level)
    }
    pub fn frame(
        &mut self,
        assets: &GameAssets,
        config: &mut ServerConfig,
        time_ms: i32,
    ) -> io::Result<()> {
        let previous_time = self.time_ms;
        self.time_ms = time_ms;
        self.world.frame_ms = time_ms.wrapping_sub(previous_time);
        if self.gameplay_error.is_some() {
            return Ok(());
        }
        crate::animation::pose::begin_frame(&mut self.world, time_ms)?;
        let mut host = GameHost {
            weapons: &mut self.weapons,
            assets,
            models: &mut self.models,
            start_time: self.start_time,
            collision: &self.collision,
            world: &mut self.world,
            config,
            strings: &mut self.configstrings,
            time_ms,
            initializing: false,
        };
        crate::vm::game_entities::touch::run_triggers(&mut self.vm, &self.scripts, &mut host)?;
        self.vm.advance(&self.scripts, &mut host)?;
        for (object, event) in host
            .world
            .run_entity_frame(time_ms, host.collision, host.models)?
        {
            self.vm.notify(object, event, &[]);
        }
        crate::combat::projectile::run_frame(
            &mut self.vm,
            &self.scripts,
            &mut host,
            previous_time,
        )?;
        crate::player::spawn::run_frame(&mut self.vm, &self.scripts, &mut host)?;
        host.world
            .vote
            .frame(time_ms, host.config, &host.world.players, host.strings)?;
        crate::player::flush_scores(&mut host.world.players);
        Ok(())
    }
}

// SP_worldspawn, 0x0811b56a, chunk_051.
fn set_worldspawn_strings(
    strings: &mut ConfigStrings,
    world: &SpawnVars,
    start_time: i32,
    motd: &[u8],
) -> io::Result<()> {
    strings.set(2, b"cod")?;
    strings.set(13, start_time.to_string().as_bytes())?;
    if let Some(track) = world.get(b"ambienttrack").filter(|value| !value.is_empty()) {
        strings.set(3, &[b"n\\".as_slice(), track].concat())?;
    } else {
        strings.set(3, b"")?;
    }
    strings.set(4, world.get(b"message").unwrap_or_default())?;
    strings.set(14, motd)?;
    strings.set(
        11,
        world
            .get(b"northyaw")
            .filter(|v| !v.is_empty())
            .unwrap_or(b"0"),
    )?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::entities::MapEntities;

    #[test]
    fn gameplay_output_preserves_target_and_call_order_and_isolates_failed_callbacks()
    -> io::Result<()> {
        let mut strings = ConfigStrings::default();
        let mut player = strings.player_commands(4);
        player.push(b"one".to_vec());
        strings.broadcast(b"two".to_vec());
        strings.set(8, b"three")?;
        player.push(b"four".to_vec());

        let checkpoint = strings.begin_callback_output();
        strings.set(8, b"uncommitted")?;
        strings.set(8, b"second update")?;
        player.push(b"discarded".to_vec());
        strings.end_callback_output(checkpoint, false);
        assert_eq!(strings.get(8), Some(b"three".as_slice()));

        let mut staged_map = strings.clone();
        staged_map.player_commands(1).push(b"staged".to_vec());
        let original = strings.take_game_updates();
        assert!(matches!(&original[..], [
            ServerUpdate::Targeted(4, first), ServerUpdate::Broadcast(second),
            ServerUpdate::ConfigString(8, third), ServerUpdate::Targeted(4, fourth)
        ] if first == b"one" && second == b"two" && third == b"three" && fourth == b"four"));
        assert_eq!(staged_map.take_game_updates().len(), 5);
        Ok(())
    }

    #[test]
    fn worldspawn_strings_keep_indices_and_single_byte_map_text() -> io::Result<()> {
        let entities = MapEntities::parse(
            b"{ classname worldspawn ambienttrack wind northyaw -91 message \"a\xffb\" }",
        )?;
        let mut strings = ConfigStrings::default();
        set_worldspawn_strings(&mut strings, &entities.records()[0], 1200, b"Welcome")?;
        assert_eq!(strings.get(3), Some(b"n\\wind".as_slice()));
        assert_eq!(strings.get(4), Some(b"a\xffb".as_slice()));
        assert_eq!(strings.get(11), Some(b"-91".as_slice()));
        assert_eq!(strings.get(13), Some(b"1200".as_slice()));
        assert!(!strings.set(14, b"Welcome\0unused")?);
        assert!(strings.set(14, b"")?);
        assert!(strings.set(2048, b"invalid").is_err());
        assert_eq!(
            strings
                .populated()
                .map(|(index, _)| index)
                .collect::<Vec<_>>(),
            [2, 3, 4, 11, 13]
        );
        Ok(())
    }
}
