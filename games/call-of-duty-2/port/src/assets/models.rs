//! Version 20 XModel and XModelParts records read by the dedicated server.

use super::GameAssets;
use crate::level::ConfigStrings;
use std::collections::BTreeMap;
use std::io;
use std::sync::Arc;

pub const MODEL_CONFIGSTRING_BASE: usize = 0x14e;

#[derive(Debug)]
pub struct ServerModel {
    pub is_default: bool,
    pub flags: u8,
    pub bounds: [[f32; 3]; 2],
    pub lods: [ModelLod; 4],
    pub collision_lod: i16,
    pub collision: Vec<CollisionSurface>,
    pub contents: u32,
    pub parts: Arc<ModelParts>,
    pub bone_bounds: Vec<BoneBounds>,
}

#[derive(Debug, Default)]
pub struct ModelLod {
    pub distance: f32,
    pub name: String,
    pub surfaces: Vec<String>,
}

#[derive(Debug)]
pub struct CollisionSurface {
    /// Plane followed by S and T projection planes, as used by XModel_TraceLine.
    pub triangles: Vec<[f32; 12]>,
    pub bounds: [[f32; 3]; 2],
    pub bone_index: i32,
    pub contents: u32,
    pub flags: u32,
}

#[derive(Debug)]
pub struct ModelParts {
    pub root_count: u16,
    pub bones: Vec<ModelBone>,
}

#[derive(Debug, Default)]
pub struct ModelBone {
    pub name: String,
    pub parent: Option<u8>,
    pub classification: u8,
    pub local_translation: [f32; 3],
    pub local_quaternion: [i16; 4],
    pub bind_pose: BoneTransform,
}

/// Absolute rest transform built by XModelParts_BuildBaseMats.
#[derive(Clone, Copy, Debug, Default)]
pub struct BoneTransform {
    pub quaternion: [f32; 4],
    pub origin: [f32; 3],
    pub scale: f32,
}

impl BoneTransform {
    fn root() -> Self {
        Self {
            quaternion: [0., 0., 0., 1.],
            origin: [0.; 3],
            scale: 2.,
        }
    }

    fn child(self, packed: [i16; 4], translation: [f32; 3]) -> Self {
        let [x, y, z, w] = packed.map(|n| f64::from(f32::from(n) * (1.0f32 / 32767.0)));
        let [px, py, pz, pw] = self.quaternion.map(f64::from);
        let mut quaternion = [
            x * pw + w * px + z * py - y * pz,
            y * pw - z * px + w * py + x * pz,
            z * pw + y * px - x * py + w * pz,
            w * pw - x * px - y * py - z * pz,
        ]
        .map(|v| v as f32);
        let len = quaternion
            .iter()
            .rev()
            .map(|&v| f64::from(v).powi(2))
            .sum::<f64>() as f32;
        let scale = if len == 0. {
            quaternion[3] = 1.;
            2.
        } else {
            2. / len
        };
        Self {
            quaternion,
            origin: self.transform_point(translation),
            scale,
        }
    }

    pub fn axes(self) -> [[f32; 3]; 3] {
        let [x, y, z, w] = self.quaternion;
        let [sx, sy, sz] = [x, y, z].map(|v| v * self.scale);
        let diagonal = |a: f32, b: f32| (1.0 - (f64::from(a) + f64::from(b))) as f32;
        [
            [diagonal(sy * y, sz * z), sx * y + sz * w, sx * z - sy * w],
            [sx * y - sz * w, diagonal(sx * x, sz * z), sy * z + sx * w],
            [sx * z + sy * w, sy * z - sx * w, diagonal(sx * x, sy * y)],
        ]
    }

    pub fn transform_point(self, point: [f32; 3]) -> [f32; 3] {
        let matrix = self.axes();
        std::array::from_fn(|col| {
            let rotated = (0..3)
                .map(|row| f64::from(point[row]) * f64::from(matrix[row][col]))
                .sum::<f64>();
            (rotated + f64::from(self.origin[col])) as f32
        })
    }
}

#[derive(Debug)]
pub struct BoneBounds {
    pub bounds: [[f32; 3]; 2],
    pub center: [f32; 3],
    pub radius_squared: f32,
}

struct Input<'a>(&'a [u8]);

fn invalid(message: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message)
}

