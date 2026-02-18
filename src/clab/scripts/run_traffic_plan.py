#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import yaml

REPO_ROOT = Path(__file__).resolve().parents[3]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run traffic_app deployment/launch by YAML plan. "
            "Useful for reproducible experiment orchestration."
        )
    )
    parser.add_argument("--plan", required=True, help="Path to traffic plan yaml.")
    return parser.parse_args()


def run_cmd(cmd: list[str], check: bool = True) -> None:
    proc = subprocess.run(cmd, check=False, text=True)
    if check and proc.returncode != 0:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(cmd)}")


def load_plan(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    if not isinstance(data, dict):
        raise ValueError("plan must be a mapping")
    return data


def main() -> int:
    args = parse_args()
    plan_path = Path(str(args.plan)).expanduser().resolve()
    if not plan_path.exists():
        print(f"plan file not found: {plan_path}", file=sys.stderr)
        return 2

    plan = load_plan(plan_path)
    lab_name = str(plan.get("lab_name", "")).strip()
    if not lab_name:
        print("plan missing required field: lab_name", file=sys.stderr)
        return 2

    use_sudo = bool(plan.get("sudo", True))

    install_cfg = dict(plan.get("install", {}) or {})
    install_enabled = bool(install_cfg.get("enabled", True))
    bin_path = str(install_cfg.get("bin_path", "bin/traffic_app"))

    tasks = list(plan.get("tasks", []) or [])
    if not tasks:
        print("plan has no tasks; nothing to run", file=sys.stderr)
        return 2

    install_nodes = install_cfg.get("nodes")
    if install_nodes is None:
        install_nodes = sorted(
            {
                str(item.get("node", "")).strip()
                for item in tasks
                if isinstance(item, dict) and str(item.get("node", "")).strip()
            }
        )

    if install_enabled:
        node_csv = ",".join(str(x) for x in install_nodes if str(x).strip())
        cmd = [
            sys.executable,
            str(REPO_ROOT / "src" / "clab" / "scripts" / "install_traffic_app_bin.py"),
            "--lab-name",
            lab_name,
            "--bin-path",
            bin_path,
            "--nodes",
            node_csv,
            "--sudo" if use_sudo else "--no-sudo",
        ]
        print(f"[1/{len(tasks)+1}] install traffic_app: {' '.join(shlex.quote(s) for s in cmd)}")
        run_cmd(cmd, check=True)

    for idx, task in enumerate(tasks, start=1):
        if not isinstance(task, dict):
            raise ValueError(f"task #{idx} must be a mapping")

        node = str(task.get("node", "")).strip()
        if not node:
            raise ValueError(f"task #{idx} missing node")
        app_args = list(task.get("args", []) or [])
        if not app_args:
            raise ValueError(f"task #{idx} missing args")

        background = bool(task.get("background", False))
        log_file = str(task.get("log_file", "/tmp/traffic_app.log"))
        delay_s = float(task.get("delay_s", 0.0))

        cmd = [
            sys.executable,
            str(REPO_ROOT / "src" / "clab" / "scripts" / "run_traffic_app.py"),
            "--lab-name",
            lab_name,
            "--node",
            node,
            "--sudo" if use_sudo else "--no-sudo",
        ]
        if background:
            cmd.extend(["--background", "--log-file", log_file])
        cmd.extend(["--", *[str(x) for x in app_args]])

        step = idx + 1 if install_enabled else idx
        total = len(tasks) + 1 if install_enabled else len(tasks)
        print(f"[{step}/{total}] run task on node={node}: {' '.join(shlex.quote(s) for s in cmd)}")
        run_cmd(cmd, check=True)

        if delay_s > 0:
            time.sleep(delay_s)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
