use std::cmp::Ordering;
use std::collections::BTreeMap;
use std::process::Command;

use serde_json::{json, Value};

use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::Route;
use crate::model::state::{NeighborFastStatePatch, NeighborStateDb};
use crate::protocols::base::{ProtocolContext, ProtocolEngine, ProtocolOutputs};
use crate::protocols::link_state::{LinkStateControlPlane, LinkStateTimers};
use crate::protocols::route_compute::{k_shortest_simple_paths, PathCandidate};

#[derive(Debug, Clone)]
pub struct DdrTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
    pub queue_sample_interval: f64,
}

impl Default for DdrTimers {
    fn default() -> Self {
        Self {
            hello_interval: 1.0,
            lsa_interval: 3.0,
            lsa_max_age: 15.0,
            queue_sample_interval: 1.0,
        }
    }
}

#[derive(Debug, Clone)]
pub struct DdrParams {
    pub timers: DdrTimers,
    pub k_paths: usize,
    pub deadline_ms: f64,
    pub flow_size_bytes: f64,
    pub link_bandwidth_bps: f64,
    pub queue_levels: usize,
    pub pressure_threshold: usize,
    pub queue_level_scale_ms: f64,
    pub neighbor_state_max_age_s: f64,
    pub randomize_route_selection: bool,
    pub rng_seed: u64,
}

impl Default for DdrParams {
    fn default() -> Self {
        Self {
            timers: DdrTimers::default(),
            k_paths: 3,
            deadline_ms: 100.0,
            flow_size_bytes: 64_000.0,
            link_bandwidth_bps: 9_600_000.0,
            queue_levels: 4,
            pressure_threshold: 2,
            queue_level_scale_ms: 8.0,
            neighbor_state_max_age_s: 0.0,
            randomize_route_selection: false,
            rng_seed: 1,
        }
    }
}

#[derive(Debug, Clone, Default)]
struct KernelBacklog {
    bytes: Option<f64>,
    packets: Option<f64>,
}

#[derive(Debug, Clone)]
struct RouteChoice {
    next_hop: u32,
    completion_ms: f64,
    hop_count: usize,
    pressure_level: usize,
}

pub struct DdrProtocol {
    protocol_name: &'static str,
    params: DdrParams,
    last_queue_sample_at: f64,
    control_plane: LinkStateControlPlane,
    queue_depth_bytes: BTreeMap<u32, f64>,
    arrivals_since_sample_bytes: BTreeMap<u32, f64>,
    estimated_queue_delay_ms: BTreeMap<u32, f64>,
    queue_sample_source: BTreeMap<u32, String>,
    neighbor_state_db: NeighborStateDb,
    rng_state: u64,
}

impl DdrProtocol {
    pub fn new(params: DdrParams) -> Self {
        Self::new_with_name(params, "ddr")
    }

    pub fn new_with_name(params: DdrParams, protocol_name: &'static str) -> Self {
        let rng_seed = params.rng_seed.max(1);
        Self {
            protocol_name,
            params,
            last_queue_sample_at: -1e9,
            control_plane: LinkStateControlPlane::new(),
            queue_depth_bytes: BTreeMap::new(),
            arrivals_since_sample_bytes: BTreeMap::new(),
            estimated_queue_delay_ms: BTreeMap::new(),
            queue_sample_source: BTreeMap::new(),
            neighbor_state_db: NeighborStateDb::default(),
            rng_state: rng_seed,
        }
    }

    fn neighbor_state_max_age_s(&self) -> f64 {
        if self.params.neighbor_state_max_age_s > 0.0 {
            self.params.neighbor_state_max_age_s
        } else {
            (self
                .params
                .timers
                .hello_interval
                .max(self.params.timers.queue_sample_interval)
                * 3.0)
                .max(1.0)
        }
    }

    fn drive(&mut self, ctx: &ProtocolContext, force_lsa: bool) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
        let mut should_recompute = false;

        if self.sample_queue_delay(ctx) {
            should_recompute = true;
        }
        if self
            .neighbor_state_db
            .age_out(ctx.now, self.neighbor_state_max_age_s())
        {
            should_recompute = true;
        }

