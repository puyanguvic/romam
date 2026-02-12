from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Tuple

from irp.model.messages import ControlMessage, MessageKind
from irp.model.routing import Route
from irp.protocols.base import ProtocolContext, ProtocolEngine, ProtocolOutputs


@dataclass
class RipTimers:
    update_interval: float = 5.0
    neighbor_timeout: float = 15.0


class RipProtocol(ProtocolEngine):
    def __init__(
        self,
        timers: RipTimers | None = None,
        infinity_metric: float = 16.0,
        poison_reverse: bool = True,
    ) -> None:
        self._timers = timers or RipTimers()
        self._infinity = float(infinity_metric)
        self._poison_reverse = poison_reverse
        self._msg_seq = 0
        self._last_update_at = -1e9
        self._routes: Dict[int, Route] = {}
        self._neighbor_vectors: Dict[int, tuple[float, Dict[int, float]]] = {}

    @property
    def name(self) -> str:
        return "rip"

    def start(self, ctx: ProtocolContext) -> ProtocolOutputs:
        outputs = ProtocolOutputs()
        routes_changed = self._recompute_routes(ctx)
        outputs.routes = list(self._routes.values()) if routes_changed else None
        outputs.outbound.extend(self._send_updates(ctx))
        self._last_update_at = ctx.now
        return outputs

    def on_timer(self, ctx: ProtocolContext) -> ProtocolOutputs:
        outputs = ProtocolOutputs()
        self._expire_neighbor_vectors(ctx)
        routes_changed = self._recompute_routes(ctx)
        periodic_due = (ctx.now - self._last_update_at) >= self._timers.update_interval
        if routes_changed:
            outputs.routes = list(self._routes.values())
        if routes_changed or periodic_due:
            outputs.outbound.extend(self._send_updates(ctx))
            self._last_update_at = ctx.now
        return outputs

    def on_message(self, ctx: ProtocolContext, message: ControlMessage) -> ProtocolOutputs:
        outputs = ProtocolOutputs()
        if message.kind != MessageKind.RIP_UPDATE:
            return outputs
        self._neighbor_vectors[message.src_router_id] = (
            ctx.now,
            self._parse_update_entries(message.payload.get("entries", [])),
        )
        if self._recompute_routes(ctx):
            outputs.routes = list(self._routes.values())
            outputs.outbound.extend(self._send_updates(ctx))
            self._last_update_at = ctx.now
        return outputs

    def _expire_neighbor_vectors(self, ctx: ProtocolContext) -> None:
        stale: List[int] = []
        for neighbor_id, (last_seen, _) in self._neighbor_vectors.items():
            link = ctx.links.get(neighbor_id)
            if link is None or not link.is_up:
                stale.append(neighbor_id)
                continue
            if (ctx.now - last_seen) > self._timers.neighbor_timeout:
                stale.append(neighbor_id)
        for neighbor_id in stale:
            self._neighbor_vectors.pop(neighbor_id, None)

    def _recompute_routes(self, ctx: ProtocolContext) -> bool:
        candidates: Dict[int, Tuple[float, int]] = {}

        for neighbor_id, link in ctx.links.items():
            if not link.is_up:
                continue
            candidates[neighbor_id] = (float(link.cost), neighbor_id)

        for neighbor_id, (_, vector) in self._neighbor_vectors.items():
            link = ctx.links.get(neighbor_id)
            if link is None or not link.is_up:
                continue
            base = float(link.cost)
            for destination, adv_metric in vector.items():
                if destination == ctx.router_id:
                    continue
                total_metric = min(self._infinity, base + float(adv_metric))
                if total_metric >= self._infinity:
                    continue
                current = candidates.get(destination)
                proposal = (total_metric, neighbor_id)
                if current is None or proposal < current:
                    candidates[destination] = proposal

        next_routes: Dict[int, Route] = {
            destination: Route(
                destination=destination,
                next_hop=next_hop,
                metric=metric,
                protocol=self.name,
            )
            for destination, (metric, next_hop) in candidates.items()
        }
        if next_routes == self._routes:
            return False
        self._routes = next_routes
        return True

    def _send_updates(self, ctx: ProtocolContext) -> List[tuple[int, ControlMessage]]:
        outbound: List[tuple[int, ControlMessage]] = []
        for neighbor_id in sorted(ctx.links.keys()):
            entries = [{"destination": ctx.router_id, "metric": 0.0}]
            for route in sorted(self._routes.values(), key=lambda r: r.destination):
                if route.next_hop == neighbor_id and route.destination != neighbor_id:
                    if self._poison_reverse:
                        metric = self._infinity
                    else:
                        continue
                else:
                    metric = min(self._infinity, route.metric)
                entries.append({"destination": route.destination, "metric": metric})
            outbound.append(
                (
                    neighbor_id,
                    self._new_message(
                        ctx.router_id,
                        MessageKind.RIP_UPDATE,
                        {"entries": entries},
                        ctx.now,
                    ),
                )
            )
        return outbound

    def _new_message(
        self,
        router_id: int,
        kind: MessageKind,
        payload: Dict,
        now: float,
    ) -> ControlMessage:
        self._msg_seq += 1
        return ControlMessage(
            protocol=self.name,
            kind=kind,
            src_router_id=router_id,
            seq=self._msg_seq,
            payload=payload,
            ts=now,
        )

    def _parse_update_entries(self, entries: List[Dict]) -> Dict[int, float]:
        parsed: Dict[int, float] = {}
        for item in entries:
            destination = int(item["destination"])
            metric = float(item["metric"])
            parsed[destination] = min(self._infinity, max(0.0, metric))
        return parsed
