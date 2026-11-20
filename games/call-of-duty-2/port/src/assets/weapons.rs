//! Multiplayer weapon registration, item bitmask, and server-side definition fields.
use super::{GameAssets, invalid, models::ModelRegistry};
use crate::entities::Tokens;
use crate::level::ConfigStrings;
use crate::vm::spawn_fields::{float_prefix, parse_int};
use std::{collections::BTreeMap, io};

const DEFAULT_WEAPON: &str = "defaultweapon_mp";
const SMALL_HEALTH: usize = 129;
const LARGE_HEALTH: usize = 130;
/// The two health definitions occupy the fixed slots immediately after weapons.
pub(crate) const HEALTH_ITEMS: [(usize, &str, &str, &str); 2] = [
    (
        SMALL_HEALTH,
        "item_health_small",
        "Small Health",
        "xmodel/health_small",
    ),
    (
        LARGE_HEALTH,
        "item_health_large",
        "Large Health",
        "xmodel/health_large",
    ),
];
pub(crate) fn health_item(name: &str) -> Option<(usize, &'static str, &'static str, &'static str)> {
    HEALTH_ITEMS.into_iter().find(|(_, classname, alias, _)| {
        name.eq_ignore_ascii_case(classname) || name.eq_ignore_ascii_case(alias)
    })
}

