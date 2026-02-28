use std::collections::VecDeque;

use crate::runtime::qdisc::core::{
    QueueDisc, QueueDiscBase, QueueDiscItem, QueueDiscSizePolicy, QueueSize, QueueSizeUnit,
};

pub struct FifoQueueDisc {
    base: QueueDiscBase,
    queue: VecDeque<QueueDiscItem>,
}

impl FifoQueueDisc {
    pub const LIMIT_EXCEEDED_DROP: &'static str = "Queue disc limit exceeded";

    pub fn new_default() -> Self {
        Self::new_with_limit(QueueSize::packets(1000))
    }

    pub fn new_with_limit(limit: QueueSize) -> Self {
        Self {
            base: QueueDiscBase::new(QueueDiscSizePolicy::SingleInternalQueue, Some(limit)),
            queue: VecDeque::new(),
        }
    }

    fn would_exceed_limit(&self, item: &QueueDiscItem) -> bool {
        let Some(limit) = self.base.max_size() else {
            return false;
        };
        match limit.unit {
            QueueSizeUnit::Packets => self.base.packet_count().saturating_add(1) > limit.value,
            QueueSizeUnit::Bytes => {
                self.base.byte_count().saturating_add(item.len_bytes()) > limit.value
            }
        }
    }
}

impl QueueDisc for FifoQueueDisc {
    fn name(&self) -> &'static str {
        "fifo"
    }

    fn base(&self) -> &QueueDiscBase {
        &self.base
    }

    fn base_mut(&mut self) -> &mut QueueDiscBase {
        &mut self.base
    }

    fn do_enqueue(&mut self, item: QueueDiscItem) -> bool {
        if self.would_exceed_limit(&item) {
            self.drop_before_enqueue(&item, Self::LIMIT_EXCEEDED_DROP);
            return false;
        }
        self.queue.push_back(item);
        true
    }

    fn do_dequeue(&mut self) -> Option<QueueDiscItem> {
        self.queue.pop_front()
    }

    fn do_peek(&self) -> Option<&QueueDiscItem> {
        self.queue.front()
    }

    fn check_config(&self) -> Result<(), String> {
        if self.max_size().is_none() {
            return Err("fifo queue disc requires max_size".to_string());
        }
        Ok(())
    }

    fn initialize_params(&mut self) -> Result<(), String> {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::runtime::qdisc::core::{QueueDisc, QueueDiscItem, QueueSize};

    use super::FifoQueueDisc;

    #[test]
    fn fifo_keeps_order() {
        let mut q = FifoQueueDisc::new_with_limit(QueueSize::packets(8));
        assert!(q.enqueue(QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(1),
            class_id: None,
            metadata: Default::default(),
        }));
        assert!(q.enqueue(QueueDiscItem {
            packet_len_bytes: 120,
            flow_id: Some(2),
            class_id: None,
            metadata: Default::default(),
        }));
        let first = q.dequeue().expect("first packet");
        let second = q.dequeue().expect("second packet");
        assert_eq!(first.flow_id, Some(1));
        assert_eq!(second.flow_id, Some(2));
    }

    #[test]
    fn fifo_drops_when_limit_exceeded() {
        let mut q = FifoQueueDisc::new_with_limit(QueueSize::packets(1));
        assert!(q.enqueue(QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(1),
            class_id: None,
            metadata: Default::default(),
        }));
        assert!(!q.enqueue(QueueDiscItem {
            packet_len_bytes: 120,
            flow_id: Some(2),
            class_id: None,
            metadata: Default::default(),
        }));
        assert_eq!(q.stats().n_total_dropped_packets_before_enqueue, 1);
        assert_eq!(
            q.stats()
                .dropped_packets_before_enqueue
                .get(FifoQueueDisc::LIMIT_EXCEEDED_DROP)
                .copied()
                .unwrap_or(0),
            1
        );
    }
}

