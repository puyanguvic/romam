#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import time
from dataclasses import dataclass


@dataclass
class ThroughputStats:
    packets: int = 0
    bytes_total: int = 0

    def add(self, payload_len: int) -> None:
        self.packets += 1
        self.bytes_total += payload_len


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Small sender/sink app for router containers (UDP + TCP)."
    )
    sub = parser.add_subparsers(dest="role", required=True)

    sink = sub.add_parser("sink", help="Run as traffic sink (receiver).")
    sink.add_argument("--proto", choices=["udp", "tcp"], default="udp")
    sink.add_argument("--bind", default="0.0.0.0", help="Bind address.")
    sink.add_argument("--port", type=int, required=True, help="Bind port.")
    sink.add_argument(
        "--buffer-size",
        type=int,
        default=65535,
        help="Receive buffer size per recv call.",
    )
    sink.add_argument(
        "--report-interval-s",
        type=float,
        default=1.0,
        help="Progress report interval (seconds).",
    )
    sink.add_argument("--listen-backlog", type=int, default=8, help="TCP listen backlog.")
    sink.add_argument(
        "--rcvbuf-bytes",
        type=int,
        default=0,
        help="Socket SO_RCVBUF; 0 means keep system default.",
    )

    send = sub.add_parser("send", help="Run as traffic sender.")
    send.add_argument("--proto", choices=["udp", "tcp"], default="udp")
    send.add_argument("--target", required=True, help="Destination address.")
    send.add_argument("--port", type=int, required=True, help="Destination port.")
    send.add_argument(
        "--packet-size",
        type=int,
        default=256,
        help="Payload size per send in bytes.",
    )
    send.add_argument(
        "--count",
        type=int,
        default=1,
        help="Number of payloads to send. 0 means unlimited (until duration or Ctrl-C).",
    )
    send.add_argument(
        "--duration-s",
        type=float,
        default=0.0,
        help="Max run duration. 0 means no time limit.",
    )
    send.add_argument(
        "--pps",
        type=float,
        default=0.0,
        help="Send pacing in packets/chunks per second. 0 means no pacing.",
    )
    send.add_argument(
        "--report-interval-s",
        type=float,
        default=1.0,
        help="Progress report interval (seconds).",
    )
    send.add_argument(
        "--connect-timeout-s",
        type=float,
        default=3.0,
        help="TCP connect timeout (seconds).",
    )
    send.add_argument(
        "--sndbuf-bytes",
        type=int,
        default=0,
        help="Socket SO_SNDBUF; 0 means keep system default.",
    )
    send.add_argument(
        "--tcp-nodelay",
        action="store_true",
        help="Enable TCP_NODELAY for TCP sender.",
    )
    return parser


def _validate_args(args: argparse.Namespace) -> None:
    if int(args.port) <= 0 or int(args.port) > 65535:
        raise ValueError(f"invalid port: {args.port}")
    if float(args.report_interval_s) <= 0:
        raise ValueError("--report-interval-s must be > 0")
    if args.role == "sink":
        if int(args.buffer_size) <= 0:
            raise ValueError("--buffer-size must be > 0")
        if int(args.listen_backlog) <= 0:
            raise ValueError("--listen-backlog must be > 0")
        if int(args.rcvbuf_bytes) < 0:
            raise ValueError("--rcvbuf-bytes must be >= 0")
        return

    if int(args.packet_size) <= 0:
        raise ValueError("--packet-size must be > 0")
    if int(args.count) < 0:
        raise ValueError("--count must be >= 0")
    if float(args.duration_s) < 0:
        raise ValueError("--duration-s must be >= 0")
    if float(args.pps) < 0:
        raise ValueError("--pps must be >= 0")
    if float(args.connect_timeout_s) <= 0:
        raise ValueError("--connect-timeout-s must be > 0")
    if int(args.sndbuf_bytes) < 0:
        raise ValueError("--sndbuf-bytes must be >= 0")


def _payload(packet_size: int, seq: int) -> bytes:
    header = f"seq={seq} ts={time.time_ns()} ".encode("ascii")
    if len(header) >= packet_size:
        return header[:packet_size]
    return header + (b"x" * (packet_size - len(header)))


def _report(prefix: str, start_ts: float, last_ts: float, stats: ThroughputStats) -> float:
    now = time.monotonic()
    elapsed = max(now - start_ts, 1e-9)
    interval = max(now - last_ts, 1e-9)
    pps = stats.packets / elapsed
    bps = stats.bytes_total * 8.0 / elapsed
    print(
        f"{prefix} elapsed={elapsed:.3f}s packets={stats.packets} bytes={stats.bytes_total} "
        f"avg_pps={pps:.2f} avg_mbps={bps / 1_000_000.0:.3f} interval={interval:.3f}s",
        flush=True,
    )
    return now


