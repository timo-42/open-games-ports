//! The shipped Linux server implements native item scatter through libc rand().
//! G_InitGame seeds it alongside the independent GSC LCG.
use std::sync::{
    Arc,
    atomic::{AtomicUsize, Ordering},
};
#[derive(Debug)]
pub struct NativeRandom {
    state: [u32; 31],
    head: usize,
    loose_opens: Option<(Arc<AtomicUsize>, usize)>,
}
impl NativeRandom {
    pub fn new(seed: u32) -> Self {
        let mut state = [0; 31];
        state[0] = if seed == 0 { 1 } else { seed };
        let mut word = i64::from(state[0] as i32);
        for cell in &mut state[1..] {
            word = 16807 * (word % 127773) - 2836 * (word / 127773);
            if word < 0 {
                word += 2147483647;
            }
            *cell = word as u32;
        }
        let mut random = Self {
            state,
            head: 3,
            loose_opens: None,
        };
        for _ in 0..310 {
            random.draw();
        }
        random
    }
    pub fn draw(&mut self) -> u32 {
        if let Some((opens, previous)) = &mut self.loose_opens {
            let current = opens.load(Ordering::Relaxed);
            let pending = current.saturating_sub(*previous);
            *previous = current;
            for _ in 0..pending {
                self.next_word();
            }
        }
        self.next_word()
    }
    pub(crate) fn track_loose_opens(&mut self, opens: Arc<AtomicUsize>, at_seed: usize) {
        self.loose_opens = Some((opens, at_seed));
    }
    fn next_word(&mut self) -> u32 {
        let value = self.state[self.head].wrapping_add(self.state[(self.head + 28) % 31]);
        self.state[self.head] = value;
        self.head = (self.head + 1) % 31;
        value >> 1
    }
    pub fn fraction(&mut self) -> f64 {
        f64::from(self.draw()) / 2147483648.
    }

    pub fn signed(&mut self) -> f64 {
        f64::from(self.draw()) / 1073741824. - 1.
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn linux_rand_sequence() {
        let mut rng = NativeRandom::new(1);
        assert_eq!(
            std::array::from_fn::<_, 4, _>(|_| rng.draw()),
            [1804289383, 846930886, 1681692777, 1714636915]
        );
    }
}
