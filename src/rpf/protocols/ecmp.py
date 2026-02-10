from __future__ import annotations

from typing import Dict

from rpf.protocols.base import RoutingProtocol


class EcmpProtocol(RoutingProtocol):
    """Topology-snapshot ECMP planner over global shortest paths."""

    def __init__(self, node_id: int, config: dict | None = None) -> None:
        super().__init__(node_id, config)
        self.k_paths = max(1, int(self.config.get("k_paths", self.config.get("max_paths", 4))))
        self.recompute_interval = max(1, int(self.config.get("recompute_interval", 3)))

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
        graph: Dict[int, Dict[int, float]] = ctx.get_topology_snapshot()
        if self.node_id not in graph:
            self.clear_routes(keep_self=True)
            self.publish_routing_table(ctx)
            return

        distances, predecessors = self.dijkstra_with_predecessors(graph, self.node_id)

        self.clear_routes(keep_self=True)
        for dst in sorted(distances):
            if dst == self.node_id:
                continue
            first_hops = self.compute_shortest_first_hops(
                start=self.node_id,
                dst=dst,
                predecessors=predecessors,
                max_paths=self.k_paths,
            )
            if not first_hops:
                continue
            self.upsert_route(
                dst=dst,
                metric=float(distances[dst]),
                candidate_next_hops=first_hops,
                source="ecmp",
            )

        self.publish_routing_table(ctx)


class EcompProtocol(EcmpProtocol):
    """Alias for users using the historical 'ecomp' name."""
