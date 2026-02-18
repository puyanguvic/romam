#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Any

import yaml


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
) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        cmd,
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.STDOUT if capture_output else None,
        env=env,
    )
    if check and proc.returncode != 0:
        out = (proc.stdout or "").strip()
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{out}")
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
