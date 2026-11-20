//! Cmd_CallVote_f, Cmd_Vote_f and the end-of-frame vote tally in protocol 118.
use crate::vm::value::script_error;
use crate::{
    admission::info_integer,
    config::ServerConfig,
    level::ConfigStrings,
    player::Players,
    vm::{GameHost, fiber::FiberVm, value::Value},
};
use std::io;

const END_TIME: usize = 15;
const DESCRIPTION: usize = 16;
const YES: usize = 17;
const NO: usize = 18;
const HAS_VOTED: u32 = 0x100000;
const BAD_MAP: &[u8] = b"\x15the server doesn't have that map";
const COMMAND_HELP: &[u8] = b"GAME_VOTECOMMANDSARE\x15 map_restart, map_rotate, map <mapname>, g_gametype <typename>, typemap <typename> <mapname>, kick <player>, clientkick <clientnum>, tempBanUser <player>, tempBanClient <clientNum>";

#[derive(Debug, Default)]
pub struct Vote {
    end_time: i32,
    execute_after: Option<i32>,
    yes: i32,
    no: i32,
    proposal: Vec<Vec<String>>,
    /// Cbuf_ExecuteText(EXEC_APPEND) commands processed by the next server frame.
    pub console_queue: Vec<Vec<String>>,
}

fn say(text: &[u8]) -> Vec<u8> {
    [b"e \"".as_slice(), text, b"\""].concat()
}

fn tell(players: &mut Players, slot: u16, text: &[u8]) -> io::Result<()> {
    players.get_mut(slot)?.commands.push(say(text));
    Ok(())
}

fn enabled(config: &ServerConfig, name: &str) -> bool {
    config
        .get(name)
        .is_none_or(|s| info_integer(s.as_bytes()) != 0)
}

fn argument(args: &[Vec<u8>], index: usize) -> &[u8] {
    let value = args.get(index).map_or(&[][..], Vec::as_slice);
    &value[..value.len().min(255)]
}

fn words(parts: &[&[u8]]) -> Vec<String> {
    parts
        .iter()
        .map(|part| String::from_utf8_lossy(part).into_owned())
        .collect()
}

fn description(host: &GameHost, mode: &[u8]) -> Vec<u8> {
    let name = String::from_utf8_lossy(mode).to_ascii_lowercase();
    let path = format!("maps/mp/gametypes/{name}.txt");
    if let Ok(text) = host.assets.read_script(&path)
        && (1..=1023).contains(&text.len())
    {
        let token = crate::entities::Tokens::new(&text)
            .next()
            .unwrap_or_default();
        return token[..token.len().min(63)].to_vec();
    }
    name.into_bytes()
}

