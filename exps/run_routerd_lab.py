#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

DEPLOY_LINE_RE = re.compile(r"^Deploy:\s+sudo\s+(\S+)\s+deploy\s+-t\s+(\S+)\s+--reconfigure\s*$")
TOPOLOGY_LINE_RE = re.compile(r"^topology_file:\s*(\S+)\s*$")


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description=(
            "Generate routerd lab, deploy with containerlab, run health check, "
            "and optionally destroy lab."
        )
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for containerlab/docker operations (default: enabled).",
    )
    parser.add_argument(
        "--keep-lab",
        action="store_true",
        help="Keep the lab running after checks (skip destroy).",
    )
    parser.add_argument(
        "--check-tail-lines",
        type=int,
        default=60,
        help="Tail lines read from /tmp/routerd.log per node during health check.",
    )
    parser.add_argument(
        "--check-max-wait-s",
        type=float,
        default=10.0,
        help="Max wait seconds for convergence evidence during health check.",
    )
    parser.add_argument(
        "--check-poll-interval-s",
        type=float,
        default=1.0,
        help="Health check polling interval in seconds.",
    )
    parser.add_argument(
        "--check-min-routes",
        type=int,
        default=-1,
        help="Minimum route count expected during health check (-1 means n_nodes-1).",
    )
    parser.add_argument(
        "--check-output-json",
        default="",
        help="Optional path to write health-check JSON report.",
    )
    args, gen_args = parser.parse_known_args()
    return args, gen_args


def with_sudo(cmd: list[str], use_sudo: bool) -> list[str]:
    return ["sudo", *cmd] if use_sudo else cmd


def run_cmd(cmd: list[str], check: bool = True, capture_output: bool = True) -> str:
    result = subprocess.run(
        cmd,
        check=check,
        text=True,
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.STDOUT if capture_output else None,
    )
    return (result.stdout or "").strip()


def infer_protocol(gen_args: list[str]) -> str:
    for idx, arg in enumerate(gen_args):
        if arg == "--protocol" and idx + 1 < len(gen_args):
            return str(gen_args[idx + 1]).strip().lower()
        if arg.startswith("--protocol="):
            return arg.split("=", maxsplit=1)[1].strip().lower()
    return "ospf"


def parse_generator_output(text: str) -> tuple[Path, str | None]:
    topology_path: Path | None = None
    deploy_clab_bin: str | None = None

    for line in text.splitlines():
        topo_match = TOPOLOGY_LINE_RE.match(line.strip())
        if topo_match:
            topology_path = Path(topo_match.group(1)).expanduser().resolve()
            continue

        deploy_match = DEPLOY_LINE_RE.match(line.strip())
        if deploy_match:
            deploy_clab_bin = deploy_match.group(1)

    if topology_path is None:
        raise RuntimeError("Cannot parse topology_file from generate_routerd_lab.py output")

    return topology_path, deploy_clab_bin


def resolve_containerlab_bin(deploy_clab_bin: str | None) -> str:
    clab_bin = shutil.which("containerlab")
    if clab_bin:
        return clab_bin
    if deploy_clab_bin:
        return deploy_clab_bin
    raise RuntimeError(
        "containerlab not found in PATH; install it or add it to PATH (e.g. ~/.local/bin)."
    )


def main() -> int:
    args, gen_args = parse_args()
    protocol = infer_protocol(gen_args)

    gen_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "generate_routerd_lab.py"),
        *gen_args,
    ]

    print(f"[1/4] Generate lab: {' '.join(gen_cmd)}")
    generated_text = run_cmd(gen_cmd, check=True, capture_output=True)
    print(generated_text)

    topology_file, deploy_clab_bin = parse_generator_output(generated_text)
    clab_bin = resolve_containerlab_bin(deploy_clab_bin)

    deploy_cmd = [clab_bin, "deploy", "-t", str(topology_file), "--reconfigure"]
    print(f"[2/4] Deploy lab: {' '.join(with_sudo(deploy_cmd, args.sudo))}")
    run_cmd(with_sudo(deploy_cmd, args.sudo), check=True, capture_output=False)

    check_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "check_routerd_lab.py"),
        "--topology-file",
        str(topology_file),
        "--expect-protocol",
        protocol,
        "--tail-lines",
        str(args.check_tail_lines),
        "--max-wait-s",
        str(args.check_max_wait_s),
        "--poll-interval-s",
        str(args.check_poll_interval_s),
        "--min-routes",
        str(args.check_min_routes),
    ]
    if args.check_output_json:
        check_cmd.extend(["--output-json", str(args.check_output_json)])
    if args.sudo:
        check_cmd.append("--sudo")

    try:
        print(f"[3/4] Check lab: {' '.join(check_cmd)}")
        run_cmd(check_cmd, check=True, capture_output=False)
    finally:
        if args.keep_lab:
            print("[4/4] Keep lab: skipping destroy (--keep-lab).")
            print(f"topology_file: {topology_file}")
        else:
            destroy_cmd = [clab_bin, "destroy", "-t", str(topology_file), "--cleanup"]
            print(f"[4/4] Destroy lab: {' '.join(with_sudo(destroy_cmd, args.sudo))}")
            run_cmd(with_sudo(destroy_cmd, args.sudo), check=False, capture_output=False)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
