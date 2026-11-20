//! Server game events and the stock GSC damage/death callback contract.
pub(crate) mod shot;
#[cfg(test)]
mod tests;
pub mod view;
use crate::{
    script::ScriptProgram,
    snapshot::PlayerState,
    vm::{
        GameHost, effects::direction_index, entity_methods::relink, fiber::FiberVm,
        game_entities::MapEntity, value::Value,
    },
};
pub use shot::ShotParameters;
pub(crate) use shot::eye;
pub use shot::turret_shot;
use std::io;

type Vec3 = [f32; 3];
const CALLBACKS: &str = "maps/mp/gametypes/_callbacksetup";
const LOCATIONS: [&str; 19] = [
    "none",
    "helmet",
    "head",
    "neck",
    "torso_upper",
    "torso_lower",
    "right_arm_upper",
    "left_arm_upper",
    "right_arm_lower",
    "left_arm_lower",
    "right_hand",
    "left_hand",
    "right_leg_upper",
    "left_leg_upper",
    "right_leg_lower",
    "left_leg_lower",
    "right_foot",
    "left_foot",
    "gun",
];
const MOD: [&str; 15] = [
    "MOD_UNKNOWN",
    "MOD_PISTOL_BULLET",
    "MOD_RIFLE_BULLET",
    "MOD_GRENADE",
    "MOD_GRENADE_SPLASH",
    "MOD_PROJECTILE",
    "MOD_PROJECTILE_SPLASH",
    "MOD_MELEE",
    "MOD_HEAD_SHOT",
    "MOD_CRUSH",
    "MOD_TELEFRAG",
    "MOD_FALLING",
    "MOD_SUICIDE",
    "MOD_TRIGGER_HURT",
    "MOD_EXPLOSIVE",
];

