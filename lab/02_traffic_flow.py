#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.topology.spec import load_spec, validate_and_normalize
from romam_lab.traffic.containerlab import start_tcp_flow, start_udp_flow


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--topo", required=True, help="containerlab topo file path")
    ap.add_argument("--spec", required=True, help="same spec used for topology")
    ap.add_argument("--proto", choices=["udp", "tcp"], required=True)
    ap.add_argument("--server", required=True, help="node name in spec")
    ap.add_argument("--client", required=True, help="node name in spec")
    ap.add_argument("--port", type=int, default=5001)
    ap.add_argument("--duration", type=int, default=10)
    ap.add_argument("--rate-mbps", type=float, default=10.0, help="udp only")
    ap.add_argument("--size", type=int, default=1200)
    args = ap.parse_args()

    spec = validate_and_normalize(load_spec(args.spec))
    node_by_name = spec["node_by_name"]
    if args.server not in node_by_name or args.client not in node_by_name:
        raise SystemExit("server/client not found in spec.nodes")

    server_ip = node_by_name[args.server]["router_id"]

    if args.proto == "udp":
        start_udp_flow(
            topo=args.topo,
            server_node=args.server,
            server_ip=server_ip,
            client_node=args.client,
            port=args.port,
            duration_s=args.duration,
            rate_mbps=args.rate_mbps,
            payload_bytes=args.size,
        )
        return 0

    start_tcp_flow(
        topo=args.topo,
        server_node=args.server,
        server_ip=server_ip,
        client_node=args.client,
        port=args.port,
        duration_s=args.duration,
        payload_bytes=args.size,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
