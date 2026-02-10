from __future__ import annotations

import hashlib
import json
from typing import Dict, List, Optional


def hash_routes(route_tables: Dict[int, Dict[int, List[int]]]) -> str:
    normalized: dict[str, dict[str, list[int]]] = {}
    for node, routes in sorted(route_tables.items()):
        normalized[str(node)] = {}
        for dst, hops in sorted(routes.items()):
            normalized[str(node)][str(dst)] = sorted(int(h) for h in hops)
    payload = json.dumps(normalized, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


class ConvergenceTracker:
    def __init__(self, stable_window: int = 5) -> None:
        self.stable_window = stable_window
        self._last_hash: Optional[str] = None
        self._same_count = 0
        self.converged_tick: Optional[int] = None

    def observe(self, tick: int, route_tables: Dict[int, Dict[int, List[int]]]) -> bool:
        current = hash_routes(route_tables)
        if current == self._last_hash:
            self._same_count += 1
        else:
            self._same_count = 1
            self._last_hash = current
        if self.converged_tick is None and self._same_count >= self.stable_window:
            self.converged_tick = tick
            return True
        return False
