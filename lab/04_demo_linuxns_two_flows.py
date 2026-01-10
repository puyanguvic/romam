#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.topology.spec import load_spec, validate_and_normalize  # noqa: E402


def _require_root():
    if os.geteuid() != 0:
        raise SystemExit("this demo needs root. try: sudo python3 lab/04_demo_linuxns_two_flows.py ...")


def _ip_netns_list() -> set[str]:
    res = subprocess.run(["ip", "netns", "list"], check=False, capture_output=True, text=True)
    if res.returncode != 0:
        return set()
    out = (res.stdout or "").strip()
    if not out:
        return set()
    names: set[str] = set()
    for line in out.splitlines():
        names.add(line.split()[0])
    return names


def _wait_for_netns(expected: set[str], timeout_s: int) -> None:
    deadline = time.time() + float(timeout_s)
    while time.time() < deadline:
        cur = _ip_netns_list()
        if expected.issubset(cur):
            return
        time.sleep(0.1)
    missing = sorted(expected - _ip_netns_list())
    raise RuntimeError(f"timeout waiting for netns: {missing}")


def _run_in_netns(ns: str, argv: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["ip", "netns", "exec", ns] + argv, check=False, capture_output=True, text=True, cwd=REPO_ROOT)


def _popen_in_netns(ns: str, argv: list[str]) -> subprocess.Popen[str]:
    return subprocess.Popen(
        ["ip", "netns", "exec", ns] + argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=REPO_ROOT,
    )


@dataclass(frozen=True)
class Flow:
    proto: str  # udp|tcp
    server: str
    client: str
    port: int
    duration_s: int
    rate_mbps: float
    size: int


def _start_flow(flow: Flow, *, ns_by_node: dict[str, str], router_id_by_node: dict[str, str], traffic_bin: str) -> None:
    if flow.proto not in ("udp", "tcp"):
        raise ValueError("proto must be udp|tcp")
    server_ip = router_id_by_node[flow.server]

    srv_argv = [
        traffic_bin,
        "--mode",
        "server",
        "--proto",
        flow.proto,
        "--bind",
        f"0.0.0.0:{flow.port}",
        "--duration",
        str(flow.duration_s),
    ]
    cli_argv = [
        traffic_bin,
        "--mode",
        "client",
        "--proto",
        flow.proto,
        "--connect",
        f"{server_ip}:{flow.port}",
        "--duration",
        str(flow.duration_s),
        "--size",
        str(flow.size),
    ]
    if flow.proto == "udp":
        cli_argv += ["--rate-mbps", str(flow.rate_mbps)]

    srv = _popen_in_netns(ns_by_node[flow.server], srv_argv)
    try:
        time.sleep(0.2)
        cli = _run_in_netns(ns_by_node[flow.client], cli_argv)
        srv_out, srv_err = srv.communicate(timeout=flow.duration_s + 3)
    except Exception:
        srv.kill()
        raise

    print(f"=== flow {flow.client} -> {flow.server} ({flow.proto}) ===")
    if cli.stdout:
        print(cli.stdout.strip())
    if cli.stderr:
        print(cli.stderr.strip())
    if cli.returncode != 0:
        print(f"client rc={cli.returncode}")
    if srv_out:
        print(srv_out.strip())
    if srv_err:
        print(srv_err.strip())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", default=os.path.join("standalone", "mininet", "specs", "linear5.toml"))
    ap.add_argument("--prefix", default="romam-")
    ap.add_argument("--romamd", default=os.path.join(".", "build-standalone", "daemon", "romamd"))
    ap.add_argument("--traffic", default=os.path.join(".", "build-standalone", "traffic", "romam-traffic"))
    ap.add_argument("--out", default=None, help="output directory for configs/logs (default: temp)")
    ap.add_argument("--converge-wait", type=int, default=4, help="seconds to wait before starting flows")
    ap.add_argument("--duration", type=int, default=10, help="flow duration seconds")
    ap.add_argument("--rate-mbps", type=float, default=10.0, help="udp flow rate")
    ap.add_argument("--size", type=int, default=1200, help="payload bytes")
    args = ap.parse_args()

    _require_root()

    romamd = os.path.abspath(args.romamd)
    traffic = os.path.abspath(args.traffic)
    if not os.path.exists(romamd):
        raise SystemExit(f"romamd not found: {romamd} (build it with cmake --build build-standalone -j)")
    if not os.path.exists(traffic):
        raise SystemExit(f"romam-traffic not found: {traffic} (build it with cmake --build build-standalone -j)")

    spec = validate_and_normalize(load_spec(args.spec))
    node_names = [n["name"] for n in spec["nodes"]]
    router_id_by_node = {n["name"]: n["router_id"] for n in spec["nodes"]}
    ns_by_node = {name: f"{args.prefix}{name}" for name in node_names}

    out_dir = os.path.abspath(args.out or tempfile.mkdtemp(prefix="romam-demo-ns-"))
    os.makedirs(out_dir, exist_ok=True)

    if len(node_names) < 4:
        raise SystemExit("need at least 4 nodes to run two flows (edit the script if you want a smaller topo)")

    # Total run time: leave enough room for setup + flows. We'll interrupt early after flows finish.
    total_run_s = max(60, int(args.converge_wait) + int(args.duration) + 30)

    runner_argv = [
        sys.executable,
        os.path.join(REPO_ROOT, "standalone", "linuxns", "run.py"),
        "--spec",
        os.path.abspath(args.spec),
        "--romamd",
        romamd,
        "--out",
        out_dir,
        "--show-routes",
        "--no-ping",
        "--cleanup",
        "--prefix",
        args.prefix,
        "--duration",
        str(total_run_s),
    ]

    runner = subprocess.Popen(runner_argv, cwd=REPO_ROOT)
    try:
        _wait_for_netns(set(ns_by_node.values()), timeout_s=20)
        time.sleep(max(0, int(args.converge_wait)))

        # Two default flows:
        # - UDP: last -> first
        # - TCP: second_last -> second
        n0, n1, n_2, n_1 = node_names[0], node_names[1], node_names[-2], node_names[-1]
        flow1 = Flow(
            proto="udp",
            server=n0,
            client=n_1,
            port=5001,
            duration_s=int(args.duration),
            rate_mbps=float(args.rate_mbps),
            size=int(args.size),
        )
        flow2 = Flow(
            proto="tcp",
            server=n1,
            client=n_2,
            port=5002,
            duration_s=int(args.duration),
            rate_mbps=0.0,
            size=int(args.size),
        )

        _start_flow(flow1, ns_by_node=ns_by_node, router_id_by_node=router_id_by_node, traffic_bin=traffic)
        _start_flow(flow2, ns_by_node=ns_by_node, router_id_by_node=router_id_by_node, traffic_bin=traffic)

        runner.send_signal(signal.SIGINT)
        rc = runner.wait(timeout=30)
        print(f"logs: {out_dir}")
        return int(rc)
    except subprocess.TimeoutExpired:
        runner.send_signal(signal.SIGINT)
        runner.wait(timeout=10)
        return 1
    finally:
        if runner.poll() is None:
            runner.send_signal(signal.SIGINT)
            try:
                runner.wait(timeout=10)
            except subprocess.TimeoutExpired:
                runner.kill()
                runner.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
