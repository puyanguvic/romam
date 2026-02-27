use std::collections::{BTreeMap, BTreeSet};

pub type Graph = BTreeMap<u32, BTreeMap<u32, f64>>;
pub type MultiMetricGraph = BTreeMap<u32, BTreeMap<u32, LinkMetrics>>;

#[derive(Debug, Clone)]
pub struct PathCandidate {
    pub nodes: Vec<u32>,
    pub cost: f64,
}

#[derive(Debug, Clone)]
pub struct SpfSingleResult {
    pub dist: BTreeMap<u32, f64>,
    pub first_hop: BTreeMap<u32, u32>,
}

#[derive(Debug, Clone)]
pub struct SpfTreeResult {
    pub dist: BTreeMap<u32, f64>,
    pub first_hop: BTreeMap<u32, u32>,
    pub parent: BTreeMap<u32, u32>,
}

impl From<SpfTreeResult> for SpfSingleResult {
    fn from(value: SpfTreeResult) -> Self {
        Self {
            dist: value.dist,
            first_hop: value.first_hop,
        }
    }
}

#[derive(Debug, Clone)]
pub struct SpfEcmpResult {
    pub dist: BTreeMap<u32, f64>,
    pub first_hops: BTreeMap<u32, BTreeSet<u32>>,
}

#[derive(Debug, Clone, Copy)]
pub struct EdgeUpdate {
    pub from: u32,
    pub to: u32,
    pub old_cost: Option<f64>,
    pub new_cost: Option<f64>,
}

#[derive(Debug, Clone)]
pub struct IncrementalSpfResult {
    pub tree: SpfTreeResult,
    pub affected_nodes: BTreeSet<u32>,
    pub used_full_recompute: bool,
}

#[derive(Debug, Clone)]
pub struct BellmanFordResult {
    pub dist: BTreeMap<u32, f64>,
    pub predecessor: BTreeMap<u32, u32>,
    pub negative_cycle_nodes: BTreeSet<u32>,
}

impl BellmanFordResult {
    pub fn has_negative_cycle(&self) -> bool {
        !self.negative_cycle_nodes.is_empty()
    }
}

#[derive(Debug, Clone, Copy)]
pub struct LinkMetrics {
    pub weight: f64,
    pub bandwidth: f64,
    pub delay: f64,
    pub loss: f64,
    pub utilization: f64,
}

#[derive(Debug, Clone, Copy, Default)]
pub struct LinkConstraints {
    pub min_bandwidth: Option<f64>,
    pub max_delay: Option<f64>,
    pub max_loss: Option<f64>,
    pub max_utilization: Option<f64>,
}

#[derive(Debug, Clone, Copy)]
pub struct WeightedSumCoefficients {
    pub weight: f64,
    pub delay: f64,
    pub loss: f64,
    pub utilization: f64,
}

impl Default for WeightedSumCoefficients {
    fn default() -> Self {
        Self {
            weight: 1.0,
            delay: 0.0,
            loss: 0.0,
            utilization: 0.0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct MultiObjectiveCost {
    pub delay: f64,
    pub loss: f64,
    pub utilization: f64,
}

impl MultiObjectiveCost {
    pub fn zero() -> Self {
        Self {
            delay: 0.0,
            loss: 0.0,
            utilization: 0.0,
        }
    }

    pub fn add_link(self, metrics: LinkMetrics) -> Self {
        Self {
            delay: self.delay + metrics.delay,
            loss: self.loss + metrics.loss,
            utilization: self.utilization + metrics.utilization,
        }
    }

    pub fn dominates(self, other: Self) -> bool {
        let no_worse = self.delay <= other.delay
            && self.loss <= other.loss
            && self.utilization <= other.utilization;
        let strictly_better = self.delay < other.delay
            || self.loss < other.loss
            || self.utilization < other.utilization;
        no_worse && strictly_better
    }
}

#[derive(Debug, Clone)]
pub struct ParetoPath {
    pub nodes: Vec<u32>,
    pub objective: MultiObjectiveCost,
}

#[derive(Debug, Clone, Copy)]
pub struct LfaCandidate {
    pub next_hop: u32,
    pub total_cost: f64,
}

#[derive(Debug, Clone)]
pub struct DvComputeInput {
    pub router_id: u32,
    pub infinity_metric: f64,
    pub link_costs: BTreeMap<u32, f64>,
    pub neighbor_vectors: BTreeMap<u32, BTreeMap<u32, f64>>,
}

pub type DvCandidates = BTreeMap<u32, (f64, u32)>;

#[derive(Debug, Default, Clone, Copy)]
pub struct DvRouteComputeEngine;

#[derive(Debug, Clone)]
pub struct NeighborRootTree {
    pub root: u32,
    pub link_cost: f64,
    pub dist: BTreeMap<u32, f64>,
    pub prev: BTreeMap<u32, u32>,
}

#[derive(Debug, Clone)]
pub struct NeighborRootForestInput {
    pub source_router_id: u32,
    pub graph: Graph,
    pub root_link_costs: BTreeMap<u32, f64>,
}

#[derive(Debug, Default, Clone, Copy)]
pub struct NeighborRootForestComputeEngine;
