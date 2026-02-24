use std::cmp::Ordering;
use std::collections::BTreeMap;

/// Canonical L3 routing entry metadata.
///
/// Note: this framework uses router-id space (`u32`) for destination/gateway
/// identifiers; forwarding-plane IP prefixes are resolved later via
/// `ForwardingConfig` mappings.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Ipv4RoutingTableEntry {
    destination: u32,
    destination_network: u32,
    destination_prefix_len: u8,
    gateway: Option<u32>,
    interface: Option<u32>,
}

/// Virtual-base style interface for protocol-specific routing entries.
///
/// Rust does not support class inheritance; this trait provides polymorphic
/// behavior similar to a virtual parent class over entries that embed
/// `Ipv4RoutingTableEntry`.
pub trait RoutingTableEntry {
    fn base(&self) -> &Ipv4RoutingTableEntry;

    fn destination(&self) -> u32 {
        self.base().get_dest()
    }

    fn next_hop(&self) -> Option<u32> {
        self.base().get_gateway()
    }

    fn interface(&self) -> Option<u32> {
        self.base().get_interface()
    }
}

impl RoutingTableEntry for Ipv4RoutingTableEntry {
    fn base(&self) -> &Ipv4RoutingTableEntry {
        self
    }
}

impl Ipv4RoutingTableEntry {
    pub fn is_host(&self) -> bool {
        self.destination_prefix_len == 32
    }

    pub fn is_network(&self) -> bool {
        !self.is_host()
    }

    pub fn is_default(&self) -> bool {
        self.destination_network == 0 && self.destination_prefix_len == 0
    }

    pub fn is_gateway(&self) -> bool {
        self.gateway.is_some()
    }

    pub fn get_gateway(&self) -> Option<u32> {
        self.gateway
    }

    pub fn get_dest(&self) -> u32 {
        self.destination
    }

    pub fn get_dest_network(&self) -> u32 {
        self.destination_network
    }

    pub fn get_dest_network_prefix_len(&self) -> u8 {
        self.destination_prefix_len
    }

    pub fn get_interface(&self) -> Option<u32> {
        self.interface
    }

    pub fn create_host_route_to(dest: u32, next_hop: u32, interface: Option<u32>) -> Self {
        Self {
            destination: dest,
            destination_network: dest,
            destination_prefix_len: 32,
            gateway: Some(next_hop),
            interface,
        }
    }

    pub fn create_host_route_to_direct(dest: u32, interface: Option<u32>) -> Self {
        Self {
            destination: dest,
            destination_network: dest,
            destination_prefix_len: 32,
            gateway: None,
            interface,
        }
    }

    pub fn create_network_route_to(
        network: u32,
        prefix_len: u8,
        next_hop: u32,
        interface: Option<u32>,
    ) -> Self {
        Self {
            destination: network,
            destination_network: network,
            destination_prefix_len: prefix_len,
            gateway: Some(next_hop),
            interface,
        }
    }

    pub fn create_network_route_to_direct(
        network: u32,
        prefix_len: u8,
        interface: Option<u32>,
    ) -> Self {
        Self {
            destination: network,
            destination_network: network,
            destination_prefix_len: prefix_len,
            gateway: None,
            interface,
        }
    }

