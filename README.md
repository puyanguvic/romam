# Routing Protocol Framework (Containerlab Only)

This repository has been cleaned to keep only the containerlab-based experiment workflow.
Mininet and pure in-process simulation components have been removed.

## What Remains

- Containerlab OSPF convergence runner: `exps/ospf_convergence_exp.py`
- Topology utilities used by the runner: `src/rpf/core/topology.py`
- Shared file/time helpers: `src/rpf/utils/io.py`
- Make target for running the experiment: `make run-ospf-convergence-exp` (legacy alias: `make run-containerlab-exp`)

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
