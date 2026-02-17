# IRP Rust Core

This crate provides a Rust foundation for IRP router daemon with explicit extension hooks.

## What is implemented

- `runtime::daemon`: per-router event loop, timer tick, packet RX/TX, RIB/FIB update flow.
- `runtime::config`: YAML config loader compatible with existing `routerd` config shape.
- `protocols::base`: protocol abstraction (`ProtocolEngine`, `ProtocolContext`, `ProtocolOutputs`).
- `protocols::ospf`: baseline OSPF-like link-state protocol.
- `protocols::rip`: baseline RIP distance-vector protocol.
- `algo::DecisionEngine`: route decision hook; default behavior is passthrough.

## Algorithm extension path

1. Keep protocol implementation stable in `protocols/*`.
2. Implement a custom decision engine in `algo/*` by implementing `DecisionEngine`.
3. Wire the engine into `runtime::daemon::RouterDaemon` (replace `PassthroughDecisionEngine`).
4. Keep fallback behavior deterministic when external AI/policy service is unavailable.

## Run

```bash
cargo run --manifest-path src/irp_rust/Cargo.toml -- \
  --config exps/routerd_examples/ospf_router1.yaml \
  --log-level INFO
```
