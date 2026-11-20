//! Entity_TouchLinked, Entity_TouchList, SV_EntityContact, and G_RunFrame trigger notifications.
use super::{GameEntities, MapEntity};
use crate::collision::CollisionMap;
use crate::script::ScriptProgram;
use crate::vm::{GameHost, fiber::FiberVm, invalid, value::Value};
use std::collections::HashSet;
use std::io;

type Vec3 = [f32; 3];

#[derive(Clone, Copy, Debug)]
pub(super) enum TouchHandler {
    None,
    Trigger,
    Hurt { next_damage: i32 },
    HurtDisabled { next_damage: i32 },
}

#[derive(Clone, Copy, Debug)]
enum Action {
    Notify {
        receiver: u32,
        other: u32,
        event: &'static str,
    },
    Hurt {
        receiver: u32,
        trigger: u32,
        damage: i32,
        flags: i32,
    },
    Delete(u32),
    Pickup {
        item: u32,
        player: u32,
    },
}

#[derive(Debug, Default)]
pub(super) struct Contacts {
    actions: Vec<Action>,
    // Object ids never repeat, so they also distinguish original spawnSerial generations.
    trigger_pairs: Vec<(u32, u32)>,
}

impl Contacts {
    fn trigger(&mut self, receiver: u32, other: u32) {
        if self.trigger_pairs.len() == 256 {
            self.actions.push(Action::Notify {
                receiver,
                other,
                event: "trigger",
            });
        } else {
            self.trigger_pairs.push((receiver, other));
        }
    }
}

impl GameEntities {
    pub fn queue_trigger(&mut self, trigger: u32, other: u32) {
        self.contacts.trigger(trigger, other);
    }
}

/// Split planes from world model bounds in preorder. Lists in each node are sorted by slot.
pub fn sector_path(entity: &MapEntity, collision: &CollisionMap) -> Vec<(usize, f32, u8)> {
    let Some(model) = collision.inline_model(0) else {
        return Vec::new();
    };
    let mut low = model.mins;
    let mut high = model.maxs;
    let mut path = Vec::new();
    loop {
        let ext = [high[0] - low[0], high[1] - low[1]];
        let axis = usize::from(ext[0] <= ext[1]);
        let split = (high[axis] + low[axis]) * 0.5;
        if !split.is_finite() || !(low[axis] < split && split < high[axis]) {
            break;
        }
        let side = if entity.link.area_mins[axis] > split {
            low[axis] = split;
            0
        } else if entity.link.area_maxs[axis] < split {
            high[axis] = split;
            1
        } else {
            break;
        };
        // Area_AllocSplitNode receives the child bounds.
        if (high[0] - low[0]).max(high[1] - low[1]) <= 512. {
            break;
        }
        path.push((axis, split, side));
    }
    path
}

pub fn area_entities(
    entities: &GameEntities,
    collision: &CollisionMap,
    mins: Vec3,
    maxs: Vec3,
    mask: u32,
) -> Vec<u16> {
    let mut entries = Vec::new();
    for (slot, ent) in entities.iter() {
        if ent.link.in_collision_world
            && ent.contents as u32 & mask != 0
            && (0..3).all(|i| ent.link.absmin[i] <= maxs[i] && mins[i] <= ent.link.absmax[i])
        {
            let path: Vec<u8> = sector_path(ent, collision)
                .into_iter()
                .map(|(_, _, side)| side)
                .collect();
            entries.push((path, slot));
        }
    }
    entries.sort_unstable();
    entries.into_iter().map(|(_, slot)| slot).collect()
}

pub fn entity_contact(collision: &CollisionMap, ent: &MapEntity, mins: Vec3, maxs: Vec3) -> bool {
    if ent.sv_flags & 0x60 == 0 {
        return collision
            .trace_entity(ent, [0.; 3], [0.; 3], mins, maxs, u32::MAX)
            .is_some_and(|hit| hit.start_solid);
    }
    let x = (mins[0] + maxs[0]) * 0.5;
    let y = (mins[1] + maxs[1]) * 0.5;
    let radius = ent.maxs[0] + (maxs[0] - x);
    let dx = x - ent.origin[0];
    let dy = y - ent.origin[1];
    let dist = (f64::from(dx) * f64::from(dx) + f64::from(dy) * f64::from(dy)) as f32;
    if ent.sv_flags & 0x20 != 0 {
        maxs[2] > ent.origin[2] && ent.origin[2] + ent.maxs[2] > mins[2] && dist < radius * radius
    } else {
        let r = radius - 64.;
        dist >= r * r
    }
}

