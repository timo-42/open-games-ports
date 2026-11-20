//! Dedicated-server sound alias banks (Com_LoadSoundAliases, bank 2).
use super::{GameAssets, invalid};
use crate::entities::Tokens;
use crate::vm::spawn_fields::{float_prefix, parse_int};
use std::collections::{BTreeMap, HashMap};
use std::io;
use std::sync::Arc;

const CHANNELS: [&str; 11] = [
    "auto",
    "auto2d",
    "menu",
    "weapon",
    "voice",
    "item",
    "body",
    "local",
    "music",
    "announcer",
    "shellshock",
];

/// Per-map metadata used by script and entity sound lookup.
#[derive(Debug, Default)]
pub struct SoundAliases {
    pub volume_groups: BTreeMap<String, f32>,
    /// Q_stricmp's ASCII-uppercase sort order, then sequence order.
    pub aliases: BTreeMap<String, Vec<SoundAlias>>,
}

impl SoundAliases {
    pub fn find(&self, name: &str) -> Option<&[SoundAlias]> {
        self.aliases
            .get(&name.to_ascii_uppercase())
            .map(Vec::as_slice)
    }

    pub fn alias_count(&self) -> usize {
        self.aliases.values().map(Vec::len).sum()
    }
}

#[derive(Debug)]
pub struct SoundAlias {
    pub name: String,
    /// CSV sequence determines variant ordering during bank construction.
    pub sequence: i32,
    pub file: Arc<str>,
    pub subtitle: String,
    pub secondary: String,
    pub volume: [f32; 2],
    pub pitch: [f32; 2],
    pub distance: [f32; 2],
    pub probability: f32,
    pub slave_volume: f32,
    pub lfe: f32,
    pub delay_ms: i32,
    /// Five flag bits, two load-type bits, four channel bits (Com_AddSoundAlias).
    pub flags: u16,
    /// The dedicated server resolves named falloff curves to null.
    pub default_falloff: bool,
}

impl Default for SoundAlias {
    fn default() -> Self {
        Self {
            name: String::new(),
            sequence: 0,
            file: Arc::from(""),
            subtitle: String::new(),
            secondary: String::new(),
            volume: [1.; 2],
            pitch: [1.; 2],
            distance: [120., 0.],
            probability: 1.,
            slave_volume: 1.,
            lfe: 0.,
            delay_ms: 0,
            flags: 1 << 5,
            default_falloff: true,
        }
    }
}

impl GameAssets {
    pub fn load_sound_aliases(&self, map: &str) -> io::Result<SoundAliases> {
        let paths = self.direct_files("soundaliases", ".csv")?;
        if paths.is_empty() {
            return Ok(SoundAliases::default());
        }
        let groups = volume_groups(&self.read_file("soundaliases/volumemodgroups.def")?)?;
        let mut records = Vec::new();
        let map = map.to_ascii_lowercase();
        for path in paths {
            read_alias_file(&self.read_file(&path)?, &path, &map, &groups, &mut records)?;
        }

        // SndAlias_MergeSortList removes same-file pairs that repeat a name
        // and sequence. Between files the later case-folded filename wins.
        records.sort_by_cached_key(|r| {
            (
                r.alias.name.to_ascii_uppercase(),
                r.alias.sequence,
                r.source.clone(),
            )
        });
        let mut aliases = BTreeMap::<String, Vec<SoundAlias>>::new();
        let mut records = records.into_iter().peekable();
        let mut sound_files = HashMap::<(String, u16), Arc<str>>::new();
        while let Some(first) = records.next() {
            let key = first.alias.name.to_ascii_uppercase();
            let sequence = first.alias.sequence;
            let mut last_source = first.source;
            let mut chosen = Some(first.alias);
            while records.peek().is_some_and(|next| {
                next.alias.name.eq_ignore_ascii_case(&key) && next.alias.sequence == sequence
            }) {
                let next = records.next().expect("peeked alias");
                if chosen.is_some() && last_source == next.source {
                    chosen = None;
                } else {
                    chosen = Some(next.alias);
                }
                last_source = next.source;
            }
            if let Some(mut alias) = chosen {
                let file_key = (alias.file.to_ascii_uppercase(), (alias.flags >> 5) & 3);
                alias.file = Arc::clone(sound_files.entry(file_key).or_insert(alias.file));
                aliases.entry(key).or_default().push(alias);
            }
        }
        Ok(SoundAliases {
            volume_groups: groups,
            aliases,
        })
    }
}

