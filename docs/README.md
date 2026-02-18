# IRP Playground Docs

This folder is the canonical entry for running and evaluating experiments.

## Start Here

- Quickstart (build + run your first experiment): `docs/quickstart.md`
- Unified experiments (scenario + benchmark): `docs/unified-experiments.md`
- Results layout and validation: `docs/results-and-validation.md`

## Scope

- Keep experiment architecture unchanged (`tools/`, containerlab, Go traffic apps).
- Prefer config-driven runs via `tools/run_unified_experiment.py`.
- Keep outputs reproducible under `results/`.
