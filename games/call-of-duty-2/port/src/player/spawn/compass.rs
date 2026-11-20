//! G_FindTargetPlayerEncoded sends one off-screen teammate per end frame.
use super::{invalid, view_origin};
use crate::{
    snapshot::world::{clusters_visible, exceeds_fog},
    vm::{GameHost, precache::fog_bounds},
};
use std::io;

fn pack(slot: u16, dx: i32, dy: i32, yaw: f32) -> u32 {
    let scale = |d: i32| {
        if d > 1024 {
            1024. / d as f32
        } else if d < -1022 {
            -1022. / d as f32
        } else {
            1.
        }
    };
    let (sx, sy) = (scale(dx), scale(dy));
    let (mut x, mut y) = (dx, dy);
    if sx < sy {
        y = (dy as f32 * sx) as i32;
    } else if sy < sx {
        x = (dx as f32 * sy) as i32;
    }
    let bucket = |d: i32| (((d.clamp(-1022, 1024) + 2) / 4 + 255) as u32) & 511;
    (((f64::from(yaw) * f64::from(0.7111111_f32)) as i32 as u32) << 24)
        | (bucket(y) << 15)
        | (bucket(x) << 6)
        | u32::from(slot)
}

pub(super) fn update(slot: u16, host: &mut GameHost<'_>) -> io::Result<()> {
    let p = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing player"))?;
    let team = p.team();
    let mut found = None;
    if matches!(team, 1 | 2) {
        let origin = view_origin(host, &p.state)?;
        let view_cluster = host
            .collision
            .leaf_cluster(host.collision.point_leaf(origin))
            .unwrap_or(-1);
        let first = p.input.compass_target.map_or(0, |s| u16::from(s) + 1);
        let range = fog_bounds(host.strings).1;
        for offset in 0..64 {
            let target = (first + offset) & 63;
            if slot == target {
                continue;
            }
            let Some(other) = host.world.players.get(target) else {
                continue;
            };
            if other.session_state != 0 || other.team() != team {
                continue;
            }
            let Some(ent) = host.world.entities.get(target) else {
                continue;
            };
            let seen = ent.linked
                && (ent.broadcast_time != 0
                    || (ent.sv_flags & 1 == 0
                        && (ent.sv_flags & 0x18 != 0
                            || (clusters_visible(
                                &ent.link.clusters,
                                ent.link.last_cluster,
                                |c| host.collision.cluster_visible(view_cluster, c),
                            ) && !exceeds_fog(
                                ent.link.absmin,
                                ent.link.absmax,
                                origin,
                                range,
                            )))));
            if seen {
                continue;
            }
            let delta = |i: usize| (f64::from(ent.origin[i] - origin[i]) + 0.5) as i32;
            let encoded = pack(target, delta(0), delta(1), ent.angles[1]);
            if encoded != 0 {
                found = Some((
                    target as u8,
                    encoded,
                    ent.state.field("eFlags").unwrap_or(0) & 0x40_0000 != 0,
                ));
            }
            break;
        }
    }
    let p = host.world.players.get_mut(slot)?;
    p.input.compass_target = found.map(|(target, _, _)| target);
    p.state.words_mut()[0x5a4 / 4] = found.map_or(0, |(_, encoded, _)| encoded);
    if let Some((_, _, pinged)) = found {
        let flags = &mut p.state.words_mut()[0xa0 / 4];
        *flags = (*flags & !0x80_0000) | (u32::from(pinged) << 23);
    }
    Ok(())
}
