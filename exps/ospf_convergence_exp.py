#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compatibility wrapper for OSPF convergence experiment. "
            "The benchmark core is implemented in run_unified_experiment.py."
        )
    )
    parser.add_argument("--topology-file", default="clab_topologies/ring6.clab.yaml")
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--ping-count", type=int, default=3)
    parser.add_argument("--ping-timeout-s", type=int, default=1)
    parser.add_argument("--startup-wait-s", type=float, default=0.8)
    parser.add_argument("--link-delay-ms", type=float, default=1.0)
    parser.add_argument(
        "--node-image",
        default="ghcr.io/srl-labs/network-multitool:latest",
    )
    parser.add_argument("--lab-name-prefix", default="ospf-convergence-clab")
    parser.add_argument("--mgmt-network-name", default="")
    parser.add_argument("--mgmt-ipv4-subnet", default="")
    parser.add_argument("--mgmt-ipv6-subnet", default="")
    parser.add_argument("--mgmt-external-access", action="store_true")
    parser.add_argument("--sudo", action="store_true")
    parser.add_argument("--keep-lab", action="store_true")
    parser.add_argument(
        "--run-output-dir",
        default="results/runs/ospf_convergence_containerlab",
    )
    parser.add_argument("--result-prefix", default="")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    config: dict[str, object] = {
        "mode": "convergence_benchmark",
        "topology": str(args.topology_file),
        "protocol": "ospf",
        "lab": {
            "node_image": str(args.node_image),
            "lab_name_prefix": str(args.lab_name_prefix),
            "mgmt_network_name": str(args.mgmt_network_name),
            "mgmt_ipv4_subnet": str(args.mgmt_ipv4_subnet),
            "mgmt_ipv6_subnet": str(args.mgmt_ipv6_subnet),
            "mgmt_external_access": bool(args.mgmt_external_access),
        },
        "benchmark": {
            "repeats": int(args.repeats),
            "ping_count": int(args.ping_count),
            "ping_timeout_s": int(args.ping_timeout_s),
            "startup_wait_s": float(args.startup_wait_s),
            "link_delay_ms": float(args.link_delay_ms),
            "run_output_dir": str(args.run_output_dir),
        },
    }
    if str(args.result_prefix).strip():
        benchmark_cfg = dict(config["benchmark"])
        benchmark_cfg["result_prefix"] = str(args.result_prefix).strip()
        config["benchmark"] = benchmark_cfg

    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".yaml",
        prefix="ospf_convergence_",
        delete=False,
        encoding="utf-8",
    ) as tmp:
        yaml.safe_dump(config, tmp, sort_keys=False)
        cfg_path = Path(tmp.name)

    try:
        cmd = [
            sys.executable,
            str(REPO_ROOT / "exps" / "run_unified_experiment.py"),
            "--config",
            str(cfg_path),
            "--sudo" if bool(args.sudo) else "--no-sudo",
        ]
        if bool(args.keep_lab):
            cmd.append("--keep-lab")
        proc = subprocess.run(cmd, check=False)
        return int(proc.returncode)
    finally:
        cfg_path.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
