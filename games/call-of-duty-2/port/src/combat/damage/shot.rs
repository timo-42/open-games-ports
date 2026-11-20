//! G_FireWeaponShot and Bullet_Fire_Extended for multiplayer hitscan weapons.
use super::*;
use crate::{
    collision::TraceResult,
    vm::{
        math::axes,
        spawn_fields::{float_prefix, parse_int},
    },
};

#[derive(Debug, Clone, Default)]
pub struct ShotParameters {
    pub multipliers: [f32; 19],
    pub(crate) rifle: bool,
    armor_piercing: bool,
    melee_damage: i32,
    pellets: i32,
    pub(crate) near_damage: i32,
    far_damage: i32,
    near_range: f32,
    far_range: f32,
    hip_min: [f32; 3],
    hip_max: [f32; 3],
    ads: f32,
}
pub(crate) fn number(bytes: &[u8]) -> f32 {
    float_prefix(bytes)
        .and_then(|(n, _)| n.parse::<f64>().ok())
        .unwrap_or(0.) as f32
}
impl ShotParameters {
    pub(crate) fn parse<'a>(raw: impl Fn(&str) -> &'a [u8]) -> Self {
        let range = |v: &[u8], fallback: u32| {
            let r = number(v);
            if r <= 0. { f32::from_bits(fallback) } else { r }
        };
        let values = |keys: [&str; 3]| keys.map(|k| number(raw(k)));
        Self {
            multipliers: LOCATIONS.map(|loc| number(raw(&format!("loc{}", loc.replace('_', ""))))),
            rifle: parse_int(raw("riflebullet")) != 0,
            armor_piercing: parse_int(raw("armorpiercing")) != 0,
            melee_damage: parse_int(raw("meleedamage")),
            pellets: parse_int(raw("shotcount")),
            near_damage: parse_int(raw("damage")),
            far_damage: parse_int(raw("mindamage")),
            near_range: range(raw("maxdamagerange"), 0x497423f0),
            far_range: range(raw("mindamagerange"), 0x497423f2),
            hip_min: values([
                "hipspreadpronemin",
                "hipspreadduckedmin",
                "hipspreadstandmin",
            ]),
            hip_max: values(["hipspreadpronemax", "hipspreadduckedmax", "hipspreadmax"]),
            ads: number(raw("adsspread")),
        }
    }
    pub fn distance_damage(&self, distance: f32) -> i32 {
        if distance < self.near_range {
            return self.near_damage;
        }
        if distance >= self.far_range {
            return self.far_damage;
        }
        let t = (distance - self.near_range) / (self.far_range - self.near_range);
        (f64::from(self.far_damage) * f64::from(t)
            + (1. - f64::from(t)) * f64::from(self.near_damage)) as i32
    }
    fn spread(&self, ps: &PlayerState, fraction: f32) -> f32 {
        let height = float(ps, "viewHeightCurrent");
        let (lo, t) = if height > 40. {
            (1, (height - 40.) / 20.)
        } else {
            (0, (height - 11.) / 29.)
        };
        let interp = |a: [f32; 3]| a[lo] + (a[lo + 1] - a[lo]) * t;
        let min = if float(ps, "fWeaponPosFrac") == 1. {
            self.ads
        } else {
            interp(self.hip_min)
        };
        min + (interp(self.hip_max) - min) * fraction
    }
}
fn add(a: Vec3, scale: f32, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| (f64::from(a[i]) + f64::from(scale) * f64::from(b[i])) as f32)
}
fn point_at_fraction(start: Vec3, end: Vec3, fraction: f32) -> Vec3 {
    std::array::from_fn(|i| {
        (f64::from(start[i]) + (f64::from(end[i]) - f64::from(start[i])) * f64::from(fraction))
            as f32
    })
}
fn sub(a: Vec3, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| a[i] - b[i])
}
fn dot(a: Vec3, b: Vec3) -> f32 {
    (0..3)
        .map(|i| f64::from(a[i]) * f64::from(b[i]))
        .sum::<f64>() as f32
}
pub(crate) fn eye(host: &GameHost, ps: &PlayerState, angles: Vec3) -> Vec3 {
    let mut origin = ["origin[0]", "origin[1]", "origin[2]"].map(|key| float(ps, key));
    let bottom = origin[2] + 8.;
    origin[2] += float(ps, "viewHeightCurrent");
    let speed = view::move_speed(ps, host.time_ms);
    let scale = view::bob_scale(ps, host.config);
    let amplitude = (speed * scale).min(dvar(host, "bg_bobMax", 8.));
    let phase = ((1.
        + f64::from(ps.field("bobCycle").expect("registered network field") as u8) / 255.)
        * std::f64::consts::TAU) as f32;
    origin[2] += ((phase + phase).sin() + (4. * phase + std::f32::consts::FRAC_PI_2).sin() * 0.2)
        * 0.75
        * amplitude;
    origin = add(origin, phase.sin() * amplitude, axes(angles)[1]);
    let lean = float(ps, "leanf");
    let eased = (2. - lean.abs()) * lean;
    origin = add(origin, eased * 20., axes([0., angles[1], eased * 16.])[1]);
    origin[2] = origin[2].max(bottom);
    origin
}

