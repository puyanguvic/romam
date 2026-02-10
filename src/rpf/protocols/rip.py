from __future__ import annotations

from typing import Dict, Mapping

from rpf.protocols.base import RoutingProtocol


class RipProtocol(RoutingProtocol):
    """RIP v2 style distance-vector protocol over the simulation transport."""

    MSG_UPDATE = "RIP_UPDATE"

    def __init__(self, node_id: int, config: dict | None = None) -> None:
        super().__init__(node_id, config)
        self.update_interval = max(1, int(self.config.get("update_interval", 5)))
        self.triggered_min_gap = max(0, int(self.config.get("triggered_min_gap", 2)))
        self.infinity_metric = float(self.config.get("infinity_metric", 16))
        self.split_horizon = bool(self.config.get("split_horizon", True))
        self.poison_reverse = bool(self.config.get("poison_reverse", True))
        self.neighbor_timeout = max(
            self.update_interval,
            int(self.config.get("neighbor_timeout", self.update_interval * 6)),
        )
        self.garbage_timeout = max(
            self.update_interval,
            int(self.config.get("garbage_timeout", self.update_interval * 4)),
        )

        self.state_set("neighbor_vectors", {})
        self.state_set("neighbor_last_seen", {})
        self.state_set("direct_links", {})
        self.state_set("advertised_vector", {self.node_id: 0.0})
        self.state_set("poisoned_until", {})
        self.state_set("last_periodic_tick", -10**9)
        self.state_set("last_trigger_tick", -10**9)
        self.register_control_handler(self.MSG_UPDATE, self._on_rip_update)

    def on_start(self, ctx) -> None:
        self._sync_links(ctx)
        self._recompute_routes(ctx)
        self.publish_routing_table(ctx)
        self._send_periodic_update(ctx, bootstrap=True)

    def on_tick(self, ctx) -> None:
        self._sync_links(ctx)
        stale_neighbors = self._expire_stale_neighbors(ctx)
        poison_changed = self._expire_poisoned_routes(ctx)

        if stale_neighbors or poison_changed:
            changed = self._recompute_routes(ctx)
            if changed:
                self.publish_routing_table(ctx)
                self._maybe_send_triggered_update(ctx)

        last_periodic = int(self.state_get("last_periodic_tick", -10**9))
        if ctx.tick - last_periodic >= self.update_interval:
            self._send_periodic_update(ctx, bootstrap=False)

    def on_message(self, ctx, msg) -> None:
        self.dispatch_control_message(ctx, msg)

    def on_link_change(self, ctx, neighbor: int, new_metric: float | None) -> None:
        _ = (neighbor, new_metric)
        self._sync_links(ctx)
        self._expire_stale_neighbors(ctx)
        changed = self._recompute_routes(ctx)
        if changed:
            self.publish_routing_table(ctx)
            self._maybe_send_triggered_update(ctx)

    def _on_rip_update(self, ctx, msg) -> None:
        src = int(msg.src)
        if src == self.node_id:
            return
        if ctx.get_link_metric(src) is None:
            return

        raw_payload = msg.payload if isinstance(msg.payload, Mapping) else {}
        raw_vector = raw_payload.get("vector", {})
        parsed_vector: Dict[int, float] = {}
        if isinstance(raw_vector, Mapping):
            for raw_dst, raw_metric in raw_vector.items():
                dst = int(raw_dst)
                metric = max(0.0, float(raw_metric))
                parsed_vector[dst] = min(metric, self.infinity_metric)
        parsed_vector[src] = 0.0

        self._sync_links(ctx)
        neighbor_vectors: Dict[int, Dict[int, float]] = self.state_get("neighbor_vectors", {})
        neighbor_last_seen: Dict[int, int] = self.state_get("neighbor_last_seen", {})
        neighbor_vectors[src] = parsed_vector
        neighbor_last_seen[src] = int(ctx.tick)
        self.state_set("neighbor_vectors", neighbor_vectors)
        self.state_set("neighbor_last_seen", neighbor_last_seen)

        changed = self._recompute_routes(ctx)
        if changed:
            self.publish_routing_table(ctx)
            self._maybe_send_triggered_update(ctx)

    def _sync_links(self, ctx) -> None:
        links = {int(n): float(m) for n, m in ctx.get_neighbors().items() if float(m) >= 0}
        self.state_set("direct_links", links)

    def _expire_stale_neighbors(self, ctx) -> bool:
        direct_links: Dict[int, float] = self.state_get("direct_links", {})
        neighbor_vectors: Dict[int, Dict[int, float]] = self.state_get("neighbor_vectors", {})
        neighbor_last_seen: Dict[int, int] = self.state_get("neighbor_last_seen", {})
        changed = False
        for nbr in list(neighbor_vectors):
            last_seen = int(neighbor_last_seen.get(nbr, -10**9))
            is_link_down = nbr not in direct_links
            is_timeout = int(ctx.tick) - last_seen > self.neighbor_timeout
            if is_link_down or is_timeout:
                neighbor_vectors.pop(nbr, None)
                neighbor_last_seen.pop(nbr, None)
                changed = True
        if changed:
            self.state_set("neighbor_vectors", neighbor_vectors)
            self.state_set("neighbor_last_seen", neighbor_last_seen)
        return changed

    def _expire_poisoned_routes(self, ctx) -> bool:
        poisoned_until: Dict[int, int] = self.state_get("poisoned_until", {})
        changed = False
        for dst, deadline in list(poisoned_until.items()):
            if int(ctx.tick) >= int(deadline):
                poisoned_until.pop(dst, None)
                changed = True
        if changed:
            self.state_set("poisoned_until", poisoned_until)
        return changed

    def _recompute_routes(self, ctx) -> bool:
        _ = ctx
        previous_adv: Dict[int, float] = self.state_get("advertised_vector", {})
        previous_routes = self.routing_table

        direct_links: Dict[int, float] = self.state_get("direct_links", {})
        neighbor_vectors: Dict[int, Dict[int, float]] = self.state_get("neighbor_vectors", {})
        poisoned_until: Dict[int, int] = self.state_get("poisoned_until", {})
        now = int(ctx.tick)

        destinations = {self.node_id} | set(direct_links.keys()) | set(previous_adv.keys()) | set(poisoned_until.keys())
        for vector in neighbor_vectors.values():
            destinations.update(vector.keys())

        finite_vector: Dict[int, float] = {self.node_id: 0.0}
        selected_next_hop: Dict[int, int] = {}

        for dst in sorted(destinations):
            if dst == self.node_id:
                continue
            best_metric = self.infinity_metric
            best_neighbor: int | None = None

            for nbr, link_metric in direct_links.items():
                candidate = self.infinity_metric
                if dst == nbr:
                    candidate = min(candidate, link_metric)
                advertised = neighbor_vectors.get(nbr, {}).get(dst)
                if advertised is not None:
                    candidate = min(candidate, link_metric + advertised)
                candidate = min(candidate, self.infinity_metric)

                if candidate + self.EPSILON < best_metric:
                    best_metric = candidate
                    best_neighbor = nbr
                elif abs(candidate - best_metric) <= self.EPSILON and best_neighbor is not None:
                    if nbr < best_neighbor:
                        best_neighbor = nbr

            if best_neighbor is not None and best_metric < self.infinity_metric:
                finite_vector[dst] = best_metric
                selected_next_hop[dst] = best_neighbor

        previous_finite = {
            int(dst)
            for dst, metric in previous_adv.items()
            if int(dst) != self.node_id and float(metric) < self.infinity_metric
        }
        new_finite = set(finite_vector.keys()) - {self.node_id}
        withdrawn = previous_finite - new_finite
        for dst in withdrawn:
            poisoned_until[dst] = now + self.garbage_timeout
        for dst in list(poisoned_until):
            if dst in new_finite or now >= int(poisoned_until[dst]):
                poisoned_until.pop(dst, None)

        advertised_vector = dict(finite_vector)
        for dst in sorted(poisoned_until):
            if dst != self.node_id and dst not in new_finite:
                advertised_vector[dst] = self.infinity_metric

        self.clear_routes(keep_self=True)
        for dst, metric in finite_vector.items():
            if dst == self.node_id:
                continue
            self.upsert_route(
                dst=dst,
                metric=float(metric),
                candidate_next_hops=[selected_next_hop[dst]],
                source="rip",
            )

        self.state_set("poisoned_until", poisoned_until)
        self.state_set("advertised_vector", advertised_vector)
        return previous_adv != advertised_vector or previous_routes != self.routing_table

    def _send_periodic_update(self, ctx, bootstrap: bool) -> None:
        reason = "bootstrap" if bootstrap else "periodic"
        self._send_vector_to_neighbors(ctx, reason=reason)
        self.state_set("last_periodic_tick", int(ctx.tick))

    def _maybe_send_triggered_update(self, ctx) -> None:
        last_trigger = int(self.state_get("last_trigger_tick", -10**9))
        if ctx.tick - last_trigger < self.triggered_min_gap:
            return
        self._send_vector_to_neighbors(ctx, reason="triggered")
        self.state_set("last_trigger_tick", int(ctx.tick))

    def _send_vector_to_neighbors(self, ctx, reason: str) -> None:
        direct_links: Dict[int, float] = self.state_get("direct_links", {})
        if not direct_links:
            return
        vector: Dict[int, float] = self.state_get("advertised_vector", {self.node_id: 0.0})

        for neighbor in sorted(direct_links):
            advertised: Dict[int, float] = {}
            for dst, metric in vector.items():
                clipped_metric = min(float(metric), self.infinity_metric)
                if self.split_horizon and dst != self.node_id and self.get_next_hops(dst) == [neighbor]:
                    if self.poison_reverse:
                        clipped_metric = self.infinity_metric
                    else:
                        continue
                advertised[int(dst)] = clipped_metric
            advertised[self.node_id] = 0.0
            self.send_control(
                ctx,
                dst=neighbor,
                msg_type=self.MSG_UPDATE,
                payload={"vector": advertised, "reason": reason},
            )
