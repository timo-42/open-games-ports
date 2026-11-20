//! GScr_OpenFile through GScr_FReadline (1.3, 0x08117120..0x081178e9).
use super::value::{Value, script_error};
use crate::assets::GameAssets;
use std::io;

#[derive(Debug, Default)]
pub(super) struct ScriptFiles {
    // OpenFile searches only slot 0. Reading snapshots the bytes and closes
    // the engine handle immediately, so a writer may coexist with that reader.
    reader: Option<Reader>,
    writer: Option<String>,
}

#[derive(Debug)]
struct Reader {
    bytes: Vec<u8>,
    line: Option<usize>,
}

fn token_on_line(input: &mut &[u8]) -> io::Result<Vec<u8>> {
    if matches!(input.first(), None | Some(b'\n' | b'\r')) {
        return Ok(Vec::new());
    }
    let token = crate::csv::field(input)?;
    if let Some(rest) = input.strip_prefix(b",") {
        *input = rest;
    }
    Ok(token)
}

impl Reader {
    fn read_line(&mut self) -> io::Result<i32> {
        let next = self.line.map_or(0, |start| {
            self.bytes[start..]
                .iter()
                .position(|&b| b == b'\n')
                .map_or(self.bytes.len(), |i| start + i + 1)
        });
        let first = self.line.replace(next).is_none();
        let mut input = &self.bytes[next..];
        if !first {
            // Com_Parse looks ahead across CR/LF. Counting starts from the
            // saved physical line, which makes an intervening blank row count 0.
            let skip = input
                .iter()
                .take_while(|&&b| matches!(b, b'\r' | b'\n'))
                .count();
            let mut lookahead = &input[skip..];
            if token_on_line(&mut lookahead)?.is_empty() {
                return Ok(-1);
            }
        }
        let mut count = 0;
        while !token_on_line(&mut input)?.is_empty() {
            count += 1;
        }
        Ok(count)
    }

    fn argument(&self, index: i32) -> io::Result<Vec<u8>> {
        let mut input = self.line.map_or(&[][..], |start| &self.bytes[start..]);
        for i in 0..=index {
            let field = token_on_line(&mut input)?;
            if field.is_empty() {
                println!(
                    "freadline failed, there aren't {} arguments on this line, there are only {i} arguments",
                    index.saturating_add(1)
                );
                return Ok(field);
            }
            if i == index {
                return Ok(field);
            }
        }
        Ok(Vec::new())
    }
}

fn integer(value: &Value) -> io::Result<i32> {
    if let Value::Int(n) = value {
        Ok(*n)
    } else {
        Err(script_error(format!(
            "type {} is not an int",
            value.type_name()
        )))
    }
}

impl ScriptFiles {
    pub(super) fn dispatch(
        &mut self,
        assets: &GameAssets,
        name: &str,
        args: &[Value],
    ) -> Option<io::Result<Value>> {
        Some(match name {
            "openfile" => self.open(assets, args),
            "closefile" => self.close(args),
            "fprintln" => self.print(assets, args),
            "freadln" => self.read(args),
            "fgetarg" => self.argument(args),
            _ => return None,
        })
    }

    fn open(&mut self, assets: &GameAssets, args: &[Value]) -> io::Result<Value> {
        let [filename, mode, ..] = args else {
            return Ok(Value::Undefined);
        };
        let filename = filename.script_string()?;
        let mode = mode.script_string()?;
        if self.writer.is_some() {
            println!("OpenFile failed.  1 files already open");
            return Ok(Value::Int(-1));
        }
        let Ok(filename) = std::str::from_utf8(&filename) else {
            return Ok(Value::Int(-1));
        };
        if open_games_vfs::layers::virtual_path(filename).is_err() {
            return Ok(Value::Int(-1));
        }
        let path = format!("scriptdata/{filename}");
        let success = match mode.as_slice() {
            b"read" => {
                if let Ok(mut bytes) = assets.read_script(&path) {
                    if let Some(end) = bytes.iter().position(|&b| b == 0) {
                        bytes.truncate(end);
                    }
                    self.reader = Some(Reader { bytes, line: None });
                    true
                } else {
                    false
                }
            }
            b"write" | b"append" => {
                let opened = if mode == b"write" {
                    assets.write_generated(&path, b"")
                } else {
                    assets.append_generated(&path, b"")
                };
                if opened.is_ok() {
                    self.writer = Some(path);
                }
                opened.is_ok()
            }
            _ => {
                println!("Valid openfile modes are 'write', 'read', and 'append'");
                false
            }
        };
        Ok(Value::Int(if success { 0 } else { -1 }))
    }

