from __future__ import annotations

import heapq
from typing import Dict

from rpf.protocols.base import RoutingProtocol


def shortest_distances(graph: Dict[int, Dict[int, float]], start: int) -> Dict[int, float]:
    dist = {start: 0.0}
    pq = [(0.0, start)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist.get(u, float("inf")):
            continue
        for v, w in graph.get(u, {}).items():
            nd = d + w
            if nd < dist.get(v, float("inf")):
                dist[v] = nd
                heapq.heappush(pq, (nd, v))
    return dist


class EcmpProtocol(RoutingProtocol):
    """Centralized ECMP planner over current topology snapshot."""

    def __init__(self, node_id: int, config: dict | None = None) -> None:
        super().__init__(node_id, config)
        self.k_paths = int(self.config.get("k_paths", 2))
        self.recompute_interval = int(self.config.get("recompute_interval", 3))

    def on_start(self, ctx) -> None:
        self._recompute(ctx)

    def on_tick(self, ctx) -> None:
        if ctx.tick % self.recompute_interval == 0:
            self._recompute(ctx)

    def on_message(self, ctx, msg) -> None:
        _ = (ctx, msg)

    def on_link_change(self, ctx, neighbor: int, new_metric: float | None) -> None:
        _ = (neighbor, new_metric)
        self._recompute(ctx)

    def _recompute(self, ctx) -> None:
        graph = ctx.get_topology_snapshot()
        my_neighbors = graph.get(self.node_id, {})
        if not my_neighbors:
            ctx.replace_routes({})
            return

        dist_from_neighbor = {nbr: shortest_distances(graph, nbr) for nbr in my_neighbors}
        all_nodes = sorted(graph.keys())
        routes = {}

        for dst in all_nodes:
            if dst == self.node_id:
                continue
            best = float("inf")
            candidates = []
            for nbr, link_cost in my_neighbors.items():
                d = dist_from_neighbor[nbr].get(dst, float("inf"))
                total = link_cost + d
                if total < best:
                    best = total
                    candidates = [nbr]
                elif total == best:
                    candidates.append(nbr)
            if candidates and best < float("inf"):
                routes[dst] = sorted(candidates)[: self.k_paths]

        ctx.replace_routes(routes)
