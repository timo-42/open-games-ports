//! Connectionless server-browser queries (SV_InfoResponse and SV_StatusResponse).
use crate::config::{self, ServerConfig};
use std::collections::BTreeMap;

const OOB: &[u8; 4] = b"\xff\xff\xff\xff";
const INFO_SIZE: usize = 1024;

// Dvars included by Dvar_InfoString(0x404), in their registered spelling.
// scr_friendlyfire and scr_killcam are exported by the stock game scripts.
const STATUS_DEFAULTS: &[(&str, &str)] = &[
    ("fs_game", ""),
    ("g_antilag", "1"),
    ("g_gametype", "dm"),
    ("gamename", "Call of Duty 2"),
    ("mapname", ""),
    ("protocol", "118"),
    ("scr_friendlyfire", "0"),
    ("scr_killcam", "1"),
    ("shortversion", "1.3"),
    ("sv_allowAnonymous", "0"),
    ("sv_floodProtect", "1"),
    ("sv_hostname", "CoD2Host"),
    ("sv_keywords", ""),
    ("sv_mapname", ""),
    ("sv_maxclients", "20"),
    ("sv_maxPing", "0"),
    ("sv_maxRate", "0"),
    ("sv_minPing", "0"),
    ("sv_privateClients", "0"),
    ("sv_punkbuster", "0"),
    ("sv_pure", "1"),
    ("sv_voice", "0"),
];

/// Clients with state greater than CS_ZOMBIE. Slots are the server's absolute indices.
#[derive(Debug, Clone)]
pub struct QueryPlayer {
    pub slot: usize,
    pub score: i32,
    pub ping: i32,
    pub name: Vec<u8>,
}

#[derive(Debug)]
struct Dvar {
    name: String,
    value: String,
    advertised: bool,
}

#[derive(Debug)]
pub struct QueryState {
    dvars: BTreeMap<String, Dvar>,
    pub players: Vec<QueryPlayer>,
    /// True when FS has an active server pure-IWD list (fs_numServerIwds).
    pub server_iwds: bool,
}

impl Default for QueryState {
    fn default() -> Self {
        let mut state = Self {
            dvars: BTreeMap::new(),
            players: Vec::new(),
            server_iwds: false,
        };
        for (key, value) in STATUS_DEFAULTS {
            state.set_serverinfo(key, value);
        }
        state
    }
}

impl QueryState {
    fn assign(&mut self, name: &str, value: &str, advertise: bool) {
        let dvar = self
            .dvars
            .entry(name.to_ascii_lowercase())
            .or_insert_with(|| Dvar {
                name: name.to_owned(),
                value: String::new(),
                advertised: false,
            });
        dvar.value.clear();
        dvar.value.push_str(value);
        dvar.advertised |= advertise;
    }

    /// Rebuild the advertised flags from the current map's registrations.
    pub fn publish_config(&mut self, config: &ServerConfig) {
        for dvar in self.dvars.values_mut() {
            dvar.advertised = false;
        }
        for (name, value) in config.dvars_with_flags(config::SERVER_INFO) {
            self.set_serverinfo(name, value);
        }
        for (name, value) in &config.values {
            self.set_dvar(name, value);
        }
    }

    pub fn set_dvar(&mut self, name: &str, value: &str) {
        self.assign(name, value, false);
    }

    pub fn set_serverinfo(&mut self, name: &str, value: &str) {
        self.assign(name, value, true);
    }

    fn value(&self, lower_name: &str) -> &str {
        self.dvars.get(lower_name).map_or("", |dvar| &dvar.value)
    }

    fn integer(&self, lower_name: &str) -> i32 {
        self.value(lower_name).parse().unwrap_or(0)
    }

    fn modded(&self) -> bool {
        self.integer("sv_pure") == 0
            || !self.value("fs_game").is_empty()
            || tokenize(self.value("sv_referencediwdnames").as_bytes())
                .iter()
                .any(|name| !is_stock_iwd(name))
    }