    fn close(&mut self, args: &[Value]) -> io::Result<Value> {
        let Some(file) = args.first() else {
            return Ok(Value::Undefined);
        };
        let slot = integer(file)?;
        if !(0..=1).contains(&slot) {
            println!("CloseFile failed, invalid file number {slot}");
        } else if slot == 0 && (self.writer.take().is_some() || self.reader.take().is_some()) {
            return Ok(Value::Int(1));
        } else {
            println!("CloseFile failed, file number {slot} was not open");
        }
        Ok(Value::Int(-1))
    }

    fn print(&self, assets: &GameAssets, args: &[Value]) -> io::Result<Value> {
        let [file, _, ..] = args else {
            println!("fprintln requires at least 2 parameters (file, output)");
            return Ok(Value::Int(-1));
        };
        let slot = integer(file)?;
        if !(0..=1).contains(&slot) {
            println!("FPrintln failed, invalid file number {slot}");
            return Ok(Value::Int(-1));
        }
        let Some(path) = self.writer.as_ref().filter(|_| slot == 0) else {
            println!("FPrintln failed, file number {slot} was not open for writing");
            return Ok(Value::Int(-1));
        };
        let mut bytes = Vec::new();
        for value in &args[1..] {
            match value.script_string() {
                Ok(text) => {
                    bytes.extend_from_slice(text.split(|&b| b == 0).next().unwrap_or_default());
                    bytes.push(b',');
                }
                Err(error) => {
                    if !bytes.is_empty() {
                        assets.append_generated(path, &bytes)?;
                    }
                    return Err(error);
                }
            }
        }
        assets.append_generated(path, &bytes)?;
        Ok(Value::Int((args.len() - 1) as i32))
    }

    fn read(&mut self, args: &[Value]) -> io::Result<Value> {
        let Some(file) = args.first() else {
            println!("freadln requires a parameter - the file to read from");
            return Ok(Value::Int(-1));
        };
        let slot = integer(file)?;
        if !(0..=1).contains(&slot) {
            println!("freadln failed, invalid file number {slot}");
        } else if let Some(reader) = self.reader.as_mut().filter(|_| slot == 0) {
            return Ok(Value::Int(reader.read_line()?));
        } else {
            println!("freadln failed, file number {slot} was not open for reading");
        }
        Ok(Value::Int(-1))
    }

