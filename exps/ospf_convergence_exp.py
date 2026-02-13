#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import csv
import ipaddress
import json
import math
import os
import pwd
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean
from typing import Any, Dict, List

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from irp.utils.io import dump_json, ensure_dir, now_tag
from topology.clab_loader import ClabTopology, load_clab_topology

PING_SUMMARY_RE = re.compile(r"(\d+) packets transmitted, (\d+) (?:packets )?received")
PING_RTT_RE = re.compile(r"(?:rtt|round-trip) min/avg/max(?:/mdev|/stddev)? = ([0-9.]+)/([0-9.]+)/")
AnyNetwork = ipaddress.IPv4Network | ipaddress.IPv6Network


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run containerlab OSPF convergence demo experiment and export probe metrics."
    )
    parser.add_argument(
        "--topology-file",
        default="clab_topologies/ring6.clab.yaml",
        help="Source containerlab topology file.",
    )
    parser.add_argument(
        "--repeats",
        type=int,
        default=1,
        help="Number of repeated runs on the same topology file.",
    )
    parser.add_argument(
        "--ping-count",
        type=int,
        default=3,
        help="ICMP packets per link probe. Default: 3.",
    )
    parser.add_argument(
        "--ping-timeout-s",
        type=int,
        default=1,
        help="Ping timeout in seconds. Default: 1.",
    )
    parser.add_argument(
        "--startup-wait-s",
        type=float,
        default=0.8,
        help="Wait time after lab deploy. Default: 0.8.",
    )
    parser.add_argument(
        "--link-delay-ms",
        type=float,
        default=1.0,
        help="One-way per-link delay configured via tc/netem. Default: 1ms.",
    )
    parser.add_argument(
        "--node-image",
        default="ghcr.io/srl-labs/network-multitool:latest",
        help="Container image for linux nodes.",
    )
    parser.add_argument(
        "--lab-name-prefix",
        default="ospf-convergence-clab",
        help="Prefix for generated containerlab lab names.",
    )
    parser.add_argument(
        "--mgmt-network-name",
        default="",
        help="Containerlab management Docker network name. Auto-generated if empty.",
    )
    parser.add_argument(
        "--mgmt-ipv4-subnet",
        default="",
        help="Containerlab management IPv4 subnet (CIDR). Auto-selected if empty.",
    )
    parser.add_argument(
        "--mgmt-ipv6-subnet",
        default="",
        help="Containerlab management IPv6 subnet (CIDR). Auto-selected if empty.",
    )
    parser.add_argument(
        "--mgmt-external-access",
        action="store_true",
        help="Enable containerlab management external access rules (disabled by default).",
    )
    parser.add_argument(
        "--sudo",
        action="store_true",
        help="Prefix containerlab/docker commands with sudo.",
    )
    parser.add_argument(
        "--keep-lab",
        action="store_true",
        help="Do not destroy the lab after each run (for debugging).",
    )
    parser.add_argument(
        "--run-output-dir",
        default="results/runs/ospf_convergence_containerlab",
        help="Directory for raw run artifacts.",
    )
    parser.add_argument(
        "--result-prefix",
        default="",
        help="Output prefix for aggregated result files.",
    )
    return parser.parse_args()


def resolve_topology_file_arg(path_text: str) -> Path:
    path = Path(path_text).expanduser()
    if not path.is_absolute():
        path = (REPO_ROOT / path).resolve()
    if not path.is_file():
        raise FileNotFoundError(f"Topology file does not exist: {path}")
    return path


def parse_ping_output(output: str) -> tuple[int, int, float | None]:
    tx = 0
    rx = 0
    avg_rtt = None

    summary_match = PING_SUMMARY_RE.search(output)
    if summary_match:
        tx = int(summary_match.group(1))
        rx = int(summary_match.group(2))

    rtt_match = PING_RTT_RE.search(output)
    if rtt_match:
        avg_rtt = float(rtt_match.group(2))

    return tx, rx, avg_rtt


