from __future__ import annotations

import heapq
from typing import Dict, Optional, Tuple

from rpf.protocols.base import RoutingProtocol


class OspfLikeProtocol(RoutingProtocol):
    def __init__(self, node_id: int, config: dict | None = None) -> None:
        super().__init__(node_id, config)
        self.spf_interval = int(self.config.get("spf_interval", 3))
        self.lsa_refresh = int(self.config.get("lsa_refresh", 20))
        self.jitter = int(self.config.get("jitter", 0))
        self.seq = 0
        self.lsdb: Dict[int, Tuple[int, Dict[int, float]]] = {}
        self.next_lsa_tick = 0
        self.next_spf_tick: Optional[int] = 0

    def on_start(self, ctx) -> None:
        self._originate_lsa(ctx)
        self._run_spf(ctx)

    def on_tick(self, ctx) -> None:
        if ctx.tick >= self.next_lsa_tick:
            self._originate_lsa(ctx)
        if self.next_spf_tick is not None and ctx.tick >= self.next_spf_tick:
            self._run_spf(ctx)
            self.next_spf_tick = None

    def on_message(self, ctx, msg) -> None:
        if msg.msg_type != "OSPF_LSA":
            return
        origin = int(msg.payload["origin"])
        seq = int(msg.payload["seq"])
        neighbors = {int(k): float(v) for k, v in msg.payload["neighbors"].items()}

        old = self.lsdb.get(origin)
        if old is not None and seq <= old[0]:
            return

        self.lsdb[origin] = (seq, neighbors)
        ctx.broadcast("OSPF_LSA", msg.payload, exclude=msg.src)
        target_tick = ctx.tick + self.spf_interval
        if self.next_spf_tick is None:
            self.next_spf_tick = target_tick
        else:
            self.next_spf_tick = min(self.next_spf_tick, target_tick)

    def on_link_change(self, ctx, neighbor: int, new_metric: float | None) -> None:
        _ = (neighbor, new_metric)
        self._originate_lsa(ctx)
        self.next_spf_tick = ctx.tick + self.spf_interval

    def _originate_lsa(self, ctx) -> None:
        neighbors = ctx.get_neighbors()
        self.seq += 1
        self.lsdb[self.node_id] = (self.seq, dict(neighbors))
        payload = {
            "origin": self.node_id,
            "seq": self.seq,
            "neighbors": {int(k): float(v) for k, v in neighbors.items()},
        }
        ctx.broadcast("OSPF_LSA", payload)
        self.next_lsa_tick = ctx.tick + self.lsa_refresh + self.jitter

    def _run_spf(self, ctx) -> None:
        graph: Dict[int, Dict[int, float]] = {}
        for origin, (_, neighbors) in self.lsdb.items():
            graph.setdefault(origin, {})
            for n, m in neighbors.items():
                graph.setdefault(n, {})
                old = graph[origin].get(n)
                if old is None or m < old:
                    graph[origin][n] = m
                old2 = graph[n].get(origin)
                if old2 is None or m < old2:
                    graph[n][origin] = m

        dist: Dict[int, float] = {self.node_id: 0.0}
        prev: Dict[int, int] = {}
        pq: list[tuple[float, int]] = [(0.0, self.node_id)]
        while pq:
            d, u = heapq.heappop(pq)
            if d > dist.get(u, float("inf")):
                continue
            for v, w in graph.get(u, {}).items():
                nd = d + w
                if nd < dist.get(v, float("inf")):
                    dist[v] = nd
                    prev[v] = u
                    heapq.heappush(pq, (nd, v))

        routes: Dict[int, list[int]] = {}
        for dst in sorted(dist):
            if dst == self.node_id:
                continue
            hop = dst
            seen = set()
            while True:
                parent = prev.get(hop)
                if parent is None:
                    hop = -1
                    break
                if parent == self.node_id:
                    break
                if hop in seen:
                    hop = -1
                    break
                seen.add(hop)
                hop = parent
            if hop >= 0:
                routes[dst] = [hop]

        ctx.replace_routes(routes)
