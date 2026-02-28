use std::collections::VecDeque;

use crate::runtime::qdisc::core::{
    QueueDisc, QueueDiscBase, QueueDiscItem, QueueDiscSizePolicy, QueueSize, QueueSizeUnit,
};

pub struct RedQueueDisc {
    base: QueueDiscBase,
    queue: VecDeque<QueueDiscItem>,
    min_th: QueueSize,
    max_th: QueueSize,
    max_p: f64,
    ecn_enabled: bool,
    avg_q: f64,
    w_q: f64,
    rng_state: u64,
}

impl RedQueueDisc {
    pub const LIMIT_EXCEEDED_DROP: &'static str = "Queue disc limit exceeded";
    pub const RED_RANDOM_DROP: &'static str = "RED random early drop";
    pub const RED_FORCED_DROP: &'static str = "RED forced drop";
    pub const RED_ECN_MARK: &'static str = "RED ECN mark";

    pub fn new_default() -> Self {
        Self::new(
            QueueSize::packets(1000),
            QueueSize::packets(64),
            QueueSize::packets(256),
            0.1,
            false,
            0x9E37_79B9_7F4A_7C15,
        )
    }

    pub fn new(
        limit: QueueSize,
        min_th: QueueSize,
        max_th: QueueSize,
        max_p: f64,
        ecn_enabled: bool,
        seed: u64,
    ) -> Self {
        Self {
            base: QueueDiscBase::new(QueueDiscSizePolicy::SingleInternalQueue, Some(limit)),
            queue: VecDeque::new(),
            min_th,
            max_th,
            max_p: max_p.clamp(0.0, 1.0),
            ecn_enabled,
            avg_q: 0.0,
            w_q: 0.002,
            rng_state: seed.max(1),
        }
    }

    fn current_size_as_f64(&self, unit: QueueSizeUnit) -> f64 {
        match unit {
            QueueSizeUnit::Packets => self.base.packet_count() as f64,
            QueueSizeUnit::Bytes => self.base.byte_count() as f64,
        }
    }

    fn threshold_to_f64(size: QueueSize) -> f64 {
        size.value as f64
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

    fn update_average_queue(&mut self) {
        let unit = self.min_th.unit;
        let q = self.current_size_as_f64(unit);
        self.avg_q = (1.0 - self.w_q) * self.avg_q + self.w_q * q;
    }

    fn next_rand_01(&mut self) -> f64 {
        self.rng_state = self
            .rng_state
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1);
        let v = self.rng_state >> 11;
        (v as f64) / ((1u64 << 53) as f64)
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

impl QueueDisc for RedQueueDisc {
    fn name(&self) -> &'static str {
        "red"
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

        self.update_average_queue();
        let instant_q = self.current_size_as_f64(self.min_th.unit);
        // Use the upper envelope of EWMA and instantaneous queue to avoid
        // under-reacting in short-lived burst windows.
        let q_metric = self.avg_q.max(instant_q);
        let min_th = Self::threshold_to_f64(self.min_th);
        let max_th = Self::threshold_to_f64(self.max_th);

        if q_metric >= max_th {
            if self.ecn_enabled && Self::is_ecn_capable(&item) {
                Self::mark_ecn(&mut item);
                self.mark_packet(&item, Self::RED_ECN_MARK);
                self.queue.push_back(item);
                return true;
            }
            self.drop_before_enqueue(&item, Self::RED_FORCED_DROP);
            return false;
        }

        if q_metric > min_th {
            let p = self.max_p * (q_metric - min_th) / (max_th - min_th).max(1e-9);
            if self.next_rand_01() < p {
                if self.ecn_enabled && Self::is_ecn_capable(&item) {
                    Self::mark_ecn(&mut item);
                    self.mark_packet(&item, Self::RED_ECN_MARK);
                    self.queue.push_back(item);
                    return true;
                }
                self.drop_before_enqueue(&item, Self::RED_RANDOM_DROP);
                return false;
            }
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
        if self.min_th.unit != self.max_th.unit {
            return Err("RED requires min_th and max_th using the same unit".to_string());
        }
        if self.min_th.value >= self.max_th.value {
            return Err("RED requires min_th < max_th".to_string());
        }
        if !(0.0..=1.0).contains(&self.max_p) {
            return Err("RED requires max_p in [0, 1]".to_string());
        }
        Ok(())
    }

    fn initialize_params(&mut self) -> Result<(), String> {
        self.avg_q = 0.0;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::runtime::qdisc::core::{QueueDisc, QueueDiscItem, QueueSize};

    use super::RedQueueDisc;

    fn ecn_item(flow_id: u32) -> QueueDiscItem {
        let mut metadata = std::collections::BTreeMap::new();
        metadata.insert("ecn_capable".to_string(), "1".to_string());
        QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(flow_id),
            class_id: None,
            metadata,
        }
    }

    fn plain_item(flow_id: u32) -> QueueDiscItem {
        QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(flow_id),
            class_id: None,
            metadata: Default::default(),
        }
    }

    #[test]
    fn red_drops_on_hard_limit() {
        let mut q = RedQueueDisc::new(
            QueueSize::packets(1),
            QueueSize::packets(1),
            QueueSize::packets(2),
            0.5,
            false,
            7,
        );
        assert!(q.enqueue(plain_item(1)));
        assert!(!q.enqueue(plain_item(2)));
        assert_eq!(q.stats().n_total_dropped_packets_before_enqueue, 1);
    }

    #[test]
    fn red_marks_ecn_when_enabled() {
        let mut q = RedQueueDisc::new(
            QueueSize::packets(1000),
            QueueSize::packets(0),
            QueueSize::packets(1),
            1.0,
            true,
            11,
        );
        assert!(q.enqueue(plain_item(1)));
        assert!(q.enqueue(ecn_item(2)));
        let _ = q.dequeue();
        let marked = q.dequeue().expect("marked packet");
        assert_eq!(marked.metadata.get("ecn_marked").map(String::as_str), Some("1"));
        assert!(q.stats().n_total_marked_packets >= 1);
    }
}
