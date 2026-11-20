//! G_MoverPush and G_TryPushingEntity. Untouched entities stay available to stationary traces.
use super::{
    CollisionMap, ScriptWorld, Vec3, io,
    trajectory::{add, length, subtract},
};
use crate::vm::{
    entity_methods::relink,
    game_entities::{frame::basis, touch::area_entities},
    items::motion::set_vector,
};

fn test_position(world: &ScriptWorld, map: &CollisionMap, slot: u16, origin: Vec3) -> Option<u16> {
    let check = world.entities.get(slot)?;
    let mask = if check.clip_mask == 0 {
        0x811
    } else if check.contents & 0x4000000 != 0 {
        return None;
    } else {
        check.clip_mask
    };
    let pass = if check.state.field("eType") == Some(4) {
        check.owner_num
    } else {
        slot
    };
    let tr = map.box_trace(
        &world.entities,
        origin,
        origin,
        check.mins,
        check.maxs,
        pass,
        mask,
    );
    (tr.start_solid || tr.all_solid).then_some(tr.entity)
}

fn move_pushed(world: &mut ScriptWorld, slot: u16, origin: Vec3, yaw: i32) -> io::Result<()> {
    let check = world.entities.get_mut(slot).expect("pushed entity");
    check.origin = origin;
    set_vector(&mut check.state, "pos.trBase", origin);
    if let Some(p) = world.players.get(slot) {
        let prev = p.state.field("delta_angles[1]").expect("client yaw");
        let p = world.players.get_mut(slot)?;
        p.state
            .set_field("delta_angles[1]", prev.wrapping_add(yaw as u32))?;
        for (key, v) in ["origin[0]", "origin[1]", "origin[2]"]
            .into_iter()
            .zip(origin)
        {
            p.state.set_field(key, v.to_bits())?;
        }
    }
    Ok(())
}

fn try_pushing(
    world: &mut ScriptWorld,
    map: &CollisionMap,
    slot: u16,
    pusher: u16,
    translation: Vec3,
    rotation: Vec3,
) -> io::Result<Push> {
    let check = world.entities.get(slot).expect("pushed entity");
    let original = check.origin;
    let new_origin = add(original, translation);
    let pivot = world.entities.get(pusher).expect("current mover").origin;
    let relative = subtract(new_origin, pivot);
    let axis = basis(rotation);
    let rotated = std::array::from_fn(|i| {
        (f64::from(relative[0]) * f64::from(axis[0][i])
            + f64::from(relative[1]) * f64::from(axis[1][i])
            + f64::from(relative[2]) * f64::from(axis[2][i])) as f32
    });
    let adjusted = add(new_origin, subtract(rotated, relative));
    let limit = check.maxs[0] * 0.5;
    let mut clear = test_position(world, map, slot, adjusted)
        .is_none()
        .then_some(adjusted);
    if clear.is_none() && limit > 4. {
        // Only player-sized candidates reach this path. The original tests
        // 0, -4, +4, ... on Z and -4, +4, -8, +8, ... on X and Y.
        let offsets = (1..)
            .map(|i| i as f32 * 4.)
            .take_while(|&v| v < limit)
            .flat_map(|v| [-v, v]);
        'probes: for z in std::iter::once(0.).chain(offsets.clone()) {
            for x in offsets.clone() {
                for y in offsets.clone() {
                    let probe = add(adjusted, [x, y, z]);
                    if test_position(world, map, slot, probe).is_none() {
                        clear = Some(probe);
                        break 'probes;
                    }
                }
            }
        }
    }
    if let Some(target) = clear {
        let check = world.entities.get_mut(slot).expect("pushed entity");
        if check.state.field("groundEntityNum") != Some(u32::from(pusher)) {
            check.state.set_field("groundEntityNum", 1023);
        }
        let yaw = i32::from(crate::player::spawn::short_angle(rotation[1]));
        move_pushed(world, slot, target, yaw)?;
        Ok(Push::Moved {
            origin: original,
            yaw,
        })
    } else if test_position(world, map, slot, original).is_none() {
        world
            .entities
            .get_mut(slot)
            .expect("pushed entity")
            .state
            .set_field("groundEntityNum", 1023);
        Ok(Push::Clear)
    } else {
        Ok(Push::Blocked)
    }
}
enum Push {
    Moved { origin: Vec3, yaw: i32 },
    Clear,
    Blocked,
}

fn link(world: &mut ScriptWorld, slot: u16, map: &CollisionMap) -> io::Result<()> {
    let e = world.entities.get_mut(slot).expect("pushed entity");
    relink(e, map, e.model_index != 0)
}

