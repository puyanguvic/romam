use std::collections::BTreeMap;

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::model::control_plane::MessageDescriptor;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MessageKind {
    Hello,
    OspfLsa,
    DdrLsa,
    RipUpdate,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ControlMessage {
    pub protocol: String,
    pub kind: MessageKind,
    pub src_router_id: u32,
    pub seq: u64,
    #[serde(default)]
    pub descriptor: MessageDescriptor,
    #[serde(default)]
    pub payload: BTreeMap<String, Value>,
    pub ts: f64,
}

impl ControlMessage {
    pub fn new(
        protocol: impl Into<String>,
        kind: MessageKind,
        src_router_id: u32,
        seq: u64,
        descriptor: MessageDescriptor,
        payload: BTreeMap<String, Value>,
        ts: f64,
    ) -> Self {
        Self {
            protocol: protocol.into(),
            kind,
            src_router_id,
            seq,
            descriptor,
            payload,
            ts,
        }
    }
}

pub fn encode_message(message: &ControlMessage) -> Result<Vec<u8>> {
    serde_json::to_vec(message).context("failed to encode control message")
}

pub fn decode_message(data: &[u8]) -> Result<ControlMessage> {
    serde_json::from_slice(data).context("failed to decode control message")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn message_roundtrip() {
        let mut payload = BTreeMap::new();
        payload.insert("router_id".to_string(), Value::from(7_u64));
        payload.insert("note".to_string(), Value::from("hello"));

        let msg = ControlMessage {
            protocol: "ospf".to_string(),
            kind: MessageKind::Hello,
            src_router_id: 1,
            seq: 3,
            descriptor: MessageDescriptor::hello(),
            payload,
            ts: 12.5,
        };

        let encoded = encode_message(&msg).expect("encode should succeed");
        let decoded = decode_message(&encoded).expect("decode should succeed");
        assert_eq!(decoded, msg);
    }

    #[test]
    fn decode_legacy_message_without_descriptor() {
        let raw = br#"{
            "protocol": "ospf",
            "kind": "hello",
            "src_router_id": 1,
            "seq": 2,
            "payload": {"router_id": 1},
            "ts": 3.0
        }"#;
        let decoded = decode_message(raw).expect("legacy decode should succeed");
        assert_eq!(decoded.descriptor, MessageDescriptor::default());
    }
}
