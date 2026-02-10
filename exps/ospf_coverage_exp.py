#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import csv
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean
from typing import Any, Dict, List

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from rpf.backends.emu import EmuBackend
from rpf.eval.metrics import compute_metrics
from rpf.utils.io import ensure_dir


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run OSPF coverage experiment and export convergence metrics."
    )
    parser.add_argument("--n-nodes", type=int, default=50, help="Topology size. Default: 50.")
    parser.add_argument(
        "--repeats",
        type=int,
        default=5,
        help="Number of repeated runs with different seeds.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Base random seed. Repeat i uses seed+i.",
    )
    parser.add_argument(
        "--topology",
        choices=["ring", "er", "ba"],
        default="er",
        help="Topology type.",
    )
    parser.add_argument("--er-p", type=float, default=0.12, help="ER topology edge probability.")
    parser.add_argument("--ba-m", type=int, default=2, help="BA topology attachment degree.")
    parser.add_argument("--max-ticks", type=int, default=220, help="Simulation max ticks.")
    parser.add_argument(
        "--run-output-dir",
        default="results/runs/ospf_coverage",
        help="Directory for raw run artifacts.",
    )
    parser.add_argument(
        "--result-prefix",
        default="",
        help="Output prefix for aggregated result files.",
    )
    return parser.parse_args()


def build_topology_cfg(args: argparse.Namespace) -> Dict[str, Any]:
    cfg: Dict[str, Any] = {
        "type": args.topology,
        "n_nodes": int(args.n_nodes),
        "default_metric": 1.0,
    }
    if args.topology == "er":
        cfg["p"] = float(args.er_p)
    elif args.topology == "ba":
        cfg["m"] = int(args.ba_m)
    return cfg


def route_coverage_ratio(route_tables: Dict[int, Dict[int, List[int]]], n_nodes: int) -> float:
    expected = n_nodes * n_nodes
    if expected <= 0:
        return 0.0
    observed = sum(len(tbl) for tbl in route_tables.values())
    return observed / expected


def run_once(
    backend: EmuBackend,
    args: argparse.Namespace,
    run_idx: int,
    topology_cfg: Dict[str, Any],
) -> Dict[str, Any]:
    seed = int(args.seed) + run_idx
    cfg = {
        "name": f"ospf_coverage_n{args.n_nodes}_r{run_idx}",
        "seed": seed,
        "protocol": "ospf_like",
        "protocol_params": {
            "spf_interval": 3,
            "lsa_refresh": 20,
            "jitter": 0,
        },
        "topology": topology_cfg,
        "engine": {"max_ticks": int(args.max_ticks)},
        "network": {"base_delay": 1, "jitter": 0, "loss_prob": 0.0},
        "convergence_window": 5,
        "output_dir": str(REPO_ROOT / args.run_output_dir),
    }
    run = backend.run(cfg)
    metrics = compute_metrics(run)
    metrics["n_nodes"] = args.n_nodes
    metrics["max_ticks"] = args.max_ticks
    metrics["converged"] = run.get("converged_tick") is not None
    metrics["route_coverage_ratio"] = round(
        route_coverage_ratio(run.get("route_tables", {}), args.n_nodes),
        6,
    )
    if run.get("converged_tick") is not None:
        metrics["convergence_ratio"] = round(
            float(run["converged_tick"]) / float(args.max_ticks),
            6,
        )
    else:
        metrics["convergence_ratio"] = None
    return metrics


def summarize(metrics_rows: List[Dict[str, Any]], args: argparse.Namespace) -> Dict[str, Any]:
    converged = [r for r in metrics_rows if r["converged"]]
    converged_ticks = [
        float(r["converged_tick"]) for r in converged if r["converged_tick"] is not None
    ]
    return {
        "experiment": "ospf_coverage_exp",
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "n_nodes": args.n_nodes,
        "repeats": args.repeats,
        "topology": args.topology,
        "max_ticks": args.max_ticks,
        "converged_runs": len(converged),
        "convergence_rate": round(len(converged) / max(1, args.repeats), 6),
        "avg_converged_tick": round(mean(converged_ticks), 3) if converged_ticks else None,
        "avg_route_coverage_ratio": round(
            mean([r["route_coverage_ratio"] for r in metrics_rows]),
            6,
        ),
        "avg_route_flaps": round(mean([float(r["route_flaps"]) for r in metrics_rows]), 3),
        "avg_hash_changes": round(mean([float(r["hash_changes"]) for r in metrics_rows]), 3),
        "runs": metrics_rows,
    }


def save_outputs(summary: Dict[str, Any], prefix: str) -> tuple[Path, Path]:
    prefix_path = REPO_ROOT / prefix
    ensure_dir(prefix_path.parent)

    json_path = prefix_path.with_suffix(".json")
    with json_path.open("w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2, sort_keys=True)

    csv_path = prefix_path.with_suffix(".csv")
    fields = [
        "run_id",
        "name",
        "protocol",
        "seed",
        "n_nodes",
        "max_ticks",
        "converged",
        "converged_tick",
        "convergence_ratio",
        "route_coverage_ratio",
        "route_flaps",
        "hash_changes",
        "delivered_messages",
        "dropped_messages",
    ]
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in summary["runs"]:
            writer.writerow({k: row.get(k) for k in fields})

    return json_path, csv_path


def main() -> None:
    args = parse_args()
    topology_cfg = build_topology_cfg(args)
    backend = EmuBackend()

    rows = [run_once(backend, args, i, topology_cfg) for i in range(args.repeats)]
    summary = summarize(rows, args)
    prefix = args.result_prefix or f"results/tables/ospf_coverage_n{args.n_nodes}"
    json_path, csv_path = save_outputs(summary, prefix)

    print("=== OSPF Coverage Experiment ===")
    print(f"n_nodes: {summary['n_nodes']}")
    print(f"repeats: {summary['repeats']}")
    print(f"topology: {summary['topology']}")
    print(f"convergence_rate: {summary['convergence_rate']}")
    print(f"avg_converged_tick: {summary['avg_converged_tick']}")
    print(f"avg_route_coverage_ratio: {summary['avg_route_coverage_ratio']}")
    print(f"avg_route_flaps: {summary['avg_route_flaps']}")
    print(f"avg_hash_changes: {summary['avg_hash_changes']}")
    print(f"json: {json_path}")
    print(f"csv: {csv_path}")


if __name__ == "__main__":
    main()
