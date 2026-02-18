use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::process::Command;
use std::sync::{Arc, RwLock};
use std::thread;
use std::time::Duration;

use anyhow::{Context, Result};
use serde::Serialize;
use serde_json::json;
use tracing::{debug, info, warn};

use crate::model::routing::{ForwardingEntry, Route};
use crate::model::state::NeighborInfo;
use crate::runtime::config::{ForwardingConfig, ManagementConfig};

#[derive(Debug, Clone, Serialize)]
pub struct NeighborSnapshot {
    pub router_id: u32,
    pub address: String,
    pub port: u16,
    pub cost: f64,
    pub interface_name: Option<String>,
    pub is_up: bool,
    pub last_seen: Option<f64>,
}

#[derive(Debug, Clone, Serialize)]
pub struct RouteSnapshot {
    pub destination: u32,
    pub next_hop: u32,
    pub metric: f64,
    pub protocol: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct DaemonSnapshot {
    pub router_id: u32,
    pub protocol: String,
    pub bind_address: String,
    pub bind_port: u16,
    pub now: f64,
    pub tick_interval: f64,
    pub dead_interval: f64,
    pub forwarding_enabled: bool,
    pub forwarding_table: u32,
    pub protocol_metrics: serde_json::Map<String, serde_json::Value>,
    pub neighbors: Vec<NeighborSnapshot>,
    pub routes: Vec<RouteSnapshot>,
    pub fib: Vec<RouteSnapshot>,
}

impl DaemonSnapshot {
    pub fn from_parts(
        router_id: u32,
        protocol: &str,
        bind_address: &str,
        bind_port: u16,
        tick_interval: f64,
        dead_interval: f64,
        forwarding_cfg: &ForwardingConfig,
        protocol_metrics: std::collections::BTreeMap<String, serde_json::Value>,
        now: f64,
        neighbors: Vec<NeighborInfo>,
        routes: Vec<Route>,
        fib: Vec<ForwardingEntry>,
    ) -> Self {
        Self {
            router_id,
            protocol: protocol.to_string(),
            bind_address: bind_address.to_string(),
            bind_port,
            now,
            tick_interval,
            dead_interval,
            forwarding_enabled: forwarding_cfg.enabled,
            forwarding_table: forwarding_cfg.table,
            protocol_metrics: protocol_metrics.into_iter().collect(),
            neighbors: neighbors
                .into_iter()
                .map(|item| NeighborSnapshot {
                    router_id: item.router_id,
                    address: item.address,
                    port: item.port,
                    cost: item.cost,
                    interface_name: item.interface_name,
                    is_up: item.is_up,
                    last_seen: item.last_seen,
                })
                .collect(),
            routes: routes
                .into_iter()
                .map(|item| RouteSnapshot {
                    destination: item.destination,
                    next_hop: item.next_hop,
                    metric: item.metric,
                    protocol: item.protocol,
                })
                .collect(),
            fib: fib
                .into_iter()
                .map(|item| RouteSnapshot {
                    destination: item.destination,
                    next_hop: item.next_hop,
                    metric: item.metric,
                    protocol: item.protocol,
                })
                .collect(),
        }
    }
}

pub struct MgmtServer {
    snapshot: Arc<RwLock<DaemonSnapshot>>,
}

impl MgmtServer {
    pub fn start(initial: DaemonSnapshot, cfg: &ManagementConfig) -> Result<Self> {
        let snapshot = Arc::new(RwLock::new(initial));

        if cfg.http.enabled {
            spawn_http_server(
                Arc::clone(&snapshot),
                cfg.http.bind_address.clone(),
                cfg.http.port,
                cfg.forwarding_table,
            )?;
            info!(
                "routingd management HTTP started on {}:{}",
                cfg.http.bind_address, cfg.http.port
            );
        }

        if cfg.grpc.enabled {
            spawn_grpc_placeholder(cfg.grpc.bind_address.clone(), cfg.grpc.port)?;
            info!(
                "routingd management gRPC placeholder started on {}:{}",
                cfg.grpc.bind_address, cfg.grpc.port
            );
        }

        Ok(Self { snapshot })
    }

