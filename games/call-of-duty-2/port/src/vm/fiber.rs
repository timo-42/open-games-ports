//! Resumable GSC execution. Call frames and operand stacks are retained on
//! waits; a thread call runs its initial segment immediately in the caller.
mod usage;
use super::bytecode::{self, Op, Place};
use super::scheduler::{Event, Resume, Scheduler, ThreadId, Wait, wait_ticks};
use super::value::{ScriptFault, Value, binary, error, script_error};
use crate::script::ScriptProgram;
use std::{collections::BTreeMap, io, sync::Arc};

pub trait Host {
    /// ScrRuntime_Error may escalate a normal script error when developer
    /// error handling is enabled. Missing implementations always propagate.
    fn abort_script_errors(&self) -> bool {
        false
    }
    fn trace_script_errors(&self) -> bool {
        true
    }
    fn flush_released_objects(&mut self) {}
    fn script_loading(&self) -> bool {
        false
    }
    fn global(&self, name: &str) -> io::Result<Value>;
    fn set_global(&mut self, name: &str, value: Value) -> io::Result<()>;
    fn field(&self, object: u32, name: &str) -> io::Result<Value>;
    fn set_field(&mut self, object: u32, name: &str, value: Value) -> io::Result<()>;
    fn builtin(&mut self, receiver: Option<u32>, name: &str, args: Vec<Value>)
    -> io::Result<Value>;
    fn builtin_with_vm(
        &mut self,
        _vm: &mut FiberVm,
        _program: &ScriptProgram,
        receiver: Option<u32>,
        name: &str,
        args: Vec<Value>,
    ) -> io::Result<Value> {
        self.builtin(receiver, name, args)
    }
    fn animation(&self, _module: &str, name: &str) -> io::Result<Value> {
        Err(error(format!("animation binding unavailable for %{name}")))
    }
    fn animtree(&self, module: &str) -> io::Result<Value> {
        Err(error(format!(
            "animation tree binding unavailable for {module}"
        )))
    }
}

#[derive(Debug)]
struct Code {
    module: String,
    function: String,
    parameters: Vec<String>,
    ops: Vec<Op>,
}

#[derive(Debug)]
struct Frame {
    code: Arc<Code>,
    pc: usize,
    receiver: u32,
    locals: BTreeMap<String, Value>,
    stack: Vec<Value>,
    captures: Vec<String>,
}

impl Frame {
    fn new(code: Arc<Code>, receiver: u32, args: Vec<Value>) -> Self {
        let locals = code.parameters.iter().cloned().zip(args).collect();
        Self {
            code,
            pc: 0,
            receiver,
            locals,
            stack: vec![],
            captures: vec![],
        }
    }
    fn pop(&mut self) -> io::Result<Value> {
        self.stack
            .pop()
            .ok_or_else(|| error("GSC operand stack underflow"))
    }
    fn arguments(&mut self, count: usize) -> io::Result<Vec<Value>> {
        (0..count).map(|_| self.pop()).collect()
    }
    fn load(&self, host: &impl Host, name: &str) -> io::Result<Value> {
        match name {
            "self" => Ok(Value::Object(self.receiver)),
            "level" | "game" | "anim" => host.global(name),
            _ => Ok(self.locals.get(name).cloned().unwrap_or_default()),
        }
    }
    fn store(
        &mut self,
        host: &mut impl Host,
        place: &Place,
        indices: usize,
        step: Option<bool>,
    ) -> io::Result<()> {
        let object = match place {
            Place::Field(_) => Some(as_object(self.pop()?)?),
            _ => None,
        };
        let indices = self.arguments(indices)?;
        let mut value = if step.is_none() {
            self.pop()?
        } else {
            Value::Undefined
        };
        if !indices.is_empty() || step.is_some() {
            let mut base = match place {
                Place::Name(name) => self.load(host, name)?,
                Place::Field(field) => host.field(object.expect("field receiver"), field)?,
            };
            let mut parents = Vec::new();
            let count = indices.len();
            for (position, index) in indices.into_iter().enumerate() {
                let child = if (position + 1 < count || step.is_some()) && base != Value::Undefined
                {
                    base.index(index.clone())?
                } else {
                    Value::Undefined
                };
                parents.push((base, index));
                base = child;
            }
            if let Some(increment) = step {
                let Value::Int(n) = base else {
                    return Err(script_error("++ and -- require an int"));
                };
                value = Value::Int(if increment {
                    n.wrapping_add(1)
                } else {
                    n.wrapping_sub(1)
                });
            }
            while let Some((mut base, index)) = parents.pop() {
                base.write_index(index, value)?;
                value = base;
            }
        }
        match place {
            Place::Name(name) => match name.as_str() {
                "game" => host.set_global(name, value)?,
                "self" | "level" | "anim" => return Err(error(format!("cannot assign {name}"))),
                _ => {
                    self.locals.insert(name.clone(), value);
                }
            },
            Place::Field(name) => host.set_field(object.expect("field receiver"), name, value)?,
        }
        Ok(())
    }

