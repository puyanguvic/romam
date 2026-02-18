# IRP Routingd (Rust)

This crate provides the `routingd` daemon used by containerlab router nodes.

## Components

- `runtime/daemon.rs`: main event loop for protocol ticks and UDP message handling.
- `runtime/forwarding.rs`: Linux FIB installer (`ip route`) with dry-run support.
- `runtime/mgmt.rs`: HTTP management API and gRPC placeholder listener.
- `runtime/config.rs`: YAML config loader for protocol/forwarding/management settings.
- `protocols/ospf.rs` and `protocols/rip.rs`: protocol engines.
- `algo/mod.rs`: policy hook for route selection.

## Binaries

- `routingd`
- `irp_routerd_rs` (compatibility alias)

## Run

```bash
cargo run --manifest-path src/irp/Cargo.toml --bin routingd -- \
  --config experiments/routerd_examples/ospf_router1.yaml \
  --log-level INFO
```

## Management API

When enabled in config, each daemon exposes:

- `GET /healthz`
- `GET /v1/status`
- `GET /v1/routes`
- `GET /v1/fib`
- `GET /v1/kernel-routes`
