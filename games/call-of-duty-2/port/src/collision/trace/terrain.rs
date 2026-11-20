//! CM_TraceThroughAabbTree and the triangle, edge, vertex and border contacts.
use super::{Sweep, TraceResult, Vec3, Visits, add_scaled, dot, load::Material, sub};
use std::ops::Range;

#[derive(Debug, Default)]
pub(super) struct Terrain {
    pub vertices: Vec<Vec3>,
    pub edges: Vec<Edge>,
    pub triangles: Vec<Triangle>,
    pub borders: Vec<[f32; 7]>,
    pub partitions: Vec<Partition>,
    pub trees: Vec<Tree>,
}
#[derive(Debug)]
pub(super) struct Tree {
    pub origin: Vec3,
    pub half: Vec3,
    pub material: usize,
    pub children: Range<usize>,
    pub partition: Option<usize>,
}
#[derive(Debug)]
pub(super) struct Partition {
    pub triangles: Range<usize>,
    pub borders: Range<usize>,
}
#[derive(Debug)]
pub(super) struct Edge {
    pub origin: Vec3,
    pub u: Vec3,
    pub v: Vec3,
    pub direction: Vec3,
}
#[derive(Debug)]
pub(super) struct Triangle {
    pub planes: [[f32; 4]; 3],
    pub vertices: [Option<usize>; 3],
    pub edges: [Option<usize>; 3],
}
fn normal(p: [f32; 4]) -> Vec3 {
    [p[0], p[1], p[2]]
}
fn dist(p: [f32; 4], v: Vec3) -> f32 {
    (dot(normal(p), v) - f64::from(p[3])) as f32
}
fn length_squared(v: Vec3) -> f32 {
    dot(v, v) as f32
}
fn scaled(v: Vec3, s: f32) -> Vec3 {
    v.map(|n| n * s)
}
fn normalize(v: Vec3) -> Vec3 {
    let len = length_squared(v).sqrt();
    if len == 0. { v } else { scaled(v, 1. / len) }
}
fn region(u: f32, v: f32) -> u8 {
    u8::from(u + v > 1.) | (u8::from(u < 0.) << 1) | (u8::from(v < 0.) << 2)
}

