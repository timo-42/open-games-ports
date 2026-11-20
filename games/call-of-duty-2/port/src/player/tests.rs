use super::*;
use crate::{
    assets::{GameAssets, weapons::WeaponRegistry},
    config::ServerConfig,
    vm::fiber::Host,
};
use std::fs;

#[test]
fn names_keep_nonblack_escapes_and_whole_bytes() {
    assert_eq!(clean_name(b" ^0^0   ^1^x^"), b"UnnamedPlayer");
    assert_eq!(clean_name(b"^1  x    ^2 y^"), b"^1  x   ^2y");
    assert_eq!(
        clean_name(&[vec![b'a'; 30], b"^3z".to_vec()].concat()),
        vec![b'a'; 30]
    );
    assert_eq!(
        clean_name(&[vec![b'a'; 29], b"^3z".to_vec()].concat()),
        [vec![b'a'; 29], b"^3".to_vec()].concat()
    );
    assert_eq!(clean_name(b"  ^0\xe9\0x"), b"\xe9");
}

#[test]
fn connect_begin_userinfo_menu_disconnect() -> io::Result<()> {
    let dir = tempfile::tempdir()?;
    let main = dir.path().join("main");
    fs::create_dir_all(main.join("weapons/mp"))?;
    fs::create_dir_all(main.join("mp"))?;
    fs::write(main.join("mp/playeranimtypes.txt"), b"none")?;
    fs::write(main.join("weapons/mp/defaultweapon_mp"), b"WEAPONFILE")?;
    let assets = GameAssets::mount_paths(dir.path(), None, "")?;
    let mut host = GameHost {
        assets: &assets,
        models: &mut Default::default(),
        weapons: &mut WeaponRegistry::load(&assets)?,
        start_time: 0,
        collision: &crate::collision::tests::map(-1, -2, &[])?,
        world: &mut Default::default(),
        config: &mut ServerConfig::from_args(&[]),
        strings: &mut Default::default(),
        time_ms: 100,
        initializing: false,
    };
    host.strings.set(0x17, b"joining")?;
    host.strings.set(0x4de + 4, b"serverinfo")?;
    let program = ScriptProgram::discover("test", "mp_test", false, |p| {
        Ok(match p {
            "codescripts/delete.gsc" => b"main() {}".to_vec(),
            "codescripts/struct.gsc" => b"initStructs() {} createStruct() {}".to_vec(),
            "maps/mp/gametypes/test.gsc" => b"main() {}".to_vec(),
            "maps/mp/gametypes/_callbacksetup.gsc" => br#"
            codecallback_startgametype() {}
            codecallback_playerconnect() {
                self endon("disconnect");
                self.statusicon = "joining";
                self waittill("begin");
                self.statusicon = "";
                self setClientCvar("ui_allow_test","0");
                level notify("connected", self);
                self waittill("menuresponse", menu, choice);
                self logchoice(menu,choice);
            }
            logchoice(menu,choice) { logprint(menu + ":" + choice); }
            codecallback_playerdisconnect() { self notify("disconnect"); }
            codecallback_playerdamage() {} codecallback_playerkilled() {}
        "#
            .to_vec(),
            _ => return Err(io::ErrorKind::NotFound.into()),
        })
    })?;
    let mut vm = FiberVm::default();
    let slot = 2;
    connect(
        &mut vm,
        &program,
        &mut host,
        Connect {
            slot,
            userinfo: br"\name\  ^1Player\cg_predictItems\1",
            persistent: Default::default(),
            local: false,
            guid: 42,
        },
    )?;
    let p = host
        .world
        .players
        .get(slot)
        .expect("connected fixture player");
    let object = p.object;
    assert_eq!(
        (
            p.connected,
            p.team(),
            p.session_state,
            p.spectator_client,
            p.use_hold_entity
        ),
        (false, 3, 2, -1, 1023)
    );
    assert_eq!(p.state.field("clientNum"), Some(2));
    assert_eq!(
        host.field(object, "statusicon")?,
        Value::String(b"joining".to_vec())
    );
    assert!(p.predict_item_pickup);
    assert_eq!(p.name, b"^1Player");
    assert_eq!(
        host.builtin(None, "isalive", vec![Value::Object(object)])?,
        Value::Int(0)
    );
    host.set_field(object, "health", Value::Int(70))?;
    assert_eq!(
        host.builtin(None, "isalive", vec![Value::Object(object)])?,
        Value::Int(1)
    );
    host.set_field(object, "maxhealth", Value::Int(50))?;
    assert_eq!(host.world.field(object, "health")?, Value::Int(50));
    assert_eq!(
        host.world
            .players
            .get(slot)
            .expect("connected fixture player")
            .state
            .words()[0x12c / 4],
        50
    );
    host.strings.set(0x1f, b"friend")?;
    host.set_field(object, "headicon", Value::String(b"FRIEND".to_vec()))?;
    host.set_field(object, "headiconteam", Value::String(b"allies".to_vec()))?;
    assert_eq!(
        host.field(object, "headicon")?,
        Value::String(b"friend".to_vec())
    );
    assert_eq!(
        host.world
            .entities
            .get(slot)
            .expect("connected fixture player")
            .state
            .field("iHeadIconTeam"),
        Some(2)
    );
    begin(&mut vm, &mut host, slot)?;
    host.strings.take_changed();
    host.builtin(
        None,
        "iprintln",
        vec![
            Value::LocalizedString(b"MP_CONNECTED".to_vec()),
            Value::Object(object),
        ],
    )?;
    assert!(matches!(host.strings.take_changed().as_slice(),
        [crate::level::ServerUpdate::Broadcast(cmd)] if cmd == b" \"MP_CONNECTED\x15^1Player^7\""));
    vm.run_current(&program, &mut host)?;
    assert_eq!(
        host.world
            .players
            .get(slot)
            .expect("connected fixture player")
            .state
            .field("pm_type"),
        Some(4)
    );
    assert_eq!(
        host.world.players.take_commands(slot),
        [b"v ui_allow_test \"0\"".to_vec()]
    );
    host.builtin(
        None,
        "setclientnamemode",
        vec![Value::String(b"manual_change".to_vec())],
    )?;
    userinfo_changed(&mut host, slot, br"\name\bad;value")?;
    assert_eq!(
        host.world
            .players
            .get(slot)
            .expect("connected fixture player")
            .name,
        b"^1Player"
    );
    host.builtin(None, "updateclientnames", vec![])?;
    assert_eq!(
        host.world
            .players
            .get(slot)
            .expect("connected fixture player")
            .name,
        b"badinfo"
    );
    let tokens = |id: &[u8]| {
        vec![
            b"mr".to_vec(),
            id.to_vec(),
            b"4".to_vec(),
            b"continue".to_vec(),
        ]
    };
    client_command(&mut vm, &program, &mut host, slot, 16, &tokens(b"15"))?;
    vm.run_current(&program, &mut host)?;
    assert!(host.world.players.game_log.is_empty());
    client_command(&mut vm, &program, &mut host, slot, 16, &tokens(b"16"))?;
    vm.run_current(&program, &mut host)?;
    assert_eq!(
        host.world.players.game_log,
        [(host.time_ms, b"serverinfo:continue".to_vec())]
    );
    client_command(
        &mut vm,
        &program,
        &mut host,
        slot,
        16,
        &[b"say".to_vec(), b"hello".to_vec()],
    )?;
    assert_eq!(
        host.world.players.take_commands(slot),
        [b"h \"\x15\x15(\x14GAME_SPECTATOR\x15)badinfo^7: ^7hello\"".to_vec()]
    );
    check_dev_commands(&mut vm, &program, &mut host, slot, object)?;
    check_score_commands(&mut host, slot, object)?;
    disconnect(&mut vm, &program, &mut host, slot)?;
    assert!(host.world.players.get(slot).is_none());
    assert!(host.world.entities.object_for_slot(slot).is_none());
    Ok(())
}

