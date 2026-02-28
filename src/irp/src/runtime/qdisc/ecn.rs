use std::collections::VecDeque;

use crate::runtime::qdisc::core::{
    QueueDisc, QueueDiscBase, QueueDiscItem, QueueDiscSizePolicy, QueueSize, QueueSizeUnit,
};

pub struct EcnQueueDisc {
    base: QueueDiscBase,
    queue: VecDeque<QueueDiscItem>,
    mark_threshold: QueueSize,
}

impl EcnQueueDisc {
    pub const LIMIT_EXCEEDED_DROP: &'static str = "Queue disc limit exceeded";
    pub const NON_ECN_DROP: &'static str = "ECN required above mark threshold";
    pub const ECN_MARK: &'static str = "ECN mark";

    pub fn new_default() -> Self {
        Self::new(QueueSize::packets(1000), QueueSize::packets(200))
    }

    pub fn new(limit: QueueSize, mark_threshold: QueueSize) -> Self {
        Self {
            base: QueueDiscBase::new(QueueDiscSizePolicy::SingleInternalQueue, Some(limit)),
            queue: VecDeque::new(),
            mark_threshold,
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

    fn above_mark_threshold(&self) -> bool {
        match self.mark_threshold.unit {
            QueueSizeUnit::Packets => self.base.packet_count() >= self.mark_threshold.value,
            QueueSizeUnit::Bytes => self.base.byte_count() >= self.mark_threshold.value,
        }
    }

    fn is_ecn_capable(item: &QueueDiscItem) -> bool {
        for key in ["ecn_capable", "ecn"] {
            let Some(raw) = item.metadata.get(key) else {
                continue;
            };
            let val = raw.trim().to_ascii_lowercase();
            if matches!(val.as_str(), "1" | "true" | "yes" | "ect0" | "ect1" | "ce") {
                return true;
            }
        }
        false
    }

    fn mark_ecn(item: &mut QueueDiscItem) {
        item.metadata.insert("ecn_marked".to_string(), "1".to_string());
        item.metadata.insert("ecn".to_string(), "ce".to_string());
    }
}

impl QueueDisc for EcnQueueDisc {
    fn name(&self) -> &'static str {
        "ecn"
    }

    fn base(&self) -> &QueueDiscBase {
        &self.base
    }

    fn base_mut(&mut self) -> &mut QueueDiscBase {
        &mut self.base
    }

    fn do_enqueue(&mut self, mut item: QueueDiscItem) -> bool {
        if self.would_exceed_limit(&item) {
            self.drop_before_enqueue(&item, Self::LIMIT_EXCEEDED_DROP);
            return false;
        }
        if self.above_mark_threshold() {
            if Self::is_ecn_capable(&item) {
                Self::mark_ecn(&mut item);
                self.mark_packet(&item, Self::ECN_MARK);
                self.queue.push_back(item);
                return true;
            }
            self.drop_before_enqueue(&item, Self::NON_ECN_DROP);
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
        let Some(limit) = self.max_size() else {
            return Err("ECN queue requires max_size".to_string());
        };
        if limit.unit != self.mark_threshold.unit {
            return Err("ECN queue requires threshold unit matching max_size unit".to_string());
        }
        if self.mark_threshold.value > limit.value {
            return Err("ECN queue requires mark_threshold <= max_size".to_string());
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

    use super::EcnQueueDisc;

    fn plain_item(flow_id: u32) -> QueueDiscItem {
        QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(flow_id),
            class_id: None,
            metadata: Default::default(),
        }
    }

    fn ecn_item(flow_id: u32) -> QueueDiscItem {
        let mut metadata = std::collections::BTreeMap::new();
        metadata.insert("ecn_capable".to_string(), "true".to_string());
        QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(flow_id),
            class_id: None,
            metadata,
        }
    }

    #[test]
    fn ecn_queue_marks_when_above_threshold() {
        let mut q = EcnQueueDisc::new(QueueSize::packets(10), QueueSize::packets(1));
        assert!(q.enqueue(plain_item(1)));
        assert!(q.enqueue(ecn_item(2)));
        let _ = q.dequeue();
        let marked = q.dequeue().expect("ecn-marked packet");
        assert_eq!(marked.metadata.get("ecn_marked").map(String::as_str), Some("1"));
        assert!(q.stats().n_total_marked_packets >= 1);
    }

    #[test]
    fn ecn_queue_drops_non_ecn_when_above_threshold() {
        let mut q = EcnQueueDisc::new(QueueSize::packets(10), QueueSize::packets(1));
        assert!(q.enqueue(plain_item(1)));
        assert!(!q.enqueue(plain_item(2)));
        assert_eq!(q.stats().n_total_dropped_packets_before_enqueue, 1);
        assert_eq!(
            q.stats()
                .dropped_packets_before_enqueue
                .get(EcnQueueDisc::NON_ECN_DROP)
                .copied()
                .unwrap_or(0),
            1
        );
    }
}

