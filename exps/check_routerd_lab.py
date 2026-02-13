#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import ast
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

import yaml

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from irp.utils.io import dump_json

RIB_LOG_RE = re.compile(r"RIB/FIB updated:\s*(\[.*\])")
START_LOG_RE = re.compile(r"routerd start: .*protocol=(\w+)")


@dataclass(frozen=True)
class NeighborSpec:
    router_id: int
    address: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate generated routerd+containerlab experiment status."
    )
    parser.add_argument(
        "--topology-file",
        required=True,
        help="Path to generated .clab.yaml file.",
    )
    parser.add_argument("--sudo", action="store_true", help="Prefix docker commands with sudo.")
    parser.add_argument(
        "--tail-lines",
        type=int,
        default=60,
        help="Number of /tmp/routerd.log lines to inspect per node.",
    )
    parser.add_argument(
        "--expect-protocol",
        choices=["ospf", "rip"],
        default="",
        help="If set, fail when daemon protocol differs.",
    )
    parser.add_argument(
        "--min-routes",
        type=int,
        default=-1,
        help=(
            "Minimum expected route count in the latest RIB/FIB update. "
            "-1 means n_nodes-1."
        ),
    )
    parser.add_argument(
        "--output-json",
        default="",
        help="Optional path to dump full validation report as JSON.",
    )
    parser.add_argument(
        "--max-wait-s",
        type=float,
        default=10.0,
        help="Max wait seconds for convergence evidence (RIB logs).",
    )
    parser.add_argument(
        "--poll-interval-s",
        type=float,
        default=1.0,
        help="Polling interval during wait window.",
    )
    return parser.parse_args()


def with_sudo(cmd: List[str], use_sudo: bool) -> List[str]:
    if use_sudo:
        return ["sudo", *cmd]
    return cmd


def run_cmd(cmd: List[str], check: bool = True) -> str:
    try:
        result = subprocess.run(
            cmd,
            check=check,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except subprocess.CalledProcessError as exc:
        output = (exc.stdout or "").strip()
        raise RuntimeError(f"Command failed ({exc.returncode}): {' '.join(cmd)}\n{output}") from exc
    return (result.stdout or "").strip()


def load_topology(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    if not data or "name" not in data:
        raise ValueError(f"Invalid topology file: {path}")
    return data


def clab_container_name(lab_name: str, node_name: str) -> str:
    return f"clab-{lab_name}-{node_name}"


def parse_config_bind(node: Dict[str, Any], topology_file: Path) -> Path:
    topology_dir = topology_file.parent
    for bind in node.get("binds", []):
        bind_text = str(bind)
        host_path = ""
        if ":/irp/configs:ro" in bind_text:
            host_path = bind_text.split(":/irp/configs:ro", maxsplit=1)[0]
        elif ":/irp/configs" in bind_text:
            host_path = bind_text.split(":/irp/configs", maxsplit=1)[0]
        if host_path:
            candidate = Path(host_path).expanduser()
            if candidate.is_absolute():
                return candidate
            # Keep the same base-path semantics as containerlab bind parsing.
            return (topology_dir / candidate).resolve()
    raise ValueError("node bind does not include /irp/configs:ro mapping")


def load_neighbors(config_dir: Path, node_name: str) -> List[NeighborSpec]:
    cfg_path = config_dir / f"{node_name}.yaml"
    with cfg_path.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f) or {}
    return [
        NeighborSpec(router_id=int(item["router_id"]), address=str(item["address"]))
        for item in cfg.get("neighbors", [])
    ]


def inspect_node(
    use_sudo: bool,
    container: str,
    tail_lines: int,
    neighbors: List[NeighborSpec],
    expect_protocol: str,
    min_routes: int,
) -> Dict[str, Any]:
    report: Dict[str, Any] = {
        "container": container,
        "running": False,
        "daemon_running": False,
        "protocol": "",
        "route_count": None,
        "neighbor_ping": [],
        "errors": [],
    }

    state = run_cmd(
        with_sudo(["docker", "inspect", "-f", "{{.State.Running}}", container], use_sudo),
        check=False,
    )
    report["running"] = state.strip().lower() == "true"
    if not report["running"]:
        report["errors"].append("container not running")
        return report

    pgrep = run_cmd(
        with_sudo(
            ["docker", "exec", container, "sh", "-lc", "pgrep -af irp.routerd || true"],
            use_sudo,
        ),
        check=False,
    )
    report["daemon_running"] = bool(pgrep.strip())
    if not report["daemon_running"]:
        report["errors"].append("routerd process not found")

    log_tail = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                f"tail -n {max(1, tail_lines)} /tmp/routerd.log || true",
            ],
            use_sudo,
        ),
        check=False,
    )
    protocol = parse_protocol_from_log(log_tail)
    report["protocol"] = protocol
    if expect_protocol and protocol and protocol != expect_protocol:
        report["errors"].append(f"protocol mismatch: got={protocol} expect={expect_protocol}")

    route_count = parse_last_route_count(log_tail)
    report["route_count"] = route_count
    if route_count is None:
        report["errors"].append("no RIB/FIB update log found")
    elif route_count < min_routes:
        report["errors"].append(f"route_count too small: {route_count} < {min_routes}")

    ping_results = []
    for neighbor in neighbors:
        ping_output = run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    container,
                    "ping",
                    "-n",
                    "-c",
                    "1",
                    "-W",
                    "1",
                    neighbor.address,
                ],
                use_sudo,
            ),
            check=False,
        )
        ok = "1 packets transmitted, 1" in ping_output and "received" in ping_output
        ping_results.append(
            {"router_id": neighbor.router_id, "address": neighbor.address, "ok": ok}
        )
    report["neighbor_ping"] = ping_results
    if not all(item["ok"] for item in ping_results):
        report["errors"].append("neighbor ping failed on one or more links")

    return report


