//! CM_BoxTrace (0x0805b894). The moving bounds define a vertical capsule.
mod brush;
mod brush_tree;
mod capsule;
pub use capsule::trace_capsule;
mod load;
mod terrain;
use super::{CollisionMap, Plane};
pub(super) use load::TraceGeometry;
use std::collections::HashSet;
use std::ops::Range;
type Vec3 = [f32; 3];

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TraceResult {
    pub fraction: f32,
    pub normal: Vec3,
    pub surface_flags: u32,
    pub contents: u32,
    pub material: Option<usize>,
    pub entity: u16,
    pub part_name: u16,
    pub part_group: u16,
    pub all_solid: bool,
    pub start_solid: bool,
}
impl Default for TraceResult {
    fn default() -> Self {
        Self {
            fraction: 1.,
            normal: [0.; 3],
            surface_flags: 0,
            contents: 0,
            material: None,
            entity: 1023,
            part_name: 0,
            part_group: 0,
            all_solid: false,
            start_solid: false,
        }
    }
}
pub(super) fn dot(a: Vec3, b: Vec3) -> f64 {
    f64::from(a[0]) * f64::from(b[0])
        + f64::from(a[1]) * f64::from(b[1])
        + f64::from(a[2]) * f64::from(b[2])
}
fn sub(a: Vec3, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| a[i] - b[i])
}
fn add_scaled(a: Vec3, scale: f32, b: Vec3) -> Vec3 {
    std::array::from_fn(|i| (f64::from(a[i]) + f64::from(scale) * f64::from(b[i])) as f32)
}
#[derive(Debug)]
struct Sweep {
    start: Vec3,
    end: Vec3,
    delta: Vec3,
    extents: Vec3,
    inv: Vec3,
    radius: f32,
    offset_z: f32,
    mask: u32,
    sight: bool,
    stationary: bool,
    point: bool,
}
impl Sweep {
    fn new(start: Vec3, end: Vec3, mins: Vec3, maxs: Vec3, mask: u32) -> Self {
        let center: Vec3 = std::array::from_fn(|i| (mins[i] + maxs[i]) * 0.5);
        let extents = sub(maxs, center);
        let stationary = start == end;
        let start = std::array::from_fn(|i| start[i] + center[i]);
        let end = std::array::from_fn(|i| end[i] + center[i]);
        let delta = sub(end, start);
        let inv = delta.map(|v| if v == 0. { 0. } else { -1. / v });
        let radius = extents[0].min(extents[2]);
        Self {
            start,
            end,
            delta,
            extents,
            inv,
            radius,
            offset_z: extents[2] - radius,
            mask,
            stationary,
            sight: false,
            point: extents == [0.; 3] && !stationary,
        }
    }
}

