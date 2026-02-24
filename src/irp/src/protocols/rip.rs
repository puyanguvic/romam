use std::collections::BTreeMap;

use serde_json::{json, Value};

use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::{Ipv4RoutingTableEntry, Route, RoutingTableEntry};
use crate::protocols::base::{Ipv4RoutingProtocol, ProtocolContext, ProtocolOutputs};

#[derive(Debug, Clone)]
pub struct RipTimers {
    pub update_interval: f64,
    pub neighbor_timeout: f64,
}

impl Default for RipTimers {
    fn default() -> Self {
        Self {
            update_interval: 5.0,
            neighbor_timeout: 15.0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RipRouteStatus {
    Valid,
    Invalid,
}

#[derive(Debug, Clone, PartialEq)]
pub struct RipRoutingTableEntry {
    base: Ipv4RoutingTableEntry,
    route_tag: u16,
    route_metric: f64,
    status: RipRouteStatus,
    changed: bool,
}

impl RoutingTableEntry for RipRoutingTableEntry {
    fn base(&self) -> &Ipv4RoutingTableEntry {
        &self.base
    }
}

impl RipRoutingTableEntry {
    pub fn new_host_route_to(destination: u32, next_hop: u32, interface: Option<u32>) -> Self {
        Self {
            base: Ipv4RoutingTableEntry::create_host_route_to(destination, next_hop, interface),
            route_tag: 0,
            route_metric: 0.0,
            status: RipRouteStatus::Invalid,
            changed: false,
        }
    }

    pub fn set_route_tag(&mut self, route_tag: u16) {
        if self.route_tag != route_tag {
            self.route_tag = route_tag;
            self.changed = true;
        }
    }

    pub fn get_route_tag(&self) -> u16 {
        self.route_tag
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

    pub fn set_route_status(&mut self, status: RipRouteStatus) {
        if self.status != status {
            self.status = status;
            self.changed = true;
        }
    }

    pub fn get_route_status(&self) -> RipRouteStatus {
        self.status
    }

    pub fn set_route_changed(&mut self, changed: bool) {
        self.changed = changed;
    }

    pub fn is_route_changed(&self) -> bool {
        self.changed
    }

    pub fn destination(&self) -> u32 {
        self.base.get_dest()
    }

    pub fn next_hop(&self) -> Option<u32> {
        self.base.get_gateway()
    }

    fn semantically_equal(&self, other: &Self) -> bool {
        self.base == other.base
            && self.route_tag == other.route_tag
            && self.route_metric.to_bits() == other.route_metric.to_bits()
            && self.status == other.status
    }

    fn to_route(&self, protocol: &str) -> Option<Route> {
        if self.status != RipRouteStatus::Valid {
            return None;
        }
        let next_hop = self.base.get_gateway()?;
        Some(Route::new_host(
            self.base.get_dest(),
            next_hop,
            self.route_metric,
            protocol,
        ))
    }
}

pub struct RipProtocol {
    timers: RipTimers,
    infinity_metric: f64,
    poison_reverse: bool,
    msg_seq: u64,
    last_update_at: f64,
    routes: BTreeMap<u32, RipRoutingTableEntry>,
    neighbor_vectors: BTreeMap<u32, (f64, BTreeMap<u32, f64>)>,
}

impl RipProtocol {
    pub fn new(timers: RipTimers, infinity_metric: f64, poison_reverse: bool) -> Self {
        Self {
            timers,
            infinity_metric,
            poison_reverse,
            msg_seq: 0,
            last_update_at: -1e9,
            routes: BTreeMap::new(),
            neighbor_vectors: BTreeMap::new(),
        }
    }

    fn expire_neighbor_vectors(&mut self, ctx: &ProtocolContext) {
        let stale: Vec<u32> = self
            .neighbor_vectors
            .iter()
            .filter_map(|(neighbor_id, (last_seen, _))| {
                let Some(link) = ctx.links.get(neighbor_id) else {
                    return Some(*neighbor_id);
                };
                if !link.is_up {
                    return Some(*neighbor_id);
                }
                ((ctx.now - *last_seen) > self.timers.neighbor_timeout).then_some(*neighbor_id)
            })
            .collect();

        for neighbor_id in stale {
            self.neighbor_vectors.remove(&neighbor_id);
        }
    }

    fn recompute_routes(&mut self, ctx: &ProtocolContext) -> bool {
        let mut candidates: BTreeMap<u32, (f64, u32)> = BTreeMap::new();

        for (neighbor_id, link) in &ctx.links {
            if !link.is_up {
                continue;
            }
            candidates.insert(*neighbor_id, (link.cost, *neighbor_id));
        }

        for (neighbor_id, (_, vector)) in &self.neighbor_vectors {
            let Some(link) = ctx.links.get(neighbor_id) else {
                continue;
            };
            if !link.is_up {
                continue;
            }
            let base = link.cost;
            for (destination, advertised_metric) in vector {
                if *destination == ctx.router_id {
                    continue;
                }
                let total_metric = self.infinity_metric.min(base + *advertised_metric);
                if total_metric >= self.infinity_metric {
                    continue;
                }

                let proposal = (total_metric, *neighbor_id);
                match candidates.get(destination) {
                    None => {
                        candidates.insert(*destination, proposal);
                    }
                    Some(current) if proposal < *current => {
                        candidates.insert(*destination, proposal);
                    }
                    _ => {}
                }
            }
        }

        let mut next_routes: BTreeMap<u32, RipRoutingTableEntry> = BTreeMap::new();
        for (destination, (metric, next_hop)) in candidates {
            let mut route = RipRoutingTableEntry::new_host_route_to(destination, next_hop, None);
            route.set_route_tag(0);
            route.set_route_metric(metric);
            route.set_route_status(RipRouteStatus::Valid);

            let changed = self
                .routes
                .get(&destination)
                .is_none_or(|previous| !route.semantically_equal(previous));
            route.set_route_changed(changed);
            next_routes.insert(destination, route);
        }

        let removed = self
            .routes
            .keys()
            .any(|destination| !next_routes.contains_key(destination));

        let unchanged = !removed
            && self.routes.len() == next_routes.len()
            && next_routes.iter().all(|(destination, route)| {
                self.routes
                    .get(destination)
                    .is_some_and(|old| route.semantically_equal(old))
            });
        if unchanged {
            return false;
        }
        self.routes = next_routes;
        true
    }

    fn export_routes(&self) -> Vec<Route> {
        self.routes
            .values()
            .filter_map(|route| route.to_route(self.name()))
            .collect()
    }

    fn send_updates(&mut self, ctx: &ProtocolContext) -> Vec<(u32, ControlMessage)> {
        let mut outbound = Vec::new();
        for neighbor_id in ctx.links.keys() {
            let mut entries = Vec::new();
            entries.push(json!({"destination": ctx.router_id, "metric": 0.0}));
            for route in self.routes.values() {
                let Some(next_hop) = route.next_hop() else {
                    continue;
                };
                let destination = route.destination();
                let route_metric = route.get_route_metric();
                let metric = if next_hop == *neighbor_id && destination != *neighbor_id {
                    if self.poison_reverse {
                        self.infinity_metric
                    } else {
                        continue;
                    }
                } else {
                    self.infinity_metric.min(route_metric)
                };
                entries.push(json!({"destination": destination, "metric": metric}));
            }

            let mut payload = BTreeMap::new();
            payload.insert("entries".to_string(), Value::Array(entries));
            outbound.push((
                *neighbor_id,
                self.new_message(ctx.router_id, MessageKind::RipUpdate, payload, ctx.now),
            ));
        }
        outbound
    }

    fn new_message(
        &mut self,
        router_id: u32,
        kind: MessageKind,
        payload: BTreeMap<String, Value>,
        now: f64,
    ) -> ControlMessage {
        self.msg_seq += 1;
        ControlMessage {
            protocol: self.name().to_string(),
            kind,
            src_router_id: router_id,
            seq: self.msg_seq,
            payload,
            ts: now,
        }
    }

    fn parse_update_entries(entries: &[Value], infinity_metric: f64) -> BTreeMap<u32, f64> {
        let mut parsed = BTreeMap::new();
        for item in entries {
            let Some(obj) = item.as_object() else {
                continue;
            };
            let Some(destination) = obj
                .get("destination")
                .and_then(Value::as_u64)
                .and_then(|v| u32::try_from(v).ok())
            else {
                continue;
            };
            let Some(metric) = obj.get("metric").and_then(Value::as_f64) else {
                continue;
            };
            parsed.insert(destination, metric.max(0.0).min(infinity_metric));
        }
        parsed
    }
}

impl Ipv4RoutingProtocol for RipProtocol {
    fn name(&self) -> &'static str {
        "rip"
    }

    fn start(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
        let routes_changed = self.recompute_routes(ctx);
        if routes_changed {
            outputs.routes = Some(self.export_routes());
        }
        outputs.outbound.extend(self.send_updates(ctx));
        self.last_update_at = ctx.now;
        outputs
    }

    fn on_timer(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
        self.expire_neighbor_vectors(ctx);

        let routes_changed = self.recompute_routes(ctx);
        let periodic_due = (ctx.now - self.last_update_at) >= self.timers.update_interval;

        if routes_changed {
            outputs.routes = Some(self.export_routes());
        }
        if routes_changed || periodic_due {
            outputs.outbound.extend(self.send_updates(ctx));
            self.last_update_at = ctx.now;
        }

        outputs
    }

    fn on_message(&mut self, ctx: &ProtocolContext, message: &ControlMessage) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();

        if message.kind != MessageKind::RipUpdate {
            return outputs;
        }

        let entries = message
            .payload
            .get("entries")
            .and_then(Value::as_array)
            .map_or_else(BTreeMap::new, |arr| {
                Self::parse_update_entries(arr, self.infinity_metric)
            });

        self.neighbor_vectors
            .insert(message.src_router_id, (ctx.now, entries));

        if self.recompute_routes(ctx) {
            outputs.routes = Some(self.export_routes());
            outputs.outbound.extend(self.send_updates(ctx));
            self.last_update_at = ctx.now;
        }

        outputs
    }

    fn metrics(&self) -> BTreeMap<String, Value> {
        let mut out = BTreeMap::new();
        out.insert(
            "update_interval_s".to_string(),
            json!(self.timers.update_interval),
        );
        out.insert(
            "neighbor_timeout_s".to_string(),
            json!(self.timers.neighbor_timeout),
        );
        out.insert("infinity_metric".to_string(), json!(self.infinity_metric));
        out.insert("poison_reverse".to_string(), json!(self.poison_reverse));
        out
    }
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use crate::protocols::base::RouterLink;

    use super::*;

    #[test]
    fn rip_routing_table_entry_tracks_change_and_accessors() {
        let mut entry = RipRoutingTableEntry::new_host_route_to(7, 2, Some(1));
        assert_eq!(entry.destination(), 7);
        assert_eq!(entry.next_hop(), Some(2));
        assert_eq!(entry.get_route_tag(), 0);
        assert_eq!(entry.get_route_status(), RipRouteStatus::Invalid);
        assert!(!entry.is_route_changed());

        entry.set_route_metric(3.0);
        assert!(entry.is_route_changed());
        assert_eq!(entry.get_route_metric(), 3.0);
        entry.set_route_changed(false);

        entry.set_route_tag(9);
        assert_eq!(entry.get_route_tag(), 9);
        assert!(entry.is_route_changed());
    }

    #[test]
    fn rip_start_has_direct_neighbor_route() {
        let mut rip = RipProtocol::new(RipTimers::default(), 16.0, true);
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

        let outputs = rip.start(&ctx);
        let routes = outputs.routes.expect("start should output routes");
        assert_eq!(routes.len(), 1);
        assert_eq!(routes[0].destination, 2);
        assert_eq!(routes[0].next_hop, 2);
        assert_eq!(routes[0].metric, 1.0);
    }
}
