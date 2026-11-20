//! Server missile entities. A thrown item tumbles under TR_GRAVITY; a rocket
//! uses a linear trajectory and occasional course changes (chunks 049/050/053).
mod bindings;
mod impact;
mod radius;
use crate::{
    assets::weapons::WeaponDefinition,
    collision::TraceResult,
    combat::damage,
    entity_delta::EntityState,
    script::ScriptProgram,
    vm::{
        GameHost,
        effects::angles,
        entity_methods::relink,
        fiber::FiberVm,
        game_entities::MapEntity,
        items::motion::{position, set_vector, stationary, vector},
        math::axes,
        spawn_fields::parse_int,
    },
};
pub(crate) use bindings::builtin;
use std::io;
type Vec3 = [f32; 3];
const CLIP_MASK: u32 = 0x2802891;
pub(crate) const SURFACES: [&str; 23] = [
    "default", "bark", "brick", "carpet", "cloth", "concrete", "dirt", "flesh", "foliage", "glass",
    "grass", "gravel", "ice", "metal", "mud", "paper", "plaster", "rock", "sand", "snow", "water",
    "wood", "asphalt",
];

#[derive(Clone, Debug, Default)]
pub struct Parameters {
    speed: i32,
    upward_speed: i32,
    damage: i32,
    radius: i32,
    inner: i32,
    outer: i32,
    drift_distance: i32,
    drift_time: f32,
    drift_ratio: f32,
    drift_angle: f32,
    bounce: [[f32; 2]; 23],
    effect: String,
}
impl Parameters {
    pub(crate) fn parse<'a>(raw: impl Fn(&str) -> &'a [u8]) -> Self {
        let int = |n| parse_int(raw(n));
        let float = |n: &str| damage::shot::number(raw(n));
        Self {
            speed: int("projectilespeed"),
            upward_speed: int("projectilespeedup"),
            damage: int("damage"),
            radius: int("explosionradius"),
            inner: int("explosioninnerdamage"),
            outer: int("explosionouterdamage"),
            drift_distance: int("destabilizedistance"),
            drift_time: float("destabilizationbasetime"),
            drift_ratio: float("destabilizationtimereductionratio"),
            drift_angle: float("destabilizationanglemax"),
            bounce: SURFACES.map(|name| {
                ["parallel", "perpendicular"].map(|axis| float(&format!("{axis}{name}bounce")))
            }),
            effect: String::from_utf8_lossy(raw("projexplosioneffect")).into_owned(),
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub(crate) struct Missile {
    next_think: i32,
    drift_ms: f32,
}

fn missing_entity() -> io::Error {
    io::Error::other("projectile entity was released")
}
fn missing_weapon() -> io::Error {
    io::Error::other("projectile weapon is not registered")
}

fn field(state: &EntityState, name: &str) -> u32 {
    state.field(name).expect("registered entity field")
}
fn dot(a: Vec3, b: Vec3) -> f64 {
    (0..3).map(|i| f64::from(a[i]) * f64::from(b[i])).sum()
}
fn length(v: Vec3) -> f32 {
    (dot(v, v) as f32).sqrt()
}
fn normalize(v: Vec3) -> Vec3 {
    let len = length(v);
    if len == 0. {
        v
    } else {
        v.map(|x| x * (1. / len))
    }
}
fn add(a: Vec3, f: f32, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| (f64::from(a[i]) + f64::from(f) * f64::from(b[i])) as f32)
}
fn sub(a: Vec3, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| a[i] - b[i])
}
fn lerp(a: Vec3, b: Vec3, f: f32) -> Vec3 {
    std::array::from_fn(|i| {
        (f64::from(a[i]) + (f64::from(b[i]) - f64::from(a[i])) * f64::from(f)) as f32
    })
}
fn angle_mod(deg: f32) -> f32 {
    (((f64::from(deg) * f64::from(182.04445f32)) as i32) & 65535) as f32 * (360. / 65536.)
}
fn fixed_angles(e: &mut MapEntity, v: Vec3) {
    e.angles = v;
    for key in ["apos.trType", "apos.trTime", "apos.trDuration"] {
        e.state.set_field(key, 0);
    }
    set_vector(&mut e.state, "apos.trBase", v);
    set_vector(&mut e.state, "apos.trDelta", [0.; 3]);
}
pub(crate) fn offhand(host: &mut GameHost, slot: u16) -> io::Result<()> {
    let player = host
        .world
        .players
        .get(slot)
        .ok_or_else(|| io::Error::other("missing projectile owner"))?;
    let ps = &player.state;
    let angles = ["viewangles[0]", "viewangles[1]", "viewangles[2]"]
        .map(|k| f32::from_bits(ps.field(k).expect("registered playerstate field")));
    let eye = damage::shot::eye(host, ps, angles);
    let angles = [
        player.view.gun_angles[0],
        player.view.gun_angles[1],
        angles[2],
    ];
    launch(
        host,
        slot,
        ps.field("offHandIndex")
            .expect("registered playerstate field") as usize,
        eye,
        angles,
        0.,
    )
}
fn spawn_missile(
    host: &mut GameHost,
    owner: u16,
    weapon: usize,
    origin: Vec3,
    class: &[u8],
) -> io::Result<u16> {
    let (slot, _) = host.world.spawn_entity(class, origin, host.time_ms)?;
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    e.owner_num = owner;
    e.clip_mask = CLIP_MASK;
    e.sv_flags = 8;
    e.state.set_field("eType", 4);
    e.state.set_field("weapon", weapon as u32);
    e.state
        .set_field("time", host.time_ms.wrapping_add(50) as u32);
    Ok(slot)
}

