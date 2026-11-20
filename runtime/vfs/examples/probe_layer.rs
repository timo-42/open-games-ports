//! Read-only package probe; optional member hash streams through the VFS.
use open_games_vfs::layers::{Layer, Sources};
use sha2::{Digest, Sha256};
use std::io::Read;
fn main() -> Result<(), String> {
    let mut args = std::env::args().skip(1);
    let path = args.next().ok_or("usage: probe_layer SOURCE [MEMBER]")?;
    let layer = Layer::open(std::path::Path::new(&path), "probe")?;
    for artifact in &layer.artifacts {
        println!(
            "artifact={} bytes={} sha256={}",
            artifact.name, artifact.bytes, artifact.sha256
        );
    }
    for warning in &layer.warnings {
        eprintln!("{warning}");
    }
    let sources = Sources::new(vec![layer], "probe-v1")?;
    let files = sources.files()?;
    println!("files={}", files.len());
    if let Some(member) = args.next() {
        let mut stream = sources.open(&member)?;
        let mut hash = Sha256::new();
        let mut block = [0; 65536];
        let mut size = 0;
        loop {
            let n = stream.read(&mut block).map_err(|e| e.to_string())?;
            if n == 0 {
                break;
            }
            hash.update(&block[..n]);
            size += n;
        }
        println!("member={member} bytes={size} sha256={:x}", hash.finalize());
    } else {
        for file in files {
            println!("{file}");
        }
    }
    Ok(())
}