type Proposal = (Vec<Vec<String>>, Vec<u8>);
fn proposal(
    host: &GameHost,
    command: &[u8],
    arg: &[u8],
    third: &[u8],
) -> Result<Proposal, &'static [u8]> {
    let map_ok = |map: &[u8]| host.assets.map_exists(&String::from_utf8_lossy(map));
    match command.to_ascii_lowercase().as_slice() {
        b"map_restart" => Ok((
            vec![words(&[b"fast_restart"])],
            b"GAME_VOTE_MAPRESTART".to_vec(),
        )),
        b"map_rotate" => Ok((vec![words(&[command])], b"GAME_VOTE_NEXTMAP".to_vec())),
        b"map" => {
            if !map_ok(arg) {
                return Err(BAD_MAP);
            }
            Ok((
                vec![words(&[command, arg])],
                [b"GAME_VOTE_MAP\x15".as_slice(), arg].concat(),
            ))
        }
        b"typemap" | b"g_gametype" => {
            if !host.config.has_gametype(&String::from_utf8_lossy(arg)) {
                return Err(b"GAME_INVALIDGAMETYPE");
            }
            let type_map = command.eq_ignore_ascii_case(b"typemap");
            if type_map && !map_ok(third) {
                return Err(BAD_MAP);
            }
            let change_type = !type_map
                || !arg
                    .eq_ignore_ascii_case(host.config.get("g_gametype").unwrap_or("").as_bytes());
            let change_map = type_map
                && !third.eq_ignore_ascii_case(host.config.map.as_deref().unwrap_or("").as_bytes());
            if !change_type && !change_map {
                return Err(b"GAME_TYPEMAP_NOCHANGE");
            }
            let mut commands = Vec::new();
            let mut display = Vec::new();
            if change_type {
                commands.push(words(&[b"g_gametype", arg]));
                display.extend(b"GAME_VOTE_GAMETYPE\x14");
                display.extend(description(host, arg));
            }
            if change_map {
                if change_type {
                    display.extend(b"\x15 - \x14");
                }
                display.extend(b"GAME_VOTE_MAP\x15");
                display.extend(third);
                commands.push(words(&[b"map", third]));
            } else {
                commands.push(words(&[b"map_restart"]));
            }
            Ok((commands, display))
        }
        b"kick" | b"clientkick" | b"tempbanuser" | b"tempbanclient" => {
            let by_name = command.eq_ignore_ascii_case(b"kick")
                || command.eq_ignore_ascii_case(b"tempbanuser");
            let target = if by_name {
                // Cmd_CallVote_f takes the last connected matching name.
                host.world
                    .players
                    .iter()
                    .filter(|(_, p)| {
                        p.connected
                            && crate::player::clean_display_name(&p.name).eq_ignore_ascii_case(arg)
                    })
                    .last()
            } else {
                let slot = info_integer(arg);
                u16::try_from(slot)
                    .ok()
                    .filter(|_| slot != 0 || arg == b"0")
                    .and_then(|slot| {
                        host.world
                            .players
                            .get(slot)
                            .filter(|p| p.connected)
                            .map(|p| (slot, p))
                    })
            };
            let (slot, player) = target.ok_or(&b"GAME_CLIENTNOTONSERVER"[..])?;
            let action = if command[0].eq_ignore_ascii_case(&b't') {
                "tempBanClient"
            } else {
                "clientkick"
            };
            Ok((
                vec![vec![action.into(), slot.to_string()]],
                [
                    format!("GAME_VOTE_KICK\x15({slot})").as_bytes(),
                    &player.name,
                ]
                .concat(),
            ))
        }
        _ => Err(b"GAME_INVALIDVOTESTRING"),
    }
}

fn allowed(command: &[u8]) -> bool {
    [
        b"map_restart".as_slice(),
        b"map_rotate",
        b"map",
        b"typemap",
        b"g_gametype",
        b"kick",
        b"clientkick",
        b"tempBanUser",
        b"tempBanClient",
    ]
    .iter()
    .any(|name| command.eq_ignore_ascii_case(name))
}

