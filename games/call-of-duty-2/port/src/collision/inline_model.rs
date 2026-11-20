//! CMod_LoadSubmodels and CMod_LoadSubmodelBrushNodes.
use super::{field, invalid};
use std::io;

#[derive(Clone, Copy, Debug)]
pub struct InlineModel {
    pub mins: [f32; 3],
    pub maxs: [f32; 3],
    /// Union of the brush and collision-AABB material contents.
    pub contents: u32,
}

pub(super) fn load(
    models: &[u8],
    materials: &[u8],
    brushes: &[u8],
    trees: &[u8],
) -> io::Result<Vec<InlineModel>> {
    let (models, remainder) = models.as_chunks::<48>();
    if !remainder.is_empty() || models.is_empty() || models.len() > 1023 {
        return Err(invalid("invalid collision submodel count"));
    }
    let (materials, rem_m) = materials.as_chunks::<72>();
    let (brushes, rem_b) = brushes.as_chunks::<4>();
    let (trees, rem_t) = trees.as_chunks::<32>();
    if materials.is_empty() || !rem_m.is_empty() || !rem_b.is_empty() || !rem_t.is_empty() {
        return Err(invalid("invalid submodel collision lump size"));
    }
    let material_contents = |bytes: [u8; 2]| -> io::Result<u32> {
        let index = usize::from(u16::from_le_bytes(bytes));
        materials
            .get(index)
            .map(|record| field(record, 0x44) as u32)
            .ok_or_else(|| invalid("invalid submodel material index"))
    };
    let mut result = Vec::with_capacity(models.len());
    for (id, record) in models.iter().enumerate() {
        let mins = std::array::from_fn(|i| f32::from_bits(field(record, 4 * i) as u32) - 1.);
        let maxs = std::array::from_fn(|i| f32::from_bits(field(record, 12 + 4 * i) as u32) + 1.);
        let mut contents = 0;
        if id != 0 {
            let range = |offset| -> io::Result<std::ops::Range<usize>> {
                let first = usize::try_from(field(record, offset))
                    .map_err(|_| invalid("negative submodel index"))?;
                let count = usize::try_from(field(record, offset + 4))
                    .map_err(|_| invalid("negative submodel count"))?;
                let end = first
                    .checked_add(count)
                    .ok_or_else(|| invalid("submodel range overflow"))?;
                Ok(first..end)
            };
            for brush in brushes
                .get(range(0x28)?)
                .ok_or_else(|| invalid("invalid submodel brush range"))?
            {
                contents |= material_contents([brush[2], brush[3]])? & 0xdfff_fffb;
            }
            for tree in trees
                .get(range(0x20)?)
                .ok_or_else(|| invalid("invalid submodel collision tree range"))?
            {
                contents |= material_contents([tree[24], tree[25]])?;
            }
        }
        result.push(InlineModel {
            mins,
            maxs,
            contents,
        });
    }
    Ok(result)
}