fn invalid(text: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, text)
}
fn float(ps: &PlayerState, name: &str) -> f32 {
    f32::from_bits(ps.field(name).expect("registered player netfield"))
}
fn vecval(v: Option<Vec3>) -> Value {
    v.map_or(Value::Undefined, Value::Vector)
}
fn text(v: &str) -> Value {
    Value::String(v.as_bytes().to_vec())
}
fn slot_value(host: &GameHost, slot: Option<u16>) -> Value {
    slot.and_then(|s| host.world.entities.object_for_slot(s))
        .map_or(Value::Undefined, Value::Object)
}
fn slot_arg(host: &GameHost, arg: &Value) -> Option<u16> {
    if let Value::Object(object) = arg {
        host.world.entities.slot_for_object(*object)
    } else {
        None
    }
}
fn normal(v: Vec3) -> Vec3 {
    let len = (v.iter().map(|x| f64::from(*x).powi(2)).sum::<f64>() as f32).sqrt();
    if len != 0. {
        let inverse = len.recip();
        v.map(|x| x * inverse)
    } else {
        [0.; 3]
    }
}
fn yaw(v: Vec3) -> f32 {
    if v[0] == 0. && v[1] == 0. {
        0.
    } else {
        (f64::from(v[1]).atan2(f64::from(v[0])).to_degrees() as f32).rem_euclid(360.)
    }
}
fn dvar(host: &GameHost, key: &str, default: f32) -> f32 {
    host.config
        .get(key)
        .and_then(|s| s.parse().ok())
        .unwrap_or(default)
}
pub(crate) fn temp<'a>(
    host: &'a mut GameHost,
    point: Vec3,
    event: u32,
) -> io::Result<&'a mut MapEntity> {
    let (slot, _) = host.world.spawn_entity(
        b"tempEntity",
        point.map(|v| (v as i32) as f32),
        host.time_ms,
    )?;
    let ent = host.world.entities.get_mut(slot).expect("new event entity");
    ent.free_after_event_at = Some(host.time_ms);
    ent.state.set_field("eType", event + 10);
    relink(ent, host.collision, false)?;
    Ok(ent)
}
#[derive(Clone, Debug)]
pub(crate) struct Hit {
    pub(crate) victim: u16,
    pub(crate) inflictor: Option<u16>,
    pub(crate) attacker: Option<u16>,
    pub(crate) damage: i32,
    pub(crate) flags: i32,
    pub(crate) means: usize,
    pub(crate) weapon: usize,
    pub(crate) point: Option<Vec3>,
    pub(crate) direction: Option<Vec3>,
    pub(crate) location: usize,
    pub(crate) offset: i32,
}
impl Hit {
    fn damage_entity(
        &self,
        vm: &mut FiberVm,
        program: &ScriptProgram,
        host: &mut GameHost,
    ) -> io::Result<()> {
        let Some(entity) = host
            .world
            .entities
            .get(self.victim)
            .filter(|e| e.take_damage && e.flags & 1 == 0)
        else {
            return Ok(());
        };
        let object = entity.object;
        let flags = entity.flags;
        let health = entity_health(host, object)?;
        let mut damage = self.damage.max(1);
        if flags & 2 != 0 && health.saturating_sub(damage) < 1 {
            damage = health.wrapping_sub(1);
        }
        let health = health.wrapping_sub(damage);
        host.world.set_field(object, "health", Value::Int(health))?;
        let other = slot_value(host, Some(self.attacker.unwrap_or(1022)));
        vm.notify(object, "damage", &[Value::Int(damage), other.clone()]);
        if health < 1 {
            if health < -999 {
                host.world.set_field(object, "health", Value::Int(-999))?;
            }
            vm.notify(object, "death", std::slice::from_ref(&other));
        }
        if let Value::Object(attacker) = other
            && host.world.field(object, "classname")? == text("trigger_damage")
        {
            shot::damage_trigger(
                vm,
                program,
                host,
                self.victim,
                attacker,
                damage,
                self.means as i32,
            )?;
        }
        Ok(())
    }
    fn callback(&self, host: &GameHost, killed: Option<i32>) -> Vec<Value> {
        let mut args = vec![
            slot_value(host, self.inflictor),
            slot_value(host, self.attacker),
            Value::Int(self.damage),
        ];
        if killed.is_none() {
            args.push(Value::Int(self.flags));
        }
        args.extend([
            text(MOD.get(self.means).unwrap_or(&"badMOD")),
            text(
                host.weapons
                    .get(self.weapon)
                    .map_or("none", |def| &def.name),
            ),
        ]);
        if killed.is_none() {
            args.push(vecval(self.point));
        }
        args.extend([
            vecval(self.direction),
            text(LOCATIONS[self.location]),
            Value::Int(self.offset),
        ]);
        if let Some(duration) = killed {
            args.push(Value::Int(duration));
        }
        args
    }
    pub(crate) fn damage(
        mut self,
        vm: &mut FiberVm,
        program: &ScriptProgram,
        host: &mut GameHost,
    ) -> io::Result<()> {
        let Some(p) = host.world.players.get(self.victim) else {
            return self.damage_entity(vm, program, host);
        };
        if !p.connected
            || p.input.noclip
            || p.input.ufo
            || !host
                .world
                .entities
                .get(self.victim)
                .is_some_and(|e| e.take_damage)
        {
            return Ok(());
        }
        if self.location >= LOCATIONS.len() {
            return Err(invalid("invalid damage hit location"));
        }
        let object = p.object;
        self.weapon = self
            .inflictor
            .or(self.attacker)
            .and_then(|slot| host.world.entities.get(slot))
            .and_then(|e| e.state.field("weapon"))
            .unwrap_or(0) as usize;
        let mult = if let Some(def) = host
            .weapons
            .get(self.weapon)
            .filter(|d| self.weapon != 0 && d.weapon_type == 0)
        {
            def.shot.multipliers[self.location]
        } else {
            host.weapons.hit_location_multipliers[self.location]
        };
        self.damage = (self.damage as f32 * mult) as i32;
        if self.damage <= 0 {
            return Ok(());
        }
        vm.invoke(
            program,
            host,
            CALLBACKS,
            "codecallback_playerdamage",
            object,
            self.callback(host, None),
        )?;
        Ok(())
    }
}
pub fn use_trigger(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
    other: u32,
) -> io::Result<()> {
    let Some(trigger) = host.world.entities.get(slot) else {
        return Ok(());
    };
    shot::damage_trigger(
        vm,
        program,
        host,
        slot,
        other,
        trigger.trigger_accumulate.wrapping_add(1),
        -1,
    )
}
fn entity_health(host: &GameHost, object: u32) -> io::Result<i32> {
    Ok(match host.world.field(object, "health")? {
        Value::Int(n) => n,
        _ => 0,
    })
}