impl<'a> Input<'a> {
    fn take(&mut self, length: usize) -> io::Result<&'a [u8]> {
        let (value, rest) = self
            .0
            .split_at_checked(length)
            .ok_or_else(|| invalid("truncated XModel data"))?;
        self.0 = rest;
        Ok(value)
    }

    fn bytes<const N: usize>(&mut self) -> io::Result<[u8; N]> {
        Ok(self.take(N)?.try_into().expect("exact field length"))
    }

    fn byte(&mut self) -> io::Result<u8> {
        Ok(self.bytes::<1>()?[0])
    }
    fn short(&mut self) -> io::Result<u16> {
        Ok(u16::from_le_bytes(self.bytes()?))
    }
    fn word(&mut self) -> io::Result<u32> {
        Ok(u32::from_le_bytes(self.bytes()?))
    }
    fn float(&mut self) -> io::Result<f32> {
        Ok(f32::from_bits(self.word()?))
    }
    fn floats<const N: usize>(&mut self) -> io::Result<[f32; N]> {
        let mut values = [0.; N];
        for v in &mut values {
            *v = self.float()?;
        }
        Ok(values)
    }

    fn name(&mut self) -> io::Result<String> {
        let length = self
            .0
            .iter()
            .position(|&byte| byte == 0)
            .ok_or_else(|| invalid("unterminated XModel name"))?;
        let bytes = self.take(length + 1)?;
        std::str::from_utf8(&bytes[..length])
            .map(str::to_owned)
            .map_err(|_| invalid("invalid XModel name encoding"))
    }

    fn version(&mut self) -> io::Result<()> {
        if self.0.is_empty() || self.short()? != 20 {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "expected XModel version 20",
            ));
        }
        Ok(())
    }
}

impl ModelParts {
    fn parse(bytes: &[u8]) -> io::Result<Self> {
        let mut input = Input(bytes);
        input.version()?;
        let child_count = usize::from(input.short()?);
        let root_count = input.short()?;
        let total = child_count + usize::from(root_count);
        if total > 127 {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "XModel has more than 127 bones",
            ));
        }
        let mut bones: Vec<ModelBone> = Vec::with_capacity(total);
        for index in 0..total {
            let mut bone = ModelBone {
                local_quaternion: [0, 0, 0, 32767],
                bind_pose: BoneTransform::root(),
                ..ModelBone::default()
            };
            if index >= usize::from(root_count) {
                let parent = input.byte()?;
                if usize::from(parent) >= index {
                    return Err(invalid("XModel parent must precede child"));
                }
                bone.parent = Some(parent);
                bone.local_translation = input.floats()?;
                let mut square = 0i32;
                for v in &mut bone.local_quaternion[..3] {
                    *v = input.short()? as i16;
                    square = square.wrapping_add(i32::from(*v) * i32::from(*v));
                }
                let remaining = 0x3fff0001i32.wrapping_sub(square);
                bone.local_quaternion[3] = if remaining < 1 {
                    0
                } else {
                    (remaining as f32).sqrt().round() as i16
                };
                bone.bind_pose = bones[usize::from(parent)]
                    .bind_pose
                    .child(bone.local_quaternion, bone.local_translation);
            }
            bones.push(bone);
        }
        for bone in &mut bones {
            bone.name = input.name()?;
        }
        for bone in &mut bones {
            bone.classification = input.byte()?;
        }
        Ok(Self { root_count, bones })
    }
}

impl ServerModel {
    /// XModel_InitDefault uses zeroed model bounds and one unnamed root bone.
    fn fallback() -> Self {
        Self {
            is_default: true,
            flags: 0,
            bounds: [[0.; 3]; 2],
            lods: std::array::from_fn(|_| ModelLod {
                surfaces: vec![String::new()],
                ..ModelLod::default()
            }),
            collision_lod: 0,
            collision: Vec::new(),
            contents: 0,
            parts: Arc::new(ModelParts {
                root_count: 1,
                bones: vec![ModelBone::default()],
            }),
            bone_bounds: vec![BoneBounds {
                bounds: [[-16.; 3], [16.; 3]],
                center: [0.; 3],
                radius_squared: 0.,
            }],
        }
    }

