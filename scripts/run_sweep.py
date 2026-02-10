#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

from rpf.backends.emu import EmuBackend
from rpf.cli.run_emu import load_effective_config
from rpf.utils.io import load_yaml


def main() -> None:
    parser = argparse.ArgumentParser(description="Run parameter sweep")
    parser.add_argument("--config", required=True)
    args = parser.parse_args()

    sweep_cfg = load_yaml(args.config)
    base_cfg = load_effective_config(args.config)

    protocols = sweep_cfg.get("protocols", [base_cfg.get("protocol", "ospf_like")])
    sizes = sweep_cfg.get("sizes", [base_cfg.get("topology", {}).get("n_nodes", 20)])
    repeats = int(sweep_cfg.get("repeats", 1))

    backend = EmuBackend()
    outputs = []

    for protocol in protocols:
        for n in sizes:
            for rep in range(repeats):
                cfg = dict(base_cfg)
                cfg["name"] = f"sweep_{protocol}_n{n}_r{rep}"
                cfg["protocol"] = protocol
                topo = dict(cfg.get("topology", {}))
                topo.setdefault("type", "er")
                topo["n_nodes"] = int(n)
                cfg["topology"] = topo
                cfg["seed"] = int(base_cfg.get("seed", 1)) + rep
                out = backend.run(cfg)
                outputs.append(
                    {
                        "run_id": out["run_id"],
                        "protocol": protocol,
                        "n_nodes": n,
                        "repeat": rep,
                        "converged_tick": out["converged_tick"],
                    }
                )

    out_path = Path("results/tables/sweep_summary.json")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(outputs, f, indent=2, ensure_ascii=False)


if __name__ == "__main__":
    main()
