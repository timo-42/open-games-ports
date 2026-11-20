//! GSC instructions for the resumable Rust interpreter. Jump targets are
//! instruction indices within one function. The original compiler in chunks
//! 010–012 emits addresses into a byte buffer instead.
use super::value::{Value, error};
use crate::script::ast::{Expr, Function, Statement, SwitchCase};
use std::io;

#[derive(Clone, Debug, PartialEq)]
pub enum Place {
    Name(String),
    Field(String),
}

#[derive(Clone, Debug, PartialEq)]
pub enum Op {
    Push(Value),
    Load(String),
    Function {
        module: Option<String>,
        name: String,
    },
    Animation {
        tree: String,
        name: String,
    },
    AnimTree(String),
    Vector,
    Field(String),
    Index,
    Size,
    Unary(String),
    CastBool,
    Binary(String),
    DeveloperCall {
        name: String,
        argc: usize,
    },
    Call {
        argc: usize,
        method: bool,
        threaded: bool,
    },
    Store {
        owner: Place,
        indices: usize,
        step: Option<bool>,
    },
    Pop,
    Jump(usize),
    JumpFalse(usize),
    JumpTrue(usize),
    Switch {
        cases: Vec<(Value, usize)>,
        default: usize,
    },
    Return,
    Wait,
    WaitFrameEnd,
    WaitTill(Vec<String>),
    WaitTillMatch(usize),
    Notify(usize),
    EndOn,
}

#[derive(Default)]
struct Scope {
    breaks: Vec<usize>,
    // Switches permit break; continue refers to the nearest enclosing loop.
    continues: Option<Vec<usize>>,
}

#[derive(Default)]
struct Compiler {
    code: Vec<Op>,
    scopes: Vec<Scope>,
}

pub fn compile(function: &Function) -> io::Result<Vec<Op>> {
    let mut compiler = Compiler::default();
    compiler.block(&function.body)?;
    compiler.emit(Op::Push(Value::Undefined));
    compiler.emit(Op::Return);
    Ok(compiler.code)
}

impl Compiler {
    fn emit(&mut self, op: Op) -> usize {
        let index = self.code.len();
        self.code.push(op);
        index
    }

    fn patch_jump(&mut self, jump: usize, destination: usize) {
        match &mut self.code[jump] {
            Op::Jump(target) | Op::JumpFalse(target) | Op::JumpTrue(target) => {
                *target = destination
            }
            _ => unreachable!("compiler recorded a non-jump"),
        }
    }

    fn block(&mut self, stmts: &[Statement]) -> io::Result<()> {
        for stmt in stmts {
            self.statement(stmt)?;
        }
        Ok(())
    }

    fn statement(&mut self, stmt: &Statement) -> io::Result<()> {
        use Statement::*;
        match stmt {
            Block(body) => self.block(body)?,
            Expression(expr) => {
                if !self.event_statement(expr)? {
                    self.expr(expr)?;
                    self.emit(Op::Pop);
                }
            }
            Assign {
                target,
                operator,
                value,
            } => {
                // ScrCompile_CompoundAssign reads the target, evaluates rhs,
                // then evaluates the target address again to store the result.
                if operator == "=" {
                    self.expr(value)?;
                } else {
                    self.expr(target)?;
                    self.expr(value)?;
                    let operator = operator
                        .strip_suffix('=')
                        .ok_or_else(|| error("invalid assignment"))?;
                    self.emit(Op::Binary(operator.into()));
                }
                self.store(target, None)?;
            }
            Update { target, increment } => self.store(target, Some(*increment))?,
            If {
                condition,
                then_branch,
                else_branch,
            } => {
                self.expr(condition)?;
                let otherwise = self.emit(Op::JumpFalse(0));
                self.statement(then_branch)?;
                if let Some(other) = else_branch {
                    let done = self.emit(Op::Jump(0));
                    self.patch_jump(otherwise, self.code.len());
                    self.statement(other)?;
                    self.patch_jump(done, self.code.len());
                } else {
                    self.patch_jump(otherwise, self.code.len());
                }
            }
            While { condition, body } => self.loop_body(Some(condition), body, None)?,
            For {
                init,
                condition,
                step,
                body,
            } => {
                if let Some(init) = init {
                    self.statement(init)?;
                }
                self.loop_body(condition.as_ref(), body, step.as_deref())?;
            }
            Switch { value, cases } => self.switch(value, cases)?,
            Return(value) => {
                if let Some(value) = value {
                    self.expr(value)?;
                } else {
                    self.emit(Op::Push(Value::Undefined));
                }
                self.emit(Op::Return);
            }
            Break => {
                let jump = self.emit(Op::Jump(0));
                self.scopes
                    .last_mut()
                    .ok_or_else(|| error("break outside loop or switch"))?
                    .breaks
                    .push(jump);
            }
            Continue => {
                let jump = self.emit(Op::Jump(0));
                self.scopes
                    .iter_mut()
                    .rev()
                    .find_map(|scope| scope.continues.as_mut())
                    .ok_or_else(|| error("continue outside loop"))?
                    .push(jump);
            }
            Wait(value) => {
                self.expr(value)?;
                self.emit(Op::Wait);
            }
            WaitFrameEnd => {
                self.emit(Op::WaitFrameEnd);
            }
            // The dedicated interpreter consumes profile markers without
            // changing script state. Scr_CompileNop omits breakpoint entirely.
            Breakpoint | Profile { .. } => {}
            Empty => {}
        }
        Ok(())
    }