    fn parse(
        bytes: &[u8],
        parts: impl FnOnce(&str) -> io::Result<Arc<ModelParts>>,
    ) -> io::Result<Self> {
        let mut input = Input(bytes);
        input.version()?;
        let flags = input.byte()?;
        let bounds = [input.floats()?, input.floats()?];
        let mut lods: [ModelLod; 4] = Default::default();
        for lod in &mut lods {
            lod.distance = input.float()?;
            lod.name = input.name()?;
        }
        let collision_lod = input.word()? as i16;
        let surface_count = input.word()? as usize;
        if surface_count > input.0.len() / 40 {
            return Err(invalid("truncated XModel collision surfaces"));
        }
        let mut collision = Vec::with_capacity(surface_count);
        let mut contents = 0;
        for _ in 0..surface_count {
            let triangle_count = input.word()? as usize;
            if triangle_count > input.0.len() / 48 {
                return Err(invalid("truncated XModel collision triangles"));
            }
            let mut triangles = Vec::with_capacity(triangle_count);
            for _ in 0..triangle_count {
                triangles.push(input.floats()?);
            }
            let mins = input.floats()?.map(|x| x - 0.001);
            let maxs = input.floats()?.map(|x| x + 0.001);
            let bone_index = input.word()? as i32;
            let surface_contents = input.word()? & 0xdffffffb;
            let flags = input.word()?;
            contents |= surface_contents;
            collision.push(CollisionSurface {
                triangles,
                bounds: [mins, maxs],
                bone_index,
                contents: surface_contents,
                flags,
            });
        }
        for lod in &mut lods {
            if !lod.name.is_empty() {
                let count = input.short()?;
                for _ in 0..count {
                    lod.surfaces.push(input.name()?);
                }
            }
        }
        let parts = parts(&lods[0].name)?;
        let mut bone_bounds = Vec::with_capacity(parts.bones.len());
        for _ in &parts.bones {
            let mins = input.floats::<3>()?;
            let maxs = input.floats::<3>()?;
            let center = std::array::from_fn(|i| (mins[i] + maxs[i]) * 0.5);
            // Vec3_Sub stores three floats. Vec3_LengthSq accumulates the
            // products in the x87 register before storing a single float.
            let radius_squared = (0..3)
                .map(|i| f64::from(maxs[i] - center[i]).powi(2))
                .sum::<f64>() as f32;
            bone_bounds.push(BoneBounds {
                bounds: [mins, maxs],
                center,
                radius_squared,
            });
        }
        Ok(Self {
            is_default: false,
            flags,
            bounds,
            lods,
            collision_lod,
            collision,
            contents,
            parts,
            bone_bounds,
        })
    }
}

#[derive(Clone, Debug, Default)]
pub struct ModelRegistry {
    models: BTreeMap<u8, Arc<ServerModel>>,
    by_name: BTreeMap<String, Arc<ServerModel>>,
    parts: BTreeMap<String, Arc<ModelParts>>,
    bone_names: BTreeMap<String, u16>,
}

impl ModelRegistry {
    /// SV_RegisterModel loads server model data independently of network model slots.
    pub(crate) fn register(
        &mut self,
        assets: &GameAssets,
        name: &str,
    ) -> io::Result<Arc<ServerModel>> {
        let key = name.to_ascii_lowercase();
        if let Some(model) = self.by_name.get(&key) {
            return Ok(Arc::clone(model));
        }
        let model = Arc::new(load_model(assets, name, &mut self.parts)?);
        self.register_bone_names(&model)?;
        self.by_name.insert(key, Arc::clone(&model));
        Ok(model)
    }

    /// G_ModelIndex searches the contiguous configstring range in ascending order.
    pub fn index(
        &mut self,
        assets: &GameAssets,
        strings: &mut ConfigStrings,
        name: &str,
        initializing: bool,
    ) -> io::Result<u8> {
        if name.is_empty() {
            return Ok(0);
        }
        for index in 1..=u8::MAX {
            let slot = MODEL_CONFIGSTRING_BASE + usize::from(index);
            let existing = strings
                .get(slot)
                .expect("model range within server configstrings");
            if existing.eq_ignore_ascii_case(name.as_bytes()) {
                return Ok(index);
            }
            if existing.is_empty() {
                if !initializing {
                    return Err(invalid(&format!("model '{name}' not precached")));
                }
                let model = self.register(assets, name)?;
                strings.set(slot, name.as_bytes())?;
                self.models.insert(index, model);
                return Ok(index);
            }
        }
        Err(invalid("model configstring range exhausted"))
    }

