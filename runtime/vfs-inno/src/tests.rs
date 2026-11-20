#![allow(clippy::unwrap_used)]

use std::io::Write;

use flate2::Compression;
use flate2::write::ZlibEncoder;

use super::*;

fn md5_hex(bytes: &[u8]) -> String {
    hex(&Md5::digest(bytes))
}

fn zlib(bytes: &[u8]) -> Vec<u8> {
    let mut encoder = ZlibEncoder::new(Vec::new(), Compression::default());
    encoder.write_all(bytes).unwrap();
    encoder.finish().unwrap()
}

fn entry(index: usize, packed: u64, before: Option<&str>, after: Option<&str>) -> GogEntry {
    GogEntry {
        index,
        packed,
        before_install: before.map(str::to_owned),
        after_install: after.map(str::to_owned),
    }
}

#[test]
fn pascal_calls_parse_quoted_and_bare_arguments() {
    assert_eq!(
        pascal_call(
            "before_install('ab', 'VIDEOSMK\\10.smk', 2)",
            "before_install"
        ),
        Some(vec!["ab".into(), "VIDEOSMK\\10.smk".into(), "2".into()])
    );
    assert_eq!(
        pascal_call("  after_install ( 'it''s, odd' ,7 , 9 ) ", "after_install"),
        Some(vec!["it's, odd".into(), "7".into(), "9".into()])
    );
    // A different call, a prefix of another name, trailing text, no close.
    assert_eq!(pascal_call("check_if_install('x')", "before_install"), None);
    assert_eq!(
        pascal_call("before_install_dependency('a', 'b', 1)", "before_install"),
        None
    );
    assert_eq!(
        pascal_call("after_install('a', 1, 2) junk", "after_install"),
        None
    );
    assert_eq!(pascal_call("after_install('a', 1", "after_install"), None);
}

#[test]
fn layouts_are_classified_from_entries_only() {
    assert_eq!(layout(false, 0, 0), Ok(Layout::Plain));
    assert_eq!(layout(false, 3, 7), Ok(Layout::Plain));
    assert_eq!(layout(true, 0, 1500), Ok(Layout::Legacy));
    assert_eq!(layout(true, 1600, 0), Ok(Layout::Galaxy));
    assert_eq!(layout(true, 1600, 2), Ok(Layout::Galaxy));
    assert!(layout(true, 0, 0).is_err());
    assert!(gog_temp_path(
        "{tmp}/d6/b0\\d6b0c99491789bb06f4173693e747b7d"
    ));
    assert!(!gog_temp_path("{tmp}\\background.jpg"));
    assert!(!gog_temp_path("{app}\\d6b0c99491789bb06f4173693e747b7d"));
}

#[test]
fn install_hooks_give_files_with_consecutive_parts_and_skip_redistributables() {
    let a = "0fd0e5e5cda0f86da1c9460504c5f107";
    let p1 = "756a426b7896b65ac129237352b8596a";
    let p2 = "b9c00f849751ff627437c749d7616e84";
    let entries = [
        entry(
            1,
            50,
            Some(
                "before_install_dependency('2e87c207550a0577279abc82aea68b14', '__redist\\x.exe', 2)",
            ),
            Some("after_install_dependency('2e87c207550a0577279abc82aea68b14', 50, 60)"),
        ),
        entry(
            2,
            5,
            None,
            Some("after_install_dependency('ffffffffffffffffffffffffffffffff', 5, 6)"),
        ),
        entry(
            3,
            10,
            Some(&format!("before_install('{a}', 'VIDEOSMK\\10.smk', 2)")),
            Some(&format!("after_install('{p1}', 10, 100)")),
        ),
        entry(
            4,
            20,
            None,
            Some(&format!("after_install('{p2}', 20, 200)")),
        ),
        entry(
            5,
            3,
            Some(
                "before_install('8bc8f170bae88e2def989e798f9dd3e2', '__support/save\\Game.dat', 1)",
            ),
            Some("after_install('8bc8f170bae88e2def989e798f9dd3e2', 3, 9)"),
        ),
    ];
    let files = gog_files_from_install_hooks(&entries).unwrap().unwrap();
    assert_eq!(files.len(), 2);
    assert_eq!(files[0].path, "VIDEOSMK\\10.smk");
    assert_eq!(files[0].md5, a);
    assert_eq!(files[0].bytes, 300);
    assert_eq!(
        files[0]
            .parts
            .iter()
            .map(|p| (p.index, p.packed, p.bytes))
            .collect::<Vec<_>>(),
        [(3, 10, 100), (4, 20, 200)]
    );
    assert_eq!(files[1].path, "__support/save\\Game.dat");
    assert_eq!(files[1].bytes, 9);
}