fn spawn_grenade(
    host: &mut GameHost,
    owner: u16,
    weapon: usize,
    origin: Vec3,
    velocity: Vec3,
) -> io::Result<u16> {
    let slot = spawn_missile(host, owner, weapon, origin, b"grenade")?;
    let def = host
        .weapons
        .get(weapon)
        .ok_or_else(|| io::Error::other("missing grenade definition"))?;
    let player = host.world.players.get_mut(owner)?;
    let fuse = player
        .state
        .field("grenadeTimeLeft")
        .expect("registered playerstate field") as i32;
    player.state.set_field("grenadeTimeLeft", 0)?;
    let now = host.time_ms;
    let s = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    let mut orientation = angles(velocity);
    orientation[0] = angle_mod(orientation[0] - 120.);
    set_vector(&mut s.state, "pos.trDelta", velocity.map(f32::trunc));
    s.angles = orientation;
    set_vector(&mut s.state, "apos.trBase", orientation);
    set_vector(
        &mut s.state,
        "apos.trDelta",
        [
            720. + host.world.random.float(-45., 45.),
            0.,
            360. + host.world.random.float(-45., 45.),
        ],
    );
    s.state.set_field("eFlags", 0x1000000);
    s.state.set_field("pos.trType", 5);
    s.state.set_field("pos.trTime", now as u32);
    s.state.set_field("apos.trType", 2);
    s.state.set_field("apos.trTime", now as u32);
    s.projectile = Some(Missile {
        next_think: now.wrapping_add(if fuse == 0 {
            def.handling.fuse_time
        } else {
            fuse
        }),
        drift_ms: 0.,
    });

    Ok(slot)
}

/// player_die drops a cooked offhand with an independent random velocity.
pub(crate) fn drop_held(host: &mut GameHost, owner: u16) -> io::Result<()> {
    let p = host.world.players.get_mut(owner)?;
    if p.state.field("grenadeTimeLeft") == Some(0) {
        return Ok(());
    }
    let weapon = p
        .state
        .field("offHandIndex")
        .expect("registered playerstate field") as usize;
    let mut origin = host
        .world
        .entities
        .get(owner)
        .ok_or_else(missing_entity)?
        .origin;
    origin[2] += 40.;
    let rng = &mut host.world.native_random;
    let velocity = [rng.signed(), rng.signed(), rng.fraction()].map(|v| v as f32 * 160.);
    spawn_grenade(host, owner, weapon, origin, velocity)?;
    Ok(())
}

