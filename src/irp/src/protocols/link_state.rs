use std::collections::BTreeMap;

use serde_json::{json, Value};

use crate::model::control_plane::{
    ExchangePolicy, ExchangeScope, ExchangeState, MessageDescriptor,
};
use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::state::LinkStateDb;
use crate::protocols::base::ProtocolContext;

#[derive(Debug, Clone, Copy)]
pub struct LinkStateTimers {
    pub hello_interval: f64,
    pub lsa_interval: f64,
    pub lsa_max_age: f64,
    pub lsa_min_trigger_spacing_s: f64,
}

#[derive(Debug, Clone)]
pub struct LinkStateTick {
    pub hello_due: bool,
    pub lsa_payload: Option<BTreeMap<String, Value>>,
    pub topology_changed: bool,
}

pub struct LinkStateControlPlane {
    msg_seq: u64,
    lsa_seq: i64,
    hello_exchange: ExchangeState,
    lsa_exchange: ExchangeState,
    hello_scope: ExchangeScope,
    lsa_scope: ExchangeScope,
    last_local_links: BTreeMap<u32, f64>,
    lsdb: LinkStateDb,
}

impl LinkStateControlPlane {
    pub fn new() -> Self {
        Self {
            msg_seq: 0,
            lsa_seq: 0,
            hello_exchange: ExchangeState::default(),
            lsa_exchange: ExchangeState::default(),
            hello_scope: ExchangeScope::OneHop,
            lsa_scope: ExchangeScope::FloodDomain,
            last_local_links: BTreeMap::new(),
            lsdb: LinkStateDb::default(),
        }
    }

    pub fn set_descriptor_scopes(&mut self, hello_scope: ExchangeScope, lsa_scope: ExchangeScope) {
        self.hello_scope = hello_scope;
        self.lsa_scope = lsa_scope;
    }

    pub fn tick(
        &mut self,
        ctx: &ProtocolContext,
        timers: LinkStateTimers,
        force_lsa: bool,
    ) -> LinkStateTick {
        let now = ctx.now;
        let hello_due = self
            .hello_exchange
            .periodic_due(now, ExchangePolicy::periodic(timers.hello_interval));

        let links: BTreeMap<u32, f64> = ctx
            .links
            .iter()
            .filter_map(|(router_id, link)| link.is_up.then_some((*router_id, link.cost)))
            .collect();

        let local_links_changed = links != self.last_local_links;
        let lsa_policy = ExchangePolicy::hybrid(
            timers.lsa_interval,
            timers.lsa_min_trigger_spacing_s.max(0.0),
        );
        let periodic_lsa_due = self.lsa_exchange.periodic_due(now, lsa_policy);
        let triggered_lsa_due = if force_lsa {
            self.lsa_exchange.mark_triggered(now);
            true
        } else if local_links_changed {
            self.lsa_exchange.trigger_due(now, lsa_policy)
        } else {
            false
        };
        let should_originate = periodic_lsa_due || triggered_lsa_due;

        let lsa_payload = if should_originate {
            self.lsa_seq += 1;
            self.last_local_links = links.clone();
            self.lsdb
                .upsert(ctx.router_id, self.lsa_seq, links.clone(), now);

            let links_payload: Vec<Value> = links
                .iter()
                .map(|(router_id, cost)| json!({"neighbor_id": router_id, "cost": cost}))
                .collect();
            let mut payload = BTreeMap::new();
            payload.insert("origin_router_id".to_string(), json!(ctx.router_id));
            payload.insert("lsa_seq".to_string(), json!(self.lsa_seq));
            payload.insert("links".to_string(), Value::Array(links_payload));
            Some(payload)
        } else {
            None
        };

        let aged = self.lsdb.age_out(now, timers.lsa_max_age);
        LinkStateTick {
            hello_due,
            lsa_payload,
            topology_changed: aged || should_originate,
        }
    }

