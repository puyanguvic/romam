# IRP Routingd (Rust)

This crate provides the `routingd` daemon used by containerlab router nodes.

## Components

- `runtime/daemon.rs`: main event loop for protocol ticks and UDP message handling.
- `runtime/forwarding.rs`: Linux FIB installer (`ip route`) with dry-run support.
- `runtime/mgmt.rs`: HTTP management API and gRPC placeholder listener.
- `runtime/config.rs`: YAML config loader for protocol/forwarding/management settings.
- `protocols/{ospf,rip,ecmp,topk,ddr}.rs`: protocol engines (`dgr` and `octopus` are runtime modes on `ddr` core).
  - `ddr` core maintains neighbor fast-state in NSDB-style storage (queue/util/delay/loss) with freshness aging.
- `protocols/link_state.rs`: shared link-state control plane (hello/LSA/LSDB lifecycle).
- `protocols/route_compute.rs`: shared SPF/KSP route-computation helpers.
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

`GET /v1/metrics` includes protocol runtime counters plus IRP layered design tags
(`slow_state_scope`, `fast_state_scope`, `decision_layers`).
