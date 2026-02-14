#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import shlex
import subprocess
import sys
import time
from dataclasses import asdict, dataclass

FINAL_STATS_RE = re.compile(
    r"(?P<mode>udp send final|tcp send final)\s+"
    r"elapsed=(?P<elapsed>[0-9.]+)s\s+"
    r"packets=(?P<packets>\d+)\s+"
    r"bytes=(?P<bytes>\d+)\s+"
    r"avg_pps=(?P<avg_pps>[0-9.]+)\s+"
    r"avg_mbps=(?P<avg_mbps>[0-9.]+)"
)
SINK_STATS_RE = re.compile(
    r"(?P<mode>udp sink(?: final)?|tcp sink(?: final)?)\s+"
    r"elapsed=(?P<elapsed>[0-9.]+)s\s+"
    r"packets=(?P<packets>\d+)\s+"
    r"bytes=(?P<bytes>\d+)\s+"
    r"avg_pps=(?P<avg_pps>[0-9.]+)\s+"
    r"avg_mbps=(?P<avg_mbps>[0-9.]+)"
)


@dataclass(frozen=True)
class ProbeStats:
    mode: str
    elapsed_s: float
    packets: int
    bytes_total: int
    avg_pps: float
    avg_mbps: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="One-shot sender/sink traffic probe inside a routerd lab."
    )
    parser.add_argument("--lab-name", required=True, help="Containerlab lab name.")
    parser.add_argument("--src-node", required=True, help="Source node name (sender).")
    parser.add_argument("--dst-node", required=True, help="Destination node name (sink).")
    parser.add_argument("--dst-ip", required=True, help="Destination IP reachable from src node.")
    parser.add_argument("--proto", choices=["udp", "tcp"], default="udp")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--packet-size", type=int, default=256)
    parser.add_argument("--count", type=int, default=1000)
    parser.add_argument("--duration-s", type=float, default=0.0)
    parser.add_argument("--pps", type=float, default=200.0)
    parser.add_argument("--report-interval-s", type=float, default=1.0)
    parser.add_argument("--warmup-s", type=float, default=0.7)
    parser.add_argument(
        "--sender-timeout-s",
        type=float,
        default=120.0,
        help="Max wall-clock time for sender execution.",
    )
    parser.add_argument(
        "--sink-log-file",
        default="/tmp/traffic_probe_sink.log",
        help="Sink log path in destination container.",
    )
    parser.add_argument(
        "--output-json",
        default="",
        help="Optional path to dump probe report as JSON.",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for docker exec (default: enabled).",
    )
    return parser.parse_args()


def _with_sudo(cmd: list[str], use_sudo: bool) -> list[str]:
    return ["sudo", *cmd] if use_sudo else cmd


def _run(
    cmd: list[str],
    check: bool = True,
    timeout_s: float | None = None,
) -> subprocess.CompletedProcess:
    proc = subprocess.run(
        cmd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout_s,
    )
    if check and proc.returncode != 0:
        output = (proc.stdout or "").strip()
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{output}")
    return proc


def _container_name(lab_name: str, node_name: str) -> str:
    return f"clab-{lab_name}-{node_name}"


def _parse_final_stats(text: str) -> ProbeStats | None:
    for line in reversed(text.splitlines()):
        match = FINAL_STATS_RE.search(line.strip())
        if not match:
            continue
        return ProbeStats(
            mode=str(match.group("mode")),
            elapsed_s=float(match.group("elapsed")),
            packets=int(match.group("packets")),
            bytes_total=int(match.group("bytes")),
            avg_pps=float(match.group("avg_pps")),
            avg_mbps=float(match.group("avg_mbps")),
        )
    return None


def _parse_sink_stats(text: str) -> ProbeStats | None:
    for line in reversed(text.splitlines()):
        match = SINK_STATS_RE.search(line.strip())
        if not match:
            continue
        return ProbeStats(
            mode=str(match.group("mode")),
            elapsed_s=float(match.group("elapsed")),
            packets=int(match.group("packets")),
            bytes_total=int(match.group("bytes")),
            avg_pps=float(match.group("avg_pps")),
            avg_mbps=float(match.group("avg_mbps")),
        )
    return None