def pctl(values: List[float], percentile: float) -> float | None:
    if not values:
        return None
    xs = sorted(values)
    idx = int(math.ceil(len(xs) * percentile)) - 1
    idx = max(0, min(idx, len(xs) - 1))
    return xs[idx]


def sanitize_label(value: str, max_len: int = 40) -> str:
    sanitized = re.sub(r"[^a-z0-9-]", "-", value.lower()).strip("-")
    if not sanitized:
        sanitized = "clab"
    return sanitized[:max_len]


def with_sudo(cmd: List[str], use_sudo: bool) -> List[str]:
    if use_sudo:
        return ["sudo", *cmd]
    return cmd


def run_cmd(
    cmd: List[str],
    capture_output: bool = True,
    check: bool = True,
    env: Dict[str, str] | None = None,
) -> str:
    try:
        result = subprocess.run(
            cmd,
            check=check,
            text=True,
            stdout=subprocess.PIPE if capture_output else None,
            stderr=subprocess.STDOUT if capture_output else None,
            env=env,
        )
    except subprocess.CalledProcessError as exc:
        output = (exc.stdout or "").strip()
        raise RuntimeError(
            f"Command failed ({exc.returncode}): {' '.join(cmd)}\n{output}"
        ) from exc
    return (result.stdout or "").strip()


def containerlab_search_candidates() -> List[Path]:
    candidates: List[Path] = []
    seen: set[str] = set()

    def add_candidate(path_str: str | None) -> None:
        if not path_str:
            return
        path = Path(path_str).expanduser()
        key = str(path)
        if key in seen:
            return
        seen.add(key)
        candidates.append(path)

    add_candidate(shutil.which("containerlab"))
    add_candidate(os.environ.get("CONTAINERLAB_BIN"))
    add_candidate(str(Path.home() / ".local/bin" / "containerlab"))

    sudo_user = os.environ.get("SUDO_USER")
    if sudo_user:
        try:
            sudo_home = Path(pwd.getpwnam(sudo_user).pw_dir)
            add_candidate(str(sudo_home / ".local/bin" / "containerlab"))
        except KeyError:
            pass

    add_candidate("/usr/local/bin/containerlab")
    add_candidate("/usr/bin/containerlab")
    add_candidate("/bin/containerlab")
    return candidates


def local_containerlab_path() -> str | None:
    for candidate in containerlab_search_candidates():
        try:
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return str(candidate.resolve(strict=False))
        except OSError:
            continue
    return None


def ensure_prerequisites(use_sudo: bool) -> None:
    if shutil.which("docker") is None:
        raise RuntimeError("Missing required command(s): docker")
    if local_containerlab_path() is None:
        search_hint = ", ".join(str(p) for p in containerlab_search_candidates())
        raise RuntimeError(
            "Missing required command: containerlab.\n"
            f"Searched: {search_hint}\n"
            "Install containerlab on host and ensure it is available in PATH.\n"
            "If using sudo, prefer 'make ... EXP_USE_SUDO=1' instead of 'sudo make ...'."
        )
    if use_sudo and shutil.which("sudo") is None:
        raise RuntimeError("Missing required command: sudo")
    try:
        run_cmd(with_sudo(["docker", "info"], use_sudo))
    except RuntimeError as exc:
        if not use_sudo and shutil.which("sudo") is not None:
            raise RuntimeError(
                "Docker daemon is unreachable for current user. "
                "Please rerun with --sudo (or EXP_USE_SUDO=1 in make)."
            ) from exc
        raise


def clab_command(
    action: str,
    topology_path: Path,
    lab_name: str,
) -> List[str]:
    action_flag = "--reconfigure" if action == "deploy" else "--cleanup"
    local_bin = local_containerlab_path()
    if local_bin is None:
        raise RuntimeError(
            "Missing required command: containerlab.\n"
            "Install containerlab on host and rerun."
        )
    return [local_bin, action, "-t", str(topology_path), "--name", lab_name, action_flag]


