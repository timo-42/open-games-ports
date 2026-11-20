//! Script object handles survive entity-slot reuse. Released handles can still
//! be held by GSC code, so a new allocation never aliases an older handle.
use super::{invalid, value::Value};
use std::{collections::BTreeMap, io};

pub(super) type Fields = BTreeMap<String, Value>;

#[derive(Debug)]
pub(super) struct Objects {
    live: BTreeMap<u32, Fields>,
    /// Scr_FlushEntityReleaseChain removes scripted fields after this tick's
    /// ready threads finish. Native fields are inaccessible immediately.
    awaiting_flush: BTreeMap<u32, Fields>,
    next: u32,
}

impl Default for Objects {
    fn default() -> Self {
        Self {
            live: BTreeMap::from([(0, Fields::new())]),
            awaiting_flush: BTreeMap::new(),
            next: 1,
        }
    }
}

impl Objects {
    pub(super) fn retained_fields(&self) -> impl Iterator<Item = &Fields> {
        self.live.values().chain(self.awaiting_flush.values())
    }

    pub(super) fn allocate(&mut self) -> io::Result<u32> {
        let id = self.next;
        self.next = id
            .checked_add(1)
            .ok_or_else(|| invalid("script object pool exhausted"))?;
        self.live.insert(id, Fields::new());
        Ok(id)
    }

    pub(super) fn field(&self, id: u32, name: &str) -> io::Result<Value> {
        if id >= self.next {
            return Err(invalid("unknown script object"));
        }
        Ok(self
            .live
            .get(&id)
            .or_else(|| self.awaiting_flush.get(&id))
            .and_then(|fields| fields.get(name))
            .cloned()
            .unwrap_or_default())
    }

    pub(super) fn live_fields(&self, id: u32) -> Option<&Fields> {
        self.live.get(&id)
    }

    pub(super) fn live_fields_mut(&mut self, id: u32) -> io::Result<&mut Fields> {
        self.live
            .get_mut(&id)
            .ok_or_else(|| invalid("unknown live script object"))
    }

    pub(super) fn is_released(&self, id: u32) -> bool {
        id < self.next && !self.live.contains_key(&id)
    }

    pub(super) fn release(&mut self, id: u32, entity: bool) {
        if let Some(mut fields) = self.live.remove(&id) {
            if entity {
                for name in [
                    "classname",
                    "origin",
                    "model",
                    "spawnflags",
                    "target",
                    "targetname",
                    "count",
                    "health",
                    "dmg",
                    "angles",
                ] {
                    fields.remove(name);
                }
            }
            if !fields.is_empty() {
                self.awaiting_flush.insert(id, fields);
            }
        }
    }

    pub(super) fn flush_released(&mut self) {
        self.awaiting_flush.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vm::ScriptWorld;

    #[test]
    fn released_objects_preserve_only_script_fields_until_tick_end() -> io::Result<()> {
        let mut world = ScriptWorld::default();
        let (slot, object) = world.spawn_entity(b"script_origin", [1., 2., 3.], 0)?;
        world.set_field(object, "userfield", Value::Int(9))?;
        world.free_entity(slot, 0)?;
        assert!(world.objects.is_released(object));
        assert_eq!(world.field(object, "origin")?, Value::Undefined);
        world.set_field(object, "userfield", Value::Int(2))?;
        world.set_field(object, "newfield", Value::Int(3))?;
        assert_eq!(world.field(object, "userfield")?, Value::Int(9));
        assert_eq!(world.field(object, "newfield")?, Value::Undefined);
        world.objects.flush_released();
        assert_eq!(world.field(object, "userfield")?, Value::Undefined);
        let (reused, fresh) = world.spawn_entity(b"script_origin", [0.; 3], 500)?;
        assert_eq!(slot, reused);
        assert_ne!(object, fresh);
        assert_eq!(world.objects.live.len(), 2); // level plus the new entity
        assert!(world.objects.awaiting_flush.is_empty());
        Ok(())
    }
}
