use std::collections::{BTreeMap, BTreeSet};

use serde_json::{json, Value};

use crate::model::control_plane::ExchangeScope;
use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::{Ipv4RoutingTableEntry, Route, RoutingTableEntry};
use crate::protocols::base::{Ipv4RoutingProtocol, ProtocolContext, ProtocolOutputs};
use crate::protocols::link_state::{LinkStateControlPlane, LinkStateTimers};
use crate::protocols::route_compute::compute_spf_ecmp;

#[derive(Debug, Clone)]
pub struct EcmpTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
    pub lsa_min_trigger_spacing_s: f64,
}

impl Default for EcmpTimers {
    fn default() -> Self {
        Self {
            hello_interval: 1.0,
            lsa_interval: 3.0,
            lsa_max_age: 15.0,
            lsa_min_trigger_spacing_s: 0.0,
        }
    }
}

#[derive(Debug, Clone)]
pub struct EcmpParams {
    pub timers: EcmpTimers,
    pub hash_seed: u64,
}

impl Default for EcmpParams {
    fn default() -> Self {
        Self {
            timers: EcmpTimers::default(),
            hash_seed: 1,
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct EcmpRoutingTableEntry {
    base: Ipv4RoutingTableEntry,
    route_metric: f64,
    next_hops: BTreeSet<u32>,
    selected_next_hop: u32,
    changed: bool,
}

impl RoutingTableEntry for EcmpRoutingTableEntry {
    fn base(&self) -> &Ipv4RoutingTableEntry {
        &self.base
    }
}

impl EcmpRoutingTableEntry {
    pub fn new_host_route_to(
        destination: u32,
        selected_next_hop: u32,
        mut next_hops: BTreeSet<u32>,
        interface: Option<u32>,
    ) -> Self {
        next_hops.insert(selected_next_hop);
        Self {
            base: Ipv4RoutingTableEntry::create_host_route_to(
                destination,
                selected_next_hop,
                interface,
            ),
            route_metric: 0.0,
            next_hops,
            selected_next_hop,
            changed: false,
        }
    }

    pub fn set_route_metric(&mut self, route_metric: f64) {
        if self.route_metric.to_bits() != route_metric.to_bits() {
            self.route_metric = route_metric;
            self.changed = true;
        }
    }

    pub fn get_route_metric(&self) -> f64 {
        self.route_metric
    }

    pub fn next_hops(&self) -> &BTreeSet<u32> {
        &self.next_hops
    }

    pub fn selected_next_hop(&self) -> u32 {
        self.selected_next_hop
    }

    pub fn set_route_changed(&mut self, changed: bool) {
        self.changed = changed;
    }

    pub fn is_route_changed(&self) -> bool {
        self.changed
    }

    fn to_route(&self, protocol: &str) -> Option<Route> {
        let next_hop = self.next_hop()?;
        Some(Route::new_host(
            self.destination(),
            next_hop,
            self.route_metric,
            protocol,
        ))
    }
}

pub struct EcmpProtocol {
    params: EcmpParams,
    control_plane: LinkStateControlPlane,
}

impl EcmpProtocol {
    pub fn new(params: EcmpParams) -> Self {
        Self {
            params,
            control_plane: LinkStateControlPlane::new(),
        }
    }

    pub fn set_descriptor_scopes(&mut self, hello_scope: ExchangeScope, lsa_scope: ExchangeScope) {
        self.control_plane
            .set_descriptor_scopes(hello_scope, lsa_scope);
    }

    fn drive(&mut self, ctx: &ProtocolContext, force_lsa: bool) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
        let tick = self.control_plane.tick(
            ctx,
            LinkStateTimers {
                hello_interval: self.params.timers.hello_interval,
                lsa_interval: self.params.timers.lsa_interval,
                lsa_max_age: self.params.timers.lsa_max_age,
                lsa_min_trigger_spacing_s: self.params.timers.lsa_min_trigger_spacing_s,
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

    fn compute_routes(&self, ctx: &ProtocolContext) -> Vec<Route> {
        let src = ctx.router_id;
        let graph = self.control_plane.build_graph(src, false);
        let result = compute_spf_ecmp(&graph, src);
        let dist = result.dist;
        let first_hops = result.first_hops;

        dist.into_iter()
            .filter_map(|(destination, metric)| {
                if destination == src {
                    return None;
                }
                let hops = first_hops.get(&destination)?;
                let next_hop = self.pick_next_hop(src, destination, hops)?;
                let mut entry = EcmpRoutingTableEntry::new_host_route_to(
                    destination,
                    next_hop,
                    hops.clone(),
                    None,
                );
                entry.set_route_metric(metric);
                entry.set_route_changed(false);
                entry.to_route(self.name())
            })
            .collect()
    }

    fn pick_next_hop(&self, src: u32, destination: u32, hops: &BTreeSet<u32>) -> Option<u32> {
        if hops.is_empty() {
            return None;
        }
        let candidates: Vec<u32> = hops.iter().copied().collect();
        let idx = (self.hash_mix(src, destination) as usize) % candidates.len();
        candidates.get(idx).copied()
    }

    fn hash_mix(&self, src: u32, destination: u32) -> u64 {
        let mut x = self.params.hash_seed;
        x ^= (src as u64).wrapping_mul(0x9E37_79B1_85EB_CA87);
        x = x.rotate_left(13);
        x ^= (destination as u64).wrapping_mul(0xC2B2_AE3D_27D4_EB4F);
        x ^= x >> 33;
        x = x.wrapping_mul(0xFF51_AFD7_ED55_8CCD);
        x ^= x >> 33;
        x
    }
}

impl Ipv4RoutingProtocol for EcmpProtocol {
    fn name(&self) -> &'static str {
        "ecmp"
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
                LinkStateControlPlane::parse_links(raw, false)
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
        out.insert("hash_seed".to_string(), json!(self.params.hash_seed));
        out.insert(
            "lsa_min_trigger_spacing_s".to_string(),
            json!(self.params.timers.lsa_min_trigger_spacing_s),
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
    fn ecmp_start_installs_direct_route() {
        let mut ecmp = EcmpProtocol::new(EcmpParams::default());
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

        let outputs = ecmp.start(&ctx);
        let routes = outputs.routes.expect("start should output routes");
        assert!(routes.iter().any(|route| {
            route.destination == 2 && route.next_hop == 2 && route.protocol == "ecmp"
        }));
    }

    #[test]
    fn ecmp_chooses_equal_cost_path_deterministically() {
        let mut ecmp = EcmpProtocol::new(EcmpParams {
            hash_seed: 2026,
            ..EcmpParams::default()
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

        ecmp.control_plane
            .upsert_lsa(1, 1, BTreeMap::from([(2_u32, 1.0), (3_u32, 1.0)]), 0.0);
        ecmp.control_plane
            .upsert_lsa(2, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);
        ecmp.control_plane
            .upsert_lsa(3, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);

        let routes_a = ecmp.compute_routes(&ctx);
        let routes_b = ecmp.compute_routes(&ctx);
        let to_4_a = routes_a
            .iter()
            .find(|route| route.destination == 4)
            .expect("route to 4 should exist");
        let to_4_b = routes_b
            .iter()
            .find(|route| route.destination == 4)
            .expect("route to 4 should exist");
        assert_eq!(to_4_a.next_hop, to_4_b.next_hop);
        assert!(to_4_a.next_hop == 2 || to_4_a.next_hop == 3);
        assert_eq!(to_4_a.metric, 2.0);
    }

    #[test]
    fn ecmp_entry_keeps_all_equal_cost_next_hops() {
        let mut next_hops = BTreeSet::new();
        next_hops.insert(2);
        next_hops.insert(3);

        let mut entry = EcmpRoutingTableEntry::new_host_route_to(4, 2, next_hops, None);
        entry.set_route_metric(2.0);

        assert_eq!(entry.destination(), 4);
        assert_eq!(entry.next_hop(), Some(2));
        assert_eq!(entry.selected_next_hop(), 2);
        assert_eq!(entry.get_route_metric(), 2.0);
        assert_eq!(entry.next_hops().len(), 2);
        assert!(entry.next_hops().contains(&2));
        assert!(entry.next_hops().contains(&3));
    }
}
