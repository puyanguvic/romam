use std::cmp::Ordering;
use std::collections::BinaryHeap;

#[derive(Debug, Clone, Copy, PartialEq)]
struct QueueEntry {
    node: u32,
    cost: f64,
}

impl Eq for QueueEntry {}

impl Ord for QueueEntry {
    fn cmp(&self, other: &Self) -> Ordering {
        other
            .cost
            .total_cmp(&self.cost)
            .then_with(|| other.node.cmp(&self.node))
    }
}

impl PartialOrd for QueueEntry {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

#[derive(Debug, Default, Clone)]
pub struct DistanceFrontier {
    heap: BinaryHeap<QueueEntry>,
}

impl DistanceFrontier {
    pub fn new() -> Self {
        Self {
            heap: BinaryHeap::new(),
        }
    }

    pub fn push(&mut self, node: u32, cost: f64) {
        self.heap.push(QueueEntry { node, cost });
    }

    pub fn pop_min<F>(&mut self, mut is_stale: F) -> Option<(u32, f64)>
    where
        F: FnMut(u32, f64) -> bool,
    {
        while let Some(entry) = self.heap.pop() {
            if is_stale(entry.node, entry.cost) {
                continue;
            }
            return Some((entry.node, entry.cost));
        }
        None
    }
}