    pub fn publish(&self, snapshot: DaemonSnapshot) {
        if let Ok(mut guard) = self.snapshot.write() {
            *guard = snapshot;
        }
    }
}

fn spawn_http_server(
    snapshot: Arc<RwLock<DaemonSnapshot>>,
    bind_address: String,
    port: u16,
    forwarding_table: u32,
) -> Result<()> {
    let listener = TcpListener::bind((bind_address.as_str(), port)).with_context(|| {
        format!("failed to bind management HTTP server at {bind_address}:{port}")
    })?;
    listener
        .set_nonblocking(true)
        .context("failed to set HTTP listener non-blocking")?;

    thread::spawn(move || loop {
        match listener.accept() {
            Ok((stream, _addr)) => {
                if let Err(err) = handle_http_stream(stream, &snapshot, forwarding_table) {
                    debug!("management HTTP request failed: {err}");
                }
            }
            Err(err) if err.kind() == std::io::ErrorKind::WouldBlock => {
                thread::sleep(Duration::from_millis(80));
            }
            Err(err) => {
                warn!("management HTTP accept error: {err}");
                thread::sleep(Duration::from_millis(200));
            }
        }
    });

    Ok(())
}

fn handle_http_stream(
    mut stream: TcpStream,
    snapshot: &Arc<RwLock<DaemonSnapshot>>,
    forwarding_table: u32,
) -> Result<()> {
    let mut buf = [0_u8; 4096];
    let n = stream
        .read(&mut buf)
        .context("failed to read HTTP request from management socket")?;
    if n == 0 {
        return Ok(());
    }

    let request = String::from_utf8_lossy(&buf[..n]);
    let first_line = request.lines().next().unwrap_or_default();
    let path = first_line.split_whitespace().nth(1).unwrap_or("/");

    let body = match path {
        "/healthz" => json!({"status": "ok"}),
        "/v1/status" => {
            let state = snapshot
                .read()
                .map_err(|_| anyhow::anyhow!("management state lock poisoned"))?
                .clone();
            serde_json::to_value(state).unwrap_or_else(|_| json!({"status": "encode_error"}))
        }
        "/v1/routes" => {
            let routes = snapshot
                .read()
                .map_err(|_| anyhow::anyhow!("management state lock poisoned"))?
                .routes
                .clone();
            json!({"routes": routes})
        }
        "/v1/fib" => {
            let fib = snapshot
                .read()
                .map_err(|_| anyhow::anyhow!("management state lock poisoned"))?
                .fib
                .clone();
            json!({"fib": fib})
        }
        "/v1/metrics" => {
            let state = snapshot
                .read()
                .map_err(|_| anyhow::anyhow!("management state lock poisoned"))?
                .clone();
            let neighbors_total = state.neighbors.len();
            let neighbors_up = state.neighbors.iter().filter(|item| item.is_up).count();
            json!({
                "router_id": state.router_id,
                "protocol": state.protocol,
                "time_s": state.now,
                "neighbors_total": neighbors_total,
                "neighbors_up": neighbors_up,
                "route_count": state.routes.len(),
                "fib_count": state.fib.len(),
                "forwarding_enabled": state.forwarding_enabled,
                "forwarding_table": state.forwarding_table,
                "protocol_metrics": state.protocol_metrics,
            })
        }
        "/v1/kernel-routes" => {
            let routes = collect_kernel_routes(forwarding_table);
            json!({"table": forwarding_table, "routes": routes})
        }
        _ => json!({"error": "not_found", "path": path}),
    };

    let status_line = if path == "/healthz"
        || path == "/v1/status"
        || path == "/v1/routes"
        || path == "/v1/fib"
        || path == "/v1/metrics"
        || path == "/v1/kernel-routes"
    {
        "HTTP/1.1 200 OK"
    } else {
        "HTTP/1.1 404 Not Found"
    };

    let payload = serde_json::to_vec(&body).unwrap_or_else(|_| b"{\"error\":\"encode\"}".to_vec());
    let response = format!(
        "{status_line}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
        payload.len()
    );
    stream
        .write_all(response.as_bytes())
        .context("failed to write HTTP headers")?;
    stream
        .write_all(&payload)
        .context("failed to write HTTP body")?;
    Ok(())
}

fn spawn_grpc_placeholder(bind_address: String, port: u16) -> Result<()> {
    let listener = TcpListener::bind((bind_address.as_str(), port)).with_context(|| {
        format!("failed to bind management gRPC placeholder at {bind_address}:{port}")
    })?;
    listener
        .set_nonblocking(true)
        .context("failed to set gRPC placeholder listener non-blocking")?;

    thread::spawn(move || loop {
        match listener.accept() {
            Ok((mut stream, _addr)) => {
                let _ = stream.write_all(b"routingd gRPC endpoint placeholder\n");
            }
            Err(err) if err.kind() == std::io::ErrorKind::WouldBlock => {
                thread::sleep(Duration::from_millis(120));
            }
            Err(err) => {
                warn!("management gRPC placeholder accept error: {err}");
                thread::sleep(Duration::from_millis(200));
            }
        }
    });
    Ok(())
}

fn collect_kernel_routes(table: u32) -> Vec<String> {
    let output = Command::new("ip")
        .args(["route", "show", "table", &table.to_string()])
        .output();

    match output {
        Ok(out) if out.status.success() => String::from_utf8_lossy(&out.stdout)
            .lines()
            .map(str::trim)
            .filter(|line| !line.is_empty())
            .map(ToString::to_string)
            .collect(),
        Ok(out) => vec![format!(
            "ip route show table {} failed: {}",
            table,
            String::from_utf8_lossy(&out.stderr).trim()
        )],
        Err(err) => vec![format!("ip route show unavailable: {err}")],
    }
}
