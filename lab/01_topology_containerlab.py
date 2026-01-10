#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.topology.containerlab import generate


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--name", default=None)
    ap.add_argument("--image", default="romam:clab")
    ap.add_argument("--route-table-pref", type=int, default=1000)
    args = ap.parse_args()

    res = generate(
        spec_path=args.spec,
        out_dir=args.out,
        name=args.name,
        image=args.image,
        route_table_pref=args.route_table_pref,
    )
    print(res["topo_path"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
