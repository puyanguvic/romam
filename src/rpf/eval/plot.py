from __future__ import annotations

import argparse
import csv
from pathlib import Path


def plot_summary(input_csv: str, out_png: str) -> None:
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:  # pragma: no cover
        raise RuntimeError("matplotlib is required for plotting") from exc

    rows = []
    with Path(input_csv).open("r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    names = [r["run_id"] for r in rows]
    conv = [int(r["converged_tick"]) if r["converged_tick"] not in {"", "None"} else -1 for r in rows]

    plt.figure(figsize=(10, 4))
    plt.bar(range(len(names)), conv)
    plt.xticks(range(len(names)), names, rotation=75, fontsize=8)
    plt.ylabel("Converged Tick")
    plt.tight_layout()
    Path(out_png).parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_png, dpi=150)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot summary CSV")
    parser.add_argument("--in", dest="input_csv", required=True)
    parser.add_argument("--out", dest="out_png", required=True)
    args = parser.parse_args()
    plot_summary(args.input_csv, args.out_png)