fn entity_method(
    host: &mut GameHost,
    slot: u16,
    object: u32,
    name: &str,
    args: &[Value],
) -> io::Result<Value> {
    let ent = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing entity"))?;
    match name {
        "viewkick" => {
            let maximum = ent.max_health;
            let p = host.world.players.get_mut(slot)?;
            let [Value::Int(force), Value::Vector(source)] = args else {
                return Err(invalid(
                    "USAGE: <player> viewkick <force 0-127> <source position>",
                ));
            };
            let amount = force.wrapping_mul(maximum).wrapping_add(50) / 100;
            if amount < 0 {
                return Err(invalid("viewkick: damage below zero"));
            }
            let self_damage = p.damage_feedback.amount > 0 && p.damage_feedback.direction.is_none();
            p.damage_feedback.amount = amount;
            let origin = ["origin[0]", "origin[1]", "origin[2]"].map(|key| float(&p.state, key));
            if !self_damage {
                p.damage_feedback.direction = Some(std::array::from_fn(|i| origin[i] - source[i]));
            }
        }
        "getnormalhealth" => {
            let health = entity_health(host, object)?;
            let fraction = match host.world.players.get(slot) {
                Some(p) if health != 0 => health as f32 / p.max_health as f32,
                _ => health as f32,
            };
            return Ok(Value::Float(fraction));
        }
        "setnormalhealth" => {
            let fraction = args
                .first()
                .ok_or_else(|| invalid("expected health fraction"))?
                .number()?
                .min(1.);
            let health = if let Some(p) = host.world.players.get(slot) {
                let scaled = (f64::from(fraction) * f64::from(p.max_health)) as f32;
                host.world
                    .players
                    .get_mut(slot)?
                    .commands
                    .push(br#"I "0""#.to_vec());
                (f64::from(scaled) + 0.5).floor() as i32
            } else if ent.max_health == 0 {
                fraction as i32
            } else {
                (f64::from(ent.max_health) * f64::from(fraction)) as i32
            };
            if health > 0 {
                host.world.set_entity_health(object, health)?;
            } else {
                host.config
                    .messages
                    .push("ERROR: Cannot setnormalhealth to 0 or below.".into());
            }
        }
        "enableaimassist" | "disableaimassist" => {
            if !ent.bmodel {
                return Err(invalid(
                    "Currently only supported on entities with brush models",
                ));
            }
            let flags = ent
                .state
                .field("eFlags")
                .expect("registered entity netfield");
            ent.state.set_field(
                "eFlags",
                if name == "enableaimassist" {
                    flags | 0x800
                } else {
                    flags & !0x800
                },
            );
        }
        _ => unreachable!(),
    }
    Ok(Value::Undefined)
}

pub fn builtin(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    receiver: Option<u32>,
    name: &str,
    args: &[Value],
) -> Option<io::Result<Value>> {
    if name == "obituary" && receiver.is_none() {
        return Some(obituary(host, args));
    }
    let object = receiver?;
    if matches!(
        name,
        "viewkick" | "getnormalhealth" | "setnormalhealth" | "enableaimassist" | "disableaimassist"
    ) {
        let slot = host.world.entities.slot_for_object(object)?;
        return Some(entity_method(host, slot, object, name, args));
    }
    let slot = host.world.players.slot_for_object(object)?;
    Some(
        match name {
            "finishplayerdamage" => finish(vm, program, host, slot, args),
            "suicide" => suicide(vm, program, host, slot),
            _ => return None,
        }
        .map(|()| Value::Undefined),
    )
}
fn obituary(host: &mut GameHost, args: &[Value]) -> io::Result<Value> {
    let [victim, attacker, weapon, means, ..] = args else {
        return Err(invalid(
            "obituary requires victim, attacker, weapon and means of death",
        ));
    };
    let victim = slot_arg(host, victim).ok_or_else(|| invalid("obituary requires entity"))?;
    let attacker = slot_arg(host, attacker).unwrap_or(1022);
    let weapon = host.weapons.resolve(
        host.assets,
        &String::from_utf8_lossy(&weapon.script_string()?),
        host.initializing,
    )?;
    let mode = means.script_string()?;
    let mode = MOD
        .iter()
        .position(|m| m.as_bytes().eq_ignore_ascii_case(&mode))
        .unwrap_or(0);
    let event = temp(host, [0.; 3], 0xc6)?;
    event.state.set_field("otherEntityNum", u32::from(victim));
    event
        .state
        .set_field("attackerEntityNum", u32::from(attacker));
    event.state.set_field(
        "eventParm",
        if matches!(mode, 7 | 8 | 9 | 11 | 12) {
            mode as u32 | 128
        } else {
            weapon as u32
        },
    );
    event.sv_flags = 8;
    Ok(Value::Undefined)
}
pub(crate) fn parse_locations(bytes: &[u8]) -> io::Result<[f32; 19]> {
    let data = bytes
        .strip_prefix(b"LOCDMGTABLE")
        .ok_or_else(|| invalid("bad hit location table header"))?;
    if data.len() > 8191 || data.iter().any(|b| matches!(b, b'"' | b';')) {
        return Err(invalid("invalid hit location table"));
    }
    let mut result = std::array::from_fn(|i| if i == 18 { 0. } else { 1. });
    let mut pairs = data
        .strip_prefix(b"\\")
        .unwrap_or(data)
        .split(|b| *b == b'\\');
    while let Some(key) = pairs.next().filter(|k| !k.is_empty()) {
        let val = pairs
            .next()
            .ok_or_else(|| invalid("missing hit location multiplier"))?;
        let index = LOCATIONS
            .iter()
            .position(|s| s.as_bytes().eq_ignore_ascii_case(key))
            .ok_or_else(|| invalid("unknown hit location"))?;
        result[index] = shot::number(val);
    }
    Ok(result)
}
fn finish(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
    args: &[Value],
) -> io::Result<()> {
    let [
        inflictor,
        attacker,
        Value::Int(damage),
        Value::Int(flags),
        mode,
        weapon,
        point,
        dir,
        location,
        Value::Int(offset),
        ..,
    ] = args
    else {
        return Err(invalid("invalid finishPlayerDamage parameters"));
    };
    if *damage <= 0 {
        return Ok(());
    }
    let vector = |v: &Value| match v {
        Value::Undefined => Ok(None),
        Value::Vector(x) => Ok(Some(*x)),
        _ => Err(invalid("expected vector")),
    };
    let attacker = slot_arg(host, attacker).unwrap_or(1022);
    // 0x080fb170 deliberately reads argument 1 for both entity slots when the inflictor is defined.
    let inflictor = if slot_arg(host, inflictor).is_some() {
        attacker
    } else {
        1022
    };
    let mode = mode.script_string()?;
    let loc = location.script_string()?;
    let mut hit = Hit {
        victim: slot,
        attacker: Some(attacker),
        inflictor: Some(inflictor),
        damage: *damage,
        flags: *flags,
        means: MOD
            .iter()
            .position(|n| n.as_bytes().eq_ignore_ascii_case(&mode))
            .unwrap_or(0),
        weapon: host.weapons.resolve(
            host.assets,
            &String::from_utf8_lossy(&weapon.script_string()?),
            false,
        )?,
        point: vector(point)?,
        direction: vector(dir)?,
        location: LOCATIONS
            .iter()
            .position(|n| n.as_bytes() == loc)
            .unwrap_or(0),
        offset: *offset,
    };
    let direction = normal(hit.direction.unwrap_or([0.; 3]));
    let entity = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing damage target entity"))?;
    let (object, flags) = (entity.object, entity.flags);
    let previous_health = entity_health(host, object)?;
    let knockback = dvar(host, "g_knockback", 1000.) / 250.;
    let tick_scale = dvar(host, "player_dmgtimer_timePerPoint", 100.);
    let max_time = dvar(host, "player_dmgtimer_maxTime", 750.);
    let p = host.world.players.get_mut(slot)?;
    let state = &mut p.state;
    let pm_flags = state.field("pm_flags").expect("registered network field");
    if flags & 8 == 0
        && hit.flags & 4 == 0
        && state.field("eFlags").expect("registered network field") & 0x300 == 0
    {
        let scale = if pm_flags & 1 != 0 {
            0.02
        } else if pm_flags & 2 != 0 {
            0.15
        } else {
            0.3
        };
        let kick = ((hit.damage as f32 * scale) as i32).min(60);
        if kick != 0 {
            for (key, d) in ["velocity[0]", "velocity[1]", "velocity[2]"]
                .into_iter()
                .zip(direction)
            {
                state.set_field(
                    key,
                    (float(state, key) + d * (kick as f32 * knockback)).to_bits(),
                )?;
            }
            if state.field("pm_time") == Some(0) {
                state.set_field("pm_time", (kick * 2).clamp(50, 200) as u32)?;
                state.set_field("pm_flags", pm_flags | 0x400)?;
            }
        }
    }
    if flags & 1 != 0 {
        return Ok(());
    }
    p.damage_feedback.amount = p.damage_feedback.amount.saturating_add(hit.damage);
    p.damage_feedback.direction = hit.direction.map(normal);
    if flags & 2 != 0 && previous_health.saturating_sub(hit.damage) < 1 {
        hit.damage = previous_health - 1;
    }
    let timer = ((state
        .field("damageTimer")
        .expect("registered network field") as i32)
        .saturating_add((hit.damage as f32 * tick_scale) as i32))
    .min(max_time as i32);
    state.set_field("damageTimer", timer as u32)?;
    state.set_field("damageDuration", timer as u32)?;
    let view_yaw = float(state, "viewangles[1]");
    state.set_field(
        "flinchYaw",
        (yaw(direction) as i32
            - if view_yaw < 0. {
                360. + view_yaw
            } else {
                view_yaw
            } as i32) as u32,
    )?;
    let health = previous_health.saturating_sub(hit.damage).max(-999);
    host.world.set_entity_health(object, health)?;
    if let Some(def) = host
        .weapons
        .get(hit.weapon)
        .filter(|w| hit.weapon != 0 && w.weapon_type == 0)
        && let Some(point) = hit.point
    {
        let add = u32::from(def.shot.rifle);
        let impact = temp(host, point, 0xb6 + add)?;
        impact
            .state
            .set_field("eventParm", direction_index(direction));
        impact.state.set_field("scale", direction_index(direction));
        impact.state.set_field("surfType", 7);
        impact
            .state
            .set_field("otherEntityNum", u32::from(attacker));
        impact.client_mask[usize::from(slot) / 32] |= 1 << (slot & 31);
        let impact = temp(host, point, 0xb9 + add)?;
        impact.state.set_field("surfType", 7);
        impact
            .state
            .set_field("otherEntityNum", u32::from(attacker));
        impact.state.set_field("clientNum", u32::from(slot));
        impact.client_mask = [u32::MAX; 2];
        impact.client_mask[usize::from(slot) / 32] &= !(1 << (slot & 31));
    }
    vm.notify(
        object,
        "damage",
        &[Value::Int(hit.damage), slot_value(host, Some(attacker))],
    );
    if health <= 0 {
        hit.direction = Some(direction);
        die(vm, program, host, &hit)?;
    }
    let health = entity_health(host, object)?;
    host.world.players.get_mut(slot)?.state.words_mut()[0x12c / 4] = health as u32;
    Ok(())
}
pub fn suicide(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
) -> io::Result<()> {
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing player"))?;
    e.flags &= !3;
    let object = e.object;
    host.world.set_field(object, "health", Value::Int(0))?;
    host.world.players.get_mut(slot)?.state.words_mut()[0x12c / 4] = 0;
    die(
        vm,
        program,
        host,
        &Hit {
            victim: slot,
            inflictor: Some(slot),
            attacker: Some(slot),
            damage: 100000,
            flags: 0,
            means: 12,
            weapon: 0,
            point: None,
            direction: None,
            location: 0,
            offset: 0,
        },
    )
}
fn die(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    hit: &Hit,
) -> io::Result<()> {
    let p = host.world.players.get_mut(hit.victim)?;
    let pm_type = p.state.field("pm_type").expect("registered network field");
    let object = p.object;
    if pm_type > 1 || p.state.field("pm_flags").expect("registered network field") & 0x400000 != 0 {
        return Ok(());
    }
    let e = host
        .world
        .entities
        .get_mut(hit.victim)
        .ok_or_else(|| invalid("missing dead player entity"))?;
    if e.model_index == 0 {
        return Ok(());
    }
    let mut attributed = hit.clone();
    if let Some(turret) = hit
        .attacker
        .and_then(|a| host.world.entities.get(a))
        .filter(|e| e.state.field("eType") == Some(9) && e.owner_num != 1023)
    {
        attributed.attacker = Some(turret.owner_num);
    }
    if attributed.weapon != 0
        && let Some(attacker) = attributed.attacker
        && host
            .world
            .players
            .get(attacker)
            .is_some_and(|p| p.state.words()[0xa0 / 4] & 0x300 != 0)
        && let Some(turret) = host
            .world
            .entities
            .get(attacker)
            .and_then(|e| e.state.field("otherEntityNum"))
            .and_then(|n| host.world.entities.get(n as u16))
        && turret.state.field("eType") == Some(9)
    {
        attributed.weapon = turret.state.field("weapon").expect("registered netfield") as usize;
    }
    let hit = &attributed;
    vm.notify(object, "death", &[slot_value(host, hit.attacker)]);
    crate::combat::projectile::drop_held(host, hit.victim)?;
    host.world
        .players
        .get_mut(hit.victim)?
        .state
        .set_field("pm_type", pm_type + 6)?;
    let p = host.world.players.get_mut(hit.victim)?;
    let duration = host.world.animations.event(
        &mut p.anim_conditions,
        &mut p.state,
        1,
        false,
        true,
        &mut host.world.native_random,
    );
    p.anim_conditions.play_sounds(&mut p.state, host.strings)?;
    vm.invoke(
        program,
        host,
        CALLBACKS,
        "codecallback_playerkilled",
        object,
        hit.callback(host, Some(duration)),
    )?;
    crate::player::send_death_scores(&mut host.world.players, hit.victim);
    let target = hit
        .attacker
        .filter(|s| *s != hit.victim)
        .or(hit.inflictor.filter(|s| *s != hit.victim))
        .and_then(|s| host.world.entities.get_mut(s).map(|e| e.origin));
    let ent = host
        .world
        .entities
        .get_mut(hit.victim)
        .ok_or_else(|| invalid("missing dead player entity"))?;
    let killer_yaw = target.map_or(ent.angles[1], |point| {
        yaw(std::array::from_fn(|i| point[i] - ent.origin[i]))
    });
    ent.take_damage = true;
    ent.contents = 0x04000000;
    ent.angles[2] = 0.;
    ent.state.set_field("loopSound", 0);
    ent.maxs[2] = 30.;
    let angles = ent.angles;
    relink(ent, host.collision, ent.model_index != 0)?;
    host.world.set_field(object, "health", Value::Int(0))?;
    let ps = &mut host.world.players.get_mut(hit.victim)?.state;
    ps.words_mut()[0x12c / 4] = 0;
    ps.words_mut()[0x130 / 4] = killer_yaw as i32 as u32;
    for (key, a) in ["viewangles[0]", "viewangles[1]", "viewangles[2]"]
        .into_iter()
        .zip(angles)
    {
        ps.set_field(key, a.to_bits())?;
    }
    Ok(())
}

#[derive(Debug, Default)]
pub struct DamageFeedback {
    amount: i32,
    direction: Option<Vec3>,
}
pub fn apply_feedback(p: &mut crate::player::Player, now: i32) -> io::Result<()> {
    p.damage_feedback
        .apply(&mut p.state, p.max_health, &mut p.view, now)
}
impl DamageFeedback {
    fn apply(
        &mut self,
        state: &mut PlayerState,
        max_health: i32,
        view: &mut view::ViewMotion,
        now: i32,
    ) -> io::Result<()> {
        if state.field("pm_type").expect("registered network field") >= 6 {
            return Ok(());
        }
        state.set_field("damageCount", 0)?;
        if self.amount <= 0 || max_health <= 0 {
            return Ok(());
        }
        let amount = (self.amount.saturating_mul(100) / max_health).min(127);
        let spread = (float(state, "aimSpreadScale") + amount as f32).min(255.);
        state.set_field("aimSpreadScale", spread.to_bits())?;
        let strength = (amount as f32 * 0.2).clamp(5., 90.);
        view.damage_time = now.wrapping_sub(20);
        let (pitch, yaw) = if let Some(d) = self.direction {
            let angles =
                ["viewangles[0]", "viewangles[1]", "viewangles[2]"].map(|key| float(state, key));
            let [forward, right, _] = crate::vm::math::axes(angles);
            let dot = |axis: Vec3| {
                d.into_iter()
                    .zip(axis)
                    .map(|(a, b)| f64::from(a) * f64::from(b))
                    .sum::<f64>() as f32
            };
            view.damage_angles = [strength * dot(forward), strength * dot(right)];
            (
                (f64::from(view::direction_pitch(d)) * 256. / 360.) as u32,
                (f64::from(yaw(d)) * 256. / 360.) as u32,
            )
        } else {
            view.damage_angles = [-strength, 0.];
            (255, 255)
        };
        state.set_field("damagePitch", pitch)?;
        state.set_field("damageYaw", yaw)?;
        state.set_field(
            "damageEvent",
            state
                .field("damageEvent")
                .expect("registered network field")
                .wrapping_add(1),
        )?;
        state.set_field("damageCount", amount as u32)?;
        *self = Default::default();
        Ok(())
    }
}
pub fn dispatch_events(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    slot: u16,
    previous: i32,
) -> io::Result<()> {
    let p = host.world.players.get_mut(slot)?;
    let object = p.object;
    let mut index = previous.max(
        (p.state
            .field("eventSequence")
            .expect("registered player netfield") as i32)
            .saturating_sub(4),
    );
    loop {
        let p = host.world.players.get_mut(slot)?;
        if index
            >= p.state
                .field("eventSequence")
                .expect("registered player netfield") as i32
        {
            break;
        }
        let n = (index & 3) as usize;
        let event = p.state.words()[0xa8 / 4 + n];
        let parm = p.state.words()[0xb8 / 4 + n] as i32;
        index = index.wrapping_add(1);
        match event {
            0x9e..=0xa0 | 0xaf => shot::fire(vm, program, host, slot)?,
            0xa4 => shot::melee(vm, program, host, slot)?,
            0xa6 => crate::combat::projectile::offhand(host, slot)?,
            0xa8..=0xac => vm.notify(
                object,
                [
                    "binocular_enter",
                    "binocular_exit",
                    "binocular_fire",
                    "binocular_release",
                    "binocular_drop",
                ][(event - 0xa8) as usize],
                &[],
            ),
            0xc5 => {
                if host
                    .world
                    .entities
                    .get_mut(slot)
                    .is_some_and(|e| e.flags & 3 == 0)
                {
                    suicide(vm, program, host, slot)?;
                }
            }
            0x74..=0x8a => {
                if host
                    .world
                    .entities
                    .get(slot)
                    .and_then(|e| e.state.field("eType"))
                    != Some(1)
                {
                    return Ok(());
                }
                if parm == 0 {
                    continue;
                }
                let scale = if parm < 100 { parm as f32 * 0.01 } else { 1.1 };
                let p = host.world.players.get_mut(slot)?;
                let damage = (scale * (p.state.words()[0x134 / 4] as i32 as f32)) as i32;
                Hit {
                    victim: slot,
                    inflictor: None,
                    attacker: None,
                    damage,
                    flags: 0,
                    means: 11,
                    weapon: 0,
                    point: None,
                    direction: None,
                    location: 0,
                    offset: 0,
                }
                .damage(vm, program, host)?;
            }
            _ => (),
        }
    }
    Ok(())
}
pub fn trigger_hurt(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost,
    player: u16,
    trigger: u16,
    damage: i32,
    flags: i32,
) -> io::Result<()> {
    Hit {
        victim: player,
        inflictor: Some(trigger),
        attacker: Some(trigger),
        damage,
        flags,
        means: 13,
        weapon: 0,
        point: None,
        direction: None,
        location: 0,
        offset: 0,
    }
    .damage(vm, program, host)
}
