use std::collections::BTreeMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use anyhow::Result;
use serde_json::{Map, Value};
use tracing::{debug, info, warn};

use crate::algo::{DecisionContext, DecisionEngine, PassthroughDecisionEngine};
use crate::model::messages::{decode_message, encode_message};
use crate::model::routing::{ForwardingTable, RouteTable};
use crate::model::state::{NeighborInfo, NeighborTable};
use crate::protocols::base::{ProtocolContext, ProtocolEngine, RouterLink};
use crate::protocols::ospf::{OspfProtocol, OspfTimers};
use crate::protocols::rip::{RipProtocol, RipTimers};
use crate::runtime::config::DaemonConfig;
use crate::runtime::forwarding::{
    ForwardingApplier, LinuxForwardingApplier, NullForwardingApplier,
};
use crate::runtime::mgmt::{DaemonSnapshot, MgmtServer};
use crate::runtime::transport::UdpTransport;

pub struct RouterDaemon {
    cfg: DaemonConfig,
    transport: UdpTransport,
    neighbor_table: NeighborTable,
    protocol: Box<dyn ProtocolEngine>,
    route_table: RouteTable,
    forwarding_table: ForwardingTable,
    applier: Box<dyn ForwardingApplier>,
    policy: Box<dyn DecisionEngine>,
    mgmt: MgmtServer,
    running: Arc<AtomicBool>,
    epoch: Instant,
}

impl RouterDaemon {
    pub fn new(cfg: DaemonConfig) -> Result<Self> {
        let transport = UdpTransport::bind(&cfg.bind_address, cfg.bind_port, 65_535)?;
        let neighbors: Vec<NeighborInfo> = cfg
            .neighbors
            .iter()
            .map(|neighbor| NeighborInfo {
                router_id: neighbor.router_id,
                address: neighbor.address.clone(),
                port: neighbor.port,
                cost: neighbor.cost,
                last_seen: None,
                is_up: false,
            })
            .collect();

        let protocol = Self::build_protocol(&cfg)?;
        let applier: Box<dyn ForwardingApplier> = if cfg.forwarding.enabled {
            Box::new(LinuxForwardingApplier::new(cfg.forwarding.clone()))
        } else {
            Box::new(NullForwardingApplier)
        };
        let initial_snapshot = DaemonSnapshot::from_parts(
            cfg.router_id,
            &cfg.protocol,
            &cfg.bind_address,
            cfg.bind_port,
            cfg.tick_interval,
            cfg.dead_interval,
            &cfg.forwarding,
            0.0,
            neighbors.clone(),
            Vec::new(),
            Vec::new(),
        );
        let mgmt = MgmtServer::start(initial_snapshot, &cfg.management)?;

        Ok(Self {
            cfg,
            transport,
            neighbor_table: NeighborTable::new(neighbors),
            protocol,
            route_table: RouteTable::default(),
            forwarding_table: ForwardingTable::default(),
            applier,
            policy: Box::new(PassthroughDecisionEngine),
            mgmt,
            running: Arc::new(AtomicBool::new(true)),
            epoch: Instant::now(),
        })
    }

    pub fn run_forever(&mut self) -> Result<()> {
        self.install_signal_handlers()?;

        info!(
            "routerd start: router_id={} protocol={} bind={}:{} neighbors={:?}",
            self.cfg.router_id,
            self.cfg.protocol,
            self.cfg.bind_address,
            self.cfg.bind_port,
            self.cfg
                .neighbors
                .iter()
                .map(|neighbor| neighbor.router_id)
                .collect::<Vec<u32>>()
        );

        let start_outputs = self.protocol.start(&self.context(self.now_secs()));
        self.apply_outputs(start_outputs)?;
        self.publish_snapshot();

        let mut next_tick = self.now_secs() + self.cfg.tick_interval;

        while self.running.load(Ordering::Relaxed) {
            let now = self.now_secs();
            let timeout_s = (next_tick - now).max(0.0);
            let timeout = if timeout_s <= 0.0 {
                Duration::from_millis(1)
            } else {
                Duration::from_secs_f64(timeout_s)
            };

            if let Some((payload, _addr)) = self.transport.recv(timeout)? {
                self.handle_packet(&payload, self.now_secs())?;
            }

            let now = self.now_secs();
            if now >= next_tick {
                let changed = self
                    .neighbor_table
                    .refresh_liveness(now, self.cfg.dead_interval);
                if !changed.is_empty() {
                    self.publish_snapshot();
                }
                let timer_outputs = self.protocol.on_timer(&self.context(now));
                self.apply_outputs(timer_outputs)?;
                next_tick = now + self.cfg.tick_interval;
            }
        }

        info!("routerd stopped");
        Ok(())
    }

    fn install_signal_handlers(&self) -> Result<()> {
        let running = Arc::clone(&self.running);
        ctrlc::set_handler(move || {
            running.store(false, Ordering::Relaxed);
        })?;
        Ok(())
    }

    fn handle_packet(&mut self, payload: &[u8], now: f64) -> Result<()> {
        let message = match decode_message(payload) {
            Ok(message) => message,
            Err(err) => {
                warn!("drop invalid packet: {err}");
                return Ok(());
            }
        };

        if message.protocol != self.protocol.name() {
            return Ok(());
        }

        if self.neighbor_table.get(message.src_router_id).is_none() {
            debug!(
                "drop packet from unknown router_id={}",
                message.src_router_id
            );
            return Ok(());
        }

        self.neighbor_table.mark_seen(message.src_router_id, now);
        self.publish_snapshot();

        let outputs = self.protocol.on_message(&self.context(now), &message);
        self.apply_outputs(outputs)
    }

