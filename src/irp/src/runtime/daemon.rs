use std::collections::{BTreeMap, BTreeSet};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use anyhow::Result;
use serde_json::{Map, Value};
use tracing::{debug, info, warn};

use crate::algo::{DecisionContext, DecisionEngine, PassthroughDecisionEngine};
use crate::model::control_plane::ExchangeScope;
use crate::model::messages::{decode_message, encode_message};
use crate::model::routing::{ForwardingTable, RouteTable};
use crate::model::state::{NeighborInfo, NeighborTable};
use crate::protocols::base::{
    Ipv4RoutingProtocol, ProtocolContext, ProtocolOutputs, QdiscAction, QdiscLinkSnapshot,
    RouterLink,
};
use crate::protocols::ddr::{DdrParams, DdrProtocol, DdrTimers};
use crate::protocols::ecmp::{EcmpParams, EcmpProtocol, EcmpTimers};
use crate::protocols::ospf::{OspfProtocol, OspfTimers};
use crate::protocols::profile::build_protocol_metrics;
use crate::protocols::rip::{RipProtocol, RipTimers};
use crate::protocols::spath::{SPathAlgorithm, SPathParams, SPathProtocol, SPathTimers};
use crate::protocols::topk::{TopkParams, TopkProtocol, TopkTimers};
use crate::runtime::config::DaemonConfig;
use crate::runtime::forwarding::{
    ForwardingApplier, LinuxForwardingApplier, NullForwardingApplier,
};
use crate::runtime::mgmt::{DaemonSnapshot, MgmtServer, QdiscSnapshot};
use crate::runtime::qdisc::{LinuxTcQdiscDriver, QdiscController, QdiscRuntimeStats};
use crate::runtime::transport::UdpTransport;