    /// Answer a complete UDP datagram, including its connectionless marker.
    pub fn response(&self, packet: &[u8]) -> Option<Vec<u8>> {
        let args = oob_tokens(packet)?;
        let challenge = args.get(1).map_or(&[][..], Vec::as_slice);
        let (header, info, rows, capacity) = match args.first()?.as_slice() {
            b"getinfo" => (
                b"infoResponse\n".as_slice(),
                self.info(challenge),
                Vec::new(),
                1024,
            ),
            b"getstatus" => (
                b"statusResponse\n".as_slice(),
                self.status(challenge),
                self.player_rows(),
                8192,
            ),
            _ => return None,
        };
        let mut reply = OOB.to_vec();
        reply.extend(header);
        reply.extend(info.0);
        reply.extend(rows);
        reply.truncate(OOB.len() + capacity - 1);
        Some(reply)
    }

    fn info(&self, challenge: &[u8]) -> InfoString {
        let mut info = InfoString::default();
        let max_clients = self.integer("sv_maxclients").clamp(0, 64) as usize;
        let private = self.integer("sv_privateclients").clamp(0, 64) as usize;
        let occupied = self
            .players
            .iter()
            .filter(|p| p.slot < private.max(max_clients))
            .count();
        let occupied_private = self.players.iter().filter(|p| p.slot < private).count();
        info.set("challenge", challenge);
        info.set("protocol", b"118");
        info.set("hostname", self.value("sv_hostname"));
        info.set("mapname", self.value("mapname"));
        if occupied > 0 {
            info.set("clients", occupied.to_string());
        }
        let visible_slots = (max_clients + occupied_private).saturating_sub(private);
        if visible_slots > 0 {
            info.set("sv_maxclients", visible_slots.to_string());
        }
        info.set("gametype", self.value("g_gametype"));
        if self.integer("sv_pure") != 0 || self.server_iwds {
            info.set("pure", "1");
        }
        for (name, key) in [("sv_minping", "minPing"), ("sv_maxping", "maxPing")] {
            self.nonzero(&mut info, name, key);
        }
        info.set("game", self.value("fs_game"));
        self.nonzero(&mut info, "sv_allowanonymous", "sv_allowAnonymous");
        self.nonzero(&mut info, "sv_disableclientconsole", "con_disabled");
        if !self.value("g_password").is_empty() {
            info.set("pswrd", "1");
        }
        self.nonzero(&mut info, "scr_friendlyfire", "ff");
        self.nonzero(&mut info, "scr_killcam", "kc");
        info.set("hw", "1");
        info.set("mod", if self.modded() { "1" } else { "0" });
        info.set("voice", self.integer("sv_voice").to_string());
        info.set("pb", self.integer("sv_punkbuster").to_string());
        info
    }

    fn nonzero(&self, info: &mut InfoString, dvar: &str, key: &str) {
        let value = self.integer(dvar);
        if value != 0 {
            info.set(key, value.to_string());
        }
    }

    pub fn advertised_info(&self) -> Vec<u8> {
        let mut info = InfoString::default();
        for dvar in self.dvars.values().filter(|dvar| dvar.advertised) {
            info.set(&dvar.name, &dvar.value);
        }
        info.0
    }

    /// Systeminfo uses DVAR_SYSTEMINFO (8), separately from the browser's
    /// advertised settings. Dvar_InfoString_Big also visits keys in name order.
    pub fn system_info(&self, server_id: u8, iwds: &[(&str, String)]) -> Vec<u8> {
        let mut values = BTreeMap::new();
        for (name, default) in ServerConfig::system_defaults() {
            let value = self
                .dvars
                .get(&name.to_ascii_lowercase())
                .map_or(default, |dvar| dvar.value.as_str());
            values.insert(name.to_ascii_lowercase(), (name, value));
        }
        let id = server_id.to_string();
        values.insert("sv_serverid".into(), ("sv_serverid", id.as_str()));
        for (name, value) in iwds {
            values.insert(name.to_ascii_lowercase(), (*name, value.as_str()));
        }
        let mut info = Vec::new();
        for (name, value) in values.into_values() {
            // Info_SetValueForKey_Big sanitizes up to 8191 bytes, then rejects
            // the entire pair if the resulting string would exceed 1024 bytes.
            let clean: Vec<_> = value
                .bytes()
                .take(8191)
                .take_while(|b| *b != 0)
                .filter(|b| !matches!(b, b'\\' | b';' | b'"'))
                .collect();
            if !clean.is_empty() && info.len() + name.len() + clean.len() + 2 <= INFO_SIZE {
                info.push(b'\\');
                info.extend(name.as_bytes());
                info.push(b'\\');
                info.extend(clean);
            }
        }
        info
    }

