# Routing Protocol Framework

A lightweight, extensible routing protocol framework for emulation, protocol comparison, and reproducible evaluation.

## Features

- Tick-based simulation engine for deterministic experiments.
- Pluggable routing protocols (`ospf_like`, `rip_like`, `ecmp`).
- Config-driven experiments (topology, failures, metrics drift, sweeps).
- Structured run logs (`jsonl`) and summary tables.
- Optional Mininet integration stubs for realism validation.

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

## Project Layout

The project follows the structure described in your design with `core`, `protocols`, `backends`, `eval`, `cli`, `utils`, `scripts`, and `tests` modules.

## Output

By default, outputs are stored under `results/`:

- `results/runs/` raw run artifacts
- `results/tables/` summary tables
- `results/figs/` charts

## Notes

- This baseline implementation focuses on emulation backend and deterministic tick engine.
- Mininet backend is included as interfaces/stubs for further extension.
