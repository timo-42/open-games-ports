use crate::{
    assets::weapons::{WeaponDefinition, WeaponRegistry},
    snapshot::PlayerState,
};

pub(super) const WEAPON: usize = 0xd4 / 4;
pub(super) const OFFHAND: usize = 0xd0 / 4;
const OWNED: usize = 0x544 / 4;
const SLOT_BYTES: usize = 0x554 / 4;
const RECHAMBER: usize = 0x55c / 4;

pub(crate) fn registered(
    registry: &WeaponRegistry,
) -> impl Iterator<Item = (usize, &WeaponDefinition)> {
    (1..128).map_while(|id| Some((id, registry.get(id)?)))
}

pub(crate) fn owned(ps: &PlayerState, id: usize) -> bool {
    id < 128 && ps.words()[OWNED + id / 32] & (1 << (id % 32)) != 0
}
fn mark(ps: &mut PlayerState, id: usize) {
    ps.words_mut()[OWNED + id / 32] |= 1 << (id % 32);
}
fn unmark(ps: &mut PlayerState, id: usize) {
    ps.words_mut()[OWNED + id / 32] &= !(1 << (id % 32));
}
pub(crate) fn held(ps: &PlayerState, slot: usize) -> usize {
    ((ps.words()[SLOT_BYTES] >> (8 * slot)) & 0xff) as usize
}
pub(crate) fn set_slot(ps: &mut PlayerState, slot: usize, id: usize) {
    let shift = 8 * slot;
    let word = &mut ps.words_mut()[SLOT_BYTES];
    *word = (*word & !(0xff << shift)) | ((id as u32) << shift);
}

pub(crate) fn current(ps: &PlayerState, registry: &WeaponRegistry) -> usize {
    let flags = ps.words()[3];
    if flags & 0x20000 != 0 {
        registered(registry)
            .find(|(_, d)| d.weapon_type == 3)
            .map_or(0, |(id, _)| id)
    } else if flags & 16 != 0 {
        ps.words()[OFFHAND] as usize
    } else {
        ps.words()[WEAPON] as usize
    }
}

pub(crate) fn selectable(ps: &PlayerState, id: usize, def: &WeaponDefinition) -> bool {
    owned(ps, id)
        && (def.offhand_class != 0
            || held(ps, 1) == id
            || held(ps, 2) == id
            || def.alt_weapon_index == id)
}

pub fn selectable_weapons(ps: &PlayerState, weapons: &WeaponRegistry) -> [u32; 4] {
    let mut bits = [0; 4];
    for (id, def) in registered(weapons) {
        if selectable(ps, id, def) {
            bits[id / 32] |= 1 << (id % 32);
        }
    }
    bits
}

pub(crate) fn ammo(def: &WeaponDefinition) -> usize {
    0x144 / 4 + def.ammo_index
}
pub(crate) fn clip(def: &WeaponDefinition) -> usize {
    0x344 / 4 + def.clip_index
}
pub(super) fn total(ps: &PlayerState, def: &WeaponDefinition) -> i32 {
    (ps.words()[ammo(def)] as i32).wrapping_add(ps.words()[clip(def)] as i32)
}

pub(crate) fn find_slot(ps: &PlayerState, registry: &WeaponRegistry, id: usize) -> Option<usize> {
    if !owned(ps, id) {
        return None;
    }
    let mut current = id;
    let mut visited = [false; 128];
    while current < 128 && !visited[current] {
        visited[current] = true;
        let def = registry.get(current)?;
        if !(1..=2).contains(&def.weapon_slot) {
            break;
        }
        if let Some(slot) = (1..=2).find(|&n| held(ps, n) == current) {
            return Some(slot);
        }
        current = def.alt_weapon_index;
        if current == 0 {
            break;
        }
    }
    None
}

/// BG_TakeWeapon (0x080ebffe). The stock stack replacement test refers to
/// the removed definition when scanning potential replacements.
pub(crate) fn take(ps: &mut PlayerState, reg: &WeaponRegistry, id: usize) {
    if !owned(ps, id) {
        return;
    }
    let def = reg.get(id).expect("registered weapon");
    if let Some(slot) = find_slot(ps, reg, id) {
        let replacement = if def.slot_stackable {
            registered(reg)
                .map(|(id, _)| id)
                .find(|&id| owned(ps, id) && find_slot(ps, reg, id).is_none())
                .unwrap_or(0)
        } else {
            0
        };
        set_slot(ps, slot, replacement);
    }
    let mut current = id;
    while owned(ps, current) {
        unmark(ps, current);
        current = reg.get(current).expect("registered alt").alt_weapon_index;
        if current == 0 {
            break;
        }
    }
    if ps.words()[WEAPON] as usize == id {
        ps.words_mut()[WEAPON] = 0;
    }
}

