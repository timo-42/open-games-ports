//! SV_BuildClientSnapshot live world path (0x0809a408).
use super::{ClientState, PlayerState, Snapshot};
use crate::collision::CollisionMap;
use crate::vm::game_entities::{GameEntities, MapEntity};
use std::io;

pub(super) fn view_origin(player: &PlayerState) -> [f32; 3] {
    let field = |name| f32::from_bits(player.field(name).expect("defined player netfield"));
    let mut point = [
        field("origin[0]"),
        field("origin[1]"),
        field("origin[2]") + field("viewHeightCurrent"),
    ];
    let lean = field("leanf");
    if lean != 0.0 {
        // BG_ApplyLeanRotationOffset applies the right vector of (0, yaw, roll).
        let eased = ((2.0 - f64::from(lean.abs())) * f64::from(lean)) as f32;
        let sin_cos = |degrees: f32| {
            let radians = (f64::from(degrees) * (std::f64::consts::PI / 180.0)) as f32;
            let (sin, cos) = f64::from(radians).sin_cos();
            (sin as f32, cos as f32)
        };
        let (sy, cy) = sin_cos(field("viewangles[1]"));
        let (sr, cr) = sin_cos(eased * 16.);
        let right = [cr * sy, -cr * cy, -sr];
        let distance = f64::from(eased * 20.);
        for (coordinate, axis) in point.iter_mut().zip(right) {
            *coordinate = (f64::from(*coordinate) + distance * f64::from(axis)) as f32;
        }
    }
    point
}

pub(crate) fn exceeds_fog(min: [f32; 3], max: [f32; 3], eye: [f32; 3], range: f32) -> bool {
    if range == 0.0 || range == f32::MAX {
        return false;
    }
    let mut distance_squared = 0.0;
    for axis in 0..3 {
        let lo = min[axis] - eye[axis];
        let hi = max[axis] - eye[axis];
        if lo * hi <= 0.0 {
            continue;
        }
        let low_squared = lo * lo;
        let high_squared = hi * hi;
        distance_squared += if low_squared > high_squared {
            high_squared
        } else {
            low_squared
        };
    }
    distance_squared > range
}

pub(crate) fn clusters_visible(clusters: &[i16], last: i16, visible: impl Fn(i16) -> bool) -> bool {
    if clusters.iter().copied().any(&visible) {
        return true;
    }
    let Some(&start) = clusters.last() else {
        return false;
    };
    if last == 0 {
        return false;
    }
    // The original compares the *stopping cluster* with lastCluster, even
    // when the scan passed lastCluster. Preserve the inclusive-loop boundary.
    let stopped = (start..=last).find(|&c| visible(c));
    stopped != Some(last)
}

fn visible_entity(entity: &mut MapEntity, slot: u16, excluded: u16, time: i32) -> bool {
    if !entity.linked || slot == excluded {
        return false;
    }
    if entity.broadcast_time == 0 {
        return entity.sv_flags & 1 == 0
            && entity.client_mask[usize::from(excluded / 32)] & (1 << (excluded % 32)) == 0;
    }
    if entity.broadcast_time > 0 && entity.broadcast_time.wrapping_sub(time) < 0 {
        // The expiry frame falls through to PVS without repeating the mask test.
        entity.broadcast_time = 0;
    }
    true
}

/// Uses the recipient's current player state (including follow-camera state)
/// and client states for occupied slots whose network state is above zombie.
pub fn build_live(
    server_time: i32,
    flags: u8,
    player: &PlayerState,
    clients: Vec<ClientState>,
    entities: &mut GameEntities,
    collision: &CollisionMap,
    fog_distance_squared: f32,
) -> io::Result<Snapshot> {
    let recipient = player.field("clientNum").expect("defined player netfield");
    // clientMask has two 32-bit words, one bit per player slot.
    if recipient >= 64 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "invalid snapshot view client",
        ));
    }
    let eye = view_origin(player);
    let observer = collision.leaf_cluster(collision.point_leaf(eye));
    let mut frame = Snapshot {
        server_time,
        flags,
        player: player.clone(),
        clients,
        entities: Vec::new(),
    };
    if let Some(cluster) = observer.filter(|&cluster| cluster >= 0) {
        for (slot, entity) in entities.iter_mut() {
            if !visible_entity(entity, slot, recipient as u16, server_time) {
                continue;
            }
            if entity.broadcast_time != 0
                || entity.sv_flags & 0x18 != 0
                || (clusters_visible(&entity.link.clusters, entity.link.last_cluster, |c| {
                    collision.cluster_visible(cluster, c)
                }) && !exceeds_fog(
                    entity.link.absmin,
                    entity.link.absmax,
                    eye,
                    fog_distance_squared,
                ))
            {
                frame.entities.push(entity.state.clone());
            }
        }
    }
    Ok(frame)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lean_camera_and_broadcast_expiration() -> io::Result<()> {
        let mut player = PlayerState::default();
        for (field, value) in [
            ("origin[0]", 11.3125_f32),
            ("origin[1]", -5.125),
            ("origin[2]", 1.625),
            ("viewHeightCurrent", 5.),
            ("viewangles[1]", 12.2),
            ("leanf", -0.1),
        ] {
            player.set_field(field, value.to_bits())?;
        }
        assert_eq!(
            view_origin(&player).map(f32::to_bits),
            [0x41282b67, 0xbfb54102, 0x40cd8d1a]
        );
        let mut world = crate::vm::ScriptWorld::default();
        let (slot, _) = world.spawn_entity(b"script_origin", [0.; 3], 0)?;
        let entity = world.entities.get_mut(slot).expect("spawned emitter");
        entity.linked = true;
        entity.client_mask[1] = 1 << 4;
        assert!(!visible_entity(entity, slot, 36, 500));
        entity.broadcast_time = 500;
        assert!(visible_entity(entity, slot, 36, 500));
        assert_eq!(entity.broadcast_time, 500);
        assert!(visible_entity(entity, slot, 36, 501));
        assert_eq!(entity.broadcast_time, 0);
        assert!(!visible_entity(entity, slot, 36, 501));
        Ok(())
    }

    #[test]
    fn last_cluster_overflow_and_distance_boundaries() {
        assert!(!clusters_visible(&[], 17, |_| true));
        assert!(clusters_visible(&[3, 8], 11, |c| c == 9));
        assert!(!clusters_visible(&[3, 8], 11, |c| c == 11));
        assert!(clusters_visible(&[3, 8], 11, |_| false));
        assert!(!clusters_visible(&[3, 8], 0, |c| c == 9));
        let low = [3., 4., -5.];
        let high = [10., 8., 5.];
        assert!(exceeds_fog(low, high, [0.; 3], 24.));
        assert!(!exceeds_fog(low, high, [0.; 3], 25.));
        assert!(!exceeds_fog(low, high, [0.; 3], f32::MAX));
    }
}
