use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum StateClass {
    Liveness,
    Topology,
    DistanceVector,
    NeighborFastState,
    Opaque,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ExchangeScope {
    OneHop,
    FloodDomain,
    RouterDomain,
}

impl ExchangeScope {
    pub fn from_str(raw: &str) -> Option<Self> {
        match raw.trim().to_ascii_lowercase().as_str() {
            "one_hop" | "one-hop" | "neighbor" | "neighbor_only" => Some(Self::OneHop),
            "flood_domain" | "flood-domain" | "flood" | "area" => Some(Self::FloodDomain),
            "router_domain" | "router-domain" | "global" | "domain" => Some(Self::RouterDomain),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::OneHop => "one_hop",
            Self::FloodDomain => "flood_domain",
            Self::RouterDomain => "router_domain",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ExchangeMode {
    Periodic,
    Triggered,
    Hybrid,
}

fn default_schema_version() -> u16 {
    1
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MessageDescriptor {
    #[serde(default = "default_schema_version")]
    pub schema_version: u16,
    #[serde(default)]
    pub state_class: Option<StateClass>,
    #[serde(default)]
    pub scope: Option<ExchangeScope>,
    #[serde(default)]
    pub mode: Option<ExchangeMode>,
    #[serde(default)]
    pub max_age_s: Option<u32>,
}

impl Default for MessageDescriptor {
    fn default() -> Self {
        Self {
            schema_version: default_schema_version(),
            state_class: None,
            scope: None,
            mode: None,
            max_age_s: None,
        }
    }
}

impl MessageDescriptor {
    pub fn hello() -> Self {
        Self {
            state_class: Some(StateClass::Liveness),
            scope: Some(ExchangeScope::OneHop),
            mode: Some(ExchangeMode::Periodic),
            ..Self::default()
        }
    }

    pub fn topology_lsa(max_age_s: Option<u32>) -> Self {
        Self {
            state_class: Some(StateClass::Topology),
            scope: Some(ExchangeScope::FloodDomain),
            mode: Some(ExchangeMode::Hybrid),
            max_age_s,
            ..Self::default()
        }
    }

    pub fn rip_update(max_age_s: Option<u32>) -> Self {
        Self {
            state_class: Some(StateClass::DistanceVector),
            scope: Some(ExchangeScope::OneHop),
            mode: Some(ExchangeMode::Hybrid),
            max_age_s,
            ..Self::default()
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RecordFreshness {
    Fresh,
    Stale,
    Expired,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct StateLifetimePolicy {
    pub fresh_for_s: f64,
    pub stale_for_s: f64,
}

impl StateLifetimePolicy {
    pub fn strict(max_age_s: f64) -> Self {
        Self {
            fresh_for_s: max_age_s.max(0.0),
            stale_for_s: 0.0,
        }
    }

    pub fn with_stale_window(max_age_s: f64, stale_window_s: f64) -> Self {
        Self {
            fresh_for_s: max_age_s.max(0.0),
            stale_for_s: stale_window_s.max(0.0),
        }
    }

    pub fn classify(self, learned_at: f64, now: f64) -> RecordFreshness {
        let age_s = (now - learned_at).max(0.0);
        if age_s <= self.fresh_for_s {
            return RecordFreshness::Fresh;
        }
        if age_s <= (self.fresh_for_s + self.stale_for_s) {
            return RecordFreshness::Stale;
        }
        RecordFreshness::Expired
    }

    pub fn is_usable(self, learned_at: f64, now: f64) -> bool {
        !matches!(self.classify(learned_at, now), RecordFreshness::Expired)
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ExchangePolicy {
    pub periodic_interval_s: Option<f64>,
    pub min_trigger_spacing_s: f64,
}

impl ExchangePolicy {
    pub fn periodic(interval_s: f64) -> Self {
        Self {
            periodic_interval_s: Some(interval_s.max(0.0)),
            min_trigger_spacing_s: 0.0,
        }
    }

    pub fn hybrid(interval_s: f64, min_trigger_spacing_s: f64) -> Self {
        Self {
            periodic_interval_s: Some(interval_s.max(0.0)),
            min_trigger_spacing_s: min_trigger_spacing_s.max(0.0),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ExchangeState {
    last_periodic_at: f64,
    last_triggered_at: f64,
}

impl Default for ExchangeState {
    fn default() -> Self {
        Self {
            last_periodic_at: -1e9,
            last_triggered_at: -1e9,
        }
    }
}

impl ExchangeState {
    pub fn periodic_due(&mut self, now: f64, policy: ExchangePolicy) -> bool {
        let Some(interval_s) = policy.periodic_interval_s else {
            return false;
        };
        if (now - self.last_periodic_at) < interval_s {
            return false;
        }
        self.last_periodic_at = now;
        true
    }

    pub fn trigger_due(&mut self, now: f64, policy: ExchangePolicy) -> bool {
        if (now - self.last_triggered_at) < policy.min_trigger_spacing_s {
            return false;
        }
        self.last_triggered_at = now;
        true
    }

    pub fn mark_periodic(&mut self, now: f64) {
        self.last_periodic_at = now;
    }

    pub fn mark_triggered(&mut self, now: f64) {
        self.last_triggered_at = now;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn freshness_classification_obeys_windows() {
        let policy = StateLifetimePolicy::with_stale_window(5.0, 2.0);
        assert_eq!(policy.classify(0.0, 4.0), RecordFreshness::Fresh);
        assert_eq!(policy.classify(0.0, 6.0), RecordFreshness::Stale);
        assert_eq!(policy.classify(0.0, 8.1), RecordFreshness::Expired);
    }

    #[test]
    fn exchange_state_gates_periodic_and_trigger() {
        let mut state = ExchangeState::default();
        let policy = ExchangePolicy::hybrid(3.0, 1.0);

        assert!(state.periodic_due(0.0, policy));
        assert!(!state.periodic_due(2.0, policy));
        assert!(state.periodic_due(3.1, policy));

        assert!(state.trigger_due(5.0, policy));
        assert!(!state.trigger_due(5.5, policy));
        assert!(state.trigger_due(6.1, policy));
    }
}