#[derive(Default)]
struct Visits {
    partitions: HashSet<usize>,
    edges: HashSet<usize>,
    vertices: HashSet<usize>,
}
#[derive(Debug)]
struct Leaf {
    brushes: brush_tree::BrushTree,
    trees: Range<usize>,
}
impl CollisionMap {
    /// Original CM_PointContents. The world selects one BSP leaf, and an
    /// inline model selects its embedded leaf. Terrain triangles have no volume.
    pub fn model_point_contents(&self, model: u16, point: Vec3) -> u32 {
        let leaf = if model == 0 {
            self.geometry.leaves.get(self.point_leaf(point))
        } else {
            self.geometry.models.get(usize::from(model) - 1)
        };
        leaf.map_or(0, |leaf| {
            leaf.brushes
                .point_contents(point, &self.geometry.brushes, &self.planes)
        })
    }
    /// Collision against the BSP world, excluding independently linked game entities.
    pub fn trace(&self, start: Vec3, end: Vec3, mins: Vec3, maxs: Vec3, mask: u32) -> TraceResult {
        self.trace_world(Sweep::new(start, end, mins, maxs, mask))
    }
    fn trace_world(&self, sweep: Sweep) -> TraceResult {
        let mut hit = TraceResult::default();
        let mut seen = Visits::default();
        let mut remaining_position_leaves = 1024;
        let mut stack = vec![(0_i16, sweep.start, sweep.end, 0., 1.)];
        while let Some((child, start, end, t0, t1)) = stack.pop() {
            if (sweep.sight && hit.fraction < 1.)
                || hit.fraction == 0.
                || remaining_position_leaves == 0
            {
                break;
            }
            if child < 0 {
                if sweep.stationary {
                    remaining_position_leaves -= 1;
                }
                if let Some(leaf) = self.geometry.leaves.get((!child) as usize) {
                    self.geometry
                        .trace_leaf(leaf, &sweep, &self.planes, &mut seen, &mut hit);
                }
                continue;
            }
            let node = &self.nodes[child as usize];
            let plane = &self.planes[node.plane];
            if sweep.stationary {
                let low = std::array::from_fn(|i| sweep.start[i] - sweep.extents[i] - 1.);
                let high = std::array::from_fn(|i| sweep.start[i] + sweep.extents[i] + 1.);
                let sides = plane.box_sides(low, high);
                if sides != 1 {
                    stack.push((node.children[1], start, end, t0, t1));
                }
                if sides != 2 {
                    stack.push((node.children[0], start, end, t0, t1));
                }
                continue;
            }
            let signed = |p: Vec3| match plane.axis {
                Some(i) => p[i] - plane.dist,
                None => (dot(p, plane.normal) - f64::from(plane.dist)) as f32,
            };
            let a = signed(start);
            let b = signed(end);
            let reach = match plane.axis {
                Some(i) => sweep.extents[i] + 0.125,
                None if sweep.point => 0.125,
                None => 2048.,
            };
            if a.min(b) >= reach {
                stack.push((node.children[0], start, end, t0, t1));
            } else if a.max(b) <= -reach {
                stack.push((node.children[1], start, end, t0, t1));
            } else if hit.fraction > t0 {
                let (first, near, far) = brush_tree::split_segment(a, b, reach);
                let point = |scale| add_scaled(start, scale, sub(end, start));
                stack.push((
                    node.children[1 - first],
                    point(near),
                    end,
                    t0 + (t1 - t0) * near,
                    t1,
                ));
                stack.push((
                    node.children[first],
                    start,
                    point(far),
                    t0,
                    t0 + (t1 - t0) * far,
                ));
            }
        }
        if hit.fraction < 1. {
            hit.entity = 1022;
        }
        hit
    }
    /// CM_TraceLeafItems uses the unpadded brush predicate for sight queries,
    /// including when both input points coincide.
    pub(super) fn bsp_point_clear(
        &self,
        model: u16,
        start: Vec3,
        end: Vec3,
        mask: u32,
    ) -> Option<bool> {
        let mut sweep = Sweep::new(start, end, [0.; 3], [0.; 3], mask);
        sweep.sight = true;
        sweep.stationary = false;
        sweep.point = true;
        if model == 0 {
            return Some(self.trace_world(sweep).fraction == 1.);
        }
        let leaf = self.geometry.models.get(usize::from(model) - 1)?;
        let mut result = TraceResult::default();
        self.geometry.trace_leaf(
            leaf,
            &sweep,
            &self.planes,
            &mut Visits::default(),
            &mut result,
        );
        Some(result.fraction == 1.)
    }
    /// Trace in an inline BSP model's local coordinates. Model zero is the world.
    /// SV assigns the linked entity number after transforming the result normal.
    pub fn trace_model(
        &self,
        model: u16,
        start: Vec3,
        end: Vec3,
        mins: Vec3,
        maxs: Vec3,
        mask: u32,
    ) -> Option<TraceResult> {
        if model == 0 {
            return Some(self.trace(start, end, mins, maxs, mask));
        }
        let leaf = self.geometry.models.get(usize::from(model) - 1)?;
        let mut hit = TraceResult::default();
        self.geometry.trace_leaf(
            leaf,
            &Sweep::new(start, end, mins, maxs, mask),
            &self.planes,
            &mut Visits::default(),
            &mut hit,
        );
        Some(hit)
    }
}
impl TraceGeometry {
    fn trace_leaf(
        &self,
        leaf: &Leaf,
        sweep: &Sweep,
        planes: &[Plane],
        seen: &mut Visits,
        hit: &mut TraceResult,
    ) {
        leaf.brushes
            .trace(sweep, &self.brushes, planes, &self.materials, hit);
        if hit.fraction == 0. {
            return;
        }
        for index in leaf.trees.clone() {
            self.terrain
                .trace_tree(index, &self.materials, sweep, seen, hit);
            if hit.fraction == 0. || (sweep.sight && hit.fraction < 1.) {
                return;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::collision::Node;
    use brush::Brush;
    use brush_tree::BrushTree;
    use load::Material;

    fn fixture() -> CollisionMap {
        let incline = Plane {
            normal: [0.6, 0., 0.8],
            dist: 6.,
            axis: None,
        };
        let brushes = vec![Brush {
            bounds: [[-10., -10., -10.], [10., 10., 10.]],
            axial_material: [[0; 3], [0; 3]],
            sides: vec![(0, 1)],
            contents: 17,
        }];
        let leaf = || Leaf {
            brushes: BrushTree::build(&[0], &brushes).expect("single fixture brush"),
            trees: 0..0,
        };
        let leaves = vec![leaf()];
        let models = vec![leaf()];
        CollisionMap {
            static_models: Vec::new(),
            static_sectors: Default::default(),
            geometry: TraceGeometry {
                materials: vec![
                    Material {
                        flags: 3,
                        contents: 17,
                    },
                    Material {
                        flags: 5,
                        contents: 17,
                    },
                ],
                brushes,
                leaves,
                models,
                ..Default::default()
            },
            planes: vec![incline],
            nodes: vec![Node {
                plane: 0,
                children: [-1, -1],
            }],
            inline_models: Vec::new(),
            clusters: vec![0],
            cluster_count: 1,
            row_bytes: 0,
            visibility: Vec::new(),
        }
    }
    #[test]
    fn box_trace_reports_surface_and_stationary_overlap() {
        let bsp = fixture();
        let ray = bsp.trace([0., 15., 0.], [0., -15., 0.], [0.; 3], [0.; 3], 1);
        assert_eq!(ray.fraction, 4.875 * (1. / 30.));
        assert_eq!(
            (
                ray.normal,
                ray.material,
                ray.surface_flags,
                ray.contents,
                ray.entity
            ),
            ([0., 1., 0.], Some(0), 3, 17, 1022)
        );
        let hull = bsp.trace(
            [0., 15., 0.],
            [0., -15., 0.],
            [-2., -2., -4.],
            [2., 2., 4.],
            1,
        );
        assert_eq!(hull.fraction, 2.875 * (1. / 30.));
        let pos = bsp.trace([0.; 3], [0.; 3], [-2.; 3], [2.; 3], 1);
        assert!(pos.start_solid && pos.all_solid);
        assert_eq!(pos.fraction, 0.);
        let escape = bsp.trace([0.; 3], [0., 20., 0.], [-2.; 3], [2.; 3], 1);
        assert!(escape.start_solid && !escape.all_solid);
        assert_eq!(escape.fraction, 1.);
        assert_eq!(
            bsp.trace([0.; 3], [0.; 3], [-2.; 3], [2.; 3], 2),
            TraceResult::default()
        );
    }
    #[test]
    fn brush_sight_observes_strict_entry_and_does_not_pad_hits() {
        let bsp = fixture();
        let clear = |a, b| bsp.bsp_point_clear(0, a, b, 1).expect("world model");
        assert!(clear([0., 20., 0.], [0., 10.05, 0.]));
        assert!(clear([0., 20., 0.], [0., 10., 0.]));
        assert!(!clear([0., 20., 0.], [0., 9.95, 0.]));
        assert!(clear([0., 10., 0.], [0., 20., 0.]));
        assert!(!clear([0.; 3], [0., 20., 0.]));
        assert!(!clear([0.; 3], [0.; 3]));
        assert_eq!(bsp.bsp_point_clear(1, [0.; 3], [0.; 3], 1), Some(false));
    }
    #[test]
    fn point_contents_includes_brush_faces_and_checks_the_sloping_plane() {
        let mut bsp = fixture();
        assert_eq!(bsp.model_point_contents(0, [0., 10., 0.]), 17);
        assert_eq!(bsp.model_point_contents(0, [0., 10.01, 0.]), 0);
        assert_eq!(bsp.model_point_contents(1, [10., 0., 1.]), 0);
        bsp.geometry.brushes.push(Brush {
            bounds: [[-1.; 3], [1.; 3]],
            axial_material: [[0; 3]; 2],
            sides: Vec::new(),
            contents: 0x8000_0020,
        });
        bsp.geometry.leaves[0].brushes =
            BrushTree::build(&[0, 1], &bsp.geometry.brushes).expect("overlapping brushes");
        let entities = crate::vm::game_entities::GameEntities::default();
        assert_eq!(bsp.model_point_contents(0, [0.; 3]), 0x8000_0031);
        assert_eq!(
            bsp.point_contents(&entities, [0.; 3], 1023, 0x8000_0020),
            0x8000_0020
        );
    }
    #[test]
    fn sloping_brush_and_inline_model_use_capsule_support() {
        let bsp = fixture();
        let a = [0., 0., 25.];
        let b = [0., 0., 0.];
        let impact = bsp
            .trace_model(1, a, b, [-2., -2., -4.], [2., 2., 4.], 1)
            .expect("trace against inline fixture model");
        assert_eq!(
            (impact.normal, impact.material, impact.surface_flags),
            ([0.6, 0., 0.8], Some(1), 5)
        );
        // Support along the sloping plane is the sphere radius plus the
        // projection of the vertical segment connecting the two cap centers.
        let expected = (20. - (6. + 2. + 2. * 0.8) - 0.125) / 20.;
        assert!((impact.fraction - expected).abs() < 0.000001);
        assert!(bsp.trace_model(2, a, b, [0.; 3], [0.; 3], 1).is_none());
    }
}
