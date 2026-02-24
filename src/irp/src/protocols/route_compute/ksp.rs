use std::cmp::Ordering;

use super::{Graph, PathCandidate};

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

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;

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
