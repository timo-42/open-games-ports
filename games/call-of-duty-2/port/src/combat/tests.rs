use super::*;
use crate::{
    assets::{GameAssets, weapons::WeaponRegistry},
    snapshot::PlayerState,
    usercmd::UserCmd,
    vm::value::Value,
};
use std::{fs, io};
fn weapons() -> io::Result<WeaponRegistry> {
    let root = tempfile::tempdir()?;
    let main = root.path().join("main");
    let dir = main.join("weapons/mp");
    fs::create_dir_all(&dir)?;
    fs::create_dir_all(main.join("mp"))?;
    fs::write(main.join("mp/playeranimtypes.txt"), b"none")?;
    fs::write(dir.join("defaultweapon_mp"), b"WEAPONFILE")?;
    fs::write(dir.join("rifle"), br"WEAPONFILE\weaponSlot\primary\ammoName\round\clipName\rifleclip\startAmmo\12\maxAmmo\20\clipSize\4\fireTime\0.12\reloadTime\0.6\reloadEmptyTime\0.8\reloadAddTime\0.5\hipSpreadFireAdd\0.2")?;
    fs::write(dir.join("grenade"),br"WEAPONFILE\weaponType\grenade\weaponClass\grenade\offhandClass\Frag Grenade\clipOnly\1\ammoName\grenade\clipName\grenade\clipSize\3\startAmmo\3")?;
    let assets = GameAssets::mount_paths(root.path(), None, "")?;
    let mut weapons = WeaponRegistry::load(&assets)?;
    for name in ["rifle", "grenade"] {
        weapons.resolve(&assets, name, true)?;
    }
    Ok(weapons)
}
#[test]
fn inventory_shared_playerstate_and_reliable_commands() -> io::Result<()> {
    let weapons = weapons()?;
    let mut state = PlayerState::default();
    let mut commands = Vec::new();
    let given = Value::String(b"rifle".to_vec());
    let arg = std::slice::from_ref(&given);
    dispatch(
        "giveweapon",
        arg,
        true,
        b"test",
        &mut state,
        &weapons,
        &mut commands,
    )
    .expect("registered weapon method")?;
    dispatch(
        "switchtoweapon",
        arg,
        true,
        b"test",
        &mut state,
        &weapons,
        &mut commands,
    )
    .expect("registered weapon method")?;
    assert_eq!(commands, [br#"I "1""#.to_vec(), b"a 2".to_vec()]);
    let def = weapons.get(2).expect("registered fixture weapon");
    assert_eq!(state.words()[inventory::ammo(def)], 8);
    assert_eq!(state.words()[inventory::clip(def)], 4);
    assert_eq!(
        dispatch(
            "getammocount",
            arg,
            true,
            b"test",
            &mut state,
            &weapons,
            &mut commands
        )
        .expect("registered weapon method")?,
        Value::Int(12)
    );
    let missing = [Value::String(b"unregistered".to_vec())];
    assert_eq!(
        dispatch(
            "getammocount",
            &missing,
            true,
            b"test",
            &mut state,
            &weapons,
            &mut commands
        )
        .expect("registered weapon method")?,
        Value::Int(0)
    );
    let ordnance = Value::String(b"grenade".to_vec());
    dispatch(
        "giveweapon",
        &[ordnance],
        true,
        b"test",
        &mut state,
        &weapons,
        &mut commands,
    )
    .expect("registered weapon method")?;
    assert_eq!(&commands[2..], &[b"C 3".to_vec(), br#"I "1""#.to_vec()]);
    assert_eq!(selectable_weapons(&state, &weapons), [12, 0, 0, 0]);
    dispatch(
        "takeweapon",
        arg,
        true,
        b"test",
        &mut state,
        &weapons,
        &mut commands,
    )
    .expect("registered weapon method")?;
    assert_eq!(selectable_weapons(&state, &weapons), [8, 0, 0, 0]);
    assert_eq!(state.words()[inventory::ammo(def)], 0);
    Ok(())
}
#[test]
fn weapon_frame_predicts_shot_and_timed_reload() -> io::Result<()> {
    let weapons = weapons()?;
    let def = weapons.get(2).expect("registered fixture weapon");
    let mut state = PlayerState::default();
    let mut commands = Vec::new();
    for method in ["giveweapon", "setspawnweapon"] {
        dispatch(
            method,
            &[Value::String(b"rifle".to_vec())],
            true,
            b"test",
            &mut state,
            &weapons,
            &mut commands,
        )
        .expect("registered weapon method")?;
    }
    let fire = UserCmd {
        server_time: 50,
        weapon: 2,
        buttons: 1,
        ..Default::default()
    };
    step(&mut state, &weapons, &fire, &UserCmd::default(), 50)?;
    assert_eq!(state.words()[0x34 / 4], 119); // WEAPONFILE converts through single-precision seconds.
    assert_eq!(state.words()[0xa8 / 4], 0x9e);
    assert_eq!(state.words()[inventory::clip(def)], 3);
    let reload = UserCmd {
        buttons: 0x10,
        ..fire
    };
    step(&mut state, &weapons, &reload, &fire, 50)?;
    assert_eq!(state.words()[0xd8 / 4], 5);
    assert_eq!(&state.words()[0xa8 / 4..0xa8 / 4 + 3], &[0x9e, 0x95, 0x96]);
    let idle = UserCmd { buttons: 0, ..fire };
    for _ in 0..9 {
        step(&mut state, &weapons, &idle, &idle, 50)?;
    }
    assert_eq!(state.words()[inventory::clip(def)], 3);
    step(&mut state, &weapons, &idle, &idle, 50)?;
    assert_eq!(state.words()[inventory::clip(def)], 4);
    assert_eq!(state.words()[inventory::ammo(def)], 7);
    assert_eq!(state.words()[0xd8 / 4], 5);
    for _ in 0..2 {
        step(&mut state, &weapons, &idle, &idle, 50)?;
    }
    assert_eq!(state.words()[0xd8 / 4], 0);
    Ok(())
}

#[test]
fn mounted_step_resets_binoculars_once_and_restarts_idle_animation_each_step() {
    let mut ps = PlayerState::default();
    let words = ps.words_mut();
    words[3] = 0x20851;
    words[0xd8 / 4] = 19;
    words[0xdc / 4] = 1.0f32.to_bits();
    words[0x34 / 4] = 360;
    words[0x38 / 4] = 100;
    force_ready(&mut ps);
    let words = ps.words();
    assert_eq!(words[3], 1);
    assert_eq!(words[0xd8 / 4], 0);
    assert_eq!(words[0xdc / 4], 0);
    assert_eq!(&words[0x34 / 4..=0x38 / 4], &[0, 0]);
    assert_eq!(&words[0xa4 / 4..=0xac / 4], &[2, 0x95, 0xa9]);
    assert_eq!(words[0x5d0 / 4], 512);
    force_ready(&mut ps);
    assert_eq!(ps.words()[0xa4 / 4], 2);
    assert_eq!(ps.words()[0x5d0 / 4], 0);
}
