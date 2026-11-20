//! Server-side leg, torso and view-bone controllers for the player DObj.
use super::pose::{BoneControl, PoseModel};
use crate::entity_delta::EntityState;

const CONTROL_BONES: [&str; 6] = ["back_low", "back_mid", "back_up", "neck", "head", "pelvis"];
const CROUCHED: u32 = 4;
const PRONE: u32 = 8;
const FIRING: u32 = 0x40;
const USING_TURRET: u32 = 0x300;
const MANTLING: u32 = 0x4000;
const DEAD: u32 = 0x20000;
const AIMING_DOWN_SIGHTS: u32 = 0x40000;
const IDLE_OR_CROUCHED_IDLE: u32 = (1 << 1) | (1 << 2);
const CLIMBING_UP_OR_DOWN: u32 = (1 << 18) | (1 << 19);

#[derive(Clone, Copy, Debug, Default)]
pub struct ViewInput {
    pub view_angles: [f32; 3],
    pub movement_yaw: f32,
    pub lean: f32,
    pub torso_height: f32,
    pub torso_pitch: f32,
    pub waist_pitch: f32,
    pub flags: u32,
    pub movement_bits: u32,
    pub firing: bool,
    pub legs_anim_flags: u32,
}

impl ViewInput {
    pub fn from_state(
        state: &EntityState,
        view_angles: [f32; 3],
        movement_bits: u32,
        firing: bool,
        legs_anim_flags: u32,
    ) -> Self {
        let field = |name| state.field(name).expect("registered entity state field");
        Self {
            view_angles,
            movement_yaw: f32::from_bits(field("angles2[1]")),
            lean: f32::from_bits(field("leanf")),
            torso_height: f32::from_bits(field("fTorsoHeight")),
            torso_pitch: f32::from_bits(field("fTorsoPitch")),
            waist_pitch: f32::from_bits(field("fWaistPitch")),
            flags: field("eFlags"),
            movement_bits,
            firing,
            legs_anim_flags,
        }
    }
}

#[derive(Clone, Copy, Debug, Default)]
struct Swing {
    angle: f32,
    moving: bool,
}

fn quantize(angle: f32) -> f32 {
    let units = (f64::from(angle) * f64::from(182.04445_f32)).trunc();
    // FISTP writes the integer-indefinite word on overflow; its low half is zero.
    if !(f64::from(i32::MIN)..=f64::from(i32::MAX)).contains(&units) {
        return 0.;
    }
    f32::from(units as i32 as u16) * (360. / 65536.)
}

fn subtract(a: f32, b: f32) -> f32 {
    let difference = a - b;
    let mut d = difference % 360.;
    if d == 0. && difference < 0. {
        // The original reaches positive zero by adding 360 repeatedly.
        return 0.;
    }
    if d > 180. {
        d -= 360.;
    }
    if d < -180. {
        d += 360.;
    }
    d
}

impl Swing {
    /// BG_SwingAngles (0x080da7dc).
    fn update(&mut self, goal: f32, tolerance: f32, limit: f32, speed: f32, frame_ms: i32) {
        let difference = subtract(goal, self.angle);
        if !self.moving && difference.abs() <= tolerance {
            return;
        }
        let scale = (difference.abs() * 0.05).max(0.5);
        let step = (f64::from(frame_ms) * f64::from(scale) * f64::from(speed)) as f32;
        self.moving = step < difference.abs();
        let delta = if self.moving {
            step.copysign(difference)
        } else {
            difference
        };
        self.angle = quantize(self.angle + delta);
        let remaining = subtract(goal, self.angle);
        if remaining > limit {
            self.angle = quantize(goal - limit);
        } else if remaining < -limit {
            self.angle = quantize(goal + limit);
        }
    }
}

#[derive(Clone, Debug, Default)]
pub struct PlayerControllers {
    legs_yaw: Swing,
    torso_yaw: Swing,
    torso_pitch: Swing,
    /// First six entries are the control bone angles; the final two are
    /// tag_origin angles and translation, respectively.
    blended: [[f32; 3]; 8],
}

impl PlayerControllers {
    pub(super) fn copy_blend(&mut self, previous: &Self) {
        self.blended = previous.blended;
    }

