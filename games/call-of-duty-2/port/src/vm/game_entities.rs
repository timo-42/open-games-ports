//! G_CallSpawn, the map-time G_Spawn pool and getent/getentarray.
mod corpses;
mod frame;
pub mod interaction;
pub(crate) mod mover;
pub mod tag_links;
pub mod touch;
pub(crate) mod trigger_methods;
pub mod turret_use;
pub(crate) use corpses::clone_player;
mod fields;
use super::ScriptWorld;
use super::spawn_fields::{SpawnFields, float_prefix, parse_int};
use super::value::{ArrayKey, Value};
use crate::entities::{MapEntities, SpawnVars};
use crate::entity_delta::EntityState;
use frame::IdleTurret;
use std::collections::{BTreeMap, VecDeque};
use std::io;

const FIRST_MAP_SLOT: usize = 72;
const WORLD_SLOT: usize = 1022;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MapResource<'a> {
    Model(&'a [u8]),
    TurretWeapon(&'a [u8]),
    ItemIndex(&'a [u8]),
    ItemModel(&'a [u8]),
    HintString(&'a [u8]),
}

#[derive(Debug)]
pub struct MapEntity {
    pub object: u32,
    pub owner_num: u16,
    pub clip_mask: u32,
    mover: Option<Box<mover::Mover>>,
    pub(crate) projectile: Option<crate::combat::projectile::Missile>,
    pub trigger_claim: u16,
    pub trigger_team: u8,
    pub trigger_threshold: i32,
    pub trigger_accumulate: i32,
    /// The inline collision model from "*N"; gentity.s.index, distinct from gentity.model.
    pub brush_model: u16,
    pub model_index: u8,
    pub tag_link: Option<tag_links::TagLink>,
    pub attachments: [super::model_methods::ModelAttachment; 7],
    pub dobj_pose: Option<crate::collision::dobj::DObjPose>,
    pub(crate) player_skeleton: Option<Box<crate::animation::pose::PlayerSkeleton>>,
    idle_turret: Option<IdleTurret>,
    pub(crate) turret: Option<Box<turret_use::Turret>>,
    pub(crate) mantle_blocker_free_at: Option<i32>,
    pub(crate) corpse_think: Option<i32>,
    pub(crate) corpse_animation: Option<Box<corpses::BodyAnimation>>,
    pub(crate) dropped_item: Option<super::items::DroppedItem>,
    pub state: EntityState,
    pub linked: bool,
    pub sv_flags: u8,
    /// Zero uses normal visibility, negative forces broadcast, positive expires in server time.
    pub broadcast_time: i32,
    pub flags: u32,
    pub client_mask: [u32; 2],
    pub contents: i32,
    pub max_health: i32,
    pub take_damage: bool,
    touch_handler: touch::TouchHandler,
    pub bmodel: bool,
    pub origin: [f32; 3],
    pub angles: [f32; 3],
    pub link: super::entity_methods::SpatialLink,
    pub mins: [f32; 3],
    pub maxs: [f32; 3],
    pub event_time: i32,
    pub shared_event_time: i32,
    /// Start time of a freeAfterEvent entity (server milliseconds).
    pub free_after_event_at: Option<i32>,
}

impl MapEntity {
    fn new(slot: u16, object: u32) -> Self {
        Self {
            object,
            owner_num: 1023,
            clip_mask: 0,
            mover: None,
            projectile: None,
            trigger_claim: 1023,
            trigger_team: 0,
            trigger_threshold: 0,
            trigger_accumulate: 0,
            brush_model: 0,
            model_index: 0,
            tag_link: None,
            attachments: Default::default(),
            dobj_pose: None,
            player_skeleton: None,
            idle_turret: None,
            turret: None,
            mantle_blocker_free_at: None,
            corpse_think: None,
            corpse_animation: None,
            dropped_item: None,
            state: EntityState::new(slot),
            linked: false,
            sv_flags: 0,
            broadcast_time: 0,
            flags: 0,
            client_mask: [0; 2],
            contents: 0,
            max_health: 0,
            take_damage: false,
            touch_handler: touch::TouchHandler::None,
            bmodel: false,
            origin: [0.; 3],
            angles: [0.; 3],
            link: Default::default(),
            mins: [0.; 3],
            maxs: [0.; 3],
            event_time: 0,
            shared_event_time: 0,
            free_after_event_at: None,
        }
    }
    pub(crate) fn stamp_event(&mut self, now: i32) {
        self.event_time = now;
        self.shared_event_time = now;
        if let Some(created) = &mut self.free_after_event_at {
            *created = now;
        }
    }

    /// G_AddEvent stores non-player events in the four-slot entity-state ring.
    pub(crate) fn add_event(&mut self, event: u32, param: u32, now: i32) {
        let sequence = self.state.field("eventSequence").expect("event sequence");
        let slot = sequence as usize & 3;
        self.state.set_field(
            ["events[0]", "events[1]", "events[2]", "events[3]"][slot],
            event,
        );
        self.state.set_field(
            [
                "eventParms[0]",
                "eventParms[1]",
                "eventParms[2]",
                "eventParms[3]",
            ][slot],
            param,
        );
        self.state
            .set_field("eventSequence", sequence.wrapping_add(1));
        self.stamp_event(now);
    }

    /// Network solid word maintained by SV_LinkEntity before it links the world-sector tree.
    pub fn update_solid(&mut self) {
        let solid = if self.bmodel {
            0xff_ffff
        } else if self.contents & 0x0200_0001 != 0 {
            let width = (self.maxs[0] as i32).clamp(1, 255) as u32;
            let below = ((1.0 - self.mins[2]) as i32).clamp(1, 255) as u32;
            let above = ((self.maxs[2] + 32.0) as i32).clamp(1, 255) as u32;
            width | (below << 8) | (above << 16)
        } else {
            0
        };
        self.state.set_field("solid", solid);
    }

    fn set_transform(&mut self, fields: &BTreeMap<String, Value>) {
        for (key, names) in [
            (
                "origin",
                ["pos.trBase[0]", "pos.trBase[1]", "pos.trBase[2]"],
            ),
            (
                "angles",
                ["apos.trBase[0]", "apos.trBase[1]", "apos.trBase[2]"],
            ),
        ] {
            if let Some(Value::Vector(values)) = fields.get(key) {
                if key == "origin" {
                    self.origin = *values;
                } else {
                    self.angles = *values;
                }
                for (name, value) in names.iter().zip(values) {
                    self.state.set_field(name, value.to_bits());
                }
            }
        }
    }
}

#[derive(Debug)]
pub struct GameEntities {
    slots: Vec<Option<MapEntity>>,
    world: Option<MapEntity>,
    corpse_ring: usize,
    free: VecDeque<(u16, i32)>,
    unsupported: BTreeMap<String, Vec<u16>>,
    contacts: touch::Contacts,
}

impl Default for GameEntities {
    fn default() -> Self {
        Self {
            slots: (0..FIRST_MAP_SLOT).map(|_| None).collect(),
            world: None,
            corpse_ring: 0,
            free: VecDeque::new(),
            unsupported: BTreeMap::new(),
            contacts: Default::default(),
        }
    }
}

impl GameEntities {
    /// The world is stored in slot 1022 outside level.num_entities and is never returned by getentarray().
    pub fn iter(&self) -> impl Iterator<Item = (u16, &MapEntity)> {
        self.slots
            .iter()
            .enumerate()
            .filter_map(|(n, entity)| entity.as_ref().map(|e| (n as u16, e)))
    }

    pub fn object_for_slot(&self, slot: u16) -> Option<u32> {
        let slot = usize::from(slot);
        if slot == WORLD_SLOT {
            self.world.as_ref().map(|entity| entity.object)
        } else {
            self.slots.get(slot)?.as_ref().map(|entity| entity.object)
        }
    }

    pub fn get_mut(&mut self, slot: u16) -> Option<&mut MapEntity> {
        if usize::from(slot) == WORLD_SLOT {
            self.world.as_mut()
        } else {
            self.slots.get_mut(usize::from(slot))?.as_mut()
        }
    }

    pub fn get(&self, slot: u16) -> Option<&MapEntity> {
        if usize::from(slot) == WORLD_SLOT {
            self.world.as_ref()
        } else {
            self.slots.get(usize::from(slot))?.as_ref()
        }
    }

    pub fn slot_for_object(&self, object: u32) -> Option<u16> {
        self.iter()
            .find(|(_, e)| e.object == object)
            .map(|(slot, _)| slot)
            .or_else(|| {
                self.world
                    .as_ref()
                    .filter(|e| e.object == object)
                    .map(|_| WORLD_SLOT as u16)
            })
    }

    pub fn for_object_mut(&mut self, object: u32) -> Option<(u16, &mut MapEntity)> {
        let slot = self.slot_for_object(object)?;
        Some((slot, self.get_mut(slot)?))
    }

    fn allocate_slot(&mut self, now: i32) -> io::Result<u16> {
        if self.free.front().is_some_and(|&(_, freed)| {
            now.wrapping_sub(freed) >= 500 || self.slots.len() >= WORLD_SLOT
        }) {
            return Ok(self.free.pop_front().expect("reusable slot").0);
        }
        let slot = self.slots.len();
        if slot >= WORLD_SLOT {
            return Err(super::invalid("G_Spawn: no free entities"));
        }
        self.slots.push(None);
        Ok(slot as u16)
    }

    pub fn iter_mut(&mut self) -> impl Iterator<Item = (u16, &mut MapEntity)> {
        self.slots
            .iter_mut()
            .enumerate()
            .filter_map(|(slot, e)| Some((slot as u16, e.as_mut()?)))
    }

    pub fn num_entities(&self) -> usize {
        self.slots.len()
    }

    /// Spawn classes whose native callbacks still have coverage gaps.
    pub fn unsupported_handlers(&self) -> &BTreeMap<String, Vec<u16>> {
        &self.unsupported
    }
}

impl ScriptWorld {
    pub fn spawn_map_entities(
        &mut self,
        map: &MapEntities,
        radiant: &SpawnFields,
    ) -> io::Result<()> {
        self.spawn_map_entities_at(map, radiant, 0)
    }

    pub fn spawn_map_entities_at(
        &mut self,
        map: &MapEntities,
        radiant: &SpawnFields,
        now: i32,
    ) -> io::Result<()> {
        self.spawn_map_with_precache(map, radiant, now, &mut |_| {
            Err(super::invalid(
                "model registry required for named map models",
            ))
        })
    }

    pub fn spawn_map_with_precache(
        &mut self,
        map: &MapEntities,
        radiant: &SpawnFields,
        now: i32,
        model_index: &mut impl FnMut(&[u8]) -> io::Result<u8>,
    ) -> io::Result<()> {
        self.spawn_map_with_resources(map, radiant, now, &mut |request| match request {
            MapResource::Model(name) => model_index(name),
            MapResource::HintString(_) => Err(super::invalid("map hint string registry required")),
            MapResource::ItemIndex(_) | MapResource::ItemModel(_) => {
                Err(super::invalid("item registry required"))
            }
            MapResource::TurretWeapon(_) => Err(super::invalid("turret weapon registry required")),
        })
    }

    pub fn spawn_map_with_resources(
        &mut self,
        map: &MapEntities,
        radiant: &SpawnFields,
        now: i32,
        precache: &mut impl FnMut(MapResource<'_>) -> io::Result<u8>,
    ) -> io::Result<()> {
        let mut entities = GameEntities::default();
        let object = self.allocate_object()?;
        let vars = &map.records()[0]; // MapEntities requires a first worldspawn.
        let mut world_fields = default_fields();
        world_fields.insert(
            "classname".to_owned(),
            Value::String(b"worldspawn".to_vec()),
        );
        world_fields.insert(
            "spawnflags".to_owned(),
            Value::Int(parse_int(vars.get(b"spawnflags").unwrap_or_default())),
        );
        *self.object_fields_mut(object)? = world_fields;
        entities.world = Some(MapEntity::new(WORLD_SLOT as u16, object));
        // level.time is constant throughout the initial spawn pass. Freed
        // slots remain on the FIFO for 500 ms unless the pool reaches 1022.
        for vars in map.records().iter().skip(1) {
            let class = vars.get(b"classname").unwrap_or_default();
            if class == b"script_struct" {
                continue;
            }
            let item_index = if super::items::spawn::is_item_class(class) {
                precache(MapResource::ItemIndex(class))?
            } else {
                0
            };
            let slot = entities.allocate_slot(now)?;
            // G_ParseField calls G_ModelIndex even for a subsequently freed misc_model.
            let mut model = 0;
            for (key, name) in vars.pairs() {
                if key.eq_ignore_ascii_case(b"model") && !name.is_empty() && !name.starts_with(b"*")
                {
                    model = precache(MapResource::Model(name))?;
                }
            }
            // All five handlers call G_FreeEntity after allocation. This
            // matters for slot numbers on maps containing many static models.
            if matches!(
                class,
                b"info_null" | b"func_group" | b"light" | b"misc_model" | b"corona"
            ) {
                entities.free.push_back((slot, now));
                continue;
            }
            // Exact-case entries from g_spawnFuncTable. Other classnames,
            // including ordinary mp_* spawn points, only receive G_ParseField.
            if matches!(
                class,
                b"trigger_multiple"
                    | b"trigger_radius"
                    | b"trigger_disk"
                    | b"trigger_hurt"
                    | b"trigger_once"
                    | b"misc_mg42"
                    | b"misc_turret"
                    | b"trigger_use"
                    | b"trigger_use_touch"
                    | b"trigger_damage"
            ) {
                entities
                    .unsupported
                    .entry(String::from_utf8_lossy(class).into_owned())
                    .or_default()
                    .push(slot);
            }
            let object = self.allocate_object()?;
            let (mut fields, brush_model) = parse_fields(vars, radiant);
            let mut entity = MapEntity::new(slot, object);
            entity.brush_model = brush_model;
            entity.state.set_field("index", u32::from(brush_model));
            entity.set_transform(&fields);
            entity.model_index = model;
            if matches!(class, b"misc_mg42" | b"misc_turret") {
                let weapon = vars
                    .get(b"weaponinfo")
                    .ok_or_else(|| super::invalid("no weaponinfo specified for turret"))?;
                let weapon_index = precache(MapResource::TurretWeapon(weapon))?;
                if weapon_index == 0 {
                    return Err(super::invalid("invalid weaponinfo for turret"));
                }
                turret_use::script::available(&entities)?;
                turret_use::script::setup(
                    &mut entity,
                    turret_use::Turret::from_spawn(vars),
                    weapon_index,
                    now,
                );
            }
            if item_index != 0 {
                let item_model = precache(MapResource::ItemModel(class))?;
                let flags = parse_int(vars.get(b"spawnflags").unwrap_or_default());
                let count = parse_int(vars.get(b"count").unwrap_or_default());
                super::items::spawn::setup(
                    &mut entity,
                    usize::from(item_index),
                    item_model,
                    flags,
                    count,
                    Some(now.wrapping_add(100)),
                );
            } else {
                apply_map_spawn(class, vars, &mut fields, &mut entity, model)?;
            }
            if matches!(class, b"trigger_use" | b"trigger_use_touch") {
                if let Some(hint) = vars
                    .get(b"cursorhint")
                    .and_then(|s| trigger_methods::cursor_hint(s, true))
                {
                    entity.state.set_field("dmgFlags", hint);
                }
                if let Some(text) = vars.get(b"hintstring") {
                    let index = precache(MapResource::HintString(text))?;
                    entity.state.set_field("scale", u32::from(index));
                }
            }
            *self.object_fields_mut(object)? = fields;
            entities.slots[usize::from(slot)] = Some(entity);
        }
        self.entities = entities;
        Ok(())
    }

    /// Allocate the raw entity used by G_TempEntity, playLoopedFx and the GSC
    /// spawn builtins. Callers perform their specific entity initialization.
    pub fn spawn_entity(
        &mut self,
        class: &[u8],
        origin: [f32; 3],
        now: i32,
    ) -> io::Result<(u16, u32)> {
        let slot = self.entities.allocate_slot(now)?;
        let object = self.allocate_object()?;
        let mut fields = default_fields();
        fields.insert("classname".into(), Value::String(class.to_vec()));
        fields.insert("origin".into(), Value::Vector(origin));
        let mut entity = MapEntity::new(slot, object);
        entity.set_transform(&fields);
        *self.object_fields_mut(object)? = fields;
        self.entities.slots[usize::from(slot)] = Some(entity);
        Ok((slot, object))
    }

    /// Client slots are permanently reserved ahead of the dynamic spawn pool.
    pub fn attach_player(&mut self, slot: u16, _now: i32) -> io::Result<u32> {
        if slot >= 64 || self.entities.slots[slot as usize].is_some() {
            return Err(super::invalid("client slot unavailable"));
        }
        let object = self.allocate_object()?;
        *self.object_fields_mut(object)? = default_fields();
        self.entities.slots[slot as usize] = Some(MapEntity::new(slot, object));
        Ok(object)
    }

    pub fn detach_player(&mut self, slot: u16, _now: i32) -> io::Result<()> {
        if slot >= 64 {
            return Err(super::invalid("invalid client slot"));
        }
        tag_links::release(self, slot)?;
        if let Some(ent) = self.entities.slots[slot as usize].take() {
            self.clear_entity_references(slot, ent.object);
            self.objects.release(ent.object, true);
        }
        Ok(())
    }

    /// GScr_spawn dispatches G_CallSpawnEntity after setting classname,
    /// r.currentOrigin and the optional spawnflags integer.
    pub fn spawn_from_script(&mut self, args: &[Value], now: i32) -> io::Result<Value> {
        let [class, Value::Vector(origin), rest @ ..] = args else {
            return Err(super::invalid("spawn expects classname and origin"));
        };
        let class = class.script_string()?;
        let flags = match rest.first() {
            None => 0,
            Some(Value::Int(n)) => *n,
            _ => return Err(super::invalid("spawnflags is not an integer")),
        };
        if !matches!(
            class.as_slice(),
            b"script_origin" | b"script_model" | b"info_notnull" | b"trigger_radius"
        ) {
            return Err(super::invalid(format!(
                "unsupported spawn handler '{}'",
                String::from_utf8_lossy(&class)
            )));
        }
        let bounds = if class == b"trigger_radius" {
            let [_, radius, height, ..] = rest else {
                return Err(super::invalid(
                    "trigger_radius requires spawnflags, radius and height",
                ));
            };
            Some((radius.number()?, height.number()?))
        } else {
            None
        };
        let (slot, object) = self.spawn_entity(&class, *origin, now)?;
        self.objects
            .live_fields_mut(object)?
            .insert("spawnflags".into(), Value::Int(flags));
        let entity = self.entities.get_mut(slot).expect("allocated entity");
        if let Some((radius, height)) = bounds {
            set_radius(entity, radius, height, flags);
        } else if class == b"script_origin" || class == b"script_model" {
            entity.flags |= 0x1000;
            entity.state.set_field("eType", 6);
            entity.linked = true;
            if class == b"script_origin" {
                entity.sv_flags = 1;
            } else {
                entity.sv_flags = 4;
                entity.contents = 0x2080;
            }
            entity.update_solid();
        }
        Ok(Value::Object(object))
    }

    pub(in crate::vm) fn spawn_linked_from_script(
        &mut self,
        args: &[Value],
        now: i32,
        collision: &crate::collision::CollisionMap,
    ) -> io::Result<Value> {
        let value = self.spawn_from_script(args, now)?;
        if let Value::Object(object) = value {
            let (slot, ent) = self.entities.for_object_mut(object).expect("script spawn");
            if ent.linked
                && let Err(error) =
                    super::entity_methods::relink(ent, collision, ent.model_index != 0)
            {
                self.free_entity(slot, now)?;
                return Err(error);
            }
        }
        Ok(value)
    }

    /// Validate ScriptEntCmd_delete before the VM dispatches its death notification.
    pub fn check_script_delete(&self, object: u32) -> io::Result<()> {
        let slot = self
            .entities
            .slot_for_object(object)
            .ok_or_else(|| super::invalid("not a live entity"))?;
        if slot < 64 {
            return Err(super::invalid("Cannot delete a client entity"));
        }
        if !(64..WORLD_SLOT).contains(&usize::from(slot)) {
            return Err(super::invalid("cannot release reserved map slot"));
        }
        Ok(())
    }

    pub fn delete_from_script(&mut self, object: u32, now: i32) -> io::Result<Value> {
        self.check_script_delete(object)?;
        let slot = self
            .entities
            .slot_for_object(object)
            .expect("validated script entity");
        self.free_entity(slot, now)?;
        Ok(Value::Undefined)
    }

    pub fn free_entity(&mut self, slot: u16, now: i32) -> io::Result<()> {
        let slot_index = usize::from(slot);
        if !(64..WORLD_SLOT).contains(&slot_index) {
            return Err(super::invalid("cannot release reserved map slot"));
        }
        tag_links::release(self, slot)?;
        let entry = self
            .entities
            .slots
            .get_mut(slot_index)
            .and_then(Option::take)
            .ok_or_else(|| super::invalid("entity slot is not in use"))?;
        self.objects.release(entry.object, true);
        self.clear_entity_references(slot, entry.object);
        if slot_index >= FIRST_MAP_SLOT {
            self.entities.free.push_back((slot, now));
        }
        for slots in self.entities.unsupported.values_mut() {
            slots.retain(|&used| used != slot);
        }
        self.entities.unsupported.retain(|_, v| !v.is_empty());
        Ok(())
    }

    fn clear_entity_references(&mut self, slot: u16, object: u32) {
        for (_, other) in self.entities.iter_mut() {
            if other.owner_num == slot {
                other.owner_num = 1023;
            }
            if other.state.field("groundEntityNum") == Some(u32::from(slot)) {
                other.state.set_field("groundEntityNum", 1023);
            }
        }
        for (_, p) in self.players.iter_mut() {
            if p.input.look_at == Some(object) {
                p.input.look_at = None;
            }
            if p.use_hold_entity == i32::from(slot) {
                p.use_hold_entity = 1023;
            }
            if p.state.words()[0x5a0 / 4] == u32::from(slot) {
                p.state.words_mut()[0x5a0 / 4] = 1023;
            }
        }
    }

    pub fn expire_event_entities(&mut self, now: i32) -> io::Result<()> {
        let expired: Vec<_> = self
            .entities
            .iter()
            .filter(|(_, e)| {
                e.free_after_event_at
                    .is_some_and(|start| now.wrapping_sub(start) > 300)
            })
            .map(|(n, _)| n)
            .collect();
        for slot in expired {
            self.free_entity(slot, now)?;
        }
        Ok(())
    }

    pub fn get_map_entities(&self, args: &[Value], array: bool) -> io::Result<Value> {
        let filter = if array && args.is_empty() {
            None
        } else {
            let Some(search) = args.first() else {
                return Err(super::invalid("getent missing search value"));
            };
            let Some(field) = args.get(1) else {
                return Err(super::invalid("missing entity field name"));
            };
            let name = field.script_string()?;
            // Only the type-3 native fields in g_entityScriptFieldTable are searchable.
            if !matches!(name.as_slice(), b"classname" | b"target" | b"targetname") {
                return Ok(Value::Undefined);
            }
            Some((
                String::from_utf8(name).expect("ASCII field"),
                Value::String(search.script_string()?),
            ))
        };
        let mut matches = self.entities.iter().filter_map(|(_, entry)| {
            let fields = self.objects.live_fields(entry.object)?;
            filter
                .as_ref()
                .is_none_or(|(key, value)| fields.get(key) == Some(value))
                .then_some(Value::Object(entry.object))
        });
        if array {
            Ok(Value::Array(
                matches
                    .enumerate()
                    .map(|(i, value)| (ArrayKey::Int(i as i32), value))
                    .collect(),
            ))
        } else {
            let first = matches.next().unwrap_or_default();
            if matches.next().is_some() {
                return Err(super::invalid("getent used with more than one entity"));
            }
            Ok(first)
        }
    }
}

fn numeric(text: &[u8]) -> f32 {
    float_prefix(text)
        .and_then(|(w, _)| w.parse::<f64>().ok())
        .unwrap_or(0.) as f32
}

fn parse_vector(mut text: &[u8]) -> [f32; 3] {
    let mut result = [0.; 3];
    for coord in &mut result {
        let Some((word, tail)) = float_prefix(text) else {
            break;
        };
        let Ok(n) = word.parse() else {
            break;
        };
        *coord = n;
        text = tail;
    }
    result
}

fn set_radius(ent: &mut MapEntity, radius: f32, height: f32, flags: i32) {
    ent.touch_handler = touch::TouchHandler::Trigger;
    ent.mins = [-radius, -radius, 0.];
    ent.maxs = [radius, radius, height];
    ent.sv_flags = 0x21;
    ent.linked = true;
    ent.contents = if flags & 8 == 0 { 0x4000_0000 } else { 0 } | (flags & 7) << 18;
    ent.update_solid();
}

// Class-specific spawn state is copied to the snapshot and shared collision records.
fn apply_map_spawn(
    class: &[u8],
    vars: &SpawnVars,
    fields: &mut BTreeMap<String, Value>,
    ent: &mut MapEntity,
    model: u8,
) -> io::Result<()> {
    let mut flags = match fields.get("spawnflags") {
        Some(Value::Int(n)) => *n,
        _ => 0,
    };
    if matches!(
        class,
        b"trigger_radius" | b"trigger_multiple" | b"trigger_disk"
    ) && vars.get(b"wait").is_some_and(|wait| numeric(wait) <= 0.)
        || class == b"trigger_once"
    {
        flags |= 0x10;
        fields.insert("spawnflags".into(), Value::Int(flags));
    }
    match class {
        b"script_model" | b"script_origin" | b"script_brushmodel" => {
            ent.flags |= 0x1000;
            ent.state.set_field("eType", 6);
            if vars.get(b"light").is_some() || vars.get(b"color").is_some() {
                let color = parse_vector(vars.get(b"color").unwrap_or(b"1 1 1"))
                    .map(|v| ((v * 255.) as i32).min(255) as u32);
                let alpha =
                    ((numeric(vars.get(b"light").unwrap_or(b"100")) / 4.) as i32).min(255) as u32;
                ent.state.set_field(
                    "constantLight",
                    alpha << 24 | color[2] << 16 | color[1] << 8 | color[0],
                );
            }
            if class == b"script_model" {
                ent.sv_flags = 4;
                ent.contents = 0x2080;
                if model != 0 {
                    ent.state.set_field("index", u32::from(model));
                }
            } else if class == b"script_brushmodel" {
                ent.bmodel = true;
                ent.contents = 1;
            } else if ent.state.field("constantLight") == Some(0) {
                ent.sv_flags = 1;
            } else {
                ent.state.set_field("eFlags", 0x20);
            }
            ent.linked = true;
        }
        b"trigger_radius" => {
            let radius = vars
                .get(b"radius")
                .ok_or_else(|| super::invalid("radius not specified for trigger_radius"))?;
            let height = vars
                .get(b"height")
                .ok_or_else(|| super::invalid("height not specified for trigger_radius"))?;
            set_radius(ent, numeric(radius), numeric(height), flags);
        }
        b"trigger_hurt" => {
            if flags & 1 == 0 {
                ent.touch_handler = touch::TouchHandler::Hurt { next_damage: 0 };
            } else {
                ent.touch_handler = touch::TouchHandler::HurtDisabled { next_damage: 0 };
            }
            ent.bmodel = true;
            ent.contents = 0x405c0008;
            ent.sv_flags = 1;
            ent.linked = true;
            ent.state.set_field("eFlags", 1);
            if fields.get("dmg") == Some(&Value::Int(0)) {
                fields.insert("dmg".into(), Value::Int(5));
            }
        }
        b"trigger_lookat" => {
            ent.bmodel = true;
            ent.contents = 0x2000_0000;
            ent.sv_flags = 1;
            ent.linked = true;
            ent.state.set_field("eFlags", 1);
        }
        b"trigger_damage" => {
            ent.take_damage = true;
            ent.trigger_accumulate = parse_int(vars.get(b"accumulate").unwrap_or_default());
            ent.trigger_threshold = parse_int(vars.get(b"threshold").unwrap_or_default());
            if vars.get(b"wait").is_some_and(|wait| numeric(wait) <= 0.) {
                fields.insert("spawnflags".into(), Value::Int(flags | 0x200));
            }
            fields.insert("health".into(), Value::Int(32000));
            ent.bmodel = true;
            ent.contents = 0x405c0008;
            ent.sv_flags = 1;
            ent.linked = true;
            ent.state.set_field("eFlags", 1);
        }
        b"trigger_multiple" | b"trigger_once" | b"trigger_disk" => {
            ent.touch_handler = touch::TouchHandler::Trigger;
            ent.linked = true;
            ent.contents = if flags & 8 == 0 { 0x4000_0000 } else { 0 } | ((flags & 7) << 18);
            if class == b"trigger_disk" {
                let radius = 64.0
                    + numeric(
                        vars.get(b"radius")
                            .ok_or_else(|| super::invalid("trigger_disk requires radius"))?,
                    );
                ent.mins = [-radius, -radius, -100000.];
                ent.maxs = [radius, radius, 100000.];
                ent.sv_flags = 0x41;
            } else {
                ent.bmodel = true;
                ent.sv_flags = 1;
                ent.state.set_field("eFlags", 1);
            }
        }
        b"trigger_use" | b"trigger_use_touch" => {
            ent.bmodel = true;
            ent.contents = 0x200000;
            ent.sv_flags = 1;
            ent.linked = true;
            ent.state.set_field("scale", 255);
            ent.state.set_field("dmgFlags", 2);
        }
        b"misc_turret" | b"misc_mg42" => {
            if fields.get("health") == Some(&Value::Int(0)) {
                fields.insert("health".into(), Value::Int(100));
            }
        }
        _ => return Ok(()),
    }
    ent.update_solid();
    // SV_LinkEntity snaps r.currentAngles but leaves the trajectory base alone.
    for angle in &mut ent.angles {
        let near = angle.round();
        let delta = near - *angle;
        if delta * delta < 1.0000001e-6 {
            *angle = near;
        }
    }
    fields.insert("angles".into(), Value::Vector(ent.angles));
    Ok(())
}

fn default_fields() -> BTreeMap<String, Value> {
    let mut fields = BTreeMap::new();
    fields.insert("classname".to_owned(), Value::String(b"noclass".to_vec()));
    fields.insert("model".to_owned(), Value::String(Vec::new()));
    for name in ["spawnflags", "count", "health", "dmg"] {
        fields.insert(name.to_owned(), Value::Int(0));
    }
    for name in ["origin", "angles"] {
        fields.insert(name.to_owned(), Value::Vector([0.0; 3]));
    }

    fields
}

fn parse_fields(vars: &SpawnVars, radiant: &SpawnFields) -> (BTreeMap<String, Value>, u16) {
    let mut fields = default_fields();
    let mut brush = 0;
    for (key, raw) in vars.pairs() {
        let lower = key.to_ascii_lowercase();
        let value = match lower.as_slice() {
            b"classname" | b"target" | b"targetname" => Value::String(unescape(raw)),
            b"spawnflags" | b"count" | b"health" | b"dmg" => Value::Int(parse_int(raw)),
            b"origin" | b"angles" => Value::Vector(parse_vector(raw)),
            b"model" => {
                if let Some(index) = raw.strip_prefix(b"*") {
                    brush = parse_int(index) as u16;
                    continue;
                }
                Value::String(raw.to_vec())
            }
            _ => {
                if let Some((name, value)) = radiant.parse(key, raw) {
                    fields.insert(name.to_owned(), value);
                }
                continue;
            }
        };
        fields.insert(
            String::from_utf8(lower).expect("ASCII builtin field"),
            value,
        );
    }
    (fields, brush)
}

// G_NewString / Print_UnescapeAndEmit only translates backslash-n. Other
// escaped characters collapse to a single backslash.
fn unescape(text: &[u8]) -> Vec<u8> {
    let mut chars = text.iter().copied();
    let mut result = Vec::with_capacity(text.len());
    while let Some(byte) = chars.next() {
        if byte == b'\\' {
            result.push(if chars.next() == Some(b'n') {
                b'\n'
            } else {
                b'\\'
            });
        } else {
            result.push(byte);
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn slot_order_case_sensitive_spawn_and_mutable_queries() -> io::Result<()> {
        let map = MapEntities::parse(
            br#"
          { classname worldspawn } { classname script_struct } { classname misc_model }
          { classname mp_dm_spawn targetname here origin "1 -2 3" }
          { classname LIGHT targetname here } { classname script_brushmodel model "*9" }
        "#,
        )?;
        let mut world = ScriptWorld::default();
        world.spawn_map_entities(&map, &SpawnFields::default())?;
        assert_eq!(
            world
                .entities
                .iter()
                .map(|(slot, _)| slot)
                .collect::<Vec<_>>(),
            [73, 74, 75]
        );
        let object = world.entities.iter().next().expect("spawn point").1.object;
        assert_eq!(world.field(object, "origin")?, Value::Vector([1., -2., 3.]));
        let class = Value::String(b"classname".to_vec());
        assert_eq!(
            world.get_map_entities(&[Value::String(b"mp_dm_spawn".to_vec()), class], false)?,
            Value::Object(object)
        );
        let target = [
            Value::String(b"here".to_vec()),
            Value::String(b"targetname".to_vec()),
        ];
        assert!(world.get_map_entities(&target, false).is_err());
        assert_eq!(world.get_map_entities(&target, true)?.size()?, 2);
        world.set_field(object, "targetname", Value::Undefined)?;
        assert_eq!(world.get_map_entities(&target, true)?.size()?, 1);
        assert_eq!(
            world.entities.iter().last().expect("brush").1.brush_model,
            9
        );
        assert!(world.entities.unsupported_handlers().is_empty());
        Ok(())
    }

    #[test]
    fn runtime_reuse_and_event_expiry_use_server_time() -> io::Result<()> {
        let mut world = ScriptWorld::default();
        let (first, id) = world.spawn_entity(b"tempEntity", [1., 2., 3.], 100)?;
        world
            .entities
            .get_mut(first)
            .expect("spawned")
            .free_after_event_at = Some(100);
        world.expire_event_entities(400)?;
        assert_eq!(world.entities.object_for_slot(first), Some(id));
        world.expire_event_entities(401)?;
        assert_eq!(world.entities.object_for_slot(first), None);
        let (early, _) = world.spawn_entity(b"noclass", [0.; 3], 900)?;
        assert_ne!(first, early);
        let (ready, new_id) = world.spawn_entity(b"noclass", [0.; 3], 901)?;
        assert_eq!(ready, first);
        assert_ne!(id, new_id);
        let result = world.spawn_from_script(
            &[
                Value::String(b"trigger_radius".to_vec()),
                Value::Vector([1., 2., 3.]),
                Value::Int(3),
                Value::Float(24.),
                Value::Float(12.),
            ],
            905,
        )?;
        let Value::Object(handle) = result else {
            panic!("expected entity");
        };
        let (_, radius) = world.entities.for_object_mut(handle).expect("live trigger");
        assert_eq!(radius.mins, [-24., -24., 0.]);
        assert_eq!(radius.maxs, [24., 24., 12.]);
        assert_eq!(radius.sv_flags, 0x21);
        assert_eq!(radius.contents, 0x400c0000);
        Ok(())
    }

    #[test]
    fn reuses_oldest_freed_slot_only_when_full() -> io::Result<()> {
        let mut text = b"{ classname worldspawn }".to_vec();
        for _ in FIRST_MAP_SLOT..WORLD_SLOT {
            text.extend(b" { classname light }");
        }
        text.extend(b" { classname mp_dm_spawn } { classname info_notnull }");
        let mut world = ScriptWorld::default();
        world.spawn_map_entities(&MapEntities::parse(&text)?, &SpawnFields::default())?;
        assert_eq!(world.entities.num_entities(), WORLD_SLOT);
        assert_eq!(
            world.entities.iter().map(|(n, _)| n).collect::<Vec<_>>(),
            [72, 73]
        );
        Ok(())
    }
}
