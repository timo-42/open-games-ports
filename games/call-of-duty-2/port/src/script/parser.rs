use super::ast::{Expr, Function, Statement, SwitchCase};
use super::lexer::{Kind, Token, decode_string, tokenize};
use super::{Module, invalid, normalize};
use std::collections::BTreeSet;
use std::io;

fn developer_builtin(name: &str) -> bool {
    matches!(
        name,
        "print"
            | "println"
            | "print3d"
            | "line"
            | "assert"
            | "assertex"
            | "assertmsg"
            | "getentbynum"
    )
}

// ScrCompile_GetUnresolvedCalleeId tests this file's declarations before
// resolving a bare name against the builtin table. An include cannot shadow
// these entries, while a function declared later in the same file can.
fn developer_overrides(tokens: &[Token<'_>]) -> BTreeSet<String> {
    let mut names = BTreeSet::new();
    let mut depth = 0usize;
    for pair in tokens.windows(2) {
        match pair[0].bytes {
            b"{" => depth += 1,
            b"}" => depth = depth.saturating_sub(1),
            _ if depth == 0 && pair[0].kind == Kind::Word && pair[1].bytes == b"(" => {
                let name = pair[0].ascii().to_ascii_lowercase();
                if developer_builtin(&name) {
                    names.insert(name);
                }
            }
            _ => {}
        }
    }
    names
}

pub(super) fn scan(source: &[u8], developer_script: bool) -> io::Result<Module> {
    let tokens = tokenize(source)?;
    Parser {
        tokens: &tokens,
        position: 0,
        depth: 0,
        developer_script,
        developer_scope: false,
        developer_overrides: developer_overrides(&tokens),
        developer_result_uses: Vec::new(),
        module: Module::default(),
        tree: None,
    }
    .module()
}

struct Parser<'a> {
    tokens: &'a [Token<'a>],
    position: usize,
    depth: usize,
    developer_script: bool,
    developer_scope: bool,
    developer_overrides: BTreeSet<String>,
    developer_result_uses: Vec<usize>,
    module: Module,
    tree: Option<String>,
}