#[derive(Clone, Debug, Default)]
pub struct WeaponDefinition {
    pub view: crate::combat::damage::view::ViewParameters,
    pub shot: crate::combat::damage::ShotParameters,
    pub projectile: crate::combat::projectile::Parameters,
    pub handling: crate::combat::WeaponHandling,
    pub name: String,
    pub world_model: String,
    pub projectile_model: String,
    pub use_hint_string: String,
    pub drop_hint_string: String,
    /// G_SpawnTurret's third-person/player loop and stop aliases, in wire index order.
    pub turret_arcs: [f32; 4],
    pub turret_spread: f32,
    pub turret_anim_increment: f32,
    pub turret_sounds: [String; 4],
    pub display_name: String,
    pub weapon_type: usize,
    pub player_anim_type: usize,
    pub stance: usize,
    pub offhand_class: usize,
    pub overlay_reticle: usize,
    pub projectile_explosion: usize,
    pub weapon_class: usize,
    pub weapon_slot: usize,
    pub slot_stackable: bool,
    pub enemy_crosshair_range: f32,
    pub alt_weapon_index: usize,
    pub ammo_index: usize,
    pub clip_index: usize,
    pub shared_ammo_cap_index: Option<usize>,
    pub drop_ammo_min: i32,
    pub drop_ammo_max: i32,
    pub start_ammo: i32,
    pub max_ammo: i32,
    pub clip_size: i32,
    pub accuracy: [Vec<[f32; 2]>; 2],
    alt_weapon: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ItemRegistration {
    Weapon(usize),
    HealthModel(&'static str),
}

#[derive(Clone, Debug)]
pub struct WeaponRegistry {
    pub hit_location_multipliers: [f32; 19],
    definitions: Vec<WeaponDefinition>,
    player_anim_types: Vec<String>,
    ammo: Vec<(String, i32)>,
    clips: Vec<(String, i32)>,
    shared_caps: Vec<(String, i32)>,
    health_registered: [bool; 2],
    registrations: Vec<ItemRegistration>,
}

impl WeaponRegistry {
    /// BG_InitWeaponRegistry reserves slot zero for "none"; G_SetupWeaponDef
    /// then registers the default multiplayer weapon through the ordinary loader.
    pub fn load(assets: &GameAssets) -> io::Result<Self> {
        let mut none = WeaponDefinition {
            name: "none".into(),
            shared_ammo_cap_index: Some(0),
            ..Default::default()
        };
        // BG_GetNoWeaponDef keeps the sentinel even if an accuracy file fails.
        for (idx, folder) in ["aivsai", "aivsplayer"].into_iter().enumerate() {
            let Some(graph) = accuracy_graph(assets, folder, "noweapon.accu")? else {
                break;
            };
            none.accuracy[idx] = graph;
        }
        let anim_file = assets.read_file("mp/playeranimtypes.txt")?;
        if anim_file.len() > 4095 {
            return Err(invalid("mp/playeranimtypes.txt max size exceeded"));
        }
        let mut tokens = Tokens::new(&anim_file);
        let mut player_anim_types = Vec::new();
        while let Some(token) = tokens.next().filter(|token| !token.is_empty()) {
            if player_anim_types.len() == 64 {
                return Err(invalid("player anim type array size exceeded"));
            }
            player_anim_types.push(String::from_utf8_lossy(&token).into_owned());
        }
        let mut registry = Self {
            hit_location_multipliers: std::array::from_fn(|i| if i == 18 { 0. } else { 1. }),
            definitions: vec![none],
            player_anim_types,
            ammo: vec![(String::new(), 0)],
            clips: vec![(String::new(), 0)],
            shared_caps: vec![(String::new(), 0)],
            health_registered: [false; 2],
            registrations: Vec::new(),
        };
        registry.resolve(assets, DEFAULT_WEAPON, true)?;
        Ok(registry)
    }

    pub fn load_hit_locations(&mut self, assets: &GameAssets) -> io::Result<()> {
        self.hit_location_multipliers =
            crate::combat::damage::parse_locations(&assets.read_file("info/mp_lochit_dmgtable")?)?;
        Ok(())
    }

    pub fn player_anim_types(&self) -> &[String] {
        &self.player_anim_types
    }

    pub fn get(&self, index: usize) -> Option<&WeaponDefinition> {
        self.definitions.get(index)
    }

    /// G_LoadWeaponDef only searches existing registrations after initialization.
    pub fn resolve(
        &mut self,
        assets: &GameAssets,
        name: &str,
        initializing: bool,
    ) -> io::Result<usize> {
        if name.is_empty() || name.eq_ignore_ascii_case("none") {
            return Ok(0);
        }
        if let Some(index) = self
            .definitions
            .iter()
            .position(|def| def.name.eq_ignore_ascii_case(name))
        {
            return Ok(index);
        }
        if !initializing {
            return Ok(0);
        }
        // Slot 129 starts the fixed item table. BG_FillInWeaponItem writes
        // one item per weapon, so overlapping that table would corrupt it.
        if self.definitions.len() >= SMALL_HEALTH {
            return Err(invalid("multiplayer weapon item table full"));
        }

        // On an unsuccessful alt-weapon chain no partially registered definition
        // may survive to turn the same request into a successful cache hit.
        let lengths = (
            self.definitions.len(),
            self.ammo.len(),
            self.clips.len(),
            self.shared_caps.len(),
            self.registrations.len(),
        );
        let result = self.register(assets, name);
        if result.is_err() {
            self.definitions.truncate(lengths.0);
            self.ammo.truncate(lengths.1);
            self.clips.truncate(lengths.2);
            self.shared_caps.truncate(lengths.3);
            self.registrations.truncate(lengths.4);
        }
        result
    }

    fn register(&mut self, assets: &GameAssets, name: &str) -> io::Result<usize> {
        let file = match load_file(assets, name, &self.player_anim_types)? {
            Some(file) => file,
            None if !name.eq_ignore_ascii_case(DEFAULT_WEAPON) => {
                load_file(assets, DEFAULT_WEAPON, &self.player_anim_types)?
                    .ok_or_else(|| invalid("BG_LoadWeaponDef: Could not find default weapon"))?
            }
            None => return Err(invalid("BG_LoadWeaponDef: Could not find default weapon")),
        };
        let mut def = file.def;
        def.name = name.to_owned();
        def.ammo_index = group(&mut self.ammo, file.ammo, def.max_ammo, "ammo")?;
        if !file.cap.is_empty() {
            def.shared_ammo_cap_index = Some(group(
                &mut self.shared_caps,
                file.cap,
                file.cap_limit,
                "shared ammo cap",
            )?);
        }
        def.clip_index = group(&mut self.clips, file.clip, def.clip_size, "clip")?;
        let alt = std::mem::take(&mut def.alt_weapon);
        let index = self.definitions.len();
        self.definitions.push(def);
        if !alt.is_empty() {
            let alt_index = self.resolve(assets, &alt, true)?;
            let other = &self.definitions[alt_index];
            let current = &self.definitions[index];
            if alt_index == 0
                || other.weapon_slot != current.weapon_slot
                || other.slot_stackable != current.slot_stackable
            {
                return Err(invalid(format!(
                    "incompatible altWeapon '{alt}' for '{name}'"
                )));
            }
            self.definitions[index].alt_weapon_index = alt_index;
        }
        // BG_RegisterWeaponDef invokes G_PrecacheWeapon after resolving its alt.
        self.registrations.push(ItemRegistration::Weapon(index));
        Ok(index)
    }

    pub fn item_registered(&self, index: usize) -> bool {
        self.get(index).is_some()
            || index
                .checked_sub(SMALL_HEALTH)
                .and_then(|i| self.health_registered.get(i))
                .copied()
                .unwrap_or(false)
    }
    /// Item_SetupDroppedWeapon registers the health item without dirtying
    /// configstring 8; its own Ent_SetModelByName publishes the model.
    pub(crate) fn register_spawned_health(&mut self, id: usize) {
        self.health_registered[id - SMALL_HEALTH] = true;
    }
    pub fn precache_item(&mut self, assets: &GameAssets, name: &str) -> io::Result<usize> {
        if let Some((index, _, _, model)) = health_item(name) {
            if !self.health_registered[index - SMALL_HEALTH] {
                self.health_registered[index - SMALL_HEALTH] = true;
                self.registrations
                    .push(ItemRegistration::HealthModel(model));
            }
            return Ok(index);
        }
        let index = self.resolve(assets, name, true)?;
        if index == 0 {
            return Err(invalid(format!("unknown item '{name}'")));
        }
        Ok(index)
    }

    pub fn take_registrations(&mut self) -> Vec<ItemRegistration> {
        std::mem::take(&mut self.registrations)
    }

    /// Apply G_PrecacheWeapon and ItemRegistry_Register in callback order.
    pub fn publish(
        &mut self,
        assets: &GameAssets,
        models: &mut ModelRegistry,
        strings: &mut ConfigStrings,
    ) -> io::Result<()> {
        if self.registrations.is_empty() {
            return Ok(());
        }
        for item in &self.registrations {
            match *item {
                ItemRegistration::Weapon(index) => {
                    let def = &self.definitions[index];
                    for hint in [&def.use_hint_string, &def.drop_hint_string] {
                        if !hint.is_empty() {
                            let slot = (0x4fe..0x51e)
                                .find(|&slot| {
                                    let current =
                                        strings.get(slot).expect("hint configstring range");
                                    current.is_empty() || current == hint.as_bytes()
                                })
                                .ok_or_else(|| {
                                    invalid("too many weapon hintstrings (maximum 32)")
                                })?;
                            strings.set(slot, hint.as_bytes())?;
                        }
                    }
                    models.weapon_world_index(assets, strings, &def.world_model, true)?;
                    models.index(assets, strings, &def.projectile_model, true)?;
                }
                ItemRegistration::HealthModel(name) => {
                    models.index(assets, strings, name, true)?;
                }
            }
        }
        strings.set(7, self.names_configstring().as_bytes())?;
        strings.set(8, self.items_configstring().as_bytes())?;
        self.registrations.clear();
        Ok(())
    }

    pub fn names_configstring(&self) -> String {
        let mut names = self.definitions[1..]
            .iter()
            .map(|def| def.name.as_str())
            .collect::<Vec<_>>()
            .join(" ");
        names.truncate(names.floor_char_boundary(8191));
        names
    }

    pub fn items_configstring(&self) -> String {
        const HEX: &[u8; 16] = b"0123456789abcdef";
        let mut nibbles = [0usize; 33];
        for index in 0..self.definitions.len() {
            nibbles[index / 4] |= 1 << (index % 4);
        }
        for (i, &registered) in self.health_registered.iter().enumerate() {
            if registered {
                let index = SMALL_HEALTH + i;
                nibbles[index / 4] |= 1 << (index % 4);
            }
        }
        nibbles.into_iter().map(|n| char::from(HEX[n])).collect()
    }
}

fn group(
    groups: &mut Vec<(String, i32)>,
    name: String,
    count: i32,
    kind: &str,
) -> io::Result<usize> {
    if let Some(index) = groups
        .iter()
        .position(|(existing, _)| existing.eq_ignore_ascii_case(&name))
    {
        if index != 0 && groups[index].1 != count {
            return Err(invalid(format!("mismatched {kind} count for '{name}'")));
        }
        return Ok(index);
    }
    groups.push((name, count));
    Ok(groups.len() - 1)
}

struct WeaponFile {
    def: WeaponDefinition,
    ammo: String,
    clip: String,
    cap: String,
    cap_limit: i32,
}

// BG_LoadWeaponFile and WeaponAccuracy_LoadFile both bound the payload to
// 8191 bytes after the magic and treat malformed headers as a failed lookup.
fn payload<'a>(bytes: &'a [u8], magic: &[u8]) -> Option<&'a [u8]> {
    let body = bytes.strip_prefix(magic)?;
    (body.len() < 8192).then(|| body.split(|&b| b == 0).next().unwrap_or_default())
}

