#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from irp.utils.io import now_tag
from topology.clab_loader import ClabTopology, load_clab_topology

KNOWN_KEYS = ("topology_file", "configs_dir", "deploy_env_file", "lab_name")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Line topology experiment: deploy OSPF lab, send UDP from first node "
            "to last node, collect probe report, optionally destroy lab."
        )
    )
    parser.add_argument(
        "--profile",
        default="line5",
        choices=["line5"],
        help="Built-in topology profile. Default: line5.",
    )
    parser.add_argument(
        "--topology-file",
        default="",
        help="Optional topology file path. Overrides --profile when provided.",
    )
    parser.add_argument("--port", type=int, default=9000, help="UDP sink/listen port.")
    parser.add_argument("--packet-size", type=int, default=512)
    parser.add_argument("--count", type=int, default=5000)
    parser.add_argument("--duration-s", type=float, default=0.0)
    parser.add_argument("--pps", type=float, default=1000.0)
    parser.add_argument("--report-interval-s", type=float, default=1.0)
    parser.add_argument("--warmup-s", type=float, default=0.7)
    parser.add_argument("--sender-timeout-s", type=float, default=120.0)
    parser.add_argument(
        "--lab-check-min-routes",
        type=int,
        default=0,
        help="Forwarded to run_routerd_lab --check-min-routes (default: 0).",
    )
    parser.add_argument(
        "--lab-check-max-wait-s",
        type=float,
        default=20.0,
        help="Forwarded to run_routerd_lab --check-max-wait-s (default: 20).",
    )
    parser.add_argument(
        "--lab-check-poll-interval-s",
        type=float,
        default=1.0,
        help="Forwarded to run_routerd_lab --check-poll-interval-s (default: 1).",
    )
    parser.add_argument(
        "--converge-tail-lines",
        type=int,
        default=120,
        help="Tail lines used by explicit convergence check before sending.",
    )
    parser.add_argument(
        "--converge-max-wait-s",
        type=float,
        default=60.0,
        help="Max wait seconds for explicit convergence check before sending.",
    )
    parser.add_argument(
        "--converge-poll-interval-s",
        type=float,
        default=1.0,
        help="Polling interval for explicit convergence check before sending.",
    )
    parser.add_argument(
        "--converge-min-routes",
        type=int,
        default=-1,
        help="Required route count before send (-1 means n_nodes-1).",
    )
    parser.add_argument(
        "--allow-unconverged-send",
        action="store_true",
        help="Continue to send UDP even if explicit convergence check fails.",
    )
    parser.add_argument(
        "--output-json",
        default="",
        help="Optional report output path. Default: results/runs/line_ospf_udp_<ts>.json",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for docker/containerlab operations (default: enabled).",
    )
    parser.add_argument(
        "--keep-lab",
        action="store_true",
        help="Keep lab after experiment (skip destroy).",
    )
    return parser.parse_args()


