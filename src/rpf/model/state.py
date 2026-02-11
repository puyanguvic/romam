from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Iterable, List, Tuple


@dataclass
class NeighborInfo:
    router_id: int
    address: str
    port: int
    cost: float
    last_seen: float | None = None
    is_up: bool = False


class NeighborTable:
    def __init__(self, neighbors: Iterable[NeighborInfo]) -> None:
        self._neighbors: Dict[int, NeighborInfo] = {n.router_id: n for n in neighbors}

    def snapshot(self) -> Dict[int, NeighborInfo]:
        return {rid: NeighborInfo(**vars(info)) for rid, info in self._neighbors.items()}

    def mark_seen(self, router_id: int, now: float) -> bool:
        neighbor = self._neighbors.get(router_id)
        if neighbor is None:
            return False
        was_up = neighbor.is_up
        neighbor.last_seen = now
        neighbor.is_up = True
        return not was_up

    def refresh_liveness(self, now: float, dead_interval: float) -> List[int]:
        changed: List[int] = []
        for rid, neighbor in self._neighbors.items():
            if neighbor.last_seen is None:
                continue
            alive = (now - neighbor.last_seen) <= dead_interval
            if alive != neighbor.is_up:
                neighbor.is_up = alive
                changed.append(rid)
        return changed

    def get(self, router_id: int) -> NeighborInfo | None:
        return self._neighbors.get(router_id)

    def items(self) -> List[Tuple[int, NeighborInfo]]:
        return list(self._neighbors.items())


@dataclass(frozen=True)
class LinkStateRecord:
    router_id: int
    seq: int
    links: Dict[int, float]
    learned_at: float


class LinkStateDB:
    def __init__(self) -> None:
        self._records: Dict[int, LinkStateRecord] = {}

    def upsert(self, router_id: int, seq: int, links: Dict[int, float], now: float) -> bool:
        current = self._records.get(router_id)
        if current is not None and seq <= current.seq:
            return False
        self._records[router_id] = LinkStateRecord(
            router_id=int(router_id),
            seq=int(seq),
            links={int(k): float(v) for k, v in links.items()},
            learned_at=float(now),
        )
        return True

    def get(self, router_id: int) -> LinkStateRecord | None:
        return self._records.get(router_id)

    def records(self) -> List[LinkStateRecord]:
        return list(self._records.values())

    def age_out(self, now: float, max_age: float) -> bool:
        before = len(self._records)
        self._records = {
            rid: record
            for rid, record in self._records.items()
            if (now - record.learned_at) <= max_age
        }
        return len(self._records) != before
