#!/usr/bin/env bash
set -euo pipefail

python -m rpf.cli.main run-mininet --config "${CONFIG:-configs/experiments/failure_recovery.yaml}"
