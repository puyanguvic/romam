#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

from topology.clab_loader import load_clab_topology


def main() -> None:
    parser = argparse.ArgumentParser(description="Export normalized topology JSON from .clab.yaml")
    parser.add_argument(
        "--topology-file",
        required=True,
        help="Input containerlab topology file (.clab.yaml).",
    )
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    source = load_clab_topology(Path(args.topology_file))
    payload = {
        "name": source.raw.get("name"),
        "source_topology_file": str(source.source_path),
        "nodes": source.node_names,
        "links": [
            {
                "left_node": link.left_node,
                "left_iface": link.left_iface,
                "left_ip": link.left_ip,
                "right_node": link.right_node,
                "right_iface": link.right_iface,
                "right_ip": link.right_ip,
                "cost": link.cost,
            }
            for link in source.links
        ],
    }
    with Path(args.out).open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)


if __name__ == "__main__":
    main()
