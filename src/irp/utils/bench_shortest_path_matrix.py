from __future__ import annotations

import argparse
import csv
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any


def parse_csv_ints(raw: str) -> list[int]:
    out: list[int] = []
    for item in raw.split(","):
        item = item.strip()
        if not item:
            continue
        out.append(int(item))
    return out


def parse_csv_floats(raw: str) -> list[float]:
    out: list[float] = []
    for item in raw.split(","):
        item = item.strip()
        if not item:
            continue
        out.append(float(item))
    return out


def run_one(
    cwd: Path,
    nodes: int,
    density: float,
    seeds: int,
    start_seed: int,
    iterations: int,
    k_paths: int,
    pareto_max_paths: int,
) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="route_compute_bench_") as tmp_dir:
        out_path = Path(tmp_dir) / "result.json"
        cmd = [
            "cargo",
            "run",
            "--quiet",
            "--bin",
            "route_compute_bench",
            "--",
            "--nodes",
            str(nodes),
            "--density",
            str(density),
            "--seeds",
            str(seeds),
            "--start-seed",
            str(start_seed),
            "--iterations",
            str(iterations),
            "--k-paths",
            str(k_paths),
            "--pareto-max-paths",
            str(pareto_max_paths),
            "--output-json",
            str(out_path),
        ]
        subprocess.run(cmd, cwd=str(cwd), check=True)
        with out_path.open("r", encoding="utf-8") as f:
            return json.load(f)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Batch benchmark for route_compute strategies"
    )
    parser.add_argument(
        "--nodes-list",
        default="50,100,200",
        help="Comma-separated node counts, e.g. 50,100,200",
    )
    parser.add_argument(
        "--density-list",
        default="0.05,0.1",
        help="Comma-separated graph densities, e.g. 0.05,0.1",
    )
    parser.add_argument("--seeds", type=int, default=3)
    parser.add_argument("--start-seed", type=int, default=1)
    parser.add_argument("--iterations", type=int, default=8)
    parser.add_argument("--k-paths", type=int, default=3)
    parser.add_argument("--pareto-max-paths", type=int, default=8)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results/route_compute_bench"),
        help="Output directory for matrix JSON/CSV",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    nodes_list = parse_csv_ints(args.nodes_list)
    density_list = parse_csv_floats(args.density_list)

    results: list[dict[str, Any]] = []
    rows: list[dict[str, Any]] = []

    for nodes in nodes_list:
        for density in density_list:
            run = run_one(
                cwd=repo_root,
                nodes=nodes,
                density=density,
                seeds=args.seeds,
                start_seed=args.start_seed,
                iterations=args.iterations,
                k_paths=args.k_paths,
                pareto_max_paths=args.pareto_max_paths,
            )
            results.append(
                {
                    "nodes": nodes,
                    "density": density,
                    "aggregate": run.get("aggregate", []),
                }
            )
            for algo in run.get("aggregate", []):
                rows.append(
                    {
                        "nodes": nodes,
                        "density": density,
                        "algorithm": algo.get("algorithm"),
                        "runtime_ms": algo.get("runtime_ms"),
                        "reachable_ratio": algo.get("reachable_ratio"),
                        "mean_metric": algo.get("mean_metric"),
                        "p95_metric": algo.get("p95_metric"),
                        "mean_next_hops": algo.get("mean_next_hops"),
                    }
                )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    json_path = args.output_dir / "matrix.json"
    csv_path = args.output_dir / "matrix.csv"

    payload = {
        "config": {
            "nodes_list": nodes_list,
            "density_list": density_list,
            "seeds": args.seeds,
            "start_seed": args.start_seed,
            "iterations": args.iterations,
            "k_paths": args.k_paths,
            "pareto_max_paths": args.pareto_max_paths,
        },
        "results": results,
    }

    with json_path.open("w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=2, sort_keys=True)

    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "nodes",
                "density",
                "algorithm",
                "runtime_ms",
                "reachable_ratio",
                "mean_metric",
                "p95_metric",
                "mean_next_hops",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {json_path}")
    print(f"Wrote {csv_path}")


if __name__ == "__main__":
    main()
