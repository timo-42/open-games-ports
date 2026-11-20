//! XAnim_LoadParts (0x080c3f8a), version 14.
use std::io;

#[derive(Debug)]
struct Input<'a>(&'a [u8]);

fn invalid(msg: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, msg)
}

impl<'a> Input<'a> {
    fn take(&mut self, len: usize) -> io::Result<&'a [u8]> {
        let (front, rest) = self
            .0
            .split_at_checked(len)
            .ok_or_else(|| invalid("truncated XAnim"))?;
        self.0 = rest;
        Ok(front)
    }
    fn u8(&mut self) -> io::Result<u8> {
        Ok(self.take(1)?[0])
    }
    fn u16(&mut self) -> io::Result<u16> {
        Ok(u16::from_le_bytes(
            self.take(2)?.try_into().expect("two bytes"),
        ))
    }
    fn f32(&mut self) -> io::Result<f32> {
        Ok(f32::from_le_bytes(
            self.take(4)?.try_into().expect("four bytes"),
        ))
    }
    fn string(&mut self) -> io::Result<String> {
        let len = self
            .0
            .iter()
            .position(|b| *b == 0)
            .ok_or_else(|| invalid("unterminated XAnim name"))?;
        let name = self.take(len + 1)?;
        Ok(std::str::from_utf8(&name[..len])
            .map_err(|_| invalid("invalid XAnim name"))?
            .to_owned())
    }
    fn keys<T>(
        &mut self,
        frames: u16,
        mut read: impl FnMut(&mut Self) -> io::Result<T>,
    ) -> io::Result<Keys<T>> {
        let count = self.u16()?;
        if count > frames {
            return Err(invalid("too many XAnim keys"));
        }
        let mut indices = Vec::new();
        if count > 1 && count < frames {
            for _ in 0..count {
                indices.push(if frames <= 256 {
                    self.u8()? as u16
                } else {
                    self.u16()?
                });
            }
            if indices[0] != 0
                || indices.last() != Some(&(frames - 1))
                || indices.windows(2).any(|p| p[0] >= p[1])
            {
                return Err(invalid("invalid XAnim key frame index"));
            }
        }
        let values = (0..count).map(|_| read(self)).collect::<io::Result<_>>()?;
        Ok(Keys { indices, values })
    }
    fn bone(&mut self, frames: u16, two_d: bool, reverse: bool) -> io::Result<BoneTrack> {
        let mut previous = None::<[i16; 4]>;
        let rotation = self.keys(frames, |input| {
            let mut q = [0i16; 4];
            let start = if two_d { 2 } else { 0 };
            for component in &mut q[start..3] {
                *component = input.u16()? as i16;
            }
            let squared: i64 = q[..3].iter().map(|v| i64::from(*v).pow(2)).sum();
            q[3] = ((32767i64.pow(2) - squared).max(0) as f32).sqrt().round() as i16;
            let flip = previous.map_or(reverse, |p| {
                p.into_iter()
                    .zip(q)
                    .map(|(a, b)| i32::from(a) * i32::from(b))
                    .fold(0i32, i32::wrapping_add)
                    < 0
            });
            if flip {
                q = q.map(i16::wrapping_neg);
            }
            previous = Some(q);
            Ok(q)
        })?;
        if !two_d && rotation.values.is_empty() {
            return Err(invalid("missing 3D XAnim rotation"));
        }
        let translation = self.keys(frames, |input| {
            Ok([input.f32()?, input.f32()?, input.f32()?])
        })?;
        Ok(BoneTrack {
            rotation,
            translation,
        })
    }
}

#[derive(Debug, Clone)]
struct Keys<T> {
    indices: Vec<u16>,
    values: Vec<T>,
}

