#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import shlex
import time
from pathlib import Path
from typing import Any

import yaml
from common import load_json, parse_bool_arg, resolve_clab_bin, run_clab_command

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TOPOLOGY_DATA = REPO_ROOT / "src" / "clab" / "topology-data.json"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Start multiple apps on clab nodes through `clab exec`."
    )
    parser.add_argument(
        "--topology-data",
        default=str(DEFAULT_TOPOLOGY_DATA),
        help="Path to topology-data.json.",
    )
    parser.add_argument(
        "--plan",
        required=True,
        help="App plan file (YAML/JSON). Must contain `apps` list.",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=None,
        help="Use sudo for clab command. Default from topology-data.json (fallback false).",
    )
    parser.add_argument(
        "--continue-on-error",
        action="store_true",
        help="Continue launching remaining apps if one app fails.",
    )
    parser.add_argument(
        "--clab-bin",
        default="",
        help="Path/name of clab binary. Default: auto-detect clab/containerlab.",
    )
    return parser.parse_args()


def load_plan(path: Path) -> list[dict[str, Any]]:
    text = path.read_text(encoding="utf-8")
    if path.suffix.lower() == ".json":
        data = json.loads(text)
    else:
        data = yaml.safe_load(text)
    if not isinstance(data, dict):
        raise ValueError("Plan root must be an object with key `apps`.")
    apps = data.get("apps", [])
    if not isinstance(apps, list) or not apps:
        raise ValueError("Plan must contain non-empty `apps` list.")
    out: list[dict[str, Any]] = []
    for idx, app in enumerate(apps, start=1):
        if not isinstance(app, dict):
            raise ValueError(f"apps[{idx}] must be an object.")
        out.append(app)
    return out


def build_app_cmd(item: dict[str, Any]) -> str:
    cmd = str(item.get("cmd", "")).strip()
    args = list(item.get("args", []) or [])
    if not cmd and not args:
        raise ValueError("Each app must provide `cmd` or `args`.")
    if not cmd:
        cmd = shlex.join(str(part) for part in args)

    env_map = dict(item.get("env", {}) or {})
    if env_map:
        prefix = " ".join(f"{key}={shlex.quote(str(value))}" for key, value in env_map.items())
        cmd = f"{prefix} {cmd}".strip()

    background = bool(item.get("background", False))
    if not background:
        return f"sh -lc {shlex.quote(cmd)}"

    log_file = str(item.get("log_file", "/tmp/app.log"))
    return (
        f"nohup sh -lc {shlex.quote(cmd)} "
        f">{shlex.quote(log_file)} 2>&1 & echo $!"
    )


def main() -> int:
    args = parse_args()
    topology_data_path = Path(str(args.topology_data)).expanduser().resolve()
    if not topology_data_path.is_file():
        raise FileNotFoundError(f"topology-data json not found: {topology_data_path}")
    plan_path = Path(str(args.plan)).expanduser().resolve()
    if not plan_path.is_file():
        raise FileNotFoundError(f"plan file not found: {plan_path}")

    topology_data = load_json(topology_data_path)
    lab_name = str(topology_data.get("lab_name", "")).strip()
    if not lab_name:
        raise ValueError("topology-data.json missing `lab_name`.")

    apps = load_plan(plan_path)
    clab_bin = resolve_clab_bin(str(args.clab_bin).strip())
    use_sudo = parse_bool_arg(args.sudo, bool(topology_data.get("sudo", False)))

    total = len(apps)
    failures = 0
    for idx, item in enumerate(apps, start=1):
        node = str(item.get("node", "")).strip()
        if not node:
            raise ValueError(f"apps[{idx}] missing `node`.")
        name = str(item.get("name", f"app_{idx}")).strip()
        delay_s = float(item.get("delay_s", 0.0))
        app_cmd = build_app_cmd(item)
        cmd = [
            clab_bin,
            "exec",
            "--name",
            lab_name,
            "--node",
            node,
            "--cmd",
            app_cmd,
        ]
        print(f"[{idx}/{total}] node={node} name={name}")
        proc = run_clab_command(
            cmd,
            use_sudo=use_sudo,
            check=False,
            capture_output=True,
        )
        text = (proc.stdout or "").strip()
        if proc.returncode == 0:
            if text:
                print(text)
        else:
            failures += 1
            print(f"[error] rc={proc.returncode} node={node} name={name}")
            if text:
                print(text)
            if not bool(args.continue_on_error):
                raise RuntimeError("Abort due to app launch failure.")
        if delay_s > 0:
            time.sleep(delay_s)

    if failures > 0:
        print(f"completed_with_failures: {failures}")
        return 1
    print("completed: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

