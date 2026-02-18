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
    pub interface_name: Option<String>,
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
pub struct HttpManagementConfig {
    pub enabled: bool,
    pub bind_address: String,
    pub port: u16,
}

#[derive(Debug, Clone)]
pub struct GrpcManagementConfig {
    pub enabled: bool,
    pub bind_address: String,
    pub port: u16,
}

#[derive(Debug, Clone)]
pub struct ManagementConfig {
    pub http: HttpManagementConfig,
    pub grpc: GrpcManagementConfig,
    pub forwarding_table: u32,
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
    pub management: ManagementConfig,
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
    iface: Option<String>,
}

#[derive(Debug, Deserialize, Default)]
struct RawForwarding {
    enabled: Option<bool>,
    dry_run: Option<bool>,
    table: Option<u32>,
    destination_prefixes: Option<BTreeMap<String, String>>,
    next_hop_ips: Option<BTreeMap<String, String>>,
}

#[derive(Debug, Deserialize, Default)]
struct RawManagementEndpoint {
    enabled: Option<bool>,
    bind: Option<String>,
    address: Option<String>,
    port: Option<u16>,
}

#[derive(Debug, Deserialize, Default)]
struct RawManagement {
    http: Option<RawManagementEndpoint>,
    grpc: Option<RawManagementEndpoint>,
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
    management: Option<RawManagement>,
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
    let management_raw = raw_cfg.management.unwrap_or_default();

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
            interface_name: item
                .iface
                .map(|name| name.trim().to_string())
                .filter(|name| !name.is_empty()),
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

    let bind_address = bind
        .address
        .or(raw_cfg.bind_address)
        .unwrap_or_else(|| "0.0.0.0".to_string());
    let bind_port = bind.port.or(raw_cfg.bind_port).unwrap_or(5500);

    let http_raw = management_raw.http.unwrap_or_default();
    let grpc_raw = management_raw.grpc.unwrap_or_default();
    let management = ManagementConfig {
        http: HttpManagementConfig {
            enabled: http_raw.enabled.unwrap_or(true),
            bind_address: endpoint_address(http_raw.bind.or(http_raw.address)),
            port: endpoint_port(http_raw.port, bind_port, 10_000),
        },
        grpc: GrpcManagementConfig {
            enabled: grpc_raw.enabled.unwrap_or(true),
            bind_address: endpoint_address(grpc_raw.bind.or(grpc_raw.address)),
            port: endpoint_port(grpc_raw.port, bind_port, 11_000),
        },
        forwarding_table: forwarding.table,
    };

    Ok(DaemonConfig {
        router_id: raw_cfg.router_id,
        protocol,
        bind_address,
        bind_port,
        tick_interval: timers.tick_interval.unwrap_or(1.0),
        dead_interval: timers.dead_interval.unwrap_or(4.0),
        neighbors,
        protocol_params,
        forwarding,
        management,
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

fn endpoint_address(raw: Option<String>) -> String {
    raw.unwrap_or_else(|| "0.0.0.0".to_string())
}

fn endpoint_port(raw: Option<u16>, bind_port: u16, offset: u16) -> u16 {
    match raw {
        Some(port) => port,
        None => {
            let candidate = u32::from(bind_port) + u32::from(offset);
            if candidate <= u32::from(u16::MAX) {
                candidate as u16
            } else {
                bind_port
            }
        }
    }
}
