use std::collections::BTreeMap;

use serde_json::{json, Value};

use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::Route;
use crate::protocols::base::{ProtocolContext, ProtocolEngine, ProtocolOutputs};
use crate::protocols::link_state::{LinkStateControlPlane, LinkStateTimers};
use crate::protocols::route_compute::compute_spf_single;

#[derive(Debug, Clone)]
pub struct OspfTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
}

impl Default for OspfTimers {
    fn default() -> Self {
        Self {
            hello_interval: 1.0,
            lsa_interval: 3.0,
            lsa_max_age: 15.0,
        }
    }
}

pub struct OspfProtocol {
    timers: OspfTimers,
    control_plane: LinkStateControlPlane,
}

impl OspfProtocol {
    pub fn new(timers: OspfTimers) -> Self {
        Self {
            timers,
            control_plane: LinkStateControlPlane::new(),
        }
    }

    fn drive(&mut self, ctx: &ProtocolContext, force_lsa: bool) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
        let tick = self.control_plane.tick(
            ctx,
            LinkStateTimers {
                hello_interval: self.timers.hello_interval,
                lsa_interval: self.timers.lsa_interval,
                lsa_max_age: self.timers.lsa_max_age,
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
        let result = compute_spf_single(&graph, src);
        let dist = result.dist;
        let first_hop = result.first_hop;

        dist.into_iter()
            .filter_map(|(destination, metric)| {
                if destination == src {
                    return None;
                }
                let next_hop = first_hop.get(&destination).copied()?;
                Some(Route {
                    destination,
                    next_hop,
                    metric,
                    protocol: self.name().to_string(),
                })
            })
            .collect()
    }
}

impl ProtocolEngine for OspfProtocol {
    fn name(&self) -> &'static str {
        "ospf"
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
        out.insert(
            "hello_interval_s".to_string(),
            json!(self.timers.hello_interval),
        );
        out.insert(
            "lsa_interval_s".to_string(),
            json!(self.timers.lsa_interval),
        );
        out.insert("lsa_max_age_s".to_string(), json!(self.timers.lsa_max_age));
        out
    }
}