    pub fn legs_yaw(&self) -> (f32, bool) {
        (self.legs_yaw.angle, self.legs_yaw.moving)
    }
    pub fn torso_yaw(&self) -> f32 {
        self.torso_yaw.angle
    }
    pub fn set_legs_yaw(&mut self, angle: f32) {
        self.legs_yaw.angle = angle;
    }

    /// BG_PlayerAngles (0x080da9c8), called before PlayerAnim_SetMoveParams.
    pub fn update_angles(&mut self, input: &ViewInput, frame_ms: i32, swing_speed: f32) {
        let flags = input.flags;
        let turret = flags & USING_TURRET != 0;
        let ladder = input.movement_bits & CLIMBING_UP_OR_DOWN != 0;
        let yaw = quantize(input.view_angles[1]);
        let move_yaw = yaw + input.movement_yaw;
        if turret
            || ladder
            || flags & MANTLING != 0
            || input.movement_bits & IDLE_OR_CROUCHED_IDLE == 0
        {
            self.legs_yaw.moving = true;
            self.torso_yaw.moving = true;
            self.torso_pitch.moving = true;
        } else if input.firing {
            self.torso_yaw.moving = true;
            self.torso_pitch.moving = true;
        }
        let (legs_goal, torso_goal, torso_clamp) = if flags & DEAD != 0 {
            (yaw, yaw, 90.)
        } else if ladder {
            (move_yaw, move_yaw, 0.)
        } else if flags & MANTLING != 0 {
            (yaw, yaw, 90.)
        } else if flags & PRONE != 0 {
            (move_yaw, yaw, 90.)
        } else if flags & FIRING != 0 {
            (move_yaw, yaw, 45.)
        } else if flags & AIMING_DOWN_SIGHTS != 0 {
            (move_yaw, yaw, 90.)
        } else {
            (
                move_yaw,
                (f64::from(input.movement_yaw) * f64::from(0.3_f32) + f64::from(yaw)) as f32,
                90.,
            )
        };
        self.torso_yaw
            .update(torso_goal, 0., torso_clamp, swing_speed, frame_ms);
        if flags & DEAD != 0 {
            self.legs_yaw
                .update(legs_goal, 0., 150., swing_speed, frame_ms);
        } else if flags & PRONE != 0 {
            self.legs_yaw = Swing {
                angle: move_yaw,
                moving: false,
            };
        } else if input.legs_anim_flags & 0x30 != 0 {
            self.legs_yaw.moving = false;
            self.legs_yaw.update(yaw, 0., 150., swing_speed, frame_ms);
        } else {
            let tolerance = if self.legs_yaw.moving { 0. } else { 40. };
            self.legs_yaw
                .update(legs_goal, tolerance, 150., swing_speed, frame_ms);
        }
        if turret || ladder {
            let fixed_yaw = if turret { yaw } else { move_yaw };
            self.legs_yaw.angle = fixed_yaw;
            self.torso_yaw.angle = fixed_yaw;
        }
        let pitch = if flags & DEAD != 0 || turret || ladder || flags == MANTLING {
            0.
        } else {
            let pitch = input.view_angles[0];
            if pitch > 180. {
                (pitch - 360.) * 0.6
            } else {
                pitch * 0.6
            }
        };
        self.torso_pitch.update(pitch, 0., 45., 0.15, frame_ms);
    }