    fn status(&self, challenge: &[u8]) -> InfoString {
        let mut info = self.completion_info(challenge);
        info.set(
            "pswrd",
            if self.value("g_password").is_empty() {
                "0"
            } else {
                "1"
            },
        );
        info.set("mod", if self.modded() { "1" } else { "0" });
        info
    }

    fn completion_info(&self, challenge: &[u8]) -> InfoString {
        let mut info = InfoString(self.advertised_info());
        info.set("challenge", challenge);
        if self.integer("fs_restrict") != 0 {
            let mut keywords = b"demo ".to_vec();
            if let Some(value) = info.value(b"sv_keywords") {
                keywords.extend(value);
            }
            info.set("sv_keywords", keywords);
        }
        info
    }

    pub fn master_status(&self, game_complete: bool, challenge: &[u8]) -> Vec<u8> {
        let (header, info, capacity) = if game_complete {
            (
                b"gameCompleteStatus\n".as_slice(),
                self.completion_info(challenge),
                0x1ffff,
            )
        } else {
            (b"statusResponse\n".as_slice(), self.status(challenge), 8191)
        };
        let mut packet = OOB.to_vec();
        packet.extend(header);
        packet.extend(info.0);
        packet.extend(self.player_rows());
        packet.truncate(4 + capacity);
        packet
    }

    fn player_rows(&self) -> Vec<u8> {
        let mut rows = vec![b'\n'];
        let max_clients = self.integer("sv_maxclients").clamp(0, 64) as usize;
        let mut players: Vec<_> = self
            .players
            .iter()
            .filter(|p| p.slot < max_clients)
            .collect();
        players.sort_unstable_by_key(|p| p.slot);
        for p in players {
            let mut line = format!("{} {} \"", p.score, p.ping).into_bytes();
            line.extend(&p.name);
            line.extend(b"\"\n");
            line.truncate(1023);
            rows.extend(line);
        }
        rows
    }
}

// FS_iwIwd compares bare IWD names as reported by sv_referencedIwdNames.
fn is_stock_iwd(name: &[u8]) -> bool {
    let path: Vec<u8> = name
        .iter()
        .map(|b| match b {
            b'\\' | b':' => b'/',
            b => b.to_ascii_lowercase(),
        })
        .collect();
    fn stock_index(digits: &[u8]) -> bool {
        digits.len() == 2 && digits.iter().all(u8::is_ascii_digit) && digits <= b"24".as_slice()
    }
    if let Some(index) = path.strip_prefix(b"main/iw_") {
        return stock_index(index);
    }
    // The original's initial strstr("localized_") is case sensitive.
    if name.get(5..15) != Some(b"localized_") {
        return false;
    }
    path.strip_prefix(b"main/localized_").is_some_and(|rest| {
        rest.windows(5)
            .any(|s| s.starts_with(b"_iw") && stock_index(&s[3..]))
    })
}

#[derive(Default)]
pub(crate) struct InfoString(pub(crate) Vec<u8>);

impl InfoString {
    fn value(&self, key: &[u8]) -> Option<&[u8]> {
        let mut fields = self.0.split(|b| *b == b'\\').skip(1);
        while let (Some(name), Some(value)) = (fields.next(), fields.next()) {
            if name.eq_ignore_ascii_case(key) {
                return Some(value);
            }
        }
        None
    }

