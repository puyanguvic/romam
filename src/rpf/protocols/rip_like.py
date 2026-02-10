from __future__ import annotations

from typing import Dict

from rpf.protocols.base import RoutingProtocol


class RipLikeProtocol(RoutingProtocol):
    def __init__(self, node_id: int, config: dict | None = None) -> None:
        super().__init__(node_id, config)
        self.update_interval = int(self.config.get("update_interval", 5))
        self.triggered_min_gap = int(self.config.get("triggered_min_gap", 2))
        self.infinity_metric = int(self.config.get("infinity_metric", 16))
        self.split_horizon = bool(self.config.get("split_horizon", True))
        self.distance: Dict[int, float] = {self.node_id: 0.0}
        self.next_hop: Dict[int, int] = {self.node_id: self.node_id}
        self.last_trigger_tick = -10**9

    def on_start(self, ctx) -> None:
        self._publish_routes(ctx)
        self._send_update(ctx)

    def on_tick(self, ctx) -> None:
        if ctx.tick % self.update_interval == 0:
            self._send_update(ctx)

    def on_message(self, ctx, msg) -> None:
        if msg.msg_type != "RIP_UPDATE":
            return
        link_metric = ctx.get_link_metric(msg.src)
        if link_metric is None:
            return

        vector = {int(k): float(v) for k, v in msg.payload.get("vector", {}).items()}
        changed = False
        for dst, nbr_cost in vector.items():
            if dst == self.node_id:
                continue
            candidate = min(float(self.infinity_metric), link_metric + nbr_cost)
            current = self.distance.get(dst, float(self.infinity_metric))
            route_via = self.next_hop.get(dst)
            if candidate < current or (route_via == msg.src and candidate != current):
                self.distance[dst] = candidate
                self.next_hop[dst] = msg.src
                changed = True

        if changed:
            self._publish_routes(ctx)
            if ctx.tick - self.last_trigger_tick >= self.triggered_min_gap:
                self._send_update(ctx)
                self.last_trigger_tick = ctx.tick

    def on_link_change(self, ctx, neighbor: int, new_metric: float | None) -> None:
        if new_metric is None:
            changed = False
            for dst in list(self.next_hop.keys()):
                if self.next_hop.get(dst) == neighbor and dst != self.node_id:
                    self.distance[dst] = float(self.infinity_metric)
                    changed = True
            if changed:
                self._publish_routes(ctx)
                self._send_update(ctx)

    def _publish_routes(self, ctx) -> None:
        routes = {}
        for dst, metric in self.distance.items():
            if dst == self.node_id:
                continue
            if metric < self.infinity_metric:
                routes[dst] = [self.next_hop[dst]]
        ctx.replace_routes(routes)

    def _send_update(self, ctx) -> None:
        neighbors = sorted(ctx.get_neighbors())
        for nbr in neighbors:
            vector = {}
            for dst, cost in self.distance.items():
                if cost >= self.infinity_metric:
                    continue
                if self.split_horizon and dst != self.node_id and self.next_hop.get(dst) == nbr:
                    continue
                vector[dst] = min(float(self.infinity_metric), float(cost))
            vector[self.node_id] = 0.0
            ctx.send(nbr, "RIP_UPDATE", {"vector": vector})