def parse_protocol_from_log(log_text: str) -> str:
    for line in reversed(log_text.splitlines()):
        match = START_LOG_RE.search(line)
        if match:
            return str(match.group(1))
    return ""


def parse_last_route_count(log_text: str) -> int | None:
    for line in reversed(log_text.splitlines()):
        match = RIB_LOG_RE.search(line)
        if not match:
            continue
        try:
            payload = ast.literal_eval(match.group(1))
            if isinstance(payload, list):
                return len(payload)
        except (ValueError, SyntaxError):
            return None
    return None


def main() -> int:
    args = parse_args()
    topology_file = Path(args.topology_file).expanduser().resolve()
    topo = load_topology(topology_file)

    lab_name = str(topo["name"])
    nodes = dict(topo.get("topology", {}).get("nodes", {}))
    if not nodes:
        raise RuntimeError("No nodes found in topology.")
    min_routes = int(args.min_routes)
    if min_routes < 0:
        min_routes = max(0, len(nodes) - 1)

    first_node = next(iter(nodes.values()))
    config_dir = parse_config_bind(first_node, topology_file)

    def collect_reports() -> Dict[str, Any]:
        reports: Dict[str, Any] = {}
        for node_name in sorted(nodes.keys()):
            container = clab_container_name(lab_name, node_name)
            neighbors = load_neighbors(config_dir, node_name)
            reports[node_name] = inspect_node(
                use_sudo=bool(args.sudo),
                container=container,
                tail_lines=int(args.tail_lines),
                neighbors=neighbors,
                expect_protocol=str(args.expect_protocol),
                min_routes=min_routes,
            )
        return reports

    deadline = time.monotonic() + max(0.0, float(args.max_wait_s))
    node_reports = collect_reports()
    all_errors = flatten_errors(node_reports)
    while all_errors and time.monotonic() < deadline:
        # Most common transient error is missing early convergence logs.
        if not all("no RIB/FIB update log found" in item for item in all_errors):
            break
        time.sleep(max(0.2, float(args.poll_interval_s)))
        node_reports = collect_reports()
        all_errors = flatten_errors(node_reports)

    summary = {
        "lab_name": lab_name,
        "topology_file": str(topology_file),
        "n_nodes": len(nodes),
        "min_routes_required": min_routes,
        "passed": len(all_errors) == 0,
        "errors": all_errors,
        "nodes": node_reports,
    }

    print("=== Routerd Lab Check ===")
    print(f"lab_name: {summary['lab_name']}")
    print(f"n_nodes: {summary['n_nodes']}")
    print(f"min_routes_required: {summary['min_routes_required']}")
    print(f"passed: {summary['passed']}")
    if all_errors:
        print("errors:")
        for item in all_errors:
            print(f"- {item}")
    else:
        print("errors: none")

    if args.output_json:
        dump_json(Path(args.output_json), summary)
        print(f"report_json: {args.output_json}")

    return 0 if summary["passed"] else 2


def flatten_errors(node_reports: Dict[str, Any]) -> List[str]:
    out: List[str] = []
    for node_name, report in node_reports.items():
        for error in report["errors"]:
            out.append(f"{node_name}: {error}")
    return out


if __name__ == "__main__":
    raise SystemExit(main())
