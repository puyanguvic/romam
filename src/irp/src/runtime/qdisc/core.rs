use std::collections::BTreeMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum QueueSizeUnit {
    Packets,
    Bytes,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct QueueSize {
    pub unit: QueueSizeUnit,
    pub value: u64,
}

impl QueueSize {
    pub fn packets(value: u64) -> Self {
        Self {
            unit: QueueSizeUnit::Packets,
            value,
        }
    }

    pub fn bytes(value: u64) -> Self {
        Self {
            unit: QueueSizeUnit::Bytes,
            value,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum QueueDiscSizePolicy {
    SingleInternalQueue,
    SingleChildQueueDisc,
    MultipleQueues,
    NoLimits,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum QueueDropPhase {
    BeforeEnqueue,
    AfterDequeue,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct QueueDiscStats {
    pub n_total_received_packets: u64,
    pub n_total_received_bytes: u64,
    pub n_total_enqueued_packets: u64,
    pub n_total_enqueued_bytes: u64,
    pub n_total_dequeued_packets: u64,
    pub n_total_dequeued_bytes: u64,
    pub n_total_requeued_packets: u64,
    pub n_total_requeued_bytes: u64,
    pub n_total_dropped_packets_before_enqueue: u64,
    pub n_total_dropped_packets_after_dequeue: u64,
    pub n_total_dropped_bytes_before_enqueue: u64,
    pub n_total_dropped_bytes_after_dequeue: u64,
    pub n_total_marked_packets: u64,
    pub n_total_marked_bytes: u64,
    pub dropped_packets_before_enqueue: BTreeMap<String, u64>,
    pub dropped_packets_after_dequeue: BTreeMap<String, u64>,
    pub dropped_bytes_before_enqueue: BTreeMap<String, u64>,
    pub dropped_bytes_after_dequeue: BTreeMap<String, u64>,
    pub marked_packets: BTreeMap<String, u64>,
    pub marked_bytes: BTreeMap<String, u64>,
}

impl QueueDiscStats {
    pub fn n_total_dropped_packets(&self) -> u64 {
        self.n_total_dropped_packets_before_enqueue + self.n_total_dropped_packets_after_dequeue
    }

    pub fn n_total_dropped_bytes(&self) -> u64 {
        self.n_total_dropped_bytes_before_enqueue + self.n_total_dropped_bytes_after_dequeue
    }

    pub fn record_received(&mut self, bytes: u64) {
        self.n_total_received_packets += 1;
        self.n_total_received_bytes += bytes;
    }

    pub fn record_enqueued(&mut self, bytes: u64) {
        self.n_total_enqueued_packets += 1;
        self.n_total_enqueued_bytes += bytes;
    }

    pub fn record_dequeued(&mut self, bytes: u64) {
        self.n_total_dequeued_packets += 1;
        self.n_total_dequeued_bytes += bytes;
    }

    pub fn record_requeued(&mut self, bytes: u64) {
        self.n_total_requeued_packets += 1;
        self.n_total_requeued_bytes += bytes;
    }

    pub fn record_marked(&mut self, bytes: u64, reason: &str) {
        self.n_total_marked_packets += 1;
        self.n_total_marked_bytes += bytes;
        *self.marked_packets.entry(reason.to_string()).or_insert(0) += 1;
        *self.marked_bytes.entry(reason.to_string()).or_insert(0) += bytes;
    }

    pub fn record_drop(&mut self, bytes: u64, reason: &str, phase: QueueDropPhase) {
        match phase {
            QueueDropPhase::BeforeEnqueue => {
                self.n_total_dropped_packets_before_enqueue += 1;
                self.n_total_dropped_bytes_before_enqueue += bytes;
                *self
                    .dropped_packets_before_enqueue
                    .entry(reason.to_string())
                    .or_insert(0) += 1;
                *self
                    .dropped_bytes_before_enqueue
                    .entry(reason.to_string())
                    .or_insert(0) += bytes;
            }
            QueueDropPhase::AfterDequeue => {
                self.n_total_dropped_packets_after_dequeue += 1;
                self.n_total_dropped_bytes_after_dequeue += bytes;
                *self
                    .dropped_packets_after_dequeue
                    .entry(reason.to_string())
                    .or_insert(0) += 1;
                *self
                    .dropped_bytes_after_dequeue
                    .entry(reason.to_string())
                    .or_insert(0) += bytes;
            }
        }
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct QueueDiscItem {
    pub packet_len_bytes: u32,
    pub flow_id: Option<u32>,
    pub class_id: Option<u32>,
    pub metadata: BTreeMap<String, String>,
}

impl QueueDiscItem {
    pub fn len_bytes(&self) -> u64 {
        u64::from(self.packet_len_bytes)
    }
}

#[derive(Debug, Clone)]
pub struct QueueDiscBase {
    size_policy: QueueDiscSizePolicy,
    max_size: Option<QueueSize>,
    n_packets: u64,
    n_bytes: u64,
    stats: QueueDiscStats,
}

impl QueueDiscBase {
    pub fn new(size_policy: QueueDiscSizePolicy, max_size: Option<QueueSize>) -> Self {
        Self {
            size_policy,
            max_size,
            n_packets: 0,
            n_bytes: 0,
            stats: QueueDiscStats::default(),
        }
    }

    pub fn size_policy(&self) -> QueueDiscSizePolicy {
        self.size_policy
    }

    pub fn max_size(&self) -> Option<QueueSize> {
        self.max_size
    }

    pub fn set_max_size(&mut self, size: QueueSize) -> bool {
        self.max_size = Some(size);
        true
    }

    pub fn current_size(&self) -> QueueSize {
        match self.max_size {
            Some(limit) if limit.unit == QueueSizeUnit::Bytes => QueueSize::bytes(self.n_bytes),
            _ => QueueSize::packets(self.n_packets),
        }
    }

    pub fn packet_count(&self) -> u64 {
        self.n_packets
    }

    pub fn byte_count(&self) -> u64 {
        self.n_bytes
    }

    pub fn stats(&self) -> &QueueDiscStats {
        &self.stats
    }

    pub fn stats_mut(&mut self) -> &mut QueueDiscStats {
        &mut self.stats
    }

    fn on_packet_enqueued(&mut self, bytes: u64) {
        self.n_packets += 1;
        self.n_bytes += bytes;
        self.stats.record_enqueued(bytes);
    }

    fn on_packet_dequeued(&mut self, bytes: u64) {
        self.n_packets = self.n_packets.saturating_sub(1);
        self.n_bytes = self.n_bytes.saturating_sub(bytes);
        self.stats.record_dequeued(bytes);
    }
}

pub trait QueueDisc: Send {
    fn name(&self) -> &'static str;

    fn base(&self) -> &QueueDiscBase;

    fn base_mut(&mut self) -> &mut QueueDiscBase;

    fn do_enqueue(&mut self, item: QueueDiscItem) -> bool;

    fn do_dequeue(&mut self) -> Option<QueueDiscItem>;

    fn do_peek(&self) -> Option<&QueueDiscItem>;

    fn check_config(&self) -> Result<(), String>;

    fn initialize_params(&mut self) -> Result<(), String>;

    fn size_policy(&self) -> QueueDiscSizePolicy {
        self.base().size_policy()
    }

    fn max_size(&self) -> Option<QueueSize> {
        self.base().max_size()
    }

    fn set_max_size(&mut self, size: QueueSize) -> bool {
        self.base_mut().set_max_size(size)
    }

    fn current_size(&self) -> QueueSize {
        self.base().current_size()
    }

    fn stats(&self) -> &QueueDiscStats {
        self.base().stats()
    }

    fn stats_mut(&mut self) -> &mut QueueDiscStats {
        self.base_mut().stats_mut()
    }

    fn enqueue(&mut self, item: QueueDiscItem) -> bool {
        let bytes = item.len_bytes();
        self.base_mut().stats_mut().record_received(bytes);
        if self.do_enqueue(item) {
            self.base_mut().on_packet_enqueued(bytes);
            return true;
        }
        false
    }

    fn dequeue(&mut self) -> Option<QueueDiscItem> {
        let item = self.do_dequeue()?;
        let bytes = item.len_bytes();
        self.base_mut().on_packet_dequeued(bytes);
        Some(item)
    }

    fn peek(&self) -> Option<&QueueDiscItem> {
        self.do_peek()
    }

    fn drop_before_enqueue(&mut self, item: &QueueDiscItem, reason: &str) {
        self.base_mut().stats_mut().record_drop(
            item.len_bytes(),
            reason,
            QueueDropPhase::BeforeEnqueue,
        );
    }

    fn drop_after_dequeue(&mut self, item: &QueueDiscItem, reason: &str) {
        self.base_mut().stats_mut().record_drop(
            item.len_bytes(),
            reason,
            QueueDropPhase::AfterDequeue,
        );
    }

    fn mark_packet(&mut self, item: &QueueDiscItem, reason: &str) {
        self.base_mut()
            .stats_mut()
            .record_marked(item.len_bytes(), reason);
    }
}

