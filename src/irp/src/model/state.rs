use std::collections::BTreeMap;

#[derive(Debug, Clone)]
pub struct NeighborInfo {
    pub router_id: u32,
    pub address: String,
    pub port: u16,
    pub cost: f64,
    pub interface_name: Option<String>,
    pub last_seen: Option<f64>,
    pub is_up: bool,
}

#[derive(Debug, Default)]
pub struct NeighborTable {
    neighbors: BTreeMap<u32, NeighborInfo>,
}

impl NeighborTable {
    pub fn new(neighbors: Vec<NeighborInfo>) -> Self {
        let neighbors = neighbors
            .into_iter()
            .map(|neighbor| (neighbor.router_id, neighbor))
            .collect();
        Self { neighbors }
    }

    pub fn mark_seen(&mut self, router_id: u32, now: f64) -> bool {
        let Some(neighbor) = self.neighbors.get_mut(&router_id) else {
            return false;
        };
        let was_up = neighbor.is_up;
        neighbor.last_seen = Some(now);
        neighbor.is_up = true;
        !was_up
    }

    pub fn refresh_liveness(&mut self, now: f64, dead_interval: f64) -> Vec<u32> {
        let mut changed = Vec::new();
        for (router_id, neighbor) in &mut self.neighbors {
            let Some(last_seen) = neighbor.last_seen else {
                continue;
            };
            let alive = (now - last_seen) <= dead_interval;
            if alive != neighbor.is_up {
                neighbor.is_up = alive;
                changed.push(*router_id);
            }
        }
        changed
    }

    pub fn get(&self, router_id: u32) -> Option<&NeighborInfo> {
        self.neighbors.get(&router_id)
    }

    pub fn iter(&self) -> impl Iterator<Item = (&u32, &NeighborInfo)> {
        self.neighbors.iter()
    }
}

#[derive(Debug, Clone)]
pub struct LinkStateRecord {
    pub router_id: u32,
    pub seq: i64,
    pub links: BTreeMap<u32, f64>,
    pub learned_at: f64,
}

#[derive(Debug, Default)]
pub struct LinkStateDb {
    records: BTreeMap<u32, LinkStateRecord>,
}

impl LinkStateDb {
    pub fn upsert(
        &mut self,
        router_id: u32,
        seq: i64,
        links: BTreeMap<u32, f64>,
        now: f64,
    ) -> bool {
        if let Some(current) = self.records.get(&router_id) {
            if seq <= current.seq {
                return false;
            }
        }

        self.records.insert(
            router_id,
            LinkStateRecord {
                router_id,
                seq,
                links,
                learned_at: now,
            },
        );
        true
    }

    pub fn records(&self) -> Vec<LinkStateRecord> {
        self.records.values().cloned().collect()
    }

    pub fn age_out(&mut self, now: f64, max_age: f64) -> bool {
        let before = self.records.len();
        self.records
            .retain(|_, record| (now - record.learned_at) <= max_age);
        before != self.records.len()
    }
}
