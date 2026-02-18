use std::collections::{BTreeMap, BTreeSet};

use serde_json::{json, Value};

use crate::model::messages::{ControlMessage, MessageKind};
use crate::model::routing::Route;
use crate::model::state::LinkStateDb;
use crate::protocols::base::{ProtocolContext, ProtocolEngine, ProtocolOutputs};

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
    msg_seq: u64,
    lsa_seq: i64,
    last_hello_at: f64,
    last_lsa_at: f64,
    last_local_links: BTreeMap<u32, f64>,
    lsdb: LinkStateDb,
}

impl EcmpProtocol {
    pub fn new(params: EcmpParams) -> Self {
        Self {
            params,
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

        if (now - self.last_hello_at) >= self.params.timers.hello_interval {
            outputs.outbound.extend(self.send_hello(ctx));
            self.last_hello_at = now;
        }

        let links: BTreeMap<u32, f64> = ctx
            .links
            .iter()
            .filter_map(|(router_id, link)| link.is_up.then_some((*router_id, link.cost)))
            .collect();

        let mut should_originate = force_lsa || links != self.last_local_links;
        if (now - self.last_lsa_at) >= self.params.timers.lsa_interval {
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

        let aged = self.lsdb.age_out(now, self.params.timers.lsa_max_age);
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
        let mut first_hops: BTreeMap<u32, BTreeSet<u32>> = BTreeMap::new();
        let mut visited: BTreeSet<u32> = BTreeSet::new();

        dist.insert(src, 0.0);
        first_hops.insert(src, BTreeSet::new());

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
                    let candidate_hops: BTreeSet<u32> = if u == src {
                        BTreeSet::from([*v])
                    } else {
                        first_hops.get(&u).cloned().unwrap_or_default()
                    };
                    if candidate_hops.is_empty() {
                        continue;
                    }

                    let best = dist.get(v).copied().unwrap_or(f64::INFINITY);
                    if candidate_metric < best {
                        dist.insert(*v, candidate_metric);
                        first_hops.insert(*v, candidate_hops);
                        continue;
                    }

                    if (candidate_metric - best).abs() <= f64::EPSILON {
                        let entry = first_hops.entry(*v).or_default();
                        entry.extend(candidate_hops);
                    }
                }
            }
        }

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

        ecmp.lsdb
            .upsert(1, 1, BTreeMap::from([(2_u32, 1.0), (3_u32, 1.0)]), 0.0);
        ecmp.lsdb.upsert(2, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);
        ecmp.lsdb.upsert(3, 1, BTreeMap::from([(4_u32, 1.0)]), 0.0);

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
