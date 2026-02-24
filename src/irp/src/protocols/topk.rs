use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};

use serde_json::{json, Value};

use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::Route;
use crate::protocols::base::{Ipv4RoutingProtocol, ProtocolContext, ProtocolOutputs};
use crate::protocols::link_state::{LinkStateControlPlane, LinkStateTimers};
use crate::protocols::route_compute::k_shortest_simple_paths;

#[derive(Debug, Clone)]
pub struct TopkTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
}

impl Default for TopkTimers {
    fn default() -> Self {
        Self {
            hello_interval: 1.0,
            lsa_interval: 3.0,
            lsa_max_age: 15.0,
        }
    }
}

#[derive(Debug, Clone)]
pub struct TopkParams {
    pub timers: TopkTimers,
    pub k_paths: usize,
    pub explore_probability: f64,
    pub selection_hold_time_s: f64,
    pub rng_seed: u64,
}

impl Default for TopkParams {
    fn default() -> Self {
        Self {
            timers: TopkTimers::default(),
            k_paths: 3,
            explore_probability: 0.3,
            selection_hold_time_s: 3.0,
            rng_seed: 1,
        }
    }
}

#[derive(Debug, Clone)]
struct NextHopCandidate {
    next_hop: u32,
    metric: f64,
    hop_count: usize,
}

#[derive(Debug, Clone)]
struct RouteSelection {
    next_hop: u32,
    selected_at: f64,
}

pub struct TopkProtocol {
    params: TopkParams,
    control_plane: LinkStateControlPlane,
    rng_state: u64,
    selections: BTreeMap<u32, RouteSelection>,
}

impl TopkProtocol {
    pub fn new(params: TopkParams) -> Self {
        let rng_seed = params.rng_seed.max(1);
        Self {
            params,
            control_plane: LinkStateControlPlane::new(),
            rng_state: rng_seed,
            selections: BTreeMap::new(),
        }
    }

    fn drive(&mut self, ctx: &ProtocolContext, force_lsa: bool) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
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
            outputs.outbound.extend(self.flood_lsa(ctx, payload, None));
        }

        if tick.topology_changed {
            outputs.routes = Some(self.compute_routes(ctx));
        }

        outputs
    }

    fn send_hello(&mut self, ctx: &ProtocolContext) -> Vec<(u32, ControlMessage)> {
        self.control_plane
            .send_hello(self.name(), ctx, |_neighbor_id| {
                let mut payload = BTreeMap::new();
                payload.insert("router_id".to_string(), json!(ctx.router_id));
                payload
            })
    }

    fn flood_lsa(
        &mut self,
        ctx: &ProtocolContext,
        payload: &BTreeMap<String, Value>,
        exclude: Option<u32>,
    ) -> Vec<(u32, ControlMessage)> {
        self.control_plane
            .flood_lsa(self.name(), MessageKind::OspfLsa, ctx, payload, exclude)
    }

    fn build_graph(&self, ctx: &ProtocolContext) -> BTreeMap<u32, BTreeMap<u32, f64>> {
        self.control_plane.build_graph(ctx.router_id, true)
    }

    fn next_random_u64(&mut self) -> u64 {
        self.rng_state = self
            .rng_state
            .wrapping_mul(6_364_136_223_846_793_005)
            .wrapping_add(1);
        self.rng_state
    }

    fn next_random_unit(&mut self) -> f64 {
        let raw = self.next_random_u64() >> 11;
        (raw as f64) / ((1_u64 << 53) as f64)
    }

    fn next_random_index(&mut self, bound: usize) -> usize {
        if bound <= 1 {
            return 0;
        }
        ((self.next_random_u64() >> 32) as usize) % bound
    }

    fn choose_candidate(
        &mut self,
        destination: u32,
        now: f64,
        candidates: &[NextHopCandidate],
    ) -> NextHopCandidate {
        let hold = self.params.selection_hold_time_s.max(0.0);
        if let Some(prev) = self.selections.get(&destination) {
            if (now - prev.selected_at) < hold {
                if let Some(candidate) = candidates.iter().find(|c| c.next_hop == prev.next_hop) {
                    return candidate.clone();
                }
            }
        }

        if candidates.len() == 1 {
            return candidates[0].clone();
        }

        let explore_p = self.params.explore_probability.clamp(0.0, 1.0);
        if self.next_random_unit() < explore_p {
            let idx = self.next_random_index(candidates.len());
            return candidates[idx].clone();
        }

        candidates[0].clone()
    }

    fn compute_routes(&mut self, ctx: &ProtocolContext) -> Vec<Route> {
        let graph = self.build_graph(ctx);
        let mut routes = Vec::new();
        let mut active_destinations = BTreeSet::new();

        for destination in graph.keys() {
            if *destination == ctx.router_id {
                continue;
            }
            let paths =
                k_shortest_simple_paths(&graph, ctx.router_id, *destination, self.params.k_paths);
            if paths.is_empty() {
                continue;
            }

            let mut per_hop_best: BTreeMap<u32, NextHopCandidate> = BTreeMap::new();
            for path in paths {
                if path.nodes.len() < 2 {
                    continue;
                }
                let candidate = NextHopCandidate {
                    next_hop: path.nodes[1],
                    metric: path.cost,
                    hop_count: path.nodes.len() - 1,
                };
                let keep = match per_hop_best.get(&candidate.next_hop) {
                    None => true,
                    Some(current) => {
                        candidate.metric < current.metric
                            || (candidate.metric == current.metric
                                && candidate.hop_count < current.hop_count)
                    }
                };
                if keep {
                    per_hop_best.insert(candidate.next_hop, candidate);
                }
            }

            if per_hop_best.is_empty() {
                continue;
            }

            let mut candidates: Vec<NextHopCandidate> = per_hop_best.into_values().collect();
            candidates.sort_by(|a, b| match a.metric.partial_cmp(&b.metric) {
                Some(Ordering::Less) => Ordering::Less,
                Some(Ordering::Greater) => Ordering::Greater,
                _ => {
                    if a.next_hop != b.next_hop {
                        a.next_hop.cmp(&b.next_hop)
                    } else {
                        a.hop_count.cmp(&b.hop_count)
                    }
                }
            });

            let chosen = self.choose_candidate(*destination, ctx.now, &candidates);
            self.selections.insert(
                *destination,
                RouteSelection {
                    next_hop: chosen.next_hop,
                    selected_at: ctx.now,
                },
            );
            active_destinations.insert(*destination);

            routes.push(Route::new_host(
                *destination,
                chosen.next_hop,
                chosen.metric,
                self.name(),
            ));
        }

        self.selections
            .retain(|destination, _| active_destinations.contains(destination));
        routes
    }
}