fn number(text: &[u8]) -> f32 {
    float_prefix(text)
        .and_then(|(prefix, _)| prefix.parse::<f64>().ok())
        .unwrap_or(0.) as f32
}

fn volume_groups(bytes: &[u8]) -> io::Result<BTreeMap<String, f32>> {
    let body = bytes
        .strip_prefix(b"VOLUMEMODGROUPS")
        .ok_or_else(|| invalid("invalid volumemodgroups header"))?;
    if body.len() >= 8192 {
        return Err(invalid("volumemodgroups too long"));
    }
    let mut groups = BTreeMap::new();
    let mut tokens = Tokens::new(body);
    let mut count = 0;
    while let Some(name) = tokens.next() {
        if name.starts_with(b"}") {
            break;
        }
        let value = tokens
            .next()
            .filter(|v| !v.starts_with(b"}"))
            .ok_or_else(|| invalid("volume group missing scale"))?;
        if count == 32 || name.len() >= 64 {
            return Err(invalid("invalid volume group count or name"));
        }
        let name = String::from_utf8_lossy(&name).to_ascii_lowercase();
        groups.entry(name).or_insert_with(|| number(&value));
        count += 1;
    }
    Ok(groups)
}

/// Com_ParseCSV permits quotes anywhere in a cell, discards unquoted CR and
/// consumes the entire field after its 1023-byte token buffer fills. Those
/// game-specific rules differ from csv-core's configurable record delimiters.
struct Csv<'a> {
    remaining: &'a [u8],
}

impl Csv<'_> {
    fn row(&mut self) -> io::Result<Option<Vec<String>>> {
        while self
            .remaining
            .first()
            .is_some_and(|byte| matches!(byte, b'\n' | b'\r'))
        {
            self.remaining = &self.remaining[1..];
        }
        if self.remaining.is_empty() {
            return Ok(None);
        }
        let mut row = Vec::new();
        loop {
            let cell = crate::csv::field(&mut self.remaining)?;
            if row.len() < 256 {
                row.push(String::from_utf8_lossy(&cell).into_owned());
            }
            let Some((&delimiter, rest)) = self.remaining.split_first() else {
                break;
            };
            self.remaining = rest;
            if delimiter == b'\n' {
                break;
            }
        }
        Ok(Some(row))
    }
}

struct SourceAlias {
    source: String,
    alias: SoundAlias,
}

fn read_alias_file(
    bytes: &[u8],
    path: &str,
    map: &str,
    groups: &BTreeMap<String, f32>,
    out: &mut Vec<SourceAlias>,
) -> io::Result<()> {
    let mut csv = Csv {
        remaining: bytes.split(|&b| b == 0).next().unwrap_or_default(),
    };
    let mut headers = Vec::<String>::new();
    while let Some(row) = csv.row()? {
        if row
            .first()
            .is_none_or(|s| s.is_empty() || s.starts_with('#'))
        {
            continue;
        }
        if headers.is_empty() {
            headers = row.into_iter().map(|s| s.to_ascii_lowercase()).collect();
            if !["name", "file"]
                .iter()
                .all(|&key| headers.iter().any(|h| h == key))
            {
                break;
            }
            continue;
        }
        // A missing name or file ends this CSV but leaves earlier rows loaded.
        if !["name", "file"].iter().all(|&key| {
            headers
                .iter()
                .zip(&row)
                .any(|(h, v)| h == key && !v.is_empty())
        }) {
            break;
        }
        if let Some(alias) = parse_alias(&headers, &row, map, groups)? {
            out.push(SourceAlias {
                source: path.to_owned(),
                alias,
            });
        }
    }
    Ok(())
}

