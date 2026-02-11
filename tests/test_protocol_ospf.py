from __future__ import annotations

from rpf.model.messages import ControlMessage, MessageKind
from rpf.protocols.base import ProtocolContext, RouterLink
from rpf.protocols.ospf import OspfProtocol, OspfTimers


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


def test_ospf_prefers_shortest_path_via_neighbor() -> None:
    proto = OspfProtocol(timers=OspfTimers(hello_interval=1.0, lsa_interval=30.0, lsa_max_age=60.0))

    start_ctx = _ctx(now=0.0, links={2: (1.0, True), 3: (5.0, True)})
    proto.start(start_ctx)

    msg_from_2 = ControlMessage(
        protocol="ospf",
        kind=MessageKind.OSPF_LSA,
        src_router_id=2,
        seq=1,
        ts=1.0,
        payload={
            "origin_router_id": 2,
            "lsa_seq": 1,
            "links": [{"neighbor_id": 1, "cost": 1.0}, {"neighbor_id": 3, "cost": 1.0}],
        },
    )
    proto.on_message(_ctx(now=1.0, links={2: (1.0, True), 3: (5.0, True)}), msg_from_2)

    msg_from_3 = ControlMessage(
        protocol="ospf",
        kind=MessageKind.OSPF_LSA,
        src_router_id=3,
        seq=1,
        ts=1.0,
        payload={
            "origin_router_id": 3,
            "lsa_seq": 1,
            "links": [{"neighbor_id": 2, "cost": 1.0}],
        },
    )
    outputs = proto.on_message(_ctx(now=1.1, links={2: (1.0, True), 3: (5.0, True)}), msg_from_3)
    assert outputs.routes is not None

    routes = {route.destination: route for route in outputs.routes}
    assert routes[2].next_hop == 2
    assert routes[2].metric == 1.0
    assert routes[3].next_hop == 2
    assert routes[3].metric == 2.0
