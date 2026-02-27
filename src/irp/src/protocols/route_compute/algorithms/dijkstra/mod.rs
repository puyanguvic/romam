use std::collections::{BTreeMap, BTreeSet};

use super::super::frontier::DistanceFrontier;
use super::super::{
    EdgeUpdate, Graph, IncrementalSpfResult, LfaCandidate, SpfEcmpResult, SpfSingleResult,
    SpfTreeResult,
};

const EPS: f64 = 1e-9;

fn edge_cost_supported(edge_cost: f64) -> bool {
    edge_cost.is_finite() && edge_cost >= 0.0
}

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

fn is_stale(best: f64, candidate: f64) -> bool {
    candidate > best + EPS
}

pub fn compute_spf_tree(graph: &Graph, src: u32) -> SpfTreeResult {
    let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
    let mut first_hop: BTreeMap<u32, u32> = BTreeMap::new();
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
                Some(best) => is_stale(best, cost),
                None => true,
            }
        }) else {
            break;
        };
        settled.insert(u);

        if let Some(neighbors) = graph.get(&u) {
            for (v, edge_cost) in neighbors {
                if !edge_cost_supported(*edge_cost) {
                    continue;
                }

                let candidate_metric = cost_u + *edge_cost;
                let candidate_hop = if u == src {
                    *v
                } else {
                    *first_hop.get(&u).unwrap_or(v)
                };

                let best_metric = dist.get(v).copied().unwrap_or(f64::INFINITY);
                let best_hop = first_hop.get(v).copied().unwrap_or(u32::MAX);
                let best_parent = parent.get(v).copied().unwrap_or(u32::MAX);

                let better_metric = candidate_metric + EPS < best_metric;
                let equal_metric = (candidate_metric - best_metric).abs() <= EPS;
                let better_hop = equal_metric && candidate_hop < best_hop;
                let better_parent = equal_metric && candidate_hop == best_hop && u < best_parent;

                if better_metric || better_hop || better_parent {
                    dist.insert(*v, candidate_metric);
                    first_hop.insert(*v, candidate_hop);
                    parent.insert(*v, u);
                    frontier.push(*v, candidate_metric);
                    settled.remove(v);
                }
            }
        }
    }

    SpfTreeResult {
        dist,
        first_hop,
        parent,
    }
}

pub fn compute_spf_single(graph: &Graph, src: u32) -> SpfSingleResult {
    compute_spf_tree(graph, src).into()
}

pub fn compute_spf_partial(graph: &Graph, src: u32, targets: &BTreeSet<u32>) -> SpfSingleResult {
    let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
    let mut first_hop: BTreeMap<u32, u32> = BTreeMap::new();
    let mut settled: BTreeSet<u32> = BTreeSet::new();
    let mut pending = targets.clone();
    pending.remove(&src);

    let mut frontier = DistanceFrontier::new();
    dist.insert(src, 0.0);
    frontier.push(src, 0.0);

    loop {
        if pending.is_empty() {
            break;
        }
        let Some((u, cost_u)) = frontier.pop_min(|node, cost| {
            if settled.contains(&node) {
                return true;
            }
            match dist.get(&node).copied() {
                Some(best) => is_stale(best, cost),
                None => true,
            }
        }) else {
            break;
        };

        settled.insert(u);
        pending.remove(&u);

        if let Some(neighbors) = graph.get(&u) {
            for (v, edge_cost) in neighbors {
                if !edge_cost_supported(*edge_cost) {
                    continue;
                }
                let candidate_metric = cost_u + *edge_cost;
                let candidate_hop = if u == src {
                    *v
                } else {
                    *first_hop.get(&u).unwrap_or(v)
                };
                let best_metric = dist.get(v).copied().unwrap_or(f64::INFINITY);
                let best_hop = first_hop.get(v).copied().unwrap_or(u32::MAX);

                let better_metric = candidate_metric + EPS < best_metric;
                let equal_metric = (candidate_metric - best_metric).abs() <= EPS;
                let better_hop = equal_metric && candidate_hop < best_hop;
                if better_metric || better_hop {
                    dist.insert(*v, candidate_metric);
                    first_hop.insert(*v, candidate_hop);
                    frontier.push(*v, candidate_metric);
                    settled.remove(v);
                }
            }
        }
    }

    SpfSingleResult { dist, first_hop }
}

pub fn compute_spf_ecmp(graph: &Graph, src: u32) -> SpfEcmpResult {
    let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
    let mut first_hops: BTreeMap<u32, BTreeSet<u32>> = BTreeMap::new();
    let mut settled: BTreeSet<u32> = BTreeSet::new();
    let mut frontier = DistanceFrontier::new();

    dist.insert(src, 0.0);
    first_hops.insert(src, BTreeSet::new());
    frontier.push(src, 0.0);

    loop {
        let Some((u, cost_u)) = frontier.pop_min(|node, cost| {
            if settled.contains(&node) {
                return true;
            }
            match dist.get(&node).copied() {
                Some(best) => is_stale(best, cost),
                None => true,
            }
        }) else {
            break;
        };

        settled.insert(u);

        if let Some(neighbors) = graph.get(&u) {
            for (v, edge_cost) in neighbors {
                if !edge_cost_supported(*edge_cost) {
                    continue;
                }

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
                if candidate_metric + EPS < best {
                    dist.insert(*v, candidate_metric);
                    first_hops.insert(*v, candidate_hops);
                    frontier.push(*v, candidate_metric);
                    settled.remove(v);
                    continue;
                }

                if (candidate_metric - best).abs() <= EPS {
                    let entry = first_hops.entry(*v).or_default();
                    entry.extend(candidate_hops);
                }
            }
        }
    }

    SpfEcmpResult { dist, first_hops }
}