        let tick = self.control_plane.tick(
            ctx,
            LinkStateTimers {
                hello_interval: self.params.timers.hello_interval,
                lsa_interval: self.params.timers.lsa_interval,
                lsa_max_age: self.params.timers.lsa_max_age,
            },
            force_lsa,
        );

        if tick.hello_due {
            outputs.outbound.extend(self.send_hello(ctx));
        }

        if let Some(payload) = tick.lsa_payload.as_ref() {
            outputs
                .outbound
                .extend(self.flood_message(ctx, payload, None));
            should_recompute = true;
        }

        if tick.topology_changed {
            should_recompute = true;
        }
        if should_recompute {
            outputs.routes = Some(self.compute_routes(ctx));
        }

        outputs
    }

    fn note_arrival_bytes(&mut self, neighbor_id: u32, bytes: f64) {
        if bytes <= 0.0 {
            return;
        }
        let entry = self
            .arrivals_since_sample_bytes
            .entry(neighbor_id)
            .or_insert(0.0);
        *entry += bytes;
    }

    fn fallback_packet_size_bytes() -> f64 {
        1200.0
    }

    fn effective_bandwidth_bps(&self) -> f64 {
        self.params.link_bandwidth_bps.max(1e-9)
    }

    fn packets_to_bytes(packets: f64) -> f64 {
        packets.max(0.0) * Self::fallback_packet_size_bytes()
    }

    fn bytes_to_delay_ms(&self, bytes: f64) -> f64 {
        1_000.0 * 8.0 * bytes.max(0.0) / self.effective_bandwidth_bps().max(1e-9)
    }

    fn sample_queue_delay(&mut self, ctx: &ProtocolContext) -> bool {
        let sample_interval = self.params.timers.queue_sample_interval.max(0.05);
        let elapsed = ctx.now - self.last_queue_sample_at;
        if elapsed < sample_interval {
            return false;
        }

        let mut changed = false;
        let link_service_bytes =
            (elapsed.max(sample_interval) * self.effective_bandwidth_bps() / 8.0).max(1e-9);

        for (neighbor_id, link) in &ctx.links {
            if !link.is_up {
                if self.queue_depth_bytes.remove(neighbor_id).is_some() {
                    changed = true;
                }
                if self.estimated_queue_delay_ms.remove(neighbor_id).is_some() {
                    changed = true;
                }
                self.queue_sample_source.remove(neighbor_id);
                self.arrivals_since_sample_bytes.remove(neighbor_id);
                self.neighbor_state_db.remove(*neighbor_id);
                continue;
            }

            if let Some(iface) = link
                .interface_name
                .as_deref()
                .map(str::trim)
                .filter(|name| !name.is_empty())
            {
                if let Some(backlog) = Self::read_kernel_backlog(iface) {
                    self.arrivals_since_sample_bytes.remove(neighbor_id);

                    let queue_bytes = backlog
                        .bytes
                        .unwrap_or_else(|| Self::packets_to_bytes(backlog.packets.unwrap_or(0.0)));
                    let queue_delay_ms = self.bytes_to_delay_ms(queue_bytes);

                    let old_q_bytes = self
                        .queue_depth_bytes
                        .insert(*neighbor_id, queue_bytes)
                        .unwrap_or(-1.0);
                    let old_delay = self
                        .estimated_queue_delay_ms
                        .insert(*neighbor_id, queue_delay_ms)
                        .unwrap_or(-1.0);
                    self.queue_sample_source.insert(
                        *neighbor_id,
                        if backlog.bytes.is_some() {
                            "kernel_tc_bytes".to_string()
                        } else {
                            "kernel_tc_packets_est_bytes".to_string()
                        },
                    );

                    if (old_q_bytes - queue_bytes).abs() > 1e-6
                        || (old_delay - queue_delay_ms).abs() > 1e-6
                    {
                        changed = true;
                    }
                    continue;
                }
            }

            let q_prev_bytes = self
                .queue_depth_bytes
                .get(neighbor_id)
                .copied()
                .unwrap_or(0.0);
            let arrivals_bytes = self
                .arrivals_since_sample_bytes
                .remove(neighbor_id)
                .unwrap_or(0.0);
            let serviced_bytes = (q_prev_bytes + arrivals_bytes).min(link_service_bytes);
            let q_next_bytes = (q_prev_bytes + arrivals_bytes - serviced_bytes).max(0.0);
            let queue_delay_ms = self.bytes_to_delay_ms(q_next_bytes);

            let old_q_bytes = self
                .queue_depth_bytes
                .insert(*neighbor_id, q_next_bytes)
                .unwrap_or(-1.0);
            let old_delay = self
                .estimated_queue_delay_ms
                .insert(*neighbor_id, queue_delay_ms)
                .unwrap_or(-1.0);
            self.queue_sample_source
                .insert(*neighbor_id, "local_model_bytes".to_string());

            if (old_q_bytes - q_next_bytes).abs() > 1e-6
                || (old_delay - queue_delay_ms).abs() > 1e-6
            {
                changed = true;
            }
        }

        self.last_queue_sample_at = ctx.now;
        changed
    }

    fn read_kernel_backlog(iface: &str) -> Option<KernelBacklog> {
        let output = Command::new("tc")
            .args(["-s", "qdisc", "show", "dev", iface])
            .output()
            .ok()?;
        if !output.status.success() {
            return None;
        }
        let text = String::from_utf8(output.stdout).ok()?;
        Self::parse_kernel_backlog(&text)
    }

    fn parse_kernel_backlog(text: &str) -> Option<KernelBacklog> {
        for line in text.lines() {
            let tokens: Vec<&str> = line.split_whitespace().collect();
            for (idx, token) in tokens.iter().enumerate() {
                if *token != "backlog" {
                    continue;
                }
                let mut bytes = None;
                let mut packets = None;
                for candidate in tokens.iter().skip(idx + 1).take(6) {
                    if let Some(raw) = candidate.strip_suffix('b') {
                        if let Ok(value) = raw.parse::<f64>() {
                            bytes = Some(value.max(0.0));
                        }
                    }
                    if let Some(raw) = candidate.strip_suffix('p') {
                        if let Ok(value) = raw.parse::<f64>() {
                            packets = Some(value.max(0.0));
                        }
                    }
                }
                if bytes.is_some() || packets.is_some() {
                    return Some(KernelBacklog { bytes, packets });
                }
            }
        }
        None
    }

    fn send_hello(&mut self, ctx: &ProtocolContext) -> Vec<(u32, ControlMessage)> {
        const CONTROL_ARRIVAL_BYTES: f64 = 256.0;
        let queue_levels: BTreeMap<u32, usize> = ctx
            .links
            .keys()
            .copied()
            .map(|neighbor_id| (neighbor_id, self.queue_level_for_neighbor(neighbor_id)))
            .collect();
        let out = self
            .control_plane
            .send_hello(self.name(), ctx, |neighbor_id| {
                let mut payload = BTreeMap::new();
                payload.insert("router_id".to_string(), json!(ctx.router_id));
                payload.insert(
                    "queue_level".to_string(),
                    json!(queue_levels.get(&neighbor_id).copied().unwrap_or(0)),
                );
                payload
            });
        for (neighbor_id, _) in &out {
            self.note_arrival_bytes(*neighbor_id, CONTROL_ARRIVAL_BYTES);
        }
        out
    }

    fn flood_message(
        &mut self,
        ctx: &ProtocolContext,
        payload: &BTreeMap<String, Value>,
        exclude: Option<u32>,
    ) -> Vec<(u32, ControlMessage)> {
        const CONTROL_ARRIVAL_BYTES: f64 = 256.0;
        let out =
            self.control_plane
                .flood_lsa(self.name(), MessageKind::DdrLsa, ctx, payload, exclude);
        for (neighbor_id, _) in &out {
            self.note_arrival_bytes(*neighbor_id, CONTROL_ARRIVAL_BYTES);
        }
        out
    }

    fn build_graph(&self, ctx: &ProtocolContext) -> BTreeMap<u32, BTreeMap<u32, f64>> {
        self.control_plane.build_graph(ctx.router_id, true)
    }

    fn transfer_delay_ms(&self) -> f64 {
        self.bytes_to_delay_ms(self.params.flow_size_bytes.max(1.0))
    }

    fn queue_level_for_neighbor(&self, neighbor_id: u32) -> usize {
        let delay_ms = self
            .estimated_queue_delay_ms
            .get(&neighbor_id)
            .copied()
            .unwrap_or(0.0);
        self.quantize_queue_delay_ms(delay_ms)
    }

    fn quantize_queue_delay_ms(&self, delay_ms: f64) -> usize {
        let levels = self.params.queue_levels.max(1);
        if levels == 1 {
            return 0;
        }
        let scale_ms = self.params.queue_level_scale_ms.max(1e-6);
        let normalized = (delay_ms.max(0.0) / scale_ms).min(1.0);
        let mut level = (normalized * levels as f64).floor() as usize;
        if level >= levels {
            level = levels - 1;
        }
        level
    }

    fn payload_non_negative_f64(payload: &BTreeMap<String, Value>, key: &str) -> Option<f64> {
        payload
            .get(key)
            .and_then(Value::as_f64)
            .filter(|value| value.is_finite() && *value >= 0.0)
    }

    fn payload_unit_interval_f64(payload: &BTreeMap<String, Value>, key: &str) -> Option<f64> {
        Self::payload_non_negative_f64(payload, key).map(|value| value.min(1.0))
    }

    fn evaluate_path(&self, path: &PathCandidate, now: f64) -> Option<RouteChoice> {
        if path.nodes.len() < 2 {
            return None;
        }
        let next_hop = path.nodes[1];
        let pressure_level = self
            .neighbor_state_db
            .get_queue_level_fresh(next_hop, now, self.neighbor_state_max_age_s())
            .unwrap_or_else(|| self.queue_level_for_neighbor(next_hop));
        let queue_delay_ms = self
            .estimated_queue_delay_ms
            .get(&next_hop)
            .copied()
            .unwrap_or(0.0);
        let completion_ms = path.cost + queue_delay_ms + self.transfer_delay_ms();
        Some(RouteChoice {
            next_hop,
            completion_ms,
            hop_count: path.nodes.len() - 1,
            pressure_level,
        })
    }

    fn route_choice_better(a: &RouteChoice, b: &RouteChoice) -> bool {
        match a.completion_ms.partial_cmp(&b.completion_ms) {
            Some(Ordering::Less) => true,
            Some(Ordering::Greater) => false,
            _ => {
                if a.next_hop != b.next_hop {
                    a.next_hop < b.next_hop
                } else if a.pressure_level != b.pressure_level {
                    a.pressure_level < b.pressure_level
                } else {
                    a.hop_count < b.hop_count
                }
            }
        }
    }

    fn is_high_pressure(&self, choice: &RouteChoice) -> bool {
        choice.pressure_level > self.params.pressure_threshold
    }

    fn next_random_u64(&mut self) -> u64 {
        self.rng_state = self
            .rng_state
            .wrapping_mul(6_364_136_223_846_793_005)
            .wrapping_add(1);
        self.rng_state
    }

    fn choose_route(&mut self, candidates: &[RouteChoice]) -> Option<RouteChoice> {
        if candidates.is_empty() {
            return None;
        }
        if self.params.randomize_route_selection && candidates.len() > 1 {
            let idx = (self.next_random_u64() as usize) % candidates.len();
            return Some(candidates[idx].clone());
        }

        let mut best = candidates[0].clone();
        for candidate in candidates.iter().skip(1) {
            if Self::route_choice_better(candidate, &best) {
                best = candidate.clone();
            }
        }
        Some(best)
    }

    fn compute_routes(&mut self, ctx: &ProtocolContext) -> Vec<Route> {
        let graph = self.build_graph(ctx);
        let mut routes = Vec::new();

        for destination in graph.keys() {
            if *destination == ctx.router_id {
                continue;
            }
            let paths =
                k_shortest_simple_paths(&graph, ctx.router_id, *destination, self.params.k_paths);
            if paths.is_empty() {
                continue;
            }

            let mut by_next_hop: BTreeMap<u32, RouteChoice> = BTreeMap::new();
            for path in &paths {
                let Some(choice) = self.evaluate_path(path, ctx.now) else {
                    continue;
                };
                let keep = match by_next_hop.get(&choice.next_hop) {
                    None => true,
                    Some(existing) => Self::route_choice_better(&choice, existing),
                };
                if keep {
                    by_next_hop.insert(choice.next_hop, choice);
                }
            }
            if by_next_hop.is_empty() {
                continue;
            }

            let all_candidates: Vec<RouteChoice> = by_next_hop.values().cloned().collect();
            let deadline_candidates: Vec<RouteChoice> = all_candidates
                .iter()
                .filter(|choice| choice.completion_ms <= self.params.deadline_ms)
                .cloned()
                .collect();
            let base_candidates = if deadline_candidates.is_empty() {
                all_candidates
            } else {
                deadline_candidates
            };
            let low_pressure_candidates: Vec<RouteChoice> = base_candidates
                .iter()
                .filter(|choice| !self.is_high_pressure(choice))
                .cloned()
                .collect();
            let selection_pool = if low_pressure_candidates.is_empty() {
                &base_candidates
            } else {
                &low_pressure_candidates
            };

            let Some(chosen) = self.choose_route(selection_pool) else {
                continue;
            };
            routes.push(Route {
                destination: *destination,
                next_hop: chosen.next_hop,
                metric: chosen.completion_ms,
                protocol: self.name().to_string(),
            });
        }

        routes
    }
}

