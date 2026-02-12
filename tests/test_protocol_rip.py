from __future__ import annotations

from irp.model.messages import ControlMessage, MessageKind
from irp.protocols.base import ProtocolContext, RouterLink
from irp.protocols.rip import RipProtocol, RipTimers


def _ctx(now: float, links: dict[int, tuple[float, bool]]) -> ProtocolContext:
    return ProtocolContext(
        router_id=1,
        now=now,
        links={
            rid: RouterLink(
                neighbor_id=rid,
                cost=cost,
                address=f"10.0.{rid}.1",
                port=5500,
                is_up=is_up,
            )
            for rid, (cost, is_up) in links.items()
        },
    )


def test_rip_updates_route_when_better_vector_arrives() -> None:
    proto = RipProtocol(timers=RipTimers(update_interval=30.0, neighbor_timeout=30.0))
    proto.start(_ctx(now=0.0, links={2: (1.0, True), 3: (4.0, True)}))

    update_from_2 = ControlMessage(
        protocol="rip",
        kind=MessageKind.RIP_UPDATE,
        src_router_id=2,
        seq=1,
        ts=1.0,
        payload={
            "entries": [
                {"destination": 2, "metric": 0.0},
                {"destination": 3, "metric": 1.0},
            ]
        },
    )
    outputs = proto.on_message(_ctx(now=1.0, links={2: (1.0, True), 3: (4.0, True)}), update_from_2)
    assert outputs.routes is not None
    routes = {route.destination: route for route in outputs.routes}
    assert routes[3].next_hop == 2
    assert routes[3].metric == 2.0


def test_rip_falls_back_when_neighbor_down() -> None:
    proto = RipProtocol(timers=RipTimers(update_interval=30.0, neighbor_timeout=30.0))
    proto.start(_ctx(now=0.0, links={2: (1.0, True), 3: (4.0, True)}))

    update_from_2 = ControlMessage(
        protocol="rip",
        kind=MessageKind.RIP_UPDATE,
        src_router_id=2,
        seq=1,
        ts=1.0,
        payload={
            "entries": [
                {"destination": 2, "metric": 0.0},
                {"destination": 3, "metric": 1.0},
            ]
        },
    )
    proto.on_message(_ctx(now=1.0, links={2: (1.0, True), 3: (4.0, True)}), update_from_2)

    outputs = proto.on_timer(_ctx(now=2.0, links={2: (1.0, False), 3: (4.0, True)}))
    assert outputs.routes is not None
    routes = {route.destination: route for route in outputs.routes}
    assert routes[3].next_hop == 3
    assert routes[3].metric == 4.0