#[test]
fn install_hooks_reject_broken_part_lists_and_defer_unhooked_payloads() {
    let file = |index, parts| {
        entry(
            index,
            4,
            Some(&format!(
                "before_install('0fd0e5e5cda0f86da1c9460504c5f107', 'a.bin', {parts})"
            )),
            Some("after_install('0fd0e5e5cda0f86da1c9460504c5f107', 4, 8)"),
        )
    };
    // A second file starting before the first has all its parts.
    assert!(gog_files_from_install_hooks(&[file(1, 2), file(2, 1)]).is_err());
    // A file whose last part is missing.
    assert!(gog_files_from_install_hooks(&[file(1, 2)]).is_err());
    // Packed size disagreeing with the installer's own location.
    let mut wrong = file(1, 1);
    wrong.packed = 5;
    assert!(gog_files_from_install_hooks(&[wrong]).is_err());
    // Bad MD5 text.
    let bad = entry(
        1,
        4,
        Some("before_install('xyz', 'a.bin', 1)"),
        Some("after_install('xyz', 4, 8)"),
    );
    assert!(gog_files_from_install_hooks(&[bad]).is_err());
    // Payloads without hooks: the caller's .hashdb maps them.
    assert_eq!(
        gog_files_from_install_hooks(&[entry(1, 4, None, None)]),
        Ok(None)
    );
    assert_eq!(gog_files_from_install_hooks(&[]), Ok(None));
}

#[test]
fn gog_parts_are_joined_and_every_md5_is_checked() {
    let (one, two) = (b"first part ".repeat(50), b"second".repeat(9));
    let whole = [one.as_slice(), two.as_slice()].concat();
    let parts = [
        GogPart {
            index: 7,
            packed: 0,
            bytes: one.len() as u64,
            md5: md5_hex(&one),
        },
        GogPart {
            index: 8,
            packed: 0,
            bytes: two.len() as u64,
            md5: md5_hex(&two),
        },
    ];
    let stored = [zlib(&one), zlib(&two)];
    let len = whole.len() as u64;
    assert_eq!(
        join_gog_parts(&parts, &stored, len, &md5_hex(&whole)).unwrap(),
        whole
    );
    // Whole-file MD5, part MD5, part length and part count mismatches.
    assert!(join_gog_parts(&parts, &stored, len, &md5_hex(b"other")).is_err());
    let mut bad = parts.clone();
    bad[1].md5 = md5_hex(b"other");
    assert!(join_gog_parts(&bad, &stored, len, &md5_hex(&whole)).is_err());
    let mut short = parts.clone();
    short[0].bytes -= 1;
    assert!(join_gog_parts(&short, &stored, len, &md5_hex(&whole)).is_err());
    assert!(join_gog_parts(&parts, &stored[..1], len, &md5_hex(&whole)).is_err());
    // A corrupt zlib stream.
    let mut corrupt = stored.clone();
    corrupt[0][4] ^= 0xff;
    assert!(join_gog_parts(&parts, &corrupt, len, &md5_hex(&whole)).is_err());
}
