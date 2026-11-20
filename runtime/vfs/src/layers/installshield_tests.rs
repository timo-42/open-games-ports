use super::*;
use std::io::{Read, Write};
fn put(data: &mut [u8], offset: usize, value: u32) {
    data[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
}
fn fixture(compressed: bool) -> (tempfile::TempDir, Vec<u8>) {
    let temp = tempfile::tempdir().unwrap();
    let payload = b"synthetic original CAB payload".to_vec();
    let mut packed = payload.clone();
    if compressed {
        let mut encoder =
            flate2::write::DeflateEncoder::new(Vec::new(), flate2::Compression::default());
        encoder.write_all(&payload).unwrap();
        let block = encoder.finish().unwrap();
        packed = (block.len() as u16).to_le_bytes().to_vec();
        packed.extend(block);
    }
    let mut hdr = vec![0; 2048];
    hdr[..4].copy_from_slice(b"ISc(");
    put(&mut hdr, 4, 0x01009500);
    put(&mut hdr, 12, 32);
    let cab = 32;
    let table = 1024;
    let desc = 1100;
    put(&mut hdr, cab + 12, (table - cab) as u32);
    put(&mut hdr, cab + 28, 1);
    put(&mut hdr, cab + 40, 1);
    put(&mut hdr, cab + 44, (desc - table) as u32);
    put(&mut hdr, cab + 62, 700);
    put(&mut hdr, cab + 700 + 4, 750);
    put(&mut hdr, cab + 750, 800);
    put(&mut hdr, cab + 750 + 22, 0);
    put(&mut hdr, cab + 750 + 26, 0);
    hdr[cab + 800..cab + 804].copy_from_slice(b"Base");
    put(&mut hdr, table, 300);
    hdr[table + 300..table + 304].copy_from_slice(b"Data");
    hdr[desc..desc + 2].copy_from_slice(&(if compressed { 4u16 } else { 0 }).to_le_bytes());
    hdr[desc + 2..desc + 10].copy_from_slice(&(payload.len() as u64).to_le_bytes());
    hdr[desc + 10..desc + 18].copy_from_slice(&(packed.len() as u64).to_le_bytes());
    hdr[desc + 18..desc + 26].copy_from_slice(&512u64.to_le_bytes());
    hdr[desc + 26..desc + 42].copy_from_slice(&Md5::digest(&payload));
    put(&mut hdr, desc + 58, 310);
    hdr[table + 310..table + 318].copy_from_slice(b"Test.bin");
    hdr[desc + 85..desc + 87].copy_from_slice(&1u16.to_le_bytes());
    let mut volume = vec![0; 512];
    volume.extend(packed);
    fs::write(temp.path().join("data1.cab"), volume).unwrap();
    fs::write(temp.path().join("data1.hdr"), hdr).unwrap();
    (temp, payload)
}
#[test]
fn stored_and_deflate_cabinets_are_verified_seekable_vfs_layers() {
    for compressed in [false, true] {
        let (temp, expected) = fixture(compressed);
        let source = super::super::Layer::open(&temp.path().join("data1.cab"), "base").unwrap();
        assert_eq!(source.artifacts.len(), 2);
        assert_eq!(source.files().unwrap(), ["groups/base/data/test.bin"]);
        let view = source
            .select(
                &[("groups/base/data/test.bin", "mods/bf2/test.bin")],
                "game-map",
            )
            .unwrap();
        let sources = super::super::Sources::new(vec![view], "test").unwrap();
        assert_eq!(sources.read("MODS/BF2/Test.bin").unwrap(), expected);
        let mut stream = sources.open("mods/bf2/test.bin").unwrap();
        stream.seek(SeekFrom::Start(10)).unwrap();
        let mut tail = Vec::new();
        stream.read_to_end(&mut tail).unwrap();
        assert_eq!(tail, expected[10..]);
    }
}
#[test]
fn rejects_corrupt_payload_traversal_cycles_missing_volume_and_split_flags() {
    let (temp, _) = fixture(true);
    let path = temp.path().join("data1.cab");
    let mut volume = fs::read(&path).unwrap();
    volume[514] ^= 0xff;
    fs::write(&path, volume).unwrap();
    let source = super::super::Layer::open(&path, "corrupt").unwrap();
    assert!(source.open_file("groups/base/data/test.bin").is_err());
    let hdrpath = temp.path().join("data1.hdr");
    let original = fs::read(&hdrpath).unwrap();
    let mut bad = original.clone();
    bad[1100..1102].copy_from_slice(&5u16.to_le_bytes());
    assert!(Archive::index(&bad).is_err());
    let mut bad = original.clone();
    bad[1334..1338].copy_from_slice(b"../x");
    assert!(Archive::index(&bad).is_err());
    let mut bad = original.clone();
    put(&mut bad, 32 + 700 + 8, 700);
    assert!(Archive::index(&bad).is_err());
    fs::remove_file(path).unwrap();
    assert!(Archive::open(&hdrpath).is_err());
    for n in 0..original.len() {
        let _ = Archive::index(&original[..n]);
    }
}

#[test]
fn embedded_setup_cabinets_remain_ranges_in_the_original_executable() {
    let (temp, expected) = fixture(true);
    let mut exe = vec![0; 512];
    exe[..2].copy_from_slice(b"MZ");
    put(&mut exe, 60, 64);
    exe[64..68].copy_from_slice(b"PE\0\0");
    exe[70..72].copy_from_slice(&1u16.to_le_bytes());
    put(&mut exe, 104, 512);
    for name in ["data1.cab", "data1.hdr"] {
        let bytes = fs::read(temp.path().join(name)).unwrap();
        exe.extend(format!("{name}\0Disk1\\{name}\x000.0.0.0\0{}\0", bytes.len()).as_bytes());
        exe.extend(bytes);
    }
    let path = temp.path().join("update.exe");
    fs::write(&path, &exe).unwrap();
    let layer = super::super::Layer::open(&path, "embedded-cab").unwrap();
    assert_eq!(layer.artifacts.len(), 1);
    let source = super::super::Sources::new(vec![layer], "embedded").unwrap();
    assert_eq!(source.read("groups/base/data/test.bin").unwrap(), expected);
    assert_eq!(fs::read(path).unwrap(), exe);
    let mut delta = exe[..512].to_vec();
    for (name, bytes) in [
        ("PATCH.RTD", b"MODIFY ADD DELETE".as_slice()),
        ("PATCH.RTP", b"K*\x20\x03".as_slice()),
    ] {
        delta.extend(format!("{name}\0Disk1\\{name}\x000.0.0.0\0{}\0", bytes.len()).as_bytes());
        delta.extend(bytes);
    }
    let path = temp.path().join("delta.exe");
    fs::write(&path, delta).unwrap();
    assert!(
        super::super::Layer::open(&path, "delta")
            .unwrap_err()
            .contains("RTPatch")
    );
}