impl ProtocolEngine for DdrProtocol {
    fn name(&self) -> &'static str {
        self.protocol_name
    }

    fn start(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs {
        self.drive(ctx, true)
    }

    fn on_timer(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs {
        self.drive(ctx, false)
    }

    fn on_message(&mut self, ctx: &ProtocolContext, message: &ControlMessage) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();

        if message.kind == MessageKind::Hello {
            let queue_level = message
                .payload
                .get("queue_level")
                .and_then(Value::as_u64)
                .and_then(|value| usize::try_from(value).ok())
                .unwrap_or(0);
            let clamped = queue_level.min(self.params.queue_levels.saturating_sub(1));
            let patch = NeighborFastStatePatch {
                queue_level: Some(clamped),
                interface_utilization: Self::payload_unit_interval_f64(
                    &message.payload,
                    "interface_utilization",
                ),
                delay_ms: Self::payload_non_negative_f64(&message.payload, "delay_ms")
                    .or_else(|| Self::payload_non_negative_f64(&message.payload, "queue_delay_ms")),
                loss_rate: Self::payload_unit_interval_f64(&message.payload, "loss_rate")
                    .or_else(|| Self::payload_unit_interval_f64(&message.payload, "drop_rate")),
            };
            let changed =
                self.neighbor_state_db
                    .upsert_fast_state(message.src_router_id, patch, ctx.now);
            if changed {
                outputs.routes = Some(self.compute_routes(ctx));
            }
            return outputs;
        }
        if message.kind != MessageKind::DdrLsa {
            return outputs;
        }

        let origin = message
            .payload
            .get("origin_router_id")
            .and_then(Value::as_u64)
            .and_then(|v| u32::try_from(v).ok())
            .unwrap_or(message.src_router_id);
        let lsa_seq = message
            .payload
            .get("lsa_seq")
            .and_then(Value::as_i64)
            .unwrap_or(-1);
        let links = message
            .payload
            .get("links")
            .and_then(Value::as_array)
            .map_or_else(BTreeMap::new, |raw| {
                LinkStateControlPlane::parse_links(raw, true)
            });

        let changed = self
            .control_plane
            .upsert_lsa(origin, lsa_seq, links, ctx.now);
        if !changed {
            return outputs;
        }

        outputs.outbound.extend(self.flood_message(
            ctx,
            &message.payload,
            Some(message.src_router_id),
        ));
        outputs.routes = Some(self.compute_routes(ctx));
        outputs
    }

    fn metrics(&self) -> BTreeMap<String, Value> {
        let mut out = BTreeMap::new();

        let queue_delay_ms = self
            .estimated_queue_delay_ms
            .iter()
            .map(|(neighbor_id, value)| (neighbor_id.to_string(), json!(value)))
            .collect::<serde_json::Map<String, Value>>();
        let queue_depth_bytes = self
            .queue_depth_bytes
            .iter()
            .map(|(neighbor_id, value)| (neighbor_id.to_string(), json!(value)))
            .collect::<serde_json::Map<String, Value>>();
        let sample_source = self
            .queue_sample_source
            .iter()
            .map(|(neighbor_id, value)| (neighbor_id.to_string(), json!(value)))
            .collect::<serde_json::Map<String, Value>>();
        let queue_level_local = self
            .estimated_queue_delay_ms
            .keys()
            .map(|neighbor_id| {
                (
                    neighbor_id.to_string(),
                    json!(self.queue_level_for_neighbor(*neighbor_id)),
                )
            })
            .collect::<serde_json::Map<String, Value>>();
        let queue_level_neighbor = self
            .neighbor_state_db
            .queue_levels_snapshot()
            .iter()
            .map(|(neighbor_id, value)| (neighbor_id.to_string(), json!(value)))
            .collect::<serde_json::Map<String, Value>>();
        let neighbor_fast_state = self
            .neighbor_state_db
            .fast_state_snapshot()
            .iter()
            .map(|(neighbor_id, state)| {
                let mut fields = serde_json::Map::new();
                if let Some(value) = state.queue_level {
                    fields.insert("queue_level".to_string(), json!(value));
                }
                if let Some(value) = state.interface_utilization {
                    fields.insert("interface_utilization".to_string(), json!(value));
                }
                if let Some(value) = state.delay_ms {
                    fields.insert("delay_ms".to_string(), json!(value));
                }
                if let Some(value) = state.loss_rate {
                    fields.insert("loss_rate".to_string(), json!(value));
                }
                (neighbor_id.to_string(), Value::Object(fields))
            })
            .collect::<serde_json::Map<String, Value>>();

        out.insert("queue_delay_ms".to_string(), Value::Object(queue_delay_ms));
        out.insert(
            "queue_depth_bytes".to_string(),
            Value::Object(queue_depth_bytes),
        );
        out.insert("sample_source".to_string(), Value::Object(sample_source));
        out.insert(
            "queue_level_local".to_string(),
            Value::Object(queue_level_local),
        );
        out.insert(
            "queue_level_neighbor".to_string(),
            Value::Object(queue_level_neighbor),
        );
        out.insert(
            "neighbor_fast_state".to_string(),
            Value::Object(neighbor_fast_state),
        );
        out.insert("k_paths".to_string(), json!(self.params.k_paths));
        out.insert("deadline_ms".to_string(), json!(self.params.deadline_ms));
        out.insert("queue_levels".to_string(), json!(self.params.queue_levels));
        out.insert(
            "pressure_threshold".to_string(),
            json!(self.params.pressure_threshold),
        );
        out.insert(
            "queue_level_scale_ms".to_string(),
            json!(self.params.queue_level_scale_ms),
        );
        out.insert(
            "randomize_route_selection".to_string(),
            json!(self.params.randomize_route_selection),
        );
        out.insert("rng_seed".to_string(), json!(self.params.rng_seed));
        out.insert(
            "effective_bandwidth_bps".to_string(),
            json!(self.effective_bandwidth_bps()),
        );
        out.insert(
            "queue_sample_interval_s".to_string(),
            json!(self.params.timers.queue_sample_interval),
        );
        out.insert(
            "neighbor_state_max_age_s".to_string(),
            json!(self.neighbor_state_max_age_s()),
        );
        out.insert(
            "neighbor_state_max_age_configured_s".to_string(),
            json!(self.params.neighbor_state_max_age_s),
        );
        out
    }
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;
    use crate::protocols::base::RouterLink;

    #[test]
    fn ddr_start_installs_direct_route() {
        let mut ddr = DdrProtocol::new(DdrParams::default());
        let mut links = BTreeMap::new();
        links.insert(
            2,
            RouterLink {
                neighbor_id: 2,
                cost: 1.0,
                address: "10.0.12.2".to_string(),
                port: 5500,
                interface_name: None,
                is_up: true,
            },
        );
        let ctx = ProtocolContext {
            router_id: 1,
            now: 0.0,
            links,
        };

        let outputs = ddr.start(&ctx);
        let routes = outputs.routes.expect("start should output routes");
        assert!(routes.iter().any(|route| {
            route.destination == 2 && route.next_hop == 2 && route.protocol == "ddr"
        }));
    }

    #[test]
    fn ddr_prefers_meeting_deadline() {
        let mut ddr = DdrProtocol::new(DdrParams {
            deadline_ms: 80.0,
            flow_size_bytes: 1200.0,
            link_bandwidth_bps: 9_600_000.0,
            ..DdrParams::default()
        });
        ddr.estimated_queue_delay_ms.insert(2, 100.0);
        ddr.estimated_queue_delay_ms.insert(3, 1.0);

        let mut links = BTreeMap::new();
        links.insert(
            2,
            RouterLink {
                neighbor_id: 2,
                cost: 1.0,
                address: "10.0.12.2".to_string(),
                port: 5500,
                interface_name: None,
                is_up: true,
            },
        );
        links.insert(
            3,
            RouterLink {
                neighbor_id: 3,
                cost: 2.0,
                address: "10.0.13.3".to_string(),
                port: 5500,
                interface_name: None,
                is_up: true,
            },
        );
        let ctx = ProtocolContext {
            router_id: 1,
            now: 0.0,
            links,
        };

        ddr.control_plane
            .upsert_lsa(1, 1, BTreeMap::from([(2_u32, 1.0), (3_u32, 2.0)]), 0.0);
        ddr.control_plane
            .upsert_lsa(2, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);
        ddr.control_plane
            .upsert_lsa(3, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);

        let routes = ddr.compute_routes(&ctx);
        let to_4 = routes
            .iter()
            .find(|route| route.destination == 4)
            .expect("route to 4 should exist");
        assert_eq!(to_4.next_hop, 3);
    }

    #[test]
    fn parse_kernel_backlog_from_tc_output() {
        let text = "\
qdisc fq_codel 0: dev eth1 root refcnt 2 limit 10240p\n\
 Sent 1234 bytes 12 pkt (dropped 0, overlimits 0 requeues 0)\n\
 backlog 0b 7p requeues 0\n";
        let parsed = DdrProtocol::parse_kernel_backlog(text).expect("backlog parsed");
        assert_eq!(parsed.bytes, Some(0.0));
        assert_eq!(parsed.packets, Some(7.0));
    }

    #[test]
    fn parse_kernel_backlog_with_bytes_only() {
        let text = "\
qdisc fq_codel 0: dev eth1 root refcnt 2 limit 10240p\n\
 backlog 8192b 0p requeues 0\n";
        let parsed = DdrProtocol::parse_kernel_backlog(text).expect("backlog parsed");
        assert_eq!(parsed.bytes, Some(8192.0));
        assert_eq!(parsed.packets, Some(0.0));
    }

    #[test]
    fn ddr_metrics_contains_queue_maps() {
        let mut ddr = DdrProtocol::new(DdrParams::default());
        ddr.queue_depth_bytes.insert(2, 6000.0);
        ddr.estimated_queue_delay_ms.insert(2, 2.5);
        ddr.queue_sample_source.insert(2, "kernel_tc".to_string());
        ddr.neighbor_state_db.upsert_fast_state(
            2,
            NeighborFastStatePatch {
                queue_level: Some(1),
                interface_utilization: Some(0.4),
                ..NeighborFastStatePatch::default()
            },
            0.0,
        );
        let metrics = ddr.metrics();
        assert!(metrics.contains_key("queue_delay_ms"));
        assert!(metrics.contains_key("queue_depth_bytes"));
        assert!(metrics.contains_key("sample_source"));
        assert!(metrics.contains_key("queue_level_local"));
        assert!(metrics.contains_key("queue_level_neighbor"));
        assert!(metrics.contains_key("neighbor_fast_state"));
    }

    #[test]
    fn hello_updates_neighbor_queue_level() {
        let mut ddr = DdrProtocol::new(DdrParams::default());
        let mut links = BTreeMap::new();
        links.insert(
            2,
            RouterLink {
                neighbor_id: 2,
                cost: 1.0,
                address: "10.0.12.2".to_string(),
                port: 5500,
                interface_name: None,
                is_up: true,
            },
        );
        let ctx = ProtocolContext {
            router_id: 1,
            now: 0.0,
            links,
        };
        let mut payload = BTreeMap::new();
        payload.insert("queue_level".to_string(), json!(3));
        payload.insert("interface_utilization".to_string(), json!(0.8));
        payload.insert("delay_ms".to_string(), json!(3.2));
        payload.insert("loss_rate".to_string(), json!(0.02));
        let msg = ControlMessage {
            protocol: "ddr".to_string(),
            kind: MessageKind::Hello,
            src_router_id: 2,
            seq: 1,
            payload,
            ts: 0.0,
        };
        ddr.on_message(&ctx, &msg);
        assert_eq!(
            ddr.neighbor_state_db
                .get_queue_level_fresh(2, 0.0, ddr.neighbor_state_max_age_s()),
            Some(3)
        );
        let state = ddr
            .neighbor_state_db
            .get_state_fresh(2, 0.0, ddr.neighbor_state_max_age_s())
            .expect("neighbor state should be present");
        assert_eq!(state.interface_utilization, Some(0.8));
        assert_eq!(state.delay_ms, Some(3.2));
        assert_eq!(state.loss_rate, Some(0.02));
    }

    #[test]
    fn ddr_filters_high_pressure_neighbor_reports() {
        let mut ddr = DdrProtocol::new(DdrParams {
            pressure_threshold: 0,
            randomize_route_selection: false,
            flow_size_bytes: 1200.0,
            link_bandwidth_bps: 9_600_000.0,
            ..DdrParams::default()
        });
        ddr.neighbor_state_db.upsert_queue_level(2, 3, 0.0);
        ddr.neighbor_state_db.upsert_queue_level(3, 0, 0.0);

        let mut links = BTreeMap::new();
        links.insert(
            2,
            RouterLink {
                neighbor_id: 2,
                cost: 1.0,
                address: "10.0.12.2".to_string(),
                port: 5500,
                interface_name: None,
                is_up: true,
            },
        );
        links.insert(
            3,
            RouterLink {
                neighbor_id: 3,
                cost: 1.0,
                address: "10.0.13.3".to_string(),
                port: 5500,
                interface_name: None,
                is_up: true,
            },
        );
        let ctx = ProtocolContext {
            router_id: 1,
            now: 0.0,
            links,
        };
        ddr.control_plane
            .upsert_lsa(1, 1, BTreeMap::from([(2_u32, 1.0), (3_u32, 1.0)]), 0.0);
        ddr.control_plane
            .upsert_lsa(2, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);
        ddr.control_plane
            .upsert_lsa(3, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);

        let routes = ddr.compute_routes(&ctx);
        let to_4 = routes
            .iter()
            .find(|route| route.destination == 4)
            .expect("route to 4 should exist");
        assert_eq!(to_4.next_hop, 3);
    }

    #[test]
    fn stale_neighbor_queue_report_expires() {
        let mut ddr = DdrProtocol::new(DdrParams::default());
        assert!(ddr.neighbor_state_db.upsert_queue_level(2, 3, 0.0));
        assert_eq!(
            ddr.neighbor_state_db.get_queue_level_fresh(2, 5.0, 1.0),
            None
        );
        assert!(ddr.neighbor_state_db.age_out(5.0, 1.0));
    }
}
