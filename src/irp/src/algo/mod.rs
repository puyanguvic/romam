use crate::model::routing::Route;

#[derive(Debug, Clone)]
pub struct DecisionContext {
    pub router_id: u32,
    pub protocol: String,
    pub now: f64,
}

pub trait DecisionEngine: Send + Sync {
    fn choose_routes(&self, ctx: &DecisionContext, protocol_routes: &[Route]) -> Vec<Route>;
}

#[derive(Debug, Default)]
pub struct PassthroughDecisionEngine;

impl DecisionEngine for PassthroughDecisionEngine {
    fn choose_routes(&self, _ctx: &DecisionContext, protocol_routes: &[Route]) -> Vec<Route> {
        protocol_routes.to_vec()
    }
}
