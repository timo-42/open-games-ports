//! Cooperative script-thread queues from OP_wait, OP_waittillFrameEnd,
//! Scr_NotifyObject and Scr_ResumeSavedStacks. A spawned script runs inline
//! until it finishes or suspends, even when spawned by another script.
use std::collections::{BTreeMap, VecDeque};
use std::io;

const CLOCK_MASK: u32 = 0xff_ffff;
const MAX_WAIT: u32 = CLOCK_MASK - 1;
pub type ThreadId = u64;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Event {
    pub object: u32,
    pub name: String,
}

#[derive(Debug)]
pub enum Wait<V> {
    Ticks(u32),
    FrameEnd,
    Event(Event),
    /// waittillmatch compares a prefix of notify arguments.
    Match(Event, Vec<V>),
}

#[derive(Debug, PartialEq)]
pub enum Resume<V> {
    Time,
    Notify(Vec<V>),
    /// Pop this frame and all deeper frames, returning undefined to the caller.
    Unwind(usize),
}

/// OP_wait multiplies using binary32, rounds to the nearest tick (half up)
/// and promotes positive, sub-tick waits to one 50 ms tick.
pub fn wait_ticks(seconds: f32) -> io::Result<u32> {
    if seconds < 0.0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "negative wait is not allowed",
        ));
    }
    let ticks = (f64::from(seconds * 20.0) + 0.5).floor();
    if !ticks.is_finite() || ticks > f64::from(MAX_WAIT) {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "wait is too long",
        ));
    }
    Ok((ticks as u32).max(u32::from(seconds > 0.0)))
}

#[derive(Debug)]
enum Site<V> {
    Running,
    Tick(u32),
    Event(Event, Option<Vec<V>>),
}

#[derive(Debug)]
struct Thread<C, V> {
    owner: u32,
    saved: Option<C>,
    site: Site<V>,
    resume: Resume<V>,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
struct Listener {
    thread: ThreadId,
    /// None for waittill / waittillmatch. Some(depth) for endon.
    end_scope: Option<usize>,
}

#[derive(Debug)]
pub struct Scheduler<C, V> {
    tick: u32,
    next_thread: ThreadId,
    threads: BTreeMap<ThreadId, Thread<C, V>>,
    by_tick: BTreeMap<u32, VecDeque<ThreadId>>,
    listeners: BTreeMap<Event, Vec<Listener>>,
}

impl<C, V> Default for Scheduler<C, V> {
    fn default() -> Self {
        Self {
            tick: 0,
            next_thread: 1,
            threads: BTreeMap::new(),
            by_tick: BTreeMap::new(),
            listeners: BTreeMap::new(),
        }
    }
}

impl<C, V> Scheduler<C, V> {
    pub(super) fn suspended(&self) -> impl Iterator<Item = (ThreadId, &C)> {
        self.threads
            .iter()
            .filter_map(|(&id, thread)| Some((id, thread.saved.as_ref()?)))
    }

    pub(super) fn endon_count(&self, thread: ThreadId) -> usize {
        self.listeners
            .values()
            .flatten()
            .filter(|listener| listener.thread == thread && listener.end_scope.is_some())
            .count()
    }

    pub fn tick(&self) -> u32 {
        self.tick
    }

    /// The VM already holds the new thread's initial call frame.
    pub fn start(&mut self, owner: u32) -> ThreadId {
        let id = self.next_thread;
        self.next_thread += 1;
        self.threads.insert(
            id,
            Thread {
                owner,
                saved: None,
                site: Site::Running,
                resume: Resume::Time,
            },
        );
        id
    }

    pub fn owner(&self, id: ThreadId) -> Option<u32> {
        Some(self.threads.get(&id)?.owner)
    }

    pub fn endon(&mut self, id: ThreadId, scope: usize, event: Event) {
        if self.threads.contains_key(&id) {
            self.listeners.entry(event).or_default().push(Listener {
                thread: id,
                end_scope: Some(scope),
            });
        }
    }

    pub fn leave_scope(&mut self, id: ThreadId, scope: usize) {
        self.remove_listeners(|entry| {
            entry.thread == id && entry.end_scope.is_some_and(|s| s >= scope)
        });
    }

    fn remove_listeners(&mut self, remove: impl Fn(&Listener) -> bool) {
        self.listeners.retain(|_, entries| {
            entries.retain(|entry| !remove(entry));
            !entries.is_empty()
        });
    }

    fn remove_from_tick(&mut self, id: ThreadId) {
        if let Some(Thread {
            site: Site::Tick(tick),
            ..
        }) = self.threads.get(&id)
            && let Some(slot) = self.by_tick.get_mut(tick)
        {
            slot.retain(|thread| *thread != id);
            if slot.is_empty() {
                self.by_tick.remove(tick);
            }
        }
    }

    pub fn finish(&mut self, id: ThreadId) {
        self.remove_from_tick(id);
        self.remove_listeners(|entry| entry.thread == id);
        self.threads.remove(&id);
    }

