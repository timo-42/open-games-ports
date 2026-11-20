use super::*;
use crate::{
    assets::{GameAssets, weapons::WeaponRegistry},
    collision,
    config::ServerConfig,
    player::{self, Connect},
};
use std::fs;
fn weapon_assets(definition: &[u8]) -> io::Result<(tempfile::TempDir, GameAssets)> {
    let dir = tempfile::tempdir()?;
    let main = dir.path().join("main");
    fs::create_dir_all(main.join("weapons/mp"))?;
    fs::create_dir_all(main.join("mp"))?;
    fs::write(main.join("mp/playeranimtypes.txt"), b"none")?;
    fs::write(main.join("weapons/mp/defaultweapon_mp"), definition)?;
    let assets = GameAssets::mount_paths(dir.path(), None, "")?;
    Ok((dir, assets))
}

#[test]
fn nested_damage_and_killed_callbacks_control_health_and_scoring() -> io::Result<()> {
    let (_source, assets) = weapon_assets(br"WEAPONFILE\locHead\2\rifleBullet\1")?;
    let mut host = GameHost {
        assets: &assets,
        models: &mut Default::default(),
        weapons: &mut WeaponRegistry::load(&assets)?,
        start_time: 0,
        collision: &collision::tests::map(-1, -2, &[])?,
        world: &mut Default::default(),
        config: &mut ServerConfig::from_args(&[]),
        strings: &mut Default::default(),
        time_ms: 200,
        initializing: false,
    };
    let program = ScriptProgram::discover("test", "mp_test", false, |path| {
        Ok(match path {
            "codescripts/delete.gsc" => b"main() {}".to_vec(),
            "codescripts/struct.gsc" => b"initStructs(){} createStruct(){}".to_vec(),
            "maps/mp/gametypes/test.gsc" => b"main(){}".to_vec(),
            "maps/mp/gametypes/_callbacksetup.gsc" => br#"
            codecallback_startgametype() {}
            codecallback_playerconnect() {}
            codecallback_playerdisconnect() {}
            codecallback_playerdamage(i,a,n,flags,mod,weapon,point,dir,loc,offset) {
                self finishPlayerDamage(i,a,n,flags,mod,weapon,point,dir,loc,offset);
            }
            codecallback_playerkilled(i,a,n,mod,weapon,dir,loc,offset,duration) {
                self.sessionstate = "dead";
                self.deaths++;
                a.score++;
                obituary(self,a,weapon,mod);
            }
        "#
            .to_vec(),
            _ => return Err(io::ErrorKind::NotFound.into()),
        })
    })?;
    let mut vm = FiberVm::default();
    for slot in 0..2 {
        player::connect(
            &mut vm,
            &program,
            &mut host,
            Connect {
                slot,
                persistent: Value::default(),
                userinfo: br"\name\P",
                guid: 0,
                local: false,
            },
        )?;
        player::begin(&mut vm, &mut host, slot)?;
        let player = host.world.players.get_mut(slot)?;
        player.session_state = 0;
        player.max_health = 100;
        player.state.set_field("pm_type", 0)?;
        let object = player.object;
        host.world.set_field(object, "health", Value::Int(100))?;
        let entity = host.world.entities.get_mut(slot).expect("connected");
        entity.model_index = 1;
        entity.state.set_field("weapon", 1);
        entity.take_damage = true;
    }
    let mut hit = Hit {
        victim: 1,
        attacker: Some(0),
        inflictor: Some(0),
        damage: 12,
        flags: 0,
        means: 2,
        weapon: 1,
        point: Some([100.4, 0., 0.]),
        direction: Some([1., 0., 0.]),
        location: 2,
        offset: 50,
    };
    hit.clone().damage(&mut vm, &program, &mut host)?;
    let player = host.world.players.get_mut(1)?;
    assert_eq!(player.state.words()[0x12c / 4], 76);
    assert_eq!(player.state.field("damageTimer"), Some(750));
    assert_eq!(player.state.field("pm_time"), Some(50));
    assert_eq!(float(&player.state, "velocity[0]"), 28.);
    apply_feedback(player, 200)?;
    assert_eq!(player.state.field("damageCount"), Some(24));
    let impacts = host
        .world
        .entities
        .iter()
        .filter(|(slot, _)| *slot >= 72)
        .map(|(_, ent)| (ent.state.field("eType"), ent.client_mask))
        .collect::<Vec<_>>();
    assert_eq!(impacts, [(Some(193), [2, 0]), (Some(196), [!2, u32::MAX])]);
    hit.damage = 100;
    hit.damage(&mut vm, &program, &mut host)?;
    let player = host.world.players.get_mut(1)?;
    assert_eq!(
        (
            player.deaths,
            player.session_state,
            player.state.words()[0x12c / 4]
        ),
        (1, 1, 0)
    );
    assert_eq!(host.world.players.get_mut(0)?.score, 1);
    let event = &host.world.entities.iter().last().expect("obituary").1.state;
    assert_eq!(event.field("eType"), Some(208));
    assert_eq!(event.field("otherEntityNum"), Some(1));
    assert_eq!(event.field("attackerEntityNum"), Some(0));
    assert_eq!(event.field("eventParm"), Some(1));
    Ok(())
}