pub(super) fn move_and_push(
    world: &mut ScriptWorld,
    slot: u16,
    map: &CollisionMap,
    translation: Vec3,
    rotation: Vec3,
) -> io::Result<bool> {
    let mover = world.entities.get(slot).expect("current mover");
    let (low, high) = if mover.angles != [0.; 3] || rotation != [0.; 3] {
        let radius = length(std::array::from_fn(|i| {
            mover.mins[i].abs().max(mover.maxs[i].abs())
        }));
        (
            mover.origin.map(|x| x - radius),
            mover.origin.map(|x| x + radius),
        )
    } else {
        (mover.link.absmin, mover.link.absmax)
    };
    let dest_low = add(low, translation);
    let dest_high = add(high, translation);
    let sweep_low = std::array::from_fn(|i| low[i].min(dest_low[i]));
    let sweep_high = std::array::from_fn(|i| high[i].max(dest_high[i]));
    let candidates = area_entities(&world.entities, map, sweep_low, sweep_high, 0x2000180);
    let mover = world.entities.get_mut(slot).expect("current mover");
    mover.origin = add(mover.origin, translation);
    mover.angles = add(mover.angles, rotation);
    link(world, slot, map)?;
    let pushed: Vec<_> = candidates
        .into_iter()
        .filter(|&n| {
            let check = world.entities.get(n).expect("area entity");
            let kind = check.state.field("eType").expect("entity type");
            if n == slot || !(matches!(kind, 1 | 3 | 4) || ((64..72).contains(&n) && kind == 2)) {
                return false;
            }
            check.state.field("groundEntityNum") == Some(u32::from(slot))
                || ((0..3).all(|i| {
                    check.link.absmin[i] < dest_high[i] && dest_low[i] < check.link.absmax[i]
                }) && test_position(world, map, n, check.origin) == Some(slot))
        })
        .collect();
    for &id in &pushed {
        let e = world.entities.get_mut(id).expect("pushed entity");
        e.linked = false;
        e.link.in_collision_world = false;
    }
    let mut undo = Vec::new();
    let mut blocked = false;
    for &n in &pushed {
        match try_pushing(world, map, n, slot, translation, rotation)? {
            Push::Moved { origin, yaw } => {
                undo.push((n, origin, yaw));
                link(world, n, map)?;
            }
            Push::Clear => link(world, n, map)?,
            Push::Blocked => {
                if world
                    .entities
                    .get(n)
                    .expect("pushed entity")
                    .state
                    .field("eType")
                    == Some(3)
                {
                    link(world, n, map)?;
                } else {
                    blocked = true;
                }
            }
        }
    }
    for n in pushed {
        link(world, n, map)?;
    }
    if blocked {
        for (n, origin, yaw) in undo.into_iter().rev() {
            move_pushed(world, n, origin, -yaw)?;
            link(world, n, map)?;
        }
    }
    Ok(blocked)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn platform_transports_rider_and_tests_its_original_position_when_blocked() -> io::Result<()> {
        let map = crate::collision::tests::map(-1, -2, &[])?;
        // State sampled from G_MoverPush on 1.3, with a player-shaped entity
        // riding the platform and an optional solid capsule at its destination.
        for (turn, translation, wall_size, want, grounded, blocked) in [
            (
                [0., 0., 0.],
                [5., 3., 1.],
                0.,
                [815., -1197., 3519.],
                true,
                false,
            ),
            (
                [0., 90., 0.],
                [5., 3., 1.],
                0.,
                [805., -1187., 3519.],
                true,
                false,
            ),
            (
                [20., 30., 40.],
                [-7., 1., 2.],
                0.,
                [811.00726, -1201.9636, 3511.537],
                true,
                false,
            ),
            (
                [0., 0., 0.],
                [12., 0., 0.],
                7.,
                [810., -1200., 3518.],
                false,
                false,
            ),
            (
                [0., 0., 0.],
                [12., 0., 0.],
                17.,
                [810., -1200., 3518.],
                true,
                true,
            ),
        ] {
            let mut world = ScriptWorld::default();
            let mut spawn = |kind, origin, size: f32, contents| -> io::Result<u16> {
                let (n, _) = world.spawn_entity(b"test", origin, 0)?;
                let entity = world.entities.get_mut(n).expect("test entity");
                entity.mins = [-size; 3];
                entity.maxs = [size; 3];
                entity.contents = contents;
                entity.state.set_field("eType", kind);
                relink(entity, &map, false)?;
                Ok(n)
            };
            let platform = spawn(6, [800., -1200., 3500.], 15., 1)?;
            let rider = spawn(1, [810., -1200., 3518.], 2., 128)?;
            if wall_size > 0. {
                spawn(0, [822., -1200., 3518.], wall_size, 1)?;
            }
            world
                .entities
                .get_mut(rider)
                .expect("test rider")
                .state
                .set_field("groundEntityNum", u32::from(platform));
            assert_eq!(
                move_and_push(&mut world, platform, &map, translation, turn)?,
                blocked
            );
            let rider = world.entities.get(rider).expect("test rider");
            assert_eq!(rider.origin, want);
            assert_eq!(
                rider.state.field("groundEntityNum"),
                Some(if grounded { u32::from(platform) } else { 1023 })
            );
        }
        Ok(())
    }
}
