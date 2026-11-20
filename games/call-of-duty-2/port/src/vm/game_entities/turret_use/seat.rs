//! Turret control-bone transforms and seated animation root placement.
use super::{MapEntity, Vec3};
use crate::{
    animation::{
        PlayerAnimations,
        pose::{self, BoneControl, PlayerTree, PoseModel, PoseNode},
    },
    assets::models::{BoneTransform, ModelRegistry},
    collision::dobj::{DObjPose, PosedModel},
    player::spawn::copy_entity_state,
    vm::{GameHost, entity_methods::relink, invalid, value::Value},
};
use std::io;

fn transform(origin: Vec3, axis: [Vec3; 3], local: Vec3) -> Vec3 {
    std::array::from_fn(|i| {
        (f64::from(origin[i])
            + (0..3)
                .map(|j| f64::from(axis[j][i]) * f64::from(local[j]))
                .sum::<f64>()) as f32
    })
}
fn tag(e: &MapEntity, models: &ModelRegistry, name: &str) -> Option<BoneTransform> {
    let parts = &models.get(e.model_index)?.parts;
    let i = parts
        .bones
        .iter()
        .position(|b| b.name.eq_ignore_ascii_case(name))?;
    Some(
        e.dobj_pose
            .as_ref()
            .and_then(|p| p.models.first())
            .filter(|m| m.model_index == e.model_index)
            .and_then(|m| m.bones.get(i))
            .copied()
            .unwrap_or(parts.bones[i].bind_pose),
    )
}
pub(crate) fn tag_world(e: &MapEntity, models: &ModelRegistry, name: &str) -> Option<Vec3> {
    Some(transform(
        e.origin,
        super::super::frame::basis(e.angles),
        tag(e, models, name)?.origin,
    ))
}
pub(super) fn pose_turret(e: &mut MapEntity, models: &ModelRegistry) -> io::Result<()> {
    let parts = &models
        .get(e.model_index)
        .ok_or_else(|| invalid("turret model is unavailable"))?
        .parts;
    let [pitch, yaw, flash] = ["angles2[0]", "angles2[1]", "angles2[2]"]
        .map(|k| f32::from_bits(e.state.field(k).expect("registered network field")));
    let mut controls = Vec::new();
    for (bone, item) in parts.bones.iter().enumerate() {
        let (p, y) = match item.name.as_str() {
            "tag_aim" | "tag_aim_animated" => (pitch, yaw),
            "tag_flash" => (flash, 0.),
            _ => continue,
        };
        let (sp, cp) = (0.5 * f64::from(p).to_radians()).sin_cos();
        let (sy, cy) = (0.5 * f64::from(y).to_radians()).sin_cos();
        controls.push(BoneControl {
            bone,
            rotation: [-sy * sp, cy * sp, sy * cp, cy * cp].map(|n| n as f32),
            translation: [0.; 3],
            control_rotation: true,
        });
    }
    let bones = pose::evaluate(
        &[PoseModel {
            parts,
            parent_tag: None,
        }],
        &PoseNode::Group(vec![]),
        &controls,
    )?;
    e.dobj_pose = Some(DObjPose {
        models: vec![PosedModel {
            model_index: e.model_index,
            bones,
            parent_bone: None,
            ignore_collision: false,
        }],
    });
    Ok(())
}

