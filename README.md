# Intelligent Routing Protocol (IRP) Framework (Containerlab + Per-Router Daemon)

This project now follows a routing-suite style architecture:
- one `routerd` process per router container,
- protocol engines (OSPF / RIP) running inside that daemon,
- containerlab only manages containers/links/namespaces/lifecycle/fault injection.

The control plane is organized into four core parts:
- control messages (`src/irp_rust/src/model/messages.rs`)
- local protocol state (`src/irp_rust/src/model/state.rs`)
- routing table / RIB (`src/irp_rust/src/model/routing.rs`)
- forwarding / FIB apply layer (`src/irp_rust/src/runtime/forwarding.rs`)

Code layout keeps protocol implementation and experiment topology tooling parallel:
- device-side routing protocol/runtime: `src/irp_rust/src/protocols/` + `src/irp_rust/src/runtime/`
- experiment-side topology loading/lab tooling: `src/topology/`

## Main Components

- Router daemon runtime (extensible core): `src/irp_rust/src/runtime/daemon.rs`
- Protocol engines:
  - OSPF-like link-state: `src/irp_rust/src/protocols/ospf.rs`
  - RIP distance-vector: `src/irp_rust/src/protocols/rip.rs`
- Config loader: `src/irp_rust/src/runtime/config.rs`
- Topology file loader + lab tools: `src/topology/clab_loader.py`, `src/topology/labgen.py`
- Example daemon configs: `exps/routerd_examples/`
- Experiment utilities: `exps/ospf_convergence_exp.py`

## Setup

1. Install project dependencies:

```bash
make install
```

2. Ensure Docker is installed and running.

3. Install `containerlab` locally (example):

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

## Run Router Daemon (Inside Each Router Container)

Per container (router) run one daemon:

```bash
make build-routerd-rs
make run-routerd-rs ROUTERD_RS_CONFIG=/path/to/router.yaml ROUTERD_RS_LOG_LEVEL=INFO
```

Rust extensibility points:
- protocol abstraction: `src/irp_rust/src/protocols/base.rs`
- decision/policy hook (default passthrough): `src/irp_rust/src/algo/mod.rs`
- rust core notes: `src/irp_rust/README.md`

OSPF example config: `exps/routerd_examples/ospf_router1.yaml`  
RIP example config: `exps/routerd_examples/rip_router1.yaml`

### Config Shape

```yaml
router_id: 1
protocol: ospf  # or rip
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
python3 exps/generate_routerd_lab.py --profile star6 --protocol rip
```

Built-in profiles (few common topologies, one-arg selection):
- `line5`
- `ring6`
- `star6`
- `fullmesh4`
- `spineleaf2x4`

For a custom topology file, use `--topology-file`:

```bash
python3 exps/generate_routerd_lab.py \
  --protocol rip \
  --topology-file clab_topologies/spineleaf2x4.clab.yaml
```

`--protocol` is independent from topology file, so the same file can run either `ospf` or `rip`.

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
`run-routerd-lab`. Topology files in `clab_topologies/` are parameterized with environment
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

## Validate A Running Lab

After `containerlab deploy`, run:

```bash
make check-routerd-lab \
  CHECK_TOPOLOGY_FILE=clab_topologies/ring6.clab.yaml \
  CHECK_LAB_NAME=<lab_name> \
  CHECK_CONFIG_DIR=results/runs/routerd_labs/<lab_name>/configs \
  CHECK_USE_SUDO=1 \
  CHECK_EXPECT_PROTOCOL=ospf
```

What it checks per node:
- container is running,
- `irp_routerd_rs` process exists,
- neighbor IP ping (from generated config) succeeds,
- latest `RIB/FIB updated` route count is at least `n_nodes-1` (or `CHECK_MIN_ROUTES`).

By default checker waits up to `10s` for early convergence logs (`CHECK_MAX_WAIT_S`).

## Run OSPF Convergence Demo

```bash
make run-ospf-convergence-exp EXP_TOPOLOGY_FILE=clab_topologies/ring6.clab.yaml EXP_REPEATS=1
```

Direct script usage:

```bash
python3 exps/ospf_convergence_exp.py --topology-file clab_topologies/ring6.clab.yaml --repeats 1
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
make run-ospf-convergence-exp EXP_TOPOLOGY_FILE=clab_topologies/spineleaf2x4.clab.yaml
```

## Run Sender / Sink Apps On Routers (UDP + TCP)

This repo includes a lightweight traffic app:
- go binary: `/irp/bin/traffic_app`
- roles: `sink` and `send`
- protocols: `udp` and `tcp`
- patterns: `bulk` and `onoff` (for sender)

When lab is started via `run-routerd-lab`, Rust routerd binary (`/irp/bin/irp_routerd_rs`)
and the data-plane binary (`/irp/bin/traffic_app`) are copied into each container.
`run-traffic-app` and `run-traffic-probe` use `/irp/bin/traffic_app` directly.

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
python3 exps/run_traffic_app.py --lab-name <lab_name> --node r1 -- \
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

## Line Topology OSPF UDP Experiment (r1 -> rN)

This experiment uses the line topology (`line5`) and OSPF, then sends UDP traffic from
the first node to the last node (for `line5`: `r1 -> r5`).
Before sending, it now runs an explicit convergence wait check (`check_routerd_lab`).

```bash
make run-line-ospf-udp-exp \
  LINE_EXP_PACKET_SIZE=512 \
  LINE_EXP_COUNT=5000 \
  LINE_EXP_PPS=1000 \
  LINE_EXP_OUTPUT_JSON=results/runs/line_ospf_udp_exp.json
```

Useful options:
- `LINE_EXP_USE_SUDO=1` (default) for docker/containerlab operations.
- `LINE_EXP_KEEP_LAB=1` to keep the lab after probe.
- `LINE_EXP_TOPOLOGY_FILE=<path>` to override built-in `line5` topology.
- This experiment enables routerd forwarding automatically (`--forwarding-enabled --no-forwarding-dry-run`).
- `LINE_EXP_LAB_CHECK_MIN_ROUTES=0` (default) relaxes route-count gating in lab pre-check.
- `LINE_EXP_LAB_CHECK_MAX_WAIT_S=20` controls OSPF pre-check wait window.
- `LINE_EXP_CONVERGE_MAX_WAIT_S=60` controls explicit convergence wait before send.
- `LINE_EXP_CONVERGE_MIN_ROUTES=-1` means `n_nodes-1` routes required before send.
- `LINE_EXP_ALLOW_UNCONVERGED_SEND=1` can force-send even if convergence check fails.

## Outputs

- Per-run artifacts: `results/runs/ospf_convergence_containerlab/`
- Aggregated results: `results/tables/ospf_convergence_containerlab_<topology>.json` and `.csv`
