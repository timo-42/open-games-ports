use super::{archive::Archive, refpack, sha256_hex};
fn archive(members: &[(&str, &[u8])]) -> Vec<u8> {
    let directory_end = 16
        + members
            .iter()
            .map(|(name, _)| 9 + name.len())
            .sum::<usize>();
    let length = directory_end + members.iter().map(|(_, data)| data.len()).sum::<usize>();
    let mut result = b"BIGF".to_vec();
    result.extend_from_slice(&(length as u32).to_be_bytes());
    result.extend_from_slice(&(members.len() as u32).to_be_bytes());
    result.extend_from_slice(&(directory_end as u32).to_be_bytes());
    let mut offset = directory_end;
    for (name, data) in members {
        result.extend_from_slice(&(offset as u32).to_be_bytes());
        result.extend_from_slice(&(data.len() as u32).to_be_bytes());
        result.extend_from_slice(name.as_bytes());
        result.push(0);
        offset += data.len();
    }
    for (_, data) in members {
        result.extend_from_slice(data);
    }
    result
}

fn packed(payload: &[u8]) -> Vec<u8> {
    assert!(payload.len() < 1 << 24);
    let mut output = vec![
        0x10,
        0xfb,
        (payload.len() >> 16) as u8,
        (payload.len() >> 8) as u8,
        payload.len() as u8,
    ];
    let mut cursor = 0;
    while payload.len() - cursor >= 4 {
        let count = (payload.len() - cursor).min(112) & !3;
        output.push(0xe0 + ((count - 4) / 4) as u8);
        output.extend_from_slice(&payload[cursor..cursor + count]);
        cursor += count;
    }
    output.push(0xfc + (payload.len() - cursor) as u8);
    output.extend_from_slice(&payload[cursor..]);
    output
}

#[test]
fn archive_preserves_names_reports_padding_and_rejects_ambiguous_lookups() {
    let mut data = archive(&[
        ("MENU.FSH", b"image"),
        ("menu.fsh", b"other"),
        ("../escape", b"retained"),
    ]);
    data.extend_from_slice(&[0, 0]);
    let index = Archive::parse(&data).unwrap();
    assert_eq!(index.magic, "BIGF");
    assert_eq!(index.warnings.len(), 1);
    assert_eq!(
        index
            .entries
            .iter()
            .map(|entry| entry.name.as_str())
            .collect::<Vec<_>>(),
        ["MENU.FSH", "menu.fsh", "../escape"]
    );
    assert!(
        index
            .read(&data, "menu.fsh")
            .unwrap_err()
            .contains("Ambiguous")
    );
    assert!(index.read(&data, "../escape").is_err());
    let mut big4 = archive(&[("folder\\name", b"image")]);
    big4[..4].copy_from_slice(b"BIG4");
    let length = big4.len() as u32;
    big4[4..8].copy_from_slice(&length.to_le_bytes());
    let index = Archive::parse(&big4).unwrap();
    assert_eq!(index.declared_size, big4.len());
    assert_eq!(index.read(&big4, "FOLDER/NAME").unwrap(), b"image");
}

#[test]
fn archive_bounds_and_overlap_are_validated_before_reading_members() {
    let good = archive(&[("first", b"data"), ("second", b"more")]);
    for (offset, value) in [
        (8, u32::MAX),
        (12, u32::MAX),
        (16, 0),
        (20, u32::MAX),
        (16 + 9 + 5, (good.len() - 7) as u32),
    ] {
        let mut broken = good.clone();
        broken[offset..offset + 4].copy_from_slice(&value.to_be_bytes());
        assert!(
            Archive::parse(&broken).is_err(),
            "accepted invalid integer at {offset}"
        );
    }
    for end in 0..good.len() {
        assert!(
            Archive::parse(&good[..end]).is_err(),
            "accepted prefix of {end} bytes"
        );
    }
    let mut exact_duplicate = good.clone();
    let first_offset: [u8; 4] = exact_duplicate[16..20].try_into().unwrap();
    exact_duplicate[30..34].copy_from_slice(&first_offset);
    assert!(Archive::parse(&exact_duplicate).is_ok());
}

#[test]
fn refpack_control_families_include_overlapping_copies() {
    let vectors: &[(&[u8], &[u8])] = &[
        (b"\xe0ABCD\xfc", b"ABCD"),
        (b"\xffABC", b"ABC"),
        (b"\x03\x02ABC\xfc", b"ABCABC"),
        (b"\x01\x00A\xfc", b"AAAA"),
        (b"\x80\xc0\x02ABC\xfc", b"ABCABCA"),
        (b"\xc3\x00\x02\x00ABC\xfc", b"ABCABCAB"),
    ];
    for (stream, expected) in vectors {
        let mut data = vec![0x10, 0xfb, 0, 0, expected.len() as u8];
        data.extend_from_slice(stream);
        assert_eq!(refpack::decompress(&data, 1024).unwrap(), *expected);
    }
    let bytes: Vec<_> = (0..255).collect();
    assert_eq!(refpack::decompress(&packed(&bytes), 255).unwrap(), bytes);
    assert!(refpack::decompress(&packed(&bytes), 254).is_err());
    assert_eq!(refpack::decompress(&packed(b""), 0).unwrap(), b"");
}

#[test]
fn refpack_rejects_truncation_and_invalid_output_sizes() {
    for data in [
        b"\x10\xfb\0\0\x04\xe0AB".as_slice(),
        b"\x10\xfb\0\0\x03\0\0\xfc",
        b"\x10\xfb\0\0\x04\xe0ABCD",
        b"\x10\xfb\0\0\x03\xe0ABCD\xfc",
        b"\x90\xfb\0\0\0",
        b"\x10\xfb\0\0\x01\xfc",
        b"\x10\xfb\0\0\0\xc0\0",
    ] {
        assert!(
            refpack::decompress(data, 1024).is_err(),
            "accepted {data:?}"
        );
    }
    let valid = packed(b"bounded literals and repeated data");
    for end in 0..valid.len() {
        assert!(refpack::decompress(&valid[..end], 1024).is_err());
    }
}

#[test]
fn sha256_matches_published_short_and_multiblock_vectors() {
    for (input, expected) in [
        (
            b"".as_slice(),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        ),
        (
            b"abc".as_slice(),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        ),
        (
            b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq".as_slice(),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        ),
    ] {
        assert_eq!(sha256_hex(input), expected);
    }
    assert_eq!(
        sha256_hex(&vec![b'a'; 1_000_000]),
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"
    );
}
