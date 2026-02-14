#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

import yaml


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Install prebuilt traffic_app binary into running containerlab nodes "
            "at /irp/bin/traffic_app."
        )
    )
    parser.add_argument("--lab-name", required=True, help="Containerlab lab name.")
    parser.add_argument(
        "--bin-path",
        default="bin/traffic_app",
        help="Local binary path to copy into containers.",
    )
    parser.add_argument(
        "--topology-file",
        default="",
        help="Optional topology file used to resolve node names.",
    )
    parser.add_argument(
        "--nodes",
        default="",
        help="Optional comma-separated node names (e.g. r1,r2,r3).",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for docker operations (default: enabled).",
    )
    return parser.parse_args()


def with_sudo(cmd: list[str], use_sudo: bool) -> list[str]:
    return ["sudo", *cmd] if use_sudo else cmd


def run_cmd(cmd: list[str], *, check: bool = True) -> str:
    proc = subprocess.run(
        cmd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    output = (proc.stdout or "").strip()
    if check and proc.returncode != 0:
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{output}")
    return output


def parse_nodes_from_topology(topology_file: Path) -> list[str]:
    with topology_file.open("r", encoding="utf-8") as f:
        topo = yaml.safe_load(f) or {}
    nodes = dict(topo.get("topology", {}).get("nodes", {}))
    return [str(name) for name in nodes.keys()]


def parse_nodes_from_containers(lab_name: str, use_sudo: bool) -> list[str]:
    output = run_cmd(with_sudo(["docker", "ps", "--format", "{{.Names}}"], use_sudo), check=True)
    prefix = f"clab-{lab_name}-"
    nodes: list[str] = []
    for line in output.splitlines():
        name = line.strip()
        if not name.startswith(prefix):
            continue
        nodes.append(name[len(prefix) :])
    return sorted(nodes)


def parse_nodes(nodes_csv: str) -> list[str]:
    out: list[str] = []
    for raw in str(nodes_csv).split(","):
        node = raw.strip()
        if node:
            out.append(node)
    return out


def install_binary_for_node(
    *,
    lab_name: str,
    node: str,
    bin_path: Path,
    use_sudo: bool,
) -> None:
    container = f"clab-{lab_name}-{node}"
    run_cmd(
        with_sudo(["docker", "exec", container, "mkdir", "-p", "/irp/bin"], use_sudo),
        check=True,
    )
    run_cmd(
        with_sudo(
            ["docker", "cp", str(bin_path.resolve()), f"{container}:/irp/bin/traffic_app"],
            use_sudo,
        ),
        check=True,
    )
    run_cmd(
        with_sudo(["docker", "exec", container, "chmod", "+x", "/irp/bin/traffic_app"], use_sudo),
        check=True,
    )


def main() -> int:
    args = parse_args()
    bin_path = Path(str(args.bin_path)).expanduser().resolve()
    if not bin_path.exists():
        print(f"binary not found: {bin_path}", file=sys.stderr)
        return 2

    nodes_from_arg = parse_nodes(str(args.nodes))
    if nodes_from_arg:
        nodes = nodes_from_arg
    elif str(args.topology_file).strip():
        topo_file = Path(str(args.topology_file)).expanduser().resolve()
        if not topo_file.exists():
            print(f"topology file not found: {topo_file}", file=sys.stderr)
            return 2
        nodes = parse_nodes_from_topology(topo_file)
    else:
        nodes = parse_nodes_from_containers(str(args.lab_name), bool(args.sudo))

    if not nodes:
        print("no nodes resolved; provide --nodes or --topology-file", file=sys.stderr)
        return 2

    print(f"installing {bin_path} into lab={args.lab_name} nodes={','.join(nodes)}")
    for node in nodes:
        install_binary_for_node(
            lab_name=str(args.lab_name),
            node=str(node),
            bin_path=bin_path,
            use_sudo=bool(args.sudo),
        )
        print(f"installed on node={node}")

    print("done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
