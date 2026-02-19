#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import json
import shlex
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.common import (
    load_json,
    load_management_ports_from_configs,
    load_topology_node_names,
    parse_bool_arg,
    parse_output_json,
    summarize_neighbor_fast_state_from_metrics_map,
    resolve_clab_bin,
    resolve_path,
    run_clab_command,
)

DEFAULT_TOPOLOGY_DATA = REPO_ROOT / "src" / "clab" / "topology-data.json"
DEFAULT_ENDPOINTS = "/v1/status,/v1/routes,/v1/fib,/v1/kernel-routes"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect routingd APIs and containerlab inspect output into one JSON report."
    )
    parser.add_argument(
        "--topology-data",
        default=str(DEFAULT_TOPOLOGY_DATA),
        help="Path to topology-data.json (default: src/clab/topology-data.json).",
    )
    parser.add_argument(
        "--out",
        "--output-json",
        dest="output_json",
        default="",
        help="Path to output JSON file (default: results/runs/clab/<lab>/collect-<utc>.json).",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=None,
        help="Use sudo for clab commands (default: from topology-data.json, fallback false).",
    )
    parser.add_argument(
        "--clab-bin",
        default="",
        help="Containerlab binary path/name (default: auto-detect clab/containerlab).",
    )
    parser.add_argument(
        "--endpoints",
        default=DEFAULT_ENDPOINTS,
        help="Comma-separated routingd API paths (example: /v1/status,/v1/routes).",
    )
    parser.add_argument(
        "--http-timeout-s",
        type=float,
        default=4.0,
        help="Timeout in seconds for each in-container API fetch.",
    )
    return parser.parse_args()


def load_ports(
    topology_data: dict[str, Any],
    topology_data_dir: Path,
    nodes: list[str],
) -> dict[str, int]:
    ports: dict[str, int] = {}
    raw = dict(topology_data.get("management_http_ports", {}) or {})
    for node, value in raw.items():
        try:
            port = int(value)
        except (TypeError, ValueError):
            continue
        if port > 0:
            ports[str(node)] = port

    config_dir_value = str(topology_data.get("config_dir", "")).strip()
    if config_dir_value:
        config_dir = resolve_path(config_dir_value, topology_data_dir)
        if config_dir.is_dir():
            ports.update(load_management_ports_from_configs(config_dir, nodes))
    return ports


def fetch_node_endpoint(
    *,
    clab_bin: str,
    use_sudo: bool,
    lab_name: str,
    node: str,
    port: int,
    endpoint: str,
    timeout_s: float,
) -> tuple[int, Any]:
    url = f"http://127.0.0.1:{port}{endpoint}"
    probe = (
        f"wget -qO- {shlex.quote(url)} 2>/dev/null "
        f"|| curl -fsS --max-time {max(float(timeout_s), 0.5):.2f} {shlex.quote(url)} 2>/dev/null "
        "|| true"
    )
    cmd = [
        clab_bin,
        "exec",
        "--name",
        lab_name,
        "--node",
        node,
        "--cmd",
        f"sh -lc {shlex.quote(probe)}",
    ]
    proc = run_clab_command(cmd, use_sudo=use_sudo, check=False, capture_output=True)
    return proc.returncode, parse_output_json(proc.stdout or "")


def main() -> int:
    args = parse_args()
    topology_data_path = Path(str(args.topology_data)).expanduser().resolve()
    if not topology_data_path.is_file():
        raise FileNotFoundError(f"topology-data json not found: {topology_data_path}")
    topology_data = load_json(topology_data_path)
    topology_data_dir = topology_data_path.parent

    topology_value = str(topology_data.get("topology_file", "")).strip()
    if not topology_value:
        raise ValueError("topology-data.json missing `topology_file`.")
    topology_file = resolve_path(topology_value, topology_data_dir)
    if not topology_file.is_file():
        raise FileNotFoundError(f"topology yaml not found: {topology_file}")
    lab_name = str(topology_data.get("lab_name", "")).strip()
    if not lab_name:
        raise ValueError("topology-data.json missing `lab_name`.")

    nodes = [str(node) for node in list(topology_data.get("nodes", []) or []) if str(node).strip()]
    if not nodes:
        nodes = load_topology_node_names(topology_file)
    ports = load_ports(topology_data, topology_data_dir, nodes)

    endpoints = [part.strip() for part in str(args.endpoints).split(",") if part.strip()]
    if not endpoints:
        raise ValueError("`--endpoints` resolved to empty list.")

    use_sudo = parse_bool_arg(args.sudo, bool(topology_data.get("sudo", False)))
    clab_bin = resolve_clab_bin(str(args.clab_bin).strip())

    inspect_proc = run_clab_command(
        [clab_bin, "inspect", "--name", lab_name, "--format", "json"],
        use_sudo=use_sudo,
        check=False,
        capture_output=True,
    )
    inspect_data = parse_output_json(inspect_proc.stdout or "")

    per_node: dict[str, Any] = {}
    for node in nodes:
        port = int(ports.get(node, 0))
        node_result: dict[str, Any] = {"port": port, "apis": {}, "errors": []}
        if port <= 0:
            node_result["errors"].append("missing management_http_port")
            per_node[node] = node_result
            continue
        for endpoint in endpoints:
            rc, payload = fetch_node_endpoint(
                clab_bin=clab_bin,
                use_sudo=use_sudo,
                lab_name=lab_name,
                node=node,
                port=port,
                endpoint=endpoint,
                timeout_s=float(args.http_timeout_s),
            )
            node_result["apis"][endpoint] = {"rc": rc, "payload": payload}
        per_node[node] = node_result

    ts = datetime.now(timezone.utc)
    if args.output_json:
        out_path = Path(str(args.output_json)).expanduser().resolve()
    else:
        out_path = (
            REPO_ROOT
            / "results"
            / "runs"
            / "clab"
            / lab_name
            / f"collect-{ts.strftime('%Y%m%dT%H%M%SZ')}.json"
        )
    out_path.parent.mkdir(parents=True, exist_ok=True)

    report = {
        "collected_at_utc": ts.isoformat(),
        "topology_data_path": str(topology_data_path),
        "topology_file": str(topology_file),
        "lab_name": lab_name,
        "nodes": nodes,
        "management_http_ports": ports,
        "inspect": {
            "rc": int(inspect_proc.returncode),
            "payload": inspect_data,
        },
        "routingd_api": per_node,
        "neighbor_fast_state_summary": summarize_neighbor_fast_state_from_metrics_map(
            {
                node: (
                    (node_payload.get("apis", {}) or {})
                    .get("/v1/metrics", {})
                    .get("payload", {})
                    if isinstance(node_payload, dict)
                    else {}
                )
                for node, node_payload in per_node.items()
            }
        ),
    }
    out_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"saved: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