    /// Collapse just the failing instruction's operands. Arguments for a
    /// surrounding expression or function call stay on the operand stack.
    fn repair(&mut self, op: &Op, before: usize, fault: &ScriptFault) -> bool {
        let (consumed, replacement) = match op {
            Op::Vector => (3, Some(Value::Undefined)),
            Op::Field(_) | Op::Size | Op::Unary(_) | Op::CastBool => (1, Some(Value::Undefined)),
            Op::Index => (2, Some(Value::Undefined)),
            Op::Binary(_) => (2, Some(fault.result.clone())),
            Op::DeveloperCall { argc, .. } => (*argc, Some(Value::Undefined)),
            Op::Call { argc, method, .. } => {
                (1 + argc + usize::from(*method), Some(Value::Undefined))
            }
            Op::Store {
                owner,
                indices,
                step,
            } => (
                usize::from(matches!(owner, Place::Field(_)))
                    + indices
                    + usize::from(step.is_none()),
                None,
            ),
            // A failed condition consumes the operand and falls through.
            Op::JumpTrue(_) | Op::JumpFalse(_) | Op::Wait => (1, None),
            Op::Switch { default, .. } => {
                self.pc = *default;
                (1, None)
            }
            Op::WaitTill(names) => {
                // OP_SafeSetWaittillVariableFieldCached at the next PC sees
                // the code-position marker when there are no notify arguments.
                for name in names {
                    self.locals.remove(name);
                }
                self.captures.clear();
                (2, None)
            }
            Op::EndOn => (2, None),
            Op::Notify(n) | Op::WaitTillMatch(n) => (2 + n, None),
            _ => return false,
        };
        let Some(base) = before.checked_sub(consumed) else {
            return false;
        };
        if self.stack.len() < base {
            return false;
        }
        self.stack.truncate(base);
        if let Some(value) = replacement {
            self.stack.push(value);
        }
        true
    }

    fn event(&mut self) -> io::Result<Event> {
        let object = as_object(self.pop()?)?;
        let Value::String(raw) = self.pop()? else {
            return Err(script_error("first parameter must evaluate to a string"));
        };
        Ok(Event {
            object,
            name: raw.into_iter().map(char::from).collect(),
        })
    }
}

fn as_object(value: Value) -> io::Result<u32> {
    match value {
        Value::Object(id) => Ok(id),
        other => Err(script_error(format!(
            "{} is not an object",
            other.type_name()
        ))),
    }
}

#[derive(Debug)]
enum Control {
    Next,
    Terminate,
    Return(Value),
    Call(Frame),
    Wait(Wait<Value>),
}