def run_clab_action(
    args: argparse.Namespace,
    action: str,
    topology_path: Path,
    lab_name: str,
    env_overrides: Dict[str, str],
) -> None:
    base_cmd = clab_command(action, topology_path, lab_name)
    if args.sudo:
        export_vars = [f"{key}={value}" for key, value in env_overrides.items()]
        cmd = ["sudo", "env", *export_vars, *base_cmd]
        cmd_env = None
    else:
        cmd = base_cmd
        cmd_env = {**os.environ, **env_overrides}
    try:
        run_cmd(cmd, env=cmd_env)
    except RuntimeError as exc:
        raise RuntimeError(
            f"containerlab action '{action}' failed.\n"
            f"Command: {' '.join(cmd)}\n"
            f"Error:\n{exc}"
        ) from exc


def format_delay(delay_ms: float) -> str:
    text = f"{delay_ms:.3f}".rstrip("0").rstrip(".")
    return f"{text}ms"


def list_docker_network_subnets(use_sudo: bool) -> tuple[List[AnyNetwork], List[AnyNetwork]]:
    network_ids_raw = run_cmd(with_sudo(["docker", "network", "ls", "-q"], use_sudo))
    network_ids = [line.strip() for line in network_ids_raw.splitlines() if line.strip()]
    if not network_ids:
        return [], []
    details_raw = run_cmd(with_sudo(["docker", "network", "inspect", *network_ids], use_sudo))
    details = json.loads(details_raw)

    ipv4_nets: List[AnyNetwork] = []
    ipv6_nets: List[AnyNetwork] = []
    for item in details:
        ipam = item.get("IPAM", {})
        configs = ipam.get("Config") or []
        for cfg in configs:
            subnet = cfg.get("Subnet")
            if not subnet:
                continue
            try:
                net = ipaddress.ip_network(subnet, strict=False)
            except ValueError:
                continue
            if net.version == 4:
                ipv4_nets.append(net)
            else:
                ipv6_nets.append(net)
    return ipv4_nets, ipv6_nets


def choose_mgmt_subnets(args: argparse.Namespace, lab_name: str) -> tuple[str, str, str]:
    if args.mgmt_ipv4_subnet and args.mgmt_ipv6_subnet:
        mgmt_name = args.mgmt_network_name or f"{lab_name}-mgmt"
        return mgmt_name, args.mgmt_ipv4_subnet, args.mgmt_ipv6_subnet

    used_v4, used_v6 = list_docker_network_subnets(args.sudo)

    selected_v4 = args.mgmt_ipv4_subnet
    if not selected_v4:
        for second in range(200, 255):
            for third in range(0, 256):
                candidate = ipaddress.ip_network(f"10.{second}.{third}.0/24")
                if not any(candidate.overlaps(existing) for existing in used_v4):
                    selected_v4 = str(candidate)
                    break
            if selected_v4:
                break
        if not selected_v4:
            raise RuntimeError("Unable to find a free IPv4 management subnet for containerlab.")

    selected_v6 = args.mgmt_ipv6_subnet
    if not selected_v6:
        for a in range(0x20, 0x100):
            for b in range(0x20, 0x100):
                candidate = ipaddress.ip_network(f"fd00:{a:02x}:{b:02x}::/64")
                if not any(candidate.overlaps(existing) for existing in used_v6):
                    selected_v6 = str(candidate)
                    break
            if selected_v6:
                break
        if not selected_v6:
            raise RuntimeError("Unable to find a free IPv6 management subnet for containerlab.")

    mgmt_name = args.mgmt_network_name or f"{lab_name}-mgmt"
    return mgmt_name, selected_v4, selected_v6


