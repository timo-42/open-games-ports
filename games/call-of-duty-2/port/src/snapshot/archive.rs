//! Server archives used by follow cameras, killcams and historical queries.
//! The byte ring is retained independently of the decoded frame cache.
mod decode;
use super::{ClientState, PlayerState, Snapshot, fields, player, tables::SHARED_FIELDS, world};
use crate::{
    collision::CollisionMap,
    entity_delta::EntityState,
    message::{MessageReader, MessageWriter},
    vm::game_entities::{GameEntities, MapEntity},
};
use std::{
    collections::{BTreeMap, VecDeque},
    io,
    sync::Arc,
};

type SharedEntity = [u32; 0x114 / 4];
const ZERO: SharedEntity = [0; 0x114 / 4];
const MAX_WIRE_BYTES: usize = 0x2000000;

fn shared(entity: &MapEntity, broadcast: bool) -> SharedEntity {
    let mut words = ZERO;
    words[..60].copy_from_slice(entity.state.words());
    words[60] = u32::from(entity.sv_flags)
        | if broadcast && entity.broadcast_time != 0 {
            8
        } else {
            0
        };
    words[61..63].copy_from_slice(&entity.client_mask);
    words[63..66].copy_from_slice(&entity.link.absmin.map(f32::to_bits));
    words[66..69].copy_from_slice(&entity.link.absmax.map(f32::to_bits));
    words
}

pub fn copy_player(player: &PlayerState) -> Option<PlayerState> {
    if player.words()[0xc / 4] & 0x80_0000 == 0 {
        return None;
    }
    let mut copy = player.clone();
    copy.words_mut()[0x7a8 / 4..0x1728 / 4].fill(0);
    Some(copy)
}

#[derive(Debug)]
struct Client {
    state: ClientState,
    player: Option<PlayerState>,
}

#[derive(Debug, Default)]
pub struct ArchivedFrame {
    time: i32,
    clients: BTreeMap<u8, Client>,
    entities: Vec<SharedEntity>,
}

impl ArchivedFrame {
    pub fn player_state(&self, slot: u8, server_time: i32) -> Option<(PlayerState, ClientState)> {
        let client = self.clients.get(&slot)?;
        let mut player = client.player.clone()?;
        let delta = server_time.wrapping_sub(self.time) as u32;
        let words = player.words_mut();
        let rebase = |value: &mut u32| {
            if *value != 0 {
                *value = value.wrapping_add(delta);
            }
        };
        for index in [0, 4, 0x11, 0x1c, 0x3f, 0x177] {
            rebase(&mut words[index]);
        }
        for i in 0..31 {
            let start = 0x5d4 + i * 32;
            for j in [16, 0, 8, 14] {
                let value = &mut words[start + j];
                if *value != 0 {
                    rebase(value);
                    if j == 0 && (*value as i32) > server_time {
                        *value = server_time as u32;
                    }
                }
            }
        }
        words[0x1e9] = words[0x1e9].wrapping_add(delta);
        Some((player, client.state.clone()))
    }

    pub fn build(
        &self,
        server_time: i32,
        flags: u8,
        player: &PlayerState,
        collision: &CollisionMap,
        fog_distance_squared: f32,
    ) -> io::Result<Snapshot> {
        let recipient = player.words()[0xcc / 4] as usize;
        if recipient >= 64 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "invalid archived snapshot view client",
            ));
        }
        let mut frame = Snapshot {
            server_time,
            flags,
            player: player.clone(),
            clients: self.clients.values().map(|c| c.state.clone()).collect(),
            entities: Vec::new(),
        };
        let eye = world::view_origin(player);
        let Some(cluster) = collision
            .leaf_cluster(collision.point_leaf(eye))
            .filter(|&c| c >= 0)
        else {
            return Ok(frame);
        };
        let delta = server_time.wrapping_sub(self.time) as u32;
        for words in &self.entities {
            if words[0] as usize == recipient
                || words[61 + recipient / 32] & (1 << (recipient % 32)) != 0
            {
                continue;
            }
            let mins = std::array::from_fn(|i| f32::from_bits(words[63 + i]));
            let maxs = std::array::from_fn(|i| f32::from_bits(words[66 + i]));
            if words[60] & 0x18 == 0
                && (!collision.box_visible(cluster, mins, maxs)
                    || world::exceeds_fog(mins, maxs, eye, fog_distance_squared))
            {
                continue;
            }
            let mut state = std::array::from_fn(|i| words[i]);
            for i in [4, 13, 21, 22] {
                if state[i] != 0 {
                    state[i] = state[i].wrapping_add(delta);
                }
            }
            frame.entities.push(EntityState::from_words(state));
        }
        Ok(frame)
    }
}

