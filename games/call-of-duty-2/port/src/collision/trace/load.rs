use super::{
    Leaf, Vec3,
    brush::Brush,
    terrain::{Edge, Partition, Terrain, Tree, Triangle},
};
use crate::{
    assets::ServerMap,
    collision::{field, invalid},
};
use std::{io, ops::Range};

#[derive(Debug)]
pub(super) struct Material {
    pub flags: u32,
    pub contents: u32,
}

#[derive(Debug, Default)]
pub(in crate::collision) struct TraceGeometry {
    pub(super) brushes: Vec<Brush>,
    pub(super) materials: Vec<Material>,
    pub(super) leaves: Vec<Leaf>,
    pub(super) models: Vec<Leaf>,
    pub(super) terrain: Terrain,
}
fn records<const N: usize>(map: &ServerMap, lump: usize) -> io::Result<&[[u8; N]]> {
    let (rows, tail) = map
        .lump(lump)
        .ok_or_else(|| invalid("missing collision geometry lump"))?
        .as_chunks::<N>();
    if !tail.is_empty() {
        return Err(invalid("partial collision geometry record"));
    }
    Ok(rows)
}
fn short(rec: &[u8], off: usize) -> usize {
    usize::from(u16::from_le_bytes([rec[off], rec[off + 1]]))
}
fn number(rec: &[u8], off: usize) -> f32 {
    f32::from_bits(field(rec, off) as u32)
}
fn vector(rec: &[u8], off: usize) -> Vec3 {
    std::array::from_fn(|i| number(rec, off + 4 * i))
}
fn index(value: i32, size: usize) -> io::Result<usize> {
    let value = value as usize;
    if value >= size {
        Err(invalid("collision geometry index out of range"))
    } else {
        Ok(value)
    }
}
fn range(start: i32, count: usize, size: usize) -> io::Result<Range<usize>> {
    if count == 0 {
        return Ok(0..0);
    }
    let start = index(start, size)?;
    let end = start
        .checked_add(count)
        .filter(|&end| end <= size)
        .ok_or_else(|| invalid("invalid collision geometry range"))?;
    Ok(start..end)
}
fn optional_indices(rec: &[u8], off: usize, size: usize) -> io::Result<[Option<usize>; 3]> {
    let mut out = [None; 3];
    for (i, slot) in out.iter_mut().enumerate() {
        let value = field(rec, off + i * 4);
        if value >= 0 {
            *slot = Some(index(value, size)?);
        }
    }
    Ok(out)
}
impl TraceGeometry {
    pub(in crate::collision) fn load(map: &ServerMap, plane_count: usize) -> io::Result<Self> {
        let materials = records::<72>(map, 0)?
            .iter()
            .map(|r| Material {
                flags: field(r, 64) as u32,
                contents: field(r, 68) as u32,
            })
            .collect::<Vec<_>>();
        let mut side_rows = records::<8>(map, 5)?.iter();
        let mut brushes = Vec::new();
        for row in records::<4>(map, 6)? {
            let side_count = short(row, 0);
            let extra = side_count
                .checked_sub(6)
                .ok_or_else(|| invalid("brush has fewer than six sides"))?;
            let material = index(short(row, 2) as i32, materials.len())?;
            let mut bounds = [[0.; 3]; 2];
            let mut axial_material = [[0; 3]; 2];
            for axis in 0..3 {
                for group in 0..2 {
                    let r = side_rows
                        .next()
                        .ok_or_else(|| invalid("short axial brush side lump"))?;
                    bounds[group][axis] = number(r, 0);
                    axial_material[group][axis] = index(field(r, 4), materials.len())?;
                }
            }
            let mut sides = Vec::new();
            for _ in 0..extra {
                let r = side_rows
                    .next()
                    .ok_or_else(|| invalid("short brush side lump"))?;
                sides.push((
                    index(field(r, 0), plane_count)?,
                    index(field(r, 4), materials.len())?,
                ));
            }
            brushes.push(Brush {
                bounds,
                axial_material,
                sides,
                contents: materials[material].contents & 0xdfff_fffb,
            });
        }
        let leaf_brushes = records::<4>(map, 27)?
            .iter()
            .map(|r| index(field(r, 0), brushes.len()))
            .collect::<io::Result<Vec<_>>>()?;
        let vertices = records::<16>(map, 29)?
            .iter()
            .map(|r| vector(r, 4))
            .collect::<Vec<_>>();
        let edges = records::<56>(map, 30)?
            .iter()
            .map(|r| Edge {
                origin: vector(r, 4),
                u: vector(r, 16),
                v: vector(r, 28),
                direction: vector(r, 40).map(|n| n * (1. / number(r, 52))),
            })
            .collect::<Vec<_>>();
        let mut triangles = Vec::new();
        for r in records::<72>(map, 31)? {
            triangles.push(Triangle {
                planes: std::array::from_fn(|i| std::array::from_fn(|j| number(r, 16 * i + 4 * j))),
                vertices: optional_indices(r, 48, vertices.len())?,
                edges: optional_indices(r, 60, edges.len())?,
            });
        }
        let borders = records::<28>(map, 32)?
            .iter()
            .map(|r| std::array::from_fn(|i| number(r, i * 4)))
            .collect::<Vec<_>>();
        let mut partitions = Vec::new();
        for r in records::<12>(map, 33)? {
            partitions.push(Partition {
                triangles: range(field(r, 4), usize::from(r[2]), triangles.len())?,
                borders: range(field(r, 8), usize::from(r[3]), borders.len())?,
            });
        }
        let tree_rows = records::<32>(map, 34)?;
        let mut trees = Vec::new();
        for r in tree_rows {
            let count = short(r, 26);
            let children = range(field(r, 28), count, tree_rows.len())?;
            let partition = if count == 0 {
                Some(index(field(r, 28), partitions.len())?)
            } else {
                None
            };
            trees.push(Tree {
                origin: vector(r, 0),
                half: vector(r, 12),
                material: index(short(r, 24) as i32, materials.len())?,
                children,
                partition,
            });
        }
        // Shared children are legal. Back edges would prevent traversal from terminating.
        let mut state = vec![0; trees.len()];
        for root in 0..trees.len() {
            let mut stack = vec![(root, false)];
            while let Some((n, done)) = stack.pop() {
                if done {
                    state[n] = 2;
                    continue;
                }
                match state[n] {
                    1 => return Err(invalid("cycle in collision AABB tree")),
                    2 => continue,
                    _ => {}
                }
                state[n] = 1;
                stack.push((n, true));
                stack.extend(trees[n].children.clone().map(|child| (child, false)));
            }
        }
        let mut leaves = Vec::new();
        for r in records::<36>(map, 26)? {
            leaves.push(Leaf {
                brushes: super::brush_tree::BrushTree::build(
                    &leaf_brushes[range(field(r, 16), field(r, 20) as usize, leaf_brushes.len())?],
                    &brushes,
                )?,
                trees: range(field(r, 8), field(r, 12) as usize, trees.len())?,
            });
        }
        let mut models = Vec::new();
        for r in records::<48>(map, 35)?.iter().skip(1) {
            let indices =
                range(field(r, 40), field(r, 44) as usize, brushes.len())?.collect::<Vec<_>>();
            models.push(Leaf {
                brushes: super::brush_tree::BrushTree::build(&indices, &brushes)?,
                trees: range(field(r, 32), field(r, 36) as usize, trees.len())?,
            });
        }
        Ok(Self {
            materials,
            brushes,
            leaves,
            models,
            terrain: Terrain {
                vertices,
                edges,
                triangles,
                borders,
                partitions,
                trees,
            },
        })
    }
}