pub struct RouterDaemon {
    cfg: DaemonConfig,
    transport: UdpTransport,
    neighbor_table: NeighborTable,
    protocol: Box<dyn Ipv4RoutingProtocol>,
    route_table: RouteTable,
    forwarding_table: ForwardingTable,
    applier: Box<dyn ForwardingApplier>,
    qdisc_controller: Option<QdiscController>,
    qdisc_snapshots: BTreeMap<String, QdiscSnapshot>,
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
                interface_name: neighbor.interface_name.clone(),
                last_seen: None,
                is_up: false,
            })
            .collect();

        let mut protocol = Self::build_protocol(&cfg)?;
        protocol.set_ipv4_context(cfg.router_id);
        let applier: Box<dyn ForwardingApplier> = if cfg.forwarding.enabled {
            Box::new(LinuxForwardingApplier::new(cfg.forwarding.clone()))
        } else {
            Box::new(NullForwardingApplier)
        };
        let qdisc_controller = if cfg.qdisc.enabled {
            Some(QdiscController::new(
                Box::new(LinuxTcQdiscDriver::new(cfg.qdisc.dry_run)),
                &cfg.qdisc,
            )?)
        } else {
            None
        };
        let initial_snapshot = DaemonSnapshot::from_parts(
            cfg.router_id,
            &cfg.protocol,
            &cfg.bind_address,
            cfg.bind_port,
            cfg.tick_interval,
            cfg.dead_interval,
            &cfg.forwarding,
            build_protocol_metrics(&cfg.protocol, protocol.metrics()),
            0.0,
            neighbors.clone(),
            Vec::new(),
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
            qdisc_controller,
            qdisc_snapshots: BTreeMap::new(),
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
        self.apply_qdisc_profiles()?;
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
                let qdisc_changed = self.refresh_qdisc_snapshots();
                for router_id in &changed {
                    let outputs = if self
                        .neighbor_table
                        .get(*router_id)
                        .is_some_and(|neighbor| neighbor.is_up)
                    {
                        self.protocol
                            .notify_interface_up(&self.context(now), *router_id)
                    } else {
                        self.protocol
                            .notify_interface_down(&self.context(now), *router_id)
                    };
                    self.apply_outputs(outputs)?;
                }
                if !changed.is_empty() || qdisc_changed {
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

        let became_up = self.neighbor_table.mark_seen(message.src_router_id, now);
        if became_up {
            let outputs = self
                .protocol
                .notify_interface_up(&self.context(now), message.src_router_id);
            self.apply_outputs(outputs)?;
        }
        self.publish_snapshot();

        let outputs = self.protocol.on_message(&self.context(now), &message);
        self.apply_outputs(outputs)
    }

    fn apply_outputs(&mut self, outputs: ProtocolOutputs) -> Result<()> {
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

        let qdisc_changed = self.apply_qdisc_actions(outputs.qdisc_actions)?;

        let Some(protocol_routes) = outputs.routes else {
            if qdisc_changed {
                self.publish_snapshot();
            }
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

        let rib_updated = self
            .route_table
            .replace_protocol_routes(self.protocol.name(), &protocol_routes);
        let fib_updated = self.forwarding_table.sync_from_routes(&selected_routes);
        if !rib_updated && !fib_updated {
            if qdisc_changed {
                self.publish_snapshot();
            }
            return Ok(());
        }

        if fib_updated {
            let fib_entries = self.forwarding_table.snapshot();
            self.applier.apply(&fib_entries)?;

            let summary: Vec<(u32, u32, f64)> = fib_entries
                .iter()
                .map(|entry| (entry.destination, entry.next_hop, entry.metric))
                .collect();
            info!("RIB/FIB updated: {:?}", summary);
        }
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
                        interface_name: info.interface_name.clone(),
                        is_up: info.is_up,
                    },
                )
            })
            .collect();
        let qdisc_by_neighbor: BTreeMap<u32, QdiscLinkSnapshot> = self
            .neighbor_table
            .iter()
            .filter_map(|(router_id, info)| {
                let iface = info
                    .interface_name
                    .as_deref()
                    .map(str::trim)
                    .filter(|name| !name.is_empty())?;
                let snapshot = self.qdisc_snapshots.get(iface);
                Some((
                    *router_id,
                    QdiscLinkSnapshot {
                        interface_name: Some(iface.to_string()),
                        kind: snapshot.and_then(|item| item.kind.clone()),
                        backlog_bytes: snapshot.and_then(|item| item.backlog_bytes),
                        backlog_packets: snapshot.and_then(|item| item.backlog_packets),
                        drops: snapshot.and_then(|item| item.drops),
                        overlimits: snapshot.and_then(|item| item.overlimits),
                        requeues: snapshot.and_then(|item| item.requeues),
                        error: snapshot.and_then(|item| item.error.clone()),
                    },
                ))
            })
            .collect();

        ProtocolContext {
            router_id: self.cfg.router_id,
            now,
            links,
            qdisc_by_neighbor,
        }
    }

    fn now_secs(&self) -> f64 {
        self.epoch.elapsed().as_secs_f64()
    }

    fn publish_snapshot(&self) {
        self.mgmt.publish(self.build_snapshot(self.now_secs()));
    }

    fn apply_qdisc_profiles(&mut self) -> Result<()> {
        let ifaces = self.qdisc_interfaces();
        {
            let Some(controller) = self.qdisc_controller.as_ref() else {
                return Ok(());
            };
            controller.apply_to_interfaces(&ifaces)?;
        }
        let _ = self.refresh_qdisc_snapshots();
        Ok(())
    }

    fn qdisc_interfaces(&self) -> Vec<String> {
        self.cfg
            .neighbors
            .iter()
            .filter_map(|n| {
                n.interface_name
                    .as_deref()
                    .map(str::trim)
                    .filter(|iface| !iface.is_empty())
                    .map(str::to_string)
            })
            .collect::<BTreeSet<String>>()
            .into_iter()
            .collect()
    }

    fn refresh_qdisc_snapshots(&mut self) -> bool {
        if self.qdisc_controller.is_none() {
            let had_any = !self.qdisc_snapshots.is_empty();
            self.qdisc_snapshots.clear();
            return had_any;
        }
        let tracked = self.qdisc_interfaces();
        let mut changed = false;
        for iface in &tracked {
            if self.refresh_qdisc_snapshot_for_interface(iface) {
                changed = true;
            }
        }
        let tracked_set: BTreeSet<String> = tracked.into_iter().collect();
        let before = self.qdisc_snapshots.len();
        self.qdisc_snapshots
            .retain(|iface, _| tracked_set.contains(iface));
        changed || self.qdisc_snapshots.len() != before
    }

    fn refresh_qdisc_snapshot_for_interface(&mut self, iface: &str) -> bool {
        let Some(controller) = self.qdisc_controller.as_ref() else {
            return false;
        };
        let next = match controller.stats_for_interface(iface) {
            Ok(stats) => Self::qdisc_snapshot_from_stats(iface, stats),
            Err(err) => QdiscSnapshot {
                interface_name: iface.to_string(),
                error: Some(err.to_string()),
                ..QdiscSnapshot::default()
            },
        };
        let changed = self.qdisc_snapshots.get(iface) != Some(&next);
        self.qdisc_snapshots.insert(iface.to_string(), next);
        changed
    }

    fn qdisc_snapshot_from_stats(iface: &str, stats: QdiscRuntimeStats) -> QdiscSnapshot {
        QdiscSnapshot {
            interface_name: iface.to_string(),
            kind: stats.kind,
            backlog_bytes: stats.backlog_bytes,
            backlog_packets: stats.backlog_packets,
            drops: stats.drops,
            overlimits: stats.overlimits,
            requeues: stats.requeues,
            error: None,
        }
    }

    fn apply_qdisc_actions(&mut self, actions: Vec<QdiscAction>) -> Result<bool> {
        if actions.is_empty() {
            return Ok(false);
        }
        if self.qdisc_controller.is_none() {
            warn!(
                "protocol emitted {} qdisc action(s) while qdisc runtime is disabled",
                actions.len()
            );
            return Ok(false);
        }

        let mut changed = false;
        for action in actions {
            match action {
                QdiscAction::ApplyDefault { interface_name } => {
                    if let Some(controller) = self.qdisc_controller.as_ref() {
                        controller.apply_for_interface(&interface_name)?;
                    }
                    if self.refresh_qdisc_snapshot_for_interface(&interface_name) {
                        changed = true;
                    }
                }
                QdiscAction::ApplyProfile {
                    interface_name,
                    kind,
                    handle,
                    params,
                } => {
                    if let Some(controller) = self.qdisc_controller.as_ref() {
                        controller.apply_custom_for_interface(
                            &interface_name,
                            &kind,
                            handle.as_deref(),
                            &params,
                        )?;
                    }
                    if self.refresh_qdisc_snapshot_for_interface(&interface_name) {
                        changed = true;
                    }
                }
                QdiscAction::Clear { interface_name } => {
                    if let Some(controller) = self.qdisc_controller.as_ref() {
                        controller.clear_for_interface(&interface_name)?;
                    }
                    if self.refresh_qdisc_snapshot_for_interface(&interface_name) {
                        changed = true;
                    }
                }
            }
        }

        Ok(changed)
    }

    fn build_snapshot(&self, now: f64) -> DaemonSnapshot {
        let neighbors = self
            .neighbor_table
            .iter()
            .map(|(_, item)| item.clone())
            .collect::<Vec<_>>();
        let routes = self.route_table.snapshot();
        let fib = self.forwarding_table.snapshot();
        let qdisc = self.qdisc_snapshots.values().cloned().collect::<Vec<_>>();
        DaemonSnapshot::from_parts(
            self.cfg.router_id,
            self.protocol.name(),
            &self.cfg.bind_address,
            self.cfg.bind_port,
            self.cfg.tick_interval,
            self.cfg.dead_interval,
            &self.cfg.forwarding,
            build_protocol_metrics(self.protocol.name(), self.protocol.metrics()),
            now,
            neighbors,
            routes,
            fib,
            qdisc,
        )
    }

    fn build_protocol(cfg: &DaemonConfig) -> Result<Box<dyn Ipv4RoutingProtocol>> {
        let params = &cfg.protocol_params;
        let hello_scope = param_exchange_scope(params, "hello_scope", ExchangeScope::OneHop);
        let lsa_scope = param_exchange_scope(params, "lsa_scope", ExchangeScope::FloodDomain);
        match cfg.protocol.as_str() {
            "ospf" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                let lsa_min_trigger_spacing_s =
                    param_f64(params, "lsa_min_trigger_spacing_s", 0.0).max(0.0);

                let mut protocol = OspfProtocol::new(OspfTimers {
                    hello_interval,
                    lsa_interval,
                    lsa_max_age,
                    lsa_min_trigger_spacing_s,
                });
                protocol.set_descriptor_scopes(hello_scope, lsa_scope);
                Ok(Box::new(protocol))
            }
            "rip" => {
                let update_interval = param_f64(params, "update_interval", 5.0);
                let neighbor_timeout =
                    param_f64(params, "neighbor_timeout", cfg.dead_interval.max(15.0));
                let update_min_trigger_spacing_s =
                    param_f64(params, "update_min_trigger_spacing_s", 0.0).max(0.0);
                let infinity_metric = param_f64(params, "infinity_metric", 16.0);
                let poison_reverse = param_bool(params, "poison_reverse", true);
                let update_scope =
                    param_exchange_scope(params, "rip_update_scope", ExchangeScope::OneHop);

                let mut protocol = RipProtocol::new(
                    RipTimers {
                        update_interval,
                        neighbor_timeout,
                        update_min_trigger_spacing_s,
                    },
                    infinity_metric,
                    poison_reverse,
                );
                protocol.set_update_scope(update_scope);
                Ok(Box::new(protocol))
            }
            "ecmp" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                let lsa_min_trigger_spacing_s =
                    param_f64(params, "lsa_min_trigger_spacing_s", 0.0).max(0.0);
                let hash_seed = param_u64(params, "hash_seed", 1);

                let mut protocol = EcmpProtocol::new(EcmpParams {
                    timers: EcmpTimers {
                        hello_interval,
                        lsa_interval,
                        lsa_max_age,
                        lsa_min_trigger_spacing_s,
                    },
                    hash_seed,
                });
                protocol.set_descriptor_scopes(hello_scope, lsa_scope);
                Ok(Box::new(protocol))
            }
            "topk" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                let lsa_min_trigger_spacing_s =
                    param_f64(params, "lsa_min_trigger_spacing_s", 0.0).max(0.0);
                let k_paths = param_usize(params, "k_paths", 3).max(1);
                let explore_probability = param_f64(params, "explore_probability", 0.3);
                let selection_hold_time_s = param_f64(params, "selection_hold_time_s", 3.0);
                let rng_seed = param_u64(params, "rng_seed", 1);

                let mut protocol = TopkProtocol::new(TopkParams {
                    timers: TopkTimers {
                        hello_interval,
                        lsa_interval,
                        lsa_max_age,
                        lsa_min_trigger_spacing_s,
                    },
                    k_paths,
                    explore_probability,
                    selection_hold_time_s,
                    rng_seed,
                });
                protocol.set_descriptor_scopes(hello_scope, lsa_scope);
                Ok(Box::new(protocol))
            }
            "spath" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                let lsa_min_trigger_spacing_s =
                    param_f64(params, "lsa_min_trigger_spacing_s", 0.0).max(0.0);
                let algorithm =
                    SPathAlgorithm::from_str(&param_string(params, "algorithm", "dijkstra"));
                let k_paths = param_usize(params, "k_paths", 3).max(1);
                let hash_seed = param_u64(params, "hash_seed", 1);

                let mut protocol = SPathProtocol::new(SPathParams {
                    timers: SPathTimers {
                        hello_interval,
                        lsa_interval,
                        lsa_max_age,
                        lsa_min_trigger_spacing_s,
                    },
                    algorithm,
                    k_paths,
                    hash_seed,
                });
                protocol.set_descriptor_scopes(hello_scope, lsa_scope);
                Ok(Box::new(protocol))
            }
            "ddr" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                let lsa_min_trigger_spacing_s =
                    param_f64(params, "lsa_min_trigger_spacing_s", 0.0).max(0.0);
                let queue_sample_interval =
                    param_f64(params, "queue_sample_interval", cfg.tick_interval.max(0.5));
                let k_paths = param_usize(params, "k_paths", 3);
                let deadline_ms = param_f64(params, "deadline_ms", 100.0);
                let flow_size_bytes = param_f64(params, "flow_size_bytes", 64_000.0).max(1.0);
                let link_bandwidth_bps =
                    param_f64(params, "link_bandwidth_bps", 9_600_000.0).max(1.0);
                let queue_levels = param_usize(params, "queue_levels", 4).max(1);
                let pressure_threshold = param_usize(params, "pressure_threshold", 2);
                let queue_level_scale_ms = param_f64(params, "queue_level_scale_ms", 8.0).max(1e-6);
                let neighbor_state_max_age_s =
                    param_f64(params, "neighbor_state_max_age_s", 0.0).max(0.0);
                let randomize_route_selection =
                    param_bool(params, "randomize_route_selection", false);
                let rng_seed = param_u64(params, "rng_seed", 1);

                let mut protocol = DdrProtocol::new_with_name(
                    DdrParams {
                        timers: DdrTimers {
                            hello_interval,
                            lsa_interval,
                            lsa_max_age,
                            lsa_min_trigger_spacing_s,
                            queue_sample_interval,
                        },
                        k_paths,
                        deadline_ms,
                        flow_size_bytes,
                        link_bandwidth_bps,
                        queue_levels,
                        pressure_threshold,
                        queue_level_scale_ms,
                        neighbor_state_max_age_s,
                        randomize_route_selection,
                        rng_seed,
                    },
                    "ddr",
                );
                protocol.set_descriptor_scopes(hello_scope, lsa_scope);
                Ok(Box::new(protocol))
            }
            "dgr" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                let lsa_min_trigger_spacing_s =
                    param_f64(params, "lsa_min_trigger_spacing_s", 0.0).max(0.0);
                let queue_sample_interval =
                    param_f64(params, "queue_sample_interval", cfg.tick_interval.max(0.5));
                let k_paths = param_usize(params, "k_paths", 3);
                let deadline_ms = param_f64(params, "deadline_ms", 100.0);
                let flow_size_bytes = param_f64(params, "flow_size_bytes", 64_000.0).max(1.0);
                let link_bandwidth_bps =
                    param_f64(params, "link_bandwidth_bps", 9_600_000.0).max(1.0);
                let queue_levels = param_usize(params, "queue_levels", 4).max(1);
                let pressure_threshold = param_usize(params, "pressure_threshold", 2);
                let queue_level_scale_ms = param_f64(params, "queue_level_scale_ms", 8.0).max(1e-6);
                let neighbor_state_max_age_s =
                    param_f64(params, "neighbor_state_max_age_s", 0.0).max(0.0);
                let randomize_route_selection =
                    param_bool(params, "randomize_route_selection", true);
                let rng_seed = param_u64(params, "rng_seed", 1);

                let mut protocol = DdrProtocol::new_with_name(
                    DdrParams {
                        timers: DdrTimers {
                            hello_interval,
                            lsa_interval,
                            lsa_max_age,
                            lsa_min_trigger_spacing_s,
                            queue_sample_interval,
                        },
                        k_paths,
                        deadline_ms,
                        flow_size_bytes,
                        link_bandwidth_bps,
                        queue_levels,
                        pressure_threshold,
                        queue_level_scale_ms,
                        neighbor_state_max_age_s,
                        randomize_route_selection,
                        rng_seed,
                    },
                    "dgr",
                );
                protocol.set_descriptor_scopes(hello_scope, lsa_scope);
                Ok(Box::new(protocol))
            }
            "octopus" => {
                let hello_interval = param_f64(params, "hello_interval", 1.0);
                let lsa_interval = param_f64(params, "lsa_interval", 3.0);
                let lsa_max_age =
                    param_f64(params, "lsa_max_age", (cfg.dead_interval * 3.0).max(10.0));
                let lsa_min_trigger_spacing_s =
                    param_f64(params, "lsa_min_trigger_spacing_s", 0.0).max(0.0);
                let queue_sample_interval =
                    param_f64(params, "queue_sample_interval", cfg.tick_interval.max(0.5));
                let k_paths = param_usize(params, "k_paths", 3);
                // Octopus is queue-aware multipath with stochastic exploration, so deadline
                // filtering defaults to effectively disabled unless explicitly configured.
                let deadline_ms = param_f64(params, "deadline_ms", 1_000_000_000.0);
                let flow_size_bytes = param_f64(params, "flow_size_bytes", 64_000.0).max(1.0);
                let link_bandwidth_bps =
                    param_f64(params, "link_bandwidth_bps", 9_600_000.0).max(1.0);
                let queue_levels = param_usize(params, "queue_levels", 4).max(1);
                let pressure_threshold =
                    param_usize(params, "pressure_threshold", queue_levels - 1);
                let queue_level_scale_ms = param_f64(params, "queue_level_scale_ms", 8.0).max(1e-6);
                let neighbor_state_max_age_s =
                    param_f64(params, "neighbor_state_max_age_s", 0.0).max(0.0);
                let randomize_route_selection =
                    param_bool(params, "randomize_route_selection", true);
                let rng_seed = param_u64(params, "rng_seed", 1);

                let mut protocol = DdrProtocol::new_with_name(
                    DdrParams {
                        timers: DdrTimers {
                            hello_interval,
                            lsa_interval,
                            lsa_max_age,
                            lsa_min_trigger_spacing_s,
                            queue_sample_interval,
                        },
                        k_paths,
                        deadline_ms,
                        flow_size_bytes,
                        link_bandwidth_bps,
                        queue_levels,
                        pressure_threshold,
                        queue_level_scale_ms,
                        neighbor_state_max_age_s,
                        randomize_route_selection,
                        rng_seed,
                    },
                    "octopus",
                );
                protocol.set_descriptor_scopes(hello_scope, lsa_scope);
                Ok(Box::new(protocol))
            }
            "irp" => anyhow::bail!(
                "protocol 'irp' is an abstract architecture and cannot be instantiated; \
use a concrete protocol (ospf, rip, ecmp, topk, spath, ddr, dgr, octopus)"
            ),
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

fn param_usize(params: &Map<String, Value>, key: &str, default: usize) -> usize {
    match params.get(key) {
        Some(Value::Number(num)) => num
            .as_u64()
            .and_then(|v| usize::try_from(v).ok())
            .unwrap_or(default),
        Some(Value::String(text)) => text.parse::<usize>().unwrap_or(default),
        _ => default,
    }
}

fn param_u64(params: &Map<String, Value>, key: &str, default: u64) -> u64 {
    match params.get(key) {
        Some(Value::Number(num)) => num.as_u64().unwrap_or(default),
        Some(Value::String(text)) => text.parse::<u64>().unwrap_or(default),
        _ => default,
    }
}

fn param_string(params: &Map<String, Value>, key: &str, default: &str) -> String {
    match params.get(key) {
        Some(Value::String(text)) => text.clone(),
        Some(Value::Number(num)) => num.to_string(),
        Some(Value::Bool(flag)) => flag.to_string(),
        _ => default.to_string(),
    }
}

fn param_exchange_scope(
    params: &Map<String, Value>,
    key: &str,
    default: ExchangeScope,
) -> ExchangeScope {
    let raw = param_string(params, key, default.as_str());
    ExchangeScope::from_str(&raw).unwrap_or(default)
}