def run_cmd(
    cmd: list[str],
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
        raise RuntimeError(
            f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{(proc.stdout or '').strip()}"
        )
    return proc


def parse_run_routerd_lab_output(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in text.splitlines():
        stripped = line.strip()
        for key in KNOWN_KEYS:
            prefix = f"{key}:"
            if stripped.startswith(prefix):
                out[key] = stripped[len(prefix) :].strip()
    missing = [key for key in KNOWN_KEYS if key not in out or not out[key]]
    if missing:
        raise RuntimeError(
            "Failed to parse run_routerd_lab output, missing keys: "
            + ", ".join(sorted(missing))
        )
    return out


def load_env_file(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=" not in stripped:
            continue
        key, value = stripped.split("=", maxsplit=1)
        out[key.strip()] = value.strip()
    return out


def _node_degree(topo: ClabTopology) -> dict[str, int]:
    out = {name: 0 for name in topo.node_names}
    for link in topo.links:
        out[link.left_node] = out.get(link.left_node, 0) + 1
        out[link.right_node] = out.get(link.right_node, 0) + 1
    return out


def choose_end_nodes(topo: ClabTopology) -> tuple[str, str]:
    if len(topo.node_names) < 2:
        raise RuntimeError("Topology must contain at least 2 nodes.")
    degree = _node_degree(topo)
    first = str(topo.node_names[0])
    last = str(topo.node_names[-1])
    if first != last and degree.get(first) == 1 and degree.get(last) == 1:
        return first, last

    ends = [name for name in topo.node_names if degree.get(name) == 1]
    if len(ends) != 2:
        raise RuntimeError(
            "Topology is not line-like (cannot determine two endpoints): "
            f"found {len(ends)} endpoint candidates."
        )
    return str(ends[0]), str(ends[1])


def choose_node_data_ip(topo: ClabTopology, node_name: str) -> str:
    candidates: list[str] = []
    for link in topo.links:
        if link.left_node == node_name and link.left_ip:
            candidates.append(str(link.left_ip))
        if link.right_node == node_name and link.right_ip:
            candidates.append(str(link.right_ip))
    if not candidates:
        raise RuntimeError(f"No data-plane IP found for node={node_name}.")
    return candidates[0]


def run_containerlab_destroy(
    topology_file: Path,
    lab_name: str,
    deploy_env: dict[str, str],
    use_sudo: bool,
) -> None:
    clab_bin = shutil.which("containerlab")
    if clab_bin is None:
        raise RuntimeError("containerlab not found in PATH.")
    destroy_cmd = [
        clab_bin,
        "destroy",
        "-t",
        str(topology_file),
        "--name",
        str(lab_name),
        "--cleanup",
    ]
    if use_sudo:
        exports = [f"{k}={v}" for k, v in deploy_env.items()]
        run_cmd(["sudo", "env", *exports, *destroy_cmd], check=False, capture_output=False)
    else:
        run_cmd(
            destroy_cmd,
            check=False,
            capture_output=False,
            env={**os.environ, **deploy_env},
        )


def main() -> int:
    args = parse_args()

    runlab_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "run_routerd_lab.py"),
        "--protocol",
        "ospf",
        "--keep-lab",
    ]
    if args.topology_file:
        runlab_cmd.extend(["--topology-file", str(args.topology_file)])
    else:
        runlab_cmd.extend(["--profile", str(args.profile)])
    runlab_cmd.extend(["--forwarding-enabled", "--no-forwarding-dry-run"])
    runlab_cmd.extend(
        [
            "--check-min-routes",
            str(int(args.lab_check_min_routes)),
            "--check-max-wait-s",
            str(float(args.lab_check_max_wait_s)),
            "--check-poll-interval-s",
            str(float(args.lab_check_poll_interval_s)),
        ]
    )
    runlab_cmd.append("--sudo" if bool(args.sudo) else "--no-sudo")

    print(f"[1/4] Deploy line OSPF lab: {' '.join(runlab_cmd)}")
    runlab_proc = run_cmd(runlab_cmd, check=False, capture_output=True)
    runlab_output = (runlab_proc.stdout or "").strip()
    if runlab_output:
        print(runlab_output)
    parsed = parse_run_routerd_lab_output(runlab_output)
    if runlab_proc.returncode != 0:
        print(
            "[warn] run_routerd_lab returned non-zero, but lab metadata was parsed. "
            "Continue with UDP probe.",
            file=sys.stderr,
        )
    topology_file = Path(parsed["topology_file"]).expanduser().resolve()
    lab_name = parsed["lab_name"]
    deploy_env_file = Path(parsed["deploy_env_file"]).expanduser().resolve()
    deploy_env = load_env_file(deploy_env_file)
    deploy_env["CLAB_LAB_NAME"] = lab_name

    topo = load_clab_topology(topology_file)
    src_node, dst_node = choose_end_nodes(topo)
    dst_ip = choose_node_data_ip(topo, dst_node)
    target_min_routes = int(args.converge_min_routes)
    if target_min_routes < 0:
        target_min_routes = max(len(topo.node_names) - 1, 0)
    check_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "check_routerd_lab.py"),
        "--topology-file",
        str(topology_file),
        "--lab-name",
        str(lab_name),
        "--config-dir",
        parsed["configs_dir"],
        "--expect-protocol",
        "ospf",
        "--tail-lines",
        str(int(args.converge_tail_lines)),
        "--max-wait-s",
        str(float(args.converge_max_wait_s)),
        "--poll-interval-s",
        str(float(args.converge_poll_interval_s)),
        "--min-routes",
        str(target_min_routes),
    ]
    if bool(args.sudo):
        check_cmd.append("--sudo")
    print(
        "[2/4] Wait for convergence: "
        f"min_routes={target_min_routes} max_wait_s={args.converge_max_wait_s}"
    )
    converge_proc = run_cmd(check_cmd, check=False, capture_output=True)
    converge_output = (converge_proc.stdout or "").strip()
    if converge_output:
        print(converge_output)
    if converge_proc.returncode != 0:
        if not bool(args.allow_unconverged_send):
            raise RuntimeError(
                "Convergence check failed. "
                "Use --allow-unconverged-send to continue anyway."
            )
        print(
            "[warn] Convergence check failed, continue because --allow-unconverged-send is set.",
            file=sys.stderr,
        )
    output_json = str(args.output_json).strip()
    if not output_json:
        output_json = str(
            REPO_ROOT / "results" / "runs" / f"line_ospf_udp_{now_tag().lower()}.json"
        )

    probe_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "run_traffic_probe.py"),
        "--lab-name",
        lab_name,
        "--src-node",
        src_node,
        "--dst-node",
        dst_node,
        "--dst-ip",
        dst_ip,
        "--proto",
        "udp",
        "--port",
        str(int(args.port)),
        "--packet-size",
        str(int(args.packet_size)),
        "--count",
        str(int(args.count)),
        "--duration-s",
        str(float(args.duration_s)),
        "--pps",
        str(float(args.pps)),
        "--report-interval-s",
        str(float(args.report_interval_s)),
        "--warmup-s",
        str(float(args.warmup_s)),
        "--sender-timeout-s",
        str(float(args.sender_timeout_s)),
        "--output-json",
        output_json,
        "--sudo" if bool(args.sudo) else "--no-sudo",
    ]

    probe_rc = 1
    try:
        print(
            "[3/4] Run UDP probe: "
            f"src={src_node} dst={dst_node} dst_ip={dst_ip} "
            f"packet_size={args.packet_size} count={args.count} pps={args.pps}"
        )
        probe_proc = run_cmd(probe_cmd, check=False, capture_output=False)
        probe_rc = int(probe_proc.returncode)
        print(f"probe_output_json: {output_json}")
        return probe_rc
    finally:
        if bool(args.keep_lab):
            print("[4/4] Keep lab (--keep-lab set).")
            print(f"lab_name: {lab_name}")
            print(f"topology_file: {topology_file}")
            print(f"deploy_env_file: {deploy_env_file}")
        else:
            print("[4/4] Destroy lab.")
            run_containerlab_destroy(
                topology_file=topology_file,
                lab_name=lab_name,
                deploy_env=deploy_env,
                use_sudo=bool(args.sudo),
            )


if __name__ == "__main__":
    raise SystemExit(main())