fn build_children(parent: &BTreeMap<u32, u32>) -> BTreeMap<u32, BTreeSet<u32>> {
    let mut children: BTreeMap<u32, BTreeSet<u32>> = BTreeMap::new();
    for (child, p) in parent {
        children.entry(*p).or_default().insert(*child);
    }
    children
}

fn collect_subtree(root: u32, children: &BTreeMap<u32, BTreeSet<u32>>, out: &mut BTreeSet<u32>) {
    if !out.insert(root) {
        return;
    }
    if let Some(kids) = children.get(&root) {
        for child in kids {
            collect_subtree(*child, children, out);
        }
    }
}

fn build_reverse_graph(graph: &Graph) -> BTreeMap<u32, Vec<(u32, f64)>> {
    let mut reverse: BTreeMap<u32, Vec<(u32, f64)>> = BTreeMap::new();
    for (u, neighbors) in graph {
        for (v, cost) in neighbors {
            reverse.entry(*v).or_default().push((*u, *cost));
        }
    }
    reverse
}

fn is_increase_or_removal(old_cost: Option<f64>, new_cost: Option<f64>) -> bool {
    match (
        old_cost.filter(|c| c.is_finite()),
        new_cost.filter(|c| c.is_finite()),
    ) {
        (Some(old), Some(new)) => new > old + EPS,
        (Some(_), None) => true,
        _ => false,
    }
}

pub fn compute_incremental_spf(
    graph: &Graph,
    src: u32,
    previous: &SpfTreeResult,
    updates: &[EdgeUpdate],
) -> IncrementalSpfResult {
    if updates.is_empty() {
        return IncrementalSpfResult {
            tree: previous.clone(),
            affected_nodes: BTreeSet::new(),
            used_full_recompute: false,
        };
    }

    let mut affected_nodes = BTreeSet::new();
    let children = build_children(&previous.parent);

    for update in updates {
        affected_nodes.insert(update.from);
        affected_nodes.insert(update.to);

        if is_increase_or_removal(update.old_cost, update.new_cost) {
            if previous.parent.get(&update.to).copied() == Some(update.from) {
                collect_subtree(update.to, &children, &mut affected_nodes);
            }
            if previous.parent.get(&update.from).copied() == Some(update.to) {
                collect_subtree(update.from, &children, &mut affected_nodes);
            }
        }
    }

    let node_count = collect_nodes(graph).len().max(1);
    if affected_nodes.len().saturating_mul(2) >= node_count {
        return IncrementalSpfResult {
            tree: compute_spf_tree(graph, src),
            affected_nodes,
            used_full_recompute: true,
        };
    }

    let mut dist = previous.dist.clone();
    let mut first_hop = previous.first_hop.clone();
    let mut parent = previous.parent.clone();

    for node in &affected_nodes {
        if *node == src {
            continue;
        }
        dist.remove(node);
        first_hop.remove(node);
        parent.remove(node);
    }

    dist.insert(src, 0.0);
    first_hop.remove(&src);
    parent.remove(&src);

    let reverse = build_reverse_graph(graph);
    let mut frontier = DistanceFrontier::new();

    if affected_nodes.contains(&src) {
        frontier.push(src, 0.0);
    }

    for node in affected_nodes.iter().copied().filter(|n| *n != src) {
        let mut best_metric = f64::INFINITY;
        let mut best_parent = u32::MAX;
        let mut best_hop = u32::MAX;

        if let Some(incoming) = reverse.get(&node) {
            for (pred, edge_cost) in incoming {
                if !edge_cost_supported(*edge_cost) {
                    continue;
                }
                let Some(pred_dist) = dist.get(pred).copied() else {
                    continue;
                };
                if !pred_dist.is_finite() {
                    continue;
                }

                let candidate_metric = pred_dist + *edge_cost;
                let candidate_hop = if *pred == src {
                    node
                } else {
                    match first_hop.get(pred).copied() {
                        Some(hop) => hop,
                        None => continue,
                    }
                };

                let better_metric = candidate_metric + EPS < best_metric;
                let equal_metric = (candidate_metric - best_metric).abs() <= EPS;
                let better_hop = equal_metric && candidate_hop < best_hop;
                let better_parent =
                    equal_metric && candidate_hop == best_hop && *pred < best_parent;

                if better_metric || better_hop || better_parent {
                    best_metric = candidate_metric;
                    best_parent = *pred;
                    best_hop = candidate_hop;
                }
            }
        }

        if best_metric.is_finite() {
            dist.insert(node, best_metric);
            first_hop.insert(node, best_hop);
            parent.insert(node, best_parent);
            frontier.push(node, best_metric);
        }
    }

    loop {
        let Some((u, cost_u)) = frontier.pop_min(|node, cost| match dist.get(&node).copied() {
            Some(best) => is_stale(best, cost),
            None => true,
        }) else {
            break;
        };

        if let Some(neighbors) = graph.get(&u) {
            for (v, edge_cost) in neighbors {
                if !edge_cost_supported(*edge_cost) {
                    continue;
                }
                let candidate_metric = cost_u + *edge_cost;
                let candidate_hop = if u == src {
                    *v
                } else {
                    match first_hop.get(&u).copied() {
                        Some(hop) => hop,
                        None => continue,
                    }
                };

                let best_metric = dist.get(v).copied().unwrap_or(f64::INFINITY);
                let best_hop = first_hop.get(v).copied().unwrap_or(u32::MAX);
                let best_parent = parent.get(v).copied().unwrap_or(u32::MAX);

                let better_metric = candidate_metric + EPS < best_metric;
                let equal_metric = (candidate_metric - best_metric).abs() <= EPS;
                let better_hop = equal_metric && candidate_hop < best_hop;
                let better_parent = equal_metric && candidate_hop == best_hop && u < best_parent;

                if better_metric || better_hop || better_parent {
                    dist.insert(*v, candidate_metric);
                    first_hop.insert(*v, candidate_hop);
                    parent.insert(*v, u);
                    frontier.push(*v, candidate_metric);
                }
            }
        }
    }

    IncrementalSpfResult {
        tree: SpfTreeResult {
            dist,
            first_hop,
            parent,
        },
        affected_nodes,
        used_full_recompute: false,
    }
}

