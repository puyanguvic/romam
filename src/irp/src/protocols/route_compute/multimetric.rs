use std::collections::BTreeMap;

use super::{
    compute_spf_single, Graph, LinkConstraints, LinkMetrics, MultiMetricGraph, MultiObjectiveCost,
    ParetoPath, SpfSingleResult, WeightedSumCoefficients,
};

fn metric_edge_usable(metrics: LinkMetrics, constraints: LinkConstraints) -> bool {
    if !metrics.weight.is_finite()
        || !metrics.bandwidth.is_finite()
        || !metrics.delay.is_finite()
        || !metrics.loss.is_finite()
        || !metrics.utilization.is_finite()
    {
        return false;
    }
    if metrics.weight < 0.0
        || metrics.bandwidth < 0.0
        || metrics.delay < 0.0
        || metrics.loss < 0.0
        || metrics.utilization < 0.0
    {
        return false;
    }
    if let Some(min_bw) = constraints.min_bandwidth {
        if metrics.bandwidth < min_bw {
            return false;
        }
    }
    if let Some(max_delay) = constraints.max_delay {
        if metrics.delay > max_delay {
            return false;
        }
    }
    if let Some(max_loss) = constraints.max_loss {
        if metrics.loss > max_loss {
            return false;
        }
    }
    if let Some(max_utilization) = constraints.max_utilization {
        if metrics.utilization > max_utilization {
            return false;
        }
    }
    true
}

pub fn build_weight_graph_from_constraints(
    graph: &MultiMetricGraph,
    constraints: LinkConstraints,
) -> Graph {
    let mut filtered: Graph = BTreeMap::new();
    for (u, neighbors) in graph {
        let mut out = BTreeMap::new();
        for (v, metrics) in neighbors {
            if metric_edge_usable(*metrics, constraints) {
                out.insert(*v, metrics.weight);
            }
        }
        filtered.insert(*u, out);
    }
    filtered
}

pub fn compute_cspf(
    graph: &MultiMetricGraph,
    src: u32,
    constraints: LinkConstraints,
) -> SpfSingleResult {
    let filtered = build_weight_graph_from_constraints(graph, constraints);
    compute_spf_single(&filtered, src)
}

pub fn compute_weighted_sum_shortest_path(
    graph: &MultiMetricGraph,
    src: u32,
    coefficients: WeightedSumCoefficients,
) -> SpfSingleResult {
    let mut scalar_graph: Graph = BTreeMap::new();
    for (u, neighbors) in graph {
        let mut out = BTreeMap::new();
        for (v, metrics) in neighbors {
            if !metric_edge_usable(*metrics, LinkConstraints::default()) {
                continue;
            }
            let score = coefficients.weight * metrics.weight
                + coefficients.delay * metrics.delay
                + coefficients.loss * metrics.loss
                + coefficients.utilization * metrics.utilization;
            if score.is_finite() && score >= 0.0 {
                out.insert(*v, score);
            }
        }
        scalar_graph.insert(*u, out);
    }

    compute_spf_single(&scalar_graph, src)
}

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
                objective_lex_order(a.objective, b.objective)
                    .then_with(|| compare_path_candidate_path(&a.path, &b.path))
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
                if !metric_edge_usable(*metrics, LinkConstraints::default()) {
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

    let mut out: Vec<ParetoPath> = dedup
        .into_iter()
        .map(|(nodes, objective)| ParetoPath { nodes, objective })
        .collect();

    // Keep only globally non-dominated solutions.
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
        objective_lex_order(a.objective, b.objective)
            .then_with(|| compare_path_candidate_path(&a.nodes, &b.nodes))
    });
    filtered.truncate(max_paths);
    out.clear();

    filtered
}

fn compare_path_candidate_path(a: &[u32], b: &[u32]) -> std::cmp::Ordering {
    a.cmp(b)
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
    fn cspf_filters_edges_before_spf() {
        let graph: MultiMetricGraph = BTreeMap::from([
            (
                1,
                BTreeMap::from([
                    (2, mm(1.0, 20.0, 5.0, 0.01, 0.2)),
                    (3, mm(1.0, 5.0, 1.0, 0.01, 0.1)),
                ]),
            ),
            (2, BTreeMap::from([(4, mm(1.0, 20.0, 5.0, 0.01, 0.2))])),
            (3, BTreeMap::from([(4, mm(1.0, 5.0, 1.0, 0.01, 0.1))])),
            (4, BTreeMap::new()),
        ]);

        let result = compute_cspf(
            &graph,
            1,
            LinkConstraints {
                min_bandwidth: Some(10.0),
                ..Default::default()
            },
        );

        assert_eq!(result.first_hop.get(&4).copied(), Some(2));
        assert_eq!(result.dist.get(&4).copied(), Some(2.0));
    }

    #[test]
    fn weighted_sum_can_flip_best_path() {
        let graph: MultiMetricGraph = BTreeMap::from([
            (
                1,
                BTreeMap::from([
                    (2, mm(1.0, 10.0, 9.0, 0.01, 0.8)),
                    (3, mm(1.0, 10.0, 3.0, 0.01, 0.2)),
                ]),
            ),
            (2, BTreeMap::from([(4, mm(1.0, 10.0, 9.0, 0.01, 0.8))])),
            (3, BTreeMap::from([(4, mm(1.0, 10.0, 3.0, 0.01, 0.2))])),
            (4, BTreeMap::new()),
        ]);

        let by_weight = compute_weighted_sum_shortest_path(
            &graph,
            1,
            WeightedSumCoefficients {
                weight: 1.0,
                delay: 0.0,
                loss: 0.0,
                utilization: 0.0,
            },
        );
        assert_eq!(by_weight.first_hop.get(&4).copied(), Some(2));

        let by_delay = compute_weighted_sum_shortest_path(
            &graph,
            1,
            WeightedSumCoefficients {
                weight: 0.0,
                delay: 1.0,
                loss: 0.0,
                utilization: 0.0,
            },
        );
        assert_eq!(by_delay.first_hop.get(&4).copied(), Some(3));
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