pub fn is_touching(receiver: u32, args: &[Value], host: &GameHost<'_>) -> io::Result<Value> {
    let entities = &host.world.entities;
    let Some(Value::Object(other)) = args.first() else {
        return Err(invalid("istouching expects an entity"));
    };
    let lookup = |object| {
        entities
            .slot_for_object(object)
            .and_then(|slot| entities.get(slot))
            .ok_or_else(|| invalid("not a live entity"))
    };
    let mut a = lookup(receiver)?;
    let mut b = lookup(*other)?;
    if a.bmodel || a.sv_flags & 0x60 != 0 {
        std::mem::swap(&mut a, &mut b);
        if a.bmodel || a.sv_flags & 0x60 != 0 {
            return Err(invalid(
                "istouching cannot be called on 2 brush/cylinder entities",
            ));
        }
    }
    let mut mins: Vec3 = std::array::from_fn(|i| a.origin[i] + a.mins[i]);
    let mut maxs: Vec3 = std::array::from_fn(|i| a.origin[i] + a.maxs[i]);
    let z = maxs[2] - mins[2];
    let width = (maxs[0] - mins[0]).max(maxs[1] - mins[1]);
    if z < width {
        let half = (width - z) * 0.5;
        mins[2] -= half;
        maxs[2] += half;
    }
    Ok(Value::Int(i32::from(entity_contact(
        host.collision,
        b,
        mins,
        maxs,
    ))))
}

fn dispatch_contact(
    host: &mut GameHost<'_>,
    slot: u16,
    other: u16,
    linked: bool,
) -> io::Result<()> {
    let player = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing player for touch"))?;
    let player_object = player.object;
    let can_take_damage = host
        .world
        .entities
        .get(slot)
        .is_some_and(|entity| entity.take_damage);
    let Some(entity) = host.world.entities.get(other) else {
        return Ok(());
    };
    let other_object = entity.object;
    let handler = entity.touch_handler;
    let item = entity.state.field("eType") == Some(3);
    if linked && matches!(handler, TouchHandler::None) && !item {
        return Ok(());
    }
    let (a, b) = if linked {
        (other_object, player_object)
    } else {
        (player_object, other_object)
    };
    let queue = &mut host.world.entities.contacts;
    for (receiver, other) in [(a, b), (b, a)] {
        queue.actions.push(Action::Notify {
            receiver,
            other,
            event: "touch",
        });
    }
    if item {
        if !host.world.client_is_spawning {
            queue.actions.push(Action::Pickup {
                item: other_object,
                player: player_object,
            });
        }
        return Ok(());
    }
    let source_flags = || -> io::Result<i32> {
        match host.world.field(other_object, "spawnflags")? {
            Value::Int(n) => Ok(n),
            _ => Ok(0),
        }
    };
    match handler {
        TouchHandler::None | TouchHandler::HurtDisabled { .. } => {}
        TouchHandler::Trigger => {
            let flags = source_flags()?;
            host.world
                .entities
                .contacts
                .trigger(other_object, player_object);
            if flags & 0x10 != 0 {
                host.world
                    .entities
                    .contacts
                    .actions
                    .push(Action::Delete(other_object));
            }
        }
        TouchHandler::Hurt { next_damage } if can_take_damage && next_damage <= host.time_ms => {
            let flags = source_flags()?;
            let Value::Int(damage) = host.world.field(other_object, "dmg")? else {
                return Err(invalid("trigger_hurt has invalid dmg"));
            };
            let trigger = host.world.entities.get_mut(other).expect("touch entity");
            let next_damage = host
                .time_ms
                .wrapping_add(if flags & 0x10 == 0 { 50 } else { 1000 });
            trigger.touch_handler = if flags & 0x20 == 0 {
                TouchHandler::Hurt { next_damage }
            } else {
                TouchHandler::HurtDisabled { next_damage }
            };
            let queue = &mut host.world.entities.contacts;
            queue.trigger(other_object, player_object);
            queue.actions.push(Action::Hurt {
                receiver: player_object,
                trigger: other_object,
                damage,
                flags: if flags & 8 == 0 { 0 } else { 0x10 },
            });
        }
        TouchHandler::Hurt { .. } => {}
    }
    Ok(())
}

pub fn touch_list(slot: u16, touched: &[u16], host: &mut GameHost<'_>) -> io::Result<()> {
    let mut seen = HashSet::new();
    for &other in touched {
        if seen.insert(other) {
            dispatch_contact(host, slot, other, false)?;
        }
    }
    Ok(())
}

