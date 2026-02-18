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

