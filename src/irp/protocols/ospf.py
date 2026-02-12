from __future__ import annotations

import heapq
from dataclasses import dataclass
from typing import Dict, List

from irp.model.messages import ControlMessage, MessageKind
from irp.model.routing import Route
from irp.model.state import LinkStateDB
from irp.protocols.base import ProtocolContext, ProtocolEngine, ProtocolOutputs


@dataclass
class OspfTimers:
    hello_interval: float = 1.0
    lsa_interval: float = 3.0
    lsa_max_age: float = 15.0


class OspfProtocol(ProtocolEngine):
    def __init__(self, timers: OspfTimers | None = None) -> None:
        self._timers = timers or OspfTimers()
        self._msg_seq = 0
        self._lsa_seq = 0
        self._last_hello_at = -1e9
        self._last_lsa_at = -1e9
        self._last_local_links: Dict[int, float] = {}
        self._lsdb = LinkStateDB()

    @property
    def name(self) -> str:
        return "ospf"

    def start(self, ctx: ProtocolContext) -> ProtocolOutputs:
        return self._drive(ctx, force_lsa=True)

    def on_timer(self, ctx: ProtocolContext) -> ProtocolOutputs:
        return self._drive(ctx, force_lsa=False)

    def on_message(self, ctx: ProtocolContext, message: ControlMessage) -> ProtocolOutputs:
        outputs = ProtocolOutputs()
        if message.kind == MessageKind.HELLO:
            return outputs
        if message.kind != MessageKind.OSPF_LSA:
            return outputs

        origin = int(message.payload.get("origin_router_id", message.src_router_id))
        lsa_seq = int(message.payload.get("lsa_seq", -1))
        links = self._parse_links(message.payload.get("links", []))
        changed = self._lsdb.upsert(origin, lsa_seq, links, ctx.now)
        if not changed:
            return outputs

        outputs.outbound.extend(
            self._flood_lsa(ctx, message.payload, exclude=message.src_router_id)
        )
        outputs.routes = self._compute_routes(ctx)
        return outputs

    def _drive(self, ctx: ProtocolContext, force_lsa: bool) -> ProtocolOutputs:
        outputs = ProtocolOutputs()
        now = ctx.now

        if (now - self._last_hello_at) >= self._timers.hello_interval:
            outputs.outbound.extend(self._send_hello(ctx))
            self._last_hello_at = now

        links = {rid: link.cost for rid, link in ctx.links.items() if link.is_up}
        should_originate = force_lsa or links != self._last_local_links
        if (now - self._last_lsa_at) >= self._timers.lsa_interval:
            should_originate = True
        if should_originate:
            self._lsa_seq += 1
            self._last_local_links = dict(links)
            payload = {
                "origin_router_id": ctx.router_id,
                "lsa_seq": self._lsa_seq,
                "links": [
                    {"neighbor_id": rid, "cost": cost}
                    for rid, cost in sorted(links.items())
                ],
            }
            self._lsdb.upsert(ctx.router_id, self._lsa_seq, links, now)
            outputs.outbound.extend(self._flood_lsa(ctx, payload, exclude=None))
            self._last_lsa_at = now

        aged = self._lsdb.age_out(now, self._timers.lsa_max_age)
        if aged or should_originate:
            outputs.routes = self._compute_routes(ctx)
        return outputs

    def _send_hello(self, ctx: ProtocolContext) -> List[tuple[int, ControlMessage]]:
        payload = {"router_id": ctx.router_id}
        out: List[tuple[int, ControlMessage]] = []
        for neighbor_id in sorted(ctx.links.keys()):
            out.append(
                (
                    neighbor_id,
                    self._new_message(ctx.router_id, MessageKind.HELLO, payload, ctx.now),
                )
            )
        return out

    def _flood_lsa(
        self,
        ctx: ProtocolContext,
        payload: Dict,
        exclude: int | None,
    ) -> List[tuple[int, ControlMessage]]:
        out: List[tuple[int, ControlMessage]] = []
        for neighbor_id in sorted(ctx.links.keys()):
            if exclude is not None and neighbor_id == exclude:
                continue
            out.append(
                (
                    neighbor_id,
                    self._new_message(ctx.router_id, MessageKind.OSPF_LSA, dict(payload), ctx.now),
                )
            )
        return out

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

    def _compute_routes(self, ctx: ProtocolContext) -> List[Route]:
        graph: Dict[int, Dict[int, float]] = {}
        for record in self._lsdb.records():
            graph.setdefault(record.router_id, {})
            for neighbor_id, cost in record.links.items():
                graph[record.router_id][neighbor_id] = float(cost)
                graph.setdefault(neighbor_id, {})

        src = ctx.router_id
        graph.setdefault(src, {})
        dist: Dict[int, float] = {src: 0.0}
        first_hop: Dict[int, int] = {}
        heap: List[tuple[float, int]] = [(0.0, src)]

        while heap:
            cost_u, u = heapq.heappop(heap)
            if cost_u > dist.get(u, float("inf")):
                continue
            for v, edge_cost in graph.get(u, {}).items():
                candidate = cost_u + float(edge_cost)
                candidate_hop = v if u == src else first_hop[u]
                best = dist.get(v, float("inf"))
                if candidate < best:
                    dist[v] = candidate
                    first_hop[v] = candidate_hop
                    heapq.heappush(heap, (candidate, v))
                elif candidate == best and candidate_hop < first_hop.get(v, 1 << 30):
                    first_hop[v] = candidate_hop
                    heapq.heappush(heap, (candidate, v))

        routes: List[Route] = []
        for destination, total_cost in sorted(dist.items()):
            if destination == src:
                continue
            next_hop = first_hop.get(destination)
            if next_hop is None:
                continue
            routes.append(
                Route(
                    destination=destination,
                    next_hop=next_hop,
                    metric=float(total_cost),
                    protocol=self.name,
                )
            )
        return routes

    @staticmethod
    def _parse_links(raw_links: List[Dict]) -> Dict[int, float]:
        links: Dict[int, float] = {}
        for item in raw_links:
            neighbor_id = int(item["neighbor_id"])
            links[neighbor_id] = float(item["cost"])
        return links
