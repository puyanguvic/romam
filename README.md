# Intelligent Routing Protocol (IRP) Routing Suite

This repo uses a layered architecture:
- `routingd` (Rust daemon) is the protocol and route-computation core.
- `node_supervisor` (Rust) is the node-local process manager for `routingd` and optional apps.
- containerlab only manages topology/container lifecycle.
- Go traffic apps are experiment tools and are configured/injected per node.

## Documentation

- Docs index: `docs/README.md`
- Quickstart: `docs/quickstart.md`
- Unified experiments: `docs/unified-experiments.md`
- Results and validation: `docs/results-and-validation.md`

## 5-Minute Quickstart

```bash
make install
make build-routerd-rs
make build-traffic-app-go

PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/line3_irp_multi_apps.yaml \
  --poll-interval-s 1 \
  --sudo
```

Validate outputs:

```bash
python3 tools/validate_unified_metrics.py \
  --input results/runs/unified_experiments \
  --recursive
```

Control/observability paths:
- route computation + RIB/FIB snapshot: `src/irp/src/runtime/daemon.rs`
- kernel route install/readback (netlink via `ip route`): `src/irp/src/runtime/forwarding.rs`
- HTTP management API (`/v1/status`, `/v1/routes`, `/v1/fib`, `/v1/kernel-routes`): `src/irp/src/runtime/mgmt.rs`
- node-level process state (`/tmp/node_supervisor_state.json`): `src/irp/src/bin/node_supervisor.rs`

## Main Components

- Router daemon runtime (extensible core): `src/irp/src/runtime/daemon.rs`
- Protocol engines:
  - OSPF-like link-state: `src/irp/src/protocols/ospf.rs`
  - RIP distance-vector: `src/irp/src/protocols/rip.rs`
  - ECMP equal-cost multipath baseline: `src/irp/src/protocols/ecmp.rs`
  - Top-K random multipath baseline: `src/irp/src/protocols/topk.rs`
  - DDR/DGR delay-aware routing core: `src/irp/src/protocols/ddr.rs`
    - uses `tc -s qdisc` backlog when neighbor `iface` is present in config; converts queue bytes to delay via `link_bandwidth_bps` (falls back to local estimator otherwise)
  - IRP mode entry: `protocol: irp` (currently routed through the OSPF-style core with IRP params)
- Decision/policy hook: `src/irp/src/algo/mod.rs`
- Management API: `src/irp/src/runtime/mgmt.rs` (HTTP + gRPC placeholder)
- Runtime entrypoint: `src/irp/src/main.rs` (`routingd` binary)
- Node process supervisor: `src/irp/src/bin/node_supervisor.rs`
- Topology file loader + lab tools: `src/clab/clab_loader.py`, `src/clab/labgen.py`
- Example daemon configs: `experiments/routerd_examples/`
- Experiment utilities index: `experiments/README.md`

### Layout Conventions

- Canonical automation entrypoints: `tools/`
- Canonical experiment assets/configs: `experiments/`

## Setup

1. Install project dependencies:

```bash
make install
```

2. Ensure Docker is installed and running.

3. Install Rust musl target (required for router binaries running in router container image):

```bash
rustup target add x86_64-unknown-linux-musl
```

4. Install `containerlab` locally (example):

```bash
mkdir -p "$HOME/.local/bin"
CLAB_VER=0.73.0
curl -fL -o /tmp/containerlab.tar.gz \
  "https://github.com/srl-labs/containerlab/releases/download/v${CLAB_VER}/containerlab_${CLAB_VER}_linux_amd64.tar.gz"
tar -xzf /tmp/containerlab.tar.gz -C /tmp
install -m 0755 /tmp/containerlab "$HOME/.local/bin/containerlab"
containerlab version
```

If needed:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

If musl link tools are missing on Debian/Ubuntu:

```bash
sudo apt-get update && sudo apt-get install -y musl-tools
```

