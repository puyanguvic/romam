#!/usr/bin/env bash
set -euo pipefail

python -m rpf.eval.summarize --runs "${RUNS_DIR:-results/runs}" --out "${OUT_FILE:-results/tables/summary.csv}"
