//! CoD2 script values and operators (Scr_Eval* in chunk_015).
use std::collections::BTreeMap;
use std::io;

pub(super) fn error(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

/// An error raised through Scr_Error and handled by VM_ExecuteInternal's
/// opcode-specific recovery. Asset reads and incomplete native bindings use
/// ordinary I/O errors and cannot enter this path.
#[derive(Debug)]
pub(super) struct ScriptFault {
    message: String,
    pub(super) result: Value,
}

impl std::fmt::Display for ScriptFault {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.message)
    }
}
impl std::error::Error for ScriptFault {}

impl ScriptFault {
    fn division_by_zero(result: Value) -> io::Error {
        io::Error::new(
            io::ErrorKind::InvalidData,
            Self {
                message: "divide by 0".into(),
                result,
            },
        )
    }

    pub(super) fn from_error(error: &io::Error) -> Option<&Self> {
        error.get_ref()?.downcast_ref()
    }
}

pub(crate) fn script_error(message: impl Into<String>) -> io::Error {
    io::Error::new(
        io::ErrorKind::InvalidData,
        ScriptFault {
            message: message.into(),
            result: Value::Undefined,
        },
    )
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum ArrayKey {
    Int(i32),
    String(Vec<u8>),
}

impl ArrayKey {
    fn from_value(index: Value) -> io::Result<Self> {
        match index {
            // Scr_IsSmallNameIndex: signed names occupy the 24-bit variable pool.
            Value::Int(index) if (-0x7e0002..0x800000).contains(&index) => Ok(Self::Int(index)),
            Value::Int(index) => Err(script_error(format!("array index {index} out of range"))),
            Value::String(text) => Ok(Self::String(text)),
            other => Err(script_error(format!(
                "{} is not an array index",
                other.type_name()
            ))),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Default)]
pub enum Value {
    #[default]
    Undefined,
    Int(i32),
    Float(f32),
    String(Vec<u8>),
    LocalizedString(Vec<u8>),
    Vector([f32; 3]),
    /// The source VM detaches shared arrays on write. Cloning an owned map
    /// provides the same value semantics; objects inside retain their identity.
    Array(BTreeMap<ArrayKey, Value>),
    Object(u32),
    Function {
        module: Option<String>,
        name: String,
    },
    Animation(u32),
}

impl Value {
    pub fn type_name(&self) -> &'static str {
        match self {
            Self::Undefined => "undefined",
            Self::Int(_) => "int",
            Self::Float(_) => "float",
            Self::String(_) => "string",
            Self::LocalizedString(_) => "localized string",
            Self::Vector(_) => "vector",
            Self::Array(_) => "array",
            Self::Object(_) => "object",
            Self::Function { .. } => "function",
            Self::Animation(_) => "animation",
        }
    }

    pub fn number(&self) -> io::Result<f32> {
        match self {
            Self::Int(value) => Ok(*value as f32),
            Self::Float(value) => Ok(*value),
            _ => Err(script_error(format!(
                "type {} is not a float",
                self.type_name()
            ))),
        }
    }

    pub fn truthy(&self) -> io::Result<bool> {
        match self {
            Self::Int(value) => Ok(*value != 0),
            Self::Float(value) => Ok(*value != 0.0),
            _ => Err(script_error(format!(
                "cannot cast {} to bool",
                self.type_name()
            ))),
        }
    }

    pub fn size(&self) -> io::Result<i32> {
        match self {
            Self::Array(values) => Ok(values.len() as i32),
            Self::String(text) => Ok(text.len() as i32),
            Self::Object(_) => Ok(1),
            _ => Err(script_error(format!(
                "size cannot be applied to {}",
                self.type_name()
            ))),
        }
    }

    pub fn index(&self, index: Value) -> io::Result<Value> {
        match self {
            Self::Array(values) => Ok(values
                .get(&ArrayKey::from_value(index)?)
                .cloned()
                .unwrap_or_default()),
            Self::Vector(vector) => match index {
                Self::Int(n) => vector
                    .get(n as usize)
                    .copied()
                    .map(Self::Float)
                    .ok_or_else(|| script_error(format!("vector index {n} out of range"))),
                _ => Err(script_error(format!(
                    "{} is not a vector index",
                    index.type_name()
                ))),
            },
            Self::String(text) => match index {
                Self::Int(n) => text
                    .get(n as usize)
                    .map(|&byte| Self::String(vec![byte]))
                    .ok_or_else(|| script_error(format!("string index {n} out of range"))),
                _ => Err(script_error(format!(
                    "{} is not a string index",
                    index.type_name()
                ))),
            },
            _ => Err(script_error(format!(
                "{} is not an array, string, or vector",
                self.type_name()
            ))),
        }
    }

    pub fn write_index(&mut self, index: Value, value: Value) -> io::Result<()> {
        let key = ArrayKey::from_value(index)?;
        if matches!(self, Self::Undefined) && !matches!(value, Self::Undefined) {
            *self = Self::Array(BTreeMap::new());
        }
        match self {
            Self::Array(values) => {
                if value == Self::Undefined {
                    values.remove(&key);
                } else {
                    values.insert(key, value);
                }
                Ok(())
            }
            Self::Vector(_) => Err(script_error(
                "vector components cannot be individually changed",
            )),
            Self::String(_) => Err(script_error(
                "string characters cannot be individually changed",
            )),
            _ => Err(script_error(format!(
                "{} is not an array",
                self.type_name()
            ))),
        }
    }

    pub fn script_string(&self) -> io::Result<Vec<u8>> {
        match self {
            Self::String(text) => Ok(text.clone()),
            Self::Int(n) => Ok(n.to_string().into_bytes()),
            Self::Float(n) => Ok(general_float(*n).into_bytes()),
            Self::Vector(v) => Ok(format!(
                "({}, {}, {})",
                general_float(v[0]),
                general_float(v[1]),
                general_float(v[2])
            )
            .into_bytes()),
            _ => Err(script_error(format!(
                "cannot cast {} to string",
                self.type_name()
            ))),
        }
    }
}

// The original interns floating-point values through sprintf("%g"): six significant
// digits, scientific notation below exponent -4 or at/above 6, no trailing zeros.
pub(crate) fn general_float(value: f32) -> String {
    if !value.is_finite() {
        return if value.is_nan() {
            if value.is_sign_negative() {
                "-nan"
            } else {
                "nan"
            }
        } else if value.is_sign_negative() {
            "-inf"
        } else {
            "inf"
        }
        .into();
    }
    let scientific = format!("{value:.5e}");
    let (mantissa, exponent) = scientific
        .split_once('e')
        .expect("finite scientific notation");
    let exp: i32 = exponent.parse().expect("numeric exponent");
    if !(-4..6).contains(&exp) {
        return format!(
            "{}e{exp:+03}",
            mantissa.trim_end_matches('0').trim_end_matches('.')
        );
    }
    let precision = (5 - exp) as usize;
    let text = format!("{value:.precision$}");
    if text.contains('.') {
        text.trim_end_matches('0').trim_end_matches('.').to_owned()
    } else {
        text
    }
}

pub fn binary(op: &str, left: Value, right: Value) -> io::Result<Value> {
    use Value::*;
    let unsupported = || {
        script_error(format!(
            "operator {op} cannot be applied to {} and {}",
            left.type_name(),
            right.type_name()
        ))
    };
    if op == "==" || op == "!=" {
        let equal = match (&left, &right) {
            (Undefined, Undefined) => true,
            (Int(a), Int(b)) => a == b,
            (Int(_) | Float(_), Int(_) | Float(_)) => {
                (left.number()? - right.number()?).abs() < 1e-6
            }
            (String(a), String(b)) | (LocalizedString(a), LocalizedString(b)) => a == b,
            (Vector(a), Vector(b)) => a == b,
            (Object(a), Object(b)) => a == b,
            (Function { .. }, Function { .. }) | (Animation(_), Animation(_)) => left == right,
            _ => return Err(unsupported()),
        };
        return Ok(Int(i32::from(if op == "==" { equal } else { !equal })));
    }
    if op == "+" && (matches!(left, String(_)) || matches!(right, String(_))) {
        let mut text = left.script_string()?;
        let suffix = right.script_string()?;
        if text.len().saturating_add(suffix.len()) > 8192 {
            return Err(script_error("cannot concat - max string length exceeded"));
        }
        text.extend(suffix);
        return Ok(String(text));
    }
    if let (Vector(a), Vector(b)) = (&left, &right) {
        return match op {
            "+" => Ok(Vector(std::array::from_fn(|axis| a[axis] + b[axis]))),
            "-" => Ok(Vector(std::array::from_fn(|axis| a[axis] - b[axis]))),
            _ => Err(unsupported()),
        };
    }
    if let (Int(a), Int(b)) = (&left, &right) {
        let (a, b) = (*a, *b);
        let value = match op {
            "+" => Int(a.wrapping_add(b)),
            "-" => Int(a.wrapping_sub(b)),
            "*" => Int(a.wrapping_mul(b)),
            "/" if b == 0 => return Err(ScriptFault::division_by_zero(Float(0.))),
            "%" if b == 0 => return Err(ScriptFault::division_by_zero(Int(0))),
            "/" => Float(a as f32 / b as f32),
            "%" => Int(a.wrapping_rem(b)),
            "&" => Int(a & b),
            "|" => Int(a | b),
            "^" => Int(a ^ b),
            "<<" => Int(a.wrapping_shl(b as u32)),
            ">>" => Int(a.wrapping_shr(b as u32)),
            "<" => Int(i32::from(a < b)),
            ">" => Int(i32::from(a > b)),
            "<=" => Int(i32::from(a <= b)),
            ">=" => Int(i32::from(a >= b)),
            _ => return Err(unsupported()),
        };
        return Ok(value);
    }
    let (a, b) = (left.number()?, right.number()?);
    match op {
        "+" => Ok(Float(a + b)),
        "-" => Ok(Float(a - b)),
        "*" => Ok(Float(a * b)),
        "/" if b == 0.0 => Err(ScriptFault::division_by_zero(Float(0.))),
        "/" => Ok(Float(a / b)),
        "<" => Ok(Int(i32::from(a < b))),
        ">" => Ok(Int(i32::from(a > b))),
        // Scr_EvalLessEqual and Scr_EvalGreaterEqual negate the opposite comparison, including unordered values.
        "<=" => Ok(Int(i32::from(
            a.partial_cmp(&b) != Some(std::cmp::Ordering::Greater),
        ))),
        ">=" => Ok(Int(i32::from(
            a.partial_cmp(&b) != Some(std::cmp::Ordering::Less),
        ))),
        _ => Err(unsupported()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use Value::{Float, Int, Vector};

    #[test]
    fn arithmetic_and_comparisons_match_vm_result_types() {
        assert_eq!(binary("/", Int(7), Int(2)).expect("divide"), Float(3.5));
        assert_eq!(
            binary("+", Int(i32::MAX), Int(1)).expect("add"),
            Int(i32::MIN)
        );
        assert_eq!(binary(">>", Int(-9), Int(34)).expect("shift"), Int(-3));
        assert_eq!(
            binary("==", Float(1.000_000_5), Int(1)).expect("equality"),
            Int(1)
        );
        assert_eq!(
            binary(">=", Float(f32::NAN), Int(3)).expect("unordered compare"),
            Int(1)
        );
        assert!(binary("/", Float(1.), Float(-0.)).is_err());
        assert!(binary("*", Vector([1., 2., 3.]), Float(3.)).is_err());
        assert!(Value::Undefined.truthy().is_err());
    }

    #[test]
    fn arrays_detach_while_children_keep_object_identity() {
        let mut array = Value::Undefined;
        array
            .write_index(Int(-1), Value::Object(3))
            .expect("make array");
        let mut copy = array.clone();
        copy.write_index(Int(-1), Value::Undefined)
            .expect("remove index");
        assert_eq!(copy.size().expect("size"), 0);
        assert_eq!(array.index(Int(-1)).expect("read"), Value::Object(3));
        assert_eq!(array.index(Int(0)).expect("absent"), Value::Undefined);
        assert!(array.index(Int(0x800000)).is_err());
        assert!(binary("==", array.clone(), array).is_err());
    }

    #[test]
    fn string_conversion_and_index_use_original_format_and_bytes() {
        let prefix = Value::String(b"v=".to_vec());
        assert_eq!(
            binary("+", prefix, Vector([0.000_01, -0.0, 1_000_000.0])).expect("join"),
            Value::String(b"v=(1e-05, -0, 1e+06)".to_vec())
        );
        assert_eq!(
            Value::String(vec![b'x', 0xe4])
                .index(Int(1))
                .expect("single byte"),
            Value::String(vec![0xe4])
        );
        assert_eq!(Float(999_999.9).script_string().expect("round"), b"1e+06");
    }
}