fn try_read(assets: &GameAssets, path: &str) -> io::Result<Option<Vec<u8>>> {
    match assets.read_file(path) {
        Ok(bytes) => Ok(Some(bytes)),
        Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(None),
        Err(e) => Err(e),
    }
}

fn number(bytes: &[u8]) -> f32 {
    float_prefix(bytes)
        .and_then(|(s, _)| s.parse::<f64>().ok())
        .unwrap_or(0.0) as f32
}

fn accuracy_graph(
    assets: &GameAssets,
    folder: &str,
    name: &str,
) -> io::Result<Option<Vec<[f32; 2]>>> {
    let Some(bytes) = try_read(assets, &format!("accuracy/{folder}/{name}"))? else {
        return Ok(None);
    };
    let Some(body) = payload(&bytes, b"WEAPONACCUFILE") else {
        return Ok(None);
    };
    let mut tokens = Tokens::new(body);
    let count = parse_int(&tokens.next().unwrap_or_default());
    let mut knots = Vec::new();
    let mut coordinate = || tokens.next().filter(|s| !s.is_empty() && s[0] != b'}');
    while let (Some(x), Some(y)) = (coordinate(), coordinate()) {
        knots.push([number(&x), number(&y)]);
        if knots.len() >= 16 {
            return Ok(None);
        }
    }
    Ok((knots.len() as i32 == count).then_some(knots))
}

