#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import random


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate random failure schedule")
    parser.add_argument("--n-events", type=int, default=5)
    parser.add_argument("--max-tick", type=int, default=100)
    parser.add_argument("--n-nodes", type=int, default=20)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    events = []
    for _ in range(args.n_events):
        tick = rng.randint(1, args.max_tick - 1)
        u = rng.randint(0, args.n_nodes - 1)
        v = rng.randint(0, args.n_nodes - 1)
        while v == u:
            v = rng.randint(0, args.n_nodes - 1)
        action = rng.choice(["remove_link", "add_link", "update_metric"])
        event = {"tick": tick, "action": action, "u": u, "v": v}
        if action in {"add_link", "update_metric"}:
            event["metric"] = round(rng.uniform(1.0, 10.0), 2)
        events.append(event)

    events.sort(key=lambda e: e["tick"])
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(events, f, indent=2, sort_keys=True)


if __name__ == "__main__":
    main()
