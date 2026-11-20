use super::*;
use crate::message::MessageReader;

#[test]
fn acknowledged_history_and_entity_client_merges() -> io::Result<()> {
    let mut history = SnapshotHistory::default();
    let before = Snapshot {
        entities: vec![EntityState::new(7), EntityState::new(12)],
        clients: vec![ClientState::new(1), ClientState::new(8)],
        ..Default::default()
    };
    history.record(10, before);
    let mut baseline = EntityState::new(1022);
    assert!(baseline.set_field("eType", 6));
    let frame = Snapshot {
        server_time: 400,
        flags: 5,
        entities: vec![EntityState::new(12), baseline.clone()],
        clients: vec![ClientState::new(8), ClientState::new(63)],
        ..Default::default()
    };
    let mut out = MessageWriter::new(1024);
    history.write(&mut out, 38, Some(10), &frame, &[baseline])?;
    out.write_byte(7)?;
    let mut msg = MessageReader::new(out.as_bytes());
    assert_eq!(msg.read_byte()?, 6);
    assert_eq!(msg.read_long()?, 400);
    assert_eq!(msg.read_byte()?, 28);
    assert_eq!(msg.read_byte()?, 5);
    assert_eq!(msg.read_byte()?, 0);
    assert_eq!(msg.read_bits(8)?, 0);
    assert_eq!(msg.read_bits(10)?, 7);
    assert!(msg.read_bit()?);
    assert_eq!(msg.read_bits(10)?, 1022);
    assert_eq!(msg.read_bits(2)?, 0);
    assert_eq!(msg.read_bits(10)?, 1023);
    assert!(msg.read_bit()?);
    assert_eq!(msg.read_bits(6)?, 1);
    assert!(msg.read_bit()?);
    assert!(msg.read_bit()?);
    assert_eq!(msg.read_bits(6)?, 63);
    assert_eq!(msg.read_bits(2)?, 0);
    assert!(!msg.read_bit()?);
    assert_eq!(msg.read_byte()?, 7);

    for (sequence, ack) in [(39, 10), (10, 10), (8, 10), (11, 0), (13, 12)] {
        assert!(history.base(sequence, Some(ack)).is_none());
    }
    history.record(42, Snapshot::default());
    assert!(history.base(12, Some(10)).is_none());
    Ok(())
}

#[test]
fn player_signed_zero_stats_inventory_objectives_and_hud() -> io::Result<()> {
    let mut frame = PlayerState::default();
    frame.set_field("origin[1]", (-0.0_f32).to_bits())?;
    let words = frame.words_mut();
    words[0x12c / 4] = 65_535;
    words[0x138 / 4] = 63;
    words[0x140 / 4] = 248;
    words[0x144 / 4 + 63] = 73;
    words[0x344 / 4 + 16] = 250;
    words[0x5e4 / 4] = 3;
    words[0x5e4 / 4 + 4] = 1022;
    words[0x1728 / 4] = 2;
    words[0x1728 / 4 + 2] = (-512.0_f32).to_bits();
    let mut out = MessageWriter::new(1024);
    player::write_delta(&mut out, None, &frame)?;
    let mut m = MessageReader::new(out.as_bytes());
    assert_eq!(m.read_byte()?, 2);
    assert!(!m.read_bit()?); // commandTime
    assert!(m.read_bit()?); // raw -0.0 differs from +0.0
    assert!(!m.read_bit()?); // compact player float, biased by 4096
    assert_eq!(m.read_bits(5)?, 0);
    assert_eq!(m.read_byte()?, 128);
    assert!(m.read_bit()?);
    assert_eq!(m.read_bits(6)?, 0b101001);
    assert_eq!(m.read_short()?, -1);
    assert_eq!(m.read_bits(6)?, 63);
    assert_eq!(m.read_byte()?, 248);
    assert!(m.read_bit()?);
    assert_eq!(m.read_bits(4)?, 8);
    assert_eq!(m.read_short()?, i16::MIN);
    assert_eq!(m.read_short()?, 73);
    assert_eq!(m.read_bits(2)?, 2);
    assert_eq!(m.read_short()?, 1);
    assert_eq!(m.read_short()?, 250);
    assert_eq!(m.read_bits(2)?, 0);
    assert!(m.read_bit()?); // objectives
    assert_eq!(m.read_bits(3)?, 3);
    assert!(m.read_bit()?);
    assert_eq!(m.read_bits(4)?, 0); // no origins or icon
    assert_eq!(m.read_bits(2)?, 3); // changed, nonzero entity
    assert_eq!(m.read_bits(2)?, 2);
    assert_eq!(m.read_byte()?, 255);
    assert!(!m.read_bit()?); // team
    for _ in 1..16 {
        assert_eq!(m.read_bits(4)?, 0);
    }
    assert!(m.read_bit()?); // HUD
    assert_eq!(m.read_bits(5)?, 1);
    assert_eq!(m.read_bits(5)?, 1); // y, type
    assert_eq!(m.read_bits(3)?, 3); // changed nonzero compact -512
    assert_eq!(m.read_bits(2)?, 0);
    assert_eq!(m.read_byte()?, 0);
    assert_eq!(m.read_bits(2)?, 3);
    assert_eq!(m.read_bits(4)?, 2);
    assert_eq!(m.read_bits(5)?, 0); // second HUD array
    Ok(())
}

#[test]
fn name_update_clears_previous_tail_and_invalid_indices_fail() {
    let mut client = ClientState::new(0);
    client.set_name(&[b'x'; 32]);
    assert_eq!(client.field("name[28]"), Some(0x00787878));
    client.set_name(b"ab\0suffix");
    assert_eq!(client.field("name[0]"), Some(0x6261));
    assert_eq!(client.field("name[28]"), Some(0));
    let history = SnapshotHistory::default();
    let frame = Snapshot {
        clients: vec![ClientState::new(64)],
        ..Default::default()
    };
    assert!(
        history
            .write(&mut MessageWriter::new(32), 1, None, &frame, &[])
            .is_err()
    );
    let frame = Snapshot {
        entities: vec![EntityState::new(1), EntityState::new(1)],
        ..Default::default()
    };
    assert!(
        history
            .write(&mut MessageWriter::new(32), 1, None, &frame, &[])
            .is_err()
    );
}
