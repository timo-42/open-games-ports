//! Missile bounces, timed think, direct hits and radius damage (G_CanDamage).
use super::*;
use crate::vm::{effects::direction_index, items::NativeRandom};

fn impact_angles(
    e: &mut MapEntity,
    time: i32,
    normal: Vec3,
    stop: bool,
    native: &mut NativeRandom,
) -> Vec3 {
    let dst = vector(&e.state, "apos.trBase");
    let mut spin = vector(&e.state, "apos.trDelta");
    let mut a = if field(&e.state, "apos.trType") == 0 {
        dst
    } else {
        add(
            dst,
            (time.wrapping_sub(field(&e.state, "apos.trTime") as i32) as f32) * 0.001,
            spin,
        )
    };
    if normal[2] <= 0.1 {
        if !stop {
            spin[0] = angle_mod((native.draw() as i32 & 127) as f32 - 63. + spin[0]);
            set_vector(&mut e.state, "apos.trDelta", spin);
        }
        return a;
    }
    let fwd = axes([0., a[1], 0.])[0];
    let pitch = angles(add(fwd, -(dot(normal, fwd) as f32), normal))[0];
    let mut delta = pitch - a[0];
    while delta > 180. {
        delta -= 360.;
    }
    while delta < -180. {
        delta += 360.;
    }
    if !stop {
        let jitter = f64::from(0.85f32) + f64::from(0.3f32) * native.fraction();
        spin[0] = (f64::from(spin[0]) * jitter) as f32;
        if delta.abs() < 80. {
            spin[0] = -spin[0];
        }
        set_vector(&mut e.state, "apos.trDelta", spin);
    }
    a[0] = angle_mod(a[0]);
    if a[0] > 180. {
        a[0] -= 360.;
    }
    a[0] = if stop || delta.abs() < 45. {
        angle_mod(if a[0].abs() > 90. {
            pitch + 180.
        } else {
            pitch
        })
    } else if delta.abs() < 80. {
        angle_mod(a[0] + delta * 0.25)
    } else {
        angle_mod(a[0])
    };
    a
}

fn bounce_velocity(velocity: Vec3, normal: Vec3, [parallel, perpendicular]: [f32; 2]) -> Vec3 {
    let projection = dot(velocity, normal) as f32;
    let reflected = add(velocity, -2. * projection, normal);
    let speed = length(velocity);
    if speed <= 0. || projection > 0. {
        return reflected;
    }
    // The server stores the normal speed ratio as a float at 0x0810d310
    // before using extended precision for the material interpolation.
    let normal_ratio = projection / -speed;
    let factor = (f64::from(parallel)
        + (f64::from(perpendicular) - f64::from(parallel)) * f64::from(normal_ratio))
        as f32;
    reflected.map(|v| v * factor)
}
fn bounce(host: &mut GameHost, slot: u16, tr: &TraceResult, previous: i32) -> io::Result<bool> {
    let now = host.time_ms;
    let e = host.world.entities.get(slot).ok_or_else(missing_entity)?;
    let underwater = water(host, e.origin);
    let def = host
        .weapons
        .get(field(&e.state, "weapon") as usize)
        .ok_or_else(missing_weapon)?;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    let time = previous
        .wrapping_add((f64::from(now.wrapping_sub(previous)) * f64::from(tr.fraction)) as i32);
    let mut vel = vector(&e.state, "pos.trDelta");
    if field(&e.state, "pos.trType") == 5 {
        let dt = time.wrapping_sub(field(&e.state, "pos.trTime") as i32) as f32 * 0.001;
        vel[2] = (f64::from(vel[2]) - 800. * f64::from(dt)) as f32;
    }
    if tr.normal[2] > 0.7 {
        e.state.set_field("groundEntityNum", u32::from(tr.entity));
    }
    let index = ((tr.surface_flags >> 20) & 31) as usize;
    let dissipation = def
        .projectile
        .bounce
        .get(index)
        .copied()
        .unwrap_or(def.projectile.bounce[0]);
    let reflected = bounce_velocity(vel, tr.normal, dissipation);
    let stop = tr.normal[2] > 0.7 && length(reflected) < 20.;
    let a = impact_angles(e, time, tr.normal, stop, &mut host.world.native_random);
    if stop {
        stationary(e, e.origin, 0);
        fixed_angles(e, a);
        return Ok(false);
    }
    set_vector(&mut e.state, "pos.trDelta", reflected);
    e.origin = add(
        e.origin,
        0.1,
        [tr.normal[0], tr.normal[1], tr.normal[2].min(0.)],
    );
    set_vector(&mut e.state, "pos.trBase", e.origin);
    set_vector(&mut e.state, "apos.trBase", a);
    e.state.set_field("pos.trTime", now as u32);
    e.state.set_field("apos.trTime", now as u32);
    Ok(!underwater && length(sub(reflected, vel)) > 100.)
}

