# Experiments Guide

This directory keeps experiment tooling focused on reusable entrypoints.

## Canonical Entrypoints

- `generate_routerd_lab.py`
  Generate per-node router configs and deployment env from a topology/profile.
- `run_routerd_lab.py`
  End-to-end lab lifecycle: generate, deploy, health-check, optional destroy.
- `check_routerd_lab.py`
  Runtime checks for processes, API reachability, routing tables, and neighbors.
- `run_unified_experiment.py`
  Single YAML workflow for scenario mode (apps/faults/polling) and convergence
  benchmark mode (repeated deploy + per-link ping probe + summary table).
- `run_traffic_app.py`
  Run one traffic app command inside a router container.
- `run_traffic_probe.py`
  One-shot sender/sink probe with JSON result output.
- `run_traffic_plan.py`
  Execute ordered traffic tasks from a plan file.
- `install_traffic_app_bin.py`
  Copy host-built `traffic_app` into routers in a running lab.
- `ospf_convergence_exp.py`
  Compatibility wrapper that maps legacy CLI flags to unified benchmark mode.

## Config And Assets

- `routerd_examples/`
  Protocol configs, unified experiment YAMLs, and traffic plans.
- `container_images/`
  Dockerfiles used by experiment workflows.

## Recommended Use

1. For protocol/lab validation: `run_routerd_lab.py` + `check_routerd_lab.py`.
2. For reproducible scenario experiments: `run_unified_experiment.py` with YAML under `routerd_examples/unified_experiments/`.
3. For data-plane only tests: `run_traffic_probe.py` or `run_traffic_plan.py`.