pub fn touch_linked(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let player = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing client"))?;
    if player.state.words()[1] as i32 >= 2 {
        return Ok(());
    }
    let origin: Vec3 = std::array::from_fn(|i| f32::from_bits(player.state.words()[0x14 / 4 + i]));
    let ext = [40., 40., 52.];
    let query_mins = std::array::from_fn(|i| origin[i] - ext[i]);
    let query_maxs = std::array::from_fn(|i| origin[i] + ext[i]);
    let candidates = area_entities(
        &host.world.entities,
        host.collision,
        query_mins,
        query_maxs,
        0x405c0008,
    );
    let ent = host
        .world
        .entities
        .get(slot)
        .ok_or_else(|| invalid("missing player entity"))?;
    let mut mins: Vec3 = std::array::from_fn(|i| origin[i] + ent.mins[i]);
    let mut maxs: Vec3 = std::array::from_fn(|i| origin[i] + ent.maxs[i]);
    let z = maxs[2] - mins[2];
    for i in 0..2 {
        let width = maxs[i] - mins[i];
        if z < width {
            let half = (width - z) * 0.5;
            mins[i] += half;
            maxs[i] -= half;
        }
    }
    for other in candidates {
        let ent = host.world.entities.get(other).expect("selected slot");
        let hit = if ent.state.field("eType") == Some(3) {
            crate::vm::items::player_near(origin, &ent.state, host.time_ms)
        } else {
            !matches!(
                ent.touch_handler,
                TouchHandler::None | TouchHandler::HurtDisabled { .. }
            ) && entity_contact(host.collision, ent, mins, maxs)
        };
        if hit {
            dispatch_contact(host, slot, other, true)?;
        }
    }
    Ok(())
}

/// Flush synchronous usercmd actions as soon as the calling game VM is available.
pub fn flush_contacts(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
) -> io::Result<()> {
    for action in std::mem::take(&mut host.world.entities.contacts.actions) {
        let slot = |id| host.world.entities.slot_for_object(id);
        match action {
            Action::Notify {
                receiver,
                other,
                event,
            } if slot(receiver).is_some() && slot(other).is_some() => {
                vm.notify(receiver, event, &[Value::Object(other)])
            }
            Action::Hurt {
                receiver,
                trigger,
                damage,
                flags,
            } => {
                if let (Some(player), Some(t)) = (slot(receiver), slot(trigger)) {
                    crate::combat::damage::trigger_hurt(
                        vm, program, host, player, t, damage, flags,
                    )?;
                }
            }
            Action::Delete(object) if slot(object).is_some() => {
                vm.invoke(program, host, "codescripts/delete", "main", object, vec![])?;
            }
            Action::Pickup { item, player } => {
                if let (Some(i), Some(p)) = (slot(item), slot(player)) {
                    crate::vm::items::touch(vm, host, p, i, true)?;
                }
            }
            _ => {}
        }
    }
    Ok(())
}

/// G_RunFrame copies the pair queue once. Each pass allows one trigger event
/// per receiver, removes entries by swapping from the tail, then runs scripts.
pub fn run_triggers(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
) -> io::Result<()> {
    flush_contacts(vm, program, host)?;
    let mut pairs = std::mem::take(&mut host.world.entities.contacts.trigger_pairs);
    while !pairs.is_empty() {
        let mut seen = HashSet::new();
        let mut i = 0;
        while i < pairs.len() {
            let (a, b) = pairs[i];
            let entities = &host.world.entities;
            if entities.slot_for_object(a).is_some() && entities.slot_for_object(b).is_some() {
                if !seen.insert(a) {
                    i += 1;
                    continue;
                }
                vm.notify(a, "trigger", &[Value::Object(b)]);
            }
            pairs.swap_remove(i);
        }
        vm.run_current(program, host)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn cylinder_touch_uses_open_radius_and_vertical_limits() -> io::Result<()> {
        let cm = crate::collision::tests::map(-1, -2, &[])?;
        let mut entity = MapEntity::new(77, 2);
        super::super::set_radius(&mut entity, 12., 24., 0);
        assert!(entity_contact(&cm, &entity, [10., -3., 0.], [16., 3., 20.]));
        assert!(!entity_contact(
            &cm,
            &entity,
            [12., -3., 0.],
            [18., 3., 20.]
        ));
        assert!(!entity_contact(
            &cm,
            &entity,
            [-3., -3., -10.],
            [3., 3., 0.]
        ));
        assert!(!entity_contact(
            &cm,
            &entity,
            [-3., -3., 24.],
            [3., 3., 30.]
        ));
        Ok(())
    }
    #[test]
    fn full_trigger_queue_preserves_old_pairs_and_notifies_the_new_pair() {
        let mut queue = Contacts::default();
        for other in 0..257 {
            queue.trigger(33, other);
        }
        assert_eq!(queue.trigger_pairs.len(), 256);
        assert_eq!(queue.trigger_pairs[255], (33, 255));
        assert!(matches!(
            queue.actions.as_slice(),
            [Action::Notify {
                receiver: 33,
                other: 256,
                event: "trigger"
            }]
        ));
    }
}
