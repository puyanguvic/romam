use std::collections::{BTreeMap, BTreeSet};

pub type Graph = BTreeMap<u32, BTreeMap<u32, f64>>;

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
pub struct SpfEcmpResult {
    pub dist: BTreeMap<u32, f64>,
    pub first_hops: BTreeMap<u32, BTreeSet<u32>>,
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
