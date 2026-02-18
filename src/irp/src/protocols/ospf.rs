use std::collections::{BTreeMap, BTreeSet};

use serde_json::{json, Value};

use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::Route;
use crate::model::state::LinkStateDb;
use crate::protocols::base::{ProtocolContext, ProtocolEngine, ProtocolOutputs};

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
    msg_seq: u64,
    lsa_seq: i64,
    last_hello_at: f64,
    last_lsa_at: f64,
    last_local_links: BTreeMap<u32, f64>,
    lsdb: LinkStateDb,
}

impl OspfProtocol {
    pub fn new(timers: OspfTimers) -> Self {
        Self {
            timers,
            msg_seq: 0,
            lsa_seq: 0,
            last_hello_at: -1e9,
            last_lsa_at: -1e9,
            last_local_links: BTreeMap::new(),
            lsdb: LinkStateDb::default(),
        }
    }

    fn drive(&mut self, ctx: &ProtocolContext, force_lsa: bool) -> ProtocolOutputs {
        let mut outputs = ProtocolOutputs::default();
        let now = ctx.now;

        if (now - self.last_hello_at) >= self.timers.hello_interval {
            outputs.outbound.extend(self.send_hello(ctx));
            self.last_hello_at = now;
        }

        let links: BTreeMap<u32, f64> = ctx
            .links
            .iter()
            .filter_map(|(router_id, link)| link.is_up.then_some((*router_id, link.cost)))
            .collect();

        let mut should_originate = force_lsa || links != self.last_local_links;
        if (now - self.last_lsa_at) >= self.timers.lsa_interval {
            should_originate = true;
        }

        if should_originate {
            self.lsa_seq += 1;
            self.last_local_links = links.clone();

            let links_payload: Vec<Value> = links
                .iter()
                .map(|(router_id, cost)| json!({"neighbor_id": router_id, "cost": cost}))
                .collect();

            let mut payload = BTreeMap::new();
            payload.insert("origin_router_id".to_string(), json!(ctx.router_id));
            payload.insert("lsa_seq".to_string(), json!(self.lsa_seq));
            payload.insert("links".to_string(), Value::Array(links_payload));

            self.lsdb.upsert(ctx.router_id, self.lsa_seq, links, now);
            outputs.outbound.extend(self.flood_lsa(ctx, &payload, None));
            self.last_lsa_at = now;
        }

        let aged = self.lsdb.age_out(now, self.timers.lsa_max_age);
        if aged || should_originate {
            outputs.routes = Some(self.compute_routes(ctx));
        }

        outputs
    }

    fn send_hello(&mut self, ctx: &ProtocolContext) -> Vec<(u32, ControlMessage)> {
        let mut out = Vec::new();
        for neighbor_id in ctx.links.keys() {
            let mut payload = BTreeMap::new();
            payload.insert("router_id".to_string(), json!(ctx.router_id));
            out.push((
                *neighbor_id,
                self.new_message(ctx.router_id, MessageKind::Hello, payload, ctx.now),
            ));
        }
        out
    }

    fn flood_lsa(
        &mut self,
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
                    ctx.router_id,
                    MessageKind::OspfLsa,
                    payload.clone(),
                    ctx.now,
                ),
            ));
        }
        out
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

    fn compute_routes(&self, ctx: &ProtocolContext) -> Vec<Route> {
        let mut graph: BTreeMap<u32, BTreeMap<u32, f64>> = BTreeMap::new();

        for record in self.lsdb.records() {
            graph.entry(record.router_id).or_default();
            for (neighbor_id, cost) in record.links {
                graph
                    .entry(record.router_id)
                    .or_default()
                    .insert(neighbor_id, cost);
                graph.entry(neighbor_id).or_default();
            }
        }

        let src = ctx.router_id;
        graph.entry(src).or_default();

        let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
        let mut first_hop: BTreeMap<u32, u32> = BTreeMap::new();
        let mut visited: BTreeSet<u32> = BTreeSet::new();

        dist.insert(src, 0.0);

        loop {
            let mut candidate: Option<(u32, f64)> = None;
            for (node, node_dist) in &dist {
                if visited.contains(node) {
                    continue;
                }
                match candidate {
                    None => candidate = Some((*node, *node_dist)),
                    Some((best_node, best_dist)) => {
                        if *node_dist < best_dist || (*node_dist == best_dist && *node < best_node)
                        {
                            candidate = Some((*node, *node_dist));
                        }
                    }
                }
            }

            let Some((u, cost_u)) = candidate else {
                break;
            };
            visited.insert(u);

            if let Some(neighbors) = graph.get(&u) {
                for (v, edge_cost) in neighbors {
                    let candidate_metric = cost_u + *edge_cost;
                    let candidate_hop = if u == src {
                        *v
                    } else {
                        *first_hop.get(&u).unwrap_or(v)
                    };

                    let best = dist.get(v).copied().unwrap_or(f64::INFINITY);
                    let best_hop = first_hop.get(v).copied().unwrap_or(u32::MAX);

                    if candidate_metric < best
                        || (candidate_metric == best && candidate_hop < best_hop)
                    {
                        dist.insert(*v, candidate_metric);
                        first_hop.insert(*v, candidate_hop);
                    }
                }
            }
        }

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

    fn parse_links(raw_links: &[Value]) -> BTreeMap<u32, f64> {
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
            links.insert(neighbor_id, cost);
        }
        links
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
            .map_or_else(BTreeMap::new, |raw| Self::parse_links(raw));

        let changed = self.lsdb.upsert(origin, lsa_seq, links, ctx.now);
        if !changed {
            return outputs;
        }

        outputs
            .outbound
            .extend(self.flood_lsa(ctx, &message.payload, Some(message.src_router_id)));
        outputs.routes = Some(self.compute_routes(ctx));
        outputs
    }
}