    /// G_PrecacheWeapon registers the requested name but gives a missing
    /// weapon world model the geometry of xmodel/defaultweapon.
    pub fn weapon_world_index(
        &mut self,
        assets: &GameAssets,
        strings: &mut ConfigStrings,
        name: &str,
        initializing: bool,
    ) -> io::Result<u8> {
        let index = self.index(assets, strings, name, initializing)?;
        if self.get(index).is_some_and(|model| model.is_default) {
            let replacement = self.register(assets, "xmodel/defaultweapon")?;
            self.models.insert(index, replacement);
        }
        Ok(index)
    }

    fn register_bone_names(&mut self, model: &ServerModel) -> io::Result<()> {
        for bone in &model.parts.bones {
            if !bone.name.is_empty() && !self.bone_names.contains_key(&bone.name) {
                let handle = u16::try_from(self.bone_names.len() + 1)
                    .map_err(|_| invalid("model bone name table exhausted"))?;
                self.bone_names.insert(bone.name.clone(), handle);
            }
        }
        Ok(())
    }

    /// Interned name for the server's trace partName field. Zero denotes the unnamed part.
    pub fn bone_name_handle(&self, name: &str) -> u16 {
        self.bone_names.get(name).copied().unwrap_or(0)
    }

    pub fn bone_name(&self, handle: u16) -> Option<&str> {
        self.bone_names
            .iter()
            .find_map(|(name, id)| (*id == handle).then_some(name.as_str()))
    }

    /// XModel_GetBoneIndex walks names in reverse. Callers supply the text of the
    /// script string handle, which is already lowercased for script tag parameters.
    pub(crate) fn bone_index(&self, model_index: u8, tag: &[u8]) -> Option<usize> {
        self.get(model_index)?
            .parts
            .bones
            .iter()
            .rposition(|bone| !bone.name.is_empty() && bone.name.as_bytes() == tag)
    }

    pub fn get(&self, index: u8) -> Option<&ServerModel> {
        self.models.get(&index).map(Arc::as_ref)
    }
}

fn asset_path(prefix: &str, name: &str) -> String {
    let mut path = format!("{prefix}/{name}");
    path.truncate(path.floor_char_boundary(63));
    path
}

fn load_model(
    assets: &GameAssets,
    path: &str,
    cache: &mut BTreeMap<String, Arc<ModelParts>>,
) -> io::Result<ServerModel> {
    let (prefix, name) = path
        .split_once('/')
        .ok_or_else(|| invalid("bad model name"))?;
    if !prefix.eq_ignore_ascii_case("xmodel") {
        return Err(invalid("bad model name"));
    }
    let result = assets
        .read_file(&asset_path("xmodel", name))
        .and_then(|bytes| {
            ServerModel::parse(&bytes, |lod_name| {
                let key = lod_name.to_ascii_lowercase();
                if let Some(parts) = cache.get(&key) {
                    return Ok(Arc::clone(parts));
                }
                let parts = Arc::new(ModelParts::parse(
                    &assets.read_file(&asset_path("xmodelparts", lod_name))?,
                )?);
                cache.insert(key, Arc::clone(&parts));
                Ok(parts)
            })
        });
    match result {
        Err(error)
            if matches!(
                error.kind(),
                io::ErrorKind::NotFound | io::ErrorKind::Unsupported
            ) =>
        {
            eprintln!("Cannot load {path}: {error}; using default model");
            Ok(ServerModel::fallback())
        }
        other => other,
    }
}

