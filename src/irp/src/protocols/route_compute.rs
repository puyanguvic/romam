use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};

pub type Graph = BTreeMap<u32, BTreeMap<u32, f64>>;

#[derive(Debug, Clone)]
pub struct PathCandidate {
    pub nodes: Vec<u32>,
    pub cost: f64,
}

#[derive(Debug, Clone)]
pub struct SpfSingleResult {
    pub dist: BTreeMap<u32, f64>,
    pub first_hop: BTreeMap<u32, u32>,
}

#[derive(Debug, Clone)]
pub struct SpfEcmpResult {
    pub dist: BTreeMap<u32, f64>,
    pub first_hops: BTreeMap<u32, BTreeSet<u32>>,
}

pub fn compare_path_candidate(a: &PathCandidate, b: &PathCandidate) -> Ordering {
    match a.cost.partial_cmp(&b.cost) {
        Some(Ordering::Less) => Ordering::Less,
        Some(Ordering::Greater) => Ordering::Greater,
        _ => a.nodes.cmp(&b.nodes),
    }
}

pub fn k_shortest_simple_paths(
    graph: &Graph,
    src: u32,
    dst: u32,
    k_paths: usize,
) -> Vec<PathCandidate> {
    let max_hops = graph.len().max(2);
    let max_results = k_paths.max(1);
    let mut frontier = vec![PathCandidate {
        nodes: vec![src],
        cost: 0.0,
    }];
    let mut out = Vec::new();
    let mut expanded = 0usize;
    let expand_limit = graph.len().saturating_mul(graph.len().max(2)) * max_results.max(2);

    while !frontier.is_empty() && out.len() < max_results && expanded <= expand_limit {
        let best_idx = frontier
            .iter()
            .enumerate()
            .min_by(|(_, a), (_, b)| compare_path_candidate(a, b))
            .map(|(idx, _)| idx)
            .unwrap_or(0);
        let state = frontier.swap_remove(best_idx);
        let u = *state.nodes.last().unwrap_or(&src);

        if u == dst {
            out.push(state);
            continue;
        }
        if state.nodes.len() >= max_hops {
            continue;
        }

        if let Some(neighbors) = graph.get(&u) {
            for (neighbor, cost) in neighbors {
                if state.nodes.contains(neighbor) {
                    continue;
                }
                if !cost.is_finite() || *cost < 0.0 {
                    continue;
                }
                let mut next_nodes = state.nodes.clone();
                next_nodes.push(*neighbor);
                frontier.push(PathCandidate {
                    nodes: next_nodes,
                    cost: state.cost + *cost,
                });
            }
        }
        expanded += 1;
    }
    out
}

pub fn compute_spf_single(graph: &Graph, src: u32) -> SpfSingleResult {
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
                    if *node_dist < best_dist || (*node_dist == best_dist && *node < best_node) {
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

                if candidate_metric < best || (candidate_metric == best && candidate_hop < best_hop)
                {
                    dist.insert(*v, candidate_metric);
                    first_hop.insert(*v, candidate_hop);
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
                    if *node_dist < best_dist || (*node_dist == best_dist && *node < best_node) {
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

    SpfEcmpResult { dist, first_hops }
}

#[cfg(test)]
mod tests {
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

    #[test]
    fn k_shortest_returns_multiple_simple_paths() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 1.5)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);
        let out = k_shortest_simple_paths(&graph, 1, 4, 2);
        assert_eq!(out.len(), 2);
        assert_eq!(out[0].nodes, vec![1, 2, 4]);
        assert_eq!(out[1].nodes, vec![1, 3, 4]);
    }
}