    fn apply_outputs(&mut self, outputs: crate::protocols::base::ProtocolOutputs) -> Result<()> {
        for (neighbor_id, message) in outputs.outbound {
            let Some(neighbor) = self.neighbor_table.get(neighbor_id) else {
                continue;
            };
            match encode_message(&message) {
                Ok(payload) => {
                    self.transport
                        .send(&payload, &neighbor.address, neighbor.port)?;
                }
                Err(err) => {
                    warn!("skip outbound message encode failure: {err}");
                }
            }
        }

        let Some(protocol_routes) = outputs.routes else {
            return Ok(());
        };

        let selected_routes = self.policy.choose_routes(
            &DecisionContext {
                router_id: self.cfg.router_id,
                protocol: self.protocol.name().to_string(),
                now: self.now_secs(),
            },
            &protocol_routes,
        );

        let updated = self
            .route_table
            .replace_protocol_routes(self.protocol.name(), &selected_routes);
        if !updated {
            return Ok(());
        }

        let routes = self.route_table.snapshot();
        let fib_updated = self.forwarding_table.sync_from_routes(&routes);
        if !fib_updated {
            return Ok(());
        }

        let fib_entries = self.forwarding_table.snapshot();
        self.applier.apply(&fib_entries)?;

        let summary: Vec<(u32, u32, f64)> = fib_entries
            .iter()
            .map(|entry| (entry.destination, entry.next_hop, entry.metric))
            .collect();
        info!("RIB/FIB updated: {:?}", summary);
        self.publish_snapshot();

        Ok(())
    }

    fn context(&self, now: f64) -> ProtocolContext {
        let links: BTreeMap<u32, RouterLink> = self
            .neighbor_table
            .iter()
            .map(|(router_id, info)| {
                (
                    *router_id,
                    RouterLink {
                        neighbor_id: *router_id,
                        cost: info.cost,
                        address: info.address.clone(),
                        port: info.port,
                        is_up: info.is_up,
                    },
                )
            })
            .collect();

        ProtocolContext {
            router_id: self.cfg.router_id,
            now,
            links,
        }
    }

    fn now_secs(&self) -> f64 {
        self.epoch.elapsed().as_secs_f64()
    }

    fn publish_snapshot(&self) {
        self.mgmt.publish(self.build_snapshot(self.now_secs()));
    }

    fn build_snapshot(&self, now: f64) -> DaemonSnapshot {
        let neighbors = self
            .neighbor_table
            .iter()
            .map(|(_, item)| item.clone())
            .collect::<Vec<_>>();
        let routes = self.route_table.snapshot();
        let fib = self.forwarding_table.snapshot();
        DaemonSnapshot::from_parts(
            self.cfg.router_id,
            self.protocol.name(),
            &self.cfg.bind_address,
            self.cfg.bind_port,
            self.cfg.tick_interval,
            self.cfg.dead_interval,
            &self.cfg.forwarding,
            now,
            neighbors,
            routes,
            fib,
        )
    }

    fn build_protocol(cfg: &DaemonConfig) -> Result<Box<dyn ProtocolEngine>> {
        let params = &cfg.protocol_params;
        match cfg.protocol.as_str() {
            "ospf" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));

                Ok(Box::new(OspfProtocol::new(OspfTimers {
                    hello_interval,
                    lsa_interval,
                    lsa_max_age,
                })))
            }
            "rip" => {
                let update_interval = param_f64(params, "update_interval", 5.0);
                let neighbor_timeout =
                    param_f64(params, "neighbor_timeout", cfg.dead_interval.max(15.0));
                let infinity_metric = param_f64(params, "infinity_metric", 16.0);
                let poison_reverse = param_bool(params, "poison_reverse", true);

                Ok(Box::new(RipProtocol::new(
                    RipTimers {
                        update_interval,
                        neighbor_timeout,
                    },
                    infinity_metric,
                    poison_reverse,
                )))
            }
            "irp" => {
                let alpha = param_f64(params, "alpha", 1.0);
                let beta = param_f64(params, "beta", 2.0);
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                info!(
                    "IRP mode enabled (current engine: OSPF base), alpha={}, beta={}",
                    alpha, beta
                );
                Ok(Box::new(OspfProtocol::new(OspfTimers {
                    hello_interval,
                    lsa_interval,
                    lsa_max_age,
                })))
            }
            _ => anyhow::bail!("unsupported protocol: {}", cfg.protocol),
        }
    }
}

fn param_f64(params: &Map<String, Value>, key: &str, default: f64) -> f64 {
    match params.get(key) {
        Some(Value::Number(num)) => num.as_f64().unwrap_or(default),
        Some(Value::String(text)) => text.parse::<f64>().unwrap_or(default),
        _ => default,
    }
}

fn param_bool(params: &Map<String, Value>, key: &str, default: bool) -> bool {
    match params.get(key) {
        Some(Value::Bool(flag)) => *flag,
        Some(Value::String(text)) => match text.trim().to_ascii_lowercase().as_str() {
            "1" | "true" | "yes" => true,
            "0" | "false" | "no" => false,
            _ => default,
        },
        _ => default,
    }
}
