use std::collections::BTreeMap;

use super::super::{
    Graph, LinkMetrics, MultiMetricGraph, SpfSingleResult, WeightedSumCoefficients,
};
use super::dijkstra::compute_spf_single;

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

pub fn compute_weighted_sum_shortest_path(
    graph: &MultiMetricGraph,
    src: u32,
    coefficients: WeightedSumCoefficients,
) -> SpfSingleResult {
    let mut scalar_graph: Graph = BTreeMap::new();
    for (u, neighbors) in graph {
        let mut out = BTreeMap::new();
        for (v, metrics) in neighbors {
            if !metric_edge_usable(*metrics) {
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
}
