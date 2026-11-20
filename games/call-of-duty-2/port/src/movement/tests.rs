use super::*;
use crate::snapshot::PlayerState;

fn flat(start: Vec3, end: Vec3, mins: Vec3, _: Vec3, _: u32) -> Hit {
    if start[2] + mins[2] >= 0. && end[2] + mins[2] < 0. {
        Hit {
            fraction: (start[2] + mins[2]) / (start[2] - end[2]),
            entity: 1022,
            normal: [0., 0., 1.],
            surface_flags: 5 << 20,
            ..Default::default()
        }
    } else {
        Hit::default()
    }
}
fn tick(ps: &mut MovementState, ms: i32, forward: i8, right: i8, buttons: u32) {
    let cmd = UserCmd {
        server_time: ps.command_time + ms,
        forward_move: forward,
        right_move: right,
        buttons,
        ..Default::default()
    };
    advance_traced_steps(ps, &cmd, &Default::default(), &flat, |_, _, _, _| Ok(()))
        .expect("simulate movement on fixture geometry");
}

#[test]
fn grounded_strafe_jump_and_landing() {
    let mut ps = MovementState {
        command_time: 1000,
        ..Default::default()
    };
    tick(&mut ps, 50, 127, 0, 0);
    assert_eq!(ps.ground_entity, 1022);
    assert_eq!(ps.velocity, [86., 0., 0.]); // 190 * 9 * .05 = 85.5, Sys_SnapVector
    assert!((ps.origin[0] - 4.275).abs() < 0.0001);
    for _ in 0..16 {
        tick(&mut ps, 50, 0, 127, 0);
    }
    assert_eq!(ps.velocity, [1., -152., 0.]); // player_strafeSpeedScale
    tick(&mut ps, 50, 0, 0, 0x400);
    assert_eq!(ps.ground_entity, 1023);
    assert_eq!(ps.velocity[2], 210.);
    assert!((ps.origin[2] - 11.49).abs() < 0.001);
    assert_eq!(ps.jump_time, 1900);
    for _ in 0..20 {
        tick(&mut ps, 50, 0, 0, 0);
    }
    assert_eq!(ps.ground_entity, 1022);
    assert!(ps.pm_time > 0);
    let mut out = PlayerState::default();
    ps.write_player(&mut out);
    assert!(out.words()[0xa4 / 4] >= 2);
    let seq = out.words()[0xa4 / 4];
    ps.write_player(&mut out);
    assert_eq!(out.words()[0xa4 / 4], seq);
}

#[test]
fn crouch_height_curve_and_blocked_clearance() {
    let mut ps = MovementState {
        command_time: 1000,
        view_height: 60.,
        view_height_target: 60,
        ..Default::default()
    };
    tick(&mut ps, 50, 0, 0, 0x200);
    assert_eq!(ps.view_height_target, 40);
    assert_eq!(ps.entity_bounds, [[-15., -15., 0.], [15., 15., 50.]]);
    assert_eq!(ps.maxs, [15., 15., 70.]);
    assert_eq!(ps.view_height, 60.);
    tick(&mut ps, 60, 0, 0, 0x200);
    assert_eq!(ps.view_height, 56.);
    tick(&mut ps, 140, 0, 0, 0x200);
    assert_eq!(ps.view_height, 40.);
    let cmd = UserCmd {
        server_time: ps.command_time + 50,
        ..Default::default()
    };
    advance_traced_steps(
        &mut ps,
        &cmd,
        &Default::default(),
        &|a, b, mins, maxs, mask| {
            if a == b && maxs[2] > 50. {
                Hit {
                    start_solid: true,
                    all_solid: true,
                    ..Default::default()
                }
            } else {
                flat(a, b, mins, maxs, mask)
            }
        },
        |_, _, _, _| Ok(()),
    )
    .expect("simulate movement on fixture geometry");
    assert_eq!(ps.view_height_target, 40);
    tick(&mut ps, 50, 0, 0, 0);
    tick(&mut ps, 200, 0, 0, 0);
    assert_eq!(ps.view_height, 60.);
    assert_eq!(ps.entity_flags & 4, 0);
    assert_eq!(ps.entity_bounds, [[-15., -15., 0.], [15., 15., 70.]]);
}

