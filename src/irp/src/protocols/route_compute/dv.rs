use std::collections::BTreeMap;

use super::{DvCandidates, DvComputeInput, DvRouteComputeEngine, RouteComputeEngine};

impl RouteComputeEngine for DvRouteComputeEngine {
    type Input = DvComputeInput;
    type Output = DvCandidates;

    fn compute(&self, input: &Self::Input) -> Self::Output {
        compute_distance_vector_candidates(input)
    }
}

pub fn compute_distance_vector_candidates(input: &DvComputeInput) -> DvCandidates {
    let mut candidates: DvCandidates = BTreeMap::new();

    for (neighbor_id, link_cost) in &input.link_costs {
        if !link_cost.is_finite() || *link_cost < 0.0 {
            continue;
        }
        candidates.insert(*neighbor_id, (*link_cost, *neighbor_id));
    }

    for (neighbor_id, vector) in &input.neighbor_vectors {
        let Some(base) = input.link_costs.get(neighbor_id).copied() else {
            continue;
        };
        if !base.is_finite() || base < 0.0 {
            continue;
        }
        for (destination, advertised_metric) in vector {
            if *destination == input.router_id {
                continue;
            }
            if !advertised_metric.is_finite() || *advertised_metric < 0.0 {
                continue;
            }

            let total_metric = input.infinity_metric.min(base + *advertised_metric);
            if total_metric >= input.infinity_metric {
                continue;
            }

            let proposal = (total_metric, *neighbor_id);
            match candidates.get(destination) {
                None => {
                    candidates.insert(*destination, proposal);
                }
                Some(current) if proposal < *current => {
                    candidates.insert(*destination, proposal);
                }
                _ => {}
            }
        }
    }

    candidates
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;

    #[test]
    fn dv_candidates_prefer_lower_metric_then_next_hop() {
        let input = DvComputeInput {
            router_id: 1,
            infinity_metric: 16.0,
            link_costs: BTreeMap::from([(2, 1.0), (3, 1.0)]),
            neighbor_vectors: BTreeMap::from([
                (2, BTreeMap::from([(4, 2.0)])),
                (3, BTreeMap::from([(4, 2.0)])),
            ]),
        };

        let result = compute_distance_vector_candidates(&input);
        assert_eq!(result.get(&4).copied(), Some((3.0, 2)));
    }
}