    // Info_SetValueForKey: remove any exact match, strip the three wire
    // delimiters, append at the tail only if the bounded pair fits.
    pub(crate) fn set(&mut self, key: &str, value: impl AsRef<[u8]>) {
        let reserved = |b: &u8| matches!(b, b'\\' | b';' | b'"');
        if self.0.len() >= INFO_SIZE || key.as_bytes().iter().any(reserved) {
            return;
        }

        let mut fields = self.0.split(|b| *b == b'\\').skip(1);
        let mut offset = 0;
        while let (Some(name), Some(old)) = (fields.next(), fields.next()) {
            let end = offset + 2 + name.len() + old.len();
            if name == key.as_bytes() {
                self.0.drain(offset..end);
                break;
            }
            offset = end;
        }
        let clean: Vec<u8> = value
            .as_ref()
            .iter()
            .take(INFO_SIZE - 1)
            .take_while(|b| **b != 0)
            .filter(|b| !reserved(b))
            .copied()
            .collect();
        if clean.is_empty() {
            return;
        }
        let mut pair = vec![b'\\'];
        pair.extend(key.as_bytes());
        pair.push(b'\\');
        pair.extend(clean);
        pair.truncate(INFO_SIZE - 1);
        if self.0.len() + pair.len() <= INFO_SIZE {
            self.0.extend(pair);
        }
    }
}

/// MSG_ReadStringLine normalizes the first line before command tokenization.
pub fn oob_tokens(packet: &[u8]) -> Option<Vec<Vec<u8>>> {
    let line: Vec<_> = packet
        .strip_prefix(OOB)?
        .iter()
        .take(1023)
        .take_while(|b| !matches!(b, 0 | b'\n'))
        .map(|b| match b {
            b'%' => b'.',
            0x92 => b'\'',
            b => *b,
        })
        .collect();
    Some(tokenize(&line))
}

/// Cmd_TokenizeStringInternal with an unlimited final argument length.
pub fn tokenize(text: &[u8]) -> Vec<Vec<u8>> {
    let mut rest = text.split(|b| *b == 0).next().unwrap_or_default();
    let mut args = Vec::new();
    let space = |b: &u8| (*b as i8) < 33;
    loop {
        if args.len() == 512 {
            return Vec::new();
        }
        while rest.first().is_some_and(space) {
            rest = &rest[1..];
        }
        if rest.is_empty() || rest.starts_with(b"//") {
            break;
        }
        if rest.starts_with(b"/*") {
            let Some(end) = rest.windows(2).position(|s| s == b"*/") else {
                break;
            };
            rest = &rest[end + 2..];
            continue;
        }
        let mut token = Vec::new();
        if rest[0] == b'"' {
            rest = &rest[1..];
            while let Some((&byte, tail)) = rest.split_first() {
                rest = tail;
                if byte == b'"' {
                    break;
                }
                if byte == b'\\' && rest.first() == Some(&b'"') {
                    token.push(b'"');
                    rest = &rest[1..];
                } else {
                    token.push(byte);
                }
            }
        } else {
            while let Some((&byte, tail)) = rest.split_first() {
                if space(&byte)
                    || byte == b'"'
                    || rest.starts_with(b"//")
                    || rest.starts_with(b"/*")
                {
                    break;
                }
                token.push(byte);
                rest = tail;
            }
        }
        args.push(token);
        if rest.is_empty() {
            break;
        }
        if rest.first().is_some_and(space) {
            rest = &rest[1..];
        }
    }
    args
}

#[cfg(test)]
mod tests {
    use super::*;

    fn packet(line: &[u8]) -> Vec<u8> {
        [OOB.as_slice(), line].concat()
    }

