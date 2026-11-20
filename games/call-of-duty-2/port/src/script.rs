//! Source discovery for GScr_LoadScripts (0x0811041e). The declarations and
//! dependencies identified here precede bytecode generation and execution.

pub mod ast;
mod lexer;
mod parser;

use parser::scan;
use std::collections::{BTreeMap, BTreeSet};
use std::io;

const CALLBACKS: &[&str] = &[
    "codecallback_startgametype",
    "codecallback_playerconnect",
    "codecallback_playerdisconnect",
    "codecallback_playerdamage",
    "codecallback_playerkilled",
];

#[derive(Debug, Default)]
pub struct ScriptProgram {
    modules: BTreeMap<String, Module>,
    pub(crate) animation_trees: Vec<(String, BTreeSet<String>)>,
}

#[derive(Debug, Default)]
struct Module {
    functions: BTreeMap<String, ast::Function>,
    animation_uses: Vec<(String, Option<String>)>,
    includes: Vec<String>,
    external: Vec<(String, String)>,
}

fn invalid(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

fn normalize(path: &str) -> io::Result<String> {
    let path = path
        .split(['\\', '/'])
        .filter(|s| !s.is_empty())
        .collect::<Vec<_>>()
        .join("/")
        .to_ascii_lowercase();
    if path.len() >= 1024 {
        return Err(invalid("script filename exceeds 1023 bytes"));
    }
    Ok(path)
}

// The engine formats both startup module names and the .gsc path in 64-byte buffers.
fn engine_path(mut path: String) -> String {
    path.truncate(path.floor_char_boundary(63));
    path
}

impl ScriptProgram {
    pub fn discover(
        gametype: &str,
        map: &str,
        developer_script: bool,
        mut read: impl FnMut(&str) -> io::Result<Vec<u8>>,
    ) -> io::Result<Self> {
        let mut program = Self::default();
        let gametype = engine_path(format!("maps/mp/gametypes/{gametype}"));
        let map = engine_path(format!("maps/mp/{map}"));
        let roots: [(&str, &[&str], bool); 5] = [
            ("codescripts/delete", &["main"], true),
            ("codescripts/struct", &["initstructs", "createstruct"], true),
            (&gametype, &["main"], true),
            ("maps/mp/gametypes/_callbacksetup", CALLBACKS, true),
            (&map, &["main"], false),
        ];
        for (root, labels, required) in roots {
            let root = normalize(root)?;
            let mut pending = vec![root.clone()];
            while let Some(name) = pending.pop() {
                if program.modules.contains_key(&name) {
                    continue;
                }
                let path = engine_path(format!("{name}.gsc"));
                let bytes = match read(&path) {
                    Ok(bytes) => bytes,
                    Err(e) if !required && name == root && e.kind() == io::ErrorKind::NotFound => {
                        continue;
                    }
                    Err(e) => {
                        return Err(io::Error::new(
                            e.kind(),
                            format!("could not read script {path}: {e}"),
                        ));
                    }
                };
                let module =
                    scan(&bytes, developer_script).map_err(|e| invalid(format!("{path}: {e}")))?;
                pending.extend(module.external.iter().rev().map(|(file, _)| file.clone()));
                pending.extend(module.includes.iter().rev().cloned());
                for (name, node) in &module.animation_uses {
                    let index = program
                        .animation_trees
                        .iter()
                        .position(|(tree, _)| tree == name)
                        .unwrap_or_else(|| {
                            program
                                .animation_trees
                                .push((name.clone(), BTreeSet::new()));
                            program.animation_trees.len() - 1
                        });
                    if let Some(node) = node {
                        program.animation_trees[index].1.insert(node.clone());
                    }
                }
                program.modules.insert(name, module);
            }
            if required {
                for label in labels {
                    if !program.has_label(&root, label) {
                        return Err(invalid(format!(
                            "could not find label '{label}' in script '{root}'"
                        )));
                    }
                }
            }
        }
        for (name, module) in &program.modules {
            for (file, label) in &module.external {
                if !program.has_label(file, label) {
                    return Err(invalid(format!("{name}: unknown function {file}::{label}")));
                }
            }
        }
        Ok(program)
    }

    fn has_label(&self, file: &str, label: &str) -> bool {
        self.resolve_function(file, label).is_some()
    }

    /// Resolve in the declaring file; direct includes import the original
    /// definition without importing that definition's own includes.
    pub fn resolve_function(&self, file: &str, label: &str) -> Option<(&str, &ast::Function)> {
        let (file, module) = self
            .modules
            .get_key_value(file)
            .or_else(|| self.modules.get_key_value(&normalize(file).ok()?))?;
        let lowercase;
        let label = if label.bytes().any(|c| c.is_ascii_uppercase()) {
            lowercase = label.to_ascii_lowercase();
            &lowercase
        } else {
            label
        };
        if let Some(function) = module.functions.get(label) {
            return Some((file, function));
        }
        module.includes.iter().find_map(|path| {
            self.modules
                .get(path)?
                .functions
                .get(label)
                .map(|body| (path.as_str(), body))
        })
    }

    pub fn function(&self, file: &str, label: &str) -> Option<&ast::Function> {
        self.resolve_function(file, label)
            .map(|(_, function)| function)
    }

    pub fn module_count(&self) -> usize {
        self.modules.len()
    }

    /// Each declaration belongs to its defining module, including functions
    /// imported by other modules and callable through their symbol tables.
    pub fn declarations(&self) -> impl Iterator<Item = (&str, &str, &ast::Function)> {
        self.modules.iter().flat_map(|(path, module)| {
            module
                .functions
                .iter()
                .map(move |(name, function)| (path.as_str(), name.as_str(), function))
        })
    }

    pub fn function_count(&self) -> usize {
        self.modules
            .values()
            .map(|module| module.functions.len())
            .sum()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn declarations_imports_comments_and_developer_blocks() {
        let source = br#"
            #include Maps\MP\Lib;
            #using_animtree("aidummy");
            // a\bad::call();
            MAIN(x) {
                Text = &"a\\bogus::func and \"escaped\" /#";
                /* { comment\ignored::fn } */
                x = mAPS\mp\A::fOO;
                self thread maps\mp\B::bar( ::handler );
                return ::Callback;
            }
            /# debugOnly() { missing\dev::trace(); } #/
        "#;
        let module = scan(source, false).expect("valid GSC");
        assert_eq!(
            module
                .functions
                .keys()
                .map(String::as_str)
                .collect::<Vec<_>>(),
            ["main"]
        );
        assert_eq!(module.includes, ["maps/mp/lib"]);
        assert_eq!(
            module.external,
            Vec::from([
                ("maps/mp/a".into(), "foo".into()),
                ("maps/mp/b".into(), "bar".into())
            ])
        );
        let module = scan(source, true).expect("developer block active");
        assert!(module.functions.contains_key("debugonly"));
        assert!(
            module
                .external
                .contains(&("missing/dev".into(), "trace".into()))
        );
    }

    #[test]
    fn loads_argument_reference_before_enclosing_call() {
        let module = scan(br"main() { outer::call(inner::make(), ::local); }", false)
            .expect("valid nested calls");
        assert_eq!(
            module.external,
            [
                ("inner".into(), "make".into()),
                ("outer".into(), "call".into()),
            ]
        );
    }

    fn fixtures() -> BTreeMap<String, Vec<u8>> {
        let callbacks: String = CALLBACKS
            .iter()
            .map(|name| format!("{name}() {{}}\n"))
            .collect();
        [
            ("codescripts/delete.gsc", "Main() {}".into()),
            (
                "codescripts/struct.gsc",
                "initStructs() {} createStruct() {}".into(),
            ),
            (
                "maps/mp/gametypes/test.gsc",
                "#include Common\\Core;".into(),
            ),
            ("maps/mp/gametypes/_callbacksetup.gsc", callbacks),
            (
                "common/core.gsc",
                "main() { maps\\mp\\gametypes\\test::main(); }".into(),
            ),
        ]
        .into_iter()
        .map(|(key, value)| (key.to_owned(), value.into_bytes()))
        .collect()
    }

    fn discover(files: &BTreeMap<String, Vec<u8>>) -> io::Result<ScriptProgram> {
        let mut reads = std::collections::BTreeSet::new();
        ScriptProgram::discover("TEST", "mp_missing", false, |path| {
            assert!(reads.insert(path.to_owned()), "reopened {path}");
            files
                .get(path)
                .cloned()
                .ok_or_else(|| io::ErrorKind::NotFound.into())
        })
    }

    #[test]
    fn handles_cycles_optional_level_and_direct_include_exports() {
        let mut files = fixtures();
        let program = discover(&files).expect("valid script closure");
        assert_eq!((program.module_count(), program.function_count()), (5, 9));
        assert_eq!(
            program
                .resolve_function(r"MAPS\MP\GAMETYPES\TEST", "Main")
                .expect("engine normalizes external lookups")
                .0,
            "common/core"
        );
        files.insert("common/core.gsc".into(), b"#include Deep;".to_vec());
        files.insert("deep.gsc".into(), b"main() {}".to_vec());
        assert!(
            discover(&files)
                .expect_err("imports do not propagate")
                .to_string()
                .contains("main")
        );
    }

    #[test]
    fn diagnoses_missing_module_or_required_label() {
        let mut files = fixtures();
        files.insert("codescripts/delete.gsc".into(), b"other() {}".to_vec());
        assert!(
            discover(&files)
                .expect_err("missing main")
                .to_string()
                .contains("codescripts/delete")
        );
        files.insert(
            "codescripts/delete.gsc".into(),
            b"main() { missing\\foo::bar(); }".to_vec(),
        );
        assert_eq!(
            discover(&files)
                .expect_err("missing external module")
                .kind(),
            io::ErrorKind::NotFound
        );
        files.insert("missing/foo.gsc".into(), b"other() {}".to_vec());
        assert!(
            discover(&files)
                .expect_err("missing external function")
                .to_string()
                .contains("missing/foo::bar")
        );
    }
}
