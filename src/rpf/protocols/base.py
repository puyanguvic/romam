from __future__ import annotations

import heapq
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, Iterable, List, Mapping, Sequence, Set, Tuple


@dataclass(frozen=True)
class RouteRecord:
    dst: int
    metric: float
    next_hops: Tuple[int, ...]
    source: str = "dynamic"
    metadata: Dict[str, Any] = field(default_factory=dict)


class RoutingProtocol:
    """Base class for distributed routing protocols.

    The class standardizes four core components:
    - control messages: registration and dispatch of per-message handlers
    - local storage: protocol private state in ``local_store``
    - routing table: RIB/FIB management helpers
    - forwarding path selection: next-hop selection policy hook
    """

    EPSILON = 1e-9

    def __init__(self, node_id: int, config: dict | None = None) -> None:
        self.node_id = node_id
        self.config = config or {}
        self.local_store: Dict[str, Any] = {}
        self._control_handlers: Dict[str, Callable[[Any, Any], None]] = {}
        self._rib: Dict[int, RouteRecord] = {}
        self._fib: Dict[int, List[int]] = {}
        self._install_self_route()

    def on_start(self, ctx: Any) -> None:
        _ = ctx

    def on_tick(self, ctx: Any) -> None:
        _ = ctx

    def on_message(self, ctx: Any, msg: Any) -> None:
        self.dispatch_control_message(ctx, msg)

    def on_link_change(self, ctx: Any, neighbor: int, new_metric: float | None) -> None:
        _ = (ctx, neighbor, new_metric)

    @property
    def routing_table(self) -> Dict[int, List[int]]:
        return {dst: list(hops) for dst, hops in self._fib.items()}

    @property
    def rib(self) -> Dict[int, RouteRecord]:
        return dict(self._rib)

    def state_get(self, key: str, default: Any = None) -> Any:
        return self.local_store.get(key, default)

    def state_set(self, key: str, value: Any) -> None:
        self.local_store[key] = value

    def state_pop(self, key: str, default: Any = None) -> Any:
        return self.local_store.pop(key, default)

    def register_control_handler(self, msg_type: str, handler: Callable[[Any, Any], None]) -> None:
        self._control_handlers[msg_type] = handler

    def unregister_control_handler(self, msg_type: str) -> None:
        self._control_handlers.pop(msg_type, None)

    def dispatch_control_message(self, ctx: Any, msg: Any) -> bool:
        handler = self._control_handlers.get(getattr(msg, "msg_type", ""))
        if handler is None:
            return False
        handler(ctx, msg)
        return True

    def send_control(
        self,
        ctx: Any,
        dst: int,
        msg_type: str,
        payload: Mapping[str, Any],
        ttl: int = 64,
    ) -> None:
        ctx.send(int(dst), msg_type, dict(payload), ttl=ttl)

    def broadcast_control(
        self,
        ctx: Any,
        msg_type: str,
        payload: Mapping[str, Any],
        exclude: int | None = None,
    ) -> None:
        ctx.broadcast(msg_type, dict(payload), exclude=exclude)

    def get_route_metric(self, dst: int, default: float = float("inf")) -> float:
        record = self._rib.get(int(dst))
        if record is None:
            return default
        return float(record.metric)

    def get_next_hops(self, dst: int) -> List[int]:
        return list(self._fib.get(int(dst), []))

    def clear_routes(self, keep_self: bool = True) -> bool:
        previous = self.routing_table
        self._rib.clear()
        self._fib.clear()
        if keep_self:
            self._install_self_route()
        return previous != self.routing_table

    def remove_route(self, dst: int) -> bool:
        dst = int(dst)
        if dst == self.node_id:
            return False
        existed = dst in self._fib
        self._rib.pop(dst, None)
        self._fib.pop(dst, None)
        return existed

    def select_forwarding_paths(
        self,
        dst: int,
        candidate_next_hops: Sequence[int],
        metric: float,
        source: str,
    ) -> List[int]:
        _ = (dst, metric, source)
        hops = sorted({int(h) for h in candidate_next_hops})
        if not hops:
            return []
        max_paths = int(self.config.get("max_paths", 0))
        if max_paths > 0:
            return hops[:max_paths]
        return hops

    def upsert_route(
        self,
        dst: int,
        metric: float,
        candidate_next_hops: Sequence[int],
        source: str,
        metadata: Mapping[str, Any] | None = None,
    ) -> bool:
        dst = int(dst)
        if dst == self.node_id:
            return self._install_self_route()

        selected_hops = self.select_forwarding_paths(dst, candidate_next_hops, metric, source)
        if not selected_hops:
            return self.remove_route(dst)

        normalized_metric = float(metric)
        record = RouteRecord(
            dst=dst,
            metric=normalized_metric,
            next_hops=tuple(selected_hops),
            source=str(source),
            metadata=dict(metadata or {}),
        )
        changed = self._rib.get(dst) != record
        self._rib[dst] = record
        self._fib[dst] = list(record.next_hops)
        return changed

    def publish_routing_table(self, ctx: Any) -> None:
        ctx.replace_routes(self.routing_table)

    def _install_self_route(self) -> bool:
        record = RouteRecord(
            dst=self.node_id,
            metric=0.0,
            next_hops=(self.node_id,),
            source="self",
            metadata={},
        )
        changed = self._rib.get(self.node_id) != record
        self._rib[self.node_id] = record
        self._fib[self.node_id] = [self.node_id]
        return changed

    @classmethod
    def dijkstra_with_predecessors(
        cls,
        graph: Mapping[int, Mapping[int, float]],
        start: int,
    ) -> Tuple[Dict[int, float], Dict[int, Set[int]]]:
        start = int(start)
        distances: Dict[int, float] = {start: 0.0}
        predecessors: Dict[int, Set[int]] = {start: set()}
        pq: List[Tuple[float, int]] = [(0.0, start)]

        while pq:
            dist_u, u = heapq.heappop(pq)
            if dist_u > distances.get(u, float("inf")) + cls.EPSILON:
                continue
            for v, weight in graph.get(u, {}).items():
                weight = float(weight)
                if weight < 0:
                    continue
                nd = dist_u + weight
                current = distances.get(v, float("inf"))
                if nd + cls.EPSILON < current:
                    distances[v] = nd
                    predecessors[v] = {u}
                    heapq.heappush(pq, (nd, v))
                elif abs(nd - current) <= cls.EPSILON:
                    predecessors.setdefault(v, set()).add(u)

        return distances, predecessors

    @classmethod
    def shortest_distances(cls, graph: Mapping[int, Mapping[int, float]], start: int) -> Dict[int, float]:
        distances, _ = cls.dijkstra_with_predecessors(graph, start)
        return distances

    @classmethod
    def compute_shortest_first_hops(
        cls,
        start: int,
        dst: int,
        predecessors: Mapping[int, Iterable[int]],
        max_paths: int = 0,
    ) -> List[int]:
        start = int(start)
        dst = int(dst)
        if dst == start:
            return []

        hops: Set[int] = set()
        stack: List[int] = [dst]
        visited: Set[int] = set()

        while stack:
            node = stack.pop()
            if node in visited:
                continue
            visited.add(node)
            for parent in predecessors.get(node, []):
                parent = int(parent)
                if parent == start:
                    hops.add(node)
                else:
                    stack.append(parent)

        candidates = sorted(h for h in hops if h != start)
        if max_paths > 0:
            return candidates[:max_paths]
        return candidates
