#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.observe.containerlab import collect_routes_json
from romam_lab.topology.spec import load_spec, validate_and_normalize


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--topo", required=True, help="containerlab topo file path")
    ap.add_argument("--spec", required=True, help="same spec used for topology")
    ap.add_argument("--out", required=True, help="output directory")
    ap.add_argument("--table", type=int, default=100)
    args = ap.parse_args()

    spec = validate_and_normalize(load_spec(args.spec))
    os.makedirs(args.out, exist_ok=True)

    for n in spec["nodes"]:
        name = n["name"]
        routes = collect_routes_json(topo=args.topo, node=name, table=args.table)
        path = os.path.join(args.out, f"routes.{name}.table{args.table}.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(routes, f, ensure_ascii=False, indent=2)
        print(path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
