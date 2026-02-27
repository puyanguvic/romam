use std::collections::{BTreeMap, BTreeSet, VecDeque};

use super::{BellmanFordResult, Graph};

fn collect_nodes(graph: &Graph) -> BTreeSet<u32> {
    let mut nodes = BTreeSet::new();
    for (u, neighbors) in graph {
        nodes.insert(*u);
        for v in neighbors.keys() {
            nodes.insert(*v);
        }
    }
    nodes
}

pub fn compute_bellman_ford(graph: &Graph, src: u32) -> BellmanFordResult {
    let mut nodes = collect_nodes(graph);
    nodes.insert(src);

    let mut dist: BTreeMap<u32, f64> = nodes.iter().map(|node| (*node, f64::INFINITY)).collect();
    let mut predecessor: BTreeMap<u32, u32> = BTreeMap::new();
    dist.insert(src, 0.0);

    for _ in 0..nodes.len().saturating_sub(1) {
        let mut changed = false;
        for (u, neighbors) in graph {
            let base = dist.get(u).copied().unwrap_or(f64::INFINITY);
            if !base.is_finite() {
                continue;
            }
            for (v, weight) in neighbors {
                if !weight.is_finite() {
                    continue;
                }
                let candidate = base + *weight;
                let current = dist.get(v).copied().unwrap_or(f64::INFINITY);
                if candidate < current {
                    dist.insert(*v, candidate);
                    predecessor.insert(*v, *u);
                    changed = true;
                }
            }
        }
        if !changed {
            break;
        }
    }

    let mut negative_cycle_seed: BTreeSet<u32> = BTreeSet::new();
    for (u, neighbors) in graph {
        let base = dist.get(u).copied().unwrap_or(f64::INFINITY);
        if !base.is_finite() {
            continue;
        }
        for (v, weight) in neighbors {
            if !weight.is_finite() {
                continue;
            }
            let candidate = base + *weight;
            let current = dist.get(v).copied().unwrap_or(f64::INFINITY);
            if candidate < current {
                negative_cycle_seed.insert(*u);
                negative_cycle_seed.insert(*v);
            }
        }
    }

    // Expand to all nodes reachable from any negative cycle seed.
    let mut negative_cycle_nodes = BTreeSet::new();
    let mut queue: VecDeque<u32> = negative_cycle_seed.iter().copied().collect();
    while let Some(node) = queue.pop_front() {
        if !negative_cycle_nodes.insert(node) {
            continue;
        }
        if let Some(neighbors) = graph.get(&node) {
            for next in neighbors.keys() {
                queue.push_back(*next);
            }
        }
    }

    BellmanFordResult {
        dist,
        predecessor,
        negative_cycle_nodes,
    }
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;

    #[test]
    fn bellman_ford_handles_negative_edges_without_cycle() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 10.0)])),
            (2, BTreeMap::from([(3, -2.0)])),
            (3, BTreeMap::new()),
        ]);

        let out = compute_bellman_ford(&graph, 1);
        assert!(!out.has_negative_cycle());
        assert_eq!(out.dist.get(&3).copied(), Some(-1.0));
        assert_eq!(out.predecessor.get(&3).copied(), Some(2));
    }

    #[test]
    fn bellman_ford_marks_negative_cycle_reachable_nodes() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0)])),
            (2, BTreeMap::from([(3, 1.0)])),
            (3, BTreeMap::from([(2, -3.0), (4, 1.0)])),
            (4, BTreeMap::new()),
        ]);

        let out = compute_bellman_ford(&graph, 1);
        assert!(out.has_negative_cycle());
        assert!(out.negative_cycle_nodes.contains(&2));
        assert!(out.negative_cycle_nodes.contains(&3));
        assert!(out.negative_cycle_nodes.contains(&4));
    }
}
