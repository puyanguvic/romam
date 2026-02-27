use std::collections::{BTreeMap, BTreeSet};

use super::{
    compute_bellman_ford, compute_cspf, compute_pareto_shortest_paths, compute_spf_ecmp,
    compute_spf_single, compute_weighted_sum_shortest_path, k_shortest_simple_paths, Graph,
    LinkConstraints, MultiMetricGraph, WeightedSumCoefficients,
};

#[derive(Debug, Clone, Copy)]
pub enum NextHopSelectionPolicy {
    Lowest,
    Hash { seed: u64 },
}

#[derive(Debug, Clone)]
pub enum ScalarRouteAlgorithm {
    Dijkstra,
    Ecmp,
    BellmanFord,
    YenKShortest { k_paths: usize },
}

#[derive(Debug, Clone)]
pub struct ScalarRouteStrategyConfig {
    pub algorithm: ScalarRouteAlgorithm,
    pub selection: NextHopSelectionPolicy,
}

impl Default for ScalarRouteStrategyConfig {
    fn default() -> Self {
        Self {
            algorithm: ScalarRouteAlgorithm::Dijkstra,
            selection: NextHopSelectionPolicy::Lowest,
        }
    }
}

#[derive(Debug, Clone)]
pub enum MultiMetricRouteAlgorithm {
    Cspf {
        constraints: LinkConstraints,
    },
    WeightedSum {
        coefficients: WeightedSumCoefficients,
    },
    Pareto {
        max_paths: usize,
    },
}

#[derive(Debug, Clone)]
pub struct MultiMetricRouteStrategyConfig {
    pub algorithm: MultiMetricRouteAlgorithm,
    pub selection: NextHopSelectionPolicy,
}

#[derive(Debug, Clone)]
pub struct StrategyRouteEntry {
    pub destination: u32,
    pub metric: f64,
    pub next_hops: BTreeSet<u32>,
    pub selected_next_hop: u32,
}

fn hash_mix(src: u32, destination: u32, seed: u64) -> u64 {
    let mut x = seed.max(1);
    x ^= (src as u64).wrapping_mul(0x9E37_79B1_85EB_CA87);
    x = x.rotate_left(13);
    x ^= (destination as u64).wrapping_mul(0xC2B2_AE3D_27D4_EB4F);
    x ^= x >> 33;
    x = x.wrapping_mul(0xFF51_AFD7_ED55_8CCD);
    x ^= x >> 33;
    x
}

fn pick_selected_hop(
    src: u32,
    destination: u32,
    candidates: &[(u32, f64)],
    selection: NextHopSelectionPolicy,
) -> Option<(u32, f64)> {
    if candidates.is_empty() {
        return None;
    }

    match selection {
        NextHopSelectionPolicy::Lowest => candidates
            .iter()
            .copied()
            .min_by(|a, b| a.1.total_cmp(&b.1).then_with(|| a.0.cmp(&b.0))),
        NextHopSelectionPolicy::Hash { seed } => {
            let mut sorted = candidates.to_vec();
            sorted.sort_by(|a, b| a.0.cmp(&b.0));
            let idx = (hash_mix(src, destination, seed) as usize) % sorted.len();
            sorted.get(idx).copied()
        }
    }
}

fn collect_scalar_nodes(graph: &Graph) -> BTreeSet<u32> {
    let mut nodes = BTreeSet::new();
    for (u, neighbors) in graph {
        nodes.insert(*u);
        for v in neighbors.keys() {
            nodes.insert(*v);
        }
    }
    nodes
}

fn collect_multimetric_nodes(graph: &MultiMetricGraph) -> BTreeSet<u32> {
    let mut nodes = BTreeSet::new();
    for (u, neighbors) in graph {
        nodes.insert(*u);
        for v in neighbors.keys() {
            nodes.insert(*v);
        }
    }
    nodes
}

fn first_hop_from_predecessor(
    predecessor: &BTreeMap<u32, u32>,
    src: u32,
    destination: u32,
) -> Option<u32> {
    if destination == src {
        return None;
    }

    let mut current = destination;
    let max_steps = predecessor.len() + 1;
    for _ in 0..max_steps {
        let parent = predecessor.get(&current).copied()?;
        if parent == src {
            return Some(current);
        }
        current = parent;
    }

    None
}