/// G_TouchTriggerEntity (0x08120b26) gives a weapon and publishes offhand selection.
pub(crate) fn give(
    ps: &mut PlayerState,
    reg: &WeaponRegistry,
    id: usize,
    commands: &mut impl crate::level::CommandSink,
) -> bool {
    let Some(def) = reg.get(id) else {
        return false;
    };
    if id >= 128 || owned(ps, id) || matches!(def.weapon_class, 7 | 8) {
        return false;
    }
    mark(ps, id);
    ps.words_mut()[RECHAMBER + id / 32] &= !(1 << (id % 32));
    if def.weapon_class == 9 {
        return true;
    }
    if def.offhand_class == 0 {
        if (1..=2).contains(&def.weapon_slot)
            && let Some(slot) = (1..=2).find(|&s| held(ps, s) == 0)
        {
            set_slot(ps, slot, id);
        }
        let mut alt = def.alt_weapon_index;
        while alt != 0 && alt < 128 && !owned(ps, alt) {
            mark(ps, alt);
            alt = reg.get(alt).expect("registered alternate").alt_weapon_index;
        }
    } else {
        let previous = ps.words()[OFFHAND] as usize;
        let selected = if previous == 0 {
            Some(id)
        } else {
            reg.get(previous)
                .filter(|old| total(ps, old) < 1)
                .map(|old| {
                    registered(reg)
                        .find(|(other, def)| {
                            def.offhand_class == old.offhand_class
                                && owned(ps, *other)
                                && total(ps, def) != 0
                        })
                        .map_or(id, |(id, _)| id)
                })
        };
        if let Some(selected) = selected {
            ps.words_mut()[OFFHAND] = selected as u32;
            commands.push(format!("C {selected}").into_bytes());
        }
    }
    true
}

/// BG_GetWeaponAmmoRoom counts each ammo pool once across weapons sharing a cap.
pub(crate) fn shared_room(ps: &PlayerState, reg: &WeaponRegistry, def: &WeaponDefinition) -> i32 {
    let mut counted = [false; 256];
    let mut room = def.handling.shared_ammo_cap;
    for (id, other) in registered(reg) {
        if !owned(ps, id) || other.shared_ammo_cap_index != def.shared_ammo_cap_index {
            continue;
        }
        let index = if other.handling.clip_only {
            clip(other)
        } else {
            ammo(other)
        };
        let flag = &mut counted[index - 0x144 / 4];
        if !*flag {
            *flag = true;
            room = room.wrapping_sub(ps.words()[index] as i32);
        }
    }
    room
}

pub(crate) fn give_ammo(
    ps: &mut PlayerState,
    reg: &WeaponRegistry,
    id: usize,
    amount: i32,
    fill: bool,
    commands: &mut impl crate::level::CommandSink,
) -> i32 {
    let def = reg.get(id).expect("registered weapon");
    let reserve = ammo(def);
    let magazine = clip(def);
    let before = total(ps, def);
    ps.words_mut()[reserve] = ps.words()[reserve].wrapping_add(amount as u32);
    if def.handling.clip_only {
        give(ps, reg, id, commands);
    }
    if (fill || def.handling.clip_only) && id != 0 {
        let count = (def.clip_size - ps.words()[magazine] as i32).min(ps.words()[reserve] as i32);
        ps.words_mut()[reserve] = ps.words()[reserve].wrapping_sub(count as u32);
        ps.words_mut()[magazine] = ps.words()[magazine].wrapping_add(count as u32);
    }
    ps.words_mut()[reserve] = if def.handling.clip_only {
        0
    } else {
        (ps.words()[reserve] as i32).min(def.max_ammo) as u32
    };
    ps.words_mut()[magazine] = (ps.words()[magazine] as i32).min(def.clip_size) as u32;
    if def.shared_ammo_cap_index.is_some() {
        let room = shared_room(ps, reg, def);
        if room < 0 {
            let index = if def.handling.clip_only {
                magazine
            } else {
                reserve
            };
            ps.words_mut()[index] = (ps.words()[index] as i32 + room).max(0) as u32;
            if def.handling.clip_only && ps.words()[index] == 0 {
                take(ps, reg, id);
                return 0;
            }
        }
    }
    total(ps, def).wrapping_sub(before)
}