def run_udp_sink(args: argparse.Namespace) -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if int(args.rcvbuf_bytes) > 0:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, int(args.rcvbuf_bytes))
    sock.bind((str(args.bind), int(args.port)))
    sock.settimeout(float(args.report_interval_s))
    stats = ThroughputStats()
    start_ts = time.monotonic()
    last_report_ts = start_ts
    print(f"udp sink listening on {args.bind}:{args.port}", flush=True)
    try:
        while True:
            try:
                payload, _addr = sock.recvfrom(int(args.buffer_size))
                stats.add(len(payload))
            except TimeoutError:
                pass
            now = time.monotonic()
            if now - last_report_ts >= float(args.report_interval_s):
                last_report_ts = _report("udp sink", start_ts, last_report_ts, stats)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
    _report("udp sink final", start_ts, last_report_ts, stats)
    return 0


def run_tcp_sink(args: argparse.Namespace) -> int:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if int(args.rcvbuf_bytes) > 0:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, int(args.rcvbuf_bytes))
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((str(args.bind), int(args.port)))
    listener.listen(int(args.listen_backlog))
    listener.settimeout(float(args.report_interval_s))
    stats = ThroughputStats()
    start_ts = time.monotonic()
    last_report_ts = start_ts
    print(f"tcp sink listening on {args.bind}:{args.port}", flush=True)
    try:
        while True:
            try:
                conn, peer = listener.accept()
            except TimeoutError:
                conn = None
            if conn is not None:
                print(f"tcp sink accepted peer={peer[0]}:{peer[1]}", flush=True)
                with conn:
                    conn.settimeout(float(args.report_interval_s))
                    while True:
                        try:
                            payload = conn.recv(int(args.buffer_size))
                        except TimeoutError:
                            payload = None
                        if payload is None:
                            now = time.monotonic()
                            if now - last_report_ts >= float(args.report_interval_s):
                                last_report_ts = _report(
                                    "tcp sink", start_ts, last_report_ts, stats
                                )
                            continue
                        if payload:
                            stats.add(len(payload))
                        else:
                            break
                print(f"tcp sink peer closed peer={peer[0]}:{peer[1]}", flush=True)
            now = time.monotonic()
            if now - last_report_ts >= float(args.report_interval_s):
                last_report_ts = _report("tcp sink", start_ts, last_report_ts, stats)
    except KeyboardInterrupt:
        pass
    finally:
        listener.close()
    _report("tcp sink final", start_ts, last_report_ts, stats)
    return 0


def _send_with_pacing(
    sock: socket.socket,
    proto: str,
    target: tuple[str, int],
    packet_size: int,
    count: int,
    duration_s: float,
    pps: float,
    report_interval_s: float,
) -> int:
    stats = ThroughputStats()
    start_ts = time.monotonic()
    last_report_ts = start_ts

    while True:
        now = time.monotonic()
        if count > 0 and stats.packets >= count:
            break
        if duration_s > 0 and now - start_ts >= duration_s:
            break

        if pps > 0:
            target_send_ts = start_ts + (stats.packets / pps)
            if target_send_ts > now:
                time.sleep(min(target_send_ts - now, 0.2))
                continue

        payload = _payload(packet_size, stats.packets + 1)
        if proto == "udp":
            sock.sendto(payload, target)
        else:
            sock.sendall(payload)
        stats.add(len(payload))

        now = time.monotonic()
        if now - last_report_ts >= report_interval_s:
            last_report_ts = _report(f"{proto} send", start_ts, last_report_ts, stats)

    _report(f"{proto} send final", start_ts, last_report_ts, stats)
    return 0


def run_udp_send(args: argparse.Namespace) -> int:
    target = (str(args.target), int(args.port))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if int(args.sndbuf_bytes) > 0:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, int(args.sndbuf_bytes))
    try:
        print(
            f"udp send target={args.target}:{args.port} packet_size={args.packet_size} "
            f"count={args.count} duration_s={args.duration_s} pps={args.pps}",
            flush=True,
        )
        return _send_with_pacing(
            sock=sock,
            proto="udp",
            target=target,
            packet_size=int(args.packet_size),
            count=int(args.count),
            duration_s=float(args.duration_s),
            pps=float(args.pps),
            report_interval_s=float(args.report_interval_s),
        )
    finally:
        sock.close()


def run_tcp_send(args: argparse.Namespace) -> int:
    target = (str(args.target), int(args.port))
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(float(args.connect_timeout_s))
    if bool(args.tcp_nodelay):
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    if int(args.sndbuf_bytes) > 0:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, int(args.sndbuf_bytes))
    try:
        sock.connect(target)
        sock.settimeout(None)
        print(
            f"tcp send connected target={args.target}:{args.port} packet_size={args.packet_size} "
            f"count={args.count} duration_s={args.duration_s} pps={args.pps}",
            flush=True,
        )
        return _send_with_pacing(
            sock=sock,
            proto="tcp",
            target=target,
            packet_size=int(args.packet_size),
            count=int(args.count),
            duration_s=float(args.duration_s),
            pps=float(args.pps),
            report_interval_s=float(args.report_interval_s),
        )
    finally:
        sock.close()


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()
    try:
        _validate_args(args)
    except ValueError as exc:
        parser.error(str(exc))

    if args.role == "sink":
        if args.proto == "udp":
            return run_udp_sink(args)
        return run_tcp_sink(args)

    if args.proto == "udp":
        return run_udp_send(args)
    return run_tcp_send(args)


if __name__ == "__main__":
    raise SystemExit(main())
