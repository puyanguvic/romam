use std::collections::BTreeMap;

use crate::model::messages::ControlMessage;
use crate::model::routing::Route;
use serde_json::Value;

#[derive(Debug, Clone)]
pub struct RouterLink {
    pub neighbor_id: u32,
    pub cost: f64,
    pub address: String,
    pub port: u16,
    pub interface_name: Option<String>,
    pub is_up: bool,
}

#[derive(Debug, Clone)]
pub struct ProtocolContext {
    pub router_id: u32,
    pub now: f64,
    pub links: BTreeMap<u32, RouterLink>,
}

#[derive(Debug, Default, Clone)]
pub struct ProtocolOutputs {
    pub outbound: Vec<(u32, ControlMessage)>,
    pub routes: Option<Vec<Route>>,
}

pub trait ProtocolEngine: Send {
    fn name(&self) -> &'static str;
    fn start(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs;
    fn on_timer(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs;
    fn on_message(&mut self, ctx: &ProtocolContext, message: &ControlMessage) -> ProtocolOutputs;
    fn metrics(&self) -> BTreeMap<String, Value> {
        BTreeMap::new()
    }
}
