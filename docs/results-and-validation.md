# Results and Validation

## Result Locations

- Unified scenario reports:
  `results/runs/unified_experiments/<lab_name>/report_<timestamp>.json`
- Unified benchmark per-run outputs:
  `results/runs/unified_experiments/convergence_benchmark/*.json` (or configured `run_output_dir`)
- Unified benchmark tables:
  `results/tables/<protocol>_convergence_unified_<topology>.json` and `.csv`

## Standardized Run Artifacts

Under each run directory:

- `config.yaml`
- `topology.yaml`
- `traffic.yaml`
- `logs/`
- `metrics.json`
- `summary.md`

## JSON Validation

Use the lightweight validator:

```bash
python3 tools/validate_unified_metrics.py --input <file-or-dir>
```

Useful flags:

- `--mode auto|scenario|benchmark_run|benchmark_summary`
- `--recursive / --no-recursive`
- `--fail-fast`

Example:

```bash
python3 tools/validate_unified_metrics.py \
  --input results/runs/unified_experiments \
  --recursive
```