fn using_turret(host: &GameHost, ps: &PlayerState, slot: u16) -> bool {
    ps.words()[0xa0 / 4] & 0x300 != 0
        && host
            .world
            .entities
            .get(ps.words()[0x594 / 4] as u16)
            .is_some_and(|turret| turret.owner_num == slot)
}

pub(super) fn fire(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
) -> io::Result<()> {
    let p = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing firing player"))?;
    let ps = &p.state;
    if using_turret(host, ps, slot) {
        return Ok(());
    }
    let weapon = ps.field("weapon").expect("registered network field") as usize;
    let angles = ["viewangles[0]", "viewangles[1]", "viewangles[2]"].map(|key| float(ps, key));
    let origin = eye(host, ps, angles);
    let def = host
        .weapons
        .get(weapon)
        .ok_or_else(|| invalid("unknown weapon"))?;
    let angles = [p.view.gun_angles[0], p.view.gun_angles[1], angles[2]];
    let spread = def.shot.spread(ps, p.view.spread_fraction);
    if def.weapon_type != 0 {
        return crate::combat::projectile::launch(host, slot, weapon, origin, angles, spread);
    }
    let offset = if host.config.get("g_antilag") == Some("0") {
        0
    } else {
        host.time_ms.wrapping_sub(p.command.server_time)
    };
    fire_shot(
        vm,
        program,
        host,
        Shot {
            owner: slot,
            shooter: slot,
            weapon,
            origin,
            angles,
            spread,
            offset,
        },
    )
}
pub fn turret_shot(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    player: u16,
    turret: u16,
    muzzle: Vec3,
    spread: f32,
) -> io::Result<()> {
    let ps = &host
        .world
        .players
        .get(player)
        .ok_or_else(|| invalid("missing turret operator"))?
        .state;
    let angles = ["viewangles[0]", "viewangles[1]", "viewangles[2]"].map(|key| float(ps, key));
    let ent = host
        .world
        .entities
        .get_mut(turret)
        .ok_or_else(|| invalid("missing turret"))?;
    let weapon = ent.state.field("weapon").expect("registered network field") as usize;
    fire_shot(
        vm,
        program,
        host,
        Shot {
            owner: player,
            shooter: turret,
            weapon,
            origin: muzzle,
            angles,
            spread,
            offset: 0,
        },
    )
}
struct Shot {
    owner: u16,
    shooter: u16,
    weapon: usize,
    origin: Vec3,
    angles: Vec3,
    spread: f32,
    offset: i32,
}
fn fire_shot(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    frame: Shot,
) -> io::Result<()> {
    let Shot {
        owner: slot,
        shooter,
        weapon,
        origin,
        angles,
        spread,
        offset,
    } = frame;
    let def = host
        .weapons
        .get(weapon)
        .ok_or_else(|| invalid("unknown weapon"))?;
    let shot = def.shot.clone();
    let (count, range) = if def.weapon_class == 3 {
        (shot.pellets, shot.far_range)
    } else {
        (1, 8192.)
    };
    let [forward, right, up] = axes(angles);
    let spread = (f64::from(spread).to_radians().tan() as f32) * range;
    let fps = host
        .config
        .get("sv_fps")
        .and_then(|v| v.parse::<i32>().ok())
        .unwrap_or(20)
        .clamp(1, 1000);
    let mut original_positions = Vec::new();
    let result = (|| {
        if offset > 1000 / fps {
            for (other, client) in host
                .world
                .players
                .iter()
                .filter(|(_, p)| p.connected && p.session_state == 0)
            {
                if let Some(origin) = host.world.archive.interpolated_origin(
                    other as u8,
                    host.time_ms.wrapping_sub(offset),
                    host.time_ms,
                    fps,
                    Some(&client.state),
                ) && let Some(entity) = host.world.entities.get_mut(other)
                {
                    original_positions.push((other, entity.origin));
                    entity.origin = origin;
                    relink(entity, host.collision, entity.model_index != 0)?;
                }
            }
        }
        for _ in 0..count {
            let angle = (360. * host.world.native_random.fraction()) as f32;
            let angle = f64::from(angle).to_radians() as f32;
            let radius = host.world.native_random.fraction() as f32;
            let end = add(
                add(
                    add(origin, range, forward),
                    radius * angle.cos() * spread,
                    right,
                ),
                radius * angle.sin() * spread,
                up,
            );
            let mut start = origin;
            let mut skip = shooter;
            let mut power = 1.;
            for _ in 0..13 {
                let impact = trace(host, start, end, skip, shot.rifle)?;
                let point = point_at_fraction(start, end, impact.fraction);
                trigger_segment(
                    vm,
                    program,
                    host,
                    slot,
                    start,
                    point,
                    shot.near_damage,
                    if shot.rifle { 2 } else { 1 },
                    false,
                )?;
                if impact.fraction >= 1. {
                    break;
                }
                let ray = normal(sub(end, start));
                let projection = dot(ray, impact.normal);
                let reflection = add(ray, -2. * projection, impact.normal);
                let player_hit = host.world.players.get(impact.entity).is_some();
                if impact.surface_flags & 4 == 0 && !player_hit {
                    let corpse = host
                        .world
                        .entities
                        .get(impact.entity)
                        .is_some_and(|e| e.state.field("eType") == Some(2));
                    let surface = if corpse {
                        7
                    } else {
                        (impact.surface_flags >> 20) & 31
                    };
                    let event = temp(host, point, 0xb6 + u32::from(shot.rifle))?;
                    event
                        .state
                        .set_field("eventParm", direction_index(impact.normal));
                    event.state.set_field("scale", direction_index(reflection));
                    event.state.set_field("surfType", surface);
                    event.state.set_field("otherEntityNum", u32::from(shooter));
                }
                if impact.contents & 0x10 != 0 {
                    let epsilon = if -projection >= 0.125 {
                        0.25 / -projection
                    } else {
                        0.
                    };
                    start = add(point, epsilon, ray);
                    continue;
                }
                if impact.entity == slot
                    || !host
                        .world
                        .entities
                        .get(impact.entity)
                        .is_some_and(|e| e.take_damage)
                {
                    break;
                }
                let delta = sub(start, point);
                let damage = (shot.distance_damage(dot(delta, delta).sqrt()) as f32 * power) as i32;
                let flags =
                    (if shot.rifle { 32 } else { 0 }) | (if shot.armor_piercing { 2 } else { 0 });
                Hit {
                    victim: impact.entity,
                    inflictor: Some(slot),
                    attacker: Some(slot),
                    damage,
                    flags,
                    means: if shot.rifle { 2 } else { 1 },
                    weapon,
                    point: Some(point),
                    direction: Some(forward),
                    location: usize::from(impact.part_group),
                    offset,
                }
                .damage(vm, program, host)?;
                let team = host.world.players.get(slot).map_or(0, |p| p.team());
                if !player_hit
                    || !shot.rifle
                    || (team != 0
                        && host
                            .world
                            .players
                            .get(impact.entity)
                            .is_some_and(|p| p.team() == team)
                        && host.config.get("scr_friendlyfire").unwrap_or("0") == "0")
                {
                    break;
                }
                start = point;
                skip = impact.entity;
                power *= 0.5;
            }
        }
        Ok(())
    })();
    for (other, previous) in original_positions {
        if let Some(entity) = host.world.entities.get_mut(other) {
            entity.origin = previous;
            relink(entity, host.collision, entity.model_index != 0)?;
        }
    }
    result
}
fn trace(
    host: &GameHost,
    start: Vec3,
    end: Vec3,
    skip: u16,
    rifle: bool,
) -> io::Result<TraceResult> {
    host.collision.trace_point(
        &host.world.entities,
        host.models,
        start,
        end,
        skip,
        0x2802831,
        rifle,
    )
}