def build_probe_plan(source: ClabTopology) -> List[Dict[str, Any]]:
    probes: List[Dict[str, Any]] = []
    for link in source.links:
        probes.append(
            {
                "src_node": link.left_node,
                "dst_node": link.right_node,
                "src_iface": link.left_iface,
                "dst_iface": link.right_iface,
                "src_ip": link.left_ip,
                "dst_ip": link.right_ip,
                "target_ip": link.right_ip,
            }
        )
    return probes


def build_clab_env(
    node_image: str,
    lab_name: str,
    mgmt_network_name: str,
    mgmt_ipv4_subnet: str,
    mgmt_ipv6_subnet: str,
    mgmt_external_access: bool,
) -> Dict[str, str]:
    return {
        "CLAB_NODE_IMAGE": str(node_image),
        "CLAB_LAB_NAME": lab_name,
        "CLAB_MGMT_NETWORK": mgmt_network_name,
        "CLAB_MGMT_IPV4_SUBNET": mgmt_ipv4_subnet,
        "CLAB_MGMT_IPV6_SUBNET": mgmt_ipv6_subnet,
        "CLAB_MGMT_EXTERNAL_ACCESS": "true" if mgmt_external_access else "false",
    }


def clab_container_name(lab_name: str, node_name: str) -> str:
    return f"clab-{lab_name}-{node_name}"


def configure_interfaces(
    lab_name: str,
    probes: List[Dict[str, Any]],
    delay_ms: float,
    use_sudo: bool,
) -> None:
    tc_delay = format_delay(delay_ms)
    for probe in probes:
        src_node = str(probe["src_node"])
        dst_node = str(probe["dst_node"])
        src_container = clab_container_name(lab_name, src_node)
        dst_container = clab_container_name(lab_name, dst_node)
        src_iface = str(probe["src_iface"])
        dst_iface = str(probe["dst_iface"])
        src_cidr = f"{probe['src_ip']}/30"
        dst_cidr = f"{probe['dst_ip']}/30"

        run_cmd(
            with_sudo(
                ["docker", "exec", src_container, "ip", "link", "set", src_iface, "up"],
                use_sudo,
            )
        )
        run_cmd(
            with_sudo(
                ["docker", "exec", dst_container, "ip", "link", "set", dst_iface, "up"],
                use_sudo,
            )
        )
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    src_container,
                    "ip",
                    "addr",
                    "replace",
                    src_cidr,
                    "dev",
                    src_iface,
                ],
                use_sudo,
            )
        )
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    dst_container,
                    "ip",
                    "addr",
                    "replace",
                    dst_cidr,
                    "dev",
                    dst_iface,
                ],
                use_sudo,
            )
        )
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    src_container,
                    "tc",
                    "qdisc",
                    "replace",
                    "dev",
                    src_iface,
                    "root",
                    "netem",
                    "delay",
                    tc_delay,
                ],
                use_sudo,
            )
        )
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    dst_container,
                    "tc",
                    "qdisc",
                    "replace",
                    "dev",
                    dst_iface,
                    "root",
                    "netem",
                    "delay",
                    tc_delay,
                ],
                use_sudo,
            )
        )