impl<T> Keys<T> {
    fn sample(&self, frame: f32) -> Option<(&T, &T, f32)> {
        let end = self.values.len().checked_sub(1)?;
        if end == 0 {
            return Some((&self.values[0], &self.values[0], 0.));
        }
        let (prev, fraction) = if self.indices.is_empty() {
            let prev = (frame as usize).min(end);
            (prev, frame - prev as f32)
        } else {
            let key = self
                .indices
                .partition_point(|&index| f32::from(index) <= frame)
                .saturating_sub(1);
            let fraction = if key == end {
                0.
            } else {
                (frame - f32::from(self.indices[key]))
                    / f32::from(self.indices[key + 1] - self.indices[key])
            };
            (key, fraction)
        };
        Some((
            &self.values[prev],
            &self.values[(prev + 1).min(end)],
            fraction,
        ))
    }
}

#[derive(Debug, Clone)]
pub struct BoneTrack {
    rotation: Keys<[i16; 4]>,
    translation: Keys<[f32; 3]>,
}

impl BoneTrack {
    pub(super) fn has_rotation(&self) -> bool {
        !self.rotation.values.is_empty()
    }

    pub(super) fn sample(&self, frame: f32) -> ([f64; 4], [f64; 3]) {
        let rotation = self
            .rotation
            .sample(frame)
            .map_or([0., 0., 0., 32767.], |(a, b, t)| {
                std::array::from_fn(|i| {
                    f64::from(a[i]) + (f64::from(b[i]) - f64::from(a[i])) * f64::from(t)
                })
            });
        let translation = self.translation.sample(frame).map_or([0.; 3], |(a, b, t)| {
            std::array::from_fn(|i| {
                f64::from(a[i]) + (f64::from(b[i]) - f64::from(a[i])) * f64::from(t)
            })
        });
        (rotation, translation)
    }
}

/// A clip's `frames` is the number of intervals over normalized clip time.
#[derive(Debug, Clone)]
pub struct Clip {
    pub frames: u16,
    pub fps: i16,
    pub looping: bool,
    pub notifies: Vec<(String, f32)>,
    pub delta: Option<BoneTrack>,
    pub bones: Vec<(String, BoneTrack)>,
}

