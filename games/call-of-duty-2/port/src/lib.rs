//! Dedicated-server state and protocol for the CoD2 1.3 Rust port.
#![forbid(unsafe_code)]

pub mod admission;
pub mod animation;
pub mod assets;
pub mod bots;
pub mod challenge;
pub mod client;
pub mod collision;
pub mod combat;
pub mod config;
mod csv;
pub mod download;
pub mod entities;
pub mod entity_delta;
pub mod gamestate;
pub mod level;
pub mod master;
pub mod message;
pub mod movement;
pub mod netchan;
pub mod player;
pub mod query;
pub mod rcon;
pub mod script;
pub mod snapshot;
pub mod usercmd;
pub mod vm;
mod voice;

pub mod vote;