impl GameAssets {
    pub fn load_model(&self, name: &str) -> io::Result<ServerModel> {
        load_model(self, name, &mut BTreeMap::new())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn parts_fixture() -> Vec<u8> {
        let mut bytes = Vec::from(20u16.to_le_bytes());
        bytes.extend_from_slice(&1u16.to_le_bytes());
        bytes.extend_from_slice(&1u16.to_le_bytes());
        bytes.push(0);
        for value in [1.0f32, 2., 3.] {
            bytes.extend_from_slice(&value.to_le_bytes());
        }
        bytes.extend_from_slice(&[0; 6]);
        bytes.extend_from_slice(b"root\0child\0");
        bytes.extend_from_slice(&[1, 3]);
        bytes
    }

    fn model_fixture() -> Vec<u8> {
        let mut bytes = Vec::from(20u16.to_le_bytes());
        bytes.push(3);
        for v in [-10.0f32, -10., -10., 10., 10., 10.] {
            bytes.extend_from_slice(&v.to_le_bytes());
        }
        for name in ["parts", "", "", ""] {
            bytes.extend_from_slice(&0f32.to_le_bytes());
            bytes.extend_from_slice(name.as_bytes());
            bytes.push(0);
        }
        bytes.extend_from_slice(&u32::MAX.to_le_bytes());
        bytes.extend_from_slice(&1u32.to_le_bytes());
        bytes.extend_from_slice(&1u32.to_le_bytes());
        bytes.extend_from_slice(&[0; 48 + 24]);
        bytes.extend_from_slice(&0u32.to_le_bytes());
        bytes.extend_from_slice(&0x20000005u32.to_le_bytes());
        bytes.extend_from_slice(&7u32.to_le_bytes());
        bytes.extend_from_slice(&1u16.to_le_bytes());
        bytes.extend_from_slice(b"material\0");
        for _ in 0..2 {
            for v in [0f32, 0., 0., 2., 4., 6.] {
                bytes.extend_from_slice(&v.to_le_bytes());
            }
        }
        bytes
    }

    #[test]
    fn decodes_header_collision_and_hierarchy() {
        let model = ServerModel::parse(&model_fixture(), |name| {
            assert_eq!(name, "parts");
            Ok(Arc::new(ModelParts::parse(&parts_fixture())?))
        })
        .expect("valid model");
        assert_eq!(model.flags, 3);
        assert_eq!(model.collision_lod, -1);
        assert_eq!(model.contents, 1);
        assert_eq!(model.collision[0].triangles.len(), 1);
        assert_eq!(model.collision[0].bounds, [[-0.001; 3], [0.001; 3]]);
        assert_eq!(model.lods[0].surfaces, ["material"]);
        let child = &model.parts.bones[1];
        assert_eq!(
            (&*child.name, child.parent, child.classification),
            ("child", Some(0), 3)
        );
        assert_eq!(child.local_quaternion, [0, 0, 0, 32767]);
        assert_eq!(model.bone_bounds[1].radius_squared, 14.);
        assert_eq!(model.bone_bounds[1].center, [1., 2., 3.]);
    }

    #[test]
    fn rejects_truncated_data_and_invalid_hierarchy() {
        let mut parts = parts_fixture();
        parts[6] = 1;
        assert_eq!(
            ModelParts::parse(&parts).expect_err("self parent").kind(),
            io::ErrorKind::InvalidData
        );
        let mut bad = model_fixture();
        bad.pop();
        assert!(
            ServerModel::parse(&bad, |_| Ok(Arc::new(ModelParts::parse(&parts_fixture())?)))
                .is_err()
        );
    }

    #[test]
    fn registry_mutates_configstrings_only_after_load_and_reuses_names() -> io::Result<()> {
        let dir = tempfile::tempdir()?;
        let main = dir.path().join("main");
        fs::create_dir_all(main.join("xmodel"))?;
        fs::create_dir_all(main.join("xmodelparts"))?;
        fs::write(main.join("xmodel/defaultweapon"), model_fixture())?;
        fs::write(main.join("xmodelparts/parts"), parts_fixture())?;
        fs::write(main.join("xmodel/broken"), [20, 0])?;
        let assets = GameAssets::mount_paths(dir.path(), None, "")?;
        let mut strings = ConfigStrings::default();
        let mut models = ModelRegistry::default();
        assert!(
            models
                .index(&assets, &mut strings, "invalid", true)
                .is_err()
        );
        assert!(
            models
                .index(&assets, &mut strings, "xmodel/broken", true)
                .is_err()
        );
        assert!(
            models
                .index(&assets, &mut strings, "xmodel/missing", false)
                .is_err()
        );
        assert_eq!(
            models.index(&assets, &mut strings, "xmodel/missing", true)?,
            1
        );
        assert!(models.get(1).expect("default model").is_default);
        assert_eq!(
            models.index(&assets, &mut strings, "XMODEL/MISSING", false)?,
            1
        );
        assert_eq!(
            models.weapon_world_index(&assets, &mut strings, "xmodel/missing", true)?,
            1
        );
        assert!(!models.get(1).expect("weapon geometry").is_default);
        assert_eq!(
            strings.get(MODEL_CONFIGSTRING_BASE + 1),
            Some(b"xmodel/missing".as_slice())
        );
        Ok(())
    }
}