pub fn compute_scalar_route_entries(
    graph: &Graph,
    src: u32,
    config: &ScalarRouteStrategyConfig,
) -> Vec<StrategyRouteEntry> {
    let mut out = Vec::new();

    match &config.algorithm {
        ScalarRouteAlgorithm::Dijkstra => {
            let result = compute_spf_single(graph, src);
            for (destination, metric) in result.dist {
                if destination == src || !metric.is_finite() {
                    continue;
                }
                let Some(next_hop) = result.first_hop.get(&destination).copied() else {
                    continue;
                };
                out.push(StrategyRouteEntry {
                    destination,
                    metric,
                    next_hops: BTreeSet::from([next_hop]),
                    selected_next_hop: next_hop,
                });
            }
        }
        ScalarRouteAlgorithm::Ecmp => {
            let result = compute_spf_ecmp(graph, src);
            for (destination, metric) in result.dist {
                if destination == src || !metric.is_finite() {
                    continue;
                }
                let Some(hops) = result.first_hops.get(&destination) else {
                    continue;
                };
                if hops.is_empty() {
                    continue;
                }
                let candidates: Vec<(u32, f64)> = hops.iter().map(|hop| (*hop, metric)).collect();
                let Some((selected_next_hop, selected_metric)) =
                    pick_selected_hop(src, destination, &candidates, config.selection)
                else {
                    continue;
                };
                out.push(StrategyRouteEntry {
                    destination,
                    metric: selected_metric,
                    next_hops: hops.clone(),
                    selected_next_hop,
                });
            }
        }
        ScalarRouteAlgorithm::BellmanFord => {
            let result = compute_bellman_ford(graph, src);
            for (destination, metric) in result.dist {
                if destination == src || !metric.is_finite() {
                    continue;
                }
                if result.negative_cycle_nodes.contains(&destination) {
                    continue;
                }
                let Some(next_hop) =
                    first_hop_from_predecessor(&result.predecessor, src, destination)
                else {
                    continue;
                };
                out.push(StrategyRouteEntry {
                    destination,
                    metric,
                    next_hops: BTreeSet::from([next_hop]),
                    selected_next_hop: next_hop,
                });
            }
        }
        ScalarRouteAlgorithm::YenKShortest { k_paths } => {
            let nodes = collect_scalar_nodes(graph);
            for destination in nodes {
                if destination == src {
                    continue;
                }
                let paths = k_shortest_simple_paths(graph, src, destination, *k_paths);
                if paths.is_empty() {
                    continue;
                }

                let mut per_hop_best: BTreeMap<u32, f64> = BTreeMap::new();
                for path in paths {
                    if path.nodes.len() < 2 || !path.cost.is_finite() {
                        continue;
                    }
                    let hop = path.nodes[1];
                    per_hop_best
                        .entry(hop)
                        .and_modify(|metric| {
                            if path.cost < *metric {
                                *metric = path.cost;
                            }
                        })
                        .or_insert(path.cost);
                }
                if per_hop_best.is_empty() {
                    continue;
                }

                let candidates: Vec<(u32, f64)> = per_hop_best
                    .iter()
                    .map(|(hop, metric)| (*hop, *metric))
                    .collect();
                let Some((selected_next_hop, selected_metric)) =
                    pick_selected_hop(src, destination, &candidates, config.selection)
                else {
                    continue;
                };

                out.push(StrategyRouteEntry {
                    destination,
                    metric: selected_metric,
                    next_hops: per_hop_best.keys().copied().collect(),
                    selected_next_hop,
                });
            }
        }
    }

    out.sort_by(|a, b| a.destination.cmp(&b.destination));
    out
}