/// One instance belongs to one loaded script program and its object world.
#[derive(Debug, Default)]
pub struct FiberVm {
    scheduler: Scheduler<Vec<Frame>, Value>,
    compiled: BTreeMap<(String, String), Arc<Code>>,
    pub diagnostics: Vec<String>,
}

impl FiberVm {
    pub fn tick(&self) -> u32 {
        self.scheduler.tick()
    }

    fn code(
        &mut self,
        program: &ScriptProgram,
        module: &str,
        name: &str,
    ) -> io::Result<Option<Arc<Code>>> {
        let Some((module, function)) = program.resolve_function(module, name) else {
            return Ok(None);
        };
        let key = (module.to_owned(), name.to_owned());
        if let Some(cached) = self.compiled.get(&key) {
            return Ok(Some(cached.clone()));
        }
        let code = Arc::new(Code {
            module: key.0.clone(),
            function: key.1.clone(),
            parameters: function.parameters.clone(),
            ops: bytecode::compile(function)?,
        });
        self.compiled.insert(key, code.clone());
        Ok(Some(code))
    }

    /// OP_checkclearparams discards surplus arguments and leaves the already
    /// bound formal parameters intact when it resumes after its script error.
    fn enter(
        &mut self,
        host: &impl Host,
        code: Arc<Code>,
        owner: u32,
        args: Vec<Value>,
    ) -> io::Result<Frame> {
        if args.len() > code.parameters.len() {
            let message = format!(
                "{}::{}: function called with too many parameters",
                code.module, code.function
            );
            if host.abort_script_errors() {
                return Err(error(message));
            }
            if host.trace_script_errors() {
                self.diagnostics.push(message);
            }
        }
        Ok(Frame::new(code, owner, args))
    }

    pub fn invoke(
        &mut self,
        program: &ScriptProgram,
        host: &mut impl Host,
        module: &str,
        name: &str,
        receiver: u32,
        args: Vec<Value>,
    ) -> io::Result<Value> {
        let code = self
            .code(program, module, name)?
            .ok_or_else(|| error(format!("unknown script function {module}::{name}")))?;
        let frame = self.enter(host, code, receiver, args)?;
        let thread = self.scheduler.start(receiver);
        self.run(
            program,
            host,
            thread,
            vec![frame],
            0,
            &mut std::time::Instant::now(),
        )
    }

    pub fn notify(&mut self, object: u32, name: &str, args: &[Value]) {
        let diagnostics = &mut self.diagnostics;
        self.scheduler.notify_by(
            &Event {
                object,
                name: name.to_owned(),
            },
            args,
            |a, b| match binary("==", a.clone(), b.clone()).and_then(|v| v.truthy()) {
                Ok(equal) => equal,
                Err(e) => {
                    diagnostics.push(format!("waittillmatch: {e}"));
                    false
                }
            },
        );
    }

    pub fn run_current(&mut self, program: &ScriptProgram, host: &mut impl Host) -> io::Result<()> {
        let mut loop_start = std::time::Instant::now();
        while let Some((thread, mut frames, resume)) = self.scheduler.pop_ready() {
            match resume {
                Resume::Notify(args) => {
                    let frame = frames.last_mut().expect("suspended call frame");
                    let mut values = args.into_iter();
                    for name in frame.captures.drain(..) {
                        frame.locals.insert(name, values.next().unwrap_or_default());
                    }
                }
                Resume::Unwind(depth) => {
                    frames.truncate(depth);
                    if let Some(caller) = frames.last_mut() {
                        caller.stack.push(Value::Undefined);
                    }
                }
                Resume::Time => {}
            }
            self.run(program, host, thread, frames, 0, &mut loop_start)?;
        }
        Ok(())
    }

    pub fn advance(&mut self, program: &ScriptProgram, host: &mut impl Host) -> io::Result<()> {
        self.run_current(program, host)?;
        host.flush_released_objects();
        self.scheduler.advance_tick()
    }

