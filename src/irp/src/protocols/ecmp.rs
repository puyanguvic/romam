use std::collections::{BTreeMap, BTreeSet};

use serde_json::{json, Value};

use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::Route;
use crate::protocols::base::{ProtocolContext, ProtocolEngine, ProtocolOutputs};
use crate::protocols::link_state::{LinkStateControlPlane, LinkStateTimers};
use crate::protocols::route_compute::compute_spf_ecmp;

#[derive(Debug, Clone)]
pub struct EcmpTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
}

impl Default for EcmpTimers {
    fn default() -> Self {
        Self {
            hello_interval: 1.0,
            lsa_interval: 3.0,
            lsa_max_age: 15.0,
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
                Some(Route {
                    destination,
                    next_hop,
                    metric,
                    protocol: self.name().to_string(),
                })
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

impl ProtocolEngine for EcmpProtocol {
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
}
