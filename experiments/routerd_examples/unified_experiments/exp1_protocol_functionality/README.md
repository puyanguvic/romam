# Exp1: Protocol Functionality + Convergence + Converged Route Tables

This experiment is the first functional validation suite for routing protocols.

Goals:
- verify each protocol can converge in a non-trivial topology (`abilene`)
- measure initial convergence time
- inject a deterministic link failure and measure reconvergence
- export route-table snapshots after convergence (before and after fault)

## Protocol Configs

- `ospf.yaml`
- `rip.yaml`
- `ecmp.yaml`
- `topk.yaml`
- `ddr.yaml`
- `dgr.yaml`
- `octopus.yaml`

All configs share the same scenario:
- topology: `abilene`
- traffic: `new_york -> los_angeles`, UDP bulk, multi-flow
- fault: `link_down [new_york, chicago]` at `t=30s`
- duration: `90s`

## Run One Protocol

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/exp1_protocol_functionality/ospf.yaml \
  --poll-interval-s 1 \
  --sudo
```

## Run Full Matrix + Auto Summary

```bash
PYTHONPATH=src python3 tools/run_exp1_protocol_functionality.py --sudo
```

Outputs:
- per-protocol raw reports: `results/runs/exp1_protocol_functionality_abilene/reports/`
- run logs: `results/runs/exp1_protocol_functionality_abilene/logs/`
- summary table: `results/tables/exp1_protocol_functionality_abilene.csv`
- summary JSON: `results/tables/exp1_protocol_functionality_abilene.json`
- converged route snapshots (Markdown):
  `results/tables/exp1_protocol_functionality_abilene_routes.md`