    fn argument(&self, args: &[Value]) -> io::Result<Value> {
        let [file, index, ..] = args else {
            println!("freadline requires at least 2 parameters (file, string)");
            return Ok(Value::String(Vec::new()));
        };
        let slot = integer(file)?;
        let index = integer(index)?;
        if !(0..=1).contains(&slot) {
            println!("freadline failed, invalid file number {slot}");
        } else if index < 0 {
            println!("freadline failed, invalid argument number {index}");
        } else if let Some(reader) = self.reader.as_ref().filter(|_| slot == 0) {
            return Ok(Value::String(reader.argument(index)?));
        } else {
            println!("freadline failed, file number {slot} was not open for reading");
        }
        Ok(Value::String(Vec::new()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn text(value: &str) -> Value {
        Value::String(value.as_bytes().to_vec())
    }

    #[test]
    fn original_scriptdata_slot_csv_and_home_precedence() -> io::Result<()> {
        let root = tempfile::tempdir()?;
        let base = root.path().join("base");
        let home = root.path().join("home");
        let stock = base.join("mods/test/scriptdata");
        fs::create_dir_all(base.join("main"))?;
        fs::write(base.join("main/default_mp.cfg"), b"")?;
        fs::create_dir_all(&stock)?;
        fs::create_dir_all(&home)?;
        fs::write(
            stock.join("fixture.csv"),
            b"one,\"two, two\",three\r\n\nnext,last,\nempty,,lost\n",
        )?;
        fs::write(stock.join("round.csv"), b"base copy")?;
        let assets = GameAssets::mount_paths(&base, Some(&home), "mods/test")?;
        let mut files = ScriptFiles::default();
        let zero = Value::Int(0);
        let ok = Value::Int(1);
        let fail = Value::Int(-1);
        let mut call = |name, args: &[Value]| {
            files
                .dispatch(&assets, name, args)
                .expect("registered script file native")
        };
        assert_eq!(call("openfile", &[])?, Value::Undefined);
        for bad in ["../escape", "/absolute", "C:\\escape", "sub/../../escape"] {
            assert_eq!(call("openfile", &[text(bad), text("write")])?, fail);
        }
        assert_eq!(
            call("openfile", &[text("round.csv"), text("append")])?,
            zero
        );
        assert_eq!(call("fprintln", &[zero.clone(), text("discard")])?, ok);
        assert_eq!(call("closefile", std::slice::from_ref(&zero))?, ok);
        assert_eq!(assets.read_script("scriptdata/round.csv")?, b"discard,");
        assert_eq!(call("openfile", &[text("round.csv"), text("write")])?, zero);
        assert_eq!(
            call(
                "fprintln",
                &[zero.clone(), text("one"), text("two"), Value::Int(42)]
            )?,
            Value::Int(3)
        );
        assert_eq!(
            call("openfile", &[text("another.csv"), text("write")])?,
            fail
        );
        assert_eq!(call("closefile", std::slice::from_ref(&zero))?, ok);
        assert_eq!(
            call("openfile", &[text("round.csv"), text("append")])?,
            zero
        );
        assert_eq!(call("fprintln", &[zero.clone(), text("more")])?, ok);
        assert_eq!(call("closefile", std::slice::from_ref(&zero))?, ok);
        assert_eq!(
            fs::read(home.join("mods/test/scriptdata/round.csv"))?,
            b"one,two,42,more,"
        );
        assert_eq!(fs::read(stock.join("round.csv"))?, b"base copy");
        assert_eq!(call("openfile", &[text("round.csv"), text("read")])?, zero);
        assert_eq!(call("freadln", std::slice::from_ref(&zero))?, Value::Int(4));
        assert_eq!(
            call("fgetarg", &[zero.clone(), Value::Int(3)])?,
            text("more")
        );
        assert_eq!(
            call("openfile", &[text("fixture.csv"), text("read")])?,
            zero
        );
        for (count, expected) in [
            (3, ["one", "two, two", "three"]),
            (0, ["", "", ""]),
            (2, ["next", "last", ""]),
            (1, ["empty", "", ""]),
            (-1, ["", "", ""]),
            (-1, ["", "", ""]),
        ] {
            assert_eq!(
                call("freadln", std::slice::from_ref(&zero))?,
                Value::Int(count)
            );
            for (i, token) in expected.into_iter().enumerate() {
                assert_eq!(
                    call("fgetarg", &[zero.clone(), Value::Int(i as i32)])?,
                    text(token)
                );
            }
        }
        assert_eq!(
            call("openfile", &[text("coexists.csv"), text("write")])?,
            zero
        );
        assert_eq!(call("closefile", std::slice::from_ref(&zero))?, ok);
        assert_eq!(call("closefile", std::slice::from_ref(&zero))?, ok);
        assert_eq!(call("closefile", std::slice::from_ref(&zero))?, fail);
        Ok(())
    }
}
