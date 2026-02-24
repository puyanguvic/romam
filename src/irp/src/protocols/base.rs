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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SocketErrno {
    NoRouteToHost,
    InvalidArgument,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct InterfaceAddress {
    pub address: u32,
    pub prefix_len: u8,
}

#[derive(Debug, Clone, PartialEq)]
pub enum RouteInputResult {
    ForwardUnicast(Route),
    LocalDeliver,
    Error(SocketErrno),
    NotHandled,
}

pub trait Ipv4RoutingProtocol: Send {
    fn name(&self) -> &'static str;
    fn start(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs;
    fn on_timer(&mut self, ctx: &ProtocolContext) -> ProtocolOutputs;
    fn on_message(&mut self, ctx: &ProtocolContext, message: &ControlMessage) -> ProtocolOutputs;
    fn metrics(&self) -> BTreeMap<String, Value> {
        BTreeMap::new()
    }

    fn route_output(
        &self,
        _ctx: &ProtocolContext,
        destination: u32,
        oif: Option<u32>,
        rib: &[Route],
    ) -> Result<Route, SocketErrno> {
        if destination == 0 {
            return Err(SocketErrno::InvalidArgument);
        }
        rib.iter()
            .filter(|route| route.destination == destination)
            .filter(|route| oif.is_none() || route.entry.get_interface() == oif)
            .min_by(|left, right| {
                left.metric
                    .total_cmp(&right.metric)
                    .then_with(|| left.next_hop.cmp(&right.next_hop))
            })
            .cloned()
            .ok_or(SocketErrno::NoRouteToHost)
    }

    fn route_input(
        &self,
        ctx: &ProtocolContext,
        destination: u32,
        _ingress_interface: Option<u32>,
        rib: &[Route],
    ) -> RouteInputResult {
        if destination == ctx.router_id {
            return RouteInputResult::LocalDeliver;
        }
        match self.route_output(ctx, destination, None, rib) {
            Ok(route) => RouteInputResult::ForwardUnicast(route),
            Err(err) => RouteInputResult::Error(err),
        }
    }

    fn notify_interface_up(&mut self, _ctx: &ProtocolContext, _interface: u32) -> ProtocolOutputs {
        ProtocolOutputs::default()
    }

    fn notify_interface_down(
        &mut self,
        _ctx: &ProtocolContext,
        _interface: u32,
    ) -> ProtocolOutputs {
        ProtocolOutputs::default()
    }

    fn notify_add_address(
        &mut self,
        _ctx: &ProtocolContext,
        _interface: u32,
        _address: InterfaceAddress,
    ) -> ProtocolOutputs {
        ProtocolOutputs::default()
    }

    fn notify_remove_address(
        &mut self,
        _ctx: &ProtocolContext,
        _interface: u32,
        _address: InterfaceAddress,
    ) -> ProtocolOutputs {
        ProtocolOutputs::default()
    }

    fn set_ipv4_context(&mut self, _router_id: u32) {}

    fn print_routing_table(&self, routes: &[Route], now: f64) -> String {
        let mut lines = Vec::new();
        lines.push(format!(
            "protocol={} now={:.3}s routes={}",
            self.name(),
            now,
            routes.len()
        ));
        for route in routes {
            lines.push(format!(
                "dst={} via={} metric={} proto={}",
                route.destination, route.next_hop, route.metric, route.protocol
            ));
        }
        lines.join("\n")
    }
}

/// Backward-compatible alias trait for existing code paths.
pub trait ProtocolEngine: Ipv4RoutingProtocol {}
impl<T: Ipv4RoutingProtocol + ?Sized> ProtocolEngine for T {}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use super::*;
    use crate::model::messages::ControlMessage;
    use crate::model::routing::Route;

    struct DummyProtocol;

    impl Ipv4RoutingProtocol for DummyProtocol {
        fn name(&self) -> &'static str {
            "dummy"
        }

        fn start(&mut self, _ctx: &ProtocolContext) -> ProtocolOutputs {
            ProtocolOutputs::default()
        }

        fn on_timer(&mut self, _ctx: &ProtocolContext) -> ProtocolOutputs {
            ProtocolOutputs::default()
        }

        fn on_message(
            &mut self,
            _ctx: &ProtocolContext,
            _message: &ControlMessage,
        ) -> ProtocolOutputs {
            ProtocolOutputs::default()
        }
    }

    fn test_context() -> ProtocolContext {
        ProtocolContext {
            router_id: 1,
            now: 0.0,
            links: BTreeMap::new(),
        }
    }

    #[test]
    fn route_output_uses_best_metric() {
        let protocol = DummyProtocol;
        let ctx = test_context();
        let rib = vec![
            Route::new_host(7, 3, 5.0, "ospf"),
            Route::new_host(7, 2, 3.0, "ospf"),
        ];
        let selected = protocol
            .route_output(&ctx, 7, None, &rib)
            .expect("route should exist");
        assert_eq!(selected.next_hop, 2);
        assert_eq!(selected.metric, 3.0);
    }

    #[test]
    fn route_input_marks_local_delivery() {
        let protocol = DummyProtocol;
        let ctx = test_context();
        let result = protocol.route_input(&ctx, 1, None, &[]);
        assert_eq!(result, RouteInputResult::LocalDeliver);
    }
}
