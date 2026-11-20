//! Dedicated-server BSP, collision geometry, static XModel placement, and visibility queries.
mod box_leafs;
mod contents;
pub mod dobj;
mod inline_model;
mod locational;
mod server_trace;
mod sight;
mod static_models;
mod trace;
pub use inline_model::InlineModel;
pub use trace::{TraceResult, trace_capsule};

use crate::assets::ServerMap;
use std::io;

#[derive(Debug)]
struct Plane {
    normal: [f32; 3],
    dist: f32,
    axis: Option<usize>,
}

#[derive(Debug)]
struct Node {
    plane: usize,
    children: [i16; 2],
}

#[derive(Debug)]
pub struct CollisionMap {
    static_models: Vec<static_models::StaticModel>,
    static_sectors: static_models::Sectors,
    geometry: trace::TraceGeometry,
    inline_models: Vec<InlineModel>,
    planes: Vec<Plane>,
    nodes: Vec<Node>,
    clusters: Vec<i16>,
    cluster_count: usize,
    row_bytes: usize,
    visibility: Vec<u8>,
}

fn invalid(message: &'static str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message)
}

fn field(record: &[u8], offset: usize) -> i32 {
    i32::from_le_bytes(
        record[offset..offset + 4]
            .try_into()
            .expect("field in fixed-size BSP record"),
    )
}

impl CollisionMap {
    pub fn load(map: &ServerMap) -> io::Result<Self> {
        let lump = |index| map.lump(index).ok_or_else(|| invalid("missing BSP lump"));
        let mut collision = Self::decode(lump(4)?, lump(25)?, lump(26)?, lump(36)?)?;
        collision.inline_models = inline_model::load(lump(35)?, lump(0)?, lump(6)?, lump(34)?)?;
        collision.geometry = trace::TraceGeometry::load(map, collision.planes.len())?;
        Ok(collision)
    }

    pub fn inline_model(&self, index: u16) -> Option<&InlineModel> {
        self.inline_models.get(usize::from(index))
    }

    fn decode(planes: &[u8], nodes: &[u8], leaves: &[u8], pvs: &[u8]) -> io::Result<Self> {
        for (bytes, stride) in [(planes, 16), (nodes, 36), (leaves, 36)] {
            if bytes.is_empty() || !bytes.len().is_multiple_of(stride) {
                return Err(invalid("invalid plane, node or leaf record count"));
            }
        }
        let planes: Vec<_> = planes
            .as_chunks::<16>()
            .0
            .iter()
            .map(|record| {
                let float = |offset| f32::from_bits(field(record, offset) as u32);
                let normal = [float(0), float(4), float(8)];
                Plane {
                    axis: normal.iter().position(|&x| x == 1.0),
                    normal,
                    dist: float(12),
                }
            })
            .collect();
        let clusters: Vec<i16> = leaves
            .as_chunks::<36>()
            .0
            .iter()
            .map(|record| {
                field(record, 0)
                    .try_into()
                    .map_err(|_| invalid("leaf cluster exceeds signed 16 bits"))
            })
            .collect::<io::Result<_>>()?;
        let node_count = nodes.len() / 36;
        let nodes: Vec<Node> = nodes
            .as_chunks::<36>()
            .0
            .iter()
            .map(|record| {
                let plane = field(record, 0) as usize;
                if plane >= planes.len() {
                    return Err(invalid("invalid BSP node plane index"));
                }
                let mut children = [0_i16; 2];
                for (index, child) in children.iter_mut().enumerate() {
                    *child = i16::try_from(field(record, 4 + index * 4))
                        .map_err(|_| invalid("node child exceeds signed 16 bits"))?;
                    if (*child >= 0 && *child as usize >= node_count)
                        || (*child < 0 && (-1 - i32::from(*child)) as usize >= clusters.len())
                    {
                        return Err(invalid("invalid BSP child index"));
                    }
                }
                Ok(Node { plane, children })
            })
            .collect::<io::Result<_>>()?;
        validate_tree(&nodes)?;

        let leaf_cluster_count = clusters
            .iter()
            .copied()
            .max()
            .map_or(0, |last| (i32::from(last) + 1).max(0) as usize);
        let (cluster_count, row_bytes, visibility) = if pvs.is_empty() {
            (leaf_cluster_count, 0, Vec::new())
        } else {
            if pvs.len() < 8 {
                return Err(invalid("short BSP visibility header"));
            }
            let count =
                usize::try_from(field(pvs, 0)).map_err(|_| invalid("negative cluster count"))?;
            let stride = usize::try_from(field(pvs, 4))
                .map_err(|_| invalid("negative visibility row size"))?;
            let available = &pvs[8..];
            if stride < count.div_ceil(8)
                || count < leaf_cluster_count
                || count
                    .checked_mul(stride)
                    .is_none_or(|bytes| bytes > available.len())
            {
                return Err(invalid("invalid BSP visibility dimensions"));
            }
            (count, stride, available.to_vec())
        };
        Ok(Self {
            inline_models: Vec::new(),
            geometry: Default::default(),
            static_models: Vec::new(),
            static_sectors: Default::default(),
            planes,
            nodes,
            clusters,
            cluster_count,
            row_bytes,
            visibility,
        })
    }

