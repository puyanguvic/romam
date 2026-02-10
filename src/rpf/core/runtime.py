from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

from rpf.core.topology import Topology
from rpf.core.types import ExternalEvent, Message
from rpf.protocols.registry import load_protocol


@dataclass
class Router:
    node_id: int
    protocol: Any
    routes: Dict[int, List[int]] = field(default_factory=dict)


class ProtocolContext:
    def __init__(self, runtime: "RouterRuntime", node_id: int, tick: int) -> None:
        self._runtime = runtime
        self.node_id = node_id
        self.tick = tick

    def send(self, dst: int, msg_type: str, payload: dict, ttl: int = 64) -> None:
        self._runtime._outbound.append(
            Message(
                tick_created=self.tick,
                src=self.node_id,
                dst=dst,
                msg_type=msg_type,
                payload=dict(payload),
                ttl=ttl,
            )
        )

    def broadcast(self, msg_type: str, payload: dict, exclude: Optional[int] = None) -> None:
        for nbr in sorted(self._runtime.topology.neighbors(self.node_id)):
            if exclude is not None and nbr == exclude:
                continue
            self.send(nbr, msg_type, payload)

    def get_neighbors(self) -> Dict[int, float]:
        return self._runtime.topology.neighbors(self.node_id)

    def get_link_metric(self, neighbor: int) -> Optional[float]:
        return self._runtime.topology.metric(self.node_id, neighbor)

    def get_topology_snapshot(self) -> Dict[int, Dict[int, float]]:
        return self._runtime.topology.snapshot()

    def install_route(self, dst: int, next_hops: List[int]) -> None:
        self._runtime._install_route(self.node_id, dst, next_hops)

    def replace_routes(self, routes: Dict[int, List[int]]) -> None:
        self._runtime._replace_routes(self.node_id, routes)


class RouterRuntime:
    def __init__(
        self,
        topology: Topology,
        protocol_name: str,
        protocol_config: Optional[dict] = None,
    ) -> None:
        self.topology = topology
        self.protocol_name = protocol_name
        self.protocol_config = protocol_config or {}
        self.routers: Dict[int, Router] = {}
        self._outbound: List[Message] = []
        self.route_flaps = 0

        protocol_cls = load_protocol(protocol_name)
        for node in self.topology.nodes():
            proto = protocol_cls(node_id=node, config=self.protocol_config)
            routes = {node: [node]}
            self.routers[node] = Router(node_id=node, protocol=proto, routes=routes)

    @property
    def route_tables(self) -> Dict[int, Dict[int, List[int]]]:
        return {n: dict(r.routes) for n, r in self.routers.items()}

    def bootstrap(self) -> None:
        for node in sorted(self.routers):
            ctx = ProtocolContext(self, node, tick=0)
            self.routers[node].protocol.on_start(ctx)

    def process_tick(self, tick: int, incoming: List[Message]) -> None:
        inbox: Dict[int, List[Message]] = {n: [] for n in self.routers}
        for msg in incoming:
            if msg.dst in inbox:
                inbox[msg.dst].append(msg)
        for node in sorted(inbox):
            inbox[node].sort(key=lambda m: m.sort_key())

        for node in sorted(self.routers):
            router = self.routers[node]
            ctx = ProtocolContext(self, node, tick=tick)
            router.protocol.on_tick(ctx)
            for msg in inbox[node]:
                router.protocol.on_message(ctx, msg)

    def handle_event(self, tick: int, event: ExternalEvent) -> None:
        action = event.action
        p = event.params
        if action == "remove_link":
            u, v = int(p["u"]), int(p["v"])
            self.topology.remove_link(u, v)
            self._notify_link_change(tick, u, v, None)
            return
        if action == "add_link":
            u, v = int(p["u"]), int(p["v"])
            metric = float(p.get("metric", 1.0))
            self.topology.add_link(u, v, metric)
            self._notify_link_change(tick, u, v, metric)
            return
        if action == "update_metric":
            u, v = int(p["u"]), int(p["v"])
            metric = float(p["metric"])
            self.topology.update_metric(u, v, metric)
            self._notify_link_change(tick, u, v, metric)
            return
        raise ValueError(f"Unsupported event action: {action}")

    def _notify_link_change(self, tick: int, u: int, v: int, metric: Optional[float]) -> None:
        if u in self.routers:
            ctx_u = ProtocolContext(self, u, tick=tick)
            self.routers[u].protocol.on_link_change(ctx_u, neighbor=v, new_metric=metric)
        if v in self.routers:
            ctx_v = ProtocolContext(self, v, tick=tick)
            self.routers[v].protocol.on_link_change(ctx_v, neighbor=u, new_metric=metric)

    def consume_outbound(self) -> List[Message]:
        out = self._outbound
        self._outbound = []
        out.sort(key=lambda m: (m.tick_created, *m.sort_key()))
        return out

    def _install_route(self, node: int, dst: int, next_hops: List[int]) -> None:
        hops = sorted(set(int(h) for h in next_hops))
        router = self.routers[node]
        old = router.routes.get(dst)
        if old != hops:
            self.route_flaps += 1
            router.routes[dst] = hops

    def _replace_routes(self, node: int, routes: Dict[int, List[int]]) -> None:
        router = self.routers[node]
        cleaned: Dict[int, List[int]] = {node: [node]}
        for dst, hops in routes.items():
            d = int(dst)
            if d == node:
                continue
            hs = sorted(set(int(h) for h in hops))
            if hs:
                cleaned[d] = hs
        if router.routes != cleaned:
            self.route_flaps += 1
            router.routes = cleaned
