#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json

from topology.topology import Topology


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate topology JSON")
    parser.add_argument(
        "--type",
        required=True,
        choices=["line", "ring", "star", "fullmesh", "spineleaf", "grid", "er", "ba"],
    )
    parser.add_argument("--n", type=int, default=20, help="Node count for ring/er/ba")
    parser.add_argument("--n-spines", type=int, default=2)
    parser.add_argument("--n-leaves", type=int, default=4)
    parser.add_argument("--rows", type=int, default=4)
    parser.add_argument("--cols", type=int, default=4)
    parser.add_argument("--p", type=float, default=0.05)
    parser.add_argument("--m", type=int, default=2)
    parser.add_argument("--metric", type=float, default=1.0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    if args.type == "line":
        topo = Topology.line(args.n, args.metric)
    elif args.type == "ring":
        topo = Topology.ring(args.n, args.metric)
    elif args.type == "star":
        topo = Topology.star(args.n, args.metric)
    elif args.type == "fullmesh":
        topo = Topology.fullmesh(args.n, args.metric)
    elif args.type == "spineleaf":
        topo = Topology.spineleaf(args.n_spines, args.n_leaves, args.metric)
    elif args.type == "grid":
        topo = Topology.grid(args.rows, args.cols, args.metric)
    elif args.type == "er":
        topo = Topology.er(args.n, args.p, args.metric, args.seed)
    else:
        topo = Topology.ba(args.n, args.m, args.metric, args.seed)

    payload = {
        "type": args.type,
        "nodes": topo.nodes(),
        "edges": [e.__dict__ for e in topo.edge_list()],
    }
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)


if __name__ == "__main__":
    main()
