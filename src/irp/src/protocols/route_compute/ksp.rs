use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};

use super::{frontier::DistanceFrontier, Graph, PathCandidate};

const EPS: f64 = 1e-9;

pub fn compare_path_candidate(a: &PathCandidate, b: &PathCandidate) -> Ordering {
    a.cost
        .total_cmp(&b.cost)
        .then_with(|| a.nodes.cmp(&b.nodes))
}

fn dijkstra_path_with_filters(
    graph: &Graph,
    src: u32,
    dst: u32,
    blocked_nodes: &BTreeSet<u32>,
    blocked_edges: &BTreeSet<(u32, u32)>,
) -> Option<PathCandidate> {
    if blocked_nodes.contains(&src) || blocked_nodes.contains(&dst) {
        return None;
    }
    if src == dst {
        return Some(PathCandidate {
            nodes: vec![src],
            cost: 0.0,
        });
    }

    let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
    let mut parent: BTreeMap<u32, u32> = BTreeMap::new();
    let mut settled: BTreeSet<u32> = BTreeSet::new();
    let mut frontier = DistanceFrontier::new();

    dist.insert(src, 0.0);
    frontier.push(src, 0.0);

    loop {
        let Some((u, cost_u)) = frontier.pop_min(|node, cost| {
            if settled.contains(&node) {
                return true;
            }
            match dist.get(&node).copied() {
                Some(best) => cost > best + EPS,
                None => true,
            }
        }) else {
            break;
        };

        settled.insert(u);
        if u == dst {
            break;
        }

        if let Some(neighbors) = graph.get(&u) {
            for (v, edge_cost) in neighbors {
                if blocked_nodes.contains(v)
                    || blocked_edges.contains(&(u, *v))
                    || !edge_cost.is_finite()
                    || *edge_cost < 0.0
                {
                    continue;
                }

                let candidate_metric = cost_u + *edge_cost;
                let best_metric = dist.get(v).copied().unwrap_or(f64::INFINITY);
                let best_parent = parent.get(v).copied().unwrap_or(u32::MAX);

                let better_metric = candidate_metric + EPS < best_metric;
                let equal_metric = (candidate_metric - best_metric).abs() <= EPS;
                let better_parent = equal_metric && u < best_parent;
                if better_metric || better_parent {
                    dist.insert(*v, candidate_metric);
                    parent.insert(*v, u);
                    frontier.push(*v, candidate_metric);
                    settled.remove(v);
                }
            }
        }
    }

    let total_cost = dist.get(&dst).copied()?;
    if !total_cost.is_finite() {
        return None;
    }

    let mut reversed = vec![dst];
    let mut current = dst;
    let max_steps = parent.len() + 1;
    for _ in 0..max_steps {
        if current == src {
            reversed.reverse();
            return Some(PathCandidate {
                nodes: reversed,
                cost: total_cost,
            });
        }
        current = parent.get(&current).copied()?;
        reversed.push(current);
    }

    None
}

fn path_cost(graph: &Graph, nodes: &[u32]) -> Option<f64> {
    if nodes.len() < 2 {
        return Some(0.0);
    }
    let mut cost = 0.0;
    for edge in nodes.windows(2) {
        let u = edge[0];
        let v = edge[1];
        let weight = graph.get(&u)?.get(&v).copied()?;
        if !weight.is_finite() || weight < 0.0 {
            return None;
        }
        cost += weight;
    }
    Some(cost)
}

pub fn yen_k_shortest_simple_paths(
    graph: &Graph,
    src: u32,
    dst: u32,
    k_paths: usize,
) -> Vec<PathCandidate> {
    let max_results = k_paths.max(1);

    let Some(first_path) =
        dijkstra_path_with_filters(graph, src, dst, &BTreeSet::new(), &BTreeSet::new())
    else {
        return Vec::new();
    };

    let mut shortest_paths = vec![first_path];
    let mut candidate_pool: Vec<PathCandidate> = Vec::new();
    let mut seen_paths: BTreeSet<Vec<u32>> = BTreeSet::new();
    seen_paths.insert(shortest_paths[0].nodes.clone());

    for k in 1..max_results {
        let previous_path = &shortest_paths[k - 1];
        if previous_path.nodes.len() < 2 {
            break;
        }

        for spur_idx in 0..previous_path.nodes.len() - 1 {
            let spur_node = previous_path.nodes[spur_idx];
            let root_path: Vec<u32> = previous_path.nodes[..=spur_idx].to_vec();
            let Some(root_cost) = path_cost(graph, &root_path) else {
                continue;
            };

            let mut blocked_edges: BTreeSet<(u32, u32)> = BTreeSet::new();
            for path in &shortest_paths {
                if path.nodes.len() > spur_idx + 1 && path.nodes[..=spur_idx] == root_path {
                    blocked_edges.insert((path.nodes[spur_idx], path.nodes[spur_idx + 1]));
                }
            }

            let blocked_nodes: BTreeSet<u32> = root_path
                .iter()
                .take(root_path.len().saturating_sub(1))
                .copied()
                .collect();

            let Some(spur_path) =
                dijkstra_path_with_filters(graph, spur_node, dst, &blocked_nodes, &blocked_edges)
            else {
                continue;
            };

            let mut total_nodes = root_path[..root_path.len().saturating_sub(1)].to_vec();
            total_nodes.extend(spur_path.nodes);

            if !seen_paths.insert(total_nodes.clone()) {
                continue;
            }

            candidate_pool.push(PathCandidate {
                nodes: total_nodes,
                cost: root_cost + spur_path.cost,
            });
        }

        if candidate_pool.is_empty() {
            break;
        }

        let best_idx = candidate_pool
            .iter()
            .enumerate()
            .min_by(|(_, a), (_, b)| compare_path_candidate(a, b))
            .map(|(idx, _)| idx)
            .unwrap_or(0);

        shortest_paths.push(candidate_pool.swap_remove(best_idx));
    }

    shortest_paths
}

pub fn k_shortest_simple_paths(
    graph: &Graph,
    src: u32,
    dst: u32,
    k_paths: usize,
) -> Vec<PathCandidate> {
    yen_k_shortest_simple_paths(graph, src, dst, k_paths)
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

    #[test]
    fn yen_returns_sorted_by_cost_then_lexicographic() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 1.0)])),
            (2, BTreeMap::from([(4, 1.0), (3, 0.5)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);

        let out = yen_k_shortest_simple_paths(&graph, 1, 4, 3);
        assert_eq!(out.len(), 3);
        assert_eq!(out[0].nodes, vec![1, 2, 4]);
        assert_eq!(out[1].nodes, vec![1, 3, 4]);
        assert_eq!(out[2].nodes, vec![1, 2, 3, 4]);
    }

    #[test]
    fn yen_handles_src_equal_dst() {
        let graph: Graph = BTreeMap::from([(1, BTreeMap::new())]);
        let out = yen_k_shortest_simple_paths(&graph, 1, 1, 3);
        assert_eq!(out.len(), 1);
        assert_eq!(out[0].nodes, vec![1]);
        assert_eq!(out[0].cost, 0.0);
    }
}
