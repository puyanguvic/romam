#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import hashlib
import ipaddress
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from irp.utils.io import now_tag
from topology.labgen import LabGenParams, generate_routerd_lab

PROFILE_PRESETS: dict[str, dict[str, int | str]] = {
    "line5": {"topology": "line", "n_nodes": 5},
    "ring6": {"topology": "ring", "n_nodes": 6},
    "star6": {"topology": "star", "n_nodes": 6},
    "fullmesh4": {"topology": "fullmesh", "n_nodes": 4},
    "spineleaf2x4": {"topology": "spineleaf", "n_nodes": 6, "n_spines": 2, "n_leaves": 4},
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate containerlab topology and per-router routerd configs."
    )
    parser.add_argument("--protocol", choices=["ospf", "rip"], default="ospf")
    parser.add_argument(
        "--profile",
        choices=sorted(PROFILE_PRESETS.keys()),
        default="",
        help=(
            "Use a built-in topology template. When set, it overrides related topology-size "
            "arguments while keeping --protocol configurable."
        ),
    )
    parser.add_argument(
        "--topology",
        choices=["line", "ring", "star", "fullmesh", "spineleaf", "er", "ba", "grid"],
        default="ring",
    )
    parser.add_argument("--n-nodes", type=int, default=6)
    parser.add_argument("--n-spines", type=int, default=2)
    parser.add_argument("--n-leaves", type=int, default=4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--er-p", type=float, default=0.2)
    parser.add_argument("--ba-m", type=int, default=2)
    parser.add_argument("--rows", type=int, default=3)
    parser.add_argument("--cols", type=int, default=3)
    parser.add_argument(
        "--node-image",
        default="ghcr.io/srl-labs/network-multitool:latest",
        help="Container image used for each linux node.",
    )
    parser.add_argument("--bind-port", type=int, default=5500)
    parser.add_argument("--tick-interval", type=float, default=1.0)
    parser.add_argument("--dead-interval", type=float, default=4.0)
    parser.add_argument("--ospf-hello-interval", type=float, default=1.0)
    parser.add_argument("--ospf-lsa-interval", type=float, default=3.0)
    parser.add_argument("--ospf-lsa-max-age", type=float, default=15.0)
    parser.add_argument("--rip-update-interval", type=float, default=5.0)
    parser.add_argument("--rip-neighbor-timeout", type=float, default=15.0)
    parser.add_argument("--rip-infinity-metric", type=float, default=16.0)
    parser.add_argument(
        "--rip-poison-reverse",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Enable poison reverse in RIP updates (default: enabled).",
    )
    parser.add_argument(
        "--lab-name",
        default="",
        help="Containerlab lab name. Auto-generated if empty.",
    )
    parser.add_argument(
        "--output-dir",
        default="results/runs/routerd_labs",
        help="Output directory for generated lab assets.",
    )
    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="routerd log level inside containers.",
    )
    parser.add_argument(
        "--mgmt-network-name",
        default="",
        help="Containerlab management network name. Default: <lab-name>-mgmt.",
    )
    parser.add_argument(
        "--mgmt-ipv4-subnet",
        default="",
        help="Containerlab management IPv4 subnet in CIDR format.",
    )
    parser.add_argument(
        "--mgmt-ipv6-subnet",
        default="",
        help="Containerlab management IPv6 subnet in CIDR format.",
    )
    parser.add_argument(
        "--mgmt-external-access",
        action="store_true",
        help="Enable containerlab mgmt external access.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    topology_type, n_nodes, n_spines, n_leaves = resolve_topology_shape(args)
    topology_label = str(args.profile) if args.profile else topology_type
    lab_name = args.lab_name or f"routerd-{args.protocol}-{topology_label}-{now_tag().lower()}"
    output_dir = REPO_ROOT / args.output_dir / lab_name
    mgmt_network_name, mgmt_ipv4_subnet, mgmt_ipv6_subnet = resolve_mgmt_settings(args, lab_name)
    params = LabGenParams(
        protocol=str(args.protocol),
        topology_type=topology_type,
        n_nodes=n_nodes,
        seed=int(args.seed),
        er_p=float(args.er_p),
        ba_m=int(args.ba_m),
        rows=int(args.rows),
        cols=int(args.cols),
        node_image=str(args.node_image),
        bind_port=int(args.bind_port),
        tick_interval=float(args.tick_interval),
        dead_interval=float(args.dead_interval),
        ospf_hello_interval=float(args.ospf_hello_interval),
        ospf_lsa_interval=float(args.ospf_lsa_interval),
        ospf_lsa_max_age=float(args.ospf_lsa_max_age),
        rip_update_interval=float(args.rip_update_interval),
        rip_neighbor_timeout=float(args.rip_neighbor_timeout),
        rip_infinity_metric=float(args.rip_infinity_metric),
        rip_poison_reverse=bool(args.rip_poison_reverse),
        output_dir=output_dir,
        lab_name=lab_name,
        log_level=str(args.log_level),
        source_dir=SRC_DIR,
        mgmt_network_name=mgmt_network_name,
        mgmt_ipv4_subnet=mgmt_ipv4_subnet,
        mgmt_ipv6_subnet=mgmt_ipv6_subnet,
        mgmt_external_access=bool(args.mgmt_external_access),
        n_spines=n_spines,
        n_leaves=n_leaves,
    )
    result = generate_routerd_lab(params)
    clab_bin = shutil.which("containerlab") or "containerlab"
    print("Generated routerd lab assets")
    print(f"lab_name: {result['lab_name']}")
    if args.profile:
        print(f"profile: {args.profile}")
    print(f"protocol: {args.protocol}")
    print(f"topology: {topology_type}")
    if topology_type == "spineleaf":
        print(f"shape: n_spines={n_spines}, n_leaves={n_leaves}")
    else:
        print(f"shape: n_nodes={n_nodes}")
    print(f"topology_file: {result['topology_file']}")
    print(f"configs_dir: {result['configs_dir']}")
    print(f"mgmt_network: {mgmt_network_name}")
    print(f"mgmt_ipv4_subnet: {mgmt_ipv4_subnet}")
    print(f"mgmt_ipv6_subnet: {mgmt_ipv6_subnet}")
    print()
    print(f"Deploy:  sudo {clab_bin} deploy -t {result['topology_file']} --reconfigure")
    print(f"Destroy: sudo {clab_bin} destroy -t {result['topology_file']} --cleanup")
    return 0


def resolve_mgmt_settings(args: argparse.Namespace, lab_name: str) -> tuple[str, str, str]:
    name = str(args.mgmt_network_name or f"{lab_name}-mgmt")
    ipv4 = str(args.mgmt_ipv4_subnet).strip()
    ipv6 = str(args.mgmt_ipv6_subnet).strip()
    if ipv4 and ipv6:
        return name, ipv4, ipv6

    used_v4, used_v6 = list_docker_network_subnets()
    selected_v4 = ipv4 or first_free_v4(used_v4)
    selected_v6 = ipv6 or first_free_v6(used_v6)
    if selected_v4 and selected_v6:
        return name, selected_v4, selected_v6

    # deterministic fallback even when docker network inspect is unavailable
    token = hashlib.sha256(lab_name.encode("utf-8")).hexdigest()
    fallback_octet = int(token[:2], 16)
    fallback_v4 = f"10.250.{fallback_octet}.0/24"
    fallback_v6 = f"fd00:fa:{fallback_octet:02x}::/64"
    return name, selected_v4 or fallback_v4, selected_v6 or fallback_v6


def resolve_topology_shape(args: argparse.Namespace) -> tuple[str, int, int, int]:
    topology_type = str(args.topology)
    n_nodes = int(args.n_nodes)
    n_spines = int(args.n_spines)
    n_leaves = int(args.n_leaves)
    if not args.profile:
        return topology_type, n_nodes, n_spines, n_leaves

    preset = PROFILE_PRESETS[str(args.profile)]
    topology_type = str(preset["topology"])
    n_nodes = int(preset.get("n_nodes", n_nodes))
    n_spines = int(preset.get("n_spines", n_spines))
    n_leaves = int(preset.get("n_leaves", n_leaves))
    return topology_type, n_nodes, n_spines, n_leaves


def list_docker_network_subnets() -> tuple[
    List[ipaddress.IPv4Network],
    List[ipaddress.IPv6Network],
]:
    ids_proc = subprocess.run(
        ["docker", "network", "ls", "-q"],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    if ids_proc.returncode != 0:
        return [], []
    network_ids = [line.strip() for line in (ids_proc.stdout or "").splitlines() if line.strip()]
    if not network_ids:
        return [], []

    inspect_proc = subprocess.run(
        ["docker", "network", "inspect", *network_ids],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    if inspect_proc.returncode != 0:
        return [], []

    try:
        details = json.loads(inspect_proc.stdout or "[]")
    except json.JSONDecodeError:
        return [], []

    used_v4: List[ipaddress.IPv4Network] = []
    used_v6: List[ipaddress.IPv6Network] = []
    for item in details:
        for cfg in (item.get("IPAM", {}).get("Config") or []):
            subnet = cfg.get("Subnet")
            if not subnet:
                continue
            try:
                net = ipaddress.ip_network(subnet, strict=False)
            except ValueError:
                continue
            if isinstance(net, ipaddress.IPv4Network):
                used_v4.append(net)
            elif isinstance(net, ipaddress.IPv6Network):
                used_v6.append(net)
    return used_v4, used_v6


def first_free_v4(used: List[ipaddress.IPv4Network]) -> str:
    for second in range(240, 255):
        for third in range(0, 256):
            candidate = ipaddress.ip_network(f"10.{second}.{third}.0/24")
            if not any(candidate.overlaps(existing) for existing in used):
                return str(candidate)
    return ""


def first_free_v6(used: List[ipaddress.IPv6Network]) -> str:
    for a in range(0xF0, 0x100):
        for b in range(0x00, 0x100):
            candidate = ipaddress.ip_network(f"fd00:{a:02x}:{b:02x}::/64")
            if not any(candidate.overlaps(existing) for existing in used):
                return str(candidate)
    return ""


if __name__ == "__main__":
    raise SystemExit(main())
