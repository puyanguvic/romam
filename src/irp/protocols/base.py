from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from irp.model.messages import ControlMessage
from irp.model.routing import Route


@dataclass(frozen=True)
class RouterLink:
    neighbor_id: int
    cost: float
    address: str
    port: int
    is_up: bool


@dataclass(frozen=True)
class ProtocolContext:
    router_id: int
    now: float
    links: Dict[int, RouterLink]


@dataclass
class ProtocolOutputs:
    outbound: List[Tuple[int, ControlMessage]] = field(default_factory=list)
    routes: Optional[List[Route]] = None


class ProtocolEngine(ABC):
    @property
    @abstractmethod
    def name(self) -> str:
        raise NotImplementedError

    @abstractmethod
    def start(self, ctx: ProtocolContext) -> ProtocolOutputs:
        raise NotImplementedError

    @abstractmethod
    def on_timer(self, ctx: ProtocolContext) -> ProtocolOutputs:
        raise NotImplementedError

    @abstractmethod
    def on_message(self, ctx: ProtocolContext, message: ControlMessage) -> ProtocolOutputs:
        raise NotImplementedError
