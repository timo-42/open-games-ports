//! Brush position / sweep tests at 0x080594f6 and 0x08059dc0.
use super::{Plane, Sweep, TraceResult, Vec3, dot, load::Material};
#[derive(Debug)]
pub(super) struct Brush {
    pub bounds: [Vec3; 2],
    pub axial_material: [[usize; 3]; 2],
    pub sides: Vec<(usize, usize)>,
    pub contents: u32,
}
impl Brush {
    pub fn trace(
        &self,
        s: &Sweep,
        planes: &[Plane],
        materials: &[Material],
        hit: &mut TraceResult,
    ) {
        if s.sight {
            if self.blocks_sight(s, planes) {
                hit.fraction = 0.;
            }
            return;
        }
        let mut enter = 0.;
        let mut leave = hit.fraction;
        let mut end_inside = true;
        let mut side = None;
        let extents = [s.radius, s.radius, s.radius + s.offset_z];
        for group in 0..2 {
            let sign = if group == 0 { -1. } else { 1. };
            for (axis, &extent) in extents.iter().enumerate() {
                let corner = self.bounds[group][axis];
                let a = (s.start[axis] - corner) * sign - extent;
                let b = (s.end[axis] - corner) * sign - extent;
                if a > 0. {
                    if b >= a.min(0.125) {
                        return;
                    }
                    let t =
                        ((f64::from(a) - 0.125) * f64::from(s.inv[axis]) * f64::from(sign)) as f32;
                    if t >= leave {
                        return;
                    }
                    end_inside &= b <= 0.;
                    if t > enter || side.is_none() {
                        enter = enter.max(t);
                        let mut normal = [0.; 3];
                        normal[axis] = sign;
                        side = Some((normal, self.axial_material[group][axis]));
                    }
                } else if b > 0. {
                    let t = a * s.inv[axis] * sign;
                    if t <= enter {
                        return;
                    }
                    leave = leave.min(t);
                    end_inside = false;
                }
            }
        }
        for &(index, material) in &self.sides {
            let plane = &planes[index];
            let reach = (f64::from(plane.dist)
                + f64::from(s.radius)
                + f64::from((plane.normal[2] * s.offset_z).abs())) as f32;
            let a = (dot(s.start, plane.normal) - f64::from(reach)) as f32;
            let b = (dot(s.end, plane.normal) - f64::from(reach)) as f32;
            if a > 0. {
                if b >= a.min(0.125) {
                    return;
                }
                end_inside &= b <= 0.;
                let diff = a - b;
                if enter * diff < a - 0.125 {
                    enter = (a - 0.125) / diff;
                    if enter >= leave {
                        return;
                    }
                } else if side.is_some() {
                    continue;
                }
                side = Some((plane.normal, material));
            } else if b > 0. {
                let diff = a - b;
                if leave * diff < a {
                    leave = a / diff;
                    if leave <= enter {
                        return;
                    }
                }
                end_inside = false;
            }
        }
        hit.contents = self.contents;
        if let Some((normal, material)) = side {
            hit.normal = normal;
            hit.surface_flags = materials[material].flags;
            hit.material = Some(material);
            hit.fraction = enter;
        } else {
            hit.start_solid = true;
            if end_inside {
                hit.all_solid = true;
                hit.fraction = 0.;
            }
        }
    }

    // CM_TraceBrushBounds, 0x0805bbe6. Tangential entries and an exit at
    // t=0 are rejected. A segment entirely within the brush obstructs sight.
    fn blocks_sight(&self, s: &Sweep, planes: &[Plane]) -> bool {
        let mut enter = 0.0_f32;
        let mut leave = 1.0_f32;
        for (corner, sign) in self.bounds.iter().zip([-1., 1.]) {
            for (axis, &edge) in corner.iter().enumerate() {
                let a = (s.start[axis] - edge) * sign;
                let b = (s.end[axis] - edge) * sign;
                let t = a * s.inv[axis] * sign;
                if a > 0. {
                    if b > 0. || leave <= t {
                        return false;
                    }
                    enter = enter.max(t);
                } else if b > 0. {
                    if t <= enter {
                        return false;
                    }
                    leave = leave.min(t);
                }
            }
        }
        for &(index, _) in &self.sides {
            let plane = &planes[index];
            let a = (dot(s.start, plane.normal) - f64::from(plane.dist)) as f32;
            let b = (dot(s.end, plane.normal) - f64::from(plane.dist)) as f32;
            if a > 0. {
                if b > 0. {
                    return false;
                }
                if enter * (a - b) < a {
                    enter = a / (a - b);
                    if leave <= enter {
                        return false;
                    }
                }
            } else if b > 0. && leave * (a - b) < a {
                leave = a / (a - b);
                if leave <= enter {
                    return false;
                }
            }
        }
        true
    }
}