    pub fn create_default_route(next_hop: u32, interface: Option<u32>) -> Self {
        Self {
            destination: 0,
            destination_network: 0,
            destination_prefix_len: 0,
            gateway: Some(next_hop),
            interface,
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct Route {
    pub entry: Ipv4RoutingTableEntry,
    pub destination: u32,
    pub next_hop: u32,
    pub metric: f64,
    pub protocol: String,
}

impl Route {
    pub fn new_host(
        destination: u32,
        next_hop: u32,
        metric: f64,
        protocol: impl Into<String>,
    ) -> Self {
        Self {
            entry: Ipv4RoutingTableEntry::create_host_route_to(destination, next_hop, None),
            destination,
            next_hop,
            metric,
            protocol: protocol.into(),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct ForwardingEntry {
    pub destination: u32,
    pub next_hop: u32,
    pub metric: f64,
    pub protocol: String,
}

#[derive(Debug, Default)]
pub struct RouteTable {
    routes: Vec<Route>,
}

impl RouteTable {
    pub fn replace_protocol_routes(&mut self, protocol: &str, routes: &[Route]) -> bool {
        let mut next: Vec<Route> = self
            .routes
            .iter()
            .filter(|route| route.protocol != protocol)
            .cloned()
            .collect();
        next.extend(routes.iter().cloned());
        Self::normalize_routes(&mut next);
        if next == self.routes {
            return false;
        }
        self.routes = next;
        true
    }

    pub fn snapshot(&self) -> Vec<Route> {
        self.routes.clone()
    }

    fn normalize_routes(routes: &mut Vec<Route>) {
        routes.sort_by(|left, right| {
            left.destination
                .cmp(&right.destination)
                .then_with(|| left.next_hop.cmp(&right.next_hop))
                .then_with(|| left.metric.total_cmp(&right.metric))
                .then_with(|| left.protocol.cmp(&right.protocol))
        });
        routes.dedup_by(|left, right| {
            left.destination == right.destination
                && left.next_hop == right.next_hop
                && left.metric.to_bits() == right.metric.to_bits()
                && left.protocol == right.protocol
        });
    }
}

#[derive(Debug, Default)]
pub struct ForwardingTable {
    entries: BTreeMap<u32, ForwardingEntry>,
}

impl ForwardingTable {
    pub fn sync_from_routes(&mut self, routes: &[Route]) -> bool {
        let mut next_entries: BTreeMap<u32, ForwardingEntry> = BTreeMap::new();
        for route in routes {
            let candidate = ForwardingEntry {
                destination: route.destination,
                next_hop: route.next_hop,
                metric: route.metric,
                protocol: route.protocol.clone(),
            };
            match next_entries.get(&route.destination) {
                None => {
                    next_entries.insert(route.destination, candidate);
                }
                Some(current) => {
                    if Self::prefers_first_route(&current.protocol) {
                        continue;
                    }
                    if Self::forwarding_candidate_better(route, current) {
                        next_entries.insert(route.destination, candidate);
                    }
                }
            }
        }

        if next_entries == self.entries {
            return false;
        }
        self.entries = next_entries;
        true
    }

    pub fn snapshot(&self) -> Vec<ForwardingEntry> {
        self.entries.values().cloned().collect()
    }

    fn prefers_first_route(protocol: &str) -> bool {
        matches!(protocol, "ddr" | "dgr" | "octopus")
    }

    fn forwarding_candidate_better(candidate: &Route, current: &ForwardingEntry) -> bool {
        match candidate.metric.total_cmp(&current.metric) {
            Ordering::Less => true,
            Ordering::Greater => false,
            Ordering::Equal => {
                if candidate.next_hop != current.next_hop {
                    candidate.next_hop < current.next_hop
                } else {
                    candidate.protocol < current.protocol
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn route_table_keeps_multi_path_entries_for_same_destination() {
        let mut table = RouteTable::default();
        let updated = table.replace_protocol_routes(
            "ddr",
            &[
                Route {
                    ..Route::new_host(4, 2, 10.0, "ddr")
                },
                Route {
                    ..Route::new_host(4, 3, 12.0, "ddr")
                },
            ],
        );
        assert!(updated);
        let snapshot = table.snapshot();
        assert_eq!(snapshot.len(), 2);
        assert!(snapshot
            .iter()
            .any(|route| route.destination == 4 && route.next_hop == 2));
        assert!(snapshot
            .iter()
            .any(|route| route.destination == 4 && route.next_hop == 3));
    }

    #[test]
    fn forwarding_table_selects_best_metric_per_destination() {
        let mut fib = ForwardingTable::default();
        let changed = fib.sync_from_routes(&[
            Route {
                ..Route::new_host(4, 2, 10.0, "ospf")
            },
            Route {
                ..Route::new_host(4, 3, 8.0, "ospf")
            },
            Route {
                ..Route::new_host(5, 2, 4.0, "ospf")
            },
        ]);
        assert!(changed);
        let entries = fib.snapshot();
        assert_eq!(entries.len(), 2);
        let to_4 = entries
            .iter()
            .find(|entry| entry.destination == 4)
            .expect("fib route to 4 should exist");
        assert_eq!(to_4.next_hop, 3);
        assert_eq!(to_4.metric, 8.0);
    }

    #[test]
    fn forwarding_table_for_ddr_keeps_first_route_per_destination() {
        let mut fib = ForwardingTable::default();
        let changed = fib.sync_from_routes(&[
            Route {
                ..Route::new_host(4, 3, 20.0, "ddr")
            },
            Route {
                ..Route::new_host(4, 2, 10.0, "ddr")
            },
        ]);
        assert!(changed);
        let entries = fib.snapshot();
        assert_eq!(entries.len(), 1);
        let to_4 = entries
            .iter()
            .find(|entry| entry.destination == 4)
            .expect("fib route to 4 should exist");
        assert_eq!(to_4.next_hop, 3);
        assert_eq!(to_4.metric, 20.0);
    }

    #[test]
    fn ipv4_routing_table_entry_helpers_work() {
        let host = Ipv4RoutingTableEntry::create_host_route_to(7, 3, Some(1));
        assert!(host.is_host());
        assert!(!host.is_default());
        assert!(host.is_gateway());
        assert_eq!(host.get_dest(), 7);
        assert_eq!(host.get_gateway(), Some(3));
        assert_eq!(host.get_interface(), Some(1));

        let def = Ipv4RoutingTableEntry::create_default_route(2, Some(4));
        assert!(def.is_default());
        assert!(def.is_network());
        assert_eq!(def.get_dest_network_prefix_len(), 0);
    }
}
