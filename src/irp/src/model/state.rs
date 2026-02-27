use std::collections::BTreeMap;

use crate::model::control_plane::{RecordFreshness, StateLifetimePolicy};

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
        let policy = StateLifetimePolicy::strict(max_age);
        let before = self.records.len();
        self.records
            .retain(|_, record| policy.is_usable(record.learned_at, now));
        before != self.records.len()
    }
}

#[derive(Debug, Clone)]
pub struct NeighborStateRecord {
    pub router_id: u32,
    pub state: NeighborFastState,
    pub learned_at: f64,
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct NeighborFastState {
    pub queue_level: Option<usize>,
    pub interface_utilization: Option<f64>,
    pub delay_ms: Option<f64>,
    pub loss_rate: Option<f64>,
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct NeighborFastStatePatch {
    pub queue_level: Option<usize>,
    pub interface_utilization: Option<f64>,
    pub delay_ms: Option<f64>,
    pub loss_rate: Option<f64>,
}

impl NeighborFastStatePatch {
    pub fn is_empty(&self) -> bool {
        self.queue_level.is_none()
            && self.interface_utilization.is_none()
            && self.delay_ms.is_none()
            && self.loss_rate.is_none()
    }
}

#[derive(Debug, Default)]
pub struct NeighborStateDb {
    records: BTreeMap<u32, NeighborStateRecord>,
}

impl NeighborStateDb {
    pub fn upsert_fast_state(
        &mut self,
        router_id: u32,
        patch: NeighborFastStatePatch,
        now: f64,
    ) -> bool {
        if patch.is_empty() {
            return false;
        }

        if let Some(record) = self.records.get_mut(&router_id) {
            let mut changed = false;
            changed |= apply_optional(&mut record.state.queue_level, patch.queue_level);
            changed |= apply_optional(
                &mut record.state.interface_utilization,
                patch.interface_utilization,
            );
            changed |= apply_optional(&mut record.state.delay_ms, patch.delay_ms);
            changed |= apply_optional(&mut record.state.loss_rate, patch.loss_rate);
            record.learned_at = now;
            return changed;
        }

        let state = NeighborFastState {
            queue_level: patch.queue_level,
            interface_utilization: patch.interface_utilization,
            delay_ms: patch.delay_ms,
            loss_rate: patch.loss_rate,
        };
        self.records.insert(
            router_id,
            NeighborStateRecord {
                router_id,
                state,
                learned_at: now,
            },
        );
        true
    }

    pub fn upsert_queue_level(&mut self, router_id: u32, queue_level: usize, now: f64) -> bool {
        self.upsert_fast_state(
            router_id,
            NeighborFastStatePatch {
                queue_level: Some(queue_level),
                ..NeighborFastStatePatch::default()
            },
            now,
        )
    }

    pub fn get_state_fresh(
        &self,
        router_id: u32,
        now: f64,
        max_age: f64,
    ) -> Option<&NeighborFastState> {
        let record = self.records.get(&router_id)?;
        if StateLifetimePolicy::strict(max_age).classify(record.learned_at, now)
            == RecordFreshness::Fresh
        {
            Some(&record.state)
        } else {
            None
        }
    }

    pub fn freshness(&self, router_id: u32, now: f64, max_age: f64) -> Option<RecordFreshness> {
        let record = self.records.get(&router_id)?;
        Some(StateLifetimePolicy::strict(max_age).classify(record.learned_at, now))
    }

    pub fn get_queue_level_fresh(&self, router_id: u32, now: f64, max_age: f64) -> Option<usize> {
        self.get_state_fresh(router_id, now, max_age)
            .and_then(|state| state.queue_level)
    }

    pub fn age_out(&mut self, now: f64, max_age: f64) -> bool {
        let policy = StateLifetimePolicy::strict(max_age);
        let before = self.records.len();
        self.records
            .retain(|_, record| policy.is_usable(record.learned_at, now));
        before != self.records.len()
    }

    pub fn remove(&mut self, router_id: u32) -> bool {
        self.records.remove(&router_id).is_some()
    }

    pub fn queue_levels_snapshot(&self) -> BTreeMap<u32, usize> {
        self.records
            .iter()
            .filter_map(|(router_id, record)| record.state.queue_level.map(|v| (*router_id, v)))
            .collect()
    }

    pub fn fast_state_snapshot(&self) -> BTreeMap<u32, NeighborFastState> {
        self.records
            .iter()
            .map(|(router_id, record)| (*router_id, record.state.clone()))
            .collect()
    }
}

fn apply_optional<T: PartialEq>(slot: &mut Option<T>, incoming: Option<T>) -> bool {
    let Some(value) = incoming else {
        return false;
    };
    if slot.as_ref() == Some(&value) {
        return false;
    }
    *slot = Some(value);
    true
}

#[cfg(test)]
mod tests {
    use super::{NeighborFastStatePatch, NeighborStateDb};

    #[test]
    fn nsdb_respects_freshness_window() {
        let mut nsdb = NeighborStateDb::default();
        assert!(nsdb.upsert_queue_level(2, 3, 0.0));
        assert_eq!(nsdb.get_queue_level_fresh(2, 0.5, 1.0), Some(3));
        assert_eq!(nsdb.get_queue_level_fresh(2, 1.5, 1.0), None);
        assert!(nsdb.age_out(1.5, 1.0));
        assert_eq!(nsdb.get_queue_level_fresh(2, 1.5, 1.0), None);
    }

    #[test]
    fn nsdb_merges_fast_state_patch() {
        let mut nsdb = NeighborStateDb::default();
        assert!(nsdb.upsert_fast_state(
            2,
            NeighborFastStatePatch {
                queue_level: Some(1),
                ..NeighborFastStatePatch::default()
            },
            0.0,
        ));
        assert!(nsdb.upsert_fast_state(
            2,
            NeighborFastStatePatch {
                interface_utilization: Some(0.5),
                delay_ms: Some(2.0),
                loss_rate: Some(0.01),
                ..NeighborFastStatePatch::default()
            },
            0.1,
        ));
        let state = nsdb
            .get_state_fresh(2, 0.2, 1.0)
            .expect("state should remain fresh");
        assert_eq!(state.queue_level, Some(1));
        assert_eq!(state.interface_utilization, Some(0.5));
        assert_eq!(state.delay_ms, Some(2.0));
        assert_eq!(state.loss_rate, Some(0.01));
    }
}
