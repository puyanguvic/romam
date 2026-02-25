use std::collections::{BTreeMap, BTreeSet};

use super::{frontier::DistanceFrontier, Graph, SpfEcmpResult, SpfSingleResult};

pub fn compute_spf_single(graph: &Graph, src: u32) -> SpfSingleResult {
    let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
    let mut first_hop: BTreeMap<u32, u32> = BTreeMap::new();
    let mut visited: BTreeSet<u32> = BTreeSet::new();
    let mut frontier = DistanceFrontier::new();

    dist.insert(src, 0.0);
    frontier.push(src, 0.0);

    loop {
        let Some((u, cost_u)) = frontier.pop_min(|node, cost| {
            if visited.contains(&node) {
                return true;
            }
            match dist.get(&node) {
                Some(best) => cost.total_cmp(best).is_gt(),
                None => true,
            }
        }) else {
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

                let better_metric = candidate_metric < best;
                let better_hop = candidate_metric == best && candidate_hop < best_hop;
                if better_metric || better_hop {
                    dist.insert(*v, candidate_metric);
                    first_hop.insert(*v, candidate_hop);
                    if better_metric {
                        frontier.push(*v, candidate_metric);
                    }
                }
            }
        }
    }

    SpfSingleResult { dist, first_hop }
}

pub fn compute_spf_ecmp(graph: &Graph, src: u32) -> SpfEcmpResult {
    let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
    let mut first_hops: BTreeMap<u32, BTreeSet<u32>> = BTreeMap::new();
    let mut visited: BTreeSet<u32> = BTreeSet::new();
    let mut frontier = DistanceFrontier::new();

    dist.insert(src, 0.0);
    first_hops.insert(src, BTreeSet::new());
    frontier.push(src, 0.0);

    loop {
        let Some((u, cost_u)) = frontier.pop_min(|node, cost| {
            if visited.contains(&node) {
                return true;
            }
            match dist.get(&node) {
                Some(best) => cost.total_cmp(best).is_gt(),
                None => true,
            }
        }) else {
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
                    frontier.push(*v, candidate_metric);
                    continue;
                }

                if (candidate_metric - best).abs() <= f64::EPSILON {
                    let entry = first_hops.entry(*v).or_default();
                    entry.extend(candidate_hops);
                }
            }
        }
    }

    SpfEcmpResult { dist, first_hops }
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;

    #[test]
    fn spf_single_prefers_lower_next_hop_on_tie() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 1.0)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);
        let result = compute_spf_single(&graph, 1);
        assert_eq!(result.first_hop.get(&4).copied(), Some(2));
        assert_eq!(result.dist.get(&4).copied(), Some(2.0));
    }
}