    /// PlayerView_ComputeSway (0x080db362), called using the current movement condition.
    fn target(&self, input: &ViewInput) -> [[f32; 3]; 8] {
        let flags = input.flags;
        let mut out = [[0.; 3]; 8];
        if flags & USING_TURRET != 0 {
            return out;
        }
        let mut pitch = 0.;
        if input.movement_bits & CLIMBING_UP_OR_DOWN == 0 {
            pitch = self.torso_pitch.angle;
            if flags & PRONE != 0 {
                pitch = quantize(pitch);
                if pitch > 180. {
                    pitch -= 360.;
                }
                pitch *= if pitch > 0. { 0.5 } else { 0.25 };
            }
        }
        let yaw = subtract(self.torso_yaw.angle, self.legs_yaw.angle);
        let head_pitch = subtract(input.view_angles[0], pitch);
        let pitch = subtract(pitch, 0.);
        let head_yaw = subtract(input.view_angles[1], self.torso_yaw.angle);
        let lean = (f64::from(input.lean) * (2. - f64::from(input.lean.abs()))) as f32;
        let roll = (f64::from(lean) * 50. * f64::from(0.925_f32)) as f32;
        let prone_offset = if input.torso_pitch != 0. || input.waist_pitch != 0. {
            subtract(input.torso_pitch, input.waist_pitch)
        } else {
            0.
        };
        let mut head_roll = roll;
        let root_yaw = if flags & DEAD != 0 {
            self.legs_yaw.angle
        } else {
            subtract(self.legs_yaw.angle, input.view_angles[1])
        };
        out[6][1] = root_yaw;
        let mut origin = [0., 0., input.torso_height];
        if lean != 0. {
            let distance = if lean > 0. {
                2.5
            } else if flags & CROUCHED != 0 {
                12.5
            } else {
                5.
            };
            origin[1] = -lean * distance;
        }
        if flags & PRONE == 0 {
            let torso_roll = if lean != 0. && (flags & CROUCHED == 0 || lean <= 0.) {
                roll * 1.25
            } else {
                roll
            };
            head_roll = torso_roll;
            out[6][2] = (f64::from(lean) * 50. * f64::from(0.075_f32)) as f32;
            out[0] = [pitch * 0.2 + prone_offset, yaw * 0.4, torso_roll * 0.5];
            out[1] = [pitch * 0.3, yaw * 0.4, torso_roll * 0.5];
            out[2] = [pitch * 0.5, yaw * 0.2, torso_roll * -0.6];
        } else {
            if lean != 0. {
                head_roll *= 0.5;
            }
            out[6][0] = input.torso_pitch;
            let radians = f64::from((f64::from(yaw) * std::f64::consts::PI / 180.) as f32);
            let sine = f64::from(radians.sin() as f32);
            let cosine = f64::from(radians.cos() as f32);
            origin[0] = (0. + (1. - cosine) * -24.) as f32;
            origin[1] = (f64::from(origin[1]) - sine * 12.) as f32;
            if f64::from(lean) * sine > 0. {
                origin[1] = (f64::from(origin[1]) - (1. - cosine) * f64::from(lean) * 16.) as f32;
            }
            out[0] = [prone_offset, roll * -1.2, roll * 0.3];
            out[1] = [
                0.,
                (f64::from(yaw) * f64::from(0.1_f32) - f64::from(roll) * f64::from(0.2_f32)) as f32,
                roll * 0.2,
            ];
            out[2] = [
                pitch,
                (f64::from(yaw) * f64::from(0.8_f32) + f64::from(roll)) as f32,
                roll * -0.2,
            ];
        }
        out[3] = [head_pitch * 0.3, head_yaw * 0.3, 0.];
        out[4] = [head_pitch * 0.7, head_yaw * 0.7, head_roll * -0.3];
        if input.waist_pitch != 0. || input.torso_pitch != 0. {
            out[5][0] = subtract(input.waist_pitch, input.torso_pitch);
        }
        out[7] = origin;
        out
    }

    /// PlayerView_ApplySwayBlend (0x080dbc70).
    pub fn blend(&mut self, input: &ViewInput, frame_ms: i32) {
        let target = self.target(input);
        let step = frame_ms as f32 * 0.36_f32;
        for (current, goal) in self.blended[..7].iter_mut().zip(&target) {
            for (current, &goal) in current.iter_mut().zip(goal) {
                let diff = goal - *current;
                *current = if diff > step {
                    *current + step
                } else if diff < -step {
                    *current - step
                } else {
                    goal
                };
            }
        }
        let position = &mut self.blended[7];
        let delta = std::array::from_fn::<_, 3, _>(|i| target[7][i] - position[i]);
        let len = delta
            .iter()
            .map(|&v| f64::from(v) * f64::from(v))
            .sum::<f64>() as f32;
        if len != 0. {
            let guess = f64::from(f32::from_bits(
                0x5f3759df_u32.wrapping_sub(len.to_bits() >> 1),
            ));
            let inv = (guess * (1.5 - f64::from(len * 0.5) * guess * guess)) as f32;
            let step = frame_ms as f32 * 0.1_f32;
            let frac = step * inv;
            if frac < 1. {
                for i in 0..3 {
                    position[i] =
                        (f64::from(position[i]) + f64::from(delta[i]) * f64::from(frac)) as f32;
                }
            } else {
                *position = target[7];
            }
        }
    }

