# Routing Protocol Framework

A lightweight, extensible routing protocol framework for emulation, protocol comparison, and reproducible evaluation.

## Features

- Tick-based simulation engine for deterministic experiments.
- Pluggable routing protocols (`ospf_like`, `rip_like`, `ecmp`).
- Config-driven experiments (topology, failures, metrics drift, sweeps).
- Structured run logs (`jsonl`) and summary tables.
- Containerlab support via a standalone `1ms` link-delay experiment script.

## Quick Start

```bash
make install
make test
make run-emu CONFIG=configs/experiments/failure_recovery.yaml
```

or with CLI:

```bash
rpf run --config configs/experiments/failure_recovery.yaml
```

## Experiment Environment Setup (Ubuntu)

1. Install Python dependencies:

```bash
make install
```

2. Ensure Docker is installed and running.

3. Install a local `containerlab` binary (required for this project):

```bash
mkdir -p "$HOME/.local/bin"
CLAB_VER=0.73.0
curl -fL -o /tmp/containerlab.tar.gz \
  "https://github.com/srl-labs/containerlab/releases/download/v${CLAB_VER}/containerlab_${CLAB_VER}_linux_amd64.tar.gz"
tar -xzf /tmp/containerlab.tar.gz -C /tmp
install -m 0755 /tmp/containerlab "$HOME/.local/bin/containerlab"
containerlab version
```

If `containerlab` is not found, add `~/.local/bin` to `PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

4. If Docker requires root in your environment, run experiments with:

```bash
EXP_USE_SUDO=1
```

## Containerlab Experiment (1ms Link Delay)

Run the standalone containerlab coverage experiment (all links use `1ms` delay):

```bash
make run-containerlab-exp EXP_N_NODES=20 EXP_REPEATS=3 EXP_TOPOLOGY=er EXP_ER_P=0.2
```

Prerequisites:

- Use a locally installed `containerlab` binary (see setup section above).
- The experiment script invokes host `containerlab` directly (`deploy ... --reconfigure` / `destroy ... --cleanup`), not the clab Docker image.
- `docker compose --profile clab ...` is no longer supported for this experiment.
- If your environment needs root privileges for Docker/containerlab, set `EXP_USE_SUDO=1`.
- Prefer `make run-containerlab-exp EXP_USE_SUDO=1 ...` instead of `sudo make ...` to avoid PATH differences.
- The script auto-selects a free containerlab management subnet to avoid Docker network conflicts.
- Management `external-access` is disabled by default.

or directly:

```bash
python3 exps/ospf_coverage_containerlab_exp.py --n-nodes 20 --repeats 3 --topology er --er-p 0.2
```

If you need fixed management subnets:

```bash
make run-containerlab-exp EXP_USE_SUDO=1 \
  EXP_MGMT_NETWORK_NAME=clab-mgmt-romam \
  EXP_MGMT_IPV4_SUBNET=10.250.10.0/24 \
  EXP_MGMT_IPV6_SUBNET=fd00:fa:10::/64
```

If you explicitly need management external access rules:

```bash
make run-containerlab-exp EXP_USE_SUDO=1 EXP_MGMT_EXTERNAL_ACCESS=1
```

Outputs:

- Run artifacts: `results/runs/ospf_coverage_containerlab/`
- Aggregated tables: `results/tables/ospf_coverage_containerlab_n*.json` and `.csv`

## Project Layout

The project follows the structure described in your design with `core`, `protocols`, `backends`, `eval`, `cli`, `utils`, `scripts`, and `tests` modules.

## Output

By default, outputs are stored under `results/`:

- `results/runs/` raw run artifacts
- `results/tables/` summary tables
- `results/figs/` charts

## Notes

- This baseline implementation focuses on emulation backend and deterministic tick engine.
- A standalone containerlab experiment script is available at `exps/ospf_coverage_containerlab_exp.py`.
