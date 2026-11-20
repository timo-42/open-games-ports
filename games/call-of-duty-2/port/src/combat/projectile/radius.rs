//! G_RadiusDamage and G_CanDamage use different sample points for players and brush entities.
use super::*;
use crate::vm::game_entities::touch::area_entities;

fn exposure(host: &GameHost, slot: u16, origin: Vec3) -> io::Result<f32> {
    let entity = host.world.entities.get(slot).ok_or_else(missing_entity)?;
    let clear = |from, to| {
        host.collision.point_clear(
            &host.world.entities,
            host.models,
            from,
            to,
            slot,
            0x802891,
            false,
        )
    };
    let Some(p) = host.world.players.get(slot) else {
        let mid: Vec3 =
            std::array::from_fn(|i| (entity.link.absmin[i] + entity.link.absmax[i]) * 0.5);
        for [x, y] in [[0., 0.], [15., 15.], [15., -15.], [-15., 15.], [-15., -15.]] {
            if clear([mid[0] + x, mid[1] + y, mid[2]], origin)? {
                return Ok(1.);
            }
        }
        return Ok(0.);
    };
    let angles = ["viewangles[0]", "viewangles[1]", "viewangles[2]"]
        .map(|key| f32::from_bits(p.state.field(key).expect("registered playerstate field")));
    let eye = damage::shot::eye(host, &p.state, angles);
    let height = (eye[2] - entity.origin[2]) * 0.5;
    let mut dir = sub(origin, entity.origin);
    dir[2] = 0.;
    dir = normalize(dir);
    let perpendicular = [-dir[1], dir[0], 0.];
    let mid: Vec3 = std::array::from_fn(|i| (eye[i] + entity.origin[i]) * 0.5);
    let mut hits = 0;
    for (offset, z) in [
        (0., 0.),
        (15., height),
        (15., -height),
        (-15., height),
        (-15., -height),
    ] {
        let mut dest = add(mid, offset, perpendicular);
        dest[2] += z;
        if clear(origin, dest)? {
            hits += 1;
        }
    }
    Ok((hits as f32 / 3.).min(1.))
}
pub(super) struct Blast {
    pub origin: Vec3,
    pub radius: f32,
    pub inner: f32,
    pub outer: f32,
    pub inflictor: Option<u16>,
    pub attacker: u16,
    pub weapon: usize,
    pub means: usize,
    pub ignore: Option<u16>,
    pub ignore_players: bool,
}
impl Blast {
    pub fn apply(
        &self,
        vm: &mut FiberVm,
        program: &ScriptProgram,
        host: &mut GameHost,
    ) -> io::Result<()> {
        let origin = self.origin;
        let radius = self.radius.max(1.);
        let extent = radius * std::f32::consts::SQRT_2;
        for victim in area_entities(
            &host.world.entities,
            host.collision,
            origin.map(|x| x - extent),
            origin.map(|x| x + extent),
            u32::MAX,
        ) {
            if Some(victim) == self.ignore {
                continue;
            }
            let Some(e) = host.world.entities.get(victim) else {
                continue;
            };
            if !e.take_damage || (self.ignore_players && host.world.players.get(victim).is_some()) {
                continue;
            }
            let mut dir = sub(e.origin, origin);
            let displacement = if e.bmodel {
                std::array::from_fn(|i| {
                    (e.link.absmin[i] - origin[i])
                        .max(origin[i] - e.link.absmax[i])
                        .max(0.)
                })
            } else {
                dir
            };
            let distance = length(displacement);
            if distance >= radius {
                continue;
            }
            let damage = self.outer + (1. - distance / radius) * (self.inner - self.outer);
            let mut fraction = exposure(host, victim, origin)?;
            if fraction == 0. {
                let mid: Vec3 =
                    std::array::from_fn(|i| (e.link.absmin[i] + e.link.absmax[i]) * 0.5);
                if length(sub(mid, origin)) >= radius * 0.2
                    || host
                        .collision
                        .box_trace(
                            &host.world.entities,
                            origin,
                            mid,
                            [0.; 3],
                            [0.; 3],
                            1023,
                            0x811,
                        )
                        .fraction
                        >= 1.
                {
                    continue;
                }
                fraction = 0.1;
            }
            dir[2] += 24.;
            damage::Hit {
                victim,
                inflictor: self.inflictor,
                attacker: Some(self.attacker),
                damage: (damage * fraction) as i32,
                flags: 1,
                means: self.means,
                weapon: self.weapon,
                point: Some(origin),
                direction: Some(dir),
                location: 0,
                offset: 0,
            }
            .damage(vm, program, host)?;
        }
        Ok(())
    }
}