def main() -> int:
    args = parse_args()
    src_container = _container_name(str(args.lab_name), str(args.src_node))
    dst_container = _container_name(str(args.lab_name), str(args.dst_node))
    sink_pid_file = "/tmp/traffic_probe_sink.pid"

    sink_inner = (
        "PYTHONPATH=/irp/src nohup python3 -m applications.traffic_app "
        f"sink --proto {args.proto} --bind 0.0.0.0 --port {int(args.port)} "
        f"--report-interval-s {float(args.report_interval_s)} "
        f">{shlex.quote(str(args.sink_log_file))} 2>&1 & "
        f"echo $! >{sink_pid_file} && cat {sink_pid_file}"
    )
    sink_start_cmd = _with_sudo(
        ["docker", "exec", dst_container, "sh", "-lc", sink_inner],
        bool(args.sudo),
    )

    sender_inner = (
        "PYTHONPATH=/irp/src python3 -m applications.traffic_app "
        f"send --proto {args.proto} --target {args.dst_ip} --port {int(args.port)} "
        f"--packet-size {int(args.packet_size)} --count {int(args.count)} "
        f"--duration-s {float(args.duration_s)} --pps {float(args.pps)} "
        f"--report-interval-s {float(args.report_interval_s)}"
    )
    sender_cmd = _with_sudo(
        ["docker", "exec", src_container, "sh", "-lc", sender_inner],
        bool(args.sudo),
    )

    sink_pid = ""
    sender_output = ""
    stop_output = ""
    sender_rc = 0
    try:
        start_proc = _run(sink_start_cmd, check=True)
        sink_pid = (start_proc.stdout or "").strip().splitlines()[-1].strip()
        if not sink_pid.isdigit():
            raise RuntimeError(f"failed to get sink pid, output={start_proc.stdout!r}")

        time.sleep(max(float(args.warmup_s), 0.0))
        send_proc = _run(sender_cmd, check=False, timeout_s=max(float(args.sender_timeout_s), 1.0))
        sender_output = (send_proc.stdout or "").strip()
        sender_rc = int(send_proc.returncode)
    finally:
        if sink_pid:
            stop_inner = (
                f"kill -INT {sink_pid} >/dev/null 2>&1 || true; "
                f"sleep 0.2; "
                f"tail -n 80 {shlex.quote(str(args.sink_log_file))} || true"
            )
            stop_cmd = _with_sudo(
                ["docker", "exec", dst_container, "sh", "-lc", stop_inner],
                bool(args.sudo),
            )
            stop_proc = _run(stop_cmd, check=False)
            stop_output = (stop_proc.stdout or "").strip()

    sender_stats = _parse_final_stats(sender_output)
    sink_stats = _parse_sink_stats(stop_output)
    delivery_ratio = None
    if sender_stats is not None and sender_stats.packets > 0 and sink_stats is not None:
        delivery_ratio = float(sink_stats.packets) / float(sender_stats.packets)
    report = {
        "ok": (
            sender_rc == 0
            and sender_stats is not None
            and sink_stats is not None
            and sink_stats.packets > 0
        ),
        "proto": args.proto,
        "lab_name": args.lab_name,
        "src_node": args.src_node,
        "dst_node": args.dst_node,
        "dst_ip": args.dst_ip,
        "port": int(args.port),
        "packet_size": int(args.packet_size),
        "count": int(args.count),
        "duration_s": float(args.duration_s),
        "pps": float(args.pps),
        "sender_rc": sender_rc,
        "sender_stats": asdict(sender_stats) if sender_stats else None,
        "sink_stats": asdict(sink_stats) if sink_stats else None,
        "delivery_ratio": delivery_ratio,
        "sender_output": sender_output,
        "sink_tail": stop_output,
    }

    print("=== traffic probe report ===")
    print(json.dumps(report, ensure_ascii=True, indent=2))

    if args.output_json:
        with open(args.output_json, "w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=True, indent=2)
            f.write("\n")

    return 0 if report["ok"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.TimeoutExpired as exc:
        print(f"sender timeout after {exc.timeout}s", file=sys.stderr)
        raise SystemExit(124)