pub(crate) fn launch(
    host: &mut GameHost,
    owner: u16,
    weapon: usize,
    origin: Vec3,
    view_angles: Vec3,
    spread: f32,
) -> io::Result<()> {
    let def = host.weapons.get(weapon).ok_or_else(missing_weapon)?;
    if def.weapon_type == 2 {
        return launch_rocket(host, owner, weapon, origin, view_angles, spread);
    }
    let forward = axes(view_angles)[0];
    let mut velocity = forward.map(|n| n * def.projectile.speed as f32);
    velocity[2] += def.projectile.upward_speed as f32;
    let slot = spawn_grenade(host, owner, weapon, origin, velocity)?;
    let player = host.world.players.get(owner).ok_or_else(missing_entity)?;
    let owner_velocity = ["velocity[0]", "velocity[1]", "velocity[2]"]
        .map(|key| f32::from_bits(player.state.field(key).expect("registered velocity field")));
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    let dir = normalize(velocity);
    let velocity = add(
        vector(&e.state, "pos.trDelta"),
        dot(dir, owner_velocity) as f32,
        dir,
    );
    set_vector(&mut e.state, "pos.trDelta", velocity);
    Ok(())
}

/// A turret uses the same rocket launch path at zero spread. The server
/// consumes both random draws and applies recoil only to client-backed owners.
pub(crate) fn turret_rocket(
    host: &mut GameHost,
    turret: u16,
    weapon: usize,
    muzzle: Vec3,
    view: Vec3,
) -> io::Result<()> {
    launch_rocket(host, turret, weapon, muzzle, view, 0.)
}

fn launch_rocket(
    host: &mut GameHost,
    owner: u16,
    weapon: usize,
    origin: Vec3,
    view_angles: Vec3,
    spread: f32,
) -> io::Result<()> {
    let now = host.time_ms;
    let def = host.weapons.get(weapon).ok_or_else(missing_weapon)?;
    let speed = def.projectile.speed as f32;
    let drift_ms = def.projectile.drift_distance as f32 / speed * 1000.;
    let [forward, right, up] = axes(view_angles);
    let slot = spawn_missile(host, owner, weapon, origin, b"rocket")?;
    let owner_flags = host.world.entities.get(owner).map_or(0, |e| e.flags);
    let s = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(missing_entity)?;
    s.flags |= owner_flags & 0x20000;
    let radius = 16. * (f64::from(spread).to_radians().tan() as f32);
    let angle = f64::from((host.world.native_random.fraction() * 360.) as f32).to_radians() as f32;
    let distance = host.world.native_random.fraction() as f32;
    let (sin, cos) = f64::from(angle).sin_cos();
    let direction = normalize(normalize(add(
        add(
            forward.map(|n| n * 16.),
            radius * (distance * cos as f32),
            right,
        ),
        radius * (distance * sin as f32),
        up,
    )));
    set_vector(
        &mut s.state,
        "pos.trDelta",
        direction.map(|n| (n * speed).trunc()),
    );
    fixed_angles(s, angles(direction));
    s.state.set_field("pos.trType", 2);
    s.state.set_field("pos.trTime", now.wrapping_sub(50) as u32);
    s.state.set_field("eFlags", 0x400);
    s.projectile = Some(Missile {
        next_think: now.wrapping_add(30_000),
        drift_ms,
    });
    if host.world.players.get(owner).is_some() {
        let player = host.world.players.get_mut(owner)?;
        let fields = ["velocity[0]", "velocity[1]", "velocity[2]"];
        let prior = fields
            .map(|k| f32::from_bits(player.state.field(k).expect("registered velocity field")));
        for (name, v) in fields.into_iter().zip(add(prior, -64., forward)) {
            player.state.set_field(name, v.to_bits())?;
        }
    }
    Ok(())
}