pub fn compute_lfa_backup_next_hops(
    graph: &Graph,
    src: u32,
    destination: u32,
    primary_next_hops: &BTreeSet<u32>,
) -> Vec<LfaCandidate> {
    let src_spf = compute_spf_single(graph, src);
    let Some(dist_src_to_dst) = src_spf.dist.get(&destination).copied() else {
        return Vec::new();
    };

    let Some(neighbors) = graph.get(&src) else {
        return Vec::new();
    };

    let mut backups = Vec::new();
    for (neighbor, edge_cost) in neighbors {
        if !edge_cost_supported(*edge_cost) {
            continue;
        }
        if primary_next_hops.contains(neighbor) {
            continue;
        }

        let n_spf = compute_spf_single(graph, *neighbor);
        let Some(dist_n_to_dst) = n_spf.dist.get(&destination).copied() else {
            continue;
        };
        let Some(dist_n_to_src) = n_spf.dist.get(&src).copied() else {
            continue;
        };

        if dist_n_to_dst + EPS < dist_n_to_src + dist_src_to_dst {
            backups.push(LfaCandidate {
                next_hop: *neighbor,
                total_cost: *edge_cost + dist_n_to_dst,
            });
        }
    }

    backups.sort_by(|a, b| {
        a.total_cost
            .total_cmp(&b.total_cost)
            .then_with(|| a.next_hop.cmp(&b.next_hop))
    });
    backups
}

#[cfg(test)]
mod tests {
    use std::collections::{BTreeMap, BTreeSet};

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
    fn partial_spf_returns_requested_targets() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 5.0)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);
        let targets = BTreeSet::from([4]);

        let out = compute_spf_partial(&graph, 1, &targets);
        assert_eq!(out.dist.get(&4).copied(), Some(2.0));
        assert_eq!(out.first_hop.get(&4).copied(), Some(2));
    }

    #[test]
    fn incremental_spf_recomputes_subtree_on_tree_edge_increase() {
        let old_graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 4.0)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);
        let new_graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 10.0), (3, 4.0)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);

        let prev = compute_spf_tree(&old_graph, 1);
        let updated = compute_incremental_spf(
            &new_graph,
            1,
            &prev,
            &[EdgeUpdate {
                from: 1,
                to: 2,
                old_cost: Some(1.0),
                new_cost: Some(10.0),
            }],
        );

        assert_eq!(updated.tree.first_hop.get(&4).copied(), Some(3));
        assert_eq!(updated.tree.dist.get(&4).copied(), Some(5.0));
        assert!(updated.affected_nodes.contains(&2));
    }

    #[test]
    fn lfa_returns_loop_free_backup_next_hop() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 1.0)])),
            (2, BTreeMap::from([(1, 1.0), (4, 1.0)])),
            (3, BTreeMap::from([(1, 1.0), (4, 1.0)])),
            (4, BTreeMap::from([(2, 1.0), (3, 1.0)])),
        ]);

        let backups = compute_lfa_backup_next_hops(&graph, 1, 4, &BTreeSet::from([2]));
        assert!(!backups.is_empty());
        assert_eq!(backups[0].next_hop, 3);
    }
}