    #[test]
    fn baseline_empty_server_wire_replies() {
        let mut server = QueryState::default();
        server.set_dvar("SV_MAXCLIENTS", "8");
        server.set_dvar("mapname", "mp_toujane");
        assert_eq!(server.response(&packet(b"getinfo abc123")),
            Some(packet(br"infoResponse
\challenge\abc123\protocol\118\hostname\CoD2Host\mapname\mp_toujane\sv_maxclients\8\gametype\dm\pure\1\kc\1\hw\1\mod\0\voice\0\pb\0")));
        assert_eq!(server.response(&packet(b"getstatus abc123")),
            Some(packet(br"statusResponse
\g_antilag\1\g_gametype\dm\gamename\Call of Duty 2\mapname\mp_toujane\protocol\118\scr_friendlyfire\0\scr_killcam\1\shortversion\1.3\sv_allowAnonymous\0\sv_floodProtect\1\sv_hostname\CoD2Host\sv_maxclients\8\sv_maxPing\0\sv_maxRate\0\sv_minPing\0\sv_privateClients\0\sv_punkbuster\0\sv_pure\1\sv_voice\0\challenge\abc123\pswrd\0\mod\0
")));
    }

    #[test]
    fn private_slots_and_sorted_scores() {
        let mut server = QueryState::default();
        server.set_dvar("sv_maxclients", "5");
        server.set_dvar("sv_privateClients", "2");
        server.players = vec![
            QueryPlayer {
                slot: 3,
                score: -1,
                ping: 72,
                name: b"Bl\xfce".to_vec(),
            },
            QueryPlayer {
                slot: 0,
                score: 8,
                ping: 15,
                name: b"^1Red".to_vec(),
            },
        ];
        let info = server.info(b"x");
        assert_eq!(info.value(b"clients"), Some(b"2".as_slice()));
        assert_eq!(info.value(b"sv_maxclients"), Some(b"4".as_slice()));
        assert_eq!(
            server.player_rows(),
            b"\n8 15 \"^1Red\"\n-1 72 \"Bl\xfce\"\n"
        );
    }

    #[test]
    fn oob_command_and_challenge() {
        let state = QueryState::default();
        assert_eq!(state.response(b"getinfo 1"), None);
        assert_eq!(state.response(&packet(b"GetInfo 1")), None);
        assert_eq!(state.response(&packet(b"nothing\ngetinfo")), None);
        assert_eq!(
            oob_tokens(&packet(b"getinfo/*x*/ \"a\x92b%c\\\"d\"\x00t")),
            Some(vec![b"getinfo".to_vec(), b"a'b.c\"d".to_vec()])
        );
        assert_eq!(tokenize(b"a\x80b//c"), vec![b"a".to_vec(), b"b".to_vec()]);
        assert_eq!(tokenize(&b"x ".repeat(512)), Vec::<Vec<u8>>::new());
        let reply = state
            .response(&packet(b"getinfo \"x\\;\\\"y\""))
            .expect("reply");
        assert!(reply.starts_with(&packet(
            br"infoResponse
\challenge\xy\protocol\118"
        )));
    }

    #[test]
    fn flags_and_exported_settings() {
        let mut state = QueryState::default();
        state.set_dvar("g_password", "secret");
        state.set_dvar("sv_keywords", "competitive");
        state.set_dvar("fs_restrict", "1");
        state.set_serverinfo("HostRegion", "EU");
        state.set_dvar("HOSTREGION", "US");
        state.set_dvar(
            "sv_referencedIwdNames",
            "main/iw_15 MAIN:localized_german_IW03",
        );
        assert!(!state.modded());
        state.set_dvar("sv_referencedIwdNames", "main/iw_15 main/custom");
        let info = state.status(b"cookie");
        for (key, expected) in [
            ("pswrd", "1"),
            ("mod", "1"),
            ("sv_keywords", "demo competitive"),
            ("HostRegion", "US"),
        ] {
            assert_eq!(info.value(key.as_bytes()), Some(expected.as_bytes()));
        }
        assert_eq!(info.value(b"g_password"), None);
        state.set_dvar("sv_pure", "0");
        assert_eq!(state.info(b"").value(b"pure"), None);
        state.server_iwds = true;
        assert_eq!(state.info(b"").value(b"pure"), Some(b"1".as_slice()));
    }

    #[test]
    fn bounded_info_pairs_and_replacement() {
        let mut info = InfoString::default();
        info.set("x", "before");
        info.set("a", "1");
        info.set("x", r#"s;t\r"ip"#);
        assert_eq!(info.0, br"\a\1\x\strip");
        info.set("a", "");
        info.set("bad;key", "x");
        assert_eq!(info.0, br"\x\strip");
        info.set("tooBig", "a".repeat(2000));
        assert_eq!(info.0, br"\x\strip");
        info.set("x", "b".repeat(2000));
        assert_eq!(info.0.len(), 1023);
        assert_eq!(info.value(b"x").map(<[u8]>::len), Some(1020));
    }
}
