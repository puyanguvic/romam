use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

use anyhow::{Context, Result};
use serde::Deserialize;
use serde_json::{Map, Value};

#[derive(Debug, Clone)]
pub struct NeighborConfig {
    pub router_id: u32,
    pub address: String,
    pub port: u16,
    pub cost: f64,
}

#[derive(Debug, Clone)]
pub struct ForwardingConfig {
    pub enabled: bool,
    pub dry_run: bool,
    pub table: u32,
    pub destination_prefixes: BTreeMap<u32, String>,
    pub next_hop_ips: BTreeMap<u32, String>,
}

impl Default for ForwardingConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            dry_run: true,
            table: 254,
            destination_prefixes: BTreeMap::new(),
            next_hop_ips: BTreeMap::new(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct DaemonConfig {
    pub router_id: u32,
    pub protocol: String,
    pub bind_address: String,
    pub bind_port: u16,
    pub tick_interval: f64,
    pub dead_interval: f64,
    pub neighbors: Vec<NeighborConfig>,
    pub protocol_params: Map<String, Value>,
    pub forwarding: ForwardingConfig,
}

#[derive(Debug, Deserialize, Default)]
struct RawBind {
    address: Option<String>,
    port: Option<u16>,
}

#[derive(Debug, Deserialize, Default)]
struct RawTimers {
    tick_interval: Option<f64>,
    dead_interval: Option<f64>,
}

#[derive(Debug, Deserialize)]
struct RawNeighbor {
    router_id: u32,
    address: String,
    port: Option<u16>,
    cost: Option<f64>,
}

#[derive(Debug, Deserialize, Default)]
struct RawForwarding {
    enabled: Option<bool>,
    dry_run: Option<bool>,
    table: Option<u32>,
    destination_prefixes: Option<BTreeMap<String, String>>,
    next_hop_ips: Option<BTreeMap<String, String>>,
}

#[derive(Debug, Deserialize)]
struct RawDaemonConfig {
    router_id: u32,
    protocol: Option<String>,
    bind: Option<RawBind>,
    timers: Option<RawTimers>,
    #[serde(default)]
    neighbors: Vec<RawNeighbor>,
    #[serde(default)]
    protocol_params: BTreeMap<String, serde_yaml::Value>,
    forwarding: Option<RawForwarding>,
    bind_address: Option<String>,
    bind_port: Option<u16>,
}

pub fn load_daemon_config(path: &Path) -> Result<DaemonConfig> {
    let raw_text = fs::read_to_string(path)
        .with_context(|| format!("failed to read config file {}", path.display()))?;
    let raw_cfg: RawDaemonConfig =
        serde_yaml::from_str(&raw_text).context("failed to parse daemon config yaml")?;

    let bind = raw_cfg.bind.unwrap_or_default();
    let timers = raw_cfg.timers.unwrap_or_default();
    let forwarding_raw = raw_cfg.forwarding.unwrap_or_default();

    let protocol = raw_cfg
        .protocol
        .unwrap_or_else(|| "ospf".to_string())
        .to_lowercase();

    let protocol_params = raw_cfg
        .protocol_params
        .get(&protocol)
        .map(yaml_to_json_object)
        .transpose()?
        .unwrap_or_default();

    let neighbors = raw_cfg
        .neighbors
        .into_iter()
        .map(|item| NeighborConfig {
            router_id: item.router_id,
            address: item.address,
            port: item.port.unwrap_or(5500),
            cost: item.cost.unwrap_or(1.0),
        })
        .collect();

    let forwarding = ForwardingConfig {
        enabled: forwarding_raw.enabled.unwrap_or(false),
        dry_run: forwarding_raw.dry_run.unwrap_or(true),
        table: forwarding_raw.table.unwrap_or(254),
        destination_prefixes: parse_int_key_map(
            forwarding_raw.destination_prefixes.unwrap_or_default(),
        )?,
        next_hop_ips: parse_int_key_map(forwarding_raw.next_hop_ips.unwrap_or_default())?,
    };

    Ok(DaemonConfig {
        router_id: raw_cfg.router_id,
        protocol,
        bind_address: bind
            .address
            .or(raw_cfg.bind_address)
            .unwrap_or_else(|| "0.0.0.0".to_string()),
        bind_port: bind.port.or(raw_cfg.bind_port).unwrap_or(5500),
        tick_interval: timers.tick_interval.unwrap_or(1.0),
        dead_interval: timers.dead_interval.unwrap_or(4.0),
        neighbors,
        protocol_params,
        forwarding,
    })
}

fn yaml_to_json_object(value: &serde_yaml::Value) -> Result<Map<String, Value>> {
    let json_value = serde_json::to_value(value).context("failed to convert protocol params")?;
    Ok(match json_value {
        Value::Object(obj) => obj,
        _ => Map::new(),
    })
}

fn parse_int_key_map(raw: BTreeMap<String, String>) -> Result<BTreeMap<u32, String>> {
    let mut out = BTreeMap::new();
    for (key, value) in raw {
        let parsed = key
            .parse::<u32>()
            .with_context(|| format!("invalid integer map key: {key}"))?;
        out.insert(parsed, value);
    }
    Ok(out)
}