    /// CM_PointLeafnum_r (0x08058b90). Points exactly on a split use its front child.
    pub fn point_leaf(&self, point: [f32; 3]) -> usize {
        let mut child = 0_i16;
        while child >= 0 {
            let node = &self.nodes[child as usize];
            let plane = &self.planes[node.plane];
            let distance = if let Some(axis) = plane.axis {
                point[axis] - plane.dist
            } else {
                // The i386 dot-product accumulates x, y, z before the distance
                // is stored as f32. f64 retains the full f32 products.
                let [x, y, z] = plane.normal.map(f64::from);
                let [px, py, pz] = point.map(f64::from);
                (x * px + y * py + z * pz - f64::from(plane.dist)) as f32
            };
            child = node.children[usize::from(distance < 0.0)];
        }
        (-1 - i32::from(child)) as usize
    }

    pub fn leaf_cluster(&self, leaf: usize) -> Option<i16> {
        self.clusters.get(leaf).copied()
    }

    /// CM_ClusterPVS (0x08059186) selects row zero for an unknown observer
    /// cluster. Maps without a visibility lump have an all-visible row.
    pub fn cluster_visible(&self, from: i16, to: i16) -> bool {
        let Ok(to) = usize::try_from(to) else {
            return false;
        };
        if to >= self.cluster_count {
            return false;
        }
        if self.row_bytes == 0 {
            return true;
        }
        let row = usize::try_from(from)
            .ok()
            .filter(|&c| c < self.cluster_count)
            .unwrap_or(0);
        self.visibility[row * self.row_bytes + to / 8] & (1 << (to % 8)) != 0
    }
}

// Children may share subtrees. A back edge would prevent point classification
// from terminating, so reject it once when the map is loaded.
fn validate_tree(nodes: &[Node]) -> io::Result<()> {
    let mut state = vec![0u8; nodes.len()];
    let mut work = vec![(0usize, false)];
    while let Some((index, leaving)) = work.pop() {
        if leaving {
            state[index] = 2;
            continue;
        }
        match state[index] {
            1 => return Err(invalid("cycle in BSP node tree")),
            2 => continue,
            _ => {}
        }
        state[index] = 1;
        work.push((index, true));
        for &child in &nodes[index].children {
            if child >= 0 {
                work.push((child as usize, false));
            }
        }
    }
    Ok(())
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    fn record(words: &[i32], size: usize) -> Vec<u8> {
        let mut result: Vec<_> = words.iter().flat_map(|x| x.to_le_bytes()).collect();
        result.resize(size, 0);
        result
    }

    pub(crate) fn map(front: i32, back: i32, vis: &[u8]) -> io::Result<CollisionMap> {
        let normal = [0.5_f32, 0.5, 0.0, 5.0].map(|v| v.to_bits() as i32);
        let nodes = record(&[0, front, back], 36);
        let leaves = [record(&[0], 36), record(&[8], 36)].concat();
        CollisionMap::decode(&record(&normal, 16), &nodes, &leaves, vis)
    }

    #[test]
    fn plane_and_visibility_rows() {
        let mut vis = record(&[9, 2], 8);
        vis.resize(26, 0);
        vis[9] = 1;
        vis[24] = 1;
        let bsp = map(-1, -2, &vis).expect("valid BSP");
        assert_eq!(bsp.point_leaf([3.0, 7.0, 99.0]), 0);
        assert_eq!(bsp.point_leaf([3.0, 6.0, 99.0]), 1);
        assert_eq!(bsp.leaf_cluster(1), Some(8));
        assert!(bsp.cluster_visible(0, 8));
        assert!(bsp.cluster_visible(8, 0));
        assert!(!bsp.cluster_visible(8, 8));
        assert!(bsp.cluster_visible(-1, 8));
        assert!(!bsp.cluster_visible(0, -1));
        assert!(!bsp.cluster_visible(0, 9));
        assert!(map(-1, -2, &[]).expect("no PVS").cluster_visible(8, 8));
    }

    #[test]
    fn rejects_invalid_tree_and_truncated_vis() {
        assert!(map(0, -1, &[]).is_err());
        assert!(map(1, -1, &[]).is_err());
        assert!(map(-1, -3, &[]).is_err());
        assert!(map(-32769, -1, &[]).is_err());
        assert!(map(-1, -2, &record(&[9, 2], 25)).is_err());
        assert!(map(-1, -2, &record(&[8, 2], 24)).is_err());
    }
}
