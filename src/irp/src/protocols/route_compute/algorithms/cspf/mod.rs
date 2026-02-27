use std::collections::BTreeMap;

use super::super::{Graph, LinkConstraints, LinkMetrics, MultiMetricGraph, SpfSingleResult};
use super::dijkstra::compute_spf_single;

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
}
