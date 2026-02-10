#!/usr/bin/env bash
set -euo pipefail

PYTHONPATH=src python -m rpf.cli.main run --config configs/experiments/failure_recovery.yaml
PYTHONPATH=src python -m rpf.cli.main run --config configs/experiments/metric_drift.yaml
PYTHONPATH=src python -m scripts.run_sweep --config configs/experiments/sweep_100_300_1000.yaml
PYTHONPATH=src python -m rpf.eval.summarize --runs results/runs --out results/tables/summary.csv