    pub fn send_hello<F>(
        &mut self,
        protocol_name: &str,
        ctx: &ProtocolContext,
        mut payload_for_neighbor: F,
    ) -> Vec<(u32, ControlMessage)>
    where
        F: FnMut(u32) -> BTreeMap<String, Value>,
    {
        let mut out = Vec::new();
        for neighbor_id in ctx.links.keys() {
            out.push((
                *neighbor_id,
                self.new_message(
                    protocol_name,
                    ctx.router_id,
                    MessageKind::Hello,
                    payload_for_neighbor(*neighbor_id),
                    ctx.now,
                ),
            ));
        }
        out
    }

    pub fn flood_lsa(
        &mut self,
        protocol_name: &str,
        kind: MessageKind,
        ctx: &ProtocolContext,
        payload: &BTreeMap<String, Value>,
        exclude: Option<u32>,
    ) -> Vec<(u32, ControlMessage)> {
        let mut out = Vec::new();
        for neighbor_id in ctx.links.keys() {
            if exclude == Some(*neighbor_id) {
                continue;
            }
            out.push((
                *neighbor_id,
                self.new_message(
                    protocol_name,
                    ctx.router_id,
                    kind.clone(),
                    payload.clone(),
                    ctx.now,
                ),
            ));
        }
        out
    }

    pub fn upsert_lsa(
        &mut self,
        origin: u32,
        lsa_seq: i64,
        links: BTreeMap<u32, f64>,
        now: f64,
    ) -> bool {
        self.lsdb.upsert(origin, lsa_seq, links, now)
    }

    pub fn build_graph(
        &self,
        router_id: u32,
        validate_nonnegative: bool,
    ) -> BTreeMap<u32, BTreeMap<u32, f64>> {
        let mut graph: BTreeMap<u32, BTreeMap<u32, f64>> = BTreeMap::new();
        for record in self.lsdb.records() {
            graph.entry(record.router_id).or_default();
            for (neighbor_id, cost) in record.links {
                if validate_nonnegative && (!cost.is_finite() || cost < 0.0) {
                    continue;
                }
                graph
                    .entry(record.router_id)
                    .or_default()
                    .insert(neighbor_id, cost);
                graph.entry(neighbor_id).or_default();
            }
        }
        graph.entry(router_id).or_default();
        graph
    }

    pub fn parse_links(raw_links: &[Value], validate_nonnegative: bool) -> BTreeMap<u32, f64> {
        let mut links = BTreeMap::new();
        for item in raw_links {
            let Some(obj) = item.as_object() else {
                continue;
            };
            let Some(neighbor_id) = obj
                .get("neighbor_id")
                .and_then(Value::as_u64)
                .and_then(|v| u32::try_from(v).ok())
            else {
                continue;
            };
            let Some(cost) = obj.get("cost").and_then(Value::as_f64) else {
                continue;
            };
            if validate_nonnegative && (!cost.is_finite() || cost < 0.0) {
                continue;
            }
            links.insert(neighbor_id, cost);
        }
        links
    }

    fn new_message(
        &mut self,
        protocol_name: &str,
        router_id: u32,
        kind: MessageKind,
        payload: BTreeMap<String, Value>,
        now: f64,
    ) -> ControlMessage {
        self.msg_seq += 1;
        let descriptor = match kind {
            MessageKind::Hello => {
                let mut descriptor = MessageDescriptor::hello();
                descriptor.scope = Some(self.hello_scope);
                descriptor
            }
            MessageKind::OspfLsa | MessageKind::DdrLsa => {
                let mut descriptor = MessageDescriptor::topology_lsa(None);
                descriptor.scope = Some(self.lsa_scope);
                descriptor
            }
            MessageKind::RipUpdate => MessageDescriptor::rip_update(None),
        };
        ControlMessage::new(
            protocol_name.to_string(),
            kind,
            router_id,
            self.msg_seq,
            descriptor,
            payload,
            now,
        )
    }
}
