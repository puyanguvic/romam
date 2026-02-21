#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.common import SUPPORTED_PROTOCOLS, SUPPORTED_PROTOCOLS_SET


@dataclass(frozen=True)
class RunResult:
    protocol: str
    config: Path
    report_json: Path
    log_file: Path
    artifacts_dir: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run Exp1 protocol functionality suite (scenario mode) and export "
            "convergence + converged-route snapshots."
        )
    )
    parser.add_argument(
        "--config-dir",
        default="experiments/routerd_examples/unified_experiments/exp1_protocol_functionality",
        help="Directory that stores per-protocol YAML configs.",
    )
    parser.add_argument(
        "--protocols",
        default=",".join(SUPPORTED_PROTOCOLS),
        help="Comma-separated protocols to run.",
    )
    parser.add_argument(
        "--output-root",
        default="results/runs/exp1_protocol_functionality_abilene",
        help="Run outputs root (reports/logs/summary artifacts).",
    )
    parser.add_argument(
        "--table-prefix",
        default="results/tables/exp1_protocol_functionality_abilene",
        help="Prefix for summary json/csv/md outputs (without suffix).",
    )
    parser.add_argument(
        "--poll-interval-s",
        type=float,
        default=1.0,
        help="Polling interval forwarded to run_unified_experiment.py.",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for containerlab/docker commands (default: enabled).",
    )
    parser.add_argument(
        "--keep-lab",
        action="store_true",
        help="Keep lab after each run.",
    )
    parser.add_argument(
        "--skip-run",
        action="store_true",
        help="Skip execution and only summarize existing reports under output-root/reports.",
    )
    return parser.parse_args()


def parse_protocols(raw: str) -> list[str]:
    items = [str(part).strip().lower() for part in str(raw).split(",") if str(part).strip()]
    if not items:
        raise ValueError("--protocols resolved to empty list")
    unknown = [name for name in items if name not in SUPPORTED_PROTOCOLS_SET]
    if unknown:
        raise ValueError(
            "unsupported protocol(s): "
            + ", ".join(unknown)
            + "; supported: "
            + ", ".join(SUPPORTED_PROTOCOLS)
        )
    # keep order and de-duplicate
    out: list[str] = []
    seen: set[str] = set()
    for name in items:
        if name in seen:
            continue
        seen.add(name)
        out.append(name)
    return out


def parse_artifacts_dir(stdout: str) -> str:
    for line in stdout.splitlines():
        text = line.strip()
        if text.startswith("artifacts_dir:"):
            return text.split(":", maxsplit=1)[1].strip()
    return ""


def parse_artifacts_dir_from_log_file(path: Path) -> str:
    if not path.is_file():
        return ""
    return parse_artifacts_dir(path.read_text(encoding="utf-8"))


