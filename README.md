# Intelligent Routing Protocol (IRP) Framework (Containerlab + Per-Router Daemon)

This project now follows a routing-suite style architecture:
- one `routerd` process per router container,
- protocol engines (OSPF / RIP) running inside that daemon,
- containerlab only manages containers/links/namespaces/lifecycle/fault injection.

The control plane is organized into four core parts:
- control messages (`src/irp/model/messages.py`)
- local protocol state (`src/irp/model/state.py`)
- routing table / RIB (`src/irp/model/routing.py`)
- forwarding / FIB apply layer (`src/irp/runtime/forwarding.py`)

Code layout keeps protocol implementation and experiment topology tooling parallel:
- device-side routing protocol/runtime: `src/irp/protocols/` + `src/irp/runtime/`
- experiment-side topology/lab generation: `src/topology/`

## Main Components

- Router daemon runtime: `src/irp/routerd.py`, `src/irp/runtime/daemon.py`
- Protocol engines:
  - OSPF-like link-state: `src/irp/protocols/ospf.py`
  - RIP distance-vector: `src/irp/protocols/rip.py`
- Config loader: `src/irp/runtime/config.py`
- Topology model + lab generators: `src/topology/topology.py`, `src/topology/labgen.py`
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
python3 -m irp.routerd --config /path/to/router.yaml --log-level INFO
```

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

## Generate Containerlab Topology + Per-Router Configs

Use the generator to create one `.clab.yaml` and one `routerd` config per node:

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

For parameterized/random topologies, keep using `--topology` + size params, for example:

```bash
python3 exps/generate_routerd_lab.py --protocol rip --topology er --n-nodes 8 --seed 7
```

`--protocol` is independent from topology, so the same topology can run either `ospf` or `rip`.

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
- `results/runs/routerd_labs/<lab_name>/<lab_name>.clab.yaml`
- `results/runs/routerd_labs/<lab_name>/configs/r*.yaml`

The generator now writes a dedicated containerlab `mgmt` network block and auto-selects
free subnets when possible, to avoid default `clab` subnet conflicts.

The generated topology already includes node `exec` commands to:
- bring up data interfaces,
- assign /30 link IPs,
- start `python3 -m irp.routerd` inside each router container.

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
  sudo "$(which containerlab)" deploy -t <topology_file> --reconfigure
  ```
- Deploying the wrong run:
  always deploy the same `topology_file` path printed by your latest `make gen-routerd-lab`.

## Validate A Running Lab

After `containerlab deploy`, run:

```bash
make check-routerd-lab \
  CHECK_TOPOLOGY_FILE=results/runs/routerd_labs/<lab_name>/<lab_name>.clab.yaml \
  CHECK_USE_SUDO=1 \
  CHECK_EXPECT_PROTOCOL=ospf
```

What it checks per node:
- container is running,
- `irp.routerd` process exists,
- neighbor IP ping (from generated config) succeeds,
- latest `RIB/FIB updated` route count is at least `n_nodes-1` (or `CHECK_MIN_ROUTES`).

By default checker waits up to `10s` for early convergence logs (`CHECK_MAX_WAIT_S`).

## Run OSPF Convergence Demo

```bash
make run-ospf-convergence-exp EXP_N_NODES=6 EXP_REPEATS=1 EXP_TOPOLOGY=er EXP_ER_P=0.2
```

Direct script usage:

```bash
python3 exps/ospf_convergence_exp.py --n-nodes 6 --repeats 1 --topology er --er-p 0.2
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

## Outputs

- Per-run artifacts: `results/runs/ospf_convergence_containerlab/`
- Aggregated results: `results/tables/ospf_convergence_containerlab_n*.json` and `.csv`
