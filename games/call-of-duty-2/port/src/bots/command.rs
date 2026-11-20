use crate::{usercmd::UserCmd, vm::items::NativeRandom};
fn fraction(rng: &mut NativeRandom) -> f64 {
    f64::from(rng.draw()) / 2147483648.0
}

fn movement_axis(rng: &mut NativeRandom) -> i8 {
    if fraction(rng) < f64::from(0.33f32) {
        127
    } else if fraction(rng) < 0.5 {
        -127
    } else {
        0
    }
}

/// SV_BotRandomUserCmd starts from a zeroed command, including serverTime.
/// ClientThink applies its usual clock clamp and movement simulation.
pub fn random_command(rng: &mut NativeRandom, weapon: u8, archived: bool) -> UserCmd {
    let mut cmd = UserCmd {
        weapon,
        ..Default::default()
    };
    if !archived {
        if fraction(rng) < 0.5 {
            cmd.buttons |= 1;
        }
        if fraction(rng) < 0.5 {
            cmd.buttons |= 0x28;
        }
        cmd.forward_move = movement_axis(rng);
        cmd.right_move = movement_axis(rng);
        for angle in &mut cmd.angles {
            if fraction(rng) < f64::from(0.33f32) {
                *angle = (360.0 * rng.signed()) as i32 as u16;
            }
        }
    }
    cmd
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn original_seeded_commands_preserve_conditional_draw_order_and_archive_gate() {
        let mut rng = NativeRandom::new(1);
        let expected = [
            (0, 0, 0, [0, 0, 0]),
            (40, 0, -127, [0, 0, 38]),
            (1, 0, 0, [0, 0, 77]),
            (41, 127, -127, [0, -281, 0]),
        ];
        for (i, (buttons, forward_move, right_move, angles)) in expected.into_iter().enumerate() {
            let cmd = random_command(&mut rng, i as u8, i == 0);
            assert_eq!(
                cmd,
                UserCmd {
                    weapon: i as u8,
                    buttons,
                    forward_move,
                    right_move,
                    angles: angles.map(|n| n as u16),
                    ..Default::default()
                }
            );
        }
    }
}
