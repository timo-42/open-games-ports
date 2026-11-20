//! Area_LinkBoxRecord / Area_LinkIntoSubtree insert sorted, one-based box
//! lists. A later insertion can push an earlier box one sector deeper.
use super::{StaticModel, Vec3};
type Bounds = [[f32; 2]; 2];
#[derive(Debug)]
struct Node {
    axis: usize,
    dist: f32,
    children: [Option<usize>; 2],
    list: Vec<usize>,
}
impl Node {
    fn new(bounds: Bounds) -> Self {
        let extent: [f32; 2] = std::array::from_fn(|i| bounds[1][i] - bounds[0][i]);
        let axis = usize::from(extent[0] <= extent[1]);
        Self {
            axis,
            dist: (bounds[0][axis] + bounds[1][axis]) * 0.5,
            children: [None; 2],
            list: Vec::new(),
        }
    }
    fn side(&self, model: &StaticModel) -> Option<usize> {
        if self.dist < model.bounds[0][self.axis] {
            Some(0)
        } else if self.dist > model.bounds[1][self.axis] {
            Some(1)
        } else {
            None
        }
    }
    fn insert(&mut self, index: usize) {
        let at = self.list.partition_point(|i| *i < index);
        self.list.insert(at, index);
    }
}
#[derive(Debug, Default)]
pub(in crate::collision) struct Sectors(Vec<Node>);
impl Sectors {
    pub fn build(models: &[StaticModel], low: Vec3, high: Vec3) -> Self {
        let bounds = [[low[0], low[1]], [high[0], high[1]]];
        let mut nodes = vec![Node::new(bounds)];
        for (index, model) in models.iter().enumerate() {
            let mut id = 0;
            let mut region = bounds;
            loop {
                let node = &nodes[id];
                let Some(side) = node.side(model) else { break };
                region[side][node.axis] = node.dist;
                let Some(child) = node.children[side] else {
                    break;
                };
                id = child;
            }
            nodes[id].insert(index);
            for old in std::mem::take(&mut nodes[id].list) {
                let child = nodes[id].side(&models[old]).and_then(|side| {
                    if let Some(child) = nodes[id].children[side] {
                        return Some(child);
                    }
                    if nodes.len() >= 1023 {
                        return None;
                    }
                    let next = Node::new(region);
                    if region[1][next.axis] - region[0][next.axis] <= 512. {
                        return None;
                    }
                    let child = nodes.len();
                    nodes[id].children[side] = Some(child);
                    nodes.push(next);
                    Some(child)
                });
                if let Some(child) = child {
                    nodes[child].insert(old);
                } else {
                    nodes[id].list.push(old);
                }
            }
        }
        Self(nodes)
    }
    /// Traverses each node list before descending the near side, then the far side.
    pub fn visit(
        &self,
        start: Vec3,
        end: Vec3,
        mut limit: f32,
        mut visit: impl FnMut(usize) -> std::io::Result<f32>,
    ) -> std::io::Result<()> {
        if self.0.is_empty() {
            return Ok(());
        }
        let mut stack = vec![(0, start, end, 0., limit)];
        while let Some((id, a, b, t0, t1)) = stack.pop() {
            let node = &self.0[id];
            for &model in &node.list {
                limit = visit(model)?;
                if limit == 0. {
                    return Ok(());
                }
            }
            let left = a[node.axis] - node.dist;
            let right = b[node.axis] - node.dist;
            if left * right < 0. {
                if limit <= t0 {
                    continue;
                }
                let t = left / (left - right);
                let mid = std::array::from_fn(|i| {
                    (f64::from(a[i]) + (f64::from(b[i]) - f64::from(a[i])) * f64::from(t)) as f32
                });
                let near = usize::from(right >= 0.);
                let cut = (f64::from(t0) + (f64::from(t1) - f64::from(t0)) * f64::from(t)) as f32;
                if let Some(child) = node.children[1 - near] {
                    stack.push((child, mid, b, cut, t1));
                }
                if let Some(child) = node.children[near] {
                    stack.push((child, a, mid, t0, cut));
                }
            } else if let Some(child) = node.children[usize::from(left.min(right) < 0.)] {
                stack.push((child, a, b, t0, t1));
            }
        }
        Ok(())
    }
}