## Run Router Daemon (Inside Each Router Container)

Per container (router) run one daemon:

```bash
make build-routerd-rs
make run-routerd-rs ROUTERD_RS_CONFIG=/path/to/router.yaml ROUTERD_RS_LOG_LEVEL=INFO
```

Rust extensibility points:
- protocol abstraction: `src/irp/src/protocols/base.rs`
- decision/policy hook: `src/irp/src/algo/mod.rs`
- rust core notes: `src/irp/README.md`

OSPF example config: `experiments/routerd_examples/ospf_router1.yaml`  
RIP example config: `experiments/routerd_examples/rip_router1.yaml`

### Config Shape

```yaml
router_id: 1
protocol: ospf  # or rip/ecmp/topk/ddr/dgr/irp
bind:
  address: 0.0.0.0
  port: 5500
timers:
  tick_interval: 1.0
  dead_interval: 4.0
neighbors:
  - router_id: 2
    address: 10.0.12.2
    port: 5500
    cost: 1.0
protocol_params:
  ospf:
    hello_interval: 1.0
    lsa_interval: 3.0
forwarding:
  enabled: false
  dry_run: true
management:
  http:
    enabled: true
    bind: 0.0.0.0
    port: 18001
  grpc:
    enabled: true
    bind: 0.0.0.0
    port: 19001
```

`forwarding.enabled=true` enables Linux route programming (`ip route`) based on protocol output.
You need `destination_prefixes` and `next_hop_ips` mappings in config to install concrete kernel routes.

## Generate Routerd Configs (File-Driven Topology)

Use the generator to create per-node `routerd` configs and a deploy env file.
Containerlab keeps using the original topology file directly:

```bash
make gen-routerd-lab LABGEN_PROFILE=ring6 LABGEN_PROTOCOL=ospf
```

Or run directly:

```bash
python3 tools/generate_routerd_lab.py --profile star6 --protocol rip
```

Built-in profiles (few common topologies, one-arg selection):
- `line3`
- `line5`
- `ring6`
- `star6`
- `fullmesh4`
- `spineleaf2x4`

For a custom topology file, use `--topology-file`:

```bash
python3 tools/generate_routerd_lab.py \
  --protocol rip \
  --topology-file src/clab/topologies/spineleaf2x4.clab.yaml
```

`--protocol` is independent from topology file, so the same file can run `ospf`, `rip`, `ecmp`, `topk`, `ddr`, `dgr`, or `irp`.

DDR validation config example:

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/line3_ddr_validation.yaml \
  --poll-interval-s 1 \
  --sudo
```

ECMP validation config example:

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/line3_ecmp_validation.yaml \
  --poll-interval-s 1 \
  --sudo
```

Top-K random routing validation config example:

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/line3_topk_validation.yaml \
  --poll-interval-s 1 \
  --sudo
```

DGR validation config example:

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/line3_dgr_validation.yaml \
  --poll-interval-s 1 \
  --sudo
```

`dgr` routing params support queue-level back-pressure controls:
- `queue_levels`
- `pressure_threshold`
- `queue_level_scale_ms`
- `randomize_route_selection`
- `rng_seed`

## One-Command Lab Run

For a quick end-to-end run (generate + deploy + health-check + destroy):

```bash
make run-routerd-lab LABGEN_PROFILE=ring6 LABGEN_PROTOCOL=rip
```

Keep the lab after checks:

```bash
make run-routerd-lab LABGEN_PROFILE=ring6 LABGEN_PROTOCOL=rip RUNLAB_KEEP_LAB=1
```

Generated assets are written under:
- `results/runs/routerd_labs/<lab_name>/configs/*.yaml`
- `results/runs/routerd_labs/<lab_name>/deploy.env`

`deploy.env` carries containerlab variable overrides (`name/mgmt/image`) and is consumed by
`run-routerd-lab`. Topology files in `src/clab/topologies/` are parameterized with environment
variables, so the same source file can be reused across runs.

