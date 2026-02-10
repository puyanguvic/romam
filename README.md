# Routing Protocol Framework

A lightweight, extensible routing protocol framework for emulation, protocol comparison, and reproducible evaluation.

## Features

- Tick-based simulation engine for deterministic experiments.
- Pluggable routing protocols (`ospf_like`, `rip_like`, `ecmp`).
- Config-driven experiments (topology, failures, metrics drift, sweeps).
- Structured run logs (`jsonl`) and summary tables.
- Mininet support via a standalone `1ms` link-delay experiment script.

## Quick Start

```bash
make install
make test
make run-emu CONFIG=configs/experiments/failure_recovery.yaml
```

or with CLI:

```bash
rpf run --config configs/experiments/failure_recovery.yaml
```

## Mininet Experiment (1ms Link Delay)

Run the standalone Mininet coverage experiment (all links use `1ms` delay):

```bash
make run-mininet-exp EXP_N_NODES=20 EXP_REPEATS=3 EXP_TOPOLOGY=er EXP_ER_P=0.2
```

or directly:

```bash
python3 exps/ospf_coverage_mininet_exp.py --n-nodes 20 --repeats 3 --topology er --er-p 0.2
```

Outputs:

- Run artifacts: `results/runs/ospf_coverage_mininet/`
- Aggregated tables: `results/tables/ospf_coverage_mininet_n*.json` and `.csv`

## Project Layout

The project follows the structure described in your design with `core`, `protocols`, `backends`, `eval`, `cli`, `utils`, `scripts`, and `tests` modules.

## Output

By default, outputs are stored under `results/`:

- `results/runs/` raw run artifacts
- `results/tables/` summary tables
- `results/figs/` charts

## Notes

- This baseline implementation focuses on emulation backend and deterministic tick engine.
- CLI Mininet backend remains interface/stub based.
- A standalone Mininet experiment script is available at `exps/ospf_coverage_mininet_exp.py`.
