//! Script random and orientation functions in chunk_105. The game RNG is
//! independent of libc rand() used by the network challenge code.
use super::value::{Value, script_error as error};
use std::io;
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Debug, Clone, Copy)]
pub struct GameRandom {
    seed: u32,
}

impl Default for GameRandom {
    fn default() -> Self {
        // G_InitGame reseeds from Sys_MillisecondsRaw (low word of epoch ms).
        let seed = match SystemTime::now().duration_since(UNIX_EPOCH) {
            Ok(elapsed) => elapsed.as_millis() as u32,
            Err(before) => (before.duration().as_millis() as u32).wrapping_neg(),
        };
        Self::new(seed)
    }
}

impl GameRandom {
    pub fn new(seed: u32) -> Self {
        Self { seed }
    }

    pub fn seed(&self) -> u32 {
        self.seed
    }

    fn sample(&mut self) -> i32 {
        self.seed = self.seed.wrapping_mul(0x343fd).wrapping_add(0x269ec3);
        (self.seed >> 17) as i32
    }

    /// Rand_Int, 0x080aaada. The 32-bit multiply can wrap, even for an
    /// otherwise valid pair of limits. Its following right shift is signed.
    pub fn integer(&mut self, lo: i32, hi: i32) -> i32 {
        let scaled = hi.wrapping_sub(lo).wrapping_mul(self.sample());
        lo.wrapping_add(scaled >> 15)
    }

    /// Rand_Float, 0x080aaa84. The reference keeps the scale and sum on
    /// the x87 stack and rounds to binary32 when writing the final result.
    pub fn float(&mut self, lo: f32, hi: f32) -> f32 {
        let lo = f64::from(lo);
        (lo + (f64::from(hi) - lo) * f64::from(self.sample()) / 32768.) as f32
    }
}

fn argument(args: &[Value], index: usize) -> io::Result<&Value> {
    args.get(index)
        .ok_or_else(|| error(format!("parameter {} does not exist", index + 1)))
}

fn integer(value: &Value) -> io::Result<i32> {
    if let Value::Int(value) = value {
        Ok(*value)
    } else {
        Err(error(format!("type {} is not an int", value.type_name())))
    }
}

fn vector(value: &Value) -> io::Result<[f32; 3]> {
    if let Value::Vector(value) = value {
        Ok(*value)
    } else {
        Err(error(format!("type {} is not a vector", value.type_name())))
    }
}

// Angles_ToVectors, 0x080a5226. It rounds the angle in radians and each
// sine/cosine to binary32 before combining them at x87 precision.
pub(crate) fn axes(angles: [f32; 3]) -> [[f32; 3]; 3] {
    let [pitch, yaw, roll] = angles.map(|degrees| {
        let radians = f64::from(degrees).to_radians() as f32;
        let (s, c) = f64::from(radians).sin_cos();
        (f64::from(s as f32), f64::from(c as f32))
    });
    let (sp, cp) = pitch;
    let (sy, cy) = yaw;
    let (sr, cr) = roll;
    [
        [cp * cy, cp * sy, -sp].map(|x| x as f32),
        [-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp].map(|x| x as f32),
        [cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp].map(|x| x as f32),
    ]
}

pub fn dispatch(name: &str, args: &[Value], random: &mut GameRandom) -> io::Result<Option<Value>> {
    let result = match name {
        "randomint" => {
            let limit = integer(argument(args, 0)?)?;
            if limit < 1 {
                return Err(error("RandomInt parm must be positive integer."));
            }
            Value::Int(random.integer(0, limit))
        }
        "randomintrange" => {
            let lo = integer(argument(args, 0)?)?;
            let hi = integer(argument(args, 1)?)?;
            if hi <= lo {
                return Err(error("RandomIntRange range must be positive integer."));
            }
            Value::Int(random.integer(lo, hi))
        }
        "randomfloat" => Value::Float(random.float(0.0, argument(args, 0)?.number()?)),
        "randomfloatrange" => {
            let lo = argument(args, 0)?.number()?;
            let hi = argument(args, 1)?.number()?;
            if hi <= lo {
                return Err(error("Scr_RandomFloatRange range must be positive float."));
            }
            Value::Float(random.float(lo, hi))
        }
        "vectortoangles" => Value::Vector(super::effects::angles(vector(argument(args, 0)?)?)),
        "anglestoforward" | "anglestoright" | "anglestoup" => {
            let [forward, right, up] = axes(vector(argument(args, 0)?)?);
            Value::Vector(match name {
                "anglestoforward" => forward,
                "anglestoright" => right,
                _ => up,
            })
        }
        _ => return Ok(None),
    };
    Ok(Some(result))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn random_range_inputs_and_next_seed_match_original() {
        // Recorded by calling the original Rand_Int/Rand_Float in order.
        let mut random = GameRandom::new(0xfedcba98);
        assert_eq!(random.integer(0, 11), 9);
        assert_eq!(random.seed, 0xd4e8cefb);
        assert_eq!(random.integer(-50, 52), 28);
        assert_eq!(random.integer(i32::MIN, i32::MAX), i32::MAX);
        assert_eq!(random.float(0., 1.), 0.7385559);
        assert_eq!(random.float(-50., 41.), -48.97525);
        assert_eq!(random.float(-7.5, -0.25), -6.2025757);
        assert_eq!(random.float(-f32::MAX, f32::MAX), -3.1070702e38);
        assert_eq!(random.seed, 0x0b212621);
    }

    #[test]
    fn script_checks_ranges_before_drawing() -> io::Result<()> {
        let mut random = GameRandom::new(0);
        assert!(dispatch("randomint", &[Value::Int(0)], &mut random).is_err());
        assert!(
            dispatch(
                "randomfloatrange",
                &[Value::Int(2), Value::Int(2)],
                &mut random
            )
            .is_err()
        );
        assert_eq!(random.seed, 0);
        assert_eq!(
            dispatch("randomfloat", &[Value::Int(0)], &mut random)?,
            Some(Value::Float(0.))
        );
        assert_eq!(random.seed, 0x269ec3);
        Ok(())
    }

    #[test]
    fn three_basis_vectors_match_original_float_words() {
        let basis = axes([11.25, -91.5, 33.75]).map(|v| v.map(f32::to_bits));
        assert_eq!(
            basis,
            [
                [0xbcd2523d, 0xbf7afeb7, 0xbe47c5c2],
                [0xbf540e94, 0x3e053cc2, 0xbf0b7e3f],
                [0xbf0f43a8, 0xbe1727d9, 0x3f50c428],
            ]
        );
    }
}
