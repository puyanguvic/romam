#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import ast
import json
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

import yaml

REPO_ROOT = Path(__file__).resolve().parents[3]
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
        help="Path to source .clab.yaml file.",
    )
    parser.add_argument(
        "--lab-name",
        default="",
        help="Containerlab lab name. Defaults to topology `name` when omitted.",
    )
    parser.add_argument(
        "--config-dir",
        default="",
        help="Directory of per-node routerd configs. Auto-detected from topology binds if omitted.",
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
        choices=["ospf", "rip", "irp"],
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


def load_management_http_port(config_dir: Path, node_name: str) -> int:
    cfg_path = config_dir / f"{node_name}.yaml"
    with cfg_path.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f) or {}
    try:
        return int(cfg.get("management", {}).get("http", {}).get("port", 0))
    except (TypeError, ValueError):
        return 0


def inspect_node(
    use_sudo: bool,
    container: str,
    tail_lines: int,
    neighbors: List[NeighborSpec],
    management_http_port: int,
    expect_protocol: str,
    min_routes: int,
) -> Dict[str, Any]:
    report: Dict[str, Any] = {
        "container": container,
        "running": False,
        "daemon_running": False,
        "node_supervisor_running": False,
        "protocol": "",
        "management_api_ok": False,
        "management_route_count": None,
        "kernel_route_count": None,
        "route_count": None,
        "neighbor_ping": [],
        "routingd_state": {},
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

    report["daemon_running"] = _check_daemon_running(use_sudo, container)
    if not report["daemon_running"]:
        report["errors"].append("routerd process not found")
    report["node_supervisor_running"] = _check_node_supervisor_running(use_sudo, container)
    if not report["node_supervisor_running"]:
        report["errors"].append("node_supervisor process not found")
    supervisor_state_text = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                "cat /tmp/node_supervisor_state.json 2>/dev/null || true",
            ],
            use_sudo,
        ),
        check=False,
    )
    _attach_supervisor_state(report, supervisor_state_text)

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

    if management_http_port > 0:
        status_text = run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    container,
                    "sh",
                    "-lc",
                    (
                        f"wget -qO- http://127.0.0.1:{management_http_port}/v1/status 2>/dev/null "
                        f"|| curl -fsS http://127.0.0.1:{management_http_port}/v1/status "
                        "2>/dev/null "
                        "|| true"
                    ),
                ],
                use_sudo,
            ),
            check=False,
        )
        if status_text.strip():
            try:
                status_obj = json.loads(status_text)
                report["management_api_ok"] = True
                routes = list(status_obj.get("routes", []))
                report["management_route_count"] = len(routes)
            except json.JSONDecodeError:
                report["errors"].append("management status API returned non-JSON")
        else:
            if _has_http_probe_tool(use_sudo, container):
                report["errors"].append("management status API unreachable")
            else:
                report["errors"].append(
                    "management status probe unavailable: neither wget nor curl found"
                )

        kernel_text = run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    container,
                    "sh",
                    "-lc",
                    (
                        f"wget -qO- http://127.0.0.1:{management_http_port}/v1/kernel-routes "
                        "2>/dev/null "
                        f"|| curl -fsS http://127.0.0.1:{management_http_port}/v1/kernel-routes "
                        "2>/dev/null "
                        "|| true"
                    ),
                ],
                use_sudo,
            ),
            check=False,
        )
        if kernel_text.strip():
            try:
                kernel_obj = json.loads(kernel_text)
                report["kernel_route_count"] = len(list(kernel_obj.get("routes", [])))
            except json.JSONDecodeError:
                report["errors"].append("management kernel-routes API returned non-JSON")

    route_count = parse_last_route_count(log_tail)
    if report["management_route_count"] is not None:
        route_count = int(report["management_route_count"])
    report["route_count"] = route_count
    if route_count is None:
        report["errors"].append("no RIB/FIB update log found")
        lowered = log_tail.lower()
        if "traceback" in lowered or "module not found" in lowered:
            report["errors"].append("routerd log shows exception")
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

    if report["errors"]:
        supervisor_tail = run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    container,
                    "sh",
                    "-lc",
                    "tail -n 30 /tmp/node_supervisor.log 2>/dev/null || true",
                ],
                use_sudo,
            ),
            check=False,
        )
        if supervisor_tail.strip():
            report["errors"].append(
                f"node_supervisor.log last: {_last_nonempty_line(supervisor_tail)}"
            )
        routerd_tail = run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    container,
                    "sh",
                    "-lc",
                    "tail -n 30 /tmp/routerd.log 2>/dev/null || true",
                ],
                use_sudo,
            ),
            check=False,
        )
        if routerd_tail.strip():
            report["errors"].append(f"routerd.log last: {_last_nonempty_line(routerd_tail)}")

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


