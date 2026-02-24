use std::collections::{BTreeMap, BTreeSet};

use super::{
    Graph, NeighborRootForestComputeEngine, NeighborRootForestInput, NeighborRootTree,
    PathCandidate, RouteComputeEngine,
};

impl RouteComputeEngine for NeighborRootForestComputeEngine {
    type Input = NeighborRootForestInput;
    type Output = Vec<NeighborRootTree>;

    fn compute(&self, input: &Self::Input) -> Self::Output {
        build_neighbor_rooted_forest(&input.graph, input.source_router_id, &input.root_link_costs)
    }
}

pub fn dijkstra_without_source(
    graph: &Graph,
    root: u32,
    source: u32,
) -> (BTreeMap<u32, f64>, BTreeMap<u32, u32>) {
    let mut dist: BTreeMap<u32, f64> = BTreeMap::new();
    let mut prev: BTreeMap<u32, u32> = BTreeMap::new();
    let mut visited: BTreeSet<u32> = BTreeSet::new();

    if root == source || !graph.contains_key(&root) {
        return (dist, prev);
    }
    dist.insert(root, 0.0);

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
                if *v == source || !edge_cost.is_finite() || *edge_cost < 0.0 {
                    continue;
                }
                let candidate_metric = cost_u + *edge_cost;
                let best = dist.get(v).copied().unwrap_or(f64::INFINITY);
                let best_prev = prev.get(v).copied().unwrap_or(u32::MAX);
                let better = candidate_metric < best || (candidate_metric == best && u < best_prev);
                if better {
                    dist.insert(*v, candidate_metric);
                    prev.insert(*v, u);
                }
            }
        }
    }

    (dist, prev)
}

pub fn reconstruct_root_path(
    root: u32,
    destination: u32,
    prev: &BTreeMap<u32, u32>,
) -> Option<Vec<u32>> {
    if destination == root {
        return Some(vec![root]);
    }
    let mut reversed = vec![destination];
    let mut current = destination;
    let max_steps = prev.len() + 1;
    for _ in 0..max_steps {
        let parent = prev.get(&current).copied()?;
        reversed.push(parent);
        if parent == root {
            reversed.reverse();
            return Some(reversed);
        }
        current = parent;
    }
    None
}

pub fn build_neighbor_rooted_forest(
    graph: &Graph,
    source_router_id: u32,
    root_link_costs: &BTreeMap<u32, f64>,
) -> Vec<NeighborRootTree> {
    let mut trees = Vec::new();
    for (root, link_cost) in root_link_costs {
        if !link_cost.is_finite() || *link_cost < 0.0 {
            continue;
        }
        let (dist, prev) = dijkstra_without_source(graph, *root, source_router_id);
        trees.push(NeighborRootTree {
            root: *root,
            link_cost: *link_cost,
            dist,
            prev,
        });
    }
    trees
}

pub fn build_path_via_neighbor_root(
    source_router_id: u32,
    destination: u32,
    tree: &NeighborRootTree,
) -> Option<PathCandidate> {
    if destination == tree.root {
        return Some(PathCandidate {
            nodes: vec![source_router_id, tree.root],
            cost: tree.link_cost,
        });
    }
    let root_to_dst_cost = tree.dist.get(&destination).copied()?;
    let root_path = reconstruct_root_path(tree.root, destination, &tree.prev)?;
    if root_path.first().copied() != Some(tree.root) {
        return None;
    }
    if root_path.contains(&source_router_id) {
        return None;
    }
    let mut nodes = Vec::with_capacity(root_path.len() + 1);
    nodes.push(source_router_id);
    nodes.extend(root_path);
    Some(PathCandidate {
        nodes,
        cost: tree.link_cost + root_to_dst_cost,
    })
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;

    #[test]
    fn forest_path_builds_source_neighbor_destination_chain() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);
        let forest = build_neighbor_rooted_forest(&graph, 1, &BTreeMap::from([(2, 1.0)]));
        assert_eq!(forest.len(), 1);

        let path = build_path_via_neighbor_root(1, 4, &forest[0]).expect("path should exist");
        assert_eq!(path.nodes, vec![1, 2, 4]);
        assert_eq!(path.cost, 2.0);
    }
}
