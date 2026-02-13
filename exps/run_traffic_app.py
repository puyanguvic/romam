#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shlex
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run irp traffic_app inside a router container."
    )
    parser.add_argument("--lab-name", required=True, help="Containerlab lab name.")
    parser.add_argument("--node", required=True, help="Node name (e.g. r1).")
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for docker exec (default: enabled).",
    )
    parser.add_argument(
        "--background",
        action="store_true",
        help="Run app in background with nohup inside the container.",
    )
    parser.add_argument(
        "--log-file",
        default="/tmp/traffic_app.log",
        help="Background mode log file path inside container.",
    )
    parser.add_argument(
        "app_args",
        nargs=argparse.REMAINDER,
        help="Arguments forwarded to `python3 -m irp.apps.traffic_app`.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    app_args = list(args.app_args)
    if app_args and app_args[0] == "--":
        app_args = app_args[1:]
    if not app_args:
        print(
            "Missing app args. Example: -- send --proto udp --target 10.0.0.2 --port 9000",
            file=sys.stderr,
        )
        return 2

    container = f"clab-{args.lab_name}-{args.node}"
    inner_cmd = "PYTHONPATH=/irp/src python3 -m irp.apps.traffic_app " + shlex.join(app_args)
    if bool(args.background):
        log_file = shlex.quote(str(args.log_file))
        inner_cmd = f"nohup {inner_cmd} >{log_file} 2>&1 & echo $!"

    cmd = ["docker", "exec", container, "sh", "-lc", inner_cmd]
    if bool(args.sudo):
        cmd = ["sudo", *cmd]

    proc = subprocess.run(
        cmd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    output = (proc.stdout or "").strip()
    if output:
        print(output)
    if proc.returncode != 0:
        return proc.returncode
    if bool(args.background):
        print(f"started background app in {container}, log={args.log_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