def run_once(
    args: argparse.Namespace,
    source_topology: ClabTopology,
    run_idx: int,
) -> Dict[str, Any]:
    ensure_prerequisites(args.sudo)

    node_count = len(source_topology.node_names)
    run_id = f"ospf_convergence_containerlab_n{node_count}_r{run_idx}_{now_tag()}"
    topology_tag = str(source_topology.source_path.name.removesuffix(".clab.yaml"))
    lab_name = sanitize_label(
        f"{args.lab_name_prefix}-{topology_tag}-r{run_idx}-{now_tag()}",
        48,
    )
    mgmt_network_name, mgmt_ipv4_subnet, mgmt_ipv6_subnet = choose_mgmt_subnets(args, lab_name)
    probe_plan = build_probe_plan(source_topology)
    clab_env = build_clab_env(
        node_image=str(args.node_image),
        lab_name=lab_name,
        mgmt_network_name=mgmt_network_name,
        mgmt_ipv4_subnet=mgmt_ipv4_subnet,
        mgmt_ipv6_subnet=mgmt_ipv6_subnet,
        mgmt_external_access=bool(args.mgmt_external_access),
    )
    run_dir = ensure_dir(REPO_ROOT / args.run_output_dir)

    successful_probes = 0
    failed_probes = 0
    rtts: List[float] = []
    probe_details: List[Dict[str, Any]] = []

    lab_deployed = False
    try:
        run_clab_action(
            args,
            "deploy",
            source_topology.source_path,
            lab_name,
            clab_env,
        )
        lab_deployed = True
        time.sleep(max(0.0, float(args.startup_wait_s)))

        configure_interfaces(lab_name, probe_plan, float(args.link_delay_ms), args.sudo)

        for probe in probe_plan:
            src_node = str(probe["src_node"])
            src_container = clab_container_name(lab_name, src_node)
            target_ip = str(probe["target_ip"])
            output = run_cmd(
                with_sudo(
                    [
                        "docker",
                        "exec",
                        src_container,
                        "ping",
                        "-n",
                        "-c",
                        str(int(args.ping_count)),
                        "-W",
                        str(int(args.ping_timeout_s)),
                        target_ip,
                    ],
                    args.sudo,
                ),
                check=False,
            )

            tx, rx, avg_rtt = parse_ping_output(output)
            ok = tx > 0 and rx == tx
            if ok:
                successful_probes += 1
            else:
                failed_probes += 1
            if avg_rtt is not None:
                rtts.append(avg_rtt)

            probe_details.append(
                {
                    "src_node": probe["src_node"],
                    "dst_node": probe["dst_node"],
                    "target_ip": target_ip,
                    "packets_tx": tx,
                    "packets_rx": rx,
                    "avg_rtt_ms": avg_rtt,
                    "success": ok,
                }
            )
    finally:
        if lab_deployed and not args.keep_lab:
            try:
                run_clab_action(
                    args,
                    "destroy",
                    source_topology.source_path,
                    lab_name,
                    clab_env,
                )
            except RuntimeError:
                pass

    run_payload = {
        "run_id": run_id,
        "name": f"ospf_convergence_containerlab_n{node_count}_r{run_idx}",
        "run_idx": run_idx,
        "n_nodes": node_count,
        "topology": topology_tag,
        "edge_count": len(probe_plan),
        "link_delay_ms": float(args.link_delay_ms),
        "node_image": args.node_image,
        "clab_mode": "host-binary",
        "lab_name": lab_name,
        "mgmt_network_name": mgmt_network_name,
        "mgmt_ipv4_subnet": mgmt_ipv4_subnet,
        "mgmt_ipv6_subnet": mgmt_ipv6_subnet,
        "mgmt_external_access": bool(args.mgmt_external_access),
        "source_topology_file": str(source_topology.source_path),
        "topology_file": str(source_topology.source_path),
        "successful_probes": successful_probes,
        "failed_probes": failed_probes,
        "probe_success_ratio": round(successful_probes / max(1, len(probe_plan)), 6),
        "converged": failed_probes == 0,
        "avg_rtt_ms": round(mean(rtts), 3) if rtts else None,
        "p95_rtt_ms": round(float(pctl(rtts, 0.95)), 3) if rtts else None,
        "probes": probe_details,
    }
    dump_json(run_dir / f"{run_id}.json", run_payload)
    return {k: v for k, v in run_payload.items() if k != "probes"}