pub(crate) fn call(
    vm: &mut FiberVm,
    host: &mut GameHost,
    slot: u16,
    args: &[Vec<u8>],
) -> io::Result<()> {
    if !enabled(host.config, "g_allowVote") {
        return tell(&mut host.world.players, slot, b"GAME_VOTINGNOTENABLED");
    }
    let native = enabled(host.config, "g_oldVoting");
    let player = host.world.players.get_mut(slot)?;
    let object = player.object;
    if native && host.world.vote.end_time != 0 {
        return tell(&mut host.world.players, slot, b"GAME_VOTEALREADYINPROGRESS");
    }
    // The shipped server checks a reserved voteCount word here. No stock code increments it.
    if native && player.team() == 3 {
        return tell(&mut host.world.players, slot, b"GAME_NOSPECTATORCALLVOTE");
    }
    let [command, arg, third] = [1, 2, 3].map(|i| argument(args, i));
    if [command, arg, third].iter().any(|s| s.contains(&b';')) {
        return tell(&mut host.world.players, slot, b"GAME_INVALIDVOTESTRING");
    }
    if !native {
        vm.notify(
            object,
            "call_vote",
            &[command, arg, third].map(|v| Value::String(v.to_vec())),
        );
        return Ok(());
    }
    if !allowed(command) {
        tell(&mut host.world.players, slot, b"GAME_INVALIDVOTESTRING")?;
        return tell(&mut host.world.players, slot, COMMAND_HELP);
    }
    let vote = &mut host.world.vote;
    if vote.execute_after.take().is_some() {
        vote.console_queue.append(&mut vote.proposal);
    }
    let (proposal, display) = match proposal(host, command, arg, third) {
        Ok(result) => result,
        Err(text) => return tell(&mut host.world.players, slot, text),
    };
    let vote = &mut host.world.vote;
    vote.proposal = proposal;
    vote.end_time = host.time_ms.wrapping_add(30000);
    vote.yes = 1;
    vote.no = 0;
    let player = host.world.players.get_mut(slot)?;
    host.strings.broadcast(say(
        &[b"GAME_CALLEDAVOTE\x15".as_slice(), &player.name].concat()
    ));
    for (index, player) in host.world.players.iter_mut() {
        let flags = player.state.field("eFlags").expect("registered netfield") & !HAS_VOTED;
        player.state.set_field(
            "eFlags",
            if index == slot {
                flags | HAS_VOTED
            } else {
                flags
            },
        )?;
    }
    host.strings
        .set(END_TIME, vote.end_time.to_string().as_bytes())?;
    host.strings.set(DESCRIPTION, &display)?;
    host.strings.set(YES, b"1")?;
    host.strings.set(NO, b"0")?;
    Ok(())
}

fn is_yes(answer: &[u8]) -> bool {
    // Cmd_Vote_f examines byte one for uppercase Y and '1'.
    answer.first() == Some(&b'y') || matches!(answer.get(1), Some(b'Y' | b'1'))
}

pub(crate) fn cast(
    vm: &mut FiberVm,
    host: &mut GameHost,
    slot: u16,
    args: &[Vec<u8>],
) -> io::Result<()> {
    let yes = is_yes(argument(args, 1));
    let vote = &mut host.world.vote;
    let player = host.world.players.get_mut(slot)?;
    if !enabled(host.config, "g_oldVoting") {
        vm.notify(
            player.object,
            "vote",
            &[Value::String(if yes {
                b"yes".to_vec()
            } else {
                b"no".to_vec()
            })],
        );
        return Ok(());
    }
    let flags = player.state.field("eFlags").expect("registered netfield");
    if vote.end_time == 0 {
        return tell(&mut host.world.players, slot, b"GAME_NOVOTEINPROGRESS");
    }
    if flags & HAS_VOTED != 0 {
        return tell(&mut host.world.players, slot, b"GAME_VOTEALREADYCAST");
    }
    if player.team() == 3 {
        return tell(&mut host.world.players, slot, b"GAME_NOSPECTATORVOTE");
    }
    player.commands.push(say(b"GAME_VOTECAST"));
    player.state.set_field("eFlags", flags | HAS_VOTED)?;
    let (index, count) = if yes {
        (YES, &mut vote.yes)
    } else {
        (NO, &mut vote.no)
    };
    *count += 1;
    host.strings.set(index, count.to_string().as_bytes())?;
    Ok(())
}

impl Vote {
    fn result(&self, time: i32, voters: i32, abstain_weight: f32) -> Option<bool> {
        if self.end_time == 0 {
            return None;
        }
        if time.wrapping_sub(self.end_time) >= 0 {
            let abstainers = voters - (self.no + self.yes);
            let weighted = (f64::from(abstainers) * f64::from(abstain_weight)).ceil();
            return Some(f64::from(self.yes) > f64::from(self.no) + weighted);
        }
        let threshold = voters / 2 + 1;
        if self.yes >= threshold {
            Some(true)
        } else if self.no > voters - threshold {
            Some(false)
        } else {
            None
        }
    }

