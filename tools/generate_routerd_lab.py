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
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from clab.labgen import LabGenParams, generate_routerd_lab
from irp.utils.io import now_tag
from tools.common import ADAPTIVE_PROTOCOLS, STOCHASTIC_ADAPTIVE_PROTOCOLS, SUPPORTED_PROTOCOLS

PROFILE_TO_TOPOLOGY_FILE: dict[str, str] = {
    "line3": "src/clab/topologies/line3.clab.yaml",
    "line5": "src/clab/topologies/line5.clab.yaml",
    "ring6": "src/clab/topologies/ring6.clab.yaml",
    "abilene": "src/clab/topologies/abilene.clab.yaml",
    "geant": "src/clab/topologies/geant.clab.yaml",
    "uunet": "src/clab/topologies/uunet.clab.yaml",
    "cernet": "src/clab/topologies/cernet.clab.yaml",
    "star6": "src/clab/topologies/star6.clab.yaml",
    "fullmesh4": "src/clab/topologies/fullmesh4.clab.yaml",
    "spineleaf2x4": "src/clab/topologies/spineleaf2x4.clab.yaml",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate routerd-enabled containerlab topology and per-node configs."
    )
    parser.add_argument(
        "--protocol",
        choices=list(SUPPORTED_PROTOCOLS),
        default="ospf",
    )
    parser.add_argument(
        "--routing-alpha",
        type=float,
        default=1.0,
        help="Legacy routing alpha parameter (kept for backward-compatible config parsing).",
    )
    parser.add_argument(
        "--routing-beta",
        type=float,
        default=2.0,
        help="Legacy routing beta parameter (kept for backward-compatible config parsing).",
    )
    parser.add_argument(
        "--profile",
        choices=sorted(PROFILE_TO_TOPOLOGY_FILE.keys()),
        default="ring6",
        help="Built-in topology profile under src/clab/topologies/.",
    )
    parser.add_argument(
        "--topology-file",
        default="",
        help="Path to source .clab.yaml file. Overrides --profile when provided.",
    )
    parser.add_argument(
        "--node-image",
        default="romam/network-multitool-routerd:latest",
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
    parser.add_argument("--ecmp-hash-seed", type=int, default=1)
    parser.add_argument("--topk-k-paths", type=int, default=3)
    parser.add_argument("--topk-explore-probability", type=float, default=0.3)
    parser.add_argument("--topk-selection-hold-time-s", type=float, default=3.0)
    parser.add_argument("--topk-rng-seed", type=int, default=1)
    parser.add_argument(
        "--ddr-k-paths",
        "--dgr-k-paths",
        "--octopus-k-paths",
        dest="ddr_k_paths",
        type=int,
        default=3,
    )
    parser.add_argument(
        "--ddr-deadline-ms",
        "--dgr-deadline-ms",
        "--octopus-deadline-ms",
        dest="ddr_deadline_ms",
        type=float,
        default=100.0,
    )
    parser.add_argument(
        "--ddr-flow-size-bytes",
        "--dgr-flow-size-bytes",
        "--octopus-flow-size-bytes",
        dest="ddr_flow_size_bytes",
        type=float,
        default=64000.0,
    )
    parser.add_argument(
        "--ddr-link-bandwidth-bps",
        "--dgr-link-bandwidth-bps",
        "--octopus-link-bandwidth-bps",
        dest="ddr_link_bandwidth_bps",
        type=float,
        default=9600000.0,
    )
    parser.add_argument(
        "--ddr-queue-sample-interval",
        "--dgr-queue-sample-interval",
        "--octopus-queue-sample-interval",
        dest="ddr_queue_sample_interval",
        type=float,
        default=1.0,
    )
    parser.add_argument(
        "--ddr-queue-levels",
        "--dgr-queue-levels",
        "--octopus-queue-levels",
        dest="ddr_queue_levels",
        type=int,
        default=4,
    )
    parser.add_argument(
        "--ddr-pressure-threshold",
        "--dgr-pressure-threshold",
        "--octopus-pressure-threshold",
        dest="ddr_pressure_threshold",
        type=int,
        default=2,
    )
    parser.add_argument(
        "--ddr-queue-level-scale-ms",
        "--dgr-queue-level-scale-ms",
        "--octopus-queue-level-scale-ms",
        dest="ddr_queue_level_scale_ms",
        type=float,
        default=8.0,
    )
    parser.add_argument(
        "--ddr-randomized-selection",
        "--dgr-randomized-selection",
        "--octopus-randomized-selection",
        dest="ddr_randomized_selection",
        action=argparse.BooleanOptionalAction,
        default=None,
    )
    parser.add_argument(
        "--ddr-rng-seed",
        "--dgr-rng-seed",
        "--octopus-rng-seed",
        dest="ddr_rng_seed",
        type=int,
        default=1,
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
    parser.add_argument(
        "--forwarding-enabled",
        action="store_true",
        help="Enable Linux FIB programming from protocol routes in routerd.",
    )
    parser.add_argument(
        "--forwarding-dry-run",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="When forwarding is enabled, keep FIB operations in dry-run mode (default: enabled).",
    )
    parser.add_argument(
        "--mgmt-http-enabled",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Enable routingd HTTP management API (default: enabled).",
    )
    parser.add_argument(
        "--mgmt-http-bind",
        default="0.0.0.0",
        help="Bind address for routingd HTTP management API.",
    )
    parser.add_argument(
        "--mgmt-http-port-base",
        type=int,
        default=18000,
        help="Base HTTP management port; actual node port = base + router_id.",
    )
    parser.add_argument(
        "--mgmt-grpc-enabled",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Enable routingd gRPC management listener (default: enabled).",
    )
    parser.add_argument(
        "--mgmt-grpc-bind",
        default="0.0.0.0",
        help="Bind address for routingd gRPC management listener.",
    )
    parser.add_argument(
        "--mgmt-grpc-port-base",
        type=int,
        default=19000,
        help="Base gRPC management port; actual node port = base + router_id.",
    )
    parser.add_argument(
        "--qdisc-enabled",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Enable runtime qdisc controller in routingd configs.",
    )
    parser.add_argument(
        "--qdisc-dry-run",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="When qdisc is enabled, do not execute tc commands (default: enabled).",
    )
    parser.add_argument(
        "--qdisc-default-kind",
        default="",
        help=(
            "Default root qdisc kind for all interfaces "
            "(e.g. fifo/pfifo_fast/ecn/red/fq_codel/prio/drr/netem/tbf)."
        ),
    )
    parser.add_argument(
        "--qdisc-default-handle",
        default="",
        help="Optional default root qdisc handle (e.g. 1:).",
    )
    parser.add_argument(
        "--qdisc-default-parent",
        default="",
        help="Optional default parent selector (reserved for future classful extension).",
    )
    parser.add_argument(
        "--qdisc-default-params-json",
        default="",
        help='JSON object for default qdisc params, e.g. {"limit":"10240","bands":"3"}.',
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Use sudo when probing Docker networks for free mgmt subnets.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    qdisc_default_params = parse_qdisc_params_json(str(args.qdisc_default_params_json))
    randomized_selection = (
        bool(args.ddr_randomized_selection)
        if args.ddr_randomized_selection is not None
        else str(args.protocol) in STOCHASTIC_ADAPTIVE_PROTOCOLS
    )
    source_topology_file = resolve_source_topology_file(args)
    topology_label = source_topology_file.name.removesuffix(".clab.yaml")
    lab_name = args.lab_name or f"routerd-{args.protocol}-{topology_label}-{now_tag().lower()}"
    output_dir = REPO_ROOT / args.output_dir / lab_name
    mgmt_network_name, mgmt_ipv4_subnet, mgmt_ipv6_subnet = resolve_mgmt_settings(args, lab_name)
    params = LabGenParams(
        protocol=str(args.protocol),
        routing_alpha=float(args.routing_alpha),
        routing_beta=float(args.routing_beta),
        topology_file=source_topology_file,
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
        ecmp_hash_seed=int(args.ecmp_hash_seed),
        topk_k_paths=int(args.topk_k_paths),
        topk_explore_probability=float(args.topk_explore_probability),
        topk_selection_hold_time_s=float(args.topk_selection_hold_time_s),
        topk_rng_seed=int(args.topk_rng_seed),
        output_dir=output_dir,
        lab_name=lab_name,
        log_level=str(args.log_level),
        mgmt_network_name=mgmt_network_name,
        mgmt_ipv4_subnet=mgmt_ipv4_subnet,
        mgmt_ipv6_subnet=mgmt_ipv6_subnet,
        mgmt_external_access=bool(args.mgmt_external_access),
        forwarding_enabled=bool(args.forwarding_enabled),
        forwarding_dry_run=bool(args.forwarding_dry_run),
        mgmt_http_enabled=bool(args.mgmt_http_enabled),
        mgmt_http_bind=str(args.mgmt_http_bind),
        mgmt_http_port_base=int(args.mgmt_http_port_base),
        mgmt_grpc_enabled=bool(args.mgmt_grpc_enabled),
        mgmt_grpc_bind=str(args.mgmt_grpc_bind),
        mgmt_grpc_port_base=int(args.mgmt_grpc_port_base),
        ddr_k_paths=int(args.ddr_k_paths),
        ddr_deadline_ms=float(args.ddr_deadline_ms),
        ddr_flow_size_bytes=float(args.ddr_flow_size_bytes),
        ddr_link_bandwidth_bps=float(args.ddr_link_bandwidth_bps),
        ddr_queue_sample_interval=float(args.ddr_queue_sample_interval),
        ddr_queue_levels=int(args.ddr_queue_levels),
        ddr_pressure_threshold=int(args.ddr_pressure_threshold),
        ddr_queue_level_scale_ms=float(args.ddr_queue_level_scale_ms),
        ddr_randomize_route_selection=bool(randomized_selection),
        ddr_rng_seed=int(args.ddr_rng_seed),
        qdisc_enabled=bool(args.qdisc_enabled),
        qdisc_dry_run=bool(args.qdisc_dry_run),
        qdisc_default_kind=str(args.qdisc_default_kind).strip(),
        qdisc_default_handle=str(args.qdisc_default_handle).strip(),
        qdisc_default_parent=str(args.qdisc_default_parent).strip(),
        qdisc_default_params=qdisc_default_params,
    )
    result = generate_routerd_lab(params)
    clab_bin = shutil.which("containerlab") or "containerlab"
    print("Generated routerd lab assets")
    print(f"lab_name: {result['lab_name']}")
    if not args.topology_file:
        print(f"profile: {args.profile}")
    print(f"protocol: {args.protocol}")
    if str(args.protocol) == "ecmp":
        print(f"ecmp_hash_seed: {int(args.ecmp_hash_seed)}")
    if str(args.protocol) == "topk":
        print(f"topk_k_paths: {int(args.topk_k_paths)}")
        print(f"topk_explore_probability: {float(args.topk_explore_probability)}")
        print(f"topk_selection_hold_time_s: {float(args.topk_selection_hold_time_s)}")
        print(f"topk_rng_seed: {int(args.topk_rng_seed)}")
    if str(args.protocol) in ADAPTIVE_PROTOCOLS:
        print(f"ddr_k_paths: {int(args.ddr_k_paths)}")
        print(f"ddr_deadline_ms: {float(args.ddr_deadline_ms)}")
        print(f"ddr_flow_size_bytes: {float(args.ddr_flow_size_bytes)}")
        print(f"ddr_link_bandwidth_bps: {float(args.ddr_link_bandwidth_bps)}")
        print(f"ddr_queue_sample_interval: {float(args.ddr_queue_sample_interval)}")
        print(f"ddr_queue_levels: {int(args.ddr_queue_levels)}")
        print(f"ddr_pressure_threshold: {int(args.ddr_pressure_threshold)}")
        print(f"ddr_queue_level_scale_ms: {float(args.ddr_queue_level_scale_ms)}")
        print(f"ddr_randomized_selection: {bool(randomized_selection)}")
        print(f"ddr_rng_seed: {int(args.ddr_rng_seed)}")
    print(f"source_topology_file: {source_topology_file}")
    print(f"topology_file: {result['topology_file']}")
    print(f"configs_dir: {result['configs_dir']}")
    print(f"deploy_env_file: {result['deploy_env_file']}")
    print(f"mgmt_network: {mgmt_network_name}")
    print(f"mgmt_ipv4_subnet: {mgmt_ipv4_subnet}")
    print(f"mgmt_ipv6_subnet: {mgmt_ipv6_subnet}")
    print(f"forwarding_enabled: {bool(args.forwarding_enabled)}")
    print(f"forwarding_dry_run: {bool(args.forwarding_dry_run)}")
    print(f"mgmt_http_enabled: {bool(args.mgmt_http_enabled)}")
    print(f"mgmt_http_bind: {args.mgmt_http_bind}")
    print(f"mgmt_http_port_base: {int(args.mgmt_http_port_base)}")
    print(f"mgmt_grpc_enabled: {bool(args.mgmt_grpc_enabled)}")
    print(f"mgmt_grpc_bind: {args.mgmt_grpc_bind}")
    print(f"mgmt_grpc_port_base: {int(args.mgmt_grpc_port_base)}")
    print(f"qdisc_enabled: {bool(args.qdisc_enabled)}")
    print(f"qdisc_dry_run: {bool(args.qdisc_dry_run)}")
    print(f"qdisc_default_kind: {str(args.qdisc_default_kind).strip()}")
    print(f"qdisc_default_handle: {str(args.qdisc_default_handle).strip()}")
    print(f"qdisc_default_parent: {str(args.qdisc_default_parent).strip()}")
    print(f"qdisc_default_params: {qdisc_default_params}")
    print()
    print(
        "Deploy:  "
        f"sudo env $(cat {result['deploy_env_file']} | xargs) "
        f"{clab_bin} deploy -t {result['topology_file']} --name {result['lab_name']} --reconfigure"
    )
    print(
        "Destroy: "
        f"sudo env $(cat {result['deploy_env_file']} | xargs) "
        f"{clab_bin} destroy -t {result['topology_file']} --name {result['lab_name']} --cleanup"
    )
    return 0


def resolve_source_topology_file(args: argparse.Namespace) -> Path:
    if args.topology_file:
        path = Path(str(args.topology_file)).expanduser()
        if not path.is_absolute():
            path = (REPO_ROOT / path).resolve()
    else:
        path = (REPO_ROOT / PROFILE_TO_TOPOLOGY_FILE[str(args.profile)]).resolve()
    if not path.is_file():
        raise FileNotFoundError(f"Topology file does not exist: {path}")
    return path


def parse_qdisc_params_json(raw: str) -> dict[str, str]:
    text = str(raw).strip()
    if not text:
        return {}
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid --qdisc-default-params-json: {exc}") from exc
    if not isinstance(parsed, dict):
        raise ValueError("--qdisc-default-params-json must be a JSON object")
    return {str(k): str(v) for k, v in parsed.items()}


def resolve_mgmt_settings(args: argparse.Namespace, lab_name: str) -> tuple[str, str, str]:
    name = str(args.mgmt_network_name or f"{lab_name}-mgmt")
    ipv4 = str(args.mgmt_ipv4_subnet).strip()
    ipv6 = str(args.mgmt_ipv6_subnet).strip()
    if ipv4 and ipv6:
        return name, ipv4, ipv6

    used_v4, used_v6 = list_docker_network_subnets(use_sudo=bool(args.sudo))
    selected_v4 = ipv4 or first_free_v4(used_v4)
    selected_v6 = ipv6 or first_free_v6(used_v6)
    if selected_v4 and selected_v6:
        return name, selected_v4, selected_v6

    token = hashlib.sha256(lab_name.encode("utf-8")).hexdigest()
    fallback_octet = int(token[:2], 16)
    fallback_v4 = f"10.250.{fallback_octet}.0/24"
    fallback_v6 = f"fd00:fa:{fallback_octet:02x}::/64"
    return name, selected_v4 or fallback_v4, selected_v6 or fallback_v6


def with_sudo(cmd: list[str], use_sudo: bool) -> list[str]:
    return ["sudo", *cmd] if use_sudo else cmd


def list_docker_network_subnets(use_sudo: bool) -> tuple[
    List[ipaddress.IPv4Network],
    List[ipaddress.IPv6Network],
]:
    ids_proc = subprocess.run(
        with_sudo(["docker", "network", "ls", "-q"], use_sudo),
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
        with_sudo(["docker", "network", "inspect", *network_ids], use_sudo),
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
