from __future__ import annotations

from typing import Dict


def compute_metrics(run: Dict) -> Dict:
    hashes = run.get("route_hashes", [])
    return {
        "run_id": run.get("run_id"),
        "name": run.get("name"),
        "protocol": run.get("protocol"),
        "seed": run.get("seed"),
        "converged_tick": run.get("converged_tick"),
        "route_flaps": run.get("route_flaps", 0),
        "delivered_messages": run.get("delivered_messages", 0),
        "dropped_messages": run.get("dropped_messages", 0),
        "hash_changes": _count_hash_changes(hashes),
    }


def _count_hash_changes(hashes: list[str]) -> int:
    if not hashes:
        return 0
    changes = 0
    prev = hashes[0]
    for h in hashes[1:]:
        if h != prev:
            changes += 1
        prev = h
    return changes