fn point(
    host: &GameHost,
    start: Vec3,
    end: Vec3,
    owner: u16,
    mask: u32,
) -> io::Result<TraceResult> {
    let mut tr = host.collision.trace_point(
        &host.world.entities,
        host.models,
        start,
        end,
        owner,
        mask,
        false,
    )?;
    if tr.start_solid {
        tr.fraction = 0.;
        tr.normal = normalize(sub(start, end));
    }
    Ok(tr)
}
fn water(host: &GameHost, pos: Vec3) -> bool {
    host.collision
        .point_contents(&host.world.entities, pos, 1023, 32)
        != 0
}
fn advance_rocket(
    ent: &mut MapEntity,
    def: &WeaponDefinition,
    now: i32,
    random: &mut crate::vm::math::GameRandom,
) {
    let missile = ent.projectile.as_mut().expect("called for live rocket");
    if (missile.drift_ms.round_ties_even() as i32)
        .wrapping_add(field(&ent.state, "pos.trTime") as i32)
        >= now
    {
        return;
    }
    let slope = f64::from(f64::from(def.projectile.drift_angle).to_radians() as f32).tan() as f32;
    let previous = normalize(vector(&ent.state, "pos.trDelta"));
    let jitter = std::array::from_fn(|_| random.float(-1., 1.) * slope);
    let dir = normalize(add(previous, 1., jitter));
    let flags = field(&ent.state, "eFlags");
    missile.drift_ms = if flags & 0x10000 == 0 {
        def.projectile.drift_time * 1000.
    } else {
        missile.drift_ms * def.projectile.drift_ratio
    };
    set_vector(
        &mut ent.state,
        "pos.trDelta",
        dir.map(|v| v * def.projectile.speed as f32),
    );
    set_vector(&mut ent.state, "pos.trBase", ent.origin);
    ent.state.set_field("pos.trTime", now as u32);
    ent.state.set_field("eFlags", flags | 0x10000);
    fixed_angles(ent, angles(dir));
}

