//! Collision scene for BSP misc_model placements (CM_CreateStaticModel).
mod sectors;
use super::{CollisionMap, TraceResult, dobj, invalid};
use crate::assets::{GameAssets, ServerMap, models::ServerModel};
use crate::vm::{math::axes, spawn_fields::float_prefix};
pub(super) use sectors::Sectors;
use std::{collections::BTreeMap, io, sync::Arc};
type Vec3 = [f32; 3];
type Matrix = [Vec3; 3];

#[derive(Debug)]
pub(super) struct StaticModel {
    model: Arc<ServerModel>,
    origin: Vec3,
    inverse: Matrix,
    bounds: [Vec3; 2],
}

fn scan_vec(bytes: &[u8], out: &mut Vec3) {
    let mut rest = bytes;
    for slot in out {
        let Some((prefix, next)) = float_prefix(rest) else {
            break;
        };
        let Ok(value) = prefix.parse() else { break };
        *slot = value;
        rest = next;
    }
}
fn inverse(m: Matrix) -> Matrix {
    let [[a, b, c], [d, e, f], [g, h, i]] = m.map(|row| row.map(f64::from));
    let adj = [
        [i * e - h * f, -(i * b - h * c), b * f - c * e],
        [-(i * d - g * f), a * i - c * g, -(f * a - d * c)],
        [d * h - e * g, -(h * a - g * b), a * e - b * d],
    ];
    let det = (adj[0][0] * a + adj[0][1] * d + adj[0][2] * g) as f32;
    let inv = f64::from(1. / det);
    adj.map(|row| row.map(|n| (n * inv) as f32))
}
impl StaticModel {
    fn new(model: Arc<ServerModel>, origin: Vec3, angles: Vec3, scale: Vec3) -> Self {
        let [front, right, up] = axes(angles);
        let mut matrix = [front, right.map(|r| 0. - r), up];
        for (row, scale) in matrix.iter_mut().zip(scale) {
            *row = row.map(|v| v * scale);
        }
        let mut low = [f32::MAX; 3];
        let mut high = [f32::MIN; 3];
        for surface in &model.collision {
            for corner in 0..8 {
                let point = std::array::from_fn(|i| surface.bounds[(corner >> i) & 1][i]);
                let rotated = dobj::rotate_back(point, matrix);
                for i in 0..3 {
                    low[i] = low[i].min(rotated[i]);
                    high[i] = high[i].max(rotated[i]);
                }
            }
        }
        let bounds = [low, high].map(|v| std::array::from_fn(|i| v[i] + origin[i]));
        Self {
            model,
            origin,
            inverse: inverse(matrix),
            bounds,
        }
    }
    fn hit(
        &self,
        start: Vec3,
        end: Vec3,
        mask: u32,
        limit: f32,
    ) -> io::Result<Option<dobj::ModelHit<'_>>> {
        if self.model.contents & mask == 0 || !dobj::box_intersects(start, end, self.bounds, limit)
        {
            return Ok(None);
        }
        let transform = |point: Vec3| {
            dobj::rotate_back(
                std::array::from_fn(|i| point[i] - self.origin[i]),
                self.inverse,
            )
        };
        dobj::trace_model(
            &self.model,
            None,
            transform(start),
            transform(end),
            mask,
            limit,
        )
    }
}
impl CollisionMap {
    /// Load BSP geometry and all static XModel placements from the same VFS.
    pub fn load_with_assets(map: &ServerMap, assets: &GameAssets) -> io::Result<Self> {
        let mut scene = Self::load(map)?;
        let mut cache: BTreeMap<String, Arc<ServerModel>> = BTreeMap::new();
        for record in map.entities.records() {
            let mut misc = false;
            let mut path = &[][..];
            let mut origin = [0.; 3];
            let mut angles = [0.; 3];
            let mut scale = [1.; 3];
            for (key, value) in record.pairs() {
                if key.eq_ignore_ascii_case(b"classname") {
                    misc |= value.eq_ignore_ascii_case(b"misc_model");
                } else if key.eq_ignore_ascii_case(b"model") {
                    path = value;
                } else if key.eq_ignore_ascii_case(b"origin") {
                    scan_vec(value, &mut origin);
                } else if key.eq_ignore_ascii_case(b"angles") {
                    scan_vec(value, &mut angles);
                } else if key.eq_ignore_ascii_case(b"modelscale_vec") {
                    scan_vec(value, &mut scale);
                } else if key.eq_ignore_ascii_case(b"modelscale") {
                    scale = [float_prefix(value)
                        .and_then(|(s, _)| s.parse::<f64>().ok())
                        .unwrap_or(0.) as f32; 3];
                }
            }
            if !misc
                || !path
                    .get(..7)
                    .is_some_and(|s| s.eq_ignore_ascii_case(b"xmodel/"))
            {
                continue;
            }
            if scale.contains(&0.) {
                return Err(invalid("static model has a zero scale component"));
            }
            let name = std::str::from_utf8(path)
                .map_err(|_| invalid("invalid static model path"))?
                .to_ascii_lowercase();
            let model = match cache.entry(name) {
                std::collections::btree_map::Entry::Occupied(slot) => slot.get().clone(),
                std::collections::btree_map::Entry::Vacant(slot) => {
                    let model = Arc::new(assets.load_model(slot.key())?);
                    slot.insert(model).clone()
                }
            };
            if model.contents != 0 && !model.collision.is_empty() {
                scene
                    .static_models
                    .push(StaticModel::new(model, origin, angles, scale));
            }
        }
        if let Some(model) = scene.inline_model(0) {
            scene.static_sectors = Sectors::build(&scene.static_models, model.mins, model.maxs);
        }
        Ok(scene)
    }

    /// Collision_IsSegmentClear stops at the first static mesh hit. Each
    /// candidate is tested over the entire original segment.
    pub(super) fn static_models_clear(
        &self,
        start: Vec3,
        end: Vec3,
        mask: u32,
    ) -> io::Result<bool> {
        let mut clear = true;
        self.static_sectors.visit(start, end, 1., |index| {
            clear = self.static_models[index]
                .hit(start, end, mask, 1.)?
                .is_none();
            Ok(if clear { 1. } else { 0. })
        })?;
        Ok(clear)
    }

    /// Area_SweepBoxFromPoints tests static meshes only for locational traces
    /// with extraCheck enabled. The caller supplies the preceding BSP hit.
    pub fn clip_static_models(
        &self,
        start: Vec3,
        end: Vec3,
        mask: u32,
        hit: &mut TraceResult,
    ) -> io::Result<()> {
        self.static_sectors
            .visit(start, end, hit.fraction, |index| {
                let placement = &self.static_models[index];
                if let Some(contact) = placement.hit(start, end, mask, hit.fraction)? {
                    let normal = dobj::local(contact.normal, [0.; 3], placement.inverse);
                    let length = (super::trace::dot(normal, normal) as f32).sqrt();
                    hit.normal = if length == 0. {
                        normal
                    } else {
                        normal.map(|n| n * (1. / length))
                    };
                    hit.fraction = contact.fraction;
                    hit.surface_flags = contact.surface_flags;
                    hit.contents = contact.contents;
                    hit.start_solid = false;
                    hit.all_solid = false;
                    hit.entity = 1022;
                }
                Ok(hit.fraction)
            })
    }
}
