#!/usr/bin/env python3
from __future__ import annotations

import json
import math
import os
import shlex
import shutil
import subprocess
from pathlib import Path
from typing import Any

import yaml

SUPPORTED_PROTOCOLS: tuple[str, ...] = (
    "ospf",
    "rip",
    "ecmp",
    "topk",
    "irp",
    "ddr",
    "dgr",
    "octopus",
)
SUPPORTED_PROTOCOLS_SET = frozenset(SUPPORTED_PROTOCOLS)
ADAPTIVE_PROTOCOLS = frozenset({"ddr", "dgr", "octopus"})
STOCHASTIC_ADAPTIVE_PROTOCOLS = frozenset({"dgr", "octopus"})


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError(f"Expected object JSON: {path}")
    return data


def load_yaml(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    if not isinstance(data, dict):
        raise ValueError(f"Expected mapping YAML: {path}")
    return data


def parse_keyed_output(text: str, *, keys: tuple[str, ...] | list[str]) -> dict[str, str]:
    out: dict[str, str] = {}
    key_list = [str(key) for key in keys]
    for line in text.splitlines():
        stripped = line.strip()
        for key in key_list:
            prefix = f"{key}:"
            if stripped.startswith(prefix):
                out[key] = stripped[len(prefix) :].strip()
    return out


def parse_env_file(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        text = line.strip()
        if not text or text.startswith("#") or "=" not in text:
            continue
        key, value = text.split("=", maxsplit=1)
        out[key.strip()] = value.strip()
    return out


def resolve_path(path_value: str, base_dir: Path) -> Path:
    path = Path(path_value).expanduser()
    if path.is_absolute():
        return path.resolve()
    base_candidate = (base_dir / path).resolve()
    if base_candidate.exists():
        return base_candidate
    cwd_candidate = (Path.cwd() / path).resolve()
    if cwd_candidate.exists():
        return cwd_candidate
    return base_candidate


def resolve_clab_bin(preferred: str = "") -> str:
    if preferred:
        return preferred
    for candidate in ("clab", "containerlab"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("Cannot find `clab` or `containerlab` in PATH.")


def run_command(
    cmd: list[str],
    *,
    check: bool = True,
    capture_output: bool = True,
    env: dict[str, str] | None = None,
    cwd: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        cmd,
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.STDOUT if capture_output else None,
        env=env,
        cwd=str(cwd) if cwd is not None else None,
    )
    if check and proc.returncode != 0:
        out = (proc.stdout or "").strip()
        pretty_cmd = " ".join(shlex.quote(str(token)) for token in cmd)
        raise RuntimeError(f"Command failed ({proc.returncode}): {pretty_cmd}\n{out}")
    return proc


def run_clab_command(
    cmd: list[str],
    *,
    use_sudo: bool,
    env_overrides: dict[str, str] | None = None,
    check: bool = True,
    capture_output: bool = True,
) -> subprocess.CompletedProcess[str]:
    if use_sudo:
        exports = [f"{k}={v}" for k, v in sorted((env_overrides or {}).items())]
        return run_command(
            ["sudo", "env", *exports, *cmd],
            check=check,
            capture_output=capture_output,
        )
    merged_env = {**os.environ, **(env_overrides or {})}
    return run_command(
        cmd,
        check=check,
        capture_output=capture_output,
        env=merged_env,
    )


def parse_bool_arg(arg_value: bool | None, fallback: bool) -> bool:
    if arg_value is None:
        return fallback
    return bool(arg_value)


def load_topology_node_names(topology_file: Path) -> list[str]:
    topo = load_yaml(topology_file)
    nodes = dict(topo.get("topology", {}).get("nodes", {}))
    return [str(name) for name in nodes.keys()]


def load_management_ports_from_configs(config_dir: Path, nodes: list[str]) -> dict[str, int]:
    out: dict[str, int] = {}
    for node in nodes:
        cfg = load_yaml(config_dir / f"{node}.yaml")
        try:
            port = int(cfg.get("management", {}).get("http", {}).get("port", 0))
        except (TypeError, ValueError):
            port = 0
        if port > 0:
            out[node] = port
    return out


def parse_output_json(text: str) -> Any:
    payload = text.strip()
    if not payload:
        return {}
    try:
        return json.loads(payload)
    except json.JSONDecodeError:
        return {"raw": payload}


def summarize_neighbor_fast_state_from_metrics_map(
    metrics_by_node: dict[str, Any],
) -> dict[str, Any]:
    per_node: dict[str, dict[str, Any]] = {}
    nodes_with_fast_state = 0
    total_entries = 0

    all_queue_levels: list[float] = []
    all_interface_utils: list[float] = []
    all_delay_ms: list[float] = []
    all_loss_rates: list[float] = []

    for node, metrics_obj in metrics_by_node.items():
        protocol_metrics = (
            metrics_obj.get("protocol_metrics", {})
            if isinstance(metrics_obj, dict)
            else {}
        )
        if not isinstance(protocol_metrics, dict):
            protocol_metrics = {}
        fast_state = protocol_metrics.get("neighbor_fast_state", {})
        if not isinstance(fast_state, dict):
            fast_state = {}

        node_queue_levels: list[float] = []
        node_interface_utils: list[float] = []
        node_delay_ms: list[float] = []
        node_loss_rates: list[float] = []
        node_entries = 0

        for fields in fast_state.values():
            if not isinstance(fields, dict):
                continue
            node_entries += 1

            queue_level = _non_negative_float(fields.get("queue_level"))
            if queue_level is not None:
                node_queue_levels.append(queue_level)
                all_queue_levels.append(queue_level)

            interface_util = _unit_interval_float(fields.get("interface_utilization"))
            if interface_util is not None:
                node_interface_utils.append(interface_util)
                all_interface_utils.append(interface_util)

            delay_ms = _non_negative_float(fields.get("delay_ms"))
            if delay_ms is not None:
                node_delay_ms.append(delay_ms)
                all_delay_ms.append(delay_ms)

            loss_rate = _unit_interval_float(fields.get("loss_rate"))
            if loss_rate is not None:
                node_loss_rates.append(loss_rate)
                all_loss_rates.append(loss_rate)

        if node_entries > 0:
            nodes_with_fast_state += 1
            total_entries += node_entries

        per_node[str(node)] = {
            "entries": node_entries,
            "avg_queue_level": _safe_mean(node_queue_levels),
            "avg_interface_utilization": _safe_mean(node_interface_utils),
            "avg_delay_ms": _safe_mean(node_delay_ms),
            "avg_loss_rate": _safe_mean(node_loss_rates),
        }

    return {
        "sampled_nodes": len(metrics_by_node),
        "nodes_with_neighbor_fast_state": nodes_with_fast_state,
        "neighbor_fast_state_entries": total_entries,
        "avg_queue_level": _safe_mean(all_queue_levels),
        "avg_interface_utilization": _safe_mean(all_interface_utils),
        "avg_delay_ms": _safe_mean(all_delay_ms),
        "avg_loss_rate": _safe_mean(all_loss_rates),
        "per_node": per_node,
    }


def _safe_mean(values: list[float]) -> float | None:
    if not values:
        return None
    return sum(values) / float(len(values))


def _non_negative_float(value: Any) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    f = float(value)
    if not math.isfinite(f) or f < 0.0:
        return None
    return f


def _unit_interval_float(value: Any) -> float | None:
    f = _non_negative_float(value)
    if f is None:
        return None
    return min(1.0, f)
