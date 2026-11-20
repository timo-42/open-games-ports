//! Server XAnim clip blending and DObj model-space bone evaluation.
mod clip;
mod player_skeleton;
mod tree;
use crate::assets::models::{BoneTransform, ModelParts};
pub use clip::{BoneTrack, Clip};
pub(crate) use player_skeleton::{PlayerSkeleton, begin_frame};
use std::{collections::BTreeMap, io};
pub use tree::{PlayerTree, update_player};

#[derive(Debug)]
pub enum PoseNode<'a> {
    Leaf { clip: &'a Clip, time: f32 },
    Group(Vec<(f32, PoseNode<'a>)>),
}

#[derive(Debug)]
pub struct PoseModel<'a> {
    pub parts: &'a ModelParts,
    pub parent_tag: Option<&'a str>,
}

#[derive(Debug)]
pub struct BoneControl {
    pub bone: usize,
    pub rotation: [f32; 4],
    pub translation: [f32; 3],
    /// Control rotations are concatenated on the opposite side of the parent rotation.
    pub control_rotation: bool,
}

#[derive(Clone, Copy, Debug, Default)]
struct Blend {
    q: [f32; 4],
    pos: [f32; 3],
    weight: f32,
}

fn length_sq(q: [f32; 4]) -> f32 {
    q.into_iter()
        .rev()
        .map(|v| f64::from(v) * f64::from(v))
        .sum::<f64>() as f32
}
fn rsqrt(v: f32) -> f32 {
    let guess = f32::from_bits(0x5f3759df - (v.to_bits() >> 1)) as f64;
    (guess * (1.5 - f64::from(v * 0.5) * guess * guess)) as f32
}
fn add<const N: usize, T: Into<f64>>(out: &mut [f32; N], values: [T; N], scale: f32) {
    for (dest, value) in out.iter_mut().zip(values) {
        *dest = (f64::from(*dest) + value.into() * f64::from(scale)) as f32;
    }
}

impl PoseNode<'_> {
    // XAnim_EvalNodeParts discards the weight of a sole active child. When
    // multiple children contribute to a non-root branch it normalizes the
    // quaternion separately from the positional weights.
    fn accumulate(
        &self,
        names: &BTreeMap<&str, usize>,
        out: &mut [Blend],
        weight: f32,
        nested: bool,
    ) -> io::Result<()> {
        match self {
            Self::Leaf { clip, time } => {
                if !(0. ..=1.).contains(time) {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidData,
                        "invalid XAnim clip time",
                    ));
                }
                let frame = *time * f32::from(clip.frames);
                for (name, track) in &clip.bones {
                    if let Some(&index) = names.get(name.as_str()) {
                        let (q, pos) = track.sample(frame);
                        let bone = &mut out[index];
                        if track.has_rotation() {
                            add(&mut bone.q, q, weight * f32::from_bits(0x38000100));
                        } else {
                            bone.q[3] += weight;
                        }
                        add(&mut bone.pos, pos, weight);
                        bone.weight += weight;
                    }
                }
            }
            Self::Group(nodes) => {
                let mut active = nodes.iter().filter(|(w, _)| *w != 0.);
                let Some((first_weight, first)) = active.next() else {
                    return Ok(());
                };
                let Some(second) = active.next() else {
                    return first.accumulate(names, out, weight, nested);
                };
                let mut child = vec![Blend::default(); out.len()];
                first.accumulate(names, &mut child, *first_weight, true)?;
                for (w, node) in std::iter::once(second).chain(active) {
                    node.accumulate(names, &mut child, *w, true)?;
                }
                for (bone, sub) in out.iter_mut().zip(child) {
                    if sub.weight == 0. {
                        continue;
                    }
                    let pos_scale = weight / sub.weight;
                    let len = length_sq(sub.q);
                    let rot_scale = if nested && len != 0. {
                        rsqrt(len) * weight
                    } else {
                        pos_scale
                    };
                    add(&mut bone.q, sub.q, rot_scale);
                    add(&mut bone.pos, sub.pos, pos_scale);
                    bone.weight += weight;
                }
            }
        }
        Ok(())
    }
}
fn concat(a: [f32; 4], b: [f32; 4]) -> [f32; 4] {
    let [x, y, z, w] = a.map(f64::from);
    let [px, py, pz, pw] = b.map(f64::from);
    [
        z * py + w * px + x * pw - y * pz,
        x * pz + w * py + y * pw - z * px,
        w * pz + y * px + z * pw - x * py,
        w * pw - x * px - y * py - z * pz,
    ]
    .map(|x| x as f32)
}

/// Compose all models in DObj order, applying controls before the hierarchy
/// pass. Unattached secondary models can share named bones with earlier models.
pub fn evaluate(
    models: &[PoseModel<'_>],
    tree: &PoseNode<'_>,
    controls: &[BoneControl],
) -> io::Result<Vec<BoneTransform>> {
    evaluate_cached(models, tree, controls, &BTreeMap::new())
}

struct Hierarchy<'a> {
    names: BTreeMap<&'a str, usize>,
    parents: Vec<Option<usize>>,
    aliases: Vec<Option<usize>>,
}

