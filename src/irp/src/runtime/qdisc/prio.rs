use crate::runtime::qdisc::core::{
    QueueDisc, QueueDiscBase, QueueDiscItem, QueueDiscSizePolicy,
};
use crate::runtime::qdisc::fifo::FifoQueueDisc;

pub struct PrioQueueDisc {
    base: QueueDiscBase,
    classes: Vec<FifoQueueDisc>,
    prio2band: [usize; 16],
}

impl PrioQueueDisc {
    pub const MIN_CLASSES: usize = 2;
    pub const DEFAULT_CLASSES: usize = 3;

    pub fn new_default() -> Self {
        Self::new_with_classes(Self::DEFAULT_CLASSES)
    }

    pub fn new_with_classes(n_classes: usize) -> Self {
        let class_count = n_classes.max(Self::MIN_CLASSES);
        let mut classes = Vec::with_capacity(class_count);
        for _ in 0..class_count {
            classes.push(FifoQueueDisc::new_default());
        }
        Self {
            base: QueueDiscBase::new(QueueDiscSizePolicy::NoLimits, None),
            classes,
            prio2band: Self::default_prio2band(class_count),
        }
    }

    fn default_prio2band(n_classes: usize) -> [usize; 16] {
        let default = [1usize, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1];
        let max_band = n_classes.saturating_sub(1);
        let mut out = [0usize; 16];
        for (i, band) in default.into_iter().enumerate() {
            out[i] = band.min(max_band);
        }
        out
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

    fn classify_band(&self, item: &QueueDiscItem) -> usize {
        if let Some(class_id) = item.class_id {
            let band = class_id as usize;
            if band < self.classes.len() {
                return band;
            }
        }
        self.prio2band[(Self::item_priority(item) & 0x0f) as usize]
    }

    pub fn set_band_for_priority(&mut self, prio: u8, band: usize) -> Result<(), String> {
        if prio >= 16 {
            return Err("priority must be in [0, 15]".to_string());
        }
        if band >= self.classes.len() {
            return Err("band out of range".to_string());
        }
        self.prio2band[prio as usize] = band;
        Ok(())
    }

    pub fn band_for_priority(&self, prio: u8) -> Result<usize, String> {
        if prio >= 16 {
            return Err("priority must be in [0, 15]".to_string());
        }
        Ok(self.prio2band[prio as usize])
    }

    pub fn class_count(&self) -> usize {
        self.classes.len()
    }
}

impl QueueDisc for PrioQueueDisc {
    fn name(&self) -> &'static str {
        "prio"
    }

    fn base(&self) -> &QueueDiscBase {
        &self.base
    }

    fn base_mut(&mut self) -> &mut QueueDiscBase {
        &mut self.base
    }

    fn do_enqueue(&mut self, item: QueueDiscItem) -> bool {
        let band = self.classify_band(&item);
        if band >= self.classes.len() {
            self.drop_before_enqueue(&item, "Selected band out of range");
            return false;
        }
        self.classes[band].enqueue(item)
    }

    fn do_dequeue(&mut self) -> Option<QueueDiscItem> {
        for class in &mut self.classes {
            if let Some(item) = class.dequeue() {
                return Some(item);
            }
        }
        None
    }

    fn do_peek(&self) -> Option<&QueueDiscItem> {
        for class in &self.classes {
            if let Some(item) = class.peek() {
                return Some(item);
            }
        }
        None
    }

    fn check_config(&self) -> Result<(), String> {
        if self.classes.len() < Self::MIN_CLASSES {
            return Err("prio queue disc requires at least 2 classes".to_string());
        }
        Ok(())
    }

    fn initialize_params(&mut self) -> Result<(), String> {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use crate::runtime::qdisc::core::{QueueDisc, QueueDiscItem};

    use super::PrioQueueDisc;

    fn item_with_priority(flow_id: u32, priority: u8) -> QueueDiscItem {
        let mut metadata = BTreeMap::new();
        metadata.insert("priority".to_string(), priority.to_string());
        QueueDiscItem {
            packet_len_bytes: 100,
            flow_id: Some(flow_id),
            class_id: None,
            metadata,
        }
    }

    #[test]
    fn prio_dequeue_prefers_lower_band_index() {
        let mut q = PrioQueueDisc::new_default();
        assert!(q.enqueue(item_with_priority(1, 1))); // band 2
        assert!(q.enqueue(item_with_priority(2, 0))); // band 1
        assert!(q.enqueue(item_with_priority(3, 6))); // band 0

        let a = q.dequeue().expect("dequeue a");
        let b = q.dequeue().expect("dequeue b");
        let c = q.dequeue().expect("dequeue c");
        assert_eq!(a.flow_id, Some(3));
        assert_eq!(b.flow_id, Some(2));
        assert_eq!(c.flow_id, Some(1));
    }

    #[test]
    fn class_id_overrides_priomap() {
        let mut q = PrioQueueDisc::new_default();
        let mut item = item_with_priority(9, 6); // would map to band 0
        item.class_id = Some(2);
        assert!(q.enqueue(item));
        let out = q.dequeue().expect("dequeue with class id");
        assert_eq!(out.flow_id, Some(9));
    }

    #[test]
    fn set_and_get_band_for_priority() {
        let mut q = PrioQueueDisc::new_default();
        q.set_band_for_priority(0, 2).expect("set mapping");
        assert_eq!(q.band_for_priority(0).expect("get mapping"), 2);
    }
}