fn name_valid(name: &str) -> bool {
    name.len() < 63 && name.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_')
}

fn loadspec_matches(map: &str, spec: &str) -> bool {
    let spec = spec.to_ascii_lowercase();
    let positive = !spec.starts_with('!') || map == "menu";
    let bytes = spec.as_bytes();
    let mut from = 0;
    if !map.is_empty() {
        for (start, window) in bytes.windows(map.len()).enumerate() {
            if window != map.as_bytes() {
                continue;
            }
            let end = start + map.len();
            if (start == 0 || bytes[start - 1] < b'"') && bytes.get(end).is_none_or(|&b| b < b'!') {
                return positive;
            }
            from = start + 1;
        }
    }
    if map == "menu" || &bytes[from..] != b"all_mp" {
        !positive
    } else {
        positive
    }
}

fn parse_alias(
    headers: &[String],
    row: &[String],
    map: &str,
    groups: &BTreeMap<String, f32>,
) -> io::Result<Option<SoundAlias>> {
    let mut alias = SoundAlias::default();
    let mut volume_scale = 1.;
    let mut load = map != "menu";
    let mut max_volume_set = false;
    let mut max_pitch_set = false;
    let mut seen = std::collections::BTreeSet::new();
    for (column, value) in headers.iter().zip(row).filter(|(_, v)| !v.is_empty()) {
        let numeric = || number(value.as_bytes());
        let integer = || parse_int(value.as_bytes());
        match column.as_str() {
            "name" if name_valid(value) => alias.name.clone_from(value),
            "file" if value.len() < 63 => alias.file = Arc::from(value.as_str()),
            "secondaryaliasname" if name_valid(value) => alias.secondary.clone_from(value),
            "subtitle" if value.is_ascii() => alias.subtitle.clone_from(value),
            "name" | "file" | "secondaryaliasname" | "subtitle" => return Ok(None),
            "sequence" => alias.sequence = integer(),
            "vol_min" => {
                alias.volume[0] = numeric();
                if !max_volume_set {
                    alias.volume[1] = alias.volume[0];
                }
            }
            "vol_max" => {
                alias.volume[1] = numeric();
                max_volume_set = true;
            }
            "vol_mod" => {
                volume_scale = *groups
                    .get(&value.to_ascii_lowercase())
                    .ok_or_else(|| invalid(format!("unknown sound volume group {value}")))?
            }
            "pitch_min" => {
                alias.pitch[0] = numeric();
                if !max_pitch_set {
                    alias.pitch[1] = alias.pitch[0];
                }
            }
            "pitch_max" => {
                alias.pitch[1] = numeric();
                max_pitch_set = true;
            }
            "dist_min" => alias.distance[0] = numeric(),
            "dist_max" => alias.distance[1] = numeric(),
            "channel" => {
                let Some(channel) = CHANNELS.iter().position(|&v| v.eq_ignore_ascii_case(value))
                else {
                    return Ok(None);
                };
                alias.flags |= (channel as u16) << 7;
            }
            "type" => match value.to_ascii_lowercase().as_str() {
                "loaded" => (),
                "primed" | "streamed" => {
                    alias.flags = (alias.flags & !(3 << 5)) | (2 << 5);
                }
                _ => return Ok(None),
            },
            "loop" => match value.to_ascii_lowercase().as_str() {
                "looping" => alias.flags |= 1,
                "nonlooping" => (),
                _ => return Ok(None),
            },
            "probability" => alias.probability = numeric(),
            "loadspec" => load = loadspec_matches(map, value),
            "masterslave" if value.eq_ignore_ascii_case("master") => alias.flags |= 2,
            "masterslave" => {
                alias.flags |= 4;
                alias.slave_volume = numeric();
            }
            "volumefalloffcurve" => alias.default_falloff = false,
            "startdelay" => alias.delay_ms = integer(),
            "reverb" => {
                if value.contains("fulldrylevel") {
                    alias.flags |= 8;
                }
                if value.contains("nowetlevel") {
                    alias.flags |= 16;
                }
            }
            "lfe percentage" => alias.lfe = numeric().clamp(0., 1.),
            "speakermap" => (),
            _ => continue,
        }
        if !seen.insert(column) {
            return Ok(None);
        }
    }
    if !load
        || !alias
            .volume
            .iter()
            .chain([&alias.slave_volume])
            .all(|v| (0.0..=1.0).contains(v))
    {
        return Ok(None);
    }
    if alias.volume[1] < alias.volume[0] {
        alias.volume.swap(0, 1);
    }
    if alias.pitch[1] < alias.pitch[0] {
        alias.pitch.swap(0, 1);
    }
    if alias.distance[1] == 0. {
        alias.distance[1] = alias.distance[0] * 5.;
    }
    if alias.pitch[0] <= 0. || alias.distance[0] <= 0. || alias.distance[1] < alias.distance[0] {
        return Ok(None);
    }
    for volume in &mut alias.volume {
        *volume = (*volume * volume_scale).clamp(0., 1.);
    }
    Ok(Some(alias))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn map_bank_applies_defaults_overrides_and_duplicate_rules() -> io::Result<()> {
        let root = tempfile::tempdir()?;
        let directory = root.path().join("main/soundaliases");
        std::fs::create_dir_all(&directory)?;
        std::fs::write(
            directory.join("volumemodgroups.def"),
            b"VOLUMEMODGROUPS\n// comment\nimpact 1.5\n",
        )?;
        std::fs::write(directory.join("base.csv"), b"name,sequence,file,vol_min,vol_mod,pitch_min,pitch_max,channel,type,loadspec,subtitle\r\n# comment\r\nHit,2,Loud.WAV,.5,Impact,2,1,weapon,primed,ALL_MP,\"two, three\"\r\nhit,1,first.wav,1,impact,,,,,mp_test,\r\nSkip,0,x,,,,,,,mp_other,\r\nDrop,0,x,,,,,,,,\r\nDrop,0,y,,,,,,,,\r\n")?;
        std::fs::write(
            directory.join("upgrade.csv"),
            b"file,name,sequence\nnew.wav,Hit,1\n",
        )?;
        let assets = GameAssets::mount_paths(root.path(), None, "")?;
        let bank = assets.load_sound_aliases("MP_TEST")?;
        assert_eq!(bank.volume_groups["impact"], 1.5);
        assert_eq!(bank.alias_count(), 2);
        let variants = bank.find("hit").expect("selected variants");
        assert_eq!(variants[0].file.as_ref(), "new.wav");
        assert_eq!(variants[1].file.as_ref(), "Loud.WAV");
        assert_eq!(variants[1].volume, [0.75, 0.75]);
        assert_eq!(variants[1].pitch, [1., 2.]);
        assert_eq!(variants[1].distance, [120., 600.]);
        assert_eq!(variants[1].subtitle, "two, three");
        assert_eq!(variants[1].flags, (3 << 7) | (2 << 5));
        Ok(())
    }

    #[test]
    fn load_specs_match_whole_names_and_invert_exclusions() {
        assert!(loadspec_matches("mp_farm", "all_mp"));
        assert!(!loadspec_matches("mp_farm", "!mp_farm"));
        assert!(loadspec_matches("mp_farm", "!mp_farmhouse"));
        assert!(loadspec_matches("mp_farm", "MP_TOWN MP_FARM"));
        assert!(!loadspec_matches("menu", "all_mp"));
        assert!(!loadspec_matches("é", "xéx"));
    }

    #[test]
    fn rejects_incomplete_volume_groups_and_unterminated_quoted_field() {
        assert!(volume_groups(b"INVALID foo 1").is_err());
        assert!(volume_groups(b"VOLUMEMODGROUPS foo").is_err());
        assert!(
            Csv {
                remaining: b"x,\"unclosed"
            }
            .row()
            .is_err()
        );
    }
}
