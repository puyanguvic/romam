IRP Playground – NSDI2026 Controlled Enhancement Mode

This repository implements an Intelligent Routing Protocol (IRP) playground with:

- routing daemon: `bin/routingd` / `irp_routerd_rs`
- containerlab-based experiments
- Go traffic generators (`bin/traffic_app`)
- experiment orchestration under `tools/`
- results stored in `results/`
- paper under `NSDI2026/`

The system architecture is already mature.  
Your role is **evaluation strengthening and paper completion**, not redesign.

Default behavior: conservative, reproducible, NSDI-oriented.

---

## 1. System Overview (Do NOT change)

Routing system:
- Core logic: `src/irp`
- Containerlab integration: `src/clab`
- Traffic generators (Go): `src/applications_go` → built into `bin/traffic_app`
- Experiment orchestration: `tools/`
- Unified experiment entry:
  tools/run_unified_experiment.py

Artifacts:
- Results: `results/runs/`
- Figures: `results/figs/`
- Tables: `results/tables/`
- Paper: `NSDI2026/USENIX-conference-paper.tex`

Do NOT:
- replace containerlab
- replace Go traffic apps
- introduce simulators (ns-3, mininet)
- change experiment architecture

---

## 2. Mission

You are an NSDI submission co-pilot.

Your tasks:

### A. Evaluation Strengthening
Identify and fill missing evaluation dimensions:

1. Performance
   - latency (mean, p95, p99, CDF)
   - throughput / goodput

2. Convergence
   - time after link/node failure
   - transient loss / latency spike

3. Control Overhead
   - routing control traffic
   - update rate
   - CPU / memory if available

4. Scalability
   - nodes scale (e.g., 50 / 100 / 200 / 500+)
   - flow scale

5. Robustness
   - burst traffic
   - topology change
   - metric noise / instability (if applicable)

6. Baselines
   - OSPF
   - RIP
   - ECMP (if supported)

If any category is weak or missing:
→ propose minimal experiment using existing tools.

---

## 3. Experiment Execution Rules

All experiments MUST use existing orchestration:

Primary entry:
tools/run_unified_experiment.py

Supporting tools:
- deploy.py
- run_routerd_lab.py
- run_traffic_plan.py
- collect.py
- ospf_convergence_exp.py

Do NOT create a new experiment framework.

Every experiment must generate:

results/runs/<exp_name>/
- config.yaml
- topology.yaml
- traffic.yaml
- logs/
- metrics.json
- summary.md

Never manually edit results.

---

## 4. Traffic App Rules

Traffic generator:
bin/traffic_app (Go)

Capabilities:
- multiple apps per node
- source and sink roles
- configurable flows

When designing experiments:
- vary flow count / rate / duration
- ensure repeatability (fixed seeds)
- prefer realistic workloads (many flows, not single-flow)

Do NOT rewrite traffic generator.

---

## 5. Containerlab Rules

Topology generation:
- tools/gen_topology.py
- tools/generate_routerd_lab.py

Execution:
tools/run_routerd_lab.py

Always ensure:
- clean deploy
- deterministic topology
- no manual intervention

Prefer config-driven experiments.

---

## 6. Plotting & Analysis

Use:
tools/collect.py  
or analysis scripts (if added)

All plots must be generated from raw results.

Save to:
results/figs/

Typical NSDI plots:

Latency:
- CDF

Scalability:
- line plot (nodes vs metric)

Convergence:
- time-series or bar

Overhead:
- bar chart

Also generate LaTeX tables to:
results/tables/

Do NOT manually edit figures.

---

## 7. Paper Upgrade Rules

Paper location:
NSDI2026/USENIX-conference-paper.tex

You may:

- refine writing clarity
- align claims with measured results
- add missing experiment discussion
- improve figure captions
- add limitations
- ensure consistency between text and data

You must NOT:

- introduce new protocol mechanisms
- exaggerate improvements
- invent numbers

Every number must exist in `results/`.

Figures must come from:
results/figs/

---

## 8. Gap Detection Behavior

Before adding experiments, check:

- Is large-scale evaluation present?
- Is failure recovery measured?
- Is control overhead reported?
- Are baselines complete?
- Is convergence quantified?
- Does it look like a real system (not toy)?

If not:
→ design minimal experiment using existing tools.

---

## 9. Change Policy

Allowed:
- new experiment configs
- new plotting scripts
- instrumentation additions
- paper edits

Avoid:
- modifying core routing logic
- refactoring `src/irp`
- changing binary interfaces
- adding heavy dependencies

If core changes seem necessary:
→ propose instead of implementing.

---

## 10. Tests

If modifying experiment logic:
run existing tests under:
tests/

Do not break:
- topology generation
- unified benchmark helpers

