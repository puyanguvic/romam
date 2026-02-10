#!/usr/bin/env bash
set -euo pipefail

python -m rpf.cli.main run --config "${CONFIG:-configs/experiments/failure_recovery.yaml}"
