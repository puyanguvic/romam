from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from rpf.eval.metrics import compute_metrics


def summarize_runs(runs_dir: str, out_csv: str) -> None:
    runs_path = Path(runs_dir)
    rows = []
    for result_file in sorted(runs_path.rglob("result.json")):
        with result_file.open("r", encoding="utf-8") as f:
            run = json.load(f)
        rows.append(compute_metrics(run))

    out_path = Path(out_csv)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "run_id",
        "name",
        "protocol",
        "seed",
        "converged_tick",
        "route_flaps",
        "delivered_messages",
        "dropped_messages",
        "hash_changes",
    ]
    with out_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Summarize run results into CSV")
    parser.add_argument("--runs", required=True, help="Directory containing run folders")
    parser.add_argument("--out", required=True, help="Output CSV path")
    args = parser.parse_args()
    summarize_runs(args.runs, args.out)