### Common Pitfalls

- `Permission denied` under `results/runs/routerd_labs/...`:
  this is usually caused by previous `sudo make` creating root-owned files.
  Fix with:
  ```bash
  sudo chown -R "$USER:$USER" results/runs/routerd_labs
  ```
- `sudo: containerlab: command not found`:
  your `sudo` PATH may not include `~/.local/bin`.
  Use absolute path:
  ```bash
  sudo "$(which containerlab)" deploy -t <topology_file> --name <lab_name> --reconfigure
  ```
- Deploying the wrong run:
  always use the `lab_name` and `deploy_env_file` printed by your latest `make gen-routerd-lab`.
- `nohup: can't execute '/irp/bin/node_supervisor': No such file or directory` inside container:
  host-built Rust binary is not compatible with container libc.
  Rebuild with musl target:
  ```bash
  rustup target add x86_64-unknown-linux-musl
  make build-routerd-rs
  ```

## Validate A Running Lab

After `containerlab deploy`, run:

```bash
make check-routerd-lab \
  CHECK_TOPOLOGY_FILE=src/clab/topologies/ring6.clab.yaml \
  CHECK_LAB_NAME=<lab_name> \
  CHECK_CONFIG_DIR=results/runs/routerd_labs/<lab_name>/configs \
  CHECK_USE_SUDO=1 \
  CHECK_EXPECT_PROTOCOL=ospf
```

What it checks per node:
- container is running,
- `routingd` process exists (`/irp/bin/routingd` or compatibility alias),
- `node_supervisor` process exists and state file can be collected (`/tmp/node_supervisor_state.json`),
- management HTTP API (`/v1/status`) is reachable when configured,
- management kernel-route API (`/v1/kernel-routes`) can be queried,
- neighbor IP ping (from generated config) succeeds,
- latest `RIB/FIB updated` route count is at least `n_nodes-1` (or `CHECK_MIN_ROUTES`).

By default checker waits up to `10s` for early convergence logs (`CHECK_MAX_WAIT_S`).

## Run OSPF Convergence Demo

Recommended (unified benchmark config):

```bash
make run-unified-experiment \
  UNIFIED_CONFIG_FILE=experiments/routerd_examples/unified_experiments/ring6_ospf_convergence_benchmark.yaml \
  UNIFIED_USE_SUDO=1
```

Legacy-compatible wrapper (still supported):

```bash
make run-ospf-convergence-exp EXP_TOPOLOGY_FILE=src/clab/topologies/ring6.clab.yaml EXP_REPEATS=1
```

Direct script usage:

```bash
python3 tools/ospf_convergence_exp.py --topology-file src/clab/topologies/ring6.clab.yaml --repeats 1
```

If your environment requires privilege escalation for Docker/containerlab:

```bash
make run-ospf-convergence-exp EXP_USE_SUDO=1
```

Optional fixed management network/subnets:

```bash
make run-ospf-convergence-exp EXP_USE_SUDO=1 \
  EXP_MGMT_NETWORK_NAME=clab-mgmt-romam \
  EXP_MGMT_IPV4_SUBNET=10.250.10.0/24 \
  EXP_MGMT_IPV6_SUBNET=fd00:fa:10::/64
```

For `spineleaf2x4` convergence tests:

```bash
make run-ospf-convergence-exp EXP_TOPOLOGY_FILE=src/clab/topologies/spineleaf2x4.clab.yaml
```

## Run Sender / Sink Apps On Routers (UDP + TCP)

This repo includes a lightweight traffic app:
- go binary: `/irp/bin/traffic_app`
- roles: `sink` and `send`
- protocols: `udp` and `tcp`
- patterns: `bulk` and `onoff` (for sender)

Recommended workflow:
- build router image once: `make build-routerd-node-image`
- run lab with generated topology (default image `romam/network-multitool-routerd:latest`)