#[derive(Debug)]
struct Cached {
    index: i64,
    base: bool,
    frame: Arc<ArchivedFrame>,
}

#[derive(Debug, Default)]
pub struct SnapshotArchive {
    pub enabled: bool,
    baselines: BTreeMap<u16, SharedEntity>,
    wire: VecDeque<Vec<u8>>,
    wire_bytes: usize,
    next: i64,
    cache: VecDeque<Cached>,
    cached_entities: usize,
    cached_clients: usize,
}

impl SnapshotArchive {
    pub(crate) fn reuse_baselines(&mut self, old: &Self) {
        self.baselines.clone_from(&old.baselines);
    }

    pub fn freeze_baselines(&mut self, entities: &GameEntities) {
        self.baselines = entities
            .iter()
            .filter(|(n, e)| *n != 0 && e.linked)
            .map(|(n, e)| (n, shared(e, false)))
            .collect();
    }

    fn cache(&mut self, index: i64, base: bool, frame: ArchivedFrame) -> Arc<ArchivedFrame> {
        self.cached_clients += frame.clients.len();
        self.cached_entities += frame.entities.len();
        let frame = Arc::new(frame);
        self.cache.push_back(Cached {
            index,
            base,
            frame: Arc::clone(&frame),
        });
        while self.cache.len() > 512 || self.cached_entities > 16384 || self.cached_clients > 4096 {
            let removed = self
                .cache
                .pop_front()
                .expect("count includes a cached frame");
            self.cached_clients -= removed.frame.clients.len();
            self.cached_entities -= removed.frame.entities.len();
        }
        frame
    }