#[test]
fn spectator_movement_and_reference_substeps() {
    let mut ps = MovementState {
        pm_type: 4,
        command_time: 350,
        speed: 400.,
        origin: [0., 0., 500.],
        ..Default::default()
    };
    let cmd = UserCmd {
        server_time: 450,
        forward_move: 127,
        ..Default::default()
    };
    ps.seed_previous_command(UserCmd {
        weapon: 7,
        offhand: 2,
        ..Default::default()
    });
    let mut steps = Vec::new();
    advance_traced_steps(&mut ps, &cmd, &Default::default(), &flat, |_, c, o, ms| {
        steps.push((c.server_time, o.weapon, o.offhand, ms));
        Ok(())
    })
    .expect("simulate movement on fixture geometry");
    assert_eq!(steps, [(416, 7, 2, 66), (450, 0, 0, 34)]);
    assert_eq!(ps.view_height_target, 0);
    assert!((ps.origin[0] - 47.19693).abs() < 0.001);
    let pos = ps.origin;
    tick(&mut ps, -1, 0, 0, 0);
    assert_eq!(ps.origin, pos);
    tick(&mut ps, 50, 0, 0, 0x80);
    assert!(ps.origin[2] > pos[2]);
}

#[test]
fn empty_space_gravity_preserves_full_velocity_at_half_step() {
    let mut ps = MovementState {
        origin: [0., 0., 122.],
        ..Default::default()
    };
    tick(&mut ps, 50, 0, 0, 0);
    assert_eq!(ps.origin[2], 121.);
    assert_eq!(ps.velocity[2], -40.);
}

#[test]
fn prone_transition_uses_intermediate_crouch_and_four_hundred_ms_curve() {
    let mut ps = MovementState {
        command_time: 1000,
        ground_entity: 1022,
        view_height: 60.,
        view_height_target: 60,
        ..Default::default()
    };
    for _ in 0..5 {
        tick(&mut ps, 50, 0, 0, 0x100);
    }
    assert_eq!((ps.view_height_target, ps.view_height), (40, 40.));
    tick(&mut ps, 50, 0, 0, 0x100);
    assert_eq!(
        (ps.view_height_target, ps.pm_flags & 3, ps.entity_flags & 12),
        (11, 1, 8)
    );
    for _ in 0..4 {
        tick(&mut ps, 50, 0, 0, 0x100);
    }
    assert_eq!(ps.view_height, 15.);
    for _ in 0..4 {
        tick(&mut ps, 50, 0, 0, 0x100);
    }
    assert_eq!(ps.view_height, 11.);
    tick(&mut ps, 50, 0, 0, 0);
    assert_eq!(
        (ps.view_height_target, ps.view_height, ps.height_lerp_down),
        (40, 11., false)
    );
    for _ in 0..8 {
        tick(&mut ps, 50, 0, 0, 0);
    }
    assert_eq!(ps.view_height, 40.);
    for _ in 0..5 {
        tick(&mut ps, 50, 0, 0, 0);
    }
    assert_eq!(ps.view_height, 60.);
}

#[test]
fn prone_requires_rear_clearance_and_supported_waist() {
    let mut ps = MovementState {
        command_time: 1000,
        ground_entity: 1022,
        view_height: 60.,
        view_height_target: 60,
        ..Default::default()
    };
    let blocked = |a: Vec3, b: Vec3, mins: Vec3, maxs: Vec3, mask: u32| {
        if mins == [-6.; 3] && b[0] < a[0] - 15. {
            Hit {
                fraction: 0.1,
                normal: [1., 0., 0.],
                entity: 1022,
                ..Default::default()
            }
        } else {
            flat(a, b, mins, maxs, mask)
        }
    };
    let cmd = UserCmd {
        server_time: 1050,
        buttons: 0x100,
        ..Default::default()
    };
    advance_traced_steps(
        &mut ps,
        &cmd,
        &Default::default(),
        &blocked,
        |_, _, _, _| Ok(()),
    )
    .expect("simulate movement on fixture geometry");
    assert_eq!(ps.view_height_target, 60);
    assert_eq!(ps.pm_flags & 0x10003, 0x10000);
    assert!(ps.events.contains(&(0x8c, 0)));
}