pub(super) fn contact(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
    mut trace: TraceResult,
    hit: Vec3,
    previous: i32,
) -> io::Result<()> {
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    let weapon = field(&e.state, "weapon") as usize;
    let def = host.weapons.get(weapon).ok_or_else(missing_weapon)?;
    let rocket = def.weapon_type == 2;
    let direct = def.projectile.damage;
    let owner = e.owner_num;
    let origin = e.origin;
    let surface = (trace.surface_flags >> 20) & 31;
    e.state.set_field("surfType", surface);
    let bounces = field(&e.state, "eFlags") & 0x1000000 != 0;
    let target_player = host.world.players.get(trace.entity).is_some();
    let target = host
        .world
        .entities
        .get(trace.entity)
        .is_some_and(|e| e.take_damage);
    if (!target && bounces) || (target && direct == 0) {
        if target_player && trace.surface_flags == 0 {
            trace.surface_flags = 7 << 20;
        }
        if bounce(host, slot, &trace, previous)? && !trace.start_solid {
            host.world
                .entities
                .get_mut(slot)
                .ok_or_else(missing_entity)?
                .add_event(0xbb, (trace.surface_flags >> 20) & 31, host.time_ms);
        }
        return Ok(());
    }
    let direct_hit = target
        && trace.entity != owner
        && host.world.players.get(owner).is_some_and(|shooter| {
            host.world.players.get(trace.entity).is_some_and(|victim| {
                victim
                    .state
                    .field("pm_type")
                    .expect("registered playerstate field")
                    < 6
                    && (shooter.team() == 0 || victim.team() != shooter.team())
            })
        });
    if target && direct != 0 {
        let current = host.world.entities.get(slot).ok_or_else(missing_entity)?;
        let mut dir = vector(&current.state, "pos.trDelta");
        if field(&current.state, "pos.trType") == 5 {
            let elapsed = host
                .time_ms
                .wrapping_sub(field(&current.state, "pos.trTime") as i32)
                as f32
                * 0.001;
            dir[2] = (f64::from(dir[2]) - 800. * f64::from(elapsed)) as f32;
        }
        if length(dir) == 0. {
            dir[2] = 1.;
        }
        damage::Hit {
            victim: trace.entity,
            inflictor: Some(slot),
            attacker: Some(owner),
            damage: direct,
            flags: 0,
            means: if rocket { 5 } else { 3 },
            weapon,
            point: Some(origin),
            direction: Some(dir),
            location: 0,
            offset: 0,
        }
        .damage(vm, program, host)?;
    }
    if direct != 0 {
        damage::shot::trigger_segment(
            vm,
            program,
            host,
            owner,
            origin,
            hit,
            direct,
            if rocket { 5 } else { 3 },
            false,
        )?;
    }
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    e.add_event(
        if direct_hit || trace.part_group != 0 {
            0xbe
        } else {
            0xbd
        },
        direction_index(trace.normal),
        host.time_ms,
    );
    e.free_after_event_at = Some(host.time_ms);
    e.projectile = None;
    e.state.set_field("eType", 0);
    e.state
        .set_field("eFlags", (field(&e.state, "eFlags") ^ 2) | 0x20);
    e.flags |= 0x800;
    let base = vector(&e.state, "pos.trBase");
    let center = std::array::from_fn(|i| {
        if base[i] <= hit[i] {
            hit[i].floor()
        } else {
            hit[i].ceil()
        }
    });
    stationary(e, center, 0);
    splash(vm, program, host, slot, Some(trace.entity))?;
    if let Some(e) = host.world.entities.get_mut(slot) {
        relink(e, host.collision, false)?;
    }
    Ok(())
}