    pub fn record<'a>(
        &mut self,
        server_time: i32,
        fps: i32,
        clients: impl IntoIterator<Item = (&'a ClientState, &'a PlayerState)>,
        entities: &GameEntities,
    ) -> io::Result<()> {
        if !self.enabled {
            return Ok(());
        }
        let base = self
            .cache
            .iter()
            .rev()
            .find(|entry| entry.base && entry.index >= self.next - i64::from(fps));
        let frame = ArchivedFrame {
            time: server_time,
            clients: clients
                .into_iter()
                .map(|(c, p)| {
                    (
                        c.number(),
                        Client {
                            state: c.clone(),
                            player: copy_player(p),
                        },
                    )
                })
                .collect(),
            entities: entities
                .iter()
                .filter(|(_, e)| {
                    e.linked
                        && (e.broadcast_time != 0
                            || (e.sv_flags & 1 == 0
                                && (e.sv_flags & 0x18 != 0 || !e.link.clusters.is_empty())))
                })
                .map(|(_, e)| shared(e, true))
                .collect(),
        };
        let mut writer = MessageWriter::new(0x20000);
        let encoded = self.encode(&mut writer, base, &frame);
        if let Err(error) = encoded {
            // A large frame is dropped without incrementing the archived frame index.
            if error.kind() == io::ErrorKind::InvalidData {
                return Ok(());
            }
            return Err(error);
        }
        if base.is_none() {
            self.cache(self.next, true, frame);
        }
        let bytes = writer.into_bytes();
        self.wire_bytes += bytes.len();
        self.wire.push_back(bytes);
        self.next += 1;
        while self.wire.len() > 1200 || self.wire_bytes > MAX_WIRE_BYTES {
            self.wire_bytes -= self.wire.pop_front().expect("archive contains frame").len();
        }
        Ok(())
    }

    fn encode(
        &self,
        w: &mut MessageWriter,
        base: Option<&Cached>,
        frame: &ArchivedFrame,
    ) -> io::Result<()> {
        w.write_bit(base.is_none())?;
        if let Some(base) = base {
            w.write_long(base.index as i32)?;
        }
        w.write_long(frame.time)?;
        for (num, c) in &frame.clients {
            let old = base.and_then(|b| b.frame.clients.get(num));
            super::write_client(w, old.map(|c| &c.state), Some(&c.state), true)?;
            w.write_bit(c.player.is_some())?;
            if let Some(new) = &c.player {
                player::write_delta(w, old.and_then(|c| c.player.as_ref()), new)?;
            }
        }
        w.write_bit(false)?;
        for new in &frame.entities {
            let old = self.baselines.get(&(new[0] as u16)).unwrap_or(&ZERO);
            let count = fields::change_count(SHARED_FIELDS, old, new);
            w.write_bits(new[0], 10)?;
            w.write_bit(false)?;
            w.write_bit(count != 0)?;
            if count != 0 {
                w.write_byte(count as u8)?;
                fields::write_fields(w, &SHARED_FIELDS[..count], old, new)?;
            }
        }
        w.write_bits(1023, 10)
    }

    fn load(&mut self, index: i64) -> Option<Arc<ArchivedFrame>> {
        let first = self.next - self.wire.len() as i64;
        if index < first || index >= self.next {
            return None;
        }
        if let Some(entry) = self.cache.iter().rev().find(|entry| entry.index == index) {
            return Some(Arc::clone(&entry.frame));
        }
        let offset = (index - first) as usize;
        // Resolve the full base before borrowing the retained archive bytes again.
        let mut m = MessageReader::new(&self.wire[offset]);
        let full = m.read_bit().ok()?;
        let base = if full {
            None
        } else {
            let n = i64::from(m.read_long().ok()?);
            if n >= index {
                return None;
            }
            Some(self.load(n)?)
        };
        let mut m = MessageReader::new(&self.wire[offset]);
        m.read_bit().ok()?;
        if !full {
            m.read_long().ok()?;
        }
        let mut frame = ArchivedFrame {
            time: m.read_long().ok()?,
            ..Default::default()
        };
        while m.read_bit().ok()? {
            let n = m.read_bits(6).ok()? as u8;
            let old = base.as_ref().and_then(|b| b.clients.get(&n));
            let state = decode::client(&mut m, old.map(|c| &c.state), n).ok()?;
            let player = if m.read_bit().ok()? {
                Some(decode::player(&mut m, old.and_then(|c| c.player.as_ref())).ok()?)
            } else {
                None
            };
            frame.clients.insert(n, Client { state, player });
        }
        loop {
            let n = m.read_bits(10).ok()?;
            if n == 1023 {
                break;
            }
            let mut entity = *self.baselines.get(&(n as u16)).unwrap_or(&ZERO);
            decode::record(&mut m, SHARED_FIELDS, &mut entity, n).ok()?;
            frame.entities.push(entity);
        }
        Some(self.cache(index, full, frame))
    }

    /// SV_GetInterpolatedPlayerOrigin searches up to ten frames on each side
    /// of a shot's server timestamp. Missing history on one side selects the other.
    pub fn interpolated_origin(
        &mut self,
        slot: u8,
        shot_time: i32,
        server_time: i32,
        fps: i32,
        live: Option<&PlayerState>,
    ) -> Option<[f32; 3]> {
        if !(1..=1000).contains(&fps) {
            return None;
        }
        let step = 1000 / fps;
        let base_time = server_time / step * step;
        let shot_age = base_time.wrapping_sub(shot_time) / step * step;
        let mut search = |offset: i32, direction: i32| {
            let mut age = shot_age.wrapping_add(offset * step);
            for _ in 0..10 {
                let frame = self.lookup(&mut age, fps);
                let player = match &frame {
                    Some(frame) => frame.clients.get(&slot).and_then(|c| c.player.as_ref()),
                    None if age < 1 => live.filter(|p| p.words()[3] & 0x800000 != 0),
                    None => None,
                };
                if let Some(player) = player {
                    let origin = std::array::from_fn(|i| f32::from_bits(player.words()[5 + i]));
                    return Some((age, origin));
                }
                age = age.wrapping_add(direction * step);
            }
            None
        };
        // The original stores frame offsets +2 and +1 at 0x0817db80.
        match (search(2, 1), search(1, -1)) {
            (Some((a, first)), Some((b, second))) => {
                let t = (shot_time % step) as f32 / a.wrapping_sub(b) as f32;
                Some(std::array::from_fn(|i| {
                    (f64::from(second[i]) * f64::from(t)
                        + (1.0 - f64::from(t)) * f64::from(first[i])) as f32
                }))
            }
            (Some((_, origin)), None) | (None, Some((_, origin))) => Some(origin),
            (None, None) => None,
        }
    }

    pub fn lookup(&mut self, age: &mut i32, fps: i32) -> Option<Arc<ArchivedFrame>> {
        if !self.enabled || *age <= 0 || fps <= 0 {
            return None;
        }
        let requested = (i64::from(*age) * i64::from(fps)) / 1000;
        let count = requested.min(1200).min(self.next);
        if count < requested {
            *age = (count * 1000 / i64::from(fps)) as i32;
        }
        for n in self.next - count..self.next {
            if let Some(frame) = self.load(n) {
                return Some(frame);
            }
        }
        *age = 0;
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vm::ScriptWorld;

    #[test]
    fn rewind_interpolates_between_aged_frames_and_drops_missing_targets() -> io::Result<()> {
        let entities = GameEntities::default();
        let mut archive = SnapshotArchive {
            enabled: true,
            ..Default::default()
        };
        let mut player = PlayerState::default();
        player.set_field("pm_flags", 0x800000)?;
        let client = ClientState::new(3);
        for index in 0..12 {
            let n = index as f32;
            for (destination, coordinate) in
                player.words_mut()[5..8]
                    .iter_mut()
                    .zip([n * 1.125, n * 2.75, 20. - n * 3.5])
            {
                *destination = coordinate.to_bits();
            }
            archive.record(300 + 50 * index, 20, [(&client, &player)], &entities)?;
        }
        // Original SV_GetInterpolatedPlayerOrigin at shot time 746 and server time 900.
        let origin = archive
            .interpolated_origin(3, 746, 900, 20, Some(&player))
            .expect("historical origin");
        assert_eq!(
            origin.map(f32::to_bits),
            [0x410e8f5c, 0x41ae3d71, 0xc0f70a3e]
        );
        assert!(archive.interpolated_origin(4, 746, 900, 20, None).is_none());
        Ok(())
    }

    #[test]
    fn delta_archive_uses_base_and_rebases_player_and_hud_times() -> io::Result<()> {
        let mut world = ScriptWorld::default();
        let mut archive = SnapshotArchive::default();
        let (slot, _) = world.spawn_entity(b"script_origin", [0.; 3], 0)?;
        let entity = world.entities.get_mut(slot).expect("spawned entity");
        entity.linked = true;
        entity.sv_flags = 8;
        entity.state.set_field("pos.trTime", 950);
        archive.freeze_baselines(&world.entities);
        let mut player = PlayerState::default();
        player.set_field("pm_flags", 0x800000)?;
        player.set_field("commandTime", 999)?;
        player.set_field("viewangles[1]", 0.1_f32.to_bits())?;
        player.words_mut()[0x7a8 / 4] = 1;
        player.words_mut()[0x1728 / 4] = 2;
        player.words_mut()[0x1728 / 4 + 10] = 1100;
        player.words_mut()[0x1728 / 4 + 26] = 990;
        let mut client = ClientState::new(1);
        client.set_name(b"one");
        archive.record(1000, 20, [(&client, &player)], &world.entities)?;
        assert_eq!(archive.next, 0);
        archive.enabled = true;
        archive.record(1000, 20, [(&client, &player)], &world.entities)?;
        let mut age = 500;
        let full = archive.lookup(&mut age, 20).expect("recorded full frame");
        assert_eq!(age, 50);
        let (from, _) = full.player_state(1, 1000).expect("recorded player");
        assert_eq!(from.words()[0x1728 / 4], 2);
        assert_eq!(from.words()[0x7a8 / 4], 0);
        // Unlike a reconstructed delta, the recently cached full frame retains raw floats.
        assert_eq!(from.field("viewangles[1]"), Some(0.1_f32.to_bits()));
        player.set_field("viewangles[1]", 0.2_f32.to_bits())?;
        player.words_mut()[0x344 / 4 + 63] = 47;
        client.set_name(b"two");
        archive.record(1050, 20, [(&client, &player)], &world.entities)?;
        let delta = archive.lookup(&mut 50, 20).expect("recorded delta frame");
        let (result, client) = delta.player_state(1, 1200).expect("rebased player");
        assert_eq!(result.words()[0x1728 / 4 + 10], 1200);
        assert_eq!(result.words()[0x1728 / 4 + 26], 1140);
        assert_eq!(result.field("commandTime"), Some(1149));
        assert_eq!(result.words()[0x7a4 / 4], 150);
        assert_eq!(result.words()[0x344 / 4 + 63], 47);
        assert_eq!(
            result.field("viewangles[1]"),
            Some((36_f32 * 360. / 65536.).to_bits())
        );
        assert_eq!(client.field("name[0]"), Some(u32::from_le_bytes(*b"two\0")));
        assert_eq!(delta.entities[0][0], u32::from(slot));
        Ok(())
    }
}