`run-routerd-lab` now prefers binaries already baked in image:
- `/irp/bin/routingd` (or compatibility alias `/irp/bin/irp_routerd_rs`)
- `/irp/bin/traffic_app`

If image is missing binaries, script falls back to host build + copy.

### Go data-plane setup on host (recommended)

Install Go on host and ensure `go` is in `PATH`, then build and install binary:

```bash
make build-traffic-app-go
make install-traffic-app-bin \
  INSTALL_TRAFFIC_BIN_LAB_NAME=<lab_name>
```

Optional architecture overrides (if needed):

```bash
make build-traffic-app-go TRAFFIC_GOOS=linux TRAFFIC_GOARCH=amd64 TRAFFIC_CGO_ENABLED=0
```

If lab is started via `run-routerd-lab`, it already attempts this build+copy step automatically
(using env overrides `ROMAM_TRAFFIC_GOOS`, `ROMAM_TRAFFIC_GOARCH`, `ROMAM_TRAFFIC_CGO_ENABLED`).

### Traffic plan (config-driven)

```bash
make run-traffic-plan TRAFFIC_PLAN_FILE=experiments/routerd_examples/traffic_plans/line5_udp.yaml
```

Plan runner:
- optional install step (`install_traffic_app_bin.py`)
- ordered per-node task launch (`run_traffic_app.py`)
- supports background sink and delayed sender start

## Unified Experiment YAML (Scenario + Benchmark)

Use one top-level YAML to run the full loop in either mode:
- `mode: scenario` (default): deploy topology + inject protocol config, write app specs
  into `node_supervisor`, launch apps, inject faults, and poll `/v1/routes` + `/v1/metrics`.
- `mode: convergence_benchmark`: run repeated deploy/precheck/ping-probe loops and export
  summary JSON/CSV.

Examples:
- `experiments/routerd_examples/unified_experiments/line3_irp_onoff_fault.yaml`
- `experiments/routerd_examples/unified_experiments/line3_irp_multi_apps.yaml`
- `experiments/routerd_examples/unified_experiments/line3_rip_validation.yaml`
- `experiments/routerd_examples/unified_experiments/ring6_ospf_convergence_benchmark.yaml`

Recommended run flow (verified on this repo):

```bash
make build-routerd-rs
make run-unified-experiment \
  UNIFIED_CONFIG_FILE=experiments/routerd_examples/unified_experiments/line3_irp_multi_apps.yaml \
  UNIFIED_USE_SUDO=1
```

Equivalent direct script run:

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/line3_irp_multi_apps.yaml \
  --poll-interval-s 1 \
  --sudo
```

RIP validation on 3-node line topology:

```bash
make run-unified-experiment \
  UNIFIED_CONFIG_FILE=experiments/routerd_examples/unified_experiments/line3_rip_validation.yaml \
  UNIFIED_USE_SUDO=1
```

Default outputs:
- `mode: scenario`: `results/runs/unified_experiments/<lab_name>/report_<timestamp>.json`
- `mode: convergence_benchmark`: `results/tables/<protocol>_convergence_unified_<topology>.json` and `.csv`
- Standardized artifacts (both modes):
  `results/runs/.../<run_id>/config.yaml`, `topology.yaml`, `traffic.yaml`, `logs/`,
  `metrics.json`, `summary.md`

Validate unified JSON outputs (lightweight schema checks):

```bash
python3 tools/validate_unified_metrics.py \
  --input results/runs/unified_experiments \
  --recursive