// G_UpdateWeaponTargetLock and Bullet_FireFromEntity. Melee samples up to
// five segments in the same view plane as the ranged trace.
pub(super) fn melee(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
) -> io::Result<()> {
    let player = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| invalid("missing player"))?;
    let ps = &player.state;
    if using_turret(host, ps, slot) {
        return Ok(());
    }
    let angles = ["viewangles[0]", "viewangles[1]", "viewangles[2]"].map(|key| float(ps, key));
    let [forward, right, up] = axes(angles);
    let origin = eye(host, ps, angles);
    let weapon = ps.field("weapon").expect("registered network field") as usize;
    let damage = host
        .weapons
        .get(weapon)
        .ok_or_else(|| invalid("missing melee weapon"))?
        .shot
        .melee_damage;
    let reach = dvar(host, "player_meleeRange", 64.);
    let width = dvar(host, "player_meleeWidth", 10.);
    let height = dvar(host, "player_meleeHeight", 10.);
    let offsets = [[0., 0.], [1., 1.], [1., -1.], [-1., 1.], [-1., -1.]];
    let count = if width > 0. || height > 0. { 5 } else { 1 };
    for (sample, [x, y]) in offsets[..count].iter().enumerate() {
        let end = add(
            add(add(origin, reach, forward), width * x, right),
            height * y,
            up,
        );
        let result = trace(host, origin, end, slot, false)?;
        let point = point_at_fraction(origin, end, result.fraction);
        if sample == 0 {
            trigger_segment(vm, program, host, slot, origin, point, damage, 7, false)?;
        }
        if result.surface_flags & 0x10 != 0 || result.fraction == 1. {
            continue;
        }
        let player_hit = host.world.players.get(result.entity).is_some();
        let event = temp(host, point, if player_hit { 0xad } else { 0xae })?;
        event
            .state
            .set_field("otherEntityNum", u32::from(result.entity));
        event
            .state
            .set_field("eventParm", direction_index(result.normal));
        event.state.set_field("weapon", weapon as u32);
        if host
            .world
            .entities
            .get(result.entity)
            .is_some_and(|e| e.take_damage)
        {
            let extra = (host.world.native_random.draw() % 5) as i32;
            Hit {
                victim: result.entity,
                attacker: Some(slot),
                inflictor: Some(slot),
                damage: damage + extra,
                flags: 0,
                means: 7,
                weapon,
                point: Some(point),
                direction: Some(forward),
                location: usize::from(result.part_group),
                offset: 0,
            }
            .damage(vm, program, host)?;
        }
        break;
    }
    Ok(())
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn trigger_segment(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    attacker: u16,
    start: Vec3,
    end: Vec3,
    damage: i32,
    means: i32,
    flagged_only: bool,
) -> io::Result<()> {
    let lo = std::array::from_fn(|i| start[i].min(end[i]));
    let hi = std::array::from_fn(|i| start[i].max(end[i]));
    let candidates = crate::vm::game_entities::touch::area_entities(
        &host.world.entities,
        host.collision,
        lo,
        hi,
        0x400000,
    );
    let Some(other) = host.world.entities.object_for_slot(attacker) else {
        return Ok(());
    };
    for candidate in candidates {
        let Some(e) = host.world.entities.get(candidate) else {
            continue;
        };
        let object = e.object;
        if (flagged_only && e.flags & 0x4000 == 0)
            || host.world.field(object, "classname")? != text("trigger_damage")
        {
            continue;
        }
        if !host
            .collision
            .trace_entity(e, start, end, [0.; 3], [0.; 3], u32::MAX)
            .is_some_and(|tr| tr.fraction < 1. || tr.start_solid)
        {
            continue;
        }
        vm.notify(
            object,
            "damage",
            &[Value::Int(damage), Value::Object(other)],
        );
        damage_trigger(vm, program, host, candidate, other, damage, means)?;
    }
    Ok(())
}
pub(super) fn damage_trigger(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
    other: u32,
    damage: i32,
    means: i32,
) -> io::Result<()> {
    let Some(e) = host.world.entities.get(slot) else {
        return Ok(());
    };
    let object = e.object;
    let threshold = e.trigger_threshold;
    let accumulate = e.trigger_accumulate;
    let flags = match host.world.field(object, "spawnflags")? {
        Value::Int(n) => n,
        _ => 0,
    };
    let health = entity_health(host, object)?;
    let mask = match means {
        1 => 1,
        2 => 2,
        3 | 5 => 4 | 8,
        4 | 6 => 4 | 8 | 16,
        14 => 8,
        7 => 32,
        0 | 9..=13 => 256,
        _ => 0,
    };
    if (threshold < 1 || threshold <= damage)
        && (flags & mask) == 0
        && (accumulate == 0 || accumulate <= 32000 - health)
    {
        if means != -1 {
            host.world.entities.queue_trigger(object, other);
        }
        host.world.set_field(object, "health", Value::Int(32000))?;
        if flags & 0x200 != 0 {
            vm.invoke(
                program,
                host,
                "codescripts/delete",
                "main",
                object,
                Vec::new(),
            )?;
        }
    } else if accumulate == 0 && means != -1 {
        host.world.set_field(object, "health", Value::Int(32000))?;
    }
    Ok(())
}