    pub fn bones(&self, models: &[PoseModel<'_>]) -> Vec<BoneControl> {
        let mut controls = Vec::with_capacity(7);
        for (index, name) in CONTROL_BONES
            .iter()
            .copied()
            .chain(["tag_origin"])
            .enumerate()
        {
            if let Some(bone) = models
                .iter()
                .flat_map(|model| &model.parts.bones)
                .position(|bone| bone.name == name)
            {
                controls.push(BoneControl {
                    bone,
                    rotation: quaternion(self.blended[index]),
                    translation: if index == 6 { self.blended[7] } else { [0.; 3] },
                    control_rotation: index != 6,
                });
            }
        }
        controls
    }
}

// DObj_SetBoneTransform (0x080bbd20) computes half-angle sine/cosine in
// double precision, then stores each result in a single-precision local.
fn quaternion(angles: [f32; 3]) -> [f32; 4] {
    let [(sp, cp), (sy, cy), (sr, cr)] = angles.map(|angle| {
        let rad = f64::from((f64::from(angle) * std::f64::consts::PI / 360.) as f32);
        (f64::from(rad.sin() as f32), f64::from(rad.cos() as f32))
    });
    // The server spills the four yaw/pitch products before combining roll.
    let sin_pitch_yaw = f64::from((-sp * sy) as f32);
    let sin_pitch_cos_yaw = f64::from((sp * cy) as f32);
    let cos_pitch_sin_yaw = f64::from((cp * sy) as f32);
    let cos_pitch_yaw = f64::from((cp * cy) as f32);
    [
        (sr * cos_pitch_yaw + cr * sin_pitch_yaw) as f32,
        (cr * sin_pitch_cos_yaw + sr * cos_pitch_sin_yaw) as f32,
        (-sr * sin_pitch_cos_yaw + cr * cos_pitch_sin_yaw) as f32,
        (cr * cos_pitch_yaw - sr * sin_pitch_yaw) as f32,
    ]
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::assets::models::{ModelBone, ModelParts};

    #[test]
    fn full_left_lean_matches_original_local_bone_writes() {
        let input = ViewInput {
            view_angles: [-130., -140., 0.],
            movement_yaw: -65.,
            lean: -1.,
            torso_height: -0.5,
            torso_pitch: -6.375,
            waist_pitch: -12.5,
            ..ViewInput::default()
        };
        let mut controllers = PlayerControllers::default();
        controllers.update_angles(&input, 17, 0.2);
        controllers.blend(&input, 17);
        let parts = ModelParts {
            root_count: 1,
            bones: ["tag_origin", "unused", "head", "back_low"]
                .into_iter()
                .map(|name| ModelBone {
                    name: name.to_owned(),
                    ..ModelBone::default()
                })
                .collect(),
        };
        let controls = controllers.bones(&[PoseModel {
            parts: &parts,
            parent_tag: None,
        }]);
        assert_eq!(controls.len(), 3);
        assert_eq!(controls[0].bone, 3);
        assert!(controls[0].control_rotation);
        assert_eq!(
            controls[0].rotation.map(f32::to_bits),
            [0xbd5b3dea, 0xbaa7ade8, 0xbd5b3dea, 0x3f7f43ea]
        );
        assert_eq!(controls[1].bone, 2);
        assert_eq!(
            controls[1].rotation.map(f32::to_bits),
            [0x3d4e5f7d, 0xbd65af06, 0xbd4e5f7e, 0x3f7ef20b]
        );
        assert_eq!(controls[2].bone, 0);
        assert!(!controls[2].control_rotation);
        assert_eq!(
            controls[2].rotation.map(f32::to_bits),
            [0xbd05d387, 0xbae4ee2c, 0x3d5a88cb, 0x3f7f7f85]
        );
        assert_eq!(
            controls[2].translation.map(f32::to_bits),
            [0, 0x3fd82ddc, 0xbe2cf17d]
        );
    }
}