impl Ipv4RoutingProtocol for TopkProtocol {
    fn name(&self) -> &'static str {
        "topk"
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
            return outputs;
        }
        if message.kind != MessageKind::OspfLsa {
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

        outputs
            .outbound
            .extend(self.flood_lsa(ctx, &message.payload, Some(message.src_router_id)));
        outputs.routes = Some(self.compute_routes(ctx));
        outputs
    }

    fn metrics(&self) -> BTreeMap<String, Value> {
        let mut out = BTreeMap::new();
        out.insert("k_paths".to_string(), json!(self.params.k_paths));
        out.insert(
            "explore_probability".to_string(),
            json!(self.params.explore_probability),
        );
        out.insert(
            "selection_hold_time_s".to_string(),
            json!(self.params.selection_hold_time_s),
        );
        out.insert("rng_seed".to_string(), json!(self.params.rng_seed));
        out
    }
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;
    use crate::protocols::base::RouterLink;

    #[test]
    fn topk_start_installs_direct_route() {
        let mut topk = TopkProtocol::new(TopkParams::default());
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

        let outputs = topk.start(&ctx);
        let routes = outputs.routes.expect("start should output routes");
        assert!(routes.iter().any(|route| {
            route.destination == 2 && route.next_hop == 2 && route.protocol == "topk"
        }));
    }

    #[test]
    fn topk_can_explore_non_shortest_path() {
        let mut topk = TopkProtocol::new(TopkParams {
            k_paths: 2,
            explore_probability: 1.0,
            selection_hold_time_s: 0.0,
            rng_seed: 2,
            ..TopkParams::default()
        });
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

        topk.control_plane
            .upsert_lsa(1, 1, BTreeMap::from([(2_u32, 1.0), (3_u32, 1.0)]), 0.0);
        topk.control_plane
            .upsert_lsa(2, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);
        topk.control_plane
            .upsert_lsa(3, 1, BTreeMap::from([(4_u32, 2.0)]), 0.0);

        let mut seen = BTreeSet::new();
        for idx in 0..16 {
            let probe_ctx = ProtocolContext {
                router_id: ctx.router_id,
                now: idx as f64,
                links: ctx.links.clone(),
            };
            let routes = topk.compute_routes(&probe_ctx);
            let to_4 = routes
                .iter()
                .find(|route| route.destination == 4)
                .expect("route to 4 should exist");
            seen.insert(to_4.next_hop);
        }
        assert!(seen.contains(&2));
        assert!(seen.contains(&3));
    }
}
