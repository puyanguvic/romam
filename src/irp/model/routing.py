from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Iterable, List


@dataclass(frozen=True)
class Route:
    destination: int
    next_hop: int
    metric: float
    protocol: str


@dataclass(frozen=True)
class ForwardingEntry:
    destination: int
    next_hop: int
    metric: float
    protocol: str


class RouteTable:
    def __init__(self) -> None:
        self._routes: Dict[int, Route] = {}

    def replace_protocol_routes(self, protocol: str, routes: Iterable[Route]) -> bool:
        updated = False
        stale = [dst for dst, route in self._routes.items() if route.protocol == protocol]
        for dst in stale:
            del self._routes[dst]
            updated = True
        for route in routes:
            dst = int(route.destination)
            prev = self._routes.get(dst)
            if prev != route:
                self._routes[dst] = route
                updated = True
        return updated

    def snapshot(self) -> List[Route]:
        return sorted(self._routes.values(), key=lambda r: r.destination)


class ForwardingTable:
    def __init__(self) -> None:
        self._entries: Dict[int, ForwardingEntry] = {}

    def sync_from_routes(self, routes: Iterable[Route]) -> bool:
        next_entries = {
            int(route.destination): ForwardingEntry(
                destination=int(route.destination),
                next_hop=int(route.next_hop),
                metric=float(route.metric),
                protocol=route.protocol,
            )
            for route in routes
        }
        if next_entries == self._entries:
            return False
        self._entries = next_entries
        return True

    def snapshot(self) -> List[ForwardingEntry]:
        return sorted(self._entries.values(), key=lambda e: e.destination)


@dataclass
class ForwardingPlan:
    to_install: List[ForwardingEntry] = field(default_factory=list)
    to_remove: List[ForwardingEntry] = field(default_factory=list)