pub(super) fn think(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
) -> io::Result<()> {
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    let def = host
        .weapons
        .get(field(&e.state, "weapon") as usize)
        .ok_or_else(missing_weapon)?;
    if def.offhand_class == 2 && field(&e.state, "groundEntityNum") == 1023 {
        e.projectile.as_mut().ok_or_else(missing_entity)?.next_think = 50;
        return Ok(());
    }
    let center = position(&e.state, host.time_ms).map(f32::trunc);
    stationary(e, center, 0);
    let mut down = center;
    down[2] -= 16.;
    let trace = host.collision.box_trace(
        &host.world.entities,
        center,
        down,
        [0.; 3],
        [0.; 3],
        slot,
        0x811,
    );
    let submerged = water(host, center);
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    e.state.set_field("eType", 0);
    e.state.set_field("eFlags", field(&e.state, "eFlags") | 32);
    e.flags |= 0x800;
    e.sv_flags |= 8;
    e.projectile = None;
    e.add_event(
        if def.projectile_explosion == 2 {
            0xbf
        } else {
            0xbc
        },
        direction_index(trace.normal),
        host.time_ms,
    );
    e.state.set_field(
        "surfType",
        if submerged {
            20
        } else {
            (trace.surface_flags >> 20) & 31
        },
    );
    if def.projectile.effect.is_empty() {
        e.free_after_event_at = Some(host.time_ms);
    } else {
        let bytes = host.assets.read_file(&def.projectile.effect)?;
        let expiry = host
            .time_ms
            .wrapping_add((1. + effect_duration(&bytes)) as i32);
        e.state
            .set_field("eFlags", field(&e.state, "eFlags") | 0x10000);
        e.state.set_field("time", host.time_ms as u32);
        e.state.set_field("time2", expiry as u32);
    }
    splash(vm, program, host, slot, None)?;
    if let Some(e) = host.world.entities.get_mut(slot) {
        relink(e, host.collision, false)?;
    }
    Ok(())
}

/// FxEffectTemplate_GetDuration takes the maximum life and spawn delay across
/// primitives. Nested particle curve definitions do not affect this value.
fn effect_duration(bytes: &[u8]) -> f32 {
    let mut depth = 0;
    let mut best = 0_f32;
    let mut delay = 0_f32;
    let mut life = 0_f32;
    for line in bytes.split(|&b| b == b'\n') {
        let mut tokens = line
            .split(|b| b.is_ascii_whitespace())
            .filter(|s| !s.is_empty());
        let Some(first) = tokens.next() else { continue };
        match first {
            b"{" => {
                depth += 1;
                if depth == 1 {
                    delay = 0.;
                    life = 0.;
                }
            }
            b"}" => {
                if depth == 1 {
                    best = best.max(life + delay);
                }
                depth -= 1;
            }
            name if depth == 1
                && (name.eq_ignore_ascii_case(b"life") || name.eq_ignore_ascii_case(b"delay")) =>
            {
                let range = tokens
                    .map(damage::shot::number)
                    .reduce(f32::max)
                    .unwrap_or(0.);
                if name.eq_ignore_ascii_case(b"life") {
                    life = range
                } else {
                    delay = range;
                }
            }
            _ => {}
        }
    }
    best
}

fn splash(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
    ignore: Option<u16>,
) -> io::Result<()> {
    let entity = host.world.entities.get(slot).ok_or_else(missing_entity)?;
    let owner = entity.owner_num;
    if host.world.entities.object_for_slot(owner).is_none() {
        return Ok(());
    }
    let weapon = field(&entity.state, "weapon") as usize;
    let def = host.weapons.get(weapon).ok_or_else(missing_weapon)?;
    if def.projectile.inner == 0 {
        return Ok(());
    }
    super::radius::Blast {
        origin: entity.origin,
        radius: def.projectile.radius as f32,
        inner: def.projectile.inner as f32,
        outer: def.projectile.outer as f32,
        inflictor: Some(slot),
        attacker: owner,
        weapon,
        means: if def.weapon_type == 2 { 6 } else { 4 },
        ignore,
        ignore_players: false,
    }
    .apply(vm, program, host)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bounce_retains_original_single_precision_intermediate() {
        let reflected = bounce_velocity(
            [-851., 4., f32::from_bits(0xc448_c001)],
            [0., 0., 1.],
            [0.5, 0.25],
        );
        assert_eq!(
            reflected.map(f32::to_bits),
            [0xc38b_bede, 0x3fa8_278c, 0x4383_dd04]
        );
    }

    #[test]
    fn effect_lifetime_uses_longest_primitive_excluding_nested_curves() {
        let source=b"Emitter\n{\nlife 5 10\ndelay 4 8\ncurve\n{\nlife 900000\n}\n}\nTrail\n{\nlife 11\ndelay 20\n}\n";
        assert_eq!(effect_duration(source), 31.);
        assert_eq!(effect_duration(b""), 0.);
    }
}