impl Triangle {
    fn crosses_face(&self, pos: Vec3, d: f32, r: f32, offset: f32) -> bool {
        let nz = self.planes[0][2];
        let far = d + (2. * offset) * nz;
        if far <= -r {
            return false;
        }
        let u = dist(self.planes[1], pos);
        let v = dist(self.planes[2], pos);
        let inside = |z| region(u + z * self.planes[1][2], v + z * self.planes[2][2]) == 0;
        inside((-r - d) / nz) || inside(if far < r { 2. * offset } else { (r - d) / nz })
    }
    fn point(&self, s: &Sweep, h: &mut TraceResult) {
        let b = dist(self.planes[0], s.end);
        let a = dist(self.planes[0], s.start);
        if b >= 0. || a <= 0. {
            return;
        }
        let denominator = f64::from(a) - f64::from(b);
        let t = (((f64::from(a) - 0.125) / denominator) as f32).max(0.);
        if t >= h.fraction {
            return;
        }
        let point = add_scaled(s.start, (f64::from(a) / denominator) as f32, s.delta);
        let u = dist(self.planes[1], point);
        let v = dist(self.planes[2], point);
        if (-0.001..=1.001).contains(&u) && v >= -0.001 && u + v <= 1.001 {
            h.fraction = t;
            h.normal = normal(self.planes[0]);
        }
    }
}
impl Terrain {
    pub fn trace_tree(
        &self,
        root: usize,
        materials: &[Material],
        s: &Sweep,
        seen: &mut Visits,
        h: &mut TraceResult,
    ) {
        let material_idx = self.trees[root].material;
        let mat = &materials[material_idx];
        if mat.contents & s.mask == 0 {
            return;
        }
        let before = h.fraction;
        let mut stack = vec![root];
        while let Some(index) = stack.pop() {
            let node = &self.trees[index];
            if (0..3).any(|i| {
                s.start[i].min(s.end[i]) - s.extents[i] > node.origin[i] + node.half[i]
                    || s.start[i].max(s.end[i]) + s.extents[i] < node.origin[i] - node.half[i]
            }) {
                continue;
            }
            if let Some(part) = node.partition {
                if !seen.partitions.insert(part) {
                    continue;
                }
                let partition = &self.partitions[part];
                for tri in &self.triangles[partition.triangles.clone()] {
                    if s.stationary {
                        if self.contains(tri, s) {
                            h.all_solid = true;
                            h.start_solid = true;
                            h.fraction = 0.;
                        }
                    } else if s.point {
                        tri.point(s, h)
                    } else {
                        self.capsule(tri, s, s.offset_z, seen, h);
                        if tri.planes[0][2] < 0. {
                            self.capsule(tri, s, -s.offset_z, seen, h);
                        }
                    }
                }
                if s.offset_z != 0. && (s.delta[0] != 0. || s.delta[1] != 0.) {
                    for &wall in &self.borders[partition.borders.clone()] {
                        border(wall, s, h);
                    }
                }
            } else {
                stack.extend(node.children.clone().rev());
            }
        }
        if h.fraction < before {
            h.contents = mat.contents;
            h.surface_flags = mat.flags;
            h.material = Some(material_idx);
        }
    }
    fn contains(&self, t: &Triangle, s: &Sweep) -> bool {
        let n = normal(t.planes[0]);
        let offset = if n[2] < 0. { -s.offset_z } else { s.offset_z };
        let mut pos = s.start;
        pos[2] -= offset;
        let d = dist(t.planes[0], pos);
        if d >= s.radius {
            return false;
        }
        if d <= -s.radius {
            return t.crosses_face(pos, d, s.radius, offset);
        }
        let projected = add_scaled(pos, -d, n);
        let flags = region(dist(t.planes[1], projected), dist(t.planes[2], projected));
        if flags == 0 {
            return true;
        }
        for k in 0..3 {
            if flags & (1 << k) == 0 {
                if let Some(index) = t.vertices[k] {
                    let rel = sub(pos, self.vertices[index]);
                    if dot(rel, rel) < f64::from(s.radius).powi(2) {
                        return true;
                    }
                }
            } else if let Some(index) = t.edges[k] {
                let edge = &self.edges[index];
                let rel = sub(pos, edge.origin);
                let at = dot(rel, edge.direction) as f32;
                if (0.0..=1.).contains(&at) {
                    let separation =
                        add_scaled(rel, -at / length_squared(edge.direction), edge.direction);
                    if dot(separation, separation) < f64::from(s.radius).powi(2) {
                        return true;
                    }
                }
            }
        }
        false
    }
    fn capsule(
        &self,
        t: &Triangle,
        s: &Sweep,
        offset: f32,
        seen: &mut Visits,
        h: &mut TraceResult,
    ) {
        let n = normal(t.planes[0]);
        let radius = s.radius + 0.125;
        let mut pos = s.start;
        pos[2] -= offset;
        let mut end = s.end;
        end[2] -= offset;
        let b = dist(t.planes[0], end);
        if b >= radius {
            return;
        }
        let a = dist(t.planes[0], pos);
        let diff = a - b;
        if diff <= 0.0001 {
            return;
        }
        if a <= -radius {
            if t.crosses_face(pos, a, radius, offset) {
                h.normal = n;
                h.fraction = 0.;
                h.start_solid = true;
            }
            return;
        }
        let time = ((a - radius) / diff).max(0.);
        if time > h.fraction {
            return;
        }
        let contact = add_scaled(pos, time, s.delta);
        let flags = region(dist(t.planes[1], contact), dist(t.planes[2], contact));
        if flags == 0 {
            h.normal = n;
            h.fraction = time;
            h.start_solid |= a < s.radius;
            return;
        }
        for k in 0..3 {
            let candidate = if flags & (1 << k) == 0 {
                let Some(i) = t.vertices[k] else { continue };
                if !seen.vertices.insert(i) {
                    continue;
                }
                let rel = sub(pos, self.vertices[i]);
                contact_time(rel, s.delta, radius, h.fraction, true).map(|(time, len_sq)| {
                    let normal = if time == 0. {
                        normalize(rel)
                    } else {
                        scaled(add_scaled(rel, time, s.delta), 1. / radius)
                    };
                    (time, normal, len_sq)
                })
            } else {
                let Some(i) = t.edges[k] else { continue };
                if !seen.edges.insert(i) {
                    continue;
                }
                let edge = &self.edges[i];
                let rel = sub(pos, edge.origin);
                let projected = |v| [dot(v, edge.u) as f32, dot(v, edge.v) as f32, 0.];
                let origin = projected(rel);
                let delta = projected(s.delta);
                contact_time(origin, delta, radius, h.fraction, false).and_then(|(time, len_sq)| {
                    let at = dot(add_scaled(rel, time, s.delta), edge.direction) as f32;
                    if !(0.0..=1.).contains(&at) {
                        return None;
                    }
                    let pos = add_scaled(origin, time, delta);
                    let normal = if time == 0. {
                        normalize(add_scaled(scaled(edge.u, pos[0]), pos[1], edge.v))
                    } else {
                        let scale = |i: usize| {
                            ((f64::from(time) * f64::from(delta[i]) + f64::from(origin[i]))
                                / f64::from(radius)) as f32
                        };
                        add_scaled(scaled(edge.u, scale(0)), scale(1), edge.v)
                    };
                    Some((time, normal, len_sq))
                })
            };
            if let Some((time, normal, len_sq)) = candidate {
                h.normal = if n[2] >= 0.7 && (0.0..0.7).contains(&normal[2]) && s.delta[2] < 0. {
                    n
                } else {
                    normal
                };
                h.fraction = time;
                if time == 0. {
                    h.start_solid |= len_sq < s.radius * s.radius;
                    return;
                }
            }
        }
    }
}
fn contact_time(
    rel: Vec3,
    delta: Vec3,
    r: f32,
    maximum: f32,
    allow_tangent: bool,
) -> Option<(f32, f32)> {
    let projection = dot(rel, delta) as f32;
    if projection >= 0. {
        return None;
    }
    let len_sq = length_squared(rel);
    let c = (f64::from(len_sq) - f64::from(r).powi(2)) as f32;
    if c <= 0. {
        return Some((0., len_sq));
    }
    let a = length_squared(delta);
    let discriminant = (f64::from(projection).powi(2) - f64::from(a) * f64::from(c)) as f32;
    if discriminant < 0. || (!allow_tangent && discriminant == 0.) {
        return None;
    }
    let time = ((-f64::from(discriminant.sqrt()) - f64::from(projection)) / f64::from(a)) as f32;
    (time < maximum).then_some((time, len_sq))
}
fn border(w: [f32; 7], s: &Sweep, h: &mut TraceResult) {
    let [nx, ny, d, z, slope, start, length] = w;
    let normal = [nx, ny, 0.];
    let delta = dot(s.delta, normal) as f32;
    if delta >= 0. {
        return;
    }
    let radius = s.radius + 0.125;
    let mut t = (radius - (dot(s.start, normal) as f32 - d)) / delta;
    let len_sq = length_squared(s.delta);
    if t >= h.fraction || t * len_sq.sqrt() < -radius {
        return;
    }
    let mut pos = add_scaled(s.start, t, s.delta);
    let mut side = (ny * pos[0] - nx * pos[1]) - start;
    if side < 0. || side > length {
        side = side.clamp(0., length);
        let at = start + side;
        let rel = [
            s.start[0] - (nx * d + ny * at),
            s.start[1] - (ny * d - nx * at),
            0.,
        ];
        let proj = dot(s.delta, rel) as f32;
        if proj >= 0. {
            return;
        }
        let dist = length_squared(rel);
        let c = dist - radius * radius;
        if c < 0. {
            if (s.start[2] - (z + side * slope)).abs() <= s.offset_z {
                h.normal = normal;
                h.fraction = 0.;
                h.start_solid |= s.radius * s.radius > dist;
            }
            return;
        }
        let disc = proj * proj - len_sq * c;
        if disc < 0. {
            return;
        }
        t = (-proj - disc.sqrt()) / len_sq;
        if t <= 0. || t >= h.fraction {
            return;
        }
        pos = add_scaled(s.start, t, s.delta);
    } else {
        t = t.max(0.)
    }
    if (pos[2] - (z + side * slope)).abs() <= s.offset_z {
        h.normal = normal;
        h.fraction = t;
    }
}