    fn run(
        &mut self,
        program: &ScriptProgram,
        host: &mut impl Host,
        thread: ThreadId,
        mut frames: Vec<Frame>,
        outer_depth: usize,
        loop_start: &mut std::time::Instant,
    ) -> io::Result<Value> {
        loop {
            if let Some(depth) = self.scheduler.cancelled_scope(thread) {
                frames.truncate(depth);
                if let Some(caller) = frames.last_mut() {
                    caller.stack.push(Value::Undefined);
                }
            }
            let depth = frames.len();
            let Some(frame) = frames.last_mut() else {
                self.scheduler.finish(thread);
                return Ok(Value::Undefined);
            };
            if depth + outer_depth > 31 {
                self.scheduler.finish(thread);
                return Err(error(
                    "script stack overflow (too many embedded function calls)",
                ));
            }
            let stack_depth = frame.stack.len();
            let pc = frame.pc;
            let code = frame.code.clone();
            let control = code
                .ops
                .get(pc)
                .ok_or_else(|| error("invalid script instruction pointer"))
                .and_then(|op| {
                    frame.pc += 1;
                    self.instruction(
                        program,
                        host,
                        thread,
                        frame,
                        op,
                        depth,
                        outer_depth,
                        loop_start,
                    )
                });
            match control {
                Ok(Control::Next) => {}
                Ok(Control::Terminate) => {
                    self.scheduler.finish(thread);
                    return Ok(Value::Undefined);
                }
                Ok(Control::Call(child)) => frames.push(child),
                Ok(Control::Return(value)) => {
                    self.scheduler.leave_scope(thread, depth - 1);
                    frames.pop();
                    if let Some(caller) = frames.last_mut() {
                        caller.stack.push(value);
                    } else {
                        self.scheduler.finish(thread);
                        return Ok(value);
                    }
                }
                Ok(Control::Wait(wait)) => {
                    if matches!(wait,Wait::Ticks(ticks) if ticks != 0) {
                        *loop_start = std::time::Instant::now();
                    }
                    if let Err(e) = self.scheduler.suspend(thread, frames, wait) {
                        self.scheduler.finish(thread);
                        return Err(error(format!(
                            "{}::{} at op {pc}: {e}",
                            code.module, code.function
                        )));
                    }
                    return Ok(Value::Undefined);
                }
                Err(e) => {
                    if let Some(fault) = ScriptFault::from_error(&e)
                        && !host.abort_script_errors()
                        && let Some(op) = code.ops.get(pc)
                        && frame.repair(op, stack_depth, fault)
                    {
                        if host.trace_script_errors() {
                            self.diagnostics.push(format!(
                                "{}::{} at op {pc}: {e}",
                                code.module, code.function
                            ));
                        }
                        continue;
                    }
                    self.scheduler.finish(thread);
                    return Err(error(format!(
                        "{}::{} at op {pc}: {e}",
                        code.module, code.function
                    )));
                }
            }
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn instruction(
        &mut self,
        program: &ScriptProgram,
        host: &mut impl Host,
        thread: ThreadId,
        frame: &mut Frame,
        op: &Op,
        depth: usize,
        outer_depth: usize,
        loop_start: &mut std::time::Instant,
    ) -> io::Result<Control> {
        let value = match op {
            Op::Push(v) => v.clone(),
            Op::Load(name) => frame.load(host, name)?,
            Op::Function { module, name } => Value::Function {
                module: Some(
                    module
                        .clone()
                        .unwrap_or_else(|| frame.code.module.clone())
                        .replace('\\', "/"),
                ),
                name: name.clone(),
            },
            Op::Animation { tree, name } => host.animation(tree, name)?,
            Op::AnimTree(tree) => host.animtree(tree)?,
            Op::Vector => Value::Vector([
                frame.pop()?.number()?,
                frame.pop()?.number()?,
                frame.pop()?.number()?,
            ]),
            Op::Field(field) => {
                let object = as_object(frame.pop()?)?;
                host.field(object, field)?
            }
            Op::Index => {
                let source = frame.pop()?;
                source.index(frame.pop()?)?
            }
            Op::Size => Value::Int(frame.pop()?.size()?),
            Op::Unary(operator) => match (operator.as_str(), frame.pop()?) {
                ("!", v) => Value::Int(i32::from(!v.truthy()?)),
                ("~", Value::Int(n)) => Value::Int(!n),
                ("-", Value::Int(n)) => Value::Int(n.wrapping_neg()),
                ("-", Value::Float(n)) => Value::Float(-n),
                (op, v) => {
                    return Err(script_error(format!(
                        "{op} cannot be applied to {}",
                        v.type_name()
                    )));
                }
            },
            Op::CastBool => Value::Int(i32::from(frame.pop()?.truthy()?)),
            Op::Binary(op) => {
                let right = frame.pop()?;
                binary(op, frame.pop()?, right)?
            }
            Op::DeveloperCall { name, argc } => {
                let arguments = frame.arguments(*argc)?;
                host.builtin_with_vm(self, program, None, name, arguments)?
            }
            Op::Call {
                argc,
                method,
                threaded,
            } => {
                let Value::Function { module, name } = frame.pop()? else {
                    return Err(script_error("expected a function pointer"));
                };
                let receiver = if *method {
                    Some(as_object(frame.pop()?)?)
                } else {
                    None
                };
                let arguments = frame.arguments(*argc)?;
                let module = module.as_deref().unwrap_or(&frame.code.module);
                if let Some(code) = self.code(program, module, &name)? {
                    if depth + outer_depth >= 31 {
                        return Err(script_error(
                            "script stack overflow (too many embedded function calls)",
                        ));
                    }
                    let owner = receiver.unwrap_or(frame.receiver);
                    let child = self.enter(host, code, owner, arguments)?;
                    if !threaded {
                        return Ok(Control::Call(child));
                    }
                    let new_thread = self.scheduler.start(owner);
                    self.run(
                        program,
                        host,
                        new_thread,
                        vec![child],
                        outer_depth + depth,
                        loop_start,
                    )?
                } else if receiver.is_none() && name == "resettimeout" {
                    *loop_start = std::time::Instant::now();
                    Value::Undefined
                } else {
                    host.builtin_with_vm(self, program, receiver, &name, arguments)?
                }
            }
            Op::Store {
                owner,
                indices,
                step,
            } => {
                frame.store(host, owner, *indices, *step)?;
                return Ok(Control::Next);
            }
            Op::Pop => {
                frame.pop()?;
                return Ok(Control::Next);
            }
            Op::Jump(to) => {
                // OP_jumpback checks the wall clock only on a back edge.
                if *to < frame.pc && loop_start.elapsed().as_millis() >= 2500 {
                    // Loop warnings appear on the server console even while a
                    // loading script is still running this frame.
                    eprintln!(
                        "{}::{}: potential infinite loop in script",
                        frame.code.module, frame.code.function
                    );
                    *loop_start = std::time::Instant::now();
                    if !host.script_loading() {
                        if host.abort_script_errors() {
                            return Err(error("potential infinite loop in script"));
                        }
                        return Ok(Control::Terminate);
                    }
                }
                frame.pc = *to;
                return Ok(Control::Next);
            }
            Op::JumpFalse(to) | Op::JumpTrue(to) => {
                if frame.pop()?.truthy()? == matches!(op, Op::JumpTrue(_)) {
                    frame.pc = *to;
                }
                return Ok(Control::Next);
            }
            Op::Switch { cases, default } => {
                let key = frame.pop()?;
                if let Value::Int(index) = key
                    && !(-0x7e0002..0x800000).contains(&index)
                {
                    return Err(script_error(format!("switch index {index} out of range")));
                }
                if !matches!(key, Value::Int(_) | Value::String(_)) {
                    return Err(script_error("switch requires an integer or string"));
                }
                frame.pc = cases
                    .iter()
                    .find(|(label, _)| *label == key)
                    .map_or(*default, |(_, to)| *to);
                return Ok(Control::Next);
            }
            Op::Return => return Ok(Control::Return(frame.pop()?)),
            Op::Wait => {
                let ticks = match frame.pop()? {
                    Value::Int(i) => u32::try_from(i.wrapping_mul(20))
                        .map_err(|_| script_error("negative wait is not allowed"))?,
                    Value::Float(f) => wait_ticks(f).map_err(|e| script_error(e.to_string()))?,
                    v => {
                        return Err(script_error(format!(
                            "type {} is not a float",
                            v.type_name()
                        )));
                    }
                };
                if ticks > 0xff_fffe {
                    return Err(script_error("wait is too long"));
                }
                return Ok(Control::Wait(Wait::Ticks(ticks)));
            }
            Op::WaitFrameEnd => return Ok(Control::Wait(Wait::FrameEnd)),
            Op::WaitTill(names) => {
                frame.captures.clone_from(names);
                return Ok(Control::Wait(Wait::Event(frame.event()?)));
            }
            Op::WaitTillMatch(argc) => {
                let event = frame.event()?;
                return Ok(Control::Wait(Wait::Match(event, frame.arguments(*argc)?)));
            }
            Op::Notify(argc) => {
                let event = frame.event()?;
                self.notify(event.object, &event.name, &frame.arguments(*argc)?);
                return Ok(Control::Next);
            }
            Op::EndOn => {
                self.scheduler.endon(thread, depth - 1, frame.event()?);
                return Ok(Control::Next);
            }
        };
        frame.stack.push(value);
        Ok(Control::Next)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct TraceHost {
        strict: bool,
        fields: BTreeMap<(u32, String), Value>,
        trace: Vec<Vec<u8>>,
    }

    impl Host for TraceHost {
        fn abort_script_errors(&self) -> bool {
            self.strict
        }

        fn global(&self, name: &str) -> io::Result<Value> {
            match name {
                "level" => Ok(Value::Object(0)),
                _ => Err(error("unknown test global")),
            }
        }
        fn set_global(&mut self, _: &str, _: Value) -> io::Result<()> {
            Err(error("unknown test global"))
        }
        fn field(&self, id: u32, name: &str) -> io::Result<Value> {
            Ok(self
                .fields
                .get(&(id, name.to_owned()))
                .cloned()
                .unwrap_or_default())
        }
        fn set_field(&mut self, id: u32, name: &str, value: Value) -> io::Result<()> {
            self.fields.insert((id, name.to_owned()), value);
            Ok(())
        }
        fn builtin(
            &mut self,
            _receiver: Option<u32>,
            name: &str,
            args: Vec<Value>,
        ) -> io::Result<Value> {
            match (name, args.as_slice()) {
                ("logprint", [Value::String(text)]) => {
                    self.trace.push(text.clone());
                    Ok(Value::Undefined)
                }
                ("isdefined", [value]) => Ok(Value::Int(i32::from(*value != Value::Undefined))),
                _ => super::super::builtins::dispatch(
                    name,
                    &args,
                    &mut crate::config::ServerConfig::from_args(&[]),
                    0,
                )?
                .ok_or_else(|| error(format!("unknown test binding {name}"))),
            }
        }
    }

    fn program(source: &[u8]) -> ScriptProgram {
        ScriptProgram::discover("test","mp_test",false,|p| Ok(match p{
            "codescripts/delete.gsc"=>b"main() {}".to_vec(),
            "codescripts/struct.gsc"=>b"initStructs() {} createStruct() {}".to_vec(),
            "maps/mp/gametypes/_callbacksetup.gsc"=>b"codecallback_startgametype(){} codecallback_playerconnect(){} codecallback_playerdisconnect(){} codecallback_playerdamage(){} codecallback_playerkilled(){}".to_vec(),
            "maps/mp/gametypes/test.gsc"=>source.to_vec(),
            _=>return Err(io::ErrorKind::NotFound.into()),
        })).expect("script compiles")
    }

    #[test]
    fn recover_script_errors_and_resume_with_original_results() -> io::Result<()> {
        // The Linux 1.3 server logged these values and control-flow decisions
        // in a synthetic GSC map overlay with developer and logfile disabled.
        let script = program(
            br#"
            main() {
                x = 2; zero = x - x; s = "s";
                level.quotient = x / zero;
                level.remainder = x % zero;
                level.bad = isdefined(x * s) + isdefined(s[42]) + isdefined(x.size)
                    + isdefined((s,1,2)) + isdefined(!s) + isdefined(level.absent.value)
                    + isdefined(vectornormalize(s)) + isdefined(acos(x))
                    + isdefined(setcvar("bad var","bad"));
                if (s) { logprint("then"); } else { logprint("else"); }
                a = x; a[1] = 4; level.original = a;
                wait -1; a notify("x",42);
                first_output=21; second_output=22;
                a waittill("test", first_output, second_output);
                level.wait_outputs = isdefined(first_output) + isdefined(second_output);
                a waittillmatch("go", 11); a endon("killed"); level notify(3, "arg", a);
                level.call = isdefined([[x]](s));
                switch(s.size * 1.5) { case 2: break; default: logprint("default"); break; }
                level.or = s || effect(0); level.and = s && effect(1);
                level.rhs = isdefined(0 || s) + isdefined(1 && s);
                level.extra = first(x, 26);
                s++; level.same = s;
                logprint("end");
            }
            effect(i) { logprint("effect"+i); return i; }
            first(i) { return i; }
        "#,
        );
        let mut vm = FiberVm::default();
        let mut host = TraceHost::default();
        vm.invoke(
            &script,
            &mut host,
            "maps/mp/gametypes/test",
            "main",
            0,
            vec![],
        )?;
        assert_eq!(
            host.trace,
            ["then", "default", "effect0", "effect1", "end"].map(str::as_bytes)
        );
        assert_eq!(host.field(0, "quotient")?, Value::Float(0.));
        for (name, number) in [
            ("remainder", 0),
            ("bad", 0),
            ("original", 2),
            ("wait_outputs", 0),
            ("call", 0),
            ("or", 0),
            ("and", 1),
            ("rhs", 0),
            ("extra", 2),
        ] {
            assert_eq!(host.field(0, name)?, Value::Int(number), "{name}");
        }
        assert_eq!(host.field(0, "same")?, Value::String(b"s".to_vec()));
        assert_eq!(vm.diagnostics.len(), 27);
        Ok(())
    }

    #[test]
    fn recursive_calls_recover_at_the_original_frame_limit() -> io::Result<()> {
        let script = program(
            br#"main() {
            prof_begin("recursion"); breakpoint; prof_end("recursion");
            level.result = recurse(0);
        }
        recurse(i) {
            level.depth = i;
            if (i > 50) return 99;
            r = recurse(i+1);
            if (!isdefined(r)) return 17;
            return r;
        }"#,
        );
        let mut vm = FiberVm::default();
        let mut host = TraceHost::default();
        vm.invoke(
            &script,
            &mut host,
            "maps/mp/gametypes/test",
            "main",
            0,
            vec![],
        )?;
        assert_eq!(host.field(0, "depth")?, Value::Int(29));
        assert_eq!(host.field(0, "result")?, Value::Int(17));
        assert_eq!(vm.diagnostics.len(), 1);
        Ok(())
    }

    #[test]
    fn fatal_script_error_stops_before_the_next_statement() {
        let script = program(
            br#"main() {
            logprint("before");
            v = 0; x = 42 / v;
            logprint("after");
        }"#,
        );
        let mut vm = FiberVm::default();
        let mut host = TraceHost {
            strict: true,
            ..Default::default()
        };
        let err = vm
            .invoke(
                &script,
                &mut host,
                "maps/mp/gametypes/test",
                "main",
                0,
                vec![],
            )
            .expect_err("developer mode stops the script");
        assert!(err.to_string().contains("divide by 0"));
        assert_eq!(host.trace, [b"before"]);
    }

    #[test]
    fn missing_native_binding_still_stops_the_vm() {
        let source =
            program(br#"main() { logprint("before"); absent_native(31); logprint("after"); }"#);
        let mut vm = FiberVm::default();
        let mut host = TraceHost::default();
        let result = vm.invoke(
            &source,
            &mut host,
            "maps/mp/gametypes/test",
            "main",
            0,
            vec![],
        );
        assert!(
            result
                .expect_err("script execution must fail")
                .to_string()
                .contains("unknown test binding absent_native")
        );
        assert_eq!(host.trace, [b"before"]);
        assert!(vm.diagnostics.is_empty());
    }

    #[test]
    fn original_scheduler_trace_through_compiler_and_interpreter() {
        // A stock Linux 1.3 server ran this synthetic map script's sequence
        // against mp_toujane in a disposable fs_game source layer.
        let script=program(br#"
            main() {
                level thread delayed("a"); level thread delayed("b");
                level thread on_event("c"); level thread on_event("d");
                level thread at_end("e"); level thread at_end("f");
                level thread cancelled(); level thread nested();
                logprint("start"); level notify("go",31); level notify("kill"); logprint("post_notify");
            }
            delayed(s) { wait 0; logprint(s); }
            on_event(s) { level waittill("go",n); logprint(s+n); }
            at_end(s) { waittillframeend; logprint(s); }
            cancelled() { level endon("kill"); wait .1; logprint("unexpected"); }
            nested() { leaf(); logprint("parent"); }
            leaf() { level endon("kill"); wait .05; logprint("badleaf"); }
        "#);
        let mut vm = FiberVm::default();
        let mut host = TraceHost::default();
        vm.invoke(
            &script,
            &mut host,
            "maps/mp/gametypes/test",
            "main",
            0,
            vec![],
        )
        .expect("entry");
        for _ in 0..3 {
            vm.advance(&script, &mut host).expect("frame");
        }
        let expected = [
            "start",
            "post_notify",
            "parent",
            "d31",
            "c31",
            "b",
            "a",
            "e",
            "f",
        ];
        assert_eq!(host.trace, expected.map(|s| s.as_bytes()));
    }

    #[test]
    fn call_preserves_partially_evaluated_expression_through_wait() {
        let script = program(
            br#"
            main() {
                a[2][3] = 7;
                a[2][3]++;
                level.result = a[2][3] * yield_value();
            }
            yield_value() { wait .05; return 3; }
        "#,
        );
        let mut vm = FiberVm::default();
        let mut host = TraceHost::default();
        vm.invoke(
            &script,
            &mut host,
            "maps/mp/gametypes/test",
            "main",
            0,
            vec![],
        )
        .expect("initial yield");
        assert_eq!(host.field(0, "result").expect("field"), Value::Undefined);
        vm.advance(&script, &mut host).expect("time");
        vm.run_current(&script, &mut host).expect("resume");
        assert_eq!(host.field(0, "result").expect("field"), Value::Int(24));
    }

    #[test]
    fn self_endon_stops_current_thread_before_next_opcode() {
        let script = program(br#"main() {level endon("done"); level notify("done"); unknown(); }"#);
        let mut vm = FiberVm::default();
        vm.invoke(
            &script,
            &mut TraceHost::default(),
            "maps/mp/gametypes/test",
            "main",
            0,
            vec![],
        )
        .expect("self-cancelled");
    }
}