    pub fn frame(
        &mut self,
        time: i32,
        config: &ServerConfig,
        players: &Players,
        strings: &mut ConfigStrings,
    ) -> io::Result<()> {
        if !enabled(config, "g_oldVoting") {
            return Ok(());
        }
        if self.execute_after.is_some_and(|due| due < time) {
            self.execute_after = None;
            self.console_queue.append(&mut self.proposal);
        }
        let voters = players
            .iter()
            .filter(|(_, p)| p.connected && p.team() != 3)
            .count() as i32;
        let weight = config
            .get("g_voteAbstainWeight")
            .and_then(|n| n.parse().ok())
            .unwrap_or(0.5);
        if let Some(passed) = self.result(time, voters, weight) {
            if passed {
                self.execute_after = Some(time.wrapping_add(3000));
            }
            strings.broadcast(say(if passed {
                b"GAME_VOTEPASSED"
            } else {
                b"GAME_VOTEFAILED"
            }));
            self.end_time = 0;
            strings.set(END_TIME, b"")?;
        }
        Ok(())
    }
}

/// Script voting publishes a UI separately from the native tally.
pub(crate) fn builtin(
    name: &str,
    args: &[Value],
    host: &mut GameHost,
) -> Option<io::Result<Value>> {
    let arg = args.first();
    if !matches!(
        name,
        "setvotestring"
            | "setvotetime"
            | "setvoteyescount"
            | "setvotenocount"
            | "mapexists"
            | "kick"
            | "ban"
            | "clientprint"
    ) {
        return None;
    }
    Some((|| {
        let Some(arg) = arg else {
            return Ok(Value::Undefined);
        };
        let integer = || match arg {
            Value::Int(n) => Ok(*n),
            value => Err(script_error(format!(
                "type {} is not an int",
                value.type_name()
            ))),
        };
        let text = |value: &Value| {
            value
                .script_string()
                .map_err(|err| script_error(err.to_string()))
        };
        let vote = &mut host.world.vote;
        match name {
            "clientprint" => {
                let Value::Object(id) = arg else {
                    return Err(script_error("expected entity"));
                };
                let slot = host
                    .world
                    .entities
                    .slot_for_object(*id)
                    .ok_or_else(|| script_error("expected entity"))?;
                let message = text(
                    args.get(1)
                        .ok_or_else(|| script_error("parameter 2 does not exist"))?,
                )?;
                if host.world.players.get(slot).is_some() {
                    tell(&mut host.world.players, slot, &message)?;
                }
            }
            "kick" | "ban" => {
                let command = if name == "kick" {
                    "tempBanClient"
                } else {
                    "banClient"
                };
                vote.console_queue
                    .push(vec![command.to_owned(), integer()?.to_string()]);
            }
            "mapexists" => {
                return Ok(Value::Int(
                    host.assets
                        .map_exists(&String::from_utf8_lossy(&text(arg)?))
                        as i32,
                ));
            }
            _ => {
                if name == "setvotestring" {
                    host.strings.set(DESCRIPTION, &text(arg)?)?;
                }
                if matches!(name, "setvotestring" | "setvotetime") {
                    let deadline = if name == "setvotetime" {
                        integer()?
                    } else {
                        vote.end_time
                    };
                    host.strings
                        .set(END_TIME, deadline.to_string().as_bytes())?;
                }
                if name != "setvotenocount" {
                    let yes = if name == "setvoteyescount" {
                        integer()?
                    } else {
                        vote.yes
                    };
                    host.strings.set(YES, yes.to_string().as_bytes())?;
                }
                let no = if name == "setvotenocount" {
                    integer()?
                } else {
                    vote.no
                };
                host.strings.set(NO, no.to_string().as_bytes())?;
            }
        }
        Ok(Value::Undefined)
    })())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn early_majority_and_timeout_abstentions() {
        let mut vote = Vote {
            end_time: 30100,
            yes: 1,
            no: 1,
            ..Default::default()
        };
        assert_eq!(vote.result(30100, 5, 0.5), Some(false));
        assert_eq!(vote.result(30099, 5, 0.5), None);
        vote.yes = 3;
        assert_eq!(vote.result(30099, 5, 0.5), Some(true));
        vote.yes = 1;
        vote.no = 2;
        assert_eq!(vote.result(30099, 4, 0.5), Some(false));
        vote.yes = 2;
        vote.no = 1;
        assert_eq!(vote.result(30100, 4, 0.), Some(true));
        assert_eq!(vote.result(30100, 4, 1.), Some(false));
    }
    #[test]
    fn first_and_second_vote_answer_bytes() {
        for answer in [b"yes".as_slice(), b"y", b"aY", b"n1"] {
            assert!(is_yes(answer));
        }
        for answer in [b"Yes".as_slice(), b"Y", b"1", b"NO", b""] {
            assert!(!is_yes(answer));
        }
    }

    #[test]
    fn typed_map_vote_and_script_vote_share_the_client_command_entrypoint() -> io::Result<()> {
        use crate::{
            assets::{GameAssets, weapons::WeaponRegistry},
            level::ServerUpdate,
            player::{self, Connect},
            script::ScriptProgram,
            vm::fiber::Host,
        };
        use std::fs;
        let directory = tempfile::tempdir()?;
        let main = directory.path().join("main");
        for path in ["maps/mp/gametypes", "weapons/mp", "mp"] {
            fs::create_dir_all(main.join(path))?;
        }
        fs::write(main.join("maps/mp/other.d3dbsp"), b"test")?;
        fs::write(
            main.join("maps/mp/gametypes/dm.txt"),
            br#""MPUI_DEATHMATCH""#,
        )?;
        fs::write(main.join("mp/playeranimtypes.txt"), b"none")?;
        fs::write(main.join("weapons/mp/defaultweapon_mp"), b"WEAPONFILE")?;
        let assets = GameAssets::mount_paths(directory.path(), None, "")?;
        let mut config =
            ServerConfig::from_args(&["+set".into(), "g_gametype".into(), "tdm".into()]);
        config.set_gametypes(vec!["dm".into(), "tdm".into()]);
        config.map = Some("current".to_owned());
        let mut host = GameHost {
            assets: &assets,
            models: &mut Default::default(),
            weapons: &mut WeaponRegistry::load(&assets)?,
            start_time: 0,
            collision: &crate::collision::tests::map(-1, -2, &[])?,
            world: &mut Default::default(),
            config: &mut config,
            strings: &mut Default::default(),
            time_ms: 1000,
            initializing: false,
        };
        // The GSC symbols and the console commands use different names.
        for (symbol, command) in [("kick", "tempBanClient"), ("ban", "banClient")] {
            assert_eq!(
                builtin(symbol, &[], &mut host).expect("registered GSC vote builtin")?,
                Value::Undefined
            );
            assert!(
                builtin(symbol, &[Value::String(b"0; quit".to_vec())], &mut host)
                    .expect("registered GSC vote builtin")
                    .is_err()
            );
            assert!(host.world.vote.console_queue.is_empty());
            assert_eq!(
                builtin(symbol, &[Value::Int(7)], &mut host)
                    .expect("registered GSC vote builtin")?,
                Value::Undefined
            );
            assert_eq!(
                host.world.vote.console_queue.pop(),
                Some(vec![command.into(), "7".into()])
            );
        }
        let script = ScriptProgram::discover("test", "mp_test", false, |p| {
            Ok(match p {
            "maps/mp/gametypes/test.gsc"|"codescripts/delete.gsc"=> b"main(){}".to_vec(),
            "codescripts/struct.gsc"=>b"initStructs(){} createStruct(){}".to_vec(),
            "maps/mp/gametypes/_callbacksetup.gsc"=>br#"
                codecallback_startgametype(){}
                codecallback_playerconnect(){
                    self waittill("call_vote", name, a, b);
                    self.testcommand = name + ":" + a + ":" + b;
                    self waittill("vote", answer);
                    self.testanswer = answer;
                }
                codecallback_playerdisconnect(){} codecallback_playerdamage(){} codecallback_playerkilled(){}
            "#.to_vec(),
            _=>return Err(io::ErrorKind::NotFound.into())
        })
        })?;
        let mut vm = FiberVm::default();
        for (slot, name) in [b"caller".as_slice(), b"other"].into_iter().enumerate() {
            let slot = slot as u16;
            player::connect(
                &mut vm,
                &script,
                &mut host,
                Connect {
                    slot,
                    userinfo: &[br"\name\".as_slice(), name].concat(),
                    persistent: Value::Array(Default::default()),
                    local: false,
                    guid: 0,
                },
            )?;
            player::begin(&mut vm, &mut host, slot)?;
            let object = host
                .world
                .players
                .get(slot)
                .expect("connected fixture player")
                .object;
            host.set_field(object, "sessionteam", Value::String(b"allies".to_vec()))?;
        }
        let trigger = |vm: &mut FiberVm, host: &mut GameHost, slot, command: &[u8]| {
            player::client_command(
                vm,
                &script,
                host,
                slot,
                16,
                &crate::query::tokenize(command),
            )
        };
        trigger(&mut vm, &mut host, 0, b"callvote typemap dm other")?;
        assert!(matches!(&host.strings.take_changed()[..],
            [ServerUpdate::Broadcast(c), ServerUpdate::ConfigString(15,t), ServerUpdate::ConfigString(16,d), ServerUpdate::ConfigString(17,y), ServerUpdate::ConfigString(18,n)]
            if c==b"e \"GAME_CALLEDAVOTE\x15caller\"" && t==b"31000" && d==b"GAME_VOTE_GAMETYPE\x14MPUI_DEATHMATCH\x15 - \x14GAME_VOTE_MAP\x15other" && y==b"1" && n==b"0"));
        trigger(&mut vm, &mut host, 1, b"vote yes")?;
        host.world
            .vote
            .frame(1050, host.config, &host.world.players, host.strings)?;
        assert_eq!(
            host.world.players.take_commands(1),
            [b"e \"GAME_VOTECAST\"".to_vec()]
        );
        assert!(matches!(&host.strings.take_changed()[..],
            [ServerUpdate::ConfigString(17,y),ServerUpdate::Broadcast(c),ServerUpdate::ConfigString(15,t)] if y==b"2" && c==b"e \"GAME_VOTEPASSED\"" && t.is_empty()));
        host.world
            .vote
            .frame(4050, host.config, &host.world.players, host.strings)?;
        assert!(host.world.vote.console_queue.is_empty());
        host.world
            .vote
            .frame(4100, host.config, &host.world.players, host.strings)?;
        assert_eq!(
            host.world.vote.console_queue,
            [["g_gametype", "dm"], ["map", "other"]]
        );
        host.config
            .execute("set g_oldVoting 0", |_| Err(io::ErrorKind::NotFound.into()))?;
        trigger(&mut vm, &mut host, 1, b"callvote team allies axis")?;
        vm.run_current(&script, &mut host)?;
        trigger(&mut vm, &mut host, 1, b"vote Y")?;
        vm.run_current(&script, &mut host)?;
        let object = host
            .world
            .players
            .get(1)
            .expect("connected fixture player")
            .object;
        assert_eq!(
            host.field(object, "testcommand")?,
            Value::String(b"team:allies:axis".to_vec())
        );
        assert_eq!(
            host.field(object, "testanswer")?,
            Value::String(b"no".to_vec())
        );
        Ok(())
    }
}