pub fn compute_multimetric_route_entries(
    graph: &MultiMetricGraph,
    src: u32,
    config: &MultiMetricRouteStrategyConfig,
) -> Vec<StrategyRouteEntry> {
    let mut out = Vec::new();

    match &config.algorithm {
        MultiMetricRouteAlgorithm::Cspf { constraints } => {
            let result = compute_cspf(graph, src, *constraints);
            for (destination, metric) in result.dist {
                if destination == src || !metric.is_finite() {
                    continue;
                }
                let Some(next_hop) = result.first_hop.get(&destination).copied() else {
                    continue;
                };
                out.push(StrategyRouteEntry {
                    destination,
                    metric,
                    next_hops: BTreeSet::from([next_hop]),
                    selected_next_hop: next_hop,
                });
            }
        }
        MultiMetricRouteAlgorithm::WeightedSum { coefficients } => {
            let result = compute_weighted_sum_shortest_path(graph, src, *coefficients);
            for (destination, metric) in result.dist {
                if destination == src || !metric.is_finite() {
                    continue;
                }
                let Some(next_hop) = result.first_hop.get(&destination).copied() else {
                    continue;
                };
                out.push(StrategyRouteEntry {
                    destination,
                    metric,
                    next_hops: BTreeSet::from([next_hop]),
                    selected_next_hop: next_hop,
                });
            }
        }
        MultiMetricRouteAlgorithm::Pareto { max_paths } => {
            let nodes = collect_multimetric_nodes(graph);
            for destination in nodes {
                if destination == src {
                    continue;
                }

                let paths = compute_pareto_shortest_paths(graph, src, destination, *max_paths);
                if paths.is_empty() {
                    continue;
                }

                let mut per_hop_best: BTreeMap<u32, f64> = BTreeMap::new();
                for path in paths {
                    if path.nodes.len() < 2 {
                        continue;
                    }
                    let score =
                        path.objective.delay + path.objective.loss + path.objective.utilization;
                    let hop = path.nodes[1];
                    per_hop_best
                        .entry(hop)
                        .and_modify(|metric| {
                            if score < *metric {
                                *metric = score;
                            }
                        })
                        .or_insert(score);
                }

                if per_hop_best.is_empty() {
                    continue;
                }

                let candidates: Vec<(u32, f64)> = per_hop_best
                    .iter()
                    .map(|(hop, metric)| (*hop, *metric))
                    .collect();
                let Some((selected_next_hop, selected_metric)) =
                    pick_selected_hop(src, destination, &candidates, config.selection)
                else {
                    continue;
                };

                out.push(StrategyRouteEntry {
                    destination,
                    metric: selected_metric,
                    next_hops: per_hop_best.keys().copied().collect(),
                    selected_next_hop,
                });
            }
        }
    }

    out.sort_by(|a, b| a.destination.cmp(&b.destination));
    out
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;
    use crate::protocols::route_compute::LinkMetrics;

    #[test]
    fn scalar_ecmp_hash_selection_is_deterministic() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 1.0)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);

        let cfg = ScalarRouteStrategyConfig {
            algorithm: ScalarRouteAlgorithm::Ecmp,
            selection: NextHopSelectionPolicy::Hash { seed: 2026 },
        };

        let a = compute_scalar_route_entries(&graph, 1, &cfg);
        let b = compute_scalar_route_entries(&graph, 1, &cfg);
        let a4 = a
            .iter()
            .find(|entry| entry.destination == 4)
            .expect("route to 4");
        let b4 = b
            .iter()
            .find(|entry| entry.destination == 4)
            .expect("route to 4");

        assert_eq!(a4.selected_next_hop, b4.selected_next_hop);
        assert_eq!(a4.next_hops.len(), 2);
    }

    #[test]
    fn scalar_yen_k_shortest_collects_next_hop_set() {
        let graph: Graph = BTreeMap::from([
            (1, BTreeMap::from([(2, 1.0), (3, 1.0)])),
            (2, BTreeMap::from([(4, 1.0)])),
            (3, BTreeMap::from([(4, 1.0)])),
            (4, BTreeMap::new()),
        ]);

        let cfg = ScalarRouteStrategyConfig {
            algorithm: ScalarRouteAlgorithm::YenKShortest { k_paths: 2 },
            selection: NextHopSelectionPolicy::Lowest,
        };

        let out = compute_scalar_route_entries(&graph, 1, &cfg);
        let to_4 = out
            .iter()
            .find(|entry| entry.destination == 4)
            .expect("route to 4");

        assert_eq!(to_4.next_hops, BTreeSet::from([2, 3]));
        assert_eq!(to_4.selected_next_hop, 2);
    }

    #[test]
    fn multimetric_cspf_strategy_filters_low_bandwidth_path() {
        let graph: MultiMetricGraph = BTreeMap::from([
            (
                1,
                BTreeMap::from([
                    (
                        2,
                        LinkMetrics {
                            weight: 1.0,
                            bandwidth: 100.0,
                            delay: 10.0,
                            loss: 0.01,
                            utilization: 0.3,
                        },
                    ),
                    (
                        3,
                        LinkMetrics {
                            weight: 1.0,
                            bandwidth: 5.0,
                            delay: 1.0,
                            loss: 0.01,
                            utilization: 0.1,
                        },
                    ),
                ]),
            ),
            (
                2,
                BTreeMap::from([(
                    4,
                    LinkMetrics {
                        weight: 1.0,
                        bandwidth: 100.0,
                        delay: 10.0,
                        loss: 0.01,
                        utilization: 0.3,
                    },
                )]),
            ),
            (
                3,
                BTreeMap::from([(
                    4,
                    LinkMetrics {
                        weight: 1.0,
                        bandwidth: 5.0,
                        delay: 1.0,
                        loss: 0.01,
                        utilization: 0.1,
                    },
                )]),
            ),
            (4, BTreeMap::new()),
        ]);

        let cfg = MultiMetricRouteStrategyConfig {
            algorithm: MultiMetricRouteAlgorithm::Cspf {
                constraints: LinkConstraints {
                    min_bandwidth: Some(50.0),
                    ..Default::default()
                },
            },
            selection: NextHopSelectionPolicy::Lowest,
        };

        let out = compute_multimetric_route_entries(&graph, 1, &cfg);
        let to_4 = out
            .iter()
            .find(|entry| entry.destination == 4)
            .expect("route to 4");
        assert_eq!(to_4.selected_next_hop, 2);
    }
}