fn enum_index(value: &str, values: &[impl AsRef<str>], key: &str) -> io::Result<usize> {
    if value.is_empty() {
        return Ok(0);
    }
    values
        .iter()
        .position(|item| item.as_ref().eq_ignore_ascii_case(value))
        .ok_or_else(|| invalid(format!("unknown {key} '{value}'")))
}

fn load_file(
    assets: &GameAssets,
    name: &str,
    anim_types: &[String],
) -> io::Result<Option<WeaponFile>> {
    let mut path = format!("weapons/mp/{name}");
    path.truncate(path.floor_char_boundary(63));
    let Some(bytes) = try_read(assets, &path)? else {
        return Ok(None);
    };
    let Some(body) = payload(&bytes, b"WEAPONFILE") else {
        return Ok(None);
    };
    if body.iter().any(|b| matches!(b, b'"' | b';')) {
        return Ok(None);
    }
    let mut parts = body
        .strip_prefix(b"\\")
        .unwrap_or(body)
        .split(|&b| b == b'\\');
    let mut fields = BTreeMap::new();
    while let (Some(key), Some(value)) = (parts.next(), parts.next()) {
        // Info_ValueForKey searches from the start and ignores key case.
        fields
            .entry(String::from_utf8_lossy(key).to_ascii_lowercase())
            .or_insert(value);
    }
    let raw = |key: &str| fields.get(key).copied().unwrap_or_default();
    let text = |key: &str| String::from_utf8_lossy(raw(key)).into_owned();
    let weapon_type = enum_index(
        &text("weapontype"),
        &["bullet", "grenade", "projectile", "binoculars"],
        "weapon type",
    )?;
    let weapon_class = enum_index(
        &text("weaponclass"),
        &[
            "rifle",
            "mg",
            "smg",
            "spread",
            "pistol",
            "grenade",
            "rocketlauncher",
            "turret",
            "non-player",
            "item",
        ],
        "weapon class",
    )?;
    let weapon_slot = enum_index(
        &text("weaponslot"),
        &["none", "primary", "primaryb"],
        "weapon slot",
    )?;
    if number(raw("enemycrosshairrange")) > 15000.0 {
        return Err(invalid(format!(
            "enemyCrosshairRange exceeds 15000 in '{name}'"
        )));
    }
    if weapon_type == 2
        && (parse_int(raw("projectilespeed")) <= 0
            || number(raw("destabilizationanglemax")) >= 45.0
            || number(raw("destabilizationanglemax")) < 0.0
            || number(raw("destabilizationbasetime")) <= 0.0
            || number(raw("destabilizationtimereductionratio")) <= 0.0)
    {
        return Err(invalid(format!("invalid projectile settings in '{name}'")));
    }
    let mut accuracy = [Vec::new(), Vec::new()];
    if matches!(weapon_type, 0 | 2) {
        for (index, (key, folder)) in [
            ("aivsaiaccuracygraph", "aivsai"),
            ("aivsplayeraccuracygraph", "aivsplayer"),
        ]
        .into_iter()
        .enumerate()
        {
            let graph_name = text(key);
            if !graph_name.is_empty() {
                let Some(graph) = accuracy_graph(assets, folder, &graph_name)? else {
                    return Ok(None);
                };
                accuracy[index] = graph;
            }
        }
    }
    Ok(Some(WeaponFile {
        def: WeaponDefinition {
            view: crate::combat::damage::view::ViewParameters::parse(raw),
            shot: crate::combat::damage::ShotParameters::parse(raw),
            enemy_crosshair_range: number(raw("enemycrosshairrange")),
            projectile: crate::combat::projectile::Parameters::parse(raw),
            handling: crate::combat::WeaponHandling::parse(raw),
            world_model: text("worldmodel"),
            projectile_model: text("projectilemodel"),
            use_hint_string: text("usehintstring"),
            drop_hint_string: text("drophintstring"),
            turret_arcs: ["toparc", "rightarc", "bottomarc", "leftarc"].map(|key| number(raw(key))),
            turret_spread: number(raw("playerspread")),
            turret_anim_increment: number(raw("animhorrotateinc")),
            turret_sounds: [
                "loopfiresound",
                "loopfiresoundplayer",
                "stopfiresound",
                "stopfiresoundplayer",
            ]
            .map(text),
            display_name: text("displayname"),
            weapon_type,
            weapon_class,
            weapon_slot,
            player_anim_type: enum_index(&text("playeranimtype"), anim_types, "player anim type")?,
            stance: enum_index(&text("stance"), &["stand", "duck", "prone"], "stance")?,
            offhand_class: enum_index(
                &text("offhandclass"),
                &["None", "Frag Grenade", "Smoke Grenade"],
                "offhand class",
            )?,
            overlay_reticle: enum_index(
                &text("adsoverlayreticle"),
                &["none", "crosshair", "FG42", "Springfield", "binoculars"],
                "overlay reticle",
            )?,
            projectile_explosion: enum_index(
                &text("projexplosiontype"),
                &["grenade", "rocket", "none"],
                "projectile explosion",
            )?,
            slot_stackable: parse_int(raw("slotstackable")) != 0,
            alt_weapon: text("altweapon"),
            drop_ammo_min: parse_int(raw("dropammomin")),
            drop_ammo_max: parse_int(raw("dropammomax")),
            start_ammo: parse_int(raw("startammo")),
            max_ammo: parse_int(raw("maxammo")),
            clip_size: parse_int(raw("clipsize")),
            accuracy,
            ..Default::default()
        },
        ammo: text("ammoname").to_ascii_lowercase(),
        clip: text("clipname").to_ascii_lowercase(),
        cap: text("sharedammocapname"),
        cap_limit: parse_int(raw("sharedammocap")),
    }))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn fixture(files: &[(&str, &[u8])]) -> io::Result<(tempfile::TempDir, GameAssets)> {
        let root = tempfile::tempdir()?;
        fs::create_dir_all(root.path().join("main/mp"))?;
        fs::write(
            root.path().join("main/mp/playeranimtypes.txt"),
            "none\nother\n",
        )?;
        for &(name, bytes) in files {
            let path = root.path().join("main").join(name);
            fs::create_dir_all(path.parent().expect("fixture file has a parent directory"))?;
            fs::write(path, bytes)?;
        }
        let assets = GameAssets::mount_paths(root.path(), None, "")?;
        Ok((root, assets))
    }

    #[test]
    fn ordered_registration_alt_and_fixed_health_items() -> io::Result<()> {
        let (_root, assets) = fixture(&[
            ("weapons/mp/defaultweapon_mp", br"WEAPONFILE\worldmodel\xmodel/from_file"),
            ("weapons/mp/first", br"WEAPONFILE\worldModel\xmodel/first\WorldModel\ignored\altWeapon\second\weaponSlot\primary\ammoName\Rounds\maxAmmo\30"),
            ("weapons/mp/second", br"WEAPONFILE\altWeapon\first\weaponSlot\Primary\ammoName\rounds\maxAmmo\30"),
        ])?;
        let mut registry = WeaponRegistry::load(&assets)?;
        assert_eq!(
            registry.items_configstring(),
            format!("3{}", "0".repeat(32))
        );
        assert_eq!(registry.resolve(&assets, "first", false)?, 0);
        assert_eq!(registry.precache_item(&assets, "first")?, 2);
        assert_eq!(registry.precache_item(&assets, "FIRST")?, 2);
        let first = registry.get(2).expect("registered fixture weapon");
        assert_eq!(first.world_model, "xmodel/first");
        assert_eq!(first.alt_weapon_index, 3);
        assert_eq!(
            registry
                .get(3)
                .expect("registered fixture weapon")
                .ammo_index,
            first.ammo_index
        );
        assert_eq!(
            registry
                .get(3)
                .expect("registered fixture weapon")
                .alt_weapon_index,
            2
        );
        assert_eq!(registry.precache_item(&assets, "Small Health")?, 129);
        assert_eq!(registry.precache_item(&assets, "ITEM_HEALTH_SMALL")?, 129);
        assert_eq!(
            registry.take_registrations(),
            vec![
                ItemRegistration::Weapon(1),
                ItemRegistration::Weapon(3),
                ItemRegistration::Weapon(2),
                ItemRegistration::HealthModel("xmodel/health_small")
            ]
        );
        assert_eq!(
            registry.names_configstring(),
            "defaultweapon_mp first second"
        );
        assert_eq!(
            registry.items_configstring(),
            format!("f{}2", "0".repeat(31))
        );
        Ok(())
    }

    #[test]
    fn fallback_uses_real_default_and_rolls_back_failed_alt() -> io::Result<()> {
        let (_root, assets) = fixture(&[
            (
                "weapons/mp/defaultweapon_mp",
                br"WEAPONFILE\worldModel\xmodel/fallback",
            ),
            ("weapons/mp/alternate", br"WEAPONFILE\weaponSlot\primaryb"),
            (
                "weapons/mp/broken",
                br"WEAPONFILE\altWeapon\alternate\weaponSlot\primary",
            ),
            ("weapons/mp/bad_header", b"junk"),
        ])?;
        let mut registry = WeaponRegistry::load(&assets)?;
        assert!(registry.resolve(&assets, "broken", true).is_err());
        assert_eq!(registry.names_configstring(), DEFAULT_WEAPON);
        for name in ["absent", "bad_header"] {
            let index = registry.resolve(&assets, name, true)?;
            let definition = registry.get(index).expect("registered fixture weapon");
            assert_eq!(definition.world_model, "xmodel/fallback");
            assert_eq!(definition.name, name);
        }
        assert!(registry.precache_item(&assets, "none").is_err());
        let (_root, assets) = fixture(&[("weapons/mp/defaultweapon_mp", b"bad")])?;
        assert!(WeaponRegistry::load(&assets).is_err());
        Ok(())
    }

    #[test]
    fn publishes_hintstrings_and_models_in_callback_order() -> io::Result<()> {
        let (_root, assets) = fixture(&[
            ("weapons/mp/defaultweapon_mp", br"WEAPONFILE\worldModel\xmodel/defaultweapon"),
            ("weapons/mp/turret", br"WEAPONFILE\weaponClass\turret\worldModel\xmodel/stand\projectileModel\xmodel/effect\useHintString\USE\dropHintString\DROP"),
        ])?;
        let mut weapons = WeaponRegistry::load(&assets)?;
        let mut models = ModelRegistry::default();
        let mut strings = ConfigStrings::default();
        weapons.precache_item(&assets, "turret")?;
        weapons.precache_item(&assets, "Large Health")?;
        weapons.publish(&assets, &mut models, &mut strings)?;
        assert_eq!(strings.get(0x4fe).expect("valid configstring slot"), b"USE");
        assert_eq!(
            strings.get(0x4ff).expect("valid configstring slot"),
            b"DROP"
        );
        for (i, expected) in [
            b"xmodel/defaultweapon".as_slice(),
            b"xmodel/stand",
            b"xmodel/effect",
            b"xmodel/health_large",
        ]
        .iter()
        .enumerate()
        {
            assert_eq!(
                strings.get(0x14f + i).expect("valid configstring slot"),
                *expected
            );
        }
        assert_eq!(
            strings.get(7).expect("valid configstring slot"),
            b"defaultweapon_mp turret"
        );
        assert_eq!(
            strings.get(8).expect("valid configstring slot"),
            format!("7{}4", "0".repeat(31)).as_bytes()
        );
        Ok(())
    }

    #[test]
    fn reads_graph_knots_and_falls_back_for_invalid_counts() -> io::Result<()> {
        let (_root, assets) = fixture(&[
            (
                "weapons/mp/defaultweapon_mp",
                br"WEAPONFILE\worldModel\default",
            ),
            (
                "weapons/mp/with_graph",
                br"WEAPONFILE\aiVsAiAccuracyGraph\a.accu\aiVsPlayerAccuracyGraph\b.accu",
            ),
            (
                "weapons/mp/bad_graph",
                br"WEAPONFILE\aiVsAiAccuracyGraph\bad.accu",
            ),
            (
                "accuracy/aivsai/a.accu",
                b"WEAPONACCUFILE\n2\n0.25 0.5\n1 0\n",
            ),
            ("accuracy/aivsplayer/b.accu", b"WEAPONACCUFILE\n1\n0 0\n"),
            ("accuracy/aivsai/bad.accu", b"WEAPONACCUFILE\n3\n0 0\n"),
        ])?;
        let mut registry = WeaponRegistry::load(&assets)?;
        let good = registry.resolve(&assets, "with_graph", true)?;
        let bad = registry.resolve(&assets, "bad_graph", true)?;
        assert_eq!(
            registry
                .get(good)
                .expect("registered fixture weapon")
                .accuracy,
            [vec![[0.25, 0.5], [1.0, 0.0]], vec![[0.0, 0.0]]]
        );
        assert_eq!(
            registry
                .get(bad)
                .expect("registered fixture weapon")
                .world_model,
            "default"
        );
        Ok(())
    }
}