#[derive(Clone, Copy)]
struct Root {
    rot: [f32; 2],
    pos: Vec3,
}
impl Root {
    fn mix(self, b: Self, t: f32) -> Self {
        // XAnim_BlendStepC discards weights below 0.001. A single remaining
        // child supplies the full transform in XAnim_BlendNode.
        if t < 0.001 {
            return self;
        }
        if 1. - t < 0.001 {
            return b;
        }
        Self {
            rot: std::array::from_fn(|i| self.rot[i] * (1. - t) + b.rot[i] * t),
            pos: std::array::from_fn(|i| self.pos[i] * (1. - t) + b.pos[i] * t),
        }
    }
    fn normalized(mut self) -> Self {
        let len = self.rot[0].hypot(self.rot[1]);
        if len != 0. {
            self.rot = self.rot.map(|v| v / len);
        }
        self
    }
}
fn root(bank: &PlayerAnimations, tree: &PlayerTree, node: u16) -> Root {
    let (rot, pos) = bank
        .pose_clip(node)
        .map_or(([0., 32767.], [0.; 3]), |clip| {
            clip.delta_at(tree.turret_clip_time(node))
        });
    Root { rot, pos }
}
#[derive(Clone, Copy)]
struct StanceRow {
    row: u16,
    left: u16,
    fraction: f32,
    root: Root,
}
impl StanceRow {
    fn goals(self, weight: f32, out: &mut Vec<(u16, f32)>) {
        out.push((self.row, weight));
        out.push((self.left, 1. - self.fraction));
        if self.fraction != 0. {
            out.push((self.left + 1, self.fraction));
        }
    }
}
fn seated_root(
    bank: &PlayerAnimations,
    tree: &mut PlayerTree,
    node: u16,
    tag_yaw: f32,
    inc: f32,
    height: f32,
    frame_ms: i32,
) -> io::Result<Root> {
    let mut previous: Option<StanceRow> = None;
    let rows = bank.child_range(node);
    let mut goals = Vec::with_capacity(6);
    for row in rows.clone() {
        let children = bank.child_range(row);
        let n = children.len();
        if n == 0 {
            return Err(invalid("turret stance animation has no children"));
        }
        let f = (n as f32 * 0.5 - tag_yaw / inc).clamp(0., (n - 1) as f32);
        let left = children.start + f as u16;
        let fraction = f.fract();
        let a = root(bank, tree, left);
        let value = if fraction != 0. {
            a.mix(root(bank, tree, left + 1), fraction)
        } else {
            a
        };
        let current = StanceRow {
            row,
            left,
            fraction,
            root: value,
        };
        if height <= value.pos[2] || row + 1 == rows.end {
            let result = if let Some(p) = previous.filter(|_| height <= value.pos[2]) {
                let blend = (height - p.root.pos[2]) / (value.pos[2] - p.root.pos[2]);
                current.goals(blend, &mut goals);
                p.goals(1. - blend, &mut goals);
                p.root.normalized().mix(value.normalized(), blend)
            } else {
                current.goals(1., &mut goals);
                value
            };
            tree.set_turret_weights(bank, node, &goals, frame_ms);
            return Ok(result);
        }
        previous = Some(current);
    }
    Err(invalid("turret player animation has no children"))
}
pub(super) fn update(slot: u16, number: u16, inc: f32, host: &mut GameHost<'_>) -> io::Result<()> {
    let e = host
        .world
        .entities
        .get(number)
        .ok_or_else(|| invalid("missing turret"))?;
    let p = host.world.players.get_mut(slot)?;
    let node = p.state.field("legsAnim").expect("registered network field") as u16 & !0x200;
    if host
        .world
        .animations
        .animation_for_index(node)
        .is_none_or(|a| a.flags & 4 == 0)
    {
        return Ok(());
    }
    let Some(tag) = tag(e, host.models, "tag_weapon") else {
        return Ok(());
    };
    let tag_axis = tag.axes();
    let yaw = f64::from(tag_axis[0][1])
        .atan2(f64::from(tag_axis[0][0]))
        .to_degrees() as f32;
    let axis = super::super::frame::basis(e.angles);
    let base_origin = e.origin;
    let user = host
        .world
        .entities
        .get(slot)
        .ok_or_else(|| invalid("missing occupant entity"))?;
    let height = (0..3)
        .rev()
        .map(|i| f64::from(user.origin[i] - base_origin[i]) * f64::from(axis[2][i]))
        .sum::<f64>() as f32;
    let r = seated_root(
        &host.world.animations,
        &mut p.pose_tree,
        node,
        yaw,
        inc,
        height - tag.origin[2],
        host.world.frame_ms,
    )?;
    let radians = f64::from(yaw).to_radians() as f32;
    let (s, c) = f64::from(radians).sin_cos();
    let (s, c) = (f64::from(s as f32), f64::from(c as f32));
    let [x, y, _] = r.pos.map(f64::from);
    let xy = [
        (x * c - y * s) as f32 + tag.origin[0],
        (y * c + x * s) as f32 + tag.origin[1],
        height,
    ];
    let mut pos = transform(base_origin, axis, xy);
    let mut start = pos;
    let mut end = pos;
    start[2] += f32::from_bits(p.state.words()[0xf8 / 4]);
    end[2] -= 60.;
    let tr = host.collision.box_trace(
        &host.world.entities,
        start,
        end,
        [0.; 3],
        [0.; 3],
        slot,
        0x2810011,
    );
    if tr.fraction < 1. {
        pos[2] = (f64::from(start[2])
            + f64::from(tr.fraction) * (f64::from(end[2]) - f64::from(start[2])))
            as f32;
    }
    let rotation = 2. * r.rot[0].atan2(r.rot[1]).to_degrees() + yaw;
    let (sy, cy) = rotation.to_radians().sin_cos();
    let forward: Vec3 = std::array::from_fn(|i| cy * axis[0][i] + sy * axis[1][i]);
    let left: Vec3 = std::array::from_fn(|i| -sy * axis[0][i] + cy * axis[1][i]);
    let angles = [
        (-forward[2])
            .atan2(forward[0].hypot(forward[1]))
            .to_degrees(),
        forward[1].atan2(forward[0]).to_degrees().rem_euclid(360.),
        left[2].atan2(axis[2][2]).to_degrees(),
    ];
    let e = host
        .world
        .entities
        .get_mut(slot)
        .ok_or_else(|| invalid("missing occupant entity"))?;
    let p = host.world.players.get_mut(slot)?;
    if let Some(skeleton) = e.player_skeleton.as_mut() {
        skeleton.refresh_tree(&p.pose_tree);
    }
    p.state.words_mut()[0x14 / 4..0x20 / 4].copy_from_slice(&pos.map(f32::to_bits));
    copy_entity_state(&mut p.state, &mut e.state, false, |_| {});
    e.origin = pos;
    e.angles = angles;
    relink(e, host.collision, e.model_index != 0)?;
    let object = e.object;
    host.world.set_field(object, "origin", Value::Vector(pos))?;
    host.world
        .set_field(object, "angles", Value::Vector(angles))
}