    fn loop_body(
        &mut self,
        condition: Option<&Expr>,
        body: &Statement,
        step: Option<&Statement>,
    ) -> io::Result<()> {
        let head = self.code.len();
        self.scopes.push(Scope {
            continues: Some(Vec::new()),
            ..Scope::default()
        });
        let exit = if let Some(condition) = condition {
            self.expr(condition)?;
            Some(self.emit(Op::JumpFalse(0)))
        } else {
            None
        };
        self.statement(body)?;
        let next = self.code.len();
        if let Some(step) = step {
            self.statement(step)?;
        }
        self.emit(Op::Jump(head));
        let end = self.code.len();
        if let Some(exit) = exit {
            self.patch_jump(exit, end);
        }
        self.finish_scope(next);
        Ok(())
    }

    fn finish_scope(&mut self, next: usize) {
        let scope = self.scopes.pop().expect("loop or switch scope");
        for jump in scope.breaks {
            self.patch_jump(jump, self.code.len());
        }
        for jump in scope.continues.into_iter().flatten() {
            self.patch_jump(jump, next);
        }
    }

    fn switch(&mut self, key: &Expr, cases: &[SwitchCase]) -> io::Result<()> {
        self.expr(key)?;
        let dispatch = self.emit(Op::Switch {
            cases: vec![],
            default: 0,
        });
        let mut labels = Vec::new();
        let mut default = None;
        self.scopes.push(Scope::default());
        for case in cases {
            if let Some(expr) = &case.value {
                let value = case_value(expr)?;
                if labels.iter().any(|(existing, _)| *existing == value) {
                    return Err(error("duplicate switch case"));
                }
                labels.push((value, self.code.len()));
            } else if default.replace(self.code.len()).is_some() {
                return Err(error("duplicate default case"));
            }
            // Case bodies are adjacent. The lack of a break is fall-through.
            self.block(&case.body)?;
        }
        let end = self.code.len();
        self.code[dispatch] = Op::Switch {
            cases: labels,
            default: default.unwrap_or(end),
        };
        self.finish_scope(end);
        Ok(())
    }

    fn store(&mut self, target: &Expr, step: Option<bool>) -> io::Result<()> {
        let mut target = target;
        let mut indices = 0;
        while let Expr::Index(base, index) = target {
            self.expr(index)?;
            indices += 1;
            target = base;
        }
        let owner = match target {
            Expr::Name(name) => Place::Name(name.clone()),
            Expr::Field(base, name) => {
                self.expr(base)?;
                Place::Field(name.clone())
            }
            _ => return Err(error("not an lvalue")),
        };
        self.emit(Op::Store {
            owner,
            indices,
            step,
        });
        Ok(())
    }