    /// Save interpreter state at a wait boundary.
    pub fn suspend(&mut self, id: ThreadId, state: C, wait: Wait<V>) -> io::Result<()> {
        let thread = self
            .threads
            .get_mut(&id)
            .ok_or_else(|| io::Error::other("unknown script thread"))?;
        if !matches!(thread.site, Site::Running) || matches!(thread.resume, Resume::Unwind(_)) {
            return Err(io::Error::other(
                "cannot suspend inactive or terminating script frame",
            ));
        }
        match wait {
            Wait::Ticks(ticks) if ticks > MAX_WAIT => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "wait is too long",
                ));
            }
            Wait::Ticks(ticks) => {
                let wake = (self.tick + ticks) & CLOCK_MASK;
                self.by_tick.entry(wake).or_default().push_front(id);
                thread.site = Site::Tick(wake);
            }
            Wait::FrameEnd => {
                self.by_tick.entry(self.tick).or_default().push_back(id);
                thread.site = Site::Tick(self.tick);
            }
            Wait::Event(_) | Wait::Match(_, _) => {
                let (event, filter) = match wait {
                    Wait::Event(event) => (event, None),
                    Wait::Match(event, filter) => (event, Some(filter)),
                    _ => unreachable!(),
                };
                self.listeners
                    .entry(event.clone())
                    .or_default()
                    .push(Listener {
                        thread: id,
                        end_scope: None,
                    });
                thread.site = Site::Event(event, filter);
            }
        }
        thread.saved = Some(state);
        thread.resume = Resume::Time;
        Ok(())
    }

    pub fn pop_ready(&mut self) -> Option<(ThreadId, C, Resume<V>)> {
        let queue = self.by_tick.get_mut(&self.tick)?;
        let id = queue.pop_front()?;
        if queue.is_empty() {
            self.by_tick.remove(&self.tick);
        }
        let thread = self.threads.get_mut(&id)?;
        thread.site = Site::Running;
        Some((
            id,
            thread.saved.take()?,
            std::mem::replace(&mut thread.resume, Resume::Time),
        ))
    }

    /// Scr_AdvanceTime drains the current queue before advancing the 24-bit clock.
    pub fn advance_tick(&mut self) -> io::Result<()> {
        if self.by_tick.contains_key(&self.tick) {
            return Err(io::Error::other("current script threads have not finished"));
        }
        self.tick = (self.tick + 1) & CLOCK_MASK;
        Ok(())
    }

    /// Check after executing a notify or a synchronous nested thread. If the
    /// active thread has an endon, the interpreter unwinds it before proceeding.
    pub fn cancelled_scope(&mut self, id: ThreadId) -> Option<usize> {
        let thread = self.threads.get_mut(&id)?;
        if !matches!(thread.site, Site::Running) {
            return None;
        }
        if let Resume::Unwind(depth) = thread.resume {
            thread.resume = Resume::Time;
            Some(depth)
        } else {
            None
        }
    }

    fn trigger_endon(&mut self, id: ThreadId, scope: usize) {
        let active = self
            .threads
            .get(&id)
            .is_some_and(|t| matches!(t.site, Site::Running));
        if scope == 0 && !active {
            self.finish(id);
            return;
        }
        self.leave_scope(id, scope);
        self.remove_from_tick(id);
        self.remove_listeners(|entry| entry.thread == id && entry.end_scope.is_none());
        if let Some(thread) = self.threads.get_mut(&id) {
            let depth = match thread.resume {
                Resume::Unwind(old) => old.min(scope),
                _ => scope,
            };
            thread.resume = Resume::Unwind(depth);
            if !active {
                thread.site = Site::Tick(self.tick);
                self.by_tick.entry(self.tick).or_default().push_front(id);
            }
        }
    }

    /// The VM supplies Scr_EvalEquals for waittillmatch. Regular waittill
    /// receives the original arguments. A successful match resumes without them.
    pub fn notify_by(&mut self, event: &Event, args: &[V], mut equals: impl FnMut(&V, &V) -> bool)
    where
        V: Clone,
    {
        let Some(listeners) = self.listeners.get(event).cloned() else {
            return;
        };
        for listener in listeners {
            if !self
                .listeners
                .get(event)
                .is_some_and(|entries| entries.contains(&listener))
            {
                continue;
            }
            if let Some(scope) = listener.end_scope {
                self.trigger_endon(listener.thread, scope);
                continue;
            }
            let Some(thread) = self.threads.get_mut(&listener.thread) else {
                continue;
            };
            let Site::Event(registered, filter) = &thread.site else {
                continue;
            };
            if registered != event {
                continue;
            }
            thread.resume = match filter {
                Some(prefix)
                    if prefix.len() <= args.len()
                        && prefix.iter().zip(args).all(|(a, b)| equals(a, b)) =>
                {
                    Resume::Time
                }
                Some(_) => continue,
                None => Resume::Notify(args.to_vec()),
            };
            thread.site = Site::Tick(self.tick);
            self.by_tick
                .entry(self.tick)
                .or_default()
                .push_front(listener.thread);
            if let Some(entries) = self.listeners.get_mut(event) {
                entries.retain(|entry| *entry != listener);
            }
        }
        if self.listeners.get(event).is_some_and(Vec::is_empty) {
            self.listeners.remove(event);
        }
    }

    pub fn notify(&mut self, event: &Event, args: &[V])
    where
        V: Clone + PartialEq,
    {
        self.notify_by(event, args, V::eq);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn event(name: &str) -> Event {
        Event {
            object: 1,
            name: name.into(),
        }
    }

    #[test]
    fn original_map_script_order() {
        // The equivalent map script ran on the pinned Linux 1.3 executable:
        // two wait 0, two waittill("go", n), two waittillframeend, root and
        // nested endon("kill"). Both notify calls ran before any resumes.
        let mut s = Scheduler::<&str, i32>::default();
        for (label, wait) in [
            ("a", Wait::Ticks(0)),
            ("b", Wait::Ticks(0)),
            ("c", Wait::Event(event("go"))),
            ("d", Wait::Event(event("go"))),
            ("e", Wait::FrameEnd),
            ("f", Wait::FrameEnd),
        ] {
            let id = s.start(1);
            s.suspend(id, label, wait).expect("first yield");
        }
        let root = s.start(1);
        s.endon(root, 0, event("kill"));
        s.suspend(root, "unexpected", Wait::Ticks(2))
            .expect("root sleeps");
        let nested = s.start(1);
        s.endon(nested, 1, event("kill"));
        s.suspend(nested, "parent", Wait::Ticks(1))
            .expect("nested sleeps");
        s.notify(&event("go"), &[31]);
        s.notify(&event("kill"), &[]);
        let mut trace = vec!["start".to_owned(), "post_notify".to_owned()];
        while let Some((id, text, resume)) = s.pop_ready() {
            let text = match resume {
                Resume::Notify(args) => format!("{text}{}", args[0]),
                Resume::Unwind(1) if text == "parent" => text.into(),
                Resume::Time => text.into(),
                unexpected => panic!("unexpected resume {unexpected:?}"),
            };
            trace.push(text);
            s.finish(id);
        }
        assert_eq!(
            trace,
            [
                "start",
                "post_notify",
                "parent",
                "d31",
                "c31",
                "b",
                "a",
                "e",
                "f"
            ]
        );
        for _ in 0..2 {
            s.advance_tick().expect("drained");
            assert!(s.pop_ready().is_none());
        }
    }

    #[test]
    fn wait_quantization_and_wrapping_clock() {
        assert_eq!(wait_ticks(0.0).expect("zero"), 0);
        assert_eq!(wait_ticks(0.001).expect("short"), 1);
        assert_eq!(wait_ticks(0.075).expect("half rounds up"), 2);
        assert!(wait_ticks(-1.0).is_err());
        assert!(wait_ticks(900_000.0).is_err());
        let mut s = Scheduler::<i32, i32> {
            tick: CLOCK_MASK,
            ..Default::default()
        };
        let id = s.start(42);
        assert_eq!(s.owner(id), Some(42));
        s.suspend(id, 456, Wait::Ticks(1)).expect("sleep");
        s.advance_tick().expect("next");
        assert_eq!(s.tick(), 0);
        assert!(s.advance_tick().is_err());
        assert_eq!(s.pop_ready(), Some((id, 456, Resume::Time)));
    }

    #[test]
    fn waittillmatch_and_owner_isolation() {
        let mut s = Scheduler::<(), i32>::default();
        let id = s.start(10);
        s.suspend(id, (), Wait::Match(event("hit"), vec![2, 3]))
            .expect("match");
        s.notify(
            &Event {
                object: 2,
                name: "hit".into(),
            },
            &[2, 3],
        );
        s.notify(&event("hit"), &[2]);
        s.notify(&event("hit"), &[2, 4]);
        assert!(s.pop_ready().is_none());
        s.notify(&event("hit"), &[2, 3, 4]);
        assert_eq!(s.pop_ready(), Some((id, (), Resume::Time)));
    }

    #[test]
    fn endon_lifetime_matches_active_call_frame() {
        let mut s = Scheduler::<(), i32>::default();
        let id = s.start(1);
        s.endon(id, 1, event("a"));
        s.leave_scope(id, 1);
        s.notify(&event("a"), &[]);
        assert_eq!(s.cancelled_scope(id), None);
        s.endon(id, 1, event("b"));
        s.endon(id, 0, event("b"));
        s.notify(&event("b"), &[]);
        assert_eq!(s.cancelled_scope(id), Some(0));
        s.finish(id);
        assert_eq!(s.owner(id), None);
    }
}