def summarize(metrics_rows: List[Dict[str, Any]], args: argparse.Namespace) -> Dict[str, Any]:
    return {
        "experiment": "ospf_convergence_exp",
        "created_at_utc": datetime.now(timezone.utc).isoformat(),
        "n_nodes": metrics_rows[0]["n_nodes"] if metrics_rows else None,
        "repeats": args.repeats,
        "topology": metrics_rows[0]["topology"] if metrics_rows else None,
        "source_topology_file": metrics_rows[0]["source_topology_file"] if metrics_rows else None,
        "link_delay_ms": float(args.link_delay_ms),
        "node_image": args.node_image,
        "clab_mode": "host-binary",
        "mgmt_network_name": metrics_rows[0]["mgmt_network_name"] if metrics_rows else None,
        "mgmt_ipv4_subnet": metrics_rows[0]["mgmt_ipv4_subnet"] if metrics_rows else None,
        "mgmt_ipv6_subnet": metrics_rows[0]["mgmt_ipv6_subnet"] if metrics_rows else None,
        "mgmt_external_access": metrics_rows[0]["mgmt_external_access"] if metrics_rows else None,
        "avg_probe_success_ratio": round(
            mean([float(row["probe_success_ratio"]) for row in metrics_rows]),
            6,
        ),
        "convergence_success_rate": round(
            mean([1.0 if bool(row["converged"]) else 0.0 for row in metrics_rows]),
            6,
        ),
        "avg_rtt_ms": round(
            mean(
                [
                    float(row["avg_rtt_ms"])
                    for row in metrics_rows
                    if row["avg_rtt_ms"] is not None
                ]
            ),
            3,
        )
        if any(row["avg_rtt_ms"] is not None for row in metrics_rows)
        else None,
        "avg_p95_rtt_ms": round(
            mean(
                [
                    float(row["p95_rtt_ms"])
                    for row in metrics_rows
                    if row["p95_rtt_ms"] is not None
                ]
            ),
            3,
        )
        if any(row["p95_rtt_ms"] is not None for row in metrics_rows)
        else None,
        "runs": metrics_rows,
    }


def save_outputs(summary: Dict[str, Any], prefix: str) -> tuple[Path, Path]:
    prefix_path = REPO_ROOT / prefix
    ensure_dir(prefix_path.parent)

    json_path = prefix_path.with_suffix(".json")
    with json_path.open("w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2, sort_keys=True)

    csv_path = prefix_path.with_suffix(".csv")
    fields = [
        "run_id",
        "name",
        "run_idx",
        "n_nodes",
        "topology",
        "source_topology_file",
        "edge_count",
        "link_delay_ms",
        "node_image",
        "clab_mode",
        "lab_name",
        "mgmt_network_name",
        "mgmt_ipv4_subnet",
        "mgmt_ipv6_subnet",
        "mgmt_external_access",
        "successful_probes",
        "failed_probes",
        "probe_success_ratio",
        "converged",
        "avg_rtt_ms",
        "p95_rtt_ms",
    ]
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in summary["runs"]:
            writer.writerow({k: row.get(k) for k in fields})

    return json_path, csv_path


def main() -> None:
    args = parse_args()
    source_topology_file = resolve_topology_file_arg(str(args.topology_file))
    source_topology = load_clab_topology(source_topology_file)

    rows = [run_once(args, source_topology, i) for i in range(args.repeats)]
    summary = summarize(rows, args)
    prefix = (
        args.result_prefix
        or "results/tables/ospf_convergence_containerlab_"
        + source_topology.source_path.name.removesuffix(".clab.yaml")
    )
    json_path, csv_path = save_outputs(summary, prefix)

    print("=== OSPF Convergence Containerlab Experiment ===")
    print(f"n_nodes: {summary['n_nodes']}")
    print(f"repeats: {summary['repeats']}")
    print(f"topology: {summary['topology']}")
    print(f"source_topology_file: {summary['source_topology_file']}")
    print(f"link_delay_ms: {summary['link_delay_ms']}")
    print(f"avg_probe_success_ratio: {summary['avg_probe_success_ratio']}")
    print(f"convergence_success_rate: {summary['convergence_success_rate']}")
    print(f"avg_rtt_ms: {summary['avg_rtt_ms']}")
    print(f"avg_p95_rtt_ms: {summary['avg_p95_rtt_ms']}")
    print(f"json: {json_path}")
    print(f"csv: {csv_path}")


if __name__ == "__main__":
    main()
