from __future__ import annotations

import shutil
import subprocess
from dataclasses import dataclass
from typing import List, Optional, Tuple


@dataclass(frozen=True)
class ClabTarget:
    topo: str
    node: str


def _require_containerlab():
    if shutil.which("containerlab") is None:
        raise RuntimeError("missing required tool: containerlab (install it on the host)")


def exec_cmd(*, topo: str, node: str, cmd: str, check: bool = True, capture: bool = True) -> subprocess.CompletedProcess[str]:
    _require_containerlab()
    argv = ["containerlab", "exec", "-t", topo, "--node", node, "--cmd", cmd]
    return subprocess.run(argv, check=check, capture_output=capture, text=True)


def start_background(*, topo: str, node: str, cmd: str, log_path: str = "/tmp/romam-bg.log") -> Tuple[int, str]:
    sh = f"sh -lc 'rm -f {log_path} ; ({cmd}) >{log_path} 2>&1 & echo $!'"
    res = exec_cmd(topo=topo, node=node, cmd=sh, check=True, capture=True)
    pid_s = (res.stdout or "").strip().splitlines()[-1].strip() if (res.stdout or "").strip() else ""
    try:
        pid = int(pid_s)
    except Exception:
        raise RuntimeError(f"failed to start background job on {node}: {res.stdout}\n{res.stderr}")
    return pid, log_path


def stop_background(*, topo: str, node: str, pid: int) -> None:
    exec_cmd(topo=topo, node=node, cmd=f"sh -lc 'kill {int(pid)} 2>/dev/null || true'", check=True, capture=True)


def read_file(*, topo: str, node: str, path: str) -> str:
    res = exec_cmd(topo=topo, node=node, cmd=f"sh -lc 'cat {path} 2>/dev/null || true'", check=True, capture=True)
    return (res.stdout or "").strip()


def start_udp_flow(
    *,
    topo: str,
    server_node: str,
    server_ip: str,
    client_node: str,
    port: int = 5001,
    duration_s: int = 10,
    rate_mbps: float = 10.0,
    payload_bytes: int = 1200,
) -> None:
    srv_cmd = f"romam-traffic --mode server --proto udp --bind 0.0.0.0:{port} --duration {duration_s}"
    cli_cmd = (
        f"romam-traffic --mode client --proto udp --connect {server_ip}:{port} "
        f"--duration {duration_s} --rate-mbps {rate_mbps} --size {payload_bytes}"
    )
    pid, log_path = start_background(topo=topo, node=server_node, cmd=srv_cmd, log_path="/tmp/romam-traffic-server.log")
    try:
        exec_cmd(topo=topo, node=client_node, cmd=f"sh -lc {cli_cmd!r}", check=True, capture=True)
    finally:
        stop_background(topo=topo, node=server_node, pid=pid)
        _ = read_file(topo=topo, node=server_node, path=log_path)


def start_tcp_flow(
    *,
    topo: str,
    server_node: str,
    server_ip: str,
    client_node: str,
    port: int = 5001,
    duration_s: int = 10,
    payload_bytes: int = 1200,
) -> None:
    srv_cmd = f"romam-traffic --mode server --proto tcp --bind 0.0.0.0:{port} --duration {duration_s}"
    cli_cmd = f"romam-traffic --mode client --proto tcp --connect {server_ip}:{port} --duration {duration_s} --size {payload_bytes}"
    pid, log_path = start_background(topo=topo, node=server_node, cmd=srv_cmd, log_path="/tmp/romam-traffic-server.log")
    try:
        exec_cmd(topo=topo, node=client_node, cmd=f"sh -lc {cli_cmd!r}", check=True, capture=True)
    finally:
        stop_background(topo=topo, node=server_node, pid=pid)
        _ = read_file(topo=topo, node=server_node, path=log_path)


def run_many(*, topo: str, jobs: List[str], node: Optional[str] = None) -> List[subprocess.CompletedProcess[str]]:
    _require_containerlab()
    out: List[subprocess.CompletedProcess[str]] = []
    for cmd in jobs:
        if node is None:
            raise ValueError("node is required for run_many() currently")
        out.append(exec_cmd(topo=topo, node=node, cmd=cmd, check=True, capture=True))
    return out
