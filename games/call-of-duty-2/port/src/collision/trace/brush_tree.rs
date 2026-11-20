//! Leaf-local brush order affects hit materials at coincident planes.
use super::{Plane, Sweep, TraceResult, Vec3, brush::Brush, load::Material};
use crate::collision::invalid;
use std::io;

#[derive(Debug)]
pub(super) struct BrushTree {
    bounds: [Vec3; 2],
    nodes: Vec<Node>,
}
#[derive(Debug)]
enum Node {
    List(Vec<usize>),
    Split {
        axis: usize,
        dist: f32,
        gap: f32,
        middle: Option<usize>,
        children: [usize; 2],
    },
}
impl BrushTree {
    pub fn build(indices: &[usize], brushes: &[Brush]) -> io::Result<Self> {
        let mut bounds = [[f32::MAX; 3], [f32::MIN; 3]];
        for &index in indices {
            let brush = &brushes[index];
            let [low, high] = &mut bounds;
            for (axis, (lo, hi)) in low.iter_mut().zip(high).enumerate() {
                *lo = lo.min(brush.bounds[0][axis]);
                *hi = hi.max(brush.bounds[1][axis]);
            }
        }
        let mut tree = Self {
            bounds,
            nodes: Vec::new(),
        };
        tree.split(indices, brushes, bounds, 0)?;
        Ok(tree)
    }
    fn split(
        &mut self,
        indices: &[usize],
        brushes: &[Brush],
        bounds: [Vec3; 2],
        depth: u8,
    ) -> io::Result<usize> {
        let mut best_score = 0.;
        let mut choice = None;
        if depth >= 128 {
            return Err(invalid("brush tree depth exceeds map limits"));
        }
        if indices.len() >= 4 {
            for axis in 0..3 {
                let (mut lows, mut highs): (Vec<_>, Vec<_>) = indices
                    .iter()
                    .map(|&i| (brushes[i].bounds[0][axis], brushes[i].bounds[1][axis]))
                    .unzip();
                lows.sort_unstable_by(f32::total_cmp);
                highs.sort_unstable_by(f32::total_cmp);
                for &i in indices {
                    for side in brushes[i].bounds {
                        let at = side[axis];
                        let hi = lows.partition_point(|x| *x < at);
                        let lo = highs.partition_point(|x| *x <= at);
                        let n = lo.min(indices.len() - hi);
                        if n < 2 {
                            continue;
                        }
                        let upper = lows[hi];
                        let lower = highs[lo - 1];
                        let score =
                            (upper - bounds[0][axis]).min(bounds[1][axis] - lower) * (n - 1) as f32;
                        if score > best_score {
                            choice = Some((axis, (lower + upper) * 0.5));
                            best_score = score;
                        }
                    }
                }
            }
        }
        let id = self.nodes.len();
        if let Some((axis, dist)) = choice {
            let mut middle = Vec::new();
            let mut high = Vec::new();
            let mut low = Vec::new();
            let mut high_gap = f32::MAX;
            let mut low_gap = f32::MAX;
            for &index in indices {
                let [a, b] = brushes[index].bounds;
                if dist <= a[axis] {
                    high.push(index);
                    high_gap = high_gap.min(a[axis] - dist);
                } else if b[axis] <= dist {
                    low.push(index);
                    low_gap = low_gap.min(dist - b[axis]);
                } else {
                    middle.push(index);
                }
            }
            if high.len() < 2 || low.len() < 2 {
                self.nodes.push(Node::List(indices.to_vec()));
                return Ok(id);
            }
            self.nodes.push(Node::List(Vec::new()));
            let middle = if middle.is_empty() {
                None
            } else {
                Some(self.split(&middle, brushes, bounds, depth + 1)?)
            };
            let mut max_bounds = bounds;
            max_bounds[0][axis] = dist + high_gap;
            let gap = low_gap.min(high_gap);
            let mut min_bounds = bounds;
            min_bounds[1][axis] = dist - gap;
            let children = [
                self.split(&high, brushes, max_bounds, depth + 1)?,
                self.split(&low, brushes, min_bounds, depth + 1)?,
            ];
            self.nodes[id] = Node::Split {
                axis,
                dist,
                gap,
                middle,
                children,
            };
        } else {
            self.nodes.push(Node::List(indices.to_vec()));
        }
        Ok(id)
    }
    pub fn trace(
        &self,
        s: &Sweep,
        brushes: &[Brush],
        planes: &[Plane],
        materials: &[Material],
        h: &mut TraceResult,
    ) {
        if !self.overlaps(s, h.fraction) {
            return;
        }
        // Each clipped segment carries its own fourth (fraction) component.
        let mut stack = vec![(0, s.start, s.end, 0., h.fraction, false)];
        while let Some((index, start, end, t0, t1, after_middle)) = stack.pop() {
            match &self.nodes[index] {
                Node::List(indices) => {
                    for &i in indices {
                        let brush = &brushes[i];
                        if brush.contents & s.mask != 0 {
                            brush.trace(s, planes, materials, h);
                            if (s.sight && h.fraction == 0.) || (s.stationary && h.all_solid) {
                                return;
                            }
                        }
                    }
                }
                Node::Split {
                    axis,
                    dist,
                    gap,
                    middle,
                    children,
                } => {
                    if !after_middle && let Some(i) = middle {
                        stack.push((index, start, end, t0, t1, true));
                        stack.push((*i, start, end, t0, t1, false));
                        continue;
                    }
                    let axis = *axis;
                    let dist = *dist;
                    let a = start[axis] - dist;
                    let b = end[axis] - dist;
                    if s.stationary {
                        if a - s.extents[axis] <= 0. {
                            stack.push((children[1], start, end, t0, t1, false));
                        }
                        if a + s.extents[axis] >= 0. {
                            stack.push((children[0], start, end, t0, t1, false));
                        }
                    } else {
                        let reach = s.extents[axis] + 0.125 - gap;
                        if a.min(b) >= reach {
                            if a.max(b) > -reach {
                                stack.push((children[0], start, end, t0, t1, false));
                            }
                        } else if a.max(b) <= -reach {
                            stack.push((children[1], start, end, t0, t1, false));
                        } else if h.fraction > t0 {
                            let (first, near, far) = split_segment(a, b, reach);
                            let point =
                                |scale| super::add_scaled(start, scale, super::sub(end, start));
                            stack.push((
                                children[1 - first],
                                point(near),
                                end,
                                t0 + (t1 - t0) * near,
                                t1,
                                false,
                            ));
                            stack.push((
                                children[first],
                                start,
                                point(far),
                                t0,
                                t0 + (t1 - t0) * far,
                                false,
                            ));
                        }
                    }
                }
            }
        }
    }
    /// CM_PointContents tests strict padded leaf bounds, then walks one
    /// side of each KD plane along with any overlapping brush list.
    pub(super) fn point_contents(&self, p: Vec3, brushes: &[Brush], planes: &[Plane]) -> u32 {
        if (0..3).any(|i| p[i] <= self.bounds[0][i] - 0.125 || p[i] >= self.bounds[1][i] + 0.125) {
            return 0;
        }
        let mut contents = 0;
        let mut stack = vec![0];
        while let Some(index) = stack.pop() {
            match &self.nodes[index] {
                Node::List(indices) => {
                    for &index in indices {
                        let brush = &brushes[index];
                        if (0..3).all(|i| brush.bounds[0][i] <= p[i] && p[i] <= brush.bounds[1][i])
                            && brush.sides.iter().all(|&(plane, _)| {
                                let plane = &planes[plane];
                                super::dot(p, plane.normal) <= f64::from(plane.dist)
                            })
                        {
                            contents |= brush.contents;
                        }
                    }
                }
                Node::Split {
                    axis,
                    dist,
                    middle,
                    children,
                    ..
                } => {
                    stack.extend(*middle);
                    stack.push(children[usize::from(p[*axis] <= *dist)]);
                }
            }
        }
        contents
    }
    fn overlaps(&self, s: &Sweep, fraction: f32) -> bool {
        let mut enter = 0.0_f32;
        let mut leave = fraction;
        for group in 0..2 {
            let sign = if group == 0 { -1. } else { 1. };
            for i in 0..3 {
                let corner = self.bounds[group][i] + sign * (s.extents[i] + 0.125);
                let a = (s.start[i] - corner) * sign;
                let b = (s.end[i] - corner) * sign;
                if a > 0. {
                    if b > 0. {
                        return false;
                    }
                    let t = a * s.inv[i] * sign;
                    if leave <= t {
                        return false;
                    }
                    enter = enter.max(t);
                } else if b > 0. {
                    let t = a * s.inv[i] * sign;
                    if t <= enter {
                        return false;
                    }
                    leave = leave.min(t);
                }
            }
        }
        true
    }
}
pub(super) fn split_segment(a: f32, b: f32, reach: f32) -> (usize, f32, f32) {
    let distance = (b - a).abs();
    if distance > 4.7683716e-7 {
        let side = usize::from(b > a);
        let d = if side == 1 { -a } else { a };
        let inv = 1. / distance;
        (
            side,
            ((d - reach) * inv).max(0.),
            ((d + reach) * inv).min(1.),
        )
    } else {
        (0, 0., 1.)
    }
}