impl Clip {
    pub fn parse(bytes: &[u8]) -> io::Result<Self> {
        let mut input = Input(bytes);
        if input.u16()? != 14 {
            return Err(invalid("unsupported XAnim version"));
        }
        let frame_count = input.u16()?;
        let num_bones = usize::from(input.u16()?);
        let flags = input.u8()?;
        let fps = input.u16()? as i16;
        let looping = flags & 1 != 0;
        let count = frame_count
            .checked_add(u16::from(looping))
            .ok_or_else(|| invalid("invalid XAnim frame count"))?;
        let frames = count.checked_sub(1).ok_or_else(|| invalid("empty XAnim"))?;
        let delta = if flags & 2 != 0 {
            Some(input.bone(count, true, false)?)
        } else {
            None
        };
        let negative = input.take(num_bones.div_ceil(8))?;
        let twod = input.take(num_bones.div_ceil(8))?;
        let names = (0..num_bones)
            .map(|_| input.string())
            .collect::<io::Result<Vec<_>>>()?;
        let mut bones = Vec::with_capacity(num_bones);
        for (i, name) in names.into_iter().enumerate() {
            let mask = 1 << (i % 8);
            bones.push((
                name,
                input.bone(count, twod[i / 8] & mask != 0, negative[i / 8] & mask != 0)?,
            ));
        }
        let mut notifies = Vec::new();
        for _ in 0..input.u8()? {
            let name = input.string()?;
            let notify_frame = input.u16()?;
            notifies.push((
                name,
                if frames == 0 {
                    0.
                } else {
                    f32::from(notify_frame) / f32::from(frames)
                },
            ));
        }
        notifies.push(("end".into(), 1.));
        Ok(Self {
            frames,
            fps,
            looping,
            notifies,
            delta,
            bones,
        })
    }
    pub(crate) fn has_root_track(&self) -> bool {
        self.delta.is_some()
    }
    pub fn delta_at(&self, time: f32) -> ([f32; 2], [f32; 3]) {
        self.delta
            .as_ref()
            .map_or(([0., 32767.], [0.; 3]), |track| {
                let (q, v) = track.sample(time.clamp(0., 1.) * f32::from(self.frames));
                ([q[2] as f32, q[3] as f32], v.map(|n| n as f32))
            })
    }
    /// XAnim_NodeBlendSample and Quat_PlanarAngleDeg return signed yaw in degrees.
    pub fn relative_yaw(&self, start: f32, end: f32) -> f32 {
        let (a, _) = self.delta_at(start);
        let (b, _) = self.delta_at(end);
        let [az, aw] = a.map(f64::from);
        let [bz, bw] = b.map(f64::from);
        let k = f64::from(f32::from_bits(0x30800200));
        let z = ((bz * aw - bw * az) * k) as f32;
        let w = ((bw * aw + bz * az) * k) as f32;
        if z == 0. && w == 0. {
            return 0.;
        }
        let zz = z * z;
        let norm = (f64::from(w) * f64::from(w) + f64::from(zz)) as f32;
        let scale = f64::from(2f32 / norm);
        let sin = f64::from(z) * f64::from(w) * scale;
        let cos = 1. - f64::from(zz) * scale;
        sin.atan2(cos).to_degrees() as f32
    }
    pub fn relative_translation(&self, start: f32, end: f32) -> [f32; 3] {
        self.translation_delta(start, end, false)
    }
    /// XAnim_BlendNode requests reflection by the end sample's yaw. Direct
    /// clip queries in XAnim_NodeBlendSample use the start sample's yaw.
    pub(super) fn translation_delta(&self, start: f32, end: f32, reflect_at_end: bool) -> [f32; 3] {
        let (from, a) = self.delta_at(start);
        let (to, mut b) = self.delta_at(end);
        let [z, w] = if reflect_at_end { to } else { from };
        if self.looping && end < start {
            let first = self.translation_at(0.);
            let last = self.end_translation();
            for i in 0..3 {
                b[i] = (b[i] + last[i]) - first[i];
            }
        }
        let mut v = std::array::from_fn(|i| b[i] - a[i]);
        let zz = z * z;
        let length = (f64::from(w) * f64::from(w) + f64::from(zz)) as f32;
        if length != 0. {
            let k = 2. / length;
            let scaled = zz * k;
            let zw = (f64::from(z) * f64::from(w) * f64::from(k)) as f32;
            let [x, y] = [f64::from(v[0]), f64::from(v[1])];
            v[0] = ((1. - f64::from(scaled)) * x + f64::from(zw) * y) as f32;
            v[1] = (y - (f64::from(scaled) * y + f64::from(zw) * x)) as f32;
        }
        v
    }
    pub fn end_translation(&self) -> [f32; 3] {
        self.translation_at(1.)
    }

    pub fn translation_at(&self, time: f32) -> [f32; 3] {
        self.delta.as_ref().map_or([0.; 3], |track| {
            track
                .sample(time.clamp(0., 1.) * f32::from(self.frames))
                .1
                .map(|n| n as f32)
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sparse_word_indices_and_loop_translation() -> io::Result<()> {
        let mut data = vec![];
        for n in [14u16, 300, 0] {
            data.extend(n.to_le_bytes());
        }
        data.push(3);
        data.extend(30u16.to_le_bytes());
        data.extend(0u16.to_le_bytes());
        data.extend(3u16.to_le_bytes());
        for n in [0u16, 200, 300] {
            data.extend(n.to_le_bytes());
        }
        for x in [10f32, 20., 40.] {
            for value in [x, 0., 0.] {
                data.extend(value.to_le_bytes());
            }
        }
        data.push(0);
        let clip = Clip::parse(&data)?;
        assert!(clip.looping);
        assert_eq!(clip.frames, 300);
        assert_eq!(clip.delta_at(0.5), ([0., 32767.], [17.5, 0., 0.]));
        assert_eq!(clip.relative_translation(1., 0.5), [7.5, 0., 0.]);
        assert_eq!(clip.translation_at(2.), [40., 0., 0.]);
        assert_eq!(clip.notifies, vec![("end".to_owned(), 1.)]);
        data[17..19].copy_from_slice(&0u16.to_le_bytes());
        assert!(Clip::parse(&data).is_err());
        Ok(())
    }
}
