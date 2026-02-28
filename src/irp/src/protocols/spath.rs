use std::collections::BTreeMap;

use serde_json::{json, Value};

use crate::model::control_plane::ExchangeScope;
use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::Route;
use crate::protocols::base::{Ipv4RoutingProtocol, ProtocolContext, ProtocolOutputs};
use crate::protocols::link_state::{LinkStateControlPlane, LinkStateTimers};
use crate::protocols::route_compute::{
    compute_scalar_route_entries, NextHopSelectionPolicy, ScalarRouteAlgorithm,
    ScalarRouteStrategyConfig,
};

#[derive(Debug, Clone)]
pub struct SPathTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
    pub lsa_min_trigger_spacing_s: f64,
}

impl Default for SPathTimers {
    fn default() -> Self {
        Self {
            hello_interval: 1.0,
            lsa_interval: 3.0,
            lsa_max_age: 15.0,
            lsa_min_trigger_spacing_s: 0.0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SPathAlgorithm {
    Dijkstra,
    Ecmp,
    BellmanFord,
    YenKShortest,
}

impl SPathAlgorithm {
    pub fn from_str(raw: &str) -> Self {
        match raw.trim().to_ascii_lowercase().as_str() {
            "ecmp" => Self::Ecmp,
            "bellman_ford" | "bellman-ford" | "bf" => Self::BellmanFord,
            "yen" | "yen_ksp" | "yen-ksp" | "ksp" | "yen_k_shortest" => Self::YenKShortest,
            _ => Self::Dijkstra,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Dijkstra => "dijkstra",
            Self::Ecmp => "ecmp",
            Self::BellmanFord => "bellman_ford",
            Self::YenKShortest => "yen_ksp",
        }
    }

    fn scalar_strategy(self, k_paths: usize) -> ScalarRouteAlgorithm {
        match self {
            Self::Dijkstra => ScalarRouteAlgorithm::Dijkstra,
            Self::Ecmp => ScalarRouteAlgorithm::Ecmp,
            Self::BellmanFord => ScalarRouteAlgorithm::BellmanFord,
            Self::YenKShortest => ScalarRouteAlgorithm::YenKShortest {
                k_paths: k_paths.max(1),
            },
        }
    }

    fn requires_nonnegative_links(self) -> bool {
        !matches!(self, Self::BellmanFord)
    }
}

#[derive(Debug, Clone)]
pub struct SPathParams {
    pub timers: SPathTimers,
    pub algorithm: SPathAlgorithm,
    pub k_paths: usize,
    pub hash_seed: u64,
}

impl Default for SPathParams {
    fn default() -> Self {
        Self {
            timers: SPathTimers::default(),
            algorithm: SPathAlgorithm::Dijkstra,
            k_paths: 3,
            hash_seed: 1,
        }
    }
}

pub struct SPathProtocol {
    params: SPathParams,
    control_plane: LinkStateControlPlane,
}

impl SPathProtocol {
    pub fn new(params: SPathParams) -> Self {
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
        let graph = self
            .control_plane
            .build_graph(src, self.params.algorithm.requires_nonnegative_links());
        let strategy = ScalarRouteStrategyConfig {
            algorithm: self.params.algorithm.scalar_strategy(self.params.k_paths),
            selection: NextHopSelectionPolicy::Hash {
                seed: self.params.hash_seed,
            },
        };

        compute_scalar_route_entries(&graph, src, &strategy)
            .into_iter()
            .map(|entry| {
                Route::new_host(
                    entry.destination,
                    entry.selected_next_hop,
                    entry.metric,
                    self.name(),
                )
            })
            .collect()
    }

    fn parse_links(&self, raw_links: &[Value]) -> BTreeMap<u32, f64> {
        LinkStateControlPlane::parse_links(
            raw_links,
            self.params.algorithm.requires_nonnegative_links(),
        )
    }
}

impl Ipv4RoutingProtocol for SPathProtocol {
    fn name(&self) -> &'static str {
        "spath"
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
            .map_or_else(BTreeMap::new, |raw| self.parse_links(raw));

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
        out.insert(
            "algorithm".to_string(),
            json!(self.params.algorithm.as_str()),
        );
        out.insert("k_paths".to_string(), json!(self.params.k_paths));
        out.insert("hash_seed".to_string(), json!(self.params.hash_seed));
        out.insert(
            "hello_interval_s".to_string(),
            json!(self.params.timers.hello_interval),
        );
        out.insert(
            "lsa_interval_s".to_string(),
            json!(self.params.timers.lsa_interval),
        );
        out.insert(
            "lsa_min_trigger_spacing_s".to_string(),
            json!(self.params.timers.lsa_min_trigger_spacing_s),
        );
        out.insert(
            "lsa_max_age_s".to_string(),
            json!(self.params.timers.lsa_max_age),
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
    fn spath_start_installs_direct_route() {
        let mut spath = SPathProtocol::new(SPathParams::default());
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
            qdisc_by_neighbor: BTreeMap::new(),
        };

        let outputs = spath.start(&ctx);
        let routes = outputs.routes.expect("start should output routes");
        assert!(routes.iter().any(|route| {
            route.destination == 2 && route.next_hop == 2 && route.protocol == "spath"
        }));
    }

    #[test]
    fn spath_supports_ecmp_strategy() {
        let mut spath = SPathProtocol::new(SPathParams {
            algorithm: SPathAlgorithm::Ecmp,
            hash_seed: 2026,
            ..SPathParams::default()
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
            qdisc_by_neighbor: BTreeMap::new(),
        };

        spath
            .control_plane
            .upsert_lsa(1, 1, BTreeMap::from([(2, 1.0), (3, 1.0)]), 0.0);
        spath
            .control_plane
            .upsert_lsa(2, 1, BTreeMap::from([(4, 1.0)]), 0.0);
        spath
            .control_plane
            .upsert_lsa(3, 1, BTreeMap::from([(4, 1.0)]), 0.0);

        let routes = spath.compute_routes(&ctx);
        let to_4 = routes
            .iter()
            .find(|route| route.destination == 4)
            .expect("route to 4");
        assert!(to_4.next_hop == 2 || to_4.next_hop == 3);
        assert_eq!(to_4.protocol, "spath");
    }
}
