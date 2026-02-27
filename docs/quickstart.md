# Quickstart

## Prerequisites

- Docker running
- `containerlab` in `PATH`
- Rust target: `x86_64-unknown-linux-musl`

Install dependencies:

```bash
make install
rustup target add x86_64-unknown-linux-musl
```

## Build Binaries

```bash
make build-routerd-rs
make build-traffic-app-go
```

## Run a Unified Scenario Experiment

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/line3_ospf_multi_apps.yaml \
  --poll-interval-s 1 \
  --sudo
```

## Run a Unified Convergence Benchmark

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/ring6_ospf_convergence_benchmark.yaml \
  --sudo
```

## Validate Outputs

```bash
python3 tools/validate_unified_metrics.py \
  --input results/runs/unified_experiments \
  --recursive
```
