use std::collections::VecDeque;

use crate::runtime::qdisc::core::{
    QueueDisc, QueueDiscBase, QueueDiscItem, QueueDiscSizePolicy, QueueSize, QueueSizeUnit,
};

pub struct PfifoFastQueueDisc {
    base: QueueDiscBase,
    bands: Vec<VecDeque<QueueDiscItem>>,
}

impl PfifoFastQueueDisc {
    pub const LIMIT_EXCEEDED_DROP: &'static str = "Queue disc limit exceeded";
    pub const N_BANDS: usize = 3;

    // Linux-style priomap used by pfifo_fast.
    const PRIO2BAND: [usize; 16] = [1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1];

    pub fn new_default() -> Self {
        Self::new_with_limit(QueueSize::packets(1000))
    }

    pub fn new_with_limit(limit: QueueSize) -> Self {
        let mut bands = Vec::with_capacity(Self::N_BANDS);
        for _ in 0..Self::N_BANDS {
            bands.push(VecDeque::new());
        }
        Self {
            base: QueueDiscBase::new(QueueDiscSizePolicy::MultipleQueues, Some(limit)),
            bands,
        }
    }

    fn would_exceed_limit(&self, item: &QueueDiscItem) -> bool {
        let Some(limit) = self.base.max_size() else {
            return false;
        };
        match limit.unit {
            QueueSizeUnit::Packets => self.base.packet_count() >= limit.value,
            QueueSizeUnit::Bytes => {
                self.base.byte_count().saturating_add(item.len_bytes()) > limit.value
            }
        }
    }

    fn item_priority(item: &QueueDiscItem) -> u8 {
        for key in ["priority", "socket_priority", "prio"] {
            let Some(raw) = item.metadata.get(key) else {
                continue;
            };
            if let Ok(v) = raw.trim().parse::<u16>() {
                return (v & 0x00ff) as u8;
            }
        }
        0
    }

    fn select_band(item: &QueueDiscItem) -> usize {
        let prio = Self::item_priority(item);
        Self::PRIO2BAND[(prio & 0x0f) as usize]
    }
}

impl QueueDisc for PfifoFastQueueDisc {
    fn name(&self) -> &'static str {
        "pfifo_fast"
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
        let band = Self::select_band(&item);
        if band >= self.bands.len() {
            self.drop_before_enqueue(&item, "Invalid pfifo_fast band");
            return false;
        }
        self.bands[band].push_back(item);
        true
    }

    fn do_dequeue(&mut self) -> Option<QueueDiscItem> {
        for band in &mut self.bands {
            if let Some(item) = band.pop_front() {
                return Some(item);
            }
        }
        None
    }

    fn do_peek(&self) -> Option<&QueueDiscItem> {
        for band in &self.bands {
            if let Some(item) = band.front() {
                return Some(item);
            }
        }
        None
    }

    fn check_config(&self) -> Result<(), String> {
        let Some(limit) = self.max_size() else {
            return Err("pfifo_fast queue disc requires max_size".to_string());
        };
        if limit.unit != QueueSizeUnit::Packets {
            return Err("pfifo_fast queue disc requires packet-mode max_size".to_string());
        }
        if self.bands.len() != Self::N_BANDS {
            return Err("pfifo_fast queue disc requires exactly 3 bands".to_string());
        }
        Ok(())
    }

    fn initialize_params(&mut self) -> Result<(), String> {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::runtime::qdisc::core::{QueueDisc, QueueDiscItem, QueueSize, QueueSizeUnit};

    use super::PfifoFastQueueDisc;

    fn item_with_priority(priority: u8, flow_id: u32) -> QueueDiscItem {
        let mut metadata = std::collections::BTreeMap::new();
        metadata.insert("priority".to_string(), priority.to_string());
        QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(flow_id),
            class_id: None,
            metadata,
        }
    }

    #[test]
    fn pfifo_fast_prefers_higher_band_priority() {
        let mut q = PfifoFastQueueDisc::new_with_limit(QueueSize::packets(10));
        assert!(q.enqueue(item_with_priority(0, 10))); // band 1
        assert!(q.enqueue(item_with_priority(1, 11))); // band 2
        assert!(q.enqueue(item_with_priority(6, 12))); // band 0

        let first = q.dequeue().expect("first dequeue");
        let second = q.dequeue().expect("second dequeue");
        let third = q.dequeue().expect("third dequeue");
        assert_eq!(first.flow_id, Some(12));
        assert_eq!(second.flow_id, Some(10));
        assert_eq!(third.flow_id, Some(11));
    }

    #[test]
    fn pfifo_fast_drops_when_limit_exceeded() {
        let mut q = PfifoFastQueueDisc::new_with_limit(QueueSize::packets(1));
        assert!(q.enqueue(item_with_priority(0, 1)));
        assert!(!q.enqueue(item_with_priority(6, 2)));
        assert_eq!(q.stats().n_total_dropped_packets_before_enqueue, 1);
        assert_eq!(
            q.stats()
                .dropped_packets_before_enqueue
                .get(PfifoFastQueueDisc::LIMIT_EXCEEDED_DROP)
                .copied()
                .unwrap_or(0),
            1
        );
    }

    #[test]
    fn pfifo_fast_requires_packet_mode_limit() {
        let q = PfifoFastQueueDisc::new_with_limit(QueueSize {
            unit: QueueSizeUnit::Bytes,
            value: 2048,
        });
        assert!(q.check_config().is_err());
    }
}

