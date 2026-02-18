# Unified Experiments

Unified experiments are executed through:

```bash
tools/run_unified_experiment.py
```

## Modes

- `scenario`: deploy lab once, run apps/faults, poll management APIs.
- `convergence_benchmark`: repeated deploy + link probes + aggregate summary tables.

## Example Configs

- `experiments/routerd_examples/unified_experiments/line3_irp_multi_apps.yaml`
- `experiments/routerd_examples/unified_experiments/line3_rip_validation.yaml`
- `experiments/routerd_examples/unified_experiments/ring6_ospf_convergence_benchmark.yaml`
- `experiments/routerd_examples/unified_experiments/line3_irp_onoff_fault.yaml`

## Command Template

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config <config.yaml> \
  --sudo
```

Options:

- `--poll-interval-s <float>`: scenario polling interval.
- `--output-json <path>`: custom summary/report path.
- `--keep-lab`: do not destroy lab after run.

## Fault Injection

Supported in scenario mode:

- `link_down` with `faults[].link: [nodeA, nodeB]`
- `app_stop` / `app_start` with `faults[].node` + `faults[].app`

## Standardized Artifacts

Each run emits:

- `config.yaml`
- `topology.yaml`
- `traffic.yaml`
- `logs/`
- `metrics.json`
- `summary.md`
