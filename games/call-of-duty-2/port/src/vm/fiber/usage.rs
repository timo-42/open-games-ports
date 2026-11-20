//! Operator diagnostics for the native VM's suspended threads and retained strings.
use super::{FiberVm, Value};
use crate::vm::{ScriptWorld, value::ArrayKey};
use std::{collections::BTreeMap, fmt::Write};

#[derive(Default)]
struct StringUsage {
    strings: usize,
    bytes: usize,
}

impl StringUsage {
    fn string(&mut self, bytes: usize) {
        self.strings += 1;
        self.bytes += bytes;
    }

    fn value(&mut self, value: &Value) {
        match value {
            Value::String(value) | Value::LocalizedString(value) => self.string(value.len()),
            Value::Array(values) => {
                for (key, value) in values {
                    if let ArrayKey::String(key) = key {
                        self.string(key.len());
                    }
                    self.value(value);
                }
            }
            Value::Function { module, name } => {
                if let Some(module) = module {
                    self.string(module.len());
                }
                self.string(name.len());
            }
            _ => {}
        }
    }

    fn fields(&mut self, fields: &BTreeMap<String, Value>) {
        for (name, value) in fields {
            self.string(name.len());
            self.value(value);
        }
    }
}

impl FiberVm {
    pub fn script_usage(&self) -> String {
        let mut groups = BTreeMap::new();
        for (thread, frames) in self.scheduler.suspended() {
            let stack = frames
                .iter()
                .rev()
                .map(|f| {
                    (
                        f.code.module.as_str(),
                        f.code.function.as_str(),
                        f.pc.saturating_sub(1),
                    )
                })
                .collect::<Vec<_>>();
            let (count, locals, operands, endons) = groups.entry(stack).or_insert((0, 0, 0, 0));
            *count += 1;
            *locals += frames.iter().map(|f| f.locals.len()).sum::<usize>();
            *operands += frames.iter().map(|f| f.stack.len()).sum::<usize>();
            *endons += self.scheduler.endon_count(thread);
        }
        let mut text = String::from("Native VM suspended script threads\n");
        for (stack, (count, locals, operands, endons)) in groups {
            writeln!(text,"count: {count}, locals: {locals}, stack values: {operands}, endon listeners: {endons}").expect("formatting into a String is infallible");
            for (depth, (module, function, pc)) in stack.iter().enumerate() {
                if depth != 0 {
                    text.push_str("called from:\n");
                }
                writeln!(text, "{module}::{function} at instruction {pc}")
                    .expect("formatting into a String is infallible");
            }
        }
        text
    }

    pub fn string_usage(&self, world: &ScriptWorld) -> String {
        let mut objects = StringUsage::default();
        objects.value(&world.game);
        for fields in world.objects.retained_fields() {
            objects.fields(fields);
        }
        let mut suspended = StringUsage::default();
        for (_, frames) in self.scheduler.suspended() {
            for frame in frames {
                suspended.fields(&frame.locals);
                for value in &frame.stack {
                    suspended.value(value);
                }
                for name in &frame.captures {
                    suspended.string(name.len());
                }
            }
        }
        format!(
            "Native VM retained string payloads\ngame globals and object fields: {} strings, {} bytes\nsuspended frame locals, operands and captures: {} strings, {} bytes\n",
            objects.strings, objects.bytes, suspended.strings, suspended.bytes
        )
    }
}