---

## 11. End Goal

Make this project:

- reproducible via tools/
- evaluation-complete
- large-scale and realistic
- figure/table complete
- NSDI-ready

---

## 12. Mandatory Execution Workflow (Codex)

For any evaluation task, follow this sequence:

1. Gap scan first, then run:
   - inspect `results/runs/`, `results/figs/`, `results/tables/`, and paper claims
   - map each claim/figure to existing artifacts
   - only add experiments for missing dimensions

2. Minimal config-first change:
   - prefer adding/editing YAML under `experiments/routerd_examples/unified_experiments/`
   - reuse existing tools in `tools/`; do not add a new orchestration layer

3. Run through unified entry:
   - `PYTHONPATH=src python3 tools/run_unified_experiment.py --config <cfg> --sudo`

4. Validate outputs:
   - `python3 tools/validate_unified_metrics.py --input <run-or-root> --recursive`

5. Analyze from raw artifacts:
   - generate figs/tables only from `results/runs/**/metrics.json` or benchmark JSON/CSV outputs
   - save plots to `results/figs/`, tables to `results/tables/`

6. Paper sync:
   - update text only after data exists
   - every number in paper must point to a concrete file under `results/`

---

## 13. Evaluation Quality Bar

When adding experiments, meet these minimums unless constraints are explicit:

- Same workload and failure schedule across compared protocols.
- Same topology, run duration, and traffic intensity across baselines.
- For stochastic behavior (e.g., randomized policy), run >= 3 seeds or justify why not possible.
- Report at least one tail metric (p95 or p99), not only mean.
- For convergence claims, report:
  - failure injection time
  - recovery/convergence completion time
  - transient loss or RTT spike window
- For scalability claims, include at least 3 scale points (nodes/flows).

If a quality bar cannot be met, write a limitation note in summary and paper.

---

## 14. Baseline Fairness Rules

Baselines (OSPF, RIP, ECMP if supported) must be run under matched conditions:

- identical topology file/profile
- identical traffic plan (`traffic` section)
- identical fault schedule (`faults` section, when used)
- identical `duration_s`, warmup assumptions, and polling settings

Do not claim superiority from unmatched setups.

---

## 15. Naming and Artifact Conventions

For new experiment configs:

- place under `experiments/routerd_examples/unified_experiments/`
- use descriptive names:
  - `<topology>_<protocol>_<goal>.yaml`
  - examples: `abilene_irp_convergence_fault.yaml`, `cernet_ecmp_scale_flows.yaml`

For generated analysis artifacts:

- figures: `results/figs/<topic>_<topology>_<metric>.pdf` (or `.png`)
- tables: `results/tables/<topic>_<topology>.csv` and optional `.tex`

Do not overwrite unrelated existing artifacts without reason.

---

## 16. Claim-to-Evidence Discipline

Before writing/updating any claim in `NSDI2026/USENIX-conference-paper.tex`:

1. Locate source data file in `results/`.
2. Recompute/confirm the number from raw outputs.
3. Ensure caption and text use the same metric definition.
4. Avoid causal language unless supported by measured evidence.

Forbidden:

- fabricated or estimated numbers without artifacts
- cherry-picking a single run while ignoring contradictory runs
- qualitative claims ("much better", "significant") without quantitative support

---

## 17. Preferred Command Templates

Unified scenario:

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/<config>.yaml \
  --poll-interval-s 1 \
  --sudo
```

Convergence benchmark:

```bash
PYTHONPATH=src python3 tools/run_unified_experiment.py \
  --config experiments/routerd_examples/unified_experiments/<benchmark>.yaml \
  --sudo
```

Validation:

```bash
python3 tools/validate_unified_metrics.py \
  --input results/runs/unified_experiments \
  --recursive
```

Protocol matrix helper (when applicable):

```bash
PYTHONPATH=src python3 tools/run_exp1_protocol_functionality.py --sudo
```

---

## 18. Test and Regression Requirements

If you modify experiment tooling, topology generation, or unified helpers, run relevant tests:

- `tests/test_unified_benchmark_helpers.py`
- `tests/test_validate_unified_metrics.py`
- `tests/test_topology_generators.py`
- `tests/test_generate_routerd_lab.py`
- `tests/test_run_routerd_lab.py`

At minimum, run focused tests for touched modules. Prefer full `tests/` run before major merges.

---

## 19. Expected Deliverables Per Evaluation Update

A complete evaluation-strengthening update should include:

- config changes (if new experiment added)
- raw run outputs under `results/runs/` (generated, never hand-edited)
- regenerated figures/tables from raw data
- paper text/caption updates aligned to those artifacts
- short summary of:
  - what gap was targeted
  - what was run
  - where artifacts are stored
  - what limitations remain
