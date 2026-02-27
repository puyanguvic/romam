# IRP Routingd (Rust)

This crate provides the `routingd` daemon used by containerlab router nodes.

## Components

- `runtime/daemon.rs`: main event loop for protocol ticks and UDP message handling.
- `runtime/forwarding.rs`: Linux FIB installer (`ip route`) with dry-run support.
- `runtime/mgmt.rs`: HTTP management API and gRPC placeholder listener.
- `runtime/config.rs`: YAML config loader for protocol/forwarding/management settings.
- `protocols/{ospf,rip,ecmp,topk,spath,ddr}.rs`: protocol engines (`dgr` and `octopus` are runtime modes on `ddr` core).
  - `ddr` core maintains neighbor fast-state in NSDB-style storage (queue/util/delay/loss) with freshness aging.
- `protocols/link_state.rs`: shared link-state control plane (hello/LSA/LSDB lifecycle).
- `protocols/route_compute/`: shared route-computation modules (SPF/KSP/DV/Bellman-Ford/CSPF/Pareto/neighbor-rooted forest/strategy layer).
- `algo/mod.rs`: policy hook for route selection.

## Route Compute Modules

`protocols/route_compute/` separates algorithm implementations from protocol state machines:

- `mod.rs`: module entrypoint and public re-exports.
- `traits.rs`: generic compute interface (`RouteComputeEngine`).
- `types.rs`: shared graph/result/input structs.
- `algorithms/`: one-folder-per-algorithm implementations (low coupling, package-style organization).
  - `algorithms/dijkstra/`: SPF single/ECMP/partial/incremental/LFA helpers.
  - `algorithms/bellman_ford/`: Bellman-Ford with negative-cycle marking.
  - `algorithms/yen/`: Yen K-shortest simple paths.
  - `algorithms/cspf/`: constrained shortest path filtering + SPF.
  - `algorithms/weighted_sum/`: weighted multi-metric scalarization + SPF.
  - `algorithms/pareto/`: non-dominated multi-objective path set.
- `dv.rs`: distance-vector candidate computation used by RIP.
- `neighbor_forest.rs`: neighbor-rooted forest/path construction used by DDR/DGR.
- `strategy.rs`: algorithm selection layer (Dijkstra/ECMP/Bellman-Ford/Yen/CSPF/Weighted/Pareto).

## Binaries

- `routingd`
- `irp_routerd_rs` (compatibility alias)
- `route_compute_bench` (algorithm runtime + path quality benchmark)

## Run

```bash
cargo run --manifest-path src/irp/Cargo.toml --bin routingd -- \
  --config experiments/routerd_examples/ospf_router1.yaml \
  --log-level INFO
```

Use strategy-driven link-state routing (`spath`) with runtime algorithm selection:

```yaml
protocol: spath
protocol_params:
  spath:
    algorithm: yen_ksp   # dijkstra | ecmp | bellman_ford | yen_ksp
    k_paths: 4
    hash_seed: 2026
```

Benchmark route-compute algorithms:

```bash
cargo run --bin route_compute_bench -- \
  --nodes 200 --density 0.08 --seeds 3 --iterations 8 \
  --output-json results/route_compute_bench/single_run.json

python3 utils/bench_shortest_path_matrix.py \
  --nodes-list 50,100,200 --density-list 0.05,0.1 \
  --output-dir results/route_compute_bench
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
