from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

NodeId = int
Metric = float
Payload = Dict[str, Any]


@dataclass(frozen=True)
class Message:
    tick_created: int
    src: NodeId
    dst: NodeId
    msg_type: str
    payload: Payload
    ttl: int = 64

    def sort_key(self) -> tuple[int, int, str, str]:
        return (self.src, self.dst, self.msg_type, repr(self.payload))


@dataclass(frozen=True)
class ExternalEvent:
    tick: int
    action: str
    params: Dict[str, Any] = field(default_factory=dict)


@dataclass
class RoutePlan:
    node: NodeId
    routes: Dict[NodeId, List[NodeId]]


@dataclass
class RunResult:
    converged_tick: Optional[int]
    route_hashes: List[str]
    route_tables: Dict[NodeId, Dict[NodeId, List[NodeId]]]
    delivered_messages: int
    dropped_messages: int
    events_applied: int
    route_flaps: int