impl<'a> Hierarchy<'a> {
    fn new(models: &[PoseModel<'a>]) -> io::Result<Self> {
        let size: usize = models.iter().map(|m| m.parts.bones.len()).sum();
        if size > 127 || models.len() > 8 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "DObj bone/model limit exceeded",
            ));
        }
        let mut names = BTreeMap::new();
        let mut aliases = Vec::with_capacity(size);
        let mut parents = Vec::with_capacity(size);
        for model in models {
            let base = parents.len();
            let parent = model.parent_tag.and_then(|tag| names.get(tag)).copied();
            for (i, bone) in model.parts.bones.iter().enumerate() {
                let index = parents.len();
                let first = *names.entry(bone.name.as_str()).or_insert(index);
                aliases.push((base != 0 && parent.is_none() && first != index).then_some(first));
                let link = bone.parent.map(|p| usize::from(p) + base).or(parent);
                if link.is_some_and(|p| p >= base + i) {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidData,
                        "invalid DObj bone parent",
                    ));
                }
                parents.push(link);
            }
        }
        Ok(Self {
            names,
            parents,
            aliases,
        })
    }
    fn required(&self, mut index: usize) -> Vec<usize> {
        let mut required = vec![index];
        while let Some(next) = self.aliases[index].or(self.parents[index]) {
            required.push(next);
            index = next;
        }
        required
    }
}

fn evaluate_cached(
    models: &[PoseModel<'_>],
    tree: &PoseNode<'_>,
    controls: &[BoneControl],
    retained: &BTreeMap<usize, BoneTransform>,
) -> io::Result<Vec<BoneTransform>> {
    let Hierarchy {
        names,
        parents,
        aliases,
    } = Hierarchy::new(models)?;
    let size = parents.len();
    let mut local = vec![Blend::default(); size];
    tree.accumulate(&names, &mut local, 1., false)?;
    let mut reversed = vec![false; size];
    for control in controls {
        let slot = local.get_mut(control.bone).ok_or_else(|| {
            io::Error::new(io::ErrorKind::InvalidInput, "invalid DObj control bone")
        })?;
        *slot = Blend {
            q: control.rotation,
            pos: control.translation,
            weight: 1.,
        };
        reversed[control.bone] = control.control_rotation;
    }
    let mut result = Vec::<BoneTransform>::with_capacity(size);
    for (i, bone) in models.iter().flat_map(|m| m.parts.bones.iter()).enumerate() {
        if let Some(previous) = retained.get(&i) {
            result.push(*previous);
            continue;
        }
        if let Some(duplicate) = aliases[i] {
            result.push(result[duplicate]);
            continue;
        }
        let sampled = local[i];
        let mut q = if sampled.weight != 0. {
            sampled.q
        } else if bone.parent.is_some() {
            bone.local_quaternion
                .map(|v| f32::from(v) * f32::from_bits(0x38000100))
        } else {
            [0., 0., 0., 1.]
        };
        let mut pos = sampled.pos;
        if bone.parent.is_some() {
            for (v, offset) in pos.iter_mut().zip(bone.local_translation) {
                *v += offset;
            }
        }
        if let Some(parent) = parents[i] {
            let p = result[parent];
            q = if reversed[i] {
                concat(p.quaternion, q)
            } else {
                concat(q, p.quaternion)
            };
            pos = p.transform_point(pos);
        }
        let len = length_sq(q);
        let scale = if len == 0. {
            q[3] = 1.;
            2.
        } else {
            2. / len
        };
        result.push(BoneTransform {
            quaternion: q,
            origin: pos,
            scale,
        });
    }
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::assets::models::ModelBone;

    #[test]
    fn assembled_pose_melds_secondary_models_and_respects_named_tags() -> io::Result<()> {
        let root = ModelBone {
            name: "root".into(),
            ..Default::default()
        };
        let tag = ModelBone {
            name: "tag".into(),
            parent: Some(0),
            local_translation: [3., 0., 0.],
            local_quaternion: [0, 0, 0, 32767],
            ..Default::default()
        };
        let body = ModelParts {
            root_count: 1,
            bones: vec![root, tag],
        };
        let extension = ModelParts {
            root_count: 1,
            bones: vec![
                ModelBone {
                    name: "tag".into(),
                    ..Default::default()
                },
                ModelBone {
                    name: "tip".into(),
                    parent: Some(0),
                    local_translation: [0., 2., 0.],
                    local_quaternion: [0, 0, 0, 32767],
                    ..Default::default()
                },
            ],
        };
        let tree = PoseNode::Group(vec![]);
        let mut models = [
            PoseModel {
                parts: &body,
                parent_tag: None,
            },
            PoseModel {
                parts: &extension,
                parent_tag: None,
            },
        ];
        let controllers = [BoneControl {
            bone: 0,
            rotation: [0., 0., 0., 1.],
            translation: [40., 0., 0.],
            control_rotation: false,
        }];
        let bones = evaluate(&models, &tree, &controllers)?;
        assert_eq!(bones[2].origin, bones[1].origin);
        assert_eq!(bones[3].origin, [43., 2., 0.]);
        models[1].parent_tag = Some("root");
        let rigid = evaluate(&models, &tree, &controllers)?;
        assert_eq!(rigid[2].origin, bones[0].origin);
        assert_eq!(rigid[3].origin, [40., 2., 0.]);
        Ok(())
    }
}