fn check_dev_commands(
    vm: &mut FiberVm,
    program: &ScriptProgram,
    host: &mut GameHost<'_>,
    slot: u16,
    object: u32,
) -> io::Result<()> {
    let mut cmd = |host: &mut GameHost<'_>, text: &str| {
        let args = text
            .split_ascii_whitespace()
            .map(|s| s.as_bytes().to_vec())
            .collect::<Vec<_>>();
        client_command(vm, program, host, slot, 16, &args)
    };
    cmd(host, "god")?;
    assert_eq!(
        host.world.players.take_commands(slot),
        [b"e \"GAME_CHEATSNOTENABLED\"".to_vec()]
    );
    host.config
        .execute("devmap mp_test", |_| Err(io::ErrorKind::NotFound.into()))?;
    host.set_field(object, "health", Value::Int(0))?;
    cmd(host, "god")?;
    assert_eq!(
        host.world.players.take_commands(slot),
        [b"e \"GAME_MUSTBEALIVECOMMAND\"".to_vec()]
    );
    host.set_field(object, "health", Value::Int(30))?;
    cmd(host, "god")?;
    cmd(host, "ufo")?;
    cmd(host, "give health 5")?;
    assert_eq!(host.world.field(object, "health")?, Value::Int(35));
    cmd(host, "take health 99")?;
    assert_eq!(host.world.field(object, "health")?, Value::Int(1));
    assert_eq!(
        host.world.players.take_commands(slot),
        [
            b"e \"GAME_GODMODE_ON\"".to_vec(),
            b"e \"GAME_UFOON\"".to_vec()
        ]
    );
    Ok(())
}

fn check_score_commands(host: &mut GameHost<'_>, slot: u16, object: u32) -> io::Result<()> {
    host.set_field(object, "sessionteam", Value::String(b"allies".to_vec()))?;
    assert_eq!(
        host.builtin(
            None,
            "getteamplayersalive",
            vec![Value::String(b"allies".to_vec())]
        )?,
        Value::Int(1)
    );
    host.builtin(
        None,
        "setwinningteam",
        vec![Value::String(b"axis".to_vec())],
    )?;
    assert_eq!(host.strings.get(0x16), Some(br"\winner\-1".as_slice()));
    host.builtin(None, "setwinningplayer", vec![Value::Object(object)])?;
    assert_eq!(host.strings.get(0x16), Some(br"\winner\3".as_slice()));
    host.world.players.team_scores = [5, 9];
    host.builtin(Some(object), "updatescores", vec![])?;
    host.builtin(Some(object), "showscoreboard", vec![])?;
    host.builtin(
        Some(object),
        "sayteam",
        vec![Value::LocalizedString(b"HELLO".to_vec())],
    )?;
    host.builtin(
        None,
        "clientannouncement",
        vec![
            Value::Object(object),
            Value::LocalizedString(b"NOTICE".to_vec()),
        ],
    )?;
    assert_eq!(
        host.world.players.take_commands(slot),
        [
            b"H 9".to_vec(),
            b"G 5".to_vec(),
            b"b 1 5 9 2 0 0 0 0".to_vec(),
            b"i \"\x15\x15^8(\x14GAME_DEAD\x15)(\x14GAME_ALLIES\x15)badinfo^7: ^5\x14HELLO\""
                .to_vec(),
            b"c \"NOTICE\" 2".to_vec()
        ]
    );
    Ok(())
}