#[test]
fn player_body_overlap_retries_trace_and_reports_entity_once() {
    let mut ps = MovementState {
        command_time: 900,
        view_height: 60.,
        view_height_target: 60,
        ..Default::default()
    };
    let body_hits = Cell::new(0);
    let probe = |start, end, mins, maxs, mask| {
        if mask & 0x200_0000 != 0 && start != end {
            body_hits.set(body_hits.get() + 1);
            Hit {
                entity: 7,
                contents: 0x2000000,
                all_solid: true,
                start_solid: true,
                fraction: 0.,
                ..Default::default()
            }
        } else {
            flat(start, end, mins, maxs, mask)
        }
    };
    let cmd = UserCmd {
        server_time: 1000,
        forward_move: 127,
        ..Default::default()
    };
    advance_traced_steps(&mut ps, &cmd, &Default::default(), &probe, |_, _, _, _| {
        Ok(())
    })
    .expect("simulate movement on fixture geometry");
    assert_eq!(ps.take_touched(), [7]);
    assert!(ps.take_touched().is_empty());
    assert!(body_hits.get() > 0);
    assert_eq!(ps.ground_entity, 1022);
    assert!(ps.origin[0] > 7.);
}

#[test]
fn ladder_attachment_climbing_and_jump_push() {
    let mut ps = MovementState {
        command_time: 1000,
        origin: [15.125, 0., 50.],
        delta_angles: [0, 32768, 0],
        ..Default::default()
    };
    let wall = |a: Vec3, b: Vec3, mins: Vec3, _: Vec3, mask: u32| {
        let d = a[0] + mins[0];
        if mask & 1 != 0 && d >= 0. && b[0] + mins[0] < 0. {
            Hit {
                fraction: ((d - 0.125) / (a[0] - b[0])).max(0.),
                normal: [1., 0., 0.],
                entity: 1022,
                surface_flags: 8 | (16 << 20),
                ..Default::default()
            }
        } else {
            Hit::default()
        }
    };
    let cmd = UserCmd {
        server_time: 1050,
        forward_move: 127,
        angles: [62000, 0, 0],
        ..Default::default()
    };
    advance_traced_steps(&mut ps, &cmd, &Default::default(), &wall, |_, _, _, _| {
        Ok(())
    })
    .expect("simulate movement on fixture geometry");
    assert_ne!(ps.pm_flags & 0x20, 0);
    assert_eq!(ps.ladder_normal, [1., 0., 0.]);
    assert!(ps.origin[2] > 50.);
    let cmd = UserCmd {
        server_time: 1100,
        buttons: 0x400,
        ..cmd
    };
    advance_traced_steps(&mut ps, &cmd, &Default::default(), &wall, |_, _, _, _| {
        Ok(())
    })
    .expect("simulate movement on fixture geometry");
    assert_eq!(ps.pm_flags & 0x20, 0);
    assert!(ps.velocity[0] > 0.);
    assert_eq!(ps.jump_time, 1100);
}

#[test]
fn acceleration_rounds_once_before_projection() {
    // x87 retains the full product in PM_Accelerate before writing accelSpeed.
    // A second rounding can select a different slide plane at a stair edge.
    let mut state = MovementState::default();
    accelerate_dir(
        &mut state,
        [1., 0., 0.],
        190.,
        9.,
        0.05,
        &MovementSettings::default(),
    );
    assert_eq!(state.velocity[0].to_bits(), 85.5_f32.to_bits());
}