#[test]
fn hit_table_uses_engine_location_order_and_defaults() -> io::Result<()> {
    let locations = parse_locations(br"LOCDMGTABLE\left_arm_upper\2.5\right_arm_lower\3.5")?;
    assert_eq!(locations[7], 2.5);
    assert_eq!(locations[8], 3.5);
    assert_eq!(locations[0], 1.);
    assert_eq!(locations[18], 0.);
    assert!(parse_locations(br"BADTABLE\head\2").is_err());
    Ok(())
}

#[test]
fn recoil_encodes_direction_and_strength_at_end_of_frame() -> io::Result<()> {
    for amount in [1, 75, 125] {
        for (direction, yaw, pitch) in [
            (None, 255, 255),
            (Some([0., 0., 0.]), 0, 64),
            (Some([1., 0., 0.]), 0, 0),
            (Some([0., -1., 0.]), 192, 0),
            (Some([0., 0., 1.]), 0, 192),
            (Some([-0.25, 0.25, -0.707]), 96, 45),
        ] {
            let mut ps = PlayerState::default();
            ps.set_field("aimSpreadScale", 180f32.to_bits())?;
            ps.set_field("damageEvent", 3)?;
            let mut feedback = DamageFeedback { amount, direction };
            let mut motion = view::ViewMotion::default();
            feedback.apply(&mut ps, 100, &mut motion, 1000)?;
            assert_eq!(ps.field("damageEvent"), Some(4));
            assert_eq!(ps.field("damageYaw"), Some(yaw));
            assert_eq!(ps.field("damagePitch"), Some(pitch));
            assert_eq!(ps.field("damageCount"), Some(amount as u32));
            assert_eq!(
                float(&ps, "aimSpreadScale"),
                (180. + amount as f32).min(255.)
            );
            assert_eq!(motion.damage_time, 980);
            assert_eq!(feedback.amount, 0);
        }
    }
    Ok(())
}

#[test]
fn firing_angles_and_spring_follow_the_command_sequence() -> io::Result<()> {
    let (_source,assets)=weapon_assets(concat!(
        r"WEAPONFILE\aimDownSight\1\adsAimPitch\4\adsBobFactor\0.75\adsViewBobMult\0.1",
        r"\hipIdleAmount\5\adsIdleAmount\9\hipIdleSpeed\1\adsIdleSpeed\1.25",
        r"\idleCrouchFactor\0.7\idleProneFactor\0.2\gunMaxPitch\3\gunMaxYaw\4",
        r"\hipGunKickAccel\500\hipGunKickSpeedMax\250\hipGunKickSpeedDecay\5\hipGunKickStaticDecay\120",
        r"\adsGunKickAccel\500\adsGunKickSpeedMax\250\adsGunKickSpeedDecay\5\adsGunKickStaticDecay\120"
    ).as_bytes())?;
    let weapons = WeaponRegistry::load(&assets)?;
    let config = ServerConfig::from_args(&[]);
    let mut state = PlayerState::default();
    let mut motion = view::ViewMotion::default();
    let mut kick = [0.; 3];
    // The 1.3 dedicated executable evaluated the same synthetic definition and commands.
    let expected = [
        [12.5, 71.25, 0., 0.],
        [14.653216, 66.349915, 0., 0.],
        [16.281216, 60.145546, 17.259186, -12.457313],
        [13.040607, 55.996555, 4.645382, 0.],
        [14.949882, 49.71158, 0., 0.],
    ];
    for (i, (blend, ms, flags, speed)) in [
        (0f32, 50, 0, 0f32),
        (0.25, 33, 0, 170.),
        (1., 8, 4, 100.),
        (0.4, 120, 8, 120.),
        (0.8, 50, 0, 170.),
    ]
    .into_iter()
    .enumerate()
    {
        state.set_field("commandTime", 1000)?;
        state.set_field("weapon", 1)?;
        state.set_field("bobCycle", 187)?;
        state.set_field("eFlags", flags)?;
        state.set_field("speed", 190)?;
        state.set_field("velocity[0]", speed.to_bits())?;
        state.set_field("fWeaponPosFrac", blend.to_bits())?;
        state.set_field("holdBreathScale", 1.2f32.to_bits())?;
        state.set_field("leanf", 0.25f32.to_bits())?;
        let height = match flags {
            8 => 11,
            4 => 40,
            _ => 60,
        };
        state.set_field("viewHeightTarget", height)?;
        state.set_field("viewHeightCurrent", (height as f32).to_bits())?;
        let angles = [12.5 + i as f32, 71.25 - 5. * i as f32, -4.5 + i as f32];
        for (key, a) in ["viewangles[0]", "viewangles[1]", "viewangles[2]"]
            .into_iter()
            .zip(angles)
        {
            state.set_field(key, a.to_bits())?;
        }
        if i == 2 {
            motion.damage_time = 980;
            motion.damage_angles = [-5., 2.];
            kick = [23., -18., 0.];
        }
        motion.advance(&state, &mut kick, &weapons, &config, 1000 + ms, 1000 + ms);
        let output = [motion.gun_angles[0], motion.gun_angles[1], kick[0], kick[1]];
        for (a, b) in output.into_iter().zip(expected[i]) {
            assert!((a - b).abs() < 0.00001, "command {i}: {a} vs {b}");
        }
    }
    Ok(())
}