def run_one_protocol(
    *,
    protocol: str,
    cfg_path: Path,
    report_path: Path,
    log_path: Path,
    poll_interval_s: float,
    use_sudo: bool,
    keep_lab: bool,
) -> RunResult:
    cmd = [
        sys.executable,
        str(REPO_ROOT / "tools" / "run_unified_experiment.py"),
        "--config",
        str(cfg_path),
        "--output-json",
        str(report_path),
        "--poll-interval-s",
        str(float(poll_interval_s)),
        "--sudo" if use_sudo else "--no-sudo",
    ]
    if keep_lab:
        cmd.append("--keep-lab")

    print(f"[run] protocol={protocol}")
    proc = subprocess.run(
        cmd,
        check=False,
        cwd=str(REPO_ROOT),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(proc.stdout or "", encoding="utf-8")

    if proc.returncode != 0:
        tail = "\n".join((proc.stdout or "").splitlines()[-40:])
        raise RuntimeError(
            f"protocol={protocol} failed with code {proc.returncode}. "
            f"See log: {log_path}\n{tail}"
        )

    if not report_path.exists():
        raise RuntimeError(f"protocol={protocol} did not produce report json: {report_path}")

    return RunResult(
        protocol=protocol,
        config=cfg_path,
        report_json=report_path,
        log_file=log_path,
        artifacts_dir=parse_artifacts_dir(proc.stdout or ""),
    )


def load_report(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError(f"expected object json: {path}")
    return data


def sample_min_route_count(sample: dict[str, Any] | None) -> int | None:
    if not isinstance(sample, dict):
        return None
    node_data = sample.get("node_data", {})
    if not isinstance(node_data, dict):
        return None
    counts: list[int] = []
    for node_obj in node_data.values():
        if not isinstance(node_obj, dict):
            continue
        raw_count = node_obj.get("route_count")
        if isinstance(raw_count, (int, float)):
            counts.append(int(raw_count))
            continue
        routes = node_obj.get("routes", [])
        if isinstance(routes, list):
            counts.append(len(routes))
    if not counts:
        return None
    return min(counts)


def _float_or_none(value: Any) -> float | None:
    if isinstance(value, (int, float)):
        return float(value)
    try:
        return float(str(value))
    except (TypeError, ValueError):
        return None


def _int_or_none(value: Any) -> int | None:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    try:
        return int(str(value))
    except (TypeError, ValueError):
        return None


def route_sort_key(route: dict[str, Any]) -> tuple[float, float, float, str]:
    destination = _float_or_none(route.get("destination"))
    next_hop = _float_or_none(route.get("next_hop"))
    metric = _float_or_none(route.get("metric"))
    protocol = str(route.get("protocol", ""))
    return (
        destination if destination is not None else 1e15,
        next_hop if next_hop is not None else 1e15,
        metric if metric is not None else 1e15,
        protocol,
    )


def normalize_routes(sample: dict[str, Any] | None) -> dict[str, Any]:
    if not isinstance(sample, dict):
        return {
            "t_s": None,
            "all_converged": False,
            "min_route_count": None,
            "nodes": {},
        }
    node_data = sample.get("node_data", {})
    out_nodes: dict[str, list[dict[str, Any]]] = {}
    if isinstance(node_data, dict):
        for node in sorted(node_data.keys()):
            node_obj = node_data.get(node, {})
            routes_raw = node_obj.get("routes", []) if isinstance(node_obj, dict) else []
            rows: list[dict[str, Any]] = []
            if isinstance(routes_raw, list):
                for route in routes_raw:
                    if not isinstance(route, dict):
                        continue
                    rows.append(
                        {
                            "destination": route.get("destination"),
                            "next_hop": route.get("next_hop"),
                            "metric": route.get("metric"),
                            "protocol": route.get("protocol"),
                        }
                    )
            rows.sort(key=route_sort_key)
            out_nodes[str(node)] = rows
    return {
        "t_s": _float_or_none(sample.get("t_s")),
        "all_converged": bool(sample.get("all_converged", False)),
        "min_route_count": sample_min_route_count(sample),
        "nodes": out_nodes,
    }


def first_link_fault_row(report: dict[str, Any]) -> dict[str, Any] | None:
    conv = report.get("convergence", {})
    if not isinstance(conv, dict):
        return None
    fault_rows = conv.get("fault_reconvergence", [])
    if not isinstance(fault_rows, list):
        return None
    for row in fault_rows:
        if not isinstance(row, dict):
            continue
        fault = row.get("fault", {})
        if isinstance(fault, dict) and str(fault.get("type", "")).lower() == "link_down":
            return row
    return None


def route_signature_for_destination(
    sample: dict[str, Any] | None,
    *,
    node: str,
    destination_router_id: int,
) -> tuple[int | None, float | None, str | None] | None:
    if not isinstance(sample, dict):
        return None
    node_data = sample.get("node_data", {})
    if not isinstance(node_data, dict):
        return None
    node_obj = node_data.get(node, {})
    if not isinstance(node_obj, dict):
        return None
    routes = node_obj.get("routes", [])
    if not isinstance(routes, list):
        return None
    for route in routes:
        if not isinstance(route, dict):
            continue
        dst = _int_or_none(route.get("destination"))
        if dst != int(destination_router_id):
            continue
        return (
            _int_or_none(route.get("next_hop")),
            _float_or_none(route.get("metric")),
            str(route.get("protocol", "")) or None,
        )
    return None


def node_router_id_from_sample(sample: dict[str, Any] | None, node: str) -> int | None:
    if not isinstance(sample, dict):
        return None
    node_data = sample.get("node_data", {})
    if not isinstance(node_data, dict):
        return None
    node_obj = node_data.get(node, {})
    if not isinstance(node_obj, dict):
        return None
    metrics = node_obj.get("metrics", {})
    if not isinstance(metrics, dict):
        return None
    return _int_or_none(metrics.get("router_id"))


def node_neighbors_up(sample: dict[str, Any] | None, node: str) -> int | None:
    if not isinstance(sample, dict):
        return None
    node_data = sample.get("node_data", {})
    if not isinstance(node_data, dict):
        return None
    node_obj = node_data.get(node, {})
    if not isinstance(node_obj, dict):
        return None
    metrics = node_obj.get("metrics", {})
    if not isinstance(metrics, dict):
        return None
    return _int_or_none(metrics.get("neighbors_up"))


def compute_link_down_route_switch_t(
    *,
    report: dict[str, Any],
    fault_row: dict[str, Any] | None,
) -> tuple[float | None, float | None]:
    if not isinstance(fault_row, dict):
        return None, None
    fault_obj = fault_row.get("fault", {})
    if not isinstance(fault_obj, dict):
        return None, None
    if str(fault_obj.get("type", "")).lower() != "link_down":
        return None, None
    link = fault_obj.get("link", [])
    if not isinstance(link, list) or len(link) != 2:
        return None, None
    left = str(link[0]).strip()
    right = str(link[1]).strip()
    if not left or not right:
        return None, None

    samples = report.get("samples", [])
    if not isinstance(samples, list) or not samples:
        return None, None
    fault_t = _float_or_none(fault_obj.get("applied_at_s"))
    if fault_t is None:
        return None, None

    pre_fault: dict[str, Any] | None = None
    for sample in samples:
        if not isinstance(sample, dict):
            continue
        if not bool(sample.get("all_converged", False)):
            continue
        t_s = _float_or_none(sample.get("t_s"))
        if t_s is None or t_s >= fault_t:
            continue
        pre_fault = sample
    if pre_fault is None:
        return None, None

    left_id = node_router_id_from_sample(pre_fault, left)
    right_id = node_router_id_from_sample(pre_fault, right)
    if left_id is None or right_id is None:
        return None, None

    left_baseline = route_signature_for_destination(
        pre_fault,
        node=left,
        destination_router_id=right_id,
    )
    right_baseline = route_signature_for_destination(
        pre_fault,
        node=right,
        destination_router_id=left_id,
    )
    left_baseline_nbr = node_neighbors_up(pre_fault, left)
    right_baseline_nbr = node_neighbors_up(pre_fault, right)

    left_switch_t: float | None = None
    right_switch_t: float | None = None

    for sample in samples:
        if not isinstance(sample, dict):
            continue
        t_s = _float_or_none(sample.get("t_s"))
        if t_s is None or t_s < fault_t:
            continue

        if left_switch_t is None:
            left_now = route_signature_for_destination(
                sample,
                node=left,
                destination_router_id=right_id,
            )
            left_nbr_now = node_neighbors_up(sample, left)
            route_changed = left_now != left_baseline
            neighbor_dropped = (
                left_baseline_nbr is not None
                and left_nbr_now is not None
                and left_nbr_now < left_baseline_nbr
            )
            if route_changed or neighbor_dropped:
                left_switch_t = t_s

        if right_switch_t is None:
            right_now = route_signature_for_destination(
                sample,
                node=right,
                destination_router_id=left_id,
            )
            right_nbr_now = node_neighbors_up(sample, right)
            route_changed = right_now != right_baseline
            neighbor_dropped = (
                right_baseline_nbr is not None
                and right_nbr_now is not None
                and right_nbr_now < right_baseline_nbr
            )
            if route_changed or neighbor_dropped:
                right_switch_t = t_s

        if left_switch_t is not None and right_switch_t is not None:
            break

    switch_candidates = [x for x in [left_switch_t, right_switch_t] if x is not None]
    if not switch_candidates:
        return None, None
    switch_t = max(switch_candidates)
    return switch_t, max(0.0, switch_t - fault_t)


def find_snapshots(report: dict[str, Any]) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    samples = report.get("samples", [])
    if not isinstance(samples, list) or not samples:
        return None, None

    link_row = first_link_fault_row(report)
    fault_t = None
    if isinstance(link_row, dict):
        fault = link_row.get("fault", {})
        if isinstance(fault, dict):
            fault_t = _float_or_none(fault.get("applied_at_s"))

    pre_fault: dict[str, Any] | None = None
    post_fault: dict[str, Any] | None = None
    for sample in samples:
        if not isinstance(sample, dict) or not bool(sample.get("all_converged", False)):
            continue
        t_s = _float_or_none(sample.get("t_s"))
        if pre_fault is None:
            pre_fault = sample
        if fault_t is None:
            continue
        if t_s is None:
            continue
        if t_s < fault_t:
            pre_fault = sample
        elif t_s >= fault_t and post_fault is None:
            post_fault = sample

    return pre_fault, post_fault


def find_fault_window_snapshots(
    report: dict[str, Any],
    *,
    fault_t_s: float | None,
    post_fault_threshold_s: float | None,
) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    samples = report.get("samples", [])
    if not isinstance(samples, list) or not samples:
        return None, None

    pre_fault: dict[str, Any] | None = None
    post_fault: dict[str, Any] | None = None
    for sample in samples:
        if not isinstance(sample, dict) or not bool(sample.get("all_converged", False)):
            continue
        t_s = _float_or_none(sample.get("t_s"))
        if t_s is None:
            continue
        if fault_t_s is None:
            if pre_fault is None:
                pre_fault = sample
        elif t_s < fault_t_s:
            pre_fault = sample

        threshold = post_fault_threshold_s if post_fault_threshold_s is not None else fault_t_s
        if threshold is None:
            continue
        if t_s >= threshold and post_fault is None:
            post_fault = sample
            break
    return pre_fault, post_fault


def summarize_report(run: RunResult) -> tuple[dict[str, Any], dict[str, Any]]:
    report = load_report(run.report_json)
    conv = report.get("convergence", {}) if isinstance(report.get("convergence"), dict) else {}
    link_row = first_link_fault_row(report)
    fault_t = None
    if isinstance(link_row, dict):
        fault = link_row.get("fault", {})
        if isinstance(fault, dict):
            fault_t = _float_or_none(fault.get("applied_at_s"))
    route_switch_t, route_switch_delay = compute_link_down_route_switch_t(
        report=report,
        fault_row=link_row,
    )
    pre_fault_sample, post_fault_sample = find_fault_window_snapshots(
        report,
        fault_t_s=fault_t,
        post_fault_threshold_s=route_switch_t,
    )
    if post_fault_sample is None:
        fallback_pre, fallback_post = find_snapshots(report)
        pre_fault_sample = pre_fault_sample or fallback_pre
        post_fault_sample = fallback_post

    reconv_t = None
    reconv_delay = None
    if isinstance(link_row, dict):
        fault = link_row.get("fault", {})
        if isinstance(fault, dict):
            fault_t = _float_or_none(fault.get("applied_at_s"))
        reconv_t = _float_or_none(link_row.get("reconverged_at_s"))
        reconv_delay = _float_or_none(link_row.get("reconvergence_delay_s"))

    row = {
        "protocol": run.protocol,
        "config": str(run.config),
        "report_json": str(run.report_json),
        "log_file": str(run.log_file),
        "artifacts_dir": run.artifacts_dir,
        "sample_count": (
            len(report.get("samples", []))
            if isinstance(report.get("samples"), list)
            else 0
        ),
        "initial_converged_at_s": _float_or_none(conv.get("initial_converged_at_s")),
        "link_down_time_s": fault_t,
        "link_down_reconverged_at_s": reconv_t,
        "link_down_reconvergence_delay_s": reconv_delay,
        "link_down_route_switch_at_s": route_switch_t,
        "link_down_route_switch_delay_s": route_switch_delay,
        "pre_fault_sample_t_s": _float_or_none(
            pre_fault_sample.get("t_s") if isinstance(pre_fault_sample, dict) else None
        ),
        "post_fault_sample_t_s": _float_or_none(
            post_fault_sample.get("t_s") if isinstance(post_fault_sample, dict) else None
        ),
        "pre_fault_min_route_count": sample_min_route_count(pre_fault_sample),
        "post_fault_min_route_count": sample_min_route_count(post_fault_sample),
    }

    snapshots = {
        "protocol": run.protocol,
        "pre_fault": normalize_routes(pre_fault_sample),
        "post_fault": normalize_routes(post_fault_sample),
    }
    return row, snapshots


def fmt_cell(value: Any, digits: int = 3) -> str:
    if value is None:
        return "-"
    if isinstance(value, float):
        return f"{value:.{digits}f}"
    return str(value)


def write_summary_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    fields = [
        "protocol",
        "initial_converged_at_s",
        "link_down_time_s",
        "link_down_reconverged_at_s",
        "link_down_reconvergence_delay_s",
        "link_down_route_switch_at_s",
        "link_down_route_switch_delay_s",
        "pre_fault_sample_t_s",
        "post_fault_sample_t_s",
        "pre_fault_min_route_count",
        "post_fault_min_route_count",
        "sample_count",
        "config",
        "report_json",
        "log_file",
        "artifacts_dir",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({key: row.get(key) for key in fields})


def render_routes_md(snapshot: dict[str, Any]) -> list[str]:
    lines: list[str] = []
    nodes = snapshot.get("nodes", {}) if isinstance(snapshot, dict) else {}
    if not isinstance(nodes, dict) or not nodes:
        return ["- no converged snapshot"]

    lines.append("| node | destination | next_hop | metric | protocol |")
    lines.append("|---|---:|---:|---:|---|")
    for node in sorted(nodes.keys()):
        routes = nodes.get(node, [])
        if not isinstance(routes, list) or not routes:
            lines.append(f"| {node} | - | - | - | - |")
            continue
        for route in routes:
            if not isinstance(route, dict):
                continue
            lines.append(
                "| "
                + f"{node} | {fmt_cell(route.get('destination'), digits=0)}"
                + f" | {fmt_cell(route.get('next_hop'), digits=0)}"
                + f" | {fmt_cell(route.get('metric'))}"
                + f" | {fmt_cell(route.get('protocol'))} |"
            )
    return lines


def write_summary_markdown(
    path: Path,
    *,
    rows: list[dict[str, Any]],
    snapshots_by_protocol: dict[str, dict[str, Any]],
) -> None:
    lines: list[str] = [
        "# Exp1 Protocol Functionality (abilene)",
        "",
        "## Convergence Summary",
        "",
        (
            "| protocol | initial_converged_s | fault_reconverged_s | "
            "fault_reconvergence_delay_s | route_switch_delay_s | pre_fault_min_routes | "
            "post_fault_min_routes |"
        ),
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            "| "
            + f"{row['protocol']}"
            + f" | {fmt_cell(row.get('initial_converged_at_s'))}"
            + f" | {fmt_cell(row.get('link_down_reconverged_at_s'))}"
            + f" | {fmt_cell(row.get('link_down_reconvergence_delay_s'))}"
            + f" | {fmt_cell(row.get('link_down_route_switch_delay_s'))}"
            + f" | {fmt_cell(row.get('pre_fault_min_route_count'), digits=0)}"
            + f" | {fmt_cell(row.get('post_fault_min_route_count'), digits=0)} |"
        )

    for row in rows:
        protocol = str(row["protocol"])
        snap = snapshots_by_protocol.get(protocol, {})
        pre_fault = snap.get("pre_fault", {}) if isinstance(snap, dict) else {}
        post_fault = snap.get("post_fault", {}) if isinstance(snap, dict) else {}

        lines.extend(
            [
                "",
                f"## Protocol: {protocol}",
                f"- report: `{row.get('report_json', '-')}`",
                f"- artifacts_dir: `{row.get('artifacts_dir', '-')}`",
                (
                    "- fault route-switch t_s: "
                    f"{fmt_cell(row.get('link_down_route_switch_at_s'))}"
                ),
                (
                    "- fault route-switch delay_s: "
                    f"{fmt_cell(row.get('link_down_route_switch_delay_s'))}"
                ),
                (
                    "- pre-fault converged sample t_s: "
                    f"{fmt_cell(pre_fault.get('t_s') if isinstance(pre_fault, dict) else None)}"
                ),
                (
                    "- post-fault converged sample t_s: "
                    f"{fmt_cell(post_fault.get('t_s') if isinstance(post_fault, dict) else None)}"
                ),
                "",
                "### Converged Routes (Pre-Fault)",
            ]
        )
        lines.extend(render_routes_md(pre_fault if isinstance(pre_fault, dict) else {}))
        lines.extend(["", "### Converged Routes (Post-Fault)"])
        lines.extend(render_routes_md(post_fault if isinstance(post_fault, dict) else {}))

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()

    protocols = parse_protocols(args.protocols)
    cfg_dir = (REPO_ROOT / str(args.config_dir)).resolve()
    if not cfg_dir.is_dir():
        raise FileNotFoundError(f"config dir not found: {cfg_dir}")

    output_root = (REPO_ROOT / str(args.output_root)).resolve()
    reports_dir = output_root / "reports"
    logs_dir = output_root / "logs"
    reports_dir.mkdir(parents=True, exist_ok=True)
    logs_dir.mkdir(parents=True, exist_ok=True)

    run_results: list[RunResult] = []
    for protocol in protocols:
        cfg_path = cfg_dir / f"{protocol}.yaml"
        if not cfg_path.is_file():
            raise FileNotFoundError(f"missing config for protocol={protocol}: {cfg_path}")
        report_path = reports_dir / f"{protocol}.json"
        log_path = logs_dir / f"{protocol}.log"

        if args.skip_run:
            if not report_path.is_file():
                raise FileNotFoundError(
                    f"--skip-run set but report missing for protocol={protocol}: {report_path}"
                )
            run_results.append(
                RunResult(
                    protocol=protocol,
                    config=cfg_path,
                    report_json=report_path,
                    log_file=log_path,
                    artifacts_dir=parse_artifacts_dir_from_log_file(log_path),
                )
            )
            continue

        run_results.append(
            run_one_protocol(
                protocol=protocol,
                cfg_path=cfg_path,
                report_path=report_path,
                log_path=log_path,
                poll_interval_s=float(args.poll_interval_s),
                use_sudo=bool(args.sudo),
                keep_lab=bool(args.keep_lab),
            )
        )

    summary_rows: list[dict[str, Any]] = []
    snapshots_by_protocol: dict[str, dict[str, Any]] = {}
    for run in run_results:
        row, snapshots = summarize_report(run)
        summary_rows.append(row)
        snapshots_by_protocol[run.protocol] = snapshots

    summary_payload = {
        "experiment": "exp1_protocol_functionality_abilene",
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "protocols": protocols,
        "rows": summary_rows,
        "route_snapshots": snapshots_by_protocol,
    }

    output_root.mkdir(parents=True, exist_ok=True)
    run_summary_json = output_root / "summary.json"
    run_summary_json.write_text(
        json.dumps(summary_payload, ensure_ascii=False, indent=2, sort_keys=True),
        encoding="utf-8",
    )

    table_prefix = (REPO_ROOT / str(args.table_prefix)).resolve()
    table_prefix.parent.mkdir(parents=True, exist_ok=True)
    table_json = table_prefix.with_suffix(".json")
    table_csv = table_prefix.with_suffix(".csv")
    table_md = table_prefix.with_name(table_prefix.name + "_routes").with_suffix(".md")

    table_json.write_text(
        json.dumps(summary_payload, ensure_ascii=False, indent=2, sort_keys=True),
        encoding="utf-8",
    )
    write_summary_csv(table_csv, summary_rows)
    write_summary_markdown(
        table_md,
        rows=summary_rows,
        snapshots_by_protocol=snapshots_by_protocol,
    )

    print(f"run_summary_json: {run_summary_json}")
    print(f"table_json: {table_json}")
    print(f"table_csv: {table_csv}")
    print(f"route_markdown: {table_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
