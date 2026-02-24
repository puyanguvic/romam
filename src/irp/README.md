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
- `protocols/route_compute/`: shared route-computation modules (SPF/KSP/DV/neighbor-rooted forest).
- `algo/mod.rs`: policy hook for route selection.

## Route Compute Modules

`protocols/route_compute/` separates algorithm implementations from protocol state machines:

- `mod.rs`: module entrypoint and public re-exports.
- `traits.rs`: generic compute interface (`RouteComputeEngine`).
- `types.rs`: shared graph/result/input structs.
- `spf.rs`: shortest-path computations for OSPF/ECMP.
- `ksp.rs`: K-shortest simple-path helper for TopK-like policies.
- `dv.rs`: distance-vector candidate computation used by RIP.
- `neighbor_forest.rs`: neighbor-rooted forest/path construction used by DDR/DGR.

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