    // waittill operands after the event are names to bind on resumption,
    // whereas waittillmatch and notify take values (Scr_CompileWaittill et al.).
    fn event_statement(&mut self, expr: &Expr) -> io::Result<bool> {
        let Expr::Call {
            receiver: Some(receiver),
            target,
            arguments,
            threaded: false,
        } = expr
        else {
            return Ok(false);
        };
        let Expr::Function { module: None, name } = target.as_ref() else {
            return Ok(false);
        };
        if !matches!(
            name.as_str(),
            "waittill" | "waittillmatch" | "notify" | "endon"
        ) {
            return Ok(false);
        }
        let (event, rest) = arguments
            .split_first()
            .ok_or_else(|| error("missing event name"))?;
        let op = match name.as_str() {
            "waittill" => {
                let names = rest
                    .iter()
                    .map(|expr| match expr {
                        Expr::Name(name) if !matches!(name.as_str(), "self" | "level" | "game") => {
                            Ok(name.clone())
                        }
                        _ => Err(error("waittill output must be a local variable")),
                    })
                    .collect::<io::Result<Vec<_>>>()?;
                Op::WaitTill(names)
            }
            "endon" => {
                if !rest.is_empty() {
                    return Err(error("endon requires one event name"));
                }
                Op::EndOn
            }
            _ => {
                for arg in rest.iter().rev() {
                    self.expr(arg)?;
                }
                if name == "notify" {
                    Op::Notify(rest.len())
                } else {
                    Op::WaitTillMatch(rest.len())
                }
            }
        };
        self.expr(event)?;
        self.expr(receiver)?;
        self.emit(op);
        Ok(true)
    }

    fn expr(&mut self, expr: &Expr) -> io::Result<()> {
        use Expr::*;
        let op = match expr {
            Undefined => Op::Push(Value::Undefined),
            Integer(n) => Op::Push(Value::Int(*n)),
            Bool(b) => Op::Push(Value::Int(i32::from(*b))),
            Float(n) => Op::Push(Value::Float(*n)),
            String(s) => Op::Push(Value::String(s.clone())),
            LocalizedString(s) => Op::Push(Value::LocalizedString(s.clone())),
            EmptyArray => Op::Push(Value::Array(Default::default())),
            Name(n) => Op::Load(n.clone()),
            Function { module, name } => Op::Function {
                module: module.clone(),
                name: name.clone(),
            },
            Animation { tree, name } => Op::Animation {
                tree: tree.clone(),
                name: name.clone(),
            },
            AnimTree(tree) => Op::AnimTree(tree.clone()),
            Field(base, name) => {
                self.expr(base)?;
                Op::Field(name.clone())
            }
            Index(base, index) => {
                self.expr(index)?;
                self.expr(base)?;
                Op::Index
            }
            Size(e) => {
                self.expr(e)?;
                Op::Size
            }
            Vector(components) => {
                for component in components.iter().rev() {
                    self.expr(component)?;
                }
                Op::Vector
            }
            Unary { operator, value } => {
                self.expr(value)?;
                Op::Unary(operator.clone())
            }
            Binary {
                operator,
                left,
                right,
            } if operator == "&&" || operator == "||" => {
                self.logical(operator, left, right)?;
                return Ok(());
            }
            Binary {
                operator,
                left,
                right,
            } => {
                self.expr(left)?;
                self.expr(right)?;
                Op::Binary(operator.clone())
            }
            DeveloperCall { name, arguments } => {
                if arguments.len() > 255 {
                    return Err(error("exceeded argument limit"));
                }
                for arg in arguments.iter().rev() {
                    self.expr(arg)?;
                }
                Op::DeveloperCall {
                    name: name.clone(),
                    argc: arguments.len(),
                }
            }
            Call {
                receiver,
                target,
                arguments,
                threaded,
            } => {
                if arguments.len() > 255 {
                    return Err(error("exceeded argument limit"));
                }
                // ScrCompile_{Function,Method}Call compile arguments right to
                // left, followed by the method receiver, then a dynamic target.
                for arg in arguments.iter().rev() {
                    self.expr(arg)?;
                }
                if let Some(receiver) = receiver {
                    self.expr(receiver)?;
                }
                self.expr(target)?;
                Op::Call {
                    argc: arguments.len(),
                    method: receiver.is_some(),
                    threaded: *threaded,
                }
            }
        };
        self.emit(op);
        Ok(())
    }

    fn logical(&mut self, operator: &str, lhs: &Expr, rhs: &Expr) -> io::Result<()> {
        self.expr(lhs)?;
        let or = operator == "||";
        let short_circuit = self.emit(if or {
            Op::JumpTrue(0)
        } else {
            Op::JumpFalse(0)
        });
        self.expr(rhs)?;
        self.emit(Op::CastBool);
        let done = self.emit(Op::Jump(0));
        self.patch_jump(short_circuit, self.code.len());
        self.emit(Op::Push(Value::Int(i32::from(or))));
        self.patch_jump(done, self.code.len());
        Ok(())
    }
}

