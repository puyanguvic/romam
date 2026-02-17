use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq)]
pub struct Route {
    pub destination: u32,
    pub next_hop: u32,
    pub metric: f64,
    pub protocol: String,
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
    routes: BTreeMap<u32, Route>,
}

impl RouteTable {
    pub fn replace_protocol_routes(&mut self, protocol: &str, routes: &[Route]) -> bool {
        let mut updated = false;

        let stale: Vec<u32> = self
            .routes
            .iter()
            .filter_map(|(dst, route)| (route.protocol == protocol).then_some(*dst))
            .collect();

        for dst in stale {
            self.routes.remove(&dst);
            updated = true;
        }

        for route in routes {
            let prev = self.routes.get(&route.destination);
            if prev != Some(route) {
                self.routes.insert(route.destination, route.clone());
                updated = true;
            }
        }

        updated
    }

    pub fn snapshot(&self) -> Vec<Route> {
        self.routes.values().cloned().collect()
    }
}

#[derive(Debug, Default)]
pub struct ForwardingTable {
    entries: BTreeMap<u32, ForwardingEntry>,
}

impl ForwardingTable {
    pub fn sync_from_routes(&mut self, routes: &[Route]) -> bool {
        let next_entries: BTreeMap<u32, ForwardingEntry> = routes
            .iter()
            .map(|route| {
                (
                    route.destination,
                    ForwardingEntry {
                        destination: route.destination,
                        next_hop: route.next_hop,
                        metric: route.metric,
                        protocol: route.protocol.clone(),
                    },
                )
            })
            .collect();

        if next_entries == self.entries {
            return false;
        }
        self.entries = next_entries;
        true
    }

    pub fn snapshot(&self) -> Vec<ForwardingEntry> {
        self.entries.values().cloned().collect()
    }
}