pub(crate) fn run_frame(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    previous: i32,
) -> io::Result<()> {
    let slots: Vec<_> = host
        .world
        .entities
        .iter()
        .filter_map(|(slot, e)| e.projectile.map(|_| slot))
        .collect();
    for slot in slots {
        if host
            .world
            .entities
            .get(slot)
            .is_some_and(|e| e.projectile.is_some())
        {
            run_missile(vm, program, host, slot, previous)?;
        }
    }
    Ok(())
}
fn run_missile(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
    previous: i32,
) -> io::Result<()> {
    let now = host.time_ms;
    let e = host.world.entities.get(slot).ok_or_else(missing_entity)?;
    let owner = e.owner_num;
    let mask = e.clip_mask;
    let from = e.origin;
    let mut down = from;
    down[2] -= 1.5;
    if field(&e.state, "pos.trType") == 0
        && field(&e.state, "groundEntityNum") != 1022
        && point(host, from, down, owner, mask)?.fraction == 1.
    {
        let e = host
            .world
            .entities
            .get_mut(slot)
            .ok_or_else(missing_entity)?;
        stationary(e, from, 5);
        e.state.set_field("pos.trTime", now as u32);
    }
    let e = host.world.entities.get(slot).ok_or_else(missing_entity)?;
    let target = position(&e.state, now);
    if length(sub(target, from)) >= 0.001 {
        let include_water = vector(&e.state, "pos.trDelta")[2].abs() > 30. && !water(host, from);
        let mut tr = point(
            host,
            from,
            target,
            owner,
            if include_water { mask | 32 } else { mask },
        )?;
        if tr.surface_flags & 0x1f00000 == 0x1400000 {
            let mut dir = normalize(vector(&e.state, "pos.trDelta"));
            dir[2] = dir[2].abs();
            let event = damage::temp(host, from, 0xb6)?;
            event
                .state
                .set_field("eventParm", crate::vm::effects::direction_index(tr.normal));
            event
                .state
                .set_field("scale", crate::vm::effects::direction_index(dir));
            event.state.set_field("surfType", 20);
            event.state.set_field("otherEntityNum", u32::from(slot));
            tr = point(host, from, target, owner, mask)?;
        }
        let weapon = field(
            &host
                .world
                .entities
                .get(slot)
                .ok_or_else(missing_entity)?
                .state,
            "weapon",
        );
        let def = host
            .weapons
            .get(weapon as usize)
            .ok_or_else(missing_weapon)?;
        let grenade = def.weapon_type == 1;
        let inner_damage = def.projectile.inner;
        if grenade
            && let Some(obstacle) = host.world.entities.get_mut(tr.entity)
            && obstacle.flags & 0x8000 != 0
        {
            let obstacle_slot = tr.entity;
            let contents = std::mem::replace(&mut obstacle.contents, 0);
            let retrace = point(host, from, target, owner, mask);
            host.world
                .entities
                .get_mut(obstacle_slot)
                .ok_or_else(missing_entity)?
                .contents = contents;
            tr = retrace?;
        }
        let mut hit = lerp(from, target, tr.fraction);
        let e = host.world.entities.get(slot).ok_or_else(missing_entity)?;
        let mut adjusted = hit;
        if field(&e.state, "eFlags") & 0x1000000 != 0 && (tr.fraction == 1. || tr.normal[2] > 0.7) {
            let mut below = hit;
            below[2] -= 1.5;
            let ground = point(host, hit, below, owner, mask)?;
            if ground.fraction < 1. && ground.entity == 1022 {
                tr = ground;
                hit = lerp(hit, below, tr.fraction);
                let mut base = vector(&e.state, "pos.trBase");
                base[2] += hit[2] + 1.5 - adjusted[2];
                adjusted = hit;
                adjusted[2] += 1.5;
                set_vector(
                    &mut host
                        .world
                        .entities
                        .get_mut(slot)
                        .ok_or_else(missing_entity)?
                        .state,
                    "pos.trBase",
                    base,
                );
            }
        }
        let e = host
            .world
            .entities
            .get_mut(slot)
            .ok_or_else(missing_entity)?;
        e.origin = adjusted;
        relink(e, host.collision, false)?;
        if grenade {
            damage::shot::trigger_segment(
                vm,
                program,
                host,
                slot,
                from,
                adjusted,
                inner_damage,
                3,
                true,
            )?;
        }
        let e = host
            .world
            .entities
            .get_mut(slot)
            .ok_or_else(missing_entity)?;
        if tr.fraction < 1. {
            if tr.surface_flags & 16 != 0 {
                return host.world.free_entity(slot, now);
            }
            impact::contact(vm, program, host, slot, tr, hit, previous)?;
            if host
                .world
                .entities
                .get(slot)
                .is_none_or(|e| e.projectile.is_none())
            {
                return Ok(());
            }
        } else if length(vector(&e.state, "pos.trDelta")) != 0. {
            e.state.set_field("groundEntityNum", 1023);
            let def = host
                .weapons
                .get(field(&e.state, "weapon") as usize)
                .ok_or_else(missing_weapon)?;
            if def.weapon_type == 2 && e.flags & 0x20000 == 0 {
                advance_rocket(e, def, now, &mut host.world.random);
            }
        }
    }
    let e = host.world.entities.get(slot).ok_or_else(missing_entity)?;
    if e.projectile
        .is_some_and(|m| m.next_think > 0 && m.next_think <= now)
    {
        impact::think(vm, program, host, slot)?;
    }
    Ok(())
}
