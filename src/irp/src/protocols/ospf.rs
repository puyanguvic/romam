use std::collections::BTreeMap;

use serde_json::{json, Value};

use crate::model::control_plane::ExchangeScope;
use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::{Ipv4RoutingTableEntry, Route, RoutingTableEntry};
use crate::protocols::base::{Ipv4RoutingProtocol, ProtocolContext, ProtocolOutputs};
use crate::protocols::link_state::{LinkStateControlPlane, LinkStateTimers};
use crate::protocols::route_compute::compute_spf_single;

#[derive(Debug, Clone)]
pub struct OspfTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
    pub lsa_min_trigger_spacing_s: f64,
}

impl Default for OspfTimers {
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
pub enum OspfRouteType {
    IntraArea,
}

#[derive(Debug, Clone, PartialEq)]
pub struct OspfRoutingTableEntry {
    base: Ipv4RoutingTableEntry,
    route_metric: f64,
    area_id: u32,
    route_type: OspfRouteType,
    changed: bool,
}

impl RoutingTableEntry for OspfRoutingTableEntry {
    fn base(&self) -> &Ipv4RoutingTableEntry {
        &self.base
    }
}

impl OspfRoutingTableEntry {
    pub fn new_host_route_to(destination: u32, next_hop: u32, interface: Option<u32>) -> Self {
        Self {
            base: Ipv4RoutingTableEntry::create_host_route_to(destination, next_hop, interface),
            route_metric: 0.0,
            area_id: 0,
            route_type: OspfRouteType::IntraArea,
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

    pub fn set_area_id(&mut self, area_id: u32) {
        if self.area_id != area_id {
            self.area_id = area_id;
            self.changed = true;
        }
    }

    pub fn get_area_id(&self) -> u32 {
        self.area_id
    }

    pub fn set_route_type(&mut self, route_type: OspfRouteType) {
        if self.route_type != route_type {
            self.route_type = route_type;
            self.changed = true;
        }
    }

    pub fn get_route_type(&self) -> OspfRouteType {
        self.route_type
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

    pub fn set_descriptor_scopes(&mut self, hello_scope: ExchangeScope, lsa_scope: ExchangeScope) {
        self.control_plane
            .set_descriptor_scopes(hello_scope, lsa_scope);
    }

    fn drive(&mut self, ctx: &ProtocolContext, force_lsa: bool) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
        let tick = self.control_plane.tick(
            ctx,
            LinkStateTimers {
                hello_interval: self.timers.hello_interval,
                lsa_interval: self.timers.lsa_interval,
                lsa_max_age: self.timers.lsa_max_age,
                lsa_min_trigger_spacing_s: self.timers.lsa_min_trigger_spacing_s,
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
                let mut entry =
                    OspfRoutingTableEntry::new_host_route_to(destination, next_hop, None);
                entry.set_area_id(0);
                entry.set_route_type(OspfRouteType::IntraArea);
                entry.set_route_metric(metric);
                entry.set_route_changed(false);
                entry.to_route(self.name())
            })
            .collect()
    }
}

impl Ipv4RoutingProtocol for OspfProtocol {
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
        out.insert(
            "lsa_min_trigger_spacing_s".to_string(),
            json!(self.timers.lsa_min_trigger_spacing_s),
        );
        out.insert("lsa_max_age_s".to_string(), json!(self.timers.lsa_max_age));
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::protocols::base::RouterLink;

    #[test]
    fn ospf_entry_tracks_metric_and_type() {
        let mut entry = OspfRoutingTableEntry::new_host_route_to(4, 2, None);
        entry.set_route_metric(3.0);
        entry.set_route_type(OspfRouteType::IntraArea);
        entry.set_area_id(0);
        assert_eq!(entry.destination(), 4);
        assert_eq!(entry.next_hop(), Some(2));
        assert_eq!(entry.get_route_metric(), 3.0);
        assert_eq!(entry.get_route_type(), OspfRouteType::IntraArea);
        assert_eq!(entry.get_area_id(), 0);
    }

    #[test]
    fn ospf_start_installs_direct_route() {
        let mut ospf = OspfProtocol::new(OspfTimers::default());
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

        let outputs = ospf.start(&ctx);
        let routes = outputs.routes.expect("start should output routes");
        assert!(routes.iter().any(|route| route.destination == 2
            && route.next_hop == 2
            && route.protocol == "ospf"));
    }
}
