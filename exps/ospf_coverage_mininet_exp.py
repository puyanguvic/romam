#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean
from typing import Any, Dict, List

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from rpf.core.topology import Topology
from rpf.utils.io import dump_json, ensure_dir, now_tag

MININET_IMPORT_ERROR: Exception | None = None
try:
    from mininet.clean import cleanup
    from mininet.link import TCLink
    from mininet.log import setLogLevel
    from mininet.net import Mininet
except Exception as exc:  # pragma: no cover - runtime environment dependent
    MININET_IMPORT_ERROR = exc


PING_SUMMARY_RE = re.compile(r"(\\d+) packets transmitted, (\\d+) (?:packets )?received")
PING_RTT_RE = re.compile(r"(?:rtt|round-trip) min/avg/max(?:/mdev|/stddev)? = ([0-9.]+)/([0-9.]+)/")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run Mininet link-delay (1ms) coverage experiment and export probe metrics."
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
    parser.add_argument(
        "--ping-count",
        type=int,
        default=3,
        help="ICMP packets per link probe. Default: 3.",
    )
    parser.add_argument(
        "--ping-timeout-s",
        type=int,
        default=1,
        help="Ping timeout in seconds. Default: 1.",
    )
    parser.add_argument(
        "--startup-wait-s",
        type=float,
        default=0.4,
        help="Wait time after net.start(). Default: 0.4.",
    )
    parser.add_argument(
        "--run-output-dir",
        default="results/runs/ospf_coverage_mininet",
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


def edge_ip_pair(edge_idx: int) -> tuple[str, str]:
    second_octet = edge_idx // 256
    third_octet = edge_idx % 256
    if second_octet > 255:
        raise ValueError("Too many edges for /30 IP allocation plan (max 65536 links).")
    subnet = f"10.{second_octet}.{third_octet}"
    return f"{subnet}.1", f"{subnet}.2"


def parse_ping_output(output: str) -> tuple[int, int, float | None]:
    tx = 0
    rx = 0
    avg_rtt = None

    summary_match = PING_SUMMARY_RE.search(output)
    if summary_match:
        tx = int(summary_match.group(1))
        rx = int(summary_match.group(2))

    rtt_match = PING_RTT_RE.search(output)
    if rtt_match:
        avg_rtt = float(rtt_match.group(2))

    return tx, rx, avg_rtt


def pctl(values: List[float], percentile: float) -> float | None:
    if not values:
        return None
    xs = sorted(values)
    idx = int(math.ceil(len(xs) * percentile)) - 1
    idx = max(0, min(idx, len(xs) - 1))
    return xs[idx]


def ensure_mininet_available() -> None:
    if MININET_IMPORT_ERROR is not None:
        raise RuntimeError(
            "Mininet is unavailable in this environment. "
            f"Import error: {MININET_IMPORT_ERROR!r}"
        )


def run_once(
    args: argparse.Namespace,
    topology_cfg: Dict[str, Any],
    run_idx: int,
) -> Dict[str, Any]:
    ensure_mininet_available()

    seed = int(args.seed) + run_idx
    topology = Topology.from_config(topology_cfg, seed=seed)
    edges = topology.edge_list()
    run_id = f"ospf_coverage_mininet_n{args.n_nodes}_r{run_idx}_{now_tag()}"

    setLogLevel("error")
    net = Mininet(topo=None, build=False, controller=None, autoSetMacs=True)
    hosts: Dict[int, Any] = {}
    probe_plan: List[Dict[str, Any]] = []

    for node in topology.nodes():
        hosts[node] = net.addHost(f"h{node}")

    for edge_idx, edge in enumerate(edges):
        ip_u, ip_v = edge_ip_pair(edge_idx)
        net.addLink(
            hosts[edge.u],
            hosts[edge.v],
            cls=TCLink,
            delay="1ms",
            intfName1=f"h{edge.u}-e{edge_idx}",
            intfName2=f"h{edge.v}-e{edge_idx}",
            params1={"ip": f"{ip_u}/30"},
            params2={"ip": f"{ip_v}/30"},
        )
        probe_plan.append(
            {
                "u": edge.u,
                "v": edge.v,
                "target_ip": ip_v,
            }
        )

    successful_probes = 0
    failed_probes = 0
    rtts: List[float] = []
    probe_details: List[Dict[str, Any]] = []

    try:
        net.build()
        net.start()
        time.sleep(max(0.0, float(args.startup_wait_s)))

        for probe in probe_plan:
            host = hosts[int(probe["u"])]
            target_ip = str(probe["target_ip"])
            output = host.cmd(
                f"ping -n -c {int(args.ping_count)} -W {int(args.ping_timeout_s)} {target_ip}"
            )
            tx, rx, avg_rtt = parse_ping_output(output)
            ok = tx > 0 and rx == tx
            if ok:
                successful_probes += 1
            else:
                failed_probes += 1
            if avg_rtt is not None:
                rtts.append(avg_rtt)

            probe_details.append(
                {
                    "u": probe["u"],
                    "v": probe["v"],
                    "target_ip": target_ip,
                    "packets_tx": tx,
                    "packets_rx": rx,
                    "avg_rtt_ms": avg_rtt,
                    "success": ok,
                }
            )
    finally:
        try:
            net.stop()
        finally:
            cleanup()

    run_payload = {
        "run_id": run_id,
        "name": f"ospf_coverage_mininet_n{args.n_nodes}_r{run_idx}",
        "seed": seed,
        "n_nodes": args.n_nodes,
        "topology": args.topology,
        "edge_count": len(edges),
        "link_delay_ms": 1,
        "successful_probes": successful_probes,
        "failed_probes": failed_probes,
        "probe_success_ratio": round(successful_probes / max(1, len(edges)), 6),
        "avg_rtt_ms": round(mean(rtts), 3) if rtts else None,
        "p95_rtt_ms": round(float(pctl(rtts, 0.95)), 3) if rtts else None,
        "probes": probe_details,
    }

    run_dir = ensure_dir(REPO_ROOT / args.run_output_dir)
    dump_json(run_dir / f"{run_id}.json", run_payload)

    return {k: v for k, v in run_payload.items() if k != "probes"}


def summarize(metrics_rows: List[Dict[str, Any]], args: argparse.Namespace) -> Dict[str, Any]:
    return {
        "experiment": "ospf_coverage_mininet_exp",
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "n_nodes": args.n_nodes,
        "repeats": args.repeats,
        "topology": args.topology,
        "link_delay_ms": 1,
        "avg_probe_success_ratio": round(
            mean([float(row["probe_success_ratio"]) for row in metrics_rows]),
            6,
        ),
        "avg_rtt_ms": round(
            mean(
                [
                    float(row["avg_rtt_ms"])
                    for row in metrics_rows
                    if row["avg_rtt_ms"] is not None
                ]
            ),
            3,
        )
        if any(row["avg_rtt_ms"] is not None for row in metrics_rows)
        else None,
        "avg_p95_rtt_ms": round(
            mean(
                [
                    float(row["p95_rtt_ms"])
                    for row in metrics_rows
                    if row["p95_rtt_ms"] is not None
                ]
            ),
            3,
        )
        if any(row["p95_rtt_ms"] is not None for row in metrics_rows)
        else None,
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
        "seed",
        "n_nodes",
        "topology",
        "edge_count",
        "link_delay_ms",
        "successful_probes",
        "failed_probes",
        "probe_success_ratio",
        "avg_rtt_ms",
        "p95_rtt_ms",
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

    rows = [run_once(args, topology_cfg, i) for i in range(args.repeats)]
    summary = summarize(rows, args)
    prefix = args.result_prefix or f"results/tables/ospf_coverage_mininet_n{args.n_nodes}"
    json_path, csv_path = save_outputs(summary, prefix)

    print("=== OSPF Coverage Mininet Experiment ===")
    print(f"n_nodes: {summary['n_nodes']}")
    print(f"repeats: {summary['repeats']}")
    print(f"topology: {summary['topology']}")
    print(f"link_delay_ms: {summary['link_delay_ms']}")
    print(f"avg_probe_success_ratio: {summary['avg_probe_success_ratio']}")
    print(f"avg_rtt_ms: {summary['avg_rtt_ms']}")
    print(f"avg_p95_rtt_ms: {summary['avg_p95_rtt_ms']}")
    print(f"json: {json_path}")
    print(f"csv: {csv_path}")


if __name__ == "__main__":
    main()
