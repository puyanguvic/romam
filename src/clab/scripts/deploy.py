#!/usr/bin/env python3
from __future__ import annotations

import argparse
from datetime import datetime, timezone
from pathlib import Path

from common import (
    load_json,
    load_topology_node_names,
    parse_bool_arg,
    parse_output_json,
    resolve_clab_bin,
    resolve_path,
    run_clab_command,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_TOPOLOGY_DATA = REPO_ROOT / "src" / "clab" / "topology-data.json"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Read topology-data.json and deploy containerlab topology."
    )
    parser.add_argument(
        "--topology-data",
        default=str(DEFAULT_TOPOLOGY_DATA),
        help="Path to topology-data.json.",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=None,
        help="Use sudo for clab command. Default from topology-data.json (fallback false).",
    )
    parser.add_argument(
        "--reconfigure",
        action=argparse.BooleanOptionalAction,
        default=None,
        help="Pass --reconfigure to clab deploy. Default true unless overridden in JSON.",
    )
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="Add or override env variable used by deploy. Repeatable.",
    )
    parser.add_argument(
        "--clab-bin",
        default="",
        help="Path/name of clab binary. Default: auto-detect clab/containerlab.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print command without executing.")
    parser.add_argument(
        "--inspect-after",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Run clab inspect after deploy (default: enabled).",
    )
    return parser.parse_args()


def parse_kv(items: list[str]) -> dict[str, str]:
    out: dict[str, str] = {}
    for item in items:
        if "=" not in item:
            raise ValueError(f"Invalid --set value: {item!r}, expected KEY=VALUE")
        key, value = item.split("=", maxsplit=1)
        key = key.strip()
        if not key:
            raise ValueError(f"Invalid --set key in: {item!r}")
        out[key] = value.strip()
    return out


def main() -> int:
    args = parse_args()
    topology_data_path = Path(str(args.topology_data)).expanduser().resolve()
    if not topology_data_path.is_file():
        raise FileNotFoundError(f"topology-data json not found: {topology_data_path}")

    data = load_json(topology_data_path)
    data_dir = topology_data_path.parent
    topology_value = str(data.get("topology_file", "")).strip()
    if not topology_value:
        raise ValueError("topology-data.json missing `topology_file`.")
    topology_file = resolve_path(topology_value, data_dir)
    if not topology_file.is_file():
        raise FileNotFoundError(f"topology yaml not found: {topology_file}")

    lab_name = str(data.get("lab_name", "")).strip()
    if not lab_name:
        lab_name = topology_file.stem.replace(".clab", "")

    env_overrides = {str(k): str(v) for k, v in dict(data.get("env", {}) or {}).items()}
    env_overrides.update(parse_kv(list(args.set)))
    use_sudo = parse_bool_arg(args.sudo, bool(data.get("sudo", False)))
    reconfigure = parse_bool_arg(args.reconfigure, bool(data.get("reconfigure", True)))
    clab_bin = resolve_clab_bin(str(args.clab_bin).strip())

    deploy_cmd = [clab_bin, "deploy", "-t", str(topology_file), "--name", lab_name]
    if reconfigure:
        deploy_cmd.append("--reconfigure")

    print(f"topology-data: {topology_data_path}")
    print(f"lab_name: {lab_name}")
    print(f"topology_file: {topology_file}")
    print(f"sudo: {use_sudo}")
    if env_overrides:
        print(f"env_overrides: {env_overrides}")
    print(f"deploy_cmd: {' '.join(deploy_cmd)}")
    if args.dry_run:
        return 0

    run_clab_command(
        deploy_cmd,
        use_sudo=use_sudo,
        env_overrides=env_overrides,
        check=True,
        capture_output=False,
    )

    if bool(args.inspect_after):
        inspect_proc = run_clab_command(
            [clab_bin, "inspect", "--name", lab_name, "--format", "json"],
            use_sudo=use_sudo,
            check=False,
            capture_output=True,
        )
        inspect_data = parse_output_json(inspect_proc.stdout or "")
        if isinstance(inspect_data, dict):
            nodes = inspect_data.get("containers", {})
            if isinstance(nodes, dict):
                print(f"inspect_nodes: {len(nodes)}")
            else:
                node_names = load_topology_node_names(topology_file)
                print(f"inspect_nodes_fallback: {len(node_names)}")
        else:
            node_names = load_topology_node_names(topology_file)
            print(f"inspect_nodes_fallback: {len(node_names)}")

    print(f"deployed_at: {datetime.now(timezone.utc).isoformat()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

