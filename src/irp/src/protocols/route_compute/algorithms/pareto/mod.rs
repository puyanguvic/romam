use std::collections::BTreeMap;

use super::super::{LinkMetrics, MultiMetricGraph, MultiObjectiveCost, ParetoPath};

#[derive(Debug, Clone)]
struct ParetoLabel {
    node: u32,
    path: Vec<u32>,
    objective: MultiObjectiveCost,
}

fn objective_lex_order(a: MultiObjectiveCost, b: MultiObjectiveCost) -> std::cmp::Ordering {
    a.delay
        .total_cmp(&b.delay)
        .then_with(|| a.loss.total_cmp(&b.loss))
        .then_with(|| a.utilization.total_cmp(&b.utilization))
}

fn metric_edge_usable(metrics: LinkMetrics) -> bool {
    if !metrics.weight.is_finite()
        || !metrics.bandwidth.is_finite()
        || !metrics.delay.is_finite()
        || !metrics.loss.is_finite()
        || !metrics.utilization.is_finite()
    {
        return false;
    }
    metrics.weight >= 0.0
        && metrics.bandwidth >= 0.0
        && metrics.delay >= 0.0
        && metrics.loss >= 0.0
        && metrics.utilization >= 0.0
}

fn dominated_by_set(cost: MultiObjectiveCost, set: &[MultiObjectiveCost]) -> bool {
    set.iter()
        .any(|known| known.dominates(cost) || *known == cost)
}

fn insert_non_dominated(costs: &mut Vec<MultiObjectiveCost>, candidate: MultiObjectiveCost) {
    if dominated_by_set(candidate, costs) {
        return;
    }
    costs.retain(|old| !candidate.dominates(*old));
    costs.push(candidate);
    costs.sort_by(|a, b| objective_lex_order(*a, *b));
}

pub fn compute_pareto_shortest_paths(
    graph: &MultiMetricGraph,
    src: u32,
    dst: u32,
    max_paths: usize,
) -> Vec<ParetoPath> {
    if max_paths == 0 {
        return Vec::new();
    }

    let max_hops = graph.len().max(2);
    let mut frontier = vec![ParetoLabel {
        node: src,
        path: vec![src],
        objective: MultiObjectiveCost::zero(),
    }];

    let mut labels_per_node: BTreeMap<u32, Vec<MultiObjectiveCost>> = BTreeMap::new();
    let mut solutions: Vec<ParetoPath> = Vec::new();
    let expand_limit = graph
        .len()
        .saturating_mul(graph.len().max(2))
        .saturating_mul(max_paths.max(2));
    let mut expanded = 0usize;

    while !frontier.is_empty() && expanded <= expand_limit {
        let best_idx = frontier
            .iter()
            .enumerate()
            .min_by(|(_, a), (_, b)| {
                objective_lex_order(a.objective, b.objective).then_with(|| a.path.cmp(&b.path))
            })
            .map(|(idx, _)| idx)
            .unwrap_or(0);

        let label = frontier.swap_remove(best_idx);

        let per_node = labels_per_node.entry(label.node).or_default();
        if dominated_by_set(label.objective, per_node) {
            expanded += 1;
            continue;
        }
        insert_non_dominated(per_node, label.objective);

        if label.node == dst {
            solutions.push(ParetoPath {
                nodes: label.path,
                objective: label.objective,
            });
            expanded += 1;
            continue;
        }

        if label.path.len() >= max_hops {
            expanded += 1;
            continue;
        }

        if let Some(neighbors) = graph.get(&label.node) {
            for (next, metrics) in neighbors {
                if !metric_edge_usable(*metrics) {
                    continue;
                }
                if label.path.contains(next) {
                    continue;
                }
                let next_objective = label.objective.add_link(*metrics);
                let dominated = labels_per_node
                    .get(next)
                    .map(|set| dominated_by_set(next_objective, set))
                    .unwrap_or(false);
                if dominated {
                    continue;
                }

                let mut next_path = label.path.clone();
                next_path.push(*next);
                frontier.push(ParetoLabel {
                    node: *next,
                    path: next_path,
                    objective: next_objective,
                });
            }
        }
        expanded += 1;
    }

    let mut dedup: BTreeMap<Vec<u32>, MultiObjectiveCost> = BTreeMap::new();
    for sol in solutions {
        dedup
            .entry(sol.nodes)
            .and_modify(|existing| {
                if objective_lex_order(sol.objective, *existing).is_lt() {
                    *existing = sol.objective;
                }
            })
            .or_insert(sol.objective);
    }

    let out: Vec<ParetoPath> = dedup
        .into_iter()
        .map(|(nodes, objective)| ParetoPath { nodes, objective })
        .collect();

    let mut filtered = Vec::new();
    for candidate in &out {
        let dominated = out.iter().any(|other| {
            other.nodes != candidate.nodes && other.objective.dominates(candidate.objective)
        });
        if !dominated {
            filtered.push(candidate.clone());
        }
    }

    filtered.sort_by(|a, b| {
        objective_lex_order(a.objective, b.objective).then_with(|| a.nodes.cmp(&b.nodes))
    });
    filtered.truncate(max_paths);

    filtered
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;

    fn mm(weight: f64, bw: f64, delay: f64, loss: f64, util: f64) -> LinkMetrics {
        LinkMetrics {
            weight,
            bandwidth: bw,
            delay,
            loss,
            utilization: util,
        }
    }

    #[test]
    fn pareto_keeps_non_dominated_paths() {
        let graph: MultiMetricGraph = BTreeMap::from([
            (
                1,
                BTreeMap::from([
                    (2, mm(1.0, 10.0, 2.0, 0.3, 0.4)),
                    (3, mm(1.0, 10.0, 3.0, 0.1, 0.3)),
                ]),
            ),
            (2, BTreeMap::from([(4, mm(1.0, 10.0, 2.0, 0.3, 0.4))])),
            (3, BTreeMap::from([(4, mm(1.0, 10.0, 3.0, 0.1, 0.3))])),
            (4, BTreeMap::new()),
        ]);

        let out = compute_pareto_shortest_paths(&graph, 1, 4, 4);
        assert_eq!(out.len(), 2);
        assert!(out.iter().any(|p| p.nodes == vec![1, 2, 4]));
        assert!(out.iter().any(|p| p.nodes == vec![1, 3, 4]));
    }
}
