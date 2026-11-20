//! G_InitGame's level/start callbacks and SV_SpawnServer's settling frames.
use super::Level;
use crate::{assets::GameAssets, config::ServerConfig, vm::GameHost};
use std::io;

impl Level {
    pub(super) fn start_gameplay(
        &mut self,
        assets: &GameAssets,
        config: &mut ServerConfig,
        start_time: i32,
        gametype_module: &str,
    ) -> io::Result<()> {
        let map_module = format!("maps/mp/{}", self.name);
        let mut host = GameHost {
            weapons: &mut self.weapons,
            assets,
            models: &mut self.models,
            start_time,
            collision: &self.collision,
            world: &mut self.world,
            config,
            strings: &mut self.configstrings,
            time_ms: start_time,
            initializing: true,
        };
        self.vm.invoke(
            &self.scripts,
            &mut host,
            gametype_module,
            "main",
            0,
            Vec::new(),
        )?;
        // GScr_LoadLevelScript looks up main without requiring that the map supplies it.
        if self.scripts.resolve_function(&map_module, "main").is_some() {
            self.vm
                .invoke(&self.scripts, &mut host, &map_module, "main", 0, vec![])?;
        }
        self.vm.invoke(
            &self.scripts,
            &mut host,
            "maps/mp/gametypes/_callbacksetup",
            "codecallback_startgametype",
            0,
            vec![],
        )?;
        // ItemRegistry_BuildNameList and ItemRegistry_BuildBitmask publish the
        // final registrations even when the last script call did not add an item.
        self.configstrings
            .set(7, self.weapons.names_configstring().as_bytes())?;
        self.configstrings
            .set(8, self.weapons.items_configstring().as_bytes())?;
        for frame in 1..=3 {
            self.frame(assets, config, start_time.wrapping_add(frame * 100))?;
        }
        self.freeze_baselines();
        Ok(())
    }
}