```

The unified config supports fault injection, e.g.:
- `link_down` with `faults[].link: [r2, r3]`
- `app_stop` / `app_start` with `faults[].node` + `faults[].app` (applied via `node_supervisor`)

Cleanup command (when you keep lab running or need manual cleanup):

```bash
sudo containerlab destroy -t src/clab/topologies/line3.clab.yaml --name <lab_name> --cleanup
```

### Multi-app AppSpec schema

`run_unified_experiment.py` supports:
- `mode`: `scenario` (default) or `convergence_benchmark`
- `node_apps`: list of `{node_id, apps[]}`
- `apps`: flat list where each app includes `node` or `node_id`

Each app entry supports:
- `name`, `kind` (`sender` / `sink` / `custom`)
- `bin`, `args`, `env`
- `restart` (`never` / `on-failure` / `always`) and `max_restarts`
- `delay_s`, `log_file`, optional `cpu_affinity`

Built-in guardrails:
- sink port conflict check on same node for `traffic_app` sink endpoints
- sender target guard (`localhost/127.0.0.1/::1` rejected)
- app lifecycle supervision + restart events included in report
- app env defaults: `APP_ID`, `NODE_ID`, `APP_ROLE`
- traffic app periodic stats can emit JSON when `TRAFFIC_LOG_JSON=1`

### 1) Start sink on `r6` (UDP, port 9000)

```bash
make run-traffic-app \
  TRAFFIC_LAB_NAME=<lab_name> \
  TRAFFIC_NODE=r6 \
  TRAFFIC_BACKGROUND=1 \
  TRAFFIC_LOG_FILE=/tmp/udp_sink.log \
  TRAFFIC_ARGS="sink --proto udp --bind 0.0.0.0 --port 9000 --report-interval-s 1"
```

### 2) Send UDP packets from `r1` to `r6`

```bash
make run-traffic-app \
  TRAFFIC_LAB_NAME=<lab_name> \
  TRAFFIC_NODE=r1 \
  TRAFFIC_ARGS="send --proto udp --target <r6_reachable_ip> --port 9000 --packet-size 256 --count 1000 --pps 200"
```

### 3) TCP mode example

Start TCP sink:

```bash
make run-traffic-app \
  TRAFFIC_LAB_NAME=<lab_name> \
  TRAFFIC_NODE=r6 \
  TRAFFIC_BACKGROUND=1 \
  TRAFFIC_LOG_FILE=/tmp/tcp_sink.log \
  TRAFFIC_ARGS="sink --proto tcp --bind 0.0.0.0 --port 9001"
```

Run TCP sender:

```bash
make run-traffic-app \
  TRAFFIC_LAB_NAME=<lab_name> \
  TRAFFIC_NODE=r1 \
  TRAFFIC_ARGS="send --proto tcp --target <r6_reachable_ip> --port 9001 --packet-size 1024 --duration-s 10 --pps 500 --tcp-nodelay"
```

### 3.1) OnOff pattern example (orthogonal to transport)

```bash
make run-traffic-app \
  TRAFFIC_LAB_NAME=<lab_name> \
  TRAFFIC_NODE=r1 \
  TRAFFIC_ARGS="send --proto udp --target <r6_reachable_ip> --port 9000 --pattern onoff --on-ms 2000 --off-ms 1000 --packet-size 1200 --duration-s 30 --pps 1000"
```

Direct script usage is also supported:

```bash
python3 tools/run_traffic_app.py --lab-name <lab_name> --node r1 -- \
  send --proto udp --target <ip> --port 9000 --count 100
```

### 4) One-command probe (auto start sink + run sender + collect report)

```bash
make run-traffic-probe \
  PROBE_LAB_NAME=<lab_name> \
  PROBE_SRC_NODE=r1 \
  PROBE_DST_NODE=r6 \
  PROBE_DST_IP=<r6_reachable_ip> \
  PROBE_PROTO=udp \
  PROBE_PACKET_SIZE=512 \
  PROBE_COUNT=5000 \
  PROBE_PPS=1000 \
  PROBE_OUTPUT_JSON=results/runs/traffic_probe_r1_r6.json
```

It prints a JSON report with sender throughput and sink log tail.

## Outputs

- Per-run artifacts: `results/runs/ospf_convergence_containerlab/`
- Aggregated results: `results/tables/ospf_convergence_containerlab_<topology>.json` and `.csv`
