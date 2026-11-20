//! Owned GSC syntax used by server scripts. Identifiers are ASCII case-folded.

#[derive(Debug, Clone, PartialEq)]
pub struct Function {
    pub parameters: Vec<String>,
    pub body: Vec<Statement>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Statement {
    Block(Vec<Statement>),
    Expression(Expr),
    Assign {
        target: Expr,
        operator: String,
        value: Expr,
    },
    Update {
        target: Expr,
        increment: bool,
    },
    If {
        condition: Expr,
        then_branch: Box<Self>,
        else_branch: Option<Box<Self>>,
    },
    While {
        condition: Expr,
        body: Box<Self>,
    },
    For {
        init: Option<Box<Self>>,
        condition: Option<Expr>,
        step: Option<Box<Self>>,
        body: Box<Self>,
    },
    Switch {
        value: Expr,
        cases: Vec<SwitchCase>,
    },
    Return(Option<Expr>),
    Wait(Expr),
    WaitFrameEnd,
    Break,
    Continue,
    Breakpoint,
    Profile {
        begin: bool,
        name: Vec<u8>,
    },
    Empty,
}

#[derive(Debug, Clone, PartialEq)]
pub struct SwitchCase {
    /// None represents default.
    pub value: Option<Expr>,
    pub body: Vec<Statement>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Undefined,
    Integer(i32),
    Float(f32),
    Bool(bool),
    String(Vec<u8>),
    LocalizedString(Vec<u8>),
    Name(String),
    EmptyArray,
    Vector(Box<[Expr; 3]>),
    Field(Box<Expr>, String),
    Index(Box<Expr>, Box<Expr>),
    Size(Box<Expr>),
    Unary {
        operator: String,
        value: Box<Expr>,
    },
    Binary {
        operator: String,
        left: Box<Expr>,
        right: Box<Expr>,
    },
    Function {
        module: Option<String>,
        name: String,
    },
    Call {
        receiver: Option<Box<Expr>>,
        target: Box<Expr>,
        arguments: Vec<Expr>,
        threaded: bool,
    },
    DeveloperCall {
        name: String,
        arguments: Vec<Expr>,
    },
    Animation {
        tree: String,
        name: String,
    },
    AnimTree(String),
}
