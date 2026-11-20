//! Com_ModifyMsec and the millisecond residual consumed by SV_Frame.
use std::time::{Duration, Instant};

#[derive(Debug)]
pub(crate) struct FrameClock {
    sampled: Instant,
    pub(crate) residual: u32,
}

impl FrameClock {
    pub(crate) fn new(sampled: Instant) -> Self {
        Self {
            sampled,
            residual: 0,
        }
    }

    /// Sys_Milliseconds samples an integer clock. Carry sub-millisecond time
    /// across calls before applying Com_ModifyMsec's per-frame rounding.
    pub(crate) fn elapsed(&mut self, now: Instant) -> u64 {
        let millis = now.saturating_duration_since(self.sampled).as_millis() as u64;
        self.sampled += Duration::from_millis(millis);
        millis
    }
}

/// 0x08062594. The dedicated server clamps long hitches to five seconds, and
/// an active listen server clamps them to 200 ms. Fixedtime overrides timescale.
pub(crate) fn modified_msec(
    elapsed: u64,
    fixed: u32,
    scale: f32,
    dedicated: bool,
) -> (u32, Option<u64>) {
    let millis = if fixed != 0 {
        u64::from(fixed)
    } else if scale != 1.0 {
        (f64::from(elapsed as f32 * scale) + 0.5).floor() as u64
    } else {
        elapsed
    };
    let warning = (dedicated && (501..500000).contains(&millis)).then_some(millis);
    (
        millis.clamp(1, if dedicated { 5000 } else { 200 }) as u32,
        warning,
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn residual_keeps_wall_fractions_and_ignores_a_sample_before_a_map_reset() {
        let start = Instant::now();
        let mut clock = FrameClock::new(start);
        assert_eq!(clock.elapsed(start - Duration::from_secs(1)), 0);
        assert_eq!(clock.elapsed(start + Duration::from_micros(5900)), 5);
        assert_eq!(clock.elapsed(start + Duration::from_micros(11200)), 6);
    }

    #[test]
    fn fixed_time_rounding_and_hitch_bounds_match_dedicated_and_listen_servers() {
        assert_eq!(modified_msec(5, 0, 0.5, true), (3, None));
        assert_eq!(modified_msec(1, 0, 0.001, true), (1, None));
        assert_eq!(modified_msec(68, 1000, 0.5, true), (1000, Some(1000)));
        assert_eq!(modified_msec(500, 0, 1.0, true), (500, None));
        assert_eq!(modified_msec(501, 0, 1.0, true), (501, Some(501)));
        assert_eq!(modified_msec(45, 0, 1.5, true), (68, None));
        assert_eq!(modified_msec(15000, 0, 1.0, true), (5000, Some(15000)));
        assert_eq!(modified_msec(500000, 0, 1.0, true), (5000, None));
        assert_eq!(modified_msec(1000, 0, 1.0, false), (200, None));
    }
}