impl<'a> Parser<'a> {
    fn current(&self) -> Token<'a> {
        self.tokens[self.position]
    }

    fn at(&self, bytes: &[u8]) -> bool {
        self.current().bytes == bytes
    }

    fn take(&mut self, bytes: &[u8]) -> bool {
        if self.at(bytes) {
            self.position += 1;
            true
        } else {
            false
        }
    }

    fn error(&self, message: &str) -> io::Error {
        let token = self.current();
        invalid(format!(
            "byte {} at '{}': {message}",
            token.offset,
            String::from_utf8_lossy(token.bytes)
        ))
    }

    fn expect(&mut self, token: &[u8]) -> io::Result<()> {
        if self.take(token) {
            Ok(())
        } else {
            Err(self.error(&format!("expected '{}'", String::from_utf8_lossy(token))))
        }
    }

    fn ident(&mut self) -> io::Result<String> {
        let token = self.current();
        if token.kind != Kind::Word {
            return Err(self.error("expected identifier"));
        }
        self.position += 1;
        Ok(token.ascii().to_ascii_lowercase())
    }

    fn nested<T>(&mut self, operation: impl FnOnce(&mut Self) -> io::Result<T>) -> io::Result<T> {
        if self.depth >= 100 {
            return Err(self.error("script nesting limit reached"));
        }
        self.depth += 1;
        let result = operation(self);
        self.depth -= 1;
        result
    }

    fn module(mut self) -> io::Result<Module> {
        let mut dev_only = false;
        while self.current().kind != Kind::End {
            if self.take(b"/#") {
                dev_only = true;
                self.developer_scope = true;
                continue;
            }
            if self.take(b"#/") {
                dev_only = false;
                self.developer_scope = false;
                continue;
            }
            if self.take(b"#") {
                if self.take(b"include") {
                    let path = normalize(&self.ident()?)?;
                    if self.module.includes.contains(&path) {
                        return Err(self.error("duplicate #include"));
                    }
                    self.module.includes.push(path);
                } else {
                    self.expect(b"using_animtree")?;
                    self.expect(b"(")?;
                    let tree = self.current();
                    if tree.kind != Kind::Text {
                        return Err(self.error("expected animation tree name"));
                    }
                    if dev_only {
                        return Err(
                            self.error("cannot put #using_animtree inside /# ... #/ comment")
                        );
                    }
                    let name = decode_string(tree);
                    if !name.iter().all(|b| b.is_ascii_alphanumeric() || *b == b'_') {
                        return Err(self.error("bad anim tree name"));
                    }
                    let name = String::from_utf8(name)
                        .expect("validated ASCII")
                        .to_ascii_lowercase();
                    self.module.animation_uses.push((name.clone(), None));
                    self.tree = Some(name);
                    self.position += 1;
                    self.expect(b")")?;
                }
                self.expect(b";")?;
                continue;
            }
            let name = self.ident()?;
            self.expect(b"(")?;
            let mut parameters = Vec::new();
            if !self.at(b")") {
                loop {
                    parameters.push(self.ident()?);
                    if !self.take(b",") {
                        break;
                    }
                }
            }
            self.expect(b")")?;
            self.expect(b"{")?;
            let refs = (self.module.external.len(), self.module.animation_uses.len());
            let body = self.block(b"}")?;
            if dev_only && !self.developer_script {
                self.module.external.truncate(refs.0);
                self.module.animation_uses.truncate(refs.1);
                continue;
            }
            if self
                .module
                .functions
                .insert(name.clone(), Function { parameters, body })
                .is_some()
            {
                return Err(self.error(&format!("function '{name}' already defined")));
            }
        }
        if let Some(&position) = self.developer_result_uses.first() {
            self.position = position;
            return Err(self.error("return value of developer command can not be accessed if not in a /# ... #/ comment"));
        }
        Ok(self.module)
    }

    fn block(&mut self, end: &[u8]) -> io::Result<Vec<Statement>> {
        let mut body = Vec::new();
        while !self.take(end) {
            body.push(self.statement()?);
        }
        Ok(body)
    }

    fn statement(&mut self) -> io::Result<Statement> {
        self.nested(Self::statement_inner)
    }

    fn control_condition(&mut self) -> io::Result<Expr> {
        self.expect(b"(")?;
        let result = self.expr()?;
        self.expect(b")")?;
        Ok(result)
    }

    fn statement_inner(&mut self) -> io::Result<Statement> {
        use Statement as S;
        if self.take(b"{") {
            return Ok(S::Block(self.block(b"}")?));
        }
        if self.take(b"/#") {
            let refs = (self.module.external.len(), self.module.animation_uses.len());
            let enclosing_scope = self.developer_scope;
            self.developer_scope = true;
            let body = self.block(b"#/")?;
            self.developer_scope = enclosing_scope;
            if self.developer_script {
                return Ok(S::Block(body));
            }
            self.module.external.truncate(refs.0);
            self.module.animation_uses.truncate(refs.1);
            return Ok(S::Empty);
        }
        if self.take(b";") {
            return Ok(S::Empty);
        }
        if self.take(b"if") {
            let condition = self.control_condition()?;
            let then_branch = Box::new(self.statement()?);
            let else_branch = if self.take(b"else") {
                Some(Box::new(self.statement()?))
            } else {
                None
            };
            return Ok(S::If {
                condition,
                then_branch,
                else_branch,
            });
        }
        if self.take(b"while") {
            return Ok(S::While {
                condition: self.control_condition()?,
                body: Box::new(self.statement()?),
            });
        }
        if self.take(b"for") {
            self.expect(b"(")?;
            let init = if self.at(b";") {
                None
            } else {
                Some(Box::new(self.simple_statement()?))
            };
            self.expect(b";")?;
            let condition = if self.at(b";") {
                None
            } else {
                Some(self.expr()?)
            };
            self.expect(b";")?;
            let step = if self.at(b")") {
                None
            } else {
                Some(Box::new(self.simple_statement()?))
            };
            self.expect(b")")?;
            return Ok(S::For {
                init,
                condition,
                step,
                body: Box::new(self.statement()?),
            });
        }
        if self.take(b"switch") {
            let value = self.control_condition()?;
            self.expect(b"{")?;
            let mut cases = Vec::new();
            while !self.take(b"}") {
                let label = if self.take(b"case") {
                    Some(self.expr()?)
                } else {
                    self.expect(b"default")?;
                    None
                };
                self.expect(b":")?;
                let mut body = Vec::new();
                while !(self.at(b"case") || self.at(b"default") || self.at(b"}")) {
                    body.push(self.statement()?);
                }
                cases.push(SwitchCase { value: label, body });
            }
            return Ok(S::Switch { value, cases });
        }
        let stmt = if self.take(b"return") {
            S::Return(if self.at(b";") {
                None
            } else {
                Some(self.expr()?)
            })
        } else if self.take(b"wait") {
            S::Wait(self.expr()?)
        } else if self.take(b"waittillframeend") {
            S::WaitFrameEnd
        } else if self.take(b"continue") {
            S::Continue
        } else if self.take(b"break") {
            S::Break
        } else if self.take(b"breakpoint") {
            S::Breakpoint
        } else if self.at(b"prof_begin") || self.at(b"prof_end") {
            let begin = self.at(b"prof_begin");
            self.position += 1;
            self.expect(b"(")?;
            let marker = self.current();
            if marker.kind != Kind::Text {
                return Err(self.error("expected a quoted profile marker"));
            }
            let name = decode_string(marker);
            self.position += 1;
            self.expect(b")")?;
            S::Profile { begin, name }
        } else {
            self.simple_statement()?
        };
        self.expect(b";")?;
        Ok(stmt)
    }

    fn simple_statement(&mut self) -> io::Result<Statement> {
        let pending = self.developer_result_uses.len();
        let refs = (self.module.external.len(), self.module.animation_uses.len());
        let target = self.expr()?;
        let operator = self.current();
        if matches!(
            operator.bytes,
            b"=" | b"+="
                | b"-="
                | b"*="
                | b"/="
                | b"%="
                | b"&="
                | b"|="
                | b"^="
                | b"<<="
                | b">>="
                | b"++"
                | b"--"
        ) {
            if !matches!(target, Expr::Name(_) | Expr::Field(..) | Expr::Index(..)) {
                return Err(self.error("assignment requires a variable, field or index"));
            }
            self.position += 1;
            if matches!(operator.bytes, b"++" | b"--") {
                return Ok(Statement::Update {
                    target,
                    increment: operator.bytes == b"++",
                });
            }
            return Ok(Statement::Assign {
                target,
                operator: operator.ascii().into(),
                value: self.expr()?,
            });
        }
        // ScrCompile_DevBlockLeave discards the complete instruction range,
        // including argument calls and their side effects, in a normal build.
        if matches!(target, Expr::DeveloperCall { .. }) {
            self.developer_result_uses.truncate(pending);
            if !self.developer_script {
                self.module.external.truncate(refs.0);
                self.module.animation_uses.truncate(refs.1);
                return Ok(Statement::Empty);
            }
            return Ok(Statement::Expression(target));
        }
        if matches!(target, Expr::Call { .. }) {
            Ok(Statement::Expression(target))
        } else {
            Err(self.error("expected call or assignment"))
        }
    }

    fn expr(&mut self) -> io::Result<Expr> {
        self.binary(1)
    }

    fn binary(&mut self, min_precedence: u8) -> io::Result<Expr> {
        self.nested(|parser| {
            let mut left = parser.primary()?;
            loop {
                let operator = parser.current();
                let precedence = match operator.bytes {
                    b"||" => 1,
                    b"&&" => 2,
                    b"|" => 3,
                    b"^" => 4,
                    b"&" => 5,
                    b"==" | b"!=" => 6,
                    b"<" | b"<=" | b">" | b">=" => 7,
                    b"<<" | b">>" => 8,
                    b"+" | b"-" => 9,
                    b"*" | b"/" | b"%" => 10,
                    _ => break,
                };
                if precedence < min_precedence {
                    break;
                }
                parser.position += 1;
                let right = parser.binary(precedence + 1)?;
                left = Expr::Binary {
                    operator: operator.ascii().into(),
                    left: Box::new(left),
                    right: Box::new(right),
                };
            }
            Ok(left)
        })
    }

    fn primary(&mut self) -> io::Result<Expr> {
        let token = self.current();
        let mut expr = if self.take(b"(") {
            let first = self.expr()?;
            let value = if self.take(b",") {
                let second = self.expr()?;
                self.expect(b",")?;
                Expr::Vector(Box::new([first, second, self.expr()?]))
            } else {
                first
            };
            self.expect(b")")?;
            value
        } else if matches!(token.bytes, b"!" | b"~" | b"-") {
            self.position += 1;
            Expr::Unary {
                operator: token.ascii().into(),
                value: Box::new(self.binary(11)?),
            }
        } else if token.kind == Kind::Number {
            self.position += 1;
            if token.bytes.iter().any(|b| matches!(b, b'.' | b'e' | b'E')) {
                Expr::Float(
                    token
                        .ascii()
                        .parse()
                        .map_err(|_| self.error("invalid float literal"))?,
                )
            } else {
                Expr::Integer(
                    token
                        .ascii()
                        .parse::<i64>()
                        .map_err(|_| self.error("invalid integer literal"))?
                        as i32,
                )
            }
        } else if matches!(token.kind, Kind::Text | Kind::LocalizedText) {
            self.position += 1;
            if token.kind == Kind::Text {
                Expr::String(decode_string(token))
            } else {
                Expr::LocalizedString(decode_string(token))
            }
        } else if self.take(b"undefined") {
            Expr::Undefined
        } else if self.take(b"true") {
            Expr::Bool(true)
        } else if self.take(b"false") {
            Expr::Bool(false)
        } else if self.take(b"%") {
            let name = self.ident()?;
            let tree = self
                .tree
                .clone()
                .ok_or_else(|| self.error("#using_animtree was not specified"))?;
            self.module
                .animation_uses
                .push((tree.clone(), Some(name.clone())));
            Expr::Animation { tree, name }
        } else if self.take(b"#") {
            self.expect(b"animtree")?;
            Expr::AnimTree(
                self.tree
                    .clone()
                    .ok_or_else(|| self.error("#using_animtree was not specified"))?,
            )
        } else if self.at(b"[") && !self.pointer() {
            self.position += 1;
            self.expect(b"]")?;
            Expr::EmptyArray
        } else if self.take(b"thread") {
            self.named_or_dynamic(None, true, true)?
        } else {
            self.named_or_dynamic(None, false, false)?
        };

        loop {
            if self.take(b".") {
                let field = self.ident()?;
                expr = if field == "size" {
                    Expr::Size(Box::new(expr))
                } else {
                    Expr::Field(Box::new(expr), field)
                };
            } else if self.at(b"[") && !self.pointer() {
                self.position += 1;
                let index = self.expr()?;
                self.expect(b"]")?;
                expr = Expr::Index(Box::new(expr), Box::new(index));
            } else if self.current().kind == Kind::Word || self.pointer() {
                let threaded = self.take(b"thread");
                expr = self.named_or_dynamic(Some(Box::new(expr)), threaded, true)?;
            } else {
                break;
            }
        }
        Ok(expr)
    }

    fn pointer(&self) -> bool {
        self.at(b"[")
            && self
                .tokens
                .get(self.position + 1)
                .is_some_and(|t| t.bytes == b"[")
    }

    fn named_or_dynamic(
        &mut self,
        receiver: Option<Box<Expr>>,
        threaded: bool,
        require_call: bool,
    ) -> io::Result<Expr> {
        let position = self.position;
        let bare_name = self.current().kind == Kind::Word;
        let dynamic = self.pointer();
        let target = if dynamic {
            self.position += 2;
            let pointer = self.expr()?;
            self.expect(b"]")?;
            self.expect(b"]")?;
            pointer
        } else if self.take(b"::") {
            Expr::Function {
                module: None,
                name: self.ident()?,
            }
        } else {
            let name = self.ident()?;
            if self.take(b"::") {
                Expr::Function {
                    module: Some(normalize(&name)?),
                    name: self.ident()?,
                }
            } else if self.at(b"(") || require_call {
                Expr::Function { module: None, name }
            } else {
                return Ok(Expr::Name(name));
            }
        };
        let call = self.take(b"(");
        if !call && (require_call || dynamic) {
            return Err(self.error("expected call arguments"));
        }
        let developer = if call
            && bare_name
            && receiver.is_none()
            && !threaded
            && let Expr::Function { module: None, name } = &target
            && developer_builtin(name)
            && !self.developer_overrides.contains(name)
        {
            Some(name.clone())
        } else {
            None
        };
        let enclosing_scope = self.developer_scope;
        if developer.is_some() {
            // Nested debugger commands may supply arguments to another developer command.
            self.developer_scope = true;
        }
        let mut arguments = Vec::new();
        if call {
            if !self.at(b")") {
                loop {
                    arguments.push(self.expr()?);
                    if !self.take(b",") {
                        break;
                    }
                }
            }
            self.expect(b")")?;
        }
        self.developer_scope = enclosing_scope;
        if let Some(name) = developer {
            if !enclosing_scope {
                self.developer_result_uses.push(position);
            }
            return Ok(Expr::DeveloperCall { name, arguments });
        }
        if !dynamic
            && let Expr::Function {
                module: Some(path),
                name,
            } = &target
        {
            self.module.external.push((path.clone(), name.clone()));
        }
        if call {
            Ok(Expr::Call {
                receiver,
                target: Box::new(target),
                arguments,
                threaded,
            })
        } else {
            Ok(target)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn function(source: &[u8]) -> Function {
        scan(source, false)
            .expect("valid GSC")
            .functions
            .remove("main")
            .expect("main")
    }

    #[test]
    fn developer_calls_honor_result_scope_and_local_overrides() {
        let source = b"main(){println(other::tick(), getentbynum(0)); /# obj=getentbynum(0); #/}";
        let normal = scan(source, false).expect("discard developer code");
        assert!(normal.external.is_empty());
        assert_eq!(
            normal.functions["main"].body,
            [Statement::Empty, Statement::Empty]
        );
        let debug = scan(source, true).expect("emit developer code");
        assert_eq!(debug.external, [("other".into(), "tick".into())]);
        for enabled in [true, false] {
            let error = scan(b"main(){x=getentbynum(0);}", enabled)
                .expect_err("illicit use of a developer return value");
            assert!(
                error
                    .to_string()
                    .contains("return value of developer command")
            );
            let normal = scan(b"main(){println(other::tick());} println(x){}", enabled)
                .expect("local function shadows developer command");
            assert_eq!(normal.external.len(), 1);
            assert!(matches!(
                normal.functions["main"].body[0],
                Statement::Expression(Expr::Call { .. })
            ));
        }
    }

    #[test]
    fn pratt_tree_preserves_binary_precedence_and_left_associativity() {
        let program = function(b"main(a,b,c,d) { return a - b - c * d; }");
        assert_eq!(program.parameters, ["a", "b", "c", "d"]);
        let Statement::Return(Some(Expr::Binary {
            operator,
            left,
            right,
        })) = &program.body[0]
        else {
            panic!("expected binary return");
        };
        assert_eq!(operator, "-");
        assert!(matches!(left.as_ref(), Expr::Binary{operator, ..} if operator=="-"));
        assert!(matches!(right.as_ref(), Expr::Binary{operator, ..} if operator=="*"));
    }

    #[test]
    fn method_thread_pointer_field_index_and_event_bindings_survive_parse() {
        let program = function(
            br#"main(client) {
            client thread [[level.callbacks[0]]]( (1, .5, -2) );
            client waittill("damage", victim, attacker);
            victim.health -= 4;
        }"#,
        );
        let Statement::Expression(Expr::Call {
            receiver,
            threaded,
            target,
            arguments,
        }) = &program.body[0]
        else {
            panic!("expected thread");
        };
        assert!(*threaded);
        assert!(matches!(receiver.as_deref(),Some(Expr::Name(name)) if name=="client"));
        assert!(
            matches!(target.as_ref(),Expr::Index(obj,_) if matches!(obj.as_ref(),Expr::Field(_,name) if name=="callbacks"))
        );
        assert!(matches!(&arguments[..], [Expr::Vector(values)] if values[1]==Expr::Float(0.5)));
        let Statement::Expression(Expr::Call {
            target, arguments, ..
        }) = &program.body[1]
        else {
            panic!("expected waittill");
        };
        assert!(matches!(target.as_ref(),Expr::Function{module:None,name} if name=="waittill"));
        assert_eq!(
            arguments,
            &[
                Expr::String(b"damage".to_vec()),
                Expr::Name("victim".into()),
                Expr::Name("attacker".into())
            ]
        );
        assert!(
            matches!(&program.body[2],Statement::Assign{target:Expr::Field(_,field),operator,..} if field=="health" && operator=="-=")
        );
    }

    #[test]
    fn preserves_for_switch_case_fallthrough_and_default() {
        let program = function(
            br#"main() { for (i=0;i<3;i++) {
            switch (i) { case 0: case 1: wait 1; break; default: continue; }
        } }"#,
        );
        let Statement::For {
            init: Some(_),
            condition: Some(_),
            step: Some(step),
            body,
        } = &program.body[0]
        else {
            panic!("expected for");
        };
        assert!(matches!(
            step.as_ref(),
            Statement::Update {
                increment: true,
                ..
            }
        ));
        let Statement::Block(stmts) = body.as_ref() else {
            panic!("expected loop body");
        };
        let Statement::Switch { cases, .. } = &stmts[0] else {
            panic!("expected switch");
        };
        assert!(cases[0].body.is_empty());
        assert_eq!(
            cases[1].body,
            [Statement::Wait(Expr::Integer(1)), Statement::Break]
        );
        assert_eq!(cases[2].value, None);
        assert_eq!(cases[2].body, [Statement::Continue]);
    }

    #[test]
    fn rejects_unknown_body_syntax_with_input_offset() {
        let error = scan(br#"main() { self.x = @; }"#, false).expect_err("unknown syntax");
        assert!(error.to_string().contains("byte 18"));
    }
}
