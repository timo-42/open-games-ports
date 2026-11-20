use super::*;
use crate::assets::models::{BoneBounds, CollisionSurface, ModelBone, ModelParts};
use std::sync::Arc;

fn transform(x: f32) -> BoneTransform {
    BoneTransform {
        quaternion: [0., 0., 0., 1.],
        origin: [x, 0., 0.],
        scale: 2.,
    }
}

fn model(bones: &[(&str, u8, f32)]) -> ServerModel {
    let bones = bones
        .iter()
        .map(|&(name, classification, x)| ModelBone {
            name: name.into(),
            classification,
            bind_pose: transform(x),
            ..ModelBone::default()
        })
        .collect::<Vec<_>>();
    let bone_bounds = bones
        .iter()
        .map(|_| BoneBounds {
            bounds: [[-1.; 3], [1.; 3]],
            center: [0.; 3],
            radius_squared: 3.,
        })
        .collect();
    ServerModel {
        is_default: false,
        flags: 0,
        bounds: [[-10.; 3], [10.; 3]],
        lods: Default::default(),
        collision_lod: 0,
        collision: Vec::new(),
        contents: 0,
        parts: Arc::new(ModelParts {
            root_count: bones.len() as u16,
            bones,
        }),
        bone_bounds,
    }
}

#[test]
fn triangles_transform_into_bone_space_and_respect_mask_and_occlusion() -> io::Result<()> {
    let mut m = model(&[("frame", 0, 2.)]);
    m.collision.push(CollisionSurface {
        triangles: vec![[1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0.]],
        bounds: [[-0.001, -0.001, -0.001], [0.001, 1.001, 1.001]],
        bone_index: 0,
        contents: 4,
        flags: 123,
    });
    let s = [4., 0.25, 0.25];
    let e = [0., 0.25, 0.25];
    let hit = trace_model(&m, None, s, e, 4, 1.)?.expect("front of triangle");
    assert_eq!(hit.fraction, (2. - 0.125) / 4.);
    assert_eq!(hit.normal, [1., 0., 0.]);
    assert_eq!(
        (hit.contents, hit.surface_flags, hit.part_name),
        (4, 123, "frame")
    );
    assert!(trace_model(&m, None, e, s, 4, 1.)?.is_none());
    assert!(trace_model(&m, None, s, e, 1, 1.)?.is_none());
    assert!(trace_model(&m, None, s, e, 4, 0.4)?.is_none());
    assert!(trace_model(&m, None, [4., 0.8, 0.8], [0., 0.8, 0.8], 4, 1.)?.is_none());
    let pose = [transform(3.)];
    assert_eq!(
        trace_model(&m, Some(&pose), s, e, 4, 1.)?
            .expect("fixture segment hits the model")
            .fraction,
        (1. - 0.125) / 4.
    );
    Ok(())
}

#[test]
fn priority_can_choose_a_farther_part_within_the_world_clip() -> io::Result<()> {
    let m = model(&[("leg", 13, 0.), ("head", 2, 4.)]);
    let views = [ModelView {
        model: &m,
        pose: None,
        parent_bone: None,
        ignore_collision: false,
    }];
    let s = [-4., 0., 0.];
    let e = [8., 0., 0.];
    let pistol = hitbox::trace(&views, s, e, false, 1.)?.expect("fixture segment hits the model");
    let rifle = hitbox::trace(&views, s, e, true, 1.)?.expect("fixture segment hits the model");
    assert_eq!(
        (pistol.part_name, pistol.part_group, pistol.fraction),
        ("leg", 13, 3. / 12.)
    );
    assert_eq!(
        (rifle.part_name, rifle.part_group, rifle.fraction),
        ("head", 2, 7. / 12.)
    );
    assert_eq!(rifle.normal, [-1., 0., 0.]);
    assert_eq!(
        hitbox::trace(&views, s, e, true, 0.5)?
            .expect("fixture segment hits the model")
            .part_name,
        "leg"
    );
    Ok(())
}

#[test]
fn attached_and_melded_parts_inherit_class_from_ignored_parent() -> io::Result<()> {
    let parent = model(&[("skull", 2, -4.)]);
    let child = model(&[("skull", 0, 2.)]);
    for tag in [None, Some(0)] {
        let views = [
            ModelView {
                model: &parent,
                pose: None,
                parent_bone: None,
                ignore_collision: true,
            },
            ModelView {
                model: &child,
                pose: None,
                parent_bone: tag,
                ignore_collision: false,
            },
        ];
        let hit = hitbox::trace(&views, [-4., 0., 0.], [8., 0., 0.], true, 1.)?
            .expect("fixture segment hits the model");
        assert_eq!(
            (hit.part_name, hit.part_group, hit.fraction),
            ("skull", 2, 5. / 12.)
        );
    }
    Ok(())
}
