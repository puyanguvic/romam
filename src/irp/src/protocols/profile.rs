use std::collections::BTreeMap;

use serde_json::{json, Value};

fn strv(items: &[&str]) -> Value {
    Value::Array(items.iter().map(|item| json!(item)).collect())
}

fn profile_for(protocol: &str) -> BTreeMap<String, Value> {
    let mut out = BTreeMap::new();
    out.insert("design_profile".to_string(), json!("irp-layered-v1"));
    out.insert("control_messaging".to_string(), json!("distributed"));
    out.insert("slow_state_scope".to_string(), json!("global"));

    match protocol {
        "rip" => {
            out.insert("base_routing_model".to_string(), json!("distance_vector"));
            out.insert("fast_state_enabled".to_string(), json!(false));
            out.insert("fast_state_scope".to_string(), json!("none"));
            out.insert("adaptive_forwarding_enabled".to_string(), json!(false));
            out.insert("decision_layers".to_string(), strv(&["base_routing"]));
            out.insert("fast_state_signals".to_string(), strv(&[]));
        }
        "ddr" | "dgr" | "octopus" => {
            out.insert("base_routing_model".to_string(), json!("link_state_ksp"));
            out.insert("fast_state_enabled".to_string(), json!(true));
            out.insert("fast_state_scope".to_string(), json!("local_neighbor"));
            out.insert("adaptive_forwarding_enabled".to_string(), json!(true));
            out.insert(
                "decision_layers".to_string(),
                strv(&["base_routing", "adaptive_forwarding"]),
            );
            out.insert(
                "fast_state_signals".to_string(),
                strv(&[
                    "neighbor_queue_level",
                    "local_queue_delay_ms",
                    "local_queue_depth_bytes",
                ]),
            );
        }
        _ => {
            out.insert("base_routing_model".to_string(), json!("link_state"));
            out.insert("fast_state_enabled".to_string(), json!(false));
            out.insert("fast_state_scope".to_string(), json!("none"));
            out.insert("adaptive_forwarding_enabled".to_string(), json!(false));
            out.insert("decision_layers".to_string(), strv(&["base_routing"]));
            out.insert("fast_state_signals".to_string(), strv(&[]));
        }
    }

    out
}

pub fn build_protocol_metrics(
    protocol: &str,
    runtime_metrics: BTreeMap<String, Value>,
) -> BTreeMap<String, Value> {
    let mut merged = profile_for(protocol);
    for (key, value) in runtime_metrics {
        merged.insert(key, value);
    }
    merged
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rip_profile_marks_slow_state_only() {
        let metrics = build_protocol_metrics("rip", BTreeMap::new());
        assert_eq!(
            metrics
                .get("adaptive_forwarding_enabled")
                .and_then(Value::as_bool),
            Some(false)
        );
        assert_eq!(
            metrics.get("fast_state_scope").and_then(Value::as_str),
            Some("none")
        );
    }

    #[test]
    fn octopus_profile_marks_local_fast_state() {
        let metrics = build_protocol_metrics("octopus", BTreeMap::new());
        assert_eq!(
            metrics
                .get("adaptive_forwarding_enabled")
                .and_then(Value::as_bool),
            Some(true)
        );
        assert_eq!(
            metrics.get("fast_state_scope").and_then(Value::as_str),
            Some("local_neighbor")
        );
    }
}