fn case_value(expr: &Expr) -> io::Result<Value> {
    let integer = match expr {
        Expr::Integer(n) => *n,
        Expr::Unary { operator, value } if operator == "-" => match value.as_ref() {
            Expr::Integer(n) => n.wrapping_neg(),
            _ => return Err(error("case expression must be an int or string")),
        },
        Expr::String(bytes) => return Ok(Value::String(bytes.clone())),
        _ => return Err(error("case expression must be an int or string")),
    };
    if !(-0x7e0002..0x800000).contains(&integer) {
        return Err(error("case index out of range"));
    }
    Ok(Value::Int(integer))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn function(body: Vec<Statement>) -> Function {
        Function {
            parameters: vec![],
            body,
        }
    }

    fn name(text: &str) -> Expr {
        Expr::Name(text.into())
    }

    fn method(name: &str, arguments: Vec<Expr>) -> Expr {
        Expr::Call {
            receiver: Some(Box::new(Expr::Name("self".into()))),
            target: Box::new(Expr::Function {
                module: None,
                name: name.into(),
            }),
            arguments,
            threaded: false,
        }
    }

    #[test]
    fn waittill_outputs_are_bindings_and_notify_inputs_are_values() -> io::Result<()> {
        let args = vec![name("event"), name("a"), name("b")];
        let code = compile(&function(vec![
            Statement::Expression(method("waittill", args.clone())),
            Statement::Expression(method("notify", args)),
        ]))?;
        assert_eq!(
            code,
            [
                Op::Load("event".into()),
                Op::Load("self".into()),
                Op::WaitTill(vec!["a".into(), "b".into()]),
                Op::Load("b".into()),
                Op::Load("a".into()),
                Op::Load("event".into()),
                Op::Load("self".into()),
                Op::Notify(2),
                Op::Push(Value::Undefined),
                Op::Return
            ]
        );
        assert!(
            compile(&function(vec![Statement::Expression(method(
                "waittill",
                vec![name("event"), Expr::Integer(3)]
            )),]))
            .is_err()
        );
        Ok(())
    }

    #[test]
    fn switch_continue_reaches_for_step_and_break_leaves_switch() -> io::Result<()> {
        let code = compile(&function(vec![Statement::For {
            init: None,
            condition: Some(name("again")),
            step: Some(Box::new(Statement::Update {
                target: name("i"),
                increment: true,
            })),
            body: Box::new(Statement::Switch {
                value: name("i"),
                cases: vec![
                    SwitchCase {
                        value: Some(Expr::Integer(0)),
                        body: vec![Statement::Continue],
                    },
                    SwitchCase {
                        value: None,
                        body: vec![Statement::Break],
                    },
                ],
            }),
        }]))?;
        assert_eq!(
            code,
            [
                Op::Load("again".into()),
                Op::JumpFalse(8),
                Op::Load("i".into()),
                Op::Switch {
                    cases: vec![(Value::Int(0), 4)],
                    default: 5
                },
                Op::Jump(6),
                Op::Jump(6),
                Op::Store {
                    owner: Place::Name("i".into()),
                    indices: 0,
                    step: Some(true)
                },
                Op::Jump(0),
                Op::Push(Value::Undefined),
                Op::Return,
            ]
        );
        Ok(())
    }

    #[test]
    fn array_assignment_evaluates_rhs_then_indices_then_owner() -> io::Result<()> {
        let target = Expr::Index(
            Box::new(Expr::Index(
                Box::new(Expr::Field(Box::new(name("actor")), "array".into())),
                Box::new(name("i")),
            )),
            Box::new(name("j")),
        );
        let code = compile(&function(vec![Statement::Assign {
            target,
            operator: "=".into(),
            value: name("v"),
        }]))?;
        assert_eq!(
            code,
            [
                Op::Load("v".into()),
                Op::Load("j".into()),
                Op::Load("i".into()),
                Op::Load("actor".into()),
                Op::Store {
                    owner: Place::Field("array".into()),
                    indices: 2,
                    step: None
                },
                Op::Push(Value::Undefined),
                Op::Return,
            ]
        );
        Ok(())
    }
}