def _attach_supervisor_state(report: Dict[str, Any], state_text: str) -> None:
    payload = state_text.strip()
    if not payload:
        return
    try:
        state_obj = json.loads(payload)
    except json.JSONDecodeError:
        report["errors"].append("node_supervisor_state.json is invalid JSON")
        return
    if not isinstance(state_obj, dict):
        report["errors"].append("node_supervisor_state.json is not an object")
        return

    processes = state_obj.get("processes", [])
    if not isinstance(processes, list):
        report["errors"].append("node_supervisor_state.json missing processes list")
        return

    routingd = None
    for item in processes:
        if not isinstance(item, dict):
            continue
        name = str(item.get("name", "")).strip().lower()
        kind = str(item.get("kind", "")).strip().lower()
        if name == "routingd" or kind == "routingd":
            routingd = item
            break
    if routingd is None:
        report["errors"].append("routingd entry missing in node_supervisor_state")
        return

    state = {
        "running": bool(routingd.get("running", False)),
        "pid": routingd.get("pid"),
        "restart_count": routingd.get("restart_count"),
        "launch_count": routingd.get("launch_count"),
        "last_exit_code": routingd.get("last_exit_code"),
        "last_error": str(routingd.get("last_error", "")).strip(),
        "pending_restart": bool(routingd.get("pending_restart", False)),
    }
    report["routingd_state"] = state
    if not state["running"]:
        report["errors"].append(
            "routingd not running in supervisor state: "
            f"exit={state['last_exit_code']} error={state['last_error'] or 'n/a'}"
        )
    elif state["last_error"]:
        report["errors"].append(f"routingd supervisor last_error: {state['last_error']}")


def _has_http_probe_tool(use_sudo: bool, container: str) -> bool:
    probe = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                "(command -v wget >/dev/null 2>&1 || command -v curl >/dev/null 2>&1) "
                "&& echo yes || echo no",
            ],
            use_sudo,
        ),
        check=False,
    ).strip()
    return probe.lower() == "yes"


def _last_nonempty_line(text: str) -> str:
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if not lines:
        return ""
    tail = lines[-1]
    if len(tail) > 240:
        return f"{tail[:237]}..."
    return tail


def _check_daemon_running(use_sudo: bool, container: str) -> bool:
    pid_text = run_cmd(
        with_sudo(
            ["docker", "exec", container, "sh", "-lc", "cat /tmp/routerd.pid 2>/dev/null || true"],
            use_sudo,
        ),
        check=False,
    ).strip()
    if pid_text.isdigit():
        cmdline = run_cmd(
            with_sudo(
                ["docker", "exec", container, "sh", "-lc", f"ps -p {pid_text} -o args= || true"],
                use_sudo,
            ),
            check=False,
        ).strip()
        if (
            "/irp/bin/irp_routerd_rs" in cmdline
            or "irp_routerd_rs --config" in cmdline
            or "/irp/bin/routingd" in cmdline
            or "routingd --config" in cmdline
        ):
            return True

    pgrep_out = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                "pgrep -af 'routingd|irp_routerd_rs' || true",
            ],
            use_sudo,
        ),
        check=False,
    ).strip()
    return bool(pgrep_out)


def _check_node_supervisor_running(use_sudo: bool, container: str) -> bool:
    pid_text = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                "cat /tmp/node_supervisor.pid 2>/dev/null || true",
            ],
            use_sudo,
        ),
        check=False,
    ).strip()
    if pid_text.isdigit():
        cmdline = run_cmd(
            with_sudo(
                ["docker", "exec", container, "sh", "-lc", f"ps -p {pid_text} -o args= || true"],
                use_sudo,
            ),
            check=False,
        ).strip()
        if "/irp/bin/node_supervisor" in cmdline or "node_supervisor --config" in cmdline:
            return True

    pgrep_out = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                "pgrep -af 'node_supervisor' || true",
            ],
            use_sudo,
        ),
        check=False,
    ).strip()
    return bool(pgrep_out)


def main() -> int:
    args = parse_args()
    topology_file = Path(args.topology_file).expanduser().resolve()
    topo = load_topology(topology_file)

    lab_name = str(args.lab_name or topo["name"])
    nodes = dict(topo.get("topology", {}).get("nodes", {}))
    if not nodes:
        raise RuntimeError("No nodes found in topology.")
    min_routes = int(args.min_routes)
    if min_routes < 0:
        min_routes = max(0, len(nodes) - 1)

    if args.config_dir:
        config_dir = Path(str(args.config_dir)).expanduser().resolve()
    else:
        first_node = next(iter(nodes.values()))
        config_dir = parse_config_bind(first_node, topology_file)

    def collect_reports() -> Dict[str, Any]:
        reports: Dict[str, Any] = {}
        for node_name in sorted(nodes.keys()):
            container = clab_container_name(lab_name, node_name)
            neighbors = load_neighbors(config_dir, node_name)
            management_http_port = load_management_http_port(config_dir, node_name)
            reports[node_name] = inspect_node(
                use_sudo=bool(args.sudo),
                container=container,
                tail_lines=int(args.tail_lines),
                neighbors=neighbors,
                management_http_port=management_http_port,
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
